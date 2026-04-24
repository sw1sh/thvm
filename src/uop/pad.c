// uop/pad.c - construct a UOP_PAD node.
//
// Heap layout: [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...] where
// b_i and e_i are the begin / end pad widths for axis i.

fn Term uop_pad(Term src, u32 ndim, const u32 *begin_end) {
  u64 loc = heap_alloc(1 + 2 * ndim);
  heap_set(loc, src);
  for (u32 i = 0; i < 2 * ndim; i++) {
    heap_set(loc + 1 + i, term_new(0, TAG_NUM, DT_I32, begin_end[i]));
  }
  return term_new(0, TAG_UOP, UOP_PAD, loc);
}
