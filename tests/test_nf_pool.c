// test_nf_pool.c -- nf rewired through WnfPool (T=1).
//
// The legacy `Term work[NF_WORK_CAP]` worklist is gone; nf now creates
// a 1-worker WnfPool, drains it, and routes redex_fire's locally-fresh
// pushes through wnf_pool_push.  This test exercises a few small
// reductions to confirm behaviour matches expectations -- the broader
// regression suite (`make test`) covers the rest.

#include "../src/thvm.c"
#include "test.h"

static int test_nf_app_lam(void) {
  TEST_BEGIN("nf via pool: APP(lam x. ERA, ERA) -> ERA");
  thvm_init();

  // Body is ERA, no VAR -- result is exactly ERA after one APP-LAM
  // (no SUB-resolve step needed; nf only fires interactions, it
  // doesn't rewrite VAR -> SUB cells -- that's cnf's job).
  u64  lam_loc = heap_alloc(1);
  heap_set(lam_loc, term_new(0, TAG_ERA, 0, 0));
  Term lam     = term_new(0, TAG_LAM, 0, lam_loc);

  Term era     = term_new(0, TAG_ERA, 0, 0);
  u64  app_loc = heap_alloc(2);
  heap_set(app_loc + 0, lam);
  heap_set(app_loc + 1, era);
  Term app     = term_new(0, TAG_APP, 0, app_loc);

  u64 itrs0 = ITRS;
  Term out  = nf(app);
  CHECK_EQ(term_tag(out), TAG_ERA);
  CHECK_EQ(ITRS - itrs0, 1u);   // one APP-LAM fire

  thvm_free();
  return 0;
}

static int test_nf_op2_chain(void) {
  TEST_BEGIN("nf via pool: nested OP2 collapses to NUM");
  thvm_init();

  // (1 + 2) + 3 = 6
  u64 add_inner = heap_alloc(2);
  heap_set(add_inner + 0, term_new(0, TAG_NUM, DT_INT32, 1));
  heap_set(add_inner + 1, term_new(0, TAG_NUM, DT_INT32, 2));
  Term inner = term_new(0, TAG_OP2, OP_ADD, add_inner);

  u64 add_outer = heap_alloc(2);
  heap_set(add_outer + 0, inner);
  heap_set(add_outer + 1, term_new(0, TAG_NUM, DT_INT32, 3));
  Term outer = term_new(0, TAG_OP2, OP_ADD, add_outer);

  u64 itrs0 = ITRS;
  Term out  = nf(outer);
  CHECK_EQ(term_tag(out), TAG_NUM);
  CHECK_EQ(term_val(out), 6u);
  CHECK_EQ(ITRS - itrs0, 2u);   // two OP2 fires

  thvm_free();
  return 0;
}

static int test_nf_idempotent(void) {
  TEST_BEGIN("nf via pool: nf(nf(t)) == nf(t), no extra ITRS");
  thvm_init();

  // Reduce something simple, then re-nf and confirm no work happens.
  u64 add = heap_alloc(2);
  heap_set(add + 0, term_new(0, TAG_NUM, DT_INT32, 7));
  heap_set(add + 1, term_new(0, TAG_NUM, DT_INT32, 8));
  Term t = term_new(0, TAG_OP2, OP_ADD, add);

  Term once  = nf(t);
  u64  itrs1 = ITRS;
  Term twice = nf(once);
  CHECK_EQ(ITRS, itrs1);              // no further interactions
  CHECK_EQ(term_tag(twice), TAG_NUM);
  CHECK_EQ(term_val(twice), 15u);

  thvm_free();
  return 0;
}

static int test_nf_pool_lifecycle(void) {
  TEST_BEGIN("redex_worklist_attach_pool / detach hygiene");
  thvm_init();

  // Verify nf cleanly detaches: after a run, a manual buffer-mode
  // attach should still work (the pool slot is cleared).
  u64 add = heap_alloc(2);
  heap_set(add + 0, term_new(0, TAG_NUM, DT_INT32, 2));
  heap_set(add + 1, term_new(0, TAG_NUM, DT_INT32, 3));
  Term t = term_new(0, TAG_OP2, OP_ADD, add);
  (void)nf(t);

  // Now use the legacy attach path -- it should not see any stale
  // pool pointer and should accept pushes into a buffer.
  Term buf[16] = {0};
  u32  n = 0;
  redex_worklist_attach(buf, &n, 16);
  // Build a small redex (lam+era app) and push it manually -- the
  // push must go to buf, not the (now-freed) pool.
  u64  lam_loc = heap_alloc(1);
  heap_set(lam_loc, term_new(0, TAG_VAR, 0, lam_loc));
  Term lam     = term_new(0, TAG_LAM, 0, lam_loc);
  Term era     = term_new(0, TAG_ERA, 0, 0);
  u64  app_loc = heap_alloc(2);
  heap_set(app_loc + 0, lam);
  heap_set(app_loc + 1, era);
  Term app     = term_new(0, TAG_APP, 0, app_loc);
  nf_work_push(app);
  CHECK_EQ(n, 1u);
  CHECK_EQ(buf[0], app);
  redex_worklist_detach();

  thvm_free();
  return 0;
}

int main(void) {
  test_nf_app_lam();
  test_nf_op2_chain();
  test_nf_idempotent();
  test_nf_pool_lifecycle();
  TEST_REPORT();
}
