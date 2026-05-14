// codegen/apply_opt.c -- mutate a KernelEntry's axes by applying one
// TOpt.  Owns the axis rewrite previously done by WL TKernelApplyOpt;
// WL is a thin LibraryLink wrapper now.
//
// E9 reshape: KpSchedule no longer carries an axis_types[] or
// full_shape[] field; `kernel_apply_opt` is now signal-driven --
// validates the opt against the current shape derived on-demand from
// `axes_compute_full_shape` (output_shape + tail-reduce +
// scalar-reduce + applied_opts), then appends the opt to
// applied_opts[].  Per-axis kax_type / extents are derived on read
// via `axes_resolve_kax_type` / `axes_resolve_full_shape` (codegen/
// axis.c).  The validation contract (axis-in-range, arg-divides,
// GLOBAL targets a LOOP, SWAP target in-range, applied_opts/MAX_AXES
// caps) still gates the writes.
//
// Each opt class:
//
//   UPCAST / UNROLL / LOCAL / GROUP / GROUPTOP
//     Split full_shape[axis] into outer (= old/arg) + inner (= arg).
//     Insert a new axis after `axis`; the new inner axis takes the
//     opt's KAX_ type by virtue of `axes_compute_axis_types` replaying
//     applied_opts deterministically.  Validation: arg must divide
//     the current `axes_compute_full_shape`-derived axis size and
//     n_derived < MAX_AXES.
//
//   GLOBAL
//     Mark the selected LOOP axis as GLOBAL via the applied_opts log.
//     Validation: arg must equal the current axis size and the axis
//     must currently resolve to KAX_LOOP.
//
//   SWAP
//     Position swap recorded in applied_opts.  Validation: both
//     positions in range.  The resolver picks up the swap when
//     replaying applied_opts.
//
//   PADTO / NOLOCALS
//     Reserved.  Rejected here; no axis-structure mutation.
//
//   TC
//     Kernel-aware metadata (matmul-shape gate).  Reads
//     ke->cached_lift.store_root via uop_dag_classify_matmul_shape
//     (DAG-side matmul classifier).  Rangeify produces the canonical
//     MUL+REDUCE+OPT_TC scalar_uops pattern for every matmul-shaped
//     kernel, which the lifter (kernel_lift_to_uop) turns into the
//     UOp DAG this gate inspects.  Does not mutate axis structure;
//     routes to tile_anno_record_opt.
//
// Returns 1 on success, 0 on validation failure (axis out of range,
// arg doesn't divide, applied_opts full, MAX_AXES exceeded).

// KOP_TC gate counter -- exposes dispatch coverage of the DAG
// classifier to the surgical suite.
static u64 APPLY_OPT_TC_GATE_DAG = 0;

fn u64 kernel_apply_opt_tc_dag_count(void) {
  return APPLY_OPT_TC_GATE_DAG;
}
fn void kernel_apply_opt_tc_counters_reset(void) {
  APPLY_OPT_TC_GATE_DAG = 0;
}

// Matmul-shape + dtype gate for KOP_TC: reads
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

// Forward decl: defined in uop/apply_opt_dag.c, included after this file
// in thvm.c.  The forward decl matches `fn` (static inline) so the
// generated symbols stay private to the TU.
fn Term uop_dag_apply_kopt(Term root, KOpt opt);

fn int kernel_apply_opt(KernelEntry *ke, KOpt opt) {
  if (ke == NULL) {
    return 0;
  }
  // DAG-mode: Phase E path.  When cached_lift.store_root is populated
  // (Python-built kernels and post-lift production kernels), mutate the
  // DAG directly via uop_dag_apply_kopt.  This avoids the legacy
  // KpSchedule.applied_opts[] log + axes_compute_axis_types resolver
  // chain.  The renderer reads the post-mutation DAG directly
  // (UOP_RANGE.axis_type for parallel TC, UOP_OPT(_, TC, factor) for
  // the simdgroup_matrix template).
  if (ke->cached_lift.store_root != 0) {
    Term new_root = uop_dag_apply_kopt(ke->cached_lift.store_root, opt);
    if (new_root == 0 || new_root == ke->cached_lift.store_root) return 0;
    ke->cached_lift.store_root = new_root;
    ke->compute_root = new_root;
    if (opt.op == KOP_TC) APPLY_OPT_TC_GATE_DAG++;
    return 1;
  }
  if (ke->schedule == NULL) {
    return 0;
  }
  if (opt.op == KOP_TC) {
    // KOP_TC.arg is the simdgroup matmul tile size.  Apple GPUs accept
    // 8/16/32 (the only sizes simdgroup_matrix supports today); reject
    // anything else so autotune can't propose an unbuildable kernel.
    if (opt.axis >= axes_resolve_n_axes(ke)
        || (opt.arg != 8 && opt.arg != 16 && opt.arg != 32)) {
      return 0;
    }
    u32 dtype = 0;
    if (!apply_opt_tc_classify(ke, &dtype) || dtype != DT_FP32) {
      return 0;
    }
    return tile_anno_record_opt(ke, opt);
  }
  KpSchedule *sched = ke->schedule;
  if (sched->n_applied >= MAX_OPTS) {
    return 0;
  }
  // Derive the current pre-opt extents on-demand from
  // (output_shape + tail-reduce + scalar-reduce + applied_opts) via
  // axes_compute_full_shape.  Validate the opt against the derived
  // scratch, then append to applied_opts -- subsequent calls see
  // the post-opt state by replaying the longer log.
  u32 cur_shape[MAX_AXES];
  u32 n_axes = axes_compute_full_shape(ke, cur_shape, MAX_AXES);
  if (n_axes == 0 || opt.axis >= n_axes) {
    return 0;
  }

  if (kop_splits_axis(opt.op)) {
    if (opt.arg == 0) {
      return 0;
    }
    u32 axis_size = cur_shape[opt.axis];
    if (axis_size % opt.arg != 0) {
      return 0;
    }
    if (n_axes >= MAX_AXES) {
      return 0;
    }
    // axes_compute_full_shape will replay this same split (shift
    // entries > axis right; insert axis_size/arg at axis, arg at
    // axis+1) when it next reads applied_opts.  No scratch to mutate.
  } else if (opt.op == KOP_GLOBAL) {
    u32 axis_size = cur_shape[opt.axis];
    if (opt.arg != axis_size
        || axes_resolve_kax_type(ke, opt.axis) != KAX_LOOP) {
      return 0;
    }
    // axes_compute_full_shape's KOP_GLOBAL arm is a no-op for shape;
    // axes_compute_axis_types stamps KAX_GLOBAL on read.
  } else if (opt.op == KOP_SWAP) {
    if ((u8)opt.arg >= n_axes) {
      return 0;
    }
    // Resolver replays SWAP on its scratch; nothing to mutate here.
  } else {
    return 0;
  }

  sched->applied_opts[sched->n_applied++] = opt;
  // No version bump: freshness is tile_axes_hash(ke) over
  // (applied_opts, output_shape, source_uop) and recording the new
  // opt already mutates that hash deterministically.
  return 1;
}
