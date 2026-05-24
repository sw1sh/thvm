// backend/cpu/buf_incref.c - bump a buffer's refcount (view aliasing).
//
// Called by tensor_view_of when a new TenDesc shares another's buf_id.

fn void cpu_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].refcount++;
  // Note: arena views (parent_buf_id != 0) deliberately do NOT bump
  // the parent here.  parent.refcount tracks the number of LIVE view
  // CpuBufs, not the sum of their internal refcounts -- TenDesc
  // aliasing of the same view (which is what triggers this incref)
  // shares one view CpuBuf and therefore one parent reservation.
  // The single parent decref happens in cpu_buf_free when the view
  // CpuBuf itself is finally freed.
}
