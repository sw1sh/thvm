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
//   THVM_BACKEND=metal + f32 TILE_MMA/GEMM kernel
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
// axes_default_for / cg_emit do.
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

static u32 propose_scalar_reduce_axis_size(KernelEntry const *ke) {
  if (ke == NULL || ke->scalar_uops == NULL) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_REDUCE_SUM && u->op != S_REDUCE_MAX) {
      continue;
    }
    if (u->src_count < 2 || u->src[1] == 0
        || u->src[1] >= ke->n_scalar_uops) {
      return 0;
    }
    ScalarUop const *rng = &ke->scalar_uops[u->src[1]];
    if (rng->op != S_RANGE) {
      return 0;
    }
    u32 axis_type = (u32)(rng->extra >> 32);
    u32 extent    = (u32)(rng->extra & 0xFFFFFFFFu);
    if (axis_type != S_AXIS_REDUCE) {
      return 0;
    }
    return extent;
  }
  return 0;
}

static u32 propose_reduce_axis_size(KernelEntry const *ke) {
  u32 size = propose_kprog_reduce_axis_size(ke);
  return size != 0 ? size : propose_scalar_reduce_axis_size(ke);
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

static int propose_metal_tile_group_reduce_kernel(KernelEntry const *ke);

static u32 propose_group_reduce_opts(KernelEntry const *ke, KOpt *out,
                                     u32 n, u32 cap, u8 axis,
                                     u32 axis_size) {
  if (axis_size == 0 || axis == 0xFF
      || !propose_metal_tile_group_reduce_kernel(ke)) {
    return n;
  }
  if (axis_size <= 256 && n < cap) {
    out[n].op   = KOP_GROUP;
    out[n].axis = axis;
    out[n].arg  = axis_size;
    n++;
  }
  static const u32 group_factors[] = {256, 128, 64, 32, 16, 8, 4, 2};
  u32 n_group_factors = sizeof(group_factors)/sizeof(*group_factors);
  for (u32 i = 0; i < n_group_factors; i++) {
    u32 f = group_factors[i];
    if (f == axis_size) {
      continue;
    }
    if (axis_size % f != 0 || f > axis_size) {
      continue;
    }
    if (n >= cap) {
      break;
    }
    out[n].op   = KOP_GROUP;
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

static int propose_metal_tile_scalar_reduce_op_ok(u8 op) {
  switch (op) {
    case S_NONE:
    case S_RANGE:
    case S_DEFINE_PARAM:
    case S_DEFINE_OUTPUT:
    case S_INDEX:
    case S_INDEX_E:
    case S_LOAD:
    case S_STORE:
    case S_BUFFERIZE:
    case S_ADD:
    case S_MUL:
    case S_NEG:
    case S_RECIP:
    case S_SQRT:
    case S_EXP2:
    case S_LOG2:
    case S_CMPLT:
    case S_CMPEQ:
    case S_REDUCE_SUM:
    case S_REDUCE_MAX:
    case S_CAST:
    case S_RESHAPE_V:
    case S_CONST:
    case S_ICONST:
    case S_IADD:
    case S_ISUB:
    case S_IMUL:
    case S_IDIV:
    case S_IMOD:
    case S_ILT:
    case S_IAND:
    case S_IWHERE:
      return 1;
    default:
      return 0;
  }
}

static int propose_scalar_op_carries_kernel_dtype(ScalarUop const *u) {
  switch (u->op) {
    case S_LOAD:
    case S_STORE:
    case S_CONST:
    case S_ADD:
    case S_MUL:
    case S_NEG:
    case S_RECIP:
    case S_SQRT:
    case S_EXP2:
    case S_LOG2:
    case S_CMPLT:
    case S_CMPEQ:
    case S_REDUCE_SUM:
    case S_REDUCE_MAX:
    case S_CAST:
    case S_RESHAPE_V:
      return 1;
    case S_IWHERE:
      return u->dtype != DT_INT64;
    default:
      return 0;
  }
}

static int propose_metal_tile_scalar_reduce_kernel(KernelEntry const *ke) {
  if (!propose_metal_tile_enabled() || ke->scalar_uops == NULL
      || ke->n_scalar_uops < 2 || ke->output_dtype != DT_FP32) {
    return 0;
  }
  if (tile_rejects_conv2d_flat_cin1(ke)) {
    return 0;
  }
  int has_reduce = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (!propose_metal_tile_scalar_reduce_op_ok(u->op)) {
      return 0;
    }
    if (propose_scalar_op_carries_kernel_dtype(u) && u->dtype != DT_FP32) {
      return 0;
    }
    if (u->op == S_REDUCE_SUM || u->op == S_REDUCE_MAX) {
      has_reduce = 1;
    }
  }
  return has_reduce;
}

static int propose_metal_reduce_unroll_kernel(KernelEntry const *ke) {
  if (!propose_metal_backend_enabled()) {
    return 1;
  }
  if (propose_metal_tile_scalar_reduce_kernel(ke)) {
    return 1;
  }
  if (ke->n_ops == 0 || ke->program[ke->n_ops - 1].opcode != UOP_REDUCE) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
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

static int propose_metal_tile_kernel(KernelEntry const *ke) {
  if (!propose_metal_tile_enabled()) {
    return 0;
  }
  if (ke->output_dtype != DT_FP32 || ke->scalar_uops == NULL
      || ke->n_scalar_uops < 2 || ke->tile_uops == NULL
      || ke->n_tile_uops < 2 || ke->axes == NULL || ke->n_inputs > 30) {
    return 0;
  }
  int has_loop = 0;
  u32 n_axes_p = tile_anno_axis_count_or_kernelaxes(ke);
  for (u8 i = 0; i < n_axes_p; i++) {
    TileAxisInfo info;
    if (!tile_anno_axis_or_kernelaxes(ke, i, &info)) continue;
    if (info.kax_type == KAX_LOOP) {
      has_loop = 1;
      break;
    }
  }
  if (!has_loop) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }
  return 1;
}

static int propose_metal_tile_group_reduce_kernel(KernelEntry const *ke) {
  if (!propose_metal_tile_kernel(ke)) {
    return 0;
  }
  TilePlanInfo info;
  if (!tile_collect_plan_info(ke, &info)) {
    return 0;
  }
  return info.scalar_reduce_id != 0;
}

fn u32 kernel_opts_propose(KernelEntry const *ke, KOpt *out, u32 cap) {
  if (ke == NULL || out == NULL || cap == 0) return 0;
  u32 n = 0;

  static const u32 split_factors[] = {16, 8, 4, 2};
  u32 n_factors = sizeof(split_factors)/sizeof(*split_factors);

  if (propose_metal_backend_enabled() && ke->axes != NULL
      && ke->axes->n_axes > 0) {
    TileGemmInfo gemm;
    if (tile_analyze_gemm(ke, NULL, &gemm) && gemm.dtype == DT_FP32) {
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

  if (propose_metal_tile_enabled() && ke->axes != NULL
      && ke->axes->n_axes > 0) {
    TileConv2DInfo conv;
    if (tile_analyze_conv2d_flat(ke, &conv)) {
      u8  red_axis = propose_reduce_axis_index(ke);
      TileAxisInfo red_info;
      u32 red_size = (red_axis != 0xFF
                      && tile_anno_axis_or_kernelaxes(ke, red_axis, &red_info))
                       ? red_info.extent : 0;
      n = propose_conv2d_local_opts(ke, out, n, cap);
      n = propose_conv2d_upcast_opts(ke, out, n, cap);
      n = propose_group_reduce_opts(ke, out, n, cap, red_axis, red_size);
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
    n = propose_group_reduce_opts(ke, out, n, cap, axis_idx, axis_size);
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
    if (propose_metal_tile_kernel(ke)) {
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
