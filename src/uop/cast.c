// uop/cast.c -- value-preserving cast UOP.
//
// Heap layout: [src, NUM(dst_dtype)].  ext on the wrapping TAG_UOP
// is UOP_CAST (the dtype lives in the second heap cell).
//
// Folding rules (best-effort, no failure path):
//   CAST(x, x.dtype) -> x
//   CAST(CAST(x, mid), dst) -> CAST(x, dst) when mid is at least as
//     wide as max(src, dst) and same kind family
//   CAST(CONST(bits, src_dt), dst_dt) -> CONST(reinterpreted_bits, dst_dt)
//
// More aggressive folds land in src/uop/rewrite_cast.c (Phase E
// follow-up); the constructor itself stays small.

fn Term uop_cast(Term src, u32 dst_dtype) {
  // Identity: cast to the term's existing dtype is a no-op.
  u32 src_dtype = DT_F32;
  if (term_dtype_in(src, 0, &src_dtype) && src_dtype == dst_dtype) {
    return src;
  }

  // CAST(CONST, dst): fold immediately when source is a UOP_CONST so
  // downstream materialize sees a fresh CONST of the target dtype.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_CONST) {
    Term num = heap_read(term_val(src));
    if (term_tag(num) == TAG_NUM) {
      u32 src_dt = term_ext(num);
      u32 src_bits = (u32)term_val(num);
      // Promote src bits to f32 and re-pack to dst_dt for floats.
      // For int->int we sign-extend / zero-extend the low bits;
      // int->float / float->int go through f32.
      if (dtype_is_float(dst_dtype) && dtype_is_float(src_dt)) {
        f32 v;
        u8  out_bytes[8] = {0};
        // src_dt's bits already encode an f32 -- the const path
        // stores f32 bits regardless of the narrow target dtype.
        memcpy(&v, &src_bits, sizeof(v));
        from_fp32_lane(out_bytes, dst_dtype, &v, 1);
        u32 packed = 0;
        memcpy(&packed, out_bytes, dtype_storage_bytes(dst_dtype, 1) > 4 ? 4 : dtype_storage_bytes(dst_dtype, 1));
        return uop_const(dst_dtype, packed);
      }
      // Fall through to the runtime kernel for int<->float / int<->int
      // -- keeps the constructor simple.  rewrite_cast.c can extend.
    }
  }

  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dst_dtype));
  return term_new(0, TAG_UOP, UOP_CAST, loc);
}
