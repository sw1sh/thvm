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
// else in [wm, CPU_BUFS_NEXT) is freed.  CPU_BUFS_NEXT does NOT
// reset to wm because preserved bufs may sit at high indices;
// the table accumulates sparse "long-term" slots over many
// TRealize calls (CPU_BUFS_CAP = 1<<20 has plenty of headroom).
fn void cpu_buf_pool_rollback_with_preserve(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].preserved) continue;
    if (CPU_BUFS[i].data || CPU_BUFS[i].handle) {
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
