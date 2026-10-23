// tinygrad's MAX as selects on a broadcast, promoted pair: at a tie on floats the
// average, so each side gets half the gradient as MAX's rule gives
// (mixin/gradient.py:78-79).
fn Tensor nn_maximum(Tensor a, Tensor b) {
  if (!nn_broadcast(&a, &b)) {
    a.t = 0;
    return a;
  }
  if (a.dtype != b.dtype) {
    u32 d = nn_promote(a.dtype, b.dtype);
    a = nn_cast(a, d);
    b = nn_cast(b, d);
  }
  Tensor lt = nn_cmplt(a, b);
  if (!dtype_is_float(a.dtype)) {
    return nn_where(lt, b, a);
  }
  Tensor eq  = nn_cast(nn_binop(UOP_CMPEQ, a, b), DT_FP32);
  Tensor avg = nn_binop_f(UOP_MUL, a, 0.5);
  avg = nn_binop(UOP_ADD, avg, nn_binop_f(UOP_MUL, b, 0.5));
  return nn_where(lt, b, nn_where(eq, avg, a));
}

// -max(-a, -b).
fn Tensor nn_minimum(Tensor a, Tensor b) {
  Tensor na = nn_unary(UOP_NEG, a);
  return nn_unary(UOP_NEG, nn_maximum(na, nn_unary(UOP_NEG, b)));
}
