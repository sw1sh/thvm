// codegen/profile.c - per-kernel introspection: FLOPS, dispatch
// route, source rendering hooks.  Backend-agnostic; the actual
// dispatch counters are bumped from the per-backend dispatchers
// (cpu_blas_dispatch, cpu_jit_dispatch, cpu_interpret).
//
// FLOPS estimate is the static count baked into the kernel program:
// per KProgOp we charge a tiny constant cost based on its opcode
// (1 flop for ADD/MUL, 2 for FMA-style chains, 1 for unary, 0 for
// movement / reshape / load).  Multiplied by op->numel because each
// program slot writes one value per element.  This is the same
// methodology tinygrad's `BEAM` profiler uses; it's a coarse number
// but useful for "are we hitting BLAS GFLOPs?" sanity checks.

// KDispatchKind is declared in thvm.h so the Metal .m file (compiled
// in a separate TU) can pass route ids back to cg_profile_record.

// Per-kid running counters.  Array indexed by kid; reset on
// thvm_init via cg_profile_reset.  Out-of-range kids ignored.
#define KPROFILE_CAP   65536
typedef struct {
  KDispatchKind kind;          // last route taken for this kid
  u64           dispatch_count; // total fires
  u64           total_us;       // cumulative wall time
  // True GPU execution time, summed across fires.  Only populated on
  // the Metal backend when THVM_METAL_PROFILE_PEROP=1 (each kernel
  // then dispatches in its own command buffer, so
  // [cmd GPUEndTime]-[cmd GPUStartTime] is a per-kernel number).
  // Otherwise zero -- distinguish "no GPU samples" from "0 us".
  u64           gpu_us;
  u64           gpu_samples;    // # of GPU-time samples folded in
} KProfileSlot;
static KProfileSlot K_PROFILE[KPROFILE_CAP];

fn void cg_profile_reset(void) {
  memset(K_PROFILE, 0, sizeof(K_PROFILE));
}

// Walk a lifted DAG counting FLOPs.  Each elementwise compute op
// contributes `iter_extent` flops; UOP_REDUCE multiplies its body's
// inner contribution by the reduce-axis extent (the source iterates
// reduce_extent times per output element).  Bounded depth.
static u64 cg_kernel_flops_dag(Term t, u64 iter_extent, u32 depth) {
  if (depth > 256 || iter_extent == 0 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID
      || op == UOP_RANGE || op == UOP_INDEX_E) return 0;
  u64 total = 0;
  u64 loc = term_val(t);
  if (op == UOP_OPT) {
    return cg_kernel_flops_dag(uop_opt_target(t), iter_extent, depth + 1);
  }
  if (op == UOP_REDUCE) {
    // Find the reduce-axis RANGE extent by walking the body.
    u32 red_axis = (u32)term_val(heap_read(loc + 2));
    Term src = heap_read(loc + 0);
    u32 red_ext = 0;
    Term stack[64];
    u32 sp = 0;
    if (term_tag(src) == TAG_UOP) stack[sp++] = src;
    while (sp > 0 && red_ext == 0) {
      Term n = stack[--sp];
      if (term_tag(n) != TAG_UOP) continue;
      u32 nop = term_ext(n);
      if (nop == UOP_RANGE && uop_range_axis_id(n) == red_axis) {
        red_ext = uop_range_extent(n);
        break;
      }
      if (nop == UOP_BUFFER || nop == UOP_CONST || nop == UOP_INVALID) continue;
      u8 ar = uop_arity((u8)nop);
      u64 nloc = term_val(n);
      for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 64; i++) {
        Term c = heap_read(nloc + i);
        if (term_tag(c) == TAG_UOP) stack[sp++] = c;
      }
    }
    if (red_ext == 0) red_ext = 1;
    // The REDUCE itself contributes one accumulator-combine op per
    // source element (the implicit SUM/MAX/MIN over the body's value).
    u64 body_iter = iter_extent * (u64)red_ext;
    total += body_iter;
    // Recurse into body with multiplied iter_extent.
    total += cg_kernel_flops_dag(src, body_iter, depth + 1);
    return total;
  }
  // STORE/AFTER: pass through to value.
  if (op == UOP_STORE) {
    Term val = heap_read(loc + 2);
    return cg_kernel_flops_dag(val, iter_extent, depth + 1);
  }
  if (op == UOP_AFTER) {
    total += cg_kernel_flops_dag(heap_read(loc + 0), iter_extent, depth + 1);
    total += cg_kernel_flops_dag(heap_read(loc + 1), iter_extent, depth + 1);
    return total;
  }
  // Elementwise compute ops: count this node then recurse.
  switch (op) {
    case UOP_NEG: case UOP_RECIP: case UOP_SQRT:
    case UOP_EXP2: case UOP_LOG2:
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
      total += iter_extent;
      break;
    default: break;
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term c = heap_read(loc + i);
    if (term_tag(c) == TAG_UOP) {
      total += cg_kernel_flops_dag(c, iter_extent, depth + 1);
    }
  }
  return total;
}

// Compute static FLOPs for a single execution of the kernel program.
// Returns u64 -- typical kernels stay well below 2^63 even for
// 4096x4096 GEMV (=33 MFLOP per fire).
fn u64 cg_kernel_flops(KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u64 iter = ke->output_numel ? (u64)ke->output_numel : 1;
  return cg_kernel_flops_dag(ke->cached_lift.store_root, iter, 0);
}

void cg_profile_record(u32 kid, KDispatchKind kind, u64 elapsed_us) {
  if (kid == 0 || kid >= KPROFILE_CAP) return;
  KProfileSlot *s = &K_PROFILE[kid];
  s->kind            = kind;
  s->dispatch_count += 1;
  s->total_us       += elapsed_us;
}

u32 cg_kernel_dispatch_kind(u32 kid) {
  if (kid == 0 || kid >= KPROFILE_CAP) return KDISPATCH_NONE;
  return (u32)K_PROFILE[kid].kind;
}

fn u64 cg_kernel_dispatch_count(u32 kid) {
  if (kid == 0 || kid >= KPROFILE_CAP) return 0;
  return K_PROFILE[kid].dispatch_count;
}

fn u64 cg_kernel_total_us(u32 kid) {
  if (kid == 0 || kid >= KPROFILE_CAP) return 0;
  return K_PROFILE[kid].total_us;
}

// Record a true per-kernel GPU execution time sample (microseconds).
// Called from the Metal backend's standalone-submit path when
// THVM_METAL_PROFILE_PEROP=1.  External linkage: the .m TU calls it.
void cg_profile_record_gpu(u32 kid, u64 gpu_us) {
  if (kid == 0 || kid >= KPROFILE_CAP) return;
  KProfileSlot *s = &K_PROFILE[kid];
  s->gpu_us      += gpu_us;
  s->gpu_samples += 1;
}

fn u64 cg_kernel_gpu_us(u32 kid) {
  if (kid == 0 || kid >= KPROFILE_CAP) return 0;
  return K_PROFILE[kid].gpu_us;
}

fn u64 cg_kernel_gpu_samples(u32 kid) {
  if (kid == 0 || kid >= KPROFILE_CAP) return 0;
  return K_PROFILE[kid].gpu_samples;
}

// Wallclock helper -- gettimeofday / clock_gettime on macOS.
#include <sys/time.h>
u64 cg_now_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (u64)tv.tv_sec * 1000000 + (u64)tv.tv_usec;
}
