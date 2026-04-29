// backend/cpu/jit.c - shell out to clang to compile a generated
// C source into a .dylib, dlopen it, and cache the resolved
// `void k(...)` symbol against the program's hash so subsequent
// dispatches skip both compilation and dlsym.
//
// Per-kernel function signature (matches codegen.c):
//   void k(float *out,
//          const float *in0, ..., const float *in{N-1},
//          unsigned out_numel,
//          unsigned in_numel0, ..., unsigned in_numel{N-1});
//
// Dispatch packs the input pointers + numels and calls through a
// generated trampoline -- variadic invocation isn't portable, but
// since N is bounded by the kernel's n_inputs we just walk the
// args manually and ferry through CpuJitFn (a function pointer
// whose signature matches the generated function).
//
// Cache key: KProgOp[] + n_inputs + per-input numel.  Numel goes
// in because the codegen embeds broadcast checks (numel==1) at
// emit time, so a program with the same KProgOp[] but different
// input numels needs a fresh build.

#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

typedef void (*CpuJitFn)(float *out, const float *const *ins,
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
  }
  u8 const *bytes = (u8 const *)ke->program;
  size_t total = (size_t)ke->n_ops * sizeof(KProgOp);
  for (size_t i = 0; i < total; i++) {
    h ^= (u64)bytes[i]; h *= 0x100000001b3ULL;
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
  // codegen reads in0[i] flat, with no stride support yet.  The
  // interpreter pre-materializes strided inputs into temp buffers.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid != 0 && tid < TENS_NEXT && !TENS[tid].view.contiguous) return 0;
  }
  // Pack input pointers + numels into local arrays and call the
  // generated function.  Stack-sized to ke->n_inputs (covered by
  // VLA below); no fixed cap beyond what the codegen supports.
  u32 ni = ke->n_inputs;
  const float *ins_buf  [ni ? ni : 1];
  unsigned     nums_buf [ni ? ni : 1];
  for (u32 i = 0; i < ni; i++) {
    ins_buf [i] = (const float *)CPU_BUFS[in_buf_ids[i]].data;
    nums_buf[i] = ke->input_numels[i];
  }
  float *out = (float *)CPU_BUFS[out_buf_id].data;
  unsigned numel = ke->program[ke->n_ops - 1].numel;
  jfn(out, ins_buf, numel, nums_buf);
  return 1;
}
