// test_kernel_lift.c - Phase C wedge: ScalarUop arena -> UOp DAG.
//
// Builds synthetic kernel programs in the scalar_uops arena (mirrors
// what rangeify produces from a real schedule), runs kernel_lift_to_uop,
// and verifies the resulting UOp DAG renders to valid MSL via the
// existing F0-F3 renderer.

#include "../src/thvm.c"
#include "test.h"
#include <unistd.h>

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

static int xcrun_metal_available(void) {
  return system("xcrun -f metal >/dev/null 2>&1") == 0;
}

static int compile_through_metal(const char *msl_text) {
  char path[64];
  snprintf(path, sizeof(path), "/tmp/thvm_lift_%d.metal", getpid());
  FILE *fp = fopen(path, "w");
  if (fp == NULL) return -1;
  fputs(msl_text, fp);
  fclose(fp);
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
           "xcrun metal -x metal -c %s -o /dev/null 2>/dev/null", path);
  int rc = system(cmd);
  unlink(path);
  return WEXITSTATUS(rc);
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
  CHECK(contains(buf, "if (tid >= 32u) return;"));
  CHECK(contains(buf, "uint a0 = tid;"));
  CHECK(!contains(buf, "for (uint a0 ="));
  CHECK(contains(buf, "out[a0] = 1.0f"));
  if (xcrun_metal_available()) CHECK_EQ(compile_through_metal(buf), 0);

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
  CHECK(contains(buf2, "if (tid >= 16u) return;"));
  CHECK(contains(buf2, "uint a0 = tid;"));
  CHECK(!contains(buf2, "for (uint a0 ="));
  CHECK(contains(buf2, "in0[a0]"));
  CHECK(contains(buf2, "in1[a0]"));
  CHECK(contains(buf2, "* 2.0f"));
  CHECK(contains(buf2, " + "));
  if (xcrun_metal_available()) CHECK_EQ(compile_through_metal(buf2), 0);

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
  CHECK(contains(buf3, "uint a0 = tid;"));
  CHECK(!contains(buf3, "for (uint a0 ="));
  CHECK(contains(buf3, "float _acc1 = 0.0f"));
  CHECK(contains(buf3, "for (uint a1 = 0; a1 < 16"));
  CHECK(contains(buf3, "= _acc1;"));
  if (xcrun_metal_available()) CHECK_EQ(compile_through_metal(buf3), 0);

  TEST_BEGIN("kernel-lift/flip-axis-rewrites-iter");
  // STORE(INDEX(OUT, r), FLIP(LOAD(IN[r]), bitmask=1)) -- when axis 0
  // is flipped, the load reads in0[(extent-1)-r] instead of in0[r].
  kid = kernel_alloc();
  ke = &KERNELS[kid];
  ke->output_dtype = DT_FP32;
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs = 1;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_tids[0]   = 0;

  u32 rf      = emit_range_axis(ke, 0 /*LOOP*/, 16);
  u32 in_pf   = rangeify_emit_leaf(ke, S_DEFINE_PARAM, DT_FP32, 0);
  u32 out_df  = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 idx_in_f[2] = { in_pf, rf };
  u32 idx_in_fid  = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_in_f, 0);
  u32 ld_f    = rangeify_emit(ke, S_LOAD, DT_FP32, 1, &idx_in_fid, 0);
  u32 flip_s[2] = { ld_f, rf };
  u32 flip    = rangeify_emit(ke, S_FLIP, DT_FP32, 2, flip_s, /*mask=*/1);
  u32 idx_out_f[2] = { out_df, rf };
  u32 idx_out_fid  = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_out_f, 0);
  u32 st_f_s[2] = { idx_out_fid, flip };
  u32 st_f    = rangeify_emit(ke, S_STORE, DT_FP32, 2, st_f_s, 0);
  u32 buf_f_s[2] = { st_f, rf };
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_f_s, 0);

  KernelUopLift lift_f = {0};
  CHECK_EQ(kernel_lift_to_uop(ke, &lift_f), 1);
  char buf_flip[2048];
  fp = fmemopen(buf_flip, sizeof(buf_flip), "w");
  cg_render_uop_kernel(lift_f.store_root, "k_flip", lift_f.out_buf,
                       lift_f.in_bufs, lift_f.n_inputs, fp);
  fclose(fp);
  // The flipped read uses (extent-1 - a0) = (15 - a0).
  CHECK(contains(buf_flip, "(15 - a0)"));
  CHECK(contains(buf_flip, "out[a0]"));

  TEST_BEGIN("kernel-lift/shrink-shifts-iter");
  // STORE(INDEX(OUT, r), SHRINK(LOAD(IN[r]), begin=4)) -- the load
  // reads in0[r + 4], not in0[r].
  kid = kernel_alloc();
  ke = &KERNELS[kid];
  ke->output_dtype = DT_FP32;
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs = 1;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_tids[0]   = 0;

  u32 rs       = emit_range_axis(ke, 0 /*LOOP*/, 8);
  u32 in_ps    = rangeify_emit_leaf(ke, S_DEFINE_PARAM, DT_FP32, 0);
  u32 out_ds   = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 idx_in_s2[2] = { in_ps, rs };
  u32 idx_in_sid   = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_in_s2, 0);
  u32 ld_s     = rangeify_emit(ke, S_LOAD, DT_FP32, 1, &idx_in_sid, 0);
  // SHRINK with begin=4 (packed at bits 0..15 of extra).
  u32 shr_s[2] = { ld_s, rs };
  u32 shr      = rangeify_emit(ke, S_SHRINK, DT_FP32, 2, shr_s, /*extra=*/4);
  u32 idx_out_s2[2] = { out_ds, rs };
  u32 idx_out_sid   = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_out_s2, 0);
  u32 st_s2[2] = { idx_out_sid, shr };
  u32 st_sid   = rangeify_emit(ke, S_STORE, DT_FP32, 2, st_s2, 0);
  u32 buf_s2[2] = { st_sid, rs };
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, buf_s2, 0);

  KernelUopLift lift_s = {0};
  CHECK_EQ(kernel_lift_to_uop(ke, &lift_s), 1);
  char buf_shr[2048];
  fp = fmemopen(buf_shr, sizeof(buf_shr), "w");
  cg_render_uop_kernel(lift_s.store_root, "k_shrink", lift_s.out_buf,
                       lift_s.in_bufs, lift_s.n_inputs, fp);
  fclose(fp);
  CHECK(contains(buf_shr, "(a0 + 4)"));

  TEST_BEGIN("kernel-lift/reshape-v-decomposes-flat-idx");
  // STORE(INDEX(OUT, r0, r1), RESHAPE_V(LOAD(IN[r_in]), N_out=2))
  // 2D output (r0:4 x r1:4) reshapes a 1D input range r_in:16.
  // Lifter computes flat_idx = r0*4 + r1, then r_in = flat_idx.
  kid = kernel_alloc();
  ke = &KERNELS[kid];
  ke->output_dtype = DT_FP32;
  kernel_inputs_reserve(ke, 1);
  ke->n_inputs = 1;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_tids[0]   = 0;

  u32 r0v = emit_range_axis(ke, 0 /*LOOP*/, 4);
  u32 r1v = emit_range_axis(ke, 0 /*LOOP*/, 4);
  u32 r_in = emit_range_axis(ke, 0 /*LOOP*/, 16);

  u32 in_pv  = rangeify_emit_leaf(ke, S_DEFINE_PARAM, DT_FP32, 0);
  u32 out_dv = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);

  u32 idx_in_v[2] = { in_pv, r_in };
  u32 idx_in_vid  = rangeify_emit(ke, S_INDEX, DT_FP32, 2, idx_in_v, 0);
  u32 ld_v   = rangeify_emit(ke, S_LOAD, DT_FP32, 1, &idx_in_vid, 0);
  // RESHAPE_V: src[0]=ld, src[1..3]=out iters (r0v, r1v),
  //             src[3..]=in iters (r_in); extra=N_out=2
  u32 rv_s[4] = { ld_v, r0v, r1v, r_in };
  u32 rv     = rangeify_emit(ke, S_RESHAPE_V, DT_FP32, 4, rv_s, /*N_out=*/2);

  u32 idx_out_v[3] = { out_dv, r0v, r1v };
  u32 idx_out_vid  = rangeify_emit(ke, S_INDEX, DT_FP32, 3, idx_out_v, 0);
  u32 st_v_s[2] = { idx_out_vid, rv };
  u32 st_v   = rangeify_emit(ke, S_STORE, DT_FP32, 2, st_v_s, 0);
  u32 buf_v_s[3] = { st_v, r0v, r1v };
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 3, buf_v_s, 0);

  KernelUopLift lift_v = {0};
  CHECK_EQ(kernel_lift_to_uop(ke, &lift_v), 1);
  char buf_rv[2048];
  fp = fmemopen(buf_rv, sizeof(buf_rv), "w");
  cg_render_uop_kernel(lift_v.store_root, "k_resh", lift_v.out_buf,
                       lift_v.in_bufs, lift_v.n_inputs, fp);
  fclose(fp);
  // The flat_idx is (a0 * 4 + a1); the input range gets that mod 16
  // (which simplifier may reduce since the value is already in [0,16)).
  CHECK(contains(buf_rv, "(a0 * 4)"));
  CHECK(contains(buf_rv, "if (tid >= 16u) return;"));
  CHECK(contains(buf_rv, "uint a0 = (tid / 4u) % 4u;"));
  CHECK(contains(buf_rv, "uint a1 = tid % 4u;"));
  CHECK(!contains(buf_rv, "for (uint a0 ="));
  CHECK(!contains(buf_rv, "for (uint a1 ="));

  thvm_free();
  TEST_REPORT();
}
