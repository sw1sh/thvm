// codegen/profile.c - per-kernel introspection: FLOPS, dispatch
// route, source rendering hooks.  Backend-agnostic; the actual
// dispatch counters are bumped from the per-backend dispatchers
// (cpu_blas_dispatch, cpu_jit_dispatch, cpu_uop_walk).
//
// The ops and bytes of a kernel run are tinygrad's Estimates
// (renderer/__init__.py:16-61), taken over the lifted DAG, and every
// dispatch adds them to tinygrad's GlobalCounters.

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

// tinygrad's Estimates.from_uops (renderer/__init__.py:26-61) over a lifted
// DAG, where a node runs once per value of the RANGE axes it reads, its free
// axes (a REDUCE binds its own): an ALU node charges the product of their
// extents in ops and a REDUCE one combine per iteration of its source, and a
// buffer's loads, and apart its stores, charge the bytes their addresses
// span, capped at the buffer's size.  Free axes are a bitmask over the first
// 64 axes met; a node past them charges as if it ran once.
typedef struct {
  u64  *memo_loc;
  u64  *memo_axes;
  u32   memo_cap, memo_n;
  u32   axis_id[64];
  u64   axis_ext[64];
  u32   n_axes;
  Term  buf[64];
  u8    buf_store[64];
  u64   buf_bytes[64], buf_acc[64];
  u32   n_bufs;
  u64   ops;
} CgEst;

static u64 cg_est_prod(CgEst const *e, u64 axes) {
  u64 p = 1;
  for (u32 i = 0; i < e->n_axes; i++) {
    if ((axes >> i) & 1) {
      p = e->axis_ext[i] != 0 && p > UINT64_MAX / e->axis_ext[i] ? UINT64_MAX : p * e->axis_ext[i];
    }
  }
  return p;
}

static u64 cg_est_axis(CgEst *e, u32 id, u64 ext) {
  for (u32 i = 0; i < e->n_axes; i++) {
    if (e->axis_id[i] == id) {
      return 1ull << i;
    }
  }
  if (e->n_axes == 64) {
    return 0;
  }
  e->axis_id[e->n_axes] = id;
  e->axis_ext[e->n_axes] = ext;
  return 1ull << e->n_axes++;
}

static void cg_est_access(CgEst *e, Term buf, u64 axes, u8 store) {
  if (term_tag(buf) != TAG_UOP || term_ext(buf) != UOP_BUFFER || uop_buffer_scope(buf) != UOP_SCOPE_GLOBAL) {
    return;
  }
  u64 isz = dtype_itemsize(uop_buffer_dtype(buf));
  u32 k = 0;
  while (k < e->n_bufs && !(e->buf[k] == buf && e->buf_store[k] == store)) {
    k++;
  }
  if (k == e->n_bufs) {
    if (k == 64) {
      return;
    }
    u64 numel = 1;
    for (u32 d = 0; d < uop_buffer_ndim(buf); d++) {
      numel *= uop_buffer_dim(buf, d);
    }
    e->buf[k] = buf;
    e->buf_store[k] = store;
    e->buf_bytes[k] = numel * isz;
    e->buf_acc[k] = 0;
    e->n_bufs++;
  }
  u64 acc = e->buf_acc[k] + cg_est_prod(e, axes) * isz;
  e->buf_acc[k] = acc < e->buf_bytes[k] ? acc : e->buf_bytes[k];
}

static u64 cg_est_axes(CgEst *e, Term t, u32 depth) {
  if (term_tag(t) != TAG_UOP || depth > 4096) {
    return 0;
  }
  u64 loc = term_val(t);
  u32 h = (u32)((loc * 0x9E3779B97F4A7C15ull) >> 40) & (e->memo_cap - 1);
  while (e->memo_loc[h] != 0) {
    if (e->memo_loc[h] == loc + 1) {
      return e->memo_axes[h];
    }
    h = (h + 1) & (e->memo_cap - 1);
  }
  u32 op = term_ext(t);
  u64 axes = 0;
  switch (op) {
    case UOP_RANGE:
      axes = cg_est_axis(e, uop_range_axis_id(t), uop_range_extent(t));
      break;
    case UOP_OPT:
      axes = cg_est_axes(e, uop_opt_target(t), depth + 1);
      break;
    case UOP_REDUCE: {
      axes = cg_est_axes(e, uop_reduce_src(t), depth + 1);
      e->ops += cg_est_prod(e, axes);
      for (u32 i = 0; i < uop_reduce_n_axes(t); i++) {
        axes &= ~cg_est_axis(e, uop_reduce_axis(t, i), 1);
      }
      break;
    }
    case UOP_INDEX_E:
      axes = cg_est_axes(e, heap_read(loc + 1), depth + 1);
      cg_est_access(e, heap_read(loc + 0), axes, 0);
      break;
    case UOP_STORE:
      axes = cg_est_axes(e, heap_read(loc + 1), depth + 1);
      cg_est_access(e, heap_read(loc + 0), axes, 1);
      axes |= cg_est_axes(e, heap_read(loc + 2), depth + 1);
      break;
    case UOP_AFTER:
      axes = cg_est_axes(e, heap_read(loc + 0), depth + 1) | cg_est_axes(e, heap_read(loc + 1), depth + 1);
      break;
    default: {
      for (u8 i = 0; i < uop_arity((u8)op) && i < MAX_UOP_SRC; i++) {
        axes |= cg_est_axes(e, heap_read(loc + i), depth + 1);
      }
      if (uop_is_unary_elementwise((u8)op) || uop_is_binary_elementwise((u8)op)
          || op == UOP_IADD || op == UOP_ISUB || op == UOP_IMUL || op == UOP_ILT || op == UOP_IWHERE) {
        e->ops += cg_est_prod(e, axes);
      }
      break;
    }
  }
  if (2 * (e->memo_n + 1) > e->memo_cap) {
    // Grow, rehashing, so the walk stays linear in the DAG.
    u32 cap = e->memo_cap * 2;
    u64 *nl = (u64 *)calloc(cap, sizeof(u64)), *na = (u64 *)calloc(cap, sizeof(u64));
    if (nl == NULL || na == NULL) {
      free(nl); free(na);
      return axes;
    }
    for (u32 k = 0; k < e->memo_cap; k++) {
      if (e->memo_loc[k] == 0) continue;
      u32 g = (u32)(((e->memo_loc[k] - 1) * 0x9E3779B97F4A7C15ull) >> 40) & (cap - 1);
      while (nl[g] != 0) g = (g + 1) & (cap - 1);
      nl[g] = e->memo_loc[k];
      na[g] = e->memo_axes[k];
    }
    free(e->memo_loc); free(e->memo_axes);
    e->memo_loc = nl;
    e->memo_axes = na;
    e->memo_cap = cap;
  }
  h = (u32)((loc * 0x9E3779B97F4A7C15ull) >> 40) & (e->memo_cap - 1);
  while (e->memo_loc[h] != 0) h = (h + 1) & (e->memo_cap - 1);
  e->memo_loc[h] = loc + 1;
  e->memo_axes[h] = axes;
  e->memo_n++;
  return axes;
}

// A run's ops and bytes, cached on the kernel for the root they were taken
// from.  0 for a kernel with no lifted DAG.
fn void cg_kernel_estimates(KernelEntry *ke, u64 *ops, u64 *mem) {
  if (ke->est_root != ke->cached_lift.store_root || ke->est_root == 0) {
    CgEst e = {0};
    e.memo_cap = 1024;
    e.memo_loc = (u64 *)calloc(e.memo_cap, sizeof(u64));
    e.memo_axes = (u64 *)calloc(e.memo_cap, sizeof(u64));
    if (e.memo_loc != NULL && e.memo_axes != NULL && ke->cached_lift.store_root != 0) {
      cg_est_axes(&e, ke->cached_lift.store_root, 0);
    }
    free(e.memo_loc);
    free(e.memo_axes);
    ke->est_ops = e.ops;
    ke->est_mem = 0;
    for (u32 k = 0; k < e.n_bufs; k++) ke->est_mem += e.buf_acc[k];
    ke->est_root = ke->cached_lift.store_root;
  }
  *ops = ke->est_ops;
  *mem = ke->est_mem;
}

fn u64 cg_kernel_flops(KernelEntry const *ke) {
  u64 ops = 0, mem = 0;
  if (ke != NULL) cg_kernel_estimates((KernelEntry *)ke, &ops, &mem);
  return ops;
}

// tinygrad's GlobalCounters (helpers.py:296-304), counted from start: the
// kernels run and their ops and bytes (engine/realize.py:65-67).  py's
// GlobalCounters reads them less their values at its last reset.
static u64 GC_COUNTS[3];

fn void cg_count_dispatch(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) return;
  u64 ops, mem;
  cg_kernel_estimates(&KERNELS[kid], &ops, &mem);
  GC_COUNTS[0] += 1;
  GC_COUNTS[1] += ops;
  GC_COUNTS[2] += mem;
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

// True when THVM_KERNEL_PROFILE=N (N != 0) requests a per-kernel dump.
// The Metal backend consults this to switch the batched ICB replay onto
// the one-command-buffer-per-op path, so a plain THVM_KERNEL_PROFILE run
// gets TRUE per-kernel GPU timestamps instead of wall/n_ops averages.
// Cached env read (single-threaded init path mirrors the other toggles).
int cg_profile_kernel_enabled(void) {
  static int known = 0;
  static int enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_KERNEL_PROFILE");
    enabled = (e != NULL && e[0] != '\0' && !(e[0] == '0' && e[1] == '\0'));
    known = 1;
  }
  return enabled;
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

// Snapshot / restore the whole per-kid profile slot.  The post-capture
// isolated autotune (codegen/autotune.c) FIRES the kernel many times on
// scratch buffers; each fire calls cg_profile_record, mutating
// K_PROFILE[kid].kind -- which cg_kernel_dispatch_kind feeds into the JIT
// replay's ICB-batching decision (jit_replay_try_metal_graph_run).  If a
// benched tile variant takes a different dispatch route than the captured
// baseline, the search would leave `kind` wrong and the warm replay's
// batching would diverge from the capture's.  Snapshot before the search,
// restore after, so the search is profile-state-neutral.  Opaque u64[4]
// blob (KProfileSlot is 5 u64-sized fields) so the caller needs no struct.
void cg_profile_snapshot(u32 kid, u64 out[6]) {
  for (u32 i = 0; i < 6; i++) out[i] = 0;
  if (kid == 0 || kid >= KPROFILE_CAP) return;
  KProfileSlot const *s = &K_PROFILE[kid];
  out[0] = (u64)s->kind;
  out[1] = s->dispatch_count;
  out[2] = s->total_us;
  out[3] = s->gpu_us;
  out[4] = s->gpu_samples;
}

void cg_profile_restore(u32 kid, u64 const in[6]) {
  if (kid == 0 || kid >= KPROFILE_CAP) return;
  KProfileSlot *s = &K_PROFILE[kid];
  s->kind           = (KDispatchKind)in[0];
  s->dispatch_count = in[1];
  s->total_us       = in[2];
  s->gpu_us         = in[3];
  s->gpu_samples    = in[4];
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
    case KDISPATCH_JIT:        return "cpu_jit";
    case KDISPATCH_INTERPRETER:return "uop_walk";
    case KDISPATCH_METAL_TILE: return "metal";
    case KDISPATCH_METAL_OP:   return "metal_op";
    case KDISPATCH_METAL_MPS:  return "metal_mps";
    case KDISPATCH_CUDA_JIT:   return "cuda_jit";
    case KDISPATCH_CUSTOM:     return "custom";
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
  fprintf(fp, "[thvm prof]  kid    fires       total_us       avg_us  pct       gpu_us   avg_gpu  route       flops/fire  shape\n");
  for (u32 i = 0; i < top_n; i++) {
    u32 kid = kids[i];
    KProfileSlot const *s = &K_PROFILE[kid];
    KernelEntry const *ke = &KERNELS[kid];
    u64 avg = s->dispatch_count ? (s->total_us / s->dispatch_count) : 0;
    double pct = total_all ? (100.0 * (double)s->total_us / (double)total_all) : 0.0;
    u64 avg_gpu = s->gpu_samples ? (s->gpu_us / s->gpu_samples) : 0;
    char shape_buf[96];
    int n = 0;
    n += snprintf(shape_buf + n, (int)sizeof(shape_buf) - n, "[");
    for (u32 d = 0; d < ke->output_shape.ndim && d < MAX_DIM; d++) {
      n += snprintf(shape_buf + n, (int)sizeof(shape_buf) - n,
                    "%s%u", d ? "," : "", ke->output_shape.dims[d]);
    }
    snprintf(shape_buf + n, (int)sizeof(shape_buf) - n, "]");
    u64 flops = cg_kernel_flops(ke);
    fprintf(fp, "[thvm prof] %4u %8llu %14llu %12llu %4.1f%% %12llu %9llu %-10s %11llu  %s\n",
            kid,
            (unsigned long long)s->dispatch_count,
            (unsigned long long)s->total_us,
            (unsigned long long)avg,
            pct,
            (unsigned long long)s->gpu_us,
            (unsigned long long)avg_gpu,
            kdispatch_tag(s->kind),
            (unsigned long long)flops,
            shape_buf);
  }
  fflush(fp);
}
