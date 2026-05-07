// codegen/cg.c - shared codegen utilities.
//
// Post-F6 (3b60fa7c) this file is a thin support layer for the CPU
// JIT pre-build gate. The original Renderer abstraction + cg_emit
// driver were deleted when render_uop_c became the sole CPU JIT
// emit path; what remains are predicates the dispatch ladder uses
// to decide whether a kernel is JIT-eligible.
//
// Public surface:
//   - cg_program_dtype(ke): uniform-dtype check, DT_COUNT on mixed.
//   - cg_kernel_has_extra_outputs(ke): multi-output rejector.
//   - cg_supports(ke): full pre-flight gate; cpu_jit_dispatch calls
//     this before warming up the JIT cache.
//
// The supported-op predicate (cg_supports) accepts CONST + the
// elementwise ALU set anywhere, and REDUCE only as the last op
// (SUM / MAX kinds).  Anything else -- movement, multi-REDUCE,
// mid-program REDUCE -- bails and the caller falls back to the
// per-op interpreter (backend/cpu/op/*.c).

// Predicate: does this dtype have a clang-buildable native C type
// the JIT renderer can emit directly?  f16/bf16/fp8/int4 need
// conversion routines that don't inline cleanly into the fused
// elementwise loop, so they bail to the interpreter.
static int cg_dtype_supported(u32 dt) {
  switch (dt) {
    case DT_BOOL:
    case DT_INT8:  case DT_UINT8:
    case DT_INT16: case DT_UINT16:
    case DT_INT32: case DT_UINT32:
    case DT_INT64: case DT_UINT64:
    case DT_FP32:  case DT_FP64:
      return 1;
    default:
      return 0;
  }
}

// Does the op support non-float dtypes?  The transcendentals
// (RECIP / EXP2 / LOG2 / SQRT) are float-only at the kernel level.
static int cg_op_is_float_only(u8 op) {
  return op == UOP_RECIP || op == UOP_EXP2
      || op == UOP_LOG2  || op == UOP_SQRT;
}

// Pick the kernel's uniform dtype: scan every op + every input and
// return the dtype if all match, else DT_COUNT (= sentinel).  Used
// by cg_supports to gate non-uniform kernels off the CPU JIT.
u32 cg_program_dtype(KernelEntry const *ke) {
  if (ke->n_ops == 0) return DT_COUNT;
  u32 dt = ke->program[0].dtype;
  for (u32 i = 1; i < ke->n_ops; i++) {
    if (ke->program[i].dtype != dt) return DT_COUNT;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != dt) return DT_COUNT;
  }
  return dt;
}

static u32 cg_src_numel(KernelEntry const *ke, u32 raw) {
  if (KSRC_IS_INPUT(raw)) {
    u32 idx = KSRC_INDEX(raw);
    return idx < ke->n_inputs ? ke->input_numels[idx] : 0;
  }
  u32 idx = KSRC_INDEX(raw);
  return idx < ke->n_ops ? ke->program[idx].numel : 0;
}

// Multi-output kernel guard (Step 3 of multi-output groundwork).
// Returns 1 iff the kernel writes more than one output buffer.
// Callers (renderers + dispatchers) bail when this returns 1 until
// per-output emit + dispatch paths land.  See
// docs/plans/bufferize.md "Multi-output kernel infrastructure".
// Externally visible (no `fn`) so the Metal .m TU can call it.
int cg_kernel_has_extra_outputs(KernelEntry const *ke) {
  return ke != NULL && ke->n_extra_outputs > 0;
}

int cg_supports(KernelEntry const *ke) {
  // Multi-output programs aren't supported by the legacy KProgOp[]
  // renderer.  The merge planner (step 2) is gated OFF by default,
  // so this only fires when THVM_KERNEL_MERGE=1 is set without the
  // matching codegen + dispatch rollout.  Falls back to interpreter.
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  // Phase C slice 7: when the materialize-time lift succeeded the
  // kernel is renderable via cg_render_uop_kernel_c_root regardless
  // of program[] (which may be NULL under THVM_PHASE_C7_FREE_PROGRAM).
  // Skip the per-KProgOp gate; cpu_jit_build will hand the lifted
  // root directly to the renderer.
  if (ke->cached_lift.store_root != 0) return 1;
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp const *p = &ke->program[i];
    u8 op = p->opcode;
    if (op == UOP_REDUCE) {
      // REDUCE has to be the last op (the "reduce-tail" pattern --
      // outer per-output loop + inner accumulator).  A REDUCE
      // followed by more ops would need a two-pass shader.
      if (i + 1 != ke->n_ops) return 0;
      u8 kind = (u8)((ke->program[i].arg >> 24) & 0xFFu);
      if (kind != REDUCE_SUM && kind != REDUCE_MAX) return 0;
    } else {
      switch (op) {
        case UOP_CONST:
        case UOP_ADD: case UOP_MUL:
        case UOP_NEG:
        case UOP_CMPLT: case UOP_CMPEQ:
          break;
        case UOP_RECIP: case UOP_SQRT:
        case UOP_EXP2:  case UOP_LOG2:
          break;     // float-only; uniformity check below catches int dtypes
        case UOP_RESHAPE: {
          u32 nsrc = p->n_src > 0 ? cg_src_numel(ke, p->src[0]) : 0;
          if (p->n_src != 1 || p->numel != nsrc) return 0;
          break;
        }
        default:
          return 0;
      }
    }
  }
  // Uniform-dtype check: pick one dtype for the whole kernel; any
  // mixed-dtype kernel (post-CAST chain inlined into one boundary,
  // for example) bails to the interpreter.
  u32 dt = cg_program_dtype(ke);
  if (dt == DT_COUNT)               return 0;
  if (!cg_dtype_supported(dt))      return 0;
  // Float-only ops on integer dtypes can't compile.
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (cg_op_is_float_only(ke->program[i].opcode)
        && dt != DT_FP32 && dt != DT_FP64) return 0;
  }
  return 1;
}

