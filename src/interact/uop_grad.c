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
  // Heap layout is [y, gy, NUM(n), x_1, ..., x_n].  n>1 fires the
  // chain rule per target by lowering to a TAG_CTR of n unary
  // uop_grad(y, gy, x_i) terms.  Each unary grad walks the chain
  // rule independently, but the forward DAG (y and its descendants)
  // lives at SHARED heap locs so materialize's per-realize memo
  // dedups every kernel emitted from those forward UOps -- the
  // savings show up as one realize + one set of forward kernels
  // covering all n targets.
  Term n_cell = heap_read(loc + 2);
  u32  n      = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 1;
  if (n > 1) {
    Term children[256];   // n_max guarded by the cap below
    if (n > 256) return grad_term;   // bail; very rare
    for (u32 i = 0; i < n; i++) {
      Term x_i = heap_read(loc + 3 + i);
      children[i] = uop_grad(y, gy, x_i);
    }
    return term_new_ctr(0, children, n);
  }
  Term target = heap_read(loc + 3);

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
      // d(a*b)/dt = (da/dt)*b + a*(db/dt).  Cotangent for each
      // branch is gy * (other input); MUL broadcasts via the
      // kernel's numel-cycling, so no explicit lift needed.
      // (Earlier this lifted gy to target shape via
      // expand_to_target -- wrong when target rank doesn't match
      // MUL's output rank, e.g. CONV2D-bias-grad downstream of a
      // softmax+CE chain.)
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term gy_a = uop_binary(UOP_MUL, b, gy);
      Term gy_b = uop_binary(UOP_MUL, a, gy);
      Term ga = uop_grad(a, gy_a, target);
      Term gb = uop_grad(b, gy_b, target);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_NEG: {
      // NEG output shape == input shape, so gy already has the
      // right shape; no lift needed.  (Earlier this called
      // expand_to_target(gy, target) which mis-shapes gy when the
      // grad's target is at a different rank than the current
      // y -- e.g. NEG sitting downstream of a REDUCE_SUM whose
      // input is a multi-element tensor.)
      Term a    = heap_read(y_loc + 0);
      Term n_gy = uop_unary(UOP_NEG, gy);
      return uop_grad(a, n_gy, target);
    }

    case UOP_LOG2: {
      // d(log2 a)/dx = 1/(a * ln 2) * da/dx, so:
      //   GRAD[LOG2(a), gy, t] = GRAD[a, gy * RECIP(a) * CONST(1/ln 2), t]
      // gy already has LOG2's output shape (== input a's shape);
      // no lift needed.
      Term a       = heap_read(y_loc + 0);
      f32 inv_ln2  = 1.4426950408889634f;   // 1 / ln(2)
      u32 inv_bits;
      memcpy(&inv_bits, &inv_ln2, sizeof inv_bits);
      Term k       = uop_const(DT_F32, inv_bits);
      Term ra      = uop_unary(UOP_RECIP, a);
      Term ra_k    = uop_binary(UOP_MUL, ra, k);
      Term gy_a    = uop_binary(UOP_MUL, gy, ra_k);
      return uop_grad(a, gy_a, target);
    }

    case UOP_EXP2: {
      // d(2^a)/dx = 2^a * ln(2) * da/dx, so:
      //   GRAD[EXP2(a), gy, t] = GRAD[a, gy * EXP2(a) * CONST(ln 2), t]
      // gy already has EXP2's output shape (== input a's shape).
      Term a       = heap_read(y_loc + 0);
      f32 ln2_f    = 0.6931471805599453f;
      u32 ln2_bits;
      memcpy(&ln2_bits, &ln2_f, sizeof ln2_bits);
      Term ln2     = uop_const(DT_F32, ln2_bits);
      Term ea      = uop_unary(UOP_EXP2, a);
      Term ea_ln2  = uop_binary(UOP_MUL, ea, ln2);
      Term gy_a    = uop_binary(UOP_MUL, gy, ea_ln2);
      return uop_grad(a, gy_a, target);
    }

    case UOP_RECIP: {
      // d(1/a)/dx = -1/a^2 * da/dx, so:
      //   GRAD[RECIP(a), gy, t] = GRAD[a, gy * -(1/a)^2, t]
      // Allocate independent RECIP nodes so the diagram has no
      // shared references.  gy has RECIP's output shape == input
      // a's shape.
      Term a      = heap_read(y_loc + 0);
      Term ra1    = uop_unary(UOP_RECIP, a);
      Term ra2    = uop_unary(UOP_RECIP, a);
      Term sq     = uop_binary(UOP_MUL, ra1, ra2);
      Term nsq    = uop_unary(UOP_NEG, sq);
      Term gy_a   = uop_binary(UOP_MUL, gy, nsq);
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
      // Lift gy to EXPAND's OUTPUT shape first so the REDUCE_SUMs
      // below have a well-defined per-axis source extent.  Without
      // the lift, a scalar gy from the seed (or any same-numel-but-
      // wrong-rank cotangent) lets REDUCE_SUM degrade to a no-op
      // and the cotangent leaks out unsummed -- the bias-grad bug
      // from the conv2d-lowered switchover.
      u32 out_ndim = (u32)term_val(heap_read(y_loc + 1));
      u32 out_dims[MAX_DIM];
      for (u32 i = 0; i < out_ndim; i++) {
        out_dims[i] = (u32)term_val(heap_read(y_loc + 2 + i));
      }
      Term g = uop_expand(gy, out_ndim, out_dims);
      for (i32 axis = (i32)src_shape.ndim - 1; axis >= 0; axis--) {
        if (src_shape.dims[axis] == 1 && out_dims[axis] > 1) {
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

    case UOP_SHRINK: {
      // SHRINK extracts the slice [b_i, e_i) on each axis.  The
      // gradient embeds the cotangent back into the source extent
      // by zero-padding outside the kept slice:
      //   GRAD[SHRINK(a, [b,e]), gy, t] =
      //     GRAD[a, PAD(EXPAND(gy, out_shape), [b_i, src_dim_i - e_i]), t]
      // Heap layout is [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...].
      // gy must first be lifted to SHRINK's output shape -- otherwise
      // a scalar gy from the seed would let PAD implicitly rank-up to
      // {1} and produce widths against the wrong source extent.
      Term a = term_resolve(heap_read(y_loc + 0));
      if (term_tag(a) == TAG_NUM) return grad_zero(target);
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) return grad_zero(target);

      Shape src_shape;
      if (!term_shape_in(a, 0, &src_shape) || src_shape.ndim == 0) {
        return uop_grad(a, gy, target);
      }

      u32 out_dims[MAX_DIM];
      u32 widths[2 * MAX_DIM];
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 b = (u32)term_val(heap_read(y_loc + 1 + 2 * i));
        u32 e = (u32)term_val(heap_read(y_loc + 2 + 2 * i));
        out_dims[i] = (e > b) ? (e - b) : 0;
        widths[2 * i + 0] = b;
        widths[2 * i + 1] = (src_shape.dims[i] > e) ? (src_shape.dims[i] - e) : 0;
      }
      Term gy_shaped = uop_expand(gy, src_shape.ndim, out_dims);
      Term g         = uop_pad(gy_shaped, src_shape.ndim, widths);
      return uop_grad(a, g, target);
    }

    case UOP_FLIP: {
      // FLIP mirrors selected axes (axes_bitmask).  FLIP is its own
      // inverse, so the gradient flips the cotangent on the same
      // axes:
      //   GRAD[FLIP(a, mask), gy, t] =
      //     GRAD[a, FLIP(EXPAND(gy, src_shape), mask), t]
      // Heap: [src, NUM(axes_bitmask)].  Output shape = source shape.
      Term a = term_resolve(heap_read(y_loc + 0));
      if (term_tag(a) == TAG_NUM) return grad_zero(target);
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) return grad_zero(target);

      Shape src_shape;
      if (!term_shape_in(a, 0, &src_shape) || src_shape.ndim == 0) {
        return uop_grad(a, gy, target);
      }

      u32 mask       = (u32)term_val(heap_read(y_loc + 1));
      Term gy_shaped = uop_expand(gy, src_shape.ndim, src_shape.dims);
      Term g         = uop_flip(gy_shaped, mask);
      return uop_grad(a, g, target);
    }

    case UOP_PERMUTE: {
      // PERMUTE reorders axes: out.dim[i] = src.dim[perm[i]].  The
      // gradient permutes the cotangent by the INVERSE permutation
      // so axes line up with the source again:
      //   GRAD[PERMUTE(a, perm), gy, t] =
      //     GRAD[a, PERMUTE(EXPAND(gy, out_shape), inv_perm), t]
      // where inv_perm[perm[i]] = i.
      // Heap: [src, NUM(p0), NUM(p1), ...]; ndim cached in ext.
      Term a = term_resolve(heap_read(y_loc + 0));
      if (term_tag(a) == TAG_NUM) return grad_zero(target);
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) return grad_zero(target);

      Shape src_shape;
      if (!term_shape_in(a, 0, &src_shape) || src_shape.ndim == 0) {
        return uop_grad(a, gy, target);
      }

      u32 ndim = src_shape.ndim;
      u32 perm[MAX_DIM], inv_perm[MAX_DIM], out_dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        perm[i] = (u32)term_val(heap_read(y_loc + 1 + i));
        out_dims[i] = src_shape.dims[perm[i]];
      }
      for (u32 i = 0; i < ndim; i++) inv_perm[perm[i]] = i;

      Term gy_shaped = uop_expand(gy, ndim, out_dims);
      Term g         = uop_permute(gy_shaped, ndim, inv_perm);
      return uop_grad(a, g, target);
    }

    case UOP_PAD: {
      // PAD zero-fills [b_i, e_i] on each axis.  The gradient
      // SHRINKs the cotangent back to the unpadded extent:
      //   GRAD[PAD(a, [b,e]), gy, t] =
      //     GRAD[a, SHRINK(EXPAND(gy, out_shape),
      //                    [b_i, b_i + src_dim_i)), t]
      // Heap layout: [src, NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...]
      // (ndim implicit in src).  gy lifted to PAD's output shape
      // first so SHRINK has a well-formed per-axis source extent
      // (mirrors the SHRINK rule's reasoning above).
      Term a = term_resolve(heap_read(y_loc + 0));
      if (term_tag(a) == TAG_NUM) return grad_zero(target);
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) return grad_zero(target);

      Shape src_shape;
      if (!term_shape_in(a, 0, &src_shape) || src_shape.ndim == 0) {
        return uop_grad(a, gy, target);
      }

      u32 out_dims[MAX_DIM];
      u32 ranges[2 * MAX_DIM];
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 b = (u32)term_val(heap_read(y_loc + 1 + 2 * i));
        u32 e = (u32)term_val(heap_read(y_loc + 2 + 2 * i));
        out_dims[i]        = src_shape.dims[i] + b + e;
        ranges[2 * i + 0]  = b;
        ranges[2 * i + 1]  = b + src_shape.dims[i];
      }
      Term gy_shaped = uop_expand(gy, src_shape.ndim, out_dims);
      Term g         = uop_shrink(gy_shaped, src_shape.ndim, ranges);
      return uop_grad(a, g, target);
    }

    default:
      fprintf(stderr, "interact_grad: unhandled UOp opcode %u\n", y_op);
      return grad_zero(target);
  }
}
