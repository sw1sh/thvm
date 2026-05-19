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

  // === vtable dispatch: CUDA_BACKEND.dispatch_kernel end to end ======
  // Stage 3: a KernelEntry routed through cuda_dispatch_kernel (the
  // backend-vtable entry THVM_BACKEND=cuda selects) renders the lifted
  // DAG, nvrtc-compiles it, packs the cuLaunchKernel args and launches.
  // Build the matmul DAG, populate a synthetic KernelEntry's
  // cached_lift exactly as the schedule's materialize lift would, and
  // dispatch -- no standalone thvm_cuda_* helper in the path.
  TEST_BEGIN("cuda-backend/vtable-dispatch-kernel-matmul");
  {
    enum { M = 8, K = 4, N = 8 };
    u32 dC[2] = { M, N }, dA[2] = { M, K }, dB[2] = { K, N };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB, 2);
    Term r_m = uop_range(0, KAX_LOOP, M);
    Term r_n = uop_range(1, KAX_LOOP, N);
    Term r_k = uop_range(2, KAX_REDUCE, K);
    Term kK = uop_const(DT_INT32, K), kN = uop_const(DT_INT32, N);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kK), r_k);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, kN), r_n);
    Term mul = uop_binary(UOP_MUL, uop_index_e(A, addrA), uop_index_e(B, addrB));
    Term red = uop_reduce(REDUCE_SUM, 2, mul);
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kN), r_n);
    Term st = uop_store(C, addrC, red);

    // Synthetic KernelEntry: cached_lift.store_root + n_inputs +
    // output_numel are everything cuda_dispatch_kernel reads.
    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    ke->cached_lift.store_root = st;
    ke->cached_lift.n_inputs = 2;
    ke->n_inputs = 2;
    ke->output_numel = M * N;

    float hA[M * K], hB[K * N], hC_ref[M * N], hC_gpu[M * N];
    for (int i = 0; i < M * K; i++) hA[i] = (float)(i % 7) * 0.5f - 1.0f;
    for (int i = 0; i < K * N; i++) hB[i] = (float)(i % 5) * 0.25f + 0.3f;
    for (int m = 0; m < M; m++)
      for (int n = 0; n < N; n++) {
        float acc = 0.0f;
        for (int k = 0; k < K; k++) acc += hA[m * K + k] * hB[k * N + n];
        hC_ref[m * N + n] = acc;
      }

    u32 bC = thvm_cuda_buf_alloc((u64)M * N * sizeof(float));
    u32 bA = thvm_cuda_buf_alloc((u64)M * K * sizeof(float));
    u32 bB = thvm_cuda_buf_alloc((u64)K * N * sizeof(float));
    CHECK(thvm_cuda_buf_write(bA, hA, sizeof hA) == 0);
    CHECK(thvm_cuda_buf_write(bB, hB, sizeof hB) == 0);

    // Drive the backend vtable directly: in_buf_ids = {A, B} (slot
    // order), out_buf_id = C.
    u32 in_buf_ids[2] = { bA, bB };
    int rc = CUDA_BACKEND.dispatch_kernel(ke, in_buf_ids, bC);
    if (rc != 0) {
      fprintf(stderr, "  vtable dispatch failed: %s\n", thvm_cuda_last_error());
    }
    CHECK(rc == 0);

    CHECK(thvm_cuda_buf_read(bC, hC_gpu, sizeof hC_gpu) == 0);
    int all_ok = 1;
    for (int i = 0; i < M * N; i++) {
      if (!approx_eq(hC_gpu[i], hC_ref[i])) {
        all_ok = 0;
        fprintf(stderr, "  vtable matmul mismatch at %d: gpu=%f ref=%f\n",
                i, hC_gpu[i], hC_ref[i]);
      }
    }
    CHECK(all_ok);

    thvm_cuda_buf_free(bC);
    thvm_cuda_buf_free(bA);
    thvm_cuda_buf_free(bB);
  }

  // === vtable dispatch of a KOP_LOCAL-split kernel ===================
  // KOP_LOCAL splits the M output axis RANGE(0,LOOP,8) into an outer
  // LOOP(2) + an inner LOCAL(4).  cuda_dag_dispatch_shape must then
  // launch grid = 2*N, block = 4 (LOOP product x LOCAL product) rather
  // than the flat 64-thread shape -- and the result must still match
  // the CPU reference.  Exercises cuda_dag_dispatch_shape's LOCAL arm
  // + the renderer's tg/tt decode agreeing on axis order.
  TEST_BEGIN("cuda-backend/vtable-dispatch-kop-local");
  {
    enum { M = 8, K = 4, N = 8 };
    u32 dC[2] = { M, N }, dA[2] = { M, K }, dB[2] = { K, N };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB, 2);
    Term r_m = uop_range(0, KAX_LOOP, M);
    Term r_n = uop_range(1, KAX_LOOP, N);
    Term r_k = uop_range(2, KAX_REDUCE, K);
    Term kK = uop_const(DT_INT32, K), kN = uop_const(DT_INT32, N);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kK), r_k);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, kN), r_n);
    Term mul = uop_binary(UOP_MUL, uop_index_e(A, addrA), uop_index_e(B, addrB));
    Term red = uop_reduce(REDUCE_SUM, 2, mul);
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kN), r_n);
    Term st = uop_store(C, addrC, red);

    KOpt opt = { .op = KOP_LOCAL, .axis = 0, .arg = 4 };
    Term st_local = uop_dag_apply_kopt(st, opt);
    CHECK(st_local != 0);

    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    ke->cached_lift.store_root = st_local;
    ke->cached_lift.n_inputs = 2;
    ke->n_inputs = 2;
    ke->output_numel = M * N;

    float hA[M * K], hB[K * N], hC_ref[M * N], hC_gpu[M * N];
    for (int i = 0; i < M * K; i++) hA[i] = (float)(i % 7) * 0.5f - 1.0f;
    for (int i = 0; i < K * N; i++) hB[i] = (float)(i % 5) * 0.25f + 0.3f;
    for (int m = 0; m < M; m++)
      for (int n = 0; n < N; n++) {
        float acc = 0.0f;
        for (int k = 0; k < K; k++) acc += hA[m * K + k] * hB[k * N + n];
        hC_ref[m * N + n] = acc;
      }

    u32 bC = thvm_cuda_buf_alloc((u64)M * N * sizeof(float));
    u32 bA = thvm_cuda_buf_alloc((u64)M * K * sizeof(float));
    u32 bB = thvm_cuda_buf_alloc((u64)K * N * sizeof(float));
    CHECK(thvm_cuda_buf_write(bA, hA, sizeof hA) == 0);
    CHECK(thvm_cuda_buf_write(bB, hB, sizeof hB) == 0);

    u32 in_buf_ids[2] = { bA, bB };
    int rc = CUDA_BACKEND.dispatch_kernel(ke, in_buf_ids, bC);
    if (rc != 0) {
      fprintf(stderr, "  vtable LOCAL dispatch failed: %s\n",
              thvm_cuda_last_error());
    }
    CHECK(rc == 0);

    CHECK(thvm_cuda_buf_read(bC, hC_gpu, sizeof hC_gpu) == 0);
    int all_ok = 1;
    for (int i = 0; i < M * N; i++) {
      if (!approx_eq(hC_gpu[i], hC_ref[i])) {
        all_ok = 0;
        fprintf(stderr, "  LOCAL-split matmul mismatch at %d: gpu=%f ref=%f\n",
                i, hC_gpu[i], hC_ref[i]);
      }
    }
    CHECK(all_ok);

    thvm_cuda_buf_free(bC);
    thvm_cuda_buf_free(bA);
    thvm_cuda_buf_free(bB);
  }

  // === vtable dispatch of a KOP_UPCAST-split kernel ==================
  // KOP_UPCAST splits the M output axis RANGE(0,LOOP,8) into an outer
  // LOOP(2) + an inner UPCAST(4): each thread now computes 4 M-rows.
  // UPCAST is in-thread, so cuda_dag_dispatch_shape leaves it out of
  // the grid/block -- the launch stays the flat LOOP-product shape and
  // the renderer's UPCAST loop covers the 4 rows per thread.  Result
  // must still match the CPU reference.
  TEST_BEGIN("cuda-backend/vtable-dispatch-kop-upcast");
  {
    enum { M = 8, K = 4, N = 8 };
    u32 dC[2] = { M, N }, dA[2] = { M, K }, dB[2] = { K, N };
    Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dC, 0);
    Term A = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dA, 1);
    Term B = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dB, 2);
    Term r_m = uop_range(0, KAX_LOOP, M);
    Term r_n = uop_range(1, KAX_LOOP, N);
    Term r_k = uop_range(2, KAX_REDUCE, K);
    Term kK = uop_const(DT_INT32, K), kN = uop_const(DT_INT32, N);
    Term addrA = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kK), r_k);
    Term addrB = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_k, kN), r_n);
    Term mul = uop_binary(UOP_MUL, uop_index_e(A, addrA), uop_index_e(B, addrB));
    Term red = uop_reduce(REDUCE_SUM, 2, mul);
    Term addrC = uop_int_binary(UOP_IADD,
                                uop_int_binary(UOP_IMUL, r_m, kN), r_n);
    Term st = uop_store(C, addrC, red);

    KOpt opt = { .op = KOP_UPCAST, .axis = 0, .arg = 4 };
    Term st_up = uop_dag_apply_kopt(st, opt);
    CHECK(st_up != 0);

    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    ke->cached_lift.store_root = st_up;
    ke->cached_lift.n_inputs = 2;
    ke->n_inputs = 2;
    ke->output_numel = M * N;

    float hA[M * K], hB[K * N], hC_ref[M * N], hC_gpu[M * N];
    for (int i = 0; i < M * K; i++) hA[i] = (float)(i % 7) * 0.5f - 1.0f;
    for (int i = 0; i < K * N; i++) hB[i] = (float)(i % 5) * 0.25f + 0.3f;
    for (int m = 0; m < M; m++)
      for (int n = 0; n < N; n++) {
        float acc = 0.0f;
        for (int k = 0; k < K; k++) acc += hA[m * K + k] * hB[k * N + n];
        hC_ref[m * N + n] = acc;
      }

    u32 bC = thvm_cuda_buf_alloc((u64)M * N * sizeof(float));
    u32 bA = thvm_cuda_buf_alloc((u64)M * K * sizeof(float));
    u32 bB = thvm_cuda_buf_alloc((u64)K * N * sizeof(float));
    CHECK(thvm_cuda_buf_write(bA, hA, sizeof hA) == 0);
    CHECK(thvm_cuda_buf_write(bB, hB, sizeof hB) == 0);

    u32 in_buf_ids[2] = { bA, bB };
    int rc = CUDA_BACKEND.dispatch_kernel(ke, in_buf_ids, bC);
    if (rc != 0) {
      fprintf(stderr, "  vtable UPCAST dispatch failed: %s\n",
              thvm_cuda_last_error());
    }
    CHECK(rc == 0);

    CHECK(thvm_cuda_buf_read(bC, hC_gpu, sizeof hC_gpu) == 0);
    int all_ok = 1;
    for (int i = 0; i < M * N; i++) {
      if (!approx_eq(hC_gpu[i], hC_ref[i])) {
        all_ok = 0;
        fprintf(stderr, "  UPCAST-split matmul mismatch at %d: gpu=%f ref=%f\n",
                i, hC_gpu[i], hC_ref[i]);
      }
    }
    CHECK(all_ok);

    thvm_cuda_buf_free(bC);
    thvm_cuda_buf_free(bA);
    thvm_cuda_buf_free(bB);
  }

  // === vtable dispatch of a KOP_SIMD_REDUCE multi-row reduce =========
  // out[r] = sum_c in[r,c] for R rows of width C.  KOP_SIMD_REDUCE
  // wraps the reduce in OPT(_, SIMD_REDUCE, _): each row's reduction is
  // a warp-collective __shfl_xor_sync butterfly.  The 32 lanes of a
  // warp cooperate on ONE row, so cuda_dag_dispatch_shape must launch
  // grid = R blocks, block = 32 -- and the renderer (RMU_SIMD_WARP)
  // must decode the row axis from `tg`, not `tid` (a `tid` decode
  // would scatter a warp across 32 distinct rows).  Result must match
  // the per-row CPU reference.
  TEST_BEGIN("cuda-backend/vtable-dispatch-kop-simd-reduce");
  {
    enum { R = 12, C = 96 };
    u32 dOut[1] = { R }, dIn[2] = { R, C };
    Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dOut, 0);
    Term in  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dIn,  1);
    Term r_r = uop_range(0, KAX_LOOP, R);
    Term r_c = uop_range(1, KAX_REDUCE, C);
    Term kC  = uop_const(DT_INT32, C);
    Term addrIn = uop_int_binary(UOP_IADD,
                                 uop_int_binary(UOP_IMUL, r_r, kC), r_c);
    Term red = uop_reduce(REDUCE_SUM, 1, uop_index_e(in, addrIn));
    Term st  = uop_store(out, r_r, red);

    KOpt opt = { .op = KOP_SIMD_REDUCE, .axis = 0, .arg = 0 };
    Term st_sr = uop_dag_apply_kopt(st, opt);
    CHECK(st_sr != 0);
    CHECK(st_sr != st);    // the wrapper actually rewrote the DAG

    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    ke->cached_lift.store_root = st_sr;
    ke->cached_lift.n_inputs = 1;
    ke->n_inputs = 1;
    ke->output_numel = R;

    float hIn[R * C], hOut_ref[R], hOut_gpu[R];
    for (int i = 0; i < R * C; i++) hIn[i] = (float)(i % 13) * 0.1f - 0.6f;
    for (int r = 0; r < R; r++) {
      float acc = 0.0f;
      for (int c = 0; c < C; c++) acc += hIn[r * C + c];
      hOut_ref[r] = acc;
    }

    u32 bOut = thvm_cuda_buf_alloc((u64)R * sizeof(float));
    u32 bIn  = thvm_cuda_buf_alloc((u64)R * C * sizeof(float));
    CHECK(thvm_cuda_buf_write(bIn, hIn, sizeof hIn) == 0);

    u32 in_buf_ids[1] = { bIn };
    int rc = CUDA_BACKEND.dispatch_kernel(ke, in_buf_ids, bOut);
    if (rc != 0) {
      fprintf(stderr, "  vtable SIMD_REDUCE dispatch failed: %s\n",
              thvm_cuda_last_error());
    }
    CHECK(rc == 0);

    CHECK(thvm_cuda_buf_read(bOut, hOut_gpu, sizeof hOut_gpu) == 0);
    int all_ok = 1;
    for (int r = 0; r < R; r++) {
      if (!approx_eq(hOut_gpu[r], hOut_ref[r])) {
        all_ok = 0;
        fprintf(stderr, "  SIMD_REDUCE row %d mismatch: gpu=%f ref=%f\n",
                r, hOut_gpu[r], hOut_ref[r]);
      }
    }
    CHECK(all_ok);

    thvm_cuda_buf_free(bOut);
    thvm_cuda_buf_free(bIn);
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
