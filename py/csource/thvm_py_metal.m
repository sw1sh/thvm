// thvm_py_metal.m -- in-process Metal compile + dispatch + timing
// for the Python ctypes wrapper. Avoids xcrun subprocess + metallib
// roundtrips during BEAM search.
//
// This file is intentionally independent of thvm's own Metal backend
// init (which reads a precompiled default.metallib we don't ship with
// the Python package). We hold our own MTLDevice / MTLCommandQueue +
// small handle tables for PSOs and buffers.
//
// All entry points are extern "C" via EXPORT and prefixed py_metal_*.

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))

// ---------------- runtime state ----------------
static id<MTLDevice>       g_dev = nil;
static id<MTLCommandQueue> g_queue = nil;
static double              g_ns_per_tick = 0.0;

// Handle tables. Index 0 reserved as "invalid". Bound by allocation
// pattern: agents typically build a few PSOs and many buffers per
// run, so 256 PSOs + 1024 buffers is plenty.
#define PSO_CAP 256
#define BUF_CAP 1024
static id<MTLComputePipelineState> g_psos[PSO_CAP];
static id<MTLBuffer>               g_bufs[BUF_CAP];
static uint32_t                    g_n_psos = 1;  // 0 reserved
static uint32_t                    g_n_bufs = 1;

static uint64_t now_ns(void) {
  return (uint64_t)(mach_absolute_time() * g_ns_per_tick);
}

// ---------------- lifecycle ----------------
EXPORT int py_metal_init(void) {
  if (g_dev != nil) return 1;          // already initialized
  mach_timebase_info_data_t tb;
  mach_timebase_info(&tb);
  g_ns_per_tick = (double)tb.numer / (double)tb.denom;

  g_dev = MTLCreateSystemDefaultDevice();
  if (g_dev == nil) {
    fprintf(stderr, "py_metal_init: no Metal device available\n");
    return 0;
  }
  g_queue = [g_dev newCommandQueue];
  if (g_queue == nil) {
    fprintf(stderr, "py_metal_init: failed to create command queue\n");
    g_dev = nil;
    return 0;
  }
  return 1;
}

EXPORT void py_metal_shutdown(void) {
  for (uint32_t i = 1; i < g_n_psos; i++) g_psos[i] = nil;
  for (uint32_t i = 1; i < g_n_bufs; i++) g_bufs[i] = nil;
  g_n_psos = 1;
  g_n_bufs = 1;
  g_queue = nil;
  g_dev = nil;
}

// ---------------- compile MSL string -> PSO handle ----------------
// Returns a 1-based PSO handle; 0 on failure (and writes a reason
// into the optional err_buf if provided). err_buf size must be >= 256.
EXPORT uint32_t py_metal_compile_msl(const char *src, const char *fn_name,
                                     char *err_buf, uint32_t err_buf_size) {
  if (g_dev == nil) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "metal_not_initialized");
    return 0;
  }
  if (g_n_psos >= PSO_CAP) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "pso_table_full");
    return 0;
  }
  @autoreleasepool {
    NSError *err = nil;
    NSString *srcStr = [NSString stringWithUTF8String:src];
    id<MTLLibrary> lib = [g_dev newLibraryWithSource:srcStr options:nil error:&err];
    if (lib == nil) {
      if (err_buf && err_buf_size) {
        const char *msg = err ? [[err localizedDescription] UTF8String] : "(unknown)";
        snprintf(err_buf, err_buf_size, "compile: %s", msg);
      }
      return 0;
    }
    id<MTLFunction> fn = [lib newFunctionWithName:[NSString stringWithUTF8String:fn_name]];
    if (fn == nil) {
      if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "function_not_found:%s", fn_name);
      return 0;
    }
    id<MTLComputePipelineState> pso =
        [g_dev newComputePipelineStateWithFunction:fn error:&err];
    if (pso == nil) {
      if (err_buf && err_buf_size) {
        const char *msg = err ? [[err localizedDescription] UTF8String] : "(unknown)";
        snprintf(err_buf, err_buf_size, "pso: %s", msg);
      }
      return 0;
    }
    uint32_t handle = g_n_psos++;
    g_psos[handle] = pso;
    return handle;
  }
}

EXPORT uint32_t py_metal_pso_max_threads(uint32_t handle) {
  if (handle == 0 || handle >= g_n_psos) return 0;
  return (uint32_t)g_psos[handle].maxTotalThreadsPerThreadgroup;
}

EXPORT void py_metal_pso_release(uint32_t handle) {
  if (handle == 0 || handle >= g_n_psos) return;
  g_psos[handle] = nil;
}

// ---------------- buffers ----------------
EXPORT uint32_t py_metal_buf_alloc(uint64_t nbytes) {
  if (g_dev == nil) return 0;
  if (g_n_bufs >= BUF_CAP) return 0;
  id<MTLBuffer> b = [g_dev newBufferWithLength:nbytes
                                       options:MTLResourceStorageModeShared];
  if (b == nil) return 0;
  uint32_t handle = g_n_bufs++;
  g_bufs[handle] = b;
  return handle;
}

EXPORT int py_metal_buf_write(uint32_t handle, const void *src, uint64_t nbytes) {
  if (handle == 0 || handle >= g_n_bufs) return 0;
  if (nbytes > g_bufs[handle].length) return 0;
  memcpy(g_bufs[handle].contents, src, nbytes);
  return 1;
}

EXPORT int py_metal_buf_read(uint32_t handle, void *dst, uint64_t nbytes) {
  if (handle == 0 || handle >= g_n_bufs) return 0;
  if (nbytes > g_bufs[handle].length) return 0;
  memcpy(dst, g_bufs[handle].contents, nbytes);
  return 1;
}

EXPORT void py_metal_buf_release(uint32_t handle) {
  if (handle == 0 || handle >= g_n_bufs) return;
  g_bufs[handle] = nil;
}

// ---------------- dispatch ----------------
// Encodes one command buffer with the given PSO + buffer bindings,
// commits, waits, returns wall time in nanoseconds. buf_handles is
// a list of buffer handles bound at indices [0..n_bufs-1] in order.
//
// Returns 0 on dispatch error (and writes a reason if err_buf given).
EXPORT uint64_t py_metal_dispatch(uint32_t pso_handle,
                                  const uint32_t *buf_handles,
                                  uint32_t n_bufs,
                                  uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                  uint32_t tg_x, uint32_t tg_y, uint32_t tg_z,
                                  char *err_buf, uint32_t err_buf_size) {
  if (pso_handle == 0 || pso_handle >= g_n_psos) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "bad_pso_handle");
    return 0;
  }
  @autoreleasepool {
    uint64_t t0 = now_ns();
    id<MTLCommandBuffer> cmd = [g_queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:g_psos[pso_handle]];
    for (uint32_t i = 0; i < n_bufs; i++) {
      uint32_t h = buf_handles[i];
      if (h == 0 || h >= g_n_bufs) {
        if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "bad_buf_handle[%u]=%u", i, h);
        [enc endEncoding];
        return 0;
      }
      [enc setBuffer:g_bufs[h] offset:0 atIndex:i];
    }
    [enc dispatchThreads:MTLSizeMake(grid_x, grid_y, grid_z)
       threadsPerThreadgroup:MTLSizeMake(tg_x, tg_y, tg_z)];
    [enc endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    if (cmd.error) {
      if (err_buf && err_buf_size) {
        snprintf(err_buf, err_buf_size, "dispatch: %s",
                 [[cmd.error localizedDescription] UTF8String]);
      }
      return 0;
    }
    return now_ns() - t0;
  }
}

// Variant that times only the GPU portion (commit -> waitUntilCompleted)
// and returns BOTH wall time and GPU time so the caller can see the
// CPU-side encode overhead.
EXPORT int py_metal_dispatch_timed(uint32_t pso_handle,
                                   const uint32_t *buf_handles, uint32_t n_bufs,
                                   uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                   uint32_t tg_x, uint32_t tg_y, uint32_t tg_z,
                                   uint64_t *wall_ns_out, uint64_t *gpu_ns_out,
                                   char *err_buf, uint32_t err_buf_size) {
  if (pso_handle == 0 || pso_handle >= g_n_psos) {
    if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "bad_pso_handle");
    return 0;
  }
  @autoreleasepool {
    uint64_t t0 = now_ns();
    id<MTLCommandBuffer> cmd = [g_queue commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:g_psos[pso_handle]];
    for (uint32_t i = 0; i < n_bufs; i++) {
      uint32_t h = buf_handles[i];
      if (h == 0 || h >= g_n_bufs) {
        if (err_buf && err_buf_size) snprintf(err_buf, err_buf_size, "bad_buf_handle[%u]=%u", i, h);
        [enc endEncoding];
        return 0;
      }
      [enc setBuffer:g_bufs[h] offset:0 atIndex:i];
    }
    [enc dispatchThreads:MTLSizeMake(grid_x, grid_y, grid_z)
       threadsPerThreadgroup:MTLSizeMake(tg_x, tg_y, tg_z)];
    [enc endEncoding];
    uint64_t t_commit = now_ns();
    [cmd commit];
    [cmd waitUntilCompleted];
    uint64_t t_done = now_ns();
    if (cmd.error) {
      if (err_buf && err_buf_size) {
        snprintf(err_buf, err_buf_size, "dispatch: %s",
                 [[cmd.error localizedDescription] UTF8String]);
      }
      return 0;
    }
    if (wall_ns_out) *wall_ns_out = t_done - t0;
    if (gpu_ns_out)  *gpu_ns_out  = t_done - t_commit;
    return 1;
  }
}
