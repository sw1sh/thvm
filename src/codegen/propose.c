// codegen/propose.c -- shape-heuristic kernel opt proposer.
//
// Given a finalized KernelEntry (post-default-axes), suggest a small
// set of candidate TOpts the autotune loop should try.  Today's
// heuristics are deliberately narrow:
//
//   reduce-tail kernel + axis_size % factor == 0 -> propose UNROLL
//   factor for factor in {2, 4, 8, 16}.
//
//   THVM_BACKEND=metal + THVM_TILE=1 + rank-1 f32 scalar/tile kernel
//   -> propose LOCAL tile factors.  The autotune loop applies the
//   matching outer GLOBAL mark when benchmarking these candidates.
//
//   THVM_BACKEND=metal + f32 GEMM kernel
//   -> propose TC tile sizes.  The first implementation uses TC as
//   metadata for the fixed direct Metal GEMM renderer; later it maps
//   to simdgroup MMA variants.
//
//   THVM_BACKEND=metal + THVM_TILE=1 + im2col-fused Conv2D template
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

// Reduce-axis size for a tail-REDUCE kernel, or 0 if not reduce-tail
// (or if shape inference fails).  Mirrors the same calc that
// axes_default_for + render_uop's accumulator hoist do.
static u32 propose_kprog_reduce_axis_size(KernelEntry const *ke) {
  if (ke->n_ops == 0) return 0;
  KProgOp const *rd = &ke->program[ke->n_ops - 1];
  if (rd->opcode != UOP_REDUCE) return 0;
  u32 src_numel;
  if (KSRC_IS_INPUT(rd->src[0])) src_numel = ke->input_numels[KSRC_INDEX(rd->src[0])];
  else                           src_numel = ke->program[KSRC_INDEX(rd->src[0])].numel;
  u32 out_numel = ke->output_numel ? ke->output_numel : 1;
  return src_numel / out_numel;
}

// Walk the lifted UOp DAG for the reduce axis extent when
// cached_lift.store_root is populated (the autotune-time path
// pays the lift cost at materialize-time).
static u32 propose_uop_reduce_axis_size(KernelEntry const *ke) {
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

// Matmul-shape + dtype gate for the TC tile-size proposer.  Reads
// uop_dag_classify_matmul_shape over ke->cached_lift.store_root.
static int propose_tc_classify(KernelEntry const *ke, u32 *out_dtype) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  UopDagGemmShape shape;
  if (!uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke,
                                     &shape)) {
    return 0;
  }
  if (out_dtype != NULL) *out_dtype = shape.dtype;
  PROPOSE_TC_DAG++;
  return 1;
}

static u32 propose_reduce_axis_size(KernelEntry const *ke) {
  u32 size = propose_uop_reduce_axis_size(ke);
  if (size != 0) return size;
  return propose_kprog_reduce_axis_size(ke);
}

// Index of the reduce axis -- the last axis of type KAX_REDUCE.
// Returns 0xFF if none (caller checks `< n_axes`).
// Phase E migration: reads via tile_anno_axis_or_kernelaxes so
// it works whether tile_uops is fresh, stale, or absent.
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
  char const *backend = getenv("THVM_BACKEND");
  return backend != NULL && strcmp(backend, "metal") == 0;
}

static int propose_metal_tile_enabled(void) {
  char const *tile    = getenv("THVM_TILE");
  return propose_metal_backend_enabled() && tile != NULL && tile[0] == '1';
}

static int propose_metal_reduce_unroll_kernel(KernelEntry const *ke) {
  if (!propose_metal_backend_enabled()) {
    return 1;
  }
  // When cached_lift.store_root is populated, mirror the per-op
  // KProgOp gate via the lifted UOp DAG: every dtype-carrying node
  // is FP32 AND at least one UOP_REDUCE is reachable.  Lifted
  // kernels skip the per-op walk entirely.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }
  if (ke->cached_lift.store_root != 0) {
    if (!uop_dag_dtype_uniform(ke->cached_lift.store_root, DT_FP32)) {
      return 0;
    }
    return uop_dag_is_reduce_unroll_kernel(ke->cached_lift.store_root);
  }
  if (ke->n_ops == 0 || ke->program[ke->n_ops - 1].opcode != UOP_REDUCE) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp const *op = &ke->program[i];
    if (op->dtype != DT_FP32) {
      return 0;
    }
    switch (op->opcode) {
      case UOP_CONST:
      case UOP_ADD:
      case UOP_MUL:
      case UOP_NEG:
      case UOP_RECIP:
      case UOP_SQRT:
      case UOP_EXP2:
      case UOP_LOG2:
      case UOP_CMPLT:
      case UOP_CMPEQ:
        break;
      case UOP_REDUCE:
        if (i + 1 != ke->n_ops) {
          return 0;
        }
        break;
      default:
        return 0;
    }
  }
  return 1;
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
  if (propose_metal_backend_enabled() && ke->cached_lift.store_root != 0) {
    u32 dtype = 0;
    if (propose_tc_classify(ke, &dtype) && dtype == DT_FP32) {
      static const u32 tc_tiles[] = {32, 16, 8};
      u32 n_tc_tiles = sizeof(tc_tiles)/sizeof(*tc_tiles);
      for (u32 i = 0; i < n_tc_tiles && n < cap; i++) {
        out[n].op   = KOP_TC;
        out[n].axis = 0;
        out[n].arg  = tc_tiles[i];
        n++;
      }
      return n;
    }
  }

  // Mirrors the BEAM TC entry gate above.  tile_analyze_conv2d_flat
  // accepts DAG kernels via uop_dag_classify_conv2d_flat_shape (when
  // ke->cached_lift.store_root != 0) AND keeps the program[] path
  // as a fallback for lift-decline fixtures.  OR the gate:
  // production kernels carry cached_lift.store_root, but the
  // tests/test_tile_graph.c::metal-conv2d-flat-proposes-local
  // fixture builds KProgOp + KpSchedule without running the lifter,
  // so the axes-presence proxy still has to fire.
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
  return n;
}
