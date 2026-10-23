// batchnorm with a precomputed mean and invstd = 1 / sqrt(var + eps), each
// reshaped to (1, C, 1, ...), at least rank 2; weight and bias are optional
// (t == 0).  Tensor.batchnorm's order, (x - mean) * invstd * weight + bias;
// tinygrad's (mixin/op.py:1664-1683) scales by weight before invstd.
fn Tensor nn_batchnorm(Tensor x, Tensor weight, Tensor bias, Tensor mean, Tensor invstd) {
  u32 nd = x.shape.ndim < 2 ? 2 : x.shape.ndim, s[MAX_DIM];
  for (u32 i = 0; i < nd; i++) {
    s[i] = 1;
  }
  s[1] = shape_numel(mean.shape);
  Tensor y = nn_sub(x, nn_reshape(mean, nd, s));
  s[1] = shape_numel(invstd.shape);
  y = nn_binop(UOP_MUL, y, nn_reshape(invstd, nd, s));
  if (weight.t != 0) {
    s[1] = shape_numel(weight.shape);
    y = nn_binop(UOP_MUL, y, nn_reshape(weight, nd, s));
  }
  if (bias.t != 0) {
    s[1] = shape_numel(bias.shape);
    y = nn_binop(UOP_ADD, y, nn_reshape(bias, nd, s));
  }
  return y;
}
