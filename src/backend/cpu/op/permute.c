// backend/cpu/op/permute.c - axis-aware reorder.
//
// Output shape: out_dims[i] = src0_dims[axis_perm[i]].  Per output
// index, decompose into per-axis output coords; for each axis i,
// the corresponding source coord is the same numerical value but
// at SOURCE axis axis_perm[i].  Build the source flat index using
// row-major source strides.
//
// Falls back to memcpy if shape info missing OR perm is identity
// (defensive; identity perm should be rare since the constructor
// is typically invoked precisely to reorder).

fn void cpu_op_permute(void *out, void **srcs, u32 const *src_numels,
                       KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  void *src   = srcs[0];
  u8    ndim  = p->src0_ndim;
  u32   esz   = (p->dtype == DT_F32) ? sizeof(f32) : sizeof(i32);

  if (ndim == 0) {
    memcpy(out, src, (size_t)out_numel * esz);
    return;
  }

  // Precompute source strides (row-major over src0_dims).
  u32 src_stride[MAX_DIM] = {0};
  src_stride[ndim - 1] = 1;
  for (i32 axis = (i32)ndim - 2; axis >= 0; axis--) {
    src_stride[axis] = src_stride[axis + 1] * p->src0_dims[axis + 1];
  }

  if (p->dtype == DT_F32) {
    f32 *dst = (f32 *)out;
    f32 *s   = (f32 *)src;
    for (u32 oi = 0; oi < out_numel; oi++) {
      // Decompose oi into per-axis OUTPUT coords (row-major over
      // out_dims), then map each output axis i to source axis
      // axis_perm[i] and accumulate src_idx via that source stride.
      u32 tmp = oi;
      u32 src_idx = 0;
      for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
        u32 od = p->out_dims[axis];
        u32 c  = tmp % od;
        tmp   /= od;
        src_idx += c * src_stride[p->axis_perm[axis]];
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
        u32 c  = tmp % od;
        tmp   /= od;
        src_idx += c * src_stride[p->axis_perm[axis]];
      }
      dst[oi] = s[src_idx];
    }
  }
}
