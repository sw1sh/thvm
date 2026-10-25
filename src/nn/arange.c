// Tensor.arange as tinygrad builds it (mixin/op.py:189-218): the running sum
// of a lazy full of step plus start - step, cast to dtype, a sum codegen folds
// to index math (uop/reduce_collapse.c).  _pool unfolds a window of n by
// repeating its input about n times, an index past int32 once n passes 2^15,
// so a longer ramp takes its counts as q * 2^15 + r from two short ramps of
// ones, times step: the folded value.  start and step fit int32 (an int CONST
// holds 32 bits; Tensor.arange takes wider ones from numpy).
#define NN_RAMP_MAX (1u << 15)

// v, 2v, ..., nv in dtype dt: _cumalu's zero pad, pool and sum over a full of v.
static Tensor nn_ramp(u32 n, u32 dt, double v) {
  Shape s = {1, {n}};
  u32 pad[2] = {n - 1, 0};
  u32 one = 1, axis = 1;
  Tensor x = nn_move(UOP_PAD, nn_full(v, dt, s), 1, pad);
  return nn_reduce(REDUCE_SUM, nn_pool(x, 1, &n, &one, &one), 1, &axis, 0);
}

fn Tensor nn_arange(double start, double stop, double step, u32 dtype) {
  // ceil((stop - start) / step) without libm, which libthvm_py does not link.
  double len = step != 0 ? (stop - start) / step : 0;
  Tensor r = {0, dtype, {1, {0}}};
  if (len > (double)UINT32_MAX) {
    return r;
  }
  u32 n = len > 0 ? (u32)len : 0;
  n += len > 0 && (double)n < len;
  if (n == 0) {
    r.t = term_new(0, TAG_TEN, dtype, tensor_alloc(CURRENT_BACKEND, r.shape, dtype));
    return r;
  }
  // An int ramp runs in int32, or int64 for a 64-bit one or one past 2^31.
  int is_float = dtype_is_float(dtype);
  u32 idt = n > 2147483647u || dtype == DT_INT64 || dtype == DT_UINT64 ? DT_INT64 : DT_INT32;
  u32 acc = is_float ? nn_promote(dtype, DT_FP32) : idt;
  if (n <= NN_RAMP_MAX) {
    r = nn_ramp(n, acc, step);
  } else {
    u32 q = (n + NN_RAMP_MAX - 1) / NN_RAMP_MAX, flat = q * NN_RAMP_MAX;
    u32 qs[2] = {q, 1}, rs[2] = {1, NN_RAMP_MAX}, keep[2] = {0, n};
    Tensor qr = nn_ramp(q, idt, 1);
    Tensor hi = nn_binop(UOP_MUL, nn_sub(qr, nn_scalar(qr, 1, 0)), nn_scalar(qr, NN_RAMP_MAX, 0));
    r = nn_binop(UOP_ADD, nn_reshape(hi, 2, qs), nn_reshape(nn_ramp(NN_RAMP_MAX, idt, 1), 2, rs));
    r = nn_move(UOP_SHRINK, nn_reshape(r, 1, &flat), 1, keep);
    r = nn_binop(UOP_MUL, nn_cast(r, acc), nn_scalar(nn_cast(r, acc), step, is_float));
  }
  r = nn_binop(UOP_ADD, r, nn_scalar(r, start - step, is_float));
  return nn_cast(r, dtype);
}
