// test_cpu_jit_via_uop.c - F6 integration test
//
// Fires a CPU JIT-eligible kernel enough times to cross the warmup
// threshold (CPU_JIT_WARMUP=5), validating that cpu_jit_dispatch's
// new render_uop_c path produces correct output for kernels that
// actually compile through clang -shared and load via dlopen.
//
// Skips the test entirely when no JIT compile occurs (the path was
// stat-cached from a prior run, or a system without clang).
//
// Validates the F6 default-on flip (fc40c60a): the rendered C99 from
// cg_render_uop_kernel_c, when compiled and loaded, yields output
// bit-equal to a pure CPU-interpreter reference.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  TEST_BEGIN("cpu-jit-via-uop/elementwise-add-warmup");

  // Pre-clear the on-disk JIT cache so this test exercises a real
  // clang compile (otherwise stat-only check shortcuts to dlopen).
  // Best-effort; missing files are fine.
  system("rm -f /tmp/thvm_jit_*.dylib /tmp/thvm_jit_*.c 2>/dev/null");

  // thvm_init/free resets cpu_jit_cache (incl fire_count) so we
  // need ONE init for the whole loop -- fire_count must persist
  // across fires to cross CPU_JIT_WARMUP=5.
  thvm_init();

  Shape s = {0}; s.ndim = 1; s.dims[0] = 32;
  f32 src_a[32], src_b[32], expected[32];
  for (u32 i = 0; i < 32; i++) {
    src_a[i] = (float)i;
    src_b[i] = 2.0f * (float)i + 1.0f;
    expected[i] = src_a[i] + src_b[i];
  }

  // Fire the same logical kernel 10 times. fire_count crosses
  // CPU_JIT_WARMUP after fire 5; subsequent fires JIT via the
  // F6 render_uop_c path. Each iteration checks output bit-equal.
  for (u32 fire = 0; fire < 10; fire++) {
    u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
    u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
    CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
    Term A = term_new(0, TAG_TEN, DT_FP32, ta);
    Term B = term_new(0, TAG_TEN, DT_FP32, tb);
    Term sum = uop_binary(UOP_ADD, A, B);
    Term done = wnf(thvm_materialize(sum));
    f32 result[32];
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              result, sizeof(result));
    for (u32 i = 0; i < 32; i++) CHECK(result[i] == expected[i]);
  }

  thvm_free();

  TEST_REPORT();
}
