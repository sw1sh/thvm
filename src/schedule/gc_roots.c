// schedule/gc_roots.c - tracing-GC root collection.
//
// gc_collect_roots populates `out[0..n)` with the live Term
// roots from which the gc-mark walk needs to trace.  Sources:
//
//   1. The wnf result the caller is about to return to WL.
//   2. WNF_LAST_STACK -- eliminator frames captured when wnf_n
//      bailed early; each may still hold refs the next
//      reduction step needs.
//   3. DEFS[name] for each name with a non-zero entry; TRef
//      roots that gc_mark_term follows into BOOK_HEAP via
//      TAG_REF / TAG_ALO recursion.
//   4. EXTERN_PINNED_TERMS -- every Term a foreign caller is
//      holding; covers forward-intermediate kernel outputs
//      reachable only from the caller's wrappers.
//
// ALO_STATES is NOT a Term source: it maps book locs to fresh
// dyn locs; the actual Term content lives at HEAP[new_loc] and
// gets discovered through the TAG_ALO cells referencing it.
//
// `cap` bounds the output to a fixed buffer the caller owns
// (typical: 256).  Excess roots are silently dropped -- the
// caller is expected to pick a cap large enough that the
// truncation never bites.  For our scale (single-step bench
// runs with a few dozen roots at most), 256 is generous.
fn void gc_collect_roots(Term result, Term *out, u32 cap, u32 *out_n) {
  u32 n = 0;
  if (out == NULL || out_n == NULL || cap == 0) {
    if (out_n) *out_n = 0;
    return;
  }
  if (result != 0 && n < cap) out[n++] = result;
  for (u32 i = 0; i < WNF_LAST_STACK_LEN && n < cap; i++) {
    if (WNF_LAST_STACK[i] != 0) out[n++] = WNF_LAST_STACK[i];
  }
  for (u32 i = 0; i < DEFS_CAP && n < cap; i++) {
    if (DEFS[i] != 0) out[n++] = DEFS[i];
  }
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN && n < cap; i++) {
    if (EXTERN_PINNED_TERMS[i] != 0) out[n++] = EXTERN_PINNED_TERMS[i];
  }
  *out_n = n;
}
