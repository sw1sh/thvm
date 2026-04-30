// test_rewrite_pri.c -- stage 8.3b: prim_rewrite_step.
//
// Verifies that `prim_rewrite_step` registered at
// ATP_PRIM_REWRITE_STEP can be invoked via APP-PRI evaluation,
// returning either the rewritten term (on match) or ERA (on
// failure), matching the C-side `thvm_match` + `thvm_subst_apply`
// behavior for the same inputs.

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

// Helper: invoke prim_rewrite_step via 3-step APP chain and
// reduce to wnf.
static Term ic_rewrite_step(Term lhs, Term rhs, Term target) {
  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_REWRITE_STEP));
  heap_set(l1 + 1, lhs);
  Term step1 = term_new(0, TAG_APP, 0, l1);

  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, rhs);
  Term step2 = term_new(0, TAG_APP, 0, l2);

  u64 l3 = heap_alloc(2);
  heap_set(l3 + 0, step2);
  heap_set(l3 + 1, target);
  Term step3 = term_new(0, TAG_APP, 0, l3);

  return wnf(step3);
}

int main(void) {
  thvm_init();

  // Bootstrap to register the primitive (any KboConfig works).
  static const KboConfig DUMMY_CFG = {
    .weights = NULL, .precedence = NULL,
    .n_labels = 0, .var_weight = 1,
  };
  AtpState *bootstrap = thvm_atp_init(&DUMMY_CFG, 0);
  CHECK(bootstrap != NULL);

  TEST_BEGIN("rewrite-pri/direct-match");
  {
    // Rule: f(x, e) -> x.  Target: f(a, e).  Expected: a.
    Term lhs    = mk_f(mk_v(VAR_x), mk_e());
    Term rhs    = mk_v(VAR_x);
    Term target = mk_f(mk_a(), mk_e());

    Term out = ic_rewrite_step(lhs, rhs, target);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), LAB_a);
    CHECK_EQ(term_ctr_n(out), 0u);
  }

  TEST_BEGIN("rewrite-pri/no-match-different-head");
  {
    // Rule: f(x, e) -> x.  Target: g(a).  Top symbol mismatch.
    Term lhs    = mk_f(mk_v(VAR_x), mk_e());
    Term rhs    = mk_v(VAR_x);
    Term target = mk_g(mk_a());

    Term out = ic_rewrite_step(lhs, rhs, target);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  TEST_BEGIN("rewrite-pri/no-match-second-arg-mismatch");
  {
    // Rule: f(x, e) -> x.  Target: f(a, b).  Second arg fails
    // to match `e`.
    Term lhs    = mk_f(mk_v(VAR_x), mk_e());
    Term rhs    = mk_v(VAR_x);
    Term target = mk_f(mk_a(), mk_b());

    Term out = ic_rewrite_step(lhs, rhs, target);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  TEST_BEGIN("rewrite-pri/fvr-only-lhs-binds-anything");
  {
    // Rule: x -> a.  Target: f(b, e).  LHS is just x; matches
    // anything; binds x to f(b, e); rhs is `a` (no var) so
    // result is `a` unchanged.
    Term lhs    = mk_v(VAR_x);
    Term rhs    = mk_a();
    Term target = mk_f(mk_b(), mk_e());

    Term out = ic_rewrite_step(lhs, rhs, target);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), LAB_a);
  }

  TEST_BEGIN("rewrite-pri/nested-ctr-binds-multiple-vars");
  {
    // Rule: f(g(x), y) -> g(y).  Target: f(g(a), b).  Binds
    // x -> a, y -> b; rhs g(y) substitutes to g(b).
    Term lhs    = mk_f(mk_g(mk_v(VAR_x)), mk_v(VAR_y));
    Term rhs    = mk_g(mk_v(VAR_y));
    Term target = mk_f(mk_g(mk_a()), mk_b());

    Term out = ic_rewrite_step(lhs, rhs, target);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), LAB_g);
    CHECK_EQ(term_ctr_n(out), 1u);
    Term child = term_ctr_at(out, 0);
    CHECK_EQ(term_tag(child), TAG_CTR);
    CHECK_EQ(term_ext(child), LAB_b);
  }

  TEST_BEGIN("rewrite-pri/repeated-var-must-match-consistently");
  {
    // Rule: f(x, x) -> x.  Target: f(a, a) matches with σ = {x->a}.
    Term lhs    = mk_f(mk_v(VAR_x), mk_v(VAR_x));
    Term rhs    = mk_v(VAR_x);

    // Positive case.
    Term target_ok = mk_f(mk_a(), mk_a());
    Term out_ok = ic_rewrite_step(lhs, rhs, target_ok);
    CHECK_EQ(term_tag(out_ok), TAG_CTR);
    CHECK_EQ(term_ext(out_ok), LAB_a);

    // Negative case: f(a, b) -- first occurrence binds x=a,
    // second occurrence sees b which is != a, match fails.
    Term target_no = mk_f(mk_a(), mk_b());
    Term out_no = ic_rewrite_step(lhs, rhs, target_no);
    CHECK_EQ(term_tag(out_no), TAG_ERA);
  }

  thvm_atp_free(bootstrap);
  thvm_free();
  TEST_REPORT();
}
