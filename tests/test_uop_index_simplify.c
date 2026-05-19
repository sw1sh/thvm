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

  Term r = uop_range(0, KAX_LOOP, 8);
  Term r2 = uop_range(1, KAX_LOOP, 100);
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

  // === divandmod identity folds: (c*x + y) // c -> x ; (c*x + y) % c -> y ===
  TEST_BEGIN("simplify/divmod-affine-c-mul-x-divides-back-to-x");
  // c=3, x=r2 (extent 100), y=0: (3*r2)/3 -> r2.
  Term three = uop_const(DT_INT32, 3);
  Term mul3 = uop_int_binary(UOP_IMUL, r2, three);
  CHECK_EQ(uop_int_binary(UOP_IDIV, mul3, three), r2);

  TEST_BEGIN("simplify/divmod-affine-c-mul-x-mod-c-zero");
  // (3*r2) % 3 -> 0.
  Term mod3 = uop_int_binary(UOP_IMOD, mul3, three);
  CHECK_EQ(term_ext(mod3), UOP_CONST);

  TEST_BEGIN("simplify/divmod-affine-c-mul-x-plus-y-div-c-distributes");
  // (3*r2 + r) / 3.  r.extent (8) > 3, so the shallow `(c*x+y)/c -> x`
  // fold can't fire (y not provably < c).  The generalised div-of-affine
  // rule (d | c, m=1) still applies: (3*r2 + r)/3 -> r2 + r/3.
  Term mix = uop_int_binary(UOP_IADD, mul3, r);
  Term div_distr = uop_int_binary(UOP_IDIV, mix, three);
  CHECK_EQ(term_ext(div_distr), UOP_IADD);
  CHECK_EQ(heap_read(term_val(div_distr) + 0), r2);
  Term div_distr_rhs = heap_read(term_val(div_distr) + 1);
  CHECK_EQ(term_ext(div_distr_rhs), UOP_IDIV);
  CHECK_EQ(heap_read(term_val(div_distr_rhs) + 0), r);

  TEST_BEGIN("simplify/divmod-affine-c-mul-x-plus-y-divides-when-y-in-range");
  // (3*r2 + ya) / 3 -> r2 when ya in [0, 3).  Use a RANGE with extent 3.
  Term ya = uop_range(99, KAX_LOOP, 3);
  Term mix3 = uop_int_binary(UOP_IADD, mul3, ya);
  CHECK_EQ(uop_int_binary(UOP_IDIV, mix3, three), r2);
  // (3*r2 + ya) % 3 -> ya.
  CHECK_EQ(uop_int_binary(UOP_IMOD, mix3, three), ya);

  // === Affine normalization (B2 rest) ===
  TEST_BEGIN("simplify/affine-c1x-plus-c2x-collapses");
  // (3*r) + (5*r) -> 8*r
  Term r3 = uop_int_binary(UOP_IMUL, r, three);
  Term five = uop_const(DT_INT32, 5);
  Term r5 = uop_int_binary(UOP_IMUL, r, five);
  Term sum_collapsed = uop_int_binary(UOP_IADD, r3, r5);
  CHECK_EQ(term_ext(sum_collapsed), UOP_IMUL);
  CHECK_EQ(heap_read(term_val(sum_collapsed) + 0), r);

  TEST_BEGIN("simplify/affine-x-plus-cx-collapses");
  // r + (3*r) -> 4*r
  Term plus_self = uop_int_binary(UOP_IADD, r, r3);
  CHECK_EQ(term_ext(plus_self), UOP_IMUL);
  CHECK_EQ(heap_read(term_val(plus_self) + 0), r);
  Term coef = heap_read(term_val(plus_self) + 1);
  CHECK_EQ(term_ext(coef), UOP_CONST);

  TEST_BEGIN("simplify/affine-cx-plus-x-collapses-also");
  // (3*r) + r -> 4*r
  Term plus_self_rev = uop_int_binary(UOP_IADD, r3, r);
  CHECK_EQ(term_ext(plus_self_rev), UOP_IMUL);
  CHECK_EQ(heap_read(term_val(plus_self_rev) + 0), r);

  TEST_BEGIN("simplify/affine-skips-different-vars");
  // (3*r) + (5*r2) doesn't collapse; r != r2.
  Term r2_5 = uop_int_binary(UOP_IMUL, r2, five);
  Term mixed = uop_int_binary(UOP_IADD, r3, r2_5);
  CHECK_EQ(term_ext(mixed), UOP_IADD);

  TEST_BEGIN("simplify/divmod-recombination-collapses");
  // (r2 // 7) * 7 + (r2 % 7) -> r2.
  Term seven = uop_const(DT_INT32, 7);
  Term div7  = uop_int_binary(UOP_IDIV, r2, seven);
  Term div7_mul7 = uop_int_binary(UOP_IMUL, div7, seven);
  Term mod7  = uop_int_binary(UOP_IMOD, r2, seven);
  Term recombined = uop_int_binary(UOP_IADD, div7_mul7, mod7);
  CHECK_EQ(recombined, r2);

  TEST_BEGIN("simplify/divmod-recombination-skips-different-divisors");
  // (r2 // 7) * 7 + (r2 % 11) doesn't collapse; divisors differ.
  Term eleven = uop_const(DT_INT32, 11);
  Term mod11 = uop_int_binary(UOP_IMOD, r2, eleven);
  Term not_recombined = uop_int_binary(UOP_IADD, div7_mul7, mod11);
  CHECK_EQ(term_ext(not_recombined), UOP_IADD);

  TEST_BEGIN("simplify/idiv-gcd-aware-c-mul-x-div-c-times-y");
  // (3 * r2) // 6 -> r2 // 2 (gcd factors out the common 3).
  Term r2_times_3 = uop_int_binary(UOP_IMUL, r2, three);
  Term six = uop_const(DT_INT32, 6);
  Term gcd_div = uop_int_binary(UOP_IDIV, r2_times_3, six);
  CHECK_EQ(term_ext(gcd_div), UOP_IDIV);
  CHECK_EQ(heap_read(term_val(gcd_div) + 0), r2);
  Term gcd_div_b = heap_read(term_val(gcd_div) + 1);
  CHECK_EQ(term_ext(gcd_div_b), UOP_CONST);

  TEST_BEGIN("simplify/idiv-gcd-aware-skips-non-divisible");
  // (3 * r2) // 7 doesn't simplify (7 % 3 != 0).
  Term seven_b = uop_const(DT_INT32, 7);
  Term not_gcd = uop_int_binary(UOP_IDIV, r2_times_3, seven_b);
  CHECK_EQ(term_ext(not_gcd), UOP_IDIV);
  // Should NOT have rewritten the LHS (no fold should fire).
  CHECK_EQ(heap_read(term_val(not_gcd) + 0), r2_times_3);

  TEST_BEGIN("simplify/iadd-of-iadd-collapses-consts");
  // (r2 + 3) + 5 -> r2 + 8.
  Term r2_plus_3 = uop_int_binary(UOP_IADD, r2, three);
  Term r2_plus_8 = uop_int_binary(UOP_IADD, r2_plus_3, five);
  CHECK_EQ(term_ext(r2_plus_8), UOP_IADD);
  CHECK_EQ(heap_read(term_val(r2_plus_8) + 0), r2);
  Term plus_const = heap_read(term_val(r2_plus_8) + 1);
  CHECK_EQ(term_ext(plus_const), UOP_CONST);

  TEST_BEGIN("simplify/iadd-of-isub-collapses");
  // (r2 - 3) + 8 -> r2 + 5.
  Term ate = uop_const(DT_INT32, 8);
  Term r2_minus_3 = uop_int_binary(UOP_ISUB, r2, three);
  Term net5 = uop_int_binary(UOP_IADD, r2_minus_3, ate);
  CHECK_EQ(term_ext(net5), UOP_IADD);
  CHECK_EQ(heap_read(term_val(net5) + 0), r2);

  TEST_BEGIN("simplify/imul-of-imul-collapses-consts");
  // (r2 * 3) * 5 -> r2 * 15.
  Term r2_times_15 = uop_int_binary(UOP_IMUL, r2_times_3, five);
  CHECK_EQ(term_ext(r2_times_15), UOP_IMUL);
  CHECK_EQ(heap_read(term_val(r2_times_15) + 0), r2);
  Term mul_const = heap_read(term_val(r2_times_15) + 1);
  CHECK_EQ(term_ext(mul_const), UOP_CONST);

  TEST_BEGIN("simplify/isub-of-iadd-collapses");
  // (r2 + 8) - 3 -> r2 + 5.
  Term r2_plus_8b = uop_int_binary(UOP_IADD, r2, ate);
  Term sub_iadd = uop_int_binary(UOP_ISUB, r2_plus_8b, three);
  CHECK_EQ(term_ext(sub_iadd), UOP_IADD);
  CHECK_EQ(heap_read(term_val(sub_iadd) + 0), r2);

  TEST_BEGIN("simplify/isub-of-isub-collapses");
  // (r2 - 3) - 5 -> r2 - 8.
  Term r2_minus_3_b = uop_int_binary(UOP_ISUB, r2, three);
  Term sub_isub = uop_int_binary(UOP_ISUB, r2_minus_3_b, five);
  CHECK_EQ(term_ext(sub_isub), UOP_ISUB);
  CHECK_EQ(heap_read(term_val(sub_isub) + 0), r2);

  TEST_BEGIN("simplify/nested-div-mod");
  // (r2 % 6) // 2 -> (r2 // 2) % 3.  6 = 2*3, c=2, k=3.
  Term six_b = uop_const(DT_INT32, 6);
  Term mod_6 = uop_int_binary(UOP_IMOD, r2, six_b);
  Term div_2 = uop_int_binary(UOP_IDIV, mod_6, two);
  CHECK_EQ(term_ext(div_2), UOP_IMOD);
  // Right operand is k = 3.
  Term mod_const = heap_read(term_val(div_2) + 1);
  CHECK_EQ(term_ext(mod_const), UOP_CONST);

  TEST_BEGIN("simplify/nested-div-mod-skips-non-multiple");
  // (r2 % 7) // 2 doesn't simplify (7 not divisible by 2).
  Term seven_c = uop_const(DT_INT32, 7);
  Term mod_7 = uop_int_binary(UOP_IMOD, r2, seven_c);
  Term div_2_b = uop_int_binary(UOP_IDIV, mod_7, two);
  CHECK_EQ(term_ext(div_2_b), UOP_IDIV);  // not folded

  TEST_BEGIN("simplify/nested-mod-mod");
  // (r2 % 6) % 2 -> r2 % 2 (since 2 | 6).
  Term mod6_b = uop_int_binary(UOP_IMOD, r2, six_b);
  Term mod_2 = uop_int_binary(UOP_IMOD, mod6_b, two);
  CHECK_EQ(term_ext(mod_2), UOP_IMOD);
  CHECK_EQ(heap_read(term_val(mod_2) + 0), r2);
  Term mod_2_const = heap_read(term_val(mod_2) + 1);
  CHECK_EQ(term_ext(mod_2_const), UOP_CONST);

  TEST_BEGIN("simplify/nested-mod-mod-skips-non-multiple");
  // (r2 % 7) % 2 doesn't simplify.
  Term mod_7_2 = uop_int_binary(UOP_IMOD, mod_7, two);
  CHECK_EQ(term_ext(mod_7_2), UOP_IMOD);
  // Make sure it's the OUTER mod, not folded into r2.
  CHECK_EQ(heap_read(term_val(mod_7_2) + 0), mod_7);

  TEST_BEGIN("simplify/add-div-split");
  // (r + 8) // 3 -> (r + 2) // 3 + 2 (since 8 = 2*3 + 2, and r >= 0).
  // r is RANGE so non-negative.  c=8, d=3, c/d=2, c%d=2.
  Term split = uop_int_binary(UOP_IDIV,
                  uop_int_binary(UOP_IADD, r, uop_const(DT_INT32, 8)),
                  uop_const(DT_INT32, 3));
  // Expected outer shape: IADD(IDIV(...), 2).
  CHECK_EQ(term_ext(split), UOP_IADD);
  Term split_lhs = heap_read(term_val(split) + 0);
  Term split_rhs = heap_read(term_val(split) + 1);
  CHECK_EQ(term_ext(split_lhs), UOP_IDIV);
  CHECK_EQ(term_ext(split_rhs), UOP_CONST);

  TEST_BEGIN("simplify/add-div-split-skips-c-less-than-d");
  // (r + 2) // 3 -- c < d, doesn't split.
  Term no_split = uop_int_binary(UOP_IDIV,
                     uop_int_binary(UOP_IADD, r, two),
                     uop_const(DT_INT32, 3));
  CHECK_EQ(term_ext(no_split), UOP_IDIV);  // unchanged

  TEST_BEGIN("simplify/divmod-affine-roundtrip-end-to-end");
  // Resolve a 1D->1D identity-via-flat reshape: ndim=2, dims=[2,3].
  // resolve builds: flat = i0*3 + i1; in_iters[0] = flat / 3,
  // in_iters[1] = flat % 3 (for the 2-axis output side; 1-axis input
  // bottom).  We test the fold path directly here: with i0 ext>=, i1 ext=3,
  // (i0*3 + i1) / 3 -> i0 and (i0*3 + i1) % 3 -> i1.
  Term i0 = uop_range(200, KAX_LOOP, 2);
  Term i1 = uop_range(201, KAX_LOOP, 3);
  Term reshape_flat = uop_int_binary(UOP_IADD,
                       uop_int_binary(UOP_IMUL, i0, three), i1);
  CHECK_EQ(uop_int_binary(UOP_IDIV, reshape_flat, three), i0);
  CHECK_EQ(uop_int_binary(UOP_IMOD, reshape_flat, three), i1);

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
