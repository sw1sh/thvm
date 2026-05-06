// test_aot_metal.c -- Phase 7 iter A: AOT-on-Metal toolchain smoke.
//
// Validates the smallest possible Metal-AOT slice end-to-end:
//   1. Build OP2_ADD(NUM(3), NUM(4)) in book_heap.
//   2. Dispatch the `aot_eval_op2_fold` MSL kernel via
//      thvm_aot_metal_op2_fold (one thread, copy-in shared MTLBuffer).
//   3. Verify the result tag is TAG_NUM and value is 7.
//
// Built with -DTHVM_HAS_METAL so src/thvm.c skips the C stub and
// links the real Objective-C backend object (build/backend_metal.o)
// + the metallib produced from src/backend/metal/shaders/.

#include "../src/thvm.c"
#include "test.h"

extern Term thvm_aot_metal_op2_fold(Term *book_heap, u64 book_cells,
                                    u64 root_loc);
extern int  thvm_aot_metal_op2_fold_batch(Term *book_heap, u64 book_cells,
                                          u64 *root_locs, u32 n_roots,
                                          Term *result_out);
extern Term thvm_aot_metal_mat_app(Term *book_heap, u64 book_cells,
                                   u64 root_loc, u64 *book_next_inout);

int main(void) {
  thvm_init();

  // Allocate three contiguous book_heap cells: [arg0, arg1, root].
  u64 args = book_alloc(2);
  book_set(args + 0, term_new(0, TAG_NUM, 0, 3));
  book_set(args + 1, term_new(0, TAG_NUM, 0, 4));
  u64 root = book_alloc(1);
  book_set(root, term_new(0, TAG_OP2, OP_ADD, args));

  TEST_BEGIN("op2 add fold via Metal kernel");
  Term result = thvm_aot_metal_op2_fold(BOOK_HEAP, BOOK_CAP, root);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)7);

  TEST_BEGIN("op2 mul fold via Metal kernel");
  book_set(root, term_new(0, TAG_OP2, OP_MUL, args));
  result = thvm_aot_metal_op2_fold(BOOK_HEAP, BOOK_CAP, root);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)12);

  TEST_BEGIN("op2 lt fold via Metal kernel");
  book_set(root, term_new(0, TAG_OP2, OP_LT, args));
  result = thvm_aot_metal_op2_fold(BOOK_HEAP, BOOK_CAP, root);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)1);  // 3 < 4

  // Iter B-2: batch fan-out -- 64 independent OP2 redexes,
  // one dispatch.  Build OP2(NUM(i), NUM(i+1)) at root[i] for
  // i in 0..63 with mixed ops; verify each result.
  TEST_BEGIN("op2 fold batch (64 roots, one dispatch)");
  enum { N = 64 };
  u64  roots[N];
  Term batch_results[N];
  static const u32 ops[5] = { OP_ADD, OP_SUB, OP_MUL, OP_EQ, OP_LT };
  for (u32 i = 0; i < N; i++) {
    u64 a = book_alloc(2);
    book_set(a + 0, term_new(0, TAG_NUM, 0, i));
    book_set(a + 1, term_new(0, TAG_NUM, 0, i + 1));
    u64 r = book_alloc(1);
    book_set(r, term_new(0, TAG_OP2, ops[i % 5], a));
    roots[i] = r;
  }
  CHECK_EQ(thvm_aot_metal_op2_fold_batch(BOOK_HEAP, BOOK_CAP,
                                          roots, N, batch_results), 0);
  for (u32 i = 0; i < N; i++) {
    u32 av = i, bv = i + 1, want;
    switch (ops[i % 5]) {
      case OP_ADD: want = av + bv;                break;
      case OP_SUB: want = av - bv;                break;
      case OP_MUL: want = av * bv;                break;
      case OP_EQ:  want = (av == bv) ? 1u : 0u;   break;
      case OP_LT:  want = (av <  bv) ? 1u : 0u;   break;
      default:     want = 0u;                     break;
    }
    CHECK_EQ(term_tag(batch_results[i]), (u64)TAG_NUM);
    CHECK_EQ(term_val(batch_results[i]), (u64)want);
  }

  // Iter C-1+C-2: MAT-on-NUM dispatch with GPU bump allocator.
  // Build App(MAT[v=7, [Lam[NUM(42)], Lam[NUM(99)]]], NUM(x)).
  //   matched x=7   -> raw handler (Lam[NUM(42)])
  //   unmatched x=3 -> App(fallback, NUM(3)) -- allocated on GPU
  TEST_BEGIN("mat-on-num matched arm");
  u64 m_loc = book_alloc(2);
  book_set(m_loc + 0, term_new(0, TAG_NUM, 0, 42));   // handler (raw NUM)
  book_set(m_loc + 1, term_new(0, TAG_NUM, 0, 99));   // fallback (raw NUM)
  Term mat = term_new(0, TAG_MAT, /*match_val=*/7, m_loc);

  u64 a_loc_match = book_alloc(2);
  book_set(a_loc_match + 0, mat);
  book_set(a_loc_match + 1, term_new(0, TAG_NUM, 0, 7));   // x = 7 (match)
  u64 root_match = book_alloc(1);
  book_set(root_match, term_new(0, TAG_APP, 0, a_loc_match));

  u64 book_next_state = BOOK_NEXT;
  result = thvm_aot_metal_mat_app(BOOK_HEAP, BOOK_CAP, root_match,
                                  &book_next_state);
  CHECK_EQ(term_tag(result), (u64)TAG_NUM);
  CHECK_EQ(term_val(result), (u64)42);
  // Matched arm doesn't allocate -- book_next_state should be unchanged.
  CHECK_EQ(book_next_state, BOOK_NEXT);

  TEST_BEGIN("mat-on-num fallback arm allocates App(fallback, NUM(x))");
  u64 a_loc_miss = book_alloc(2);
  book_set(a_loc_miss + 0, mat);
  book_set(a_loc_miss + 1, term_new(0, TAG_NUM, 0, 3));    // x = 3 (miss)
  u64 root_miss = book_alloc(1);
  book_set(root_miss, term_new(0, TAG_APP, 0, a_loc_miss));

  u64 next_before = BOOK_NEXT;
  book_next_state = next_before;
  result = thvm_aot_metal_mat_app(BOOK_HEAP, BOOK_CAP, root_miss,
                                  &book_next_state);
  // Fallback path allocates 2 cells [fallback, NUM(x)] on the GPU.
  CHECK_EQ(book_next_state, next_before + 2);
  // Result is an App term whose val points at the new pair.
  CHECK_EQ(term_tag(result), (u64)TAG_APP);
  u64 new_loc = term_val(result);
  CHECK_EQ(new_loc, next_before);
  // The pair holds [fallback=NUM(99), NUM(3)].
  Term f_cell = BOOK_HEAP[new_loc + 0];
  Term x_cell = BOOK_HEAP[new_loc + 1];
  CHECK_EQ(term_tag(f_cell), (u64)TAG_NUM);
  CHECK_EQ(term_val(f_cell), (u64)99);
  CHECK_EQ(term_tag(x_cell), (u64)TAG_NUM);
  CHECK_EQ(term_val(x_cell), (u64)3);

  // Iter RR: batch-vs-sequential dispatch perf.  Quantifies the
  // kernel-launch-overhead win of dispatching N OP2 redexes in one
  // launch (iter B-2) vs N separate launches.  No CHECK -- numbers
  // shift across machines, just informational stderr output.
  TEST_BEGIN("perf: batch (1 launch) vs sequential (N launches)");
  enum { BENCH_N = 256 };
  u64  bench_roots[BENCH_N];
  Term bench_results[BENCH_N];
  for (u32 i = 0; i < BENCH_N; i++) {
    u64 a = book_alloc(2);
    book_set(a + 0, term_new(0, TAG_NUM, 0, i));
    book_set(a + 1, term_new(0, TAG_NUM, 0, i + 1));
    u64 r = book_alloc(1);
    book_set(r, term_new(0, TAG_OP2, OP_ADD, a));
    bench_roots[i] = r;
  }
  // Warm caches with one batch dispatch.
  thvm_aot_metal_op2_fold_batch(BOOK_HEAP, BOOK_CAP, bench_roots,
                                 BENCH_N, bench_results);

  // Sequential timing: BENCH_N separate single-redex launches.
  u64 t_seq0 = cg_now_us();
  for (u32 i = 0; i < BENCH_N; i++) {
    Term r = thvm_aot_metal_op2_fold(BOOK_HEAP, BOOK_CAP, bench_roots[i]);
    bench_results[i] = r;
  }
  u64 dt_seq = cg_now_us() - t_seq0;

  // Batch timing: 1 launch handles all BENCH_N.
  u64 t_batch0 = cg_now_us();
  thvm_aot_metal_op2_fold_batch(BOOK_HEAP, BOOK_CAP, bench_roots,
                                 BENCH_N, bench_results);
  u64 dt_batch = cg_now_us() - t_batch0;

  fprintf(stderr,
    "  bench: %u OP2 folds  sequential = %llu us  batch = %llu us  "
    "speedup ~%llux\n",
    (u32)BENCH_N,
    (unsigned long long)dt_seq, (unsigned long long)dt_batch,
    (unsigned long long)(dt_batch == 0 ? 0 : dt_seq / dt_batch));
  // Sanity: batch must be at least as fast as sequential.
  CHECK(dt_batch <= dt_seq);

  thvm_free();
  TEST_REPORT();
}
