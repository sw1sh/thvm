// Public bridge for WL FFI (thvmlink.c) and tests (test_bufferize)
// that want to peek at the rendered MSL without going through Metal
// dispatch. After the dispatch ladder collapse (88f536c3 / 4e30432b),
// the only Metal MSL emit path is metal_tile_jit_encode -> render_uop;
// cg_emit_metal forwards to cg_emit_tile_metal which routes there.
char *cg_emit_metal(KernelEntry const *ke) {
  return cg_emit_tile_metal((KernelEntry *)ke);
}

// Forward declaration of the threadgroup-staged TC tile picker, defined
// in render_uop.c (#included after this file in the unity build).  The
// dispatch-shape code below must size the threadgroup grid to the same
// tile geometry the renderer emits, so it calls the identical picker.
typedef struct {
  u32 local_m, local_n;   // simdgroups along M / N within a threadgroup
  u32 rm, rn;             // register 8x8 tiles per simdgroup along M / N
  u32 kb;                 // K-block staged in threadgroup memory
} RmuTcTile;
static int rmu_tc_pick_tile(u32 m_extent, u32 n_extent, u32 k_extent,
                            u32 batch_extent, RmuTcTile *out);


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
// Read the OPT(_, GROUP_REDUCE, k) factor wrapping a KAX_GROUP_REDUCE
// range -- the cooperative threadgroup size.  See cuda_dag_group_reduce_
// factor for the same logic on the CUDA side; the GROUP_REDUCE template
// in render_uop.c iterates the full reduce extent in strides of `k`.
static u32 rmt_dag_group_reduce_factor(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_OPT) {
    u32 kind = (u32)term_val(heap_read(loc + 1));
    if (kind == UOP_OPT_GROUP_REDUCE) {
      return (u32)term_val(heap_read(loc + 2));
    }
    return rmt_dag_group_reduce_factor(heap_read(loc + 0));
  }
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return 0;
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    u32 f = rmt_dag_group_reduce_factor(heap_read(loc + i));
    if (f != 0) return f;
  }
  return 0;
}

static int rmt_dag_dispatch_shape(KernelEntry const *ke, u32 *groups_x,
                                  u32 *threads_x) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  if (n == 0) return 0;
  u64 total = 1;            // product of promoted-LOOP output extents
  u64 local_total = 1;      // product of KAX_LOCAL extents
  int has_group_reduce = 0;
  for (u32 i = 0; i < n; i++) {
    if (exts[i] == 0) return 0;
    switch (types[i]) {
      case KAX_LOOP:         total *= (u64)exts[i]; break;
      case KAX_LOCAL:        local_total *= (u64)exts[i]; break;
      case KAX_GROUP_REDUCE: has_group_reduce = 1; break;
      // KAX_UPCAST / KAX_UNROLL / KAX_REDUCE: in-thread, not in `tid`.
      default: break;
    }
  }
  u32 group_reduce_extent = has_group_reduce
      ? rmt_dag_group_reduce_factor(ke->cached_lift.store_root) : 0;
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 groups, threads;
  if (group_reduce_extent != 0) {
    if (group_reduce_extent > 256) return 0;
    if (total > 0xFFFFFFFFu) return 0;
    // A GROUP_REDUCE axis that COEXISTS with LOCAL axes (tinygrad matvec:
    // GROUP the reduce + LOCAL the output row) needs BOTH in the
    // threadgroup -- local_total rows * group_extent reduce-threads.  The
    // render_uop GROUP template lays the group out as the innermost tt dim.
    // local_total == 1 (GROUP-only / GROUPTOP) -> threads = group_extent,
    // unchanged.
    u64 t = (u64)group_reduce_extent * local_total;
    if (t > 1024) return 0;                 // Apple maxTotalThreadsPerTG
    groups  = (u32)total;
    threads = (u32)t;
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

// q8 weight-only-quantized matmul detection for dispatch-grid sizing.
// The B operand is a CAST(int8 -> bf16) over the INT8 weight buffer
// (TMatMul[x, Transpose[Cast[w_int8, bf16]]]); uop_dag_classify_matmul_shape
// declines it (its uniform-dtype gate rejects the int8 vs bf16 mismatch, so
// BLAS / MPS never run it) -- but the renderer's rmu_emit_matmul_tc DOES emit
// the tiled simdgroup kernel for it (b_is_int8 char->bfloat staging), so the
// grid must still be sized to the tile.  Reads M/N/K off the (OPT-wrapped)
// matmul shape via the same recognisers the renderer uses.  Returns 1 +
// M/N/K when the store_root is an int8-B matmul; 0 otherwise.
static int rmt_q8_matmul_shape(Term sroot, u32 *out_M, u32 *out_N, u32 *out_K) {
  if (sroot == 0 || term_tag(sroot) != TAG_UOP
      || term_ext(sroot) != UOP_STORE) return 0;
  // Peel an OPT(_, TC, _) wrapper to reach the REDUCE, then the MUL's B.
  Term value = heap_read(term_val(sroot) + 2);
  while (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
         && uop_opt_kind(value) == UOP_OPT_TC) {
    value = uop_opt_target(value);
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term mul = heap_read(term_val(value) + 0);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term rhs = heap_read(term_val(mul) + 1);
  // B must be a CAST whose source INDEX_Es an INT8 buffer.
  if (term_tag(rhs) != TAG_UOP
      || (term_ext(rhs) != UOP_CAST && term_ext(rhs) != UOP_BITCAST)) return 0;
  Term idx = heap_read(term_val(rhs) + 0);
  if (term_tag(idx) != TAG_UOP || term_ext(idx) != UOP_INDEX_E) return 0;
  Term buf = heap_read(term_val(idx) + 0);
  if (term_tag(buf) != TAG_UOP || term_ext(buf) != UOP_BUFFER) return 0;
  if (uop_buffer_dtype(buf) != DT_INT8) return 0;
  // Recover M/N (+ extents) and K from the recognisers (cast-aware).
  u32 m_axis, m_ext, n_axis, n_ext, k_ext = 0;
  if (!uop_matmul_mn_axes(sroot, &m_axis, &m_ext, &n_axis, &n_ext)) return 0;
  if (!uop_classify_matmul(sroot, &k_ext, NULL)) return 0;
  if (m_ext == 0 || n_ext == 0 || k_ext == 0) return 0;
  if (out_M != NULL) *out_M = m_ext;
  if (out_N != NULL) *out_N = n_ext;
  if (out_K != NULL) *out_K = k_ext;
  return 1;
}

// THVM_FUSE_MATMUL_INPUT: recover M/N/K for a fused-A matmul (A is an inlined
// elementwise producer, B a clean buffer read) so the dispatch sizes the TILE
// grid -- one threadgroup per output tile -- not the GLOBAL-axis-count grid
// (which would launch tile_m/8 x tile_n/8 too many threadgroups and corrupt
// every tile past the first).  uop_classify_matmul / uop_matmul_mn_axes already
// accept the fused A (recognise_tc.c).  Returns 0 unless A is genuinely fused.
static int rmt_fused_a_matmul_shape(Term sroot, u32 *out_M, u32 *out_N,
                                    u32 *out_K) {
  if (!ru_fuse_matmul_input_on()) return 0;
  if (sroot == 0 || term_tag(sroot) != TAG_UOP
      || term_ext(sroot) != UOP_STORE) return 0;
  Term value = heap_read(term_val(sroot) + 2);
  while (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
         && uop_opt_kind(value) == UOP_OPT_TC) {
    value = uop_opt_target(value);
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term mul = heap_read(term_val(value) + 0);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  // A (after one cast peel) must NOT be a bare INDEX_E -- that is the fused
  // producer signature (a plain matmul goes through the BLAS-slot classifier).
  Term lhs = heap_read(term_val(mul) + 0);
  if (term_tag(lhs) == TAG_UOP
      && (term_ext(lhs) == UOP_CAST || term_ext(lhs) == UOP_BITCAST)) {
    Term src = heap_read(term_val(lhs) + 0);
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) return 0;
  } else if (term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_INDEX_E) {
    return 0;
  }
  // uop_classify_matmul / uop_matmul_mn_axes expect an UNWRAPPED store root
  // (they bail on a leading OPT_TC), so rebuild the store around the peeled
  // REDUCE value (mirrors udg_classify_matmul_store).
  Term cstore = sroot;
  if (value != heap_read(term_val(sroot) + 2)) {
    cstore = uop_store(heap_read(term_val(sroot) + 0),
                       heap_read(term_val(sroot) + 1), value);
  }
  u32 m_axis, m_ext, n_axis, n_ext, k_ext = 0;
  if (!uop_matmul_mn_axes(cstore, &m_axis, &m_ext, &n_axis, &n_ext)) return 0;
  if (!uop_classify_matmul(cstore, &k_ext, NULL)) return 0;
  if (m_ext == 0 || n_ext == 0 || k_ext == 0) return 0;
  if (out_M != NULL) *out_M = m_ext;
  if (out_N != NULL) *out_N = n_ext;
  if (out_K != NULL) *out_K = k_ext;
  return 1;
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
    // TRUE batched gemm (attention's mhaBmm): the value is OPT(_, TC, 0)
    // wrapped and the batch/M/N axes are GLOBAL, but uop_dag_classify_matmul_-
    // shape rejects the 3-range-per-operand shape, so the 2-D branches below
    // bail.  Size the grid directly: batch * (M/8) * (N/8) threadgroups, 32
    // threads each (one simdgroup per (batch, 8x8 tile)) -- mirrors the
    // renderer's is_batched parallel_tc layout exactly.  The batch axis is
    // NOT divided by 8 (it is the gemm-index dim, not a tile dim).
    if (tc_template) {
      u32 b_ax, b_ext, m_ax, m_ext, n_ax, n_ext, k_ext;
      if (uop_classify_batched_matmul(sroot, &b_ax, &b_ext, &m_ax, &m_ext,
                                      &n_ax, &n_ext, &k_ext)
          && (m_ext % 8u) == 0 && (n_ext % 8u) == 0 && b_ext >= 1) {
        // THVM_TC_BATCHED (default OFF -- residual FLUX-pipeline gap, see
        // render_uop.c): the batched-tiled emitter is correct in isolation but a full
        // FluxGenerate still decodes garbage with it on.  This dispatch-grid gate
        // MUST match rmu_emit_matmul_tc_tiled's gate -- if they disagree the grid and
        // kernel mismatch (worse than either consistent path).  Default OFF -> the
        // parallel_tc grid (correct).  THVM_TC_BATCHED=1 opts BOTH gates back in.
        static int _tcb_k = 0, _tcb = 0;
        if (!_tcb_k) { char const *e = getenv("THVM_TC_BATCHED");
                       if (e != NULL && e[0] == '1') _tcb = 1; _tcb_k = 1; }
        RmuTcTile btile;
        if (_tcb && (k_ext % 8u) == 0
            && rmu_tc_pick_tile(m_ext, n_ext, k_ext, b_ext, &btile)) {
          u32 tm = btile.local_m * btile.rm * 8u, tn = btile.local_n * btile.rn * 8u;
          if ((m_ext % tm) == 0 && (n_ext % tn) == 0) {
            u64 ntg = (u64)b_ext * (u64)(m_ext / tm) * (u64)(n_ext / tn);
            if (ntg > 0 && ntg <= 0xFFFFFFFFu) {
              if (groups_x  != NULL) *groups_x  = (u32)ntg;
              if (threads_x != NULL) *threads_x = btile.local_m * btile.local_n * 32u;
              if (getenv("THVM_DISP_TRACE"))
                fprintf(stderr, "[disp] BATCHED tiled M=%u N=%u K=%u b=%u tile=%ux%u "
                        "grid=%llu threads=%u\n", m_ext, n_ext, k_ext, b_ext, tm, tn,
                        (unsigned long long)ntg, btile.local_m * btile.local_n * 32u);
              return 1;
            }
          }
        }
        u64 ntg = (u64)b_ext * (u64)(m_ext / 8u) * (u64)(n_ext / 8u);
        if (ntg > 0 && ntg <= 0xFFFFFFFFu) {
          if (groups_x  != NULL) *groups_x  = (u32)ntg;
          if (threads_x != NULL) *threads_x = 32u;
          return 1;
        }
      }
    }
    // Parallel TC: the simdgroup_matrix template binds each GLOBAL output
    // axis to the threadgroup grid, one simdgroup (32 threads) per 8x8
    // output tile (render_uop.c rmu_emit_matmul_tc parallel_tc branch).
    // grid = product(GLOBAL_extent / 8) threadgroups, 32 threads each.
    // Only the GLOBAL-promoted axes parallelise; a non-promoted output
    // axis stays an in-kernel for-loop (still correct, just serial).
    // Without this the TC kernel would take the output_numel default
    // grid below, racing many simdgroups onto the same tiles.
    //
    // BUT the simdgroup_matrix template only fires when M%8==0 AND N%8==0
    // (render_uop.c rmu_emit_matmul_tc bails to a scalar accumulator
    // otherwise -- it can't tile a ragged 8x8 fragment).  When it bails,
    // the kernel emits the default `tid`-decoded scalar body whose correct
    // grid is output_numel, NOT the TC tile grid.  So if the matmul shape
    // doesn't qualify, skip the TC tile-grid branch and fall through to
    // the output_numel default -- otherwise we'd launch only
    // product(M_tiles) * 32 threads and leave most output cells unwritten
    // (e.g. a {8,K}x{K,500} matmul: M=8 -> 1 tile -> 32 threads, but the
    // scalar kernel needs 8*500=4000 threads).
    // q8 int8-weight matmul: uop_dag_classify_matmul_shape declines it (the
    // int8 vs bf16 uniform-dtype gate), so size the tiled grid off the
    // cast-aware recognisers instead -- one threadgroup per output tile,
    // LOCAL_M*LOCAL_N*32 threads each (mirrors rmu_emit_matmul_tc_tiled).
    if (tc_template) {
      u32 qM = 0, qN = 0, qK = 0;
      if (rmt_q8_matmul_shape(sroot, &qM, &qN, &qK)
          && (qM % 8u) == 0 && (qN % 8u) == 0 && (qK % 8u) == 0) {
        u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
        u32 na = uop_dag_collect_axes(sroot, ids, types, exts, MAX_AXES);
        u32 n_global = 0;
        for (u32 i = 0; i < na; i++) if (types[i] == KAX_GLOBAL) n_global++;
        RmuTcTile tile;
        if (n_global >= 2 && rmu_tc_pick_tile(qM, qN, qK, 0u, &tile)) {
          u32 tile_m = tile.local_m * tile.rm * 8u;
          u32 tile_n = tile.local_n * tile.rn * 8u;
          u64 ntg = (u64)(qM / tile_m) * (u64)(qN / tile_n);
          u32 nthreads = tile.local_m * tile.local_n * 32u;
          if (ntg > 0 && ntg <= 0xFFFFFFFFu) {
            if (groups_x  != NULL) *groups_x  = (u32)ntg;
            if (threads_x != NULL) *threads_x = nthreads;
            return 1;
          }
        }
      }
    }
    // THVM_FUSE_MATMUL_INPUT: fused-A matmul.  uop_dag_classify_matmul_shape
    // declines (A is an inlined producer, not a single input slot), so size the
    // TILE grid off the cast-aware recognisers -- one threadgroup per output
    // tile, LOCAL_M*LOCAL_N*32 threads each (mirrors rmu_emit_matmul_tc_tiled).
    if (tc_template) {
      u32 fM = 0, fN = 0, fK = 0;
      if (rmt_fused_a_matmul_shape(sroot, &fM, &fN, &fK)
          && (fM % 8u) == 0 && (fN % 8u) == 0 && (fK % 8u) == 0) {
        u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
        u32 na = uop_dag_collect_axes(sroot, ids, types, exts, MAX_AXES);
        u32 n_global = 0;
        for (u32 i = 0; i < na; i++) if (types[i] == KAX_GLOBAL) n_global++;
        RmuTcTile tile;
        if (n_global >= 2 && rmu_tc_pick_tile(fM, fN, fK, 0u, &tile)) {
          u32 tile_m = tile.local_m * tile.rm * 8u;
          u32 tile_n = tile.local_n * tile.rn * 8u;
          u64 ntg = (u64)(fM / tile_m) * (u64)(fN / tile_n);
          u32 nthreads = tile.local_m * tile.local_n * 32u;
          if (ntg > 0 && ntg <= 0xFFFFFFFFu) {
            if (groups_x  != NULL) *groups_x  = (u32)ntg;
            if (threads_x != NULL) *threads_x = nthreads;
            return 1;
          }
        }
      }
    }
    if (tc_template) {
      UopDagGemmShape gemm = {0};
      int simdgroup_fires =
          uop_dag_classify_matmul_shape(sroot, ke, &gemm)
          && gemm.M != 0 && gemm.N != 0
          && (gemm.M % 8u) == 0 && (gemm.N % 8u) == 0;
      // A fused-A matmul intentionally fails the BLAS-slot classifier; don't
      // clear tc_template for it (the fused grid above already returned, but if
      // its tile picker declined we still want the simdgroup/scalar path, not a
      // mis-sized grid).
      if (!simdgroup_fires && !rmt_fused_a_matmul_shape(sroot, NULL, NULL, NULL))
        tc_template = 0;
    }
    if (tc_template) {
      // Threadgroup-staged tiled path: when both M and N are GLOBAL and
      // the tile picker accepts the shape, the renderer emits a
      // TILE_M x TILE_N tile per threadgroup with LOCAL_M*LOCAL_N
      // simdgroups.  Size the grid to match: one threadgroup per output
      // tile, LOCAL_M*LOCAL_N*32 threads each.  Mirrors
      // rmu_emit_matmul_tc's gate exactly (render_uop.c).
      {
        UopDagGemmShape g2 = {0};
        if (uop_dag_classify_matmul_shape(sroot, ke, &g2)
            && g2.M != 0 && g2.N != 0 && g2.K != 0
            && (g2.dtype == DT_FP32 || g2.dtype == DT_BF16)) {
          // Both M and N must be GLOBAL-promoted for the tiled emit.
          u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
          u32 na = uop_dag_collect_axes(sroot, ids, types, exts, MAX_AXES);
          u32 n_global = 0;
          for (u32 i = 0; i < na; i++) if (types[i] == KAX_GLOBAL) n_global++;
          RmuTcTile tile;
          if (n_global >= 2
              && rmu_tc_pick_tile(g2.M, g2.N, g2.K, 0u, &tile)) {
            u32 tile_m = tile.local_m * tile.rm * 8u;
            u32 tile_n = tile.local_n * tile.rn * 8u;
            u64 ntg = (u64)(g2.M / tile_m) * (u64)(g2.N / tile_n);
            u32 nthreads = tile.local_m * tile.local_n * 32u;
            if (ntg > 0 && ntg <= 0xFFFFFFFFu) {
              if (groups_x  != NULL) *groups_x  = (u32)ntg;
              if (threads_x != NULL) *threads_x = nthreads;
              return 1;
            }
          }
        }
      }
      u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
      u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                                   exts, MAX_AXES);
      u64 tiles = 1;
      int any_global = 0;
      for (u32 i = 0; i < n; i++) {
        if (types[i] == KAX_GLOBAL) {
          if (exts[i] == 0 || (exts[i] % 8u) != 0) { any_global = 0; break; }
          tiles *= (u64)(exts[i] / 8u);
          any_global = 1;
        }
      }
      if (any_global && tiles > 0 && tiles <= 0xFFFFFFFFu) {
        if (groups_x  != NULL) *groups_x  = (u32)tiles;
        if (threads_x != NULL) *threads_x = 32u;
        return 1;
      }
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
  // cached_lift.store_root == 0 means the materialize-time lift
  // declined (no source_uop / no unified store_root); the renderer
  // can't emit a valid kernel.
  if (ke->cached_lift.store_root == 0) return 0;
  u64 total = ke->output_numel ? (u64)ke->output_numel : 1;
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 threads = total < 256 ? (u32)total : 256u;
  u32 groups  = (u32)((total + (u64)threads - 1) / (u64)threads);
  if (groups == 0 || threads == 0) return 0;
  if (groups_x  != NULL) *groups_x  = groups;
  if (threads_x != NULL) *threads_x = threads;
  return 1;
}


// Render through cached KernelUopLift + cg_render_uop_kernel_root.
// This is the primary (and only) Metal MSL emit path: the lifter
// (materialize-time) handles every kernel shape (matmul, conv2d_flat
// including multi-input X im2col, elementwise, reduce,
// movement-fused subtrees).
//
// Bumps KERNEL_LIFT_ATTEMPTS / SUCCESSES counters as it goes; readable
// via kernel_lift_attempts() / kernel_lift_successes() for diagnostics.
//
// Passes cached_lift.store_root directly into
// cg_render_uop_kernel_root, which discovers buffer slots structurally
// from the DAG via UOP_BUFFER.instance.  No in_bufs[] dependency:
// kernel_lift.c stamps instance=slot+1 on each input BUFFER, so the
// renderer reconstructs slot positions from the DAG itself.
static char *cg_emit_via_uop(KernelEntry const *ke) {
  kernel_lift_count_attempt();
  // Metal hardware caps buffer attributes at index 30 (31 slots total
  // including output).  Reject kernels with too many inputs.
  if (ke->n_inputs > 30) return NULL;
  // Read the cached KernelUopLift populated by emit_kernel_for_boundary.
  // cached_root==0 means the materialize-time lift declined (no
  // source_uop / no unified store_root / n_inputs over cap) -- no
  // amount of re-running will help.
  Term cached_root = ke->cached_lift.store_root;
  if (cached_root == 0) return NULL;
  kernel_lift_count_success();
  // Render to a malloc'd string, matching cg_emit_tile_metal's
  // contract.  Use kernel name "k" so MTLLibrary lookup behaves like
  // the existing path.
  // F3.1: pre-render pass installs UOP_OPT(_, TC, 0) on matmul-shaped
  // STORE roots so render_uop's simdgroup_matrix template fires.
  // No-op for non-matmul kernels.
  //
  // The parallel variant ALSO re-stamps the M and N output axes
  // KAX_GLOBAL when K/M/N are all multiples of 8, so rmu_emit_matmul_tc
  // takes the parallel_tc branch (one simdgroup per 8x8 output tile)
  // and cg_tile_metal_dispatch_shape launches product(extent/8)
  // threadgroups -- the whole GPU works the matmul instead of a single
  // 32-thread simdgroup running it serially under the legacy
  // `if(sgi==0u && tg==0u)` guard.  Each threadgroup owns a unique 8x8
  // tile, so there is no multi-simdgroup write race.  Ragged shapes
  // (M%8 or N%8 != 0) fall back to the guarded path automatically.
  Term store_root = uop_recognise_tc_parallel(cached_root);
  // TRUE batched matmul (attention's mhaBmm): the 2-D recogniser above
  // rejects the 3-range-per-operand shape and returns the root unchanged,
  // so try the batched recogniser next.  It wraps OPT(_, TC, 0) + stamps
  // batch/M/N GLOBAL, and rmu_emit_matmul_tc emits one simdgroup per
  // (batch, 8x8 output tile).  No-op (returns input) for every non-batched
  // kernel, so the 2-D path stays byte-identical.
  if (store_root == cached_root) {
    store_root = uop_recognise_batched_tc_parallel(store_root);
  }
  // Default-parallelise happens inside the renderer (rmu_emit_store /
  // rmu_emit_output_loops): plain-LOOP output axes are decoded from
  // `tid` instead of looped serially.  For non-matmul kernels we do NOT
  // rewrite the DAG's axis_type here -- a KAX_GLOBAL rewrite would make
  // the renderer's legacy single-`tg` branch fire and collide multiple
  // grid axes.  The matmul TC path is the exception: uop_recognise_tc_-
  // parallel above stamps M/N GLOBAL because rmu_emit_matmul_tc's
  // parallel_tc branch binds each threadgroup to a unique 8x8 tile (no
  // collision), and cg_tile_metal_dispatch_shape sizes the grid to match.
  // Render into a temp file (portable across macOS/Linux/Windows) and
  // read it back, rather than a fixed stack buffer -- no length cap.
  FILE *fp = tmpfile();
  if (fp == NULL) return NULL;
  // Structural entry: derive kernel signature (output dtype + per-
  // input dtype) from the BUFFER nodes in the DAG via slot index;
  // names ("out", "in0", "in1", ...) are decoded structurally too.
  cg_render_uop_kernel_root(store_root, "k", fp);
  long n = ftell(fp);
  if (n <= 0) { fclose(fp); return NULL; }
  char *out = (char *)malloc((size_t)n + 1);
  if (out == NULL) { fclose(fp); return NULL; }
  rewind(fp);
  size_t got = fread(out, 1, (size_t)n, fp);
  fclose(fp);
  out[got] = '\0';
  return out;
}

char *cg_emit_tile_metal(KernelEntry const *ke) {
  // Render through the UOp-DAG renderer.  The lifter handles every
  // kernel shape (matmul / conv2d / elementwise / reduce / movement-
  // fused / im2col multi-input).  Returns NULL when the lifter
  // declines (n_inputs > 30, malformed shape, etc.) so the dispatch
  // ladder can route the kernel through a non-tile path if any.
  return cg_emit_via_uop(ke);
}

