// softmax over one axis (tinygrad mixin/op.py:657-700), or with `log` its
// log_softmax: x - max, exp, and the sum's reciprocal or log.  Unlike tinygrad's
// _softmax the max is not detached; its gradient flows through REDUCE_MAX's
// backward, a mask at the argmax.
fn Tensor nn_softmax(Tensor x, i32 axis, int log) {
  u32 ax = (u32)(axis < 0 ? axis + (i32)x.shape.ndim : axis);
  Tensor m = nn_reduce(REDUCE_MAX, x, 1, &ax, 1);
  Tensor shifted = nn_sub(x, m);
  Tensor e = nn_elementwise(NN_EXP, shifted);
  Tensor s = nn_reduce(REDUCE_SUM, e, 1, &ax, 1);
  if (log) {
    return nn_sub(shifted, nn_elementwise(NN_LOG, s));
  }
  return nn_binop(UOP_MUL, e, nn_unary(UOP_RECIP, s));
}
