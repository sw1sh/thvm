// codegen/propose.c -- shape-heuristic kernel opt proposer.
//
// Given a finalized KernelEntry (post-default-axes), suggest a small
// set of candidate TOpts the autotune loop should try.  Today's
// heuristics are deliberately narrow:
//
//   reduce-tail kernel + axis_size % factor == 0 -> propose UNROLL
//   factor for factor in {2, 4, 8, 16}.
//
// As more opt classes get codegen support (UPCAST output axes,
// LOCAL/GLOBAL Metal bindings, GROUP_REDUCE, etc.) they slot in
// here as additional rules.  The output is a flat list of KOpt;
// the autotune loop applies each one in isolation against the
// baseline (no-opt) variant.

// Reduce-axis size for a tail-REDUCE kernel, or 0 if not reduce-tail
// (or if shape inference fails).  Mirrors the same calc that
// axes_default_for / cg_emit do.
static u32 propose_reduce_axis_size(KernelEntry const *ke) {
  if (ke->n_ops == 0) return 0;
  KProgOp const *rd = &ke->program[ke->n_ops - 1];
  if (rd->opcode != UOP_REDUCE) return 0;
  u32 src_numel;
  if (KSRC_IS_INPUT(rd->src[0])) src_numel = ke->input_numels[KSRC_INDEX(rd->src[0])];
  else                           src_numel = ke->program[KSRC_INDEX(rd->src[0])].numel;
  u32 out_numel = ke->output_numel ? ke->output_numel : 1;
  return src_numel / out_numel;
}

// Index of the reduce axis in axis_types[] -- the last axis of type
// KAX_REDUCE.  Returns 0xFF if none (caller checks `< n_axes`).
static u8 propose_reduce_axis_index(KernelEntry const *ke) {
  if (ke->axes == NULL) return 0xFF;
  for (i32 i = (i32)ke->axes->n_axes - 1; i >= 0; i--) {
    if (ke->axes->axis_types[i] == KAX_REDUCE) return (u8)i;
  }
  return 0xFF;
}

// First LOOP-typed axis at or after `start`, or 0xFF if none.  Used
// to find an output axis to UPCAST.  We pick the FIRST LOOP because
// for elementwise kernels the output is flattened into one loop
// today; later passes that emit a structured nest will want a more
// nuanced choice (innermost LOOP, hardware vector width, etc.).
static u8 propose_first_loop_axis(KernelEntry const *ke, u8 start) {
  if (ke->axes == NULL) return 0xFF;
  for (u8 i = start; i < ke->axes->n_axes; i++) {
    if (ke->axes->axis_types[i] == KAX_LOOP) return i;
  }
  return 0xFF;
}

fn u32 kernel_opts_propose(KernelEntry const *ke, KOpt *out, u32 cap) {
  if (ke == NULL || out == NULL || cap == 0) return 0;
  u32 n = 0;

  static const u32 split_factors[] = {16, 8, 4, 2};
  u32 n_factors = sizeof(split_factors)/sizeof(*split_factors);

  // Reduce-tail UNROLL candidates: {2, 4, 8, 16} where divisible.
  // Skip 1 (= no opt; the autotune loop tracks the baseline
  // separately).  Larger factors first so wins compose if the
  // autotune later supports composite proposals.
  u32 axis_size = propose_reduce_axis_size(ke);
  u8  axis_idx  = propose_reduce_axis_index(ke);
  if (axis_size > 0 && axis_idx != 0xFF) {
    for (u32 i = 0; i < n_factors; i++) {
      u32 f = split_factors[i];
      if (axis_size % f != 0) continue;
      if (n >= cap) break;
      out[n].op   = KOP_UNROLL;
      out[n].axis = axis_idx;
      out[n].arg  = f;
      n++;
    }
  }

  // Elementwise UPCAST candidates: {2, 4, 8, 16} where the selected
  // LOOP axis is divisible.  Targets the first LOOP axis (rank-1 kernels and
  // tinygrad-style flattened outputs both have only one LOOP axis;
  // multi-axis kernels will benefit from a smarter pick once the
  // structured nest renderer arrives).  Skipped for reduce-tail
  // kernels because UPCAST on a reduce kernel would cross the
  // reduce-axis boundary.
  if (axis_size == 0 && ke->output_numel > 0) {
    u8 loop_axis = propose_first_loop_axis(ke, 0);
    if (loop_axis != 0xFF) {
      u32 loop_axis_size = ke->axes->full_shape[loop_axis];
      for (u32 i = 0; i < n_factors; i++) {
        u32 f = split_factors[i];
        if (loop_axis_size % f != 0) continue;
        if (n >= cap) break;
        out[n].op   = KOP_UPCAST;
        out[n].axis = loop_axis;
        out[n].arg  = f;
        n++;
      }
    }
  }
  return n;
}
