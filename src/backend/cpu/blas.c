// backend/cpu/blas.c - recognise matmul-shaped kernels (gemm, batched gemm,
// contraction, im2col) in the lifted UOp DAG (uop_dag_classify_*) and run them
// in cblas: Apple Accelerate on macOS, OpenBLAS or BLIS on Linux.

#ifdef __APPLE__
#define ACCELERATE_NEW_LAPACK
// The cblas sub-header, not the Accelerate.h umbrella: Xcode 26 SDKs ship a
// Sparse header that fails to compile from C.
#include <vecLib/cblas_new.h>
#define HAVE_BLAS 1
#elif defined(THVM_HAVE_CBLAS)
// The Makefile defines THVM_HAVE_CBLAS when it finds OpenBLAS or BLIS
// (THVM_CBLAS=0 opts out); without it matmuls run as JIT loops, ~60x slower.
#include <cblas.h>
#define HAVE_BLAS 1
#else
#define HAVE_BLAS 0
#endif

#if HAVE_BLAS

// GEMM and batched-GEMM dispatches, read by tests.
static u64 BLAS_GEMM_DISPATCH_DAG = 0;

// 1 iff getenv(name) starts with '1', read once into *slot (-1 = unread).
static int env_flag_on(int *slot, const char *name) {
  if (*slot == -1) {
    char const *e = getenv(name);
    *slot = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return *slot;
}

fn u64 cpu_blas_gemm_dispatch_dag_count(void) {
  return BLAS_GEMM_DISPATCH_DAG;
}
fn void cpu_blas_gemm_dispatch_counters_reset(void) {
  BLAS_GEMM_DISPATCH_DAG = 0;
}

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

static int blas_gemm_shape(KernelEntry const *ke, UopDagGemmShape *g) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, g)) {
    return 0;
  }
  // cblas has no bf16 GEMM (the classifier admits bf16 for the Metal
  // tensor-core path), so a bf16 matmul falls through to the walker.
  if (g->dtype != DT_FP32 && g->dtype != DT_FP64) return 0;
  // A symbolic (kvar) dim runs at its bound value; buffers are sized at the
  // kvar's upper bound, so the GEMM fills the first M rows.
  g->M   = kvar_extent_runtime(g->M);
  g->N   = kvar_extent_runtime(g->N);
  g->K   = kvar_extent_runtime(g->K);
  g->ldA = kvar_extent_runtime(g->ldA);
  g->ldB = kvar_extent_runtime(g->ldB);
  return g->N > 1;
}

static int blas_batched_gemm_shape(KernelEntry const *ke,
                                   UopDagBatchedGemmShape *g) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_batched_gemm(ke->cached_lift.store_root, ke, g)) {
    return 0;
  }
  return g->dtype == DT_FP32 && (g->N >= 2 || g->M >= 2);
}

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

static int blas_im2col_shape(KernelEntry const *ke, UopDagIm2colShape *c) {
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_classify_im2col_contraction(ke->cached_lift.store_root, ke, c)) {
    return 0;
  }
  if (c->dtype != DT_FP32) return 0;
  if (c->M < 2 || (u64)c->K_row * c->K_col < 4) return 0;
  u64 n_total = (u64)c->N_patchless * c->KH_total;
  if (n_total < 2) return 0;
  if (c->N_outer_extent < 1) return 0;
  // Caps the per-dispatch patches scratch at 2^28 floats (1 GiB).
  u64 scratch_n = n_total * c->K_row * c->K_col;
  return scratch_n != 0 && scratch_n <= (u64)1 << 28;
}

static int blas_try_gemm(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  UopDagGemmShape gemm;
  if (!blas_gemm_shape(ke, &gemm)) return 0;
  u32 a_buf = in_buf_ids[gemm.a_input];
  u32 b_buf = in_buf_ids[gemm.b_input];
  if (a_buf == 0 || b_buf == 0) return 0;
  u32 elem_bytes = (gemm.dtype == DT_FP32) ? sizeof(float) : sizeof(double);
  u32 a_elems = (u32)(CPU_BUFS[a_buf].nbytes / elem_bytes);
  u32 b_elems = (u32)(CPU_BUFS[b_buf].nbytes / elem_bytes);
  if (a_elems < gemm.M * gemm.K) return 0;
  if (b_elems < gemm.K * gemm.N) return 0;
  // ldC is the output's row stride, not N: a symbolic-seq output is allocated
  // at the kvar's upper bound, so its stride can exceed the runtime N.
  u32 ldC = (u32) TENS[ke->output_tid].view.strides[0];
  if (ldC == 0) ldC = gemm.N;
  blas_emit_gemm(gemm.dtype, gemm.M, gemm.N, gemm.K, gemm.ldA, gemm.ldB, ldC,
                 gemm.flags & 1u, gemm.flags & 2u,
                 a_buf, b_buf, out_buf_id);
  BLAS_GEMM_DISPATCH_DAG++;
  return 1;
}

// A{batch,M,K} . B{batch,K,N} as one sgemm per batch (multi-head attention);
// ld, trans and batch strides come from the permuted views' addresses.
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

// out[M_outer, M_inner, N] = sum_K W[K, N] * G[M_outer, K, M_inner] (the
// conv-backward x-grad) as one sgemm per M_outer slice, G transposed.
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

// patches[((cin*KH + kh)*KW + kw)*OH*OW + oh*OW + ow] =
// X_b[cin*X_Cin_str + (oh + kh)*X_W + ow + kw], one memcpy per OW-long row.
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

// im2col conv (tinygrad mixin/movement.py:569 _pool), one sgemm per batch:
// forward out[b] = W . patches, weight-grad dW += dY[b] . patches^T.
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
    if (c.forward_conv) {
      // Each batch writes its own out slice, so beta is 0.
      float *out_b = out_base + (u64)b * c.N_outer_out_stride;
      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                  (int)c.M, (int)K_total, (int)N_total,
                  1.0f, B_base,  (int)c.Y_M_stride,     // W,       ldA = N_total
                        patches, (int)K_total,          // patches, ldB = K_total
                  0.0f, out_b,   (int)c.out_M_stride);  // out[b],  ldC = K_total
    } else {
      // beta 1 after the first batch accumulates dW over the batch.
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

// THVM_CPU_BLAS_DISABLE=1 sends every kernel past BLAS, to bisect a classifier.
static int blas_disabled(void) {
  static int slot = -1;
  return env_flag_on(&slot, "THVM_CPU_BLAS_DISABLE");
}

// 1 iff cpu_blas_dispatch would take this kernel, judged before any buffer
// exists: hand_opts leaves these bare, as an UPCAST would hide the pattern.
fn int cpu_blas_takes(KernelEntry const *ke) {
  if (blas_disabled()) return 0;
  UopDagGemmShape        gemm;
  UopDagBatchedGemmShape batched;
  UopDagContractionShape contr;
  UopDagIm2colShape      im2col;
  return blas_gemm_shape(ke, &gemm) || blas_batched_gemm_shape(ke, &batched)
      || blas_contraction_shape(ke, &contr) || blas_im2col_shape(ke, &im2col);
}

// KDISPATCH_BLAS_GEMM, for the profiler, when a route ran; 0 otherwise.
fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (blas_disabled()) return 0;
  if (blas_try_gemm(ke, in_buf_ids, out_buf_id)) return KDISPATCH_BLAS_GEMM;
  if (blas_try_batched_gemm(ke, in_buf_ids, out_buf_id))
    return KDISPATCH_BLAS_GEMM;
  if (blas_try_contraction(ke, in_buf_ids, out_buf_id))
    return KDISPATCH_BLAS_GEMM;
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
