// tinygrad's conv_transpose2d (mixin/op.py:1607-1645) over the n = w.ndim - 2
// spatial axes: w (C_in, C_out / groups, *k) regrouped to (C_out, C_in / groups,
// *k) and flipped, x dilated by the stride (each element followed by s - 1
// zeros), then a stride-1 nn_conv2d padded (k - 1) d - begin before and
// (k - 1) d - end + output_padding after each axis.  pad[2n] is (begin, end) per
// spatial axis.
fn Tensor nn_conv_transpose2d(Tensor x, Tensor w, Tensor b, u32 groups, u32 const *stride,
                              u32 const *dilation, i32 const *pad, u32 const *output_padding) {
  u32 nd = w.shape.ndim, n = nd - 2;
  if (nd < 3 || x.shape.ndim != nd || groups == 0 || w.shape.dims[0] % groups != 0
      || nd + 1 > MAX_DIM || 2 + 2 * n > MAX_DIM) {
    x.t = 0;
    return x;
  }
  u32 k[MAX_DIM];
  memcpy(k, &w.shape.dims[2], n * sizeof k[0]);
  // w.unflatten(0, (groups, -1)).transpose(1, 2).flip(3, ...).
  u32 wu[MAX_DIM] = {groups, w.shape.dims[0] / groups}, perm[MAX_DIM], flip = 0;
  memcpy(&wu[2], &w.shape.dims[1], (n + 1) * sizeof wu[0]);
  for (u32 a = 0; a < nd + 1; a++) {
    perm[a] = a;
  }
  perm[1] = 2;
  perm[2] = 1;
  for (u32 j = 0; j < n; j++) {
    flip |= 1u << (3 + j);
  }
  w = nn_move(UOP_FLIP, nn_permute(nn_reshape(w, nd + 1, wu), nd + 1, perm), 1, &flip);
  int strided = 0;
  for (u32 j = 0; j < n; j++) {
    strided |= stride[j] > 1;
  }
  if (strided) {
    // (k) -> (k, 1) -> pad (k, s) -> (k * s) -> shrink (k * s - (s - 1)).
    u32 xs[MAX_DIM] = {x.shape.dims[0], x.shape.dims[1]}, be[2 * MAX_DIM] = {0};
    u32 flat[MAX_DIM] = {x.shape.dims[0], x.shape.dims[1]}, cut[MAX_DIM];
    for (u32 j = 0; j < n; j++) {
      u32 kj = x.shape.dims[2 + j];
      xs[2 + 2 * j] = kj;
      xs[3 + 2 * j] = 1;
      be[2 * (3 + 2 * j) + 1] = stride[j] - 1;
      flat[2 + j] = kj * stride[j];
      cut[2 + j] = kj * stride[j] - (stride[j] - 1);
    }
    cut[0] = flat[0], cut[1] = flat[1];
    x = nn_move(UOP_PAD, nn_reshape(x, 2 + 2 * n, xs), 2 + 2 * n, be);
    x = nn_shrink_to(nn_reshape(x, nd, flat), cut);
  }
  i32 cpad[2 * MAX_DIM];
  u32 ones[MAX_DIM];
  for (u32 j = 0; j < n; j++) {
    i32 span = (i32)((k[j] - 1) * dilation[j]);
    cpad[2 * j]     = span - pad[2 * j];
    cpad[2 * j + 1] = span - pad[2 * j + 1] + (i32)output_padding[j];
    ones[j] = 1;
  }
  u32 wf[MAX_DIM] = {wu[0] * wu[2], wu[1]};
  memcpy(&wf[2], k, n * sizeof wf[0]);
  return nn_conv2d(x, nn_reshape(w, nd, wf), b, groups, ones, dilation, cpad);
}
