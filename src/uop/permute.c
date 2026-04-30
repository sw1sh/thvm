// uop/permute.c - construct a UOP_PERMUTE node.
//
// Heap layout: [src, NUM(ndim), NUM(p0), NUM(p1), ..., NUM(p_{ndim-1})]
// where p[i] is the source axis index that becomes the i-th
// output axis.  Total cells: 2 + ndim.

fn Term uop_permute(Term src, u32 ndim, const u32 *perm) {
  // Identity permute: perm[i] == i for all i -> drop the op.
  // (Common in autograd where the inverse-of-identity transpose
  // appears for symmetric chains.)
  u8 identity = 1;
  for (u32 i = 0; i < ndim; i++) {
    if (perm[i] != i) { identity = 0; break; }
  }
  if (identity) return src;
  // Compose permute-of-permute: outer.perm[i] = inner.perm[outer.perm[i]].
  // Drops the intermediate node; the composition gets hash-cons'd
  // like any normal permute.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_PERMUTE) {
    u64 inner_loc = term_val(src);
    Term inner_ndim_cell = heap_read(inner_loc + 1);
    if (term_tag(inner_ndim_cell) == TAG_NUM
        && (u32)term_val(inner_ndim_cell) == ndim) {
      u32 composed[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        u32 inner_p = (u32)term_val(heap_read(inner_loc + 2 + perm[i]));
        composed[i] = inner_p;
      }
      Term inner_src = heap_read(inner_loc);
      return uop_permute(inner_src, ndim, composed);
    }
  }
  u32 key_buf[1 + MAX_DIM];
  key_buf[0] = ndim;
  for (u32 i = 0; i < ndim; i++) key_buf[1 + i] = perm[i];
  u64 key = uop_mov_hash(UOP_PERMUTE, src, key_buf, 1 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, perm[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_PERMUTE, loc);
  uop_mov_insert(key, t);
  return t;
}
