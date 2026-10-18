// 1 iff every extent of `s` is static and 1 <= numel < 2^31: kernel addresses
// are 32-bit signed, the frontend returns early on an empty gather or scatter,
// and a kvar extent has no lowering.  uop_gather and uop_scatter share it.
fn int uop_indexed_shape_ok(Shape const *s) {
  u64 numel = 1;
  for (u32 d = 0; d < s->ndim; d++) {
    if (kvar_extent_is_var(s->dims[d])) {
      return 0;
    }
    numel *= s->dims[d];
    if (numel >= (1ull << 31)) {
      return 0;
    }
  }
  return numel >= 1;
}
