// term/new_dsu.c - construct a TAG_DSU (dynamic-label SUP).
//
// Heap layout: [lab_term, a, b].  Strict on lab_term: wnf reduces
// the label cell first, then dispatches based on the resolved tag
// (NUM => plain SUP^n{a,b}, ERA => ERA, SUP => nested SUP via
// cross-product on cloned a, b).  Mirrors HVM4's term_new_dsu in
// TinyHVM/HVM4/clang/term/new/dsu.c.

fn Term term_new_dsu(Term lab_term, Term a, Term b) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, lab_term);
  heap_set(loc + 1, a);
  heap_set(loc + 2, b);
  return term_new(0, TAG_DSU, 0, loc);
}
