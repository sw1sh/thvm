// backend/cpu/buf_read.c - copy a buffer into a host-side destination.

fn int cpu_buf_read(u32 buf_id, void *dst, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return -1;
  if (nbytes > CPU_BUFS[buf_id].nbytes)       return -1;
  memcpy(dst, CPU_BUFS[buf_id].data, nbytes);
  return 0;
}
