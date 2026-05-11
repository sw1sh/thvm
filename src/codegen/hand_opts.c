// codegen/hand_opts.c -- port of tinygrad's hand_coded_optimizations
// (tinygrad/codegen/opt/heuristic.py) into thvm.
//
// Given a finalized kernel (post-rangeify, scalar arena -> lifted UOp
// DAG in `cached_lift.store_root`, or a schedule-path fixture), inspect
// its shape/dtype and apply a sensible sequence of KOpts via the
// existing `kernel_apply_opt`.  This is the cheap-by-default analogue
// of tinygrad's hand-coded catalogue (no benchmarking, unlike BEAM
// autotune which stays gated on THVM_AUTOTUNE).  It is wired into
// kernel_fire_by_id and runs before dispatch, NOT gated on
// THVM_AUTOTUNE -- BUT see "DEFAULT STATE: OFF" below: in this v1 the
// THVM_HAND_CODED_OPTS env knob defaults to OFF, so absent that flag
// the function is a no-op (it still marks ke->schedule->autotuned so
// re-dispatch is cheap).
//
// === DEFAULT STATE: OFF ============================================
// THVM_HAND_CODED_OPTS defaults to OFF in this v1.  Reason: thvm's
// DAG-mode renderer (render_uop.c) disables its default-parallelise
// pass the moment ANY per-axis OPT wrapper is present
// (`any_opt` gate in rmu_emit_store / rmu_emit_output_loops) -- so a
// UPCAST/LOCAL applied to a multi-output-axis kernel turns the
// remaining output axes back into serial for-loops with NO bounds
// guard, while cg_tile_metal_dispatch_shape (which reads
// ke->schedule's applied_opts log, NOT the mutated DAG) still launches
// one-thread-per-output-element.  The kernel then over-computes by
// ~Nthreads and may read OOB.  Until the renderer learns a
// multi-GLOBAL modulus decode for OPT'd kernels AND
// cg_tile_metal_dispatch_shape derives its shape from the DAG, the
// safe default is OFF.  Flip THVM_HAND_CODED_OPTS=1 to exercise the
// heuristic (it applies cleanly to single-output-axis kernels: the
// reduce-tail elementwise+GROUP/UNROLL cases, where the renderer's
// reduce-shape emit + the conv2d/group-reduce dispatch readers agree).
//
// === Heuristic cases ported (mirrors hand_coded_optimizations) ====
//   1. TC -- matmul-shaped + f32 + K % tile == 0 -> KOP_TC(tile=8).
//      (uop_recognise_tc already wraps every matmul STORE in
//      OPT(_, TC, 0) inside cg_emit_via_uop, so this is mostly a
//      re-affirm; kept for the explicit `applied_opts` record on the
//      schedule path + future parallel-TC dispatch work.)
//   2. UPCAST -- pick an upcast factor (4, then 2) on the innermost
//      output axis if its extent divides.
//   3. LOCAL -- group output axes into ~256-thread threadgroups.
//   4. GROUP -- if the reduce axis is large, split it for a
//      threadgroup-collective reduce (KOP_GROUPTOP/KOP_GROUP).
//   5. UNROLL -- unroll the reduce axis by a small factor (4, or full
//      if extent <= 32).
//
// Each kernel_apply_opt can fail (axis out of range, factor doesn't
// divide, validation reject) -- we skip and try the next, exactly as
// tinygrad's `except KernelOptError: pass`.  After each successful
// apply the axis indices SHIFT, so we re-query the axis list every
// time.

// --- env knob ------------------------------------------------------
// Default ON would be the goal; v1 ships OFF (see header).  Memoised:
// -1 = uninitialised, 0 = off, 1 = on.
static int HAND_CODED_OPTS_ENABLED = -1;
static int hand_coded_opts_enabled(void) {
  if (HAND_CODED_OPTS_ENABLED < 0) {
    char const *e = getenv("THVM_HAND_CODED_OPTS");
    // Default OFF for v1.  THVM_HAND_CODED_OPTS=1 turns it on.
    HAND_CODED_OPTS_ENABLED = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return HAND_CODED_OPTS_ENABLED;
}

// --- axis snapshot -------------------------------------------------
// Unified view of "the kernel's current axis list", whichever backing
// store applies.  Mirrors tinygrad's k.rngs / k.full_shape / k.axis
// _types: a flat array of (kax_type, extent), indexed 0..n.  In DAG
// mode the index IS the RANGE leaf's axis_id (the lifter assigns 0..n
// in order, and kernel_apply_opt's DAG split shifts axis_ids > target
// right by one, keeping index == axis_id).  In schedule mode the index
// is the position in axes_compute_full_shape's derived list.
typedef struct {
  u32 n;
  u8  kax_type[MAX_AXES];
  u32 extent  [MAX_AXES];
} HandOptAxes;

static int hand_opt_snapshot_axes(KernelEntry const *ke, HandOptAxes *out) {
  if (ke == NULL || out == NULL) return 0;
  out->n = 0;
  if (ke->cached_lift.store_root != 0) {
    u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
    u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                                 exts, MAX_AXES);
    if (n == 0) return 0;
    // ids are sorted ascending; treat the slot index as the axis index
    // (the lifter / DAG-split keep axis_id == ordinal position).  If a
    // gap appears (shouldn't, but be defensive), bail rather than mis-
    // index a subsequent kernel_apply_opt.
    for (u32 i = 0; i < n; i++) {
      if (ids[i] != i) return 0;
      if (types[i] > KAX_GROUP_REDUCE) return 0;
      out->kax_type[i] = (u8)types[i];
      out->extent[i]   = exts[i];
    }
    out->n = n;
    return 1;
  }
  if (ke->schedule != NULL) {
    u32 exts[MAX_AXES];
    u32 n = axes_compute_full_shape(ke, exts, MAX_AXES);
    if (n == 0) return 0;
    for (u32 i = 0; i < n; i++) {
      out->kax_type[i] = axes_resolve_kax_type(ke, i);
      out->extent[i]   = exts[i];
    }
    out->n = n;
    return 1;
  }
  return 0;
}

// Index of the (last) REDUCE-class axis -- mirrors the way tinygrad
// uses k.first_reduce / k.unrollable_dims for the "reduce" axis.  We
// take the LAST KAX_REDUCE (matches propose_reduce_axis_index).
// Returns 0xFFFFFFFF if none.
static u32 hand_opt_reduce_axis(HandOptAxes const *ax) {
  for (i32 i = (i32)ax->n - 1; i >= 0; i--) {
    if (ax->kax_type[i] == KAX_REDUCE) return (u32)i;
  }
  return 0xFFFFFFFFu;
}

// Index of the innermost (last) "upcastable" output axis -- a KAX_LOOP
// or KAX_GLOBAL axis (the renderer's default-parallelise turns LOOP
// output axes into GLOBAL grid axes, so both are candidates).  We scan
// from the back, skipping the reduce/upcast/unroll/local axes.  Mirrors
// tinygrad's k.upcastable_dims[-1].  Returns 0xFFFFFFFF if none.
static u32 hand_opt_last_output_axis(HandOptAxes const *ax) {
  for (i32 i = (i32)ax->n - 1; i >= 0; i--) {
    if (ax->kax_type[i] == KAX_LOOP) return (u32)i;
  }
  return 0xFFFFFFFFu;
}

// Product of every KAX_LOOP output-axis extent (tinygrad's
// prod(output_shape[i] for i in upcastable_dims)).
static u64 hand_opt_output_loop_product(HandOptAxes const *ax) {
  u64 p = 1;
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_LOOP) p *= (u64)ax->extent[i];
  }
  return p;
}

// Product of every KAX_UPCAST extent (== floats already upcasted per
// thread; tinygrad's k.upcast_size()).
static u64 hand_opt_upcast_size(HandOptAxes const *ax) {
  u64 p = 1;
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_UPCAST) p *= (u64)ax->extent[i];
  }
  return p;
}

// --- matmul classification ----------------------------------------
// Returns 1 with K_extent in *out_K if the kernel is matmul-shaped
// f32.  Mirrors tinygrad's "kernel has exactly one reduce axis + the
// device has tensor cores + dtype matches" gate.
static int hand_opt_classify_matmul(KernelEntry const *ke, u32 *out_K) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  if (ke->output_dtype != DT_FP32) return 0;
  UopDagGemmShape gemm;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, &gemm)) {
    return 0;
  }
  if (gemm.dtype != DT_FP32) return 0;
  if (out_K != NULL) *out_K = gemm.K;
  return 1;
}

// True iff the kernel is dispatched on the Metal backend.  CPU kernels
// route matmul/gemv/dot/conv through cBLAS / the clang-JIT'd
// interpreter from the bare (un-OPT'd) DAG; applying a hand-coded opt
// mutates the DAG so those classifiers stop recognising it -> the
// kernel falls back to the slow per-element interpreter (or, for the
// BLAS-parity tests, the dispatch-count assertion fails).  So the
// heuristic only runs for Metal-backed kernels.
static int hand_opt_kernel_on_metal(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  // METAL_BACKEND.id == 2 (cpu == 1) -- see kautotune_backend_id.
  return b != NULL && b->id == 2;
}

// --- the heuristic -------------------------------------------------
// Returns the number of opts successfully applied.
fn u32 kernel_hand_coded_opts(struct KernelEntry *ke) {
  if (ke == NULL) return 0;
  // Mark "decided" up front (whether or not anything applies) so a
  // re-dispatch doesn't re-run.  Mirrors kernel_autotune's
  // ke->schedule->autotuned = 1 at the start.  schedule may be NULL
  // for some test fixtures; tolerate that.
  if (ke->schedule != NULL) ke->schedule->autotuned = 1;
  if (!hand_coded_opts_enabled()) return 0;
  if (!hand_opt_kernel_on_metal(ke)) return 0;

  u32 n_applied = 0;
  HandOptAxes ax;

  // ---- 1. tensor cores first ----
  // tinygrad: if the kernel is matmul-shaped and the device has TCs,
  // try Opt(OptOps.TC, 0, ...) -- and if it sticks, apply only the TC
  // hand-coded extras (M/N upcasts) then RETURN; the generic
  // UPCAST/LOCAL/GROUP/UNROLL catalogue does NOT run on a TC kernel.
  // thvm: KOP_TC(tile=8/16/32) when K % tile == 0.  thvm's KOP_TC is a
  // marker (uop_recognise_tc already wraps every matmul STORE in
  // OPT(_, TC, 0) inside cg_emit_via_uop -- the simdgroup_matrix
  // template fires off that); splitting M/N/K via UPCAST/UNROLL would
  // turn their extents into non-8-multiples and force the template to
  // bail to the scalar accumulator (slower).  So for v1: apply TC,
  // then RETURN.  (Follow-up: K-padding + a tiled M/N reshape, like
  // tinygrad, to unlock parallel-TC with sub-tile M/N.)
  {
    u32 K = 0;
    if (hand_opt_classify_matmul(ke, &K)) {
      static const u32 tc_tiles[] = {8, 16, 32};
      for (u32 i = 0; i < 3; i++) {
        u32 tile = tc_tiles[i];
        if (K == 0 || K % tile != 0) continue;
        KOpt opt = { KOP_TC, 0, tile };
        if (kernel_apply_opt(ke, opt)) { n_applied++; break; }
      }
      return n_applied;
    }
  }

  // ---- 2. UPCAST the innermost output axis ----
  // tinygrad: for splits in [4]: if last_axis % splits == 0:
  //   apply_opt(Opt(OptOps.UPCAST, last_axis, splits))
  // We try 4 then 2.  Cap at ~32 floats/thread (k.upcast_size() < 32).
  {
    static const u32 upcast_factors[] = {4, 2};
    for (u32 i = 0; i < 2; i++) {
      if (!hand_opt_snapshot_axes(ke, &ax)) break;
      if (hand_opt_upcast_size(&ax) >= 32) break;
      u32 axis = hand_opt_last_output_axis(&ax);
      if (axis == 0xFFFFFFFFu) break;
      u32 f = upcast_factors[i];
      if (ax.extent[axis] % f != 0 || ax.extent[axis] / f < 1) continue;
      // Heuristic gate: only upcast when there's enough work left
      // (tinygrad upcasts more only when prod(output_loop) >= 1024).
      // For the first split allow it whenever the axis divides; for
      // subsequent ones require the bigger product.
      if (i > 0 && hand_opt_output_loop_product(&ax) < 1024) break;
      KOpt opt = { KOP_UPCAST, (u8)axis, f };
      if (kernel_apply_opt(ke, opt)) { n_applied++; }
    }
  }

  // ---- 3. LOCAL: group output axes into ~256-thread threadgroups ----
  // tinygrad: for axis,sz in sorted(...): if prod(local_dims)*sz <= 256:
  //   apply_opt(Opt(OptOps.LOCAL, axis, sz))
  // We pick, for each KAX_LOOP output axis (innermost first), the
  // largest size in {256,128,64,32,16,8,4,2} that divides its extent
  // and keeps the running threadgroup size <= 256.
  {
    static const u32 local_szs[] = {256, 128, 64, 32, 16, 8, 4, 2};
    u64 running = 1;
    // Re-snapshot each iteration (LOCAL splits shift axis indices).
    for (int guard = 0; guard < MAX_AXES; guard++) {
      if (!hand_opt_snapshot_axes(ke, &ax)) break;
      // Find the innermost KAX_LOOP output axis that hasn't been
      // localised yet (a localised axis becomes KAX_LOCAL).
      u32 axis = 0xFFFFFFFFu;
      for (i32 i = (i32)ax.n - 1; i >= 0; i--) {
        if (ax.kax_type[i] == KAX_LOOP) { axis = (u32)i; break; }
      }
      if (axis == 0xFFFFFFFFu) break;
      u32 chosen = 0;
      for (u32 s = 0; s < 8; s++) {
        u32 sz = local_szs[s];
        if (ax.extent[axis] % sz != 0) continue;
        if (running * (u64)sz > 256) continue;
        chosen = sz;
        break;
      }
      if (chosen == 0) break;       // can't localise this axis usefully
      KOpt opt = { KOP_LOCAL, (u8)axis, chosen };
      if (!kernel_apply_opt(ke, opt)) break;
      n_applied++;
      running *= (u64)chosen;
      if (running >= 256) break;
      // tinygrad caps at 3 LOCAL splits.
      if (guard >= 2) break;
    }
  }

  // ---- 4. GROUP the reduce axis when it's large ----
  // tinygrad: if prod(output_shape[upcastable]) <= 2048:
  //   for axis in (0,1,2): try apply_opt(Opt(OptOps.GROUPTOP, axis, 16))
  // thvm: when the reduce axis extent >= 1024 and divides by a group
  // factor, KOP_GROUPTOP(reduce_axis, factor).  We try factors
  // {256,128,64,32,16} (largest that divides).  GROUPTOP keeps the
  // outer slice; the renderer's GROUP_REDUCE emit + the dispatch
  // shape's GROUP_REDUCE mode agree on a single-output-axis layout.
  {
    if (hand_opt_snapshot_axes(ke, &ax)) {
      u32 axis = hand_opt_reduce_axis(&ax);
      if (axis != 0xFFFFFFFFu && ax.extent[axis] >= 1024
          && hand_opt_output_loop_product(&ax) <= 2048) {
        static const u32 group_factors[] = {256, 128, 64, 32, 16};
        for (u32 i = 0; i < 5; i++) {
          u32 f = group_factors[i];
          if (ax.extent[axis] % f != 0 || f >= ax.extent[axis]) continue;
          KOpt opt = { KOP_GROUPTOP, (u8)axis, f };
          if (kernel_apply_opt(ke, opt)) { n_applied++; break; }
        }
      }
    }
  }

  // ---- 5. UNROLL the reduce axis a bit ----
  // tinygrad applies Opt(OptOps.UNROLL, reduce_axis, factor) here.
  // thvm SKIPS this in v1: the renderer (render_uop.c) already emits
  // `#pragma unroll(K)` over small reduce axes by default (commit
  // dd50948e), so an explicit KOP_UNROLL split is redundant -- and a
  // *full* unroll (factor == extent -> outer extent 1) currently
  // confuses rmu_emit_store_reduce's reduce-range matcher (the REDUCE
  // node's axis field no longer names any live RANGE), producing a
  // bare `#pragma unroll` with no following loop -> Metal compile
  // failure.  Re-enable once the renderer's UNROLL-split handling is
  // hardened.  (No-op here, kept for the explicit "ported but skipped"
  // record.)

  return n_applied;
}

// Should this kernel get the hand-coded heuristic on its next fire?
// Mirrors kernel_should_autotune's cheap pre-check: the env opt-in is
// on AND the per-shape autotuned flag is still 0.  We don't pre-run
// the heuristic to check "would it apply at least one opt" the way
// kernel_should_autotune calls kernel_opts_propose -- the heuristic is
// itself the cheap analysis, so we just run it (it sets the flag
// regardless, so re-dispatch is a no-op).
fn int kernel_should_hand_code_opts(struct KernelEntry const *ke) {
  if (!hand_coded_opts_enabled()) return 0;
  if (ke == NULL) return 0;
  // schedule may be NULL on some fixtures -- if so we can't memoise
  // the decision, but we still want to run the heuristic once.  Use
  // the autotuned flag when present; otherwise allow.
  if (ke->schedule != NULL && ke->schedule->autotuned) return 0;
  return 1;
}
