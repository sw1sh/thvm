// uop/mov_cache.c - hash-cons cache for movement-op UOPs.
//
// RESHAPE / EXPAND / PERMUTE / FLIP / PAD / SHRINK / REDUCE allocate
// (2 + ndim) to (2 + 2*ndim) cells per construction.  In the chain
// rule's recursion, the same (op, src, args) combination often
// recurs across rounds (e.g. EXPAND(CONST(0), target.shape) appears
// in every TGrad WL wrapper at the same shape; nested rounds emit
// many copies).  Hash-cons by (op, src, args) so the second and
// later calls reuse the existing heap loc.
//
// Sharing UOP terms across references is always safe (UOPs are
// immutable; materialize dedups by heap loc identity).  This is
// the same model as the CONST cache in const.c, just with extra
// args in the key.
//
// Cleared by uop_mov_cache_reset() in thvm_init -- a stale entry
// pointing into a freed heap range would mis-resolve.
//
// Multi-context: the cached `term` AND its `src` key are per-context
// heap locs (heap_next restarts at 0 per context).  An entry cached
// by context A and looked up in context B both mis-keys (B's src loc
// collides with A's distinct node) and mis-resolves (the returned
// term dereferences B's heap at A's loc).  The key folds the current
// context slot id so cross-context entries never alias (the FLUX
// cross-session bug: a 2nd model context reused the 1st context's
// cached movement Terms and read its heap -> a malformed `{}[[2]]`
// shape).

#define UOP_MOV_CACHE_CAP (1u << 18)            // 256K slots
typedef struct {
  u64  key;                                      // 0 = empty
  Term term;                                     // cached UOP term
} UopMovSlot;
static UopMovSlot UOP_MOV_CACHE[UOP_MOV_CACHE_CAP];

fn void uop_mov_cache_reset(void) {
  memset(UOP_MOV_CACHE, 0, sizeof(UOP_MOV_CACHE));
}

// FNV-1a 64-bit hash of (op, src, vals[0..n-1]).  Result has bit 63
// forced on so the empty-slot sentinel (0) never collides with a
// valid hash.
static inline u64 uop_mov_hash(u32 op, u64 src, u32 const *vals, u32 n_vals) {
  u64 h = 0xcbf29ce484222325ULL;
  // Fold the context slot so an entry keyed in one context never
  // matches a same-(op,src,vals) construction in another (their src
  // locs collide because heap_next restarts per context).
  h ^= (u64)thvm_context_current(); h *= 0x100000001b3ULL;
  h ^= (u64)op; h *= 0x100000001b3ULL;
  h ^= src;     h *= 0x100000001b3ULL;
  for (u32 i = 0; i < n_vals; i++) {
    h ^= (u64)vals[i]; h *= 0x100000001b3ULL;
  }
  return h | (1ULL << 63);
}

// Look up; returns the cached term if hit, else 0 (= TAG_APP at
// loc=0, which uop_const ensures is never a real result).
static inline Term uop_mov_lookup(u64 key) {
  u32 mask = UOP_MOV_CACHE_CAP - 1;
  // Mix key for index probe (key already has high bit set).
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < UOP_MOV_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    UopMovSlot *s = &UOP_MOV_CACHE[i];
    if (s->key == 0) return 0;
    if (s->key == key) return s->term;
  }
  return 0;
}

// Insert (op, src, vals) -> term.  No-op if cache is full.
static inline void uop_mov_insert(u64 key, Term t) {
  u32 mask = UOP_MOV_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < UOP_MOV_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    UopMovSlot *s = &UOP_MOV_CACHE[i];
    if (s->key == 0) {
      s->key = key;
      s->term = t;
      return;
    }
  }
}
