// backend/cpu/op/_promote.c -- run an f32 elementwise kernel on a
// promoted-to-f32 view of f16 / bf16 / fp8 inputs, then demote the
// f32 result back to the destination dtype.  Mirrors tinygrad's
// "no native ALU on narrow floats; convert at every use" pattern.
//
// The kernel callback signature matches the existing per-op
// cpu_op_<name> functions: the implementation just routes everything
// through f32 buffers.  Each call allocates `n_src + 1` temp f32
// buffers sized to out_numel; the temps are released before return.

typedef void (*CpuF32KernelFn)(void *out, void **srcs, u32 const *src_numels,
                               KProgOp const *p, u32 out_numel);

// Run an f32 elementwise kernel for an op whose declared dtype is a
// narrow-float (DT_FP16 / DT_BF16 / DT_FP8E4M3 / DT_FP8E5M2).  The
// caller's `srcs[]` already point at the narrow-float source bytes;
// we promote each source into a per-input temp f32 buffer (sized to
// the source's own numel), call the f32 kernel against an f32-dtype
// shadow KProgOp, and demote the f32 result back to the narrow type.
fn void cpu_op_run_via_f32(CpuF32KernelFn f32_kernel,
                            void *out, void **srcs,
                            u32 const *src_numels,
                            KProgOp const *p, u32 out_numel) {
  // Source promote: per-input f32 buffer of size `src_numels[i]`.
  f32 *promoted_srcs[MAX_UOP_SRC] = {0};
  for (u8 s = 0; s < p->n_src; s++) {
    u32 n = src_numels[s] ? src_numels[s] : 1;
    promoted_srcs[s] = (f32 *)malloc((size_t)n * sizeof(f32));
    to_fp32_lane(promoted_srcs[s], srcs[s], p->dtype, n);
  }

  // Output: temp f32, demoted at the end.  Could fast-path when
  // p->dtype is already DT_FP32 -- but the caller never invokes this
  // helper in that case (the f32 kernel runs directly).
  f32 *f32_out = (f32 *)malloc((size_t)out_numel * sizeof(f32));

  // Build a shadow op with dtype = DT_FP32 so the kernel's per-dtype
  // switch picks the f32 path.  Everything else (n_src, numel,
  // shape) carries through unchanged.
  KProgOp shadow = *p;
  shadow.dtype = DT_FP32;

  void *src_ptrs[MAX_UOP_SRC];
  for (u8 s = 0; s < p->n_src; s++) src_ptrs[s] = promoted_srcs[s];

  f32_kernel(f32_out, src_ptrs, src_numels, &shadow, out_numel);

  // Demote f32 result back to `out`.
  from_fp32_lane(out, p->dtype, f32_out, out_numel);

  for (u8 s = 0; s < p->n_src; s++) free(promoted_srcs[s]);
  free(f32_out);
}
