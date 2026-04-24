// backend/cpu/op/exp2.c - element-wise 2^x (f32 only).

fn void cpu_op_exp2(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  (void)p;
  u8 bs = (src_numels[0] == 1);
  f32 *o = (f32 *)out;
  f32 *a = (f32 *)srcs[0];
  for (u32 i = 0; i < out_numel; i++) o[i] = exp2f(a[bs ? 0 : i]);
}
