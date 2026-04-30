// backend/cpu/op/add.c - element-wise add with broadcast.
//
// Broadcast rule: if a source has numel 1, repeat its single
// element for every output position.  Bool ADD reduces to logical OR.
// f16 / bf16 use the promote-to-f32 path (cpu_op_run_via_f32);
// f32 / f64 / integers run native.

fn void cpu_op_add(void *out, void **srcs, u32 const *src_numels,
                   KProgOp const *p, u32 out_numel) {
  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0], *b = (f32 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++) o[i] = a[ba ? 0 : i] + b[bb ? 0 : i];
      break;
    }
    case DT_FP64: {
      f64 *o = (f64 *)out, *a = (f64 *)srcs[0], *b = (f64 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++) o[i] = a[ba ? 0 : i] + b[bb ? 0 : i];
      break;
    }
    case DT_FP16:
    case DT_BF16:
    case DT_FP8E4M3:
    case DT_FP8E5M2:
    case DT_INT4:
    case DT_UINT4:
      cpu_op_run_via_f32(cpu_op_add, out, srcs, src_numels, p, out_numel);
      break;
    case DT_BOOL: {
      u8 *o = (u8 *)out, *a = (u8 *)srcs[0], *b = (u8 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++)
        o[i] = (u8)((a[ba ? 0 : i] | b[bb ? 0 : i]) & 1);
      break;
    }
#define CASE(DT, T) INT_BIN_CASE(DT, T, +)
    EACH_INT_DTYPE(CASE)
#undef CASE
    default:
      fprintf(stderr, "cpu_op_add: dtype %u not supported\n", p->dtype);
      abort();
  }
}
