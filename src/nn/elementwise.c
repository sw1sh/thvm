// tinygrad's composite elementwise ops (mixin/elementwise.py) on the unary and
// binary UOps, each in Tensor's own construction order (py/thvm/tensor.py), so
// the graphs agree.  x itself, t == 0, for an op past NN_ELEMENTWISE_COUNT.
fn Tensor nn_elementwise(u32 op, Tensor x) {
  switch (op) {
    case NN_EXP: {
      return nn_unary(UOP_EXP2, nn_binop_f(UOP_MUL, x, 1.4426950408889634));
    }
    case NN_LOG: {
      return nn_binop_f(UOP_MUL, nn_unary(UOP_LOG2, x), 0.6931471805599453);
    }
    case NN_SIGMOID: {
      Tensor e = nn_elementwise(NN_EXP, nn_unary(UOP_NEG, x));
      return nn_unary(UOP_RECIP, nn_binop_f(UOP_ADD, e, 1.0));
    }
    case NN_TANH: {
      // 2 * sigmoid(2x) - 1 (mixin/elementwise.py tanh).
      Tensor s = nn_elementwise(NN_SIGMOID, nn_binop_f(UOP_MUL, x, 2.0));
      return nn_binop_f(UOP_ADD, nn_binop_f(UOP_MUL, s, 2.0), -1.0);
    }
    case NN_RELU: {
      // (x > 0).where(x, 0), not maximum(0), which would pass half the gradient
      // through at 0 (mixin/elementwise.py:675-684).
      Tensor mask = nn_cmplt(nn_scalar(x, 0.0, 0), x);
      return nn_where(mask, x, nn_scalar(x, 0.0, 0));
    }
    case NN_GELU: {
      // 0.5 x (1 + tanh(sqrt(2/pi) (x + 0.044715 x^3))) (mixin/elementwise.py:780-795).
      Tensor half = nn_binop_f(UOP_MUL, x, 0.5);
      Tensor cube = nn_binop(UOP_MUL, nn_binop(UOP_MUL, x, x), x);
      Tensor in   = nn_binop(UOP_ADD, x, nn_binop_f(UOP_MUL, cube, 0.044715));
      Tensor th   = nn_elementwise(NN_TANH, nn_binop_f(UOP_MUL, in, 0.7978845608028654));
      return nn_binop(UOP_MUL, half, nn_binop_f(UOP_ADD, th, 1.0));
    }
    case NN_GELU_ERF: {
      Tensor half = nn_binop_f(UOP_MUL, x, 0.5);
      Tensor e    = nn_elementwise(NN_ERF, nn_binop_f(UOP_MUL, x, 0.7071067811865475));
      return nn_binop(UOP_MUL, half, nn_binop_f(UOP_ADD, e, 1.0));
    }
    case NN_ERF: {
      // Abramowitz and Stegun 7.1.26, as tinygrad (mixin/elementwise.py:1059-1071).
      static double const coef[5] = {1.061405429, -1.453152027, 1.421413741, -0.284496736,
                                     0.254829592};
      Tensor neg  = nn_cmplt(x, nn_scalar(x, 0.0, 0));
      Tensor m1   = nn_scalar(neg, -1.0, 1);
      Tensor sign = nn_where(neg, m1, nn_scalar(neg, 1.0, 1));
      Tensor q    = nn_binop_f(UOP_MUL, nn_binop(UOP_MUL, sign, x), 0.3275911);
      Tensor t    = nn_binop_f(UOP_MUL, nn_unary(UOP_RECIP, nn_binop_f(UOP_ADD, q, 1.0)), 1.0);
      // functools.reduce(acc * t + c, coef, 0.0): the first acc is the float 0.0.
      Tensor poly = nn_binop_f(UOP_ADD, nn_binop_f(UOP_MUL, t, 0.0), coef[0]);
      for (u32 i = 1; i < 5; i++) {
        poly = nn_binop_f(UOP_ADD, nn_binop(UOP_MUL, poly, t), coef[i]);
      }
      Tensor tp = nn_binop(UOP_MUL, t, poly);
      Tensor g  = nn_elementwise(NN_EXP, nn_unary(UOP_NEG, nn_elementwise(NN_SQUARE, x)));
      Tensor r  = nn_binop_f(UOP_ADD, nn_unary(UOP_NEG, nn_binop(UOP_MUL, tp, g)), 1.0);
      return nn_binop(UOP_MUL, sign, r);
    }
    case NN_RSQRT: {
      return nn_unary(UOP_RECIP, nn_unary(UOP_SQRT, x));
    }
    case NN_SQUARE: {
      return nn_binop(UOP_MUL, x, x);
    }
    case NN_SIGN: {
      // (x != 0).where((x < 0).where(-1, 1), 0) (mixin/elementwise.py:902-910), with
      // the outer select on x == 0 since there is no CMPNE.
      Tensor zero = nn_scalar(x, 0.0, 0);
      Tensor neg  = nn_where(nn_cmplt(x, zero), nn_scalar(x, -1.0, 0), nn_scalar(x, 1.0, 0));
      return nn_where(nn_binop(UOP_CMPEQ, x, zero), nn_scalar(x, 0.0, 0), neg);
    }
    case NN_ABS: {
      // x * sign(x) (mixin/elementwise.py:912-920): its gradient at 0 is 0.
      return nn_binop(UOP_MUL, x, nn_elementwise(NN_SIGN, x));
    }
    default: {
      x.t = 0;
      return x;
    }
  }
}
