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
  CHECK(contains(buf2, "device float *out"));
  CHECK(contains(buf2, "[[ buffer(0) ]]"));
  CHECK(contains(buf2, "thread_position_in_grid"));
  CHECK(contains(buf2, "= 1.0f;"));    // CONST literal
  CHECK(contains(buf2, "[a0]"));            // RANGE addr

  TEST_BEGIN("render-uop/multi-input-signature");
  // Output + 2 inputs.  Use distinct shapes so each hash-cons to a
  // unique heap loc (otherwise output and in0 can collide on (scope,
  // dtype, ndim, dims) and confuse the buffer-name map).
  u32 dims_in0[1] = { 64 };
  u32 dims_in1[1] = { 128 };
  Term in0 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_in0);
  Term in1 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP16, 1, dims_in1);
  Term in_bufs[2] = { in0, in1 };
  Term load0 = uop_index_e(in0, r);
  Term st_load = uop_store(out, r, load0);
  char buf3[2048];
  fp = fmemopen(buf3, sizeof(buf3), "w");
  cg_render_uop_kernel(st_load, "copy", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf3, "device const float *in0"));
  CHECK(contains(buf3, "[[ buffer(1) ]]"));
  CHECK(contains(buf3, "device const half *in1"));
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
  CHECK(contains(buf7, "device half *out"));

  TEST_BEGIN("render-uop/elementwise-add-mul");
  // STORE(out, addr, ADD(LOAD(in0, addr), MUL(LOAD(in1, addr), CONST(2)))).
  Term ld0_ew = uop_index_e(in0, r);
  Term load1 = uop_index_e(in1, r);
  Term two   = uop_const(DT_FP32, 0x40000000u); // 2.0f
  Term mul_v = uop_binary(UOP_MUL, load1, two);
  Term sum_v = uop_binary(UOP_ADD, ld0_ew, mul_v);
  Term st_ew = uop_store(out, r, sum_v);
  char buf8[2048];
  fp = fmemopen(buf8, sizeof(buf8), "w");
  cg_render_uop_kernel(st_ew, "k_ew", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf8, " + ("));
  CHECK(contains(buf8, " * 2.0f"));

  TEST_BEGIN("render-uop/unary-neg-recip-sqrt");
  Term load_a = uop_index_e(in0, r);
  Term neg_v  = uop_unary(UOP_NEG,   load_a);
  Term rec_v  = uop_unary(UOP_RECIP, load_a);
  Term sq_v   = uop_unary(UOP_SQRT,  load_a);
  Term sum1   = uop_binary(UOP_ADD, neg_v, rec_v);
  Term sum2   = uop_binary(UOP_ADD, sum1, sq_v);
  Term st_un  = uop_store(out, r, sum2);
  char buf9[2048];
  fp = fmemopen(buf9, sizeof(buf9), "w");
  cg_render_uop_kernel(st_un, "k_un", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf9, "(-in0"));        // NEG
  CHECK(contains(buf9, "(1.0f/in0"));    // RECIP
  CHECK(contains(buf9, "sqrt(in0"));     // SQRT builtin

  TEST_BEGIN("render-uop/exp2-log2-builtins");
  Term ex_v  = uop_unary(UOP_EXP2, load_a);
  Term lg_v  = uop_unary(UOP_LOG2, load_a);
  Term elw   = uop_binary(UOP_ADD, ex_v, lg_v);
  Term st_el = uop_store(out, r, elw);
  char buf10[2048];
  fp = fmemopen(buf10, sizeof(buf10), "w");
  cg_render_uop_kernel(st_el, "k_el", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf10, "exp2(in0"));
  CHECK(contains(buf10, "log2(in0"));

  TEST_BEGIN("render-uop/range-wraps-store-in-for-loops");
  // Single-axis kernel: STORE(out, RANGE(0), CONST) emits one for-loop.
  Term r1ax    = uop_range(0, 0, 32);
  Term st_1ax  = uop_store(out, r1ax, one);
  char bufrw1[2048];
  fp = fmemopen(bufrw1, sizeof(bufrw1), "w");
  cg_render_uop_kernel(st_1ax, "k_1ax", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufrw1, "for (uint a0 = 0; a0 < 32"));
  CHECK(contains(bufrw1, "}\n}"));   // closing brace + kernel close

  TEST_BEGIN("render-uop/two-range-axes-nested-loops");
  // STORE(out, RANGE(0)*8 + RANGE(1), CONST) emits two nested loops.
  Term r2_0 = uop_range(0, 0, 4);
  Term r2_1 = uop_range(1, 0, 8);
  Term k8   = uop_const(DT_INT32, 8);
  Term mul2 = uop_int_binary(UOP_IMUL, r2_0, k8);
  Term add2 = uop_int_binary(UOP_IADD, mul2, r2_1);
  Term st_2 = uop_store(out, add2, one);
  char bufrw2[2048];
  fp = fmemopen(bufrw2, sizeof(bufrw2), "w");
  cg_render_uop_kernel(st_2, "k_2ax", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufrw2, "for (uint a0 = 0; a0 < 4"));
  CHECK(contains(bufrw2, "for (uint a1 = 0; a1 < 8"));

  TEST_BEGIN("render-uop/unroll-pragma-on-opt-annotated-range");
  // STORE(out, OPT(RANGE(0, REDUCE, 16), UNROLL, 4), CONST) emits
  // `#pragma unroll(4)` above the corresponding for-loop.
  Term r_un  = uop_range(0, 1 /*REDUCE*/, 16);
  Term r_op  = uop_opt(r_un, UOP_OPT_UNROLL, 4);
  Term st_un_loop = uop_store(out, r_op, one);
  char bufun[2048];
  fp = fmemopen(bufun, sizeof(bufun), "w");
  cg_render_uop_kernel(st_un_loop, "k_unroll", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufun, "#pragma unroll(4)"));
  CHECK(contains(bufun, "for (uint a0 = 0; a0 < 16"));

  TEST_BEGIN("render-uop/unroll-pragma-zero-factor-bare-pragma");
  // factor=0 emits bare `#pragma unroll` (full unroll).
  Term r_un2 = uop_range(1, 0 /*LOOP*/, 8);
  Term r_op2 = uop_opt(r_un2, UOP_OPT_UNROLL, 0);
  Term st_un2 = uop_store(out, r_op2, one);
  char bufun2[2048];
  fp = fmemopen(bufun2, sizeof(bufun2), "w");
  cg_render_uop_kernel(st_un2, "k_unroll0", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufun2, "#pragma unroll\n"));

  TEST_BEGIN("render-uop/local-bind-via-opt-emits-tt");
  // OPT(RANGE(LOOP, 64), LOCAL, 0) -> `uint a0 = tt;` (no for-loop).
  Term r_lc  = uop_range(0, 0 /*LOOP*/, 64);
  Term lc_op = uop_opt(r_lc, UOP_OPT_LOCAL, 0);
  Term st_lc = uop_store(out, lc_op, one);
  char buflc[2048];
  fp = fmemopen(buflc, sizeof(buflc), "w");
  cg_render_uop_kernel(st_lc, "k_local", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(buflc, "uint a0 = tt"));
  // No for-loop wrapping the local-bound axis.
  CHECK(!contains(buflc, "for (uint a0 ="));

  TEST_BEGIN("render-uop/upcast-emits-pragma-unroll");
  Term r_uc  = uop_range(1, 0 /*LOOP*/, 4);
  Term uc_op = uop_opt(r_uc, UOP_OPT_UPCAST, 4);
  Term st_uc = uop_store(out, uc_op, one);
  char bufuc[2048];
  fp = fmemopen(bufuc, sizeof(bufuc), "w");
  cg_render_uop_kernel(st_uc, "k_upcast", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufuc, "#pragma unroll(4)"));
  CHECK(contains(bufuc, "for (uint a1 = 0; a1 < 4"));

  TEST_BEGIN("render-uop/reduce-sum-as-store-value");
  // STORE(out, RANGE(0), REDUCE(LOAD(in0, RANGE(1)), SUM, axis=1))
  // Output axis 0 wraps a SUM reduction over axis 1.  Renderer hoists
  // an accumulator outside the reduce loop, references it in the store.
  Term r_out = uop_range(0, 0 /*LOOP*/, 32);
  Term r_red_ax = uop_range(1, 1 /*REDUCE*/, 16);
  Term ld_red_in = uop_index_e(in0, r_red_ax);
  Term red_sum  = uop_reduce(REDUCE_SUM, /*axis=*/1, ld_red_in);
  Term st_red_sum = uop_store(out, r_out, red_sum);
  char bufrs[2048];
  fp = fmemopen(bufrs, sizeof(bufrs), "w");
  cg_render_uop_kernel(st_red_sum, "k_sum", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(bufrs, "for (uint a0 = 0; a0 < 32"));
  CHECK(contains(bufrs, "float _acc1 = 0.0f"));
  CHECK(contains(bufrs, "for (uint a1 = 0; a1 < 16"));
  CHECK(contains(bufrs, "_acc1 = _acc1 + in0"));
  CHECK(contains(bufrs, "] = _acc1;"));

  TEST_BEGIN("render-uop/tc-pattern-match-emits-simdgroup");
  // Build matmul shape: STORE(C, m*N+n,
  //   OPT(REDUCE(MUL(A[m*K+k], B[k*N+n]), SUM, k), TC, 0)).
  // K extent = 32 (divisible by 8) -> simdgroup template fires.
  u32 dimsA[1] = { 16*32 };
  u32 dimsB[1] = { 32*16 };
  Term A = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsA);
  Term B = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsB);
  Term in_ab[2] = { A, B };
  // Output dims (M, N) and reduce dim K.
  Term r_m = uop_range(0, 0 /*LOOP*/, 16);
  Term r_n = uop_range(1, 0, 16);
  Term r_k = uop_range(2, 1 /*REDUCE*/, 32);
  Term k16_tc = uop_const(DT_INT32, 16);
  Term k32_tc = uop_const(DT_INT32, 32);
  // A[m*K + k]
  Term mK    = uop_int_binary(UOP_IMUL, r_m, k32_tc);
  Term addrA = uop_int_binary(UOP_IADD, mK, r_k);
  Term ldA   = uop_index_e(A, addrA);
  // B[k*N + n]
  Term kN    = uop_int_binary(UOP_IMUL, r_k, k16_tc);
  Term addrB = uop_int_binary(UOP_IADD, kN, r_n);
  Term ldB   = uop_index_e(B, addrB);
  // MUL + REDUCE(SUM, k_axis=2)
  Term mul_tc = uop_binary(UOP_MUL, ldA, ldB);
  Term redM  = uop_reduce(REDUCE_SUM, /*axis=*/2, mul_tc);
  Term tc_v  = uop_opt(redM, UOP_OPT_TC, 0);
  // C[m*N + n]
  Term mN    = uop_int_binary(UOP_IMUL, r_m, k16_tc);
  Term addrC = uop_int_binary(UOP_IADD, mN, r_n);
  Term st_tc = uop_store(out, addrC, tc_v);
  char buftc[2048];
  fp = fmemopen(buftc, sizeof(buftc), "w");
  cg_render_uop_kernel(st_tc, "k_gemm", out, in_ab, 2, fp);
  fclose(fp);
  CHECK(contains(buftc, "/* TC simdgroup_matrix matmul"));
  CHECK(contains(buftc, "simdgroup_matrix<float, 8, 8> _a_mat"));
  CHECK(contains(buftc, "simdgroup_matrix<float, 8, 8> _b_mat"));
  CHECK(contains(buftc, "simdgroup_matrix<float, 8, 8> _c_mat"));
  CHECK(contains(buftc, "simdgroup_load(_a_mat"));
  CHECK(contains(buftc, "simdgroup_load(_b_mat"));
  CHECK(contains(buftc, "simdgroup_multiply_accumulate(_c_mat"));
  CHECK(contains(buftc, "simdgroup_store(_c_mat"));
  CHECK(contains(buftc, "for (uint a2 = 0; a2 < 32; a2 += 8)"));
  // Outer loops over M, N still emitted.
  CHECK(contains(buftc, "for (uint a0 = 0; a0 < 16"));
  CHECK(contains(buftc, "for (uint a1 = 0; a1 < 16"));

  TEST_BEGIN("render-uop/tc-non-multiple-of-8-falls-back");
  // K extent = 7 (not divisible by 8) -> F2b skips, F1e accumulator.
  Term r_k_bad = uop_range(3, 1, 7);
  Term mK_bad    = uop_int_binary(UOP_IMUL, r_m, uop_const(DT_INT32, 7));
  Term addrA_bad = uop_int_binary(UOP_IADD, mK_bad, r_k_bad);
  Term ldA_bad   = uop_index_e(A, addrA_bad);
  Term kN_bad    = uop_int_binary(UOP_IMUL, r_k_bad, k16_tc);
  Term addrB_bad = uop_int_binary(UOP_IADD, kN_bad, r_n);
  Term ldB_bad   = uop_index_e(B, addrB_bad);
  Term mul_bad   = uop_binary(UOP_MUL, ldA_bad, ldB_bad);
  Term red_bad   = uop_reduce(REDUCE_SUM, /*axis=*/3, mul_bad);
  Term tc_bad    = uop_opt(red_bad, UOP_OPT_TC, 0);
  Term st_bad    = uop_store(out, addrC, tc_bad);
  char buftc2[2048];
  fp = fmemopen(buftc2, sizeof(buftc2), "w");
  cg_render_uop_kernel(st_bad, "k_gemm_bad", out, in_ab, 2, fp);
  fclose(fp);
  CHECK(contains(buftc2, "/* TC tile mismatch"));
  CHECK(contains(buftc2, "_acc3 = _acc3 + ("));

  TEST_BEGIN("render-uop/reduce-max-uses-fmax");
  Term ld_max_in = uop_index_e(in0, r_red_ax);
  Term red_max = uop_reduce(REDUCE_MAX, /*axis=*/1, ld_max_in);
  Term st_red_max = uop_store(out, r_out, red_max);
  char bufrm[2048];
  fp = fmemopen(bufrm, sizeof(bufrm), "w");
  cg_render_uop_kernel(st_red_max, "k_max", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(bufrm, "= -INFINITY"));
  CHECK(contains(bufrm, "fmax(_acc1, "));

  TEST_BEGIN("render-uop/legacy-kax-global-emits-tg");
  // Direct axis_type=5 (legacy KAX_GLOBAL) without OPT also emits tg.
  Term r_gl  = uop_range(2, 5 /*GLOBAL*/, 32);
  Term st_gl = uop_store(out, r_gl, one);
  char bufgl[2048];
  fp = fmemopen(bufgl, sizeof(bufgl), "w");
  cg_render_uop_kernel(st_gl, "k_global", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufgl, "uint a2 = tg"));

  TEST_BEGIN("render-uop/reduce-axis-marked");
  // RANGE with axis_type=1 (REDUCE) emits /*reduce*/ comment.
  Term r_red   = uop_range(2, 1 /*REDUCE*/, 16);
  Term ld_red  = uop_index_e(in0, r_red);
  Term st_red  = uop_store(out, r_red, ld_red);
  char bufred[2048];
  fp = fmemopen(bufred, sizeof(bufred), "w");
  cg_render_uop_kernel(st_red, "k_red2", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(bufred, "/*reduce*/"));

  TEST_BEGIN("render-uop/cmplt-cmpeq-comparison");
  Term lt    = uop_binary(UOP_CMPLT, load_a, two);
  Term eq    = uop_binary(UOP_CMPEQ, load_a, two);
  Term st_lt = uop_store(out, r, lt);
  Term st_eq = uop_store(out, r, eq);
  char buf11[2048];
  fp = fmemopen(buf11, sizeof(buf11), "w");
  cg_render_uop_kernel(st_lt, "k_lt", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf11, " < 2.0f"));
  fp = fmemopen(buf11, sizeof(buf11), "w");
  cg_render_uop_kernel(st_eq, "k_eq", out, in_bufs, 2, fp);
  fclose(fp);
  CHECK(contains(buf11, " == 2.0f"));

  thvm_free();
  TEST_REPORT();
}
