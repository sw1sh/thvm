// uop/const.c - construct a UOP_CONST node, with a small cache.
//
// Heap layout: [NUM(bits)] -- a single TAG_NUM cell carrying the
// raw bits of the constant value.  EXT of the UOP_CONST term holds
// the dtype so the reader knows how to interpret the bits.
//
// Cache: a fixed-size open-addressed hash table keyed by (dtype, bits)
// returns the existing CONST term when one was already allocated in
// this session.  CONSTs are immutable atoms (a NUM in heap[loc] +
// the wrapping UOP term) -- sharing the heap loc across references
// is always safe; materialize dedups on heap loc identity, so a
// shared CONST becomes one kernel input slot regardless of how
// many times it's referenced.
//
// Big payoff for higher-order TGrad: the chain rule emits CONST(0)
// at every TEN leaf SUP, plus CONST(ln 2) / CONST(1/ln 2) /
// CONST(0.5) inside RECIP/EXP2/LOG2/SQRT adjoints.  Without the
// cache, each invocation allocated a fresh heap cell, multiplying
// per round (NUM cells dominated the leak in the a^6-round-6
// profile -- 188K NUMs out of 622K total cells).
//
// Cleared by uop_const_cache_reset() in thvm_init / thvm_free; a
// stale entry pointing into a freed heap range would mis-resolve.
//
// Multi-context: the cached `term` carries a per-context heap loc
// (the wrapping NUM cell lives in CURRENT_CTX->heap, and heap_next
// restarts at 0 per context).  A const Term cached by context A and
// returned to context B would dereference B's heap at A's loc --
// garbage.  The key folds the current context slot id (bits 48..51)
// so cross-context lookups never alias (the FLUX cross-session bug:
// a 2nd model context reused the 1st context's cached const/movement
// Terms and read its heap -> a malformed `{}[[2]]` shape).

#define UOP_CONST_CACHE_CAP  (1u << 14)         // 16K slots, ~50% load is fine
typedef struct {
  u64 key;                                       // (ctx << 48) | (dtype << 32) | bits, 0 = empty
  Term term;                                     // cached uop_const Term
} UopConstSlot;
static UopConstSlot UOP_CONST_CACHE[UOP_CONST_CACHE_CAP];
static u32          UOP_CONST_CACHE_SIZE = 0;

fn void uop_const_cache_reset(void) {
  memset(UOP_CONST_CACHE, 0, sizeof(UOP_CONST_CACHE));
  UOP_CONST_CACHE_SIZE = 0;
}

static inline u64 uop_const_key(u32 dtype, u32 bits) {
  // Pack into a non-zero u64 key; ensure key is never 0 (we use 0
  // as the empty-slot sentinel).  dtype | bits are both u32; OR a
  // high marker bit so {dtype=0, bits=0} doesn't collide with empty.
  // Fold the context slot (0..15) into bits 48..51 so a const cached
  // in one context is never returned to another (its heap loc would
  // dereference the wrong context's heap).
  u64 ctx = (u64)thvm_context_current() & 0xF;
  return ((u64)dtype << 32) | (u64)bits | (ctx << 48) | (1ULL << 63);
}

fn Term uop_const(u32 dtype, u32 bits) {
  u64 key = uop_const_key(dtype, bits);
  u32 mask = UOP_CONST_CACHE_CAP - 1;
  // FNV-ish: combine low/high u32 of key into a u32 hash.
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13;  h *= 0x5bd1e995u;  h ^= h >> 15;
  for (u32 probe = 0; probe < UOP_CONST_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    UopConstSlot *s = &UOP_CONST_CACHE[i];
    if (s->key == key) return s->term;
    if (s->key == 0) {
      u64 loc = heap_alloc(1);
      heap_set(loc, term_new(0, TAG_NUM, dtype, bits));
      Term t = term_new(0, TAG_UOP, UOP_CONST, loc);
      s->key  = key;
      s->term = t;
      UOP_CONST_CACHE_SIZE++;
      return t;
    }
  }
  // Cache full: fall back to fresh allocation.  Shouldn't happen in
  // practice (16K slots is way more distinct constants than any
  // workload produces), but the cache should never block a build.
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, dtype, bits));
  return term_new(0, TAG_UOP, UOP_CONST, loc);
}
