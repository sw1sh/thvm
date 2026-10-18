// schedule/kernel_lift.c - package the UOP_STORE root that rangeify_unified
// emitted for a kernel's boundary as a KernelUopLift (thvm.h) for the
// renderers and dispatchers.

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

// The UOP_BUFFER for input slot `slot`, shaped like its TenDesc; a slot
// with no TenDesc (input_tids[slot] == 0) gets a 1-D placeholder.  The
// instance is slot + 1 (0 is the output's), so equal-shaped slots stay
// distinct terms under hash-consing.
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

// Fills `out` from the store root rangeify_unified emitted for ke's
// boundary: one UOP_BUFFER per input slot in in_bufs[], out_buf, and the
// raw store_root, which emit_kernel_for_boundary (materialize.c) then
// rewrites for the walker.  Returns 0 when the boundary has no such root.
// A caller that owns `out` releases it with kernel_lift_release.
fn int kernel_lift_to_uop(KernelEntry const *ke, KernelUopLift *out) {
  if (ke == NULL || out == NULL) return 0;
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
