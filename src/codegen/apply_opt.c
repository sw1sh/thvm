// codegen/apply_opt.c - apply one KOpt to a kernel: uop_dag_apply_kopt rewrites
// its lifted DAG, and schedule->applied_opts logs the opt.

// Defined in uop/apply_opt_dag.c, which thvm.c includes after this file.
fn Term uop_dag_apply_kopt(Term root, KOpt opt);

// 1 when the opt applies; 0 for a kernel without a DAG or with no_opts, or when
// uop_dag_apply_kopt declines or changes nothing (no such axis, a split that does
// not divide, an unsupported op).
fn int kernel_apply_opt(KernelEntry *ke, KOpt opt) {
  if (ke == NULL || ke->no_opts) {
    return 0;
  }
  if (ke->cached_lift.store_root != 0) {
    Term new_root = uop_dag_apply_kopt(ke->cached_lift.store_root, opt);
    if (new_root == 0 || new_root == ke->cached_lift.store_root) return 0;
    ke->cached_lift.store_root = new_root;
    // The log feeds TKernelOpts["Applied"] and the Metal backend.  A hand-built
    // entry has no schedule; past MAX_OPTS the opt applies unlogged.
    if (ke->schedule != NULL && ke->schedule->n_applied < MAX_OPTS) {
      ke->schedule->applied_opts[ke->schedule->n_applied++] = opt;
    }
    return 1;
  }
  return 0;
}
