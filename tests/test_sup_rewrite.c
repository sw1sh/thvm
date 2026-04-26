// test_sup_rewrite.c -- stage 8.3c: SUP of rules + APP-SUP fan-out demo.
//
// Demonstrates that a SUP of `prim_rewrite_step` calls reduces to
// the same rewrite outcomes as direct C-side `thvm_match` +
// `thvm_subst_apply` calls would produce per rule.
//
// Caveat carries over from 8.1d-ii: APP-SUP shares its argument
// via a DUP, but our IC has no DUP-CTR yet, so passing CTR-shaped
// rule LHSs / target terms via APP-SUP fan-out gets stuck at
// DP0/DP1.  Like 8.1d-ii, the rule-shaped tests use the
// "fully-applied PRI inside the SUP" pattern -- each child of the
// SUP is `APP(APP(APP(PRI(REWRITE), lhs_i), rhs_i), target)`,
// fully saturated and ready to fire when wnf'd.  An additional
// smaller test exercises APP-SUP itself with DUP-friendly NUM
// args to confirm the fan-out machinery is operational.

#include "../src/thvm.c"
#include "test.h"

#define LAB_e 1u
#define LAB_g 2u
#define LAB_f 3u
#define LAB_a 4u
#define LAB_b 5u
#define VAR_x 0u
#define VAR_y 1u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_b(void) { return term_new_ctr(LAB_b, NULL, 0); }
static Term mk_g(Term x)         { Term cs[1] = {x}; return term_new_ctr(LAB_g, cs, 1); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_v(u32 id) { return term_new_fvr(id); }

static Term mk_app(Term fun, Term arg) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, fun);
  heap_set(loc + 1, arg);
  return term_new(0, TAG_APP, 0, loc);
}

static Term mk_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

// Build a fully-saturated APP(APP(APP(PRI(REWRITE), lhs), rhs),
// target).  All three args are pre-supplied; wnf'ing this fires
// prim_rewrite_step immediately.
static Term mk_rewrite_call(Term lhs, Term rhs, Term target) {
  Term step1 = mk_app(term_new_pri(ATP_PRIM_REWRITE_STEP), lhs);
  Term step2 = mk_app(step1, rhs);
  return mk_app(step2, target);
}

// Reference: direct C-side rewrite outcome -- matches what
// prim_rewrite_step computes.
static Term ref_rewrite(Term lhs, Term rhs, Term target) {
  RewriteSubst subst = {{0}};
  if (!thvm_match(lhs, target, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_subst_apply(rhs, &subst);
}

static u8 term_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      if (ns != term_ctr_n(t)) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!term_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// Identity primitive used by the APP-SUP NUM demonstration.
static Term id_prim(Term *args) { return args[0]; }

int main(void) {
  thvm_init();

  // Bootstrap to register prim_rewrite_step.
  static const KboConfig DUMMY_CFG = {
    .weights = NULL, .precedence = NULL,
    .n_labels = 0, .var_weight = 1,
  };
  AtpState *bootstrap = thvm_atp_init(&DUMMY_CFG, 0);
  CHECK(bootstrap != NULL);

  TEST_BEGIN("sup-rewrite/two-rules-one-applies");
  {
    // Rule 0: f(x, e) -> x.  Rule 1: g(x) -> a.
    // Target: f(b, e) -- only rule 0 matches.
    Term r0_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term r0_rhs = mk_v(VAR_x);
    Term r1_lhs = mk_g(mk_v(VAR_x));
    Term r1_rhs = mk_a();
    Term target = mk_f(mk_b(), mk_e());

    Term ref_0 = ref_rewrite(r0_lhs, r0_rhs, target);
    Term ref_1 = ref_rewrite(r1_lhs, r1_rhs, target);
    CHECK_EQ(term_tag(ref_0), TAG_CTR);
    CHECK_EQ(term_ext(ref_0), LAB_b);
    CHECK_EQ(term_tag(ref_1), TAG_ERA);

    Term sup = mk_sup(7u,
                     mk_rewrite_call(r0_lhs, r0_rhs, target),
                     mk_rewrite_call(r1_lhs, r1_rhs, target));
    Term outcome = wnf(sup);
    CHECK_EQ(term_tag(outcome), TAG_SUP);
    CHECK_EQ(term_ext(outcome), 7u);
    Term out_0 = wnf(heap_read(term_val(outcome) + 0));
    Term out_1 = wnf(heap_read(term_val(outcome) + 1));
    CHECK(term_eq(out_0, ref_0));
    CHECK(term_eq(out_1, ref_1));
  }

  TEST_BEGIN("sup-rewrite/two-rules-both-apply");
  {
    // Rule 0: x -> a.  Rule 1: x -> b.
    // Target: f(c, e) -- both bare-FVR rules match anything.
    Term r0_lhs = mk_v(VAR_x);
    Term r0_rhs = mk_a();
    Term r1_lhs = mk_v(VAR_x);
    Term r1_rhs = mk_b();
    Term target = mk_f(mk_g(mk_a()), mk_e());

    Term ref_0 = ref_rewrite(r0_lhs, r0_rhs, target);
    Term ref_1 = ref_rewrite(r1_lhs, r1_rhs, target);
    CHECK_EQ(term_ext(ref_0), LAB_a);
    CHECK_EQ(term_ext(ref_1), LAB_b);

    Term sup = mk_sup(7u,
                     mk_rewrite_call(r0_lhs, r0_rhs, target),
                     mk_rewrite_call(r1_lhs, r1_rhs, target));
    (void)wnf(sup);
    Term out_0 = wnf(heap_read(term_val(sup) + 0));
    Term out_1 = wnf(heap_read(term_val(sup) + 1));
    CHECK(term_eq(out_0, ref_0));
    CHECK(term_eq(out_1, ref_1));
  }

  TEST_BEGIN("sup-rewrite/three-rules-mixed");
  {
    // Three rules; nested SUP since SUPs are binary:
    //   r0: f(x, e) -> x       (matches f(a, e) -> a)
    //   r1: g(x) -> a          (no match against f(...))
    //   r2: f(x, y) -> g(y)    (matches -> g(e))
    Term r0_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term r0_rhs = mk_v(VAR_x);
    Term r1_lhs = mk_g(mk_v(VAR_x));
    Term r1_rhs = mk_a();
    Term r2_lhs = mk_f(mk_v(VAR_x), mk_v(VAR_y));
    Term r2_rhs = mk_g(mk_v(VAR_y));
    Term target = mk_f(mk_a(), mk_e());

    Term ref_0 = ref_rewrite(r0_lhs, r0_rhs, target);
    Term ref_1 = ref_rewrite(r1_lhs, r1_rhs, target);
    Term ref_2 = ref_rewrite(r2_lhs, r2_rhs, target);

    Term inner = mk_sup(8u,
                       mk_rewrite_call(r1_lhs, r1_rhs, target),
                       mk_rewrite_call(r2_lhs, r2_rhs, target));
    Term outer = mk_sup(7u,
                       mk_rewrite_call(r0_lhs, r0_rhs, target),
                       inner);
    (void)wnf(outer);
    Term out_0 = wnf(heap_read(term_val(outer) + 0));
    Term inner_term = wnf(heap_read(term_val(outer) + 1));
    CHECK_EQ(term_tag(inner_term), TAG_SUP);
    Term out_1 = wnf(heap_read(term_val(inner_term) + 0));
    Term out_2 = wnf(heap_read(term_val(inner_term) + 1));
    CHECK(term_eq(out_0, ref_0));
    CHECK_EQ(term_tag(out_1), TAG_ERA);
    CHECK_EQ(term_tag(ref_1), TAG_ERA);
    CHECK(term_eq(out_2, ref_2));
  }

  TEST_BEGIN("sup-rewrite/app-sup-fan-out-with-num-args");
  {
    // Confirms APP-SUP commutation operates with PRI children
    // when the arg is DUP-friendly (NUM).  Two identity-PRI
    // children apply to a single NUM; APP-SUP DUPs the arg and
    // each branch returns the NUM unchanged.
    prim_register(50u, id_prim, 1u);
    Term sup_partials = mk_sup(11u, term_new_pri(50u), term_new_pri(50u));
    Term arg = term_new(0, TAG_NUM, DT_I32, 33);
    Term app = mk_app(sup_partials, arg);
    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_SUP);
    CHECK_EQ(term_ext(out), 11u);
    Term out_l = wnf(heap_read(term_val(out) + 0));
    Term out_r = wnf(heap_read(term_val(out) + 1));
    CHECK_EQ(term_tag(out_l), TAG_NUM);
    CHECK_EQ((u32)term_val(out_l), 33u);
    CHECK_EQ(term_tag(out_r), TAG_NUM);
    CHECK_EQ((u32)term_val(out_r), 33u);
  }

  thvm_atp_free(bootstrap);
  thvm_free();
  TEST_REPORT();
}
