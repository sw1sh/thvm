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

static Term uop_sym_rule(Term t, void *user) {
  (void)user;
  if (!sym_is(t, UOP_MUL)) return 0;
  return sym_mul_rule(heap_read(term_val(t)), heap_read(term_val(t) + 1));
}

fn Term uop_sym(Term root) {
  UOpGraphRewriteRule rules[1] = { { "sym", uop_sym_rule } };
  return uop_graph_rewrite(root, rules, 1, NULL);
}
