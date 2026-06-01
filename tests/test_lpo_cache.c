// test_lpo_cache.c -- Stage 8 LPO/KBO orientability cache.
//
// T1 Round-trip:      cache_put(h1,h2,GT); cache_get(h1,h2) == GT.
// T2 Invalidation:    cache_put; invalidate(); cache_get == miss.
// T3 Differential:    10k random Term pairs; cached verdict ==
//                     thvm_kbo verdict on every one.
// T4 Hit rate:        100k Term pairs with repeats; report hit rate.
//
// Built with -DTHVM_ATP_LPO_ORIENT_CACHE so the symbol surface of
// src/atp/lpo_cache.c is live (the cache APIs collapse to nothing in
// the default build).

#include "../src/thvm.c"
#include "test.h"

#ifndef THVM_ATP_LPO_ORIENT_CACHE
// The Makefile rule for this target sets the flag; bail out clearly
// if it gets built without it (e.g. invoked by hand).
int main(void) {
  fprintf(stderr, "test_lpo_cache requires -DTHVM_ATP_LPO_ORIENT_CACHE\n");
  return 1;
}
#else

// Hooks defined in src/atp/lpo_cache.c + src/atp/_.c.  All symbols
// live in the same translation unit (this test #includes src/thvm.c
// which pulls them in), so the `static` / `static inline` definitions
// are directly callable from this main() without extra declarations.
// The `fn`-linked entry points (thvm_atp_orient_stats,
// atp_lpo_orient_cache_invalidate) are likewise visible TU-wide.

// Group-signature KBO config -- borrowed from test_kbo.c so the
// differential test (T3) has a concrete oracle.  Six labels: { _, e,
// i, f, a, g }.  Precedence g > i > f > e > a.  Weights all 1 except i
// = 0 (Waldmeister default).
#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define LAB_g 5u
#define LAB_MAX 6u

static u32 group_weights   [LAB_MAX] = {0, 1, 0, 1, 1, 1};
static u32 group_precedence[LAB_MAX] = {0, 2, 4, 3, 1, 5};

static const KboConfig GROUP_CFG = {
  .weights     = group_weights,
  .precedence  = group_precedence,
  .n_labels    = LAB_MAX,
  .var_weight  = 1,
};

static Term mk_e(void)              { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void)              { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_i(Term x)            { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }
static Term mk_f(Term x, Term y)    { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_g(Term x)            { Term cs[1] = {x};    return term_new_ctr(LAB_g, cs, 1); }

// xorshift64* PRNG -- deterministic across runs so failures reproduce.
static u64 g_rng = 0xcafef00dd15ea5e5ull;
static u32 prng_u32(void) {
  u64 x = g_rng;
  x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
  g_rng = x;
  return (u32)((x * 2685821657736338717ull) >> 32);
}

// Random small Term builder over the group signature.  depth keeps the
// recursion bounded -- ~5 levels max so the corpus stays cheap.
static Term mk_random_term(int depth) {
  if (depth <= 0) {
    // leaf: either a constant or a variable
    u32 c = prng_u32() % 5;
    switch (c) {
      case 0: return mk_e();
      case 1: return mk_a();
      case 2: return term_new_fvr(0);
      case 3: return term_new_fvr(1);
      default: return term_new_fvr(2);
    }
  }
  u32 c = prng_u32() % 6;
  switch (c) {
    case 0: return mk_e();
    case 1: return mk_a();
    case 2: return mk_g(mk_random_term(depth - 1));
    case 3: return mk_i(mk_random_term(depth - 1));
    case 4: return mk_f(mk_random_term(depth - 1), mk_random_term(depth - 1));
    default: return term_new_fvr(prng_u32() % 3);
  }
}

int main(void) {
  thvm_init();

  // T1 Round-trip.  Put a verdict for a synthetic hash pair, read it
  // back -- every (lh, rh, verdict) combination round-trips.
  TEST_BEGIN("lpo-cache/round-trip");
  {
    atp_lpo_orient_cache_invalidate();
    KboCmp verdicts[4] = {KBO_EQ, KBO_GT, KBO_LT, KBO_UN};
    for (int i = 0; i < 4; i++) {
      u64 lh = 0xdeadbeef00000000ull | (u64)i;
      u64 rh = 0xfeedfacecafe0000ull | (u64)i;
      atp_lpo_orient_cache_put(lh, rh, verdicts[i]);
      KboCmp got = KBO_UN;
      CHECK_EQ(atp_lpo_orient_cache_get(lh, rh, &got), 1);
      CHECK_EQ((int)got, (int)verdicts[i]);
    }
  }

  // T2 Invalidation.  Put, invalidate, get must miss.
  TEST_BEGIN("lpo-cache/invalidation");
  {
    atp_lpo_orient_cache_invalidate();
    u64 lh = 0xabcdef0123456789ull;
    u64 rh = 0x9876543210fedcbaull;
    atp_lpo_orient_cache_put(lh, rh, KBO_GT);
    KboCmp got = KBO_UN;
    CHECK_EQ(atp_lpo_orient_cache_get(lh, rh, &got), 1);
    CHECK_EQ((int)got, (int)KBO_GT);
    atp_lpo_orient_cache_invalidate();
    CHECK_EQ(atp_lpo_orient_cache_get(lh, rh, &got), 0);
  }

  // T3 Differential.  10k random Term pairs over the group signature;
  // every cached verdict must match the oracle (atp_compare_uncached,
  // which dispatches to thvm_kbo).  Run BOTH `atp_compare` (cached)
  // and `atp_compare_uncached` and assert equality every time.
  TEST_BEGIN("lpo-cache/differential-kbo");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 1u << 14);
    CHECK(s != NULL);
    atp_lpo_orient_cache_invalidate();
    g_rng = 0xcafef00dd15ea5e5ull;
    int n = 10000;
    int mismatches = 0;
    for (int i = 0; i < n; i++) {
      Term lhs = mk_random_term(3);
      Term rhs = mk_random_term(3);
      KboCmp fresh  = atp_compare_uncached(s, lhs, rhs);
      KboCmp cached = atp_compare(s, lhs, rhs);
      if (fresh != cached) mismatches++;
    }
    CHECK_EQ(mismatches, 0);
    thvm_atp_free(s);
  }

  // T4 Hit rate.  100k Term pairs drawn from a small corpus (~64
  // distinct terms) so structural-hash collisions are minimal but the
  // SAME pair is revisited often; report hits+swap_hits / probes.
  TEST_BEGIN("lpo-cache/hit-rate");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 1u << 14);
    CHECK(s != NULL);
    atp_lpo_orient_cache_invalidate();
    // Build a small fixed corpus.
    enum { CORPUS = 64 };
    Term corpus[CORPUS];
    g_rng = 0x12345678abcdef01ull;
    for (int i = 0; i < CORPUS; i++) corpus[i] = mk_random_term(2);
    int n = 100000;
    for (int i = 0; i < n; i++) {
      u32 a = prng_u32() % CORPUS;
      u32 b = prng_u32() % CORPUS;
      (void)atp_compare(s, corpus[a], corpus[b]);
    }
    u64 probes, hits, swap_hits, collisions;
    thvm_atp_orient_stats(&probes, &hits, &swap_hits, &collisions);
    u64 total_hits = hits + swap_hits;
    double rate = (probes > 0) ? (double)total_hits / (double)probes : 0.0;
    printf("  lpo-cache/hit-rate: probes=%llu hits=%llu swap=%llu coll=%llu rate=%.3f\n",
           (unsigned long long)probes, (unsigned long long)hits,
           (unsigned long long)swap_hits, (unsigned long long)collisions, rate);
    // With 64 corpus x 64 = 4096 distinct ordered pairs and 100k probes,
    // we expect well over 90% hit rate (most pairs hit on the 2nd+
    // visit).  Use a conservative 0.80 threshold so noise tolerated.
    CHECK(rate >= 0.80);
    thvm_atp_free(s);
  }

  TEST_REPORT();
}

#endif /* THVM_ATP_LPO_ORIENT_CACHE */
