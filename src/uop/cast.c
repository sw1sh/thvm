// Heap layout: [src, NUM(dst_dtype)].  Float to float, a CONST folds as
// tinygrad's root.const_like(c.val) (uop/symbolic.py:151-155): the payload
// keeps v's f32 bits, which uop_const truncates to dst_dtype.

fn Term uop_cast(Term src, u32 dst_dtype) {
  u32 src_dtype = DT_FP32;
  if (term_dtype_in(src, 0, &src_dtype) && src_dtype == dst_dtype) {
    return src;
  }

  // A CAST of a CONST to or from an int dtype stays a node; the kernel
  // converts it.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_CONST) {
    Term num = heap_read(term_val(src));
    if (term_tag(num) == TAG_NUM
        && dtype_is_float(term_ext(num)) && dtype_is_float(dst_dtype)) {
      return uop_const(dst_dtype, (u32)term_val(num));
    }
  }

  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dst_dtype));
  return term_new(0, TAG_UOP, UOP_CAST, loc);
}
