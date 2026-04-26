// test_any.c - TAG_ANY wildcard.
//
// ANY is atomic.
//   EQL(ANY, x) -> NUM(1)        (matches anything)
//   EQL(x, ANY) -> NUM(1)
//   ! &L{x0,x1} = ANY  ->  x0 <- ANY,  x1 <- ANY

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("any/eql-any-num-yields-one");
  {
    Term any = term_new_any();
    Term n   = term_new(0, TAG_NUM, 0, 42);
    Term r   = wnf(term_new_eql(any, n));
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 1);
  }

  TEST_BEGIN("any/eql-num-any-yields-one");
  {
    Term any = term_new_any();
    Term n   = term_new(0, TAG_NUM, 0, 42);
    Term r   = wnf(term_new_eql(n, any));
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 1);
  }

  TEST_BEGIN("any/dup-any-dp0");
  {
    Term any = term_new_any();
    u64  dup = heap_alloc(1);
    heap_set(dup, any);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    u64  itrs_before = ITRS;
    Term r = wnf(dp0);
    CHECK_EQ(term_tag(r), TAG_ANY);
    CHECK_EQ(ITRS - itrs_before, 1);  // single DUP-ANY
  }

  TEST_BEGIN("any/dup-any-dp1-via-sub");
  {
    Term any = term_new_any();
    u64  dup = heap_alloc(1);
    heap_set(dup, any);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    Term dp1 = term_new(0, TAG_DP1, 7, dup);
    u64  itrs_before = ITRS;
    Term r0 = wnf(dp0);
    Term r1 = wnf(dp1);
    CHECK_EQ(term_tag(r0), TAG_ANY);
    CHECK_EQ(term_tag(r1), TAG_ANY);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("any/eql-any-with-sup-on-other-side");
  {
    // EQL(&L{NUM(2), NUM(3)}, ANY) -- the SUP commutes first, then
    // each branch's EQL becomes EQL(NUM(_), ANY) -> NUM(1).
    u64 sloc = heap_alloc(2);
    heap_set(sloc + 0, term_new(0, TAG_NUM, 0, 2));
    heap_set(sloc + 1, term_new(0, TAG_NUM, 0, 3));
    Term sup = term_new(0, TAG_SUP, 7, sloc);
    Term any = term_new_any();
    Term t   = term_new_eql(sup, any);
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 1);
    CHECK_EQ(term_tag(out[1]), TAG_NUM);
    CHECK_EQ(term_val(out[1]), 1);
  }

  thvm_free();
  TEST_REPORT();
}
