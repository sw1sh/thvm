// b ** e, tinygrad's POW (mixin/elementwise.py:547-570) as its float decomposition
// (codegen/decomp/transcendental.py:257-265): exp2(log2|b| * e), NaN for a negative
// base under a non-integer exponent (a -inf base stays |b| ** e), negated for an
// odd integer exponent, and 1 wherever e == 0.  An int pair computes in f32 and
// rounds back to its dtype.
fn Tensor nn_pow(Tensor b, Tensor e) {
  if (!nn_broadcast(&b, &e)) {
    b.t = 0;
    return b;
  }
  u32 dt  = b.dtype == e.dtype ? b.dtype : nn_promote(b.dtype, e.dtype);
  u32 fdt = dtype_is_float(dt) ? dt : DT_FP32;
  b = nn_cast(b, fdt);
  e = nn_cast(e, fdt);
  Tensor neg  = nn_cmplt(b, nn_scalar(b, 0.0, 1));
  Tensor absb = nn_where(neg, nn_unary(UOP_NEG, b), b);
  Tensor ret  = nn_unary(UOP_EXP2, nn_binop(UOP_MUL, nn_unary(UOP_LOG2, absb), e));
  // e is an integer when it survives a round trip through i32; odd when |e| - 2 *
  // trunc(|e| / 2) is 1 (no value-level mod).
  Tensor isint = nn_binop(UOP_CMPEQ, e, nn_cast(nn_cast(e, DT_INT32), fdt));
  Tensor abse  = nn_where(nn_cmplt(e, nn_scalar(e, 0.0, 1)), nn_unary(UOP_NEG, e), e);
  Tensor halfe = nn_cast(nn_cast(nn_binop_f(UOP_MUL, abse, 0.5), DT_INT32), fdt);
  Tensor odd   = nn_binop(UOP_CMPEQ, nn_sub(abse, nn_binop_f(UOP_MUL, halfe, 2.0)),
                          nn_scalar(abse, 1.0, 1));
  Tensor ninf  = nn_binop(UOP_CMPEQ, b, nn_scalar(b, -INFINITY, 1));
  Tensor nonint = nn_where(ninf, ret, nn_scalar(ret, NAN, 1));
  Tensor negb  = nn_where(isint, nn_where(odd, nn_unary(UOP_NEG, ret), ret), nonint);
  Tensor out   = nn_where(neg, negb, ret);
  out = nn_where(nn_binop(UOP_CMPEQ, e, nn_scalar(e, 0.0, 1)), nn_scalar(out, 1.0, 1), out);
  if (fdt != dt) {
    // Round half away from zero, then truncate into the int dtype.
    Tensor down = nn_binop_f(UOP_ADD, out, -0.5), up = nn_binop_f(UOP_ADD, out, 0.5);
    out = nn_cast(nn_where(nn_cmplt(out, nn_scalar(out, 0.0, 1)), down, up), dt);
  }
  return out;
}
