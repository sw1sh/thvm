// dtype/lane.c -- promote / demote primitives between any wired
// dtype and the f32 ALU buffer used by every elementwise kernel.
//
// to_fp32_lane(dst, src, dt, n) reads `n` `dt` elements from `src`
// and writes `n` f32s to `dst`.  from_fp32_lane(dst, dt, src, n)
// is the inverse.  f32 / f64 paths use direct casts; f16 / bf16 /
// fp8 go through the fp_convert / fp8 routines.  Integer dtypes are
// also covered (sint -> sitofp, uint -> uitofp) so a single helper
// handles the cross-family route.
//
// Used by:
//   - cpu_op_run_via_f32 (elementwise promote-to-f32 ALU for f16,
//     bf16, fp8 -- they don't have native ALU paths in interpret.c)
//   - cpu_op_cast (Phase E) for value-preserving CAST
//   - rewrite_cast.c constant-folder for CAST(CONST, dt)

void to_fp32_lane(f32 *dst, void const *src, u32 dt, u32 n) {
  switch (dt) {
    case DT_FP32:   memcpy(dst, src, (size_t)n * sizeof(f32)); break;
    case DT_FP64:   { f64 const *s = (f64 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_FP16:   { u16 const *s = (u16 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = fp16_to_f32(s[i]); break; }
    case DT_BF16:   { u16 const *s = (u16 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = bf16_to_f32(s[i]); break; }
    case DT_FP8E4M3:{ u8  const *s = (u8  const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = fp8e4m3_to_f32(s[i]); break; }
    case DT_FP8E5M2:{ u8  const *s = (u8  const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = fp8e5m2_to_f32(s[i]); break; }
    case DT_BOOL:   { u8  const *s = (u8  const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (s[i] & 1) ? 1.0f : 0.0f; break; }
    case DT_INT8:   { i8  const *s = (i8  const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_UINT8:  { u8  const *s = (u8  const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_INT16:  { i16 const *s = (i16 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_UINT16: { u16 const *s = (u16 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_INT32:  { i32 const *s = (i32 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_UINT32: { u32 const *s = (u32 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_INT64:  { i64 const *s = (i64 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    case DT_UINT64: { u64 const *s = (u64 const *)src;
                      for (u32 i = 0; i < n; i++) dst[i] = (f32)s[i]; break; }
    default:
      fprintf(stderr, "to_fp32_lane: dtype %u not yet wired\n", dt);
      abort();
  }
}

void from_fp32_lane(void *dst, u32 dt, f32 const *src, u32 n) {
  switch (dt) {
    case DT_FP32:   memcpy(dst, src, (size_t)n * sizeof(f32)); break;
    case DT_FP64:   { f64 *d = (f64 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (f64)src[i]; break; }
    case DT_FP16:   { u16 *d = (u16 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = f32_to_fp16(src[i]); break; }
    case DT_BF16:   { u16 *d = (u16 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = f32_to_bf16(src[i]); break; }
    case DT_FP8E4M3:{ u8  *d = (u8  *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = f32_to_fp8e4m3(src[i]); break; }
    case DT_FP8E5M2:{ u8  *d = (u8  *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = f32_to_fp8e5m2(src[i]); break; }
    case DT_BOOL:   { u8  *d = (u8  *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (src[i] != 0.0f) ? 1u : 0u; break; }
    case DT_INT8:   { i8  *d = (i8  *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (i8 )src[i]; break; }
    case DT_UINT8:  { u8  *d = (u8  *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (u8 )src[i]; break; }
    case DT_INT16:  { i16 *d = (i16 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (i16)src[i]; break; }
    case DT_UINT16: { u16 *d = (u16 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (u16)src[i]; break; }
    case DT_INT32:  { i32 *d = (i32 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (i32)src[i]; break; }
    case DT_UINT32: { u32 *d = (u32 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (u32)src[i]; break; }
    case DT_INT64:  { i64 *d = (i64 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (i64)src[i]; break; }
    case DT_UINT64: { u64 *d = (u64 *)dst;
                      for (u32 i = 0; i < n; i++) d[i] = (u64)src[i]; break; }
    default:
      fprintf(stderr, "from_fp32_lane: dtype %u not yet wired\n", dt);
      abort();
  }
}
