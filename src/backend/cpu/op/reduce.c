// backend/cpu/op/reduce.c - SUM / MAX reduction over a single axis.
//
// Encoding of `p->arg` (set in materialize.c / materialize_in_env.c):
//     bits 24..31 : kind  (REDUCE_SUM = 0 or REDUCE_MAX = 1)
//     bits  0..23 : inner = product of input dims AFTER the reduced
//                   axis.  Lets us stride correctly over an
//                   arbitrary axis without the kernel needing the
//                   full input shape.  inner = 1 means the reduced
//                   axis is innermost (the old assumption).
//
// For input shape S, axis A, in_numel = outer * S[A] * inner where
// outer = prod(S[0..A-1]) and inner = prod(S[A+1..end]).  Output
// drops S[A], so out_numel = outer * inner.  Each output index oi
// corresponds to (outer_idx = oi / inner, inner_idx = oi % inner)
// and reduces over k of input[outer_idx, k, inner_idx]:
//     in_flat = outer_idx * (axis_size * inner) + k * inner + inner_idx
// axis_size is recovered as in_numel / out_numel.

fn void cpu_op_reduce(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  u32 kind     = (p->arg >> 24) & 0xFF;
  u32 inner    =  p->arg        & 0x00FFFFFF;
  u32 in_numel = src_numels[0];
  if (out_numel == 0) out_numel = 1;
  if (inner    == 0) inner    = 1;
  u32 axis_size = in_numel / out_numel;

  if (p->dtype == DT_F32) {
    f32 *o = (f32 *)out;
    f32 *a = (f32 *)srcs[0];
    for (u32 oi = 0; oi < out_numel; oi++) {
      u32 outer_idx = oi / inner;
      u32 inner_idx = oi % inner;
      f32 acc = (kind == REDUCE_MAX) ? -INFINITY : 0.0f;
      for (u32 k = 0; k < axis_size; k++) {
        f32 v = a[outer_idx * (axis_size * inner) + k * inner + inner_idx];
        acc = (kind == REDUCE_MAX) ? (v > acc ? v : acc) : (acc + v);
      }
      o[oi] = acc;
    }
  } else {
    i32 *o = (i32 *)out;
    i32 *a = (i32 *)srcs[0];
    for (u32 oi = 0; oi < out_numel; oi++) {
      u32 outer_idx = oi / inner;
      u32 inner_idx = oi % inner;
      i32 acc = (kind == REDUCE_MAX) ? INT32_MIN : 0;
      for (u32 k = 0; k < axis_size; k++) {
        i32 v = a[outer_idx * (axis_size * inner) + k * inner + inner_idx];
        acc = (kind == REDUCE_MAX) ? (v > acc ? v : acc) : (acc + v);
      }
      o[oi] = acc;
    }
  }
}
