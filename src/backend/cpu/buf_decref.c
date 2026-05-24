// backend/cpu/buf_decref.c - decrement a buffer's refcount; free at zero.
//
// The mirror of cpu_buf_incref.  Called from tensor_release when a
// TenDesc's own refcount hits zero.

fn void cpu_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  if (CPU_BUFS[buf_id].refcount == 0) return;
  // Arena view parent decref happens inside cpu_buf_free below, so
  // every path that retires a view (this decref, pool_rollback's
  // direct free) decrements the parent uniformly.
  if (--CPU_BUFS[buf_id].refcount == 0) cpu_buf_free(buf_id);
}
