// backend/cpu/buf_decref.c - decrement a buffer's refcount; free at zero.

fn void cpu_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  if (CPU_BUFS[buf_id].refcount == 0) return;
  // An arena view's parent ref is dropped in cpu_buf_free, the one place
  // every path that retires a view goes through.
  if (--CPU_BUFS[buf_id].refcount == 0) cpu_buf_free(buf_id);
}
