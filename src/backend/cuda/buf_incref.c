// backend/cuda/buf_incref.c - bump a buffer's refcount (view aliasing).

fn void cuda_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  CUDA_BUFS[buf_id].refcount++;
}
