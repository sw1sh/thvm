// backend/cpu/init.c - lifecycle for the CPU backend + its buffer table.
//
// CPU_BUFS[] is a parallel table to TENS[]: each TenDesc.buf_id indexes
// into it for the actual bytes.  Multiple TenDescs can share the same
// buf_id (view aliasing); CPU_BUFS[id].refcount controls the storage
// lifetime separately from the TenDesc.refcount.
//
// A buffer can be "owned" (we malloc'd it; free on release) or
// "external" (the bytes belong to someone else; we call a per-buffer
// on_release callback).  External buffers let the WL bridge construct
// a tensor over a NumericArray's bytes without copying, holding the
// NumericArray alive for the tensor's lifetime.
//
// CpuBuf typedef + the CPU_BUFS / CPU_BUFS_NEXT / CPU_FREELIST /
// CPU_FREELIST_LEN identifiers now live inside TContext (see thvm.h);
// the macros there resolve them through CURRENT_CTX so the rest of
// the CPU backend keeps compiling unchanged.  Allocation is done by
// cpu_init below (called from thvm_init / thvm_context_create after
// the ctx arrays are wired up).

fn int cpu_init(void) {
  // thvm_init / thvm_context_create allocate CPU_BUFS up-front (so
  // cpu_buf_freelist's NULL guards stay accurate even before the
  // first cpu_buf_alloc call).  Just sanity-check it's there.
  return CPU_BUFS == NULL ? -1 : 0;
}

// Release every live buffer's payload and rewind the descriptor +
// freelist cursors back to empty.  Shared by cpu_shutdown (teardown,
// where the CPU_BUFS array is freed right after) and thvm_reset (per-
// frame reclaim, where the array stays mapped for the next frame): in
// both cases the owning TenDescs are about to disappear, so any
// surviving payload would leak.  CPU_BUFS itself is freed by
// thvm_free / thvm_context_destroy, not here.
fn void cpu_buf_free_all(void) {
  extern u64 CPU_MEM_LIVE;   // defined in buf_alloc.c (included after this)
  if (CPU_BUFS == NULL) return;
  for (u64 i = 1; i < CPU_BUFS_NEXT; i++) {
    CpuBuf *b = &CPU_BUFS[i];
    if (b->refcount == 0) continue;
    if (b->owns_data) {
      if (b->data) free(b->data);
    } else if (b->on_release) {
      b->on_release(b->handle);
    }
    // Zero the slot so a re-issued descriptor starts clean and a stale
    // arena-view parent ref can't drive a double free on the next pass.
    *b = (CpuBuf){0};
  }
  CPU_MEM_LIVE     = 0;
  CPU_BUFS_NEXT    = 1;
  CPU_FREELIST_LEN = 0;
}

fn void cpu_shutdown(void) {
  cpu_buf_free_all();
}
