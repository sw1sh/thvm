// test_heap.c - flat heap allocator, read/set, take, subst_var.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("heap/alloc-bumps-and-returns-distinct-locs");
  u64 a = heap_alloc(2);
  u64 b = heap_alloc(3);
  CHECK_EQ(a, 0);
  CHECK_EQ(b, 2);
  CHECK_EQ(HEAP_NEXT, 5);

  TEST_BEGIN("heap/set-then-read");
  Term t1 = term_new(0, TAG_LAM, 0, 42);
  Term t2 = term_new(0, TAG_VAR, 0, 7);
  heap_set(a + 0, t1);
  heap_set(a + 1, t2);
  CHECK_EQ(heap_read(a + 0), t1);
  CHECK_EQ(heap_read(a + 1), t2);

  TEST_BEGIN("heap/take-clears-cell");
  Term taken = heap_take(a + 0);
  CHECK_EQ(taken, t1);
  CHECK_EQ(heap_read(a + 0), 0);

  TEST_BEGIN("heap/subst_var-sets-sub-flag");
  Term value = term_new(0, TAG_ERA, 0, 0);
  heap_subst_var(b, value);
  Term cell = heap_read(b);
  CHECK_EQ(term_sub_get(cell), 1);
  CHECK_EQ(term_tag(cell),     TAG_ERA);

  thvm_free();
  TEST_REPORT();
}
