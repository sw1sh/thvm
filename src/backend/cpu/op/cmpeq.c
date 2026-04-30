// backend/cpu/op/cmpeq.c - element-wise compare-equal.
//
// Output dtype matches the input dtype (f32 -> 1.0f/0.0f, integer
// dtypes -> typed 1/0).  This convention predates the bool dtype
// (Phase B); the REDUCE_MAX grad rule and other graph-level consumers
// rely on the output sharing dtype with the inputs.  Phase E will
// add an explicit CAST to bool when the consumer needs it.

#define CMPEQ_INT_CASE(DT, T)                                          \
    case DT: {                                                         \
        T *o = (T *)out, *a = (T *)srcs[0], *b = (T *)srcs[1];         \
        u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);       \
        for (u32 i = 0; i < out_numel; i++)                            \
            o[i] = (a[ba ? 0 : i] == b[bb ? 0 : i]) ? (T)1 : (T)0;     \
        break;                                                         \
    }

fn void cpu_op_cmpeq(void *out, void **srcs, u32 const *src_numels,
                     KProgOp const *p, u32 out_numel) {
  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0], *b = (f32 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++)
        o[i] = a[ba ? 0 : i] == b[bb ? 0 : i] ? 1.0f : 0.0f;
      break;
    }
    case DT_FP64: {
      f64 *o = (f64 *)out, *a = (f64 *)srcs[0], *b = (f64 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++)
        o[i] = a[ba ? 0 : i] == b[bb ? 0 : i] ? 1.0 : 0.0;
      break;
    }
    case DT_FP16:
    case DT_BF16:
      cpu_op_run_via_f32(cpu_op_cmpeq, out, srcs, src_numels, p, out_numel);
      break;
    case DT_BOOL: {
      u8 *o = (u8 *)out, *a = (u8 *)srcs[0], *b = (u8 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++)
        o[i] = ((a[ba ? 0 : i] & 1) == (b[bb ? 0 : i] & 1)) ? 1 : 0;
      break;
    }
#define CASE(DT, T) CMPEQ_INT_CASE(DT, T)
    EACH_INT_DTYPE(CASE)
#undef CASE
    default:
      fprintf(stderr, "cpu_op_cmpeq: dtype %u not supported\n", p->dtype);
      abort();
  }
}

#undef CMPEQ_INT_CASE
