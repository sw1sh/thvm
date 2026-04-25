// test_wl_pin.c - wpt1 of the WL-pinned-Terms arc.

#include "../src/thvm.c"
#include "test.h"

static u32 G_ITER_COUNT = 0;
static Term G_ITER_LAST = 0;
static void count_cb(Term t) { G_ITER_COUNT++; G_ITER_LAST = t; }

int main(void) {
  thvm_init();

  TEST_BEGIN("wl-pin/empty-table-iter-no-op");
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 0);

  TEST_BEGIN("wl-pin/pin-and-iter");
  Term a = term_new(0, TAG_TEN, DT_F32, 7);
  Term b = term_new(0, TAG_TEN, DT_F32, 9);
  wl_pin_term(a);
  wl_pin_term(b);
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 2);

  TEST_BEGIN("wl-pin/unpin-removes-entry");
  wl_unpin_term(a);
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 1);
  CHECK_EQ(G_ITER_LAST, b);

  TEST_BEGIN("wl-pin/unpin-missing-is-noop");
  wl_unpin_term(a);   // already unpinned -- no crash, no removal
  wl_unpin_term(0);   // sentinel ignored
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 1);

  TEST_BEGIN("wl-pin/pin-zero-is-noop");
  wl_pin_term(0);
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 1);

  TEST_BEGIN("wl-pin/iter-with-null-callback-no-crash");
  wl_pinned_for_each(NULL);   // must not crash

  TEST_BEGIN("wl-pin/clear-empties-table");
  wl_pin_clear();
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 0);

  TEST_BEGIN("wl-pin/saturated-push-silently-drops");
  // Fill the table to capacity, then push one more.  The
  // overflow drops; iter still finds exactly CAP entries.
  for (u32 i = 0; i < WL_PINNED_TERMS_CAP; i++) {
    wl_pin_term(term_new(0, TAG_TEN, DT_F32, i + 1));
  }
  wl_pin_term(term_new(0, TAG_TEN, DT_F32, 99999));   // overflow
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, WL_PINNED_TERMS_CAP);

  TEST_BEGIN("wl-pin/thvm-init-clears-table");
  thvm_free();
  thvm_init();
  G_ITER_COUNT = 0;
  wl_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 0);

  thvm_free();
  TEST_REPORT();
}
