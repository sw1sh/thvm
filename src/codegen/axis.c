// codegen/axis.c -- a kernel's post-opt axis structure, tinygrad's
// Kernel.axis_types[] / Kernel.full_shape[] (tinygrad/codegen/opt/kernel.py).
// The lifted UOp DAG (cached_lift.store_root) IS that structure:
// uop_dag_apply_kopt mutates its RANGE leaves in place for every
// split-class opt (kernel_apply_opt, codegen/apply_opt.c).

// Collect the kernel's full post-opt axis structure from cached_lift
// .store_root.  The lifted DAG is post-mutation (uop_dag_apply_kopt
// rewrites RANGE leaves in place for every split-class opt), so the
// RANGE set + their axis_type fields encode the FINAL axis layout.
// Output axes whose extent is 1 may not appear as RANGE leaves (the
// addr collapses to CONST(0)); we synthesise them from output_shape.
//
// Returns the axis count (sorted by axis_id); 0 when the kernel has no
// schedule (a py-built kernel) or its DAG is empty or overflows `cap`.
// Caller arrays must be sized >= MAX_AXES.
fn u32 axes_collect(KernelEntry const *ke, u8 *kax_out, u32 *extent_out,
                    u32 cap) {
  if (ke == NULL || ke->schedule == NULL || ke->cached_lift.store_root == 0
      || cap == 0) {
    return 0;
  }
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
