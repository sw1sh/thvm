// codegen/tile_anno.c - the applied-opts log and axis hash the Metal
// backend and autotune read.

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
  u8  kax[MAX_AXES];
  u32 ext[MAX_AXES];
  u32 n = axes_collect(ke, kax, ext, MAX_AXES);
  h ^= (u64)n;
  h *= 0x100000001b3ULL;
  for (u32 i = 0; i < n; i++) {
    h ^= (u64)kax[i];
    h *= 0x100000001b3ULL;
    h ^= (u64)ext[i];
    h *= 0x100000001b3ULL;
  }
  return h;
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

