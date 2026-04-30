// backend/cpu/op/flip.c - axis-aware mirror of selected axes.
//
// `KProgOp.arg` holds the axes_bitmask (bit i set iff axis i is
// flipped).  `KProgOp.src0_dims` carries the source's per-axis
// shape so we can decompose each output index, mirror the
// flipped axes, and reassemble the source flat index.
//
// Output shape == input shape (FLIP doesn't change rank or sizes,
// only data ordering), so out_dims == src0_dims at this point.

fn void cpu_op_flip(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  void *src       = srcs[0];
  u32   axes_mask = p->arg;
  u8    ndim      = p->src0_ndim;

  // Fall back to memcpy if no axes are flipped or if shape info is
  // missing -- both are correct (FLIP with zero mask is a no-op).
  if (axes_mask == 0u || ndim == 0) {
    u32 esz = dtype_itemsize(p->dtype);
    memcpy(out, src, (size_t)out_numel * esz);
    return;
  }

  if (p->dtype == DT_F32) {
    f32 *dst = (f32 *)out;
    f32 *s   = (f32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      // Decompose oi into per-axis coords (row-major), mirror the
      // flipped axes, then re-encode into the source flat index.
      u32 tmp = oi;
      u32 src_idx = 0;
      u32 stride  = 1;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 d = p->src0_dims[axis];
        u32 c = tmp % d;
        tmp  /= d;
        if (axes_mask & (1u << (u32)axis)) c = d - 1u - c;
        src_idx += c * stride;
        stride  *= d;
      }
      dst[oi] = s[src_idx];
    }
  } else {
    i32 *dst = (i32 *)out;
    i32 *s   = (i32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      u32 tmp = oi;
      u32 src_idx = 0;
      u32 stride  = 1;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 d = p->src0_dims[axis];
        u32 c = tmp % d;
        tmp  /= d;
        if (axes_mask & (1u << (u32)axis)) c = d - 1u - c;
        src_idx += c * stride;
        stride  *= d;
      }
      dst[oi] = s[src_idx];
    }
  }
}
