// tinygrad's conv2d (mixin/op.py:1564-1605) on 2 spatial dims: x (B, C_in, H, W)
// and w (C_out, cin, kH, kW) padded, unfolded by nn_pool, multiplied as one
// broadcast MUL and summed over (cin, kH, kW), which stay the trailing axes since
// reduce_chain_collect (schedule/uop_meta.c) fuses only contiguous trailing axes.
// b.t == 0 is no bias.  t == 0 for a rank other than 4, groups * cin != C_in or
// an empty window.  The constructor sequence is Tensor.conv2d's
// (py/thvm/tensor.py).
fn Tensor nn_conv2d(Tensor x, Tensor w, Tensor b, u32 groups, u32 const stride[2],
                    u32 const dilation[2], u32 const pad[2]) {
  if (x.shape.ndim != 4 || w.shape.ndim != 4) {
    x.t = 0;
    return x;
  }
  if (pad[0] != 0 || pad[1] != 0) {
    u32 be[8] = {0, 0, 0, 0, pad[0], pad[0], pad[1], pad[1]};
    x = nn_move(UOP_PAD, x, 4, be);
  }
  u32 B = x.shape.dims[0], C_in = x.shape.dims[1];
  u32 C_out = w.shape.dims[0], cin = w.shape.dims[1], kH = w.shape.dims[2], kW = w.shape.dims[3];
  x = nn_pool(x, 2, &w.shape.dims[2], stride, dilation);
  if (x.t == 0 || groups == 0 || groups * cin != C_in) {
    x.t = 0;
    return x;
  }
  u32 Ho = x.shape.dims[2], Wo = x.shape.dims[3];
  Tensor out;
  if (groups == 1) {
    // Both broadcast to (B, C_out, Ho, Wo, C_in, kH, kW).
    u32 perm[6] = {0, 2, 3, 1, 4, 5};
    u32 xr[7]   = {B, 1, Ho, Wo, C_in, kH, kW};
    u32 wr[7]   = {1, C_out, 1, 1, C_in, kH, kW};
    u32 full[7] = {B, C_out, Ho, Wo, C_in, kH, kW};
    u32 red[3]  = {4, 5, 6};
    x   = nn_expand(nn_reshape(nn_permute(x, 6, perm), 7, xr), 7, full);
    w   = nn_expand(nn_reshape(w, 7, wr), 7, full);
    out = nn_reduce(REDUCE_SUM, nn_binop(UOP_MUL, x, w), 3, red);
  } else {
    // C_in split into (groups, cin), C_out into (groups, rcout); both broadcast
    // to (B, groups, rcout, Ho, Wo, cin, kH, kW).
    u32 rcout   = C_out / groups;
    u32 xr[8]   = {B, groups, cin, 1, Ho, Wo, kH, kW};
    u32 xe[8]   = {B, groups, cin, rcout, Ho, Wo, kH, kW};
    u32 perm[8] = {0, 1, 3, 4, 5, 2, 6, 7};
    u32 wr[8]   = {1, groups, rcout, 1, 1, cin, kH, kW};
    u32 full[8] = {B, groups, rcout, Ho, Wo, cin, kH, kW};
    u32 red[3]  = {5, 6, 7};
    u32 o4[4]   = {B, C_out, Ho, Wo};
    x   = nn_permute(nn_expand(nn_reshape(x, 8, xr), 8, xe), 8, perm);
    w   = nn_expand(nn_reshape(w, 8, wr), 8, full);
    out = nn_reshape(nn_reduce(REDUCE_SUM, nn_binop(UOP_MUL, x, w), 3, red), 4, o4);
  }
  if (b.t != 0) {
    u32 b4[4] = {1, shape_numel(b.shape), 1, 1};
    out = nn_binop(UOP_ADD, out, nn_reshape(b, 4, b4));
  }
  return out;
}
