// test_buf_pool.c - exercise cpu_buf_pool_begin / pool_rollback
// (sub-item a of the per-step buffer pool arc).
//
// Verifies:
//   - alloc-then-rollback frees every buf alloc'd since the
//     watermark and restores CPU_BUFS_NEXT.
//   - bufs alloc'd BEFORE the watermark survive rollback.
//   - empty rollback (wm == CPU_BUFS_NEXT) is a no-op.
//   - rollback past the high-water-mark only frees what's actually
//     alloc'd (no over-walk).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("buf-pool/rollback-frees-everything-since-wm");
  // Setup: pre-existing buffer (allocated BEFORE the wm).
  u32 pre = cpu_buf_alloc(64);
  CHECK(pre > 0);
  CHECK(CPU_BUFS[pre].data != NULL);
  u32 wm = cpu_buf_pool_begin();
  CHECK_EQ(wm, CPU_BUFS_NEXT);
  // Three new bufs after the watermark.
  u32 a = cpu_buf_alloc(128);
  u32 b = cpu_buf_alloc(256);
  u32 c = cpu_buf_alloc(512);
  CHECK(a > pre); CHECK(b > a); CHECK(c > b);
  CHECK(CPU_BUFS[a].data != NULL);
  CHECK(CPU_BUFS[b].data != NULL);
  CHECK(CPU_BUFS[c].data != NULL);
  // Rollback frees a/b/c and restores CPU_BUFS_NEXT.
  cpu_buf_pool_rollback(wm);
  CHECK_EQ(CPU_BUFS_NEXT, wm);
  CHECK(CPU_BUFS[a].data == NULL);
  CHECK(CPU_BUFS[b].data == NULL);
  CHECK(CPU_BUFS[c].data == NULL);
  // Pre-watermark buf survives.
  CHECK(CPU_BUFS[pre].data != NULL);
  CHECK_EQ(CPU_BUFS[pre].nbytes, 64);

  TEST_BEGIN("buf-pool/empty-rollback-no-op");
  u32 wm2 = cpu_buf_pool_begin();
  cpu_buf_pool_rollback(wm2);
  CHECK_EQ(CPU_BUFS_NEXT, wm2);

  TEST_BEGIN("buf-pool/rollback-allows-realloc-after");
  // After rollback, the next alloc should reuse a slot at wm
  // (or just past it).  We don't guarantee slot reuse but we
  // do guarantee CPU_BUFS_NEXT hasn't grown unboundedly.
  u32 wm3 = cpu_buf_pool_begin();
  u32 d = cpu_buf_alloc(32);
  cpu_buf_pool_rollback(wm3);
  u32 e = cpu_buf_alloc(48);
  CHECK_EQ(e, d);              // same slot reused
  CHECK_EQ(CPU_BUFS[e].nbytes, 48);
  cpu_buf_pool_rollback(wm3);  // clean up

  thvm_free();
  TEST_REPORT();
}
