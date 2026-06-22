// codegen/propose.c -- shape-heuristic kernel opt proposer.
//
// Given a finalized KernelEntry (post-default-axes), suggest a small
// set of candidate TOpts the autotune loop should try.  Today's
// heuristics are deliberately narrow:
//
//   reduce-tail kernel + axis_size % factor == 0 -> propose UNROLL
//   factor for factor in {2, 4, 8, 16}.
//
//   DEV=metal + THVM_TILE=1 + rank-1 f32 scalar/tile kernel
//   -> propose LOCAL tile factors.  The autotune loop applies the
//   matching outer GLOBAL mark when benchmarking these candidates.
//
//   DEV=metal + f32 GEMM kernel
//   -> propose TC tile sizes.  The first implementation uses TC as
//   metadata for the fixed direct Metal GEMM renderer; later it maps
//   to simdgroup MMA variants.
//
//   DEV=metal + THVM_TILE=1 + im2col-fused Conv2D template
//   -> propose LOCAL threadgroup-size factors over a loop axis,
//   UPCAST output-per-thread factors, and UNROLL factors over the
//   reduce axis.  The generated tile renderer reads those as SIMT
//   group width, outputs per thread, and reduction unroll count.
//
// As more opt classes get codegen support (UPCAST output axes,
// LOCAL/GLOBAL Metal bindings, GROUP_REDUCE, etc.) they slot in
// here as additional rules.  The output is a flat list of KOpt;
// the autotune loop first applies each one against the baseline and
// can then expand the best variants into short opt sequences.

// Walk the lifted UOp DAG for the reduce axis extent when
// cached_lift.store_root is populated (the autotune-time path
// pays the lift cost at materialize-time).
static u32 propose_reduce_axis_size_from_dag(KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  return uop_dag_reduce_axis_extent(ke->cached_lift.store_root);
}

// TC tile-size proposer counter.  The only gate is the DAG
// classifier; counter exposes coverage to the surgical suite.
static u64 PROPOSE_TC_DAG = 0;

fn u64 kernel_opts_propose_tc_dag_count(void) {
  return PROPOSE_TC_DAG;
}
fn void kernel_opts_propose_tc_counters_reset(void) {
  PROPOSE_TC_DAG = 0;
}

static u32 propose_append_unique(KOpt *out, u32 n, u32 cap, KOpt cand);

// Matmul-shape + dtype gate for the TC tile-size proposer.  Reads
// uop_dag_classify_matmul_shape over ke->cached_lift.store_root.  When
// out_shape is non-NULL it also returns the full {M,N,K,dtype} so the
// proposer can enumerate valid RmuTcTile candidates for the search.
static int propose_tc_classify(KernelEntry const *ke, u32 *out_dtype,
                               UopDagGemmShape *out_shape) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  UopDagGemmShape shape;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke,
                                     &shape)) {
    return 0;
  }
  if (out_dtype != NULL) *out_dtype = shape.dtype;
  if (out_shape != NULL) *out_shape = shape;
  PROPOSE_TC_DAG++;
  return 1;
}

// Enumerate VALID simdgroup_matrix tile configs for an (M,N,K) matmul and
// emit them as packed KOP_TC candidates (tc_tile_pack -> the opt arg the
// emitter unpacks).  Only tiles that pass tc_tile_valid -- the SAME
// divisibility/threadgroup-memory/occupancy check the emitter applies --
// are offered, so the autotune never benches an invalid kernel.  Ordered
// largest-tile-first (best A/B fragment reuse) so the cap keeps the most
// promising candidates.  This is the matmul analogue of tinygrad's
// TC+UPCAST+LOCAL action space: the tile config IS the matmul's
// UPCAST/LOCAL, carried inside KOP_TC (never as an independent split,
// which corrupts the simdgroup emitter).
static u32 propose_tc_tile_candidates(u32 M, u32 N, u32 K, int c_is_bf,
                                      u32 unit, int is_cuda,
                                      KOpt *out, u32 n, u32 cap) {
  // local_m/local_n: simdgroups along M/N.  rm/rn: register 8x8 tiles per
  // simdgroup.  kb: K-block staged in threadgroup memory (multiple of 8).
  // Full action space (the tile config IS the matmul's UPCAST/LOCAL); the
  // cold-search cost is bounded NOT by curating this set but by the per-kernel
  // search timeout + per-candidate early-stop in kernel_autotune (mirrors
  // tinygrad's BEAM_TIMEOUT_SEC + _time_program early_stop=best*3).
  static const u32 locals[]  = {4, 2, 1};
  static const u32 regs[]    = {8, 4, 2, 1};
  static const u32 kbs[]     = {64, 32, 16, 8};
  for (u32 li = 0; li < sizeof(locals)/sizeof(*locals) && n < cap; li++) {
    for (u32 lj = 0; lj < sizeof(locals)/sizeof(*locals) && n < cap; lj++) {
      for (u32 ri = 0; ri < sizeof(regs)/sizeof(*regs) && n < cap; ri++) {
        for (u32 rj = 0; rj < sizeof(regs)/sizeof(*regs) && n < cap; rj++) {
          for (u32 ki = 0; ki < sizeof(kbs)/sizeof(*kbs) && n < cap; ki++) {
            u32 lm = locals[li], ln = locals[lj];
            u32 rm = regs[ri],   rn = regs[rj];
            u32 kb = kbs[ki];
            if (!tc_tile_valid(M, N, K, lm, ln, rm, rn, kb,
                               unit, is_cuda, c_is_bf)) {
              continue;
            }
            n = propose_append_unique(out, n, cap,
                  (KOpt){ KOP_TC, 0, tc_tile_pack(lm, ln, rm, rn, kb) });
          }
        }
      }
    }
  }
  return n;
}

static u32 propose_reduce_axis_size(KernelEntry const *ke) {
  return propose_reduce_axis_size_from_dag(ke);
}

// Index of the reduce axis -- the last axis of type KAX_REDUCE.
// Returns 0xFF if none (caller checks `< n_axes`).
// Reads through tile_anno_axis_count_or_kernelaxes / tile_anno_axis,
// which derive axis info from cached_lift.store_root.
static u8 propose_reduce_axis_index(KernelEntry const *ke) {
  u32 n = tile_anno_axis_count_or_kernelaxes(ke);
  if (n == 0) return 0xFF;
  for (i32 i = (i32)n - 1; i >= 0; i--) {
    TileAxisInfo info;
    if (tile_anno_axis_or_kernelaxes(ke, (u32)i, &info)
        && info.kax_type == KAX_REDUCE) {
      return (u8)i;
    }
  }
  return 0xFF;
}

// First LOOP-typed axis at or after `start` that can be split by
// `factor`, or 0xFF if none.  Leading unit/broadcast axes are common
// in movement-heavy graphs; picking axis 0 unconditionally makes those
// kernels look untunable even when an inner axis has plenty of work.
static u8 propose_loop_axis_for_factor(KernelEntry const *ke, u8 start,
                                       u32 factor) {
  u32 n = tile_anno_axis_count_or_kernelaxes(ke);
  for (u8 i = start; i < n; i++) {
    TileAxisInfo info;
    if (!tile_anno_axis_or_kernelaxes(ke, i, &info)) continue;
    if (info.kax_type != KAX_LOOP) continue;
    if (factor <= info.extent && info.extent % factor == 0) {
      return i;
    }
  }
  return 0xFF;
}

// DAG-axis counterpart of propose_loop_axis_for_factor: find the
// first KAX_LOOP axis (by axis id, scanning ids >= `start`) in the
// lifted DAG that `factor` divides, returning its axis id or 0xFF.
//
// propose_loop_axis_for_factor reads tile_anno / axes_resolve_n_axes,
// which returns 0 when ke->schedule == NULL -- the case for a
// synthetic KernelEntry built by the py bridge (kernel_alloc +
// kernel_set_cached_lift, no materialize / schedule).  The CUDA
// autotune sweep drives propose exactly that way, so the CUDA block
// must derive its loop axes straight from cached_lift.store_root's
// RANGE leaves instead.  uop_dag_collect_axes returns the same
// (id, type, extent) the renderer decodes, so the resulting KOpt
// axis id is directly applicable via uop_dag_apply_kopt.
static u8 propose_dag_loop_axis_for_factor(KernelEntry const *ke,
                                           u8 start, u32 factor) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0xFF;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  u8 best = 0xFF;
  for (u32 i = 0; i < n; i++) {
    if (types[i] != KAX_LOOP) continue;
    if (ids[i] < start) continue;
    if (factor > exts[i] || exts[i] % factor != 0) continue;
    if (best == 0xFF || ids[i] < best) best = (u8)ids[i];
  }
  return best;
}

static u32 propose_conv2d_local_opts(KernelEntry const *ke, KOpt *out,
                                     u32 n, u32 cap) {
  static const u32 local_factors[] = {256, 128, 64, 32, 16, 8, 4, 2};
  u32 n_local_factors = sizeof(local_factors)/sizeof(*local_factors);
  u32 n_axes = tile_anno_axis_count_or_kernelaxes(ke);
  for (u32 i = 0; i < n_local_factors && n < cap; i++) {
    u32 f = local_factors[i];
    for (u8 axis = 0; axis < n_axes; axis++) {
      TileAxisInfo info;
      if (!tile_anno_axis_or_kernelaxes(ke, axis, &info)) continue;
      if (info.kax_type != KAX_LOOP) continue;
      if (info.extent < f || info.extent % f != 0) continue;
      out[n].op   = KOP_LOCAL;
      out[n].axis = axis;
      out[n].arg  = f;
      n++;
      break;
    }
  }
  return n;
}

static u32 propose_conv2d_upcast_opts(KernelEntry const *ke, KOpt *out,
                                      u32 n, u32 cap) {
  static const u32 upcast_factors[] = {8, 4, 2};
  u32 n_upcast_factors = sizeof(upcast_factors)/sizeof(*upcast_factors);
  u32 n_axes = tile_anno_axis_count_or_kernelaxes(ke);
  for (u32 i = 0; i < n_upcast_factors && n < cap; i++) {
    u32 f = upcast_factors[i];
    for (u8 axis = 0; axis < n_axes; axis++) {
      TileAxisInfo info;
      if (!tile_anno_axis_or_kernelaxes(ke, axis, &info)) continue;
      if (info.kax_type != KAX_LOOP) continue;
      if (info.extent < f || info.extent % f != 0) continue;
      out[n].op   = KOP_UPCAST;
      out[n].axis = axis;
      out[n].arg  = f;
      n++;
      break;
    }
  }
  return n;
}

static u32 propose_conv2d_unroll_opts(KernelEntry const *ke, KOpt *out,
                                      u32 n, u32 cap) {
  static const u32 unroll_factors[] = {2};
  u8 axis = propose_reduce_axis_index(ke);
  if (axis == 0xFF) {
    return n;
  }
  TileAxisInfo info;
  if (!tile_anno_axis_or_kernelaxes(ke, axis, &info)) return n;
  u32 axis_size = info.extent;
  u32 n_unroll_factors = sizeof(unroll_factors)/sizeof(*unroll_factors);
  for (u32 i = 0; i < n_unroll_factors && n < cap; i++) {
    u32 f = unroll_factors[i];
    if (axis_size < f || axis_size % f != 0) {
      continue;
    }
    out[n].op   = KOP_UNROLL;
    out[n].axis = axis;
    out[n].arg  = f;
    n++;
  }
  return n;
}

static int propose_metal_backend_enabled(void) {
  return thvm_dev_name_is(getenv("DEV"), "metal");
}

static int propose_cuda_backend_enabled(void) {
  return thvm_dev_name_is(getenv("DEV"), "cuda");
}

static int propose_metal_tile_enabled(void) {
  char const *tile    = getenv("THVM_TILE");
  return propose_metal_backend_enabled() && tile != NULL && tile[0] == '1';
}

static int propose_metal_reduce_unroll_kernel(KernelEntry const *ke) {
  if (!propose_metal_backend_enabled()) {
    return 1;
  }
  // FP32-only: every input dtype must be FP32, every dtype-carrying
  // node in the lifted DAG must be FP32, and at least one UOP_REDUCE
  // must be reachable.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_dtype_uniform(ke->cached_lift.store_root, DT_FP32)) {
    return 0;
  }
  // A matmul (reduce-of-mul) passes the reduce-unroll structural walk, but
  // its K axis is the simdgroup contraction axis -- splitting it produces
  // the same broken simdgroup_load MSL as splitting M/N (see
  // propose_rich_action_space).  KOP_TC is the only faithful tiling lever
  // for a matmul; never offer a K-UNROLL on one.
  Term base = ke->cached_lift_init_root != 0 ? ke->cached_lift_init_root
                                             : ke->cached_lift.store_root;
  if (uop_classify_matmul(base, NULL, NULL)) {
    return 0;
  }
  return uop_dag_is_reduce_unroll_kernel(ke->cached_lift.store_root);
}

// Append `cand` to out[] iff it is not already present and there is room.
// Returns the new count.  The autotune beam dedups whole sequences too,
// but pruning identical single-opt candidates here keeps the proposed
// set tight (mirrors tinygrad's `acted_kernels` dedup-by-applied-state).
static u32 propose_append_unique(KOpt *out, u32 n, u32 cap, KOpt cand) {
  if (n >= cap) return n;
  for (u32 i = 0; i < n; i++) {
    if (out[i].op == cand.op && out[i].axis == cand.axis
        && out[i].arg == cand.arg) {
      return n;
    }
  }
  out[n] = cand;
  return n + 1;
}

// Port of tinygrad's `actions` list (codegen/opt/search.py:13-24),
// restricted to the 5 OptOps thvm has codegen for: UPCAST, UNROLL,
// LOCAL, GROUP_REDUCE (=GROUP), TC.  tinygrad's GROUPTOP/SWAP/THREAD/
// PADTO/NOLOCALS are skipped (no matching applicable OptOp on the thvm
// split/group renderers).  Each candidate is emitted over the kernel's
// REAL axis ids (uop_dag_collect_axes) -- thvm's KOP axis is a raw
// axis_id that uop_dag_apply_kopt looks up by RANGE leaf, not tinygrad's
// positional class index -- and the autotune apply step bails (returns 0,
// loop `continue`s) on any that don't divide / don't match the axis type,
// exactly like tinygrad's `_get_acted_kernels` prunes invalid opts.
//
//   UPCAST: arg in {2,3,4,5,7} over each output (LOOP) axis
//   UNROLL: arg in {2,4,7,8,16} over each reduce axis
//   LOCAL : arg in {2,3,4,8,13,16,29} over each output (LOOP) axis
//   GROUP : arg in {4,8,16} over each reduce axis
static u32 propose_rich_action_space(KernelEntry const *ke, KOpt *out,
                                     u32 n, u32 cap) {
  if (ke == NULL) return n;
  // Enumerate axes from the BASE (pre-heuristic) DAG snapshot, not the
  // current heuristic-optimized store_root.  kernel_opts_propose runs
  // AFTER kernel_hand_coded_opts (so store_root's output axes are already
  // KAX_GLOBAL / its reduce axes already split), but the autotune bench
  // applies each candidate on the RESET state (axes_reset_to_default
  // rewinds store_root to cached_lift_init_root, where the output axes are
  // KAX_LOOP and the reduce axes KAX_REDUCE).  Proposing against the base
  // is also exactly what tinygrad does -- its `actions` target the base
  // kernel's axes.  Fall back to store_root for the synthetic / FFI path
  // (no materialize -> init_root == 0).
  Term base = ke->cached_lift_init_root != 0 ? ke->cached_lift_init_root
                                             : ke->cached_lift.store_root;
  if (base == 0) return n;

  // Skip the raw split/group action space on tensor-core MATMUL kernels.
  // The Metal renderer recognises a matmul's reduce-of-mul structure
  // (uop_classify_matmul, the same recogniser rmu_emit_matmul_tc uses) and
  // emits a simdgroup_matrix tile whose load address expressions name the
  // M/N/K axes by their UN-split ids.  An independent UPCAST/UNROLL/LOCAL/
  // GROUP split inserts/renumbers one of those axes, so the simdgroup_load
  // then references an axis var that was never declared in the tile loop
  // ("use of undeclared identifier a2") -- the MSL fails to compile under
  // JIT capture and silently falls back to a zero-producing path.  thvm's
  // KOP_TC is the ONLY faithful tiling lever for these kernels (its tiles
  // ARE proposed, above); tinygrad likewise lets the TC opt own a matmul's
  // M/N/K and never stacks a raw split on a simdgroup axis.  The rich split
  // set is the real lever for the NON-matmul bulk (elementwise / reduce /
  // norm / softmax), which is exactly where BEAM had zero candidates before.
  if (uop_classify_matmul(base, NULL, NULL)) {
    return n;
  }

  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n_ax = uop_dag_collect_axes(base, ids, types, exts, MAX_AXES);
  if (n_ax == 0) return n;

  static const u32 upcast_args[] = {2, 3, 4, 5, 7};
  static const u32 unroll_args[] = {2, 4, 7, 8, 16};
  static const u32 local_args [] = {2, 3, 4, 8, 13, 16, 29};
  static const u32 group_args [] = {4, 8, 16};

  // Output-axis (LOOP) opts: UPCAST then LOCAL.
  for (u32 a = 0; a < n_ax && n < cap; a++) {
    if (types[a] != KAX_LOOP) continue;
    for (u32 i = 0; i < sizeof(upcast_args)/sizeof(*upcast_args) && n < cap; i++) {
      n = propose_append_unique(out, n, cap,
            (KOpt){ KOP_UPCAST, ids[a], upcast_args[i] });
    }
    for (u32 i = 0; i < sizeof(local_args)/sizeof(*local_args) && n < cap; i++) {
      n = propose_append_unique(out, n, cap,
            (KOpt){ KOP_LOCAL, ids[a], local_args[i] });
    }
  }
  // Reduce-axis opts: UNROLL then GROUP.
  for (u32 a = 0; a < n_ax && n < cap; a++) {
    if (types[a] != KAX_REDUCE) continue;
    for (u32 i = 0; i < sizeof(unroll_args)/sizeof(*unroll_args) && n < cap; i++) {
      n = propose_append_unique(out, n, cap,
            (KOpt){ KOP_UNROLL, ids[a], unroll_args[i] });
    }
    for (u32 i = 0; i < sizeof(group_args)/sizeof(*group_args) && n < cap; i++) {
      n = propose_append_unique(out, n, cap,
            (KOpt){ KOP_GROUP, ids[a], group_args[i] });
    }
  }
  return n;
}

fn u32 kernel_opts_propose(KernelEntry const *ke, KOpt *out, u32 cap) {
  if (ke == NULL || out == NULL || cap == 0) return 0;
  u32 n = 0;

  static const u32 split_factors[] = {16, 8, 4, 2};
  u32 n_factors = sizeof(split_factors)/sizeof(*split_factors);

  // Gate flip: BEAM TC entry checks DAG presence directly.  The body's
  // `propose_tc_classify` already requires `cached_lift.store_root != 0`
  // (DAG-side matmul classifier), so the outer `axes != NULL && axis_count
  // > 0` proxy is redundant -- every materialized kernel that reaches
  // this proposer has both, and `cached_lift.store_root != 0` is a
  // strict tightening that matches what the body actually needs.
  // TC candidates first (heuristic-ordered) but DO NOT return early: a
  // matmul kernel must ALSO get the rich UPCAST/LOCAL/UNROLL/GROUP action
  // space appended below.  tinygrad's `actions` (search.py:13-24) offers
  // every OptOp on every kernel and prunes the inapplicable ones; the
  // pre-rich thin TC-only return meant BEAM could never FIND the good tile
  // for a FLUX matmul -- the whole motivation for porting the rich set.
  if (propose_metal_backend_enabled() && ke->cached_lift.store_root != 0) {
    u32 dtype = 0;
    UopDagGemmShape shape;
    if (propose_tc_classify(ke, &dtype, &shape)
        && (dtype == DT_FP32 || dtype == DT_BF16)) {
      // Rich tile search: enumerate every VALID simdgroup_matrix tile
      // (local_m/n x rm/rn x kb) for this {M,N,K} and offer each as a
      // packed KOP_TC.  The emitter unpacks + re-validates each before
      // use; the heuristic remains the no-config default.  This replaces
      // the old thin {32,16,8} arg set (which the emitter ignored, so the
      // search saw 3 identical kernels) -- the actual FLUX perf lever.
      int c_is_bf = (dtype == DT_BF16);
      n = propose_tc_tile_candidates(shape.M, shape.N, shape.K, c_is_bf,
                                     /*unit=*/8u, /*is_cuda=*/0, out, n, cap);
    }
  }
  // CUDA: same rich TC tile search, WMMA 16x16x16 fragments (unit=16).  The
  // emitter-side override hook (rmu_tc_apply_opt_config CUDA path) is already
  // wired; tc_tile_valid(is_cuda=1) rejects non-16-multiple K-blocks etc.
  if (propose_cuda_backend_enabled() && ke->cached_lift.store_root != 0) {
    u32 dtype = 0;
    UopDagGemmShape shape;
    if (propose_tc_classify(ke, &dtype, &shape)
        && (dtype == DT_FP32 || dtype == DT_BF16)) {
      int c_is_bf = (dtype == DT_BF16);
      n = propose_tc_tile_candidates(shape.M, shape.N, shape.K, c_is_bf,
                                     /*unit=*/16u, /*is_cuda=*/1, out, n, cap);
    }
  }

  // Mirrors the BEAM TC entry gate above.  tile_analyze_conv2d_flat
  // accepts DAG kernels via uop_dag_classify_conv2d_flat_shape when
  // ke->cached_lift.store_root != 0; the axes-presence proxy gates
  // any synthetic fixture that builds KpSchedule without running
  // the lifter.
  if (propose_metal_tile_enabled()
      && (ke->cached_lift.store_root != 0
          || (ke->schedule != NULL
              && tile_anno_axis_count_or_kernelaxes(ke) > 0))) {
    TileConv2DInfo conv;
    if (tile_analyze_conv2d_flat(ke, &conv)) {
      n = propose_conv2d_local_opts(ke, out, n, cap);
      n = propose_conv2d_upcast_opts(ke, out, n, cap);
      return propose_conv2d_unroll_opts(ke, out, n, cap);
    }
  }

  // Reduce-tail UNROLL candidates: {2, 4, 8, 16} where divisible.
  // Skip 1 (= no opt; the autotune loop tracks the baseline
  // separately).  Larger factors first so wins compose if the
  // autotune later supports composite proposals.
  u8  axis_idx  = propose_reduce_axis_index(ke);
  TileAxisInfo axis_info;
  u32 axis_size = axis_idx == 0xFF ? propose_reduce_axis_size(ke)
                                   : (tile_anno_axis_or_kernelaxes(ke, axis_idx,
                                        &axis_info) ? axis_info.extent : 0);
  if (axis_size > 0 && axis_idx != 0xFF
      && propose_metal_reduce_unroll_kernel(ke)) {
    for (u32 i = 0; i < n_factors; i++) {
      u32 f = split_factors[i];
      if (axis_size % f != 0) continue;
      if (n >= cap) break;
      out[n].op   = KOP_UNROLL;
      out[n].axis = axis_idx;
      out[n].arg  = f;
      n++;
    }
  }

  // Elementwise output-axis candidates.  Pick the first LOOP axis
  // that each factor can actually split, not just axis 0.  This keeps
  // leading-size-1 movement views from hiding large inner loops from
  // the Metal LOCAL/GLOBAL autotune path.
  if (axis_size == 0 && ke->output_numel > 0) {
    int metal_elementwise_ok = propose_metal_tile_enabled()
        && ke->output_dtype == DT_FP32
        && ke->n_inputs <= 30;
    if (metal_elementwise_ok) {
      for (u32 i = 0; i < ke->n_inputs && metal_elementwise_ok; i++) {
        if (ke->input_dtypes[i] != DT_FP32) metal_elementwise_ok = 0;
      }
    }
    if (metal_elementwise_ok) {
      static const u32 local_factors[] = {256, 128, 64, 32, 16, 8, 4, 2};
      u32 n_local_factors = sizeof(local_factors)/sizeof(*local_factors);
      for (u32 i = 0; i < n_local_factors; i++) {
        u32 f = local_factors[i];
        u8 loop_axis = propose_loop_axis_for_factor(ke, 0, f);
        if (loop_axis == 0xFF) continue;
        if (n >= cap) break;
        out[n].op   = KOP_LOCAL;
        out[n].axis = loop_axis;
        out[n].arg  = f;
        n++;
      }
    }
    if (!propose_metal_backend_enabled()) {
      for (u32 i = 0; i < n_factors; i++) {
        u32 f = split_factors[i];
        u8 loop_axis = propose_loop_axis_for_factor(ke, 0, f);
        if (loop_axis == 0xFF) continue;
        if (n >= cap) break;
        out[n].op   = KOP_UPCAST;
        out[n].axis = loop_axis;
        out[n].arg  = f;
        n++;
      }
    }
  }

  // CUDA: propose KOP_LOCAL on an output LOOP axis -- maps the tile to
  // a CUDA threadblock.  LOCAL is never offered on the non-Metal path
  // otherwise (the blocks above give UPCAST/UNROLL only), yet it is
  // the key occupancy lever: cuda_dag_dispatch_shape launches a
  // LOCAL-split kernel with the matching block geometry, so these
  // candidates are dispatchable.  Reduce-tail kernels (matmul) are
  // included -- the output LOOP axes tile independently of the
  // in-thread reduce axis, so this fires whether axis_size is 0 or not.
  if (propose_cuda_backend_enabled()) {
    // Loop-axis lookup: the schedule-reading propose_loop_axis_for_factor
    // returns 0xFF for a synthetic (no-schedule) KernelEntry, which is
    // exactly how the py-driven CUDA autotune sweep builds the kernel.
    // Fall back to the DAG-axis reader so the sweep actually gets
    // candidates; a production kernel with a schedule still resolves
    // through the schedule path first.
    static const u32 cuda_local[] = {256, 128, 64, 32};
    u32 n_cuda_local = sizeof(cuda_local) / sizeof(*cuda_local);
    for (u32 i = 0; i < n_cuda_local && n < cap; i++) {
      u8 loop_axis = propose_loop_axis_for_factor(ke, 0, cuda_local[i]);
      if (loop_axis == 0xFF) {
        loop_axis = propose_dag_loop_axis_for_factor(ke, 0, cuda_local[i]);
      }
      if (loop_axis == 0xFF) continue;
      out[n].op   = KOP_LOCAL;
      out[n].axis = loop_axis;
      out[n].arg  = cuda_local[i];
      n++;
    }
    // KOP_UPCAST: each thread computes `factor` output rows -- the
    // outputs-per-thread tiling lever (raises arithmetic intensity,
    // shrinks the launched thread count).  UPCAST is in-thread, so
    // cuda_dag_dispatch_shape leaves it out of the grid/block and the
    // flat geometry already covers it -- no dispatch change needed.
    // Gated on axis_size > 0 (reduce-tail kernels -- matmul): the
    // elementwise block above already proposes UPCAST when axis_size
    // is 0, so this only fills the matmul gap it leaves.
    if (axis_size > 0) {
      static const u32 cuda_upcast[] = {8, 4, 2};
      u32 n_cuda_upcast = sizeof(cuda_upcast) / sizeof(*cuda_upcast);
      for (u32 i = 0; i < n_cuda_upcast && n < cap; i++) {
        u8 loop_axis = propose_loop_axis_for_factor(ke, 0, cuda_upcast[i]);
        if (loop_axis == 0xFF) {
          loop_axis = propose_dag_loop_axis_for_factor(ke, 0, cuda_upcast[i]);
        }
        if (loop_axis == 0xFF) continue;
        out[n].op   = KOP_UPCAST;
        out[n].axis = loop_axis;
        out[n].arg  = cuda_upcast[i];
        n++;
      }
    }
  }

  // Rich action space for EVERY Metal kernel -- tinygrad's `actions`
  // (search.py:13-24) offered on TC / matmul / elementwise / plain-reduce
  // alike, not just the special cases handled above.  This is the whole
  // point of the rich port: a FLUX matmul that got only ~3 TC tiles before
  // now also gets UPCAST/LOCAL/UNROLL/GROUP candidates so BEAM can FIND a
  // good tile instead of settling for a worse one.  propose_rich_action_space
  // dedups against everything already emitted; the autotune apply step
  // prunes any candidate that does not divide / match its axis type
  // (returns 0 -> the search loop `continue`s), mirroring tinygrad's
  // `_get_acted_kernels`.  The speed-sanity gate + per-variant bench keep
  // only variants that beat the heuristic by a margin, so a larger
  // candidate set can only ever help.
  if (propose_metal_backend_enabled()) {
    n = propose_rich_action_space(ke, out, n, cap);
  }
  return n;
}
