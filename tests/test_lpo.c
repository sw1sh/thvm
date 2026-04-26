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
static Term mk_v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

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

  thvm_free();
  TEST_REPORT();
}
