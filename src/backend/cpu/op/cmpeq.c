// backend/cpu/op/cmpeq.c - element-wise compare-equal.
//
// Mirror of cmplt.c with `==` instead of `<`.  Output dtype matches
// the inputs; conventionally a bool, but step 12 keeps it as the
// input dtype to avoid introducing a bool tag.  Used by the
// REDUCE_MAX grad rule to build the one-hot argmax mask
// `MASK[i] = (a[i] == max)`.

fn void cpu_op_cmpeq(void *out, void **srcs, u32 const *src_numels,
                     KProgOp const *p, u32 out_numel) {
  u8 ba = (src_numels[0] == 1);
  u8 bb = (src_numels[1] == 1);
  if (p->dtype == DT_F32) {
    f32 *o = (f32 *)out;
    f32 *a = (f32 *)srcs[0];
    f32 *b = (f32 *)srcs[1];
    for (u32 i = 0; i < out_numel; i++)
      o[i] = a[ba ? 0 : i] == b[bb ? 0 : i] ? 1.0f : 0.0f;
  } else {
    i32 *o = (i32 *)out;
    i32 *a = (i32 *)srcs[0];
    i32 *b = (i32 *)srcs[1];
    for (u32 i = 0; i < out_numel; i++)
      o[i] = a[ba ? 0 : i] == b[bb ? 0 : i] ? 1 : 0;
  }
}
