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

#define CUDA_JIT_CACHE_CAP 128

typedef struct {
  u64        key;        // 0 = empty slot; FNV hash of the .cu source
  CUmodule   module;
  CUfunction func;
} CudaJitSlot;
static CudaJitSlot CUDA_JIT_CACHE[CUDA_JIT_CACHE_CAP];

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

static CudaJitSlot *cuda_jit_lookup_slot(u64 key) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < CUDA_JIT_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (CUDA_JIT_CACHE_CAP - 1);
    if (CUDA_JIT_CACHE[i].key == key) return &CUDA_JIT_CACHE[i];
    if (CUDA_JIT_CACHE[i].key == 0)   return &CUDA_JIT_CACHE[i];
  }
  return NULL;
}

// Compile a .cu source string with nvrtc and load it as a module.
// Returns a resolved CUfunction for `kernel_name`, or NULL on any
// nvrtc / driver failure (the reason lands in thvm_cuda_last_error).
fn CUfunction cuda_jit_compile(const char *cu_src, const char *kernel_name) {
  if (!CUDA_READY) {
    snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR,
             "cuda_jit_compile: backend not initialised");
    return NULL;
  }
  u64 key = cuda_jit_hash(cu_src);
  CudaJitSlot *slot = cuda_jit_lookup_slot(key);
  if (slot != NULL && slot->key == key && slot->func != NULL) {
    return slot->func;   // cache hit -- skip nvrtc + module load
  }

  // --- nvrtc compile ------------------------------------------------
  nvrtcProgram prog;
  nvrtcResult nr = nvrtcCreateProgram(&prog, cu_src, kernel_name,
                                      0, NULL, NULL);
  if (nr != NVRTC_SUCCESS) {
    snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR,
             "nvrtcCreateProgram: %s", nvrtcGetErrorString(nr));
    return NULL;
  }
  // --gpu-architecture: track the live device.  V100 -> compute_70.
  char arch_opt[32];
  int sm = cuda_device_sm();
  if (sm <= 0) sm = 70;   // safe default if the probe failed
  snprintf(arch_opt, sizeof arch_opt, "--gpu-architecture=compute_%d", sm);
  const char *opts[] = { arch_opt };
  nr = nvrtcCompileProgram(prog, 1, opts);
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
  char *ptx = (char *)malloc(ptx_sz);
  if (ptx == NULL) {
    nvrtcDestroyProgram(&prog);
    snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR, "PTX alloc failed");
    return NULL;
  }
  nvrtcGetPTX(prog, ptx);
  nvrtcDestroyProgram(&prog);

  // --- module load + function lookup -------------------------------
  CUmodule module;
  CUresult cr = cuModuleLoadData(&module, ptx);
  free(ptx);
  if (cr != CUDA_SUCCESS) {
    cuda_set_error("cuModuleLoadData", cr);
    return NULL;
  }
  CUfunction func;
  cr = cuModuleGetFunction(&func, module, kernel_name);
  if (cr != CUDA_SUCCESS) {
    cuda_set_error("cuModuleGetFunction", cr);
    cuModuleUnload(module);
    return NULL;
  }
  if (slot != NULL) {
    slot->key    = key;
    slot->module = module;
    slot->func   = func;
  } else {
    // Cache full -- the kernel still works, it just won't be cached.
    // cuModuleUnload would invalidate `func`, so leak the module to
    // end-of-session (cuCtxDestroy reclaims it).
  }
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
                              NULL,              // stream (default)
                              args, NULL);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuLaunchKernel", r);
    return -1;
  }
  r = cuCtxSynchronize();
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuCtxSynchronize", r);
    return -1;
  }
  return 0;
}

// Backend-vtable dispatch_kernel.  Stage 2 lands the runtime
// primitives (init / buffers / nvrtc compile / launch) and the
// standalone thvm_cuda_* entry points the e2e test drives; wiring
// CUDA into the schedule's KernelEntry dispatch path is Stage 3
// (THVM_BACKEND=cuda + the py bridge).  Until then this returns the
// loud error sentinel, exactly as the Metal stub did before its .m
// glue landed.
fn int cuda_dispatch_kernel(struct KernelEntry *ke,
                            u32 *in_buf_ids, u32 out_buf_id) {
  (void)ke; (void)in_buf_ids; (void)out_buf_id;
  fprintf(stderr,
    "thvm: cuda backend -- dispatch_kernel not wired (Stage 3); use the "
    "thvm_cuda_* render+launch entry points directly\n");
  return -1;
}
