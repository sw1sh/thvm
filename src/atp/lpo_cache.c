// Stage 8: LPO/KBO orientability cache.
//
// Direct-mapped, epoch-stamped cache keyed by `(atp_term_struct_hash(s),
// atp_term_struct_hash(t))`.  Memoises the verdict returned by
// `atp_compare_uncached` so the hot reduction-order checks
// (atp_ordered_try_top, RHS-vs-LHS gates, interreduce, FT-norm) re-use
// the same comparison once per (structural-hash, epoch).
//
// Gated entirely by THVM_ATP_LPO_ORIENT_CACHE: with the flag off this
// file compiles to nothing and the default build is byte-identical.
// Cache invalidations are wired at thvm_atp_init, after GC, and at every
// thvm_kbo_invalidate / thvm_lpo_invalidate site.
//
// Soundness: the verdict only depends on `(lhs, rhs, precedence)`; the
// precedence is fixed within an epoch (a precedence change bumps the
// epoch via the explicit invalidate hook).  Structural-hash collisions
// silently recompute -- a 128-bit FNV-64 pair collision is astronomical
// (< 2^-64 per probe), and `THVM_ATPFT_NORM_VERIFY=1` runs the
// uncached path alongside and aborts on disagreement.
//
// See docs/atp/atpft_plan.md Stage 8.

#ifdef THVM_ATP_LPO_ORIENT_CACHE

#ifndef ATP_LPO_ORIENT_BITS
#define ATP_LPO_ORIENT_BITS 16
#endif
#define ATP_LPO_ORIENT_SIZE (1u << ATP_LPO_ORIENT_BITS)
#define ATP_LPO_ORIENT_MASK (ATP_LPO_ORIENT_SIZE - 1u)

// Cache entry.  `verdict` stores the raw `KboCmp` byte (EQ=0, GT=1,
// LT=-1==255, UN=2).  Entries are validated by epoch + both stored
// hashes -- collisions recompute, never return a stale verdict.
typedef struct {
  u64 lh;
  u64 rh;
  u32 epoch;
  i8  verdict;
  u8  _pad[3];
} AtpLpoOrientEnt;

static AtpLpoOrientEnt g_atp_lpo_orient[ATP_LPO_ORIENT_SIZE];
static u32             g_atp_lpo_orient_epoch = 1u;

// Stats counters -- exposed via thvm_atp_orient_stats for hit-rate
// reporting on the AndAssoc bench.  Zero-initialised; reset on epoch
// bump so each run reports its own numbers.
static u64 g_atp_lpo_orient_probes     = 0;
static u64 g_atp_lpo_orient_hits       = 0;
static u64 g_atp_lpo_orient_swap_hits  = 0;
static u64 g_atp_lpo_orient_collisions = 0;

static inline u32 atp_lpo_orient_hash(u64 lh, u64 rh) {
  u64 h = lh * 0x9E3779B97F4A7C15ull ^ rh * 0xBF58476D1CE4E5B9ull;
  h ^= h >> 29;
  return (u32)h & ATP_LPO_ORIENT_MASK;
}

// Bump the epoch (O(1) invalidation).  On wraparound clear epochs to 0.
// Stats are reset on invalidation so per-run measurements are clean.
fn void atp_lpo_orient_cache_invalidate(void) {
  if (++g_atp_lpo_orient_epoch == 0u) {
    for (u32 i = 0; i < ATP_LPO_ORIENT_SIZE; i++)
      g_atp_lpo_orient[i].epoch = 0u;
    g_atp_lpo_orient_epoch = 1u;
  }
  g_atp_lpo_orient_probes     = 0;
  g_atp_lpo_orient_hits       = 0;
  g_atp_lpo_orient_swap_hits  = 0;
  g_atp_lpo_orient_collisions = 0;
}

// Look up a cached verdict.  Returns 1 on hit (with *out set to the
// verdict), 0 on miss.  Probes the swapped slot too: the order is
// antisymmetric (lpo(a,b) ~ INV(lpo(b,a))) so a (b,a)-keyed hit yields
// (a,b)'s verdict by inverting GT <-> LT.
static inline int atp_lpo_orient_cache_get(u64 lh, u64 rh, KboCmp *out) {
  g_atp_lpo_orient_probes++;
  u32 idx = atp_lpo_orient_hash(lh, rh);
  AtpLpoOrientEnt *e = &g_atp_lpo_orient[idx];
  if (e->epoch == g_atp_lpo_orient_epoch && e->lh == lh && e->rh == rh) {
    g_atp_lpo_orient_hits++;
    *out = (KboCmp)e->verdict;
    return 1;
  }
  // Symmetric probe: same epoch + swapped hashes invert GT/LT.
  u32 idx2 = atp_lpo_orient_hash(rh, lh);
  AtpLpoOrientEnt *e2 = &g_atp_lpo_orient[idx2];
  if (e2->epoch == g_atp_lpo_orient_epoch && e2->lh == rh && e2->rh == lh) {
    g_atp_lpo_orient_swap_hits++;
    KboCmp v = (KboCmp)e2->verdict;
    if (v == KBO_GT) v = KBO_LT;
    else if (v == KBO_LT) v = KBO_GT;
    *out = v;
    return 1;
  }
  return 0;
}

// Store a freshly-computed verdict.  No chaining, no LRU -- a collision
// overwrites.  The verdict is the raw 4-valued KboCmp.
static inline void atp_lpo_orient_cache_put(u64 lh, u64 rh, KboCmp v) {
  u32 idx = atp_lpo_orient_hash(lh, rh);
  AtpLpoOrientEnt *e = &g_atp_lpo_orient[idx];
  if (e->epoch == g_atp_lpo_orient_epoch &&
      (e->lh != lh || e->rh != rh)) {
    g_atp_lpo_orient_collisions++;
  }
  e->lh      = lh;
  e->rh      = rh;
  e->epoch   = g_atp_lpo_orient_epoch;
  e->verdict = (i8)v;
}

// Read the running counters (probes, hits, swap_hits, collisions).
// Any pointer may be NULL.  Exposed so bench harnesses + tests can
// report hit-rate on a representative workload.
fn void thvm_atp_orient_stats(u64 *probes, u64 *hits,
                              u64 *swap_hits, u64 *collisions) {
  if (probes     != NULL) *probes     = g_atp_lpo_orient_probes;
  if (hits       != NULL) *hits       = g_atp_lpo_orient_hits;
  if (swap_hits  != NULL) *swap_hits  = g_atp_lpo_orient_swap_hits;
  if (collisions != NULL) *collisions = g_atp_lpo_orient_collisions;
}

// Differential-mode gate: under THVM_ATPFT_NORM_VERIFY=1 the wrapped
// atp_compare runs BOTH the cached and the uncached paths and aborts
// on disagreement.  Probed once and cached -- the env knob does not
// change at runtime.
static int atp_lpo_orient_verify_gate(void) {
  static int gate = -1;
  if (gate < 0) {
    const char *e = getenv("THVM_ATPFT_NORM_VERIFY");
    gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return gate;
}

#endif /* THVM_ATP_LPO_ORIENT_CACHE */
