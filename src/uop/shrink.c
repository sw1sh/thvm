// uop/shrink.c - construct a UOP_SHRINK node.
//
// Heap layout: [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...] where
// b_i / e_i bound the kept slice [b_i, e_i) along axis i.  Inverse
// of UOP_PAD's shape operation.

fn Term uop_shrink(Term src, u32 ndim, const u32 *begin_end) {
  u64 loc = heap_alloc(1 + 2 * ndim);
  heap_set(loc, src);
  for (u32 i = 0; i < 2 * ndim; i++) {
    heap_set(loc + 1 + i, term_new(0, TAG_NUM, DT_I32, begin_end[i]));
  }
  return term_new(0, TAG_UOP, UOP_SHRINK, loc);
}
