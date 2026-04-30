// test_slot_reuse.c - bm4b: verify the rollback walk pushes
// owning, non-preserved bufs to the freelist so the next
// thvm_realize's allocations recycle them.
//
// Strategy: build a small UOP that allocates intermediate bufs,
// realize it twice, and verify CPU_BUFS_NEXT doesn't grow on
// the second call (because the freelist supplied the slots).

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("slot-reuse/freelist-push-on-rollback-clears-refcount");
  // Direct unit-test on the helper: alloc, push, observe refcount=0.
  u32 a = cpu_buf_alloc(64);
  CHECK(a > 0);
  CHECK_EQ(CPU_BUFS[a].refcount, 1);
  cpu_buf_freelist_push(a);
  CHECK_EQ(CPU_BUFS[a].refcount, 0);   // bm4b: push drops refcount

  TEST_BEGIN("slot-reuse/realloc-after-push-pops-same-slot");
  // Same as the bm4a check but exercising the new push path.
  u32 b = cpu_buf_alloc(64);
  CHECK_EQ(b, a);                       // recycled
  CHECK_EQ(CPU_BUFS[b].refcount, 1);    // pop reset

  TEST_BEGIN("slot-reuse/rollback-pushes-non-preserved-owning-bufs");
  // Drop a fresh CpuBuf into [wm, NEXT) and DON'T mark it
  // preserved.  cpu_buf_pool_rollback_with_preserve should push
  // it to the freelist (refcount drops to 0); a subsequent
  // cpu_buf_alloc with matching nbytes pops it back out.
  thvm_free();
  thvm_init();
  u32 wm = cpu_buf_pool_begin();
  u32 c1 = cpu_buf_alloc(48);     // owning, not preserved
  u32 c2 = cpu_buf_alloc(48);     // owning, not preserved
  CHECK(c1 > 0); CHECK(c2 > 0);
  CHECK_EQ(CPU_BUFS[c1].refcount, 1);
  CHECK_EQ(CPU_BUFS[c2].refcount, 1);
  cpu_buf_pool_rollback_with_preserve(wm);
  // Both owning + not preserved -> pushed -> refcount 0.
  CHECK_EQ(CPU_BUFS[c1].refcount, 0);
  CHECK_EQ(CPU_BUFS[c2].refcount, 0);
  // Next 48-byte alloc must reuse one of them.
  u32 c3 = cpu_buf_alloc(48);
  CHECK(c3 == c1 || c3 == c2);
  CHECK_EQ(CPU_BUFS[c3].refcount, 1);

  TEST_BEGIN("slot-reuse/rollback-skips-preserved-owning-bufs");
  // A preserved buf survives rollback unchanged.
  thvm_free();
  thvm_init();
  u32 wm2 = cpu_buf_pool_begin();
  u32 keep = cpu_buf_alloc(64);
  cpu_buf_mark_preserved(keep);
  cpu_buf_pool_rollback_with_preserve(wm2);
  CHECK_EQ(CPU_BUFS[keep].refcount, 1);
  CHECK_EQ(CPU_BUFS[keep].preserved, 1);
  // 64-byte alloc should NOT reuse `keep` (it's preserved, not freelist'd).
  u32 fresh = cpu_buf_alloc(64);
  CHECK(fresh != keep);

  TEST_BEGIN("slot-reuse/rollback-frees-non-owning-bufs");
  // External (non-owning) bufs hit the cpu_buf_free path on
  // rollback, NOT the freelist (the storage isn't ours to recycle).
  thvm_free();
  thvm_init();
  static u8 borrowed[16] = {0};
  u32 wm3 = cpu_buf_pool_begin();
  u32 ext = cpu_buf_alloc_external(borrowed, 16, NULL, NULL);
  CHECK_EQ(CPU_BUFS[ext].owns_data, 0);
  cpu_buf_pool_rollback_with_preserve(wm3);
  // cpu_buf_free zeroes the slot (data NULL, refcount 0).
  CHECK_EQ(CPU_BUFS[ext].refcount, 0);
  CHECK_EQ(CPU_BUFS[ext].data, NULL);

  thvm_free();
  TEST_REPORT();
}
