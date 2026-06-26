// backend/cpu/jit.c - shell out to clang to compile a generated
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
// Cache key: cached_lift.store_root + n_inputs + per-input numel.
// Numel goes in because the codegen embeds broadcast checks
// (numel==1) at emit time, so a program with the same lifted DAG
// but different input numels needs a fresh build.

#ifndef _WIN32
#include <dlfcn.h>  // Windows: dlopen family shimmed in util/portable_win.h
#include <spawn.h>      // posix_spawn for the parallel cold-start compile pool
#include <sys/wait.h>   // waitpid for the spawned clang workers
extern char **environ;
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

#define CPU_JIT_CACHE_CAP 256
// JIT warmup gate: a kernel hash must fire CPU_JIT_WARMUP times before
// committing to a clang compile (interpreted until then).  Default 0 =
// compile on first fire (tinygrad always compiles).  At BS=128 an
// interpreted conv step is ~100s, so any warmup>0 makes the first few
// steps catastrophically slow; compiling immediately pays one clang
// pass (~ms/kernel) and every subsequent step is fast.  The source-keyed
// cache + on-disk /tmp dylib reuse make repeat compiles free.
#define CPU_JIT_WARMUP 0
// Override the warmup threshold via THVM_CPU_JIT_WARMUP (0 = compile on
// first fire).  Lets a training loop with large one-shot-feeling kernels
// (e.g. the conv im2col STORE) commit to a compile sooner.
static u32 cpu_jit_warmup(void) {
  static int v = -1;
  if (v < 0) { const char *e = getenv("THVM_CPU_JIT_WARMUP"); v = e ? atoi(e) : CPU_JIT_WARMUP; }
  return (u32)v;
}

// Optimization level for the clang JIT compile, tunable via
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
typedef struct {
  u64       key;          // 0 = empty
  CpuJitFn  func;         // NULL until compile completes
  void     *dl_handle;
  u32       n_inputs;
  u32       fire_count;   // # of dispatches before func was set
} CpuJitSlot;
static CpuJitSlot CPU_JIT_CACHE[CPU_JIT_CACHE_CAP];

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
    if (CPU_JIT_CACHE[i].dl_handle) dlclose(CPU_JIT_CACHE[i].dl_handle);
    CPU_JIT_CACHE[i].key        = 0;
    CPU_JIT_CACHE[i].func       = NULL;
    CPU_JIT_CACHE[i].dl_handle  = NULL;
    CPU_JIT_CACHE[i].n_inputs   = 0;
    CPU_JIT_CACHE[i].fire_count = 0;
  }
  // Per-kid resolved-fn pointers reset too: the KERNELS table is
  // re-bumped from 0 by ctx init so stale pointers would otherwise
  // alias fresh kids that happen to land on the same slot.
  if (KID_JIT_FN != NULL) {
    memset(KID_JIT_FN, 0, (size_t)KERNELS_CAP * sizeof(*KID_JIT_FN));
  }
}

static CpuJitSlot *cpu_jit_lookup_slot(u64 key) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < CPU_JIT_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (CPU_JIT_CACHE_CAP - 1);
    if (CPU_JIT_CACHE[i].key == key) return &CPU_JIT_CACHE[i];
    if (CPU_JIT_CACHE[i].key == 0)   return &CPU_JIT_CACHE[i];
  }
  return NULL;
}

// Compile + load.  Returns the resolved `k` function pointer or NULL
// on any failure (caller falls back to interpreter).
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
  snprintf(dl_path,  sizeof dl_path,  "/tmp/thvm_jit_%016llx_%s.dylib",
           (unsigned long long)key, cpu_jit_opt_tag());

  // Skip the compile if the .dylib is already on disk (e.g. from a
  // previous session) and current.  Stat-only check; we don't try to
  // verify cache freshness against the current source.
  struct stat st;
  if (stat(dl_path, &st) == 0) {
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    if (!h) return NULL;
    CpuJitFn jfn = (CpuJitFn)dlsym(h, "k");
    if (!jfn) { dlclose(h); return NULL; }
    CpuJitSlot *s = cpu_jit_lookup_slot(key);
    if (s) { s->key = key; s->func = jfn; s->dl_handle = h; s->n_inputs = ke->n_inputs; }
    return jfn;
  }

  FILE *f = fopen(src_path, "w");
  if (!f) return NULL;
  fputs(src, f);
  fclose(f);

  char cmd[768];
  // -O3 default (not -O2): on the rendered conv-backward gather-reduce
  // kernels (deeply-nested masked-index expressions) -O3's extra
  // unrolling + scalar opts give ~1.6x over -O2 with NO float-semantics
  // change (-O3 does NOT enable -ffast-math, so the reduction stays
  // scalar + bit-identical).  THVM_CPU_JIT_OPT overrides; the dylib
  // filename carries the opt tag so an artifact built at a different
  // level is never reused under the wrong assumptions.
  snprintf(cmd, sizeof cmd,
           "clang -%s -fPIC -shared -o '%s' '%s' 2>/dev/null",
           cpu_jit_opt_tag(), dl_path, src_path);
  if (system(cmd) != 0) return NULL;

  void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
  if (!h) return NULL;
  CpuJitFn jfn = (CpuJitFn)dlsym(h, "k");
  if (!jfn) { dlclose(h); return NULL; }

  CpuJitSlot *s = cpu_jit_lookup_slot(key);
  if (s) { s->key = key; s->func = jfn; s->dl_handle = h; s->n_inputs = ke->n_inputs; }
  return jfn;
}

// Render the kernel C source and canonicalize its a<N>/_acc<N> ids so
// structurally-identical kernels are byte-identical across steps.  The
// JIT key is the hash of THIS source (not the lifted-DAG term loc, which
// is a fresh heap cell every realize and so never repeats -> the kernel
// never crossed the warmup gate and stayed on the interpreter forever).
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
  if (!cg_supports(ke)) return 0;
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
  // Batch key = FNV-1a over every job's per-kernel content key, so a
  // structurally-identical batch (same kernels, same order) maps to the
  // same combined dylib across processes.
  u64 bk = 0xcbf29ce484222325ULL;
  for (u32 i = 0; i < njobs; i++) { bk ^= jobs[i].key; bk *= 0x100000001b3ULL; }
  bk |= (1ULL << 63);
  char comb_c[256], comb_dl[256];
  snprintf(comb_c,  sizeof comb_c,  "/tmp/thvm_jit_comb_%016llx.c",
           (unsigned long long)bk);
  snprintf(comb_dl, sizeof comb_dl, "/tmp/thvm_jit_comb_%016llx_%s.dylib",
           (unsigned long long)bk, cpu_jit_opt_tag());
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
    char optflag[8];
    snprintf(optflag, sizeof optflag, "-%s", cpu_jit_opt_tag());
    char *argv[8] = { (char *)"clang", optflag, (char *)"-fPIC",
                      (char *)"-shared", (char *)"-o", comb_dl, comb_c, NULL };
    pid_t pid;
    if (posix_spawnp(&pid, "clang", NULL, NULL, argv, environ) != 0) return NULL;
    int status;
    waitpid(pid, &status, 0);
    if (stat(comb_dl, &st) != 0) return NULL;   // compile failed
  }
  return dlopen(comb_dl, RTLD_NOW | RTLD_LOCAL);
}
#endif  // !_WIN32

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
  if (!enabled) return;
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
    CpuJitSlot *s = cpu_jit_lookup_slot(key);
    if (s != NULL && s->key == key && s->func != NULL) { free(src); continue; }

    char dl_path[256];
    snprintf(dl_path, sizeof dl_path, "/tmp/thvm_jit_%016llx_%s.dylib",
             (unsigned long long)key, cpu_jit_opt_tag());
    struct stat st;
    if (stat(dl_path, &st) == 0) { free(src); continue; }  // on-disk cache hit

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

  int dbg = (getenv("THVM_CPU_JIT_PARALLEL_DBG") != NULL);
  u64 t0 = dbg ? cg_now_us() : 0;
  // Build ONE combined dylib holding every fresh kernel (entry points
  // renamed k_<hex>), then dlopen it ONCE.  This collapses the N slow
  // first-dlopen syspolicyd provenance checks (the dominant cold cost on
  // macOS) to a single check.  The single combined clang invocation
  // compiles all N kernels in one TU; on a 20-kernel forward that one
  // clang is comparable to the old ncores-wide parallel pool but pays
  // one fork/exec instead of N.
  char comb_dl[256] = {0};
  void *h = cpu_jit_build_combined(jobs, njobs, comb_dl, sizeof comb_dl);
  if (dbg)
    fprintf(stderr, "[jit-pre] combined-compiled %u kernels in %.0f ms (range [%u,%u))\n",
            njobs, (double)(cg_now_us() - t0) / 1000.0, kid_lo, kid_hi);

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
      CpuJitSlot *s = cpu_jit_lookup_slot(jobs[i].key);
      if (s != NULL && s->func == NULL) {
        s->key = jobs[i].key;
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
  if (!cpu_jit_eligible(ke)) return 0;
  // Per-kid fast path: if this kid was already compiled to a JIT fn
  // pointer, reuse it directly -- skips cpu_jit_render_canon (heap
  // walk into a tmpfile) and the source hash entirely.  The kid -> fn
  // binding is invariant once set; kernel_apply_opt only rewires
  // cached_lift BEFORE the first dispatch, after which a kid never
  // changes shape/source.  Skip the per-kid cache for kids that have
  // never compiled before (KID_JIT_FN[kid] == NULL) -- they fall
  // through to the full source-hash path which has its own warmup
  // gate + on-disk dylib reuse.
  u32 _kid = (u32)(ke - KERNELS);
  if (getenv("THVM_CPU_JIT_PARALLEL_DBG"))
    fprintf(stderr, "[jit-disp] kid=%u kidfn=%p\n", _kid,
            (KID_JIT_FN != NULL && _kid < KERNELS_CAP) ? KID_JIT_FN[_kid] : (void*)1);
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
  CpuJitSlot *s = cpu_jit_lookup_slot(key);
  CpuJitFn jfn = (s && s->key == key) ? s->func : NULL;

  if (jfn == NULL) {
    // Warmup gate: if the cached dylib doesn't already exist on
    // disk AND this kernel hasn't been fired enough times to be
    // worth a clang compile, bail to the interpreter.  Track the
    // pending hash in the cache slot's `fire_count` so the next
    // fire can decide.  Mirrors what tinygrad does implicitly via
    // its compile cache: one-shot kernels pay no compile cost;
    // training-loop kernels cross the threshold almost immediately.
    char dl_path[256];
    snprintf(dl_path, sizeof dl_path, "/tmp/thvm_jit_%016llx_%s.dylib",
             (unsigned long long)key, cpu_jit_opt_tag());
    struct stat st;
    int dl_exists = (stat(dl_path, &st) == 0);
    if (!dl_exists) {
      if (s != NULL) {
        u32 warmup = cpu_jit_warmup();
        if (s->key == 0) {                 // claim a fresh slot for tracking
          s->key = key;
          s->func = NULL;
          s->dl_handle = NULL;
          s->n_inputs = ke->n_inputs;
          s->fire_count = 1;
          if (warmup > 1) { free(src); return 0; }
          // warmup<=1: fall through to compile on first fire.
        } else if (s->key == key && s->func == NULL) {
          s->fire_count++;
          if (s->fire_count < warmup) { free(src); return 0; }
          // Crossed warmup threshold: fall through to compile.
        }
      } else {
        free(src);
        return 0;                          // table full; stay on interpreter
      }
    }
    jfn = cpu_jit_build(ke, key, src);
    if (!jfn) { free(src); return 0; }
  }
  free(src);                               // source no longer needed (hit or compiled)

  // Skip the JIT path if any input is a non-contiguous view --
  // gated behind THVM_JIT_STRIDED=1.  Each unique stride pattern
  // hits a fresh clang compile (cpu_jit_hash includes view stride
  // info), which doubles LeNet wallclock for one-shot training
  // kernels without persistent compile-cache amortization.
  // Render_uop_c (post-F6) handles non-contig views correctly via
  // kernel_lift's view.strides path -- the gate is purely a perf
  // knob, not a correctness one.
  // Strided/chained-input kernels (the conv im2col unfold reads the
  // _pool view) are now DEFAULT-JIT'd: the source-keyed cache amortizes
  // the compile across the whole training run (the old "doubles LeNet
  // wall" concern assumed the term-loc key never reused the dylib, so
  // every step recompiled).  THVM_JIT_STRIDED=0 reverts to the walker
  // for these.  A single conv2 unfold STORE drops from ~1s interpreted
  // to a compiled strided loop.
  static int strided_jit_known = 0, strided_jit_enabled = 1;
  if (!strided_jit_known) {
    char const *e = getenv("THVM_JIT_STRIDED");
    if (e != NULL) strided_jit_enabled = (e[0] != '0');
    strided_jit_known = 1;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 || tid >= TENS_NEXT) continue;
    if (TENS[tid].nviews > 0 || !TENS[tid].view.contiguous) {
      if (!strided_jit_enabled) return 0;
    }
  }
  // Cache the resolved (kid -> fn) so the next fire of this kid
  // skips render+hash+lookup (the early-return at the top of this
  // function).  Only cached when jfn is non-NULL AND the per-input
  // strided-jit gate above didn't reject -- this means a future call
  // with identical inputs is safe to short-circuit.
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

