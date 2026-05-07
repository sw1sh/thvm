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

// === Phase E3: KOP_SWAP UPatRule mirror =============================
//
// codegen/apply_opt.c:92-103 stamps:
//
//   if ((u8)opt.arg >= ax->n_axes) return 0;
//   u8 ti = ax->axis_types[opt.axis];
//   u8 tj = ax->axis_types[opt.arg];
//   ax->axis_types[opt.axis] = tj;
//   ax->axis_types[opt.arg]  = ti;
//   ax->full_shape[opt.axis] <-> ax->full_shape[opt.arg];
//
// kernel_lift.c:1416-1420 replays the same on the SplitAxis cur[]
// vector, swapping the entire entry (including extent/origin/factor).
// The lifter then emits UOP_RANGE leaves keyed by post-replay position,
// so the leaves carry the post-swap axis_types.
//
// === Composition with KOP_GLOBAL =====================================
//
// KOP_SWAP is meaningful for axis_type only when something else has
// previously stamped a non-LOOP type on one of the swapped positions
// (otherwise both ends are KAX_LOOP and the swap is a no-op for
// axis_type).  In practice the autotune sequence shape is "LOCAL/UPCAST
// split, GLOBAL stamp, then SWAP to reorder".  E3 therefore composes
// against KOP_GLOBAL within the same scan: we simulate desired[i]
// transitions through both KOP_GLOBAL and KOP_SWAP entries, ignoring
// split-class opts (whose axis-insertion semantics belong to a later
// wedge alongside per-axis index drift tracking).
//
// === Single-pass full-history simulation =============================
//
// The rule walks applied_opts left-to-right, tracking a
// desired_axis_type[MAX_AXES] state initialised to KAX_LOOP for every
// position.  KOP_GLOBAL(a, _) sets desired[a]=KAX_GLOBAL when desired[a]
// is currently KAX_LOOP (mirrors apply_opt's LOOP precondition); other
// transitions are ignored (out-of-scope for this wedge).  KOP_SWAP(a,
// b) swaps desired[a] and desired[b].
//
// For the matched UOP_RANGE leaf at axis_id=a, if desired[a] differs
// from the leaf's current axis_type, rewrite to a UOP_RANGE with the
// computed axis_type.  Otherwise the rule no-ops and the original Term
// flows through unchanged.
//
// Composition handled:
//   - SWAP(a,b) alone:       no-op (both desired ends stay LOOP).
//   - GLOBAL(a) then SWAP(a,b):
//       desired[a]=GLOBAL, then SWAP -> desired[a]=LOOP, desired[b]=GLOBAL.
//       The UOP_RANGE at axis_id=b should be rewritten to KAX_GLOBAL.
//   - SWAP(a,b) then SWAP(b,c):
//       desired stays LOOP everywhere -> no rewrite.
//   - GLOBAL(a) then SWAP(a,b) then SWAP(b,c):
//       desired = GLOBAL at c (after composing the two swaps).
//
// Idempotence: re-applying the rule produces the same desired[a],
// matched against the now-updated leaf axis_type -- the second pass
// returns 0 because the leaf already carries desired[a].

// rule body (KOP_SWAP scope): matches a UOP_RANGE leaf, simulates the
// composed KOP_GLOBAL/KOP_SWAP history, and rewrites the leaf to its
// computed axis_type when different from the current one.
static Term rw_kop_swap_stamp(Term const *bindings, void *ctx_in) {
  Term range = bindings[0];
  if (term_tag(range) != TAG_UOP || term_ext(range) != UOP_RANGE) return 0;

  UopApplyOptCtx const *ctx = (UopApplyOptCtx const *)ctx_in;
  if (ctx == NULL || ctx->applied_opts == NULL || ctx->n_applied == 0) {
    return 0;
  }

  // Simulate desired axis_type per position over the opts history.
  u8 desired[MAX_AXES];
  for (u32 i = 0; i < MAX_AXES; i++) desired[i] = (u8)KAX_LOOP;
  for (u32 i = 0; i < ctx->n_applied; i++) {
    KOpt const *o = &ctx->applied_opts[i];
    if (o->op == KOP_GLOBAL) {
      if ((u32)o->axis < MAX_AXES && desired[o->axis] == (u8)KAX_LOOP) {
        desired[o->axis] = (u8)KAX_GLOBAL;
      }
    } else if (o->op == KOP_SWAP) {
      u32 a = (u32)o->axis;
      u32 b = (u32)o->arg;
      if (a < MAX_AXES && b < MAX_AXES) {
        u8 t = desired[a];
        desired[a] = desired[b];
        desired[b] = t;
      }
    }
    // Other opts (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP/TC/PADTO/NOLOCALS)
    // are out of scope for the SWAP wedge; their axis-insertion semantics
    // belong to later wedges that will track per-opt index drift.
  }

  u32 axis_id = uop_range_axis_id(range);
  if (axis_id >= MAX_AXES) return 0;
  u8  cur_at  = (u8)uop_range_axis_type(range);
  if (desired[axis_id] == cur_at) return 0;
  return uop_range_with_axis_type(range, desired[axis_id]);
}

static UPat const upat_kop_swap_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

static UPatRule const upat_kop_swap_rules[1] = {
  {&upat_kop_swap_range, rw_kop_swap_stamp},
};

// Public entry: walk the DAG rooted at `root`, simulating the composed
// KOP_GLOBAL/KOP_SWAP history in `applied_opts` and stamping each
// UOP_RANGE leaf with its computed axis_type.  Returns the rewritten
// root (input root unchanged when the simulated state matches the leaf
// already).  Idempotent: re-applying with the same opts is a no-op
// because the leaves already carry the simulated axis_types.
fn Term uop_apply_kop_swap(Term root, KOpt const *applied_opts,
                           u32 n_applied) {
  UopApplyOptCtx ctx = { applied_opts, n_applied };
  return uop_pattern_rewrite(root, upat_kop_swap_rules, 1, &ctx);
}

// === Phase E4-E6: split-class UPatRule (UPCAST/UNROLL/LOCAL/GROUP/
//                                        GROUPTOP) -- pragmatic stamp ====
//
// codegen/apply_opt.c:61-85 splits axis a with arg k into:
//   outer at position a    (size /= k, axis_type unchanged)
//   inner at position a+1  (size  = k, axis_type = inner_kax)
// where inner_kax is:
//   KOP_UPCAST   -> KAX_UPCAST
//   KOP_UNROLL   -> KAX_UNROLL
//   KOP_LOCAL    -> KAX_LOCAL
//   KOP_GROUP    -> KAX_GROUP_REDUCE
//   KOP_GROUPTOP -> KAX_GROUP_REDUCE
//
// kernel_lift.c:1574-1617 mirrors the same on its SplitAxis cur[] and
// then emits UOP_RANGE leaves keyed by post-replay position.  Each
// emitted leaf already carries the post-replay axis_type / extent.
//
// === Pragmatic scope (stamp-only, no DAG split) ========================
//
// The full UPat-driven port would need to REPLACE one UOP_RANGE leaf
// with two new UOP_RANGE leaves wired into a UOP_IADD/IMUL chain --
// the same DAG transform kernel_lift.c performs structurally today.
// That's a multi-step rewrite that doesn't fit a single rewrite-fn
// emitting one Term.  This wedge keeps splits in kernel_lift.c's
// replay loop and ports only the axis_type stamping: the rule
// simulates the same cur[] evolution to compute desired[a] for every
// post-replay position, then stamps EXISTING UOP_RANGE leaves whose
// axis_id matches a position whose desired[] differs from the leaf's
// current axis_type.
//
// The actual range-creation (the "split" itself) stays in
// kernel_lift.c structural-replay until a future wedge introduces a
// `uop_range_split` primitive that returns a (outer, inner) pair and
// rewires the consumer's INDEX_E address arithmetic.  Until then, this
// rule is exercised in E4-E6 tests on already-emitted leaves and can be
// composed declaratively over apply_opt history without altering the
// in-tree structural lowering.
//
// === Single-pass full-history simulation ==============================
//
// The rule walks applied_opts left-to-right, tracking:
//   - n_cur:           current count of populated axes
//   - desired[i]:      the post-replay axis_type at position i
// initialised so positions 0..n_init are KAX_LOOP (callers are expected
// to drive the rule on UOP_RANGE leaves whose pre-split axis_id sits in
// 0..n_init; for the stamp scope this is the same as the lifter's
// initial cur[] vector).  Each opt updates the state:
//
//   SPLIT(o.axis, k) at position p=o.axis:
//     - Shift desired[p+1..n_cur] right by 1.
//     - desired[p+1] = inner_kax(o.op).  (Outer at p keeps its type.)
//     - n_cur++.
//
//   GLOBAL(o.axis): desired[a] = KAX_GLOBAL when KAX_LOOP (LOOP guard).
//   SWAP(a, b):     swap desired[a] <-> desired[b].
//
// For each matched UOP_RANGE leaf whose axis_id is in [0, n_cur), if
// desired[axis_id] differs from the leaf's current axis_type, rewrite
// the leaf with the simulated axis_type.  Otherwise no-op.
//
// Idempotent: desired[a] is a pure function of `applied_opts`, so a
// second pass produces the same desired and the leaves are already
// stamped.

static u8 kop_inner_axis_type(u8 op) {
  if (op == KOP_UPCAST)   return (u8)KAX_UPCAST;
  if (op == KOP_UNROLL)   return (u8)KAX_UNROLL;
  if (op == KOP_LOCAL)    return (u8)KAX_LOCAL;
  if (op == KOP_GROUP)    return (u8)KAX_GROUP_REDUCE;
  if (op == KOP_GROUPTOP) return (u8)KAX_GROUP_REDUCE;
  return (u8)KAX_LOOP;
}

static int kop_is_split(u8 op) {
  return op == KOP_UPCAST || op == KOP_UNROLL || op == KOP_LOCAL
      || op == KOP_GROUP  || op == KOP_GROUPTOP;
}

// Simulate the full applied_opts history on a desired[MAX_AXES] vector.
// Returns the post-replay axis count (n_cur).  All positions in
// [0, n_cur) carry their post-replay axis_type in `desired_out`.
// Positions in [n_cur, MAX_AXES) are KAX_LOOP (initial state).
static u32 sim_kop_history(KOpt const *applied_opts, u32 n_applied,
                           u8 *desired_out) {
  for (u32 i = 0; i < MAX_AXES; i++) desired_out[i] = (u8)KAX_LOOP;
  // The lifter seeds cur[] from the BUFFERIZE S_RANGE.src list, whose
  // count isn't visible here; for the stamp port we treat any axis
  // referenced by an opt (or matched against a UOP_RANGE leaf) as a
  // valid initial position.  n_cur tracks the highest live position.
  u32 n_cur = 0;
  for (u32 i = 0; i < n_applied; i++) {
    KOpt const *o = &applied_opts[i];
    u8 op = o->op;
    if (kop_is_split(op)) {
      u32 a = (u32)o->axis;
      if (a >= MAX_AXES - 1) continue;     // out-of-range: skip
      // Grow n_cur to cover position a if needed (the matched leaf may
      // be the only thing referencing this axis, so initial-population
      // is implicit).
      if (a >= n_cur) n_cur = a + 1;
      if (n_cur >= MAX_AXES) continue;     // table full: skip the split
      // Shift positions a+1 .. n_cur-1 right by 1 to make room for the
      // new inner axis at position a+1.
      for (u32 j = n_cur; j > a + 1; j--) desired_out[j] = desired_out[j - 1];
      // Outer at position a keeps its desired_out[a] unchanged.  Inner
      // at position a+1 takes the opt's KAX type.
      desired_out[a + 1] = kop_inner_axis_type(op);
      n_cur++;
    } else if (op == KOP_GLOBAL) {
      u32 a = (u32)o->axis;
      if (a >= MAX_AXES) continue;
      if (a >= n_cur) n_cur = a + 1;
      if (desired_out[a] == (u8)KAX_LOOP) {
        desired_out[a] = (u8)KAX_GLOBAL;
      }
    } else if (op == KOP_SWAP) {
      u32 a = (u32)o->axis;
      u32 b = (u32)o->arg;
      if (a >= MAX_AXES || b >= MAX_AXES) continue;
      if (a >= n_cur) n_cur = a + 1;
      if (b >= n_cur) n_cur = b + 1;
      u8 t = desired_out[a];
      desired_out[a] = desired_out[b];
      desired_out[b] = t;
    } else if (op == KOP_TC) {
      // Phase E7: KOP_TC is kernel-aware metadata (tensor-core hint
      // recognised by render_uop's rmu_detect_matmul_tc /
      // rmu_emit_matmul_tc and by uop_recognise_tc which installs the
      // UOP_OPT(_, TC, 0) wrapper around the matmul reduce).  It does
      // NOT mutate axis_types in either codegen/apply_opt.c (where
      // kernel_apply_opt routes TC to tile_anno_record_opt without
      // touching ax->axis_types[]) or in kernel_lift.c's structural
      // replay (which explicitly skips KOP_TC: "Tensor-core opt is
      // metadata-only; pattern-matched in render").  We make the
      // no-op explicit here so the simulation enumerates every KOP_*
      // class and KOP_TC composes safely with later GLOBAL/SWAP/SPLIT
      // entries (the autotune sequence guard requires TC to appear at
      // index 0, but the rule must still produce the same desired[]
      // when callers pass an applied_opts list whose TC entry happens
      // to land elsewhere -- e.g. test seams or future relaxations).
    }
    // Remaining opts (KOP_PADTO, KOP_NOLOCALS) are reserved and not
    // emitted by any current producer; they're out of scope for E*.
  }
  return n_cur;
}

// rule body: matches a UOP_RANGE leaf, simulates the composed split +
// GLOBAL + SWAP history, and rewrites the leaf to its post-replay
// axis_type when different from the current one.
static Term rw_kop_split_stamp(Term const *bindings, void *ctx_in) {
  Term range = bindings[0];
  if (term_tag(range) != TAG_UOP || term_ext(range) != UOP_RANGE) return 0;

  UopApplyOptCtx const *ctx = (UopApplyOptCtx const *)ctx_in;
  if (ctx == NULL || ctx->applied_opts == NULL || ctx->n_applied == 0) {
    return 0;
  }

  u8 desired[MAX_AXES];
  u32 n_cur = sim_kop_history(ctx->applied_opts, ctx->n_applied, desired);
  (void)n_cur;

  u32 axis_id = uop_range_axis_id(range);
  if (axis_id >= MAX_AXES) return 0;
  u8  cur_at  = (u8)uop_range_axis_type(range);
  if (desired[axis_id] == cur_at) return 0;
  return uop_range_with_axis_type(range, desired[axis_id]);
}

static UPat const upat_kop_split_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

static UPatRule const upat_kop_split_rules[1] = {
  {&upat_kop_split_range, rw_kop_split_stamp},
};

// Public entry: walk the DAG rooted at `root`, simulating the composed
// split (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) + GLOBAL + SWAP history in
// `applied_opts` and stamping each UOP_RANGE leaf with its post-replay
// axis_type.  This is the pragmatic stamp-only port for E4-E6: the
// underlying axis-INSERTION (each split adds a new UOP_RANGE leaf) stays
// in kernel_lift.c's structural replay; this rule only fixes up
// axis_type on already-emitted leaves whose axis_id sits in the
// post-replay range.  Idempotent: desired[a] is a pure function of
// applied_opts.  See the file-header block above for the simulation
// rules.
fn Term uop_apply_kop_split(Term root, KOpt const *applied_opts,
                            u32 n_applied) {
  UopApplyOptCtx ctx = { applied_opts, n_applied };
  return uop_pattern_rewrite(root, upat_kop_split_rules, 1, &ctx);
}

// === Phase E7: KOP_TC UPatRule mirror ================================
//
// codegen/apply_opt.c:116-132 routes KOP_TC through `kernel_apply_opt`
// (NOT axes_apply_opt): it validates the axis index and the requested
// MMA tile, calls tile_analyze_gemm to confirm the kernel's matmul
// shape + DT_FP32 dtype, and on success records the opt via
// `tile_anno_record_opt` -- which appends the opt to applied_opts[]
// and bumps version WITHOUT touching ax->axis_types[] or
// ax->full_shape[].  kernel_lift.c:1628-1629 mirrors the same
// "metadata-only" treatment in its structural replay
// ("Tensor-core opt is metadata-only; pattern-matched in render").
//
// Concretely: KOP_TC's KernelAxes mutation is the empty mutation.  Its
// downstream effect lives entirely in render_uop.c
// (rmu_detect_matmul_tc / rmu_emit_matmul_tc) and the producer
// uop_recognise_tc, both of which key off the UOP_OPT(_, TC, 0)
// wrapper installed structurally on the matmul reduce -- not off
// any UOP_RANGE.axis_type stamp.
//
// === This rule's job =================================================
//
// Mirror that empty mutation declaratively, sharing the same
// `sim_kop_history` simulation as KOP_GLOBAL/SWAP/SPLIT so callers
// who feed `applied_opts` containing a KOP_TC entry alongside other
// opts get the SAME desired[] outcome they would get from the
// existing combined rule.  The shared simulation already enumerates
// KOP_TC explicitly (see the KOP_TC branch in sim_kop_history above)
// so the rule body and dispatch are identical to the split-class
// rule -- a separate public entry just lets callers name "TC was
// applied" at the API level for symmetry with E2/E3/E4-E6.
//
// In practice the autotune sequence guard
// (kautotune_seq_can_append in src/codegen/autotune.c) restricts
// KOP_TC to the FIRST entry of an opts sequence, so applied_opts in
// the autotune flow is either {TC} alone (for which the rule produces
// desired = [LOOP, LOOP, ...] -- a no-op) or {TC, ...other ops...} (in
// which case the rule produces the same desired[] the post-TC opts
// would produce on their own).  Idempotent: desired[a] is a pure
// function of applied_opts -- KOP_TC contributes nothing.

// rule body: matches a UOP_RANGE leaf, simulates the full applied_opts
// history (including any KOP_TC entries -- explicit no-ops) on
// desired[MAX_AXES], and rewrites the leaf to its post-replay
// axis_type when different from the current one.  Identical to
// rw_kop_split_stamp; lifted as a separate symbol for clarity at the
// rule-table level.
static Term rw_kop_tc_stamp(Term const *bindings, void *ctx_in) {
  Term range = bindings[0];
  if (term_tag(range) != TAG_UOP || term_ext(range) != UOP_RANGE) return 0;

  UopApplyOptCtx const *ctx = (UopApplyOptCtx const *)ctx_in;
  if (ctx == NULL || ctx->applied_opts == NULL || ctx->n_applied == 0) {
    return 0;
  }

  u8 desired[MAX_AXES];
  u32 n_cur = sim_kop_history(ctx->applied_opts, ctx->n_applied, desired);
  (void)n_cur;

  u32 axis_id = uop_range_axis_id(range);
  if (axis_id >= MAX_AXES) return 0;
  u8  cur_at  = (u8)uop_range_axis_type(range);
  if (desired[axis_id] == cur_at) return 0;
  return uop_range_with_axis_type(range, desired[axis_id]);
}

static UPat const upat_kop_tc_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

static UPatRule const upat_kop_tc_rules[1] = {
  {&upat_kop_tc_range, rw_kop_tc_stamp},
};

// Public entry: walk the DAG rooted at `root`, simulating the full
// applied_opts history (including any KOP_TC entries, which are
// explicit no-ops at the axis_type level) and stamping each UOP_RANGE
// leaf with its post-replay axis_type.  KOP_TC does not mutate
// axis_type in either codegen/apply_opt.c or kernel_lift.c
// structural-replay -- see the file-header block above for the
// detailed semantics.  Idempotent: desired[a] is a pure function of
// applied_opts.
fn Term uop_apply_kop_tc(Term root, KOpt const *applied_opts,
                        u32 n_applied) {
  UopApplyOptCtx ctx = { applied_opts, n_applied };
  return uop_pattern_rewrite(root, upat_kop_tc_rules, 1, &ctx);
}

// === E9-prep wedge 1: unified kernel-opts pass =======================
//
// uop_apply_kernel_opts composes the four E2/E3/E4-E6/E7 entries into a
// single DAG walk that stamps every UOP_RANGE leaf with its post-replay
// axis_type.  The composition is faithful to the lifter's structural
// replay (kernel_lift.c:1568-1633) when applied AFTER the lifter has
// already produced UOP_RANGE leaves keyed by their post-replay
// axis_id and extent.
//
// Faithful semantics (matching the lifter):
//   - Initial desired[] starts at KAX_LOOP for every position.
//   - Splits (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) shift desired[] right
//     and stamp inner = inner_kax(op).
//   - GLOBAL only stamps when desired[a]==KAX_LOOP AND opt.arg matches
//     the current outer extent.  The extent guard mirrors
//     kernel_lift.c:1619 ("o.arg != cur[o.axis].extent -> return 0").
//   - SWAP swaps desired[a] <-> desired[b] (and the cur[] entries the
//     lifter keeps).
//   - TC is metadata-only -- explicit no-op.
//
// The shared `sim_kop_history` (used by E4-E7) already handles
// splits/SWAP/GLOBAL/TC composition but lacks the extent guard on
// GLOBAL because it has no per-axis extent table.  This unified pass
// runs the simulation with extent tracking, mirroring the lifter's
// SplitAxis cur[] vector.
//
// Idempotent: desired[a] is a pure function of `applied_opts` and the
// initial extents read from the matched UOP_RANGE leaves.  Running the
// pass twice produces hash-cons-identical output.
//
// Wired into materialize.c after kernel_lift_to_uop succeeds.  In the
// default config the lifter already stamps the same axis_types via its
// own structural replay, so this pass is a no-op (every leaf's
// axis_type already equals desired[axis_id]).  The `THVM_E9_VALIDATE=1`
// env knob asserts that no-op semantically: when set,
// uop_apply_kernel_opts records every range it would have rewritten
// into a fire counter; non-zero on exit means the rules and the
// lifter disagree.

// Per-pass fire counter for validation.  Captured per call (not
// global) via the ctx struct.
typedef struct {
  KOpt const *applied_opts;
  u32         n_applied;
  // Pre-computed desired[] from the simulation, indexed by axis_id.
  u8          desired[MAX_AXES];
  // For each position whose desired[i] is KAX_GLOBAL, the arg that
  // the GLOBAL opt requested.  When the matched leaf's extent doesn't
  // equal global_arg[i], the GLOBAL stamp is rejected (mirrors the
  // lifter's `o.arg != cur[o.axis].extent` guard at kernel_lift.c:1619).
  // Zero means "no GLOBAL stamp on this position" (the
  // `desired[i] == KAX_GLOBAL` check below is the live signal).
  u32         global_arg[MAX_AXES];
  u32         n_cur;
  u32         fire_count;  // diagnostic: incremented per rewrite
} UopApplyKernelOptsCtx;

// Simulate the full applied_opts history with extent tracking.  Mirrors
// kernel_lift.c:1568-1633.  Writes desired[i]=axis_type and
// global_arg[i]=opt.arg-at-stamp-time when the GLOBAL guard fires (the
// rewrite body re-checks against the matched leaf's extent).
// Positions beyond n_cur stay at the LOOP/0 initial state.  Returns 1
// always; the rule body decides whether to stamp.
static int sim_kop_history_with_extents(KOpt const *applied_opts,
                                        u32 n_applied,
                                        u8 *desired_out,
                                        u32 *global_arg_out,
                                        u32 *n_cur_out) {
  for (u32 i = 0; i < MAX_AXES; i++) {
    desired_out[i]    = (u8)KAX_LOOP;
    global_arg_out[i] = 0;
  }
  u32 n_cur = 0;
  for (u32 i = 0; i < n_applied; i++) {
    KOpt const *o = &applied_opts[i];
    u8 op = o->op;
    if (kop_is_split(op)) {
      u32 a = (u32)o->axis;
      if (a >= MAX_AXES - 1) continue;
      if (a >= n_cur) n_cur = a + 1;
      if (n_cur >= MAX_AXES) continue;
      // Shift right (positions a+1..n_cur-1 -> a+2..n_cur).
      for (u32 j = n_cur; j > a + 1; j--) {
        desired_out[j]    = desired_out[j - 1];
        global_arg_out[j] = global_arg_out[j - 1];
      }
      // Outer at position a keeps its desired/global_arg unchanged.
      // Inner at position a+1 takes the opt's KAX type.
      desired_out[a + 1]    = kop_inner_axis_type(op);
      global_arg_out[a + 1] = 0;
      n_cur++;
    } else if (op == KOP_GLOBAL) {
      u32 a = (u32)o->axis;
      if (a >= MAX_AXES) continue;
      if (a >= n_cur) n_cur = a + 1;
      // Lifter guard: only stamps when LOOP.  We capture o.arg here;
      // the rule body re-checks `arg == leaf.extent` against the
      // matched UOP_RANGE leaf at this axis_id.
      if (desired_out[a] != (u8)KAX_LOOP) continue;
      desired_out[a]    = (u8)KAX_GLOBAL;
      global_arg_out[a] = o->arg;
    } else if (op == KOP_SWAP) {
      u32 a = (u32)o->axis;
      u32 b = (u32)o->arg;
      if (a >= MAX_AXES || b >= MAX_AXES) continue;
      if (a >= n_cur) n_cur = a + 1;
      if (b >= n_cur) n_cur = b + 1;
      u8  t = desired_out[a];
      u32 g = global_arg_out[a];
      desired_out[a]    = desired_out[b];
      global_arg_out[a] = global_arg_out[b];
      desired_out[b]    = t;
      global_arg_out[b] = g;
    } else if (op == KOP_TC) {
      // Metadata-only; no axis_type or extent mutation.
    }
    // Remaining (KOP_PADTO, KOP_NOLOCALS) are reserved.
  }
  *n_cur_out = n_cur;
  return 1;
}

static Term rw_kernel_opts_stamp(Term const *bindings, void *ctx_in) {
  Term range = bindings[0];
  if (term_tag(range) != TAG_UOP || term_ext(range) != UOP_RANGE) return 0;

  UopApplyKernelOptsCtx *ctx = (UopApplyKernelOptsCtx *)ctx_in;
  if (ctx == NULL || ctx->applied_opts == NULL || ctx->n_applied == 0) {
    return 0;
  }

  u32 axis_id = uop_range_axis_id(range);
  if (axis_id >= MAX_AXES) return 0;
  u8  cur_at  = (u8)uop_range_axis_type(range);
  u8  want    = ctx->desired[axis_id];
  if (want == cur_at) return 0;
  // Strict GLOBAL extent guard: the lifter rejects KOP_GLOBAL when
  // o.arg != cur[o.axis].extent (kernel_lift.c:1619).  Mirror that
  // by checking the matched leaf's extent against the captured arg.
  // When the guard fails, treat as no-op -- the lifter would not
  // have stamped this axis in the first place.
  if (want == (u8)KAX_GLOBAL) {
    u32 leaf_ext = uop_range_extent(range);
    if (ctx->global_arg[axis_id] != leaf_ext) return 0;
  }
  ctx->fire_count++;
  return uop_range_with_axis_type(range, want);
}

static UPat const upat_kernel_opts_range = {UOP_RANGE, 0, 0, 0, NULL, NULL};

static UPatRule const upat_kernel_opts_rules[1] = {
  {&upat_kernel_opts_range, rw_kernel_opts_stamp},
};

// Public entry: compose all four UPatRules (E2 GLOBAL, E3 SWAP,
// E4-E6 splits, E7 TC) into a single DAG walk and stamp every
// UOP_RANGE leaf reachable from `root` with its post-replay axis_type.
//
// In the default lifter config (kernel_lift.c stamps axis_types via
// its own structural replay before producing the DAG), this pass
// observes desired[axis_id] == leaf.axis_type for every leaf and
// returns hash-cons-identical output.  That bit-equality is what
// validates the rules: if the unified pass diverges from the lifter,
// either the lifter or the rules are wrong about how applied_opts[]
// composes.
fn Term uop_apply_kernel_opts(Term root, KOpt const *applied_opts,
                              u32 n_applied) {
  if (root == 0 || applied_opts == NULL || n_applied == 0) return root;
  UopApplyKernelOptsCtx ctx;
  ctx.applied_opts = applied_opts;
  ctx.n_applied    = n_applied;
  ctx.fire_count   = 0;
  sim_kop_history_with_extents(applied_opts, n_applied,
                               ctx.desired, ctx.global_arg, &ctx.n_cur);
  return uop_pattern_rewrite(root, upat_kernel_opts_rules, 1, &ctx);
}

// Validation entry: same walk as uop_apply_kernel_opts but reports
// the fire-count via *out_fires (0 means "no rule fired -- the lifter
// and the rules agree on every UOP_RANGE leaf in the DAG").  The
// returned Term is the rewritten root; the caller can compare it to
// `root` to detect any structural divergence (when fires == 0 the two
// should be hash-cons-identical).
fn Term uop_apply_kernel_opts_validate(Term root, KOpt const *applied_opts,
                                       u32 n_applied, u32 *out_fires) {
  if (out_fires != NULL) *out_fires = 0;
  if (root == 0 || applied_opts == NULL || n_applied == 0) return root;
  UopApplyKernelOptsCtx ctx;
  ctx.applied_opts = applied_opts;
  ctx.n_applied    = n_applied;
  ctx.fire_count   = 0;
  sim_kop_history_with_extents(applied_opts, n_applied,
                               ctx.desired, ctx.global_arg, &ctx.n_cur);
  Term out = uop_pattern_rewrite(root, upat_kernel_opts_rules, 1, &ctx);
  if (out_fires != NULL) *out_fires = ctx.fire_count;
  return out;
}
