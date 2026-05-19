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
  // Only the structural-lift path is wired: the CUDA renderer entry
  // (cg_render_uop_kernel_cuda_root) consumes a lifted UOp DAG root.
  // A kernel the lifter declined (store_root == 0) has no DAG to
  // render -- there is no CUDA equivalent of the legacy KProgOp loop,
  // so bail.
  if (!cg_supports(ke) || ke->cached_lift.store_root == 0) return -1;
  Term store_root = ke->cached_lift.store_root;

  // Render the lifted DAG to a .cu string, then nvrtc-compile it.
  // cg_render_uop_kernel_cuda_root is called directly (rather than via
  // thvm_cuda_render in _.c) because jit.c is #included before _.c --
  // a forward call would hit an implicit declaration.
  char  *cu  = NULL;
  size_t csz = 0;
  FILE  *cfp = open_memstream(&cu, &csz);
  if (cfp == NULL) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- open_memstream failed\n");
    return -1;
  }
  cg_render_uop_kernel_cuda_root(store_root, "k", cfp);
  fclose(cfp);
  if (cu == NULL) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- render produced no source\n");
    return -1;
  }
  CUfunction func = cuda_jit_compile(cu, "k");
  free(cu);
  if (func == NULL) {
    fprintf(stderr, "thvm: cuda_dispatch_kernel -- compile failed: %s\n",
            thvm_cuda_last_error());
    return -1;
  }

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

  // Launch geometry: total threads = output_numel (one promoted
  // output element per thread; the renderer's `tid >= total` guard
  // makes a slightly-over-sized grid safe).  A scalar-output kernel
  // (output_numel <= 1, e.g. a full reduce) launches a single thread.
  u64 total = ke->output_numel;
  if (total == 0) total = 1;
  u32 block_x = 256;
  if ((u64)block_x > total) {
    // Round the block down to the nearest warp multiple that still
    // covers `total`, never below 32 (warp granularity).
    block_x = (u32)((total + 31) / 32 * 32);
    if (block_x < 32) block_x = 32;
  }
  u32 grid_x = (u32)((total + block_x - 1) / block_x);

  return cuda_jit_launch(func, grid_x, block_x, args);
}
