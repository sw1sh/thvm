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
//     (DAG-side matmul classifier).  The unified rangeify pass produces
//     the canonical MUL+REDUCE+OPT_TC UOp DAG pattern for every
//     matmul-shaped kernel, which the lifter (kernel_lift_to_uop)
//     packages as the per-kernel root this gate inspects.  Does not
//     mutate axis structure; routes to tile_anno_record_opt.
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
    if (opt.op == KOP_TC) APPLY_OPT_TC_GATE_DAG++;
    // Mirror the mutation into schedule->applied_opts so WL
    // introspection (TKernelOpts["Applied"]) and the axis-type
    // resolver see the post-opt state.  Skip when the schedule slot
    // is unallocated (Python-built kernels) or already full.
    if (ke->schedule != NULL && ke->schedule->n_applied < MAX_OPTS) {
      ke->schedule->applied_opts[ke->schedule->n_applied++] = opt;
    }
    return 1;
  }
  // No cached_lift.store_root: declined kernel (no DAG to mutate).
  return 0;
}
