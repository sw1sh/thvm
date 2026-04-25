// test_metal_real.c -- dual-TU build smoke test.
//
// Compiled with -DTHVM_HAS_METAL so src/thvm.c skips its C stub
// for the Metal backend; METAL_BACKEND comes from
// build/backend_metal.o (built from src/backend/metal/_.m).  The
// real metal_init still hasn't been written -- this test just
// confirms the dual-TU build works (no duplicate symbols, no
// missing references) and the stub semantics are identical to the
// C stub the existing test_metal_stub.c covers.

// THVM_HAS_METAL is provided by the Makefile's -D flag for this
// binary; the runtime sees it and skips its C-side metal stub
// include so build/backend_metal.o owns METAL_BACKEND instead.
#include "../src/thvm.c"
#include "test.h"

int main(void) {
  TEST_BEGIN("metal-real/dual-tu-build-links");
  unsetenv("THVM_BACKEND");
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  thvm_free();

  TEST_BEGIN("metal-real/THVM_BACKEND-metal-selects-m-backend");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);
  CHECK_EQ(CURRENT_BACKEND->id, 2);
  // Now-real metal_buf_alloc returns a non-zero buf_id.
  u32 b = CURRENT_BACKEND->buf_alloc(64);
  CHECK(b != 0);
  CURRENT_BACKEND->buf_free(b);
  thvm_free();

  TEST_BEGIN("metal-real/init-shutdown-cycle-survives");
  // metal_init opens MTLDevice + MTLCommandQueue; metal_shutdown
  // nils the references.  Verify a second cycle re-opens cleanly.
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  thvm_free();
  thvm_init();
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);
  thvm_free();

  TEST_BEGIN("metal-real/buf-write-read-roundtrip");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  // Allocate a 16-element f32 buffer on Metal, write a known
  // sequence via shared-storage memcpy, read it back, compare.
  float src[16], dst[16];
  for (int i = 0; i < 16; i++) src[i] = (float)i * 1.5f;
  u32 bid = CURRENT_BACKEND->buf_alloc(sizeof(src));
  CHECK(bid != 0);
  CHECK_EQ(CURRENT_BACKEND->buf_write(bid, src, sizeof(src)), 0);
  CHECK_EQ(CURRENT_BACKEND->buf_read (bid, dst, sizeof(dst)), 0);
  for (int i = 0; i < 16; i++) CHECK(src[i] == dst[i]);
  CURRENT_BACKEND->buf_free(bid);
  thvm_free();

  TEST_BEGIN("metal-real/buf-refcount-shared-storage");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  u32 bid2 = CURRENT_BACKEND->buf_alloc(64);
  CHECK(bid2 != 0);
  CURRENT_BACKEND->buf_incref(bid2);
  CURRENT_BACKEND->buf_decref(bid2);
  // Refcount still 1, buffer should still be readable.
  char tmp[8];
  CHECK_EQ(CURRENT_BACKEND->buf_read(bid2, tmp, sizeof(tmp)), 0);
  CURRENT_BACKEND->buf_decref(bid2);  // drops to 0; frees.
  // Now invalid; read should fail.
  CHECK_EQ(CURRENT_BACKEND->buf_read(bid2, tmp, sizeof(tmp)), -1);
  thvm_free();

  unsetenv("THVM_BACKEND");
  TEST_REPORT();
}
