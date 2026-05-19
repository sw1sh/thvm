// backend/cuda/buf_read.c - copy a device buffer into host memory.
//
// Counterpart of cuda_buf_write: a cuMemcpyDtoH download where the CPU
// backend's cpu_buf_read is a plain memcpy.

fn int cuda_buf_read(u32 buf_id, void *dst, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return -1;
  CudaBuf *b = &CUDA_BUFS[buf_id];
  if (b->dptr == 0) return -1;
  if (nbytes > b->nbytes) return -1;
  CUresult r = cuMemcpyDtoH(dst, b->dptr, (size_t)nbytes);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuMemcpyDtoH", r);
    return -1;
  }
  return 0;
}
