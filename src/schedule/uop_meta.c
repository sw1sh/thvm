// schedule/uop_meta.c - leaf utilities the rest of the schedule
// pipeline depends on: per-op arity, elementwise predicates, and
// best-effort shape/dtype inference.
//
// Pulled out of materialize.c so bufferize_classify and consumer_count
// (which use uop_arity) can be included BEFORE materialize.c -- which
// in turn calls bufferize_classify to build the boundary set before
// emitting kernels.

fn u8 uop_arity(u8 op) {
  switch (op) {
    // Atoms whose heap holds only NUM cells (or nothing): zero
    // recursable Term children.
    case UOP_CONST:
    case UOP_RANGE:    // heap = [NUM(axis_id), NUM(axis_type), NUM(extent)]
    case UOP_INVALID:  // heap = [NUM(0)] sentinel
      return 0;
    // UOP_OPT carries [target, NUM(kind), NUM(factor)]; only the
    // target slot is a recursable Term child.
    case UOP_OPT:
      return 1;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
    case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
    case UOP_REDUCE:  case UOP_LOAD:
    case UOP_CAST:    case UOP_BITCAST:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_ASSIGN:
    // Symbolic-INDEX layer integer ALU + INDEX_E.  Heap = [a, b]
    // for the IX_* ops, [buffer, addr] for INDEX_E.  Both slots are
    // recursable Term children that the rewriter must descend into
    // to reach UOP_RANGE leaves nested inside production lifter
    // output (UOP_INDEX_E.addr -> IADD/IMUL chain -> UOP_RANGE).
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_INDEX_E:
      return 2;
    // Ternary symbolic ops.  IWHERE = [cond, then, else];
    // STORE = [buf, addr, value].  All three slots are Term
    // children; covering them lets the rewriter walk from the
    // STORE root all the way down to UOP_RANGE leaves.
    case UOP_IWHERE:
    case UOP_STORE:
      return 3;
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
  // TUOpGradWithTarget chain-rule projection: cell is [y, gy, target];
  // the gradient result has y's dtype (= gy's dtype = leaf's dtype).
  // Without this branch, materialize falls through to DT_FP32 here and
  // higher-order TGrad on non-f32 inputs lands at the wrong dtype.
  if ((tag == TAG_DP0 || tag == TAG_DP1)
      && (term_ext(t) & DUP_GRAD_FLAG) != 0) {
    Term y = heap_read(term_val(t));
    return term_dtype_in(y, env_id, out);
  }
  *out = DT_FP32; return 1;
}

typedef struct {
  Term src;
  u32  kind;
  u32  n_reduces;
  u64  locs[MAX_DIM];
  u32  axis_start;
  u32  axis_end;
  u32  inner;
  u32  axis_size;
  u64  out_numel;
  u8   src_ndim;
  u8   out_ndim;
  u8   n_reduce_axes;
  u32  src_dims[MAX_DIM];
  u32  out_dims[MAX_DIM];
  u8   reduce_axes[MAX_DIM];
} ReduceChainInfo;

// Returns u64 so shapes whose product exceeds 2^32 (e.g. BS=512
// conv-bwd dInput at [32,800,204800] = 5.24e9 elements) don't get
// truncated.  The cast-to-u32 it formerly performed silently
// overflowed; callers now read u64 and gate u32-typed downstream
// fields explicitly.
static u64 shape_numel_u32(Shape const *s) {
  u64 n = 1;
  for (u32 i = 0; i < s->ndim; i++) n *= (u64)s->dims[i];
  return n;
}

// Collapses a chain of same-kind reductions into the equivalent
// single contiguous-axis reduction accepted by KProgOp REDUCE.  For
// SUM, require the original axes to be consumed from inner to outer
// so the fused loop preserves the old row-major addition order.
static int reduce_chain_collect(Term root, ReduceChainInfo *out) {
  memset(out, 0, sizeof(*out));
  root = term_resolve(root);
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_REDUCE) return 0;

  Term cur = root;
  u32  axes_outer[MAX_DIM] = {0};
  u32  n = 0;
  u32  kind = 0xFFFFFFFFu;
  while (term_tag(cur) == TAG_UOP && term_ext(cur) == UOP_REDUCE) {
    if (n >= MAX_DIM) return 0;
    u64 loc = term_val(cur);
    Term k_term = heap_read(loc + 1);
    Term a_term = heap_read(loc + 2);
    if (term_tag(k_term) != TAG_NUM || term_tag(a_term) != TAG_NUM) return 0;
    u32 k = (u32)term_val(k_term);
    if (kind == 0xFFFFFFFFu) kind = k;
    else if (kind != k) return 0;
    out->locs[n] = loc;
    axes_outer[n] = (u32)term_val(a_term);
    n++;
    cur = term_resolve(heap_read(loc));
  }
  if (n < 2) return 0;

  Shape src_shape = {0};
  Shape out_shape = {0};
  if (!term_shape_in(cur, 0, &src_shape))  return 0;
  if (!term_shape_in(root, 0, &out_shape)) return 0;
  if (src_shape.ndim > MAX_DIM || out_shape.ndim > MAX_DIM) return 0;

  u32 live_axes[MAX_DIM] = {0};
  for (u32 i = 0; i < src_shape.ndim; i++) live_axes[i] = i;
  u32 live_n = src_shape.ndim;
  u32 removed[MAX_DIM] = {0};
  for (u32 oi = 0; oi < n; oi++) {
    u32 ri = n - 1 - oi;
    u32 axis = axes_outer[ri];
    if (live_n == 0 || axis >= live_n) return 0;
    removed[oi] = live_axes[axis];
    for (u32 j = axis + 1; j < live_n; j++) {
      live_axes[j - 1] = live_axes[j];
    }
    live_n--;
  }

  if (live_n != out_shape.ndim) return 0;
  for (u32 i = 0; i < live_n; i++) {
    if (src_shape.dims[live_axes[i]] != out_shape.dims[i]) return 0;
  }

  u32 min_axis = removed[0];
  u32 max_axis = removed[0];
  for (u32 i = 1; i < n; i++) {
    if (removed[i] < min_axis) min_axis = removed[i];
    if (removed[i] > max_axis) max_axis = removed[i];
  }
  u8 seen[MAX_DIM] = {0};
  for (u32 i = 0; i < n; i++) {
    if (removed[i] >= MAX_DIM || seen[removed[i]]) return 0;
    seen[removed[i]] = 1;
  }
  // Fusion contract: the rangeify body emit (src/schedule/rangeify.c)
  // collapses a multi-axis reduce into ONE flattened reduce RANGE of
  // extent prod(per-axis extents) and relies on row-major arithmetic
  // to visit the (r0 * ext1 * ... + r1 * ... + rk) block.  That trick
  // only stays correct when the reduced axes are the CONTIGUOUS
  // TRAILING axes of the pre-reduce input -- otherwise the inner
  // address computation skips the non-reduced axis sandwiched between
  // reduced ones.  For non-trailing or non-contiguous reduce chains
  // (e.g. batchnorm's reduce(B,H,W) over {B,C,H,W} = axes {0,2,3}),
  // fall back to one kernel per reduce.
  if (max_axis - min_axis + 1 != n) return 0;
  if (max_axis != src_shape.ndim - 1) return 0;

  if (kind == REDUCE_SUM) {
    for (u32 i = 1; i < n; i++) {
      if (removed[i - 1] <= removed[i]) return 0;
    }
  }

  u64 axis_size = 1;
  for (u32 i = 0; i < n; i++) axis_size *= src_shape.dims[removed[i]];
  u64 inner = 1;
  for (u32 i = max_axis + 1; i < src_shape.ndim; i++) inner *= src_shape.dims[i];
  u64 out_numel = shape_numel_u32(&out_shape);
  u64 src_numel = shape_numel_u32(&src_shape);
  if (axis_size == 0 || inner == 0 || inner > 0x00FFFFFFu) return 0;
  if (out_numel == 0 || axis_size * out_numel != src_numel) return 0;
  if (axis_size > UINT32_MAX || out_numel > UINT32_MAX) return 0;

  out->src        = cur;
  out->kind       = kind;
  out->n_reduces  = n;
  out->axis_start = min_axis;
  out->axis_end   = max_axis + 1;
  out->inner      = (u32)inner;
  out->axis_size  = (u32)axis_size;
  out->out_numel  = out_numel;
  out->src_ndim   = (u8)src_shape.ndim;
  out->out_ndim   = (u8)out_shape.ndim;
  out->n_reduce_axes = (u8)n;
  for (u32 i = 0; i < src_shape.ndim; i++) {
    out->src_dims[i] = src_shape.dims[i];
  }
  for (u32 i = 0; i < out_shape.ndim; i++) {
    out->out_dims[i] = out_shape.dims[i];
  }
  u32 sorted_axes[MAX_DIM];
  for (u32 i = 0; i < n; i++) {
    sorted_axes[i] = removed[i];
  }
  for (u32 i = 0; i < n; i++) {
    for (u32 j = i + 1; j < n; j++) {
      if (sorted_axes[j] < sorted_axes[i]) {
        u32 tmp = sorted_axes[i];
        sorted_axes[i] = sorted_axes[j];
        sorted_axes[j] = tmp;
      }
    }
  }
  for (u32 i = 0; i < n; i++) {
    out->reduce_axes[i] = (u8)sorted_axes[i];
  }
  return 1;
}
