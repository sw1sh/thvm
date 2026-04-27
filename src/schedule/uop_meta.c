// schedule/uop_meta.c - leaf utilities the rest of the schedule
// pipeline depends on: per-op arity, elementwise predicates, and
// best-effort shape/dtype inference.
//
// Pulled out of materialize.c so realize_classify and consumer_count
// (which use uop_arity) can be included BEFORE materialize.c -- which
// in turn calls realize_classify to build the boundary set before
// emitting kernels.

fn u8 uop_arity(u8 op) {
  switch (op) {
    case UOP_CONST:
      return 0;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
    case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
    case UOP_REDUCE:  case UOP_LOAD:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_ASSIGN:
      return 2;
    default:
      return 0;
  }
}

fn u8 uop_is_unary_elementwise(u8 op) {
  return op == UOP_NEG || op == UOP_RECIP || op == UOP_EXP2
      || op == UOP_LOG2 || op == UOP_SQRT;
}

fn u8 uop_is_binary_elementwise(u8 op) {
  return op == UOP_ADD || op == UOP_MUL || op == UOP_CMPLT || op == UOP_CMPEQ;
}

// Best-effort shape inference used by interact_grad to figure out
// the shape of an upstream UOp/TEN at GRAD-unroll time.  Pure
// function over the heap; does NOT consult any shape environment
// (callers always pass env_id=0).  The g2 rewrite may replace this
// with a one-pass shape annotation, but keeping the recursion here
// is the smallest surface that lets interact_grad keep working.
fn int term_shape_in(Term t, u32 env_id, Shape *out) {
  (void)env_id;
  // Same VAR/ALO resolve as visit() in materialize.c -- shape inference
  // walking through a beta-substituted body needs to see the bound
  // arg's TEN, not the bare VAR cell.
  t = term_resolve(t);
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    if (tid != 0 && tid < TENS_NEXT) { *out = TENS[tid].view.shape; return 1; }
    return 0;
  }
  if (tag != TAG_UOP) return 0;
  u8  op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_KERNEL) {
    Term outbuf = heap_read(loc);
    if (term_tag(outbuf) == TAG_TEN) {
      u32 tid = (u32)term_val(outbuf);
      if (tid != 0 && tid < TENS_NEXT) { *out = TENS[tid].view.shape; return 1; }
    }
    return 0;
  }
  if (uop_is_unary_elementwise(op) || op == UOP_LOAD || op == UOP_FLIP) {
    return term_shape_in(heap_read(loc), 0, out);
  }
  if (uop_is_binary_elementwise(op)) {
    Shape la, lb; int la_ok = term_shape_in(heap_read(loc + 0), 0, &la);
    int lb_ok = term_shape_in(heap_read(loc + 1), 0, &lb);
    if (!la_ok && !lb_ok) return 0;
    if (!la_ok) { *out = lb; return 1; }
    if (!lb_ok) { *out = la; return 1; }
    u32 na = 1, nb = 1;
    for (u32 i = 0; i < la.ndim; i++) na *= la.dims[i];
    for (u32 i = 0; i < lb.ndim; i++) nb *= lb.dims[i];
    *out = (na >= nb) ? la : lb; return 1;
  }
  if (op == UOP_CONST) {
    out->ndim = 1; out->dims[0] = 1;
    for (u32 i = 1; i < MAX_DIM; i++) out->dims[i] = 0;
    return 1;
  }
  if (op == UOP_RESHAPE || op == UOP_EXPAND) {
    u32 ndim = (u32)term_val(heap_read(loc + 1));
    out->ndim = ndim;
    for (u32 i = 0; i < ndim && i < MAX_DIM; i++)
      out->dims[i] = (u32)term_val(heap_read(loc + 2 + i));
    for (u32 i = ndim; i < MAX_DIM; i++) out->dims[i] = 0;
    return 1;
  }
  if (op == UOP_PAD || op == UOP_SHRINK) {
    Shape cs; if (!term_shape_in(heap_read(loc), 0, &cs)) return 0;
    out->ndim = cs.ndim;
    for (u32 i = 0; i < cs.ndim; i++) {
      u32 b = (u32)term_val(heap_read(loc + 1 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 2 + 2 * i));
      out->dims[i] = (op == UOP_PAD) ? cs.dims[i] + b + e
                                     : ((e > b) ? (e - b) : 0);
    }
    for (u32 i = cs.ndim; i < MAX_DIM; i++) out->dims[i] = 0;
    return 1;
  }
  if (op == UOP_PERMUTE) {
    Shape cs; if (!term_shape_in(heap_read(loc), 0, &cs)) return 0;
    out->ndim = cs.ndim;
    for (u32 i = 0; i < cs.ndim; i++) {
      u32 pi = (u32)term_val(heap_read(loc + 1 + i));
      out->dims[i] = cs.dims[pi];
    }
    for (u32 i = cs.ndim; i < MAX_DIM; i++) out->dims[i] = 0;
    return 1;
  }
  if (op == UOP_REDUCE) {
    Shape cs; if (!term_shape_in(heap_read(loc), 0, &cs)) return 0;
    u32 axis = (u32)term_val(heap_read(loc + 2));
    if (cs.ndim <= 1) {
      out->ndim = 1; out->dims[0] = 1;
      for (u32 i = 1; i < MAX_DIM; i++) out->dims[i] = 0; return 1;
    }
    u32 dst = 0;
    for (u32 i = 0; i < cs.ndim; i++) { if (i == axis) continue; out->dims[dst++] = cs.dims[i]; }
    out->ndim = dst;
    for (u32 i = dst; i < MAX_DIM; i++) out->dims[i] = 0;
    return 1;
  }
  return 0;
}

fn int term_dtype_in(Term t, u32 env_id, u32 *out) {
  (void)env_id;
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    if (tid != 0 && tid < TENS_NEXT) { *out = TENS[tid].dtype; return 1; }
  }
  if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
    Term outbuf = heap_read(term_val(t));
    if (term_tag(outbuf) == TAG_TEN) {
      u32 tid = (u32)term_val(outbuf);
      if (tid != 0 && tid < TENS_NEXT) { *out = TENS[tid].dtype; return 1; }
    }
  }
  *out = DT_F32; return 1;
}
