// codegen/hand_opts.c - tinygrad's hand_coded_optimizations
// (tinygrad/codegen/opt/heuristic.py) over shape primitives only, one section
// per rule with its heuristic.py lines.  Runs on CPU, Metal and CUDA.

// HAND_CODED_OPTS=0 or tinygrad's NOOPT=1 turns it off.  Read live, not memoised,
// so a long-lived runtime can flip NOOPT (kernel_opts.wlt does).
static int hand_coded_opts_enabled(void) {
  char const *noopt = getenv("NOOPT");
  if (noopt != NULL && noopt[0] != '\0' && noopt[0] != '0') {
    return 0;
  }
  char const *e = getenv("HAND_CODED_OPTS");
  return (e != NULL && e[0] == '0') ? 0 : 1;
}

// An int env knob (MV_*, NOLOCALS); `dflt` when unset or not a number.
static int hand_opt_getenv_int(char const *name, int dflt) {
  char const *e = getenv(name);
  if (e == NULL || e[0] == '\0') return dflt;
  int sign = 1, v = 0; char const *p = e;
  if (*p == '-') { sign = -1; p++; }
  if (!(*p >= '0' && *p <= '9')) return dflt;
  while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
  return sign * v;
}

// tinygrad's k.rngs / k.full_shape / k.axis_types, in axis_id order (tinygrad
// orders by axis type first).  A KOpt names an axis_id, not a slot, since
// uop_dag_apply_kopt finds the RANGE by id: build it from axis_id[].
typedef struct {
  u32 n;
  u8  kax_type[MAX_AXES];
  u32 extent  [MAX_AXES];
  u32 axis_id [MAX_AXES];
} HandOptAxes;

static int hand_opt_snapshot_axes(KernelEntry const *ke, HandOptAxes *out) {
  if (ke == NULL || out == NULL) return 0;
  out->n = 0;
  if (ke->cached_lift.store_root == 0) return 0;
  u32 ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  u32 n = uop_dag_collect_axes(ke->cached_lift.store_root, ids, types,
                               exts, MAX_AXES);
  if (n == 0) return 0;
  for (u32 i = 0; i < n; i++) {
    if (types[i] > KAX_GROUP_REDUCE) return 0;
    out->kax_type[i] = (u8)types[i];
    out->extent[i]   = exts[i];
    out->axis_id[i]  = ids[i];
  }
  out->n = n;
  return 1;
}

// tinygrad's k.upcastable_dims (postrange.py:99-101), thvm's LOOP standing for
// WEAK; out[] gets the slots.  A symbolic (kvar) extent packs its var id
// (0x80000000 | id), so skip it as isinstance(s, int) does.
static u32 hand_opt_upcastable_dims(HandOptAxes const *ax, u32 *out) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) {
    u8 t = ax->kax_type[i];
    if (kvar_extent_is_var(ax->extent[i])) continue;   // isinstance(s, int)
    if ((t == KAX_LOOP || t == KAX_GLOBAL || t == KAX_LOCAL) && ax->extent[i] > 1) {
      out[n++] = i;
    }
  }
  return n;
}

// tinygrad's k.unrollable_dims (postrange.py:102-104): its reduce-typed LOCAL is
// thvm's GROUP_REDUCE, and reduce_axes keeps the axes a REDUCE of `root` reduces.
static u32 hand_opt_unrollable_dims(HandOptAxes const *ax, Term root, u32 *out) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) {
    u8 t = ax->kax_type[i];
    if (kvar_extent_is_var(ax->extent[i])) continue;   // isinstance(s, int)
    if ((t == KAX_REDUCE || t == KAX_GROUP_REDUCE) && ax->extent[i] > 1
        && uop_dag_reduces_axis(root, ax->axis_id[i])) {
      out[n++] = i;
    }
  }
  return n;
}

// len(k.axes_of(*types)) (postrange.py:90).
static u32 hand_opt_axes_of_n(HandOptAxes const *ax, u8 const *types, u32 n_types) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) {
    for (u32 j = 0; j < n_types; j++) {
      if (ax->kax_type[i] == types[j]) { n++; break; }
    }
  }
  return n;
}

// tinygrad's k.upcast_size() (postrange.py:97); a symbolic (kvar) axis counts at
// its upper bound, not its packed extent.
static u64 hand_opt_upcast_size(HandOptAxes const *ax) {
  u64 p = 1;
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_UPCAST || ax->kax_type[i] == KAX_UNROLL) {
      p *= (u64)kvar_extent_static(ax->extent[i]);
    }
  }
  return p;
}

// prod(k.full_shape[i] for i in k.upcastable_dims) (heuristic.py:82, 115).
static u64 hand_opt_output_loop_product(HandOptAxes const *ax) {
  u32 dims[MAX_AXES];
  u32 n = hand_opt_upcastable_dims(ax, dims);
  u64 p = 1;
  for (u32 i = 0; i < n; i++) p *= (u64)ax->extent[dims[i]];
  return p;
}

// tinygrad's k.upcasted (postrange.py:251-252).
static u32 hand_opt_upcasted(HandOptAxes const *ax) {
  u8 t[] = { KAX_UPCAST, KAX_UNROLL };
  return hand_opt_axes_of_n(ax, t, 2);
}

// tinygrad's k.group_for_reduces (postrange.py:253-254).
static u32 hand_opt_group_for_reduces(HandOptAxes const *ax) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) if (ax->kax_type[i] == KAX_GROUP_REDUCE) n++;
  return n;
}

// Has a REDUCE or GROUP_REDUCE axis, as a conv weight-grad does: a small output
// over a huge reduce, whose upcasts can starve the GPU of threads.
static int hand_opt_is_reduce_heavy(HandOptAxes const *ax) {
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_REDUCE || ax->kax_type[i] == KAX_GROUP_REDUCE) {
      return 1;
    }
  }
  return 0;
}

// 1 for an f32 or bf16 matmul the TC section can take; *out_K gets its K.
static int hand_opt_classify_matmul(KernelEntry const *ke, u32 *out_K) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  if (ke->output_dtype != DT_FP32 && ke->output_dtype != DT_BF16) return 0;
  UopDagGemmShape gemm;
  if (uop_dag_classify_matmul_shape(ke->cached_lift.store_root, ke, &gemm)) {
    if (gemm.dtype != DT_FP32 && gemm.dtype != DT_BF16) return 0;
    if (out_K != NULL) *out_K = gemm.K;
    return 1;
  }
  // THVM_FUSE_MATMUL_INPUT: a fused A has no input slot, so only
  // uop_classify_matmul sees the matmul; rmu_emit_matmul_tc tiles it, A inline.
  if (ru_fuse_matmul_input_on()) {
    u32 k_extent = 0, unit_axis = 0;
    if (uop_classify_matmul(ke->cached_lift.store_root, &k_extent, &unit_axis)
        && unit_axis == 0 && k_extent != 0) {
      if (out_K != NULL) *out_K = k_extent;
      return 1;
    }
  }
  // THVM_FUSE_MATMUL_EPILOGUE: a matmul under an elementwise epilogue escapes
  // both classifiers above; the UPCASTs would split its M/N off the tiled path.
  {
    u32 k_extent = 0;
    if (uop_classify_epilogue_matmul(ke->cached_lift.store_root, &k_extent,
                                     NULL, NULL, NULL, NULL) && k_extent != 0) {
      if (out_K != NULL) *out_K = k_extent;
      return 1;
    }
  }
  return 0;
}

// Backend ids: CPU_BACKEND 1, METAL_BACKEND 2, CUDA_BACKEND 3.
static int hand_opt_kernel_on_gpu(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  return b != NULL && (b->id == 2 || b->id == 3);
}

static int hand_opt_kernel_on_cpu(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  return b != NULL && b->id == 1;
}

// nvrtc spills registers where Metal does not, so a section may skip CUDA, as
// heuristic.py:113 special-cases the DSP.
static int hand_opt_kernel_is_cuda(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  return b != NULL && b->id == 3;
}

// tinygrad's matvec test (heuristic.py:63-69): a STORE of SUM(MUL(INDEX_E,
// INDEX_E)) whose first reduce axis is an arm of idx0, every idx0 axis in idx1.
// *out_red_aid gets that axis_id, *out_idx0 / *out_idx1 the decoded addresses.
static int hand_opt_match_matvec(Term root, u32 *out_red_aid,
                                 UopDagAddrCoeffsView *out_idx0,
                                 UopDagAddrCoeffsView *out_idx1) {
  if (root == 0 || term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) {
    return 0;
  }
  Term value = heap_read(term_val(root) + 2);
  while (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT) {
    value = uop_opt_target(value);
  }
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(value) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(value) < 1) return 0;
  u32 red_aid = uop_reduce_axis(value, 0);
  Term mul = uop_reduce_src(value);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term a = heap_read(term_val(mul) + 0);
  Term b = heap_read(term_val(mul) + 1);
  if (term_tag(a) != TAG_UOP || term_ext(a) != UOP_INDEX_E) return 0;
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_INDEX_E) return 0;
  Term addr_a = heap_read(term_val(a) + 1);
  Term addr_b = heap_read(term_val(b) + 1);
  UopDagAddrCoeffsView ca, cb;
  uop_dag_decode_addr_coeffs(addr_a, &ca);
  uop_dag_decode_addr_coeffs(addr_b, &cb);
  if (!ca.ok || !cb.ok) return 0;
  // tinygrad's `u is first_reduce_rng` over idx0's arms: the decoder gives a bare
  // RANGE arm coefficient 1.
  if (uop_dag_addr_coeff_lookup(&ca, red_aid) != 1) return 0;
  for (u32 i = 0; i < ca.n_axes; i++) {
    if (uop_dag_addr_coeff_lookup(&cb, ca.axis_ids[i]) == 0) return 0;
  }
  if (out_red_aid) *out_red_aid = red_aid;
  if (out_idx0)    *out_idx0    = ca;
  if (out_idx1)    *out_idx1    = cb;
  return 1;
}

// The stride test of heuristic.py:121-132 for axis_id `rng`: 1 iff an address
// lacks rng but holds every UPCAST/UNROLL axis.  *out_num_strides counts the
// addresses using rng, *out_sum_strides sums its coefficients over them.
static int hand_opt_upcast_buf_stride(Term root, u32 rng,
                                      u32 const *upcast_unroll_aids,
                                      u32 n_uu,
                                      u32 *out_num_strides,
                                      u32 *out_sum_strides) {
  Term addrs[64];
  u32 n_bufs = uop_dag_collect_buf_addrs(root, addrs, 64);
  int any_strideless = 0;
  u32 num_strides = 0;
  u32 sum_strides = 0;
  for (u32 i = 0; i < n_bufs; i++) {
    UopDagAddrCoeffsView cv;
    uop_dag_decode_addr_coeffs(addrs[i], &cv);
    if (!cv.ok) {
      continue;
    }
    // tinygrad tests `rng in idx.backward_slice`, which excludes idx itself
    // (uop/ops.py:265-268): a bare RANGE address, a 1-D output's, holds no
    // axis there, while its split_uop(ADD) term still adds 1 to sum_strides.
    int bare  = term_tag(addrs[i]) == TAG_UOP && term_ext(addrs[i]) == UOP_RANGE;
    u32 coeff = uop_dag_addr_coeff_lookup(&cv, rng);
    if (bare) {
      sum_strides += coeff;
      if (n_uu == 0) any_strideless = 1;
    } else if (coeff != 0) {
      num_strides += 1;
      sum_strides += coeff;
    } else {
      int all_uu_present = 1;
      for (u32 j = 0; j < n_uu; j++) {
        u32 uuc = uop_dag_addr_coeff_lookup(&cv, upcast_unroll_aids[j]);
        if (uuc == 0) {
          all_uu_present = 0; break;
        }
      }
      if (all_uu_present) any_strideless = 1;
    }
  }
  *out_num_strides = num_strides;
  *out_sum_strides = sum_strides;
  return any_strideless;
}

// The CUDA floor, set by the first CUDA kernel; -1 before.
static int OCC_FLOOR_AUTO = -1;
// The smallest launch grid an UPCAST may leave a reduce-heavy CUDA kernel: 80
// threads per SM, a tuned constant; 0 (no floor) elsewhere.  Not in tinygrad.
static u32 hand_opt_occupancy_floor(KernelEntry const *ke) {
#ifdef THVM_HAS_CUDA
  if (hand_opt_kernel_is_cuda(ke)) {
    if (OCC_FLOOR_AUTO < 0) {
      int n_sm = cuda_device_sm_count();
      OCC_FLOOR_AUTO = (n_sm > 0) ? n_sm * 80 : 0;
    }
    return (u32)OCC_FLOOR_AUTO;
  }
#else
  (void)ke;
#endif
  return 0;
}

// hand_coded_optimizations on `ke`; returns the number of opts applied.
fn u32 kernel_hand_coded_opts(struct KernelEntry *ke) {
  if (ke == NULL) return 0;
  // Done up front so a re-dispatch skips it, in its own flag: setting
  // `autotuned` would stop the BEAM pass that follows on the fire path.
  if (ke->schedule != NULL) ke->schedule->hand_coded_done = 1;
  if (!hand_coded_opts_enabled()) return 0;
  // tinygrad runs this on every renderer (postrange.py:272-276), gating sections
  // inside; a slow opt'd kernel is a renderer gap (docs/tinygrad_late_passes.md).
  // On CPU only a kernel cBLAS takes stays bare, since BLAS matches the bare DAG.
  int on_cpu = hand_opt_kernel_on_cpu(ke);
  if (on_cpu) {
    if (cpu_blas_takes(ke)) return 0;
  } else if (!hand_opt_kernel_on_gpu(ke)) {
    return 0;
  }

  u32 n_applied = 0;
  HandOptAxes ax;

  // Tensor cores (heuristic.py:27-44), then return: KOP_TC wraps the REDUCE and,
  // in place of tinygrad's M/N upcasts, every 8-multiple LOOP axis goes GLOBAL
  // (the parallel simdgroup grid).  CPU has no tensor cores.
  if (!on_cpu) {
    u32 K = 0;
    if (hand_opt_classify_matmul(ke, &K)) {
      static const u32 tc_tiles[] = {8, 16, 32};
      for (u32 i = 0; i < 3; i++) {
        u32 tile = tc_tiles[i];
        if (K == 0 || K % tile != 0) continue;
        KOpt opt = { KOP_TC, 0, tile };
        if (kernel_apply_opt(ke, opt)) { n_applied++; break; }
      }
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
    // A batched matmul (attention's mhaBmm) escapes the 2-D classifier: TC, then
    // batch, M and N GLOBAL, the batch at any extent (one simdgroup per batch and
    // 8x8 tile), while M, N and K are still one RANGE each.
    {
      u32 b_ax, b_ext, m_ax, m_ext, n_ax, n_ext, k_ext;
      if (uop_classify_batched_matmul(ke->cached_lift.store_root, &b_ax, &b_ext,
                                      &m_ax, &m_ext, &n_ax, &n_ext, &k_ext)
          && (m_ext % 8u) == 0 && (n_ext % 8u) == 0 && (k_ext % 8u) == 0) {
        static const u32 tc_tiles[] = {8, 16, 32};
        u32 applied_tc = 0;
        for (u32 i = 0; i < 3; i++) {
          u32 tile = tc_tiles[i];
          if (k_ext % tile != 0) continue;
          KOpt opt = { KOP_TC, 0, tile };
          if (kernel_apply_opt(ke, opt)) { applied_tc = 1; n_applied++; break; }
        }
        if (applied_tc) {
          KOpt gb = { KOP_GLOBAL, b_ax, b_ext };
          KOpt gm = { KOP_GLOBAL, m_ax, m_ext };
          KOpt gn = { KOP_GLOBAL, n_ax, n_ext };
          if (kernel_apply_opt(ke, gb)) n_applied++;
          if (kernel_apply_opt(ke, gm)) n_applied++;
          if (kernel_apply_opt(ke, gn)) n_applied++;
          return n_applied;
        }
      }
    }
  }

  // No IMAGE float4 upcast (heuristic.py:49-59): thvm has no image dtype.

  // Matvec (heuristic.py:61-79): GROUP the reduce by MV_THREADS_PER_ROW, then
  // LOCAL, UPCAST a global axis by MV_BLOCKSIZE and MV_ROWS_PER_THREAD; return.
  {
    int MV          = hand_opt_getenv_int("MV", 1);
    int MV_BLOCK    = hand_opt_getenv_int("MV_BLOCKSIZE", 4);
    int MV_TPR      = hand_opt_getenv_int("MV_THREADS_PER_ROW", 8);
    int MV_RPT      = hand_opt_getenv_int("MV_ROWS_PER_THREAD", 4);
    // Needs k.ren.has_local (heuristic.py:63): not on CPU.
    if (on_cpu) MV = 0;
    if (MV != 0 && (MV_BLOCK > 1 || MV_TPR > 1 || MV_RPT > 1)) {
      u32 red_aid = 0;
      UopDagAddrCoeffsView idx0v, idx1v;
      (void)idx1v;
      if (hand_opt_snapshot_axes(ke, &ax)
          && ax.n >= 2
          && hand_opt_match_matvec(ke->cached_lift.store_root,
                                   &red_aid, &idx0v, &idx1v)) {
        // Slot == axis_id here: materialize renumbers the ids 0..n-1.
        u32 red_ext = (red_aid < ax.n) ? ax.extent[red_aid] : 0;
        // tinygrad's global_idx loop (heuristic.py:70).
        for (u32 i = 0; i < ax.n; i++) {
          u8 t = ax.kax_type[i];
          if (t != KAX_LOOP && t != KAX_GLOBAL) continue;
          u32 g_ext = ax.extent[i];
          if (red_ext == 0 || g_ext == 0) continue;
          if ((red_ext % (u32)MV_TPR) != 0) continue;
          if ((g_ext   % (u32)(MV_BLOCK * MV_RPT)) != 0) continue;
          if (MV_TPR > 1) {
            KOpt o = { KOP_GROUP, red_aid, (u32)MV_TPR };
            if (kernel_apply_opt(ke, o)) n_applied++;
          }
          if (MV_BLOCK > 1) {
            // Re-snapshot and find the global axis again by its extent.
            HandOptAxes ax2;
            if (hand_opt_snapshot_axes(ke, &ax2)) {
              for (u32 j = 0; j < ax2.n; j++) {
                u8 tj = ax2.kax_type[j];
                if ((tj == KAX_LOOP || tj == KAX_GLOBAL) && ax2.extent[j] == g_ext) {
                  KOpt o = { KOP_LOCAL, ax2.axis_id[j], (u32)MV_BLOCK };
                  if (kernel_apply_opt(ke, o)) { n_applied++; break; }
                }
              }
            }
          }
          if (MV_RPT > 1) {
            HandOptAxes ax3;
            if (hand_opt_snapshot_axes(ke, &ax3)) {
              for (u32 j = 0; j < ax3.n; j++) {
                u8 tj = ax3.kax_type[j];
                if ((tj == KAX_LOOP || tj == KAX_GLOBAL) && ax3.extent[j] == g_ext) {
                  KOpt o = { KOP_UPCAST, ax3.axis_id[j], (u32)MV_RPT };
                  if (kernel_apply_opt(ke, o)) { n_applied++; break; }
                }
              }
            }
          }
          return n_applied;
        }
      }
    }
  }

  // Grouping (heuristic.py:81-90): GROUPTOP a reduce axis by 16 when the output
  // is small, then stop if a reduce is grouped.  Not on CPU, which has no local
  // memory (postrange.py:118); a GROUP_REDUCE also blocks parallel accumulators.
  if (!on_cpu) {
    int NOLOCALS = hand_opt_getenv_int("NOLOCALS", 0);
    if (hand_opt_snapshot_axes(ke, &ax)) {
      u64 olp = hand_opt_output_loop_product(&ax);
      // tinygrad drops 2048 to 240 on QCOM; thvm does under NOLOCALS=1.
      u64 thr = NOLOCALS ? 240 : 2048;
      if (olp <= thr) {
        u32 red_aids[MAX_AXES]; u32 red_exts[MAX_AXES]; u32 n_red = 0;
        for (u32 i = 0; i < ax.n; i++) {
          if (ax.kax_type[i] == KAX_REDUCE) {
            red_aids[n_red] = ax.axis_id[i];
            red_exts[n_red] = ax.extent[i];
            n_red++;
          }
        }
        // tinygrad takes the first of three reduce axes that splits by 16, thvm
        // the largest: the fewest strided steps per thread (rmu_emit_group_reduce
        // loops the other reduce axes serially).
        u32 sz = 16;
        i32 best_idx = -1;
        u32 best_ext = 0;
        for (u32 i = 0; i < n_red; i++) {
          if (red_exts[i] < sz) continue;
          if (red_exts[i] % sz != 0) continue;
          if (red_exts[i] > best_ext) {
            best_ext = red_exts[i];
            best_idx = (i32)i;
          }
        }
        if (best_idx >= 0) {
          u32 axis = red_aids[best_idx];
          KOpt opt = { KOP_GROUPTOP, axis, sz };
          int ok = kernel_apply_opt(ke, opt);
          if (ok) n_applied++;
        }
      }
    }
    if (hand_opt_snapshot_axes(ke, &ax)) {
      if (hand_opt_group_for_reduces(&ax) > 0) return n_applied;
    }
  }

  // The masked-axis upcast (heuristic.py:94-110) is not ported.

  // Upcasts (heuristic.py:112-138): while the output has >= 1024 elements and
  // the upcast size is under 32, UPCAST by 3 or 4 the least-strided axis.
  {
    u32 occ_min_grid = hand_opt_occupancy_floor(ke);
    u32 upcasted_set[MAX_AXES];
    u32 n_upcasted_set = 0;
    while (1) {
      if (!hand_opt_snapshot_axes(ke, &ax)) break;
      u64 olp = hand_opt_output_loop_product(&ax);
      u64 us  = hand_opt_upcast_size(&ax);
      if (olp < 1024 || us >= 32) break;
      int reduce_heavy = (occ_min_grid > 0) && hand_opt_is_reduce_heavy(&ax);

      // The stride test looks axes up by axis_id, not slot.
      u32 uu_aids[MAX_AXES]; u32 n_uu = 0;
      for (u32 i = 0; i < ax.n; i++) {
        if (ax.kax_type[i] == KAX_UPCAST || ax.kax_type[i] == KAX_UNROLL) {
          uu_aids[n_uu++] = ax.axis_id[i];
        }
      }

      u32 dims[MAX_AXES];
      u32 n_dims = hand_opt_upcastable_dims(&ax, dims);

      // Least (num_strides, sum_strides, axis, amount): sorted(xb_choices)[0].
      int have_best = 0;
      u32 best_num = 0, best_sum = 0, best_axis = 0, best_amt = 0;
      for (u32 di = 0; di < n_dims; di++) {
        u32 axis = dims[di];
        int already = 0;
        for (u32 i = 0; i < n_upcasted_set; i++) {
          if (upcasted_set[i] == axis) { already = 1; break; }
        }
        if (already) continue;
        static const u32 amounts[] = { 3, 4 };
        for (u32 ai = 0; ai < 2; ai++) {
          u32 amt = amounts[ai];
          if (ax.extent[axis] % amt != 0) continue;
          u32 num = 0, sum = 0;
          if (!hand_opt_upcast_buf_stride(ke->cached_lift.store_root,
                                          ax.axis_id[axis], uu_aids, n_uu,
                                          &num, &sum)) {
            continue;   // no buf strideless on this axis
          }
          if (!have_best
              || num < best_num
              || (num == best_num && sum < best_sum)
              || (num == best_num && sum == best_sum && axis < best_axis)
              || (num == best_num && sum == best_sum && axis == best_axis && amt < best_amt)) {
            have_best = 1;
            best_num = num; best_sum = sum; best_axis = axis; best_amt = amt;
          }
        }
      }
      if (!have_best) {
        break;
      }
      // Not in tinygrad: stop before an UPCAST leaves a reduce-heavy kernel's
      // grid under the occupancy floor.
      if (reduce_heavy) {
        u64 olp_after = olp / (u64)best_amt;
        if (olp_after < (u64)occ_min_grid) {
          break;
        }
      }
      KOpt opt = { KOP_UPCAST, ax.axis_id[best_axis], best_amt };
      if (!kernel_apply_opt(ke, opt)) {
        break;
      }
      n_applied++;
      if (n_upcasted_set < MAX_AXES) upcasted_set[n_upcasted_set++] = best_axis;
    }
  }

  // Unroll the last reduce axis (heuristic.py:140-154): fully when its extent is
  // <= 32, a second one too when both are <= 3, else by 4.  Not on CUDA, where
  // UNROLL on top of UPCAST makes nvrtc spill.
  if (!hand_opt_kernel_is_cuda(ke)) {
    if (hand_opt_snapshot_axes(ke, &ax)) {
      u32 unr[MAX_AXES];
      u32 n_unr = hand_opt_unrollable_dims(&ax, ke->cached_lift.store_root, unr);
      u64 us = hand_opt_upcast_size(&ax);
      u8 unroll_t[] = { KAX_UNROLL };
      u32 n_existing_unroll = hand_opt_axes_of_n(&ax, unroll_t, 1);
      if (n_unr > 0 && (us <= 4 || n_existing_unroll == 0) && us < 64) {
        u32 last = unr[n_unr - 1];
        u32 ext  = ax.extent[last];
        if (ext <= 32) {
          // tinygrad's split by 0 is the full extent (heuristic.py:145); thvm
          // needs the extent itself.
          KOpt opt = { KOP_UNROLL, ax.axis_id[last], ext };
          if (kernel_apply_opt(ke, opt)) {
            n_applied++;
            // heuristic.py:146-148; re-snapshot, the UNROLL renumbered the axes.
            if (ext <= 3 && hand_opt_snapshot_axes(ke, &ax)) {
              u32 unr2[MAX_AXES];
              u32 n_unr2 = hand_opt_unrollable_dims(&ax, ke->cached_lift.store_root,
                                                    unr2);
              if (n_unr2 > 0) {
                u32 last2 = unr2[n_unr2 - 1];
                u32 ext2  = ax.extent[last2];
                if (ext2 <= 3) {
                  KOpt opt2 = { KOP_UNROLL, ax.axis_id[last2], ext2 };
                  if (kernel_apply_opt(ke, opt2)) n_applied++;
                }
              }
            }
          }
        } else {
          if (ext % 4 == 0) {
            KOpt opt = { KOP_UNROLL, ax.axis_id[last], 4 };
            if (kernel_apply_opt(ke, opt)) n_applied++;
          }
        }
      }
    }
  }

  // Nothing upcast: UPCAST the last upcastable axis by 4 (heuristic.py:156-159).
  {
    if (hand_opt_snapshot_axes(ke, &ax)) {
      if (hand_opt_upcasted(&ax) == 0) {
        u32 dims[MAX_AXES];
        u32 n_dims = hand_opt_upcastable_dims(&ax, dims);
        if (n_dims > 0) {
          u32 last = dims[n_dims - 1];
          if (ax.extent[last] % 4 == 0) {
            KOpt opt = { KOP_UPCAST, ax.axis_id[last], 4 };
            if (kernel_apply_opt(ke, opt)) n_applied++;
          }
        }
      }
    }
  }

  // Locals (heuristic.py:163-191, the non-QCOM branch): rank GLOBAL/LOOP axes
  // expand-first, give each the largest of 16, 8, 4, 3, 2 (or 32 on axis 0) that
  // divides it within a 128-thread block, and LOCAL the first three; not on CPU.
  if (!on_cpu) {
    int NOLOCALS = hand_opt_getenv_int("NOLOCALS", 0);
    // NOLOCALS=1 skips them; uop_dag_apply_kopt declines KOP_NOLOCALS itself.
    if (NOLOCALS) {
      KOpt opt = { KOP_NOLOCALS, 0, 0 };
      if (kernel_apply_opt(ke, opt)) n_applied++;
    } else {
      if (hand_opt_snapshot_axes(ke, &ax)) {
        // expand: some address does not use the axis (heuristic.py:179).
        Term addrs[64];
        u32 n_bufs = uop_dag_collect_buf_addrs(ke->cached_lift.store_root,
                                                   addrs, 64);
        UopDagAddrCoeffsView decoded[64];
        u32 n_decoded = 0;
        for (u32 i = 0; i < n_bufs; i++) {
          uop_dag_decode_addr_coeffs(addrs[i], &decoded[n_decoded]);
          if (decoded[n_decoded].ok) n_decoded++;
        }

        typedef struct { u32 axis; int expand; } LocalCand;
        LocalCand cands[MAX_AXES];
        u32 n_cands = 0;
        for (u32 i = 0; i < ax.n; i++) {
          u8 t = ax.kax_type[i];
          if (t != KAX_LOOP && t != KAX_GLOBAL) continue;
          // tinygrad skips non-CONST extents; a kvar one fails at the split.
          int expand = 0;
          for (u32 j = 0; j < n_decoded; j++) {
            if (uop_dag_addr_coeff_lookup(&decoded[j], i) == 0) { expand = 1; break; }
          }
          cands[n_cands].axis   = i;
          cands[n_cands].expand = expand;
          n_cands++;
        }
        // tinygrad's key (-expand, -axis) (heuristic.py:182).
        for (u32 i = 1; i < n_cands; i++) {
          LocalCand k = cands[i];
          i32 j = (i32)i - 1;
          while (j >= 0) {
            int ex_j = cands[j].expand, ex_k = k.expand;
            if (ex_j > ex_k) break;
            if (ex_j == ex_k && cands[j].axis > k.axis) break;
            cands[j + 1] = cands[j];
            j--;
          }
          cands[j + 1] = k;
        }

        typedef struct { u32 axis; u32 sz; } LocalPick;
        LocalPick picks[MAX_AXES];
        u32 n_picks = 0;
        u32 local_running = 1;
        for (u32 i = 0; i < n_cands; i++) {
          u32 axis = cands[i].axis;
          u32 ext  = ax.extent[axis];
          u32 facs[6]; u32 nf = 0;
          if (axis == 0) facs[nf++] = 32;
          facs[nf++] = 16; facs[nf++] = 8; facs[nf++] = 4;
          facs[nf++] = 3;  facs[nf++] = 2;
          u32 picked = 0;
          for (u32 fi = 0; fi < nf; fi++) {
            u32 f = facs[fi];
            if (ext % f != 0) continue;
            if ((u64)local_running * (u64)f > 128u) continue;
            picked = f; break;
          }
          if (picked > 0) {
            picks[n_picks].axis = axis;
            picks[n_picks].sz   = picked;
            n_picks++;
            local_running *= picked;
          }
        }
        // sorted(to_local[:3]) (heuristic.py:187).
        if (n_picks > 3) n_picks = 3;
        for (u32 i = 1; i < n_picks; i++) {
          LocalPick k = picks[i]; i32 j = (i32)i - 1;
          while (j >= 0 && picks[j].axis > k.axis) {
            picks[j + 1] = picks[j]; j--;
          }
          picks[j + 1] = k;
        }
        // A split puts its inner range in the next slot, moving later picks up
        // one; tinygrad's deleted_shape counts the axes a split uses up instead,
        // since its extent-1 ranges leave k.rngs.
        u32 shift = 0;
        for (u32 i = 0; i < n_picks; i++) {
          u32 current_axis = picks[i].axis + shift;
          u32 sz = picks[i].sz;
          HandOptAxes axc;
          if (!hand_opt_snapshot_axes(ke, &axc)) break;
          if (current_axis >= axc.n) continue;
          KOpt opt = { KOP_LOCAL, axc.axis_id[current_axis], sz };
          if (kernel_apply_opt(ke, opt)) {
            n_applied++;
            shift++;
          }
        }
      }
    }
  }

  return n_applied;
}

// 1 when the heuristic is on and has yet to run on `ke`, whose opts are open.
fn int kernel_should_hand_code_opts(struct KernelEntry const *ke) {
  if (!hand_coded_opts_enabled()) return 0;
  if (ke == NULL || ke->no_opts) return 0;
  if (ke->schedule != NULL && ke->schedule->hand_coded_done) return 0;
  return 1;
}

