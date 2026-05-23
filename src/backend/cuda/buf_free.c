// backend/cuda/buf_free.c - release a device buffer's storage
// unconditionally.  Callers normally go through cuda_buf_decref
// (refcount aware); direct free is for the shutdown path.

fn void cuda_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  CudaBuf *b = &CUDA_BUFS[buf_id];
  if (b->dptr != 0) {
    extern u64 CUDA_MEM_LIVE;
    if (CUDA_MEM_LIVE >= b->nbytes) CUDA_MEM_LIVE -= b->nbytes; else CUDA_MEM_LIVE = 0;
    cuMemFree(b->dptr);
  }
  b->dptr     = 0;
  b->nbytes   = 0;
  b->refcount = 0;
}
