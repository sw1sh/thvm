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

  thvm_free();
  TEST_REPORT();
}
