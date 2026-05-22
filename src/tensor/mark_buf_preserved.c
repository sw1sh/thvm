// tensor/mark_buf_preserved.c - mark a tensor's backend storage as
// preserved for the current rollback boundary.

fn void tensor_mark_buf_preserved(u32 id) {
  if (id == 0 || id >= TENS_NEXT) return;
  TenDesc *d = &TENS[id];
  if (d->buf_id == 0 || d->backend == NULL) return;
  switch (d->backend->id) {
    case 1: cpu_buf_mark_preserved(d->buf_id); break;
    case 2: thvm_metal_buf_mark_preserved(d->buf_id); break;
#ifdef THVM_HAS_CUDA
    case 3: cuda_buf_mark_preserved(d->buf_id); break;
#endif
    default: break;
  }
}
