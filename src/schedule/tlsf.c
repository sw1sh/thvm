// schedule/tlsf.c - Two-Level Segregated Fit (TLSF) allocator.
//
// Port of tinygrad's TLSFAllocator (runtime/support/memory.py:23-115),
// without alignment or a base address.  arena_compute (materialize.c)
// uses it to give each plannable buffer an offset in one shared arena, so
// buffers with disjoint lifetimes share bytes.  The first level is
// bit_length(size), the second a linear split of [2^(b-1), 2^b); alloc
// takes the smallest non-empty fitting bin and free merges with free
// physical neighbours.

// tinygrad's planner builds TLSFAllocator(block_size=256, lv2_cnt=32)
// (schedule/memory.py:41-48); its l2_cnt is lv2_cnt.bit_length() = 6
// (runtime/support/memory.py:34).  TLSF_L2_BITS is 5, so lv2 < 16 and
// bins 16..31 stay empty.
#define TLSF_BLOCK_SIZE 256u
#define TLSF_LV2_CNT    32u
#define TLSF_L2_BITS    5u
#define TLSF_LV1_MAX    64u                 // bit_length of u64 size
#define TLSF_INVALID    0xFFFFFFFFu

// Physical-block descriptor.  Blocks form a doubly-linked list in
// address order (prev / next) covering [0, total).  Free blocks
// additionally form a singly-linked list per (lv1, lv2) bin (fnext).
typedef struct {
  u64 start;
  u64 size;
  u32 prev;                                 // adjacent prev block id, or TLSF_INVALID
  u32 next;                                 // adjacent next block id, or TLSF_INVALID
  u32 fnext;                                // next free block in same bin, or TLSF_INVALID
  u8  is_free;
  u8  alive;                                // 0 = slot is recyclable (was merged into a neighbour)
} TlsfBlock;

#define TLSF_BLOCKS_CAP 16384u
typedef struct {
  u64       total;
  TlsfBlock blocks[TLSF_BLOCKS_CAP];
  u32       n_blocks;                       // bump (incl. dead slots; recycled by free_slot)
  u32       free_slots[TLSF_BLOCKS_CAP];    // stack of recycled ids
  u32       free_slot_top;
  u32       bins[TLSF_LV1_MAX][TLSF_LV2_CNT]; // head block id per bin, TLSF_INVALID = empty
  u32       lv1_count[TLSF_LV1_MAX];
} TlsfAllocator;

// Python's int.bit_length.
static u32 tlsf_bit_length(u64 v) {
  if (v == 0) return 0;
  u32 b = 0;
  while (v) { b++; v >>= 1; }
  return b;
}

static u32 tlsf_lv1(u64 size) { return tlsf_bit_length(size); }

static u32 tlsf_lv2(u64 size) {
  u32 bl = tlsf_bit_length(size);
  if (bl == 0) return 0;
  u32 base = bl - 1u;
  u64 msb_val = 1ULL << base;
  // memory.py:46 lv2: (size - (1 << (bl-1))) // (1 << max(0, bl - l2_cnt))
  u32 shift = bl > TLSF_L2_BITS ? bl - TLSF_L2_BITS : 0u;
  return (u32)((size - msb_val) >> shift);
}

static u64 tlsf_round_up(u64 v, u64 align) {
  if (align == 0) return v;
  return (v + align - 1) / align * align;
}

static u32 tlsf_block_new(TlsfAllocator *a) {
  u32 id;
  if (a->free_slot_top > 0) {
    id = a->free_slots[--a->free_slot_top];
  } else {
    if (a->n_blocks >= TLSF_BLOCKS_CAP) {
      fprintf(stderr, "tlsf: block pool exhausted (cap=%u)\n", TLSF_BLOCKS_CAP);
      exit(1);
    }
    id = a->n_blocks++;
  }
  TlsfBlock *b = &a->blocks[id];
  b->start = 0; b->size = 0;
  b->prev = b->next = b->fnext = TLSF_INVALID;
  b->is_free = 0; b->alive = 1;
  return id;
}

static void tlsf_block_free_slot(TlsfAllocator *a, u32 id) {
  a->blocks[id].alive = 0;
  a->free_slots[a->free_slot_top++] = id;
}

static void tlsf_bin_insert(TlsfAllocator *a, u32 bid) {
  TlsfBlock *b = &a->blocks[bid];
  u32 l1 = tlsf_lv1(b->size);
  u32 l2 = tlsf_lv2(b->size);
  if (l1 >= TLSF_LV1_MAX) {
    fprintf(stderr, "tlsf: lv1=%u out of range for size=%llu\n",
            l1, (unsigned long long)b->size);
    exit(1);
  }
  b->fnext = a->bins[l1][l2];
  a->bins[l1][l2] = bid;
  a->lv1_count[l1]++;
  b->is_free = 1;
}

static void tlsf_bin_remove(TlsfAllocator *a, u32 bid) {
  TlsfBlock *b = &a->blocks[bid];
  u32 l1 = tlsf_lv1(b->size);
  u32 l2 = tlsf_lv2(b->size);
  u32 *cur = &a->bins[l1][l2];
  while (*cur != TLSF_INVALID) {
    if (*cur == bid) {
      *cur = a->blocks[bid].fnext;
      a->blocks[bid].fnext = TLSF_INVALID;
      a->lv1_count[l1]--;
      b->is_free = 0;
      return;
    }
    cur = &a->blocks[*cur].fnext;
  }
  fprintf(stderr, "tlsf: bid=%u not found in bin (%u,%u)\n", bid, l1, l2);
  exit(1);
}

// Splits free block bid (size > new_size) into two adjacent free blocks,
// bid keeping the first new_size bytes (memory.py:62-67 _split_block).
static void tlsf_split(TlsfAllocator *a, u32 bid, u64 new_size) {
  TlsfBlock *b = &a->blocks[bid];
  u64 old_size = b->size;
  u32 old_next = b->next;
  tlsf_bin_remove(a, bid);
  b->size = new_size;
  tlsf_bin_insert(a, bid);
  u32 tail_id = tlsf_block_new(a);
  TlsfBlock *tail = &a->blocks[tail_id];
  tail->start = b->start + new_size;
  tail->size  = old_size - new_size;
  tail->prev  = bid;
  tail->next  = old_next;
  tlsf_bin_insert(a, tail_id);
  if (old_next != TLSF_INVALID) {
    a->blocks[old_next].prev = tail_id;
  }
  a->blocks[bid].next = tail_id;
}

// Absorbs every free block to the right of free block bid
// (memory.py:69-79 _merge_right).
static void tlsf_merge_right(TlsfAllocator *a, u32 bid) {
  TlsfBlock *b = &a->blocks[bid];
  if (!b->is_free) return;
  while (b->next != TLSF_INVALID && a->blocks[b->next].is_free) {
    u32 nb = b->next;
    TlsfBlock *n = &a->blocks[nb];
    u64 grown = b->size + n->size;
    u32 nb_next = n->next;
    tlsf_bin_remove(a, bid);
    tlsf_bin_remove(a, nb);
    b->size = grown;
    b->next = nb_next;
    if (nb_next != TLSF_INVALID) a->blocks[nb_next].prev = bid;
    tlsf_block_free_slot(a, nb);
    tlsf_bin_insert(a, bid);
  }
}

// Walk left while neighbours are free, then merge right from there
// (memory.py:81-84 _merge_block).
static void tlsf_merge_block(TlsfAllocator *a, u32 bid) {
  while (a->blocks[bid].prev != TLSF_INVALID
         && a->blocks[a->blocks[bid].prev].is_free) {
    bid = a->blocks[bid].prev;
  }
  tlsf_merge_right(a, bid);
}

// After init one free block covers [0, total).
static void tlsf_init(TlsfAllocator *a, u64 total) {
  memset(a, 0, sizeof(*a));
  for (u32 l1 = 0; l1 < TLSF_LV1_MAX; l1++) {
    for (u32 l2 = 0; l2 < TLSF_LV2_CNT; l2++) a->bins[l1][l2] = TLSF_INVALID;
  }
  a->total = total;
  if (total == 0) return;
  u32 root = tlsf_block_new(a);
  a->blocks[root].start = 0;
  a->blocks[root].size  = total;
  a->blocks[root].prev  = TLSF_INVALID;
  a->blocks[root].next  = TLSF_INVALID;
  tlsf_bin_insert(a, root);
}

// The storage is inline in the allocator, so there is nothing to release.
static void tlsf_dispose(TlsfAllocator *a) {
  (void)a;
}

// The live block starting at `start`, by linear scan (tlsf_free runs once
// per buffer).
static u32 tlsf_find_by_start(TlsfAllocator *a, u64 start) {
  for (u32 i = 0; i < a->n_blocks; i++) {
    if (!a->blocks[i].alive) continue;
    if (a->blocks[i].start == start) return i;
  }
  return TLSF_INVALID;
}

// The offset of a fresh `req_size`-byte block, or (u64)-1 when none fits
// (memory.py:86-112 alloc).
static u64 tlsf_alloc(TlsfAllocator *a, u64 req_size) {
  if (req_size < TLSF_BLOCK_SIZE) req_size = TLSF_BLOCK_SIZE;
  u64 size = req_size;
  // Round up to the next bucket so any entry there can fit (memory.py:91).
  u32 size_bl = tlsf_bit_length(size);
  u32 round_shift = size_bl > TLSF_L2_BITS ? size_bl - TLSF_L2_BITS : 0u;
  u64 round_align = 1ULL << round_shift;
  size = tlsf_round_up(size, round_align);

  u32 start_l1 = tlsf_lv1(size);
  u32 start_l2 = tlsf_lv2(size);
  // Only size's own first level starts at its lv2 bin (memory.py:96).
  for (u32 l1 = start_l1; l1 < TLSF_LV1_MAX; l1++) {
    if (a->lv1_count[l1] == 0) continue;
    u32 l2_start = (l1 == start_l1) ? start_l2 : 0u;
    for (u32 l2 = l2_start; l2 < TLSF_LV2_CNT; l2++) {
      u32 bid = a->bins[l1][l2];
      if (bid == TLSF_INVALID) continue;
      if (a->blocks[bid].size > req_size) {
        tlsf_split(a, bid, req_size);
      }
      tlsf_bin_remove(a, bid);
      return a->blocks[bid].start;
    }
  }
  return (u64)-1;
}

// memory.py:114-115 free, plus unknown-offset and double-free checks.
static void tlsf_free(TlsfAllocator *a, u64 start) {
  u32 bid = tlsf_find_by_start(a, start);
  if (bid == TLSF_INVALID) {
    fprintf(stderr, "tlsf: free of unknown offset %llu\n",
            (unsigned long long)start);
    exit(1);
  }
  if (a->blocks[bid].is_free) {
    fprintf(stderr, "tlsf: double-free of offset %llu\n",
            (unsigned long long)start);
    exit(1);
  }
  tlsf_bin_insert(a, bid);
  tlsf_merge_block(a, bid);
}
