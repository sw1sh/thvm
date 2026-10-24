// backend/cpu/jit.c - compile a kernel's rendered C into a dylib and call its
// `k`.  Keyed by the source hash, so a structurally identical kernel reuses the
// build across steps and the dylib on disk across processes.

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

// kvar_vals[i] is the bound of the kernel's i-th kvar in kvar_collect_from_dag
// order, the order render_uop declares `unsigned V_<name> = kvar_vals[i]`.
typedef void (*CpuJitFn)(void *out, const void *const *ins,
                         unsigned n, const unsigned *in_numels,
                         const unsigned *kvar_vals);

// out_vals holds KVAR_USED_CAP entries, zeroed first so an unfilled bound is an
// empty loop; kvar_runtime clamps each filled one to the allocation's bound.
static void cpu_jit_kvar_vals(KernelEntry const *ke, unsigned *out_vals) {
  for (u32 i = 0; i < KVAR_USED_CAP; i++) out_vals[i] = 0;
  u32 ids[KVAR_USED_CAP];
  u32 nkv = ke->cached_lift.store_root
          ? kvar_collect_from_dag(ke->cached_lift.store_root, ids, KVAR_USED_CAP)
          : 0;
  for (u32 i = 0; i < nkv; i++) out_vals[i] = (unsigned)kvar_runtime(ids[i]);
}

// THVM_CPU_JIT_WARMUP nonzero runs every kernel in the UOp walker, building or
// loading nothing; unset or 0, a kernel compiles on its first fire, as in tinygrad.
static int cpu_jit_walker_only(void) {
  static int v = -1;
  if (v < 0) {
    char const *e = getenv("THVM_CPU_JIT_WARMUP");
    v = e != NULL && atoi(e) != 0;
  }
  return v;
}

// clang's parser nesting limit: a rendered kernel is one nested expression as
// deep as its fused chain, and past the default 256 the compile fails.
#define CPU_JIT_BRACKET_DEPTH 4096

// THVM_CPU_JIT_OPT picks O0 to O3; O3 (no fast-math) is ~1.6x over O2 on
// conv-backward.  The dylib name carries the level, so builds never mix.
static const char *cpu_jit_opt_tag(void) {
  static char tag[4] = {0};
  if (tag[0] == '\0') {
    const char *e = getenv("THVM_CPU_JIT_OPT");
    char c = (e != NULL && e[0] == 'O' && e[1] >= '0' && e[1] <= '3') ? e[1] : '3';
    tag[0] = 'O'; tag[1] = c; tag[2] = '\0';
  }
  return tag;
}

static int cpu_cc_is_clang(void);

// 1 iff the kernels compile with -march=native (cpu_jit_compile_shared).
static int cpu_jit_native(void) {
#if defined(__x86_64__)
  return cpu_cc_is_clang();
#else
  return 0;
#endif
}

// The on-disk dylib of one kernel (stem "", keyed by its source hash) or of
// one combined cold-start batch (stem "comb_", keyed by the batch key); "n"
// marks a -march=native build.
static void cpu_jit_dylib_path(char *out, size_t cap, char const *stem, u64 key) {
  snprintf(out, cap, "/tmp/thvm_jit_%s%016llx_%s%s.dylib", stem,
           (unsigned long long)key, cpu_jit_opt_tag(), cpu_jit_native() ? "n" : "");
}

static int cpu_jit_dylib_cached(u64 key) {
  char dl_path[256];
  cpu_jit_dylib_path(dl_path, sizeof dl_path, "", key);
  struct stat st;
  return stat(dl_path, &st) == 0;
}

// Source hash -> compiled fn, open addressing, doubled before half full so a
// run of many distinct kernels (a decode loop's per-position ones) stays compiled.
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

// A structural kernel key; the JIT cache and dylib names use cpu_src_hash.
fn u64 cpu_jit_hash(KernelEntry const *ke) {
  u64 h = 0xcbf29ce484222325ULL;
  h ^= (u64)ke->n_inputs; h *= 0x100000001b3ULL;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h ^= (u64)ke->input_numels[i]; h *= 0x100000001b3ULL;
    // Input strides and chains render into the source, so they key it too.
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
  h ^= (u64)ke->cached_lift.store_root; h *= 0x100000001b3ULL;
  // applied_opts alone fixes axis_types and full_shape, so it keys both.
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

// KID_JIT_FN, each kid's resolved fn, lives on TContext because it indexes that
// context's kernel table: a global one would mix contexts' kernels.
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
  // A reset kernel table re-issues kid ids, so their fns go too.
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

static void cpu_jit_cache_store(u64 key, CpuJitFn jfn, void *dl_handle,
                                u32 n_inputs) {
  CpuJitSlot *s = cpu_jit_claim_slot(key);
  if (s != NULL) {
    s->func      = jfn;
    s->dl_handle = dl_handle;
    s->n_inputs  = n_inputs;
  }
}

// THVM_CPU_CC, else $CC, else the first of clang, cc and gcc on PATH (the Linux
// pods ship only gcc); resolved once.
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

// A failed compile leaves the kernel to the much slower walker, so it is said
// once; THVM_CPU_JIT_VERBOSE=1 keeps the compiler's own diagnostics.
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

// Spawned without a shell; diagnostics are dropped unless `loud` (an injected
// kernel has no walker to fall back on) or THVM_CPU_JIT_VERBOSE=1.  No errno, as
// tinygrad compiles (runtime/support/compiler_cpu.py), so a sqrt is an
// instruction, not a libm call; contraction stays off, so a kernel's floats
// match the walker's.  tinygrad's -march=native on x86-64 only with clang: gcc
// 11 with AVX-512 miscompiles a where over three 10x10 buffers.  The dylibs
// stay in this machine's /tmp.
fn int cpu_jit_compile_shared(char const *src_path, char const *dl_path, int loud) {
  char opt[8], bracket[40];
  snprintf(opt, sizeof opt, "-%s", cpu_jit_opt_tag());
  snprintf(bracket, sizeof bracket, "-fbracket-depth=%d", CPU_JIT_BRACKET_DEPTH);
  char *argv[12];
  u32   ac = 0;
  argv[ac++] = (char *)cpu_cc();
  argv[ac++] = opt;
  argv[ac++] = (char *)"-fno-math-errno";
  argv[ac++] = (char *)"-ffp-contract=off";
  if (cpu_cc_is_clang()) {
    argv[ac++] = bracket;
  }
  if (cpu_jit_native()) {
    argv[ac++] = (char *)"-march=native";
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

// Render, compile and run a free-standing root once, under private paths the
// cache never sees: 0, -1 when the renderer declines, -2 on a build failure.
fn int cpu_jit_run_root(Term root, void *out, const void *const *ins, unsigned n,
                        const unsigned *in_numels) {
  static u32 seq = 0;
  char src_path[96], dl_path[96];
  snprintf(src_path, sizeof src_path, "/tmp/thvm_run_root_%d_%u.c", (int)getpid(), seq);
  snprintf(dl_path, sizeof dl_path, "/tmp/thvm_run_root_%d_%u.so", (int)getpid(), seq++);
  FILE *f = fopen(src_path, "w");
  if (f == NULL) {
    return -2;
  }
  cg_render_uop_kernel_c_root(root, "k", f);
  long len = ftell(f);
  fclose(f);
  int rc = -1;
  if (len > 0) {
    void    *h = NULL;
    CpuJitFn k = cpu_jit_compile_shared(src_path, dl_path, 0)
                   ? (CpuJitFn)cpu_jit_load(dl_path, "k", &h) : NULL;
    rc = -2;
    if (k != NULL) {
      unsigned kvars[KVAR_USED_CAP] = {0};
      k(out, ins, n, in_numels, kvars);
      dlclose(h);
      rc = 0;
    }
  }
  unlink(src_path);
  unlink(dl_path);
  return rc;
}

static CpuJitFn cpu_jit_build(KernelEntry const *ke, u64 key, const char *src) {
  if (ke->cached_lift.store_root == 0 || src == NULL) return NULL;

  char src_path[256], dl_path[256];
  snprintf(src_path, sizeof src_path, "/tmp/thvm_jit_%016llx.c",
           (unsigned long long)key);
  cpu_jit_dylib_path(dl_path, sizeof dl_path, "", key);

  // The content-hash name is the freshness check: a dylib on disk is reused.
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

// The rendered source (caller frees) is the JIT key: unlike the lifted root,
// a fresh heap cell every realize, it repeats for structurally equal kernels.
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
  // Axis ids are dense per kernel (uop_dag_renumber_axes in materialize), so
  // equal kernels render byte-identical.
  return raw;
}

static u64 cpu_src_hash(const char *src) {
  u64 h = 0xcbf29ce484222325ULL;
  for (const char *p = src; *p; p++) { h ^= (u64)(unsigned char)*p; h *= 0x100000001b3ULL; }
  return h | (1ULL << 63);
}

// The C render types a float under 4 bytes as float without converting at load
// or store, so it would read the wrong width; the walker converts.
static int cpu_jit_narrow_float(u32 dt) {
  return dtype_is_float(dt) && dtype_itemsize(dt) < 4;
}

// Shared by cpu_jit_dispatch and the precompile pass.
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
// One kernel of a cold-start batch: its source, already written, and its entry
// k_<hex>, unique so many kernels share one combined dylib.
typedef struct {
  u64  key;
  char src_path[256];
  char dl_path[256];
  char func_name[24];
} CpuJitJob;

// FNV-1a over the jobs' keys, so the same kernels in the same order map to one
// combined dylib across processes.
static u64 cpu_jit_batch_key(CpuJitJob const *jobs, u32 njobs) {
  u64 bk = 0xcbf29ce484222325ULL;
  for (u32 i = 0; i < njobs; i++) { bk ^= jobs[i].key; bk *= 0x100000001b3ULL; }
  return bk | (1ULL << 63);
}

#endif  // !_WIN32

#ifndef _WIN32
// macOS checks a new unsigned dylib's provenance on its first dlopen, ~240ms and
// serialized, once per image: one combined dylib per batch pays it once.
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
    // Repeated #include, typedef and #define lines are identical
    // redefinitions, which C11 allows.
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

// Before the fires, build every fresh JIT-eligible CPU kernel in [kid_lo,
// kid_hi) into one dylib and fill the cache; THVM_CPU_JIT_PARALLEL=0 skips it.
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

  // On the heap: 1024 jobs of ~520 bytes would overflow a safe stack frame.
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
    // Only CPU kernels reach cpu_jit_dispatch.
    if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) continue;
    if (TENS[ke->output_tid].backend != &CPU_BACKEND) continue;
    if (!cpu_jit_eligible(ke)) continue;
    if (kid < KERNELS_CAP && KID_JIT_FN != NULL && KID_JIT_FN[kid] != NULL)
      continue;

    // The fire-time hand opts first (idempotent), so this renders the source,
    // and so the key, that cpu_jit_dispatch will look up.
    if (kernel_should_hand_code_opts(ke)) kernel_hand_coded_opts(ke);
    // Autotune rewrites store_root at fire time, after hand opts, so its
    // kernels compile lazily.
    if (kernel_should_autotune(ke)) continue;

    char *src = cpu_jit_render_canon(ke);
    if (src == NULL) continue;
    u64 key = cpu_src_hash(src);

    CpuJitSlot *s = cpu_jit_find_slot(key);
    if (s != NULL && s->func != NULL) { free(src); continue; }
    if (cpu_jit_dylib_cached(key)) { free(src); continue; }

    char dl_path[256];
    cpu_jit_dylib_path(dl_path, sizeof dl_path, "", key);

    int dup = 0;
    for (u32 i = 0; i < nseen; i++) if (seen_keys[i] == key) { dup = 1; break; }
    if (dup) { free(src); continue; }
    if (nseen < CPU_JIT_BATCH_CAP) seen_keys[nseen++] = key;

    // Rename `k` to k_<hex> for the combined image: the render emits no other
    // top-level definition, so one signature rewrite suffices.
    char func_name[24];
    snprintf(func_name, sizeof func_name, "k_%016llx", (unsigned long long)key);
    // The full signature prefix, so no stray "void k(" matches; a kernel
    // without it builds lazily on its own instead.
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

  char comb_dl[256] = {0};
  void *h = cpu_jit_build_combined(jobs, njobs, comb_dl, sizeof comb_dl);

  // Only the first filled slot owns the shared image's handle, so
  // cpu_jit_cache_reset dlcloses it exactly once.
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
  // A kid's resolved fn skips the render (a DAG walk into a tmpfile) and hash:
  // the opt passes rewrite cached_lift only before a kernel's first dispatch.
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
  // THVM_DUMP_KERNEL_SRC=<kid> (0 for every kid) prints the kid's key and shape
  // once; the source is /tmp/thvm_jit_<key>.c when this process compiled it.
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
  free(src);

  if (_kid < KERNELS_CAP && KID_JIT_FN != NULL) KID_JIT_FN[_kid] = (void *)jfn;
  u32 ni = ke->n_inputs;
  const void *ins_buf  [ni ? ni : 1];
  unsigned    nums_buf [ni ? ni : 1];
  for (u32 i = 0; i < ni; i++) {
    ins_buf [i] = CPU_BUFS[in_buf_ids[i]].data;
    nums_buf[i] = ke->input_numels[i];
  }
  void *out = CPU_BUFS[out_buf_id].data;
  unsigned numel = (unsigned)ke->output_numel;
  unsigned kvar_vals[KVAR_USED_CAP];
  cpu_jit_kvar_vals(ke, kvar_vals);
  jfn(out, ins_buf, numel, nums_buf, kvar_vals);
  return 1;
}

