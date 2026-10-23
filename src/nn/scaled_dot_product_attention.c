// softmax(q k^T / sqrt(d) + mask) v (tinygrad mixin/rand.py:298-327): the mask is
// additive (mask.t != 0) or, with `causal`, 0 on and below the diagonal and -inf
// above.  No dropout.
fn Tensor nn_scaled_dot_product_attention(Tensor q, Tensor k, Tensor v, Tensor mask,
                                          int causal) {
  u32 nk = k.shape.ndim;
  if (nk < 2 || q.shape.ndim == 0 || (causal && mask.t != 0)) {
    q.t = 0;
    return q;
  }
  u32 perm[MAX_DIM];
  for (u32 i = 0; i < nk; i++) {
    perm[i] = i;
  }
  perm[nk - 2] = nk - 1;
  perm[nk - 1] = nk - 2;
  Tensor qk = nn_dot(q, nn_permute(k, nk, perm));
  if (qk.t == 0) {
    return qk;
  }
  qk = nn_binop_f(UOP_MUL, qk, 1.0 / sqrt((double)q.shape.dims[q.shape.ndim - 1]));
  if (causal) {
    Tensor tri = nn_tri(nn_full(1, qk.dtype, qk.shape), 0, 1);
    Tensor zero = nn_full(0, qk.dtype, qk.shape);
    mask = nn_where(tri, zero, nn_full(-INFINITY, qk.dtype, qk.shape));
  }
  if (mask.t != 0) {
    qk = nn_binop(UOP_ADD, qk, mask);
  }
  return nn_dot(nn_softmax(qk, -1, 0), v);
}
