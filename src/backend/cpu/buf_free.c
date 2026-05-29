// backend/cpu/buf_free.c - release a buffer's storage unconditionally.
//
// Callers should almost always go through cpu_buf_decref (refcount
// aware).  Direct free is kept for the shutdown path.

fn void cpu_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CpuBuf *b = &CPU_BUFS[buf_id];
  // Snapshot parent BEFORE we zero the slot.  Arena views (external
  // bufs pointing into a parent arena CpuBuf) must drop their parent
  // ref no matter which code path frees them -- pool_rollback's
  // direct cpu_buf_free path is one such case (vs cpu_buf_decref /
  // cpu_buf_freelist_push which already snapshot+decref themselves).
  // Without this, the parent arena's refcount stays inflated by the
  // freed view's contribution and the arena leaks until session end.
  u32 parent = b->parent_buf_id;
  if (b->owns_data) {
    extern u64 CPU_MEM_LIVE;
    if (CPU_MEM_LIVE >= b->nbytes) CPU_MEM_LIVE -= b->nbytes; else CPU_MEM_LIVE = 0;
    free(b->data);
  } else if (b->on_release) {
    b->on_release(b->handle);
  }
  b->data       = NULL;
  b->nbytes     = 0;
  b->refcount   = 0;
  b->handle     = NULL;
  b->on_release = NULL;
  b->owns_data  = 0;
  b->preserved  = 0;
  b->freeable   = 0;
  b->skip_freelist = 0;
  b->jit_pinned = 0;
  b->parent_buf_id = 0;
  if (parent != 0 && parent < CPU_BUFS_NEXT
      && CPU_BUFS[parent].refcount > 0) {
    if (--CPU_BUFS[parent].refcount == 0) cpu_buf_free(parent);
  }
}
