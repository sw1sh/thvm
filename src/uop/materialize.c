// uop/materialize.c - construct a UOP_MATERIALIZE wrapper.
//
// The wrapper itself is just a TAG_UOP with a single heap cell
// holding the wrapped expression.  Reducing it (via TWnf) fires
// the materialize rewrite rule that arrives in commit 3.

fn Term uop_materialize(Term expr) {
  u64 loc = heap_alloc(1);
  heap_set(loc, expr);
  return term_new(0, TAG_UOP, UOP_MATERIALIZE, loc);
}
