// tinygrad's batchnorm (mixin/op.py:1664-1683) over x's axis 1 with a
// precomputed mean and invstd = 1 / sqrt(var + eps): (x - mean) * weight * invstd
// + bias, each parameter reshaped to (1, C, 1, ...); weight and bias are optional
// (t == 0).  t == 0 below rank 2.
fn Tensor nn_batchnorm(Tensor x, Tensor weight, Tensor bias, Tensor mean, Tensor invstd) {
  u32 nd = x.shape.ndim, s[MAX_DIM];
  if (nd < 2) {
    x.t = 0;
    return x;
  }
  for (u32 i = 0; i < nd; i++) {
    s[i] = i == 1 ? x.shape.dims[1] : 1;
  }
  Tensor y = nn_sub(x, nn_reshape(mean, nd, s));
  if (weight.t != 0) {
    y = nn_binop(UOP_MUL, y, nn_reshape(weight, nd, s));
  }
  // An invstd of rank 1 is per channel; a broadcastable one goes in as it is.
  y = nn_binop(UOP_MUL, y, invstd.shape.ndim == 1 ? nn_reshape(invstd, nd, s) : invstd);
  if (bias.t != 0) {
    y = nn_binop(UOP_ADD, y, nn_reshape(bias, nd, s));
  }
  return y;
}
