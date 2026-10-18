// Constructor-time rewrites: each uop_rewrite_* returns a simplified Term, or 0
// to build the node as asked.  A rule may drop a CONST operand, keeping the
// other's shape, but mul(x, 0) stays: a {1} CONST would replace x's shape.

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

fn Term uop_rewrite_binary(u32 opcode, Term a, Term b) {
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

  switch (opcode) {
    case UOP_ADD:
      if (uop_const_is_zero_f32(b)) return a;
      if (uop_const_is_zero_f32(a)) return b;
      break;
    case UOP_MUL:
      if (uop_const_is_one_f32(b)) return a;
      if (uop_const_is_one_f32(a)) return b;
      break;
    // x < x and x == x fold to a {1} CONST all the same, which only an
    // elementwise consumer broadcasts back to x's shape.
    case UOP_CMPLT:
      if (a == b) return uop_const(DT_FP32, f32_bits(0.0f));
      break;
    case UOP_CMPEQ:
      if (a == b) return uop_const(DT_FP32, f32_bits(1.0f));
      break;
    default: break;
  }
  return 0;
}

fn Term uop_rewrite_unary(u32 opcode, Term src) {
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

  if (term_tag(src) == TAG_UOP) {
    u32 inner_op = term_ext(src);
    if ((opcode == UOP_NEG   && inner_op == UOP_NEG) ||
        (opcode == UOP_RECIP && inner_op == UOP_RECIP)) {
      return heap_read(term_val(src));
    }
  }
  return 0;
}

// For RESHAPE(RESHAPE(x)) or EXPAND(EXPAND(x)), the x to build the outer op at
// (its shape overrides the inner one); 0 otherwise.
fn Term uop_rewrite_movement_src(u32 outer_op, Term src) {
  if (term_tag(src) != TAG_UOP) return 0;
  u32 inner_op = term_ext(src);
  if (outer_op == UOP_RESHAPE && inner_op == UOP_RESHAPE) {
    return heap_read(term_val(src));
  }
  if (outer_op == UOP_EXPAND && inner_op == UOP_EXPAND) {
    return heap_read(term_val(src));
  }
  return 0;
}
