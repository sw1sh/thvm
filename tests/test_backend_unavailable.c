// test_backend_unavailable.c -- a backend that fails to start computes nothing.  Where Metal
// builds, this binary links the real Metal backend with its metallib path pointing at no file
// (Makefile: backend_metal_nolib.o); elsewhere the Metal stub, whose init reports that the build
// has no Metal.  Either way the backend is refused with a reason, a realize on it fails instead of
// reading back zeros, the same graph on the CPU is right, and DEV=metal leaves the CPU the
// default (tinygrad raises when a device fails to open).
#include "../src/thvm.c"
#include "test.h"

#define N 3
static f32 const XS[N] = { 1.0f, 2.0f, 3.0f };

static Term ten_cpu(void) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = N;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)XS, sizeof XS);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static void check_doubled(Term r) {
  r = term_resolve(r);
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  f32 got[N];
  TENS[term_val(r)].backend->buf_read(TENS[term_val(r)].buf_id, got, sizeof got);
  for (u32 i = 0; i < N; i++) {
    CHECK(got[i] == 2.0f * XS[i]);
  }
}

int main(void) {
  unsetenv("DEV");
  thvm_init();

  TEST_BEGIN("backend-unavailable/metal-refused-with-reason");
  CHECK(ctx_ensure_backend(THVM_DEV_METAL) == NULL);
  CHECK(backend_unavailable_reason(THVM_DEV_METAL)[0] != '\0');
  CHECK(backend_unavailable_reason(THVM_DEV_CPU)[0] == '\0');

  TEST_BEGIN("backend-unavailable/realize-on-metal-fails");
  Term x = ten_cpu();
  Term m = uop_copy_dev(x, THVM_DEV_METAL);
  CHECK(thvm_realize(uop_binary(UOP_ADD, m, m)) == 0);
  CHECK(strstr(thvm_realize_error(), "metal") != NULL);

  TEST_BEGIN("backend-unavailable/cpu-graph-still-right");
  check_doubled(thvm_realize(uop_binary(UOP_ADD, x, x)));
  thvm_free();

  TEST_BEGIN("backend-unavailable/DEV-metal-keeps-cpu-default");
  setenv("DEV", "metal", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  Term y = ten_cpu();
  check_doubled(thvm_realize(uop_binary(UOP_ADD, y, y)));
  thvm_free();

  unsetenv("DEV");
  TEST_REPORT();
}
