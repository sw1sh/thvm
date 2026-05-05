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
// External linkage so the metal backend (compiled as a separate
// translation unit, backend_metal.o) can call these.
u32 tile_anno_applied_opts_count(KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) return 0;
  return ke->axes->n_applied;
}

KOpt const *tile_anno_applied_opts(KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL) return NULL;
  return ke->axes->applied_opts;
}

// Hash all per-axis (kax_type, extent) pairs into the running FNV-1a
// state and return the updated hash.  Used by kernel_program_cache.c
// and autotune.c to produce cache keys that include axis structure.
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

// Phase E writer-side facade: route axis-mutating opt application
// through the tile_anno API.  Today this is a thin wrapper over
// kernel_apply_opt (which mutates ke->axes); when Phase F flips
// the source-of-truth, the wrapper switches to mutating TILE_AXIS
// directly + bumping tile_axes_version.  Migrating callers to this
// helper now means Phase F's flip is a one-file change.
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
  if (ke == NULL || ke->axes == NULL) return 0;
  if (ke->axes->n_applied >= MAX_OPTS) return 0;
  ke->axes->applied_opts[ke->axes->n_applied++] = opt;
  ke->axes->version++;
  if (ke->axes->version == 0) ke->axes->version = 1;
  return 1;
}

// Reset the axes back to the default LOOP/REDUCE shape (used between
// autotune bench candidates so each candidate starts from a fresh
// baseline).  Preserves the autotuned flag and the version counter
// so cache freshness checks see the mutation.  Phase F flip switches
// this to also clear/rebuild TILE_AXIS.
void tile_anno_axes_reset(KernelEntry *ke) {
  if (ke == NULL || ke->axes == NULL) return;
  u8  autotuned = ke->axes->autotuned;
  u32 version   = ke->axes->version;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->autotuned = autotuned;
  ke->axes->version   = version;
  axes_default_for(ke);
  axes_ensure_scalar_reduce(ke);
}

// Phase E writer-side: split an axis at position d into
// (outer at d, inner at d+1).  outer keeps its current type with
// size = orig_size / factor; inner takes new_inner_type with size =
// factor.  Today writes ke->axes via the existing axes_apply_opt
// machinery (encoded as a KOP_LOCAL/UPCAST/UNROLL/GROUP/GROUPTOP);
// when Phase F flips, this becomes the canonical TILE_AXIS array
// reshape primitive.
//
// Returns 1 on success, 0 if d is out of range, factor doesn't
// divide axis_size, or the array is full.
int tile_anno_apply_split(KernelEntry *ke, u32 d, u32 factor,
                          u32 new_inner_type) {
  if (ke == NULL || factor == 0) return 0;
  // Map new_inner_type back to a KOpt op so we can route through
  // axes_apply_opt for now.  Phase F replaces this with direct
  // TILE_AXIS array surgery.
  u8 op;
  switch (new_inner_type) {
    case KAX_LOCAL:        op = KOP_LOCAL;    break;
    case KAX_UPCAST:       op = KOP_UPCAST;   break;
    case KAX_UNROLL:       op = KOP_UNROLL;   break;
    case KAX_GROUP_REDUCE: op = KOP_GROUP;    break;
    default: return 0;  // Unsupported split target; caller bails.
  }
  KOpt opt = { op, (u8)d, factor };
  return tile_anno_apply_opt(ke, opt);
}

// Append a new axis at the end.  Today writes through KernelAxes
// (the existing source-of-truth); when Phase F flips, this writes
// to TILE_AXIS directly.  Returns 1 on success, 0 if the axis array
// is full.
int tile_anno_axis_append(KernelEntry *ke, TileAxisInfo info) {
  if (ke == NULL || ke->axes == NULL) return 0;
  if (ke->axes->n_axes >= MAX_AXES) return 0;
  u32 d = ke->axes->n_axes++;
  ke->axes->axis_types[d] = info.kax_type;
  ke->axes->full_shape[d] = info.extent;
  ke->axes->version++;
  if (ke->axes->version == 0) ke->axes->version = 1;
  return 1;
}

// Insert a new axis BEFORE position d (i.e. existing axes at d..n-1
// shift right by one; the new axis takes slot d).  When tile_uops
// is also populated, mirrors the insertion at the TILE_AXIS array
// level too.  Returns 1 on success, 0 if d is out of range or
// either backing store is full.
//
// This is the structural primitive that Phase F's source-of-truth
// flip needs: axes_apply_opt's split logic reduces to (a) read the
// existing axis at d, (b) shrink it (extent /= factor), (c) insert
// a new axis at d+1 with the inner type and size = factor.
int tile_anno_axis_insert(KernelEntry *ke, u32 d, TileAxisInfo info) {
  if (ke == NULL || ke->axes == NULL) return 0;
  if (d > ke->axes->n_axes) return 0;
  if (ke->axes->n_axes >= MAX_AXES) return 0;
  // Shift right.
  for (i32 i = (i32)ke->axes->n_axes; i > (i32)d; i--) {
    ke->axes->axis_types[i] = ke->axes->axis_types[i - 1];
    ke->axes->full_shape[i] = ke->axes->full_shape[i - 1];
  }
  ke->axes->axis_types[d] = info.kax_type;
  ke->axes->full_shape[d] = info.extent;
  ke->axes->n_axes++;
  ke->axes->version++;
  if (ke->axes->version == 0) ke->axes->version = 1;
  // (When Phase F flips, the TILE_AXIS array surgery happens here:
  //  shift root->src[d+1..] right, allocate a new TILE_AXIS leaf,
  //  patch root->src[d+1] to point at it, bump src_count.)
  return 1;
}

// Sanity check: returns 1 iff KernelAxes and TILE_AXIS agree on
// axis count + per-axis (kax_type, extent).  Useful for debugging
// the migration -- if this ever returns 0, tile_uops is stale and
// the freshness check should bail.
int tile_anno_axes_match(KernelEntry const *ke) {
  if (ke == NULL || ke->axes == NULL || ke->tile_uops == NULL) return 1;
  u32 ka_n = ke->axes->n_axes;
  u32 tile_n = tile_anno_axis_count(ke);
  if (ka_n != tile_n) return 0;
  for (u32 i = 0; i < ka_n; i++) {
    TileAxisInfo info;
    if (!tile_anno_axis_at(ke, i, &info)) return 0;
    if (info.kax_type != ke->axes->axis_types[i]) return 0;
    if (info.extent != ke->axes->full_shape[i]) return 0;
  }
  return 1;
}

// Direct per-axis write.  Updates both KernelAxes (legacy backing
// store) AND TILE_AXIS (when tile_uops is fresh) with the new
// TileAxisInfo.  Used by future code paths that want to set
// memory_scope or vector_width on an existing axis without going
// through the KOpt machinery.  Returns 1 on success, 0 if `d` is
// out of range or both backing stores are missing.
int tile_anno_axis_set(KernelEntry *ke, u32 d, TileAxisInfo info) {
  if (ke == NULL) return 0;
  int wrote = 0;
  // Update KernelAxes (the existing source of truth) when present.
  if (ke->axes != NULL && d < ke->axes->n_axes) {
    ke->axes->axis_types[d] = info.kax_type;
    ke->axes->full_shape[d] = info.extent;
    ke->axes->version++;
    if (ke->axes->version == 0) ke->axes->version = 1;
    wrote = 1;
  }
  // Mirror to TILE_AXIS when tile_uops is populated and the axis
  // exists.  This catches the memory_scope + vector_width fields
  // that KernelAxes doesn't carry.
  if (ke->tile_uops != NULL && ke->tile_root != 0
      && ke->tile_root < ke->n_tile_uops) {
    TileUop *root = &ke->tile_uops[ke->tile_root];
    if (root->op == TILE_LOOP_NEST && (u32)d + 1 < (u32)root->src_count) {
      u32 axis_id = root->src[1 + d];
      if (axis_id != 0 && axis_id < ke->n_tile_uops) {
        TileUop *axis = &ke->tile_uops[axis_id];
        if (axis->op == TILE_AXIS) {
          axis->extra = tile_axis_pack(info);
          // Keep tile_axes_version in sync so the freshness check
          // in tile_anno_tile_uops_fresh sees the post-write version.
          if (ke->axes != NULL) {
            ke->tile_axes_version = ke->axes->version;
          }
          wrote = 1;
        }
      }
    }
  }
  return wrote;
}
