// codegen/autotune.c -- per-program-shape opt benchmarker.
//
// Given a kernel, walk the proposer's candidates, time each variant
// (n_runs back-to-back fires + min wallclock), pick the winner, and
// leave the kernel's KernelAxes mutated to the winning opt.  Because
// axes live on the shared KpCacheSlot (Phase 16 per-program-shape
// sharing), the pick automatically applies to every other kid with
// the same KProgOp[] -- a training loop that emits one new kid per
// step inherits the autotuned variant on iter 2+.
//
// Reset semantics: each variant is benched against the SAME baseline
// axes (no opts).  Composite proposals (UNROLL=4 then UPCAST=2) are
// not supported in this MVP -- proposers return single-opt candidates
// only.  When a multi-opt proposer arrives it should append a new
// outer loop here that explores combinations.

// Number of dispatches per variant.  Larger reduces noise but costs
// real wallclock; 5 is enough to separate small kernels from each
// other against a ~10us-resolution clock.
#define KAUTOTUNE_N_RUNS 5

// Reset axes->applied_opts[] back to empty.  Recomputes default
// axis_types/full_shape from the kernel's output_shape + tail
// REDUCE so subsequent axes_apply_opt sees a clean slate.
static void axes_reset_to_default(KernelEntry *ke) {
  if (ke->axes == NULL) return;
  memset(ke->axes, 0, sizeof(KernelAxes));
  axes_default_for(ke);
}

// Time `n_runs` back-to-back kernel_fire_by_id calls; return min
// wallclock in microseconds.  Min (not mean) filters one-shot OS
// jitter (page faults, scheduler hiccups).
static u64 autotune_time_kernel(u32 kid, u32 n_runs) {
  if (n_runs == 0) n_runs = 1;
  u64 best = (u64)-1;
  for (u32 i = 0; i < n_runs; i++) {
    u64 t0 = cg_now_us();
    kernel_fire_by_id(kid);
    u64 dt = cg_now_us() - t0;
    if (dt < best) best = dt;
  }
  return best;
}

// Run propose -> bench -> apply-winner.  Returns 1 if a winning opt
// was applied, 0 if no opt beat baseline (or no proposer candidates).
// Idempotent: calling twice on the same kid runs the bench again
// (bench numbers may differ; that's OK, the winner converges).
fn int kernel_autotune(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) return 0;
  KernelEntry *ke = &KERNELS[kid];
  if (ke->axes == NULL) return 0;

  KOpt candidates[16];
  u32  n_cand = kernel_opts_propose(ke, candidates,
                                    sizeof(candidates)/sizeof(*candidates));
  if (n_cand == 0) return 0;

  // Baseline (no opts).
  axes_reset_to_default(ke);
  // Warm the JIT so the first variant doesn't pay the compile cost
  // alone -- compile each before timing the bench loop, so the
  // measurements compare hot kernels.
  kernel_fire_by_id(kid);
  u64 best_us = autotune_time_kernel(kid, KAUTOTUNE_N_RUNS);
  KOpt best_opt = { KOP_NONE, 0, 0 };

  for (u32 i = 0; i < n_cand; i++) {
    axes_reset_to_default(ke);
    if (!axes_apply_opt(ke->axes, candidates[i])) continue;
    kernel_fire_by_id(kid);                     // JIT warm
    u64 us = autotune_time_kernel(kid, KAUTOTUNE_N_RUNS);
    if (us < best_us) {
      best_us  = us;
      best_opt = candidates[i];
    }
  }

  // Re-apply the winner (or leave baseline if nothing beat it).
  axes_reset_to_default(ke);
  if (best_opt.op != KOP_NONE) {
    axes_apply_opt(ke->axes, best_opt);
    return 1;
  }
  return 0;
}
