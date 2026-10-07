// test_heap_atomic.c -- atomic heap primitives (single-thread sanity).
//
// Multi-thread torture comes with the pthread worker pool slice; here
// we just confirm the atomic helpers are observationally equivalent
// to the plain heap_* under serial use, and that CAS / exchange /
// fetch-add return the right values + leave the cell in the right
// state.

#include "../src/thvm.c"
#include "test.h"

static int test_read_acq_set_rel_roundtrip(void) {
  TEST_BEGIN("heap_set_rel + heap_read_acq round-trip a Term");
  thvm_init();
  u64 loc = heap_alloc(1);
  Term t = term_new(0, TAG_NUM, DT_INT32, 0xC0FFEE);
  heap_set_rel(loc, t);
  CHECK_EQ(heap_read_acq(loc), t);
  CHECK_EQ(heap_read(loc), t);                // plain reader sees same
  thvm_free();
  return 0;
}

static int test_take_atomic(void) {
  TEST_BEGIN("heap_take_atomic returns prior value, leaves 0");
  thvm_init();
  u64 loc = heap_alloc(1);
  Term t = term_new(0, TAG_NUM, DT_INT32, 42);
  heap_set(loc, t);
  CHECK_EQ(heap_take_atomic(loc), t);
  CHECK_EQ(heap_read(loc), 0u);
  // Second take on an empty cell returns 0.
  CHECK_EQ(heap_take_atomic(loc), 0u);
  thvm_free();
  return 0;
}

static int test_alloc_serial(void) {
  TEST_BEGIN("heap_alloc bumps HEAP_NEXT serially (atomic-fetch-add)");
  thvm_init();
  u64 a = heap_alloc(3);
  u64 b = heap_alloc(2);
  u64 c = heap_alloc(1);
  CHECK_EQ(b, a + 3u);
  CHECK_EQ(c, a + 5u);
  CHECK_EQ(HEAP_NEXT, a + 6u);
  thvm_free();
  return 0;
}

int main(void) {
  test_read_acq_set_rel_roundtrip();
  test_take_atomic();
  test_alloc_serial();
  TEST_REPORT();
}
