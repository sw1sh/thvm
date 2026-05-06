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

static int test_cas_success(void) {
  TEST_BEGIN("heap_cas commits on match, leaves desired");
  thvm_init();
  u64 loc = heap_alloc(1);
  Term old = term_new(0, TAG_NUM, DT_INT32, 1);
  Term new_ = term_new(0, TAG_NUM, DT_INT32, 2);
  heap_set(loc, old);
  Term seen = old;
  CHECK(heap_cas(loc, &seen, new_) == 1);
  CHECK_EQ(seen, old);                        // expected unchanged on success
  CHECK_EQ(heap_read(loc), new_);
  thvm_free();
  return 0;
}

static int test_cas_failure(void) {
  TEST_BEGIN("heap_cas fails on mismatch, writes back current");
  thvm_init();
  u64 loc = heap_alloc(1);
  Term real    = term_new(0, TAG_NUM, DT_INT32, 7);
  Term wrong   = term_new(0, TAG_NUM, DT_INT32, 99);
  Term desired = term_new(0, TAG_NUM, DT_INT32, 8);
  heap_set(loc, real);
  Term seen = wrong;
  CHECK(heap_cas(loc, &seen, desired) == 0);
  CHECK_EQ(seen, real);                       // expected updated to current
  CHECK_EQ(heap_read(loc), real);             // cell unchanged
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

static int test_subst_var_rel(void) {
  TEST_BEGIN("heap_subst_var_rel writes SUB-flagged value (release)");
  thvm_init();
  u64 loc = heap_alloc(1);
  Term v = term_new(0, TAG_NUM, DT_INT32, 0xBEEF);
  heap_subst_var_rel(loc, v);
  Term cell = heap_read_acq(loc);
  CHECK(term_sub_get(cell) == 1);
  // Stripping the SUB bit recovers the original value.
  CHECK_EQ(term_sub_set(cell, 0), v);
  thvm_free();
  return 0;
}

int main(void) {
  test_read_acq_set_rel_roundtrip();
  test_take_atomic();
  test_cas_success();
  test_cas_failure();
  test_alloc_serial();
  test_subst_var_rel();
  TEST_REPORT();
}
