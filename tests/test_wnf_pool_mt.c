// test_wnf_pool_mt.c -- pthread-based parallel drain (spawn + steal +
// termination handshake).
//
// Validates wnf_pool_drain_parallel with a stub fire function that
// hammers heap allocations + ITRS bumps.  No real interactions yet
// (nf wiring lands in a follow-up); this test proves the spawn path,
// per-worker WNF state routing, and termination logic are sound.

#include "../src/thvm.c"
#include "test.h"

// Countdown fire: r is a Term whose val field encodes a u32 counter.
// Each fire decrements the counter; if still > 0, push the new term
// back so some worker eventually consumes it.  Each fire bumps ITRS
// and allocates a heap cell to exercise the parallel-safe allocator.
static u64 mt_countdown_fire(WnfPool *p, u32 tid, Term r, void *user) {
  (void)user;
  (void)tid;
  u32 v = (u32)term_val(r);
  ITRS++;
  // Burn a heap cell every fire to exercise atomic alloc.
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, DT_INT32, v));
  if (v > 1) wnf_pool_push(p, tid, term_new(0, TAG_NUM, DT_INT32, v - 1));
  return 0;
}

static int test_drain_parallel_n2(void) {
  TEST_BEGIN("wnf_pool_drain_parallel n=2 -- spawn + termination");
  thvm_init();
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  CHECK(wnf_pool_init(p, 2) == 1);

  // Seed: a single Term=200.  After ~200 fires (across both workers)
  // the bag drains and termination fires.
  wnf_pool_push(p, 0, term_new(0, TAG_NUM, DT_INT32, 200));
  u64 itrs0 = ITRS;
  u64 fired = wnf_pool_drain_parallel(p, mt_countdown_fire, NULL);

  CHECK_EQ(fired, 200u);
  CHECK_EQ(ITRS - itrs0, 200u);
  // Both workers got at least one fire (under random work-stealing,
  // worker 0 should fire more since it gets the seed; worker 1 may
  // get a few via steal).  Just check totals are consistent.
  u64 sum = p->workers[0].fires + p->workers[1].fires;
  CHECK_EQ(sum, 200u);

  wnf_pool_free(p);
  free(p);
  thvm_free();
  return 0;
}

static int test_drain_parallel_n4(void) {
  TEST_BEGIN("wnf_pool_drain_parallel n=4 -- pre-seeded across workers");
  thvm_init();
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  CHECK(wnf_pool_init(p, 4) == 1);

  // Seed every worker so each has work without needing to steal.
  // Each worker gets a Term=50 chain -> 50 fires per worker = 200 total.
  for (u32 t = 0; t < 4; t++) {
    wnf_pool_push(p, t, term_new(0, TAG_NUM, DT_INT32, 50));
  }
  u64 itrs0 = ITRS;
  u64 fired = wnf_pool_drain_parallel(p, mt_countdown_fire, NULL);

  CHECK_EQ(fired, 200u);
  CHECK_EQ(ITRS - itrs0, 200u);
  CHECK(p->workers[0].fires + p->workers[1].fires +
        p->workers[2].fires + p->workers[3].fires == 200u);

  wnf_pool_free(p);
  free(p);
  thvm_free();
  return 0;
}

static int test_drain_parallel_steal_required(void) {
  TEST_BEGIN("wnf_pool_drain_parallel n=4 -- only worker 0 seeded");
  thvm_init();
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  CHECK(wnf_pool_init(p, 4) == 1);

  // All work starts in worker 0's bag.  Persistent-worker wakeups are
  // fast enough that worker 0 often drains the bag before peers can
  // grab anything, so we can't reliably assert steals > 0 -- but
  // steal_attempts > 0 confirms the steal-from-peer code path runs
  // on the idle workers.
  wnf_pool_push(p, 0, term_new(0, TAG_NUM, DT_INT32, 400));
  u64 fired = wnf_pool_drain_parallel(p, mt_countdown_fire, NULL);
  CHECK_EQ(fired, 400u);
  u32 attempted = 0;
  for (u32 t = 1; t < 4; t++) {
    if (p->workers[t].steal_attempts > 0) attempted++;
  }
  CHECK(attempted >= 1);

  wnf_pool_free(p);
  free(p);
  thvm_free();
  return 0;
}

static int test_drain_parallel_n1_fastpath(void) {
  TEST_BEGIN("wnf_pool_drain_parallel n=1 -- skip pthread overhead");
  thvm_init();
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  CHECK(wnf_pool_init(p, 1) == 1);

  wnf_pool_push(p, 0, term_new(0, TAG_NUM, DT_INT32, 100));
  u64 fired = wnf_pool_drain_parallel(p, mt_countdown_fire, NULL);
  CHECK_EQ(fired, 100u);
  CHECK_EQ(p->workers[0].fires, 100u);

  wnf_pool_free(p);
  free(p);
  thvm_free();
  return 0;
}

int main(void) {
  test_drain_parallel_n1_fastpath();
  test_drain_parallel_n2();
  test_drain_parallel_n4();
  test_drain_parallel_steal_required();
  TEST_REPORT();
}
