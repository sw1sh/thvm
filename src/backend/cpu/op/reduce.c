// backend/cpu/op/reduce.c - SUM / MAX reduction.
//
// Step 12 v1: reduce over a single axis.  For 1-D inputs the output
// is a single-element buffer; for higher-rank inputs the reduced
// axis is dropped.  Movement ops stay as separate kernels in v1,
// so the input shape (src_numels[0]) and output shape (out_numel)
// fully determine the per-output stride.

fn void cpu_op_reduce(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  u32 kind = (p->arg >> 16) & 0xFFFF;
  u32 in_numel = src_numels[0];
  if (out_numel == 0) out_numel = 1;
  u32 group = in_numel / out_numel;

  if (p->dtype == DT_F32) {
    f32 *o = (f32 *)out;
    f32 *a = (f32 *)srcs[0];
    for (u32 oi = 0; oi < out_numel; oi++) {
      f32 acc = (kind == REDUCE_MAX) ? -INFINITY : 0.0f;
      for (u32 j = 0; j < group; j++) {
        f32 v = a[oi * group + j];
        acc = (kind == REDUCE_MAX) ? (v > acc ? v : acc) : (acc + v);
      }
      o[oi] = acc;
    }
  } else {
    i32 *o = (i32 *)out;
    i32 *a = (i32 *)srcs[0];
    for (u32 oi = 0; oi < out_numel; oi++) {
      i32 acc = (kind == REDUCE_MAX) ? INT32_MIN : 0;
      for (u32 j = 0; j < group; j++) {
        i32 v = a[oi * group + j];
        acc = (kind == REDUCE_MAX) ? (v > acc ? v : acc) : (acc + v);
      }
      o[oi] = acc;
    }
  }
}
