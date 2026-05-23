// uop/detach.c - stop-gradient marker (tinygrad UOp detach).
//
// Identity at runtime; uop_graph_simplify unwraps DETACH(x) -> x before
// materialize so no kernel/render/walker path observes it.  Its only
// live role is in uop_grad: the cotangent reaching a DETACH dies (the
// child is not differentiated), matching `x.detach()`.
fn Term uop_detach(Term src) {
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  return term_new(0, TAG_UOP, UOP_DETACH, loc);
}
