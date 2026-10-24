// b ** e, tinygrad's POW (mixin/elementwise.py:549-568) over the promoted
// operands: uop/sym.c lowers it at emit, simplify_pow for a constant exponent,
// xpow_int for an int pair and xpow for a float one, and its gradient is
// tinygrad's POW rule (interact/uop_grad.c).
fn Tensor nn_pow(Tensor b, Tensor e) {
  if (!nn_broadcast(&b, &e)) {
    b.t = 0;
    return b;
  }
  u32 dt = b.dtype == e.dtype ? b.dtype : nn_promote(b.dtype, e.dtype);
  b = nn_cast(b, dt);
  b.t = uop_binary(UOP_POW, b.t, nn_cast(e, dt).t);
  return b;
}
