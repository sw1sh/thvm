// term/new_and.c - construct a TAG_AND.
//
// Heap layout: [a, b].  Short-circuit boolean: strict on a only.
//   AND(NUM(0), b)      -> NUM(0)        (b never reduced)
//   AND(NUM(n != 0), b) -> wnf(b)
//   AND(ERA, b)         -> ERA
//   AND(SUP_L{a0,a1}, b) -> &L{AND(a0,B0), AND(a1,B1)}, !&L{B0,B1}=b
//   otherwise           -> stuck
fn Term term_new_and(Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_AND, 0, loc);
}
