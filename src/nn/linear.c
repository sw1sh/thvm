// tinygrad's linear (mixin/op.py:1254-1271): x * w for a 1-D w, else x @ w, then
// + b when b.t != 0.
fn Tensor nn_linear(Tensor x, Tensor w, Tensor b) {
  Tensor y = w.shape.ndim == 1 ? nn_binop(UOP_MUL, x, w) : nn_dot(x, w);
  return b.t != 0 && y.t != 0 ? nn_binop(UOP_ADD, y, b) : y;
}
