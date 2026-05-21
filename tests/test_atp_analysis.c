// test_atp_analysis.c -- algebraic-structure detection + precedence
// generation for the IC-native ATP.
//
// Exercises src/atp/precedence.c -- the port of Waldmeister's
// PhilMarlow (algebraic-structure recognition) and
// Praezedenzgenerator (precedence generator).  Verifies that
// commutativity / associativity / idempotence / identity /
// inverse / distributivity axioms are detected from an equation
// set, and that the generated precedence respects the expected
// arity / inverse / distributor ordering.

#include "../src/thvm.c"
#include "test.h"

// Function-symbol labels for the fixtures below.
#define L_f 1u   // binary product
#define L_g 2u   // binary sum
#define L_e 3u   // unit constant
#define L_i 4u   // unary inverse
#define L_a 5u   // ordinary constant

static Term v(u32 id) { return term_new_fvr(id); }
static Term ctr0(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term ctr1(u32 lab, Term x) {
  Term c[1] = { x };
  return term_new_ctr(lab, c, 1);
}
static Term ctr2(u32 lab, Term x, Term y) {
  Term c[2] = { x, y };
  return term_new_ctr(lab, c, 2);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("atp-analysis/commutativity");
  {
    // f(x0,x1) = f(x1,x0)
    Term lhs[1] = { ctr2(L_f, v(0), v(1)) };
    Term rhs[1] = { ctr2(L_f, v(1), v(0)) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 1, p, 8);
    CHECK(p[L_f].seen);
    CHECK(p[L_f].is_commutative);
    CHECK(!p[L_f].is_associative);
    CHECK(!p[L_f].is_idempotent);
    CHECK_EQ(p[L_f].arity, 2u);
  }

  TEST_BEGIN("atp-analysis/associativity-both-orientations");
  {
    // f(f(x0,x1),x2) = f(x0,f(x1,x2))
    Term lhs[1] = { ctr2(L_f, ctr2(L_f, v(0), v(1)), v(2)) };
    Term rhs[1] = { ctr2(L_f, v(0), ctr2(L_f, v(1), v(2))) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 1, p, 8);
    CHECK(p[L_f].is_associative);
    CHECK(!p[L_f].is_commutative);

    // Mirrored orientation: f(x0,f(x1,x2)) = f(f(x0,x1),x2).
    Term lhs2[1] = { ctr2(L_f, v(0), ctr2(L_f, v(1), v(2))) };
    Term rhs2[1] = { ctr2(L_f, ctr2(L_f, v(0), v(1)), v(2)) };
    AtpSymProps p2[8];
    atp_analyze_axioms(lhs2, rhs2, 1, p2, 8);
    CHECK(p2[L_f].is_associative);
  }

  TEST_BEGIN("atp-analysis/idempotence");
  {
    // f(x0,x0) = x0
    Term lhs[1] = { ctr2(L_f, v(0), v(0)) };
    Term rhs[1] = { v(0) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 1, p, 8);
    CHECK(p[L_f].is_idempotent);
    CHECK(!p[L_f].is_commutative);
  }

  TEST_BEGIN("atp-analysis/left-and-right-identity");
  {
    // f(e,x0) = x0  and  f(x0,e) = x0
    Term lhs[2] = { ctr2(L_f, ctr0(L_e), v(0)),
                    ctr2(L_f, v(0), ctr0(L_e)) };
    Term rhs[2] = { v(0), v(0) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 2, p, 8);
    CHECK(p[L_f].has_left_unit);
    CHECK(p[L_f].has_right_unit);
    CHECK(p[L_e].is_unit_symbol);
    CHECK_EQ(p[L_e].arity, 0u);
  }

  TEST_BEGIN("atp-analysis/identity-rhs-orientation");
  {
    // x0 = f(x0,e)  -- the `= x` side sits on the left.
    Term lhs[1] = { v(0) };
    Term rhs[1] = { ctr2(L_f, v(0), ctr0(L_e)) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 1, p, 8);
    CHECK(p[L_f].has_right_unit);
    CHECK(p[L_e].is_unit_symbol);
  }

  TEST_BEGIN("atp-analysis/inverse");
  {
    // f(i(x0),x0) = e  -- left inverse.
    Term lhs[1] = { ctr2(L_f, ctr1(L_i, v(0)), v(0)) };
    Term rhs[1] = { ctr0(L_e) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 1, p, 8);
    CHECK(p[L_f].has_inverse);
    CHECK(p[L_i].is_inverse_symbol);
    CHECK(p[L_e].is_unit_symbol);
    CHECK_EQ(p[L_i].arity, 1u);
  }

  TEST_BEGIN("atp-analysis/distributivity");
  {
    // f(x0,g(x1,x2)) = g(f(x0,x1),f(x0,x2))  -- f distributes over g.
    Term lhs[1] = { ctr2(L_f, v(0), ctr2(L_g, v(1), v(2))) };
    Term rhs[1] = { ctr2(L_g, ctr2(L_f, v(0), v(1)),
                              ctr2(L_f, v(0), v(2))) };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 1, p, 8);
    CHECK(p[L_f].distributes);
    CHECK_EQ(p[L_f].distributes_over, L_g);
    CHECK(!p[L_g].distributes);
  }

  TEST_BEGIN("atp-analysis/group-axiom-set");
  {
    // Full group axioms over f / i / e:
    //   f(x0,e)        = x0          (right identity)
    //   f(x0,i(x0))    = e           (right inverse)
    //   f(f(x0,x1),x2) = f(x0,f(x1,x2))   (associativity)
    Term lhs[3] = {
      ctr2(L_f, v(0), ctr0(L_e)),
      ctr2(L_f, v(0), ctr1(L_i, v(0))),
      ctr2(L_f, ctr2(L_f, v(0), v(1)), v(2)),
    };
    Term rhs[3] = {
      v(0),
      ctr0(L_e),
      ctr2(L_f, v(0), ctr2(L_f, v(1), v(2))),
    };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 3, p, 8);
    CHECK(p[L_f].is_associative);
    CHECK(p[L_f].has_right_unit);
    CHECK(p[L_f].has_inverse);
    CHECK(p[L_i].is_inverse_symbol);
    CHECK(p[L_e].is_unit_symbol);

    // Precedence: inverse `i` highest, then binary `f`, then the
    // unit `e`, then any ordinary constant.  Generated from the
    // analysis above.
    u32 prec[8] = {0};
    u32 n = atp_generate_precedence(p, 8, prec);
    CHECK_EQ(n, 3u);   // f, i, e are the three seen symbols
    CHECK(prec[L_i] > prec[L_f]);   // inverse above the product
    CHECK(prec[L_f] > prec[L_e]);   // binary above the unit constant
    CHECK_EQ(prec[L_a], 0u);        // unseen label stays rank 0
  }

  TEST_BEGIN("atp-analysis/ring-distributor-above-sum");
  {
    // Ring fragment: f (product) distributes over g (sum), and g
    // is AC.  The distributor f should outrank g.
    //   f(x0,g(x1,x2)) = g(f(x0,x1),f(x0,x2))
    //   g(x0,x1)       = g(x1,x0)
    //   g(g(x0,x1),x2) = g(x0,g(x1,x2))
    Term lhs[3] = {
      ctr2(L_f, v(0), ctr2(L_g, v(1), v(2))),
      ctr2(L_g, v(0), v(1)),
      ctr2(L_g, ctr2(L_g, v(0), v(1)), v(2)),
    };
    Term rhs[3] = {
      ctr2(L_g, ctr2(L_f, v(0), v(1)), ctr2(L_f, v(0), v(2))),
      ctr2(L_g, v(1), v(0)),
      ctr2(L_g, v(0), ctr2(L_g, v(1), v(2))),
    };
    AtpSymProps p[8];
    atp_analyze_axioms(lhs, rhs, 3, p, 8);
    CHECK(p[L_f].distributes);
    CHECK(p[L_g].is_commutative);
    CHECK(p[L_g].is_associative);

    u32 prec[8] = {0};
    atp_generate_precedence(p, 8, prec);
    // f distributes -> ranks above g; g is AC -> demoted within
    // its arity band.  Both effects push f over g.
    CHECK(prec[L_f] > prec[L_g]);
  }

  TEST_BEGIN("atp-analysis/auto-precedence-arity-ladder");
  {
    // No special properties: a unary symbol should still outrank a
    // binary one is NOT expected -- FuchsPraezedenz ranks higher
    // arity higher.  Here `i` is unary, `f` binary, `a` constant:
    // expect f > i > a.
    Term lhs[1] = { ctr2(L_f, ctr1(L_i, ctr0(L_a)), v(0)) };
    Term rhs[1] = { v(0) };
    u32 prec[8] = {0};
    atp_auto_precedence(lhs, rhs, 1, 8, prec);
    CHECK(prec[L_f] > prec[L_i]);   // binary above unary
    CHECK(prec[L_i] > prec[L_a]);   // unary above constant
  }

  thvm_free();
  TEST_REPORT();
}
