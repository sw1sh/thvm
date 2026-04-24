// test_dup_sup.c - duplication interacting with superposition.
//
// Same label (annihilate):
//   ! &L{x0, x1} = &L{a, b}    →   x0 ← a,  x1 ← b
//
// Different label (commute):
//   ! &L{x0, x1} = &R{a, b}    →   ! &L{A0,A1} = a
//                                  ! &L{B0,B1} = b
//                                  x0 ← &R{A0, B0}
//                                  x1 ← &R{A1, B1}
//
// Spec test. Implementation lands in step 6.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  PENDING("DUP-SUP - needs wnf + interact_dup_sup (step 6)");

  // Same-label case: build !&7{x0,x1} = &7{ERA, LAM}; <body uses x0>
  // and check x0 reduces to ERA.
  TEST_BEGIN("dup-sup/same-label-annihilate");
  Term era = term_new(0, TAG_ERA, 0, 0);
  Term lam = term_new(0, TAG_LAM, 0, 0);

  u64  sup_loc = heap_alloc(2);
  heap_set(sup_loc + 0, era);
  heap_set(sup_loc + 1, lam);
  Term sup = term_new(0, TAG_SUP, 7, sup_loc);

  u64 dup_loc = heap_alloc(1);
  heap_set(dup_loc, sup);
  Term dp0 = term_new(0, TAG_DP0, 7, dup_loc);

  Term out = wnf(dp0);
  CHECK_EQ(term_tag(out), TAG_ERA);

  thvm_free();
  TEST_REPORT();
}
