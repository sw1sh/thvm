// bench/synth/bench_blas_dag.c -- micro-benchmark for the Slice 8
// CPU BLAS DAG migration.  Times one matmul / matvec / dot dispatch
// each under default `THVM_PHASE_C7_FREE_PROGRAM=1` (post-Slice 8:
// cblas via DAG-side classifier) versus the legacy program[]-side
// matchers when the DAG path is disabled.
//
// Pre-Slice-8 the default-mode dispatch fell through to render_uop_c
// (per the session 1 audit: "30-100x slower than cblas_sgemm").  Run
// this bench by hand to confirm the regression stays fixed and that
// the dual-write knob still keeps the legacy path working.
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

// === GEMM ============================================================

static double bench_gemm_one(u32 M, u32 K, u32 N, u32 iters,
                             u64 *out_dag, u64 *out_legacy) {
  unsetenv("THVM_BACKEND"); thvm_init();
  cpu_blas_gemm_dispatch_counters_reset();

  Shape sa = {0}; sa.ndim = 2; sa.dims[0] = M; sa.dims[1] = K;
  Shape sb = {0}; sb.ndim = 2; sb.dims[0] = K; sb.dims[1] = N;

  float *mm_a = (float *)calloc((size_t)M * K, sizeof(float));
  float *mm_b = (float *)calloc((size_t)K * N, sizeof(float));
  for (u32 i = 0; i < M*K; i++) mm_a[i] = (float)((i * 13 + 7) % 19) - 9.0f;
  for (u32 i = 0; i < K*N; i++) mm_b[i] = (float)((i * 31 + 5) % 23) - 11.0f;

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

// === GEMV ============================================================

static double bench_gemv_one(u32 M, u32 K, u32 iters,
                             u64 *out_dag, u64 *out_legacy) {
  unsetenv("THVM_BACKEND"); thvm_init();
  cpu_blas_gemm_dispatch_counters_reset();

  Shape sw = {0}; sw.ndim = 2; sw.dims[0] = M; sw.dims[1] = K;
  Shape sx = {0}; sx.ndim = 2; sx.dims[0] = 1; sx.dims[1] = K;

  float *wv = (float *)calloc((size_t)M * K, sizeof(float));
  float *xv = (float *)calloc((size_t)K, sizeof(float));
  for (u32 i = 0; i < M*K; i++) wv[i] = (float)((i * 11 + 1) % 17) - 8.0f;
  for (u32 i = 0; i < K;   i++) xv[i] = (float)((i * 13 + 5) % 19) - 9.0f;

  double total = 0.0;
  for (u32 it = 0; it < iters + 1; it++) {
    u32 tw = tensor_alloc(CURRENT_BACKEND, sw, DT_FP32);
    u32 tx = tensor_alloc(CURRENT_BACKEND, sx, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[tw].buf_id, wv,
                                (size_t)M * K * sizeof(float));
    CURRENT_BACKEND->buf_write(TENS[tx].buf_id, xv,
                                (size_t)K * sizeof(float));
    Term W = term_new(0, TAG_TEN, DT_FP32, tw);
    Term X = term_new(0, TAG_TEN, DT_FP32, tx);
    u32 d_mk[2] = {M, K};
    Term Xe  = uop_expand (X, 2, d_mk);
    Term mul = uop_binary(UOP_MUL, W, Xe);
    Term red = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);

    double t0 = now_sec();
    Term done = wnf(thvm_materialize(red));
    double t1 = now_sec();
    if (it > 0) total += (t1 - t0);
    (void)done;
  }
  *out_dag    = cpu_blas_gemv_dispatch_dag_count();
  *out_legacy = cpu_blas_gemv_dispatch_legacy_count();
  free(wv);
  free(xv);
  thvm_free();
  return total / (double)iters;
}

// === DOT =============================================================

static double bench_dot_one(u32 K, u32 iters,
                            u64 *out_dag, u64 *out_legacy) {
  unsetenv("THVM_BACKEND"); thvm_init();
  cpu_blas_gemm_dispatch_counters_reset();

  Shape sd = {0}; sd.ndim = 1; sd.dims[0] = K;

  float *av = (float *)calloc((size_t)K, sizeof(float));
  float *bv = (float *)calloc((size_t)K, sizeof(float));
  for (u32 i = 0; i < K; i++) av[i] = (float)((i * 7 + 3) % 11) - 5.0f;
  for (u32 i = 0; i < K; i++) bv[i] = (float)((i * 5 + 2) % 13) - 6.0f;

  double total = 0.0;
  for (u32 it = 0; it < iters + 1; it++) {
    u32 ta = tensor_alloc(CURRENT_BACKEND, sd, DT_FP32);
    u32 tb = tensor_alloc(CURRENT_BACKEND, sd, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[ta].buf_id, av,
                                (size_t)K * sizeof(float));
    CURRENT_BACKEND->buf_write(TENS[tb].buf_id, bv,
                                (size_t)K * sizeof(float));
    Term A = term_new(0, TAG_TEN, DT_FP32, ta);
    Term B = term_new(0, TAG_TEN, DT_FP32, tb);
    Term mul = uop_binary(UOP_MUL, A, B);
    Term red = uop_reduce(REDUCE_SUM, /*axis=*/0, mul);

    double t0 = now_sec();
    Term done = wnf(thvm_materialize(red));
    double t1 = now_sec();
    if (it > 0) total += (t1 - t0);
    (void)done;
  }
  *out_dag    = cpu_blas_dot_dispatch_dag_count();
  *out_legacy = cpu_blas_dot_dispatch_legacy_count();
  free(av);
  free(bv);
  thvm_free();
  return total / (double)iters;
}

int main(void) {
  u32 iters = 5;

  printf("# blas_dag bench: per-iter avg (after one warmup)\n");

  printf("\n# === GEMM ===\n");
  printf("# size       | seconds  | gflops    | dag_count | legacy_count\n");
  u32 gemm_sizes[][3] = {
    { 64,  64,  64 },
    {128, 128, 128 },
    {256, 256, 256 },
    {512, 512, 512 },
  };
  for (u32 i = 0; i < sizeof(gemm_sizes)/sizeof(gemm_sizes[0]); i++) {
    u32 M = gemm_sizes[i][0], K = gemm_sizes[i][1], N = gemm_sizes[i][2];
    u64 dag, leg;
    double t = bench_gemm_one(M, K, N, iters, &dag, &leg);
    double gflops = 2.0 * (double)M * K * N / t / 1e9;
    printf("%4u x %4u x %4u | %.6f | %8.2f | %9llu | %12llu\n",
           M, K, N, t, gflops,
           (unsigned long long)dag, (unsigned long long)leg);
  }

  printf("\n# === GEMV ===\n");
  printf("# size       | seconds  | gflops    | dag_count | legacy_count\n");
  u32 gemv_sizes[][2] = {
    { 64,  64  },
    {128, 128 },
    {256, 256 },
    {512, 512 },
  };
  for (u32 i = 0; i < sizeof(gemv_sizes)/sizeof(gemv_sizes[0]); i++) {
    u32 M = gemv_sizes[i][0], K = gemv_sizes[i][1];
    u64 dag, leg;
    double t = bench_gemv_one(M, K, iters, &dag, &leg);
    double gflops = 2.0 * (double)M * K / t / 1e9;
    printf("       %4u x %4u | %.6f | %8.2f | %9llu | %12llu\n",
           M, K, t, gflops,
           (unsigned long long)dag, (unsigned long long)leg);
  }

  printf("\n# === DOT ===\n");
  printf("# size       | seconds  | gflops    | dag_count | legacy_count\n");
  u32 dot_sizes[] = {256, 1024, 4096, 16384};
  for (u32 i = 0; i < sizeof(dot_sizes)/sizeof(dot_sizes[0]); i++) {
    u32 K = dot_sizes[i];
    u64 dag, leg;
    double t = bench_dot_one(K, iters, &dag, &leg);
    double gflops = 2.0 * (double)K / t / 1e9;
    printf("            %5u | %.6f | %8.2f | %9llu | %12llu\n",
           K, t, gflops,
           (unsigned long long)dag, (unsigned long long)leg);
  }

  return 0;
}
