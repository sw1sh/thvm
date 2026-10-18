// backend/cpu/blas.c - pattern-recognise matmul-shaped kernels (gemm,
// batched gemm, contraction, im2col) from the lifted UOp DAG and dispatch
// them to cblas_*: Apple Accelerate on macOS, OpenBLAS / BLIS on Linux.
//
// Every dispatcher reads M/N/K + dtype + slot mapping from
// ke->cached_lift.store_root via the uop_dag_classify_* extractors.
// The unified rangeify pass produces the canonical MUL+REDUCE+OPT_TC
// UOp DAG pattern for every matmul-shaped kernel, which
// kernel_lift_to_uop packages as the per-kernel root these dispatchers
// inspect.

#ifdef __APPLE__
#define ACCELERATE_NEW_LAPACK
// Include only the cblas sub-header, NOT the <Accelerate/Accelerate.h>
// umbrella: recent Command Line Tools SDKs (Xcode 26.x) ship a broken
// Sparse/SolveImplementationTyped.h that fails to compile even from C TUs
// ("too many arguments to function call").  This file uses only cblas_*
// (sgemm/dgemm), so the narrow header is sufficient
// and sidesteps the SDK breakage.
#include <vecLib/cblas_new.h>
#define HAVE_BLAS 1
#elif defined(THVM_HAVE_CBLAS)
// Linux: OpenBLAS / BLIS through the standard cblas interface.  The
// Makefile defines THVM_HAVE_CBLAS when it finds one (THVM_CBLAS=0 opts
// out); without it matmuls run as JIT-compiled loops, ~60x slower.
#include <cblas.h>
#define HAVE_BLAS 1
#else
#define HAVE_BLAS 0
#endif

#if HAVE_BLAS

// Count cblas dispatches landed via the DAG-side classifier.
// Queried by tests through the blas_*_dispatch_dag accessors below.
// Reset by thvm_init.
static u64 BLAS_GEMM_DISPATCH_DAG = 0;

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

// Counter accessors -- see top-of-file definition block.
fn u64 cpu_blas_gemm_dispatch_dag_count(void) {
  return BLAS_GEMM_DISPATCH_DAG;
}
fn void cpu_blas_gemm_dispatch_counters_reset(void) {
  BLAS_GEMM_DISPATCH_DAG = 0;
}

// Issue cblas_{s,d}gemm given a fully-populated shape (M, N, K, ldA,
// ldB, transA, transB, a_input, b_input).  Every route funnels through
// this so the actual dispatch site stays single-source.
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

// Each route's classifier plus the checks that need no buffer, shared by
// the route's dispatcher and cpu_blas_takes.

// GEMM: M/N/K + slot mapping + ldA/ldB + transpose flags from
// `uop_dag_classify_matmul_shape`.
static int blas_gemm_shape(KernelEntry const *ke, UopDagGemmShape *g) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, g)) {
    return 0;
  }
  // cblas has no bf16 GEMM (the classifier admits bf16 for the Metal
  // tensor-core path); decline so a bf16 matmul falls to the interpreter.
  if (g->dtype != DT_FP32 && g->dtype != DT_FP64) return 0;
  // Resolve symbolic (kvar) shape dims to their per-dispatch bound value.
  // classify returns the output leading dim M kvar-packed for a symbolic-M
  // matmul ({S, dim} = onehot . tokT, S the seq kvar); the bind is set before
  // fire, so GEMM runs the actual M (buffers are sized at the kvar upper
  // bound, GEMM writes the first M rows).  Identity for literal dims.
  g->M   = kvar_extent_runtime(g->M);
  g->N   = kvar_extent_runtime(g->N);
  g->K   = kvar_extent_runtime(g->K);
  g->ldA = kvar_extent_runtime(g->ldA);
  g->ldB = kvar_extent_runtime(g->ldB);
  return g->N > 1;
}

// TRUE batched gemm (see blas_try_batched_gemm).
static int blas_batched_gemm_shape(KernelEntry const *ke,
                                   UopDagBatchedGemmShape *g) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_batched_gemm(ke->cached_lift.store_root, ke, g)) {
    return 0;
  }
  return g->dtype == DT_FP32 && (g->N >= 2 || g->M >= 2);
}

// Generalized contraction (see blas_try_contraction).
static int blas_contraction_shape(KernelEntry const *ke,
                                  UopDagContractionShape *c) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_contraction_shape(ke->cached_lift.store_root, ke, c)) {
    return 0;
  }
  // Skip degenerate shapes a simpler classifier handles better.
  if (c->K < 2) return 0;
  if (c->inner_M < 2 && c->N < 2) return 0;
  return c->dtype == DT_FP32;
}

// im2col conv contraction (see blas_try_im2col_contraction).
static int blas_im2col_shape(KernelEntry const *ke, UopDagIm2colShape *c) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_im2col_contraction(ke->cached_lift.store_root, ke, c)) {
    return 0;
  }
  if (c->dtype != DT_FP32) return 0;
  // Sanity bounds (defensive).
  if (c->M < 2 || (u64)c->K_row * c->K_col < 4) return 0;
  u64 n_total = (u64)c->N_patchless * c->KH_total;
  if (n_total < 2) return 0;
  if (c->N_outer_extent < 1) return 0;
  // The per-dispatch patches scratch is capped below 1 GiB.
  u64 scratch_n = n_total * c->K_row * c->K_col;
  return scratch_n != 0 && scratch_n <= (u64)1 << 28;
}

// DAG-side GEMM dispatcher.  Returns 1 on dispatch, 0 on no-match.
static int blas_try_gemm(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  UopDagGemmShape gemm;
  if (!blas_gemm_shape(ke, &gemm)) return 0;
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

// TRUE batched gemm: A{batch,M,K} . B{batch,K,N} -> C{batch,M,N} as a loop of
// cblas_sgemm (one per batch).  The batched multi-head attention's mhaBmm is
// the instance -- it would otherwise fall to the slow codegen reduce (the
// per-head loop it replaced used 12 BLAS sgemms).  Operands are permuted
// views, so ld/trans/batch-strides come from the address coefficients.
static int blas_try_batched_gemm(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  UopDagBatchedGemmShape g;
  if (!blas_batched_gemm_shape(ke, &g)) return 0;
  u32 a_buf = in_buf_ids[g.a_input], b_buf = in_buf_ids[g.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
  u32 esz = sizeof(float);
  u64 a_elems = (u64)(CPU_BUFS[a_buf].nbytes / esz);
  u64 b_elems = (u64)(CPU_BUFS[b_buf].nbytes / esz);
  u64 o_elems = (u64)(CPU_BUFS[out_buf_id].nbytes / esz);
  if (a_elems < (u64)(g.batch - 1) * g.batch_stride_a + g.a_span) return 0;
  if (b_elems < (u64)(g.batch - 1) * g.batch_stride_b + g.b_span) return 0;
  if (o_elems < (u64)(g.batch - 1) * g.batch_stride_c + g.c_span) return 0;
  enum CBLAS_TRANSPOSE tA = g.transA ? CblasTrans : CblasNoTrans;
  enum CBLAS_TRANSPOSE tB = g.transB ? CblasTrans : CblasNoTrans;
  float const *A = (float const *)CPU_BUFS[a_buf].data;
  float const *B = (float const *)CPU_BUFS[b_buf].data;
  float       *C = (float       *)CPU_BUFS[out_buf_id].data;
  for (u32 bi = 0; bi < g.batch; bi++) {
    cblas_sgemm(CblasRowMajor, tA, tB, (int)g.M, (int)g.N, (int)g.K,
                1.0f, A + (u64)bi * g.batch_stride_a, (int)g.ldA,
                      B + (u64)bi * g.batch_stride_b, (int)g.ldB,
                0.0f, C + (u64)bi * g.batch_stride_c, (int)g.ldC);
  }
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
  UopDagContractionShape c;
  if (!blas_contraction_shape(ke, &c)) return 0;
  u32 a_buf = in_buf_ids[c.a_input];
  u32 b_buf = in_buf_ids[c.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
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
  UopDagIm2colShape c;
  if (!blas_im2col_shape(ke, &c)) return 0;
  u64 K_total  = (u64)c.K_row * c.K_col;            // OH*OW (the pixel block)
  u64 N_total  = (u64)c.N_patchless * c.KH_total;   // Cin*KH*KW (the channel block)
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
  return 1;
}

// Bisection knob: THVM_CPU_BLAS_DISABLE=1 forces every kernel past the
// BLAS try-ladder so the walker / JIT path runs instead.  Used for
// isolating BLAS-classifier bugs (mis-mapped A/B operand order, bad
// transA/transB recovery) from kernel-body bugs.
static int blas_disabled(void) {
  static int slot = -1;
  return env_flag_on(&slot, "THVM_CPU_BLAS_DISABLE");
}

// 1 iff cpu_blas_dispatch would take this kernel, judged from its lifted
// DAG before any buffer exists.  hand_opts leaves exactly these kernels
// bare: an UPCAST/UNROLL would stop the classifiers from recognising them.
fn int cpu_blas_takes(KernelEntry const *ke) {
  if (blas_disabled()) return 0;
  UopDagGemmShape        gemm;
  UopDagBatchedGemmShape batched;
  UopDagContractionShape contr;
  UopDagIm2colShape      im2col;
  return blas_gemm_shape(ke, &gemm) || blas_batched_gemm_shape(ke, &batched)
      || blas_contraction_shape(ke, &contr) || blas_im2col_shape(ke, &im2col);
}

// Returns KDISPATCH_BLAS_GEMM when a route fired so the profiler can
// record it, or 0 on no-match (caller falls through to JIT / interpreter).
fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (blas_disabled()) return 0;
  if (blas_try_gemm(ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_GEMM;
  // True batched gemm (batch axis in A AND B AND output) -- the batched
  // multi-head attention bmm.  Before the contraction try (whose batch is
  // M-side / A-shared and would decline on a batch-in-both axis anyway).
  if (blas_try_batched_gemm(ke, in_buf_ids, out_buf_id))
    return KDISPATCH_BLAS_GEMM;
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

fn int cpu_blas_takes(KernelEntry const *ke) {
  (void)ke;
  return 0;
}

fn u64  cpu_blas_gemm_dispatch_dag_count   (void) { return 0; }
fn void cpu_blas_gemm_dispatch_counters_reset(void) {}

#endif
