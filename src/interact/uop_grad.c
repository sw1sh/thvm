// interact/uop_grad.c - chain-rule rewrite for UOP_GRAD.
//
// Step-13 minimal scope: ADD, MUL, NEG, REDUCE_SUM, plus the leaf
// short-circuits (target match, other TAG_TEN, NUM, CONST).  Fired
// by wnf when it enters a UOP_GRAD term; recursively applies the
// chain rule until no UOP_GRAD nodes remain in the output graph.
//
// VJP semantics: GRAD[y, gy, target] = ∂y/∂target · gy.
//
// `interact_grad` lifts `gy` to target's shape ONCE upfront so the
// chain rule operates entirely on target-shaped tensors -- no
// implicit broadcast in the intermediate MUL/ADD/NEG ops, no
// per-leaf EXPAND wrapping.
//
//   - leaf:  y === target           -> gy            (already lifted)
//            y is TAG_TEN otherwise  -> CONST(0) lifted to target.shape
//            y is TAG_NUM            -> CONST(0) lifted to target.shape
//   - UOP_CONST                      -> CONST(0) lifted to target.shape
//   - UOP_ADD[a, b]                  -> ADD[ grad(a, gy, target),
//                                             grad(b, gy, target) ]
//   - UOP_MUL[a, b]                  -> ADD[ grad(a, MUL[b, gy], target),
//                                             grad(b, MUL[a, gy], target) ]
//   - UOP_NEG[a]                     -> grad(a, NEG[gy], target)
//   - UOP_REDUCE_SUM[a, axis]        -> grad(a, gy, target)
// Anything else returns CONST(0) lifted to target and prints a warning.

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

// Recursive driver: applies the chain rule fully to (y, gy, target),
// returning a UOp graph that contains no UOP_GRAD nodes.  `gy` is
// always assumed to be in target's shape on entry (interact_grad
// guarantees this for the top-level call; recursive callers
// preserve the invariant since MUL/NEG of target-shaped values are
// target-shaped).
fn Term grad_rec(Term y, Term gy, Term target) {
  if (y == target) return gy;

  u8 y_tag = term_tag(y);
  if (y_tag == TAG_TEN || y_tag == TAG_NUM) return grad_zero(target);
  if (y_tag != TAG_UOP)                     return grad_zero(target);

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {
    case UOP_CONST:
      return grad_zero(target);

    case UOP_ADD: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term ga = grad_rec(a, gy, target);
      Term gb = grad_rec(b, gy, target);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_MUL: {
      // d(a*b)/dt = (da/dt)*b + a*(db/dt); push the product into gy.
      // Both `a` and `b` are target-shaped (the source graph operates
      // at one shape) so MUL with the target-shaped gy keeps the
      // invariant intact.
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term gy_a = uop_binary(UOP_MUL, b, gy);
      Term gy_b = uop_binary(UOP_MUL, a, gy);
      Term ga   = grad_rec(a, gy_a, target);
      Term gb   = grad_rec(b, gy_b, target);
      return uop_binary(UOP_ADD, ga, gb);
    }

    case UOP_NEG: {
      Term a    = heap_read(y_loc + 0);
      Term n_gy = uop_unary(UOP_NEG, gy);
      return grad_rec(a, n_gy, target);
    }

    case UOP_REDUCE: {
      // SUM only: gradient broadcasts the cotangent back to the
      // input shape.  REDUCE_MAX needs a one-hot indicator; step 14.
      Term a = heap_read(y_loc + 0);
      return grad_rec(a, gy, target);
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
  // Lift gy to target's shape ONCE so the chain rule below sees a
  // target-shaped cotangent at every step.  Removes the need to
  // either implicit-broadcast inside MUL or wrap each leaf with a
  // post-hoc EXPAND.
  Term gy_lifted = expand_to_target(gy, target);
  return grad_rec(y, gy_lifted, target);
}
