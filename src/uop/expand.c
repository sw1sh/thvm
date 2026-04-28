// uop/expand.c - construct a UOP_EXPAND node.
//
// Heap layout: [src, NUM(ndim), NUM(d0), ..., NUM(d_{ndim-1})] where
// ndim is stored explicitly at slot 1 so the materializer doesn't
// have to infer it from the source's rank.  src stays at slot 0 to
// match the per-op "child at slot i" convention used by the
// schedule/materialize child loop.
//
// Storing ndim explicitly is what makes EXPAND legitimately able to
// change rank (e.g. lift a scalar gy to a {2,3} target shape during
// backprop, which the source-rank-inference path could not represent).

fn Term uop_expand(Term src, u32 ndim, const u32 *dims) {
  // Hash-cons by (op, src, ndim, dims).  Repeat construction (e.g.
  // EXPAND(CONST(0), target.shape) appearing in every TGrad WL
  // wrap, multiplied across nested rounds) reuses the existing
  // heap loc.
  u32 key_buf[1 + MAX_DIM];
  key_buf[0] = ndim;
  for (u32 i = 0; i < ndim; i++) key_buf[1 + i] = dims[i];
  u64 key = uop_mov_hash(UOP_EXPAND, src, key_buf, 1 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_I32, dims[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_EXPAND, loc);
  uop_mov_insert(key, t);
  return t;
}
