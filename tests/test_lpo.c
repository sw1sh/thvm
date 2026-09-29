// test_lpo.c -- stage 8.5b: thvm_lpo Lexicographic Path Ordering.

#include "../src/thvm.c"
#include "test.h"

#define LAB_a 1u
#define LAB_b 2u
#define LAB_c 3u
#define LAB_f 4u
#define LAB_g 5u
#define LAB_h 6u
#define VAR_x 0u
#define VAR_y 1u

// Precedence: h > g > f > c > b > a.  Higher index wins.
static u32 demo_precedence[7] = {
  /* unused 0 */ 0,
  /* a       */ 1,
  /* b       */ 2,
  /* c       */ 3,
  /* f       */ 4,
  /* g       */ 5,
  /* h       */ 6,
};
static const LpoConfig DEMO_LPO = {
  .precedence = demo_precedence,
  .n_labels   = 7,
};

static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_b(void) { return term_new_ctr(LAB_b, NULL, 0); }
static Term mk_c(void) { return term_new_ctr(LAB_c, NULL, 0); }
static Term mk_f1(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_f, cs, 1); }
static Term mk_f2(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_g(Term x)          { Term cs[1] = {x};    return term_new_ctr(LAB_g, cs, 1); }
static Term mk_h(Term x)          { Term cs[1] = {x};    return term_new_ctr(LAB_h, cs, 1); }

// === DAG evaluator vs the flat recursion =================================
// lpo_dag_cmp (and the size dispatch lpo_flat_cmp) must return
// lpo_flat_ref's verdict on every pair.  Random
// terms over labels 1..3 (constants), 4..5 (unary), 6..7 (binary) and 8
// (ternary), with shared subterms, mutated and argument-swapped copies.

static u64 g_rnd = 0x2545F4914F6CDD1Dull;

static u32 rnd(u32 n) {
  g_rnd ^= g_rnd << 13;
  g_rnd ^= g_rnd >> 7;
  g_rnd ^= g_rnd << 17;
  return (u32)(g_rnd % n);
}

static const u32 RND_ARITY[9] = {0, 0, 0, 0, 1, 1, 2, 2, 3};

typedef struct {
  u32   nvar;
  u32   vbase;
  Term *pool;
  u32   npool;
} RndSpec;

static Term rnd_term(const RndSpec *sp, u32 depth) {
  u32 r = rnd(16);
  if (sp->npool > 0u && r < 3u) {
    return sp->pool[rnd(sp->npool)];
  }
  if (depth == 0u || r < 6u) {
    if (sp->nvar > 0u && rnd(3) == 0u) {
      return mk_v(sp->vbase + rnd(sp->nvar));
    }
    return term_new_ctr(1u + rnd(3), NULL, 0);
  }
  u32  lab = 4u + rnd(5);
  u32  ar  = RND_ARITY[lab];
  Term cs[3];
  for (u32 i = 0; i < ar; i++) {
    cs[i] = rnd_term(sp, depth - 1u);
  }
  return term_new_ctr(lab, cs, ar);
}

// A copy of t with one subterm replaced (mode 0) or, at the chosen node,
// its first two arguments swapped (mode 1).
static Term rnd_edit(const RndSpec *sp, Term t, u32 mode) {
  u8 ctr = (u8)(term_tag(t) == TAG_CTR && term_ctr_n(t) > 0u);
  if (!ctr || rnd(4) == 0u) {
    if (mode == 1u && ctr && term_ctr_n(t) >= 2u) {
      u32  n = term_ctr_n(t);
      Term cs[3];
      for (u32 i = 0; i < n; i++) {
        cs[i] = term_ctr_at(t, i);
      }
      Term tmp = cs[0];
      cs[0] = cs[1];
      cs[1] = tmp;
      return term_new_ctr(term_ext(t), cs, n);
    }
    return rnd_term(sp, 2u);
  }
  u32  n = term_ctr_n(t);
  u32  k = rnd(n);
  Term cs[3];
  for (u32 i = 0; i < n; i++) {
    cs[i] = (i == k) ? rnd_edit(sp, term_ctr_at(t, i), mode) : term_ctr_at(t, i);
  }
  return term_new_ctr(term_ext(t), cs, n);
}

// Both verdicts on the encoded pair; 0 when a side does not encode.
static u8 dag_vs_ref(Term s, Term t, const LpoConfig *cfg,
                     LpoCmp *ref, LpoCmp *dag) {
  u32 na = 0u;
  u32 nb = 0u;
  if (!lpo_flat_encode_rec(s, cfg, g_lpo_flat_a, &na) ||
      !lpo_flat_encode_rec(t, cfg, g_lpo_flat_b, &nb)) {
    return 0u;
  }
  *ref = lpo_flat_ref(cfg);
  *dag = lpo_dag_cmp(na, nb, cfg);
  CHECK_EQ((int)lpo_flat_cmp(na, nb, cfg), (int)*ref);
  return 1u;
}

// bin/test_lpo FILE: the differential over pairs dumped by a
// -DTHVM_LPO_SELFCHECK build (THVM_LPO_SELFCHECK_DUMP), with timings.
static int dag_check_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    fprintf(stderr, "cannot open %s\n", path);
    return 2;
  }
  u64    n = 0u, bad = 0u, ns_ref = 0u, ns_dag = 0u, worst_ref = 0u, worst_dag = 0u;
  u64    worst_ref_dag = 0u;
  u32    worst_n = 0u;
  u32   *prec = NULL;
  u32    prec_cap = 0u;
  u32    hdr[4];
  while (fread(hdr, sizeof(u32), 4, f) == 4) {
    if (hdr[0] != 0x504F504Cu || hdr[1] > KBO_FLAT_CAP || hdr[2] > KBO_FLAT_CAP) {
      fprintf(stderr, "bad record at pair %llu\n", (unsigned long long)n);
      bad++;
      break;
    }
    if (hdr[3] > prec_cap) {
      prec_cap = hdr[3];
      prec = (u32 *)realloc(prec, (size_t)prec_cap * sizeof(u32));
    }
    i32 rv = 0;
    if (fread(prec, sizeof(u32), hdr[3], f) != hdr[3] ||
        fread(g_lpo_flat_a, sizeof(KboFlatNode), hdr[1], f) != hdr[1] ||
        fread(g_lpo_flat_b, sizeof(KboFlatNode), hdr[2], f) != hdr[2] ||
        fread(&rv, sizeof(i32), 1, f) != 1) {
      fprintf(stderr, "truncated record at pair %llu\n", (unsigned long long)n);
      break;
    }
    LpoConfig cfg = { .precedence = prec, .n_labels = hdr[3] };
    struct timespec t0, t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    LpoCmp ref = lpo_flat_ref(&cfg);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    LpoCmp dag = lpo_dag_cmp(hdr[1], hdr[2], &cfg);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    u64 dr = (u64)(t1.tv_sec - t0.tv_sec) * 1000000000ull + (u64)t1.tv_nsec - (u64)t0.tv_nsec;
    u64 dd = (u64)(t2.tv_sec - t1.tv_sec) * 1000000000ull + (u64)t2.tv_nsec - (u64)t1.tv_nsec;
    ns_ref += dr;
    ns_dag += dd;
    if (dr > worst_ref) {
      worst_ref     = dr;
      worst_ref_dag = dd;
      worst_n       = hdr[1] + hdr[2];
    }
    if (dd > worst_dag) {
      worst_dag = dd;
    }
    if (ref != dag || (i32)dag != rv) {
      bad++;
      fprintf(stderr, "  MISMATCH pair %llu: na=%u nb=%u ref=%d dag=%d dumped=%d\n",
              (unsigned long long)n, hdr[1], hdr[2], (int)ref, (int)dag, rv);
    }
    n++;
  }
  fclose(f);
  free(prec);
  printf("  file %s: pairs=%llu mismatches=%llu ref=%.3fs dag=%.3fs "
         "worst-ref=%.3fms (n=%u, dag %.3fms) worst-dag=%.3fms\n",
         path, (unsigned long long)n, (unsigned long long)bad,
         (double)ns_ref / 1e9, (double)ns_dag / 1e9, (double)worst_ref / 1e6,
         worst_n, (double)worst_ref_dag / 1e6, (double)worst_dag / 1e6);
  return (bad == 0u && n > 0u) ? 0 : 1;
}

int main(int argc, char **argv) {
  thvm_init();

  if (argc > 1) {
    int rc = 0;
    for (int i = 1; i < argc; i++) {
      rc |= dag_check_file(argv[i]);
    }
    thvm_free();
    return rc;
  }

  TEST_BEGIN("lpo/eq-on-identical-terms");
  {
    Term s = mk_f2(mk_a(), mk_b());
    Term t = mk_f2(mk_a(), mk_b());
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_EQ);
  }

  TEST_BEGIN("lpo/gt-via-precedence");
  {
    // h > f under our precedence, and h(_) >_lpo f(_) iff
    // h(...) >_lpo every arg of f.  h(b) vs f(a): b is the
    // arg of h, a is the arg of f.  h(b) > a holds because
    // a is a strict subterm of h(b)?  No: h(b) doesn't contain
    // a.  Use precedence path: h > a (h's prec > a's), and
    // h(b) >_lpo a iff h(b) >_lpo every arg of a (vacuous, a
    // has no args), so YES.
    Term s = mk_h(mk_b());
    Term t = mk_f1(mk_a());
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_GT);
    // Symmetric.
    CHECK_EQ((int)thvm_lpo(t, s, &DEMO_LPO), (int)LPO_LT);
  }

  TEST_BEGIN("lpo/gt-via-subterm-domination");
  {
    // f(a, b) > a directly: a is a subterm of f(a, b).
    Term s = mk_f2(mk_a(), mk_b());
    Term t = mk_a();
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_GT);
  }

  TEST_BEGIN("lpo/gt-via-lex-on-equal-heads");
  {
    // f(a, c) > f(a, b) because:
    //   - same head f
    //   - lex compare args: a == a, c > b (precedence)
    //   - dominate-all-args: f(a, c) > a (subterm) AND
    //     f(a, c) > b (subterm-via-precedence: f > b, vacuous args)
    Term s = mk_f2(mk_a(), mk_c());
    Term t = mk_f2(mk_a(), mk_b());
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_GT);
  }

  TEST_BEGIN("lpo/un-on-distinct-vars");
  {
    Term s = mk_v(VAR_x);
    Term t = mk_v(VAR_y);
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_UN);
  }

  TEST_BEGIN("lpo/var-occurs-as-strict-subterm");
  {
    // f(x) > x because x occurs strictly in f(x).
    Term s = mk_f1(mk_v(VAR_x));
    Term t = mk_v(VAR_x);
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_GT);
    // Symmetric: x < f(x).
    CHECK_EQ((int)thvm_lpo(t, s, &DEMO_LPO), (int)LPO_LT);
  }

  TEST_BEGIN("lpo/var-not-in-term-incomparable");
  {
    // f(a) does NOT contain x; x doesn't dominate f(a) either.
    // Result is UN.
    Term s = mk_f1(mk_a());
    Term t = mk_v(VAR_x);
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_UN);
    CHECK_EQ((int)thvm_lpo(t, s, &DEMO_LPO), (int)LPO_UN);
  }

  TEST_BEGIN("lpo/eq-fvr-same-id");
  {
    Term s = mk_v(VAR_x);
    Term t = mk_v(VAR_x);
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_EQ);
  }

  TEST_BEGIN("lpo/group-axiom-orient-gt");
  {
    // Real-world fixture pattern: the right-id rule
    // f(x, e) -> x must orient LPO_GT under appropriate
    // precedence.  Use e=a (sentinel for `e`) for simplicity.
    // f(x, a) > x because x is a strict subterm of f(x, a).
    Term lhs = mk_f2(mk_v(VAR_x), mk_a());
    Term rhs = mk_v(VAR_x);
    CHECK_EQ((int)thvm_lpo(lhs, rhs, &DEMO_LPO), (int)LPO_GT);
  }

  TEST_BEGIN("lpo/permuted-args-lex-un-takes-lt-leg");
  {
    // f(x, y) vs f(y, x): the first arguments are incomparable, and the
    // flat recursion takes an UN lex verdict down the LT leg (y, x are
    // both below f(y, x)), so each side compares LT to the other.  The
    // DAG evaluator reproduces the verdict, quirk included.
    Term s = mk_f2(mk_v(VAR_x), mk_v(VAR_y));
    Term t = mk_f2(mk_v(VAR_y), mk_v(VAR_x));
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_LT);
    CHECK_EQ((int)thvm_lpo(t, s, &DEMO_LPO), (int)LPO_LT);
  }

  TEST_BEGIN("lpo/dag-agrees-with-flat-recursion");
  {
    // Injective precedence, a tied one (equal-precedence heads of
    // different arity reach the lex leg), and a short table (labels past
    // n_labels read precedence 0); variable ids below 64, straddling it,
    // and above it (the scan path).
    static u32 prec_inj[9] = {0, 3, 1, 2, 6, 4, 8, 5, 7};
    static u32 prec_tie[9] = {0, 1, 2, 1, 2, 3, 3, 2, 3};
    static u32 prec_short[6] = {0, 2, 1, 3, 5, 4};
    const LpoConfig cfgs[3] = {
      { .precedence = prec_inj,   .n_labels = 9 },
      { .precedence = prec_tie,   .n_labels = 9 },
      { .precedence = prec_short, .n_labels = 6 },
    };
    static const u32 vbases[3] = {0u, 62u, 100u};
    u64 fallbacks0 = g_lpo_dag_fallbacks;
    u32 checked    = 0u;
    for (u32 ci = 0; ci < 3u; ci++) {
      for (u32 vi = 0; vi < 3u; vi++) {
        Term    pool[24];
        RndSpec sp = { .nvar = 4u, .vbase = vbases[vi], .pool = pool, .npool = 0u };
        for (u32 k = 0; k < 24u; k++) {
          pool[k] = rnd_term(&sp, 3u);
        }
        sp.npool = 24u;
        for (u32 it = 0; it < 1500u; it++) {
          Term s = rnd_term(&sp, 6u);
          Term t;
          switch (rnd(5)) {
            case 0:  t = rnd_term(&sp, 6u);    break;
            case 1:  t = rnd_edit(&sp, s, 0u); break;
            case 2:  t = rnd_edit(&sp, s, 1u); break;
            case 3:  t = rnd_edit(&sp, rnd_edit(&sp, s, 1u), 0u); break;
            default: t = sp.pool[rnd(24)];     break;
          }
          LpoCmp ref = LPO_EQ;
          LpoCmp dag = LPO_EQ;
          if (dag_vs_ref(s, t, &cfgs[ci], &ref, &dag)) {
            CHECK_EQ((int)dag, (int)ref);
            checked++;
          }
          if (dag_vs_ref(t, s, &cfgs[ci], &ref, &dag)) {
            CHECK_EQ((int)dag, (int)ref);
            checked++;
          }
        }
      }
    }
    CHECK(checked >= 27000u);
    CHECK_EQ(g_lpo_dag_fallbacks, fallbacks0);
  }

  TEST_BEGIN("lpo/dag-arity-clash-falls-back");
  {
    // Label 6 at arity 2 in s and arity 1 in t: class equality is no
    // longer slice equality, so the compare takes the flat recursion.
    Term a1   = term_new_ctr(1u, NULL, 0);
    Term cs2[2] = {a1, mk_v(VAR_x)};
    Term cs1[1] = {a1};
    Term s = term_new_ctr(6u, cs2, 2);
    Term t = term_new_ctr(6u, cs1, 1);
    static u32 prec[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    const LpoConfig cfg = { .precedence = prec, .n_labels = 9 };
    u64    fallbacks0 = g_lpo_dag_fallbacks;
    LpoCmp ref = LPO_EQ;
    LpoCmp dag = LPO_EQ;
    CHECK(dag_vs_ref(s, t, &cfg, &ref, &dag));
    CHECK_EQ((int)dag, (int)ref);
    CHECK(g_lpo_dag_fallbacks > fallbacks0);
  }

  TEST_BEGIN("lpo/dag-shared-subterms-bounded");
  {
    // Fibonacci terms F(k) = f(F(k-1), F(k-2)): the tree doubles every
    // two levels while the DAG has k + 1 classes.  The flat recursion's
    // position pairs grow with the tree product; the DAG computes at
    // most (#classes)^2 class pairs.
    static u32 prec[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    const LpoConfig cfg = { .precedence = prec, .n_labels = 9 };
    Term fs[24];
    Term ft[24];
    fs[0] = mk_v(VAR_x);
    fs[1] = term_new_ctr(2u, NULL, 0);
    ft[0] = mk_v(VAR_x);
    ft[1] = term_new_ctr(3u, NULL, 0);
    for (u32 k = 2; k < 24u; k++) {
      Term cs[2] = {fs[k - 1u], fs[k - 2u]};
      Term ct[2] = {ft[k - 1u], ft[k - 2u]};
      fs[k] = term_new_ctr(6u, cs, 2);
      ft[k] = term_new_ctr(6u, ct, 2);
    }
    LpoCmp ref = LPO_EQ;
    LpoCmp dag = LPO_EQ;
    for (u32 k = 8; k <= 14u; k += 3u) {
      CHECK(dag_vs_ref(fs[k], ft[k], &cfg, &ref, &dag));
      CHECK_EQ((int)dag, (int)ref);
      CHECK(dag_vs_ref(ft[k], fs[k - 1u], &cfg, &ref, &dag));
      CHECK_EQ((int)dag, (int)ref);
    }
#ifndef THVM_LPO_SELFCHECK
    // F(22) has 57313 nodes; the DAG compare stays within 46^2 class
    // pairs (the self-check build would also run the flat recursion).
    u32 na = 0u;
    u32 nb = 0u;
    CHECK(lpo_flat_encode_rec(fs[22], &cfg, g_lpo_flat_a, &na));
    CHECK(lpo_flat_encode_rec(ft[22], &cfg, g_lpo_flat_b, &nb));
    CHECK_EQ(na, 57313u);
    u64 c0 = g_lpo_flat_compute_calls;
    (void)lpo_dag_cmp(na, nb, &cfg);
    CHECK(g_lpo_flat_compute_calls - c0 <= 46u * 46u);
#endif
  }

  thvm_free();
  TEST_REPORT();
}
