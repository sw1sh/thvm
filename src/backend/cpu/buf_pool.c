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
    if (CPU_BUFS[i].owns_data) {
      cpu_buf_freelist_push((u32)i);
    } else {
      cpu_buf_free((u32)i);
    }
  }
  // No CPU_BUFS_NEXT reset; preserved bufs would block it anyway.
}

fn void cpu_buf_mark_preserved(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].preserved = 1;
}

fn void cpu_buf_clear_preserved(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) CPU_BUFS[i].preserved = 0;
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
