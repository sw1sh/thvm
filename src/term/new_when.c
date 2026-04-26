// term/new_when.c - construct a TAG_WHEN filter.
//
// Heap layout: [cond, body].  Strict on cond only:
//   WHEN(NUM(0), _)        -> ERA            (failed branch)
//   WHEN(NUM(n != 0), body) -> wnf(body)
//   WHEN(ERA, _)           -> ERA
//   WHEN(&L{c0,c1}, body)  -> &L{WHEN(c0, B0), WHEN(c1, B1)}, !&L{B0,B1}=body
//   otherwise              -> stuck
//
// This is the IC-side primitive for "collapse to the matching one":
// WHEN(EQL(cand, expected), cand) becomes ERA when the candidate
// doesn't match (collapse drops it) and `cand` when it does.

fn Term term_new_when(Term cond, Term body) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, cond);
  heap_set(loc + 1, body);
  return term_new(0, TAG_WHEN, 0, loc);
}
