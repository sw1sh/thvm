// backend/metal/_.m -- Metal backend, Objective-C edition.
//
// Compiled separately from the single-TU C runtime; the umbrella
// src/thvm.c includes thvm.h which forward-declares METAL_BACKEND
// (extern Backend), and links this .o under -DTHVM_HAS_METAL.
//
// Includes the runtime header for type definitions (Backend,
// KernelEntry, UOP_* enums).  thvm.h is C-only but compiles
// cleanly under Objective-C / ARC.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // iter X: stat() for persistent metallib disk cache
#include <sys/types.h>
#include <sys/mman.h>   // madvise(MADV_WILLNEED): pipeline the zero-copy wrap fault-in
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>    // parallel page-fault of evicted zero-copy wrap pages
#include <dlfcn.h>      // dladdr: locate default.metallib next to the dylib

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
// Pipelined flush: a committed-but-not-yet-waited command buffer.  A realize-
// boundary flush commits its batch and returns WITHOUT blocking, so the CPU can
// encode the next realize while the GPU runs this one (the queue executes
// committed buffers in order, so the next realize's kernels still see this
// one's outputs without a CPU sync).  The deferred-decref free of THIS batch's
// buffers waits until this buffer completes -- which the NEXT flush forces
// before it frees -- so a buffer is never freed while the GPU still reads it.
// A host read (buf_read) or an explicit drain waits on the pending buffer.
static id<MTLCommandBuffer> METAL_PENDING_CMD = nil;
static u32                 *METAL_PENDING_DECREFS = NULL;
static u32                  METAL_PENDING_DECREF_LEN = 0;
static u32                  METAL_PENDING_DECREF_CAP = 0;

static void metal_buf_decref(u32 buf_id);
static void metal_buf_free(u32 buf_id);
static void metal_mps_wcache_reset(void);
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

// Per-op GPU profiling.  When enabled, batching is disabled in
// metal_dispatch_begin so every kernel dispatch gets its own command
// buffer; metal_submit_if_standalone then attributes that buffer's
// [GPUEndTime]-[GPUStartTime] to METAL_PEROP_CUR_KID, the kid set at the
// top of metal_dispatch_kernel.  The result is a real per-kernel GPU-us
// breakdown (vs. the batched path, where one flush covers ~25 kernels).
// Costs more dispatch overhead -- profile only.
//
// Triggered by THVM_METAL_PROFILE_PEROP=1 OR by THVM_KERNEL_PROFILE=N:
// the kernel-profile dump prints a gpu_us column, and without per-op
// command buffers every batched-replay kernel would get the same
// wall/n_ops average there -- useless for ranking.  Routing the profile
// run through this path makes those numbers TRUE per-kernel GPU times.
static int metal_perop_enabled(void) {
  static int known = 0;
  static int enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_METAL_PROFILE_PEROP");
    enabled = (e != NULL && e[0] == '1') || cg_profile_kernel_enabled();
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

// Resolve the metallib to load.  THVM_METAL_METALLIB is a CWD-relative
// dev path ("build/default.metallib") that works when running from the
// repo root.  In an installed paclet the CWD is arbitrary, so fall back
// to a "default.metallib" sitting next to this dylib -- `make wl-mac`
// ships one there.  dladdr on this very function yields the dylib path.
// Returns a pointer to a static buffer (single-threaded init path).
static const char *metal_metallib_path(void) {
  if (access(THVM_METAL_METALLIB, R_OK) == 0) return THVM_METAL_METALLIB;
  Dl_info info;
  static char sibling[1024];
  if (dladdr((const void *)&metal_metallib_path, &info) && info.dli_fname) {
    const char *slash = strrchr(info.dli_fname, '/');
    if (slash) {
      size_t dirlen = (size_t)(slash - info.dli_fname) + 1;
      if (dirlen + sizeof("default.metallib") <= sizeof(sibling)) {
        memcpy(sibling, info.dli_fname, dirlen);
        strcpy(sibling + dirlen, "default.metallib");
        if (access(sibling, R_OK) == 0) return sibling;
      }
    }
  }
  return THVM_METAL_METALLIB;  // last resort: let newLibraryWithURL report it
}

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

static u64 METAL_FLUSH_COMMITS;       // batched command-buffer commits+waits
static u64 METAL_STANDALONE_COMMITS;  // per-kernel standalone commits+waits
static u64 METAL_FLUSH_WAIT_US;       // total wall spent in commit+waitUntilCompleted

// Pipelining is opt-in (THVM_METAL_PIPELINE).  Default OFF preserves the
// commit+wait-per-flush behaviour; the FLUX session turns it ON (fxBoundMemory)
// to overlap CPU encode of realize N+1 with GPU exec of realize N.
static int metal_pipeline_enabled(void) {
  static int known = 0, on = 0;
  if (!known) { char const *e = getenv("THVM_METAL_PIPELINE");
                on = (e != NULL && e[0] == '1'); known = 1; }
  return on;
}

// Run the deferred decrefs that were captured for the just-completed buffer.
static void metal_run_pending_decrefs(void) {
  u32 n = METAL_PENDING_DECREF_LEN;
  METAL_PENDING_DECREF_LEN = 0;
  for (u32 i = 0; i < n; i++) metal_buf_decref(METAL_PENDING_DECREFS[i]);
}

// Wait on the pending (committed-but-unwaited) buffer, if any, then free the
// buffers that were deferred while it was in flight.  No-op when nothing is
// pending.  Called before any host read, before freeing the current batch's
// buffers, and at shutdown.
static void metal_drain_pending(void) {
  if (METAL_PENDING_CMD != nil) {
    u64 _t0 = cg_now_us();
    [METAL_PENDING_CMD waitUntilCompleted];
    METAL_FLUSH_WAIT_US += cg_now_us() - _t0;
    metal_record_gpu_time(METAL_PENDING_CMD);
    METAL_PENDING_CMD = nil;
    metal_run_pending_decrefs();
  }
}

static void metal_dispatch_flush(void) {
  id<MTLCommandBuffer> cmd = METAL_BATCH_CMD;
  METAL_BATCH_CMD = nil;
  if (metal_pipeline_enabled()) {
    // Pipelined: drain the PREVIOUS pending buffer (freeing its deferred
    // buffers now that the GPU is past them), then commit THIS batch WITHOUT
    // blocking and stash it + its deferred-decref list as the new pending.
    metal_drain_pending();
    if (cmd != nil) {
      [cmd commit];
      METAL_FLUSH_COMMITS++;
      METAL_PENDING_CMD = cmd;
      // Move the current deferred-decref list into the pending slot (it must
      // not be freed until THIS buffer completes -- the next drain).
      u32 n = METAL_DEFER_DECREF_LEN;
      if (n > METAL_PENDING_DECREF_CAP) {
        METAL_PENDING_DECREFS = (u32 *)realloc(METAL_PENDING_DECREFS,
                                               (size_t)n * sizeof(u32));
        METAL_PENDING_DECREF_CAP = METAL_PENDING_DECREFS ? n : 0;
      }
      if (METAL_PENDING_DECREFS != NULL) {
        memcpy(METAL_PENDING_DECREFS, METAL_DEFER_DECREFS, (size_t)n * sizeof(u32));
        METAL_PENDING_DECREF_LEN = n;
      } else {
        // realloc failed: fall back to freeing now (after a wait for safety).
        [cmd waitUntilCompleted];
        METAL_PENDING_CMD = nil;
        for (u32 i = 0; i < n; i++) metal_buf_decref(METAL_DEFER_DECREFS[i]);
      }
      METAL_DEFER_DECREF_LEN = 0;
      METAL_DEFER_DECREF_BYTES = 0;
    }
    metal_record_memory_peak();
    return;
  }
  // Non-pipelined (default): commit + wait, then free deferred buffers.
  if (cmd != nil) {
    u64 _t0 = cg_now_us();
    [cmd commit];
    [cmd waitUntilCompleted];
    METAL_FLUSH_WAIT_US += cg_now_us() - _t0;
    metal_record_gpu_time(cmd);
    METAL_FLUSH_COMMITS++;
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

static u64 METAL_FLUSH_FROM_REALIZE_END;
static u64 METAL_FLUSH_FROM_BUF_READ;

static void metal_dispatch_end(void) {
  if (METAL_BATCH_DEPTH == 0) return;
  METAL_BATCH_DEPTH--;
  if (METAL_BATCH_DEPTH == 0) {
    if (METAL_BATCH_CMD != nil) METAL_FLUSH_FROM_REALIZE_END++;
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
  METAL_STANDALONE_COMMITS++;
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
// Persistent on-disk .metallib (MSL->AIR) cache counters.  This cache is
// upstream of the PSO cache: a hit loads precompiled library bytes via
// newLibraryWithData: and skips the ~1.5s-per-kernel MSL->AIR frontend that
// newLibraryWithSource: re-runs on every cold start (the dominant FLUX cold
// cost).  See metal_lib_for_src / metal_cgs_compile below.
static u64 METAL_LIB_CACHE_HITS;
static u64 METAL_LIB_CACHE_MISSES;
static u64 METAL_LIB_CACHE_BYTES_R;
static u64 METAL_LIB_CACHE_BYTES_W;
// Zero-copy disk-mmap wrap counters (THVM_METAL_PSO_CACHE_STATS dump): how
// many weights wrapped, total bytes faulted resident, and the wall time the
// synchronous fault-in loop cost.  This is the cold-start "weight realization"
// the FLUX session pays at first forward -- measured, not guessed.
static u64 METAL_ZC_WRAPS;
static u64 METAL_ZC_FAULT_BYTES;
static u64 METAL_ZC_FAULT_US;
// Sub-phase breakdown of the zero-copy fault-in (THVM_ZC_PHASE_STATS): which of
// madvise / mincore-scan / page-touch / newBufferWithBytesNoCopy dominates.
static u64 METAL_ZC_MADV_US;
static u64 METAL_ZC_MINCORE_US;
static u64 METAL_ZC_TOUCH_US;
static u64 METAL_ZC_WRAP_US;
static u64 METAL_ZC_TOUCHED_PAGES;
fn void thvm_metal_jit_counters_reset(void);
static void metal_pso_cache_init(void);

static int metal_init(void) {
  // Idempotent re-init: a prior metal_init already created the device,
  // queue, and loaded the metallib, and metal_shutdown deliberately kept
  // them alive.  Skip the (disk) metallib reload and the banner print --
  // just re-arm the per-session counters + PSO cache dir.  This makes
  // TInit-per-episode cheap + quiet on Metal (issue #1, Metal follow-up).
  if (METAL_DEVICE != nil && METAL_QUEUE != nil && METAL_LIB != nil) {
    METAL_FREELIST_LEN = 0;
    METAL_BATCH_CMD    = nil;
    METAL_BATCH_DEPTH  = 0;
    METAL_DEFER_DECREF_LEN = 0;
    METAL_DEFER_DECREF_BYTES = 0;
    METAL_PEAK_LIVE_BYTES = 0;
    METAL_PEAK_RETAINED_BYTES = 0;
    METAL_PEAK_DEFERRED_BYTES = 0;
    METAL_GPU_US_TOTAL = 0;
    METAL_GPU_FLUSH_COUNT = 0;
    metal_mps_wcache_reset();
    thvm_metal_jit_counters_reset();
    metal_pso_cache_init();
    return 0;
  }
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
  const char *metallib_path = metal_metallib_path();
  NSURL    *libURL = [NSURL fileURLWithPath:
      [NSString stringWithUTF8String:metallib_path]];
  METAL_LIB = [METAL_DEVICE newLibraryWithURL:libURL error:&err];
  if (METAL_LIB == nil) {
    fprintf(stderr, "thvm: metal_init -- failed to load metallib at %s: %s\n",
            metallib_path,
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    METAL_QUEUE  = nil;
    METAL_DEVICE = nil;
    return -1;
  }
  fprintf(stderr, "thvm: metal_init -- device: %s; metallib: %s (%lu function%s)\n",
          [[METAL_DEVICE name] UTF8String],
          metallib_path,
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
  // jit_pinned: STICKY hold by an active JIT capture.  `preserved` is
  // cleared at end-of-realize (thvm_metal_buf_clear_preserved), but a
  // capture's recorded buffers must survive every sub-realize's pool
  // rollback AND every replay.  Without this a freelist-pushed captured
  // buf gets popped+memset by a later alloc, so replay reads zeroed/
  // overwritten contents.  Mirror of CpuBuf.jit_pinned / CudaBuf.jit_pinned;
  // cleared only on jit_capture_drop.
  u8            jit_pinned;
  // borrowed: this MTLBuffer wraps host pages we do NOT own (a disk-mmap
  // safetensors weight wrapped via newBufferWithBytesNoCopy + deallocator:nil
  // -- see thvm_metal_buf_wrap_external).  The underlying bytes belong to the
  // CPU-side DiskMap mmap; freeing this slot must only drop the MTLBuffer's
  // ARC ref (which, with deallocator:nil, leaves the mmap pages untouched) and
  // must NEVER park the buffer on the recycle freelist (freelist_try_pop would
  // memset + hand the borrowed pages out as a fresh tensor).  Mirror of
  // CpuBuf.owns_data == 0 for the external/disk case.
  u8            borrowed;
  // byte_offset: the within-buffer byte offset at which this tensor's element
  // 0 lives.  Nonzero ONLY for a borrowed wrap of a disk-mmap weight: the
  // MTLBuffer is wrapped over the page-aligned DiskMap base, so the weight's
  // first byte sits `minor` bytes in (minor = file byte_offset % PAGESIZE).
  // Every kernel input bind applies this via [enc setBuffer:buf offset:..],
  // so the kernel's contiguous index 0 lands on the weight -- the codegen
  // never sees the page-alignment padding.  0 for ordinary device buffers.
  u64           byte_offset;
  // host_base: the page-aligned host pointer a borrowed wrap covers (the
  // DiskMap base passed to newBufferWithBytesNoCopy).  Lets wrap_external
  // DEDUP: a repeat wrap of the same mmap region returns the existing slot
  // (incref) instead of creating a second MTLBuffer over the same pages.
  // Without this, re-realizing a weight (eager forward, per-block replay)
  // wires the same 7.75GB into the GPU page table again and again -- the
  // 100GB+ accumulation blowup.  NULL for ordinary device buffers.
  void         *host_base;
  // owns_data: 1 for a buffer this slot allocated (newBufferWithLength) and
  // must release; 0 for an arena VIEW (shares another slot's MTLBuffer at a
  // byte_offset -- the arena slot owns it, the view must NOT nil .buf or it
  // drops the shared MTLBuffer out from under every sibling view).  Borrowed
  // wraps own their wrapper object (deallocator:nil leaves the mmap pages),
  // so they keep owns_data == 1.  Mirror of CudaBuf.owns_data
  // (backend/cuda/buf_alloc.c:65,98) and CpuBuf.owns_data.
  u8            owns_data;
  // parent_buf_id: for an arena VIEW, the slot that owns the shared
  // MTLBuffer.  Each view increfs its parent at creation and the parent is
  // decref'd when the view is freed, so the arena allocation outlives every
  // slice and is released only when its own refcount hits zero.  Mirror:
  // CudaBuf.parent_buf_id (backend/cuda/buf_alloc.c:116-124,
  // buf_free.c:15,28-31) and tinygrad schedule/memory.py:59 (each buffer
  // becomes a SLICE into the shared arena).  0 for non-views.
  u32           parent_buf_id;
  // skip_freelist: never park this slot on the recycle freelist on decref-
  // to-zero.  Set on the arena allocation itself: a per-realize arena is
  // sized to ONE pass's working set and freed wholesale at end-of-realize;
  // parking it would let a later pass's best-fit pop a huge slot for a tiny
  // request (or leak it while every realize allocs a fresh arena).  Mirror
  // of CudaBuf.skip_freelist (materialize.c arena_ensure) and CpuBuf.
  u8            skip_freelist;
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
    // A borrowed wrapper holds NO device-owned bytes -- it aliases host mmap
    // pages already resident (and accounted) on the CPU side.  Counting it
    // toward the device live/retained footprint would double-count the
    // weight and falsely trip the THVM_MAX_LIVE_BYTES ceiling.
    if (METAL_BUFS[i].borrowed) continue;
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
  if (METAL_BUFS[buf_id].jit_pinned) return 0;   // held by an active JIT capture
  if (METAL_BUFS[buf_id].borrowed)   return 0;   // wraps mmap pages we don't own
  // Never recycle an arena VIEW (it owns no storage -- its .buf is the parent
  // arena's MTLBuffer at a byte_offset; a freelist pop would memset+rehand the
  // shared bytes as a fresh tensor) nor the arena allocation itself (sized to
  // one realize's working set, freed wholesale at end-of-realize; parking it
  // would let a later pass's best-fit snag a huge slot for a tiny request, or
  // leak it).  Mirror of CudaBuf.skip_freelist (materialize.c arena_ensure)
  // and the owns_data == 0 view guard.
  if (METAL_BUFS[buf_id].skip_freelist) return 0;
  if (!METAL_BUFS[buf_id].owns_data)    return 0;
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
  METAL_BUFS[id].borrowed  = 0;
  METAL_BUFS[id].byte_offset = 0;
  METAL_BUFS[id].host_base = NULL;
  METAL_BUFS[id].owns_data = 1;
  METAL_BUFS[id].parent_buf_id = 0;
  METAL_BUFS[id].skip_freelist = 0;
  if (METAL_BUFS[id].buf == nil) {
    fprintf(stderr, "thvm: metal_buf_alloc -- failed to allocate %llu bytes\n",
            (unsigned long long)nbytes);
    METAL_BUFS[id].refcount = 0;
    return 0;
  }
  metal_record_memory_peak();
  return id;
}

// Find (or grow into) a free METAL_BUFS slot.  Mirrors the slot-reuse scan
// in metal_buf_alloc / thvm_metal_buf_wrap_external.  Returns 0 if the table
// is full.
static u32 metal_buf_grab_slot(void) {
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf == nil && METAL_BUFS[i].refcount == 0) return i;
  }
  if (METAL_BUFS_NEXT >= METAL_BUFS_CAP) {
    fprintf(stderr, "thvm: metal_buf_grab_slot -- buffer table full\n");
    return 0;
  }
  return METAL_BUFS_NEXT++;
}

// Non-static handle for the per-realize arena planner (materialize.c, a
// separate TU): allocate a plain device buffer of `nbytes` and mark it as
// the arena ALLOCATION -- skip_freelist so end-of-realize frees it wholesale
// rather than parking a one-pass-sized slot for a later best-fit to snag.
// Returns the buf_id (0 on failure).  Mirror of materialize.c's CUDA arena
// branch (cuda_buf_alloc + CUDA_BUFS[id].skip_freelist = 1) and
// tinygrad schedule/memory.py:56 (UOp.new_buffer for the shared arena).
u32 thvm_metal_buf_arena_alloc(u64 nbytes) {
  u32 id = metal_buf_alloc(nbytes);
  if (id == 0) return 0;
  METAL_BUFS[id].skip_freelist = 1;
  return id;
}

// Arena VIEW: a NEW METAL_BUFS slot whose .buf is the SAME id<MTLBuffer> as
// METAL_BUFS[arena_buf_id].buf (shared, ARC-retained by the strong store), at
// .byte_offset = offset.  owns_data = 0 so metal_buf_free never frees the
// shared allocation through the view; parent_buf_id ties the view's lifetime
// to the arena, increfing the parent so the allocation outlives every slice
// and is released only when its own refcount reaches zero.  Every kernel bind
// (input AND output) applies byte_offset, so the slice reads/writes exactly
// its window of the shared buffer; Apple's hazard tracking serialises two
// accesses that alias WITHIN one MTLBuffer, which is what makes ICB-replayed
// buffer RECYCLES correct (a separate-MTLBuffer recycle is not ordered by the
// per-command [cmd setBarrier] on Apple9 -- the whole reason for the arena).
// Mirror: backend/cuda/buf_alloc.c:116-124 (cuda_buf_alloc_arena_view) and
// tinygrad ops_metal.py:192 (_offset returns MetalBuffer(buf.buf, size,
// offset)) + schedule/memory.py:59 (SLICE into the shared arena).
u32 thvm_metal_buf_arena_view(u32 arena_buf_id, u64 offset, u64 nbytes) {
  if (arena_buf_id == 0 || arena_buf_id >= METAL_BUFS_NEXT) return 0;
  id<MTLBuffer> arena = METAL_BUFS[arena_buf_id].buf;
  if (arena == nil) return 0;
  // Zero the slot's window before handing it out: a previous lifetime's bytes
  // still occupy it (the arena planner only tracks block ownership, not
  // zeroing), and kernels that accumulate (REDUCE_ADD) depend on a zero
  // start.  Mirror of materialize.c's memset (CPU) / cuMemsetD8 (CUDA) before
  // the view alloc.  Shared storage is host-visible (MTLResourceStorageMode-
  // Shared), so a plain memset through [arena contents] is correct and cheap.
  memset((char *)[arena contents] + offset, 0, (size_t)nbytes);
  u32 id = metal_buf_grab_slot();
  if (id == 0) return 0;
  METAL_BUFS[id].buf           = arena;   // shared; ARC retains via this store
  METAL_BUFS[id].nbytes        = nbytes;
  METAL_BUFS[id].refcount      = 1;
  METAL_BUFS[id].preserved     = 0;
  METAL_BUFS[id].jit_pinned    = 0;
  METAL_BUFS[id].borrowed      = 0;
  METAL_BUFS[id].byte_offset   = offset;
  METAL_BUFS[id].host_base     = NULL;
  METAL_BUFS[id].owns_data     = 0;       // view: never frees the shared buf
  METAL_BUFS[id].parent_buf_id = arena_buf_id;
  METAL_BUFS[id].skip_freelist = 0;
  METAL_BUFS[arena_buf_id].refcount++;    // keep the arena alive for the view
  metal_record_memory_peak();
  return id;
}

// Drop the producer reference the arena allocation holds (the +1 from
// thvm_metal_buf_arena_alloc).  Called by the per-realize planner at
// end-of-pass: each live view still holds a ref (incref'd in
// thvm_metal_buf_arena_view), so the allocation survives until the last view
// releases, then frees wholesale (skip_freelist keeps it off the recycle
// list).  Mirror of materialize.c's cpu_buf_decref / cuda_buf_decref of
// ARENA_BUF_ID at end-of-pass.
void thvm_metal_buf_arena_release(u32 arena_buf_id) {
  if (arena_buf_id == 0 || arena_buf_id >= METAL_BUFS_NEXT) return;
  metal_buf_decref(arena_buf_id);
}

// Parallel page-fault of a contiguous mmap byte range: N worker threads each
// touch a 1-byte-per-page stride over a disjoint sub-range, so the kernel's
// file-backed page faults issue concurrently and hit ~full SSD bandwidth.  A
// single serial touch stalls one fault at a time (~1.7 GB/s); N parallel fault
// streams reach ~3.9 GB/s.  Used by the zero-copy wrap when mincore finds a
// large non-resident region (weights evicted between the build-time prefetch
// and the first-forward fault-in).  Blocks until resident (the GPU bind right
// after needs the pages).
typedef struct { char const *base; u64 off, end, page; } ZcTouchSeg;
static void *zc_touch_seg(void *vp) {
  ZcTouchSeg *s = (ZcTouchSeg *)vp;
  volatile char sink = 0;
  for (u64 o = s->off; o < s->end; o += s->page) sink ^= s->base[o];
  (void)sink;
  return NULL;
}
static void zc_parallel_touch(char const *base, u64 len, u64 page) {
  // The fault-in touch re-reads pages evicted between the build-time prefetch
  // and first-forward use; more concurrent fault streams (8) clear them faster
  // than the 4-thread prefetch (the prefetch over 3 files would over-spawn at 8
  // each).  THVM_FAULT_THREADS overrides; falls back to THVM_PREFETCH_THREADS.
  static int nthreads = -1;
  if (nthreads < 0) {
    const char *e = getenv("THVM_FAULT_THREADS");
    if (e == NULL) e = getenv("THVM_PREFETCH_THREADS");
    int v = (e != NULL) ? atoi(e) : 8;
    nthreads = (v < 1) ? 1 : (v > 16 ? 16 : v);
  }
  if (base == NULL || len == 0 || page == 0 || nthreads <= 1) {
    volatile char sink = 0;
    for (u64 o = 0; o < len; o += page) sink ^= base[o];
    (void)sink;
    return;
  }
  pthread_t th[16];
  ZcTouchSeg segs[16];
  int spawned = 0;
  u64 npages = (len + page - 1) / page;
  u64 segpages = (npages + (u64)nthreads - 1) / (u64)nthreads;
  u64 seg = segpages * page;
  if (seg == 0) seg = page;
  for (int i = 0; i < nthreads; i++) {
    u64 o = (u64)i * seg;
    if (o >= len) break;
    segs[spawned].base = base;
    segs[spawned].off  = o;
    segs[spawned].end  = (o + seg < len) ? o + seg : len;
    segs[spawned].page = page;
    if (pthread_create(&th[spawned], NULL, zc_touch_seg, &segs[spawned]) == 0)
      spawned++;
    else
      zc_touch_seg(&segs[spawned]);   // inline fallback on spawn failure
  }
  for (int i = 0; i < spawned; i++) pthread_join(th[i], NULL);
}

// Zero-copy wrap of a host pointer into the METAL_BUFS table.  `page_base`
// must be page-aligned (mmap bases are) and `maplen` a page multiple, so
// newBufferWithBytesNoCopy can wrap the bytes in place: on Apple unified
// memory the GPU reads the SAME physical pages (no upload, no extra RSS).
// `minor` is the within-map byte offset of the wrapped tensor's element 0
// (the weight starts `minor` bytes past the page-aligned base); it is stored
// as the slot's byte_offset and applied at every kernel input bind so the
// kernel's contiguous index 0 lands on the weight.  The returned slot is
// flagged `borrowed` -- metal_buf_free only drops the MTLBuffer's ARC ref
// (deallocator:nil leaves the bytes alone) and the freelist never recycles it.
//
// Returns 0 on failure (no device, mis-aligned ptr, minor >= maplen, table
// full, or the no-copy wrap returned nil): the caller falls back to the
// staged-upload copy path, so this never silently corrupts.
u32 thvm_metal_buf_wrap_external(void *page_base, u64 maplen, u64 minor) {
  if (METAL_DEVICE == nil) return 0;
  if (page_base == NULL || maplen == 0 || minor >= maplen) return 0;
  // Dedup: a borrowed wrap of this exact mmap region already exists -> reuse it
  // (incref).  Each distinct safetensors weight is then wrapped EXACTLY once for
  // the context's lifetime, no matter how many times materialize_copy re-runs
  // (prewarm, capture, every per-block replay).  This is what makes the
  // zero-copy path safe: without it, each re-wrap newBufferWithBytesNoCopy's the
  // same pages into a fresh GPU mapping and they accumulate (the 100GB blowup).
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].borrowed && METAL_BUFS[i].buf != nil
        && METAL_BUFS[i].host_base == page_base
        && METAL_BUFS[i].byte_offset == minor) {
      METAL_BUFS[i].refcount++;
      return i;
    }
  }
  long pgl = sysconf(_SC_PAGESIZE);
  u64  page = (pgl > 0) ? (u64)pgl : 4096u;
  if (((uintptr_t)page_base % page) != 0) {
    fprintf(stderr,
      "thvm_metal_buf_wrap_external: ptr %p not page-aligned (page=%llu)\n",
      page_base, (unsigned long long)page);
    return 0;
  }
  // newBufferWithBytesNoCopy requires a page-multiple length.  A DiskMap's
  // maplen = nbytes + minor is mmap'd page-by-page, but the trailing partial
  // page is not guaranteed page-multiple; round UP to the next page (the
  // extra bytes past the file region are valid mapped-but-zero pages of the
  // mmap, never read because view bounds stop at the weight's numel).
  u64 wrap_len = (maplen + page - 1) & ~(page - 1);

  // Fault the wrapped pages RESIDENT.  newBufferWithBytesNoCopy wraps the
  // VIRTUAL pages, but the GPU's access does NOT trigger the CPU page-fault
  // handler that reads a lazy file-backed mmap from the SSD: a never-touched
  // (e.g. THVM_MMAP_NO_WILLNEED) page reads as ZERO on the GPU.  We must force
  // every page in before the GPU reads it.  This costs ~1x the weight's bytes
  // of host residency -- unavoidable, since the GPU reads these very pages in
  // unified memory -- but stays zero-copy: NO separate device buffer + NO host
  // staging copy.  The pages can be dropped (MADV_DONTNEED) after last use.
  //
  // The dominant FLUX cold cost is THIS fault-in (608 weights, ~12.7 GB):
  // with the file-level MADV_WILLNEED off (THVM_MMAP_NO_WILLNEED, set to bound
  // RSS), a bare serial touch loop stalls on each page fault with no readahead
  // (~570 MB/s = 22 s).  Issuing MADV_WILLNEED on JUST this weight's region
  // first kicks off the kernel's async readahead for the exact pages the touch
  // loop is about to read, pipelining the SSD reads (~1.7 GB/s = 7.6 s) WITHOUT
  // the whole-file fault the file-level WILLNEED would cost -- RSS stays bounded
  // to the wrapped weights.  madvise is a hint, so the touch loop still
  // guarantees residency before the GPU bind.
  //
  // The byte-touch re-reads the WHOLE region even when the pages are already
  // cache-resident (a warm session, or after the background TDiskPrefetchAsync
  // streamed the file in): a strided 1-byte-per-page walk of 12.7 GB still
  // costs ~3.8 s of pure RAM bandwidth + TLB churn for nothing.  mincore()
  // reports which pages are already resident, so we touch ONLY the non-resident
  // ones -- a fully-prefetched/warm weight set faults in ~0 (just the mincore
  // scan), while a genuinely cold page still gets forced in.
  {
    u64 t0 = cg_now_us();
    u64 npages = (maplen + page - 1) / page;
    char const *p = (char const *)page_base;
    // mincore FIRST: which pages are already resident (warm cache, e.g. after
    // the background TDiskPrefetchAsync streamed the file in)?  The
    // page-table-walking madvise(MADV_WILLNEED) is itself ~0.4 us/MB on macOS
    // (5+ s over 12.7 GB) EVEN when every page is already resident -- pure
    // overhead on a prefetched weight set.  So issue it ONLY when mincore finds
    // a non-resident page, scoped to the whole region (it kicks the kernel's
    // async readahead for the genuinely-cold pages); the touch loop below then
    // forces residency.  A fully-prefetched weight faults in ~0 (just the
    // mincore scan), the dominant FLUX-cold case.
    unsigned char *resid = (unsigned char *)malloc((size_t)npages);
    int have_resid = (resid != NULL && mincore(page_base, (size_t)maplen, (char *)resid) == 0);
    u64 tMin = cg_now_us();
    u64 tMad = tMin;
    if (have_resid) {
      // Count missing pages; only touch if any are non-resident.
      u64 missing = 0;
      for (u64 i = 0; i < npages; i++) if ((resid[i] & 1) == 0) missing++;
      if (missing != 0) {
        tMad = cg_now_us();
        // A large mostly-missing region (weights evicted between prefetch and
        // fault-in): re-fault the WHOLE region with PARALLEL threads -- N
        // concurrent fault streams hit ~full SSD bandwidth, vs the serial
        // madvise readahead's ~1.7 GB/s.  Re-touching the few resident pages
        // is cheap RAM bandwidth.  A sparse miss (a handful of pages) stays
        // serial, touching only the missing ones.
        if (missing * 4 >= npages && maplen >= (4u << 20)) {
          zc_parallel_touch(p, maplen, page);
        } else {
          madvise(page_base, (size_t)maplen, MADV_WILLNEED);
          volatile char sink = 0;
          for (u64 i = 0; i < npages; i++)
            if ((resid[i] & 1) == 0) sink ^= p[i * page];
          (void)sink;
        }
      }
      METAL_ZC_TOUCHED_PAGES += missing;
    } else {
      // mincore unavailable: parallel-touch every page (the safe fallback).
      tMad = cg_now_us();
      zc_parallel_touch(p, maplen, page);
      METAL_ZC_TOUCHED_PAGES += npages;
    }
    free(resid);
    u64 tTouch = cg_now_us();
    METAL_ZC_WRAPS++;
    METAL_ZC_FAULT_BYTES += maplen;
    METAL_ZC_MINCORE_US += tMin - t0;
    METAL_ZC_MADV_US    += tMad - tMin;
    METAL_ZC_TOUCH_US   += tTouch - tMad;
    METAL_ZC_FAULT_US   += tTouch - t0;
  }

  // Wrap the host pages in place FIRST (id<MTLBuffer> -- `id` here is the
  // Objective-C generic-object keyword, so the slot index below is named
  // `slot`, not `id`, to avoid shadowing it).  deallocator:nil keeps these
  // mmap pages owned by the CPU-side DiskMap; ARC only owns the wrapper.
  u64 tWrap0 = cg_now_us();
  id<MTLBuffer> wrapped =
      [METAL_DEVICE newBufferWithBytesNoCopy:page_base
                                      length:wrap_len
                                     options:MTLResourceStorageModeShared
                                 deallocator:nil];
  METAL_ZC_WRAP_US += cg_now_us() - tWrap0;
  if (wrapped == nil) {
    fprintf(stderr,
      "thvm_metal_buf_wrap_external: no-copy wrap of %llu bytes failed\n",
      (unsigned long long)wrap_len);
    return 0;
  }

  // Reuse a vacated slot (buf == nil, refcount 0) before bumping NEXT, like
  // metal_buf_alloc -- but NEVER the recycle freelist (those carry live
  // device bytes; a borrowed wrapper must start from a clean slot).
  u32 slot = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf == nil && METAL_BUFS[i].refcount == 0) {
      slot = i;
      break;
    }
  }
  if (slot == 0) {
    if (METAL_BUFS_NEXT >= METAL_BUFS_CAP) {
      fprintf(stderr, "thvm_metal_buf_wrap_external: buffer table full\n");
      return 0;
    }
    slot = METAL_BUFS_NEXT++;
  }
  METAL_BUFS[slot].buf        = wrapped;
  METAL_BUFS[slot].nbytes     = wrap_len;
  METAL_BUFS[slot].refcount   = 1;
  METAL_BUFS[slot].preserved  = 0;
  METAL_BUFS[slot].jit_pinned = 0;
  METAL_BUFS[slot].borrowed   = 1;
  METAL_BUFS[slot].byte_offset = minor;
  METAL_BUFS[slot].host_base   = page_base;
  // The wrapper object is owned here (deallocator:nil leaves the mmap pages);
  // metal_buf_free nils only the wrapper, never the bytes.  Not an arena view.
  METAL_BUFS[slot].owns_data     = 1;
  METAL_BUFS[slot].parent_buf_id = 0;
  METAL_BUFS[slot].skip_freelist = 0;
  metal_record_memory_peak();
  return slot;
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

u64 thvm_metal_buf_byte_offset(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return 0;
  return METAL_BUFS[buf_id].byte_offset;
}

// True iff buf_id is a borrowed (newBufferWithBytesNoCopy) disk-mmap wrap.
int thvm_metal_buf_is_borrowed(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return 0;
  return METAL_BUFS[buf_id].borrowed ? 1 : 0;
}

// Explicitly drop a borrowed disk-mmap wrap.  The per-realize pool rollback
// + thvm_metal_buf_free_unpreserved_all SKIP borrowed slots (they hold no
// device-owned bytes and a heap TAG_TEN may still reference the slot across
// later realizes), so a streaming per-block forward that wraps each block's
// weights zero-copy would otherwise accumulate one MTLBuffer wrapper per
// block for the whole model.  After a block's matmuls retire, the streaming
// loader calls this to release that block's wrappers; the underlying mmap
// pages stay owned by the CPU-side DiskMap (deallocator:nil) and are dropped
// separately via thvm_disk_buf_dontneed.  Flush any pending dispatch first so
// no in-flight command still reads the wrapper, then nil it.  No-op on a
// non-borrowed slot (refuses to free a device-owned buffer here).
void thvm_metal_buf_free_borrowed(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (!METAL_BUFS[buf_id].borrowed) return;
  // Dedup made wraps shareable (refcount > 1 when several weight TenDescs name
  // the same mmap region): decref and only drop the MTLBuffer at the last ref,
  // else an explicit free here dangles every other holder.
  if (METAL_BUFS[buf_id].refcount > 1) {
    METAL_BUFS[buf_id].refcount--;
    return;
  }
  metal_dispatch_flush();
  metal_buf_free(buf_id);
}

static void metal_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  // Snapshot the arena parent BEFORE zeroing the slot.  An arena VIEW
  // (owns_data == 0, parent_buf_id != 0) shares its parent's MTLBuffer at a
  // byte_offset; freeing the view must drop the view's contribution to the
  // parent's refcount no matter which path frees it (a direct
  // metal_buf_free, e.g. pool rollback, as well as the decref route).
  // Without this the arena allocation stays inflated by every freed view and
  // leaks until session end.  Mirror: backend/cuda/buf_free.c:15,28-31.
  u32 parent = METAL_BUFS[buf_id].parent_buf_id;
  // Nilling .buf drops THIS slot's ARC strong ref to the MTLBuffer.  For an
  // owning slot (owns_data == 1) that is the last ref and ARC frees the
  // MTLBuffer object -- for a borrowed wrapper (deallocator:nil) only the
  // wrapper, never the mmap pages; for a plain device buffer the bytes.  For
  // an arena VIEW (owns_data == 0) the parent slot still strongly references
  // the SAME shared MTLBuffer, so niling the view's copy of the pointer
  // releases only the view's retain -- the allocation survives until the
  // parent's own refcount hits zero (the chained decref below).  Mirror of
  // CudaBuf.owns_data (backend/cuda/buf_free.c:16) and tinygrad's
  // MetalBuffer(buf.buf, ...) sharing in ops_metal.py:192.
  METAL_BUFS[buf_id].buf      = nil;
  METAL_BUFS[buf_id].nbytes   = 0;
  METAL_BUFS[buf_id].refcount = 0;
  METAL_BUFS[buf_id].preserved = 0;
  METAL_BUFS[buf_id].jit_pinned = 0;
  METAL_BUFS[buf_id].borrowed = 0;
  METAL_BUFS[buf_id].byte_offset = 0;
  METAL_BUFS[buf_id].host_base = NULL;
  METAL_BUFS[buf_id].owns_data = 0;
  METAL_BUFS[buf_id].parent_buf_id = 0;
  METAL_BUFS[buf_id].skip_freelist = 0;
  // Drop the view's reference to its arena parent; free the parent only when
  // its last view (and its own producer ref) has gone.
  if (parent != 0 && parent < METAL_BUFS_NEXT
      && METAL_BUFS[parent].refcount > 0) {
    if (--METAL_BUFS[parent].refcount == 0) metal_buf_free(parent);
  }
  metal_record_memory_peak();
}

static void metal_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  METAL_BUFS[buf_id].refcount++;
  metal_record_memory_peak();
}

static u64 METAL_DECREF_FLUSHES;  // refcount==1 guard flushes (buffer-lifetime churn)

static void metal_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (METAL_BUFS[buf_id].refcount == 0) return;
  if (METAL_BUFS[buf_id].refcount == 1) {
    if (METAL_BATCH_CMD != nil) METAL_DECREF_FLUSHES++;
    metal_dispatch_flush();
    metal_drain_pending();   // about to free: the GPU must be done reading it
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
  if (METAL_BATCH_CMD != nil) METAL_FLUSH_FROM_BUF_READ++;
  metal_dispatch_flush();
  // Pipelined flushes commit without waiting; a host read of GPU-written bytes
  // must block until the writing buffer (now pending) completes.
  metal_drain_pending();
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[buf_id].buf == nil)            return -1;
  if (METAL_BUFS[buf_id].refcount == 0)         return -1;
  // Apply byte_offset: a borrowed disk-mmap wrap's element 0 sits `minor` bytes
  // into the page-aligned MTLBuffer (the same offset the kernel input bind
  // applies).  Without this a host readback of a wrapped weight reads the
  // page-alignment padding, not the weight.  0 for ordinary device buffers.
  u64 off = METAL_BUFS[buf_id].byte_offset;
  u64 cap = METAL_BUFS[buf_id].nbytes - off;
  if (nbytes > cap) nbytes = cap;
  memcpy(dst, (char *)[METAL_BUFS[buf_id].buf contents] + off, (size_t)nbytes);
  return 0;
}

static int metal_buf_write(u32 buf_id, const void *src, u64 nbytes) {
  metal_dispatch_flush();
  // A host write must not race the GPU still reading these bytes in the
  // pending (committed-but-unwaited) buffer.
  metal_drain_pending();
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[buf_id].buf == nil)            return -1;
  if (METAL_BUFS[buf_id].refcount == 0)         return -1;
  u64 off = METAL_BUFS[buf_id].byte_offset;
  u64 cap = METAL_BUFS[buf_id].nbytes - off;
  if (nbytes > cap) nbytes = cap;
  memcpy((char *)[METAL_BUFS[buf_id].buf contents] + off, src, (size_t)nbytes);
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

// STICKY JIT retain (mirror of cpu_buf_jit_pin / cuda_buf_jit_pin).  A
// capture's recorded buffers must survive clear_preserved across every
// sub-realize's rollback and every replay; the sticky flag is honoured
// (skip) by metal_buf_freelist_push_impl + the rollback below.  Metal has
// no arena-view parent chain, so no recursion is needed.  Cleared only on
// jit_capture_drop via metal_buf_jit_unpin.
void metal_buf_jit_pin(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  // A pin request for an already-freed (nil) slot means a buffer a live
  // capture still references was hard-freed out from under it -- the silent
  // no-op here is exactly how the FLUX weight ended up bound as a nil
  // MTLBuffer at replay.  The JIT finalize now keeps such a buffer's incref
  // through the recording->finalize boundary (jit_capture_release_retained_
  // except), so this must never fire; surface it loudly if it ever does
  // instead of silently re-pinning a dead slot.
  if (METAL_BUFS[buf_id].buf == nil) {
    fprintf(stderr,
        "thvm: metal_buf_jit_pin -- buf %u already freed (nil); a live JIT "
        "capture's buffer was reclaimed before re-pin.  Replay would bind a "
        "nil MTLBuffer.  This is a buffer-lifetime bug -- not re-pinning a "
        "dead slot.\n", buf_id);
    return;
  }
  METAL_BUFS[buf_id].jit_pinned = 1;
}

void metal_buf_jit_unpin(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  METAL_BUFS[buf_id].jit_pinned = 0;
}

void thvm_metal_buf_pool_rollback_with_preserve(u32 wm) {
  if (wm < 1) wm = 1;
  if (wm > METAL_BUFS_NEXT) return;
  metal_dispatch_flush();
  metal_drain_pending();   // a freed buffer must not still be read by the GPU
  for (u32 i = wm; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].preserved) continue;
    if (METAL_BUFS[i].jit_pinned) continue;   // sticky JIT retain
    // A borrowed disk-mmap wrap holds NO device bytes (just an MTLBuffer
    // object aliasing the CPU-side mmap pages), and a TAG_TEN tid that
    // materialize_copy / kernel_input_on_backend installed in the heap may
    // still reference it across later realizes.  Freeing it here would nil
    // the slot, so a fresh metal_buf_alloc reuses buf_id and the stale tid
    // then reads a DIFFERENT buffer (the WRAP-AFTER-READ corruption).  Skip
    // it: the wrapper costs ~nothing, the actual pages are dropped by the
    // caller's MADV_DONTNEED, and teardown / explicit free reclaims the slot.
    if (METAL_BUFS[i].borrowed) continue;
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

// Free EVERY live Metal buffer not marked `preserved` and not jit-pinned,
// across the WHOLE table (NOT bounded by a per-realize watermark).  Used by
// the forward-only reclaim in thvm_realize: after mark_gc_preserve marks the
// buffers reachable from the live root set (the realize result + WL's
// EXTERN_PINNED_TERMS + DEFS), this reclaims buffers that survived an EARLIER
// realize's watermark rollback but are now unreachable -- e.g. a per-layer
// eager weight upload that was momentarily WL-reachable (so preserved) during
// its layer's realize, then dropped, leaving it stranded below every later
// realize's watermark.  Returns the byte count freed.  The caller is
// responsible for having run a correct mark pass first; an unmarked-but-live
// buffer here WILL be freed.
u64 thvm_metal_buf_free_unpreserved_all(void) {
  u64 freed = 0;
  metal_dispatch_flush();
  metal_drain_pending();   // a freed buffer must not still be read by the GPU
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].buf == nil) continue;
    if (METAL_BUFS[i].refcount == 0) continue;
    if (METAL_BUFS[i].preserved) continue;
    if (METAL_BUFS[i].jit_pinned) continue;
    // Borrowed disk-mmap wraps hold no device bytes and a live heap tid may
    // still reference the slot; freeing it would alias on reuse (see
    // thvm_metal_buf_pool_rollback_with_preserve).  The mark pass also can't
    // see the wrap's CPU-side mmap as a device root, so a borrowed buf is
    // ALWAYS unpreserved here -- skip it explicitly.
    if (METAL_BUFS[i].borrowed) continue;
    freed += METAL_BUFS[i].nbytes;
    if (!metal_buf_freelist_push_impl(i)) {
      metal_buf_free(i);
    }
  }
  metal_record_memory_peak();
  return freed;
}

u64 thvm_metal_live_bytes(void) {
  u64 total = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].borrowed) continue;   // aliases host mmap, not device bytes
    if (METAL_BUFS[i].buf != nil && METAL_BUFS[i].refcount > 0) {
      total += METAL_BUFS[i].nbytes;
    }
  }
  return total;
}

u64 thvm_metal_retained_bytes(void) {
  u64 total = 0;
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    if (METAL_BUFS[i].borrowed) continue;   // aliases host mmap, not device bytes
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
    fprintf(stderr,
            "thvm: metal_lib_cache stats -- hits=%llu misses=%llu bytes_r=%llu bytes_w=%llu\n",
            (unsigned long long)METAL_LIB_CACHE_HITS,
            (unsigned long long)METAL_LIB_CACHE_MISSES,
            (unsigned long long)METAL_LIB_CACHE_BYTES_R,
            (unsigned long long)METAL_LIB_CACHE_BYTES_W);
    fprintf(stderr,
            "thvm: metal_jit_build stats -- hits=%llu misses=%llu bypass=%llu compile_ms=%llu\n",
            (unsigned long long)METAL_JIT_BUILD_HITS,
            (unsigned long long)METAL_JIT_BUILD_MISSES,
            (unsigned long long)METAL_JIT_BUILD_BYPASS,
            (unsigned long long)(METAL_JIT_BUILD_COMPILE_US / 1000));
    fprintf(stderr,
            "thvm: metal_zerocopy stats -- wraps=%llu fault_MB=%llu fault_ms=%llu\n",
            (unsigned long long)METAL_ZC_WRAPS,
            (unsigned long long)(METAL_ZC_FAULT_BYTES / (1024 * 1024)),
            (unsigned long long)(METAL_ZC_FAULT_US / 1000));
    if (getenv("THVM_ZC_PHASE_STATS") != NULL)
      fprintf(stderr,
              "thvm: metal_zerocopy phases -- madvise_ms=%llu mincore_ms=%llu touch_ms=%llu wrap_ms=%llu touched_pages=%llu\n",
              (unsigned long long)(METAL_ZC_MADV_US / 1000),
              (unsigned long long)(METAL_ZC_MINCORE_US / 1000),
              (unsigned long long)(METAL_ZC_TOUCH_US / 1000),
              (unsigned long long)(METAL_ZC_WRAP_US / 1000),
              (unsigned long long)METAL_ZC_TOUCHED_PAGES);
    fprintf(stderr,
            "thvm: metal_commits stats -- batched=%llu standalone=%llu decref_flushes=%llu wait_ms=%llu from_realize_end=%llu from_buf_read=%llu\n",
            (unsigned long long)METAL_FLUSH_COMMITS,
            (unsigned long long)METAL_STANDALONE_COMMITS,
            (unsigned long long)METAL_DECREF_FLUSHES,
            (unsigned long long)(METAL_FLUSH_WAIT_US / 1000),
            (unsigned long long)METAL_FLUSH_FROM_REALIZE_END,
            (unsigned long long)METAL_FLUSH_FROM_BUF_READ);
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
  // Drop every cached PSO.  The preserved MTLLibrary stays valid, so
  // these would survive too, but a fresh session rebuilds them lazily
  // from the live lib on first dispatch (cheap, in-memory -- no disk
  // reload) and clearing keeps teardown state simple.  The metal_jit
  // PSO cache is reset just below.
  for (u32 op = 0; op < UOP_COUNT; op++)
    for (u32 dt = 0; dt < 32; dt++)
      METAL_PIPELINES_CACHE[op][dt] = nil;
  metal_jit_cache_reset_impl();
  metal_graph_cache_reset_impl();
  // Drop the MPS converted-weight cache: it keys on source MTLBuffer
  // identity, which becomes stale once the pool frees + recycles those
  // addresses, so a reset prevents a false cache hit on a re-used slot.
  metal_mps_wcache_reset();
  // Phase 7 iter BB: drop the cached AOT book_heap MTLBuffer wrapper
  // before the host frees the underlying book_heap pages on
  // thvm_free.  The cached buffer's MTLBuffer object outlives the
  // backing memory otherwise -- harmless under ARC's lazy release
  // unless someone reuses it post-shutdown.
  AOT_METAL_HEAP_BUF = nil;
  AOT_METAL_HEAP_PTR = NULL;
  AOT_METAL_HEAP_LEN = 0;
  // Deliberately keep METAL_DEVICE / METAL_QUEUE / METAL_LIB alive across
  // teardown.  The device is a process singleton and the metallib is an
  // immutable on-disk blob, so reloading them on every TInit (the only
  // mid-session reclaim path on Metal before TReset[] worked) re-ran
  // newLibraryWithURL from disk and re-printed the "metal_init -- device
  // ..." banner once per episode (issue #1, Metal follow-up).  metal_init
  // reuses them when already loaded; only the per-session buffer table,
  // PSO/jit caches, and the AOT book-heap wrapper (nil'd above, since the
  // book pages are remapped) are torn down here.
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
  METAL_LIB_CACHE_HITS        = 0;
  METAL_LIB_CACHE_MISSES      = 0;
  METAL_LIB_CACHE_BYTES_R     = 0;
  METAL_LIB_CACHE_BYTES_W     = 0;
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
u64 thvm_metal_jit_compile_us(void)         { return METAL_JIT_BUILD_COMPILE_US; }
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

static int metal_tile_enabled(void) {
  // Default ON: render_uop is the primary Metal MSL emit path. Set
  // THVM_TILE=0 to force the DAG-side per-op encoder fall-through
  // (kept as a regression-bisection knob).
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
    // Resolve a symbolic (kvar) dim to its hi bound (see view_strided_index).
    u32 dim = kvar_extent_static(v->shape.dims[axis]);
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

// === On-device strided gather (Backend.gather_strided for Metal) =========
//
// Encodes a compute kernel dst[gid] = src[strided_index(gid)] so that
// materialize_root_alias_rec can flatten a non-contig realize-root view
// WITHOUT a GPU->host buf_read + CPU gather + host->GPU buf_write (which
// forces a per-realize device sync).  The strided index is recomputed in
// MSL EXACTLY as metal_view_strided_index does it: decompose gid
// last-axis-first, modulus every axis, accumulate coord*stride + offset.
//
// Scope (mirrors the prior reverted attempt): single-view (nviews == 0)
// + a 4-byte element dtype (f32 / i32) only.  bf16/fp16/chained-view
// gathers decline (return -1) so the correct host loop still handles
// them.
// Defined below (alongside the tile-jit compile path); forward-declared
// here so the gather PSO builder can reuse the disk-cached MSL->metallib
// compile.
static id<MTLLibrary> metal_lib_for_src(char const *src, NSError **err,
                                        int *via_data);

// The gather copies element BYTES (no value interpretation), so one kernel
// per element WIDTH covers every dtype of that width: uchar (1B: fp8/i8/u8),
// ushort (2B: bf16/fp16/i16), uint (4B: f32/i32).  8-byte (f64/i64) is not
// supported here -> the caller's host loop handles it.  PSOs cached per slot.
static id<MTLComputePipelineState> METAL_GATHER_PSO[3];  // [0]=1B [1]=2B [2]=4B

static id<MTLComputePipelineState> metal_gather_pso(u32 elem_bytes) {
  u32 slot;
  const char *mtype;
  switch (elem_bytes) {
    case 1: slot = 0; mtype = "uchar";  break;
    case 2: slot = 1; mtype = "ushort"; break;
    case 4: slot = 2; mtype = "uint";   break;
    default: return nil;
  }
  if (METAL_GATHER_PSO[slot] != nil) return METAL_GATHER_PSO[slot];
  if (METAL_DEVICE == nil) return nil;
  // Composes the FULL ShapeTracker chain (public view + prior_views,
  // innermost-applied-last) exactly like metal_tendesc_strided_index: each
  // view decodes the running index by its own shape (modulus per axis) and
  // re-accumulates coord*stride + offset.  params = [n_views, then per view:
  // ndim, offset, d0,s0, d1,s1, ...].
  char src[768];
  int n = snprintf(src, sizeof(src),
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "kernel void thvm_gather_strided(\n"
    "    device %s *dst [[buffer(0)]],\n"
    "    device const %s *src [[buffer(1)]],\n"
    "    constant int *p [[buffer(2)]],\n"
    "    uint gid [[thread_position_in_grid]]) {\n"
    "  int nv = p[0]; int o = 1; int idx = int(gid);\n"
    "  for (int vi = 0; vi < nv; vi++) {\n"
    "    int ndim = p[o]; int ni = p[o + 1]; o += 2;\n"
    "    int rem = idx;\n"
    "    for (int a = ndim - 1; a >= 0; a--) {\n"
    "      int dim = p[o + a * 2]; int str = p[o + a * 2 + 1];\n"
    "      int c = rem %% dim; rem /= dim; ni += c * str;\n"
    "    }\n"
    "    o += ndim * 2; idx = ni;\n"
    "  }\n"
    "  dst[gid] = src[idx];\n}\n", mtype, mtype);
  if (n <= 0 || (size_t)n >= sizeof(src)) return nil;
  NSError *err = nil;
  int via_data = 0;
  id<MTLLibrary> lib = metal_lib_for_src(src, &err, &via_data);
  if (lib == nil) {
    fprintf(stderr, "thvm: metal -- gather library compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"thvm_gather_strided"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm: metal -- gather function not in library\n");
    return nil;
  }
  id<MTLComputePipelineState> pso =
      [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn error:&err];
  if (pso == nil) {
    fprintf(stderr, "thvm: metal -- gather pipeline-state failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  METAL_GATHER_PSO[slot] = pso;
  return pso;
}

// p layout: [ndim, offset, d0, s0, d1, s1, ...]; numel = view.numel.
static int metal_gather_strided(u32 dst_buf_id, u32 src_buf_id,
                                TenDesc const *d) {
  // Decline what the host loop must keep handling:
  //  - 8-byte element dtypes (f64/i64): metal_gather_pso has no kernel.
  //  - flips (negative offset/stride): metal_tendesc_strided_index handles
  //    them on the host, but the GPU kernel would compute a negative index
  //    and read out of bounds.
  //  - very long view chains (> GATHER_MAX_VIEWS): bound the params buffer.
  // The gather copies raw element bytes, so 1/2/4-byte widths (fp8, bf16/fp16,
  // f32/i32) all work via the per-width kernel, and the FULL view chain
  // (public + prior_views) is composed in the kernel.
  #define GATHER_MAX_VIEWS 8
  if (d == NULL) return -1;
  u32 elem_bytes = dtype_storage_bytes(d->dtype, 1);
  if (elem_bytes != 1 && elem_bytes != 2 && elem_bytes != 4) return -1;
  if (d->nviews > (u32)GATHER_MAX_VIEWS) return -1;
  if (dst_buf_id == 0 || dst_buf_id >= METAL_BUFS_NEXT) return -1;
  if (src_buf_id == 0 || src_buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[dst_buf_id].buf == nil) return -1;
  if (METAL_BUFS[src_buf_id].buf == nil) return -1;
  if (METAL_BUFS[dst_buf_id].refcount == 0) return -1;
  if (METAL_BUFS[src_buf_id].refcount == 0) return -1;

  u32 numel = d->view.numel;
  if (numel == 0) return -1;

  // Collect the chain in apply order: public view, then prior_views[nviews-1..0]
  // -- exactly metal_tendesc_strided_index's composition order.
  View const *chain[GATHER_MAX_VIEWS + 1];
  u32 nchain = 0;
  chain[nchain++] = &d->view;
  for (i32 i = (i32)d->nviews - 1; i >= 0; i--) chain[nchain++] = &d->prior_views[i];
  for (u32 ci = 0; ci < nchain; ci++) {
    View const *cv = chain[ci];
    if (cv->shape.ndim < 1 || cv->shape.ndim > (u32)MAX_DIM) return -1;
    if (cv->offset < 0) return -1;
    for (u32 a = 0; a < cv->shape.ndim; a++)
      if (cv->strides[a] < 0) return -1;
  }

  id<MTLComputePipelineState> pso = metal_gather_pso(elem_bytes);
  if (pso == nil) return -1;

  // params: [n_views, then per view: ndim, offset, d0,s0, ...].  Static
  // (hi-bound) extent for kvar dims, matching metal_view_strided_index.
  int params[1 + (GATHER_MAX_VIEWS + 1) * (2 + 2 * MAX_DIM)];
  int pn = 0;
  params[pn++] = (int)nchain;
  for (u32 ci = 0; ci < nchain; ci++) {
    View const *cv = chain[ci];
    params[pn++] = (int)cv->shape.ndim;
    params[pn++] = (int)cv->offset;
    for (u32 a = 0; a < cv->shape.ndim; a++) {
      params[pn++] = (int)kvar_extent_static(cv->shape.dims[a]);
      params[pn++] = (int)cv->strides[a];
    }
  }
  size_t params_len = (size_t)pn * sizeof(int);

  id<MTLCommandBuffer> cmd = metal_command_buffer();
  if (cmd == nil) return -1;
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:METAL_BUFS[dst_buf_id].buf offset:0 atIndex:0];
  [enc setBuffer:METAL_BUFS[src_buf_id].buf offset:0 atIndex:1];
  [enc setBytes:params length:params_len atIndex:2];
  NSUInteger tg = MIN((NSUInteger)numel,
                      [pso maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(numel, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  // Batched: the gather encodes into the same command buffer AFTER its
  // producer dispatch (Metal preserves submission order within a buffer),
  // so the source bytes are live by the time this kernel runs.  When
  // standalone (no active batch), submit+wait here.
  metal_submit_if_standalone(cmd);
  return 0;
}

static u64 metal_tile_jit_hash(KernelEntry const *ke) {
  u64 h = 0xcbf29ce484222325ULL ^ 0x4D54494C45554F50ULL;
  // kvar wedge: if any UOP_RANGE in the kernel is variable-bound, the
  // emitted MSL uses `V_<name>` for that extent and the per-dispatch
  // numel comes through setBytes:; the kernel's input_numels[] /
  // output_numel CHANGE per BS but the MSL string does not.  We must
  // therefore exclude those numels from the hash whenever the kernel
  // is symbolic-shape -- otherwise BS=4 and BS=32 hash to different
  // slots and we re-compile for every BS.
  //
  // Input/output numels are excluded for symbolic kernels (below) for
  // the same reason.  The UOp DAG content hash captures UOP_RANGE
  // var_ids (via the extent token) and axis types, so symbolic kernels
  // at different BS values still share the same UOp identity.
  u32 used_vars[KVAR_USED_CAP];
  u32 n_vars = kvar_collect_from_dag(ke->cached_lift.store_root,
                                     used_vars, KVAR_USED_CAP);
  int is_symbolic = (n_vars > 0);
  // GC-INVARIANT structural content hash of the lifted DAG instead of
  // the raw heap location of store_root.  The autotune/BEAM search
  // hash-conses candidate DAG variants into the shared heap; a later GC
  // relocates the captured kernels' store_root, which would silently
  // change a heap-loc key -> warm-replay PSO miss/collision -> wrong or
  // no PSO -> non-finite output.  uop_dag_content_hash is invariant
  // under relocation: two structurally-identical kernels hash equal
  // (they SHOULD share a PSO); two distinct kernels hash distinct
  // (covering op codes, const bits, dtypes, shapes, axis types, applied
  // opts, transpose flags, child structure -- recursively).
  if (ke->cached_lift.store_root != 0) {
    u64 content = uop_dag_content_hash(ke->cached_lift.store_root);
    h ^= content; h *= 0x100000001b3ULL;
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

// === MTLCodeGenService: compile MSL source directly to a .metallib blob ===
//
// Ports tinygrad's MetalCompiler (tinygrad/runtime/ops_metal.py:61): the
// private MTLCodeGenService compiles MSL -> a .metallib byte blob in-process,
// which we disk-cache and reload via [MTLDevice newLibraryWithData:].  That
// skips the MSL->AIR frontend that [MTLDevice newLibraryWithSource:] re-runs
// on every cold start -- ~1.5s per tile kernel, ~100s total for FLUX's 70
// distinct matmul/attention shaders.  newLibraryWithSource: stays as the
// fallback when the service or cache dir is unavailable.
//
// The callback is a real Objective-C block (cleaner than tinygrad's ctypes
// fake-block-at--0x10 hack): MTLCodeGenServiceBuildRequest reads block->invoke
// at the standard offset and calls it synchronously before returning, so a
// stack block is safe.
typedef void (^MtlCodeGenBlock)(int32_t error, const void *dataPtr,
                                size_t dataLen, const char *errorMessage);
typedef void *(*MtlCgsCreateFn)(const char *);
typedef void  (*MtlCgsBuildFn)(void *cgs, void *unused, int requestType,
                               const char *request, size_t requestLen,
                               MtlCodeGenBlock block);

#define MTL_REQUEST_TYPE_COMPILE 13

static void         *METAL_CGS       = NULL;   // MTLCodeGenService handle
static MtlCgsBuildFn  METAL_CGS_BUILD = NULL;
static int            METAL_CGS_READY = -1;    // -1 unprobed, 0 unavailable, 1 ready

// Resolve MTLCodeGenServiceCreate/BuildRequest once.  Metal already loads
// MTLCompiler, so the symbols are usually reachable via RTLD_DEFAULT; fall
// back to an explicit dlopen of the private framework.
static void metal_cgs_init(void) {
  if (METAL_CGS_READY != -1) return;
  METAL_CGS_READY = 0;
  if (getenv("THVM_METAL_LIB_CACHE") != NULL
      && getenv("THVM_METAL_LIB_CACHE")[0] == '0') return;
  MtlCgsCreateFn create = (MtlCgsCreateFn)dlsym(RTLD_DEFAULT, "MTLCodeGenServiceCreate");
  MtlCgsBuildFn  build  = (MtlCgsBuildFn)dlsym(RTLD_DEFAULT, "MTLCodeGenServiceBuildRequest");
  if (create == NULL || build == NULL) {
    void *h = dlopen("/System/Library/PrivateFrameworks/MTLCompiler.framework/MTLCompiler",
                     RTLD_NOW | RTLD_GLOBAL);
    if (h != NULL) {
      create = (MtlCgsCreateFn)dlsym(h, "MTLCodeGenServiceCreate");
      build  = (MtlCgsBuildFn)dlsym(h, "MTLCodeGenServiceBuildRequest");
    }
  }
  if (create == NULL || build == NULL) return;
  METAL_CGS = create("thvm");
  if (METAL_CGS == NULL) return;
  METAL_CGS_BUILD = build;
  METAL_CGS_READY = 1;
}

// Compile MSL `src` to a .metallib blob (NSData of MTLB...ENDT), or nil on
// failure.  Mirrors tinygrad's request layout: <u64 src_pad_len><u64
// params_pad_len><src padded to 4, >=1 NUL><params NUL-terminated>, and the
// reply's library starts at header_size+warning_size (two u32 at byte 8/12).
static NSData *metal_cgs_compile(const char *src) {
  metal_cgs_init();
  if (METAL_CGS_READY != 1 || src == NULL) return nil;
  char const *cdir = (METAL_PSO_CACHE_DIR[0] != '\0') ? METAL_PSO_CACHE_DIR : ".";
  char params[METAL_PSO_CACHE_PATH_MAX + 192];
  int pn = snprintf(params, sizeof(params),
      "-fno-fast-math -std=metal3.1 --driver-mode=metal -x metal "
      "-fmodules-cache-path=\"%s\" -fno-caret-diagnostics", cdir);
  if (pn <= 0 || (size_t)pn >= sizeof(params)) return nil;
  size_t src_len = strlen(src);
  size_t src_pad = (src_len + 1 + 3u) & ~((size_t)3u);   // >=1 NUL, multiple of 4
  size_t par_pad = (size_t)pn + 1;                         // NUL-terminated
  size_t req_len = 16 + src_pad + par_pad;
  char *request = (char *)calloc(1, req_len);
  if (request == NULL) return nil;
  u64 a = (u64)src_pad, b = (u64)par_pad;
  memcpy(request + 0, &a, 8);
  memcpy(request + 8, &b, 8);
  memcpy(request + 16, src, src_len);
  memcpy(request + 16 + src_pad, params, (size_t)pn);
  __block NSData *result = nil;
  char cb_msg[512];
  cb_msg[0] = '\0';
  char *cb_msg_p = cb_msg;   // a pointer is block-capturable; the C array is not
  MtlCodeGenBlock cb = ^(int32_t error, const void *dataPtr,
                         size_t dataLen, const char *errorMessage) {
    if (error == 0 && dataPtr != NULL && dataLen >= 16) {
      const uint8_t *r = (const uint8_t *)dataPtr;
      u32 hdr = 0, warn = 0;
      memcpy(&hdr, r + 8, 4);
      memcpy(&warn, r + 12, 4);
      size_t off = (size_t)hdr + (size_t)warn;
      if (off <= dataLen)
        result = [NSData dataWithBytes:(r + off) length:(dataLen - off)];
    } else if (errorMessage != NULL) {
      strncpy(cb_msg_p, errorMessage, 511);
      cb_msg_p[511] = '\0';
    }
  };
  METAL_CGS_BUILD(METAL_CGS, NULL, MTL_REQUEST_TYPE_COMPILE, request, req_len, cb);
  free(request);
  if (result == nil) {
    if (cb_msg[0] != '\0')
      fprintf(stderr, "thvm: metal_cgs_compile -- %s\n", cb_msg);
    return nil;
  }
  if ([result length] < 8
      || memcmp([result bytes], "MTLB", 4) != 0) return nil;
  return result;
}

// FNV-1a over the MSL source plus a toolchain/version tag, so a renderer or
// compile-flag change invalidates stale .metallib entries.
static u64 metal_src_hash(const char *src) {
  u64 h = 0xcbf29ce484222325ULL;
  for (unsigned char const *p = (unsigned char const *)src; *p; p++) {
    h ^= *p; h *= 0x100000001b3ULL;
  }
  for (unsigned char const *p = (unsigned char const *)"metal3.1;libv1"; *p; p++) {
    h ^= *p; h *= 0x100000001b3ULL;
  }
  return h;
}

static int metal_lib_cache_path(u64 key, char *out, size_t cap) {
  int n = snprintf(out, cap, "%s/lib-%016llx.metallib",
                   METAL_PSO_CACHE_DIR, (unsigned long long)key);
  if (n <= 0 || (size_t)n >= cap) return -1;
  return 0;
}

// Build an MTLLibrary for `src`, using the on-disk .metallib cache to skip the
// MSL->AIR frontend.  Disk hit -> read bytes + newLibraryWithData:.  Miss ->
// MTLCodeGenService compile + persist + newLibraryWithData:.  Falls back to
// newLibraryWithSource: when the cache dir or service is unavailable, or when
// loading the cached bytes fails.  *via_data (optional) is set to 1 when the
// library came from newLibraryWithData (cached bytes OR a fresh service
// compile): such libraries' functions don't match the source-built PSO
// BinaryArchive, so the caller skips that archive (the archive lookup misses
// anyway and just churns disk).
static id<MTLLibrary> metal_lib_for_src(char const *src, NSError **err,
                                        int *via_data) {
  if (via_data) *via_data = 0;
  metal_cgs_init();
  if (METAL_PSO_CACHE_ENABLED && METAL_CGS_READY == 1) {
    u64 key = metal_src_hash(src);
    char path[METAL_PSO_CACHE_PATH_MAX];
    if (metal_lib_cache_path(key, path, sizeof(path)) == 0) {
      NSString *nspath = [NSString stringWithUTF8String:path];
      NSData *libdata = [NSData dataWithContentsOfFile:nspath];
      if (libdata != nil) {
        METAL_LIB_CACHE_HITS++;
        METAL_LIB_CACHE_BYTES_R += (u64)[libdata length];
      } else {
        libdata = metal_cgs_compile(src);
        if (libdata != nil) {
          METAL_LIB_CACHE_MISSES++;
          if ([libdata writeToFile:nspath atomically:YES])
            METAL_LIB_CACHE_BYTES_W += (u64)[libdata length];
        }
      }
      if (libdata != nil) {
        dispatch_data_t dd =
            dispatch_data_create([libdata bytes], [libdata length],
                                 dispatch_get_main_queue(),
                                 DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithData:dd error:err];
        if (lib != nil) { if (via_data) *via_data = 1; return lib; }
        // newLibraryWithData failed (corrupt/ABI drift): fall through to a
        // fresh source compile, which also rewrites the bad entry next time.
      }
    }
  }
  MTLCompileOptions *copts = [[MTLCompileOptions alloc] init];
  [copts setLanguageVersion:MTLLanguageVersion3_1];
  return [METAL_DEVICE newLibraryWithSource:[NSString stringWithUTF8String:src]
                                    options:copts error:err];
}

static id<MTLComputePipelineState> metal_tile_jit_build(KernelEntry const *ke,
                                                        u64 key) {
  char *src = cg_emit_tile_metal(ke);
  if (src == NULL) return nil;
  // THVM_DUMP_TILE_JIT_SRC=2 (or "all"): dump the generated MSL for
  // every tile-jit'd kernel up front, tagged with kid -- lets you see
  // whether conv matmuls picked the simdgroup_matrix template or fell
  // back to the scalar accumulator.  THVM_DUMP_TILE_JIT_SRC=1 dumps
  // only on compile failure (below).
  {
    char const *d = getenv("THVM_DUMP_TILE_JIT_SRC");
    if (d != NULL && (d[0] == '2' || d[0] == 'a')) {
      fprintf(stderr, "---- tile-jit src kid=%u ----\n%s\n----\n",
              (unsigned)(ke - KERNELS), src);
    }
  }
  u64 t0 = cg_now_us();
  NSError *err = nil;
  // The library comes from the on-disk .metallib cache (newLibraryWithData:)
  // when warm, else a fresh MTLCodeGenService / newLibraryWithSource: compile.
  // metal_lib_for_src pins MSL 3.1 (the `bfloat` floor -- macOS 14+; tinygrad
  // ops_metal.py:88) for both the service params and the source fallback.
  int lib_via_data = 0;
  id<MTLLibrary> lib = metal_lib_for_src(src, &err, &lib_via_data);
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_tile_jit -- compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    if (getenv("THVM_DUMP_TILE_JIT_SRC")) {
      fprintf(stderr, "---- failing source ----\n%s\n----\n", src);
    }
    free(src);
    return nil;
  }
  free(src);
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
  // that case fall through to the fresh-build path below.  Skip the
  // archive entirely when the library came from newLibraryWithData
  // (the .metallib cache): those functions don't key-match a
  // source-built archive, so the lookup just misses + churns disk --
  // a fresh newComputePipelineState is the same cost (measured).
  int try_archive = !lib_via_data;
  id<MTLComputePipelineState> pso =
      try_archive ? metal_pso_cache_try_load(key, mtlFn, desc) : nil;
  int from_disk = 0;
  if (pso != nil) {
    from_disk = 1;
    METAL_JIT_BUILD_DISK_HITS++;
  } else {
    if (METAL_PSO_CACHE_ENABLED && try_archive) {
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
    // PSO above is still usable for this process.  Only meaningful for
    // source-compiled libs (see try_archive above).
    if (try_archive) metal_pso_cache_store(key, desc);
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
                                 u64 const *src_offsets,
                                 id<MTLBuffer> outBuf,
                                 u64 out_offset,
                                 id<MTLCommandBuffer> cmd,
                                 u32 groups_x,
                                 u32 threads_x) {
  if (groups_x == 0 || threads_x == 0) return 0;
  id<MTLComputePipelineState> pso = metal_tile_jit_pipeline(ke);
  if (pso == nil) return 0;
  if ((NSUInteger)threads_x > [pso maxTotalThreadsPerThreadgroup]) {
    return 0;
  }

  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  // Bind the output at its byte_offset (arena VIEW -> a window of the shared
  // arena MTLBuffer; 0 for an ordinary output).  tinygrad ops_metal.py:140.
  [enc setBuffer:outBuf offset:(NSUInteger)out_offset atIndex:0];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    // A borrowed disk-mmap wrap binds at its within-buffer byte_offset so the
    // kernel's contiguous index 0 lands on the weight (the wrapped MTLBuffer
    // starts at the page-aligned DiskMap base, `minor` bytes before it).
    [enc setBuffer:src_bufs[i]
            offset:(NSUInteger)(src_offsets ? src_offsets[i] : 0)
           atIndex:(1 + i)];
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

// ICB cache hit/miss tally (THVM_ICB_STATS=1) -- a cache MISS rebuilds the
// whole indirect command buffer (the slow warm path); a HIT replays the
// pre-baked one.  Profiling whether the FLUX warm replays hit or rebuild.
static u64 G_ICB_HITS = 0, G_ICB_MISSES = 0;
static int metal_icb_stats_on(void) {
  static int known = 0, on = 0;
  if (!known) { on = (getenv("THVM_ICB_STATS") != NULL); known = 1; }
  return on;
}

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
    // The ICB indirect-command API has no setBytes: equivalent, so a kernel
    // that needs per-dispatch kvar (symbolic-dim) scalars -- which the per-op
    // tile path binds via setBytes (metal_tile_jit_encode) -- cannot be encoded
    // here.  Decline the whole graph; the per-op replay binds them correctly.
    {
      u32 kv_used[KVAR_USED_CAP];
      if (kvar_collect_from_dag(ke->cached_lift.store_root, kv_used, KVAR_USED_CAP) > 0) {
        return nil;
      }
    }
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
    // Bind the output at its byte_offset (an arena VIEW writes a window of the
    // shared arena MTLBuffer; 0 for an ordinary output).  Without this the
    // recycled kernel would write at the arena base instead of its slice.
    // tinygrad binds every buffer at its offset uniformly (ops_metal.py:140).
    [cmd setKernelBuffer:outBuf
                  offset:(NSUInteger)METAL_BUFS[r->out_buf_id].byte_offset
                 atIndex:0];
    for (u32 j = 0; j < r->n_inputs; j++) {
      u32 bid = r->in_buf_ids[j];
      if (bid == 0 || bid >= METAL_BUFS_NEXT) return nil;
      id<MTLBuffer> inBuf = METAL_BUFS[bid].buf;
      if (inBuf == nil) return nil;
      if (trace > 1) {
        fprintf(stderr, "thvm: metal_graph command %u set in %u\n", i, j);
      }
      // Borrowed disk-mmap wrap: bind at its within-buffer byte_offset so the
      // ICB-replayed kernel's contiguous index 0 lands on the weight.
      [cmd setKernelBuffer:inBuf
                    offset:(NSUInteger)METAL_BUFS[bid].byte_offset
                   atIndex:(1 + j)];
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
  if (metal_icb_stats_on()) {
    static u64 calls = 0; calls++;
    if (calls <= 30 || calls % 16 == 0)
      fprintf(stderr, "[icb-entry] call=%llu n_ops=%u %s\n",
              (unsigned long long)calls, n_ops,
              (ops == NULL || n_ops < 2 || n_ops > 256) ? "BAIL(n_ops>256)" : "ok");
  }
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
    G_ICB_HITS++;
    if (trace) {
      fprintf(stderr, "thvm: metal_graph cache hit idx=%u resources=%u\n",
              idx, resource_count);
    }
  } else {
    G_ICB_MISSES++;
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
  if (metal_icb_stats_on() && ((G_ICB_HITS + G_ICB_MISSES) % 64 == 0)) {
    u64 tot = G_ICB_HITS + G_ICB_MISSES;
    fprintf(stderr, "[icb] hits=%llu misses=%llu (%.0f%% hit)\n",
            (unsigned long long)G_ICB_HITS, (unsigned long long)G_ICB_MISSES,
            tot ? 100.0 * (double)G_ICB_HITS / (double)tot : 0.0);
  }
  if (cfgBuf != nil) {
    if (resource_count >= METAL_GRAPH_MAX_RESOURCES) return -1;
    resources[resource_count++] = cfgBuf;
  }

  // Per-op GPU-timestamp profiling.  When metal_perop_enabled() (set by
  // THVM_METAL_PROFILE_PEROP=1 or THVM_KERNEL_PROFILE=N), replace the
  // single batched ICB execution with N per-op encoder dispatches; each
  // cmd buffer commits + waits, then we read cmd.GPUEndTime -
  // cmd.GPUStartTime for true per-kernel GPU time.  Costs ~5-10x more
  // dispatch overhead than the batched path -- only for profile runs.
  // Without this, the batched fallback below records the same averaged
  // wall value (elapsed/n_ops) for every kernel and leaves gpu_us at
  // zero -- both misleading for per-kernel rankings.  Here wall time is
  // the real per-op encode+execute span and gpu_us the true GPU span.
  if (metal_perop_enabled()) {
    for (u32 i = 0; i < n_ops; i++) {
      u64 op_t0 = cg_now_us();
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
      cg_profile_record(ops[i].kid, KDISPATCH_METAL_TILE, cg_now_us() - op_t0);
      double gpu_seconds = cmd_i.GPUEndTime - cmd_i.GPUStartTime;
      if (gpu_seconds > 0.0) {
        cg_profile_record_gpu(ops[i].kid, (u64)(gpu_seconds * 1e6));
      }
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

// Predicate: every input + every reachable UOp in the lifted DAG
// uses one of the dtypes the Metal shader library covers (f32 +
// i32 today; f16 + the rest follow as shader variants land).
// Mixed-dtype or unsupported kernels return 0 so cpu_dispatch_kernel
// falls back to the CPU path (interpret / JIT).
//
// uop_dag_dtype_uniform walks the lifted UOp DAG and confirms every
// BUFFER / CONST / CAST-dst dtype equals `dt`.  Kernels whose lift
// declined (cached_lift.store_root == 0) are unsupported here.
static int metal_kernel_supported(struct KernelEntry const *ke) {
  // Materialize populates cached_lift.store_root on every emitted
  // kernel; eligibility is uniform-dtype over the lifted DAG.
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  if (ke->n_inputs == 0) return 0;
  u32 dt = ke->input_dtypes[0];
  if (dt != DT_FP32 && dt != DT_INT32) return 0;
  if (!uop_dag_dtype_uniform(ke->cached_lift.store_root, dt)) return 0;
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

// === DAG-side per-op encoder ========================================
//
// Walks ke->cached_lift.store_root (a UOP_STORE produced by
// kernel_lift_to_uop) and emits one MTLComputeCommandEncoder per
// UOp tree node so Metal hazard-tracks reads/writes across encoders.
// Fires when the lift succeeded (cached_lift.store_root != 0) AND
// metal_tile_jit_encode declined.
//
// metal_pipeline_for(opcode, dtype) -- the table that resolves a
// (UOP_*, dtype) pair to an MTLComputePipelineState -- handles the
// shader lookup.

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
  // Bind the destination at its byte_offset (arena VIEW -> a window of the
  // shared arena MTLBuffer; 0 for an intermediate or ordinary output).
  // tinygrad ops_metal.py:140.
  [enc setBuffer:METAL_BUFS[bid].buf
          offset:(NSUInteger)METAL_BUFS[bid].byte_offset
         atIndex:0];
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
  // Bind the destination at its byte_offset (arena VIEW -> a window of the
  // shared arena MTLBuffer; 0 for an intermediate or ordinary output).
  // tinygrad ops_metal.py:140.
  [enc setBuffer:METAL_BUFS[bid].buf
          offset:(NSUInteger)METAL_BUFS[bid].byte_offset
         atIndex:0];
  u32 zero_arg = 0;
  [enc setBytes:&zero_arg length:sizeof(zero_arg) atIndex:1];
  for (u32 i = 0; i < n_src; i++) {
    // Borrowed disk-mmap wrap: bind at its within-buffer byte_offset so the
    // shader's contiguous index 0 lands on the weight (see
    // thvm_metal_buf_wrap_external).  0 for ordinary buffers.
    [enc setBuffer:METAL_BUFS[src_bids[i]].buf
            offset:(NSUInteger)METAL_BUFS[src_bids[i]].byte_offset
           atIndex:(2 + i)];
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

// Resolve the destination Metal buf id for a UOP_STORE.  Every
// emitted kernel writes its single output via the UOP_BUFFER with
// instance==0; any other instance is an input slot, which is not a
// valid STORE destination.  Returns 0 to bail.
static u32 dag_enc_resolve_store_dst(DagEncCtx *c, Term store_buf,
                                     u32 primary_out_buf_id) {
  (void)c;
  u32 buf_op = 0; u64 buf_loc = 0;
  if (!uop_dag_decode_uop(store_buf, &buf_op, &buf_loc)
      || buf_op != UOP_BUFFER) {
    return 0;
  }
  u32 inst = uop_dag_buffer_instance(store_buf);
  if (inst == 0) {
    return primary_out_buf_id;
  }
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
// (caller submits the command buffer + cleans up); 0 if the encoder
// declined (unsupported op shape, alloc failure, etc.) -- callers
// have no further fallback and must surface the failure.  Releases
// its own intermediate buffer allocations on either path.
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

// === MPS matmul: route large 2-D GEMMs to MPSMatrixMultiplication =====
//
// Mirrors the CPU backend's cblas dispatch (backend/cpu/blas.c): the same
// uop_dag_classify_matmul_shape classifier recovers (M, N, K, a/b slot,
// ldA/ldB, transA/transB), then we hand the operands to Apple's vendor
// GEMM -- here MPSMatrixMultiplication instead of cblas_sgemm.  The custom
// simdgroup_matrix tiled kernel tops out ~6 TFLOP/s on the FLUX projection
// shapes; MPS hits ~11-13 TFLOP/s (~2x), which is what drops the single-
// block projections enough for the 4-step FLUX sampler to clear 3 s warm.
//
// dtype: FLUX activations + weights are bf16, but MPSMatrixMultiplication
// on this OS (Tahoe 26 / M3) ASSERTS on MPSDataTypeBFloat16 -- it accepts
// only Float32 / Float16 / Int8 / Int16.  So we convert the bf16 operands
// to a working dtype (f16 by default, f32 selectable) via tiny runtime-
// compiled conversion kernels, run MPS, then convert the result back to
// bf16.  The B (weight) operand is JIT-pinned + constant across the 4
// replay steps, so its converted copy is cached by source MTLBuffer
// identity -- only A (small activation) + C (output) reconvert per step.
//
// JIT integration: an MPS op cannot be encoded into an MTLIndirectCommand
// (the ICB API has no MPS equivalent), so the batched metal-graph replay
// path (jit_replay_try_metal_graph_run) BREAKS on a kernel whose dispatch
// kind is KDISPATCH_METAL_MPS -- it falls to the per-op replay loop, which
// calls metal_dispatch_kernel here, encoding conversions + MPS into the
// current (batched) command buffer.  Eager (non-JIT) realize hits the same
// path.

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

// Working dtype for the MPS GEMM: 0 = f16 (default; same 2-byte width as
// bf16, ~13 TFLOP/s), 1 = f32 (~12 TFLOP/s, wider range).  Read once.
static int metal_mps_dtype_f32(void) {
  static int slot = -1;
  if (slot == -1) {
    char const *e = getenv("THVM_METAL_MPS_DTYPE");
    slot = (e != NULL && (e[0] == 'f') && (e[1] == '3')) ? 1 : 0;  // "f32"
  }
  return slot;
}

// Master gate: THVM_METAL_MPS=1 opts into MPS (A/B bisection knob, mirrors
// THVM_CPU_BLAS_DISABLE).  DEFAULT OFF: MPS is faster per-matmul on a single
// eager GEMM ({768,3072}x{3072,27648}: 20ms custom -> 12ms MPS), but it
// shatters the JIT replay's single-command-buffer batching -- MPSMatrix-
// Multiplication encodeToCommandBuffer needs its own command buffer, so each
// matmul becomes a standalone commit+wait round-trip.  On the FLUX 4-step
// replay (40 big matmuls/forward) that regressed warm 857ms -> 3002ms/step
// (3.5x SLOWER).  The custom simdgroup_matrix tiled kernel batches into the
// one replay command buffer and wins.  To make MPS a net win it must encode
// into METAL_BATCH_CMD (no per-dispatch submit) -- until then, opt-in only.
static int metal_mps_enabled(void) {
  static int slot = -1;
  if (slot == -1) {
    char const *e = getenv("THVM_METAL_MPS");
    slot = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return slot;
}

// Minimum MAC count (M*N*K) to route to MPS.  Below this the MPS setup +
// conversion overhead outweighs the per-kernel speedup, so the custom
// tiled / per-op kernel wins.  Default 64M MACs (e.g. 256x256x1024) --
// well below the FLUX projections (768*3072*27648 = 65 GMAC) but above
// attention-tile-sized matmuls.
static u64 metal_mps_min_macs(void) {
  static u64 slot = (u64)-1;
  if (slot == (u64)-1) {
    char const *e = getenv("THVM_METAL_MPS_MIN_MACS");
    slot = (e != NULL && e[0] != '\0') ? strtoull(e, NULL, 10) : (64ULL << 20);
  }
  return slot;
}

static u64 METAL_MPS_DISPATCH_COUNT = 0;
fn u64 thvm_metal_mps_dispatch_count(void) { return METAL_MPS_DISPATCH_COUNT; }

// Runtime-compiled conversion-kernel PSOs (bf16<->{f16,f32}).  Compiled
// lazily on first MPS dispatch; one MTLLibrary, four functions.
static id<MTLComputePipelineState> METAL_MPS_CVT_BF16_TO_F16 = nil;
static id<MTLComputePipelineState> METAL_MPS_CVT_F16_TO_BF16 = nil;
static id<MTLComputePipelineState> METAL_MPS_CVT_BF16_TO_F32 = nil;
static id<MTLComputePipelineState> METAL_MPS_CVT_F32_TO_BF16 = nil;
static int METAL_MPS_CVT_READY = -1;   // -1 unattempted, 0 failed, 1 ok

static int metal_mps_build_cvt_kernels(void) {
  if (METAL_MPS_CVT_READY != -1) return METAL_MPS_CVT_READY;
  METAL_MPS_CVT_READY = 0;
  if (METAL_DEVICE == nil) return 0;
  // Width-preserving element conversions.  bfloat<->half<->float are all
  // native MSL scalar casts; one thread per element, contiguous.
  NSString *src = @"#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "kernel void cvt_bf16_f16(device const bfloat *in [[buffer(0)]],\n"
    "                         device half *out [[buffer(1)]],\n"
    "                         constant uint &n [[buffer(2)]],\n"
    "                         uint gid [[thread_position_in_grid]]) {\n"
    "  if (gid < n) out[gid] = (half)(float)in[gid];\n"
    "}\n"
    "kernel void cvt_f16_bf16(device const half *in [[buffer(0)]],\n"
    "                         device bfloat *out [[buffer(1)]],\n"
    "                         constant uint &n [[buffer(2)]],\n"
    "                         uint gid [[thread_position_in_grid]]) {\n"
    "  if (gid < n) out[gid] = (bfloat)(float)in[gid];\n"
    "}\n"
    "kernel void cvt_bf16_f32(device const bfloat *in [[buffer(0)]],\n"
    "                         device float *out [[buffer(1)]],\n"
    "                         constant uint &n [[buffer(2)]],\n"
    "                         uint gid [[thread_position_in_grid]]) {\n"
    "  if (gid < n) out[gid] = (float)in[gid];\n"
    "}\n"
    "kernel void cvt_f32_bf16(device const float *in [[buffer(0)]],\n"
    "                         device bfloat *out [[buffer(1)]],\n"
    "                         constant uint &n [[buffer(2)]],\n"
    "                         uint gid [[thread_position_in_grid]]) {\n"
    "  if (gid < n) out[gid] = (bfloat)in[gid];\n"
    "}\n";
  NSError *err = nil;
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithSource:src
                                                  options:nil
                                                    error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_mps -- cvt-kernel compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return 0;
  }
  struct { __strong id<MTLComputePipelineState> *dst; const char *nm; } map[4] = {
    { &METAL_MPS_CVT_BF16_TO_F16, "cvt_bf16_f16" },
    { &METAL_MPS_CVT_F16_TO_BF16, "cvt_f16_bf16" },
    { &METAL_MPS_CVT_BF16_TO_F32, "cvt_bf16_f32" },
    { &METAL_MPS_CVT_F32_TO_BF16, "cvt_f32_bf16" },
  };
  for (int i = 0; i < 4; i++) {
    id<MTLFunction> mtlf = [lib newFunctionWithName:
                            [NSString stringWithUTF8String:map[i].nm]];
    if (mtlf == nil) return 0;
    err = nil;
    id<MTLComputePipelineState> pso =
        [METAL_DEVICE newComputePipelineStateWithFunction:mtlf error:&err];
    if (pso == nil) {
      fprintf(stderr, "thvm: metal_mps -- cvt PSO %s failed: %s\n", map[i].nm,
              err ? [[err localizedDescription] UTF8String] : "(no error)");
      return 0;
    }
    *map[i].dst = pso;
  }
  METAL_MPS_CVT_READY = 1;
  return 1;
}

// Encode an element-wise width conversion of `n` contiguous elements.
static void metal_mps_encode_cvt(id<MTLCommandBuffer> cmd,
                                 id<MTLComputePipelineState> pso,
                                 id<MTLBuffer> in, id<MTLBuffer> out, u32 n) {
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:in offset:0 atIndex:0];
  [enc setBuffer:out offset:0 atIndex:1];
  [enc setBytes:&n length:sizeof(n) atIndex:2];
  NSUInteger tpt = [pso maxTotalThreadsPerThreadgroup];
  if (tpt > 256) tpt = 256;
  [enc dispatchThreads:MTLSizeMake((NSUInteger)n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tpt, 1, 1)];
  [enc endEncoding];
}

// Cache of converted weight buffers, keyed by source MTLBuffer identity.
// FLUX has a bounded weight set; a small direct-mapped cache suffices.
// Entries hold a STRONG MTLBuffer ref (the converted copy) so it survives
// across replay steps without going through the METAL_BUFS pool.
#define METAL_MPS_WCACHE_CAP 512
typedef struct {
  void           *src_ptr;   // (__bridge) source MTLBuffer identity
  u32             n;          // element count (validates the slot)
  int             work_f32;  // which working dtype this copy is in
  id<MTLBuffer>   conv;       // converted copy (strong ref)
} MpsWCacheEntry;
static MpsWCacheEntry METAL_MPS_WCACHE[METAL_MPS_WCACHE_CAP];

static void metal_mps_wcache_reset(void) {
  for (u32 i = 0; i < METAL_MPS_WCACHE_CAP; i++) {
    METAL_MPS_WCACHE[i].src_ptr = NULL;
    METAL_MPS_WCACHE[i].conv = nil;
  }
}

// Return a working-dtype copy of the bf16 weight buffer `src` (n elems),
// converting + caching on miss.  Returns nil on failure (caller declines
// MPS).  `cmd` carries the conversion dispatch on a cold miss.
static id<MTLBuffer> metal_mps_weight_copy(id<MTLBuffer> src, u32 n,
                                           int work_f32,
                                           id<MTLCommandBuffer> cmd) {
  void *key = (__bridge void *)src;
  u32 h = (u32)(((uintptr_t)key >> 6) ^ ((uintptr_t)key >> 20));
  for (u32 probe = 0; probe < METAL_MPS_WCACHE_CAP; probe++) {
    u32 i = (h + probe) & (METAL_MPS_WCACHE_CAP - 1);
    MpsWCacheEntry *e = &METAL_MPS_WCACHE[i];
    if (e->src_ptr == key && e->n == n && e->work_f32 == work_f32
        && e->conv != nil) {
      return e->conv;          // hit
    }
    if (e->src_ptr == NULL) {  // empty slot -> build here
      u32 esz = work_f32 ? 4u : 2u;
      id<MTLBuffer> conv = [METAL_DEVICE newBufferWithLength:(NSUInteger)n * esz
                                                     options:MTLResourceStorageModeShared];
      if (conv == nil) return nil;
      metal_mps_encode_cvt(cmd,
          work_f32 ? METAL_MPS_CVT_BF16_TO_F32 : METAL_MPS_CVT_BF16_TO_F16,
          src, conv, n);
      e->src_ptr  = key;
      e->n        = n;
      e->work_f32 = work_f32;
      e->conv     = conv;
      return conv;
    }
  }
  return nil;  // cache full
}

// Build an MPSMatrix view over a device buffer.  rows/cols are the matrix
// dims; the BLAS-row-major ld (leading dimension = elements per row in the
// physical layout) sets rowBytes.  For a transposed operand the physical
// layout is the un-transposed [cols, rows] tile so rowBytes keys off ld.
static MPSMatrix *metal_mps_matrix(id<MTLBuffer> buf, NSUInteger rows,
                                   NSUInteger cols, NSUInteger ld,
                                   MPSDataType dt, NSUInteger esz) {
  MPSMatrixDescriptor *d =
      [MPSMatrixDescriptor matrixDescriptorWithRows:rows
                                            columns:cols
                                           rowBytes:ld * esz
                                           dataType:dt];
  return [[MPSMatrix alloc] initWithBuffer:buf descriptor:d];
}

// Try to route this kernel to MPS.  Returns 1 on dispatch (work encoded
// into `cmd`), 0 on no-match (caller falls through to tile / per-op).
// src_bufs[i] are the ORIGINAL (possibly bf16) input MTLBuffers; outBuf is
// the output buffer (bf16 or f32).  All conversions + the MPS GEMM are
// encoded into `cmd`.
static int metal_try_mps_gemm(KernelEntry *ke, u32 *in_buf_ids,
                              u32 out_buf_id, id<MTLCommandBuffer> cmd) {
  if (!metal_mps_enabled() || METAL_DEVICE == nil) return 0;
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagGemmShape g;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, &g)) {
    return 0;
  }
  if (g.dtype != DT_BF16 && g.dtype != DT_FP32) return 0;
  // Resolve symbolic dims (identity for the literal FLUX projections).
  g.M = kvar_extent_runtime(g.M); g.N = kvar_extent_runtime(g.N);
  g.K = kvar_extent_runtime(g.K);
  g.ldA = kvar_extent_runtime(g.ldA); g.ldB = kvar_extent_runtime(g.ldB);
  if (g.M == 0 || g.N <= 1 || g.K <= 1) return 0;
  if ((u64)g.M * g.N * g.K < metal_mps_min_macs()) return 0;
  if (g.a_input >= ke->n_inputs || g.b_input >= ke->n_inputs) return 0;
  {
    static int trace_slot = -1;
    if (trace_slot == -1) {
      char const *e = getenv("THVM_METAL_MPS_TRACE");
      trace_slot = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
    }
    if (trace_slot) {
      fprintf(stderr, "[mps] kid=%u M=%u N=%u K=%u transA=%u transB=%u "
              "macs=%.1fM dt=%s\n", (u32)(ke - KERNELS), g.M, g.N, g.K,
              g.flags & 1u, (g.flags >> 1) & 1u,
              (double)((u64)g.M * g.N * g.K) / 1e6,
              g.dtype == DT_BF16 ? "bf16" : "f32");
    }
  }

  u32 a_buf = in_buf_ids[g.a_input];
  u32 b_buf = in_buf_ids[g.b_input];
  if (a_buf == 0 || a_buf >= METAL_BUFS_NEXT) return 0;
  if (b_buf == 0 || b_buf >= METAL_BUFS_NEXT) return 0;
  // A borrowed disk-mmap wrap carries a within-buffer byte_offset (the weight
  // sits past the page-aligned base).  The MPS bf16->f32 conversion kernels +
  // MPSMatrix views below read each operand from buffer offset 0, so a wrapped
  // input would feed the page-alignment padding into the GEMM.  Decline so the
  // dispatch falls through to the tile path, which applies byte_offset at the
  // input bind.  (MPS is a perf route, default-off; correctness over speed.)
  if (METAL_BUFS[a_buf].byte_offset != 0 || METAL_BUFS[b_buf].byte_offset != 0)
    return 0;
  // Same for the output: the MPSMatrix view over C_dst reads from buffer
  // offset 0, so an arena-VIEW output (a window of the shared arena MTLBuffer
  // at a nonzero byte_offset) would have MPS write at the arena base instead
  // of the slice.  Decline -> the tile/per-op path applies the output offset.
  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return 0;
  if (METAL_BUFS[out_buf_id].byte_offset != 0) return 0;
  id<MTLBuffer> A_src = METAL_BUFS[a_buf].buf;
  id<MTLBuffer> B_src = METAL_BUFS[b_buf].buf;
  id<MTLBuffer> C_dst = METAL_BUFS[out_buf_id].buf;
  if (A_src == nil || B_src == nil || C_dst == nil) return 0;
  // ldC: output row stride (>= N; matches the buffer's allocated leading
  // stride for a symbolic-seq output, identity for literal matmuls).
  u32 ldC = (u32)TENS[ke->output_tid].view.strides[0];
  if (ldC < g.N) ldC = g.N;

  int work_f32 = metal_mps_dtype_f32();
  // f32 working dtype with an f32 output avoids the output reconvert.
  int out_is_f32 = (g.dtype == DT_FP32);

  if (!metal_mps_build_cvt_kernels()) return 0;

  MPSDataType mdt = work_f32 ? MPSDataTypeFloat32 : MPSDataTypeFloat16;
  NSUInteger esz = work_f32 ? 4u : 2u;

  // --- Operand A (activation): convert bf16 -> working dtype if needed. ---
  id<MTLBuffer> A_work = A_src;
  id<MTLBuffer> A_tmp  = nil;
  if (g.dtype == DT_BF16) {
    A_tmp = [METAL_DEVICE newBufferWithLength:(NSUInteger)g.M * g.K * esz
                                      options:MTLResourceStorageModeShared];
    if (A_tmp == nil) return 0;
    metal_mps_encode_cvt(cmd,
        work_f32 ? METAL_MPS_CVT_BF16_TO_F32 : METAL_MPS_CVT_BF16_TO_F16,
        A_src, A_tmp, g.M * g.K);
    A_work = A_tmp;
  } else if (!work_f32) {
    // f32 input but f16 working dtype -- convert down.  (Uncommon; FLUX is
    // bf16.  Decline rather than add an f32->f16 kernel for a non-path.)
    return 0;
  }

  // --- Operand B (weight): cached working-dtype copy. ---
  id<MTLBuffer> B_work = B_src;
  id<MTLBuffer> B_tmp  = nil;
  if (g.dtype == DT_BF16) {
    B_work = metal_mps_weight_copy(B_src, g.K * g.N, work_f32, cmd);
    if (B_work == nil) return 0;
  } else if (!work_f32) {
    return 0;
  }

  // --- Output C: MPS writes working dtype; reconvert to bf16 after. ---
  id<MTLBuffer> C_work = C_dst;
  id<MTLBuffer> C_tmp  = nil;
  if (!out_is_f32) {
    // bf16 output (or f16 working over an f32 output -- but we declined
    // that above) -> MPS writes a working-dtype temp, then we convert it
    // to the bf16 output buffer.
    C_tmp = [METAL_DEVICE newBufferWithLength:(NSUInteger)g.M * g.N * esz
                                      options:MTLResourceStorageModeShared];
    if (C_tmp == nil) return 0;
    C_work = C_tmp;
  } else if (work_f32) {
    // f32 in, f32 out, f32 working -- MPS writes straight to C_dst.
    C_work = C_dst;
  } else {
    return 0;
  }

  // MPS matrix views.  BLAS row-major convention from the classifier:
  //   A: transA=(red!=1); physical [M,K] (ld=K) untransposed, or the
  //      stored [K,M] (ld=M) when transA.
  //   B: transB=(other!=1); physical [K,N] (ld=N) untransposed, or the
  //      stored [N,K] (ld=K) when transB.
  // MPSMatrix rows/cols describe the PHYSICAL (stored) matrix; the
  // transposeLeft/Right flags tell MPS to read it transposed.  So for a
  // transposed operand pass the stored [other, this] dims with ld.
  NSUInteger aRows, aCols, bRows, bCols;
  if (g.flags & 1u) { aRows = g.K; aCols = g.M; }   // stored [K,M]
  else              { aRows = g.M; aCols = g.K; }    // stored [M,K]
  if (g.flags & 2u) { bRows = g.N; bCols = g.K; }    // stored [N,K]
  else              { bRows = g.K; bCols = g.N; }    // stored [K,N]

  MPSMatrix *mA = metal_mps_matrix(A_work, aRows, aCols, g.ldA, mdt, esz);
  MPSMatrix *mB = metal_mps_matrix(B_work, bRows, bCols, g.ldB, mdt, esz);
  MPSMatrix *mC = metal_mps_matrix(C_work, g.M, g.N, ldC, mdt, esz);

  MPSMatrixMultiplication *mm =
      [[MPSMatrixMultiplication alloc] initWithDevice:METAL_DEVICE
                                        transposeLeft:(g.flags & 1u) ? YES : NO
                                       transposeRight:(g.flags & 2u) ? YES : NO
                                           resultRows:g.M
                                        resultColumns:g.N
                                      interiorColumns:g.K
                                                alpha:1.0
                                                 beta:0.0];
  [mm encodeToCommandBuffer:cmd leftMatrix:mA rightMatrix:mB resultMatrix:mC];

  if (C_tmp != nil) {
    metal_mps_encode_cvt(cmd,
        work_f32 ? METAL_MPS_CVT_F32_TO_BF16 : METAL_MPS_CVT_F16_TO_BF16,
        C_tmp, C_dst, g.M * g.N);
  }
  (void)A_tmp; (void)B_tmp;   // ARC retains them through the encode
  METAL_MPS_DISPATCH_COUNT++;
  return 1;
}

static int metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) return -1;
  // Per-op GPU profiling: clear the current-kid attribution before any
  // pre-dispatch flush (defer-backlog drain below) so leftover work
  // from the prior dispatch isn't double-counted; set it again once we
  // know this kernel's kid.
  METAL_PEROP_CUR_KID = 0;

  // Bound the deferred-decref backlog *between* kernels.  Within a
  // batched step the DAG-side per-op encoder materializes one
  // MTLBuffer per intermediate UOp and decref_after_batch's them all
  // -- but those decrefs run with METAL_ENCODING_DEPTH > 0, so the
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
  int dag_supported = metal_kernel_supported(ke);
  if (!tile_supported && !dag_supported) return -1;

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
            "[disp kid=%u n_inputs=%u out_numel=%llu in0_numel=%u tile=%d dag=%d]\n",
            kid, ke->n_inputs,
            (unsigned long long)ke->output_numel, in0,
            tile_supported, dag_supported);
    fflush(stderr);
  }

  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return -1;
  id<MTLBuffer> outBuf = METAL_BUFS[out_buf_id].buf;
  if (outBuf == nil) return -1;

  // View-aware pre-materialize state.  The actual strided-view ->
  // contiguous-temp copy is done LAZILY below, only once we know the
  // generated-tile path declined: metal_tile_jit_encode binds the
  // ORIGINAL strided input buffers directly and bakes the view
  // strides into the address expressions it emits, so it must NOT
  // see pre-materialised buffers.  The DAG-side per-op encoder, on
  // the other hand, reads inputs contiguously and DOES need the
  // pre-mat.  Doing it eagerly here (gated only on !tile_supported,
  // i.e. tile *eligibility*) was the conv-im2col Metal bug:
  // cg_tile_metal_dispatch_shape says "yes eligible" for the
  // im2col reduce kernel but metal_tile_jit_encode then bails --
  // and the per-op fall-through read the raw strided SHRINK-patch
  // buffers as if contiguous, computing garbage.
  u32 effective_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  u32 temp_buf_ids     [ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) temp_buf_ids[i] = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= METAL_BUFS_NEXT) { return -1; }
    effective_buf_ids[i] = ib;
  }

  __unsafe_unretained id<MTLBuffer> jit_src_bufs[ke->n_inputs ? ke->n_inputs : 1];
  u64 jit_src_offsets[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    jit_src_bufs[i]    = METAL_BUFS[effective_buf_ids[i]].buf;
    jit_src_offsets[i] = METAL_BUFS[effective_buf_ids[i]].byte_offset;
  }

  // MPS route: large 2-D GEMM -> MPSMatrixMultiplication (~2x the custom
  // tiled kernel on FLUX projection shapes).  Tried before the tile path
  // so big matmuls take the vendor GEMM; small ones (below the MAC
  // threshold) and non-matmul kernels fall through to the tile/per-op
  // paths.  Encodes conversions + MPS into a standalone command buffer.
  {
    id<MTLCommandBuffer> mps_cmd = metal_command_buffer();
    if (metal_try_mps_gemm(ke, in_buf_ids, out_buf_id, mps_cmd)) {
      metal_submit_if_standalone(mps_cmd);
      for (u32 i = 0; i < ke->n_inputs; i++) {
        if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
      }
      cg_profile_record(kid, KDISPATCH_METAL_MPS, cg_now_us() - t0);
      if (_disp_trace)
        fprintf(stderr, "[disp-done kid=%u path=mps us=%llu]\n", kid,
                (unsigned long long)(cg_now_us() - t0));
      return 0;
    }
  }

  if (tile_supported) {
    id<MTLCommandBuffer> tile_cmd = metal_command_buffer();
    if (metal_tile_jit_encode(ke, jit_src_bufs, jit_src_offsets, outBuf,
                              METAL_BUFS[out_buf_id].byte_offset, tile_cmd,
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

  if (!dag_supported) {
    for (u32 i = 0; i < ke->n_inputs; i++) {
      if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
    }
    return -1;
  }

  // Tile path declined (or wasn't eligible).  The DAG encoder below
  // reads inputs contiguously, so pre-materialise any strided-view
  // input now into a contiguous temp buffer (mirror of
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
    // A borrowed disk-mmap wrap starts at the page-aligned base; the weight's
    // element 0 is byte_offset bytes in.  Offset the CPU-side gather pointer so
    // strided reads land on the weight (the bind paths apply the same offset).
    char *base = (char *)[METAL_BUFS[ib].buf contents] + METAL_BUFS[ib].byte_offset;
    f32 *src = (f32 *)base;
    f32 *dst = (f32 *)[METAL_BUFS[tmp_id].buf contents];
    for (u32 k = 0; k < numel; k++) {
      dst[k] = src[metal_tendesc_strided_index(td, k)];
    }
    effective_buf_ids[i] = tmp_id;
    temp_buf_ids     [i] = tmp_id;
  }

  // DAG-side per-op encoder.  Fires when the lift succeeded; walks
  // ke->cached_lift.store_root and emits one MTLComputeCommandEncoder
  // per UOp tree node.  metal_kernel_supported already gates this on
  // store_root != 0, so reaching here without a lift would have
  // returned -1 above via the !dag_supported guard.
  METAL_ENCODING_DEPTH++;
  id<MTLCommandBuffer> dag_cmd = metal_command_buffer();
  int dag_ok = dag_metal_encode_kernel(ke, effective_buf_ids,
                                        out_buf_id, dag_cmd);
  METAL_ENCODING_DEPTH--;
  int rc = dag_ok ? 0 : -1;
  if (dag_ok) metal_submit_if_standalone(dag_cmd);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
  }
  if (rc == 0) cg_profile_record(kid, KDISPATCH_METAL_OP, cg_now_us() - t0);
  if (_disp_trace)
    fprintf(stderr, "[disp-done kid=%u path=dag rc=%d us=%llu]\n", kid, rc,
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
  .buf_jit_pin     = metal_buf_jit_pin,
  .buf_jit_unpin   = metal_buf_jit_unpin,
  .dispatch_begin  = metal_dispatch_begin,
  .dispatch_flush  = metal_dispatch_flush,
  .dispatch_end    = metal_dispatch_end,
  .dispatch_kernel = metal_dispatch_kernel,
  .gather_strided  = metal_gather_strided,
};
