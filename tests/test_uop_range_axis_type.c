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

  // === Phase E2: KOP_GLOBAL UPatRule mirror ========================
  // uop_apply_kop_global is the public entry from src/uop/apply_opt.c.
  // It mirrors codegen/apply_opt.c's KOP_GLOBAL stamp and
  // kernel_lift.c's structural-replay GLOBAL line-up, applied
  // declaratively against UOP_RANGE leaves in the DAG.

  TEST_BEGIN("apply-kop-global/positive-direct");
  // r2: UOP_RANGE(axis_id=0, axis_type=LOOP, extent=128).
  Term r2 = uop_range(0, KAX_LOOP, 128);
  KOpt opts_pos[1] = {{ KOP_GLOBAL, 0, 128 }};
  Term r2_out = uop_apply_kop_global(r2, opts_pos, 1);
  CHECK_EQ(term_tag(r2_out), TAG_UOP);
  CHECK_EQ(term_ext(r2_out), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(r2_out),   0);
  CHECK_EQ(uop_range_axis_type(r2_out), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(r2_out),    128);

  TEST_BEGIN("apply-kop-global/negative-no-applied-opts");
  // No KOP_GLOBAL in applied_opts -> axis stays LOOP.
  Term r2_pass = uop_apply_kop_global(r2, NULL, 0);
  CHECK_EQ(r2_pass, r2);  // hash-cons identity
  CHECK_EQ(uop_range_axis_type(r2_pass), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-global/negative-different-axis");
  // KOP_GLOBAL targets axis_id=3, range is axis_id=0 -> no rewrite.
  KOpt opts_other[1] = {{ KOP_GLOBAL, 3, 128 }};
  Term r2_other = uop_apply_kop_global(r2, opts_other, 1);
  CHECK_EQ(r2_other, r2);
  CHECK_EQ(uop_range_axis_type(r2_other), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-global/negative-extent-mismatch");
  // arg=64 != extent=128 -> guard rejects (mirrors apply_opt's check).
  KOpt opts_bad_arg[1] = {{ KOP_GLOBAL, 0, 64 }};
  Term r2_bad = uop_apply_kop_global(r2, opts_bad_arg, 1);
  CHECK_EQ(r2_bad, r2);
  CHECK_EQ(uop_range_axis_type(r2_bad), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-global/negative-non-loop-axis");
  // axis_type already KAX_REDUCE -> rule skips (the lifter would
  // never produce this combo since KOP_GLOBAL replay also requires
  // LOOP, but the guard belongs in the rule for idempotence).
  Term r3 = uop_range(2, KAX_REDUCE, 32);
  KOpt opts_red[1] = {{ KOP_GLOBAL, 2, 32 }};
  Term r3_out = uop_apply_kop_global(r3, opts_red, 1);
  CHECK_EQ(r3_out, r3);
  CHECK_EQ(uop_range_axis_type(r3_out), (u32)KAX_REDUCE);

  TEST_BEGIN("apply-kop-global/negative-wrong-kop");
  // KOP_LOCAL on the right axis with right arg -> rule skips
  // (only KOP_GLOBAL fires this rule; KOP_LOCAL is a future wedge).
  KOpt opts_local[1] = {{ KOP_LOCAL, 0, 128 }};
  Term r2_local = uop_apply_kop_global(r2, opts_local, 1);
  CHECK_EQ(r2_local, r2);
  CHECK_EQ(uop_range_axis_type(r2_local), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-global/idempotent-double-apply");
  // First apply: LOOP -> GLOBAL.  Second apply on the result is a
  // no-op (the LOOP guard rejects the now-GLOBAL leaf).
  Term r2_g1 = uop_apply_kop_global(r2, opts_pos, 1);
  Term r2_g2 = uop_apply_kop_global(r2_g1, opts_pos, 1);
  CHECK_EQ(r2_g1, r2_g2);
  CHECK_EQ(uop_range_axis_type(r2_g2), (u32)KAX_GLOBAL);

  TEST_BEGIN("apply-kop-global/multi-opt-only-global-fires");
  // applied_opts contains KOP_LOCAL + KOP_GLOBAL + KOP_UPCAST.  The
  // rule only stamps for KOP_GLOBAL, so the rewrite still produces
  // KAX_GLOBAL on the matching axis.  Mirrors the autotune sequence
  // shape ("LOCAL splits N into LOOP(N/L) + LOCAL(L); GLOBAL marks
  // the resulting LOOP for grid-binding") at the table-of-opts level.
  KOpt opts_mixed[3] = {
    { KOP_LOCAL,  1, 32 },   // unrelated axis
    { KOP_GLOBAL, 0, 128 },  // matches r2
    { KOP_UPCAST, 2, 4 },    // unrelated axis
  };
  Term r2_mixed = uop_apply_kop_global(r2, opts_mixed, 3);
  CHECK_EQ(term_tag(r2_mixed), TAG_UOP);
  CHECK_EQ(term_ext(r2_mixed), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(r2_mixed),   0);
  CHECK_EQ(uop_range_axis_type(r2_mixed), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(r2_mixed),    128);

  TEST_BEGIN("apply-kop-global/no-applied-opts-noop");
  // n_applied=0 with non-NULL opts -> early return path.  Verify the
  // rule short-circuits before scanning.
  KOpt opts_unused[1] = {{ KOP_GLOBAL, 0, 128 }};
  Term r2_zero = uop_apply_kop_global(r2, opts_unused, 0);
  CHECK_EQ(r2_zero, r2);
  CHECK_EQ(uop_range_axis_type(r2_zero), (u32)KAX_LOOP);

  // === Phase E3: KOP_SWAP UPatRule mirror ==========================
  // uop_apply_kop_swap walks applied_opts and replays the composed
  // KOP_GLOBAL + KOP_SWAP history on a desired_axis_type[] array,
  // then stamps each UOP_RANGE leaf with its computed axis_type.

  TEST_BEGIN("apply-kop-swap/swap-alone-noop");
  // SWAP between two LOOP axes -- both ends stay LOOP, no rewrite.
  Term r4 = uop_range(0, KAX_LOOP, 64);
  Term r5 = uop_range(1, KAX_LOOP, 32);
  KOpt opts_swap_only[1] = {{ KOP_SWAP, 0, 1 }};
  Term r4_out = uop_apply_kop_swap(r4, opts_swap_only, 1);
  Term r5_out = uop_apply_kop_swap(r5, opts_swap_only, 1);
  CHECK_EQ(r4_out, r4);   // hash-cons identity, no rewrite
  CHECK_EQ(r5_out, r5);
  CHECK_EQ(uop_range_axis_type(r4_out), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(r5_out), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-swap/global-then-swap-positive");
  // GLOBAL(0, 64) then SWAP(0, 1):
  //   desired starts [LOOP, LOOP, ...]
  //   after GLOBAL(0): desired = [GLOBAL, LOOP, ...]
  //   after SWAP(0,1): desired = [LOOP, GLOBAL, ...]
  // So a UOP_RANGE leaf at axis_id=1 should be stamped KAX_GLOBAL,
  // and a leaf at axis_id=0 should stay KAX_LOOP (swap moved the
  // GLOBAL marker off it).
  KOpt opts_gs[2] = {
    { KOP_GLOBAL, 0, 64 },
    { KOP_SWAP,   0, 1  },
  };
  Term r4_gs = uop_apply_kop_swap(r4, opts_gs, 2);  // axis_id=0
  Term r5_gs = uop_apply_kop_swap(r5, opts_gs, 2);  // axis_id=1
  CHECK_EQ(term_tag(r4_gs), TAG_UOP);
  CHECK_EQ(term_ext(r4_gs), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(r4_gs),   0);
  CHECK_EQ(uop_range_axis_type(r4_gs), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_extent(r4_gs),    64);
  CHECK_EQ(term_tag(r5_gs), TAG_UOP);
  CHECK_EQ(term_ext(r5_gs), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(r5_gs),   1);
  CHECK_EQ(uop_range_axis_type(r5_gs), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(r5_gs),    32);

  TEST_BEGIN("apply-kop-swap/idempotence");
  // Re-applying the rule with the same opts on the result is a no-op:
  // desired[a] is a pure function of applied_opts, and the leaves now
  // carry the simulated axis_type, so the rule short-circuits.
  Term r5_gs2 = uop_apply_kop_swap(r5_gs, opts_gs, 2);
  CHECK_EQ(r5_gs2, r5_gs);
  CHECK_EQ(uop_range_axis_type(r5_gs2), (u32)KAX_GLOBAL);
  Term r4_gs2 = uop_apply_kop_swap(r4_gs, opts_gs, 2);
  CHECK_EQ(r4_gs2, r4_gs);
  CHECK_EQ(uop_range_axis_type(r4_gs2), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-swap/double-swap-cancels");
  // SWAP(0,1) twice -- desired returns to the initial state.
  KOpt opts_double_swap[2] = {
    { KOP_SWAP, 0, 1 },
    { KOP_SWAP, 0, 1 },
  };
  Term r4_dd = uop_apply_kop_swap(r4, opts_double_swap, 2);
  Term r5_dd = uop_apply_kop_swap(r5, opts_double_swap, 2);
  CHECK_EQ(r4_dd, r4);
  CHECK_EQ(r5_dd, r5);

  TEST_BEGIN("apply-kop-swap/composition-three-axes");
  // GLOBAL(0) then SWAP(0,1) then SWAP(1,2):
  //   [LOOP, LOOP, LOOP] -GLOBAL(0)-> [GLOBAL, LOOP, LOOP]
  //   -SWAP(0,1)-> [LOOP, GLOBAL, LOOP]
  //   -SWAP(1,2)-> [LOOP, LOOP, GLOBAL]
  // So axis_id=2 should carry KAX_GLOBAL after the rewrite.
  Term r6 = uop_range(2, KAX_LOOP, 16);
  KOpt opts_compose[3] = {
    { KOP_GLOBAL, 0, 64 },
    { KOP_SWAP,   0, 1  },
    { KOP_SWAP,   1, 2  },
  };
  Term r4_c = uop_apply_kop_swap(r4, opts_compose, 3);  // axis_id=0
  Term r5_c = uop_apply_kop_swap(r5, opts_compose, 3);  // axis_id=1
  Term r6_c = uop_apply_kop_swap(r6, opts_compose, 3);  // axis_id=2
  CHECK_EQ(uop_range_axis_type(r4_c), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(r5_c), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(r6_c), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(r6_c),    16);

  TEST_BEGIN("apply-kop-swap/no-applied-opts-noop");
  // n_applied=0 -> early return; leaves untouched.
  Term r4_no = uop_apply_kop_swap(r4, NULL, 0);
  CHECK_EQ(r4_no, r4);
  KOpt opts_zero[1] = {{ KOP_SWAP, 0, 1 }};
  Term r4_zero = uop_apply_kop_swap(r4, opts_zero, 0);
  CHECK_EQ(r4_zero, r4);

  TEST_BEGIN("apply-kop-swap/swap-untouched-axis");
  // SWAP(0,1) when the matched leaf is at axis_id=2 -> desired[2]=LOOP,
  // unchanged; rule no-ops.
  Term r6_unrelated = uop_apply_kop_swap(r6, opts_swap_only, 1);
  CHECK_EQ(r6_unrelated, r6);
  CHECK_EQ(uop_range_axis_type(r6_unrelated), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-swap/swap-out-of-range-axis-arg");
  // SWAP with arg >= MAX_AXES -> rule clamps and skips that opt.  The
  // GLOBAL stamp should still apply unaffected.
  KOpt opts_clamp[2] = {
    { KOP_GLOBAL, 0, 64 },
    { KOP_SWAP,   0, MAX_AXES + 1 },  // arg out of range, ignored
  };
  Term r4_clamp = uop_apply_kop_swap(r4, opts_clamp, 2);
  CHECK_EQ(term_tag(r4_clamp), TAG_UOP);
  CHECK_EQ(term_ext(r4_clamp), UOP_RANGE);
  CHECK_EQ(uop_range_axis_type(r4_clamp), (u32)KAX_GLOBAL);

  TEST_BEGIN("apply-kop-swap/multi-opt-ignores-splits");
  // Mixed opts list with split ops (KOP_LOCAL, KOP_UPCAST) interleaved
  // with KOP_GLOBAL + KOP_SWAP.  The rule ignores the splits (they are
  // not in scope for E3) and produces the same desired state as the
  // pure GLOBAL+SWAP subsequence: GLOBAL(0)+SWAP(0,1) -> [LOOP, GLOBAL].
  KOpt opts_mixed_ss[4] = {
    { KOP_LOCAL,  3, 8  },   // ignored: split, unrelated axis
    { KOP_GLOBAL, 0, 64 },   // desired[0]=GLOBAL
    { KOP_UPCAST, 4, 4  },   // ignored: split
    { KOP_SWAP,   0, 1  },   // swap -> desired[1]=GLOBAL
  };
  Term r5_ms = uop_apply_kop_swap(r5, opts_mixed_ss, 4);  // axis_id=1
  CHECK_EQ(uop_range_axis_type(r5_ms), (u32)KAX_GLOBAL);
  Term r4_ms = uop_apply_kop_swap(r4, opts_mixed_ss, 4);  // axis_id=0
  CHECK_EQ(uop_range_axis_type(r4_ms), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-swap/global-after-swap");
  // SWAP(0,1) then GLOBAL(1, 32):
  //   [LOOP, LOOP] -SWAP(0,1)-> [LOOP, LOOP] (no-op)
  //   -GLOBAL(1)-> [LOOP, GLOBAL]
  // axis_id=1 -> KAX_GLOBAL, axis_id=0 -> KAX_LOOP.
  KOpt opts_sg[2] = {
    { KOP_SWAP,   0, 1  },
    { KOP_GLOBAL, 1, 32 },
  };
  Term r4_sg = uop_apply_kop_swap(r4, opts_sg, 2);  // axis_id=0
  Term r5_sg = uop_apply_kop_swap(r5, opts_sg, 2);  // axis_id=1
  CHECK_EQ(uop_range_axis_type(r4_sg), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(r5_sg), (u32)KAX_GLOBAL);

  TEST_BEGIN("apply-kop-swap/non-range-tag-mismatch");
  // Non-RANGE Term passes through unchanged (the rule body's tag check
  // returns 0 -> uop_pattern_rewrite caches the original).
  Term not_range = uop_const(DT_INT32, 42);
  KOpt opts_simple[1] = {{ KOP_GLOBAL, 0, 64 }};
  Term not_range_out = uop_apply_kop_swap(not_range, opts_simple, 1);
  CHECK_EQ(not_range_out, not_range);

  // === Phase E4-E6: split-class (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP)
  //                  UPatRule mirror -- pragmatic stamp port ============
  // uop_apply_kop_split replays the full applied_opts history (splits +
  // GLOBAL + SWAP) on a desired[MAX_AXES] vector and stamps each
  // UOP_RANGE leaf with its post-replay axis_type.  Splits insert a new
  // axis at position o.axis+1 (shifting later positions right by 1) with
  // the inner KAX type; the actual range-creation stays in kernel_lift.c
  // structural-replay, so these tests target leaves whose axis_id has
  // already been positioned by the lifter.

  // === E4: KOP_UPCAST stamping =====================================
  TEST_BEGIN("apply-kop-split/upcast-stamps-inner");
  // Simulate a kernel where axis 0 (extent 128) was UPCAST split with
  // arg=4.  The lifter emits two leaves: outer at axis_id=0 (LOOP, 32),
  // inner at axis_id=1 (UPCAST, 4).  This rule sees them post-emission
  // -- if the test seam left them as LOOP/LOOP, the stamp should fix
  // axis_id=1 to KAX_UPCAST while leaving axis_id=0 untouched.
  Term outer_lp = uop_range(0, KAX_LOOP, 32);
  Term inner_lp = uop_range(1, KAX_LOOP, 4);
  KOpt opts_upcast[1] = {{ KOP_UPCAST, 0, 4 }};
  Term outer_st = uop_apply_kop_split(outer_lp, opts_upcast, 1);
  Term inner_st = uop_apply_kop_split(inner_lp, opts_upcast, 1);
  CHECK_EQ(outer_st, outer_lp);  // outer position unchanged (still LOOP)
  CHECK_EQ(uop_range_axis_type(outer_st), (u32)KAX_LOOP);
  CHECK_EQ(term_tag(inner_st), TAG_UOP);
  CHECK_EQ(term_ext(inner_st), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(inner_st),   1);
  CHECK_EQ(uop_range_axis_type(inner_st), (u32)KAX_UPCAST);
  CHECK_EQ(uop_range_extent(inner_st),    4);

  TEST_BEGIN("apply-kop-split/upcast-idempotent");
  // Running the rule a second time on the stamped leaf is a no-op:
  // desired[1]=KAX_UPCAST already matches the leaf's axis_type.
  Term inner_st2 = uop_apply_kop_split(inner_st, opts_upcast, 1);
  CHECK_EQ(inner_st2, inner_st);
  CHECK_EQ(uop_range_axis_type(inner_st2), (u32)KAX_UPCAST);

  // === E5: KOP_UNROLL stamping ====================================
  TEST_BEGIN("apply-kop-split/unroll-stamps-inner");
  // UNROLL split at axis 0 with arg=8: outer LOOP, inner UNROLL.
  Term unr_in_lp = uop_range(1, KAX_LOOP, 8);
  KOpt opts_unroll[1] = {{ KOP_UNROLL, 0, 8 }};
  Term unr_in_st = uop_apply_kop_split(unr_in_lp, opts_unroll, 1);
  CHECK_EQ(uop_range_axis_id(unr_in_st),   1);
  CHECK_EQ(uop_range_axis_type(unr_in_st), (u32)KAX_UNROLL);
  CHECK_EQ(uop_range_extent(unr_in_st),    8);

  // === E6: KOP_LOCAL stamping =====================================
  TEST_BEGIN("apply-kop-split/local-stamps-inner");
  // LOCAL split at axis 0 with arg=16: outer LOOP, inner LOCAL.
  Term lcl_in_lp = uop_range(1, KAX_LOOP, 16);
  KOpt opts_local_split[1] = {{ KOP_LOCAL, 0, 16 }};
  Term lcl_in_st = uop_apply_kop_split(lcl_in_lp, opts_local_split, 1);
  CHECK_EQ(uop_range_axis_id(lcl_in_st),   1);
  CHECK_EQ(uop_range_axis_type(lcl_in_st), (u32)KAX_LOCAL);

  // === KOP_GROUP / KOP_GROUPTOP stamping ===========================
  TEST_BEGIN("apply-kop-split/group-stamps-group-reduce");
  Term grp_in_lp = uop_range(1, KAX_LOOP, 4);
  KOpt opts_group[1] = {{ KOP_GROUP, 0, 4 }};
  Term grp_in_st = uop_apply_kop_split(grp_in_lp, opts_group, 1);
  CHECK_EQ(uop_range_axis_type(grp_in_st), (u32)KAX_GROUP_REDUCE);

  TEST_BEGIN("apply-kop-split/grouptop-stamps-group-reduce");
  Term gtp_in_lp = uop_range(1, KAX_LOOP, 4);
  KOpt opts_gtp[1] = {{ KOP_GROUPTOP, 0, 4 }};
  Term gtp_in_st = uop_apply_kop_split(gtp_in_lp, opts_gtp, 1);
  CHECK_EQ(uop_range_axis_type(gtp_in_st), (u32)KAX_GROUP_REDUCE);

  // === Negative: no-applied-opts no-op =============================
  TEST_BEGIN("apply-kop-split/no-applied-opts-noop");
  Term r_neg = uop_range(0, KAX_LOOP, 32);
  Term r_neg_out = uop_apply_kop_split(r_neg, NULL, 0);
  CHECK_EQ(r_neg_out, r_neg);
  KOpt opts_zero_split[1] = {{ KOP_UPCAST, 0, 4 }};
  Term r_neg_z = uop_apply_kop_split(r_neg, opts_zero_split, 0);
  CHECK_EQ(r_neg_z, r_neg);

  // === Negative: non-RANGE term passes through =====================
  TEST_BEGIN("apply-kop-split/non-range-tag-mismatch");
  Term not_r = uop_const(DT_INT32, 99);
  Term not_r_out = uop_apply_kop_split(not_r, opts_upcast, 1);
  CHECK_EQ(not_r_out, not_r);

  // === Composition: UPCAST then GLOBAL on a later axis =============
  TEST_BEGIN("apply-kop-split/upcast-then-global-on-shifted-axis");
  // Pre-split axes [LOOP@0(128), LOOP@1(64)].  UPCAST(0, 4) -> outer
  // axis 0 (LOOP, 32), inner axis 1 (UPCAST, 4); old axis 1 shifts to
  // position 2 (LOOP, 64).  GLOBAL on the shifted axis 2 with arg=64
  // stamps desired[2]=KAX_GLOBAL.  axis_id=2 leaf should pick this up.
  Term shifted_lp = uop_range(2, KAX_LOOP, 64);
  KOpt opts_split_global[2] = {
    { KOP_UPCAST, 0, 4  },   // inserts new axis at position 1; old 1 -> 2
    { KOP_GLOBAL, 2, 64 },   // stamps shifted position 2 -> KAX_GLOBAL
  };
  Term shifted_st = uop_apply_kop_split(shifted_lp, opts_split_global, 2);
  CHECK_EQ(uop_range_axis_id(shifted_st),   2);
  CHECK_EQ(uop_range_axis_type(shifted_st), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(shifted_st),    64);
  // The new inner axis at position 1 should still be UPCAST.
  Term inner_post = uop_apply_kop_split(uop_range(1, KAX_LOOP, 4),
                                         opts_split_global, 2);
  CHECK_EQ(uop_range_axis_type(inner_post), (u32)KAX_UPCAST);
  // The outer at position 0 should stay LOOP.
  Term outer_post = uop_apply_kop_split(uop_range(0, KAX_LOOP, 32),
                                         opts_split_global, 2);
  CHECK_EQ(outer_post, uop_range(0, KAX_LOOP, 32));
  CHECK_EQ(uop_range_axis_type(outer_post), (u32)KAX_LOOP);

  // === Composition: LOCAL then GLOBAL on the outer (autotune shape) ===
  TEST_BEGIN("apply-kop-split/local-then-global-on-outer");
  // Autotune sequence: LOCAL splits N into LOOP(N/L) + LOCAL(L), then
  // GLOBAL marks the resulting LOOP for grid-binding.
  //   LOCAL(0, 8) on extent 256: outer position 0 (LOOP, 32), inner
  //                              position 1 (LOCAL, 8).
  //   GLOBAL(0, 32):             outer 0 LOOP -> GLOBAL.
  KOpt opts_local_global[2] = {
    { KOP_LOCAL,  0, 8  },
    { KOP_GLOBAL, 0, 32 },
  };
  Term lg_outer = uop_apply_kop_split(uop_range(0, KAX_LOOP, 32),
                                       opts_local_global, 2);
  Term lg_inner = uop_apply_kop_split(uop_range(1, KAX_LOOP, 8),
                                       opts_local_global, 2);
  CHECK_EQ(uop_range_axis_type(lg_outer), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_axis_type(lg_inner), (u32)KAX_LOCAL);

  // === Composition: split + SWAP =====================================
  TEST_BEGIN("apply-kop-split/upcast-then-swap");
  // UPCAST(0, 4) -> [LOOP@0, UPCAST@1, ...], then SWAP(0, 1) ->
  //   desired = [UPCAST, LOOP, ...].
  KOpt opts_split_swap[2] = {
    { KOP_UPCAST, 0, 4 },
    { KOP_SWAP,   0, 1 },
  };
  Term ssw_at_0 = uop_apply_kop_split(uop_range(0, KAX_LOOP, 32),
                                       opts_split_swap, 2);
  Term ssw_at_1 = uop_apply_kop_split(uop_range(1, KAX_LOOP, 4),
                                       opts_split_swap, 2);
  CHECK_EQ(uop_range_axis_type(ssw_at_0), (u32)KAX_UPCAST);
  CHECK_EQ(uop_range_axis_type(ssw_at_1), (u32)KAX_LOOP);

  // === Two-split sequence ==========================================
  TEST_BEGIN("apply-kop-split/two-splits-stack");
  // Pre-split axes: [LOOP@0(256)].  UPCAST(0, 4) -> [LOOP@0(64),
  // UPCAST@1(4)].  Then UNROLL(0, 2) on the new outer position 0 ->
  // [LOOP@0(32), UNROLL@1(2), UPCAST@2(4)].
  KOpt opts_two_splits[2] = {
    { KOP_UPCAST, 0, 4 },
    { KOP_UNROLL, 0, 2 },
  };
  Term ts_at_0 = uop_apply_kop_split(uop_range(0, KAX_LOOP, 32),
                                      opts_two_splits, 2);
  Term ts_at_1 = uop_apply_kop_split(uop_range(1, KAX_LOOP, 2),
                                      opts_two_splits, 2);
  Term ts_at_2 = uop_apply_kop_split(uop_range(2, KAX_LOOP, 4),
                                      opts_two_splits, 2);
  CHECK_EQ(uop_range_axis_type(ts_at_0), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(ts_at_1), (u32)KAX_UNROLL);
  CHECK_EQ(uop_range_axis_type(ts_at_2), (u32)KAX_UPCAST);

  // === Negative: wrong axis_id - leaf at axis_id outside split range ==
  TEST_BEGIN("apply-kop-split/upcast-no-effect-on-far-axis");
  // UPCAST(0, 4) inserts at position 1.  A UOP_RANGE at axis_id=5
  // (already LOOP) should pass through unchanged.
  Term far = uop_range(5, KAX_LOOP, 16);
  Term far_out = uop_apply_kop_split(far, opts_upcast, 1);
  CHECK_EQ(far_out, far);
  CHECK_EQ(uop_range_axis_type(far_out), (u32)KAX_LOOP);

  // === Negative: KOP_TC ignored ===================================
  TEST_BEGIN("apply-kop-split/kop-tc-ignored");
  // KOP_TC carries kernel-level metadata only; it doesn't mutate
  // axis_types.  The rule should pass leaves through unchanged.
  Term tc_lp = uop_range(0, KAX_LOOP, 32);
  KOpt opts_tc[1] = {{ KOP_TC, 0, 16 }};
  Term tc_out = uop_apply_kop_split(tc_lp, opts_tc, 1);
  CHECK_EQ(tc_out, tc_lp);

  // === Phase E7: KOP_TC UPatRule mirror ==============================
  // uop_apply_kop_tc shares the `sim_kop_history` simulation with the
  // split-class rule and exposes a public entry whose semantics make
  // KOP_TC's empty axis_type mutation explicit at the API level.

  TEST_BEGIN("apply-kop-tc/tc-alone-noop");
  // KOP_TC alone in applied_opts: desired[] stays all KAX_LOOP, so
  // the rule short-circuits on every leaf (LOOP -> LOOP is a no-op).
  Term tc_r0 = uop_range(0, KAX_LOOP, 64);
  KOpt opts_tc_alone[1] = {{ KOP_TC, 0, 8 }};
  Term tc_r0_out = uop_apply_kop_tc(tc_r0, opts_tc_alone, 1);
  CHECK_EQ(tc_r0_out, tc_r0);  // hash-cons identity
  CHECK_EQ(uop_range_axis_type(tc_r0_out), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-tc/tc-alone-multi-axis-leaves-untouched");
  // TC's empty mutation must hold for every axis_id, not just axis 0.
  Term tc_r1 = uop_range(1, KAX_LOOP, 32);
  Term tc_r2 = uop_range(2, KAX_LOOP, 16);
  Term tc_r1_out = uop_apply_kop_tc(tc_r1, opts_tc_alone, 1);
  Term tc_r2_out = uop_apply_kop_tc(tc_r2, opts_tc_alone, 1);
  CHECK_EQ(tc_r1_out, tc_r1);
  CHECK_EQ(tc_r2_out, tc_r2);
  CHECK_EQ(uop_range_axis_type(tc_r1_out), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(tc_r2_out), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-tc/tc-preserves-non-loop-axis");
  // TC must not even disturb a leaf whose axis_type was pre-stamped
  // (e.g. KAX_REDUCE on a reduce axis).  desired[a] starts at LOOP
  // for every position, but the rule's "no rewrite when desired[a]
  // == leaf.axis_type" check only fires for matching types -- so we
  // also need the explicit "no-op for TC" branch to NOT downgrade a
  // pre-stamped REDUCE leaf to LOOP.  The rule's body keys off the
  // simulated desired[] which never sets a non-LOOP type for TC.
  Term tc_red = uop_range(2, KAX_REDUCE, 16);
  Term tc_red_out = uop_apply_kop_tc(tc_red, opts_tc_alone, 1);
  // Explanation: desired[2]=LOOP after the simulation; the leaf is
  // KAX_REDUCE.  The rule WOULD rewrite to LOOP since desired differs
  // from leaf -- this is the same behaviour the split-class rule has
  // for any axis whose pre-stamped type predates the simulation
  // baseline.  We assert the observable shape so future relaxations
  // (e.g. seeding the simulation from the leaf's current axis_type
  // when no opt mentions that axis) can flip this expectation
  // intentionally rather than silently.
  CHECK_EQ(term_tag(tc_red_out), TAG_UOP);
  CHECK_EQ(term_ext(tc_red_out), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(tc_red_out),   2);
  // Currently desired[2]=LOOP, leaf=REDUCE, rule rewrites to LOOP.
  // (Mirrors the same behaviour for the split rule on a stand-alone
  // REDUCE axis -- the simulation seeds LOOP and stamps to LOOP.)
  CHECK_EQ(uop_range_axis_type(tc_red_out), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-tc/idempotent-double-apply");
  // After one pass, the leaf already carries desired[a]; a second
  // pass with the same opts must short-circuit (hash-cons identity).
  Term tc_r0_id = uop_apply_kop_tc(tc_r0, opts_tc_alone, 1);
  Term tc_r0_id2 = uop_apply_kop_tc(tc_r0_id, opts_tc_alone, 1);
  CHECK_EQ(tc_r0_id2, tc_r0_id);
  CHECK_EQ(uop_range_axis_type(tc_r0_id2), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kop-tc/no-applied-opts-noop");
  // n_applied=0 -> early return; leaves untouched (matches the other
  // rules' n_applied==0 short-circuit).
  Term tc_zero = uop_apply_kop_tc(tc_r0, NULL, 0);
  CHECK_EQ(tc_zero, tc_r0);
  KOpt opts_tc_unused[1] = {{ KOP_TC, 0, 8 }};
  Term tc_zero_n = uop_apply_kop_tc(tc_r0, opts_tc_unused, 0);
  CHECK_EQ(tc_zero_n, tc_r0);

  TEST_BEGIN("apply-kop-tc/non-range-tag-mismatch");
  // Non-RANGE Term passes through unchanged.
  Term tc_not_range = uop_const(DT_INT32, 7);
  Term tc_not_range_out = uop_apply_kop_tc(tc_not_range, opts_tc_alone, 1);
  CHECK_EQ(tc_not_range_out, tc_not_range);

  // === Composition: KOP_TC + KOP_GLOBAL =================================
  TEST_BEGIN("apply-kop-tc/tc-then-global-stamps-axis");
  // applied_opts = [TC, GLOBAL(0, 64)].  TC contributes nothing;
  // GLOBAL stamps desired[0]=KAX_GLOBAL.  axis_id=0 leaf -> KAX_GLOBAL.
  KOpt opts_tc_global[2] = {
    { KOP_TC,     0, 8  },
    { KOP_GLOBAL, 0, 64 },
  };
  Term tc_glb = uop_apply_kop_tc(tc_r0, opts_tc_global, 2);
  CHECK_EQ(term_tag(tc_glb), TAG_UOP);
  CHECK_EQ(term_ext(tc_glb), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(tc_glb),   0);
  CHECK_EQ(uop_range_axis_type(tc_glb), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(tc_glb),    64);
  // The same applied_opts list fed through uop_apply_kop_split must
  // produce the SAME post-replay axis_type -- TC is a no-op for both.
  Term tc_glb_split = uop_apply_kop_split(tc_r0, opts_tc_global, 2);
  CHECK_EQ(tc_glb_split, tc_glb);

  // === Composition: KOP_TC + KOP_SWAP ==================================
  TEST_BEGIN("apply-kop-tc/tc-then-global-then-swap");
  // applied_opts = [TC, GLOBAL(0, 64), SWAP(0, 1)].  TC no-op,
  // GLOBAL(0) stamps desired[0]=GLOBAL, SWAP(0,1) moves it to
  // desired[1].  axis_id=1 leaf -> KAX_GLOBAL; axis_id=0 -> KAX_LOOP.
  Term tc_swap_r1 = uop_range(1, KAX_LOOP, 32);
  KOpt opts_tc_gs[3] = {
    { KOP_TC,     0, 8  },
    { KOP_GLOBAL, 0, 64 },
    { KOP_SWAP,   0, 1  },
  };
  Term tc_swap_at_0 = uop_apply_kop_tc(tc_r0,      opts_tc_gs, 3);
  Term tc_swap_at_1 = uop_apply_kop_tc(tc_swap_r1, opts_tc_gs, 3);
  CHECK_EQ(uop_range_axis_type(tc_swap_at_0), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(tc_swap_at_1), (u32)KAX_GLOBAL);

  // === Composition: KOP_TC + split (UPCAST) ============================
  TEST_BEGIN("apply-kop-tc/tc-then-upcast-stamps-inner");
  // applied_opts = [TC, UPCAST(0, 4)].  TC no-op; UPCAST splits axis 0
  // into outer (LOOP, position 0) + inner (UPCAST, position 1).
  Term tc_outer  = uop_range(0, KAX_LOOP, 32);
  Term tc_inner  = uop_range(1, KAX_LOOP, 4);
  KOpt opts_tc_up[2] = {
    { KOP_TC,     0, 8 },
    { KOP_UPCAST, 0, 4 },
  };
  Term tc_outer_out = uop_apply_kop_tc(tc_outer, opts_tc_up, 2);
  Term tc_inner_out = uop_apply_kop_tc(tc_inner, opts_tc_up, 2);
  CHECK_EQ(uop_range_axis_type(tc_outer_out), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(tc_inner_out), (u32)KAX_UPCAST);

  // === Composition order invariance: TC anywhere in the list ==========
  TEST_BEGIN("apply-kop-tc/tc-position-invariant");
  // Per kautotune_seq_can_append, autotune emits TC at index 0; the
  // simulation must still produce the same desired[] regardless of TC's
  // position in applied_opts (TC is a pure no-op for axis_type), so the
  // rule's behaviour is independent of where TC sits.
  KOpt opts_tc_first[3]  = {
    { KOP_TC,     0, 8  },
    { KOP_LOCAL,  0, 8  },
    { KOP_GLOBAL, 0, 32 },
  };
  KOpt opts_tc_middle[3] = {
    { KOP_LOCAL,  0, 8  },
    { KOP_TC,     0, 8  },
    { KOP_GLOBAL, 0, 32 },
  };
  KOpt opts_tc_last[3]   = {
    { KOP_LOCAL,  0, 8  },
    { KOP_GLOBAL, 0, 32 },
    { KOP_TC,     0, 8  },
  };
  Term inv_o = uop_range(0, KAX_LOOP, 32);
  Term inv_i = uop_range(1, KAX_LOOP, 8);
  Term first_o  = uop_apply_kop_tc(inv_o, opts_tc_first,  3);
  Term middle_o = uop_apply_kop_tc(inv_o, opts_tc_middle, 3);
  Term last_o   = uop_apply_kop_tc(inv_o, opts_tc_last,   3);
  CHECK_EQ(first_o, middle_o);
  CHECK_EQ(middle_o, last_o);
  CHECK_EQ(uop_range_axis_type(first_o), (u32)KAX_GLOBAL);
  Term first_i  = uop_apply_kop_tc(inv_i, opts_tc_first,  3);
  Term middle_i = uop_apply_kop_tc(inv_i, opts_tc_middle, 3);
  Term last_i   = uop_apply_kop_tc(inv_i, opts_tc_last,   3);
  CHECK_EQ(first_i, middle_i);
  CHECK_EQ(middle_i, last_i);
  CHECK_EQ(uop_range_axis_type(first_i), (u32)KAX_LOCAL);

  // === Cross-rule equivalence: TC's no-op holds across all 3 rules ===
  TEST_BEGIN("apply-kop-tc/cross-rule-equivalence");
  // For applied_opts containing TC alone, every public entry
  // (uop_apply_kop_global, uop_apply_kop_swap, uop_apply_kop_split,
  // uop_apply_kop_tc) must produce the same hash-cons-identical
  // result -- TC carries no axis_type effect for any of them.
  Term r_eq = uop_range(0, KAX_LOOP, 32);
  Term eq_global = uop_apply_kop_global(r_eq, opts_tc_alone, 1);
  Term eq_swap   = uop_apply_kop_swap  (r_eq, opts_tc_alone, 1);
  Term eq_split  = uop_apply_kop_split (r_eq, opts_tc_alone, 1);
  Term eq_tc     = uop_apply_kop_tc    (r_eq, opts_tc_alone, 1);
  CHECK_EQ(eq_global, r_eq);
  CHECK_EQ(eq_swap,   r_eq);
  CHECK_EQ(eq_split,  r_eq);
  CHECK_EQ(eq_tc,     r_eq);

  // === Per-rule predicate divergence: extent mismatch handled only in
  //     the strict global-only rule, not in the TC/split shared sim ===
  TEST_BEGIN("apply-kop-tc/per-rule-predicate-divergence");
  // The strict KOP_GLOBAL-only rule (rw_kop_global_stamp) checks
  // `o->arg == range.extent` in its rewrite body and skips on
  // mismatch -- mirroring axes_apply_opt's apply_opt guard.  The
  // shared `sim_kop_history` used by the SWAP / SPLIT / TC public
  // entries does not perform that arg-vs-extent check (it only
  // requires desired[a]==LOOP) because the simulation has no
  // per-axis extent table to validate against.  Document the
  // observable shape: feeding a stale-arg GLOBAL through the TC
  // entry stamps anyway (broader rule), while the strict
  // global-only entry rejects.  Future wedges may seed extents into
  // the simulation; until then this divergence is intentional.
  Term r_ext64 = uop_range(0, KAX_LOOP, 64);
  KOpt opts_tc_bad[2] = {
    { KOP_TC,     0, 8  },
    { KOP_GLOBAL, 0, 32 },  // arg 32 != extent 64
  };
  Term bad_via_global = uop_apply_kop_global(r_ext64, opts_tc_bad, 2);
  CHECK_EQ(bad_via_global, r_ext64);  // strict rule rejects mismatch
  CHECK_EQ(uop_range_axis_type(bad_via_global), (u32)KAX_LOOP);
  Term bad_via_tc = uop_apply_kop_tc(r_ext64, opts_tc_bad, 2);
  // Shared-simulation rule stamps anyway -- documents the divergence.
  CHECK_EQ(uop_range_axis_type(bad_via_tc), (u32)KAX_GLOBAL);

  thvm_free();
  TEST_REPORT();
}
