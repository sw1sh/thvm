// test_uop_index.c - Phase B0 acceptance: symbolic INDEX layer.
//
// Verifies that UOP_RANGE, UOP_INDEX_E, UOP_I*, UOP_IWHERE, and
// UOP_INVALID:
//   1. allocate the heap layout documented in thvm.h;
//   2. hash-cons (identical args -> same Term);
//   3. compose into representative trees the way Phase B1's
//      movement-to-INDEX rules will build them.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // === UOP_RANGE: leaf ===
  TEST_BEGIN("uop-index/range-heap-layout");
  Term r0 = uop_range(/*axis_id=*/0, /*axis_type=*/KAX_LOOP, /*extent=*/32);
  CHECK_EQ(term_tag(r0), TAG_UOP);
  CHECK_EQ(term_ext(r0), UOP_RANGE);
  Term r0_axis  = heap_read(term_val(r0) + 0);
  Term r0_type  = heap_read(term_val(r0) + 1);
  Term r0_ext   = heap_read(term_val(r0) + 2);
  CHECK_EQ(term_tag(r0_axis), TAG_NUM);
  CHECK_EQ(term_val(r0_axis), 0);
  CHECK_EQ(term_val(r0_type), KAX_LOOP);
  CHECK_EQ(term_val(r0_ext),  32);

  TEST_BEGIN("uop-index/range-hash-cons");
  Term r0_again = uop_range(0, KAX_LOOP, 32);
  CHECK_EQ(r0_again, r0);
  // Different axis_id -> different Term.
  Term r1 = uop_range(1, KAX_LOOP, 32);
  CHECK(r1 != r0);
  // Different axis_type -> different Term.
  Term r0_red = uop_range(0, KAX_REDUCE, 32);
  CHECK(r0_red != r0);
  // Different extent -> different Term.
  Term r0_ext64 = uop_range(0, KAX_LOOP, 64);
  CHECK(r0_ext64 != r0);

  // === UOP_INVALID: singleton ===
  TEST_BEGIN("uop-index/invalid-singleton");
  Term inv1 = uop_invalid();
  Term inv2 = uop_invalid();
  CHECK_EQ(inv1, inv2);
  CHECK_EQ(term_tag(inv1), TAG_UOP);
  CHECK_EQ(term_ext(inv1), UOP_INVALID);

  // === UOP_I* binary: hash-cons + heap layout ===
  TEST_BEGIN("uop-index/iadd-layout");
  Term c5  = uop_const(DT_INT32, 5);
  Term c10 = uop_const(DT_INT32, 10);
  Term sum = uop_int_binary(UOP_IADD, r0, c5);
  CHECK_EQ(term_tag(sum), TAG_UOP);
  CHECK_EQ(term_ext(sum), UOP_IADD);
  CHECK_EQ(heap_read(term_val(sum) + 0), r0);
  CHECK_EQ(heap_read(term_val(sum) + 1), c5);

  TEST_BEGIN("uop-index/int-binary-hash-cons");
  Term sum_again = uop_int_binary(UOP_IADD, r0, c5);
  CHECK_EQ(sum_again, sum);
  Term diff_op   = uop_int_binary(UOP_IMUL, r0, c5);
  CHECK(diff_op != sum);
  Term diff_args = uop_int_binary(UOP_IADD, r0, c10);
  CHECK(diff_args != sum);

  TEST_BEGIN("uop-index/all-int-binary-opcodes-distinct");
  // Each opcode produces a distinct Term for the same (a, b).
  Term ops[7] = {
    uop_int_binary(UOP_IADD, r0, c5),
    uop_int_binary(UOP_ISUB, r0, c5),
    uop_int_binary(UOP_IMUL, r0, c5),
    uop_int_binary(UOP_IDIV, r0, c5),
    uop_int_binary(UOP_IMOD, r0, c5),
    uop_int_binary(UOP_ILT,  r0, c5),
    uop_int_binary(UOP_IAND, r0, c5),
  };
  for (int i = 0; i < 7; i++) {
    for (int j = i + 1; j < 7; j++) {
      CHECK(ops[i] != ops[j]);
    }
  }

  // === UOP_IWHERE: ternary ===
  TEST_BEGIN("uop-index/iwhere-layout");
  Term cond = uop_int_binary(UOP_ILT, r0, c10);
  Term then_v = uop_int_binary(UOP_ISUB, r0, c5);
  Term w = uop_iwhere(cond, then_v, uop_invalid());
  CHECK_EQ(term_tag(w), TAG_UOP);
  CHECK_EQ(term_ext(w), UOP_IWHERE);
  CHECK_EQ(heap_read(term_val(w) + 0), cond);
  CHECK_EQ(heap_read(term_val(w) + 1), then_v);
  CHECK_EQ(heap_read(term_val(w) + 2), uop_invalid());

  TEST_BEGIN("uop-index/iwhere-hash-cons");
  Term w_again = uop_iwhere(cond, then_v, uop_invalid());
  CHECK_EQ(w_again, w);

  // === UOP_INDEX_E: buffer + addr ===
  TEST_BEGIN("uop-index/index-e-layout");
  Term buf = term_new(0, TAG_TEN, DT_FP32, 1);
  Term idx = uop_index_e(buf, sum);
  CHECK_EQ(term_tag(idx), TAG_UOP);
  CHECK_EQ(term_ext(idx), UOP_INDEX_E);
  CHECK_EQ(heap_read(term_val(idx) + 0), buf);
  CHECK_EQ(heap_read(term_val(idx) + 1), sum);

  TEST_BEGIN("uop-index/index-e-hash-cons");
  Term idx_again = uop_index_e(buf, sum);
  CHECK_EQ(idx_again, idx);
  Term idx_diff_buf = uop_index_e(term_new(0, TAG_TEN, DT_FP32, 2), sum);
  CHECK(idx_diff_buf != idx);

  // === Compose: PAD-style guard expression mirroring tinygrad ===
  // ((r >= K) & (r < shape+K)).where(r-K, INVALID)
  TEST_BEGIN("uop-index/pad-style-composition");
  Term r       = uop_range(0, KAX_LOOP, 36);   // padded shape
  Term K       = uop_const(DT_INT32, 2);          // pad start
  Term sh_pK   = uop_const(DT_INT32, 34);         // shape + K (precomputed)
  Term lo_ok   = uop_int_binary(UOP_ILT, K, r);              // K < r  -> r >= K+1; close enough
  Term hi_ok   = uop_int_binary(UOP_ILT, r, sh_pK);          // r < shape+K
  Term in_bnds = uop_int_binary(UOP_IAND, lo_ok, hi_ok);
  Term shifted = uop_int_binary(UOP_ISUB, r, K);
  Term guarded = uop_iwhere(in_bnds, shifted, uop_invalid());
  Term load    = uop_index_e(buf, guarded);
  CHECK_EQ(term_tag(load), TAG_UOP);
  CHECK_EQ(term_ext(load), UOP_INDEX_E);
  // The guard tree is reachable through the addr child.
  Term load_addr = heap_read(term_val(load) + 1);
  CHECK_EQ(term_ext(load_addr), UOP_IWHERE);
  Term load_else = heap_read(term_val(load_addr) + 2);
  CHECK_EQ(term_ext(load_else), UOP_INVALID);

  thvm_free();
  TEST_REPORT();
}
