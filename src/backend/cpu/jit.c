// backend/cpu/jit.c - shell out to clang to compile a generated
// C source into a .dylib, dlopen it, and cache the resolved
// `void k(...)` symbol against the program's hash so subsequent
// dispatches skip both compilation and dlsym.
//
// Per-kernel function signature (emitted by cg_render_uop_kernel_c):
//   void k(void *out_v,
//          const void *const *ins_v,
//          unsigned out_numel,
//          const unsigned *in_numels);
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
#endif
#include <unistd.h>
#include <sys/stat.h>

typedef void (*CpuJitFn)(void *out, const void *const *ins,
                         unsigned n, const unsigned *in_numels);

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
// Cleared on cpu_jit_cache_reset (thvm_init / thvm_free).
static CpuJitFn KID_JIT_FN[KERNELS_CAP];

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
  memset(KID_JIT_FN, 0, sizeof(KID_JIT_FN));
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
  snprintf(dl_path,  sizeof dl_path,  "/tmp/thvm_jit_%016llx.dylib",
           (unsigned long long)key);

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
  snprintf(cmd, sizeof cmd,
           "clang -O2 -fPIC -shared -o '%s' '%s' 2>/dev/null",
           dl_path, src_path);
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
  char *canon = cg_canonicalize_axis_ids(raw);
  if (canon != NULL) { free(raw); return canon; }
  return raw;
}

static u64 cpu_src_hash(const char *src) {
  u64 h = 0xcbf29ce484222325ULL;
  for (const char *p = src; *p; p++) { h ^= (u64)(unsigned char)*p; h *= 0x100000001b3ULL; }
  return h | (1ULL << 63);
}

// Try the JIT path; returns 1 on success, 0 if the kernel can't be
// JITted (caller dispatches via the interpreter).
fn int cpu_jit_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (!cg_supports(ke)) return 0;
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
  if (_kid < KERNELS_CAP && KID_JIT_FN[_kid] != NULL) {
    CpuJitFn _jfn = KID_JIT_FN[_kid];
    u32 ni = ke->n_inputs;
    const void *ins_buf [ni ? ni : 1];
    unsigned    nums_buf[ni ? ni : 1];
    for (u32 i = 0; i < ni; i++) {
      ins_buf [i] = CPU_BUFS[in_buf_ids[i]].data;
      nums_buf[i] = ke->input_numels[i];
    }
    void *out = CPU_BUFS[out_buf_id].data;
    unsigned numel = (unsigned)ke->output_numel;
    _jfn(out, ins_buf, numel, nums_buf);
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
    snprintf(dl_path, sizeof dl_path, "/tmp/thvm_jit_%016llx.dylib",
             (unsigned long long)key);
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
  if (_kid < KERNELS_CAP) KID_JIT_FN[_kid] = jfn;
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
  jfn(out, ins_buf, numel, nums_buf);
  return 1;
}

