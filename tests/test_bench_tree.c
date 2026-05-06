// test_bench_tree.c -- Bend-style parallel-tree throughput.  Runs at
// T=1 and T=8 across several depths to map the T scaling curve.
// Builds in C (no WL setup overhead) so we can push past depth 14
// without WL-side memory issues.  Timing harness, not a correctness
// test -- though each run does verify the result is correct.

#include "../src/thvm.c"
#include "test.h"
#include <time.h>

static Term build_add_tree(u32 depth) {
  u32 n = 1u << depth;
  Term *level = (Term *)malloc(n * sizeof(Term));
  for (u32 i = 0; i < n; i++) level[i] = term_new(0, TAG_NUM, DT_INT32, 1);
  while (n > 1) {
    u32 m = n / 2;
    for (u32 i = 0; i < m; i++) {
      u64 loc = heap_alloc(2);
      heap_set(loc + 0, level[2*i]);
      heap_set(loc + 1, level[2*i + 1]);
      level[i] = term_new(0, TAG_OP2, OP_ADD, loc);
    }
    n = m;
  }
  Term root = level[0];
  free(level);
  return root;
}

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void bench_at(u32 depth) {
  u32 expected = 1u << depth;

  thvm_init();
  nf_set_threads(1);
  Term root = build_add_tree(depth);
  double t0 = now_s();
  Term r1 = nf(root);
  double t1 = now_s() - t0;
  u32 r1_correct = (term_tag(r1) == TAG_NUM) && (term_val(r1) == expected);
  u32 t1_rounds = wnf_pool_last_stats()->drain_rounds;
  thvm_free();

  thvm_init();
  nf_set_threads(8);
  root = build_add_tree(depth);
  t0 = now_s();
  Term r8 = nf(root);
  double t8 = now_s() - t0;
  u32 r8_correct = (term_tag(r8) == TAG_NUM) && (term_val(r8) == expected);
  u32 t8_rounds = wnf_pool_last_stats()->drain_rounds;
  thvm_free();

  printf("d=%2u  fires=%9u  T=1: %8.2fms (%2u rounds, val=%lu/%u)  "
         "T=8: %8.2fms (%2u rounds, val=%lu/%u, %.2fx)\n",
         depth, expected - 1,
         t1 * 1000.0, t1_rounds, (unsigned long)term_val(r1), expected,
         t8 * 1000.0, t8_rounds, (unsigned long)term_val(r8), expected,
         t1 / t8);
  (void)r1_correct; (void)r8_correct;
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("Bend-style balanced ADD tree (identical leaves): T=1 vs T=8\n");
  printf("===========================================================\n");
  bench_at(12);
  bench_at(14);
  bench_at(16);
  bench_at(18);
  TEST_BEGIN("bench_tree -- timing harness, no asserts");
  TEST_REPORT();
}
