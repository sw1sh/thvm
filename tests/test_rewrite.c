// test_rewrite.c - one-shot equational rewriter on TAG_CTR + TAG_FVR.
//
// Stage 3 demo (docs/plans/waldmeister_ic_atp.md): normalize
// f(a, e) -> a under the group axiom f(x, e) = x.  Plus matching
// and substitution unit cases.

#include "../src/thvm.c"
#include "test.h"

// Group signature: same labels as test_kbo.c.
#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u
#define VAR_y 1u
#define VAR_z 2u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_i(Term x) { Term cs[1] = {x}; return term_new_ctr(LAB_i, cs, 1); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }

int main(void) {
  thvm_init();

  TEST_BEGIN("match/ctr-against-ctr-same-label-arity");
  {
    RewriteSubst s = {{0}};
    Term a_term = mk_a();
    Term pat = mk_f(mk_v(VAR_x), mk_e());
    Term t   = mk_f(a_term, mk_e());
    CHECK(thvm_match(pat, t, &s));
    CHECK_EQ(s.bindings[VAR_x], a_term);  // bindings[x] = a
  }

  TEST_BEGIN("match/different-label-fails");
  {
    RewriteSubst s = {{0}};
    Term pat = mk_f(mk_v(VAR_x), mk_e());
    Term t   = mk_i(mk_a());
    CHECK_EQ(thvm_match(pat, t, &s), 0u);
  }

  TEST_BEGIN("match/non-linear-consistent");
  {
    // pat = f(x, x); t = f(a, a) -- x must match same on both sides.
    RewriteSubst s = {{0}};
    Term pat = mk_f(mk_v(VAR_x), mk_v(VAR_x));
    Term t   = mk_f(mk_a(), mk_a());
    CHECK(thvm_match(pat, t, &s));
  }

  TEST_BEGIN("match/non-linear-inconsistent-fails");
  {
    // pat = f(x, x); t = f(a, e) -- x bound twice with different terms.
    RewriteSubst s = {{0}};
    Term pat = mk_f(mk_v(VAR_x), mk_v(VAR_x));
    Term t   = mk_f(mk_a(), mk_e());
    CHECK_EQ(thvm_match(pat, t, &s), 0u);
  }

  TEST_BEGIN("subst-apply/replaces-fvr");
  {
    // term = i(x) under {x = a} -> i(a)
    RewriteSubst s = {{0}};
    s.bindings[VAR_x] = mk_a();
    Term t = mk_i(mk_v(VAR_x));
    Term r = thvm_subst_apply(t, &s);
    CHECK_EQ(term_tag(r),                    TAG_CTR);
    CHECK_EQ(term_ext(r),                    LAB_i);
    CHECK_EQ(term_tag(term_ctr_at(r, 0)),    TAG_CTR);
    CHECK_EQ(term_ext(term_ctr_at(r, 0)),    LAB_a);
  }

  TEST_BEGIN("rewrite-step/group-axiom-rewrites-once");
  {
    // Rule: f(x, e) -> x.  Apply to f(a, e); expect a.
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    Term lhs_arr[1] = {lhs};
    Term rhs_arr[1] = {rhs};
    Term t = mk_f(mk_a(), mk_e());
    Term r = thvm_rewrite_step(t, lhs_arr, rhs_arr, 1);
    CHECK_EQ(term_tag(r),  TAG_CTR);
    CHECK_EQ(term_ext(r),  LAB_a);
  }

  TEST_BEGIN("rewrite-step/non-applicable-returns-unchanged");
  {
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    Term lhs_arr[1] = {lhs};
    Term rhs_arr[1] = {rhs};
    Term t = mk_a();   // no f at top, can't fire
    Term r = thvm_rewrite_step(t, lhs_arr, rhs_arr, 1);
    CHECK_EQ(term_tag(r), TAG_CTR);
    CHECK_EQ(term_ext(r), LAB_a);
  }

  TEST_BEGIN("rewrite-normalize/headline-demo-f-a-e-to-a");
  {
    // The stage-3 demo from the plan: normalize f(a, e) under the
    // group axioms.  Only the right-identity rule is needed to fire.
    Term lhs[3] = {
      mk_f(mk_v(VAR_x), mk_e()),                   // f(x, e) = x
      mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),        // f(x, i(x)) = e
      mk_f(mk_f(mk_v(VAR_x), mk_v(VAR_y)), mk_v(VAR_z)),  // f(f(x,y),z) = f(x,f(y,z))
    };
    Term rhs[3] = {
      mk_v(VAR_x),
      mk_e(),
      mk_f(mk_v(VAR_x), mk_f(mk_v(VAR_y), mk_v(VAR_z))),
    };
    Term t = mk_f(mk_a(), mk_e());
    Term r = thvm_rewrite_normalize(t, lhs, rhs, 3, 16);
    CHECK_EQ(term_tag(r), TAG_CTR);
    CHECK_EQ(term_ext(r), LAB_a);
    // After normalization, no more rules fire on `a`.
    Term r2 = thvm_rewrite_step(r, lhs, rhs, 3);
    CHECK_EQ(term_ext(r2), LAB_a);  // unchanged
  }

  TEST_BEGIN("rewrite-normalize/inverse-rule");
  {
    // f(x, i(x)) -> e.  Apply to f(a, i(a)).
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))) };
    Term rhs[1] = { mk_e() };
    Term t = mk_f(mk_a(), mk_i(mk_a()));
    Term r = thvm_rewrite_normalize(t, lhs, rhs, 1, 16);
    CHECK_EQ(term_tag(r), TAG_CTR);
    CHECK_EQ(term_ext(r), LAB_e);
  }

  TEST_BEGIN("rewrite-step/recursive-descent-fires-at-subterm");
  {
    // 5.4 demo: rule f(x, e) -> x.  Term i(f(a, e)) -- the top
    // is `i`, not `f`, so a top-only rewriter would miss.  With
    // recursive descent, the inner f(a, e) reduces to a, and the
    // step returns i(a).
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_e()) };
    Term rhs[1] = { mk_v(VAR_x) };
    Term t = mk_i(mk_f(mk_a(), mk_e()));
    Term r = thvm_rewrite_step(t, lhs, rhs, 1);
    CHECK_EQ(term_tag(r), TAG_CTR);
    CHECK_EQ(term_ext(r), LAB_i);
    Term inner = term_ctr_at(r, 0);
    CHECK_EQ(term_tag(inner), TAG_CTR);
    CHECK_EQ(term_ext(inner), LAB_a);
  }

  TEST_BEGIN("rewrite-normalize/recursive-multi-level");
  {
    // Two levels: i(i(f(a, e))) under f(x, e) -> x reduces to
    // i(i(a)) in one fixpoint pass.
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_e()) };
    Term rhs[1] = { mk_v(VAR_x) };
    Term t = mk_i(mk_i(mk_f(mk_a(), mk_e())));
    Term r = thvm_rewrite_normalize(t, lhs, rhs, 1, 16);
    CHECK_EQ(term_ext(r), LAB_i);
    CHECK_EQ(term_ext(term_ctr_at(r, 0)), LAB_i);
    CHECK_EQ(term_ext(term_ctr_at(term_ctr_at(r, 0), 0)), LAB_a);
  }

  TEST_BEGIN("rewrite-step/top-tried-before-children");
  {
    // Rule l1: i(x) -> e (consumes a single i wrapper).
    // Rule l2: f(x, e) -> x.
    // Term: i(f(a, e)).  Top tried first: l1 matches i(_) at top,
    // returns e.  (Without descent precedence, descent might fire
    // l2 first on the inner f(a, e) -> a, yielding i(a).  Top-first
    // semantics gives e.)
    Term lhs[2] = {
      mk_i(mk_v(VAR_x)),
      mk_f(mk_v(VAR_x), mk_e()),
    };
    Term rhs[2] = {
      mk_e(),
      mk_v(VAR_x),
    };
    Term t = mk_i(mk_f(mk_a(), mk_e()));
    Term r = thvm_rewrite_step(t, lhs, rhs, 2);
    CHECK_EQ(term_ext(r), LAB_e);
  }

  thvm_free();
  TEST_REPORT();
}
