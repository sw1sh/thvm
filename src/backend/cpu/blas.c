// backend/cpu/blas.c - pattern-recognise matmul / matvec / dot
// kernels in the materialized KProgOp[] and dispatch them to
// Apple Accelerate (cblas_*) instead of the interpreter or JIT
// codegen.  10-100x speedup on the hot ML workload (matmul) -- a
// kernel that the JIT path doesn't even attempt today (it bails
// at REDUCE).
//
// Pattern matching is deliberately narrow today: we recognise
// only the exact KProgOp[] sequences that TDot / TMatVec produce.
// General matmul (rank-2 x rank-2 -> rank-2) needs an axis-aware
// EXPAND + REDUCE shape match that we'll add when there's a
// concrete consumer.
//
// Patterns:
//   DOT: 2 contig f32 inputs, both numel N.
//        op[0] = MUL(in0, in1)     numel N
//        op[1] = REDUCE_SUM(op[0]) numel 1
//        -> cblas_sdot(N, in0, 1, in1, 1)
//
//   GEMV: 2 contig f32 inputs.
//         W (numel M*K, in0), x (numel K with broadcast or {1,K}, in1).
//         op[0] = MUL(in0, in1)     numel M*K
//         op[1] = REDUCE_SUM(op[0], axis=1) numel M
//         -> cblas_sgemv(M, K, 1.0, W, K, x, 1, 0.0, out, 1)
//
// The `arg` field of REDUCE encodes (kind << 24) | inner; we use
// that to confirm SUM and recover the axis.

#ifdef __APPLE__
#define ACCELERATE_NEW_LAPACK
#include <Accelerate/Accelerate.h>
#define HAVE_BLAS 1
#else
#define HAVE_BLAS 0
#endif

#if HAVE_BLAS

// REDUCE op's `arg` field: bits 24..31 = kind, bits 0..23 = inner
// (= product of dims after the reduced axis).  Mirrors the encoding
// used by cpu_op_reduce / materialize.c's kernel emit for REDUCE.
#define BLAS_REDUCE_KIND(arg) (((arg) >> 24) & 0xFFu)
#define BLAS_REDUCE_INNER(arg) ((arg) & 0xFFFFFFu)

static int blas_op_is_mul_of(KProgOp const *p, u32 in_a, u32 in_b) {
  if (p->opcode != UOP_MUL || p->n_src != 2) return 0;
  u32 a = p->src[0], b = p->src[1];
  if (!KSRC_IS_INPUT(a) || !KSRC_IS_INPUT(b)) return 0;
  u32 ai = KSRC_INDEX(a), bi = KSRC_INDEX(b);
  return (ai == in_a && bi == in_b) || (ai == in_b && bi == in_a);
}

static int blas_op_is_reduce_sum(KProgOp const *p, u32 src_step) {
  if (p->opcode != UOP_REDUCE || p->n_src != 1) return 0;
  if (BLAS_REDUCE_KIND(p->arg) != REDUCE_SUM) return 0;
  u32 s = p->src[0];
  if (KSRC_IS_INPUT(s)) return 0;
  return KSRC_INDEX(s) == src_step;
}

// Try DOT.  Returns 1 on dispatch, 0 on no-match (caller fall-back).
static int blas_try_dot(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->n_inputs != 2) return 0;
  if (ke->n_ops    != 2) return 0;
  if (ke->input_dtypes[0] != DT_F32 || ke->input_dtypes[1] != DT_F32) return 0;
  if (ke->program[0].dtype != DT_F32 || ke->program[1].dtype != DT_F32) return 0;
  u32 n0 = ke->input_numels[0], n1 = ke->input_numels[1];
  if (n0 != n1 || n0 == 0) return 0;
  if (ke->program[0].numel != n0) return 0;
  if (ke->program[1].numel != 1)  return 0;
  if (!blas_op_is_mul_of(&ke->program[0], 0, 1)) return 0;
  if (!blas_op_is_reduce_sum(&ke->program[1], 0)) return 0;
  // Non-contig view inputs: bail (interpreter materializes them).
  for (u32 i = 0; i < 2; i++) {
    u32 tid = ke->input_tids[i];
    if (tid != 0 && tid < TENS_NEXT && !TENS[tid].view.contiguous) return 0;
  }
  float const *a = (float const *)CPU_BUFS[in_buf_ids[0]].data;
  float const *b = (float const *)CPU_BUFS[in_buf_ids[1]].data;
  float       *o = (float       *)CPU_BUFS[out_buf_id].data;
  o[0] = cblas_sdot((int)n0, a, 1, b, 1);
  return 1;
}

// Try GEMV.  W (M*K, in0) @ x (K, in1) -> {M}.  TMatVec materializes
// x as an EXPAND-view with the same numel as W (M*K) and broadcast
// strides; we look at the underlying TenDesc to recover the real K
// elements, then call cblas_sgemv on the contig backing buffer.
//
// op[0] = MUL(in0, in1)               numel = M*K
// op[1] = REDUCE_SUM(op[0])           numel = M, REDUCE inner = 1
//                                     (i.e. reducing the trailing axis).
static int blas_try_gemv(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->n_inputs != 2) return 0;
  if (ke->n_ops    != 2) return 0;
  if (ke->input_dtypes[0] != DT_F32 || ke->input_dtypes[1] != DT_F32) return 0;
  if (ke->program[0].dtype != DT_F32 || ke->program[1].dtype != DT_F32) return 0;
  if (!blas_op_is_mul_of(&ke->program[0], 0, 1)) return 0;
  if (!blas_op_is_reduce_sum(&ke->program[1], 0)) return 0;
  if (BLAS_REDUCE_INNER(ke->program[1].arg) != 1) return 0;
  u32 nMul = ke->program[0].numel;
  u32 nOut = ke->program[1].numel;
  if (nOut == 0 || nMul == 0 || nMul % nOut != 0) return 0;
  u32 M = nOut;
  u32 K = nMul / nOut;
  // Identify which input is W (contig M*K) and which is x (broadcast
  // EXPAND of K elements).  Use the underlying TenDesc.view.shape
  // to disambiguate -- W has shape {M, K} and is contiguous; x has
  // shape {1, K} or {K} with one of the strides being 0 in the
  // expanded view.
  // Identify W (M*K floats) vs x (K floats) via the underlying CpuBuf
  // size.  view.numel reflects the EXPANDED shape (M*K for both inputs
  // when x came in as an EXPAND-broadcast); the actual contig buffer
  // behind x has K floats and nbytes = 4*K.
  u32 widx = 0xFFFFFFFFu, xidx = 0xFFFFFFFFu;
  for (u32 i = 0; i < 2; i++) {
    u32 buf_id = in_buf_ids[i];
    if (buf_id == 0) return 0;
    u32 buf_floats = (u32)(CPU_BUFS[buf_id].nbytes / sizeof(float));
    if (buf_floats == M * K)       widx = i;
    else if (buf_floats == K)      xidx = i;
  }
  if (widx == 0xFFFFFFFFu || xidx == 0xFFFFFFFFu) return 0;
  float const *W = (float const *)CPU_BUFS[in_buf_ids[widx]].data;
  float const *x = (float const *)CPU_BUFS[in_buf_ids[xidx]].data;
  float       *o = (float       *)CPU_BUFS[out_buf_id].data;
  // CblasRowMajor: W is M-by-K stored row-major (lda = K); op = NoTrans.
  cblas_sgemv(CblasRowMajor, CblasNoTrans, (int)M, (int)K,
              1.0f, W, (int)K, x, 1, 0.0f, o, 1);
  return 1;
}

fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (blas_try_dot (ke, in_buf_ids, out_buf_id)) return 1;
  if (blas_try_gemv(ke, in_buf_ids, out_buf_id)) return 1;
  return 0;
}

#else  // !HAVE_BLAS

fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  (void)ke; (void)in_buf_ids; (void)out_buf_id;
  return 0;
}

#endif
