// uop/index_simplify.c - constructor-time simplifier for the integer INDEX
// layer.  uop_int_binary and uop_iwhere fold through here before hash-consing
// and return the folded Term when a rule fires (0: none did), as uop_binary
// does with uop_rewrite_binary.  The rules port tinygrad uop/symbolic.py and
// uop/divandmod.py; without them index ASTs fill up with identity ops.

// 1 with *out set when `t` is an integer CONST.
static int uop_iconst_value(Term t, i64 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t));
  if (term_tag(num) != TAG_NUM) return 0;
  u32 dtype = term_ext(num);
  if (!dtype_is_int(dtype)) return 0;
  *out = (i64)(i32)term_val(num);  // DT_INT32 sign-extension
  return 1;
}

// 1 with *out = the extent (kvar-packed when symbolic) when `t` is a RANGE.
static int uop_range_extent_into(Term t, u32 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  Term ext_cell = heap_read(term_val(t) + 2);
  if (term_tag(ext_cell) != TAG_NUM) return 0;
  *out = (u32)term_val(ext_cell);
  return 1;
}

// A DT_INT32 CONST of `v`, truncated: index values stay within int32.
static Term uop_iconst(i64 v) {
  return uop_const(DT_INT32, (u32)(i32)v);
}

// 1 iff `t` is c*x or x*c with c CONST: *c_out = c, *x_out = x.
static int uop_match_const_mul(Term t, i64 *c_out, Term *x_out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  i64 v;
  if (uop_iconst_value(a, &v)) { *c_out = v; *x_out = b; return 1; }
  if (uop_iconst_value(b, &v)) { *c_out = v; *x_out = a; return 1; }
  return 0;
}

// 1 iff `t` is provably >= 0 by structure.  Blind to IWHERE, so a masked index
// keeps its gate; past depth 64 (a col2im index nests deep) the answer is 0.
static int uop_term_nonneg_d(Term t, u32 depth) {
  if (depth > 64) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_RANGE:  return 1;  // iters in [0, extent)
    case UOP_CONST:  {
      i64 v;
      return uop_iconst_value(t, &v) && v >= 0;
    }
    case UOP_IADD:
    case UOP_IMUL: {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      return uop_term_nonneg_d(a, depth + 1) && uop_term_nonneg_d(b, depth + 1);
    }
    case UOP_IDIV:
    case UOP_IMOD: {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      i64 dv;
      if (!uop_iconst_value(b, &dv) || dv <= 0) return 0;
      return uop_term_nonneg_d(a, depth + 1);
    }
    case UOP_ISHR: {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      i64 sft;
      if (!uop_iconst_value(b, &sft) || sft < 0) return 0;
      return uop_term_nonneg_d(a, depth + 1);
    }
    default: return 0;
  }
}

static int uop_term_nonneg(Term t) { return uop_term_nonneg_d(t, 0); }

// a // d rounded toward -inf, as tinygrad's //; C `/` truncates.
static i64 uop_floordiv_i64(i64 a, i64 d) {
  i64 q = a / d;
  i64 r = a % d;
  if (r != 0 && ((r < 0) != (d < 0))) q -= 1;
  return q;
}

// Checked i64 arithmetic: 1 with *out set, 0 on overflow.
static int uop_chk_add(i64 a, i64 b, i64 *out) { return !__builtin_add_overflow(a, b, out); }
static int uop_chk_sub(i64 a, i64 b, i64 *out) { return !__builtin_sub_overflow(a, b, out); }
static int uop_chk_mul(i64 a, i64 b, i64 *out) { return !__builtin_mul_overflow(a, b, out); }

// gcd(|a|, |b|), with gcd(0, x) == |x|.
static i64 uop_gcd_i64(i64 a, i64 b) {
  if (a < 0) a = -a;
  if (b < 0) b = -b;
  while (b != 0) { i64 t = a % b; a = b; b = t; }
  return a;
}

static int uop_int_bounds_d(Term t, i64 *lo, i64 *hi, u32 depth);
static int uop_is_invalid(Term t);

// [*lo, *hi] over the corner products of [la, ha] and [lb, hb]; 0 on overflow.
static int uop_mul_corner_bounds(i64 la, i64 ha, i64 lb, i64 hb,
                                 i64 *lo, i64 *hi) {
  i64 corners[4];
  if (!uop_chk_mul(la, lb, &corners[0])) return 0;
  if (!uop_chk_mul(la, hb, &corners[1])) return 0;
  if (!uop_chk_mul(ha, lb, &corners[2])) return 0;
  if (!uop_chk_mul(ha, hb, &corners[3])) return 0;
  i64 mn = corners[0], mx = corners[0];
  for (u32 i = 1; i < 4; i++) {
    if (corners[i] < mn) mn = corners[i];
    if (corners[i] > mx) mx = corners[i];
  }
  *lo = mn; *hi = mx;
  return 1;
}

// uop_int_bounds: tinygrad's vmin/vmax (uop/ops.py:1109 _min_max).  1 when `t`
// provably lies in [*lo, *hi]; 0 (unknown) on an unhandled op, a variable
// divisor or overflow, as a too-narrow bound would miscompile later folds.
static int uop_int_bounds_d(Term t, i64 *lo, i64 *hi, u32 depth) {
  if (depth > 64) return 0;
  // A raw heap NUM, not wrapped in a CONST.
  if (term_tag(t) == TAG_NUM) {
    i64 v = (i64)(i32)term_val(t);
    *lo = v; *hi = v;
    return 1;
  }
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_CONST: {
      i64 v;
      if (!uop_iconst_value(t, &v)) return 0;
      *lo = v; *hi = v;
      return 1;
    }
    case UOP_RANGE: {
      // [0, extent - 1], taking a kvar's upper bound, which covers its runtime
      // value.  Not uop_range_static_extent: index.c comes after this file.
      u32 packed;
      if (!uop_range_extent_into(t, &packed)) return 0;
      u32 ext = kvar_extent_static(packed);
      if (ext == 0) return 0;  // empty: no value to bound
      *lo = 0; *hi = (i64)ext - 1;
      return 1;
    }
    case UOP_IADD: {
      i64 la, ha, lb, hb;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 0), &la, &ha, depth + 1)) return 0;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 1), &lb, &hb, depth + 1)) return 0;
      if (!uop_chk_add(la, lb, lo)) return 0;
      if (!uop_chk_add(ha, hb, hi)) return 0;
      return 1;
    }
    case UOP_ISUB: {
      i64 la, ha, lb, hb;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 0), &la, &ha, depth + 1)) return 0;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 1), &lb, &hb, depth + 1)) return 0;
      if (!uop_chk_sub(la, hb, lo)) return 0;
      if (!uop_chk_sub(ha, lb, hi)) return 0;
      return 1;
    }
    case UOP_IMUL: {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      i64 la, ha, lb, hb;
      if (!uop_int_bounds_d(a, &la, &ha, depth + 1)) return 0;
      if (!uop_int_bounds_d(b, &lb, &hb, depth + 1)) return 0;
      i64 c;
      if (uop_iconst_value(a, &c)) {
        if (c >= 0) { if (!uop_chk_mul(c, lb, lo) || !uop_chk_mul(c, hb, hi)) return 0; }
        else        { if (!uop_chk_mul(c, hb, lo) || !uop_chk_mul(c, lb, hi)) return 0; }
        return 1;
      }
      if (uop_iconst_value(b, &c)) {
        if (c >= 0) { if (!uop_chk_mul(c, la, lo) || !uop_chk_mul(c, ha, hi)) return 0; }
        else        { if (!uop_chk_mul(c, ha, lo) || !uop_chk_mul(c, la, hi)) return 0; }
        return 1;
      }
      return uop_mul_corner_bounds(la, ha, lb, hb, lo, hi);
    }
    case UOP_IDIV: {
      // Positive constant divisor and a non-negative numerator only: thvm
      // renders IDIV as C `/` (truncation), which equals floor only there.
      i64 d, la, ha;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &d) || d <= 0) return 0;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 0), &la, &ha, depth + 1) || la < 0) {
        return 0;
      }
      *lo = la / d;
      *hi = ha / d;
      return 1;
    }
    case UOP_IMOD: {
      // C `%` of a negative numerator is negative, so [0, c-1] (and x % c == x)
      // needs a provably non-negative numerator.
      i64 c;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &c) || c <= 0) return 0;
      Term num = heap_read(term_val(t) + 0);
      i64 la, ha;
      int nb = uop_int_bounds_d(num, &la, &ha, depth + 1);
      if (nb && la >= 0 && ha < c) {  // x % c == x
        *lo = la; *hi = ha;
        return 1;
      }
      if (!((nb && la >= 0) || uop_term_nonneg(num))) return 0;
      *lo = 0; *hi = c - 1;
      return 1;
    }
    case UOP_ISHR: {
      // The fast_idiv lowering's (x*m) >> s: a floor divide by 2**s for a
      // non-negative numerator, so an enclosing div/mod still folds.
      i64 sft, la, ha;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &sft) || sft < 0 || sft >= 63) {
        return 0;
      }
      if (!uop_int_bounds_d(heap_read(term_val(t) + 0), &la, &ha, depth + 1) || la < 0) {
        return 0;
      }
      *lo = la >> sft;
      *hi = ha >> sft;
      return 1;
    }
    case UOP_IWHERE: {
      // The union of both branches, ignoring cond; an INVALID branch (a mask,
      // no value) is skipped.
      Term x = heap_read(term_val(t) + 1);
      Term y = heap_read(term_val(t) + 2);
      int xi = uop_is_invalid(x), yi = uop_is_invalid(y);
      if (xi && yi) return 0;
      i64 lx, hx, ly, hy;
      if (yi) return uop_int_bounds_d(x, lo, hi, depth + 1);
      if (xi) return uop_int_bounds_d(y, lo, hi, depth + 1);
      if (!uop_int_bounds_d(x, &lx, &hx, depth + 1)) return 0;
      if (!uop_int_bounds_d(y, &ly, &hy, depth + 1)) return 0;
      *lo = lx < ly ? lx : ly;
      *hi = hx > hy ? hx : hy;
      return 1;
    }
    case UOP_ILT:
      *lo = 0; *hi = 1;
      return 1;
    case UOP_IAND:
    case UOP_IOR: {
      // [0, 1] only for operands in [0, 1]: on wider ints the ops are bitwise.
      i64 la, ha, lb, hb;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 0), &la, &ha, depth + 1)) return 0;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 1), &lb, &hb, depth + 1)) return 0;
      if (la < 0 || ha > 1 || lb < 0 || hb > 1) return 0;
      *lo = 0; *hi = 1;
      return 1;
    }
    default:
      return 0;
  }
}

static int uop_int_bounds(Term t, i64 *lo, i64 *hi) {
  return uop_int_bounds_d(t, lo, hi, 0);
}

// 1 iff `y` is provably in [0, bound); IWHERE-blind through uop_term_nonneg.
static int uop_term_strictly_below(Term y, i64 bound) {
  i64 lo, hi;
  return bound > 0 && uop_term_nonneg(y) && uop_int_bounds(y, &lo, &hi) && hi < bound;
}

// (m, s) with x / d == (x * m) >> s for 0 <= x <= vmax, d > 0: tinygrad
// codegen/decomp/op.py:9-18 magicgu (Hacker's Delight ch. 10).  0 where
// tinygrad asserts (no shift found) or where 2**s would overflow i64.
static int thvm_magicgu(i64 vmax, i64 d, i64 *m_out, i64 *s_out) {
  if (d <= 0 || vmax < 0) return 0;
  i64 nc = (vmax + 1) / d * d - 1;
  i32 nbits = 0;
  for (i64 v = vmax; v > 0; v >>= 1) nbits++;
  for (i32 s = 0; s <= 2 * nbits + 1; s++) {
    if (s >= 62) return 0;
    i64 pow2 = (i64)1 << s;
    i64 rhs = nc * (d - 1 - (pow2 - 1) % d);
    if (pow2 > rhs) {
      *m_out = (pow2 + d - 1 - (pow2 - 1) % d) / d;
      *s_out = s;
      return 1;
    }
  }
  return 0;
}

// 1 iff `n` is c*x + y, either side of the IADD, or c*x with *y_out = 0.
static int uop_match_affine_numerator(Term n, i64 *c_out, Term *x_out,
                                      Term *y_out) {
  if (uop_match_const_mul(n, c_out, x_out)) {
    *y_out = 0;
    return 1;
  }
  if (term_tag(n) != TAG_UOP || term_ext(n) != UOP_IADD) return 0;
  Term a = heap_read(term_val(n) + 0);
  Term b = heap_read(term_val(n) + 1);
  if (uop_match_const_mul(a, c_out, x_out)) { *y_out = b; return 1; }
  if (uop_match_const_mul(b, c_out, x_out)) { *y_out = a; return 1; }
  return 0;
}

// Append the addends of IADD tree `n` to terms[*nt..max_terms); 0 when they do
// not fit or the tree is deeper than 24.
static int uop_iadd_tree_collect_const_mul(Term n, Term *terms, u32 *nt,
                                            u32 max_terms, u32 depth) {
  if (depth > 24 || *nt >= max_terms) return 0;
  if (term_tag(n) == TAG_UOP && term_ext(n) == UOP_IADD) {
    if (!uop_iadd_tree_collect_const_mul(heap_read(term_val(n) + 0),
                                         terms, nt, max_terms, depth + 1)) return 0;
    if (!uop_iadd_tree_collect_const_mul(heap_read(term_val(n) + 1),
                                         terms, nt, max_terms, depth + 1)) return 0;
    return 1;
  }
  if (*nt >= max_terms) return 0;
  terms[(*nt)++] = n;
  return 1;
}

// Side condition of the affine div/mod folds, c*x + rest by `div`: rest >= 0 if
// div | c, 0 <= rest < c if c | div; *rest_out = the sum of rest_terms[].
static int uop_affine_rest_ok(i64 c, i64 div, Term const *rest_terms,
                              u32 n_rest, Term *rest_out) {
  if (c <= 0 || div <= 0) return 0;
  if (!(c % div == 0 || div % c == 0)) return 0;
  Term rest = 0;
  for (u32 j = 0; j < n_rest; j++) {
    if (!uop_term_nonneg(rest_terms[j])) return 0;
    rest = (rest == 0) ? rest_terms[j]
                       : uop_int_binary(UOP_IADD, rest, rest_terms[j]);
  }
  if (rest == 0) rest = uop_iconst(0);
  if (c % div != 0) {
    if (!uop_term_strictly_below(rest, c)) return 0;
  }
  *rest_out = rest;
  return 1;
}

// 1 iff `n` = c*x + rest for an addend c*x at any IADD depth (composed movement
// views nest deep), with c | div or div | c and uop_affine_rest_ok's condition.
static int uop_match_affine_numerator_deep(Term n, i64 div,
                                            i64 *c_out, Term *x_out,
                                            Term *rest_out) {
  if (div <= 0) return 0;
  // The shallow form first: cheap and common.
  {
    i64 c; Term x, y;
    if (uop_match_affine_numerator(n, &c, &x, &y)
        && (c % div == 0 || div % c == 0)) {
      Term rest_terms[1]; u32 n_rest = 0;
      if (y != 0) rest_terms[n_rest++] = y;
      Term rest;
      if (uop_affine_rest_ok(c, div, rest_terms, n_rest, &rest)) {
        *c_out = c; *x_out = x; *rest_out = rest;
        return 1;
      }
    }
  }
  if (term_tag(n) != TAG_UOP || term_ext(n) != UOP_IADD) return 0;
  Term terms[16];
  u32  nt = 0;
  if (!uop_iadd_tree_collect_const_mul(n, terms, &nt, 16, 0)) return 0;
  if (nt < 2) return 0;
  for (u32 i = 0; i < nt; i++) {
    i64 c; Term x;
    if (!uop_match_const_mul(terms[i], &c, &x) || c <= 0) continue;
    if (!(c % div == 0 || div % c == 0)) continue;
    Term rest_terms[16];
    u32  n_rest = 0;
    for (u32 j = 0; j < nt; j++) if (j != i) rest_terms[n_rest++] = terms[j];
    Term rest;
    if (!uop_affine_rest_ok(c, div, rest_terms, n_rest, &rest)) continue;
    *c_out = c; *x_out = x; *rest_out = rest;
    return 1;
  }
  return 0;
}

// 1 iff `t` is q*c or c*q with c CONST: *q_out = q, *c_out = c.
static int uop_match_mul_const(Term t, Term *q_out, i64 *c_out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  i64 v;
  if (uop_iconst_value(b, &v)) { *q_out = a; *c_out = v; return 1; }
  if (uop_iconst_value(a, &v)) { *q_out = b; *c_out = v; return 1; }
  return 0;
}

// 1 iff addend `u` is base % div (mul 1) or (base % div) * mul, CONST div and
// mul: the head of the term loop in tinygrad's fold_add_divmod_recombine.
static int uop_match_recombine_mod_term(Term u, Term *base_out, i64 *div_out,
                                        i64 *mul_out) {
  if (term_tag(u) != TAG_UOP) return 0;
  if (term_ext(u) == UOP_IMOD) {
    Term div_c = heap_read(term_val(u) + 1);
    i64 div;
    if (!uop_iconst_value(div_c, &div)) return 0;
    *base_out = heap_read(term_val(u) + 0);
    *div_out = div;
    *mul_out = 1;
    return 1;
  }
  Term m; i64 mul;
  if (uop_match_mul_const(u, &m, &mul)
      && term_tag(m) == TAG_UOP && term_ext(m) == UOP_IMOD) {
    Term div_c = heap_read(term_val(m) + 1);
    i64 div;
    if (!uop_iconst_value(div_c, &div)) return 0;
    *base_out = heap_read(term_val(m) + 0);
    *div_out = div;
    *mul_out = mul;
    return 1;
  }
  return 0;
}

// head + the terms[] but skip_i and skip_j, summed through uop_int_binary:
// tinygrad's head.usum(*rest).
static Term uop_recombine_rebuild(Term head, Term const *terms, u32 nt,
                                  u32 skip_i, u32 skip_j) {
  Term acc = head;
  for (u32 k = 0; k < nt; k++) {
    if (k == skip_i || k == skip_j) continue;
    acc = uop_int_binary(UOP_IADD, acc, terms[k]);
  }
  return acc;
}

// tinygrad uop/symbolic.py fold_add_divmod_recombine over every addend of a+b:
// a mod-term and a matching div-term recombine (the three cases below); else 0.
static Term uop_fold_add_divmod_recombine(Term a, Term b) {
  // Walk a and b, not uop_int_binary(IADD, a, b): that re-enters this rule.
  Term terms[32];
  u32  nt = 0;
  if (!uop_iadd_tree_collect_const_mul(a, terms, &nt, 32, 0)) return 0;
  if (!uop_iadd_tree_collect_const_mul(b, terms, &nt, 32, 0)) return 0;
  if (nt < 2) return 0;
  for (u32 i = 0; i < nt; i++) {
    Term base; i64 div, mul;
    if (!uop_match_recombine_mod_term(terms[i], &base, &div, &mul)) continue;
    if (div <= 0) continue;
    i64 want = div * mul;
    for (u32 j = 0; j < nt; j++) {
      if (i == j) continue;
      Term q; i64 vc;
      if (!uop_match_mul_const(terms[j], &q, &vc) || vc != want) continue;
      int exact = 0;
      // Case 1: (base%div)*mul + (base//div)*(div*mul) -> base*mul.
      if (term_tag(q) == TAG_UOP && term_ext(q) == UOP_IDIV) {
        Term q_div = heap_read(term_val(q) + 1);
        i64 qd;
        if (uop_iconst_value(q_div, &qd) && qd == div
            && heap_read(term_val(q) + 0) == base) {
          exact = 1;
        }
      }
      // Case 2, `base` itself inner//d:
      // ((inner//d)%div)*mul + (inner//(d*div))*(div*mul) -> (inner//d)*mul.
      if (!exact && term_tag(base) == TAG_UOP && term_ext(base) == UOP_IDIV) {
        Term base_div = heap_read(term_val(base) + 1);
        i64 bd;
        if (uop_iconst_value(base_div, &bd)
            && term_tag(q) == TAG_UOP && term_ext(q) == UOP_IDIV) {
          Term q_div = heap_read(term_val(q) + 1);
          i64 qd;
          if (uop_iconst_value(q_div, &qd) && qd == bd * div
              && heap_read(term_val(q) + 0) == heap_read(term_val(base) + 0)) {
            exact = 1;
          }
        }
      }
      if (exact) {
        Term head = uop_int_binary(UOP_IMUL, base, uop_iconst(mul));
        return uop_recombine_rebuild(head, terms, nt, i, j);
      }
      // Case 3, mul 1: ((base//div)%e)*div + base%div -> base%(div*e), with
      // q = (base//div)%e.
      if (mul == 1 && term_tag(q) == TAG_UOP && term_ext(q) == UOP_IMOD) {
        Term q_e = heap_read(term_val(q) + 1);
        Term q_inner = heap_read(term_val(q) + 0);
        i64 e;
        if (uop_iconst_value(q_e, &e) && e > 0
            && term_tag(q_inner) == TAG_UOP && term_ext(q_inner) == UOP_IDIV
            && heap_read(term_val(q_inner) + 0) == base) {
          Term qi_div = heap_read(term_val(q_inner) + 1);
          i64 qid;
          if (uop_iconst_value(qi_div, &qid) && qid == div) {
            Term head = uop_int_binary(UOP_IMOD, base, uop_iconst(div * e));
            return uop_recombine_rebuild(head, terms, nt, i, j);
          }
        }
      }
    }
  }
  return 0;
}

// 1 iff `t` is the INVALID sentinel.
static int uop_is_invalid(Term t) {
  return term_tag(t) == TAG_UOP && term_ext(t) == UOP_INVALID;
}

fn Term uop_simplify_int_binary(u32 opcode, Term a, Term b) {
  i64 av, bv;
  int a_const = uop_iconst_value(a, &av);
  int b_const = uop_iconst_value(b, &bv);

  if (a_const && b_const) {
    switch (opcode) {
      case UOP_IADD: return uop_iconst(av + bv);
      case UOP_ISUB: return uop_iconst(av - bv);
      case UOP_IMUL: return uop_iconst(av * bv);
      case UOP_IDIV:
        if (bv == 0) return 0;
        return uop_iconst(av / bv);
      case UOP_IMOD:
        if (bv == 0) return 0;
        return uop_iconst(av % bv);
      case UOP_ILT:  return uop_iconst(av < bv ? 1 : 0);
      case UOP_IAND: return uop_iconst(av & bv);
      case UOP_IOR:  return uop_iconst(av | bv);
      case UOP_IXOR: return uop_iconst(av ^ bv);
      case UOP_ISHR: return uop_iconst((i64)((i32)av >> (i32)bv));
      default: break;
    }
  }

  switch (opcode) {
    case UOP_IADD:
      if (a_const && av == 0) return b;
      if (b_const && bv == 0) return a;
      // (x + c1) + c2 -> x + (c1 + c2); (x - c1) + c2 -> x + (c2 - c1).
      if (b_const && term_tag(a) == TAG_UOP) {
        u32 inner_op = term_ext(a);
        if (inner_op == UOP_IADD) {
          Term lhs = heap_read(term_val(a) + 0);
          Term rhs = heap_read(term_val(a) + 1);
          i64 inner_c;
          if (uop_iconst_value(rhs, &inner_c)) {
            return uop_int_binary(UOP_IADD, lhs, uop_iconst(inner_c + bv));
          }
          if (uop_iconst_value(lhs, &inner_c)) {
            return uop_int_binary(UOP_IADD, rhs, uop_iconst(inner_c + bv));
          }
        }
        if (inner_op == UOP_ISUB) {
          Term lhs = heap_read(term_val(a) + 0);
          Term rhs = heap_read(term_val(a) + 1);
          i64 inner_c;
          if (uop_iconst_value(rhs, &inner_c)) {
            return uop_int_binary(UOP_IADD, lhs, uop_iconst(bv - inner_c));
          }
        }
      }
      // (c1*x) + (c2*x) -> (c1+c2)*x (tinygrad uop/symbolic.py:240).
      {
        i64 c1, c2;
        Term x1, x2;
        if (uop_match_const_mul(a, &c1, &x1)
            && uop_match_const_mul(b, &c2, &x2)
            && x1 == x2) {
          return uop_int_binary(UOP_IMUL, x1, uop_iconst(c1 + c2));
        }
        // x + (c*x) -> (c+1)*x, either order.
        if (uop_match_const_mul(b, &c1, &x1) && a == x1) {
          return uop_int_binary(UOP_IMUL, x1, uop_iconst(c1 + 1));
        }
        if (uop_match_const_mul(a, &c1, &x1) && b == x1) {
          return uop_int_binary(UOP_IMUL, x1, uop_iconst(c1 + 1));
        }
        // x + x -> x*2 (tinygrad uop/symbolic.py:245).
        if (a == b) return uop_int_binary(UOP_IMUL, a, uop_iconst(2));
      }
      // (y + x*c0) + x*c1 -> y + x*(c0+c1), c0 or c1 possibly an implicit 1
      // (tinygrad uop/symbolic.py:241,243,244,246).
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_IADD) {
        Term y = heap_read(term_val(a) + 0);
        Term t1 = heap_read(term_val(a) + 1);
        i64 c0 = 1, c1b = 1;
        Term x0 = 0, x1b = 0;
        int m0 = uop_match_const_mul(t1, &c0, &x0);
        int m1 = uop_match_const_mul(b, &c1b, &x1b);
        if (!m0) { x0 = t1; c0 = 1; }
        if (!m1) { x1b = b; c1b = 1; }
        if (x0 == x1b) {
          Term combined = uop_int_binary(UOP_IMUL, x0, uop_iconst(c0 + c1b));
          return uop_int_binary(UOP_IADD, y, combined);
        }
      }
      // (x + c1) + y -> (x + y) + c1 for a non-CONST y; with the CONST
      // outermost this does not re-fire (tinygrad uop/symbolic.py:293).
      if (!b_const && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IADD) {
        Term lhs = heap_read(term_val(a) + 0);
        Term rhs = heap_read(term_val(a) + 1);
        i64 ic;
        if (uop_iconst_value(rhs, &ic) && !uop_iconst_value(lhs, &ic)) {
          return uop_int_binary(UOP_IADD, uop_int_binary(UOP_IADD, lhs, b), rhs);
        }
      }
      // Divmod recombination over the whole IADD tree, (r//M)*M + r%M -> r too.
      {
        Term r = uop_fold_add_divmod_recombine(a, b);
        if (r != 0) return r;
      }
      break;
    case UOP_ISUB:
      if (b_const && bv == 0) return a;
      if (a == b)             return uop_iconst(0);
      // (x + c1) - c2 -> x + (c1 - c2); (x - c1) - c2 -> x - (c1 + c2).
      if (b_const && term_tag(a) == TAG_UOP) {
        u32 inner_op = term_ext(a);
        if (inner_op == UOP_IADD) {
          Term lhs = heap_read(term_val(a) + 0);
          Term rhs = heap_read(term_val(a) + 1);
          i64 inner_c;
          if (uop_iconst_value(rhs, &inner_c)) {
            return uop_int_binary(UOP_IADD, lhs, uop_iconst(inner_c - bv));
          }
          if (uop_iconst_value(lhs, &inner_c)) {
            return uop_int_binary(UOP_IADD, rhs, uop_iconst(inner_c - bv));
          }
        }
        if (inner_op == UOP_ISUB) {
          Term lhs = heap_read(term_val(a) + 0);
          Term rhs = heap_read(term_val(a) + 1);
          i64 inner_c;
          if (uop_iconst_value(rhs, &inner_c)) {
            return uop_int_binary(UOP_ISUB, lhs, uop_iconst(inner_c + bv));
          }
        }
      }
      break;
    case UOP_IMUL:
      if (a_const && av == 0) return uop_iconst(0);
      if (b_const && bv == 0) return uop_iconst(0);
      if (a_const && av == 1) return b;
      if (b_const && bv == 1) return a;
      // (x * c1) * c2 -> x * (c1 * c2).
      if (b_const && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IMUL) {
        i64 inner_c; Term xnum;
        if (uop_match_const_mul(a, &inner_c, &xnum)) {
          return uop_int_binary(UOP_IMUL, xnum, uop_iconst(inner_c * bv));
        }
      }
      // (x*c1)*y -> (x*y)*c1 for a non-CONST y (tinygrad uop/symbolic.py:294).
      if (!a_const && !b_const) {
        i64 c1; Term xnum, y;
        if (uop_match_const_mul(a, &c1, &xnum)) { y = b; }
        else if (uop_match_const_mul(b, &c1, &xnum)) { y = a; }
        else { c1 = 0; xnum = 0; y = 0; }
        if (xnum != 0) {
          return uop_int_binary(UOP_IMUL,
                   uop_int_binary(UOP_IMUL, xnum, y), uop_iconst(c1));
        }
      }
      break;
    case UOP_IDIV:
      if (b_const && bv == 1) return a;
      if (a_const && av == 0) return uop_iconst(0);
      if (a == b && b_const && bv != 0) return uop_iconst(1);
      // cancel_divmod (tinygrad uop/divandmod.py:12): x // c -> lo/c for x >= 0
      // with lo/c == hi/c; IWHERE-blind, so a masked numerator keeps its gate.
      if (b_const && bv > 0 && !a_const) {
        i64 lo, hi;
        if (uop_term_nonneg(a) && uop_int_bounds(a, &lo, &hi) && lo / bv == hi / bv) {
          return uop_iconst(lo / bv);
        }
      }
      // (c*x + y) / c -> x for y in [0, c), and (c*x) / c -> x.
      if (b_const && bv > 0) {
        i64 c;
        Term x, y;
        if (uop_match_affine_numerator(a, &c, &x, &y) && c == bv) {
          if (y == 0 || uop_term_strictly_below(y, c)) return x;
        }
      }
      // An affine numerator at any IADD depth (uop_affine_rest_ok):
      // (m*d*x + rest) / d -> m*x + rest/d, (c*x + rest) / (n*c) -> x / n.
      if (b_const && bv > 0 && !a_const) {
        i64 c;
        Term x, rest;
        if (uop_match_affine_numerator_deep(a, bv, &c, &x, &rest)) {
          if (c % bv == 0) {
            Term term_x = uop_int_binary(UOP_IMUL, x, uop_iconst(c / bv));
            Term term_r = uop_int_binary(UOP_IDIV, rest, b);
            return uop_int_binary(UOP_IADD, term_x, term_r);
          }
          if (bv % c == 0) {
            return uop_int_binary(UOP_IDIV, x, uop_iconst(bv / c));
          }
        }
      }
      // (c1*x) // (c1*k) -> x // k for c1 > 0.
      if (a_const && b_const) break;
      if (b_const && bv > 0) {
        i64 c1;
        Term xnum;
        if (uop_match_const_mul(a, &c1, &xnum) && c1 > 0 && bv % c1 == 0) {
          return uop_int_binary(UOP_IDIV, xnum, uop_iconst(bv / c1));
        }
      }
      // (x // c1) // c2 -> x // (c1*c2) (tinygrad uop/symbolic.py:282) for
      // x >= 0: thvm's IDIV truncates, equal to floor only for x >= 0.
      if (b_const && bv > 0
          && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IDIV) {
        Term inner_num = heap_read(term_val(a) + 0);
        Term inner_den = heap_read(term_val(a) + 1);
        i64 c1;
        if (uop_iconst_value(inner_den, &c1) && c1 > 0
            && uop_term_nonneg(inner_num)) {
          return uop_int_binary(UOP_IDIV, inner_num, uop_iconst(c1 * bv));
        }
      }
      // (x//c + a)//d -> (x + a*c)//(c*d) (tinygrad uop/divandmod.py:101), for
      // x, a >= 0 under C truncation and no overflow in a*c or c*d.
      if (b_const && bv > 0
          && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IADD
          && uop_term_nonneg(a)) {
        Term lhs = heap_read(term_val(a) + 0);
        Term rhs = heap_read(term_val(a) + 1);
        Term inner_div = 0; i64 av_a = 0;
        if (uop_iconst_value(rhs, &av_a)
            && term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_IDIV) {
          inner_div = lhs;
        } else if (uop_iconst_value(lhs, &av_a)
            && term_tag(rhs) == TAG_UOP && term_ext(rhs) == UOP_IDIV) {
          inner_div = rhs;
        }
        if (inner_div != 0) {
          Term x = heap_read(term_val(inner_div) + 0);
          i64 cv;
          if (uop_iconst_value(heap_read(term_val(inner_div) + 1), &cv)
              && cv > 0) {
            i64 ac, cd;
            if (uop_chk_mul(av_a, cv, &ac) && uop_chk_mul(cv, bv, &cd)) {
              Term num = uop_int_binary(UOP_IADD, x, uop_iconst(ac));
              return uop_int_binary(UOP_IDIV, num, uop_iconst(cd));
            }
          }
        }
      }
      // (r % (k*c)) // c -> (r // c) % k (tinygrad uop/divandmod.py:24-25).
      if (b_const && bv > 0
          && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IMOD) {
        Term mod_a = heap_read(term_val(a) + 0);
        Term mod_b = heap_read(term_val(a) + 1);
        i64 kc;
        if (uop_iconst_value(mod_b, &kc) && kc > 0 && kc % bv == 0) {
          i64 k = kc / bv;
          Term inner_div = uop_int_binary(UOP_IDIV, mod_a, uop_iconst(bv));
          return uop_int_binary(UOP_IMOD, inner_div, uop_iconst(k));
        }
      }
      // (x + c) // d -> (x + c%d) // d + c/d for c >= d, x >= 0, so the integer
      // part merges with sibling constants (tinygrad uop/divandmod.py:102-105).
      if (b_const && bv > 0
          && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IADD) {
        Term lhs = heap_read(term_val(a) + 0);
        Term rhs = heap_read(term_val(a) + 1);
        Term var_t = 0; i64 c = 0;
        if (uop_iconst_value(rhs, &c) && c >= bv && uop_term_nonneg(lhs)) {
          var_t = lhs;
        } else if (uop_iconst_value(lhs, &c) && c >= bv && uop_term_nonneg(rhs)) {
          var_t = rhs;
        }
        if (var_t != 0) {
          i64 c_mod_d = c % bv;
          i64 c_div_d = c / bv;
          Term inner = uop_int_binary(UOP_IADD, var_t, uop_iconst(c_mod_d));
          Term inner_div = uop_int_binary(UOP_IDIV, inner, uop_iconst(bv));
          return uop_int_binary(UOP_IADD, inner_div, uop_iconst(c_div_d));
        }
      }
      break;
    case UOP_IMOD:
      if (b_const && bv == 1) return uop_iconst(0);
      if (a_const && av == 0) return uop_iconst(0);
      // x % x -> 0 (tinygrad uop/symbolic.py:140).
      if (a == b) return uop_iconst(0);
      // A RANGE with extent <= c never reaches c.
      if (b_const && bv > 0) {
        u32 ext;
        if (uop_range_extent_into(a, &ext) && (i64)ext <= bv) return a;
      }
      // cancel_divmod as for IDIV: x % c -> x - (lo/c)*c (tinygrad
      // uop/divandmod.py:12).
      if (b_const && bv > 0 && !a_const) {
        i64 lo, hi, qc;
        if (uop_term_nonneg(a) && uop_int_bounds(a, &lo, &hi) && lo / bv == hi / bv
            && uop_chk_mul(lo / bv, bv, &qc)) {
          return qc == 0 ? a : uop_int_binary(UOP_ISUB, a, uop_iconst(qc));
        }
      }
      // (c*x + y) % c -> y for y in [0, c), and (c*x) % c -> 0.
      if (b_const && bv > 0) {
        i64 c;
        Term x, y;
        if (uop_match_affine_numerator(a, &c, &x, &y) && c == bv) {
          (void)x;
          if (y == 0)                                  return uop_iconst(0);
          if (uop_term_strictly_below(y, c))           return y;
        }
      }
      // At any IADD depth (uop_affine_rest_ok): (m*d*x + rest) % d -> rest % d,
      // and (c*x + rest) % (n*c) -> c*(x % n) + rest.
      if (b_const && bv > 0 && !a_const) {
        i64 c;
        Term x, rest;
        if (uop_match_affine_numerator_deep(a, bv, &c, &x, &rest)) {
          if (c % bv == 0) {
            return uop_int_binary(UOP_IMOD, rest, b);
          }
          if (bv % c == 0) {
            Term xm = uop_int_binary(UOP_IMOD, x, uop_iconst(bv / c));
            Term cm = uop_int_binary(UOP_IMUL, xm, uop_iconst(c));
            return uop_int_binary(UOP_IADD, cm, rest);
          }
        }
      }
      // (... + c*x + ...) % m -> (... + (c%m)*x + ...) % m for c >= m (exact
      // for non-negative terms); the smaller residue often folds further.
      if (b_const && bv > 1 && !a_const && term_tag(a) == TAG_UOP) {
        Term terms[16];
        u32  nt = 0;
        if ((term_ext(a) == UOP_IADD || term_ext(a) == UOP_IMUL)
            && uop_iadd_tree_collect_const_mul(a, terms, &nt, 16, 0)
            && nt >= 1) {
          int any_reduced = 0, all_nonneg = 1;
          Term rebuilt = 0;
          for (u32 i = 0; i < nt; i++) {
            i64 c; Term x;
            Term term_i = terms[i];
            if (!uop_term_nonneg(term_i)) { all_nonneg = 0; break; }
            if (uop_match_const_mul(term_i, &c, &x) && c >= bv && c % bv != 0) {
              term_i = uop_int_binary(UOP_IMUL, x, uop_iconst(c % bv));
              any_reduced = 1;
            }
            rebuilt = (rebuilt == 0) ? term_i
                                     : uop_int_binary(UOP_IADD, rebuilt, term_i);
          }
          if (all_nonneg && any_reduced && rebuilt != 0) {
            return uop_int_binary(UOP_IMOD, rebuilt, b);
          }
        }
      }
      // remove_nested_mod (tinygrad uop/divandmod.py:27-35):
      // (a%(k*c) + b) % c -> (a + b) % c, all addends >= 0 as C `%` truncates.
      if (b_const && bv > 0 && !a_const && term_tag(a) == TAG_UOP
          && term_ext(a) == UOP_IADD) {
        Term terms[16];
        u32  nt = 0;
        if (uop_iadd_tree_collect_const_mul(a, terms, &nt, 16, 0) && nt >= 1) {
          int any_stripped = 0, all_nonneg = 1;
          Term rebuilt = 0;
          for (u32 i = 0; i < nt; i++) {
            Term term_i = terms[i];
            if (!uop_term_nonneg(term_i)) { all_nonneg = 0; break; }
            if (term_tag(term_i) == TAG_UOP && term_ext(term_i) == UOP_IMOD) {
              i64 kc;
              if (uop_iconst_value(heap_read(term_val(term_i) + 1), &kc)
                  && kc > 0 && kc % bv == 0) {
                term_i = heap_read(term_val(term_i) + 0);
                any_stripped = 1;
              }
            }
            rebuilt = (rebuilt == 0) ? term_i
                                     : uop_int_binary(UOP_IADD, rebuilt, term_i);
          }
          if (all_nonneg && any_stripped && rebuilt != 0) {
            return uop_int_binary(UOP_IMOD, rebuilt, b);
          }
        }
      }
      // (r % (k*c)) % c -> r % c, remove_nested_mod with a single addend.
      if (b_const && bv > 0
          && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IMOD) {
        Term mod_a = heap_read(term_val(a) + 0);
        Term mod_b = heap_read(term_val(a) + 1);
        i64 kc;
        if (uop_iconst_value(mod_b, &kc) && kc > 0 && kc % bv == 0) {
          return uop_int_binary(UOP_IMOD, mod_a, uop_iconst(bv));
        }
      }
      break;
    case UOP_ILT:
      if (a == b) return uop_iconst(0);
      // RANGE < c is 1 for an extent of at most c.
      if (b_const && bv > 0) {
        u32 ext;
        if (uop_range_extent_into(a, &ext) && (i64)ext <= bv) return uop_iconst(1);
      }
      // RANGE < c is 0 for c <= 0, the lower check of a PAD mask.
      if (b_const && bv <= 0) {
        u32 ext;
        if (uop_range_extent_into(a, &ext)) return uop_iconst(0);
      }
      // a < c, or c < a, is constant when the bounds of a decide it (tinygrad
      // uop/symbolic.py:268-270, min == max).
      if (b_const && !a_const) {
        i64 lo, hi;
        if (uop_int_bounds(a, &lo, &hi)) {
          if (hi < bv)  return uop_iconst(1);
          if (lo >= bv) return uop_iconst(0);
        }
      }
      if (a_const && !b_const) {
        i64 lo, hi;
        if (uop_int_bounds(b, &lo, &hi)) {
          if (lo > av)  return uop_iconst(1);
          if (hi <= av) return uop_iconst(0);
        }
      }
      // (c0 + x) < c1 -> x < c1 - c0; (x - c0) < c1 -> x < c1 + c0 (tinygrad
      // uop/symbolic.py:285).
      if (b_const && term_tag(a) == TAG_UOP) {
        if (term_ext(a) == UOP_IADD) {
          Term lhs = heap_read(term_val(a) + 0);
          Term rhs = heap_read(term_val(a) + 1);
          i64 c0; Term x = 0;
          if (uop_iconst_value(lhs, &c0))      x = rhs;
          else if (uop_iconst_value(rhs, &c0)) x = lhs;
          i64 nc;
          if (x != 0 && uop_chk_sub(bv, c0, &nc))
            return uop_int_binary(UOP_ILT, x, uop_iconst(nc));
        }
        if (term_ext(a) == UOP_ISUB) {
          Term lhs = heap_read(term_val(a) + 0);
          i64 c0;
          if (uop_iconst_value(heap_read(term_val(a) + 1), &c0)) {
            i64 nc;
            if (uop_chk_add(bv, c0, &nc))
              return uop_int_binary(UOP_ILT, lhs, uop_iconst(nc));
          }
        }
      }
      // (c0*x) < c1 -> sign(c0)*x < ceil(c1/|c0|) for |c0| > 1 (tinygrad
      // uop/symbolic.py:286-288).  Exact for unbounded ints, as its weakint
      // gate assumes; an int32 c0*x that wraps compares differently.
      if (b_const) {
        i64 c0; Term x;
        if (uop_match_const_mul(a, &c0, &x) && (c0 > 1 || c0 < -1)) {
          i64  q   = -uop_floordiv_i64(-bv, c0 > 0 ? c0 : -c0);
          Term lhs = c0 > 0 ? x : uop_int_binary(UOP_IMUL, x, uop_iconst(-1));
          return uop_int_binary(UOP_ILT, lhs, uop_iconst(q));
        }
      }
      // (x // d) < c -> x < c*d for d > 0, c*d < x for d < 0 (tinygrad
      // uop/symbolic.py:289-291).  IDIV truncates, equal to floor for x >= 0 if
      // d > 0 and for x <= 0 or d == -1 if d < 0; c*d must fit int32.
      if (b_const && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IDIV) {
        Term inner_num = heap_read(term_val(a) + 0);
        i64 d, cd, lo, hi;
        if (uop_iconst_value(heap_read(term_val(a) + 1), &d) && uop_chk_mul(bv, d, &cd)
            && cd >= INT32_MIN && cd <= INT32_MAX) {
          if (d > 0 && uop_term_nonneg(inner_num)) {
            return uop_int_binary(UOP_ILT, inner_num, uop_iconst(cd));
          }
          if (d < 0 && (d == -1 || (uop_int_bounds(inner_num, &lo, &hi) && hi <= 0))) {
            return uop_int_binary(UOP_ILT, uop_iconst(cd), inner_num);
          }
        }
      }
      // x*-1 < y*-1 -> y < x (tinygrad uop/symbolic.py:298).
      {
        i64 ca, cb; Term xa, yb;
        if (uop_match_const_mul(a, &ca, &xa) && ca == -1
            && uop_match_const_mul(b, &cb, &yb) && cb == -1) {
          return uop_int_binary(UOP_ILT, yb, xa);
        }
      }
      // lt_folding (tinygrad uop/symbolic.py:203-207,297): x < c over an IADD
      // tree with g = gcd(c, non-unit coefficients) > 1 and the unit terms in
      // [0, g) -> (non-unit terms / g) < c/g.
      if (b_const && bv > 0 && !a_const && term_tag(a) == TAG_UOP
          && term_ext(a) == UOP_IADD) {
        Term terms[16];
        u32  nt = 0;
        if (uop_iadd_tree_collect_const_mul(a, terms, &nt, 16, 0) && nt >= 2) {
          i64 g = bv;
          int have_np = 0, ok = 1;
          i64 p_lo_sum = 0, p_hi_sum = 0;
          for (u32 i = 0; i < nt && ok; i++) {
            i64 c; Term x;
            if (uop_match_const_mul(terms[i], &c, &x) && c != 1 && c != -1) {
              g = uop_gcd_i64(g, c);
              have_np = 1;
            } else {
              i64 lo, hi;
              if (!uop_int_bounds(terms[i], &lo, &hi)) { ok = 0; break; }
              p_lo_sum += lo;
              p_hi_sum += hi;
            }
          }
          if (ok && have_np && g > 1 && p_lo_sum >= 0 && p_hi_sum < g) {
            Term np_sum = 0;
            for (u32 i = 0; i < nt; i++) {
              i64 c; Term x;
              if (uop_match_const_mul(terms[i], &c, &x) && c != 1 && c != -1) {
                Term reduced = uop_int_binary(UOP_IMUL, x, uop_iconst(c / g));
                np_sum = (np_sum == 0) ? reduced
                                       : uop_int_binary(UOP_IADD, np_sum, reduced);
              }
            }
            if (np_sum != 0)
              return uop_int_binary(UOP_ILT, np_sum, uop_iconst(bv / g));
          }
        }
      }
      break;
    case UOP_IAND:
      if (a_const && av == 0) return uop_iconst(0);
      if (b_const && bv == 0) return uop_iconst(0);
      // Treat 1 as boolean-true on bool/cond inputs.
      if (a_const && av == 1) return b;
      if (b_const && bv == 1) return a;
      if (a == b)             return a;
      // (x & c1) & c2 -> x & (c1 & c2) (tinygrad uop/symbolic.py:279-280).
      if (b_const && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IAND) {
        Term inner_x = heap_read(term_val(a) + 0);
        i64 inner_c;
        if (uop_iconst_value(heap_read(term_val(a) + 1), &inner_c))
          return uop_int_binary(UOP_IAND, inner_x, uop_iconst(inner_c & bv));
      }
      break;
    case UOP_IOR:
      // x | c -> c if c else x (tinygrad uop/symbolic.py:128, bool only): like
      // x & 1 -> x above, it treats an operand combined with a const as 0/1.
      if (a_const) return av ? a : b;
      if (b_const) return bv ? b : a;
      if (a == b)  return a;
      // (x | c1) | c2 -> x | (c1 | c2) (tinygrad uop/symbolic.py:279-280).
      if (b_const && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IOR) {
        Term inner_x = heap_read(term_val(a) + 0);
        i64 inner_c;
        if (uop_iconst_value(heap_read(term_val(a) + 1), &inner_c))
          return uop_int_binary(UOP_IOR, inner_x, uop_iconst(inner_c | bv));
      }
      break;
    case UOP_IXOR:
      // x ^ 0 -> x (tinygrad uop/symbolic.py:117).
      if (a_const && av == 0) return b;
      if (b_const && bv == 0) return a;
      // x ^ x -> 0 (tinygrad uop/symbolic.py:141).
      if (a == b) return uop_iconst(0);
      // (x ^ y) ^ y -> x (tinygrad uop/symbolic.py:123).
      if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_IXOR) {
        Term xa = heap_read(term_val(a) + 0);
        Term ya = heap_read(term_val(a) + 1);
        if (ya == b) return xa;
        if (xa == b) return ya;
      }
      if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_IXOR) {
        Term xb = heap_read(term_val(b) + 0);
        Term yb = heap_read(term_val(b) + 1);
        if (yb == a) return xb;
        if (xb == a) return yb;
      }
      break;
    default: break;
  }
  return 0;
}

// Defined in uop/substitute.c, later in the translation unit.
fn Term uop_substitute(Term root, Term const *from, Term const *to, u32 n);

// Bits returned by uop_index_scan.
#define UOP_SCAN_FOUND   1u  // one of the `find` terms occurs
#define UOP_SCAN_GAVE_UP 2u  // too many visits or too deep
#define UOP_SCAN_OPAQUE  4u  // a node outside the index layer (INDEX_E, LOAD, ...)
#define UOP_SCAN_STACK   256

// UOP_SCAN_* bits for the DAG under `root`, descending the I-ops and IWHERE.
// No visited set, so a shared subtree is walked per reference: `budget` caps
// the visits, the stack the depth, and UOP_SCAN_GAVE_UP means unknown.
static u32 uop_index_scan(Term root, Term const *find, u32 nfind, u32 budget) {
  Term stack[UOP_SCAN_STACK];
  u32  sp = 0, visits = 0, out = 0;
  stack[sp++] = root;
  while (sp > 0) {
    Term t = stack[--sp];
    if (++visits > budget) {
      return out | UOP_SCAN_GAVE_UP;
    }
    int hit = 0;
    for (u32 i = 0; i < nfind; i++) {
      hit |= t == find[i];
    }
    if (hit) {
      out |= UOP_SCAN_FOUND;
      continue;
    }
    if (term_tag(t) != TAG_UOP) {
      if (term_tag(t) != TAG_NUM) {
        out |= UOP_SCAN_OPAQUE;
      }
      continue;
    }
    u32 n = 0;
    switch (term_ext(t)) {
      case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
      case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
      case UOP_IXOR: case UOP_ISHR: n = 2; break;
      case UOP_IWHERE:              n = 3; break;
      case UOP_RANGE: case UOP_CONST: case UOP_INVALID: break;
      default: out |= UOP_SCAN_OPAQUE; break;
    }
    if (sp + n > UOP_SCAN_STACK) {
      return out | UOP_SCAN_GAVE_UP;
    }
    for (u32 i = 0; i < n; i++) {
      stack[sp++] = heap_read(term_val(t) + i);
    }
  }
  return out;
}

// fast_idiv (tinygrad codegen/decomp/op.py:20-34, no widening cast): x // c ->
// (x * m) >> s and x % c -> x - c*(x // c) for a bounded x >= 0.  Only a late
// rewrite (op.py:97-103, codegen/__init__.py:360): index folds want IDIV/IMOD.

// x // d for 0 <= x <= hi, d > 1, as (x * m) >> s; 0 when m*hi overflows int32.
// An even d retries as (x >> k) // (d >> k), k = ctz(d), whose smaller
// numerator may fit (op.py:26-27); d >> k is odd, so that recursion ends.
static Term uop_fast_idiv_div(Term x, i64 hi, i64 d) {
  i64 m, s;
  if (thvm_magicgu(hi, d, &m, &s) && m >= 1 && m <= 0x7fffffff
      && hi <= 0x7fffffff / m) {
    Term mul = uop_int_binary(UOP_IMUL, x, uop_iconst(m));
    return uop_int_binary(UOP_ISHR, mul, uop_iconst(s));
  }
  i32 k = __builtin_ctzll((unsigned long long)d);
  if (k == 0) {
    return 0;
  }
  return uop_fast_idiv_div(uop_int_binary(UOP_ISHR, x, uop_iconst(k)), hi >> k, d >> k);
}

// The lowered IDIV/IMOD `node`, or 0 when a guard fails.  Run bottom-up by
// uop_graph_rewrite, so a numerator's own div/mod is an ISHR already, which
// uop_int_bounds still bounds.
static Term uop_fast_idiv_node(Term node) {
  if (term_tag(node) != TAG_UOP) return 0;
  u32 op = term_ext(node);
  if (op != UOP_IDIV && op != UOP_IMOD) return 0;
  Term num = heap_read(term_val(node) + 0);
  Term den = heap_read(term_val(node) + 1);
  i64 bv;
  // Powers of two are cheap already, and a signed >> is no truncating divide.
  if (!uop_iconst_value(den, &bv) || bv <= 1 || (bv & (bv - 1)) == 0) return 0;
  i64 lo, hi;
  if (!uop_int_bounds(num, &lo, &hi) || lo < 0 || hi < 0) return 0;
  // x <= hi < c: x//c is 0 and x%c is x (op.py:22).  magicgu would return
  // m = 1, s = 0 here, lowering x//c to x.
  if (hi < bv) return (op == UOP_IDIV) ? uop_iconst(0) : num;
  Term div = uop_fast_idiv_div(num, hi, bv);
  if (div == 0) {
    return 0;
  }
  if (op == UOP_IDIV) return div;
  // x % c -> x - c*(x // c); reuses the lowered div so no IDIV remains.
  Term cm = uop_int_binary(UOP_IMUL, div, den);
  return uop_int_binary(UOP_ISUB, num, cm);
}

// The uop_graph_rewrite rule.  THVM_NO_FAST_IDIV, set nonzero, keeps the
// literal IDIV/IMOD, like tinygrad's DISABLE_FAST_IDIV (helpers.py:249).
fn Term uop_fast_idiv_rule(Term t, void *user) {
  (void)user;
  static int gate = -1;
  if (gate < 0) {
    char const *e = getenv("THVM_NO_FAST_IDIV");
    gate = e != NULL && atoi(e) != 0;
  }
  if (gate) return 0;
  return uop_fast_idiv_node(t);
}

#define UOP_CLOSURE_SCAN_BUDGET 256

// 1 iff `t` is an index leaf, which cannot contain a gate's cond.
static int uop_is_index_leaf(Term t) {
  if (term_tag(t) != TAG_UOP) {
    return term_tag(t) == TAG_NUM;
  }
  switch (term_ext(t)) {
    case UOP_RANGE: case UOP_CONST: case UOP_INVALID: return 1;
    default: return 0;
  }
}

// fold_where_closure (tinygrad uop/symbolic.py:228-233): cond is 1 inside t (an
// int cond in [0, 1]) and 0 inside f.  A gate over non-index nodes is left as
// tinygrad leaves INDEX gates; scans are bounded, as every IWHERE comes here.
static Term uop_fold_where_closure(Term cond, Term t, Term f) {
  if (term_tag(cond) != TAG_UOP) {
    return 0;
  }
  switch (term_ext(cond)) {
    case UOP_ILT: case UOP_IAND: case UOP_IOR: break;
    default: return 0;
  }
  int t_leaf = uop_is_index_leaf(t);
  int f_leaf = uop_is_index_leaf(f);
  if (t_leaf && f_leaf) {
    return 0;
  }
  if (uop_index_scan(cond, NULL, 0, UOP_CLOSURE_SCAN_BUDGET) != 0) {
    return 0;
  }
  u32 ts = t_leaf ? 0 : uop_index_scan(t, &cond, 1, UOP_CLOSURE_SCAN_BUDGET);
  u32 fs = f_leaf ? 0 : uop_index_scan(f, &cond, 1, UOP_CLOSURE_SCAN_BUDGET);
  if ((ts | fs) & (UOP_SCAN_GAVE_UP | UOP_SCAN_OPAQUE)) {
    return 0;
  }
  Term nt = t, nf = f;
  i64  lo, hi;
  if ((ts & UOP_SCAN_FOUND) && uop_int_bounds(cond, &lo, &hi) && lo >= 0 && hi <= 1) {
    Term one = uop_iconst(1);
    nt = uop_substitute(t, &cond, &one, 1);
  }
  if (fs & UOP_SCAN_FOUND) {
    Term zero = uop_iconst(0);
    nf = uop_substitute(f, &cond, &zero, 1);
  }
  if (nt == t && nf == f) {
    return 0;
  }
  return uop_iwhere(cond, nt, nf);
}

static Term uop_simplify_iwhere_body(Term cond, Term then_v, Term else_v) {
  i64 cv;
  if (uop_iconst_value(cond, &cv)) {
    return cv != 0 ? then_v : else_v;
  }
  if (then_v == else_v) return then_v;
  // The closure fold runs before the gate merges, so a merged gate repeats no
  // conjunct (tinygrad uop/symbolic.py:253-254).
  {
    Term r = uop_fold_where_closure(cond, then_v, else_v);
    if (r != 0) return r;
  }
  // a.where(b.where(c, d), d) -> (a & b).where(c, d), the PAD-mask shape
  // (tinygrad uop/symbolic.py:256-257).
  if (term_tag(then_v) == TAG_UOP && term_ext(then_v) == UOP_IWHERE) {
    Term inner_cond = heap_read(term_val(then_v) + 0);
    Term inner_then = heap_read(term_val(then_v) + 1);
    Term inner_else = heap_read(term_val(then_v) + 2);
    if (inner_else == else_v) {
      Term combined = uop_int_binary(UOP_IAND, cond, inner_cond);
      return uop_iwhere(combined, inner_then, else_v);
    }
  }
  // a.where(c, b.where(c, d)) -> (a | b).where(c, d) (tinygrad
  // uop/symbolic.py:258-259); bitwise a | b is nonzero iff a or b is.
  if (term_tag(else_v) == TAG_UOP && term_ext(else_v) == UOP_IWHERE
      && heap_read(term_val(else_v) + 1) == then_v) {
    Term inner_cond = heap_read(term_val(else_v) + 0);
    Term inner_else = heap_read(term_val(else_v) + 2);
    return uop_iwhere(uop_int_binary(UOP_IOR, cond, inner_cond), then_v, inner_else);
  }
  return 0;
}

// The body re-enters through uop_iwhere; past depth 96 an IWHERE is left
// unfolded, which is correct, only less simplified.
fn Term uop_simplify_iwhere(Term cond, Term then_v, Term else_v) {
  static _Thread_local u32 depth = 0;
  if (depth > 96) return 0;
  depth++;
  Term r = uop_simplify_iwhere_body(cond, then_v, else_v);
  depth--;
  return r;
}
