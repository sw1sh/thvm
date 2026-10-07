// uop/load.c - construct a UOP_LOAD node.
//
// Heap layout: [src] -- one cell holding the source tensor (or
// any other UOP).  LOAD is a structural marker mirroring tinygrad's
// UOps.LOAD: it makes the "this kernel reads input N from buffer X"
// step explicit instead of having materialize implicitly bind
// TAG_TEN children to input slots.  At the runtime level it's an
// identity (memcpy in the CPU kernel).

fn Term uop_load(Term src) {
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  return term_new(0, TAG_UOP, UOP_LOAD, loc);
}
