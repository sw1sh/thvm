// backend/cuda/buf_write.c - copy host bytes into a device buffer.
//
// The CPU backend's cpu_buf_write is a memcpy because its "buffers"
// already live in host memory; the CUDA buffer is device memory, so
// this is a cuMemcpyHtoD upload.

fn int cuda_buf_write(u32 buf_id, const void *src, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return -1;
  CudaBuf *b = &CUDA_BUFS[buf_id];
  if (b->dptr == 0) return -1;
  if (nbytes > b->nbytes) return -1;
  CUresult r = cuMemcpyHtoD(b->dptr, src, (size_t)nbytes);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuMemcpyHtoD", r);
    return -1;
  }
  return 0;
}
