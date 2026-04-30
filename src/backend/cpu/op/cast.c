// backend/cpu/op/cast.c -- value-preserving cast kernel.
//
// Promote source dtype -> f32, then demote f32 -> destination dtype.
// Routes through src/dtype/lane.c.  Identity (src_dtype == dst_dtype)
// is handled at the constructor level so we never see it here, but
// fall back to a memcpy as a defensive guard.

fn void cpu_op_cast(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  u32 src_dtype = p->arg;        // src dtype packed by materialize
  u32 dst_dtype = p->dtype;
  u32 n         = (src_numels[0] >= out_numel) ? out_numel : src_numels[0];

  if (src_dtype == dst_dtype) {
    // Defensive: no-op.  Constructor folds identity, so reaching
    // here means a hand-built CAST or a future opt that retains it
    // for shape reasons.
    memcpy(out, srcs[0], dtype_storage_bytes(dst_dtype, n));
    return;
  }

  // Promote -> f32 -> demote.  All bytewise; n elements out.
  f32 *tmp = (f32 *)malloc((size_t)n * sizeof(f32));
  to_fp32_lane(tmp, srcs[0], src_dtype, n);
  from_fp32_lane(out, dst_dtype, tmp, n);
  free(tmp);
}
