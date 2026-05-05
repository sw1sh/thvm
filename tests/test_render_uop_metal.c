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
  Term r_m = uop_range(0, 0, 16);
  Term r_n = uop_range(1, 0, 16);
  Term r_kk = uop_range(2, 1, 32);
  Term k16  = uop_const(DT_INT32, 16);
  Term k32  = uop_const(DT_INT32, 32);
  Term mK   = uop_int_binary(UOP_IMUL, r_m, k32);
  Term addrA= uop_int_binary(UOP_IADD, mK, r_kk);
  Term ldA  = uop_index_e(in0, addrA);
  Term kN   = uop_int_binary(UOP_IMUL, r_kk, k16);
  Term addrB= uop_int_binary(UOP_IADD, kN, r_n);
  Term ldB  = uop_index_e(in1, addrB);
  Term mulm = uop_binary(UOP_MUL, ldA, ldB);
  Term redM = uop_reduce(REDUCE_SUM, 2, mulm);
  Term tcv  = uop_opt(redM, UOP_OPT_TC, 0);
  Term mN   = uop_int_binary(UOP_IMUL, r_m, k16);
  Term addrC= uop_int_binary(UOP_IADD, mN, r_n);
  Term st_m = uop_store(out, addrC, tcv);
  char bufm[8192];
  FILE *mpm = fmemopen(bufm, sizeof(bufm), "w");
  cg_render_uop_kernel(st_m, "k_gemm", out, in_bufs, 2, mpm);
  fclose(mpm);
  CHECK_EQ(compile_through_metal(bufm), 0);

  thvm_free();
  TEST_REPORT();
}
