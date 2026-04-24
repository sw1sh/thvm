// backend/cpu/buf_read.c - copy a buffer into a host-side destination.
//
// For CPU the "buffer" already lives in host memory, so this is a
// memcpy.  Kept behind the Backend vtable so Metal can implement it
// via an MTLBlitCommandEncoder later without touching callers.

fn int cpu_buf_read(u32 buf_id, void *dst, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return -1;
  if (nbytes > CPU_BUFS[buf_id].nbytes)       return -1;
  memcpy(dst, CPU_BUFS[buf_id].data, nbytes);
  return 0;
}
