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
fn int cuda_dag_dispatch_shape(struct KernelEntry const *ke, u32 *grid_x,
                               u32 *block_x) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  if (n == 0) return 0;
  u64 total = 1;            // product of promoted-LOOP output extents
  u64 local_total = 1;      // product of KAX_LOCAL extents
  u32 group_reduce_extent = 0;
  for (u32 i = 0; i < n; i++) {
    if (exts[i] == 0) return 0;
    switch (types[i]) {
      case KAX_LOOP:         total       *= (u64)exts[i]; break;
      case KAX_LOCAL:        local_total *= (u64)exts[i]; break;
      case KAX_GROUP_REDUCE: group_reduce_extent = exts[i]; break;
      // KAX_UPCAST / KAX_UNROLL / KAX_REDUCE: in-thread, not in tid.
      default: break;
    }
  }
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
  // Only the structural-lift path is wired: the CUDA renderer entry
  // (cg_render_uop_kernel_cuda_root) consumes a lifted UOp DAG root.
  // A kernel the lifter declined (store_root == 0) has no DAG to
  // render, so bail.
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

  return cuda_jit_launch(func, grid_x, block_x, args);
}
