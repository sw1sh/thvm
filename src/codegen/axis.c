// codegen/axis.c -- KernelAxes lifecycle: default constructor +
// applied-opt query helpers.  The mutation (axes_apply_opt) lives in
// codegen/apply_opt.c.
//
// Mirrors tinygrad's `Kernel.axis_types[]` / `Kernel.full_shape[]`
// (tinygrad/codegen/opt/kernel.py).  Default at materialize-time:
// one LOOP axis per output dim, plus a trailing REDUCE axis sized at
// `src_numel / out_numel` for kernels whose final program op is
// UOP_REDUCE.  This matches today's flat `for i = 0..numel-1` emit
// and keeps the existing 393/393 test grid passing while the
// variant emitter is under construction.

fn void axes_default_for(KernelEntry *ke) {
  if (ke->axes.n_axes != 0) return;     // already configured

  KernelAxes *ax = &ke->axes;
  u32 nd = ke->output_shape.ndim;
  if (nd > MAX_AXES - 1) nd = MAX_AXES - 1;

  for (u32 i = 0; i < nd; i++) {
    ax->axis_types[i] = KAX_LOOP;
    ax->full_shape[i] = ke->output_shape.dims[i];
  }
  ax->n_axes = (u8)nd;

  // Trailing REDUCE: append axis sized at the ratio between the
  // tail-REDUCE op's source numel and the kernel output numel.
  // Mirrors WL Kernel.wl `defaultFullShape`'s redOp.numel/outNumel.
  if (ke->n_ops > 0 && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE) {
    KProgOp const *rd = &ke->program[ke->n_ops - 1];
    u32 src_numel;
    if (KSRC_IS_INPUT(rd->src[0])) {
      src_numel = ke->input_numels[KSRC_INDEX(rd->src[0])];
    } else {
      src_numel = ke->program[KSRC_INDEX(rd->src[0])].numel;
    }
    u32 out_numel = ke->output_numel ? ke->output_numel : 1;
    u32 axis_size = src_numel / out_numel;
    if (ax->n_axes < MAX_AXES) {
      ax->axis_types[ax->n_axes] = KAX_REDUCE;
      ax->full_shape[ax->n_axes] = axis_size;
      ax->n_axes++;
    }
  }
}
