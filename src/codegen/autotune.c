// codegen/autotune.c -- per-program-shape opt benchmarker.
//
// Given a kernel, walk the proposer's candidates, time each variant
// (n_runs back-to-back fires + min wallclock), pick the winner, and
// leave the kernel's KernelAxes mutated to the winning opt.  Axes
// live on the shared KpCacheSlot (autotune knobs cached per-program-
// shape), so the pick applies to every other kid with the same
// KProgOp[] -- a training loop that emits one new kid per step
// inherits the autotuned variant on iter 2+.
//
// Reset semantics: each variant is benched against the SAME baseline
// axes (no opts).  Proposers return a single visible KOpt per
// candidate; the Metal tile LOCAL candidate is internally expanded to
// LOCAL + matching GLOBAL because the renderer needs both bindings.

// Number of dispatches per variant.  Larger reduces noise but costs
// real wallclock; 5 is enough to separate small kernels from each
// other against a ~10us-resolution clock.
#define KAUTOTUNE_N_RUNS 5

static u32 kautotune_n_runs(void) {
  char const *e = getenv("THVM_AUTOTUNE_RUNS");
  if (e == NULL || e[0] == '\0') {
    return KAUTOTUNE_N_RUNS;
  }
  int n = atoi(e);
  if (n <= 0) {
    return KAUTOTUNE_N_RUNS;
  }
  if (n > 1000) {
    return 1000;
  }
  return (u32)n;
}

// Reset axes->applied_opts[] back to empty.  Recomputes default
// axis_types/full_shape from the kernel's output_shape + tail
// REDUCE so subsequent axes_apply_opt sees a clean slate.
// Preserves the `autotuned` flag so the fire-time autotune trigger
// doesn't re-fire while we're benching variants of an already-
// tuned kernel.
static void axes_reset_to_default(KernelEntry *ke) {
  if (ke->axes == NULL) {
    return;
  }
  u8 autotuned = ke->axes->autotuned;
  u32 version  = ke->axes->version;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->autotuned = autotuned;
  ke->axes->version   = version;
  axes_default_for(ke);
}

static int kernel_apply_tune_candidate(KernelEntry *ke, KOpt opt) {
  if (ke == NULL || ke->axes == NULL) {
    return 0;
  }
  if (opt.op != KOP_LOCAL) {
    return kernel_apply_opt(ke, opt);
  }
  if (!axes_apply_opt(ke->axes, opt)) {
    return 0;
  }
  if (opt.axis >= ke->axes->n_axes
      || ke->axes->axis_types[opt.axis] != KAX_LOOP) {
    return 0;
  }
  KOpt global = {
    .op   = KOP_GLOBAL,
    .axis = opt.axis,
    .arg  = ke->axes->full_shape[opt.axis],
  };
  return axes_apply_opt(ke->axes, global);
}

static void kernel_bench_fire(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) {
    return;
  }
  KernelEntry *ke = &KERNELS[kid];
  if (ke->spliced) {
    return;
  }
  u32 resolved_tids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 && ke->input_terms[i] != 0) {
      Term r = term_resolve(ke->input_terms[i]);
      if (term_tag(r) != TAG_TEN) {
        return;
      }
      tid = (u32)term_val(r);
    }
    if (tid == 0 || tid >= TENS_NEXT) {
      return;
    }
    resolved_tids[i] = tid;
  }
  if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) {
    return;
  }
  u32 in_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    in_buf_ids[i] = TENS[resolved_tids[i]].buf_id;
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;
  Backend *b = TENS[ke->output_tid].backend;
  if (b == NULL || b->dispatch_kernel == NULL) {
    return;
  }
  jit_capture_pause();
  backend_dispatch_flush_all();
  b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
  backend_dispatch_flush_all();
  jit_capture_resume();
  ITRS++;
}

// Time `n_runs` back-to-back direct dispatches of `kid`; return min
// wallclock in microseconds.  Min (not mean) filters one-shot OS jitter
// (page faults, scheduler hiccups).
fn u64 kernel_bench_us(u32 kid, u32 n_runs) {
  if (n_runs == 0) {
    n_runs = 1;
  }
  u64 best = (u64)-1;
  for (u32 i = 0; i < n_runs; i++) {
    u64 t0 = cg_now_us();
    kernel_bench_fire(kid);
    u64 dt = cg_now_us() - t0;
    if (dt < best) best = dt;
  }
  return best;
}

// Inspect-only sibling of kernel_autotune: bench the no-opt
// baseline + each proposer candidate, write the per-variant
// (op, axis, arg, us) tuple into out[].  Slot 0 is the baseline
// (op = KOP_NONE).  Returns the number of slots written
// (1 baseline + n_cand).  Restores axes to baseline at exit so
// the user can pick what to apply via TKernelApplyOpt.
fn u32 kernel_bench_variants(u32 kid, KOpt *out_opts, u64 *out_us, u32 cap) {
  if (kid == 0 || kid >= KERNELS_NEXT || cap == 0) {
    return 0;
  }
  KernelEntry *ke = &KERNELS[kid];
  if (ke->axes == NULL) {
    return 0;
  }

  KOpt cands[16];
  u32 n_cand = kernel_opts_propose(ke, cands, sizeof(cands)/sizeof(*cands));
  u32 n_out  = 1 + n_cand;
  if (n_out > cap) n_out = cap;

  // Baseline first.
  axes_reset_to_default(ke);
  kernel_bench_fire(kid);                         // JIT warm
  out_opts[0] = (KOpt){ KOP_NONE, 0, 0 };
  u32 n_runs = kautotune_n_runs();
  out_us  [0] = kernel_bench_us(kid, n_runs);

  // Each candidate.
  for (u32 i = 0; i + 1 < n_out; i++) {
    axes_reset_to_default(ke);
    if (!kernel_apply_tune_candidate(ke, cands[i])) {
      out_opts[i + 1] = (KOpt){ KOP_NONE, 0, 0 };
      out_us  [i + 1] = 0;
      continue;
    }
    kernel_bench_fire(kid);
    out_opts[i + 1] = cands[i];
    out_us  [i + 1] = kernel_bench_us(kid, n_runs);
  }

  // Leave at baseline.
  axes_reset_to_default(ke);
  tile_sync_from_scalar(ke);
  return n_out;
}

// Run propose -> bench -> apply-winner.  Returns 1 if a winning opt
// was applied, 0 if no opt beat baseline (or no proposer candidates).
// Idempotent: calling twice on the same kid runs the bench again
// (bench numbers may differ; that's OK, the winner converges).
fn int kernel_autotune(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) {
    return 0;
  }
  KernelEntry *ke = &KERNELS[kid];
  if (ke->axes == NULL) {
    return 0;
  }

  KOpt candidates[16];
  u32  n_cand = kernel_opts_propose(ke, candidates,
                                    sizeof(candidates)/sizeof(*candidates));
  if (n_cand == 0) {
    // No candidates -- still mark autotuned so the fire-time trigger
    // doesn't re-propose every dispatch.
    if (ke->axes != NULL) {
      ke->axes->autotuned = 1;
    }
    return 0;
  }

  // Mark autotuned at the START so nested/direct bench dispatches do
  // not re-enter this path if a backend helper fires through the public
  // kernel path.  axes_reset_to_default preserves the flag.
  ke->axes->autotuned = 1;

  // Baseline (no opts).
  axes_reset_to_default(ke);
  // Warm the JIT so the first variant doesn't pay the compile cost
  // alone -- compile each before timing the bench loop, so the
  // measurements compare hot kernels.
  kernel_bench_fire(kid);
  u32 n_runs = kautotune_n_runs();
  u64 best_us = kernel_bench_us(kid, n_runs);
  KOpt best_opt = { KOP_NONE, 0, 0 };

  for (u32 i = 0; i < n_cand; i++) {
    axes_reset_to_default(ke);
    if (!kernel_apply_tune_candidate(ke, candidates[i])) continue;
    kernel_bench_fire(kid);                     // JIT warm
    u64 us = kernel_bench_us(kid, n_runs);
    if (us < best_us) {
      best_us  = us;
      best_opt = candidates[i];
    }
  }

  // Re-apply the winner (or leave baseline if nothing beat it).
  // `autotuned` was set at the start; reset_to_default preserves it.
  axes_reset_to_default(ke);
  if (best_opt.op != KOP_NONE) {
    kernel_apply_tune_candidate(ke, best_opt);
    tile_sync_from_scalar(ke);
    return 1;
  }
  tile_sync_from_scalar(ke);
  return 0;
}

// Should this kernel auto-tune on its next fire?  Three conditions:
// the env opt-in is on, the per-program-shape `autotuned` flag is
// still 0, and the proposer would offer at least one candidate
// (otherwise autotune is a guaranteed no-op).  Cheap: env check
// memoizes; the propose call returns quickly when the kernel
// shape doesn't trigger any rules.
static int autotune_env_enabled(void) {
  char const *e = getenv("THVM_AUTOTUNE");
  return e != NULL && e[0] == '1';
}

fn int kernel_should_autotune(KernelEntry const *ke) {
  if (!autotune_env_enabled()) {
    return 0;
  }
  if (ke == NULL || ke->axes == NULL) {
    return 0;
  }
  if (ke->axes->autotuned) {
    return 0;
  }
  KOpt buf[16];
  return kernel_opts_propose(ke, buf, sizeof(buf)/sizeof(*buf)) > 0;
}
