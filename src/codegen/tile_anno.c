// codegen/tile_anno.c - the applied-opts log and axis hash the Metal
// backend and autotune read.

// External linkage: backend/metal/_.m, a separate translation unit, reads these.
u32 tile_anno_applied_opts_count(KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) return 0;
  return ke->schedule->n_applied;
}

KOpt const *tile_anno_applied_opts(KernelEntry const *ke) {
  if (ke == NULL || ke->schedule == NULL) return NULL;
  return ke->schedule->applied_opts;
}

// Fold the axis count and each axis' (type, extent) into FNV-1a state `h`, for
// autotune's cache key.
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

// Zero the schedule but its `autotuned` flag, between autotune candidates; the
// caller resets the DAG itself.
void tile_anno_axes_reset(KernelEntry *ke) {
  if (ke == NULL || ke->schedule == NULL) return;
  u8  autotuned = ke->schedule->autotuned;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  ke->schedule->autotuned = autotuned;
}

