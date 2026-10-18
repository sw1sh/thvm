// Heap layout: [a, b]; strict on a: NUM(0) -> NUM(0), other NUM -> wnf(b),
// ERA -> ERA, a SUP commutes (b is DUPed), anything else is stuck.
fn Term term_new_and(Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_AND, 0, loc);
}
