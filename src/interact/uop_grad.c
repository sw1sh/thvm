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

// EXPAND a scalar / shape-{1} producer to target's shape.  Returns
// `src` unchanged if target isn't a TAG_TEN (no shape to look up)
// or if its descriptor has rank 0.
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
      // SUM only: gradient broadcasts the cotangent back to the
      // input shape.  REDUCE_MAX needs a one-hot indicator; step 14.
      Term a      = heap_read(y_loc + 0);
      Term lifted = expand_to_target(gy, target);
      return uop_grad(a, lifted, target);
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

      Term g = gy;
      for (i32 axis = (i32)src_shape.ndim - 1; axis >= 0; axis--) {
        u32 out_dim = (u32)term_val(heap_read(y_loc + 1 + axis));
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
