// term/new_or.c - construct a TAG_OR.
//
// Heap layout: [a, b].  Short-circuit boolean: strict on a only.
//   OR(NUM(0), b)       -> wnf(b)
//   OR(NUM(n != 0), b)  -> NUM(1)         (b never reduced)
//   OR(ERA, b)          -> ERA
//   OR(SUP_L{a0,a1}, b) -> &L{OR(a0,B0), OR(a1,B1)}, !&L{B0,B1}=b
//   otherwise           -> stuck
fn Term term_new_or(Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_OR, 0, loc);
}
