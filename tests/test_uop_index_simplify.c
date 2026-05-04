// test_uop_index_simplify.c - Phase B2: symbolic INDEX simplifier.
//
// Verifies the constructor-time folds in index_simplify.c fire on
// the canonical patterns that uop_resolve_movement_chain emits, and
// don't fire on operands that aren't reducible.  Each block has both
// a "rule fires" and a "rule skips" case so a false positive (rule
// firing too aggressively) shows up as a regression on the skip
// assertion.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  Term r = uop_range(0, S_AXIS_LOOP, 8);
  Term r2 = uop_range(1, S_AXIS_LOOP, 100);
  Term zero = uop_const(DT_INT32, 0);
  Term one  = uop_const(DT_INT32, 1);
  Term two  = uop_const(DT_INT32, 2);
  Term ten  = uop_const(DT_INT32, 10);

  // === IADD ===
  TEST_BEGIN("simplify/iadd-zero-identity");
  CHECK_EQ(uop_int_binary(UOP_IADD, r, zero), r);
  CHECK_EQ(uop_int_binary(UOP_IADD, zero, r), r);

  TEST_BEGIN("simplify/iadd-const-fold");
  Term sum = uop_int_binary(UOP_IADD, ten, two);
  CHECK_EQ(term_ext(sum), UOP_CONST);
  i64 sv;
  CHECK(term_tag(heap_read(term_val(sum))) == TAG_NUM);
  sv = (i64)(i32)term_val(heap_read(term_val(sum)));
  CHECK_EQ(sv, 12);

  TEST_BEGIN("simplify/iadd-skips-non-zero-non-const");
  Term s2 = uop_int_binary(UOP_IADD, r, two);
  CHECK_EQ(term_ext(s2), UOP_IADD);  // not folded

  // === ISUB ===
  TEST_BEGIN("simplify/isub-zero-identity");
  CHECK_EQ(uop_int_binary(UOP_ISUB, r, zero), r);

  TEST_BEGIN("simplify/isub-self-cancel");
  CHECK_EQ(uop_int_binary(UOP_ISUB, r, r), zero);

  TEST_BEGIN("simplify/isub-const-fold");
  Term sub = uop_int_binary(UOP_ISUB, ten, two);
  CHECK_EQ(term_ext(sub), UOP_CONST);

  // === IMUL ===
  TEST_BEGIN("simplify/imul-zero-annihilator");
  CHECK_EQ(uop_int_binary(UOP_IMUL, r, zero), zero);
  CHECK_EQ(uop_int_binary(UOP_IMUL, zero, r), zero);

  TEST_BEGIN("simplify/imul-one-identity");
  CHECK_EQ(uop_int_binary(UOP_IMUL, r, one), r);
  CHECK_EQ(uop_int_binary(UOP_IMUL, one, r), r);

  TEST_BEGIN("simplify/imul-const-fold");
  Term mul = uop_int_binary(UOP_IMUL, ten, two);
  CHECK_EQ(term_ext(mul), UOP_CONST);

  TEST_BEGIN("simplify/imul-skips-rangexvar");
  Term m2 = uop_int_binary(UOP_IMUL, r, two);
  CHECK_EQ(term_ext(m2), UOP_IMUL);

  // === IDIV ===
  TEST_BEGIN("simplify/idiv-by-one-identity");
  CHECK_EQ(uop_int_binary(UOP_IDIV, r, one), r);

  TEST_BEGIN("simplify/idiv-zero-numerator");
  CHECK_EQ(uop_int_binary(UOP_IDIV, zero, r2), zero);

  TEST_BEGIN("simplify/idiv-const-fold");
  Term div = uop_int_binary(UOP_IDIV, ten, two);
  CHECK_EQ(term_ext(div), UOP_CONST);

  // === IMOD ===
  TEST_BEGIN("simplify/imod-by-one-zero");
  CHECK_EQ(uop_int_binary(UOP_IMOD, r, one), zero);

  TEST_BEGIN("simplify/imod-range-bound-aware");
  // r has extent 8; r % 10 = r (range never reaches 10).
  CHECK_EQ(uop_int_binary(UOP_IMOD, r, ten), r);
  // r has extent 8; r % 8 = r (boundary case: extent <= mod operand).
  Term eight = uop_const(DT_INT32, 8);
  CHECK_EQ(uop_int_binary(UOP_IMOD, r, eight), r);

  TEST_BEGIN("simplify/imod-skips-when-extent-too-large");
  // r2 has extent 100; r2 % 8 stays as IMOD (extent > 8).
  Term mod_big = uop_int_binary(UOP_IMOD, r2, eight);
  CHECK_EQ(term_ext(mod_big), UOP_IMOD);

  // === ILT ===
  TEST_BEGIN("simplify/ilt-self-false");
  CHECK_EQ(uop_int_binary(UOP_ILT, r, r), zero);

  TEST_BEGIN("simplify/ilt-range-bound-aware");
  // r has extent 8; r < 10 is always true.
  CHECK_EQ(uop_int_binary(UOP_ILT, r, ten), one);

  TEST_BEGIN("simplify/ilt-const-fold");
  Term lt = uop_int_binary(UOP_ILT, two, ten);
  CHECK_EQ(term_ext(lt), UOP_CONST);

  TEST_BEGIN("simplify/ilt-skips-when-cond-not-bound");
  // r2 has extent 100; r2 < 50 is not statically true.
  Term fifty = uop_const(DT_INT32, 50);
  Term lt2 = uop_int_binary(UOP_ILT, r2, fifty);
  CHECK_EQ(term_ext(lt2), UOP_ILT);

  // === IAND ===
  TEST_BEGIN("simplify/iand-zero-annihilator");
  CHECK_EQ(uop_int_binary(UOP_IAND, r, zero), zero);
  CHECK_EQ(uop_int_binary(UOP_IAND, zero, r), zero);

  TEST_BEGIN("simplify/iand-one-identity");
  CHECK_EQ(uop_int_binary(UOP_IAND, r, one), r);
  CHECK_EQ(uop_int_binary(UOP_IAND, one, r), r);

  TEST_BEGIN("simplify/iand-self-idempotent");
  CHECK_EQ(uop_int_binary(UOP_IAND, r, r), r);

  // === IWHERE ===
  TEST_BEGIN("simplify/iwhere-true-cond");
  CHECK_EQ(uop_iwhere(one, r, r2), r);

  TEST_BEGIN("simplify/iwhere-false-cond");
  CHECK_EQ(uop_iwhere(zero, r, r2), r2);

  TEST_BEGIN("simplify/iwhere-equal-branches");
  CHECK_EQ(uop_iwhere(r, r2, r2), r2);

  TEST_BEGIN("simplify/iwhere-skips-symbolic-cond");
  Term cond = uop_int_binary(UOP_ILT, r, r2);
  Term w = uop_iwhere(cond, r, r2);
  CHECK_EQ(term_ext(w), UOP_IWHERE);

  // === Composition: PAD-style mask collapses to true ===
  TEST_BEGIN("simplify/pad-style-mask-folds-when-iter-fully-in-bounds");
  // (r < 10) & (1 - (r < 0)).  r has extent 8.  (r < 10) -> 1, (r < 0) -> 0,
  // 1 - 0 -> 1, 1 & 1 -> 1.
  Term lt_hi = uop_int_binary(UOP_ILT, r, ten);
  CHECK_EQ(lt_hi, one);
  Term lt_lo = uop_int_binary(UOP_ILT, r, zero);
  CHECK_EQ(lt_lo, zero);
  Term ge_lo = uop_int_binary(UOP_ISUB, one, lt_lo);
  CHECK_EQ(ge_lo, one);
  Term axis_ok = uop_int_binary(UOP_IAND, lt_hi, ge_lo);
  CHECK_EQ(axis_ok, one);
  // IWHERE(1, load, INVALID) -> load.
  Term load = uop_const(DT_INT32, 42);
  Term guarded = uop_iwhere(axis_ok, load, uop_invalid());
  CHECK_EQ(guarded, load);

  thvm_free();
  TEST_REPORT();
}
