// backend/cpu/buf_pool.c - high-water-mark scope for cpu buffer
// allocations.  Sub-item (a) of the per-step buffer pool arc.
//
// Use case: bracket a TRealize (or any other materialize+wnf
// session) with `pool_begin()` + `pool_rollback(wm)` to free
// every buf allocated in between.  The caller is responsible
// for ensuring nothing else holds the rolled-back bufs alive
// (typically via a preserve-walk over the result's producer
// chain -- handled in sub-item b).
//
// Bypasses refcount: this is the "I know all references are
// dead" escape hatch.  Direct buf_decref / buf_free still work
// for refcount-driven release.

fn u32 cpu_buf_pool_begin(void) {
  // Snapshot the watermark.  Subsequent allocations push
  // CPU_BUFS_NEXT past this; rollback frees everything in
  // [wm, CPU_BUFS_NEXT) and restores CPU_BUFS_NEXT = wm.
  return (u32)CPU_BUFS_NEXT;
}

fn void cpu_buf_pool_rollback(u32 wm) {
  if (wm < 1) wm = 1;        // slot 0 is reserved
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].data || CPU_BUFS[i].handle) {
      cpu_buf_free((u32)i);
    }
  }
  CPU_BUFS_NEXT = wm;
}

// Variant that respects the per-CpuBuf `preserved` flag: bufs
// marked preserved (typically by walking the result-tensor's
// producer chain before rollback) are left alone; everything
// else in [wm, CPU_BUFS_NEXT) is reclaimed.  CPU_BUFS_NEXT does
// NOT reset to wm because preserved bufs may sit at high
// indices; the table accumulates sparse "long-term" slots over
// many TRealize calls (CPU_BUFS_CAP = 1<<20 has plenty of
// headroom).
//
// bm4b: rollback now distinguishes owning vs external bufs.
//   - owning (calloc'd by us): push to the freelist so the
//     storage gets recycled by the next cpu_buf_alloc with
//     matching nbytes.
//   - external (cpu_buf_alloc_external; e.g. WL-uploaded
//     NumericArray slices): cpu_buf_free, which invokes the
//     on_release callback so the WL side can disown the handle.
// The preserve set captures every TenDesc reachable from the
// result chain, so any forward intermediate the next TRealize
// might still need (lazy TGrad backward, view-only alias chains)
// stays alive; only bufs that nothing reaches go to the
// freelist.
fn void cpu_buf_pool_rollback_with_preserve(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].preserved) continue;
    if (CPU_BUFS[i].data == NULL && CPU_BUFS[i].handle == NULL) continue;
    // Skip buffers already released to the freelist (refcount==0): a
    // global reclaim (wm=1) re-scans prior realizes' freed slots, and
    // re-pushing would double-list them -> two allocations alias one
    // buffer.  Fresh scratch in a watermark scope is refcount>=1, so
    // this gate doesn't change the per-realize path.
    if (CPU_BUFS[i].refcount == 0) continue;
    if (CPU_BUFS[i].skip_freelist) {
      // Per-realize arena (see CpuBuf.skip_freelist).  Skip the
      // freelist park, real-free immediately so its 100-700MB don't
      // accumulate across steps.  Views into this arena still living
      // in [wm, CPU_BUFS_NEXT) at iteration time are tied via
      // parent_buf_id; cpu_buf_free's parent-decref chain handles
      // the lifecycle uniformly whether we free arena first or
      // last (the parent-decref gate `refcount > 0` no-ops the
      // double-decref).
      cpu_buf_free((u32)i);
    } else if (CPU_BUFS[i].owns_data) {
      cpu_buf_freelist_push((u32)i);
    } else {
      cpu_buf_free((u32)i);
    }
  }
  // No CPU_BUFS_NEXT reset; preserved bufs would block it anyway.
}

// Cross-step global reclaim: plain-free (NOT freelist-recycle) every
// live, non-preserved buffer.  Recycling a globally-reclaimed buffer
// risks handing it to a new allocation while a stale TenDesc still names
// the slot; plain free returns the storage and leaves the slot
// data==NULL so the next try_pop skips it and allocates fresh.  Used by
// py_reclaim between training steps.  refcount==0 (already freelisted)
// is skipped so we never double-free.
fn void cpu_buf_pool_free_unpreserved(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].preserved) continue;
    if (CPU_BUFS[i].refcount == 0) continue;
    if (CPU_BUFS[i].data == NULL && CPU_BUFS[i].handle == NULL) continue;
    cpu_buf_free((u32)i);
  }
}

fn void cpu_buf_mark_preserved(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].preserved = 1;
}

fn void cpu_buf_clear_preserved(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) CPU_BUFS[i].preserved = 0;
}

// Cross-realize arena-view release: arena views are by construction
// "internal forward intermediates" (consumer_count==1, last_use>0;
// see arena_boundary_is_plannable in schedule/materialize.c).  The
// per-realize tracing-GC walk may still mark some of them preserved
// (e.g., a view-tid that lands as a TAG_TEN reachable from the SINK
// result Term).  Without this routine, marking the view preserved
// also marks its backing arena preserved (CpuBuf.parent_buf_id link
// in tensor_mark_buf_preserved.c) -- and the arena stays alive
// across pool_rollback, accumulating ~600MB per training step.
//
// Tinygrad's invariant (tinygrad/schedule/memory.py:13-16,
// memory_plan_rewrite line 29): plannable buffers are EXCLUDED from
// `held_bufs` -- i.e. they don't survive into the next schedule.  In
// thvm the equivalent is to actively drop the preserve flag on
// arena views (and their parent arenas) at end-of-realize so
// pool_rollback releases the entire arena cohort.  Sink-tensor
// outputs are kept distinct via the last_use==0 gate in
// arena_boundary_is_plannable, which excludes them from the arena
// in the first place -- so the sink's standalone CpuBuf still keeps
// its preserve flag through this clear.
fn void cpu_buf_clear_preserved_arena_views(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    u32 parent = CPU_BUFS[i].parent_buf_id;
    if (parent == 0) continue;
    CPU_BUFS[i].preserved = 0;
    if (parent < CPU_BUFS_NEXT) CPU_BUFS[parent].preserved = 0;
  }
}

// Refcount-driven-free hooks (sub-item b of the refcount-driven
// free arc).  cpu_buf_mark_freeable is called from kernel_fire_by_id
// once a producer kernel's consumer_count hits zero -- i.e., every
// consumer that needed to read the producer's output buf has done
// so.  cpu_buf_clear_freeable resets the flag (companion to
// cpu_buf_clear_preserved).  The "free what hit zero" rollback
// variant lives in sub-item c (the thvm_realize integration).
fn void cpu_buf_mark_freeable(u32 buf_id) {
  if (CPU_BUFS == NULL) return;  // Metal-active: no CPU bufs to mark.
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].freeable = 1;
}

fn void cpu_buf_clear_freeable(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) CPU_BUFS[i].freeable = 0;
}
