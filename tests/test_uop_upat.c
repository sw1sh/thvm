// test_uop_upat.c - smoke test for the declarative UPat layer.
//
// Three checks:
//   1. upat_match accepts/rejects expected shapes (op + arity gates,
//      bindings populate).
//   2. uop_pattern_rewrite drives a single rule end-to-end --
//      UOP_NEG(x) -> x, replicating the imperative `strip_neg` rule
//      from test_uop_graph_rewrite.c but expressed declaratively.
//   3. A two-rule table where the first rule rejects and the second
//      fires -- exercises the rule-iteration path of the bridge.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor_1d(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

// Rule: UOP_NEG(?x) -> x.
static Term rw_strip_neg(Term const *bindings, void *ctx) {
  (void)ctx;
  return bindings[0];
}

// Rule: UOP_ADD(?a, ?b) where a == b -> a*2.  This is broader than
// the existing collect-mul-add rule but is a separate pattern -- the
// point here is the bridge, not the algebra.
static Term rw_add_self_to_mul2(Term const *bindings, void *ctx) {
  (void)ctx;
  Term a = bindings[0];
  Term b = bindings[1];
  if (a != b) return 0;
  Term two = uop_const(DT_FP32, f32_bits(2.0f));
  return uop_binary(UOP_MUL, a, two);
}

int main(void) {
  thvm_init();

  // --- (1) upat_match basics --------------------------------------
  TEST_BEGIN("upat/match-op-and-binding");
  u32 ta = alloc_f32_tensor_1d(4);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term neg_a = uop_unary(UOP_NEG, a);

  // pattern: UOP_NEG(?0) -- bind the inner term to slot 0.
  static UPat const wild_leaf = {0, 0, 0, 0, NULL};
  static UPat const pat_neg   = {UOP_NEG, 1, 0, -1, &wild_leaf};
  Term bindings[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_neg, neg_a, bindings));
  CHECK_EQ(bindings[0], a);

  TEST_BEGIN("upat/match-rejects-wrong-op");
  u32 tb = alloc_f32_tensor_1d(4);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term mul_ab = uop_binary(UOP_MUL, a, b);
  Term bindings2[UPAT_NUM_BINDINGS] = {0};
  CHECK(!upat_match(&pat_neg, mul_ab, bindings2));

  TEST_BEGIN("upat/match-any-op-wildcard");
  // pattern: any op, any arity -- accepts both UOp and leaf.
  static UPat const pat_any = {0, 0xFF, 0, 0, NULL};
  Term bindings3[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_any, mul_ab, bindings3));
  CHECK_EQ(bindings3[0], mul_ab);
  Term bindings4[UPAT_NUM_BINDINGS] = {0};
  // any-op with nsrc=0 also matches a non-UOp leaf (per upat.c).
  static UPat const pat_leaf = {0, 0, 0, 0, NULL};
  CHECK(upat_match(&pat_leaf, a, bindings4));
  CHECK_EQ(bindings4[0], a);

  // --- (2) end-to-end: strip-neg as a UPatRule --------------------
  TEST_BEGIN("upat/rewrite-strip-neg-via-bridge");
  Term root = uop_binary(UOP_MUL, neg_a, b);
  UPatRule rules[] = {{&pat_neg, rw_strip_neg}};
  Term out = uop_pattern_rewrite(root, rules, 1, NULL);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_MUL);
  // After the rewrite, the outer MUL has been rebuilt with `a` in
  // place of the stripped neg(a).
  CHECK_EQ(heap_read(term_val(out) + 0), a);
  CHECK_EQ(heap_read(term_val(out) + 1), b);
  CHECK(uop_graph_rewrite_stat_hits("upat-bridge") >= 1);

  // --- (3) two-rule bridge: first rejects, second fires ----------
  TEST_BEGIN("upat/two-rule-bridge-second-fires");
  // x + x -> x*2.  The first rule (strip_neg) won't match an ADD;
  // the second (add-self) will.
  static UPat const pat_add_children[2] = {
    {0, 0xFF, 0, 0, NULL},   // ?a (any UOp, captured as bindings[0])
    {0, 0xFF, 0, 1, NULL},   // ?b (any UOp, captured as bindings[1])
  };
  static UPat const pat_add = {UOP_ADD, 2, 0, -1, pat_add_children};
  UPatRule rules2[] = {
    {&pat_neg, rw_strip_neg},
    {&pat_add, rw_add_self_to_mul2},
  };
  Term x_plus_x = uop_binary(UOP_ADD, a, a);
  Term doubled = uop_pattern_rewrite(x_plus_x, rules2, 2, NULL);
  CHECK_EQ(term_tag(doubled), TAG_UOP);
  CHECK_EQ(term_ext(doubled), UOP_MUL);
  Term dbl_lhs = heap_read(term_val(doubled) + 0);
  Term dbl_rhs = heap_read(term_val(doubled) + 1);
  CHECK_EQ(dbl_lhs, a);
  CHECK_EQ(term_tag(dbl_rhs), TAG_UOP);
  CHECK_EQ(term_ext(dbl_rhs), UOP_CONST);
  CHECK_EQ(term_val(heap_read(term_val(dbl_rhs))), f32_bits(2.0f));

  // Negative case: a + b stays put (rule guards on a == b).
  TEST_BEGIN("upat/two-rule-bridge-guarded-no-fire");
  Term a_plus_b = uop_binary(UOP_ADD, a, b);
  Term unchanged = uop_pattern_rewrite(a_plus_b, rules2, 2, NULL);
  CHECK_EQ(term_tag(unchanged), TAG_UOP);
  CHECK_EQ(term_ext(unchanged), UOP_ADD);
  CHECK_EQ(heap_read(term_val(unchanged) + 0), a);
  CHECK_EQ(heap_read(term_val(unchanged) + 1), b);

  // --- (4) op_alt: one pattern matches a small opcode set --------
  TEST_BEGIN("upat/op-alt-matches-add-or-mul");
  static u8 const alu_alt[] = {UOP_ADD, UOP_MUL, 0};
  static UPat const pat_alu = {
    0, 2, 0, -1, pat_add_children, alu_alt
  };
  Term bindings_add[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_alu, mul_ab, bindings_add));
  CHECK_EQ(bindings_add[0], a);
  CHECK_EQ(bindings_add[1], b);
  Term bindings_mul[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_alu, a_plus_b, bindings_mul));
  CHECK_EQ(bindings_mul[0], a);
  CHECK_EQ(bindings_mul[1], b);

  TEST_BEGIN("upat/op-alt-rejects-out-of-set");
  // UOP_NEG is not in {ADD, MUL}; pat_alu must reject neg_a.
  Term neg_arity_check[UPAT_NUM_BINDINGS] = {0};
  CHECK(!upat_match(&pat_alu, neg_a, neg_arity_check));

  TEST_BEGIN("upat/op-alt-rejects-leaf");
  // op_alt requires a UOp tag; a leaf tensor has no opcode.
  Term leaf_check[UPAT_NUM_BINDINGS] = {0};
  CHECK(!upat_match(&pat_alu, a, leaf_check));

  // --- (5) nested 3-level pattern: PAD(RESHAPE(SHRINK(?inner))) --
  // Building block for the conv2d SUM-OF-SHIFTED-PADS detector
  // (Level 65/66): the conv UOp DAG produces a Fold of these
  // patches.  Movement ops have arity 1 (their dimension args live
  // in heap slots beyond the source); upat_match traverses only
  // arity children, so a 3-level nested UPat structurally walks
  // the chain without any dim inspection.
  TEST_BEGIN("upat/nested-pad-reshape-shrink");
  Shape s2d = {0}; s2d.ndim = 2; s2d.dims[0] = 4; s2d.dims[1] = 4;
  u32 ti = tensor_alloc(CURRENT_BACKEND, s2d, DT_FP32);
  Term inner = term_new(0, TAG_TEN, DT_FP32, ti);
  // SHRINK 4x4 -> 2x2 (begin/end pairs per axis).
  u32 shrink_be[4] = {1, 3, 1, 3};
  Term sh = uop_shrink(inner, 2, shrink_be);
  // RESHAPE 2x2 -> 1x4.
  u32 rshape[2] = {1, 4};
  Term rs = uop_reshape(sh, 2, rshape);
  // PAD 1x4 -> 3x4 (1 row before, 1 after).
  u32 pad_be[4] = {1, 1, 0, 0};
  Term pd = uop_pad(rs, 2, pad_be);

  // 3-level nested UPat: PAD(RESHAPE(SHRINK(?0))).
  static UPat const wild_in     = {0, 0xFF, 0, 0, NULL, NULL};
  static UPat const shrink_kids[1] = {wild_in};
  static UPat const pat_shrink = {UOP_SHRINK, 1, 0, -1, shrink_kids, NULL};
  static UPat const reshape_kids[1] = {pat_shrink};
  static UPat const pat_reshape = {UOP_RESHAPE, 1, 0, -1, reshape_kids, NULL};
  static UPat const pad_kids[1] = {pat_reshape};
  static UPat const pat_pad_chain = {UOP_PAD, 1, 0, -1, pad_kids, NULL};

  Term bindings_chain[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_pad_chain, pd, bindings_chain));
  CHECK_EQ(bindings_chain[0], inner);

  // Negative: a bare SHRINK should not match the full chain.
  Term neg_chain[UPAT_NUM_BINDINGS] = {0};
  CHECK(!upat_match(&pat_pad_chain, sh, neg_chain));

  thvm_free();
  TEST_REPORT();
}
