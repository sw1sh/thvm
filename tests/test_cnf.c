// test_cnf.c -- collapsed normal-form readback (src/cnf/_.c).
//
// cnf reduces a term to WHNF then lifts the first SUP to the top.
// Plain (non-grad) DP projections are Levy-opaque under wnf; cnf is
// the readback layer where their duplication actually fires.  This
// test exercises the four behaviours called out in the Levy-optimal
// plan (docs/plans/levy_optimal.md):
//   1. Pure terms pass through unchanged.
//   2. ERA propagates / DUP-ERA returns ERA.
//   3. Same-label DUP-SUP annihilates.
//   4. Different-label DUP-SUP commutes into a cross product.
// Plus a small recursive stream fixture to confirm collapse drives
// `@X = &L{NUM(0), NUM(1)}` style enumerators end-to-end.

#include "../src/thvm.c"
#include "test.h"

static u64 mk_dup(Term body) {
  u64 loc = heap_alloc(1);
  heap_set(loc, body);
  return loc;
}

static Term mk_app(Term f, Term a) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, a);
  return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
  thvm_init();

  // === pure pass-through ============================================
  TEST_BEGIN("cnf/num-passthrough");
  {
    Term t = build_num(7);
    Term r = cnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 7);
  }

  TEST_BEGIN("cnf/era-passthrough");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term r   = cnf(era);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  TEST_BEGIN("cnf/sup-at-top-passthrough");
  {
    // SUP at the top is the lifted form -- cnf returns it unchanged.
    Term sup = build_sup(7, build_num(1), build_num(2));
    Term r   = cnf(sup);
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 7);
  }

  // === ERA propagation through DUP ==================================
  TEST_BEGIN("cnf/dup-era-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    u64  dup = mk_dup(era);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    Term r   = cnf(dp0);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  // === Same-label DUP-SUP annihilation ==============================
  TEST_BEGIN("cnf/same-label-dup-sup-annihilates-dp0");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term n   = build_num(11);
    Term sup = build_sup(9, era, n);
    u64  dup = mk_dup(sup);
    Term dp0 = term_new(0, TAG_DP0, 9, dup);
    Term dp1 = term_new(0, TAG_DP1, 9, dup);
    Term r0  = cnf(dp0);
    Term r1  = cnf(dp1);
    CHECK_EQ(term_tag(r0), TAG_ERA);
    CHECK_EQ(term_tag(r1), TAG_NUM);
    CHECK_EQ(term_val(r1), 11);
  }

  // === Different-label DUP-SUP commute (cross product) ==============
  TEST_BEGIN("cnf/different-label-dup-sup-lifts-sup-to-top");
  {
    Term n2  = build_num(2);
    Term n3  = build_num(3);
    Term sup = build_sup(8, n2, n3);
    u64  dup = mk_dup(sup);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    Term r   = cnf(dp0);
    // Outer SUP label preserved (8) -- the DP0_7 commuted through.
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 8);
  }

  // === Lift through APP =============================================
  TEST_BEGIN("cnf/lifts-sup-through-app");
  {
    // APP(SUP^L(LAM x.x, LAM y.y), NUM(5)) -- after APP-SUP commute
    // and beta on each side, both branches reduce to NUM(5).  cnf
    // surfaces the SUP at the top.
    u64  lam_loc_a = heap_alloc(1);
    heap_set(lam_loc_a, term_new(0, TAG_VAR, 0, lam_loc_a));
    Term lam_a = term_new(0, TAG_LAM, 0, lam_loc_a);

    u64  lam_loc_b = heap_alloc(1);
    heap_set(lam_loc_b, term_new(0, TAG_VAR, 0, lam_loc_b));
    Term lam_b = term_new(0, TAG_LAM, 0, lam_loc_b);

    Term sup = build_sup(7, lam_a, lam_b);
    Term arg = build_num(5);
    Term app = mk_app(sup, arg);
    Term r   = cnf(app);
    CHECK_EQ(term_tag(r), TAG_SUP);
    u64  rloc = term_val(r);
    Term l    = cnf(heap_read(rloc + 0));
    Term rr   = cnf(heap_read(rloc + 1));
    CHECK_EQ(term_tag(l), TAG_NUM);
    CHECK_EQ(term_val(l), 5);
    CHECK_EQ(term_tag(rr), TAG_NUM);
    CHECK_EQ(term_val(rr), 5);
  }

  // === eval_collapse over a SUP-stream ==============================
  TEST_BEGIN("collapse/sup-stream-emits-three-leaves");
  {
    // &7{ NUM(0), &7{ NUM(1), NUM(2) } }
    Term inner = build_sup(7, build_num(1), build_num(2));
    Term outer = build_sup(7, build_num(0), inner);
    Term out[8] = {0};
    u64  n = eval_collapse(outer, out, 8);
    CHECK_EQ(n, 3);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 0);
    CHECK_EQ(term_val(out[1]), 1);
    CHECK_EQ(term_val(out[2]), 2);
  }

  TEST_BEGIN("collapse/era-branch-dropped");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term sup = build_sup(7, era, build_num(99));
    Term out[4] = {0};
    u64  n = eval_collapse(sup, out, 4);
    CHECK_EQ(n, 1);
    CHECK_EQ(term_val(out[0]), 99);
  }

  // === Same-label DP-over-SUP picks the per-side branch ============
  // x = &L{NUM(0), NUM(1)}, then DP0_L of x picks NUM(0), DP1_L picks
  // NUM(1) (annihilate semantics).  Confirms collapse drives the
  // projection through DUP-SUP correctly.
  TEST_BEGIN("collapse/same-label-dp-over-sup-picks-its-branch");
  {
    Term sup = build_sup(13, build_num(0), build_num(1));
    u64  dup = mk_dup(sup);
    Term dp0 = term_new(0, TAG_DP0, 13, dup);
    Term out0[4] = {0};
    u64  n0 = eval_collapse(dp0, out0, 4);
    CHECK_EQ(n0, 1);
    CHECK_EQ(term_val(out0[0]), 0);

    Term dp1 = term_new(0, TAG_DP1, 13, dup);
    Term out1[4] = {0};
    u64  n1 = eval_collapse(dp1, out1, 4);
    CHECK_EQ(n1, 1);
    CHECK_EQ(term_val(out1[0]), 1);
  }

  thvm_free();
  TEST_REPORT();
}
