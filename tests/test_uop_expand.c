// test_uop_expand.c - exercises src/uop/expander.c, the port of
// tinygrad codegen/late/expander.py to thvm's UOp graph rewrite layer.
//
// Each test builds a small UOp DAG, calls uop_expand_graph(root), then
// asserts the post-expansion shape matches what tinygrad would produce
// (no UPCAST/UNROLL RANGE leaves, UOP_UNROLL wrappers carrying
// (axis, F) tuples, vectorized ALU through GEP swizzles, CONTRACT around
// REDUCE/STORE that consumed the unrolled axes).

#include "../src/thvm.c"
#include "test.h"

static int subtree_has_range_with_axis_type(Term t, u32 want_axis_type, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_RANGE) {
    return uop_range_axis_type(t) == want_axis_type;
  }
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    if (subtree_has_range_with_axis_type(heap_read(loc + i), want_axis_type, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int subtree_count_op(Term t, u32 want_op, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  int total = (op == want_op) ? 1 : 0;
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    total += subtree_count_op(heap_read(loc + i), want_op, depth + 1);
  }
  return total;
}

// Case 1: a bare UPCAST'd RANGE.  pm_pre_expander should rewrite to
// UNROLL(VCONST(0..F-1)) carrying (axis_id, F).
static int test_case1_bare_upcast_range(void) {
  thvm_init();
  TEST_BEGIN("case1 bare UPCAST RANGE -> UNROLL(VCONST)");

  Term r = uop_range(/*axis_id=*/3, /*axis_type=*/KAX_UPCAST, /*extent=*/4);
  CHECK_EQ(term_tag(r), TAG_UOP);
  CHECK_EQ(term_ext(r), UOP_RANGE);

  Term out = uop_expand_graph(r);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_UNROLL);
  CHECK_EQ(uop_unroll_n_args(out), 1u);
  CHECK_EQ(uop_unroll_axis_id(out, 0), 3u);
  CHECK_EQ(uop_unroll_factor(out, 0), 4u);

  Term inner = heap_read(term_val(out) + 0);
  CHECK_EQ(term_tag(inner), TAG_UOP);
  CHECK_EQ(term_ext(inner), UOP_VCONST);
  CHECK_EQ(uop_vconst_n(inner), 4u);
  CHECK_EQ(uop_vconst_bits(inner, 0), 0u);
  CHECK_EQ(uop_vconst_bits(inner, 1), 1u);
  CHECK_EQ(uop_vconst_bits(inner, 2), 2u);
  CHECK_EQ(uop_vconst_bits(inner, 3), 3u);

  // No KAX_UPCAST RANGEs remain.
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UPCAST, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 2: LOOP-typed RANGE (pm_pre_expander leaves it alone).
static int test_case2_loop_range_untouched(void) {
  thvm_init();
  TEST_BEGIN("case2 LOOP RANGE untouched by expander");

  Term r = uop_range(/*axis_id=*/0, /*axis_type=*/KAX_LOOP, /*extent=*/8);
  Term out = uop_expand_graph(r);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_RANGE);
  CHECK_EQ(uop_range_axis_type(out), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_extent(out), 8u);

  thvm_free();
  TEST_REPORT();
}

// Case 3: ALU(UPCAST, scalar) -> UNROLL(ALU(VCONST, CONTRACT(scalar)))
// do_expand fires: broadcast the scalar via CONTRACT semantics, lift
// the ALU through UNROLL.
static int test_case3_alu_through_upcast(void) {
  thvm_init();
  TEST_BEGIN("case3 IADD(UPCAST_RANGE, const) -> UNROLL(IADD(VCONST, CONTRACT))");

  Term r = uop_range(/*axis_id=*/2, /*axis_type=*/KAX_UPCAST, /*extent=*/4);
  Term k = uop_const(DT_INT32, /*bits=*/10);
  Term add = uop_int_binary(UOP_IADD, r, k);

  Term out = uop_expand_graph(add);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_UNROLL);
  CHECK_EQ(uop_unroll_n_args(out), 1u);
  CHECK_EQ(uop_unroll_axis_id(out, 0), 2u);
  CHECK_EQ(uop_unroll_factor(out, 0), 4u);

  Term inner = heap_read(term_val(out) + 0);
  CHECK_EQ(term_tag(inner), TAG_UOP);
  CHECK_EQ(term_ext(inner), UOP_IADD);

  // No UPCAST RANGEs remain.
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UPCAST, 0), 0);
  // One outer UNROLL wraps everything.
  CHECK_EQ(subtree_count_op(out, UOP_UNROLL, 0), 1);
  // One CONTRACT broadcasts the scalar.
  CHECK_EQ(subtree_count_op(out, UOP_CONTRACT, 0), 1);

  thvm_free();
  TEST_REPORT();
}

// Case 4: two UPCAST ranges on different axes feeding one ALU.
// do_expand should unify into UNROLL with two (axis, F) tuples.
static int test_case4_multi_axis_upcast(void) {
  thvm_init();
  TEST_BEGIN("case4 IADD(UPCAST a=0 F=2, UPCAST a=1 F=3)");

  Term r0 = uop_range(0, KAX_UPCAST, 2);
  Term r1 = uop_range(1, KAX_UPCAST, 3);
  Term add = uop_int_binary(UOP_IADD, r0, r1);

  Term out = uop_expand_graph(add);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_UNROLL);
  CHECK_EQ(uop_unroll_n_args(out), 2u);
  // Sorted by axis_id ascending.
  CHECK_EQ(uop_unroll_axis_id(out, 0), 0u);
  CHECK_EQ(uop_unroll_factor(out, 0), 2u);
  CHECK_EQ(uop_unroll_axis_id(out, 1), 1u);
  CHECK_EQ(uop_unroll_factor(out, 1), 3u);

  Term inner = heap_read(term_val(out) + 0);
  CHECK_EQ(term_tag(inner), TAG_UOP);
  CHECK_EQ(term_ext(inner), UOP_IADD);
  // Both srcs of IADD are GEPs swizzling the corresponding VCONSTs into
  // the unified 2*3=6-wide lane order.
  Term a = heap_read(term_val(inner) + 0);
  Term b = heap_read(term_val(inner) + 1);
  CHECK_EQ(term_tag(a), TAG_UOP);
  CHECK_EQ(term_tag(b), TAG_UOP);
  CHECK_EQ(term_ext(a), UOP_GEP);
  CHECK_EQ(term_ext(b), UOP_GEP);
  CHECK_EQ(uop_gep_n_idx(a), 6u);
  CHECK_EQ(uop_gep_n_idx(b), 6u);

  // No UPCAST RANGEs remain.
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UPCAST, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 5: REDUCE over an UPCAST'd axis.  fix_reduce_unroll pulls the
// axis into a CONTRACT and drops it from the REDUCE.  do_expand then
// lifts the IADD through UNROLL, and do_contract collapses the
// CONTRACT(UNROLL(...)) into a GEP.  Net post-expansion shape:
//   REDUCE(GEP(IADD(VCONST, CONTRACT(k))), axes=[])
// Mirrors tinygrad expander.py end-to-end: a "horizontal" reduce over
// the gathered vector replaces the original axis loop.
static int test_case5_reduce_over_upcast(void) {
  thvm_init();
  TEST_BEGIN("case5 REDUCE(IADD(UPCAST axis=2, k), axes=[2]) -> "
             "REDUCE(GEP(IADD(VCONST, CONTRACT(k))), axes=[])");

  Term r = uop_range(/*axis_id=*/2, /*axis_type=*/KAX_UPCAST, /*extent=*/4);
  Term k = uop_const(DT_INT32, /*bits=*/7);
  Term add = uop_int_binary(UOP_IADD, r, k);
  // Multi-axis REDUCE over axis 2 only.
  u32 axes[1] = { 2 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, axes, add);

  Term out = uop_expand_graph(red);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_REDUCE);
  // fix_reduce_unroll dropped the axis (consumed by CONTRACT); the
  // CONTRACT(UNROLL(...)) was then collapsed by do_contract.
  CHECK_EQ(uop_reduce_n_axes(out), 0u);
  CHECK_EQ(uop_reduce_kind(out), (u32)REDUCE_SUM);

  // src of REDUCE is now a GEP over the unrolled body.
  Term red_src = heap_read(term_val(out) + 0);
  CHECK_EQ(term_tag(red_src), TAG_UOP);
  CHECK_EQ(term_ext(red_src), UOP_GEP);
  CHECK_EQ(uop_gep_n_idx(red_src), 4u);   // F=4 lanes gathered

  // No UPCAST RANGEs remain anywhere.
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UPCAST, 0), 0);
  // No CONTRACT remains in the value subtree -- do_contract collapsed
  // it.  The CONTRACT wrapping the scalar k stays (broadcast).
  // Count of UNROLLs in the post-expansion graph: zero (everything was
  // consumed by CONTRACT/GEP folding).
  CHECK_EQ(subtree_count_op(out, UOP_UNROLL, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 6: UPCAST + UNROLL on same kernel.  Both axis types get rewritten;
// the resulting graph carries both axes in its UNROLL wrappers.
static int test_case6_upcast_and_unroll(void) {
  thvm_init();
  TEST_BEGIN("case6 IMUL(UPCAST axis=0 F=2, UNROLL axis=1 F=4)");

  Term r0 = uop_range(0, KAX_UPCAST, 2);
  Term r1 = uop_range(1, KAX_UNROLL, 4);
  Term mul = uop_int_binary(UOP_IMUL, r0, r1);

  Term out = uop_expand_graph(mul);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_UNROLL);
  CHECK_EQ(uop_unroll_n_args(out), 2u);
  CHECK_EQ(uop_unroll_axis_id(out, 0), 0u);
  CHECK_EQ(uop_unroll_factor(out, 0), 2u);
  CHECK_EQ(uop_unroll_axis_id(out, 1), 1u);
  CHECK_EQ(uop_unroll_factor(out, 1), 4u);

  // Neither UPCAST nor UNROLL'd RANGE remains.
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UPCAST, 0), 0);
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UNROLL, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 7: nested ops -- NEG(IADD(UPCAST, k)) -- do_expand should fire
// at both IADD and NEG, ending with one outer UNROLL.
static int test_case7_nested_ops(void) {
  thvm_init();
  TEST_BEGIN("case7 NEG(IADD(UPCAST, k)) -> one outer UNROLL");

  Term r = uop_range(0, KAX_UPCAST, 4);
  Term k = uop_const(DT_INT32, 5);
  Term add = uop_int_binary(UOP_IADD, r, k);
  Term neg = uop_unary(UOP_NEG, add);

  Term out = uop_expand_graph(neg);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_UNROLL);
  CHECK_EQ(uop_unroll_n_args(out), 1u);
  CHECK_EQ(uop_unroll_axis_id(out, 0), 0u);
  CHECK_EQ(uop_unroll_factor(out, 0), 4u);

  Term inner = heap_read(term_val(out) + 0);
  CHECK_EQ(term_tag(inner), TAG_UOP);
  CHECK_EQ(term_ext(inner), UOP_NEG);

  // Just ONE outer UNROLL -- the double-UNROLL fold collapsed the
  // intermediate UNROLL(IADD) + UNROLL(NEG).
  CHECK_EQ(subtree_count_op(out, UOP_UNROLL, 0), 1);
  // No UPCAST RANGE remains.
  CHECK_EQ(subtree_has_range_with_axis_type(out, KAX_UPCAST, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 8: hash-cons: identical UNROLL/VCONST/CONTRACT/GEP construction
// returns identical Terms.
static int test_case8_hash_cons(void) {
  thvm_init();
  TEST_BEGIN("case8 hash-cons on UNROLL/VCONST/CONTRACT/GEP");

  u32 bits[4] = { 0, 1, 2, 3 };
  Term v1 = uop_vconst(DT_INT32, 4, bits);
  Term v2 = uop_vconst(DT_INT32, 4, bits);
  CHECK_EQ(v1, v2);

  u32 ax[1] = { 7 };
  u32 fa[1] = { 4 };
  Term u1 = uop_unroll(v1, 1, ax, fa);
  Term u2 = uop_unroll(v1, 1, ax, fa);
  CHECK_EQ(u1, u2);

  Term c1 = uop_contract(v1, 1, ax, fa);
  Term c2 = uop_contract(v1, 1, ax, fa);
  CHECK_EQ(c1, c2);

  u32 idxs[4] = { 0, 1, 2, 3 };
  Term g1 = uop_gep(v1, 4, idxs);
  Term g2 = uop_gep(v1, 4, idxs);
  CHECK_EQ(g1, g2);

  thvm_free();
  TEST_REPORT();
}

int main(void) {
  int rc = 0;
  rc |= test_case1_bare_upcast_range();
  rc |= test_case2_loop_range_untouched();
  rc |= test_case3_alu_through_upcast();
  rc |= test_case4_multi_axis_upcast();
  rc |= test_case5_reduce_over_upcast();
  rc |= test_case6_upcast_and_unroll();
  rc |= test_case7_nested_ops();
  rc |= test_case8_hash_cons();
  return rc;
}
