// backend/cuda/init.c - lifecycle for the CUDA backend + its buffer
// table.
//
// Mirrors backend/cpu/init.c.  The CUDA backend talks to the GPU via
// the CUDA *driver* API (cuda.h, libcuda.so) -- not the runtime API --
// because the driver API is the layer nvrtc-compiled modules load
// against (cuModuleLoadData / cuLaunchKernel) and it links without a
// CUDA toolkit install.  The whole file is plain C99: cuda.h and
// nvrtc.h are C headers, so unlike the Objective-C Metal backend the
// CUDA backend is #included straight into the single-TU src/thvm.c
// build (guarded by THVM_HAS_CUDA).
//
// CUDA_BUFS[] is a parallel table to TENS[], exactly like CPU_BUFS[]:
// each entry pairs a CUdeviceptr (device memory) with a host-side
// refcount so view aliases share storage.  Most device buffers are
// "owned" (cuMemAlloc'd here), but the per-realize arena planner now
// also allocates non-owning "view" entries that point into a parent
// arena's dptr at an offset (see cuda_buf_alloc_arena_view).  Mirror:
// backend/cpu/init.c + the parent_buf_id field on CpuBuf.

// cuda.h uses the bare identifier `fn` (a CUhostFn parameter +
// CUDA_HOST_NODE_PARAMS member name), which collides with thvm.h's
// `#define fn static inline`.  Drop the macro across the CUDA headers
// and restore it afterwards so the rest of the backend keeps using
// `fn` for its helpers.
#undef fn
#include <cuda.h>
#include <nvrtc.h>
#define fn static inline

typedef struct {
  CUdeviceptr dptr;          // device allocation or view base; 0 = free slot
  u64         nbytes;
  u32         refcount;
  u8          preserved;     // per-realize pool keep-flag (see buf_pool.c)
  u8          owns_data;     // 1: cuMemFree on release; 0: view, do not free
  u8          skip_freelist; // 1: pool_rollback real-frees instead of parking
  u8          jit_pinned;    // STICKY: held by an active JIT capture (replay
                             // re-fires kernels against this buf_id).
                             // pool_rollback skips; clear_preserved leaves it.
                             // Cleared only by jit_capture_drop on the slot.
  u32         parent_buf_id; // arena views: parent CudaBuf to ref/unref
} CudaBuf;

#define CUDA_BUFS_CAP       (1u << 16)
#define CUDA_FREELIST_CAP   4096

static CudaBuf CUDA_BUFS[CUDA_BUFS_CAP];
static u64     CUDA_BUFS_NEXT     = 1;   // 0 reserved for "no buffer"
static u32     CUDA_FREELIST[CUDA_FREELIST_CAP];
static u32     CUDA_FREELIST_LEN  = 0;

// Driver-API handles, set up by cuda_init.  One device + one primary
// context is all Stage 2 needs; multi-GPU is a later concern.
static int        CUDA_READY   = 0;
static CUdevice   CUDA_DEVICE  = 0;
static CUcontext  CUDA_CONTEXT = NULL;

// Dedicated stream used for cuGraph capture.  jit_replay flips
// CUDA_CUR_STREAM to this stream for the duration of the first
// capture pass; subsequent replays cuGraphLaunch on this stream too.
// All cuLaunchKernel / cuMemcpyAsync calls in the dispatch path
// honor CUDA_CUR_STREAM (NULL = default sync stream).
static CUstream   CUDA_CAPTURE_STREAM = NULL;
static CUstream   CUDA_CUR_STREAM     = NULL;   // 0/NULL = default

// Per-JIT-slot cached CUgraph + CUgraphExec.  Keyed by jit slot id.
// JIT_CAPTURE_NSLOTS lives in jit/capture.c; mirror the constant
// here (cuda backend is included before capture.c, so a sym ref
// would be a forward dep).  Bump together if capture.c bumps.
#define CUDA_JIT_GRAPH_CACHE_NSLOTS 16
typedef struct {
  CUgraph     graph;
  CUgraphExec exec;
  int         valid;
} CudaJitGraphCacheEntry;
static CudaJitGraphCacheEntry CUDA_JIT_GRAPH_CACHE[CUDA_JIT_GRAPH_CACHE_NSLOTS];

// jit.c (included after this file) owns the nvrtc module cache;
// forward-declared so cuda_shutdown can unload every cached module
// before cuCtxDestroy.
fn void cuda_jit_cache_reset(void);

// Decode a CUDA driver error to its short name for diagnostics.
static const char *cuda_err_str(CUresult r) {
  const char *s = NULL;
  cuGetErrorName(r, &s);
  return s ? s : "CUDA_ERROR_UNKNOWN";
}

// thvm_cuda_last_error: most recent driver/nvrtc failure string, for
// the test harness + Stage 3 bridge to surface a reason on a NULL
// return.  Static buffer; overwritten on each failure.
static char CUDA_LAST_ERROR[256] = {0};
fn const char *thvm_cuda_last_error(void) { return CUDA_LAST_ERROR; }
static void cuda_set_error(const char *where, CUresult r) {
  snprintf(CUDA_LAST_ERROR, sizeof CUDA_LAST_ERROR,
           "%s: %s", where, cuda_err_str(r));
}

fn int cuda_init(void) {
  if (CUDA_READY) return 0;
  CUresult r = cuInit(0);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuInit", r);
    fprintf(stderr, "thvm: cuda_init -- %s\n", CUDA_LAST_ERROR);
    return -1;
  }
  int n_dev = 0;
  r = cuDeviceGetCount(&n_dev);
  if (r != CUDA_SUCCESS || n_dev < 1) {
    cuda_set_error("cuDeviceGetCount", r);
    fprintf(stderr, "thvm: cuda_init -- no CUDA device available\n");
    return -1;
  }
  r = cuDeviceGet(&CUDA_DEVICE, 0);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuDeviceGet", r);
    fprintf(stderr, "thvm: cuda_init -- %s\n", CUDA_LAST_ERROR);
    return -1;
  }
  r = cuCtxCreate(&CUDA_CONTEXT, 0, CUDA_DEVICE);
  if (r != CUDA_SUCCESS) {
    cuda_set_error("cuCtxCreate", r);
    fprintf(stderr, "thvm: cuda_init -- %s\n", CUDA_LAST_ERROR);
    return -1;
  }
  char name[128] = {0};
  cuDeviceGetName(name, sizeof name, CUDA_DEVICE);
  int cc_major = 0, cc_minor = 0;
  cuDeviceGetAttribute(&cc_major,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, CUDA_DEVICE);
  cuDeviceGetAttribute(&cc_minor,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, CUDA_DEVICE);
  fprintf(stderr, "thvm: cuda_init -- device: %s (sm_%d%d)\n",
          name, cc_major, cc_minor);
  CUDA_READY = 1;
  return 0;
}

// Compute capability of the active device as a 2-digit int (V100 ->
// 70).  jit.c reads this to build the nvrtc --gpu-architecture flag so
// the target tracks the actual GPU rather than a hardcoded sm_70.
fn int cuda_device_sm(void) {
  int cc_major = 0, cc_minor = 0;
  if (!CUDA_READY) return 0;
  cuDeviceGetAttribute(&cc_major,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, CUDA_DEVICE);
  cuDeviceGetAttribute(&cc_minor,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, CUDA_DEVICE);
  return cc_major * 10 + cc_minor;
}

// Streaming-multiprocessor (SM) count of the active device (V100 -> 80,
// A100 -> 108, H100 -> 132/144).  hand_opts.c derives the reduce-heavy
// occupancy floor from this so the UPCAST cap auto-tunes per GPU rather
// than needing a hand-set THVM_UPCAST_REDUCE_MIN_GRID.  Returns 0 when
// CUDA is not ready (callers treat 0 as "no cap").
fn int cuda_device_sm_count(void) {
  int count = 0;
  if (!CUDA_READY) return 0;
  cuDeviceGetAttribute(&count,
      CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, CUDA_DEVICE);
  return count > 0 ? count : 0;
}

fn void cuda_shutdown(void) {
  if (!CUDA_READY) return;
  // Views (owns_data == 0) borrow another slot's dptr; only the owner
  // calls cuMemFree.  Scanning ownership-aware here avoids a double-free
  // at shutdown when arena views still occupy slots above the parent.
  for (u64 i = 1; i < CUDA_BUFS_NEXT; i++) {
    if (CUDA_BUFS[i].dptr != 0 && CUDA_BUFS[i].owns_data) {
      cuMemFree(CUDA_BUFS[i].dptr);
    }
    CUDA_BUFS[i].dptr          = 0;
    CUDA_BUFS[i].parent_buf_id = 0;
    CUDA_BUFS[i].owns_data     = 0;
    CUDA_BUFS[i].skip_freelist = 0;
  }
  // jit.c owns the module cache; it registers cuda_jit_cache_reset
  // which cuModuleUnload's every cached module before we drop the
  // context.
  cuda_jit_cache_reset();
  if (CUDA_CONTEXT != NULL) {
    cuCtxDestroy(CUDA_CONTEXT);
    CUDA_CONTEXT = NULL;
  }
  CUDA_BUFS_NEXT    = 1;
  CUDA_FREELIST_LEN = 0;
  CUDA_READY        = 0;
}
