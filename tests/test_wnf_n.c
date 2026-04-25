// test_wnf_n.c - step-bounded reduction via wnf_n.
//
// Verifies:
//   - max_steps == 0 behaves like wnf (unbounded).
//   - max_steps == 1 fires exactly one interaction.
//   - resuming via wnf on the partially-reduced term completes
//     the reduction (heap mutations stick).
//   - WNF_LAST_STACK exposes pending eliminator frames.

#include "../src/thvm.c"
#include "test.h"

static Term build_id_lam(void) {
  u64  lam_loc = heap_alloc(1);
  Term var     = term_new(0, TAG_VAR, 0, lam_loc);
  heap_set(lam_loc, var);
  return term_new(0, TAG_LAM, 0, lam_loc);
}

static Term build_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("wnf_n/zero-is-unbounded");
  Term era  = term_new(0, TAG_ERA, 0, 0);
  Term id   = build_id_lam();
  Term app  = build_app(id, era);
  u64  itrs0 = ITRS;
  Term out  = wnf_n(app, 0);
  CHECK_EQ(term_tag(out), TAG_ERA);
  CHECK_EQ(ITRS - itrs0, 1);
  CHECK_EQ(WNF_LAST_STACK_LEN, 0);

  TEST_BEGIN("wnf_n/single-app-lam-completes-in-one-step");
  Term era2 = term_new(0, TAG_ERA, 0, 0);
  Term id2  = build_id_lam();
  Term app2 = build_app(id2, era2);
  itrs0 = ITRS;
  Term out2 = wnf_n(app2, 1);
  CHECK_EQ(term_tag(out2), TAG_ERA);
  CHECK_EQ(ITRS - itrs0, 1);
  CHECK_EQ(WNF_LAST_STACK_LEN, 0);

  TEST_BEGIN("wnf_n/two-stage-bails-after-one-then-resumes");
  // ((id era) era) - two APP frames; stepping one fires the inner APP-LAM
  // (substituting era into the binder) but the outer APP frame is still
  // pending: era is the resulting whnf, the outer frame stays on the
  // stack until the next call.
  Term era3 = term_new(0, TAG_ERA, 0, 0);
  Term era4 = term_new(0, TAG_ERA, 0, 0);
  Term id3  = build_id_lam();
  Term inner = build_app(id3, era3);
  Term outer = build_app(inner, era4);
  itrs0 = ITRS;
  Term mid = wnf_n(outer, 1);
  // After one APP-LAM the inner reduces to era; outer is now (era era).
  // The outer APP frame was popped in the unwind path so the returned
  // term IS the outer APP root.
  CHECK_EQ(ITRS - itrs0, 1);
  CHECK_EQ(term_tag(mid), TAG_APP);
  // Resuming with unbounded wnf must finish: APP-ERA fires.
  itrs0 = ITRS;
  Term done = wnf(mid);
  CHECK_EQ(term_tag(done), TAG_ERA);
  CHECK(ITRS - itrs0 >= 1);

  TEST_BEGIN("wnf_n/snapshot-captures-pending-app-frame");
  // Build (((id era) era) era) so reducing one step leaves two APP
  // frames pending after the innermost APP-LAM fires.
  Term era5 = term_new(0, TAG_ERA, 0, 0);
  Term era6 = term_new(0, TAG_ERA, 0, 0);
  Term era7 = term_new(0, TAG_ERA, 0, 0);
  Term id4  = build_id_lam();
  Term i1   = build_app(id4, era5);
  Term i2   = build_app(i1, era6);
  Term i3   = build_app(i2, era7);
  // Step until just before the first interaction: max_steps=1 fires
  // the innermost APP-LAM, then the limit kicks in BEFORE the outer
  // APP-ERA frames get processed in apply.  The unwind snapshots
  // those frames into WNF_LAST_STACK.
  (void)wnf_n(i3, 1);
  CHECK_EQ(WNF_LAST_STACK_LEN, 2);
  // Innermost APP frame is at index 0 (innermost-first ordering).
  CHECK_EQ(term_tag(WNF_LAST_STACK[0]), TAG_APP);
  CHECK_EQ(term_tag(WNF_LAST_STACK[1]), TAG_APP);

  thvm_free();
  TEST_REPORT();
}
