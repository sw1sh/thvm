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
