// term/new_ddu.c - construct a TAG_DDU (dynamic-label DUP).
//
// Heap layout: [lab_term, val, body].  Strict on lab_term.  Once
// the label resolves to NUM(n), DDU reduces by cloning `val` under
// label n into projections X0/X1, then APP-ing the body to (X0, X1)
// in left-to-right order (body must therefore be a 2-arg LAM-pair).
// Mirrors HVM4's term_new_ddu in TinyHVM/HVM4/clang/term/new/ddu.c.

fn Term term_new_ddu(Term lab_term, Term val, Term body) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, lab_term);
  heap_set(loc + 1, val);
  heap_set(loc + 2, body);
  return term_new(0, TAG_DDU, 0, loc);
}
