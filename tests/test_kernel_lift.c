// test_kernel_lift.c - Phase C wedge: ScalarUop arena -> UOp DAG.
//
// Builds synthetic kernel programs in the scalar_uops arena (mirrors
// what rangeify produces from a real schedule), runs kernel_lift_to_uop,
// and verifies the resulting UOp DAG renders to valid MSL via the
// existing F0-F3 renderer.

#include "../src/thvm.c"
#include "test.h"

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

static u32 emit_range_axis(KernelEntry *ke, u32 axis_type, u32 extent) {
  u64 extra = ((u64)axis_type << 32) | (u64)extent;
  return rangeify_emit_leaf(ke, S_RANGE, DT_INT64, extra);
}

int main(void) {
  thvm_init();
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];

  TEST_BEGIN("kernel-lift/empty-kernel-rejects");
  KernelUopLift lift = {0};
  CHECK_EQ(kernel_lift_to_uop(ke, &lift), 0);

  TEST_BEGIN("kernel-lift/const-fill-kernel");
  // Build STORE(INDEX(OUT, r0), CONST(1.0f)) wrapped in BUFFERIZE(_, r0).
  ke->output_dtype = DT_FP32;
  u32 r0      = emit_range_axis(ke, 0 /*LOOP*/, 32);
  u32 out_def = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 idx_src[2] = { out_def, r0 };
  u32 idx     = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_src, 0);
  u32 c1      = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x3F800000u);
  u32 st_src[2] = { idx, c1 };
  u32 store   = rangeify_emit(ke, S_STORE, DT_FP32, 2, st_src, 0);
  u32 buf_src[2] = { store, r0 };
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_src, 0);

  CHECK_EQ(kernel_lift_to_uop(ke, &lift), 1);
  CHECK(lift.store_root != 0);
  CHECK(lift.out_buf != 0);
  CHECK_EQ(uop_buffer_dtype(lift.out_buf), DT_FP32);
  CHECK_EQ(uop_buffer_ndim (lift.out_buf), 1u);
  CHECK_EQ(uop_buffer_dim  (lift.out_buf, 0), 32u);

  // Render via cg_render_uop_kernel and verify the MSL emits a const
  // store inside a single for-loop.
  char buf[2048];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  cg_render_uop_kernel(lift.store_root, "k_const", lift.out_buf,
                       lift.in_bufs, lift.n_inputs, fp);
  fclose(fp);
  CHECK(contains(buf, "kernel void k_const"));
  CHECK(contains(buf, "for (uint a0 = 0; a0 < 32"));
  CHECK(contains(buf, "out[a0] = 1.000000f"));

  TEST_BEGIN("kernel-lift/elementwise-add-mul-kernel");
  // Reset kernel; rebuild a more complex shape.
  // STORE(INDEX(OUT, r0), ADD(LOAD(in0[r0]), MUL(LOAD(in1[r0]), CONST(2)))).
  // Two inputs use distinct ranges (different extents) so the inferred
  // input buffer shapes don't hash-cons to the same UOP_BUFFER.  Real
  // kernels get distinct buffers via TenDesc identity; the lifter's
  // shape-only inference path collides on identical shapes.
  kid = kernel_alloc();
  ke = &KERNELS[kid];
  ke->output_dtype = DT_FP32;
  // Same dtype for both inputs; the lifter's slot-disambiguating
  // uop_buffer_inst keeps them as distinct UOp DAG terms.
  kernel_inputs_reserve(ke, 2);
  ke->n_inputs = 2;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  u32 r1     = emit_range_axis(ke, 0 /*LOOP*/, 16);
  u32 in0p   = rangeify_emit_leaf(ke, S_DEFINE_PARAM, DT_FP32, 0);
  u32 in1p   = rangeify_emit_leaf(ke, S_DEFINE_PARAM, DT_FP32, 1);
  u32 out_d2 = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 idx0_s[2] = { in0p, r1 };
  u32 idx0   = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx0_s, 0);
  u32 ld0    = rangeify_emit(ke, S_LOAD, DT_FP32, 1, &idx0, 0);
  u32 idx1_s[2] = { in1p, r1 };
  u32 idx1   = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx1_s, 0);
  u32 ld1    = rangeify_emit(ke, S_LOAD, DT_FP32, 1, &idx1, 0);
  u32 c2     = rangeify_emit_leaf(ke, S_CONST, DT_FP32, 0x40000000u);
  u32 mul    = rangeify_emit_binary(ke, S_MUL, DT_FP32, ld1, c2);
  u32 add    = rangeify_emit_binary(ke, S_ADD, DT_FP32, ld0, mul);
  u32 idx_o_s[2] = { out_d2, r1 };
  u32 idx_o  = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_o_s, 0);
  u32 st_s[2] = { idx_o, add };
  u32 store2 = rangeify_emit(ke, S_STORE, DT_FP32, 2, st_s, 0);
  u32 buf_s[2] = { store2, r1 };
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_s, 0);

  KernelUopLift lift2 = {0};
  CHECK_EQ(kernel_lift_to_uop(ke, &lift2), 1);
  CHECK_EQ(lift2.n_inputs, 2u);

  char buf2[4096];
  fp = fmemopen(buf2, sizeof(buf2), "w");
  cg_render_uop_kernel(lift2.store_root, "k_ew", lift2.out_buf,
                       lift2.in_bufs, lift2.n_inputs, fp);
  fclose(fp);
  CHECK(contains(buf2, "device const float *in0"));
  CHECK(contains(buf2, "device const float *in1"));
  CHECK(contains(buf2, "for (uint a0 = 0; a0 < 16"));
  CHECK(contains(buf2, "in0[a0]"));
  CHECK(contains(buf2, "in1[a0]"));
  CHECK(contains(buf2, "* 2.000000f"));
  CHECK(contains(buf2, " + "));

  TEST_BEGIN("kernel-lift/reduce-sum-kernel");
  // STORE(INDEX(OUT, r_out), REDUCE_SUM(LOAD(in0[r_out, r_red]), r_red))
  // Reset arena: start fresh kernel.
  kid = kernel_alloc();
  ke = &KERNELS[kid];
  ke->output_dtype = DT_FP32;
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs = 1;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_tids[0]   = 0;

  u32 r_out  = emit_range_axis(ke, 0 /*LOOP*/, 8);
  u32 r_red  = emit_range_axis(ke, 1 /*REDUCE*/, 16);
  u32 in_p   = rangeify_emit_leaf(ke, S_DEFINE_PARAM, DT_FP32, 0);
  u32 out_d3 = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 idx_in_s[3] = { in_p, r_out, r_red };
  u32 idx_in = rangeify_emit(ke, S_INDEX, DT_FP32, 3, idx_in_s, 0);
  u32 ld_in  = rangeify_emit(ke, S_LOAD, DT_FP32, 1, &idx_in, 0);
  u32 red_s[2] = { ld_in, r_red };
  u32 red    = rangeify_emit(ke, S_REDUCE_SUM, DT_FP32, 2, red_s, 0);
  u32 idx_out_s[2] = { out_d3, r_out };
  u32 idx_out = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_out_s, 0);
  u32 st3_s[2] = { idx_out, red };
  u32 store3 = rangeify_emit(ke, S_STORE, DT_FP32, 2, st3_s, 0);
  // BUFFERIZE includes both ranges.
  u32 buf3_s[3] = { store3, r_out, r_red };
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 3, buf3_s, 0);

  KernelUopLift lift3 = {0};
  CHECK_EQ(kernel_lift_to_uop(ke, &lift3), 1);

  char buf3[4096];
  fp = fmemopen(buf3, sizeof(buf3), "w");
  cg_render_uop_kernel(lift3.store_root, "k_red", lift3.out_buf,
                       lift3.in_bufs, lift3.n_inputs, fp);
  fclose(fp);
  CHECK(contains(buf3, "for (uint a0 = 0; a0 < 8"));
  CHECK(contains(buf3, "float _acc1 = 0.0f"));
  CHECK(contains(buf3, "for (uint a1 = 0; a1 < 16"));
  CHECK(contains(buf3, "= _acc1;"));

  thvm_free();
  TEST_REPORT();
}
