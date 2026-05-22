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

// Preserve-aware rollback (mirrors cpu_buf_pool_rollback_with_preserve):
// bufs flagged `preserved` -- the result-chain bufs marked just before
// rollback -- survive; every other buffer allocated since the watermark
// is pushed to the freelist so the next same-size cuda_buf_alloc recycles
// its device storage instead of cuMemAlloc'ing anew.  This is what keeps
// a no-JIT CUDA training loop's device memory flat: without it the
// per-realize transient buffers leak and a 16GB GPU saturates in a few
// steps.  CUDA_BUFS_NEXT is NOT reset (preserved bufs may sit high), but
// recycled allocs pop existing slots so the table stays bounded too.
fn void cuda_buf_pool_rollback_with_preserve(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CUDA_BUFS_NEXT) return;
  for (u64 i = wm; i < CUDA_BUFS_NEXT; i++) {
    if (CUDA_BUFS[i].preserved) continue;
    if (CUDA_BUFS[i].dptr == 0) continue;
    // Skip buffers already on the freelist (refcount==0): a global
    // reclaim (wm=1) re-scans prior realizes' freed slots; re-pushing
    // would double-list them -> two allocations alias one buffer.
    if (CUDA_BUFS[i].refcount == 0) continue;
    cuda_buf_freelist_push((u32)i);
  }
}

// Cross-step global reclaim: plain cuMemFree (NOT freelist-recycle)
// every live, non-preserved device buffer.  Recycling a globally-
// reclaimed buffer risks aliasing it into a new allocation while a stale
// TenDesc still names the slot; plain free returns the device storage.
// Used by py_reclaim between training steps.
fn void cuda_buf_pool_free_unpreserved(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CUDA_BUFS_NEXT) return;
  for (u64 i = wm; i < CUDA_BUFS_NEXT; i++) {
    if (CUDA_BUFS[i].preserved) continue;
    if (CUDA_BUFS[i].refcount == 0) continue;
    if (CUDA_BUFS[i].dptr == 0) continue;
    cuda_buf_free((u32)i);
  }
}

fn void cuda_buf_mark_preserved(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  CUDA_BUFS[buf_id].preserved = 1;
}

fn void cuda_buf_clear_preserved(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CUDA_BUFS_NEXT; i++) CUDA_BUFS[i].preserved = 0;
}
