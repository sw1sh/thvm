// backend/cpu/buf_copy.c - backend-native buffer copy.

fn int cpu_buf_copy(u32 dst_buf_id, u32 src_buf_id, u64 nbytes) {
  if (dst_buf_id == 0 || dst_buf_id >= CPU_BUFS_NEXT) return -1;
  if (src_buf_id == 0 || src_buf_id >= CPU_BUFS_NEXT) return -1;
  CpuBuf *dst = &CPU_BUFS[dst_buf_id];
  CpuBuf *src = &CPU_BUFS[src_buf_id];
  if (dst->data == NULL || src->data == NULL) return -1;
  if (nbytes > dst->nbytes) nbytes = dst->nbytes;
  if (nbytes > src->nbytes) nbytes = src->nbytes;
  memmove(dst->data, src->data, (size_t)nbytes);
  return 0;
}
