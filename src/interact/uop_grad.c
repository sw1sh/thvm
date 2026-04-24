// interact/uop_grad.c - chain-rule rewrite for UOP_GRAD.
//
// Step-13 minimal scope: ADD, MUL, NEG, REDUCE_SUM, plus the leaf
// short-circuits (target match, other TAG_TEN, NUM, CONST).  Fired
// by wnf when it enters a UOP_GRAD term; recursively applies the
// chain rule until no UOP_GRAD nodes remain in the output graph.
//
// VJP semantics: GRAD[y, gy, target] = ∂y/∂target · gy.
//   - leaf:  y === target           -> gy
//            y is TAG_TEN otherwise  -> CONST(0, "f32")  (broadcast at materialize)
//            y is TAG_NUM            -> CONST(0, "f32")
//   - UOP_CONST                      -> CONST(0)
//   - UOP_ADD[a, b]                  -> ADD[ grad(a, gy, target), grad(b, gy, target) ]
//   - UOP_MUL[a, b]                  -> ADD[ grad(a, MUL[b, gy], target),
//                                             grad(b, MUL[a, gy], target) ]
//   - UOP_NEG[a]                     -> grad(a, NEG[gy], target)
//   - UOP_REDUCE_SUM[a, axis]        -> grad(a, gy, target)  (gy broadcasts via materialize)
// Anything else returns CONST(0) and prints a warning.

fn Term grad_zero(void) {
  // 0.0f bits = 0.  Shape {1}; broadcast at materialize when
  // combined with the actual gradient flow.
  return uop_const(DT_F32, 0);
}

// Recursive driver: applies the chain rule fully to (y, gy, target),
// returning a UOp graph that contains no UOP_GRAD nodes.
fn Term grad_rec(Term y, Term gy, Term target) {
  // Leaf: this y is exactly the target tensor.  Return gy.
  if (y == target) return gy;

  // Other concrete leaves: zero gradient (no dependency).
  u8 y_tag = term_tag(y);
  if (y_tag == TAG_TEN || y_tag == TAG_NUM) return grad_zero();

  if (y_tag != TAG_UOP) return grad_zero();

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {
    case UOP_CONST:
      return grad_zero();

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
      return grad_zero();
  }
}

fn Term interact_grad(Term grad_term) {
  u64  loc    = term_val(grad_term);
  Term y      = heap_read(loc + 0);
  Term gy     = heap_read(loc + 1);
  Term target = heap_read(loc + 2);
  Term raw    = grad_rec(y, gy, target);

  // Broadcast the raw gradient to target's shape.  Materialize's
  // elementwise broadcasting picks the larger side, so combining
  // raw (often shape {1} when the chain rule collapsed to constants
  // or to gy alone) with target * 0 forces the output to target's
  // shape without changing the numeric value: raw + (target * 0)
  // = raw, broadcast.
  Term zero       = uop_const(DT_F32, 0);
  Term target_zero = uop_binary(UOP_MUL, target, zero);
  return uop_binary(UOP_ADD, raw, target_zero);
}
