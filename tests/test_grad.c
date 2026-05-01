// test_grad.c - dup-style grad projection and chain-rule smoke tests.
//
// End-to-end numerical grad coverage lives in wl/THVMLink/Tests/grad.wlt.
// This C test keeps the low-level runtime contract honest: grad cells are
// DP projections with DUP_GRAD_FLAG, no-target leaf rules emit SUP routing,
// and the first structural adjoints have the expected outer shape.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static int is_grad_dp(Term t, u8 tag) {
  return term_tag(t) == tag && (term_ext(t) & DUP_GRAD_FLAG) != 0;
}

static Term same_cell_fwd(Term bwd) {
  return term_new(0, TAG_DP0, DUP_GRAD_FLAG, term_val(bwd));
}

static Term sup_zero(Term sup) {
  return heap_read(term_val(sup) + 0);
}

static Term sup_grad(Term sup) {
  return heap_read(term_val(sup) + 1);
}

static void check_leaf_sup(Term sup, Term leaf) {
  Term leaf_r = term_resolve(leaf);
  CHECK_EQ(term_tag(sup), TAG_SUP);
  CHECK_EQ(term_ext(sup), term_val(leaf_r));
  CHECK_EQ(term_ext(sup_zero(sup)), UOP_CONST);
}

int main(void) {
  thvm_init();

  u32 d3[1] = {3};
  u32 ta = alloc_f32_tensor(d3, 1);
  u32 tb = alloc_f32_tensor(d3, 1);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term gy_scalar = uop_const(DT_FP32, 0x40000000u);
  Term gy = uop_expand(gy_scalar, 1, d3);

  TEST_BEGIN("grad/projection-layout");
  Term bwd = uop_grad(a, gy);
  CHECK(is_grad_dp(bwd, TAG_DP1));
  CHECK_EQ(heap_read(term_val(bwd) + 0), a);
  CHECK_EQ(heap_read(term_val(bwd) + 1), gy);
  CHECK_EQ(heap_read(term_val(bwd) + 2), 0u);

  TEST_BEGIN("grad/fwd-projection-shares-cell");
  Term fwd = same_cell_fwd(bwd);
  CHECK(is_grad_dp(fwd, TAG_DP0));
  CHECK_EQ(term_val(fwd), term_val(bwd));
  CHECK_EQ(wnf(fwd), a);

  TEST_BEGIN("grad/explicit-target-cell-layout");
  Term tgt = uop_grad_with_target(a, gy, a);
  CHECK(is_grad_dp(tgt, TAG_DP1));
  CHECK_EQ(heap_read(term_val(tgt) + 0), a);
  CHECK_EQ(heap_read(term_val(tgt) + 1), gy);
  CHECK_EQ(heap_read(term_val(tgt) + 2), a);

  TEST_BEGIN("grad/explicit-target-leaf-match");
  CHECK_EQ(wnf(tgt), gy);

  TEST_BEGIN("grad/no-target-leaf-emits-sup");
  Term leaf = wnf(uop_grad(a, gy));
  check_leaf_sup(leaf, a);
  CHECK_EQ(sup_grad(leaf), gy);

  TEST_BEGIN("grad/no-target-add-splits-to-leaf-sups");
  Term sum = uop_binary(UOP_ADD, a, b);
  Term g_add = wnf(uop_grad(sum, gy));
  CHECK_EQ(term_ext(g_add), UOP_ADD);
  Term add_l = heap_read(term_val(g_add) + 0);
  Term add_r = heap_read(term_val(g_add) + 1);
  check_leaf_sup(add_l, a);
  check_leaf_sup(add_r, b);
  CHECK_EQ(sup_grad(add_l), gy);
  CHECK_EQ(sup_grad(add_r), gy);

  TEST_BEGIN("grad/no-target-mul-uses-product-rule");
  Term mul = uop_binary(UOP_MUL, a, b);
  Term g_mul = wnf(uop_grad(mul, gy));
  CHECK_EQ(term_ext(g_mul), UOP_ADD);
  Term mul_l = heap_read(term_val(g_mul) + 0);
  Term mul_r = heap_read(term_val(g_mul) + 1);
  check_leaf_sup(mul_l, a);
  check_leaf_sup(mul_r, b);
  CHECK_EQ(term_ext(sup_grad(mul_l)), UOP_MUL);
  CHECK_EQ(term_ext(sup_grad(mul_r)), UOP_MUL);

  TEST_BEGIN("grad/no-target-neg-wraps-cotangent");
  Term neg = uop_unary(UOP_NEG, a);
  Term g_neg = wnf(uop_grad(neg, gy));
  check_leaf_sup(g_neg, a);
  CHECK_EQ(term_ext(sup_grad(g_neg)), UOP_NEG);

  TEST_BEGIN("grad/no-target-reduce-sum-expands-cotangent");
  Term red = uop_reduce(REDUCE_SUM, 0, a);
  Term g_red = wnf(uop_grad(red, gy));
  check_leaf_sup(g_red, a);
  CHECK_EQ(term_ext(sup_grad(g_red)), UOP_EXPAND);

  TEST_BEGIN("grad/no-target-reshape-passes-through-shape");
  u32 d31[2] = {3, 1};
  Term rs = uop_reshape(a, 2, d31);
  Term g_rs = wnf(uop_grad(rs, gy));
  check_leaf_sup(g_rs, a);
  CHECK_EQ(term_ext(sup_grad(g_rs)), UOP_RESHAPE);

  TEST_BEGIN("grad/no-target-expand-reduces-cotangent");
  u32 d1[1] = {1};
  u32 t1 = alloc_f32_tensor(d1, 1);
  Term a1 = term_new(0, TAG_TEN, DT_FP32, t1);
  Term ex = uop_expand(a1, 1, d3);
  Term g_ex = wnf(uop_grad(ex, gy));
  check_leaf_sup(g_ex, a1);
  CHECK_EQ(term_ext(sup_grad(g_ex)), UOP_RESHAPE);

  TEST_BEGIN("grad/no-target-shrink-pads-cotangent");
  u32 d5[1] = {5};
  u32 t5 = alloc_f32_tensor(d5, 1);
  Term a5 = term_new(0, TAG_TEN, DT_FP32, t5);
  u32 sh[2] = {1, 4};
  Term sk = uop_shrink(a5, 1, sh);
  Term g_sk = wnf(uop_grad(sk, gy));
  check_leaf_sup(g_sk, a5);
  CHECK_EQ(term_ext(sup_grad(g_sk)), UOP_PAD);

  TEST_BEGIN("grad/no-target-pad-shrinks-cotangent");
  u32 pd[2] = {1, 1};
  Term pad = uop_pad(a, 1, pd);
  Term g_pad = wnf(uop_grad(pad, gy));
  check_leaf_sup(g_pad, a);
  CHECK_EQ(term_ext(sup_grad(g_pad)), UOP_SHRINK);

  TEST_BEGIN("grad/no-target-flip-is-involutive");
  Term fl = uop_flip(a, 1u);
  Term g_fl = wnf(uop_grad(fl, gy));
  check_leaf_sup(g_fl, a);
  CHECK_EQ(term_ext(sup_grad(g_fl)), UOP_FLIP);

  TEST_BEGIN("grad/no-target-permute-inverts-axes");
  u32 d23[2] = {2, 3};
  u32 t23 = alloc_f32_tensor(d23, 2);
  Term a23 = term_new(0, TAG_TEN, DT_FP32, t23);
  u32 perm[2] = {1, 0};
  Term pm = uop_permute(a23, 2, perm);
  Term g_pm = wnf(uop_grad(pm, gy));
  check_leaf_sup(g_pm, a23);
  CHECK_EQ(term_ext(sup_grad(g_pm)), UOP_PERMUTE);

  TEST_BEGIN("grad/no-target-cmplt-is-zero");
  Term cmp = uop_binary(UOP_CMPLT, a, b);
  Term g_cmp = wnf(uop_grad(cmp, gy));
  CHECK_EQ(term_ext(g_cmp), UOP_CONST);

  TEST_BEGIN("grad/no-target-const-is-zero");
  Term cnst = uop_const(DT_FP32, 0x40400000u);
  Term g_cnst = wnf(uop_grad(cnst, gy));
  CHECK_EQ(term_ext(g_cnst), UOP_CONST);

  thvm_free();
  TEST_REPORT();
}
