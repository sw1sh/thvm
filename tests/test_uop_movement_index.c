// test_uop_movement_index.c - Phase B1: per-USE movement-chain resolver.
//
// Verifies `uop_resolve_movement_chain` peels each movement op
// outside-in, transforming the consumer's iter context.  One block
// per movement op (PERMUTE / EXPAND / RESHAPE / PAD / SHRINK / FLIP)
// plus integration cases (chains, non-movement bottom, rank
// mismatches, valid-mask plumbing).

#include "../src/thvm.c"
#include "test.h"

// Build a TAG_TEN backed by a real tensor allocation so term_shape_in
// can resolve its shape.  ndim/dims drive the allocation.
static Term make_tensor(u32 ndim, u32 const *dims) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

int main(void) {
  thvm_init();

  // === UOP_PERMUTE ===
  TEST_BEGIN("movement-index/permute-iter-reorder");
  u32 src_dims_3[3] = {8, 16, 32};
  Term buf3 = make_tensor(3, src_dims_3);
  u32 perm[3] = {2, 0, 1};
  Term p = uop_permute(buf3, 3, perm);
  Term out_iters[3] = {
    uop_range(0, S_AXIS_LOOP, 32),  // perm output axis 0 (was input axis 2)
    uop_range(1, S_AXIS_LOOP, 8),   // perm output axis 1 (was input axis 0)
    uop_range(2, S_AXIS_LOOP, 16),  // perm output axis 2 (was input axis 1)
  };
  Term resolved[MAX_DIM] = {out_iters[0], out_iters[1], out_iters[2]};
  u32 ndim = 3;
  Term mask = 0;
  Term bottom = uop_resolve_movement_chain(p, resolved, &ndim, &mask);
  CHECK_EQ(bottom, buf3);
  CHECK_EQ(ndim, 3);
  CHECK_EQ(resolved[0], out_iters[1]);
  CHECK_EQ(resolved[1], out_iters[2]);
  CHECK_EQ(resolved[2], out_iters[0]);
  CHECK_EQ(mask, (Term)0);

  TEST_BEGIN("movement-index/non-movement-bottom-passes-through");
  Term iters_through[2] = { out_iters[0], out_iters[1] };
  u32 nd_through = 2;
  Term mask_through = 0;
  Term r_through = uop_resolve_movement_chain(buf3, iters_through, &nd_through, &mask_through);
  CHECK_EQ(r_through, buf3);
  CHECK_EQ(nd_through, 2);
  CHECK_EQ(mask_through, (Term)0);

  TEST_BEGIN("movement-index/permute-rank-mismatch-bails");
  Term iters_bad[2] = { out_iters[0], out_iters[1] };
  u32 nd_bad = 2;
  Term mask_bad = 0;
  Term r_bad = uop_resolve_movement_chain(p, iters_bad, &nd_bad, &mask_bad);
  CHECK_EQ(r_bad, (Term)0);

  // === UOP_EXPAND ===
  TEST_BEGIN("movement-index/expand-broadcast-axis-zeros-iter");
  // src shape [1, 4]; expand to [3, 4].  Axis 0 is broadcast.
  u32 small_dims[2] = {1, 4};
  Term src_small = make_tensor(2, small_dims);
  u32 exp_dims[2] = {3, 4};
  Term ex = uop_expand(src_small, 2, exp_dims);
  Term ex_iters[MAX_DIM] = {
    uop_range(10, S_AXIS_LOOP, 3),
    uop_range(11, S_AXIS_LOOP, 4),
  };
  Term ex_resolved[MAX_DIM] = {ex_iters[0], ex_iters[1]};
  u32 ex_ndim = 2;
  Term ex_mask = 0;
  Term ex_bottom = uop_resolve_movement_chain(ex, ex_resolved, &ex_ndim, &ex_mask);
  CHECK_EQ(ex_bottom, src_small);
  CHECK_EQ(ex_ndim, 2);
  // Broadcast axis: iter replaced by ICONST(0).
  Term zero_const = uop_const(DT_INT32, 0);
  CHECK_EQ(ex_resolved[0], zero_const);
  // Pass-through axis: iter unchanged.
  CHECK_EQ(ex_resolved[1], ex_iters[1]);

  // === UOP_RESHAPE ===
  TEST_BEGIN("movement-index/reshape-flat-decomposition");
  // src shape [6]; reshape to [2, 3].  Flat = i0 * 3 + i1.
  // Decompose back: in_iter[0] = flat (single axis, no mod, no div).
  u32 flat_dims[1] = {6};
  Term src_flat = make_tensor(1, flat_dims);
  u32 rs_dims[2] = {2, 3};
  Term rs = uop_reshape(src_flat, 2, rs_dims);
  Term rs_iters[MAX_DIM] = {
    uop_range(20, S_AXIS_LOOP, 2),
    uop_range(21, S_AXIS_LOOP, 3),
  };
  Term rs_resolved[MAX_DIM] = {rs_iters[0], rs_iters[1]};
  u32 rs_ndim = 2;
  Term rs_mask = 0;
  Term rs_bottom = uop_resolve_movement_chain(rs, rs_resolved, &rs_ndim, &rs_mask);
  CHECK_EQ(rs_bottom, src_flat);
  CHECK_EQ(rs_ndim, 1);
  // Expected: rs_resolved[0] = (i0 * 3) + i1.
  CHECK_EQ(term_tag(rs_resolved[0]), TAG_UOP);
  CHECK_EQ(term_ext(rs_resolved[0]), UOP_IADD);
  Term rs_lhs = heap_read(term_val(rs_resolved[0]) + 0);
  Term rs_rhs = heap_read(term_val(rs_resolved[0]) + 1);
  CHECK_EQ(term_ext(rs_lhs), UOP_IMUL);
  CHECK_EQ(rs_rhs, rs_iters[1]);
  CHECK_EQ(heap_read(term_val(rs_lhs) + 0), rs_iters[0]);
  Term rs_three = heap_read(term_val(rs_lhs) + 1);
  CHECK_EQ(term_ext(rs_three), UOP_CONST);

  // === UOP_PAD ===
  TEST_BEGIN("movement-index/pad-iter-shift-and-mask");
  // src shape [4]; pad (1, 1) -> shape [6].  in_iter = out_iter - 1.
  // valid = (out_iter < 5) & (1 - (out_iter < 1)).
  u32 pad_dims[1] = {4};
  Term src_pad = make_tensor(1, pad_dims);
  u32 pad_widths[2] = {1, 1};
  Term pd = uop_pad(src_pad, 1, pad_widths);
  Term pd_iter = uop_range(30, S_AXIS_LOOP, 6);
  Term pd_resolved[MAX_DIM] = {pd_iter};
  u32 pd_ndim = 1;
  Term pd_mask = 0;
  Term pd_bottom = uop_resolve_movement_chain(pd, pd_resolved, &pd_ndim, &pd_mask);
  CHECK_EQ(pd_bottom, src_pad);
  CHECK_EQ(pd_ndim, 1);
  // Expected: pd_resolved[0] = pd_iter - 1.
  CHECK_EQ(term_ext(pd_resolved[0]), UOP_ISUB);
  CHECK_EQ(heap_read(term_val(pd_resolved[0]) + 0), pd_iter);
  // Mask non-zero (we have a real PAD).
  CHECK(pd_mask != 0);
  CHECK_EQ(term_ext(pd_mask), UOP_IAND);

  TEST_BEGIN("movement-index/pad-with-null-mask-bails");
  Term pd_iter_null = uop_range(31, S_AXIS_LOOP, 6);
  Term pd_resolved_null[MAX_DIM] = {pd_iter_null};
  u32 pd_ndim_null = 1;
  Term pd_bottom_null = uop_resolve_movement_chain(pd, pd_resolved_null,
                                                   &pd_ndim_null, NULL);
  CHECK_EQ(pd_bottom_null, (Term)0);

  // === UOP_SHRINK ===
  TEST_BEGIN("movement-index/shrink-iter-shift");
  // src shape [10]; shrink to [4..7) -> shape [3].  in_iter = out_iter + 4.
  u32 sh_dims[1] = {10};
  Term src_sh = make_tensor(1, sh_dims);
  u32 sh_widths[2] = {4, 7};
  Term shr = uop_shrink(src_sh, 1, sh_widths);
  Term sh_iter = uop_range(40, S_AXIS_LOOP, 3);
  Term sh_resolved[MAX_DIM] = {sh_iter};
  u32 sh_ndim = 1;
  Term sh_mask = 0;
  Term sh_bottom = uop_resolve_movement_chain(shr, sh_resolved, &sh_ndim, &sh_mask);
  CHECK_EQ(sh_bottom, src_sh);
  CHECK_EQ(sh_ndim, 1);
  CHECK_EQ(term_ext(sh_resolved[0]), UOP_IADD);
  CHECK_EQ(heap_read(term_val(sh_resolved[0]) + 0), sh_iter);
  Term sh_const = heap_read(term_val(sh_resolved[0]) + 1);
  CHECK_EQ(term_ext(sh_const), UOP_CONST);
  CHECK_EQ(sh_mask, (Term)0);

  TEST_BEGIN("movement-index/shrink-zero-begin-iter-unchanged");
  u32 sh_widths_zero[2] = {0, 5};
  Term shr_zero = uop_shrink(src_sh, 1, sh_widths_zero);
  Term sh_iter_zero = uop_range(41, S_AXIS_LOOP, 5);
  Term sh_resolved_zero[MAX_DIM] = {sh_iter_zero};
  u32 sh_ndim_zero = 1;
  Term sh_mask_zero = 0;
  Term sh_bot_zero = uop_resolve_movement_chain(shr_zero, sh_resolved_zero,
                                                &sh_ndim_zero, &sh_mask_zero);
  CHECK_EQ(sh_bot_zero, src_sh);
  CHECK_EQ(sh_resolved_zero[0], sh_iter_zero);

  // === UOP_FLIP ===
  TEST_BEGIN("movement-index/flip-iter-mirror");
  // src shape [5]; flip axis 0 -> in_iter = (5-1) - out_iter.
  u32 fl_dims[1] = {5};
  Term src_fl = make_tensor(1, fl_dims);
  Term fl = uop_flip(src_fl, /*mask=*/0x1u);
  Term fl_iter = uop_range(50, S_AXIS_LOOP, 5);
  Term fl_resolved[MAX_DIM] = {fl_iter};
  u32 fl_ndim = 1;
  Term fl_mask = 0;
  Term fl_bottom = uop_resolve_movement_chain(fl, fl_resolved, &fl_ndim, &fl_mask);
  CHECK_EQ(fl_bottom, src_fl);
  CHECK_EQ(fl_ndim, 1);
  CHECK_EQ(term_ext(fl_resolved[0]), UOP_ISUB);
  Term fl_lhs = heap_read(term_val(fl_resolved[0]) + 0);
  CHECK_EQ(term_ext(fl_lhs), UOP_CONST);
  CHECK_EQ(heap_read(term_val(fl_resolved[0]) + 1), fl_iter);

  // === Chain integration ===
  TEST_BEGIN("movement-index/chain-shrink-then-permute");
  // src shape [10, 20]; shrink to [4..8, 0..15] -> [4, 15];
  // permute [1,0] -> [15, 4].  Resolve outer-permute, then shrink.
  u32 chain_dims[2] = {10, 20};
  Term src_chain = make_tensor(2, chain_dims);
  u32 chain_shrink[4] = {4, 8, 0, 15};
  Term shrunk = uop_shrink(src_chain, 2, chain_shrink);
  u32 chain_perm[2] = {1, 0};
  Term chain_top = uop_permute(shrunk, 2, chain_perm);
  Term ch_iters[MAX_DIM] = {
    uop_range(60, S_AXIS_LOOP, 15),
    uop_range(61, S_AXIS_LOOP, 4),
  };
  Term ch_resolved[MAX_DIM] = {ch_iters[0], ch_iters[1]};
  u32 ch_ndim = 2;
  Term ch_mask = 0;
  Term ch_bottom = uop_resolve_movement_chain(chain_top, ch_resolved,
                                              &ch_ndim, &ch_mask);
  CHECK_EQ(ch_bottom, src_chain);
  CHECK_EQ(ch_ndim, 2);
  // After permute [1,0]: in[1] = out[0], in[0] = out[1].
  // After shrink (4, 0): in[0] = (out[1]) + 4, in[1] = out[0] + 0 = out[0].
  CHECK_EQ(term_ext(ch_resolved[0]), UOP_IADD);
  CHECK_EQ(heap_read(term_val(ch_resolved[0]) + 0), ch_iters[1]);
  CHECK_EQ(ch_resolved[1], ch_iters[0]);

  thvm_free();
  TEST_REPORT();
}
