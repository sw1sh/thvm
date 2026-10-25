// Dynamic-label SUP. Heap layout: [lab_term, a, b]; strict on lab_term:
// NUM(n) -> SUP^n{a, b}, ERA -> ERA, a SUP label commutes over DUPed a and b.
// Mirrors HVM4's term_new_dsu in HVM4 clang/term/new/dsu.c.
fn Term term_new_dsu(Term lab_term, Term a, Term b) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, lab_term);
  heap_set(loc + 1, a);
  heap_set(loc + 2, b);
  return term_new(0, TAG_DSU, 0, loc);
}
