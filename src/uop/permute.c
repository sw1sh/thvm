// uop/permute.c - construct a UOP_PERMUTE node.
//
// Heap layout: [src, NUM(ndim), NUM(p0), NUM(p1), ..., NUM(p_{ndim-1})]
// where p[i] is the source axis index that becomes the i-th
// output axis.  Total cells: 2 + ndim.

fn Term uop_permute(Term src, u32 ndim, const u32 *perm) {
  u32 key_buf[1 + MAX_DIM];
  key_buf[0] = ndim;
  for (u32 i = 0; i < ndim; i++) key_buf[1 + i] = perm[i];
  u64 key = uop_mov_hash(UOP_PERMUTE, src, key_buf, 1 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_I32, perm[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_PERMUTE, loc);
  uop_mov_insert(key, t);
  return t;
}
