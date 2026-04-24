// backend/cpu/op/add.c - element-wise add with broadcast.
//
// Broadcast rule: if a source has numel 1, repeat its single
// element for every output position.

fn void cpu_op_add(void *out, void **srcs, u32 const *src_numels,
                   KProgOp const *p, u32 out_numel) {
  (void)p;
  if (p->dtype == DT_F32) {
    f32 *o  = (f32 *)out;
    f32 *a  = (f32 *)srcs[0];
    f32 *b  = (f32 *)srcs[1];
    u8   ba = (src_numels[0] == 1);
    u8   bb = (src_numels[1] == 1);
    for (u32 i = 0; i < out_numel; i++) o[i] = a[ba ? 0 : i] + b[bb ? 0 : i];
  } else {
    i32 *o = (i32 *)out;
    i32 *a = (i32 *)srcs[0];
    i32 *b = (i32 *)srcs[1];
    u8   ba = (src_numels[0] == 1);
    u8   bb = (src_numels[1] == 1);
    for (u32 i = 0; i < out_numel; i++) o[i] = a[ba ? 0 : i] + b[bb ? 0 : i];
  }
}
