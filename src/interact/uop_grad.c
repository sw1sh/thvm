// interact/uop_grad.c - chain-rule rewrite for UOP_GRAD.
//
// Step-13 minimal scope: ADD, MUL, NEG, REDUCE_SUM, plus the leaf
// short-circuits (target match, other TAG_TEN, NUM, CONST).  Fired
// by wnf when it enters a UOP_GRAD term; recursively applies the
// chain rule until no UOP_GRAD nodes remain in the output graph.
//
// VJP semantics: GRAD[y, gy, target] = ∂y/∂target · gy.
//
// Each chain-rule node that branches (ADD's two summands, MUL's two
// product-rule terms) lifts gy to target's shape *independently* per
// branch instead of sharing one upfront EXPAND.  Without per-branch
// allocation the graph would have a single EXPAND with N consumers,
// and visualisations that don't have DUPs for tensors lose all but
// one of those consumer wires.  `gy_lifted` tracks whether the
// caller has already wrapped gy in an EXPAND (so the leaf path
// doesn't add a redundant outer wrapping).
//
//   - leaf:  y === target           -> gy            (lift if not lifted)
//            y is TAG_TEN otherwise  -> EXPAND(CONST(0), target)
//            y is TAG_NUM            -> EXPAND(CONST(0), target)
//   - UOP_CONST                      -> EXPAND(CONST(0), target)
//   - UOP_ADD[a, b]                  -> ADD[ grad(a, gy, target, lifted),
//                                             grad(b, gy, target, lifted) ]
//   - UOP_MUL[a, b]                  -> ADD[ grad(a, MUL[b, EXPAND_a(gy)], target, 1),
//                                             grad(b, MUL[a, EXPAND_b(gy)], target, 1) ]
//   - UOP_NEG[a]                     -> grad(a, NEG[EXPAND(gy)], target, 1)
//   - UOP_REDUCE_SUM[a, axis]        -> grad(a, EXPAND(gy), target, 1)
// Anything else returns EXPAND(CONST(0), target) and prints a warning.

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

// `gy_lifted` is non-zero when the caller has already wrapped gy in
// an EXPAND to target.shape.  Used to skip redundant outer wrappings
// at deep leaf positions.
fn Term grad_rec(Term y, Term gy, Term target, int gy_lifted) {
  if (y == target) return gy_lifted ? gy : expand_to_target(gy, target);

  u8 y_tag = term_tag(y);
  if (y_tag == TAG_TEN || y_tag == TAG_NUM) return grad_zero(target);
  if (y_tag != TAG_UOP)                     return grad_zero(target);

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {
    case UOP_CONST:
      return grad_zero(target);

    case UOP_ADD: {
      // Pass gy through unchanged; each branch independently decides
      // whether to lift (at its leaf match) or push it deeper.
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term ga = grad_rec(a, gy, target, gy_lifted);
      Term gb = grad_rec(b, gy, target, gy_lifted);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_MUL: {
      // d(a*b)/dt = (da/dt)*b + a*(db/dt).  Allocate a *fresh*
      // EXPAND of gy per branch so the two MULs don't share a
      // common-ancestor EXPAND node (which would dangle in any
      // diagram that doesn't fan-out via DUP).
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term gy_a_lift = gy_lifted ? gy : expand_to_target(gy, target);
      Term gy_b_lift = gy_lifted ? gy : expand_to_target(gy, target);
      Term gy_a = uop_binary(UOP_MUL, b, gy_a_lift);
      Term gy_b = uop_binary(UOP_MUL, a, gy_b_lift);
      // The new gy_for_* are MUL outputs of target-shaped operands,
      // so they're target-shaped already -- mark lifted=1.
      Term ga = grad_rec(a, gy_a, target, 1);
      Term gb = grad_rec(b, gy_b, target, 1);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_NEG: {
      Term a      = heap_read(y_loc + 0);
      Term lifted = gy_lifted ? gy : expand_to_target(gy, target);
      Term n_gy   = uop_unary(UOP_NEG, lifted);
      return grad_rec(a, n_gy, target, 1);
    }

    case UOP_REDUCE: {
      // SUM only: gradient broadcasts the cotangent back to the
      // input shape.  REDUCE_MAX needs a one-hot indicator; step 14.
      Term a      = heap_read(y_loc + 0);
      Term lifted = gy_lifted ? gy : expand_to_target(gy, target);
      return grad_rec(a, lifted, target, 1);
    }

    default:
      fprintf(stderr, "interact_grad: unhandled UOp opcode %u\n", y_op);
      return grad_zero(target);
  }
}

fn Term interact_grad(Term grad_term) {
  u64  loc    = term_val(grad_term);
  Term y      = heap_read(loc + 0);
  Term gy     = heap_read(loc + 1);
  Term target = heap_read(loc + 2);
  // gy starts unlifted (typically a scalar UOP_CONST); each chain
  // rule node will lift it as needed.
  return grad_rec(y, gy, target, 0);
}
