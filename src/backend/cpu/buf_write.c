// backend/cpu/buf_write.c - copy a host-side source into a buffer.

fn int cpu_buf_write(u32 buf_id, const void *src, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return -1;
  if (nbytes > CPU_BUFS[buf_id].nbytes)       return -1;
  memcpy(CPU_BUFS[buf_id].data, src, nbytes);
  return 0;
}

// Write nbytes host bytes at byte_off (the KV-cache append's row write).
fn int cpu_buf_write_at(u32 buf_id, u64 byte_off, const void *src, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT)          return -1;
  if (byte_off + nbytes > CPU_BUFS[buf_id].nbytes)     return -1;
  memcpy((char *)CPU_BUFS[buf_id].data + byte_off, src, nbytes);
  return 0;
}
