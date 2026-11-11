// test_metal_stub.c -- the DEV env var picks the default backend.  This binary
// builds with the Metal stub, whose init fails, so DEV=metal leaves the CPU
// the default and Metal reports why (test_backend_unavailable covers the
// realize side).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  TEST_BEGIN("metal/default-backend-is-cpu");
  unsetenv("DEV");
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  CHECK_EQ(CURRENT_BACKEND->id, 1);
  thvm_free();

  TEST_BEGIN("metal/DEV-metal-without-metal-keeps-cpu");
  setenv("DEV", "metal", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  CHECK(ctx_ensure_backend(THVM_DEV_METAL) == NULL);
  CHECK(strstr(backend_unavailable_reason(THVM_DEV_METAL), "no Metal") != NULL);
  // Stub buf_alloc returns the "no buffer" sentinel.
  CHECK_EQ(METAL_BACKEND.buf_alloc(64), 0);
  thvm_free();

  TEST_BEGIN("metal/unknown-value-falls-back-to-cpu");
  setenv("DEV", "vulkan", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  thvm_free();

  unsetenv("DEV");
  TEST_REPORT();
}
