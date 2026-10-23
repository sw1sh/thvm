// x @ w as RESHAPE, EXPAND, MUL and a SUM, which the scheduler fuses into one
// matmul kernel.  Two 2-D operands take Tensor.__matmul__'s (M, K, N) form; any
// other ranks take tinygrad's dot (mixin/op.py:387-392): x's last axis against
// w's second to last (its only one at rank 1), the batch axes broadcast.
fn Tensor nn_dot(Tensor x, Tensor w) {
  Tensor fail = x;
  fail.t = 0;
  u32 dx = x.shape.ndim, dw = w.shape.ndim;
  if (dx == 2 && dw == 2) {
    u32 M = x.shape.dims[0], K = x.shape.dims[1], N = w.shape.dims[1];
    if (w.shape.dims[0] != K) {
      return fail;
    }
    u32 xr[3] = {M, K, 1}, wr[3] = {1, K, N}, full[3] = {M, K, N}, red[1] = {1};
    Tensor a = nn_expand(nn_reshape(x, 3, xr), 3, full);
    Tensor b = nn_expand(nn_reshape(w, 3, wr), 3, full);
    return nn_reduce(REDUCE_SUM, nn_binop(UOP_MUL, a, b), 1, red, 0);
  }
  if (dx == 0 || dw == 0) {
    return fail;
  }
  u32 aw = dw < 2 ? dw : 2;   // w's contraction axis is dw - aw
  u32 np = dx - 1 < dw - 1 ? dx - 1 : dw - 1;
  np = np < 1 ? np : 1;
  u32 nwd = (dw >= 2 ? dw - 2 : 0) + np + aw;
  if (x.shape.dims[dx - 1] != w.shape.dims[dw - aw] || dx + np > MAX_DIM || nwd > MAX_DIM) {
    return fail;
  }
  u32 xs[MAX_DIM], ws[MAX_DIM], perm[MAX_DIM], n = 0;
  for (u32 i = 0; i + 1 < dx; i++) {
    xs[n++] = x.shape.dims[i];
  }
  for (u32 i = 0; i < np; i++) {
    xs[n++] = 1;
  }
  xs[n++] = x.shape.dims[dx - 1];
  x = nn_reshape(x, n, xs);
  n = 0;
  for (u32 i = 0; i + 2 < dw; i++) {
    ws[n++] = w.shape.dims[i];
  }
  for (u32 i = 0; i < np; i++) {
    ws[n++] = 1;
  }
  for (u32 i = dw - aw; i < dw; i++) {
    ws[n++] = w.shape.dims[i];
  }
  w = nn_reshape(w, n, ws);
  // transpose(-1, -aw): the contraction axis goes last in w too.
  for (u32 i = 0; i < n; i++) {
    perm[i] = i;
  }
  perm[n - 1]  = n - aw;
  perm[n - aw] = n - 1;
  w = nn_permute(w, n, perm);
  if (!nn_broadcast(&x, &w)) {
    return fail;
  }
  u32 last = x.shape.ndim - 1;
  return nn_reduce(REDUCE_SUM, nn_binop(UOP_MUL, x, w), 1, &last, 0);
}
