// codegen/cg.c - shared codegen utilities.
//
// Thin support layer for the CPU JIT pre-build gate.  Public surface:
//   - cg_supports(ke): pre-flight gate; cpu_jit_dispatch calls this
//     before warming up the JIT cache.  Accepts any kernel whose
//     materialize-time lift populated cached_lift.store_root --
//     cpu_jit_build hands the lifted root to cg_render_uop_kernel_c_root.

int cg_supports(KernelEntry const *ke) {
  if (ke == NULL) return 0;
  // Materialize populates cached_lift.store_root on every emitted
  // kernel; cpu_jit_build hands the lifted root to
  // cg_render_uop_kernel_c_root.  Lift-decline kernels (store_root==0)
  // are not JIT-eligible and fall through to the interpreter.
  return ke->cached_lift.store_root != 0;
}
