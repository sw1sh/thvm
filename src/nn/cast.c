// Tensor.cast: x itself at its own dtype.
fn Tensor nn_cast(Tensor x, u32 dtype) {
  if (x.dtype == dtype) {
    return x;
  }
  x.t     = uop_cast(x.t, dtype);
  x.dtype = dtype;
  return x;
}
