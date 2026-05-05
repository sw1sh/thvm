// codegen/tile_anno.c - Phase E scaffolding: axis annotations on the
// Tile-IR layer.
//
// Today axis information lives in KernelAxes (a side channel on
// KernelEntry).  Phase E migrates each consumer to read directly from
// TILE_AXIS nodes via tile_axis_unpack.  This file is the new home
// for axis-mutator primitives that replace codegen/{axis,apply_opt,
// propose}.c -- once every consumer goes through tile_anno, those
// files can be deleted.
//
// Today the file ships a small read API:
//   tile_anno_axis_count(ke) -- number of TILE_AXIS leaves under
//                               TILE_LOOP_NEST root.
//   tile_anno_axis_at(ke, d, *info) -- per-axis info via
//                                       tile_axis_unpack.
//
// These mirror tile_loop_axis_count / tile_loop_axis_type / etc. but
// with a TileAxisInfo-typed return shape so the caller gets all four
// fields (kax_type, extent, memory_scope, vector_width) in one read.

fn u32 tile_anno_axis_count(KernelEntry const *ke) {
  if (ke == NULL || ke->tile_uops == NULL || ke->tile_root == 0
      || ke->tile_root >= ke->n_tile_uops) return 0;
  TileUop const *root = &ke->tile_uops[ke->tile_root];
  if (root->op != TILE_LOOP_NEST || root->src_count < 2) return 0;
  return (u32)root->src_count - 1;
}

fn int tile_anno_axis_at(KernelEntry const *ke, u32 d, TileAxisInfo *out) {
  u32 n = tile_anno_axis_count(ke);
  if (d >= n || out == NULL) return 0;
  TileUop const *root = &ke->tile_uops[ke->tile_root];
  u32 axis_id = root->src[1 + d];
  if (axis_id == 0 || axis_id >= ke->n_tile_uops) return 0;
  TileUop const *axis = &ke->tile_uops[axis_id];
  if (axis->op != TILE_AXIS) return 0;
  *out = tile_axis_unpack(axis->extra);
  return 1;
}

// Phase E migration helper: read axis info preferring TILE_AXIS,
// falling back to ke->axes when tile_uops isn't populated.  Lets
// consumers in apply_opt.c / propose.c / render_metal.c migrate to
// the new read path without first needing the tile_build to run
// upstream.  Once every consumer goes through this helper AND
// tile_uops is populated before each consumer, switch the helper's
// impl to TILE_AXIS-only and delete the KernelAxes fallback (and
// then KernelAxes itself).
// Stale-tile detection: when KernelAxes has been mutated after the
// last tile_build (apply_opt-driven autotune mutates ke->axes
// in place), tile_uops carries STALE axis info.  Prefer ke->axes
// in that case so consumers see the current state.
//
// Two-part check: (1) versions must match, and (2) axis counts must
// match too.  The count check catches version-counter collisions
// across test setups (memset zeroes version, then version++ can
// land on a value that was previously assigned to tile_axes_version
// via a different axes shape).
static int tile_anno_tile_uops_fresh(KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) return 1;  // no axes to compare
  if (ke->tile_axes_version != ke->axes->version) return 0;
  u32 tile_n = tile_anno_axis_count(ke);
  if (tile_n == 0) return 0;
  return tile_n == ke->axes->n_axes;
}

fn int tile_anno_axis_or_kernelaxes(KernelEntry const *ke, u32 d,
                                    TileAxisInfo *out) {
  if (out == NULL) return 0;
  if (tile_anno_tile_uops_fresh(ke) && tile_anno_axis_at(ke, d, out)) return 1;
  if (ke == NULL || ke->axes == NULL || d >= ke->axes->n_axes) return 0;
  out->kax_type     = ke->axes->axis_types[d];
  out->extent       = ke->axes->full_shape[d];
  out->memory_scope = 0;
  out->vector_width = 0;
  return 1;
}

fn u32 tile_anno_axis_count_or_kernelaxes(KernelEntry const *ke) {
  if (tile_anno_tile_uops_fresh(ke)) {
    u32 n = tile_anno_axis_count(ke);
    if (n != 0) return n;
  }
  if (ke == NULL || ke->axes == NULL) return 0;
  return ke->axes->n_axes;
}

// applied_opts facade: today these read from KernelAxes.applied_opts[]
// with no Tile-IR equivalent.  When Phase E grows TILE_OPT (or similar)
// nodes that record autotune mutations at the Tile-IR layer, the
// helper switches over.  For now the facade lets every consumer go
// through the same call site so the migration drops in cleanly later.
fn u32 tile_anno_applied_opts_count(KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) return 0;
  return ke->axes->n_applied;
}

fn KOpt const *tile_anno_applied_opts(KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) return NULL;
  return ke->axes->applied_opts;
}
