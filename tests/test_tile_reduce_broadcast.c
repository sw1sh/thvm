// test_tile_reduce_broadcast.c - Phase D3: reduce-broadcast analyzer
// + canonical lowering shape.
//
// `tile_analyze_reduce_broadcast` recognises kernels whose scalar
// program has a single S_REDUCE_* whose result is consumed by a
// non-store-direct op (the BN-grad / softmax / layernorm shape).
// `tile_lower_reduce_broadcast` builds the canonical TILE_BLOCK
// preamble that the renderer (Phase F) will emit as a cooperative-
// reduce shared-memory accumulator.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  u32 kid = kernel_alloc();
  KernelEntry *kep = &KERNELS[kid];

  TEST_BEGIN("tile-reduce-bcast/analyzer-zero-reduces-skips");
  KernelEntry ke = *kep;  // copy of fresh entry
  kernel_inputs_reserve(&ke, 1);
  ke.n_inputs        = 1;
  ke.input_tids[0]   = 0;
  ke.input_dtypes[0] = DT_FP32;
  ke.input_numels[0] = 4;
  ke.output_dtype    = DT_FP32;
  ke.output_numel    = 4;
  // Build a plain elementwise: STORE(INDEX(out), LOAD(INDEX(in)))
  u32 r0 = rangeify_emit_leaf(&ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | 4u);
  u32 pa = rangeify_emit_leaf(&ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc = rangeify_emit_leaf(&ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 ia_src[2] = {pa, r0};
  u32 ia = rangeify_emit(&ke, S_INDEX_E, DT_FP32, 2, ia_src, 0);
  u32 la = rangeify_emit_unary(&ke, S_LOAD, DT_FP32, ia);
  u32 ic_src[2] = {pc, r0};
  u32 ic = rangeify_emit(&ke, S_INDEX_E, DT_FP32, 2, ic_src, 0);
  u32 sto = rangeify_emit_binary(&ke, S_STORE, DT_FP32, ic, la);
  u32 buf_src[2] = {sto, r0};
  rangeify_emit(&ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);
  CHECK_EQ(tile_analyze_reduce_broadcast(&ke), 0u);
  rangeify_free(&ke);
  tile_free(&ke);

  TEST_BEGIN("tile-reduce-bcast/analyzer-pure-reduce-store-skips");
  // REDUCE -> STORE direct.  Not a broadcast pattern.
  ke = *kep;
  kernel_inputs_reserve(&ke, 1);
  ke.n_inputs        = 1;
  ke.input_tids[0]   = 0;
  ke.input_dtypes[0] = DT_FP32;
  ke.input_numels[0] = 4;
  ke.output_dtype    = DT_FP32;
  ke.output_numel    = 1;
  u32 r0_out = rangeify_emit_leaf(&ke, S_RANGE, DT_INT32,
                                  ((u64)S_AXIS_LOOP << 32) | 1u);
  u32 r0_red = rangeify_emit_leaf(&ke, S_RANGE, DT_INT32,
                                  ((u64)S_AXIS_REDUCE << 32) | 4u);
  u32 pa2 = rangeify_emit_leaf(&ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc2 = rangeify_emit_leaf(&ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 ia2_src[2] = {pa2, r0_red};
  u32 ia2 = rangeify_emit(&ke, S_INDEX_E, DT_FP32, 2, ia2_src, 0);
  u32 la2 = rangeify_emit_unary(&ke, S_LOAD, DT_FP32, ia2);
  u32 red_src[2] = {la2, r0_red};
  u32 red = rangeify_emit(&ke, S_REDUCE_SUM, DT_FP32, 2, red_src, 0);
  u32 ic2_src[2] = {pc2, r0_out};
  u32 ic2 = rangeify_emit(&ke, S_INDEX_E, DT_FP32, 2, ic2_src, 0);
  u32 sto2 = rangeify_emit_binary(&ke, S_STORE, DT_FP32, ic2, red);
  u32 buf2_src[2] = {sto2, r0_out};
  rangeify_emit(&ke, S_BUFFERIZE, DT_FP32, 2, buf2_src, 0);
  CHECK_EQ(tile_analyze_reduce_broadcast(&ke), 0u);
  rangeify_free(&ke);
  tile_free(&ke);

  TEST_BEGIN("tile-reduce-bcast/analyzer-detects-mul-after-reduce");
  // REDUCE -> MUL by 2 -> STORE.  The MUL consumes the reduce result;
  // analyzer recognises this as broadcast (the BN-grad shape).
  ke = *kep;
  kernel_inputs_reserve(&ke, 1);
  ke.n_inputs        = 1;
  ke.input_tids[0]   = 0;
  ke.input_dtypes[0] = DT_FP32;
  ke.input_numels[0] = 4;
  ke.output_dtype    = DT_FP32;
  ke.output_numel    = 1;
  u32 r0_out3 = rangeify_emit_leaf(&ke, S_RANGE, DT_INT32,
                                   ((u64)S_AXIS_LOOP << 32) | 1u);
  u32 r0_red3 = rangeify_emit_leaf(&ke, S_RANGE, DT_INT32,
                                   ((u64)S_AXIS_REDUCE << 32) | 4u);
  u32 pa3 = rangeify_emit_leaf(&ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pc3 = rangeify_emit_leaf(&ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 ia3_src[2] = {pa3, r0_red3};
  u32 ia3 = rangeify_emit(&ke, S_INDEX_E, DT_FP32, 2, ia3_src, 0);
  u32 la3 = rangeify_emit_unary(&ke, S_LOAD, DT_FP32, ia3);
  u32 red3_src[2] = {la3, r0_red3};
  u32 red3 = rangeify_emit(&ke, S_REDUCE_SUM, DT_FP32, 2, red3_src, 0);
  u32 c_two = rangeify_emit_leaf(&ke, S_CONST, DT_FP32, 0x40000000u);
  u32 scaled = rangeify_emit_binary(&ke, S_MUL, DT_FP32, red3, c_two);
  u32 ic3_src[2] = {pc3, r0_out3};
  u32 ic3 = rangeify_emit(&ke, S_INDEX_E, DT_FP32, 2, ic3_src, 0);
  u32 sto3 = rangeify_emit_binary(&ke, S_STORE, DT_FP32, ic3, scaled);
  u32 buf3_src[2] = {sto3, r0_out3};
  rangeify_emit(&ke, S_BUFFERIZE, DT_FP32, 2, buf3_src, 0);
  u32 detected = tile_analyze_reduce_broadcast(&ke);
  CHECK_EQ(detected, red3);

  TEST_BEGIN("tile-reduce-bcast/lowering-emits-canonical-block");
  // Now lower it.  Expect a TILE_STORE wrapping a TILE_BLOCK whose
  // 5 entries are alloc / reduce / barrier / load / scalar-body.
  u32 tile_store = tile_lower_reduce_broadcast(&ke, detected, /*groups=*/32);
  CHECK(tile_store != 0);
  CHECK_EQ(ke.tile_uops[tile_store].op, TILE_STORE);
  u32 block_id = ke.tile_uops[tile_store].src[0];
  CHECK_EQ(ke.tile_uops[block_id].op, TILE_BLOCK);
  CHECK_EQ(ke.tile_uops[block_id].src_count, 5);
  CHECK_EQ(ke.tile_uops[ke.tile_uops[block_id].src[0]].op, TILE_LOCAL_ALLOC);
  CHECK_EQ(ke.tile_uops[ke.tile_uops[block_id].src[1]].op, TILE_REDUCE);
  CHECK_EQ(ke.tile_uops[ke.tile_uops[block_id].src[2]].op, TILE_BARRIER);
  CHECK_EQ(ke.tile_uops[ke.tile_uops[block_id].src[3]].op, TILE_LOAD);
  CHECK_EQ(ke.tile_uops[ke.tile_uops[block_id].src[4]].op, TILE_SCALAR_BODY);

  TEST_BEGIN("tile-reduce-bcast/lowering-alloc-uses-shared-scope");
  u32 alloc_id = ke.tile_uops[block_id].src[0];
  TileAllocInfo ainfo = tile_alloc_unpack(ke.tile_uops[alloc_id].extra);
  CHECK_EQ(ainfo.scope,      TILE_MEM_SHARED);
  CHECK_EQ(ainfo.n_elements, 32);

  TEST_BEGIN("tile-reduce-bcast/lowering-barrier-uses-shared-scope");
  u32 barr_id = ke.tile_uops[block_id].src[2];
  CHECK_EQ((u32)ke.tile_uops[barr_id].extra, TILE_MEM_SHARED);

  TEST_BEGIN("tile-reduce-bcast/lowering-load-reads-from-alloc");
  u32 load_id = ke.tile_uops[block_id].src[3];
  CHECK_EQ(ke.tile_uops[load_id].src[0], alloc_id);

  rangeify_free(&ke);
  tile_free(&ke);
  thvm_free();
  TEST_REPORT();
}
