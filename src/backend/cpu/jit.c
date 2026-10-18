// backend/cpu/jit.c - shell out to the C compiler to compile a generated
// C source into a .dylib, dlopen it, and cache the resolved
// `void k(...)` symbol against the program's hash so subsequent
// dispatches skip both compilation and dlsym.
//
// Per-kernel function signature (emitted by cg_render_uop_kernel_c):
//   void k(void *out_v,
//          const void *const *ins_v,
//          unsigned out_numel,
//          const unsigned *in_numels,
//          const unsigned *kvar_vals);   // symbolic-shape loop bounds
//
// The renderer casts `out_v` and each `ins_v[i]` to the program's
// dtype-typed pointer in the prologue, so the generated kernel
// stays uniform across dtypes from the caller's POV.
//
// Cache key: the hash of the rendered source (cpu_src_hash), so a
// structurally identical kernel reuses the build across steps, and the
// .dylib on disk across processes.

#ifndef _WIN32
#include <dlfcn.h>  // Windows: dlopen family shimmed in util/portable_win.h
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#else
#include <process.h>  // _spawnvp
#endif
#include <unistd.h>
#include <sys/stat.h>

// `kvar_vals` carries the per-dispatch symbolic-shape bindings: kvar_vals[i]
// is the loop bound for the i-th kvar of this kernel in kvar_collect_from_dag
// order (the same order render_uop declares `unsigned V_<name> = kvar_vals[i]`).
// Empty / ignored for non-symbolic kernels.
typedef void (*CpuJitFn)(void *out, const void *const *ins,
                         unsigned n, const unsigned *in_numels,
                         const unsigned *kvar_vals);

// Fill `out_vals` with this kernel's kvar runtime bounds (kvar_collect_from_dag
// order).  out_vals must hold KVAR_USED_CAP entries.  ZERO-INIT first: an
// unfilled slot must read 0 (a no-op loop), NEVER uninitialized stack garbage
// -- a garbage `V_<name>` was looping ~2^31 times and OOB-spamming memory.
// kvar_runtime already clamps to [.., hi], so every filled bound fits the
// worst-case allocation.
static void cpu_jit_kvar_vals(KernelEntry const *ke, unsigned *out_vals) {
  for (u32 i = 0; i < KVAR_USED_CAP; i++) out_vals[i] = 0;
  u32 ids[KVAR_USED_CAP];
  u32 nkv = ke->cached_lift.store_root
          ? kvar_collect_from_dag(ke->cached_lift.store_root, ids, KVAR_USED_CAP)
          : 0;
  for (u32 i = 0; i < nkv; i++) out_vals[i] = (unsigned)kvar_runtime(ids[i]);
}

// THVM_CPU_JIT_WARMUP (any nonzero value) runs every kernel the JIT would
// take in the UOp walker, however warm the caches are: the batched
// precompile at realize builds nothing and the dispatch neither compiles nor
// loads a dylib.  Unset or 0, a kernel compiles on its first fire, as
// tinygrad always compiles.
static int cpu_jit_walker_only(void) {
  static int v = -1;
  if (v < 0) {
    char const *e = getenv("THVM_CPU_JIT_WARMUP");
    v = e != NULL && atoi(e) != 0;
  }
  return v;
}

// Optimization level for the clang JIT compile, tunable via
// Parser nesting limit for the generated C.  A rendered kernel is a single
// nested expression whose depth grows with the fused chain length; clang's
// default is 256, past which the compile FAILS and the kernel falls back to
// the interpreter.  4096 covers the deepest chains thvm fuses.
#define CPU_JIT_BRACKET_DEPTH 4096

// THVM_CPU_JIT_OPT (one of "O0".."O3", default "O3").  -O3 (not
// -ffast-math, so float semantics are bit-identical) gives the deeply
// nested conv-backward gather-reduce kernels ~1.6x over -O2 at steady
// state; a cold-dominated run could trade a faster opt-level (compiles
// quicker) for slightly slower kernels, but the dylib cache amortizes
// the compile across every later step so -O3 stays the default.  The
// dylib filename embeds the tag so an artifact built at a DIFFERENT opt
// level is never reused under the wrong assumptions.
static const char *cpu_jit_opt_tag(void) {
  static char tag[4] = {0};
  if (tag[0] == '\0') {
    const char *e = getenv("THVM_CPU_JIT_OPT");
    char c = (e != NULL && e[0] == 'O' && e[1] >= '0' && e[1] <= '3') ? e[1] : '3';
    tag[0] = 'O'; tag[1] = c; tag[2] = '\0';
  }
  return tag;
}

// The on-disk dylib of one kernel (stem "", keyed by its source hash) or of
// one combined cold-start batch (stem "comb_", keyed by the batch key).
static void cpu_jit_dylib_path(char *out, size_t cap, char const *stem, u64 key) {
  snprintf(out, cap, "/tmp/thvm_jit_%s%016llx_%s.dylib", stem,
           (unsigned long long)key, cpu_jit_opt_tag());
}

static int cpu_jit_dylib_cached(u64 key) {
  char dl_path[256];
  cpu_jit_dylib_path(dl_path, sizeof dl_path, "", key);
  struct stat st;
  return stat(dl_path, &st) == 0;
}

// In-memory kernel cache: source hash -> compiled fn, open addressing.  It
// doubles before it passes half full, so a run that compiles many distinct
// kernels (a decode loop whose position is baked into each step's kernels)
// keeps every one on the JIT; a kernel with no cache slot would run on the
// UOp walker instead.
typedef struct {
  u64       key;          // 0 = empty
  CpuJitFn  func;         // NULL until compile completes
  void     *dl_handle;
  u32       n_inputs;
} CpuJitSlot;
#define CPU_JIT_CACHE_INIT_CAP 256u
static CpuJitSlot *CPU_JIT_CACHE     = NULL;
static u32         CPU_JIT_CACHE_CAP = 0;    // slots, a power of two
static u32         CPU_JIT_CACHE_LEN = 0;    // claimed slots

fn u64 cpu_jit_hash(KernelEntry const *ke) {
  u64 h = 0xcbf29ce484222325ULL;
  h ^= (u64)ke->n_inputs; h *= 0x100000001b3ULL;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h ^= (u64)ke->input_numels[i]; h *= 0x100000001b3ULL;
    // Fold per-input view stride pattern (and full ShapeTracker
    // chain) into the key.  Same lifted DAG with different input
    // strides or chain depth now renders to different inline
    // expressions / chain helpers, so MUST get distinct cache
    // keys -- otherwise reuse of the wrong dylib reads memory at
    // wrong offsets.
    u32 tid = ke->input_tids[i];
    int chained = (tid != 0 && tid < TENS_NEXT && TENS[tid].nviews > 0);
    if (ke->input_views != NULL
        && (!ke->input_views[i].contiguous || chained)) {
      View const *v = &ke->input_views[i];
      h ^= (u64)v->shape.ndim;  h *= 0x100000001b3ULL;
      h ^= (u64)(u32)v->offset; h *= 0x100000001b3ULL;
      for (u32 a = 0; a < v->shape.ndim; a++) {
        h ^= (u64)v->shape.dims[a];     h *= 0x100000001b3ULL;
        h ^= (u64)(u32)v->strides[a];   h *= 0x100000001b3ULL;
      }
      if (chained) {
        h ^= 0xC4u;                     h *= 0x100000001b3ULL;
        h ^= (u64)TENS[tid].nviews;     h *= 0x100000001b3ULL;
        for (u8 k = 0; k < TENS[tid].nviews; k++) {
          View const *pv = &TENS[tid].prior_views[k];
          h ^= (u64)pv->shape.ndim;     h *= 0x100000001b3ULL;
          h ^= (u64)(u32)pv->offset;    h *= 0x100000001b3ULL;
          for (u32 a = 0; a < pv->shape.ndim; a++) {
            h ^= (u64)pv->shape.dims[a];   h *= 0x100000001b3ULL;
            h ^= (u64)(u32)pv->strides[a]; h *= 0x100000001b3ULL;
          }
        }
      }
    } else {
      // Mark "contig" explicitly so a numel-equal contig input
      // doesn't collide with a non-contig one of the same shape.
      h ^= 0xC0u; h *= 0x100000001b3ULL;
    }
  }
  // The kernel's structural identity is encoded by the lifted UOp
  // DAG root (a hash-consed Term).  Equal lifted DAGs share a Term
  // value, so the de-dup is identity-based.
  h ^= (u64)ke->cached_lift.store_root; h *= 0x100000001b3ULL;
  // Fold applied_opts into the key via tile_anno facade so two
  // kernels with identical lifted DAGs but different opts get
  // distinct .dylibs.  axis_types[] / full_shape[] are derived from
  // applied_opts + output_shape so applied_opts alone is sufficient.
  {
    u32 n_app = tile_anno_applied_opts_count(ke);
    KOpt const *opts = tile_anno_applied_opts(ke);
    h ^= (u64)n_app; h *= 0x100000001b3ULL;
    for (u32 i = 0; opts != NULL && i < n_app; i++) {
      KOpt o = opts[i];
      h ^= (u64)o.op;   h *= 0x100000001b3ULL;
      h ^= (u64)o.axis; h *= 0x100000001b3ULL;
      h ^= (u64)o.arg;  h *= 0x100000001b3ULL;
    }
  }
  return h | (1ULL << 63);
}

// Per-kid resolved JIT function-pointer cache.  Each KernelEntry
// hashes to a deterministic source-key; once resolved, the (key,fn)
// pair is invariant for the lifetime of the kid (kernel_apply_opt
// rewires cached_lift before the FIRST dispatch).  Caching by kid
// lets repeat-fire dispatches skip cpu_jit_render_canon entirely --
// the renderer walks the lifted DAG into a tmpfile each call (~60us
// for a small kernel, several hundred for a 7-deep nested loop), and
// at 360 conv-backward dispatches/step that overhead is real.
//
// Per-kid resolved-fn cache.  Lives on TContext (KID_JIT_FN macro ->
// CURRENT_CTX->kid_jit_fn) because it indexes this context's `kernels`
// table: a process-global array aliased kid N across contexts and would
// dispatch one context's compiled fn against another's buffers (the GNN
// sandbox context vs the training/engine context -- the train-then-rerank
// SIGSEGV).  Allocated by init_ctx_arrays; cleared on cpu_jit_cache_reset.
fn void cpu_jit_cache_reset(void) {
  for (u32 i = 0; i < CPU_JIT_CACHE_CAP; i++) {
    if (CPU_JIT_CACHE[i].dl_handle) {
      dlclose(CPU_JIT_CACHE[i].dl_handle);
    }
  }
  if (CPU_JIT_CACHE != NULL) {
    memset(CPU_JIT_CACHE, 0, (size_t)CPU_JIT_CACHE_CAP * sizeof *CPU_JIT_CACHE);
  }
  CPU_JIT_CACHE_LEN = 0;
  // Per-kid resolved-fn pointers reset too: the KERNELS table is
  // re-bumped from 0 by ctx init so stale pointers would otherwise
  // alias fresh kids that happen to land on the same slot.
  if (KID_JIT_FN != NULL) {
    memset(KID_JIT_FN, 0, (size_t)KERNELS_CAP * sizeof(*KID_JIT_FN));
  }
}

static u32 cpu_jit_slot_home(u64 key, u32 mask) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h & mask;
}

// The slot holding `key`, or NULL.  A slot pointer is valid until the next
// cpu_jit_claim_slot, which may move the table.
static CpuJitSlot *cpu_jit_find_slot(u64 key) {
  if (CPU_JIT_CACHE_CAP == 0) {
    return NULL;
  }
  u32 mask = CPU_JIT_CACHE_CAP - 1;
  u32 i    = cpu_jit_slot_home(key, mask);
  for (u32 probe = 0; probe < CPU_JIT_CACHE_CAP; probe++, i = (i + 1) & mask) {
    if (CPU_JIT_CACHE[i].key == key) {
      return &CPU_JIT_CACHE[i];
    }
    if (CPU_JIT_CACHE[i].key == 0) {
      return NULL;
    }
  }
  return NULL;
}

static int cpu_jit_cache_grow(void) {
  u32 cap = CPU_JIT_CACHE_CAP ? 2 * CPU_JIT_CACHE_CAP : CPU_JIT_CACHE_INIT_CAP;
  CpuJitSlot *t = (CpuJitSlot *)calloc(cap, sizeof *t);
  if (t == NULL) {
    return 0;
  }
  for (u32 j = 0; j < CPU_JIT_CACHE_CAP; j++) {
    if (CPU_JIT_CACHE[j].key == 0) {
      continue;
    }
    u32 i = cpu_jit_slot_home(CPU_JIT_CACHE[j].key, cap - 1);
    while (t[i].key != 0) {
      i = (i + 1) & (cap - 1);
    }
    t[i] = CPU_JIT_CACHE[j];
  }
  free(CPU_JIT_CACHE);
  CPU_JIT_CACHE     = t;
  CPU_JIT_CACHE_CAP = cap;
  return 1;
}

// The slot holding `key`, claimed with every other field zero when absent;
// NULL only when the table cannot grow (out of memory).
static CpuJitSlot *cpu_jit_claim_slot(u64 key) {
  CpuJitSlot *s = cpu_jit_find_slot(key);
  if (s != NULL) {
    return s;
  }
  if (2 * (CPU_JIT_CACHE_LEN + 1) > CPU_JIT_CACHE_CAP && !cpu_jit_cache_grow()) {
    return NULL;
  }
  u32 i = cpu_jit_slot_home(key, CPU_JIT_CACHE_CAP - 1);
  while (CPU_JIT_CACHE[i].key != 0) {
    i = (i + 1) & (CPU_JIT_CACHE_CAP - 1);
  }
  s = &CPU_JIT_CACHE[i];
  memset(s, 0, sizeof *s);
  s->key = key;
  CPU_JIT_CACHE_LEN++;
  return s;
}

// Record a resolved kernel under `key`.
static void cpu_jit_cache_store(u64 key, CpuJitFn jfn, void *dl_handle,
                                u32 n_inputs) {
  CpuJitSlot *s = cpu_jit_claim_slot(key);
  if (s != NULL) {
    s->func      = jfn;
    s->dl_handle = dl_handle;
    s->n_inputs  = n_inputs;
  }
}

// The C compiler for JIT kernels: THVM_CPU_CC, else $CC, else the first of
// clang / cc / gcc on PATH (the Linux pods ship gcc only).  Resolved once
// per process.  clang-only flags are added only when it is clang.
static char const *cpu_cc(void) {
  static char cc[128] = {0};
  if (cc[0] != '\0') return cc;
  char const *env = getenv("THVM_CPU_CC");
  if (env == NULL || env[0] == '\0') env = getenv("CC");
  if (env != NULL && env[0] != '\0') {
    snprintf(cc, sizeof cc, "%s", env);
    return cc;
  }
  static char const *candidates[] = {"clang", "cc", "gcc"};
  for (u32 i = 0; i < 3; i++) {
    char probe[192];
    snprintf(probe, sizeof probe, "command -v %s >/dev/null 2>&1", candidates[i]);
    if (system(probe) == 0) {
      snprintf(cc, sizeof cc, "%s", candidates[i]);
      return cc;
    }
  }
  snprintf(cc, sizeof cc, "cc");
  return cc;
}

static int cpu_cc_is_clang(void) {
  static int known = 0, clang = 0;
  if (!known) {
    char probe[320];
    snprintf(probe, sizeof probe,
             "%s -dM -E -x c /dev/null 2>/dev/null | grep -q __clang__", cpu_cc());
    clang = (system(probe) == 0);
    known = 1;
  }
  return clang;
}

// A kernel whose compile fails runs in the CPU interpreter: correct, but
// orders of magnitude slower, so say so once instead of degrading silently.
// THVM_CPU_JIT_VERBOSE=1 keeps the compiler's own diagnostics.
static int cpu_jit_verbose(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_CPU_JIT_VERBOSE");
    on = (e != NULL && e[0] == '1');
    known = 1;
  }
  return on;
}

static void cpu_jit_warn_compile_failed(char const *what) {
  static int warned = 0;
  if (warned) return;
  warned = 1;
  fprintf(stderr,
          "thvm: CPU JIT %s failed with C compiler '%s'; those kernels run in the "
          "CPU interpreter (much slower).  Set THVM_CPU_CC (or CC) to a working "
          "compiler; THVM_CPU_JIT_VERBOSE=1 shows its errors.\n", what, cpu_cc());
}

// Compile the C file `src_path` into the shared object `dl_path` with the
// JIT's compiler and flags, spawned directly (no shell).  Its diagnostics
// are dropped unless `loud` (an injected kernel, which has no interpreter
// to fall back on) or THVM_CPU_JIT_VERBOSE=1.  Returns 1 on success.
// -O3 default (not -O2): on the rendered conv-backward gather-reduce
// kernels (deeply-nested masked-index expressions) -O3's extra
// unrolling + scalar opts give ~1.6x over -O2 with NO float-semantics
// change (-O3 does NOT enable -ffast-math, so the reduction stays
// scalar + bit-identical).  THVM_CPU_JIT_OPT overrides; the dylib
// filename carries the opt tag so an artifact built at a different
// level is never reused under the wrong assumptions.
// -fbracket-depth: a rendered kernel is ONE nested C expression, so a deep
// fused elementwise chain blows clang's default 256-deep parser limit
// ("fatal error: bracket nesting level exceeded maximum of 256") and the
// whole kernel silently falls back to the interpreter -- correct, but a
// ~14x cliff on a 400-op chain.  The limit is a parser guard, not a
// codegen one; raising it costs nothing for shallow kernels.
fn int cpu_jit_compile_shared(char const *src_path, char const *dl_path, int loud) {
  char opt[8], bracket[40];
  snprintf(opt, sizeof opt, "-%s", cpu_jit_opt_tag());
  snprintf(bracket, sizeof bracket, "-fbracket-depth=%d", CPU_JIT_BRACKET_DEPTH);
  char *argv[9];
  u32   ac = 0;
  argv[ac++] = (char *)cpu_cc();
  argv[ac++] = opt;
  if (cpu_cc_is_clang()) {
    argv[ac++] = bracket;
  }
  argv[ac++] = (char *)"-fPIC";
  argv[ac++] = (char *)"-shared";
  argv[ac++] = (char *)"-o";
  argv[ac++] = (char *)dl_path;
  argv[ac++] = (char *)src_path;
  argv[ac]   = NULL;
#ifdef _WIN32
  (void)loud;
  return _spawnvp(_P_WAIT, argv[0], (char const *const *)argv) == 0;
#else
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  if (!loud && !cpu_jit_verbose()) {
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
  }
  pid_t pid;
  int   status  = -1;
  int   spawned = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ) == 0;
  posix_spawn_file_actions_destroy(&fa);
  if (spawned) {
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
  }
  return spawned && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

// dlopen `dl_path` and resolve `sym`: the symbol, with its image in
// *handle, or NULL with the image closed again.
static void *cpu_jit_load(char const *dl_path, char const *sym, void **handle) {
  void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
  if (h == NULL) {
    return NULL;
  }
  void *f = dlsym(h, sym);
  if (f == NULL) {
    dlclose(h);
    return NULL;
  }
  *handle = h;
  return f;
}

static CpuJitFn cpu_jit_build(KernelEntry const *ke, u64 key, const char *src) {
  // Render the kernel as C99 by lifting to the UOp DAG and walking
  // it via cg_render_uop_kernel_c. Output signature matches the
  // existing CPU-JIT contract (void k(out_v, ins_v, n, in_numels))
  // so the surrounding compile/dlopen/dlsym code is unchanged.
  //
  // The only JIT compile path is render_uop over the lifted DAG.
  // Kernels the lifter declines (lift_to_uop returns 0) bail to the
  // interpreter naturally; no silent JIT compile of a wrong kernel.
  //
  // Read the cached KernelUopLift populated by
  // emit_kernel_for_boundary.  The lift is deterministic in the
  // post-materialize kernel state, so reading from cache produces
  // the same store_root / out_buf / in_bufs[] as a fresh
  // kernel_lift_to_uop call.  When the lift declined, store_root
  // is 0 -- early-bail to the interpreter.
  //
  // Pass cached_lift.store_root directly into the structural-mode
  // renderer entry point: buffer names (out, inN) are decoded from
  // each UOP_BUFFER's instance field (kernel_lift.c sets slot+1 on
  // inputs, 0 on the output).  in_bufs[] is kept as a GC root and a
  // per-slot identity cache for cpu_uop_walk; the renderer doesn't
  // consult it.
  if (ke->cached_lift.store_root == 0 || src == NULL) return NULL;

  char src_path[256], dl_path[256];
  snprintf(src_path, sizeof src_path, "/tmp/thvm_jit_%016llx.c",
           (unsigned long long)key);
  cpu_jit_dylib_path(dl_path, sizeof dl_path, "", key);

  // Compile unless the .dylib is already on disk (e.g. from a previous
  // session).  Stat-only: the content-hash name is the freshness check.
  struct stat st;
  if (stat(dl_path, &st) != 0) {
    FILE *f = fopen(src_path, "w");
    if (!f) return NULL;
    fputs(src, f);
    fclose(f);
    if (!cpu_jit_compile_shared(src_path, dl_path, 0)) {
      cpu_jit_warn_compile_failed("compile");
      return NULL;
    }
  }
  void    *h   = NULL;
  CpuJitFn jfn = (CpuJitFn)cpu_jit_load(dl_path, "k", &h);
  if (jfn != NULL) {
    cpu_jit_cache_store(key, jfn, h, ke->n_inputs);
  }
  return jfn;
}

// Render the kernel C source and canonicalize its a<N>/_acc<N> ids so
// structurally-identical kernels are byte-identical across steps.  The
// JIT key is the hash of THIS source, not the lifted-DAG term loc, which
// is a fresh heap cell every realize and so never repeats.
// Caller frees the returned string.
static char *cpu_jit_render_canon(KernelEntry const *ke) {
  if (ke->cached_lift.store_root == 0) return NULL;
  FILE *fp = tmpfile();
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_c_root(ke->cached_lift.store_root, "k", fp);
  long n = ftell(fp);
  if (n <= 0) { fclose(fp); return NULL; }
  char *raw = (char *)malloc((size_t)n + 1);
  if (raw == NULL) { fclose(fp); return NULL; }
  rewind(fp);
  size_t got = fread(raw, 1, (size_t)n, fp);
  fclose(fp);
  raw[got] = '\0';
  // Axis ids are already dense 0..n per kernel (uop_dag_renumber_axes in
  // materialize), so the rendered source is byte-identical for
  // structurally-identical kernels without a post-render canonicalize.
  return raw;
}

static u64 cpu_src_hash(const char *src) {
  u64 h = 0xcbf29ce484222325ULL;
  for (const char *p = src; *p; p++) { h ^= (u64)(unsigned char)*p; h *= 0x100000001b3ULL; }
  return h | (1ULL << 63);
}

// Try the JIT path; returns 1 on success, 0 if the kernel can't be
// JITted (caller dispatches via the interpreter).
// A narrow float (FP16 / BF16 / FP8: a float dtype under 4 bytes) is
// rendered by the CPU codegen as "float" (promote-to-f32) but WITHOUT the
// fp_convert at load/store -- the generated kernel casts the packed
// <4-byte buffer to float* and reads at the wrong width, returning
// garbage.  No faithful CPU narrow-float kernel path exists, so decline
// the JIT and let the caller dispatch via the scalar interpreter
// (uop_walk), which loads/stores through fp16_to_f32 / f32_to_fp16
// correctly.  Metal / CUDA render native half and gate separately, so
// they are unaffected.
static int cpu_jit_narrow_float(u32 dt) {
  return dtype_is_float(dt) && dtype_itemsize(dt) < 4;
}

// Shared JIT-eligibility predicate (the head of cpu_jit_dispatch).  A
// kernel that fails this can't be CPU-JITted, so the parallel precompile
// pass skips it (no wasted clang).
static int cpu_jit_eligible(KernelEntry const *ke) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (cpu_jit_narrow_float(ke->output_dtype)) return 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes != NULL && cpu_jit_narrow_float(ke->input_dtypes[i]))
      return 0;
  }
  return 1;
}

#ifndef _WIN32
// --- Parallel cold-start compile pool -------------------------------------
//
// One pending compile job: the rendered source already written to
// src_path, plus the dylib path + cache key it resolves to.  func_name
// is the kernel's entry-point symbol in the COMBINED dylib (k_<hex>),
// unique per kernel so many kernels coexist in one dlopen'd image.
typedef struct {
  u64  key;
  char src_path[256];
  char dl_path[256];
  char func_name[24];
} CpuJitJob;

// Batch key = FNV-1a over every job's per-kernel content key, so a
// structurally-identical batch (same kernels, same order) maps to the
// same combined dylib across processes.
static u64 cpu_jit_batch_key(CpuJitJob const *jobs, u32 njobs) {
  u64 bk = 0xcbf29ce484222325ULL;
  for (u32 i = 0; i < njobs; i++) { bk ^= jobs[i].key; bk *= 0x100000001b3ULL; }
  return bk | (1ULL << 63);
}

#endif  // !_WIN32

#ifndef _WIN32
// Combined-dylib cold-start lever.  The dominant cost of a cold realize
// on macOS is NOT clang (the parallel pool compiles 20 kernels in
// ~160ms) but the FIRST dlopen of each freshly-built, unsigned dylib:
// syspolicyd runs a per-image code-provenance check that costs ~240ms
// the first time it sees a given dylib's content, and that check
// SERIALIZES (dyld holds a global lock; the security daemon is one IPC
// endpoint), so N kernels = N x 240ms = ~3s of pure dlopen on a 20-
// kernel forward.  The verdict is cached per unique dylib content, and
// ONE dylib holding all N kernels pays the check exactly ONCE
// regardless of how many functions it contains (measured: 1 fn or 40
// fns both ~270ms).  So we concatenate every fresh kernel's rendered
// source (entry points uniquely renamed k_<hex>) into ONE combined .c,
// compile it to ONE .dylib, and dlopen it once -- collapsing the N slow
// first-dlopens to one.  Content-addressed on disk so an identical
// batch in a later process reuses it with a single (verdict-cached)
// dlopen.  Returns the dlopen'd handle (caller dlsyms each k_<hex>) or
// NULL on any failure (caller falls back to per-kernel build).
static void *cpu_jit_build_combined(CpuJitJob const *jobs, u32 njobs,
                                    char *out_dl_path, size_t out_cap) {
  if (njobs == 0) return NULL;
  u64 bk = cpu_jit_batch_key(jobs, njobs);
  char comb_c[256], comb_dl[256];
  snprintf(comb_c,  sizeof comb_c,  "/tmp/thvm_jit_comb_%016llx.c",
           (unsigned long long)bk);
  cpu_jit_dylib_path(comb_dl, sizeof comb_dl, "comb_", bk);
  snprintf(out_dl_path, out_cap, "%s", comb_dl);

  struct stat st;
  if (stat(comb_dl, &st) != 0) {
    // Concatenate every job's already-written per-kernel .c into the
    // combined .c.  Each per-kernel source was rendered with its unique
    // k_<hex> entry-point name, so the symbols never collide; duplicate
    // #include / typedef / #define lines are byte-identical redefinitions
    // (legal C11) and clang accepts them.
    FILE *cf = fopen(comb_c, "w");
    if (cf == NULL) return NULL;
    for (u32 i = 0; i < njobs; i++) {
      FILE *jf = fopen(jobs[i].src_path, "r");
      if (jf == NULL) { fclose(cf); return NULL; }
      char buf[8192];
      size_t r;
      while ((r = fread(buf, 1, sizeof buf, jf)) > 0) fwrite(buf, 1, r, cf);
      fputc('\n', jf == NULL ? cf : cf);  // separator
      fclose(jf);
    }
    fclose(cf);
    if (!cpu_jit_compile_shared(comb_c, comb_dl, 0)) {
      cpu_jit_warn_compile_failed("parallel compile");
      return NULL;
    }
  }
  return dlopen(comb_dl, RTLD_NOW | RTLD_LOCAL);
}
#endif  // !_WIN32

// Parallel cold-start JIT warm: render + clang-compile every JIT-eligible
// CPU kernel in [kid_lo, kid_hi) CONCURRENTLY (a posix_spawn pool of up to
// ncores clang processes) and populate the on-disk dylib cache + in-memory
// CpuJitSlot table BEFORE the serial dispatch loop fires them one-at-a-time.
// Idempotent: a hash already on disk / in cache is skipped, and the batch is
// de-duped by source hash so two workers never compile the same dylib.  A
// no-op when THVM_CPU_JIT_PARALLEL=0.  The subsequent cpu_jit_dispatch fires
// then hit the cache (no compile), collapsing ~Nx serial-clang wait to one
// parallel pass.  Called from thvm_realize / thvm_realize_many once the
// materialize fixpoint has emitted the kernel table, before the driving wnf.
fn void cpu_jit_precompile_range(u32 kid_lo, u32 kid_hi) {
#ifdef _WIN32
  (void)kid_lo; (void)kid_hi;
#else
  static int known = 0, enabled = 1;
  if (!known) {
    const char *e = getenv("THVM_CPU_JIT_PARALLEL");
    enabled = (e == NULL) ? 1 : (e[0] != '0');
    known = 1;
  }
  if (!enabled || cpu_jit_walker_only()) return;
  if (kid_lo < 1) kid_lo = 1;
  if (kid_hi > KERNELS_NEXT) kid_hi = KERNELS_NEXT;
  if (kid_lo >= kid_hi) return;

  // Collect pending compile jobs.  Cap the batch; a single realize emits at
  // most a few hundred kernels.  Heap-allocated (each CpuJitJob is ~520B, so
  // a 1024 stack array would be ~520KB -- well past a safe frame).
  enum { CPU_JIT_BATCH_CAP = 1024 };
  CpuJitJob *jobs = (CpuJitJob *)malloc(CPU_JIT_BATCH_CAP * sizeof(CpuJitJob));
  if (jobs == NULL) return;
  u32 njobs = 0;
  u64 *seen_keys = (u64 *)malloc(CPU_JIT_BATCH_CAP * sizeof(u64));
  if (seen_keys == NULL) { free(jobs); return; }
  u32 nseen = 0;

  for (u32 kid = kid_lo; kid < kid_hi && njobs < CPU_JIT_BATCH_CAP; kid++) {
    KernelEntry *ke = &KERNELS[kid];
    if (ke->cached_lift.store_root == 0) continue;
    // Only CPU-routed kernels reach cpu_jit_dispatch; skip Metal/CUDA
    // outputs so a GPU run doesn't spawn useless clang jobs.
    if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) continue;
    if (TENS[ke->output_tid].backend != &CPU_BACKEND) continue;
    if (!cpu_jit_eligible(ke)) continue;
    // Already resolved for this kid -> nothing to compile.
    if (kid < KERNELS_CAP && KID_JIT_FN != NULL && KID_JIT_FN[kid] != NULL)
      continue;

    // Apply the SAME first-fire opt decision the dispatch path applies, so
    // the rendered source -- and therefore its hash + dylib -- is identical
    // to what cpu_jit_dispatch will look up.  kernel_hand_coded_opts rewrites
    // cached_lift.store_root (UPCAST register-blocking etc.) and is idempotent
    // (guards on hand_coded_done), so the later fire-path call is a no-op.
    // Without this the precompile renders the UN-optimized kernel, hashes to a
    // different key, and builds a dylib the dispatch never uses (it then
    // recompiles the optimized kernel serially -- net loss).
    if (kernel_should_hand_code_opts(ke)) kernel_hand_coded_opts(ke);
    // BEAM/autotune (when enabled) benchmarks variants at fire time and
    // rewrites store_root AFTER hand_opts, so a pre-rendered hash wouldn't
    // match; leave those kernels to the lazy fire-time compile.
    if (kernel_should_autotune(ke)) continue;

    char *src = cpu_jit_render_canon(ke);
    if (src == NULL) continue;
    u64 key = cpu_src_hash(src);

    // Already compiled in-memory?  Skip.
    CpuJitSlot *s = cpu_jit_find_slot(key);
    if (s != NULL && s->func != NULL) { free(src); continue; }
    // On disk: the dispatch loads it.
    if (cpu_jit_dylib_cached(key)) { free(src); continue; }

    char dl_path[256];
    cpu_jit_dylib_path(dl_path, sizeof dl_path, "", key);

    // De-dup the batch by hash: two structurally-identical kernels render
    // to the same source -> same dylib; compile it once.
    int dup = 0;
    for (u32 i = 0; i < nseen; i++) if (seen_keys[i] == key) { dup = 1; break; }
    if (dup) { free(src); continue; }
    if (nseen < CPU_JIT_BATCH_CAP) seen_keys[nseen++] = key;

    // Rename the entry point from `k` to a per-kernel-unique `k_<hex>` so
    // the combined dylib can hold every kernel without symbol collision.
    // The renderer emits exactly one top-level definition (`void k(...)`,
    // the entry point); helper math calls are libm names, statics don't
    // exist in the C target's emit, so a single signature rewrite is
    // sufficient.  The on-disk per-kernel .c keeps the renamed symbol; the
    // lazy single-kernel cpu_jit_build path (interpreter-fallback warmup)
    // still dlsyms "k", so it renders its own "k"-named source separately
    // and is unaffected.
    char func_name[24];
    snprintf(func_name, sizeof func_name, "k_%016llx", (unsigned long long)key);
    // Match the full entry-point signature prefix (not a bare "void k(")
    // so a stray substring elsewhere can never be rewritten.  The C target
    // emits exactly "void k(void *out_v, ...".  If it isn't found (an
    // unexpected render shape), skip this kernel from the combined batch --
    // it falls back to the lazy per-kernel cpu_jit_build on first dispatch
    // rather than risk a "void k" symbol collision in the shared image.
    static const char ENTRY_SIG[] = "void k(void *out_v";
    char *sig = strstr(src, ENTRY_SIG);
    if (sig == NULL) { free(src); continue; }
    {
      // Rewrite "void k(void *out_v..." -> "void k_<hex>(void *out_v...".
      size_t pre = (size_t)(sig - src);
      size_t suf_off = pre + 6;   // skip "void k", keep "(void *out_v..."
      size_t suflen = strlen(src + suf_off);
      size_t namelen = strlen(func_name);
      char *renamed = (char *)malloc(pre + 5 + namelen + suflen + 1);
      if (renamed == NULL) { free(src); continue; }
      memcpy(renamed, src, pre);
      memcpy(renamed + pre, "void ", 5);
      memcpy(renamed + pre + 5, func_name, namelen);
      memcpy(renamed + pre + 5 + namelen, src + suf_off, suflen + 1);
      free(src);
      src = renamed;
    }

    // Write the renamed source to its hash-named .c (the combined build
    // concatenates these; the per-kernel filename keys on the content hash
    // so an identical kernel reuses the same .c across batches).
    char src_path[256];
    snprintf(src_path, sizeof src_path, "/tmp/thvm_jit_comb_src_%016llx.c",
             (unsigned long long)key);
    FILE *f = fopen(src_path, "w");
    if (f != NULL) { fputs(src, f); fclose(f); }
    free(src);
    if (f == NULL) continue;

    jobs[njobs].key = key;
    snprintf(jobs[njobs].src_path, sizeof jobs[njobs].src_path, "%s", src_path);
    snprintf(jobs[njobs].dl_path,  sizeof jobs[njobs].dl_path,  "%s", dl_path);
    snprintf(jobs[njobs].func_name, sizeof jobs[njobs].func_name, "%s", func_name);
    njobs++;
  }

  free(seen_keys);

  if (njobs == 0) { free(jobs); return; }

  // Build ONE combined dylib holding every fresh kernel (entry points
  // renamed k_<hex>), then dlopen it ONCE.  This collapses the N slow
  // first-dlopen syspolicyd provenance checks (the dominant cold cost on
  // macOS) to a single check.  The single combined clang invocation
  // compiles all N kernels in one TU; on a 20-kernel forward that one
  // clang is comparable to the old ncores-wide parallel pool but pays
  // one fork/exec instead of N.
  char comb_dl[256] = {0};
  void *h = cpu_jit_build_combined(jobs, njobs, comb_dl, sizeof comb_dl);

  // dlsym every kernel's k_<hex> entry point out of the single combined
  // image and populate the in-memory cache so the dispatch loop finds a
  // resolved fn pointer (no serial recompile, no per-kernel dlopen).  The
  // one shared dl_handle is recorded on the FIRST slot we fill; the rest
  // reference into the same image (dropping it would unmap them), so they
  // store func only.  cpu_jit_cache_reset dlcloses every non-NULL handle,
  // which closes this shared image exactly once.
  if (h != NULL) {
    int handle_owned = 0;
    for (u32 i = 0; i < njobs; i++) {
      CpuJitFn jfn = (CpuJitFn)dlsym(h, jobs[i].func_name);
      if (jfn == NULL) continue;
      CpuJitSlot *s = cpu_jit_claim_slot(jobs[i].key);
      if (s != NULL && s->func == NULL) {
        s->func = jfn;
        s->dl_handle = handle_owned ? NULL : h;
        handle_owned = 1;
      }
    }
    // No kernel claimed the handle (all slots already filled) -- drop it.
    if (!handle_owned) dlclose(h);
  }
  free(jobs);
#endif  // _WIN32
}

fn int cpu_jit_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (!cpu_jit_eligible(ke) || cpu_jit_walker_only()) return 0;
  // Per-kid fast path: if this kid was already compiled to a JIT fn
  // pointer, reuse it directly -- skips cpu_jit_render_canon (heap
  // walk into a tmpfile) and the source hash entirely.  The kid -> fn
  // binding is invariant once set; kernel_apply_opt only rewires
  // cached_lift BEFORE the first dispatch, after which a kid never
  // changes shape/source.  Skip the per-kid cache for kids that have
  // never compiled before (KID_JIT_FN[kid] == NULL) -- they fall
  // through to the full source-hash path, which reuses a dylib on disk.
  u32 _kid = (u32)(ke - KERNELS);
  if (_kid < KERNELS_CAP && KID_JIT_FN != NULL && KID_JIT_FN[_kid] != NULL) {
    CpuJitFn _jfn = (CpuJitFn)KID_JIT_FN[_kid];
    u32 ni = ke->n_inputs;
    const void *ins_buf [ni ? ni : 1];
    unsigned    nums_buf[ni ? ni : 1];
    for (u32 i = 0; i < ni; i++) {
      ins_buf [i] = CPU_BUFS[in_buf_ids[i]].data;
      nums_buf[i] = ke->input_numels[i];
    }
    void *out = CPU_BUFS[out_buf_id].data;
    unsigned numel = (unsigned)ke->output_numel;
    unsigned kvar_vals[KVAR_USED_CAP];
    cpu_jit_kvar_vals(ke, kvar_vals);
    _jfn(out, ins_buf, numel, nums_buf, kvar_vals);
    return 1;
  }
  char *src = cpu_jit_render_canon(ke);
  if (src == NULL) return 0;
  u64 key = cpu_src_hash(src);
  // THVM_DUMP_KERNEL_SRC=<kid>: print this kid's JIT hash + shape once
  // so the caller can read the rendered C from /tmp/thvm_jit_<hash>.c.
  // The hash key embeds source content; if the same shape produces N
  // distinct sources it shows up here as N different hashes.
  {
    static int    DUMPED[2048] = {0};
    static u32    DUMP_TARGET  = 0xFFFFFFFFu;
    static int    DUMP_TARGET_KNOWN = 0;
    if (!DUMP_TARGET_KNOWN) {
      char const *e = getenv("THVM_DUMP_KERNEL_SRC");
      if (e != NULL && e[0] != '\0') DUMP_TARGET = (u32)atoi(e);
      DUMP_TARGET_KNOWN = 1;
    }
    u32 kid = (u32)(ke - KERNELS);
    if (DUMP_TARGET != 0xFFFFFFFFu && (DUMP_TARGET == 0 || kid == DUMP_TARGET)
        && kid < 2048 && !DUMPED[kid]) {
      DUMPED[kid] = 1;
      fprintf(stderr, "[kdump] kid=%u key=%016llx n_in=%u shape=[",
              kid, (unsigned long long)key, ke->n_inputs);
      for (u32 i = 0; i < ke->output_shape.ndim; i++) {
        fprintf(stderr, "%s%u", i ? "," : "", ke->output_shape.dims[i]);
      }
      fprintf(stderr, "] in_bufs=[");
      for (u32 i = 0; i < ke->n_inputs && i < 4; i++) {
        u32 numel = (ke->input_numels ? ke->input_numels[i] : 0);
        fprintf(stderr, "%s%u:%u", i ? "," : "", in_buf_ids[i], numel);
      }
      fprintf(stderr, "] out_buf=%u out_numel=%u\n", out_buf_id, ke->output_numel);
    }
  }
  CpuJitSlot *s = cpu_jit_find_slot(key);
  CpuJitFn jfn = (s != NULL) ? s->func : NULL;

  if (jfn == NULL) {
    jfn = cpu_jit_build(ke, key, src);
    if (!jfn) { free(src); return 0; }
  }
  free(src);                               // source no longer needed (hit or compiled)

  // Cache the resolved (kid -> fn) so the next fire of this kid
  // skips render+hash+lookup (the early-return at the top of this
  // function).  Only cached when jfn is non-NULL.
  if (_kid < KERNELS_CAP && KID_JIT_FN != NULL) KID_JIT_FN[_kid] = (void *)jfn;
  // Pack input pointers + numels into local arrays and call the
  // generated function.  Stack-sized to ke->n_inputs (covered by
  // VLA below); no fixed cap beyond what the codegen supports.
  // Pointers are typed as `const void *` so the same dispatch path
  // covers every supported dtype.
  u32 ni = ke->n_inputs;
  const void *ins_buf  [ni ? ni : 1];
  unsigned    nums_buf [ni ? ni : 1];
  for (u32 i = 0; i < ni; i++) {
    ins_buf [i] = CPU_BUFS[in_buf_ids[i]].data;
    nums_buf[i] = ke->input_numels[i];
  }
  void *out = CPU_BUFS[out_buf_id].data;
  // Read the output numel directly from KernelEntry.
  unsigned numel = (unsigned)ke->output_numel;
  unsigned kvar_vals[KVAR_USED_CAP];
  cpu_jit_kvar_vals(ke, kvar_vals);
  jfn(out, ins_buf, numel, nums_buf, kvar_vals);
  return 1;
}

