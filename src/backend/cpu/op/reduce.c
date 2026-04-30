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

#define REDUCE_INT_BODY(T, MIN_VAL)                                          \
    do {                                                                     \
        T *o = (T *)out, *a = (T *)srcs[0];                                  \
        for (u32 oi = 0; oi < out_numel; oi++) {                             \
            u32 outer_idx = oi / inner;                                      \
            u32 inner_idx = oi % inner;                                      \
            T   acc = (kind == REDUCE_MAX) ? (T)(MIN_VAL) : (T)0;            \
            for (u32 k = 0; k < axis_size; k++) {                            \
                T v = a[outer_idx * (axis_size * inner) + k * inner + inner_idx];\
                acc = (kind == REDUCE_MAX) ? (v > acc ? v : acc) : (T)(acc + v);\
            }                                                                \
            o[oi] = acc;                                                     \
        }                                                                    \
    } while (0)

fn void cpu_op_reduce(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  u32 kind     = (p->arg >> 24) & 0xFF;
  u32 inner    =  p->arg        & 0x00FFFFFF;
  u32 in_numel = src_numels[0];
  if (out_numel == 0) out_numel = 1;
  if (inner    == 0) inner    = 1;
  u32 axis_size = in_numel / out_numel;

  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0];
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
      break;
    }
    case DT_FP64: {
      f64 *o = (f64 *)out, *a = (f64 *)srcs[0];
      for (u32 oi = 0; oi < out_numel; oi++) {
        u32 outer_idx = oi / inner;
        u32 inner_idx = oi % inner;
        f64 acc = (kind == REDUCE_MAX) ? -INFINITY : 0.0;
        for (u32 k = 0; k < axis_size; k++) {
          f64 v = a[outer_idx * (axis_size * inner) + k * inner + inner_idx];
          acc = (kind == REDUCE_MAX) ? (v > acc ? v : acc) : (acc + v);
        }
        o[oi] = acc;
      }
      break;
    }
    case DT_FP16:
    case DT_BF16:
      cpu_op_run_via_f32(cpu_op_reduce, out, srcs, src_numels, p, out_numel);
      break;
    case DT_BOOL: {
      // SUM = OR-reduce, MAX = OR-reduce too (same answer for bools).
      u8 *o = (u8 *)out, *a = (u8 *)srcs[0];
      for (u32 oi = 0; oi < out_numel; oi++) {
        u32 outer_idx = oi / inner;
        u32 inner_idx = oi % inner;
        u8  acc = 0;
        for (u32 k = 0; k < axis_size; k++) {
          acc |= a[outer_idx * (axis_size * inner) + k * inner + inner_idx] & 1;
        }
        o[oi] = acc;
      }
      break;
    }
    case DT_INT8:   REDUCE_INT_BODY(i8 , INT8_MIN );  break;
    case DT_UINT8:  REDUCE_INT_BODY(u8 , 0);          break;
    case DT_INT16:  REDUCE_INT_BODY(i16, INT16_MIN);  break;
    case DT_UINT16: REDUCE_INT_BODY(u16, 0);          break;
    case DT_INT32:  REDUCE_INT_BODY(i32, INT32_MIN);  break;
    case DT_UINT32: REDUCE_INT_BODY(u32, 0);          break;
    case DT_INT64:  REDUCE_INT_BODY(i64, INT64_MIN);  break;
    case DT_UINT64: REDUCE_INT_BODY(u64, 0);          break;
    default:
      fprintf(stderr, "cpu_op_reduce: dtype %u not supported\n", p->dtype);
      abort();
  }
}

#undef REDUCE_INT_BODY
