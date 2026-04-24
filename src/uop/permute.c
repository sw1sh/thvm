// uop/permute.c - construct a UOP_PERMUTE node.
//
// Heap layout: [src, NUM(p0), NUM(p1), ..., NUM(p_{ndim-1})] where
// p[i] is the source axis index that becomes the i-th output axis.

fn Term uop_permute(Term src, u32 ndim, const u32 *perm) {
  u64 loc = heap_alloc(1 + ndim);
  heap_set(loc, src);
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 1 + i, term_new(0, TAG_NUM, DT_I32, perm[i]));
  }
  return term_new(0, TAG_UOP, UOP_PERMUTE, loc);
}
