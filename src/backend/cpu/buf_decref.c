// backend/cpu/buf_decref.c - decrement a buffer's refcount; free at zero.
//
// The mirror of cpu_buf_incref.  Called from tensor_release when a
// TenDesc's own refcount hits zero.

fn void cpu_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  if (CPU_BUFS[buf_id].refcount == 0) return;
  // Snapshot parent before we possibly free this cell -- after
  // cpu_buf_free, CPU_BUFS[buf_id] is reset to all-zero.
  u32 parent = CPU_BUFS[buf_id].parent_buf_id;
  if (--CPU_BUFS[buf_id].refcount == 0) cpu_buf_free(buf_id);
  if (parent != 0 && parent < CPU_BUFS_NEXT) {
    if (CPU_BUFS[parent].refcount > 0
        && --CPU_BUFS[parent].refcount == 0) {
      cpu_buf_free(parent);
    }
  }
}
