// schedule/uop_meta.c - per-op arity, elementwise predicates, best-effort
// shape, dtype and device inference, and the reduce-chain matcher.

fn u8 uop_arity(u8 op) {
  switch (op) {
    // Atoms: the heap holds only NUM cells, no Term children.
    case UOP_CONST:
    case UOP_RANGE:    // heap = [NUM(axis_id), NUM(axis_type), NUM(extent)]
    case UOP_INVALID:  // heap = [NUM(0)] sentinel
    case UOP_DEFINE_VAR:  // heap = [NUM(packed_extent)]
      return 0;
    // OPT = [target, NUM(kind), NUM(factor)].
    case UOP_OPT:
      return 1;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT: case UOP_SIN:
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
    case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
    case UOP_REDUCE:  case UOP_LOAD:    case UOP_DETACH:
    case UOP_CAST:    case UOP_BITCAST:
    // COPY = [src, NUM(device + 1)].
    case UOP_COPY:
    // BUFFERIZE = [value, NUM(addrspace), NUM(removable), NUM(n_ranges),
    // range_0, ...]: like RESHAPE's [src, NUM(ndim), NUM(d0), ...], only the
    // first slot counts.
    case UOP_BUFFERIZE:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_POW: case UOP_THREEFRY: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_ASSIGN:
    // The index-layer integer ops = [a, b] and INDEX_E = [buffer, addr];
    // walkers descend both to reach the RANGE leaves of an address.
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR: case UOP_ISHL:
    case UOP_INDEX_E:
    // GATHER = [src, idx, NUM(dim)]: the trailing NUM is payload.
    case UOP_GATHER:
      return 2;
    // IWHERE = [cond, then, else], STORE = [buf, addr, value].
    case UOP_IWHERE:
    case UOP_STORE:
    // SCATTER = [base, idx, src, NUM(dim), NUM(kind), NUM(include_self)].
    case UOP_SCATTER:
      return 3;
    default:
      return 0;
  }
}

fn u8 uop_is_unary_elementwise(u8 op) {
  return op == UOP_NEG || op == UOP_RECIP || op == UOP_EXP2
      || op == UOP_LOG2 || op == UOP_SQRT || op == UOP_SIN;
}

// The int divide and bit ops are also index-layer ops; those sit inside
// INDEX_E addresses, never as scheduled value nodes, as for IWHERE below.
fn u8 uop_is_binary_elementwise(u8 op) {
  return op == UOP_ADD  || op == UOP_MUL || op == UOP_CMPLT || op == UOP_CMPEQ
      || op == UOP_POW  || op == UOP_IDIV || op == UOP_IMOD || op == UOP_IAND || op == UOP_IOR
      || op == UOP_IXOR || op == UOP_ISHL || op == UOP_ISHR || op == UOP_THREEFRY;
}

// IWHERE doubles as tinygrad's value-graph Ops.WHERE.  Its index-layer uses
// sit inside INDEX_E addresses, never as scheduled value nodes, so calling
// it elementwise here does not touch them.
fn u8 uop_is_ternary_elementwise(u8 op) {
  return op == UOP_IWHERE;
}

// Memo for one outermost term_shape_in call, without which the recursion
// re-walks shared subgraphs exponentially.  Open-addressed and keyed by the
// packed Term (the `loc` field); each outermost call starts a new
// generation, which empties the table without a memset.
#define SHAPE_CACHE_CAP 8192            // power of two
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
  // Table full: the entry is dropped.
}

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
  // Only UOP and TEN terms are cached; the other tags are cheap.
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

// Broadcast as tinygrad's spec defines it (spec/tinyspec.tex:283): right-align
// the shapes and take the per-axis max.
static void shape_broadcast2(Shape const *a, Shape const *b, Shape *out) {
  u32 nd = a->ndim > b->ndim ? a->ndim : b->ndim;
  for (u32 i = 0; i < MAX_DIM; i++) out->dims[i] = 0;
  out->ndim = nd;
  for (u32 i = 0; i < nd; i++) {
    // The i-th axis from the end; a missing axis is 1.
    u32 da = (i < a->ndim) ? a->dims[a->ndim - 1 - i] : 1;
    u32 db = (i < b->ndim) ? b->dims[b->ndim - 1 - i] : 1;
    out->dims[nd - 1 - i] = da >= db ? da : db;
  }
}

static int term_shape_in_uncached(Term t, u32 env_id, Shape *out) {
  (void)env_id;
  // A VAR not yet substituted (before APP-LAM beta) has the shape its LAM
  // was annotated with (TLamShape; the lam_shape table, keyed by LAM loc).
  if (term_tag(t) == TAG_VAR) {
    u64 loc = term_val(t);
    Term cell = heap_read(loc);
    if (!term_sub_get(cell)) {
      Shape s;
      if (lam_shape_lookup(loc, &s)) { *out = s; return 1; }
    }
  }
  // Resolve as visit() (materialize.c) does, so a substituted VAR yields
  // its argument.
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
      || op == UOP_CAST || op == UOP_BITCAST || op == UOP_DETACH
      || op == UOP_COPY) {
    return term_shape_in(heap_read(loc), 0, out);
  }
  // BUFFERIZE has its value's shape, as tinygrad's STAGE does
  // (schedule/indexing.py:92): the trailing ranges index the buffer.
  if (op == UOP_BUFFERIZE) {
    return term_shape_in(heap_read(loc), 0, out);
  }
  // GATHER has idx's shape (slot 1); SCATTER has base's (slot 0).
  if (op == UOP_GATHER) {
    return term_shape_in(heap_read(loc + 1), 0, out);
  }
  if (op == UOP_SCATTER) {
    return term_shape_in(heap_read(loc + 0), 0, out);
  }
  if (uop_is_binary_elementwise(op)) {
    Shape la, lb; int la_ok = term_shape_in(heap_read(loc + 0), 0, &la);
    int lb_ok = term_shape_in(heap_read(loc + 1), 0, &lb);
    if (!la_ok && !lb_ok) return 0;
    if (!la_ok) { *out = lb; return 1; }
    if (!lb_ok) { *out = la; return 1; }
    shape_broadcast2(&la, &lb, out); return 1;
  }
  if (uop_is_ternary_elementwise(op)) {
    // The broadcast of cond, then and else.
    Shape acc; int have = 0;
    for (u32 k = 0; k < 3; k++) {
      Shape s;
      if (!term_shape_in(heap_read(loc + k), 0, &s)) continue;
      if (!have) { acc = s; have = 1; } else { Shape t2 = acc; shape_broadcast2(&t2, &s, &acc); }
    }
    if (!have) return 0;
    *out = acc; return 1;
  }
  if (op == UOP_CONST || op == UOP_DEFINE_VAR) {
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
    // Drops every reduced axis, as tinygrad's REDUCE shape does
    // (uop/ops.py:433-437).
    if (cs.ndim == 0) return 0;
    Term t = term_new(0, TAG_UOP, op, loc);
    u32 n_axes = uop_reduce_n_axes(t);
    u8 drop[MAX_DIM] = {0};
    for (u32 ai = 0; ai < n_axes; ai++) {
      u32 ax = uop_reduce_axis(t, ai);
      if (ax < cs.ndim) drop[ax] = 1;
    }
    u32 dst = 0;
    for (u32 i = 0; i < cs.ndim; i++) {
      if (drop[i]) continue;
      out->dims[dst++] = cs.dims[i];
    }
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
    if (op == UOP_DEFINE_VAR) {
      *out = DT_INT32;
      return 1;
    }
    if (op == UOP_CONST) {
      // CONST = [NUM(bits)] with the dtype in the NUM's ext.
      Term num = heap_read(loc);
      if (term_tag(num) == TAG_NUM) { *out = term_ext(num); return 1; }
    }
    if (op == UOP_CAST || op == UOP_BITCAST) {
      // [src, NUM(dst_dtype)]
      Term num = heap_read(loc + 1);
      if (term_tag(num) == TAG_NUM) { *out = (u32)term_val(num); return 1; }
    }
    // IWHERE has its then-branch's dtype, not the mask's.
    if (uop_is_ternary_elementwise(op)) {
      return term_dtype_in(heap_read(loc + 1), env_id, out);
    }
    // These take src[0]'s dtype (GATHER's src, SCATTER's base); binary
    // operands must share it, as tinygrad's UOps do not promote.
    if (uop_is_unary_elementwise(op) || uop_is_binary_elementwise(op)
        || op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
        || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP
        || op == UOP_REDUCE  || op == UOP_LOAD    || op == UOP_ASSIGN
        || op == UOP_BUFFERIZE || op == UOP_COPY
        || op == UOP_GATHER  || op == UOP_SCATTER) {
      Term src0 = heap_read(loc);
      return term_dtype_in(src0, env_id, out);
    }
    // INDEX_E has its buffer's dtype.  The f32 default would trip
    // uop_bitcast's itemsize check when graph_rewrite rebuilds a sub-32-bit
    // BITCAST over it (f16 -> u16, fp8 -> u8).
    if (op == UOP_INDEX_E) {
      Term buf = heap_read(loc + 0);
      return term_dtype_in(buf, env_id, out);
    }
    // BUFFER keeps its dtype at heap[loc + 1]; the f32 default would break
    // dtype-sensitive folds, as for INDEX_E.
    if (op == UOP_BUFFER) {
      *out = uop_buffer_dtype(t);
      return 1;
    }
    // AFTER = [node, after_node] has node's dtype.
    if (op == UOP_AFTER) {
      return term_dtype_in(heap_read(loc), env_id, out);
    }
  }
  // A grad projection's cell is [y, gy, target] and its result has y's dtype;
  // the f32 default would mistype a higher-order grad of non-f32 inputs.
  if ((tag == TAG_DP0 || tag == TAG_DP1)
      && (term_ext(t) & DUP_GRAD_FLAG) != 0) {
    Term y = heap_read(term_val(t));
    return term_dtype_in(y, env_id, out);
  }
  *out = DT_FP32; return 1;
}

// The THVM_DEV_* code of a backend, or -1.  Backend.id (CPU=1, METAL=2,
// CUDA=3) is a different numbering, so match the backend singletons.
static i32 backend_device_code(Backend *b) {
  if (b == NULL) return -1;
  if (b == &CPU_BACKEND)   return THVM_DEV_CPU;
  if (b == &METAL_BACKEND) return THVM_DEV_METAL;
#ifdef THVM_HAS_CUDA
  if (b == &CUDA_BACKEND)  return THVM_DEV_CUDA;
#endif
  return -1;
}

// Device of a term (THVM_DEV_*), or -1 for none, after tinygrad's UOp.device
// (uop/ops.py:895-908): a COPY names its target, a TEN its backend, and any
// other op its first source that has one, so a mixed-device graph reports the
// first device found.  thvm_realize picks its backend with it.
fn i32 term_device_in(Term t) {
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    if (tid != 0 && tid < TENS_NEXT) return backend_device_code(TENS[tid].backend);
    return -1;
  }
  if (tag != TAG_UOP) return -1;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_COPY) {
    i32 dev = uop_copy_device(loc);
    if (dev >= 0) return dev;
    return term_device_in(heap_read(loc));    // a generic COPY: follow src
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    i32 d = term_device_in(heap_read(loc + i));
    if (d >= 0) return d;
  }
  return -1;
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

// u64: a numel can pass 2^32, so callers range-check before narrowing.
static u64 shape_numel_u32(Shape const *s) {
  u64 n = 1;
  for (u32 i = 0; i < s->ndim; i++) n *= (u64)s->dims[i];
  return n;
}

// Describes in *out the single REDUCE equivalent to the chain of same-kind
// REDUCEs at root, which visit() (materialize.c) then emits as one kernel;
// 0 when there is none.  A SUM or PROD chain must consume its axes inner to outer,
// so the fused loop keeps the chain's row-major combine order.
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
    if (term_tag(k_term) != TAG_NUM) return 0;
    u32 k = (u32)term_val(k_term);
    if (kind == 0xFFFFFFFFu) kind = k;
    else if (kind != k) return 0;
    // A multi-axis REDUCE adds its axes in builder order, as one block of
    // the outer-to-inner list.
    u32 cur_n = uop_reduce_n_axes(cur);
    for (u32 ai = 0; ai < cur_n; ai++) {
      if (n >= MAX_DIM) return 0;
      out->locs[n] = loc;
      axes_outer[n] = uop_reduce_axis(cur, ai);
      n++;
    }
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
  // Only a chain over the contiguous trailing axes of its source fuses
  // (batchnorm's reduce over axes {0,2,3} of {B,C,H,W} does not); any other
  // chain emits one kernel per REDUCE.
  if (max_axis - min_axis + 1 != n) return 0;
  if (max_axis != src_shape.ndim - 1) return 0;

  if (kind != REDUCE_MAX) {
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
