// test_metal_stub.c -- THVM_BACKEND env-var swap selects METAL_BACKEND.
//
// The Metal implementation itself is a stub today (returns errors
// for everything that touches compute).  This test only verifies
// that the dispatch in thvm_init picks the right Backend struct
// based on the env var.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  TEST_BEGIN("metal/default-backend-is-cpu");
  unsetenv("THVM_BACKEND");
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  CHECK_EQ(CURRENT_BACKEND->id, 1);
  thvm_free();

  TEST_BEGIN("metal/THVM_BACKEND-metal-selects-metal-stub");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);
  CHECK_EQ(CURRENT_BACKEND->id, 2);
  // Stub buf_alloc returns the "no buffer" sentinel.
  CHECK_EQ(CURRENT_BACKEND->buf_alloc(64), 0);
  thvm_free();

  TEST_BEGIN("metal/unknown-value-falls-back-to-cpu");
  setenv("THVM_BACKEND", "vulkan", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  thvm_free();

  unsetenv("THVM_BACKEND");
  TEST_REPORT();
}
