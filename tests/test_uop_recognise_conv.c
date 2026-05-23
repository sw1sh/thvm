// test_uop_recognise_conv.c - F4: pre-render pass that wraps the
// conv2d_flat shape with UOP_OPT(_, CONV, 0). Verifies positive
// recognition (conv-shaped DAG -> wrapped) and negative gates
// (matmul -> unchanged, MAX-reduce -> unchanged, non-conv shapes
// without IDIV/IMOD addresses -> unchanged).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // ---- Build the canonical conv2d_flat shape mirroring
  // kernel_lift_from_conv2d's emit:
  //   STORE(C, r_out,
  //     REDUCE(MUL(W[w_off + (r_out / patches) * w_s0 + r_q * w_s1],
  //                X[x_off + (r_out / patches) * x_sb
  //                       + (r_q / kw) / kh * x_s2
  //                       + ((r_out / w_out) + (r_q / kw) % kh) * x_s0
  //                       + ...]),
  //            SUM, q))
  // The decisive structural feature: IDIV/IMOD nodes in either
  // INDEX_E address tree.
  //
  // Using realistic-ish dims: c_out=16, patches=64 (so r_out 0..1023),
  //                           KRED = c_in*kh*kw = 1*3*3 = 9.
  u32 dims_w[2] = { 16, 9 };
  u32 dims_x[1] = { 1024 };
  u32 dims_c[1] = { 16 * 64 };
  Term W = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w);
  Term X = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x);
  Term C = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_c);

  Term r_out = uop_range(0, 0 /*LOOP*/,   16 * 64);
  Term r_q   = uop_range(1, 1 /*REDUCE*/, 9);

  Term k_pat = uop_const(DT_INT32, 64);
  Term k_w_s0 = uop_const(DT_INT32, 9);  // weight stride along c_out
  Term k_w_s1 = uop_const(DT_INT32, 1);  // weight stride along q
  Term k_x_s0 = uop_const(DT_INT32, 8);
  Term k_x_s1 = uop_const(DT_INT32, 1);
  Term k_kw  = uop_const(DT_INT32, 3);
  Term k_kh  = uop_const(DT_INT32, 3);

  // co  = r_out / patches (IDIV -- this is the conv signature)
  Term co     = uop_int_binary(UOP_IDIV, r_out, k_pat);
  // wi  = co * w_s0 + r_q * w_s1
  Term wi_co  = uop_int_binary(UOP_IMUL, co, k_w_s0);
  Term wi_q   = uop_int_binary(UOP_IMUL, r_q, k_w_s1);
  Term wi     = uop_int_binary(UOP_IADD, wi_co, wi_q);
  Term ldW    = uop_index_e(W, wi);

  // X address: also IDIV-laden -- decompose r_q into kh_v / kw_v.
  Term kw_v   = uop_int_binary(UOP_IMOD, r_q, k_kw);
  Term qk     = uop_int_binary(UOP_IDIV, r_q, k_kw);
  Term kh_v   = uop_int_binary(UOP_IMOD, qk, k_kh);
  // xi  = co * x_s0 + (kh_v + kw_v) * x_s1   (toy address; only the
  //                                            IDIV/IMOD presence matters)
  Term ohkh   = uop_int_binary(UOP_IADD, kh_v, kw_v);
  Term xi_a   = uop_int_binary(UOP_IMUL, co, k_x_s0);
  Term xi_b   = uop_int_binary(UOP_IMUL, ohkh, k_x_s1);
  Term xi     = uop_int_binary(UOP_IADD, xi_a, xi_b);
  Term ldX    = uop_index_e(X, xi);

  Term mul    = uop_binary(UOP_MUL, ldW, ldX);
  Term red    = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);
  Term store  = uop_store(C, r_out, red);

  TEST_BEGIN("recognise-conv/conv-shape-wraps-with-opt-conv");
  Term wrapped = uop_recognise_conv(store);
  CHECK(wrapped != 0);
  CHECK(wrapped != store);
  CHECK_EQ(term_tag(wrapped), TAG_UOP);
  CHECK_EQ(term_ext(wrapped), UOP_STORE);
  // STORE.value should now be UOP_OPT(red, CONV, 0).
  Term wval = heap_read(term_val(wrapped) + 2);
  CHECK_EQ(term_ext(wval), UOP_OPT);
  CHECK_EQ(uop_opt_target(wval), red);
  CHECK_EQ(uop_opt_kind  (wval), UOP_OPT_CONV);
  CHECK_EQ(uop_opt_factor(wval), 0u);

  TEST_BEGIN("recognise-conv/idempotent-on-already-wrapped");
  Term wrapped2 = uop_recognise_conv(wrapped);
  // STORE.value is already UOP_OPT (not UOP_REDUCE), so the inner
  // pattern doesn't match and we get the input back unchanged.
  CHECK_EQ(wrapped2, wrapped);

  TEST_BEGIN("recognise-conv/classify-reports-kred");
  // Classify the conv shape: KRED = 9 (the r_q extent).
  u32 kred = 0;
  CHECK(uop_classify_conv2d(store, &kred));
  CHECK_EQ(kred, 9u);

  TEST_BEGIN("recognise-conv/multi-input-iwhere-rhs-matches");
  // Multi-input conv produces MUL(INDEX_E(W), IWHERE(cond, INDEX_E(X0,
  // xi_pi0), IWHERE(cond, INDEX_E(X1, xi_pi1), default))) where each
  // xi_pi has an IDIV/IMOD-laden address. The recogniser walks the
  // IWHERE chain to find the conv-shaped INDEX_E.
  u32 dims_x0[1] = { 1024 };
  u32 dims_x1[1] = { 1024 };
  Term X0 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x0, 7);
  Term X1 = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x1, 8);
  // Both pi addresses include IDIV (decomposed r_out / patches).
  Term xi_pi0  = uop_int_binary(UOP_IDIV, r_out, k_pat);
  Term ld_pi0  = uop_index_e(X0, xi_pi0);
  Term xi_pi1  = uop_int_binary(UOP_IMOD, r_out, k_pat);
  Term ld_pi1  = uop_index_e(X1, xi_pi1);
  Term k_one   = uop_const(DT_INT32, 1);
  Term cond    = uop_int_binary(UOP_ILT, r_q, k_one);
  Term def_v   = uop_const(DT_FP32, 0);
  Term iw_2    = uop_iwhere(cond, ld_pi1, def_v);
  Term iw_chain= uop_iwhere(cond, ld_pi0, iw_2);
  Term mul_mi  = uop_binary(UOP_MUL, ldW, iw_chain);
  Term red_mi  = uop_reduce(REDUCE_SUM, /*axis=*/1, mul_mi);
  Term store_mi= uop_store(C, r_out, red_mi);
  Term wrap_mi = uop_recognise_conv(store_mi);
  CHECK(wrap_mi != store_mi);
  Term wval_mi = heap_read(term_val(wrap_mi) + 2);
  CHECK_EQ(term_ext(wval_mi), UOP_OPT);
  CHECK_EQ(uop_opt_kind(wval_mi), UOP_OPT_CONV);

  TEST_BEGIN("recognise-conv/matmul-shape-rejects");
  // A clean matmul has no IDIV/IMOD in its addresses.  Build the
  // canonical 16x32 @ 32x16 matmul -- recogniser must NOT wrap.
  u32 dimsA[2] = { 16, 32 };
  u32 dimsB[2] = { 32, 16 };
  u32 dimsC[2] = { 16, 16 };
  Term A = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsA);
  Term B = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsB);
  Term Cm = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dimsC);
  Term r_m = uop_range(2, 0,            16);
  Term r_n = uop_range(3, 0,            16);
  Term r_k = uop_range(4, 1 /*REDUCE*/, 32);
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
  Term mul_mm = uop_binary(UOP_MUL, ldA, ldB);
  Term red_mm = uop_reduce(REDUCE_SUM, /*axis=*/4, mul_mm);
  Term mN     = uop_int_binary(UOP_IMUL, r_m, k16);
  Term addrC  = uop_int_binary(UOP_IADD, mN, r_n);
  Term store_mm = uop_store(Cm, addrC, red_mm);
  Term out_mm = uop_recognise_conv(store_mm);
  CHECK_EQ(out_mm, store_mm);
  u32 mm_kred = 99;
  CHECK(!uop_classify_conv2d(store_mm, &mm_kred));
  CHECK_EQ(mm_kred, 0u);

  TEST_BEGIN("recognise-conv/max-reduce-rejects");
  // REDUCE_MAX is not conv (conv uses SUM).  The recogniser gates
  // accordingly.
  Term red_max = uop_reduce(REDUCE_MAX, /*axis=*/1, mul);
  Term store_max = uop_store(C, r_out, red_max);
  Term out_max = uop_recognise_conv(store_max);
  CHECK_EQ(out_max, store_max);

  TEST_BEGIN("recognise-conv/non-store-passes-through");
  // Non-STORE roots pass through unchanged.
  CHECK_EQ(uop_recognise_conv(red), red);
  CHECK_EQ(uop_recognise_conv(W),   W);

  TEST_BEGIN("recognise-conv/non-mul-reduce-rejects");
  // STORE of REDUCE(non-MUL) -- e.g. REDUCE(INDEX_E) for a row-sum
  // reduction.  Recogniser only matches REDUCE(MUL(...), ...).
  Term ld_only = uop_index_e(W, wi);
  Term red_sum = uop_reduce(REDUCE_SUM, /*axis=*/1, ld_only);
  Term store_rs = uop_store(C, r_out, red_sum);
  CHECK_EQ(uop_recognise_conv(store_rs), store_rs);

  TEST_BEGIN("recognise-conv/non-conv-mul-reduce-rejects");
  // STORE of REDUCE(MUL(INDEX_E, INDEX_E)) where neither address has
  // IDIV/IMOD -- this is matmul-shaped, so CONV recogniser must NOT
  // wrap it (TC recogniser handles matmul).
  Term out_clean = uop_recognise_conv(store_mm);
  CHECK_EQ(out_clean, store_mm);

  TEST_BEGIN("recognise-conv/classify-on-non-conv-zeroes-kred");
  // Non-conv stores: classify returns 0, *out_kred stays 0.
  u32 nm_kred = 99;
  CHECK(!uop_classify_conv2d(store_max, &nm_kred));
  CHECK_EQ(nm_kred, 0u);
  u32 rs_kred = 99;
  CHECK(!uop_classify_conv2d(store_rs, &rs_kred));
  CHECK_EQ(rs_kred, 0u);

  TEST_BEGIN("recognise-conv/conv-and-tc-coexist-on-distinct-roots");
  // Verify the two recognisers don't interfere: applying both to a
  // matmul root only fires TC; applying both to a conv root only
  // fires CONV.  This is the order they run in cg_emit_via_uop.
  Term mm_after_tc   = uop_recognise_tc(store_mm);
  Term mm_after_both = uop_recognise_conv(mm_after_tc);
  // mm_after_tc was wrapped with OPT(TC); CONV recogniser sees
  // OPT-not-REDUCE and returns input unchanged.
  CHECK_EQ(mm_after_both, mm_after_tc);
  Term mm_inner = heap_read(term_val(mm_after_both) + 2);
  CHECK_EQ(uop_opt_kind(mm_inner), UOP_OPT_TC);

  Term conv_after_tc   = uop_recognise_tc(store);
  // TC recogniser must NOT wrap conv (its >2-distinct-ranges check
  // fires for the IDIV-decomposed address tree, OR -- if both sides
  // share r_out/r_q -- the same-buffer / classification rejects
  // because conv W != X but the address shape is structurally
  // distinct from matmul `m*K+k` / `k*N+n`).  Either way the conv
  // shape ends up wrapped only by CONV.
  Term conv_after_both = uop_recognise_conv(conv_after_tc);
  CHECK(conv_after_both != store);
  Term conv_inner = heap_read(term_val(conv_after_both) + 2);
  CHECK_EQ(uop_opt_kind(conv_inner), UOP_OPT_CONV);

  // === full-shape extractor ============================================
  // Build a representative single-input non-degenerate conv2d address
  // tree (mirrors kernel_lift_from_conv2d) and verify the extractor
  // recovers every shape field.  Two cases: batch=1 stride1, and
  // batched stride1.  Also check the degenerate kh=kw=1 case
  // intentionally bails so callers can fall back to input_views.

  // Local helper to build the ldW + ldX terms for a given conv shape,
  // returning the W and X INDEX_E terms via out params.
  // (Inline in main to keep the test file self-contained.)
  // Case A: c_out=8 c_in=2 kh=kw=2 batch=1 h_out=w_out=10
  //         w_off=0 w_s0=8 w_s1=1; x_off=0 x_sb=0 x_s0=11 x_s1=1 x_s2=121
  TEST_BEGIN("conv2d-extract/single-input-batch1-stride1");
  {
    u32 c_out_a = 8, c_in_a = 2, kh_a = 2, kw_a = 2;
    u32 batch_a = 1, h_out_a = 10, w_out_a = 10;
    u32 spatial_a = h_out_a * w_out_a;
    u32 patches_a = batch_a * spatial_a;
    u32 KRED_a    = c_in_a * kh_a * kw_a;
    u32 dims_w_a[2] = { c_out_a, KRED_a };
    u32 dims_x_a[1] = { 4096 };
    Term Wa = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w_a, 1);
    Term Xa = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x_a, 2);
    Term r_out_a = uop_range(0, 0, c_out_a * patches_a);
    Term r_q_a   = uop_range(1, 1, KRED_a);
    Term k_pat_a = uop_const(DT_INT32, patches_a);
    Term k_sp_a  = uop_const(DT_INT32, spatial_a);
    Term k_wo_a  = uop_const(DT_INT32, w_out_a);
    Term co_a    = uop_int_binary(UOP_IDIV, r_out_a, k_pat_a);
    Term patch_a = uop_int_binary(UOP_IMOD, r_out_a, k_pat_a);
    Term bi_a    = uop_int_binary(UOP_IDIV, patch_a, k_sp_a);
    Term sp_a    = uop_int_binary(UOP_IMOD, patch_a, k_sp_a);
    Term ow_a    = uop_int_binary(UOP_IMOD, sp_a, k_wo_a);
    Term oh_a    = uop_int_binary(UOP_IDIV, sp_a, k_wo_a);
    Term k_kw_a  = uop_const(DT_INT32, kw_a);
    Term k_kh_a  = uop_const(DT_INT32, kh_a);
    Term kw_v_a  = uop_int_binary(UOP_IMOD, r_q_a, k_kw_a);
    Term qk_a    = uop_int_binary(UOP_IDIV, r_q_a, k_kw_a);
    Term kh_v_a  = uop_int_binary(UOP_IMOD, qk_a, k_kh_a);
    Term ci_a    = uop_int_binary(UOP_IDIV, qk_a, k_kh_a);
    // wi
    Term k_w_off_a = uop_const(DT_INT32, 0);
    Term k_ws0_a   = uop_const(DT_INT32, 8);
    Term k_ws1_a   = uop_const(DT_INT32, 1);
    Term wi_co_a   = uop_int_binary(UOP_IMUL, co_a, k_ws0_a);
    Term wi_q_a    = uop_int_binary(UOP_IMUL, r_q_a, k_ws1_a);
    Term wi_sum_a  = uop_int_binary(UOP_IADD, wi_co_a, wi_q_a);
    Term wi_a      = uop_int_binary(UOP_IADD, k_w_off_a, wi_sum_a);
    // xi
    Term k_x_off_a = uop_const(DT_INT32, 0);
    Term k_xsb_a   = uop_const(DT_INT32, 0);
    Term k_xs0_a   = uop_const(DT_INT32, 11);
    Term k_xs1_a   = uop_const(DT_INT32, 1);
    Term k_xs2_a   = uop_const(DT_INT32, 121);
    Term xi_b_a    = uop_int_binary(UOP_IMUL, bi_a, k_xsb_a);
    Term xi_ci_a   = uop_int_binary(UOP_IMUL, ci_a, k_xs2_a);
    Term oh_kh_a   = uop_int_binary(UOP_IADD, oh_a, kh_v_a);
    Term xi_h_a    = uop_int_binary(UOP_IMUL, oh_kh_a, k_xs0_a);
    Term ow_kw_a   = uop_int_binary(UOP_IADD, ow_a, kw_v_a);
    Term xi_w_a    = uop_int_binary(UOP_IMUL, ow_kw_a, k_xs1_a);
    Term xi_sum1_a = uop_int_binary(UOP_IADD, xi_b_a, xi_ci_a);
    Term xi_sum2_a = uop_int_binary(UOP_IADD, xi_h_a, xi_w_a);
    Term xi_sum_a  = uop_int_binary(UOP_IADD, xi_sum1_a, xi_sum2_a);
    Term xi_a      = uop_int_binary(UOP_IADD, k_x_off_a, xi_sum_a);
    Term ldW_a = uop_index_e(Wa, wi_a);
    Term ldX_a = uop_index_e(Xa, xi_a);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(
        heap_read(term_val(ldW_a) + 1),
        heap_read(term_val(ldX_a) + 1),
        &s);
    CHECK(ok);
    CHECK_EQ(s.c_out,    c_out_a);
    CHECK_EQ(s.c_in,     c_in_a);
    CHECK_EQ(s.kh,       kh_a);
    CHECK_EQ(s.kw,       kw_a);
    CHECK_EQ(s.h_out,    h_out_a);
    CHECK_EQ(s.w_out,    w_out_a);
    CHECK_EQ(s.batch,    batch_a);
    CHECK_EQ(s.patches,  patches_a);
    CHECK_EQ(s.spatial_patches, spatial_a);
    CHECK_EQ(s.w_offset,  0);
    CHECK_EQ(s.w_stride0, 8);
    CHECK_EQ(s.w_stride1, 1);
    CHECK_EQ(s.x_offset,  0);
    CHECK_EQ(s.x_stride_b, 0);
    CHECK_EQ(s.x_stride0,  11);
    CHECK_EQ(s.x_stride1,  1);
    CHECK_EQ(s.x_stride2,  121);
    CHECK_EQ(s.axis_r_out, 0u);
    CHECK_EQ(s.axis_r_q,   1u);
  }

  TEST_BEGIN("conv2d-extract/single-input-batched");
  {
    u32 c_out_b = 16, c_in_b = 4, kh_b = 3, kw_b = 3;
    u32 batch_b = 2, h_out_b = 12, w_out_b = 12;
    u32 spatial_b = h_out_b * w_out_b;
    u32 patches_b = batch_b * spatial_b;
    u32 KRED_b    = c_in_b * kh_b * kw_b;
    u32 dims_w_b[2] = { c_out_b, KRED_b };
    u32 dims_x_b[1] = { 8192 };
    Term Wb = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w_b, 1);
    Term Xb = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x_b, 2);
    Term r_out_b = uop_range(0, 0, c_out_b * patches_b);
    Term r_q_b   = uop_range(1, 1, KRED_b);
    Term k_pat_b = uop_const(DT_INT32, patches_b);
    Term k_sp_b  = uop_const(DT_INT32, spatial_b);
    Term k_wo_b  = uop_const(DT_INT32, w_out_b);
    Term co_b    = uop_int_binary(UOP_IDIV, r_out_b, k_pat_b);
    Term patch_b = uop_int_binary(UOP_IMOD, r_out_b, k_pat_b);
    Term bi_b    = uop_int_binary(UOP_IDIV, patch_b, k_sp_b);
    Term sp_b    = uop_int_binary(UOP_IMOD, patch_b, k_sp_b);
    Term ow_b    = uop_int_binary(UOP_IMOD, sp_b, k_wo_b);
    Term oh_b    = uop_int_binary(UOP_IDIV, sp_b, k_wo_b);
    Term k_kw_b  = uop_const(DT_INT32, kw_b);
    Term k_kh_b  = uop_const(DT_INT32, kh_b);
    Term kw_v_b  = uop_int_binary(UOP_IMOD, r_q_b, k_kw_b);
    Term qk_b    = uop_int_binary(UOP_IDIV, r_q_b, k_kw_b);
    Term kh_v_b  = uop_int_binary(UOP_IMOD, qk_b, k_kh_b);
    Term ci_b    = uop_int_binary(UOP_IDIV, qk_b, k_kh_b);
    Term k_w_off_b = uop_const(DT_INT32, 0);
    Term k_ws0_b   = uop_const(DT_INT32, KRED_b);
    Term k_ws1_b   = uop_const(DT_INT32, 1);
    Term wi_co_b   = uop_int_binary(UOP_IMUL, co_b, k_ws0_b);
    Term wi_q_b    = uop_int_binary(UOP_IMUL, r_q_b, k_ws1_b);
    Term wi_sum_b  = uop_int_binary(UOP_IADD, wi_co_b, wi_q_b);
    Term wi_b      = uop_int_binary(UOP_IADD, k_w_off_b, wi_sum_b);
    Term k_x_off_b = uop_const(DT_INT32, 0);
    Term k_xsb_b   = uop_const(DT_INT32, c_in_b * 14 * 14);
    Term k_xs0_b   = uop_const(DT_INT32, 14);
    Term k_xs1_b   = uop_const(DT_INT32, 1);
    Term k_xs2_b   = uop_const(DT_INT32, 14 * 14);
    Term xi_b_b    = uop_int_binary(UOP_IMUL, bi_b, k_xsb_b);
    Term xi_ci_b   = uop_int_binary(UOP_IMUL, ci_b, k_xs2_b);
    Term oh_kh_b   = uop_int_binary(UOP_IADD, oh_b, kh_v_b);
    Term xi_h_b    = uop_int_binary(UOP_IMUL, oh_kh_b, k_xs0_b);
    Term ow_kw_b   = uop_int_binary(UOP_IADD, ow_b, kw_v_b);
    Term xi_w_b    = uop_int_binary(UOP_IMUL, ow_kw_b, k_xs1_b);
    Term xi_sum1_b = uop_int_binary(UOP_IADD, xi_b_b, xi_ci_b);
    Term xi_sum2_b = uop_int_binary(UOP_IADD, xi_h_b, xi_w_b);
    Term xi_sum_b  = uop_int_binary(UOP_IADD, xi_sum1_b, xi_sum2_b);
    Term xi_b_top  = uop_int_binary(UOP_IADD, k_x_off_b, xi_sum_b);
    Term ldW_b = uop_index_e(Wb, wi_b);
    Term ldX_b = uop_index_e(Xb, xi_b_top);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(
        heap_read(term_val(ldW_b) + 1),
        heap_read(term_val(ldX_b) + 1),
        &s);
    CHECK(ok);
    CHECK_EQ(s.c_out, c_out_b);
    CHECK_EQ(s.c_in,  c_in_b);
    CHECK_EQ(s.kh,    kh_b);
    CHECK_EQ(s.kw,    kw_b);
    CHECK_EQ(s.batch, batch_b);
    CHECK_EQ(s.h_out, h_out_b);
    CHECK_EQ(s.w_out, w_out_b);
    CHECK_EQ(s.x_stride_b, (i32)(c_in_b * 14 * 14));
    CHECK_EQ(s.x_stride2,  (i32)(14 * 14));
  }

  TEST_BEGIN("conv2d-extract/nonzero-offsets-recovered");
  {
    u32 c_out_c = 8, c_in_c = 4, kh_c = 3, kw_c = 3;
    u32 batch_c = 1, h_out_c = 8, w_out_c = 8;
    u32 spatial_c = h_out_c * w_out_c;
    u32 patches_c = batch_c * spatial_c;
    u32 KRED_c    = c_in_c * kh_c * kw_c;
    u32 dims_w_c[2] = { c_out_c, KRED_c };
    u32 dims_x_c[1] = { 4096 };
    Term Wc = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w_c, 1);
    Term Xc = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x_c, 2);
    Term r_out_c = uop_range(0, 0, c_out_c * patches_c);
    Term r_q_c   = uop_range(1, 1, KRED_c);
    Term k_pat_c = uop_const(DT_INT32, patches_c);
    Term k_sp_c  = uop_const(DT_INT32, spatial_c);
    Term k_wo_c  = uop_const(DT_INT32, w_out_c);
    Term co_c    = uop_int_binary(UOP_IDIV, r_out_c, k_pat_c);
    Term patch_c = uop_int_binary(UOP_IMOD, r_out_c, k_pat_c);
    Term sp_c    = uop_int_binary(UOP_IMOD, patch_c, k_sp_c);
    Term ow_c    = uop_int_binary(UOP_IMOD, sp_c, k_wo_c);
    Term oh_c    = uop_int_binary(UOP_IDIV, sp_c, k_wo_c);
    Term k_kw_c  = uop_const(DT_INT32, kw_c);
    Term k_kh_c  = uop_const(DT_INT32, kh_c);
    Term kw_v_c  = uop_int_binary(UOP_IMOD, r_q_c, k_kw_c);
    Term qk_c    = uop_int_binary(UOP_IDIV, r_q_c, k_kw_c);
    Term kh_v_c  = uop_int_binary(UOP_IMOD, qk_c, k_kh_c);
    Term ci_c    = uop_int_binary(UOP_IDIV, qk_c, k_kh_c);
    // Non-zero offsets:
    Term k_w_off_c = uop_const(DT_INT32, 100);
    Term k_x_off_c = uop_const(DT_INT32, 256);
    Term k_ws0_c   = uop_const(DT_INT32, KRED_c);
    Term k_ws1_c   = uop_const(DT_INT32, 1);
    Term k_xs0_c   = uop_const(DT_INT32, 10);
    Term k_xs1_c   = uop_const(DT_INT32, 1);
    Term k_xs2_c   = uop_const(DT_INT32, 100);
    Term wi_co_c   = uop_int_binary(UOP_IMUL, co_c, k_ws0_c);
    Term wi_q_c    = uop_int_binary(UOP_IMUL, r_q_c, k_ws1_c);
    Term wi_sum_c  = uop_int_binary(UOP_IADD, wi_co_c, wi_q_c);
    Term wi_c      = uop_int_binary(UOP_IADD, k_w_off_c, wi_sum_c);
    Term xi_ci_c   = uop_int_binary(UOP_IMUL, ci_c, k_xs2_c);
    Term oh_kh_c   = uop_int_binary(UOP_IADD, oh_c, kh_v_c);
    Term xi_h_c    = uop_int_binary(UOP_IMUL, oh_kh_c, k_xs0_c);
    Term ow_kw_c   = uop_int_binary(UOP_IADD, ow_c, kw_v_c);
    Term xi_w_c    = uop_int_binary(UOP_IMUL, ow_kw_c, k_xs1_c);
    Term xi_sum2_c = uop_int_binary(UOP_IADD, xi_h_c, xi_w_c);
    Term xi_sum_c  = uop_int_binary(UOP_IADD, xi_ci_c, xi_sum2_c);
    Term xi_c      = uop_int_binary(UOP_IADD, k_x_off_c, xi_sum_c);
    Term ldW_c = uop_index_e(Wc, wi_c);
    Term ldX_c = uop_index_e(Xc, xi_c);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(
        heap_read(term_val(ldW_c) + 1),
        heap_read(term_val(ldX_c) + 1),
        &s);
    CHECK(ok);
    CHECK_EQ(s.w_offset, 100);
    CHECK_EQ(s.x_offset, 256);
    CHECK_EQ(s.w_stride0, (i32)KRED_c);
    CHECK_EQ(s.x_stride2, 100);
  }

  TEST_BEGIN("conv2d-extract/1x1-cin1");
  {
    // c_in=1, kh=kw=1 => simplifier collapses ci/kh_v/kw_v entirely.
    // The dedicated 1x1 parser handles this end-to-end via divisor-
    // value disambiguation (the H arm has the same UOp tree shape as a
    // BI arm, but their divisors differ).
    u32 c_out_d = 4, c_in_d = 1, kh_d = 1, kw_d = 1;
    u32 batch_d = 1, h_out_d = 8, w_out_d = 8;
    u32 spatial_d = h_out_d * w_out_d;
    u32 patches_d = batch_d * spatial_d;
    u32 KRED_d    = c_in_d * kh_d * kw_d;
    u32 dims_w_d[2] = { c_out_d, KRED_d };
    u32 dims_x_d[1] = { 1024 };
    Term Wd = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w_d, 1);
    Term Xd = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x_d, 2);
    Term r_out_d = uop_range(0, 0, c_out_d * patches_d);
    Term r_q_d   = uop_range(1, 1, KRED_d);
    Term k_pat_d = uop_const(DT_INT32, patches_d);
    Term k_sp_d  = uop_const(DT_INT32, spatial_d);
    Term k_wo_d  = uop_const(DT_INT32, w_out_d);
    Term co_d    = uop_int_binary(UOP_IDIV, r_out_d, k_pat_d);
    Term patch_d = uop_int_binary(UOP_IMOD, r_out_d, k_pat_d);
    Term sp_d    = uop_int_binary(UOP_IMOD, patch_d, k_sp_d);
    Term ow_d    = uop_int_binary(UOP_IMOD, sp_d, k_wo_d);
    Term oh_d    = uop_int_binary(UOP_IDIV, sp_d, k_wo_d);
    Term k_kw_d  = uop_const(DT_INT32, kw_d);
    Term k_kh_d  = uop_const(DT_INT32, kh_d);
    Term kw_v_d  = uop_int_binary(UOP_IMOD, r_q_d, k_kw_d);
    Term qk_d    = uop_int_binary(UOP_IDIV, r_q_d, k_kw_d);
    Term kh_v_d  = uop_int_binary(UOP_IMOD, qk_d, k_kh_d);
    Term ci_d    = uop_int_binary(UOP_IDIV, qk_d, k_kh_d);
    Term k_ws0_d = uop_const(DT_INT32, 1);
    Term k_xs0_d = uop_const(DT_INT32, 8);
    Term k_xs1_d = uop_const(DT_INT32, 1);
    Term k_xs2_d = uop_const(DT_INT32, 64);
    Term wi_co_d = uop_int_binary(UOP_IMUL, co_d, k_ws0_d);
    Term wi_d    = uop_int_binary(UOP_IADD, wi_co_d, r_q_d);
    Term xi_ci_d = uop_int_binary(UOP_IMUL, ci_d, k_xs2_d);
    Term oh_kh_d = uop_int_binary(UOP_IADD, oh_d, kh_v_d);
    Term xi_h_d  = uop_int_binary(UOP_IMUL, oh_kh_d, k_xs0_d);
    Term ow_kw_d = uop_int_binary(UOP_IADD, ow_d, kw_v_d);
    Term xi_w_d  = uop_int_binary(UOP_IMUL, ow_kw_d, k_xs1_d);
    Term xi_sum_d = uop_int_binary(UOP_IADD, xi_h_d, xi_w_d);
    Term xi_d    = uop_int_binary(UOP_IADD, xi_ci_d, xi_sum_d);
    Term ldW_d = uop_index_e(Wd, wi_d);
    Term ldX_d = uop_index_e(Xd, xi_d);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(
        heap_read(term_val(ldW_d) + 1),
        heap_read(term_val(ldX_d) + 1),
        &s);
    CHECK(ok);
    CHECK_EQ(s.c_out,    c_out_d);
    CHECK_EQ(s.c_in,     c_in_d);
    CHECK_EQ(s.kh,       kh_d);
    CHECK_EQ(s.kw,       kw_d);
    CHECK_EQ(s.h_out,    h_out_d);
    CHECK_EQ(s.w_out,    w_out_d);
    CHECK_EQ(s.batch,    batch_d);
    CHECK_EQ(s.patches,  patches_d);
    CHECK_EQ(s.spatial_patches, spatial_d);
    CHECK_EQ(s.x_stride0,  8);
    CHECK_EQ(s.x_stride1,  1);
    CHECK_EQ(s.x_stride2,  64);
    CHECK_EQ(s.x_stride_b, 0);
  }

  TEST_BEGIN("conv2d-extract/1x1-batch1");
  {
    // 1x1 conv with c_in > 1, batch=1 -- the H arm is the only
    // IMOD-of-IDIV leaf (BI arm absent because batch==1).  CI arm has
    // x_s2 > 1 since c_in > 1.
    u32 c_out_e = 16, c_in_e = 4, kh_e = 1, kw_e = 1;
    u32 batch_e = 1, h_out_e = 8, w_out_e = 8;
    u32 spatial_e = h_out_e * w_out_e;
    u32 patches_e = batch_e * spatial_e;
    u32 KRED_e    = c_in_e * kh_e * kw_e;
    u32 dims_w_e[2] = { c_out_e, KRED_e };
    u32 dims_x_e[1] = { 4096 };
    Term We = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w_e, 1);
    Term Xe = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x_e, 2);
    Term r_out_e = uop_range(0, 0, c_out_e * patches_e);
    Term r_q_e   = uop_range(1, 1, KRED_e);
    Term k_pat_e = uop_const(DT_INT32, patches_e);
    Term k_sp_e  = uop_const(DT_INT32, spatial_e);
    Term k_wo_e  = uop_const(DT_INT32, w_out_e);
    Term co_e    = uop_int_binary(UOP_IDIV, r_out_e, k_pat_e);
    Term patch_e = uop_int_binary(UOP_IMOD, r_out_e, k_pat_e);
    Term sp_e    = uop_int_binary(UOP_IMOD, patch_e, k_sp_e);
    Term ow_e    = uop_int_binary(UOP_IMOD, sp_e, k_wo_e);
    Term oh_e    = uop_int_binary(UOP_IDIV, sp_e, k_wo_e);
    Term k_kw_e  = uop_const(DT_INT32, kw_e);
    Term k_kh_e  = uop_const(DT_INT32, kh_e);
    Term kw_v_e  = uop_int_binary(UOP_IMOD, r_q_e, k_kw_e);
    Term qk_e    = uop_int_binary(UOP_IDIV, r_q_e, k_kw_e);
    Term kh_v_e  = uop_int_binary(UOP_IMOD, qk_e, k_kh_e);
    Term ci_e    = uop_int_binary(UOP_IDIV, qk_e, k_kh_e);
    Term k_w_off_e = uop_const(DT_INT32, 0);
    Term k_ws0_e   = uop_const(DT_INT32, KRED_e);
    Term k_ws1_e   = uop_const(DT_INT32, 1);
    Term wi_co_e   = uop_int_binary(UOP_IMUL, co_e, k_ws0_e);
    Term wi_q_e    = uop_int_binary(UOP_IMUL, r_q_e, k_ws1_e);
    Term wi_sum_e  = uop_int_binary(UOP_IADD, wi_co_e, wi_q_e);
    Term wi_e      = uop_int_binary(UOP_IADD, k_w_off_e, wi_sum_e);
    // X strides: x_s0 = w_in (=w_out for stride=1, kh=1), x_s1 = 1,
    // x_s2 = h_in*w_in.  We pick concrete distinct values so the parser
    // is forced to recover them rather than guessing.
    Term k_x_off_e = uop_const(DT_INT32, 0);
    Term k_xsb_e   = uop_const(DT_INT32, 0);
    Term k_xs0_e   = uop_const(DT_INT32, 8);
    Term k_xs1_e   = uop_const(DT_INT32, 1);
    Term k_xs2_e   = uop_const(DT_INT32, 64);
    Term xi_b_e    = uop_int_binary(UOP_IMUL, /*bi=*/uop_int_binary(UOP_IDIV, patch_e, k_sp_e),
                                    k_xsb_e);
    Term xi_ci_e   = uop_int_binary(UOP_IMUL, ci_e, k_xs2_e);
    Term oh_kh_e   = uop_int_binary(UOP_IADD, oh_e, kh_v_e);
    Term xi_h_e    = uop_int_binary(UOP_IMUL, oh_kh_e, k_xs0_e);
    Term ow_kw_e   = uop_int_binary(UOP_IADD, ow_e, kw_v_e);
    Term xi_w_e    = uop_int_binary(UOP_IMUL, ow_kw_e, k_xs1_e);
    Term xi_sum1_e = uop_int_binary(UOP_IADD, xi_b_e, xi_ci_e);
    Term xi_sum2_e = uop_int_binary(UOP_IADD, xi_h_e, xi_w_e);
    Term xi_sum_e  = uop_int_binary(UOP_IADD, xi_sum1_e, xi_sum2_e);
    Term xi_e      = uop_int_binary(UOP_IADD, k_x_off_e, xi_sum_e);
    Term ldW_e = uop_index_e(We, wi_e);
    Term ldX_e = uop_index_e(Xe, xi_e);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(
        heap_read(term_val(ldW_e) + 1),
        heap_read(term_val(ldX_e) + 1),
        &s);
    CHECK(ok);
    CHECK_EQ(s.c_out,    c_out_e);
    CHECK_EQ(s.c_in,     c_in_e);
    CHECK_EQ(s.kh,       kh_e);
    CHECK_EQ(s.kw,       kw_e);
    CHECK_EQ(s.h_out,    h_out_e);
    CHECK_EQ(s.w_out,    w_out_e);
    CHECK_EQ(s.batch,    batch_e);
    CHECK_EQ(s.patches,  patches_e);
    CHECK_EQ(s.spatial_patches, spatial_e);
    CHECK_EQ(s.w_offset,  0);
    CHECK_EQ(s.w_stride0, (i32)KRED_e);
    CHECK_EQ(s.w_stride1, 1);
    CHECK_EQ(s.x_offset,  0);
    CHECK_EQ(s.x_stride_b, 0);
    CHECK_EQ(s.x_stride0,  8);
    CHECK_EQ(s.x_stride1,  1);
    CHECK_EQ(s.x_stride2,  64);
    CHECK_EQ(s.axis_r_out, 0u);
    CHECK_EQ(s.axis_r_q,   1u);
  }

  TEST_BEGIN("conv2d-extract/1x1-batched");
  {
    // 1x1 conv with batch=2 -- BI arm fires.  H arm and BI arm have
    // identical UOp tree shape (IMUL(IMOD(IDIV(r_out, X), Y), C)) but
    // different divisor values.  The parser disambiguates by trying
    // both pairings against patches.
    u32 c_out_f = 32, c_in_f = 8, kh_f = 1, kw_f = 1;
    u32 batch_f = 2, h_out_f = 4, w_out_f = 4;
    u32 spatial_f = h_out_f * w_out_f;       // 16
    u32 patches_f = batch_f * spatial_f;     // 32
    u32 KRED_f    = c_in_f * kh_f * kw_f;    // 8
    u32 dims_w_f[2] = { c_out_f, KRED_f };
    u32 dims_x_f[1] = { 8192 };
    Term Wf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_w_f, 1);
    Term Xf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_x_f, 2);
    Term r_out_f = uop_range(0, 0, c_out_f * patches_f);
    Term r_q_f   = uop_range(1, 1, KRED_f);
    Term k_pat_f = uop_const(DT_INT32, patches_f);
    Term k_sp_f  = uop_const(DT_INT32, spatial_f);
    Term k_wo_f  = uop_const(DT_INT32, w_out_f);
    Term co_f    = uop_int_binary(UOP_IDIV, r_out_f, k_pat_f);
    Term patch_f = uop_int_binary(UOP_IMOD, r_out_f, k_pat_f);
    Term bi_f    = uop_int_binary(UOP_IDIV, patch_f, k_sp_f);
    Term sp_f    = uop_int_binary(UOP_IMOD, patch_f, k_sp_f);
    Term ow_f    = uop_int_binary(UOP_IMOD, sp_f, k_wo_f);
    Term oh_f    = uop_int_binary(UOP_IDIV, sp_f, k_wo_f);
    Term k_kw_f  = uop_const(DT_INT32, kw_f);
    Term k_kh_f  = uop_const(DT_INT32, kh_f);
    Term kw_v_f  = uop_int_binary(UOP_IMOD, r_q_f, k_kw_f);
    Term qk_f    = uop_int_binary(UOP_IDIV, r_q_f, k_kw_f);
    Term kh_v_f  = uop_int_binary(UOP_IMOD, qk_f, k_kh_f);
    Term ci_f    = uop_int_binary(UOP_IDIV, qk_f, k_kh_f);
    Term k_w_off_f = uop_const(DT_INT32, 0);
    Term k_ws0_f   = uop_const(DT_INT32, KRED_f);
    Term k_ws1_f   = uop_const(DT_INT32, 1);
    Term wi_co_f   = uop_int_binary(UOP_IMUL, co_f, k_ws0_f);
    Term wi_q_f    = uop_int_binary(UOP_IMUL, r_q_f, k_ws1_f);
    Term wi_sum_f  = uop_int_binary(UOP_IADD, wi_co_f, wi_q_f);
    Term wi_f      = uop_int_binary(UOP_IADD, k_w_off_f, wi_sum_f);
    // x_sb = c_in * spatial = 8 * 16 = 128, x_s2 = 16.
    Term k_x_off_f = uop_const(DT_INT32, 0);
    Term k_xsb_f   = uop_const(DT_INT32, c_in_f * spatial_f);
    Term k_xs0_f   = uop_const(DT_INT32, w_out_f);
    Term k_xs1_f   = uop_const(DT_INT32, 1);
    Term k_xs2_f   = uop_const(DT_INT32, spatial_f);
    Term xi_b_f    = uop_int_binary(UOP_IMUL, bi_f, k_xsb_f);
    Term xi_ci_f   = uop_int_binary(UOP_IMUL, ci_f, k_xs2_f);
    Term oh_kh_f   = uop_int_binary(UOP_IADD, oh_f, kh_v_f);
    Term xi_h_f    = uop_int_binary(UOP_IMUL, oh_kh_f, k_xs0_f);
    Term ow_kw_f   = uop_int_binary(UOP_IADD, ow_f, kw_v_f);
    Term xi_w_f    = uop_int_binary(UOP_IMUL, ow_kw_f, k_xs1_f);
    Term xi_sum1_f = uop_int_binary(UOP_IADD, xi_b_f, xi_ci_f);
    Term xi_sum2_f = uop_int_binary(UOP_IADD, xi_h_f, xi_w_f);
    Term xi_sum_f  = uop_int_binary(UOP_IADD, xi_sum1_f, xi_sum2_f);
    Term xi_f      = uop_int_binary(UOP_IADD, k_x_off_f, xi_sum_f);
    Term ldW_f = uop_index_e(Wf, wi_f);
    Term ldX_f = uop_index_e(Xf, xi_f);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(
        heap_read(term_val(ldW_f) + 1),
        heap_read(term_val(ldX_f) + 1),
        &s);
    CHECK(ok);
    CHECK_EQ(s.c_out,    c_out_f);
    CHECK_EQ(s.c_in,     c_in_f);
    CHECK_EQ(s.kh,       kh_f);
    CHECK_EQ(s.kw,       kw_f);
    CHECK_EQ(s.h_out,    h_out_f);
    CHECK_EQ(s.w_out,    w_out_f);
    CHECK_EQ(s.batch,    batch_f);
    CHECK_EQ(s.patches,  patches_f);
    CHECK_EQ(s.spatial_patches, spatial_f);
    CHECK_EQ(s.x_stride_b, (i32)(c_in_f * spatial_f));
    CHECK_EQ(s.x_stride0,  (i32)w_out_f);
    CHECK_EQ(s.x_stride1,  1);
    CHECK_EQ(s.x_stride2,  (i32)spatial_f);
  }

  TEST_BEGIN("conv2d-extract/non-iadd-w-rejects");
  {
    // W addr not an IADD (just a bare RANGE) -- extractor must bail.
    Term r_lone = uop_range(2, 0, 64);
    UopDagConv2dFlatShape s = {0};
    int ok = uop_dag_extract_conv2d_flat_shape(r_lone, r_lone, &s);
    CHECK_EQ(ok, 0);
  }

  thvm_free();
  TEST_REPORT();
}
