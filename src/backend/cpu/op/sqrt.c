// backend/cpu/op/sqrt.c - element-wise square root (f32 only).

#include <math.h>

fn void cpu_op_sqrt(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  u8 bs = (src_numels[0] == 1);
  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = sqrtf(a[bs ? 0 : i]);
      break;
    }
    case DT_FP64: {
      f64 *o = (f64 *)out, *a = (f64 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = sqrt(a[bs ? 0 : i]);
      break;
    }
    case DT_FP16:
    case DT_BF16:
      cpu_op_run_via_f32(cpu_op_sqrt, out, srcs, src_numels, p, out_numel);
      break;
    default:
      fprintf(stderr, "cpu_op_sqrt: dtype %u not supported (float-only)\n", p->dtype);
      abort();
  }
}
