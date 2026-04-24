// backend/cpu/buf_incref.c - bump a buffer's refcount (view aliasing).
//
// Called by tensor_view_of when a new TenDesc shares another's buf_id.

fn void cpu_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].refcount++;
}
