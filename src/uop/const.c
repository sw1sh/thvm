// uop/const.c - construct a UOP_CONST node.
//
// Heap layout: [NUM(bits)] -- a single TAG_NUM cell carrying the
// raw bits of the constant value.  EXT of the UOP_CONST term holds
// the dtype so the reader knows how to interpret the bits.

fn Term uop_const(u32 dtype, u32 bits) {
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, dtype, bits));
  return term_new(0, TAG_UOP, UOP_CONST, loc);
}
