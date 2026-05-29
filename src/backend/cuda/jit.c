// backend/cuda/jit.c - take a rendered .cu kernel string, compile it
// with nvrtc to PTX, load the PTX as a CUDA module, resolve the
// `extern "C" __global__ void k(...)` entry point, and launch it.
//
// Mirrors backend/cpu/jit.c (clang -> .dylib -> dlopen -> dlsym) with
// the CUDA toolchain in place of the host one:
//
//   cg_render_uop_kernel_cuda_root  ->  .cu source string
//   nvrtcCreateProgram + nvrtcCompileProgram --gpu-architecture=...
//                                   ->  PTX
//   cuModuleLoadData                ->  CUmodule
//   cuModuleGetFunction             ->  CUfunction
//   cuLaunchKernel                  ->  run
//
// The nvrtc --gpu-architecture target tracks the live device's compute
// capability (cuda_device_sm()); on the V100 pod that is sm_70.  Volta
// has no tf32, so the render-side WMMA path is gated to fp16 buffers
// (see render_uop.c rmu_emit_matmul_tc) and an fp32 matmul takes the
// scalar tiled-accumulator fallback -- nothing nvrtc-version-specific
// here, the gate is purely on buffer dtype.
//
// The module cache keys CUmodule + CUfunction on an FNV hash of the
// rendered source so a repeat launch of the same kernel skips both
// the nvrtc compile and the module load.

// Direct-mapped, evicting module cache.  Sized to comfortably hold one
// training step's stable forward/backward kernels (~120) while the
// step-varying kernels (Adam bakes the bias-correction 1-b1^t / lr as
// CONST literals, so its source changes every step) evict cleanly.
// Without eviction those step-unique modules leaked to end-of-session
// (cuModuleLoadData reserves GPU code memory), saturating the device
// after a handful of no-JIT steps -- the dominant CUDA training leak.
#define CUDA_JIT_CACHE_CAP 4096

typedef struct {
  u64        key;        // 0 = empty slot; FNV hash of the .cu source
  CUmodule   module;
  CUfunction func;
} CudaJitSlot;
static CudaJitSlot CUDA_JIT_CACHE[CUDA_JIT_CACHE_CAP];

// Instrumentation: count nvrtc compiles (cache misses) and evictions
// (a home-slot collision unloading a still-cached different-key module).
// Exposed to Python to test whether the eager train's step-N nan
// correlates with cache thrashing.
u64 CUDA_JIT_COMPILES = 0;
u64 CUDA_JIT_EVICTIONS = 0;
u64 CUDA_JIT_PTX_LOADS = 0;   // of the compiles, how many were PTX passthrough
fn u64 cuda_jit_compiles(void)  { return CUDA_JIT_COMPILES; }
fn u64 cuda_jit_evictions(void) { return CUDA_JIT_EVICTIONS; }
fn u64 cuda_jit_ptx_loads(void) { return CUDA_JIT_PTX_LOADS; }

// Forward-declared in init.c so cuda_shutdown can unload every cached
// module before the context is destroyed.
fn void cuda_jit_cache_reset(void) {
  for (u32 i = 0; i < CUDA_JIT_CACHE_CAP; i++) {
    if (CUDA_JIT_CACHE[i].module != NULL) {
      cuModuleUnload(CUDA_JIT_CACHE[i].module);
    }
    CUDA_JIT_CACHE[i].key    = 0;
    CUDA_JIT_CACHE[i].module = NULL;
    CUDA_JIT_CACHE[i].func   = NULL;
  }
}

static u64 cuda_jit_hash(const char *src) {
  u64 h = 0xcbf29ce484222325ULL;
  for (const char *p = src; *p; p++) {
    h ^= (u64)(unsigned char)*p;
    h *= 0x100000001b3ULL;
  }
  return h | (1ULL << 63);   // never 0 (the empty-slot sentinel)
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// --- on-disk PTX cache (tinygrad parity) ---------------------------------
// nvrtc compilation is the dominant warmup cost (~5s for beautiful_mnist
// fwd alone, ~30+s with bwd).  In-process CudaJitSlot only deduplicates
// within a single Python invocation; tinygrad keeps a sqlite-backed disk
// cache so a second `python beautiful_mnist.py` reuses the prior run's
// compiled PTX.  thvm's port is a flat directory of .ptx files keyed on
// FNV(src + arch + fast_math_flag).  Default ON; THVM_CUDA_DISK_CACHE=0
// disables.  Lives under XDG_CACHE_HOME or ~/.cache/thvm/cuda_ptx/.
static int cuda_disk_cache_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_CUDA_DISK_CACHE");
    enabled = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
    known = 1;
  }
  return enabled;
}

static int cuda_disk_cache_path(u64 cache_key, char *out, size_t cap) {
  char const *base = getenv("THVM_CUDA_CACHE_DIR");
  char default_dir[1024];
  if (base == NULL || base[0] == '\0') {
    char const *xdg = getenv("XDG_CACHE_HOME");
    char const *home = getenv("HOME");
    if (xdg != NULL && xdg[0] != '\0') {
      snprintf(default_dir, sizeof default_dir, "%s/thvm/cuda_ptx", xdg);
    } else if (home != NULL && home[0] != '\0') {
      snprintf(default_dir, sizeof default_dir, "%s/.cache/thvm/cuda_ptx", home);
    } else {
      snprintf(default_dir, sizeof default_dir, "/tmp/thvm-cuda-ptx");
    }
    base = default_dir;
  }
  // Best-effort mkdir -p (cheap; ENOENT on partial parent path is fine
  // -- the open() below will fail and the caller falls through to nvrtc).
  {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", base);
    for (char *p = tmp + 1; *p; p++) {
      if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
  }
  int n = snprintf(out, cap, "%s/%016llx.ptx", base, (unsigned long long)cache_key);
  return (n > 0 && (size_t)n < cap) ? 1 : 0;
}

// Returned buffer is malloc'd + NUL-terminated; caller owns it.
// Returns NULL on miss (no entry, or read error).
static char *cuda_disk_cache_get(u64 cache_key) {
  if (!cuda_disk_cache_enabled()) return NULL;
  char path[1024];
  if (!cuda_disk_cache_path(cache_key, path, sizeof path)) return NULL;
  FILE *f = fopen(path, "rb");
  if (f == NULL) return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  if (sz <= 0 || sz > (long)(64 * 1024 * 1024)) { fclose(f); return NULL; }
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)sz + 1);
  if (buf == NULL) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (got != (size_t)sz) { free(buf); return NULL; }
  buf[sz] = '\0';
  return buf;
}

static void cuda_disk_cache_put(u64 cache_key, const char *ptx, size_t ptx_sz) {
  if (!cuda_disk_cache_enabled()) return;
  char path[1024];
  if (!cuda_disk_cache_path(cache_key, path, sizeof path)) return;
  // Atomic-ish: write to tmp then rename so a concurrent reader never sees
  // a partial file.  Two writers racing produce identical content (deterministic
  // nvrtc output for the same src + opts), so a last-writer-wins rename is safe.
  char tmp[1100];
  snprintf(tmp, sizeof tmp, "%s.tmp.%d", path, (int)getpid());
  FILE *f = fopen(tmp, "wb");
  if (f == NULL) return;
  size_t wrote = fwrite(ptx, 1, ptx_sz, f);
  fclose(f);
  if (wrote != ptx_sz) { unlink(tmp); return; }
  if (rename(tmp, path) != 0) unlink(tmp);
}

// Telemetry: count disk-cache hits and writes.
u64 CUDA_JIT_DISK_HITS  = 0;
u64 CUDA_JIT_DISK_WRITES = 0;
fn u64 cuda_jit_disk_hits(void)   { return CUDA_JIT_DISK_HITS;  }
fn u64 cuda_jit_disk_writes(void) { return CUDA_JIT_DISK_WRITES; }

// Open-addressed (linear probe).  A training run's kernel set is stable
// and bounded (~140 distinct sources for beautiful_mnist), so with a
// 4096-slot table they all coexist and every step after the first HITS
// -- no recompile, no module thrash.  The OLD direct-mapped cache
// evicted on every home-slot collision, so ~14 collision pairs among
// 121 kernels (birthday) ping-pong-evicted under interleaved dispatch
// -> ~140 spurious recompiles/step -> the eviction nan + the ~10s/step
// wall.  Eviction now happens only when the table is genuinely full
// (a degenerate workload), via cuda_jit_probe returning a full marker.
static CudaJitSlot *cuda_jit_home_slot(u64 key) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return &CUDA_JIT_CACHE[h & (CUDA_JIT_CACHE_CAP - 1)];
}

// Linear-probe lookup.  Returns the slot holding `key` (a hit, *found=1),
// or the first empty slot on its probe chain (a miss to insert into,
// *found=0), or -- only when the whole table is occupied -- the home slot
// to evict (*found=-1).
static CudaJitSlot *cuda_jit_probe(u64 key, int *found) {
  u32 h0 = (u32)(key ^ (key >> 32));
  h0 ^= h0 >> 13; h0 *= 0x5bd1e995u; h0 ^= h0 >> 15;
  h0 &= (CUDA_JIT_CACHE_CAP - 1);
  for (u32 i = 0; i < CUDA_JIT_CACHE_CAP; i++) {
    CudaJitSlot *s = &CUDA_JIT_CACHE[(h0 + i) & (CUDA_JIT_CACHE_CAP - 1)];
    if (s->key == key && s->func != NULL) { *found = 1; return s; }
    if (s->key == 0) { *found = 0; return s; }   // empty -> insert here
  }
  *found = -1;
  return &CUDA_JIT_CACHE[h0];   // table full -> evict home (degenerate)
}

// Detect a PTX-assembly source (emitted by render_ptx.c) vs a CUDA-C
// source (render_uop / render_linearized).  PTX modules start with a
// `.version` directive (after optional leading whitespace).
static int cuda_src_is_ptx(const char *s) {
  if (s == NULL) return 0;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  return strncmp(s, ".version", 8) == 0;
}

// Compile a CUDA-C source string with nvrtc (or load a PTX source
// directly, bypassing nvrtc) and load it as a module.  Returns a
// resolved CUfunction for `kernel_name`, or NULL on any nvrtc / driver
// failure (the reason lands in thvm_cuda_last_error).
fn CUfunction cuda_jit_compile(const char *cu_src, const char *kernel_name) {
  if (!CUDA_READY) {
    snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR,
             "cuda_jit_compile: backend not initialised");
    return NULL;
  }
  u64 key = cuda_jit_hash(cu_src);
  int found = 0;
  CudaJitSlot *slot = cuda_jit_probe(key, &found);
  if (found == 1) {
    return slot->func;   // cache hit -- skip nvrtc + module load
  }
  CUDA_JIT_COMPILES++;
  if (found == -1 && slot->module != NULL) CUDA_JIT_EVICTIONS++;
  // THVM_CUDA_LOG_COMPILES=1: one line per nvrtc compile, with the
  // source size + first source line as a fingerprint.  Used to
  // diagnose mid-stream cache-miss waves (e.g. step-4 spike from
  // step-varying CONSTs baked into kernel source).
  {
    static int trace_init = 0;
    static int trace_on   = 0;
    if (!trace_init) {
      char const *e = getenv("THVM_CUDA_LOG_COMPILES");
      trace_on = (e != NULL && e[0] != '0');
      trace_init = 1;
    }
    if (trace_on) {
      size_t sz = 0; while (cu_src[sz] != '\0' && sz < 1000000) sz++;
      // Fingerprint: find the LAST store statement (output address).
      // The output STORE is the kernel's unique signature -- different
      // output shapes have different store addresses.
      char last_out[120] = {0};
      const char *p = cu_src + sz;
      while (p > cu_src && *(p-1) != '\n') p--;  // start of last line
      // Walk back past empty closing braces
      while (p > cu_src) {
        const char *line_start = p;
        while (line_start > cu_src && *(line_start-1) != '\n') line_start--;
        if (strstr(line_start, "out[") || strstr(line_start, " = _acc")) {
          size_t i = 0;
          while (line_start[i] && line_start[i] != '\n' && i < 119) {
            last_out[i] = line_start[i]; i++;
          }
          last_out[i] = '\0';
          break;
        }
        p = line_start - 1;
        if (p <= cu_src) break;
      }
      fprintf(stderr, "[cuda-compile #%llu] %s sz=%zu %.100s\n",
              (unsigned long long)CUDA_JIT_COMPILES,
              cuda_src_is_ptx(cu_src) ? "PTX  " : "nvrtc",
              sz, last_out[0] ? last_out : "(no store found)");
      fflush(stderr);
    }
  }

  // PTX passthrough: if the rendered source is already PTX assembly
  // (the render_ptx.c emitter, detected by a leading `.version`), skip
  // nvrtc entirely and hand the text straight to cuModuleLoadData.  This
  // is the whole point of the PTX renderer -- bypass nvrtc's C++ frontend,
  // which is what blows up on the opt-rich conv kernels.  `ptx` borrows
  // cu_src in this branch (ptx_owned == 0, so we don't free it).
  char *ptx = NULL;
  int   ptx_owned = 0;
  int   is_ptx = cuda_src_is_ptx(cu_src);

  if (is_ptx) {
    ptx = (char *)cu_src;   // borrowed; freed by the caller's render buffer
    CUDA_JIT_PTX_LOADS++;
    if (getenv("THVM_CUDA_DUMP_ALL_PTX")) {
      fprintf(stderr, "--- our PTX (#%llu) ---\n%s--- end ---\n",
              (unsigned long long)CUDA_JIT_PTX_LOADS, ptx);
    }
  } else {
    // --- nvrtc compile (with on-disk PTX cache lookup first) -------
    char arch_opt[32];
    int sm = cuda_device_sm();
    if (sm <= 0) sm = 70;   // safe default if the probe failed
    snprintf(arch_opt, sizeof arch_opt, "--gpu-architecture=compute_%d", sm);
    char const *_fme = getenv("THVM_CUDA_FAST_MATH");
    int fast_math_on = (_fme != NULL && _fme[0] == '1');

    // Disk cache key: hash src bytes, then mix in arch sm + fast_math
    // flag so PTX compiled for a different target doesn't collide.
    u64 disk_key = key;
    disk_key ^= ((u64)sm) << 1;
    disk_key ^= ((u64)fast_math_on) << 33;
    disk_key |= (1ULL << 63);

    char *cached_ptx = cuda_disk_cache_get(disk_key);
    if (cached_ptx != NULL) {
      ptx = cached_ptx;
      ptx_owned = 1;
      CUDA_JIT_DISK_HITS++;
      goto module_load;
    }

    nvrtcProgram prog;
    nvrtcResult nr = nvrtcCreateProgram(&prog, cu_src, kernel_name,
                                        0, NULL, NULL);
    if (nr != NVRTC_SUCCESS) {
      snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR,
               "nvrtcCreateProgram: %s", nvrtcGetErrorString(nr));
      return NULL;
    }
    // --use_fast_math: enable fast-math intrinsics (rsqrt -> rsqrtf,
    // div -> approx, etc.).  Default ON; THVM_CUDA_NO_FAST_MATH=1
    // restores precise math.  For beautiful_mnist / similar training
    // workloads the small numeric difference doesn't change loss
    // convergence; tinygrad's CUDA renderer enables similar
    // intrinsics via UOP_OPT(_, FAST_MATH, _) wraps.
    const char *opts[16];
    u32 n_opts = 0;
    opts[n_opts++] = arch_opt;
    // nvrtc has no default filesystem include search path, so a WMMA
    // tensor-core matmul kernel's `#include <mma.h>` fails to compile
    // ("could not open source file mma.h") -- the kernel then never
    // dispatches, its output stays zero, the forward produces degenerate
    // logits, and training stalls at the random-guess loss with frozen
    // params.  Mirror tinygrad (runtime/support/compiler_cuda.py): point
    // nvrtc at the toolkit include dir from $CUDA_PATH/$CUDA_HOME, else
    // the standard locations.
    char inc_opt[512];
    const char *cuda_path = getenv("CUDA_PATH");
    if (cuda_path == NULL || cuda_path[0] == '\0') cuda_path = getenv("CUDA_HOME");
    if (cuda_path != NULL && cuda_path[0] != '\0') {
      snprintf(inc_opt, sizeof inc_opt, "-I%s/include", cuda_path);
      opts[n_opts++] = inc_opt;
    } else {
      opts[n_opts++] = "-I/usr/local/cuda/include";
      opts[n_opts++] = "-I/usr/include";
      opts[n_opts++] = "-I/opt/cuda/include";
    }
    // Default OFF: a 10% regression at BS=64 surfaced when
    // --use_fast_math was on by default (warm 154ms -> 140ms with
    // THVM_CUDA_NO_FAST_MATH=1).  Opt in per workload until cross-BS
    // validation proves a positive default; loss byte-identical at
    // BS=128 with vs without it, but BS=64 hurts measurably.
    // THVM_CUDA_FAST_MATH=1 enables (fast_math_on captured above).
    if (fast_math_on) {
      opts[n_opts++] = "--use_fast_math";
      // --ftz=true flushes denormals to zero (V100 fp32 normally takes
      // denormal stalls).  Implied by --use_fast_math on some toolkits
      // but explicit is safer.
      opts[n_opts++] = "--ftz=true";
    }
    // NOTE: --restrict would be wrong here -- arena view buffers can
    // share the same dptr via parent_buf_id at the runtime layer, and
    // two kernel args holding view bufs of the same arena alias each
    // other.  Don't enable.
    nr = nvrtcCompileProgram(prog, (int)n_opts, opts);
    if (nr != NVRTC_SUCCESS) {
      size_t log_sz = 0;
      nvrtcGetProgramLogSize(prog, &log_sz);
      char *log = (char *)malloc(log_sz + 1);
      if (log != NULL) {
        nvrtcGetProgramLog(prog, log);
        log[log_sz] = '\0';
        fprintf(stderr, "thvm: nvrtc compile failed:\n%s\n", log);
        snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR,
                 "nvrtcCompileProgram: %s (see stderr for log)",
                 nvrtcGetErrorString(nr));
        free(log);
      }
      nvrtcDestroyProgram(&prog);
      return NULL;
    }
    size_t ptx_sz = 0;
    nvrtcGetPTXSize(prog, &ptx_sz);
    ptx = (char *)malloc(ptx_sz);
    if (ptx == NULL) {
      nvrtcDestroyProgram(&prog);
      snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR, "PTX alloc failed");
      return NULL;
    }
    nvrtcGetPTX(prog, ptx);
    nvrtcDestroyProgram(&prog);
    ptx_owned = 1;
    // Persist to disk so subsequent invocations skip nvrtc entirely.
    // ptx_sz includes the trailing NUL nvrtcGetPTX writes.
    cuda_disk_cache_put(disk_key, ptx, ptx_sz > 0 ? ptx_sz - 1 : 0);
    CUDA_JIT_DISK_WRITES++;
  }

module_load:
  // --- module load + function lookup -------------------------------
  CUmodule module;
  CUresult cr = cuModuleLoadData(&module, ptx);
  if (cr != CUDA_SUCCESS) {
    if (getenv("THVM_CUDA_DUMP_FAIL_PTX")) {
      fprintf(stderr, "--- INVALID PTX (cuModuleLoadData failed) ---\n%s\n--- end ---\n", ptx);
    }
    if (ptx_owned) free(ptx);
    cuda_set_error("cuModuleLoadData", cr);
    return NULL;
  }
  if (ptx_owned) free(ptx);
  CUfunction func;
  cr = cuModuleGetFunction(&func, module, kernel_name);
  if (cr != CUDA_SUCCESS) {
    cuda_set_error("cuModuleGetFunction", cr);
    cuModuleUnload(module);
    return NULL;
  }
  // Evict the prior occupant of this home slot (a different key) and
  // unload its module so GPU code memory stays bounded at CACHE_CAP
  // modules.  Safe in the synchronous dispatch model: every launch
  // cuCtxSynchronize's before the next compile, so an evicted module's
  // CUfunction is never in flight.  jit_replay re-dispatches through
  // this cache, so an evicted kernel simply recompiles -- never a stale
  // func pointer.
  if (slot->module != NULL && slot->key != key) {
    cuModuleUnload(slot->module);
  }
  slot->key    = key;
  slot->module = module;
  slot->func   = func;
  return func;
}

// Launch a compiled kernel.  `args` is an array of n_args pointers,
// each pointing at one kernel argument value (CUdeviceptr for buffer
// pointers, unsigned for kvar args) -- exactly the cuLaunchKernel
// extra-params convention.  Grid / block are 1-D (the CUDA render
// flattens its iteration space onto a 1-D tid); the caller computes
// them from the kernel's output extents (and, for a WMMA kernel,
// grid = tiles*32, see render caveat 3).
fn int cuda_jit_launch(CUfunction func,
                       u32 grid_x, u32 block_x,
                       void **args) {
  CUresult r = cuLaunchKernel(func,
                              grid_x, 1, 1,      // grid dim
                              block_x, 1, 1,     // block dim
                              0,                 // shared mem bytes
                              CUDA_CUR_STREAM,   // 0/NULL = default sync
                              args, NULL);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuLaunchKernel", r);
    if (getenv("THVM_CUDA_LAUNCH_TRACE")) {
      fprintf(stderr, "thvm: cuLaunchKernel FAIL grid=%u block=%u err=%d\n",
              grid_x, block_x, (int)r);
    }
    return -1;
  }
  // Per-launch cuCtxSynchronize was hiding the CUDA driver's ability
  // to PIPELINE launches: every kernel made the CPU block until the
  // GPU finished, serializing what should be an asynchronous stream.
  // The default-stream memcpyDtoH used by loss.item() / .numpy() is
  // synchronous and naturally enforces "wait for all prior kernels
  // before reading data back," so removing this per-launch sync is
  // semantically equivalent for the user but lets the GPU queue many
  // kernels deep.  Set THVM_CUDA_SYNC=1 to restore the old behavior
  // for debugging (so a misbehaving kernel surfaces its error at the
  // exact launch that triggered it instead of at the next memcpy).
  static int sync_init = 0, sync_on = 0;
  if (!sync_init) {
    char const *e = getenv("THVM_CUDA_SYNC");
    sync_on = (e != NULL && e[0] != '0');
    sync_init = 1;
  }
  if (sync_on) {
    r = cuCtxSynchronize();
    if (r != CUDA_SUCCESS) {
      cuda_set_error("cuCtxSynchronize", r);
      if (getenv("THVM_CUDA_LAUNCH_TRACE")) {
        fprintf(stderr, "thvm: cuCtxSync FAIL after kernel grid=%u block=%u err=%d\n",
                grid_x, block_x, (int)r);
      }
      return -1;
    }
  }
  return 0;
}

// CUDA Graph capture/replay -- amortise per-launch overhead across the
// JIT replay sequence.  tinygrad's CUDA backend captures the captured
// dispatch sequence into a CUgraph on the first run, then cuGraphLaunch
// (a single driver call) on every subsequent run.  beautiful_mnist on
// V100 sees the captured ~120 kernels replay in ~1.6-3ms vs ~100ms+
// of per-launch overhead via individual cuLaunchKernel.
//
// THVM_CUDA_JIT_GRAPH=0 disables (forces per-launch fallback for A/B).
fn int cuda_jit_graph_enabled(void) {
  static int init = 0, on = 1;
  if (!init) {
    char const *e = getenv("THVM_CUDA_JIT_GRAPH");
    on = (e == NULL || e[0] != '0');
    init = 1;
  }
  return on;
}

// Cached fast path: if the slot already has an instantiated CUgraphExec,
// launch it and return 0.  Caller can skip the per-op replay loop.
static u64 CUDA_JIT_GRAPH_HITS = 0;
static u64 CUDA_JIT_GRAPH_CAPS = 0;
fn u64 cuda_jit_graph_hits(void) { return CUDA_JIT_GRAPH_HITS; }
fn u64 cuda_jit_graph_captures(void) { return CUDA_JIT_GRAPH_CAPS; }

fn int cuda_jit_graph_replay(u32 slot) {
  if (!cuda_jit_graph_enabled()) return -1;
  if (!CUDA_READY) return -1;
  if (slot >= CUDA_JIT_GRAPH_CACHE_NSLOTS) return -1;
  if (!CUDA_JIT_GRAPH_CACHE[slot].valid) return -1;
  CUDA_JIT_GRAPH_HITS++;
  // Drain any pending default-stream work before launching the captured
  // graph on the capture stream.  TSet / cuMemcpyHtoD on default stream
  // is host-synchronous (blocks until the copy is queued) but does NOT
  // synchronize with other streams.  Without this drain, a fresh input
  // upload could race the graph's read of that input.
  cuStreamSynchronize(NULL);
  u64 t_pre = 0, t_launch = 0;
  int graph_trace = getenv("THVM_CUDA_JIT_GRAPH_TIME") != NULL;
  if (graph_trace) t_pre = cg_now_us();
  CUresult r = cuGraphLaunch(CUDA_JIT_GRAPH_CACHE[slot].exec,
                             CUDA_CAPTURE_STREAM);
  if (graph_trace) {
    t_launch = cg_now_us() - t_pre;
    cuStreamSynchronize(CUDA_CAPTURE_STREAM);
    u64 t_sync = cg_now_us() - t_pre - t_launch;
    fprintf(stderr, "[graph] launch=%llu us, sync=%llu us\n",
            (unsigned long long)t_launch, (unsigned long long)t_sync);
  }
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuGraphLaunch", r);
    if (getenv("THVM_CUDA_JIT_GRAPH_TRACE")) {
      fprintf(stderr, "thvm: cuGraphLaunch FAIL slot=%u err=%d\n",
              slot, (int)r);
    }
    return -1;
  }
  // No cuStreamSynchronize here -- the natural sync point is the next
  // user-visible CPU read (loss.numpy() -> cuda_buf_read).  Forcing a
  // sync per replay defeats the pipelining win that's the whole point
  // of cuGraphLaunch + async dispatch.  cuda_buf_read syncs the
  // capture stream before reading; cuda_buf_write also issues an
  // explicit cuEventRecord so the next graph launch waits on the
  // upload (see buf_read.c / buf_write.c).
  return 0;
}

// Begin a fresh capture into CUDA_CAPTURE_STREAM.  Subsequent
// cuLaunchKernel / cuMemcpyDtoDAsync calls (which honor
// CUDA_CUR_STREAM) get recorded into the in-flight graph instead of
// running directly.  Returns 0 on success.
fn int cuda_jit_graph_capture_begin(void) {
  if (!cuda_jit_graph_enabled()) return -1;
  if (!CUDA_READY) return -1;
  if (CUDA_CAPTURE_STREAM == NULL) {
    CUresult r = cuStreamCreate(&CUDA_CAPTURE_STREAM, CU_STREAM_NON_BLOCKING);
    if (r != CUDA_SUCCESS) {
      cuda_set_error("cuStreamCreate(capture)", r);
      CUDA_CAPTURE_STREAM = NULL;
      return -1;
    }
  }
  // THREAD_LOCAL: only this thread's CUDA ops are captured; other
  // threads can do unrelated CUDA work concurrently.  GLOBAL would
  // serialize the whole process.
  CUresult r = cuStreamBeginCapture(CUDA_CAPTURE_STREAM,
                                    CU_STREAM_CAPTURE_MODE_THREAD_LOCAL);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuStreamBeginCapture", r);
    if (getenv("THVM_CUDA_JIT_GRAPH_TRACE")) {
      fprintf(stderr, "thvm: cuStreamBeginCapture FAIL err=%d\n", (int)r);
    }
    return -1;
  }
  CUDA_CUR_STREAM = CUDA_CAPTURE_STREAM;
  return 0;
}

// Finalize the in-flight capture, instantiate as a CUgraphExec, cache
// for this slot, then launch it immediately (the caller wanted the
// dispatches to actually happen this turn too).  Returns 0 on success.
// On failure, the cache stays invalid and the caller falls back to the
// already-completed per-launch path -- nothing was lost since the
// capture itself executed nothing.
fn int cuda_jit_graph_capture_end_and_launch(u32 slot) {
  if (CUDA_CAPTURE_STREAM == NULL) return -1;
  if (slot >= CUDA_JIT_GRAPH_CACHE_NSLOTS) {
    cuStreamEndCapture(CUDA_CAPTURE_STREAM, NULL);
    CUDA_CUR_STREAM = NULL;
    return -1;
  }
  CUgraph g = NULL;
  CUresult r = cuStreamEndCapture(CUDA_CAPTURE_STREAM, &g);
  CUDA_CUR_STREAM = NULL;
  if (r != CUDA_SUCCESS || g == NULL) {
    cuda_set_error("cuStreamEndCapture", r);
    if (getenv("THVM_CUDA_JIT_GRAPH_TRACE")) {
      fprintf(stderr, "thvm: cuStreamEndCapture FAIL err=%d\n", (int)r);
    }
    return -1;
  }
  CUgraphExec exec = NULL;
  r = cuGraphInstantiate(&exec, g, 0);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuGraphInstantiate", r);
    if (getenv("THVM_CUDA_JIT_GRAPH_TRACE")) {
      fprintf(stderr, "thvm: cuGraphInstantiate FAIL err=%d\n", (int)r);
    }
    cuGraphDestroy(g);
    return -1;
  }
  // Replace any prior cached graph for this slot (jit recapture path).
  if (CUDA_JIT_GRAPH_CACHE[slot].valid) {
    cuGraphExecDestroy(CUDA_JIT_GRAPH_CACHE[slot].exec);
    cuGraphDestroy   (CUDA_JIT_GRAPH_CACHE[slot].graph);
  }
  CUDA_JIT_GRAPH_CACHE[slot].graph = g;
  CUDA_JIT_GRAPH_CACHE[slot].exec  = exec;
  CUDA_JIT_GRAPH_CACHE[slot].valid = 1;
  CUDA_JIT_GRAPH_CAPS++;
  if (getenv("THVM_CUDA_JIT_GRAPH_TRACE")) {
    size_t n_nodes = 0;
    cuGraphGetNodes(g, NULL, &n_nodes);
    fprintf(stderr, "thvm: cuGraph captured slot=%u nodes=%zu\n",
            slot, n_nodes);
  }
  // Launch the cached graph now so the captured work actually runs.
  // No host-side sync -- the next cuda_buf_read on the result tensor
  // syncs the capture stream as needed (matches the per-launch async
  // pipeline that cuGraphLaunch is meant to amortise).
  r = cuGraphLaunch(exec, CUDA_CAPTURE_STREAM);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuGraphLaunch(first)", r);
    return -1;
  }
  return 0;
}

// Slot recapture / jit_capture_drop hook: invalidate any cached graph
// so the next replay re-captures the (possibly changed) sequence.
fn void cuda_jit_graph_invalidate(u32 slot) {
  if (slot >= CUDA_JIT_GRAPH_CACHE_NSLOTS) return;
  if (!CUDA_JIT_GRAPH_CACHE[slot].valid) return;
  cuGraphExecDestroy(CUDA_JIT_GRAPH_CACHE[slot].exec);
  cuGraphDestroy   (CUDA_JIT_GRAPH_CACHE[slot].graph);
  CUDA_JIT_GRAPH_CACHE[slot].exec  = NULL;
  CUDA_JIT_GRAPH_CACHE[slot].graph = NULL;
  CUDA_JIT_GRAPH_CACHE[slot].valid = 0;
}

// DAG-mode dispatch geometry: derive (grid_x, block_x) from the lifted
// UOp DAG's RANGE-leaf axis types/extents, the CUDA counterpart of
// render_metal.c's rmt_dag_dispatch_shape.  The CUDA structural
// renderer decodes axes from the SAME tg/tt convention as Metal
// (cg_render_uop_kernel_cuda_root: tg = blockIdx.x, tt = threadIdx.x):
//   - promoted output axes  (KAX_LOOP)         -> the `tg` grid range
//   - in-thread loops       (KAX_UPCAST / KAX_UNROLL / KAX_REDUCE)
//                                              -> do NOT contribute
//   - threadgroup-local     (KAX_LOCAL)        -> the `tt` block size
//   - threadgroup-collective(KAX_GROUP_REDUCE) -> the `tt` block size
// so a LOCAL-split kernel launches grid = prod(LOOP extents),
// block = prod(LOCAL extents) -- the flat one-thread-per-output shape
// in cuda_dispatch_kernel cannot express that (it has no LOCAL notion)
// and would mis-launch a LOCAL-split kernel.
//
// Returns 1 with (grid_x, block_x) on success; 0 if the DAG has no
// axes / overflows 32 bits / a block dim exceeds the V100 cap (1024
// threads/block on sm_70).
// Find the OPT(_, GROUP_REDUCE, k) wrapping a KAX_GROUP_REDUCE range
// and return k -- the cooperative block size to launch.  The range's
// own extent is the FULL reduce extent (each thread strides it by k);
// the OPT factor is the cooperative thread count.  Returns 0 if no
// GROUP_REDUCE OPT is present.
static u32 cuda_dag_group_reduce_factor(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_OPT) {
    u32 kind = (u32)term_val(heap_read(loc + 1));
    if (kind == UOP_OPT_GROUP_REDUCE) {
      return (u32)term_val(heap_read(loc + 2));
    }
    return cuda_dag_group_reduce_factor(heap_read(loc + 0));
  }
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return 0;
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    u32 f = cuda_dag_group_reduce_factor(heap_read(loc + i));
    if (f != 0) return f;
  }
  return 0;
}

fn int cuda_dag_dispatch_shape(struct KernelEntry const *ke, u32 *grid_x,
                               u32 *block_x) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  if (n == 0) return 0;
  u64 total = 1;            // product of promoted-LOOP output extents
  u64 local_total = 1;      // product of KAX_LOCAL extents
  int has_group_reduce = 0;
  for (u32 i = 0; i < n; i++) {
    if (exts[i] == 0) return 0;
    switch (types[i]) {
      case KAX_LOOP:         total       *= (u64)exts[i]; break;
      case KAX_LOCAL:        local_total *= (u64)exts[i]; break;
      case KAX_GROUP_REDUCE: has_group_reduce = 1; break;
      // KAX_UPCAST / KAX_UNROLL / KAX_REDUCE: in-thread, not in tid.
      default: break;
    }
  }
  // The GROUP_REDUCE block size is the OPT(_, GROUP_REDUCE, k) factor,
  // NOT the range extent (which is the full reduce extent each thread
  // strides over).  Look up the OPT wrap explicitly.
  u32 group_reduce_extent = has_group_reduce
      ? cuda_dag_group_reduce_factor(ke->cached_lift.store_root) : 0;
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 grid, block;
  // SIMD_REDUCE: a warp-collective reduce gives each reduce-axis tuple
  // one full warp (the __shfl_xor_sync butterfly only folds within a
  // warp), so one threadblock = one warp = 32 threads.  The grid is
  // the product of the output axes a reduce DEPENDS on
  // (rmu_dag_simd_warp_grid) -- a pure-broadcast output axis is
  // distributed across the 32 lanes by the renderer, so it must not
  // multiply the warp count.  Falls back to the full LOOP product
  // when no reduce-dependent output axis exists (a scalar-output
  // reduce -- grid 1).
  if (rmu_dag_has_simd_reduce(ke->cached_lift.store_root)
      && local_total <= 1 && group_reduce_extent == 0) {
    u64 sg = rmu_dag_simd_warp_grid(ke->cached_lift.store_root);
    if (sg == 0) sg = total;
    if (sg > 0xFFFFFFFFu) return 0;
    if (grid_x  != NULL) *grid_x  = (u32)sg;
    if (block_x != NULL) *block_x = 32;
    return 1;
  }
  if (group_reduce_extent != 0) {
    if (group_reduce_extent > 1024) return 0;   // V100 maxThreadsPerBlock
    grid  = (u32)total;
    block = group_reduce_extent;
  } else if (local_total > 1) {
    if (local_total > 1024) return 0;           // V100 maxThreadsPerBlock
    // tg/tt split: GLOBAL/LOOP extents -> grid (one block per LOOP
    // tuple, decoded from `tg`); LOCAL extents -> block (decoded from
    // `tt`).  Mirrors rmu_compute_global_decode_ctx.
    block = (u32)local_total;
    grid  = (u32)total;
  } else {
    // No LOCAL split: flat one-thread-per-output, block capped at 256
    // and kept a warp multiple (matches cuda_dispatch_kernel's flat
    // path so the two agree when no OPT axes are present).
    block = total < 256 ? (u32)total : 256u;
    if (block < 32) block = (u32)total;         // tiny kernels: exact
    grid  = (u32)((total + (u64)block - 1) / (u64)block);
  }
  if (grid == 0 || block == 0) return 0;
  if (grid_x  != NULL) *grid_x  = grid;
  if (block_x != NULL) *block_x = block;
  return 1;
}

// True iff the lifted DAG needs the DAG-derived launch geometry
// rather than the flat output_numel shape: either a per-axis OPT-class
// axis (UPCAST / UNROLL / LOCAL / GROUP_REDUCE -- from kernel_apply_opt
// DAG mode) is present, or a SIMD_REDUCE wrapper is (warp-per-row, so
// the launch is grid = output rows, block = 32).
fn int cuda_dag_has_opt_axes(struct KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  if (rmu_dag_has_simd_reduce(ke->cached_lift.store_root)) return 1;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  for (u32 i = 0; i < n; i++) {
    if (types[i] == KAX_UPCAST || types[i] == KAX_UNROLL
        || types[i] == KAX_LOCAL || types[i] == KAX_GROUP_REDUCE) {
      return 1;
    }
  }
  return 0;
}

// Backend-vtable dispatch_kernel.  Mirrors cpu_jit_dispatch's
// structural-lift path for the CUDA target:
//
//   ke->cached_lift.store_root  --(cg_render_uop_kernel_cuda_root)-->  .cu
//   cuda_jit_compile            --(nvrtc + module load)            -->  CUfunction
//   cuLaunchKernel              --(args = out, in0.., kvars)        -->  run
//
// The CUDA structural renderer flattens every promoted output LOOP
// axis onto a 1-D `tid` (blockIdx.x*blockDim.x + threadIdx.x) and
// guards `tid >= total`, so the launch grid only needs total threads
// >= output_numel.  Block size is capped at 256 and kept a multiple
// of 32 (warp granularity -- the SIMD-reduce lowering reads
// threadIdx.x % 32); grid_x = ceil(total / block_x).
//
// Returns 0 on success; -1 to make the caller fall back (no CPU
// interpreter on the CUDA device -- a -1 here is a hard failure the
// schedule surfaces, exactly as a Metal dispatch -1 does).
fn int cuda_dispatch_kernel(struct KernelEntry *ke,
                            u32 *in_buf_ids, u32 out_buf_id) {
  if (!CUDA_READY) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- backend not initialised\n");
    return -1;
  }
  if (ke == NULL || out_buf_id == 0 || out_buf_id >= CUDA_BUFS_NEXT) return -1;
  u64 t_dispatch_start = cg_now_us();
  // Only the structural-lift path is wired: the CUDA renderer entry
  // (cg_render_uop_kernel_cuda_root) consumes a lifted UOp DAG root.
  // A kernel the lifter declined (store_root == 0) has no DAG to
  // render, so bail.
  if (!cg_supports(ke) || ke->cached_lift.store_root == 0) return -1;
  Term store_root = ke->cached_lift.store_root;

  // Per-KernelEntry CUfunction cache.  cuda_dispatch_kernel previously
  // re-rendered + re-canonicalized + re-hashed the source on EVERY
  // dispatch (turning the cuda_jit_compile source-hash table into a
  // cache HIT after).  Render alone was 1-3 ms per call (open_memstream
  // + rmu_emit_store recursion + canonicalize), so a 124-kernel/step
  // warm loop burned 100-500 ms on rendering that yielded the SAME
  // CUfunction every time.  Cache (store_root, grid_x, block_x, func)
  // here so an unchanged-DAG dispatch skips all of it.
  static struct CudaKeFuncCache {
    Term     store_root;   // key; 0 = empty slot
    CUfunction func;
    u32      grid_x;
    u32      block_x;
    u32      n_kvar;
    u32      kvar_ids[KVAR_USED_CAP];
  } CUDA_KE_CACHE[1 << 14];   // KERNELS_CAP order of magnitude
  u32 cache_idx = (u32)(ke - KERNELS) & ((1 << 14) - 1);
  // THVM_CUDA_KE_CACHE=0 disables per-KE func/grid cache for A/B
  // (forces re-render+probe-source-hash every dispatch).
  static int ke_cache_init = 0, ke_cache_on = 1;
  if (!ke_cache_init) {
    char const *e = getenv("THVM_CUDA_KE_CACHE");
    ke_cache_on = (e == NULL || e[0] != '0');
    ke_cache_init = 1;
  }
  if (ke_cache_on
      && CUDA_KE_CACHE[cache_idx].store_root == store_root
      && CUDA_KE_CACHE[cache_idx].func != NULL) {
    u32 n_in = ke->n_inputs;
    u32 n_kvar = CUDA_KE_CACHE[cache_idx].n_kvar;
    u32 *kvar_ids = CUDA_KE_CACHE[cache_idx].kvar_ids;
    u32 n_args = 1 + n_in + n_kvar;
    CUdeviceptr dptrs   [n_args ? n_args : 1];
    unsigned    kvar_val[n_kvar ? n_kvar : 1];
    void       *args    [n_args ? n_args : 1];
    dptrs[0] = cuda_buf_dptr(out_buf_id);
    if (dptrs[0] == 0) return -1;
    args[0] = &dptrs[0];
    for (u32 i = 0; i < n_in; i++) {
      u32 ib = in_buf_ids[i];
      if (ib == 0 || ib >= CUDA_BUFS_NEXT) return -1;
      dptrs[1 + i] = cuda_buf_dptr(ib);
      if (dptrs[1 + i] == 0) return -1;
      args[1 + i] = &dptrs[1 + i];
    }
    for (u32 i = 0; i < n_kvar; i++) {
      kvar_val[i] = kernel_kvar_value(ke, kvar_ids[i]);
      args[1 + n_in + i] = &kvar_val[i];
    }
    {
      char const *fk = getenv("THVM_CUDA_DUMP_DISPATCH");
      if (fk != NULL && (u32)atoi(fk) == (u32)(ke - KERNELS)) {
        fprintf(stderr, "thvm: dispatch kid=%u grid=%u block=%u\n",
                (u32)(ke - KERNELS),
                CUDA_KE_CACHE[cache_idx].grid_x,
                CUDA_KE_CACHE[cache_idx].block_x);
        fprintf(stderr, "  out buf_id=%u dptr=%p nbytes=%llu parent=%u root=%u\n",
                out_buf_id, (void*)dptrs[0],
                (unsigned long long)CUDA_BUFS[out_buf_id].nbytes,
                CUDA_BUFS[out_buf_id].parent_buf_id,
                cuda_buf_storage_root(out_buf_id));
        for (u32 i = 0; i < n_in; i++) {
          u32 ib = in_buf_ids[i];
          fprintf(stderr, "  in[%u] buf_id=%u dptr=%p nbytes=%llu refcount=%u pinned=%u parent=%u root=%u\n",
                  i, ib, (void*)dptrs[1+i],
                  (unsigned long long)CUDA_BUFS[ib].nbytes,
                  CUDA_BUFS[ib].refcount, CUDA_BUFS[ib].jit_pinned,
                  CUDA_BUFS[ib].parent_buf_id,
                  cuda_buf_storage_root(ib));
        }
      }
    }
    // Optional per-kid GPU time via CUDA events.  Costs a sync per
    // launch (defeats async pipelining), so only on when the env knob
    // is set.  Use to identify true GPU-time hotspots, distinct from
    // the CPU dispatch-wall the cg_profile_record below captures
    // (which can be dominated by launch-queue full-waits, NOT compute).
    static int   gpu_time_init = 0, gpu_time_on = 0;
    static CUevent gpu_ev_a = NULL, gpu_ev_b = NULL;
    if (!gpu_time_init) {
      char const *e = getenv("THVM_CUDA_GPU_TIME");
      gpu_time_on = (e != NULL && e[0] != '0');
      if (gpu_time_on) {
        cuEventCreate(&gpu_ev_a, 0);
        cuEventCreate(&gpu_ev_b, 0);
      }
      gpu_time_init = 1;
    }
    if (gpu_time_on) cuEventRecord(gpu_ev_a, NULL);
    int rc = cuda_jit_launch(CUDA_KE_CACHE[cache_idx].func,
                             CUDA_KE_CACHE[cache_idx].grid_x,
                             CUDA_KE_CACHE[cache_idx].block_x, args);
    if (rc != 0 && getenv("THVM_CUDA_LAUNCH_TRACE")) {
      fprintf(stderr, "thvm: dispatch FAIL kid=%u (cache hit)\n", (u32)(ke - KERNELS));
    }
    u32 kid = (u32)(ke - KERNELS);
    if (gpu_time_on && rc == 0) {
      cuEventRecord(gpu_ev_b, NULL);
      cuEventSynchronize(gpu_ev_b);
      float ms = 0.0f;
      cuEventElapsedTime(&ms, gpu_ev_a, gpu_ev_b);
      cg_profile_record_gpu(kid, (u64)(ms * 1000.0f));
    }
    cg_profile_record(kid, KDISPATCH_CUDA_JIT, cg_now_us() - t_dispatch_start);
    return rc;
  }

  // Render the lifted DAG to a .cu string, then nvrtc-compile it.
  // cg_render_uop_kernel_cuda_root is called directly (rather than via
  // thvm_cuda_render in _.c) because jit.c is #included before _.c --
  // a forward call would hit an implicit declaration.
  // THVM_CUDA_RENDER_TRACE=1: localize warmup hangs to render vs
  // canonicalize vs nvrtc.  Prints kid + stage with flush so the LAST
  // line names the stuck stage.
  static int rt_known = 0, rt_on = 0;
  if (!rt_known) { char const *e = getenv("THVM_CUDA_RENDER_TRACE");
                   rt_on = (e != NULL && e[0] == '1'); rt_known = 1; }
  u32 rt_kid = (u32)(ke - KERNELS);
  char  *cu  = NULL;
  size_t csz = 0;
  FILE  *cfp = open_memstream(&cu, &csz);
  if (cfp == NULL) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- open_memstream failed\n");
    return -1;
  }
  if (rt_on) { fprintf(stderr, "[render] kid=%u start\n", rt_kid); fflush(stderr); }
  cg_render_uop_kernel_cuda_root(store_root, "k", cfp);
  fclose(cfp);
  if (cu == NULL) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- render produced no source\n");
    return -1;
  }
  // Axis ids are already dense 0..n per kernel (uop_dag_renumber_axes
  // runs in materialize), so render emits byte-identical source for
  // structurally-identical kernels directly -- the JIT/disk cache hits
  // without a post-render canonicalize pass.
  if (rt_on) { fprintf(stderr, "[render] kid=%u done sz=%zu, compiling\n", rt_kid, csz); fflush(stderr); }
  // THVM_CUDA_DUMP_KID=<kid>: print this kid's rendered .cu source once
  // (mirrors the CPU THVM_DUMP_KERNEL_SRC env).  Used to inspect
  // hand-coded LOCAL/UPCAST application on a specific hotspot kernel.
  {
    static u32 dump_kid = 0;
    static int dump_kid_known = 0;
    static u32 dumped_once = 0;
    if (!dump_kid_known) {
      char const *e = getenv("THVM_CUDA_DUMP_KID");
      dump_kid = (e != NULL) ? (u32)atoi(e) : 0;
      dump_kid_known = 1;
    }
    u32 this_kid = (u32)(ke - KERNELS);
    if (dump_kid != 0 && dump_kid == this_kid && dumped_once != this_kid) {
      dumped_once = this_kid;
      fprintf(stderr, "=== CUDA kernel src kid=%u ===\n%s\n=== end kid=%u ===\n",
              this_kid, cu, this_kid);
      fflush(stderr);
    }
    if (getenv("THVM_CUDA_DUMP_SHARED") && strstr(cu, "__shared__")) {
      fprintf(stderr, "=== CUDA kernel kid=%u (has __shared__) ===\n%s\n=== end ===\n",
              this_kid, cu);
      fflush(stderr);
    }
  }
  CUfunction func = cuda_jit_compile(cu, "k");
  if (func == NULL) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- compile failed: %s\n",
            thvm_cuda_last_error());
    if (getenv("THVM_CUDA_DUMP_FAILED_KERNEL")) {
      fprintf(stderr, "=== FAILED kernel src kid=%u ===\n%s=== end ===\n",
              (u32)(ke - KERNELS), cu);
    }
    free(cu);
    return -1;
  }
  free(cu);

  // Pack the cuLaunchKernel argument array.  Order matches the CUDA
  // kernel signature emitted by cg_render_uop_kernel_cuda_root:
  //   k(T *out, const T *in0, ..., unsigned V_kvar0, ...)
  // Each `args[i]` points at the value to pass for parameter i:
  // a CUdeviceptr for the buffer pointers, an unsigned for the kvars.
  u32 n_in = ke->n_inputs;
  u32 kvar_ids[KVAR_USED_CAP];
  u32 n_kvar = kvar_collect_from_dag(store_root, kvar_ids, KVAR_USED_CAP);
  u32 n_args = 1 + n_in + n_kvar;
  CUdeviceptr dptrs   [n_args ? n_args : 1];
  unsigned    kvar_val[n_kvar ? n_kvar : 1];
  void       *args    [n_args ? n_args : 1];

  dptrs[0] = cuda_buf_dptr(out_buf_id);
  if (dptrs[0] == 0) return -1;
  args[0] = &dptrs[0];
  for (u32 i = 0; i < n_in; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= CUDA_BUFS_NEXT) return -1;
    dptrs[1 + i] = cuda_buf_dptr(ib);
    if (dptrs[1 + i] == 0) return -1;
    args[1 + i] = &dptrs[1 + i];
  }
  for (u32 i = 0; i < n_kvar; i++) {
    kvar_val[i] = kernel_kvar_value(ke, kvar_ids[i]);
    args[1 + n_in + i] = &kvar_val[i];
  }

  // Launch geometry.  When the lifted DAG carries per-axis OPT axes
  // (a LOCAL split from propose.c / the autotune sweep), the flat
  // output_numel shape cannot express the tg/tt block geometry the
  // renderer decodes -- derive (grid, block) from the DAG's axis
  // types instead (cuda_dag_dispatch_shape).  Otherwise fall back to
  // the flat one-thread-per-output shape.
  u32 grid_x = 0, block_x = 0;
  if (!(cuda_dag_has_opt_axes(ke)
        && cuda_dag_dispatch_shape(ke, &grid_x, &block_x))) {
    // Flat: total threads = output_numel (one promoted output element
    // per thread; the renderer's `tid >= total` guard makes a
    // slightly-over-sized grid safe).  A scalar-output kernel
    // (output_numel <= 1, e.g. a full reduce) launches a single thread.
    u64 total = ke->output_numel;
    if (total == 0) total = 1;
    block_x = 256;
    if ((u64)block_x > total) {
      // Round the block down to the nearest warp multiple that still
      // covers `total`, never below 32 (warp granularity).
      block_x = (u32)((total + 31) / 32 * 32);
      if (block_x < 32) block_x = 32;
    }
    grid_x = (u32)((total + block_x - 1) / block_x);
  }

  int rc = cuda_jit_launch(func, grid_x, block_x, args);
  if (rc != 0 && getenv("THVM_CUDA_LAUNCH_TRACE")) {
    fprintf(stderr, "thvm: dispatch FAIL kid=%u (cache miss)\n", (u32)(ke - KERNELS));
  }
  // Populate the per-KernelEntry cache for the next dispatch of this
  // store_root.  If a different KernelEntry collides on cache_idx its
  // slot just gets overwritten -- correctness is preserved (we always
  // re-check store_root == key before reuse); only the cache hit rate
  // suffers under collisions.
  CUDA_KE_CACHE[cache_idx].store_root = store_root;
  CUDA_KE_CACHE[cache_idx].func       = func;
  CUDA_KE_CACHE[cache_idx].grid_x     = grid_x;
  CUDA_KE_CACHE[cache_idx].block_x    = block_x;
  CUDA_KE_CACHE[cache_idx].n_kvar     = n_kvar;
  for (u32 i = 0; i < n_kvar && i < KVAR_USED_CAP; i++) {
    CUDA_KE_CACHE[cache_idx].kvar_ids[i] = kvar_ids[i];
  }
  // Per-kid wall-time profile.  cuda_dispatch_kernel includes
  // render + compile (cache-hit fast path) + arg packing +
  // cuLaunchKernel + cuCtxSynchronize.  CUDA only has the
  // structural-lift JIT route (no interpreter fallback); mirror of
  // cpu_jit's record in backend/cpu/interpret.c so THVM_KERNEL_PROFILE
  // shows both backends through the same table.
  u32 kid = (u32)(ke - KERNELS);
  cg_profile_record(kid, KDISPATCH_CUDA_JIT, cg_now_us() - t_dispatch_start);
  return rc;
}
