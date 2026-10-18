// schedule/kernel_lift.c -- look up the unified rangeify pass's
// UOP_STORE root for a scheduled kernel's boundary and package it as
// KernelUopLift for the renderer / dispatcher.  KernelUopLift is
// declared in thvm.h.

fn void kernel_lift_reserve(KernelUopLift *lift, u32 needed) {
  if (needed <= lift->in_cap) {
    return;
  }
  u32 new_cap = lift->in_cap == 0 ? KERNEL_INIT_INPUT : lift->in_cap * 2;
  while (new_cap < needed) {
    new_cap *= 2;
  }
  Term *grown = (Term *)realloc(lift->in_bufs, (size_t)new_cap * sizeof(Term));
  if (grown == NULL) {
    thvm_fatal("kernel_lift_reserve: out of memory");
  }
  for (u32 i = lift->in_cap; i < new_cap; i++) {
    grown[i] = 0;
  }
  lift->in_bufs = grown;
  lift->in_cap  = new_cap;
}

fn void kernel_lift_release(KernelUopLift *lift) {
  free(lift->in_bufs);
  lift->in_bufs    = NULL;
  lift->in_cap     = 0;
  lift->n_inputs   = 0;
  lift->store_root = 0;
  lift->out_buf    = 0;
}

// Build a UOP_BUFFER for kernel input slot `slot` from the TenDesc
// shape.  Falls back to a 1D dummy when the slot has no TenDesc
// (test fixtures or paths where TenDesc isn't wired).
//
// The `instance` field on UOP_BUFFER ensures distinct slots get
// distinct Terms even when shape collides; we use slot+1 (since
// instance=0 is the output's reserved value).
static Term lift_input_buffer(KernelEntry const *ke, u32 slot) {
  if (slot >= ke->n_inputs) return 0;
  u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
  u32 tid = (ke->input_tids != NULL) ? ke->input_tids[slot] : 0;
  u32 inst = slot + 1;
  if (tid != 0 && tid < TENS_NEXT) {
    TenDesc const *td = &TENS[tid];
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                           td->view.shape.ndim, td->view.shape.dims, inst);
  }
  u32 dummy[1] = { 1 };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dummy, inst);
}

// Package the unified-rangeify pass's store_root for a kernel entry
// into a UOp DAG root suitable for cg_render_uop_kernel.  The lifter
// today is a thin short-circuit -- it looks up the unified pass's
// per-kernel output and wraps it as a KernelUopLift; the renderer /
// DAG-side encoder / cpu_uop_walk all consume the result.
//
// kernel_lift_to_uop reserves out->in_bufs for every input; a caller that
// owns `out` releases it with kernel_lift_release.
fn int kernel_lift_to_uop(KernelEntry const *ke, KernelUopLift *out) {
  if (ke == NULL || out == NULL) return 0;
  // Look up the UOP_STORE the rangeify_unified pass emitted for this
  // kernel's boundary and build the buffer fields the bypass-substitution
  // site reads (in_bufs[], n_inputs, out_buf).  store_root is the raw
  // unified root; the caller in materialize.c rewrites it for the walker.
  if (ke->source_uop == 0) {
    return 0;
  }
  u32 ru_idx = bufferize_info_find(term_val(ke->source_uop));
  if (ru_idx == 0xFFFFFFFFu) {
    return 0;
  }
  Term ru_root = rangeify_unified_store_root_at(ru_idx);
  if (ru_root == 0) {
    return 0;
  }
  kernel_lift_reserve(out, ke->n_inputs);
  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    Term in_buf = lift_input_buffer(ke, i);
    if (in_buf == 0) {
      return 0;
    }
    out->in_bufs[i] = in_buf;
  }
  out->out_buf     = uop_root_store_buf(ru_root);
  out->store_root  = ru_root;
  return 1;
}
