// test_unify.c - most-general-unifier on TAG_CTR + TAG_FVR.
//
// Stage 4 prerequisite for critical-pair enumeration.
// Robinson algorithm + occurs check.

#include "../src/thvm.c"
#include "test.h"

#define LAB_e 1u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u
#define VAR_y 1u
#define VAR_z 2u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

  TEST_BEGIN("unify/concrete-equal");
  {
    RewriteSubst s = {{0}};
    CHECK(thvm_unify(mk_e(), mk_e(), &s));
  }

  TEST_BEGIN("unify/var-against-const-binds");
  {
    RewriteSubst s = {{0}};
    Term c = mk_a();
    CHECK(thvm_unify(mk_v(VAR_x), c, &s));
    CHECK_EQ(s.bindings[VAR_x], c);
  }

  TEST_BEGIN("unify/const-against-var-binds-symmetrically");
  {
    RewriteSubst s = {{0}};
    Term c = mk_a();
    CHECK(thvm_unify(c, mk_v(VAR_x), &s));
    CHECK_EQ(s.bindings[VAR_x], c);
  }

  TEST_BEGIN("unify/two-vars-bind-to-each-other");
  {
    RewriteSubst s = {{0}};
    CHECK(thvm_unify(mk_v(VAR_x), mk_v(VAR_y), &s));
    // x -> y or y -> x; either is fine.  Walk lookups should agree.
  }

  TEST_BEGIN("unify/different-constructors-fail");
  {
    RewriteSubst s = {{0}};
    CHECK_EQ(thvm_unify(mk_e(), mk_a(), &s), 0u);
  }

  TEST_BEGIN("unify/structural-recursion");
  {
    // f(x, e) =? f(a, y)  ->  x=a, y=e
    RewriteSubst s = {{0}};
    Term lhs_a = mk_a();
    Term rhs_e = mk_e();
    Term s_term = mk_f(mk_v(VAR_x), rhs_e);
    Term t_term = mk_f(lhs_a,       mk_v(VAR_y));
    CHECK(thvm_unify(s_term, t_term, &s));
    CHECK_EQ(s.bindings[VAR_x], lhs_a);
    CHECK_EQ(s.bindings[VAR_y], rhs_e);
  }

  TEST_BEGIN("unify/occurs-check-fails");
  {
    // x =? f(x, e) -- x cannot equal a term containing itself.
    RewriteSubst s = {{0}};
    CHECK_EQ(thvm_unify(mk_v(VAR_x), mk_f(mk_v(VAR_x), mk_e()), &s), 0u);
  }

  TEST_BEGIN("unify_apply/follows-chain");
  {
    // x -> y -> a; apply to f(x, x)  ->  f(a, a).
    RewriteSubst s = {{0}};
    Term a = mk_a();
    s.bindings[VAR_x] = mk_v(VAR_y);
    s.bindings[VAR_y] = a;
    Term r = thvm_unify_apply(mk_f(mk_v(VAR_x), mk_v(VAR_x)), &s);
    CHECK_EQ(term_tag(r), TAG_CTR);
    CHECK_EQ(term_ext(r), LAB_f);
    CHECK_EQ(term_tag(term_ctr_at(r, 0)), TAG_CTR);
    CHECK_EQ(term_ext(term_ctr_at(r, 0)), LAB_a);
    CHECK_EQ(term_tag(term_ctr_at(r, 1)), TAG_CTR);
    CHECK_EQ(term_ext(term_ctr_at(r, 1)), LAB_a);
  }

  TEST_BEGIN("rename-vars/shifts-fvr-ids");
  {
    // f(x_0, e)  with offset 32  ->  f(x_32, e)
    Term r = thvm_rename_vars(mk_f(mk_v(0), mk_e()), 32);
    CHECK_EQ(term_tag(r), TAG_CTR);
    Term l = term_ctr_at(r, 0);
    CHECK_EQ(term_tag(l), TAG_FVR);
    CHECK_EQ(term_ext(l), 32u);
  }

  thvm_free();
  TEST_REPORT();
}
