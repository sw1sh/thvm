// Tensor._broadcast: a and b rewritten to their common shape (uop_broadcast2),
// untouched when the shapes already agree; 0 when an axis is neither equal nor 1.
fn int nn_broadcast(Tensor *a, Tensor *b) {
  if (a->shape.ndim == b->shape.ndim
      && memcmp(a->shape.dims, b->shape.dims, a->shape.ndim * sizeof a->shape.dims[0]) == 0) {
    return 1;
  }
  Shape out = {0, {0}};
  int nd = uop_broadcast2(&a->t, a->shape.ndim, a->shape.dims, &b->t, b->shape.ndim,
                          b->shape.dims, out.dims);
  if (nd < 0) {
    return 0;
  }
  out.ndim = (u32)nd;
  a->shape = out;
  b->shape = out;
  return 1;
}
