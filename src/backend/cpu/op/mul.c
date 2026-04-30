// backend/cpu/op/mul.c - element-wise multiply with broadcast.
// Bool MUL reduces to logical AND.

fn void cpu_op_mul(void *out, void **srcs, u32 const *src_numels,
                   KProgOp const *p, u32 out_numel) {
  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0], *b = (f32 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++) o[i] = a[ba ? 0 : i] * b[bb ? 0 : i];
      break;
    }
    case DT_BOOL: {
      u8 *o = (u8 *)out, *a = (u8 *)srcs[0], *b = (u8 *)srcs[1];
      u8 ba = (src_numels[0] == 1), bb = (src_numels[1] == 1);
      for (u32 i = 0; i < out_numel; i++)
        o[i] = (u8)(a[ba ? 0 : i] & b[bb ? 0 : i] & 1);
      break;
    }
#define CASE(DT, T) INT_BIN_CASE(DT, T, *)
    EACH_INT_DTYPE(CASE)
#undef CASE
    default:
      fprintf(stderr, "cpu_op_mul: dtype %u not supported\n", p->dtype);
      abort();
  }
}
