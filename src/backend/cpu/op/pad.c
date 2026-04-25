// backend/cpu/op/pad.c - axis-aware zero-pad.
//
// Output shape: out_dim[i] = src_dim[i] + pad_widths[2i] +
// pad_widths[2i+1] (begin / end widths interleaved).
// Indexing: each output coord c is either
//   c < begin                         -> 0 (in begin pad)
//   begin <= c < begin + src_dim      -> src[c - begin]
//   c >= begin + src_dim              -> 0 (in end pad)
// per axis.  When ANY axis is in its pad region, the output element
// is 0; otherwise it's a copy of the corresponding source element.
//
// Falls back to memcpy if shape info missing (defensive; shouldn't
// happen given the materializer always populates src0_dims for PAD).

fn void cpu_op_pad(void *out, void **srcs, u32 const *src_numels,
                   KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  void *src    = srcs[0];
  u8    ndim   = p->src0_ndim;
  u32   esz    = (p->dtype == DT_F32) ? sizeof(f32) : sizeof(i32);

  if (ndim == 0) {
    memcpy(out, src, (size_t)out_numel * esz);
    return;
  }

  // Zero-init the whole output buffer; we'll overwrite the
  // copied-from-source positions below.
  memset(out, 0, (size_t)out_numel * esz);

  if (p->dtype == DT_F32) {
    f32 *dst = (f32 *)out;
    f32 *s   = (f32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      // Decompose oi into per-axis output coords; check each
      // against its begin/end pad region; build the source flat
      // index if all axes are inside the source extent.
      u32 tmp = oi;
      u32 src_idx = 0;
      u32 src_stride = 1;
      u8  in_pad = 0;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 od = p->out_dims[axis];
        u32 sd = p->src0_dims[axis];
        u32 b  = p->pad_widths[2 * (u32)axis];
        u32 c  = tmp % od;
        tmp   /= od;
        if (c < b || c >= b + sd) { in_pad = 1; break; }
        src_idx += (c - b) * src_stride;
        src_stride *= sd;
      }
      if (!in_pad) dst[oi] = s[src_idx];
    }
  } else {
    i32 *dst = (i32 *)out;
    i32 *s   = (i32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      u32 tmp = oi;
      u32 src_idx = 0;
      u32 src_stride = 1;
      u8  in_pad = 0;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 od = p->out_dims[axis];
        u32 sd = p->src0_dims[axis];
        u32 b  = p->pad_widths[2 * (u32)axis];
        u32 c  = tmp % od;
        tmp   /= od;
        if (c < b || c >= b + sd) { in_pad = 1; break; }
        src_idx += (c - b) * src_stride;
        src_stride *= sd;
      }
      if (!in_pad) dst[oi] = s[src_idx];
    }
  }
}
