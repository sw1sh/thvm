// test_eql.c - structural equality (TAG_EQL).
//
// 1.3a: NUM-NUM compare, ERA-{L,R} propagation.
// 1.3b: SUP commutation on either port.

#include "../src/thvm.c"
#include "test.h"

static Term build_num(u32 v) {
  return term_new(0, TAG_NUM, 0, v);
}

static Term build_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
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
    // EQL(LAM, NUM) -- no commutation rule covers this, stays stuck.
    u64  lam_loc = heap_alloc(1);
    Term lam_var = term_new(0, TAG_VAR, 0, lam_loc);
    heap_set(lam_loc, lam_var);
    Term lam = term_new(0, TAG_LAM, 0, lam_loc);
    Term t   = term_new_eql(lam, build_num(5));
    Term r   = wnf(t);
    CHECK_EQ(term_tag(r), TAG_EQL);
  }

  TEST_BEGIN("eql/sup-on-left-commutes-head");
  {
    // EQL(&7{NUM(2), NUM(3)}, NUM(3)) -> SUP(7, ...) at head
    Term cand = build_sup(7, build_num(2), build_num(3));
    Term t    = term_new_eql(cand, build_num(3));
    Term r    = wnf(t);
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 7);
  }

  TEST_BEGIN("eql/sup-on-right-commutes-head");
  {
    Term cand = build_sup(7, build_num(2), build_num(3));
    Term t    = term_new_eql(build_num(3), cand);
    Term r    = wnf(t);
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 7);
  }

  TEST_BEGIN("eql/sup-on-left-collapses-to-zero-and-one");
  {
    // The stage-1 demo: SUP candidates, EQL against expected, collapse.
    // EQL(&7{NUM(2), NUM(3)}, NUM(3)) -> &7{NUM(0), NUM(1)} -> [0, 1]
    Term cand = build_sup(7, build_num(2), build_num(3));
    Term t    = term_new_eql(cand, build_num(3));
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 0);
    CHECK_EQ(term_tag(out[1]), TAG_NUM);
    CHECK_EQ(term_val(out[1]), 1);
  }

  TEST_BEGIN("eql/nested-sup-collapses-to-three-results");
  {
    // EQL(&7{1, &7{2, 3}}, NUM(2)) -> [0, 1, 0]
    Term inner = build_sup(7, build_num(2), build_num(3));
    Term cand  = build_sup(7, build_num(1), inner);
    Term t     = term_new_eql(cand, build_num(2));
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 3);
    CHECK_EQ(term_val(out[0]), 0);  // 1 == 2
    CHECK_EQ(term_val(out[1]), 1);  // 2 == 2
    CHECK_EQ(term_val(out[2]), 0);  // 3 == 2
  }

  thvm_free();
  TEST_REPORT();
}
