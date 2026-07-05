// backend/cuda/buf_free.c - release a device buffer's storage
// unconditionally.  Callers normally go through cuda_buf_decref
// (refcount aware); direct free is for the shutdown path.

fn void cuda_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  CudaBuf *b = &CUDA_BUFS[buf_id];
  // Snapshot parent BEFORE we zero the slot.  Arena views (non-owning
  // bufs pointing into a parent arena CudaBuf) must drop their parent
  // ref no matter which code path frees them -- pool_rollback's direct
  // cuda_buf_free path is one such case (cuda_buf_decref already routes
  // through here, so the parent decref happens uniformly).  Without
  // this, the parent arena's refcount stays inflated by the freed
  // view's contribution and the arena leaks until session end.
  u32 parent = b->parent_buf_id;
  if (b->dptr != 0 && b->owns_data) {
    extern u64 CUDA_MEM_LIVE;
    if (CUDA_MEM_LIVE >= b->nbytes) CUDA_MEM_LIVE -= b->nbytes; else CUDA_MEM_LIVE = 0;
    // STREAM-ORDERED free: a plain cuMemFree can release storage while
    // launches still queued on the (legacy) stream read or write it -- the
    // async CUDA_ERROR_ILLEGAL_ADDRESS that killed the Krea 256px gen once
    // the slow 64-input fusion kernels deepened the launch pipeline (the
    // per-launch-sync THVM_CUDA_SYNC=1 run completes cleanly, proving the
    // race).  Metal orders frees behind the GPU via its deferred-decref +
    // drain machinery; the CUDA analog is cuMemFreeAsync on the legacy
    // stream (0), which queues the release AFTER all prior default-stream
    // work touching the allocation.  Fallback to the synchronous free on
    // drivers without stream-ordered memory (pre-11.2).
    if (cuMemFreeAsync(b->dptr, 0) != CUDA_SUCCESS) {
      cuMemFree(b->dptr);
    }
  }
  b->dptr          = 0;
  b->nbytes        = 0;
  b->refcount      = 0;
  b->preserved     = 0;
  b->owns_data     = 0;
  b->skip_freelist = 0;
  b->parent_buf_id = 0;
  if (parent != 0 && parent < CUDA_BUFS_NEXT
      && CUDA_BUFS[parent].refcount > 0) {
    if (--CUDA_BUFS[parent].refcount == 0) cuda_buf_free(parent);
  }
}
