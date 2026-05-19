// codegen/cg.c - shared codegen utilities.
//
// Thin support layer for the CPU JIT pre-build gate.  Public surface:
//   - cg_kernel_has_extra_outputs(ke): multi-output rejector.
//   - cg_supports(ke): pre-flight gate; cpu_jit_dispatch calls this
//     before warming up the JIT cache.  Accepts any kernel whose
//     materialize-time lift populated cached_lift.store_root --
//     cpu_jit_build hands the lifted root to cg_render_uop_kernel_c_root.

// Multi-output kernel guard.  Returns 1 iff the kernel writes more
// than one output buffer.  Callers (renderers + dispatchers) bail
// when this returns 1.  Externally visible (no `fn`) so the Metal
// .m TU can call it.
int cg_kernel_has_extra_outputs(KernelEntry const *ke) {
  return ke != NULL && ke->n_extra_outputs > 0;
}

int cg_supports(KernelEntry const *ke) {
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  // Materialize populates cached_lift.store_root on every emitted
  // kernel; cpu_jit_build hands the lifted root to
  // cg_render_uop_kernel_c_root.  Lift-decline kernels (store_root==0)
  // are not JIT-eligible and fall through to the interpreter.
  return ke->cached_lift.store_root != 0;
}

