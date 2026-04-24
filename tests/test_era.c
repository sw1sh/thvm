// test_era.c - eraser propagation.
//
// (* a)         → *           (APP-ERA)
// !&L{x,y} = *  → x ← *, y ← * (DUP-ERA)
//
// Spec test. Implementation lands in step 6.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  PENDING("ERA propagation - needs wnf + interact_app_era / interact_dup_era (step 6)");

  TEST_BEGIN("era/app-era-yields-era");
  Term era = term_new(0, TAG_ERA, 0, 0);
  Term arg = term_new(0, TAG_LAM, 0, 0);
  u64  loc = heap_alloc(2);
  heap_set(loc + 0, era);
  heap_set(loc + 1, arg);
  Term app = term_new(0, TAG_APP, 0, loc);
  Term out = wnf(app);
  CHECK_EQ(term_tag(out), TAG_ERA);

  thvm_free();
  TEST_REPORT();
}
