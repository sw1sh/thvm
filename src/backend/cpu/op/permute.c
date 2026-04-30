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
//
// Phase B: width-driven (1/2/4/8 bytes); per-dtype interpretation
// happens in the caller (the gather only moves bytes).

#define PERMUTE_GATHER(T)                                                    \
    do {                                                                     \
        T *dst = (T *)out;                                                   \
        T *s   = (T *)src;                                                   \
        for (u32 oi = 0; oi < out_numel; oi++) {                             \
            u32 tmp = oi;                                                    \
            u32 src_idx = 0;                                                 \
            for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {              \
                u32 od = p->out_dims[axis];                                  \
                u32 c  = tmp % od;                                           \
                tmp   /= od;                                                 \
                src_idx += c * src_stride[p->axis_perm[axis]];               \
            }                                                                \
            dst[oi] = s[src_idx];                                            \
        }                                                                    \
    } while (0)

fn void cpu_op_permute(void *out, void **srcs, u32 const *src_numels,
                       KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  void *src   = srcs[0];
  u8    ndim  = p->src0_ndim;
  u32   esz   = dtype_itemsize(p->dtype);

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

  switch (esz) {
    case 1: PERMUTE_GATHER(u8 ); break;
    case 2: PERMUTE_GATHER(u16); break;
    case 4: PERMUTE_GATHER(u32); break;
    case 8: PERMUTE_GATHER(u64); break;
    default:
      fprintf(stderr, "cpu_op_permute: itemsize %u unsupported\n", esz);
      abort();
  }
}

#undef PERMUTE_GATHER
