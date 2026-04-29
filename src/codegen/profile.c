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

typedef enum {
  KDISPATCH_NONE        = 0,
  KDISPATCH_BLAS_DOT    = 1,
  KDISPATCH_BLAS_GEMV   = 2,
  KDISPATCH_BLAS_GEMM   = 3,
  KDISPATCH_JIT         = 4,
  KDISPATCH_INTERPRETER = 5,
} KDispatchKind;

// Per-kid running counters.  Array indexed by kid; reset on
// thvm_init via cg_profile_reset.  Out-of-range kids ignored.
#define KPROFILE_CAP   65536
typedef struct {
  KDispatchKind kind;          // last route taken for this kid
  u64           dispatch_count; // total fires
  u64           total_us;       // cumulative wall time
} KProfileSlot;
static KProfileSlot K_PROFILE[KPROFILE_CAP];

fn void cg_profile_reset(void) {
  memset(K_PROFILE, 0, sizeof(K_PROFILE));
}

// Compute static FLOPs for a single execution of the kernel program.
// Returns u64 -- typical kernels stay well below 2^63 even for
// 4096x4096 GEMV (=33 MFLOP per fire).
fn u64 cg_kernel_flops(KernelEntry const *ke) {
  if (ke == NULL) return 0;
  u64 total = 0;
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp const *p = &ke->program[i];
    u64 elem = (u64)(p->numel ? p->numel : 1);
    switch (p->opcode) {
      // Elementwise unary: 1 flop / element.
      case UOP_NEG: case UOP_RECIP: case UOP_SQRT:
      case UOP_EXP2: case UOP_LOG2:
        total += elem; break;
      // Elementwise binary: 1 flop / element.
      case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
        total += elem; break;
      // REDUCE: ~1 flop / element of the SOURCE (we only track output
      // numel here; the SUM accumulator does ~1 add per source element).
      // Look up src0 numel via the program.
      case UOP_REDUCE: {
        u32 raw = p->src[0];
        if (KSRC_IS_INPUT(raw)) {
          u32 idx = KSRC_INDEX(raw);
          if (idx < ke->n_inputs) total += ke->input_numels[idx];
        } else {
          u32 idx = KSRC_INDEX(raw);
          if (idx < ke->n_ops) total += ke->program[idx].numel;
        }
        break;
      }
      // CONST / movement / LOAD: 0 flops.
      default: break;
    }
  }
  return total;
}

fn void cg_profile_record(u32 kid, KDispatchKind kind, u64 elapsed_us) {
  if (kid == 0 || kid >= KPROFILE_CAP) return;
  KProfileSlot *s = &K_PROFILE[kid];
  s->kind            = kind;
  s->dispatch_count += 1;
  s->total_us       += elapsed_us;
}

fn u32 cg_kernel_dispatch_kind(u32 kid) {
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

// Wallclock helper -- gettimeofday / clock_gettime on macOS.
#include <sys/time.h>
fn u64 cg_now_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (u64)tv.tv_sec * 1000000 + (u64)tv.tv_usec;
}
