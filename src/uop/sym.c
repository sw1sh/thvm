// tinygrad's sym value rules on a kernel (uop/symbolic.py:466-468), at emit for
// every route: x * (1 / (1 + x)) is 1 - 1 / (1 + x), so a sigmoid's gradient,
// e * (1 / (1 + e))^2 with e an exp2 past float range, stays finite where the
// product read inf * 0.

// The value of a float CONST behind RESHAPE and EXPAND.
static int sym_float_const(Term t, f32 *v) {
  while (term_tag(t) == TAG_UOP && (term_ext(t) == UOP_RESHAPE || term_ext(t) == UOP_EXPAND)) {
    t = heap_read(term_val(t));
  }
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term c = heap_read(term_val(t));
  if (!dtype_is_float(term_ext(c))) return 0;
  u32 b = (u32)term_val(c);
  memcpy(v, &b, sizeof *v);
  return 1;
}

// 1 iff d is 1 / (1 + x), either ADD order; *one is its CONST 1.
static int sym_recip_one_plus(Term d, Term x, Term *one) {
  if (term_tag(d) != TAG_UOP || term_ext(d) != UOP_RECIP) return 0;
  Term s = heap_read(term_val(d));
  if (term_tag(s) != TAG_UOP || term_ext(s) != UOP_ADD) return 0;
  for (u32 k = 0; k < 2; k++) {
    Term c = heap_read(term_val(s) + k);
    f32  v;
    if (heap_read(term_val(s) + 1 - k) == x && sym_float_const(c, &v) && v == 1.0f) {
      *one = c;
      return 1;
    }
  }
  return 0;
}

static int sym_is(Term t, u32 op) {
  return term_tag(t) == TAG_UOP && term_ext(t) == op;
}

static Term sym_mul(Term a, Term b);

// a * b rewritten, or 0: -x * y -> -(x * y), tinygrad's (x * c) * y -> (x * y) * c
// for c = -1, then x * d -> 1 - d, x * (d * y) -> y * (1 - d) and
// x * (d + y) -> (1 - d) + x * y, each MUL and ADD either way round, as tinygrad's
// commutative UPats match.
static Term sym_mul_rule(Term a, Term b) {
  Term ab[2] = { a, b };
  for (u32 k = 0; k < 2; k++) {
    if (sym_is(ab[k], UOP_NEG)) {
      return uop_unary(UOP_NEG, sym_mul(heap_read(term_val(ab[k])), ab[1 - k]));
    }
  }
  for (u32 k = 0; k < 2; k++) {
    Term x = ab[k], o = ab[1 - k], one;
    if (sym_recip_one_plus(o, x, &one)) {
      return uop_binary(UOP_ADD, one, uop_unary(UOP_NEG, o));
    }
    if (!sym_is(o, UOP_MUL) && !sym_is(o, UOP_ADD)) continue;
    for (u32 j = 0; j < 2; j++) {
      Term d = heap_read(term_val(o) + j), y = heap_read(term_val(o) + 1 - j);
      if (!sym_recip_one_plus(d, x, &one)) continue;
      Term not_d = uop_binary(UOP_ADD, one, uop_unary(UOP_NEG, d));
      if (sym_is(o, UOP_MUL)) return sym_mul(y, not_d);
      return uop_binary(UOP_ADD, not_d, sym_mul(x, y));
    }
  }
  return 0;
}

// a * b with the rules applied to the product a rule builds, which the rewrite
// does not revisit.
static Term sym_mul(Term a, Term b) {
  Term r = sym_mul_rule(a, b);
  return r ? r : uop_binary(UOP_MUL, a, b);
}

// A CONST of dtype dt holding v (a float's f32 bits, an int's low 32).
static Term sym_c(u32 dt, double v) {
  if (dtype_is_float(dt)) {
    f32 f = (f32)v;
    u32 b;
    memcpy(&b, &f, sizeof b);
    return uop_const(dt, b);
  }
  return uop_const(dt, (u32)(i64)v);
}

static Term sym_ne(Term a, Term b) {
  return uop_where(uop_binary(UOP_CMPEQ, a, b), sym_c(DT_BOOL, 0), sym_c(DT_BOOL, 1));
}

// tinygrad's xpow (codegen/decomp/transcendental.py:257-265): exp2(e log2 |b|),
// nan for a negative base under a non-integer e (a -inf base stays |b| ** e),
// negated for an odd integer e, and 1 at e == 0.
static Term sym_xpow(Term b, Term e, u32 dt) {
  Term zero = sym_c(dt, 0), one = sym_c(dt, 1);
  Term neg  = uop_binary(UOP_CMPLT, b, zero);
  Term ret  = uop_unary(UOP_EXP2, uop_binary(UOP_MUL, uop_unary(UOP_LOG2,
                  uop_where(neg, uop_unary(UOP_NEG, b), b)), e));
  Term is_int = uop_binary(UOP_CMPEQ, e, uop_cast(uop_cast(e, DT_INT32), dt));
  Term abse   = uop_where(uop_binary(UOP_CMPLT, e, zero), uop_unary(UOP_NEG, e), e);
  Term is_odd = sym_ne(uop_int_binary(UOP_IMOD, uop_cast(abse, DT_INT32), sym_c(DT_INT32, 2)),
                       sym_c(DT_INT32, 0));
  Term nonint = uop_where(sym_ne(b, sym_c(dt, -INFINITY)), sym_c(dt, NAN), ret);
  Term neg_b  = uop_where(is_int, uop_where(is_odd, uop_unary(UOP_NEG, ret), ret), nonint);
  return uop_where(uop_binary(UOP_CMPEQ, e, zero), one, uop_where(neg, neg_b, ret));
}

// An int POW off the CPU, whose renderers have no thvm_ipow: xpow in f32 rounded
// half away from zero, and xpow_int's 0 under a negative exponent but for a base
// of 1 or -1 (transcendental.py:279-283); exact below 2**24.
static Term sym_ipow_f32(Term b, Term e, u32 dt) {
  Term r   = sym_xpow(uop_cast(b, DT_FP32), uop_cast(e, DT_FP32), DT_FP32);
  Term ret = uop_cast(uop_binary(UOP_ADD, r, uop_where(uop_binary(UOP_CMPLT, r, sym_c(DT_FP32, 0)),
                                                         sym_c(DT_FP32, -0.5), sym_c(DT_FP32, 0.5))), dt);
  if (!dtype_is_signed(dt)) return ret;
  Term one = sym_c(dt, 1);
  Term neg = uop_where(uop_binary(UOP_CMPEQ, b, sym_c(dt, -1)),
                       uop_where(sym_ne(uop_int_binary(UOP_IAND, e, one), sym_c(dt, 0)), sym_c(dt, -1), one),
                       uop_where(uop_binary(UOP_CMPEQ, b, one), one, sym_c(dt, 0)));
  return uop_where(uop_binary(UOP_CMPLT, e, sym_c(dt, 0)), neg, ret);
}

// The value of a CONST behind RESHAPE and EXPAND, float or int.
static int sym_const(Term t, double *v) {
  f32 f;
  if (sym_float_const(t, &f)) {
    *v = f;
    return 1;
  }
  while (sym_is(t, UOP_RESHAPE) || sym_is(t, UOP_EXPAND)) t = heap_read(term_val(t));
  if (!sym_is(t, UOP_CONST)) return 0;
  Term c = heap_read(term_val(t));
  u32  b = (u32)term_val(c);
  *v = dtype_is_signed(term_ext(c)) ? (double)(i32)b : (double)b;
  return 1;
}

static int  SYM_IPOW;
static Term sym_pow(Term b, Term e);

// tinygrad's simplify_pow (uop/symbolic.py:16-22) for a CONST exponent c: 1 at
// 0, (1 / x) ** -c below it but for an int x, x ** (c - 0.5) * sqrt(x) at a
// half-integer, y * y (* x for an odd c), y = x ** (c // 2), at an integer.  Past
// 256 the squarings, which render as a tree, stay a POW.
static Term sym_pow_c(Term b, double c, u32 dt) {
  if (c < 0) {
    return dtype_is_int(dt) ? 0 : sym_pow(uop_unary(UOP_RECIP, b), sym_c(dt, -c));
  }
  if (c == 0) return sym_c(dt, 1);
  if (c == 1) return b;
  if (c > 256) return 0;
  double h = c - 0.5;
  if ((double)(i64)h + 0.5 == c) {
    return h == 0 ? uop_unary(UOP_SQRT, b)
                  : uop_binary(UOP_MUL, sym_pow(b, sym_c(dt, h)), uop_unary(UOP_SQRT, b));
  }
  if ((double)(i64)c != c) return 0;
  Term y  = sym_pow(b, sym_c(dt, (double)((i64)c / 2)));
  Term yy = uop_binary(UOP_MUL, y, y);
  return ((i64)c % 2) ? uop_binary(UOP_MUL, yy, b) : yy;
}

// tinygrad's POW lowering (uop/symbolic.py:194-197, 451): simplify_pow for a
// CONST exponent, exp2(e log2 c) for a positive float CONST base c (c itself at
// 1), else xpow for a float POW; an int one stays a POW for thvm_ipow
// (codegen/ipow.c) where the renderer holds it.
static Term sym_pow(Term b, Term e) {
  u32    dt = DT_FP32;
  (void)term_dtype_in(b, 0, &dt);
  double c;
  if (sym_const(e, &c)) {
    Term r = sym_pow_c(b, c, dt);
    if (r != 0) return r;
  }
  if (dtype_is_int(dt) && !dtype_is_bool(dt)) {
    return SYM_IPOW ? uop_binary(UOP_POW, b, e) : sym_ipow_f32(b, e, dt);
  }
  f32 cb;
  if (sym_float_const(b, &cb) && cb > 0) {
    return cb == 1.0f ? b : uop_unary(UOP_EXP2, uop_binary(UOP_MUL, e, sym_c(dt, log2((double)cb))));
  }
  return sym_xpow(b, e, dt);
}

static Term uop_sym_rule(Term t, void *user) {
  (void)user;
  if (sym_is(t, UOP_POW)) {
    Term r = sym_pow(heap_read(term_val(t)), heap_read(term_val(t) + 1));
    return r == t ? 0 : r;
  }
  if (!sym_is(t, UOP_MUL)) return 0;
  return sym_mul_rule(heap_read(term_val(t)), heap_read(term_val(t) + 1));
}

// `ipow`: the kernel renders through the C, walker or CUDA route, which hold
// thvm_ipow; Metal's int POW keeps the f32 path.
fn Term uop_sym(Term root, int ipow) {
  UOpGraphRewriteRule rules[1] = { { "sym", uop_sym_rule } };
  SYM_IPOW = ipow;
  return uop_graph_rewrite(root, rules, 1, NULL);
}
