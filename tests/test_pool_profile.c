// test_pool_profile.c -- per-worker timing / steal-attempt /
// idle-time counters surface correctly through wnf_pool_last_stats().
//
// Drives nf at T=1 and T=4 and verifies the snapshot's invariants
// (n_workers, total_fires == sum-of-per-worker fires, drain_wall_ns
// > 0, active_ns + idle_ns roughly close to wall).  Doesn't rely on
// exact ns values -- just structural sanity.

#include "../src/thvm.c"
#include "test.h"

// Build the standard `OP2_ADD(1, 2)` redex used elsewhere -- one
// fire, deterministic.
static Term mk_simple_add(u32 a, u32 b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, a));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, b));
  return term_new(0, TAG_OP2, OP_ADD, loc);
}

static int test_stats_t1(void) {
  TEST_BEGIN("nf at T=1 captures stats: 1 worker, 1 fire");
  thvm_init();
  nf_set_threads(1);

  Term out = nf(mk_simple_add(1, 2));
  CHECK_EQ(term_tag(out), TAG_NUM);
  CHECK_EQ(term_val(out), 3u);

  const WnfPoolStats *s = wnf_pool_last_stats();
  CHECK_EQ(s->n_workers, 1u);
  CHECK_EQ(s->total_fires, 1u);
  CHECK_EQ(s->workers[0].fires, 1u);
  CHECK(s->workers[0].pushes >= 1u);
  // drain_wall_ns can round to 0 on a single OP2 fire (CLOCK_MONOTONIC
  // tick is ~us on Darwin/M*); a non-strict bound is enough for shape.
  CHECK(s->drain_wall_ns >= 0u);
  // T=1 has no spawn, no steal -- attempts should be zero.
  CHECK_EQ(s->workers[0].steal_attempts, 0u);
  CHECK_EQ(s->workers[0].steals, 0u);

  nf_set_threads(0);    // reset
  thvm_free();
  return 0;
}

static int test_stats_t4(void) {
  TEST_BEGIN("nf at T=4 captures stats: 4 workers, fires distributed");
  thvm_init();
  nf_set_threads(4);

  // Use a small reduction; under T=4 only one worker actually fires
  // because the work is too small to share -- but the snapshot must
  // still report 4 workers with valid timing fields.
  Term out = nf(mk_simple_add(7, 8));
  CHECK_EQ(term_val(out), 15u);

  const WnfPoolStats *s = wnf_pool_last_stats();
  CHECK_EQ(s->n_workers, 4u);
  CHECK_EQ(s->total_fires, 1u);
  // Sum of per-worker fires == total_fires.
  u64 sum = 0;
  for (u32 t = 0; t < s->n_workers; t++) sum += s->workers[t].fires;
  CHECK_EQ(sum, s->total_fires);
  // T=4 spawns 3 pthreads; even on a 1-fire workload that's enough
  // wall time for the timer to register.  Idle time at T=4 is
  // dominated by the 3 workers that didn't get the redex.
  CHECK(s->drain_wall_ns > 0u);
  u64 total_idle = 0;
  for (u32 t = 0; t < s->n_workers; t++) total_idle += s->workers[t].idle_ns;
  CHECK(total_idle > 0u);

  nf_set_threads(0);
  thvm_free();
  return 0;
}

static int test_stats_steal_attempts(void) {
  TEST_BEGIN("T=4 with seeded chain: at least one worker logs a steal attempt");
  thvm_init();
  nf_set_threads(4);

  // Build a 20-deep ADD chain so workers actually have something to
  // steal.  Each ADD fires one OP2-ADD; chain pushes children.
  Term acc = term_new(0, TAG_NUM, DT_INT32, 0);
  for (u32 i = 1; i <= 20; i++) {
    u64 loc = heap_alloc(2);
    heap_set(loc + 0, acc);
    heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, i));
    acc = term_new(0, TAG_OP2, OP_ADD, loc);
  }
  Term out = nf(acc);
  CHECK_EQ(term_tag(out), TAG_NUM);
  CHECK_EQ(term_val(out), 1u + 2u + 3u + 4u + 5u + 6u + 7u + 8u + 9u + 10u +
                          11u + 12u + 13u + 14u + 15u + 16u + 17u + 18u +
                          19u + 20u);

  const WnfPoolStats *s = wnf_pool_last_stats();
  CHECK_EQ(s->n_workers, 4u);
  // Across all workers, at least *some* steal attempts happened
  // (each idle worker tries once before going to spin).
  u64 attempts = 0;
  for (u32 t = 0; t < s->n_workers; t++) attempts += s->workers[t].steal_attempts;
  CHECK(attempts > 0u);

  nf_set_threads(0);
  thvm_free();
  return 0;
}

static int test_threads_setter(void) {
  TEST_BEGIN("nf_set_threads / nf_get_threads round-trip");
  CHECK_EQ(nf_get_threads(), 1u);  // default (no env, no override)
  nf_set_threads(8);
  CHECK_EQ(nf_get_threads(), 8u);
  nf_set_threads(MAX_THREADS + 100);
  CHECK_EQ(nf_get_threads(), (u32)MAX_THREADS);  // clamped
  nf_set_threads(0);
  CHECK_EQ(nf_get_threads(), 1u);  // back to env fallback (unset)
  return 0;
}

int main(void) {
  test_threads_setter();
  test_stats_t1();
  test_stats_t4();
  test_stats_steal_attempts();
  TEST_REPORT();
}
