// uop/reshape.c - construct a UOP_RESHAPE node.
//
// Heap layout: [src, NUM(ndim), NUM(d0), NUM(d1), ..., NUM(d_{ndim-1})]
// with ndim stored explicitly at slot 1.  src stays at slot 0 to
// match the per-op "child at slot i" convention used by the
// schedule/materialize child loop.
//
// Storing ndim explicitly removes the previous shape-recovery hack
// that walked dim cells until the running product equalled the
// input numel -- correct for shapes whose only intermediate prefix
// product reaching numel was the full one, but BROKEN for shapes
// containing leading 1s (e.g. {1, 4} on a numel-4 source would stop
// after the first cell, losing the trailing dim).

fn Term uop_reshape(Term src, u32 ndim, const u32 *dims) {
  // Collapse reshape-of-reshape (intermediate shape is overridden).
  Term collapsed = uop_rewrite_movement_src(UOP_RESHAPE, src);
  if (collapsed != 0) src = collapsed;
  // Hash-cons by (op, src, ndim, dims).  See uop/mov_cache.c.
  u32 key_buf[1 + MAX_DIM];
  key_buf[0] = ndim;
  for (u32 i = 0; i < ndim; i++) key_buf[1 + i] = dims[i];
  u64 key = uop_mov_hash(UOP_RESHAPE, src, key_buf, 1 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_I32, dims[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_RESHAPE, loc);
  uop_mov_insert(key, t);
  return t;
}
