// test_rpo.c -- thvm_rpo Recursive Path Ordering.
//
// Covers:
//   * RPO_EQ on identical terms.
//   * RPO_GT via subterm domination, precedence, lex (all-LEX matches LPO).
//   * RPO_GT via multiset status: f(a, b) vs f(b, a) MUL-status -> EQ.
//   * RPO_GT under MUL when one side has a strictly-larger element.
//   * RPO_UN on incomparable cases.
//   * Variable-occurrence rules.

#include "../src/thvm.c"
#include "test.h"

#define LAB_a 1u
#define LAB_b 2u
#define LAB_c 3u
#define LAB_f 4u
#define LAB_g 5u

// Precedence: g > f > c > b > a.
static u32 PREC[6] = {
  /* 0 */ 0u,
  /* a */ 1u,
  /* b */ 2u,
  /* c */ 3u,
  /* f */ 4u,
  /* g */ 5u,
};

// Two configurations: one with all-LEX status (== LPO equivalent), one
// where `f` is MUL.
static const RpoStatus STATUS_ALL_LEX[6] = {
  RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
  RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
};
static const RpoStatus STATUS_F_MUL[6] = {
  RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
  RPO_STATUS_LEX, RPO_STATUS_MUL, RPO_STATUS_LEX,
};
static const RpoConfig CFG_LEX = {
  .precedence = PREC, .status = STATUS_ALL_LEX, .n_labels = 6,
};
static const RpoConfig CFG_F_MUL = {
  .precedence = PREC, .status = STATUS_F_MUL, .n_labels = 6,
};

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term f2(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term g1(Term x) { Term cs[1] = {x}; return term_new_ctr(LAB_g, cs, 1); }
static Term v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

  TEST_BEGIN("rpo/eq-identical");
  {
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = f2(k(LAB_a), k(LAB_b));
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_LEX), (int)RPO_EQ);
  }

  TEST_BEGIN("rpo/gt-subterm");
  {
    // f(a, b) > a (a is a subterm of f(a, b)).
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = k(LAB_a);
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_LEX), (int)RPO_GT);
    CHECK_EQ((int)thvm_rpo(t, s, &CFG_LEX), (int)RPO_LT);
  }

  TEST_BEGIN("rpo/gt-precedence");
  {
    // g(b) vs f(a, b): g > f via precedence; g(b) > each of a, b.
    Term s = g1(k(LAB_b));
    Term t = f2(k(LAB_a), k(LAB_b));
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_LEX), (int)RPO_GT);
  }

  TEST_BEGIN("rpo/gt-lex-on-equal-heads");
  {
    // Under all-LEX: f(a, c) > f(a, b) via lex (a==a, c>b).
    Term s = f2(k(LAB_a), k(LAB_c));
    Term t = f2(k(LAB_a), k(LAB_b));
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_LEX), (int)RPO_GT);
  }

  TEST_BEGIN("rpo/mul-eq-permutation");
  {
    // Under f-MUL: f(a, b) and f(b, a) are MUL-equal (same multiset).
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = f2(k(LAB_b), k(LAB_a));
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_F_MUL), (int)RPO_EQ);
  }

  TEST_BEGIN("rpo/mul-gt-extra-larger");
  {
    // Under f-MUL: f(c, b) >_mul f(a, b) because after cancelling b,
    // the leftovers are {c} > {a} (single dominator).
    Term s = f2(k(LAB_c), k(LAB_b));
    Term t = f2(k(LAB_a), k(LAB_b));
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_F_MUL), (int)RPO_GT);
    CHECK_EQ((int)thvm_rpo(t, s, &CFG_F_MUL), (int)RPO_LT);
  }

  TEST_BEGIN("rpo/var-cases");
  {
    // x and y both FVR, distinct ids: UN.
    CHECK_EQ((int)thvm_rpo(v(0), v(1), &CFG_LEX), (int)RPO_UN);
    // f(x, b) > x (x is a strict subterm).
    Term s = f2(v(0), k(LAB_b));
    CHECK_EQ((int)thvm_rpo(s, v(0), &CFG_LEX), (int)RPO_GT);
    CHECK_EQ((int)thvm_rpo(v(0), s, &CFG_LEX), (int)RPO_LT);
  }

  TEST_BEGIN("rpo/mul-vs-lex-discrimination");
  {
    // f(a, b) vs f(b, a): under all-LEX these are RPO_UN (a vs b lex
    // differs but b vs a needed for dominate-all-args fails: f(a,b) > a
    // YES via subterm; f(a,b) > b YES via subterm; lex(a,b) vs (b,a):
    // a < b first slot -> overall LT under lex; but the dominate-all
    // requires t-side to dominate s's args, also passes since b > a.
    // Actually both directions GT under all-LEX is impossible; one is
    // GT, the other LT.  Confirm via thvm_rpo: lex order (a,b)<(b,a)
    // gives LT.
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = f2(k(LAB_b), k(LAB_a));
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_LEX), (int)RPO_LT);
    CHECK_EQ((int)thvm_rpo(t, s, &CFG_LEX), (int)RPO_GT);
    // Under f-MUL they are EQ (multiset cancels).
    CHECK_EQ((int)thvm_rpo(s, t, &CFG_F_MUL), (int)RPO_EQ);
  }

  thvm_free();
  TEST_REPORT();
}
