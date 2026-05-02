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
  CHECK_EQ(term_tag(chain_out), TAG_UOP);
  CHECK_EQ(term_ext(chain_out), UOP_RESHAPE);
  u64 cloc = term_val(chain_out);
  CHECK_EQ(heap_read(cloc + 0), a);
  CHECK_EQ(term_val(heap_read(cloc + 1)), 1);
  CHECK_EQ(term_val(heap_read(cloc + 2)), 4);
  CHECK_EQ(uop_graph_rewrite_stat_hits("movement-chain-collapse"), 1);

  thvm_free();
  TEST_REPORT();
}
