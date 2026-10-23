// Tensor._reduce without keepdim: a REDUCE_* over the n axes (distinct, in
// range), which drops them from the shape.
fn Tensor nn_reduce(u32 kind, Tensor x, u32 n, u32 const *axes) {
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
  return r;
}
