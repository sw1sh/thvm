// schedule/tile.c -- tile-plan arena above scalar-UOps.
//
// Rangeify owns semantic lowering to ScalarUop; tile.c owns the
// schedule/memory plan that wraps those scalar bodies with explicit
// axes, local memory, barriers, reductions, and future MMA nodes.

fn void tile_reserve(KernelEntry *ke, u32 needed) {
  if (needed <= ke->tile_uops_cap) {
    return;
  }
  if (needed > TILE_MAX_CAP) {
    fprintf(stderr, "tile_reserve: needed=%u exceeds cap %u\n",
            needed, TILE_MAX_CAP);
    exit(1);
  }
  u32 new_cap = ke->tile_uops_cap == 0 ? TILE_INIT_CAP : ke->tile_uops_cap * 2;
  while (new_cap < needed) {
    new_cap *= 2;
  }
  ke->tile_uops = (TileUop *)realloc(ke->tile_uops,
                                     (size_t)new_cap * sizeof(TileUop));
  if (ke->tile_uops_cap == 0) {
    memset(&ke->tile_uops[0], 0, sizeof(TileUop));
    ke->n_tile_uops = 1;
  }
  for (u32 i = ke->tile_uops_cap == 0 ? 1 : ke->tile_uops_cap;
       i < new_cap; i++) {
    memset(&ke->tile_uops[i], 0, sizeof(TileUop));
  }
  ke->tile_uops_cap = new_cap;
}

fn u32 tile_emit(KernelEntry *ke, u8 op, u32 dtype,
                 u8 src_count, const u32 *src, u64 extra) {
  if (op == TILE_NONE || op >= TILE__COUNT) {
    fprintf(stderr, "tile_emit: bad op=%u\n", op);
    exit(1);
  }
  if (src_count > TILE_MAX_SRC) {
    fprintf(stderr, "tile_emit: src_count=%u exceeds max %u\n",
            src_count, TILE_MAX_SRC);
    exit(1);
  }
  tile_reserve(ke, ke->n_tile_uops + 1);
  u32 id       = ke->n_tile_uops++;
  TileUop *u   = &ke->tile_uops[id];
  u->op        = op;
  u->src_count = src_count;
  u->dtype     = dtype;
  u->extra     = extra;
  for (u8 i = 0; i < TILE_MAX_SRC; i++) {
    u->src[i] = (i < src_count && src != NULL) ? src[i] : 0;
  }
  return id;
}

fn u32 tile_emit_leaf(KernelEntry *ke, u8 op, u32 dtype, u64 extra) {
  return tile_emit(ke, op, dtype, 0, NULL, extra);
}

fn void tile_free(KernelEntry *ke) {
  if (ke->tile_uops != NULL) {
    free(ke->tile_uops);
  }
  ke->tile_uops     = NULL;
  ke->n_tile_uops   = 0;
  ke->tile_uops_cap = 0;
  ke->tile_root     = 0;
  ke->tile_axes_version = 0;
}

fn const char *tile_op_name(u8 op) {
  switch (op) {
    case TILE_NONE:        return "TILE_NONE";
    case TILE_AXIS:        return "TILE_AXIS";
    case TILE_SCALAR_BODY: return "TILE_SCALAR_BODY";
    case TILE_LOOP_NEST:   return "TILE_LOOP_NEST";
    case TILE_LOCAL_ALLOC: return "TILE_LOCAL_ALLOC";
    case TILE_LOAD:        return "TILE_LOAD";
    case TILE_STORE:       return "TILE_STORE";
    case TILE_BARRIER:     return "TILE_BARRIER";
    case TILE_REDUCE:      return "TILE_REDUCE";
    case TILE_MMA:         return "TILE_MMA";
    default:               return "TILE_?";
  }
}

fn const char *tile_axis_name(u32 axis_type) {
  switch (axis_type) {
    case KAX_LOOP:         return "LOOP";
    case KAX_REDUCE:       return "REDUCE";
    case KAX_UPCAST:       return "UPCAST";
    case KAX_UNROLL:       return "UNROLL";
    case KAX_LOCAL:        return "LOCAL";
    case KAX_GLOBAL:       return "GLOBAL";
    case KAX_GROUP_REDUCE: return "GROUP_REDUCE";
    default:               return "?";
  }
}

static u8 tile_axis_from_scalar_axis(u32 axis_type) {
  switch (axis_type) {
    case S_AXIS_LOOP:    return KAX_LOOP;
    case S_AXIS_REDUCE:  return KAX_REDUCE;
    case S_AXIS_UNROLL:  return KAX_UNROLL;
    case S_AXIS_GLOBAL:  return KAX_GLOBAL;
    case S_AXIS_VIRT:    return KAX_LOOP;
    default:             return KAX_LOOP;
  }
}

static int tile_axis_type_ok(u32 axis_type) {
  return axis_type <= KAX_GROUP_REDUCE;
}

static int tile_id_ok(KernelEntry const *ke, u32 id) {
  return id != 0 && id < ke->n_tile_uops;
}

fn u32 tile_loop_axis_count(KernelEntry const *ke) {
  TilePlanInfo info;
  if (!tile_collect_plan_info(ke, &info)) {
    return 0;
  }
  return info.n_axes;
}

fn u32 tile_loop_axis_type(KernelEntry const *ke, u32 axis) {
  TilePlanInfo info;
  if (!tile_collect_plan_info(ke, &info) || axis >= info.n_axes) {
    return 0;
  }
  return info.axis_types[axis];
}

fn u32 tile_loop_axis_extent(KernelEntry const *ke, u32 axis) {
  TilePlanInfo info;
  if (!tile_collect_plan_info(ke, &info) || axis >= info.n_axes) {
    return 0;
  }
  return info.axis_extents[axis];
}

#define TILE_REDUCE_KIND(arg)  (((arg) >> 24) & 0xFFu)
#define TILE_REDUCE_INNER(arg) ((arg) & 0xFFFFFFu)
#define TILE_MMA_A(arg)        ((u32)((arg) & 0xFFFFu))
#define TILE_MMA_B(arg)        ((u32)(((arg) >> 16) & 0xFFFFu))
#define TILE_MMA_FLAGS(arg)    ((u32)(((arg) >> 32) & 0xFFu))

fn int tile_mma_size_supported(u32 tile) {
  return tile == 8 || tile == 16 || tile == 32;
}

static u64 tile_mma_pack(u32 a_input, u32 b_input, u32 flags) {
  return ((u64)(flags & 0xFFu) << 32)
       | ((u64)(b_input & 0xFFFFu) << 16)
       | (u64)(a_input & 0xFFFFu);
}

static u32 tile_mma_size_from_opts(KernelEntry const *ke) {
  u32 tile = 16;
  if (ke == NULL || ke->axes == NULL) {
    return tile;
  }
  for (u32 i = 0; i < ke->axes->n_applied; i++) {
    KOpt opt = ke->axes->applied_opts[i];
    if (opt.op == KOP_TC && tile_mma_size_supported(opt.arg)) {
      tile = opt.arg;
    }
  }
  return tile;
}

static int tile_gemm_op_is_mul_inputs(KProgOp const *p) {
  if (p->opcode != UOP_MUL || p->n_src != 2) {
    return 0;
  }
  return KSRC_IS_INPUT(p->src[0]) && KSRC_IS_INPUT(p->src[1]);
}

static int tile_gemm_op_is_reduce_sum_of(KProgOp const *p, u32 src_step) {
  if (p->opcode != UOP_REDUCE || p->n_src != 1) {
    return 0;
  }
  if (TILE_REDUCE_KIND(p->arg) != REDUCE_SUM) {
    return 0;
  }
  if (KSRC_IS_INPUT(p->src[0])) {
    return 0;
  }
  return KSRC_INDEX(p->src[0]) == src_step;
}

static int tile_gemm_uniform_dtype(KernelEntry const *ke, u32 *out_dtype) {
  if (ke->n_ops == 0) {
    return 0;
  }
  u32 dt = ke->program[0].dtype;
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].dtype != dt) {
      return 0;
    }
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != dt) {
      return 0;
    }
  }
  if (ke->output_dtype != dt) {
    return 0;
  }
  *out_dtype = dt;
  return 1;
}

static int tile_gemm_views_ok(KernelEntry const *ke, u32 aidx, u32 bidx,
                              u32 M, u32 N, u32 K,
                              u32 *ldA, u32 *ldB, u32 *flags) {
  *ldA = K;
  *ldB = N;
  *flags = 0;
  if (ke->input_views == NULL) {
    return 1;
  }

  View const *va = &ke->input_views[aidx];
  View const *vb = &ke->input_views[bidx];

  if (M == 1 && N == 1) {
    return 1;
  }

  if (N == 1) {
    if (va->shape.ndim != 2) {
      return 0;
    }
    if (!(va->shape.dims[0] == M && va->shape.dims[1] == K
          && va->strides[0] == (i32)K && va->strides[1] == 1)) {
      return 0;
    }
    int b_vector = vb->shape.ndim == 1
                && vb->shape.dims[0] == K
                && vb->strides[0] == 1;
    int b_row = vb->shape.ndim == 2
             && vb->shape.dims[0] == 1 && vb->shape.dims[1] == K
             && vb->strides[1] == 1;
    int b_broadcast = vb->shape.ndim == 2
                   && vb->shape.dims[0] == M && vb->shape.dims[1] == K
                   && vb->strides[0] == 0 && vb->strides[1] == 1;
    if (!b_vector && !b_row && !b_broadcast) {
      return 0;
    }
    *ldA = K;
    *ldB = 1;
    return 1;
  }

  if (va->shape.ndim != 3 || vb->shape.ndim != 3) {
    return 0;
  }

  if (va->shape.dims[0] != M || va->shape.dims[1] != K
      || va->shape.dims[2] != N) {
    return 0;
  }
  if (vb->shape.dims[0] != M || vb->shape.dims[1] != K
      || vb->shape.dims[2] != N) {
    return 0;
  }

  if (va->strides[0] == (i32)K && va->strides[1] == 1
      && va->strides[2] == 0) {
    *ldA = K;
  } else if (va->strides[0] == 1 && va->strides[1] == (i32)M
             && va->strides[2] == 0) {
    *ldA = M;
    *flags |= 1u;
  } else {
    return 0;
  }

  if (vb->strides[0] == 0 && vb->strides[1] == (i32)N
      && vb->strides[2] == 1) {
    *ldB = N;
  } else if (vb->strides[0] == 0 && vb->strides[1] == 1
             && vb->strides[2] == (i32)K) {
    *ldB = K;
    *flags |= 2u;
  } else {
    return 0;
  }

  return 1;
}

static int tile_gemm_candidate_ok(KernelEntry const *ke,
                                  u32 const *input_storage_numels,
                                  u32 aidx, u32 bidx,
                                  u32 M, u32 N, u32 K,
                                  TileGemmInfo *out) {
  if (aidx >= ke->n_inputs || bidx >= ke->n_inputs || aidx == bidx) {
    return 0;
  }
  if (input_storage_numels != NULL) {
    u32 ea = input_storage_numels[aidx];
    u32 eb = input_storage_numels[bidx];
    if (M == 1 && N == 1) {
      if (ea != K || eb != K) {
        return 0;
      }
    } else if (ea != M * K || eb != K * N) {
      return 0;
    }
  }

  u32 ldA = K;
  u32 ldB = N;
  u32 flags = 0;
  if (!tile_gemm_views_ok(ke, aidx, bidx, M, N, K, &ldA, &ldB, &flags)) {
    return 0;
  }

  out->M       = M;
  out->N       = N;
  out->K       = K;
  out->a_input = aidx;
  out->b_input = bidx;
  out->ldA     = ldA;
  out->ldB     = ldB;
  out->flags   = flags;
  out->tile_size = 16;
  return 1;
}

static int tile_gemv_expand_input(KernelEntry const *ke, u32 step,
                                  u32 M, u32 K, u32 *slot) {
  if (step >= ke->n_ops) {
    return 0;
  }
  KProgOp const *p = &ke->program[step];
  if (p->opcode != UOP_EXPAND || p->n_src != 1
      || !KSRC_IS_INPUT(p->src[0]) || p->numel != M * K) {
    return 0;
  }
  if (p->out_ndim != 2 || p->out_dims[0] != M || p->out_dims[1] != K) {
    return 0;
  }
  int rank1_vec = p->src0_ndim == 1 && p->src0_dims[0] == K;
  int rank2_row = p->src0_ndim == 2
               && p->src0_dims[0] == 1 && p->src0_dims[1] == K;
  if (!rank1_vec && !rank2_row) {
    return 0;
  }
  *slot = KSRC_INDEX(p->src[0]);
  return *slot < ke->n_inputs;
}

static int tile_gemv_mul_input_and_expand(KernelEntry const *ke, u32 step,
                                          u32 expand_step, u32 *matrix_slot) {
  if (step >= ke->n_ops) {
    return 0;
  }
  KProgOp const *p = &ke->program[step];
  if (p->opcode != UOP_MUL || p->n_src != 2) {
    return 0;
  }
  for (u32 i = 0; i < 2; i++) {
    u32 a = p->src[i];
    u32 b = p->src[1 - i];
    if (KSRC_IS_INPUT(a) && !KSRC_IS_INPUT(b) && KSRC_INDEX(b) == expand_step) {
      *matrix_slot = KSRC_INDEX(a);
      return *matrix_slot < ke->n_inputs;
    }
  }
  return 0;
}

static int tile_analyze_expanded_gemv(KernelEntry const *ke,
                                      u32 const *input_storage_numels,
                                      u32 dtype, TileGemmInfo *out) {
  if (ke->n_inputs != 2 || ke->n_ops != 3) {
    return 0;
  }
  if (!tile_gemm_op_is_reduce_sum_of(&ke->program[2], 1)) {
    return 0;
  }
  u32 inner = TILE_REDUCE_INNER(ke->program[2].arg);
  u32 n_mul = ke->program[1].numel;
  u32 n_out = ke->program[2].numel;
  if (inner != 1 || n_out == 0 || n_mul == 0 || n_mul % n_out != 0) {
    return 0;
  }
  u32 M = n_out;
  u32 N = 1;
  u32 K = n_mul / n_out;
  u32 aidx = 0;
  u32 bidx = 0;
  if (!tile_gemv_expand_input(ke, 0, M, K, &bidx)) {
    return 0;
  }
  if (!tile_gemv_mul_input_and_expand(ke, 1, 0, &aidx)) {
    return 0;
  }
  if (!tile_gemm_candidate_ok(ke, input_storage_numels, aidx, bidx,
                              M, N, K, out)) {
    return 0;
  }
  out->dtype = dtype;
  return 1;
}

int tile_analyze_gemm(KernelEntry const *ke,
                      u32 const *input_storage_numels,
                      TileGemmInfo *out) {
  if (ke == NULL || out == NULL || ke->program == NULL
      || ke->n_inputs != 2) {
    return 0;
  }
  memset(out, 0, sizeof(TileGemmInfo));
  u32 dtype = 0;
  if (!tile_gemm_uniform_dtype(ke, &dtype)) {
    return 0;
  }
  if (tile_analyze_expanded_gemv(ke, input_storage_numels, dtype, out)) {
    return 1;
  }
  if (ke->n_ops != 2) {
    return 0;
  }
  if (!tile_gemm_op_is_mul_inputs(&ke->program[0])) {
    return 0;
  }
  if (!tile_gemm_op_is_reduce_sum_of(&ke->program[1], 0)) {
    return 0;
  }

  u32 inner = TILE_REDUCE_INNER(ke->program[1].arg);
  u32 n_mul = ke->program[0].numel;
  u32 n_out = ke->program[1].numel;
  if (inner == 0 || n_out == 0 || n_mul == 0) {
    return 0;
  }
  if (n_mul % n_out != 0 || n_out % inner != 0) {
    return 0;
  }

  u32 N = inner;
  u32 K = n_mul / n_out;
  u32 M = n_out / N;
  if (M == 0 || N == 0 || K == 0) {
    return 0;
  }

  out->dtype = dtype;
  u32 first  = KSRC_INDEX(ke->program[0].src[0]);
  u32 second = KSRC_INDEX(ke->program[0].src[1]);

  if (tile_gemm_candidate_ok(ke, input_storage_numels, first, second,
                             M, N, K, out)) {
    out->dtype = dtype;
    return 1;
  }
  if (tile_gemm_candidate_ok(ke, input_storage_numels, second, first,
                             M, N, K, out)) {
    out->dtype = dtype;
    return 1;
  }
  return 0;
}

static u32 tile_isqrt_exact(u32 x) {
  for (u32 r = 1; r <= x / r; r++) {
    if (r * r == x) {
      return r;
    }
  }
  return 0;
}

static u32 tile_conv2d_threads_from_opts(KernelEntry const *ke) {
  u32 threads = 256;
  if (ke == NULL || ke->axes == NULL) {
    return threads;
  }
  for (u32 i = 0; i < ke->axes->n_applied; i++) {
    KOpt opt = ke->axes->applied_opts[i];
    if (opt.op == KOP_LOCAL && opt.arg > 0 && opt.arg <= 256) {
      threads = opt.arg;
    }
  }
  return threads;
}

static u32 tile_conv2d_outputs_from_opts(KernelEntry const *ke) {
  u32 outputs = 1;
  if (ke == NULL || ke->axes == NULL) {
    return outputs;
  }
  for (u32 i = 0; i < ke->axes->n_applied; i++) {
    KOpt opt = ke->axes->applied_opts[i];
    if (opt.op == KOP_UPCAST && opt.arg > 0 && opt.arg <= 16) {
      outputs = opt.arg;
    }
  }
  return outputs;
}

static u32 tile_conv2d_reduce_unroll_from_opts(KernelEntry const *ke,
                                               u32 reduce_extent) {
  u32 unroll = 1;
  if (ke == NULL || ke->axes == NULL || reduce_extent == 0) {
    return unroll;
  }
  for (u32 i = 0; i < ke->axes->n_applied; i++) {
    KOpt opt = ke->axes->applied_opts[i];
    if (opt.op == KOP_UNROLL && opt.arg > 1 && opt.arg <= 16
        && reduce_extent % opt.arg == 0) {
      unroll = opt.arg;
    }
  }
  return unroll;
}

static int tile_analyze_conv2d_flat_impl(KernelEntry const *ke,
                                         TileConv2DInfo *out,
                                         int allow_cin1) {
  if (ke == NULL || out == NULL || ke->n_inputs < 2 || ke->n_ops == 0
      || ke->program == NULL || ke->input_views == NULL) {
    return 0;
  }
  if (ke->input_dtypes[0] != DT_FP32 || ke->output_dtype != DT_FP32) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }
  KProgOp const *last = &ke->program[ke->n_ops - 1];
  if (last->opcode != UOP_REDUCE || last->n_src != 1
      || TILE_REDUCE_KIND(last->arg) != REDUCE_SUM) {
    return 0;
  }

  View const *wv = &ke->input_views[0];
  if (wv->shape.ndim != 3
      || (ke->output_shape.ndim != 2 && ke->output_shape.ndim != 3)) {
    return 0;
  }
  u32 c_out = wv->shape.dims[0];
  u32 k     = wv->shape.dims[1];
  u32 p     = wv->shape.dims[2];
  u32 batch = 1;
  u32 c_in = 0, h = 0, w = 0;
  i32 x_offset = 0;
  i32 x_stride_b = 0;
  i32 x_stride0 = 0, x_stride1 = 0, x_stride2 = 0;
  u32 patch_base = 0, patch_count = 0;
  if (c_out == 0 || k == 0 || p == 0) {
    return 0;
  }
  u32 kh = 0, kw = 0, h_out = 0, w_out = 0, spatial_patches = 0;
  int direct_x = ke->n_inputs == 2;
  if (direct_x) {
    View const *xv = &ke->input_views[1];
    if (xv->shape.ndim != 3 && xv->shape.ndim != 4) {
      return 0;
    }
    if (xv->shape.ndim == 3) {
      c_in      = xv->shape.dims[0];
      h         = xv->shape.dims[1];
      w         = xv->shape.dims[2];
      x_offset  = xv->offset;
      x_stride0 = xv->strides[0];
      x_stride1 = xv->strides[1];
      x_stride2 = xv->strides[2];
    } else {
      batch      = xv->shape.dims[0];
      c_in       = xv->shape.dims[1];
      h          = xv->shape.dims[2];
      w          = xv->shape.dims[3];
      x_offset   = xv->offset;
      x_stride_b = xv->strides[0];
      x_stride0  = xv->strides[1];
      x_stride1  = xv->strides[2];
      x_stride2  = xv->strides[3];
    }
    if (c_in == 0 || (c_in == 1 && !allow_cin1) || k % c_in != 0) {
      return 0;
    }
    u32 k_spatial = k / c_in;
    kh = tile_isqrt_exact(k_spatial);
    if (kh == 0) {
      return 0;
    }
    kw = kh;
    if (h < kh || w < kw) {
      return 0;
    }
    h_out = h - kh + 1;
    w_out = w - kw + 1;
    spatial_patches = h_out * w_out;
  } else {
    c_in = 1;
    kh = tile_isqrt_exact(k);
    if (kh == 0) {
      return 0;
    }
    kw = kh;
    patch_base = 1;
    patch_count = k;
    if (ke->n_inputs != 1 + patch_count) {
      return 0;
    }
    View const *pv0 = &ke->input_views[patch_base];
    if (pv0->shape.ndim == 3) {
      if (pv0->shape.dims[0] != 1) {
        return 0;
      }
      h_out = pv0->shape.dims[1];
      w_out = pv0->shape.dims[2];
    } else if (pv0->shape.ndim == 4) {
      if (pv0->shape.dims[0] != 1) {
        return 0;
      }
      batch = pv0->shape.dims[1];
      h_out = pv0->shape.dims[2];
      w_out = pv0->shape.dims[3];
    } else {
      return 0;
    }
    for (u32 i = 0; i < patch_count; i++) {
      View const *pv = &ke->input_views[patch_base + i];
      if (pv->shape.ndim != pv0->shape.ndim) {
        return 0;
      }
      if (pv->shape.ndim == 3) {
        if (pv->shape.dims[0] != 1 || pv->shape.dims[1] != h_out
            || pv->shape.dims[2] != w_out) {
          return 0;
        }
      } else if (pv->shape.dims[0] != 1 || pv->shape.dims[1] != batch
          || pv->shape.dims[2] != h_out || pv->shape.dims[3] != w_out) {
        return 0;
      }
    }
    h = h_out + kh - 1;
    w = w_out + kw - 1;
    spatial_patches = h_out * w_out;
  }
  if (kh == 0) {
    return 0;
  }
  if (batch == 0 || spatial_patches == 0 || batch * spatial_patches != p) {
    return 0;
  }
  int output_flat = ke->output_shape.ndim == 2
                 && ke->output_shape.dims[0] == c_out
                 && ke->output_shape.dims[1] == p;
  int output_chw = ke->output_shape.ndim == 3
                && ke->output_shape.dims[0] == c_out
                && ke->output_shape.dims[1] == h_out
                && ke->output_shape.dims[2] == w_out;
  if ((!output_flat && !output_chw) || ke->output_numel != c_out * p) {
    return 0;
  }
  if (wv->strides[2] != 0) {
    return 0;
  }

  memset(out, 0, sizeof(TileConv2DInfo));
  out->dtype     = DT_FP32;
  out->w_input   = 0;
  out->x_input   = direct_x ? 1 : 0;
  out->patch_input_base  = patch_base;
  out->patch_input_count = patch_count;
  out->c_out     = c_out;
  out->c_in      = c_in;
  out->h         = h;
  out->w         = w;
  out->kh        = kh;
  out->kw        = kw;
  out->h_out     = h_out;
  out->w_out     = w_out;
  out->batch     = batch;
  out->patches   = p;
  out->spatial_patches = spatial_patches;
  out->w_offset  = wv->offset;
  out->w_stride0 = wv->strides[0];
  out->w_stride1 = wv->strides[1];
  out->x_offset  = x_offset;
  out->x_stride_b = x_stride_b;
  out->x_stride0 = x_stride0;
  out->x_stride1 = x_stride1;
  out->x_stride2 = x_stride2;
  out->threads   = tile_conv2d_threads_from_opts(ke);
  out->outputs_per_thread = tile_conv2d_outputs_from_opts(ke);
  out->reduce_unroll = tile_conv2d_reduce_unroll_from_opts(ke, k);
  return 1;
}

int tile_analyze_conv2d_flat(KernelEntry const *ke, TileConv2DInfo *out) {
  return tile_analyze_conv2d_flat_impl(ke, out, 0);
}

int tile_rejects_conv2d_flat_cin1(KernelEntry const *ke) {
  TileConv2DInfo conv;
  if (!tile_analyze_conv2d_flat_impl(ke, &conv, 1)) {
    return 0;
  }
  return conv.c_in == 1 && conv.patch_input_count == 0;
}

static int tile_collect_axis_info(KernelEntry const *ke, u32 axis_id,
                                  u32 *axis_type, u32 *extent) {
  if (!tile_id_ok(ke, axis_id)) {
    return 0;
  }
  TileUop const *axis = &ke->tile_uops[axis_id];
  TileAxisInfo info = tile_axis_unpack(axis->extra);
  if (axis->op != TILE_AXIS || axis->src_count != 0
      || !tile_axis_type_ok(info.kax_type) || info.extent == 0) {
    return 0;
  }
  if (axis_type != NULL) {
    *axis_type = info.kax_type;
  }
  if (extent != NULL) {
    *extent = info.extent;
  }
  return 1;
}

static int tile_collect_mma_info(KernelEntry const *ke, u32 root_id,
                                 TileGemmInfo *out) {
  if (out == NULL || !tile_id_ok(ke, root_id)) {
    return 0;
  }
  TileUop const *root = &ke->tile_uops[root_id];
  if (root->op != TILE_MMA || root->src_count != 3) {
    return 0;
  }
  u32 axis_types[3] = {0};
  u32 extents[3] = {0};
  for (u32 i = 0; i < 3; i++) {
    if (!tile_collect_axis_info(ke, root->src[i],
                                &axis_types[i], &extents[i])) {
      return 0;
    }
  }
  if (axis_types[0] != KAX_LOOP || axis_types[1] != KAX_LOOP
      || axis_types[2] != KAX_REDUCE) {
    return 0;
  }

  TileGemmInfo gemm;
  if (!tile_analyze_gemm(ke, NULL, &gemm)) {
    return 0;
  }
  if (root->dtype != gemm.dtype
      || extents[0] != gemm.M || extents[1] != gemm.N
      || extents[2] != gemm.K
      || TILE_MMA_A(root->extra) != gemm.a_input
      || TILE_MMA_B(root->extra) != gemm.b_input
      || TILE_MMA_FLAGS(root->extra) != gemm.flags) {
    return 0;
  }
  gemm.tile_size = tile_mma_size_from_opts(ke);
  *out = gemm;
  return 1;
}

static u32 tile_find_nested_scalar_reduce(KernelEntry const *ke,
                                          u32 scalar_id,
                                          u32 depth) {
  if (ke == NULL || ke->scalar_uops == NULL || scalar_id == 0
      || scalar_id >= ke->n_scalar_uops || depth > ke->n_scalar_uops) {
    return 0;
  }
  ScalarUop const *u = &ke->scalar_uops[scalar_id];
  if (u->op == S_REDUCE_SUM || u->op == S_REDUCE_MAX) {
    return scalar_id;
  }
  for (u32 i = 0; i < u->src_count && i < SCALAR_MAX_SRC; i++) {
    u32 hit = tile_find_nested_scalar_reduce(ke, u->src[i], depth + 1);
    if (hit != 0) {
      return hit;
    }
  }
  return 0;
}

fn int tile_validate(KernelEntry const *ke) {
  if (ke == NULL || ke->tile_uops == NULL || ke->n_tile_uops == 0) {
    return 0;
  }
  if (ke->tile_uops[0].op != TILE_NONE) {
    return 0;
  }
  if (!tile_id_ok(ke, ke->tile_root)) {
    return 0;
  }

  TileUop const *root = &ke->tile_uops[ke->tile_root];
  if (root->op == TILE_MMA) {
    TileGemmInfo gemm;
    return tile_collect_mma_info(ke, ke->tile_root, &gemm);
  }
  if (root->op != TILE_LOOP_NEST || root->src_count < 2
      || root->src_count > TILE_MAX_SRC) {
    return 0;
  }

  u32 store_id = root->src[0];
  if (!tile_id_ok(ke, store_id)) {
    return 0;
  }
  TileUop const *store = &ke->tile_uops[store_id];
  if (store->op != TILE_STORE || store->src_count != 1) {
    return 0;
  }
  u32 scalar_store = (u32)store->extra;
  if (ke->scalar_uops == NULL || scalar_store == 0
      || scalar_store >= ke->n_scalar_uops
      || ke->scalar_uops[scalar_store].op != S_STORE
      || ke->scalar_uops[scalar_store].src_count < 2) {
    return 0;
  }

  u32 scalar_value = ke->scalar_uops[scalar_store].src[1];
  u32 reduce_id = 0;
  u32 body_id = store->src[0];
  if (!tile_id_ok(ke, body_id)) {
    return 0;
  }
  TileUop const *body = &ke->tile_uops[body_id];
  if (body->op == TILE_REDUCE) {
    reduce_id = body_id;
    if (body->src_count != 1) {
      return 0;
    }
    u32 scalar_reduce = (u32)body->extra;
    if (scalar_reduce == 0 || scalar_reduce >= ke->n_scalar_uops
        || scalar_reduce != scalar_value) {
      return 0;
    }
    ScalarUop const *ru = &ke->scalar_uops[scalar_reduce];
    if ((ru->op != S_REDUCE_SUM && ru->op != S_REDUCE_MAX)
        || ru->src_count < 2 || ru->src[0] == 0
        || ru->src[0] >= ke->n_scalar_uops) {
      return 0;
    }
    body_id = body->src[0];
    if (!tile_id_ok(ke, body_id)) {
      return 0;
    }
    body = &ke->tile_uops[body_id];
    scalar_value = ru->src[0];
  }
  if (body->op != TILE_SCALAR_BODY || body->src_count != 0) {
    return 0;
  }
  if (scalar_value == 0 || scalar_value >= ke->n_scalar_uops) {
    return 0;
  }
  if (reduce_id == 0) {
    u8 value_op = ke->scalar_uops[scalar_value].op;
    if (value_op == S_REDUCE_SUM || value_op == S_REDUCE_MAX) {
      return 0;
    }
  }
  if ((reduce_id == 0 && ke->scalar_uops[scalar_store].src[1] != scalar_value)
      || (u32)body->extra != scalar_value) {
    return 0;
  }
  u32 scalar_index = ke->scalar_uops[scalar_store].src[0];
  if (scalar_index == 0 || scalar_index >= ke->n_scalar_uops) {
    return 0;
  }
  u8 scalar_index_op = ke->scalar_uops[scalar_index].op;
  if (scalar_index_op != S_INDEX && scalar_index_op != S_INDEX_E) {
    return 0;
  }

  for (u32 i = 1; i < root->src_count; i++) {
    u32 axis_id = root->src[i];
    if (!tile_collect_axis_info(ke, axis_id, NULL, NULL)) {
      return 0;
    }
  }
  return 1;
}

fn int tile_collect_plan_info(KernelEntry const *ke, TilePlanInfo *out) {
  if (out == NULL) {
    return 0;
  }
  memset(out, 0, sizeof(TilePlanInfo));
  if (!tile_validate(ke)) {
    return 0;
  }

  TileUop const *root  = &ke->tile_uops[ke->tile_root];
  if (root->op == TILE_MMA) {
    TileGemmInfo gemm;
    if (!tile_collect_mma_info(ke, ke->tile_root, &gemm)) {
      return 0;
    }
    out->root_id     = ke->tile_root;
    out->dtype       = root->dtype;
    out->n_axes      = 3;
    out->mma_tile_id = ke->tile_root;
    out->mma         = gemm;
    for (u32 i = 0; i < out->n_axes; i++) {
      u32 axis_id = root->src[i];
      TileUop const *axis = &ke->tile_uops[axis_id];
      TileAxisInfo info = tile_axis_unpack(axis->extra);
      out->axis_ids    [i] = axis_id;
      out->axis_types  [i] = info.kax_type;
      out->axis_extents[i] = info.extent;
    }
    return 1;
  }

  u32 store_tile_id    = root->src[0];
  TileUop const *store = &ke->tile_uops[store_tile_id];
  u32 reduce_tile_id   = 0;
  u32 body_tile_id     = store->src[0];
  u32 scalar_store_id  = (u32)store->extra;
  ScalarUop const *su  = &ke->scalar_uops[scalar_store_id];
  u32 scalar_value_id      = su->src[1];
  u32 scalar_body_value_id = scalar_value_id;
  u32 scalar_reduce_id     = 0;

  TileUop const *body_or_reduce = &ke->tile_uops[body_tile_id];
  if (body_or_reduce->op == TILE_REDUCE) {
    reduce_tile_id = body_tile_id;
    scalar_reduce_id = (u32)body_or_reduce->extra;
    ScalarUop const *ru = &ke->scalar_uops[scalar_reduce_id];
    scalar_body_value_id = ru->src[0];
    body_tile_id = body_or_reduce->src[0];
  } else {
    scalar_reduce_id = tile_find_nested_scalar_reduce(ke, scalar_value_id, 0);
    if (scalar_reduce_id != 0) {
      ScalarUop const *ru = &ke->scalar_uops[scalar_reduce_id];
      scalar_body_value_id = ru->src[0];
    }
  }

  out->root_id              = ke->tile_root;
  out->store_tile_id        = store_tile_id;
  out->reduce_tile_id       = reduce_tile_id;
  out->body_tile_id         = body_tile_id;
  out->scalar_store_id      = scalar_store_id;
  out->scalar_index_id      = su->src[0];
  out->scalar_value_id      = scalar_value_id;
  out->scalar_body_value_id = scalar_body_value_id;
  out->scalar_reduce_id     = scalar_reduce_id;
  out->dtype                = root->dtype;
  out->n_axes               = (u32)root->src_count - 1;

  for (u32 i = 0; i < out->n_axes; i++) {
    u32 axis_id = root->src[1 + i];
    TileUop const *axis = &ke->tile_uops[axis_id];
    TileAxisInfo info = tile_axis_unpack(axis->extra);
    out->axis_ids    [i] = axis_id;
    out->axis_types  [i] = info.kax_type;
    out->axis_extents[i] = info.extent;
  }
  return 1;
}

static int tile_build_mma_from_gemm(KernelEntry *ke,
                                    TileGemmInfo const *gemm) {
  if (ke == NULL || gemm == NULL || gemm->M == 0 || gemm->N == 0
      || gemm->K == 0) {
    return 0;
  }

  TileGemmInfo keep = *gemm;
  tile_free(ke);

  u32 axes[3];
  TileAxisInfo m_info = { KAX_LOOP,   keep.M, 0, 0 };
  TileAxisInfo n_info = { KAX_LOOP,   keep.N, 0, 0 };
  TileAxisInfo k_info = { KAX_REDUCE, keep.K, 0, 0 };
  axes[0] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(m_info));
  axes[1] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(n_info));
  axes[2] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(k_info));
  ke->tile_root = tile_emit(ke, TILE_MMA, keep.dtype, 3, axes,
                            tile_mma_pack(keep.a_input, keep.b_input,
                                          keep.flags));
  if (!tile_validate(ke)) {
    tile_free(ke);
    return 0;
  }
  ke->tile_axes_version = ke->axes != NULL ? ke->axes->version : 0;
  return 1;
}

static u32 tile_find_scalar_bufferize(KernelEntry const *ke) {
  if (ke->scalar_uops == NULL) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) {
      return i;
    }
  }
  return 0;
}

static u32 tile_emit_axes_from_kernel_axes(KernelEntry *ke, u32 *out, u32 cap) {
  if (ke->axes == NULL) {
    return 0;
  }
  if (ke->axes->n_axes == 0) {
    axes_default_for(ke);
  }
  if (ke->axes->n_axes == 0 || ke->axes->n_axes > cap) {
    return 0;
  }

  for (u32 i = 0; i < ke->axes->n_axes; i++) {
    TileAxisInfo info = {
      .kax_type     = ke->axes->axis_types[i],
      .extent       = ke->axes->full_shape[i],
      .memory_scope = 0,
      .vector_width = 0,
    };
    out[i] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(info));
  }
  return ke->axes->n_axes;
}

static u32 tile_emit_axes_from_scalar_root(KernelEntry *ke, u32 root,
                                           u32 *out, u32 cap) {
  ScalarUop const *buf = &ke->scalar_uops[root];
  if (buf->op != S_BUFFERIZE || buf->src_count == 0) {
    return 0;
  }
  u32 n_axes = (u32)buf->src_count - 1;
  if (n_axes == 0 || n_axes > cap) {
    return 0;
  }

  for (u32 i = 0; i < n_axes; i++) {
    u32 rid = buf->src[1 + i];
    if (rid == 0 || rid >= ke->n_scalar_uops) {
      return 0;
    }
    ScalarUop const *r = &ke->scalar_uops[rid];
    if (r->op != S_RANGE) {
      return 0;
    }
    u32 scalar_axis = (u32)((r->extra >> 32) & 0xFFFFFFFFu);
    u32 extent      = (u32)(r->extra & 0xFFFFFFFFu);
    TileAxisInfo info = {
      .kax_type     = tile_axis_from_scalar_axis(scalar_axis),
      .extent       = extent,
      .memory_scope = 0,
      .vector_width = 0,
    };
    out[i] = tile_emit_leaf(ke, TILE_AXIS, DT_INT64, tile_axis_pack(info));
  }
  return n_axes;
}

fn int tile_build_from_scalar(KernelEntry *ke) {
  u32 root = tile_find_scalar_bufferize(ke);
  if (root == 0) {
    return 0;
  }
  if (ke->scalar_uops[root].src_count == 0) {
    return 0;
  }
  u32 scalar_store = ke->scalar_uops[root].src[0];
  if (scalar_store == 0 || scalar_store >= ke->n_scalar_uops
      || ke->scalar_uops[scalar_store].op != S_STORE
      || ke->scalar_uops[scalar_store].src_count < 2) {
    return 0;
  }
  u32 scalar_value = ke->scalar_uops[scalar_store].src[1];
  if (scalar_value == 0 || scalar_value >= ke->n_scalar_uops) {
    return 0;
  }
  u32 scalar_body_value = scalar_value;
  int has_reduce = 0;
  if (ke->scalar_uops[scalar_value].op == S_REDUCE_SUM
      || ke->scalar_uops[scalar_value].op == S_REDUCE_MAX) {
    if (ke->scalar_uops[scalar_value].src_count < 2
        || ke->scalar_uops[scalar_value].src[0] == 0
        || ke->scalar_uops[scalar_value].src[0] >= ke->n_scalar_uops) {
      return 0;
    }
    scalar_body_value = ke->scalar_uops[scalar_value].src[0];
    has_reduce = 1;
  }

  tile_free(ke);

  u32 body = tile_emit_leaf(ke, TILE_SCALAR_BODY,
                            ke->scalar_uops[scalar_body_value].dtype,
                            scalar_body_value);
  u32 store_value = body;
  if (has_reduce) {
    u32 reduce_src[1] = {body};
    store_value = tile_emit(ke, TILE_REDUCE,
                            ke->scalar_uops[scalar_value].dtype,
                            1, reduce_src, scalar_value);
  }
  u32 store_src[1] = {store_value};
  u32 store = tile_emit(ke, TILE_STORE, ke->scalar_uops[scalar_store].dtype,
                        1, store_src, scalar_store);
  u32 axes[MAX_AXES];
  u32 n_axes = tile_emit_axes_from_kernel_axes(ke, axes, MAX_AXES);
  if (n_axes == 0) {
    n_axes = tile_emit_axes_from_scalar_root(ke, root, axes, MAX_AXES);
  }
  if (n_axes == 0) {
    tile_free(ke);
    return 0;
  }

  u32 src[TILE_MAX_SRC] = {store};
  for (u32 i = 0; i < n_axes; i++) {
    src[1 + i] = axes[i];
  }
  ke->tile_root = tile_emit(ke, TILE_LOOP_NEST, ke->scalar_uops[root].dtype,
                            (u8)(1 + n_axes), src, 0);
  if (!tile_validate(ke)) {
    tile_free(ke);
    return 0;
  }
  ke->tile_axes_version = ke->axes != NULL ? ke->axes->version : 0;
  return 1;
}

fn int tile_sync_from_scalar(KernelEntry *ke) {
  if (ke == NULL) {
    return 0;
  }
  TileGemmInfo gemm;
  int wants_mma = tile_analyze_gemm(ke, NULL, &gemm);
  u32 axes_version = ke->axes != NULL ? ke->axes->version : 0;
  if (ke->tile_uops != NULL && ke->tile_axes_version == axes_version
      && tile_validate(ke)) {
    if (!wants_mma || ke->tile_uops[ke->tile_root].op == TILE_MMA) {
      return 1;
    }
  }
  if (wants_mma) {
    return tile_build_mma_from_gemm(ke, &gemm);
  }
  if (ke->scalar_uops == NULL) {
    return 0;
  }
  return tile_build_from_scalar(ke);
}

int tile_collect_mma_plan(KernelEntry *ke, TileGemmInfo *out) {
  if (out == NULL) {
    return 0;
  }
  memset(out, 0, sizeof(TileGemmInfo));
  if (!tile_sync_from_scalar(ke)) {
    return 0;
  }
  TilePlanInfo plan;
  if (!tile_collect_plan_info(ke, &plan) || plan.mma_tile_id == 0) {
    return 0;
  }
  *out = plan.mma;
  return 1;
}
