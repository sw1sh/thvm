// test_uop_upat.c - upat_match, the declarative UPat layer: op and
// arity gates, bindings, the any-op wildcard, op_alt opcode sets, a
// nested pattern, and an explicit nsrc through opcodes uop_arity()
// does not enumerate.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor_1d(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
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

  // --- (2) op_alt: one pattern matches a small opcode set --------
  static UPat const pat_add_children[2] = {
    {0, 0xFF, 0, 0, NULL},   // ?a (any UOp, captured as bindings[0])
    {0, 0xFF, 0, 1, NULL},   // ?b (any UOp, captured as bindings[1])
  };
  Term a_plus_b = uop_binary(UOP_ADD, a, b);
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

  // --- (3) nested 3-level pattern: PAD(RESHAPE(SHRINK(?inner))) --
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
  static UPat const shrink_kids[1]  = {{0, 0xFF, 0, 0, NULL, NULL}};
  static UPat const reshape_kids[1] = {{UOP_SHRINK, 1, 0, -1, shrink_kids, NULL}};
  static UPat const pad_kids[1]     = {{UOP_RESHAPE, 1, 0, -1, reshape_kids, NULL}};
  static UPat const pat_pad_chain   = {UOP_PAD, 1, 0, -1, pad_kids, NULL};

  Term bindings_chain[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_pad_chain, pd, bindings_chain));
  CHECK_EQ(bindings_chain[0], inner);

  // Negative: a bare SHRINK should not match the full chain.
  Term neg_chain[UPAT_NUM_BINDINGS] = {0};
  CHECK(!upat_match(&pat_pad_chain, sh, neg_chain));

  // --- (4) explicit-nsrc through opcodes uop_arity() doesn't enumerate --
  // UOP_INDEX_E (heap = [buffer, addr]) has arity 0 in the current
  // uop_arity() table because it isn't in the switch.  Before the
  // op_pinned relaxation, a UPat asking for nsrc=2 on UOP_INDEX_E
  // got rejected by the arity gate even though the heap layout has
  // exactly 2 Term children.  After the relaxation, when the user
  // pinned the op (here UOP_INDEX_E), nsrc is trusted -- enabling
  // depth-walks through the symbolic-INDEX layer needed by the F3
  // matmul recognizer (REDUCE -> MUL -> INDEX_E -> BUFFER).
  TEST_BEGIN("upat/index_e-explicit-nsrc-walks-children");
  u32 buf_dims[2] = {4, 8};
  Term buf_a = uop_buffer(/*scope=*/0, DT_FP32, 2, buf_dims);
  // INDEX_E(buffer, addr); addr is just any Term -- a NUM stand-in.
  Term zero  = term_new(0, TAG_NUM, DT_INT32, 0);
  Term ie    = uop_index_e(buf_a, zero);

  static UPat const ie_kids[2]  = {{UOP_BUFFER, 0, 0, 0, NULL, NULL}, {0, 0xFF, 0, 1, NULL, NULL}};
  static UPat const pat_index_e = {UOP_INDEX_E, 2, 0, -1, ie_kids, NULL};

  Term bindings_ie[UPAT_NUM_BINDINGS] = {0};
  CHECK(upat_match(&pat_index_e, ie, bindings_ie));
  CHECK_EQ(bindings_ie[0], buf_a);
  CHECK_EQ(bindings_ie[1], zero);

  // Negative: same pattern shouldn't match a non-INDEX_E node.
  Term ie_neg[UPAT_NUM_BINDINGS] = {0};
  CHECK(!upat_match(&pat_index_e, buf_a, ie_neg));

  thvm_free();
  TEST_REPORT();
}
