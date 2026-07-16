// Raw custom-kernel injection on the CUDA backend: the same eager +
// JIT capture/replay contract test_custom_kernel.c proves on CPU, with
// hand-written CUDA-C __global__ kernels over CUDA device buffers.
// cuda_custom_dispatch compiles via the shared nvrtc path
// (cuda_jit_compile) and launches on CUDA_CUR_STREAM, so the replay's
// cuGraph capture (cuda_jit_graph_capture_begin) records the custom
// launches like any rendered kernel -- replays 2 and 3 run through the
// cached CUgraphExec against in-place-refreshed input bytes.
//
// Linux+CUDA only (Makefile adds it to TESTS under the CUDA guard);
// pends cleanly when no device is present.
#include "../src/thvm.c"
#include "test.h"

static u32 mk_cuda_f32(u32 n, float const *data) {
  Shape s = {0};
  s.ndim = 1;
  s.dims[0] = n;
  u32 tid = tensor_alloc(&CUDA_BACKEND, s, DT_FP32);
  if (data != NULL) {
    CUDA_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, (u64)n * 4);
  } else {
    float z[64] = {0};
    CUDA_BACKEND.buf_write(TENS[tid].buf_id, z, (u64)n * 4);
  }
  return tid;
}

// CUDA custom-kernel ABI: natural per-argument pointers, launched with
// the spec's grid/block dims (one thread per element here).
static char const *SRC_ADDONE =
  "extern \"C\" __global__ void addone(const float *x, float *y) {\n"
  "  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;\n"
  "  if (i < 8) y[i] = x[i] + 1.0f;\n"
  "}\n";

static char const *SRC_DOUBLE =
  "extern \"C\" __global__ void dbl(const float *x, float *s) {\n"
  "  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;\n"
  "  if (i < 8) s[i] = x[i] * 2.0f;\n"
  "}\n";

static char const *SRC_ADD3 =
  "extern \"C\" __global__ void add3(const float *s, float *y) {\n"
  "  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;\n"
  "  if (i < 8) y[i] = s[i] + 3.0f;\n"
  "}\n";

static char const *SRC_TWO_OUT =
  "extern \"C\" __global__ void twoout(const float *x, float *y1, float *y2) {\n"
  "  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;\n"
  "  if (i < 8) {\n"
  "    y1[i] = x[i] + 1.0f;\n"
  "    y2[i] = x[i] * 10.0f;\n"
  "  }\n"
  "}\n";

int main(void) {
  thvm_init();
  TEST_BEGIN("custom-kernel-cuda/init");
  if (thvm_cuda_init() != 0) {
    printf("  pend  no CUDA device: %s\n", thvm_cuda_last_error());
    thvm_free();
    return 0;
  }
  TEST_BEGIN("custom-kernel-cuda/eager+jit-capture-replay");
  u32 n = 8;
  u32 grid[3]  = {1, 1, 1};
  u32 block[3] = {n, 1, 1};
  float xv[8], out[8], scratchv[8];

  // === 1. eager add-one ==============================================
  for (u32 i = 0; i < n; i++) xv[i] = (float)i;
  u32 x = mk_cuda_f32(n, xv);
  u32 y = mk_cuda_f32(n, NULL);
  u32 args1[2] = {x, y};
  u32 outs1[1] = {1};
  u32 k_add = kernel_emit_custom_program("addone", SRC_ADDONE, grid, block,
                                         args1, 2, outs1, 1, 0);
  CHECK(k_add != 0);
  CHECK(kernel_custom_fire(k_add) == 0);
  CUDA_BACKEND.buf_read(TENS[y].buf_id, out, (u64)n * 4);
  printf("  eager addone: out[3] = %.1f (want 4)\n", out[3]);
  for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] + 1.0f);

  // === 2. eager 2-kernel chain through a scratch buffer =============
  u32 cx      = mk_cuda_f32(n, xv);
  u32 scratch = mk_cuda_f32(n, NULL);
  u32 cy      = mk_cuda_f32(n, NULL);
  u32 args_d[2] = {cx, scratch};
  u32 args_a[2] = {scratch, cy};
  u32 outs_s[1] = {1};
  u32 k_dbl  = kernel_emit_custom_program("dbl", SRC_DOUBLE, grid, block,
                                          args_d, 2, outs_s, 1, 0);
  u32 k_add3 = kernel_emit_custom_program("add3", SRC_ADD3, grid, block,
                                          args_a, 2, outs_s, 1, 0);
  CHECK(k_dbl != 0 && k_add3 != 0);
  CHECK(kernel_custom_fire(k_dbl) == 0);
  CHECK(kernel_custom_fire(k_add3) == 0);
  CUDA_BACKEND.buf_read(TENS[cy].buf_id, out, (u64)n * 4);
  printf("  eager chain: out[3] = %.1f (want %.1f)\n",
         out[3], xv[3] * 2.0f + 3.0f);
  for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] * 2.0f + 3.0f);

  // === 3. JIT capture of the chain + 3 replays with fresh inputs ====
  // Replay 1 is the cuGraph cache miss (stream capture of the two
  // custom launches); replays 2 and 3 are cuGraphLaunch hits over the
  // same device pointers with in-place-refreshed bytes.
  u32 slot = jit_capture_begin();
  CHECK(slot != 0);
  CHECK(kernel_custom_fire(k_dbl) == 0);
  CHECK(kernel_custom_fire(k_add3) == 0);
  jit_capture_end();
  CHECK(jit_capture_op_count(slot) == 2);

  for (u32 rep = 1; rep <= 3; rep++) {
    for (u32 i = 0; i < n; i++) xv[i] = (float)(10 * rep + i);
    CUDA_BACKEND.buf_write(TENS[cx].buf_id, xv, (u64)n * 4);
    float poison[8];
    for (u32 i = 0; i < n; i++) poison[i] = -999.0f;
    CUDA_BACKEND.buf_write(TENS[scratch].buf_id, poison, (u64)n * 4);
    CUDA_BACKEND.buf_write(TENS[cy].buf_id,      poison, (u64)n * 4);
    jit_replay(slot);
    CUDA_BACKEND.buf_read(TENS[scratch].buf_id, scratchv, (u64)n * 4);
    CUDA_BACKEND.buf_read(TENS[cy].buf_id,      out,      (u64)n * 4);
    printf("  replay %u: scratch[0] = %.1f (want %.1f), out[0] = %.1f "
           "(want %.1f)\n", rep, scratchv[0], xv[0] * 2.0f,
           out[0], xv[0] * 2.0f + 3.0f);
    for (u32 i = 0; i < n; i++) CHECK(scratchv[i] == xv[i] * 2.0f);
    for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] * 2.0f + 3.0f);
  }
  jit_capture_drop(slot);

  // === 4. multi-out kernel under capture/replay ======================
  u32 mx  = mk_cuda_f32(n, xv);
  u32 my1 = mk_cuda_f32(n, NULL);
  u32 my2 = mk_cuda_f32(n, NULL);
  u32 args_m[3] = {mx, my1, my2};
  u32 outs_m[2] = {1, 2};
  u32 k_two = kernel_emit_custom_program("twoout", SRC_TWO_OUT, grid, block,
                                         args_m, 3, outs_m, 2, 0);
  CHECK(k_two != 0);
  slot = jit_capture_begin();
  CHECK(slot != 0);
  CHECK(kernel_custom_fire(k_two) == 0);
  jit_capture_end();

  for (u32 rep = 1; rep <= 3; rep++) {
    for (u32 i = 0; i < n; i++) xv[i] = (float)(7 * rep + i);
    CUDA_BACKEND.buf_write(TENS[mx].buf_id, xv, (u64)n * 4);
    jit_replay(slot);
    float o1[8], o2[8];
    CUDA_BACKEND.buf_read(TENS[my1].buf_id, o1, (u64)n * 4);
    CUDA_BACKEND.buf_read(TENS[my2].buf_id, o2, (u64)n * 4);
    for (u32 i = 0; i < n; i++) CHECK(o1[i] == xv[i] + 1.0f);
    for (u32 i = 0; i < n; i++) CHECK(o2[i] == xv[i] * 10.0f);
  }
  jit_capture_drop(slot);

  TEST_REPORT();
}
