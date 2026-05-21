// test_kbo_linear.c - differential test of the linear KBO comparator
// (thvm_kbo, Loechner) against the naive Baader-Nipkow oracle
// (thvm_kbo_naive).  Both compute the SAME ordering; any differing
// verdict is a bug in the linear port.
//
// Generates a large number of random term pairs over a small
// signature (function symbols of arity 0-3, a handful of variables,
// several weight/precedence configs including weight ties and a
// nonzero var_weight), plus deterministic deep left-spine / right-
// spine / repeated-variable stress terms (the O(n^2) and domination/UN
// cases), and asserts:
//   1. thvm_kbo(s,t) == thvm_kbo_naive(s,t) for every pair,
//   2. directional consistency: GT<->LT under swap, EQ/UN symmetric.

#include "../src/thvm.c"
#include "test.h"

#define NLAB 6u   // labels 0..5 ; label 0 reserved (treated like any other)
#define NVAR 4u   // variable ids 0..3

// --- small xorshift PRNG (deterministic, reproducible) ---------------
static u64 rng_state = 0x9e3779b97f4a7c15ULL;
static u64 rng_next(void) {
  u64 x = rng_state;
  x ^= x << 13; x ^= x >> 7; x ^= x << 17;
  rng_state = x;
  return x;
}
static u32 rng_u(u32 n) { return (u32)(rng_next() % n); }

// --- random term generator -------------------------------------------
// depth-limited; at depth 0 forces a leaf (constant or variable).
static Term gen_term(u32 depth) {
  // ~35% variable when allowed
  if (rng_u(100) < 35) return term_new_fvr(rng_u(NVAR));
  if (depth == 0) {
    // constant: arity-0 ctr
    return term_new_ctr(rng_u(NLAB), NULL, 0);
  }
  u32 arity = rng_u(4);   // 0..3
  if (arity == 0) return term_new_ctr(rng_u(NLAB), NULL, 0);
  Term cs[3];
  for (u32 i = 0; i < arity; i++) cs[i] = gen_term(depth - 1);
  return term_new_ctr(rng_u(NLAB), cs, arity);
}

// --- deterministic stress terms --------------------------------------
// left spine: f(f(f(... leaf, c), c), c) using a binary symbol.
static Term left_spine(u32 sym, u32 depth, Term leaf, Term filler) {
  Term acc = leaf;
  for (u32 i = 0; i < depth; i++) {
    Term cs[2] = {acc, filler};
    acc = term_new_ctr(sym, cs, 2);
  }
  return acc;
}
static Term right_spine(u32 sym, u32 depth, Term leaf, Term filler) {
  Term acc = leaf;
  for (u32 i = 0; i < depth; i++) {
    Term cs[2] = {filler, acc};
    acc = term_new_ctr(sym, cs, 2);
  }
  return acc;
}

// --- direction consistency -------------------------------------------
static int dir_ok(KboCmp fwd, KboCmp rev) {
  if (fwd == KBO_GT) return rev == KBO_LT;
  if (fwd == KBO_LT) return rev == KBO_GT;
  if (fwd == KBO_EQ) return rev == KBO_EQ;
  return rev == KBO_UN;   // UN symmetric
}

static const char *cmp_name(KboCmp c) {
  switch (c) {
    case KBO_EQ: return "EQ";
    case KBO_GT: return "GT";
    case KBO_LT: return "LT";
    default:     return "UN";
  }
}

static void print_term(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: printf("x%u", term_ext(t)); return;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      printf("c%u", term_ext(t));
      if (n) {
        printf("(");
        for (u32 i = 0; i < n; i++) {
          if (i) printf(",");
          print_term(term_ctr_at(t, i));
        }
        printf(")");
      }
      return;
    }
    default: printf("?"); return;
  }
}

// The signature configs to sweep.  weights[]/precedence[] indexed by
// label.  Includes weight ties, zero weights, and various var_weight.
static u32 W0[NLAB] = {1, 1, 1, 1, 1, 1};   // all equal -> heavy ties
static u32 W1[NLAB] = {0, 1, 2, 1, 3, 0};   // mixed, some zero
static u32 W2[NLAB] = {2, 2, 0, 1, 1, 2};   // duplicate weights -> ties
static u32 P0[NLAB] = {0, 1, 2, 3, 4, 5};   // strict total order
static u32 P1[NLAB] = {5, 4, 3, 2, 1, 0};   // reversed
static u32 P2[NLAB] = {0, 2, 4, 1, 3, 5};   // shuffled

static KboConfig CFGS[6];
static u32 NCFG;

static void init_cfgs(void) {
  u32 vw[3] = {0, 1, 2};
  u32 *ws[3] = {W0, W1, W2};
  u32 *ps[3] = {P0, P1, P2};
  NCFG = 0;
  // pair up weights x precedence x a couple of var_weights
  CFGS[NCFG++] = (KboConfig){W0, P0, NLAB, 1};
  CFGS[NCFG++] = (KboConfig){W1, P1, NLAB, 1};
  CFGS[NCFG++] = (KboConfig){W2, P2, NLAB, 2};
  CFGS[NCFG++] = (KboConfig){W1, P0, NLAB, 0};
  CFGS[NCFG++] = (KboConfig){W0, P2, NLAB, 2};
  CFGS[NCFG++] = (KboConfig){W2, P1, NLAB, 1};
  (void)vw; (void)ws; (void)ps;
}

static u64 g_pairs = 0;
static u64 g_mismatch = 0;
static u64 g_dir_fail = 0;

static void check_pair(Term s, Term t, const KboConfig *cfg) {
  g_pairs++;
  KboCmp lin = thvm_kbo(s, t, cfg);
  KboCmp nai = thvm_kbo_naive(s, t, cfg);
  if (lin != nai) {
    if (g_mismatch < 20) {
      printf("MISMATCH: linear=%s naive=%s\n  s = ", cmp_name(lin), cmp_name(nai));
      print_term(s);
      printf("\n  t = ");
      print_term(t);
      printf("\n");
    }
    g_mismatch++;
    return;
  }
  // directional consistency on the linear comparator
  KboCmp rev = thvm_kbo(t, s, cfg);
  if (!dir_ok(lin, rev)) {
    if (g_dir_fail < 20) {
      printf("DIR FAIL: fwd=%s rev=%s\n  s = ", cmp_name(lin), cmp_name(rev));
      print_term(s);
      printf("\n  t = ");
      print_term(t);
      printf("\n");
    }
    g_dir_fail++;
  }
}

int main(void) {
  thvm_init();
  init_cfgs();

  // === bulk random pairs ===
  TEST_BEGIN("kbo-linear/random-differential");
  {
    const u64 N = 220000;
    for (u64 it = 0; it < N; it++) {
      u32 d1 = rng_u(5);          // depth 0..4
      u32 d2 = rng_u(5);
      Term s = gen_term(d1);
      Term t = gen_term(d2);
      const KboConfig *cfg = &CFGS[rng_u(NCFG)];
      check_pair(s, t, cfg);
    }
    CHECK_EQ((int)g_mismatch, 0);
    CHECK_EQ((int)g_dir_fail, 0);
    printf("  [random] pairs=%llu mismatches=%llu dir_fails=%llu\n",
           (unsigned long long)g_pairs, (unsigned long long)g_mismatch,
           (unsigned long long)g_dir_fail);
  }

  // === deep spine stress (O(n^2) case for the naive recursion) ===
  TEST_BEGIN("kbo-linear/deep-spine-differential");
  {
    u64 base = g_pairs;
    for (u32 sym = 1; sym <= 3; sym++) {
      for (u32 depth = 1; depth <= 40; depth++) {
        Term x = term_new_fvr(0);
        Term y = term_new_fvr(1);
        Term c0 = term_new_ctr(0, NULL, 0);
        Term c1 = term_new_ctr(1, NULL, 0);
        // left/right spines with various fillers and leaves
        Term spines[8];
        u32 ns = 0;
        spines[ns++] = left_spine(sym, depth, x, c0);
        spines[ns++] = left_spine(sym, depth, y, c0);
        spines[ns++] = left_spine(sym, depth, x, c1);
        spines[ns++] = right_spine(sym, depth, x, c0);
        spines[ns++] = right_spine(sym, depth, y, c1);
        spines[ns++] = left_spine(sym, depth, c0, x);   // repeated var in filler
        spines[ns++] = right_spine(sym, depth, c1, y);
        spines[ns++] = left_spine(sym, depth, x, x);    // unbalanced repeated var
        for (u32 i = 0; i < ns; i++)
          for (u32 j = 0; j < ns; j++)
            for (u32 k = 0; k < NCFG; k++)
              check_pair(spines[i], spines[j], &CFGS[k]);
      }
    }
    CHECK_EQ((int)g_mismatch, 0);
    CHECK_EQ((int)g_dir_fail, 0);
    printf("  [spine] pairs=%llu (total now %llu) mismatches=%llu dir_fails=%llu\n",
           (unsigned long long)(g_pairs - base), (unsigned long long)g_pairs,
           (unsigned long long)g_mismatch, (unsigned long long)g_dir_fail);
  }

  // === domination / UN heavy: lopsided variable multisets ===
  TEST_BEGIN("kbo-linear/variable-domination-differential");
  {
    u64 base = g_pairs;
    Term x = term_new_fvr(0), y = term_new_fvr(1), z = term_new_fvr(2);
    Term leaves[3] = {x, y, z};
    // build terms with controlled, often-unbalanced var counts
    for (u32 rep = 0; rep < 4000; rep++) {
      u32 a1 = rng_u(3) + 1;
      u32 a2 = rng_u(3) + 1;
      Term cs1[3], cs2[3];
      for (u32 i = 0; i < a1; i++) cs1[i] = leaves[rng_u(3)];
      for (u32 i = 0; i < a2; i++) cs2[i] = leaves[rng_u(3)];
      Term s = term_new_ctr(1 + rng_u(2), cs1, a1);
      Term t = term_new_ctr(1 + rng_u(2), cs2, a2);
      check_pair(s, t, &CFGS[rng_u(NCFG)]);
    }
    CHECK_EQ((int)g_mismatch, 0);
    CHECK_EQ((int)g_dir_fail, 0);
    printf("  [vardom] pairs=%llu (total now %llu) mismatches=%llu dir_fails=%llu\n",
           (unsigned long long)(g_pairs - base), (unsigned long long)g_pairs,
           (unsigned long long)g_mismatch, (unsigned long long)g_dir_fail);
  }

  thvm_free();
  TEST_REPORT();
}
