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

static u64 metal_defer_limit_bytes(void) {
  static int known = 0;
  static u64 limit = 1024ull * 1024ull * 1024ull;
  if (!known) {
    char const *e = getenv("THVM_METAL_DEFER_BYTES");
    if (e != NULL && e[0] != '\0') {
      limit = strtoull(e, NULL, 10);
    }
    known = 1;
  }
  return limit;
}

static u64 metal_freelist_limit_bytes(void) {
  char const *e = getenv("THVM_METAL_FREELIST_BYTES");
  if (e != NULL && e[0] != '\0') {
    return strtoull(e, NULL, 10);
  }
  return 1024ull * 1024ull * 1024ull;
}

static void metal_dispatch_flush(void) {
  id<MTLCommandBuffer> cmd = METAL_BATCH_CMD;
  METAL_BATCH_CMD = nil;
  if (cmd != nil) {
    [cmd commit];
    [cmd waitUntilCompleted];
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
}

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
  METAL_DEFER_DECREF_BYTES = 0;
  METAL_PEAK_LIVE_BYTES = 0;
  METAL_PEAK_RETAINED_BYTES = 0;
  METAL_PEAK_DEFERRED_BYTES = 0;
  return 0;
}

// Forward decls: metal_shutdown defined after the buffer table so
// it can iterate METAL_BUFS to release outstanding buffers.
static void metal_shutdown(void);

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
static id<MTLComputePipelineState> METAL_GEMM_PSOS[3];
static id<MTLComputePipelineState> METAL_CONV2D_PSO;

// metal_jit cache decls live further down (alongside the MSL emit
// path).  Forward-declare the cache reset so metal_shutdown can
// drop the stale PSOs before the MTLLibrary they reference goes
// away.
static void metal_jit_cache_reset_impl(void);
static void metal_graph_cache_reset_impl(void);

static void metal_shutdown(void) {
  metal_dispatch_flush();
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
  // a fresh MTLLibrary.  metal_jit's cache is cleared on the next
  // metal_init (defined further down so it can see METAL_JIT_CACHE
  // / METAL_JIT_PSOS).
  for (u32 op = 0; op < UOP_COUNT; op++)
    for (u32 dt = 0; dt < 32; dt++)
      METAL_PIPELINES_CACHE[op][dt] = nil;
  for (u32 i = 0; i < 3; i++) {
    METAL_GEMM_PSOS[i] = nil;
  }
  METAL_CONV2D_PSO = nil;
  metal_jit_cache_reset_impl();
  metal_graph_cache_reset_impl();
  METAL_LIB    = nil;
  METAL_QUEUE  = nil;
  METAL_DEVICE = nil;
}

// === metal_jit: fused-program shaders ==================================
//
// Compile cg_emit_metal output to an MTLLibrary at first dispatch and
// cache the resulting MTLComputePipelineState by program hash.
// Mirrors src/backend/cpu/jit.c structurally (same FNV-1a hash, same
// open-addressing cache layout) so a kernel that JITs cleanly on CPU
// will JIT cleanly here too.
//
// Cache miss: cg_emit_metal -> [MTLDevice newLibraryWithSource:
//   options:error:] -> [lib newFunctionWithName:@"k"] ->
//   [device newComputePipelineStateWithFunction:].  The PSO lives in
//   METAL_JIT_PSOS (ARC strong) so it survives the cache slot's
//   weak-style id<MTLComputePipelineState> reference.
//
// Buffer-binding convention (matches render_metal.c's prologue):
//   buffer(0)              : output (device float *)
//   buffer(1..1+n_in-1)    : inputs (device const float *)
// One thread per output element; threadgroup size capped at the
// pipeline's maxTotalThreadsPerThreadgroup.

#define METAL_JIT_CACHE_CAP 256
typedef struct {
  u64 key;   // 0 = empty
} MetalJitSlot;
static MetalJitSlot                METAL_JIT_CACHE[METAL_JIT_CACHE_CAP];
static id<MTLComputePipelineState> METAL_JIT_PSOS [METAL_JIT_CACHE_CAP];

// Forward-declared as metal_jit_cache_reset_impl above; called from
// metal_shutdown so the next metal_init starts with a clean slate.
static void metal_jit_cache_reset_impl(void) {
  for (u32 i = 0; i < METAL_JIT_CACHE_CAP; i++) {
    METAL_JIT_CACHE[i].key = 0;
    METAL_JIT_PSOS [i]     = nil;
  }
}

static u64 metal_jit_hash(KernelEntry const *ke) {
  u64 h = 0xcbf29ce484222325ULL;
  h ^= (u64)ke->n_ops;    h *= 0x100000001b3ULL;
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

static id<MTLComputePipelineState> metal_jit_build(KernelEntry const *ke, u64 key) {
  char *src = cg_emit_metal(ke);
  if (src == NULL) return nil;
  NSString *srcStr = [NSString stringWithUTF8String:src];
  free(src);
  NSError *err = nil;
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithSource:srcStr
                                                  options:nil
                                                    error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_jit -- compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"k"];
  if (mtlFn == nil) {
    fprintf(stderr, "thvm: metal_jit -- function 'k' missing in compiled lib\n");
    return nil;
  }
  MTLComputePipelineDescriptor *desc =
      [[MTLComputePipelineDescriptor alloc] init];
  [desc setComputeFunction:mtlFn];
  [desc setSupportIndirectCommandBuffers:YES];
  id<MTLComputePipelineState> pso =
      [METAL_DEVICE newComputePipelineStateWithDescriptor:desc
                                                   options:MTLPipelineOptionNone
                                                reflection:NULL
                                                     error:&err];
  if (pso == nil) {
    fprintf(stderr, "thvm: metal_jit -- pipeline-state failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  u32 idx = metal_jit_lookup_idx(key);
  if (idx != (u32)-1) {
    METAL_JIT_CACHE[idx].key = key;
    METAL_JIT_PSOS [idx]     = pso;
  }
  return pso;
}

static id<MTLComputePipelineState> metal_jit_pipeline(KernelEntry *ke) {
  if (ke->n_inputs > 30) return nil;
  if (cg_program_dtype(ke) != DT_FP32) return nil;
  u64 key = metal_jit_hash(ke);
  u32 idx = metal_jit_lookup_idx(key);
  if (idx != (u32)-1 && METAL_JIT_CACHE[idx].key == key) {
    return METAL_JIT_PSOS[idx];
  }
  return metal_jit_build(ke, key);
}

// Encode a single fused-shader dispatch onto `cmd`.  Returns 1 on
// success (caller commits the cmd buffer), 0 if the kernel can't be
// JIT-compiled (caller falls back to the per-op interpreter path).
// The src_bufs[] are post-pre-mat buffers from the caller -- this
// helper doesn't read TENS or in_buf_ids directly.
static int metal_jit_encode(KernelEntry *ke,
                            __unsafe_unretained id<MTLBuffer> *src_bufs,
                            id<MTLBuffer> outBuf,
                            id<MTLCommandBuffer> cmd) {
  // Apple Metal exposes buffer indices 0..30 on the devices we target;
  // index 0 is the output, so direct-pointer generated shaders can
  // bind at most 30 inputs without argument buffers.
  if (ke->n_inputs > 30) return 0;
  // Multi-output kernels need N output buffers bound to indices
  // 0..N-1 (and inputs shifted to N..N+n_inputs-1).  Until step 4+
  // wires the multi-output dispatch, refuse to encode and let the
  // caller fall back to per-op shaders.
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  // Metal MSL emitter is f32-only today; non-F32 kernels fall back
  // to the per-op pipeline path (which has dtype-specific shader
  // variants from Phase I).
  if (cg_program_dtype(ke) != DT_FP32) return 0;
  id<MTLComputePipelineState> pso = metal_jit_pipeline(ke);
  if (pso == nil) return 0;
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:outBuf offset:0 atIndex:0];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    [enc setBuffer:src_bufs[i] offset:0 atIndex:(1 + i)];
  }
  NSUInteger n = (NSUInteger)ke->program[ke->n_ops - 1].numel;
  if (n == 0) n = 1;
  NSUInteger tg = MIN(n, [pso maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  return 1;
}

static u32 metal_tendesc_strided_index(TenDesc const *t, u32 flat_idx);

static int metal_cpu_add_src_value(KernelEntry *ke,
                                   f32 const **inputs,
                                   TenDesc const **input_tds,
                                   f32 const *vals,
                                   u32 raw,
                                   u32 i,
                                   f32 *out) {
  u32 idx = KSRC_INDEX(raw);
  if (KSRC_IS_INPUT(raw)) {
    if (idx >= ke->n_inputs) {
      return 0;
    }
    u32 src_i = ke->input_numels[idx] == 1 ? 0 : i;
    if (input_tds[idx] != NULL) {
      src_i = metal_tendesc_strided_index(input_tds[idx], src_i);
    }
    *out = inputs[idx][src_i];
    return 1;
  }
  if (idx >= ke->n_ops) {
    return 0;
  }
  *out = vals[idx];
  return 1;
}

static int metal_try_cpu_small_add(KernelEntry *ke,
                                   u32 *in_buf_ids,
                                   u32 out_buf_id) {
  // Multi-output kernels write to extra output buffers via
  // KProgOp.store_extra_plus_one; this CPU-side small-add fast path
  // only writes to out_buf_id (primary), so we'd silently drop the
  // extra outputs.  Bail and let the per-op encoder (which DOES
  // honor store_extra_plus_one) run instead.
  if (cg_kernel_has_extra_outputs(ke)) {
    return 0;
  }
  if (ke->n_inputs <= 30 || ke->n_ops == 0) {
    return 0;
  }
  if (ke->output_dtype != DT_FP32 || ke->output_numel > 65536) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
    if (ke->input_numels[i] != 1 && ke->input_numels[i] != ke->output_numel) {
      return 0;
    }
  }
  for (u32 step = 0; step < ke->n_ops; step++) {
    KProgOp const *p = &ke->program[step];
    if (p->numel != ke->output_numel) {
      return 0;
    }
    if (p->opcode == UOP_RESHAPE) {
      if (p->n_src != 1) {
        return 0;
      }
    } else if (p->opcode == UOP_ADD) {
      if (p->n_src != 2) {
        return 0;
      }
    } else {
      return 0;
    }
  }

  metal_dispatch_flush();
  f32 const *inputs[ke->n_inputs ? ke->n_inputs : 1];
  TenDesc const *input_tds[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= METAL_BUFS_NEXT || METAL_BUFS[ib].buf == nil) {
      return 0;
    }
    inputs[i] = (f32 const *)[METAL_BUFS[ib].buf contents];
    u32 tid = ke->input_tids[i];
    input_tds[i] = (tid != 0 && tid < TENS_NEXT) ? &TENS[tid] : NULL;
  }
  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT
      || METAL_BUFS[out_buf_id].buf == nil) {
    return 0;
  }
  f32 *out = (f32 *)[METAL_BUFS[out_buf_id].buf contents];
  f32 vals[ke->n_ops ? ke->n_ops : 1];
  for (u32 i = 0; i < ke->output_numel; i++) {
    for (u32 step = 0; step < ke->n_ops; step++) {
      KProgOp const *p = &ke->program[step];
      if (p->opcode == UOP_RESHAPE) {
        if (!metal_cpu_add_src_value(ke, inputs, input_tds, vals, p->src[0],
                                     i, &vals[step])) {
          return 0;
        }
      } else {
        f32 a;
        f32 b;
        if (!metal_cpu_add_src_value(ke, inputs, input_tds, vals, p->src[0],
                                     i, &a)
            || !metal_cpu_add_src_value(ke, inputs, input_tds, vals,
                                        p->src[1], i, &b)) {
          return 0;
        }
        vals[step] = a + b;
      }
    }
    out[i] = vals[ke->n_ops - 1];
  }
  return 1;
}

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
  char const *e = getenv("THVM_TILE");
  return e != NULL && e[0] == '1';
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
  if (ke->scalar_uops != NULL && ke->n_scalar_uops > 0) {
    u8 const *bytes = (u8 const *)ke->scalar_uops;
    size_t total = (size_t)ke->n_scalar_uops * sizeof(ScalarUop);
    for (size_t i = 0; i < total; i++) {
      h ^= (u64)bytes[i]; h *= 0x100000001b3ULL;
    }
  }
  if (ke->tile_uops != NULL && ke->n_tile_uops > 0) {
    u8 const *bytes = (u8 const *)ke->tile_uops;
    size_t total = (size_t)ke->n_tile_uops * sizeof(TileUop);
    for (size_t i = 0; i < total; i++) {
      h ^= (u64)bytes[i]; h *= 0x100000001b3ULL;
    }
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h ^= (u64)ke->input_dtypes[i]; h *= 0x100000001b3ULL;
    h ^= (u64)ke->input_numels[i]; h *= 0x100000001b3ULL;
  }
  h ^= (u64)ke->n_inputs;          h *= 0x100000001b3ULL;
  h ^= (u64)ke->output_dtype;      h *= 0x100000001b3ULL;
  h ^= (u64)ke->output_numel;      h *= 0x100000001b3ULL;
  if (ke->axes != NULL) {
    h ^= (u64)ke->axes->n_applied; h *= 0x100000001b3ULL;
    u8 const *opts = (u8 const *)ke->axes->applied_opts;
    size_t total = (size_t)ke->axes->n_applied * sizeof(KOpt);
    for (size_t i = 0; i < total; i++) {
      h ^= (u64)opts[i]; h *= 0x100000001b3ULL;
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
  NSError *err = nil;
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithSource:srcStr
                                                  options:nil
                                                    error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_tile_jit -- compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
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
  id<MTLComputePipelineState> pso =
      [METAL_DEVICE newComputePipelineStateWithDescriptor:desc
                                                   options:MTLPipelineOptionNone
                                                reflection:NULL
                                                     error:&err];
  if (pso == nil) {
    fprintf(stderr, "thvm: metal_tile_jit -- pipeline-state failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  u32 idx = metal_jit_lookup_idx(key);
  if (idx != (u32)-1) {
    METAL_JIT_CACHE[idx].key = key;
    METAL_JIT_PSOS [idx]     = pso;
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
    return METAL_JIT_PSOS[idx];
  }
  return metal_tile_jit_build(ke, key);
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
    u32 dispatch_kind = cg_kernel_dispatch_kind(ops[i].kid);
    cg_profile_record(ops[i].kid,
                      dispatch_kind == KDISPATCH_METAL_JIT
                          ? KDISPATCH_METAL_JIT
                          : KDISPATCH_METAL_TILE,
                      per);
  }
  return 0;
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
static int metal_kernel_supported(struct KernelEntry const *ke) {
  if (ke->n_ops == 0) return 0;
  u32 dt = ke->program[0].dtype;
  // v1: only the dtypes that have shader variants in the metallib.
  // f32 always; i32 added in Phase I.  f16 / bf16 / fp8 / int4 fall
  // back to CPU until per-dtype shader variants land for them.
  if (dt != DT_FP32 && dt != DT_INT32) return 0;
  for (u32 i = 0; i < ke->n_ops; i++)
    if (ke->program[i].dtype != dt) return 0;
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_dtypes[i] != dt) return 0;
  return 1;
}

static int metal_gemm_tile_index(u32 tile, u32 *idx) {
  switch (tile) {
    case 8:  *idx = 0; return 1;
    case 16: *idx = 1; return 1;
    case 32: *idx = 2; return 1;
    default: return 0;
  }
}

static int metal_specialized_diagnostics_enabled(void) {
  char const *e = getenv("THVM_METAL_SPECIALIZED");
  return e != NULL && e[0] == '1';
}

static int metal_cpu_small_add_enabled(void) {
  char const *e = getenv("THVM_METAL_CPU_SMALL_ADD");
  return e != NULL && e[0] == '1';
}

static int metal_kernel_has_applied_opt(struct KernelEntry const *ke, u8 op) {
  if (ke == NULL || ke->axes == NULL) {
    return 0;
  }
  for (u32 i = 0; i < ke->axes->n_applied; i++) {
    if (ke->axes->applied_opts[i].op == op) {
      return 1;
    }
  }
  return 0;
}

static id<MTLComputePipelineState> metal_gemm_pipeline(u32 tile) {
  u32 idx = 0;
  if (!metal_gemm_tile_index(tile, &idx)) {
    return nil;
  }
  if (METAL_GEMM_PSOS[idx] != nil) return METAL_GEMM_PSOS[idx];
  char src[4096];
  int nw = snprintf(src, sizeof(src),
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "#define TILE %uu\n"
      "kernel void thvm_gemm_tiled(device const float *A [[buffer(0)]],\n"
      "                            device const float *B [[buffer(1)]],\n"
      "                            device float *C [[buffer(2)]],\n"
      "                            constant uint *cfg [[buffer(3)]],\n"
      "                            uint2 tid [[thread_position_in_threadgroup]],\n"
      "                            uint2 gid [[threadgroup_position_in_grid]]) {\n"
      "  uint M = cfg[0], N = cfg[1], K = cfg[2];\n"
      "  uint ldA = cfg[3], ldB = cfg[4], flags = cfg[5];\n"
      "  bool transA = (flags & 1u) != 0u;\n"
      "  bool transB = (flags & 2u) != 0u;\n"
      "  uint row = gid.y * TILE + tid.y;\n"
      "  uint col = gid.x * TILE + tid.x;\n"
      "  threadgroup float As[%u];\n"
      "  threadgroup float Bs[%u];\n"
      "  uint lid = tid.y * TILE + tid.x;\n"
      "  float acc = 0.0f;\n"
      "  for (uint k0 = 0; k0 < K; k0 += TILE) {\n"
      "    uint ak = k0 + tid.x;\n"
      "    uint bk = k0 + tid.y;\n"
      "    float av = 0.0f;\n"
      "    float bv = 0.0f;\n"
      "    if (row < M && ak < K) {\n"
      "      av = transA ? A[ak * ldA + row] : A[row * ldA + ak];\n"
      "    }\n"
      "    if (col < N && bk < K) {\n"
      "      bv = transB ? B[col * ldB + bk] : B[bk * ldB + col];\n"
      "    }\n"
      "    As[lid] = av;\n"
      "    Bs[lid] = bv;\n"
      "    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
      "    for (uint kk = 0; kk < TILE && k0 + kk < K; kk++) {\n"
      "      acc += As[tid.y * TILE + kk] * Bs[kk * TILE + tid.x];\n"
      "    }\n"
      "    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
      "  }\n"
      "  if (row < M && col < N) {\n"
      "    C[row * N + col] = acc;\n"
      "  }\n"
      "}\n",
      tile, tile * tile, tile * tile);
  if (nw <= 0 || (size_t)nw >= sizeof(src)) {
    return nil;
  }
  NSError *err = nil;
  NSString *srcStr = [NSString stringWithUTF8String:src];
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithSource:srcStr
                                                  options:nil
                                                    error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_gemm -- compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"thvm_gemm_tiled"];
  if (mtlFn == nil) return nil;
  METAL_GEMM_PSOS[idx] = [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn
                                                                      error:&err];
  if (METAL_GEMM_PSOS[idx] == nil) {
    fprintf(stderr, "thvm: metal_gemm -- pipeline-state failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
  }
  return METAL_GEMM_PSOS[idx];
}

static u32 metal_isqrt_exact(u32 x) {
  for (u32 r = 1; r * r <= x; r++) {
    if (r * r == x) return r;
  }
  return 0;
}

static id<MTLComputePipelineState> metal_conv2d_pipeline(void) {
  if (METAL_CONV2D_PSO != nil) return METAL_CONV2D_PSO;
  static char const *src =
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "kernel void thvm_conv2d_flat(device const float *W [[buffer(0)]],\n"
      "                             device const float *X [[buffer(1)]],\n"
      "                             device float *Y [[buffer(2)]],\n"
      "                             constant int *cfg [[buffer(3)]],\n"
      "                             uint gid [[thread_position_in_grid]]) {\n"
      "  uint total = (uint)(cfg[0] * cfg[8]);\n"
      "  if (gid >= total) return;\n"
      "  int co = (int)(gid / (uint)cfg[8]);\n"
      "  int p  = (int)(gid - (uint)co * (uint)cfg[8]);\n"
      "  int ow = p % cfg[7];\n"
      "  int oh = p / cfg[7];\n"
      "  float acc = 0.0f;\n"
      "  for (int ci = 0; ci < cfg[1]; ci++) {\n"
      "    for (int ki = 0; ki < cfg[4]; ki++) {\n"
      "      for (int kj = 0; kj < cfg[5]; kj++) {\n"
      "        int q = ((ci * cfg[4]) + ki) * cfg[5] + kj;\n"
      "        int wi = cfg[9] + co * cfg[10] + q * cfg[11];\n"
      "        int xi = cfg[12] + ci * cfg[13] + (oh + ki) * cfg[14]\n"
      "               + (ow + kj) * cfg[15];\n"
      "        acc += W[wi] * X[xi];\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "  Y[gid] = acc;\n"
      "}\n";
  NSError *err = nil;
  NSString *srcStr = [NSString stringWithUTF8String:src];
  id<MTLLibrary> lib = [METAL_DEVICE newLibraryWithSource:srcStr
                                                  options:nil
                                                    error:&err];
  if (lib == nil) {
    fprintf(stderr, "thvm: metal_conv2d -- compile failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
    return nil;
  }
  id<MTLFunction> mtlFn = [lib newFunctionWithName:@"thvm_conv2d_flat"];
  if (mtlFn == nil) return nil;
  METAL_CONV2D_PSO = [METAL_DEVICE newComputePipelineStateWithFunction:mtlFn
                                                                  error:&err];
  if (METAL_CONV2D_PSO == nil) {
    fprintf(stderr, "thvm: metal_conv2d -- pipeline-state failed: %s\n",
            err ? [[err localizedDescription] UTF8String] : "(no error)");
  }
  return METAL_CONV2D_PSO;
}

static int metal_try_conv2d_flat(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke == NULL || ke->n_inputs != 2 || ke->n_ops == 0) return 0;
  if (ke->input_dtypes[0] != DT_FP32 || ke->input_dtypes[1] != DT_FP32) return 0;
  if (TENS[ke->output_tid].dtype != DT_FP32) return 0;
  KProgOp const *last = &ke->program[ke->n_ops - 1];
  if (last->opcode != UOP_REDUCE) return 0;

  u32 w_tid = ke->input_tids[0];
  u32 x_tid = ke->input_tids[1];
  if (w_tid == 0 || x_tid == 0 || w_tid >= TENS_NEXT || x_tid >= TENS_NEXT) {
    return 0;
  }
  TenDesc const *w = &TENS[w_tid];
  TenDesc const *x = &TENS[x_tid];
  TenDesc const *y = &TENS[ke->output_tid];
  if (w->nviews != 0 || x->nviews != 0 || y->nviews != 0) return 0;
  View const *wv = &w->view;
  View const *xv = &x->view;
  View const *yv = &y->view;
  if (wv->shape.ndim != 3 || xv->shape.ndim != 3 || yv->shape.ndim != 2) {
    return 0;
  }
  u32 cout = wv->shape.dims[0];
  u32 k    = wv->shape.dims[1];
  u32 p    = wv->shape.dims[2];
  u32 cin  = xv->shape.dims[0];
  u32 h    = xv->shape.dims[1];
  u32 wd   = xv->shape.dims[2];
  if (cout == 0 || cin == 0 || k == 0 || p == 0) return 0;
  if (yv->shape.dims[0] != cout || yv->shape.dims[1] != p) return 0;
  if (k % cin != 0) return 0;
  u32 kspat = k / cin;
  u32 kh = metal_isqrt_exact(kspat);
  if (kh == 0) return 0;
  u32 kw = kh;
  if (h < kh || wd < kw) return 0;
  u32 hout = h - kh + 1;
  u32 wout = wd - kw + 1;
  if (hout * wout != p) return 0;
  if (wv->strides[2] != 0) return 0;

  u32 wb = in_buf_ids[0], xb = in_buf_ids[1];
  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return 0;
  if (wb == 0 || xb == 0 || wb >= METAL_BUFS_NEXT || xb >= METAL_BUFS_NEXT) {
    return 0;
  }
  if (METAL_BUFS[wb].buf == nil || METAL_BUFS[xb].buf == nil
      || METAL_BUFS[out_buf_id].buf == nil) {
    return 0;
  }

  id<MTLComputePipelineState> pso = metal_conv2d_pipeline();
  if (pso == nil) return 0;
  id<MTLCommandBuffer> cmd = metal_command_buffer();
  if (cmd == nil) return 0;
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:METAL_BUFS[wb].buf offset:0 atIndex:0];
  [enc setBuffer:METAL_BUFS[xb].buf offset:0 atIndex:1];
  [enc setBuffer:METAL_BUFS[out_buf_id].buf offset:0 atIndex:2];
  int cfg[16] = {
    (int)cout, (int)cin, (int)h,    (int)wd,
    (int)kh,   (int)kw,  (int)hout, (int)wout,
    (int)p,    wv->offset, wv->strides[0], wv->strides[1],
    xv->offset, xv->strides[0], xv->strides[1], xv->strides[2],
  };
  [enc setBytes:cfg length:sizeof(cfg) atIndex:3];
  NSUInteger n = (NSUInteger)(cout * p);
  NSUInteger tg = MIN((NSUInteger)256, [pso maxTotalThreadsPerThreadgroup]);
  [enc dispatchThreads:MTLSizeMake(n, 1, 1)
   threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
  [enc endEncoding];
  metal_submit_if_standalone(cmd);
  return 1;
}

static int metal_try_gemm(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  TileGemmInfo gemm;
  if (!tile_collect_mma_plan(ke, &gemm) || gemm.dtype != DT_FP32) {
    return 0;
  }
  if (gemm.a_input >= ke->n_inputs || gemm.b_input >= ke->n_inputs
      || gemm.a_input == gemm.b_input) {
    return 0;
  }

  u32 b0 = in_buf_ids[gemm.a_input];
  u32 b1 = in_buf_ids[gemm.b_input];
  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) {
    return 0;
  }
  if (b0 == 0 || b1 == 0 || b0 >= METAL_BUFS_NEXT || b1 >= METAL_BUFS_NEXT) {
    return 0;
  }
  if (METAL_BUFS[b0].buf == nil || METAL_BUFS[b1].buf == nil
      || METAL_BUFS[out_buf_id].buf == nil) {
    return 0;
  }
  u32 a_numel = (u32)(METAL_BUFS[b0].nbytes / sizeof(float));
  u32 b_numel = (u32)(METAL_BUFS[b1].nbytes / sizeof(float));
  if (a_numel != gemm.M * gemm.K || b_numel != gemm.K * gemm.N) {
    return 0;
  }

  u32 tile = gemm.tile_size;
  u32 tile_idx = 0;
  if (!metal_gemm_tile_index(tile, &tile_idx)) {
    tile = 16;
  }
  id<MTLComputePipelineState> pso = metal_gemm_pipeline(tile);
  if (pso == nil) {
    return 0;
  }
  NSUInteger mtile = (NSUInteger)tile;
  if ([pso maxTotalThreadsPerThreadgroup] < mtile * mtile) {
    return 0;
  }
  id<MTLCommandBuffer> cmd = metal_command_buffer();
  if (cmd == nil) {
    return 0;
  }
  id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
  [enc setComputePipelineState:pso];
  [enc setBuffer:METAL_BUFS[b0].buf offset:0 atIndex:0];
  [enc setBuffer:METAL_BUFS[b1].buf offset:0 atIndex:1];
  [enc setBuffer:METAL_BUFS[out_buf_id].buf offset:0 atIndex:2];
  u32 cfg[6] = {gemm.M, gemm.N, gemm.K, gemm.ldA, gemm.ldB, gemm.flags};
  [enc setBytes:cfg length:sizeof(cfg) atIndex:3];
  MTLSize groups = MTLSizeMake(((NSUInteger)gemm.N + mtile - 1) / mtile,
                               ((NSUInteger)gemm.M + mtile - 1) / mtile,
                               1);
  MTLSize threads = MTLSizeMake(mtile, mtile, 1);
  [enc dispatchThreadgroups:groups threadsPerThreadgroup:threads];
  [enc endEncoding];
  metal_submit_if_standalone(cmd);
  return 1;
}

static int metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) return -1;

  u32 tile_groups_x  = 0;
  u32 tile_threads_x = 0;
  int tile_supported = metal_tile_enabled()
      && cg_tile_metal_dispatch_shape(ke, &tile_groups_x, &tile_threads_x);
  TileConv2DInfo tile_conv_info;
  int tile_conv_supported = tile_supported
      && tile_analyze_conv2d_flat(ke, &tile_conv_info);
  (void)tile_conv_info;
  int kprog_supported = metal_kernel_supported(ke);
  if (!tile_supported && !kprog_supported) return -1;

  // Profile this dispatch.  kid = ke - KERNELS gives the slot index
  // the WL TKernelProfile / TKernelDispatchKind surface reads.
  u32 kid = (u32)(ke - KERNELS);
  u64 t0  = cg_now_us();

  if (kprog_supported && metal_try_alias_reshape(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_METAL_ALIAS, cg_now_us() - t0);
    return 0;
  }

  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return -1;
  id<MTLBuffer> outBuf = METAL_BUFS[out_buf_id].buf;
  if (outBuf == nil) return -1;

  if (kprog_supported && metal_specialized_diagnostics_enabled()) {
    if (metal_try_conv2d_flat(ke, in_buf_ids, out_buf_id)) {
      cg_profile_record(kid, KDISPATCH_METAL_CONV, cg_now_us() - t0);
      return 0;
    }
  }

  if (kprog_supported && metal_try_gemm(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_METAL_GEMM, cg_now_us() - t0);
    return 0;
  }

  if (kprog_supported && metal_cpu_small_add_enabled()
      && metal_try_cpu_small_add(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_METAL_OP, cg_now_us() - t0);
    return 0;
  }

  if (!tile_supported && cg_supports_metal_reduce_expr(ke)) {
    __unsafe_unretained id<MTLBuffer> raw_src_bufs[ke->n_inputs ? ke->n_inputs : 1];
    int raw_ok = 1;
    for (u32 i = 0; i < ke->n_inputs; i++) {
      u32 ib = in_buf_ids[i];
      if (ib == 0 || ib >= METAL_BUFS_NEXT || METAL_BUFS[ib].buf == nil) {
        raw_ok = 0;
        break;
      }
      raw_src_bufs[i] = METAL_BUFS[ib].buf;
    }
    if (raw_ok) {
      id<MTLCommandBuffer> jit_cmd = metal_command_buffer();
      if (metal_jit_encode(ke, raw_src_bufs, outBuf, jit_cmd)) {
        metal_submit_if_standalone(jit_cmd);
        cg_profile_record(kid, KDISPATCH_METAL_JIT, cg_now_us() - t0);
        return 0;
      }
    }
  }

  // View-aware pre-materialize (the Metal counterpart to
  // cpu_interpret's strided pre-mat loop).  For each input whose
  // TenDesc carries a non-contiguous View, allocate a temp Metal
  // buffer and populate it via host-side strided index walk.
  // Sized to ke->n_inputs (KERNEL_MAX_INPUT is now a sanity bound,
  // not a typical size).  ke->n_inputs == 0 is unusual but possible
  // for a no-arg kernel (e.g. CONST root); the +1 keeps VLA legal.
  u32 effective_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  u32 temp_buf_ids     [ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) temp_buf_ids[i] = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= METAL_BUFS_NEXT) { return -1; }
    u32 tid = ke->input_tids[i];
    TenDesc const *td = (tid != 0 && tid < TENS_NEXT) ? &TENS[tid] : NULL;
    int needs_premat = (!tile_supported
                        && td != NULL
                        && (!td->view.contiguous
                            || td->view.offset != 0
                            || td->nviews != 0));
    if (needs_premat) {
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
    } else {
      effective_buf_ids[i] = ib;
    }
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
      return 0;
    }
  }

  if (!kprog_supported) {
    for (u32 i = 0; i < ke->n_inputs; i++) {
      if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
    }
    return -1;
  }

  // Try the JIT path: if cg_supports(ke), render the whole KProgOp[]
  // to a single MSL kernel and dispatch one encoder.  metal_jit_encode
  // returns 1 when it successfully encoded (caller commits + waits) and
  // 0 to bail (cg_supports rejected, or compile/PSO failed -- either way
  // we fall through to the per-op path below, which handles REDUCE +
  // movement that the JIT can't yet).
  {
    id<MTLCommandBuffer> jit_cmd = metal_command_buffer();
    if (metal_jit_encode(ke, jit_src_bufs, outBuf, jit_cmd)) {
      metal_submit_if_standalone(jit_cmd);
      for (u32 i = 0; i < ke->n_inputs; i++) {
        if (temp_buf_ids[i]) metal_buf_decref_after_batch(temp_buf_ids[i]);
      }
      cg_profile_record(kid, KDISPATCH_METAL_JIT, cg_now_us() - t0);
      return 0;
    }
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
      u32 tmp_id = metal_buf_alloc(dtype_storage_bytes(p->dtype, dst_numel));
      if (tmp_id == 0) { rc = -1; break; }
      inter_buf_ids[step] = tmp_id;
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
  return rc;
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
  .dispatch_begin  = metal_dispatch_begin,
  .dispatch_flush  = metal_dispatch_flush,
  .dispatch_end    = metal_dispatch_end,
  .dispatch_kernel = metal_dispatch_kernel,
};
