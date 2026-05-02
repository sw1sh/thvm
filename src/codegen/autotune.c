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

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

// Number of dispatches per variant.  Larger reduces noise but costs
// real wallclock; 5 is enough to separate small kernels from each
// other against a ~10us-resolution clock.
#define KAUTOTUNE_N_RUNS 5
#define KAUTOTUNE_CACHE_VERSION 1
#define KAUTOTUNE_CACHE_PATH_CAP 1024

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

static int kautotune_cache_disabled(void) {
  char const *disable = getenv("THVM_AUTOTUNE_DISABLE_CACHE");
  if (disable != NULL && disable[0] == '1') {
    return 1;
  }
  char const *cache = getenv("THVM_AUTOTUNE_CACHE");
  if (cache != NULL && cache[0] == '0' && cache[1] == '\0') {
    return 1;
  }
  return 0;
}

static u64 kautotune_hash_bytes(u64 h, void const *ptr, size_t n) {
  u8 const *bytes = (u8 const *)ptr;
  for (size_t i = 0; i < n; i++) {
    h ^= (u64)bytes[i];
    h *= 0x100000001b3ULL;
  }
  return h;
}

static u64 kautotune_hash_u64(u64 h, u64 x) {
  return kautotune_hash_bytes(h, &x, sizeof(x));
}

static u64 kautotune_hash_cstr(u64 h, char const *s) {
  if (s == NULL) {
    return kautotune_hash_u64(h, 0);
  }
  h = kautotune_hash_u64(h, (u64)strlen(s));
  return kautotune_hash_bytes(h, s, strlen(s));
}

static u32 kautotune_backend_id(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) {
    b = DEFAULT_BACKEND;
  }
  return b != NULL ? b->id : 0;
}

static char const *kautotune_backend_name(u32 backend_id) {
  switch (backend_id) {
    case 1:  return "cpu";
    case 2:  return "metal";
    default: return "unknown";
  }
}

static u64 kautotune_structural_key(KernelEntry const *ke) {
  if (ke == NULL) {
    return 0;
  }
  if (ke->scalar_uops != NULL && ke->n_scalar_uops > 0) {
    return kernel_rangeified_key(ke);
  }
  if (ke->program != NULL && ke->n_ops > 0) {
    return kernel_program_key(ke->program, ke->n_ops);
  }
  return kernel_rangeified_key(ke);
}

static u64 kautotune_cache_key(KernelEntry const *ke, KOpt const *candidates,
                               u32 n_cand, u32 n_runs) {
  u64 h = 0xcbf29ce484222325ULL;
  h = kautotune_hash_cstr(h, "thvm-autotune-cache");
  h = kautotune_hash_u64(h, KAUTOTUNE_CACHE_VERSION);
  h = kautotune_hash_u64(h, sizeof(KProgOp));
  h = kautotune_hash_u64(h, sizeof(ScalarUop));
  h = kautotune_hash_u64(h, sizeof(KOpt));
  h = kautotune_hash_u64(h, kautotune_backend_id(ke));
  h = kautotune_hash_u64(h, n_runs);
  h = kautotune_hash_cstr(h, getenv("THVM_TILE"));
  h = kautotune_hash_cstr(h, getenv("THVM_METAL_SPECIALIZED"));
  h = kautotune_hash_u64(h, kautotune_structural_key(ke));

  if (ke != NULL) {
    h = kautotune_hash_u64(h, ke->n_inputs);
    for (u32 i = 0; i < ke->n_inputs; i++) {
      h = kautotune_hash_u64(h, ke->input_dtypes[i]);
      h = kautotune_hash_u64(h, ke->input_numels[i]);
    }
    h = kautotune_hash_u64(h, ke->output_dtype);
    h = kautotune_hash_u64(h, ke->output_numel);
    h = kautotune_hash_u64(h, ke->output_shape.ndim);
    h = kautotune_hash_bytes(h, ke->output_shape.dims,
                             (size_t)ke->output_shape.ndim * sizeof(u32));
    if (ke->axes != NULL) {
      h = kautotune_hash_u64(h, ke->axes->n_axes);
      h = kautotune_hash_bytes(h, ke->axes->axis_types,
                               (size_t)ke->axes->n_axes * sizeof(u8));
      h = kautotune_hash_bytes(h, ke->axes->full_shape,
                               (size_t)ke->axes->n_axes * sizeof(u32));
    }
  }

  h = kautotune_hash_u64(h, n_cand);
  h = kautotune_hash_bytes(h, candidates, (size_t)n_cand * sizeof(KOpt));
  return h == 0 ? 1 : h;
}

static int kautotune_mkdir_one(char const *path) {
  if (path == NULL || path[0] == '\0') {
    return 0;
  }
  if (mkdir(path, 0700) == 0) {
    return 1;
  }
  if (errno != EEXIST) {
    return 0;
  }
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int kautotune_mkdir_p(char const *path) {
  if (path == NULL) {
    return 0;
  }
  size_t n = strlen(path);
  if (n == 0 || n >= KAUTOTUNE_CACHE_PATH_CAP) {
    return 0;
  }
  char tmp[KAUTOTUNE_CACHE_PATH_CAP];
  memcpy(tmp, path, n + 1);
  for (char *p = tmp + 1; *p != '\0'; p++) {
    if (*p != '/') {
      continue;
    }
    *p = '\0';
    if (!kautotune_mkdir_one(tmp)) {
      return 0;
    }
    *p = '/';
  }
  return kautotune_mkdir_one(tmp);
}

static int kautotune_cache_base_dir(char *out, size_t cap) {
  if (out == NULL || cap == 0 || kautotune_cache_disabled()) {
    return 0;
  }
  char const *custom = getenv("THVM_AUTOTUNE_CACHE_DIR");
  if (custom != NULL && custom[0] != '\0') {
    int n = snprintf(out, cap, "%s", custom);
    return n > 0 && (size_t)n < cap;
  }
  char const *xdg = getenv("XDG_CACHE_HOME");
  if (xdg != NULL && xdg[0] != '\0') {
    int n = snprintf(out, cap, "%s/thvm/autotune", xdg);
    return n > 0 && (size_t)n < cap;
  }
  char const *home = getenv("HOME");
  if (home != NULL && home[0] != '\0') {
    int n = snprintf(out, cap, "%s/.cache/thvm/autotune", home);
    return n > 0 && (size_t)n < cap;
  }
  int n = snprintf(out, cap, "/tmp/thvm-autotune");
  return n > 0 && (size_t)n < cap;
}

static int kautotune_cache_path(KernelEntry const *ke, KOpt const *candidates,
                                u32 n_cand, u32 n_runs, char *out,
                                size_t cap, u64 *out_key) {
  char base[KAUTOTUNE_CACHE_PATH_CAP];
  if (!kautotune_cache_base_dir(base, sizeof(base))) {
    return 0;
  }
  if (!kautotune_mkdir_p(base)) {
    return 0;
  }
  u32 backend_id = kautotune_backend_id(ke);
  char dir[KAUTOTUNE_CACHE_PATH_CAP];
  int n = snprintf(dir, sizeof(dir), "%s/%s", base,
                   kautotune_backend_name(backend_id));
  if (n <= 0 || (size_t)n >= sizeof(dir)) {
    return 0;
  }
  if (!kautotune_mkdir_p(dir)) {
    return 0;
  }
  u64 key = kautotune_cache_key(ke, candidates, n_cand, n_runs);
  n = snprintf(out, cap, "%s/%016llx.json", dir,
               (unsigned long long)key);
  if (n <= 0 || (size_t)n >= cap) {
    return 0;
  }
  if (out_key != NULL) {
    *out_key = key;
  }
  return 1;
}

static int kautotune_opt_equal(KOpt a, KOpt b) {
  return a.op == b.op && a.axis == b.axis && a.arg == b.arg;
}

static int kautotune_cached_opt_allowed(KOpt opt, KOpt const *candidates,
                                        u32 n_cand) {
  if (opt.op == KOP_NONE) {
    return 1;
  }
  for (u32 i = 0; i < n_cand; i++) {
    if (kautotune_opt_equal(opt, candidates[i])) {
      return 1;
    }
  }
  return 0;
}

static int kautotune_cache_load(char const *path, u64 expected_key,
                                u32 expected_backend, u32 expected_runs,
                                KOpt *out_opt, u64 *out_us) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    return 0;
  }
  unsigned version = 0;
  unsigned backend = 0;
  unsigned runs    = 0;
  unsigned op      = 0;
  unsigned axis    = 0;
  unsigned long long key     = 0;
  unsigned long long arg     = 0;
  unsigned long long best_us = 0;
  int n = fscanf(f,
      "{\"version\":%u,\"key\":\"%llx\",\"backend\":%u,\"runs\":%u,"
      "\"winner\":{\"op\":%u,\"axis\":%u,\"arg\":%llu},\"best_us\":%llu}",
      &version, &key, &backend, &runs, &op, &axis, &arg, &best_us);
  fclose(f);
  if (n != 8 || version != KAUTOTUNE_CACHE_VERSION
      || (u64)key != expected_key || backend != expected_backend
      || runs != expected_runs || op > 255 || axis > 255
      || arg > 0xFFFFFFFFULL) {
    return 0;
  }
  if (out_opt != NULL) {
    out_opt->op   = (u8)op;
    out_opt->axis = (u8)axis;
    out_opt->arg  = (u32)arg;
  }
  if (out_us != NULL) {
    *out_us = (u64)best_us;
  }
  return 1;
}

static void kautotune_cache_store(char const *path, u64 key, u32 backend_id,
                                  u32 n_runs, KOpt best_opt, u64 best_us) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    return;
  }
  fprintf(f,
          "{\"version\":%u,\"key\":\"%016llx\",\"backend\":%u,\"runs\":%u,"
          "\"winner\":{\"op\":%u,\"axis\":%u,\"arg\":%u},\"best_us\":%llu}\n",
          KAUTOTUNE_CACHE_VERSION, (unsigned long long)key, backend_id,
          n_runs, (unsigned)best_opt.op, (unsigned)best_opt.axis,
          best_opt.arg, (unsigned long long)best_us);
  fclose(f);
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

  u32 n_runs = kautotune_n_runs();
  u32 backend_id = kautotune_backend_id(ke);
  char cache_path[KAUTOTUNE_CACHE_PATH_CAP];
  u64  cache_key = 0;
  int  cache_ready = kautotune_cache_path(ke, candidates, n_cand, n_runs,
                                          cache_path, sizeof(cache_path),
                                          &cache_key);
  if (cache_ready) {
    KOpt cached_opt = { KOP_NONE, 0, 0 };
    u64  cached_us  = 0;
    if (kautotune_cache_load(cache_path, cache_key, backend_id, n_runs,
                             &cached_opt, &cached_us)
        && kautotune_cached_opt_allowed(cached_opt, candidates, n_cand)) {
      (void)cached_us;
      ke->axes->autotuned = 1;
      axes_reset_to_default(ke);
      if (cached_opt.op != KOP_NONE) {
        if (!kernel_apply_tune_candidate(ke, cached_opt)) {
          axes_reset_to_default(ke);
        } else {
          tile_sync_from_scalar(ke);
          return 1;
        }
      } else {
        tile_sync_from_scalar(ke);
        return 0;
      }
    }
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
    int applied = kernel_apply_tune_candidate(ke, best_opt);
    tile_sync_from_scalar(ke);
    if (applied && cache_ready) {
      kautotune_cache_store(cache_path, cache_key, backend_id, n_runs,
                            best_opt, best_us);
    }
    return applied;
  }
  tile_sync_from_scalar(ke);
  if (cache_ready) {
    kautotune_cache_store(cache_path, cache_key, backend_id, n_runs,
                          best_opt, best_us);
  }
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
