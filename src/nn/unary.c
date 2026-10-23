// A unary UOp (NEG, RECIP, EXP2, LOG2, SQRT, ...): x's dtype and shape.
fn Tensor nn_unary(u32 op, Tensor x) {
  x.t = uop_unary(op, x.t);
  return x;
}
