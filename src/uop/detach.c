// Stop-gradient (tinygrad Ops.DETACH): the chain rule sends no cotangent past
// it, and materialize strips DETACH(x) -> x (materialize_strip_detach).
fn Term uop_detach(Term src) {
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  return term_new(0, TAG_UOP, UOP_DETACH, loc);
}
