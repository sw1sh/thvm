// Coverage counters for kernel_lift_to_uop.  Read via
// kernel_lift_attempts() / kernel_lift_successes().  Counters are
// global (single-threaded scheduling).  Reset by thvm_init / thvm_free.
static u64 KERNEL_LIFT_ATTEMPTS;
static u64 KERNEL_LIFT_SUCCESSES;

fn u64 kernel_lift_attempts(void)        { return KERNEL_LIFT_ATTEMPTS; }
fn u64 kernel_lift_successes(void)       { return KERNEL_LIFT_SUCCESSES; }

fn void kernel_lift_counters_reset(void) {
  KERNEL_LIFT_ATTEMPTS = 0;
  KERNEL_LIFT_SUCCESSES = 0;
}

// Increment helpers for callers that precede this TU in the unity
// build (codegen/render_metal.c is #include'd first and can't reach
// the static globals directly).
fn void kernel_lift_count_attempt (void) { KERNEL_LIFT_ATTEMPTS++; }
fn void kernel_lift_count_success (void) { KERNEL_LIFT_SUCCESSES++; }

// schedule/kernel_lift.c - hand a scheduled kernel back to the
// renderer as a UOp DAG root suitable for cg_render_uop_kernel.
//
// kernel_lift_to_uop dispatches on kernel shape:
//   * multi-output kernels (n_extra_outputs > 0)  -> kernel_lift_from_kprog
//   * empty scalar arena (conv2d-direct path)     -> kernel_lift_from_conv2d
//   * everything else                             -> consume the
//       UOP_STORE root produced by the unified rangeify pass
//       (rangeify_unified_store_root_at).  The previous in-tree
//       ScalarUop walker is gone; any kernel that lacks a unified
//       store_root is reported as an unsupported shape.
//
// KernelUopLift is declared in thvm.h.

// Reject diagnostic: when env-gated, print the first ScalarUop the
// lifter doesn't handle.
// Diagnostic: when env-gated, log a lift decline.  The original
// scalar-arena inspection went with the legacy walker (commit
// 8f5e9420); kernels reaching this dispatcher either succeed via
// the unified short-circuit or via kernel_lift_from_conv2d /
// kernel_lift_from_kprog.
static void lift_reject_log(KernelEntry const *ke, u32 sid,
                            const char *where) {
  static int reject_log_inited = 0;
  static int reject_log_on     = 0;
  if (!reject_log_inited) {
    char const *e = getenv("THVM_DUMP_LIFT_REJECT");
    reject_log_on    = (e != NULL && e[0] == '1');
    reject_log_inited = 1;
  }
  if (!reject_log_on) return;
  (void)ke;
  fprintf(stderr, "lift reject: %s sid=%u\n", where, sid);
}

// Build a UOP_BUFFER for kernel input slot `slot` from the
// TenDesc shape.  Falls back to a 1D dummy when the slot has no
// TenDesc (test fixtures or paths where TenDesc isn't wired).
//
// The `instance` field on UOP_BUFFER ensures distinct slots get
// distinct Terms even when shape collides; we use slot+1 (since
// instance=0 is the output's reserved value).
static Term lift_input_buffer(KernelEntry const *ke, u32 slot) {
  if (slot >= ke->n_inputs) return 0;
  u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
  u32 tid = (ke->input_tids != NULL) ? ke->input_tids[slot] : 0;
  u32 inst = slot + 1;
  if (tid != 0 && tid < TENS_NEXT) {
    TenDesc const *td = &TENS[tid];
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                           td->view.shape.ndim, td->view.shape.dims, inst);
  }
  u32 dummy[1] = { 1 };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dummy, inst);
}

// Synthesize the conv2d_flat UOp DAG from a TileConv2DInfo.  Mirrors
// what rmt_emit_conv2d_flat produces but as UOp DAG so the renderer
// rewrite path can take over.  Layout:
//
//   for r_out in [0, c_out * patches):
//     co     = r_out / patches
//     patch  = r_out % patches
//     bi     = patch / spatial_patches
//     sp     = patch % spatial_patches
//     ow     = sp % w_out
//     oh     = sp / w_out
//     acc    = 0
//     for q in [0, KRED) where KRED = c_in * kh * kw:
//       kw_v = q % kw
//       qk   = q / kw
//       kh_v = qk % kh
//       ci   = qk / kh
//       wi   = w_offset + co * w_stride0 + q * w_stride1
//       xi   = x_offset + bi * x_stride_b + ci * x_stride2
//                       + (oh + kh_v) * x_stride0
//                       + (ow + kw_v) * x_stride1
//       acc += W[wi] * X[xi]
//     out[r_out] = acc
//
// Multi-input "patch_input_count" cases (im2col split into multiple
// input buffers via a switch on q) are handled below with a nested
// UOP_IWHERE chain selecting which input slot to read based on q.
static int kernel_lift_from_conv2d(KernelEntry const *ke,
                                   KernelUopLift *out) {
  TileConv2DInfo conv;
  if (!tile_analyze_conv2d_flat(ke, &conv)) {
    lift_reject_log(ke, 0, "conv2d/tile-analyze-fail");
    return 0;
  }
  if (conv.batch == 0 || conv.h_out == 0 || conv.w_out == 0) {
    lift_reject_log(ke, 0, "conv2d/zero-batch-or-out");
    return 0;
  }
  if (conv.spatial_patches == 0) {
    lift_reject_log(ke, 0, "conv2d/zero-spatial-patches");
    return 0;
  }
  if (conv.w_input >= ke->n_inputs) {
    lift_reject_log(ke, 0, "conv2d/w-input-oor");
    return 0;
  }
  if (conv.patch_input_count == 0 && conv.x_input >= ke->n_inputs) {
    lift_reject_log(ke, 0, "conv2d/x-input-oor");
    return 0;
  }
  if (conv.patch_input_count != 0
      && (u64)conv.patch_input_base + conv.patch_input_count > ke->n_inputs) {
    lift_reject_log(ke, 0, "conv2d/patch-input-range-oor");
    return 0;
  }
  if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) {
    lift_reject_log(ke, 0, "conv2d/n-inputs-over-cap");
    return 0;
  }

  // Buffers.  Conv2d output is [c_out, patches].
  u32 dims_w[2] = { conv.c_out, conv.c_in * conv.kh * conv.kw };
  u32 dims_x[1] = { 1024 };  // shape unused; lifter walks affine addr
  u32 dims_o[1] = { conv.c_out * conv.patches };
  u32 w_dt = (ke->input_dtypes != NULL) ? ke->input_dtypes[conv.w_input] : DT_FP32;
  u32 x_dt = (conv.x_input < ke->n_inputs && ke->input_dtypes != NULL)
             ? ke->input_dtypes[conv.x_input] : DT_FP32;
  Term W = uop_buffer_inst(UOP_SCOPE_GLOBAL, w_dt, 2, dims_w, conv.w_input + 1);
  Term X = (conv.patch_input_count == 0)
           ? uop_buffer_inst(UOP_SCOPE_GLOBAL, x_dt, 1, dims_x, conv.x_input + 1)
           : 0;  // multi-input path doesn't use a single X buffer
  Term C = uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, 1, dims_o, 0);

  u32 patches = conv.patches;
  u32 KRED    = conv.c_in * conv.kh * conv.kw;
  if (KRED == 0) {
    lift_reject_log(ke, 0, "conv2d/zero-kred");
    return 0;
  }

  // Range axes.
  Term r_out = uop_range(0, 0 /*LOOP*/,   conv.c_out * patches);
  Term r_q   = uop_range(1, 1 /*REDUCE*/, KRED);

  // Decompose r_out -> (co, patch) and patch -> (bi, oh, ow).
  Term k_patches = uop_const(DT_INT32, patches);
  Term k_spatial = uop_const(DT_INT32, conv.spatial_patches);
  Term k_w_out   = uop_const(DT_INT32, conv.w_out);
  Term co     = uop_int_binary(UOP_IDIV, r_out, k_patches);
  Term patch  = uop_int_binary(UOP_IMOD, r_out, k_patches);
  Term bi     = uop_int_binary(UOP_IDIV, patch, k_spatial);
  Term sp     = uop_int_binary(UOP_IMOD, patch, k_spatial);
  Term ow     = uop_int_binary(UOP_IMOD, sp, k_w_out);
  Term oh     = uop_int_binary(UOP_IDIV, sp, k_w_out);

  // Decompose r_q -> (ci, kh_v, kw_v).
  Term k_kw   = uop_const(DT_INT32, conv.kw);
  Term k_kh   = uop_const(DT_INT32, conv.kh);
  Term kw_v   = uop_int_binary(UOP_IMOD, r_q, k_kw);
  Term qk     = uop_int_binary(UOP_IDIV, r_q, k_kw);
  Term kh_v   = uop_int_binary(UOP_IMOD, qk, k_kh);
  Term ci     = uop_int_binary(UOP_IDIV, qk, k_kh);

  // wi = w_offset + co * w_stride0 + q * w_stride1
  Term k_w_off = uop_const(DT_INT32, (u32)conv.w_offset);
  Term k_ws0   = uop_const(DT_INT32, (u32)conv.w_stride0);
  Term k_ws1   = uop_const(DT_INT32, (u32)conv.w_stride1);
  Term wi_co   = uop_int_binary(UOP_IMUL, co, k_ws0);
  Term wi_q    = uop_int_binary(UOP_IMUL, r_q, k_ws1);
  Term wi_sum  = uop_int_binary(UOP_IADD, wi_co, wi_q);
  Term wi      = uop_int_binary(UOP_IADD, k_w_off, wi_sum);
  Term ldW     = uop_index_e(W, wi);

  Term ldX = 0;
  if (conv.patch_input_count == 0) {
    // Single X buffer: xi = x_offset + bi * x_stride_b + ci * x_stride2
    //                          + (oh + kh_v) * x_stride0
    //                          + (ow + kw_v) * x_stride1
    Term k_x_off = uop_const(DT_INT32, (u32)conv.x_offset);
    Term k_xsb   = uop_const(DT_INT32, (u32)conv.x_stride_b);
    Term k_xs0   = uop_const(DT_INT32, (u32)conv.x_stride0);
    Term k_xs1   = uop_const(DT_INT32, (u32)conv.x_stride1);
    Term k_xs2   = uop_const(DT_INT32, (u32)conv.x_stride2);
    Term xi_b    = uop_int_binary(UOP_IMUL, bi, k_xsb);
    Term xi_ci   = uop_int_binary(UOP_IMUL, ci, k_xs2);
    Term oh_kh   = uop_int_binary(UOP_IADD, oh, kh_v);
    Term xi_h    = uop_int_binary(UOP_IMUL, oh_kh, k_xs0);
    Term ow_kw   = uop_int_binary(UOP_IADD, ow, kw_v);
    Term xi_w    = uop_int_binary(UOP_IMUL, ow_kw, k_xs1);
    Term xi_sum1 = uop_int_binary(UOP_IADD, xi_b, xi_ci);
    Term xi_sum2 = uop_int_binary(UOP_IADD, xi_h, xi_w);
    Term xi_sum  = uop_int_binary(UOP_IADD, xi_sum1, xi_sum2);
    Term xi      = uop_int_binary(UOP_IADD, k_x_off, xi_sum);
    ldX          = uop_index_e(X, xi);
  } else {
    // Multi-input X via switch on q: xv = (q < 1) ? load_pi0 :
    //                                     (q < 2) ? load_pi1 : ...
    // Each pi has its own input slot + per-axis strides; address is
    // `pv->offset + bi * psb + oh * psh + ow * psw` (no ci/kh/kw --
    // those are encoded statically by which pi the q lands in).
    if (ke->input_views == NULL) {
      lift_reject_log(ke, 0, "conv2d/multi-input-no-views");
      return 0;
    }
    // Build bottom-up so the chain is right-leaning.
    ldX = uop_const(x_dt, 0);  // default for q out of range
    for (u32 pi = conv.patch_input_count; pi > 0; pi--) {
      u32 idx = pi - 1;
      u32 slot = conv.patch_input_base + idx;
      View const *pv = &ke->input_views[slot];
      i32 psb = 0, psh = 0, psw = 0;
      if (pv->shape.ndim == 3) { psh = pv->strides[1]; psw = pv->strides[2]; }
      else                     { psb = pv->strides[1]; psh = pv->strides[2];
                                 psw = pv->strides[3]; }
      u32 dt_pi = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
      u32 dims_pi[1] = { 1024 };
      Term Xpi  = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt_pi, 1, dims_pi,
                                  slot + 1);
      Term k_off = uop_const(DT_INT32, (u32)pv->offset);
      Term k_psb = uop_const(DT_INT32, (u32)psb);
      Term k_psh = uop_const(DT_INT32, (u32)psh);
      Term k_psw = uop_const(DT_INT32, (u32)psw);
      Term a_b = uop_int_binary(UOP_IMUL, bi, k_psb);
      Term a_h = uop_int_binary(UOP_IMUL, oh, k_psh);
      Term a_w = uop_int_binary(UOP_IMUL, ow, k_psw);
      Term a_bh = uop_int_binary(UOP_IADD, a_b, a_h);
      Term a_bhw = uop_int_binary(UOP_IADD, a_bh, a_w);
      Term addr_pi = uop_int_binary(UOP_IADD, k_off, a_bhw);
      Term ld_pi   = uop_index_e(Xpi, addr_pi);
      // cond: q < (idx + 1)  (i.e. q matches case `idx` in the
      // bottom-up chain we're building).
      Term k_idxp1 = uop_const(DT_INT32, idx + 1);
      Term cond    = uop_int_binary(UOP_ILT, r_q, k_idxp1);
      ldX = uop_iwhere(cond, ld_pi, ldX);
    }
  }

  // MUL + REDUCE_SUM_q(W * X)
  Term mul   = uop_binary(UOP_MUL, ldW, ldX);
  Term red   = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);

  Term store = uop_store(C, r_out, red);

  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (i == conv.w_input) {
      out->in_bufs[i] = W;
    } else if (conv.patch_input_count == 0 && i == conv.x_input) {
      out->in_bufs[i] = X;
    } else if (conv.patch_input_count != 0
               && i >= conv.patch_input_base
               && i < conv.patch_input_base + conv.patch_input_count) {
      // Re-construct the same Xpi the load chain referenced, so
      // rmu_buf_name's lookup hits the correct entry.
      View const *pv = (ke->input_views != NULL) ? &ke->input_views[i] : NULL;
      u32 dt_pi = (ke->input_dtypes != NULL) ? ke->input_dtypes[i] : DT_FP32;
      u32 dims_pi[1] = { (pv != NULL) ? (pv->numel ? pv->numel : 1024u) : 1024u };
      out->in_bufs[i] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt_pi, 1,
                                        dims_pi, i + 1);
    } else {
      out->in_bufs[i] = lift_input_buffer(ke, i);
    }
  }
  out->out_buf    = C;
  out->store_root = store;
  return 1;
}

// === Multi-output kernel lift (F6 multi-output walker) ====================
//
// Kernels produced by THVM_KERNEL_MERGE=1's splice action carry
// `n_extra_outputs > 0` and a contiguous KProgOp[] composed from
// child-then-host elementwise programs.  Rangeify is skipped for these
// (see materialize.c -- "Multi-output kernels (spliced_ok above) can't
// go through rangeify today"), so `scalar_uops` is NULL and the
// scalar-arena walker can't help.  This direct KProgOp -> UOp DAG path
// transcribes the program op-by-op, emits one UOP_STORE per output
// (primary + each `store_extra_plus_one > 0` op), and chains them with
// UOP_AFTER so the cpu_uop_walk walker dispatches each store to its
// own buffer naturally.
//
// Coverage is limited to the merger's current shape (per
// merge_boundary_is_elementwise): unary + binary elementwise ALU plus
// CONST.  RESHAPE / movement / REDUCE / CAST aren't generated by the
// splice planner today; they bail.
static u8 kprog_op_is_lift_supported(u8 op) {
  if (op == UOP_CONST) return 1;
  if (uop_is_unary_elementwise(op)) return 1;
  if (uop_is_binary_elementwise(op)) return 1;
  return 0;
}

static int kprog_step_value(KernelEntry const *ke, u32 step,
                            Term const *step_terms,
                            Term const *in_buf_terms,
                            Term addr,
                            Term *out_value) {
  KProgOp const *p = &ke->program[step];
  if (p->opcode == UOP_CONST) {
    *out_value = uop_const(p->dtype, p->arg);
    return 1;
  }
  Term src_vals[MAX_UOP_SRC] = {0};
  for (u8 s = 0; s < p->n_src; s++) {
    u32 raw = p->src[s];
    if (KSRC_IS_INPUT(raw)) {
      u32 idx = KSRC_INDEX(raw);
      if (idx >= ke->n_inputs) {
        lift_reject_log(ke, 0, "kprog-step/input-idx-oor");
        return 0;
      }
      src_vals[s] = uop_index_e(in_buf_terms[idx], addr);
    } else {
      u32 idx = KSRC_INDEX(raw);
      if (idx >= step) {
        lift_reject_log(ke, 0, "kprog-step/forward-ref");
        return 0;
      }
      if (step_terms[idx] == 0) {
        lift_reject_log(ke, 0, "kprog-step/earlier-step-bailed");
        return 0;
      }
      src_vals[s] = step_terms[idx];
    }
  }
  if (uop_is_unary_elementwise(p->opcode) && p->n_src == 1) {
    *out_value = uop_unary(p->opcode, src_vals[0]);
    return 1;
  }
  if (uop_is_binary_elementwise(p->opcode) && p->n_src == 2) {
    *out_value = uop_binary(p->opcode, src_vals[0], src_vals[1]);
    return 1;
  }
  lift_reject_log(ke, 0, "kprog-step/unsupported-shape");
  return 0;
}

// Build a UOP_BUFFER for an extra output slot.  Distinct `instance`
// keeps the Term identity-distinct from primary (instance=0) and from
// any input slot (instance=slot+1).  Slot extras start at
// 1 + KERNEL_LIFT_MAX_INPUT (== 65) so the inst space stays disjoint.
#define KERNEL_LIFT_EXTRA_INST_BASE (1u + KERNEL_LIFT_MAX_INPUT)

static Term lift_extra_output_buffer(KernelEntry const *ke, u32 ei) {
  if (ei >= ke->n_extra_outputs) return 0;
  u32 dtype = ke->extra_output_dtypes[ei];
  Shape const *sh = &ke->extra_output_shapes[ei];
  u32 ndim = sh->ndim;
  u32 dims[MAX_DIM] = {0};
  if (ndim == 0 || ndim > MAX_DIM) {
    // Degenerate: synthesise a 1D buffer with extent = numel.  Keeps
    // the lift well-formed even when the splice action stashed only
    // a numel without a full Shape.
    ndim = 1;
    dims[0] = ke->extra_output_numels[ei] ? ke->extra_output_numels[ei] : 1;
  } else {
    for (u32 d = 0; d < ndim; d++) dims[d] = sh->dims[d];
  }
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, ndim, dims,
                         KERNEL_LIFT_EXTRA_INST_BASE + ei);
}

// Lift a multi-output (post-splice) kernel directly from KProgOp[].
// Emits a STORE per output, chained by UOP_AFTER so the walker walks
// each store and routes it to the right buffer pointer via
// uwalk_resolve_buf's term-identity match.
static int kernel_lift_from_kprog(KernelEntry const *ke, KernelUopLift *out) {
  if (ke->n_extra_outputs == 0) return 0;       // single-output uses other path
  if (ke->n_ops == 0 || ke->program == NULL) {
    lift_reject_log(ke, 0, "kprog/empty-program");
    return 0;
  }
  if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) {
    lift_reject_log(ke, 0, "kprog/n-inputs-over-cap");
    return 0;
  }
  // Validate that every op is in the supported elementwise + CONST set
  // and every output (primary + extras) shares numel == output_numel.
  // The merge planner only fuses kernels with identical iter shapes,
  // so the numel uniformity is a structural invariant we can rely on.
  u32 primary_numel = ke->output_numel;
  if (primary_numel == 0) {
    lift_reject_log(ke, 0, "kprog/zero-primary-numel");
    return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp const *p = &ke->program[i];
    if (!kprog_op_is_lift_supported(p->opcode)) {
      lift_reject_log(ke, 0, "kprog/unsupported-opcode");
      return 0;
    }
    if (p->numel != 1 && p->numel != primary_numel) {
      lift_reject_log(ke, 0, "kprog/numel-mismatch");
      return 0;
    }
  }
  for (u32 ei = 0; ei < ke->n_extra_outputs; ei++) {
    if (ke->extra_output_numels[ei] != primary_numel) {
      lift_reject_log(ke, 0, "kprog/extra-output-numel-mismatch");
      return 0;
    }
  }
  // Build the output range (single LOOP axis over the flat numel).
  // Match the splice's flat layout: every elementwise op iterates
  // primary_numel positions in row-major order, and the post-pass
  // memcpy treats both primary and extras as flat numel-sized buffers.
  Term r_addr = uop_range(0, 0 /*LOOP*/, primary_numel);

  // Build input buffer terms.
  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    Term in_buf = lift_input_buffer(ke, i);
    if (in_buf == 0) {
      lift_reject_log(ke, 0, "kprog/in-buf-build-fail");
      return 0;
    }
    out->in_bufs[i] = in_buf;
  }
  // Build primary output buffer.  Shape comes from output_shape (the
  // splice path populated it via term_shape_in); fall back to a flat
  // 1D shape of primary_numel when output_shape is unset.
  u32 out_ndim = ke->output_shape.ndim;
  u32 out_dims[MAX_DIM] = {0};
  if (out_ndim == 0 || out_ndim > MAX_DIM) {
    out_ndim = 1;
    out_dims[0] = primary_numel;
  } else {
    for (u32 d = 0; d < out_ndim; d++) out_dims[d] = ke->output_shape.dims[d];
  }
  Term primary_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype,
                                     out_ndim, out_dims, 0);
  out->out_buf     = primary_buf;
  out->n_outputs   = 1u + ke->n_extra_outputs;
  if (out->n_outputs > KERNEL_LIFT_MAX_OUTPUT) {
    lift_reject_log(ke, 0, "kprog/n-outputs-over-cap");
    return 0;
  }
  out->out_bufs[0] = primary_buf;
  for (u32 ei = 0; ei < ke->n_extra_outputs; ei++) {
    Term eb = lift_extra_output_buffer(ke, ei);
    if (eb == 0) {
      lift_reject_log(ke, 0, "kprog/extra-out-buf-build-fail");
      return 0;
    }
    out->out_bufs[1 + ei] = eb;
  }
  // Walk program; cache each step's lifted UOp value Term.
  Term *step_terms = (Term *)calloc(ke->n_ops, sizeof(Term));
  if (step_terms == NULL) {
    lift_reject_log(ke, 0, "kprog/calloc-fail");
    return 0;
  }
  Term extra_stores[KERNEL_MAX_EXTRA_OUTPUTS] = {0};
  for (u32 step = 0; step < ke->n_ops; step++) {
    Term v = 0;
    if (!kprog_step_value(ke, step, step_terms, out->in_bufs, r_addr, &v)) {
      lift_reject_log(ke, 0, "kprog/step-value-fail");
      free(step_terms);
      return 0;
    }
    step_terms[step] = v;
    KProgOp const *p = &ke->program[step];
    if (p->store_extra_plus_one > 0) {
      u32 ei = (u32)p->store_extra_plus_one - 1u;
      if (ei >= ke->n_extra_outputs) {
        lift_reject_log(ke, 0, "kprog/store-extra-idx-oor");
        free(step_terms);
        return 0;
      }
      extra_stores[ei] = uop_store(out->out_bufs[1 + ei], r_addr, v);
    }
  }
  Term primary_value = step_terms[ke->n_ops - 1];
  if (primary_value == 0) {
    lift_reject_log(ke, 0, "kprog/primary-value-zero");
    free(step_terms);
    return 0;
  }
  Term primary_store = uop_store(primary_buf, r_addr, primary_value);
  free(step_terms);
  // Chain: store_root = STORE(primary) AFTER STORE(extra_0) AFTER ...
  // Walker emits the AFTER chain in inner-first order (uwalk_emit_after
  // recurses into after_node before node), so wrapping primary on the
  // outside means primary writes LAST -- the splice post-pass requires
  // the "last op writes to out_buf_id" ordering.
  Term root = primary_store;
  for (u32 ei = 0; ei < ke->n_extra_outputs; ei++) {
    if (extra_stores[ei] == 0) {
      // splice should have marked exactly one op per extra; if a slot
      // is unmarked the splice metadata is inconsistent -- bail rather
      // than emit a kernel that would silently leave the extra zero.
      lift_reject_log(ke, 0, "kprog/extra-store-unset");
      return 0;
    }
    root = uop_after(root, extra_stores[ei]);
  }
  out->store_root = root;
  return 1;
}

fn int kernel_lift_to_uop(KernelEntry const *ke, KernelUopLift *out) {
  if (ke == NULL || out == NULL) return 0;
  // Multi-output (kernel-merge splice): always go through the direct
  // KProgOp -> UOp DAG path.  Rangeify is skipped for these kernels
  // (materialize doesn't run rangeify_try_lower_elementwise when
  // spliced_ok=1, so scalar_uops stays NULL), and the KProgOp lifter
  // is the canonical path for spliced kernels regardless of arena
  // state.
  if (ke->n_extra_outputs > 0) {
    if (kernel_lift_from_kprog(ke, out)) return 1;
    return 0;
  }
  // Unified-pass short-circuit: when the rangeify_unified pass emitted
  // a UOP_STORE for this kernel's boundary, build only the buffer
  // fields the bypass-substitution site reads (in_bufs[], n_inputs,
  // out_buf) and hand back the unified root as store_root.  Walked
  // BEFORE the conv2d-direct path because the unified pass owns every
  // non-multi-output, non-conv2d kernel today.
  if (ke->source_uop != 0) {
    u32 ru_idx = bufferize_info_find(term_val(ke->source_uop));
    if (ru_idx != 0xFFFFFFFFu) {
      Term ru_root = rangeify_unified_store_root_at(ru_idx);
      if (ru_root != 0) {
        if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) {
          lift_reject_log(ke, 0, "entry/n-inputs-over-cap");
          return 0;
        }
        out->n_inputs = ke->n_inputs;
        for (u32 i = 0; i < ke->n_inputs; i++) {
          Term in_buf = lift_input_buffer(ke, i);
          if (in_buf == 0) {
            lift_reject_log(ke, 0, "entry/in-buf-build-fail");
            return 0;
          }
          out->in_bufs[i] = in_buf;
        }
        out->out_buf    = uop_store_buf(ru_root);
        out->store_root = ru_root;
        return 1;
      }
    }
  }
  // Empty kernel: try the conv2d-shape lifter.  The dedicated conv2d
  // dispatch path bypasses rangeify, so scalar_uops is NULL.
  if (ke->scalar_uops == NULL) {
    if (kernel_lift_from_conv2d(ke, out)) return 1;
    static int reject_log_inited = 0;
    static int reject_log_on     = 0;
    if (!reject_log_inited) {
      char const *e = getenv("THVM_DUMP_LIFT_REJECT");
      reject_log_on    = (e != NULL && e[0] == '1');
      reject_log_inited = 1;
    }
    if (reject_log_on) {
      fprintf(stderr,
              "lift reject: entry/no-scalar-arena n_inputs=%u "
              "n_ops=%u\n",
              ke->n_inputs, ke->n_ops);
      // Probe: source_uop populated on each KProgOp?  If yes, the
      // future kernel_lift_from_kprog can use it as the lift root.
      if (ke->n_ops > 0 && ke->program != NULL) {
        u32 set = 0;
        for (u32 i = 0; i < ke->n_ops; i++) {
          if (ke->program[i].source_uop != 0) set++;
        }
        Term last = ke->program[ke->n_ops - 1].source_uop;
        fprintf(stderr,
                "  source_uop coverage: %u/%u set; last.tag=%u last.ext=%u\n",
                set, ke->n_ops,
                (unsigned)term_tag(last), (unsigned)term_ext(last));
      }
      // Layout dump for the conv2d-flat rejecting case so the
      // operator can see what shape kernel_lift_from_conv2d
      // doesn't yet handle.  Iterate over input_views (if any)
      // to print rank + dims for each, then summarise the
      // KProgOp opcode histogram.
      if (ke->input_views != NULL) {
        for (u32 i = 0; i < ke->n_inputs && i < 32; i++) {
          View const *v = &ke->input_views[i];
          fprintf(stderr, "  in[%u] ndim=%u dims=[", i, v->shape.ndim);
          for (u32 d = 0; d < v->shape.ndim && d < MAX_DIM; d++) {
            fprintf(stderr, "%s%u", d ? "," : "", v->shape.dims[d]);
          }
          fprintf(stderr, "] strides=[");
          for (u32 d = 0; d < v->shape.ndim && d < MAX_DIM; d++) {
            fprintf(stderr, "%s%d", d ? "," : "", v->strides[d]);
          }
          fprintf(stderr, "]\n");
        }
      }
      if (ke->program != NULL && ke->n_ops > 0) {
        u32 op_hist[256] = {0};
        for (u32 i = 0; i < ke->n_ops; i++) {
          u32 op = ke->program[i].opcode;
          if (op < 256) op_hist[op]++;
        }
        fprintf(stderr, "  prog_ops:");
        for (u32 op = 0; op < 256; op++) {
          if (op_hist[op] > 0) {
            fprintf(stderr, " op%u=%u", op, op_hist[op]);
          }
        }
        fputc('\n', stderr);
      }
    }
    return 0;
  }
  // Walker fallback removed: the unified-rangeify pass handles every
  // kernel reaching this entry point.  Anything that misses the
  // short-circuit at the top (no source_uop / no bufferize_info match)
  // is reported as an unsupported shape.
  lift_reject_log(ke, 0, "entry/no-unified-store-root");
  return 0;
}
