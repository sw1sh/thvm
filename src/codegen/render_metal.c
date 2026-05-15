// Public bridge for WL FFI (thvmlink.c) and tests (test_bufferize)
// that want to peek at the rendered MSL without going through Metal
// dispatch. After the dispatch ladder collapse (88f536c3 / 4e30432b),
// the only Metal MSL emit path is metal_tile_jit_encode -> render_uop;
// cg_emit_metal forwards to cg_emit_tile_metal which routes there.
char *cg_emit_metal(KernelEntry const *ke) {
  return cg_emit_tile_metal((KernelEntry *)ke);
}


static int rmt_collect_conv2d_info(KernelEntry const *ke,
                                   TileConv2DInfo *out) {
  if (!tile_analyze_conv2d_flat(ke, out)) {
    return 0;
  }
  return out->threads > 0 && out->threads <= 256;
}

// DAG-mode dispatch shape: derive (groups, threads) from the lifted
// UOp DAG's RANGE-leaf axis types/extents, mirroring the render_uop.c
// default-parallelise convention exactly so the launched grid covers
// the kernel's `tid` decode with no over/under-launch:
//   - promoted output axes  (KAX_LOOP)  -> the flat `tid` index range
//   - in-thread loops       (KAX_UPCAST / KAX_UNROLL / KAX_REDUCE)
//                                       -> do NOT contribute to `tid`
//   - threadgroup-local     (KAX_LOCAL) -> threadgroup size
//   - threadgroup-collective(KAX_GROUP_REDUCE) -> threadgroup size
// Returns 1 with (groups, threads) on success; 0 if the DAG has no
// axes / overflow / a threadgroup dim that exceeds the hw cap.
//
// This path runs ONLY for kernels with cached_lift.store_root != 0
// (the production lifted-DAG kernels): kernel_apply_opt mutates the
// DAG (not ke->schedule->applied_opts, which stays empty), so the
// applied_opts-reading tile path would compute the pre-opt grid.
static int rmt_dag_dispatch_shape(KernelEntry const *ke, u32 *groups_x,
                                  u32 *threads_x) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  if (n == 0) return 0;
  u64 total = 1;            // product of promoted-LOOP output extents
  u64 local_total = 1;      // product of KAX_LOCAL extents
  u32 group_reduce_extent = 0;
  for (u32 i = 0; i < n; i++) {
    if (exts[i] == 0) return 0;
    switch (types[i]) {
      case KAX_LOOP:         total *= (u64)exts[i]; break;
      case KAX_LOCAL:        local_total *= (u64)exts[i]; break;
      case KAX_GROUP_REDUCE: group_reduce_extent = exts[i]; break;
      // KAX_UPCAST / KAX_UNROLL / KAX_REDUCE: in-thread, not in `tid`.
      default: break;
    }
  }
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 groups, threads;
  if (group_reduce_extent != 0) {
    if (group_reduce_extent > 256) return 0;
    if (total > 0xFFFFFFFFu) return 0;
    groups  = (u32)total;
    threads = group_reduce_extent;
  } else if (local_total > 1) {
    if (local_total > 1024) return 0;     // Apple maxTotalThreadsPerTG
    // tg/tt split (mirrors render_uop.c's RmuGlobalDecode.has_local):
    // GLOBAL extents -> grid (one threadgroup per GLOBAL tuple, decoded
    // from `tg`); LOCAL extents -> threadgroup (decoded from `tt`).  So
    // `groups` is exactly the product of promoted-LOOP output extents
    // and `threads` is exactly the product of LOCAL extents.
    if (total > 0xFFFFFFFFu) return 0;
    threads = (u32)local_total;
    groups  = (u32)total;
  } else {
    threads = total < 256 ? (u32)total : 256u;
    groups  = (u32)((total + (u64)threads - 1) / (u64)threads);
  }
  if (groups == 0 || threads == 0) return 0;
  if (groups_x  != NULL) *groups_x  = groups;
  if (threads_x != NULL) *threads_x = threads;
  return 1;
}

// True iff the lifted DAG carries any per-axis OPT-class axis
// (UPCAST / UNROLL / LOCAL / GROUP_REDUCE) -- i.e. kernel_apply_opt
// (DAG mode) ran.  The conv2d-flat dispatch reader keys its
// outputs_per_thread off ke->schedule->applied_opts (empty in DAG
// mode), so it would mis-size; bail to the generic DAG path instead.
static int rmt_dag_has_opt_axes(KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  for (u32 i = 0; i < n; i++) {
    if (types[i] == KAX_UPCAST || types[i] == KAX_UNROLL
        || types[i] == KAX_LOCAL || types[i] == KAX_GROUP_REDUCE) {
      return 1;
    }
  }
  return 0;
}

int cg_tile_metal_dispatch_shape(KernelEntry *ke, u32 *groups_x,
                                 u32 *threads_x) {
  if (ke == NULL) return 0;
  // Metal hardware caps buffer attributes at index 30; reject
  // kernels that won't render (matches the renderer's reject).
  if (ke->n_inputs > 30) return 0;
  // DAG-mode + per-axis OPTs present (hand-coded-opts / autotune ran
  // and mutated the DAG): the conv2d-flat / scalar-tile readers can't
  // see those (they key off the empty applied_opts log), so derive
  // (groups, threads) straight from the DAG's axis types.  Skip when
  // a TC matmul template is in play -- that binds its own grid.
  {
    Term sroot = ke->cached_lift.store_root;
    int tc_template = 0;
    if (sroot != 0 && term_tag(sroot) == TAG_UOP
        && term_ext(sroot) == UOP_STORE) {
      Term v = heap_read(term_val(sroot) + 2);
      if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_OPT
          && uop_opt_kind(v) == UOP_OPT_TC) tc_template = 1;
    }
    if (!tc_template && rmt_dag_has_opt_axes(ke)
        && rmt_dag_dispatch_shape(ke, groups_x, threads_x)) {
      return 1;
    }
  }
  // Conv2D flat: dispatch (c_out * patches / outputs_per_thread)
  // total threads divided into `threads` per group.
  TileConv2DInfo conv;
  if (rmt_collect_conv2d_info(ke, &conv)) {
    u64 total = (u64)conv.c_out * (u64)conv.patches;
    if (total == 0 || total > 0xFFFFFFFFu) return 0;
    u32 outputs = conv.outputs_per_thread ? conv.outputs_per_thread : 1;
    u64 threads_total = (total + (u64)outputs - 1) / (u64)outputs;
    u32 threads = conv.threads;
    u32 groups  = (u32)((threads_total + (u64)threads - 1) / (u64)threads);
    if (groups == 0) return 0;
    if (groups_x  != NULL) *groups_x  = groups;
    if (threads_x != NULL) *threads_x = threads;
    return 1;
  }
  if (tile_rejects_conv2d_flat_cin1(ke)) return 0;
  // Lifter-based fallback.  If the kernel_lift would succeed,
  // render_uop emits a valid kernel; dispatch shape is just
  // (output_numel, 256-default threadgroup).
  // The kernel's outer for-loops handle work distribution within each
  // thread (each thread runs the full body redundantly; last-writer-
  // wins on the output buffer gives correct results).  Future wedges
  // can specialize when M/N-axes get bound to thread positions via
  // UOP_OPT_LOCAL annotations.
  //
  // Phase C slice 2: prefer the cached lift outcome as the "would
  // the lifter succeed?" oracle when populated.  When 0, fall back
  // to a fresh kernel_lift_to_uop -- test infra builds kernels via
  // rangeify_emit() bypassing materialize, so cached_lift stays 0
  // even though the lift would succeed.
  if (ke->cached_lift.store_root == 0) {
    KernelUopLift fresh = {0};
    if (!kernel_lift_to_uop(ke, &fresh)) return 0;
  }
  u64 total = ke->output_numel ? (u64)ke->output_numel : 1;
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 threads = total < 256 ? (u32)total : 256u;
  u32 groups  = (u32)((total + (u64)threads - 1) / (u64)threads);
  if (groups == 0 || threads == 0) return 0;
  if (groups_x  != NULL) *groups_x  = groups;
  if (threads_x != NULL) *threads_x = threads;
  return 1;
}


// Render through kernel_lift_to_uop + cg_render_uop_kernel_root.
// This is the primary (and only) Metal MSL emit path: the lifter
// handles every kernel shape (matmul, conv2d_flat including
// multi-input X im2col, elementwise, reduce, movement-fused
// subtrees).
//
// Bumps KERNEL_LIFT_ATTEMPTS / SUCCESSES counters as it goes; readable
// via kernel_lift_attempts() / kernel_lift_successes() for diagnostics.
// (KERNEL_LIFT_COMPILES / _COMPILE_FAILS were populated by an earlier
// shadow-compile path that shell-exec'd xcrun metal; that was deleted
// when render_uop became the sole emit path -- the actual MTLLibrary
// PSO build downstream is now the source of truth for compilability.)
//
// Phase C slice 3: passes compute_root (= cached_lift.store_root)
// directly into cg_render_uop_kernel_root, which discovers buffer
// slots structurally from the DAG via UOP_BUFFER.instance.  No
// in_bufs[] dependency: kernel_lift.c stamps instance=slot+1 on each
// input BUFFER, so the renderer reconstructs slot positions from the
// DAG itself.
static char *cg_emit_via_uop(KernelEntry const *ke) {
  kernel_lift_count_attempt();
  // Metal hardware caps buffer attributes at index 30 (31 slots total
  // including output).  Reject kernels with too many inputs.
  if (ke->n_inputs > 30) return NULL;
  // Phase C slice 2: prefer the cached KernelUopLift populated by
  // emit_kernel_for_boundary.  When the cache is populated
  // (cached_lift.store_root != 0) we read it directly and skip the
  // redundant kernel_lift_to_uop call.  When the cache is 0 there
  // are two possibilities: materialize-time lift declined (the
  // common case in the materialize pipeline), OR the kernel was
  // constructed by test infra via rangeify_emit() bypassing
  // materialize entirely (test_kernel_lift_coverage,
  // test_tile_graph, test_metal_real cover this).  We can't tell
  // them apart so we fall back to a fresh on-demand lift; it will
  // succeed on rangeify-built kernels and decline on truly
  // unsupported ones, matching the pre-slice-2 behavior.
  Term cached_root = ke->cached_lift.store_root;
  KernelUopLift fresh = {0};
  if (cached_root == 0) {
    if (!kernel_lift_to_uop(ke, &fresh)) return NULL;
    cached_root = fresh.store_root;
  }
  kernel_lift_count_success();
  // Render to a malloc'd string, matching cg_emit_tile_metal's
  // contract.  Use kernel name "k" so MTLLibrary lookup behaves like
  // the existing path.
  // F3.1: pre-render pass installs UOP_OPT(_, TC, 0) on matmul-shaped
  // STORE roots so render_uop's simdgroup_matrix template fires.
  // No-op for non-matmul kernels.
  Term store_root = uop_recognise_tc(cached_root);
  // F4: same for conv2d_flat -- installs UOP_OPT(_, CONV, 0) on
  // STORE roots whose REDUCE body has IDIV/IMOD-decomposed addresses
  // so render_uop's rmu_emit_conv template fires.  No-op when the
  // matmul recogniser already wrapped the root (TC takes precedence).
  store_root = uop_recognise_conv(store_root);
  // Default-parallelise happens inside the renderer (rmu_emit_store /
  // rmu_emit_output_loops): plain-LOOP output axes are decoded from
  // `tid` instead of looped serially.  We do NOT rewrite the DAG's
  // axis_type here -- a KAX_GLOBAL rewrite would make the renderer's
  // legacy single-`tg` branch fire and collide multiple grid axes.
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) return NULL;
  // Structural entry: derive kernel signature (output dtype + per-
  // input dtype) from the BUFFER nodes in the DAG via slot index;
  // names ("out", "in0", "in1", ...) are decoded structurally too.
  cg_render_uop_kernel_root(store_root, "k", fp);
  long n = ftell(fp);
  fclose(fp);
  if (n <= 0) return NULL;
  char *out = (char *)malloc((size_t)n + 1);
  if (out == NULL) return NULL;
  memcpy(out, buf, (size_t)n);
  out[n] = '\0';
  return out;
}

char *cg_emit_tile_metal(KernelEntry const *ke) {
  // Multi-output kernels are not yet renderable through the tile
  // metal path (single `device float *out` arg + single S_STORE).
  // Bail until step 4 wires the multi-output dispatch.
  if (cg_kernel_has_extra_outputs(ke)) {
    return NULL;
  }
  // Render through the UOp-DAG renderer.  The lifter handles every
  // kernel shape (matmul / conv2d / elementwise / reduce / movement-
  // fused / im2col multi-input).  Returns NULL when the lifter
  // declines (n_inputs > 30, malformed shape, etc.) so the dispatch
  // ladder can route the kernel through a non-tile path if any.
  return cg_emit_via_uop(ke);
}

