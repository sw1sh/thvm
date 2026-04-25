// uop/reshape.c - construct a UOP_RESHAPE node.
//
// Heap layout: [src, NUM(d0), NUM(d1), ..., NUM(d_{ndim-1})].
// ext field is just UOP_RESHAPE (matches every other UOP's
// convention).  The reader recovers ndim by reading dim NUM cells
// and stopping when the running product equals the input numel
// (which RESHAPE preserves by definition).

fn Term uop_reshape(Term src, u32 ndim, const u32 *dims) {
  u64 loc = heap_alloc(1 + ndim);
  heap_set(loc, src);
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 1 + i, term_new(0, TAG_NUM, DT_I32, dims[i]));
  }
  return term_new(0, TAG_UOP, UOP_RESHAPE, loc);
}
