// Tensor._reduce: a REDUCE_* over the n axes (distinct, in range), which drops
// them from the shape, or with keepdim reshapes them back as 1s.
fn Tensor nn_reduce(u32 kind, Tensor x, u32 n, u32 const *axes, int keepdim) {
  u8 drop[MAX_DIM] = {0};
  for (u32 i = 0; i < n; i++) {
    drop[axes[i]] = 1;
  }
  Tensor r = x;
  r.shape.ndim = 0;
  for (u32 i = 0; i < x.shape.ndim; i++) {
    if (!drop[i]) {
      r.shape.dims[r.shape.ndim++] = x.shape.dims[i];
    }
  }
  r.t = uop_reduce_axes(kind, n, axes, x.t);
  if (!keepdim) {
    return r;
  }
  u32 kept[MAX_DIM];
  for (u32 i = 0; i < x.shape.ndim; i++) {
    kept[i] = drop[i] ? 1 : x.shape.dims[i];
  }
  return nn_reshape(r, x.shape.ndim, kept);
}

// Tensor.mean: the sum times 1 / (the reduced count); nan over an empty axis,
// tinygrad's 0 / 0.
fn Tensor nn_mean(Tensor x, u32 n, u32 const *axes, int keepdim) {
  u64 count = 1;
  for (u32 i = 0; i < n; i++) {
    count *= x.shape.dims[axes[i]];
  }
  Tensor s = nn_reduce(REDUCE_SUM, x, n, axes, keepdim);
  return nn_binop_f(UOP_MUL, s, count != 0 ? 1.0 / (double)count : NAN);
}
