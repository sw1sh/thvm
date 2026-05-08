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

// Predicate: every input + every program op shares the same float
// dtype (f32 or f64).  Mixed dtypes bail; integer dtypes bail too.
//
// Slice 8 (cpu_blas_dispatch DAG migration): under default
// `THVM_PHASE_C7_FREE_PROGRAM=1` the program[] array is freed at
// materialize time, so this gate is consulted only on the legacy
// fallback path (cached_lift.store_root == 0, e.g. lift declined).
// The DAG path's GEMM dispatcher walks the lifted DAG via
// `uop_dag_classify_matmul_shape` and validates dtype uniformity at
// the BUFFER terms it encounters there.
static int blas_uniform_float(KernelEntry *ke, u32 *out_dtype) {
  if (ke->program == NULL || ke->n_ops == 0) return 0;
  u32 dt = ke->program[0].dtype;
  if (dt != DT_FP32 && dt != DT_FP64) return 0;
  for (u32 i = 0; i < ke->n_ops; i++)
    if (ke->program[i].dtype != dt) return 0;
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_dtypes[i] != dt) return 0;
  *out_dtype = dt;
  return 1;
}

// Try DOT.  Returns 1 on dispatch, 0 on no-match (caller fall-back).
//
// Slice 8: this gate consumes program[] directly and therefore only
// fires on the legacy fallback path (cached_lift.store_root == 0).
// Under default THVM_PHASE_C7_FREE_PROGRAM=1 the program[] is NULL
// after materialize, so the dot pattern reaches BLAS only via the
// dual-write knob (THVM_PHASE_C7_FREE_PROGRAM=0).  Migrating dot/gemv
// to read from the lifted UOp DAG is left as session-3 work.
static int blas_try_dot(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->program == NULL || ke->n_ops == 0) return 0;
  if (ke->n_inputs != 2) return 0;
  if (ke->n_ops    != 2) return 0;
  u32 dt;
  if (!blas_uniform_float(ke, &dt)) return 0;
  u32 n0 = ke->input_numels[0], n1 = ke->input_numels[1];
  if (n0 != n1 || n0 == 0) return 0;
  if (ke->program[0].numel != n0) return 0;
  if (ke->program[1].numel != 1)  return 0;
  if (!blas_op_is_mul_of(&ke->program[0], 0, 1)) return 0;
  if (!blas_op_is_reduce_sum(&ke->program[1], 0)) return 0;
  // Non-contig view inputs OR ShapeTracker chain inputs: bail
  // (interpreter pre-materializes them).
  for (u32 i = 0; i < 2; i++) {
    u32 tid = ke->input_tids[i];
    if (tid != 0 && tid < TENS_NEXT
        && (!TENS[tid].view.contiguous || TENS[tid].nviews > 0)) return 0;
  }
  if (dt == DT_FP32) {
    float const *a = (float const *)CPU_BUFS[in_buf_ids[0]].data;
    float const *b = (float const *)CPU_BUFS[in_buf_ids[1]].data;
    float       *o = (float       *)CPU_BUFS[out_buf_id].data;
    o[0] = cblas_sdot((int)n0, a, 1, b, 1);
  } else {
    double const *a = (double const *)CPU_BUFS[in_buf_ids[0]].data;
    double const *b = (double const *)CPU_BUFS[in_buf_ids[1]].data;
    double       *o = (double       *)CPU_BUFS[out_buf_id].data;
    o[0] = cblas_ddot((int)n0, a, 1, b, 1);
  }
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
  if (ke->program == NULL || ke->n_ops == 0) return 0;
  if (ke->n_inputs != 2) return 0;
  if (ke->n_ops    != 2) return 0;
  u32 dt;
  if (!blas_uniform_float(ke, &dt)) return 0;
  if (!blas_op_is_mul_of(&ke->program[0], 0, 1)) return 0;
  if (!blas_op_is_reduce_sum(&ke->program[1], 0)) return 0;
  if (BLAS_REDUCE_INNER(ke->program[1].arg) != 1) return 0;
  u32 nMul = ke->program[0].numel;
  u32 nOut = ke->program[1].numel;
  if (nOut == 0 || nMul == 0 || nMul % nOut != 0) return 0;
  u32 M = nOut;
  u32 K = nMul / nOut;
  // Identify W (M*K elements) vs x (K elements) by underlying CpuBuf
  // size.  view.numel reflects the EXPANDED shape (M*K for both
  // inputs when x came in as an EXPAND-broadcast); the actual contig
  // buffer behind x has K elements.
  u32 elem_bytes = (dt == DT_FP32) ? sizeof(float) : sizeof(double);
  u32 widx = 0xFFFFFFFFu, xidx = 0xFFFFFFFFu;
  for (u32 i = 0; i < 2; i++) {
    u32 buf_id = in_buf_ids[i];
    if (buf_id == 0) return 0;
    u32 buf_elems = (u32)(CPU_BUFS[buf_id].nbytes / elem_bytes);
    if (buf_elems == M * K)       widx = i;
    else if (buf_elems == K)      xidx = i;
  }
  if (widx == 0xFFFFFFFFu || xidx == 0xFFFFFFFFu) return 0;
  if (dt == DT_FP32) {
    float const *W = (float const *)CPU_BUFS[in_buf_ids[widx]].data;
    float const *x = (float const *)CPU_BUFS[in_buf_ids[xidx]].data;
    float       *o = (float       *)CPU_BUFS[out_buf_id].data;
    cblas_sgemv(CblasRowMajor, CblasNoTrans, (int)M, (int)K,
                1.0f, W, (int)K, x, 1, 0.0f, o, 1);
  } else {
    double const *W = (double const *)CPU_BUFS[in_buf_ids[widx]].data;
    double const *x = (double const *)CPU_BUFS[in_buf_ids[xidx]].data;
    double       *o = (double       *)CPU_BUFS[out_buf_id].data;
    cblas_dgemv(CblasRowMajor, CblasNoTrans, (int)M, (int)K,
                1.0, W, (int)K, x, 1, 0.0, o, 1);
  }
  return 1;
}

// Try GEMM.  A:{M,K} @ B:{K,N} -> {M,N}.  TMatMul materializes the
// pair as a common-shape EXPAND-broadcast so MUL becomes elementwise:
//
//   op[0] = MUL(in0, in1)               numel = M*K*N
//   op[1] = REDUCE_SUM(op[0])           numel = M*N, REDUCE inner = N
//                                       (i.e. reducing the middle axis
//                                        of the {M,K,N} layout).
//
// Disambiguates from GEMV by inner: GEMV has inner = 1, GEMM has
// inner = N > 1 + a non-trivial K (nMul / nOut > 1).  A buffer-size
// check distinguishes A (M*K floats) from B (K*N floats).
// Slice 8 instrumentation: count cblas dispatches landed via the
// DAG-side classifier vs the legacy program[] path.  Read by
// THVM_BLAS_DISPATCH_TRACE=1 + queried by tests.  Reset by thvm_init.
static u64 BLAS_GEMM_DISPATCH_DAG    = 0;
static u64 BLAS_GEMM_DISPATCH_LEGACY = 0;

fn u64 cpu_blas_gemm_dispatch_dag_count(void) {
  return BLAS_GEMM_DISPATCH_DAG;
}
fn u64 cpu_blas_gemm_dispatch_legacy_count(void) {
  return BLAS_GEMM_DISPATCH_LEGACY;
}
fn void cpu_blas_gemm_dispatch_counters_reset(void) {
  BLAS_GEMM_DISPATCH_DAG    = 0;
  BLAS_GEMM_DISPATCH_LEGACY = 0;
}

// Issue cblas_{s,d}gemm given a fully-populated shape (M, N, K, ldA,
// ldB, transA, transB, a_input, b_input).  Both the DAG path and the
// legacy program[] path funnel through this so the actual dispatch
// site stays single-source.
static void blas_emit_gemm(u32 dt, u32 M, u32 N, u32 K,
                           u32 ldA, u32 ldB,
                           u32 trans_a, u32 trans_b,
                           u32 a_buf, u32 b_buf, u32 out_buf_id) {
  enum CBLAS_TRANSPOSE transA = trans_a ? CblasTrans : CblasNoTrans;
  enum CBLAS_TRANSPOSE transB = trans_b ? CblasTrans : CblasNoTrans;
  if (dt == DT_FP32) {
    float const *A = (float const *)CPU_BUFS[a_buf].data;
    float const *B = (float const *)CPU_BUFS[b_buf].data;
    float       *C = (float       *)CPU_BUFS[out_buf_id].data;
    cblas_sgemm(CblasRowMajor, transA, transB,
                (int)M, (int)N, (int)K,
                1.0f, A, (int)ldA, B, (int)ldB,
                0.0f, C, (int)N);
  } else {
    double const *A = (double const *)CPU_BUFS[a_buf].data;
    double const *B = (double const *)CPU_BUFS[b_buf].data;
    double       *C = (double       *)CPU_BUFS[out_buf_id].data;
    cblas_dgemm(CblasRowMajor, transA, transB,
                (int)M, (int)N, (int)K,
                1.0, A, (int)ldA, B, (int)ldB,
                0.0, C, (int)N);
  }
}

// DAG-side GEMM dispatcher (Slice 8).  Reads M/N/K + slot mapping +
// ldA/ldB + transpose flags from the lifted UOp DAG via
// `uop_dag_classify_matmul_shape`; survives `program == NULL` under
// default THVM_PHASE_C7_FREE_PROGRAM=1.  Returns 1 on dispatch, 0 on
// no-match (caller falls through to legacy or skips BLAS).
//
// Bisection knob `THVM_BLAS_DAG_DISABLE=1`: opt out of the DAG path
// (forces the legacy program[] reader, or no BLAS dispatch at all
// when program[] is freed).  Used for A/B-testing perf regressions
// + the bench/synth/bench_blas_dag.c micro-benchmark.
static int blas_try_gemm_dag(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  static int disable_inited = 0;
  static int disable_on     = 0;
  if (!disable_inited) {
    char const *e = getenv("THVM_BLAS_DAG_DISABLE");
    disable_on    = (e != NULL && e[0] == '1');
    disable_inited = 1;
  }
  if (disable_on) return 0;
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagGemmShape gemm;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, &gemm)) {
    return 0;
  }
  if (gemm.N <= 1) return 0;
  // Buffer-size sanity: the DAG path skips the storage-numels gate
  // (legacy used it to disambiguate A vs B by element count); we
  // already pin a_input / b_input from BUFFER.instance, so storage
  // sanity reduces to "buffer ids exist + at least M*K / K*N
  // elements addressable".  Bail if either is 0.
  u32 a_buf = in_buf_ids[gemm.a_input];
  u32 b_buf = in_buf_ids[gemm.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
  u32 elem_bytes = (gemm.dtype == DT_FP32) ? sizeof(float) : sizeof(double);
  u32 a_elems = (u32)(CPU_BUFS[a_buf].nbytes / elem_bytes);
  u32 b_elems = (u32)(CPU_BUFS[b_buf].nbytes / elem_bytes);
  if (a_elems < gemm.M * gemm.K) return 0;
  if (b_elems < gemm.K * gemm.N) return 0;
  blas_emit_gemm(gemm.dtype, gemm.M, gemm.N, gemm.K, gemm.ldA, gemm.ldB,
                 gemm.flags & 1u, gemm.flags & 2u,
                 a_buf, b_buf, out_buf_id);
  BLAS_GEMM_DISPATCH_DAG++;
  return 1;
}

// Legacy program[]-reading GEMM dispatcher.  Survives only when
// `program[]` is populated (THVM_PHASE_C7_FREE_PROGRAM=0 dual-write
// path, or kernels that didn't lift cleanly).  Kept verbatim from the
// pre-Slice-8 implementation so a regression in the DAG path leaves
// behaviour bit-equal.
static int blas_try_gemm_legacy(KernelEntry *ke, u32 *in_buf_ids,
                                u32 out_buf_id) {
  if (ke->program == NULL || ke->n_ops == 0) return 0;
  if (ke->n_inputs != 2) return 0;
  if (ke->n_ops    != 2) return 0;
  u32 dt;
  if (!blas_uniform_float(ke, &dt)) return 0;
  u32 elem_bytes = (dt == DT_FP32) ? sizeof(float) : sizeof(double);
  u32 b0 = in_buf_ids[0], b1 = in_buf_ids[1];
  if (b0 == 0 || b1 == 0) return 0;
  u32 storage_numels[2] = {
    (u32)(CPU_BUFS[b0].nbytes / elem_bytes),
    (u32)(CPU_BUFS[b1].nbytes / elem_bytes),
  };
  TileGemmInfo gemm;
  if (!tile_analyze_gemm(ke, storage_numels, &gemm)) return 0;
  if (gemm.dtype != dt || gemm.N <= 1) return 0;
  blas_emit_gemm(dt, gemm.M, gemm.N, gemm.K, gemm.ldA, gemm.ldB,
                 gemm.flags & 1u, gemm.flags & 2u,
                 in_buf_ids[gemm.a_input], in_buf_ids[gemm.b_input],
                 out_buf_id);
  BLAS_GEMM_DISPATCH_LEGACY++;
  return 1;
}

// Slice 8: prefer the DAG-side classifier; fall back to the legacy
// program[] reader only when the lift declined (cached_lift.store_root
// == 0) or the DAG classifier didn't recognise the shape.  Under
// default `THVM_PHASE_C7_FREE_PROGRAM=1` the legacy path is dormant
// (program[] is NULL), but the dual-write knob (=0) still needs it
// for A/B testing perf regressions.
static int blas_try_gemm(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (blas_try_gemm_dag(ke, in_buf_ids, out_buf_id)) return 1;
  return blas_try_gemm_legacy(ke, in_buf_ids, out_buf_id);
}

// Returns the specific KDispatchKind that fired (BLAS_DOT / BLAS_GEMV
// / BLAS_GEMM) so the profiler can record the route, or 0 on no-match
// (caller falls through to JIT / interpreter).
fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // BLAS routines write a single output (sgemm/sgemv/sdot all
  // expect one C buffer).  Multi-output kernels must skip BLAS
  // until the dispatch wiring lands.
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  if (blas_try_dot (ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_DOT;
  if (blas_try_gemv(ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_GEMV;
  if (blas_try_gemm(ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_GEMM;
  return 0;
}

#else  // !HAVE_BLAS

fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  (void)ke; (void)in_buf_ids; (void)out_buf_id;
  return 0;
}

fn u64  cpu_blas_gemm_dispatch_dag_count   (void) { return 0; }
fn u64  cpu_blas_gemm_dispatch_legacy_count(void) { return 0; }
fn void cpu_blas_gemm_dispatch_counters_reset(void) {}

#endif
