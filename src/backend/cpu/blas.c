// backend/cpu/blas.c - pattern-recognise matmul / matvec / dot
// kernels from the lifted UOp DAG and dispatch them to Apple
// Accelerate (cblas_*).  10-100x speedup on the hot ML workload
// (matmul) -- a kernel that the JIT path doesn't attempt (it bails
// at REDUCE).
//
// Every dispatcher reads M/N/K + dtype + slot mapping from
// ke->cached_lift.store_root via uop_dag_classify_matmul_shape /
// uop_dag_classify_dot_shape / uop_dag_classify_gemv_shape.
// Rangeify produces the canonical MUL+REDUCE+OPT_TC scalar_uops
// pattern for every matmul-shaped kernel, which kernel_lift_to_uop
// turns into the UOp DAG these dispatchers inspect.

#ifdef __APPLE__
#define ACCELERATE_NEW_LAPACK
#include <Accelerate/Accelerate.h>
#define HAVE_BLAS 1
#else
#define HAVE_BLAS 0
#endif

#if HAVE_BLAS

// Count cblas dispatches landed via the DAG-side classifier.  Read
// by THVM_BLAS_DISPATCH_TRACE=1 and queried by tests.  Reset by
// thvm_init.
static u64 BLAS_GEMM_DISPATCH_DAG = 0;
static u64 BLAS_DOT_DISPATCH_DAG  = 0;
static u64 BLAS_GEMV_DISPATCH_DAG = 0;

// Issue cblas_{s,d}dot.  Single dispatch site (the DAG GEMM/DOT/GEMV
// dispatchers all funnel through it).
static void blas_emit_dot(u32 dt, u32 K,
                          u32 a_buf, u32 b_buf, u32 out_buf_id) {
  if (dt == DT_FP32) {
    float const *a = (float const *)CPU_BUFS[a_buf].data;
    float const *b = (float const *)CPU_BUFS[b_buf].data;
    float       *o = (float       *)CPU_BUFS[out_buf_id].data;
    o[0] = cblas_sdot((int)K, a, 1, b, 1);
  } else {
    double const *a = (double const *)CPU_BUFS[a_buf].data;
    double const *b = (double const *)CPU_BUFS[b_buf].data;
    double       *o = (double       *)CPU_BUFS[out_buf_id].data;
    o[0] = cblas_ddot((int)K, a, 1, b, 1);
  }
}

// Issue cblas_{s,d}gemv given a fully-populated shape (M, K, ldW,
// transW, w_buf, x_buf, out_buf).  Funnel for the DAG + legacy paths.
static void blas_emit_gemv(u32 dt, u32 M, u32 K, u32 ldW, u32 transW,
                           u32 w_buf, u32 x_buf, u32 out_buf_id) {
  enum CBLAS_TRANSPOSE tW = transW ? CblasTrans : CblasNoTrans;
  if (dt == DT_FP32) {
    float const *W = (float const *)CPU_BUFS[w_buf].data;
    float const *x = (float const *)CPU_BUFS[x_buf].data;
    float       *o = (float       *)CPU_BUFS[out_buf_id].data;
    cblas_sgemv(CblasRowMajor, tW, (int)M, (int)K,
                1.0f, W, (int)ldW, x, 1, 0.0f, o, 1);
  } else {
    double const *W = (double const *)CPU_BUFS[w_buf].data;
    double const *x = (double const *)CPU_BUFS[x_buf].data;
    double       *o = (double       *)CPU_BUFS[out_buf_id].data;
    cblas_dgemv(CblasRowMajor, tW, (int)M, (int)K,
                1.0, W, (int)ldW, x, 1, 0.0, o, 1);
  }
}

// DAG-side DOT dispatcher.  Reads K + slot mapping from the lifted
// UOp DAG via `uop_dag_classify_dot_shape`.  Returns 1 on dispatch, 0
// on no-match.
static int blas_try_dot(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagDotShape dot;
  if (!uop_dag_classify_dot_shape(ke->cached_lift.store_root, ke, &dot)) {
    return 0;
  }
  u32 a_buf = in_buf_ids[dot.a_input];
  u32 b_buf = in_buf_ids[dot.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
  u32 elem_bytes = (dot.dtype == DT_FP32) ? sizeof(float) : sizeof(double);
  u32 a_elems = (u32)(CPU_BUFS[a_buf].nbytes / elem_bytes);
  u32 b_elems = (u32)(CPU_BUFS[b_buf].nbytes / elem_bytes);
  if (a_elems < dot.K || b_elems < dot.K) return 0;
  blas_emit_dot(dot.dtype, dot.K, a_buf, b_buf, out_buf_id);
  BLAS_DOT_DISPATCH_DAG++;
  return 1;
}

// DAG-side GEMV dispatcher.  Reads M/K + slot mapping + ldW + transpose
// flag from the lifted UOp DAG via `uop_dag_classify_gemv_shape`.
// Returns 1 on dispatch, 0 on no-match.
static int blas_try_gemv(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagGemvShape gemv;
  if (!uop_dag_classify_gemv_shape(ke->cached_lift.store_root, ke, &gemv)) {
    return 0;
  }
  u32 w_buf = in_buf_ids[gemv.w_input];
  u32 x_buf = in_buf_ids[gemv.x_input];
  if (w_buf == 0 || x_buf == 0) return 0;
  u32 elem_bytes = (gemv.dtype == DT_FP32) ? sizeof(float) : sizeof(double);
  u32 w_elems = (u32)(CPU_BUFS[w_buf].nbytes / elem_bytes);
  u32 x_elems = (u32)(CPU_BUFS[x_buf].nbytes / elem_bytes);
  if (w_elems < gemv.M * gemv.K) return 0;
  if (x_elems < gemv.K) return 0;
  blas_emit_gemv(gemv.dtype, gemv.M, gemv.K, gemv.ldW, gemv.flags & 1u,
                 w_buf, x_buf, out_buf_id);
  BLAS_GEMV_DISPATCH_DAG++;
  return 1;
}

// Counter accessors -- see top-of-file definition block.
fn u64 cpu_blas_gemm_dispatch_dag_count(void) {
  return BLAS_GEMM_DISPATCH_DAG;
}
fn u64 cpu_blas_dot_dispatch_dag_count(void) {
  return BLAS_DOT_DISPATCH_DAG;
}
fn u64 cpu_blas_gemv_dispatch_dag_count(void) {
  return BLAS_GEMV_DISPATCH_DAG;
}
fn void cpu_blas_gemm_dispatch_counters_reset(void) {
  BLAS_GEMM_DISPATCH_DAG = 0;
  BLAS_DOT_DISPATCH_DAG  = 0;
  BLAS_GEMV_DISPATCH_DAG = 0;
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
//
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

// DAG-side GEMM dispatcher.  Reads M/N/K + slot mapping + ldA/ldB +
// transpose flags from the lifted UOp DAG via
// `uop_dag_classify_matmul_shape`.  Returns 1 on dispatch, 0 on
// no-match.
static int blas_try_gemm(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagGemmShape gemm;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, &gemm)) {
    return 0;
  }
  if (gemm.N <= 1) return 0;
  // Buffer-size sanity: a_input / b_input are pinned by BUFFER.instance
  // so storage sanity reduces to "buffer ids exist + at least M*K /
  // K*N elements addressable".  Bail if either is 0.
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

// Returns the specific KDispatchKind that fired (BLAS_DOT / BLAS_GEMV
// / BLAS_GEMM) so the profiler can record the route, or 0 on no-match
// (caller falls through to JIT / interpreter).
fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // BLAS routines write a single output (sgemm/sgemv/sdot all
  // expect one C buffer).  Multi-output kernels must skip BLAS
  // until the dispatch wiring lands.
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  // Bisection knob: THVM_CPU_BLAS_DISABLE=1 forces every kernel past
  // the BLAS try-ladder so the walker / JIT path runs instead.  Used
  // for isolating BLAS-classifier bugs (mis-mapped A/B operand order,
  // bad transA/transB recovery) from kernel-body bugs.
  static int disable_known = 0, disable_on = 0;
  if (!disable_known) {
    char const *e = getenv("THVM_CPU_BLAS_DISABLE");
    disable_on = (e != NULL && e[0] == '1');
    disable_known = 1;
  }
  if (disable_on) return 0;
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
fn u64  cpu_blas_dot_dispatch_dag_count    (void) { return 0; }
fn u64  cpu_blas_gemv_dispatch_dag_count   (void) { return 0; }
fn void cpu_blas_gemm_dispatch_counters_reset(void) {}

#endif
