// uop/expand.c - construct a UOP_EXPAND node.
//
// Heap layout: [src, NUM(d0), ..., NUM(d_{ndim-1})] where each d_i
// is the new size of axis i (or the same as src if not expanding
// that axis).  Implemented by setting strides[i] = 0 in step 14;
// step 12 only stores the construction.

fn Term uop_expand(Term src, u32 ndim, const u32 *dims) {
  u64 loc = heap_alloc(1 + ndim);
  heap_set(loc, src);
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 1 + i, term_new(0, TAG_NUM, DT_I32, dims[i]));
  }
  return term_new(0, TAG_UOP, UOP_EXPAND, loc);
}
