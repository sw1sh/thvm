// backend/cpu/buf_free.c - release a buffer's storage unconditionally.
//
// Callers should almost always go through cpu_buf_decref (refcount
// aware).  Direct free is kept for the shutdown path.

fn void cpu_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  free(CPU_BUFS[buf_id].data);
  CPU_BUFS[buf_id].data     = NULL;
  CPU_BUFS[buf_id].nbytes   = 0;
  CPU_BUFS[buf_id].refcount = 0;
}
