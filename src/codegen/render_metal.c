// For the WL FFI (thvmlink.c): a kernel's MSL, rendered without a dispatch.
char *cg_emit_metal(KernelEntry const *ke) {
  return cg_emit_tile_metal((KernelEntry *)ke);
}

// render_uop.c's TC tile picker (included after this file): the dispatch shape
// below must launch the tile geometry the renderer emits.
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

// The cooperative threadgroup size of a GROUP_REDUCE kernel (render_uop.c,
// included after this file).
static u32 rmu_group_reduce_factor(Term t);

// (groups, threads) from the DAG's axis types, as render_uop decodes them: LOOP
// extents index the outputs, LOCAL extents and the GROUP_REDUCE factor fill the
// threadgroup.  0 without axes, on overflow, or past the hardware cap.
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
  // An empty reduce axis loops zero times inside its thread; only an empty
  // output axis leaves nothing to launch.
  for (u32 i = 0; i < n; i++) {
    switch (types[i]) {
      case KAX_LOOP:         total *= (u64)exts[i]; break;
      case KAX_LOCAL:        local_total *= (u64)exts[i]; break;
      case KAX_GROUP_REDUCE: has_group_reduce = 1; break;
      // KAX_UPCAST / KAX_UNROLL / KAX_REDUCE: in-thread, not in `tid`.
      default: break;
    }
  }
  u32 group_reduce_extent = has_group_reduce
      ? rmu_group_reduce_factor(ke->cached_lift.store_root) : 0;
  if (total == 0 || local_total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 groups, threads;
  if (group_reduce_extent != 0) {
    if (group_reduce_extent > 256) return 0;
    if (total > 0xFFFFFFFFu) return 0;
    // GROUP_REDUCE with LOCAL axes (matvec: GROUP the reduce, LOCAL the row)
    // puts both in the threadgroup, the group innermost in tt.
    u64 t = (u64)group_reduce_extent * local_total;
    if (t > 1024) return 0;                 // Apple maxTotalThreadsPerTG
    groups  = (u32)total;
    threads = (u32)t;
  } else if (local_total > 1) {
    if (local_total > 1024) return 0;     // Apple maxTotalThreadsPerTG
    // One threadgroup per LOOP tuple (tg), the LOCAL extents across it (tt), as
    // render_uop's RmuGlobalDecode.has_local decodes them.
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

// THVM_FUSE_MATMUL_EPILOGUE: the OPT_TC under a store value's elementwise
// epilogue (the q8 dequant MUL(OPT_TC, scale)), or 0.  *n_opt counts the OPT_TCs
// reached, *n_ew those below an elementwise op.
static Term rmt_epi_walk_opt_tc(Term t, int *n_opt, int *n_ew, int above_ew,
                                int depth) {
  if (depth > 64 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_OPT) {
    if (uop_opt_kind(t) != UOP_OPT_TC) return 0;
    (*n_opt)++;
    if (above_ew) (*n_ew)++;
    return t;
  }
  if (op == UOP_CONST || op == UOP_INDEX_E || op == UOP_BUFFER
      || op == UOP_RANGE) return 0;
  if (op == UOP_REDUCE) return 0;            // bare reduce outside an OPT_TC
  int is_ew = op == UOP_CAST || op == UOP_BITCAST
           || uop_is_unary_elementwise((u8)op)
           || uop_is_binary_elementwise((u8)op)
           || uop_is_ternary_elementwise((u8)op)
           || uop_is_movement((u8)op);
  if (!is_ew) return 0;
  u8 ar = uop_arity((u8)op);
  Term found = 0;
  for (u8 i = 0; i < ar; i++) {
    Term f = rmt_epi_walk_opt_tc(heap_read(term_val(t) + i), n_opt, n_ew, 1,
                                 depth + 1);
    if (f != 0) found = f;
  }
  return found;
}

// THVM_FUSE_MATMUL_EPILOGUE: M/N/K of an epilogue-fused matmul as the renderer
// sees it, after the uop_recognise_tc_parallel that cg_emit_via_uop applies.  0
// for a bare matmul, whose value is the OPT_TC itself and which keeps its grid.
static int rmt_epilogue_tc_shape(Term sroot, u32 *out_M, u32 *out_N, u32 *out_K) {
  if (out_M != NULL) *out_M = 0;
  if (out_N != NULL) *out_N = 0;
  if (out_K != NULL) *out_K = 0;
  if (!ru_fuse_epilogue_on()) return 0;
  if (sroot == 0 || term_tag(sroot) != TAG_UOP
      || term_ext(sroot) != UOP_STORE) return 0;
  Term troot = uop_recognise_tc_parallel(sroot);
  if (troot == 0 || term_tag(troot) != TAG_UOP
      || term_ext(troot) != UOP_STORE) return 0;
  Term value = heap_read(term_val(troot) + 2);
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
      && uop_opt_kind(value) == UOP_OPT_TC) {
    return 0;
  }
  int n_opt = 0, n_ew = 0;
  Term opt = rmt_epi_walk_opt_tc(value, &n_opt, &n_ew, 0, 0);
  if (opt == 0 || n_opt != 1 || n_ew == 0) {
    return 0;
  }
  Term reduce = uop_opt_target(opt);
  if (reduce == 0 || term_tag(reduce) != TAG_UOP
      || term_ext(reduce) != UOP_REDUCE) return 0;
  // uop_classify_matmul and uop_matmul_mn_axes want the REDUCE as the store
  // value, not the epilogue chain.
  Term synth = uop_store(heap_read(term_val(troot) + 0),
                         heap_read(term_val(troot) + 1), reduce);
  u32 m_axis, m_ext, n_axis, n_ext, k_ext = 0, unit_axis = 0;
  if (!uop_classify_matmul(synth, &k_ext, &unit_axis) || unit_axis != 0) {
    return 0;
  }
  if (!uop_matmul_mn_axes(synth, &m_axis, &m_ext, &n_axis, &n_ext)) {
    return 0;
  }
  if (m_ext == 0 || n_ext == 0 || k_ext == 0) {
    return 0;
  }
  if (out_M != NULL) *out_M = m_ext;
  if (out_N != NULL) *out_N = n_ext;
  if (out_K != NULL) *out_K = k_ext;
  return 1;
}

// M/N/K of a matmul whose B is a weight-only quantized (int8 or fp8) buffer
// behind a dequant CAST, which rmu_emit_matmul_tc stages in its tile.
static int rmt_q8_matmul_shape(Term sroot, u32 *out_M, u32 *out_N, u32 *out_K) {
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
  Term rhs = heap_read(term_val(mul) + 1);
  if (term_tag(rhs) != TAG_UOP
      || (term_ext(rhs) != UOP_CAST && term_ext(rhs) != UOP_BITCAST)) return 0;
  Term idx = heap_read(term_val(rhs) + 0);
  if (term_tag(idx) != TAG_UOP || term_ext(idx) != UOP_INDEX_E) return 0;
  Term buf = heap_read(term_val(idx) + 0);
  if (term_tag(buf) != TAG_UOP || term_ext(buf) != UOP_BUFFER) return 0;
  u32 bdt = uop_buffer_dtype(buf);
  int is_fp8 = (bdt == DT_FP8E4M3 || bdt == DT_FP8E5M2);
  if (bdt != DT_INT8 && !is_fp8) return 0;
  u32 m_axis, m_ext, n_axis, n_ext, k_ext = 0;
  if (!uop_matmul_mn_axes(sroot, &m_axis, &m_ext, &n_axis, &n_ext)) return 0;
  if (!uop_classify_matmul(sroot, &k_ext, NULL)) return 0;
  if (m_ext == 0 || n_ext == 0 || k_ext == 0) return 0;
  if (out_M != NULL) *out_M = m_ext;
  if (out_N != NULL) *out_N = n_ext;
  if (out_K != NULL) *out_K = k_ext;
  return 1;
}

// THVM_FUSE_MATMUL_INPUT: M/N/K of a fused-A matmul (A an inlined elementwise
// producer, B a buffer read), so it launches one threadgroup per output tile,
// not one per 8x8 GLOBAL tile; 0 unless A is fused.
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
  // An A that is an INDEX_E (after one CAST) is a plain matmul, not a fused one.
  Term lhs = heap_read(term_val(mul) + 0);
  if (term_tag(lhs) == TAG_UOP
      && (term_ext(lhs) == UOP_CAST || term_ext(lhs) == UOP_BITCAST)) {
    Term src = heap_read(term_val(lhs) + 0);
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) return 0;
  } else if (term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_INDEX_E) {
    return 0;
  }
  // uop_classify_matmul and uop_matmul_mn_axes reject an OPT_TC value: classify
  // a store around the peeled REDUCE, as udg_classify_matmul_store does.
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
  // Metal caps buffer arguments at index 30; cg_emit_via_uop rejects these too.
  if (ke->n_inputs > 30) {
    return 0;
  }
  {
    // A store value OPT(_, TC, _) is a TC matmul, sized by the branches below.
    Term sroot = ke->cached_lift.store_root;
    int tc_template = 0;
    if (sroot != 0 && term_tag(sroot) == TAG_UOP
        && term_ext(sroot) == UOP_STORE) {
      Term v = heap_read(term_val(sroot) + 2);
      if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_OPT
          && uop_opt_kind(v) == UOP_OPT_TC) tc_template = 1;
    }
    // THVM_FUSE_MATMUL_EPILOGUE: an epilogue-fused matmul is an elementwise chain
    // over the REDUCE, not a bare OPT_TC; launch its tile grid.
    if (!tc_template) {
      u32 eM = 0, eN = 0, eK = 0;
      if (rmt_epilogue_tc_shape(sroot, &eM, &eN, &eK)
          && eM != 0 && eN != 0 && eK != 0
          && (eM % 8u) == 0 && (eN % 8u) == 0 && (eK % 8u) == 0) {
        RmuTcTile tile;
        if (rmu_tc_pick_tile(eM, eN, eK, 0u, &tile)) {
          u32 tile_m = tile.local_m * tile.rm * 8u;
          u32 tile_n = tile.local_n * tile.rn * 8u;
          if ((eM % tile_m) == 0 && (eN % tile_n) == 0) {
            u64 ntg = (u64)(eM / tile_m) * (u64)(eN / tile_n);
            u32 nthreads = tile.local_m * tile.local_n * 32u;
            if (ntg > 0 && ntg <= 0xFFFFFFFFu) {
              if (groups_x  != NULL) *groups_x  = (u32)ntg;
              if (threads_x != NULL) *threads_x = nthreads;
              return 1;
            }
          }
        }
      }
    }
    // A batched matmul (attention's mhaBmm), which uop_dag_classify_matmul_shape
    // rejects: batch * (M/8) * (N/8) threadgroups of one simdgroup, as the
    // renderer's is_batched parallel_tc lays it out; the batch is not tiled.
    if (tc_template) {
      u32 b_ax, b_ext, m_ax, m_ext, n_ax, n_ext, k_ext;
      if (uop_classify_batched_matmul(sroot, &b_ax, &b_ext, &m_ax, &m_ext,
                                      &n_ax, &n_ext, &k_ext)
          && (m_ext % 8u) == 0 && (n_ext % 8u) == 0 && b_ext >= 1) {
        // THVM_TC_BATCHED (default off, see render_uop.c) as rmu_tc_batched_on
        // reads it: this grid must match the kernel the renderer picks.
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
    // A weight-only quantized B: one threadgroup per output tile.
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
    // THVM_FUSE_MATMUL_INPUT: uop_dag_classify_matmul_shape declines a fused A
    // (no input slot), so its tile grid is sized here.
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
    // rmu_emit_matmul_tc needs M, N multiples of 8; otherwise it emits a scalar
    // body, whose grid is the output_numel default.
    if (tc_template) {
      UopDagGemmShape gemm = {0};
      int simdgroup_fires =
          uop_dag_classify_matmul_shape(sroot, ke, &gemm)
          && gemm.M != 0 && gemm.N != 0
          && (gemm.M % 8u) == 0 && (gemm.N % 8u) == 0;
      // A fused A fails the BLAS-slot classifier by design: keep tc_template.
      if (!simdgroup_fires && !rmt_fused_a_matmul_shape(sroot, NULL, NULL, NULL))
        tc_template = 0;
    }
    if (tc_template) {
      // With M and N GLOBAL and a tile the picker accepts, rmu_emit_matmul_tc
      // stages tiles in threadgroup memory: one threadgroup per output tile.
      {
        UopDagGemmShape g2 = {0};
        if (uop_dag_classify_matmul_shape(sroot, ke, &g2)
            && g2.M != 0 && g2.N != 0 && g2.K != 0
            && (g2.dtype == DT_FP32 || g2.dtype == DT_BF16)) {
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
      // Else the parallel_tc body: a simdgroup per 8x8 tile of the GLOBAL axes,
      // a LOOP output axis a serial loop (the output_numel grid would race).
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
    // Opt axes in the DAG: size it from the axis types, as render_uop decodes it.
    if (!tc_template && rmt_dag_has_opt_axes(ke)
        && rmt_dag_dispatch_shape(ke, groups_x, threads_x)) {
      return 1;
    }
  }
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
  // Default: 256-thread groups over the outputs, which render_uop decodes from
  // tid.  Without a lift (store_root 0) nothing renders.
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


// The MSL of a kernel's lifted DAG, buffer slots read off UOP_BUFFER.instance
// (slot + 1, kernel_lift.c).  Caller frees; NULL if it cannot render.
static char *cg_emit_via_uop(KernelEntry const *ke) {
  // Metal caps buffer arguments at index 30: the output and 30 inputs.
  if (ke->n_inputs > 30) {
    return NULL;
  }
  // 0: the materialize-time lift declined, for good.
  Term cached_root = ke->cached_lift.store_root;
  if (cached_root == 0) {
    return NULL;
  }
  // uop_recognise_tc_parallel wraps a matmul REDUCE in OPT(_, TC, 0) and, with
  // M, N and K multiples of 8, stamps M and N GLOBAL: a simdgroup per 8x8 tile.
  Term store_root = uop_recognise_tc_parallel(cached_root);
  // The 2-D recogniser leaves a batched matmul (attention's mhaBmm) alone; the
  // batched one also stamps the batch GLOBAL, a simdgroup per (batch, 8x8 tile).
  if (store_root == cached_root) {
    store_root = uop_recognise_batched_tc_parallel(store_root);
  }
  // A tmpfile has no size cap.
  FILE *fp = tmpfile();
  if (fp == NULL) return NULL;
  // The Metal backend looks the function up as "k".
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
  // NULL leaves the kernel to the Metal backend's non-tile paths.
  return cg_emit_via_uop(ke);
}

