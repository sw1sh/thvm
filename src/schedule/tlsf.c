// schedule/tlsf.c - Two-Level Segregated Fit allocator.
//
// Faithful C port of tinygrad/runtime/support/memory.py TLSFAllocator
// (lines 14-106).  Used by the per-realize arena memory planner
// (schedule/mem_plan.c) to assign each plannable buffer an offset
// into a shared arena: non-overlapping lifetimes share bytes.
//
// First level: MSB(size).  Second level: TLSF_LV2_CNT linear bins
// within each lv1 covering the [2^(b-1), 2^b) range.  alloc() picks
// the smallest-bucket-first non-empty block; free() merges with
// adjacent free neighbours via a doubly-linked physical-block list.
//
// Single-pass usage: tlsf_init(...) -> alloc/free pairs in event
// order -> tlsf_dispose().  All storage is owned by the allocator
// (no caller-side block ids).

// tinygrad memory.py:24 block_size=16; thvm uses 256 to match the
// caller (mem_plan.c rounds nbytes up to TLSF_BLOCK_SIZE per buf).
// 256B matches tinygrad/schedule/memory.py:41 block_size=256.
#define TLSF_BLOCK_SIZE 256u
#define TLSF_LV2_CNT    32u                 // tinygrad memory.py:24 lv2_cnt=32
#define TLSF_L2_BITS    5u                  // log2(TLSF_LV2_CNT)
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

// --- bit_length / lv1 / lv2 helpers (tinygrad memory.py:34-37) ---

static u32 tlsf_bit_length(u64 v) {
  // Position of highest set bit + 1 (Python's int.bit_length).
  if (v == 0) return 0;
  u32 b = 0;
  while (v) { b++; v >>= 1; }
  return b;
}

static u32 tlsf_lv1(u64 size) { return tlsf_bit_length(size); }

static u32 tlsf_lv2(u64 size) {
  u32 bl = tlsf_bit_length(size);
  if (bl == 0) return 0;
  u32 base = bl - 1u;                       // MSB position
  u64 msb_val = 1ULL << base;
  // tinygrad: (size - (1 << (bl-1))) // (1 << max(0, bl - l2_cnt))
  // where l2_cnt = lv2_cnt.bit_length() = 5 for lv2_cnt=32 (line 25).
  u32 shift = bl > TLSF_L2_BITS ? bl - TLSF_L2_BITS : 0u;
  return (u32)((size - msb_val) >> shift);
}

static u64 tlsf_round_up(u64 v, u64 align) {
  if (align == 0) return v;
  return (v + align - 1) / align * align;
}

// --- slot allocator (over TlsfBlock pool) ---

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

// --- bin ops (singly-linked free list per (lv1,lv2)) ---

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

// --- block split / merge (mirrors memory.py:53-75) ---

// Split bid (must be free, with size > new_size) into two adjacent
// free blocks: bid keeps [start, start+new_size) at new_size; a new
// block covers [start+new_size, start+size) at (size - new_size).
// Reorders the bins for bid (size changed) and inserts the new tail.
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
  // Wire up prev pointer of the original next neighbour (if any).
  if (old_next != TLSF_INVALID) {
    a->blocks[old_next].prev = tail_id;
  }
  a->blocks[bid].next = tail_id;
}

// Merge bid with any free neighbour to the right; repeats while
// right neighbour is free.  bid must remain free (its is_free unchanged).
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

// Walk left while neighbours are free, then merge right from there.
// Mirrors memory.py:72-75 _merge_block.
static void tlsf_merge_block(TlsfAllocator *a, u32 bid) {
  while (a->blocks[bid].prev != TLSF_INVALID
         && a->blocks[a->blocks[bid].prev].is_free) {
    bid = a->blocks[bid].prev;
  }
  tlsf_merge_right(a, bid);
}

// --- public API ---

// Initialise with total bytes available.  After init, one big free
// block covers [0, total).
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

// Released only when memset-init is overwritten; keep tiny.
static void tlsf_dispose(TlsfAllocator *a) {
  (void)a;
}

// Look up a free block by start address (linear scan; only used by
// tlsf_free, which is called once per buffer at end-of-lifetime).
// O(n_blocks) but n_blocks is small (<= 2 * n_buffers).
static u32 tlsf_find_by_start(TlsfAllocator *a, u64 start) {
  for (u32 i = 0; i < a->n_blocks; i++) {
    if (!a->blocks[i].alive) continue;
    if (a->blocks[i].start == start) return i;
  }
  return TLSF_INVALID;
}

// Allocate `req_size` bytes.  Returns the offset (>= 0); on OOM
// returns (u64)-1.  Mirrors memory.py:77-103.
static u64 tlsf_alloc(TlsfAllocator *a, u64 req_size) {
  if (req_size < TLSF_BLOCK_SIZE) req_size = TLSF_BLOCK_SIZE;
  u64 size = req_size;                      // no alignment beyond block_size
  // Round up to the next bucket so any entry there can fit (line 82).
  u32 size_bl = tlsf_bit_length(size);
  u32 round_shift = size_bl > TLSF_L2_BITS ? size_bl - TLSF_L2_BITS : 0u;
  u64 round_align = 1ULL << round_shift;
  size = tlsf_round_up(size, round_align);

  u32 start_l1 = tlsf_lv1(size);
  u32 start_l2 = tlsf_lv2(size);
  // tinygrad memory.py:87 uses size.bit_length() to gate the l2 floor;
  // post-round-up size's bit_length matches start_l1, so the floor only
  // applies at l1 == start_l1 (deeper l1 buckets cover bigger ranges).
  for (u32 l1 = start_l1; l1 < TLSF_LV1_MAX; l1++) {
    if (a->lv1_count[l1] == 0) continue;
    u32 l2_start = (l1 == start_l1) ? start_l2 : 0u;
    for (u32 l2 = l2_start; l2 < TLSF_LV2_CNT; l2++) {
      u32 bid = a->bins[l1][l2];
      if (bid == TLSF_INVALID) continue;
      // Found smallest fitting block.
      if (a->blocks[bid].size > req_size) {
        tlsf_split(a, bid, req_size);
      }
      // Re-fetch bid (split may have re-binned it but bid is unchanged).
      tlsf_bin_remove(a, bid);
      return a->blocks[bid].start;
    }
  }
  return (u64)-1;                           // OOM
}

// Free a previously-allocated offset.  Mirrors memory.py:105-106.
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
