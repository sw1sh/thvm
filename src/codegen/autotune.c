// codegen/autotune.c -- per-kernel opt benchmarker.
//
// Given a kernel, walk the proposer's candidates, time each variant
// (n_runs back-to-back fires + min wallclock), optionally expand the
// best single opts into short sequences, pick the winner, and leave
// the kernel's KpSchedule mutated to the winning opt sequence.  The
// on-disk autotune cache is keyed by a structural FNV hash of the
// kernel program / scalar-uop graph so reruns on the same shape pick
// up the previous winner.
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
#define KAUTOTUNE_CACHE_VERSION 2
#define KAUTOTUNE_CACHE_PATH_CAP 1024
#define KAUTOTUNE_MAX_CANDIDATES 16
#define KAUTOTUNE_SEQ_MAX 4
#define KAUTOTUNE_MAX_SEQS 64

typedef struct {
  u8   n;
  KOpt opts[KAUTOTUNE_SEQ_MAX];
} KOptSeq;

typedef struct {
  KOptSeq seq;
  u64     us;
} KOptSeqBench;

static u32 kautotune_backend_id(KernelEntry const *ke);

static u32 kautotune_n_runs(void) {
  char const *e = getenv("BEAM_RUNS");
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

static u32 kautotune_env_u32(char const *name, u32 fallback, u32 max) {
  char const *e = getenv(name);
  if (e == NULL || e[0] == '\0') {
    return fallback;
  }
  int n = atoi(e);
  if (n <= 0) {
    return fallback;
  }
  if ((u32)n > max) {
    return max;
  }
  return (u32)n;
}

static int kautotune_metal_tile_enabled(void) {
  char const *tile = getenv("THVM_TILE");
  return tile != NULL && tile[0] == '1';
}

static u32 kautotune_default_depth(KernelEntry const *ke) {
  if (kautotune_backend_id(ke) == 2 && kautotune_metal_tile_enabled()) {
    TileConv2DInfo conv;
    if (tile_analyze_conv2d_flat(ke, &conv)) {
      return 2;
    }
  }
  return 1;
}

static u32 kautotune_depth(KernelEntry const *ke) {
  return kautotune_env_u32("AUTOTUNE_DEPTH",
                           kautotune_default_depth(ke),
                           KAUTOTUNE_SEQ_MAX);
}

static u32 kautotune_beam_width(void) {
  return kautotune_env_u32("BEAM", 2, 16);
}

static int kautotune_cache_disabled(void) {
  char const *disable = getenv("AUTOTUNE_DISABLE");
  if (disable != NULL && disable[0] == '1') {
    return 1;
  }
  char const *cache = getenv("AUTOTUNE_CACHE");
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

static u64 kautotune_rangeified_key(KernelEntry const *ke) {
  if (ke == NULL) {
    return 0;
  }
  u64 h = 0xcbf29ce484222325ULL ^ 0x52414E4745584B41ULL;
  if (ke->schedule != NULL && axes_resolve_n_axes(ke) > 0) {
    h = kautotune_hash_u64(h, 2);
    h = kautotune_hash_u64(h, (u64)term_tag(ke->source_uop));
    h = kautotune_hash_u64(h, (u64)term_ext(ke->source_uop));
    h = tile_anno_hash_axes(ke, h);
  } else {
    return 0;
  }
  h = kautotune_hash_u64(h, (u64)ke->n_inputs);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h = kautotune_hash_u64(h, (u64)ke->input_dtypes[i]);
    h = kautotune_hash_u64(h, (u64)ke->input_numels[i]);
  }
  h = kautotune_hash_u64(h, (u64)ke->output_dtype);
  h = kautotune_hash_u64(h, (u64)ke->output_numel);
  h = kautotune_hash_u64(h, (u64)ke->output_shape.ndim);
  h = kautotune_hash_bytes(h, ke->output_shape.dims,
                           (size_t)ke->output_shape.ndim * sizeof(u32));
  return (h & 0x3FFFFFFFFFFFFFFFULL) | (1ULL << 62);
}

fn u64 kautotune_structural_key(KernelEntry const *ke) {
  if (ke == NULL) {
    return 0;
  }
  return kautotune_rangeified_key(ke);
}

static u64 kautotune_cache_key(KernelEntry const *ke, KOpt const *candidates,
                               u32 n_cand, u32 n_runs, u32 depth,
                               u32 beam_width) {
  u64 h = 0xcbf29ce484222325ULL;
  h = kautotune_hash_cstr(h, "thvm-autotune-cache");
  h = kautotune_hash_u64(h, KAUTOTUNE_CACHE_VERSION);
  h = kautotune_hash_u64(h, sizeof(KernelEntry));
  h = kautotune_hash_u64(h, sizeof(KOpt));
  h = kautotune_hash_u64(h, kautotune_backend_id(ke));
  h = kautotune_hash_u64(h, n_runs);
  h = kautotune_hash_u64(h, depth);
  h = kautotune_hash_u64(h, beam_width);
  h = kautotune_hash_cstr(h, getenv("THVM_TILE"));
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
    if (ke->schedule != NULL) {
      // Phase E: hash via tile_anno's shared axes-hash helper.
      h = tile_anno_hash_axes(ke, h);
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
  char const *custom = getenv("AUTOTUNE_CACHE_DIR");
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
                                size_t cap, u64 *out_key, u32 depth,
                                u32 beam_width) {
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
  u64 key = kautotune_cache_key(ke, candidates, n_cand, n_runs, depth,
                                beam_width);
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

static int kautotune_cached_seq_allowed(KOptSeq const *seq,
                                        KOpt const *candidates,
                                        u32 n_cand) {
  if (seq == NULL || seq->n == 0) {
    return 1;
  }
  if (seq->n > KAUTOTUNE_SEQ_MAX) {
    return 0;
  }
  for (u32 s = 0; s < seq->n; s++) {
    int found = 0;
    for (u32 i = 0; i < n_cand; i++) {
      if (kautotune_opt_equal(seq->opts[s], candidates[i])) {
        found = 1;
        break;
      }
    }
    if (!found) {
      return 0;
    }
  }
  return 1;
}

static int kautotune_cache_load(char const *path, u64 expected_key,
                                u32 expected_backend, u32 expected_runs,
                                u32 expected_depth, u32 expected_beam,
                                KOptSeq *out_seq, u64 *out_us) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    return 0;
  }
  unsigned version = 0;
  unsigned backend = 0;
  unsigned runs    = 0;
  unsigned depth   = 0;
  unsigned beam    = 0;
  unsigned n_opts  = 0;
  unsigned op[KAUTOTUNE_SEQ_MAX]   = {0};
  unsigned axis[KAUTOTUNE_SEQ_MAX] = {0};
  unsigned long long arg[KAUTOTUNE_SEQ_MAX] = {0};
  unsigned long long key     = 0;
  unsigned long long best_us = 0;
  int n = fscanf(f,
      "{\"version\":%u,\"key\":\"%llx\",\"backend\":%u,\"runs\":%u,"
      "\"depth\":%u,\"beam\":%u,\"n_opts\":%u,"
      "\"opts\":[%u,%u,%llu,%u,%u,%llu,%u,%u,%llu,%u,%u,%llu],"
      "\"best_us\":%llu}",
      &version, &key, &backend, &runs, &depth, &beam, &n_opts,
      &op[0], &axis[0], &arg[0], &op[1], &axis[1], &arg[1],
      &op[2], &axis[2], &arg[2], &op[3], &axis[3], &arg[3],
      &best_us);
  fclose(f);
  if (n != 20 || version != KAUTOTUNE_CACHE_VERSION
      || (u64)key != expected_key || backend != expected_backend
      || runs != expected_runs || depth != expected_depth
      || beam != expected_beam || n_opts > KAUTOTUNE_SEQ_MAX) {
    return 0;
  }
  KOptSeq seq = {0};
  seq.n = (u8)n_opts;
  for (u32 i = 0; i < n_opts; i++) {
    if (op[i] > 255 || axis[i] > 255 || arg[i] > 0xFFFFFFFFULL) {
      return 0;
    }
    seq.opts[i].op   = (u8)op[i];
    seq.opts[i].axis = (u8)axis[i];
    seq.opts[i].arg  = (u32)arg[i];
  }
  if (out_seq != NULL) {
    *out_seq = seq;
  }
  if (out_us != NULL) {
    *out_us = (u64)best_us;
  }
  return 1;
}

static void kautotune_cache_store(char const *path, u64 key, u32 backend_id,
                                  u32 n_runs, u32 depth, u32 beam_width,
                                  KOptSeq best_seq, u64 best_us) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    return;
  }
  KOpt opt[KAUTOTUNE_SEQ_MAX] = {0};
  u32 n = best_seq.n;
  if (n > KAUTOTUNE_SEQ_MAX) {
    n = KAUTOTUNE_SEQ_MAX;
  }
  for (u32 i = 0; i < n; i++) {
    opt[i] = best_seq.opts[i];
  }
  fprintf(f,
          "{\"version\":%u,\"key\":\"%016llx\",\"backend\":%u,\"runs\":%u,"
          "\"depth\":%u,\"beam\":%u,\"n_opts\":%u,"
          "\"opts\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u],"
          "\"best_us\":%llu}\n",
          KAUTOTUNE_CACHE_VERSION, (unsigned long long)key, backend_id,
          n_runs, depth, beam_width, n,
          (unsigned)opt[0].op, (unsigned)opt[0].axis, opt[0].arg,
          (unsigned)opt[1].op, (unsigned)opt[1].axis, opt[1].arg,
          (unsigned)opt[2].op, (unsigned)opt[2].axis, opt[2].arg,
          (unsigned)opt[3].op, (unsigned)opt[3].axis, opt[3].arg,
          (unsigned long long)best_us);
  fclose(f);
}

// Reset axes->applied_opts[] back to empty.  Recomputes default
// axis_types/full_shape from the kernel's output_shape + tail
// REDUCE so subsequent axes_apply_opt sees a clean slate.
// Preserves the `autotuned` flag so the fire-time autotune trigger
// doesn't re-fire while we're benching variants of an already-
// tuned kernel.
static void axes_reset_to_default(KernelEntry *ke) {
  tile_anno_axes_reset(ke);
  // kernel_apply_opt's DAG-mode mutates cached_lift.store_root in
  // place.  Revert it to the materialize-time snapshot so DAG
  // readers see the baseline state.
  if (ke != NULL && ke->cached_lift_init_root != 0) {
    ke->cached_lift.store_root = ke->cached_lift_init_root;
  }
}

static int kernel_apply_tune_candidate(KernelEntry *ke, KOpt opt) {
  if (ke == NULL || ke->schedule == NULL) {
    return 0;
  }
  if (opt.op != KOP_LOCAL) {
    return tile_anno_apply_opt(ke, opt);
  }
  if (!tile_anno_apply_opt(ke, opt)) {
    return 0;
  }
  TileAxisInfo info;
  if (opt.axis >= tile_anno_axis_count_or_kernelaxes(ke)
      || !tile_anno_axis_or_kernelaxes(ke, opt.axis, &info)
      || info.kax_type != KAX_LOOP) {
    return 0;
  }
  KOpt global = {
    .op   = KOP_GLOBAL,
    .axis = opt.axis,
    .arg  = info.extent,
  };
  return tile_anno_apply_opt(ke, global);
}

static void kernel_bench_fire(u32 kid);

static int kautotune_seq_has_op(KOptSeq const *seq, u8 op) {
  for (u32 i = 0; seq != NULL && i < seq->n; i++) {
    if (seq->opts[i].op == op) {
      return 1;
    }
  }
  return 0;
}

static int kautotune_seq_contains_opt(KOptSeq const *seq, KOpt opt) {
  for (u32 i = 0; seq != NULL && i < seq->n; i++) {
    if (kautotune_opt_equal(seq->opts[i], opt)) {
      return 1;
    }
  }
  return 0;
}

static int kautotune_seq_equal(KOptSeq const *a, KOptSeq const *b) {
  if (a == NULL || b == NULL || a->n != b->n) {
    return 0;
  }
  for (u32 i = 0; i < a->n; i++) {
    if (!kautotune_opt_equal(a->opts[i], b->opts[i])) {
      return 0;
    }
  }
  return 1;
}

static int kautotune_seq_seen(KOptSeq const *seq, KOptSeq const *seen,
                              u32 n_seen) {
  for (u32 i = 0; i < n_seen; i++) {
    if (kautotune_seq_equal(seq, &seen[i])) {
      return 1;
    }
  }
  return 0;
}

static int kautotune_seq_can_append(KOptSeq const *seq, KOpt opt) {
  if (seq == NULL || seq->n >= KAUTOTUNE_SEQ_MAX) {
    return 0;
  }
  if (kautotune_seq_contains_opt(seq, opt)) {
    return 0;
  }
  if (opt.op == KOP_TC) {
    return seq->n == 0;
  }
  if (opt.op == KOP_LOCAL && kautotune_seq_has_op(seq, opt.op)) {
    return 0;
  }
  return 1;
}

static int kautotune_apply_seq(KernelEntry *ke, KOptSeq const *seq) {
  if (ke == NULL || ke->schedule == NULL || seq == NULL) {
    return 0;
  }
  if (seq->n == 0) {
    return 1;
  }
  for (u32 i = 0; i < seq->n; i++) {
    if (!kernel_apply_tune_candidate(ke, seq->opts[i])) {
      return 0;
    }
  }
  return 1;
}

static void kautotune_insert_top(KOptSeqBench *top, u32 *n_top, u32 cap,
                                 KOptSeq seq, u64 us) {
  if (cap == 0 || top == NULL || n_top == NULL) {
    return;
  }
  u32 n = *n_top;
  if (n < cap) {
    top[n].seq = seq;
    top[n].us  = us;
    n++;
  } else if (us < top[n - 1].us) {
    top[n - 1].seq = seq;
    top[n - 1].us  = us;
  } else {
    return;
  }
  for (u32 i = n - 1; i > 0 && top[i].us < top[i - 1].us; i--) {
    KOptSeqBench tmp = top[i - 1];
    top[i - 1] = top[i];
    top[i] = tmp;
  }
  *n_top = n;
}

static int kautotune_bench_seq(u32 kid, KernelEntry *ke, KOptSeq const *seq,
                               u32 n_runs, u64 *out_us) {
  axes_reset_to_default(ke);
  if (!kautotune_apply_seq(ke, seq)) {
    axes_reset_to_default(ke);
    return 0;
  }
  kernel_bench_fire(kid);
  if (out_us != NULL) {
    *out_us = kernel_bench_us(kid, n_runs);
  }
  return 1;
}

static void kernel_bench_fire(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) {
    return;
  }
  KernelEntry *ke = &KERNELS[kid];
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
  if (ke->schedule == NULL) {
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
  if (ke->schedule == NULL) {
    return 0;
  }

  KOpt candidates[KAUTOTUNE_MAX_CANDIDATES];
  u32  n_cand = kernel_opts_propose(ke, candidates,
                                    sizeof(candidates)/sizeof(*candidates));
  if (n_cand == 0) {
    // No candidates -- still mark autotuned so the fire-time trigger
    // doesn't re-propose every dispatch.
    if (ke->schedule != NULL) {
      ke->schedule->autotuned = 1;
    }
    return 0;
  }

  u32 n_runs    = kautotune_n_runs();
  u32 depth     = kautotune_depth(ke);
  u32 beam_width = kautotune_beam_width();
  u32 backend_id = kautotune_backend_id(ke);
  char cache_path[KAUTOTUNE_CACHE_PATH_CAP];
  u64  cache_key = 0;
  int  cache_ready = kautotune_cache_path(ke, candidates, n_cand, n_runs,
                                          cache_path, sizeof(cache_path),
                                          &cache_key, depth, beam_width);
  if (cache_ready) {
    KOptSeq cached_seq = {0};
    if (kautotune_cache_load(cache_path, cache_key, backend_id, n_runs,
                             depth, beam_width, &cached_seq, NULL)
        && kautotune_cached_seq_allowed(&cached_seq, candidates, n_cand)) {
      ke->schedule->autotuned = 1;
      axes_reset_to_default(ke);
      if (cached_seq.n != 0) {
        if (kautotune_apply_seq(ke, &cached_seq)) {
          return 1;
        }
        axes_reset_to_default(ke);
      } else {
        return 0;
      }
    }
  }

  // Mark autotuned at the START so nested/direct bench dispatches do
  // not re-enter this path if a backend helper fires through the public
  // kernel path.  axes_reset_to_default preserves the flag.
  ke->schedule->autotuned = 1;

  // Baseline (no opts).  Warm the JIT so the first variant doesn't
  // pay the compile cost alone -- compile each before timing the
  // bench loop, so the measurements compare hot kernels.
  axes_reset_to_default(ke);
  kernel_bench_fire(kid);
  u64 best_us = kernel_bench_us(kid, n_runs);
  KOptSeq best_seq = {0};

  if (depth == 0) {
    depth = 1;
  }
  if (beam_width == 0) {
    beam_width = 1;
  }
  if (beam_width > KAUTOTUNE_MAX_SEQS) {
    beam_width = KAUTOTUNE_MAX_SEQS;
  }

  KOptSeqBench beam[KAUTOTUNE_MAX_SEQS];
  KOptSeqBench next[KAUTOTUNE_MAX_SEQS];
  KOptSeq      seen[KAUTOTUNE_MAX_SEQS];
  u32 n_beam = 0;
  u32 n_seen = 0;

  for (u32 i = 0; i < n_cand; i++) {
    KOptSeq seq = {0};
    seq.n = 1;
    seq.opts[0] = candidates[i];
    seen[n_seen++] = seq;
    u64 us = 0;
    if (!kautotune_bench_seq(kid, ke, &seq, n_runs, &us)) {
      continue;
    }
    if (us < best_us) {
      best_us  = us;
      best_seq = seq;
    }
    kautotune_insert_top(beam, &n_beam, beam_width, seq, us);
  }

  for (u32 d = 2; d <= depth && n_beam > 0; d++) {
    u32 n_next = 0;
    for (u32 b = 0; b < n_beam; b++) {
      for (u32 i = 0; i < n_cand; i++) {
        if (!kautotune_seq_can_append(&beam[b].seq, candidates[i])) {
          continue;
        }
        KOptSeq seq = beam[b].seq;
        seq.opts[seq.n++] = candidates[i];
        if (kautotune_seq_seen(&seq, seen, n_seen)) {
          continue;
        }
        if (n_seen < KAUTOTUNE_MAX_SEQS) {
          seen[n_seen++] = seq;
        }
        u64 us = 0;
        if (!kautotune_bench_seq(kid, ke, &seq, n_runs, &us)) {
          continue;
        }
        if (us < best_us) {
          best_us  = us;
          best_seq = seq;
        }
        kautotune_insert_top(next, &n_next, beam_width, seq, us);
      }
    }
    memcpy(beam, next, (size_t)n_next * sizeof(*beam));
    n_beam = n_next;
    if (n_seen >= KAUTOTUNE_MAX_SEQS) {
      break;
    }
  }

  // Re-apply the winner (or leave baseline if nothing beat it).
  // `autotuned` was set at the start; reset_to_default preserves it.
  axes_reset_to_default(ke);
  if (best_seq.n != 0) {
    int applied = kautotune_apply_seq(ke, &best_seq);
    if (applied && cache_ready) {
      kautotune_cache_store(cache_path, cache_key, backend_id, n_runs,
                            depth, beam_width, best_seq, best_us);
    }
    return applied;
  }
  if (cache_ready) {
    kautotune_cache_store(cache_path, cache_key, backend_id, n_runs,
                          depth, beam_width, best_seq, best_us);
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
  char const *e = getenv("AUTOTUNE");
  if (e != NULL && e[0] == '1') {
    return 1;
  }
  // tinygrad folds the autotune on/off switch into BEAM: any positive
  // BEAM width runs beam search.
  char const *beam = getenv("BEAM");
  return beam != NULL && atoi(beam) > 0;
}

fn int kernel_should_autotune(KernelEntry const *ke) {
  if (!autotune_env_enabled()) {
    return 0;
  }
  if (ke == NULL || ke->schedule == NULL) {
    return 0;
  }
  if (ke->schedule->autotuned) {
    return 0;
  }
  KOpt buf[16];
  return kernel_opts_propose(ke, buf, sizeof(buf)/sizeof(*buf)) > 0;
}
