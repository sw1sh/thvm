// e's value when it is a broadcast CONST, as a Python scalar exponent is.
static int nn_pow_const_exp(Tensor e, double *c) {
  Term t = e.t;
  while (term_tag(t) == TAG_UOP && (term_ext(t) == UOP_RESHAPE || term_ext(t) == UOP_EXPAND)) {
    t = heap_read(term_val(t));
  }
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term k    = heap_read(term_val(t));
  u32  bits = (u32)term_val(k);
  if (dtype_is_float(term_ext(k))) {
    f32 f;
    memcpy(&f, &bits, sizeof f);
    *c = f;
  } else {
    *c = dtype_is_signed(term_ext(k)) ? (double)(i32)bits : (double)bits;
  }
  return 1;
}

// tinygrad's simplify_pow (uop/symbolic.py:16-21) for a constant exponent c: 1 at
// 0, (1 / x) ** -c below it, x ** (c - 0.5) * sqrt(x) at a half-integer, and
// y * y (* x for an odd c), y = x ** (c // 2), at an integer; t == 0 otherwise.
static Tensor nn_pow_c(Tensor x, double c) {
  if (c < 0) {
    return nn_pow_c(nn_unary(UOP_RECIP, x), -c);
  }
  if (c == 0) {
    return nn_full(1.0, x.dtype, x.shape);
  }
  if (c == 1) {
    return x;
  }
  double h = c - 0.5;
  if ((double)(i64)h + 0.5 == c) {
    return h == 0 ? nn_unary(UOP_SQRT, x) : nn_binop(UOP_MUL, nn_pow_c(x, h), nn_unary(UOP_SQRT, x));
  }
  if ((double)(i64)c != c) {
    x.t = 0;
    return x;
  }
  Tensor y  = nn_pow_c(x, (double)((i64)c / 2));
  Tensor yy = nn_binop(UOP_MUL, y, y);
  return ((i64)c % 2) ? nn_binop(UOP_MUL, yy, x) : yy;
}

// b ** e, tinygrad's POW (mixin/elementwise.py:547-570) as its float decomposition
// (codegen/decomp/transcendental.py:257-265): exp2(log2|b| * e), NaN for a negative
// base under a non-integer exponent (a -inf base stays |b| ** e), negated for an
// odd integer exponent, and 1 wherever e == 0.  An int pair computes in f32 and
// rounds back to its dtype, a negative exponent giving xpow_int's 0, or +-1 for a
// base of +-1 (transcendental.py:279-282).  The gradient is the decomposition's;
// tinygrad's POW has its own rule (mixin/gradient.py:75-76), which differs at a
// zero or negative base.
fn Tensor nn_pow(Tensor b, Tensor e) {
  if (!nn_broadcast(&b, &e)) {
    b.t = 0;
    return b;
  }
  u32 dt  = b.dtype == e.dtype ? b.dtype : nn_promote(b.dtype, e.dtype);
  double c;
  if (nn_pow_const_exp(e, &c) && (dtype_is_float(dt) || c >= 0) && c > -2147483648.0
      && c < 2147483648.0) {
    Tensor r = nn_pow_c(nn_cast(b, dt), c);
    if (r.t != 0) {
      return r;
    }
  }
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
    // Round half away from zero; under a negative exponent only a base of +-1
    // keeps a nonzero value, its sign that of odd e for -1.
    Tensor down = nn_binop_f(UOP_ADD, out, -0.5), up = nn_binop_f(UOP_ADD, out, 0.5);
    Tensor near = nn_where(nn_cmplt(out, nn_scalar(out, 0.0, 1)), down, up);
    Tensor sgn  = nn_where(odd, nn_scalar(out, -1.0, 1), nn_scalar(out, 1.0, 1));
    Tensor neg1 = nn_where(nn_binop(UOP_CMPEQ, b, nn_scalar(b, -1.0, 1)), sgn,
                           nn_where(nn_binop(UOP_CMPEQ, b, nn_scalar(b, 1.0, 1)),
                                    nn_scalar(out, 1.0, 1), nn_scalar(out, 0.0, 1)));
    out = nn_cast(nn_where(nn_cmplt(e, nn_scalar(e, 0.0, 1)), neg1, near), dt);
  }
  return out;
}
