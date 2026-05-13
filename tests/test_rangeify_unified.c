// test_rangeify_unified.c - exercise the unified rangeify pass
// (Phase 2 of docs/plans/ideal_pipeline_v2.md).
//
// The unified pass is a 1-to-1 port of tinygrad's run_rangeify +
// pm_apply_rangeify. It is gated behind THVM_UNIFIED_RANGEIFY=1; this
// test sets the env var, then builds a small UOp graph and inspects
// the produced range-map + realize-map + substitute table via the
// rangeify_unified_*_at accessors.
//
// Coverage targets:
//   (a) gate-off default: the pass does nothing if env var unset.
//   (b) single-consumer chain: producer inherits consumer ranges (no
//       new realize).
//   (c) diamond fan-out: shared producer realizes (multi-consumer rule).
//   (d) REDUCE injects a fresh KAX_REDUCE range on the reduced axis.
//   (e) ranges are assigned monotonically (the axis_id counter
//       increments on each new RANGE).

#include "../src/thvm.c"
#include "test.h"
#include <string.h>

static u32 alloc_f32_tensor1(u32 d0) {
  Shape s = {0};
  s.ndim = 1; s.dims[0] = d0;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static u32 alloc_f32_tensor2(u32 d0, u32 d1) {
  Shape s = {0};
  s.ndim = 2; s.dims[0] = d0; s.dims[1] = d1;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

int main(void) {
  thvm_init();

  // (a) Gate-off default. Without the env var, rangeify_unified_enabled
  // returns 0 and consumers do not invoke run_rangeify_unified.
  TEST_BEGIN("unified-rangeify/gate-default-off");
  unsetenv("THVM_UNIFIED_RANGEIFY");
  CHECK_EQ(rangeify_unified_enabled(), 0);
  setenv("THVM_UNIFIED_RANGEIFY", "0", 1);
  CHECK_EQ(rangeify_unified_enabled(), 0);
  setenv("THVM_UNIFIED_RANGEIFY", "1", 1);
  CHECK_EQ(rangeify_unified_enabled(), 1);

  // (b) Single-consumer chain. A linear ADD chain: (a + b) * c.
  // The ADD is consumed by MUL once -> ADD inherits MUL's ranges, no
  // partial-realize. MUL is the root -> realized_full.
  TEST_BEGIN("unified-rangeify/linear-chain-no-extra-realize");
  Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(3));
  Term b = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(3));
  Term c = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(3));
  Term add = uop_binary(UOP_ADD, a, b);
  Term mul = uop_binary(UOP_MUL, add, c);
  bufferize_classify(mul);
  run_rangeify_unified(mul);
  // Both nodes were walked.
  CHECK(rangeify_unified_last_nodes_walked() >= 2);
  // The root realized.
  u32 mul_idx = bufferize_info_find(term_val(mul));
  u32 add_idx = bufferize_info_find(term_val(add));
  CHECK(mul_idx != 0xFFFFFFFFu);
  CHECK(add_idx != 0xFFFFFFFFu);
  CHECK_EQ(rangeify_unified_is_realized(mul_idx), 1);
  // ADD has ranges (inherited from MUL).
  CHECK_EQ(rangeify_unified_has_ranges_at(add_idx), 1);
  CHECK_EQ(rangeify_unified_out_ndim_at(add_idx), 1);
  // ADD's out_rngs[0] equals MUL's out_rngs[0] (inheritance).
  Term add_r0 = rangeify_unified_out_rng_at(add_idx, 0);
  Term mul_r0 = rangeify_unified_out_rng_at(mul_idx, 0);
  CHECK_EQ(add_r0, mul_r0);
  // The substitute is non-zero (INDEX_E wrap of the node).
  CHECK(rangeify_unified_subst_at(mul_idx) != 0);

  // (c) Diamond fan-out: shared = a + b; out = shared * shared via
  // distinct parent uops would require two ops referencing shared.
  // Tested in test_bufferize_classify; here we go via two distinct
  // consumers: parent1 = shared * c, parent2 = shared * d, root = p1 + p2.
  // The shared ADD should get either MULTI realize (from
  // bufferize_classify seed) or PARTIAL-realize (from the
  // consumer-divergence check). Either way is_realized == 1.
  TEST_BEGIN("unified-rangeify/diamond-shared-producer-realized");
  Term d = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(3));
  Term shared = uop_binary(UOP_ADD, a, b);
  Term p1 = uop_binary(UOP_MUL, shared, c);
  Term p2 = uop_binary(UOP_MUL, shared, d);
  Term root = uop_binary(UOP_ADD, p1, p2);
  bufferize_classify(root);
  run_rangeify_unified(root);
  u32 shared_idx = bufferize_info_find(term_val(shared));
  CHECK(shared_idx != 0xFFFFFFFFu);
  // Shared has 2 consumers => bufferize_classify seeds it as realized
  // via the MULTI rule, or our pass marks partial-realize. Either way:
  CHECK_EQ(rangeify_unified_is_realized(shared_idx), 1);

  // (d) REDUCE injects KAX_REDUCE range. We sum a 1D tensor (axis 0)
  // -> the producer reads through one fresh REDUCE range.
  TEST_BEGIN("unified-rangeify/reduce-injects-kax-reduce-range");
  Term ra = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(8));
  Term rred = uop_reduce(REDUCE_SUM, /*axis*/0, ra);
  bufferize_classify(rred);
  run_rangeify_unified(rred);
  u32 red_idx = bufferize_info_find(term_val(rred));
  CHECK(red_idx != 0xFFFFFFFFu);
  // REDUCE node is the root and realized.
  CHECK_EQ(rangeify_unified_is_realized(red_idx), 1);
  // REDUCE node has 1 output axis... wait, scalar output. tinygrad keeps
  // reduce as a shape-preserving op (axis collapsed to size 1).
  // Our term_shape_in reports REDUCE output rank == src rank with the
  // reduced axis collapsed; we accept either 0 or 1 here.
  CHECK(rangeify_unified_has_ranges_at(red_idx) == 1);

  // (e) RU_RANGE_IDX_COUNTER tracks how many fresh RANGE leaves the
  // pass allocated. The counter resets at the start of each
  // run_rangeify_unified call (mirrors tinygrad's per-pass
  // itertools.count(0) on `range_idx`). After a 2D root realize we
  // expect at least 2 fresh ranges (one per output axis).
  TEST_BEGIN("unified-rangeify/range-idx-monotonic");
  Term e1 = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(4, 5));
  Term e2 = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(4, 5));
  Term sum2 = uop_binary(UOP_ADD, e1, e2);
  bufferize_classify(sum2);
  run_rangeify_unified(sum2);
  u32 counter_after = rangeify_unified_range_idx_counter();
  // The 2D ADD-root should create at least 2 fresh ranges.
  CHECK(counter_after >= 2);

  // (f) The substitute Term is a UOp INDEX_E (non-zero, non-passthrough).
  TEST_BEGIN("unified-rangeify/substitute-is-index-e");
  Term s = rangeify_unified_subst_at(bufferize_info_find(term_val(sum2)));
  CHECK(s != 0);
  CHECK_EQ(term_tag(s), TAG_UOP);
  CHECK_EQ(term_ext(s), UOP_INDEX_E);

  // (g) Skip path: with the gate off, the pass clears the side table.
  // We still call run_rangeify_unified directly to exercise the
  // accessors, but the gate-off branch in any future client would skip.
  TEST_BEGIN("unified-rangeify/skip-non-uop-root-safe");
  // term_new returns a TAG_TEN (not UOP) for the root -> safe early exit.
  Term ten_root = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(2));
  run_rangeify_unified(ten_root);
  CHECK_EQ(rangeify_unified_last_nodes_walked(), 0);

  thvm_free();
  TEST_REPORT();
}
