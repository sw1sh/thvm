// Tensor._binop: broadcast a and b, compute a mixed pair in their least upper
// dtype (nn_promote, tinygrad mixin/elementwise.py:24-34), then the binary op.
fn Tensor nn_binop(u32 op, Tensor a, Tensor b) {
  if (!nn_broadcast(&a, &b)) {
    a.t = 0;
    return a;
  }
  if (a.dtype != b.dtype) {
    u32 d = nn_promote(a.dtype, b.dtype);
    a = nn_cast(a, d);
    b = nn_cast(b, d);
  }
  a.t = uop_binary(op, a.t, b.t);
  return a;
}

// a - b as Tensor.__sub__ builds it: a + (-b).
fn Tensor nn_sub(Tensor a, Tensor b) {
  return nn_binop(UOP_ADD, a, nn_unary(UOP_NEG, b));
}
