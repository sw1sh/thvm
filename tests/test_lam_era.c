// test_lam_era.c -- LAM_ERA_MASK construction + APP-LAM / DUP-LAM
// fast-path checks.
//
// Three cases:
//   a) (λx.x) y                -- binder used; mask OFF; ITRS == 1.
//   b) (λx.42) y               -- binder unused; mask ON; ITRS == 1;
//                                 the binder cell is never written
//                                 with a SUB substitution.
//   c) dup {a b} = λx.42 in a  -- DUP-LAM produces fresh LAMs, at
//                                 least one of which has the mask.

#include "../src/thvm.c"
#include "test.h"

// (λx.x) -- binder used.
static Term build_id_lam(void) {
  u64  lam_loc = heap_alloc(1);
  Term var     = term_new(0, TAG_VAR, 0, lam_loc);
  heap_set(lam_loc, var);
  // Use lam_seal_ext to mirror what every construction site does.
  return term_new(0, TAG_LAM, lam_seal_ext(lam_loc, 0), lam_loc);
}

// (λx.NUM(n)) -- binder unused.
static Term build_const_lam(u32 n, u64 *out_lam_loc) {
  u64  lam_loc = heap_alloc(1);
  Term num     = term_new(0, TAG_NUM, 0, n);
  heap_set(lam_loc, num);
  if (out_lam_loc) *out_lam_loc = lam_loc;
  return term_new(0, TAG_LAM, lam_seal_ext(lam_loc, 0), lam_loc);
}

// APP(f, x) -- 2-cell allocation.
static Term build_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
  thvm_init();

  // (a) binder used: the mask must NOT be set, ITRS == 1, result is y.
  TEST_BEGIN("lam-era/binder-used-mask-off");
  {
    Term id     = build_id_lam();
    CHECK_EQ(term_ext(id) & LAM_ERA_MASK, 0);
    Term era    = term_new(0, TAG_ERA, 0, 0);    // y is ERA here
    Term app    = build_app(id, era);
    u64  before = ITRS;
    Term out    = wnf(app);
    CHECK_EQ(ITRS - before, 1);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  // (b) binder unused: the mask must be set, ITRS == 1, result is the
  // literal 42, and the binder heap cell is never SUB-substituted.
  TEST_BEGIN("lam-era/binder-unused-mask-on");
  {
    u64  lam_loc;
    Term k42     = build_const_lam(42, &lam_loc);
    CHECK(term_ext(k42) & LAM_ERA_MASK);
    Term cell_before = heap_read(lam_loc);
    CHECK_EQ(term_sub_get(cell_before), 0);

    Term era    = term_new(0, TAG_ERA, 0, 0);
    Term app    = build_app(k42, era);
    u64  before = ITRS;
    Term out    = wnf(app);
    CHECK_EQ(ITRS - before, 1);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ(term_val(out), 42);

    // The binder cell at lam_loc must remain unchanged (still
    // TAG_NUM 42, sub flag still 0) -- the fast-path skipped
    // heap_subst_var, so no SUB write happened.
    Term cell_after = heap_read(lam_loc);
    CHECK_EQ(term_sub_get(cell_after), 0);
    CHECK_EQ(term_tag(cell_after), TAG_NUM);
    CHECK_EQ(term_val(cell_after), 42);
  }

  // (c) DUP-LAM on a binder-unused lambda: the two new LAMs must each
  // come back with LAM_ERA_MASK set (their bodies inherit the same
  // unused-binder property).
  TEST_BEGIN("lam-era/dup-lam-propagates-mask");
  {
    u64  lam_loc;
    Term k42     = build_const_lam(7, &lam_loc);
    CHECK(term_ext(k42) & LAM_ERA_MASK);

    // ! &L{F0, F1} = (λx.7);  apply F0 to ERA -> 7.
    u64  dup_loc = heap_alloc(1);
    heap_set(dup_loc, k42);
    Term dp0     = term_new(0, TAG_DP0, 13, dup_loc);

    // Fire the DUP-LAM by entering DP0; the WHNF result is the new
    // LAM that DP0 was rewritten to.  Inspect it directly before any
    // beta strips it.
    u64  before = ITRS;
    Term new_lam = wnf(dp0);
    CHECK_EQ(ITRS - before, 1);
    CHECK_EQ(term_tag(new_lam), TAG_LAM);
    CHECK(term_ext(new_lam) & LAM_ERA_MASK);

    // Apply the new lambda to ERA -- since the binder is unused, the
    // result must be the duplicated body.  The body is DP0/DP1 of the
    // shared dup-cell, which on the active side resolves to NUM(7).
    Term era     = term_new(0, TAG_ERA, 0, 0);
    Term app     = build_app(new_lam, era);
    Term out     = wnf(app);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ(term_val(out), 7);
  }

  thvm_free();
  TEST_REPORT();
}
