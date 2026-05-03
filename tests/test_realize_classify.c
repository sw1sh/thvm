// test_realize_classify.c - exercise the UOP realization
// classifier (f1c).  Builds small UOp graphs by hand (without
// going through materialize so the heap UOps are still raw)
// and asserts which nodes the classifier flags as realized.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static u32 alloc_f32_tensor2(u32 d0, u32 d1) {
  Shape s = {0};
  s.ndim    = 2;
  s.dims[0] = d0;
  s.dims[1] = d1;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static Term reshape1(Term x, u32 n) {
  u32 dims[1] = {n};
  return uop_reshape(x, 1, dims);
}

static Term wide_reshape_add_tree(Term *xs, u32 n, u32 numel) {
  Term acc = xs[0];
  for (u32 i = 1; i < n; i++) {
    Term item = (i & 1u) ? reshape1(xs[i], numel) : xs[i];
    acc = uop_binary(UOP_ADD, acc, item);
  }
  return acc;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("realize-classify/single-uop-root-realized");
  // Just one ADD over two TENs: only the root realizes.
  u32 ta = alloc_f32_tensor(3);
  u32 tb = alloc_f32_tensor(3);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term add = uop_binary(UOP_ADD, a, b);
  realize_classify(add);
  CHECK_EQ(realize_is_realized(add), 1);
  CHECK_EQ(realize_consumer_count(add), 0);
  CHECK(realize_reasons(add) & REALIZE_REASON_ROOT);

  TEST_BEGIN("realize-classify/chain-only-root-realized");
  // (a + b) * c -- linear chain, single consumer at each step.
  // The ADD has 1 consumer (the MUL); the MUL is the root.
  // Expect: ADD not realized, MUL realized.
  u32 tc = alloc_f32_tensor(3);
  Term c = term_new(0, TAG_TEN, DT_FP32, tc);
  Term add2 = uop_binary(UOP_ADD, a, b);
  Term mul2 = uop_binary(UOP_MUL, add2, c);
  realize_classify(mul2);
  CHECK_EQ(realize_consumer_count(add2), 1);
  CHECK_EQ(realize_is_realized(add2), 0);
  CHECK_EQ(realize_is_realized(mul2), 1);

  TEST_BEGIN("realize-classify/shared-subexpr-multi-consumer-realized");
  // shared = a + b; out = shared * shared (two distinct parents
  // would be needed to trigger multi-consumer; MUL[shared, shared]
  // dedups to one).  Build a real fan-out:
  //   shared = a + b
  //   left   = shared * c
  //   right  = shared * a
  //   root   = left + right
  // shared has 2 distinct UOp parents (left, right) -> realized.
  Term shared = uop_binary(UOP_ADD, a, b);
  Term left   = uop_binary(UOP_MUL, shared, c);
  Term right  = uop_binary(UOP_MUL, shared, a);
  Term root   = uop_binary(UOP_ADD, left, right);
  realize_classify(root);
  CHECK_EQ(realize_consumer_count(shared), 2);
  CHECK_EQ(realize_is_realized(shared), 1);
  CHECK(realize_reasons(shared) & REALIZE_REASON_MULTI);
  // The single-consumer intermediates (left, right) are NOT
  // realized.
  CHECK_EQ(realize_is_realized(left), 0);
  CHECK_EQ(realize_is_realized(right), 0);
  // The root is always realized.
  CHECK_EQ(realize_is_realized(root), 1);

  TEST_BEGIN("realize-classify/dup-child-counts-as-one-consumer");
  // MUL[x, x] where x is a UOp.  x is referenced by MUL's two
  // src slots but only counts as ONE consumer (the materializer
  // dedups identical inputs into one slot).
  Term x   = uop_binary(UOP_ADD, a, b);
  Term sq  = uop_binary(UOP_MUL, x, x);
  realize_classify(sq);
  CHECK_EQ(realize_consumer_count(x), 1);
  CHECK_EQ(realize_is_realized(x), 0);   // single consumer, not realized
  CHECK_EQ(realize_is_realized(sq), 1);

  TEST_BEGIN("realize-classify/shared-const-stays-inline");
  Term two = uop_const(DT_FP32, 0x40000000u);
  Term c_left = uop_binary(UOP_MUL, a, two);
  Term c_right = uop_binary(UOP_MUL, b, two);
  Term c_root = uop_binary(UOP_ADD, c_left, c_right);
  realize_classify(c_root);
  CHECK_EQ(realize_consumer_count(two), 2);
  CHECK_EQ(realize_is_realized(two), 0);
  CHECK(realize_reasons(two) & REALIZE_REASON_MULTI);
  CHECK(realize_reasons(two) & REALIZE_REASON_INLINE);
  CHECK_EQ(realize_rewrite_stat_hits("inline-constants"), 1);
  CHECK(realize_rewrite_stats_len() >= 1);

  TEST_BEGIN("realize-classify/reduce-always-realizes");
  // (a + b) reduced -- ADD is a single-consumer intermediate
  // BUT REDUCE outputs always realize regardless of consumer
  // count.
  Term tmp     = uop_binary(UOP_ADD, a, b);
  Term reduced = uop_reduce(REDUCE_SUM, 0, tmp);
  realize_classify(reduced);
  CHECK_EQ(realize_is_realized(tmp), 0);     // ADD not REDUCE, single consumer
  CHECK_EQ(realize_is_realized(reduced), 1); // REDUCE always realizes
  CHECK(realize_reasons(reduced) & REALIZE_REASON_ROOT);
  CHECK(realize_reasons(reduced) & REALIZE_REASON_REDUCE);

  TEST_BEGIN("realize-classify/reduce-mid-graph-realizes");
  // ((a + b) reduced) * c -- the REDUCE is in the middle,
  // not the root.  REDUCE rule should still fire.
  Term tmp2     = uop_binary(UOP_ADD, a, b);
  Term reduced2 = uop_reduce(REDUCE_SUM, 0, tmp2);
  Term out      = uop_binary(UOP_MUL, reduced2, c);
  realize_classify(out);
  CHECK_EQ(realize_is_realized(tmp2), 0);
  CHECK_EQ(realize_is_realized(reduced2), 1);   // mid-graph REDUCE
  CHECK_EQ(realize_is_realized(out), 1);

  TEST_BEGIN("realize-classify/metal-reduce-fanout-inline-opt-in");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  setenv("THVM_INLINE_REDUCE_FANOUT", "1", 1);
  setenv("THVM_INLINE_REDUCE_FANOUT_MIN_NUMEL", "1", 1);
  thvm_free();
  thvm_init();
  u32 trf = alloc_f32_tensor2(2, 2);
  Term rf = term_new(0, TAG_TEN, DT_FP32, trf);
  Term rf_reduce = uop_reduce(REDUCE_SUM, 1, rf);
  Term rf_left = uop_unary(UOP_NEG, rf_reduce);
  Term rf_right = uop_binary(UOP_MUL, rf_reduce, rf_reduce);
  Term rf_root = uop_binary(UOP_ADD, rf_left, rf_right);
  realize_classify(rf_root);
  CHECK_EQ(realize_consumer_count(rf_reduce), 2);
  CHECK_EQ(realize_is_realized(rf_reduce), 0);
  CHECK_EQ(realize_rewrite_stat_hits("inline-reduce-fanout"), 1);

  TEST_BEGIN("realize-classify/metal-reduce-fanout-keeps-reduce-parent");
  thvm_free();
  thvm_init();
  u32 trp = alloc_f32_tensor2(2, 2);
  Term rp = term_new(0, TAG_TEN, DT_FP32, trp);
  Term rp_reduce = uop_reduce(REDUCE_SUM, 1, rp);
  Term rp_left = uop_unary(UOP_NEG, rp_reduce);
  Term rp_right = uop_binary(UOP_MUL, rp_reduce, rp_reduce);
  Term rp_sum = uop_binary(UOP_ADD, rp_left, rp_right);
  Term rp_root = uop_reduce(REDUCE_SUM, 0, rp_sum);
  realize_classify(rp_root);
  CHECK_EQ(realize_consumer_count(rp_reduce), 2);
  CHECK_EQ(realize_is_realized(rp_reduce), 1);
  CHECK_EQ(realize_rewrite_stat_hits("inline-reduce-fanout"), 0);
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");
  unsetenv("THVM_INLINE_REDUCE_FANOUT");
  unsetenv("THVM_INLINE_REDUCE_FANOUT_MIN_NUMEL");

  TEST_BEGIN("realize-classify/non-uop-leaves-are-not-realized");
  // TAG_TEN leaves should never appear in the table.
  CHECK_EQ(realize_is_realized(a), 0);
  CHECK_EQ(realize_is_realized(b), 0);

  TEST_BEGIN("realize-classify/thvm-materialize-populates-table");
  // f1d-a: thvm_materialize should call realize_classify before
  // its walk so f1d-b/c can read the table.  Verify by clearing
  // the table (via a fresh thvm_init), running thvm_materialize
  // on a chain, and asserting the root is flagged realized.
  thvm_free();
  thvm_init();
  u32 ta2 = alloc_f32_tensor(3);
  u32 tb2 = alloc_f32_tensor(3);
  u32 tc2 = alloc_f32_tensor(3);
  Term aa = term_new(0, TAG_TEN, DT_FP32, ta2);
  Term bb = term_new(0, TAG_TEN, DT_FP32, tb2);
  Term cc = term_new(0, TAG_TEN, DT_FP32, tc2);
  Term aa_plus_bb = uop_binary(UOP_ADD, aa, bb);
  Term times_cc   = uop_binary(UOP_MUL, aa_plus_bb, cc);
  setenv("THVM_UOP_GRAPH_SIMPLIFY", "0", 1);
  thvm_materialize(times_cc);
  // Root is realized; the chain intermediate is single-consumer.
  CHECK_EQ(realize_is_realized(times_cc), 1);
  CHECK_EQ(realize_consumer_count(aa_plus_bb), 1);
  unsetenv("THVM_UOP_GRAPH_SIMPLIFY");

  TEST_BEGIN("realize-classify/metal-pure-movement-fanout-to-reduce-now-recomputes");
  // Phase 5/3 follow-up: the historical conservative gate that
  // kept movement-bearing buffers feeding reduce-chain consumers
  // realized has been lifted now that Phase 2's per-USE BIndex
  // chain machinery + rangeify rerouting compose movement chains
  // correctly across recompute boundaries.  remove-removable-bufferize
  // unmarks `pad` (it has 2 reduce-chain consumers but the chain
  // composes through the new rangeify path) so it does NOT stay
  // realized; the test name reflects the new behavior.  Set
  // THVM_BUFFERIZE_LIFT_MOVEMENT_REDUCE_GATE=0 to restore
  // pre-Phase-2 conservative behavior for bisecting.
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  setenv("THVM_INLINE_MULTI_CONSUMER_PURE", "1", 1);
  setenv("THVM_INLINE_MULTI_CONSUMER_PURE_MIN_NUMEL", "1", 1);
  thvm_free();
  thvm_init();
  u32 tm = alloc_f32_tensor2(2, 2);
  Term m = term_new(0, TAG_TEN, DT_FP32, tm);
  u32 widths[4] = {0, 0, 0, 2};
  Term pad = uop_pad(m, 2, widths);
  Term r0 = uop_reduce(REDUCE_SUM, 1, pad);
  Term r1 = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, pad, pad));
  Term combined = uop_binary(UOP_ADD, r0, r1);
  realize_classify(combined);
  CHECK_EQ(realize_consumer_count(pad), 2);
  CHECK_EQ(realize_is_realized(pad), 0);
  CHECK(realize_reasons(pad) & REALIZE_REASON_MULTI);
  CHECK(realize_reasons(pad) & REALIZE_REASON_INLINE);
  // remove-removable-bufferize fires on this case now that the
  // movement-reduce gate is lifted.
  CHECK(realize_rewrite_stat_hits("remove-removable-bufferize") >= 1);

  TEST_BEGIN("realize-classify/metal-large-pure-movement-fanout-recomputes");
  thvm_free();
  thvm_init();
  u32 tq = alloc_f32_tensor2(2, 2);
  Term q = term_new(0, TAG_TEN, DT_FP32, tq);
  u32 qwidths[4] = {0, 0, 0, 2};
  Term qpad = uop_pad(q, 2, qwidths);
  Term qleft = uop_unary(UOP_NEG, qpad);
  Term qright = uop_binary(UOP_MUL, qpad, qpad);
  Term qroot = uop_binary(UOP_ADD, qleft, qright);
  realize_classify(qroot);
  CHECK_EQ(realize_consumer_count(qpad), 2);
  CHECK_EQ(realize_is_realized(qpad), 0);
  CHECK_EQ(realize_rewrite_stat_hits("remove-removable-bufferize"), 1);

  TEST_BEGIN("realize-classify/metal-removable-bufferize-inlines-pure-alu-fanout");
  u32 tn = alloc_f32_tensor(4);
  Term n = term_new(0, TAG_TEN, DT_FP32, tn);
  Term pure = uop_binary(UOP_ADD, n, n);
  Term pure_left = uop_binary(UOP_MUL, pure, n);
  Term pure_right = uop_binary(UOP_MUL, pure, pure);
  Term pure_root = uop_binary(UOP_ADD, pure_left, pure_right);
  realize_classify(pure_root);
  CHECK_EQ(realize_consumer_count(pure), 2);
  CHECK_EQ(realize_is_realized(pure), 0);
  CHECK(realize_reasons(pure) & REALIZE_REASON_MULTI);
  CHECK(realize_reasons(pure) & REALIZE_REASON_INLINE);
  CHECK_EQ(realize_rewrite_stat_hits("remove-removable-bufferize"), 1);

  TEST_BEGIN("realize-classify/metal-removable-bufferize-disable-keeps-fanout");
  setenv("THVM_REMOVE_REMOVABLE_BUFFERIZE", "0", 1);
  Term keep = uop_binary(UOP_ADD, n, n);
  Term keep_left = uop_binary(UOP_MUL, keep, n);
  Term keep_right = uop_binary(UOP_ADD, keep, keep);
  Term keep_root = uop_binary(UOP_ADD, keep_left, keep_right);
  realize_classify(keep_root);
  CHECK_EQ(realize_consumer_count(keep), 2);
  CHECK_EQ(realize_is_realized(keep), 1);
  CHECK_EQ(realize_rewrite_stat_hits("remove-removable-bufferize"), 0);
  unsetenv("THVM_REMOVE_REMOVABLE_BUFFERIZE");
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");
  unsetenv("THVM_INLINE_MULTI_CONSUMER_PURE");
  unsetenv("THVM_INLINE_MULTI_CONSUMER_PURE_MIN_NUMEL");

  TEST_BEGIN("realize-classify/metal-fanin-cap-splits-wide-mul-parent");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  setenv("THVM_METAL_FUSION_MAX_INPUTS", "24", 1);
  thvm_free();
  thvm_init();
  Term xs[34];
  for (u32 i = 0; i < 34; i++) {
    xs[i] = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(32));
  }
  Term tree_a = wide_reshape_add_tree(&xs[0], 16, 32);
  Term prod   = uop_binary(UOP_MUL, tree_a, tree_a);
  Term scale  = uop_const(DT_FP32, 0x3DCCCCCDu);
  Term left_m = uop_binary(UOP_MUL, prod, scale);
  Term right_m = uop_binary(UOP_MUL, xs[32], xs[33]);
  Term wide_root = uop_binary(UOP_ADD, left_m, right_m);
  realize_classify(wide_root);
  CHECK(realize_rewrite_stat_hits("metal-tile-fanin-cap") >= 1);
  CHECK(realize_is_realized(tree_a) || realize_is_realized(left_m)
      || realize_is_realized(prod));

  TEST_BEGIN("realize-classify/metal-fanin-cap-splits-unary-wrapper");
  Term tree_c = wide_reshape_add_tree(&xs[0], 16, 32);
  Term wide_neg = uop_unary(UOP_NEG, uop_binary(UOP_MUL, tree_c, tree_c));
  Term wide_unary_root = uop_binary(UOP_ADD, wide_neg, right_m);
  realize_classify(wide_unary_root);
  CHECK(realize_rewrite_stat_hits("metal-tile-fanin-cap") >= 1);
  CHECK(realize_is_realized(wide_neg) || realize_is_realized(tree_c));
  unsetenv("THVM_BACKEND");
  unsetenv("THVM_TILE");
  unsetenv("THVM_METAL_FUSION_MAX_INPUTS");

  TEST_BEGIN("realize-classify/broadcast-reduce-bn-mean-chain-inlines");
  // Phase-1 of tinygrad rule port: the broadcast-reduce predicate
  // must accept the BatchNorm-mean chain shape produced by WL's
  // `reduce / N` lowering:
  //
  //   REDUCE_SUM -> MUL(reduce, EXPAND(CONST 1/N))
  //              -> RESHAPE({1, C, 1, 1})
  //              -> EXPAND({B, C, H, W})
  //
  // The MUL's other operand is `EXPAND(CONST)` (not a bare CONST)
  // because liftNumeric/broadcastScalar wraps the literal scalar.
  // RESHAPE+PERMUTE must be valid chain hops.  Without these
  // relaxations every BN reduce stays realized and the canary kernel
  // count stays at the unfused 1070+.
  thvm_free();
  thvm_init();
  u32 t_bn = alloc_f32_tensor2(2, 4);     // {B*H*W, C} flattened source
  Term bn_x  = term_new(0, TAG_TEN, DT_FP32, t_bn);
  Term bn_r  = uop_reduce(REDUCE_SUM, 0, bn_x);    // shape {4}
  Term bn_inv_n = uop_const(DT_FP32, 0x3F000000u); // 0.5 (just any scalar)
  u32 bn_inv_dims[1] = {4};
  Term bn_inv_b = uop_expand(bn_inv_n, 1, bn_inv_dims);
  Term bn_mean  = uop_binary(UOP_MUL, bn_r, bn_inv_b);   // {4}
  u32 bn_rs_dims[2] = {1, 4};
  Term bn_rs    = uop_reshape(bn_mean, 2, bn_rs_dims);   // {1, 4}
  u32 bn_ex_dims[2] = {2, 4};
  Term bn_ex    = uop_expand(bn_rs, 2, bn_ex_dims);      // {2, 4}
  Term bn_root  = uop_binary(UOP_ADD, bn_x, bn_ex);
  realize_classify(bn_root);
  // The reduce should be inlined by inline-softmax-broadcast-reduce.
  CHECK_EQ(realize_is_realized(bn_r), 0);
  CHECK(realize_reasons(bn_r) & REALIZE_REASON_INLINE);
  CHECK(realize_rewrite_stat_hits("inline-softmax-broadcast-reduce") >= 1);

  TEST_BEGIN("realize-classify/broadcast-reduce-rejects-non-broadcast-tail");
  // Negative test: REDUCE whose parent ALU has a NON-CONST sibling
  // (here `nb_x` itself, broadcast back to the reduce shape via a
  // separate REDUCE chain) breaks the predicate.  Without a CONST
  // sibling the post-reduce-shape invariant doesn't hold and the
  // chain must NOT inline.
  thvm_free();
  thvm_init();
  u32 t_nb = alloc_f32_tensor2(4, 8);
  Term nb_x = term_new(0, TAG_TEN, DT_FP32, t_nb);
  Term nb_r = uop_reduce(REDUCE_SUM, 0, nb_x);  // shape {8}
  // Sibling is a scalar tensor of unrelated shape, NOT a const wrapper.
  u32 t_sib = alloc_f32_tensor(8);
  Term nb_sib = term_new(0, TAG_TEN, DT_FP32, t_sib);
  Term nb_alu = uop_binary(UOP_MUL, nb_r, nb_sib);  // sibling is TEN, not CONST
  u32 nb_ex_dims[2] = {4, 8};
  Term nb_root = uop_expand(uop_reshape(nb_alu, 2,
                                        (u32[]){1, 8}),
                            2, nb_ex_dims);
  realize_classify(nb_root);
  CHECK_EQ(realize_is_realized(nb_r), 1);    // no broadcast-of-CONST sibling

  thvm_free();
  TEST_REPORT();
}
