// backend/cpu/op/sqrt.c - element-wise square root (f32 only).

#include <math.h>

fn void cpu_op_sqrt(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  if (p->dtype != DT_FP32) {
    fprintf(stderr, "cpu_op_sqrt: dtype %u not supported (float-only)\n", p->dtype);
    abort();
  }
  u8 bs = (src_numels[0] == 1);
  f32 *o = (f32 *)out;
  f32 *a = (f32 *)srcs[0];
  for (u32 i = 0; i < out_numel; i++) o[i] = sqrtf(a[bs ? 0 : i]);
}
