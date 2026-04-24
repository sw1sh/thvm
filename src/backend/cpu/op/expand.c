// backend/cpu/op/expand.c - broadcast a source buffer to a larger
// element count.
//
// Step-13 minimal EXPAND: only the scalar (numel=1) -> N case is
// supported.  That covers the autograd path where grad_rec wraps
// every leaf emission in EXPAND(scalar, target.shape).  General
// per-axis broadcasting (where some axes stay fixed and others
// expand from 1 to N) is a step-14 task once view tracking lands.

fn void cpu_op_expand(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  void *src = srcs[0];
  u32 in_numel = src_numels[0];

  if (p->dtype == DT_F32) {
    f32 *dst = (f32 *)out;
    f32 *s   = (f32 *)src;
    if (in_numel == 1) {
      f32 v = s[0];
      for (u32 i = 0; i < out_numel; i++) dst[i] = v;
    } else if (in_numel == out_numel) {
      memcpy(dst, s, (size_t)out_numel * sizeof(f32));
    } else {
      // Fallback: cycle the source to fill the output.  Not
      // semantically correct for arbitrary axis broadcasting; will
      // be replaced when stride-aware EXPAND lands in step 14.
      for (u32 i = 0; i < out_numel; i++) dst[i] = s[i % in_numel];
    }
  } else {
    i32 *dst = (i32 *)out;
    i32 *s   = (i32 *)src;
    if (in_numel == 1) {
      i32 v = s[0];
      for (u32 i = 0; i < out_numel; i++) dst[i] = v;
    } else if (in_numel == out_numel) {
      memcpy(dst, s, (size_t)out_numel * sizeof(i32));
    } else {
      for (u32 i = 0; i < out_numel; i++) dst[i] = s[i % in_numel];
    }
  }
}
