// tests/test_bend_tree_sum.c
//
// Phase 1 verification: hand-coded equivalent of the Bend2 reference
// program at TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c.
//
//   type Tree { leaf{U32}, node{Tree, Tree} }
//   def build(d, x): if d==0 then leaf{x}
//                    else node{build(d-1, x*2), build(d-1, x*2+1)}
//   def sum(t):      if leaf{v} then v
//                    else let l=sum(t.l), r=sum(t.r) in l + r
//   def main(d):     sum(build(d, 0))
//
// Result for build(d, 0) sum = (2^d - 1) * 2^d / 2  (sum of leaves
// 0..2^d-1).  Verified at depths 4 and 8 below.
//
// This file ALSO doubles as the runnable T=1 vs T=N timing harness
// once depth gets large enough to be benchmarkable.  Set the env
// var BEND_TREE_DEPTH (defaults to 4, the small-correctness depth)
// to override -- bigger numbers exercise the parallel path.
//
// What this verifies for the runtime:
//   - dispatch table + AotResult routing (R_VALUE / R_SPLIT / R_CALL)
//   - cont cell alloc + write_slot OR-merge of tag bits
//   - resolve loop: walk values up the cont tree, fire conts
//   - serial runner LIFO stack handling
//   - parallel runner (only when AOT_THREADS>=2 in env at run time)
//   - cross-fn cont chaining (FN_MAIN -> CONT_MAIN -> FN_SUM)

#include "../src/thvm.c"
#include "test.h"
#include <time.h>

#define LBL_LEAF  1
#define LBL_NODE  2

#define FN_BUILD     0
#define FN_SUM       1
#define FN_MAIN      2
#define CONT_BUILD   3
#define CONT_SUM     4
#define CONT_MAIN    5

// === Term helpers ====================================================
//
// Routes all CTR construction through the AOT thread-local bump
// allocator (aot_make_ctrN) so we don't pay the global heap_next
// atomic contention on every leaf/node.  See src/aot/halloc.h.

static Term make_num(u32 v) {
  return aot_make_num_i32(v);
}

static Term make_leaf(Term v) {
  return aot_make_ctr1(LBL_LEAF, v);
}

static Term make_node(Term l, Term r) {
  return aot_make_ctr2(LBL_NODE, l, r);
}

// === Dispatch ========================================================

static AotResult my_dispatch(AotProgram *p, AotTask *t);

static AotResult fn_build(u32 ret, Term *args) {
  u32 d = (u32)term_val(args[0]);
  u32 x = (u32)term_val(args[1]);
  if (d == 0) {
    return aot_make_value(make_leaf(make_num(x)));
  }
  u64 dp = aot_alloc_cont(CONT_BUILD, 0, ret);
  Term d1 = make_num(d - 1);
  return aot_make_split(
    aot_make_task(FN_BUILD, aot_enc_ret((u32)dp, 0),
                  d1, make_num(x * 2),     0, 0),
    aot_make_task(FN_BUILD, aot_enc_ret((u32)dp, 1),
                  d1, make_num(x * 2 + 1), 0, 0));
}

static AotResult cont_build(u32 ret, Term *args) {
  (void)ret;
  return aot_make_value(make_node(args[0], args[1]));
}

static AotResult fn_sum(u32 ret, Term *args) {
  Term t = args[0];
  if (term_tag(t) == TAG_CTR && term_ext(t) == LBL_LEAF) {
    return aot_make_value(term_ctr_at(t, 0));
  }
  if (term_tag(t) == TAG_CTR && term_ext(t) == LBL_NODE) {
    u64 dp = aot_alloc_cont(CONT_SUM, 0, ret);
    return aot_make_split(
      aot_make_task(FN_SUM, aot_enc_ret((u32)dp, 0),
                    term_ctr_at(t, 0), 0, 0, 0),
      aot_make_task(FN_SUM, aot_enc_ret((u32)dp, 1),
                    term_ctr_at(t, 1), 0, 0, 0));
  }
  return aot_make_value(make_num(0));   // ERA fallback
}

static AotResult cont_sum(u32 ret, Term *args) {
  (void)ret;
  u32 a = (u32)term_val(args[0]);
  u32 b = (u32)term_val(args[1]);
  return aot_make_value(make_num(a + b));
}

// FN_MAIN: the program's entry point.  Tail-call into FN_BUILD,
// then the cont threads the built tree into FN_SUM.
static AotResult fn_main(u32 ret, Term *args) {
  u32 d = (u32)term_val(args[0]);
  u64 dp = aot_alloc_cont_call(CONT_MAIN, 0, ret);
  return aot_make_call(
    aot_make_task(FN_BUILD, aot_enc_ret((u32)dp, 0),
                  make_num(d), make_num(0), 0, 0));
}

static AotResult cont_main(u32 ret, Term *args) {
  // We have the built tree; tail-call sum with it.
  return aot_make_call(
    aot_make_task(FN_SUM, ret, args[0], 0, 0, 0));
}

static AotResult my_dispatch(AotProgram *p, AotTask *t) {
  (void)p;
  switch (t->fn_id) {
    case FN_BUILD:    return fn_build(t->ret, t->args);
    case FN_SUM:      return fn_sum(t->ret, t->args);
    case FN_MAIN:     return fn_main(t->ret, t->args);
    case CONT_BUILD:  return cont_build(t->ret, t->args);
    case CONT_SUM:    return cont_sum(t->ret, t->args);
    case CONT_MAIN:   return cont_main(t->ret, t->args);
    default:          return aot_make_value(make_num(0));
  }
}

// === Tests ===========================================================

static u32 expected_sum(u32 depth) {
  // Sum of 0..2^d-1 = (2^d - 1) * 2^d / 2  (mod 2^32 for big d).
  u64 n = 1ull << depth;
  u64 s = (n - 1ull) * n / 2ull;
  return (u32)s;
}

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
  thvm_init();
  setvbuf(stdout, NULL, _IONBF, 0);

  AotProgram prog;
  prog.dispatch = my_dispatch;

  // === Correctness at small depths via the serial runner ============
  TEST_BEGIN("tree_sum d=4 serial");
  {
    AotTask root = aot_make_task(FN_MAIN, AOT_RET_ROOT,
                                  make_num(4), 0, 0, 0);
    Term r = aot_run_serial(&prog, root);
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), expected_sum(4));
  }

  TEST_BEGIN("tree_sum d=8 serial");
  {
    AotTask root = aot_make_task(FN_MAIN, AOT_RET_ROOT,
                                  make_num(8), 0, 0, 0);
    Term r = aot_run_serial(&prog, root);
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), expected_sum(8));
  }

  // === Correctness via the parallel runner (forces 2 threads) =======
  TEST_BEGIN("tree_sum d=8 parallel T=2");
  {
    thvm_free(); thvm_init();
    AotTask root = aot_make_task(FN_MAIN, AOT_RET_ROOT,
                                  make_num(8), 0, 0, 0);
    Term r = aot_run_parallel(&prog, root, 2);
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), expected_sum(8));
  }

  TEST_BEGIN("tree_sum d=10 parallel T=4");
  {
    thvm_free(); thvm_init();
    AotTask root = aot_make_task(FN_MAIN, AOT_RET_ROOT,
                                  make_num(10), 0, 0, 0);
    Term r = aot_run_parallel(&prog, root, 4);
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), expected_sum(10));
  }

  // === Optional perf bench ==========================================
  // BEND_TREE_DEPTH=20 ./bin/test_bend_tree_sum
  // Compares T=1 / T=4 / T=8 wall times.  Skipped under the default
  // depth (4) so `make test` stays fast.
  const char *depth_env = getenv("BEND_TREE_DEPTH");
  u32 bench_depth = depth_env ? (u32)atoi(depth_env) : 0;
  if (bench_depth >= 6) {
    printf("\nperf bench at depth=%u (expected sum=%u):\n",
           bench_depth, expected_sum(bench_depth));
    for (u32 t = 1; t <= 8; t *= 2) {
      thvm_free(); thvm_init();
      AotTask root = aot_make_task(FN_MAIN, AOT_RET_ROOT,
                                    make_num(bench_depth), 0, 0, 0);
      double t0 = now_s();
      Term r = aot_run_parallel(&prog, root, t);
      double dt = now_s() - t0;
      printf("  T=%u  %8.2fms  val=%u  match=%s\n",
             t, dt * 1000.0,
             (u32)term_val(r),
             (u32)term_val(r) == expected_sum(bench_depth) ? "ok" : "FAIL");
    }
  }

  TEST_REPORT();
}
