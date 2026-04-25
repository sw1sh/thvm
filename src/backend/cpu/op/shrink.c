// backend/cpu/op/shrink.c - axis-aware sub-region extract.
//
// Inverse of cpu_op_pad: keep slice [b_i, e_i) on each axis.
// Output shape: out_dim[i] = e_i - b_i.  Per output index, the
// source coord on axis i is (output_coord_i + b_i).
//
// `KProgOp.pad_widths` is reused as `shrink_widths`: bytes
// (b_0, e_0, b_1, e_1, ...) interleaved.  No PAD/SHRINK overlap
// per op so the same storage is unambiguous.
//
// Falls back to memcpy if shape info missing (defensive).

fn void cpu_op_shrink(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  void *src    = srcs[0];
  u8    ndim   = p->src0_ndim;
  u32   esz    = (p->dtype == DT_F32) ? sizeof(f32) : sizeof(i32);

  if (ndim == 0) {
    memcpy(out, src, (size_t)out_numel * esz);
    return;
  }

  // Source row-major strides.
  u32 src_stride[MAX_DIM] = {0};
  src_stride[ndim - 1] = 1;
  for (i32 axis = (i32)ndim - 2; axis >= 0; axis--) {
    src_stride[axis] = src_stride[axis + 1] * p->src0_dims[axis + 1];
  }

  if (p->dtype == DT_F32) {
    f32 *dst = (f32 *)out;
    f32 *s   = (f32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      u32 tmp = oi;
      u32 src_idx = 0;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 od = p->out_dims[axis];
        u32 b  = p->pad_widths[2 * (u32)axis];
        u32 c  = tmp % od;
        tmp   /= od;
        src_idx += (c + b) * src_stride[axis];
      }
      dst[oi] = s[src_idx];
    }
  } else {
    i32 *dst = (i32 *)out;
    i32 *s   = (i32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      u32 tmp = oi;
      u32 src_idx = 0;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 od = p->out_dims[axis];
        u32 b  = p->pad_widths[2 * (u32)axis];
        u32 c  = tmp % od;
        tmp   /= od;
        src_idx += (c + b) * src_stride[axis];
      }
      dst[oi] = s[src_idx];
    }
  }
}
