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
      break;
    case UOP_ISUB:
      if (b_const && bv == 0) return a;
      if (a == b)             return uop_iconst(0);
      break;
    case UOP_IMUL:
      if (a_const && av == 0) return uop_iconst(0);
      if (b_const && bv == 0) return uop_iconst(0);
      if (a_const && av == 1) return b;
      if (b_const && bv == 1) return a;
      break;
    case UOP_IDIV:
      if (b_const && bv == 1) return a;
      if (a_const && av == 0) return uop_iconst(0);
      if (a == b && b_const && bv != 0) return uop_iconst(1);
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
  return 0;
}
