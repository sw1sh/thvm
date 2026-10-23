// max pool over the last nk axes without padding (tinygrad mixin/op.py:1335-1378):
// the _pool unfold, then a max over its window axes.  Non-overlapping windows that
// tile the input are a reshape into (o, k) per axis instead, which fuses into the
// max, where the unfold's repeat cannot stay a view and materializes a large buffer.
fn Tensor nn_max_pool2d(Tensor x, u32 nk, u32 const *k, u32 const *s, u32 const *d) {
  u32 nd = x.shape.ndim;
  if (nk == 0 || nk > nd || nd + nk > MAX_DIM) {
    x.t = 0;
    return x;
  }
  u32 base = nd - nk, tiles = 1;
  for (u32 j = 0; j < nk; j++) {
    tiles &= s[j] == k[j] && d[j] == 1 && k[j] != 0 && x.shape.dims[base + j] % k[j] == 0;
  }
  u32 axes[MAX_DIM];
  if (tiles) {
    u32 split[MAX_DIM];
    memcpy(split, x.shape.dims, base * sizeof split[0]);
    for (u32 j = 0; j < nk; j++) {
      split[base + 2 * j]     = x.shape.dims[base + j] / k[j];
      split[base + 2 * j + 1] = k[j];
      axes[j] = base + 2 * j + 1;
    }
    return nn_reduce(REDUCE_MAX, nn_reshape(x, nd + nk, split), nk, axes, 0);
  }
  Tensor p = nn_pool(x, nk, k, s, d);
  if (p.t == 0) {
    return p;
  }
  for (u32 j = 0; j < nk; j++) {
    axes[j] = p.shape.ndim - nk + j;
  }
  return nn_reduce(REDUCE_MAX, p, nk, axes, 0);
}
