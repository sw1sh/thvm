// test_render_uop.c - Phase F0: UOp DAG renderer skeleton.
//
// Validates the renderer-rewrite seam.  Walks UOp DAG rooted at a
// UOP_STORE / UOP_AFTER chain and emits pseudo-MSL with the right
// kernel signature, address expressions, and barrier insertion.
// Replaces test_tile_render_msl when Phase G deletes TileUop[].

#include "../src/thvm.c"
#include "test.h"

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("render-uop/empty-root-prints-empty");
  char buf[2048];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  CHECK(fp != NULL);
  u32 dims[1] = { 32 };
  Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims);
  cg_render_uop_kernel(/*root=*/0, "k0", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buf, "/* empty kernel */"));
  CHECK(contains(buf, "kernel void k0"));

  TEST_BEGIN("render-uop/const-fill-store");
  // Build STORE(out, RANGE(0, LOOP, 32), CONST(1.0f)).
  Term r       = uop_range(0, 0 /*LOOP*/, 32);
  Term one     = uop_const(DT_FP32, 0x3F800000u);
  Term st      = uop_store(out, r, one);
  char buf2[2048];
  fp = fmemopen(buf2, sizeof(buf2), "w");
  cg_render_uop_kernel(st, "fill", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buf2, "#include <metal_stdlib>"));
  CHECK(contains(buf2, "kernel void fill"));
  CHECK(contains(buf2, "device float *buf"));
  CHECK(contains(buf2, "[[ buffer(0) ]]"));
  CHECK(contains(buf2, "thread_position_in_grid"));
  CHECK(contains(buf2, "= 1.000000f;"));    // CONST literal
  CHECK(contains(buf2, "[a0]"));            // RANGE addr

  TEST_BEGIN("render-uop/multi-input-signature");
  // Output + 2 inputs.
  u32 dims_in[1] = { 32 };
  Term in0 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in);
  Term in1 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP16, 1, dims_in);
  Term in_bufs[2] = { in0, in1 };
  Term load0 = uop_index_e(in0, r);
  Term st_load = uop_store(out, r, load0);
  char buf3[2048];
  fp = fmemopen(buf3, sizeof(buf3), "w");
  cg_render_uop_kernel(st_load, "copy", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf3, "device const float *buf"));
  CHECK(contains(buf3, "[[ buffer(1) ]]"));
  CHECK(contains(buf3, "device const half *buf"));
  CHECK(contains(buf3, "[[ buffer(2) ]]"));

  TEST_BEGIN("render-uop/index-arithmetic-emits");
  // STORE(out, RANGE(0)*2 + RANGE(1), value).  Tests the symbolic
  // int expression emitter (UOP_IADD, UOP_IMUL).
  Term r0   = uop_range(0, 0, 8);
  Term r1   = uop_range(1, 0, 4);
  Term k2   = uop_const(DT_INT32, 2);
  Term mul  = uop_int_binary(UOP_IMUL, r0, k2);
  Term addr = uop_int_binary(UOP_IADD, mul, r1);
  Term st_idx = uop_store(out, addr, one);
  char buf4[2048];
  fp = fmemopen(buf4, sizeof(buf4), "w");
  cg_render_uop_kernel(st_idx, "k_idx", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buf4, "(a0 * 2)"));
  CHECK(contains(buf4, " + a1)"));

  TEST_BEGIN("render-uop/after-chain-emits-barrier-on-local-store");
  // LOCAL store followed by GLOBAL store -> barrier between.
  u32 lc_dims[1] = { 32 };
  Term lc      = uop_buffer(UOP_SCOPE_LOCAL,  DT_FP32, 1, lc_dims);
  Term lc_st   = uop_store(lc, r, one);
  Term gl_load = uop_index_e(lc, r);
  Term gl_st   = uop_store(out, r, gl_load);
  Term ord     = uop_after(gl_st, lc_st);
  char buf5[2048];
  fp = fmemopen(buf5, sizeof(buf5), "w");
  cg_render_uop_kernel(ord, "k_red", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buf5, "threadgroup_barrier"));
  CHECK(contains(buf5, "mem_flags::mem_threadgroup"));

  TEST_BEGIN("render-uop/iwhere-emits-ternary");
  // STORE(out, RANGE(0), IWHERE(RANGE(0) < 4, CONST(1.0), CONST(0.0))).
  Term k4   = uop_const(DT_INT32, 4);
  Term cond = uop_int_binary(UOP_ILT, r, k4);
  Term zero = uop_const(DT_FP32, 0u);
  Term sel  = uop_iwhere(cond, one, zero);
  Term st_w = uop_store(out, r, sel);
  char buf6[2048];
  fp = fmemopen(buf6, sizeof(buf6), "w");
  cg_render_uop_kernel(st_w, "k_w", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buf6, " ? "));
  CHECK(contains(buf6, " : "));
  CHECK(contains(buf6, "(a0 < 4)"));

  TEST_BEGIN("render-uop/dtype-mapping-fp16-half");
  Term out16 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP16, 1, dims);
  Term st16  = uop_store(out16, r, one);
  char buf7[2048];
  fp = fmemopen(buf7, sizeof(buf7), "w");
  cg_render_uop_kernel(st16, "k_h", out16, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buf7, "device half *buf"));

  thvm_free();
  TEST_REPORT();
}
