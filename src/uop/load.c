// A read marker after tinygrad's Ops.LOAD, heap [src]: an identity on its
// value, which materialize strips.
fn Term uop_load(Term src) {
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  return term_new(0, TAG_UOP, UOP_LOAD, loc);
}
