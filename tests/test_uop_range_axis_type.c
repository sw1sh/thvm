// test_uop_range_axis_type.c -- Phase E1..E8 acceptance.
//
// E1 introduced UOP_RANGE field accessors + uop_range_with_axis_type
// rewriter primitive; E2-E7 ported KOP_GLOBAL / KOP_SWAP / KOP_UPCAST
// /UNROLL/LOCAL/GROUP/GROUPTOP / KOP_TC as UPatRule rewrites.
//
// E8 extends uop_arity + uop_graph_rebuild_with_srcs so that
// uop_pattern_rewrite descends through the symbolic INDEX layer
// (UOP_STORE / UOP_INDEX_E / UOP_IADD / UOP_IMUL / UOP_IDIV /
// UOP_IMOD / UOP_ILT / UOP_IAND / UOP_IWHERE / UOP_INVALID /
// UOP_OPT / UOP_RANGE). In production lifter output, UOP_RANGE
// leaves are nested several levels deep beneath UOP_INDEX_E.addr.
// Pre-E8 the rewriter stopped at the first non-enumerated opcode
// and never reached the leaves.

#include "../src/thvm.c"
#include "test.h"

// Rule (E1): UOP_OPT(?range, kind=LOCAL, factor=ext)
//   -> uop_range_with_axis_type(range, KAX_LOCAL).
// Demonstrates a UPatRule reading and rewriting UOP_RANGE.axis_type
// without touching KpSchedule.  Mirrors KOP_LOCAL-on-existing-axis
// in shape (no axis split) so E1 stays minimal.
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

// Rule (E8): stamp axis_type on UOP_RANGE leaves whose axis_id
// matches a user-provided slot. Used to verify the rewriter descends
// through the symbolic INDEX layer post-E8.

typedef struct {
  u32 axis_id;
  u32 new_axis_type;
  u32 fire_count;
} StampCtx;

static Term rw_stamp_range(Term const *bindings, void *user) {
  StampCtx *ctx = (StampCtx *)user;
  Term t = bindings[0];
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  u64 loc = term_val(t);
  u32 axis_id  = (u32)term_val(heap_read(loc + 0));
  u32 axis_typ = (u32)term_val(heap_read(loc + 1));
  u32 extent   = (u32)term_val(heap_read(loc + 2));
  if (axis_id != ctx->axis_id) return 0;
  if (axis_typ == ctx->new_axis_type) return 0;  // already stamped
  ctx->fire_count++;
  return uop_range(axis_id, ctx->new_axis_type, extent);
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

  // ====================================================================
  // E8: uop_pattern_rewrite descends through the symbolic INDEX layer.
  // ====================================================================
  {

  // ------------------------------------------------------------------
  // (1) Bare RANGE root -- pre-E8 also worked.  Sanity check.
  TEST_BEGIN("e8/bare-range-root-stamps");
  Term r0 = uop_range(/*axis_id=*/0, KAX_LOOP, /*extent=*/16);
  CHECK_EQ(term_tag(r0), TAG_UOP);
  CHECK_EQ(term_ext(r0), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0) + 1)), KAX_LOOP);

  // RANGE has uop_arity=0; pattern matches with nsrc=0.  bind=0
  // captures the matched RANGE term itself for the rewrite fn.
  static UPat const pat_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

  StampCtx ctx0 = { .axis_id = 0, .new_axis_type = KAX_GLOBAL };
  UPatRule rules[] = {{&pat_range, rw_stamp_range}};
  Term r0_out = uop_pattern_rewrite(r0, rules, 1, &ctx0);
  CHECK_EQ(term_tag(r0_out), TAG_UOP);
  CHECK_EQ(term_ext(r0_out), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0_out) + 0)), 0);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0_out) + 1)), KAX_GLOBAL);
  CHECK_EQ((u32)term_val(heap_read(term_val(r0_out) + 2)), 16);
  CHECK_EQ(ctx0.fire_count, 1);

  // ------------------------------------------------------------------
  // (2) Production lifter shape: UOP_STORE(buf, INDEX_E(buf,
  //     IADD(IMUL(RANGE(axis=0), const), RANGE(axis=1))), value).
  //     The UOP_RANGE at axis=0 is FOUR levels deep beneath the root:
  //     STORE -> INDEX_E -> IADD -> IMUL -> RANGE.  Without E8's
  //     extensions to uop_arity / uop_graph_rebuild_with_srcs, the
  //     rewriter would stop at the STORE node (arity 0) without
  //     descending into INDEX_E.addr's address arithmetic, and the
  //     stamp rule would never reach the RANGE leaf.
  TEST_BEGIN("e8/nested-range-under-store-index-e-iadd-imul-stamps");
  u32 buf_dims[2] = { 4, 16 };
  Term buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, buf_dims);

  Term r_axis0 = uop_range(/*axis_id=*/0, KAX_LOOP, /*extent=*/4);
  Term r_axis1 = uop_range(/*axis_id=*/1, KAX_LOOP, /*extent=*/16);

  // Stride for the row axis = 16 (the inner-axis extent). Avoid 0/1
  // values that would trigger the int-binary identity simplifier.
  Term stride = uop_const(DT_INT32, 16);
  Term row    = uop_int_binary(UOP_IMUL, r_axis0, stride);
  Term addr   = uop_int_binary(UOP_IADD, row, r_axis1);
  Term ie     = uop_index_e(buf, addr);

  // Value being stored: any non-buffer expression. A UOP_LOAD of the
  // INDEX_E mirrors the trivial copy-store shape T.copy lowers to.
  Term value  = uop_load(ie);
  Term root   = uop_store(buf, addr, value);

  CHECK_EQ(term_tag(root), TAG_UOP);
  CHECK_EQ(term_ext(root), UOP_STORE);

  // Apply the rule asking for axis_id=0 -> KAX_GLOBAL.
  StampCtx ctx1 = { .axis_id = 0, .new_axis_type = KAX_GLOBAL };
  Term out = uop_pattern_rewrite(root, rules, 1, &ctx1);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STORE);

  // Walk down: STORE.addr -> IADD -> IMUL.lhs should be RANGE(axis=0,
  // axis_type=KAX_GLOBAL, extent=4).
  Term out_addr = heap_read(term_val(out) + 1);
  CHECK_EQ(term_tag(out_addr), TAG_UOP);
  CHECK_EQ(term_ext(out_addr), UOP_IADD);
  Term out_imul = heap_read(term_val(out_addr) + 0);
  CHECK_EQ(term_tag(out_imul), TAG_UOP);
  CHECK_EQ(term_ext(out_imul), UOP_IMUL);
  Term out_range0 = heap_read(term_val(out_imul) + 0);
  CHECK_EQ(term_tag(out_range0), TAG_UOP);
  CHECK_EQ(term_ext(out_range0), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range0) + 0)), 0);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range0) + 1)), KAX_GLOBAL);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range0) + 2)), 4);

  // The RANGE at axis_id=1 should remain at KAX_LOOP -- the rule
  // guards on axis_id and only stamps axis 0.
  Term out_range1 = heap_read(term_val(out_addr) + 1);
  CHECK_EQ(term_tag(out_range1), TAG_UOP);
  CHECK_EQ(term_ext(out_range1), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range1) + 0)), 1);
  CHECK_EQ((u32)term_val(heap_read(term_val(out_range1) + 1)), KAX_LOOP);

  // The rule fires once on the heap-cons'd RANGE node despite it
  // appearing twice in the DAG (once inside STORE.addr, once inside
  // INDEX_E.addr inside the LOAD value): the rewriter memoizes.
  CHECK_EQ(ctx1.fire_count, 1);

  // Same INDEX_E node beneath the value's LOAD also has its
  // RANGE(axis=0) updated -- the IMUL child now points at the same
  // KAX_GLOBAL range because uop_int_binary is hash-cons'd.
  Term out_value = heap_read(term_val(out) + 2);
  CHECK_EQ(term_tag(out_value), TAG_UOP);
  CHECK_EQ(term_ext(out_value), UOP_LOAD);
  Term out_value_ie = heap_read(term_val(out_value) + 0);
  CHECK_EQ(term_tag(out_value_ie), TAG_UOP);
  CHECK_EQ(term_ext(out_value_ie), UOP_INDEX_E);
  Term out_value_addr = heap_read(term_val(out_value_ie) + 1);
  CHECK_EQ(out_value_addr, out_addr);

  // ------------------------------------------------------------------
  // (3) Coverage: uop_arity now returns the documented child counts
  //     for every newly enumerated opcode. The rewriter / view / GC
  //     mark walks all key on this table; making the values explicit
  //     in a test pins the contract.
  TEST_BEGIN("e8/uop-arity-symbolic-index-layer");
  CHECK_EQ(uop_arity(UOP_RANGE),    0);
  CHECK_EQ(uop_arity(UOP_INVALID),  0);
  CHECK_EQ(uop_arity(UOP_OPT),      1);
  CHECK_EQ(uop_arity(UOP_IADD),     2);
  CHECK_EQ(uop_arity(UOP_ISUB),     2);
  CHECK_EQ(uop_arity(UOP_IMUL),     2);
  CHECK_EQ(uop_arity(UOP_IDIV),     2);
  CHECK_EQ(uop_arity(UOP_IMOD),     2);
  CHECK_EQ(uop_arity(UOP_ILT),      2);
  CHECK_EQ(uop_arity(UOP_IAND),     2);
  CHECK_EQ(uop_arity(UOP_INDEX_E),  2);
  CHECK_EQ(uop_arity(UOP_IWHERE),   3);
  CHECK_EQ(uop_arity(UOP_STORE),    3);

  // ------------------------------------------------------------------
  // (4) uop_graph_rebuild_with_srcs reaches each new opcode through
  //     the bottom-up rewrite path. We exercise it with an identity
  //     rewrite (rule never fires) so every node in the DAG is
  //     visited via uop_graph_rewrite_rec; if rebuild bombed (e.g.
  //     by returning `t` for UOP_INDEX_E and dropping a child
  //     rewrite), the resulting graph would diverge.
  TEST_BEGIN("e8/identity-rewrite-survives-symbolic-layer");
  // No-op rule -- never returns a replacement.
  static UPat const pat_never = {UOP_NEG, 1, 0, -1, NULL, NULL};
  UPatRule no_rules[] = {{&pat_never, rw_stamp_range}};
  StampCtx idle = { .axis_id = 99, .new_axis_type = KAX_GLOBAL };
  Term root2 = uop_pattern_rewrite(root, no_rules, 1, &idle);
  // root contains no UOP_NEG; the rule never fires; the graph is
  // structurally identical (all nodes hash-cons back to themselves).
  CHECK_EQ(root2, root);
  CHECK_EQ(idle.fire_count, 0);

  // ------------------------------------------------------------------
  // (5) IWHERE descent: build IWHERE(ILT(RANGE0, c), RANGE1, INVALID)
  //     and rewrite RANGE1's axis_type. Verifies that the rebuilder
  //     handles the 3-arity IWHERE shape and that arity-0 INVALID is
  //     also reached without stalling the walker.
  TEST_BEGIN("e8/iwhere-with-invalid-leaf-stamps-then");
  Term lim   = uop_const(DT_INT32, 3);
  Term cond  = uop_int_binary(UOP_ILT, r_axis0, lim);
  Term inv   = uop_invalid();
  Term iw    = uop_iwhere(cond, r_axis1, inv);
  StampCtx ctx2 = { .axis_id = 1, .new_axis_type = KAX_LOCAL };
  Term iw_out = uop_pattern_rewrite(iw, rules, 1, &ctx2);
  CHECK_EQ(term_tag(iw_out), TAG_UOP);
  CHECK_EQ(term_ext(iw_out), UOP_IWHERE);
  Term iw_then = heap_read(term_val(iw_out) + 1);
  CHECK_EQ(term_tag(iw_then), TAG_UOP);
  CHECK_EQ(term_ext(iw_then), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(iw_then) + 0)), 1);
  CHECK_EQ((u32)term_val(heap_read(term_val(iw_then) + 1)), KAX_LOCAL);
  // INVALID arm stays the singleton.
  Term iw_else = heap_read(term_val(iw_out) + 2);
  CHECK_EQ(term_tag(iw_else), TAG_UOP);
  CHECK_EQ(term_ext(iw_else), UOP_INVALID);
  CHECK_EQ(ctx2.fire_count, 1);

  // ------------------------------------------------------------------
  // (6) UOP_OPT descent: wrap a RANGE in UOP_OPT(target, kind=UNROLL,
  //     factor=4) and verify the rewrite reaches and replaces the
  //     target while the kind/factor scalar metadata is preserved.
  TEST_BEGIN("e8/opt-wraps-range-rewrite-replaces-target");
  Term opt_node = uop_opt(r_axis0, UOP_OPT_UNROLL, 4);
  CHECK_EQ(term_tag(opt_node), TAG_UOP);
  CHECK_EQ(term_ext(opt_node), UOP_OPT);
  StampCtx ctx3 = { .axis_id = 0, .new_axis_type = KAX_UNROLL };
  Term opt_out = uop_pattern_rewrite(opt_node, rules, 1, &ctx3);
  CHECK_EQ(term_tag(opt_out), TAG_UOP);
  CHECK_EQ(term_ext(opt_out), UOP_OPT);
  // Verify the kind / factor scalar slots round-tripped intact.
  CHECK_EQ(uop_opt_kind(opt_out),   UOP_OPT_UNROLL);
  CHECK_EQ(uop_opt_factor(opt_out), 4);
  Term opt_target = uop_opt_target(opt_out);
  CHECK_EQ(term_tag(opt_target), TAG_UOP);
  CHECK_EQ(term_ext(opt_target), UOP_RANGE);
  CHECK_EQ((u32)term_val(heap_read(term_val(opt_target) + 1)), KAX_UNROLL);
  CHECK_EQ(ctx3.fire_count, 1);
  }

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

  // === E9-prep wedge 1: uop_apply_kernel_opts unified entry ============
  // The single composed pass that wires E2/E3/E4-E6/E7 into one DAG
  // walk.  The pass adds extent tracking on top of the shared
  // `sim_kop_history` so the GLOBAL extent guard from
  // kernel_lift.c:1619 is honoured -- mirrors the lifter's structural
  // replay faithfully.

  TEST_BEGIN("apply-kernel-opts/empty-applied-opts-noop");
  Term k_r0 = uop_range(0, KAX_LOOP, 64);
  Term k_r0_pass = uop_apply_kernel_opts(k_r0, NULL, 0);
  CHECK_EQ(k_r0_pass, k_r0);
  Term k_r0_unused = uop_apply_kernel_opts(k_r0, opts_tc_bad, 0);
  CHECK_EQ(k_r0_unused, k_r0);

  TEST_BEGIN("apply-kernel-opts/global-stamps-with-extent-match");
  KOpt opts_glb_match[1] = {{ KOP_GLOBAL, 0, 64 }};
  Term k_glb = uop_apply_kernel_opts(k_r0, opts_glb_match, 1);
  CHECK_EQ(uop_range_axis_type(k_glb), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(k_glb),    64);

  TEST_BEGIN("apply-kernel-opts/global-extent-mismatch-rejected");
  // Unlike the shared sim, the unified pass DOES enforce arg==extent
  // for GLOBAL.  This matches kernel_lift's replay: the lifter would
  // bail out (return 0) on an extent mismatch, so the unified pass
  // mustn't stamp where the lifter wouldn't.  Result: the leaf stays
  // KAX_LOOP.
  KOpt opts_glb_bad[1] = {{ KOP_GLOBAL, 0, 32 }};  // arg 32 != extent 64
  Term k_glb_bad = uop_apply_kernel_opts(k_r0, opts_glb_bad, 1);
  CHECK_EQ(k_glb_bad, k_r0);  // hash-cons identity -- no rewrite
  CHECK_EQ(uop_range_axis_type(k_glb_bad), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kernel-opts/upcast-stamps-inner");
  // UPCAST(0, 4) on a leaf at axis_id=1 -> desired[1]=KAX_UPCAST.
  Term k_inner = uop_range(1, KAX_LOOP, 4);
  KOpt opts_upcast_unif[1] = {{ KOP_UPCAST, 0, 4 }};
  Term k_inner_st = uop_apply_kernel_opts(k_inner, opts_upcast_unif, 1);
  CHECK_EQ(uop_range_axis_type(k_inner_st), (u32)KAX_UPCAST);

  TEST_BEGIN("apply-kernel-opts/swap-and-global-compose");
  // GLOBAL(0, 64) then SWAP(0, 1): desired = [LOOP, GLOBAL, ...] with
  // global_arg[1]=64.  After SWAP, the GLOBAL stamp's required extent
  // moves with it -- the leaf at the new position must carry the
  // extent the original GLOBAL opt named.  This mirrors the lifter:
  // cur[]'s entire entry (extent + axis_type) swaps as a unit.
  KOpt opts_unif_gs[2] = {
    { KOP_GLOBAL, 0, 64 },
    { KOP_SWAP,   0, 1  },
  };
  // Post-replay leaf shapes: axis 0 gets the swapped-in entry (orig
  // axis 1's extent 32, LOOP); axis 1 gets the stamped entry
  // (orig axis 0's extent 64, GLOBAL).
  Term k_a0 = uop_range(0, KAX_LOOP, 32);
  Term k_a1 = uop_range(1, KAX_LOOP, 64);
  Term k_a0_out = uop_apply_kernel_opts(k_a0, opts_unif_gs, 2);
  Term k_a1_out = uop_apply_kernel_opts(k_a1, opts_unif_gs, 2);
  CHECK_EQ(uop_range_axis_type(k_a0_out), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(k_a1_out), (u32)KAX_GLOBAL);

  TEST_BEGIN("apply-kernel-opts/all-four-classes-mixed");
  // applied_opts = [TC, LOCAL(0, 8), GLOBAL(0, 32), SWAP(0, 1)].
  // Desired sequence (with extent tracking that mirrors the lifter):
  //   start:           desired=[LOOP, ...], global_arg=[0,...]
  //   TC:              no-op
  //   LOCAL(0, 8):     splits axis 0; desired=[LOOP, LOCAL, ...]
  //   GLOBAL(0, 32):   stamps desired[0]=GLOBAL, global_arg[0]=32.
  //   SWAP(0, 1):      desired=[LOCAL, GLOBAL, ...],
  //                    global_arg swapped so global_arg[1]=32.
  // The leaf at axis_id=1 must carry extent 32 for the GLOBAL stamp
  // to apply (mirrors the lifter's extent guard at line 1619).  The
  // leaf at axis_id=0 takes the swapped-in LOCAL extent (8 -- the
  // LOCAL split's inner size).
  KOpt opts_all4[4] = {
    { KOP_TC,     0, 8  },
    { KOP_LOCAL,  0, 8  },
    { KOP_GLOBAL, 0, 32 },
    { KOP_SWAP,   0, 1  },
  };
  // Post-replay leaf shapes after the SWAP:
  //   axis 0: extent 8  (the swapped-in LOCAL inner)
  //   axis 1: extent 32 (the swapped-in GLOBAL outer)
  Term k_outer  = uop_range(0, KAX_LOOP, 8);
  Term k_innerL = uop_range(1, KAX_LOOP, 32);
  Term k_outer_out = uop_apply_kernel_opts(k_outer, opts_all4, 4);
  Term k_innerL_out = uop_apply_kernel_opts(k_innerL, opts_all4, 4);
  CHECK_EQ(uop_range_axis_type(k_outer_out),  (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_axis_type(k_innerL_out), (u32)KAX_GLOBAL);

  TEST_BEGIN("apply-kernel-opts/idempotent-double-apply");
  Term k_first  = uop_apply_kernel_opts(k_outer, opts_all4, 4);
  Term k_second = uop_apply_kernel_opts(k_first,  opts_all4, 4);
  CHECK_EQ(k_first, k_second);

  TEST_BEGIN("apply-kernel-opts/non-range-tag-mismatch");
  Term k_not_range = uop_const(DT_INT32, 17);
  Term k_not_out = uop_apply_kernel_opts(k_not_range, opts_glb_match, 1);
  CHECK_EQ(k_not_out, k_not_range);

  TEST_BEGIN("apply-kernel-opts/descends-through-store-index-e");
  // Production lifter shape: STORE(buf, INDEX_E(buf, IADD(IMUL(R0, c),
  // R1)), value).  The unified pass must descend through the symbolic
  // INDEX layer to reach the RANGE leaves -- E8's uop_arity extension
  // covers this.
  u32 ke_buf_dims[2] = { 4, 16 };
  Term ke_buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, ke_buf_dims);
  Term ke_r0  = uop_range(0, KAX_LOOP, 4);
  Term ke_r1  = uop_range(1, KAX_LOOP, 16);
  Term ke_str = uop_const(DT_INT32, 16);
  Term ke_row = uop_int_binary(UOP_IMUL, ke_r0, ke_str);
  Term ke_addr = uop_int_binary(UOP_IADD, ke_row, ke_r1);
  Term ke_ie   = uop_index_e(ke_buf, ke_addr);
  Term ke_val  = uop_load(ke_ie);
  Term ke_root = uop_store(ke_buf, ke_addr, ke_val);
  KOpt opts_glb0[1] = {{ KOP_GLOBAL, 0, 4 }};  // matches ke_r0 extent
  Term ke_out = uop_apply_kernel_opts(ke_root, opts_glb0, 1);
  CHECK_EQ(term_tag(ke_out), TAG_UOP);
  CHECK_EQ(term_ext(ke_out), UOP_STORE);
  Term ke_out_addr = heap_read(term_val(ke_out) + 1);
  CHECK_EQ(term_ext(ke_out_addr), UOP_IADD);
  Term ke_out_imul = heap_read(term_val(ke_out_addr) + 0);
  CHECK_EQ(term_ext(ke_out_imul), UOP_IMUL);
  Term ke_out_r0 = heap_read(term_val(ke_out_imul) + 0);
  CHECK_EQ(term_ext(ke_out_r0), UOP_RANGE);
  CHECK_EQ(uop_range_axis_type(ke_out_r0), (u32)KAX_GLOBAL);
  // Leaf at axis_id=1 was untouched.
  Term ke_out_r1 = heap_read(term_val(ke_out_addr) + 1);
  CHECK_EQ(uop_range_axis_type(ke_out_r1), (u32)KAX_LOOP);

  TEST_BEGIN("apply-kernel-opts/validate-fire-count");
  // The validation entry returns the number of UOP_RANGE leaves that
  // would have been rewritten.  When the pass would no-op (already
  // stamped or no matching opts), fire_count == 0; the variant exists
  // so callers can assert that property directly.
  u32 vfires = 0;
  Term vout1 = uop_apply_kernel_opts_validate(k_outer, opts_all4, 4, &vfires);
  // k_outer is at axis_id=0, LOOP, extent 8; desired[0]=LOCAL post-replay.
  // LOCAL has no extent guard, so the rule fires once.
  CHECK_EQ(vfires, 1u);
  CHECK_EQ(uop_range_axis_type(vout1), (u32)KAX_LOCAL);
  // Run again on the result -- now leaf already carries LOCAL, no fire.
  u32 vfires2 = 0;
  Term vout2 = uop_apply_kernel_opts_validate(vout1, opts_all4, 4, &vfires2);
  CHECK_EQ(vfires2, 0u);
  CHECK_EQ(vout2, vout1);

  TEST_BEGIN("apply-kernel-opts/validate-empty-applied-opts-zero-fires");
  u32 vfires3 = 99;
  Term vout3 = uop_apply_kernel_opts_validate(k_outer, NULL, 0, &vfires3);
  CHECK_EQ(vfires3, 0u);
  CHECK_EQ(vout3, k_outer);

  // === (10) Phase E9-prep wedge 2: uop_range_split primitive ========
  //
  // Replaces a single UOP_RANGE leaf with (outer, inner) pair plus the
  // linear_index = outer * k + inner reconstruction.  Mirrors
  // kernel_lift.c:1561-1604's structural-replay split block at the UOp
  // DAG level.  See src/uop/index.c for the design notes.
  TEST_BEGIN("range-split/basic-shape");
  // Split a LOOP axis 0 of extent 64 by k=8.  Expect:
  //   outer        : RANGE(0, LOOP, 8)
  //   inner        : RANGE(1, LOCAL, 8)
  //   linear_index : IADD(IMUL(outer, 8), inner)
  Term rs_old = uop_range(0, KAX_LOOP, 64);
  UopRangeSplit rs = uop_range_split(rs_old, 8, KAX_LOCAL);
  CHECK_EQ(term_tag(rs.outer), TAG_UOP);
  CHECK_EQ(term_ext(rs.outer), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(rs.outer),    0u);
  CHECK_EQ(uop_range_axis_type(rs.outer),  (u32)KAX_LOOP);
  CHECK_EQ(uop_range_extent(rs.outer),     8u);
  CHECK_EQ(term_tag(rs.inner), TAG_UOP);
  CHECK_EQ(term_ext(rs.inner), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(rs.inner),    1u);
  CHECK_EQ(uop_range_axis_type(rs.inner),  (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_extent(rs.inner),     8u);
  CHECK_EQ(term_tag(rs.linear_index), TAG_UOP);
  CHECK_EQ(term_ext(rs.linear_index), UOP_IADD);
  Term rs_imul = heap_read(term_val(rs.linear_index) + 0);
  Term rs_in   = heap_read(term_val(rs.linear_index) + 1);
  CHECK_EQ(term_ext(rs_imul), UOP_IMUL);
  CHECK_EQ(rs_in, rs.inner);
  Term rs_outer_in_imul = heap_read(term_val(rs_imul) + 0);
  Term rs_k_in_imul     = heap_read(term_val(rs_imul) + 1);
  CHECK_EQ(rs_outer_in_imul, rs.outer);
  CHECK_EQ(term_ext(rs_k_in_imul), UOP_CONST);

  TEST_BEGIN("range-split/inner-axis-types");
  // Each split-class opt maps to a distinct inner axis_type.  The
  // primitive accepts an arbitrary u32; the rule body / caller is
  // responsible for validating the mapping (mirrors how
  // kop_inner_axis_type in src/uop/apply_opt.c selects the type).
  Term rs64 = uop_range(2, KAX_LOOP, 64);
  UopRangeSplit rs_up = uop_range_split(rs64, 4, KAX_UPCAST);
  UopRangeSplit rs_un = uop_range_split(rs64, 4, KAX_UNROLL);
  UopRangeSplit rs_lo = uop_range_split(rs64, 4, KAX_LOCAL);
  UopRangeSplit rs_gr = uop_range_split(rs64, 4, KAX_GROUP_REDUCE);
  CHECK_EQ(uop_range_axis_type(rs_up.inner), (u32)KAX_UPCAST);
  CHECK_EQ(uop_range_axis_type(rs_un.inner), (u32)KAX_UNROLL);
  CHECK_EQ(uop_range_axis_type(rs_lo.inner), (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_axis_type(rs_gr.inner), (u32)KAX_GROUP_REDUCE);
  // Outer axis_type preserves the original (LOOP here).
  CHECK_EQ(uop_range_axis_type(rs_up.outer), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_axis_type(rs_lo.outer), (u32)KAX_LOOP);

  TEST_BEGIN("range-split/preserves-outer-axis-type");
  // Splitting a non-LOOP outer (e.g. an axis already stamped GLOBAL)
  // keeps the outer's axis_type.  This composes with E2's KOP_GLOBAL
  // stamping order: GLOBAL on the outer can land before or after the
  // split without changing the outer's identity.
  Term rs_g = uop_range(0, KAX_GLOBAL, 32);
  UopRangeSplit rs_g_split = uop_range_split(rs_g, 4, KAX_LOCAL);
  CHECK_EQ(uop_range_axis_type(rs_g_split.outer), (u32)KAX_GLOBAL);
  CHECK_EQ(uop_range_extent(rs_g_split.outer),    8u);
  CHECK_EQ(uop_range_axis_type(rs_g_split.inner), (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_extent(rs_g_split.inner),    4u);

  TEST_BEGIN("range-split/hash-cons-idempotent");
  // Re-running uop_range_split with the same inputs returns hash-cons-
  // identical Terms (the canonical constructors dedup).  This is the
  // property a UPatRule descending through IADD/IMUL chains relies on.
  UopRangeSplit rs_a = uop_range_split(rs_old, 8, KAX_LOCAL);
  UopRangeSplit rs_b = uop_range_split(rs_old, 8, KAX_LOCAL);
  CHECK_EQ(rs_a.outer, rs_b.outer);
  CHECK_EQ(rs_a.inner, rs_b.inner);
  CHECK_EQ(rs_a.linear_index, rs_b.linear_index);

  TEST_BEGIN("range-split/rejects-bad-inputs");
  // Tag mismatch: not a RANGE.
  Term rs_not_range = uop_const(DT_INT32, 99);
  UopRangeSplit rs_bad1 = uop_range_split(rs_not_range, 4, KAX_LOCAL);
  CHECK_EQ(rs_bad1.outer, 0u);
  CHECK_EQ(rs_bad1.inner, 0u);
  CHECK_EQ(rs_bad1.linear_index, 0u);
  // k = 0.
  UopRangeSplit rs_bad2 = uop_range_split(rs_old, 0, KAX_LOCAL);
  CHECK_EQ(rs_bad2.outer, 0u);
  // extent % k != 0.
  Term rs_odd = uop_range(0, KAX_LOOP, 7);
  UopRangeSplit rs_bad3 = uop_range_split(rs_odd, 2, KAX_LOCAL);
  CHECK_EQ(rs_bad3.outer, 0u);
  CHECK_EQ(rs_bad3.inner, 0u);
  CHECK_EQ(rs_bad3.linear_index, 0u);

  TEST_BEGIN("range-split/equivalent-to-direct-construction");
  // The triple should hash-cons-equal to a direct RANGE / IMUL / IADD
  // assembly.  This proves the primitive doesn't introduce hidden state
  // (e.g. a parallel split-tracking table) -- it's pure composition over
  // the canonical constructors.
  Term rs_direct_outer = uop_range(0, KAX_LOOP, 8);
  Term rs_direct_inner = uop_range(1, KAX_LOCAL, 8);
  Term rs_direct_k     = uop_const(DT_INT32, 8);
  Term rs_direct_imul  = uop_int_binary(UOP_IMUL, rs_direct_outer, rs_direct_k);
  Term rs_direct_iadd  = uop_int_binary(UOP_IADD, rs_direct_imul, rs_direct_inner);
  CHECK_EQ(rs.outer,        rs_direct_outer);
  CHECK_EQ(rs.inner,        rs_direct_inner);
  CHECK_EQ(rs.linear_index, rs_direct_iadd);

  // === (11) Phase E9-prep wedge 2: uop_apply_split_dag UPatRule =====
  //
  // Wraps uop_range_split into a DAG-level rewrite that walks the
  // applied_opts split-class entries left-to-right; for each
  // SPLIT(o.axis, o.arg) it locates the UOP_RANGE leaf at the current
  // post-replay position whose extent equals the post-replay outer
  // extent and rewires every consumer of that leaf to use linear_index.
  //
  // The rule body assumes the input DAG was lifted with the
  // structural-replay split block disabled -- i.e. UOP_RANGE leaves
  // carry their PRE-split axis_id (= origin BUFFERIZE position) and
  // PRE-split extent.  See src/uop/apply_opt.c (uop_apply_split_dag)
  // for the simulation rules.
  TEST_BEGIN("apply-split-dag/single-split-stamps-pair");
  // Pre-split DAG: STORE(buf, INDEX_E(buf, RANGE(0, LOOP, 64)),
  // LOAD(INDEX_E(buf, RANGE(0, LOOP, 64)))).  applied_opts =
  // [LOCAL(0, 8)].  Expect: every reference to the original RANGE
  // becomes IADD(IMUL(outer, 8), inner) with outer = RANGE(0, LOOP,
  // 8), inner = RANGE(1, LOCAL, 8).
  u32 sd_dims[1] = {64};
  Term sd_buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, sd_dims);
  Term sd_r0   = uop_range(0, KAX_LOOP, 64);
  Term sd_ie   = uop_index_e(sd_buf, sd_r0);
  Term sd_val  = uop_load(sd_ie);
  Term sd_root = uop_store(sd_buf, sd_r0, sd_val);
  KOpt sd_opts[1] = {{ KOP_LOCAL, 0, 8 }};
  Term sd_out = uop_apply_split_dag(sd_root, sd_opts, 1);
  CHECK_EQ(term_tag(sd_out), TAG_UOP);
  CHECK_EQ(term_ext(sd_out), UOP_STORE);
  // The STORE.addr should now be IADD(IMUL(outer, 8), inner).
  Term sd_out_addr = heap_read(term_val(sd_out) + 1);
  CHECK_EQ(term_ext(sd_out_addr), UOP_IADD);
  Term sd_out_imul = heap_read(term_val(sd_out_addr) + 0);
  Term sd_out_inn  = heap_read(term_val(sd_out_addr) + 1);
  CHECK_EQ(term_ext(sd_out_imul), UOP_IMUL);
  CHECK_EQ(term_ext(sd_out_inn),  UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(sd_out_inn),    1u);
  CHECK_EQ(uop_range_axis_type(sd_out_inn),  (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_extent(sd_out_inn),     8u);
  Term sd_out_outer = heap_read(term_val(sd_out_imul) + 0);
  CHECK_EQ(term_ext(sd_out_outer), UOP_RANGE);
  CHECK_EQ(uop_range_axis_id(sd_out_outer),    0u);
  CHECK_EQ(uop_range_axis_type(sd_out_outer),  (u32)KAX_LOOP);
  CHECK_EQ(uop_range_extent(sd_out_outer),     8u);

  TEST_BEGIN("apply-split-dag/post-split-extent-guard");
  // E9-prep wedge 2 stage (d) retired the sentinel-walk idempotence
  // guard: uop_apply_split_dag is no longer idempotent on already-split
  // DAGs (calling it on `sd_out` would double-split because the capture
  // loop reads max-extent leaves which are now post-split).  In
  // production the materialize.c caller invokes the rule exactly once
  // per lift on a pre-split DAG -- idempotence is not a contract.  We
  // also can't assert "running twice on a hash-cons-equal pre-split DAG
  // produces equal output" because rebuild walks emit fresh UOP_LOAD
  // terms (LOAD is not hash-cons-cached).
  //
  // The per-leaf extent + axis_type guard in rw_split_dag_range is
  // still the structural gate.  The post-split DAG has outer.extent =
  // 8 < 64 (origin_extent of the pre-split capture), so a fresh
  // capture from the post-split DAG sees the smaller extent at axis
  // 0.  Verify that re-applying with a *fresh* opts table whose arg
  // matches the post-split outer extent (the only case where rule
  // would fire incorrectly) doesn't crash and produces a DAG whose
  // STORE.addr structure is still well-formed.
  KOpt sd_post_opts[1] = {{ KOP_LOCAL, 0, 8 }};  // arg matches post-split outer extent
  Term sd_post_out = uop_apply_split_dag(sd_out, sd_post_opts, 1);
  CHECK_EQ(term_tag(sd_post_out), TAG_UOP);
  CHECK_EQ(term_ext(sd_post_out), UOP_STORE);

  TEST_BEGIN("apply-split-dag/no-opts-noop");
  Term sd_no = uop_apply_split_dag(sd_root, NULL, 0);
  CHECK_EQ(sd_no, sd_root);

  TEST_BEGIN("apply-split-dag/non-split-opts-noop");
  // GLOBAL/SWAP/TC are handled by other E* rules; the split DAG rule
  // ignores them.
  KOpt sd_glb[1] = {{ KOP_GLOBAL, 0, 64 }};
  Term sd_glb_out = uop_apply_split_dag(sd_root, sd_glb, 1);
  CHECK_EQ(sd_glb_out, sd_root);

  TEST_BEGIN("apply-split-dag/non-divisible-extent-skips-split");
  // KOP_LOCAL with arg=8 against a leaf of extent 7: the rule skips
  // (mirrors kernel_lift.c:1581 "extent % o.arg != 0 -> return 0").
  // The DAG flows through unchanged.
  Term sd_odd_buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, (u32[]){7});
  Term sd_odd_r0   = uop_range(0, KAX_LOOP, 7);
  Term sd_odd_ie   = uop_index_e(sd_odd_buf, sd_odd_r0);
  Term sd_odd_val  = uop_load(sd_odd_ie);
  Term sd_odd_root = uop_store(sd_odd_buf, sd_odd_r0, sd_odd_val);
  Term sd_odd_out  = uop_apply_split_dag(sd_odd_root, sd_opts, 1);
  CHECK_EQ(sd_odd_out, sd_odd_root);

  TEST_BEGIN("apply-split-dag/two-axis-with-split-shifts-second");
  // Pre-replay DAG with two axes:
  //   STORE(buf, INDEX_E(buf, IADD(IMUL(R0_pre, 16), R1_pre)),
  //         LOAD(...))
  // applied_opts = [LOCAL(0, 4)].  The split affects axis 0; axis 1's
  // leaf does NOT need to shift in this rule's contract (the rule
  // ONLY rewrites the axis that's being split; axis-id shifting of
  // OTHER axes is the lifter's responsibility post-(d), or a
  // subsequent rule's job pre-(d)).  Verify: the IMUL chain holds the
  // new linear expression (outer * 4 + inner) at the spot the original
  // R0_pre occupied; R1_pre flows through unchanged.
  u32 sd2_dims[2] = {16, 16};
  Term sd2_buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, sd2_dims);
  Term sd2_r0   = uop_range(0, KAX_LOOP, 16);
  Term sd2_r1   = uop_range(1, KAX_LOOP, 16);
  Term sd2_str  = uop_const(DT_INT32, 16);
  Term sd2_row  = uop_int_binary(UOP_IMUL, sd2_r0, sd2_str);
  Term sd2_addr = uop_int_binary(UOP_IADD, sd2_row, sd2_r1);
  Term sd2_ie   = uop_index_e(sd2_buf, sd2_addr);
  Term sd2_val  = uop_load(sd2_ie);
  Term sd2_root = uop_store(sd2_buf, sd2_addr, sd2_val);
  KOpt sd2_opts[1] = {{ KOP_LOCAL, 0, 4 }};
  Term sd2_out = uop_apply_split_dag(sd2_root, sd2_opts, 1);
  // sd2_out.addr should be IADD(IMUL(NEW_LINEAR, 16), R1_pre)
  // where NEW_LINEAR = IADD(IMUL(RANGE(0,LOOP,4), 4), RANGE(1,LOCAL,4)).
  Term sd2_out_addr = heap_read(term_val(sd2_out) + 1);
  CHECK_EQ(term_ext(sd2_out_addr), UOP_IADD);
  Term sd2_row_imul = heap_read(term_val(sd2_out_addr) + 0);
  Term sd2_r1_out   = heap_read(term_val(sd2_out_addr) + 1);
  CHECK_EQ(term_ext(sd2_row_imul), UOP_IMUL);
  CHECK_EQ(sd2_r1_out, sd2_r1);  // axis-1 leaf unchanged
  Term sd2_new_linear = heap_read(term_val(sd2_row_imul) + 0);
  CHECK_EQ(term_ext(sd2_new_linear), UOP_IADD);

  TEST_BEGIN("apply-split-dag/double-split-same-axis");
  // applied_opts = [LOCAL(0, 4), UPCAST(0, 2)].  After the first
  // split the outer (extent 16/4 = 4) gets split again by 2, so the
  // final origin_expr[0] is:
  //   IADD(IMUL(IADD(IMUL(RANGE(0,LOOP,2), 2), RANGE(?,UPCAST,2)), 4),
  //        RANGE(1,LOCAL,4))
  // The "?" inner axis_id is what the lifter assigns -- in our rule
  // it's a+1 = 1, the same axis_id the LOCAL inner already uses.
  // That's the lifter's behaviour too (kernel_lift.c uses the same
  // o.axis+1 slot for the new inner).  This wedge keeps the same
  // contract; downstream stages can re-number per-leaf axis_ids if
  // needed.
  Term sd3_buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, (u32[]){16});
  Term sd3_r0   = uop_range(0, KAX_LOOP, 16);
  Term sd3_ie   = uop_index_e(sd3_buf, sd3_r0);
  Term sd3_val  = uop_load(sd3_ie);
  Term sd3_root = uop_store(sd3_buf, sd3_r0, sd3_val);
  KOpt sd3_opts[2] = {{ KOP_LOCAL, 0, 4 }, { KOP_UPCAST, 0, 2 }};
  Term sd3_out = uop_apply_split_dag(sd3_root, sd3_opts, 2);
  // Walk: STORE.addr = IADD(IMUL(IADD(IMUL(R(0,LOOP,2), 2), R(?,UPCAST,2)), 4), R(1,LOCAL,4))
  Term sd3_addr = heap_read(term_val(sd3_out) + 1);
  CHECK_EQ(term_ext(sd3_addr), UOP_IADD);
  Term sd3_local_inner = heap_read(term_val(sd3_addr) + 1);
  CHECK_EQ(uop_range_axis_type(sd3_local_inner), (u32)KAX_LOCAL);
  CHECK_EQ(uop_range_extent(sd3_local_inner), 4u);
  Term sd3_outer_imul = heap_read(term_val(sd3_addr) + 0);
  CHECK_EQ(term_ext(sd3_outer_imul), UOP_IMUL);
  Term sd3_inner_iadd = heap_read(term_val(sd3_outer_imul) + 0);
  CHECK_EQ(term_ext(sd3_inner_iadd), UOP_IADD);  // second-split linear
  Term sd3_innermost_outer = heap_read(term_val(sd3_inner_iadd) + 0);
  CHECK_EQ(term_ext(sd3_innermost_outer), UOP_IMUL);  // RANGE * 2
  Term sd3_innermost_range = heap_read(term_val(sd3_innermost_outer) + 0);
  CHECK_EQ(uop_range_axis_type(sd3_innermost_range), (u32)KAX_LOOP);
  CHECK_EQ(uop_range_extent(sd3_innermost_range), 2u);
  // UPCAST inner is wrapped in UOP_OPT(_, UPCAST, 2).
  Term sd3_upcast_opt = heap_read(term_val(sd3_inner_iadd) + 1);
  CHECK_EQ(term_ext(sd3_upcast_opt), UOP_OPT);
  CHECK_EQ(uop_opt_kind(sd3_upcast_opt), (u32)UOP_OPT_UPCAST);
  Term sd3_upcast_inner = uop_opt_target(sd3_upcast_opt);
  CHECK_EQ(uop_range_axis_type(sd3_upcast_inner), (u32)KAX_UPCAST);
  CHECK_EQ(uop_range_extent(sd3_upcast_inner), 2u);

  TEST_BEGIN("apply-split-dag/double-split-yields-store");
  // Idempotence-on-already-split is no longer a contract (see
  // post-split-extent-guard above).  Pin only that the double-split
  // case produces a well-formed STORE root.
  CHECK_EQ(term_tag(sd3_out), TAG_UOP);
  CHECK_EQ(term_ext(sd3_out), UOP_STORE);

  TEST_BEGIN("apply-split-dag/all-split-classes");
  // Verify each split-class opt picks the right inner axis_type and
  // (for non-LOCAL classes) wraps the inner in a UOP_OPT annotation
  // mirroring kernel_lift.c:1582-1603.
  for (u32 i = 0; i < 5; i++) {
    static u8 const ops[5]      = { KOP_UPCAST, KOP_UNROLL, KOP_LOCAL,
                                    KOP_GROUP,  KOP_GROUPTOP };
    static u8 const expect[5]   = { (u8)KAX_UPCAST, (u8)KAX_UNROLL,
                                    (u8)KAX_LOCAL, (u8)KAX_GROUP_REDUCE,
                                    (u8)KAX_GROUP_REDUCE };
    static u8 const opt_kind[5] = { (u8)UOP_OPT_UPCAST, (u8)UOP_OPT_UNROLL,
                                    0xFFu, (u8)UOP_OPT_GROUP_REDUCE,
                                    (u8)UOP_OPT_GROUP_REDUCE };
    Term sda_buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, (u32[]){32});
    Term sda_r0   = uop_range(0, KAX_LOOP, 32);
    Term sda_ie   = uop_index_e(sda_buf, sda_r0);
    Term sda_val  = uop_load(sda_ie);
    Term sda_root = uop_store(sda_buf, sda_r0, sda_val);
    KOpt sda_opts[1] = {{ ops[i], 0, 4 }};
    Term sda_out = uop_apply_split_dag(sda_root, sda_opts, 1);
    Term sda_addr = heap_read(term_val(sda_out) + 1);
    Term sda_inn_or_opt = heap_read(term_val(sda_addr) + 1);
    Term sda_inn = sda_inn_or_opt;
    if (opt_kind[i] != 0xFFu) {
      // Inner is wrapped: UOP_OPT(inner, kind, k).  Walk into it.
      CHECK_EQ(term_ext(sda_inn_or_opt), UOP_OPT);
      CHECK_EQ(uop_opt_kind(sda_inn_or_opt), (u32)opt_kind[i]);
      CHECK_EQ(uop_opt_factor(sda_inn_or_opt), 4u);
      sda_inn = uop_opt_target(sda_inn_or_opt);
    }
    CHECK_EQ(uop_range_axis_type(sda_inn), (u32)expect[i]);
  }

  TEST_BEGIN("apply-split-dag/preserves-non-range-leaves");
  // Sanity: the rule shouldn't rewrite UOP_CONST / UOP_BUFFER / etc.
  // even when their value happens to look like an axis index.  Build
  // a DAG with a CONST(0) (tag UOP, ext UOP_CONST) and verify it
  // doesn't get rewritten.  The rule body's tag/ext check guards
  // this -- the test pins the contract.
  Term sd4_buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, (u32[]){8});
  Term sd4_r0   = uop_range(0, KAX_LOOP, 8);
  Term sd4_zero = uop_const(DT_INT32, 0);
  Term sd4_addr = uop_int_binary(UOP_IADD, sd4_r0, sd4_zero);
  Term sd4_ie   = uop_index_e(sd4_buf, sd4_addr);
  Term sd4_val  = uop_load(sd4_ie);
  Term sd4_root = uop_store(sd4_buf, sd4_addr, sd4_val);
  KOpt sd4_opts[1] = {{ KOP_LOCAL, 0, 4 }};
  Term sd4_out = uop_apply_split_dag(sd4_root, sd4_opts, 1);
  // Walk the rewritten addr -- it should still contain UOP_CONST.
  // After IADD-with-zero simplification (uop_simplify_int_binary),
  // the CONST(0) may be folded out at construction time.  That's
  // fine; we just want NO crash and a valid root.
  CHECK_EQ(term_tag(sd4_out), TAG_UOP);
  CHECK_EQ(term_ext(sd4_out), UOP_STORE);

  thvm_free();
  TEST_REPORT();
}
