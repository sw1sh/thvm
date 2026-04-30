// backend/cpu/jit.c - shell out to clang to compile a generated
// C source into a .dylib, dlopen it, and cache the resolved
// `void k(...)` symbol against the program's hash so subsequent
// dispatches skip both compilation and dlsym.
//
// Per-kernel function signature (matches render_c.c):
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
typedef struct {
  u64       key;        // 0 = empty
  CpuJitFn  func;
  void     *dl_handle;
  u32       n_inputs;
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
  // Fold KernelAxes.applied_opts[] into the key so two kernels with
  // identical KProgOp[] but different opts get distinct .dylibs.
  // axis_types[] / full_shape[] are derived from applied_opts +
  // output_shape so applied_opts alone is sufficient.
  if (ke->axes != NULL) {
    KernelAxes const *ax = ke->axes;
    h ^= (u64)ax->n_applied; h *= 0x100000001b3ULL;
    for (u32 i = 0; i < ax->n_applied; i++) {
      KOpt o = ax->applied_opts[i];
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
    CPU_JIT_CACHE[i].key       = 0;
    CPU_JIT_CACHE[i].func        = NULL;
    CPU_JIT_CACHE[i].dl_handle = NULL;
    CPU_JIT_CACHE[i].n_inputs  = 0;
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
  // Render to C via the codegen/ pipeline.  cg_emit walks ke->program[]
  // and dispatches each KProgOp through the renderer; C_RENDERER lives
  // in src/codegen/render_c.c and supplies the C99-flavored bits.  A
  // future Metal renderer would slot in identically (pair with a Metal-
  // specific build path), and the rest of this file would be the only
  // bit needing per-backend orchestration.
  char *src = cg_emit(ke, &C_RENDERER);
  if (!src) return NULL;

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
  CpuJitFn jfn = (s && s->key == key) ? s->func : cpu_jit_build(ke, key);
  if (!jfn) return 0;

  // Skip the JIT path if any input is a non-contiguous view -- the
  // Codegen-time strided-index inlining (render_c emits
  // `in%u[s0*c0(i) + ... + offset]` for non-contig single-view
  // inputs, mirroring tinygrad's View.to_indexed_uops) is wired
  // and correct, but currently gated behind THVM_JIT_STRIDED=1.
  // Without the gate, every unique stride pattern hits a fresh
  // clang compile, doubling LeNet wallclock for one-shot training
  // kernels.  Tinygrad amortizes via aggressive persistent
  // caching; we'll enable by default once the cross-process JIT
  // cache is warm enough for training-loop strides.  Multi-view
  // chain inputs always bail (codegen would need to compose
  // through prior_views per access, not implemented).
  static int strided_jit_known = 0, strided_jit_enabled = 0;
  if (!strided_jit_known) {
    char const *e = getenv("THVM_JIT_STRIDED");
    strided_jit_enabled = (e && e[0] == '1');
    strided_jit_known = 1;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 || tid >= TENS_NEXT) continue;
    // Same gate for both single-view non-contig and multi-view
    // chains: render_c emits inline strided index expressions
    // (single-view) or a per-input idx helper (chain), both
    // composing to the buffer offset at codegen time.  Disable
    // by default until the JIT cache amortizes the per-stride-
    // pattern compile cost.
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
