// tinygrad's _pool unfold (mixin/movement.py:598-616): the last nk axes of x
// become windows of k with stride s and dilation d, giving shape
// (prefix..., o_0, ..., k_0, ...); each (o, k) slice is the window at output
// position o.  The constructor sequence is Tensor._pool's (py/thvm/tensor.py),
// so both build one graph.

// Python's // for a positive divisor.
static i64 nn_floordiv(i64 a, i64 b) {
  return a >= 0 ? a / b : -((-a + b - 1) / b);
}

fn Tensor nn_pool(Tensor x, u32 nk, u32 const *k, u32 const *s, u32 const *d) {
  Tensor fail = x;
  fail.t = 0;
  u32 nd = x.shape.ndim;
  if (nk == 0 || nk > nd || nd + 2 * nk > MAX_DIM) {
    return fail;
  }
  u32 noop = nd - nk;
  Tensor in = x;   // x is rebound by every step below; its dims are read from in
  u32 const *i_ = &in.shape.dims[noop];
  i64 o[MAX_DIM], f[MAX_DIM];
  for (u32 j = 0; j < nk; j++) {
    if (k[j] == 0 || s[j] == 0 || d[j] == 0 || i_[j] == 0) {
      return fail;
    }
    o[j] = nn_floordiv((i64)i_[j] - (i64)d[j] * (k[j] - 1) + s[j] - 1, s[j]);
    if (o[j] <= 0) {
      return fail;
    }
    i64 fv = nn_floordiv(o[j] * s[j] - d[j] + i_[j] - 1, i_[j]);
    f[j] = fv > 1 ? fv : 1;
  }
  // Tensor.repeat by ceildiv(k * (i*f + d), i) on each window axis: unsqueeze a
  // 1 before every repeated axis, expand it, merge it back.
  u32 unsq[MAX_DIM], expd[MAX_DIM], fin[MAX_DIM], nu = 0;
  for (u32 a = 0; a < nd; a++) {
    u32 r = 1;
    if (a >= noop) {
      u32 j = a - noop;
      r = (u32)nn_floordiv((i64)k[j] * ((i64)i_[j] * f[j] + d[j]) + i_[j] - 1, i_[j]);
    }
    if (r != 1) {
      unsq[nu] = 1;
      expd[nu] = r;
      nu++;
    }
    unsq[nu] = in.shape.dims[a];
    expd[nu] = in.shape.dims[a];
    nu++;
    fin[a] = r * in.shape.dims[a];
  }
  x = nn_reshape(x, nu, unsq);
  x = nn_expand(x, nu, expd);
  x = nn_reshape(x, nd, fin);
  u32 t[MAX_DIM];
  for (u32 j = 0; j < nk; j++) {
    t[noop + j] = (u32)(k[j] * (i_[j] * f[j] + d[j]));
  }
  memcpy(t, in.shape.dims, noop * sizeof t[0]);
  x = nn_shrink_to(x, t);
  // (k, i*f + d) -> shrink (k, o*s) -> (k, o, s) -> shrink (k, o, 1) -> (k, o).
  u32 n2 = noop + 2 * nk, n3 = noop + 3 * nk;
  for (u32 j = 0; j < nk; j++) {
    t[noop + 2 * j]     = k[j];
    t[noop + 2 * j + 1] = (u32)(i_[j] * f[j] + d[j]);
  }
  x = nn_reshape(x, n2, t);
  for (u32 j = 0; j < nk; j++) {
    t[noop + 2 * j + 1] = (u32)(o[j] * s[j]);
  }
  x = nn_shrink_to(x, t);
  for (u32 j = 0; j < nk; j++) {
    t[noop + 3 * j]     = k[j];
    t[noop + 3 * j + 1] = (u32)o[j];
    t[noop + 3 * j + 2] = s[j];
  }
  x = nn_reshape(x, n3, t);
  for (u32 j = 0; j < nk; j++) {
    t[noop + 3 * j + 2] = 1;
  }
  x = nn_shrink_to(x, t);
  for (u32 j = 0; j < nk; j++) {
    t[noop + 2 * j]     = k[j];
    t[noop + 2 * j + 1] = (u32)o[j];
  }
  x = nn_reshape(x, n2, t);
  // prefix, k_0, o_0, k_1, o_1, ... -> prefix, o_0, o_1, ..., k_0, k_1, ...
  u32 perm[MAX_DIM];
  for (u32 a = 0; a < noop; a++) {
    perm[a] = a;
  }
  for (u32 j = 0; j < nk; j++) {
    perm[noop + j]      = noop + 2 * j + 1;
    perm[noop + nk + j] = noop + 2 * j;
  }
  return nn_permute(x, n2, perm);
}
