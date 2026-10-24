// tinygrad's batchnorm (mixin/op.py:1664-1683) over x's n axes (distinct, in
// range) with a precomputed mean and invstd = 1 / sqrt(var + eps): (x - mean) *
// weight * invstd + bias, each parameter reshaped to x's shape with 1 off those
// axes; an invstd of another rank than n broadcasts as it is.  weight and bias
// are optional (t == 0).
fn Tensor nn_batchnorm(Tensor x, Tensor weight, Tensor bias, Tensor mean, Tensor invstd, u32 n,
                       u32 const *axes) {
  u32 nd = x.shape.ndim, s[MAX_DIM];
  for (u32 i = 0; i < nd; i++) {
    s[i] = 1;
  }
  for (u32 j = 0; j < n; j++) {
    s[axes[j]] = x.shape.dims[axes[j]];
  }
  Tensor y = nn_sub(x, nn_reshape(mean, nd, s));
  if (weight.t != 0) {
    y = nn_binop(UOP_MUL, y, nn_reshape(weight, nd, s));
  }
  y = nn_binop(UOP_MUL, y, invstd.shape.ndim == n ? nn_reshape(invstd, nd, s) : invstd);
  if (bias.t != 0) {
    y = nn_binop(UOP_ADD, y, nn_reshape(bias, nd, s));
  }
  return y;
}
