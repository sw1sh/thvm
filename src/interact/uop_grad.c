// interact/uop_grad.c - chain-rule rewrite for UOP_GRAD.
//
// LAZY one-layer rewriting: each interact_grad fire does a single
// structural step on y's outermost UOp, emitting fresh UOP_GRAD
// nodes for any unresolved sub-positions.  Those nested GRADs only
// fire when wnf later enters them (typically because a downstream
// consumer is forcing the head).  Matches the IC discipline where
// every interaction is one rewrite.
//
// VJP semantics: GRAD[y, gy, target] = ∂y/∂target · gy.
//
// Layer rewrites (each is exactly one step):
//   y === target                  -> gy lifted to target.shape
//   y is TAG_TEN  (other)         -> EXPAND(CONST(0), target)
//   y is TAG_NUM                  -> EXPAND(CONST(0), target)
//   UOP_CONST                     -> EXPAND(CONST(0), target)
//   UOP_ADD[a, b]                 -> ADD[ GRAD(a, gy, target),
//                                          GRAD(b, gy, target) ]
//   UOP_MUL[a, b]                 -> ADD[ GRAD(a, MUL[b, gy_lift_a], target),
//                                          GRAD(b, MUL[a, gy_lift_b], target) ]
//   UOP_NEG[a]                    -> GRAD(a, NEG[gy_lift], target)
//   UOP_REDUCE_SUM[a, axis]       -> GRAD(a, gy_lift, target)
//   anything else (LAM/APP/...)   -> the GRAD is left alone (returned
//                                    unchanged) so wnf can force y
//                                    further on the next pass.
//
// At entry we wnf y once so the outermost layer is exposed; sub-
// terms stay deferred (no eager descent into the entire graph).
// Multiple GRAD fires unfold the rule layer by layer until the
// graph is fully concrete.

// EXPAND a scalar / shape-{1} / lower-rank producer to target's
// shape.  Always passes target's full ndim + dims to uop_expand --
// safe because UOP_EXPAND now stores ndim explicitly in the heap
// (see src/uop/expand.c), so the materializer no longer infers
// ndim from the source's rank and the rank-up case (e.g. lifting
// a scalar gy to a {2,3} target during backprop) works correctly.
// Returns `src` unchanged if target isn't a TAG_TEN (no shape to
// look up) or if its descriptor has rank 0.
fn Term expand_to_target(Term src, Term target) {
  if (term_tag(target) != TAG_TEN) return src;
  u32 tid = term_val(target);
  TenDesc *desc = &TENS[tid];
  if (desc->view.shape.ndim == 0) return src;
  return uop_expand(src, desc->view.shape.ndim, desc->view.shape.dims);
}

fn Term grad_zero(Term target) {
  return expand_to_target(uop_const(DT_F32, 0), target);
}

fn Term interact_grad(Term grad_term) {
  u64  loc    = term_val(grad_term);
  Term y      = heap_read(loc + 0);
  Term gy     = heap_read(loc + 1);
  Term target = heap_read(loc + 2);

  // Lazy outermost-layer resolution -- follows VAR-SUB chains and
  // ALO unfoldings but does NOT fire materialize / kernel / grad.
  // Anything we can't structurally pattern-match below leaves the
  // GRAD unchanged so wnf surfaces it as WHNF for a later re-entry.
  y      = term_resolve(y);
  target = term_resolve(target);

  // Leaf: this y is exactly the target tensor.  Lift gy to
  // target.shape so the gradient is target-shaped end-to-end.
  if (y == target) return expand_to_target(gy, target);

  u8 y_tag = term_tag(y);
  if (y_tag == TAG_TEN || y_tag == TAG_NUM) return grad_zero(target);

  // Not a UOP -- can't pattern-match further.  Leave the GRAD term
  // unchanged so wnf treats it as WHNF and the consumer can re-fire
  // once y becomes a UOP.
  if (y_tag != TAG_UOP) return grad_term;

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {
    case UOP_KERNEL: {
      // Pass GRAD through a kernelised subtree by walking its
      // *original* UOP term -- the walker mutates parent cells
      // to UOP_KERNEL but leaves the source UOP intact in the
      // heap (orphaned).  KernelEntry.source_uop holds that root.
      // The chain rule then proceeds on the original UOp graph;
      // child cells that themselves got rewritten to kernels are
      // recursed into via this same case.
      u32  kid = (u32)term_val(heap_read(y_loc + 1));
      if (kid == 0 || kid >= KERNELS_NEXT) return grad_zero(target);
      Term src = KERNELS[kid].source_uop;
      if (src == 0) return grad_zero(target);
      return uop_grad(src, gy, target);
    }

    case UOP_CONST:
      return grad_zero(target);

    case UOP_CONV2D: {
      // CONV2D forward: output[c_out, y, x]
      //   = sum_{c_in, ky, kx} input[c_in, y+ky, x+kx]
      //                         * weights[c_out, c_in, ky, kx]
      //     + bias[c_out]
      // bias is broadcast across every output spatial position, so:
      //   grad_bias[c_out] = sum_{y, x} gy[c_out, y, x]
      // i.e. REDUCE_SUM gy over the spatial axes (1, 2 in the
      // {C_out, H_out, W_out} output).  We must first EXPAND gy to
      // the forward output shape -- otherwise a scalar gy (the
      // typical TGrad seed) reduces to itself instead of accruing
      // the spatial extent.
      //
      // grad_input and grad_weights need transposed-conv +
      // cross-correlation respectively (and UOP_FLIP / UOP_PAD
      // kernels) -- those land as separate sub-items.  For now
      // they emit grad_zero, which is correct when target is
      // bias and a controlled WRONG when target is input or
      // weights (LeNet won't accidentally take grads wrt those
      // through CONV2D yet).
      Term input    = heap_read(y_loc + 0);
      Term weights  = heap_read(y_loc + 1);
      Term bias     = heap_read(y_loc + 2);

      Shape in_shape = {0}, wt_shape = {0};
      u8 shapes_known = (term_shape_in(input,   0, &in_shape) &&
                         in_shape.ndim == 3 &&
                         term_shape_in(weights, 0, &wt_shape) &&
                         wt_shape.ndim == 4);
      Term gb;
      if (shapes_known) {
        // Forward output shape {C_out, H_out, W_out}.
        u32 out_dims[3];
        out_dims[0] = wt_shape.dims[0];                   // C_out
        out_dims[1] = in_shape.dims[1] - wt_shape.dims[2] + 1;  // H_out
        out_dims[2] = in_shape.dims[2] - wt_shape.dims[3] + 1;  // W_out
        Term gy_at_out = uop_expand(gy, 3, out_dims);
        Term gy_axis2  = uop_reduce(REDUCE_SUM, 2, gy_at_out);
        Term gy_axis1  = uop_reduce(REDUCE_SUM, 1, gy_axis2);
        gb = uop_grad(bias, gy_axis1, target);
      } else {
        // Shape lookup failed (rare at this point); fall back to
        // letting the REDUCE_SUM rule lift gy via expand_to_target
        // -- correct when target == bias is a leaf TAG_TEN.
        Term gy_axis2 = uop_reduce(REDUCE_SUM, 2, gy);
        Term gy_axis1 = uop_reduce(REDUCE_SUM, 1, gy_axis2);
        gb = uop_grad(bias, gy_axis1, target);
      }

      // grad_weights via the standard cross-correlation identity:
      //   gw[c_out, c_in, ky, kx]
      //     = sum_{y, x} input[c_in, y+ky, x+kx] * gy[c_out, y, x]
      // For C_in == 1 (LeNet's first conv), this is a single fresh
      // CONV2D call with gy reshaped to {C_out, 1, H_out, W_out} as
      // the "weights" and a zero "bias".  CONV2D's forward then
      // computes exactly the cross-correlation we want, with output
      // {C_out, kh, kw}.  Reshape that to {C_out, 1, kh, kw} so the
      // shape matches the original `weights` tensor.
      //
      // For C_in > 1, the runtime can't express this in one CONV2D
      // call (CONV sums over c_in; we'd need per-c_in slicing +
      // stacking, which needs a SHRINK kernel + a CONCAT op the
      // runtime doesn't have yet).  Falls back to grad_zero in that
      // case -- LeNet's first conv backprops correctly; deeper
      // convs need a follow-up.
      // <!-- design-question: extend to C_in > 1 later.  Options:
      //  (a) per-c_in CONV2D + CONCAT (needs a CONCAT primitive),
      //  (b) one big CONV2D over a {C_out * C_in, ...} reshape
      //      (might fold the c_in axis into c_out).
      //  (c) a dedicated UOP_CORRELATE primitive that returns rank-4. -->
      Term gw_chain;
      if (shapes_known && in_shape.dims[0] == 1) {
        u32 c_out = wt_shape.dims[0];
        u32 h_out = in_shape.dims[1] - wt_shape.dims[2] + 1;
        u32 w_out = in_shape.dims[2] - wt_shape.dims[3] + 1;
        u32 kh    = wt_shape.dims[2];
        u32 kw    = wt_shape.dims[3];
        u32 gy_w4[4]  = {c_out, 1, h_out, w_out};
        u32 zb_dim[1] = {c_out};
        u32 gw_4d[4]  = {c_out, 1, kh, kw};
        Term gy_at_out_3 = uop_expand(gy, 3,
            (u32[3]){c_out, h_out, w_out});
        Term gy_as_w     = uop_reshape(gy_at_out_3, 4, gy_w4);
        Term zero_bias   = uop_expand(uop_const(DT_F32, 0), 1, zb_dim);
        Term raw_gw      = uop_conv2d(input, gy_as_w, zero_bias);
        Term gw_4        = uop_reshape(raw_gw, 4, gw_4d);
        gw_chain = uop_grad(weights, gw_4, target);
      } else {
        // Fallback for C_in > 1: emit zero so multi-channel
        // CONV2D weights silently get no gradient (LeNet's
        // second conv).  This is wrong but doesn't crash; the
        // C_in > 1 case is its own follow-up task.
        gw_chain = grad_zero(target);
      }
      // grad_input via the standard transposed-conv identity:
      //   gi = full_conv(PAD(gy, kh-1, kw-1), PERMUTE(FLIP(weights, {2,3}),
      //                                                {1, 0, 2, 3}))
      // Concretely:
      //   gy_padded = PAD(gy_at_out, axes spatial, b=e=kh-1 / kw-1)
      //              shape {C_out, H_in + kh - 1, W_in + kw - 1}
      //   wt_flip   = FLIP(weights, axes={2, 3})  shape {C_out, C_in, kh, kw}
      //   wt_t      = PERMUTE(wt_flip, {1, 0, 2, 3})  shape {C_in, C_out, kh, kw}
      //   raw_gi    = CONV2D(gy_padded, wt_t, zero_bias{C_in})
      //              shape {C_in, H_in, W_in} = input shape ✓
      Term gi_chain;
      if (shapes_known) {
        u32 c_out = wt_shape.dims[0];
        u32 c_in  = wt_shape.dims[1];
        u32 kh    = wt_shape.dims[2];
        u32 kw    = wt_shape.dims[3];
        u32 h_out = in_shape.dims[1] - kh + 1;
        u32 w_out = in_shape.dims[2] - kw + 1;
        u32 gy_at_out_dims[3]    = {c_out, h_out, w_out};
        u32 pad_be[6]            = {0, 0,
                                    kh - 1, kh - 1,
                                    kw - 1, kw - 1};
        u32 perm_swap_co_ci[4]   = {1, 0, 2, 3};
        u32 zb_in_dim[1]         = {c_in};
        Term gy_at_out_for_input = uop_expand(gy, 3, gy_at_out_dims);
        Term gy_padded           = uop_pad(gy_at_out_for_input, 3, pad_be);
        Term wt_flip             = uop_flip(weights, 0xC);   // axes 2 + 3
        Term wt_t                = uop_permute(wt_flip, 4, perm_swap_co_ci);
        Term zero_bias_in        = uop_expand(uop_const(DT_F32, 0), 1, zb_in_dim);
        Term raw_gi              = uop_conv2d(gy_padded, wt_t, zero_bias_in);
        gi_chain = uop_grad(input, raw_gi, target);
      } else {
        gi_chain = grad_zero(target);
      }

      Term sum_iw = uop_binary(UOP_ADD, gi_chain, gw_chain);
      return uop_binary(UOP_ADD, sum_iw, gb);
    }

    case UOP_ADD: {
      // grad(a+b)/dt = grad(a) + grad(b).  Emit per-child GRADs;
      // they fire on demand.
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term ga = uop_grad(a, gy, target);
      Term gb = uop_grad(b, gy, target);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_MUL: {
      // d(a*b)/dt = (da/dt)*b + a*(db/dt).  Allocate a *fresh* gy
      // EXPAND per branch so the diagram has independent lifts (no
      // multi-reference of one shared EXPAND).  Then defer the
      // per-child GRADs.
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term gy_a_lift = expand_to_target(gy, target);
      Term gy_b_lift = expand_to_target(gy, target);
      Term gy_a = uop_binary(UOP_MUL, b, gy_a_lift);
      Term gy_b = uop_binary(UOP_MUL, a, gy_b_lift);
      Term ga = uop_grad(a, gy_a, target);
      Term gb = uop_grad(b, gy_b, target);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_NEG: {
      Term a      = heap_read(y_loc + 0);
      Term lifted = expand_to_target(gy, target);
      Term n_gy   = uop_unary(UOP_NEG, lifted);
      return uop_grad(a, n_gy, target);
    }

    case UOP_LOG2: {
      // d(log2 a)/dx = 1/(a * ln 2) * da/dx, so:
      //   GRAD[LOG2(a), gy, t] = GRAD[a, gy * RECIP(a) * CONST(1/ln 2), t]
      Term a       = heap_read(y_loc + 0);
      f32 inv_ln2  = 1.4426950408889634f;   // 1 / ln(2)
      u32 inv_bits;
      memcpy(&inv_bits, &inv_ln2, sizeof inv_bits);
      Term k       = uop_const(DT_F32, inv_bits);
      Term ra      = uop_unary(UOP_RECIP, a);
      Term ra_k    = uop_binary(UOP_MUL, ra, k);
      Term lifted  = expand_to_target(gy, target);
      Term gy_a    = uop_binary(UOP_MUL, lifted, ra_k);
      return uop_grad(a, gy_a, target);
    }

    case UOP_EXP2: {
      // d(2^a)/dx = 2^a * ln(2) * da/dx, so:
      //   GRAD[EXP2(a), gy, t] = GRAD[a, gy * EXP2(a) * CONST(ln 2), t]
      Term a       = heap_read(y_loc + 0);
      f32 ln2_f    = 0.6931471805599453f;
      u32 ln2_bits;
      memcpy(&ln2_bits, &ln2_f, sizeof ln2_bits);
      Term ln2     = uop_const(DT_F32, ln2_bits);
      Term ea      = uop_unary(UOP_EXP2, a);
      Term ea_ln2  = uop_binary(UOP_MUL, ea, ln2);
      Term lifted  = expand_to_target(gy, target);
      Term gy_a    = uop_binary(UOP_MUL, lifted, ea_ln2);
      return uop_grad(a, gy_a, target);
    }

    case UOP_RECIP: {
      // d(1/a)/dx = -1/a^2 * da/dx, so:
      //   GRAD[RECIP(a), gy, t] = GRAD[a, gy * -(1/a)^2, t]
      // Allocate independent RECIP nodes so the diagram has no
      // shared references.
      Term a      = heap_read(y_loc + 0);
      Term ra1    = uop_unary(UOP_RECIP, a);
      Term ra2    = uop_unary(UOP_RECIP, a);
      Term sq     = uop_binary(UOP_MUL, ra1, ra2);
      Term nsq    = uop_unary(UOP_NEG, sq);
      Term lifted = expand_to_target(gy, target);
      Term gy_a   = uop_binary(UOP_MUL, lifted, nsq);
      return uop_grad(a, gy_a, target);
    }

    case UOP_REDUCE: {
      // SUM: gradient broadcasts the cotangent back to the input shape.
      // MAX: gradient is gy at the argmax position, 0 elsewhere -- a
      //      one-hot mask built via CMPEQ(a, EXPAND(REDUCE_MAX(a))).
      Term a    = heap_read(y_loc + 0);
      u32  kind = (u32)term_val(heap_read(y_loc + 1));
      u32  axis = (u32)term_val(heap_read(y_loc + 2));

      if (kind == REDUCE_SUM) {
        // Lift gy to a's shape (the REDUCE's INPUT shape, not the
        // grad's target shape -- the two coincide for single-leaf
        // chains but diverge in multi-stage reductions).
        // Idiom: EXPAND gy to the REDUCE's natural output shape
        // (a_shape with axis dropped), then RESHAPE keep-dim
        // (size-1 at axis), then EXPAND to a_shape.  The first
        // EXPAND handles the scalar-gy case (numel 1 -> numel
        // out_shape) so the subsequent RESHAPE doesn't truncate.
        Shape a_shape;
        if (term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0) {
          u32 out_shape[MAX_DIM] = {0};
          u32 keep_shape[MAX_DIM] = {0};
          u32 out_ndim = 0;
          for (u32 i = 0; i < a_shape.ndim; i++) {
            keep_shape[i] = (i == axis) ? 1u : a_shape.dims[i];
            if (i != axis) out_shape[out_ndim++] = a_shape.dims[i];
          }
          if (out_ndim == 0) { out_shape[0] = 1; out_ndim = 1; }
          Term gy_at_out = uop_expand(gy, out_ndim, out_shape);
          Term gy_keep   = uop_reshape(gy_at_out, a_shape.ndim, keep_shape);
          Term lifted    = uop_expand(gy_keep, a_shape.ndim, a_shape.dims);
          return uop_grad(a, lifted, target);
        }
        Term lifted = expand_to_target(gy, target);
        return uop_grad(a, lifted, target);
      }

      // REDUCE_MAX: need a's shape to lift gy and to expand the
      // recomputed max back to a's shape for the elementwise CMPEQ.
      Shape a_shape;
      if (!term_shape_in(a, 0, &a_shape)) {
        // Fallback: target-shape lift (works when target == a in the
        // single-leaf case; less safe in multi-tensor chains, but
        // preserves the SUM-rule's behaviour as a degraded default).
        Term lifted = expand_to_target(gy, target);
        return uop_grad(a, lifted, target);
      }

      // mx naturally has a's shape with `axis` dropped; gy has the
      // same shape if it propagated through correctly, but might be
      // a scalar if it came in directly from TGrad's seed.  Same
      // EXPAND-then-RESHAPE-then-EXPAND idiom as the SUM branch
      // above so the scalar-gy case doesn't get truncated by the
      // numel-mismatched RESHAPE.
      u32 out_shape[MAX_DIM] = {0};
      u32 keep_dim_shape[MAX_DIM] = {0};
      u32 out_ndim = 0;
      for (u32 i = 0; i < a_shape.ndim; i++) {
        keep_dim_shape[i] = (i == axis) ? 1u : a_shape.dims[i];
        if (i != axis) out_shape[out_ndim++] = a_shape.dims[i];
      }
      if (out_ndim == 0) { out_shape[0] = 1; out_ndim = 1; }

      Term mx        = uop_reduce(REDUCE_MAX, axis, a);
      Term mx_keep   = uop_reshape(mx, a_shape.ndim, keep_dim_shape);
      Term mx_lifted = uop_expand(mx_keep, a_shape.ndim, a_shape.dims);

      Term gy_at_out = uop_expand(gy, out_ndim, out_shape);
      Term gy_keep   = uop_reshape(gy_at_out, a_shape.ndim, keep_dim_shape);
      Term gy_lifted = uop_expand(gy_keep, a_shape.ndim, a_shape.dims);

      Term mask      = uop_binary(UOP_CMPEQ, a, mx_lifted);
      Term cotangent = uop_binary(UOP_MUL, gy_lifted, mask);
      return uop_grad(a, cotangent, target);
    }

    case UOP_CMPLT:
      // Comparison emits a 0/1 mask; non-differentiable wrt either
      // input.  The ReLU pattern MUL[x, CMPLT(0, x)] still backprops
      // correctly because the surrounding MUL rule yields
      //   ADD[ GRAD(x, mask*gy, target),
      //        GRAD(CMPLT(...), x*gy, target) ]
      // and this case zeroes the second branch.  d(ReLU)/dx = mask.
      return grad_zero(target);

    case UOP_EXPAND: {
      // Gradient broadcasts the cotangent back along expanded axes:
      //   GRAD[EXPAND(a, new), gy, t] = GRAD[a, sum_{expanded} gy, t]
      // where "expanded" axes are those where src.dim == 1 < new.dim.
      // CONST/NUM source short-circuits to zero -- constants have no
      // gradient.
      Term a = term_resolve(heap_read(y_loc + 0));
      if (term_tag(a) == TAG_NUM) return grad_zero(target);
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) return grad_zero(target);

      Shape src_shape;
      if (!term_shape_in(a, 0, &src_shape) || src_shape.ndim == 0) {
        // Source shape unavailable or rank-0 scalar.  We can't enumerate
        // which axes were expanded statically, so passthrough -- the
        // leaf's expand_to_target reconciles shape via numel-cycling at
        // materialize time.
        return uop_grad(a, gy, target);
      }

      // EXPAND heap layout: [src, NUM(ndim), NUM(d0), ..., NUM(d_{ndim-1})];
      // dim cells live at y_loc+2..y_loc+1+ndim.
      Term g = gy;
      for (i32 axis = (i32)src_shape.ndim - 1; axis >= 0; axis--) {
        u32 out_dim = (u32)term_val(heap_read(y_loc + 2 + axis));
        if (src_shape.dims[axis] == 1 && out_dim > 1) {
          g = uop_reduce(REDUCE_SUM, (u32)axis, g);
        }
      }
      return uop_grad(a, g, target);
    }

    case UOP_RESHAPE: {
      // Reshape is identity-on-data (memcpy in the CPU/Metal kernel)
      // and view-only at the descriptor level, so the gradient is
      // structurally a passthrough: cotangent flows into the source
      // unchanged.  Numel is preserved by definition, so any later
      // expand_to_target on this gy hits the in_numel == out_numel
      // memcpy branch of cpu_op_expand -- shape is reconciled at the
      // leaf without an explicit cotangent reshape here.
      Term a = heap_read(y_loc + 0);
      return uop_grad(a, gy, target);
    }

    default:
      fprintf(stderr, "interact_grad: unhandled UOp opcode %u\n", y_op);
      return grad_zero(target);
  }
}
