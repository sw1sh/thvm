// test_when.c - boolean filter (TAG_WHEN).
//
// WHEN(NUM(0), _)        -> ERA
// WHEN(NUM(n != 0), b)   -> wnf(b)
// WHEN(ERA, _)           -> ERA
// WHEN(&L{c0,c1}, b)     -> &L{WHEN(c0, B0), WHEN(c1, B1)}, !&L{B0,B1}=b

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

  TEST_BEGIN("when/zero-cond-yields-era");
  {
    Term t = term_new_when(build_num(0), build_num(42));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  TEST_BEGIN("when/nonzero-cond-yields-body");
  {
    Term t = term_new_when(build_num(7), build_num(42));
    Term r = wnf(t);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 42);
  }

  TEST_BEGIN("when/era-cond-yields-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term t   = term_new_when(era, build_num(42));
    Term r   = wnf(t);
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  TEST_BEGIN("when/sup-cond-commutes");
  {
    // WHEN(&7{NUM(0), NUM(1)}, NUM(42)) -> &7{ERA, NUM(42)}
    // After collapse: ERA branch dropped, only NUM(42) remains.
    Term cond = build_sup(7, build_num(0), build_num(1));
    Term t    = term_new_when(cond, build_num(42));
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 1);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 42);
  }

  TEST_BEGIN("when/eql-filter-collapses-to-matching-only");
  {
    // The full stage-1 demo: SUP candidates, EQL against expected,
    // WHEN-filter, collapse -> only the matching candidate.
    //
    //   cands = &L{NUM(2), NUM(3)}
    //   t     = WHEN(EQL(cands, NUM(3)), cands)
    //
    // Expected: collapse -> [NUM(3)] (NUM(2) branch fails -> ERA -> dropped).
    //
    // We build two independent &L{NUM(2), NUM(3)} for the cond and body
    // since they need to be DUP-able through the EQL/WHEN commutations.
    Term cond_cands = build_sup(7, build_num(2), build_num(3));
    Term body_cands = build_sup(7, build_num(2), build_num(3));
    Term cond = term_new_eql(cond_cands, build_num(3));
    Term t    = term_new_when(cond, body_cands);
    Term out[4] = {0};
    u64  n = thvm_collapse(t, out, 4);
    CHECK_EQ(n, 1);
    CHECK_EQ(term_tag(out[0]), TAG_NUM);
    CHECK_EQ(term_val(out[0]), 3);
  }

  thvm_free();
  TEST_REPORT();
}
