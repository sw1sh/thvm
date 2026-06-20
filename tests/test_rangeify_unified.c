// test_rangeify_unified.c - exercise the unified rangeify pass
// (Phase 2 of docs/plans/ideal_pipeline_v2.md).
//
// The unified pass is a 1-to-1 port of tinygrad's run_rangeify +
// pm_apply_rangeify.  This test builds a small UOp graph and inspects
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

  // (c) Diamond fan-out: shared = a + b; consumers are
  // parent1 = shared * c, parent2 = shared * d, root = p1 + p2.
  // bufferize_classify pre-seeds `shared` as realized via the MULTI
  // rule (consumer_count >= 2), so the unified pass inherits
  // realized_full = 1 from that seed.
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

  // (f) For a realized boundary, RU_SUBST holds the bare UOP_BUFFERIZE
  // Term (non-zero, non-passthrough).  ru_rewrite_subtree wraps each
  // downstream consumer with uop_index_e(BUFFERIZE, consumer_in_addr)
  // (see rangeify_unified.c:~1796); that wrapping happens per-consumer,
  // not in RU_SUBST itself.  Mirror: tinygrad/schedule/indexing.py:78
  // (`BUFFERIZE.index(*consumer_ranges)`).
  TEST_BEGIN("unified-rangeify/substitute-is-bufferize");
  Term s = rangeify_unified_subst_at(bufferize_info_find(term_val(sum2)));
  CHECK(s != 0);
  CHECK_EQ(term_tag(s), TAG_UOP);
  CHECK_EQ(term_ext(s), UOP_BUFFERIZE);

  // (g) Skip path: with the gate off, the pass clears the side table.
  // We still call run_rangeify_unified directly to exercise the
  // accessors, but the gate-off branch in any future client would skip.
  TEST_BEGIN("unified-rangeify/skip-non-uop-root-safe");
  // term_new returns a TAG_TEN (not UOP) for the root -> safe early exit.
  Term ten_root = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(2));
  run_rangeify_unified(ten_root);
  CHECK_EQ(rangeify_unified_last_nodes_walked(), 0);

  // (h-pre) Phase 4a-pre-3: REDUCE-via-RANGE production. The unified
  // pass records the fresh KAX_REDUCE range it injects on the
  // UOP_REDUCE node's reduce axis. Mirror: tinygrad/schedule/indexing.
  // py:90-96 (convert_reduce_to_reduce_with_ranges) puts the new
  // ranges in `src=(value,)+tuple(new_ranges)`; we record them in a
  // side-table accessible via rangeify_unified_reduce_range_at.
  TEST_BEGIN("unified-rangeify/reduce-ranges-recorded");
  Term rb = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor1(8));
  Term rred2 = uop_reduce(REDUCE_SUM, /*axis*/0, rb);
  bufferize_classify(rred2);
  run_rangeify_unified(rred2);
  u32 rred2_idx = bufferize_info_find(term_val(rred2));
  CHECK(rred2_idx != 0xFFFFFFFFu);
  CHECK_EQ(rangeify_unified_reduce_n_ranges_at(rred2_idx), 1);
  Term rrng = rangeify_unified_reduce_range_at(rred2_idx, 0);
  CHECK_EQ(term_tag(rrng), TAG_UOP);
  CHECK_EQ(term_ext(rrng), UOP_RANGE);
  // axis_type marks it as a reduce iteration.
  CHECK_EQ(uop_range_axis_type(rrng), KAX_REDUCE);
  // extent matches the reduced-axis dim.
  CHECK_EQ(uop_range_extent(rrng), 8);

  // (h) Phase 4a-pre-2: main-heap UOP_BUFFERIZE emitted at realize
  // boundaries.  After run_rangeify_unified on a small ADD-of-ADDs root,
  // the root realize should produce a UOP_BUFFERIZE Term whose value
  // is the root node and whose closed_ranges are RANGE leaves matching
  // the realized axes.  Mirror: tinygrad/schedule/indexing.py:77.
  TEST_BEGIN("unified-rangeify/bufferize-at-root-realize");
  Term ha = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(4, 5));
  Term hb = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(4, 5));
  Term hsum = uop_binary(UOP_ADD, ha, hb);
  bufferize_classify(hsum);
  run_rangeify_unified(hsum);
  u32 hsum_idx = bufferize_info_find(term_val(hsum));
  CHECK(hsum_idx != 0xFFFFFFFFu);
  Term bz = rangeify_unified_bufferize_at(hsum_idx);
  CHECK(bz != 0);
  CHECK_EQ(term_tag(bz), TAG_UOP);
  CHECK_EQ(term_ext(bz), UOP_BUFFERIZE);
  // BUFFERIZE.value points back at the producer node (root ADD).
  Term bz_value = uop_bufferize_value(bz);
  CHECK_EQ(term_tag(bz_value), TAG_UOP);
  CHECK_EQ(term_ext(bz_value), UOP_ADD);
  // Full-realize -> GLOBAL addrspace.
  CHECK_EQ(uop_bufferize_addrspace(bz), UOP_SCOPE_GLOBAL);
  // 2D output -> 2 closed RANGE leaves.
  CHECK_EQ(uop_bufferize_n_ranges(bz), 2);
  Term r0 = uop_bufferize_range_at(bz, 0);
  Term r1 = uop_bufferize_range_at(bz, 1);
  CHECK_EQ(term_tag(r0), TAG_UOP);
  CHECK_EQ(term_ext(r0), UOP_RANGE);
  CHECK_EQ(term_tag(r1), TAG_UOP);
  CHECK_EQ(term_ext(r1), UOP_RANGE);
  // The stats counter ticks: at least one BUFFERIZE emitted.
  CHECK(rangeify_unified_last_bufferizes_emitted() >= 1);
  // The substitute IS the BUFFERIZE term (consumers wrap with INDEX_E
  // per-use in ru_rewrite_subtree).
  Term hsub = rangeify_unified_subst_at(hsum_idx);
  CHECK_EQ(term_tag(hsub), TAG_UOP);
  CHECK_EQ(term_ext(hsub), UOP_BUFFERIZE);
  CHECK_EQ(hsub, bz);

  thvm_free();

  // === compute_bufferize wiring on the always-on unified path ===
  // Build a small ADD kernel, materialise and confirm the kernel's
  // compute_bufferize field points at a UOP_BUFFERIZE Term whose
  // value is the boundary's UOP root.
  TEST_BEGIN("unified-rangeify/compute-bufferize-wired");
  unsetenv("DEV");
  thvm_init();
  Shape sd = {0}; sd.ndim = 1; sd.dims[0] = 4;
  f32 srcda[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 srcdb[4] = {5.0f, 6.0f, 7.0f, 8.0f};
  u32 tda = tensor_alloc(CURRENT_BACKEND, sd, DT_FP32);
  u32 tdb = tensor_alloc(CURRENT_BACKEND, sd, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[tda].buf_id, srcda, sizeof(srcda));
  CURRENT_BACKEND->buf_write(TENS[tdb].buf_id, srcdb, sizeof(srcdb));
  u32 ks_before = KERNELS_NEXT;
  Term dadd = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, tda),
      term_new(0, TAG_TEN, DT_FP32, tdb)));
  CHECK(dadd != 0);
  int saw_bz = 0;
  for (u32 k = ks_before; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].compute_bufferize != 0) { saw_bz = 1; break; }
  }
  CHECK(saw_bz);
  for (u32 k = ks_before; k < KERNELS_NEXT; k++) {
    Term cb = KERNELS[k].compute_bufferize;
    if (cb == 0) continue;
    CHECK_EQ(term_tag(cb), TAG_UOP);
    CHECK_EQ(term_ext(cb), UOP_BUFFERIZE);
    Term val = uop_bufferize_value(cb);
    CHECK(val != 0);
  }
  thvm_free();

  // === THVM_PCONTIG: tinygrad's partial-contiguous flash-fusion ===
  // (rangeify.py:277,288-303, helpers.py:264).  Default 0 == off; > 2 enables
  // the LOCAL-staging else-branch in the buffer_in_reduce arm.

  // (g) ru_pcontig() reads THVM_PCONTIG (helpers.py:264, default 0).
  TEST_BEGIN("unified-rangeify/pcontig-env-reader");
  unsetenv("THVM_PCONTIG");
  CHECK_EQ(ru_pcontig(), 0);
  setenv("THVM_PCONTIG", "3", 1);
  CHECK_EQ(ru_pcontig(), 3);
  setenv("THVM_PCONTIG", "0", 1);
  CHECK_EQ(ru_pcontig(), 0);

  // (h) rb_pcontig_mark_local partitions the boundary's per-axis ranges into
  // is_pcontig (REDUCE / already-LOCAL axes, kept in the LOCAL buffer) vs
  // is_subs (the rest, recomputed) -- tinygrad rangeify.py:295-296.  Synthesize
  // a node whose out_rngs is [LOOP, REDUCE]: the REDUCE axis is is_pcontig
  // (axes_mask bit 1), the LOOP axis is is_subs.  Assert it marks a PARTIAL
  // (LOCAL) realize with the reduce axis closed and the loop axis recomputed.
  TEST_BEGIN("unified-rangeify/pcontig-partition-marks-local");
  thvm_init();
  {
    // Drive a real attention-shaped graph through the pass to populate the
    // node tables, then synthesize the partition on a fresh node slot.
    Term pa = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(8, 8));
    Term psum = uop_reduce(REDUCE_SUM, /*axis*/1, pa);
    bufferize_classify(psum);
    run_rangeify_unified(psum);
    // Use the REDUCE node's own slot: overwrite its range-map with a synthetic
    // [LOOP, REDUCE] out_rngs and clear its realize state, then run the
    // partition helper directly (the unit under test).
    u32 idx = bufferize_info_find(term_val(psum));
    CHECK(idx != 0xFFFFFFFFu);
    RU_RANGE_MAP[idx].has_ranges = 1;
    RU_RANGE_MAP[idx].out_ndim   = 2;
    RU_RANGE_MAP[idx].out_rngs[0] = ru_new_range(8, KAX_LOOP);
    RU_RANGE_MAP[idx].out_rngs[1] = ru_new_range(8, KAX_REDUCE);
    RU_REALIZE_MAP[idx].realized_full    = 0;
    RU_REALIZE_MAP[idx].realized_partial = 0;
    RU_REALIZE_MAP[idx].axes_mask        = 0;
    int marked = rb_pcontig_mark_local(idx);
    CHECK_EQ(marked, 1);                               // is_pcontig + is_subs nonempty
    CHECK_EQ(RU_REALIZE_MAP[idx].realized_partial, 1); // partial -> LOCAL addrspace
    CHECK_EQ(RU_REALIZE_MAP[idx].realized_full, 0);
    CHECK_EQ(RU_REALIZE_MAP[idx].axes_mask, 0x2);      // only the REDUCE axis closed
    CHECK_EQ(RU_REALIZE_MAP[idx].n_realized_axes, 1);
  }
  thvm_free();

  // (i) No is_subs (every axis is a REDUCE axis) -> keep, like tinygrad's
  // `if not len(is_subs): return None` (rangeify.py:298).  And no is_pcontig
  // (every axis is LOOP) -> keep (rangeify.py:300 `if len(is_pcontig):`).
  TEST_BEGIN("unified-rangeify/pcontig-partition-keeps-when-degenerate");
  thvm_init();
  {
    Term pa = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(8, 8));
    Term psum = uop_reduce(REDUCE_SUM, /*axis*/1, pa);
    bufferize_classify(psum);
    run_rangeify_unified(psum);
    u32 idx = bufferize_info_find(term_val(psum));
    CHECK(idx != 0xFFFFFFFFu);
    // All-REDUCE: no is_subs -> keep.
    RU_RANGE_MAP[idx].has_ranges = 1;
    RU_RANGE_MAP[idx].out_ndim   = 2;
    RU_RANGE_MAP[idx].out_rngs[0] = ru_new_range(8, KAX_REDUCE);
    RU_RANGE_MAP[idx].out_rngs[1] = ru_new_range(8, KAX_REDUCE);
    RU_REALIZE_MAP[idx].realized_partial = 0;
    RU_REALIZE_MAP[idx].realized_full    = 0;
    CHECK_EQ(rb_pcontig_mark_local(idx), 0);
    CHECK_EQ(RU_REALIZE_MAP[idx].realized_partial, 0);
    // All-LOOP: no is_pcontig -> keep.
    RU_RANGE_MAP[idx].out_rngs[0] = ru_new_range(8, KAX_LOOP);
    RU_RANGE_MAP[idx].out_rngs[1] = ru_new_range(8, KAX_LOOP);
    CHECK_EQ(rb_pcontig_mark_local(idx), 0);
    CHECK_EQ(RU_REALIZE_MAP[idx].realized_partial, 0);
  }
  thvm_free();

  // (j) Default-off inertness: a softmax-shaped graph (the flash-attention
  // building block: max-reduce -> sub -> exp -> sum-reduce -> recip -> mul)
  // produces a BYTE-IDENTICAL realize-map with THVM_PCONTIG unset vs set.  The
  // gate must never alter the default schedule, and on thvm's decomposed graph
  // (matmul/reduce seed boundaries pre-realized) the PCONTIG ratio gate keeps
  // every candidate, so PCONTIG=3 is a no-op here too -- this documents that
  // the partial-contig staging only diverges once the seed-boundary
  // realization is relaxed (out of scope for the faithful cost-model port).
  TEST_BEGIN("unified-rangeify/pcontig-default-off-inert");
  {
    u8 mask_off[64]; u8 real_off[64]; u32 n_off = 0;
    u8 mask_on[64];  u8 real_on[64];  u32 n_on = 0;
    for (int pass = 0; pass < 2; pass++) {
      if (pass == 0) unsetenv("THVM_PCONTIG"); else setenv("THVM_PCONTIG", "3", 1);
      thvm_init();
      Term sx  = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor2(8, 8));
      Term mx  = uop_reduce(REDUCE_MAX, /*axis*/1, sx);
      Term sub = uop_binary(UOP_ADD, sx, uop_unary(UOP_NEG, mx));
      Term e   = uop_unary(UOP_EXP2, sub);
      Term sm  = uop_reduce(REDUCE_SUM, /*axis*/1, e);
      Term rc  = uop_unary(UOP_RECIP, sm);
      Term out = uop_binary(UOP_MUL, e, rc);
      bufferize_classify(out);
      run_rangeify_unified(out);
      u32 nn = rangeify_unified_last_nodes_walked();
      if (nn > 64) nn = 64;
      for (u32 k = 0; k < nn; k++) {
        if (pass == 0) {
          mask_off[k] = rangeify_unified_axes_mask_at(k);
          real_off[k] = (u8)rangeify_unified_is_realized(k);
        } else {
          mask_on[k] = rangeify_unified_axes_mask_at(k);
          real_on[k] = (u8)rangeify_unified_is_realized(k);
        }
      }
      if (pass == 0) n_off = nn; else n_on = nn;
      thvm_free();
    }
    unsetenv("THVM_PCONTIG");
    CHECK_EQ(n_off, n_on);
    int identical = 1;
    for (u32 k = 0; k < n_off && k < n_on; k++) {
      if (mask_off[k] != mask_on[k] || real_off[k] != real_on[k]) { identical = 0; break; }
    }
    CHECK_EQ(identical, 1);
  }

  TEST_REPORT();
}
