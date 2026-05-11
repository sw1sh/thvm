// test_render_uop.c - Phase F0: UOp DAG renderer skeleton.
//
// Validates the renderer-rewrite seam.  Walks UOp DAG rooted at a
// UOP_STORE / UOP_AFTER chain and emits pseudo-MSL with the right
// kernel signature, address expressions, and barrier insertion.
// Replaces test_tile_render_msl when Phase G deletes TileUop[].

#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

static int contains(const char *haystack, const char *needle) {
  return strstr(haystack, needle) != NULL;
}

// Write `msl` to a temp file and run `xcrun metal -x metal -c`; returns
// 0 if it compiled (or xcrun unavailable -- skip rather than fail).
static int xcrun_metal_available(void) {
  return system("xcrun -f metal >/dev/null 2>&1") == 0;
}
static int compile_msl_xcrun(const char *msl) {
  char path[] = "/tmp/thvm_render_uop_msl_XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) return -1;
  FILE *f = fdopen(fd, "w");
  if (f == NULL) { close(fd); unlink(path); return -1; }
  fputs(msl, f);
  fclose(f);
  char cmd[512];
  snprintf(cmd, sizeof(cmd),
           "xcrun metal -x metal -c %s -o /dev/null 2>/dev/null", path);
  int rc = system(cmd);
  unlink(path);
  return rc == 0 ? 0 : 1;
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

  TEST_BEGIN("render-uop/single-output-axis-decoded-from-tid");
  // Single-axis kernel: STORE(out, RANGE(0), CONST).  Output axes are
  // promoted to parallel grid axes -- emit `uint a0 = tid;` plus the
  // `if (tid >= N) return;` bounds guard, NOT a serial for-loop.
  Term r1ax    = uop_range(0, 0, 32);
  Term st_1ax  = uop_store(out, r1ax, one);
  char bufrw1[2048];
  fp = fmemopen(bufrw1, sizeof(bufrw1), "w");
  cg_render_uop_kernel(st_1ax, "k_1ax", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufrw1, "if (tid >= 32u) return;"));
  CHECK(contains(bufrw1, "uint a0 = tid;"));
  CHECK(!contains(bufrw1, "for (uint a0 ="));

  TEST_BEGIN("render-uop/two-output-axes-flat-tid-decode");
  // STORE(out, RANGE(0)*8 + RANGE(1), CONST): both output axes become
  // parallel grid axes decoded from the flat tid -- a0 = (tid/8)%4,
  // a1 = tid%8 -- not nested for-loops.
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
  CHECK(contains(bufrw2, "if (tid >= 32u) return;"));
  CHECK(contains(bufrw2, "uint a0 = (tid / 8u) % 4u;"));
  CHECK(contains(bufrw2, "uint a1 = tid % 8u;"));
  CHECK(!contains(bufrw2, "for (uint a0 ="));
  CHECK(!contains(bufrw2, "for (uint a1 ="));

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
  // Output axis 0 is a parallel grid axis (`uint a0 = tid;`); the
  // reduce axis 1 stays a serial in-thread loop with the accumulator.
  CHECK(contains(bufrs, "uint a0 = tid;"));
  CHECK(!contains(bufrs, "for (uint a0 ="));
  CHECK(contains(bufrs, "float _acc1 = 0.0f"));
  // Small reduce extent (16 <= RMU_REDUCE_UNROLL_MAX) -> default
  // `#pragma unroll(16)` above the reduce for-loop so the MSL
  // compiler straight-lines the contraction MADs.
  CHECK(contains(bufrs, "#pragma unroll(16)"));
  CHECK(contains(bufrs, "for (uint a1 = 0; a1 < 16"));
  CHECK(contains(bufrs, "_acc1 = _acc1 + in0"));
  CHECK(contains(bufrs, "] = _acc1;"));

  TEST_BEGIN("render-uop/upcast-composes-with-promoted-output-axes");
  // matmul-shaped reduce STORE(C, m*N+n, REDUCE(MUL(A,B), SUM, k))
  // with KOP_UPCAST applied to the N output axis (factor 4).  After
  // the DAG split the axes are: M (KAX_LOOP), N_outer (KAX_LOOP),
  // N_inner (KAX_UPCAST, wrapped in OPT), K (KAX_REDUCE).  The
  // renderer must (a) promote M + N_outer to a flat `tid` decode,
  // (b) emit N_inner as a `#pragma unroll(4)` for-loop, (c) declare
  // the reduce accumulator INSIDE that loop and the store reference
  // it there -- a regression caught a stale REDUCE.axis that made the
  // matcher mistake N_inner for the reduce loop, leaving the store's
  // `a<N_inner>` out of scope ("undeclared identifier").
  {
    u32 dC_uc[2] = { 32, 64 };
    u32 dA_uc[2] = { 32, 8  };
    u32 dB_uc[2] = { 8,  64 };
    Term C_uc = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC_uc);
    Term A_uc = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA_uc);
    Term B_uc = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB_uc);
    Term in_uc[2] = { A_uc, B_uc };
    Term rm  = uop_range(0, 0 /*LOOP*/,   32);
    Term rn  = uop_range(1, 0 /*LOOP*/,   64);
    Term rk  = uop_range(2, 1 /*REDUCE*/, 8);
    Term k8  = uop_const(DT_INT32, 8);
    Term k64 = uop_const(DT_INT32, 64);
    Term aA  = uop_index_e(A_uc, uop_int_binary(UOP_IADD,
                                  uop_int_binary(UOP_IMUL, rm, k8), rk));
    Term aB  = uop_index_e(B_uc, uop_int_binary(UOP_IADD,
                                  uop_int_binary(UOP_IMUL, rk, k64), rn));
    Term mm  = uop_binary(UOP_MUL, aA, aB);
    Term rd  = uop_reduce(REDUCE_SUM, /*axis=*/2, mm);
    Term aC  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, rm, k64), rn);
    Term st_uc = uop_store(C_uc, aC, rd);
    KOpt up = { KOP_UPCAST, 1 /*N axis*/, 4 };
    Term st_uc2 = uop_dag_apply_kopt(st_uc, up);
    CHECK(st_uc2 != 0 && st_uc2 != st_uc);
    char bufmu[4096];
    fp = fmemopen(bufmu, sizeof(bufmu), "w");
    cg_render_uop_kernel(st_uc2, "k_mm_up", C_uc, in_uc, 2, fp);
    fclose(fp);
    // M + N_outer promoted to a flat tid decode (total = 32*16 = 512).
    CHECK(contains(bufmu, "if (tid >= 512u) return;"));
    CHECK(contains(bufmu, "uint a0 = (tid / 16u) % 32u;"));
    CHECK(contains(bufmu, "uint a1 = tid % 16u;"));
    // N_inner (axis 2 after split) is the #pragma-unroll for-loop.
    CHECK(contains(bufmu, "#pragma unroll(4)"));
    CHECK(contains(bufmu, "for (uint a2 = 0; a2 < 4; a2++)"));
    // Accumulator named off the (shifted) reduce axis 3, declared
    // inside the a2 loop; the store references a2 in scope.
    CHECK(contains(bufmu, "float _acc3 = 0.0f"));
    CHECK(contains(bufmu, "for (uint a3 = 0; a3 < 8"));
    CHECK(contains(bufmu, "((a1 * 4) + a2)"));
    CHECK(contains(bufmu, "= _acc3;"));
    // No serial output-axis loop (the regression's symptom).
    CHECK(!contains(bufmu, "for (uint a0 ="));
    CHECK(!contains(bufmu, "for (uint a1 ="));
  }

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
  // K=7 (<= RMU_REDUCE_UNROLL_MAX) -> default reduce-loop unroll.
  CHECK(contains(buftc2, "#pragma unroll(7)"));
  CHECK(contains(buftc2, "_acc3 = _acc3 + ("));

  TEST_BEGIN("render-uop/conv-pattern-match-emits-template");
  // Build a conv2d-shaped STORE wrapped in OPT(_, CONV, 0) and
  // verify rmu_emit_conv fires (marker comment + #pragma unroll on
  // the small KRED loop).  Mirrors the kernel_lift_from_conv2d
  // emit shape: r_out compressed via IDIV, r_q the reduce axis.
  u32 dims_wc[2] = { 16, 9 };
  u32 dims_xc[1] = { 1024 };
  Term Wc = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_wc);
  Term Xc = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_xc);
  Term in_wx[2] = { Wc, Xc };
  Term r_out_c = uop_range(10, 0 /*LOOP*/, 1024);
  Term r_q_c   = uop_range(11, 1 /*REDUCE*/, 9);
  Term k_pat_c = uop_const(DT_INT32, 64);
  Term k_w_s0_c= uop_const(DT_INT32, 9);
  Term co_c    = uop_int_binary(UOP_IDIV, r_out_c, k_pat_c);
  Term wi_co_c = uop_int_binary(UOP_IMUL, co_c, k_w_s0_c);
  Term wi_c    = uop_int_binary(UOP_IADD, wi_co_c, r_q_c);
  Term ldWc    = uop_index_e(Wc, wi_c);
  Term xi_c    = uop_int_binary(UOP_IADD, co_c, r_q_c);
  Term ldXc    = uop_index_e(Xc, xi_c);
  Term mul_c   = uop_binary(UOP_MUL, ldWc, ldXc);
  Term red_c   = uop_reduce(REDUCE_SUM, /*axis=*/11, mul_c);
  Term conv_v  = uop_opt(red_c, UOP_OPT_CONV, 0);
  Term st_conv = uop_store(out, r_out_c, conv_v);
  char bufconv[2048];
  fp = fmemopen(bufconv, sizeof(bufconv), "w");
  cg_render_uop_kernel(st_conv, "k_conv", out, in_wx, 2, fp);
  fclose(fp);
  CHECK(contains(bufconv, "/* CONV2D template (KRED=9)"));
  CHECK(contains(bufconv, "#pragma unroll(9)"));
  CHECK(contains(bufconv, "for (uint a11 = 0; a11 < 9"));
  CHECK(contains(bufconv, "_acc11 = _acc11 +"));
  // The output axis a10 is a parallel grid axis (`uint a10 = tid;`)
  // with the `if (tid >= 1024u) return;` bounds guard, not a loop.
  CHECK(contains(bufconv, "if (tid >= 1024u) return;"));
  CHECK(contains(bufconv, "uint a10 = tid;"));
  CHECK(!contains(bufconv, "for (uint a10 ="));

  TEST_BEGIN("render-uop/conv-large-kred-skips-pragma-unroll");
  // Build a conv with KRED > RMU_CONV_UNROLL_MAX (64); the template
  // should still emit the marker but skip #pragma unroll.
  Term r_q_big = uop_range(12, 1, 128);
  Term wi_big  = uop_int_binary(UOP_IADD, wi_co_c, r_q_big);
  Term ldW_big = uop_index_e(Wc, wi_big);
  Term xi_big  = uop_int_binary(UOP_IADD, co_c, r_q_big);
  Term ldX_big = uop_index_e(Xc, xi_big);
  Term mul_big = uop_binary(UOP_MUL, ldW_big, ldX_big);
  Term red_big = uop_reduce(REDUCE_SUM, /*axis=*/12, mul_big);
  Term cv_big  = uop_opt(red_big, UOP_OPT_CONV, 0);
  Term st_big  = uop_store(out, r_out_c, cv_big);
  char bufbig[2048];
  fp = fmemopen(bufbig, sizeof(bufbig), "w");
  cg_render_uop_kernel(st_big, "k_conv_big", out, in_wx, 2, fp);
  fclose(fp);
  CHECK(contains(bufbig, "/* CONV2D template (KRED=128)"));
  CHECK(!contains(bufbig, "#pragma unroll(128)"));

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

  TEST_BEGIN("render-uop/local-upcast-global-decode");
  // matmul-shaped reduce STORE with KOP_UPCAST(N, 4) then
  // KOP_LOCAL(N_outer, 4).  After the two DAG splits the N axis-line is
  // [N_outer (KAX_LOOP -> promoted GLOBAL), N_local (KAX_LOCAL, ext 4),
  // N_inner (KAX_UPCAST, ext 4)] and M is KAX_LOOP -> promoted GLOBAL.
  // With a LOCAL axis present the renderer decodes the GLOBAL axes from
  // `tg` (not `tid`) and the LOCAL axis from `tt`; the bounds guard
  // becomes `tg >= prod(GLOBAL extents)`.
  {
    u32 dC_lu[2] = { 32, 64 };
    u32 dA_lu[2] = { 32, 8  };
    u32 dB_lu[2] = { 8,  64 };
    Term C_lu = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC_lu);
    Term A_lu = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA_lu);
    Term B_lu = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB_lu);
    Term in_lu[2] = { A_lu, B_lu };
    Term rm_lu = uop_range(0, 0 /*LOOP*/,   32);
    Term rn_lu = uop_range(1, 0 /*LOOP*/,   64);
    Term rk_lu = uop_range(2, 1 /*REDUCE*/, 8);
    Term k8_lu  = uop_const(DT_INT32, 8);
    Term k64_lu = uop_const(DT_INT32, 64);
    Term aA_lu  = uop_index_e(A_lu, uop_int_binary(UOP_IADD,
                                  uop_int_binary(UOP_IMUL, rm_lu, k8_lu), rk_lu));
    Term aB_lu  = uop_index_e(B_lu, uop_int_binary(UOP_IADD,
                                  uop_int_binary(UOP_IMUL, rk_lu, k64_lu), rn_lu));
    Term mm_lu  = uop_binary(UOP_MUL, aA_lu, aB_lu);
    Term rd_lu  = uop_reduce(REDUCE_SUM, /*axis=*/2, mm_lu);
    Term aC_lu  = uop_int_binary(UOP_IADD,
                                 uop_int_binary(UOP_IMUL, rm_lu, k64_lu), rn_lu);
    Term st_lu  = uop_store(C_lu, aC_lu, rd_lu);
    KOpt up_lu = { KOP_UPCAST, 1 /*N axis*/, 4 };
    Term st_lu1 = uop_dag_apply_kopt(st_lu, up_lu);
    CHECK(st_lu1 != 0 && st_lu1 != st_lu);
    // After UPCAST: axes M(0), N_outer(1, ext 16), N_inner(2, UPCAST 4),
    // K(3, REDUCE).  LOCAL-split N_outer (axis 1) by 4.
    KOpt lc_lu = { KOP_LOCAL, 1 /*N_outer*/, 4 };
    Term st_lu2 = uop_dag_apply_kopt(st_lu1, lc_lu);
    CHECK(st_lu2 != 0 && st_lu2 != st_lu1);
    char buflu[4096];
    fp = fmemopen(buflu, sizeof(buflu), "w");
    cg_render_uop_kernel(st_lu2, "k", C_lu, in_lu, 2, fp);
    fclose(fp);
    // GLOBAL grid = M(32) * N_outer(64/4/4 = 4) = 128 threadgroups,
    // decoded from `tg`.
    CHECK(contains(buflu, "if (tg >= 128u) return;"));
    CHECK(contains(buflu, "% 32u; /* global"));      // M decode off tg
    CHECK(contains(buflu, "(tg /"));                  // N_outer decode off tg
    CHECK(!contains(buflu, "tid /"));                 // nothing off tid
    // LOCAL axis (axis 2 after the second split) -> `uint a2 = tt;`
    CHECK(contains(buflu, "uint a2 = tt; /* local"));
    // UPCAST axis (axis 3) -> #pragma unroll(4) for-loop
    CHECK(contains(buflu, "#pragma unroll(4)"));
    CHECK(contains(buflu, "for (uint a3 = 0; a3 < 4; a3++)"));
    // The MSL must actually compile through xcrun metal (if available).
    if (xcrun_metal_available()) CHECK_EQ(compile_msl_xcrun(buflu), 0);
  }

  TEST_BEGIN("render-uop/two-local-axes-tt-decode");
  // matmul-shaped reduce STORE with KOP_UPCAST(N,4), then KOP_LOCAL on
  // both N_outer (4) and M (4) -> TWO KAX_LOCAL axes in the lifted DAG.
  // The renderer must decode each LOCAL axis from `tt` with its own
  // (stride, modulus): the innermost-in-ranges[] one is `tt % le`, the
  // next is `(tt / le_inner) % le`.  Threadgroup size = prod(LOCAL) =
  // 16; GLOBAL grid = M_outer(8) * N_outer(4) = 32 threadgroups (off tg).
  {
    u32 dC_2l[2] = { 32, 64 };
    u32 dA_2l[2] = { 32, 8  };
    u32 dB_2l[2] = { 8,  64 };
    Term C_2l = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC_2l);
    Term A_2l = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA_2l);
    Term B_2l = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB_2l);
    Term in_2l[2] = { A_2l, B_2l };
    Term rm_2l = uop_range(0, 0 /*LOOP*/,   32);
    Term rn_2l = uop_range(1, 0 /*LOOP*/,   64);
    Term rk_2l = uop_range(2, 1 /*REDUCE*/, 8);
    Term k8_2l  = uop_const(DT_INT32, 8);
    Term k64_2l = uop_const(DT_INT32, 64);
    Term aA_2l  = uop_index_e(A_2l, uop_int_binary(UOP_IADD,
                                  uop_int_binary(UOP_IMUL, rm_2l, k8_2l), rk_2l));
    Term aB_2l  = uop_index_e(B_2l, uop_int_binary(UOP_IADD,
                                  uop_int_binary(UOP_IMUL, rk_2l, k64_2l), rn_2l));
    Term mm_2l  = uop_binary(UOP_MUL, aA_2l, aB_2l);
    Term rd_2l  = uop_reduce(REDUCE_SUM, /*axis=*/2, mm_2l);
    Term aC_2l  = uop_int_binary(UOP_IADD,
                                 uop_int_binary(UOP_IMUL, rm_2l, k64_2l), rn_2l);
    Term st_2l  = uop_store(C_2l, aC_2l, rd_2l);
    // UPCAST N (axis 1) by 4: M(0), N_outer(1,LOOP,16), N_inner(2,UPCAST,4), K(3,REDUCE)
    KOpt up_2l = { KOP_UPCAST, 1, 4 };
    Term s1 = uop_dag_apply_kopt(st_2l, up_2l);
    CHECK(s1 != 0 && s1 != st_2l);
    // LOCAL N_outer (axis 1) by 4: M(0), N_oo(1,LOOP,4), N_ol(2,LOCAL,4), N_inner(3,UPCAST,4), K(4,REDUCE)
    KOpt lc1_2l = { KOP_LOCAL, 1, 4 };
    Term s2 = uop_dag_apply_kopt(s1, lc1_2l);
    CHECK(s2 != 0 && s2 != s1);
    // LOCAL M (axis 0) by 4: M_o(0,LOOP,8), M_l(1,LOCAL,4), N_oo(2,LOOP,4), N_ol(3,LOCAL,4), N_inner(4,UPCAST,4), K(5,REDUCE)
    KOpt lc2_2l = { KOP_LOCAL, 0, 4 };
    Term s3 = uop_dag_apply_kopt(s2, lc2_2l);
    CHECK(s3 != 0 && s3 != s2);
    char buf2l[4096];
    fp = fmemopen(buf2l, sizeof(buf2l), "w");
    cg_render_uop_kernel(s3, "k", C_2l, in_2l, 2, fp);
    fclose(fp);
    // GLOBAL grid = M_o(8) * N_oo(4) = 32 threadgroups, decoded from `tg`.
    CHECK(contains(buf2l, "if (tg >= 32u) return;"));
    // Two LOCAL axes off `tt`: a3 (N_ol, innermost in ranges[]) -> `tt % 4u`,
    // a1 (M_l) -> `(tt / 4u) % 4u`.
    CHECK(contains(buf2l, "uint a3 = tt % 4u; /* local"));
    CHECK(contains(buf2l, "uint a1 = (tt / 4u) % 4u; /* local"));
    // No bare `uint aN = tt;` (the single-LOCAL form) -- both are decoded.
    CHECK(!contains(buf2l, "= tt; /* local"));
    // UPCAST axis (a4) -> #pragma unroll(4) for-loop.
    CHECK(contains(buf2l, "#pragma unroll(4)"));
    CHECK(contains(buf2l, "for (uint a4 = 0; a4 < 4; a4++)"));
    // The MSL must actually compile through xcrun metal (if available).
    if (xcrun_metal_available()) CHECK_EQ(compile_msl_xcrun(buf2l), 0);
  }

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

  // F6: cg_render_uop_kernel_c emits a C99 kernel for CPU JIT use.
  // Same UOp DAG, different prologue (no kernel/[[buffer]]/thread attrs).
  TEST_BEGIN("render-uop-c/empty-root-prints-empty");
  char bufc0[2048];
  fp = fmemopen(bufc0, sizeof(bufc0), "w");
  CHECK(fp != NULL);
  cg_render_uop_kernel_c(/*root=*/0, "k0c", out, NULL, 0, fp);
  fclose(fp);
  CHECK(contains(bufc0, "/* empty kernel */"));
  CHECK(contains(bufc0, "void k0c"));
  CHECK(!contains(bufc0, "kernel void"));
  CHECK(!contains(bufc0, "metal_stdlib"));
  CHECK(!contains(bufc0, "[[ buffer"));

  TEST_BEGIN("render-uop-c/elementwise-add-cpu-signature");
  Term r_c    = uop_range(0, 0, 32);
  Term ld_aC  = uop_index_e(in0, r_c);
  Term ld_bC  = uop_index_e(in1, r_c);
  Term sumC   = uop_binary(UOP_ADD, ld_aC, ld_bC);
  Term st_c   = uop_store(out, r_c, sumC);
  char bufc1[4096];
  fp = fmemopen(bufc1, sizeof(bufc1), "w");
  cg_render_uop_kernel_c(st_c, "kadd", out, in_bufs, 2, fp);
  fclose(fp);
  // CPU-JIT contract: void k(out_v, ins_v, n, in_numels) (matches
  // the signature dlsym'd by cpu_jit_dispatch).
  CHECK(contains(bufc1, "void kadd(void *out_v, const void *const *ins_v"));
  CHECK(contains(bufc1, "unsigned n, const unsigned *in_numels"));
  // Pointer cast prologue.
  CHECK(contains(bufc1, "float *out = (float *)out_v;"));
  CHECK(contains(bufc1, "const float *in0 = (const float *)ins_v[0];"));
  // Body shares MSL emit; loops and loads compile as C99.
  CHECK(contains(bufc1, "for (uint a0 = 0; a0 < 32"));
  CHECK(contains(bufc1, "(in0[a0] + in1[a0])"));
  CHECK(contains(bufc1, "out[a0] = "));
  // Header: typedef + math.h so the body emit's `uint` and `sqrt` work.
  CHECK(contains(bufc1, "typedef unsigned int uint;"));
  CHECK(contains(bufc1, "<math.h>"));

  // F6 step 2: clang compile-validate the rendered C99. Writes the
  // emitted source to a temp file and shells out to `cc -c -Wall`.
  // Catches structural breakage that string-substring checks miss.
  TEST_BEGIN("render-uop-c/elementwise-add-clang-compiles");
  {
    char path[64];
    snprintf(path, sizeof(path), "/tmp/thvm_test_render_uop_c.c");
    FILE *cf = fopen(path, "w");
    CHECK(cf != NULL);
    fputs(bufc1, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "cc -std=c99 -Wall -Wextra -Werror -c %s "
             "-o /tmp/thvm_test_render_uop_c.o 2>&1",
             path);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "=== rendered C99 that failed to compile ===\n%s===\n",
              bufc1);
    }
    CHECK(rc == 0);
    unlink(path);
    unlink("/tmp/thvm_test_render_uop_c.o");
  }

  // F6 step 3: end-to-end clang compile + dlopen + invoke. Renders the
  // C99 to a .dylib, dlopens it, calls the kernel function with real
  // input/output pointers, verifies the output matches expected.
  // Validates the FULL F6 path (render -> clang -> dl -> invoke).
  TEST_BEGIN("render-uop-c/elementwise-add-runs-via-dlopen");
  {
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(st_c, "k", out, in_bufs, 2, fp);
    fclose(fp);
    const char *src_path = "/tmp/thvm_f6_step3.c";
    const char *dl_path  = "/tmp/thvm_f6_step3.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s 2>&1", dl_path, src_path);
    CHECK(system(cmd) == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    float a[32], b[32], y[32];
    for (u32 i = 0; i < 32; i++) {
      a[i] = (float)i;
      b[i] = 100.0f - (float)i;
    }
    const void *ins[2] = { a, b };
    unsigned numels[2] = { 32, 32 };
    kfn(y, ins, 32, numels);
    // Each y[i] = a[i] + b[i] = i + (100 - i) = 100.
    for (u32 i = 0; i < 32; i++) CHECK(y[i] == 100.0f);
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  // F6 step 5: extend coverage. Validate render_uop_c produces correct
  // output for shapes beyond the trivial elementwise add.
  // Helper: render -> compile -> dlopen -> invoke + verify in one pass.
  // Returns 0 on parity success, captured fail-info via stderr otherwise.

  TEST_BEGIN("render-uop-c/multi-input-elementwise-runs");
  {
    // y[i] = a[i] + b[i] * c[i] over 16 floats.
    u32 dimsM[1] = { 16 };
    Term outM = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsM, 0);
    Term aM   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsM, 1);
    Term bM   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsM, 2);
    Term cM   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsM, 3);
    Term ins[3] = { aM, bM, cM };
    Term rM   = uop_range(0, 0, 16);
    Term la   = uop_index_e(aM, rM);
    Term lb   = uop_index_e(bM, rM);
    Term lc   = uop_index_e(cM, rM);
    Term mul  = uop_binary(UOP_MUL, lb, lc);
    Term sum  = uop_binary(UOP_ADD, la, mul);
    Term stM  = uop_store(outM, rM, sum);
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(stM, "k", outM, ins, 3, fp);
    fclose(fp);
    const char *src_path = "/tmp/thvm_f6_step5_a.c";
    const char *dl_path  = "/tmp/thvm_f6_step5_a.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s 2>&1", dl_path, src_path);
    CHECK(system(cmd) == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    float aa[16], bb[16], cc[16], yy[16];
    for (u32 i = 0; i < 16; i++) {
      aa[i] = (float)i;
      bb[i] = 2.0f;
      cc[i] = 3.0f;
    }
    const void *insp[3] = { aa, bb, cc };
    unsigned numels[3] = { 16, 16, 16 };
    kfn(yy, insp, 16, numels);
    // y[i] = i + 2*3 = i + 6.
    for (u32 i = 0; i < 16; i++) {
      CHECK(yy[i] == (float)i + 6.0f);
    }
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  TEST_BEGIN("render-uop-c/unary-neg-runs");
  {
    // y[i] = -a[i] over 8 floats.
    u32 dimsU[1] = { 8 };
    Term outU = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsU, 0);
    Term aU   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsU, 1);
    Term insU[1] = { aU };
    Term rU   = uop_range(0, 0, 8);
    Term lU   = uop_index_e(aU, rU);
    Term ng   = uop_unary(UOP_NEG, lU);
    Term stU  = uop_store(outU, rU, ng);
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(stU, "k", outU, insU, 1, fp);
    fclose(fp);
    const char *src_path = "/tmp/thvm_f6_step5_b.c";
    const char *dl_path  = "/tmp/thvm_f6_step5_b.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s 2>&1", dl_path, src_path);
    CHECK(system(cmd) == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    float aa[8], yy[8];
    for (u32 i = 0; i < 8; i++) aa[i] = (float)i;
    const void *insp[1] = { aa };
    unsigned numels[1] = { 8 };
    kfn(yy, insp, 8, numels);
    for (u32 i = 0; i < 8; i++) CHECK(yy[i] == -(float)i);
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  TEST_BEGIN("render-uop-c/reduce-sum-runs");
  {
    // 2D input shape [4, 8]; sum-reduce over axis 1 -> output [4].
    // y[i] = sum_{j=0..7} a[i*8 + j]
    u32 dimsR[1] = { 4 };
    u32 dimsRin[1] = { 32 };  // flat 4*8
    Term outR = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsR, 0);
    Term aR   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsRin, 1);
    Term insR[1] = { aR };
    Term r0   = uop_range(0, 0 /*LOOP*/, 4);
    Term r1   = uop_range(1, 1 /*REDUCE*/, 8);
    // Address: i*8 + j (linearised 2D access).
    Term k8   = uop_const(DT_INT32, 8);
    Term i_x_8 = uop_int_binary(UOP_IMUL, r0, k8);
    Term addr  = uop_int_binary(UOP_IADD, i_x_8, r1);
    Term lR   = uop_index_e(aR, addr);
    Term red  = uop_reduce(REDUCE_SUM, /*axis=*/1, lR);
    Term stR  = uop_store(outR, r0, red);
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(stR, "k", outR, insR, 1, fp);
    fclose(fp);
    const char *src_path = "/tmp/thvm_f6_step6.c";
    const char *dl_path  = "/tmp/thvm_f6_step6.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s 2>&1", dl_path, src_path);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "=== rendered C99 that failed to compile ===\n%s===\n",
              src);
    }
    CHECK(rc == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    float aa[32], yy[4];
    for (u32 i = 0; i < 32; i++) aa[i] = (float)i;  // 0..31
    const void *insp[1] = { aa };
    unsigned numels[1] = { 32 };
    kfn(yy, insp, 4, numels);
    // y[0] = 0+1+2+...+7   = 28
    // y[1] = 8+9+...+15    = 92
    // y[2] = 16+17+...+23  = 156
    // y[3] = 24+25+...+31  = 220
    CHECK(yy[0] == 28.0f);
    CHECK(yy[1] == 92.0f);
    CHECK(yy[2] == 156.0f);
    CHECK(yy[3] == 220.0f);
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  TEST_BEGIN("render-uop-c/reduce-max-runs");
  {
    // Same shape as reduce-sum but kind=REDUCE_MAX.
    u32 dimsM2[1]   = { 4 };
    u32 dimsMin2[1] = { 32 };
    Term outM2 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsM2,   0);
    Term aM2   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsMin2, 1);
    Term insM2[1] = { aM2 };
    Term r0   = uop_range(0, 0, 4);
    Term r1   = uop_range(1, 1, 8);
    Term k8M2 = uop_const(DT_INT32, 8);
    Term ix8M2 = uop_int_binary(UOP_IMUL, r0, k8M2);
    Term addrM2 = uop_int_binary(UOP_IADD, ix8M2, r1);
    Term lM2  = uop_index_e(aM2, addrM2);
    Term redM2 = uop_reduce(REDUCE_MAX, /*axis=*/1, lM2);
    Term stM2  = uop_store(outM2, r0, redM2);
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(stM2, "k", outM2, insM2, 1, fp);
    fclose(fp);
    const char *src_path = "/tmp/thvm_f6_step7.c";
    const char *dl_path  = "/tmp/thvm_f6_step7.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s 2>&1", dl_path, src_path);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "=== rendered C99 that failed to compile ===\n%s===\n",
              src);
    }
    CHECK(rc == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    // Inject a known max value at one position per row to verify
    // the reducer picks it. row i: [i*8 .. i*8+7], with the max at
    // position i*8+7 = max_val.
    float aa[32], yy[4];
    for (u32 i = 0; i < 32; i++) aa[i] = -100.0f - (float)i;
    aa[ 7] = 1000.0f;   // row 0 max
    aa[15] = 2000.0f;   // row 1 max
    aa[23] = 3000.0f;   // row 2 max
    aa[31] = 4000.0f;   // row 3 max
    const void *insp[1] = { aa };
    unsigned numels[1] = { 32 };
    kfn(yy, insp, 4, numels);
    CHECK(yy[0] == 1000.0f);
    CHECK(yy[1] == 2000.0f);
    CHECK(yy[2] == 3000.0f);
    CHECK(yy[3] == 4000.0f);
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  TEST_BEGIN("render-uop-c/transcendentals-runs");
  {
    // Float-only unary ops via libm: sqrt/exp2/log2/recip.  Validates
    // <math.h> include emits the right calls and the math links into
    // the shared library.
    u32 dimsT[1] = { 4 };
    Term outT = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsT, 0);
    Term aT   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsT, 1);
    Term insT[1] = { aT };
    Term r    = uop_range(0, 0, 4);
    Term la   = uop_index_e(aT, r);
    Term sq   = uop_unary(UOP_SQRT,  la);
    Term stT  = uop_store(outT, r, sq);
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(stT, "k", outT, insT, 1, fp);
    fclose(fp);
    const char *src_path = "/tmp/thvm_f6_step8.c";
    const char *dl_path  = "/tmp/thvm_f6_step8.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    // -lm in case the libm symbols aren't auto-linked (Linux, BSD).
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s -lm 2>&1", dl_path, src_path);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "=== rendered C99 that failed to compile ===\n%s===\n",
              src);
    }
    CHECK(rc == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    float aa[4] = {1.0f, 4.0f, 9.0f, 16.0f}, yy[4];
    const void *insp[1] = { aa };
    unsigned numels[1] = { 4 };
    kfn(yy, insp, 4, numels);
    CHECK(yy[0] == 1.0f);
    CHECK(yy[1] == 2.0f);
    CHECK(yy[2] == 3.0f);
    CHECK(yy[3] == 4.0f);
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  TEST_BEGIN("render-uop-c/bitcast-fp32-int32-runs");
  {
    // y[i] = bitcast(in[i], int32). Each input is a fp32 with known
    // bit pattern; output is the same bits as int32.
    u32 dimsB[1] = { 4 };
    Term outB = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_INT32, 1, dimsB, 0);
    Term aB   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsB, 1);
    Term insB[1] = { aB };
    Term r    = uop_range(0, 0, 4);
    Term la   = uop_index_e(aB, r);
    Term bc   = uop_bitcast(la, DT_INT32);
    Term stB  = uop_store(outB, r, bc);
    char src[4096];
    fp = fmemopen(src, sizeof(src), "w");
    cg_render_uop_kernel_c(stB, "k", outB, insB, 1, fp);
    fclose(fp);
    // Confirm THVM_BITCAST appears in the rendered C.
    CHECK(contains(src, "THVM_BITCAST(int,"));
    const char *src_path = "/tmp/thvm_f6_step14.c";
    const char *dl_path  = "/tmp/thvm_f6_step14.dylib";
    FILE *cf = fopen(src_path, "w");
    fputs(src, cf);
    fclose(cf);
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "clang -O2 -fPIC -shared -o %s %s 2>&1", dl_path, src_path);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "=== rendered C99 that failed to compile ===\n%s===\n",
              src);
    }
    CHECK(rc == 0);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    CHECK(h != NULL);
    typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *);
    KFn kfn = (KFn)dlsym(h, "k");
    CHECK(kfn != NULL);
    // 1.0f bits = 0x3F800000 = 1065353216
    // 2.0f bits = 0x40000000 = 1073741824
    // 4.0f bits = 0x40800000 = 1082130432
    // 0.0f bits = 0x00000000 = 0
    union { float f; int32_t i; } pun;
    int32_t expected[4];
    float aa[4] = {1.0f, 2.0f, 4.0f, 0.0f};
    for (u32 i = 0; i < 4; i++) {
      pun.f = aa[i]; expected[i] = pun.i;
    }
    int32_t yy[4];
    const void *insp[1] = { aa };
    unsigned numels[1] = { 4 };
    kfn(yy, insp, 4, numels);
    for (u32 i = 0; i < 4; i++) CHECK(yy[i] == expected[i]);
    dlclose(h);
    unlink(src_path);
    unlink(dl_path);
  }

  thvm_free();
  TEST_REPORT();
}
