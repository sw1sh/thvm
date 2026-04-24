// backend/cpu/op/neg.c - element-wise negation.

fn void cpu_op_neg(void *out, void **srcs, u32 const *src_numels,
                   KProgOp const *p, u32 out_numel) {
  u8 bs = (src_numels[0] == 1);
  if (p->dtype == DT_F32) {
    f32 *o = (f32 *)out;
    f32 *a = (f32 *)srcs[0];
    for (u32 i = 0; i < out_numel; i++) o[i] = -a[bs ? 0 : i];
  } else {
    i32 *o = (i32 *)out;
    i32 *a = (i32 *)srcs[0];
    for (u32 i = 0; i < out_numel; i++) o[i] = -a[bs ? 0 : i];
  }
}
