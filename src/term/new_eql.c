// Heap layout: [a, b]; strict on a, then on b unless a decides: ERA on either
// side -> ERA, ANY -> NUM(1), a SUP commutes, two NUMs -> NUM(a == b).
fn Term term_new_eql(Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_EQL, 0, loc);
}
