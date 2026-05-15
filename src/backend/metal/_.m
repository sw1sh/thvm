// backend/metal/_.m -- Metal backend, Objective-C edition.
//
// Compiled separately from the single-TU C runtime; the umbrella
// src/thvm.c includes thvm.h which forward-declares METAL_BACKEND
// (extern Backend), and links this .o under -DTHVM_HAS_METAL.
//
// Includes the runtime header for type definitions (Backend,
// KernelEntry, KProgOp, UOP_* enums).  thvm.h is C-only but
// compiles cleanly under Objective-C / ARC.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // iter X: stat() for persistent metallib disk cache
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "../../thvm.h"

// Lazy file-scope handles to the system default Metal device, a
// command queue, and the loaded metallib.  ARC owns all three;
// metal_shutdown releases by nilling them.  All stay nil between
// init/shutdown cycles so repeated thvm_init calls reset cleanly.
static id<MTLDevice>       METAL_DEVICE = nil;
static id<MTLCommandQueue> METAL_QUEUE  = nil;
static id<MTLLibrary>      METAL_LIB    = nil;
static id<MTLCommandBuffer> METAL_BATCH_CMD = nil;
static u32                  METAL_BATCH_DEPTH = 0;
static u32                  METAL_ENCODING_DEPTH = 0;

static void metal_buf_decref(u32 buf_id);
static void metal_buf_free(u32 buf_id);
static int metal_kernel_has_applied_opt(struct KernelEntry const *ke, u8 op);

#define METAL_DEFER_DECREF_CAP (1u << 20)
static u32 METAL_DEFER_DECREFS[METAL_DEFER_DECREF_CAP];
static u32 METAL_DEFER_DECREF_LEN = 0;
static u64 METAL_DEFER_DECREF_BYTES = 0;
static u64 METAL_PEAK_LIVE_BYTES = 0;
static u64 METAL_PEAK_RETAINED_BYTES = 0;
static u64 METAL_PEAK_DEFERRED_BYTES = 0;

// GPU-time accumulator: every batch flush (metal_dispatch_flush) and
// every standalone command-buffer submit waits on the command buffer,
// then reads [cmd GPUEndTime] - [cmd GPUStartTime] -- Apple's
// wall-clock GPU execution time for that buffer.  We sum the
// microseconds across the process here.  The WL surface TMetalGpuTime[]
// reads {METAL_GPU_US_TOTAL, METAL_GPU_FLUSH_COUNT}; the
// beautiful_mnist bench takes a delta around the timed loop to report
// gpu_us_per_step (a real per-step GPU compute number, separate from
// the WL-side wall=...ms which also includes re-encode overhead).
static u64 METAL_GPU_US_TOTAL = 0;
static u64 METAL_GPU_FLUSH_COUNT = 0;

// Per-op GPU profiling.  When THVM_METAL_PROFILE_PEROP=1, batching is
// disabled in metal_dispatch_begin so every kernel dispatch gets its
// own command buffer; metal_submit_if_standalone then attributes that
// buffer's [GPUEndTime]-[GPUStartTime] to METAL_PEROP_CUR_KID, the kid
// set at the top of metal_dispatch_kernel.  The result is a real
// per-kernel GPU-us breakdown (vs. the batched path, where one flush
// covers ~25 kernels).  Costs more dispatch overhead -- profile only.
static int metal_perop_enabled(void) {
  static int known = 0;
  static int enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_METAL_PROFILE_PEROP");
    enabled = (e != NULL && e[0] == '1');
    known = 1;
  }
  return enabled;
}
static u32 METAL_PEROP_CUR_KID = 0;

static void metal_record_gpu_time(id<MTLCommandBuffer> cmd) {
  if (cmd == nil) return;
  double t0 = cmd.GPUStartTime;
  double t1 = cmd.GPUEndTime;
  if (t1 > t0 && t0 > 0.0) {
    u64 us = (u64)((t1 - t0) * 1e6);
    METAL_GPU_US_TOTAL += us;
    if (metal_perop_enabled() && METAL_PEROP_CUR_KID != 0) {
      cg_profile_record_gpu(METAL_PEROP_CUR_KID, us);
    }
  }
  METAL_GPU_FLUSH_COUNT++;
}

static void metal_record_memory_peak(void);
static void metal_freelist_trim(void);

#ifndef THVM_METAL_METALLIB
#define THVM_METAL_METALLIB "build/default.metallib"
#endif

// Forward-declared here so metal_init can reset the length on
// repeated lifecycle cycles; the actual table + length live
// alongside METAL_BUFS below.
#define METAL_FREELIST_CAP 4096
static u32 METAL_FREELIST    [METAL_FREELIST_CAP];
static u32 METAL_FREELIST_LEN = 0;

static int metal_batch_enabled(void) {
  static int known = 0;
  static int enabled = 1;
  if (!known) {
    char const *e = getenv("THVM_METAL_BATCH");
    enabled = (e == NULL || e[0] != '0');
    known = 1;
  }
  return enabled;
}

// Deferred-decref backlog cap, in bytes.  Buffers freed mid-batch sit
// here until the next flush (the in-flight command buffer must commit
// before they're safe to release).  Default 256 MiB: enough to amortise
// GPU syncs across a run of same-sized intermediates, small enough that
// the high-batch-size per-op fallback path can't balloon retained
// memory.  THVM_METAL_DEFER_BYTES (bytes; 0 = unlimited) overrides.
static u64 metal_defer_limit_bytes(void) {
  static int known = 0;
  static u64 limit = 256ull * 1024ull * 1024ull;
  if (!known) {
    char const *e = getenv("THVM_METAL_DEFER_BYTES");
    if (e != NULL && e[0] != '\0') {
      limit = strtoull(e, NULL, 10);
    }
    known = 1;
  }
  return limit;
}

// Recycle-list cap, in bytes.  Freed-but-retained MTLBuffers kept for
// same-size reuse by a future alloc.  Default 256 MiB (was 1 GiB):
// covers the recurring conv/BN intermediate shapes without pinning a
// gigabyte of idle unified memory.  THVM_METAL_FREELIST_BYTES overrides.
// Re-read every call (not memoized): tests flip this env var at runtime
// to exercise the cap-zero / cap-tight paths.
static u64 metal_freelist_limit_bytes(void) {
  char const *e = getenv("THVM_METAL_FREELIST_BYTES");
  if (e != NULL && e[0] != '\0') {
    return strtoull(e, NULL, 10);
  }
  return 256ull * 1024ull * 1024ull;
}

static void metal_dispatch_flush(void) {
  id<MTLCommandBuffer> cmd = METAL_BATCH_CMD;
  METAL_BATCH_CMD = nil;
  if (cmd != nil) {
    [cmd commit];
    [cmd waitUntilCompleted];
    metal_record_gpu_time(cmd);
  }
  u32 n = METAL_DEFER_DECREF_LEN;
  METAL_DEFER_DECREF_LEN = 0;
  METAL_DEFER_DECREF_BYTES = 0;
  for (u32 i = 0; i < n; i++) {
    metal_buf_decref(METAL_DEFER_DECREFS[i]);
  }
  metal_record_memory_peak();
}

static void metal_dispatch_begin(void) {
  if (METAL_QUEUE == nil) return;
  if (!metal_batch_enabled()) return;
  // Per-op GPU profiling needs one command buffer per kernel so the
  // GPUStartTime/GPUEndTime delta is attributable; skip the batch.
  if (metal_perop_enabled()) return;
  METAL_BATCH_DEPTH++;
}

static void metal_dispatch_end(void) {
  if (METAL_BATCH_DEPTH == 0) return;
  METAL_BATCH_DEPTH--;
  if (METAL_BATCH_DEPTH == 0) {
    metal_dispatch_flush();
  }
}

static id<MTLCommandBuffer> metal_command_buffer(void) {
  if (METAL_BATCH_DEPTH == 0) {
    return [METAL_QUEUE commandBuffer];
  }
  if (METAL_BATCH_CMD == nil) {
    METAL_BATCH_CMD = [METAL_QUEUE commandBuffer];
  }
  return METAL_BATCH_CMD;
}

static void metal_submit_if_standalone(id<MTLCommandBuffer> cmd) {
  if (cmd == nil) return;
  if (METAL_BATCH_DEPTH > 0 && cmd == METAL_BATCH_CMD) return;
  [cmd commit];
  [cmd waitUntilCompleted];
  metal_record_gpu_time(cmd);
}

// Forward decls for the JIT counters: definitions live inside the
// METAL_JIT_CACHE block further down, but metal_init / metal_shutdown
// (defined just below) need to read and reset them.
static u64 METAL_JIT_BUILD_HITS;
static u64 METAL_JIT_BUILD_MISSES;
static u64 METAL_JIT_BUILD_BYPASS;
static u64 METAL_JIT_BUILD_COMPILE_US;
// Persistent on-disk PSO cache counters.  Disk-hit = MTLBinaryArchive
// load + pipeline-state-from-archive succeeded (avoided AIR->GPU
// re-compile).  Disk-miss = no cache file, or load failed (corrupt
// file, ABI drift) so we fell back to fresh compile + serialize.
// Bytes counters track aggregate read/write across the process.
static u64 METAL_JIT_BUILD_DISK_HITS;
static u64 METAL_JIT_BUILD_DISK_MISSES;
static u64 METAL_JIT_BUILD_DISK_BYTES_R;
static u64 METAL_JIT_BUILD_DISK_BYTES_W;
fn void thvm_metal_jit_counters_reset(void);
static void metal_pso_cache_init(void);

static int metal_init(void) {
  METAL_DEVICE = MTLCreateSystemDefaultDevice();
  if (METAL_DEVICE == nil) {
    fprintf(stderr, "thvm: metal_init -- no Metal device available\n");
    return -1;
  }
  METAL_QUEUE = [METAL_DEVICE newCommandQueue];
  if (METAL_QUEUE == nil) {
    fprintf(stderr, "thvm: metal_init -- failed to create command queue on %s\n",
            [[METAL_DEVICE name] UTF8String]);
    METAL_DEVICE = nil;
    return -1;
  }
  NSError *err = nil;
  NSURL    *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:THVM_METAL_METALLIB]];
  METAL_LIB = [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (METAL_LIB == nil) {
    fprintf(stderr, "thvm: metal_init -- failed to load metallib at %s: %s\n",
            THVM_METAL_METALLIB,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    METAL_QUEUE  = nil;
    METAL_DEVICE = nil;
    return -1;
  }
  fprintf(stderr, "thvm: metal_init -- device: %s; metallib: %s (%lu function%s)\n",
          [[METAL_DEVICE name] UTF8String],
          THVM_METAL_METALLIB,
          (unsigned long)[[METAL_LIB functionNames] count],
          [[METAL_LIB functionNames] count] == 1 ? "" : "s");
  METAL_FREELIST_LEN = 0;
  METAL_BATCH_CMD    = nil;
  METAL_BATCH_DEPTH  = 0;
  METAL_DEFER_DECREF_LEN = 0;
  thvm_metal_jit_counters_reset();
  METAL_DEFER_DECREF_BYTES = 0;
  METAL_PEAK_LIVE_BYTES = 0;
  METAL_PEAK_RETAINED_BYTES = 0;
  METAL_PEAK_DEFERRED_BYTES = 0;
  METAL_GPU_US_TOTAL = 0;
  METAL_GPU_FLUSH_COUNT = 0;
  // Set up the on-disk PSO cache: mkdir, sweep stale (>30d) files.
  // Safe to call before the first dispatch; no-op if disabled via
  // THVM_METAL_PSO_CACHE=0.  Runs after METAL_DEVICE is ready.
  metal_pso_cache_init();
  return 0;
}

// Forward decls: metal_shutdown defined after the buffer table so
// it can iterate METAL_BUFS to release outstanding buffers.
static void metal_shutdown(void);

// Iter BB: file-scope cache for the AOT-on-Metal book_heap MTLBuffer
// wrapper.  Tentative-definitions here so metal_shutdown (above its
// helper) can reference them; the helper aot_metal_heap_buf is
// defined alongside the kernel dispatch code further down.
static id<MTLBuffer> AOT_METAL_HEAP_BUF = nil;
static Term         *AOT_METAL_HEAP_PTR = NULL;
static u64           AOT_METAL_HEAP_LEN = 0;

// Buffer table: parallel to TenDesc.buf_id.  buf_id 0 is reserved
// ("no buffer").  ARC owns each MTLBuffer via the strong reference
// in METAL_BUFS[].  Refcounts are tracked separately so that
// multiple TenDescs can share the same underlying buffer (view
// aliasing) -- mirrors the CPU backend's CpuBuf table.
//
// Apple Silicon's MTLResourceStorageModeShared lets the CPU access
// buffer contents directly (no blit required) -- buf_read/write
// just memcpy through `buffer.contents`.

#define METAL_BUFS_CAP (1ULL << 16)

typedef struct {
  id<MTLBuffer> buf;
  u64           nbytes;
  u32           refcount;
  u8            preserved;
} MetalBuf;

static MetalBuf METAL_BUFS[METAL_BUFS_CAP];
static u32      METAL_BUFS_NEXT = 1;

static u64 metal_freelist_bytes(void) {
  u64 total = 0;
  for (u32 i = 0; i < METAL_FREELIST_LEN; i++) {
    u32 bid = METAL_FREELIST[i];
    if (bid == 0 || bid >= METAL_BUFS_NEXT) continue;
    if (METAL_BUFS[bid].buf == nil) continue;
    if (METAL_BUFS[bid].refcount != 0) continue;
    total += METAL_BUFS[bid].nbytes;
  }
  return total;
}

static void metal_record_memory_peak(void) {
  u64 live = 0;
  u64 retained = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf == nil) continue;
    retained += METAL_BUFS[i].nbytes;
    if (METAL_BUFS[i].refcount > 0) {
      live += METAL_BUFS[i].nbytes;
    }
  }
  // Backstop: abort rather than thrash the host if the total Metal
  // buffer footprint blows past the ceiling.  See thvm_live_byte_ceiling
  // (src/thvm.h) -- the JIT capture run pins every kernel output and the
  // schedule does not yet reuse buffers across non-overlapping
  // lifetimes, so a high-batch-size cold capture can balloon without
  // bound.  Fail loud with the diagnostic instead.
  {
    u64 ceiling = thvm_live_byte_ceiling();
    if (ceiling != 0 && retained > ceiling) {
      fprintf(stderr,
        "metal: total live buffer bytes %llu exceed THVM_MAX_LIVE_BYTES "
        "ceiling %llu (live=%llu deferred=%llu freelist=%llu).  The "
        "schedule is pinning more buffers than fit -- likely the per-op "
        "fallback path materializing im2col intermediates and/or the JIT "
        "capture pinning every kernel output without buffer reuse "
        "(see docs/plans/beautiful_mnist_parity.md M1/M3/M4).  Raise "
        "THVM_MAX_LIVE_BYTES (bytes; 0 = unlimited) if this is "
        "intentional.\n",
        (unsigned long long)retained, (unsigned long long)ceiling,
        (unsigned long long)live, (unsigned long long)METAL_DEFER_DECREF_BYTES,
        (unsigned long long)metal_freelist_bytes());
      exit(1);
    }
  }
  if (live > METAL_PEAK_LIVE_BYTES) {
    METAL_PEAK_LIVE_BYTES = live;
  }
  if (retained > METAL_PEAK_RETAINED_BYTES) {
    METAL_PEAK_RETAINED_BYTES = retained;
  }
  if (METAL_DEFER_DECREF_BYTES > METAL_PEAK_DEFERRED_BYTES) {
    METAL_PEAK_DEFERRED_BYTES = METAL_DEFER_DECREF_BYTES;
  }
}

static void metal_freelist_trim(void) {
  u64 limit = metal_freelist_limit_bytes();
  u64 total = metal_freelist_bytes();
  while (METAL_FREELIST_LEN > 0 && total > limit) {
    u32 best_i = METAL_FREELIST_LEN;
    u32 best_bid = 0;
    u64 best_bytes = 0;
    for (u32 i = 0; i < METAL_FREELIST_LEN; i++) {
      u32 bid = METAL_FREELIST[i];
      if (bid == 0 || bid >= METAL_BUFS_NEXT) continue;
      if (METAL_BUFS[bid].buf == nil || METAL_BUFS[bid].refcount != 0) continue;
      if (METAL_BUFS[bid].nbytes >= best_bytes) {
        best_i = i;
        best_bid = bid;
        best_bytes = METAL_BUFS[bid].nbytes;
      }
    }
    if (best_i == METAL_FREELIST_LEN || best_bid == 0 || best_bytes == 0) {
      break;
    }
    METAL_FREELIST[best_i] = METAL_FREELIST[METAL_FREELIST_LEN - 1];
    METAL_FREELIST_LEN--;
    metal_buf_free(best_bid);
    total = best_bytes >= total ? 0 : total - best_bytes;
  }
}


// bm4c: Metal mirror of CPU's free-list (cpu_buf_freelist_push /
// cpu_buf_freelist_try_pop).  Recycles MTLBuffer slots by exact
// nbytes match; the underlying MTLBuffer object survives in the
// METAL_BUFS slot until the slot is popped or metal_shutdown
// clears it.  Drops refcount to 0 on push so the recycled-but-
// unallocated slot doesn't keep counting toward
// thvm_wl_metal_buf_table reports.
static int metal_buf_freelist_push_impl(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return 0;
  if (METAL_FREELIST_LEN >= METAL_FREELIST_CAP) return 0;
  if (METAL_BUFS[buf_id].buf == nil) return 0;
  METAL_FREELIST[METAL_FREELIST_LEN++] = buf_id;
  METAL_BUFS[buf_id].refcount = 0;   // stop counting in live bytes
  METAL_BUFS[buf_id].preserved = 0;
  metal_freelist_trim();
  metal_record_memory_peak();
  return 1;
}

static void metal_buf_freelist_push(u32 buf_id) {
  (void)metal_buf_freelist_push_impl(buf_id);
}

static u32 metal_buf_freelist_try_pop(u64 nbytes) {
  for (u32 i = 0; i < METAL_FREELIST_LEN; i++) {
    u32 bid = METAL_FREELIST[i];
    if (bid == 0 || bid >= METAL_BUFS_NEXT) continue;
    if (METAL_BUFS[bid].buf == nil) continue;             // stale entry
    if (METAL_BUFS[bid].nbytes != nbytes) continue;
    // Pop: swap with last + shrink.
    METAL_FREELIST[i] = METAL_FREELIST[METAL_FREELIST_LEN - 1];
    METAL_FREELIST_LEN--;
    // Zero shared-mode contents so the recycled slot looks fresh.
    memset([METAL_BUFS[bid].buf contents], 0, (size_t)nbytes);
    METAL_BUFS[bid].refcount = 1;
    METAL_BUFS[bid].preserved = 0;
    metal_record_memory_peak();
    return bid;
  }
  return 0;   // miss
}

static u32 metal_buf_alloc(u64 nbytes) {
  if (METAL_DEVICE == nil) return 0;
  // Refuse a pathologically large single allocation rather than
  // newBufferWithLength: it and pin tens of GB of wired unified
  // memory.  See thvm_buf_byte_ceiling (src/thvm.h).
  u64 ceiling = thvm_buf_byte_ceiling();
  if (ceiling != 0 && nbytes > ceiling) {
    fprintf(stderr,
      "metal_buf_alloc: refusing %llu-byte allocation (> THVM_MAX_BUF_BYTES "
      "ceiling %llu); a kernel program is asking for a buffer far larger "
      "than any legitimate tensor -- likely an unfused im2col/EXPAND "
      "intermediate.  Raise THVM_MAX_BUF_BYTES (bytes; 0 = unlimited) if "
      "this is intentional.\n",
      (unsigned long long)nbytes, (unsigned long long)ceiling);
    exit(1);
  }
  u64 limit = metal_defer_limit_bytes();
  if (METAL_ENCODING_DEPTH == 0
      && METAL_DEFER_DECREF_LEN > 0
      && (limit == 0 || METAL_DEFER_DECREF_BYTES + nbytes > limit)) {
    metal_dispatch_flush();
  }
  // bm4c: free-list lookup first.  Recycles a matching-size
  // MTLBuffer slot if available; falls through to fresh
  // newBufferWithLength on miss.
  u32 recycled = metal_buf_freelist_try_pop(nbytes);
  if (recycled != 0) return recycled;

  u32 id = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf == nil && METAL_BUFS[i].refcount == 0) {
      id = i;
      break;
    }
  }
  if (id == 0) {
    if (METAL_BUFS_NEXT >= METAL_BUFS_CAP) {
      fprintf(stderr, "thvm: metal_buf_alloc -- buffer table full\n");
      return 0;
    }
    id = METAL_BUFS_NEXT++;
  }
  METAL_BUFS[id].buf      = [METAL_DEVICE newBufferWithLength:nbytes
                                                      options:MTLResourceStorageModeShared];
  METAL_BUFS[id].nbytes   = nbytes;
  METAL_BUFS[id].refcount = 1;
  METAL_BUFS[id].preserved = 0;
  if (METAL_BUFS[id].buf == nil) {
    fprintf(stderr, "thvm: metal_buf_alloc -- failed to allocate %llu bytes\n",
            (unsigned long long)nbytes);
    METAL_BUFS[id].refcount = 0;
    return 0;
  }
  metal_record_memory_peak();
  return id;
}

// Non-static accessor for cross-TU push (used by thvmlink.c +
// future Metal-side rollback wiring).
void thvm_metal_buf_freelist_push(u32 buf_id) {
  metal_buf_freelist_push(buf_id);
}

// Undo a metal_buf_freelist_push: if `buf_id` is still parked on the
// recycle list (no intervening metal_buf_alloc popped it), pull it off
// and restore refcount = 1.  Used by the per-realize memory planner to
// roll back speculative pushes at end-of-pass so a later pass's
// allocations don't recycle a buf whose TenDesc is still referenced.
void thvm_metal_buf_freelist_remove(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  for (u32 i = 0; i < METAL_FREELIST_LEN; i++) {
    if (METAL_FREELIST[i] != buf_id) continue;
    METAL_FREELIST[i] = METAL_FREELIST[METAL_FREELIST_LEN - 1];
    METAL_FREELIST_LEN--;
    METAL_BUFS[buf_id].refcount = 1;
    metal_record_memory_peak();
    return;
  }
}

u32 thvm_metal_buf_refcount(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return 0;
  return METAL_BUFS[buf_id].refcount;
}

static void metal_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  METAL_BUFS[buf_id].buf      = nil;
  METAL_BUFS[buf_id].nbytes   = 0;
  METAL_BUFS[buf_id].refcount = 0;
  METAL_BUFS[buf_id].preserved = 0;
  metal_record_memory_peak();
}

static void metal_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  METAL_BUFS[buf_id].refcount++;
  metal_record_memory_peak();
}

static void metal_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (METAL_BUFS[buf_id].refcount == 0) return;
  if (METAL_BUFS[buf_id].refcount == 1) {
    metal_dispatch_flush();
  }
  if (--METAL_BUFS[buf_id].refcount != 0) return;
  if (!metal_buf_freelist_push_impl(buf_id)) {
    metal_buf_free(buf_id);
  }
}

static void metal_buf_decref_after_batch(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (METAL_BATCH_DEPTH == 0 && METAL_BATCH_CMD == nil) {
    metal_buf_decref(buf_id);
    return;
  }
  u64 reclaim_bytes = METAL_BUFS[buf_id].refcount == 1
      ? METAL_BUFS[buf_id].nbytes
      : 0;
  if (METAL_DEFER_DECREF_LEN >= METAL_DEFER_DECREF_CAP) {
    metal_dispatch_flush();
    metal_buf_decref(buf_id);
    return;
  }
  METAL_DEFER_DECREFS[METAL_DEFER_DECREF_LEN++] = buf_id;
  METAL_DEFER_DECREF_BYTES += reclaim_bytes;
  metal_record_memory_peak();
  u64 limit = metal_defer_limit_bytes();
  if (METAL_ENCODING_DEPTH == 0
      && (limit == 0 || METAL_DEFER_DECREF_BYTES >= limit)) {
    metal_dispatch_flush();
  }
}

static int metal_buf_read(u32 buf_id, void *dst, u64 nbytes) {
  metal_dispatch_flush();
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[buf_id].buf == nil)            return -1;
  if (METAL_BUFS[buf_id].refcount == 0)         return -1;
  u64 cap = METAL_BUFS[buf_id].nbytes;
  if (nbytes > cap) nbytes = cap;
  memcpy(dst, [METAL_BUFS[buf_id].buf contents], (size_t)nbytes);
  return 0;
}

static int metal_buf_write(u32 buf_id, const void *src, u64 nbytes) {
  metal_dispatch_flush();
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[buf_id].buf == nil)            return -1;
  if (METAL_BUFS[buf_id].refcount == 0)         return -1;
  u64 cap = METAL_BUFS[buf_id].nbytes;
  if (nbytes > cap) nbytes = cap;
  memcpy([METAL_BUFS[buf_id].buf contents], src, (size_t)nbytes);
  return 0;
}

static int metal_buf_copy(u32 dst_buf_id, u32 src_buf_id, u64 nbytes) {
  if (dst_buf_id == 0 || dst_buf_id >= METAL_BUFS_NEXT) return -1;
  if (src_buf_id == 0 || src_buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[dst_buf_id].buf == nil) return -1;
  if (METAL_BUFS[src_buf_id].buf == nil) return -1;
  if (METAL_BUFS[dst_buf_id].refcount == 0) return -1;
  if (METAL_BUFS[src_buf_id].refcount == 0) return -1;
  if (nbytes > METAL_BUFS[dst_buf_id].nbytes) nbytes = METAL_BUFS[dst_buf_id].nbytes;
  if (nbytes > METAL_BUFS[src_buf_id].nbytes) nbytes = METAL_BUFS[src_buf_id].nbytes;
  id<MTLCommandBuffer> cmd = metal_command_buffer();
  if (cmd == nil) return -1;
  id<MTLBlitCommandEncoder> enc = [cmd blitCommandEncoder];
  [enc copyFromBuffer:METAL_BUFS[src_buf_id].buf
         sourceOffset:0
             toBuffer:METAL_BUFS[dst_buf_id].buf
    destinationOffset:0
                 size:(NSUInteger)nbytes];
  [enc endEncoding];
  metal_submit_if_standalone(cmd);
  return 0;
}

// Accessors for the WL bridge's TMetalBufTable export (mp1 of the
// TMemoryPlan visualization arc).  Non-static so thvmlink.c can
// reach them after the Metal .o is linked in; METAL_BUFS itself
// stays file-static.
u32 thvm_metal_buf_count(void) { return METAL_BUFS_NEXT; }
void thvm_metal_buf_get(u32 i, u64 *nbytes_out, u32 *refcount_out) {
  if (i == 0 || i >= METAL_BUFS_NEXT) {
    if (nbytes_out)   *nbytes_out   = 0;
    if (refcount_out) *refcount_out = 0;
    return;
  }
  if (nbytes_out)   *nbytes_out   = METAL_BUFS[i].nbytes;
  if (refcount_out) *refcount_out = METAL_BUFS[i].refcount;
}

u32 thvm_metal_buf_pool_begin(void) {
  return METAL_BUFS_NEXT;
}

void thvm_metal_buf_mark_preserved(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (METAL_BUFS[buf_id].buf == nil) return;
  METAL_BUFS[buf_id].preserved = 1;
}

void thvm_metal_buf_pool_rollback_with_preserve(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > METAL_BUFS_NEXT) return;
  metal_dispatch_flush();
  for (u32 i = wm; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].preserved) continue;
    if (METAL_BUFS[i].buf == nil) continue;
    if (METAL_BUFS[i].refcount == 0) continue;
    if (!metal_buf_freelist_push_impl(i)) {
      metal_buf_free(i);
    }
  }
  metal_record_memory_peak();
}

void thvm_metal_buf_clear_preserved(u32 wm) {
  if (wm < 1) wm = 1;
  for (u32 i = wm; i < METAL_BUFS_NEXT; i++) {
    METAL_BUFS[i].preserved = 0;
  }
}

u64 thvm_metal_live_bytes(void) {
  u64 total = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf != nil && METAL_BUFS[i].refcount > 0) {
      total += METAL_BUFS[i].nbytes;
    }
  }
  return total;
}

u64 thvm_metal_retained_bytes(void) {
  u64 total = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf != nil) {
      total += METAL_BUFS[i].nbytes;
    }
  }
  return total;
}

u64 thvm_metal_deferred_bytes(void) {
  return METAL_DEFER_DECREF_BYTES;
}

u32 thvm_metal_deferred_len(void) {
  return METAL_DEFER_DECREF_LEN;
}

u32 thvm_metal_freelist_len(void) {
  return METAL_FREELIST_LEN;
}

u64 thvm_metal_peak_live_bytes(void) {
  return METAL_PEAK_LIVE_BYTES;
}

u64 thvm_metal_peak_retained_bytes(void) {
  return METAL_PEAK_RETAINED_BYTES;
}

u64 thvm_metal_peak_deferred_bytes(void) {
  return METAL_PEAK_DEFERRED_BYTES;
}

// Per-(opcode, dtype) pipeline cache.  Defined here so metal_shutdown
// can clear it before the MTLLibrary it points into goes away; the
// metal_pipeline_for accessor + on-demand fill live with the rest of
// the dispatch path further down.
static id<MTLComputePipelineState> METAL_PIPELINES_CACHE[UOP_COUNT][32];

// metal_jit cache decls live further down (alongside the MSL emit
// path).  Forward-declare the cache reset so metal_shutdown can
// drop the stale PSOs before the MTLLibrary they reference goes
// away.
static void metal_jit_cache_reset_impl(void);
static void metal_graph_cache_reset_impl(void);

static void metal_shutdown(void) {
  metal_dispatch_flush();
  // JIT counters: opt-in dump on shutdown so bench scripts can see
  // how much of a run's wall time was spent in MTLLibrary compiles
  // vs cache hits.  Read-only -- counters are reset by
  // metal_init below.
  if (getenv("THVM_METAL_JIT_STATS")) {
    fprintf(stderr,
            "thvm: metal_jit stats -- hits=%llu misses=%llu bypass=%llu compile_us=%llu\n",
            (unsigned long long)METAL_JIT_BUILD_HITS,
            (unsigned long long)METAL_JIT_BUILD_MISSES,
            (unsigned long long)METAL_JIT_BUILD_BYPASS,
            (unsigned long long)METAL_JIT_BUILD_COMPILE_US);
  }
  // Persistent PSO cache stats: separate env-var so bench scripts can
  // opt in to disk I/O reporting without the verbose JIT stats above.
  // Format must stay stable -- bench drivers grep for the
  // "metal_pso_cache stats" prefix.
  if (getenv("THVM_METAL_PSO_CACHE_STATS")) {
    fprintf(stderr,
            "thvm: metal_pso_cache stats -- disk_hits=%llu disk_misses=%llu bytes_r=%llu bytes_w=%llu\n",
            (unsigned long long)METAL_JIT_BUILD_DISK_HITS,
            (unsigned long long)METAL_JIT_BUILD_DISK_MISSES,
            (unsigned long long)METAL_JIT_BUILD_DISK_BYTES_R,
            (unsigned long long)METAL_JIT_BUILD_DISK_BYTES_W);
  }
  METAL_BATCH_DEPTH = 0;
  METAL_FREELIST_LEN = 0;
  METAL_DEFER_DECREF_LEN = 0;
  METAL_DEFER_DECREF_BYTES = 0;
  METAL_PEAK_LIVE_BYTES = 0;
  METAL_PEAK_RETAINED_BYTES = 0;
  METAL_PEAK_DEFERRED_BYTES = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    METAL_BUFS[i].buf      = nil;
    METAL_BUFS[i].nbytes   = 0;
    METAL_BUFS[i].refcount = 0;
    METAL_BUFS[i].preserved = 0;
  }
  METAL_BUFS_NEXT = 1;
  // Drop every cached PSO -- they reference the MTLLibrary we're
  // about to nil, and a re-init will produce fresh pipelines from
  // a fresh MTLLibrary.  The metal_jit PSO cache is cleared on the
  // next metal_init (defined further down so it can see
  // METAL_JIT_CACHE / METAL_JIT_PSOS).
  for (u32 op = 0; op < UOP_COUNT; op++)
    for (u32 dt = 0; dt < 32; dt++)
      METAL_PIPELINES_CACHE[op][dt] = nil;
  metal_jit_cache_reset_impl();
  metal_graph_cache_reset_impl();
  // Phase 7 iter BB: drop the cached AOT book_heap MTLBuffer wrapper
  // before the host frees the underlying book_heap pages on
  // thvm_free.  The cached buffer's MTLBuffer object outlives the
  // backing memory otherwise -- harmless under ARC's lazy release
  // unless someone reuses it post-shutdown.
  AOT_METAL_HEAP_BUF = nil;
  AOT_METAL_HEAP_PTR = NULL;
  AOT_METAL_HEAP_LEN = 0;
  METAL_LIB    = nil;
  METAL_QUEUE  = nil;
  METAL_DEVICE = nil;
}

// === metal_jit: fused-program shaders ==================================
//
// PSO cache shared by metal_tile_jit_build (UOp-DAG renderer path).
// Compiles render_uop output to an MTLLibrary at first dispatch and
// caches the resulting MTLComputePipelineState by program hash.
// Open-addressing cache layout matches src/backend/cpu/jit.c.
//
// Cache miss: cg_emit_tile_metal -> [MTLDevice newLibraryWithSource:
//   options:error:] -> [lib newFunctionWithName:@"k"] ->
//   [device newComputePipelineStateWithFunction:].  The PSO lives in
//   METAL_JIT_PSOS (ARC strong) so it survives the cache slot's
//   weak-style id<MTLComputePipelineState> reference.
//
// Buffer-binding convention (matches render_uop's prologue):
//   buffer(0)              : output (device float *)
//   buffer(1..1+n_in-1)    : inputs (device const float *)
// Dispatch shape comes from cg_tile_metal_dispatch_shape.

#define METAL_JIT_CACHE_CAP 256
typedef struct {
  u64 key;   // 0 = empty
} MetalJitSlot;
static MetalJitSlot                METAL_JIT_CACHE[METAL_JIT_CACHE_CAP];
static id<MTLComputePipelineState> METAL_JIT_PSOS [METAL_JIT_CACHE_CAP];

// Per-process counters for the JIT pipeline.  Storage forward-declared
// near metal_init; metal_tile_jit_build (UOp-DAG renderer path) bumps
// these.  metal_init calls _reset to zero them on each (re)init.
fn void thvm_metal_jit_counters_reset(void) {
  METAL_JIT_BUILD_HITS        = 0;
  METAL_JIT_BUILD_MISSES      = 0;
  METAL_JIT_BUILD_BYPASS      = 0;
  METAL_JIT_BUILD_COMPILE_US  = 0;
  METAL_JIT_BUILD_DISK_HITS   = 0;
  METAL_JIT_BUILD_DISK_MISSES = 0;
  METAL_JIT_BUILD_DISK_BYTES_R = 0;
  METAL_JIT_BUILD_DISK_BYTES_W = 0;
}

// Forward-declared as metal_jit_cache_reset_impl above; called from
// metal_shutdown so the next metal_init starts with a clean slate.
static void metal_jit_cache_reset_impl(void) {
  for (u32 i = 0; i < METAL_JIT_CACHE_CAP; i++) {
    METAL_JIT_CACHE[i].key = 0;
    METAL_JIT_PSOS [i]     = nil;
  }
}

// === Persistent on-disk PSO cache =====================================
//
// Caches each successfully-built MTLComputePipelineState by the
// per-kernel u64 hash already used for the in-memory cache slot
// (metal_tile_jit_hash).  Hash incorporates MSL source content
// (via the scalar/tile UOp bytes, dtypes, numels, applied opts) so
// if codegen drifts the key drifts and stale on-disk entries
// become unreachable.
//
// One file per PSO: <dir>/pso-<key:016llx>.bin where dir is
//   $THVM_METAL_PSO_CACHE_DIR
//   $XDG_CACHE_HOME/thvm/metal-jit
//   $HOME/.cache/thvm/metal-jit
// Atomic writes (tmp+rename) so a crashed process doesn't leave
// half-written files.  TTL sweep (30d, mtime) on init keeps the dir
// from growing without bound; the runtime never deletes entries
// during normal use.
//
// Env vars:
//   THVM_METAL_PSO_CACHE=0           -- disable entirely
//   THVM_METAL_PSO_CACHE_DIR=<path>  -- override default dir
//   THVM_METAL_PSO_CACHE_STATS=1     -- dump counters on shutdown
//
// Apple-API gotcha: MTLBinaryArchive caches the AIR->GPU backend
// compile, not MSL->AIR.  We still call newLibraryWithSource: +
// newFunctionWithName: on every cold start (since loading the PSO
// from the archive still requires an MTLFunction for the descriptor),
// but the archive lookup avoids the much slower AIR->GPU compile.
// This is the documented and supported Metal flow.

#define METAL_PSO_CACHE_PATH_MAX 1024
#define METAL_PSO_CACHE_TTL_SECS (30LL * 24LL * 60LL * 60LL)

static int  METAL_PSO_CACHE_ENABLED = 0;
static char METAL_PSO_CACHE_DIR[METAL_PSO_CACHE_PATH_MAX];

// 1 = disabled, 0 = enabled.  Re-read each call (cheap, tests flip
// the env at runtime).
static int metal_pso_cache_disabled(void) {
  char const *e = getenv("THVM_METAL_PSO_CACHE");
  return e != NULL && e[0] == '0';
}

// Resolve the cache directory once per process and store into
// METAL_PSO_CACHE_DIR.  Returns 0 on success, -1 on failure
// (cache disabled, path too long, mkdir fail).
static int metal_pso_cache_resolve_dir(void) {
  METAL_PSO_CACHE_DIR[0] = '\0';
  char const *override = getenv("THVM_METAL_PSO_CACHE_DIR");
  if (override != NULL && override[0] != '\0') {
    if (strlen(override) >= METAL_PSO_CACHE_PATH_MAX) return -1;
    strncpy(METAL_PSO_CACHE_DIR, override, METAL_PSO_CACHE_PATH_MAX - 1);
    METAL_PSO_CACHE_DIR[METAL_PSO_CACHE_PATH_MAX - 1] = '\0';
    return 0;
  }
  char const *xdg = getenv("XDG_CACHE_HOME");
  char const *home = getenv("HOME");
  if (xdg != NULL && xdg[0] != '\0') {
    int n = snprintf(METAL_PSO_CACHE_DIR, sizeof(METAL_PSO_CACHE_DIR),
                     "%s/thvm/metal-jit", xdg);
    if (n <= 0 || (size_t)n >= sizeof(METAL_PSO_CACHE_DIR)) return -1;
    return 0;
  }
  if (home != NULL && home[0] != '\0') {
    int n = snprintf(METAL_PSO_CACHE_DIR, sizeof(METAL_PSO_CACHE_DIR),
                     "%s/.cache/thvm/metal-jit", home);
    if (n <= 0 || (size_t)n >= sizeof(METAL_PSO_CACHE_DIR)) return -1;
    return 0;
  }
  return -1;
}

// mkdir -p for the resolved cache dir (mode 0755).  Returns 0 on
// success or already-exists, -1 on failure.
static int metal_pso_cache_mkdir_p(char const *path) {
  if (path == NULL || path[0] == '\0') return -1;
  char buf[METAL_PSO_CACHE_PATH_MAX];
  size_t n = strlen(path);
  if (n >= sizeof(buf)) return -1;
  memcpy(buf, path, n + 1);
  // Iterate from idx 1 (skip leading '/') and create each segment.
  for (size_t i = 1; i <= n; i++) {
    if (buf[i] == '/' || buf[i] == '\0') {
      char saved = buf[i];
      buf[i] = '\0';
      if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
      buf[i] = saved;
    }
  }
  return 0;
}

// O(n) sweep: stat every regular file in METAL_PSO_CACHE_DIR; unlink
// those with mtime older than METAL_PSO_CACHE_TTL_SECS.  Best-effort;
// any error skips the entry and continues.
static void metal_pso_cache_sweep_old(void) {
  if (!METAL_PSO_CACHE_ENABLED) return;
  DIR *d = opendir(METAL_PSO_CACHE_DIR);
  if (d == NULL) return;
  time_t now = time(NULL);
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (ent->d_name[0] == '.') continue;
    char path[METAL_PSO_CACHE_PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s",
                     METAL_PSO_CACHE_DIR, ent->d_name);
    if (n <= 0 || (size_t)n >= sizeof(path)) continue;
    struct stat st;
    if (stat(path, &st) != 0) continue;
    if (!S_ISREG(st.st_mode)) continue;
    if (now - st.st_mtime > METAL_PSO_CACHE_TTL_SECS) {
      (void)unlink(path);
    }
  }
  closedir(d);
}

static void metal_pso_cache_init(void) {
  METAL_PSO_CACHE_ENABLED = 0;
  if (metal_pso_cache_disabled()) return;
  if (metal_pso_cache_resolve_dir() != 0) return;
  if (metal_pso_cache_mkdir_p(METAL_PSO_CACHE_DIR) != 0) {
    fprintf(stderr, "thvm: metal_pso_cache -- mkdir %s failed (%s); cache disabled\n",
            METAL_PSO_CACHE_DIR, strerror(errno));
    return;
  }
  METAL_PSO_CACHE_ENABLED = 1;
  metal_pso_cache_sweep_old();
}

// Format the per-key path into out (cap >= METAL_PSO_CACHE_PATH_MAX).
// Returns 0 on success, -1 if truncated.
static int metal_pso_cache_path(u64 key, char *out, size_t cap) {
  int n = snprintf(out, cap, "%s/pso-%016llx.bin",
                   METAL_PSO_CACHE_DIR, (unsigned long long)key);
  if (n <= 0 || (size_t)n >= cap) return -1;
  return 0;
}

// Try to load a cached PSO for `key`.  Builds an MTLBinaryArchive
// from the on-disk file, then constructs the pipeline state with
// MTLPipelineOptionFailOnBinaryArchiveMiss so a miss inside the
// archive falls through to fresh compile.  On any failure (no file,
// corrupt, function-name mismatch), returns nil and the caller falls
// through to the fresh-build path -- which will also rewrite the
// corrupt entry.
//
// `mtlFn` must already be constructed from the freshly-compiled
// MTLLibrary; we still need the function identity to look it up in
// the archive (MTLBinaryArchive caches AIR->GPU, not MSL->AIR).
static id<MTLComputePipelineState>
metal_pso_cache_try_load(u64 key,
                         id<MTLFunction> mtlFn,
                         MTLComputePipelineDescriptor *desc) {
  if (!METAL_PSO_CACHE_ENABLED) return nil;
  char path[METAL_PSO_CACHE_PATH_MAX];
  if (metal_pso_cache_path(key, path, sizeof(path)) != 0) return nil;
  struct stat st;
  if (stat(path, &st) != 0) return nil;  // miss -- silent
  if (!S_ISREG(st.st_mode) || st.st_size <= 0) return nil;

  NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
  MTLBinaryArchiveDescriptor *adesc =
      [[MTLBinaryArchiveDescriptor alloc] init];
  [adesc setUrl:url];
  NSError *err = nil;
  id<MTLBinaryArchive> archive =
      [METAL_DEVICE newBinaryArchiveWithDescriptor:adesc error:&err];
  if (archive == nil) {
    // Corrupt or version-mismatched on-disk file.  Best-effort:
    // unlink so the fresh-build path can rewrite a valid one.
    fprintf(stderr,
            "thvm: metal_pso_cache -- archive load failed for %s (%s); evicting\n",
            path,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    (void)unlink(path);
    return nil;
  }
  [desc setComputeFunction:mtlFn];
  [desc setBinaryArchives:@[archive]];
  err = nil;
  id<MTLComputePipelineState> pso =
      [METAL_DEVICE newComputePipelineStateWithDescriptor:desc
                                                   options:MTLPipelineOptionFailOnBinaryArchiveMiss
                                                reflection:NULL
                                                     error:&err];
  // Reset the descriptor's binaryArchives so the fresh-build path
  // (if we get there) doesn't accidentally inherit it.
  [desc setBinaryArchives:nil];
  if (pso == nil) {
    // Archive opened but doesn't contain a matching pipeline.  Treat
    // as a miss -- the fresh-build path will rebuild and overwrite.
    return nil;
  }
  METAL_JIT_BUILD_DISK_BYTES_R += (u64)st.st_size;
  return pso;
}

// Capture the freshly-built PSO into a transient MTLBinaryArchive
// and serialize to disk (atomic via tmp+rename).  Non-fatal on
// failure: caller already has a valid in-memory PSO and the next
// fresh process will simply recompile.
static void metal_pso_cache_store(u64 key,
                                  MTLComputePipelineDescriptor *desc) {
  if (!METAL_PSO_CACHE_ENABLED) return;
  char path[METAL_PSO_CACHE_PATH_MAX];
  if (metal_pso_cache_path(key, path, sizeof(path)) != 0) return;
  // If a file already exists (e.g. we hit the miss-after-load path
  // because the archive had no matching entry), don't double-write.
  // Stat is cheap relative to the serialize itself.
  struct stat existing;
  if (stat(path, &existing) == 0 && S_ISREG(existing.st_mode)
      && existing.st_size > 0) {
    // Still bump the byte counter? No -- caller already counted on
    // read.  Bail.
    return;
  }
  char tmppath[METAL_PSO_CACHE_PATH_MAX];
  int n = snprintf(tmppath, sizeof(tmppath), "%s.tmp.%d",
                   path, (int)getpid());
  if (n <= 0 || (size_t)n >= sizeof(tmppath)) return;

  MTLBinaryArchiveDescriptor *adesc =
      [[MTLBinaryArchiveDescriptor alloc] init];
  [adesc setUrl:nil];  // build fresh, not loaded
  NSError *err = nil;
  id<MTLBinaryArchive> archive =
      [METAL_DEVICE newBinaryArchiveWithDescriptor:adesc error:&err];
  if (archive == nil) {
    fprintf(stderr,
            "thvm: metal_pso_cache -- create archive failed (%s); not caching\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return;
  }
  err = nil;
  BOOL added = [archive addComputePipelineFunctionsWithDescriptor:desc
                                                            error:&err];
  if (!added) {
    fprintf(stderr,
            "thvm: metal_pso_cache -- addComputePipelineFunctions failed (%s); not caching\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return;
  }
  NSURL *tmpurl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:tmppath]];
  err = nil;
  BOOL ok = [archive serializeToURL:tmpurl error:&err];
  if (!ok) {
    fprintf(stderr,
            "thvm: metal_pso_cache -- serializeToURL %s failed (%s); not caching\n",
            tmppath,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    (void)unlink(tmppath);
    return;
  }
  // Atomic publish: rename(2) is atomic on the same filesystem.
  if (rename(tmppath, path) != 0) {
    fprintf(stderr,
            "thvm: metal_pso_cache -- rename %s -> %s failed (%s); not caching\n",
            tmppath, path, strerror(errno));
    (void)unlink(tmppath);
    return;
  }
  struct stat st;
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    METAL_JIT_BUILD_DISK_BYTES_W += (u64)st.st_size;
  }
}

// External accessors for the PSO cache stats / control.  Used by
// the new test_metal_pso_cache.c test; safe to call from a process
// with no Metal device (returns 0).
u64 thvm_metal_pso_cache_disk_hits(void)    { return METAL_JIT_BUILD_DISK_HITS; }
u64 thvm_metal_pso_cache_disk_misses(void)  { return METAL_JIT_BUILD_DISK_MISSES; }
u64 thvm_metal_pso_cache_bytes_r(void)      { return METAL_JIT_BUILD_DISK_BYTES_R; }
u64 thvm_metal_pso_cache_bytes_w(void)      { return METAL_JIT_BUILD_DISK_BYTES_W; }
u64 thvm_metal_jit_hits(void)               { return METAL_JIT_BUILD_HITS; }
u64 thvm_metal_jit_misses(void)             { return METAL_JIT_BUILD_MISSES; }
int thvm_metal_pso_cache_enabled(void)      { return METAL_PSO_CACHE_ENABLED; }
char const *thvm_metal_pso_cache_dir(void)  { return METAL_PSO_CACHE_DIR; }

// Drop just the in-memory PSO cache (simulating a fresh process for
// the test); leaves on-disk files intact.  Public so test code can
// drive a synthetic warm-restart without metal_shutdown nilling the
// device.
void thvm_metal_jit_drop_in_memory_psos(void) {
  metal_jit_cache_reset_impl();
}

// Open-addressing probe: returns the slot the key lives in, or the
// first empty slot the key could be installed into.  NULL only on
// table-full (which we treat as cache-bypass below).
static u32 metal_jit_lookup_idx(u64 key) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < METAL_JIT_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (METAL_JIT_CACHE_CAP - 1);
    if (METAL_JIT_CACHE[i].key == key) return i;
    if (METAL_JIT_CACHE[i].key == 0)   return i;
  }
  return (u32)-1;
}

static u32 metal_tendesc_strided_index(TenDesc const *t, u32 flat_idx);

static int metal_try_alias_reshape(KernelEntry *ke,
                                   u32 *in_buf_ids,
                                   u32 out_buf_id) {
  if (ke == NULL || ke->n_ops != 1 || ke->output_tid == 0
      || ke->output_tid >= TENS_NEXT) {
    return 0;
  }
  KProgOp const *p = &ke->program[0];
  if (p->opcode != UOP_RESHAPE || p->n_src != 1
      || !KSRC_IS_INPUT(p->src[0])) {
    return 0;
  }
  u32 slot = KSRC_INDEX(p->src[0]);
  if (slot >= ke->n_inputs || p->numel != ke->output_numel
      || ke->input_numels[slot] != ke->output_numel) {
    return 0;
  }
  u32 in_buf_id = in_buf_ids[slot];
  if (in_buf_id == 0 || in_buf_id >= METAL_BUFS_NEXT
      || METAL_BUFS[in_buf_id].buf == nil) {
    return 0;
  }
  u32 cur_out_buf_id = TENS[ke->output_tid].buf_id;
  if (cur_out_buf_id == in_buf_id && METAL_BUFS[in_buf_id].refcount > 0) {
    return 1;
  }
  if (cur_out_buf_id == 0 || cur_out_buf_id >= METAL_BUFS_NEXT
      || METAL_BUFS[cur_out_buf_id].buf == nil) {
    return 0;
  }
  (void)out_buf_id;
  metal_buf_incref(in_buf_id);
  TENS[ke->output_tid].buf_id = in_buf_id;
  if (!metal_buf_freelist_push_impl(cur_out_buf_id)) {
    metal_buf_free(cur_out_buf_id);
  }
  return 1;
}

static int metal_tile_enabled(void) {
  // Default ON: render_uop is the primary Metal MSL emit path. Set
  // THVM_TILE=0 to opt back into the legacy KProgOp-flat shader and
  // per-op interpreter fall-through (kept around as a regression
  // bisection knob until F2 deletes them).
  char const *e = getenv("THVM_TILE");
  return e == NULL || e[0] != '0';
}

static u32 metal_view_strided_index(View const *v, u32 flat_idx) {
  if (v->contiguous) {
    return flat_idx + (u32)v->offset;
  }
  int64_t acc = v->offset;
  u32 rem = flat_idx;
  for (i32 axis = (i32)v->shape.ndim - 1; axis >= 0; axis--) {
    u32 dim = v->shape.dims[axis];
    if (dim == 0) {
      continue;
    }
    u32 c = rem % dim;
    rem /= dim;
    acc += (int64_t)c * (int64_t)v->strides[axis];
  }
  return (u32)acc;
}

static u32 metal_tendesc_strided_index(TenDesc const *t, u32 flat_idx) {
  u32 idx = metal_view_strided_index(&t->view, flat_idx);
  for (i32 i = (i32)t->nviews - 1; i >= 0; i--) {
    idx = metal_view_strided_index(&t->prior_views[i], idx);
  }
  return idx;
}

static u64 metal_tile_jit_hash(KernelEntry const *ke) {
  u64 h = 0xcbf29ce484222325ULL ^ 0x4D54494C45554F50ULL;
  // kvar wedge: if any S_RANGE in the arena is variable-bound, the
  // emitted MSL uses `V_<name>` for that extent and the per-dispatch
  // numel comes through setBytes:; the kernel's input_numels[] /
  // output_numel CHANGE per BS but the MSL string does not.  We must
  // therefore exclude those numels from the hash whenever the kernel
  // is symbolic-shape -- otherwise BS=4 and BS=32 hash to different
  // slots and we re-compile for every BS.
  //
  // Two further exclusions for symbolic kernels:
  //   - tile_uops[]: in production this stays NULL through dispatch
  //     because tile_sync_from_scalar is a no-op stub since the
  //     scalar-arena seeder was deleted.  The MSL source comes from
  //     render_uop over cached_lift.store_root, not tile_uops, so
  //     excluding tile_uops here matches the actual signal flow.
  //   - input/output numels (below).
  //
  // The UOp DAG (cached_lift.store_root) Term hash captures S_RANGE
  // var_ids via kvar_collect_from_dag, so symbolic kernels at
  // different BS values still share the same UOp identity.
  u32 used_vars[KVAR_USED_CAP];
  u32 n_vars = kvar_collect_from_dag(ke->cached_lift.store_root,
                                     used_vars, KVAR_USED_CAP);
  int is_symbolic = (n_vars > 0);
  if (ke->cached_lift.store_root != 0) {
    u64 root_bits = (u64)ke->cached_lift.store_root;
    h ^= root_bits; h *= 0x100000001b3ULL;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h ^= (u64)ke->input_dtypes[i]; h *= 0x100000001b3ULL;
    if (!is_symbolic) {
      h ^= (u64)ke->input_numels[i]; h *= 0x100000001b3ULL;
    }
  }
  h ^= (u64)ke->n_inputs;          h *= 0x100000001b3ULL;
  h ^= (u64)ke->output_dtype;      h *= 0x100000001b3ULL;
  if (!is_symbolic) {
    h ^= (u64)ke->output_numel;    h *= 0x100000001b3ULL;
  }
  // Mix the per-var ids so a kernel that uses kvar "BS" hashes
  // distinctly from one that uses kvar "SEQ" with the same axis_type.
  // The var id token is stable across BS values, so this preserves
  // the cross-BS sharing we want.
  for (u32 i = 0; i < n_vars; i++) {
    h ^= (u64)(KVAR_FLAG | used_vars[i]); h *= 0x100000001b3ULL;
  }
  {
    u32 n_app = tile_anno_applied_opts_count(ke);
    h ^= (u64)n_app; h *= 0x100000001b3ULL;
    KOpt const *opts_arr = tile_anno_applied_opts(ke);
    u8   const *opts_b   = (u8 const *)opts_arr;
    size_t total = (size_t)n_app * sizeof(KOpt);
    for (size_t i = 0; opts_arr != NULL && i < total; i++) {
      h ^= (u64)opts_b[i]; h *= 0x100000001b3ULL;
    }
  }
  return h | (1ULL << 62);
}

static id<MTLComputePipelineState> metal_tile_jit_build(KernelEntry const *ke,
                                                        u64 key) {
  char *src = cg_emit_tile_metal(ke);
  if (src == NULL) return nil;
  NSString *srcStr = [NSString stringWithUTF8String:src];
  free(src);
  // THVM_DUMP_TILE_JIT_SRC=2 (or "all"): dump the generated MSL for
  // every tile-jit'd kernel up front, tagged with kid -- lets you see
  // whether conv matmuls picked the simdgroup_matrix template or fell
  // back to the scalar accumulator.  THVM_DUMP_TILE_JIT_SRC=1 dumps
  // only on compile failure (below).
  {
    char const *d = getenv("THVM_DUMP_TILE_JIT_SRC");
    if (d != NULL && (d[0] == '2' || d[0] == 'a')) {
      fprintf(stderr, "---- tile-jit src kid=%u ----\n%s\n----\n",
              (unsigned)(ke - KERNELS), [srcStr UTF8String]);
    }
  }
  u64 t0 = cg_now_us();
  NSError *err = nil;
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithSource:srcStr
                                                  options:nil
                                                    error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_tile_jit -- compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    if (getenv("THVM_DUMP_TILE_JIT_SRC")) {
      fprintf(stderr, "---- failing source ----\n%s\n----\n",
              [srcStr UTF8String]);
    }
    return nil;
  }
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"k"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm: metal_tile_jit -- function 'k' missing in compiled lib\n");
    return nil;
  }
  MTLComputePipelineDescriptor *desc =
      [[MTLComputePipelineDescriptor alloc] init];
  [desc setComputeFunction:mtlFn];
  [desc setSupportIndirectCommandBuffers:YES];
  // Try the persistent on-disk PSO cache first.  The descriptor's
  // computeFunction must already be set; metal_pso_cache_try_load
  // attaches the archive + sets FailOnBinaryArchiveMiss so a hit
  // skips the AIR->GPU backend compile.  Returns nil on miss; in
  // that case fall through to the fresh-build path below.
  id<MTLComputePipelineState> pso =
      metal_pso_cache_try_load(key, mtlFn, desc);
  int from_disk = 0;
  if (pso != nil) {
    from_disk = 1;
    METAL_JIT_BUILD_DISK_HITS++;
  } else {
    if (METAL_PSO_CACHE_ENABLED) {
      METAL_JIT_BUILD_DISK_MISSES++;
    }
    err = nil;
    pso = [METAL_DEVICE newComputePipelineStateWithDescriptor:desc
                                                       options:MTLPipelineOptionNone
                                                    reflection:NULL
                                                         error:&err];
    if (pso == nil) {
      fprintf(stderr, "thvm: metal_tile_jit -- pipeline-state failed: %s\n",
              err ? [[err localizedDescription] UTF8String] : "(no error)");
      return nil;
    }
    // Capture the freshly-built PSO into the on-disk archive for
    // future warm restarts.  Non-fatal on failure -- the in-memory
    // PSO above is still usable for this process.
    metal_pso_cache_store(key, desc);
  }
  (void)from_disk;
  METAL_JIT_BUILD_COMPILE_US += cg_now_us() - t0;
  u32 idx = metal_jit_lookup_idx(key);
  if (idx != (u32)-1) {
    METAL_JIT_CACHE[idx].key = key;
    METAL_JIT_PSOS [idx]     = pso;
    METAL_JIT_BUILD_MISSES++;
  } else {
    METAL_JIT_BUILD_BYPASS++;
  }
  return pso;
}

static id<MTLComputePipelineState> metal_tile_jit_pipeline(KernelEntry *ke);

#define METAL_CONV_CFG_INTS 19

static int metal_tile_jit_uses_conv_cfg(KernelEntry const *ke,
                                        TileConv2DInfo *out) {
  if (metal_kernel_has_applied_opt(ke, KOP_GROUP)
      || metal_kernel_has_applied_opt(ke, KOP_GROUPTOP)) {
    return 0;
  }
  if (!tile_analyze_conv2d_flat(ke, out)) {
    return 0;
  }
  return out->threads > 0 && out->threads <= 256;
}

static void metal_conv_cfg_fill(TileConv2DInfo const *conv,
                                int cfg[METAL_CONV_CFG_INTS]) {
  cfg[0]  = (int)conv->c_out;
  cfg[1]  = (int)conv->c_in;
  cfg[2]  = (int)conv->h;
  cfg[3]  = (int)conv->w;
  cfg[4]  = (int)conv->kh;
  cfg[5]  = (int)conv->kw;
  cfg[6]  = (int)conv->h_out;
  cfg[7]  = (int)conv->w_out;
  cfg[8]  = (int)conv->patches;
  cfg[9]  = (int)conv->batch;
  cfg[10] = (int)conv->spatial_patches;
  cfg[11] = conv->w_offset;
  cfg[12] = conv->w_stride0;
  cfg[13] = conv->w_stride1;
  cfg[14] = conv->x_offset;
  cfg[15] = conv->x_stride_b;
  cfg[16] = conv->x_stride0;
  cfg[17] = conv->x_stride1;
  cfg[18] = conv->x_stride2;
}

static int metal_tile_jit_encode(KernelEntry *ke,
                                 __unsafe_unretained id<MTLBuffer> *src_bufs,
                                 id<MTLBuffer> outBuf,
                                 id<MTLCommandBuffer> cmd,
                                 u32 groups_x,
                                 u32 threads_x) {
  if (groups_x == 0 || threads_x == 0) return 0;
  // Multi-output kernels need N output buffers bound to indices
  // 0..N-1; the tile encoder binds a single outBuf at index 0.
  // Bail until step 4+ wires the multi-output dispatch.
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  id<MTLComputePipelineState> pso = metal_tile_jit_pipeline(ke);
  if (pso == nil) return 0;
  if ((NSUInteger)threads_x > [pso maxTotalThreadsPerThreadgroup]) {
    return 0;
  }

  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:outBuf offset:0 atIndex:0];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    [enc setBuffer:src_bufs[i] offset:0 atIndex:(1 + i)];
  }
  TileConv2DInfo conv;
  int is_conv = metal_tile_jit_uses_conv_cfg(ke, &conv);
  if (is_conv) {
    int cfg[METAL_CONV_CFG_INTS];
    metal_conv_cfg_fill(&conv, cfg);
    [enc setBytes:cfg length:sizeof(cfg) atIndex:(1 + ke->n_inputs)];
  }
  // kvar wedge: walk the kernel's scalar arena for any RANGE leaf
  // whose extent is variable-bound, then setBytes: each var's
  // per-dispatch runtime value as a `constant uint` kernel arg.
  // Buffer indices land after inputs (+ optional conv cfg), in
  // stable ascending-var-id order so the renderer's signature stays
  // in lockstep.  Missing bindings fall back to kvar_hi(id) (worst-
  // case static upper bound).
  {
    u32 used_vars[KVAR_USED_CAP];
    u32 n_vars = kvar_collect_from_dag(ke->cached_lift.store_root,
                                       used_vars, KVAR_USED_CAP);
    u32 base = 1 + ke->n_inputs + (is_conv ? 1u : 0u);
    for (u32 i = 0; i < n_vars; i++) {
      u32 v = kernel_kvar_value(ke, used_vars[i]);
      [enc setBytes:&v length:sizeof(v) atIndex:(base + i)];
    }
  }
  if (is_conv) {
    NSUInteger outputs = (NSUInteger)(conv.outputs_per_thread
                                      ? conv.outputs_per_thread : 1);
    NSUInteger total = (NSUInteger)(conv.c_out * conv.patches);
    NSUInteger threads_total = (total + outputs - 1) / outputs;
    [enc dispatchThreads:MTLSizeMake(threads_total, 1, 1)
     threadsPerThreadgroup:MTLSizeMake((NSUInteger)threads_x, 1, 1)];
  } else {
    [enc dispatchThreadgroups:MTLSizeMake((NSUInteger)groups_x, 1, 1)
        threadsPerThreadgroup:MTLSizeMake((NSUInteger)threads_x, 1, 1)];
  }
  [enc endEncoding];
  return 1;
}

static id<MTLComputePipelineState> metal_tile_jit_pipeline(KernelEntry *ke) {
  u64 key = metal_tile_jit_hash(ke);
  u32 idx = metal_jit_lookup_idx(key);
  if (idx != (u32)-1 && METAL_JIT_CACHE[idx].key == key) {
    METAL_JIT_BUILD_HITS++;
    return METAL_JIT_PSOS[idx];
  }
  return metal_tile_jit_build(ke, key);
}

// Test hook: expose the tile-jit PSO cache key for a kernel so tests
// can assert "two kernels with the same UOp shape but different kvar
// runtime values map to the same PSO slot".  Pure -- no Metal device
// required.
u64 thvm_metal_tile_jit_hash(KernelEntry const *ke) {
  return metal_tile_jit_hash(ke);
}

#define METAL_GRAPH_CACHE_CAP 256
#define METAL_GRAPH_MAX_RESOURCES 8192
static u64 METAL_GRAPH_KEYS[METAL_GRAPH_CACHE_CAP];
static id<MTLIndirectCommandBuffer> METAL_GRAPH_ICBS[METAL_GRAPH_CACHE_CAP];
static id<MTLBuffer> METAL_GRAPH_CFG_BUFS[METAL_GRAPH_CACHE_CAP];

static void metal_graph_cache_reset_impl(void) {
  for (u32 i = 0; i < METAL_GRAPH_CACHE_CAP; i++) {
    METAL_GRAPH_KEYS[i] = 0;
    METAL_GRAPH_ICBS[i] = nil;
    METAL_GRAPH_CFG_BUFS[i] = nil;
  }
}

static int metal_graph_trace_level(void) {
  char const *e = getenv("THVM_METAL_GRAPH_TRACE");
  if (e == NULL || e[0] == '\0') {
    return 0;
  }
  return atoi(e);
}

static u64 metal_graph_hash(u32 slot, u32 start_op,
                            JitReplayDispatch const *ops, u32 n_ops) {
  u64 h = 0xcbf29ce484222325ULL ^ 0x4D47524150484943ULL;
  h ^= (u64)slot;     h *= 0x100000001b3ULL;
  h ^= (u64)start_op; h *= 0x100000001b3ULL;
  h ^= (u64)n_ops;    h *= 0x100000001b3ULL;
  for (u32 i = 0; i < n_ops; i++) {
    JitReplayDispatch const *r = &ops[i];
    if (r->kid == 0 || r->kid >= KERNELS_NEXT) return 0;
    u32 dispatch_kind = cg_kernel_dispatch_kind(r->kid);
    h ^= (u64)r->kid;        h *= 0x100000001b3ULL;
    h ^= (u64)dispatch_kind; h *= 0x100000001b3ULL;
    h ^= (u64)r->n_inputs;   h *= 0x100000001b3ULL;
    h ^= (u64)r->out_buf_id; h *= 0x100000001b3ULL;
    if (r->out_buf_id == 0 || r->out_buf_id >= METAL_BUFS_NEXT) return 0;
    id<MTLBuffer> out = METAL_BUFS[r->out_buf_id].buf;
    if (out == nil) return 0;
    h ^= (u64)(uintptr_t)(__bridge void *)out; h *= 0x100000001b3ULL;
    for (u32 j = 0; j < r->n_inputs; j++) {
      u32 bid = r->in_buf_ids[j];
      if (bid == 0 || bid >= METAL_BUFS_NEXT) return 0;
      id<MTLBuffer> in = METAL_BUFS[bid].buf;
      if (in == nil) return 0;
      h ^= (u64)bid; h *= 0x100000001b3ULL;
      h ^= (u64)(uintptr_t)(__bridge void *)in; h *= 0x100000001b3ULL;
    }
    TileConv2DInfo conv;
    if (dispatch_kind == KDISPATCH_METAL_TILE
        && metal_tile_jit_uses_conv_cfg(&KERNELS[r->kid], &conv)) {
      int cfg[METAL_CONV_CFG_INTS];
      metal_conv_cfg_fill(&conv, cfg);
      for (u32 j = 0; j < METAL_CONV_CFG_INTS; j++) {
        h ^= (u64)(u32)cfg[j];
        h *= 0x100000001b3ULL;
      }
    }
  }
  return h | (1ULL << 61);
}

static u32 metal_graph_lookup_idx(u64 key) {
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < METAL_GRAPH_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (METAL_GRAPH_CACHE_CAP - 1);
    if (METAL_GRAPH_KEYS[i] == key) return i;
    if (METAL_GRAPH_KEYS[i] == 0) return i;
  }
  return (u32)-1;
}

static int metal_graph_collect_resources(JitReplayDispatch const *ops,
                                         u32 n_ops,
                                         __unsafe_unretained id<MTLResource> *out,
                                         u32 *out_len) {
  int trace = metal_graph_trace_level();
  u32 n = 0;
  for (u32 i = 0; i < n_ops; i++) {
    JitReplayDispatch const *r = &ops[i];
    if (r->out_buf_id == 0 || r->out_buf_id >= METAL_BUFS_NEXT) {
      if (trace) fprintf(stderr, "thvm: metal_graph resource bad out id op=%u\n", i);
      return 0;
    }
    if (METAL_BUFS[r->out_buf_id].buf == nil) {
      if (trace) fprintf(stderr, "thvm: metal_graph resource nil out op=%u bid=%u\n",
                         i, r->out_buf_id);
      return 0;
    }
    if (n >= METAL_GRAPH_MAX_RESOURCES) {
      if (trace) fprintf(stderr, "thvm: metal_graph resource cap out op=%u\n", i);
      return 0;
    }
    out[n++] = METAL_BUFS[r->out_buf_id].buf;
    for (u32 j = 0; j < r->n_inputs; j++) {
      u32 bid = r->in_buf_ids[j];
      if (bid == 0 || bid >= METAL_BUFS_NEXT) {
        if (trace) fprintf(stderr, "thvm: metal_graph resource bad in op=%u input=%u\n",
                           i, j);
        return 0;
      }
      if (METAL_BUFS[bid].buf == nil) {
        if (trace) fprintf(stderr, "thvm: metal_graph resource nil in op=%u input=%u bid=%u\n",
                           i, j, bid);
        return 0;
      }
      if (n >= METAL_GRAPH_MAX_RESOURCES) {
        if (trace) fprintf(stderr, "thvm: metal_graph resource cap in op=%u input=%u\n",
                           i, j);
        return 0;
      }
      out[n++] = METAL_BUFS[bid].buf;
    }
  }
  *out_len = n;
  return 1;
}

static u32 metal_graph_conv_cfg_count(JitReplayDispatch const *ops,
                                      u32 n_ops) {
  u32 n = 0;
  for (u32 i = 0; i < n_ops; i++) {
    JitReplayDispatch const *r = &ops[i];
    if (r->kid == 0 || r->kid >= KERNELS_NEXT) {
      return 0;
    }
    if (cg_kernel_dispatch_kind(r->kid) != KDISPATCH_METAL_TILE) {
      continue;
    }
    TileConv2DInfo conv;
    if (metal_tile_jit_uses_conv_cfg(&KERNELS[r->kid], &conv)) {
      n++;
    }
  }
  return n;
}

int thvm_metal_jit_replay_dispatch_ready(JitReplayDispatch const *r) {
  if (METAL_DEVICE == nil || r == NULL) {
    return 0;
  }
  if (r->kid == 0 || r->kid >= KERNELS_NEXT) {
    return 0;
  }
  KernelEntry *ke = &KERNELS[r->kid];
  if (ke->n_inputs != r->n_inputs || r->n_inputs > 30) {
    return 0;
  }
  u32 dispatch_kind = cg_kernel_dispatch_kind(r->kid);
  if (dispatch_kind != KDISPATCH_METAL_TILE) {
    return 0;
  }
  TileConv2DInfo conv;
  if (r->n_inputs >= 30 && metal_tile_jit_uses_conv_cfg(ke, &conv)) {
    return 0;
  }
  if (r->out_buf_id == 0 || r->out_buf_id >= METAL_BUFS_NEXT) {
    return 0;
  }
  if (METAL_BUFS[r->out_buf_id].buf == nil) {
    return 0;
  }
  for (u32 i = 0; i < r->n_inputs; i++) {
    u32 bid = r->in_buf_ids[i];
    if (bid == 0 || bid >= METAL_BUFS_NEXT) {
      return 0;
    }
    if (METAL_BUFS[bid].buf == nil) {
      return 0;
    }
  }
  return 1;
}

static id<MTLIndirectCommandBuffer> metal_graph_build(
    JitReplayDispatch const *ops, u32 n_ops,
    id<MTLBuffer> *cfg_buf_out) {
  int trace = metal_graph_trace_level();
  if (cfg_buf_out != NULL) {
    *cfg_buf_out = nil;
  }
  if (trace > 1) {
    fprintf(stderr, "thvm: metal_graph build begin n=%u\n", n_ops);
  }
  u32 conv_cfg_count = metal_graph_conv_cfg_count(ops, n_ops);
  id<MTLBuffer> cfgBuf = nil;
  int *cfgs = NULL;
  if (conv_cfg_count != 0) {
    cfgBuf = [METAL_DEVICE newBufferWithLength:
        (NSUInteger)conv_cfg_count * METAL_CONV_CFG_INTS * sizeof(int)
                                     options:MTLResourceStorageModeShared];
    if (cfgBuf == nil) {
      return nil;
    }
    cfgs = (int *)[cfgBuf contents];
  }

  MTLIndirectCommandBufferDescriptor *desc =
      [[MTLIndirectCommandBufferDescriptor alloc] init];
  [desc setCommandTypes:MTLIndirectCommandTypeConcurrentDispatch];
  [desc setInheritBuffers:NO];
  [desc setInheritPipelineState:NO];
  [desc setMaxKernelBufferBindCount:31];
  id<MTLIndirectCommandBuffer> icb =
      [METAL_DEVICE newIndirectCommandBufferWithDescriptor:desc
                                            maxCommandCount:(NSUInteger)n_ops
                                                    options:MTLResourceCPUCacheModeDefaultCache];
  if (icb == nil) {
    return nil;
  }
  if (trace > 1) {
    fprintf(stderr, "thvm: metal_graph icb allocated\n");
  }
  [icb resetWithRange:NSMakeRange(0, (NSUInteger)n_ops)];

  u32 conv_cfg_i = 0;
  for (u32 i = 0; i < n_ops; i++) {
    JitReplayDispatch const *r = &ops[i];
    if (trace > 1) {
      fprintf(stderr, "thvm: metal_graph command %u kid=%u inputs=%u\n",
              i, r->kid, r->n_inputs);
    }
    if (r->kid == 0 || r->kid >= KERNELS_NEXT) return nil;
    KernelEntry *ke = &KERNELS[r->kid];
    if (ke->n_inputs != r->n_inputs || r->n_inputs > 30) return nil;
    u32 dispatch_kind = cg_kernel_dispatch_kind(r->kid);
    if (dispatch_kind != KDISPATCH_METAL_TILE) return nil;
    u32 groups_x = 0;
    u32 threads_x = 0;
    id<MTLComputePipelineState> pso = nil;
    TileConv2DInfo conv;
    int needs_cfg = 0;
    if (!cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x)) return nil;
    needs_cfg = metal_tile_jit_uses_conv_cfg(ke, &conv);
    if (needs_cfg && ke->n_inputs >= 30) return nil;
    pso = metal_tile_jit_pipeline(ke);
    if (pso == nil) return nil;
    if ((NSUInteger)threads_x > [pso maxTotalThreadsPerThreadgroup]) return nil;
    if (r->out_buf_id == 0 || r->out_buf_id >= METAL_BUFS_NEXT) return nil;
    id<MTLBuffer> outBuf = METAL_BUFS[r->out_buf_id].buf;
    if (outBuf == nil) return nil;

    id<MTLIndirectComputeCommand> cmd = [icb indirectComputeCommandAtIndex:i];
    if (trace > 1) {
      fprintf(stderr, "thvm: metal_graph command %u set pso\n", i);
    }
    [cmd setComputePipelineState:pso];
    if (trace > 1) {
      fprintf(stderr, "thvm: metal_graph command %u set out\n", i);
    }
    [cmd setKernelBuffer:outBuf offset:0 atIndex:0];
    for (u32 j = 0; j < r->n_inputs; j++) {
      u32 bid = r->in_buf_ids[j];
      if (bid == 0 || bid >= METAL_BUFS_NEXT) return nil;
      id<MTLBuffer> inBuf = METAL_BUFS[bid].buf;
      if (inBuf == nil) return nil;
      if (trace > 1) {
        fprintf(stderr, "thvm: metal_graph command %u set in %u\n", i, j);
      }
      [cmd setKernelBuffer:inBuf offset:0 atIndex:(1 + j)];
    }
    if (needs_cfg) {
      if (cfgs == NULL || conv_cfg_i >= conv_cfg_count) return nil;
      u32 cfg_offset_ints = conv_cfg_i * METAL_CONV_CFG_INTS;
      metal_conv_cfg_fill(&conv, &cfgs[cfg_offset_ints]);
      [cmd setKernelBuffer:cfgBuf
                    offset:(NSUInteger)cfg_offset_ints * sizeof(int)
                   atIndex:(1 + ke->n_inputs)];
      conv_cfg_i++;
    }
    if (trace > 1) {
      fprintf(stderr, "thvm: metal_graph command %u dispatch\n", i);
    }
    [cmd concurrentDispatchThreadgroups:MTLSizeMake((NSUInteger)groups_x, 1, 1)
                  threadsPerThreadgroup:MTLSizeMake((NSUInteger)threads_x, 1, 1)];
    if (trace > 1) {
      fprintf(stderr, "thvm: metal_graph command %u barrier\n", i);
    }
    [cmd setBarrier];
    if (trace > 1) {
      fprintf(stderr, "thvm: metal_graph command %u done\n", i);
    }
  }
  if (cfg_buf_out != NULL) {
    *cfg_buf_out = cfgBuf;
  }
  return icb;
}

int thvm_metal_jit_replay_run(u32 slot, u32 start_op,
                              JitReplayDispatch const *ops, u32 n_ops) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) return -1;
  if (ops == NULL || n_ops < 2 || n_ops > 256) return -1;
  int trace = metal_graph_trace_level();
  if (trace) {
    fprintf(stderr, "thvm: metal_graph replay start slot=%u op=%u n=%u\n",
            slot, start_op, n_ops);
  }
  __unsafe_unretained id<MTLResource> resources[METAL_GRAPH_MAX_RESOURCES];
  u32 resource_count = 0;
  if (!metal_graph_collect_resources(ops, n_ops, resources, &resource_count)) {
    return -1;
  }

  u64 key = metal_graph_hash(slot, start_op, ops, n_ops);
  if (key == 0) return -1;
  u32 idx = metal_graph_lookup_idx(key);
  if (idx == (u32)-1) return -1;
  id<MTLIndirectCommandBuffer> icb = nil;
  id<MTLBuffer> cfgBuf = nil;
  if (METAL_GRAPH_KEYS[idx] == key && METAL_GRAPH_ICBS[idx] != nil) {
    icb = METAL_GRAPH_ICBS[idx];
    cfgBuf = METAL_GRAPH_CFG_BUFS[idx];
    if (trace) {
      fprintf(stderr, "thvm: metal_graph cache hit idx=%u resources=%u\n",
              idx, resource_count);
    }
  } else {
    if (trace) {
      fprintf(stderr, "thvm: metal_graph build idx=%u resources=%u\n",
              idx, resource_count);
    }
    icb = metal_graph_build(ops, n_ops, &cfgBuf);
    if (icb == nil) return -1;
    METAL_GRAPH_KEYS[idx] = key;
    METAL_GRAPH_ICBS[idx] = icb;
    METAL_GRAPH_CFG_BUFS[idx] = cfgBuf;
  }
  if (cfgBuf != nil) {
    if (resource_count >= METAL_GRAPH_MAX_RESOURCES) return -1;
    resources[resource_count++] = cfgBuf;
  }

  // Per-op GPU-timestamp profiling.  When THVM_METAL_PROFILE_PEROP=1,
  // replace the single batched ICB execution with N per-op encoder
  // dispatches; each cmd buffer commits + waits, then we read
  // cmd.GPUEndTime - cmd.GPUStartTime for true per-kernel GPU time.
  // Costs ~5-10x more dispatch overhead than the batched path -- only
  // for explicit profile runs.  Without this, all kernels'
  // cg_profile_record entries get the same averaged value
  // (elapsed/n_ops), which is misleading for per-kernel rankings.
  char const *e_perop = getenv("THVM_METAL_PROFILE_PEROP");
  if (e_perop != NULL && e_perop[0] == '1') {
    for (u32 i = 0; i < n_ops; i++) {
      id<MTLCommandBuffer> cmd_i = [METAL_QUEUE commandBuffer];
      if (cmd_i == nil) return -1;
      id<MTLComputeCommandEncoder> enc_i = [cmd_i computeCommandEncoder];
      if (resource_count > 0) {
        [enc_i useResources:resources
                      count:(NSUInteger)resource_count
                      usage:(MTLResourceUsageRead | MTLResourceUsageWrite)];
      }
      [enc_i executeCommandsInBuffer:icb
                           withRange:NSMakeRange((NSUInteger)i, (NSUInteger)1)];
      [enc_i endEncoding];
      [cmd_i commit];
      [cmd_i waitUntilCompleted];
      double gpu_seconds = cmd_i.GPUEndTime - cmd_i.GPUStartTime;
      u64 gpu_us = gpu_seconds > 0.0 ? (u64)(gpu_seconds * 1e6) : 0;
      cg_profile_record(ops[i].kid, KDISPATCH_METAL_TILE, gpu_us);
    }
    if (trace) {
      fprintf(stderr, "thvm: metal_graph replay perop n=%u\n", n_ops);
    }
    return 0;
  }

  u64 t0 = cg_now_us();
  id<MTLCommandBuffer> cmd = metal_command_buffer();
  if (cmd == nil) return -1;
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  if (resource_count > 0) {
    [enc useResources:resources
                count:(NSUInteger)resource_count
                usage:(MTLResourceUsageRead | MTLResourceUsageWrite)];
  }
  [enc executeCommandsInBuffer:icb
                     withRange:NSMakeRange(0, (NSUInteger)n_ops)];
  [enc endEncoding];
  metal_submit_if_standalone(cmd);
  if (trace) {
    fprintf(stderr, "thvm: metal_graph replay encoded n=%u\n", n_ops);
  }
  u64 elapsed = cg_now_us() - t0;
  u64 per = n_ops == 0 ? elapsed : elapsed / n_ops;
  for (u32 i = 0; i < n_ops; i++) {
    cg_profile_record(ops[i].kid, KDISPATCH_METAL_TILE, per);
  }
  return 0;
}

void thvm_metal_gpu_time(u64 *out_total_us, u64 *out_flush_count) {
  if (out_total_us != NULL)    *out_total_us    = METAL_GPU_US_TOTAL;
  if (out_flush_count != NULL) *out_flush_count = METAL_GPU_FLUSH_COUNT;
}

// METAL_PIPELINES_CACHE is the per-(opcode, dtype) pipeline cache;
// it's defined further up so metal_shutdown can clear it.

// Map a dtype to its shader-name suffix.  Only dtypes with native MSL
// types are listed; others bail at metal_kernel_supported.
static const char *metal_dtype_suffix(uint32_t dtype) {
  switch (dtype) {
    case DT_FP32:  return "f32";
    case DT_INT32: return "i32";
    default:       return NULL;
  }
}

static id<MTLComputePipelineState> metal_pipeline_for(uint32_t opcode,
                                                      uint32_t dtype) {
  if (opcode >= UOP_COUNT || dtype >= 32) return nil;
  if (METAL_PIPELINES_CACHE[opcode][dtype] != nil) return METAL_PIPELINES_CACHE[opcode][dtype];
  if (METAL_LIB == nil) return nil;
  const char *suffix = metal_dtype_suffix(dtype);
  if (suffix == NULL) return nil;
  const char *base = NULL;
  switch (opcode) {
    case UOP_CONST:  base = "thvm_const";   break;
    case UOP_ADD:    base = "thvm_add";     break;
    case UOP_MUL:    base = "thvm_mul";     break;
    case UOP_CMPLT:  base = "thvm_cmplt";   break;
    case UOP_CMPEQ:  base = "thvm_cmpeq";   break;
    case UOP_NEG:    base = "thvm_neg";     break;
    case UOP_RECIP:  base = "thvm_recip";   break;
    case UOP_SQRT:   base = "thvm_sqrt";    break;
    case UOP_EXP2:   base = "thvm_exp2";    break;
    case UOP_LOG2:   base = "thvm_log2";    break;
    case UOP_REDUCE: base = "thvm_reduce";  break;
    case UOP_EXPAND: base = "thvm_expand";  break;
    case UOP_RESHAPE:base = "thvm_reshape"; break;
    case UOP_FLIP:   base = "thvm_flip";    break;
    case UOP_PAD:    base = "thvm_pad";     break;
    case UOP_SHRINK: base = "thvm_shrink";  break;
    case UOP_PERMUTE:base = "thvm_permute"; break;
    default:         return nil;
  }
  NSString *fnName = [NSString stringWithFormat:@"%s_%s", base, suffix];
  // `fn` is taken by thvm.h as a `static inline` macro; use `mtlFn`.
  id<MTLFunction> mtlFn = [METAL_LIB newFunctionWithName:fnName];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm: metal -- function %s not in metallib\n",
            [fnName UTF8String]);
    return nil;
  }
  NSError *err = nil;
  id<MTLComputePipelineState> pso =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (pso == nil) {
    fprintf(stderr, "thvm: metal -- pipeline-state for %s failed: %s\n",
            [fnName UTF8String],
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  METAL_PIPELINES_CACHE[opcode][dtype] = pso;
  return pso;
}

// Buffer-binding convention:
//     buffer(0)                       : output
//     buffer(1)                       : per-op constant arg (KProgOp.arg)
//     buffer(2..2+n_in-1)             : input tensor buffers
//     buffer(2+n_in..2+2*n_in-1)      : per-input numel (uint;
//                                       1 = broadcast in shader)
// Threads = ke->program[0].numel; threadgroup size capped at the
// pipeline's maxTotalThreadsPerThreadgroup.
// Encode one op into `enc`: bind out, arg, srcs, numels, plus any
// movement-op shape info.  src_bufs[s] / src_numels[s] resolve
// already via the caller; this helper just walks the binding slots.
// Returns 0 on success, -1 if the pipeline state is missing.
// __unsafe_unretained on the buffer array avoids ARC's writeback
// for inout pointer-to-id parameters (we don't write back).
static int metal_encode_op(id<MTLComputeCommandEncoder> enc,
                            KProgOp *p,
                            __unsafe_unretained id<MTLBuffer> *src_bufs,
                            u32 *src_numels,
                            id<MTLBuffer> outBuf) {
  id<MTLComputePipelineState> pso = metal_pipeline_for(p->opcode, p->dtype);
  if (pso == nil) {
    fprintf(stderr, "thvm: metal dispatch -- no pipeline for opcode %u dtype %u\n",
            p->opcode, p->dtype);
    return -1;
  }
  [enc setComputePipelineState:pso];
  [enc setBuffer:outBuf offset:0 atIndex:0];
  [enc setBytes:&p->arg length:sizeof(p->arg) atIndex:1];
  for (u32 i = 0; i < p->n_src; i++) {
    [enc setBuffer:src_bufs[i] offset:0 atIndex:(2 + i)];
  }
  for (u32 i = 0; i < p->n_src; i++) {
    u32 nm = src_numels[i];
    [enc setBytes:&nm length:sizeof(nm) atIndex:(2 + p->n_src + i)];
  }
  // Movement-op shape info: pack src0_ndim/src0_dims and out_ndim/
  // out_dims as uint arrays of length 1+MAX_DIM so the shader can
  // walk axes without re-deriving shape from numels.
  if (p->opcode == UOP_EXPAND || p->opcode == UOP_FLIP
      || p->opcode == UOP_PAD || p->opcode == UOP_PERMUTE
      || p->opcode == UOP_SHRINK) {
    u32 src0[1 + MAX_DIM] = {0};
    u32 outd[1 + MAX_DIM] = {0};
    src0[0] = p->src0_ndim;
    outd[0] = p->out_ndim;
    for (u32 i = 0; i < MAX_DIM; i++) src0[1 + i] = p->src0_dims[i];
    for (u32 i = 0; i < MAX_DIM; i++) outd[1 + i] = p->out_dims [i];
    [enc setBytes:src0 length:sizeof(src0) atIndex:(2 + 2 * p->n_src)];
    [enc setBytes:outd length:sizeof(outd) atIndex:(2 + 2 * p->n_src + 1)];
  }
  if (p->opcode == UOP_PAD || p->opcode == UOP_SHRINK) {
    u32 padw[2 * MAX_DIM] = {0};
    for (u32 i = 0; i < 2 * MAX_DIM; i++) padw[i] = p->pad_widths[i];
    [enc setBytes:padw length:sizeof(padw) atIndex:(2 + 2 * p->n_src + 2)];
  }
  if (p->opcode == UOP_PERMUTE) {
    u32 perm[MAX_DIM] = {0};
    for (u32 i = 0; i < MAX_DIM; i++) perm[i] = p->axis_perm[i];
    [enc setBytes:perm length:sizeof(perm) atIndex:(2 + 2 * p->n_src + 2)];
  }
  NSUInteger n = (NSUInteger)p->numel;
  if (n == 0) n = 1;
  NSUInteger tg = MIN(n, [pso maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  return 0;
}

// Predicate: every input + every program op is one of the dtypes
// the Metal shader library covers (f32 + i32 today; f16 + the rest
// follow as shader variants land).  Mixed-dtype or unsupported
// kernels return -1 so cpu_dispatch_kernel falls back to the CPU
// path (interpret / JIT).
//
// Phase C slice 4: when `cached_lift.store_root != 0` (materialize-
// time lift succeeded) the per-op dtype check walks the lifted UOp
// DAG via uop_dag_dtype_uniform instead of iterating ke->program[].
// Equivalent invariant -- every BUFFER / CONST / CAST-dst dtype in
// the DAG must equal `dt` -- but the read goes through cached_lift,
// keeping program[] off the hot path for kernels that lift.  Lift
// declines (multi-output spliced, n_inputs > KERNEL_LIFT_MAX_INPUT,
// gemm/conv2d shape miss) keep the legacy program[] walk.
static int metal_kernel_supported(struct KernelEntry const *ke) {
  // Phase C slice 7: when the lift succeeded use the lifted DAG to
  // decide eligibility (kernel may have program == NULL under
  // THVM_PHASE_C7_FREE_PROGRAM=1).  When the lift declined fall back
  // to the legacy program[] walk.
  u32 dt;
  if (ke->cached_lift.store_root != 0) {
    if (ke->n_inputs == 0) return 0;
    dt = ke->input_dtypes[0];
    if (dt != DT_FP32 && dt != DT_INT32) return 0;
    if (!uop_dag_dtype_uniform(ke->cached_lift.store_root, dt)) return 0;
  } else {
    if (ke->n_ops == 0) return 0;
    dt = ke->program[0].dtype;
    // v1: only the dtypes that have shader variants in the metallib.
    // f32 always; i32 added in Phase I.  f16 / bf16 / fp8 / int4 fall
    // back to CPU until per-dtype shader variants land for them.
    if (dt != DT_FP32 && dt != DT_INT32) return 0;
    for (u32 i = 0; i < ke->n_ops; i++)
      if (ke->program[i].dtype != dt) return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_dtypes[i] != dt) return 0;
  return 1;
}

static int metal_kernel_has_applied_opt(struct KernelEntry const *ke, u8 op) {
  u32 n_app = tile_anno_applied_opts_count(ke);
  KOpt const *opts = tile_anno_applied_opts(ke);
  for (u32 i = 0; i < n_app; i++) {
    if (opts[i].op == op) {
      return 1;
    }
  }
  return 0;
}

// === Phase C slice 5: DAG-side per-op encoder ========================
//
// Mirrors the per-KProgOp encoder loop below, but walks
// ke->cached_lift.store_root (a UOP_STORE / UOP_AFTER chain produced
// by kernel_lift_to_uop) instead of ke->program[].  Fires when the
// lift succeeded (cached_lift.store_root != 0) AND metal_tile_jit
// _encode declined (today only multi-output kernels that bail at
// cg_kernel_has_extra_outputs reach this path with a non-zero
// store_root; lift declines keep the legacy program[] loop).
//
// Mapping is mechanical: KProgOp.opcode is already a UOP_* tag, so
// metal_pipeline_for(opcode, dtype) -- the table that resolves a
// (UOP_*, dtype) pair to an MTLComputePipelineState -- reuses 1:1.
// The shapes the lifter produces for multi-output kernels are
// elementwise + UOP_CONST + UOP_INDEX_E(input_buf, range_addr) only
// (kernel_lift_from_kprog rejects anything else).  REDUCE / movement
// ops never appear in lift-eligible multi-output programs because
// merge_boundary_is_elementwise filters them out at splice time.
//
// One MTLComputeCommandEncoder per UOp tree node so Metal hazard-
// tracks reads/writes across encoders -- same convention the legacy
// per-op loop relies on.

#define DAG_ENCODE_MAX_VISITED 256u
#define DAG_ENCODE_MAX_INTERMS  64u

typedef struct {
  Term term;     // UOp Term that produced this buffer's contents (0 = unused)
  u32  buf_id;   // METAL_BUFS slot holding the result
  u8   owned;    // 1 = we allocated it (intermediate); 0 = borrowed input/output
} DagEncCacheEntry;

typedef struct {
  KernelEntry        *ke;
  u32                *effective_buf_ids;   // input slot -> Metal buf id
  u32                 numel;               // elementwise per-thread numel
  u32                 dtype;               // uniform dtype (already gated)
  id<MTLCommandBuffer> cmd;
  // Memo: maps a UOp Term to the Metal buf_id holding its result.
  // Linear scan is fine for the tree sizes we see (<32 nodes per
  // multi-output kernel).
  DagEncCacheEntry    cache[DAG_ENCODE_MAX_VISITED];
  u32                 n_cache;
  // Allocated intermediates we need to release after dispatch submits.
  u32                 inter_buf_ids[DAG_ENCODE_MAX_INTERMS];
  u32                 n_inter;
} DagEncCtx;

static u32 dag_enc_lookup(DagEncCtx const *c, Term t) {
  for (u32 i = 0; i < c->n_cache; i++) {
    if (c->cache[i].term == t) return c->cache[i].buf_id;
  }
  return 0;
}

static int dag_enc_remember(DagEncCtx *c, Term t, u32 buf_id, u8 owned) {
  if (c->n_cache >= DAG_ENCODE_MAX_VISITED) return 0;
  c->cache[c->n_cache].term   = t;
  c->cache[c->n_cache].buf_id = buf_id;
  c->cache[c->n_cache].owned  = owned;
  c->n_cache++;
  return 1;
}

static u32 dag_enc_alloc_inter(DagEncCtx *c) {
  if (c->n_inter >= DAG_ENCODE_MAX_INTERMS) return 0;
  u32 nbytes = (u32)dtype_storage_bytes(c->dtype, c->numel);
  u32 bid    = metal_buf_alloc(nbytes);
  if (bid == 0) return 0;
  c->inter_buf_ids[c->n_inter++] = bid;
  return bid;
}

// Forward decl so encode_value can recurse through unary/binary.
static u32 dag_enc_value(DagEncCtx *c, Term v, u32 dst_buf_id);

// Encode a CONST node into `dst_buf_id`.  When dst_buf_id == 0 we
// allocate an intermediate.  Returns the Metal buf id holding the
// constant's broadcast fill, or 0 on failure.
static u32 dag_enc_const(DagEncCtx *c, Term v, u32 dst_buf_id) {
  u32 dtype = 0, bits = 0;
  if (!uop_dag_const_payload(v, &dtype, &bits)) return 0;
  if (dtype != c->dtype) return 0;        // metal_kernel_supported gated this
  u32 bid = dst_buf_id ? dst_buf_id : dag_enc_alloc_inter(c);
  if (bid == 0) return 0;
  id<MTLComputePipelineState> pso = metal_pipeline_for(UOP_CONST, dtype);
  if (pso == nil) return 0;
  id<MTLComputeCommandEncoder> enc = [c->cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:METAL_BUFS[bid].buf offset:0 atIndex:0];
  [enc setBytes:&bits length:sizeof(bits) atIndex:1];
  NSUInteger n = c->numel ? c->numel : 1;
  NSUInteger tg = MIN(n, [pso maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  return bid;
}

// Encode a unary or binary elementwise op whose `arg` field is 0 (the
// lifted multi-output kernels never carry a non-zero arg today; if we
// ever hit one, the legacy per-op loop's encoding takes over).
static u32 dag_enc_arith(DagEncCtx *c, Term v, u32 dst_buf_id, u32 op,
                         u32 n_src, u32 src_bids[2]) {
  u32 bid = dst_buf_id ? dst_buf_id : dag_enc_alloc_inter(c);
  if (bid == 0) return 0;
  id<MTLComputePipelineState> pso = metal_pipeline_for(op, c->dtype);
  if (pso == nil) return 0;
  id<MTLComputeCommandEncoder> enc = [c->cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:METAL_BUFS[bid].buf offset:0 atIndex:0];
  u32 zero_arg = 0;
  [enc setBytes:&zero_arg length:sizeof(zero_arg) atIndex:1];
  for (u32 i = 0; i < n_src; i++) {
    [enc setBuffer:METAL_BUFS[src_bids[i]].buf offset:0 atIndex:(2 + i)];
  }
  // src_numels mirror the per-op encoder's contract: numel is `c->numel`
  // for non-broadcast srcs (multi-output lift produces numel-uniform
  // INDEX_E reads + numel-uniform CONST broadcasts).
  for (u32 i = 0; i < n_src; i++) {
    u32 nm = c->numel;
    [enc setBytes:&nm length:sizeof(nm) atIndex:(2 + n_src + i)];
  }
  NSUInteger n = c->numel ? c->numel : 1;
  NSUInteger tg = MIN(n, [pso maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  (void)v;
  return bid;
}

// Recursive value emitter.  Returns the Metal buf id holding the
// computed value, or 0 on failure.  When dst_buf_id != 0 we MUST
// write into that exact buffer (used for STORE root values that go
// directly into the output buffer); otherwise we allocate or reuse
// an intermediate.
static u32 dag_enc_value(DagEncCtx *c, Term v, u32 dst_buf_id) {
  u32 op = 0;
  u64 loc = 0;
  if (!uop_dag_decode_uop(v, &op, &loc)) return 0;
  // Memo: shared subexpressions reuse the prior buffer when no
  // specific destination was requested.  When dst_buf_id is set
  // we must emit into the new buffer (destination is fixed by
  // the STORE root that this is the value of).
  if (dst_buf_id == 0) {
    u32 cached = dag_enc_lookup(c, v);
    if (cached != 0) return cached;
  }
  if (op == UOP_INDEX_E) {
    // INDEX_E(buf, addr) -- in lifted multi-output kernels the buffer
    // is always an input UOP_BUFFER (instance >= 1) and addr is the
    // single full-numel UOP_RANGE.  Resolve to the input slot's
    // effective Metal buf id directly; no shader call needed.
    Term buf = uop_dag_heap_read(loc, 0);
    u32 buf_op = 0; u64 buf_loc = 0;
    if (!uop_dag_decode_uop(buf, &buf_op, &buf_loc)
        || buf_op != UOP_BUFFER) return 0;
    u32 inst = uop_dag_buffer_instance(buf);
    if (inst < 1) return 0;
    u32 slot = inst - 1;
    if (slot >= c->ke->n_inputs) return 0;
    u32 bid = c->effective_buf_ids[slot];
    if (bid == 0 || bid >= METAL_BUFS_NEXT) return 0;
    if (dst_buf_id != 0) {
      // STORE wants its value in dst_buf_id but the value is an input
      // load.  Encode as a "binary ADD with a zero CONST" or
      // equivalent shader call -- but multi-output kernels don't emit
      // a STORE whose value is a bare INDEX_E (that's a trivial copy;
      // the splice planner doesn't generate them).  Bail to legacy
      // for safety.
      return 0;
    }
    if (!dag_enc_remember(c, v, bid, 0)) return 0;
    return bid;
  }
  if (op == UOP_CONST) {
    u32 bid = dag_enc_const(c, v, dst_buf_id);
    if (bid == 0) return 0;
    // Cache by Term so a subsequent reference to the same UOP_CONST
    // reuses the buffer (memoised); only safe when dst_buf_id was 0.
    if (dst_buf_id == 0) {
      if (!dag_enc_remember(c, v, bid, 1)) return 0;
    }
    return bid;
  }
  if (uop_dag_is_unary_ew(op)) {
    Term src = uop_dag_heap_read(loc, 0);
    u32 src_bid = dag_enc_value(c, src, 0);
    if (src_bid == 0) return 0;
    u32 src_arr[2] = { src_bid, 0 };
    u32 bid = dag_enc_arith(c, v, dst_buf_id, op, 1, src_arr);
    if (bid == 0) return 0;
    if (dst_buf_id == 0) {
      if (!dag_enc_remember(c, v, bid, 1)) return 0;
    }
    return bid;
  }
  if (uop_dag_is_binary_ew(op)) {
    Term a = uop_dag_heap_read(loc, 0);
    Term b = uop_dag_heap_read(loc, 1);
    u32 a_bid = dag_enc_value(c, a, 0);
    u32 b_bid = dag_enc_value(c, b, 0);
    if (a_bid == 0 || b_bid == 0) return 0;
    u32 src_arr[2] = { a_bid, b_bid };
    u32 bid = dag_enc_arith(c, v, dst_buf_id, op, 2, src_arr);
    if (bid == 0) return 0;
    if (dst_buf_id == 0) {
      if (!dag_enc_remember(c, v, bid, 1)) return 0;
    }
    return bid;
  }
  // Unsupported op (REDUCE, movement, CAST, IWHERE, etc.).  The lift's
  // multi-output path doesn't generate these -- bail to legacy for
  // any future widening.
  return 0;
}

// Resolve the destination Metal buf id for a UOP_STORE based on its
// destination UOP_BUFFER's instance: instance==0 = primary outBuf;
// instance == KERNEL_LIFT_EXTRA_INST_BASE + ei = extra output ei.
// Returns 0 if the buffer slot can't be resolved (callee bails).
#define DAG_LIFT_EXTRA_INST_BASE (1u + KERNEL_LIFT_MAX_INPUT)
static u32 dag_enc_resolve_store_dst(DagEncCtx *c, Term store_buf,
                                     u32 primary_out_buf_id) {
  u32 buf_op = 0; u64 buf_loc = 0;
  if (!uop_dag_decode_uop(store_buf, &buf_op, &buf_loc)
      || buf_op != UOP_BUFFER) {
    return 0;
  }
  u32 inst = uop_dag_buffer_instance(store_buf);
  if (inst == 0) {
    return primary_out_buf_id;
  }
  if (inst >= DAG_LIFT_EXTRA_INST_BASE) {
    u32 ei = inst - DAG_LIFT_EXTRA_INST_BASE;
    if (ei >= (u32)c->ke->n_extra_outputs) return 0;
    u32 extra_tid = c->ke->extra_output_tids[ei];
    if (extra_tid == 0 || extra_tid >= TENS_NEXT) return 0;
    u32 extra_buf_id = TENS[extra_tid].buf_id;
    if (extra_buf_id == 0 || extra_buf_id >= METAL_BUFS_NEXT
        || METAL_BUFS[extra_buf_id].buf == nil) return 0;
    return extra_buf_id;
  }
  // Input buffer instance (1..KERNEL_LIFT_MAX_INPUT) is not a valid
  // STORE destination in the lift's contract -- bail.
  return 0;
}

// Walk the AFTER chain inner-first (matches uwalk_emit_after) so the
// outermost STORE writes last.  Each STORE's value tree gets its own
// post-order encoding; intermediates allocated for one STORE are
// reused inside that STORE only (Metal hazard-tracks across encoders
// in the same command buffer so they're safe to read by the next
// STORE if any subexpression happens to be shared via hash-cons).
static int dag_enc_emit_node(DagEncCtx *c, Term node, u32 primary_out_buf_id);

static int dag_enc_emit_after(DagEncCtx *c, Term after, u32 primary_out_buf_id) {
  u32 op = 0; u64 loc = 0;
  if (!uop_dag_decode_uop(after, &op, &loc) || op != UOP_AFTER) return 0;
  Term inner       = uop_dag_heap_read(loc, 0);
  Term inner_after = uop_dag_heap_read(loc, 1);
  if (!dag_enc_emit_node(c, inner_after, primary_out_buf_id)) return 0;
  if (!dag_enc_emit_node(c, inner,       primary_out_buf_id)) return 0;
  return 1;
}

static int dag_enc_emit_store(DagEncCtx *c, Term store, u32 primary_out_buf_id) {
  u32 op = 0; u64 loc = 0;
  if (!uop_dag_decode_uop(store, &op, &loc) || op != UOP_STORE) return 0;
  Term buf = uop_dag_heap_read(loc, 0);
  Term val = uop_dag_heap_read(loc, 2);
  u32 dst  = dag_enc_resolve_store_dst(c, buf, primary_out_buf_id);
  if (dst == 0) return 0;
  u32 res = dag_enc_value(c, val, dst);
  return res != 0;
}

static int dag_enc_emit_node(DagEncCtx *c, Term node, u32 primary_out_buf_id) {
  u32 op = 0; u64 loc = 0;
  if (!uop_dag_decode_uop(node, &op, &loc)) return 0;
  if (op == UOP_STORE) return dag_enc_emit_store(c, node, primary_out_buf_id);
  if (op == UOP_AFTER) return dag_enc_emit_after(c, node, primary_out_buf_id);
  return 0;
}

// Public entry point: encode the lifted DAG.  Returns 1 on success
// (caller submits the command buffer + cleans up); 0 to fall through
// to the legacy per-op KProgOp encoder.  Releases its own intermediate
// buffer allocations on either path.
//
// `cmd` is the SAME MTLCommandBuffer the caller would pass to the
// legacy encoder; the encoder appends compute-command-encoders to it
// so Metal's hazard tracker handles inter-encoder dependencies.
static int dag_metal_encode_kernel(KernelEntry *ke,
                                   u32 *effective_buf_ids,
                                   u32 out_buf_id,
                                   id<MTLCommandBuffer> cmd) {
  Term root = ke->cached_lift.store_root;
  if (root == 0) return 0;
  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return 0;
  // Single-output kernels are already handled by metal_tile_jit_encode
  // via cg_emit_via_uop; only multi-output kernels (which tile_jit
  // rejects via cg_kernel_has_extra_outputs) reach here.  Guard against
  // accidental misroutes.
  // The encoder works for single-output too -- just falls through to a
  // single STORE walk -- so don't gate on n_extra_outputs.
  DagEncCtx ctx = {0};
  ctx.ke                 = ke;
  ctx.effective_buf_ids  = effective_buf_ids;
  ctx.numel              = ke->output_numel;
  ctx.dtype              = ke->output_dtype;
  ctx.cmd                = cmd;
  if (ctx.numel == 0) return 0;
  if (ctx.dtype != DT_FP32 && ctx.dtype != DT_INT32) return 0;
  int ok = dag_enc_emit_node(&ctx, root, out_buf_id);
  // Release our intermediates regardless of outcome.  When ok==0 the
  // caller will fall back to the legacy encoder -- that path doesn't
  // touch our partial dispatches (Metal command buffers stay in build
  // mode until commit; nothing executed yet) so dropping the buffers
  // here keeps the leak count flat.
  for (u32 i = 0; i < ctx.n_inter; i++) {
    metal_buf_decref_after_batch(ctx.inter_buf_ids[i]);
  }
  return ok;
}

static int metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) return -1;
  // Per-op GPU profiling: clear the current-kid attribution before any
  // pre-dispatch flush (defer-backlog drain below) so leftover work
  // from the prior dispatch isn't double-counted; set it again once we
  // know this kernel's kid.
  METAL_PEROP_CUR_KID = 0;

  // Bound the deferred-decref backlog *between* kernels.  Within a
  // batched step the per-op fallback path (metal-op) materializes one
  // MTLBuffer per KProgOp and decref_after_batch's them all -- but
  // those decrefs run with METAL_ENCODING_DEPTH > 0, so the
  // limit-exceeded flush in metal_buf_alloc / metal_buf_decref_after_batch
  // never fires and the backlog grows without bound (observed: 2.2 GB
  // of deferred buffers at BS=32, ~35 GB projected at BS=512).  Here,
  // at the top of the next kernel dispatch, no command encoder is open
  // and METAL_BATCH_CMD's prior work is safe to commit, so a flush is
  // sound: it commits the in-flight command buffer, waits, releases the
  // deferred buffers, and lets the next kernel start a fresh batch cmd.
  if (METAL_ENCODING_DEPTH == 0 && METAL_DEFER_DECREF_LEN > 0) {
    u64 limit = metal_defer_limit_bytes();
    if (limit != 0 && METAL_DEFER_DECREF_BYTES > limit) {
      metal_dispatch_flush();
    }
  }

  u32 tile_groups_x  = 0;
  u32 tile_threads_x = 0;
  int tile_supported = metal_tile_enabled()
      && cg_tile_metal_dispatch_shape(ke, &tile_groups_x, &tile_threads_x);
  int kprog_supported = metal_kernel_supported(ke);
  if (!tile_supported && !kprog_supported) return -1;

  // Profile this dispatch.  kid = ke - KERNELS gives the slot index
  // the WL TKernelProfile / TKernelDispatchKind surface reads.
  u32 kid = (u32)(ke - KERNELS);
  u64 t0  = cg_now_us();
  // Per-op GPU profiling: record the kid this command buffer is for so
  // metal_record_gpu_time can attribute [GPUEndTime]-[GPUStartTime].
  // Reset to 0 below so an unrelated flush (e.g. buf_read) isn't
  // mis-attributed; only the standalone submit at the end of this
  // dispatch picks it up.
  if (metal_perop_enabled()) METAL_PEROP_CUR_KID = kid;

  // THVM_DISPATCH_TRACE=1: one line per kernel dispatch with shape +
  // wall-time, plus per-op-loop checkpoint timing.  Cheap (env read
  // once) and gated off by default; the bottleneck-finder during the
  // BS=512 cold-compile investigation lived here.
  static int _disp_trace = -1;
  if (_disp_trace < 0) {
    char const *e = getenv("THVM_DISPATCH_TRACE");
    _disp_trace = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  if (_disp_trace) {
    u32 in0 = ke->n_inputs ? ke->input_numels[0] : 0;
    fprintf(stderr,
            "[disp kid=%u n_ops=%u n_inputs=%u out_numel=%llu in0_numel=%u tile=%d kprog=%d]\n",
            kid, ke->n_ops, ke->n_inputs,
            (unsigned long long)ke->output_numel, in0,
            tile_supported, kprog_supported);
    fflush(stderr);
  }

  // THVM_DUMP_KID_PROGRAM=1 prints the program op-list of every
  // dispatched kernel that contains a REDUCE.  Mirrors the
  // tile_analyze_gemm reject-ops diagnostic but for the dispatched
  // (post-rejection) side: we get to see kid 14 / kid 16's actual
  // program shape so the matmul-shape relaxation in tile_analyze_gemm
  // can target the right pattern.  Level 50.
  {
    char const *_e   = getenv("THVM_DUMP_KID_PROGRAM");
    char const *_eall = getenv("THVM_DUMP_KID_PROGRAM_ALL");
    int _dump = (_e != NULL && _e[0] == '1');
    int _all  = (_eall != NULL && _eall[0] == '1');
    if ((_dump || _all) && ke != NULL && ke->program != NULL) {
      int has_reduce = 0;
      for (u32 i = 0; i < ke->n_ops; i++) {
        if (ke->program[i].opcode == UOP_REDUCE) { has_reduce = 1; break; }
      }
      // _all bypasses the REDUCE filter so non-REDUCE kernels (e.g.
      // conv-prep kid 1) get dumped for cross-shape investigation.
      if (has_reduce || _all) {
        fprintf(stderr, "  dispatch-kid kid=%u n_inputs=%u n_ops=%u ops=[",
                kid, ke->n_inputs, ke->n_ops);
        for (u32 i = 0; i < ke->n_ops; i++) {
          fprintf(stderr, "%s%u", i ? "," : "", (unsigned)ke->program[i].opcode);
        }
        // For sizing the matmul relaxation: append per-op numel and
        // arg (the latter encodes REDUCE inner = arg & 0xFFFFFF, kind
        // = (arg>>24)&0xFF).  Reader derives M*N from REDUCE.numel,
        // N from inner, K from MUL.numel/REDUCE.numel.
        fprintf(stderr, "] numel=[");
        for (u32 i = 0; i < ke->n_ops; i++) {
          fprintf(stderr, "%s%llu", i ? "," : "",
                  (unsigned long long)ke->program[i].numel);
        }
        fprintf(stderr, "] arg=[");
        for (u32 i = 0; i < ke->n_ops; i++) {
          fprintf(stderr, "%s%u", i ? "," : "", (unsigned)ke->program[i].arg);
        }
        fprintf(stderr, "]\n");
      }
    }
  }

  if (kprog_supported && metal_try_alias_reshape(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_METAL_ALIAS, cg_now_us() - t0);
    return 0;
  }

  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return -1;
  id<MTLBuffer> outBuf = METAL_BUFS[out_buf_id].buf;
  if (outBuf == nil) return -1;

  // View-aware pre-materialize state.  The actual strided-view ->
  // contiguous-temp copy is done LAZILY below, only once we know the
  // generated-tile path declined: metal_tile_jit_encode binds the
  // ORIGINAL strided input buffers directly and bakes the view
  // strides into the address expressions it emits, so it must NOT
  // see pre-materialised buffers.  The KProgOp per-op loop and the
  // DAG-side per-op encoder, on the other hand, read inputs
  // contiguously and DO need the pre-mat.  Doing it eagerly here
  // (gated only on !tile_supported, i.e. tile *eligibility*) was the
  // conv-im2col Metal bug: cg_tile_metal_dispatch_shape says "yes
  // eligible" for the im2col reduce kernel but metal_tile_jit_encode
  // then bails -- and the per-op fall-through read the raw strided
  // SHRINK-patch buffers as if contiguous, computing garbage.
  u32 effective_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  u32 temp_buf_ids     [ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) temp_buf_ids[i] = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= METAL_BUFS_NEXT) { return -1; }
    effective_buf_ids[i] = ib;
  }

  __unsafe_unretained id<MTLBuffer> jit_src_bufs[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    jit_src_bufs[i] = METAL_BUFS[effective_buf_ids[i]].buf;
  }

  if (tile_supported) {
    id<MTLCommandBuffer> tile_cmd = metal_command_buffer();
    if (metal_tile_jit_encode(ke, jit_src_bufs, outBuf, tile_cmd,
                              tile_groups_x, tile_threads_x)) {
      metal_submit_if_standalone(tile_cmd);
      for (u32 i = 0; i < ke->n_inputs; i++) {
        if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
      }
      cg_profile_record(kid, KDISPATCH_METAL_TILE, cg_now_us() - t0);
      if (_disp_trace)
        fprintf(stderr, "[disp-done kid=%u path=tile us=%llu]\n", kid,
                (unsigned long long)(cg_now_us() - t0));
      return 0;
    }
  }

  if (!kprog_supported) {
    for (u32 i = 0; i < ke->n_inputs; i++) {
      if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
    }
    return -1;
  }

  // Tile path declined (or wasn't eligible).  The per-op / DAG
  // encoders below read inputs contiguously, so pre-materialise any
  // strided-view input now into a contiguous temp buffer (mirror of
  // cpu_interpret's strided pre-mat).  MTLResourceStorageModeShared
  // keeps the bytes host-readable so the copy is plain pointer
  // arithmetic with no extra memcpy out of Metal.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 ib  = in_buf_ids[i];
    u32 tid = ke->input_tids[i];
    TenDesc const *td = (tid != 0 && tid < TENS_NEXT) ? &TENS[tid] : NULL;
    // When rangeify folded this input's ShapeTracker chain into the
    // kernel INDEX (input_chain_composed -- the LOAD composes the full
    // chain, public view + every inner offset/stride, over the raw
    // buffer), there is nothing to materialise: skip the gather entirely
    // (mirrors cpu_premat_chained_input's early return).
    int chain_composed = (ke->input_chain_composed != NULL
                          && ke->input_chain_composed[i]);
    int needs_premat = (td != NULL && !chain_composed
                        && (!td->view.contiguous
                            || td->view.offset != 0
                            || td->nviews != 0));
    if (!needs_premat) continue;
    View const *v = &td->view;
    u32 numel = v->numel;
    u32 tmp_id = metal_buf_alloc(dtype_storage_bytes(td->dtype, numel));
    if (tmp_id == 0) {
      for (u32 k = 0; k < i; k++)
        if (temp_buf_ids[k]) metal_buf_decref_after_batch(temp_buf_ids[k]);
      return -1;
    }
    f32 *src = (f32 *)[METAL_BUFS[ib].buf contents];
    f32 *dst = (f32 *)[METAL_BUFS[tmp_id].buf contents];
    for (u32 k = 0; k < numel; k++) {
      dst[k] = src[metal_tendesc_strided_index(td, k)];
    }
    effective_buf_ids[i] = tmp_id;
    temp_buf_ids     [i] = tmp_id;
  }

  // Phase C slice 5: DAG-side per-op encoder.  Fires when the lift
  // succeeded (typical case for multi-output kernels that bypassed
  // tile_jit_encode at the cg_kernel_has_extra_outputs gate).  Skips
  // ke->program[] entirely; walks ke->cached_lift.store_root instead.
  // Falls through to the legacy KProgOp loop on failure (or when
  // store_root == 0 = lift declined).
  if (ke->cached_lift.store_root != 0) {
    METAL_ENCODING_DEPTH++;
    id<MTLCommandBuffer> dag_cmd = metal_command_buffer();
    int dag_ok = dag_metal_encode_kernel(ke, effective_buf_ids,
                                          out_buf_id, dag_cmd);
    if (dag_ok) {
      metal_submit_if_standalone(dag_cmd);
      METAL_ENCODING_DEPTH--;
      for (u32 i = 0; i < ke->n_inputs; i++) {
        if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
      }
      cg_profile_record(kid, KDISPATCH_METAL_OP, cg_now_us() - t0);
      if (_disp_trace)
        fprintf(stderr, "[disp-done kid=%u path=dag us=%llu]\n", kid,
                (unsigned long long)(cg_now_us() - t0));
      return 0;
    }
    METAL_ENCODING_DEPTH--;
    // Encoder declined (unsupported op shape, alloc failure, etc.).
    // Fall through to the legacy program[] loop below.  No partial
    // command-buffer state to roll back: dag_metal_encode_kernel
    // appends encoders to dag_cmd which we never submit on the
    // failure path; ARC drops the unsubmitted command buffer.
  }

  // Per-op interpreter path: one encoder per KProgOp[] entry.  Mirror
  // of cpu_interpret.  Allocate one Metal buf per intermediate op;
  // the final op writes to outBuf.  All ops share a single
  // MTLCommandBuffer, and Metal hazard-tracks reads/writes of
  // MTLResourceStorageModeShared buffers across encoders so each
  // encoder naturally sees the previous encoder's writes without an
  // explicit barrier.
  // Sized to ke->n_ops (KPROG_MAX_OPS is now a sanity bound, not a
  // typical size).  ke->n_ops > 0 since we early-bailed when 0.
  u32 inter_buf_ids[ke->n_ops];
  for (u32 i = 0; i < ke->n_ops; i++) inter_buf_ids[i] = 0;
  // last_use[i] = highest op index that reads intermediate i (an
  // earlier op's output).  After op last_use[i] runs, buffer i is
  // dead and -- once the GPU has drained those reads -- reclaimable.
  // No reader (degenerate) -> i (drop right after it's written).  The
  // final op writes outBuf and store_extra ops write the extra-output
  // buffer, so neither owns an inter_buf_ids[] slot; only "plain" ops
  // do, and those normally have a downstream reader.
  u32 last_use[ke->n_ops];
  for (u32 i = 0; i < ke->n_ops; i++) last_use[i] = i;
  for (u32 j = 0; j < ke->n_ops; j++) {
    KProgOp *q = &ke->program[j];
    for (u8 s = 0; s < q->n_src; s++) {
      u32 raw = q->src[s];
      if (KSRC_IS_INPUT(raw)) continue;
      u32 idx = KSRC_INDEX(raw);
      if (idx < ke->n_ops && j > last_use[idx]) last_use[idx] = j;
    }
  }
  u64 live_inter_bytes = 0;
  // Per-op-loop checkpoint threshold.  Once the materialised
  // intermediates exceed this, drain the GPU and recycle the dead
  // ones back onto the free-list (see below).  Default 64 MiB --
  // small enough that a conv-im2col program (dozens of O(100 MB)
  // intermediates) checkpoints every couple of ops, keeping the
  // count of distinct freshly-backed MTLBuffers tiny: the GPU's
  // first write to a fresh shared buffer is pathologically slow
  // (cold pages stall the command processor; a single ~80 MB
  // conv-im2col PAD intermediate measured tens of seconds of GPU
  // "execution" at BS=64+).  THVM_METAL_PEROP_BUDGET overrides
  // (bytes; 0 = use the legacy metal_defer_limit_bytes()).
  u64 inter_budget;
  {
    static int known = 0;
    static u64 budget = 0;
    if (!known) {
      char const *e = getenv("THVM_METAL_PEROP_BUDGET");
      if (e != NULL && e[0] != '\0') budget = strtoull(e, NULL, 10);
      known = 1;
    }
    inter_budget = budget != 0 ? budget : (64ull * 1024ull * 1024ull);
    if (inter_budget == 0) inter_budget = metal_defer_limit_bytes();
  }
  METAL_ENCODING_DEPTH++;
  id<MTLCommandBuffer> cmd = metal_command_buffer();
  int rc = 0;

  for (u32 step = 0; step < ke->n_ops; step++) {
    KProgOp *p = &ke->program[step];

    // Resolve this op's src buffers and numels.  KSRC_IS_INPUT(s)
    // -> kernel input (effective_buf_ids[KSRC_INDEX(s)]); plain
    // index -> output of earlier op (inter_buf_ids[KSRC_INDEX(s)]).
    __unsafe_unretained id<MTLBuffer> src_bufs[MAX_UOP_SRC] = {0};
    u32                                src_numels[MAX_UOP_SRC] = {0};
    int src_resolve_ok = 1;
    for (u8 s = 0; s < p->n_src; s++) {
      u32 raw = p->src[s];
      u32 idx = KSRC_INDEX(raw);
      if (KSRC_IS_INPUT(raw)) {
        if (idx >= ke->n_inputs) { src_resolve_ok = 0; break; }
        u32 ib = effective_buf_ids[idx];
        if (ib == 0 || ib >= METAL_BUFS_NEXT) { src_resolve_ok = 0; break; }
        src_bufs  [s] = METAL_BUFS[ib].buf;
        src_numels[s] = ke->input_numels[idx];
      } else {
        if (idx >= step) { src_resolve_ok = 0; break; }
        u32 ib = inter_buf_ids[idx];
        if (ib == 0 || ib >= METAL_BUFS_NEXT) { src_resolve_ok = 0; break; }
        src_bufs  [s] = METAL_BUFS[ib].buf;
        src_numels[s] = ke->program[idx].numel;
      }
    }
    if (!src_resolve_ok) { rc = -1; break; }

    // Decide where this op writes:
    //   - last op -> outBuf (legacy primary output);
    //   - store_extra_plus_one > 0 -> extra output's Metal buffer
    //     (Step 6 of multi-output groundwork: the merged kernel
    //     fans the marked mid-program op directly into its
    //     dedicated extra output buf; safe because the splice
    //     puts the child boundary's final op last in its own
    //     subtree, with no downstream KProgOp consuming it);
    //   - otherwise -> a fresh intermediate Metal buffer sized
    //     for this op's dtype.
    id<MTLBuffer> dst_buf;
    if (step + 1 == ke->n_ops) {
      dst_buf = outBuf;
    } else if (p->store_extra_plus_one > 0) {
      u32 extra_idx = (u32)p->store_extra_plus_one - 1u;
      if (extra_idx >= (u32)ke->n_extra_outputs) { rc = -1; break; }
      u32 extra_tid = ke->extra_output_tids[extra_idx];
      if (extra_tid == 0 || extra_tid >= TENS_NEXT) { rc = -1; break; }
      u32 extra_buf_id = TENS[extra_tid].buf_id;
      if (extra_buf_id == 0 || extra_buf_id >= METAL_BUFS_NEXT
          || METAL_BUFS[extra_buf_id].buf == nil) { rc = -1; break; }
      dst_buf = METAL_BUFS[extra_buf_id].buf;
    } else {
      u32 dst_numel = p->numel ? p->numel : 1;
      u64 dst_bytes = dtype_storage_bytes(p->dtype, dst_numel);
      // Before the alloc would push our materialized-intermediate
      // footprint over budget, drain the GPU and reclaim everything
      // dead so far.  Bounds the per-op-loop peak to roughly
      // budget + a couple of consecutive ops' outputs, instead of
      // sum-of-all-N-intermediates (the im2col-style conv-backward
      // fallback programs were materialising GBs at high BS).
      if (inter_budget != 0 && live_inter_bytes + dst_bytes > inter_budget
          && live_inter_bytes > 0) {
        u64 _cp0 = _disp_trace ? cg_now_us() : 0;
        [cmd commit];
        [cmd waitUntilCompleted];
        if (cmd == METAL_BATCH_CMD) METAL_BATCH_CMD = nil;
        for (u32 i = 0; i < step; i++) {
          if (inter_buf_ids[i] != 0 && last_use[i] < step) {
            live_inter_bytes -= METAL_BUFS[inter_buf_ids[i]].nbytes;
            // Recycle the buffer object onto the free-list instead of
            // releasing the MTLBuffer.  The per-op fallback for a
            // conv-im2col program emits dozens of equal-size
            // intermediates one after another; releasing each one
            // forces a fresh newBufferWithLength: for the next, and
            // the GPU's first write to a freshly-backed shared buffer
            // is pathologically slow (the cold pages stall the GPU
            // command processor -- a single ~80 MB conv-im2col PAD
            // intermediate measured tens of seconds of GPU "execution"
            // at BS=64+, scaling super-linearly with batch size).
            // Pushing to the free-list lets the loop reuse the same
            // handful of buffer objects (whose pages stay wired), so
            // only a couple of intermediates per size class ever pay
            // the cold-page cost.  We've already committed + waited on
            // `cmd`, so the buffer's GPU work is done -- recycling is
            // sound.
            if (!metal_buf_freelist_push_impl(inter_buf_ids[i])) {
              metal_buf_free(inter_buf_ids[i]);
            }
            inter_buf_ids[i] = 0;
          }
        }
        cmd = metal_command_buffer();
        if (_disp_trace)
          fprintf(stderr, "[disp-cp kid=%u step=%u live=%llu us=%llu]\n",
                  kid, step, (unsigned long long)live_inter_bytes,
                  (unsigned long long)(cg_now_us() - _cp0));
      }
      u32 tmp_id = metal_buf_alloc(dst_bytes);
      if (tmp_id == 0) { rc = -1; break; }
      inter_buf_ids[step] = tmp_id;
      live_inter_bytes += METAL_BUFS[tmp_id].nbytes;
      dst_buf = METAL_BUFS[tmp_id].buf;
    }

    // New encoder per op; Metal hazard-tracks shared buffer
    // dependencies across encoders in the same command buffer.
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    int enc_rc = metal_encode_op(enc, p, src_bufs, src_numels, dst_buf);
    [enc endEncoding];
    if (enc_rc != 0) { rc = enc_rc; break; }
  }

  if (rc == 0) {
    metal_submit_if_standalone(cmd);
  }

  // Cleanup: drop intermediate Metal buffers + view-pre-mat temps.
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (inter_buf_ids[i]) metal_buf_decref_after_batch(inter_buf_ids[i]);
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
  }
  METAL_ENCODING_DEPTH--;
  if (rc == 0) cg_profile_record(kid, KDISPATCH_METAL_OP, cg_now_us() - t0);
  if (_disp_trace)
    fprintf(stderr, "[disp-done kid=%u path=op rc=%d us=%llu]\n", kid, rc,
            (unsigned long long)(cg_now_us() - t0));
  return rc;
}

// === AOT-on-Metal: cached book_heap MTLBuffer wrapper ================
//
// Phase 7 iter BB: book_heap is a fixed pointer for the lifetime of
// the host's TContext.  Each AOT-on-Metal call wrapped it via
// newBufferWithBytesNoCopy, paying the page-table-setup cost.  Cache
// the resulting MTLBuffer and re-use across calls; invalidate if the
// (pointer, length) pair changes (e.g., after thvm_free + thvm_init).
// Globals AOT_METAL_HEAP_{BUF,PTR,LEN} are forward-declared near
// metal_shutdown -- it nils them on shutdown to drop ARC's strong
// ref before the host frees book_heap's backing pages.

static id<MTLBuffer> aot_metal_heap_buf(Term *book_heap, u64 book_cells) {
  u64 len = book_cells * sizeof(Term);
  if (AOT_METAL_HEAP_BUF != nil
      && AOT_METAL_HEAP_PTR == book_heap
      && AOT_METAL_HEAP_LEN == len) {
    return AOT_METAL_HEAP_BUF;
  }
  AOT_METAL_HEAP_BUF =
      [METAL_DEVICE newBufferWithBytesNoCopy:book_heap
                                      length:len
                                     options:MTLResourceStorageModeShared
                                 deallocator:nil];
  AOT_METAL_HEAP_PTR = book_heap;
  AOT_METAL_HEAP_LEN = len;
  return AOT_METAL_HEAP_BUF;
}

// === AOT-on-Metal Phase 7 iter A ====================================
//
// Smallest end-to-end slice: dispatch the `aot_eval_op2_fold` MSL
// kernel with one thread.  Caller passes its book_heap pointer + the
// root_loc of an OP2(NUM, NUM) cell; we copy the heap into a shared
// MTLBuffer, dispatch, read the resulting NUM back.
//
// Iter B+ moves to zero-copy (newBufferWithBytesNoCopy on a
// page-aligned book_heap), wider dispatch, and the layered MSL
// primitives (allocator, MAT/CTR/REF dispatch, full wnf()).

static id<MTLComputePipelineState> AOT_OP2_PSO = nil;

Term thvm_aot_metal_op2_fold(Term *book_heap, u64 book_cells, u64 root_loc) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil || METAL_LIB == nil) {
    if (metal_init() != 0) return 0;
  }
  if (AOT_OP2_PSO == nil) {
    NSError *err = nil;
    id<MTLFunction> mtlFn =
        [METAL_LIB newFunctionWithName:@"aot_eval_op2_fold"];
    if (mtlFn == nil) {
      fprintf(stderr,
        "thvm aot-metal: function 'aot_eval_op2_fold' missing in metallib\n");
      return 0;
    }
    AOT_OP2_PSO =
        [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
    if (AOT_OP2_PSO == nil) {
      fprintf(stderr, "thvm aot-metal: pipeline-state failed: %s\n",
              err ? [[err localizedDescription] UTF8String] : "(no error)");
      return 0;
    }
  }
  // Zero-copy: book_heap is 16KB-aligned (see thvm.c book_heap
  // alloc), so newBufferWithBytesNoCopy wraps it directly and the
  // GPU reads CPU's heap pages with no intermediate marshaling.
  // Length must be a page multiple; BOOK_CAP * 8 = 2 MiB = 128 pages.
  id<MTLBuffer> heapBuf =
      aot_metal_heap_buf(book_heap, book_cells);
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:sizeof(Term)
                                options:MTLResourceStorageModeShared];

  uint64_t root = root_loc;
  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:AOT_OP2_PSO];
  [enc setBuffer:heapBuf   offset:0 atIndex:0];
  [enc setBytes:&root      length:sizeof(uint64_t) atIndex:1];
  [enc setBuffer:resultBuf offset:0 atIndex:2];
  [enc dispatchThreads:MTLSizeMake(1, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  Term result;
  memcpy(&result, [resultBuf contents], sizeof(Term));
  return result;
}

// Iter C-1+C-2: MAT-on-NUM dispatch with bump allocator.  Caller
// supplies a pointer to the host's BOOK_NEXT counter; we seed an
// 8-byte shared MTLBuffer from it, dispatch, then read back so the
// host's counter reflects any cells the kernel allocated for the
// unmatched-fallback branch.  See shader for bit-pattern shape.
static id<MTLComputePipelineState> AOT_MAT_PSO = nil;

Term thvm_aot_metal_mat_app(Term *book_heap, u64 book_cells,
                             u64 root_loc, u64 *book_next_inout) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil || METAL_LIB == nil) {
    if (metal_init() != 0) return 0;
  }
  if (AOT_MAT_PSO == nil) {
    NSError *err = nil;
    id<MTLFunction> mtlFn =
        [METAL_LIB newFunctionWithName:@"aot_eval_mat_app"];
    if (mtlFn == nil) {
      fprintf(stderr,
        "thvm aot-metal: function 'aot_eval_mat_app' missing in metallib\n");
      return 0;
    }
    AOT_MAT_PSO =
        [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
    if (AOT_MAT_PSO == nil) {
      fprintf(stderr, "thvm aot-metal: mat pipeline-state failed: %s\n",
              err ? [[err localizedDescription] UTF8String] : "(no error)");
      return 0;
    }
  }
  id<MTLBuffer> heapBuf =
      aot_metal_heap_buf(book_heap, book_cells);
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:sizeof(Term)
                                options:MTLResourceStorageModeShared];
  // book_next is u32 on the GPU side (Apple GPU families lack
  // 64-bit atomic_fetch_add).  BOOK_CAP fits comfortably; assert
  // and marshal narrowed.
  if (*book_next_inout >> 32) {
    fprintf(stderr,
      "thvm aot-metal: book_next %llu exceeds 32-bit GPU bump range\n",
      (unsigned long long)*book_next_inout);
    return 0;
  }
  uint32_t book_next_u32 = (uint32_t)*book_next_inout;
  id<MTLBuffer> bookNextBuf =
      [METAL_DEVICE newBufferWithBytes:&book_next_u32
                                length:sizeof(uint32_t)
                               options:MTLResourceStorageModeShared];

  uint64_t root = root_loc;
  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:AOT_MAT_PSO];
  [enc setBuffer:heapBuf     offset:0 atIndex:0];
  [enc setBytes:&root        length:sizeof(uint64_t) atIndex:1];
  [enc setBuffer:resultBuf   offset:0 atIndex:2];
  [enc setBuffer:bookNextBuf offset:0 atIndex:3];
  [enc dispatchThreads:MTLSizeMake(1, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  Term result;
  memcpy(&result, [resultBuf contents], sizeof(Term));
  uint32_t out_u32;
  memcpy(&out_u32, [bookNextBuf contents], sizeof(uint32_t));
  *book_next_inout = (u64)out_u32;
  return result;
}

// Batch variant: fold N independent OP2 redexes in one dispatch.
// Caller supplies an array of root_locs (length n_roots) and a
// preallocated result_out array (length n_roots).  Returns 0 on
// success, -1 on failure.  Demonstrates the parallelism unlock --
// kernel launch overhead amortized over N folds.
static id<MTLComputePipelineState> AOT_OP2_BATCH_PSO = nil;

int thvm_aot_metal_op2_fold_batch(Term *book_heap, u64 book_cells,
                                   u64 *root_locs, u32 n_roots,
                                   Term *result_out) {
  if (n_roots == 0) return 0;
  if (METAL_DEVICE == nil || METAL_QUEUE == nil || METAL_LIB == nil) {
    if (metal_init() != 0) return -1;
  }
  if (AOT_OP2_BATCH_PSO == nil) {
    NSError *err = nil;
    id<MTLFunction> mtlFn =
        [METAL_LIB newFunctionWithName:@"aot_eval_op2_fold_batch"];
    if (mtlFn == nil) {
      fprintf(stderr,
        "thvm aot-metal: function 'aot_eval_op2_fold_batch' missing\n");
      return -1;
    }
    AOT_OP2_BATCH_PSO =
        [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
    if (AOT_OP2_BATCH_PSO == nil) {
      fprintf(stderr, "thvm aot-metal: batch pipeline-state failed: %s\n",
              err ? [[err localizedDescription] UTF8String] : "(no error)");
      return -1;
    }
  }
  id<MTLBuffer> heapBuf =
      aot_metal_heap_buf(book_heap, book_cells);
  id<MTLBuffer> rootsBuf =
      [METAL_DEVICE newBufferWithBytes:root_locs
                                length:n_roots * sizeof(uint64_t)
                               options:MTLResourceStorageModeShared];
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:n_roots * sizeof(Term)
                                options:MTLResourceStorageModeShared];

  uint32_t n = n_roots;
  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:AOT_OP2_BATCH_PSO];
  [enc setBuffer:heapBuf   offset:0 atIndex:0];
  [enc setBuffer:rootsBuf  offset:0 atIndex:1];
  [enc setBuffer:resultBuf offset:0 atIndex:2];
  [enc setBytes:&n         length:sizeof(uint32_t) atIndex:3];

  NSUInteger tg = MIN((NSUInteger)n_roots,
                      [AOT_OP2_BATCH_PSO maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(n_roots, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  memcpy(result_out, [resultBuf contents], n_roots * sizeof(Term));
  return 0;
}

// === AOT-on-Metal Phase 7 iter D: end-to-end compile + run =========
//
// thvm_aot_metal_compile_and_run("name", def_id, args, n_args,
//                                 book_heap, book_cells, book_next_inout)
//
// Calls thvm_aot_metal_emit to produce MSL source for the def, writes
// it to a temp file, compiles via xcrun metal/metallib, loads the
// resulting metallib, looks up `aot_def_<name>`, builds a PSO, and
// dispatches with one thread.  Result Term is returned; book_next is
// updated in place.
//
// Cache key is the FNV-1a hash of the emitted MSL string.  Repeated
// calls for an identical def re-use the cached PSO.

extern char *thvm_aot_metal_emit(uint32_t def_id, const char *name);

#define AOT_METAL_PSO_CAP 32
// Parallel arrays so ARC retains the PSO via the strong array slot
// (file-static id arrays are __strong by default).  Mirrors the
// METAL_JIT_CACHE / METAL_JIT_PSOS pattern above.
static uint64_t                    AOT_METAL_PSO_HASHES[AOT_METAL_PSO_CAP];
static id<MTLComputePipelineState> AOT_METAL_PSO_OBJS  [AOT_METAL_PSO_CAP];
static uint32_t                    AOT_METAL_PSO_N = 0;

static uint64_t aot_metal_fnv1a(const char *s) {
  uint64_t h = 0xcbf29ce484222325ULL;
  while (*s) {
    h ^= (uint64_t)(unsigned char)*s++;
    h *= 0x100000001b3ULL;
  }
  return h;
}

static id<MTLComputePipelineState>
aot_metal_pso_get(uint64_t hash, const char *name, const char *src) {
  for (uint32_t i = 0; i < AOT_METAL_PSO_N; i++) {
    if (AOT_METAL_PSO_HASHES[i] == hash) return AOT_METAL_PSO_OBJS[i];
  }
  if (AOT_METAL_PSO_N >= AOT_METAL_PSO_CAP) {
    fprintf(stderr, "thvm aot-metal: PSO cache full\n");
    return nil;
  }

  // Write src -> /tmp/thvm_aot_metal_<hash>.metal, run xcrun
  // metal -c then xcrun metallib.  We could shave latency by
  // calling [device newLibraryWithSource:options:error:] (in-process
  // compile) but the offline metallib path matches our existing
  // build/default.metallib pipeline and produces a re-loadable
  // artifact for inspection.
  char src_path[256], air_path[256], lib_path[256];
  snprintf(src_path, sizeof src_path,
           "/tmp/thvm_aot_metal_%016llx.metal", hash);
  snprintf(air_path, sizeof air_path,
           "/tmp/thvm_aot_metal_%016llx.air", hash);
  snprintf(lib_path, sizeof lib_path,
           "/tmp/thvm_aot_metal_%016llx.metallib", hash);

  // Phase 7 iter X: persistent disk cache.  If the metallib for this
  // content hash already exists from a prior session, skip the slow
  // xcrun metal/metallib invocation (~3 sec uncached) and go straight
  // to load.  In-memory PSO cache (the early return above) handles
  // same-session repeat calls; this handles fresh-session repeats.
  struct stat lib_stat;
  if (stat(lib_path, &lib_stat) != 0 || lib_stat.st_size == 0) {
    FILE *f = fopen(src_path, "w");
    if (!f) {
      fprintf(stderr, "thvm aot-metal: cannot write %s\n", src_path);
      return nil;
    }
    fputs(src, f);
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof cmd,
      "xcrun -sdk macosx metal -c %s -o %s 2>&1 && "
      "xcrun -sdk macosx metallib %s -o %s 2>&1",
      src_path, air_path, air_path, lib_path);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "thvm aot-metal: xcrun metal/metallib failed (rc=%d) for %s\n",
              rc, src_path);
      return nil;
    }
  }

  NSError *err = nil;
  NSURL    *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:lib_path]];
  id<MTLLibrary> lib =
      [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm aot-metal: load %s failed: %s\n",
            lib_path,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  char fnname[128];
  snprintf(fnname, sizeof fnname, "aot_def_%s", name);
  id<MTLFunction> mtlFn =
      [lib newFunctionWithName:[NSString stringWithUTF8String:fnname]];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm aot-metal: function '%s' missing in %s\n",
            fnname, lib_path);
    return nil;
  }
  id<MTLComputePipelineState> pso =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (pso == nil) {
    fprintf(stderr, "thvm aot-metal: pso for %s failed: %s\n",
            fnname,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  AOT_METAL_PSO_HASHES[AOT_METAL_PSO_N] = hash;
  AOT_METAL_PSO_OBJS  [AOT_METAL_PSO_N] = pso;
  AOT_METAL_PSO_N++;
  return pso;
}

Term thvm_aot_metal_compile_and_run(const char *name, u32 def_id,
                                     Term *args, u32 n_args,
                                     Term *book_heap, u64 book_cells,
                                     u64 *book_next_inout) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) {
    if (metal_init() != 0) return 0;
  }
  char *src = thvm_aot_metal_emit(def_id, name);
  if (!src) {
    extern const char *thvm_aot_metal_emit_failure_reason(void);
    const char *why = thvm_aot_metal_emit_failure_reason();
    if (why != NULL && why[0] != '\0') {
      fprintf(stderr,
              "thvm aot-metal: emit failed for def_id %u (\"%s\"): %s\n",
              def_id, name, why);
    } else {
      fprintf(stderr,
              "thvm aot-metal: emit failed for def_id %u (\"%s\")\n",
              def_id, name);
    }
    return 0;
  }
  // Phase 7 iter AA: env-gated MSL source dump.  THVM_AOT_METAL_DUMP=1
  // prints the emitted source to stderr; useful for inspecting what
  // the Metal AOT actually generates for a given def shape.
  {
    static int dump_inited = 0;
    static int dump_on     = 0;
    if (!dump_inited) {
      const char *e = getenv("THVM_AOT_METAL_DUMP");
      dump_on = (e != NULL && e[0] == '1');
      dump_inited = 1;
    }
    if (dump_on) {
      fprintf(stderr,
        "// === thvm aot-metal: emitted MSL for \"%s\" ===\n%s"
        "// === end emit \"%s\" ===\n",
        name, src, name);
    }
  }
  uint64_t hash = aot_metal_fnv1a(src);
  id<MTLComputePipelineState> pso = aot_metal_pso_get(hash, name, src);
  free(src);
  if (pso == nil) return 0;

  if (*book_next_inout >> 32) {
    fprintf(stderr, "thvm aot-metal: book_next %llu exceeds u32 range\n",
            (unsigned long long)*book_next_inout);
    return 0;
  }
  uint32_t book_next_u32 = (uint32_t)*book_next_inout;

  id<MTLBuffer> heapBuf =
      aot_metal_heap_buf(book_heap, book_cells);
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:sizeof(Term)
                                options:MTLResourceStorageModeShared];
  id<MTLBuffer> bookNextBuf =
      [METAL_DEVICE newBufferWithBytes:&book_next_u32
                                length:sizeof(uint32_t)
                               options:MTLResourceStorageModeShared];

  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:heapBuf     offset:0 atIndex:0];
  // Iter WW: args is read-only by the kernel + small (<= 64*8 bytes
  // per iter Y), so use setBytes instead of allocating an MTLBuffer.
  // Saves the alloc + ARC-release per call.  For n_args == 0 still
  // bind a placeholder so the kernel parameter slot is valid.
  if (n_args > 0) {
    [enc setBytes:args length:n_args * sizeof(Term) atIndex:1];
  } else {
    uint64_t placeholder = 0;
    [enc setBytes:&placeholder length:sizeof(placeholder) atIndex:1];
  }
  [enc setBuffer:resultBuf   offset:0 atIndex:2];
  [enc setBuffer:bookNextBuf offset:0 atIndex:3];
  [enc dispatchThreads:MTLSizeMake(1, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  Term result;
  memcpy(&result, [resultBuf contents], sizeof(Term));
  uint32_t out_u32;
  memcpy(&out_u32, [bookNextBuf contents], sizeof(uint32_t));
  *book_next_inout = (u64)out_u32;
  // Iter Z: kernel allocates compound-term cells in BOOK_HEAP via
  // aot_book_alloc.  Host runtime cnf/collapse walk HEAP (dyn heap),
  // so by default we migrate book->dyn before returning.  Atoms
  // (NUM/TEN/REF/ERA) pass through unchanged so the iter-D scalar-fold
  // shape stays zero-overhead.
  //
  // Env opt-out THVM_AOT_METAL_KEEP_BOOK=1 returns the book-rooted Term
  // verbatim so the iter Z+1 parallel collapse shader can walk it
  // directly.  Test/integration uses this gate to chain the two
  // kernels without bouncing through dyn heap.
  extern Term thvm_aot_migrate_book_to_dyn(Term root);
  uint8_t result_tag = (uint8_t)((result >> 56) & 0x7F);
  // Re-check env every call -- cached static would mean a later
  // SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "1"] inside the same
  // process (e.g., a TestReport mid-suite) wouldn't take effect.
  const char *keep_book_env = getenv("THVM_AOT_METAL_KEEP_BOOK");
  int keep_book = (keep_book_env != NULL && keep_book_env[0] == '1');
  if (!keep_book &&
      result_tag != 10 /* TAG_NUM */ && result_tag != 8 /* TAG_TEN */ &&
      result_tag != 11 /* TAG_REF */ && result_tag != 3 /* TAG_ERA */) {
    result = thvm_aot_migrate_book_to_dyn(result);
  }
  return result;
}

// === Iter Z+1: parallel cnf+collapse shader dispatch ============
//
// Walks the iter-Z output (a SUP-tree-rooted Term in BOOK_HEAP) by
// dispatching the static aot_ic_collapse PSO with grid = 2^depth.
// Each thread decodes tid into a binary path through the SUP-tree
// and drives its leaf to WHNF on-thread.  Returns leaves vector.
//
// `depth` is supplied by the caller after a host-side traversal of
// the SUP-tree (cheap: O(N) reads from book_heap to count SUP nodes).
// The host walks once to find max depth, then dispatches grid=2^depth.
// Threads whose tid bits select an ERA-pruned branch return ERA.

static id<MTLComputePipelineState> AOT_IC_COLLAPSE_PSO = nil;

static id<MTLComputePipelineState> aot_ic_collapse_pso(void) {
  if (AOT_IC_COLLAPSE_PSO != nil) return AOT_IC_COLLAPSE_PSO;
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) {
    if (metal_init() != 0) return nil;
  }
  // Load from the project's default.metallib, which already contains
  // aot_ic_collapse via the Makefile's wildcard shader compile.
  NSError *err = nil;
  NSURL *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:THVM_METAL_METALLIB]];
  id<MTLLibrary> lib =
      [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm aot-ic-collapse: load %s failed: %s\n",
            THVM_METAL_METALLIB,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn =
      [lib newFunctionWithName:@"aot_ic_collapse"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm aot-ic-collapse: function 'aot_ic_collapse' missing\n");
    return nil;
  }
  AOT_IC_COLLAPSE_PSO =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (AOT_IC_COLLAPSE_PSO == nil) {
    fprintf(stderr, "thvm aot-ic-collapse: pso failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  return AOT_IC_COLLAPSE_PSO;
}

// Returns N filled into out[0..N-1] (caller owns the buffer, must be
// >= 2^depth Terms).  Returns 0 on dispatch failure.
u64 thvm_aot_metal_ic_collapse(Term root, u32 depth,
                                Term *book_heap, u64 book_cells,
                                u64 *book_next_inout,
                                Term *out, u64 out_cap) {
  id<MTLComputePipelineState> pso = aot_ic_collapse_pso();
  if (pso == nil) return 0;
  if (depth > 30) {
    fprintf(stderr, "thvm aot-ic-collapse: depth %u exceeds 30 (>1B threads)\n",
            depth);
    return 0;
  }
  u64 n = 1ULL << depth;
  if (n > out_cap) {
    fprintf(stderr, "thvm aot-ic-collapse: n=%llu exceeds out_cap=%llu\n",
            (unsigned long long)n, (unsigned long long)out_cap);
    return 0;
  }

  if (*book_next_inout >> 32) {
    fprintf(stderr, "thvm aot-ic-collapse: book_next exceeds 32-bit range\n");
    return 0;
  }
  uint32_t book_next_u32 = (uint32_t)*book_next_inout;

  // Iter Z+2 step 5: per-thread book-heap arenas.  Pre-allocate
  // n * arena_size cells past iter Z's book_next.  Arena size
  // scales with available headroom; cap raised to 32K cells now
  // that BOOK_CAP=64M cells (512 MiB).  Earlier 1024-cell cap was
  // the bottleneck preventing V>=4 reductions from completing --
  // each per-thread reduction needs ~5K-20K cells for the redirect
  // cascade through DUP-LAM/DUP-SUP fires on shared cells.
  uint64_t headroom = (book_cells > book_next_u32)
      ? (book_cells - (uint64_t)book_next_u32 - 64ULL)
      : 0ULL;
  uint32_t arena_size = 32768u;
  if (n > 0 && headroom / n < arena_size) {
    arena_size = (uint32_t)(headroom / n);
  }
  if (arena_size < 32u) {
    fprintf(stderr,
        "thvm aot-ic-collapse: arena floor 32 unavailable "
        "(headroom %llu cells / %llu threads -> %u)\n",
        (unsigned long long)headroom, (unsigned long long)n, arena_size);
    return 0;
  }
  uint32_t arena_base = book_next_u32;

  id<MTLBuffer> heapBuf =
      aot_metal_heap_buf(book_heap, book_cells);
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:(NSUInteger)(n * sizeof(Term))
                                options:MTLResourceStorageModeShared];

  uint64_t root_u64 = (uint64_t)root;

  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:heapBuf     offset:0 atIndex:0];
  [enc setBytes:&root_u64    length:sizeof(uint64_t) atIndex:1];
  [enc setBuffer:resultBuf   offset:0 atIndex:2];
  [enc setBytes:&depth       length:sizeof(uint32_t) atIndex:3];
  [enc setBytes:&arena_base  length:sizeof(uint32_t) atIndex:4];
  [enc setBytes:&arena_size  length:sizeof(uint32_t) atIndex:5];
  // Threadgroup size: cap at 256 or n (whichever is smaller); Apple
  // GPU's max threads/threadgroup is 1024 but 256 is a sensible
  // default for thread-private-heavy kernels (our wnf stack is 256
  // Terms per thread = 2 KiB, well under the 32 KiB private budget).
  NSUInteger tg = (NSUInteger)((n < 256) ? n : 256);
  [enc dispatchThreads:MTLSizeMake(n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  // Iter Z+2 step 6: DO advance book_next past the arena range.
  // Without this, subsequent kernel-1 calls allocate into the
  // arena and overwrite cells that this call's intermediate IC
  // fires wrote with SUB bits (or substituted LAM/DUP cells in
  // iter Z's range that the next kernel-1 happens to overwrite
  // partially before its own writes complete).  Empirically:
  // skipping the advance caused V=4 to lose the SAT-True path
  // when run after V=2 + V=3 in the same wolframscript session
  // even though isolated V=4 worked.  Advancing trades book-heap
  // headroom (each collapse permanently consumes n*arena_size
  // cells) for cross-call correctness.  V=7 with the bench's
  // accumulated state still fits because BOOK_CAP=4M cells.
  *book_next_inout = (u64)arena_base + (u64)n * (u64)arena_size;

  memcpy(out, [resultBuf contents], (size_t)(n * sizeof(Term)));
  return n;
}

// === Iter Z+2 step 4: generic per-def runner ============================
//
// Replaces the per-def emit + xcrun metallib roundtrip (which produced
// ~10K-line shaders for V>=4 Church-bool formulas) with a single static
// shader that takes the def's book root Term as a constant.  One PSO
// across all defs; no per-def MSL compile.
//
// Caller passes the def's book-heap root term (DEFS[def_id]) plus an
// args array; the kernel builds APP(...root, args[0]..args[n-1])
// chain in book heap and runs wnf to WHNF.

static id<MTLComputePipelineState> AOT_IC_DEF_RUN_PSO = nil;

static id<MTLComputePipelineState> aot_ic_def_run_pso(void) {
  if (AOT_IC_DEF_RUN_PSO != nil) return AOT_IC_DEF_RUN_PSO;
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) {
    if (metal_init() != 0) return nil;
  }
  NSError *err = nil;
  NSURL *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:THVM_METAL_METALLIB]];
  id<MTLLibrary> lib =
      [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm aot-ic-def-run: load %s failed: %s\n",
            THVM_METAL_METALLIB,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn =
      [lib newFunctionWithName:@"aot_ic_def_run"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm aot-ic-def-run: function 'aot_ic_def_run' missing\n");
    return nil;
  }
  AOT_IC_DEF_RUN_PSO =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (AOT_IC_DEF_RUN_PSO == nil) {
    fprintf(stderr, "thvm aot-ic-def-run: pso failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  return AOT_IC_DEF_RUN_PSO;
}

// Run a def via the generic IC shader.  root = DEFS[def_id] (book
// term), args/n_args = caller-supplied arguments.  Returns the WHNF
// term (BOOK_HEAP-rooted unless KEEP_BOOK is off, in which case the
// caller migrates).  Single thread; same wnf state machine as iter Z's
// per-def emit, just at the cost of a runtime APP-chain build instead
// of compile-time inlining.
Term thvm_aot_metal_ic_def_run(Term root,
                                Term *args, u32 n_args,
                                Term *book_heap, u64 book_cells,
                                u64 *book_next_inout) {
  id<MTLComputePipelineState> pso = aot_ic_def_run_pso();
  if (pso == nil) return 0;

  if (*book_next_inout >> 32) {
    fprintf(stderr,
        "thvm aot-ic-def-run: book_next %llu exceeds 32-bit GPU range\n",
        (unsigned long long)*book_next_inout);
    return 0;
  }
  uint32_t book_next_u32 = (uint32_t)*book_next_inout;

  id<MTLBuffer> heapBuf =
      aot_metal_heap_buf(book_heap, book_cells);
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:sizeof(Term)
                                options:MTLResourceStorageModeShared];
  id<MTLBuffer> bookNextBuf =
      [METAL_DEVICE newBufferWithBytes:&book_next_u32
                                length:sizeof(uint32_t)
                               options:MTLResourceStorageModeShared];

  uint64_t root_u64 = (uint64_t)root;

  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:heapBuf     offset:0 atIndex:0];
  // Args buffer (small; setBytes avoids alloc).
  if (n_args > 0) {
    [enc setBytes:args length:n_args * sizeof(Term) atIndex:1];
  } else {
    uint64_t placeholder = 0;
    [enc setBytes:&placeholder length:sizeof(placeholder) atIndex:1];
  }
  [enc setBuffer:resultBuf   offset:0 atIndex:2];
  [enc setBuffer:bookNextBuf offset:0 atIndex:3];
  [enc setBytes:&root_u64    length:sizeof(uint64_t) atIndex:4];
  [enc setBytes:&n_args      length:sizeof(uint32_t) atIndex:5];
  [enc dispatchThreads:MTLSizeMake(1, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  Term result;
  memcpy(&result, [resultBuf contents], sizeof(Term));
  uint32_t out_u32;
  memcpy(&out_u32, [bookNextBuf contents], sizeof(uint32_t));
  *book_next_inout = (u64)out_u32;
  // Migrate book->dyn unless KEEP_BOOK env is set (chain to iter Z+1
  // collapse), mirroring the per-def wrapper's behavior.
  extern Term thvm_aot_migrate_book_to_dyn(Term root);
  uint8_t result_tag = (uint8_t)((result >> 56) & 0x7F);
  const char *keep_env = getenv("THVM_AOT_METAL_KEEP_BOOK");
  int keep_book = (keep_env != NULL && keep_env[0] == '1');
  if (!keep_book &&
      result_tag != 10 /* TAG_NUM */ && result_tag != 8 /* TAG_TEN */ &&
      result_tag != 11 /* TAG_REF */ && result_tag != 3 /* TAG_ERA */) {
    result = thvm_aot_migrate_book_to_dyn(result);
  }
  return result;
}

// === Lever 3: bitmask CNF eval kernel dispatch ==========================
//
// Bypasses the IC reduction pipeline for SAT-shaped problems.
// Caller passes packed pos/neg literal bitmasks per clause + var
// count; kernel evaluates the CNF at every assignment in [0, 2^V)
// and writes 1/0 per leaf.

static id<MTLComputePipelineState> AOT_CNF_BITMASK_PSO = nil;

static id<MTLComputePipelineState> aot_cnf_bitmask_pso(void) {
  if (AOT_CNF_BITMASK_PSO != nil) return AOT_CNF_BITMASK_PSO;
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) {
    if (metal_init() != 0) return nil;
  }
  NSError *err = nil;
  NSURL *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:THVM_METAL_METALLIB]];
  id<MTLLibrary> lib =
      [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm aot-cnf-bitmask: load %s failed: %s\n",
            THVM_METAL_METALLIB,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn =
      [lib newFunctionWithName:@"aot_cnf_bitmask"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm aot-cnf-bitmask: function 'aot_cnf_bitmask' missing\n");
    return nil;
  }
  AOT_CNF_BITMASK_PSO =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (AOT_CNF_BITMASK_PSO == nil) {
    fprintf(stderr, "thvm aot-cnf-bitmask: pso failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  return AOT_CNF_BITMASK_PSO;
}

// Returns n_leaves on success (== 1<<n_vars), 0 on failure.  Writes
// per-leaf 1/0 into out[0..n_leaves-1].
u64 thvm_aot_metal_cnf_bitmask(const uint32_t *clauses_pos,
                                const uint32_t *clauses_neg,
                                uint32_t n_clauses,
                                uint32_t n_vars,
                                uint32_t *out, u64 out_cap) {
  id<MTLComputePipelineState> pso = aot_cnf_bitmask_pso();
  if (pso == nil) return 0;
  if (n_vars > 30) {
    fprintf(stderr, "thvm aot-cnf-bitmask: n_vars %u exceeds 30 (>1B threads)\n",
            n_vars);
    return 0;
  }
  u64 n_leaves = 1ULL << n_vars;
  if (n_leaves > out_cap) {
    fprintf(stderr, "thvm aot-cnf-bitmask: n_leaves=%llu exceeds out_cap=%llu\n",
            (unsigned long long)n_leaves, (unsigned long long)out_cap);
    return 0;
  }

  id<MTLBuffer> posBuf =
      [METAL_DEVICE newBufferWithBytes:clauses_pos
                                length:n_clauses * sizeof(uint32_t)
                               options:MTLResourceStorageModeShared];
  id<MTLBuffer> negBuf =
      [METAL_DEVICE newBufferWithBytes:clauses_neg
                                length:n_clauses * sizeof(uint32_t)
                               options:MTLResourceStorageModeShared];
  id<MTLBuffer> resultBuf =
      [METAL_DEVICE newBufferWithLength:(NSUInteger)(n_leaves * sizeof(uint32_t))
                                options:MTLResourceStorageModeShared];

  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:posBuf    offset:0 atIndex:0];
  [enc setBuffer:negBuf    offset:0 atIndex:1];
  [enc setBytes:&n_clauses length:sizeof(uint32_t) atIndex:2];
  [enc setBuffer:resultBuf offset:0 atIndex:3];
  NSUInteger tg = (NSUInteger)((n_leaves < 256) ? n_leaves : 256);
  [enc dispatchThreads:MTLSizeMake(n_leaves, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  memcpy(out, [resultBuf contents], (size_t)(n_leaves * sizeof(uint32_t)));
  return n_leaves;
}

// === Path B: Survey Propagation (SP) iteration kernel ===================
//
// CNF survey propagation via factor graph message passing.  The host
// constructs CSR adjacency lists from the input CNF, allocates an
// edge-message buffer (eta), iteratively dispatches the aot_sp_iter
// kernel (one synchronous SP update over all edges), and applies
// damping + convergence check on host.  Returns the final eta vector
// after convergence or max-iters.
//
// Algorithm reference: Mezard-Parisi-Zecchina, "Survey propagation:
// an algorithm for satisfiability", arXiv cs/0212002.

static id<MTLComputePipelineState> AOT_SP_ITER_PSO = nil;

static id<MTLComputePipelineState> aot_sp_iter_pso(void) {
  if (AOT_SP_ITER_PSO != nil) return AOT_SP_ITER_PSO;
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) {
    if (metal_init() != 0) return nil;
  }
  NSError *err = nil;
  NSURL *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:THVM_METAL_METALLIB]];
  id<MTLLibrary> lib =
      [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm aot-sp-iter: load %s failed: %s\n",
            THVM_METAL_METALLIB,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"aot_sp_iter"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm aot-sp-iter: function 'aot_sp_iter' missing\n");
    return nil;
  }
  AOT_SP_ITER_PSO =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (AOT_SP_ITER_PSO == nil) {
    fprintf(stderr, "thvm aot-sp-iter: pso failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  return AOT_SP_ITER_PSO;
}

// Run SP to convergence (or until max_iters).  All host-side allocs
// are managed here; caller passes the formula as parallel arrays
// (one per edge: clause idx, var idx, sign) plus CSR adjacencies for
// clause->edges and var->edges.
//
// Returns 1 on convergence (max delta < threshold), 0 on max_iters
// reached without convergence, -1 on error.  Final eta written to
// out_eta[0..n_edges-1].
int thvm_aot_metal_sp_run(
    const uint32_t *edges_clause,   // [n_edges]
    const uint32_t *edges_var,      // [n_edges]
    const uint8_t  *edges_sign,     // [n_edges]
    const uint32_t *clause_edges_off,  // [n_clauses+1] CSR row ptrs
    const uint32_t *clause_edges_flat, // [n_edges] CSR col idx
    const uint32_t *var_edges_off,     // [n_vars+1]
    const uint32_t *var_edges_flat,    // [n_edges]
    uint32_t        n_edges,
    uint32_t        n_clauses,
    uint32_t        n_vars,
    uint32_t        max_iters,
    float           damping,    // alpha; 0=no update, 1=replace
    float           threshold,  // converge when max|new-old| < threshold
    float          *out_eta) {
  id<MTLComputePipelineState> pso = aot_sp_iter_pso();
  if (pso == nil) return -1;

  id<MTLBuffer> ec_buf = [METAL_DEVICE newBufferWithBytes:edges_clause
      length:n_edges*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> ev_buf = [METAL_DEVICE newBufferWithBytes:edges_var
      length:n_edges*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> es_buf = [METAL_DEVICE newBufferWithBytes:edges_sign
      length:n_edges*sizeof(uint8_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> co_buf = [METAL_DEVICE newBufferWithBytes:clause_edges_off
      length:(n_clauses+1)*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> cf_buf = [METAL_DEVICE newBufferWithBytes:clause_edges_flat
      length:n_edges*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> vo_buf = [METAL_DEVICE newBufferWithBytes:var_edges_off
      length:(n_vars+1)*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> vf_buf = [METAL_DEVICE newBufferWithBytes:var_edges_flat
      length:n_edges*sizeof(uint32_t) options:MTLResourceStorageModeShared];

  // Init eta_in randomly in [0, 0.1] (small, biased toward "no warning").
  // Two ping-pong buffers for synchronous SP updates.
  id<MTLBuffer> a_buf = [METAL_DEVICE newBufferWithLength:n_edges*sizeof(float)
      options:MTLResourceStorageModeShared];
  id<MTLBuffer> b_buf = [METAL_DEVICE newBufferWithLength:n_edges*sizeof(float)
      options:MTLResourceStorageModeShared];
  float *a = (float *)[a_buf contents];
  uint32_t seed = 0xDEADBEEFu;
  for (uint32_t i = 0; i < n_edges; i++) {
    seed = seed * 1664525u + 1013904223u;
    a[i] = (float)(seed >> 8) / (float)(1u << 24) * 0.1f;
  }

  uint32_t n_edges_u = n_edges;
  id<MTLBuffer> ne_buf = [METAL_DEVICE newBufferWithBytes:&n_edges_u
      length:sizeof(uint32_t) options:MTLResourceStorageModeShared];

  // Iterate.  Ping-pong between a_buf (eta_in) and b_buf (eta_out).
  id<MTLBuffer> in_buf  = a_buf;
  id<MTLBuffer> out_buf = b_buf;
  int converged = 0;
  uint32_t actual_iters = 0;
  for (uint32_t iter = 0; iter < max_iters; iter++) {
    actual_iters = iter + 1;
    id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
    id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
    [enc setComputePipelineState:pso];
    [enc setBuffer:ec_buf offset:0 atIndex:0];
    [enc setBuffer:ev_buf offset:0 atIndex:1];
    [enc setBuffer:es_buf offset:0 atIndex:2];
    [enc setBuffer:co_buf offset:0 atIndex:3];
    [enc setBuffer:cf_buf offset:0 atIndex:4];
    [enc setBuffer:vo_buf offset:0 atIndex:5];
    [enc setBuffer:vf_buf offset:0 atIndex:6];
    [enc setBuffer:in_buf  offset:0 atIndex:7];
    [enc setBuffer:out_buf offset:0 atIndex:8];
    [enc setBuffer:ne_buf  offset:0 atIndex:9];
    NSUInteger tg = (NSUInteger)((n_edges < 256) ? n_edges : 256);
    [enc dispatchThreads:MTLSizeMake(n_edges, 1, 1)
     threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
    [enc endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];

    // Damping + convergence check on host (small, <100K edges typical).
    float *in_p  = (float *)[in_buf  contents];
    float *out_p = (float *)[out_buf contents];
    float max_delta = 0.0f;
    for (uint32_t e = 0; e < n_edges; e++) {
      float updated = (1.0f - damping) * in_p[e] + damping * out_p[e];
      float delta   = updated - in_p[e];
      if (delta < 0.0f) delta = -delta;
      if (delta > max_delta) max_delta = delta;
      out_p[e] = updated;  // commit damped value into out
    }
    if (max_delta < threshold) {
      converged = 1;
      break;
    }

    // Swap buffers for next iter.
    id<MTLBuffer> tmp = in_buf;
    in_buf = out_buf;
    out_buf = tmp;
  }

  // Copy final values from in_buf (last update written to out_buf,
  // then loop ends -- but only if converged; otherwise in_buf has
  // the latest because we swap).  Actually after swap, in_buf is
  // the latest iter's output.  On convergence, we wrote into out_buf
  // (which is at this point the iteration's output buffer).
  float *final_p = converged ? (float *)[out_buf contents]
                              : (float *)[in_buf contents];
  memcpy(out_eta, final_p, n_edges * sizeof(float));

  fprintf(stderr, "[sp] iters=%u converged=%d\n", actual_iters, converged);
  return converged;
}

// === Path B step 2: per-variable surveys after SP convergence ============

static id<MTLComputePipelineState> AOT_SP_SURVEYS_PSO = nil;

static id<MTLComputePipelineState> aot_sp_surveys_pso(void) {
  if (AOT_SP_SURVEYS_PSO != nil) return AOT_SP_SURVEYS_PSO;
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) {
    if (metal_init() != 0) return nil;
  }
  NSError *err = nil;
  NSURL *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:THVM_METAL_METALLIB]];
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (lib == nil) return nil;
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"aot_sp_surveys"];
  if (mtlFn == nil) return nil;
  AOT_SP_SURVEYS_PSO =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  return AOT_SP_SURVEYS_PSO;
}

int thvm_aot_metal_sp_surveys(
    const uint32_t *var_edges_off, const uint32_t *var_edges_flat,
    const uint8_t  *edges_sign,
    const float    *eta,
    uint32_t n_edges, uint32_t n_vars,
    float *out_w_pos, float *out_w_neg, float *out_bias) {
  id<MTLComputePipelineState> pso = aot_sp_surveys_pso();
  if (pso == nil) return -1;

  id<MTLBuffer> vo = [METAL_DEVICE newBufferWithBytes:var_edges_off
      length:(n_vars+1)*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> vf = [METAL_DEVICE newBufferWithBytes:var_edges_flat
      length:n_edges*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> es = [METAL_DEVICE newBufferWithBytes:edges_sign
      length:n_edges*sizeof(uint8_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> et = [METAL_DEVICE newBufferWithBytes:eta
      length:n_edges*sizeof(float) options:MTLResourceStorageModeShared];
  id<MTLBuffer> wp = [METAL_DEVICE newBufferWithLength:n_vars*sizeof(float)
      options:MTLResourceStorageModeShared];
  id<MTLBuffer> wn = [METAL_DEVICE newBufferWithLength:n_vars*sizeof(float)
      options:MTLResourceStorageModeShared];
  id<MTLBuffer> bi = [METAL_DEVICE newBufferWithLength:n_vars*sizeof(float)
      options:MTLResourceStorageModeShared];
  id<MTLBuffer> nv = [METAL_DEVICE newBufferWithBytes:&n_vars
      length:sizeof(uint32_t) options:MTLResourceStorageModeShared];

  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:vo offset:0 atIndex:0];
  [enc setBuffer:vf offset:0 atIndex:1];
  [enc setBuffer:es offset:0 atIndex:2];
  [enc setBuffer:et offset:0 atIndex:3];
  [enc setBuffer:wp offset:0 atIndex:4];
  [enc setBuffer:wn offset:0 atIndex:5];
  [enc setBuffer:bi offset:0 atIndex:6];
  [enc setBuffer:nv offset:0 atIndex:7];
  NSUInteger tg = (NSUInteger)((n_vars < 256) ? n_vars : 256);
  [enc dispatchThreads:MTLSizeMake(n_vars, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  [cmd commit];
  [cmd waitUntilCompleted];

  memcpy(out_w_pos, [wp contents], n_vars * sizeof(float));
  memcpy(out_w_neg, [wn contents], n_vars * sizeof(float));
  memcpy(out_bias,  [bi contents], n_vars * sizeof(float));
  return 0;
}

// === Path B step 2: SP-DEC solve loop ====================================
//
// Top-level solver: iterates SP + decimation until the formula
// either solves (residual empty), contradicts (UNSAT), or becomes
// "easy enough" that we hand off to the bitmask kernel for the
// residual.  Returns 0 on SAT (out_assignment filled), -1 on UNSAT,
// 1 if we hit max-decimations without resolving.

#define SP_BITMASK_RESIDUAL_VARS 24   // residual <= 24 vars -> bitmask
#define SP_BIAS_THRESHOLD       0.01f // below this, formula is "easy"
#define SP_DEC_MAX_STEPS        2000  // cap

int thvm_aot_metal_sp_solve(
    const int32_t *cnf_lits,    // signed 1-based vars, flat
    const uint32_t *cnf_bounds, // [n_clauses+1] CSR-style
    uint32_t n_clauses,
    uint32_t n_vars,
    uint32_t sp_max_iters,
    float    damping,
    float    threshold,
    int8_t  *out_assignment) {  // [n_vars] -1/+1, or 0 if unset
  // Working state: per-variable assignment (0=unset, +1=T, -1=F);
  // per-clause active flag.  We don't physically remove clauses;
  // we mark them.  After each decimation, simplification removes
  // satisfied clauses + falsified literals.
  int8_t  *assign = (int8_t *)calloc(n_vars, sizeof(int8_t));
  uint8_t *clause_active = (uint8_t *)malloc(n_clauses);
  if (!assign || !clause_active) {
    free(assign); free(clause_active); return -2;
  }
  for (uint32_t c = 0; c < n_clauses; c++) clause_active[c] = 1;

  // We rebuild the edge arrays + CSR each decimation step from the
  // current active set.  Inefficient but simple.  Hot path is SP,
  // not the formula manipulation.

  for (uint32_t step = 0; step < SP_DEC_MAX_STEPS; step++) {
    // Build active edges.
    uint32_t n_active_clauses = 0;
    uint32_t n_edges = 0;
    for (uint32_t c = 0; c < n_clauses; c++) {
      if (!clause_active[c]) continue;
      // Count active lits in this clause (those whose var isn't
      // already falsified by assignment).
      uint32_t cl_start = cnf_bounds[c], cl_end = cnf_bounds[c + 1];
      uint32_t live = 0;
      uint32_t sat_lit = 0;
      for (uint32_t k = cl_start; k < cl_end; k++) {
        int32_t lit = cnf_lits[k];
        uint32_t v = (lit > 0) ? (uint32_t)(lit - 1) : (uint32_t)(-lit - 1);
        int8_t want = (lit > 0) ? 1 : -1;
        if (assign[v] == want) { sat_lit = 1; break; }
        if (assign[v] == 0) live++;
        // assign[v] == -want means falsified literal; skip it
      }
      if (sat_lit) { clause_active[c] = 0; continue; }
      if (live == 0) {
        // Empty clause -> UNSAT
        free(assign); free(clause_active); return -1;
      }
      n_active_clauses++;
      n_edges += live;
    }

    if (n_active_clauses == 0) {
      // SAT -- copy assignment, set unfixed to +1 (default).
      for (uint32_t v = 0; v < n_vars; v++) {
        out_assignment[v] = assign[v] == 0 ? 1 : assign[v];
      }
      free(assign); free(clause_active);
      return 0;
    }

    // Count remaining unfixed variables.
    uint32_t n_unfixed = 0;
    for (uint32_t v = 0; v < n_vars; v++) if (assign[v] == 0) n_unfixed++;

    // If residual is small enough, hand off to bitmask.
    if (n_unfixed <= SP_BITMASK_RESIDUAL_VARS) {
      // Build a smaller CNF for the residual: remap unfixed vars to
      // dense indices, encode active clauses (with falsified lits
      // dropped).  Run bitmask, find first SAT assignment, fill in
      // the original var indices.
      int32_t *var_remap = (int32_t *)malloc(n_vars * sizeof(int32_t));
      uint32_t *unfixed_orig = (uint32_t *)malloc(n_unfixed * sizeof(uint32_t));
      uint32_t r = 0;
      for (uint32_t v = 0; v < n_vars; v++) {
        if (assign[v] == 0) {
          var_remap[v] = (int32_t)r;
          unfixed_orig[r] = v;
          r++;
        } else {
          var_remap[v] = -1;
        }
      }
      // Build pos/neg bitmasks per active clause.
      uint32_t *pos_masks = (uint32_t *)malloc(n_active_clauses * sizeof(uint32_t));
      uint32_t *neg_masks = (uint32_t *)malloc(n_active_clauses * sizeof(uint32_t));
      uint32_t ci = 0;
      for (uint32_t c = 0; c < n_clauses; c++) {
        if (!clause_active[c]) continue;
        uint32_t pm = 0, nm = 0;
        for (uint32_t k = cnf_bounds[c]; k < cnf_bounds[c + 1]; k++) {
          int32_t lit = cnf_lits[k];
          uint32_t v = (lit > 0) ? (uint32_t)(lit - 1) : (uint32_t)(-lit - 1);
          if (assign[v] != 0) continue;  // falsified, drop
          uint32_t bit = 1u << var_remap[v];
          if (lit > 0) pm |= bit; else nm |= bit;
        }
        pos_masks[ci] = pm;
        neg_masks[ci] = nm;
        ci++;
      }
      uint64_t n_leaves = 1ULL << n_unfixed;
      extern u64 thvm_aot_metal_cnf_bitmask(const uint32_t *, const uint32_t *,
                                              uint32_t, uint32_t,
                                              uint32_t *, u64);
      uint32_t *res = (uint32_t *)malloc(n_leaves * sizeof(uint32_t));
      u64 nout = thvm_aot_metal_cnf_bitmask(pos_masks, neg_masks,
                                             n_active_clauses, n_unfixed,
                                             res, n_leaves);
      free(pos_masks); free(neg_masks);
      if (nout == 0) {
        free(res); free(var_remap); free(unfixed_orig);
        free(assign); free(clause_active);
        return -2;
      }
      // Find first SAT assignment.
      int found = -1;
      for (uint64_t a = 0; a < n_leaves; a++) {
        if (res[a]) { found = (int)a; break; }
      }
      free(res);
      if (found < 0) {
        free(var_remap); free(unfixed_orig);
        free(assign); free(clause_active);
        return -1;  // UNSAT
      }
      for (uint32_t k = 0; k < n_unfixed; k++) {
        uint32_t v = unfixed_orig[k];
        assign[v] = (((uint32_t)found >> k) & 1u) ? 1 : -1;
      }
      free(var_remap); free(unfixed_orig);
      for (uint32_t v = 0; v < n_vars; v++) {
        out_assignment[v] = assign[v] == 0 ? 1 : assign[v];
      }
      free(assign); free(clause_active);
      return 0;
    }

    // Build CSR + edge arrays for the active formula.
    uint32_t *edges_clause = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
    uint32_t *edges_var    = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
    uint8_t  *edges_sign   = (uint8_t  *)malloc(n_edges * sizeof(uint8_t));
    uint32_t *clause_off   = (uint32_t *)malloc((n_active_clauses + 1) * sizeof(uint32_t));
    uint32_t *clause_flat  = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
    uint32_t *var_off      = (uint32_t *)malloc((n_vars + 1) * sizeof(uint32_t));
    uint32_t *var_flat     = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
    float    *eta_out      = (float    *)malloc(n_edges * sizeof(float));
    float    *w_pos        = (float    *)malloc(n_vars * sizeof(float));
    float    *w_neg        = (float    *)malloc(n_vars * sizeof(float));
    float    *bias         = (float    *)malloc(n_vars * sizeof(float));

    uint32_t e = 0;
    uint32_t ac = 0;
    clause_off[0] = 0;
    for (uint32_t c = 0; c < n_clauses; c++) {
      if (!clause_active[c]) continue;
      for (uint32_t k = cnf_bounds[c]; k < cnf_bounds[c + 1]; k++) {
        int32_t lit = cnf_lits[k];
        uint32_t v = (lit > 0) ? (uint32_t)(lit - 1) : (uint32_t)(-lit - 1);
        if (assign[v] != 0) continue;
        edges_clause[e] = ac;
        edges_var[e]    = v;
        edges_sign[e]   = (lit > 0) ? 0u : 1u;
        clause_flat[e]  = e;
        e++;
      }
      ac++;
      clause_off[ac] = e;
    }
    // Build var->edges CSR.
    for (uint32_t v = 0; v <= n_vars; v++) var_off[v] = 0;
    for (uint32_t ee = 0; ee < n_edges; ee++) var_off[edges_var[ee] + 1]++;
    for (uint32_t v = 1; v <= n_vars; v++) var_off[v] += var_off[v - 1];
    uint32_t *vc = (uint32_t *)malloc(n_vars * sizeof(uint32_t));
    for (uint32_t v = 0; v < n_vars; v++) vc[v] = var_off[v];
    for (uint32_t ee = 0; ee < n_edges; ee++) {
      var_flat[vc[edges_var[ee]]++] = ee;
    }
    free(vc);

    // Run SP.
    int rc = thvm_aot_metal_sp_run(
        edges_clause, edges_var, edges_sign,
        clause_off, clause_flat, var_off, var_flat,
        n_edges, n_active_clauses, n_vars,
        sp_max_iters, damping, threshold, eta_out);
    if (rc < 0) {
      free(edges_clause); free(edges_var); free(edges_sign);
      free(clause_off); free(clause_flat); free(var_off); free(var_flat);
      free(eta_out); free(w_pos); free(w_neg); free(bias);
      free(assign); free(clause_active); return -2;
    }

    // Compute surveys.
    rc = thvm_aot_metal_sp_surveys(var_off, var_flat, edges_sign,
                                     eta_out, n_edges, n_vars,
                                     w_pos, w_neg, bias);
    if (rc < 0) {
      free(edges_clause); free(edges_var); free(edges_sign);
      free(clause_off); free(clause_flat); free(var_off); free(var_flat);
      free(eta_out); free(w_pos); free(w_neg); free(bias);
      free(assign); free(clause_active); return -2;
    }

    // Find max-bias unfixed variable.
    int32_t best_v = -1;
    float best_bias = SP_BIAS_THRESHOLD;
    for (uint32_t v = 0; v < n_vars; v++) {
      if (assign[v] != 0) continue;
      if (bias[v] > best_bias) {
        best_bias = bias[v];
        best_v = (int32_t)v;
      }
    }
    int8_t fix_sign = 0;
    if (best_v >= 0) {
      fix_sign = (w_pos[best_v] > w_neg[best_v]) ? 1 : -1;
      assign[best_v] = fix_sign;
    } else {
      // No biased variable; fix the first unfixed to +1 (greedy).
      for (uint32_t v = 0; v < n_vars; v++) {
        if (assign[v] == 0) { assign[v] = 1; best_v = (int32_t)v; fix_sign = 1; break; }
      }
    }

    free(edges_clause); free(edges_var); free(edges_sign);
    free(clause_off); free(clause_flat); free(var_off); free(var_flat);
    free(eta_out); free(w_pos); free(w_neg); free(bias);
  }

  free(assign); free(clause_active);
  return 1;  // gave up
}

Backend METAL_BACKEND = {
  .id              = 2,
  .view_aware      = 1,
  .id              = 2,
  .view_aware      = 1,   // metal_dispatch_kernel pre-materializes
                          // non-contig inputs into temp Metal bufs
                          // (host-side strided copy via inlined
                          // view_strided_index), so f3 view-only
                          // aliases now flow through Metal correctly.
  .init            = metal_init,
  .shutdown        = metal_shutdown,
  .buf_alloc       = metal_buf_alloc,
  .buf_free        = metal_buf_free,
  .buf_incref      = metal_buf_incref,
  .buf_decref      = metal_buf_decref,
  .buf_read        = metal_buf_read,
  .buf_write       = metal_buf_write,
  .buf_copy        = metal_buf_copy,
  .buf_refcount        = thvm_metal_buf_refcount,
  .buf_freelist_push   = thvm_metal_buf_freelist_push,
  .buf_freelist_remove = thvm_metal_buf_freelist_remove,
  .dispatch_begin  = metal_dispatch_begin,
  .dispatch_flush  = metal_dispatch_flush,
  .dispatch_end    = metal_dispatch_end,
  .dispatch_kernel = metal_dispatch_kernel,
};
