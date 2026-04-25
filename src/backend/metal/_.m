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

static int  metal_init(void)                                    { return 0; }
static void metal_shutdown(void)                                { /* nop */ }

static u32  metal_buf_alloc(u64 nbytes)                         { (void)nbytes;     return 0; }
static void metal_buf_free (u32 buf_id)                         { (void)buf_id;     /* nop */ }
static void metal_buf_incref(u32 buf_id)                        { (void)buf_id;     /* nop */ }
static void metal_buf_decref(u32 buf_id)                        { (void)buf_id;     /* nop */ }
static int  metal_buf_read (u32 buf_id, void *dst, u64 nbytes)  { (void)buf_id; (void)dst; (void)nbytes; return -1; }
static int  metal_buf_write(u32 buf_id, const void *src, u64 nbytes) { (void)buf_id; (void)src; (void)nbytes; return -1; }

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
