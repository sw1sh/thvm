// uop/apply_opt.c -- UPatRule[] mirror of codegen/apply_opt.c's
// KernelAxes mutations against UOP_RANGE.axis_type.
//
// Phase E ports each apply_opt op class to a UPatRule over the UOP_RANGE
// leaves emitted by kernel_lift_to_uop's structural-replay loop
// (src/schedule/kernel_lift.c).  E1 landed the read/write seam (field
// accessors on UOP_RANGE + uop_range_with_axis_type rewriter primitive
// in src/uop/index.c, plus an end-to-end UPatRule probe in
// tests/test_uop_range_axis_type.c); E2 lands the first concrete
// mutation: KOP_GLOBAL.
//
// The rule does NOT yet replace the corresponding write in
// codegen/apply_opt.c -- both representations stay live during the
// E* wedge sequence.  KernelAxes.axis_types[] remains the primary
// source of truth; this rule mirrors the same decision in
// declarative form so subsequent E* wedges can compose against it.
//
// === KOP_GLOBAL semantics (mirroring axes_apply_opt) =================
//
// codegen/apply_opt.c:86-91 stamps:
//
//   if (opt.arg != axis_size || ax->axis_types[opt.axis] != KAX_LOOP)
//     return 0;
//   ax->axis_types[opt.axis] = KAX_GLOBAL;
//
// kernel_lift.c:1411-1415 replays the same:
//
//   if (cur[o.axis].axis_type != KAX_LOOP || o.arg != cur[o.axis].extent)
//     return 0;
//   cur[o.axis].axis_type = KAX_GLOBAL;
//
// Both guard on (axis_type==KAX_LOOP, arg==extent).  This rule mirrors
// the same predicate against a UOP_RANGE leaf: when applied_opts
// contains a KOP_GLOBAL whose axis index equals UOP_RANGE.axis_id and
// whose arg equals UOP_RANGE.extent, the rule rewrites the leaf to a
// new UOP_RANGE with axis_type=KAX_GLOBAL via uop_range_with_axis_type.
//
// === Scoping ==========================================================
//
// A KernelAxes can carry a sequence of opts that interleave splits
// (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) with SWAPs and GLOBALs.  Splits
// reshape cur[] (insert a new entry); SWAPs reorder it.  After replay,
// the final UOP_RANGE.axis_id equals the position in the post-replay
// cur[] vector -- not the axis index that KOP_GLOBAL named when it was
// recorded.  Tracking that index drift requires the same per-opt
// shifting the lifter does.
//
// E2 keeps the rule simple: it matches when KOP_GLOBAL.axis is a
// stable index, i.e. the no-split-and-no-swap-between case.  This is
// the typical autotune use ("LOCAL splits N into LOOP(N/L) + LOCAL(L);
// GLOBAL marks the resulting LOOP(N/L) for grid-binding") whose
// LOCAL split happens BEFORE GLOBAL on the SAME axis -- in that case
// KOP_GLOBAL.axis still equals the outer axis_id in cur[].  The full
// SWAP-between-GLOBAL coverage lands in a later wedge alongside
// KOP_SWAP's own port.  When the rule's guard fails, the lifter's
// in-tree replay still produces the correct UOP_RANGE.axis_type, so
// behaviour is unchanged.
//
// === Rewriter reach (orthogonal limitation) ===========================
//
// uop_pattern_rewrite descends through nodes whose opcodes appear in
// uop_arity()'s switch (src/schedule/uop_meta.c) and whose rebuild
// case is enumerated in uop_graph_rebuild_with_srcs (graph_rewrite.c).
// Both currently cover float arithmetic (UOP_NEG/RECIP/EXP2/LOG2/SQRT,
// UOP_ADD/MUL/CMPLT/CMPEQ, UOP_REDUCE), movement (UOP_RESHAPE/PERMUTE/
// EXPAND/PAD/SHRINK/FLIP), and UOP_LOAD/CAST/BITCAST -- but NOT the
// integer arithmetic (UOP_IADD/IMUL/IDIV/IMOD/ILT/IAND), UOP_INDEX_E,
// UOP_OPT, UOP_RANGE, UOP_IWHERE, or UOP_INVALID.  In production
// kernel_lift output the UOP_RANGE leaves are always nested inside
// UOP_INDEX_E.addr expressions (which themselves are IADD/IMUL chains),
// so this rule only fires when applied to a bare UOP_RANGE root.
// Wiring it into a production pass requires extending uop_arity +
// uop_graph_rebuild_with_srcs first; that's a separate wedge from the
// per-mutation port itself, deferred until the pass actually consumes
// kernel_lift output.

// === ctx + rewrite fn =================================================

typedef struct {
  KOpt const *applied_opts;
  u32         n_applied;
} UopApplyOptCtx;

// rule body: matches a UOP_RANGE leaf (bound at slot 0); checks
// applied_opts for a KOP_GLOBAL whose axis == range.axis_id and
// arg == range.extent; rewrites to range with axis_type=KAX_GLOBAL.
//
// Guards (mirroring the apply_opt + lift replay):
//   - range.axis_type must be KAX_LOOP (idempotence: re-running the
//     rule on an already-globalised range is a no-op).
//   - applied_opts[i].arg must equal range.extent (apply_opt rejects
//     KOP_GLOBAL with a stale full-axis-size; this rule mirrors).
//
// Returns 0 on no-match (uop_pattern_rewrite caches the original term
// unchanged) so non-LOOP ranges and ranges without a matching
// KOP_GLOBAL flow through.
static Term rw_kop_global_stamp(Term const *bindings, void *ctx_in) {
  Term range = bindings[0];
  if (term_tag(range) != TAG_UOP || term_ext(range) != UOP_RANGE) return 0;
  if (uop_range_axis_type(range) != KAX_LOOP) return 0;

  UopApplyOptCtx const *ctx = (UopApplyOptCtx const *)ctx_in;
  if (ctx == NULL || ctx->applied_opts == NULL || ctx->n_applied == 0) {
    return 0;
  }

  u32 axis_id = uop_range_axis_id(range);
  u32 extent  = uop_range_extent(range);
  for (u32 i = 0; i < ctx->n_applied; i++) {
    KOpt const *o = &ctx->applied_opts[i];
    if (o->op != KOP_GLOBAL)         continue;
    if ((u32)o->axis != axis_id)     continue;
    if (o->arg != extent)            continue;
    return uop_range_with_axis_type(range, KAX_GLOBAL);
  }
  return 0;
}

// === UPat declaration =================================================
// Match a UOP_RANGE leaf and capture it at bindings[0].  UOP_RANGE has
// heap arity 0 from upat_match's perspective (children of UOP_RANGE are
// raw NUMs, not Terms walked by uop_arity); we trust the op pinning to
// match leaves and inspect fields via the accessors in the rewrite body.
static UPat const upat_kop_global_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

static UPatRule const upat_kop_global_rules[1] = {
  {&upat_kop_global_range, rw_kop_global_stamp},
};

// Public entry: walk the DAG rooted at `root`, applying the KOP_GLOBAL
// stamp rule under `applied_opts` ctx.  Returns the rewritten root
// (input root unchanged when no UOP_RANGE leaves match).  Idempotent:
// running on a previously-stamped DAG is a no-op (the LOOP guard
// rejects KAX_GLOBAL leaves on the second pass).
fn Term uop_apply_kop_global(Term root, KOpt const *applied_opts,
                             u32 n_applied) {
  UopApplyOptCtx ctx = { applied_opts, n_applied };
  return uop_pattern_rewrite(root, upat_kop_global_rules, 1, &ctx);
}
