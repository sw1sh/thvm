// backend/cpu/op/neg.c - element-wise negation.
//
// Bool / unsigned NEG isn't a meaningful operation; we route to wrap
// arithmetic (-x for unsigned wraps modulo 2^width, matching tinygrad
// + LLVM behavior).  The grad chain rule never produces NEG on int
// dtypes; only direct user calls reach the unsigned path.

fn void cpu_op_neg(void *out, void **srcs, u32 const *src_numels,
                   KProgOp const *p, u32 out_numel) {
  u8 bs = (src_numels[0] == 1);
  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = -a[bs ? 0 : i];
      break;
    }
    case DT_FP64: {
      f64 *o = (f64 *)out, *a = (f64 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = -a[bs ? 0 : i];
      break;
    }
    case DT_FP16:
    case DT_BF16:
    case DT_FP8E4M3:
    case DT_FP8E5M2:
      cpu_op_run_via_f32(cpu_op_neg, out, srcs, src_numels, p, out_numel);
      break;
#define CASE(DT, T) INT_UN_CASE(DT, T, -x)
    EACH_INT_DTYPE(CASE)
#undef CASE
    case DT_BOOL: {
      // -bool collapses to bool itself (the only nontrivial value is 1
      // and -1 == 1 mod 2); makes NEG trivially safe in mixed graphs.
      u8 *o = (u8 *)out, *a = (u8 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = (u8)(a[bs ? 0 : i] & 1);
      break;
    }
    default:
      fprintf(stderr, "cpu_op_neg: dtype %u not supported\n", p->dtype);
      abort();
  }
}
