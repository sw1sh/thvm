// test_extern_pin.c - external-caller pin table API.

#include "../src/thvm.c"
#include "test.h"

static u32 G_ITER_COUNT = 0;
static Term G_ITER_LAST = 0;
static void count_cb(Term t) { G_ITER_COUNT++; G_ITER_LAST = t; }

int main(void) {
  thvm_init();

  TEST_BEGIN("extern-pin/empty-table-iter-no-op");
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 0);

  TEST_BEGIN("extern-pin/pin-and-iter");
  Term a = term_new(0, TAG_TEN, DT_F32, 7);
  Term b = term_new(0, TAG_TEN, DT_F32, 9);
  extern_pin_term(a);
  extern_pin_term(b);
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 2);

  TEST_BEGIN("extern-pin/unpin-removes-entry");
  extern_unpin_term(a);
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 1);
  CHECK_EQ(G_ITER_LAST, b);

  TEST_BEGIN("extern-pin/unpin-missing-is-noop");
  extern_unpin_term(a);   // already unpinned -- no crash, no removal
  extern_unpin_term(0);   // sentinel ignored
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 1);

  TEST_BEGIN("extern-pin/pin-zero-is-noop");
  extern_pin_term(0);
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 1);

  TEST_BEGIN("extern-pin/iter-with-null-callback-no-crash");
  extern_pinned_for_each(NULL);

  TEST_BEGIN("extern-pin/clear-empties-table");
  extern_pin_clear();
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 0);

  TEST_BEGIN("extern-pin/saturated-push-silently-drops");
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_CAP; i++) {
    extern_pin_term(term_new(0, TAG_TEN, DT_F32, i + 1));
  }
  extern_pin_term(term_new(0, TAG_TEN, DT_F32, 99999));   // overflow
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, EXTERN_PINNED_TERMS_CAP);

  TEST_BEGIN("extern-pin/thvm-init-clears-table");
  thvm_free();
  thvm_init();
  G_ITER_COUNT = 0;
  extern_pinned_for_each(count_cb);
  CHECK_EQ(G_ITER_COUNT, 0);

  thvm_free();
  TEST_REPORT();
}
