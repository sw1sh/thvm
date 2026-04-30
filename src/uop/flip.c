// uop/flip.c - construct a UOP_FLIP node.
//
// Heap layout: [src, NUM(axes_bitmask)].  Bit i of the bitmask is
// set iff axis i is flipped.  Implemented in step 14 by negating
// strides[i] and adjusting offset.

fn Term uop_flip(Term src, u32 axes_bitmask) {
  u32 args[1] = {axes_bitmask};
  u64 key = uop_mov_hash(UOP_FLIP, src, args, 1);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, axes_bitmask));
  Term t = term_new(0, TAG_UOP, UOP_FLIP, loc);
  uop_mov_insert(key, t);
  return t;
}
