// codegen/tile_anno.c - axis annotation read API atop KpSchedule.
//
// Resolves axis count + per-axis TileAxisInfo from the KpSchedule
// signal trio (output_shape + tail-reduce + scalar-reduce + applied_opts).
// Thin wrappers over the axes_resolve_* family.

fn int tile_anno_axis_or_kernelaxes(KernelEntry const *ke, u32 d,
                                    TileAxisInfo *out) {
  if (out == NULL || ke == NULL || ke->schedule == NULL) return 0;
  u32 extent = 0;
  if (!axes_resolve_full_shape(ke, d, &extent)) return 0;
  out->kax_type     = axes_resolve_kax_type(ke, d);
  out->extent       = extent;
  out->memory_scope = 0;
  out->vector_width = 0;
  return 1;
}

fn u32 tile_anno_axis_count_or_kernelaxes(KernelEntry const *ke) {
  return axes_resolve_n_axes(ke);
}

// applied_opts facade.  External linkage so the metal backend
// (compiled as a separate translation unit, backend_metal.o) can
// call these.
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

// Writer-side facade: thin wrapper over kernel_apply_opt.  Kept for
// the WL FFI (thvm_wl_kernel_apply_opt) and autotune callsites.
int tile_anno_apply_opt(KernelEntry *ke, KOpt opt) {
  return kernel_apply_opt(ke, opt);
}

// Reset the axes back to the default LOOP/REDUCE shape (used between
// autotune bench candidates so each candidate starts from a fresh
// baseline).  Preserves the autotuned flag.
void tile_anno_axes_reset(KernelEntry *ke) {
  if (ke == NULL || ke->schedule == NULL) return;
  u8  autotuned = ke->schedule->autotuned;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  ke->schedule->autotuned = autotuned;
}

