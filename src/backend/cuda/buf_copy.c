// backend/cuda/buf_copy.c - device-to-device buffer copy.
//
// cuMemcpyDtoD where the CPU backend's cpu_buf_copy is a memmove.

fn int cuda_buf_copy(u32 dst_buf_id, u32 src_buf_id, u64 nbytes) {
  if (dst_buf_id == 0 || dst_buf_id >= CUDA_BUFS_NEXT) return -1;
  if (src_buf_id == 0 || src_buf_id >= CUDA_BUFS_NEXT) return -1;
  CudaBuf *dst = &CUDA_BUFS[dst_buf_id];
  CudaBuf *src = &CUDA_BUFS[src_buf_id];
  if (dst->dptr == 0 || src->dptr == 0) return -1;
  if (nbytes > dst->nbytes) nbytes = dst->nbytes;
  if (nbytes > src->nbytes) nbytes = src->nbytes;
  CUresult r = cuMemcpyDtoD(dst->dptr, src->dptr, (size_t)nbytes);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuMemcpyDtoD", r);
    return -1;
  }
  return 0;
}
