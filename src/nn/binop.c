// Tensor._binop: broadcast a and b, compute a mixed pair in f32
// (least_upper_dtype on the f32 / i32 lattice, tinygrad
// mixin/elementwise.py:24-34), then the binary op in a's dtype.
fn Tensor nn_binop(u32 op, Tensor a, Tensor b) {
  if (!nn_broadcast(&a, &b)) {
    a.t = 0;
    return a;
  }
  if (a.dtype != b.dtype) {
    a = nn_cast(a, DT_FP32);
    b = nn_cast(b, DT_FP32);
  }
  a.t = uop_binary(op, a.t, b.t);
  return a;
}

// a - b as Tensor.__sub__ builds it: a + (-b).
fn Tensor nn_sub(Tensor a, Tensor b) {
  return nn_binop(UOP_ADD, a, nn_unary(UOP_NEG, b));
}
