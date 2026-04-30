// codegen/propose.c -- shape-heuristic kernel opt proposer.
//
// Given a finalized KernelEntry (post-default-axes), suggest a small
// set of candidate TOpts the autotune loop should try.  Today's
// heuristics are deliberately narrow (Phase 16 MVP):
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

fn u32 kernel_opts_propose(KernelEntry const *ke, KOpt *out, u32 cap) {
  if (ke == NULL || out == NULL || cap == 0) return 0;
  u32 n = 0;

  // Reduce-tail UNROLL candidates: {2, 4, 8, 16} where divisible.
  // Skip 1 (= no opt; the autotune loop tracks the baseline
  // separately).  Larger factors first so wins compose if the
  // autotune later supports composite proposals.
  u32 axis_size = propose_reduce_axis_size(ke);
  u8  axis_idx  = propose_reduce_axis_index(ke);
  if (axis_size > 0 && axis_idx != 0xFF) {
    static const u32 unroll_factors[] = {16, 8, 4, 2};
    for (u32 i = 0; i < sizeof(unroll_factors)/sizeof(*unroll_factors); i++) {
      u32 f = unroll_factors[i];
      if (axis_size % f != 0) continue;
      if (n >= cap) break;
      out[n].op   = KOP_UNROLL;
      out[n].axis = axis_idx;
      out[n].arg  = f;
      n++;
    }
  }
  return n;
}
