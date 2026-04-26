// test_and_or.c - short-circuit boolean AND / OR with SUP commutation.
//
// AND short-circuits to 0 on false; OR short-circuits to 1 on true.
// ERA on either input port (after the wnf of the strict side) yields
// ERA.  SUP commutes through.

#include "../src/thvm.c"
#include "test.h"

static Term build_num(u32 v) { return term_new(0, TAG_NUM, 0, v); }

static Term build_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

int main(void) {
  thvm_init();

  // === AND ============================================================

  TEST_BEGIN("and/zero-short-circuits");
  {
    // AND(NUM(0), NUM(99)) -> NUM(0); the right side never reduces.
    Term t = term_new_and(build_num(0), build_num(99));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 0);
  }

  TEST_BEGIN("and/nonzero-passes-through-to-b");
  {
    Term t = term_new_and(build_num(1), build_num(42));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 42);
  }

  TEST_BEGIN("and/era-on-left-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term t   = term_new_and(era, build_num(7));
    Term r   = wnf(t);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  TEST_BEGIN("and/sup-on-left-commutes");
  {
    // AND(&7{NUM(0), NUM(1)}, NUM(42)) -> &7{NUM(0), NUM(42)}
    // Left branch: AND(0, 42) -> 0
    // Right branch: AND(1, 42) -> 42
    Term sup = build_sup(7, build_num(0), build_num(1));
    Term t   = term_new_and(sup, build_num(42));
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 0);
    CHECK_EQ(term_val(out[1]), 42);
  }

  TEST_BEGIN("and/eql-filter-pattern");
  {
    // The "collapse to the matching one" demo, partial: AND(EQL(c,e), c)
    // For a non-matching candidate, AND(0, c) -> 0.
    // For the matching candidate, AND(1, c) -> c (unevaluated).
    // After collapse: [NUM(0), NUM(3)]  -- the failed slot is 0, the
    // matching slot is the candidate itself.
    Term cands = build_sup(7, build_num(2), build_num(3));
    Term cands_dup_root = heap_alloc(1);
    heap_set(cands_dup_root, cands);
    Term cands_l = term_new(0, TAG_DP0, 7, cands_dup_root);
    Term cands_r = term_new(0, TAG_DP1, 7, cands_dup_root);
    Term cands_sup = build_sup(7, cands_l, cands_r);

    Term cands2 = build_sup(7, build_num(2), build_num(3));
    Term tested = term_new_eql(cands2, build_num(3));
    Term t = term_new_and(tested, cands_sup);

    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 0);  // 2 != 3 -> AND(0, 2) -> 0
    CHECK_EQ(term_val(out[1]), 3);  // 3 == 3 -> AND(1, 3) -> 3
  }

  // === OR =============================================================

  TEST_BEGIN("or/nonzero-short-circuits");
  {
    Term t = term_new_or(build_num(7), build_num(99));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 1);
  }

  TEST_BEGIN("or/zero-passes-through-to-b");
  {
    Term t = term_new_or(build_num(0), build_num(42));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 42);
  }

  TEST_BEGIN("or/era-on-left-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term t   = term_new_or(era, build_num(7));
    Term r   = wnf(t);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  TEST_BEGIN("or/sup-on-left-commutes");
  {
    // OR(&7{NUM(0), NUM(1)}, NUM(42)) -> &7{NUM(42), NUM(1)}
    Term sup = build_sup(7, build_num(0), build_num(1));
    Term t   = term_new_or(sup, build_num(42));
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(term_val(out[0]), 42);  // OR(0, 42) -> 42
    CHECK_EQ(term_val(out[1]), 1);   // OR(1, _) -> 1
  }

  thvm_free();
  TEST_REPORT();
}
