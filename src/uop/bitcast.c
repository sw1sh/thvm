// uop/bitcast.c -- bit-level reinterpret cast.
//
// Heap layout: [src, NUM(dst_dtype)].  Source and destination must
// share itemsize.  Mirrors tinygrad's Ops.BITCAST -- the runtime
// kernel is a memcpy with a width sanity check.
//
// Backward gradient: zero of src.dtype (matches tinygrad/gradient.py:42
// where BITCAST returns None).
//
// Folding rules:
//   BITCAST(x, x.dtype) -> x
//   BITCAST(BITCAST(x, mid), dst) -> BITCAST(x, dst) when itemsizes match
//   BITCAST(CONST(bits, src), dst) -> CONST(bits, dst) when itemsize matches

fn Term uop_bitcast(Term src, u32 dst_dtype) {
  // Identity.
  u32 src_dtype = DT_F32;
  if (term_dtype_in(src, 0, &src_dtype) && src_dtype == dst_dtype) {
    return src;
  }

  // Width sanity: refuse to construct a BITCAST whose itemsizes
  // differ.  The caller should explicitly route through CAST first.
  // dtype_itemsize aborts on packed nibble dtypes; gate on that
  // so the assertion only fires at materialize time, not here.
  if (!dtype_is_packed(src_dtype) && !dtype_is_packed(dst_dtype)
      && dtype_itemsize(src_dtype) != dtype_itemsize(dst_dtype)) {
    fprintf(stderr, "uop_bitcast: itemsize mismatch: src=%s (%u B) dst=%s (%u B)\n",
            dtype_name(src_dtype), dtype_itemsize(src_dtype),
            dtype_name(dst_dtype), dtype_itemsize(dst_dtype));
    return src;   // best-effort: return the src so callers see no-op
                  // instead of a half-built UOP_BITCAST.
  }

  // BITCAST(BITCAST(x, mid), dst) -> BITCAST(x, dst).  Always safe
  // because bitcast is bit-preserving.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_BITCAST) {
    Term inner = heap_read(term_val(src));
    return uop_bitcast(inner, dst_dtype);
  }

  // BITCAST(CONST, dst): fold to fresh CONST with the same bits.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_CONST) {
    Term num = heap_read(term_val(src));
    if (term_tag(num) == TAG_NUM) {
      u32 bits = (u32)term_val(num);
      return uop_const(dst_dtype, bits);
    }
  }

  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dst_dtype));
  return term_new(0, TAG_UOP, UOP_BITCAST, loc);
}
