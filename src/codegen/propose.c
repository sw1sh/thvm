// codegen/propose.c - the candidate KOpts the autotune search benches for a
// kernel, chosen by shape (tinygrad's BEAM actions, codegen/opt/search.py:15-23).

static u32 propose_reduce_axis_size_from_dag(KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  return uop_dag_reduce_axis_extent(ke->cached_lift.store_root);
}

static u32 propose_append_unique(KOpt *out, u32 n, u32 cap, KOpt cand);

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
  return 1;
}

static u32 propose_reduce_axis_size(KernelEntry const *ke) {
  return propose_reduce_axis_size_from_dag(ke);
}

// The slot (not axis_id) of the last KAX_REDUCE axis, or 0xFF.
static u8 propose_reduce_axis_index(KernelEntry const *ke) {
  u8  kax[MAX_AXES];
  u32 ext[MAX_AXES];
  u32 n = axes_collect(ke, kax, ext, MAX_AXES);
  for (i32 i = (i32)n - 1; i >= 0; i--) {
    if (kax[i] == KAX_REDUCE) {
      return (u8)i;
    }
  }
  return 0xFF;
}

// The axis_id (a KOpt names an id, not a slot) and extent of the last reduce
// axis in cached_lift_init_root, the root the bench applies candidates to: the
// live root's ids move with every split.  0xFFFFFFFF if none.
static u32 propose_reduce_axis_id(KernelEntry const *ke, u32 *out_extent) {
  if (out_extent != NULL) *out_extent = 0;
  if (ke == NULL) return 0xFFFFFFFFu;
  Term root = ke->cached_lift_init_root != 0 ? ke->cached_lift_init_root
                                             : ke->cached_lift.store_root;
  if (root == 0) return 0xFFFFFFFFu;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(root, ids, types, exts, MAX_AXES);
  u32 best_id = 0xFFFFFFFFu, best_ext = 0;
  for (u32 i = 0; i < n; i++) {
    if (types[i] != KAX_REDUCE) continue;
    if (best_id == 0xFFFFFFFFu || ids[i] > best_id) {  // last (largest-id) reduce
      best_id  = ids[i];
      best_ext = exts[i];
    }
  }
  if (out_extent != NULL) *out_extent = best_ext;
  return best_id;
}

// The first LOOP slot from `start` that `factor` splits, or 0xFF: leading unit
// axes are common in movement-heavy graphs.
static u8 propose_loop_axis_for_factor(KernelEntry const *ke, u8 start,
                                       u32 factor) {
  u8  kax[MAX_AXES];
  u32 ext[MAX_AXES];
  u32 n = axes_collect(ke, kax, ext, MAX_AXES);
  for (u8 i = start; i < n; i++) {
    if (kax[i] != KAX_LOOP) continue;
    if (factor <= ext[i] && ext[i] % factor == 0) {
      return i;
    }
  }
  return 0xFF;
}

// propose_loop_axis_for_factor over the DAG's RANGEs, returning an axis_id, for
// a hand-built KernelEntry: without a schedule axes_collect reads nothing.
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
  u8  kax[MAX_AXES];
  u32 ext[MAX_AXES];
  u32 n_axes = axes_collect(ke, kax, ext, MAX_AXES);
  for (u32 i = 0; i < n_local_factors && n < cap; i++) {
    u32 f = local_factors[i];
    for (u8 axis = 0; axis < n_axes; axis++) {
      if (kax[axis] != KAX_LOOP) continue;
      if (ext[axis] < f || ext[axis] % f != 0) continue;
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
  u8  kax[MAX_AXES];
  u32 ext[MAX_AXES];
  u32 n_axes = axes_collect(ke, kax, ext, MAX_AXES);
  for (u32 i = 0; i < n_upcast_factors && n < cap; i++) {
    u32 f = upcast_factors[i];
    for (u8 axis = 0; axis < n_axes; axis++) {
      if (kax[axis] != KAX_LOOP) continue;
      if (ext[axis] < f || ext[axis] % f != 0) continue;
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
  u32 axis_size = 0;
  u32 axis = propose_reduce_axis_id(ke, &axis_size);
  if (axis == 0xFFFFFFFFu || axis_size == 0) {
    return n;
  }
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

// Split-K for the conv-reduce kernel, which reduces C_in serially in one thread:
// GROUPTOP factors dividing the reduce extent (tinygrad's GROUP and GROUPTOP
// actions, search.py:16-17).  The bench keeps one only if correct and faster.
static u32 propose_conv2d_group_opts(KernelEntry const *ke, KOpt *out,
                                     u32 n, u32 cap) {
  static const u32 group_factors[] = {32, 16, 8, 4, 2};
  u32 axis_size = 0;
  u32 axis = propose_reduce_axis_id(ke, &axis_size);
  if (axis == 0xFFFFFFFFu || axis_size == 0) return n;
  u32 n_group_factors = sizeof(group_factors)/sizeof(*group_factors);
  for (u32 i = 0; i < n_group_factors && n < cap; i++) {
    u32 f = group_factors[i];
    if (axis_size < f || axis_size % f != 0) {
      continue;
    }
    out[n].op   = KOP_GROUPTOP;
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
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) {
      return 0;
    }
  }
  if (ke->cached_lift.store_root == 0) return 0;
  if (!uop_dag_dtype_uniform(ke->cached_lift.store_root, DT_FP32)) {
    return 0;
  }
  // A matmul's K is the simdgroup contraction axis: splitting it breaks the
  // simdgroup_load addresses as an M/N split does (propose_rich_action_space).
  Term base = ke->cached_lift_init_root != 0 ? ke->cached_lift_init_root
                                             : ke->cached_lift.store_root;
  if (uop_classify_matmul(base, NULL, NULL)) {
    return 0;
  }
  return uop_dag_is_reduce_unroll_kernel(ke->cached_lift.store_root);
}

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

// tinygrad's BEAM actions (codegen/opt/search.py:15-23) over the base kernel's
// axis_ids, without TC, SWAP, PADTO and the top LOCAL splits; kernel_apply_opt
// rejects what does not fit, as get_kernel_actions (search.py:85-103) does.
static u32 propose_rich_action_space(KernelEntry const *ke, KOpt *out,
                                     u32 n, u32 cap) {
  if (ke == NULL) return n;
  // cached_lift_init_root, the pre-heuristic root the bench applies to, as
  // tinygrad's actions start from the base kernel; store_root when there is none.
  Term base = ke->cached_lift_init_root != 0 ? ke->cached_lift_init_root
                                             : ke->cached_lift.store_root;
  if (base == 0) return n;

  // No splits on a matmul: the simdgroup_matrix tile addresses M/N/K by their
  // un-split ids, so a split leaves it naming an undeclared axis; KOP_TC owns it.
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

  // Matmuls get no candidates: hand_opts' KOP_TC with its M/N KOP_GLOBAL promote
  // is the whole TC config, and a lone KOP_TC on the reset root mis-tiles.
  if (propose_metal_backend_enabled() && ke->cached_lift.store_root != 0) {
    u32 dtype = 0;
    UopDagGemmShape shape;
    if (propose_tc_classify(ke, &dtype, &shape)
        && (dtype == DT_FP32 || dtype == DT_BF16)) {
      return 0;
    }
  }
  if (propose_cuda_backend_enabled() && ke->cached_lift.store_root != 0) {
    u32 dtype = 0;
    UopDagGemmShape shape;
    if (propose_tc_classify(ke, &dtype, &shape)
        && (dtype == DT_FP32 || dtype == DT_BF16)) {
      return 0;
    }
  }

  if (propose_metal_tile_enabled() && ke->cached_lift.store_root != 0) {
    TileConv2DInfo conv;
    if (tile_analyze_conv2d_flat(ke, &conv)) {
      n = propose_conv2d_local_opts(ke, out, n, cap);
      n = propose_conv2d_upcast_opts(ke, out, n, cap);
      n = propose_conv2d_unroll_opts(ke, out, n, cap);
      return propose_conv2d_group_opts(ke, out, n, cap);
    }
  }

  u8  kax[MAX_AXES];
  u32 ext[MAX_AXES];
  u32 n_axes    = axes_collect(ke, kax, ext, MAX_AXES);
  u8  axis_idx  = propose_reduce_axis_index(ke);
  u32 axis_size = axis_idx == 0xFF ? propose_reduce_axis_size(ke)
                : axis_idx < n_axes ? ext[axis_idx] : 0;
  // UNROLL names the reduce axis by axis_id; the slot is only a fallback.
  u32 reduce_aid = propose_reduce_axis_id(ke, NULL);
  u32 reduce_axis = (reduce_aid != 0xFFFFFFFFu) ? reduce_aid : (u32)axis_idx;
  if (axis_size > 0 && axis_idx != 0xFF
      && propose_metal_reduce_unroll_kernel(ke)) {
    for (u32 i = 0; i < n_factors; i++) {
      u32 f = split_factors[i];
      if (axis_size % f != 0) continue;
      if (n >= cap) break;
      out[n].op   = KOP_UNROLL;
      out[n].axis = reduce_axis;
      out[n].arg  = f;
      n++;
    }
  }

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

  // CUDA: a LOCAL output LOOP axis is the threadblock (cuda_dag_dispatch_shape),
  // which the rules above never offer here; reducing kernels get it too.
  if (propose_cuda_backend_enabled()) {
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
    // UPCAST: `factor` outputs per thread, in-thread, so cuda_dag_dispatch_shape
    // needs nothing new.  Only for reducing kernels: the elementwise rule has it.
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

  // Metal: tinygrad's BEAM action space on top; the bench keeps a candidate
  // only if it applies, stays correct and beats the heuristic.
  if (propose_metal_backend_enabled()) {
    n = propose_rich_action_space(ke, out, n, cap);
  }
  return n;
}
