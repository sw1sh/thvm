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

#ifndef THVM_METAL_METALLIB
#define THVM_METAL_METALLIB "build/default.metallib"
#endif

// Forward-declared here so metal_init can reset the length on
// repeated lifecycle cycles; the actual table + length live
// alongside METAL_BUFS below.
#define METAL_FREELIST_CAP 4096
static u32 METAL_FREELIST    [METAL_FREELIST_CAP];
static u32 METAL_FREELIST_LEN = 0;

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

// bm4c: Metal mirror of CPU's free-list (cpu_buf_freelist_push /
// cpu_buf_freelist_try_pop).  Recycles MTLBuffer slots by exact
// nbytes match; the underlying MTLBuffer object survives in the
// METAL_BUFS slot until the slot is popped or metal_shutdown
// clears it.  Drops refcount to 0 on push so the recycled-but-
// unallocated slot doesn't keep counting toward
// thvm_wl_metal_buf_table reports.
//
// No caller wired yet: thvm_realize's rollback only walks the
// CPU pool.  A backend-aware preserve+rollback (queued as a
// follow-up) will push from there.  Standalone the primitives
// + alloc-side recycling are still useful when a future
// metal_buf_decref hits zero -- the buf could go to the
// freelist instead of an outright nil-out.

static void metal_buf_freelist_push(u32 buf_id) {
  if (buf_id == 0 || buf_id >= METAL_BUFS_NEXT) return;
  if (METAL_FREELIST_LEN >= METAL_FREELIST_CAP) return;   // saturated
  if (METAL_BUFS[buf_id].buf == nil) return;              // already freed
  METAL_FREELIST[METAL_FREELIST_LEN++] = buf_id;
  METAL_BUFS[buf_id].refcount = 0;   // stop counting in live bytes
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
    return bid;
  }
  return 0;   // miss
}

static u32 metal_buf_alloc(u64 nbytes) {
  if (METAL_DEVICE == nil) return 0;
  // bm4c: free-list lookup first.  Recycles a matching-size
  // MTLBuffer slot if available; falls through to fresh
  // newBufferWithLength on miss.
  u32 recycled = metal_buf_freelist_try_pop(nbytes);
  if (recycled != 0) return recycled;
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

static void metal_shutdown(void) {
  METAL_FREELIST_LEN = 0;
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

static id<MTLComputePipelineState> METAL_PIPELINES[UOP_COUNT] = { nil };

static id<MTLComputePipelineState> metal_pipeline_for(uint32_t opcode) {
  if (opcode >= UOP_COUNT) return nil;
  if (METAL_PIPELINES[opcode] != nil) return METAL_PIPELINES[opcode];
  if (METAL_LIB == nil)               return nil;
  NSString *fnName = nil;
  switch (opcode) {
    case UOP_CONST: fnName = @"thvm_const"; break;
    case UOP_ADD:   fnName = @"thvm_add";   break;
    case UOP_MUL:   fnName = @"thvm_mul";   break;
    case UOP_CMPLT: fnName = @"thvm_cmplt"; break;
    case UOP_CMPEQ: fnName = @"thvm_cmpeq"; break;
    case UOP_NEG:   fnName = @"thvm_neg";   break;
    case UOP_RECIP: fnName = @"thvm_recip"; break;
    case UOP_SQRT:  fnName = @"thvm_sqrt";  break;
    case UOP_EXP2:  fnName = @"thvm_exp2";  break;
    case UOP_LOG2:  fnName = @"thvm_log2";  break;
    case UOP_REDUCE: fnName = @"thvm_reduce";  break;
    case UOP_EXPAND: fnName = @"thvm_expand";  break;
    case UOP_RESHAPE:fnName = @"thvm_reshape"; break;
    case UOP_FLIP:   fnName = @"thvm_flip";    break;
    case UOP_PAD:    fnName = @"thvm_pad";     break;
    case UOP_SHRINK: fnName = @"thvm_shrink";  break;
    case UOP_PERMUTE:fnName = @"thvm_permute"; break;
    default:         return nil;
  }
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
  METAL_PIPELINES[opcode] = pso;
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
  id<MTLComputePipelineState> pso = metal_pipeline_for(p->opcode);
  if (pso == nil) {
    fprintf(stderr, "thvm: metal dispatch -- no pipeline for opcode %u\n", p->opcode);
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

static int metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (METAL_DEVICE == nil || METAL_QUEUE == nil) return -1;
  if (ke->n_ops == 0) return -1;
  if (out_buf_id == 0 || out_buf_id >= METAL_BUFS_NEXT) return -1;
  id<MTLBuffer> outBuf = METAL_BUFS[out_buf_id].buf;
  if (outBuf == nil) return -1;

  // View-aware pre-materialize (the Metal counterpart to
  // cpu_interpret's strided pre-mat loop).  For each input whose
  // TenDesc carries a non-contiguous View, allocate a temp Metal
  // buffer and populate it via host-side strided index walk.
  u32 effective_buf_ids[KERNEL_MAX_INPUT];
  u32 temp_buf_ids     [KERNEL_MAX_INPUT] = {0};
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 ib = in_buf_ids[i];
    if (ib == 0 || ib >= METAL_BUFS_NEXT) { return -1; }
    u32 tid = ke->input_tids[i];
    int needs_premat = (tid != 0 && tid < TENS_NEXT
                        && !TENS[tid].view.contiguous);
    if (needs_premat) {
      View const *v = &TENS[tid].view;
      u32 numel = v->numel;
      u32 tmp_id = metal_buf_alloc((u64)numel * 4);
      if (tmp_id == 0) {
        for (u32 k = 0; k < i; k++)
          if (temp_buf_ids[k]) metal_buf_decref(temp_buf_ids[k]);
        return -1;
      }
      f32 *src = (f32 *)[METAL_BUFS[ib].buf contents];
      f32 *dst = (f32 *)[METAL_BUFS[tmp_id].buf contents];
      for (u32 k = 0; k < numel; k++) {
        int64_t acc = v->offset;
        u32 rem = k;
        for (i32 axis = (i32)v->shape.ndim - 1; axis >= 0; axis--) {
          u32 dim = v->shape.dims[axis];
          if (dim == 0) continue;
          u32 c = rem % dim;
          rem /= dim;
          acc += (int64_t)c * (int64_t)v->strides[axis];
        }
        dst[k] = src[(u32)acc];
      }
      effective_buf_ids[i] = tmp_id;
      temp_buf_ids     [i] = tmp_id;
    } else {
      effective_buf_ids[i] = ib;
    }
  }

  // Multi-op driver: mirror cpu_interpret.  Allocate one Metal buf
  // per intermediate op; final op writes to outBuf.  All ops run
  // inside a single MTLCommandBuffer with one encoder per op --
  // Metal hazard-tracks reads/writes of MTLResourceStorageModeShared
  // buffers across encoders in the same command buffer, so each
  // encoder naturally sees the previous encoder's writes without an
  // explicit barrier.
  u32 inter_buf_ids[KPROG_MAX_OPS] = {0};
  id<MTLCommandBuffer> cmd = [METAL_QUEUE commandBuffer];
  int rc = 0;

  for (u32 step = 0; step < ke->n_ops; step++) {
    KProgOp *p = &ke->program[step];

    // Skip prefix LOADs (mirror cpu_interpret line 59): when LOAD
    // appears before the final op, the input buffer is already
    // bound; downstream ops read it via KSRC_AS_INPUT directly.
    // The final-position LOAD (when it's the user-intended op via
    // TUOpLoad) still runs the memcpy kernel.
    if (p->opcode == UOP_LOAD && step + 1 < ke->n_ops) continue;

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

    // Decide where this op writes: last op -> outBuf; else allocate
    // a fresh intermediate Metal buffer at p->numel * 4 bytes.
    id<MTLBuffer> dst_buf;
    if (step + 1 == ke->n_ops) {
      dst_buf = outBuf;
    } else {
      u32 dst_numel = p->numel ? p->numel : 1;
      u32 tmp_id = metal_buf_alloc((u64)dst_numel * 4);
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
    [cmd commit];
    [cmd waitUntilCompleted];
  }

  // Cleanup: drop intermediate Metal buffers + view-pre-mat temps.
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (inter_buf_ids[i]) metal_buf_decref(inter_buf_ids[i]);
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (temp_buf_ids[i]) metal_buf_decref(temp_buf_ids[i]);
  }
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
  .dispatch_kernel = metal_dispatch_kernel,
};
