// schedule/heap_rooted_preserve.c - hrp1 of the heap-rooted
// preserve arc.
//
// `mark_heap_rooted_preserve()` walks the live dyn heap range
// [0, HEAP_NEXT) and marks every TenDesc reachable from a
// TAG_TEN cell as preserved on its owning backend.
// This is a strict superset of the bufs reachable from the
// result tensor's producer chain that mark_preserved_chain
// catches today, in two important ways:
//
//   1. UOP children stored in HEAP[] (e.g., the operand cells
//      of a TUOpAdd[a, b] sitting at expr_loc + 0..1) are
//      visited directly by the linear scan -- no separate
//      UOP-arity walk needed.  A pending UOP_GRAD term still
//      sits in the heap with TAG_TEN children pointing at
//      forward intermediates; this catches them.
//
//   2. Bufs reachable through HEAP cells but NOT through the
//      result's producer_kid chain (e.g., a TenDesc the WL
//      caller still holds via a separate TTerm) get
//      preserved too -- mark_preserved_chain misses these.
//
// Conversely, the heap-rooted scan is MORE PERMISSIVE: if a
// TAG_TEN cell sits in the heap as an unreachable orphan
// (no live path from any term the runtime is acting on), its
// buf gets preserved anyway.  That's fine -- spurious
// preservation only leaks bytes, never breaks correctness.
//
// term_resolve follows SUB-bit chains (a substituted variable
// at heap[loc] holds the new term with SUB=1; we want the
// resolved tag, not the SUB pointer's own tag).
fn void mark_heap_rooted_preserve(void) {
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    Term t = HEAP[i];
    if (t == 0) continue;
    Term r = term_resolve(t);
    if (term_tag(r) != TAG_TEN) continue;
    u32 tid = (u32)term_val(r);
    tensor_mark_buf_preserved(tid);
  }
}
