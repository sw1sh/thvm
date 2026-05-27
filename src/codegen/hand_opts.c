// codegen/hand_opts.c -- faithful port of tinygrad's
// hand_coded_optimizations heuristic (tinygrad/codegen/opt/heuristic.py).
//
// Strategy: mirror tinygrad's `hand_coded_optimizations(k)` line-by-line.
// The heuristic operates on shape primitives only -- it does NOT know
// the words "conv", "matmul", "tileable", etc.  This file is therefore
// shape-primitive only.  Per-section comments cite the upstream
// heuristic.py line range.
//
// Sections ported:
//   1.  TC  (heuristic.py 27-46)     -- thvm-specific Metal-only marker
//                                       apply + 8-multiple GLOBAL promote;
//                                       returns on success
//   2.  IMAGE float4 (51-62)         -- SKIPPED (thvm has no ImageDType)
//   3.  MATVEC (65-82)               -- PORTED
//   4.  GROUPTOP early gate (84-90)  -- PORTED (kernel_apply_opt declines
//                                       KOP_GROUPTOP today, so the section
//                                       is structurally faithful but no-op
//                                       in practice; will fire when the
//                                       DAG path adds GROUP support)
//   5.  Mask UPCAST  (97-106)        -- SKIPPED (TODO: detect IWHERE
//                                       mask axes from the DAG)
//   6.  Main UPCAST loop (108-134)   -- PORTED (the load-bearing pass)
//   7.  UNROLL last reduce (138-150) -- PORTED
//   8.  Easy UPCAST fallback (153-5) -- PORTED
//   9.  LOCAL (159-176)              -- PORTED
//   10. THREAD (180-189)             -- SKIPPED (thvm has no KOP_THREAD)
//   11. NOLOCALS env                 -- PORTED (no-op when backend declines)
//
// Backend gate: Metal id=2 only.  CUDA enablement is a separate session.

// --- env knob ------------------------------------------------------
// Default ON.  HAND_CODED_OPTS=0 disables; NOOPT=1 (tinygrad's
// inverse-sense knob) also disables and wins if both set.
// Memoised: -1 = uninitialised, 0 = off, 1 = on.
static int HAND_CODED_OPTS_ENABLED = -1;
static int hand_coded_opts_enabled(void) {
  if (HAND_CODED_OPTS_ENABLED < 0) {
    char const *noopt = getenv("NOOPT");
    if (noopt != NULL && noopt[0] != '\0' && noopt[0] != '0') {
      HAND_CODED_OPTS_ENABLED = 0;            // tinygrad NOOPT=1
    } else {
      char const *e = getenv("HAND_CODED_OPTS");
      HAND_CODED_OPTS_ENABLED = (e != NULL && e[0] == '0') ? 0 : 1;
    }
  }
  return HAND_CODED_OPTS_ENABLED;
}

// Integer env reader; falls back to `dflt` if unset / unparseable.
// Mirrors tinygrad helpers.getenv for the MV_* + NOLOCALS knobs.
static int hand_opt_getenv_int(char const *name, int dflt) {
  char const *e = getenv(name);
  if (e == NULL || e[0] == '\0') return dflt;
  int sign = 1, v = 0; char const *p = e;
  if (*p == '-') { sign = -1; p++; }
  if (!(*p >= '0' && *p <= '9')) return dflt;
  while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
  return sign * v;
}

// --- axis snapshot -------------------------------------------------
// Unified view of "the kernel's current axis list" -- mirrors tinygrad's
// k.rngs / k.full_shape / k.axis_types.  The SLOT INDEX is the ordinal
// position; the axis_id may not be zero-based / contiguous (kernels
// lifted from the production rangeify pipeline carry global axis_ids
// that start anywhere in 0..u32_max).  KOpt's `axis` field targets by
// axis_id (uop_dag_apply_kopt looks up the RANGE leaf by id), so
// consumers must translate slot -> axis_id when building the KOpt.
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

// --- tinygrad scheduler primitives over HandOptAxes -----------------

// tinygrad: k.upcastable_dims = [i for i in axes_of(GLOBAL, LOCAL, LOOP)
//                                if full_shape[i] > 1].  thvm's axis types
// are KAX_LOOP/GLOBAL/LOCAL (no separate WARP); LOOP corresponds to GLOBAL
// pre-promotion.  Returns count; out[] gets the axis indices.
static u32 hand_opt_upcastable_dims(HandOptAxes const *ax, u32 *out) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) {
    u8 t = ax->kax_type[i];
    if ((t == KAX_LOOP || t == KAX_GLOBAL || t == KAX_LOCAL) && ax->extent[i] > 1) {
      out[n++] = i;
    }
  }
  return n;
}

// tinygrad: k.unrollable_dims = [i for i in axes_of(GROUP_REDUCE, REDUCE)
//                                if full_shape[i] > 1].
static u32 hand_opt_unrollable_dims(HandOptAxes const *ax, u32 *out) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) {
    u8 t = ax->kax_type[i];
    if ((t == KAX_REDUCE || t == KAX_GROUP_REDUCE) && ax->extent[i] > 1) {
      out[n++] = i;
    }
  }
  return n;
}

// tinygrad: k.axes_of(*types).
static u32 hand_opt_axes_of_n(HandOptAxes const *ax, u8 const *types, u32 n_types) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) {
    for (u32 j = 0; j < n_types; j++) {
      if (ax->kax_type[i] == types[j]) { n++; break; }
    }
  }
  return n;
}

// tinygrad: k.upcast_size() = prod(full_shape[a] for a in axes_of(UPCAST, UNROLL)).
static u64 hand_opt_upcast_size(HandOptAxes const *ax) {
  u64 p = 1;
  for (u32 i = 0; i < ax->n; i++) {
    if (ax->kax_type[i] == KAX_UPCAST || ax->kax_type[i] == KAX_UNROLL) {
      p *= (u64)ax->extent[i];
    }
  }
  return p;
}

// tinygrad: prod(output_shape[i] for i in upcastable_dims).
// output_shape replaces extents on REDUCE/UNROLL/GROUP_REDUCE axes with 1;
// upcastable_dims is GLOBAL/LOCAL/LOOP only, so equivalently just the
// product of upcastable extents.
static u64 hand_opt_output_loop_product(HandOptAxes const *ax) {
  u32 dims[MAX_AXES];
  u32 n = hand_opt_upcastable_dims(ax, dims);
  u64 p = 1;
  for (u32 i = 0; i < n; i++) p *= (u64)ax->extent[dims[i]];
  return p;
}

// tinygrad: k.upcasted = len(axes_of(UPCAST, UNROLL)).
static u32 hand_opt_upcasted(HandOptAxes const *ax) {
  u8 t[] = { KAX_UPCAST, KAX_UNROLL };
  return hand_opt_axes_of_n(ax, t, 2);
}

// tinygrad: k.group_for_reduces = len(axes_of(GROUP_REDUCE)).
static u32 hand_opt_group_for_reduces(HandOptAxes const *ax) {
  u32 n = 0;
  for (u32 i = 0; i < ax->n; i++) if (ax->kax_type[i] == KAX_GROUP_REDUCE) n++;
  return n;
}

// --- matmul classification (for TC kept as-is from prior commit) ----
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

// --- GPU gate -------------------------------------------------------
// CPU kernels route matmul/gemv/dot/conv through cBLAS / the clang-JIT'd
// interpreter from the bare (un-OPT'd) DAG; applying a hand-coded opt
// mutates the DAG so those classifiers stop recognising it.  GPU
// backends have no BLAS fallback -- the structural-lift JIT is the only
// path, so the shape-primitive heuristic always helps.  METAL_BACKEND.id
// == 2, CUDA_BACKEND.id == 3 (cpu == 1) -- see kautotune_backend_id.
static int hand_opt_kernel_on_gpu(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  return b != NULL && (b->id == 2 || b->id == 3);
}

// Returns 1 if the kernel will run on CUDA (backend id == 3).  Used for
// target-aware heuristic branching: nvrtc's C++ frontend is sensitive to
// register pressure in ways Metal/MSL is not, so a few sections of the
// hand-coded heuristic are CUDA-suppressed.  Mirrors tinygrad's
// `k.ren.target.device == "DSP"` / AMX skip pattern (heuristic.py 37, 109).
static int hand_opt_kernel_is_cuda(KernelEntry const *ke) {
  Backend *b = NULL;
  if (ke != NULL && ke->output_tid > 0 && ke->output_tid < TENS_NEXT) {
    b = TENS[ke->output_tid].backend;
  }
  if (b == NULL) b = DEFAULT_BACKEND;
  return b != NULL && b->id == 3;
}

// --- matvec detection (heuristic.py 65-82) --------------------------
// Returns 1 if the kernel matches tinygrad's matvec shape:
//   STORE.value is REDUCE_SUM of MUL of INDEX_E,INDEX_E
//   AND first reduce axis appears directly in idx0 as an IADD arm
//   AND every axis referenced in idx0 is also referenced in idx1.
// Writes the first reduce axis_id to *out_red_aid + decoded idx1 coeffs
// into *out_idx1 so the caller can check divisibility.
static int hand_opt_match_matvec(Term root, u32 *out_red_aid,
                                 UopDagAddrCoeffsView *out_idx0,
                                 UopDagAddrCoeffsView *out_idx1) {
  if (root == 0 || term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) {
    return 0;
  }
  Term value = heap_read(term_val(root) + 2);
  // Peel a leading OPT(_, _) wrapper if present (uop_recognise_tc /
  // _conv may have wrapped the REDUCE).
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
  // The first reduce range must appear directly in idx0 (i.e., as an
  // axis with coeff 1 -- "u is first_reduce_rng" in tinygrad).  Our
  // decoder normalises a bare RANGE leaf to coeff 1, so check that.
  if (uop_dag_addr_coeff_lookup(&ca, red_aid) != 1) return 0;
  // All idx0 axes must also be in idx1.
  for (u32 i = 0; i < ca.n_axes; i++) {
    if (uop_dag_addr_coeff_lookup(&cb, ca.axis_ids[i]) == 0) return 0;
  }
  if (out_red_aid) *out_red_aid = red_aid;
  if (out_idx0)    *out_idx0    = ca;
  if (out_idx1)    *out_idx1    = cb;
  return 1;
}

// --- main UPCAST loop's stride-heuristic helper (heuristic.py 115-128).
// For one candidate axis `rng` (axis_id), with current set of UPCAST/
// UNROLL axes `upcast_unroll_aids`, walk the kernel's INDEX_E.addrs and
// compute:
//   - any_strideless: 1 iff at least one buf has rng absent from its
//     addr AND every UPCAST/UNROLL axis present in that buf's addr (the
//     "rng not in b.idx.backward_slice && all(r2 in ...)" test);
//   - num_strides:    count of bufs whose addr references rng;
//   - sum_strides:    sum of rng's coefficients across all bufs that
//     reference it (tinygrad walks split_uop(ADD) and counts bare
//     RANGE arms as 1 and IMUL(RANGE,CONST) arms as CONST; the decoder
//     already accumulates that into the per-axis coefficient).
// Returns any_strideless; the caller uses (num_strides, sum_strides)
// to sort the candidate list.
static int hand_opt_upcast_buf_stride(Term root, u32 rng,
                                      u32 const *upcast_unroll_aids,
                                      u32 n_uu,
                                      u32 *out_num_strides,
                                      u32 *out_sum_strides) {
  Term addrs[64];
  u32 n_bufs = uop_dag_collect_index_e_addrs(root, addrs, 64);
  int any_strideless = 0;
  u32 num_strides = 0;
  u32 sum_strides = 0;
  for (u32 i = 0; i < n_bufs; i++) {
    UopDagAddrCoeffsView cv;
    uop_dag_decode_addr_coeffs(addrs[i], &cv);
    if (!cv.ok) continue;
    u32 coeff = uop_dag_addr_coeff_lookup(&cv, rng);
    if (coeff != 0) {
      num_strides += 1;
      sum_strides += coeff;
    } else {
      // rng absent from this buf.  Check: are all existing UPCAST/UNROLL
      // axes also absent?  tinygrad's test is "rng not in BS && all(r2 in
      // BS for r2 in upcast/unroll ranges)" -- i.e., "the candidate axis
      // is absent but the already-upcasted ones are present".  An UPCAST
      // axis present in EVERY buf means upcasting on rng will not
      // conflict (rng's stride-0-on-this-buf vectorizes the load).
      int all_uu_present = 1;
      for (u32 j = 0; j < n_uu; j++) {
        if (uop_dag_addr_coeff_lookup(&cv, upcast_unroll_aids[j]) == 0) {
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

// --- the heuristic -------------------------------------------------
// Returns the number of opts successfully applied.
fn u32 kernel_hand_coded_opts(struct KernelEntry *ke) {
  if (ke == NULL) return 0;
  // Mark "decided" up front (whether or not anything applies) so a
  // re-dispatch doesn't re-run.  Mirrors kernel_autotune's
  // ke->schedule->autotuned = 1 at the start.
  if (ke->schedule != NULL) ke->schedule->autotuned = 1;
  if (!hand_coded_opts_enabled()) return 0;
  // GPU gate: runs on both Metal (b->id == 2) and CUDA (b->id == 3),
  // matching tinygrad's spec (heuristic.py applies to all renderers).
  // Intra-heuristic target-aware skips (see hand_opt_kernel_is_cuda
  // users below) are the tinygrad-faithful way to handle backend
  // peculiarities -- mirrors heuristic.py:37 (AMX skip) and :109
  // (DSP skip).  See docs/tinygrad_late_passes.md for the V100
  // measurement (warm step is currently 130x naive baseline; the
  // remediation is to port tinygrad/renderer/ptx.py, not to narrow
  // the gate).
  if (!hand_opt_kernel_on_gpu(ke)) return 0;

  u32 n_applied = 0;
  HandOptAxes ax;

  // ---- Section 1: TC (heuristic.py 27-46) ----------------------------
  // thvm-specific: KOP_TC marker (uop_recognise_tc has usually already
  // wrapped the matmul STORE in OPT(_, TC, 0); this is the explicit
  // record on the schedule path) + parallel-TC GLOBAL promote for any
  // 8-multiple output axis.  Tinygrad RETURNs after TC + M/N UPCAST;
  // we mirror that.
  {
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
  }

  // ---- Section 2: IMAGE float4 -- SKIPPED (no ImageDType in thvm) ----

  // ---- Section 3: MATVEC (heuristic.py 65-82) -----------------------
  // SUM-of-MUL-of-INDEX_E,INDEX_E where the first reduce axis appears
  // directly in idx0 AND every idx0 axis appears in idx1.  Apply
  // GROUP(reduce, MV_THREADS_PER_ROW), LOCAL(global, MV_BLOCKSIZE),
  // UPCAST(global, MV_ROWS_PER_THREAD), then RETURN.
  {
    int MV          = hand_opt_getenv_int("MV", 1);
    int MV_BLOCK    = hand_opt_getenv_int("MV_BLOCKSIZE", 4);
    int MV_TPR      = hand_opt_getenv_int("MV_THREADS_PER_ROW", 8);
    int MV_RPT      = hand_opt_getenv_int("MV_ROWS_PER_THREAD", 4);
    if (MV != 0 && (MV_BLOCK > 1 || MV_TPR > 1 || MV_RPT > 1)) {
      u32 red_aid = 0;
      UopDagAddrCoeffsView idx0v, idx1v;
      (void)idx1v;
      if (hand_opt_snapshot_axes(ke, &ax)
          && ax.n >= 2
          && hand_opt_match_matvec(ke->cached_lift.store_root,
                                   &red_aid, &idx0v, &idx1v)) {
        // ax indices are axis_ids (snapshot guarantee).
        u32 red_ext = (red_aid < ax.n) ? ax.extent[red_aid] : 0;
        // Find a GLOBAL/LOOP axis (the "global_idx" loop in tinygrad,
        // which iterates `axes_of(AxisType.GLOBAL)`).
        for (u32 i = 0; i < ax.n; i++) {
          u8 t = ax.kax_type[i];
          if (t != KAX_LOOP && t != KAX_GLOBAL) continue;
          u32 g_ext = ax.extent[i];
          if (red_ext == 0 || g_ext == 0) continue;
          if ((red_ext % (u32)MV_TPR) != 0) continue;
          if ((g_ext   % (u32)(MV_BLOCK * MV_RPT)) != 0) continue;
          // Apply GROUP on the FIRST reduce axis (tinygrad uses axis 0
          // which is the index into axes_of(REDUCE); we use the axis_id).
          if (MV_TPR > 1) {
            KOpt o = { KOP_GROUP, (u8)red_aid, (u32)MV_TPR };
            if (kernel_apply_opt(ke, o)) n_applied++;
          }
          if (MV_BLOCK > 1) {
            // After GROUP-split, axis indices may have shifted; re-snapshot
            // to find the current global axis position.
            HandOptAxes ax2;
            if (hand_opt_snapshot_axes(ke, &ax2)) {
              // Find an axis with the same extent g_ext that is still LOOP/GLOBAL.
              for (u32 j = 0; j < ax2.n; j++) {
                u8 tj = ax2.kax_type[j];
                if ((tj == KAX_LOOP || tj == KAX_GLOBAL) && ax2.extent[j] == g_ext) {
                  KOpt o = { KOP_LOCAL, (u8)ax2.axis_id[j], (u32)MV_BLOCK };
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
                  KOpt o = { KOP_UPCAST, (u8)ax3.axis_id[j], (u32)MV_RPT };
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

  // ---- Section 4: GROUPTOP early gate (heuristic.py 84-90) ----------
  // if prod(output_loop_dims) <= 2048 (240 if NOLOCALS):
  //   for axis, sz in product((0,1,2), (16,)): try GROUPTOP(axis, sz);
  //   break on first success.
  // Then: if group_for_reduces > 0: return.
  //
  // tinygrad's axis indexes into `axes_of(AxisType.REDUCE)` -- the i-th
  // REDUCE axis among all axes.  We translate (0,1,2) -> the
  // corresponding axis_id by scanning the snapshot for REDUCE-kind axes.
  //
  // Env gate THVM_GROUPTOP=0 disables the gate entirely (default ON);
  // useful for A/B vs the pre-GROUP_REDUCE baseline while the
  // CUDA-launch-context corruption is investigated.
  if (hand_opt_getenv_int("THVM_GROUPTOP", 1) != 0)
  {
    int NOLOCALS = hand_opt_getenv_int("NOLOCALS", 0);
    if (hand_opt_snapshot_axes(ke, &ax)) {
      u64 olp = hand_opt_output_loop_product(&ax);
      u64 thr = NOLOCALS ? 240 : 2048;
      if (olp <= thr) {
        // red_aids[k] holds the axis_id of the k-th REDUCE axis; red_exts[k]
        // its extent.  Snapshot positions and axis_ids may not match
        // (uop_dag_collect_axes returns axes in DAG-walk order, axis_ids
        // can have gaps from prior axis-id remapping passes).
        u32 red_aids[MAX_AXES]; u32 red_exts[MAX_AXES]; u32 n_red = 0;
        for (u32 i = 0; i < ax.n; i++) {
          if (ax.kax_type[i] == KAX_REDUCE) {
            red_aids[n_red] = ax.axis_id[i];
            red_exts[n_red] = ax.extent[i];
            n_red++;
          }
        }
        static int trace = -1;
        if (trace < 0) {
          const char *e = getenv("THVM_GROUP_REDUCE_TRACE");
          trace = (e != NULL && e[0] != '0') ? 1 : 0;
        }
        if (trace) {
          fprintf(stderr, "[group] n_red=%u olp=%llu reduces:", n_red,
                  (unsigned long long)olp);
          for (u32 i = 0; i < n_red; i++) {
            fprintf(stderr, " a%u(ext=%u)", red_aids[i], red_exts[i]);
          }
          fputc('\n', stderr);
        }
        // rmu_emit_group_reduce now handles multi-axis (serial REDUCE
        // loops around the cooperative GROUP axis), so apply GROUPTOP
        // to the LARGEST reduce axis that divides evenly by 16.  Picking
        // the largest axis minimizes per-thread iteration count for the
        // strided walk.
        // Cooperative size: tinygrad's GROUPTOP default = 16.  A/B on
        // V100 beautiful_mnist showed sz=16 beats sz=32 (more kernels
        // divide 16 cleanly; smaller block yields better occupancy for
        // shared-mem-bound reduces).  Override via THVM_GROUP_SZ.
        u32 sz = (u32)hand_opt_getenv_int("THVM_GROUP_SZ", 16);
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
          KOpt opt = { KOP_GROUPTOP, (u8)axis, sz };
          int ok = kernel_apply_opt(ke, opt);
          if (trace) fprintf(stderr,
              "[group] apply GROUPTOP a%u sz=%u (best of %u red axes) -> %d\n",
              axis, sz, n_red, ok);
          if (ok) n_applied++;
        } else if (trace) {
          fprintf(stderr, "[group] skip: no reduce axis divides %u\n", sz);
        }
      }
    }
    if (hand_opt_snapshot_axes(ke, &ax)) {
      if (hand_opt_group_for_reduces(&ax) > 0) return n_applied;
    }
  }

  // ---- Section 5: Mask UPCAST -- SKIPPED ----------------------------
  // TODO: detect IWHERE mask-axis structure from the DAG.

  // ---- Section 6: Main UPCAST loop (heuristic.py 108-134) -----------
  // while prod(output_loop_dims) >= 1024 AND upcast_size() < 32:
  //   for axis in upcastable_dims, for amount in (3, 4):
  //     skip if already upcasted on `axis` this loop, skip if
  //     full_shape[axis] % amount != 0;
  //     run the buffer-stride heuristic (any buf with rng absent AND all
  //     existing UPCAST/UNROLL axes present); collect (num_strides,
  //     sum_strides, axis, amount); sort; apply best UPCAST.
  {
    u32 upcasted_set[MAX_AXES];
    u32 n_upcasted_set = 0;
    while (1) {
      if (!hand_opt_snapshot_axes(ke, &ax)) break;
      u64 olp = hand_opt_output_loop_product(&ax);
      u64 us  = hand_opt_upcast_size(&ax);
      if (olp < 1024 || us >= 32) break;

      // Build list of current UPCAST/UNROLL axis_ids for the stride test.
      u32 uu_aids[MAX_AXES]; u32 n_uu = 0;
      for (u32 i = 0; i < ax.n; i++) {
        if (ax.kax_type[i] == KAX_UPCAST || ax.kax_type[i] == KAX_UNROLL) {
          uu_aids[n_uu++] = i;
        }
      }

      u32 dims[MAX_AXES];
      u32 n_dims = hand_opt_upcastable_dims(&ax, dims);

      // Best candidate so far (sorted ascending by (num_strides, sum_strides, axis, amount)).
      int have_best = 0;
      u32 best_num = 0, best_sum = 0, best_axis = 0, best_amt = 0;
      for (u32 di = 0; di < n_dims; di++) {
        u32 axis = dims[di];
        // Skip if axis was already chosen in a prior iteration of THIS while loop.
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
                                          axis, uu_aids, n_uu, &num, &sum)) {
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
      if (!have_best) break;
      KOpt opt = { KOP_UPCAST, (u8)ax.axis_id[best_axis], best_amt };
      if (!kernel_apply_opt(ke, opt)) break;
      n_applied++;
      if (n_upcasted_set < MAX_AXES) upcasted_set[n_upcasted_set++] = best_axis;
    }
  }

  // ---- Section 7: UNROLL last reduce dim (heuristic.py 138-150) -----
  // if unrollable_dims AND (upcast_size <= 4 OR no UNROLL axes yet)
  //                    AND upcast_size < 64:
  //   if full_shape[last_unrollable] <= 32: UNROLL(last, 0)
  //     if s <= 3 AND another unrollable AND its extent <= 3: UNROLL again
  //   else: if full_shape[last_unrollable] % 4 == 0: UNROLL(last, 4)
  //
  // thvm's KOP_UNROLL split needs k > 0, so we translate `0` (tinygrad's
  // "full extent" sentinel) into the actual extent.
  //
  // THVM RENDERER LIMIT: a FULL-extent UNROLL (k == extent, leaving the
  // outer REDUCE axis at extent 1) confuses rmu_emit_store_reduce's
  // reduce-range matcher: the REDUCE node's axis field no longer names
  // any live RANGE, so the renderer emits a bare `#pragma unroll` with
  // no following loop and the kernel produces wrong output.  Until the
  // renderer's reduce matcher is fixed to follow UNROLL'd reduce axes,
  // suppress the full-extent UNROLL.  The split-by-4 branch (factor 4 <
  // extent) is unaffected and still applies.
  //
  // CUDA SKIP: stacking UNROLL on top of UPCAST blows V100 register
  // pressure -- nvrtc spills, the kernel runs at a fraction of peak.
  // Tinygrad gets away with the same heuristic because their PTX
  // renderer schedules registers directly; thvm's C-source path has no
  // such control.  Mirrors tinygrad's backend-aware skips at
  // heuristic.py:37 (AMX) + 109 (DSP).
  if (!hand_opt_kernel_is_cuda(ke)) {
    if (hand_opt_snapshot_axes(ke, &ax)) {
      u32 unr[MAX_AXES];
      u32 n_unr = hand_opt_unrollable_dims(&ax, unr);
      u64 us = hand_opt_upcast_size(&ax);
      u8 unroll_t[] = { KAX_UNROLL };
      u32 n_existing_unroll = hand_opt_axes_of_n(&ax, unroll_t, 1);
      if (n_unr > 0 && (us <= 4 || n_existing_unroll == 0) && us < 64) {
        u32 last = unr[n_unr - 1];
        u32 ext  = ax.extent[last];
        if (ext <= 32) {
          // tinygrad: UNROLL(last, 0) -- full extent.
          // thvm: see THVM RENDERER LIMIT in the comment above; skip
          // full-extent UNROLL.
          (void)last;
        } else {
          if (ext % 4 == 0) {
            KOpt opt = { KOP_UNROLL, (u8)ax.axis_id[last], 4 };
            if (kernel_apply_opt(ke, opt)) n_applied++;
          }
        }
      }
    }
  }

  // ---- Section 8: Easy UPCAST fallback (heuristic.py 153-155) -------
  // if not upcasted AND upcastable_dims AND full_shape[last] % 4 == 0:
  //   UPCAST(last, 4)
  {
    if (hand_opt_snapshot_axes(ke, &ax)) {
      if (hand_opt_upcasted(&ax) == 0) {
        u32 dims[MAX_AXES];
        u32 n_dims = hand_opt_upcastable_dims(&ax, dims);
        if (n_dims > 0) {
          u32 last = dims[n_dims - 1];
          if (ax.extent[last] % 4 == 0) {
            KOpt opt = { KOP_UPCAST, (u8)ax.axis_id[last], 4 };
            if (kernel_apply_opt(ke, opt)) n_applied++;
          }
        }
      }
    }
  }

  // ---- Section 9: LOCAL (heuristic.py 159-176) ----------------------
  // local_axis_ranking = [(any(rng not in b.idx.backward_slice for b in bufs),
  //                       axis)
  //                       for axis in axes_of(GLOBAL, LOOP)
  //                       if rngs[axis].src[0].op is Ops.CONST]
  // sort by (-x[0], -x[1]).   For each: pick local_sz from
  //   ([32] if axis == 0) + [16, 8, 4, 3, 2]
  // such that full_shape[axis] % local_sz == 0 AND local_size * local_sz <= 128.
  // Take top 3 (sorted by axis ascending), apply each LOCAL.
  //
  // Section 11 NOLOCALS gate runs first: NOLOCALS=1 -> KOP_NOLOCALS instead.
  {
    int NOLOCALS = hand_opt_getenv_int("NOLOCALS", 0);
    if (NOLOCALS) {
      KOpt opt = { KOP_NOLOCALS, 0, 0 };
      if (kernel_apply_opt(ke, opt)) n_applied++;
    } else {
      if (hand_opt_snapshot_axes(ke, &ax)) {
        // Build (expand_flag, axis_idx) pairs for GLOBAL/LOOP axes.
        // expand_flag = 1 iff any INDEX_E.addr does NOT reference axis
        // (the axis is "expand-like" for that buf).
        Term addrs[64];
        u32 n_bufs = uop_dag_collect_index_e_addrs(ke->cached_lift.store_root,
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
          // tinygrad gates on rngs[axis].src[0].op is CONST -- thvm
          // RANGE extents are all CONST today, so this is always true.
          int expand = 0;
          for (u32 j = 0; j < n_decoded; j++) {
            if (uop_dag_addr_coeff_lookup(&decoded[j], i) == 0) { expand = 1; break; }
          }
          cands[n_cands].axis   = i;
          cands[n_cands].expand = expand;
          n_cands++;
        }
        // Sort by (-expand, -axis): expand=1 first, then highest axis first.
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

        // Walk in ranked order, pick the largest local_sz for each axis.
        typedef struct { u32 axis; u32 sz; } LocalPick;
        LocalPick picks[MAX_AXES];
        u32 n_picks = 0;
        u32 local_running = 1;
        for (u32 i = 0; i < n_cands; i++) {
          u32 axis = cands[i].axis;
          u32 ext  = ax.extent[axis];
          // Candidate factors: [32] if axis==0 else [], then [16,8,4,3,2].
          u32 facs[6]; u32 nf = 0;
          if (axis == 0) facs[nf++] = 32;
          facs[nf++] = 16; facs[nf++] = 8; facs[nf++] = 4;
          facs[nf++] = 3;  facs[nf++] = 2;
          u32 picked = 0;
          for (u32 fi = 0; fi < nf; fi++) {
            u32 f = facs[fi];
            if (ext % f != 0) continue;
            if ((u64)local_running * (u64)f > 128) continue;
            picked = f; break;
          }
          if (picked > 0) {
            picks[n_picks].axis = axis;
            picks[n_picks].sz   = picked;
            n_picks++;
            local_running *= picked;
          }
        }
        // Truncate to first 3 picks (tinygrad: to_local[:3]).
        if (n_picks > 3) n_picks = 3;
        // Sort the chosen picks by axis ascending.
        for (u32 i = 1; i < n_picks; i++) {
          LocalPick k = picks[i]; i32 j = (i32)i - 1;
          while (j >= 0 && picks[j].axis > k.axis) {
            picks[j + 1] = picks[j]; j--;
          }
          picks[j + 1] = k;
        }
        // Apply.  Thvm's uop_dag_apply_split keeps the OUTER range at the
        // pre-split axis_id and inserts the inner range at axis_id+1
        // (shifting every subsequent axis up by one).  Tinygrad's
        // `deleted_shape` accounting decrements the axis index for
        // subsequent fully-consumed splits because tinygrad collapses
        // extent-1 axes; thvm keeps them.  Since `picks` is sorted
        // ascending by original axis_id, the current axis_id for pick i
        // equals its original axis_id plus the count of prior successful
        // splits (each inserted one new axis at position < current).
        u32 shift = 0;
        for (u32 i = 0; i < n_picks; i++) {
          u32 current_axis = picks[i].axis + shift;
          u32 sz = picks[i].sz;
          HandOptAxes axc;
          if (!hand_opt_snapshot_axes(ke, &axc)) break;
          if (current_axis >= axc.n) continue;
          KOpt opt = { KOP_LOCAL, (u8)axc.axis_id[current_axis], sz };
          if (kernel_apply_opt(ke, opt)) {
            n_applied++;
            shift++;       // one more axis sitting above subsequent picks
          }
        }
      }
    }
  }

  // ---- Section 10: THREAD -- SKIPPED (no KOP_THREAD in thvm) --------

  return n_applied;
}

// Should this kernel get the hand-coded heuristic on its next fire?
// Mirrors kernel_autotune's cheap pre-check: the env opt-in is on AND
// the per-shape autotuned flag is still 0.
fn int kernel_should_hand_code_opts(struct KernelEntry const *ke) {
  if (!hand_coded_opts_enabled()) return 0;
  if (ke == NULL) return 0;
  if (ke->schedule != NULL && ke->schedule->autotuned) return 0;
  return 1;
}
