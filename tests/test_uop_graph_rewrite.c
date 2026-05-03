// test_uop_graph_rewrite.c - named bottom-up UOp graph rewrite pass.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static Term strip_neg(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_NEG) {
    return 0;
  }
  return heap_read(term_val(t));
}

static Term raw_unary(u32 opcode, Term src) {
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  return term_new(0, TAG_UOP, opcode, loc);
}

static Term raw_binary(u32 opcode, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_UOP, opcode, loc);
}

static Term raw_reshape(Term src, u32 ndim, const u32 *dims) {
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, dims[i]));
  }
  return term_new(0, TAG_UOP, UOP_RESHAPE, loc);
}

static Term raw_dim_movement(u32 opcode, Term src, u32 ndim, const u32 *vals) {
  u64 loc = heap_alloc(2 + ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, vals[i]));
  }
  return term_new(0, TAG_UOP, opcode, loc);
}

static Term raw_bounds_movement(u32 opcode, Term src, u32 ndim, const u32 *vals) {
  u64 loc = heap_alloc(2 + 2 * ndim);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, ndim));
  for (u32 i = 0; i < 2 * ndim; i++) {
    heap_set(loc + 2 + i, term_new(0, TAG_NUM, DT_INT32, vals[i]));
  }
  return term_new(0, TAG_UOP, opcode, loc);
}

static Term raw_with_num_arg(u32 opcode, Term src, u32 val) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, val));
  return term_new(0, TAG_UOP, opcode, loc);
}

static int test_shape_equal(Shape const *a, Shape const *b) {
  if (a->ndim != b->ndim) {
    return 0;
  }
  for (u32 i = 0; i < a->ndim; i++) {
    if (a->dims[i] != b->dims[i]) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("uop-graph-rewrite/bottom-up-rebuilds-parent");
  u32 ta = alloc_f32_tensor(4);
  u32 tb = alloc_f32_tensor(4);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term neg = uop_unary(UOP_NEG, a);
  Term root = uop_binary(UOP_MUL, neg, b);
  UOpGraphRewriteRule rules[] = {{"strip-neg", strip_neg}};
  Term out = uop_graph_rewrite(root, rules, 1, NULL);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_MUL);
  CHECK_EQ(heap_read(term_val(out) + 0), a);
  CHECK_EQ(heap_read(term_val(out) + 1), b);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-neg"), 1);

  TEST_BEGIN("uop-graph-rewrite/memoizes-shared-subgraph");
  u32 tc = alloc_f32_tensor(4);
  Term c = term_new(0, TAG_TEN, DT_FP32, tc);
  Term shared_neg = uop_unary(UOP_NEG, a);
  Term left = uop_binary(UOP_MUL, shared_neg, b);
  Term right = uop_binary(UOP_MUL, shared_neg, c);
  Term sum = uop_binary(UOP_ADD, left, right);
  Term sum_out = uop_graph_rewrite(sum, rules, 1, NULL);
  CHECK_EQ(term_ext(sum_out), UOP_ADD);
  Term left_out = heap_read(term_val(sum_out) + 0);
  Term right_out = heap_read(term_val(sum_out) + 1);
  CHECK_EQ(heap_read(term_val(left_out) + 0), a);
  CHECK_EQ(heap_read(term_val(right_out) + 0), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-neg"), 1);

  TEST_BEGIN("uop-graph-rewrite/preserves-movement-metadata");
  u32 widths[2] = {1, 2};
  Term pad = uop_pad(uop_unary(UOP_NEG, a), 1, widths);
  Term pad_out = uop_graph_rewrite(pad, rules, 1, NULL);
  CHECK_EQ(term_tag(pad_out), TAG_UOP);
  CHECK_EQ(term_ext(pad_out), UOP_PAD);
  u64 ploc = term_val(pad_out);
  CHECK_EQ(heap_read(ploc + 0), a);
  CHECK_EQ(term_val(heap_read(ploc + 1)), 1);
  CHECK_EQ(term_val(heap_read(ploc + 2)), 1);
  CHECK_EQ(term_val(heap_read(ploc + 3)), 2);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-neg"), 1);

  TEST_BEGIN("uop-view/exposes-op-and-sources");
  UOpView view;
  CHECK(uop_view(root, &view));
  CHECK_EQ(view.term, root);
  CHECK_EQ(view.op, UOP_MUL);
  CHECK_EQ(view.arity, 2);
  CHECK_EQ(uop_view_src(&view, 0), neg);
  CHECK_EQ(uop_view_src(&view, 1), b);
  CHECK(uop_view_op(root, UOP_MUL, NULL));
  CHECK(!uop_view_op(root, UOP_ADD, NULL));
  CHECK(uop_is_movement(UOP_PAD));
  CHECK(!uop_is_movement(UOP_ADD));

  TEST_BEGIN("uop-graph-simplify/applies-symbolic-binary");
  Term zero = uop_const(DT_FP32, f32_bits(0.0f));
  Term raw_add = raw_binary(UOP_ADD, a, zero);
  Term add_out = uop_graph_simplify(raw_add);
  CHECK_EQ(add_out, a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("symbolic-binary"), 1);

  TEST_BEGIN("uop-graph-simplify/applies-symbolic-unary");
  Term raw_neg = raw_unary(UOP_NEG, raw_unary(UOP_NEG, a));
  Term neg_out = uop_graph_simplify(raw_neg);
  CHECK_EQ(neg_out, a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("symbolic-unary"), 1);

  TEST_BEGIN("uop-graph-simplify/collapses-movement-chain");
  u32 dims_inner[2] = {2, 2};
  u32 dims_outer[1] = {4};
  Term raw_chain = raw_reshape(raw_reshape(a, 2, dims_inner), 1, dims_outer);
  Term chain_out = uop_graph_simplify(raw_chain);
  CHECK_EQ(chain_out, a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-chain-collapse"), 1);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);

  TEST_BEGIN("uop-graph-simplify/drops-identity-movement");
  u32 dims_a[1] = {4};
  u32 perm_id[1] = {0};
  u32 pad_zero[2] = {0, 0};
  u32 shrink_full[2] = {0, 4};
  CHECK_EQ(uop_graph_simplify(raw_reshape(a, 1, dims_a)), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);
  CHECK_EQ(uop_graph_simplify(raw_dim_movement(UOP_EXPAND, a, 1, dims_a)), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);
  CHECK_EQ(uop_graph_simplify(raw_dim_movement(UOP_PERMUTE, a, 1, perm_id)), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);
  CHECK_EQ(uop_graph_simplify(raw_bounds_movement(UOP_PAD, a, 1, pad_zero)), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);
  CHECK_EQ(uop_graph_simplify(raw_bounds_movement(UOP_SHRINK, a, 1, shrink_full)), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);
  CHECK_EQ(uop_graph_simplify(raw_with_num_arg(UOP_FLIP, a, 0)), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-identity"), 1);

  TEST_BEGIN("uop-graph-simplify/canonicalizes-commutative-operands");
  Term two = uop_const(DT_FP32, f32_bits(2.0f));
  Term raw_mul_const_left = raw_binary(UOP_MUL, two, a);
  Term mul_const_right = uop_graph_simplify(raw_mul_const_left);
  CHECK_EQ(term_tag(mul_const_right), TAG_UOP);
  CHECK_EQ(term_ext(mul_const_right), UOP_MUL);
  CHECK_EQ(heap_read(term_val(mul_const_right) + 0), a);
  CHECK_EQ(heap_read(term_val(mul_const_right) + 1), two);
  CHECK_EQ(uop_graph_rewrite_stat_hits("commutative-canonicalize"), 1);

  TEST_BEGIN("uop-graph-simplify/reassociates-constant-operands");
  Term three = uop_const(DT_FP32, f32_bits(3.0f));
  Term nested_add = raw_binary(UOP_ADD,
                               raw_binary(UOP_ADD, a, two),
                               three);
  Term reassoc = uop_graph_simplify(nested_add);
  CHECK_EQ(term_tag(reassoc), TAG_UOP);
  CHECK_EQ(term_ext(reassoc), UOP_ADD);
  CHECK_EQ(heap_read(term_val(reassoc) + 0), a);
  Term five = heap_read(term_val(reassoc) + 1);
  CHECK_EQ(term_tag(five), TAG_UOP);
  CHECK_EQ(term_ext(five), UOP_CONST);
  CHECK_EQ(term_val(heap_read(term_val(five))), f32_bits(5.0f));
  CHECK_EQ(uop_graph_rewrite_stat_hits("symbolic-reassociate-const"), 1);

  TEST_BEGIN("uop-graph-simplify/composes-pad-shrink-flip");
  u32 pad_inner[4] = {1, 2, 0, 3};
  u32 pad_outer[4] = {4, 5, 6, 7};
  Term pad_chain = raw_bounds_movement(
      UOP_PAD,
      raw_bounds_movement(UOP_PAD, a, 2, pad_inner),
      2,
      pad_outer);
  Term pad_folded = uop_graph_simplify(pad_chain);
  CHECK_EQ(term_tag(pad_folded), TAG_UOP);
  CHECK_EQ(term_ext(pad_folded), UOP_PAD);
  CHECK_EQ(heap_read(term_val(pad_folded)), a);
  CHECK_EQ(term_val(heap_read(term_val(pad_folded) + 2)), 5);
  CHECK_EQ(term_val(heap_read(term_val(pad_folded) + 3)), 7);
  CHECK_EQ(term_val(heap_read(term_val(pad_folded) + 4)), 6);
  CHECK_EQ(term_val(heap_read(term_val(pad_folded) + 5)), 10);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-chain-collapse"), 1);

  u32 shrink_inner[4] = {2, 8, 1, 7};
  u32 shrink_outer[4] = {1, 4, 2, 5};
  Term shrink_chain = raw_bounds_movement(
      UOP_SHRINK,
      raw_bounds_movement(UOP_SHRINK, a, 2, shrink_inner),
      2,
      shrink_outer);
  Term shrink_folded = uop_graph_simplify(shrink_chain);
  CHECK_EQ(term_tag(shrink_folded), TAG_UOP);
  CHECK_EQ(term_ext(shrink_folded), UOP_SHRINK);
  CHECK_EQ(heap_read(term_val(shrink_folded)), a);
  CHECK_EQ(term_val(heap_read(term_val(shrink_folded) + 2)), 3);
  CHECK_EQ(term_val(heap_read(term_val(shrink_folded) + 3)), 6);
  CHECK_EQ(term_val(heap_read(term_val(shrink_folded) + 4)), 3);
  CHECK_EQ(term_val(heap_read(term_val(shrink_folded) + 5)), 6);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-chain-collapse"), 1);

  Term flip_chain = raw_with_num_arg(
      UOP_FLIP,
      raw_with_num_arg(UOP_FLIP, a, 3),
      2);
  Term flip_folded = uop_graph_simplify(flip_chain);
  CHECK_EQ(term_tag(flip_folded), TAG_UOP);
  CHECK_EQ(term_ext(flip_folded), UOP_FLIP);
  CHECK_EQ(heap_read(term_val(flip_folded)), a);
  CHECK_EQ(term_val(heap_read(term_val(flip_folded) + 1)), 1);
  CHECK_EQ(uop_graph_simplify(raw_with_num_arg(
      UOP_FLIP,
      raw_with_num_arg(UOP_FLIP, a, 3),
      3)), a);

  TEST_BEGIN("uop-graph-simplify/folds-identity-cast");
  Term raw_cast = raw_with_num_arg(UOP_CAST, a, DT_FP32);
  Term cast_out = uop_graph_simplify(raw_cast);
  CHECK_EQ(cast_out, a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("symbolic-cast"), 1);

  TEST_BEGIN("uop-graph-simplify/folds-nested-bitcast");
  Term raw_bitcast = raw_with_num_arg(UOP_BITCAST,
                                      raw_with_num_arg(UOP_BITCAST, a, DT_INT32),
                                      DT_FP32);
  Term bitcast_out = uop_graph_simplify_checked(raw_bitcast, 0);
  CHECK_EQ(bitcast_out, a);
  CHECK(uop_graph_rewrite_stat_hits("symbolic-cast") >= 1);

  TEST_BEGIN("uop-graph-simplify-checked/accepts-shape-dtype-stable");
  Term checked_add = uop_graph_simplify_checked(raw_add, 0);
  CHECK_EQ(checked_add, a);
  Shape raw_add_shape;
  Shape checked_add_shape;
  CHECK(term_shape_in(raw_add, 0, &raw_add_shape));
  CHECK(term_shape_in(checked_add, 0, &checked_add_shape));
  CHECK(test_shape_equal(&raw_add_shape, &checked_add_shape));
  u32 raw_add_dtype;
  u32 checked_add_dtype;
  CHECK(term_dtype_in(raw_add, 0, &raw_add_dtype));
  CHECK(term_dtype_in(checked_add, 0, &checked_add_dtype));
  CHECK_EQ(raw_add_dtype, checked_add_dtype);

  TEST_BEGIN("uop-graph-simplify-checked/rejects-shape-changing-rule");
  Term raw_eq = raw_binary(UOP_CMPEQ, a, a);
  Term eq_unchecked = uop_graph_simplify(raw_eq);
  CHECK_EQ(term_tag(eq_unchecked), TAG_UOP);
  CHECK_EQ(term_ext(eq_unchecked), UOP_CONST);
  Term eq_checked = uop_graph_simplify_checked(raw_eq, 0);
  CHECK_EQ(eq_checked, raw_eq);

  TEST_BEGIN("uop-graph-simplify/reduce-const-mul-distributes-rhs");
  Term mul_const_rhs = raw_binary(UOP_MUL, a, two);
  Term reduce_mul_rhs = uop_reduce(REDUCE_SUM, 0, mul_const_rhs);
  Term reduce_mul_rhs_out = uop_graph_simplify(reduce_mul_rhs);
  CHECK_EQ(term_tag(reduce_mul_rhs_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_mul_rhs_out), UOP_MUL);
  Term mul_lhs = heap_read(term_val(reduce_mul_rhs_out) + 0);
  Term mul_rhs = heap_read(term_val(reduce_mul_rhs_out) + 1);
  CHECK_EQ(term_tag(mul_lhs), TAG_UOP);
  CHECK_EQ(term_ext(mul_lhs), UOP_REDUCE);
  CHECK_EQ(heap_read(term_val(mul_lhs)), a);
  CHECK_EQ(mul_rhs, two);
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-const-mul-distribute"), 1);

  TEST_BEGIN("uop-graph-simplify/reduce-const-mul-distributes-lhs");
  Term mul_const_lhs = raw_binary(UOP_MUL, two, a);
  Term reduce_mul_lhs = uop_reduce(REDUCE_SUM, 0, mul_const_lhs);
  Term reduce_mul_lhs_out = uop_graph_simplify(reduce_mul_lhs);
  CHECK_EQ(term_tag(reduce_mul_lhs_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_mul_lhs_out), UOP_MUL);
  Term lhs2 = heap_read(term_val(reduce_mul_lhs_out) + 0);
  Term rhs2 = heap_read(term_val(reduce_mul_lhs_out) + 1);
  CHECK_EQ(term_tag(lhs2), TAG_UOP);
  CHECK_EQ(term_ext(lhs2), UOP_REDUCE);
  CHECK_EQ(heap_read(term_val(lhs2)), a);
  CHECK_EQ(rhs2, two);
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-const-mul-distribute"), 1);

  TEST_BEGIN("uop-graph-simplify/reduce-max-mul-const-stays");
  Term mul_for_max = raw_binary(UOP_MUL, a, two);
  Term reduce_max_mul = uop_reduce(REDUCE_MAX, 0, mul_for_max);
  Term reduce_max_mul_out = uop_graph_simplify(reduce_max_mul);
  CHECK_EQ(term_tag(reduce_max_mul_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_max_mul_out), UOP_REDUCE);
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-const-mul-distribute"), 0);

  TEST_BEGIN("uop-graph-simplify/reduce-mul-non-const-stays");
  u32 td = alloc_f32_tensor(4);
  Term d = term_new(0, TAG_TEN, DT_FP32, td);
  Term mul_non_const = raw_binary(UOP_MUL, a, d);
  Term reduce_mul_non_const = uop_reduce(REDUCE_SUM, 0, mul_non_const);
  Term reduce_mul_non_const_out = uop_graph_simplify(reduce_mul_non_const);
  CHECK_EQ(term_tag(reduce_mul_non_const_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_mul_non_const_out), UOP_REDUCE);
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-const-mul-distribute"), 0);

  TEST_BEGIN("uop-graph-simplify/reduce-add-const-distributes");
  Term add_const = raw_binary(UOP_ADD, a, two);
  Term reduce_add_const = uop_reduce(REDUCE_SUM, 0, add_const);
  Term reduce_add_const_out = uop_graph_simplify(reduce_add_const);
  CHECK_EQ(term_tag(reduce_add_const_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_add_const_out), UOP_ADD);
  Term add_lhs = heap_read(term_val(reduce_add_const_out) + 0);
  Term add_rhs = heap_read(term_val(reduce_add_const_out) + 1);
  CHECK_EQ(term_tag(add_lhs), TAG_UOP);
  CHECK_EQ(term_ext(add_lhs), UOP_REDUCE);
  CHECK_EQ(heap_read(term_val(add_lhs)), a);
  // a has shape {4}, so 2.0 * 4 = 8.0.
  CHECK_EQ(term_tag(add_rhs), TAG_UOP);
  CHECK_EQ(term_ext(add_rhs), UOP_CONST);
  CHECK_EQ(term_val(heap_read(term_val(add_rhs))), f32_bits(8.0f));
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-add-const-distribute"), 1);

  TEST_BEGIN("uop-graph-simplify/reduce-max-add-const-stays");
  Term add_for_max = raw_binary(UOP_ADD, a, two);
  Term reduce_max_add = uop_reduce(REDUCE_MAX, 0, add_for_max);
  Term reduce_max_add_out = uop_graph_simplify(reduce_max_add);
  CHECK_EQ(term_tag(reduce_max_add_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_max_add_out), UOP_REDUCE);
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-add-const-distribute"), 0);

  TEST_BEGIN("uop-graph-simplify/reduce-add-non-const-stays");
  Term te2 = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(4));
  Term add_non_const = raw_binary(UOP_ADD, a, te2);
  Term reduce_add_non_const = uop_reduce(REDUCE_SUM, 0, add_non_const);
  Term reduce_add_non_const_out = uop_graph_simplify(reduce_add_non_const);
  CHECK_EQ(term_tag(reduce_add_non_const_out), TAG_UOP);
  CHECK_EQ(term_ext(reduce_add_non_const_out), UOP_REDUCE);
  CHECK_EQ(uop_graph_rewrite_stat_hits("reduce-add-const-distribute"), 0);

  TEST_BEGIN("uop-graph-simplify/collect-mul-add-shared-x");
  // x*c0 + x*c1 -> x*(c0+c1).  c0=2.0, c1=3.0.
  Term three_c = uop_const(DT_FP32, f32_bits(3.0f));
  Term x_times_2 = raw_binary(UOP_MUL, a, two);
  Term x_times_3 = raw_binary(UOP_MUL, a, three_c);
  Term sum_two_threes = raw_binary(UOP_ADD, x_times_2, x_times_3);
  Term collected = uop_graph_simplify(sum_two_threes);
  CHECK_EQ(term_tag(collected), TAG_UOP);
  CHECK_EQ(term_ext(collected), UOP_MUL);
  Term coll_lhs = heap_read(term_val(collected) + 0);
  Term coll_rhs = heap_read(term_val(collected) + 1);
  CHECK_EQ(coll_lhs, a);
  CHECK_EQ(term_tag(coll_rhs), TAG_UOP);
  CHECK_EQ(term_ext(coll_rhs), UOP_CONST);
  CHECK_EQ(term_val(heap_read(term_val(coll_rhs))), f32_bits(5.0f));
  CHECK(uop_graph_rewrite_stat_hits("collect-mul-add") >= 1);

  TEST_BEGIN("uop-graph-simplify/collect-mul-add-x-plus-x");
  // x + x -> x*2.
  Term x_plus_x = raw_binary(UOP_ADD, a, a);
  Term doubled = uop_graph_simplify(x_plus_x);
  CHECK_EQ(term_tag(doubled), TAG_UOP);
  CHECK_EQ(term_ext(doubled), UOP_MUL);
  Term dbl_rhs = heap_read(term_val(doubled) + 1);
  CHECK_EQ(term_val(heap_read(term_val(dbl_rhs))), f32_bits(2.0f));
  CHECK(uop_graph_rewrite_stat_hits("collect-mul-add") >= 1);

  TEST_BEGIN("uop-graph-simplify/collect-mul-add-x-plus-x-times-c");
  // x + x*3 -> x*4.
  Term x_plus_3x = raw_binary(UOP_ADD, a, x_times_3);
  Term coalesced = uop_graph_simplify(x_plus_3x);
  CHECK_EQ(term_tag(coalesced), TAG_UOP);
  CHECK_EQ(term_ext(coalesced), UOP_MUL);
  Term co_rhs = heap_read(term_val(coalesced) + 1);
  CHECK_EQ(term_val(heap_read(term_val(co_rhs))), f32_bits(4.0f));
  CHECK(uop_graph_rewrite_stat_hits("collect-mul-add") >= 1);

  TEST_BEGIN("uop-graph-simplify/collect-mul-add-distinct-operands-stay");
  // x*2 + y*3 -> stays (different operands).
  Term y = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(4));
  Term y_times_3 = raw_binary(UOP_MUL, y, three_c);
  Term mixed = raw_binary(UOP_ADD, x_times_2, y_times_3);
  Term mixed_out = uop_graph_simplify(mixed);
  CHECK_EQ(term_tag(mixed_out), TAG_UOP);
  CHECK_EQ(term_ext(mixed_out), UOP_ADD);

  TEST_BEGIN("uop-graph-simplify-materialize/opt-in-hook");
  setenv("THVM_UOP_GRAPH_SIMPLIFY", "1", 1);
  Term mat_candidate = raw_binary(UOP_ADD, a, zero);
  Term mat_simplified = uop_graph_simplify_materialize(mat_candidate, 0);
  CHECK_EQ(mat_simplified, a);
  Term mat_out = thvm_materialize(mat_candidate);
  CHECK_EQ(mat_out, a);

  thvm_free();
  TEST_REPORT();
}
