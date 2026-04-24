// backend/cpu/op/const.c - materialize a scalar CONST into a buffer.
//
// Writes a single element at out[0].  The CpuBuf for the output
// was allocated with numel=1 by materialize_expr, so downstream
// elementwise ops broadcast it via the n_elems check in the
// per-op loops.

fn void cpu_op_const(void *out, void **srcs, u32 const *src_numels,
                     KProgOp const *p, u32 out_numel) {
  (void)srcs; (void)src_numels; (void)out_numel;
  if (p->dtype == DT_F32) {
    f32 v;
    u32 bits = p->arg;
    memcpy(&v, &bits, sizeof(v));
    ((f32 *)out)[0] = v;
  } else {
    ((i32 *)out)[0] = (i32)p->arg;
  }
}
