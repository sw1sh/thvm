// test_uop_opt.c - Phase D'4: UOP_OPT annotation round-trip.
//
// Validates the annotation opcode that attaches optimisation
// directives (UNROLL, UPCAST, TC, LOCAL, GROUP_REDUCE) to a target
// node.  Per the TileLang correspondence: T.unroll = OPT(range,
// UNROLL, factor); T.gemm composes matmul + OPT(_, TC); T.Parallel
// = OPT(range, LOCAL, 0).  The renderer (F0+) pattern-matches the
// (target shape, opt kind) pair to fire specialised templates.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  Term r = uop_range(0, KAX_REDUCE, 16);

  TEST_BEGIN("uop-opt/unroll-with-factor");
  Term unr = uop_opt(r, UOP_OPT_UNROLL, 4);
  CHECK(unr != 0);
  CHECK_EQ(term_tag(unr), TAG_UOP);
  CHECK_EQ(term_ext(unr), UOP_OPT);
  CHECK_EQ(uop_opt_target(unr), r);
  CHECK_EQ(uop_opt_kind  (unr), UOP_OPT_UNROLL);
  CHECK_EQ(uop_opt_factor(unr), 4u);

  TEST_BEGIN("uop-opt/hash-cons-shares");
  Term unr2 = uop_opt(r, UOP_OPT_UNROLL, 4);
  CHECK_EQ(unr, unr2);

  TEST_BEGIN("uop-opt/different-kind-distinct");
  Term up = uop_opt(r, UOP_OPT_UPCAST, 4);
  CHECK(up != unr);
  CHECK_EQ(uop_opt_kind(up), UOP_OPT_UPCAST);

  TEST_BEGIN("uop-opt/different-factor-distinct");
  Term unr8 = uop_opt(r, UOP_OPT_UNROLL, 8);
  CHECK(unr8 != unr);
  CHECK_EQ(uop_opt_factor(unr8), 8u);

  TEST_BEGIN("uop-opt/different-target-distinct");
  Term r2 = uop_range(1, 1, 16);
  Term unr_r2 = uop_opt(r2, UOP_OPT_UNROLL, 4);
  CHECK(unr_r2 != unr);
  CHECK_EQ(uop_opt_target(unr_r2), r2);

  TEST_BEGIN("uop-opt/tc-no-factor");
  // TC and LOCAL annotations carry no factor.
  u32 dims_a[2] = { 64, 32 };
  u32 dims_b[2] = { 32, 64 };
  Term a = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_a);
  Term b = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_b);
  Term tc = uop_opt(a, UOP_OPT_TC, 0);
  CHECK_EQ(uop_opt_kind  (tc), UOP_OPT_TC);
  CHECK_EQ(uop_opt_factor(tc), 0u);
  CHECK_EQ(uop_opt_target(tc), a);
  (void)b;

  TEST_BEGIN("uop-opt/local-bind-to-thread");
  Term lc = uop_opt(r, UOP_OPT_LOCAL, 0);
  CHECK_EQ(uop_opt_kind(lc), UOP_OPT_LOCAL);

  TEST_BEGIN("uop-opt/group-reduce-with-factor");
  Term gr = uop_opt(r, UOP_OPT_GROUP_REDUCE, 32);
  CHECK_EQ(uop_opt_kind  (gr), UOP_OPT_GROUP_REDUCE);
  CHECK_EQ(uop_opt_factor(gr), 32u);

  TEST_BEGIN("uop-opt/non-opt-term-rejects");
  Term c = uop_const(DT_FP32, 0x3F800000u);
  CHECK_EQ(uop_opt_target(c), 0u);
  CHECK_EQ(uop_opt_kind  (c), 0u);
  CHECK_EQ(uop_opt_factor(c), 0u);

  TEST_BEGIN("uop-opt/stacks-on-target");
  // OPT(OPT(r, UNROLL, 4), TC, 0) -- stacked annotations are valid;
  // construction wraps target as-is, hash-cons distinguishes.
  Term stacked = uop_opt(unr, UOP_OPT_TC, 0);
  CHECK(stacked != unr);
  CHECK_EQ(uop_opt_target(stacked), unr);
  CHECK_EQ(uop_opt_kind  (stacked), UOP_OPT_TC);
  CHECK_EQ(uop_opt_target(uop_opt_target(stacked)), r);

  thvm_free();
  TEST_REPORT();
}
