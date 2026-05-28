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
  // Async on CUDA_CUR_STREAM so this copy is captureable into a CUgraph
  // (cuMemcpyDtoD without a stream is a synchronous null-stream op which
  // cuStreamBeginCapture rejects).  On the default-stream path
  // (CUDA_CUR_STREAM==NULL) the semantics match the prior cuMemcpyDtoD:
  // the next default-stream op waits on this copy.
  CUresult r = cuMemcpyDtoDAsync(dst->dptr, src->dptr, (size_t)nbytes,
                                 CUDA_CUR_STREAM);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuMemcpyDtoDAsync", r);
    return -1;
  }
  return 0;
}
