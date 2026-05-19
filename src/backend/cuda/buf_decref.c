// backend/cuda/buf_decref.c - decrement a buffer's refcount; free at
// zero.  Mirror of cuda_buf_incref; called from tensor_release when a
// TenDesc's own refcount hits zero.

fn void cuda_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  if (CUDA_BUFS[buf_id].refcount == 0) return;
  if (--CUDA_BUFS[buf_id].refcount == 0) cuda_buf_free(buf_id);
}
