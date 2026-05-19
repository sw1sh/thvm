// thvm_py_cuda.c -- in-process CUDA compile + dispatch + timing for the
// Python ctypes wrapper.  The CUDA-side analogue of thvm_py_metal.m.
//
// Like the Metal bridge, this file is intentionally independent of
// thvm's own CUDA backend (src/backend/cuda/): it holds its own CUDA
// driver context + small handle tables for compiled functions and
// device buffers, so the Python package can drive a GPU without
// pulling the whole single-TU runtime into a second copy.
//
// The CUDA driver API (cuda.h, libcuda) is what nvrtc-compiled modules
// load against; nvrtc.h compiles the rendered .cu string to PTX.  Both
// are plain C, so -- unlike the Objective-C Metal bridge -- this is a
// .c, not a .m.
//
// All entry points are extern "C" via EXPORT and prefixed py_cuda_*.

#include <cuda.h>
#include <nvrtc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EXPORT __attribute__((visibility("default")))

// ---------------- runtime state ----------------
static int       g_ready = 0;
static CUdevice  g_dev = 0;
static CUcontext g_ctx = NULL;
static int       g_sm = 0;          // compute capability as 2-digit int

// Handle tables.  Index 0 reserved as "invalid".  An agent typically
// builds a few kernels and many buffers per run.
#define FN_CAP  256
#define BUF_CAP 1024

typedef struct {
  CUmodule   module;
  CUfunction func;
} FnSlot;

typedef struct {
  CUdeviceptr dptr;
  uint64_t    nbytes;
} BufSlot;

static FnSlot   g_fns [FN_CAP];
static BufSlot  g_bufs[BUF_CAP];
static uint32_t g_n_fns  = 1;   // 0 reserved
static uint32_t g_n_bufs = 1;

// Most recent driver / nvrtc failure string, surfaced to the Python
// layer on a 0/NULL return.  Static buffer; overwritten on each error.
static char g_err[512] = {0};
static void set_drv_err(const char *where, CUresult r) {
  const char *s = NULL;
  cuGetErrorName(r, &s);
  snprintf(g_err, sizeof g_err, "%s: %s", where, s ? s : "CUDA_ERROR_UNKNOWN");
}

static uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

EXPORT const char *py_cuda_last_error(void) { return g_err; }

// ---------------- lifecycle ----------------
EXPORT int py_cuda_init(void) {
  if (g_ready) return 1;                 // already initialized
  CUresult r = cuInit(0);
  if (r != CUDA_SUCCESS) { set_drv_err("cuInit", r); return 0; }
  int n_dev = 0;
  r = cuDeviceGetCount(&n_dev);
  if (r != CUDA_SUCCESS || n_dev < 1) {
    snprintf(g_err, sizeof g_err, "no CUDA device available");
    return 0;
  }
  r = cuDeviceGet(&g_dev, 0);
  if (r != CUDA_SUCCESS) { set_drv_err("cuDeviceGet", r); return 0; }
  r = cuCtxCreate(&g_ctx, 0, g_dev);
  if (r != CUDA_SUCCESS) { set_drv_err("cuCtxCreate", r); return 0; }
  int cc_major = 0, cc_minor = 0;
  cuDeviceGetAttribute(&cc_major,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, g_dev);
  cuDeviceGetAttribute(&cc_minor,
      CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, g_dev);
  g_sm = cc_major * 10 + cc_minor;
  g_ready = 1;
  return 1;
}

EXPORT void py_cuda_shutdown(void) {
  if (!g_ready) return;
  for (uint32_t i = 1; i < g_n_bufs; i++) {
    if (g_bufs[i].dptr != 0) { cuMemFree(g_bufs[i].dptr); g_bufs[i].dptr = 0; }
  }
  for (uint32_t i = 1; i < g_n_fns; i++) {
    if (g_fns[i].module != NULL) { cuModuleUnload(g_fns[i].module); g_fns[i].module = NULL; }
    g_fns[i].func = NULL;
  }
  g_n_bufs = 1;
  g_n_fns  = 1;
  if (g_ctx != NULL) { cuCtxDestroy(g_ctx); g_ctx = NULL; }
  g_ready = 0;
}

// Compute capability of the active device as a 2-digit int (V100 -> 70).
EXPORT uint32_t py_cuda_device_sm(void) { return (uint32_t)g_sm; }

// ---------------- compile .cu string -> CUfunction handle ----------------
// nvrtc-compiles `src` to PTX with --gpu-architecture tracking the live
// device, loads the PTX as a module, resolves `fn_name`.  Returns a
// 1-based function handle; 0 on failure (reason in err_buf if given).
EXPORT uint32_t py_cuda_compile(const char *src, const char *fn_name,
                                char *err_buf, uint32_t err_buf_size) {
  if (!g_ready) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "cuda_not_initialized");
    return 0;
  }
  if (g_n_fns >= FN_CAP) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "fn_table_full");
    return 0;
  }
  nvrtcProgram prog;
  nvrtcResult nr = nvrtcCreateProgram(&prog, src, fn_name, 0, NULL, NULL);
  if (nr != NVRTC_SUCCESS) {
    if (err_buf && err_buf_size)
      snprintf(err_buf, err_buf_size, "nvrtcCreateProgram: %s",
               nvrtcGetErrorString(nr));
    return 0;
  }
  // --gpu-architecture tracks the device (V100 -> compute_70).
  char arch_opt[32];
  int sm = g_sm > 0 ? g_sm : 70;
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
      if (err_buf && err_buf_size)
        snprintf(err_buf, err_buf_size, "compile: %s -- %s",
                 nvrtcGetErrorString(nr), log);
      free(log);
    }
    nvrtcDestroyProgram(&prog);
    return 0;
  }
  size_t ptx_sz = 0;
  nvrtcGetPTXSize(prog, &ptx_sz);
  char *ptx = (char *)malloc(ptx_sz);
  if (ptx == NULL) {
    nvrtcDestroyProgram(&prog);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "ptx_alloc_failed");
    return 0;
  }
  nvrtcGetPTX(prog, ptx);
  nvrtcDestroyProgram(&prog);

  CUmodule module;
  CUresult cr = cuModuleLoadData(&module, ptx);
  free(ptx);
  if (cr != CUDA_SUCCESS) {
    set_drv_err("cuModuleLoadData", cr);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    return 0;
  }
  CUfunction func;
  cr = cuModuleGetFunction(&func, module, fn_name);
  if (cr != CUDA_SUCCESS) {
    set_drv_err("cuModuleGetFunction", cr);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    cuModuleUnload(module);
    return 0;
  }
  uint32_t handle = g_n_fns++;
  g_fns[handle].module = module;
  g_fns[handle].func   = func;
  return handle;
}

EXPORT void py_cuda_fn_release(uint32_t handle) {
  if (handle == 0 || handle >= g_n_fns) return;
  if (g_fns[handle].module != NULL) {
    cuModuleUnload(g_fns[handle].module);
    g_fns[handle].module = NULL;
  }
  g_fns[handle].func = NULL;
}

// ---------------- buffers ----------------
EXPORT uint32_t py_cuda_buf_alloc(uint64_t nbytes) {
  if (!g_ready || g_n_bufs >= BUF_CAP) return 0;
  if (nbytes == 0) nbytes = 1;           // cuMemAlloc rejects 0
  CUdeviceptr dptr = 0;
  CUresult r = cuMemAlloc(&dptr, (size_t)nbytes);
  if (r != CUDA_SUCCESS) { set_drv_err("cuMemAlloc", r); return 0; }
  cuMemsetD8(dptr, 0, (size_t)nbytes);
  uint32_t handle = g_n_bufs++;
  g_bufs[handle].dptr   = dptr;
  g_bufs[handle].nbytes = nbytes;
  return handle;
}

EXPORT int py_cuda_buf_write(uint32_t handle, const void *src, uint64_t nbytes) {
  if (handle == 0 || handle >= g_n_bufs) return 0;
  if (nbytes > g_bufs[handle].nbytes) return 0;
  CUresult r = cuMemcpyHtoD(g_bufs[handle].dptr, src, (size_t)nbytes);
  if (r != CUDA_SUCCESS) { set_drv_err("cuMemcpyHtoD", r); return 0; }
  return 1;
}

EXPORT int py_cuda_buf_read(uint32_t handle, void *dst, uint64_t nbytes) {
  if (handle == 0 || handle >= g_n_bufs) return 0;
  if (nbytes > g_bufs[handle].nbytes) return 0;
  CUresult r = cuMemcpyDtoH(dst, g_bufs[handle].dptr, (size_t)nbytes);
  if (r != CUDA_SUCCESS) { set_drv_err("cuMemcpyDtoH", r); return 0; }
  return 1;
}

EXPORT void py_cuda_buf_release(uint32_t handle) {
  if (handle == 0 || handle >= g_n_bufs) return;
  if (g_bufs[handle].dptr != 0) {
    cuMemFree(g_bufs[handle].dptr);
    g_bufs[handle].dptr = 0;
  }
}

// ---------------- dispatch ----------------
// Build the cuLaunchKernel extra-params array from the buffer handles
// (one CUdeviceptr pointer per kernel arg, in order) and launch a 1-D
// grid.  Returns wall ns; 0 on dispatch error (reason in err_buf).
EXPORT uint64_t py_cuda_dispatch(uint32_t fn_handle,
                                 const uint32_t *buf_handles, uint32_t n_bufs,
                                 uint32_t grid_x, uint32_t block_x,
                                 char *err_buf, uint32_t err_buf_size) {
  if (fn_handle == 0 || fn_handle >= g_n_fns) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "bad_fn_handle");
    return 0;
  }
  CUdeviceptr dptrs[BUF_CAP];
  void       *args [BUF_CAP];
  if (n_bufs > BUF_CAP) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "too_many_bufs");
    return 0;
  }
  for (uint32_t i = 0; i < n_bufs; i++) {
    uint32_t h = buf_handles[i];
    if (h == 0 || h >= g_n_bufs) {
      if (err_buf && err_buf_size)
        snprintf(err_buf, err_buf_size, "bad_buf_handle[%u]=%u", i, h);
      return 0;
    }
    dptrs[i] = g_bufs[h].dptr;
    args [i] = &dptrs[i];
  }
  uint64_t t0 = now_ns();
  CUresult r = cuLaunchKernel(g_fns[fn_handle].func,
                              grid_x, 1, 1, block_x, 1, 1,
                              0, NULL, args, NULL);
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuLaunchKernel", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    return 0;
  }
  r = cuCtxSynchronize();
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuCtxSynchronize", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    return 0;
  }
  return now_ns() - t0;
}

// Variant that returns BOTH a wall ns and a true GPU ns.  The GPU time
// comes from CUDA events bracketing the launch on the default stream
// (cuEventRecord before / after, cuEventElapsedTime).  CUDA events
// measure GPU execution directly -- the honest analogue of the Metal
// GPUEndTime/GPUStartTime fix in py_metal_dispatch_timed: it excludes
// the CPU-side launch + synchronize wall overhead.
EXPORT int py_cuda_dispatch_timed(uint32_t fn_handle,
                                  const uint32_t *buf_handles, uint32_t n_bufs,
                                  uint32_t grid_x, uint32_t block_x,
                                  uint64_t *wall_ns_out, uint64_t *gpu_ns_out,
                                  char *err_buf, uint32_t err_buf_size) {
  if (fn_handle == 0 || fn_handle >= g_n_fns) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "bad_fn_handle");
    return 0;
  }
  CUdeviceptr dptrs[BUF_CAP];
  void       *args [BUF_CAP];
  if (n_bufs > BUF_CAP) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "too_many_bufs");
    return 0;
  }
  for (uint32_t i = 0; i < n_bufs; i++) {
    uint32_t h = buf_handles[i];
    if (h == 0 || h >= g_n_bufs) {
      if (err_buf && err_buf_size)
        snprintf(err_buf, err_buf_size, "bad_buf_handle[%u]=%u", i, h);
      return 0;
    }
    dptrs[i] = g_bufs[h].dptr;
    args [i] = &dptrs[i];
  }
  CUevent ev_start = NULL, ev_stop = NULL;
  CUresult r = cuEventCreate(&ev_start, CU_EVENT_DEFAULT);
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuEventCreate", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    return 0;
  }
  r = cuEventCreate(&ev_stop, CU_EVENT_DEFAULT);
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuEventCreate", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    cuEventDestroy(ev_start);
    return 0;
  }

  uint64_t t0 = now_ns();
  cuEventRecord(ev_start, NULL);
  r = cuLaunchKernel(g_fns[fn_handle].func,
                     grid_x, 1, 1, block_x, 1, 1,
                     0, NULL, args, NULL);
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuLaunchKernel", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    cuEventDestroy(ev_start); cuEventDestroy(ev_stop);
    return 0;
  }
  cuEventRecord(ev_stop, NULL);
  r = cuEventSynchronize(ev_stop);
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuEventSynchronize", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    cuEventDestroy(ev_start); cuEventDestroy(ev_stop);
    return 0;
  }
  uint64_t t_done = now_ns();

  float gpu_ms = 0.0f;
  r = cuEventElapsedTime(&gpu_ms, ev_start, ev_stop);
  cuEventDestroy(ev_start);
  cuEventDestroy(ev_stop);
  if (r != CUDA_SUCCESS) {
    set_drv_err("cuEventElapsedTime", r);
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "%s", g_err);
    return 0;
  }
  if (wall_ns_out) *wall_ns_out = t_done - t0;
  if (gpu_ns_out)  *gpu_ns_out  = (uint64_t)((double)gpu_ms * 1e6);
  return 1;
}
