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
// without touching KernelAxes.  Mirrors KOP_LOCAL-on-existing-axis
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

  thvm_free();
  TEST_REPORT();
}
