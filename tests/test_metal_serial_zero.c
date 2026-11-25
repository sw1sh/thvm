// A serial replay run zeroes a recycled output before its kernel writes it
// (thvm_metal_serial_run, zero_first), so a kernel that writes only part of the
// buffer leaves zeros in the rest, as it would in a fresh one.  The fill clears
// 256-byte blocks and then the bytes left over: each size below is under one
// block, whole blocks, or blocks plus a remainder, in 4-byte and 1-byte
// elements.  Built -DTHVM_HAS_METAL.
#include "../src/thvm.c"
#include "test.h"

static const char *FLOAT_SRC =
  "#include <metal_stdlib>\nusing namespace metal;\n"
  "kernel void first_f(device const float *x [[buffer(0)]], device float *y [[buffer(1)]],\n"
  "    uint gid [[thread_position_in_grid]]) { if (gid == 0) y[0] = x[0] + 1.0f; }\n";

static const char *BYTE_SRC =
  "#include <metal_stdlib>\nusing namespace metal;\n"
  "kernel void first_b(device const uchar *x [[buffer(0)]], device uchar *y [[buffer(1)]],\n"
  "    uint gid [[thread_position_in_grid]]) { if (gid == 0) y[0] = x[0] + 1; }\n";

static u32 metal_tensor(u32 numel, u32 dtype) {
  Shape sh = {0};
  sh.ndim = 1;
  sh.dims[0] = numel;
  return tensor_alloc(CURRENT_BACKEND, sh, dtype);
}

// Fills y with 0xA5 bytes, runs the one-element kernel as a serial replay run
// with zero_first set, and returns the number of bytes of y that are wrong.
static u64 run_case(u32 numel, u32 dtype, const char *name, const char *src) {
  u64 esz    = dtype_itemsize(dtype);
  u64 nbytes = (u64)numel * esz;
  u32 xt = metal_tensor(1, dtype);
  u32 yt = metal_tensor(numel, dtype);
  u8 *host = (u8 *)malloc((size_t)nbytes);
  memset(host, 0xA5, (size_t)nbytes);
  CURRENT_BACKEND->buf_write(TENS[yt].buf_id, host, nbytes);
  float xf = 41.0f;
  u8    xb = 41;
  CURRENT_BACKEND->buf_write(TENS[xt].buf_id, dtype == DT_FP32 ? (void *)&xf : (void *)&xb, esz);

  u32 grid[3] = {1, 1, 1}, block[3] = {1, 1, 1};
  u32 args[2] = {xt, yt};
  u32 outs[1] = {1};
  u32 kid = kernel_emit_custom_program(name, src, grid, block, args, 2, outs, 1, 0);
  if (kid == 0) { free(host); return nbytes; }

  JitReplayDispatch rec;
  memset(&rec, 0, sizeof(rec));
  rec.kid = kid;
  rec.n_inputs = 2;
  rec.in_buf_ids[0] = TENS[xt].buf_id;
  rec.in_buf_ids[1] = TENS[yt].buf_id;
  rec.out_buf_id = TENS[yt].buf_id;
  u8 kind = 1, zero_first = 1;
  u32 groups = 0, threads = 0;
  TenDesc shell;
  memset(&shell, 0, sizeof(shell));
  if (thvm_metal_serial_run(&rec, 1, &kind, &zero_first, &groups, &threads, &shell) != 0) {
    free(host);
    return nbytes;
  }
  CURRENT_BACKEND->buf_read(TENS[yt].buf_id, host, nbytes);

  u64 wrong = 0;
  if (dtype == DT_FP32) {
    float first;
    memcpy(&first, host, sizeof(first));
    if (first != 42.0f) wrong += esz;
  } else if (host[0] != 42) {
    wrong += esz;
  }
  for (u64 i = esz; i < nbytes; i++) {
    if (host[i] != 0) wrong++;
  }
  free(host);
  return wrong;
}

int main(void) {
  setenv("DEV", "metal", 1);
  thvm_init();
  TEST_BEGIN("metal/serial-replay-zeroes-a-recycled-output");
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);

  static const u32 float_numels[] = {1, 3, 63, 64, 65, 250, 4099, 262147};
  for (u32 i = 0; i < sizeof(float_numels) / sizeof(float_numels[0]); i++) {
    u64 wrong = run_case(float_numels[i], DT_FP32, "first_f", FLOAT_SRC);
    if (wrong != 0) printf("  f32 x %u: %llu wrong bytes\n", float_numels[i], (unsigned long long)wrong);
    CHECK_EQ(wrong, 0);
  }
  static const u32 byte_numels[] = {1, 2, 255, 256, 257, 1001, 70001};
  for (u32 i = 0; i < sizeof(byte_numels) / sizeof(byte_numels[0]); i++) {
    u64 wrong = run_case(byte_numels[i], DT_UINT8, "first_b", BYTE_SRC);
    if (wrong != 0) printf("  u8 x %u: %llu wrong bytes\n", byte_numels[i], (unsigned long long)wrong);
    CHECK_EQ(wrong, 0);
  }
  thvm_free();
  TEST_REPORT();
}
