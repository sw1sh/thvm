// test_render_uop_metal.c - Phase F3: render_uop output must compile
// through `xcrun metal -c`.
//
// Validates that the UOp DAG renderer's pseudo-MSL output isn't
// just "looks right" but is actually valid Metal Shading Language
// the Apple shader compiler accepts.  Catches structural bugs
// (unbalanced braces, undefined builtins, type mismatches) that the
// string-grep tests in test_render_uop.c can miss.
//
// Each fixture renders a canonical kernel shape through
// cg_render_uop_kernel, writes the output to a temp .metal file,
// and shells out to `xcrun metal -c <file> -o /dev/null`.  A non-
// zero exit code from the Metal compiler fails the test.
//
// Skipped silently when `xcrun metal` isn't available (CI without
// Xcode); detected by checking `xcrun -f metal` first.
//
// This is the F3 parity seam from the migration plan: when the
// renderer rewrite proper flips render_metal to call into render_uop,
// every kernel scheduled by the system flows through this validator.

#include "../src/thvm.c"
#include "test.h"
#include <unistd.h>

static int xcrun_metal_available(void) {
  // Check `xcrun -f metal` exit status; 0 means available.
  int rc = system("xcrun -f metal >/dev/null 2>&1");
  return rc == 0;
}

static int compile_through_metal(const char *msl_text) {
  char path[64];
  snprintf(path, sizeof(path), "/tmp/thvm_render_uop_%d.metal", getpid());
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

int main(void) {
  thvm_init();

  if (!xcrun_metal_available()) {
    fprintf(stderr, "skipping: xcrun metal not available\n");
    return 0;
  }

  // Distinct buffer shapes so each hash-cons to a unique heap loc.
  u32 dimsOut[1] = { 256 };
  u32 dimsIn0[1] = { 512 };
  u32 dimsIn1[1] = { 1024 };
  Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsOut);
  Term in0 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsIn0);
  Term in1 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsIn1);
  Term in_bufs[2] = { in0, in1 };

  TEST_BEGIN("render-uop-metal/const-fill-compiles");
  Term r1   = uop_range(0, 0, 32);
  Term cone = uop_const(DT_FP32, 0x3F800000u);
  Term st1  = uop_store(out, r1, cone);
  char buf1[4096];
  FILE *mp1 = fmemopen(buf1, sizeof(buf1), "w");
  cg_render_uop_kernel(st1, "k_fill", out, NULL, 0, mp1);
  fclose(mp1);
  CHECK_EQ(compile_through_metal(buf1), 0);

  TEST_BEGIN("render-uop-metal/elementwise-compiles");
  Term r_e0 = uop_range(0, 0, 32);
  Term ld_a = uop_index_e(in0, r_e0);
  Term ld_b = uop_index_e(in1, r_e0);
  Term mul  = uop_binary(UOP_MUL, ld_a, ld_b);
  Term add  = uop_binary(UOP_ADD, mul, cone);
  Term st_e = uop_store(out, r_e0, add);
  char bufe[4096];
  FILE *mpe = fmemopen(bufe, sizeof(bufe), "w");
  cg_render_uop_kernel(st_e, "k_elw", out, in_bufs, 2, mpe);
  fclose(mpe);
  CHECK_EQ(compile_through_metal(bufe), 0);

  TEST_BEGIN("render-uop-metal/reduce-sum-compiles");
  Term r_o = uop_range(0, 0, 32);
  Term r_k = uop_range(1, 1, 16);
  Term ld  = uop_index_e(in0, r_k);
  Term red = uop_reduce(REDUCE_SUM, /*axis=*/1, ld);
  Term st_r = uop_store(out, r_o, red);
  char bufr[4096];
  FILE *mpr = fmemopen(bufr, sizeof(bufr), "w");
  cg_render_uop_kernel(st_r, "k_red", out, in_bufs, 2, mpr);
  fclose(mpr);
  CHECK_EQ(compile_through_metal(bufr), 0);

  TEST_BEGIN("render-uop-metal/matmul-tc-compiles");
  // STORE(out, m*16+n, OPT(REDUCE(MUL(A[m*32+k], B[k*16+n]), SUM, k=2), TC, 0))
  // Distinct 2-D buffers so A/B hash-cons separately (1-D would
  // collapse 16x32 and 32x16 to the same 512-element buffer).
  u32 dimsA[2] = {16, 32};
  u32 dimsB[2] = {32, 16};
  Term mmA = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA);
  Term mmB = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB);
  Term mm_in_bufs[2] = { mmA, mmB };
  Term r_m = uop_range(0, 0, 16);
  Term r_n = uop_range(1, 0, 16);
  Term r_kk = uop_range(2, 1, 32);
  Term k16  = uop_const(DT_INT32, 16);
  Term k32  = uop_const(DT_INT32, 32);
  Term mK   = uop_int_binary(UOP_IMUL, r_m, k32);
  Term addrA= uop_int_binary(UOP_IADD, mK, r_kk);
  Term ldA  = uop_index_e(mmA, addrA);
  Term kN   = uop_int_binary(UOP_IMUL, r_kk, k16);
  Term addrB= uop_int_binary(UOP_IADD, kN, r_n);
  Term ldB  = uop_index_e(mmB, addrB);
  Term mulm = uop_binary(UOP_MUL, ldA, ldB);
  Term redM = uop_reduce(REDUCE_SUM, 2, mulm);
  Term tcv  = uop_opt(redM, UOP_OPT_TC, 0);
  Term mN   = uop_int_binary(UOP_IMUL, r_m, k16);
  Term addrC= uop_int_binary(UOP_IADD, mN, r_n);
  Term st_m = uop_store(out, addrC, tcv);
  char bufm[8192];
  FILE *mpm = fmemopen(bufm, sizeof(bufm), "w");
  cg_render_uop_kernel(st_m, "k_gemm", out, mm_in_bufs, 2, mpm);
  fclose(mpm);
  CHECK_EQ(compile_through_metal(bufm), 0);

  TEST_BEGIN("render-uop-metal/recogniser-installs-tc-on-bare-matmul");
  // Build the same shape WITHOUT the manual OPT wrap and let
  // uop_recognise_tc install it.  Validates the F3.1 producer side
  // wires through render_uop's existing simdgroup_matrix consumer.
  Term st_bare = uop_store(out, addrC, redM);
  Term st_rec  = uop_recognise_tc(st_bare);
  CHECK(st_rec != st_bare);
  // Render and grep for simdgroup_matrix to confirm the OPT wrap
  // routed to rmu_emit_matmul_tc.
  char bufr2[8192];
  FILE *mpr2 = fmemopen(bufr2, sizeof(bufr2), "w");
  cg_render_uop_kernel(st_rec, "k_gemm_rec", out, mm_in_bufs, 2, mpr2);
  fclose(mpr2);
  CHECK(strstr(bufr2, "simdgroup_matrix<float, 8, 8>") != NULL);
  CHECK(strstr(bufr2, "simdgroup_multiply_accumulate") != NULL);
  CHECK_EQ(compile_through_metal(bufr2), 0);

  TEST_BEGIN("render-uop-metal/matmul-tc-tiled-threadgroup-staged");
  // Both M and N stamped KAX_GLOBAL (5) at a size the tiled picker
  // accepts (M=128, N=64, K=16 -> 128x64 tile, 8 simdgroups, KB=8).
  // This is the threadgroup-staged, register-blocked path
  // (rmu_emit_matmul_tc_tiled): assert the threadgroup arrays + barrier
  // + register accumulator array + multi-MMA body are emitted and the
  // MSL compiles through xcrun metal.
  u32 dimsA2[2] = {128, 16};
  u32 dimsB2[2] = {16, 64};
  Term tA = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA2);
  Term tB = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB2);
  Term t_in[2] = { tA, tB };
  Term tm = uop_range(0, 5 /*GLOBAL*/, 128);
  Term tn = uop_range(1, 5 /*GLOBAL*/, 64);
  Term tk = uop_range(2, 1 /*REDUCE*/, 16);
  Term c16 = uop_const(DT_INT32, 16);
  Term c64 = uop_const(DT_INT32, 64);
  Term tAaddr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, tm, c16), tk);
  Term tBaddr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, tk, c64), tn);
  Term tmul = uop_binary(UOP_MUL, uop_index_e(tA, tAaddr), uop_index_e(tB, tBaddr));
  Term tred = uop_reduce(REDUCE_SUM, 2, tmul);
  Term ttc  = uop_opt(tred, UOP_OPT_TC, 0);
  Term tCaddr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, tm, c64), tn);
  Term st_t = uop_store(out, tCaddr, ttc);
  char buft[16384];
  FILE *mpt = fmemopen(buft, sizeof(buft), "w");
  cg_render_uop_kernel(st_t, "k_gemm_tiled", out, t_in, 2, mpt);
  fclose(mpt);
  CHECK(strstr(buft, "TC tiled matmul") != NULL);
  CHECK(strstr(buft, "threadgroup float _Asm[") != NULL);
  CHECK(strstr(buft, "threadgroup float _Bsm[") != NULL);
  CHECK(strstr(buft, "threadgroup_barrier(mem_flags::mem_threadgroup);") != NULL);
  CHECK(strstr(buft, "simdgroup_matrix<float, 8, 8> _acc[") != NULL);
  CHECK(strstr(buft, "simdgroup_multiply_accumulate(_acc[") != NULL);
  // A must stage from in0 and B from in1 (the buf-name aliasing bug
  // would put both on the same source).  K=16 is 4-aligned, so both
  // operands take the vectorised float4 cooperative-load path.
  CHECK(strstr(buft, "(device const float4*)(in0 + (_tm") != NULL);
  CHECK(strstr(buft, "(device const float4*)(in1 + ") != NULL);
  CHECK_EQ(compile_through_metal(buft), 0);

  TEST_BEGIN("render-uop-metal/matmul-tc-tiled-autotune-packed-config");
  // SAME M=128,N=64,K=16 tiled matmul, but the OPT_TC carries an autotune-
  // PACKED tile config (tc_tile_pack) instead of factor 0.  The emitter must
  // emit THAT geometry (lm=2 ln=1 rm=2 rn=2 kb=16 -> 32x16, KB=16) and the
  // MSL must still compile through xcrun metal -- proving the autotune can
  // drive an arbitrary VALID tile, not just the heuristic.
  u32 packed_tile = tc_tile_pack(2, 1, 2, 2, 16);
  CHECK(tc_tile_valid(128, 64, 16, 2, 1, 2, 2, 16, 8, 0, 0));
  Term ttc_p  = uop_opt(tred, UOP_OPT_TC, packed_tile);
  Term st_t_p = uop_store(out, tCaddr, ttc_p);
  char buft_p[16384];
  FILE *mpt_p = fmemopen(buft_p, sizeof(buft_p), "w");
  cg_render_uop_kernel(st_t_p, "k_gemm_tiled_packed", out, t_in, 2, mpt_p);
  fclose(mpt_p);
  CHECK(strstr(buft_p,
    "TC tiled matmul: tile 32x16, 2x1 simdgroups, 2x2 reg, KB=16 */") != NULL);
  CHECK(strstr(buft_p, "simdgroup_multiply_accumulate(_acc[") != NULL);
  CHECK_EQ(compile_through_metal(buft_p), 0);

  TEST_BEGIN("render-uop-metal/matmul-tc-tiled-q8-int8-weight");
  // q8 weight-only-quantized FLUX path: A {M,K} bf16, B = Transpose[Cast[
  // W_int8, bf16]] (W {N,K} contiguous int8, so B's K-axis has stride 1 and
  // N-axis stride K = the b_trans coalesced-K-vector path).  Output bf16.
  // The tiled emitter must read W as `char4` (half the bf16 bytes) and cast
  // char -> bfloat into the bfloat-staged _Bsm; the MMA runs at the native
  // bf16 tensor-core rate.  M=128, N=64, K=16 -> 128x64 tile.
  u32 qdimsO[2] = {128, 64};
  u32 qdimsA[2] = {128, 16};
  u32 qdimsW[2] = {64, 16};        // W {N,K} contiguous int8
  Term qOut = uop_buffer(UOP_SCOPE_GLOBAL, DT_BF16, 2, qdimsO);   // bf16 output
  Term qA  = uop_buffer(UOP_SCOPE_GLOBAL, DT_BF16, 2, qdimsA);
  Term qWi = uop_buffer(UOP_SCOPE_GLOBAL, DT_INT8, 2, qdimsW);
  Term q_in[2] = { qA, qWi };
  Term qm = uop_range(0, 5 /*GLOBAL*/, 128);
  Term qn = uop_range(1, 5 /*GLOBAL*/, 64);
  Term qk = uop_range(2, 1 /*REDUCE*/, 16);
  Term qc16 = uop_const(DT_INT32, 16);
  Term qc64 = uop_const(DT_INT32, 64);
  // A[m*K + k]
  Term qAaddr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, qm, qc16), qk);
  // B = Transpose[W]: W[n*K + k] (n stride K, k stride 1)
  Term qBaddr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, qn, qc16), qk);
  // The int8 weight read is cast int8 -> bf16 (the TUOpCast in the WL test).
  Term qbCast = uop_cast(uop_index_e(qWi, qBaddr), DT_BF16);
  Term qmul = uop_binary(UOP_MUL, uop_index_e(qA, qAaddr), qbCast);
  Term qred = uop_reduce(REDUCE_SUM, 2, qmul);
  Term qCaddr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, qm, qc64), qn);
  // Let the producer-side recogniser install OPT(_, TC, 0) + stamp M/N GLOBAL,
  // exactly as the Metal render path does (render_metal.c).
  Term qst  = uop_recognise_tc_parallel(uop_store(qOut, qCaddr, qred));
  CHECK(qst != uop_store(qOut, qCaddr, qred));   // recogniser must fire on int8 B
  char bufq[16384];
  FILE *mpq = fmemopen(bufq, sizeof(bufq), "w");
  cg_render_uop_kernel(qst, "k_gemm_q8", qOut, q_in, 2, mpq);
  fclose(mpq);
  // Tiled TC path with bfloat staging (A bf16, B int8->bfloat).
  CHECK(strstr(bufq, "TC tiled matmul") != NULL);
  CHECK(strstr(bufq, "threadgroup bfloat _Bsm[") != NULL);
  CHECK(strstr(bufq, "simdgroup_matrix<bfloat, 8, 8>") != NULL);
  CHECK(strstr(bufq, "simdgroup_multiply_accumulate(_acc[") != NULL);
  // B must read the int8 weight as char4 and dequant char -> bfloat in
  // the cooperative staging (the half-bandwidth fused dequant).
  CHECK(strstr(bufq, "char4 _bv = *(device const char4*)(in1 +") != NULL);
  CHECK(strstr(bufq, "(bfloat)_bv.x") != NULL);
  CHECK_EQ(compile_through_metal(bufq), 0);

  thvm_free();
  TEST_REPORT();
}
