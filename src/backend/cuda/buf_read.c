// backend/cuda/buf_read.c - copy a device buffer into host memory.
//
// Counterpart of cuda_buf_write: a cuMemcpyDtoH download where the CPU
// backend's cpu_buf_read is a plain memcpy.

fn int cuda_buf_read(u32 buf_id, void *dst, u64 nbytes) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return -1;
  CudaBuf *b = &CUDA_BUFS[buf_id];
  if (b->dptr == 0) return -1;
  if (nbytes > b->nbytes) return -1;
  // If JIT capture stream has been used, drain it first so any
  // captured work writing to this buffer (cuGraphLaunch'd async) is
  // visible before the host-blocking DtoH copy.  Without this, the
  // graph replay's writes race the readback.  cuStreamSynchronize on
  // the capture stream is the only sync point in the graph-replay
  // pipeline; the per-op cuLaunchKernel path stays serialized on the
  // default stream and doesn't need this.
  if (CUDA_CAPTURE_STREAM != NULL) {
    cuStreamSynchronize(CUDA_CAPTURE_STREAM);
  }
  CUresult r = cuMemcpyDtoH(dst, b->dptr, (size_t)nbytes);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuMemcpyDtoH", r);
    return -1;
  }
  return 0;
}
