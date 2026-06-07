// test_cuda_mixed.c - mixed CPU+CUDA realize: the device-in-graph port's
// per-op device routing + fire-time cross-device COPY transfer, on the
// CUDA backend.  Mirrors the CPU+Metal mixed test run on macOS: a CUDA
// compute feeds a CPU compute through an explicit COPY boundary, and a
// multi-hop cpu->cuda->cpu->cuda ping-pong.  Built only on Linux+CUDA.
#include "../src/thvm.c"
#include "test.h"

static u32 cpu_tensor(const float *data, u32 n) {
  Shape s = {0}; s.ndim = 1; s.dims[0] = n;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, (u64)n * sizeof(float));
  return tid;
}

static void read_result(Term r, float *out, u32 n) {
  Term t = term_resolve(r);
  u32  tid = (u32)term_val(t);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, (u64)n * sizeof(float));
}

int main(void) {
  thvm_init();                       // CPU default device
  int failures = 0;

  // (x ->cuda  *  y ->cuda) ->cpu  +  x   ==  x*y + x
  TEST_BEGIN("cuda-mixed/cuda-mul-then-cpu-add");
  {
    const float xd[4] = {2.f, 3.f, 4.f, 5.f};
    const float yd[4] = {10.f, 10.f, 10.f, 10.f};
    Term x = term_new(0, TAG_TEN, DT_FP32, cpu_tensor(xd, 4));
    Term y = term_new(0, TAG_TEN, DT_FP32, cpu_tensor(yd, 4));
    Term mul = uop_binary(UOP_MUL, uop_copy_dev(x, THVM_DEV_CUDA),
                                    uop_copy_dev(y, THVM_DEV_CUDA));
    Term add = uop_binary(UOP_ADD, uop_copy_dev(mul, THVM_DEV_CPU), x);
    float out[4] = {0};
    read_result(thvm_realize(add), out, 4);
    const float exp[4] = {22.f, 33.f, 44.f, 55.f};
    for (int i = 0; i < 4; i++) {
      if (!(out[i] > exp[i] - 1e-3f && out[i] < exp[i] + 1e-3f)) {
        printf("  mismatch [%d]: got %f want %f\n", i, out[i], exp[i]);
        failures++;
      }
    }
  }

  // 3-hop ping-pong:  ((x ->cuda + x ->cuda) ->cpu  *  x) ->cuda  ==  2*x*x
  TEST_BEGIN("cuda-mixed/cpu-cuda-cpu-cuda-pingpong");
  {
    const float xd[4] = {2.f, 3.f, 4.f, 5.f};
    Term x = term_new(0, TAG_TEN, DT_FP32, cpu_tensor(xd, 4));
    Term s   = uop_binary(UOP_ADD, uop_copy_dev(x, THVM_DEV_CUDA),
                                    uop_copy_dev(x, THVM_DEV_CUDA));
    Term mul = uop_binary(UOP_MUL, uop_copy_dev(s, THVM_DEV_CPU), x);
    Term z   = uop_copy_dev(mul, THVM_DEV_CUDA);
    float out[4] = {0};
    read_result(thvm_realize(z), out, 4);
    const float exp[4] = {8.f, 18.f, 32.f, 50.f};   // 2*x*x
    for (int i = 0; i < 4; i++) {
      if (!(out[i] > exp[i] - 1e-3f && out[i] < exp[i] + 1e-3f)) {
        printf("  mismatch [%d]: got %f want %f\n", i, out[i], exp[i]);
        failures++;
      }
    }
  }

  printf("  %s  (%d failures)\n", failures == 0 ? "ok" : "FAIL", failures);
  return failures == 0 ? 0 : 1;
}
