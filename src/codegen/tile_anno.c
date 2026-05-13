// codegen/tile_anno.c - Phase E scaffolding: axis annotations on the
// Tile-IR layer.
//
// Today axis information lives in KpSchedule (a side channel on
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
// falling back to ke->schedule when tile_uops isn't populated.  Lets
// consumers in apply_opt.c / propose.c / render_metal.c migrate to
// the new read path without first needing the tile_build to run
// upstream.  Once every consumer goes through this helper AND
// tile_uops is populated before each consumer, switch the helper's
// impl to TILE_AXIS-only and delete the KpSchedule fallback (and
// then KpSchedule itself).
// Stale-tile detection: when KpSchedule has been mutated after the
// last tile_build (apply_opt-driven autotune mutates ke->schedule
// in place), tile_uops carries STALE axis info.  Prefer ke->schedule
// in that case so consumers see the current state.
//
// E9 session 2: freshness compares `ke->tile_axes_hash` (snapshot
// captured when tile_uops was built) against `tile_axes_hash(ke)`
// (current content hash over applied_opts + output_shape +
// source_uop).  Replaced the legacy u32 version counter; the hash
// is collision-resistant by construction so the secondary axis-count
// check is no longer needed for disambiguation, but kept as a cheap
// structural guard.
static int tile_anno_tile_uops_fresh(KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) return 1;  // no axes to compare
  if (ke->tile_axes_hash != tile_axes_hash(ke)) return 0;
  u32 tile_n = tile_anno_axis_count(ke);
  if (tile_n == 0) return 0;
  return tile_n == axes_resolve_n_axes(ke);
}

fn int tile_anno_axis_or_kernelaxes(KernelEntry const *ke, u32 d,
                                    TileAxisInfo *out) {
  if (out == NULL) return 0;
  if (tile_anno_tile_uops_fresh(ke) && tile_anno_axis_at(ke, d, out)) return 1;
  // E9 session 3: read kax_type + extent through the resolvers (signal-
  // derived from output_shape + tail-reduce + scalar-reduce +
  // applied_opts) instead of `ke->schedule->full_shape[]` directly.  The
  // `ke->schedule == NULL` guard moves into the resolvers; both bail to 0 /
  // KAX_LOOP when the kernel hasn't been axes-defaulted yet.
  if (ke == NULL || ke->schedule == NULL) return 0;
  u32 extent = 0;
  if (!axes_resolve_full_shape(ke, d, &extent)) return 0;
  out->kax_type     = axes_resolve_kax_type(ke, d);
  out->extent       = extent;
  out->memory_scope = 0;
  out->vector_width = 0;
  return 1;
}

fn u32 tile_anno_axis_count_or_kernelaxes(KernelEntry const *ke) {
  if (tile_anno_tile_uops_fresh(ke)) {
    u32 n = tile_anno_axis_count(ke);
    if (n != 0) return n;
  }
  // E9 session 3: count via resolver (signal-derived).
  return axes_resolve_n_axes(ke);
}

// applied_opts facade: today these read from KpSchedule.applied_opts[]
// with no Tile-IR equivalent.  When Phase E grows TILE_OPT (or similar)
// nodes that record autotune mutations at the Tile-IR layer, the
// helper switches over.  For now the facade lets every consumer go
// through the same call site so the migration drops in cleanly later.
// External linkage so the metal backend (compiled as a separate
// translation unit, backend_metal.o) can call these.
u32 tile_anno_applied_opts_count(KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) return 0;
  return ke->schedule->n_applied;
}

KOpt const *tile_anno_applied_opts(KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) return NULL;
  return ke->schedule->applied_opts;
}

// Hash all per-axis (kax_type, extent) pairs into the running FNV-1a
// state and return the updated hash.  Used by autotune.c to produce
// cache keys that include axis structure.
// Memory_scope and vector_width are NOT included today; they'll join
// when the cache slot format expands to TileAxisInfo arrays.
u64 tile_anno_hash_axes(KernelEntry const *ke, u64 h) {
  u32 n = tile_anno_axis_count_or_kernelaxes(ke);
  h ^= (u64)n;
  h *= 0x100000001b3ULL;
  for (u32 i = 0; i < n; i++) {
    TileAxisInfo info;
    if (!tile_anno_axis_or_kernelaxes(ke, i, &info)) {
      info.kax_type = 0; info.extent = 0;
    }
    h ^= (u64)info.kax_type;
    h *= 0x100000001b3ULL;
    h ^= (u64)info.extent;
    h *= 0x100000001b3ULL;
  }
  return h;
}

// Phase E writer-side facade: thin wrapper over kernel_apply_opt.
// Kept for the WL FFI (thvm_wl_kernel_apply_opt) and autotune
// callsites so a future TILE_AXIS source-of-truth flip is a
// single-file change.
//
// Returns 1 on success (the opt was applied), 0 on failure (invalid
// opt for this kernel, axis out of range, etc.).
int tile_anno_apply_opt(KernelEntry *ke, KOpt opt) {
  return kernel_apply_opt(ke, opt);
}

// Record an opt as applied without mutating axis structure.  Used
// for metadata-only opts (KOP_TC) that downstream consumers read
// from applied_opts but that don't change axis types/extents.
// Returns 1 on success, 0 if applied_opts is full.
int tile_anno_record_opt(KernelEntry *ke, KOpt opt) {
  if (ke == NULL || ke->schedule == NULL) return 0;
  if (ke->schedule->n_applied >= MAX_OPTS) return 0;
  ke->schedule->applied_opts[ke->schedule->n_applied++] = opt;
  // E9 session 2: freshness via `tile_axes_hash(ke)` over the new
  // applied_opts log; no version bump.
  return 1;
}

// Reset the axes back to the default LOOP/REDUCE shape (used between
// autotune bench candidates so each candidate starts from a fresh
// baseline).  Preserves the autotuned flag.
void tile_anno_axes_reset(KernelEntry *ke) {
  if (ke == NULL || ke->schedule == NULL) return;
  u8  autotuned = ke->schedule->autotuned;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  ke->schedule->autotuned = autotuned;
  axes_default_for(ke);
  axes_ensure_scalar_reduce(ke);
}

// Append a new axis at the end.
//
// E9 session 5: writer-private scratch
// (`_writer.full_shape[]` / `_writer.n_axes`) retired.  The
// resolvers derive axis count + extents from
// (output_shape + tail-reduce + scalar-reduce + applied_opts) on
// read, so the only writer-trio shape signal that matters is
// applied_opts (mutated by `kernel_apply_opt`).  This function is a
// no-op now; symbol kept for the public header signature.
//
// Returns 1 (info accepted; nothing to record); 0 only on NULL kernel
// for callers that want to detect a closed kernel.
int tile_anno_axis_append(KernelEntry *ke, TileAxisInfo info) {
  if (ke == NULL || ke->schedule == NULL) return 0;
  (void)info;
  return 1;
}
