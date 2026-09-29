// uop/indexed_shape_ok.c - the extent check the indexed-op constructors
// (uop_gather, uop_scatter) share.
//
// 1 iff every extent of `s` is static and 1 <= numel < 2^31.  Kernel
// addresses are 32-bit signed, an empty gather or scatter is the
// frontend's early return, and a symbolic (kvar) extent has no lowering.

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
