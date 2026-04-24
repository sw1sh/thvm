// interact/uop_grad.c - chain-rule rewrite for UOP_GRAD.
//
// Step-13 minimal scope: ADD, MUL, NEG, REDUCE_SUM, plus the leaf
// short-circuits (target match, other TAG_TEN, NUM, CONST).  Fired
// by wnf when it enters a UOP_GRAD term; recursively applies the
// chain rule until no UOP_GRAD nodes remain in the output graph.
//
// VJP semantics: GRAD[y, gy, target] = ∂y/∂target · gy.
//   - leaf:  y === target           -> EXPAND(gy, target.shape)
//            y is TAG_TEN otherwise  -> EXPAND(CONST(0), target.shape)
//            y is TAG_NUM            -> EXPAND(CONST(0), target.shape)
//   - UOP_CONST                      -> EXPAND(CONST(0), target.shape)
//   - UOP_ADD[a, b]                  -> ADD[ grad(a, gy, target), grad(b, gy, target) ]
//   - UOP_MUL[a, b]                  -> ADD[ grad(a, MUL[b, gy], target),
//                                             grad(b, MUL[a, gy], target) ]
//   - UOP_NEG[a]                     -> grad(a, NEG[gy], target)
//   - UOP_REDUCE_SUM[a, axis]        -> grad(a, gy, target)  (gy broadcasts via materialize)
// Anything else returns EXPAND(CONST(0), target.shape) and prints a warning.

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
// returning a UOp graph that contains no UOP_GRAD nodes.
fn Term grad_rec(Term y, Term gy, Term target) {
  // Leaf: this y is exactly the target tensor.  Lift the cotangent
  // to target's shape so the gradient is target-shaped without an
  // outer broadcast wrapper.
  if (y == target) return expand_to_target(gy, target);

  // Other concrete leaves: zero gradient (no dependency).
  u8 y_tag = term_tag(y);
  if (y_tag == TAG_TEN || y_tag == TAG_NUM) return grad_zero(target);

  if (y_tag != TAG_UOP) return grad_zero(target);

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
      // input shape.  Materialize handles the shape-1 broadcast in
      // elementwise ops.  REDUCE_MAX needs a one-hot indicator;
      // step 14 task.
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
  return grad_rec(y, gy, target);
}
