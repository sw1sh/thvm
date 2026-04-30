// backend/cpu/op/recip.c - element-wise reciprocal (1/x).
//
// Float-only.  The grad chain rule produces RECIP only on float
// dtypes; integer paths route through CAST -> f32 -> RECIP -> CAST
// at the WL surface (Phase E).

fn void cpu_op_recip(void *out, void **srcs, u32 const *src_numels,
                     KProgOp const *p, u32 out_numel) {
  u8 bs = (src_numels[0] == 1);
  if (p->dtype == DT_FP32) {
    f32 *o = (f32 *)out;
    f32 *a = (f32 *)srcs[0];
    for (u32 i = 0; i < out_numel; i++) o[i] = 1.0f / a[bs ? 0 : i];
    return;
  }
  if (p->dtype == DT_FP64) {
    f64 *o = (f64 *)out;
    f64 *a = (f64 *)srcs[0];
    for (u32 i = 0; i < out_numel; i++) o[i] = 1.0 / a[bs ? 0 : i];
    return;
  }
  if (p->dtype == DT_FP16 || p->dtype == DT_BF16
      || p->dtype == DT_FP8E4M3 || p->dtype == DT_FP8E5M2) {
    cpu_op_run_via_f32(cpu_op_recip, out, srcs, src_numels, p, out_numel);
    return;
  }
  if (p->dtype == DT_INT32) {
    // Legacy integer reciprocal (integer division-by-zero returns 0).
    // Kept for back-compat; nothing in the new graph emits it.
    i32 *o = (i32 *)out;
    i32 *a = (i32 *)srcs[0];
    for (u32 i = 0; i < out_numel; i++) {
      i32 v = a[bs ? 0 : i];
      o[i] = v != 0 ? (i32)(1 / v) : 0;
    }
    return;
  }
  fprintf(stderr, "cpu_op_recip: dtype %u not supported (float-only)\n", p->dtype);
  abort();
}
