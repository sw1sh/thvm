// tinygrad's layernorm (mixin/op.py) over the n axes: (x - mean) * rsqrt(var + eps),
// the variance the biased mean of squares.
fn Tensor nn_layernorm(Tensor x, u32 n, u32 const *axes, double eps) {
  Tensor y   = nn_sub(x, nn_mean(x, n, axes, 1));
  Tensor var = nn_mean(nn_binop(UOP_MUL, y, y), n, axes, 1);
  return nn_binop(UOP_MUL, y, nn_elementwise(NN_RSQRT, nn_binop_f(UOP_ADD, var, eps)));
}
