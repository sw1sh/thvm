// uop/flip.c - construct a UOP_FLIP node.
//
// Heap layout: [src, NUM(axes_bitmask)].  Bit i of the bitmask is
// set iff axis i is flipped.  Implemented in step 14 by negating
// strides[i] and adjusting offset.

fn Term uop_flip(Term src, u32 axes_bitmask) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, axes_bitmask));
  return term_new(0, TAG_UOP, UOP_FLIP, loc);
}
