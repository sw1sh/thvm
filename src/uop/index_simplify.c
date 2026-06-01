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

// Conservative lower-bound estimator (mirror of uop_term_max_value;
// tinygrad symbolic vmin).  Returns 1 and sets *out to a value V such
// that the term is provably >= V (RANGE leaves >= 0).  Used by
// parse_valid / uop_given_valid to derive the [vmin, vmax] window of a
// guarded expression.
static int uop_term_min_value(Term t, i64 *out) {
  if (term_tag(t) == TAG_NUM) { *out = (i64)(i32)term_val(t); return 1; }
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_RANGE: {
      u32 ext;
      if (!uop_range_extent_into(t, &ext)) return 0;
      *out = 0;  // iters in [0, extent)
      return 1;
    }
    case UOP_CONST: { i64 v; if (!uop_iconst_value(t, &v)) return 0; *out = v; return 1; }
    case UOP_IADD: {
      i64 a, b;
      if (!uop_term_min_value(heap_read(term_val(t) + 0), &a)) return 0;
      if (!uop_term_min_value(heap_read(term_val(t) + 1), &b)) return 0;
      *out = a + b;
      return 1;
    }
    case UOP_IMUL: {
      // Non-negative operands only (index exprs); min = min(a)*min(b).
      i64 amin, bmin;
      if (!uop_term_min_value(heap_read(term_val(t) + 0), &amin)) return 0;
      if (!uop_term_min_value(heap_read(term_val(t) + 1), &bmin)) return 0;
      if (amin < 0 || bmin < 0) return 0;
      *out = amin * bmin;
      return 1;
    }
    case UOP_IDIV: {
      i64 dv, a;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &dv) || dv <= 0) return 0;
      if (!uop_term_min_value(heap_read(term_val(t) + 0), &a) || a < 0) return 0;
      *out = a / dv;
      return 1;
    }
    case UOP_IMOD:
      *out = 0;  // x % c in [0, c) for non-negative x
      return 1;
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
static int uop_term_nonneg_d(Term t, u32 depth) {
  // Bounded recursion over the finite index DAG (mirrors the depth>48
  // guards on uop_index_substitute / uop_term_has_range_aid in this
  // file).  The composed conv-backward col2im index can nest deeply
  // (and on the CUDA symbolic-codegen path the iwhere valid-fold builds
  // very deep masked-divmod trees); without this cap the recursion
  // overflows the stack.  A deep/pathological term is conservatively
  // "not provably nonneg" (a missed simplification, never wrong).
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
    default: return 0;
  }
}

static int uop_term_nonneg(Term t) { return uop_term_nonneg_d(t, 0); }

// === uop_int_bounds: conservative [lo, hi] interval inference ===
//
// A single two-sided range estimator over the int UOp index DAG,
// mirroring tinygrad's symbolic (vmin, vmax) pair.  Unlike the older
// one-sided uop_term_min_value / uop_term_max_value above (which bail
// the moment a sign is mixed and special-case IMOD's lower bound to 0),
// this returns a tight-as-cheaply-possible inclusive interval for both
// endpoints in one pass, with full sign handling on IMUL / IDIV and
// overflow guards on every intermediate.
//
// Contract: returns 1 and fills *lo/*hi such that the value of `t` is
// provably in [*lo, *hi] for ALL assignments of its RANGE leaves (each
// RANGE r ranges over [0, static_extent(r)-1]).  Returns 0 when no
// finite two-sided bound can be derived; callers MUST treat 0 as "no
// information" and never assume anything about *lo/*hi in that case.
//
// CONSERVATISM: every unhandled op, unknown leaf (BUFFER/LOAD/etc.),
// non-constant divisor/modulus, or arithmetic overflow returns 0.  A
// wrong (too-narrow) bound would silently miscompile a later div/mod or
// valid-mask fold, so the function errs hard toward 0/unknown.
//
// This primitive is NOT yet wired into any simplification rule -- it is
// verified infrastructure for a later increment.

// Floor division for i64: C `/` truncates toward zero, but interval
// arithmetic over IDIV needs floor (round toward -inf) so that the
// endpoints stay sound for negative numerators.  Requires d > 0.
static i64 uop_floordiv_i64(i64 a, i64 d) {
  i64 q = a / d;
  i64 r = a % d;
  if (r != 0 && ((r < 0) != (d < 0))) q -= 1;
  return q;
}

// Checked i64 add/sub/mul: return 1 and write *out on success, 0 on
// overflow.  __builtin_*_overflow is available on clang/gcc (the only
// compilers this codebase targets).
static int uop_chk_add(i64 a, i64 b, i64 *out) { return !__builtin_add_overflow(a, b, out); }
static int uop_chk_sub(i64 a, i64 b, i64 *out) { return !__builtin_sub_overflow(a, b, out); }
static int uop_chk_mul(i64 a, i64 b, i64 *out) { return !__builtin_mul_overflow(a, b, out); }

static int uop_int_bounds_d(Term t, i64 *lo, i64 *hi, u32 depth);
static int uop_is_invalid(Term t);  // defined below (invalid_gate section)

// Min/max of the four corner products la*lb, la*hb, ha*lb, ha*hb, with
// overflow checking.  Returns 1 and fills *lo/*hi on success, 0 if any
// corner overflows i64.
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

static int uop_int_bounds_d(Term t, i64 *lo, i64 *hi, u32 depth) {
  if (depth > 64) return 0;  // pathological nesting -> unknown
  // Bare NUM leaf (a raw heap integer, not wrapped in a CONST UOp).
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
      // Iter ranges over [0, static_extent-1].  static_extent is the
      // worst-case upper bound even for kvar (symbolic) extents, so the
      // interval stays sound when the runtime extent is smaller.
      // (uop_range_extent_into reads the raw packed extent; kvar_extent_static
      // unpacks a symbolic extent's registered upper bound.  We use those two
      // -- not index.c's uop_range_static_extent -- because index.c is
      // #included AFTER this file in thvm.c, so its accessors aren't visible
      // here yet, whereas the kvar helpers and uop_range_extent_into are.)
      u32 packed;
      if (!uop_range_extent_into(t, &packed)) return 0;
      u32 ext = kvar_extent_static(packed);
      if (ext == 0) return 0;  // degenerate / empty -- no useful bound
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
      // [la - hb, ha - lb].
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
      // const * range: scale, swapping endpoints for a negative const.
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
      // Both non-const but bounded: min/max over the four corners.
      return uop_mul_corner_bounds(la, ha, lb, hb, lo, hi);
    }
    case UOP_IDIV: {
      // Positive constant divisor only.  thvm renders IDIV as C `/`
      // (truncation toward zero), which equals floor ONLY for a
      // non-negative numerator; for a negative numerator C-trunc and floor
      // disagree, so a floor bound would be unsound vs the emitted code.
      // Index expressions are non-negative, so we bound only the provably
      // non-negative case and return unknown otherwise.  (la>=0 => the
      // whole range is non-negative; or uop_term_nonneg proves it when the
      // interval bound is loose -- then clamp the low end to 0.)
      i64 d;
      if (!uop_iconst_value(heap_read(term_val(t) + 1), &d) || d <= 0) return 0;
      Term num = heap_read(term_val(t) + 0);
      i64 la, ha;
      if (!uop_int_bounds_d(num, &la, &ha, depth + 1)) return 0;
      if (la < 0) {
        if (!uop_term_nonneg(num)) return 0;
        la = 0;  // proven >= 0; the interval lower bound was just loose
      }
      *lo = uop_floordiv_i64(la, d);  // == la/d for la >= 0 (trunc == floor)
      *hi = uop_floordiv_i64(ha, d);
      return 1;
    }
    case UOP_IMOD: {
      // Positive constant modulus c.  For a non-negative numerator C `%`
      // lands in [0, c); for a negative numerator C `%` is negative, so
      // the [0, c-1] bound (and the x%c==x tightening) is sound only when
      // the numerator is provably non-negative.  Index exprs are
      // non-negative; return unknown otherwise.
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
    case UOP_IWHERE: {
      // select(cond, x, y): union the two branch intervals, ignoring
      // cond.  An INVALID branch is a masking sentinel (no numeric
      // value); skip it and take the other branch's bound.  If both are
      // INVALID, or the live branch is unbounded, return 0.
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
      // Comparison result is boolean 0/1.
      *lo = 0; *hi = 1;
      return 1;
    case UOP_IAND:
    case UOP_IOR: {
      // Boolean conjunction/disjunction on 0/1 -> [0, 1].  Only claim
      // this when BOTH operands are themselves provably in [0, 1]; a
      // bitwise use on wider integers has no such bound (return 0).
      i64 la, ha, lb, hb;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 0), &la, &ha, depth + 1)) return 0;
      if (!uop_int_bounds_d(heap_read(term_val(t) + 1), &lb, &hb, depth + 1)) return 0;
      if (la < 0 || ha > 1 || lb < 0 || hb > 1) return 0;
      *lo = 0; *hi = 1;
      return 1;
    }
    default:
      return 0;  // unknown op / BUFFER / LOAD / etc. -- no info
  }
}

// Public entry: infer a conservative inclusive [lo, hi] for the
// int-typed UOp `t`.  Returns 1 on success, 0 if unbounded/unknown.
static int uop_int_bounds(Term t, i64 *lo, i64 *hi) {
  return uop_int_bounds_d(t, lo, hi, 0);
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

// Recognise a bare `IMUL(q, const)` with the constant on either side.
// Returns 1 on match; *q_out is the non-const operand, *c_out the const.
static int uop_match_mul_const(Term t, Term *q_out, i64 *c_out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  i64 v;
  if (uop_iconst_value(b, &v)) { *q_out = a; *c_out = v; return 1; }
  if (uop_iconst_value(a, &v)) { *q_out = b; *c_out = v; return 1; }
  return 0;
}

// Recognise a "MOD-term" addend for fold_add_divmod_recombine: either a
// bare `base % div` (mul = 1) or `(base % div) * mul`.  On match returns
// 1 and fills *base_out, *div_out, *mul_out.  `div`/`mul` are CONST.
// Mirror of the head of tinygrad/uop/symbolic.py:30-33's term loop.
static int uop_match_recombine_mod_term(Term u, Term *base_out, i64 *div_out,
                                        i64 *mul_out) {
  if (term_tag(u) != TAG_UOP) return 0;
  // Bare `base % div`  ->  mul = 1.
  if (term_ext(u) == UOP_IMOD) {
    Term div_c = heap_read(term_val(u) + 1);
    i64 div;
    if (!uop_iconst_value(div_c, &div)) return 0;
    *base_out = heap_read(term_val(u) + 0);
    *div_out = div;
    *mul_out = 1;
    return 1;
  }
  // `(base % div) * mul`.
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

// Rebuild the sum of `terms[]` (count `nt`) skipping indices `skip_i` and
// `skip_j`, then add `head` (the recombined leaf).  Left-associates via
// uop_int_binary so the result is itself re-simplified.  The analog of
// tinygrad's `head.usum(*[t for k,t in ... if k not in (i,j)])`.
static Term uop_recombine_rebuild(Term head, Term const *terms, u32 nt,
                                  u32 skip_i, u32 skip_j) {
  Term acc = head;
  for (u32 k = 0; k < nt; k++) {
    if (k == skip_i || k == skip_j) continue;
    acc = uop_int_binary(UOP_IADD, acc, terms[k]);
  }
  return acc;
}

// Port of tinygrad/uop/symbolic.py:28-49 fold_add_divmod_recombine.  Splits
// the IADD tree under `(a + b)` into its full list of addend leaves, then
// looks for a (MOD-term, matching div-term) pair that recombines into a
// flatter form.  The three exact cases (all value-identities):
//   1. (base%div)*mul + (base//div)*(div*mul)            -> base*mul
//   2. ((base//d)%div)*mul + (base//(d*div))*(div*mul)   -> (base//d)*mul
//   3. ((base//div)%e)*div + base%div  (mul==1)          -> base%(div*e)
// On a match returns the recombined sum; else 0.  This generalises the
// shallow `(r//M)*M + (r%M) -> r` rule (a special case of #1 with mul=1)
// past the two-direct-operand shape to terms buried in a deep left-
// associated IADD tree (the rangeify im2col / maxpool-reshape INDEX).
static Term uop_fold_add_divmod_recombine(Term a, Term b) {
  // Collect leaves from the two operands directly -- we must NOT rebuild
  // `uop_int_binary(IADD, a, b)` here, since that re-enters this rule on
  // the same operands and loops.  Each operand is already a simplified
  // (sub)tree; walking both yields the full addend list of the sum a+b.
  Term terms[32];
  u32  nt = 0;
  if (!uop_iadd_tree_collect_const_mul(a, terms, &nt, 32, 0)) return 0;
  if (!uop_iadd_tree_collect_const_mul(b, terms, &nt, 32, 0)) return 0;
  if (nt < 2) return 0;
  for (u32 i = 0; i < nt; i++) {
    Term base; i64 div, mul;
    if (!uop_match_recombine_mod_term(terms[i], &base, &div, &mul)) continue;
    if (div <= 0) continue;
    i64 want = div * mul;  // the div-term's required coefficient
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
      // Case 2: ((base//d)%div)*mul + (base//(d*div))*(div*mul)
      //         -> (base//d)*mul.   Here `base` is itself `inner // d`.
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
      // Case 3: ((base//div)%e)*div + base%div  (mul==1)
      //         -> base%(div*e).   Here terms[i] is the bare `base%div`
      //         (mul==1) and terms[j] = ((base//div)%e) * div, so the
      //         `v` coefficient `want == div`, and `q = (base//div) % e`.
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

// === invalid_gate / propagate_invalid (tinygrad symbolic.py:25-65) ===

// The valid-given index folds (propagate_invalid push + uop_given_valid)
// gate behind THVM_FUSE_CONV_BWD so the shared index simplifier stays
// bit-identical when the conv-backward fusion is off.  Cached: the env
// is fixed for the process lifetime.
static int uop_valid_fold_enabled(void) {
  static int v = -1;
  if (v < 0) { char const *e = getenv("THVM_FUSE_CONV_BWD"); v = (e && e[0] == '1') ? 1 : 0; }
  return v;
}

// True iff `t` is the bare INVALID sentinel CONST.
static int uop_is_invalid(Term t) {
  return term_tag(t) == TAG_UOP && term_ext(t) == UOP_INVALID;
}

// Recognise the `cond.where(x, INVALID)` invalid-gate shape (symbolic.py:26
// `invalid_gate = UPat.var("cond").where(UPat.var("x"), invalid_pat)`).
// Returns 1 and fills *cond_out/*x_out on match.
static int uop_match_invalid_gate(Term t, Term *cond_out, Term *x_out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IWHERE) return 0;
  Term else_v = heap_read(term_val(t) + 2);
  if (!uop_is_invalid(else_v)) return 0;
  *cond_out = heap_read(term_val(t) + 0);
  *x_out    = heap_read(term_val(t) + 1);
  return 1;
}

// === Binary integer simplifier ===

fn Term uop_simplify_int_binary(u32 opcode, Term a, Term b) {
  i64 av, bv;
  int a_const = uop_iconst_value(a, &av);
  int b_const = uop_iconst_value(b, &bv);

  // propagate_invalid: push a (non-comparison) binary op past an
  // invalid-gate so the gate stays at the top, where uop_given_valid
  // can fold the body against its own condition.  Mirrors
  // tinygrad/uop/symbolic.py:55-65:
  //   (op, src=(cond.where(x,INV), y)) -> cond.where(x op y, INV)
  //   (op, src=(y, cond.where(x,INV))) -> cond.where(y op x, INV)
  //   (op, src=(INV, *))               -> INV   (binary with invalid)
  // Comparisons (ILT) keep INVALID as a propagated 0/1 elsewhere; we
  // only handle the arithmetic ops the col2im decode uses.
  if (uop_valid_fold_enabled()
      && (opcode == UOP_IADD || opcode == UOP_ISUB || opcode == UOP_IMUL
          || opcode == UOP_IDIV || opcode == UOP_IMOD || opcode == UOP_IAND)) {
    if (uop_is_invalid(a) || uop_is_invalid(b)) return uop_invalid();
    Term gc, gx;
    if (uop_match_invalid_gate(a, &gc, &gx)) {
      Term inner = uop_int_binary(opcode, gx, b);
      return uop_iwhere(gc, inner, uop_invalid());
    }
    if (uop_match_invalid_gate(b, &gc, &gx)) {
      Term inner = uop_int_binary(opcode, a, gx);
      return uop_iwhere(gc, inner, uop_invalid());
    }
  }

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
      case UOP_IOR:  return uop_iconst(av | bv);
      case UOP_IXOR: return uop_iconst(av ^ bv);
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
      }
      // Divmod recombination over the full IADD tree (tinygrad
      // fold_add_divmod_recombine).  Subsumes the shallow two-operand
      // `(r//M)*M + (r%M) -> r` case as case 1 with mul=1, and also
      // collapses the deep im2col / maxpool-reshape gather index where
      // the matching (mod, div*mul) pair is buried among many addends.
      {
        Term r = uop_fold_add_divmod_recombine(a, b);
        if (r != 0) return r;
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

// === uop_given_valid (tinygrad symbolic.py:303-353) ===

// Mint a throwaway RANGE leaf with a reserved high axis_id (won't
// collide with real kernel axes, which are densely numbered from 0) and
// a constrained extent.  uop_given_valid substitutes a guarded
// expression with such a fake so the div/mod folds see the TIGHTER
// [0, extent) bound from the valid clause, then substitutes the fake
// back -- it never escapes into a kernel body.  Mirrors tinygrad's
// `UOp.variable(f"fake{i}", v0, v1)` in uop_given_valid.
#define UOP_VALID_FAKE_AXIS_BASE 0x70000000u
static Term uop_make_fake_range(u32 extent) {
  static u32 ctr = 0;
  if (extent == 0) return 0;
  return uop_range(UOP_VALID_FAKE_AXIS_BASE + (ctr++ & 0xFFFF), KAX_LOOP, extent);
}


// Rebuild `t`, replacing every structural occurrence of `from` with
// `to`, routing through the simplifying constructors (uop_int_binary /
// uop_iwhere) so the substituted tree is re-simplified bottom-up.  This
// is the `substitute` half of tinygrad's uop_given_valid: it lets a
// bounded fake-variable propagate the div/mod folds, then the inverse
// substitution restores the original sub-expression.  Bounded recursion
// over the finite index DAG.
static Term uop_index_substitute(Term t, Term from, Term to, u32 depth) {
  if (t == from) return to;
  if (depth > 48) return t;
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
    case UOP_IXOR: {
      Term a = uop_index_substitute(heap_read(term_val(t) + 0), from, to, depth + 1);
      Term b = uop_index_substitute(heap_read(term_val(t) + 1), from, to, depth + 1);
      if (a == heap_read(term_val(t) + 0) && b == heap_read(term_val(t) + 1)) return t;
      return uop_int_binary(op, a, b);
    }
    case UOP_IWHERE: {
      Term c = uop_index_substitute(heap_read(term_val(t) + 0), from, to, depth + 1);
      Term x = uop_index_substitute(heap_read(term_val(t) + 1), from, to, depth + 1);
      Term e = uop_index_substitute(heap_read(term_val(t) + 2), from, to, depth + 1);
      if (c == heap_read(term_val(t) + 0) && x == heap_read(term_val(t) + 1)
          && e == heap_read(term_val(t) + 2)) return t;
      return uop_iwhere(c, x, e);
    }
    default: return t;
  }
}

// True iff the DAG rooted at `t` contains a UOP_RANGE leaf whose
// axis_id equals `aid`.  Bounded recursion over the finite index DAG.
static int uop_term_has_range_aid(Term t, u32 aid, u32 depth) {
  if (depth > 48 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_RANGE) return (u32)term_val(heap_read(term_val(t) + 0)) == aid;
  if (op == UOP_CONST || op == UOP_INVALID) return 0;
  u32 n = (op == UOP_IWHERE) ? 3 : 2;
  for (u32 i = 0; i < n; i++) {
    if (uop_term_has_range_aid(heap_read(term_val(t) + i), aid, depth + 1)) return 1;
  }
  return 0;
}

// True iff `clause` and `x` share at least one RANGE leaf (same axis_id).
// Mirrors tinygrad's `any(r in x.ranges for r in c.ranges)`
// (symbolic.py:383).
static int uop_term_shares_range_with(Term clause, Term x, u32 depth) {
  if (depth > 48 || term_tag(clause) != TAG_UOP) return 0;
  u32 op = term_ext(clause);
  if (op == UOP_RANGE) {
    u32 aid = (u32)term_val(heap_read(term_val(clause) + 0));
    return uop_term_has_range_aid(x, aid, 0);
  }
  if (op == UOP_CONST || op == UOP_INVALID) return 0;
  u32 n = (op == UOP_IWHERE) ? 3 : 2;
  for (u32 i = 0; i < n; i++) {
    if (uop_term_shares_range_with(heap_read(term_val(clause) + i), x, depth + 1)) return 1;
  }
  return 0;
}

// parse_valid (symbolic.py:303-314): decompose a single valid clause
// into (expr, is_upper, bound):
//   X < c        -> (X, upper, c-1)        [X <= c-1]
//   (X < c).ne(1) i.e. X >= c              [thvm has no CMPNE form; the
//                                           PAD lower guard is ILT(s-1, X)
//                                           = (X > s-1) = (X >= s); we
//                                           recognise that as a lower bound]
// Returns 1 on a recognised clause.  *is_upper: 1 => X <= bound, 0 => X >= bound.
static int uop_parse_valid(Term v, Term *expr_out, int *is_upper, i64 *bound_out) {
  if (term_tag(v) != TAG_UOP || term_ext(v) != UOP_ILT) return 0;
  Term lhs = heap_read(term_val(v) + 0);
  Term rhs = heap_read(term_val(v) + 1);
  i64 c;
  // X < c  ->  X <= c-1
  if (uop_iconst_value(rhs, &c)) {
    *expr_out = lhs; *is_upper = 1; *bound_out = c - 1;
    return 1;
  }
  // c < X  ->  X >= c+1   (the PAD lower guard ILT(s-1, X))
  if (uop_iconst_value(lhs, &c)) {
    *expr_out = rhs; *is_upper = 0; *bound_out = c + 1;
    return 1;
  }
  return 0;
}

// Split a valid condition (a left-assoc IAND tree) into its clauses.
static u32 uop_valid_split_and(Term v, Term *out, u32 cap, u32 depth) {
  if (depth > 32 || cap == 0) return 0;
  if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_IAND) {
    u32 n0 = uop_valid_split_and(heap_read(term_val(v) + 0), out, cap, depth + 1);
    u32 n1 = uop_valid_split_and(heap_read(term_val(v) + 1), out + n0, cap - n0, depth + 1);
    return n0 + n1;
  }
  out[0] = v;
  return 1;
}

// uop_given_valid (symbolic.py:316-353): simplify `uop` assuming `valid`
// is true.  For each parsed clause, substitute the bounded expression
// with a fresh RANGE that has the constrained extent, re-simplify, and
// substitute back; keep the result only if the substitution changed and
// re-simplified the body (a strictly-smaller form).  This is what folds
// the col2im decode: given `(X) < 125` the guarded `X//25` / `X%25`
// simplify against the tightened [vmin, vmax] window of X.
static Term uop_given_valid(Term valid, Term uop) {
  Term clauses[16];
  u32 nc = uop_valid_split_and(valid, clauses, 16, 0);
  // Aggregate per-expression bounds: lo[i], hi[i] (NULL == unknown).
  Term exprs[16]; i64 los[16], his[16]; int has_lo[16], has_hi[16];
  u32 ne = 0;
  for (u32 i = 0; i < nc; i++) {
    Term e; int up; i64 b;
    if (!uop_parse_valid(clauses[i], &e, &up, &b)) continue;
    // Find or insert.
    u32 idx = ne;
    for (u32 j = 0; j < ne; j++) if (exprs[j] == e) { idx = j; break; }
    if (idx == ne) {
      if (ne >= 16) break;
      exprs[ne] = e; has_lo[ne] = 0; has_hi[ne] = 0; ne++;
    }
    if (up) { his[idx] = b; has_hi[idx] = 1; }
    else    { los[idx] = b; has_lo[idx] = 1; }
  }
  for (u32 i = 0; i < ne; i++) {
    Term e = exprs[i];
    // Skip a bare RANGE / CONST -- nothing to fold.
    if (term_tag(e) != TAG_UOP) continue;
    u32 eop = term_ext(e);
    if (eop == UOP_RANGE || eop == UOP_CONST) continue;
    i64 lo, hi;
    if (has_lo[i]) lo = los[i]; else if (!uop_term_min_value(e, &lo)) continue;
    if (has_hi[i]) hi = his[i]; else if (!uop_term_max_value(e, &hi)) continue;
    if (lo < 0 || hi < lo) continue;
    // Build a fresh bounded RANGE standing in for `e` over [0, hi-lo]
    // shifted by lo (i.e. fake in [0, hi-lo], e == fake+lo).  We model
    // the constrained variable as a RANGE of extent (hi-lo+1) plus lo.
    u64 span = (u64)(hi - lo) + 1;
    if (span == 0 || span > 0x40000000ull) continue;
    Term fake = uop_make_fake_range((u32)span);
    if (fake == 0) continue;
    Term fake_expr = (lo == 0) ? fake
                   : uop_int_binary(UOP_IADD, fake, uop_iconst(lo));
    Term sub = uop_index_substitute(uop, e, fake_expr, 0);
    if (sub == uop) continue;  // e not present in uop -- skip
    // Substitute the fake back to the original expr and re-simplify.
    Term back = uop_index_substitute(sub, fake, e, 0);
    if (back != uop) uop = back;
  }
  return uop;
}

// === Ternary IWHERE simplifier ===

static Term uop_simplify_iwhere_body(Term cond, Term then_v, Term else_v) {
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
  // drop_and_clauses (tinygrad symbolic.py:382-385): in WHERE(cond, x, INV)
  // partition cond's AND-clauses by whether their ranges appear in x; if
  // any clause's ranges are absent from x, drop it (the gate can't affect
  // x's value there).  Gated behind the conv-bwd opt.
  if (uop_is_invalid(else_v) && uop_valid_fold_enabled()
      && term_tag(cond) == TAG_UOP && term_ext(cond) == UOP_IAND) {
    Term clauses[16];
    u32 nclauses = uop_valid_split_and(cond, clauses, 16, 0);
    if (nclauses >= 2) {
      Term kept = 0;
      int dropped = 0;
      for (u32 i = 0; i < nclauses; i++) {
        if (uop_term_shares_range_with(clauses[i], then_v, 0)) {
          kept = (kept == 0) ? clauses[i]
                             : uop_int_binary(UOP_IAND, kept, clauses[i]);
        } else {
          dropped = 1;
        }
      }
      if (dropped && kept != 0 && kept != cond) {
        return uop_iwhere(kept, then_v, else_v);
      }
    }
  }
  // gated_given_valid (tinygrad symbolic.py:408-412): when the gate is a
  // valid mask (`WHERE(cond, body, INVALID)`), simplify `body` against
  // the bounds `cond` asserts.  Gated behind the conv-bwd opt so the
  // shared index simplifier stays bit-identical with the flag OFF.
  if (uop_is_invalid(else_v) && uop_valid_fold_enabled()
      && term_tag(then_v) == TAG_UOP) {
    u32 to = term_ext(then_v);
    if (to == UOP_IDIV || to == UOP_IMOD || to == UOP_IADD
        || to == UOP_IMUL || to == UOP_ISUB) {
      Term folded = uop_given_valid(cond, then_v);
      if (folded != then_v) return uop_iwhere(cond, folded, else_v);
    }
  }
  return 0;
}

// Recursion-depth guard around the iwhere simplifier.  Several folds
// above re-wrap via uop_iwhere -> uop_simplify_iwhere (the IWHERE-of-
// IWHERE collapse, drop_and_clauses, and especially gated_given_valid,
// which substitutes the gate's bounds into the body via
// uop_index_substitute).  On the fused conv-backward col2im index the
// CUDA symbolic-codegen path drives gated_given_valid without reaching a
// fixpoint (each pass yields a different `folded`), so the
// uop_iwhere<->uop_simplify_iwhere<->uop_index_substitute cycle grows
// the stack without bound (SIGSEGV).  Bail to the raw (un-folded) IWHERE
// once nesting is pathological -- always correct, just a missed
// simplification; the schedule-level realize keeps the kernel feasible.
fn Term uop_simplify_iwhere(Term cond, Term then_v, Term else_v) {
  static _Thread_local u32 depth = 0;
  if (depth > 96) return 0;
  depth++;
  Term r = uop_simplify_iwhere_body(cond, then_v, else_v);
  depth--;
  return r;
}
