// test_kbo.c - Knuth-Bendix ordering on first-order terms.
//
// Stage 2 demo from docs/plans/waldmeister_ic_atp.md: orient the
// group axiom f(x, e) = x correctly under the Waldmeister default
// KBO config (weights i=0, f=1, e=1, a=1; precedence i > f > e > a;
// w0 = 1).

#include "../src/thvm.c"
#include "test.h"

// Hardcoded label ids for the {e, i, f, a} group signature.
#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define LAB_MAX 5u

// Variable ids
#define VAR_x 0u
#define VAR_y 1u

static u32 group_weights   [LAB_MAX] = {0, 1, 0, 1, 1};   // [_, e, i, f, a]
static u32 group_precedence[LAB_MAX] = {0, 2, 4, 3, 1};   // i > f > e > a

static const KboConfig GROUP_CFG = {
  .weights     = group_weights,
  .precedence  = group_precedence,
  .n_labels    = LAB_MAX,
  .var_weight  = 1,
};

// Term builders for the group signature.
static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_i(Term x) { Term cs[1] = {x}; return term_new_ctr(LAB_i, cs, 1); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }

int main(void) {
  thvm_init();

  TEST_BEGIN("kbo/equal-terms");
  {
    Term s = mk_e();
    Term t = mk_e();
    CHECK_EQ((int)thvm_kbo(s, t, &GROUP_CFG), KBO_EQ);
  }

  TEST_BEGIN("kbo/equal-with-vars");
  {
    Term s = mk_f(term_new_fvr(VAR_x), mk_e());
    Term t = mk_f(term_new_fvr(VAR_x), mk_e());
    CHECK_EQ((int)thvm_kbo(s, t, &GROUP_CFG), KBO_EQ);
  }

  TEST_BEGIN("kbo/different-vars-incomparable");
  {
    Term s = term_new_fvr(VAR_x);
    Term t = term_new_fvr(VAR_y);
    CHECK_EQ((int)thvm_kbo(s, t, &GROUP_CFG), KBO_UN);
  }

  TEST_BEGIN("kbo/group-axiom-f-x-e-greater-than-x");
  {
    // The headline demo: f(x, e) > x under Waldmeister's default
    // group-axiom KBO.
    //   weight(f(x, e)) = w(f) + w0 + w(e) = 1 + 1 + 1 = 3
    //   weight(x)       = w0                = 1
    //   var_x: count_s = 1, count_t = 1  -> dominates OK
    //   3 > 1  -> GT
    Term s = mk_f(term_new_fvr(VAR_x), mk_e());
    Term t = term_new_fvr(VAR_x);
    CHECK_EQ((int)thvm_kbo(s, t, &GROUP_CFG), KBO_GT);
    // Reverse direction.
    CHECK_EQ((int)thvm_kbo(t, s, &GROUP_CFG), KBO_LT);
  }

  TEST_BEGIN("kbo/dominance-failure-is-incomparable");
  {
    // s = e (no x), t = x.  weight(s) = 1, weight(t) = 1; tie on
    // weight.  vc_s[x]=0, vc_t[x]=1: s does not dominate.  vc_t[x] >=
    // vc_s[x] but the equal-weight case with t being a variable and s
    // a CTR triggers the FVR branch -> UN.
    Term s = mk_e();
    Term t = term_new_fvr(VAR_x);
    KboCmp c = thvm_kbo(s, t, &GROUP_CFG);
    // s has no var; weights tied but s is a CTR vs t-FVR -> UN.
    CHECK_EQ((int)c, KBO_UN);
  }

  TEST_BEGIN("kbo/inverse-greater-than-multiplication");
  {
    // i has highest precedence.  i(a) vs f(a, a):
    //   weight(i(a))    = w(i) + w(a) = 0 + 1 = 1
    //   weight(f(a, a)) = w(f) + w(a) + w(a) = 1 + 1 + 1 = 3
    //   3 > 1 -> f(a, a) > i(a).
    // (Precedence only kicks in on weight ties.)
    Term s = mk_f(mk_a(), mk_a());
    Term t = mk_i(mk_a());
    CHECK_EQ((int)thvm_kbo(s, t, &GROUP_CFG), KBO_GT);
  }

  TEST_BEGIN("kbo/precedence-tiebreak-on-equal-weight");
  {
    // i(e) vs f(e, e):
    //   weight(i(e)) = 0 + 1 = 1
    //   weight(f(e, e)) = 1 + 1 + 1 = 3
    // Doesn't tie.  Need a real precedence tie.  Construct via
    // i(a) vs e: w(i(a)) = 1, w(e) = 1.  Top symbols: i (prec=4) vs e
    // (prec=2).  i > e in precedence -> i(a) > e.
    Term s = mk_i(mk_a());
    Term t = mk_e();
    CHECK_EQ((int)thvm_kbo(s, t, &GROUP_CFG), KBO_GT);
  }

  // === soa eqn-2: unorientable single-binary-op equation ===============
  // ShefferAxiomsOrAssociativity's 3rd axiom (tools/baselines/wm_align_
  // reports/soa.txt).  Under the Sheffer KBO (one binary symbol
  // opCenterdot weight 1, var_weight 1):
  //   LHS = oc(oc(V0, oc(V1,V2)), oc(V0, oc(V1,V2)))
  //   RHS = oc(oc(oc(V1,V1), V0), oc(oc(V2,V2), V0))
  // weight(LHS) = weight(RHS) (7 oc + 6 var leaves each) and the variable
  // multisets match {V0:2,V1:2,V2:2}, so it is a KBO weight+var tie.  The
  // standard lex tiebreak recurses to arg1: oc(V0, oc(V1,V2)) vs
  // oc(oc(V1,V1), V0); their arg1 is V0 (a variable) vs oc(V1,V1) (a term
  // NOT containing V0) -- a variable-vs-non-containing-term pair is
  // INCOMPARABLE, so the whole comparison is KBO_UN.  Waldmeister keeps
  // this fact unoriented (mono=0); thvm must too (r_orient stays 0).  The
  // production linear comparator must agree with the naive oracle here.
  TEST_BEGIN("kbo/soa-eqn2-unorientable");
  {
    // Sheffer signature: single binary symbol opCenterdot = LAB_f (weight
    // 1), var_weight 1.  Reuse the group cfg's LAB_f as opCenterdot; the
    // weight (1) and var_weight (1) match the soa .pr ordering, and no
    // other symbol appears in these terms, so the precedence chain is
    // never consulted (top symbols always equal).
    u32 V0 = 0u, V1 = 1u, V2 = 2u;
    // inner = oc(V1, V2); two fresh copies avoid pointer-sharing.
    Term lhs = mk_f(mk_f(term_new_fvr(V0), mk_f(term_new_fvr(V1), term_new_fvr(V2))),
                    mk_f(term_new_fvr(V0), mk_f(term_new_fvr(V1), term_new_fvr(V2))));
    Term rhs = mk_f(mk_f(mk_f(term_new_fvr(V1), term_new_fvr(V1)), term_new_fvr(V0)),
                    mk_f(mk_f(term_new_fvr(V2), term_new_fvr(V2)), term_new_fvr(V0)));
    // Standard KBO: incomparable in both directions.
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &GROUP_CFG), KBO_UN);
    CHECK_EQ((int)thvm_kbo(rhs, lhs, &GROUP_CFG), KBO_UN);
    // The production linear comparator must match the naive oracle.
    CHECK_EQ((int)thvm_kbo(lhs, rhs, &GROUP_CFG),
             (int)thvm_kbo_naive(lhs, rhs, &GROUP_CFG));
    CHECK_EQ((int)thvm_kbo(rhs, lhs, &GROUP_CFG),
             (int)thvm_kbo_naive(rhs, lhs, &GROUP_CFG));
    // arg1 subpair: oc(V0, oc(V1,V2)) vs oc(oc(V1,V1), V0) -- also UN
    // (recurses to V0 vs oc(V1,V1): variable vs non-containing term).
    Term la1 = mk_f(term_new_fvr(V0), mk_f(term_new_fvr(V1), term_new_fvr(V2)));
    Term ra1 = mk_f(mk_f(term_new_fvr(V1), term_new_fvr(V1)), term_new_fvr(V0));
    CHECK_EQ((int)thvm_kbo(la1, ra1, &GROUP_CFG), KBO_UN);
    CHECK_EQ((int)thvm_kbo(la1, ra1, &GROUP_CFG),
             (int)thvm_kbo_naive(la1, ra1, &GROUP_CFG));
  }

  // === LPO pretest fast-path agreement =================================
  // `thvm_lpo` runs the Waldmeister `LPOVortests` variable-set pretest
  // before the full recursion (`lpo_rec`).  The pretest must be a pure
  // optimization: assert it agrees with the un-pretested `lpo_rec` on a
  // spread of term pairs spanning the variable-set cases (disjoint vars,
  // shared vars, subset, superset, no vars).
  TEST_BEGIN("lpo/pretest-agrees-with-full-recursion");
  {
    static u32 lpo_prec[LAB_MAX] = {0, 2, 4, 3, 1};   // e<a<f<i style
    const LpoConfig LPO_CFG = {
      .precedence = lpo_prec,
      .n_labels   = LAB_MAX,
    };
    // A pool of terms covering every variable-set shape:
    //   - constants (empty var set)
    //   - single-var terms (x, y, z)
    //   - multi-var terms with shared / disjoint / nested var sets
    Term x = term_new_fvr(0u);
    Term y = term_new_fvr(1u);
    Term z = term_new_fvr(2u);
    Term pool[14];
    u32 np = 0;
    pool[np++] = mk_e();                                  // {}
    pool[np++] = mk_a();                                  // {}
    pool[np++] = x;                                       // {x}
    pool[np++] = y;                                       // {y}
    pool[np++] = mk_i(x);                                 // {x}
    pool[np++] = mk_i(y);                                 // {y}
    pool[np++] = mk_f(x, mk_e());                         // {x}
    pool[np++] = mk_f(x, y);                              // {x,y}
    pool[np++] = mk_f(y, x);                              // {x,y}
    pool[np++] = mk_f(x, mk_i(y));                        // {x,y}
    pool[np++] = mk_f(mk_f(x, y), z);                     // {x,y,z}
    pool[np++] = mk_f(mk_i(x), mk_a());                   // {x}
    pool[np++] = mk_f(z, z);                              // {z}
    pool[np++] = mk_f(mk_f(x, x), mk_i(y));               // {x,y}
    for (u32 i = 0; i < np; i++) {
      for (u32 j = 0; j < np; j++) {
        LpoCmp full = lpo_rec(pool[i], pool[j], &LPO_CFG);
        LpoCmp fast = thvm_lpo(pool[i], pool[j], &LPO_CFG);
        CHECK_EQ((int)fast, (int)full);
      }
    }
  }

  thvm_free();
  TEST_REPORT();
}
