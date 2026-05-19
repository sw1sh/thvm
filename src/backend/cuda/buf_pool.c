// backend/cuda/buf_pool.c - high-water-mark scope for CUDA buffer
// allocations.  Mirrors backend/cpu/buf_pool.c.
//
// pool_begin() snapshots CUDA_BUFS_NEXT; pool_rollback(wm) frees every
// buffer allocated since the snapshot.  Bypasses refcount -- the "I
// know all references are dead" escape hatch the per-realize memory
// planner uses to bound a materialize session.

fn u32 cuda_buf_pool_begin(void) {
  return (u32)CUDA_BUFS_NEXT;
}

fn void cuda_buf_pool_rollback(u32 wm) {
  if (wm < 1) wm = 1;                 // slot 0 reserved
  if (wm > CUDA_BUFS_NEXT) return;
  for (u64 i = wm; i < CUDA_BUFS_NEXT; i++) {
    if (CUDA_BUFS[i].dptr != 0) cuda_buf_free((u32)i);
  }
  CUDA_BUFS_NEXT = wm;
}
