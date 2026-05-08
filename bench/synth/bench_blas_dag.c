// bench/synth/bench_blas_dag.c -- micro-benchmark for the Slice 8
// CPU BLAS DAG migration.  Times a single 256x256x256 f32 matmul
// dispatch under each of:
//   - default (THVM_PHASE_C7_FREE_PROGRAM=1) post-Slice-8: cblas via DAG
//   - THVM_PHASE_C7_FREE_PROGRAM=0 (dual-write): cblas via legacy or DAG
//   - THVM_DUMP_GEMM_REJECT=1 mode (no behaviour change; useful for trace)
//
// Pre-Slice-8 the default-mode dispatch fell through to render_uop_c
// (per the session 1 audit: "30-100x slower than cblas_sgemm").  This
// bench is meant to be run by hand to verify the regression is fixed.
//
// Build: in worktree root,
//   make build
//   clang -O2 -Wall -DTHVM_HAS_METAL -I. -o bin/bench_blas_dag \
//     bench/synth/bench_blas_dag.c -lm -framework Accelerate \
//     -framework Metal -framework Foundation \
//     build/backend_metal.o
//
// Run: bin/bench_blas_dag

#include "../../src/thvm.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double bench_one(u32 M, u32 K, u32 N, u32 iters,
                        u64 *out_dag, u64 *out_legacy) {
  unsetenv("THVM_BACKEND"); thvm_init();
  cpu_blas_gemm_dispatch_counters_reset();

  Shape sa = {0}; sa.ndim = 2; sa.dims[0] = M; sa.dims[1] = K;
  Shape sb = {0}; sb.ndim = 2; sb.dims[0] = K; sb.dims[1] = N;

  float *mm_a = (float *)calloc((size_t)M * K, sizeof(float));
  float *mm_b = (float *)calloc((size_t)K * N, sizeof(float));
  for (u32 i = 0; i < M*K; i++) mm_a[i] = (float)((i * 13 + 7) % 19) - 9.0f;
  for (u32 i = 0; i < K*N; i++) mm_b[i] = (float)((i * 31 + 5) % 23) - 11.0f;

  // Warm up: single dispatch to populate the kernel cache + JIT path.
  // After warmup, time `iters` further dispatches.
  double total = 0.0;
  for (u32 it = 0; it < iters + 1; it++) {
    u32 ta = tensor_alloc(CURRENT_BACKEND, sa, DT_FP32);
    u32 tb = tensor_alloc(CURRENT_BACKEND, sb, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[ta].buf_id, mm_a,
                                (size_t)M * K * sizeof(float));
    CURRENT_BACKEND->buf_write(TENS[tb].buf_id, mm_b,
                                (size_t)K * N * sizeof(float));
    Term A = term_new(0, TAG_TEN, DT_FP32, ta);
    Term B = term_new(0, TAG_TEN, DT_FP32, tb);
    u32 d_mk1[3] = {M, K, 1};
    u32 d_1kn[3] = {1, K, N};
    u32 d_mkn[3] = {M, K, N};
    Term Ar  = uop_reshape(A,  3, d_mk1);
    Term Ae  = uop_expand (Ar, 3, d_mkn);
    Term Br  = uop_reshape(B,  3, d_1kn);
    Term Be  = uop_expand (Br, 3, d_mkn);
    Term mul = uop_binary(UOP_MUL, Ae, Be);
    Term red = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);

    double t0 = now_sec();
    Term done = wnf(thvm_materialize(red));
    double t1 = now_sec();
    if (it > 0) total += (t1 - t0);
    (void)done;
  }
  *out_dag    = cpu_blas_gemm_dispatch_dag_count();
  *out_legacy = cpu_blas_gemm_dispatch_legacy_count();
  free(mm_a);
  free(mm_b);
  thvm_free();
  return total / (double)iters;
}

int main(void) {
  u32 sizes[][3] = {
    { 64,  64,  64 },
    {128, 128, 128 },
    {256, 256, 256 },
    {512, 512, 512 },
  };
  u32 n_sizes = (u32)(sizeof(sizes) / sizeof(sizes[0]));
  u32 iters = 5;

  printf("# blas_dag bench: per-iter avg (after one warmup)\n");
  printf("# size       | seconds  | gflops    | dag_count | legacy_count\n");
  for (u32 i = 0; i < n_sizes; i++) {
    u32 M = sizes[i][0], K = sizes[i][1], N = sizes[i][2];
    u64 dag, leg;
    double t = bench_one(M, K, N, iters, &dag, &leg);
    double gflops = 2.0 * (double)M * K * N / t / 1e9;
    printf("%4u x %4u x %4u | %.6f | %8.2f | %9llu | %12llu\n",
           M, K, N, t, gflops,
           (unsigned long long)dag, (unsigned long long)leg);
  }
  return 0;
}
