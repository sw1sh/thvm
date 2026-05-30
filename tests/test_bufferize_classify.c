// test_bufferize_classify.c - exercise the UOP realization
// classifier.  Builds small UOp graphs by hand (without going
// through materialize so the heap UOps are still raw) and asserts
// which nodes the classifier flags as realized.

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

int main(void) {
  thvm_init();

  TEST_BEGIN("realize-classify/single-uop-root-realized");
  // Just one ADD over two TENs: only the root realizes.
  u32 ta = alloc_f32_tensor(3);
  u32 tb = alloc_f32_tensor(3);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term add = uop_binary(UOP_ADD, a, b);
  bufferize_classify(add);
  CHECK_EQ(bufferize_is_realized(add), 1);
  CHECK_EQ(bufferize_consumer_count(add), 0);
  CHECK(bufferize_reasons(add) & BUFFERIZE_REASON_ROOT);

  TEST_BEGIN("realize-classify/chain-only-root-realized");
  // (a + b) * c -- linear chain, single consumer at each step.
  // The ADD has 1 consumer (the MUL); the MUL is the root.
  // Expect: ADD not realized, MUL realized.
  u32 tc = alloc_f32_tensor(3);
  Term c = term_new(0, TAG_TEN, DT_FP32, tc);
  Term add2 = uop_binary(UOP_ADD, a, b);
  Term mul2 = uop_binary(UOP_MUL, add2, c);
  bufferize_classify(mul2);
  CHECK_EQ(bufferize_consumer_count(add2), 1);
  CHECK_EQ(bufferize_is_realized(add2), 0);
  CHECK_EQ(bufferize_is_realized(mul2), 1);

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
  bufferize_classify(root);
  CHECK_EQ(bufferize_consumer_count(shared), 2);
  CHECK_EQ(bufferize_is_realized(shared), 1);
  CHECK(bufferize_reasons(shared) & BUFFERIZE_REASON_MULTI);
  // The single-consumer intermediates (left, right) are NOT
  // realized.
  CHECK_EQ(bufferize_is_realized(left), 0);
  CHECK_EQ(bufferize_is_realized(right), 0);
  // The root is always realized.
  CHECK_EQ(bufferize_is_realized(root), 1);

  TEST_BEGIN("realize-classify/dup-child-counts-as-one-consumer");
  // MUL[x, x] where x is a UOp.  x is referenced by MUL's two
  // src slots but only counts as ONE consumer (the materializer
  // dedups identical inputs into one slot).
  Term x   = uop_binary(UOP_ADD, a, b);
  Term sq  = uop_binary(UOP_MUL, x, x);
  bufferize_classify(sq);
  CHECK_EQ(bufferize_consumer_count(x), 1);
  CHECK_EQ(bufferize_is_realized(x), 0);   // single consumer, not realized
  CHECK_EQ(bufferize_is_realized(sq), 1);

  TEST_BEGIN("realize-classify/reduce-always-realizes");
  // (a + b) reduced -- ADD is a single-consumer intermediate
  // BUT REDUCE outputs always realize regardless of consumer
  // count.
  Term tmp     = uop_binary(UOP_ADD, a, b);
  Term reduced = uop_reduce(REDUCE_SUM, 0, tmp);
  bufferize_classify(reduced);
  CHECK_EQ(bufferize_is_realized(tmp), 0);     // ADD not REDUCE, single consumer
  CHECK_EQ(bufferize_is_realized(reduced), 1); // REDUCE always realizes
  CHECK(bufferize_reasons(reduced) & BUFFERIZE_REASON_ROOT);
  CHECK(bufferize_reasons(reduced) & BUFFERIZE_REASON_REDUCE);

  TEST_BEGIN("realize-classify/reduce-mid-graph-realizes");
  // ((a + b) reduced) * c -- the REDUCE is in the middle,
  // not the root.  REDUCE rule should still fire.
  Term tmp2     = uop_binary(UOP_ADD, a, b);
  Term reduced2 = uop_reduce(REDUCE_SUM, 0, tmp2);
  Term out      = uop_binary(UOP_MUL, reduced2, c);
  bufferize_classify(out);
  CHECK_EQ(bufferize_is_realized(tmp2), 0);
  CHECK_EQ(bufferize_is_realized(reduced2), 1);   // mid-graph REDUCE
  CHECK_EQ(bufferize_is_realized(out), 1);

  TEST_BEGIN("realize-classify/non-uop-leaves-are-not-realized");
  // TAG_TEN leaves should never appear in the table.
  CHECK_EQ(bufferize_is_realized(a), 0);
  CHECK_EQ(bufferize_is_realized(b), 0);

  TEST_BEGIN("realize-classify/thvm-materialize-populates-table");
  // thvm_materialize must call realize_classify before its walk so
  // downstream passes can read the table.  Verify by clearing the
  // table (via a fresh thvm_init), running thvm_materialize on a
  // chain, and asserting the root is flagged realized.
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
  CHECK_EQ(bufferize_is_realized(times_cc), 1);
  CHECK_EQ(bufferize_consumer_count(aa_plus_bb), 1);
  unsetenv("THVM_UOP_GRAPH_SIMPLIFY");

  TEST_BEGIN("realize-classify/broadcast-reduce-bn-mean-chain-inlines");
  // The broadcast-reduce predicate must accept the BatchNorm-mean
  // chain shape produced by WL's `reduce / N` lowering:
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
  bufferize_classify(bn_root);
  // The reduce should be inlined by the broadcast-reduce prune that
  // bufferize_classify pre-seeds (formerly the named
  // inline-softmax-broadcast-reduce rule, now folded inline).
  CHECK_EQ(bufferize_is_realized(bn_r), 0);
  CHECK(bufferize_reasons(bn_r) & BUFFERIZE_REASON_INLINE);

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
  bufferize_classify(nb_root);
  CHECK_EQ(bufferize_is_realized(nb_r), 1);    // no broadcast-of-CONST sibling

  TEST_BEGIN("realize-classify/reduce-epilogue-fuse-off-default");
  // reduce(x) * scalar_const, with the scalar-MUL as the realize root.
  // DEFAULT (flag unset): the reduce stays a realized boundary -- its
  // [8] output is stored, then the epilogue reads it.  The fuse rule is
  // opt-in; bit-exact behaviour must not change without the flag.
  thvm_free();
  thvm_init();
  unsetenv("THVM_FUSE_REDUCE_EPILOGUE");
  u32 t_ep = alloc_f32_tensor2(4, 8);
  Term ep_x = term_new(0, TAG_TEN, DT_FP32, t_ep);
  Term ep_r = uop_reduce(REDUCE_SUM, 0, ep_x);          // shape {8}
  Term ep_half = uop_const(DT_FP32, 0x3F000000u);       // 0.5f
  Term ep_root = uop_binary(UOP_MUL, ep_r,
                            uop_expand(ep_half, 1, (u32[]){8}));
  bufferize_classify(ep_root);
  CHECK_EQ(bufferize_is_realized(ep_root), 1);           // the root
  CHECK_EQ(bufferize_is_realized(ep_r), 1);              // reduce realized (default)

  TEST_BEGIN("realize-classify/reduce-epilogue-fuse-on");
  // THVM_FUSE_REDUCE_EPILOGUE=1: the single-consumer reduce whose sole
  // consumer is the shape-preserving scalar-broadcast MUL fuses into the
  // epilogue's kernel -- the reduce is no longer a realized boundary.
  thvm_free();
  thvm_init();
  setenv("THVM_FUSE_REDUCE_EPILOGUE", "1", 1);
  u32 t_ep2 = alloc_f32_tensor2(4, 8);
  Term ep2_x = term_new(0, TAG_TEN, DT_FP32, t_ep2);
  Term ep2_r = uop_reduce(REDUCE_SUM, 0, ep2_x);         // shape {8}
  Term ep2_half = uop_const(DT_FP32, 0x3F000000u);       // 0.5f
  Term ep2_root = uop_binary(UOP_MUL, ep2_r,
                             uop_expand(ep2_half, 1, (u32[]){8}));
  bufferize_classify(ep2_root);
  CHECK_EQ(bufferize_is_realized(ep2_root), 1);          // the root stays
  CHECK_EQ(bufferize_is_realized(ep2_r), 0);             // reduce fused away
  CHECK(bufferize_reasons(ep2_r) & BUFFERIZE_REASON_INLINE);
  unsetenv("THVM_FUSE_REDUCE_EPILOGUE");

  thvm_free();
  TEST_REPORT();
}
