// The mean negative log-likelihood of log_softmax(x) over `axis` at the int labels
// y (tinygrad mixin/op.py:1723-1746, without ignore_index or smoothing).  The
// one-hot is an in-graph compare against an arange (:1005-1008), so a TinyJit
// replay reads each step's labels.
fn Tensor nn_sparse_categorical_crossentropy(Tensor x, Tensor y, i32 axis) {
  u32 nd = x.shape.ndim;
  u32 ax = (u32)(axis < 0 ? axis + (i32)nd : axis);
  u32 n = x.shape.dims[ax];
  if (y.shape.ndim + 1 != nd || nd > MAX_DIM) {
    x.t = 0;
    return x;
  }
  // y.unsqueeze(ax) == arange(n).reshape(n, 1, ...).
  u32 ys[MAX_DIM], as[MAX_DIM], all[MAX_DIM];
  for (u32 i = 0, j = 0; i < nd; i++) {
    ys[i] = i == ax ? 1 : y.shape.dims[j++];
  }
  Tensor yu = nn_reshape(y, nd, ys);
  as[0] = n;
  for (u32 i = 1; i < nd - ax; i++) {
    as[i] = 1;
  }
  Tensor ar = nn_reshape(nn_arange(0, n, 1, DT_INT32), nd - ax, as);
  Tensor oh = nn_cast(nn_binop(UOP_CMPEQ, yu, ar), DT_FP32);
  Tensor l  = nn_binop(UOP_MUL, nn_softmax(x, (i32)ax, 1), oh);
  l = nn_reduce(REDUCE_SUM, l, 1, &ax, 0);
  for (u32 i = 0; i < l.shape.ndim; i++) {
    all[i] = i;
  }
  return nn_binop_f(UOP_MUL, nn_mean(l, l.shape.ndim, all, 0), -1.0);
}
