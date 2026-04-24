// test_era.c - eraser propagation.
//
// (* a)               -> *                        (APP-ERA)
// ! &L{x0,x1} = *     -> x0 <- *, x1 <- *         (DUP-ERA)

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("era/app-era-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term arg = term_new(0, TAG_LAM, 0, 0);
    u64  loc = heap_alloc(2);
    heap_set(loc + 0, era);
    heap_set(loc + 1, arg);
    Term app = term_new(0, TAG_APP, 0, loc);
    u64  itrs_before = ITRS;
    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_ERA);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("era/dup-era-active-side-yields-era");
  {
    Term era     = term_new(0, TAG_ERA, 0, 0);
    u64  dup_loc = heap_alloc(1);
    heap_set(dup_loc, era);
    Term dp0 = term_new(0, TAG_DP0, 7, dup_loc);
    Term out = wnf(dp0);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  thvm_free();
  TEST_REPORT();
}
