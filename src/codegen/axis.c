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
  // Idempotent: if `ke->axes` already has a non-zero n_axes, another
  // kid sharing this kernel_program_cache slot already populated
  // it.  Per-program-shape sharing means there's nothing to do.
  if (ke->axes == NULL || ke->axes->n_axes != 0) {
    return;
  }

  KernelAxes *ax = ke->axes;
  u32 nd = ke->output_shape.ndim;
  if (nd > MAX_AXES - 1) {
    nd = MAX_AXES - 1;
  }

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
  ax->version++;
  if (ax->version == 0) {
    ax->version = 1;
  }
}

static u32 axes_scalar_reduce_extent(KernelEntry const *ke) {
  if (ke == NULL || ke->scalar_uops == NULL) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_REDUCE_SUM && u->op != S_REDUCE_MAX) {
      continue;
    }
    if (u->src_count < 2 || u->src[1] == 0
        || u->src[1] >= ke->n_scalar_uops) {
      return 0;
    }
    ScalarUop const *rng = &ke->scalar_uops[u->src[1]];
    if (rng->op != S_RANGE) {
      return 0;
    }
    u32 axis_type = (u32)(rng->extra >> 32);
    u32 extent    = (u32)(rng->extra & 0xFFFFFFFFu);
    if (axis_type != S_AXIS_REDUCE || extent == 0) {
      return 0;
    }
    return extent;
  }
  return 0;
}

static int axes_has_reduce_axis(KernelAxes const *ax) {
  if (ax == NULL) {
    return 0;
  }
  for (u32 i = 0; i < ax->n_axes; i++) {
    if (ax->axis_types[i] == KAX_REDUCE
        || ax->axis_types[i] == KAX_GROUP_REDUCE) {
      return 1;
    }
  }
  return 0;
}

fn void axes_ensure_scalar_reduce(struct KernelEntry *ke) {
  if (ke == NULL || ke->axes == NULL) {
    return;
  }
  if (ke->axes->n_axes == 0) {
    axes_default_for(ke);
  }
  if (axes_has_reduce_axis(ke->axes)) {
    return;
  }
  u32 extent = axes_scalar_reduce_extent(ke);
  if (extent == 0 || ke->axes->n_axes >= MAX_AXES) {
    return;
  }
  // Phase E writer migration: route through tile_anno_axis_append.
  // Today the helper writes ke->axes; when Phase F flips, it writes
  // TILE_AXIS directly and bumps tile_axes_version.
  TileAxisInfo info = { KAX_REDUCE, extent, 0, 0 };
  (void)tile_anno_axis_append(ke, info);
}
