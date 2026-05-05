// uop/index_simplify.c - constructor-time simplifier for the symbolic
// INDEX layer (Phase B2).
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
static int uop_range_extent(Term t, u32 *out) {
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

// "Bounded" check: is `y` provably in [0, bound)?  RANGE leaves
// have a known extent; ICONST in range qualifies; everything else
// fails (the simplifier stays conservative).
static int uop_term_strictly_below(Term y, i64 bound) {
  if (bound <= 0) return 0;
  u32 ext;
  if (uop_range_extent(y, &ext)) return (i64)ext <= bound;
  i64 v;
  if (uop_iconst_value(y, &v)) return v >= 0 && v < bound;
  return 0;
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
      // RESHAPE-roundtrip fold: `(c*x + y) / c` -> `x` when y in [0, c).
      // Also handles the bare `(c*x) / c` -> x case (y = 0 implicit).
      if (b_const && bv > 0) {
        i64 c;
        Term x, y;
        if (uop_match_affine_numerator(a, &c, &x, &y) && c == bv) {
          if (y == 0 || uop_term_strictly_below(y, c)) return x;
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
      break;
    case UOP_IMOD:
      if (b_const && bv == 1) return uop_iconst(0);
      if (a_const && av == 0) return uop_iconst(0);
      // Range-bound-aware: if `a` is a UOP_RANGE with extent <= bv,
      // then `a % bv = a` (the iter never reaches bv).
      if (b_const && bv > 0) {
        u32 ext;
        if (uop_range_extent(a, &ext) && (i64)ext <= bv) return a;
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
      break;
    case UOP_ILT:
      if (a == b) return uop_iconst(0);
      // Range-bound-aware: if `a` is RANGE with extent <= bv, then
      // a < bv is always true.
      if (b_const && bv > 0) {
        u32 ext;
        if (uop_range_extent(a, &ext) && (i64)ext <= bv) return uop_iconst(1);
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
        if (uop_range_extent(a, &ext)) return uop_iconst(0);
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
