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

  TEST_BEGIN("realize-classify/reduce-always-realizes");
  // (a + b) reduced -- ADD is a single-consumer intermediate
  // BUT REDUCE outputs always realize regardless of consumer
  // count.
  Term tmp     = uop_binary(UOP_ADD, a, b);
  Term reduced = uop_reduce(REDUCE_SUM, 0, tmp);
  realize_classify(reduced);
  CHECK_EQ(realize_is_realized(tmp), 0);     // ADD not REDUCE, single consumer
  CHECK_EQ(realize_is_realized(reduced), 1); // REDUCE always realizes

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
  thvm_materialize(times_cc);
  // Root is realized; the chain intermediate is single-consumer.
  CHECK_EQ(realize_is_realized(times_cc), 1);
  CHECK_EQ(realize_consumer_count(aa_plus_bb), 1);

  thvm_free();
  TEST_REPORT();
}
