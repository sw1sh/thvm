// A lazy constant of `shape`: EXPAND(RESHAPE(CONST)) with no buffer, tinygrad's
// const_like (uop/ops.py:603-605).  A float dtype keeps v's f32 bits, an int
// dtype the low 32 bits of its truncation, through 64 bits so 0xFFFFFFFF stays
// itself, where (i32) of it overflows to INT32_MIN.
fn Tensor nn_full(double v, u32 dtype, Shape shape) {
  u32 bits;
  if (dtype_is_float(dtype)) {
    f32 f = (f32)v;
    memcpy(&bits, &f, sizeof bits);
  } else {
    bits = (v >= 0) ? (u32)(u64)v : (u32)(i64)v;
  }
  u32 ones[MAX_DIM] = {0};
  for (u32 i = 0; i < MAX_DIM; i++) {
    ones[i] = 1;
  }
  Tensor c = {uop_const(dtype, bits), dtype, {0, {0}}};
  return nn_expand(nn_reshape(c, shape.ndim, ones), shape.ndim, shape.dims);
}

// A Python scalar beside ref, as a weak rank-0 CONST (tinygrad
// mixin/elementwise.py:21-34): ref's dtype, except that a float scalar makes an
// int operation f32.
fn Tensor nn_scalar(Tensor ref, double v, int is_float) {
  u32 dtype = is_float && !dtype_is_float(ref.dtype) ? DT_FP32 : ref.dtype;
  Shape s0 = {0, {0}};
  return nn_full(v, dtype, s0);
}

// x op v for a float scalar v; every scalar-on-the-left form (1.0 + x, 2.0 * x)
// builds the same graph with x first.
fn Tensor nn_binop_f(u32 op, Tensor x, double v) {
  return nn_binop(op, x, nn_scalar(x, v, 1));
}
