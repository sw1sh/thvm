// uop/pad.c - construct a UOP_PAD node.
//
// Heap layout: [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...] where
// b_i and e_i are the begin / end pad widths for axis i.

fn Term uop_pad(Term src, u32 ndim, const u32 *begin_end) {
  u32 key_buf[1 + 2 * MAX_DIM];
  key_buf[0] = ndim;
  for (u32 i = 0; i < 2 * ndim; i++) key_buf[1 + i] = begin_end[i];
  u64 key = uop_mov_hash(UOP_PAD, src, key_buf, 1 + 2 * ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(1 + 2 * ndim);
  heap_set(loc, src);
  for (u32 i = 0; i < 2 * ndim; i++) {
    heap_set(loc + 1 + i, term_new(0, TAG_NUM, DT_I32, begin_end[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_PAD, loc);
  uop_mov_insert(key, t);
  return t;
}
