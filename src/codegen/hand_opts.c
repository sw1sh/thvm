// codegen/hand_opts.c -- port of tinygrad's hand_coded_optimizations
// (tinygrad/codegen/opt/heuristic.py) into thvm.
//
// Given a finalized kernel (post-rangeify, scalar arena -> lifted UOp
// DAG in `cached_lift.store_root`, or a schedule-path fixture), inspect
// its shape/dtype and apply a sensible sequence of KOpts via the
// existing `kernel_apply_opt`.  This is the cheap-by-default analogue
// of tinygrad's hand-coded catalogue (no benchmarking, unlike BEAM
// autotune, which stays gated on AUTOTUNE).  It is wired into
// kernel_fire_by_id and runs before dispatch -- but see "DEFAULT
// STATE" below: the HAND_CODED_OPTS knob defaults to opts-ON.  When
// disabled the function is a no-op (it still marks
// ke->schedule->autotuned so re-dispatch is cheap).
//
// === DEFAULT STATE: ON (v2) ========================================
// HAND_CODED_OPTS defaults to ON.  HAND_CODED_OPTS=0 or NOOPT=1
// disables.  Earlier (v1) it was OFF because the DAG-mode renderer
// gated its default-parallelise pass off the moment ANY per-axis OPT
// wrapper was present, so a UPCAST on a multi-output-axis kernel
// turned the remaining output axes into serial for-loops with no
// bounds guard while the dispatch still launched one-thread-per-
// output-element.  v2 fixed both sides:
//   - render_uop.c: the default-parallelise pass now promotes each
//     plain-KAX_LOOP output axis individually (the OUTER half of a
//     UPCAST split), so it COMPOSES with the OPT'd inner axis (which
//     emits via its `#pragma unroll` for-loop path).
//   - render_metal.c: cg_tile_metal_dispatch_shape derives
//     (groups, threads) from the lifted DAG's RANGE-leaf axis types
//     for DAG-mode kernels with per-axis OPTs (rmt_dag_dispatch_shape),
//     so the launched grid covers the renderer's `tid` decode exactly.
//   - apply_opt_dag.c: an axis split now also shifts the UOP_REDUCE
//     node's axis field, so the renderer's reduce-range matcher
//     keeps finding it (otherwise it mistook the UPCAST'd inner axis
//     for the reduce loop -> "undeclared identifier").
// v3 wires multi-axis LOCAL: render_uop.c's RmuGlobalDecode now carries
// a per-LOCAL (stride, modulus) `tt`-decode mirroring its multi-GLOBAL
// `tg`-decode, and rmt_dag_dispatch_shape's `threads = prod(LOCAL
// extents)` already supported any axis count.  GROUP is still SKIPPED.
//
// === Heuristic cases ported (mirrors hand_coded_optimizations) ====
//   1. TC -- matmul-shaped + f32 + K % tile == 0 -> KOP_TC(tile=8).
//      (uop_recognise_tc already wraps every matmul STORE in
//      OPT(_, TC, 0) inside cg_emit_via_uop, so this is mostly a
//      re-affirm; kept for the explicit `applied_opts` record on the
//      schedule path + future parallel-TC dispatch work.)
//   2. UPCAST -- for a TILEABLE reduce kernel (OPT_CONV-marked conv-
//      matmul kernel with a multi-axis output + a reduce nest -- the
//      BS=512 beautiful_mnist conv kernels, which lower through
//      render_uop.c's rmu_emit_conv template), run tinygrad's UPCAST
//      loop: while output_loop_product >= 1024 and upcast_size < 16,
//      split the INNERMOST KAX_LOOP axis that divides 4 (then 2) --
//      repeating naturally spreads the upcast across axes.  Plus the
//      "if nothing upcasted, do one easy UPCAST 4" fallback.  For a
//      non-tileable kernel keep the prior conservative one-axis pass
//      (4 then 2 on the last axis) -- the generic accumulator path
//      that maxpool / BatchNorm reductions lower through has only been
//      validated on a one-UPCAST-one-LOCAL output.
//   3. LOCAL -- for a tileable conv kernel, tinygrad's "local groups"
//      pass: scan KAX_LOOP axes innermost->outermost, split each by
//      the largest factor in {32,16,8,4,3,2} that divides and keeps
//      prod(LOCAL extents) <= 256, up to 3 LOCAL axes (the renderer's
//      multi-LOCAL `tt`-decode -- piece 1 -- handles >=2).  For a
//      non-tileable kernel keep the prior single-LOCAL split.
//   4. GROUP -- SKIPPED for v3: the GROUP_REDUCE renderer + dispatch
//      readers were validated only on the un-UPCAST'd reduce-tail
//      shape; re-enable once cross-checked against a UPCAST'd output.
//   5. UNROLL -- skipped (renderer already `#pragma unroll`s small
//      reduce axes by default; an explicit full UNROLL still confuses
//      the reduce-range matcher).
//
// Each kernel_apply_opt can fail (axis out of range, factor doesn't
// divide, validation reject) -- we skip and try the next, exactly as
// tinygrad's `except KernelOptError: pass`.  After each successful
// apply the axis indices SHIFT, so we re-query the axis list every
// time.

// --- env knob ------------------------------------------------------
// Default ON (v2; see header).  HAND_CODED_OPTS=0 disables; NOOPT=1
// (tinygrad's inverse-sense knob) also disables and wins if both set.
// Memoised: -1 = uninitialised, 0 = off, 1 = on.
static int HAND_CODED_OPTS_ENABLED = -1;

// Side-channel "hand_opts has run for this kid" memo.  ke->schedule
// is NULL on Python-built kernels (the CUDA bridge route) so we can't
// stash on ke->schedule->autotuned alone -- without this memo, hand_
// opts re-ran every fire on those kernels (~7ms overhead * 29 fires/
// step regressed wall ~200ms even when the heuristic was a no-op).
// One byte per kid, indexed (kid - KERNELS); KERNELS_CAP-bounded.
static u8 HAND_OPTS_RAN[KERNELS_CAP] = {0};
static int hand_coded_opts_enabled(void) {
  if (HAND_CODED_OPTS_ENABLED < 0) {
    char const *noopt = getenv("NOOPT");
    if (noopt != NULL && noopt[0] != '\0' && noopt[0] != '0') {
      HAND_CODED_OPTS_ENABLED = 0;            // tinygrad NOOPT=1
    } else {
      char const *e = getenv("HAND_CODED_OPTS");
      // Default ON.  Only an explicit "0" disables.
      HAND_CODED_OPTS_ENABLED = (e != NULL && e[0] == '0') ? 0 : 1;
    }
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
  u32 axis_id [MAX_AXES];   // actual axis id (may not equal slot
                             // index -- kernels lifted from production
                             // pipeline carry non-zero-based ids).
} HandOptAxes;

static int hand_opt_snapshot_axes(KernelEntry const *ke, HandOptAxes *out) {
  if (ke == NULL || out == NULL) return 0;
  out->n = 0;
  if (ke->cached_lift.store_root != 0) {
    u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
    u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                                 exts, MAX_AXES);
    if (n == 0) return 0;
    // ids are sorted ascending but NOT necessarily zero-based -- a
    // kernel lifted from the production pipeline (rangeify -> kernel
    // -> lift) carries global axis ids that may start anywhere in
    // 0..u32_max.  The slot index is the ordinal position; the
    // axis_id is stored separately and passed to kernel_apply_opt as
    // opt.axis, since uop_dag_apply_kopt targets by axis_id (not slot).
    for (u32 i = 0; i < n; i++) {
      if (types[i] > KAX_GROUP_REDUCE) return 0;
      out->kax_type[i] = (u8)types[i];
      out->extent[i]   = exts[i];
      out->axis_id[i]  = ids[i];
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
      // Schedule mode: legacy applied_opts uses slot index as axis,
      // see apply_opt.c.  Keep the existing semantics.
      out->axis_id[i]  = i;
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

// Same as above but skip axes with extent 1 (degenerate -- already
// fully consumed by a prior split, can't be split further).  Used by
// the multi-LOCAL pass to find the next-LOOP axis that's still
// non-trivial.
static u32 hand_opt_last_nontriv_output_axis(HandOptAxes const *ax) {
  for (i32 i = (i32)ax->n - 1; i >= 0; i--) {
    if (ax->kax_type[i] == KAX_LOOP && ax->extent[i] > 1) return (u32)i;
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

// Product of every KAX_LOCAL extent (== threadgroup size; tinygrad's
// prod(local_dims)).  1 if no LOCAL axes yet.
static u64 hand_opt_local_size(HandOptAxes const *ax) {
  u64 p = 1;
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_LOCAL) p *= (u64)ax->extent[i];
  }
  return p;
}

// Count of KAX_LOCAL axes (tinygrad caps local-split to <= 3 axes).
static u32 hand_opt_n_local_axes(HandOptAxes const *ax) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) if (ax->kax_type[i] == KAX_LOCAL) n++;
  return n;
}

// Count of KAX_LOOP output axes.
static u32 hand_opt_n_loop_axes(HandOptAxes const *ax) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) if (ax->kax_type[i] == KAX_LOOP) n++;
  return n;
}

// True iff the lifted DAG's STORE value is OPT(_, CONV, _) -- the
// im2col `_pool` conv-matmul marker installed by uop_recognise_conv.
// These kernels lower through render_uop.c's rmu_emit_conv template,
// which handles a multi-UPCAST + multi-LOCAL output tiling correctly
// (the (cin,kh,kw) reduce nest is recovered + nested, the output axes
// promoted/threadbound).  Other reduce kernels (maxpool, BatchNorm
// reductions) lower through the generic accumulator path, which the
// surgical suite has only validated on a one-UPCAST-one-LOCAL output;
// deeper tiling there can produce an axis order the generic path
// emits out of dependency order ("use of undeclared identifier") and
// the kernel falls to the slow per-element interpreter.  So the deep
// conv-style tiling (case 2/3's "tileable" arms) is gated on this.
static int hand_opt_is_conv_kernel(KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  Term sroot = ke->cached_lift.store_root;
  if (term_tag(sroot) != TAG_UOP || term_ext(sroot) != UOP_STORE) return 0;
  Term value = heap_read(term_val(sroot) + 2);
  // Already wrapped (rare -- recogniser usually runs at render time)?
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
      && uop_opt_kind(value) == UOP_OPT_CONV) return 1;
  // Otherwise detect the conv2d SHAPE structurally -- includes both
  // the flat im2col-matmul form (single REDUCE axis + IDIV/IMOD
  // addresses, lowers through rmu_emit_conv) AND the direct multi-
  // axis form (separate Cin/kH/kW reduce axes + linear addresses,
  // lowers through the generic multi-axis accumulator path).  Both
  // benefit from LOCAL/UPCAST tiling.
  return uop_classify_conv2d_any(sroot, NULL);
}

// True iff this is a conv-matmul kernel we want to tile deeply: the
// OPT_CONV marker is present AND the output is multi-axis (>=2 KAX_LOOP)
// AND there's at least one reduce axis -- the shape the BS=512
// beautiful_mnist conv-matmul kernels take (output {B, cOut, hOut, wOut}
// or flattened {cOut, B*hOut*wOut}, reduce nest (cin,kh,kw)).  Everything
// else gets the conservative one-UPCAST-one-LOCAL treatment.
static int hand_opt_is_tileable_reduce(KernelEntry const *ke,
                                       HandOptAxes const *ax) {
  if (!hand_opt_is_conv_kernel(ke)) return 0;
  u32 r = hand_opt_reduce_axis(ax);
  if (r == 0xFFFFFFFFu) return 0;
  if (hand_opt_n_loop_axes(ax) < 2) return 0;
  // Deep tiling (case-2 UPCAST + case-3 LOCAL) was validated against
  // the OPT_CONV-wrapped flat conv form that lowers through
  // render_uop.c's rmu_emit_conv template (single-axis K, decomposed
  // via the conv-split recovery into Cin*kH*kW nested loops with
  // straight-line UPCAST-blocked accumulators).  The direct multi-
  // axis form (separate Cin/kH/kW reduce axes) lowers through the
  // GENERIC rmu_emit_store_reduce path, which does NOT correctly
  // handle UPCAST'd output axes inside a multi-axis REDUCE body
  // (the renderer emits the output decode + closes the loops, but
  // the reduce body is dropped -- see the THVM_CUDA_DUMP_KID=3 dump
  // when UPCAST is applied: empty reduce, _acc=0 stored).  So gate
  // the deep-tile path on the flat form only; the direct form falls
  // through to the conservative one-LOCAL pass below.
  u32 kred_flat = 0;
  if (!uop_classify_conv2d(ke->cached_lift.store_root, &kred_flat)) return 0;
  if (ax->extent[r] < 128) return 0;
  u32 c_axis = hand_opt_last_output_axis(ax);
  if (c_axis == 0xFFFFFFFFu || ax->extent[c_axis] < 4) return 0;
  u32 first = 0xFFFFFFFFu;
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_LOOP) { first = i; break; }
  }
  if (first == 0xFFFFFFFFu || ax->extent[first] < 4) return 0;
  return 1;
}

// True iff this kernel is a direct-multi-axis conv (the CUDA kid=3
// shape from beautiful_mnist BS>=128).  These lower through the
// GENERIC reduce emit path, not rmu_emit_conv, so we apply only the
// safe single-LOCAL split (no UPCAST -- see hand_opt_is_tileable_reduce
// comment for why).  Returns 1 iff (a) classifies as direct conv AND
// (b) K product is large enough (>= 128) to amortise the LOCAL split.
static int hand_opt_is_direct_conv(KernelEntry const *ke,
                                   HandOptAxes const *ax) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u32 kred_direct = 0;
  if (!uop_classify_conv2d_direct(ke->cached_lift.store_root, &kred_direct)) return 0;
  if (kred_direct < 128) return 0;
  // Need at least 2 KAX_LOOP output axes to have a meaningful LOCAL split.
  if (hand_opt_n_loop_axes(ax) < 2) return 0;
  return 1;
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

// True iff the kernel is dispatched on a GPU backend (Metal == id 2
// or CUDA == id 3).  CPU kernels route matmul/gemv/dot/conv through
// cBLAS / the clang-JIT'd interpreter from the bare (un-OPT'd) DAG;
// applying a hand-coded opt mutates the DAG so those classifiers stop
// recognising it -> the kernel falls back to the slow per-element
// interpreter (or, for the BLAS-parity tests, the dispatch-count
// assertion fails).  So the heuristic only runs on GPU backends; the
// per-backend factor tables in kernel_hand_coded_opts branch on
// ke->output_tid backend id to pick the right tiling.
static int hand_opt_kernel_on_gpu(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  if (b == NULL) return 0;
  return b->id == 2 || b->id == 3;
}

// Returns the backend id for the kernel's output tensor (1 == cpu,
// 2 == metal, 3 == cuda).  0 if no backend resolves.  Mirrors
// kautotune_backend_id; used here to pick per-backend factor tables.
static u32 hand_opt_backend_id(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  return (b != NULL) ? b->id : 0;
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
  {
    u32 kid = (u32)(ke - KERNELS);
    if (kid < KERNELS_CAP) HAND_OPTS_RAN[kid] = 1;
  }
  if (!hand_coded_opts_enabled()) return 0;
  if (!hand_opt_kernel_on_gpu(ke)) return 0;
  u32 backend_id = hand_opt_backend_id(ke);     // 2 == metal, 3 == cuda

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
  // TC is currently a Metal-only optimization in hand_opts: the
  // parallel-TC GLOBAL promotion was tuned against the simdgroup_matrix
  // template + Metal threadgroup dispatch (cg_tile_metal_dispatch_shape).
  // CUDA has a WMMA path in the renderer but the parallel-TC dispatch
  // geometry hasn't been cross-validated -- skip TC on CUDA for now;
  // the existing flat one-thread-per-output dispatch is fast enough on
  // V100 for the matmul shapes thvm currently emits (no matmul shows
  // up as a top hot kid on beautiful_mnist).
  if (backend_id == 2) {
    u32 K = 0;
    if (hand_opt_classify_matmul(ke, &K)) {
      static const u32 tc_tiles[] = {8, 16, 32};
      for (u32 i = 0; i < 3; i++) {
        u32 tile = tc_tiles[i];
        if (K == 0 || K % tile != 0) continue;
        KOpt opt = { KOP_TC, 0, tile };
        if (kernel_apply_opt(ke, opt)) { n_applied++; break; }
      }
      // Parallel TC: promote the matmul's output (M/N) axes from LOOP to
      // GLOBAL so the renderer's simdgroup_matrix template binds one
      // simdgroup per 8x8 output tile across the grid, instead of running
      // every tile serially in a single guarded simdgroup (~75x slower --
      // see docs/perf_cross_backend.md).  An output axis qualifies when
      // its extent is a multiple of 8 (the simdgroup_matrix<8,8> tile);
      // the K reduce axis is KAX_REDUCE so it is never touched, and a
      // non-8-multiple output axis stays a LOOP (the template keeps it as
      // an in-kernel serial loop, still correct).  Only meaningful on
      // Metal; the dispatch-shape reader (cg_tile_metal_dispatch_shape)
      // sizes grid = product(extent/8) threadgroups x 32 threads.
      if (n_applied > 0) {
        u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
        u32 na = uop_dag_collect_axes(ke->cached_lift.store_root, ids,
                                      types, exts, MAX_AXES);
        for (u32 i = 0; i < na; i++) {
          if (types[i] == KAX_LOOP && exts[i] != 0 && (exts[i] % 8u) == 0) {
            KOpt g = { KOP_GLOBAL, ids[i], exts[i] };
            if (kernel_apply_opt(ke, g)) n_applied++;
          }
        }
      }
      return n_applied;
    }
  }

  if (!hand_opt_snapshot_axes(ke, &ax)) return n_applied;
  int tileable    = hand_opt_is_tileable_reduce(ke, &ax);
  int direct_conv = !tileable && hand_opt_is_direct_conv(ke, &ax);
  // CUDA: only run opts on the conv-shaped kernels (flat + direct).
  // The Metal-tuned UPCAST+LOCAL factor table regresses non-conv
  // kernels on V100 -- the existing flat one-thread-per-output
  // dispatch is faster than the conservative one-axis tile.  Re-
  // enable for non-conv kernels once a CUDA-specific factor table
  // for the non-conv path is added.
  if (backend_id == 3 && !tileable && !direct_conv) return n_applied;

  // ---- 2. UPCAST: pull floats into per-thread registers ----
  // tinygrad's "potentially do more upcasts" loop (heuristic.py
  //   while prod(output_loop_dims) >= 1024 and upcast_size() < 32:
  //     pick an upcastable axis, UPCAST by 3 or 4)
  // plus its "if nothing upcasted and easy to, do one UPCAST 4" fallback
  // and the trailing "if not k.upcasted ... UPCAST last by 4".
  //
  // For a TILEABLE reduce kernel ({B,cOut,hOut,wOut}-shaped output with
  // a (cin,kh,kw) reduce nest -- the BS=512 beautiful_mnist conv kernels)
  // we run the loop: each pass picks the INNERMOST KAX_LOOP axis whose
  // extent divides 4 (then 2), splits its inner half off as KAX_UPCAST.
  // Repeating naturally SPREADS the upcast across axes (once an axis's
  // outer half no longer divides, the scan moves to the next one).  Cap
  // upcast_size at ~16 floats/thread (each step at most x4, conservative
  // side of tinygrad's <32 -- the conv2 reference kernel does 16).
  // For NON-tileable kernels keep the prior conservative one-axis pass
  // (try 4 then 2 on the last output axis), unchanged.
  if (tileable) {
    // Two-axis register-blocking: UPCAST on the M axis (output axis 0 --
    // the conv-matmul "M" / cOut output axis: the weight read depends on
    // it, the conv-input read does not) AND on the contiguous spatial axis
    // (the LAST output axis -- wOut: the conv-input read depends on it,
    // the weight read does not).  rmu_emit_conv now emits Um*Uw straight-
    // line accumulators inside the reduce nest; the MSL compiler CSEs the
    // weight load (cOut-only) across Uw outputs and the conv-input load
    // (wOut-only) across Um outputs -- the classic 2D register-blocked
    // matmul inner, with Um+Uw loads sustaining Um*Uw MADs per iter.
    if (hand_opt_snapshot_axes(ke, &ax)) {
      u32 m_axis = 0xFFFFFFFFu;
      for (u32 i = 0; i < ax.n; i++) {
        if (ax.kax_type[i] == KAX_LOOP) { m_axis = i; break; }
      }
      if (m_axis != 0xFFFFFFFFu) {
        u32 ext = ax.extent[m_axis];
        static const u32 m_factors[] = {8, 4, 2};
        for (u32 i = 0; i < 3; i++) {
          u32 f = m_factors[i];
          if (ext % f != 0 || ext / f < 1) continue;
          KOpt opt = { KOP_UPCAST, (u8)ax.axis_id[m_axis], f };
          if (kernel_apply_opt(ke, opt)) n_applied++;
          break;
        }
      }
    }
    // Second UPCAST on the contiguous spatial axis (wOut) -- the 2D
    // register-block inner.  Only fires when (a) the M UPCAST product
    // is modest (<=4) so total live registers stay reasonable, AND (b)
    // wOut OUTER / 2 >= 32 -- a FULL simdgroup width of threadgroup
    // lanes is left for the LOCAL pass even after the inner-2 split.
    // beautiful_mnist's wOut maxes out at 20, so this is dormant on
    // that workload; large-spatial convs (wOut >= 64) do exercise it.
    if (hand_opt_snapshot_axes(ke, &ax)
        && hand_opt_upcast_size(&ax) <= 4) {
      u32 c_axis = hand_opt_last_output_axis(&ax);
      if (c_axis != 0xFFFFFFFFu) {
        u32 ext = ax.extent[c_axis];
        u32 f = 2;
        if (ext % f == 0 && ext / f >= 32) {
          KOpt opt = { KOP_UPCAST, (u8)ax.axis_id[c_axis], f };
          if (kernel_apply_opt(ke, opt)) n_applied++;
        }
      }
    }
  } else if (direct_conv) {
    // Direct-multi-axis conv lowers through the generic reduce emit
    // path which doesn't currently cope with UPCAST'd output axes
    // (drops the reduce body, _acc=0 stored).  Skip UPCAST here; just
    // the LOCAL split below.  Re-enable when the generic emit path
    // is hardened against UPCAST + multi-axis REDUCE.
  } else {
    static const u32 upcast_factors[] = {4, 2};
    for (u32 i = 0; i < 2; i++) {
      if (!hand_opt_snapshot_axes(ke, &ax)) break;
      if (hand_opt_upcast_size(&ax) >= 32) break;
      u32 axis = hand_opt_last_output_axis(&ax);
      if (axis == 0xFFFFFFFFu) break;
      u32 f = upcast_factors[i];
      if (ax.extent[axis] % f != 0 || ax.extent[axis] / f < 1) continue;
      if (i > 0 && hand_opt_output_loop_product(&ax) < 1024) break;
      KOpt opt = { KOP_UPCAST, (u8)ax.axis_id[axis], f };
      if (kernel_apply_opt(ke, opt)) { n_applied++; }
    }
  }

  // ---- 3. LOCAL: fill the threadgroup ----
  // tinygrad's "local groups" pass (heuristic.py): rank the
  // GLOBAL/LOOP-class axes, then for each pick the largest local_sz in
  //   ([32] if axis==0) + [16,8,4,3,2]
  // with full_shape % local_sz == 0 and (running local product) *
  // local_sz <= 128, apply LOCAL on up to 3 axes.  We mirror that:
  // multi-axis LOCAL is now wired in the renderer (RmuGlobalDecode's
  // per-LOCAL (stride,modulus) tt-decode -- piece 1) and
  // rmt_dag_dispatch_shape's threads = prod(LOCAL extents).  Cap the
  // threadgroup product at 256 (Apple's maxTotalThreadsPerThreadgroup
  // is 1024; 256 matches tinygrad-on-Metal and keeps occupancy high).
  //
  // We scan KAX_LOOP axes from innermost to outermost (matches the
  // renderer's emission/decode order); after each split the outer half
  // stays KAX_LOOP (-> promoted GLOBAL grid axis) and the inner half
  // becomes KAX_LOCAL.  Re-snapshot every pass (axis indices shift).
  // For NON-tileable kernels: keep the prior single-LOCAL behaviour
  // (one split, largest factor up to 256 on the last output axis).
  {
    u32 local_cap = 256u;
    if (tileable) {
      // LOCAL on the contiguous spatial axis (wOut -- LAST output axis,
      // innermost stride 1 in the conv-input read): consecutive threads
      // land on consecutive conv-input columns (coalesced).  Prefer a
      // multiple of the simdgroup width (32) when possible.  Cap product
      // at local_cap (== 256).
      static const u32 conv_local_factors[] = {32, 64, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2};
      u32 cur_local_prod = 1;
      if (hand_opt_snapshot_axes(ke, &ax)) {
        u32 c_axis = hand_opt_last_output_axis(&ax);
        if (c_axis != 0xFFFFFFFFu) {
          u32 ext = ax.extent[c_axis];
          for (u32 i = 0; i < 12; i++) {
            u32 f = conv_local_factors[i];
            if (f > (u32)local_cap) continue;
            if (ext % f != 0 || ext / f < 1) continue;
            KOpt opt = { KOP_LOCAL, (u8)ax.axis_id[c_axis], f };
            if (kernel_apply_opt(ke, opt)) { n_applied++; cur_local_prod = f; }
            break;
          }
        }
      }
      // Second LOCAL on the next non-degenerate output axis (hOut) when
      // the wOut LOCAL was below a full simdgroup width (32 threads).
      // This happens on the small-spatial late convs (wOut 6, 8) --
      // without a second LOCAL the threadgroup has fewer than a warp's
      // worth of lanes and the kernel under-utilises the GPU.  hOut
      // LOCAL lands consecutive simdgroups on consecutive hOut rows
      // (still coalesced for the conv-input read which is row-major in
      // (hOut, wOut)).  Skip ext=1 axes (the wOut outer half post-split
      // is degenerate when LOCAL took the full wOut extent).
      if (cur_local_prod > 0 && cur_local_prod < 32
          && hand_opt_snapshot_axes(ke, &ax)) {
        u32 h_axis = hand_opt_last_nontriv_output_axis(&ax);
        if (h_axis != 0xFFFFFFFFu) {
          u32 ext = ax.extent[h_axis];
          for (u32 i = 0; i < 12; i++) {
            u32 f = conv_local_factors[i];
            if ((u64)cur_local_prod * f > (u64)local_cap) continue;
            if (ext % f != 0 || ext / f < 1) continue;
            KOpt opt = { KOP_LOCAL, (u8)ax.axis_id[h_axis], f };
            if (kernel_apply_opt(ke, opt)) { n_applied++; cur_local_prod *= f; }
            break;
          }
        }
      }
    } else if (direct_conv) {
      // Direct-multi-axis conv on CUDA: LOCAL/UPCAST alone (without
      // UPCAST register-blocking or shared-mem GROUP_REDUCE cooperative
      // load) costs MORE than the flat one-thread-per-output dispatch on
      // V100 -- the renderer adds inner-loop arithmetic (a*F+b axis
      // decode) for each LOCAL split without compensating throughput.
      // The renderer's multi-axis-REDUCE UPCAST path drops the reduce
      // body (rendered source has _acc=0 stored, no MAC loop), and the
      // GROUP_REDUCE path (which WOULD emit __shared__ + per-block
      // cooperative weight load) is gated off in hand_opts v3 pending
      // UPCAST cross-check.  Until one of those is unblocked, applying
      // LOCAL alone here regresses kid=3 / kid=12 by 2x.  Treat as a
      // no-op: classifier still flags the kernels so future work has
      // something to gate on.  Re-enable LOCAL when UPCAST + multi-
      // axis-REDUCE in the generic emit path is fixed.
      (void)direct_conv;
    } else {
      static const u32 single_local_factors[] = {256, 128, 64, 32, 16, 8, 4, 2};
      if (hand_opt_snapshot_axes(ke, &ax)) {
        u32 axis = hand_opt_last_output_axis(&ax);
        if (axis != 0xFFFFFFFFu) {
          u32 ext = ax.extent[axis];
          for (u32 i = 0; i < 8; i++) {
            u32 f = single_local_factors[i];
            if (f > 256) continue;
            if (ext % f != 0 || ext / f < 1) continue;
            KOpt opt = { KOP_LOCAL, (u8)ax.axis_id[axis], f };
            if (kernel_apply_opt(ke, opt)) { n_applied++; }
            break;  // one LOCAL split only
          }
        }
      }
    }
  }

  // ---- 4. GROUP the reduce axis: SKIPPED in v2 ----
  // KOP_GROUPTOP / KOP_GROUP split the reduce axis for a threadgroup-
  // collective reduce.  The GROUP_REDUCE renderer + GROUP_REDUCE
  // dispatch mode were validated on the un-UPCAST'd reduce-tail shape
  // only; combined with case 2's output UPCAST they haven't been
  // cross-checked.  Re-enable once a UPCAST+GROUP fixture is in the
  // surgical suite.

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
  if (ke->schedule != NULL && ke->schedule->autotuned) return 0;
  u32 kid = (u32)(ke - KERNELS);
  if (kid < KERNELS_CAP && HAND_OPTS_RAN[kid]) return 0;
  return 1;
}
