// test_uop_reduce_collapse.c - arange/reduce-collapse closed-form fold.
//
// Verifies uop_reduce_arange_collapse (port of tinygrad
// codegen/simplify.py pm_reduce_simplify) folds a single-axis SUM REDUCE
// over a pure-RANGE triangular mask to its closed form -- the
// arange/cumsum/one-hot construction -- so the reduce-axis loop is
// eliminated.  Each block checks (a) the REDUCE is gone after the fold
// and (b) the folded expression evaluates to the right count, plus a
// "skips" case so an over-eager fold shows up as a regression.

#include "../src/thvm.c"
#include "test.h"

// 1 iff the DAG rooted at `t` still contains a UOP_REDUCE.
static int has_reduce(Term t, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_REDUCE) return 1;
  if (op == UOP_BUFFER || op == UOP_KERNEL) return 0;
  u8 ar = uop_arity(op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar; i++)
    if (has_reduce(heap_read(loc + i), depth + 1)) return 1;
  return 0;
}

// Evaluate an integer index expression with the free RANGE leaf
// `outer_aid` bound to `outer_val` (every other RANGE treated as its
// own iter is NOT expected here -- the folded form references only the
// outer axis + consts).  Minimal interpreter over the I* ops.
static i64 eval_idx(Term t, u32 outer_aid, i64 outer_val, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST: return (i32)term_val(heap_read(loc + 0));
    case UOP_RANGE:
      return ((u32)term_val(heap_read(loc + 0)) == outer_aid) ? outer_val : 0;
    case UOP_INVALID: return 0;
    case UOP_IADD: return eval_idx(heap_read(loc+0),outer_aid,outer_val,depth+1)
                        + eval_idx(heap_read(loc+1),outer_aid,outer_val,depth+1);
    case UOP_ISUB: return eval_idx(heap_read(loc+0),outer_aid,outer_val,depth+1)
                        - eval_idx(heap_read(loc+1),outer_aid,outer_val,depth+1);
    case UOP_IMUL: return eval_idx(heap_read(loc+0),outer_aid,outer_val,depth+1)
                        * eval_idx(heap_read(loc+1),outer_aid,outer_val,depth+1);
    case UOP_ILT:  return (eval_idx(heap_read(loc+0),outer_aid,outer_val,depth+1)
                         < eval_idx(heap_read(loc+1),outer_aid,outer_val,depth+1)) ? 1 : 0;
    case UOP_IWHERE:
      return eval_idx(heap_read(loc+0),outer_aid,outer_val,depth+1)
           ? eval_idx(heap_read(loc+1),outer_aid,outer_val,depth+1)
           : eval_idx(heap_read(loc+2),outer_aid,outer_val,depth+1);
    default: return 0;
  }
}

int main(void) {
  thvm_init();

  // === arange upper-tail: SUM_r IWHERE(ILT(6, r0 + r1), 1, 0), r1 ext 8 ===
  // This is the exact thvm form for arange(8): out[r0] = count of r1 in
  // [0,8) with r0+r1 > 6.  Closed form: 8 - clamp(7 - r0, 0, 8).
  TEST_BEGIN("reduce-collapse/arange-upper-tail-folds");
  {
    u32 OUTER = 0, INNER = 1;
    Term r0 = uop_range(OUTER, KAX_LOOP, 8);
    Term r1 = uop_range(INNER, KAX_LOOP, 8);
    Term six = uop_const(DT_INT32, 6);
    Term one = uop_const(DT_INT32, 1);
    Term affine = uop_int_binary(UOP_IADD, r0, r1);    // r0 + r1
    Term cond = uop_int_binary(UOP_ILT, six, affine);  // 6 < r0+r1
    Term body = uop_iwhere(cond, one, uop_invalid());
    Term red = uop_reduce(REDUCE_SUM, INNER, body);
    Term folded = uop_reduce_arange_collapse(red);
    CHECK(folded != red);
    CHECK(!has_reduce(folded, 0));
    // out[r0] should equal r0 + 1 (clamped to [0,8]); arange's epilogue
    // then subtracts 1 to give r0.  Spot-check several r0.
    for (i64 v = 0; v <= 7; v++) {
      i64 want = v + 1;            // count of r1: r1 > 6 - v  ==  v+1 (v<=6), 8 (v=7)
      if (v == 7) want = 8;
      CHECK_EQ(eval_idx(folded, OUTER, v, 0), want);
    }
  }

  // === lower-tail: SUM_r IWHERE(ILT(r1, r0), 1, 0), r1 ext 8 ===
  // count of r1 in [0,8) with r1 < r0  ==  clamp(r0, 0, 8).
  TEST_BEGIN("reduce-collapse/lower-tail-folds");
  {
    u32 OUTER = 2, INNER = 3;
    Term r0 = uop_range(OUTER, KAX_LOOP, 8);
    Term r1 = uop_range(INNER, KAX_LOOP, 8);
    Term one = uop_const(DT_INT32, 1);
    Term cond = uop_int_binary(UOP_ILT, r1, r0);    // r1 < r0
    Term body = uop_iwhere(cond, one, uop_const(DT_INT32, 0));
    Term red = uop_reduce(REDUCE_SUM, INNER, body);
    Term folded = uop_reduce_arange_collapse(red);
    CHECK(folded != red);
    CHECK(!has_reduce(folded, 0));
    for (i64 v = 0; v <= 7; v++) CHECK_EQ(eval_idx(folded, OUTER, v, 0), v);
  }

  // === skips: body references the reduce axis in `val` (not a pure mask) ===
  // SUM_r IWHERE(ILT(6, r0+r1), r1, 0): val depends on r1 -> not foldable
  // by the bound rule; must be left as a REDUCE.
  TEST_BEGIN("reduce-collapse/skips-axis-dependent-val");
  {
    u32 OUTER = 4, INNER = 5;
    Term r0 = uop_range(OUTER, KAX_LOOP, 8);
    Term r1 = uop_range(INNER, KAX_LOOP, 8);
    Term six = uop_const(DT_INT32, 6);
    Term affine = uop_int_binary(UOP_IADD, r0, r1);
    Term cond = uop_int_binary(UOP_ILT, six, affine);
    Term body = uop_iwhere(cond, r1, uop_const(DT_INT32, 0));   // val == r1
    Term red = uop_reduce(REDUCE_SUM, INNER, body);
    Term folded = uop_reduce_arange_collapse(red);
    CHECK_EQ(folded, red);          // unchanged
    CHECK(has_reduce(folded, 0));
  }

  // === skips: MAX reduce (only SUM folds) ===
  TEST_BEGIN("reduce-collapse/skips-max-reduce");
  {
    u32 OUTER = 6, INNER = 7;
    Term r0 = uop_range(OUTER, KAX_LOOP, 8);
    Term r1 = uop_range(INNER, KAX_LOOP, 8);
    Term six = uop_const(DT_INT32, 6);
    Term one = uop_const(DT_INT32, 1);
    Term affine = uop_int_binary(UOP_IADD, r0, r1);
    Term cond = uop_int_binary(UOP_ILT, six, affine);
    Term body = uop_iwhere(cond, one, uop_const(DT_INT32, 0));
    Term red = uop_reduce(REDUCE_MAX, INNER, body);
    Term folded = uop_reduce_arange_collapse(red);
    CHECK_EQ(folded, red);
    CHECK(has_reduce(folded, 0));
  }

  TEST_REPORT();
}
