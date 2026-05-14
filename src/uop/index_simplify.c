// uop/index_simplify.c - constructor-time simplifier for the symbolic
// INDEX layer.
//
// Folds applied in `uop_int_binary` and `uop_iwhere` before hash-cons.
// Without these, the resolver builds giant ASTs of identity ops
// (`x + 0`, `x * 1`, `(x // 1) % d`, etc.) that the downstream MSL
// compiler can't simplify.  Mirrors tinygrad's symbolic.py rules.
//
// Integration: each rule returns a non-zero Term when it fires;
// callers (uop_int_binary, uop_iwhere) skip hash-cons for that
// branch and return the folded term directly.  Identical to the
// uop_rewrite_binary pattern for float ops.

// === Inspection helpers ===

// Read a CONST's integer value (DT_INT32 for now).  Returns 1 on
// success, 0 if `t` is not a CONST or its dtype isn't int.
static int uop_iconst_value(Term t, i64 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t));
  if (term_tag(num) != TAG_NUM) return 0;
  u32 dtype = term_ext(num);
  if (!dtype_is_int(dtype)) return 0;
  *out = (i64)(i32)term_val(num);  // DT_INT32 sign-extension
  return 1;
}

// Read a UOP_RANGE's extent.  Returns 1 if `t` is a RANGE leaf with
// an int extent in its heap layout.  Used by range-bound-aware folds.
// (Distinct from the public uop_range_extent accessor: that returns
// the extent directly; this one writes via an out param.)
static int uop_range_extent_into(Term t, u32 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  Term ext_cell = heap_read(term_val(t) + 2);
  if (term_tag(ext_cell) != TAG_NUM) return 0;
  *out = (u32)term_val(ext_cell);
  return 1;
}

// Pack an i64 into a fresh DT_INT32 UOP_CONST.  Truncates beyond i32
// range; callers folding indices stay well within i32.
static Term uop_iconst(i64 v) {
  return uop_const(DT_INT32, (u32)(i32)v);
}

// Recognise `c * x` (UOP_IMUL with one ICONST operand).  Returns 1
// on match; *c_out is the constant, *x_out is the other operand.
// Either operand of the IMUL can be the constant; we canonicalise
// here.
static int uop_match_const_mul(Term t, i64 *c_out, Term *x_out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  i64 v;
  if (uop_iconst_value(a, &v)) { *c_out = v; *x_out = b; return 1; }
  if (uop_iconst_value(b, &v)) { *c_out = v; *x_out = a; return 1; }
  return 0;
}

static int uop_term_nonneg(Term t);  // defined below

// Conservative upper-bound estimator over UOp index expressions:
// returns 1 and sets *out to a (not necessarily tight) value V such
// that the term is provably <= V, assuming every RANGE leaf takes
// values in [0, extent).  Returns 0 when no finite bound can be
// derived structurally.  Mirrors tinygrad's symbolic vmax.
//
// This lets the div/mod-of-affine folds (below) recognise that the
// residue `y` in a `(c*x + y) / c` form is provably < c even when
// `y` is a compound packed-axis expression like
// `((a1*256 + a2)*2 + a3)*4 + a4` -- the case the composed im2col
// strided-view INDEX produces.  Bounded recursion -- DAG is finite.
static int uop_term_max_value(Term t, i64 *out) {
  if (term_tag(t) == TAG_NUM) { *out = (i64)(i32)term_val(t); return 1; }
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_RANGE: {
      u32 ext;
      if (!uop_range_extent_into(t, &ext) || ext == 0) return 0;
      *out = (i64)ext - 1;
      return 1;
    }
    case UOP_CONST: {
      i64 v;
      if (!uop_iconst_value(t, &v)) return 0;
      *out = v;
      return 1;
    }
    case UOP_IADD: {
      i64 a, b;
      if (!uop_term_max_value(heap_read(term_val(t) + 0), &a)) return 0;
      if (!uop_term_max_value(heap_read(term_val(t) + 1), &b)) return 0;
      *out = a + b;
      return 1;
    }
    case UOP_ISUB: {
      // x - y <= max(x) - 0 (y is non-negative for index exprs).
      i64 a;
      if (!uop_term_nonneg(heap_read(term_val(t) + 1))) return 0;
      if (!uop_term_max_value(heap_read(term_val(t) + 0), &a)) return 0;
      *out = a;
      return 1;
    }
    case UOP_IMUL: {
      i64 a, b;
      if (!uop_term_max_value(heap_read(term_val(t) + 0), &a)) return 0;
      if (!uop_term_max_value(heap_read(term_val(t) + 1), &b)) return 0;
      if (a < 0 || b < 0) return 0;  // sign-mixing -- bail
      *out = a * b;
      return 1;
    }
    case UOP_IDIV: {
      i64 dv, a;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &dv) || dv <= 0) return 0;
      if (!uop_term_max_value(heap_read(term_val(t) + 0), &a)) return 0;
      if (a < 0) return 0;
      *out = a / dv;
      return 1;
    }
    case UOP_IMOD: {
      i64 dv;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &dv) || dv <= 0) return 0;
      *out = dv - 1;
      return 1;
    }
    case UOP_IWHERE: {
      i64 a, b;
      if (!uop_term_max_value(heap_read(term_val(t) + 1), &a)) return 0;
      if (!uop_term_max_value(heap_read(term_val(t) + 2), &b)) return 0;
      *out = a > b ? a : b;
      return 1;
    }
    default: return 0;
  }
}

// "Bounded" check: is `y` provably in [0, bound)?  RANGE leaves
// have a known extent; ICONST in range qualifies; compound non-neg
// expressions with a derivable max < bound qualify; everything else
// fails (the simplifier stays conservative).
static int uop_term_strictly_below(Term y, i64 bound) {
  if (bound <= 0) return 0;
  u32 ext;
  if (uop_range_extent_into(y, &ext)) return (i64)ext <= bound;
  i64 v;
  if (uop_iconst_value(y, &v)) return v >= 0 && v < bound;
  i64 mx;
  if (uop_term_nonneg(y) && uop_term_max_value(y, &mx)) return mx < bound;
  return 0;
}

// Conservative non-negative estimator over UOp index expressions.
// Returns 1 only when the value is provably >= 0 by structure.
// Mirrors scalar/simplify.c's simplify_value_nonneg.
static int uop_term_nonneg(Term t) {
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
      return uop_term_nonneg(a) && uop_term_nonneg(b);
    }
    case UOP_IDIV:
    case UOP_IMOD: {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      i64 dv;
      if (!uop_iconst_value(b, &dv) || dv <= 0) return 0;
      return uop_term_nonneg(a);
    }
    default: return 0;
  }
}

// Recognise `(c * x) + y` shape on a numerator.  Returns 1 on match;
// *c_out, *x_out, *y_out filled in.  *y_out may be 0 if the shape is
// the simpler `c * x` (no addend).  Either operand of the IADD may
// hold the IMUL.
static int uop_match_affine_numerator(Term n, i64 *c_out, Term *x_out,
                                      Term *y_out) {
  // Plain `c * x` -- treat y as 0.
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

// Search a (possibly left-associated) IADD tree of index terms for a
// const-mul leaf `c*x` whose coefficient `c` is in a divisibility
// relation with `div` (either c | div or div | c -- i.e. one of the
// div/mod-of-affine folds below could fire on `(c*x + rest) op div`),
// AND for which the SUM of all the *other* terms in the tree is
// provably in [0, c).  On success returns 1 and sets *c_out, *x_out,
// and *rest_out to a Term that equals the sum of the remaining terms
// (rebuilt via uop_int_binary so it is itself simplified).  This
// generalises uop_match_affine_numerator past the shallow-IADD shape
// to the deep chains produced by composing a movement-view ShapeTracker
// into the kernel INDEX (conv im2col strided view, maxpool reshape).
//
// `depth` bounds the recursion; the index DAGs are small but finite.
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

// Validate the side condition that the div/mod-of-affine folds below
// need for numerator `c*x + rest` against divisor/modulus `div`:
//   - c % div == 0 (div divides c): only rest >= 0.
//   - div % c == 0 (c divides div): rest in [0, c).
// `rest` is given as a list of addend terms (their sum is the residue).
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
  if (c % div != 0) {  // div % c == 0 -- shrink-divisor needs rest < c
    if (!uop_term_strictly_below(rest, c)) return 0;
  }
  *rest_out = rest;
  return 1;
}

static int uop_match_affine_numerator_deep(Term n, i64 div,
                                            i64 *c_out, Term *x_out,
                                            Term *rest_out) {
  if (div <= 0) return 0;
  // Shallow form first (cheap, common).
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
  // Deep form: search a left-associated IADD tree for a const-mul leaf
  // whose coefficient is in a divisibility relation with `div`.
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

// === Binary integer simplifier ===

fn Term uop_simplify_int_binary(u32 opcode, Term a, Term b) {
  i64 av, bv;
  int a_const = uop_iconst_value(a, &av);
  int b_const = uop_iconst_value(b, &bv);

  // Constant-fold: both operands ICONST.
  if (a_const && b_const) {
    switch (opcode) {
      case UOP_IADD: return uop_iconst(av + bv);
      case UOP_ISUB: return uop_iconst(av - bv);
      case UOP_IMUL: return uop_iconst(av * bv);
      case UOP_IDIV:
        if (bv == 0) return 0;  // can't fold division by zero
        return uop_iconst(av / bv);
      case UOP_IMOD:
        if (bv == 0) return 0;
        return uop_iconst(av % bv);
      case UOP_ILT:  return uop_iconst(av < bv ? 1 : 0);
      case UOP_IAND: return uop_iconst(av & bv);
      default: break;
    }
  }

  // Identity / annihilator rules.
  switch (opcode) {
    case UOP_IADD:
      if (a_const && av == 0) return b;
      if (b_const && bv == 0) return a;
      // (x + c1) + c2 -> x + (c1+c2)  --  the IADD-of-IADD collapse.
      // (x - c1) + c2 -> x + (c2-c1)  --  IADD-of-ISUB.
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
      // Affine normalization: (c1*x) + (c2*x) -> (c1+c2)*x.
      // Mirrors emit_ibinop's IADD-of-IADD-with-const collapse on
      // the scalar side; brings the UOp simplifier to parity.
      {
        i64 c1, c2;
        Term x1, x2;
        if (uop_match_const_mul(a, &c1, &x1)
            && uop_match_const_mul(b, &c2, &x2)
            && x1 == x2) {
          return uop_int_binary(UOP_IMUL, x1, uop_iconst(c1 + c2));
        }
        // x + (c*x) -> (c+1)*x  (and the reverse)
        if (uop_match_const_mul(b, &c1, &x1) && a == x1) {
          return uop_int_binary(UOP_IMUL, x1, uop_iconst(c1 + 1));
        }
        if (uop_match_const_mul(a, &c1, &x1) && b == x1) {
          return uop_int_binary(UOP_IMUL, x1, uop_iconst(c1 + 1));
        }
        // Divmod recombination: (r // M) * M + (r % M) -> r.
        // Either operand of the IADD can be the IMUL.  Helps
        // RESHAPE-roundtrip chains where the consumer's flat-
        // decompose-recompose composes back to the source iter.
        Term mul_term, mod_term;
        if (uop_match_const_mul(a, &c1, &x1)
            && term_tag(b) == TAG_UOP && term_ext(b) == UOP_IMOD) {
          mul_term = a; mod_term = b; (void)mul_term;
        } else if (uop_match_const_mul(b, &c1, &x1)
                   && term_tag(a) == TAG_UOP && term_ext(a) == UOP_IMOD) {
          mul_term = b; mod_term = a; (void)mul_term;
        } else {
          mod_term = 0;
        }
        if (mod_term != 0) {
          // Verify x1 = (orig // c1) and mod_term = orig % c1 share `orig`.
          Term mod_a = heap_read(term_val(mod_term) + 0);
          Term mod_b = heap_read(term_val(mod_term) + 1);
          i64 mod_c;
          if (uop_iconst_value(mod_b, &mod_c) && mod_c == c1
              && term_tag(x1) == TAG_UOP && term_ext(x1) == UOP_IDIV) {
            Term div_a = heap_read(term_val(x1) + 0);
            Term div_b = heap_read(term_val(x1) + 1);
            i64 div_c;
            if (uop_iconst_value(div_b, &div_c) && div_c == c1
                && div_a == mod_a) {
              return mod_a;
            }
          }
        }
      }
      break;
    case UOP_ISUB:
      if (b_const && bv == 0) return a;
      if (a == b)             return uop_iconst(0);
      // (x + c1) - c2 -> x + (c1 - c2)  --  ISUB-of-IADD-with-const
      // (x - c1) - c2 -> x - (c1 + c2)  --  ISUB-of-ISUB-with-const
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
      break;
    case UOP_IDIV:
      if (b_const && bv == 1) return a;
      if (a_const && av == 0) return uop_iconst(0);
      if (a == b && b_const && bv != 0) return uop_iconst(1);
      // Range-bound-aware: `x // c -> 0` when 0 <= x and max(x) < c.
      // Mirrors the IMOD range-bound fold below; collapses the residual
      // `(a5/25)/32`-style divides that fall out of the deep affine
      // composition once a constant divisor exceeds the numerator's
      // provable bound.
      if (b_const && bv > 0 && !a_const) {
        i64 mx;
        if (uop_term_nonneg(a) && uop_term_max_value(a, &mx) && mx < bv)
          return uop_iconst(0);
      }
      // RESHAPE-roundtrip fold: `(c*x + y) / c` -> `x` when y in [0, c).
      // Also handles the bare `(c*x) / c` -> x case (y = 0 implicit).
      if (b_const && bv > 0) {
        i64 c;
        Term x, y;
        if (uop_match_affine_numerator(a, &c, &x, &y) && c == bv) {
          if (y == 0 || uop_term_strictly_below(y, c)) return x;
        }
      }
      // Generalised div-of-affine over a (possibly deep) IADD tree.
      // For numerator `c*x + rest` with divisor `d`:
      //   - d | c (c = m*d):  -> m*x + rest/d            (rest >= 0)
      //   - c | d (d = n*c):  -> x / n                   (0 <= rest < c)
      // These collapse the chained div/mod the rangeify ShapeTracker
      // composer emits when it folds a movement-view chain into the
      // kernel INDEX (conv im2col strided view; maxpool reshape) --
      // turning a ~20-divide-per-iter address into a handful.  The
      // bare `c*x / c -> x` and `(c1*xnum) // bv` cases above are
      // strict specialisations; this is the general form.
      if (b_const && bv > 0 && !a_const) {
        i64 c;
        Term x, rest;
        if (uop_match_affine_numerator_deep(a, bv, &c, &x, &rest)) {
          if (c % bv == 0) {
            // distribute: (m*d*x + rest) / d -> m*x + rest/d
            Term term_x = uop_int_binary(UOP_IMUL, x, uop_iconst(c / bv));
            Term term_r = uop_int_binary(UOP_IDIV, rest, b);
            return uop_int_binary(UOP_IADD, term_x, term_r);
          }
          if (bv % c == 0) {
            // shrink divisor: (c*x + rest) / (n*c) -> x / n  (rest < c)
            return uop_int_binary(UOP_IDIV, x, uop_iconst(bv / c));
          }
        }
      }
      // GCD-aware: (k*c1) // (k*c2) -> c1 // c2 when k>0.  Plays
      // out as `(c*x) // (c*y) = x/y` when both numerator and
      // denominator factor through the same constant.
      if (a_const && b_const) break;  // caught by the constant fold above
      if (b_const && bv > 0) {
        i64 c1;
        Term xnum;
        if (uop_match_const_mul(a, &c1, &xnum) && c1 > 0 && bv % c1 == 0) {
          // (c1 * xnum) // bv -> xnum // (bv / c1)
          return uop_int_binary(UOP_IDIV, xnum, uop_iconst(bv / c1));
        }
      }
      // Nested div-mod: (r % (k*c)) // c -> (r // c) % k when c | (k*c).
      // Mirrors tinygrad's divandmod.py:26-27 IDIV branch.  Common in
      // chained RESHAPE flat-decompose chains.
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
      // add-div-split: (x + c) // d -> (x + c%d) // d + c/d when c >= d
      // and x >= 0.  Hoists the integer part of c so a downstream
      // const-folder can merge it with sibling constants.  Mirrors
      // tinygrad's divandmod.py:112-113 / scalar simplify rule
      // rule_add_div_split.
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
      // Range-bound-aware: if `a` is a UOP_RANGE with extent <= bv,
      // then `a % bv = a` (the iter never reaches bv).  Generalised:
      // `x % c -> x` for any non-negative `x` whose provable max < c.
      if (b_const && bv > 0) {
        u32 ext;
        if (uop_range_extent_into(a, &ext) && (i64)ext <= bv) return a;
        if (!a_const) {
          i64 mx;
          if (uop_term_nonneg(a) && uop_term_max_value(a, &mx) && mx < bv)
            return a;
        }
      }
      // RESHAPE-roundtrip fold: `(c*x + y) % c` -> y when y in [0, c).
      // For the bare `(c*x) % c` shape (y implicit), result is 0.
      if (b_const && bv > 0) {
        i64 c;
        Term x, y;
        if (uop_match_affine_numerator(a, &c, &x, &y) && c == bv) {
          (void)x;
          if (y == 0)                                  return uop_iconst(0);
          if (uop_term_strictly_below(y, c))           return y;
        }
      }
      // Generalised mod-of-affine over a (possibly deep) IADD tree.
      // For numerator `c*x + rest` with modulus `d`:
      //   - d | c (c = m*d):  -> rest % d              (rest >= 0)
      //   - c | d (d = n*c):  -> c*(x % n) + rest      (0 <= rest < c)
      // Companion of the IDIV rule above; same composed-chain motive.
      if (b_const && bv > 0 && !a_const) {
        i64 c;
        Term x, rest;
        if (uop_match_affine_numerator_deep(a, bv, &c, &x, &rest)) {
          if (c % bv == 0) {
            // c*x term vanishes mod d: (m*d*x + rest) % d -> rest % d
            return uop_int_binary(UOP_IMOD, rest, b);
          }
          if (bv % c == 0) {
            // (c*x + rest) % (n*c) -> c*(x % n) + rest   (rest < c)
            Term xm = uop_int_binary(UOP_IMOD, x, uop_iconst(bv / c));
            Term cm = uop_int_binary(UOP_IMUL, xm, uop_iconst(c));
            return uop_int_binary(UOP_IADD, cm, rest);
          }
        }
      }
      // Coefficient-reduction mod m: in `(... + c*x + ...) % m`, replace
      // every const-mul term `c*x` whose coefficient has `c >= m` by
      // `(c % m)*x` -- an exact identity (c*x = (q*m + r)*x and q*m*x
      // vanishes mod m).  Then re-take the mod; the smaller residue often
      // collapses via the range-bound IMOD rule above.  Turns the
      // `(kh*25 + oh) % 24`-style residues the deep affine composition
      // leaves into plain `(kh + oh)` (max 23 < 24).
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
      // Nested mod-mod: (r % (k*c)) % c -> r % c when c | (k*c).
      // Mirrors tinygrad's divandmod.py:26-27 MOD branch.
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
      // Range-bound-aware: if `a` is RANGE with extent <= bv, then
      // a < bv is always true.
      if (b_const && bv > 0) {
        u32 ext;
        if (uop_range_extent_into(a, &ext) && (i64)ext <= bv) return uop_iconst(1);
      }
      // Iter values from RANGE are non-negative, so RANGE < 0 (or any
      // non-positive constant) is always false.  This is the PAD
      // bounds-check lower-end fold: when `begin == 0` we never build
      // `RANGE < 0` directly, but combining a `RANGE - begin` shift
      // with a follow-on bound check can produce one.  Even at
      // construction time the simple `RANGE < ICONST(<=0)` shape shows
      // up in the canonical PAD-mask collapse.
      if (b_const && bv <= 0) {
        u32 ext;
        if (uop_range_extent_into(a, &ext)) return uop_iconst(0);
      }
      break;
    case UOP_IAND:
      if (a_const && av == 0) return uop_iconst(0);
      if (b_const && bv == 0) return uop_iconst(0);
      // Treat 1 as boolean-true on bool/cond inputs.
      if (a_const && av == 1) return b;
      if (b_const && bv == 1) return a;
      if (a == b)             return a;
      break;
    default: break;
  }
  return 0;
}

// === Ternary IWHERE simplifier ===

fn Term uop_simplify_iwhere(Term cond, Term then_v, Term else_v) {
  i64 cv;
  if (uop_iconst_value(cond, &cv)) {
    return cv != 0 ? then_v : else_v;
  }
  if (then_v == else_v) return then_v;
  // IWHERE-of-IWHERE collapse: when the then-branch is itself an
  // IWHERE that shares the same else-branch (the canonical PAD-mask
  // shape: IWHERE(c1, IWHERE(c2, val, INVALID), INVALID)), combine
  // the conditions via IAND.  Mirrors emit_iwhere's nested-fold in
  // schedule/rangeify.c.
  if (term_tag(then_v) == TAG_UOP && term_ext(then_v) == UOP_IWHERE) {
    Term inner_cond = heap_read(term_val(then_v) + 0);
    Term inner_then = heap_read(term_val(then_v) + 1);
    Term inner_else = heap_read(term_val(then_v) + 2);
    if (inner_else == else_v) {
      Term combined = uop_int_binary(UOP_IAND, cond, inner_cond);
      return uop_iwhere(combined, inner_then, else_v);
    }
  }
  return 0;
}
