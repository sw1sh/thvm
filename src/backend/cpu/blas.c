// backend/cpu/blas.c - pattern-recognise matmul / matvec / dot
// kernels from the lifted UOp DAG and dispatch them to Apple
// Accelerate (cblas_*).  10-100x speedup on the hot ML workload
// (matmul) -- a kernel that the JIT path doesn't attempt (it bails
// at REDUCE).
//
// Every dispatcher reads M/N/K + dtype + slot mapping from
// ke->cached_lift.store_root via uop_dag_classify_matmul_shape /
// uop_dag_classify_dot_shape / uop_dag_classify_gemv_shape.
// The unified rangeify pass produces the canonical MUL+REDUCE+OPT_TC
// UOp DAG pattern for every matmul-shaped kernel, which
// kernel_lift_to_uop packages as the per-kernel root these dispatchers
// inspect.

#ifdef __APPLE__
#define ACCELERATE_NEW_LAPACK
#include <Accelerate/Accelerate.h>
#define HAVE_BLAS 1
#else
#define HAVE_BLAS 0
#endif

#if HAVE_BLAS

// Count cblas dispatches landed via the DAG-side classifier.
// Queried by tests through the blas_*_dispatch_dag accessors below.
// Reset by thvm_init.
static u64 BLAS_GEMM_DISPATCH_DAG = 0;
static u64 BLAS_DOT_DISPATCH_DAG  = 0;
static u64 BLAS_GEMV_DISPATCH_DAG = 0;
static u64 BLAS_CONTRACTION_DISPATCH_DAG = 0;

// Cached "env flag set to '1'" check.  Each dispatcher had its own
// `static int known; static int on;` block; this collapses them.  The
// cache is keyed by the static variable's address (one cache slot per
// call site -- pass a unique &on_flag_var as `slot`).  Returns 1 iff
// getenv(name)[0] == '1' on first call; subsequent calls return the
// cached value without touching getenv again.
static int env_flag_on(int *slot, const char *name) {
  // -1 = unread, 0 = off, 1 = on.  Single-threaded init is safe for
  // the dispatch path; the cache hides the getenv after first call.
  if (*slot == -1) {
    char const *e = getenv(name);
    *slot = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return *slot;
}

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
  // cblas_*gemv (CblasRowMajor) requires lda >= max(1, N) where the
  // matrix is passed as (M, N) == (M, K) here -- i.e. ldW >= K.  A
  // misclassified shape with ldW < K trips an "invalid parameter 7"
  // abort; decline so the (correct) walker/JIT path handles it instead.
  if (gemv.ldW < gemv.K || gemv.M == 0 || gemv.K == 0) return 0;
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
fn u64 cpu_blas_contraction_dispatch_dag_count(void) {
  return BLAS_CONTRACTION_DISPATCH_DAG;
}
fn void cpu_blas_gemm_dispatch_counters_reset(void) {
  BLAS_GEMM_DISPATCH_DAG = 0;
  BLAS_DOT_DISPATCH_DAG  = 0;
  BLAS_GEMV_DISPATCH_DAG = 0;
  BLAS_CONTRACTION_DISPATCH_DAG = 0;
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
                           u32 ldA, u32 ldB, u32 ldC,
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
                0.0f, C, (int)ldC);
  } else {
    double const *A = (double const *)CPU_BUFS[a_buf].data;
    double const *B = (double const *)CPU_BUFS[b_buf].data;
    double       *C = (double       *)CPU_BUFS[out_buf_id].data;
    cblas_dgemm(CblasRowMajor, transA, transB,
                (int)M, (int)N, (int)K,
                1.0, A, (int)ldA, B, (int)ldB,
                0.0, C, (int)ldC);
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
  // Resolve symbolic (kvar) shape dims to their per-dispatch bound value.
  // classify returns the output leading dim M kvar-packed for a symbolic-M
  // matmul ({S, dim} = onehot . tokT, S the seq kvar); the bind is set before
  // fire, so GEMM runs the actual M (buffers are sized at the kvar upper
  // bound, GEMM writes the first M rows).  Identity for literal dims.
  gemm.M   = kvar_extent_runtime(gemm.M);
  gemm.N   = kvar_extent_runtime(gemm.N);
  gemm.K   = kvar_extent_runtime(gemm.K);
  gemm.ldA = kvar_extent_runtime(gemm.ldA);
  gemm.ldB = kvar_extent_runtime(gemm.ldB);
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
  // ldC is the OUTPUT buffer's logical row stride, NOT N: a symbolic-seq
  // matmul output ({S, *}) is allocated at the kvar STATIC upper bound, so its
  // row stride is hi >= N (the runtime extent).  Passing N would pack the
  // cblas rows at the wrong pitch into a buffer every consumer indexes at the
  // static stride.  Identity for literal matmuls (strides[0] == N).
  u32 ldC = (u32) TENS[ke->output_tid].view.strides[0];
  if (ldC == 0) ldC = gemm.N;
  blas_emit_gemm(gemm.dtype, gemm.M, gemm.N, gemm.K, gemm.ldA, gemm.ldB, ldC,
                 gemm.flags & 1u, gemm.flags & 2u,
                 a_buf, b_buf, out_buf_id);
  BLAS_GEMM_DISPATCH_DAG++;
  return 1;
}

// DAG-side generalized-contraction dispatcher.  Recognises a kernel
// shape `out[M_axes, N_axes] = sum_{K} A[K, N_axes] * B[M_outer, K,
// M_inner]` and routes it to a batched cblas_sgemm over M_outer (or
// a single sgemm when n_M == 1).
//
// In contraction-space: the W operand is the (K, N) factor (= a_input)
// and the G operand is the (M_outer, K, M_inner) factor (= b_input).
// Per-batch BLAS arguments:
//   sub_A_blas = G_slice (K, inner_M)  -- transposed in BLAS to (inner_M, K)
//   sub_B_blas = W       (K, N)        -- as-is
//   sub_C_blas = out_slice (inner_M, N)
//   cblas_sgemm(RowMajor, Trans, NoTrans,
//               M=inner_M, N=N, K=K,
//               1.0, sub_A_blas, ldB,
//                    sub_B_blas, ldA,
//               0.0, sub_C_blas, ldC)
//
// The conv-backward x-grad kernel `out[a0,a1,a2,a3,a4,a5] =
// sum_{a6} W[a6,a3,a4,a5] * G[a0,a6,a1,a2]` is the canonical instance:
// inner_M = (OH*OW), N = (C_in*KH*KW), K = C_out, batch = B.
static int blas_try_contraction(KernelEntry *ke, u32 *in_buf_ids,
                                u32 out_buf_id) {
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagContractionShape c;
  if (!uop_dag_classify_contraction_shape(ke->cached_lift.store_root, ke, &c)) {
    return 0;
  }
  static int trace_slot = -1;
  if (env_flag_on(&trace_slot, "THVM_BLAS_CONTRACTION_TRACE")) {
    fprintf(stderr, "blas_try_contraction: K=%u N=%u inner_M=%u batch=%u "
            "ldA=%u ldB=%u ldC=%u bsa=%u bsb=%u bsc=%u\n",
            c.K, c.N, c.inner_M, c.batch, c.ldA, c.ldB, c.ldC,
            c.batch_stride_a, c.batch_stride_b, c.batch_stride_c);
  }
  // Skip degenerate shapes a simpler classifier handles better.
  if (c.K < 2) return 0;
  if (c.inner_M < 2 && c.N < 2) return 0;
  u32 a_buf = in_buf_ids[c.a_input];
  u32 b_buf = in_buf_ids[c.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
  if (c.dtype != DT_FP32) return 0;
  u32 esz = sizeof(float);
  u64 a_elems = (u64)(CPU_BUFS[a_buf].nbytes / esz);
  u64 b_elems = (u64)(CPU_BUFS[b_buf].nbytes / esz);
  u64 o_elems = (u64)(CPU_BUFS[out_buf_id].nbytes / esz);
  u64 a_need = (u64)c.K * c.N;
  u64 b_need = (u64)c.batch * c.K * c.inner_M;
  u64 o_need = (u64)c.batch * c.inner_M * c.N;
  if (a_elems < a_need) return 0;
  if (b_elems < b_need) return 0;
  if (o_elems < o_need) return 0;

  float const *W_base   = (float const *)CPU_BUFS[a_buf].data;
  float const *G_base   = (float const *)CPU_BUFS[b_buf].data;
  float       *OUT_base = (float       *)CPU_BUFS[out_buf_id].data;
  for (u32 bi = 0; bi < c.batch; bi++) {
    float const *G_slice = G_base + (u64)bi * c.batch_stride_b;
    float       *C_slice = OUT_base + (u64)bi * c.batch_stride_c;
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                (int)c.inner_M, (int)c.N, (int)c.K,
                1.0f, G_slice, (int)c.ldB,
                      W_base,  (int)c.ldA,
                0.0f, C_slice, (int)c.ldC);
  }
  BLAS_CONTRACTION_DISPATCH_DAG++;
  return 1;
}

// im2col patch-gather helper.  Materialises a row-major
// patches[N_total, K_pixels] block from a single batch slice of X.
//
//   patches[((cin*KH + kh)*KW + kw) * K_pixels + oh*OW + ow]
//     = X_b[cin*X_Cin_str + (oh+kh)*X_W + (ow+kw)]
//
// The innermost OW loop is stride-1 in both source and destination so we
// fold it into a single memcpy of length OW.  Used by both the BWD
// weight-grad dispatcher (per K_outer B iter) and the FWD conv dispatcher
// (per N_outer B iter) -- the gather shape is the same.
static void im2col_gather_patches(float *patches,
                                  float const *X_b,
                                  u32 N_patch, u32 KH, u32 KW,
                                  u32 OH, u32 OW,
                                  u32 X_W, u32 X_Cin_str) {
  u64 K_pixels = (u64)OH * OW;
  for (u32 cin = 0; cin < N_patch; cin++) {
    for (u32 kh = 0; kh < KH; kh++) {
      for (u32 kw = 0; kw < KW; kw++) {
        float *row = patches + ((u64)((cin * KH + kh) * KW + kw)) * K_pixels;
        for (u32 oh = 0; oh < OH; oh++) {
          float const *src = X_b + (u64)cin * X_Cin_str
                                 + (u64)(oh + kh) * X_W + kw;
          memcpy(row + (u64)oh * OW, src, (size_t)OW * sizeof(float));
        }
      }
    }
  }
}

// C7.2 dispatcher: im2col conv as a batched GEMM.  Handles two duals:
//
//   BWD (forward_conv=0): conv-backward weight-grad
//     dW[Cout, Cin, KH, KW] = sum_{B, OH, OW}
//                              dY[B, Cout, OH, OW] * X[B, Cin, OH+KH, OW+KW]
//     Per-batch:
//       patches[Cin*KH*KW, OH*OW] from X[b];   sub_dY = dY[b] (Cout, OH*OW)
//       cblas_sgemm(NoTrans, Trans, M=Cout, N=Cin*KH*KW, K=OH*OW,
//                   1.0, sub_dY, ldB=OH*OW, patches, ldA=OH*OW,
//                   beta=(b==0?0:1), dW, ldC=Cin*KH*KW)
//
//   FWD (forward_conv=1): conv forward
//     out[B, Cout, OH, OW] = sum_{Cin, KH, KW}
//                              X[B, Cin, OH+KH, OW+KW] * W[Cout, Cin, KH, KW]
//     Per-batch:
//       patches[Cin*KH*KW, OH*OW] from X[b];   W is (Cout, Cin*KH*KW)
//       cblas_sgemm(NoTrans, NoTrans, M=Cout, N=OH*OW, K=Cin*KH*KW,
//                   1.0, W, ldB=Cin*KH*KW, patches, ldA=OH*OW,
//                   beta=0, out[b], ldC=OH*OW)
//
// Per dispatch one scratch (N_total * K_pixels) float block is allocated
// once and reused across the B loop.  Reference: tinygrad's `_pool`
// lowering (mixin/movement.py:569) expresses both directions as the same
// im2col ShapeTracker view.
static int blas_try_im2col_contraction(KernelEntry *ke, u32 *in_buf_ids,
                                       u32 out_buf_id) {
  if (ke->cached_lift.store_root == 0) return 0;
  UopDagIm2colShape c;
  if (!uop_dag_classify_im2col_contraction(ke->cached_lift.store_root, ke, &c)) {
    return 0;
  }
  static int trace_slot = -1;
  if (env_flag_on(&trace_slot, "THVM_BLAS_CONTRACTION_TRACE")) {
    // K_row/K_col now hold OH/OW (pixel extents) in both directions; the
    // KH/KW kernel extents live in patch_extent[].
    fprintf(stderr, "blas_try_im2col: dir=%s Cout=%u OH=%u OW=%u KH=%u KW=%u "
            "Cin=%u Bouter=%u X[H,W]=[%u,%u]\n",
            c.forward_conv ? "fwd" : "bwd", c.M, c.K_row, c.K_col,
            c.patch_extent[0], c.patch_extent[1], c.N_patchless,
            c.N_outer_extent, c.X_H, c.X_W);
  }
  if (c.dtype != DT_FP32) return 0;
  // Sanity bounds (defensive).
  if (c.M < 2 || (u64)c.K_row * c.K_col < 4) return 0;
  u64 K_total  = (u64)c.K_row * c.K_col;            // OH*OW (the pixel block)
  u64 N_total  = (u64)c.N_patchless * c.KH_total;   // Cin*KH*KW (the channel block)
  if (N_total < 2) return 0;
  if (c.N_outer_extent < 1) return 0;
  u32 a_buf = in_buf_ids[c.a_input];
  u32 b_buf = in_buf_ids[c.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
  u32 esz = sizeof(float);
  u64 a_elems = (u64)(CPU_BUFS[a_buf].nbytes / esz);
  u64 b_elems = (u64)(CPU_BUFS[b_buf].nbytes / esz);
  u64 o_elems = (u64)(CPU_BUFS[out_buf_id].nbytes / esz);
  u64 a_need = (u64)c.N_outer_extent * c.X_Cin_stride;
  u64 b_need;
  u64 o_need;
  if (c.forward_conv) {
    b_need = (u64)c.M * N_total;                          // W is (Cout, Cin*KH*KW)
    o_need = (u64)c.N_outer_extent * c.M * K_total;       // out is (B, Cout, OH*OW)
  } else {
    b_need = (u64)c.N_outer_extent * c.M * K_total;       // dY is (B, Cout, OH*OW)
    o_need = (u64)c.M * N_total;                          // dW is (Cout, Cin*KH*KW)
  }
  if (a_elems < a_need || b_elems < b_need || o_elems < o_need) return 0;

  // Scratch patches buffer.  Allocated once per dispatch, reused across batches.
  u64 scratch_n = N_total * K_total;
  if (scratch_n == 0 || scratch_n > (u64)1 << 28) return 0;     // <1 GiB cap
  float *patches = (float *)malloc((size_t)(scratch_n * sizeof(float)));
  if (patches == NULL) return 0;

  float const *X_base   = (float const *)CPU_BUFS[a_buf].data;
  float const *B_base   = (float const *)CPU_BUFS[b_buf].data;
  float       *out_base = (float       *)CPU_BUFS[out_buf_id].data;

  u32 KH        = c.patch_extent[0];
  u32 KW        = c.patch_extent[1];
  u32 OH        = c.K_row;
  u32 OW        = c.K_col;
  u32 X_W       = c.X_W;
  u32 X_Cin_str = c.X_Cin_stride;
  u32 X_outer   = c.X_outer_stride;
  u32 N_patch   = c.N_patchless;        // Cin

  for (u32 b = 0; b < c.N_outer_extent; b++) {
    float const *X_b = X_base + (u64)b * X_outer;
    im2col_gather_patches(patches, X_b, N_patch, KH, KW, OH, OW, X_W, X_Cin_str);
    // The gather wrote patches in (N_total=Cin*KH*KW rows, K_pixels=OH*OW cols)
    // row-major layout.  Both branches use that same buffer; they only differ
    // in (a) which axis of patches is the sgemm-K axis vs the sgemm-N axis,
    // and (b) where the output is written / whether to accumulate.
    if (c.forward_conv) {
      // FWD per-batch: out[b] = W @ patches.  sgemm-K = Cin*KH*KW (channels),
      // sgemm-N = OH*OW (pixels).  patches is already (K, N)-shaped, so
      // NoTrans on both.  beta=0 since each batch writes a distinct out slice.
      float *out_b = out_base + (u64)b * c.N_outer_out_stride;
      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                  (int)c.M, (int)K_total, (int)N_total,
                  1.0f, B_base,  (int)c.Y_M_stride,     // W,       ldA = N_total
                        patches, (int)K_total,          // patches, ldB = K_total
                  0.0f, out_b,   (int)c.out_M_stride);  // out[b],  ldC = K_total
    } else {
      // BWD per-batch: dW += dY[b] @ patches^T.  sgemm-K = OH*OW (pixels),
      // sgemm-N = Cin*KH*KW (channels).  patches is (N, K)-shaped for this
      // direction so use TransB.  beta=1 after the first batch to accumulate
      // dW across the batch dimension.
      float const *dY_b = B_base + (u64)b * c.Y_outer_stride;
      float beta = (b == 0) ? 0.0f : 1.0f;
      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                  (int)c.M, (int)N_total, (int)K_total,
                  1.0f, dY_b,    (int)c.Y_M_stride,     // dY[b],   ldA = K_total
                        patches, (int)K_total,          // patches, ldB = K_total
                  beta, out_base, (int)c.out_M_stride); // dW,      ldC = N_total
    }
  }
  free(patches);
  BLAS_CONTRACTION_DISPATCH_DAG++;
  return 1;
}

// Returns the specific KDispatchKind that fired (BLAS_DOT / BLAS_GEMV
// / BLAS_GEMM) so the profiler can record the route, or 0 on no-match
// (caller falls through to JIT / interpreter).
fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // Bisection knob: THVM_CPU_BLAS_DISABLE=1 forces every kernel past
  // the BLAS try-ladder so the walker / JIT path runs instead.  Used
  // for isolating BLAS-classifier bugs (mis-mapped A/B operand order,
  // bad transA/transB recovery) from kernel-body bugs.
  static int disable_slot = -1;
  if (env_flag_on(&disable_slot, "THVM_CPU_BLAS_DISABLE")) return 0;
  if (blas_try_dot (ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_DOT;
  if (blas_try_gemv(ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_GEMV;
  if (blas_try_gemm(ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_GEMM;
  // Generalized contraction (compound M/N axes; conv-backward x-grad
  // is the canonical instance).  Routed as KDISPATCH_BLAS_GEMM so the
  // profiler aggregates it with the simple-matmul route.
  if (blas_try_contraction(ke, in_buf_ids, out_buf_id))
    return KDISPATCH_BLAS_GEMM;
  // C7.2 im2col conv-bwd weight-grad: per-batch im2col gather + sgemm.
  // Only matches the multi-K REDUCE shape produced by C7.3 chain-fuse,
  // so it's a no-op until reduce.c's uop_reduce_multi activates the
  // REDUCE-of-REDUCE collapse.
  if (blas_try_im2col_contraction(ke, in_buf_ids, out_buf_id))
    return KDISPATCH_BLAS_GEMM;
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
fn u64  cpu_blas_contraction_dispatch_dag_count (void) { return 0; }
fn void cpu_blas_gemm_dispatch_counters_reset(void) {}

#endif
