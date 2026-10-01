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

// === Textbook reference ==================================================
// Baader & Nipkow, "Term Rewriting and All That", Def. 5.4.12, over the
// strict precedence f >_F g iff prec(f) > prec(g) (labels past n_labels
// read 0, so distinct symbols of equal precedence are incomparable):
//
//   s >lpo t iff
//     (LPO1)  t is a variable of s and s != t, or
//     (LPO2)  s = f(s_1..s_m), t = g(t_1..t_n), and
//       (a)   s_i = t or s_i >lpo t for some i, or
//       (b)   f >_F g and s >lpo t_j for every j, or
//       (c)   f = g, s >lpo t_j for every j, and s_i >lpo t_i at the
//             first i with s_i != t_i.
//
// Written from the definition alone, sharing no code with src/lpo.  A
// verdict cache on the (s, t) cell pair keeps the recursion polynomial
// without changing what it computes.

static const LpoConfig *g_ref_cfg   = NULL;
static u32              g_ref_stamp = 0u;
static u64              g_ref_both  = 0u;   // s > t and t > s at once

#define REF_MEMO_BITS 16

typedef struct {
  Term s;
  Term t;
  u32  stamp;
  u8   gt;
} RefMemo;

static RefMemo g_ref_memo[1u << REF_MEMO_BITS];

static u32 ref_prec(u32 lab) {
  return (lab < g_ref_cfg->n_labels) ? g_ref_cfg->precedence[lab] : 0u;
}

static u8 ref_same(Term s, Term t) {
  if (term_tag(s) != term_tag(t) || term_ext(s) != term_ext(t)) {
    return 0u;
  }
  if (term_tag(s) != TAG_CTR) {
    return 1u;
  }
  u32 n = term_ctr_n(s);
  if (n != term_ctr_n(t)) {
    return 0u;
  }
  for (u32 i = 0; i < n; i++) {
    if (!ref_same(term_ctr_at(s, i), term_ctr_at(t, i))) {
      return 0u;
    }
  }
  return 1u;
}

static u8 ref_var_in(u32 v, Term t) {
  if (term_tag(t) == TAG_FVR) {
    return (u8)(term_ext(t) == v);
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) {
    if (ref_var_in(v, term_ctr_at(t, i))) {
      return 1u;
    }
  }
  return 0u;
}

static u8 ref_gt(Term s, Term t);

static u8 ref_gt_compute(Term s, Term t) {
  if (term_tag(t) == TAG_FVR) {
    return (u8)(term_tag(s) == TAG_CTR && ref_var_in(term_ext(t), s));
  }
  if (term_tag(s) == TAG_FVR) {
    return 0u;
  }
  u32 m = term_ctr_n(s);
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < m; i++) {
    Term si = term_ctr_at(s, i);
    if (ref_same(si, t) || ref_gt(si, t)) {
      return 1u;
    }
  }
  u32 f = term_ext(s);
  u32 g = term_ext(t);
  u8  b = (u8)(ref_prec(f) > ref_prec(g));
  if (!b && f != g) {
    return 0u;
  }
  for (u32 j = 0; j < n; j++) {
    if (!ref_gt(s, term_ctr_at(t, j))) {
      return 0u;
    }
  }
  if (b) {
    return 1u;
  }
  for (u32 i = 0; i < m; i++) {
    Term si = term_ctr_at(s, i);
    Term ti = term_ctr_at(t, i);
    if (!ref_same(si, ti)) {
      return ref_gt(si, ti);
    }
  }
  return 0u;
}

static u8 ref_gt(Term s, Term t) {
  u64      h = (u64)s * 0x9E3779B97F4A7C15ull ^ (u64)t * 0xC2B2AE3D27D4EB4Full;
  RefMemo *e = &g_ref_memo[h >> (64 - REF_MEMO_BITS)];
  if (e->stamp == g_ref_stamp && e->s == s && e->t == t) {
    return e->gt;
  }
  u8 r = ref_gt_compute(s, t);
  e->s     = s;
  e->t     = t;
  e->stamp = g_ref_stamp;
  e->gt    = r;
  return r;
}

static LpoCmp ref_cmp(Term s, Term t, const LpoConfig *cfg) {
  g_ref_cfg = cfg;
  g_ref_stamp++;
  if (ref_same(s, t)) {
    return LPO_EQ;
  }
  u8 gt = ref_gt(s, t);
  u8 lt = ref_gt(t, s);
  if (gt && lt) {
    g_ref_both++;
  }
  return gt ? LPO_GT : lt ? LPO_LT : LPO_UN;
}

static LpoCmp lpo_inv(LpoCmp c) {
  return (c == LPO_GT) ? LPO_LT : (c == LPO_LT) ? LPO_GT : c;
}

// Every evaluator on one pair: thvm_lpo (flat recursion up to
// LPO_DAG_MIN_NODES nodes, DAG above), the flat recursion and the DAG
// on the encoded pair, the Term-tree recursion, and RPO with every
// symbol LEX.
typedef struct {
  LpoCmp top;
  LpoCmp flat;
  LpoCmp dag;
  LpoCmp tree;
  LpoCmp rpo;
  u8     encoded;
  u8     have_dag;
  u32    nodes;
} LpoAll;

static void lpo_all(Term s, Term t, const LpoConfig *cfg, LpoAll *o) {
  o->top      = thvm_lpo(s, t, cfg);
  o->have_dag = 0u;
  u32 na = 0u;
  u32 nb = 0u;
  o->encoded = (u8)(lpo_flat_encode_rec(s, cfg, g_lpo_flat_a, &na) &&
                    lpo_flat_encode_rec(t, cfg, g_lpo_flat_b, &nb));
  o->nodes = na + nb;
  if (o->encoded) {
    o->flat = lpo_flat_ref(cfg);
    if (lpo_dag_build(na, nb, cfg)) {
      o->dag      = lpo_dag_compute(g_ld_cls[0][0], g_ld_cls[1][0]);
      o->have_dag = 1u;
    }
  }
  thvm_lpo_invalidate();
  o->tree = lpo_rec(s, t, cfg);
  RpoConfig rc = { .precedence = cfg->precedence, .status = NULL,
                   .n_labels = cfg->n_labels };
  o->rpo = (LpoCmp)thvm_rpo(s, t, &rc);
}

// Mismatches per evaluator against the reference, and the first few
// pairs printed.
typedef struct {
  u64 pairs;
  u64 top;
  u64 flat;
  u64 dag;
  u64 tree;
  u64 rpo;
  u64 dag_pairs;
  u64 big_pairs;   // thvm_lpo took the DAG (more than LPO_DAG_MIN_NODES)
  u64 printed;
} DiffStats;

static void term_print(Term t, FILE *f) {
  if (term_tag(t) == TAG_FVR) {
    fprintf(f, "x%u", term_ext(t));
    return;
  }
  u32 n = term_ctr_n(t);
  fprintf(f, "s%u", term_ext(t));
  if (n > 0u) {
    fputc('(', f);
    for (u32 i = 0; i < n; i++) {
      if (i > 0u) {
        fputc(',', f);
      }
      term_print(term_ctr_at(t, i), f);
    }
    fputc(')', f);
  }
}

static LpoCmp diff_pair(Term s, Term t, const LpoConfig *cfg, DiffStats *st) {
  LpoCmp ref = ref_cmp(s, t, cfg);
  LpoAll o;
  lpo_all(s, t, cfg, &o);
  st->pairs++;
  u8 bad = 0u;
  if (o.top != ref) {
    st->top++;
    bad = 1u;
  }
  if (!o.encoded || o.flat != ref) {
    st->flat++;
    bad = 1u;
  }
  if (o.have_dag) {
    st->dag_pairs++;
    if (o.dag != ref) {
      st->dag++;
      bad = 1u;
    }
  }
  if (o.nodes > LPO_DAG_MIN_NODES) {
    st->big_pairs++;
  }
  if (o.tree != ref) {
    st->tree++;
    bad = 1u;
  }
  if (o.rpo != ref) {
    st->rpo++;
    bad = 1u;
  }
  if (bad && st->printed < 5u) {
    st->printed++;
    fprintf(stderr, "  ref=%d top=%d flat=%d dag=%d tree=%d rpo=%d  ",
            (int)ref, (int)o.top, (int)o.flat, o.have_dag ? (int)o.dag : 9,
            (int)o.tree, (int)o.rpo);
    term_print(s, stderr);
    fputs("  vs  ", stderr);
    term_print(t, stderr);
    fputc('\n', stderr);
  }
  return ref;
}

// Rebuild t with fresh cells, substituting sigma[v - vbase] for the
// variables vbase..vbase+nvar-1 when sigma is non-NULL.
static Term term_rebuild(Term t, const Term *sigma, u32 vbase, u32 nvar) {
  if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (sigma != NULL && v >= vbase && v < vbase + nvar) {
      return sigma[v - vbase];
    }
    return t;
  }
  u32  n = term_ctr_n(t);
  Term cs[3];
  for (u32 i = 0; i < n; i++) {
    cs[i] = term_rebuild(term_ctr_at(t, i), sigma, vbase, nvar);
  }
  return term_new_ctr(term_ext(t), cs, n);
}

// Proper subterms of t, preorder, at most cap.
static u32 term_proper_subterms(Term t, Term *out, u32 cap, u32 k) {
  if (term_tag(t) != TAG_CTR) {
    return k;
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n && k < cap; i++) {
    Term c = term_ctr_at(t, i);
    out[k++] = c;
    k = term_proper_subterms(c, out, cap, k);
  }
  return k;
}

// A one-hole context: a unary, binary or ternary symbol with the hole
// at a random argument and random terms elsewhere.
typedef struct {
  u32  lab;
  u32  ar;
  u32  hole;
  Term cs[3];
} Ctx;

static void ctx_new(const RndSpec *sp, Ctx *c) {
  c->lab  = 4u + rnd(5);
  c->ar   = RND_ARITY[c->lab];
  c->hole = rnd(c->ar);
  for (u32 i = 0; i < c->ar; i++) {
    c->cs[i] = rnd_term(sp, 2u);
  }
}

static Term ctx_fill(const Ctx *c, Term x) {
  Term cs[3];
  for (u32 i = 0; i < c->ar; i++) {
    cs[i] = (i == c->hole) ? x : c->cs[i];
  }
  return term_new_ctr(c->lab, cs, c->ar);
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

  TEST_BEGIN("lpo/permuted-args-incomparable");
  {
    // f(x, y) vs f(y, x): the first arguments are incomparable, so
    // neither lexicographic direction holds, and no argument of one side
    // reaches the other: UN both ways on every evaluator (Waldmeister
    // LPO.c:300-306).  Wrapped in g-towers the pair exceeds
    // LPO_DAG_MIN_NODES and thvm_lpo takes the DAG.
    Term x = mk_v(VAR_x);
    Term y = mk_v(VAR_y);
    Term gx = x;
    Term gy = y;
    for (u32 k = 0; k < 20u; k++) {
      gx = mk_g(gx);
      gy = mk_g(gy);
    }
    Term pairs[2][2] = {
      { mk_f2(x, y),   mk_f2(y, x)   },
      { mk_f2(gx, gy), mk_f2(gy, gx) },
    };
    for (u32 k = 0; k < 2u; k++) {
      for (u32 d = 0; d < 2u; d++) {
        Term   s = pairs[k][d];
        Term   t = pairs[k][1u - d];
        LpoAll o;
        lpo_all(s, t, &DEMO_LPO, &o);
        CHECK_EQ((int)o.top, (int)LPO_UN);
        CHECK_EQ((int)o.flat, (int)LPO_UN);
        CHECK(o.have_dag);
        CHECK_EQ((int)o.dag, (int)LPO_UN);
        CHECK_EQ((int)o.tree, (int)LPO_UN);
        CHECK_EQ((int)o.rpo, (int)LPO_UN);
        CHECK_EQ((int)ref_cmp(s, t, &DEMO_LPO), (int)LPO_UN);
#ifndef THVM_LPO_SELFCHECK
        CHECK_EQ((u32)(o.nodes > LPO_DAG_MIN_NODES), k);
#endif
      }
    }
  }

  TEST_BEGIN("lpo/lex-incomparable-is-not-stable-as-lt");
  {
    // f(x, c) vs f(c, x): incomparable.  Ordering it LT (f(c, x) above
    // both x and c) breaks stability: x := h(a) gives f(h(a), c) > f(c,
    // h(a)) by lex, as h > c.
    Term s = mk_f2(mk_v(VAR_x), mk_c());
    Term t = mk_f2(mk_c(), mk_v(VAR_x));
    CHECK_EQ((int)thvm_lpo(s, t, &DEMO_LPO), (int)LPO_UN);
    CHECK_EQ((int)thvm_lpo(t, s, &DEMO_LPO), (int)LPO_UN);
    Term ha = mk_h(mk_a());
    Term s1 = mk_f2(ha, mk_c());
    Term t1 = mk_f2(mk_c(), ha);
    CHECK_EQ((int)thvm_lpo(s1, t1, &DEMO_LPO), (int)LPO_GT);
    CHECK_EQ((int)ref_cmp(s1, t1, &DEMO_LPO), (int)LPO_GT);
  }

  TEST_BEGIN("lpo/tied-heads-incomparable");
  {
    // f and g tied at 4; b, c and h tied at 2: distinct symbols of equal
    // precedence order nothing by themselves, and a term whose greatest
    // symbol only ties with a constant is not above it (the Vortest).
    static u32 prec[7] = {0, 1, 2, 2, 4, 4, 2};
    const LpoConfig cfg = { .precedence = prec, .n_labels = 7 };
    Term x = mk_v(VAR_x);
    Term cases[9][2] = {
      { mk_f1(x),         mk_g(x)       },   // tied unary heads
      { mk_b(),           mk_c()        },   // tied constants
      { mk_f1(mk_b()),    mk_c()        },   // f above c
      { mk_f1(mk_b()),    mk_g(mk_b())  },
      { mk_f1(mk_c()),    mk_g(mk_b())  },
      { mk_h(mk_b()),     mk_c()        },   // h ties with c
      { mk_h(mk_c()),     mk_c()        },   // c is a subterm
      { mk_g(mk_h(mk_a())), mk_h(mk_c()) },  // g above h and c
      { mk_h(mk_a()),     mk_b()        },
    };
    static const LpoCmp want[9] = {
      LPO_UN, LPO_UN, LPO_GT, LPO_UN, LPO_UN, LPO_UN, LPO_GT, LPO_GT, LPO_UN,
    };
    for (u32 k = 0; k < 9u; k++) {
      for (u32 d = 0; d < 2u; d++) {
        Term   s = cases[k][d];
        Term   t = cases[k][1u - d];
        LpoCmp w = (d == 0u) ? want[k] : lpo_inv(want[k]);
        LpoAll o;
        lpo_all(s, t, &cfg, &o);
        CHECK_EQ((int)ref_cmp(s, t, &cfg), (int)w);
        CHECK_EQ((int)o.top, (int)w);
        CHECK_EQ((int)o.flat, (int)w);
        CHECK(o.have_dag);
        CHECK_EQ((int)o.dag, (int)w);
        CHECK_EQ((int)o.tree, (int)w);
        CHECK_EQ((int)o.rpo, (int)w);
      }
    }
  }

  TEST_BEGIN("lpo/dag-agrees-with-flat-recursion");
  {
    // Injective precedence, a tied one (distinct heads of equal
    // precedence, incomparable), and a short table (labels past n_labels
    // read precedence 0); variable ids below 64, straddling it, and above
    // it (the scan path).
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

  TEST_BEGIN("lpo/textbook-reference-properties");
  {
    // Random terms over constants (labels 1-3), unary (4, 5), binary
    // (6, 7) and ternary (8) symbols and four variables, under fixed and
    // random precedences, strict and tied, and a short table (labels past
    // n_labels read 0); variable ids below 64, straddling it (the
    // top-level variable-set pretest's bitset) and above it.
    //   differential: every evaluator equals the textbook reference;
    //   antisymmetry: compare(s, t) is the inverse of compare(t, s);
    //   irreflexivity: EQ exactly on structurally equal terms;
    //   transitivity: every GT chain s > t > u in a pool gives s > u;
    //   stability: s > t gives s.sigma > t.sigma;
    //   monotonicity: s > t gives C[s] > C[t];
    //   subterm property: s > u for every proper subterm u of s.
    static u32 fixed[6][9] = {
      {0, 3, 1, 2, 6, 4, 8, 5, 7},   // strict
      {0, 8, 7, 6, 5, 4, 3, 2, 1},   // strict, constants on top
      {0, 1, 2, 1, 2, 3, 3, 2, 3},   // ties across arities
      {0, 1, 1, 1, 1, 1, 1, 1, 1},   // one class
      {0, 2, 2, 5, 5, 3, 5, 3, 1},
      {0, 2, 1, 3, 5, 4, 0, 0, 0},   // as a 6-label table, 6-8 read 0
    };
    static u32 randp[24][9];
    const u32  n_cfg = 6u + 24u;
    static const u32 vbases[3] = {0u, 62u, 100u};
    DiffStats st;
    memset(&st, 0, sizeof st);
    u64 anti_bad    = 0u;
    u64 eq_bad      = 0u;
    u64 trans_chain = 0u;
    u64 trans_bad   = 0u;
    u64 stab_n      = 0u;
    u64 stab_bad    = 0u;
    u64 mono_n      = 0u;
    u64 mono_bad    = 0u;
    u64 sub_n       = 0u;
    u64 sub_bad     = 0u;
    u64 cnt[4]      = {0u, 0u, 0u, 0u};   // reference verdicts EQ GT LT UN
    for (u32 r = 0; r < 24u; r++) {
      randp[r][0] = 0u;
      if (r % 2u == 0u) {
        for (u32 l = 1; l < 9u; l++) {
          randp[r][l] = l;
        }
        for (u32 l = 8u; l > 1u; l--) {
          u32 j = 1u + rnd(l);
          u32 tmp = randp[r][l];
          randp[r][l] = randp[r][j];
          randp[r][j] = tmp;
        }
      } else {
        for (u32 l = 1; l < 9u; l++) {
          randp[r][l] = rnd(4);
        }
      }
    }
    for (u32 ci = 0; ci < n_cfg; ci++) {
      LpoConfig cfg;
      cfg.precedence = (ci < 6u) ? fixed[ci] : randp[ci - 6u];
      cfg.n_labels   = (ci == 5u) ? 6u : 9u;
      for (u32 vi = 0; vi < 3u; vi++) {
        Term    pool[24];
        RndSpec sp = { .nvar = 4u, .vbase = vbases[vi], .pool = pool, .npool = 0u };
        for (u32 k = 0; k < 24u; k++) {
          pool[k] = rnd_term(&sp, 3u);
        }
        sp.npool = 24u;
        for (u32 it = 0; it < 300u; it++) {
          Term s = rnd_term(&sp, 5u);
          Term t;
          switch (rnd(6)) {
            case 0:  t = rnd_term(&sp, 5u);    break;
            case 1:  t = rnd_edit(&sp, s, 0u); break;
            case 2:  t = rnd_edit(&sp, s, 1u); break;
            case 3:  t = rnd_edit(&sp, rnd_edit(&sp, s, 1u), 0u); break;
            case 4:  t = term_rebuild(s, NULL, 0u, 0u); break;
            default: t = sp.pool[rnd(24)];     break;
          }
          LpoCmp ref = diff_pair(s, t, &cfg, &st);
          LpoCmp rev = diff_pair(t, s, &cfg, &st);
          cnt[(ref == LPO_EQ) ? 0 : (ref == LPO_GT) ? 1 : (ref == LPO_LT) ? 2 : 3]++;
          LpoCmp top = thvm_lpo(s, t, &cfg);
          LpoCmp pot = thvm_lpo(t, s, &cfg);
          if (pot != lpo_inv(top) || rev != lpo_inv(ref)) {
            anti_bad++;
          }
          if ((top == LPO_EQ) != ref_same(s, t) || thvm_lpo(s, s, &cfg) != LPO_EQ) {
            eq_bad++;
          }
          // Stability and monotonicity on an ordered pair.
          if (ref == LPO_GT || ref == LPO_LT) {
            Term hi = (ref == LPO_GT) ? s : t;
            Term lo = (ref == LPO_GT) ? t : s;
            Term sigma[4];
            for (u32 v = 0; v < 4u; v++) {
              sigma[v] = rnd_term(&sp, 2u);
            }
            Term his = term_rebuild(hi, sigma, sp.vbase, 4u);
            Term los = term_rebuild(lo, sigma, sp.vbase, 4u);
            stab_n++;
            if (diff_pair(his, los, &cfg, &st) != LPO_GT ||
                thvm_lpo(his, los, &cfg) != LPO_GT) {
              stab_bad++;
            }
            Ctx c;
            ctx_new(&sp, &c);
            mono_n++;
            if (thvm_lpo(ctx_fill(&c, hi), ctx_fill(&c, lo), &cfg) != LPO_GT) {
              mono_bad++;
            }
          }
          // Subterm property.
          Term subs[48];
          u32  ns = term_proper_subterms(s, subs, 48u, 0u);
          for (u32 k = 0; k < ns; k++) {
            sub_n++;
            if (thvm_lpo(s, subs[k], &cfg) != LPO_GT ||
                thvm_lpo(subs[k], s, &cfg) != LPO_LT) {
              sub_bad++;
            }
          }
        }
        // Transitivity over a pool of related terms: random terms, their
        // subterms, edits, argument swaps, instances and contexts.
        Term tp[40];
        u32  nt = 0u;
        while (nt < 8u) {
          tp[nt++] = rnd_term(&sp, 4u);
        }
        while (nt < 40u) {
          Term base = tp[rnd(nt)];
          switch (rnd(5)) {
            case 0: {
              Term subs[16];
              u32  ns = term_proper_subterms(base, subs, 16u, 0u);
              tp[nt++] = (ns > 0u) ? subs[rnd(ns)] : rnd_term(&sp, 3u);
              break;
            }
            case 1:  tp[nt++] = rnd_edit(&sp, base, 0u); break;
            case 2:  tp[nt++] = rnd_edit(&sp, base, 1u); break;
            case 3: {
              Term sigma[4];
              for (u32 v = 0; v < 4u; v++) {
                sigma[v] = (rnd(2) == 0u) ? mk_v(sp.vbase + rnd(4)) : rnd_term(&sp, 1u);
              }
              tp[nt++] = term_rebuild(base, sigma, sp.vbase, 4u);
              break;
            }
            default: {
              Ctx c;
              ctx_new(&sp, &c);
              tp[nt++] = ctx_fill(&c, base);
              break;
            }
          }
        }
        static LpoCmp mat[40][40];
        for (u32 i = 0; i < nt; i++) {
          for (u32 j = 0; j < nt; j++) {
            mat[i][j] = thvm_lpo(tp[i], tp[j], &cfg);
          }
        }
        for (u32 i = 0; i < nt; i++) {
          for (u32 j = 0; j < nt; j++) {
            if (mat[i][j] != LPO_GT) {
              continue;
            }
            for (u32 k = 0; k < nt; k++) {
              if (mat[j][k] == LPO_GT) {
                trans_chain++;
                if (mat[i][k] != LPO_GT) {
                  trans_bad++;
                }
              }
            }
          }
        }
      }
    }
    printf("  reference: pairs=%llu (EQ %llu GT %llu LT %llu UN %llu) dag=%llu "
           "top-dag=%llu mismatches top=%llu flat=%llu dag=%llu tree=%llu rpo=%llu\n",
           (unsigned long long)st.pairs, (unsigned long long)cnt[0],
           (unsigned long long)cnt[1], (unsigned long long)cnt[2],
           (unsigned long long)cnt[3], (unsigned long long)st.dag_pairs,
           (unsigned long long)st.big_pairs, (unsigned long long)st.top,
           (unsigned long long)st.flat, (unsigned long long)st.dag,
           (unsigned long long)st.tree, (unsigned long long)st.rpo);
    printf("  properties: antisym-bad=%llu eq-bad=%llu transitive chains=%llu bad=%llu "
           "stability=%llu bad=%llu monotonic=%llu bad=%llu subterm=%llu bad=%llu\n",
           (unsigned long long)anti_bad, (unsigned long long)eq_bad,
           (unsigned long long)trans_chain, (unsigned long long)trans_bad,
           (unsigned long long)stab_n, (unsigned long long)stab_bad,
           (unsigned long long)mono_n, (unsigned long long)mono_bad,
           (unsigned long long)sub_n, (unsigned long long)sub_bad);
    CHECK_EQ(st.top, 0u);
    CHECK_EQ(st.flat, 0u);
    CHECK_EQ(st.dag, 0u);
    CHECK_EQ(st.tree, 0u);
    CHECK_EQ(st.rpo, 0u);
    CHECK_EQ(g_ref_both, 0u);
    CHECK_EQ(anti_bad, 0u);
    CHECK_EQ(eq_bad, 0u);
    CHECK_EQ(trans_bad, 0u);
    CHECK_EQ(stab_bad, 0u);
    CHECK_EQ(mono_bad, 0u);
    CHECK_EQ(sub_bad, 0u);
    // Coverage: every verdict occurs, the DAG ran on every encoded pair,
    // thvm_lpo took both paths, and the properties were not vacuous.
    CHECK(cnt[0] > 100u && cnt[1] > 1000u && cnt[2] > 1000u && cnt[3] > 1000u);
    CHECK_EQ(st.dag_pairs, st.pairs);
#ifndef THVM_LPO_SELFCHECK
    CHECK(st.big_pairs > 1000u && st.pairs - st.big_pairs > 1000u);
#endif
    CHECK(trans_chain > 1000u);
    CHECK(stab_n > 1000u && mono_n > 1000u && sub_n > 10000u);
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

  TEST_BEGIN("lpo/quiet-leaves-memo-and-stats");
  {
    // An observer's verdict: thvm_lpo's on every pair, with the
    // persistent memo, its epoch and the statistics as they were.
    static LpoMemoEnt memo0[LPO_MEMO_SIZE];
    static Term       qs[400], qt[400];
    static LpoCmp     qv[400];
    thvm_lpo_set_persist(1u);
    thvm_lpo_invalidate();
    Term    pool[24];
    RndSpec sp = { .nvar = 4u, .vbase = 0u, .pool = pool, .npool = 0u };
    for (u32 k = 0; k < 24u; k++) {
      pool[k] = rnd_term(&sp, 3u);
    }
    sp.npool = 24u;
    for (u32 it = 0; it < 200u; it++) {
      Term s = rnd_term(&sp, 5u);
      Term t = rnd_term(&sp, 5u);
      (void)thvm_lpo(s, t, &DEMO_LPO);
    }
    for (u32 it = 0; it < 400u; it++) {
      qs[it] = rnd_term(&sp, 5u);
      qt[it] = (it % 2u == 0u) ? rnd_term(&sp, 5u) : rnd_edit(&sp, qs[it], (it % 4u == 1u) ? 0u : 1u);
    }
    memcpy(memo0, g_lpo_memo, sizeof memo0);
    u32 epoch0 = g_lpo_epoch;
    u64 a0 = 0, b0 = 0, c0 = 0, d0 = 0;
    thvm_lpo_flat_stats(&a0, &b0, &c0, &d0);
    u32 quiet = 0u;
    for (u32 it = 0; it < 400u; it++) {
      quiet += (u32)thvm_lpo_quiet(qs[it], qt[it], &DEMO_LPO, &qv[it]);
    }
    u64 a1 = 0, b1 = 0, c1 = 0, d1 = 0;
    thvm_lpo_flat_stats(&a1, &b1, &c1, &d1);
    CHECK_EQ(quiet, 400u);
    CHECK(a0 == a1 && b0 == b1 && c0 == c1 && d0 == d1);
    CHECK_EQ(g_lpo_epoch, epoch0);
    CHECK(memcmp(memo0, g_lpo_memo, sizeof memo0) == 0);
    for (u32 it = 0; it < 400u; it++) {
      CHECK_EQ((int)qv[it], (int)thvm_lpo(qs[it], qt[it], &DEMO_LPO));
    }
    thvm_lpo_set_persist(0u);
  }

  thvm_free();
  TEST_REPORT();
}
