// Dynamic-label DUP. Heap layout: [lab_term, val, body]; strict on lab_term:
// NUM(n) -> (body X0 X1), X0/X1 being val's label-n projections.
// Mirrors HVM4's term_new_ddu in HVM4 clang/term/new/ddu.c.
fn Term term_new_ddu(Term lab_term, Term val, Term body) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, lab_term);
  heap_set(loc + 1, val);
  heap_set(loc + 2, body);
  return term_new(0, TAG_DDU, 0, loc);
}
