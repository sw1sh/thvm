// test_eql.c - structural equality (TAG_EQL) basic cases.
//
// Stage 1.3a: NUM-NUM compare, ERA-{L,R} propagation.  SUP commutation
// lands in stage 1.3b and is exercised separately there.

#include "../src/thvm.c"
#include "test.h"

static Term build_num(u32 v) {
  return term_new(0, TAG_NUM, 0, v);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("eql/num-num-equal");
  {
    Term t = term_new_eql(build_num(7), build_num(7));
    u64  itrs_before = ITRS;
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 1);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("eql/num-num-not-equal");
  {
    Term t = term_new_eql(build_num(2), build_num(3));
    u64  itrs_before = ITRS;
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 0);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("eql/era-on-left-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term t   = term_new_eql(era, build_num(5));
    u64  itrs_before = ITRS;
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_ERA);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("eql/era-on-right-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term t   = term_new_eql(build_num(5), era);
    u64  itrs_before = ITRS;
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_ERA);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("eql/non-num-non-era-stuck");
  {
    // EQL(LAM, NUM) -- no commutation rule yet, just stays stuck.
    u64  lam_loc = heap_alloc(1);
    Term lam_var = term_new(0, TAG_VAR, 0, lam_loc);
    heap_set(lam_loc, lam_var);
    Term lam = term_new(0, TAG_LAM, 0, lam_loc);
    Term t   = term_new_eql(lam, build_num(5));
    Term r   = wnf(t);
    CHECK_EQ(term_tag(r), TAG_EQL);
  }

  thvm_free();
  TEST_REPORT();
}
