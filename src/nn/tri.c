// tinygrad's triu / tril (mixin/op.py:233-280) over x's last two axes: the 0/1
// mask arange(r)[:, None] + diagonal < arange(c) + 1 selects x (triu) or zero
// (tril, at diagonal + 1); the other elements are 0.
fn Tensor nn_tri(Tensor x, i32 diagonal, int lower) {
  u32 nd = x.shape.ndim;
  if (nd < 2) {
    x.t = 0;
    return x;
  }
  u32 r = x.shape.dims[nd - 2], c = x.shape.dims[nd - 1];
  u32 rs[2] = {r, 1}, cs[2] = {1, c};
  Tensor row = nn_reshape(nn_arange(0, r, 1, DT_INT32), 2, rs);
  Tensor col = nn_reshape(nn_arange(0, c, 1, DT_INT32), 2, cs);
  Tensor rd  = nn_binop(UOP_ADD, row, nn_scalar(row, diagonal + (lower ? 1 : 0), 0));
  Tensor mask = nn_cmplt(rd, nn_binop(UOP_ADD, col, nn_scalar(col, 1, 0)));
  Tensor zero = nn_full(0, x.dtype, x.shape);
  return lower ? nn_where(mask, zero, x) : nn_where(mask, x, zero);
}
