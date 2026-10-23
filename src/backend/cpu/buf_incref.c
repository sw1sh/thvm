// backend/cpu/buf_incref.c - bump a buffer's refcount (view aliasing).

fn void cpu_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].refcount++;
  // An arena view does not bump its parent: the parent counts live view
  // CpuBufs, each released once by cpu_buf_free, not their aliases.
}
