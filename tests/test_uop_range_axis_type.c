// test_uop_range_axis_type.c -- Phase E1 acceptance:
//   - UOP_RANGE field accessors (axis_id, axis_type, extent) round-
//     trip the heap layout documented in thvm.h.
//   - uop_range_with_axis_type returns a hash-cons-shared Term equal
//     to direct construction with the new axis_type.
//   - A UPatRule that consumes UOP_RANGE.axis_type fires end-to-end:
//     promotes a UOP_OPT(UOP_RANGE, LOCAL, ext)-wrapped range whose
//     axis_type is KAX_LOOP into a fresh UOP_RANGE with
//     axis_type = KAX_LOCAL, replicating the simplest apply_opt
//     mutation (KOP_GLOBAL / KOP_LOCAL stamping an existing axis)
//     in declarative form.
//
// Phase E1 leaves apply_opt.c + KernelAxes.axis_types[] as primary
// source of truth; this is groundwork for the broader port (E2..En).
// See docs/plans/ideal_pipeline.md row E and the Phase E section of
// docs/plans/ideal_pipeline_handoff.md.

#include "../src/thvm.c"
#include "test.h"

// Rule: UOP_OPT(?range, kind=LOCAL, factor=ext) -> uop_range_with_axis_type(range, KAX_LOCAL).
// Demonstrates a UPatRule reading and rewriting UOP_RANGE.axis_type
// without touching KernelAxes.  Mirrors KOP_LOCAL-on-existing-axis in
// shape (no axis split) so E1 stays minimal; the full E2 wedge will
// add the split-and-stamp variant.
static Term rw_promote_loop_to_local(Term const *bindings, void *ctx) {
  (void)ctx;
  Term opt   = bindings[0];   // captured UOP_OPT node
  Term range = uop_opt_target(opt);
  if (term_tag(range) != TAG_UOP || term_ext(range) != UOP_RANGE) return 0;
  if (uop_range_axis_type(range) != KAX_LOOP) return 0;
  if (uop_opt_kind(opt) != UOP_OPT_LOCAL)     return 0;
  if (uop_opt_factor(opt) != uop_range_extent(range)) return 0;
  return uop_range_with_axis_type(range, KAX_LOCAL);
}

int main(void) {
  thvm_init();

  // === (1) Accessor round-trip ====================================
  TEST_BEGIN("range-axis-type/accessors");
  Term r = uop_range(/*axis_id=*/3, /*axis_type=*/KAX_LOOP, /*extent=*/64);
  CHECK_EQ(uop_range_axis_id(r),   3);
  CHECK_EQ(uop_range_axis_type(r), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_extent(r),    64);

  TEST_BEGIN("range-axis-type/accessors-tag-mismatch");
  // Non-RANGE Term returns 0 (not a heap dereference).
  Term not_a_range = uop_const(DT_INT32, 7);
  CHECK_EQ(uop_range_axis_id(not_a_range),   0);
  CHECK_EQ(uop_range_axis_type(not_a_range), 0);
  CHECK_EQ(uop_range_extent(not_a_range),    0);

  // === (2) uop_range_with_axis_type produces hash-cons-equal Term ==
  TEST_BEGIN("range-axis-type/with-axis-type-equiv-direct");
  // Promote LOOP -> GLOBAL (KAX_GLOBAL=5).  Direct uop_range
  // construction with the new axis_type must hash-cons to the same
  // Term that uop_range_with_axis_type returns.
  Term r_global = uop_range_with_axis_type(r, KAX_GLOBAL);
  Term r_global_direct = uop_range(3, KAX_GLOBAL, 64);
  CHECK_EQ(r_global, r_global_direct);
  CHECK_EQ(uop_range_axis_type(r_global), (u32)KAX_GLOBAL);
  // Original Term is unchanged (UOP_RANGE is hash-consed).
  CHECK_EQ(uop_range_axis_type(r), (u32)KAX_LOOP);

  TEST_BEGIN("range-axis-type/with-axis-type-tag-mismatch");
  // Non-RANGE Term passes through unchanged.
  CHECK_EQ(uop_range_with_axis_type(not_a_range, KAX_LOCAL), not_a_range);

  TEST_BEGIN("range-axis-type/with-axis-type-noop");
  // Same axis_type still hash-conses to the input.
  Term r_same = uop_range_with_axis_type(r, KAX_LOOP);
  CHECK_EQ(r_same, r);

  // === (3) UPatRule end-to-end: OPT(range, LOCAL, ext) -> range' ==
  TEST_BEGIN("range-axis-type/upatrule-promotes-loop-to-local");
  // Build OPT(range, LOCAL, extent=64).  upat_match should walk into
  // the OPT node and bind it to slot 0; the rewrite reads OPT.target,
  // checks the LOOP guard, and emits the LOCAL-stamped range.
  Term opt = uop_opt(r, UOP_OPT_LOCAL, 64);

  // pat: UOP_OPT (capture in bindings[0]).  UOP_OPT has heap arity 3
  // (target, kind, factor) but the rule only needs the target child;
  // explicit nsrc=1 trusts the op_pinned relaxation (UP1) and walks
  // child[0] = target.  We don't constrain target structurally here
  // (the rewrite body re-inspects via uop_opt_target / uop_range_*).
  static UPat const pat_opt_target = {0, 0xFF, 0, -1, NULL, NULL};
  static UPat const pat_opt        = {UOP_OPT, 1, 0, 0, &pat_opt_target, NULL};
  UPatRule rules[] = {{&pat_opt, rw_promote_loop_to_local}};

  Term out = uop_pattern_rewrite(opt, rules, 1, NULL);
  // Expected: a UOP_RANGE whose axis_type is now KAX_LOCAL and whose
  // axis_id/extent match the original.
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(out),   3);
  CHECK_EQ(uop_range_axis_type(out), (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_extent(out),    64);

  // === (4) Negative: rule guards against type / kind / factor mismatch
  TEST_BEGIN("range-axis-type/upatrule-rejects-non-loop");
  // axis_type already KAX_REDUCE -- guard rejects.
  Term r_red       = uop_range(4, KAX_REDUCE, 32);
  Term opt_red     = uop_opt(r_red, UOP_OPT_LOCAL, 32);
  Term out_red     = uop_pattern_rewrite(opt_red, rules, 1, NULL);
  // Rule guard fails -> rewrite returns 0; bridge passes the original
  // node through and uop_graph_rewrite caches it.  Either way, the
  // returned root is structurally the original OPT (with target's
  // axis_type still KAX_REDUCE).
  CHECK_EQ(term_tag(out_red), TAG_UOP);
  CHECK_EQ(term_ext(out_red), UOP_OPT);
  Term out_red_target = uop_opt_target(out_red);
  CHECK_EQ(uop_range_axis_type(out_red_target), (u32)KAX_REDUCE);

  TEST_BEGIN("range-axis-type/upatrule-rejects-wrong-kind");
  // OPT_UNROLL on a LOOP range -- wrong kind, guard rejects.
  Term opt_unroll = uop_opt(r, UOP_OPT_UNROLL, 64);
  Term out_unroll = uop_pattern_rewrite(opt_unroll, rules, 1, NULL);
  CHECK_EQ(term_tag(out_unroll), TAG_UOP);
  CHECK_EQ(term_ext(out_unroll), UOP_OPT);
  CHECK_EQ(uop_opt_kind(out_unroll), (u32)UOP_OPT_UNROLL);

  TEST_BEGIN("range-axis-type/upatrule-rejects-factor-mismatch");
  // factor != extent -- guard rejects (E1 keeps the LOCAL-on-existing-
  // axis case structural; the split case is E2+).
  Term opt_factor = uop_opt(r, UOP_OPT_LOCAL, 8);
  Term out_factor = uop_pattern_rewrite(opt_factor, rules, 1, NULL);
  CHECK_EQ(term_tag(out_factor), TAG_UOP);
  CHECK_EQ(term_ext(out_factor), UOP_OPT);
  CHECK_EQ(uop_opt_factor(out_factor), 8u);

  thvm_free();
  TEST_REPORT();
}
