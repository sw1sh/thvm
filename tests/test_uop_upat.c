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

  thvm_free();
  TEST_REPORT();
}
