// tinygrad's conv2d (mixin/op.py:1564-1605) over the n = w.ndim - 2 spatial axes:
// x (B, C_in, *in) and w (C_out, cin, *k), x padded by pad[2n] ((begin, end) per
// spatial axis, a negative side shrinking as tinygrad's _pad_constant does),
// unfolded by nn_pool, multiplied as one broadcast MUL and summed over (cin, *k),
// which stay the trailing axes since reduce_chain_collect (schedule/uop_meta.c)
// fuses only contiguous trailing axes.  groups == 1 takes Tensor.conv2d's
// (B, C_out, *o, C_in, *k) layout, other groups tinygrad's
// (B, groups, rcout, *o, cin, *k), so n is at most 2 within MAX_DIM.  b.t == 0 is
// no bias.  t == 0 for mismatched ranks, groups * cin != C_in, an empty window or
// too many axes.
fn Tensor nn_conv2d(Tensor x, Tensor w, Tensor b, u32 groups, u32 const *stride,
                    u32 const *dilation, i32 const *pad) {
  Tensor fail = x;
  fail.t = 0;
  u32 nd = w.shape.ndim, n = nd - 2;
  if (nd < 3 || x.shape.ndim != nd || groups == 0 || 4 + 2 * n > MAX_DIM) {
    return fail;
  }
  int padded = 0, shrunk = 0;
  for (u32 i = 0; i < 2 * n; i++) {
    padded |= pad[i] > 0;
    shrunk |= pad[i] < 0;
  }
  if (shrunk) {
    u32 be[2 * MAX_DIM];
    for (u32 a = 0; a < nd; a++) {
      i32 lo = a < 2 ? 0 : pad[2 * (a - 2)], hi = a < 2 ? 0 : pad[2 * (a - 2) + 1];
      be[2 * a]     = lo < 0 ? (u32)-lo : 0;
      be[2 * a + 1] = x.shape.dims[a] - (hi < 0 ? (u32)-hi : 0);
    }
    x = nn_move(UOP_SHRINK, x, nd, be);
  }
  if (padded) {
    u32 be[2 * MAX_DIM] = {0};
    for (u32 j = 0; j < 2 * n; j++) {
      be[4 + j] = pad[j] > 0 ? (u32)pad[j] : 0;
    }
    x = nn_move(UOP_PAD, x, nd, be);
  }
  u32 B = x.shape.dims[0], C_in = x.shape.dims[1], C_out = w.shape.dims[0], cin = w.shape.dims[1];
  u32 k[MAX_DIM], o[MAX_DIM];
  memcpy(k, &w.shape.dims[2], n * sizeof k[0]);
  x = nn_pool(x, n, k, stride, dilation);
  if (x.t == 0 || groups * cin != C_in) {
    return fail;
  }
  memcpy(o, &x.shape.dims[2], n * sizeof o[0]);
  u32 xr[MAX_DIM], xe[MAX_DIM], wr[MAX_DIM], full[MAX_DIM], perm[MAX_DIM], red[MAX_DIM];
  Tensor out;
  if (groups == 1) {
    // x (B, C_in, *o, *k) -> (B, *o, C_in, *k) -> (B, 1, *o, C_in, *k); w
    // (C_out, C_in, *k) -> (1, C_out, 1 * n, C_in, *k); both broadcast to
    // (B, C_out, *o, C_in, *k).
    u32 m = 3 + 2 * n;
    perm[0] = 0;
    perm[1 + n] = 1;
    for (u32 j = 0; j < n; j++) {
      perm[1 + j]     = 2 + j;
      perm[2 + n + j] = 2 + n + j;
    }
    u32 const head_x[2] = {B, 1}, head_w[2] = {1, C_out}, head_f[2] = {B, C_out};
    for (u32 a = 0; a < 2; a++) {
      xr[a] = head_x[a], wr[a] = head_w[a], full[a] = head_f[a];
    }
    for (u32 j = 0; j < n; j++) {
      xr[2 + j] = o[j], wr[2 + j] = 1, full[2 + j] = o[j];
      xr[3 + n + j] = wr[3 + n + j] = full[3 + n + j] = k[j];
    }
    xr[2 + n] = wr[2 + n] = full[2 + n] = C_in;
    for (u32 j = 0; j <= n; j++) {
      red[j] = 2 + n + j;
    }
    x   = nn_expand(nn_reshape(nn_permute(x, 2 + 2 * n, perm), m, xr), m, full);
    w   = nn_expand(nn_reshape(w, m, wr), m, full);
    out = nn_reduce(REDUCE_SUM, nn_binop(UOP_MUL, x, w), n + 1, red, 0);
  } else {
    // C_in split into (groups, cin), C_out into (groups, rcout):
    // x (B, groups, cin, 1, *o, *k) expanded to rcout, cin moved behind *o; w
    // (1, groups, rcout, 1 * n, cin, *k); both broadcast to
    // (B, groups, rcout, *o, cin, *k).
    u32 m = 4 + 2 * n, rcout = C_out / groups;
    u32 const hx[4] = {B, groups, cin, 1}, hxe[4] = {B, groups, cin, rcout};
    u32 const hw[3] = {1, groups, rcout}, hf[3] = {B, groups, rcout};
    for (u32 a = 0; a < 4; a++) {
      xr[a] = hx[a], xe[a] = hxe[a];
    }
    for (u32 a = 0; a < 3; a++) {
      wr[a] = hw[a], full[a] = hf[a];
    }
    for (u32 j = 0; j < n; j++) {
      xr[4 + j] = xe[4 + j] = o[j];
      xr[4 + n + j] = xe[4 + n + j] = k[j];
      wr[3 + j] = 1, full[3 + j] = o[j];
      wr[4 + n + j] = full[4 + n + j] = k[j];
    }
    wr[3 + n] = full[3 + n] = cin;
    perm[0] = 0, perm[1] = 1, perm[2] = 3, perm[3 + n] = 2;
    for (u32 j = 0; j < n; j++) {
      perm[3 + j]     = 4 + j;
      perm[4 + n + j] = 4 + n + j;
    }
    for (u32 j = 0; j <= n; j++) {
      red[j] = 3 + n + j;
    }
    u32 on[MAX_DIM] = {B, C_out};
    memcpy(&on[2], o, n * sizeof on[0]);
    x   = nn_permute(nn_expand(nn_reshape(x, m, xr), m, xe), m, perm);
    w   = nn_expand(nn_reshape(w, m, wr), m, full);
    out = nn_reduce(REDUCE_SUM, nn_binop(UOP_MUL, x, w), n + 1, red, 0);
    out = nn_reshape(out, 2 + n, on);
  }
  if (b.t != 0) {
    u32 bs[MAX_DIM] = {1, shape_numel(b.shape), 1, 1, 1, 1, 1, 1};
    out = nn_binop(UOP_ADD, out, nn_reshape(b, 2 + n, bs));
  }
  return out;
}
