// test_uop_scatter_max.c -- UOP_SCATTER MAX and MIN forward through
// thvm_realize.
//
// tinygrad's scatter_reduce amax / amin (mixin/op.py:1129-1130) is
//   out = M.maximum(T),  M = mask.where(src, I).max(-1),
//   T   = self if include_self else _inv_mask(self, I) (touched ? I : self)
// with I = dtypes.min (amax) or dtypes.max (amin) of the dtype
// (dtype.py:70-78): -inf / +inf for a float, the ends of the integer range,
// False / True for bool.  On the CPU backend MAX renders as
// (a < b) ? b : a (codegen/decomp/op.py:84) and a reduce folds
// acc = MAX(acc, x) (codegen/__init__.py:203).  amin is the mirror image,
// (x < acc) ? x : acc, which tinygrad reaches by negating (float) or
// inverting (int) the same MAX (mixin/op.py:494, elementwise.py:404-408).
// The host reference folds exactly so: S[j] starts at I and folds the valid
// p with t(p) = j in row-major p order, then out = combine(S, T).
//
// Every case compares bytes.  Run the binary twice, as test_uop_scatter:
// as is (C JIT) and with THVM_CPU_JIT_WARMUP=1000000000 (UOp walker);
// check_dispatch asserts which backend ran.
//
//   1. MAX and MIN, include_self 0 and 1, dims 0 and 1, duplicate ids and
//      invalid ids {-1, V, 10982377, -19238}, fractional values;
//   2. ties: equal src values at one position, a src equal to the base,
//      -0.0 then +0.0 at one position (the first in p order stays);
//   3. all hits at a position equal to I (-inf for MAX, +inf for MIN), and
//      an untouched base equal to I;
//   4. NaN: a NaN src never wins (acc < NaN and NaN < acc are false), a NaN
//      base yields to S where it is combined, and an untouched NaN base
//      becomes I (MAX(I, NaN) = I), with or without include_self;
//   5. exact identity bits per dtype (f32, f16, bf16, i32, u32, i8, u8,
//      bool): an identity-CONST base leaves I's bytes where nothing lands,
//      and hits equal to I without include_self give I (the f16, bf16 and
//      bool kernels run on the walker in both modes: the CPU JIT declines
//      them);
//   6. kernel shape: one kernel, 2 stores for an identity-CONST base with
//      include_self and 3 otherwise, no REDUCE, at most max(|base|, |idx|)
//      iterations per store, no_opts set;
//   7. operand forms: a computed src (2 kernels), idx expanded from an {E}
//      tensor, a SHRINK view of the base;
//   8. JIT capture, then replays with the index rewritten in place;
//   9. BEAM=2 AUTOTUNE_CACHE=0: no scatter kernel is searched and the
//      values stay exact;
//  10. a copying GC with live SCATTER(MAX / MIN) graphs as roots keeps
//      them whole: the collected graphs realize to the same bytes.
#include "../src/thvm.c"
#include "test.h"

static int interp_mode(void) {
  char const *e = getenv("THVM_CPU_JIT_WARMUP");
  return e != NULL && atoi(e) > 1;
}

static Shape shape_of(u32 ndim, u32 const *dims) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
  }
  return s;
}

static u32 numel_of(Shape const *s) {
  u32 n = 1;
  for (u32 d = 0; d < s->ndim; d++) {
    n *= s->dims[d];
  }
  return n;
}

static u32 mk(u32 dtype, Shape const *s, void const *data) {
  u32 tid = tensor_alloc(&CPU_BACKEND, *s, dtype);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                          (u64)numel_of(s) * dtype_itemsize(dtype));
  }
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

// A CONST of `dtype` holding payload `bits`, expanded to shape s.
static Term full(u32 dtype, Shape const *s, u32 bits) {
  u32 ones[MAX_DIM] = { 1, 1, 1, 1, 1, 1, 1, 1 };
  return uop_expand(uop_reshape(uop_const(dtype, bits), s->ndim, ones),
                    s->ndim, s->dims);
}

// === element access through doubles (exact for every dtype used here) ===
static double load_d(u32 dt, void const *buf, u32 i) {
  u8 const *b = (u8 const *)buf;
  switch (dt) {
    case DT_FP32: { f32 v; memcpy(&v, b + 4 * (size_t)i, 4); return (double)v; }
    case DT_FP16: { u16 v; memcpy(&v, b + 2 * (size_t)i, 2); return (double)fp16_to_f32(v); }
    case DT_BF16: { u16 v; memcpy(&v, b + 2 * (size_t)i, 2); return (double)bf16_to_f32(v); }
    case DT_INT32: { i32 v; memcpy(&v, b + 4 * (size_t)i, 4); return (double)v; }
    case DT_UINT32: { u32 v; memcpy(&v, b + 4 * (size_t)i, 4); return (double)v; }
    case DT_INT8: return (double)(i8)b[i];
    case DT_UINT8: return (double)b[i];
    case DT_BOOL: return (double)(b[i] != 0);
    default: return 0.0;
  }
}

static void store_d(u32 dt, void *buf, u32 i, double v) {
  u8 *b = (u8 *)buf;
  switch (dt) {
    case DT_FP32: { f32 x = (f32)v; memcpy(b + 4 * (size_t)i, &x, 4); break; }
    case DT_FP16: { u16 x = f32_to_fp16((f32)v); memcpy(b + 2 * (size_t)i, &x, 2); break; }
    case DT_BF16: { u16 x = f32_to_bf16((f32)v); memcpy(b + 2 * (size_t)i, &x, 2); break; }
    case DT_INT32: { i32 x = (i32)v; memcpy(b + 4 * (size_t)i, &x, 4); break; }
    case DT_UINT32: { u32 x = (u32)v; memcpy(b + 4 * (size_t)i, &x, 4); break; }
    case DT_INT8: b[i] = (u8)(i8)v; break;
    case DT_UINT8: b[i] = (u8)v; break;
    case DT_BOOL: b[i] = (u8)(v != 0.0); break;
    default: break;
  }
}

// I for `kind` over `dt` as a value: tinygrad dtypes.min / dtypes.max.
static double ident_d(u32 kind, u32 dt) {
  int mx = (kind == SCATTER_MAX);
  switch (dt) {
    case DT_FP32: case DT_FP16: case DT_BF16: return mx ? -INFINITY : INFINITY;
    case DT_INT32: return mx ? -2147483648.0 : 2147483647.0;
    case DT_UINT32: return mx ? 0.0 : 4294967295.0;
    case DT_INT8: return mx ? -128.0 : 127.0;
    case DT_UINT8: return mx ? 0.0 : 255.0;
    default: return mx ? 0.0 : 1.0;
  }
}

// I's storage bytes, written out by hand: the reference never derives them
// from the runtime.
static u32 ident_storage(u32 kind, u32 dt) {
  int mx = (kind == SCATTER_MAX);
  switch (dt) {
    case DT_FP32: return mx ? 0xFF800000u : 0x7F800000u;
    case DT_FP16: return mx ? 0xFC00u : 0x7C00u;
    case DT_BF16: return mx ? 0xFF80u : 0x7F80u;
    case DT_INT32: return mx ? 0x80000000u : 0x7FFFFFFFu;
    case DT_UINT32: return mx ? 0u : 0xFFFFFFFFu;
    case DT_INT8: return mx ? 0x80u : 0x7Fu;
    case DT_UINT8: return mx ? 0u : 0xFFu;
    default: return mx ? 0u : 1u;
  }
}

// I as a CONST payload (uop/const.c): a float's f32 bits, an int's low 32
// bits.
static u32 ident_payload(u32 kind, u32 dt) {
  if (dtype_is_float(dt)) {
    f32 v = (kind == SCATTER_MAX) ? -INFINITY : INFINITY;
    u32 bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
  }
  double v = ident_d(kind, dt);
  return v < 0.0 ? (u32)(i32)v : (u32)v;
}

// === host reference ===
static double comb(u32 kind, double acc, double x) {
  if (kind == SCATTER_MAX) {
    return (acc < x) ? x : acc;
  }
  return (x < acc) ? x : acc;
}

// Target flat index of position p, or -1 when idx[p] is out of range.
static i32 target_of(u32 p, Shape const *bs, Shape const *is, i32 const *idx,
                     u32 dim) {
  u32 c[MAX_DIM];
  u32 rem = p;
  for (i32 d = (i32)is->ndim - 1; d >= 0; d--) {
    c[d] = rem % is->dims[d];
    rem /= is->dims[d];
  }
  i32 v = idx[p];
  if (v < 0 || (u32)v >= bs->dims[dim]) {
    return -1;
  }
  c[dim] = (u32)v;
  u32 j = 0;
  for (u32 d = 0; d < bs->ndim; d++) {
    j = j * bs->dims[d] + c[d];
  }
  return (i32)j;
}

static void ref_maxmin(u32 kind, u32 dt, void const *base, Shape const *bs,
                       i32 const *idx, void const *src, Shape const *is,
                       u32 dim, u32 self, void *out) {
  u32     bn  = numel_of(bs), in = numel_of(is);
  double  idv = ident_d(kind, dt);
  double *s   = (double *)malloc((size_t)bn * sizeof(double));
  u8     *hit = (u8 *)calloc(bn, 1);
  for (u32 j = 0; j < bn; j++) {
    s[j] = idv;
  }
  for (u32 p = 0; p < in; p++) {
    i32 j = target_of(p, bs, is, idx, dim);
    if (j >= 0) {
      s[j] = comb(kind, s[j], load_d(dt, src, p));
      hit[j] = 1;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    double t = (self || !hit[j]) ? load_d(dt, base, j) : idv;
    store_d(dt, out, j, comb(kind, s[j], t));
  }
  free(s);
  free(hit);
}

static int check_bytes(void const *got, void const *want, u32 n, u32 dt,
                       char const *what) {
  u32 sz = dtype_itemsize(dt);
  u32 shown = 0, bad = 0;
  for (u32 i = 0; i < n; i++) {
    if (memcmp((u8 const *)got + (size_t)i * sz, (u8 const *)want + (size_t)i * sz,
               sz) != 0) {
      bad++;
      if (shown < 8) {
        fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", what, i,
                load_d(dt, got, i), load_d(dt, want, i));
        shown++;
      }
    }
  }
  CHECK(bad == 0);
  return bad == 0;
}

static u32 DISPATCHED = 0;

// The CPU JIT declines a kernel with a narrow-float buffer
// (cpu_jit_narrow_float, backend/cpu/jit.c) or a bool input (DT_BOOL is
// dtype 0, the untyped-slot decline of cg_render_uop_kernel_c_root), so the
// walker runs it in either mode.
static int jit_declines(u32 k) {
  u32 od = KERNELS[k].output_dtype;
  if (dtype_is_float(od) && dtype_itemsize(od) < 4) {
    return 1;
  }
  for (u32 i = 0; i < KERNELS[k].n_inputs && KERNELS[k].input_dtypes != NULL; i++) {
    u32 d = KERNELS[k].input_dtypes[i];
    if (d == DT_BOOL || (dtype_is_float(d) && dtype_itemsize(d) < 4)) {
      return 1;
    }
  }
  return 0;
}

static void check_dispatch(u32 k0) {
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    u32 kind = cg_kernel_dispatch_kind(k);
    if (kind == KDISPATCH_NONE
        || (kind >= KDISPATCH_BLAS_DOT && kind <= KDISPATCH_BLAS_GEMM)) {
      continue;
    }
    u32 want = (interp_mode() || jit_declines(k)) ? KDISPATCH_INTERPRETER
                                                  : KDISPATCH_JIT;
    if (kind != want) {
      fprintf(stderr, "  kernel %u: dispatch %u, output dtype %u, inputs", k, kind,
              KERNELS[k].output_dtype);
      for (u32 i = 0; i < KERNELS[k].n_inputs && KERNELS[k].input_dtypes != NULL; i++) {
        fprintf(stderr, " %u", KERNELS[k].input_dtypes[i]);
      }
      fprintf(stderr, "\n");
    }
    CHECK_EQ(kind, want);
    DISPATCHED++;
  }
}

// Realize t into out (n elements of dt); returns the result tid, 0 when the
// realize gave no tensor.
static u32 realize_into(Term t, void *out, u32 n, u32 dt) {
  Term r = term_resolve(thvm_realize(t));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    memset(out, 0, (size_t)n * dtype_itemsize(dt));
    return 0;
  }
  u32 tid = (u32)term_val(r);
  CHECK_EQ(TENS[tid].dtype, dt);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, (u64)n * dtype_itemsize(dt));
  return tid;
}

static u32 kernel_for(u32 tid, u32 k0) {
  for (u32 k = KERNELS_NEXT; k-- > k0 && k > 0;) {
    if (KERNELS[k].output_tid == tid) {
      return k;
    }
  }
  return 0;
}

static void collect_ranges(Term t, u32 *aid, u32 *ext, u32 *n, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) {
    return;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 a = uop_range_axis_id(t);
    for (u32 i = 0; i < *n; i++) {
      if (aid[i] == a) {
        return;
      }
    }
    if (*n < 64) {
      aid[*n] = a;
      ext[*n] = uop_range_extent(t);
      (*n)++;
    }
    return;
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    collect_ranges(heap_read(loc + i), aid, ext, n, depth + 1);
  }
}

typedef struct {
  u32 n_stores;
  u64 max_loops;
} StoreStats;

static void store_stats(Term root, StoreStats *st) {
  if (term_tag(root) == TAG_UOP && term_ext(root) == UOP_AFTER) {
    store_stats(uop_after_after_node(root), st);
    store_stats(uop_after_node(root), st);
    return;
  }
  st->n_stores++;
  u32 aid[64], ext[64], n = 0;
  collect_ranges(root, aid, ext, &n, 0);
  u64 prod = 1;
  for (u32 i = 0; i < n; i++) {
    prod *= ext[i];
  }
  if (prod > st->max_loops) {
    st->max_loops = prod;
  }
}

static int has_op(Term t, u32 want, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) {
    return 0;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == want) {
    return 1;
  }
  u8 ar = (op == UOP_AFTER) ? 2 : uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    if (has_op(heap_read(loc + i), want, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

// The scatter kernel writing tid: its store count, no REDUCE, at most
// max(|base|, |idx|) iterations per store, no_opts set.  Returns its id.
static u32 check_scatter_kernel(u32 tid, u32 k0, u32 want_stores,
                                u64 base_numel, u64 idx_numel) {
  u32 kid = (tid != 0) ? kernel_for(tid, k0) : 0;
  CHECK(kid != 0);
  if (kid == 0) {
    return 0;
  }
  Term root = KERNELS[kid].cached_lift.store_root;
  CHECK(root != 0);
  StoreStats st = { 0, 0 };
  store_stats(root, &st);
  CHECK_EQ(st.n_stores, want_stores);
  CHECK(st.max_loops <= (base_numel > idx_numel ? base_numel : idx_numel));
  CHECK(!has_op(root, UOP_REDUCE, 0));
  CHECK_EQ(KERNELS[kid].no_opts, 1);
  return kid;
}

// Scatter (base, idx, src) of `kind`, realize, compare with the reference
// and check the kernel.  base and src are host arrays of dtype dt; a
// non-zero base_term replaces the base tensor (its logical values are
// `base`).  Returns the scatter kernel id.
static u32 scatter_case(u32 kind, u32 dt, Shape const *bs, Shape const *is,
                        u32 dim, u32 self, void const *base, Term base_term,
                        i32 const *idx, void const *src, u32 want_stores,
                        char const *what) {
  u32   bn   = numel_of(bs), in = numel_of(is);
  u32   sz   = dtype_itemsize(dt);
  void *want = malloc((size_t)bn * sz);
  void *got  = malloc((size_t)bn * sz);
  ref_maxmin(kind, dt, base, bs, idx, src, is, dim, self, want);
  if (base_term == 0) {
    base_term = ten(mk(dt, bs, base));
  }
  Term s = uop_scatter(base_term, ten(mk(DT_INT32, is, idx)), ten(mk(dt, is, src)),
                       dim, kind, self);
  CHECK(s != 0);
  u32 k0  = KERNELS_NEXT;
  u32 tid = realize_into(s, got, bn, dt);
  check_bytes(got, want, bn, dt, what);
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  u32 kid = check_scatter_kernel(tid, k0, want_stores, bn, in);
  free(want);
  free(got);
  return kid;
}

static void fill(f32 *v, u32 n, f32 scale, f32 bias) {
  for (u32 q = 0; q < n; q++) {
    v[q] = bias + scale * (f32)((i32)(q * 7u % 23u) - 11);
  }
}

static char const *kname(u32 kind) {
  return kind == SCATTER_MAX ? "max" : "min";
}

// === 1-2. MAX / MIN with duplicates, invalid ids and ties ===
#define SV 5
#define SD 3
#define SE 6
static i32 const IDX0[SE * SD] = {
  1, 3, 1,   -1, SV, 0,   1, 1, 4,
  10982377, 2, 2,   0, -19238, 4,   1, 3, 1,
};

static void test_maxmin(void) {
  u32   bd[2] = { SV, SD }, id[2] = { SE, SD };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  f32   base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.37f, 0.11f);
  fill(src, SE * SD, 0.29f, -0.07f);
  // Position (1, 0) is hit by p = (0, 0), (2, 0), (5, 0): a tie between
  // the first and the last.  Position (1, 1) is hit by (2, 1) alone, equal
  // to the base there.  Position (1, 2) is hit by (0, 2) = -0.0, then
  // (5, 2) = +0.0.  Position (2, 1) is hit by (3, 1) alone.
  src[0 * SD + 0] = 1.75f;
  src[5 * SD + 0] = 1.75f;
  src[2 * SD + 0] = -0.5f;
  src[2 * SD + 1] = base[1 * SD + 1];
  src[0 * SD + 2] = -0.0f;
  src[5 * SD + 2] = 0.0f;
  base[1 * SD + 2] = -1.25f;
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 s = 0; s < 2; s++) {
      char what[64];
      snprintf(what, sizeof what, "%s self %u dim 0", kname(k), s);
      scatter_case(k, DT_FP32, &bs, &is, 0, s, base, 0, IDX0, src, 3, what);
    }
  }
  // Signed zeros: with include_self 0 position (1, 2) holds the first of
  // -0.0, +0.0 for both kinds.
  f32 want[SV * SD];
  ref_maxmin(SCATTER_MAX, DT_FP32, base, &bs, IDX0, src, &is, 0, 0, want);
  CHECK(want[1 * SD + 2] == 0.0f && signbit(want[1 * SD + 2]));
  ref_maxmin(SCATTER_MIN, DT_FP32, base, &bs, IDX0, src, &is, 0, 0, want);
  CHECK(want[1 * SD + 2] == 0.0f && signbit(want[1 * SD + 2]));
  // Along dim 1: base (3, 6), idx (3, 4).
  u32   bd1[2] = { 3, 6 }, id1[2] = { 3, 4 };
  Shape bs1 = shape_of(2, bd1), is1 = shape_of(2, id1);
  static i32 const idx1[12] = { 5, 0, 5, 6,  -1, 2, 2, 2,  3, 1, 0, 3 };
  f32 base1[18], src1[12];
  fill(base1, 18, 0.5f, 0.0f);
  fill(src1, 12, 0.25f, 0.0f);
  src1[5] = src1[7];
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 s = 0; s < 2; s++) {
      char what[64];
      snprintf(what, sizeof what, "%s self %u dim 1", kname(k), s);
      scatter_case(k, DT_FP32, &bs1, &is1, 1, s, base1, 0, idx1, src1, 3, what);
    }
  }
}

// === 3-4. the identity and NaN ===
// 1-D, V = 9 positions, E = 17 ids:
//   0: NaN, 1, 2 (NaN first)       1: 1, NaN, 2 (NaN in the middle)
//   2: untouched, base NaN         3: -0.0 then +0.0
//   4: I, I (every hit is I)       5: untouched, base I
//   6: NaN only, base finite       7: 3, 3, base 3 (ties with the base)
//   8: 2.5 only, base NaN
// plus the invalid ids -1, 9, 10982377.
#define NV 9
#define NE 17
static i32 const NIDX[NE] = { 0, 0, 0, 1, 1, 1, -1, 3, 3, 4, 4, 6, 7, 7, NV,
                              8, 10982377 };

static void special_values(u32 kind, f32 *base, f32 *src) {
  f32 const nan = NAN;
  f32 const idv = (kind == SCATTER_MAX) ? -INFINITY : INFINITY;
  f32 const b[NV] = { nan, 0.5f, nan, 0.0f, 7.0f, idv, -1.0f, 3.0f, nan };
  f32 const s[NE] = { nan, 1.0f, 2.0f, 1.0f, nan, 2.0f, 5.0f, -0.0f, 0.0f,
                      idv, idv, nan, 3.0f, 3.0f, 9.0f, 2.5f, -9.0f };
  memcpy(base, b, sizeof b);
  memcpy(src, s, sizeof s);
  if (kind == SCATTER_MIN) {
    base[4] = -7.0f;
  }
}

static void test_identity_and_nan(void) {
  u32   vd[1] = { NV }, ed[1] = { NE };
  Shape bs = shape_of(1, vd), is = shape_of(1, ed);
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    f32 base[NV], src[NE];
    special_values(k, base, src);
    f32 const idv = (k == SCATTER_MAX) ? -INFINITY : INFINITY;
    for (u32 s = 0; s < 2; s++) {
      char what[64];
      snprintf(what, sizeof what, "%s self %u identity / NaN", kname(k), s);
      scatter_case(k, DT_FP32, &bs, &is, 0, s, base, 0, NIDX, src, 3, what);
      f32 want[NV];
      ref_maxmin(k, DT_FP32, base, &bs, NIDX, src, &is, 0, s, want);
      // The reference written out: a NaN src never wins, a NaN base yields
      // where it is combined and an untouched one becomes I.
      f32 const fold = (k == SCATTER_MAX) ? 2.0f : 1.0f;
      CHECK(want[0] == fold);
      CHECK(want[1] == ((s && k == SCATTER_MIN) ? 0.5f : fold));
      CHECK(want[2] == idv);
      CHECK(want[3] == 0.0f && signbit(want[3]));
      CHECK(want[4] == (s ? base[4] : idv));
      CHECK(want[5] == idv);
      CHECK(want[6] == (s ? -1.0f : idv));
      CHECK(want[7] == 3.0f);
      CHECK(want[8] == 2.5f);
    }
  }
}

// === 5. exact identity bits per dtype ===
static u32 const DTYPES[8] = { DT_FP32, DT_FP16, DT_BF16, DT_INT32, DT_UINT32,
                               DT_INT8, DT_UINT8, DT_BOOL };

// n values of dt from a pattern that crosses the dtype's range; every
// fourth one is I of `kind` when with_ident is set.
static void gen(u32 dt, u32 kind, u32 n, u32 seed, int with_ident, void *out) {
  for (u32 q = 0; q < n; q++) {
    u32 h = (q + 1u) * 2654435761u + seed * 40503u;
    double v;
    switch (dt) {
      case DT_FP32: case DT_FP16: case DT_BF16:
        v = 0.25 * (double)((i32)(h % 41u) - 20);
        break;
      case DT_INT32: v = (double)(i32)h; break;
      case DT_UINT32: v = (double)h; break;
      case DT_INT8: v = (double)(i32)((h >> 7) % 256u) - 128.0; break;
      case DT_UINT8: v = (double)((h >> 7) % 256u); break;
      default: v = (double)((h >> 9) & 1u); break;
    }
    if (with_ident && q % 4u == 3u) {
      v = ident_d(kind, dt);
    }
    store_d(dt, out, q, v);
  }
}

static void test_identity_bits(void) {
  u32   bd[2] = { 6, 2 }, id[2] = { 5, 2 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  // Rows 2 and 5 are never written; row 4 only by ids equal to I.
  static i32 const idx[10] = { 1, 0,  -1, 4,  3, 1,  6, 10982377,  0, 3 };
  for (u32 di = 0; di < 8; di++) {
    u32 dt = DTYPES[di];
    u32 sz = dtype_itemsize(dt);
    for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
      u8 base[64], src[64], want[64];
      gen(dt, k, 12, 1u + di, 0, base);
      gen(dt, k, 10, 7u + di, 1, src);
      store_d(dt, src, 3, ident_d(k, dt));
      char what[64];
      // An identity-CONST base with include_self: 2 stores, and the
      // untouched positions hold I's bytes.
      u8 ibase[64];
      for (u32 j = 0; j < 12; j++) {
        store_d(dt, ibase, j, ident_d(k, dt));
      }
      snprintf(what, sizeof what, "dtype %u %s identity base", dt, kname(k));
      scatter_case(k, dt, &bs, &is, 0, 1, ibase,
                   full(dt, &bs, ident_payload(k, dt)), idx, src, 2, what);
      ref_maxmin(k, dt, ibase, &bs, idx, src, &is, 0, 1, want);
      u32 ib = ident_storage(k, dt);
      static u32 const untouched[4] = { 2 * 2, 2 * 2 + 1, 5 * 2, 5 * 2 + 1 };
      for (u32 q = 0; q < 4; q++) {
        CHECK(memcmp(want + (size_t)untouched[q] * sz, &ib, sz) == 0);
      }
      // A data base, include_self 1 and 0: 3 stores each.
      for (u32 s = 0; s < 2; s++) {
        snprintf(what, sizeof what, "dtype %u %s self %u", dt, kname(k), s);
        scatter_case(k, dt, &bs, &is, 0, s, base, 0, idx, src, 3, what);
      }
      // Without include_self, row 4 column 1 is hit only by src[3] = I.
      ref_maxmin(k, dt, base, &bs, idx, src, &is, 0, 0, want);
      CHECK(memcmp(want + (size_t)(4 * 2 + 1) * sz, &ib, sz) == 0);
    }
  }
}

// === 7. operand forms ===
static Term rows(Term ids, u32 n, u32 d) {
  u32 n1[2] = { n, 1 };
  u32 nd[2] = { n, d };
  return uop_expand(uop_reshape(ids, 2, n1), 2, nd);
}

static void test_operand_forms(void) {
  enum { V = 5, D = 3, E = 11 };
  static i32 const ids[E] = { 4, 0, 4, -1, 2, V, 1, 4, 0, 3, 2 };
  u32   bd[2] = { V, D }, id[2] = { E, D }, e1[1] = { E };
  Shape bs = shape_of(2, bd), is = shape_of(2, id), es = shape_of(1, e1);
  i32 grid[E * D];
  for (u32 p = 0; p < E * D; p++) {
    grid[p] = ids[p / D];
  }
  f32 base[V * D], src[E * D], neg[E * D], want[V * D], got[V * D];
  fill(base, V * D, 0.5f, 0.25f);
  fill(src, E * D, 0.37f, 0.11f);
  for (u32 p = 0; p < E * D; p++) {
    neg[p] = -src[p];
  }
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 s = 0; s < 2; s++) {
      char what[64];
      // idx expanded from an {E} tensor, src computed: 2 kernels.
      ref_maxmin(k, DT_FP32, base, &bs, grid, neg, &is, 0, s, want);
      Term y = uop_scatter(ten(mk(DT_FP32, &bs, base)),
                           rows(ten(mk(DT_INT32, &es, ids)), E, D),
                           uop_unary(UOP_NEG, ten(mk(DT_FP32, &is, src))), 0, k, s);
      u32 k0 = KERNELS_NEXT;
      u32 tid = realize_into(y, got, V * D, DT_FP32);
      snprintf(what, sizeof what, "%s self %u computed src", kname(k), s);
      check_bytes(got, want, V * D, DT_FP32, what);
      CHECK_EQ(KERNELS_NEXT - k0, 2u);
      check_dispatch(k0);
      check_scatter_kernel(tid, k0, 3, V * D, E * D);
      // The base a row SHRINK of a (V + 2, D) tensor, read in place.
      f32 raw[(V + 2) * D];
      fill(raw, (V + 2) * D, 0.5f, -0.25f);
      u32   rd[2] = { V + 2, D }, be[4] = { 2, V + 2, 0, D };
      Shape rs = shape_of(2, rd);
      Term  bv = uop_shrink(ten(mk(DT_FP32, &rs, raw)), 2, be);
      snprintf(what, sizeof what, "%s self %u base view", kname(k), s);
      scatter_case(k, DT_FP32, &bs, &is, 0, s, raw + 2 * D, bv, grid, src, 3, what);
    }
  }
}

// === 8. JIT capture, replays with the index rewritten in place ===
static void test_jit_replay(void) {
  u32   bd[2] = { SV, SD }, id[2] = { SE, SD };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  f32   base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  fill(src, SE * SD, 0.37f, 0.11f);
  i32 idx[4][SE * SD];
  for (u32 rep = 0; rep < 4; rep++) {
    for (u32 p = 0; p < SE * SD; p++) {
      idx[rep][p] = (i32)((p * (3u + rep) + rep) % (SV + 2)) - 1;
    }
  }
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    u32  it = mk(DT_INT32, &is, idx[0]);
    Term y  = uop_scatter(ten(mk(DT_FP32, &bs, base)), ten(it),
                          ten(mk(DT_FP32, &is, src)), 0, k, 0);
    u32 slot = jit_capture_begin();
    CHECK(slot != 0);
    Term r = term_resolve(thvm_realize(y));
    jit_capture_end_with_result(r);
    CHECK_EQ(term_tag(r), TAG_TEN);
    if (term_tag(r) != TAG_TEN) {
      return;
    }
    u32 tid = (u32)term_val(r);
    f32 want[SV * SD], got[SV * SD];
    for (u32 rep = 0; rep < 4; rep++) {
      if (rep > 0) {
        CPU_BACKEND.buf_write(TENS[it].buf_id, idx[rep], SE * SD * 4);
        f32 poison[SV * SD];
        for (u32 q = 0; q < SV * SD; q++) {
          poison[q] = -999.0f;
        }
        TENS[tid].backend->buf_write(TENS[tid].buf_id, poison, SV * SD * 4);
        jit_replay(slot);
      }
      TENS[tid].backend->buf_read(TENS[tid].buf_id, got, SV * SD * 4);
      ref_maxmin(k, DT_FP32, base, &bs, idx[rep], src, &is, 0, 0, want);
      char what[48];
      snprintf(what, sizeof what, "%s replay %u", kname(k), rep);
      check_bytes(got, want, SV * SD, DT_FP32, what);
    }
    jit_capture_drop(slot);
  }
}

// === 9. BEAM=2 searches no scatter kernel ===
static void test_beam(void) {
  setenv("BEAM", "2", 1);
  setenv("AUTOTUNE_CACHE", "0", 1);
  u32   vd[1] = { NV }, ed[1] = { NE };
  Shape bs = shape_of(1, vd), is = shape_of(1, ed);
  u32   bd[2] = { SV, SD }, id[2] = { SE, SD };
  Shape bs2 = shape_of(2, bd), is2 = shape_of(2, id);
  f32   base2[SV * SD], src2[SE * SD];
  fill(base2, SV * SD, 0.37f, 0.11f);
  fill(src2, SE * SD, 0.29f, -0.07f);
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    f32 base[NV], src[NE];
    special_values(k, base, src);
    for (u32 s = 0; s < 2; s++) {
      char what[64];
      snprintf(what, sizeof what, "beam %s self %u", kname(k), s);
      u32 kid = scatter_case(k, DT_FP32, &bs, &is, 0, s, base, 0, NIDX, src, 3, what);
      CHECK(kid != 0 && !kernel_should_autotune(&KERNELS[kid]));
      kid = scatter_case(k, DT_FP32, &bs2, &is2, 0, s, base2, 0, IDX0, src2, 3, what);
      CHECK(kid != 0 && !kernel_should_autotune(&KERNELS[kid]));
    }
  }
  unsetenv("BEAM");
  unsetenv("AUTOTUNE_CACHE");
}

// === 10. a GC with live SCATTER(MAX / MIN) graphs ===
static int in_from_space(Term t) {
  u64 loc = term_val(t);
  return loc >= gc_from_start() && loc < gc_from_end();
}

static void test_gc_live_graphs(void) {
  u32   bd[2] = { SV, SD }, id[2] = { SE, SD };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  f32   base[SV * SD], src[SE * SD], neg[SE * SD];
  fill(base, SV * SD, 0.37f, 0.11f);
  fill(src, SE * SD, 0.29f, -0.07f);
  for (u32 p = 0; p < SE * SD; p++) {
    neg[p] = -src[p];
  }
  u32 bt = mk(DT_FP32, &bs, base);
  u32 it = mk(DT_INT32, &is, IDX0);
  u32 st = mk(DT_FP32, &is, src);
  Term roots[4];
  u32  kinds[4] = { SCATTER_MAX, SCATTER_MAX, SCATTER_MIN, SCATTER_MIN };
  for (u32 r = 0; r < 4; r++) {
    roots[r] = uop_scatter(ten(bt), ten(it), uop_unary(UOP_NEG, ten(st)), 0,
                           kinds[r], r % 2u);
    CHECK(roots[r] != 0);
  }
  CHECK(gc_enabled());
  gc_collect(roots, 4);
  for (u32 r = 0; r < 4; r++) {
    Term y = roots[r];
    CHECK_EQ(term_ext(y), UOP_SCATTER);
    CHECK(in_from_space(y));
    CHECK_EQ(uop_scatter_kind(y), kinds[r]);
    CHECK_EQ(uop_scatter_include_self(y), r % 2u);
    CHECK_EQ(uop_scatter_dim(y), 0u);
    CHECK_EQ(uop_scatter_base(y), ten(bt));
    CHECK_EQ(uop_scatter_idx(y), ten(it));
    Term ns = uop_scatter_src(y);
    CHECK_EQ(term_ext(ns), UOP_NEG);
    CHECK(in_from_space(ns));
    f32 want[SV * SD], got[SV * SD];
    ref_maxmin(kinds[r], DT_FP32, base, &bs, IDX0, neg, &is, 0, r % 2u, want);
    u32 k0 = KERNELS_NEXT;
    realize_into(y, got, SV * SD, DT_FP32);
    char what[48];
    snprintf(what, sizeof what, "after GC %s self %u", kname(kinds[r]), r % 2u);
    check_bytes(got, want, SV * SD, DT_FP32, what);
    check_dispatch(k0);
  }
}

// === PROD still has no lowering ===
static void test_prod_declined(void) {
  u32   bd[2] = { SV, SD }, id[2] = { SE, SD };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  f32   base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  fill(src, SE * SD, 0.37f, 0.11f);
  Term s = uop_scatter(ten(mk(DT_FP32, &bs, base)), ten(mk(DT_INT32, &is, IDX0)),
                       ten(mk(DT_FP32, &is, src)), 0, SCATTER_PROD, 1);
  CHECK(s != 0);
  u32 k0 = KERNELS_NEXT;
  Term r = term_resolve(thvm_realize(s));
  CHECK(term_tag(r) != TAG_TEN);
  CHECK_EQ(KERNELS_NEXT, k0);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("scatter-max/maxmin");
  test_maxmin();
  TEST_BEGIN("scatter-max/identity-and-nan");
  test_identity_and_nan();
  TEST_BEGIN("scatter-max/identity-bits");
  test_identity_bits();
  TEST_BEGIN("scatter-max/operand-forms");
  test_operand_forms();
  TEST_BEGIN("scatter-max/jit-replay");
  test_jit_replay();
  TEST_BEGIN("scatter-max/beam");
  test_beam();
  TEST_BEGIN("scatter-max/gc-live-graphs");
  test_gc_live_graphs();
  TEST_BEGIN("scatter-max/prod-declined");
  test_prod_declined();
  TEST_BEGIN("scatter-max/dispatched");
  CHECK(DISPATCHED >= 60);
  thvm_free();
  TEST_REPORT();
}
