// src/aot/cont.c
//
// Continuation cells for the Bend2-style AOT runtime.
//
// When `dispatch(task)` returns AOT_R_SPLIT, the parent has spawned
// two independent child tasks and needs to wait for both values
// before its continuation can fire.  The waiting state lives in a
// continuation cell (a "dp" -- dispatch pad -- in Bend2's naming).
//
// Cell layout (4 + n_extra heap slots, allocated together via
// heap_alloc):
//
//   slot 0:  header   = (cont_id << 12) | (n_extra << 2) | pending
//   slot 1:  ret      = parent's return-slot encoding (passed up
//                       once both children land)
//   slot 2:  child_0  = full Term written by the slot-0 child
//   slot 3:  child_1  = full Term written by the slot-1 child
//   slot 4..3+n_extra: captured args (CPS bound vars closed over)
//
// Pending count starts at 2 (two children to wait for).  Each
// resolved child decrements via atomic_fetch_sub on the header.
// The decrementing thread that observes (old & 3) == 1 was the
// "last writer" and is responsible for firing the cont.
//
// We DON'T mirror Bend2's OR-into-tag-byte trick: thvm's Term has
// its TAG at bit 56-62 (7 bits) and EXT at bit 38-55 (18 bits), so
// you can't just byte-OR them into one word.  Instead each child
// writer stores its full 64-bit Term into its own dedicated slot
// (slot 2 or slot 3).  No atomic OR is needed because slots 2 and
// 3 are exclusive per child.  The atomic_fetch_sub on the header
// (ACQ_REL) is the single synchronization point: by the time the
// last writer observes pending=0, the prior plain stores into
// slots 2 and 3 are visible to it.
//
// n_extra (header bits 11..2, max 1023) is reserved for cont
// continuations that need to capture additional bound-var Terms
// beyond the two child results (e.g., source `let !{a, b} = ... in
// node{a, b, captured_x}` -- captured_x is saved at alloc time so
// fire_cont can pass it through).  Phase 1's hand-coded tree_sum
// has n_extra == 0.

#include "task.h"

#define AOT_CONT_PENDING_MASK   0x3u            // bits 0..1
#define AOT_CONT_NEXTRA_SHIFT   2
#define AOT_CONT_NEXTRA_MASK    0x3FFu          // bits 2..11
#define AOT_CONT_FNID_SHIFT     12              // bits 12..31

// Allocate a fresh cont cell (split case: pending=2).  Caller passes
// the cont fn id, # of captured extras, and parent's ret encoding.
// Captured extras are written by the caller into slots 4..3+n_extra
// AFTER this returns.
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

// Same as aot_alloc_cont but for AOT_R_CALL (one child, pending=1).
// Slot 3 stays unused; only slot 2 will be written.
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

// Write a child's Term into its slot, then decrement pending.
// Returns 1 iff this was the last writer (caller fires the cont).
//
// The plain heap_set into our exclusive slot is followed by an
// atomic_fetch_sub on the header word with ACQ_REL: under T>1 that
// gives a happens-before edge from "I wrote my slot" to "the firing
// thread reads it".  At T=1 the atomic ops degrade to plain RMW
// and everything is correct trivially.
fn u32 aot_write_slot(u32 ret, Term val) {
  u32 dp   = aot_dec_dp(ret);
  u32 slot = aot_dec_slot(ret);
  heap_set(dp + 2 + slot, val);
  u64 old = __atomic_fetch_sub(&HEAP[dp + 0], 1, __ATOMIC_ACQ_REL);
  return ((u32)old & AOT_CONT_PENDING_MASK) == 1u ? 1 : 0;
}

// Read the cell, build an AotTask whose fn_id = cont_id, args =
// the two child values + any captured n_extra args.  Caller
// dispatches the resulting task to fire the cont's body.
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
