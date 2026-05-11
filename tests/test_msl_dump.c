// test_msl_dump.c -- ad-hoc: dump emitted MSL for representative
// beautiful_mnist-shaped kernels and time the Apple compiler.
// Verifies whether 1.2 sec/kernel cold-compile is MSL bloat or
// something else in the schedule pipeline.

#include "../src/thvm.c"
#include "test.h"
#include <unistd.h>
#include <sys/time.h>

static double now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static double compile_msl_via_xcrun(const char *msl, int show_errors) {
  char path[64];
  snprintf(path, sizeof(path), "/tmp/thvm_msl_%d.metal", getpid());
  FILE *fp = fopen(path, "w");
  if (fp == NULL) return -1;
  fputs(msl, fp);
  fclose(fp);
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "xcrun metal -x metal -c %s -o /dev/null %s",
           path, show_errors ? "2>&1" : "2>/dev/null");
  double t0 = now_ms();
  int rc = system(cmd);
  double t1 = now_ms();
  unlink(path);
  (void)rc;
  return t1 - t0;
}

static void dump(const char *label, const char *msl) {
  size_t lines = 0, bytes = 0;
  for (const char *p = msl; *p; p++) { bytes++; if (*p == '\n') lines++; }
  fprintf(stderr, "\n[%s] %zu bytes, %zu lines\n", label, bytes, lines);
  fprintf(stderr, "----\n%s----\n", msl);
  double ms = compile_msl_via_xcrun(msl, 0);
  fprintf(stderr, "[%s] xcrun metal -c: %.1f ms\n", label, ms);
}

int main(void) {
  thvm_init();

  // --- 1) elementwise scale: out[r] = in[r] * 2 ---
  TEST_BEGIN("msl/elementwise");
  {
    u32 dN[1] = { 9216 };  // BS=4 maxpool1 out numel
    Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dN);
    Term in0 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dN);
    Term in_bufs[1] = { in0 };
    Term r0  = uop_range(0, 0, 9216);
    Term ld  = uop_index_e(in0, r0);
    Term k2  = uop_const(DT_FP32, 0x40000000u);  // 2.0f
    Term mul = uop_binary(UOP_MUL, ld, k2);
    Term st  = uop_store(out, r0, mul);
    char buf[8192];
    FILE *fp = fmemopen(buf, sizeof(buf), "w");
    cg_render_uop_kernel(st, "k_elw", out, in_bufs, 1, fp);
    fclose(fp);
    dump("elementwise N=9216", buf);
  }

  // --- 2) reduce-sum: out[m] = sum_k(in[m*K + k]) shape={M=32, K=25} ---
  TEST_BEGIN("msl/reduce-sum-tail");
  {
    u32 dM[1] = { 32 };
    u32 dMK[2] = { 32, 25 };
    Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dM);
    Term in0 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dMK);
    Term in_bufs[1] = { in0 };
    Term r_m = uop_range(0, 0, 32);
    Term r_k = uop_range(1, 1, 25);
    Term k25 = uop_const(DT_INT32, 25);
    Term mK  = uop_int_binary(UOP_IMUL, r_m, k25);
    Term ad  = uop_int_binary(UOP_IADD, mK, r_k);
    Term ld  = uop_index_e(in0, ad);
    Term red = uop_reduce(REDUCE_SUM, 1, ld);
    Term st  = uop_store(out, r_m, red);
    char buf[8192];
    FILE *fp = fmemopen(buf, sizeof(buf), "w");
    cg_render_uop_kernel(st, "k_red", out, in_bufs, 1, fp);
    fclose(fp);
    dump("reduce-sum M=32 K=25", buf);
  }

  // --- 3) matmul: out[m,n] = sum_k A[m,k]*B[k,n], M=32 N=2304 K=25
  // Matches BS=4 beautiful_mnist conv1 im2col-matmul shape.
  TEST_BEGIN("msl/matmul-32x2304x25");
  {
    u32 dC[2] = { 32, 2304 };
    u32 dA[2] = { 32, 25 };
    u32 dB[2] = { 25, 2304 };
    Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC);
    Term mA  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA);
    Term mB  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB);
    Term in_bufs[2] = { mA, mB };
    Term r_m  = uop_range(0, 0, 32);
    Term r_n  = uop_range(1, 0, 2304);
    Term r_kk = uop_range(2, 1, 25);
    Term k25  = uop_const(DT_INT32, 25);
    Term k2304= uop_const(DT_INT32, 2304);
    Term mK   = uop_int_binary(UOP_IMUL, r_m, k25);
    Term addrA= uop_int_binary(UOP_IADD, mK, r_kk);
    Term ldA  = uop_index_e(mA, addrA);
    Term kN   = uop_int_binary(UOP_IMUL, r_kk, k2304);
    Term addrB= uop_int_binary(UOP_IADD, kN, r_n);
    Term ldB  = uop_index_e(mB, addrB);
    Term mulm = uop_binary(UOP_MUL, ldA, ldB);
    Term redM = uop_reduce(REDUCE_SUM, 2, mulm);
    Term mN   = uop_int_binary(UOP_IMUL, r_m, k2304);
    Term addrC= uop_int_binary(UOP_IADD, mN, r_n);
    Term st   = uop_store(out, addrC, redM);
    char buf[16384];
    FILE *fp = fmemopen(buf, sizeof(buf), "w");
    cg_render_uop_kernel(st, "k_gemm", out, in_bufs, 2, fp);
    fclose(fp);
    dump("matmul 32x2304x25 (naive)", buf);
  }

  // --- 4) matmul WITH TC opt installed by uop_recognise_tc ---
  TEST_BEGIN("msl/matmul-32x2304x25-tc");
  {
    u32 dC[2] = { 32, 2304 };
    u32 dA[2] = { 32, 25 };
    u32 dB[2] = { 25, 2304 };
    Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC);
    Term mA  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA);
    Term mB  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB);
    Term in_bufs[2] = { mA, mB };
    Term r_m  = uop_range(0, 0, 32);
    Term r_n  = uop_range(1, 0, 2304);
    Term r_kk = uop_range(2, 1, 25);
    Term k25  = uop_const(DT_INT32, 25);
    Term k2304= uop_const(DT_INT32, 2304);
    Term mK   = uop_int_binary(UOP_IMUL, r_m, k25);
    Term addrA= uop_int_binary(UOP_IADD, mK, r_kk);
    Term ldA  = uop_index_e(mA, addrA);
    Term kN   = uop_int_binary(UOP_IMUL, r_kk, k2304);
    Term addrB= uop_int_binary(UOP_IADD, kN, r_n);
    Term ldB  = uop_index_e(mB, addrB);
    Term mulm = uop_binary(UOP_MUL, ldA, ldB);
    Term redM = uop_reduce(REDUCE_SUM, 2, mulm);
    Term mN   = uop_int_binary(UOP_IMUL, r_m, k2304);
    Term addrC= uop_int_binary(UOP_IADD, mN, r_n);
    Term st   = uop_store(out, addrC, redM);
    Term st_rec = uop_recognise_tc(st);
    char buf[16384];
    FILE *fp = fmemopen(buf, sizeof(buf), "w");
    cg_render_uop_kernel(st_rec, "k_gemm_tc", out, in_bufs, 2, fp);
    fclose(fp);
    dump("matmul 32x2304x25 (TC-recognised)", buf);
  }

  thvm_free();
  TEST_REPORT();
}
