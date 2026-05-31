// test_ft.c - Stage 2 of AtpFt: constructors, boundary converters,
// structural equality, hash, and pretty-print.
//
// Differential tests against the existing heap-Term path:
//   * ft_from_term + ft_to_term round-trips bit-identical via
//     atp_term_struct_hash agreement.
//   * ft_hash agrees with atp_term_struct_hash on 1000 random Terms.
//   * atp_pretty_ft byte-identical to atp_pretty_term on the same.
//   * ft_eq agrees with structural Term equality.
//   * Same term in scratch arena vs persistent arena: ft_eq + ft_hash
//     return identical results.

#include "../src/thvm.c"

#ifndef THVM_ATPFT_ALLOC
#define THVM_ATPFT_ALLOC 1
#endif
#ifndef THVM_ATPFT_CONVERT
#define THVM_ATPFT_CONVERT 1
#endif

#include "../src/atp/ft.h"
#include "../src/atp/ft_alloc.c"
#include "../src/atp/ft.c"

#include "test.h"

// --- Random term generator -------------------------------------------
//
// xorshift32 + a tiny recursive builder: at depth 0 emit a leaf (var
// or 0-arity constructor); at depth > 0 either descend into an
// arity-2 CTR or emit a leaf, biased toward more depth so the corpus
// has plenty of structure but stays bounded.  Var ids cycle modulo
// 8; CTR labels modulo 16 -- enough variety to exercise multiple
// hash buckets without overflowing the var-id field.

static u32 rng32(u32 *seed) {
  u32 x = *seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *seed = x ? x : 0xdeadbeefu;
  return *seed;
}

static Term mk_random_term(u32 *seed, u32 depth) {
  u32 r = rng32(seed);
  if (depth == 0u || (r & 3u) == 0u) {
    if (r & 0x10u) {
      return term_new_fvr((r >> 5) & 7u);          // var id 0..7
    }
    // 0-arity CTR constant.
    return term_new_ctr((r >> 5) & 15u, NULL, 0u);
  }
  // Arity-2 CTR with two recursive children.
  Term kids[2];
  kids[0] = mk_random_term(seed, depth - 1u);
  kids[1] = mk_random_term(seed, depth - 1u);
  return term_new_ctr((r >> 5) & 15u, kids, 2u);
}

// Structural equality on Term (small port matching ft_eq's semantics:
// same tag, same ext, same arity, same recursive children).  Sticks
// to TAG_CTR + TAG_FVR + 0-arity constants -- the shapes the random
// generator above produces.
static int term_struct_eq(Term a, Term b) {
  if (term_tag(a) != term_tag(b)) return 0;
  if (term_ext(a) != term_ext(b)) return 0;
  if (term_tag(a) == TAG_FVR) return 1;
  if (term_tag(a) == TAG_CTR) {
    u32 na = term_ctr_n(a);
    u32 nb = term_ctr_n(b);
    if (na != nb) return 0;
    for (u32 i = 0; i < na; i++) {
      if (!term_struct_eq(term_ctr_at(a, i), term_ctr_at(b, i))) return 0;
    }
    return 1;
  }
  // Other tags (NUM, etc.) match by tag+ext alone.
  return 1;
}

int main(void) {
  thvm_init();

  // ---- Test 1: ftnew_const + ftnew_var basics ----------------------
  TEST_BEGIN("ft/new-const-var-basics");
  {
    AtpFt a;
    ft_init(&a);
    AtpFtCell *cst = ftnew_const(&a, 42u, 0);
    CHECK_EQ(cst->sym, 42u);
    CHECK_EQ(cst->arity, 0u);
    CHECK((cst->flags & ATPFT_FLAG_GROUND) != 0u);
    CHECK(cst->end == cst);
    CHECK(cst->next == NULL);

    AtpFtCell *v = ftnew_var(&a, 7u, 0);
    CHECK_EQ(v->sym, WF_VAR_BIT | 7u);
    CHECK_EQ(v->arity, 0u);
    CHECK((v->flags & ATPFT_FLAG_GROUND) == 0u);   // var is NEVER ground
    CHECK(v->end == v);
    CHECK(v->next == NULL);
    CHECK(ft_is_var(v));
    CHECK_EQ(ft_var_id(v), 7u);
    CHECK(!ft_is_var(cst));
    ft_destroy(&a);
  }

  // ---- Test 2: ftnew_ctr stitches and OR-folds GROUND --------------
  TEST_BEGIN("ft/new-ctr-stitch-ground");
  {
    AtpFt a;
    ft_init(&a);
    // (sym=3 (sym=1) (sym=2)) -- both children ground, parent ground.
    AtpFtCell *c0 = ftnew_const(&a, 1u, 0);
    AtpFtCell *c1 = ftnew_const(&a, 2u, 0);
    AtpFtCell *kids[2] = {c0, c1};
    AtpFtCell *p = ftnew_ctr(&a, 3u, 2u, kids, 0);
    CHECK_EQ(p->sym, 3u);
    CHECK_EQ(p->arity, 2u);
    CHECK((p->flags & ATPFT_FLAG_GROUND) != 0u);
    CHECK(p->next == c0);
    CHECK(c0->end->next == c1);
    CHECK(p->end == c1);
    CHECK(p->end->next == NULL);

    // Same shape but with a var child -> parent NOT ground.
    AtpFtCell *vc = ftnew_var(&a, 0u, 0);
    AtpFtCell *cc = ftnew_const(&a, 5u, 0);
    AtpFtCell *kids2[2] = {vc, cc};
    AtpFtCell *p2 = ftnew_ctr(&a, 9u, 2u, kids2, 0);
    CHECK((p2->flags & ATPFT_FLAG_GROUND) == 0u);
    // Sibling-jump still consistent.
    CHECK(p2->next == vc);
    CHECK(vc->end->next == cc);
    CHECK(p2->end == cc);

    ft_destroy(&a);
  }

  // ---- Helpers for the 1000-term suites ----------------------------
  enum { N_RAND = 1000, DEPTH = 5 };
  u32 seed = 0xa5a5a5a5u;

  // Materialize 1000 random Terms first so the same corpus is shared
  // by T3..T6 (otherwise the rng diverges between tests).
  Term *src = (Term *)malloc(sizeof(Term) * N_RAND);
  for (u32 i = 0; i < N_RAND; i++) src[i] = mk_random_term(&seed, DEPTH);

  // ---- Test 3: ft_from_term + ft_to_term round-trip ----------------
  TEST_BEGIN("ft/round-trip-from-to-term");
  {
    AtpFt a;
    ft_init(&a);
    u32 pass = 0;
    for (u32 i = 0; i < N_RAND; i++) {
      AtpFtCell *x = ft_from_term(&a, src[i], 0);
      Term       y = ft_to_term(x);
      if (term_struct_eq(src[i], y)) pass += 1u;
    }
    CHECK_EQ(pass, (u32)N_RAND);
    ft_destroy(&a);
  }

  // ---- Test 4: ft_hash matches atp_term_struct_hash ----------------
  TEST_BEGIN("ft/hash-matches-term-hash");
  {
    AtpFt a;
    ft_init(&a);
    u32 pass = 0;
    for (u32 i = 0; i < N_RAND; i++) {
      AtpFtCell *x = ft_from_term(&a, src[i], 0);
      u64 h_ft   = ft_hash(x);
      u64 h_term = atp_term_struct_hash(src[i]);
      if (h_ft == h_term) pass += 1u;
    }
    CHECK_EQ(pass, (u32)N_RAND);
    ft_destroy(&a);
  }

  // ---- Test 5: ft_eq reflexive + symmetric + agrees with Term-eq ---
  TEST_BEGIN("ft/eq-reflexive-symmetric-agrees");
  {
    AtpFt a;
    ft_init(&a);
    // Convert ALL 1000 first so x[i] and y[i] live in the same arena.
    AtpFtCell **x = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * N_RAND);
    AtpFtCell **y = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * N_RAND);
    for (u32 i = 0; i < N_RAND; i++) {
      x[i] = ft_from_term(&a, src[i], 0);
      y[i] = ft_from_term(&a, src[i], 0);   // independent copy
    }
    u32 refl = 0, sym_ok = 0, agree = 0;
    for (u32 i = 0; i < N_RAND; i++) {
      if (ft_eq(x[i], x[i])) refl += 1u;
      int e_xy = ft_eq(x[i], y[i]);
      int e_yx = ft_eq(y[i], x[i]);
      if (e_xy == e_yx) sym_ok += 1u;
      int t_eq = term_struct_eq(src[i], src[i]);    // always 1
      if (e_xy == t_eq) agree += 1u;
    }
    CHECK_EQ(refl,    (u32)N_RAND);
    CHECK_EQ(sym_ok,  (u32)N_RAND);
    CHECK_EQ(agree,   (u32)N_RAND);
    // Negative case: distinct shapes are NOT equal.
    AtpFtCell *p = ftnew_var(&a, 0u, 0);
    AtpFtCell *q = ftnew_var(&a, 1u, 0);
    CHECK(!ft_eq(p, q));
    AtpFtCell *r = ftnew_const(&a, 0u, 0);          // var0 vs const0
    CHECK(!ft_eq(p, r));
    free(x);
    free(y);
    ft_destroy(&a);
  }

  // ---- Test 6: atp_pretty_ft byte-identical to atp_pretty_term ----
  TEST_BEGIN("ft/pretty-ft-matches-term");
  {
    AtpFt a;
    ft_init(&a);
    enum { BUFSZ = 4096 };
    char buf_t[BUFSZ];
    char buf_f[BUFSZ];
    u32 pass = 0;
    for (u32 i = 0; i < N_RAND; i++) {
      AtpFtCell *x = ft_from_term(&a, src[i], 0);
      u32 nt = atp_pretty_term(src[i], buf_t, BUFSZ);
      u32 nf = atp_pretty_ft  (x,      buf_f, BUFSZ);
      if (nt == nf && memcmp(buf_t, buf_f, nt) == 0) pass += 1u;
    }
    CHECK_EQ(pass, (u32)N_RAND);
    ft_destroy(&a);
  }

  // ---- Test 7: persistent vs scratch arena hand off the same term --
  TEST_BEGIN("ft/persistent-vs-scratch-agree");
  {
    AtpFt a;
    ft_init(&a);
    u32 hash_ok = 0, eq_ok = 0;
    for (u32 i = 0; i < N_RAND; i++) {
      AtpFtCell *xp = ft_from_term(&a, src[i], 0);  // persistent
      AtpFtCell *xs = ft_from_term(&a, src[i], 1);  // scratch
      if (ft_hash(xp) == ft_hash(xs)) hash_ok += 1u;
      if (ft_eq(xp, xs))              eq_ok   += 1u;
    }
    CHECK_EQ(hash_ok, (u32)N_RAND);
    CHECK_EQ(eq_ok,   (u32)N_RAND);
    ft_destroy(&a);
  }

  free(src);
  thvm_free();
  TEST_REPORT();
}
