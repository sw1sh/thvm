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
