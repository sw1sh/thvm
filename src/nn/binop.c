// Tensor._binop: broadcast a and b to one shape (uop_broadcast2), compute a
// mixed pair in f32 (least_upper_dtype on the f32 / i32 lattice, tinygrad
// mixin/elementwise.py:24-34), then the binary op in a's dtype.
fn Tensor nn_binop(u32 op, Tensor a, Tensor b) {
  Tensor r = a;
  if (a.shape.ndim != b.shape.ndim
      || memcmp(a.shape.dims, b.shape.dims, a.shape.ndim * sizeof a.shape.dims[0]) != 0) {
    int nd = uop_broadcast2(&a.t, a.shape.ndim, a.shape.dims, &b.t, b.shape.ndim, b.shape.dims,
                            r.shape.dims);
    if (nd < 0) {
      r.t = 0;
      return r;
    }
    r.shape.ndim = (u32)nd;
  }
  if (a.dtype != b.dtype) {
    a = nn_cast(a, DT_FP32);
    b = nn_cast(b, DT_FP32);
  }
  r.dtype = a.dtype;
  r.t     = uop_binary(op, a.t, b.t);
  return r;
}
