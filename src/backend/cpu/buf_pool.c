// backend/cpu/buf_pool.c - watermark scopes over CPU buffer slots.  A rollback
// frees or parks every slot allocated since cpu_buf_pool_begin, bypassing
// refcounts, so the caller must know those buffers are dead.

fn u32 cpu_buf_pool_begin(void) {
  return (u32)CPU_BUFS_NEXT;
}

fn void cpu_buf_pool_rollback(u32 wm) {
  if (wm < 1) wm = 1;        // slot 0 is reserved
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].jit_pinned) continue;   // JIT capture holds this buf
    if (CPU_BUFS[i].data || CPU_BUFS[i].handle) {
      cpu_buf_free((u32)i);
    }
  }
  CPU_BUFS_NEXT = wm;
}

// Reclaim every unpreserved, unpinned slot from wm up: park an owned one, free
// an external or arena one.  CPU_BUFS_NEXT stays, as preserved slots sit above wm.
fn void cpu_buf_pool_rollback_with_preserve(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].preserved) continue;
    if (CPU_BUFS[i].jit_pinned) continue;   // sticky JIT retain
    if (CPU_BUFS[i].data == NULL && CPU_BUFS[i].handle == NULL) continue;
    // A parked slot (refcount 0) pushed again would hand one block to two
    // allocations; a global reclaim (wm 1) re-scans parked slots.
    if (CPU_BUFS[i].refcount == 0) continue;
    if (CPU_BUFS[i].skip_freelist) {
      // A per-realize arena is freed, not parked, so its hundreds of MB do
      // not pile up; a view freed after it finds the parent at refcount 0.
      cpu_buf_free((u32)i);
    } else if (CPU_BUFS[i].owns_data) {
      cpu_buf_freelist_push((u32)i);
    } else {
      cpu_buf_free((u32)i);
    }
  }
}

// Global reclaim (py_reclaim): free, not park, every live unpreserved slot, so
// no stale TenDesc naming a slot meets a later tensor's bytes.
fn void cpu_buf_pool_free_unpreserved(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > CPU_BUFS_NEXT) return;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].preserved) continue;
    if (CPU_BUFS[i].jit_pinned) continue;   // sticky JIT retain
    if (CPU_BUFS[i].refcount == 0) continue;
    if (CPU_BUFS[i].data == NULL && CPU_BUFS[i].handle == NULL) continue;
    cpu_buf_free((u32)i);
  }
}

fn void cpu_buf_mark_preserved(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].preserved = 1;
}

// A capture's buffers outlive every rollback and replay, but preserved is
// cleared per realize: a pin is its own flag, set on a view's arena too.
fn void cpu_buf_jit_pin(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  if (CPU_BUFS[buf_id].data == NULL) return;
  CPU_BUFS[buf_id].jit_pinned = 1;
  u32 parent = CPU_BUFS[buf_id].parent_buf_id;
  if (parent != 0 && parent < CPU_BUFS_NEXT) cpu_buf_jit_pin(parent);
}

fn void cpu_buf_jit_unpin(u32 buf_id) {
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].jit_pinned = 0;
  u32 parent = CPU_BUFS[buf_id].parent_buf_id;
  if (parent != 0 && parent < CPU_BUFS_NEXT) cpu_buf_jit_unpin(parent);
}

fn void cpu_buf_clear_preserved(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) CPU_BUFS[i].preserved = 0;
}

// Arena views are plannable intermediates (tinygrad schedule/memory.py:13), so a
// realize drops their preserve bits, and their arena's, for rollback to free.
fn void cpu_buf_clear_preserved_arena_views(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) {
    u32 parent = CPU_BUFS[i].parent_buf_id;
    if (parent == 0) continue;
    CPU_BUFS[i].preserved = 0;
    // A parent below wm is an earlier realize's buffer (a WL-held weight a view
    // reads), not this realize's arena, so it keeps its bit.
    if (parent >= wm && parent < CPU_BUFS_NEXT) CPU_BUFS[parent].preserved = 0;
  }
}

fn void cpu_buf_mark_freeable(u32 buf_id) {
  if (CPU_BUFS == NULL) return;
  if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return;
  CPU_BUFS[buf_id].freeable = 1;
}

fn void cpu_buf_clear_freeable(u32 wm) {
  if (wm < 1) wm = 1;
  for (u64 i = wm; i < CPU_BUFS_NEXT; i++) CPU_BUFS[i].freeable = 0;
}
