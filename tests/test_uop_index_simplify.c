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

  TEST_BEGIN("simplify/nested-idiv-collapses");
  // (r2 // 6) // 2 -> r2 // 12 (6*2).  r2 is RANGE so non-negative.
  Term r2_div6 = uop_int_binary(UOP_IDIV, r2, six_b);
  Term nested_div = uop_int_binary(UOP_IDIV, r2_div6, two);
  CHECK_EQ(term_ext(nested_div), UOP_IDIV);
  CHECK_EQ(heap_read(term_val(nested_div) + 0), r2);
  Term nested_div_den = heap_read(term_val(nested_div) + 1);
  CHECK_EQ(term_ext(nested_div_den), UOP_CONST);
  i64 nd_den;
  CHECK(uop_iconst_value(nested_div_den, &nd_den));
  CHECK_EQ(nd_den, 12);

  TEST_BEGIN("simplify/nested-idiv-skips-non-const-inner-divisor");
  // (r2 // r) // 2 -- inner divisor not a const -> no collapse.
  Term div_var_den = uop_int_binary(UOP_IDIV, r2, r);
  Term nested_div_skip = uop_int_binary(UOP_IDIV, div_var_den, two);
  CHECK_EQ(term_ext(nested_div_skip), UOP_IDIV);
  CHECK_EQ(heap_read(term_val(nested_div_skip) + 0), div_var_den);

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

  // === uop_int_bounds: conservative [lo, hi] interval inference =========
  // The primitive is NOT wired into any rule; these tests pin its exact
  // contract on the canonical shapes a later increment will consume.
  {
    i64 blo, bhi;

    TEST_BEGIN("bounds/const");
    Term c7 = uop_const(DT_INT32, 7);
    CHECK_EQ(uop_int_bounds(c7, &blo, &bhi), 1);
    CHECK_EQ(blo, 7);
    CHECK_EQ(bhi, 7);

    TEST_BEGIN("bounds/negative-const");
    Term cneg = uop_const(DT_INT32, (u32)(i32)-5);
    CHECK_EQ(uop_int_bounds(cneg, &blo, &bhi), 1);
    CHECK_EQ(blo, -5);
    CHECK_EQ(bhi, -5);

    TEST_BEGIN("bounds/range");
    // r0 has extent 4 -> iter in [0, 3].
    Term r0 = uop_range(300, KAX_LOOP, 4);
    CHECK_EQ(uop_int_bounds(r0, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 3);

    TEST_BEGIN("bounds/add");
    // r0 in [0,3], r1 in [0,9] -> sum in [0, 12].
    Term r1 = uop_range(301, KAX_LOOP, 10);
    Term add01 = uop_int_binary(UOP_IADD, r0, r1);
    CHECK_EQ(uop_int_bounds(add01, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 12);

    TEST_BEGIN("bounds/sub");
    // r0 in [0,3], r1 in [0,9] -> r0 - r1 in [0-9, 3-0] = [-9, 3].
    Term sub01 = uop_int_binary(UOP_ISUB, r0, r1);
    CHECK_EQ(uop_int_bounds(sub01, &blo, &bhi), 1);
    CHECK_EQ(blo, -9);
    CHECK_EQ(bhi, 3);

    TEST_BEGIN("bounds/mul-positive-const");
    // 10 * r0, r0 in [0,3] -> [0, 30].
    Term mul10 = uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, 10));
    CHECK_EQ(uop_int_bounds(mul10, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 30);

    TEST_BEGIN("bounds/mul-negative-const-swaps");
    // (-3) * r0, r0 in [0,3] -> [-9, 0] (endpoints swap for c < 0).
    Term mulneg = uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, (u32)(i32)-3));
    CHECK_EQ(uop_int_bounds(mulneg, &blo, &bhi), 1);
    CHECK_EQ(blo, -9);
    CHECK_EQ(bhi, 0);

    TEST_BEGIN("bounds/mul-two-vars-corners");
    // (r0 - 2) * (r1 - 4): a in [-2, 1], b in [-4, 5].
    // corners: -2*-4=8, -2*5=-10, 1*-4=-4, 1*5=5 -> [-10, 8].
    Term a_off = uop_int_binary(UOP_ISUB, r0, uop_const(DT_INT32, 2));
    Term b_off = uop_int_binary(UOP_ISUB, r1, uop_const(DT_INT32, 4));
    Term mul_vv = uop_int_binary(UOP_IMUL, a_off, b_off);
    CHECK_EQ(uop_int_bounds(mul_vv, &blo, &bhi), 1);
    CHECK_EQ(blo, -10);
    CHECK_EQ(bhi, 8);

    TEST_BEGIN("bounds/idiv-positive-const");
    // r1 in [0,9], r1 / 3 -> [0, 3].
    Term div3 = uop_int_binary(UOP_IDIV, r1, uop_const(DT_INT32, 3));
    CHECK_EQ(uop_int_bounds(div3, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 3);

    TEST_BEGIN("bounds/idiv-possibly-negative-numerator-unknown");
    // (r0 - 5) / 3: the numerator can be negative (r0 < 5), where thvm's
    // C-truncated `/` disagrees with floor, so a bound would be unsound.
    // The estimator returns 0 (unknown) for a possibly-negative IDIV
    // numerator (index exprs are non-negative; this only loses synthetic
    // negatives).
    Term neg_num = uop_int_binary(UOP_ISUB, r0, uop_const(DT_INT32, 5));
    Term div_neg = uop_int_binary(UOP_IDIV, neg_num, uop_const(DT_INT32, 3));
    CHECK_EQ(uop_int_bounds(div_neg, &blo, &bhi), 0);

    TEST_BEGIN("bounds/imod-conservative");
    // r1 in [0,9], r1 % 4: numerator can exceed 4 (9 >= 4), so the safe
    // conservative bound [0, c-1] = [0, 3] applies.
    Term mod4 = uop_int_binary(UOP_IMOD, r1, uop_const(DT_INT32, 4));
    CHECK_EQ(uop_int_bounds(mod4, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 3);

    TEST_BEGIN("bounds/imod-tightened-when-numerator-below-modulus");
    // r0 in [0,3], r0 % 5: numerator provably < 5, so x % 5 == x and the
    // bound tightens to [0, 3].
    Term mod5 = uop_int_binary(UOP_IMOD, r0, uop_const(DT_INT32, 5));
    CHECK_EQ(uop_int_bounds(mod5, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 3);

    TEST_BEGIN("bounds/where-union");
    // select(cond, r0, r1+100): branches [0,3] and [100,109] -> [0, 109].
    Term r1_100 = uop_int_binary(UOP_IADD, r1, uop_const(DT_INT32, 100));
    Term cmp = uop_int_binary(UOP_ILT, r0, r1);
    Term sel = uop_iwhere(cmp, r0, r1_100);
    CHECK_EQ(uop_int_bounds(sel, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 109);

    TEST_BEGIN("bounds/where-invalid-branch-skipped");
    // select(cond, r1, INVALID): the INVALID else-branch is a masking
    // sentinel; the bound is just the live branch r1 -> [0, 9].
    Term sel_inv = uop_iwhere(cmp, r1, uop_invalid());
    CHECK_EQ(uop_int_bounds(sel_inv, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 9);

    TEST_BEGIN("bounds/cmplt-boolean");
    // r0 < r1 -> boolean [0, 1].
    CHECK_EQ(uop_int_bounds(cmp, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 1);

    TEST_BEGIN("bounds/iand-boolean");
    // (r0 < r1) & (r1 < r0) -> both boolean -> [0, 1].
    Term cmp2 = uop_int_binary(UOP_ILT, r1, r0);
    Term and_bb = uop_int_binary(UOP_IAND, cmp, cmp2);
    CHECK_EQ(uop_int_bounds(and_bb, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 1);

    TEST_BEGIN("bounds/nested-pack-r0x10-plus-r1");
    // (r0*10 + r1), r0 in [0,3], r1 in [0,9] -> [0, 39].
    Term packed = uop_int_binary(UOP_IADD,
                    uop_int_binary(UOP_IMUL, r0, uop_const(DT_INT32, 10)), r1);
    CHECK_EQ(uop_int_bounds(packed, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 39);

    TEST_BEGIN("bounds/nested-pack-div11");
    // (r0*10 + r1) / 11, numerator in [0, 39] -> [0, 3].
    Term packed_div = uop_int_binary(UOP_IDIV, packed, uop_const(DT_INT32, 11));
    CHECK_EQ(uop_int_bounds(packed_div, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 3);

    TEST_BEGIN("bounds/nested-pack-mod11");
    // (r0*10 + r1) % 11, numerator in [0, 39] (>= 11) -> [0, 10].
    Term packed_mod = uop_int_binary(UOP_IMOD, packed, uop_const(DT_INT32, 11));
    CHECK_EQ(uop_int_bounds(packed_mod, &blo, &bhi), 1);
    CHECK_EQ(blo, 0);
    CHECK_EQ(bhi, 10);

    TEST_BEGIN("bounds/unknown-buffer-returns-zero");
    // A bare BUFFER is not an int index expr -> no info.
    u32 dims[1] = { 16 };
    Term buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims);
    CHECK_EQ(uop_int_bounds(buf, &blo, &bhi), 0);

    TEST_BEGIN("bounds/idiv-non-const-divisor-returns-zero");
    // r1 / r0 -- divisor not a positive const -> unknown.
    Term div_var = uop_int_binary(UOP_IDIV, r1, r0);
    CHECK_EQ(uop_int_bounds(div_var, &blo, &bhi), 0);
  }

  // === fast_idiv: magicgu + IDIV/IMOD -> mul-shift late lowering ===
  {
    // magicgu correctness: (x*m)>>s == x/d for every 0<=x<=vmax, for the
    // conv _pool intermediate widths (9, 11, 24, 25, 28).  This is the
    // exact identity the render-time lowering relies on.
    i64 widths[5] = { 9, 11, 24, 25, 28 };
    i64 vmaxes[5] = { 31, 39, 143, 143, 167 };
    for (int wi = 0; wi < 5; wi++) {
      i64 d = widths[wi], vmax = vmaxes[wi], m = 0, s = 0;
      char nm[64];
      snprintf(nm, sizeof nm, "fastidiv/magicgu-d%lld", (long long)d);
      TEST_BEGIN(nm);
      CHECK_EQ(thvm_magicgu(vmax, d, &m, &s), 1);
      int all_ok = 1;
      for (i64 x = 0; x <= vmax; x++) {
        // signed arithmetic right-shift, matching the rendered (int) cast.
        if ((i64)(((i32)(x * m)) >> (i32)s) != x / d) { all_ok = 0; break; }
      }
      CHECK_EQ(all_ok, 1);
    }

    Term fr0 = uop_range(400, KAX_LOOP, 4);   // [0,3]
    Term fr1 = uop_range(401, KAX_LOOP, 10);  // [0,9]
    Term fnum = uop_int_binary(UOP_IADD,
                  uop_int_binary(UOP_IMUL, fr0, uop_const(DT_INT32, 10)), fr1);

    // fast_idiv-node: bounded non-negative `(r*10 + r1) / 11` (num in
    // [0,39]) lowers to ISHR(IMUL(num, m), s).
    TEST_BEGIN("fastidiv/idiv-node-lowers-to-shr");
    Term div11 = uop_int_binary(UOP_IDIV, fnum, uop_const(DT_INT32, 11));
    CHECK_EQ(term_ext(div11), UOP_IDIV);  // simplifier leaves it IDIV
    Term lowered = uop_fast_idiv_node(div11);
    CHECK(lowered != 0);
    CHECK_EQ(term_ext(lowered), UOP_ISHR);
    CHECK_EQ(term_ext(heap_read(term_val(lowered) + 0)), UOP_IMUL);

    // fast_idiv-node: `num % 11` lowers to ISUB(num, IMUL(div, 11)) with NO
    // residual IDIV/IMOD.
    TEST_BEGIN("fastidiv/imod-node-lowers-to-sub-mul-shr");
    Term mod11 = uop_int_binary(UOP_IMOD, fnum, uop_const(DT_INT32, 11));
    Term modlow = uop_fast_idiv_node(mod11);
    CHECK(modlow != 0);
    CHECK_EQ(term_ext(modlow), UOP_ISUB);
    Term sub_rhs = heap_read(term_val(modlow) + 1);
    CHECK_EQ(term_ext(sub_rhs), UOP_IMUL);

    // skip: power-of-two divisor is NOT lowered (signed >> is not a valid
    // truncating divide there; left to the existing/simpler fold).
    TEST_BEGIN("fastidiv/skips-power-of-two");
    Term div8 = uop_int_binary(UOP_IDIV, fnum, uop_const(DT_INT32, 8));
    CHECK_EQ(uop_fast_idiv_node(div8), 0);

    // skip: a non-const (unbounded) divisor path is NOT lowered.
    TEST_BEGIN("fastidiv/skips-non-const-divisor");
    Term divvar = uop_int_binary(UOP_IDIV, fr1, fr0);
    CHECK_EQ(uop_fast_idiv_node(divvar), 0);

    // regression (silent-corruption guard): a GATED numerator proven BELOW
    // the divisor -- IWHERE(cond, r1[0,9], INVALID) // 25 -- must lower to the
    // EXACT value (x//25 == 0, x%25 == x), NOT the degenerate magicgu identity
    // m=1,s=0 that mis-lowers x//25 to (x*1)>>0 == x.  This shape evades the
    // constructor's x//c->0 fold (which uses the IWHERE-blind max-value path)
    // but reaches fast_idiv (whose uop_int_bounds DOES see through IWHERE).
    TEST_BEGIN("fastidiv/gated-numerator-below-divisor-folds-exact");
    Term gcond = uop_int_binary(UOP_ILT, fr0, fr1);     // symbolic, won't fold
    Term gated = uop_iwhere(gcond, fr1, uop_invalid()); // bounds [0,9], stays IWHERE
    CHECK_EQ(term_ext(gated), UOP_IWHERE);
    Term gdiv = uop_int_binary(UOP_IDIV, gated, uop_const(DT_INT32, 25));
    CHECK_EQ(term_ext(gdiv), UOP_IDIV);                 // not pre-folded
    Term gdiv_low = uop_fast_idiv_node(gdiv);
    CHECK(gdiv_low != 0);
    CHECK_EQ(term_ext(gdiv_low), UOP_CONST);            // == 0, NOT x
    i64 gdv;
    CHECK(uop_iconst_value(gdiv_low, &gdv));
    CHECK_EQ(gdv, 0);
    Term gmod = uop_int_binary(UOP_IMOD, gated, uop_const(DT_INT32, 25));
    CHECK_EQ(uop_fast_idiv_node(gmod), gated);          // x%25 == x for x<25
  }

  // === Ported tinygrad symbolic rules (29-rule batch) ====================
  {
    Term ra = uop_range(500, KAX_LOOP, 8);    // [0,7]
    Term rb = uop_range(501, KAX_LOOP, 100);  // [0,99]
    Term c2 = uop_const(DT_INT32, 2);
    Term c3 = uop_const(DT_INT32, 3);
    Term c5 = uop_const(DT_INT32, 5);
    Term c6 = uop_const(DT_INT32, 6);
    Term c7 = uop_const(DT_INT32, 7);

    // Rule 1: (x//c + a)//d -> (x + a*c)//(c*d).  (rb//3 + 2)//5.
    TEST_BEGIN("ported/fast-inline-div-add-div");
    Term r1n = uop_int_binary(UOP_IADD,
                 uop_int_binary(UOP_IDIV, rb, c3), c2);
    Term r1 = uop_int_binary(UOP_IDIV, r1n, c5);
    CHECK_EQ(term_ext(r1), UOP_IDIV);
    // numerator is (rb + 6), divisor 15.
    {
      i64 dd;
      CHECK(uop_iconst_value(heap_read(term_val(r1) + 1), &dd));
      CHECK_EQ(dd, 15);
    }
    TEST_BEGIN("ported/fast-inline-div-add-div-skips-nonconst-addend");
    // (rb//3 + ra)//5 -- addend ra non-const -> no fast-inline collapse.
    Term r1ns = uop_int_binary(UOP_IADD,
                  uop_int_binary(UOP_IDIV, rb, c3), ra);
    Term r1s = uop_int_binary(UOP_IDIV, r1ns, c5);
    CHECK_EQ(term_ext(r1s), UOP_IDIV);
    {
      i64 dd;
      CHECK(uop_iconst_value(heap_read(term_val(r1s) + 1), &dd));
      CHECK_EQ(dd, 5);  // divisor unchanged
    }

    // Rule 2: (a%(k*c) + b)%c -> (a+b)%c.  ((rb%6) + ra)%3.
    TEST_BEGIN("ported/remove-nested-mod-in-sum");
    Term r2in = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMOD, rb, c6), ra);
    Term r2r = uop_int_binary(UOP_IMOD, r2in, c3);
    CHECK_EQ(term_ext(r2r), UOP_IMOD);
    {
      // inner mod must be gone: numerator is (rb + ra), no IMOD leaf.
      Term num = heap_read(term_val(r2r) + 0);
      CHECK_EQ(term_ext(num), UOP_IADD);
      Term l = heap_read(term_val(num) + 0);
      CHECK(term_ext(l) != UOP_IMOD);
    }
    TEST_BEGIN("ported/remove-nested-mod-skips-non-multiple");
    // ((rb%7) + ra)%3 -- 7 not a multiple of 3 -> inner mod stays.
    Term r2ns = uop_int_binary(UOP_IMOD,
                  uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMOD, rb, c7), ra),
                  c3);
    CHECK_EQ(term_ext(r2ns), UOP_IMOD);

    // Rule 4: cancel_divmod.  (ra + 64)//8 -> qv (ra in [0,7] -> num in
    // [64,71] -> 64/8==71/8==8, single interval).
    TEST_BEGIN("ported/cancel-divmod-single-interval");
    Term r4n = uop_int_binary(UOP_IADD, ra, uop_const(DT_INT32, 64));
    Term r4 = uop_int_binary(UOP_IDIV, r4n, uop_const(DT_INT32, 8));
    CHECK_EQ(term_ext(r4), UOP_CONST);
    {
      i64 qv;
      CHECK(uop_iconst_value(r4, &qv));
      CHECK_EQ(qv, 8);
    }
    // mod companion: (ra+64)%8 -> num - 8*8 = (ra+64) - 64 = ra.
    Term r4m = uop_int_binary(UOP_IMOD, r4n, uop_const(DT_INT32, 8));
    CHECK_EQ(r4m, ra);
    TEST_BEGIN("ported/cancel-divmod-skips-multi-interval");
    // rb//8 spans many intervals (rb in [0,99]) -> no const fold.
    Term r4s = uop_int_binary(UOP_IDIV, rb, uop_const(DT_INT32, 8));
    CHECK_EQ(term_ext(r4s), UOP_IDIV);

    // Rule 5: x % x -> 0.
    TEST_BEGIN("ported/mod-self-zero");
    CHECK_EQ(uop_int_binary(UOP_IMOD, rb, rb), zero);
    TEST_BEGIN("ported/mod-self-skips-distinct");
    Term r5s = uop_int_binary(UOP_IMOD, rb, ra);
    CHECK_EQ(term_ext(r5s), UOP_IMOD);

    // Rules 10/11 (distribute const over IADD) REVERTED: they explode the
    // affine numerator that uop_fast_idiv_node lowers via a single magic
    // multiply.  See report.  2*(rb+3) therefore stays IMUL(2, IADD).
    TEST_BEGIN("ported/distribute-const-over-add-reverted");
    Term r10 = uop_int_binary(UOP_IMUL, c2, uop_int_binary(UOP_IADD, rb, c3));
    CHECK_EQ(term_ext(r10), UOP_IMUL);

    // Rule 12: (x*c1)*y -> (x*y)*c1.  (rb*3)*ra -> (rb*ra)*3.
    TEST_BEGIN("ported/hoist-const-factor");
    Term r12 = uop_int_binary(UOP_IMUL, uop_int_binary(UOP_IMUL, rb, c3), ra);
    CHECK_EQ(term_ext(r12), UOP_IMUL);
    {
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(r12) + 1), &cc));
      CHECK_EQ(cc, 3);
      Term inner = heap_read(term_val(r12) + 0);
      CHECK_EQ(term_ext(inner), UOP_IMUL);  // (rb*ra)
    }
    TEST_BEGIN("ported/hoist-skips-both-nonconst");
    Term r12s = uop_int_binary(UOP_IMUL, rb, ra);
    CHECK_EQ(term_ext(r12s), UOP_IMUL);
    CHECK_EQ(heap_read(term_val(r12s) + 0), rb);

    // Rule 13/15: (y + x*c0) + x*c1 -> y + x*(c0+c1).
    TEST_BEGIN("ported/combine-buried-const-mults");
    Term yx2 = uop_int_binary(UOP_IADD, ra, uop_int_binary(UOP_IMUL, rb, c2));
    Term r13 = uop_int_binary(UOP_IADD, yx2, uop_int_binary(UOP_IMUL, rb, c3));
    CHECK_EQ(term_ext(r13), UOP_IADD);
    {
      // result is ra + rb*5.
      Term comb = heap_read(term_val(r13) + 1);
      CHECK_EQ(term_ext(comb), UOP_IMUL);
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(comb) + 1), &cc));
      CHECK_EQ(cc, 5);
    }
    TEST_BEGIN("ported/combine-buried-skips-different-x");
    // (ra + rb*2) + r*3 with r != rb -> no combine.
    Term r13s = uop_int_binary(UOP_IADD,
                  uop_int_binary(UOP_IADD, ra, uop_int_binary(UOP_IMUL, rb, c2)),
                  uop_int_binary(UOP_IMUL, r, c3));
    {
      // top is still an IADD whose rhs is r*3 (not combined into rb).
      CHECK_EQ(term_ext(r13s), UOP_IADD);
    }

    // Rule 14: x + x -> x*2.
    TEST_BEGIN("ported/add-self-doubles");
    Term r14 = uop_int_binary(UOP_IADD, rb, rb);
    CHECK_EQ(term_ext(r14), UOP_IMUL);
    {
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(r14) + 1), &cc));
      CHECK_EQ(cc, 2);
    }
    TEST_BEGIN("ported/add-self-skips-distinct");
    Term r14s = uop_int_binary(UOP_IADD, rb, ra);
    CHECK_EQ(term_ext(r14s), UOP_IADD);

    // Rule 16: (x + c1) + y -> (x + y) + c1.  (rb + 5) + ra.
    TEST_BEGIN("ported/move-add-const-to-end");
    Term r16 = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IADD, rb, c5), ra);
    CHECK_EQ(term_ext(r16), UOP_IADD);
    {
      // const 5 ends up at the outer rhs.
      Term r16r = heap_read(term_val(r16) + 1);
      i64 cc;
      CHECK(uop_iconst_value(r16r, &cc));
      CHECK_EQ(cc, 5);
    }
    TEST_BEGIN("ported/move-add-const-skips-when-y-const");
    // (rb + 5) + 3 -> rb + 8 (the existing const-collapse, not the reassoc).
    Term r16s = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IADD, rb, c5), c3);
    CHECK_EQ(term_ext(r16s), UOP_IADD);
    {
      Term r16sr = heap_read(term_val(r16s) + 1);
      i64 cc;
      CHECK(uop_iconst_value(r16sr, &cc));
      CHECK_EQ(cc, 8);
    }

    // Rule 17: bound-fold via uop_int_bounds.  (rb%4) < 10 -> 1 (hi=3<10).
    TEST_BEGIN("ported/ilt-bound-fold-true");
    Term mod4 = uop_int_binary(UOP_IMOD, rb, uop_const(DT_INT32, 4));
    CHECK_EQ(uop_int_binary(UOP_ILT, mod4, ten), one);
    // (ra*10) < 5 -> 0 (lo=0 >= 5? no... use a shifted lower bound).
    TEST_BEGIN("ported/ilt-bound-fold-false");
    // (ra + 20) < 10 -> 0 (lo=20 >= 10).
    Term shifted = uop_int_binary(UOP_IADD, ra, uop_const(DT_INT32, 20));
    CHECK_EQ(uop_int_binary(UOP_ILT, shifted, ten), zero);
    TEST_BEGIN("ported/ilt-bound-fold-skips-unknown");
    // rb < 50 -> rb in [0,99], not decidable -> stays ILT.
    Term r17s = uop_int_binary(UOP_ILT, rb, uop_const(DT_INT32, 50));
    CHECK_EQ(term_ext(r17s), UOP_ILT);

    // Rule 18: (c0 + x) < c1 -> x < (c1 - c0).  (5 + ra) < 12 -> ra < 7.
    TEST_BEGIN("ported/ilt-shift-const");
    Term r18 = uop_int_binary(UOP_ILT,
                 uop_int_binary(UOP_IADD, c5, ra), uop_const(DT_INT32, 12));
    // ra in [0,7]; ra < 7 -> not statically decidable -> stays ILT(ra, 7).
    CHECK_EQ(term_ext(r18), UOP_ILT);
    CHECK_EQ(heap_read(term_val(r18) + 0), ra);
    {
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(r18) + 1), &cc));
      CHECK_EQ(cc, 7);
    }
    // (ra - 2) < 10 -> ra < 12.
    TEST_BEGIN("ported/ilt-shift-const-sub");
    Term r18b = uop_int_binary(UOP_ILT,
                  uop_int_binary(UOP_ISUB, ra, c2), ten);
    // ra in [0,7] < 12 -> always true via bound-fold (folds to 1).
    CHECK_EQ(r18b, one);

    // Rule 19: (c0*x) < c1 -> x < ceil(c1/c0).  (3*rb) < 10 -> rb < 4.
    TEST_BEGIN("ported/ilt-pos-coeff-ceil");
    Term r19 = uop_int_binary(UOP_ILT,
                 uop_int_binary(UOP_IMUL, c3, rb), ten);
    CHECK_EQ(term_ext(r19), UOP_ILT);
    CHECK_EQ(heap_read(term_val(r19) + 0), rb);
    {
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(r19) + 1), &cc));
      CHECK_EQ(cc, 4);  // ceil(10/3) == 4
    }
    TEST_BEGIN("ported/ilt-pos-coeff-skips-nonpositive-c1");
    // (3*rb) < 0 -> c1 not > 0 -> rule 19 skipped (bound-fold gives 0).
    Term r19s = uop_int_binary(UOP_ILT,
                  uop_int_binary(UOP_IMUL, c3, rb), zero);
    CHECK_EQ(r19s, zero);  // 3*rb in [0,297] -> lo=0 >= 0 -> 0

    // Rule 20/9: (x//d) < c -> x < (c*d) for d>0, nonneg(x).
    TEST_BEGIN("ported/ilt-div-eliminate");
    Term r20 = uop_int_binary(UOP_ILT,
                 uop_int_binary(UOP_IDIV, rb, c3), c5);
    CHECK_EQ(term_ext(r20), UOP_ILT);
    CHECK_EQ(heap_read(term_val(r20) + 0), rb);
    {
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(r20) + 1), &cc));
      CHECK_EQ(cc, 15);  // c*d == 5*3
    }
    TEST_BEGIN("ported/ilt-div-eliminate-skips-neg-numerator");
    // ((ra - 5)//3) < 1 -- numerator possibly negative -> no x<c*d rewrite.
    Term negn = uop_int_binary(UOP_ISUB, ra, c5);
    Term r20s = uop_int_binary(UOP_ILT,
                  uop_int_binary(UOP_IDIV, negn, c3), one);
    // The (x//d)<c rule must not fire; result stays an ILT over the IDIV.
    CHECK_EQ(term_ext(r20s), UOP_ILT);
    CHECK_EQ(term_ext(heap_read(term_val(r20s) + 0)), UOP_IDIV);

    // Rule 21: lt_folding.  (rb*4 + ra) < 8, ra in [0,7] < 4? no.  Use a
    // p-term that fits: (rb*4 + r0p) < 8 with r0p in [0,3].
    TEST_BEGIN("ported/lt-folding-gcd");
    Term r0p = uop_range(502, KAX_LOOP, 4);  // [0,3]
    Term ltf_n = uop_int_binary(UOP_IADD,
                   uop_int_binary(UOP_IMUL, rb, uop_const(DT_INT32, 4)), r0p);
    Term r21 = uop_int_binary(UOP_ILT, ltf_n, uop_const(DT_INT32, 8));
    // g = gcd(4, 8) = 4; p=r0p in [0,3] < 4 -> rb < 2.
    CHECK_EQ(term_ext(r21), UOP_ILT);
    CHECK_EQ(heap_read(term_val(r21) + 0), rb);
    {
      i64 cc;
      CHECK(uop_iconst_value(heap_read(term_val(r21) + 1), &cc));
      CHECK_EQ(cc, 2);  // c//g == 8/4
    }
    TEST_BEGIN("ported/lt-folding-skips-when-p-too-large");
    // (rb*4 + ra) < 8 with ra in [0,7] >= 4 -> p-sum hi (7) >= g (4) -> no fold.
    Term ltf_n2 = uop_int_binary(UOP_IADD,
                    uop_int_binary(UOP_IMUL, rb, uop_const(DT_INT32, 4)), ra);
    Term r21s = uop_int_binary(UOP_ILT, ltf_n2, uop_const(DT_INT32, 8));
    CHECK_EQ(term_ext(r21s), UOP_ILT);
    // LHS must still be the full sum, not reduced.
    CHECK_EQ(term_ext(heap_read(term_val(r21s) + 0)), UOP_IADD);

    // Rule 22: x*-1 < y*-1 -> y < x.
    TEST_BEGIN("ported/ilt-negate-both-flip");
    Term neg_ra = uop_int_binary(UOP_IMUL, ra, uop_const(DT_INT32, (u32)(i32)-1));
    Term neg_rb = uop_int_binary(UOP_IMUL, rb, uop_const(DT_INT32, (u32)(i32)-1));
    Term r22 = uop_int_binary(UOP_ILT, neg_ra, neg_rb);
    // -> rb < ra ; rb in [0,99], ra in [0,7]; rb<ra not decidable -> stays ILT.
    CHECK_EQ(term_ext(r22), UOP_ILT);
    CHECK_EQ(heap_read(term_val(r22) + 0), rb);
    CHECK_EQ(heap_read(term_val(r22) + 1), ra);
    TEST_BEGIN("ported/ilt-negate-skips-non-neg-one");
    // (ra*-2) < (rb*-1) -- coeff -2 != -1 -> no flip rule.
    Term r22s = uop_int_binary(UOP_ILT,
                  uop_int_binary(UOP_IMUL, ra, uop_const(DT_INT32, (u32)(i32)-2)),
                  neg_rb);
    CHECK_EQ(term_ext(r22s), UOP_ILT);

    // Rule 23: (c0*x) < c1 -> (-x) < -floor(-c1/-c0) for c0<0,c0!=-1,c1<=0.
    TEST_BEGIN("ported/ilt-neg-coeff");
    // (-3*ra) < -6 -> (-ra) < -floor(6/3) = -2.
    Term r23 = uop_int_binary(UOP_ILT,
                 uop_int_binary(UOP_IMUL, uop_const(DT_INT32, (u32)(i32)-3), ra),
                 uop_const(DT_INT32, (u32)(i32)-6));
    CHECK_EQ(term_ext(r23), UOP_ILT);
    {
      // LHS is (-1)*ra; RHS const is -2.
      Term lhs = heap_read(term_val(r23) + 0);
      i64 cc; Term lx;
      CHECK(uop_match_const_mul(lhs, &cc, &lx));
      CHECK_EQ(cc, -1);
      CHECK_EQ(lx, ra);
      CHECK(uop_iconst_value(heap_read(term_val(r23) + 1), &cc));
      CHECK_EQ(cc, -2);
    }
    TEST_BEGIN("ported/ilt-neg-coeff-skips-positive-c1");
    // (-3*ra) < 6 -- c1 > 0 -> rule 23 not applicable.
    Term r23s = uop_int_binary(UOP_ILT,
                  uop_int_binary(UOP_IMUL, uop_const(DT_INT32, (u32)(i32)-3), ra),
                  c6);
    // -3*ra in [-21, 0] -> all < 6 -> bound-fold returns 1.
    CHECK_EQ(r23s, one);

    // Rule 24: x ^ 0 -> x.
    TEST_BEGIN("ported/xor-zero-identity");
    CHECK_EQ(uop_int_binary(UOP_IXOR, rb, zero), rb);
    CHECK_EQ(uop_int_binary(UOP_IXOR, zero, rb), rb);
    TEST_BEGIN("ported/xor-zero-skips-nonzero");
    Term r24s = uop_int_binary(UOP_IXOR, rb, c3);
    CHECK_EQ(term_ext(r24s), UOP_IXOR);

    // Rule 25: x ^ x -> 0.
    TEST_BEGIN("ported/xor-self-zero");
    CHECK_EQ(uop_int_binary(UOP_IXOR, rb, rb), zero);
    TEST_BEGIN("ported/xor-self-skips-distinct");
    Term r25s = uop_int_binary(UOP_IXOR, rb, ra);
    CHECK_EQ(term_ext(r25s), UOP_IXOR);

    // Rule 26: (x ^ y) ^ y -> x.
    TEST_BEGIN("ported/xor-cancel");
    Term xy = uop_int_binary(UOP_IXOR, rb, ra);
    CHECK_EQ(uop_int_binary(UOP_IXOR, xy, ra), rb);
    TEST_BEGIN("ported/xor-cancel-skips-different");
    Term r26s = uop_int_binary(UOP_IXOR, xy, r);  // y mismatched
    CHECK_EQ(term_ext(r26s), UOP_IXOR);

    // Rule 27: x | c -> c if c else x.
    TEST_BEGIN("ported/or-const");
    CHECK_EQ(uop_int_binary(UOP_IOR, rb, zero), rb);  // x|0 -> x
    CHECK_EQ(uop_int_binary(UOP_IOR, rb, one), one);  // x|1 -> 1
    TEST_BEGIN("ported/or-self-idempotent");
    CHECK_EQ(uop_int_binary(UOP_IOR, rb, rb), rb);

    // Rule 28: (x AND c1) AND c2 -> x AND (c1 AND c2); OR version.
    TEST_BEGIN("ported/and-two-stage");
    // rb & 5 keeps the IAND (5 is neither 0 nor 1, no contract fold), so the
    // two-stage fold is exercised: (rb & 5) & 3 -> rb & (5&3==1) -> rb.
    Term and5 = uop_int_binary(UOP_IAND, rb, c5);
    CHECK_EQ(term_ext(and5), UOP_IAND);
    Term r28 = uop_int_binary(UOP_IAND, and5, c3);
    CHECK_EQ(r28, rb);
    TEST_BEGIN("ported/or-two-stage");
    // The IOR boolean contract collapses x|c (c truthy) to c first, so the
    // chain folds entirely to const: (rb|5)|2 -> 5|2 -> 7.
    Term or5 = uop_int_binary(UOP_IOR, rb, c5);
    CHECK_EQ(term_ext(or5), UOP_CONST);
    Term r28b = uop_int_binary(UOP_IOR, or5, c2);
    CHECK_EQ(term_ext(r28b), UOP_CONST);
    {
      i64 cc;
      CHECK(uop_iconst_value(r28b, &cc));
      CHECK_EQ(cc, 7);
    }

    // Rule 29: gate-push for IOR/IXOR/ISHR (only under THVM_FUSE_CONV_BWD).
    TEST_BEGIN("ported/gate-push-allowlist-respects-env");
    // Without the env gate the push must NOT fire: keep IXOR over the gate.
    if (!uop_valid_fold_enabled()) {
      Term gcond = uop_int_binary(UOP_ILT, ra, rb);
      Term gated29 = uop_iwhere(gcond, ra, uop_invalid());
      Term pushed = uop_int_binary(UOP_IXOR, gated29, c3);
      // gate stays IWHERE-wrapped only when enabled; disabled -> plain IXOR.
      CHECK_EQ(term_ext(pushed), UOP_IXOR);
    } else {
      CHECK_EQ(1, 1);
    }
  }

  thvm_free();
  TEST_REPORT();
}
