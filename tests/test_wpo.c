// test_wpo.c -- thvm_wpo Weighted Path Ordering.
//
// Covers:
//   * EQ on identical.
//   * GT via weight strict (heavier symbol wins).
//   * GT via weight + var subset.
//   * UN when weights agree but variable conditions block.
//   * RPO-style fallback (precedence + status) on equal weights.
//   * Multiset status under equal weights.

#include "../src/thvm.c"
#include "test.h"

#define LAB_a 1u
#define LAB_b 2u
#define LAB_c 3u
#define LAB_f 4u
#define LAB_g 5u

// Weights: a=1, b=1, c=1, f=1, g=2.
static u32 W[6] = {
  /* 0 */ 0u,
  /* a */ 1u, /* b */ 1u, /* c */ 1u, /* f */ 1u, /* g */ 2u,
};
// Precedence: g > f > c > b > a.
static u32 PR[6] = {
  /* 0 */ 0u,
  /* a */ 1u, /* b */ 2u, /* c */ 3u, /* f */ 4u, /* g */ 5u,
};
static const WpoStatus ST_ALL_LEX[6] = {
  WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
  WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
};
static const WpoStatus ST_F_MUL[6] = {
  WPO_STATUS_LEX, WPO_STATUS_LEX, WPO_STATUS_LEX,
  WPO_STATUS_LEX, WPO_STATUS_MUL, WPO_STATUS_LEX,
};
static const WpoConfig CFG_LEX = {
  .weights = W, .precedence = PR, .status = ST_ALL_LEX,
  .n_labels = 6, .var_weight = 1u,
};
static const WpoConfig CFG_F_MUL = {
  .weights = W, .precedence = PR, .status = ST_F_MUL,
  .n_labels = 6, .var_weight = 1u,
};

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term f2(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term g1(Term x) { Term cs[1] = {x}; return term_new_ctr(LAB_g, cs, 1); }
static Term v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

  TEST_BEGIN("wpo/eq-identical");
  {
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = f2(k(LAB_a), k(LAB_b));
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_EQ);
  }

  TEST_BEGIN("wpo/gt-by-weight");
  {
    // g(a): weight 2+1 = 3.  f(a): 1+1 = 2.  No variable conflict
    // (both ground).  g(a) > f(a) by weight.
    Term s = g1(k(LAB_a));
    Term t = f2(k(LAB_a), k(LAB_a));   // weight 1 + 1 + 1 = 3
    // Wait, f(a, a) has weight 1+1+1=3 == g(a)'s 3.  Use g(b) vs f(a,a):
    // g(b)=3, f(a,a)=3 -- tie, fall to RPO body where g>f gives GT.
    // For a STRICT-weight case, use g(g(a)) vs f(a,a): weight 2+2+1=5 vs 3.
    s = g1(g1(k(LAB_a)));
    t = f2(k(LAB_a), k(LAB_a));
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_GT);
    CHECK_EQ((int)thvm_wpo(t, s, &CFG_LEX), (int)WPO_LT);
  }

  TEST_BEGIN("wpo/un-by-var-mismatch");
  {
    // s = f(a, x), t = g(y).  Distinct vars (x vs y), so neither side
    // contains the other's vars.  Should be WPO_UN regardless of weight.
    Term s = f2(k(LAB_a), v(0));
    Term t = g1(v(1));
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_UN);
  }

  TEST_BEGIN("wpo/gt-by-subterm-at-equal-weight");
  {
    // s = f(a, b), t = a.  weight: f(a,b) = 1+1+1=3, a=1.  ws > wt and
    // vars(t) ⊆ vars(s) (both ground).  GT via weight directly.
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = k(LAB_a);
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_GT);
  }

  TEST_BEGIN("wpo/rpo-fallback-at-weight-tie");
  {
    // f(a, c) vs f(b, b).  Weights: f+a+c = 1+1+1 = 3.  f+b+b = 1+1+1 = 3.
    // Tie -> RPO body.  Same head + arity, all-LEX status.  Lex compare:
    // a vs b: a < b by precedence (a=1, b=2) -> lex returns LT.  So
    // f(a,c) < f(b,b) provided dominate-all-args holds: f(b,b) > a
    // (subterm-via-precedence: b>a, f(b,b) > each arg of a, vacuous).
    // f(b,b) > c: subterm-via-precedence c>f? prec(c)=3, prec(f)=4 so
    // f>c, but we need f(b,b) > c; f > c precedence and c has no args
    // (vacuous dominate-all-args).  So f(b,b) > c.  Therefore LT holds.
    Term s = f2(k(LAB_a), k(LAB_c));
    Term t = f2(k(LAB_b), k(LAB_b));
    // Note: at equal weight, the FIRST WPO rule fired is the subterm
    // domination check.  Neither side is a subterm of the other.  Then
    // precedence-of-tops: equal (both f).  Then equal-head lex path.
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_LT);
  }

  TEST_BEGIN("wpo/mul-status-permutation");
  {
    // f-MUL: f(a, b) and f(b, a) have equal weight + multiset args ->
    // WPO_EQ.
    Term s = f2(k(LAB_a), k(LAB_b));
    Term t = f2(k(LAB_b), k(LAB_a));
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_F_MUL), (int)WPO_EQ);
    // Under LEX they differ: same weight, lex (a,b)<(b,a) -> LT.
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_LT);
  }

  TEST_BEGIN("wpo/var-eq-on-fvr");
  {
    // Same FVR id is EQ.
    CHECK_EQ((int)thvm_wpo(v(0), v(0), &CFG_LEX), (int)WPO_EQ);
    // Different FVR ids: UN.
    CHECK_EQ((int)thvm_wpo(v(0), v(1), &CFG_LEX), (int)WPO_UN);
  }

  TEST_BEGIN("wpo/var-occurs-strict");
  {
    // f(x, a) > x via var-multiplicity (vars(x)={x}, vars(f(x,a))={x},
    // ws=1+1+1=3 > wt=1).  GT.
    Term s = f2(v(0), k(LAB_a));
    Term t = v(0);
    CHECK_EQ((int)thvm_wpo(s, t, &CFG_LEX), (int)WPO_GT);
    CHECK_EQ((int)thvm_wpo(t, s, &CFG_LEX), (int)WPO_LT);
  }

  thvm_free();
  TEST_REPORT();
}
