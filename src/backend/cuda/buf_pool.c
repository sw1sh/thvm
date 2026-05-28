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
    if (CUDA_BUFS[i].jit_pinned) continue;   // JIT capture holds this buf
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
    if (CUDA_BUFS[i].jit_pinned) continue;   // sticky JIT retain
    if (CUDA_BUFS[i].dptr == 0) continue;
    // Skip buffers already on the freelist (refcount==0): a global
    // reclaim (wm=1) re-scans prior realizes' freed slots; re-pushing
    // would double-list them -> two allocations alias one buffer.
    if (CUDA_BUFS[i].refcount == 0) continue;
    if (CUDA_BUFS[i].skip_freelist) {
      // Per-realize arena (see CudaBuf.skip_freelist).  Skip the
      // freelist park, real-free immediately so its 100-700MB don't
      // accumulate across steps.  Views into this arena still living
      // in [wm, CUDA_BUFS_NEXT) at iteration time are tied via
      // parent_buf_id; cuda_buf_free's parent-decref chain handles
      // the lifecycle uniformly whether we free arena first or last
      // (the parent-decref gate `refcount > 0` no-ops the double).
      cuda_buf_free((u32)i);
    } else if (CUDA_BUFS[i].owns_data) {
      cuda_buf_freelist_push((u32)i);
    } else {
      cuda_buf_free((u32)i);
    }
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

// STICKY pin: jit_capture sets this so the buf survives every
// subsequent realize's pool_rollback.  Cleared on jit_capture_drop.
// We do NOT bump refcount here -- the JIT already calls buf_incref
// separately (in jit_capture_retain_buf), and double-counting confuses
// the schedule's per-realize buffer planner.  Skip dead buffers: a buf
// freed by the time the JIT retain runs has dptr==0; pinning it would
// just hold a dead slot.
//
// Arena views: pin recursively into the parent arena.  Without this
// the view stays alive but its dptr points into the OLD arena's
// (cuMemFree'd) storage; the NEXT realize allocates a possibly-smaller
// arena at a different dptr, and the captured op's dispatch writes
// into the wrong / freed region -> CUDA_ERROR_ILLEGAL_ADDRESS.
fn void cuda_buf_jit_pin(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  if (CUDA_BUFS[buf_id].dptr == 0) return;
  CUDA_BUFS[buf_id].jit_pinned = 1;
  u32 parent = CUDA_BUFS[buf_id].parent_buf_id;
  if (parent != 0 && parent < CUDA_BUFS_NEXT) {
    cuda_buf_jit_pin(parent);
  }
}

fn void cuda_buf_jit_unpin(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CUDA_BUFS_NEXT) return;
  CUDA_BUFS[buf_id].jit_pinned = 0;
  u32 parent = CUDA_BUFS[buf_id].parent_buf_id;
  if (parent != 0 && parent < CUDA_BUFS_NEXT) {
    cuda_buf_jit_unpin(parent);
  }
}

fn void cuda_buf_clear_preserved(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CUDA_BUFS_NEXT; i++) CUDA_BUFS[i].preserved = 0;
}

// Cross-realize arena-view release: same role as
// backend/cpu/buf_pool.c::cpu_buf_clear_preserved_arena_views.  Arena
// views are by construction internal forward intermediates
// (consumer_count == 1, last_use > 0; arena_boundary_is_plannable in
// schedule/materialize.c).  The per-realize tracing GC may still mark
// some preserved (e.g. a view-tid reachable from the SINK Term); that
// preserve also marks the parent arena alive via
// tensor_mark_buf_preserved's parent_buf_id link, which would block
// pool_rollback from reclaiming the entire arena cohort.  Drop the
// preserve flag on both halves so the arena dies at end-of-realize.
fn void cuda_buf_clear_preserved_arena_views(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CUDA_BUFS_NEXT; i++) {
    u32 parent = CUDA_BUFS[i].parent_buf_id;
    if (parent == 0) continue;
    CUDA_BUFS[i].preserved = 0;
    if (parent < CUDA_BUFS_NEXT) CUDA_BUFS[parent].preserved = 0;
  }
}
