// codegen/axis.c - a kernel's axis types and extents, tinygrad's
// Scheduler.axis_types and full_shape (codegen/opt/postrange.py:31-33), read off
// the RANGE leaves of its lifted DAG.

// The axes in axis_id order.  An output axis id below output_shape.ndim with no
// RANGE (an extent-1 axis whose address folded to CONST(0)) comes back as LOOP.
// 0 without a schedule or DAG, or past `cap`; the arrays take `cap` entries.
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
  u32 out_ndim = ke->output_shape.ndim;
  if (out_ndim > MAX_AXES - 1) out_ndim = MAX_AXES - 1;
  for (u32 d = 0; d < out_ndim && n < MAX_AXES; d++) {
    int found = 0;
    for (u32 i = 0; i < n; i++) if (axis_ids[i] == d) { found = 1; break; }
    if (!found) {
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
