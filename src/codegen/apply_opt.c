// codegen/apply_opt.c -- mutate a KernelEntry's axes by applying one
// TOpt.  Owns the axis rewrite previously done by WL TKernelApplyOpt;
// WL is a thin LibraryLink wrapper now.
//
// E9 reshape: KernelAxes no longer carries an axis_types[] field.
// `kernel_apply_opt` records the opt in applied_opts[] and maintains
// `n_axes` + `full_shape[]` for split-class opts; the kax_type per
// axis is derived on demand from (output_shape + tail-reduce +
// scalar-reduce + applied_opts) by `axes_compute_axis_types` /
// `axes_resolve_kax_type` (codegen/axis.c).  The validation contract
// (axis-in-range, arg-divides, GLOBAL targets a LOOP, SWAP target
// in-range, applied_opts/MAX_AXES caps) still gates the writes; the
// LOOP precondition for KOP_GLOBAL consults `axes_resolve_kax_type`
// instead of a stored axis_types[].
//
// Each opt class:
//
//   UPCAST / UNROLL / LOCAL / GROUP / GROUPTOP
//     Split full_shape[axis] into outer (= old/arg) + inner (= arg).
//     Insert a new axis after `axis`; the new inner axis takes the
//     opt's KAX_ type by virtue of `axes_compute_axis_types` replaying
//     applied_opts deterministically.  Validation: arg must divide
//     full_shape[axis].
//
//   GLOBAL
//     Mark the selected LOOP axis as GLOBAL via the applied_opts log.
//     Validation: arg must equal the current axis size and the axis
//     must currently resolve to KAX_LOOP.
//
//   SWAP
//     Swap full_shape[axis] <-> full_shape[arg].  No new axis.  The
//     resolver picks up the position swap when replaying applied_opts.
//
//   PADTO / NOLOCALS
//     Reserved.  Rejected here; no axis-structure mutation.
//
//   TC
//     Kernel-aware metadata (matmul-shape gate).  Slice 8 session 5:
//     reads `ke->cached_lift.store_root` via
//     `uop_dag_classify_matmul_shape` (DAG-side matmul classifier).
//     The legacy `tile_analyze_gemm` fallback retired with the
//     dedicated KProgOp-side recogniser; rangeify produces the
//     canonical MUL+REDUCE+OPT_TC scalar_uops pattern for every
//     matmul-shaped kernel, which the lifter (kernel_lift_to_uop)
//     turns into the UOp DAG this gate inspects.  Does not mutate
//     axis structure; routes to tile_anno_record_opt.
//
// Returns 1 on success, 0 on validation failure (axis out of range,
// arg doesn't divide, applied_opts full, MAX_AXES exceeded).

// Slice 8 session 5: KOP_TC gate counter.  The legacy fallback arm
// (tile_analyze_gemm over program[]) retired with session 5's tile.c
// deletion; the only remaining gate is the DAG classifier.  The
// counter is kept to expose dispatch coverage to the surgical suite.
static u64 APPLY_OPT_TC_GATE_DAG = 0;

fn u64 kernel_apply_opt_tc_dag_count(void) {
  return APPLY_OPT_TC_GATE_DAG;
}
fn void kernel_apply_opt_tc_counters_reset(void) {
  APPLY_OPT_TC_GATE_DAG = 0;
}

// Slice 8 session 5: matmul-shape + dtype gate for KOP_TC reads
// uop_dag_classify_matmul_shape over ke->cached_lift.store_root.
// Returns 1 with `*out_dtype` populated on match; 0 otherwise.
static int apply_opt_tc_classify(KernelEntry const *ke, u32 *out_dtype) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  UopDagGemmShape shape;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke,
                                     &shape)) {
    return 0;
  }
  if (out_dtype != NULL) *out_dtype = shape.dtype;
  APPLY_OPT_TC_GATE_DAG++;
  return 1;
}

// True if this opt class splits an axis (vs SWAP / no-op opts).
static int kop_splits_axis(u8 op) {
  return op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL
      || op == KOP_GROUP  || op == KOP_GROUPTOP;
}

fn int kernel_apply_opt(KernelEntry *ke, KOpt opt) {
  if (ke == NULL || ke->axes == NULL) {
    return 0;
  }
  if (opt.op == KOP_TC) {
    if (opt.axis >= tile_anno_axis_count_or_kernelaxes(ke)
        || !tile_mma_size_supported(opt.arg)) {
      return 0;
    }
    u32 dtype = 0;
    if (!apply_opt_tc_classify(ke, &dtype) || dtype != DT_FP32) {
      return 0;
    }
    return tile_anno_record_opt(ke, opt);
  }
  KernelAxes *ax = ke->axes;
  if (ax->n_applied >= MAX_OPTS) {
    return 0;
  }
  // E9 session 4: writer-private scratch.  `ax->_writer.full_shape[]` /
  // `ax->_writer.n_axes` are NOT to be read outside the writer trio
  // (apply_opt.c body, tile_anno.c writer-trio, axis.c lifecycle /
  // validators).  External readers go through axes_resolve_full_shape
  // / axes_resolve_n_axes.
  if (opt.axis >= ax->_writer.n_axes) {
    return 0;
  }

  if (kop_splits_axis(opt.op)) {
    if (opt.arg == 0) {
      return 0;
    }
    u32 axis_size = ax->_writer.full_shape[opt.axis];
    if (axis_size % opt.arg != 0) {
      return 0;
    }
    if (ax->_writer.n_axes >= MAX_AXES) {
      return 0;
    }
    // Shift full_shape[] entries after opt.axis right by one to make
    // room for the new inner axis.  No axis_types[] array to shift --
    // axes_compute_axis_types replays the same insertion against the
    // signal-derived initial layout.
    for (i32 i = (i32)ax->_writer.n_axes; i > (i32)opt.axis + 1; i--) {
      ax->_writer.full_shape[i] = ax->_writer.full_shape[i - 1];
    }
    // Outer keeps its original type by replay; size shrinks by split
    // factor.  Inner takes the opt's type by replay; size = split
    // factor.
    ax->_writer.full_shape[opt.axis]     = axis_size / opt.arg;
    ax->_writer.full_shape[opt.axis + 1] = opt.arg;
    ax->_writer.n_axes++;
  } else if (opt.op == KOP_GLOBAL) {
    u32 axis_size = ax->_writer.full_shape[opt.axis];
    if (opt.arg != axis_size
        || axes_resolve_kax_type(ke, opt.axis) != KAX_LOOP) {
      return 0;
    }
    // No axis_types[] write; resolver replays KOP_GLOBAL against the
    // post-applied_opts state.
  } else if (opt.op == KOP_SWAP) {
    if ((u8)opt.arg >= ax->_writer.n_axes) {
      return 0;
    }
    u32 si = ax->_writer.full_shape[opt.axis];
    u32 sj = ax->_writer.full_shape[opt.arg];
    ax->_writer.full_shape[opt.axis] = sj;
    ax->_writer.full_shape[opt.arg]  = si;
    // No axis_types[] swap; resolver replays KOP_SWAP against the
    // post-applied_opts state.
  } else {
    return 0;
  }

  ax->applied_opts[ax->n_applied++] = opt;
  // E9 session 2: no version++.  Freshness is `tile_axes_hash(ke)` over
  // (applied_opts, output_shape, source_uop) -- recording the new opt
  // already mutates the hash deterministically.
  return 1;
}
