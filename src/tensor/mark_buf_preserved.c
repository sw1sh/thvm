// tensor/mark_buf_preserved.c - mark a tensor's backend storage as
// preserved for the current rollback boundary.

fn void tensor_mark_buf_preserved(u32 id) {
  if (id == 0 || id >= TENS_NEXT) return;
  TenDesc *d = &TENS[id];
  if (d->buf_id == 0 || d->backend == NULL) return;
  switch (d->backend->id) {
    case 1: {
      cpu_buf_mark_preserved(d->buf_id);
      // Arena view: also keep the backing arena CpuBuf alive --
      // tracing GC reaches d->buf_id via TenDesc, but the arena
      // CpuBuf has no Term naming it so this is the only path that
      // can mark it preserved.
      if (CPU_BUFS != NULL && d->buf_id < CPU_BUFS_NEXT) {
        u32 parent = CPU_BUFS[d->buf_id].parent_buf_id;
        if (parent != 0) cpu_buf_mark_preserved(parent);
      }
      break;
    }
    case 2: thvm_metal_buf_mark_preserved(d->buf_id); break;
#ifdef THVM_HAS_CUDA
    case 3: {
      cuda_buf_mark_preserved(d->buf_id);
      // Arena view: tracing GC reaches the view via TenDesc but the
      // parent arena CudaBuf has no Term naming it.  Mark parent too
      // so pool_rollback doesn't free arena bytes while a view's dptr
      // still references them (mirror of the CPU branch above).
      if (d->buf_id < CUDA_BUFS_NEXT) {
        u32 parent = CUDA_BUFS[d->buf_id].parent_buf_id;
        if (parent != 0) cuda_buf_mark_preserved(parent);
      }
      break;
    }
#endif
    default: break;
  }
}
