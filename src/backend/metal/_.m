// backend/metal/_.m -- Metal backend stub, Objective-C edition.
//
// Same semantics as src/backend/metal/_.c but compiled separately
// from the single-TU runtime.  Linked into binaries that build with
// -DTHVM_HAS_METAL (where src/thvm.c skips including the .c stub).
//
// Real Metal init (MTLDevice, MTLCommandQueue, metallib loading)
// lands in the next two task items.  This file just establishes
// the dual-TU build shape so the Makefile changes can be tested
// independently from the Objective-C runtime calls.

#include <stdio.h>
#include <stdint.h>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

// Minimal local copies of the runtime types we touch.  Keeping them
// inline avoids a chain of #includes through src/thvm.c (which is
// the umbrella include for the C runtime and would clash with the
// Objective-C compilation context).
typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;

struct KernelEntry;

typedef struct Backend {
  u32   id;
  int   (*init)(void);
  void  (*shutdown)(void);
  u32   (*buf_alloc)(u64 nbytes);
  void  (*buf_free) (u32 buf_id);
  void  (*buf_incref)(u32 buf_id);
  void  (*buf_decref)(u32 buf_id);
  int   (*buf_read) (u32 buf_id, void *dst, u64 nbytes);
  int   (*buf_write)(u32 buf_id, const void *src, u64 nbytes);
  int   (*dispatch_kernel)(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
} Backend;

// Lazy file-scope handles to the system default Metal device, a
// command queue, and the loaded metallib.  ARC owns all three;
// metal_shutdown releases by nilling them.  All stay nil between
// init/shutdown cycles so repeated thvm_init calls reset cleanly.
static id<MTLDevice>       METAL_DEVICE = nil;
static id<MTLCommandQueue> METAL_QUEUE  = nil;
static id<MTLLibrary>      METAL_LIB    = nil;

#ifndef THVM_METAL_METALLIB
#define THVM_METAL_METALLIB "build/default.metallib"
#endif

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
} MetalBuf;

static MetalBuf METAL_BUFS[METAL_BUFS_CAP];
static u32      METAL_BUFS_NEXT = 1;

static u32 metal_buf_alloc(u64 nbytes) {
  if (METAL_DEVICE == nil) return 0;
  if (METAL_BUFS_NEXT >= METAL_BUFS_CAP) {
    fprintf(stderr, "thvm: metal_buf_alloc -- buffer table full\n");
    return 0;
  }
  u32 id = METAL_BUFS_NEXT++;
  METAL_BUFS[id].buf      = [METAL_DEVICE newBufferWithLength:nbytes
                                                      options:MTLResourceStorageModeShared];
  METAL_BUFS[id].nbytes   = nbytes;
  METAL_BUFS[id].refcount = 1;
  if (METAL_BUFS[id].buf == nil) {
    fprintf(stderr, "thvm: metal_buf_alloc -- failed to allocate %llu bytes\n",
            (unsigned long long)nbytes);
    METAL_BUFS[id].refcount = 0;
    return 0;
  }
  return id;
}

static void metal_buf_free(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  METAL_BUFS[buf_id].buf      = nil;
  METAL_BUFS[buf_id].nbytes   = 0;
  METAL_BUFS[buf_id].refcount = 0;
}

static void metal_buf_incref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  METAL_BUFS[buf_id].refcount++;
}

static void metal_buf_decref(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (METAL_BUFS[buf_id].refcount == 0) return;
  if (--METAL_BUFS[buf_id].refcount == 0) metal_buf_free(buf_id);
}

static int metal_buf_read(u32 buf_id, void *dst, u64 nbytes) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[buf_id].buf == nil)            return -1;
  u64 cap = METAL_BUFS[buf_id].nbytes;
  if (nbytes > cap) nbytes = cap;
  memcpy(dst, [METAL_BUFS[buf_id].buf contents], (size_t)nbytes);
  return 0;
}

static int metal_buf_write(u32 buf_id, const void *src, u64 nbytes) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return -1;
  if (METAL_BUFS[buf_id].buf == nil)            return -1;
  u64 cap = METAL_BUFS[buf_id].nbytes;
  if (nbytes > cap) nbytes = cap;
  memcpy([METAL_BUFS[buf_id].buf contents], src, (size_t)nbytes);
  return 0;
}

static void metal_shutdown(void) {
  for (u32 i = 1; i < METAL_BUFS_NEXT; i++) {
    METAL_BUFS[i].buf      = nil;
    METAL_BUFS[i].nbytes   = 0;
    METAL_BUFS[i].refcount = 0;
  }
  METAL_BUFS_NEXT = 1;
  METAL_LIB    = nil;
  METAL_QUEUE  = nil;
  METAL_DEVICE = nil;
}

static int  metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  (void)ke; (void)in_buf_ids; (void)out_buf_id;
  fprintf(stderr, "thvm: metal backend (.m stub) -- dispatch_kernel not implemented\n");
  return -1;
}

Backend METAL_BACKEND = {
  .id              = 2,
  .init            = metal_init,
  .shutdown        = metal_shutdown,
  .buf_alloc       = metal_buf_alloc,
  .buf_free        = metal_buf_free,
  .buf_incref      = metal_buf_incref,
  .buf_decref      = metal_buf_decref,
  .buf_read        = metal_buf_read,
  .buf_write       = metal_buf_write,
  .dispatch_kernel = metal_dispatch_kernel,
};
