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

  thvm_free();
  TEST_REPORT();
}
