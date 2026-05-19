// codegen/apply_opt.c -- mutate a KernelEntry's lifted UOp DAG by
// applying one TOpt.  Wraps uop_dag_apply_kopt: the DAG-side rewriter
// validates the opt against the current RANGE leaves, rewrites them
// in place, and returns the new store_root (or 0 on bail).  The
// outer dispatcher mirrors the mutation into schedule->applied_opts
// for WL introspection (TKernelOpts["Applied"]).
//
// Returns 1 on success, 0 on bail (declined kernel, validation
// failure inside uop_dag_apply_kopt, applied_opts full).

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
