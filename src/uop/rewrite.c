// uop/rewrite.c - constructor-time pattern rewrites for UOp graphs.
//
// Each `uop_rewrite_*` inspects the operands of a soon-to-be-built
// UOp and returns either:
//   - a simplified Term (constant fold, identity reduction, etc.),
//     in which case the caller skips the regular construction.
//   - 0, meaning "no rule matched, proceed with normal hash-cons".
//
// Rules stay at the constructor level so every consumer benefits
// without touching schedule / interact / wnf.  Hash-cons (mov_cache)
// runs after rewrite, so simplified forms get the same dedup.
//
// Shape safety: rules that drop a CONST operand keep the surviving
// operand's shape (CONST has shape {1} and broadcasts; x + 0 = x at
// x's shape, etc.).  Rules that would change a tensor's shape (e.g.
// `mul(x, CONST(0)) -> CONST(0)`) are NOT included here -- the
// CONST has shape {1}, the original mul had x's shape, and downstream
// consumers expecting x's shape would break without an EXPAND wrap.

// === CONST inspection ===

static int uop_is_const(Term t) {
  return term_tag(t) == TAG_UOP && term_ext(t) == UOP_CONST;
}

static u32 uop_const_dtype(Term t) {
  return term_ext(heap_read(term_val(t)));
}

static u32 uop_const_bits(Term t) {
  return (u32)term_val(heap_read(term_val(t)));
}

static int uop_const_is_zero_f32(Term t) {
  if (!uop_is_const(t)) return 0;
  if (uop_const_dtype(t) != DT_FP32) return 0;
  f32 v;
  u32 b = uop_const_bits(t);
  memcpy(&v, &b, sizeof v);
  return v == 0.0f;
}

static int uop_const_is_one_f32(Term t) {
  if (!uop_is_const(t)) return 0;
  if (uop_const_dtype(t) != DT_FP32) return 0;
  f32 v;
  u32 b = uop_const_bits(t);
  memcpy(&v, &b, sizeof v);
  return v == 1.0f;
}

static u32 f32_bits(f32 v) { u32 b; memcpy(&b, &v, sizeof b); return b; }
static f32 bits_f32(u32 b) { f32 v; memcpy(&v, &b, sizeof v); return v; }

// === binary rewrites ===

fn Term uop_rewrite_binary(u32 opcode, Term a, Term b) {
  // Constant fold: both operands CONST f32.
  if (uop_is_const(a) && uop_is_const(b)
      && uop_const_dtype(a) == DT_FP32
      && uop_const_dtype(b) == DT_FP32) {
    f32 av = bits_f32(uop_const_bits(a));
    f32 bv = bits_f32(uop_const_bits(b));
    f32 r;
    int folded = 1;
    switch (opcode) {
      case UOP_ADD:   r = av + bv; break;
      case UOP_MUL:   r = av * bv; break;
      case UOP_CMPLT: r = (av < bv) ? 1.0f : 0.0f; break;
      case UOP_CMPEQ: r = (av == bv) ? 1.0f : 0.0f; break;
      default: folded = 0; r = 0;
    }
    if (folded) return uop_const(DT_FP32, f32_bits(r));
  }

  // Identity rules.  `x op CONST(identity) -> x` keeps x's shape;
  // CONST is shape {1} and would broadcast to x's shape anyway.
  switch (opcode) {
    case UOP_ADD:
      if (uop_const_is_zero_f32(b)) return a;
      if (uop_const_is_zero_f32(a)) return b;
      break;
    case UOP_MUL:
      if (uop_const_is_one_f32(b)) return a;
      if (uop_const_is_one_f32(a)) return b;
      break;
    case UOP_CMPLT:
      // x < x -> false everywhere.  Safe even at tensor shapes:
      // CMPLT broadcasts 0 to the input shape, so a CONST(0) at
      // shape {1} reduces to the same all-zero output downstream.
      if (a == b) return uop_const(DT_FP32, f32_bits(0.0f));
      break;
    case UOP_CMPEQ:
      if (a == b) return uop_const(DT_FP32, f32_bits(1.0f));
      break;
    default: break;
  }
  return 0;
}

// === unary rewrites ===

fn Term uop_rewrite_unary(u32 opcode, Term src) {
  // Constant fold.
  if (uop_is_const(src) && uop_const_dtype(src) == DT_FP32) {
    f32 v = bits_f32(uop_const_bits(src));
    f32 r;
    int folded = 1;
    switch (opcode) {
      case UOP_NEG:   r = -v; break;
      case UOP_RECIP: r = 1.0f / v; break;
      case UOP_EXP2:  r = (f32)exp2(v); break;
      case UOP_LOG2:  r = (f32)log2(v); break;
      case UOP_SQRT:  r = (f32)sqrt(v); break;
      default: folded = 0; r = 0;
    }
    if (folded) return uop_const(DT_FP32, f32_bits(r));
  }

  // Self-inverse pairs: f(f(x)) -> x.
  if (term_tag(src) == TAG_UOP) {
    u32 inner_op = term_ext(src);
    if ((opcode == UOP_NEG   && inner_op == UOP_NEG) ||
        (opcode == UOP_RECIP && inner_op == UOP_RECIP)) {
      return heap_read(term_val(src));    // unwrap inner src
    }
  }
  return 0;
}

// === movement-op chain collapse ===
//
// reshape(reshape(x, _), s)  -> reshape(x, s)
// expand(expand(x, _), s)    -> expand(x, s)
//
// Both shape changes terminate at `s`; the intermediate one is
// redundant.  Returns the source term to feed into a fresh
// constructor; caller still allocates the outer reshape/expand
// with its `dims` array, just at the deeper source.

fn Term uop_rewrite_movement_src(u32 outer_op, Term src) {
  if (term_tag(src) != TAG_UOP) return 0;
  u32 inner_op = term_ext(src);
  // RESHAPE-of-RESHAPE collapses (the inner shape is fully overridden).
  if (outer_op == UOP_RESHAPE && inner_op == UOP_RESHAPE) {
    return heap_read(term_val(src));    // src's src
  }
  // EXPAND-of-EXPAND collapses similarly.
  if (outer_op == UOP_EXPAND && inner_op == UOP_EXPAND) {
    return heap_read(term_val(src));
  }
  return 0;
}
