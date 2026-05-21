// tools/xbackend_bench.c - cross-backend raw-kernel-dispatch microbench.
//
// Builds a hand-written UOp DAG (matmul / elementwise / reduce), wraps it
// in a synthetic KernelEntry exactly as the schedule's materialize lift
// would, and times DEFAULT_BACKEND->dispatch_kernel in a steady-state
// loop drained by one buf_read.  The same binary runs on CPU, Metal
// (macOS) and CUDA (Linux+CUDA pod) by selecting DEV={cpu,metal,cuda} --
// an apples-to-apples comparison of thvm's own codegen across backends
// (no vendor BLAS path).
//
//   build (cpu/metal): make tools/xbackend_bench
//   build (cuda pod):  CUDA on, -DTHVM_HAS_CUDA wired by the Makefile
//   run:               DEV=cpu ./tools/xbackend_bench <which> <n> <reps>
//                      which in {matmul,ew,reduce}

#include "../src/thvm.c"
#include <stdio.h>
#include <stdlib.h>

// matmul: C[MxN] = A[MxK] @ B[KxN], scalar K-reduction.
static Term build_matmul(int M, int K, int N, Term *A_o, Term *B_o, Term *C_o) {
  u32 dC[2] = { (u32)M, (u32)N }, dA[2] = { (u32)M, (u32)K }, dB[2] = { (u32)K, (u32)N };
  Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC, 0);
  Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA, 1);
  Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB, 2);
  Term r_m = uop_range(0, KAX_LOOP, M);
  Term r_n = uop_range(1, KAX_LOOP, N);
  Term r_k = uop_range(2, KAX_REDUCE, K);
  Term kK = uop_const(DT_INT32, K), kN = uop_const(DT_INT32, N);
  Term addrA = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_m, kK), r_k);
  Term addrB = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_k, kN), r_n);
  Term mul = uop_binary(UOP_MUL, uop_index_e(A, addrA), uop_index_e(B, addrB));
  Term red = uop_reduce(REDUCE_SUM, 2, mul);
  Term addrC = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_m, kN), r_n);
  *A_o = A; *B_o = B; *C_o = C;
  return uop_store(C, addrC, red);
}

// elementwise: out[i] = in0[i]*in0[i] + in1[i], flat numel.
static Term build_ew(int numel, Term *A_o, Term *B_o, Term *C_o) {
  u32 d[1] = { (u32)numel };
  Term C  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 0);
  Term A  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 1);
  Term B  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 2);
  Term r  = uop_range(0, KAX_LOOP, numel);
  Term la = uop_index_e(A, r), lb = uop_index_e(B, r);
  Term v  = uop_binary(UOP_ADD, uop_binary(UOP_MUL, la, la), lb);
  *A_o = A; *B_o = B; *C_o = C;
  return uop_store(C, r, v);
}

// row reduce: out[r] = sum_c in[r,c]  (R rows of width Cw).
static Term build_reduce(int R, int Cw, Term *A_o, Term *C_o) {
  u32 dO[1] = { (u32)R }, dI[2] = { (u32)R, (u32)Cw };
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dO, 0);
  Term in  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dI, 1);
  Term r_r = uop_range(0, KAX_LOOP, R);
  Term r_c = uop_range(1, KAX_REDUCE, Cw);
  Term kC  = uop_const(DT_INT32, Cw);
  Term addr = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r_r, kC), r_c);
  Term red  = uop_reduce(REDUCE_SUM, 1, uop_index_e(in, addr));
  *A_o = in; *C_o = out;
  return uop_store(out, r_r, red);
}

int main(int argc, char **argv) {
  const char *which = argc > 1 ? argv[1] : "matmul";
  int n    = argc > 2 ? atoi(argv[2]) : 256;
  int reps = argc > 3 ? atoi(argv[3]) : 50;
  const char *dev = getenv("DEV"); if (!dev) dev = "cpu";

  thvm_init();
  Backend *b = DEFAULT_BACKEND;

  Term st, A = 0, B = 0, C = 0;
  u64 aN, bN, cN;          // element counts
  double flops;
  if (!strcmp(which, "matmul")) {
    st = build_matmul(n, n, n, &A, &B, &C);
    aN = bN = cN = (u64)n * n;  flops = 2.0 * n * n * n;
  } else if (!strcmp(which, "ew")) {
    st = build_ew(n, &A, &B, &C);
    aN = bN = cN = (u64)n;      flops = 2.0 * n;
  } else { // reduce: n x n
    st = build_reduce(n, n, &A, &C);
    aN = (u64)n * n; bN = 0; cN = (u64)n;  flops = (double)n * n;
  }

  u32 bA = b->buf_alloc(aN * 4);
  u32 bB = bN ? b->buf_alloc(bN * 4) : 0;
  u32 bC = b->buf_alloc(cN * 4);
  float *hA = malloc(aN * 4), *hB = bN ? malloc(bN * 4) : NULL;
  for (u64 i = 0; i < aN; i++) hA[i] = (float)((i % 7) * 0.5 - 1.0);
  if (hB) for (u64 i = 0; i < bN; i++) hB[i] = (float)((i % 5) * 0.25 + 0.3);
  b->buf_write(bA, hA, aN * 4);
  if (bB) b->buf_write(bB, hB, bN * 4);

  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  kernel_inputs_reserve(ke, 2);
  ke->cached_lift.store_root = st;
  ke->cached_lift.out_buf    = C;
  ke->cached_lift.in_bufs[0] = A;
  if (B) ke->cached_lift.in_bufs[1] = B;
  ke->cached_lift.n_inputs = B ? 2 : 1;
  ke->n_inputs    = B ? 2 : 1;
  ke->output_numel = (u32)cN;

  // Optionally apply the matmul tensor-core opts the production hand-opts
  // path applies (THVM_BENCH_OPT=1): TC marker + GLOBAL on the 8-multiple
  // output axes, so the bench measures the parallel simdgroup_matrix
  // kernel rather than the un-optimised default.
  if (!strcmp(which, "matmul") && getenv("THVM_BENCH_OPT") && (n % 8) == 0) {
    KOpt tc = { KOP_TC, 0, 8 };
    kernel_apply_opt(ke, tc);
    KOpt g0 = { KOP_GLOBAL, 0, (u32)n };
    kernel_apply_opt(ke, g0);
    KOpt g1 = { KOP_GLOBAL, 1, (u32)n };
    kernel_apply_opt(ke, g1);
  }

  u32 in_buf_ids[2] = { bA, bB };
  float *hC = malloc(cN * 4);

  // warmup + sync
  for (int i = 0; i < 3; i++) b->dispatch_kernel(ke, in_buf_ids, bC);
  b->buf_read(bC, hC, cN * 4);

  // steady-state: reps dispatches, drained by one read.
  u64 t0 = cg_now_us();
  for (int r = 0; r < reps; r++) b->dispatch_kernel(ke, in_buf_ids, bC);
  b->buf_read(bC, hC, cN * 4);
  u64 t1 = cg_now_us();

  double ms = (double)(t1 - t0) / 1000.0 / reps;
  printf("%-5s %-8s n=%-6d %8.4f ms/iter  %8.1f GFLOPS  (out[0]=%.3f)\n",
         dev, which, n, ms, flops / (ms * 1e6), hC[0]);

  free(hA); free(hB); free(hC);
  thvm_free();
  return 0;
}
