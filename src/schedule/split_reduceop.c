// schedule/split_reduceop.c - tinygrad's split_reduceop (schedule/prepare.py:
// 110-132): a REDUCE of at least 32768 elements into few outputs becomes two.
// The first reduces contiguous chunks of one reduced axis into up to 256
// partials per output, so its kernel has outputs to upcast where one serial
// accumulator would chain every add; the second reduces the partials.  The
// rewrite reaches the new first REDUCE too, as tinygrad's graph_rewrite does.
// SPLIT_REDUCEOP=0 turns it off.

// Axis ids of the probe ranges, far above the ones rangeify hands out.
#define SPLIT_REDUCEOP_AID_BASE 0x70000000u

// The first REDUCEs of the splits, kept realized as tinygrad's .contiguous()
// keeps them: bufferize_classify's reduce-chain unmark would fuse each back
// into its second REDUCE.  The list lives for one hash-cons generation
// (uop/mov_cache.c), the in-place rewrites of one realize, so a materialize
// the realize retries still keeps a REDUCE it split before.
static u64 *SPLIT_REDUCEOP_KEEP     = NULL;
static u32  SPLIT_REDUCEOP_KEEP_N   = 0;
static u32  SPLIT_REDUCEOP_KEEP_CAP = 0;
static u32  SPLIT_REDUCEOP_KEEP_GEN = 0;

fn int split_reduceop_keeps(u64 loc) {
  if (SPLIT_REDUCEOP_KEEP_GEN != UOP_MOV_GEN) return 0;
  for (u32 i = 0; i < SPLIT_REDUCEOP_KEEP_N; i++) {
    if (SPLIT_REDUCEOP_KEEP[i] == loc) return 1;
  }
  return 0;
}

static void split_reduceop_keep(u64 loc) {
  if (SPLIT_REDUCEOP_KEEP_GEN != UOP_MOV_GEN) {
    SPLIT_REDUCEOP_KEEP_N   = 0;
    SPLIT_REDUCEOP_KEEP_GEN = UOP_MOV_GEN;
  }
  if (SPLIT_REDUCEOP_KEEP_N == SPLIT_REDUCEOP_KEEP_CAP) {
    u32  cap  = SPLIT_REDUCEOP_KEEP_CAP ? SPLIT_REDUCEOP_KEEP_CAP * 2 : 16;
    u64 *keep = (u64 *)realloc(SPLIT_REDUCEOP_KEEP, cap * sizeof(u64));
    if (keep == NULL) thvm_fatal("split_reduceop: out of memory");
    SPLIT_REDUCEOP_KEEP     = keep;
    SPLIT_REDUCEOP_KEEP_CAP = cap;
  }
  SPLIT_REDUCEOP_KEEP[SPLIT_REDUCEOP_KEEP_N++] = loc;
}

// tinygrad's is_expanded (prepare.py:119-123): out[i] = 1 iff axis i of t, of
// shape s, does not reach the index into the base of t's movement ops.  0 when
// a swizzle declines.
static int split_reduceop_expanded(Term t, Shape const *s, u8 *out) {
  Term idx[RU_MAX_AXES], tmp[RU_MAX_AXES];
  u32  n = s->ndim;
  for (u32 i = 0; i < n; i++) {
    idx[i] = uop_range(SPLIT_REDUCEOP_AID_BASE + i, KAX_LOOP, s->dims[i]);
  }
  Term u = term_resolve(t);
  while (term_tag(u) == TAG_UOP && uop_is_movement((u8)term_ext(u))) {
    u32 in_n = 0;
    if (!ru_apply_movement(term_val(u), (u8)term_ext(u), idx, n, tmp, &in_n)) return 0;
    for (u32 i = 0; i < in_n; i++) idx[i] = tmp[i];
    n = in_n;
    u = term_resolve(heap_read(term_val(u)));
  }
  u8 reached[MAX_DIM] = {0};
  for (u32 i = 0; i < n; i++) {
    u32 aids[RU_MAX_AXES];
    u32 na = ru_collect_range_axes(idx[i], aids, RU_MAX_AXES);
    for (u32 j = 0; j < na; j++) {
      u32 a = aids[j] - SPLIT_REDUCEOP_AID_BASE;
      if (aids[j] >= SPLIT_REDUCEOP_AID_BASE && a < s->ndim) reached[a] = 1;
    }
  }
  for (u32 i = 0; i < s->ndim; i++) out[i] = !reached[i];
  return 1;
}

// 1 iff the contraction REDUCE at loc broadcasts an operand, a GEMM or GEMV
// that thvm dispatches to its matmul path (tinygrad splits it too).
static int split_reduceop_is_gemm(u64 loc, Shape const *xs) {
  if (!bufferize_reduce_is_contraction(loc)) return 0;
  Term mul = term_resolve(heap_read(loc + 0));
  for (u32 k = 0; k < 2; k++) {
    u8 ex[MAX_DIM];
    if (!split_reduceop_expanded(heap_read(term_val(mul) + k), xs, ex)) return 1;
    for (u32 i = 0; i < xs->ndim; i++) {
      if (ex[i]) return 1;
    }
  }
  return 0;
}

// Splits the REDUCE `red` in place: its cells become REDUCE(first, last axis)
// of the same kind, which fits, as the original holds at least one axis.
static void split_reduceop_one(Term red) {
  u32 kind = uop_reduce_kind(red);
  if (kind != REDUCE_SUM && kind != REDUCE_MAX) return;
  u64   loc = term_val(red);
  Term  x   = term_resolve(uop_reduce_src(red));
  Shape xs  = {0};
  Shape os  = {0};
  if (!term_shape_in(x, 0, &xs) || !term_shape_in(red, 0, &os)) return;
  if (xs.ndim + 1 > MAX_DIM) return;
  u32 n_axes = uop_reduce_n_axes(red);
  u8  is_red[MAX_DIM] = {0};
  for (u32 ai = 0; ai < n_axes; ai++) {
    u32 a = uop_reduce_axis(red, ai);
    if (a >= xs.ndim) return;
    is_red[a] = 1;
  }
  u64 r_numel = 1, o_numel = 1;
  for (u32 i = 0; i < xs.ndim; i++) {
    if (kvar_extent_is_var(xs.dims[i])) return;
    if (is_red[i]) r_numel *= xs.dims[i];
    else           o_numel *= xs.dims[i];
  }
  // REDUCEOP_SPLIT_THRESHOLD and REDUCEOP_SPLIT_SIZE (2**22 outputs).
  if (o_numel == 0 || r_numel < 32768) return;
  u64 dmax = (1ull << 22) / o_numel;
  if (dmax > 256) dmax = 256;
  if (dmax < 8) return;
  if (split_reduceop_is_gemm(loc, &xs)) return;
  u8 expanded[MAX_DIM];
  if (!split_reduceop_expanded(x, &xs, expanded)) return;
  u32 axis = 0, divisor = 0;
  for (u32 i = 0; i < xs.ndim && divisor == 0; i++) {
    if (!is_red[i] || expanded[i]) continue;
    for (u32 d = (u32)dmax; d >= 8; d--) {
      if (xs.dims[i] % d == 0) {
        axis    = i;
        divisor = d;
        break;
      }
    }
  }
  if (divisor == 0) return;

  // x.unflatten(axis, (divisor, rest)) with the divisor axis moved last; the
  // reduced axes keep x's numbering, the rest half standing where `axis` was.
  // A rest of 1 is not reduced, as tinygrad's _rop skips size-1 axes.
  u32 rest = xs.dims[axis] / divisor;
  u32 dims1[MAX_DIM];
  u32 n1 = 0;
  for (u32 i = 0; i < xs.ndim; i++) {
    dims1[n1++] = (i == axis) ? divisor : xs.dims[i];
    if (i == axis && rest > 1) dims1[n1++] = rest;
  }
  Term x1 = (rest > 1) ? uop_reshape(x, n1, dims1) : x;
  u32 perm[MAX_DIM];
  u32 np = 0;
  for (u32 p = 0; p < n1; p++) {
    if (p != axis) perm[np++] = p;
  }
  perm[np++] = axis;
  Term x2 = uop_permute(x1, n1, perm);
  u32 axes2[MAX_DIM];
  u32 n2 = 0;
  for (u32 ai = 0; ai < n_axes; ai++) {
    u32 a = uop_reduce_axis(red, ai);
    if (rest > 1)       axes2[n2++] = a;
    else if (a < axis)  axes2[n2++] = a;
    else if (a > axis)  axes2[n2++] = a - 1;
  }
  Term first = uop_reduce_multi(kind, n2, axes2, x2);
  split_reduceop_one(first);
  split_reduceop_keep(term_val(first));
  heap_set(loc + 0, first);
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, 1));
  heap_set(loc + 3, term_new(0, TAG_NUM, DT_INT32, os.ndim));
}

fn void split_reduceop(Term root) {
  char const *e = getenv("SPLIT_REDUCEOP");
  if (e != NULL && e[0] == '0') return;
  // CPU only until a GPU run checks it: Metal and CUDA group a large reduce.
  if (CURRENT_BACKEND != &CPU_BACKEND) return;
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  Term t;
  while ((t = uop_walk_pop(&w)) != 0) {
    u32 op = term_ext(t);
    if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) continue;
    if (op == UOP_REDUCE) split_reduceop_one(t);
    u8  ar  = uop_arity((u8)op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar; i++) {
      uop_walk_push(&w, heap_read(loc + i));
    }
  }
  uop_walk_free(&w);
}
