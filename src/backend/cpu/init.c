// backend/cpu/init.c - CPU backend lifecycle.  A CPU_BUFS slot (a TContext
// table, thvm.h) owns its bytes or wraps a caller's, such as a WL NumericArray,
// which it hands back through on_release.

fn int cpu_init(void) {
  // The context maps CPU_BUFS before any backend init.
  return CPU_BUFS == NULL ? -1 : 0;
}

// Release every live buffer's payload and rewind the slot and freelist
// cursors, for cpu_shutdown and thvm_reset.  CPU_BUFS itself stays mapped.
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
    // A zeroed slot keeps a stale arena-view parent ref from a double free.
    *b = (CpuBuf){0};
  }
  CPU_MEM_LIVE     = 0;
  CPU_BUFS_NEXT    = 1;
  CPU_FREELIST_LEN = 0;
}

fn void cpu_shutdown(void) {
  cpu_buf_free_all();
}
