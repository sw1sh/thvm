// test_uop_recognise_tc.c - F3.1: pre-render pass that wraps the
// matmul shape with UOP_OPT(_, TC, 0). Verifies positive recognition
// (matmul -> wrapped) and negative gates (non-matmul -> unchanged,
// SAME-input matmul -> unchanged, MAX reduction -> unchanged).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // Build a 16x16 = 16x32 @ 32x16 matmul shape mirroring the
  // render_uop test.  STORE(C, m*N+n,
  //   REDUCE(MUL(A[m*K+k], B[k*N+n]), SUM, k)).
  // 2-D buffers so A ([16,32]) and B ([32,16]) hash-cons distinctly,
  // even though both flatten to 512 elements. The 1-D dimsA={512},
  // dimsB={512} form would have hash-collapsed them to one term and
  // tripped the recogniser's "same input" gate.
  u32 dimsA[2] = { 16, 32 };
  u32 dimsB[2] = { 32, 16 };
  u32 dimsC[2] = { 16, 16 };
  Term A = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA);
  Term B = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB);
  Term C = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsC);

  Term r_m = uop_range(0, 0 /*LOOP*/,   16);
  Term r_n = uop_range(1, 0,            16);
  Term r_k = uop_range(2, 1 /*REDUCE*/, 32);
  Term k16 = uop_const(DT_INT32, 16);
  Term k32 = uop_const(DT_INT32, 32);

  // A[m*K + k]
  Term mK    = uop_int_binary(UOP_IMUL, r_m, k32);
  Term addrA = uop_int_binary(UOP_IADD, mK, r_k);
  Term ldA   = uop_index_e(A, addrA);
  // B[k*N + n]
  Term kN    = uop_int_binary(UOP_IMUL, r_k, k16);
  Term addrB = uop_int_binary(UOP_IADD, kN, r_n);
  Term ldB   = uop_index_e(B, addrB);
  // MUL + REDUCE(SUM, k)
  Term mul   = uop_binary(UOP_MUL, ldA, ldB);
  Term red   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul);
  // C[m*N + n]
  Term mN    = uop_int_binary(UOP_IMUL, r_m, k16);
  Term addrC = uop_int_binary(UOP_IADD, mN, r_n);
  Term store = uop_store(C, addrC, red);

  TEST_BEGIN("recognise-tc/matmul-wraps-with-opt-tc");
  Term wrapped = uop_recognise_tc(store);
  CHECK(wrapped != 0);
  CHECK(wrapped != store);
  CHECK_EQ(term_tag(wrapped), TAG_UOP);
  CHECK_EQ(term_ext(wrapped), UOP_STORE);
  // Inspect: STORE.value should now be UOP_OPT(red, TC, 0).
  Term wval = heap_read(term_val(wrapped) + 2);
  CHECK_EQ(term_ext(wval), UOP_OPT);
  CHECK_EQ(uop_opt_target(wval), red);
  CHECK_EQ(uop_opt_kind  (wval), UOP_OPT_TC);
  CHECK_EQ(uop_opt_factor(wval), 0u);

  TEST_BEGIN("recognise-tc/idempotent-on-already-wrapped");
  Term wrapped2 = uop_recognise_tc(wrapped);
  // STORE.value is already UOP_OPT (not UOP_REDUCE), so the pattern
  // doesn't match and we get the input back unchanged.
  CHECK_EQ(wrapped2, wrapped);

  TEST_BEGIN("recognise-tc/same-buffer-rejects");
  // Square-of-self matmul (REDUCE(MUL(A, A))) is not gemm-shaped --
  // a tensor x tensor reduce w/ identical operand isn't matmul.
  // The recogniser's distinctness gate must reject this.
  Term ldA2     = uop_index_e(A, addrA);
  Term mul_sq   = uop_binary(UOP_MUL, ldA, ldA2);
  Term red_sq   = uop_reduce(REDUCE_SUM, /*axis=*/2, mul_sq);
  Term store_sq = uop_store(C, addrC, red_sq);
  Term out_sq   = uop_recognise_tc(store_sq);
  CHECK_EQ(out_sq, store_sq);

  TEST_BEGIN("recognise-tc/max-reduce-rejects");
  // REDUCE_MAX is not matmul.  render_uop's TC template only handles
  // SUM; the recogniser gates accordingly so a useless OPT wrap
  // doesn't get installed.
  Term red_max   = uop_reduce(REDUCE_MAX, /*axis=*/2, mul);
  Term store_max = uop_store(C, addrC, red_max);
  Term out_max   = uop_recognise_tc(store_max);
  CHECK_EQ(out_max, store_max);

  TEST_BEGIN("recognise-tc/non-store-passes-through");
  // Non-STORE roots (e.g. a bare REDUCE or anything else) should
  // pass through unchanged -- the recogniser only operates on
  // matmul-shaped UOP_STORE roots.
  CHECK_EQ(uop_recognise_tc(red),  red);
  CHECK_EQ(uop_recognise_tc(A),    A);

  TEST_BEGIN("recognise-tc/non-matmul-store-passes-through");
  // STORE of a non-matmul value (e.g. just a CONST broadcast).  The
  // pattern requires REDUCE(MUL(INDEX_E, INDEX_E)); without that
  // shape we keep the original.
  Term zero      = uop_const(DT_FP32, 0);
  Term store_z   = uop_store(C, addrC, zero);
  Term out_z     = uop_recognise_tc(store_z);
  CHECK_EQ(out_z, store_z);

  TEST_BEGIN("recognise-tc/classify-reports-k-extent");
  // Classify the matmul shape we built: K extent = 32 (multiple of 8).
  // The dispatch ladder uses this to decide whether render_uop's
  // simdgroup_matrix template will fit (K%8==0) or whether to bail
  // and let metal_try_gemm's tile-shared-mem path take over (F3.4).
  u32 mm_k = 0;
  CHECK(uop_classify_matmul(store, &mm_k));
  CHECK_EQ(mm_k, 32u);

  // Non-matmul store: classify returns 0, K extent stays 0.
  u32 nm_k = 99;
  CHECK(!uop_classify_matmul(store_z, &nm_k));
  CHECK_EQ(nm_k, 0u);

  // Same-buffer (square-of-self) store: classify returns 0.
  u32 sq_k = 99;
  CHECK(!uop_classify_matmul(store_sq, &sq_k));
  CHECK_EQ(sq_k, 0u);

  // MAX-reduce store: classify returns 0 (SUM-only).
  u32 mx_k = 99;
  CHECK(!uop_classify_matmul(store_max, &mx_k));
  CHECK_EQ(mx_k, 0u);

  // K extent NOT a multiple of 8: classify still returns 1 (matmul
  // shape) but the caller (cg_emit_via_uop) is responsible for
  // checking K%8 and deciding the dispatch fall-through.  Build a
  // fresh matmul with K=7 and verify.
  u32 dimsA7[2] = {16, 7};
  u32 dimsB7[2] = {7, 16};
  Term A7 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA7);
  Term B7 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB7);
  Term r_k7 = uop_range(7, 1, 7);
  Term k7   = uop_const(DT_INT32, 7);
  Term mK7  = uop_int_binary(UOP_IMUL, r_m, k7);
  Term aA7  = uop_int_binary(UOP_IADD, mK7, r_k7);
  Term ldA7 = uop_index_e(A7, aA7);
  Term kN7  = uop_int_binary(UOP_IMUL, r_k7, k16);
  Term aB7  = uop_int_binary(UOP_IADD, kN7, r_n);
  Term ldB7 = uop_index_e(B7, aB7);
  Term mul7 = uop_binary(UOP_MUL, ldA7, ldB7);
  Term red7 = uop_reduce(REDUCE_SUM, /*axis=*/7, mul7);
  Term st7  = uop_store(C, addrC, red7);
  u32 k7_out = 0;
  CHECK(uop_classify_matmul(st7, &k7_out));
  CHECK_EQ(k7_out, 7u);

  TEST_BEGIN("recognise-tc/mn-axes-extract");
  // The canonical matmul (M=16 on axis 0, N=16 on axis 1, K=32 on
  // axis 2) -- uop_matmul_mn_axes returns the two non-reduce output
  // axes + extents the parallel-TC wrap re-stamps GLOBAL.
  {
    u32 m_ax = 99, m_ext = 0, n_ax = 99, n_ext = 0;
    CHECK(uop_matmul_mn_axes(store, &m_ax, &m_ext, &n_ax, &n_ext));
    CHECK_EQ(m_ax, 0u);
    CHECK_EQ(m_ext, 16u);
    CHECK_EQ(n_ax, 1u);
    CHECK_EQ(n_ext, 16u);
    // Non-matmul store: returns 0, out params reset.
    u32 zm = 7, zn = 7;
    CHECK(!uop_matmul_mn_axes(store_z, &zm, NULL, &zn, NULL));
  }

  TEST_BEGIN("recognise-tc/parallel-stamps-m-n-global");
  // K/M/N all multiples of 8 -> parallel wrap installs OPT(_, TC, 0)
  // AND re-stamps the M (axis 0) and N (axis 1) ranges KAX_GLOBAL so
  // render_uop's rmu_emit_matmul_tc takes the parallel_tc branch and
  // cg_tile_metal_dispatch_shape launches one threadgroup per 8x8 tile.
  {
    Term par = uop_recognise_tc_parallel(store);
    CHECK(par != 0);
    CHECK(par != store);
    // STORE.value is OPT(_, TC, 0).
    Term pval = heap_read(term_val(par) + 2);
    CHECK_EQ(term_ext(pval), UOP_OPT);
    CHECK_EQ(uop_opt_kind(pval), UOP_OPT_TC);
    // addr_c references RANGE(0, GLOBAL, 16) and RANGE(1, GLOBAL, 16).
    Term paddr = heap_read(term_val(par) + 1);
    Term pr[16]; u32 pn = 0;
    rmu_collect_ranges(paddr, pr, &pn);
    int m_global = 0, n_global = 0;
    for (u32 i = 0; i < pn; i++) {
      u32 aid   = (u32)term_val(heap_read(term_val(pr[i]) + 0));
      u32 atype = (u32)term_val(heap_read(term_val(pr[i]) + 1));
      if (aid == 0 && atype == KAX_GLOBAL) m_global = 1;
      if (aid == 1 && atype == KAX_GLOBAL) n_global = 1;
    }
    CHECK(m_global);
    CHECK(n_global);
  }

  TEST_BEGIN("recognise-tc/parallel-ragged-k-keeps-axes-loop");
  // K=7 (not a multiple of 8) -> the simdgroup template bails to the
  // scalar accumulator (output_numel grid), so the parallel wrap must
  // NOT stamp GLOBAL (it would mis-size the dispatch grid).  Falls back
  // to the plain (guarded) wrap: OPT installed, axes stay LOOP.
  {
    Term par7 = uop_recognise_tc_parallel(st7);
    CHECK(par7 != 0);
    Term p7val = heap_read(term_val(par7) + 2);
    CHECK_EQ(term_ext(p7val), UOP_OPT);
    CHECK_EQ(uop_opt_kind(p7val), UOP_OPT_TC);
    Term p7addr = heap_read(term_val(par7) + 1);
    Term p7r[16]; u32 p7n = 0;
    rmu_collect_ranges(p7addr, p7r, &p7n);
    for (u32 i = 0; i < p7n; i++) {
      u32 atype = (u32)term_val(heap_read(term_val(p7r[i]) + 1));
      CHECK(atype != KAX_GLOBAL);
    }
  }

  TEST_BEGIN("recognise-tc/dag-classify-matmul-shape-unwrapped");
  {
    // Build a synthetic 16x32 @ 32x16 matmul kernel + populate the
    // KernelEntry fields uop_dag_classify_matmul_shape consults
    // (cached_lift.store_root + input_views[]).  Verifies the
    // shape extractor recovers M=16, N=16, K=32, ldA=32, ldB=16,
    // a_input=0, b_input=1, no transposes, dtype=DT_FP32.
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts[2]   = {DT_FP32, DT_FP32};
    static View vws[2];
    vws[0].shape.ndim = 3;
    vws[0].shape.dims[0] = 16; vws[0].shape.dims[1] = 32; vws[0].shape.dims[2] = 16;
    vws[0].strides[0]    = 32; vws[0].strides[1]    = 1;  vws[0].strides[2]    = 0;
    vws[1].shape.ndim = 3;
    vws[1].shape.dims[0] = 16; vws[1].shape.dims[1] = 32; vws[1].shape.dims[2] = 16;
    vws[1].strides[0]    = 0;  vws[1].strides[1]    = 16; vws[1].strides[2]    = 1;
    ke.input_dtypes = dts;
    ke.input_views  = vws;

    // Build BUFFER terms with instance == slot+1 for inputs and 0
    // for the output, mirroring lift_input_buffer / lift_output_buffer.
    u32 dimsAi[2] = {16, 32};
    u32 dimsBi[2] = {32, 16};
    u32 dimsCi[2] = {16, 16};
    Term Ai = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsAi, 1);
    Term Bi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsBi, 2);
    Term Ci = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsCi, 0);
    Term r_mi = uop_range(20, 0,            16);
    Term r_ni = uop_range(21, 0,            16);
    Term r_ki = uop_range(22, 1 /*REDUCE*/, 32);
    Term k16i = uop_const(DT_INT32, 16);
    Term k32i = uop_const(DT_INT32, 32);
    Term mKi  = uop_int_binary(UOP_IMUL, r_mi, k32i);
    Term aAi  = uop_int_binary(UOP_IADD, mKi, r_ki);
    Term ldAi = uop_index_e(Ai, aAi);
    Term kNi  = uop_int_binary(UOP_IMUL, r_ki, k16i);
    Term aBi  = uop_int_binary(UOP_IADD, kNi, r_ni);
    Term ldBi = uop_index_e(Bi, aBi);
    Term muli = uop_binary(UOP_MUL, ldAi, ldBi);
    Term redi = uop_reduce(REDUCE_SUM, /*axis=*/22, muli);
    Term mNi  = uop_int_binary(UOP_IMUL, r_mi, k16i);
    Term aCi  = uop_int_binary(UOP_IADD, mNi, r_ni);
    Term store_unwrapped = uop_store(Ci, aCi, redi);
    ke.cached_lift.store_root = store_unwrapped;

    UopDagGemmShape g = {0};
    CHECK(uop_dag_classify_matmul_shape(store_unwrapped, &ke, &g));
    CHECK_EQ(g.dtype, (u32)DT_FP32);
    CHECK_EQ(g.M, 16u);
    CHECK_EQ(g.N, 16u);
    CHECK_EQ(g.K, 32u);
    CHECK_EQ(g.a_input, 0u);
    CHECK_EQ(g.b_input, 1u);
    CHECK_EQ(g.ldA, 32u);
    CHECK_EQ(g.ldB, 16u);
    CHECK_EQ(g.flags, 0u);

    TEST_BEGIN("recognise-tc/dag-classify-matmul-shape-tc-wrapped");
    // Same shape but with the OPT(_, TC, 0) wrapper installed --
    // mirrors what kernel_lift_from_gemm produces.  Classifier must
    // peel the wrapper and still recognise the matmul.
    Term tci   = uop_opt(redi, UOP_OPT_TC, 0);
    Term store_wrapped = uop_store(Ci, aCi, tci);
    UopDagGemmShape g2 = {0};
    CHECK(uop_dag_classify_matmul_shape(store_wrapped, &ke, &g2));
    CHECK_EQ(g2.M, 16u);
    CHECK_EQ(g2.N, 16u);
    CHECK_EQ(g2.K, 32u);
    CHECK_EQ(g2.a_input, 0u);
    CHECK_EQ(g2.b_input, 1u);
    CHECK_EQ(g2.flags, 0u);

    TEST_BEGIN("recognise-tc/dag-classify-non-matmul-rejected");
    // STORE of CONST -- not a matmul.  Returns 0; out struct stays
    // unchanged (caller-provided initialisation is preserved on bail).
    Term zeroi = uop_const(DT_FP32, 0);
    Term store_z = uop_store(Ci, aCi, zeroi);
    UopDagGemmShape g3 = {0};
    g3.M = 0xDEADu;  // Sentinel: classifier must not overwrite on reject.
    CHECK(!uop_dag_classify_matmul_shape(store_z, &ke, &g3));
    CHECK_EQ(g3.M, 0xDEADu);
  }

  TEST_BEGIN("recognise-tc/conv-shape-rejected-too-many-ranges");
  // Conv2d single-input lift output: REDUCE(MUL(W[co*K+q], X[bi*Sb +
  // ci*S2 + (oh+kh_v)*S0 + ...])).  W's address has 2 ranges, X's
  // has 4+. The classifier must reject because the simdgroup_matrix
  // template assumes 2-range linear layout per operand (matmul);
  // applying it to conv X address would produce wrong loads.
  u32 dimsW[2] = {16, 9};       // c_out=16, K=c_in*kh*kw=9
  u32 dimsX[1] = {1024};        // X is rank-1 contiguous in conv lift
  Term Wb = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsW);
  Term Xb = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsX);

  Term r_co = uop_range(10, 0, 16);
  Term r_q  = uop_range(11, 1, 9);
  Term r_bi = uop_range(12, 0, 4);
  Term r_oh = uop_range(13, 0, 8);
  Term r_ow = uop_range(14, 0, 8);
  Term k9_  = uop_const(DT_INT32, 9);
  Term sb   = uop_const(DT_INT32, 256);
  Term s0   = uop_const(DT_INT32, 8);
  Term s1   = uop_const(DT_INT32, 1);
  // W[co*9 + q]
  Term coK  = uop_int_binary(UOP_IMUL, r_co, k9_);
  Term wi   = uop_int_binary(UOP_IADD, coK, r_q);
  Term ldW  = uop_index_e(Wb, wi);
  // X[bi*sb + oh*s0 + ow*s1] -- 3 ranges (bi/oh/ow) so classifier rejects
  Term biSb = uop_int_binary(UOP_IMUL, r_bi, sb);
  Term ohS0 = uop_int_binary(UOP_IMUL, r_oh, s0);
  Term owS1 = uop_int_binary(UOP_IMUL, r_ow, s1);
  Term xi_a = uop_int_binary(UOP_IADD, biSb, ohS0);
  Term xi   = uop_int_binary(UOP_IADD, xi_a, owS1);
  Term ldX  = uop_index_e(Xb, xi);
  Term mulC = uop_binary(UOP_MUL, ldW, ldX);
  Term redC = uop_reduce(REDUCE_SUM, /*axis=*/11, mulC);
  Term stC  = uop_store(C, addrC, redC);
  u32 conv_k = 99;
  CHECK(!uop_classify_matmul(stC, &conv_k));
  // Also: uop_recognise_tc on this conv-shape store returns input
  // unchanged (no OPT wrap installed).
  CHECK_EQ(uop_recognise_tc(stC), stC);

  // === DOT classifier coverage ===
  TEST_BEGIN("recognise-tc/dot-classifier-positive");
  {
    // Build STORE(C{1}, 0, REDUCE_SUM(MUL(INDEX_E(A, k), INDEX_E(B, k)))).
    // Use _inst variants with distinct instance ids so A/B don't hash-
    // cons to the same Term (they're shape-identical rank-1 K-elem
    // buffers, just like the lift produces for input slots).
    u32 dimsAv[1] = {64};
    u32 dimsBv[1] = {64};
    u32 dimsCv[1] = {1};
    Term Av = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsAv, 1);
    Term Bv = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsBv, 2);
    Term Cv = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCv, 0);
    Term r_kd = uop_range(40, 1 /*REDUCE*/, 64);
    Term ldAv = uop_index_e(Av, r_kd);
    Term ldBv = uop_index_e(Bv, r_kd);
    Term mulD = uop_binary(UOP_MUL, ldAv, ldBv);
    Term redD = uop_reduce(REDUCE_SUM, /*axis=*/40, mulD);
    Term zero = uop_const(DT_INT32, 0);
    Term stD  = uop_store(Cv, zero, redD);
    u32 dk = 0;
    CHECK(uop_classify_dot(stD, &dk));
    CHECK_EQ(dk, 64u);
    // Matmul classifier rejects this shape (1 range per addr, not 2).
    u32 mk = 99;
    CHECK(!uop_classify_matmul(stD, &mk));
    CHECK_EQ(mk, 0u);
    // GEMV classifier also rejects (needs 1+2 ranges, has 1+1).
    u32 gk = 99; int wf = 1;
    CHECK(!uop_classify_gemv(stD, &gk, &wf));
    CHECK_EQ(gk, 0u);
    CHECK_EQ(wf, 0);
  }

  // === GEMV classifier coverage ===
  TEST_BEGIN("recognise-tc/gemv-classifier-positive");
  {
    // Build STORE(c{M}, m, REDUCE_SUM(MUL(INDEX_E(W, m*K+k),
    //                                      INDEX_E(x, k)))).
    enum { GM = 16, GK = 24 };
    u32 dimsW[2] = {GM, GK};
    u32 dimsX[1] = {GK};
    u32 dimsCo[1] = {GM};
    Term Wb = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsW);
    Term Xb = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsX);
    Term Co = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCo);
    Term r_mg = uop_range(50, 0 /*LOOP*/,   GM);
    Term r_kg = uop_range(51, 1 /*REDUCE*/, GK);
    Term kK   = uop_const(DT_INT32, GK);
    Term mK   = uop_int_binary(UOP_IMUL, r_mg, kK);
    Term aW   = uop_int_binary(UOP_IADD, mK, r_kg);
    Term ldW2 = uop_index_e(Wb, aW);
    Term ldXg = uop_index_e(Xb, r_kg);
    Term mulG = uop_binary(UOP_MUL, ldW2, ldXg);
    Term redG = uop_reduce(REDUCE_SUM, /*axis=*/51, mulG);
    Term stG  = uop_store(Co, r_mg, redG);
    u32 gk = 0; int wf = 0;
    CHECK(uop_classify_gemv(stG, &gk, &wf));
    CHECK_EQ(gk, (u32)GK);
    CHECK_EQ(wf, 1);  // matrix is on src[0] of MUL
    // DOT classifier rejects (needs 1+1, here 2+1).
    u32 dk = 99;
    CHECK(!uop_classify_dot(stG, &dk));
    CHECK_EQ(dk, 0u);
    // Matmul classifier rejects (needs 2+2 ranges, here 2+1).
    u32 mk = 99;
    CHECK(!uop_classify_matmul(stG, &mk));
    CHECK_EQ(mk, 0u);

    TEST_BEGIN("recognise-tc/gemv-classifier-w-on-src-1");
    // Same shape but MUL operands swapped: W on src[1], x on src[0].
    Term mulG2 = uop_binary(UOP_MUL, ldXg, ldW2);
    Term redG2 = uop_reduce(REDUCE_SUM, /*axis=*/51, mulG2);
    Term stG2  = uop_store(Co, r_mg, redG2);
    u32 gk2 = 0; int wf2 = 1;
    CHECK(uop_classify_gemv(stG2, &gk2, &wf2));
    CHECK_EQ(gk2, (u32)GK);
    CHECK_EQ(wf2, 0);
  }

  // === DAG-side shape extractor for DOT ===
  TEST_BEGIN("recognise-tc/dag-classify-dot-shape");
  {
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts2[2] = {DT_FP32, DT_FP32};
    static View vws2[2];
    vws2[0].shape.ndim = 1; vws2[0].shape.dims[0] = 64;
    vws2[0].strides[0] = 1;
    vws2[1].shape.ndim = 1; vws2[1].shape.dims[0] = 64;
    vws2[1].strides[0] = 1;
    ke.input_dtypes = dts2;
    ke.input_views  = vws2;

    u32 dimsAv[1] = {64};
    u32 dimsBv[1] = {64};
    u32 dimsCv[1] = {1};
    Term Ai2 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsAv, 1);
    Term Bi2 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsBv, 2);
    Term Ci2 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCv, 0);
    Term r_kd2 = uop_range(60, 1, 64);
    Term ldAv2 = uop_index_e(Ai2, r_kd2);
    Term ldBv2 = uop_index_e(Bi2, r_kd2);
    Term mulD2 = uop_binary(UOP_MUL, ldAv2, ldBv2);
    Term redD2 = uop_reduce(REDUCE_SUM, /*axis=*/60, mulD2);
    Term zero2 = uop_const(DT_INT32, 0);
    Term stD2  = uop_store(Ci2, zero2, redD2);
    ke.cached_lift.store_root = stD2;

    UopDagDotShape ds = {0};
    CHECK(uop_dag_classify_dot_shape(stD2, &ke, &ds));
    CHECK_EQ(ds.dtype,   (u32)DT_FP32);
    CHECK_EQ(ds.K,       64u);
    CHECK_EQ(ds.a_input, 0u);
    CHECK_EQ(ds.b_input, 1u);

    // Sentinel: classifier must not overwrite output struct on reject.
    Term zero_v = uop_const(DT_FP32, 0);
    Term store_zero = uop_store(Ci2, zero2, zero_v);
    UopDagDotShape ds2 = {0};
    ds2.K = 0xCAFEu;
    CHECK(!uop_dag_classify_dot_shape(store_zero, &ke, &ds2));
    CHECK_EQ(ds2.K, 0xCAFEu);
  }

  // === DAG-side shape extractor for GEMV ===
  TEST_BEGIN("recognise-tc/dag-classify-gemv-shape");
  {
    enum { GM2 = 16, GK2 = 24 };
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts3[2] = {DT_FP32, DT_FP32};
    static View vws3[2];
    // W: rank-2 {GM, GK} contiguous strides {GK, 1}.
    vws3[0].shape.ndim = 2;
    vws3[0].shape.dims[0] = GM2; vws3[0].shape.dims[1] = GK2;
    vws3[0].strides[0]    = GK2; vws3[0].strides[1]    = 1;
    // x: rank-1 {GK} contig strides {1}.
    vws3[1].shape.ndim = 1;
    vws3[1].shape.dims[0] = GK2;
    vws3[1].strides[0]    = 1;
    ke.input_dtypes = dts3;
    ke.input_views  = vws3;

    u32 dimsW[2]  = {GM2, GK2};
    u32 dimsX[1]  = {GK2};
    u32 dimsCo[1] = {GM2};
    Term Wi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsW, 1);
    Term Xi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsX, 2);
    Term Coi= uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCo,0);
    Term r_mg2 = uop_range(70, 0,            GM2);
    Term r_kg2 = uop_range(71, 1 /*REDUCE*/, GK2);
    Term kKv   = uop_const(DT_INT32, GK2);
    Term mK2   = uop_int_binary(UOP_IMUL, r_mg2, kKv);
    Term aW2   = uop_int_binary(UOP_IADD, mK2, r_kg2);
    Term ldW3  = uop_index_e(Wi, aW2);
    Term ldXg2 = uop_index_e(Xi, r_kg2);
    Term mulG3 = uop_binary(UOP_MUL, ldW3, ldXg2);
    Term redG3 = uop_reduce(REDUCE_SUM, /*axis=*/71, mulG3);
    Term stG3  = uop_store(Coi, r_mg2, redG3);
    ke.cached_lift.store_root = stG3;

    UopDagGemvShape gs = {0};
    CHECK(uop_dag_classify_gemv_shape(stG3, &ke, &gs));
    CHECK_EQ(gs.dtype,   (u32)DT_FP32);
    CHECK_EQ(gs.M,       (u32)GM2);
    CHECK_EQ(gs.K,       (u32)GK2);
    CHECK_EQ(gs.w_input, 0u);
    CHECK_EQ(gs.x_input, 1u);
    CHECK_EQ(gs.ldW,     (u32)GK2);
    CHECK_EQ(gs.flags,   0u);

    TEST_BEGIN("recognise-tc/dag-classify-gemv-x-broadcast-rank2");
    // Re-use ke but swap x view to rank-2 {M,K} broadcast strides {0,1}
    // (the WL TMatVec representation post-EXPAND).  Classifier must
    // accept this layout too.
    vws3[1].shape.ndim = 2;
    vws3[1].shape.dims[0] = GM2; vws3[1].shape.dims[1] = GK2;
    vws3[1].strides[0]    = 0;   vws3[1].strides[1]    = 1;
    UopDagGemvShape gs2 = {0};
    CHECK(uop_dag_classify_gemv_shape(stG3, &ke, &gs2));
    CHECK_EQ(gs2.M, (u32)GM2);
    CHECK_EQ(gs2.K, (u32)GK2);
    CHECK_EQ(gs2.w_input, 0u);
    CHECK_EQ(gs2.x_input, 1u);
  }

  // === addr-stride extractor =============================================
  TEST_BEGIN("recognise-tc/extract-strides-matmul-A-non-trans");
  {
    // addr_A = IADD(IMUL(r_m, ICONST(K)), r_k)  -- non-transposed A
    Term r_m_e = uop_range(100, 0, 16);
    Term r_k_e = uop_range(101, 1, 32);
    Term kK_e  = uop_const(DT_INT32, 32);
    Term mK_e  = uop_int_binary(UOP_IMUL, r_m_e, kK_e);
    Term aA_e  = uop_int_binary(UOP_IADD, mK_e, r_k_e);
    u32 red_c = 99, oth_c = 99, oth_a = 99;
    CHECK(uop_dag_extract_matmul_strides_from_addr(aA_e, /*red_axis=*/101,
                                                   &red_c, &oth_c, &oth_a));
    CHECK_EQ(red_c, 1u);   // r_k bare
    CHECK_EQ(oth_c, 32u);  // r_m * 32
    CHECK_EQ(oth_a, 100u); // m axis id
  }

  TEST_BEGIN("recognise-tc/extract-strides-matmul-A-trans");
  {
    // addr_A_trans = IADD(r_m, IMUL(r_k, ICONST(M)))  -- transposed A
    Term r_m_e = uop_range(110, 0, 16);
    Term r_k_e = uop_range(111, 1, 32);
    Term kM_e  = uop_const(DT_INT32, 16);
    Term kM_e2 = uop_int_binary(UOP_IMUL, r_k_e, kM_e);
    Term aAt   = uop_int_binary(UOP_IADD, r_m_e, kM_e2);
    u32 red_c = 99, oth_c = 99, oth_a = 99;
    CHECK(uop_dag_extract_matmul_strides_from_addr(aAt, /*red_axis=*/111,
                                                   &red_c, &oth_c, &oth_a));
    CHECK_EQ(red_c, 16u);  // r_k * 16 (= M)
    CHECK_EQ(oth_c, 1u);   // r_m bare
    CHECK_EQ(oth_a, 110u);
  }

  TEST_BEGIN("recognise-tc/extract-strides-matmul-B-non-trans");
  {
    // addr_B = IADD(IMUL(r_k, ICONST(N)), r_n) -- non-transposed B
    Term r_k_e = uop_range(120, 1, 32);
    Term r_n_e = uop_range(121, 0, 16);
    Term kN_e  = uop_const(DT_INT32, 16);
    Term kN_e2 = uop_int_binary(UOP_IMUL, r_k_e, kN_e);
    Term aB_e  = uop_int_binary(UOP_IADD, kN_e2, r_n_e);
    u32 red_c = 99, oth_c = 99, oth_a = 99;
    CHECK(uop_dag_extract_matmul_strides_from_addr(aB_e, /*red_axis=*/120,
                                                   &red_c, &oth_c, &oth_a));
    CHECK_EQ(red_c, 16u);  // r_k * 16 (= N)
    CHECK_EQ(oth_c, 1u);
    CHECK_EQ(oth_a, 121u);
  }

  TEST_BEGIN("recognise-tc/extract-strides-matmul-B-trans");
  {
    // addr_B_trans = IADD(r_k, IMUL(r_n, ICONST(K))) -- transposed B
    Term r_k_e = uop_range(130, 1, 32);
    Term r_n_e = uop_range(131, 0, 16);
    Term kK_e  = uop_const(DT_INT32, 32);
    Term kK_e2 = uop_int_binary(UOP_IMUL, r_n_e, kK_e);
    Term aBt   = uop_int_binary(UOP_IADD, r_k_e, kK_e2);
    u32 red_c = 99, oth_c = 99, oth_a = 99;
    CHECK(uop_dag_extract_matmul_strides_from_addr(aBt, /*red_axis=*/130,
                                                   &red_c, &oth_c, &oth_a));
    CHECK_EQ(red_c, 1u);   // r_k bare
    CHECK_EQ(oth_c, 32u);  // r_n * 32 (= K)
    CHECK_EQ(oth_a, 131u);
  }

  TEST_BEGIN("recognise-tc/extract-strides-rejects-non-iadd");
  {
    // Bare RANGE (no IADD) is not a 2-axis matmul address.
    Term r_k_e = uop_range(140, 1, 32);
    u32 red_c = 99, oth_c = 99, oth_a = 99;
    CHECK(!uop_dag_extract_matmul_strides_from_addr(r_k_e, 140,
                                                    &red_c, &oth_c, &oth_a));
    CHECK_EQ(red_c, 0u); CHECK_EQ(oth_c, 0u); CHECK_EQ(oth_a, 0u);
  }

  TEST_BEGIN("recognise-tc/extract-strides-rejects-wrong-red-axis");
  {
    // IADD-shaped but neither arm references the requested reduce axis.
    Term r_a = uop_range(150, 0, 16);
    Term r_b = uop_range(151, 0, 32);
    Term k4  = uop_const(DT_INT32, 4);
    Term mul = uop_int_binary(UOP_IMUL, r_a, k4);
    Term add = uop_int_binary(UOP_IADD, mul, r_b);
    u32 red_c = 99, oth_c = 99, oth_a = 99;
    CHECK(!uop_dag_extract_matmul_strides_from_addr(add, /*red_axis=*/999,
                                                    &red_c, &oth_c, &oth_a));
  }

  // === DAG-side classifier WITHOUT input_views[] populated ==============
  TEST_BEGIN("recognise-tc/dag-classify-matmul-no-input-views-non-trans");
  {
    // Re-run the matmul classifier with ke->input_views = NULL.  The
    // session-2 DAG path must succeed unaided.
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts[2]   = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts;
    ke.input_views  = NULL;          // <-- decoupling proof

    u32 dimsAi[2] = {16, 32};
    u32 dimsBi[2] = {32, 16};
    u32 dimsCi[2] = {16, 16};
    Term Ai = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsAi, 1);
    Term Bi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsBi, 2);
    Term Ci = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsCi, 0);
    Term r_mn = uop_range(200, 0,            16);
    Term r_nn = uop_range(201, 0,            16);
    Term r_kn = uop_range(202, 1 /*REDUCE*/, 32);
    Term k16n = uop_const(DT_INT32, 16);
    Term k32n = uop_const(DT_INT32, 32);
    // A: m * K + k  -> non-transposed
    Term aA  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, r_mn, k32n),
                              r_kn);
    // B: k * N + n  -> non-transposed
    Term aB  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, r_kn, k16n),
                              r_nn);
    Term ldAn = uop_index_e(Ai, aA);
    Term ldBn = uop_index_e(Bi, aB);
    Term mln  = uop_binary(UOP_MUL, ldAn, ldBn);
    Term rdn  = uop_reduce(REDUCE_SUM, /*axis=*/202, mln);
    Term aC   = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_mn, k16n),
                               r_nn);
    Term stN  = uop_store(Ci, aC, rdn);
    UopDagGemmShape g = {0};
    CHECK(uop_dag_classify_matmul_shape(stN, &ke, &g));
    CHECK_EQ(g.M, 16u); CHECK_EQ(g.N, 16u); CHECK_EQ(g.K, 32u);
    CHECK_EQ(g.ldA, 32u); CHECK_EQ(g.ldB, 16u);
    CHECK_EQ(g.flags, 0u);  // neither transposed
  }

  TEST_BEGIN("recognise-tc/dag-classify-matmul-no-input-views-trans-A");
  {
    // A transposed (storage [K,M] viewed [M,K]): addr = IADD(r_m,
    // IMUL(r_k, ICONST(M))).  ldA = M, transA = 1.
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts[2] = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts; ke.input_views = NULL;

    u32 dimsAt[2] = {32, 16};   // storage [K,M]
    u32 dimsBi[2] = {32, 16};
    u32 dimsCi[2] = {16, 16};
    Term At = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsAt, 1);
    Term Bi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsBi, 2);
    Term Ci = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsCi, 0);
    Term r_m  = uop_range(210, 0,            16);
    Term r_n  = uop_range(211, 0,            16);
    Term r_k  = uop_range(212, 1 /*REDUCE*/, 32);
    Term k16  = uop_const(DT_INT32, 16);
    // A transposed: addr = m + k*M
    Term aA   = uop_int_binary(UOP_IADD, r_m,
                               uop_int_binary(UOP_IMUL, r_k, k16));
    // B non-trans: addr = k*N + n
    Term aB   = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_k, k16),
                               r_n);
    Term ldA  = uop_index_e(At, aA);
    Term ldB  = uop_index_e(Bi, aB);
    Term mlt  = uop_binary(UOP_MUL, ldA, ldB);
    Term rdt  = uop_reduce(REDUCE_SUM, 212, mlt);
    Term aC   = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_m, k16),
                               r_n);
    Term stT  = uop_store(Ci, aC, rdt);
    UopDagGemmShape g = {0};
    CHECK(uop_dag_classify_matmul_shape(stT, &ke, &g));
    CHECK_EQ(g.M, 16u); CHECK_EQ(g.N, 16u); CHECK_EQ(g.K, 32u);
    CHECK_EQ(g.ldA, 16u);    // M
    CHECK_EQ(g.ldB, 16u);    // N (non-trans B)
    CHECK_EQ(g.flags, 1u);   // bit 0 = transA
  }

  TEST_BEGIN("recognise-tc/dag-classify-matmul-no-input-views-trans-B");
  {
    // B transposed: addr = k + n*K, ldB=K.
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts[2] = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts; ke.input_views = NULL;

    u32 dimsAi[2] = {16, 32};
    u32 dimsBt[2] = {16, 32};
    u32 dimsCi[2] = {16, 16};
    Term Ai = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsAi, 1);
    Term Bt = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsBt, 2);
    Term Ci = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsCi, 0);
    Term r_m = uop_range(220, 0, 16);
    Term r_n = uop_range(221, 0, 16);
    Term r_k = uop_range(222, 1, 32);
    Term k16 = uop_const(DT_INT32, 16);
    Term k32 = uop_const(DT_INT32, 32);
    // A non-trans: addr = m*K + k
    Term aA  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, r_m, k32),
                              r_k);
    // B transposed: addr = k + n*K
    Term aB  = uop_int_binary(UOP_IADD, r_k,
                              uop_int_binary(UOP_IMUL, r_n, k32));
    Term ldA = uop_index_e(Ai, aA);
    Term ldB = uop_index_e(Bt, aB);
    Term mlt = uop_binary(UOP_MUL, ldA, ldB);
    Term rdt = uop_reduce(REDUCE_SUM, 222, mlt);
    Term aC  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, r_m, k16),
                              r_n);
    Term stT = uop_store(Ci, aC, rdt);
    UopDagGemmShape g = {0};
    CHECK(uop_dag_classify_matmul_shape(stT, &ke, &g));
    CHECK_EQ(g.M, 16u); CHECK_EQ(g.K, 32u);
    CHECK_EQ(g.ldA, 32u);    // K (non-trans A)
    CHECK_EQ(g.ldB, 32u);    // K (trans B)
    CHECK_EQ(g.flags, 2u);   // bit 1 = transB
  }

  TEST_BEGIN("recognise-tc/dag-classify-matmul-no-input-views-trans-AB");
  {
    // Both A and B transposed.
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts[2] = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts; ke.input_views = NULL;

    u32 dimsAt[2] = {32, 16};   // storage [K,M]
    u32 dimsBt[2] = {16, 32};   // storage [N,K]
    u32 dimsCi[2] = {16, 16};
    Term At = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsAt, 1);
    Term Bt = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsBt, 2);
    Term Ci = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsCi, 0);
    Term r_m = uop_range(230, 0, 16);
    Term r_n = uop_range(231, 0, 16);
    Term r_k = uop_range(232, 1, 32);
    Term k16 = uop_const(DT_INT32, 16);
    Term k32 = uop_const(DT_INT32, 32);
    Term aA  = uop_int_binary(UOP_IADD, r_m,
                              uop_int_binary(UOP_IMUL, r_k, k16));
    Term aB  = uop_int_binary(UOP_IADD, r_k,
                              uop_int_binary(UOP_IMUL, r_n, k32));
    Term ldA = uop_index_e(At, aA);
    Term ldB = uop_index_e(Bt, aB);
    Term ml  = uop_binary(UOP_MUL, ldA, ldB);
    Term rd  = uop_reduce(REDUCE_SUM, 232, ml);
    Term aC  = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, r_m, k16),
                              r_n);
    Term st  = uop_store(Ci, aC, rd);
    UopDagGemmShape g = {0};
    CHECK(uop_dag_classify_matmul_shape(st, &ke, &g));
    CHECK_EQ(g.ldA, 16u);    // M
    CHECK_EQ(g.ldB, 32u);    // K
    CHECK_EQ(g.flags, 3u);   // both bits set
  }

  TEST_BEGIN("recognise-tc/dag-classify-gemv-no-input-views");
  {
    enum { GM3 = 16, GK3 = 24 };
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts3v[2] = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts3v;
    ke.input_views  = NULL;          // <-- decoupling proof

    u32 dimsW[2]  = {GM3, GK3};
    u32 dimsX[1]  = {GK3};
    u32 dimsCo[1] = {GM3};
    Term Wi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsW, 1);
    Term Xi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsX, 2);
    Term Coi= uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCo,0);
    Term r_mg = uop_range(300, 0,            GM3);
    Term r_kg = uop_range(301, 1 /*REDUCE*/, GK3);
    Term kKg  = uop_const(DT_INT32, GK3);
    // W non-trans: m*K + k
    Term aW   = uop_int_binary(UOP_IADD,
                               uop_int_binary(UOP_IMUL, r_mg, kKg),
                               r_kg);
    Term ldW  = uop_index_e(Wi, aW);
    Term ldX  = uop_index_e(Xi, r_kg);
    Term mul  = uop_binary(UOP_MUL, ldW, ldX);
    Term red  = uop_reduce(REDUCE_SUM, 301, mul);
    Term st   = uop_store(Coi, r_mg, red);
    UopDagGemvShape g = {0};
    CHECK(uop_dag_classify_gemv_shape(st, &ke, &g));
    CHECK_EQ(g.M, (u32)GM3);
    CHECK_EQ(g.K, (u32)GK3);
    CHECK_EQ(g.ldW, (u32)GK3);
    CHECK_EQ(g.flags, 0u);
  }

  TEST_BEGIN("recognise-tc/dag-classify-gemv-no-input-views-trans-W");
  {
    enum { GM4 = 16, GK4 = 24 };
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts4v[2] = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts4v;
    ke.input_views  = NULL;

    u32 dimsWt[2] = {GK4, GM4};   // storage [K,M]
    u32 dimsX[1]  = {GK4};
    u32 dimsCo[1] = {GM4};
    Term Wt = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsWt, 1);
    Term Xi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsX, 2);
    Term Coi= uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCo,0);
    Term r_mg = uop_range(310, 0, GM4);
    Term r_kg = uop_range(311, 1, GK4);
    Term kMg  = uop_const(DT_INT32, GM4);
    // W transposed: addr = m + k*M
    Term aW   = uop_int_binary(UOP_IADD, r_mg,
                               uop_int_binary(UOP_IMUL, r_kg, kMg));
    Term ldW  = uop_index_e(Wt, aW);
    Term ldX  = uop_index_e(Xi, r_kg);
    Term mul  = uop_binary(UOP_MUL, ldW, ldX);
    Term red  = uop_reduce(REDUCE_SUM, 311, mul);
    Term st   = uop_store(Coi, r_mg, red);
    UopDagGemvShape g = {0};
    CHECK(uop_dag_classify_gemv_shape(st, &ke, &g));
    CHECK_EQ(g.M, (u32)GM4);
    CHECK_EQ(g.K, (u32)GK4);
    CHECK_EQ(g.ldW, (u32)GM4);
    CHECK_EQ(g.flags, 1u);   // transW
  }

  TEST_BEGIN("recognise-tc/dag-classify-dot-no-input-views");
  {
    KernelEntry ke = {0};
    ke.n_inputs = 2;
    ke.output_dtype = DT_FP32;
    static u32 dts2v[2] = {DT_FP32, DT_FP32};
    ke.input_dtypes = dts2v;
    ke.input_views  = NULL;          // <-- decoupling proof

    u32 dimsAv[1] = {64};
    u32 dimsBv[1] = {64};
    u32 dimsCv[1] = {1};
    Term Ai = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsAv, 1);
    Term Bi = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsBv, 2);
    Term Co = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dimsCv, 0);
    Term r_kd = uop_range(400, 1, 64);
    Term ldA  = uop_index_e(Ai, r_kd);
    Term ldB  = uop_index_e(Bi, r_kd);
    Term mul  = uop_binary(UOP_MUL, ldA, ldB);
    Term red  = uop_reduce(REDUCE_SUM, 400, mul);
    Term zero = uop_const(DT_INT32, 0);
    Term st   = uop_store(Co, zero, red);
    UopDagDotShape g = {0};
    CHECK(uop_dag_classify_dot_shape(st, &ke, &g));
    CHECK_EQ(g.K, 64u);
    CHECK_EQ(g.a_input, 0u);
    CHECK_EQ(g.b_input, 1u);
  }

  thvm_free();
  TEST_REPORT();
}
