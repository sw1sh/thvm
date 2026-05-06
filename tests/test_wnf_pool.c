// test_wnf_pool.c -- WnfPool data-structure + drain semantics, with
// a stub fire function.  Real nf wiring lands in a later commit once
// the heap atomics + per-worker context are in.

#include "../src/thvm.c"
#include "test.h"

// Stub fire: pretend each Term value is a "countdown" counter.  Each
// fire decrements it; if still > 0, push it back.  Lets us verify the
// pop -> fire -> push loop without depending on real interactions.
static u64 fake_fire(WnfPool *p, u32 tid, Term r, void *user) {
  u64 *fires_seen = (u64 *)user;
  (*fires_seen)++;
  if (r > 1) wnf_pool_push(p, tid, (Term)(r - 1));
  return 0;
}

static int test_init_free(void) {
  TEST_BEGIN("wnf_pool_init / wnf_pool_free");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  CHECK(wnf_pool_init(p, 1) == 1);
  CHECK(p->n == 1);
  CHECK(p->workers[0].id == 0);
  // Bag capacity is a power-of-two; pool.c picks 2^18 = 256K slots
  // for nf-scale workloads (separate from collapse's WSPQ_CAP_POW2).
  CHECK(p->workers[0].bag.cap == 262144u);
  wnf_pool_free(p);
  free(p);
  return 0;
}

static int test_init_rejects_bad_n(void) {
  TEST_BEGIN("wnf_pool_init rejects n=0 and n>MAX_THREADS");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  CHECK(wnf_pool_init(p, 0) == 0);
  CHECK(wnf_pool_init(p, MAX_THREADS + 1) == 0);
  free(p);
  return 0;
}

static int test_push_pop_lifo(void) {
  TEST_BEGIN("push/pop preserves LIFO -- nf-compatible order");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  wnf_pool_init(p, 1);

  for (Term i = 1; i <= 5; i++) wnf_pool_push(p, 0, i);

  Term out = 0;
  for (Term expect = 5; expect >= 1; expect--) {
    CHECK(wnf_pool_pop_local(p, 0, &out) == 1);
    CHECK_EQ(out, expect);
  }
  CHECK(wnf_pool_pop_local(p, 0, &out) == 0);

  wnf_pool_free(p);
  free(p);
  return 0;
}

static int test_push_zero_dropped(void) {
  TEST_BEGIN("push of 0 is silently dropped (matches nf_work_push)");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  wnf_pool_init(p, 1);
  wnf_pool_push(p, 0, 0);
  Term out = 0;
  CHECK(wnf_pool_pop_local(p, 0, &out) == 0);
  wnf_pool_free(p);
  free(p);
  return 0;
}

static int test_drain_local(void) {
  TEST_BEGIN("wnf_pool_drain_local fires until empty");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  wnf_pool_init(p, 1);

  // Push a single Term=10 countdown -- should fire 10 times.
  wnf_pool_push(p, 0, 10);
  u64 fires_seen = 0;
  u64 fired = wnf_pool_drain_local(p, 0, fake_fire, &fires_seen,
                                   /*max_fires=*/100);
  CHECK_EQ(fired, 10);
  CHECK_EQ(fires_seen, 10);
  CHECK_EQ(p->workers[0].fires, 10);
  CHECK_EQ(atomic_load_explicit(&p->total_fires.v, memory_order_relaxed),
           10);

  Term out = 0;
  CHECK(wnf_pool_pop_local(p, 0, &out) == 0);   // drained

  wnf_pool_free(p);
  free(p);
  return 0;
}

static int test_drain_max_fires(void) {
  TEST_BEGIN("wnf_pool_drain_local respects max_fires");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  wnf_pool_init(p, 1);

  wnf_pool_push(p, 0, 50);
  u64 fires_seen = 0;
  u64 fired = wnf_pool_drain_local(p, 0, fake_fire, &fires_seen,
                                   /*max_fires=*/10);
  CHECK_EQ(fired, 10);
  CHECK_EQ(fires_seen, 10);

  // 50 - 10 = 40 should still be on the bag.
  Term out = 0;
  CHECK(wnf_pool_pop_local(p, 0, &out) == 1);
  CHECK_EQ(out, 40);

  wnf_pool_free(p);
  free(p);
  return 0;
}

static int test_steal_n2(void) {
  TEST_BEGIN("wnf_pool_steal moves work between workers");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  wnf_pool_init(p, 2);

  // Worker 1 has work; worker 0 steals.
  for (Term i = 1; i <= 3; i++) wnf_pool_push(p, 1, i);
  CHECK(wnf_pool_any_work(p, 0) == 1);

  Term out = 0;
  u32 cursor = 0;
  CHECK(wnf_pool_steal(p, 0, &out, &cursor) == 1);
  // Steal pulls from victim's top (FIFO) -> oldest first.
  CHECK_EQ(out, 1);
  CHECK_EQ(p->workers[0].steals, 1);

  wnf_pool_free(p);
  free(p);
  return 0;
}

static int test_merge_itrs(void) {
  TEST_BEGIN("wnf_pool_merge_itrs sums + resets per-worker deltas");
  WnfPool *p = (WnfPool *)calloc(1, sizeof(WnfPool));
  wnf_pool_init(p, 3);
  p->workers[0].itrs_delta = 5;
  p->workers[1].itrs_delta = 7;
  p->workers[2].itrs_delta = 11;
  CHECK_EQ(wnf_pool_merge_itrs(p), 23);
  CHECK_EQ(p->workers[0].itrs_delta, 0);
  CHECK_EQ(p->workers[1].itrs_delta, 0);
  CHECK_EQ(p->workers[2].itrs_delta, 0);
  wnf_pool_free(p);
  free(p);
  return 0;
}

int main(void) {
  test_init_free();
  test_init_rejects_bad_n();
  test_push_pop_lifo();
  test_push_zero_dropped();
  test_drain_local();
  test_drain_max_fires();
  test_steal_n2();
  test_merge_itrs();
  TEST_REPORT();
}
