// backend/cpu/op/exp2.c - element-wise 2^x (f32 only).

fn void cpu_op_exp2(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  u8 bs = (src_numels[0] == 1);
  switch (p->dtype) {
    case DT_FP32: {
      f32 *o = (f32 *)out, *a = (f32 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = exp2f(a[bs ? 0 : i]);
      break;
    }
    case DT_FP64: {
      f64 *o = (f64 *)out, *a = (f64 *)srcs[0];
      for (u32 i = 0; i < out_numel; i++) o[i] = exp2(a[bs ? 0 : i]);
      break;
    }
    case DT_FP16:
    case DT_BF16:
    case DT_FP8E4M3:
    case DT_FP8E5M2:
      cpu_op_run_via_f32(cpu_op_exp2, out, srcs, src_numels, p, out_numel);
      break;
    default:
      fprintf(stderr, "cpu_op_exp2: dtype %u not supported (float-only)\n", p->dtype);
      abort();
  }
}
