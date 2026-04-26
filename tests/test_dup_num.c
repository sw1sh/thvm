// test_dup_num.c - DUP on TAG_NUM annihilates atomically.
//
// ! &L{x0, x1} = NUM(v)   ->   x0 <- NUM(v),  x1 <- NUM(v)

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("dup-num/dp0-yields-num");
  {
    Term n   = term_new(0, TAG_NUM, 0, 42);
    u64  dup = heap_alloc(1);
    heap_set(dup, n);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    u64  itrs_before = ITRS;
    Term out = wnf(dp0);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ(term_val(out), 42);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("dup-num/dp0-then-dp1-shares-via-sub");
  {
    // After dp0 fires, the dup cell holds NUM with the SUB flag, so a
    // subsequent dp1 entry resolves it without firing another DUP-NUM.
    Term n   = term_new(0, TAG_NUM, 0, 99);
    u64  dup = heap_alloc(1);
    heap_set(dup, n);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    Term dp1 = term_new(0, TAG_DP1, 7, dup);
    u64  itrs_before = ITRS;
    Term r0 = wnf(dp0);
    Term r1 = wnf(dp1);
    CHECK_EQ(term_tag(r0), TAG_NUM);
    CHECK_EQ(term_val(r0), 99);
    CHECK_EQ(term_tag(r1), TAG_NUM);
    CHECK_EQ(term_val(r1), 99);
    CHECK_EQ(ITRS - itrs_before, 1);  // single DUP-NUM
  }

  thvm_free();
  TEST_REPORT();
}
