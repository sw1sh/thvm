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
// Cache key: KProgOp[] + n_inputs + per-input numel.  Numel goes
// in because the codegen embeds broadcast checks (numel==1) at
// emit time, so a program with the same KProgOp[] but different
// input numels needs a fresh build.

#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

typedef void (*CpuJitFn)(void *out, const void *const *ins,
                         unsigned n, const unsigned *in_numels);

#define CPU_JIT_CACHE_CAP 256
// JIT warmup gate.  When a kernel hash hasn't been compiled yet
// (no in-memory `func`, no /tmp dylib on disk), the FIRST few
// fires use the interpreter and bump `fire_count`.  Once the
// counter hits CPU_JIT_WARMUP, the next fire commits to a clang
// compile.  Mirrors tinygrad's approach of NOT JIT-compiling
// rarely-fired kernels: a one-shot kernel pays no compile cost,
// while a kernel inside a training loop crosses the threshold
// almost immediately and amortizes the compile across the rest
// of the run.
#define CPU_JIT_WARMUP 5
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
  h ^= (u64)ke->n_ops; h *= 0x100000001b3ULL;
  h ^= (u64)ke->n_inputs; h *= 0x100000001b3ULL;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h ^= (u64)ke->input_numels[i]; h *= 0x100000001b3ULL;
    // Fold per-input view stride pattern (and full ShapeTracker
    // chain) into the key.  Same KProgOp[] with different input
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
  u8 const *bytes = (u8 const *)ke->program;
  size_t total = (size_t)ke->n_ops * sizeof(KProgOp);
  for (size_t i = 0; i < total; i++) {
    h ^= (u64)bytes[i]; h *= 0x100000001b3ULL;
  }
  // Fold applied_opts into the key via tile_anno facade so two
  // kernels with identical KProgOp[] but different opts get
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

fn void cpu_jit_cache_reset(void) {
  for (u32 i = 0; i < CPU_JIT_CACHE_CAP; i++) {
    if (CPU_JIT_CACHE[i].dl_handle) dlclose(CPU_JIT_CACHE[i].dl_handle);
    CPU_JIT_CACHE[i].key        = 0;
    CPU_JIT_CACHE[i].func       = NULL;
    CPU_JIT_CACHE[i].dl_handle  = NULL;
    CPU_JIT_CACHE[i].n_inputs   = 0;
    CPU_JIT_CACHE[i].fire_count = 0;
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
static CpuJitFn cpu_jit_build(KernelEntry const *ke, u64 key) {
  // Render the kernel as C99 by lifting to the UOp DAG and walking
  // it via cg_render_uop_kernel_c. Output signature matches the
  // existing CPU-JIT contract (void k(out_v, ins_v, n, in_numels))
  // so the surrounding compile/dlopen/dlsym code is unchanged.
  //
  // F6 step 15 (2026-05-08): the legacy cg_emit + render_c.c
  // KProgOp-flat path is gone after a passing 1851-test validation
  // and a default-on flip in fc40c60a. Kernels the lifter declines
  // (lift_to_uop returns 0) bail to the interpreter naturally; no
  // silent JIT compile of a wrong kernel.
  KernelUopLift lift = {0};
  if (!kernel_lift_to_uop(ke, &lift)) return NULL;
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_c(lift.store_root, "k", lift.out_buf,
                         lift.in_bufs, lift.n_inputs, fp);
  long n = ftell(fp);
  fclose(fp);
  if (n <= 0) return NULL;
  char *src = (char *)malloc((size_t)n + 1);
  if (src == NULL) return NULL;
  memcpy(src, buf, (size_t)n);
  src[n] = '\0';

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
    free(src);
    void *h = dlopen(dl_path, RTLD_NOW | RTLD_LOCAL);
    if (!h) return NULL;
    CpuJitFn jfn = (CpuJitFn)dlsym(h, "k");
    if (!jfn) { dlclose(h); return NULL; }
    CpuJitSlot *s = cpu_jit_lookup_slot(key);
    if (s) { s->key = key; s->func = jfn; s->dl_handle = h; s->n_inputs = ke->n_inputs; }
    return jfn;
  }

  FILE *f = fopen(src_path, "w");
  if (!f) { free(src); return NULL; }
  fputs(src, f);
  fclose(f);
  free(src);

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

// Try the JIT path; returns 1 on success, 0 if the kernel can't be
// JITted (caller dispatches via the interpreter).
fn int cpu_jit_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (!cg_supports(ke)) return 0;
  u64 key = cpu_jit_hash(ke);
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
        if (s->key == 0) {                 // claim a fresh slot for tracking
          s->key = key;
          s->func = NULL;
          s->dl_handle = NULL;
          s->n_inputs = ke->n_inputs;
          s->fire_count = 1;
          return 0;
        }
        if (s->key == key && s->func == NULL) {
          s->fire_count++;
          if (s->fire_count < CPU_JIT_WARMUP) return 0;
          // Crossed warmup threshold: fall through to compile.
        }
      } else {
        return 0;                          // table full; stay on interpreter
      }
    }
    jfn = cpu_jit_build(ke, key);
    if (!jfn) return 0;
  }

  // Skip the JIT path if any input is a non-contiguous view --
  // gated behind THVM_JIT_STRIDED=1.  Each unique stride pattern
  // hits a fresh clang compile (cpu_jit_hash includes view stride
  // info), which doubles LeNet wallclock for one-shot training
  // kernels without persistent compile-cache amortization.
  // Render_uop_c (post-F6) handles non-contig views correctly via
  // kernel_lift's view.strides path -- the gate is purely a perf
  // knob, not a correctness one.
  static int strided_jit_known = 0, strided_jit_enabled = 0;
  if (!strided_jit_known) {
    char const *e = getenv("THVM_JIT_STRIDED");
    strided_jit_enabled = (e && e[0] == '1');
    strided_jit_known = 1;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 || tid >= TENS_NEXT) continue;
    if (TENS[tid].nviews > 0 || !TENS[tid].view.contiguous) {
      if (!strided_jit_enabled) return 0;
    }
  }
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
  unsigned numel = ke->program[ke->n_ops - 1].numel;
  jfn(out, ins_buf, numel, nums_buf);
  return 1;
}

