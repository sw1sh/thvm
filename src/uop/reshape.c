// Heap layout: [src, NUM(ndim), NUM(d0), ..., NUM(d_{ndim-1})].

fn Term uop_reshape(Term src, u32 ndim, const u32 *dims) {
  Term collapsed = uop_rewrite_movement_src(UOP_RESHAPE, src);
  if (collapsed != 0) src = collapsed;
  // A reshape to src's own shape is src, as tinygrad's (mixin/movement.py:184),
  // here after the RESHAPE(RESHAPE) collapse: an unsqueeze a flatten undoes, as
  // a one-piece cat's, leaves no node.
  Shape s;
  if (term_shape_in(src, 0, &s) && s.ndim == ndim && memcmp(s.dims, dims, ndim * sizeof *dims) == 0) {
    return src;
  }
  u32 key_buf[1 + MAX_DIM];
  key_buf[0] = ndim;
  for (u32 i = 0; i < ndim; i++) key_buf[1 + i] = dims[i];
  u64 key = uop_mov_hash(UOP_RESHAPE, src, key_buf, 1 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, dims[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_RESHAPE, loc);
  uop_mov_insert(key, t);
  return t;
}
