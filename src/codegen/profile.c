// codegen/profile.c - per-kernel introspection: FLOPS, dispatch
// route, source rendering hooks.  Backend-agnostic; the actual
// dispatch counters are bumped from the per-backend dispatchers
// (cpu_blas_dispatch, cpu_jit_dispatch, cpu_interpret).
//
// FLOPS estimate is the static count baked into the lifted DAG:
// per UOp we charge a tiny constant cost based on its opcode
// (1 flop for ADD/MUL, 2 for FMA-style chains, 1 for unary, 0 for
// movement / reshape / load).  Multiplied by the op's element
// count because each compute node writes one value per element.
// This is the same methodology tinygrad's `BEAM` profiler uses;
// it's a coarse number but useful for "are we hitting BLAS GFLOPs?"
// sanity checks.

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

// THVM_KERNEL_PROFILE=N: arm an atexit handler the first time any
// kernel dispatch records itself.  Independent of thvm_free (which py
// only calls on explicit Thvm.close()) so a Ctrl-C / sys.exit path
// still prints the table.  atexit handlers run in LIFO order; the
// dispatch table the dumper reads (K_PROFILE + KERNELS[]) is process-
// global side data, valid right up to libc teardown.
static int  PROFILE_ATEXIT_ARMED = 0;
static u32  PROFILE_ATEXIT_TOP_N = 0;
static void cg_profile_atexit(void) {
  // Skip when the table is empty (e.g. thvm_free already ran and reset
  // K_PROFILE).  Avoids a duplicate "0 kernels" footer when both the
  // explicit thvm_free dump AND the atexit handler fire.
  for (u32 i = 1; i < KPROFILE_CAP; i++) {
    if (K_PROFILE[i].dispatch_count != 0) {
      cg_profile_dump(stderr, PROFILE_ATEXIT_TOP_N);
      return;
    }
  }
}
static void cg_profile_arm_atexit_once(void) {
  if (PROFILE_ATEXIT_ARMED) return;
  PROFILE_ATEXIT_ARMED = 1;
  char const *e = getenv("THVM_KERNEL_PROFILE");
  if (e == NULL || e[0] == '\0' || (e[0] == '0' && e[1] == '\0')) return;
  u32 n = (u32)atoi(e);
  if (n == 0) n = 20;
  PROFILE_ATEXIT_TOP_N = n;
  atexit(cg_profile_atexit);
}

void cg_profile_record(u32 kid, KDispatchKind kind, u64 elapsed_us) {
  if (kid == 0 || kid >= KPROFILE_CAP) return;
  cg_profile_arm_atexit_once();
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

// THVM_KERNEL_PROFILE=N dump: print the top-N kernels by total
// wall-time at thvm_free. Each line is one kid: total_us, fires,
// avg_us, dispatch route, output shape, FLOPS-per-fire estimate.
// Routed through cg_profile_dump so the .c TU calls a single hook;
// implementation lives next to the per-kid table.
//
// Route id -> short tag (mirrors KDispatchKind in thvm.h).
static char const *kdispatch_tag(KDispatchKind k) {
  switch (k) {
    case KDISPATCH_BLAS_GEMM:  return "blas_gemm";
    case KDISPATCH_BLAS_GEMV:  return "blas_gemv";
    case KDISPATCH_BLAS_DOT:   return "blas_dot";
    case KDISPATCH_JIT:        return "cpu_jit";
    case KDISPATCH_INTERPRETER:return "uop_walk";
    case KDISPATCH_METAL_TILE: return "metal";
    case KDISPATCH_METAL_OP:   return "metal_op";
    default:                   return "none";
  }
}

void cg_profile_dump(FILE *fp, u32 top_n) {
  if (fp == NULL) return;
  if (top_n == 0) top_n = 20;
  // Build kid-index array sorted by total_us desc. Skip kid==0.
  u32 kids[KPROFILE_CAP];
  u32 nkids = 0;
  u64 total_all = 0;
  for (u32 i = 1; i < KPROFILE_CAP; i++) {
    if (K_PROFILE[i].dispatch_count == 0) continue;
    kids[nkids++] = i;
    total_all += K_PROFILE[i].total_us;
  }
  // Simple selection sort over top_n (nkids tends to be <2000; this is
  // a one-shot at shutdown, no point pulling in qsort).
  if (top_n > nkids) top_n = nkids;
  for (u32 i = 0; i < top_n; i++) {
    u32 best = i;
    for (u32 j = i + 1; j < nkids; j++) {
      if (K_PROFILE[kids[j]].total_us > K_PROFILE[kids[best]].total_us) best = j;
    }
    u32 tmp = kids[i]; kids[i] = kids[best]; kids[best] = tmp;
  }
  fprintf(fp, "\n[thvm prof] %u kernels with samples, total_wall=%llu us\n",
          nkids, (unsigned long long)total_all);
  fprintf(fp, "[thvm prof]  kid    fires       total_us       avg_us  pct  route       flops/fire  shape\n");
  for (u32 i = 0; i < top_n; i++) {
    u32 kid = kids[i];
    KProfileSlot const *s = &K_PROFILE[kid];
    KernelEntry const *ke = &KERNELS[kid];
    u64 avg = s->dispatch_count ? (s->total_us / s->dispatch_count) : 0;
    double pct = total_all ? (100.0 * (double)s->total_us / (double)total_all) : 0.0;
    char shape_buf[96];
    int n = 0;
    n += snprintf(shape_buf + n, (int)sizeof(shape_buf) - n, "[");
    for (u32 d = 0; d < ke->output_shape.ndim && d < MAX_DIM; d++) {
      n += snprintf(shape_buf + n, (int)sizeof(shape_buf) - n,
                    "%s%u", d ? "," : "", ke->output_shape.dims[d]);
    }
    snprintf(shape_buf + n, (int)sizeof(shape_buf) - n, "]");
    u64 flops = cg_kernel_flops(ke);
    fprintf(fp, "[thvm prof] %4u %8llu %14llu %12llu %4.1f%% %-10s %11llu  %s\n",
            kid,
            (unsigned long long)s->dispatch_count,
            (unsigned long long)s->total_us,
            (unsigned long long)avg,
            pct,
            kdispatch_tag(s->kind),
            (unsigned long long)flops,
            shape_buf);
  }
  fflush(fp);
}
