// test_cuda_ptx.c - end-to-end validation of the PTX renderer path on a
// real CUDA device.  Builds a simple single-thread elementwise kernel,
// renders it to PTX via cg_render_linearized_ptx, hands the PTX text to
// thvm_cuda_compile (which now detects the `.version` prefix and skips
// nvrtc, loading the PTX directly with cuModuleLoadData), launches it
// with grid=1/block=1, and checks the output against a CPU reference.
//
// This proves the whole bypass-nvrtc pipeline end-to-end:
//   build LinKernel -> cg_render_linearized_ptx -> cuModuleLoadData
//   -> cuLaunchKernel -> read back -> compare.
//
// The single output axis indexes the store, so the PTX renderer PROMOTES
// it to a parallel thread (M3a thread geometry): each thread decodes its
// element index from the flat thread id and is gated by a `tid >= N`
// bounds guard.  The launch is grid=1 / block=N (one thread per element).
// The opt-rich parallel-accumulator (UPCAST) shape is milestone 3b/c.
//
// Builds + runs ONLY in the Linux+CUDA build (THVM_HAS_CUDA); the
// Makefile wires it under the CUDA guard.

#include "../src/thvm.c"
#include "test.h"

static int approx_eq(float a, float b) {
  float d = a - b;
  if (d < 0) d = -d;
  return d <= 1e-4f;
}

// Render a linearized kernel to a heap PTX string (caller frees).
static char *render_ptx_str(Term sink, const char *name, int sm) {
  LinKernel lk;
  if (!uop_linearize(sink, &lk)) return NULL;
  static char buf[1 << 18];
  FILE *fp = fmemopen(buf, sizeof(buf) - 1, "w");
  if (fp == NULL) return NULL;
  int ok = cg_render_linearized_ptx(&lk, name, sm, fp);
  long n = ftell(fp);
  fclose(fp);
  if (!ok) return NULL;
  if (n < 0) n = 0;
  buf[n] = 0;
  char *out = (char *)malloc((size_t)n + 1);
  if (out == NULL) return NULL;
  memcpy(out, buf, (size_t)n + 1);
  return out;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("cuda-ptx/init");
  if (thvm_cuda_init() != 0) {
    printf("  pend  no CUDA device: %s\n", thvm_cuda_last_error());
    thvm_free();
    return 0;
  }
  CHECK(thvm_cuda_available() == 1);

  // === out[i] = in[i] * 2 + 1, single-thread loop over N ============
  TEST_BEGIN("cuda-ptx/elementwise-loop-vs-cpu-reference");
  {
    enum { N = 16 };
    u32 dims[1] = { N };
    Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
    Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
    Term r       = uop_range(0, KAX_LOOP, N);
    Term ld      = uop_load(uop_index_e(in_buf, r));
    Term two     = uop_const(DT_FP32, 0x40000000u);   // 2.0f
    Term one     = uop_const(DT_FP32, 0x3f800000u);   // 1.0f
    Term mul     = uop_binary(UOP_MUL, ld, two);
    Term add     = uop_binary(UOP_ADD, mul, one);
    Term st      = uop_store(out_buf, uop_index_e(out_buf, r), add);

    int sm = cuda_device_sm();
    if (sm <= 0) sm = 70;
    char *ptx = render_ptx_str(st, "ptx_ew", sm);
    CHECK(ptx != NULL);
    // Sanity: it really is PTX, not CUDA-C.
    CHECK(strstr(ptx, ".visible .entry ptx_ew") != NULL);

    CUfunction func = thvm_cuda_compile(ptx, "ptx_ew");
    if (func == NULL) {
      fprintf(stderr, "  ptx compile/load failed: %s\n", thvm_cuda_last_error());
      fprintf(stderr, "----- PTX -----\n%s\n---------------\n", ptx);
    }
    CHECK(func != NULL);

    float hIn[N], hRef[N], hGpu[N];
    for (int i = 0; i < N; i++) {
      hIn[i] = (float)i * 0.5f - 3.0f;
      hRef[i] = hIn[i] * 2.0f + 1.0f;
    }

    u32 bOut = thvm_cuda_buf_alloc((u64)N * sizeof(float));
    u32 bIn  = thvm_cuda_buf_alloc((u64)N * sizeof(float));
    CHECK(bOut != 0 && bIn != 0);
    CHECK(thvm_cuda_buf_write(bIn, hIn, sizeof hIn) == 0);

    u32 buf_ids[2] = { bOut, bIn };
    CUdeviceptr dptrs[2];
    void *args[2];
    thvm_cuda_pack_args(buf_ids, 2, dptrs, args);
    // Promoted parallel axis: one thread per element, guarded by tid>=N.
    CHECK(thvm_cuda_launch(func, /*grid_x=*/1, /*block_x=*/N, args) == 0);

    CHECK(thvm_cuda_buf_read(bOut, hGpu, sizeof hGpu) == 0);
    int all_ok = 1;
    for (int i = 0; i < N; i++) {
      if (!approx_eq(hGpu[i], hRef[i])) {
        all_ok = 0;
        fprintf(stderr, "  ptx mismatch at %d: gpu=%f ref=%f\n",
                i, hGpu[i], hRef[i]);
      }
    }
    CHECK(all_ok);

    thvm_cuda_buf_free(bOut);
    thvm_cuda_buf_free(bIn);
    free(ptx);
  }

  // === 2-D grid: out[i,j] = in[i,j] + 1, MxM, decoded from one flat ===
  // thread id ((tid/M)%M, tid%M).  Validates the multi-axis div/rem
  // promotion on real hardware (the conv-output-axis decode shape).
  TEST_BEGIN("cuda-ptx/2d-grid-decode-vs-cpu-reference");
  {
    enum { M = 4 };
    u32 dims[2] = { M, M };
    Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims, 0);
    Term in_buf  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims, 1);
    Term ri      = uop_range(0, KAX_LOOP, M);
    Term rj      = uop_range(1, KAX_LOOP, M);
    Term addr    = uop_int_binary(UOP_IADD,
                     uop_int_binary(UOP_IMUL, ri, uop_const(DT_INT32, M)), rj);
    Term ld      = uop_load(uop_index_e(in_buf, addr));
    Term one     = uop_const(DT_FP32, 0x3f800000u);
    Term add     = uop_binary(UOP_ADD, ld, one);
    Term st      = uop_store(out_buf, uop_index_e(out_buf, addr), add);

    int sm = cuda_device_sm();
    if (sm <= 0) sm = 70;
    char *ptx = render_ptx_str(st, "ptx_2d", sm);
    CHECK(ptx != NULL);

    CUfunction func = thvm_cuda_compile(ptx, "ptx_2d");
    if (func == NULL)
      fprintf(stderr, "  2d compile/load failed: %s\n", thvm_cuda_last_error());
    CHECK(func != NULL);

    float hIn[M * M], hRef[M * M], hGpu[M * M];
    for (int i = 0; i < M * M; i++) { hIn[i] = (float)i - 5.0f; hRef[i] = hIn[i] + 1.0f; }

    u32 bOut = thvm_cuda_buf_alloc((u64)M * M * sizeof(float));
    u32 bIn  = thvm_cuda_buf_alloc((u64)M * M * sizeof(float));
    CHECK(bOut != 0 && bIn != 0);
    CHECK(thvm_cuda_buf_write(bIn, hIn, sizeof hIn) == 0);

    u32 buf_ids[2] = { bOut, bIn };
    CUdeviceptr dptrs[2];
    void *args[2];
    thvm_cuda_pack_args(buf_ids, 2, dptrs, args);
    CHECK(thvm_cuda_launch(func, /*grid_x=*/1, /*block_x=*/(M * M), args) == 0);

    CHECK(thvm_cuda_buf_read(bOut, hGpu, sizeof hGpu) == 0);
    int all_ok = 1;
    for (int i = 0; i < M * M; i++)
      if (!approx_eq(hGpu[i], hRef[i])) {
        all_ok = 0;
        fprintf(stderr, "  2d mismatch at %d: gpu=%f ref=%f\n", i, hGpu[i], hRef[i]);
      }
    CHECK(all_ok);

    thvm_cuda_buf_free(bOut);
    thvm_cuda_buf_free(bIn);
    free(ptx);
  }

  thvm_free();
  TEST_REPORT();
}
