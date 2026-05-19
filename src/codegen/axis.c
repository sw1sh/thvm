// codegen/axis.c -- KpSchedule lifecycle: default constructor +
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
  // Signal-driven resolvers cover the initial state (nd LOOPs +
  // optional trailing REDUCE) directly from (output_shape +
  // tail-reduce + scalar-reduce).  No scratch to populate; symbol
  // kept so existing call ordering in materialize.c / tile.c stays
  // valid.
  (void)ke;
}

// Collect the kernel's full post-opt axis structure from cached_lift
// .store_root.  The lifted DAG is post-mutation (uop_dag_apply_kopt
// rewrites RANGE leaves in place for every split-class opt), so the
// RANGE set + their axis_type fields encode the FINAL axis layout.
// Output axes whose extent is 1 may not appear as RANGE leaves (the
// addr collapses to CONST(0)); we synthesise them from output_shape.
//
// Returns axis count (sorted by axis_id), or 0 when the DAG is empty.
// Caller arrays must be sized >= MAX_AXES.
static u32 axes_dag_collect(KernelEntry const *ke, u8 *kax_out,
                            u32 *extent_out, u32 cap) {
  if (ke == NULL || ke->cached_lift.store_root == 0 || cap == 0) return 0;
  u32 axis_ids[MAX_AXES] = {0};
  u32 axis_types[MAX_AXES] = {0};
  u32 extents[MAX_AXES] = {0};
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root,
                               axis_ids, axis_types, extents,
                               MAX_AXES);
  // Synthesise missing output axes (extent 1 ones whose addr folded
  // to CONST(0)).  Output axis_ids are [0, output_shape.ndim).
  u32 out_ndim = ke->output_shape.ndim;
  if (out_ndim > MAX_AXES - 1) out_ndim = MAX_AXES - 1;
  for (u32 d = 0; d < out_ndim && n < MAX_AXES; d++) {
    int found = 0;
    for (u32 i = 0; i < n; i++) if (axis_ids[i] == d) { found = 1; break; }
    if (!found) {
      // Insert at sorted position.
      u32 ins = n;
      for (u32 i = 0; i < n; i++) if (axis_ids[i] > d) { ins = i; break; }
      for (u32 i = n; i > ins; i--) {
        axis_ids[i]   = axis_ids[i - 1];
        axis_types[i] = axis_types[i - 1];
        extents[i]    = extents[i - 1];
      }
      axis_ids[ins]   = d;
      axis_types[ins] = (u32)KAX_LOOP;
      extents[ins]    = ke->output_shape.dims[d];
      n++;
    }
  }
  if (n == 0 || n > cap) return 0;
  for (u32 i = 0; i < n; i++) {
    kax_out[i]    = (u8)axis_types[i];
    extent_out[i] = extents[i];
  }
  return n;
}

// Predicate: "does this kernel's lifted DAG carry a REDUCE-class
// axis (KAX_REDUCE or KAX_GROUP_REDUCE)?".  axes_dag_collect reads
// the final post-opt RANGE leaves out of cached_lift.store_root,
// which is already mutated in place by every split-class opt.
fn int axes_will_have_reduce_axis(KernelEntry const *ke) {
  if (ke == NULL) {
    return 0;
  }
  if (ke->cached_lift.store_root == 0) return 0;
  u8 kax[MAX_AXES] = {0};
  u32 ext[MAX_AXES] = {0};
  u32 n = axes_dag_collect(ke, kax, ext, MAX_AXES);
  for (u32 i = 0; i < n; i++) {
    if (kax[i] == KAX_REDUCE || kax[i] == KAX_GROUP_REDUCE) return 1;
  }
  return 0;
}

// Derive per-axis kax_type[] from the higher-level signals
// (output_shape + tail-reduce + scalar-reduce + applied_opts).
// Mirrors the writer trio (axes_default_for +
// axes_ensure_scalar_reduce + axes_apply_opt) exactly:
//
//   1. Initial state: `nd = output_shape.ndim` LOOPs (clipped to
//      MAX_AXES-1), optionally followed by a single trailing REDUCE.
//      The trailing-REDUCE is present iff
//        - the kernel program ends in UOP_REDUCE (axes_default_for
//          appends it), OR
//        - the scalar arena carries an S_REDUCE_* over an
//          S_AXIS_REDUCE range (axes_ensure_scalar_reduce appends it).
//   2. Replay applied_opts in order using the same structural logic
//      as axes_apply_opt: KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP
//      split the indicated axis and insert a new inner axis with the
//      opt's KAX_ type; KOP_GLOBAL stamps the indicated axis as
//      KAX_GLOBAL; KOP_SWAP exchanges two positions; KOP_TC carries
//      no axis-structure mutation in axes_apply_opt (rejected there;
//      kernel_apply_opt handles it as metadata).
//
// Returns the number of axes written to `out` (matches the post-replay
// `n_axes` derived from initial_n_axes + count(split-class opts)).
// On overflow, returns 0.
//
// Used by axes_resolve_kax_type as the single kax_type read point
// outside the writer trio.
fn u32 axes_compute_axis_types(struct KernelEntry const *ke, u8 *out,
                               u32 cap) {
  if (ke == NULL || ke->schedule == NULL || out == NULL || cap == 0) {
    return 0;
  }
  // uop_dag_apply_kopt mutates RANGE leaves in place for every
  // split-class opt, so the DAG's post-opt RANGE set encodes the
  // final axis layout.  axes_reset_to_default reverts
  // cached_lift.store_root from cached_lift_init_root so autotune's
  // bench loop is consistent.
  u8 kax_dag[MAX_AXES] = {0};
  u32 ext_dag[MAX_AXES] = {0};
  u32 n_dag = axes_dag_collect(ke, kax_dag, ext_dag, MAX_AXES);
  if (n_dag == 0 || n_dag > cap) return 0;
  for (u32 i = 0; i < n_dag; i++) out[i] = kax_dag[i];
  return n_dag;
}

// Single kax_type read point outside the writer trio.  Returns the
// simulator output -- signal-derived from (output_shape + tail-reduce
// + scalar-reduce + applied_opts), which mirrors the writer trio
// (axes_default_for + axes_ensure_scalar_reduce + axes_apply_opt) by
// construction.
//
// Returns the resolved kax_type (KAX_*) for axis `d`.  When ke /
// axes are NULL, `d >= n_axes`, or the simulator can't speak
// (overflow / unknown opt -- bug in the writer trio's applied_opts
// log), returns KAX_LOOP as a safe default.
fn u8 axes_resolve_kax_type(struct KernelEntry const *ke, u32 d) {
  if (ke == NULL || ke->schedule == NULL) {
    return KAX_LOOP;
  }
  u8 types[MAX_AXES] = {0};
  u32 n = axes_compute_axis_types(ke, types, MAX_AXES);
  if (n == 0 || d >= n) {
    return KAX_LOOP;
  }
  return types[d];
}

// Derive per-axis full_shape extents from the higher-level signals
// (output_shape + tail-reduce + scalar-reduce + applied_opts).
// Mirrors the writer trio (axes_default_for +
// axes_ensure_scalar_reduce + axes_apply_opt) exactly:
//
//   1. Initial state: extents[i] = output_shape.dims[i] for i < nd
//      (clipped to MAX_AXES-1), optionally followed by a trailing
//      REDUCE extent (= source-numel / output-numel for tail-REDUCE
//      programs, or `axes_scalar_reduce_extent(ke)` for scalar-arena
//      reductions).
//   2. Replay applied_opts in order using the same structural logic
//      as kernel_apply_opt: split-class opts (UPCAST/UNROLL/LOCAL/
//      GROUP/GROUPTOP) divide the indicated extent by opt.arg and
//      insert opt.arg as the inner extent at axis+1; KOP_SWAP
//      exchanges two extents in place; KOP_GLOBAL/TC/PADTO/NOLOCALS
//      carry no shape mutation.
//
// Returns the number of extents written to `out`; 0 on overflow,
// unknown opt, or invalid replay (axis out of range, arg doesn't
// divide).  By construction, the value matches `ke->schedule->full_shape`
// + `ke->schedule->n_axes` after the writer trio has produced the same
// applied_opts log.
fn u32 axes_compute_full_shape(struct KernelEntry const *ke, u32 *out,
                               u32 cap) {
  if (ke == NULL || ke->schedule == NULL || out == NULL || cap == 0) {
    return 0;
  }
  // The post-opt RANGE leaves carry final extents.
  u8 kax_dag[MAX_AXES] = {0};
  u32 ext_dag[MAX_AXES] = {0};
  u32 n_dag = axes_dag_collect(ke, kax_dag, ext_dag, MAX_AXES);
  if (n_dag == 0 || n_dag > cap) return 0;
  for (u32 i = 0; i < n_dag; i++) out[i] = ext_dag[i];
  return n_dag;
}

// Per-axis full_shape resolver.  Returns the derived extent for
// axis `d` (signal-replay over output_shape + applied_opts); 0 when
// ke/axes are NULL, d is out of range, or the simulator can't speak.
// This is the only authoritative source -- nothing left to
// cross-check against.
fn u32 axes_resolve_full_shape(struct KernelEntry const *ke, u32 d,
                               u32 *out_extent) {
  if (ke == NULL || ke->schedule == NULL) {
    if (out_extent != NULL) *out_extent = 0;
    return 0;
  }
  u32 extents[MAX_AXES] = {0};
  u32 n = axes_compute_full_shape(ke, extents, MAX_AXES);
  if (n == 0 || d >= n) {
    if (out_extent != NULL) *out_extent = 0;
    return 0;
  }
  if (out_extent != NULL) *out_extent = extents[d];
  return 1;
}

// Axis-count resolver.  Returns the derived axis count
// (output_shape.ndim clipped to MAX_AXES-1, plus 1 if a trailing
// REDUCE-class axis is present, plus the count of split-class
// applied_opts).  Authoritative count.
fn u32 axes_resolve_n_axes(struct KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) {
    return 0;
  }
  u32 extents[MAX_AXES] = {0};
  return axes_compute_full_shape(ke, extents, MAX_AXES);
}

fn void axes_ensure_scalar_reduce(struct KernelEntry *ke) {
  // Signal-driven resolvers cover the trailing REDUCE-axis case
  // directly via axes_scalar_reduce_extent inside
  // axes_compute_full_shape.  No scratch to extend; symbol kept so
  // existing call ordering in materialize.c / tile_anno.c stays
  // valid.
  (void)ke;
}
