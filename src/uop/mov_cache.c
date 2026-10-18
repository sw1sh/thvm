// Hash-cons cache for the UOp constructors: an equal (op, src, args)
// construction returns the Term built first, which is safe as UOps are
// immutable.  Reset wherever the heap is freed or compacted.

// The table doubles before it passes half full rather than drop inserts: the
// renderer names a kernel's buffer slots by UOP_BUFFER identity, so equal
// BUFFERs must stay one Term.
#define UOP_MOV_CACHE_INIT_CAP (1u << 18)
typedef struct {
  u64  key;                                      // 0 = empty
  Term term;
} UopMovSlot;
static UopMovSlot *UOP_MOV_CACHE     = NULL;
static u32         UOP_MOV_CACHE_CAP = 0;       // slots, a power of two
static u32         UOP_MOV_CACHE_LEN = 0;       // claimed slots

fn void uop_mov_cache_reset(void) {
  if (UOP_MOV_CACHE != NULL) {
    memset(UOP_MOV_CACHE, 0, (size_t)UOP_MOV_CACHE_CAP * sizeof *UOP_MOV_CACHE);
  }
  UOP_MOV_CACHE_LEN = 0;
}

static inline u32 uop_mov_home(u64 key, u32 mask) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h & mask;
}

// 0 when out of memory, the cache keeping its current contents.
static int uop_mov_cache_grow(void) {
  u32 cap = UOP_MOV_CACHE_CAP ? 2 * UOP_MOV_CACHE_CAP : UOP_MOV_CACHE_INIT_CAP;
  UopMovSlot *t = (UopMovSlot *)calloc(cap, sizeof *t);
  if (t == NULL) {
    return 0;
  }
  for (u32 j = 0; j < UOP_MOV_CACHE_CAP; j++) {
    if (UOP_MOV_CACHE[j].key == 0) {
      continue;
    }
    u32 i = uop_mov_home(UOP_MOV_CACHE[j].key, cap - 1);
    while (t[i].key != 0) {
      i = (i + 1) & (cap - 1);
    }
    t[i] = UOP_MOV_CACHE[j];
  }
  free(UOP_MOV_CACHE);
  UOP_MOV_CACHE     = t;
  UOP_MOV_CACHE_CAP = cap;
  return 1;
}

// FNV-1a of (context, op, src, vals), with bit 63 set so a key is never the
// empty slot's 0.
static inline u64 uop_mov_hash(u32 op, u64 src, u32 const *vals, u32 n_vals) {
  u64 h = 0xcbf29ce484222325ULL;
  // heap_next restarts per context, so without the context slot two contexts'
  // src locs, and the cached Terms they name, would collide.
  h ^= (u64)thvm_context_current(); h *= 0x100000001b3ULL;
  h ^= (u64)op; h *= 0x100000001b3ULL;
  h ^= src;     h *= 0x100000001b3ULL;
  for (u32 i = 0; i < n_vals; i++) {
    h ^= (u64)vals[i]; h *= 0x100000001b3ULL;
  }
  return h | (1ULL << 63);
}

// The cached Term, or 0 on a miss (a UOp Term is never 0).
static inline Term uop_mov_lookup(u64 key) {
  if (UOP_MOV_CACHE_CAP == 0) {
    return 0;
  }
  u32 mask = UOP_MOV_CACHE_CAP - 1;
  for (u32 i = uop_mov_home(key, mask); ; i = (i + 1) & mask) {
    UopMovSlot *s = &UOP_MOV_CACHE[i];
    if (s->key == 0) {
      return 0;
    }
    if (s->key == key) {
      return s->term;
    }
  }
}

// Out of memory leaves the Term uncached.
static inline void uop_mov_insert(u64 key, Term t) {
  if (2 * (UOP_MOV_CACHE_LEN + 1) > UOP_MOV_CACHE_CAP && !uop_mov_cache_grow()) {
    return;
  }
  u32 mask = UOP_MOV_CACHE_CAP - 1;
  for (u32 i = uop_mov_home(key, mask); ; i = (i + 1) & mask) {
    UopMovSlot *s = &UOP_MOV_CACHE[i];
    if (s->key == 0) {
      s->key  = key;
      s->term = t;
      UOP_MOV_CACHE_LEN++;
      return;
    }
    if (s->key == key) {
      s->term = t;
      return;
    }
  }
}
