// test_cuda_backend.c - Stage 2: end-to-end CUDA backend test.
//
// Mirrors tests/test_metal_real.c in shape, for the CUDA backend.
// This test ONLY builds + runs in the Linux+CUDA build (THVM_HAS_CUDA);
// the Makefile wires it under the CUDA guard, so the macOS build never
// sees it.
//
// The exit criterion of the CUDA backend slice (docs/plans/
// cuda_backend.md Stage 2): build a small UOp DAG, render it via
// cg_render_uop_kernel_cuda_root, compile the .cu with nvrtc, allocate
// device buffers, cuLaunchKernel, copy the result back, and assert it
// matches a CPU reference within fp32 tolerance.
//
// Two DAGs are exercised:
//   1. a plain fp32 matmul  C[MxN] = A[MxK] @ B[KxN]   (scalar
//      accumulator path -- on the V100 WMMA is fp16-only, so an fp32
//      matmul renders the tiled-scalar fallback)
//   2. a vector reduce       out[0] = sum(in[k])
//
// Both go render -> nvrtc -> alloc -> launch -> read-back -> compare.

#include "../src/thvm.c"
#include "test.h"

// fp32 comparison tolerance: a 4..32-wide K-reduction in fp32 has
// rounding on the order of K*eps; 1e-3 is comfortably loose.
static int approx_eq(float a, float b) {
  float d = a - b;
  if (d < 0) d = -d;
  return d <= 1e-3f;
}

int main(void) {
  thvm_init();

  // The CUDA backend may not be present (no device / no driver).  If
  // cuda_init declines, the suite stays green via PENDING -- the test
  // is a hard requirement only on the GPU pod.
  TEST_BEGIN("cuda-backend/init");
  if (thvm_cuda_init() != 0) {
    printf("  pend  no CUDA device: %s\n", thvm_cuda_last_error());
    thvm_free();
    return 0;
  }
  CHECK(thvm_cuda_available() == 1);

  // === matmul: C[MxN] = A[MxK] @ B[KxN] ==============================
  TEST_BEGIN("cuda-backend/matmul-fp32-vs-cpu-reference");
  {
    enum { M = 8, K = 4, N = 8 };
    // --- build the UOp DAG ---------------------------------------
    // STORE(C[m*N+n], REDUCE(MUL(A[m*K+k], B[k*N+n]), SUM, k))
    u32 dC[2] = { M, N };
    u32 dA[2] = { M, K };
    u32 dB[2] = { K, N };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB, 2);
    Term r_m = uop_range(0, KAX_LOOP, M);
    Term r_n = uop_range(1, KAX_LOOP, N);
    Term r_k = uop_range(2, KAX_REDUCE, K);
    Term kK  = uop_const(DT_INT32, K);
    Term kN  = uop_const(DT_INT32, N);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kK), r_k);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, kN), r_n);
    Term mul   = uop_binary(UOP_MUL, uop_index_e(A, addrA),
                                     uop_index_e(B, addrB));
    Term red   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul);
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kN), r_n);
    Term st    = uop_store(C, addrC, red);

    // --- render + nvrtc compile ----------------------------------
    char *cu = thvm_cuda_render(st, "k_mm");
    CHECK(cu != NULL);
    CUfunction func = thvm_cuda_compile(cu, "k_mm");
    if (func == NULL) {
      fprintf(stderr, "  matmul compile failed: %s\n", thvm_cuda_last_error());
    }
    CHECK(func != NULL);

    // --- host inputs + CPU reference -----------------------------
    float hA[M * K], hB[K * N], hC_ref[M * N], hC_gpu[M * N];
    for (int i = 0; i < M * K; i++) hA[i] = (float)(i % 7) * 0.5f - 1.0f;
    for (int i = 0; i < K * N; i++) hB[i] = (float)(i % 5) * 0.25f + 0.3f;
    for (int m = 0; m < M; m++) {
      for (int n = 0; n < N; n++) {
        float acc = 0.0f;
        for (int k = 0; k < K; k++) acc += hA[m * K + k] * hB[k * N + n];
        hC_ref[m * N + n] = acc;
      }
    }

    // --- device buffers + upload ---------------------------------
    u32 bC = thvm_cuda_buf_alloc((u64)M * N * sizeof(float));
    u32 bA = thvm_cuda_buf_alloc((u64)M * K * sizeof(float));
    u32 bB = thvm_cuda_buf_alloc((u64)K * N * sizeof(float));
    CHECK(bC != 0 && bA != 0 && bB != 0);
    CHECK(thvm_cuda_buf_write(bA, hA, sizeof hA) == 0);
    CHECK(thvm_cuda_buf_write(bB, hB, sizeof hB) == 0);

    // --- launch --------------------------------------------------
    // The renderer promotes the two output axes (m, n) onto a 1-D
    // grid; the kernel decodes a0/a1 from `tid` and guards
    // `tid >= M*N`.  So total threads = M*N, here in one block.
    u32 buf_ids[3] = { bC, bA, bB };
    CUdeviceptr dptrs[3];
    void *args[3];
    thvm_cuda_pack_args(buf_ids, 3, dptrs, args);
    u32 total = M * N;
    CHECK(thvm_cuda_launch(func, /*grid_x=*/1, /*block_x=*/total, args) == 0);

    // --- read back + compare -------------------------------------
    CHECK(thvm_cuda_buf_read(bC, hC_gpu, sizeof hC_gpu) == 0);
    int all_ok = 1;
    for (int i = 0; i < M * N; i++) {
      if (!approx_eq(hC_gpu[i], hC_ref[i])) {
        all_ok = 0;
        fprintf(stderr, "  matmul mismatch at %d: gpu=%f ref=%f\n",
                i, hC_gpu[i], hC_ref[i]);
      }
    }
    CHECK(all_ok);

    thvm_cuda_buf_free(bC);
    thvm_cuda_buf_free(bA);
    thvm_cuda_buf_free(bB);
    free(cu);
  }

  // === reduce: out[0] = sum(in[k]) ===================================
  TEST_BEGIN("cuda-backend/reduce-sum-vs-cpu-reference");
  {
    enum { LEN = 64 };
    // STORE(out[0], REDUCE(in[k], SUM, k))
    u32 dI[1] = { LEN };
    u32 dO[1] = { 1 };
    Term in  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dI, 1);
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dO, 0);
    Term r_k = uop_range(0, KAX_REDUCE, LEN);
    Term red = uop_reduce(REDUCE_SUM, /*axis=*/0, uop_index_e(in, r_k));
    Term st  = uop_store(out, uop_const(DT_INT32, 0), red);

    char *cu = thvm_cuda_render(st, "k_red");
    CHECK(cu != NULL);
    CUfunction func = thvm_cuda_compile(cu, "k_red");
    if (func == NULL) {
      fprintf(stderr, "  reduce compile failed: %s\n", thvm_cuda_last_error());
    }
    CHECK(func != NULL);

    float hIn[LEN], hOut_gpu = 0.0f, ref = 0.0f;
    for (int i = 0; i < LEN; i++) {
      hIn[i] = (float)(i % 11) * 0.125f - 0.4f;
      ref += hIn[i];
    }

    u32 bIn  = thvm_cuda_buf_alloc((u64)LEN * sizeof(float));
    u32 bOut = thvm_cuda_buf_alloc(sizeof(float));
    CHECK(bIn != 0 && bOut != 0);
    CHECK(thvm_cuda_buf_write(bIn, hIn, sizeof hIn) == 0);

    // The scalar-accumulator reduce kernel has no output-axis grid
    // promotion (out is a scalar) -- every thread re-runs the full
    // serial sum and writes out[0].  Launch a single thread so the
    // write is unambiguous.
    u32 buf_ids[2] = { bOut, bIn };
    CUdeviceptr dptrs[2];
    void *args[2];
    thvm_cuda_pack_args(buf_ids, 2, dptrs, args);
    CHECK(thvm_cuda_launch(func, /*grid_x=*/1, /*block_x=*/1, args) == 0);

    CHECK(thvm_cuda_buf_read(bOut, &hOut_gpu, sizeof hOut_gpu) == 0);
    if (!approx_eq(hOut_gpu, ref)) {
      fprintf(stderr, "  reduce mismatch: gpu=%f ref=%f\n", hOut_gpu, ref);
    }
    CHECK(approx_eq(hOut_gpu, ref));

    thvm_cuda_buf_free(bIn);
    thvm_cuda_buf_free(bOut);
    free(cu);
  }

  // === buffer roundtrip: write -> read identity ======================
  TEST_BEGIN("cuda-backend/buf-write-read-roundtrip");
  {
    float src[16], dst[16];
    for (int i = 0; i < 16; i++) src[i] = (float)i * 3.5f - 2.0f;
    u32 b = thvm_cuda_buf_alloc(sizeof src);
    CHECK(b != 0);
    CHECK(thvm_cuda_buf_write(b, src, sizeof src) == 0);
    CHECK(thvm_cuda_buf_read(b, dst, sizeof dst) == 0);
    int ok = 1;
    for (int i = 0; i < 16; i++) if (src[i] != dst[i]) ok = 0;
    CHECK(ok);
    thvm_cuda_buf_free(b);
  }

  // === jit cache: second compile of the same source is a hit ========
  TEST_BEGIN("cuda-backend/jit-cache-hit-on-identical-source");
  {
    u32 dims[1] = { 4 };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
    Term in0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
    Term r0  = uop_range(0, KAX_LOOP, 4);
    Term ld  = uop_index_e(in0, r0);
    Term st  = uop_store(out, r0, ld);
    char *cu = thvm_cuda_render(st, "k_id");
    CHECK(cu != NULL);
    CUfunction f1 = thvm_cuda_compile(cu, "k_id");
    CUfunction f2 = thvm_cuda_compile(cu, "k_id");
    CHECK(f1 != NULL);
    // Identical source -> cache hit -> same CUfunction handle.
    CHECK(f1 == f2);
    free(cu);
  }

  thvm_cuda_shutdown();
  thvm_free();
  TEST_REPORT();
}
