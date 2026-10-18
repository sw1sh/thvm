// src/aot/cont.c -- continuation cells (Bend2's dp): 4 + n_extra cells
// [cont_id << 12 | n_extra << 2 | pending, parent ret, child0, child1, extras].
// Each child stores its whole Term in its own slot (Bend2 ORs a tag byte into
// a shared word; a Term's tag and ext do not fit one), then an ACQ_REL
// fetch_sub on pending publishes it; the writer taking pending to 0 fires.

#include "task.h"

#define AOT_CONT_PENDING_MASK   0x3u            // bits 0..1
#define AOT_CONT_NEXTRA_SHIFT   2
#define AOT_CONT_NEXTRA_MASK    0x3FFu          // bits 2..11
#define AOT_CONT_FNID_SHIFT     12              // bits 12..31

// SPLIT cell, pending 2.  The caller writes the n_extra captured Terms to
// slots 4.. after this returns.
fn u64 aot_alloc_cont(u32 cont_id, u32 n_extra, u32 ret) {
  u64 dp = aot_heap_alloc(4 + n_extra);
  u64 hdr = ((u64)cont_id << AOT_CONT_FNID_SHIFT)
          | ((u64)(n_extra & AOT_CONT_NEXTRA_MASK) << AOT_CONT_NEXTRA_SHIFT)
          | 2u;
  heap_set(dp + 0, (Term)hdr);
  heap_set(dp + 1, (Term)ret);
  heap_set(dp + 2, (Term)0);
  heap_set(dp + 3, (Term)0);
  return dp;
}

// CALL cell, pending 1: only slot 2 is written.
fn u64 aot_alloc_cont_call(u32 cont_id, u32 n_extra, u32 ret) {
  u64 dp = aot_heap_alloc(4 + n_extra);
  u64 hdr = ((u64)cont_id << AOT_CONT_FNID_SHIFT)
          | ((u64)(n_extra & AOT_CONT_NEXTRA_MASK) << AOT_CONT_NEXTRA_SHIFT)
          | 1u;
  heap_set(dp + 0, (Term)hdr);
  heap_set(dp + 1, (Term)ret);
  heap_set(dp + 2, (Term)0);
  heap_set(dp + 3, (Term)0);
  return dp;
}

// Returns 1 iff this was the last writer, which then fires the cont.
fn u32 aot_write_slot(u32 ret, Term val) {
  u32 dp   = aot_dec_dp(ret);
  u32 slot = aot_dec_slot(ret);
  heap_set(dp + 2 + slot, val);
  u64 old = __atomic_fetch_sub(&HEAP[dp + 0], 1, __ATOMIC_ACQ_REL);
  return ((u32)old & AOT_CONT_PENDING_MASK) == 1u ? 1 : 0;
}

// The cont's task: fn_id cont_id, ret the parent's slot, args the two child
// values then the extras (at most AOT_MAX_ARGS - 2 of them).
fn AotTask aot_fire_cont(u64 dp) {
  Term hdr_t   = heap_read(dp + 0);
  u32  hdr     = (u32)hdr_t;
  u32  cont_id = hdr >> AOT_CONT_FNID_SHIFT;
  u32  n_extra = (hdr >> AOT_CONT_NEXTRA_SHIFT) & AOT_CONT_NEXTRA_MASK;

  AotTask t = aot_make_task(cont_id, (u32)heap_read(dp + 1),
                            heap_read(dp + 2),
                            heap_read(dp + 3),
                            0, 0);
  for (u32 i = 0; i < n_extra && i + 2 < AOT_MAX_ARGS; i++) {
    t.args[2 + i] = heap_read(dp + 4 + i);
  }
  return t;
}
