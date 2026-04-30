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
    case UOP_CAST:    case UOP_BITCAST:
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

// Per-call memoization for term_shape_in.  Without it, recursive
// shape inference re-walked shared subgraphs exponentially -- the
// dominant cost of materialize for the bound-w SGD pattern (where
// each iter's gradient references the same target/lr UOPs).
// Loc-keyed open-addressed hash; outer entry assigns a fresh
// generation, so we never need to memset 8K slots.  Cell holds
// (gen, loc, shape, valid) where valid=0 means cached-negative.
#define SHAPE_CACHE_CAP 8192            // power of two, > deepest UOP DAG
typedef struct {
  u32   gen;
  u32   valid;     // 0 = cached negative result, 1 = positive
  u64   loc;
  Shape shape;
} ShapeCacheSlot;
static ShapeCacheSlot SHAPE_CACHE[SHAPE_CACHE_CAP];
static u32 SHAPE_CACHE_DEPTH = 0;       // re-entrant guard
static u32 SHAPE_CACHE_GEN   = 0;       // monotonic; 0 = invalid

static inline u32 shape_cache_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (SHAPE_CACHE_CAP - 1);
}

// Returns: 0 = miss, 1 = positive hit (out = shape), -1 = negative hit.
static int shape_cache_lookup(u64 loc, Shape *out) {
  u32 h = shape_cache_hash(loc);
  for (u32 probe = 0; probe < SHAPE_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (SHAPE_CACHE_CAP - 1);
    ShapeCacheSlot *s = &SHAPE_CACHE[i];
    if (s->gen != SHAPE_CACHE_GEN) return 0;     // empty (slot belongs to past gen)
    if (s->loc == loc) {
      if (s->valid) { *out = s->shape; return 1; }
      return -1;
    }
  }
  return 0;
}

static void shape_cache_insert(u64 loc, Shape const *shape, int valid) {
  u32 h = shape_cache_hash(loc);
  for (u32 probe = 0; probe < SHAPE_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (SHAPE_CACHE_CAP - 1);
    ShapeCacheSlot *s = &SHAPE_CACHE[i];
    if (s->gen != SHAPE_CACHE_GEN || s->loc == loc) {
      s->gen   = SHAPE_CACHE_GEN;
      s->loc   = loc;
      s->valid = valid ? 1 : 0;
      if (valid) s->shape = *shape;
      return;
    }
  }
  // table full -- silently drop, caller falls back to recursion
}

// term_shape_in_inner: the recursive worker.  Outer entry bumps
// the generation so prior cache entries become invisible without
// the cost of a memset.
static int term_shape_in_inner(Term t, u32 env_id, Shape *out);

fn int term_shape_in(Term t, u32 env_id, Shape *out) {
  if (SHAPE_CACHE_DEPTH == 0) {
    SHAPE_CACHE_GEN++;
    if (SHAPE_CACHE_GEN == 0) SHAPE_CACHE_GEN = 1;   // skip 0 sentinel on wrap
  }
  SHAPE_CACHE_DEPTH++;
  int ok = term_shape_in_inner(t, env_id, out);
  SHAPE_CACHE_DEPTH--;
  return ok;
}

static int term_shape_in_uncached(Term t, u32 env_id, Shape *out);

static int term_shape_in_inner(Term t, u32 env_id, Shape *out) {
  // Cache lookup keyed on the packed Term value.  Skip for atoms /
  // tags whose val isn't a heap loc; for those the recursion bottom
  // is cheap anyway.
  u8 tag0 = term_tag(t);
  int cacheable = (tag0 == TAG_UOP || tag0 == TAG_TEN);
  if (cacheable) {
    int hit = shape_cache_lookup((u64)t, out);
    if (hit == 1) return 1;
    if (hit == -1) return 0;
  }
  int ok = term_shape_in_uncached(t, env_id, out);
  if (cacheable) shape_cache_insert((u64)t, out, ok);
  return ok;
}

static int term_shape_in_uncached(Term t, u32 env_id, Shape *out) {
  (void)env_id;
  // Bound-var shape: a TLam can carry a shape annotation for its
  // bound variable (registered via TLamShape in the WL surface,
  // stored in the lam_shape table keyed by the LAM's heap loc).
  // When a TVAR points at that loc and the cell hasn't been
  // SUB-substituted yet (pre APP-LAM beta), look up the shape.
  // After substitution term_resolve below would unwrap to the
  // arg's TEN and the existing tag-dispatch would handle it.
  if (term_tag(t) == TAG_VAR) {
    u64 loc = term_val(t);
    Term cell = heap_read(loc);
    if (!term_sub_get(cell)) {
      Shape s;
      if (lam_shape_lookup(loc, &s)) { *out = s; return 1; }
    }
  }
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
  if (uop_is_unary_elementwise(op) || op == UOP_LOAD || op == UOP_FLIP
      || op == UOP_CAST || op == UOP_BITCAST) {
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
      u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
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
      u32 pi = (u32)term_val(heap_read(loc + 2 + i));
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
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    if (tid != 0 && tid < TENS_NEXT) { *out = TENS[tid].dtype; return 1; }
  }
  if (tag == TAG_UOP) {
    u32 op  = term_ext(t);
    u64 loc = term_val(t);
    if (op == UOP_KERNEL) {
      Term outbuf = heap_read(loc);
      if (term_tag(outbuf) == TAG_TEN) {
        u32 tid = (u32)term_val(outbuf);
        if (tid != 0 && tid < TENS_NEXT) { *out = TENS[tid].dtype; return 1; }
      }
    }
    if (op == UOP_CONST) {
      // The CONST cell carries [NUM(bits)]; the dtype is the NUM's ext.
      Term num = heap_read(loc);
      if (term_tag(num) == TAG_NUM) { *out = term_ext(num); return 1; }
    }
    if (op == UOP_CAST || op == UOP_BITCAST) {
      // dtype lives in the second heap cell as NUM(dst_dtype).
      Term num = heap_read(loc + 1);
      if (term_tag(num) == TAG_NUM) { *out = (u32)term_val(num); return 1; }
    }
    // Elementwise + reduce + movement ops inherit dtype from src[0]
    // (and binary ops require both srcs share a dtype -- the strict
    // policy matches tinygrad's no-implicit-promotion stance).
    if (uop_is_unary_elementwise(op) || uop_is_binary_elementwise(op)
        || op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
        || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP
        || op == UOP_REDUCE  || op == UOP_LOAD    || op == UOP_ASSIGN) {
      Term src0 = heap_read(loc);
      return term_dtype_in(src0, env_id, out);
    }
  }
  *out = DT_FP32; return 1;
}
