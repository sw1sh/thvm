// backend/cpu/op/log2.c - element-wise log base 2 (f32 only).

fn void cpu_op_log2(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  if (p->dtype != DT_FP32) {
    fprintf(stderr, "cpu_op_log2: dtype %u not supported (float-only)\n", p->dtype);
    abort();
  }
  u8 bs = (src_numels[0] == 1);
  f32 *o = (f32 *)out;
  f32 *a = (f32 *)srcs[0];
  for (u32 i = 0; i < out_numel; i++) o[i] = log2f(a[bs ? 0 : i]);
}
