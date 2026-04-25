// backend/cpu/op/expand.c - axis-aware broadcast.
//
// Given source shape src0_dims (rank src0_ndim) and output shape
// out_dims (rank out_ndim), each output index oi decomposes into
// per-axis coords; the source flat index is built by IGNORING the
// coord on any axis where src0_dims[axis] == 1 (broadcast) -- those
// axes contribute 0 to the source offset.  Remaining axes contribute
// coord * src_stride[axis] (row-major over src shape).
//
// This is the standard tinygrad MovementOps.EXPAND: stride[i] = 0
// for broadcast axes, normal stride otherwise.  Handles leading-axis
// ({2}->{2,2} = {a,a,b,b}), trailing-axis ({1,3}->{2,3} = each row
// is the source), full scalar ({1}->anything), and identity
// (src_numel == out_numel as memcpy fast path).
//
// Falls back to the legacy in_numel-cycle if KProgOp doesn't carry
// shape info (out_ndim == 0) -- preserves correctness for the
// pre-plumbing call sites the materializer hadn't been updated for
// when the field was first added.

static inline void expand_index_walker(u32 oi, u8 ndim,
                                       u32 const *out_dims,
                                       u32 const *src_dims,
                                       u32 *src_index_out) {
  // Decompose oi into per-axis coords (row-major over out_dims),
  // accumulating into the source flat index using src_strides
  // computed on the fly.  Walk axes from innermost to outermost.
  u32 src_idx = 0;
  u32 src_stride = 1;
  for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
    u32 od = out_dims[axis];
    u32 sd = src_dims[axis];
    u32 coord = oi % od;
    oi /= od;
    if (sd != 1) {                  // not a broadcast axis: contribute
      src_idx += coord * src_stride;
    }
    src_stride *= sd;               // sd is 1 on broadcast axes -> no growth
  }
  *src_index_out = src_idx;
}

fn void cpu_op_expand(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  void *src = srcs[0];
  u32 in_numel = src_numels[0];

  u8 use_axis_aware = (p->out_ndim > 0)
                   && (p->src0_ndim == p->out_ndim);

  if (p->dtype == DT_F32) {
    f32 *dst = (f32 *)out;
    f32 *s   = (f32 *)src;
    if (in_numel == 1) {
      f32 v = s[0];
      for (u32 i = 0; i < out_numel; i++) dst[i] = v;
      return;
    }
    if (in_numel == out_numel) {
      memcpy(dst, s, (size_t)out_numel * sizeof(f32));
      return;
    }
    if (use_axis_aware) {
      for (u32 oi = 0; oi < out_numel; oi++) {
        u32 si;
        expand_index_walker(oi, p->out_ndim, p->out_dims,
                            p->src0_dims, &si);
        dst[oi] = s[si];
      }
      return;
    }
    // Legacy fallback (correct for trailing-axis broadcast only).
    for (u32 i = 0; i < out_numel; i++) dst[i] = s[i % in_numel];
  } else {
    i32 *dst = (i32 *)out;
    i32 *s   = (i32 *)src;
    if (in_numel == 1) {
      i32 v = s[0];
      for (u32 i = 0; i < out_numel; i++) dst[i] = v;
      return;
    }
    if (in_numel == out_numel) {
      memcpy(dst, s, (size_t)out_numel * sizeof(i32));
      return;
    }
    if (use_axis_aware) {
      for (u32 oi = 0; oi < out_numel; oi++) {
        u32 si;
        expand_index_walker(oi, p->out_ndim, p->out_dims,
                            p->src0_dims, &si);
        dst[oi] = s[si];
      }
      return;
    }
    for (u32 i = 0; i < out_numel; i++) dst[i] = s[i % in_numel];
  }
}
