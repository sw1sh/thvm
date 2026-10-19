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
  TEST_BEGIN("nf detaches its pool when it returns");
  thvm_init();
  u64 add = heap_alloc(2);
  heap_set(add + 0, term_new(0, TAG_NUM, DT_INT32, 2));
  heap_set(add + 1, term_new(0, TAG_NUM, DT_INT32, 3));
  Term t = term_new(0, TAG_OP2, OP_ADD, add);
  (void)nf(t);
  CHECK(NF_POOL == 0);
  thvm_free();
  return 0;
}

static Term nf_test_add(u32 a, u32 b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, a));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, b));
  return term_new(0, TAG_OP2, OP_ADD, loc);
}

// A step session the WL stepper opened is not nf's: nf leaves it attached.
static int test_nf_keeps_a_session_it_did_not_open(void) {
  TEST_BEGIN("nf leaves a session it did not open attached");
  thvm_init();
  Term held = nf_test_add(2, 3);
  (void)redex_step_attach(&held, 1);
  CHECK(STEP_ACTIVE);
  Term out = nf(nf_test_add(4, 5));
  CHECK_EQ(term_tag(out), TAG_NUM);
  CHECK_EQ(term_val(out), 9u);
  CHECK(STEP_ACTIVE);
  redex_step_detach();
  thvm_free();
  return 0;
}

// More redexes than the bag's 2^18 seed slots: the ones past the clamp fire
// too, from a re-enumeration.
static int test_nf_past_the_seed_cap(void) {
  TEST_BEGIN("nf fires every redex past the seed cap");
  thvm_init();
  nf_set_threads(1);
  u32   n    = 300000;
  Term *kids = (Term *)malloc(n * sizeof(Term));
  for (u32 i = 0; i < n; i++) kids[i] = nf_test_add(i, 1);
  Term out = nf(term_new_ctr(0, kids, n));
  free(kids);
  int all = 1;
  for (u32 i = 0; i < n && all; i++) {
    Term c = term_resolve(term_ctr_at(out, i));
    all = term_tag(c) == TAG_NUM && term_val(c) == i + 1;
  }
  CHECK(all);
  nf_set_threads(0);
  thvm_free();
  return 0;
}

// nf reduces its own term's redexes only: a KERNEL an earlier realize left in
// some other heap cell, naming a kernel the kernel GC stripped, is not fired
// (firing it outside a realize is fatal).
static int test_nf_leaves_other_terms_redexes(void) {
  TEST_BEGIN("nf fires only its own term's redexes");
  thvm_init();
  u32  kid = kernel_alloc();
  KERNELS[kid].stripped = 1;
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = 2;
  u64 kl = heap_alloc(2);
  heap_set(kl + 0, term_new(0, TAG_TEN, DT_FP32, tensor_alloc(&CPU_BACKEND, s, DT_FP32)));
  heap_set(kl + 1, term_new(0, TAG_NUM, 0, kid));
  u64 holder = heap_alloc(1);
  heap_set(holder, term_new(0, TAG_UOP, UOP_KERNEL, kl));
  u64  itrs0 = ITRS;
  Term out   = nf(nf_test_add(2, 3));
  CHECK_EQ(term_tag(out), TAG_NUM);
  CHECK_EQ(term_val(out), 5u);
  CHECK_EQ(ITRS - itrs0, 1u);
  thvm_free();
  return 0;
}

int main(void) {
  test_nf_app_lam();
  test_nf_op2_chain();
  test_nf_idempotent();
  test_nf_pool_lifecycle();
  test_nf_keeps_a_session_it_did_not_open();
  test_nf_past_the_seed_cap();
  test_nf_leaves_other_terms_redexes();
  TEST_REPORT();
}
