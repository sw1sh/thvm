// test_dup_sup.c - duplication interacting with superposition.
//
// Same label (annihilate):
//   ! &L{x0, x1} = &L{a, b}   ->   x0 <- a,  x1 <- b
//
// Different label (commute):
//   ! &L{x0, x1} = &R{a, b}   ->   ! &L{A0,A1} = a
//                                  ! &L{B0,B1} = b
//                                  x0 <- &R{A0, B0}
//                                  x1 <- &R{A1, B1}

#include "../src/thvm.c"
#include "test.h"

// Build &lab{a, b} -- allocates 2 cells, returns the SUP term.
static Term build_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

// Build the DUP cell holding `body`, returning the dup_loc so callers
// can construct DP0/DP1 against it.
static u64 build_dup(Term body) {
  u64 loc = heap_alloc(1);
  heap_set(loc, body);
  return loc;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("dup-sup/same-label-annihilate-dp0");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term lam = term_new(0, TAG_LAM, 0, 0);
    Term sup = build_sup(7, era, lam);
    u64  dup = build_dup(sup);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    u64  itrs_before = ITRS;
    Term out = wnf(dp0);
    CHECK_EQ(term_tag(out), TAG_ERA);
    CHECK_EQ(ITRS - itrs_before, 1);  // one DUP-SUP firing
  }

  TEST_BEGIN("dup-sup/same-label-substitutes-dp1-side");
  {
    // After dp0 fires DUP-SUP, the dp1 cell should hold the LAM with
    // the SUB flag set so a subsequent dp1 entry picks it up.
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term lam = term_new(0, TAG_LAM, 0, 0);
    Term sup = build_sup(7, era, lam);
    u64  dup = build_dup(sup);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    Term dp1 = term_new(0, TAG_DP1, 7, dup);
    (void)wnf(dp0);
    Term out = wnf(dp1);
    CHECK_EQ(term_tag(out), TAG_LAM);
  }

  TEST_BEGIN("dup-sup/cross-label-commute-dp0-head");
  {
    // !&7{x0,x1} = &8{ERA, ERA}; project x0.
    // Head should be SUP(label=8) wrapping two stuck DP0_7 nodes.
    Term era1 = term_new(0, TAG_ERA, 0, 0);
    Term era2 = term_new(0, TAG_ERA, 0, 0);
    Term sup  = build_sup(8, era1, era2);
    u64  dup  = build_dup(sup);
    Term dp0  = term_new(0, TAG_DP0, 7, dup);
    u64  itrs_before = ITRS;
    Term out  = wnf(dp0);
    CHECK_EQ(term_tag(out), TAG_SUP);
    CHECK_EQ(term_ext(out), 8);  // outer label preserved from R-SUP
    CHECK_EQ(ITRS - itrs_before, 1);  // single DUP-SUP commute
  }

  TEST_BEGIN("dup-sup/cross-label-commute-inner-structure");
  {
    // After commute, the returned SUP's two cells should hold DP0_7
    // nodes that themselves resolve to ERA (via DUP-ERA on each leaf).
    Term era1 = term_new(0, TAG_ERA, 0, 0);
    Term era2 = term_new(0, TAG_ERA, 0, 0);
    Term sup  = build_sup(8, era1, era2);
    u64  dup  = build_dup(sup);
    Term dp0  = term_new(0, TAG_DP0, 7, dup);
    Term out  = wnf(dp0);
    u64  out_loc = term_val(out);
    Term l    = wnf(heap_read(out_loc + 0));
    Term r    = wnf(heap_read(out_loc + 1));
    CHECK_EQ(term_tag(l), TAG_ERA);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  TEST_BEGIN("dup-sup/cross-label-commute-both-projections");
  {
    // Project DP0 first, then DP1 -- both must yield SUP(R).  DP1 is
    // resolved via the SUB flag installed by heap_subst_cop, so it
    // doesn't fire a second DUP-SUP.
    Term era1 = term_new(0, TAG_ERA, 0, 0);
    Term era2 = term_new(0, TAG_ERA, 0, 0);
    Term sup  = build_sup(8, era1, era2);
    u64  dup  = build_dup(sup);
    Term dp0  = term_new(0, TAG_DP0, 7, dup);
    Term dp1  = term_new(0, TAG_DP1, 7, dup);
    u64  itrs_before = ITRS;
    Term out0 = wnf(dp0);
    Term out1 = wnf(dp1);
    CHECK_EQ(term_tag(out0), TAG_SUP);
    CHECK_EQ(term_ext(out0), 8);
    CHECK_EQ(term_tag(out1), TAG_SUP);
    CHECK_EQ(term_ext(out1), 8);
    CHECK_EQ(ITRS - itrs_before, 1);  // only one commute fires
  }

  thvm_free();
  TEST_REPORT();
}
