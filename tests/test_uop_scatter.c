// test_uop_scatter.c -- UOP_SCATTER forward (SUM, PROD and REPLACE) through
// thvm_realize.
//
// Every case compares the realized tensor with a host reference byte for
// byte.  The SUM reference is tinygrad's algebra (mixin/op.py:1126-1129):
// S[j] sums src over the valid p with t(p) = j in row-major p order
// starting from 0, and out = S + T with T = base (include_self) or
// (touched ? 0 : base).  REPLACE is last-write-wins along dim, base where
// untouched (_masked_merge, :1170-1177).  Run the binary twice, as
// test_uop_gather.c does: as is (C JIT), and with THVM_CPU_JIT_WARMUP=
// 1000000000 (UOp walker); check_dispatch asserts which backend ran.
//
//   1. SUM with include_self 1 and 0, dims 0 and 1, duplicates and invalid
//      ids {-1, V, 10982377, -19238}; the S-then-self order is visible;
//   2. an identity-CONST base gives 2 stores, a non-identity base 3;
//   3. an all-invalid idx leaves base; REPLACE is last-write-wins;
//   4. idx as an expand of an {E} tensor and as edge_index[1]; a computed
//      src realizes into its own kernel (2 kernels), a gathered one too
//      (message passing, out[dst[e]] += h[src[e]]); base views read in
//      place through their storage, as idx and src are (a view-alias TEN
//      with an offset or a PERMUTE, lazy row / column SHRINK and FLIP
//      views), and a computed base fused into the stores, one kernel each;
//      a src viewing a realized view reads that view's buffer;
//   5. the scatter is an intermediate: its output buffer may be a reused
//      arena slot, and the init store covers it;
//   6. every store loops at most max(|base|, |idx|) times, no REDUCE, and
//      the kernel is flagged no_opts;
//   7. JIT capture, then replays with the index rewritten in place and the
//      output poisoned first;
//   8. PROD folds src from 1 by MUL, out = S * T, with include_self 1 and 0
//      and dims 0 and 1; MAX and MIN are covered by test_uop_scatter_max.c.
#include "../src/thvm.c"
#include "test.h"
#include "tu_probes.h"

static u32 mk(u32 dtype, u32 ndim, u32 const *dims, void const *data) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dtype);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                          (u64)TENS[tid].view.numel * dtype_itemsize(dtype));
  }
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

static Term fconst(u32 ndim, u32 const *dims, f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  u32 ones[MAX_DIM] = {0};
  for (u32 i = 0; i < MAX_DIM; i++) {
    ones[i] = 1;
  }
  return uop_expand(uop_reshape(uop_const(DT_FP32, bits), ndim, ones), ndim, dims);
}

static u32 numel_of(Shape const *s) {
  u32 n = 1;
  for (u32 d = 0; d < s->ndim; d++) {
    n *= s->dims[d];
  }
  return n;
}

static Shape shape2(u32 d0, u32 d1) {
  Shape s = {0};
  s.ndim = 2;
  s.dims[0] = d0;
  s.dims[1] = d1;
  return s;
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

// SUM or PROD: S[j] folds src from the identity in p order, out = S (op) T.
static void ref_fold(u32 kind, f32 const *base, Shape const *bs, i32 const *idx,
                     f32 const *src, Shape const *is, u32 dim, int self,
                     f32 *out) {
  u32 bn = numel_of(bs), in = numel_of(is);
  f32 id  = (kind == SCATTER_PROD) ? 1.0f : 0.0f;
  f32 *s  = (f32 *)malloc((size_t)bn * 4);
  u8  *hit = (u8 *)calloc(bn, 1);
  for (u32 j = 0; j < bn; j++) {
    s[j] = id;
  }
  for (u32 p = 0; p < in; p++) {
    i32 j = target_of(p, bs, is, idx, dim);
    if (j >= 0) {
      s[j] = (kind == SCATTER_PROD) ? s[j] * src[p] : s[j] + src[p];
      hit[j] = 1;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    f32 t = (self || !hit[j]) ? base[j] : id;
    out[j] = (kind == SCATTER_PROD) ? s[j] * t : s[j] + t;
  }
  free(s);
  free(hit);
}

static void ref_sum(f32 const *base, Shape const *bs, i32 const *idx,
                    f32 const *src, Shape const *is, u32 dim, int self,
                    f32 *out) {
  ref_fold(SCATTER_SUM, base, bs, idx, src, is, dim, self, out);
}

static void ref_replace(f32 const *base, Shape const *bs, i32 const *idx,
                        f32 const *src, Shape const *is, u32 dim, f32 *out) {
  u32 bn = numel_of(bs), in = numel_of(is);
  memcpy(out, base, (size_t)bn * 4);
  for (u32 p = 0; p < in; p++) {
    i32 j = target_of(p, bs, is, idx, dim);
    if (j >= 0) {
      out[j] = src[p];
    }
  }
}

static void check_bytes(void const *got, void const *want, u32 n,
                        char const *what) {
  f32 const *g = (f32 const *)got;
  f32 const *w = (f32 const *)want;
  u32 shown = 0;
  for (u32 i = 0; i < n && shown < 8; i++) {
    if (memcmp(&g[i], &w[i], 4) != 0) {
      fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", what, i, g[i], w[i]);
      shown++;
    }
  }
  CHECK(memcmp(got, want, (size_t)n * 4) == 0);
}

static u32 DISPATCHED = 0;

static void check_dispatch(u32 k0) {
  u32 want = cpu_jit_walker_only() ? KDISPATCH_INTERPRETER : KDISPATCH_JIT;
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    u32 kind = cg_kernel_dispatch_kind(k);
    if (kind == KDISPATCH_NONE
        || kind == KDISPATCH_BLAS_GEMM) {
      continue;
    }
    CHECK_EQ(kind, want);
    DISPATCHED++;
  }
}

static u32 realize_into(Term t, void *out, u32 n) {
  Term r = term_resolve(thvm_realize(t));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    memset(out, 0, (size_t)n * 4);
    return 0;
  }
  u32 tid = (u32)term_val(r);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, (u64)n * 4);
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
// max(|base|, |idx|) iterations per store, no_opts set.
static void check_scatter_kernel(u32 tid, u32 k0, u32 want_stores,
                                 u64 base_numel, u64 idx_numel) {
  u32 kid = (tid != 0) ? kernel_for(tid, k0) : 0;
  CHECK(kid != 0);
  if (kid == 0) {
    return;
  }
  Term root = KERNELS[kid].cached_lift.store_root;
  CHECK(root != 0);
  StoreStats st = { 0, 0 };
  store_stats(root, &st);
  CHECK_EQ(st.n_stores, want_stores);
  CHECK(st.max_loops <= (base_numel > idx_numel ? base_numel : idx_numel));
  CHECK(!has_op(root, UOP_REDUCE, 0));
  CHECK_EQ(KERNELS[kid].no_opts, 1);
}

static void fill(f32 *v, u32 n, f32 scale, f32 bias) {
  for (u32 q = 0; q < n; q++) {
    v[q] = bias + scale * (f32)((i32)(q * 7u % 23u) - 11);
  }
}

// A scatter of (base, idx, src) with the given kind; compares with the
// reference and checks the kernel.
static void scatter_case(Shape const *bs, Shape const *is, u32 dim, u32 kind,
                         u32 self, f32 const *base, i32 const *idx,
                         f32 const *src, u32 want_stores, char const *what) {
  u32 bn = numel_of(bs), in = numel_of(is);
  f32 *want = (f32 *)malloc((size_t)bn * 4);
  f32 *got  = (f32 *)malloc((size_t)bn * 4);
  if (kind == SCATTER_SUM || kind == SCATTER_PROD) {
    ref_fold(kind, base, bs, idx, src, is, dim, (int)self, want);
  } else {
    ref_replace(base, bs, idx, src, is, dim, want);
  }
  Term s = uop_scatter(ten(mk(DT_FP32, bs->ndim, bs->dims, base)),
                       ten(mk(DT_INT32, is->ndim, is->dims, idx)),
                       ten(mk(DT_FP32, is->ndim, is->dims, src)), dim, kind, self);
  CHECK(s != 0);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(s, got, bn);
  check_bytes(got, want, bn, what);
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_scatter_kernel(tid, k0, want_stores, bn, in);
  free(want);
  free(got);
}

// === 1. SUM with duplicates and invalid ids ===
#define SV 5
#define SD 3
#define SE 6
static i32 const IDX0[SE * SD] = {
  1, 3, 1,   -1, SV, 0,   1, 1, 4,
  10982377, 2, 2,   0, -19238, 4,   1, 3, 1,
};

static void test_sum(void) {
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  f32 base[SV * SD], src[SE * SD];
  // Large base, fractional src: S + base rounds differently from summing
  // src into base one element at a time.
  fill(base, SV * SD, 1.0e6f, 3.0e7f);
  fill(src, SE * SD, 0.37f, 0.11f);
  f32 s_first[SV * SD];
  ref_sum(base, &bs, IDX0, src, &is, 0, 1, s_first);
  f32 seq[SV * SD];
  memcpy(seq, base, sizeof seq);
  for (u32 p = 0; p < SE * SD; p++) {
    i32 j = target_of(p, &bs, &is, IDX0, 0);
    if (j >= 0) {
      seq[j] = seq[j] + src[p];
    }
  }
  CHECK(memcmp(seq, s_first, sizeof seq) != 0);
  scatter_case(&bs, &is, 0, SCATTER_SUM, 1, base, IDX0, src, 3, "sum self dim 0");
  scatter_case(&bs, &is, 0, SCATTER_SUM, 0, base, IDX0, src, 3, "sum no-self dim 0");
  // Along dim 1: base (3, 6), idx (3, 4).
  Shape bs1 = shape2(3, 6), is1 = shape2(3, 4);
  static i32 const idx1[12] = { 5, 0, 5, 6,  -1, 2, 2, 2,  3, 1, 0, 3 };
  f32 base1[18], src1[12];
  fill(base1, 18, 0.5f, 0.0f);
  fill(src1, 12, 0.25f, 0.0f);
  scatter_case(&bs1, &is1, 1, SCATTER_SUM, 1, base1, idx1, src1, 3, "sum self dim 1");
  scatter_case(&bs1, &is1, 1, SCATTER_SUM, 0, base1, idx1, src1, 3, "sum no-self dim 1");
}

// === 2. an identity-CONST base drops the finishing store ===
static void test_identity_base(void) {
  u32 bd[2] = { SV, SD }, id[2] = { SE, SD };
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  f32 zero[SV * SD], src[SE * SD];
  memset(zero, 0, sizeof zero);
  fill(src, SE * SD, 0.37f, 0.11f);
  f32 want[SV * SD], got[SV * SD];
  ref_sum(zero, &bs, IDX0, src, &is, 0, 1, want);
  Term s = uop_scatter(fconst(2, bd, 0.0f), ten(mk(DT_INT32, 2, id, IDX0)),
                       ten(mk(DT_FP32, 2, id, src)), 0, SCATTER_SUM, 1);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(s, got, SV * SD);
  check_bytes(got, want, SV * SD, "sum into zeros");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_scatter_kernel(tid, k0, 2, SV * SD, SE * SD);
}

// === 3. all-invalid idx, REPLACE ===
static void test_invalid_and_replace(void) {
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  f32 base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  fill(src, SE * SD, 0.75f, 0.0f);
  i32 none[SE * SD];
  for (u32 p = 0; p < SE * SD; p++) {
    none[p] = (p % 2) ? -1 : SV + (i32)p;
  }
  scatter_case(&bs, &is, 0, SCATTER_SUM, 1, base, none, src, 3, "sum all-invalid");
  scatter_case(&bs, &is, 0, SCATTER_SUM, 0, base, none, src, 3, "sum no-self all-invalid");
  scatter_case(&bs, &is, 0, SCATTER_REPLACE, 0, base, IDX0, src, 2, "replace");
  scatter_case(&bs, &is, 0, SCATTER_REPLACE, 0, base, none, src, 2, "replace all-invalid");
}

// === 4. idx forms and a computed src ===
static Term rows(Term ids, u32 n, u32 d) {
  u32 n1[2] = { n, 1 };
  u32 nd[2] = { n, d };
  return uop_expand(uop_reshape(ids, 2, n1), 2, nd);
}

static void test_operand_forms(void) {
  Shape bs = shape2(SV, SD);
  u32 bd[2] = { SV, SD };
  f32 base[SV * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  // idx expanded from an {E} tensor.
  enum { E = 11 };
  static i32 const ids[E] = { 4, 0, 4, -1, 2, SV, 1, 4, 0, 3, 2 };
  i32 grid[E * SD];
  for (u32 p = 0; p < E * SD; p++) {
    grid[p] = ids[p / SD];
  }
  Shape is = shape2(E, SD);
  u32 id[2] = { E, SD };
  f32 src[E * SD];
  fill(src, E * SD, 0.37f, 0.11f);
  f32 want[SV * SD], got[SV * SD];
  ref_sum(base, &bs, grid, src, &is, 0, 1, want);
  u32 e1[1] = { E };
  Term s = uop_scatter(ten(mk(DT_FP32, 2, bd, base)),
                       rows(ten(mk(DT_INT32, 1, e1, ids)), E, SD),
                       ten(mk(DT_FP32, 2, id, src)), 0, SCATTER_SUM, 1);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(s, got, SV * SD);
  check_bytes(got, want, SV * SD, "idx expanded");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_scatter_kernel(tid, k0, 3, SV * SD, E * SD);
  // idx = edge_index[1].
  static i32 const edges[2 * E] = { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0,
                                    4, 0, 4, -1, 2, SV, 1, 4, 0, 3, 2 };
  u32 ed[2] = { 2, E };
  u32 ebe[4] = { 1, 2, 0, E };
  Term row1 = uop_reshape(uop_shrink(ten(mk(DT_INT32, 2, ed, edges)), 2, ebe), 1, e1);
  s = uop_scatter(ten(mk(DT_FP32, 2, bd, base)), rows(row1, E, SD),
                  ten(mk(DT_FP32, 2, id, src)), 0, SCATTER_SUM, 1);
  k0 = KERNELS_NEXT;
  tid = realize_into(s, got, SV * SD);
  check_bytes(got, want, SV * SD, "idx edge_index[1]");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_scatter_kernel(tid, k0, 3, SV * SD, E * SD);
  // A computed src realizes into its own kernel.
  f32 neg[E * SD];
  for (u32 p = 0; p < E * SD; p++) {
    neg[p] = -src[p];
  }
  ref_sum(base, &bs, grid, neg, &is, 0, 0, want);
  s = uop_scatter(ten(mk(DT_FP32, 2, bd, base)),
                  rows(ten(mk(DT_INT32, 1, e1, ids)), E, SD),
                  uop_unary(UOP_NEG, ten(mk(DT_FP32, 2, id, src))), 0,
                  SCATTER_SUM, 0);
  k0 = KERNELS_NEXT;
  tid = realize_into(s, got, SV * SD);
  check_bytes(got, want, SV * SD, "computed src");
  CHECK_EQ(KERNELS_NEXT - k0, 2u);
  check_dispatch(k0);
  check_scatter_kernel(tid, k0, 3, SV * SD, E * SD);
}

enum {
  BASE_ALIAS, BASE_PERMUTE_ALIAS, BASE_SHRINK_ROWS, BASE_SHRINK_COLS,
  BASE_FLIP, BASE_COMPUTED, BASE_FORMS
};

// A fresh (SV, SD) base of the given form; eff receives its logical values.
static Term base_form(u32 form, f32 *eff) {
  u32 bd[2] = { SV, SD };
  f32 raw[(SV + 2) * (SD + 2)];
  fill(raw, (SV + 2) * (SD + 2), 0.5f, 0.25f);
  switch (form) {
    case BASE_ALIAS: {
      memcpy(eff, raw + SD, (size_t)SV * SD * 4);
      u32 d[2] = { SV + 1, SD }, be[4] = { 1, SV + 1, 0, SD };
      Term a = materialize_uop_in_env(uop_shrink(ten(mk(DT_FP32, 2, d, raw)), 2, be), 0);
      CHECK(term_tag(a) == TAG_TEN && TENS[(u32)term_val(a)].view.offset != 0);
      return a;
    }
    case BASE_PERMUTE_ALIAS: {
      for (u32 r = 0; r < SV; r++) {
        for (u32 c = 0; c < SD; c++) {
          eff[r * SD + c] = raw[c * SV + r];
        }
      }
      u32 d[2] = { SD, SV }, perm[2] = { 1, 0 };
      Term a = materialize_uop_in_env(uop_permute(ten(mk(DT_FP32, 2, d, raw)), 2, perm), 0);
      CHECK(term_tag(a) == TAG_TEN && !TENS[(u32)term_val(a)].view.contiguous);
      return a;
    }
    case BASE_SHRINK_ROWS: {
      memcpy(eff, raw + 2 * SD, (size_t)SV * SD * 4);
      u32 d[2] = { SV + 2, SD }, be[4] = { 2, SV + 2, 0, SD };
      return uop_shrink(ten(mk(DT_FP32, 2, d, raw)), 2, be);
    }
    case BASE_SHRINK_COLS: {
      for (u32 r = 0; r < SV; r++) {
        for (u32 c = 0; c < SD; c++) {
          eff[r * SD + c] = raw[r * (SD + 2) + c + 1];
        }
      }
      u32 d[2] = { SV, SD + 2 }, be[4] = { 0, SV, 1, SD + 1 };
      return uop_shrink(ten(mk(DT_FP32, 2, d, raw)), 2, be);
    }
    case BASE_FLIP: {
      for (u32 r = 0; r < SV; r++) {
        for (u32 c = 0; c < SD; c++) {
          eff[r * SD + c] = raw[(SV - 1 - r) * SD + c];
        }
      }
      return uop_flip(ten(mk(DT_FP32, 2, bd, raw)), 1u);
    }
    default: {
      for (u32 q = 0; q < SV * SD; q++) {
        eff[q] = -raw[q];
      }
      return uop_unary(UOP_NEG, ten(mk(DT_FP32, 2, bd, raw)));
    }
  }
}

static void test_base_views(void) {
  static char const *const names[BASE_FORMS] = {
    "view-alias TEN", "PERMUTE view-alias TEN", "row SHRINK", "column SHRINK",
    "FLIP", "computed",
  };
  static u32 const kinds[3]  = { SCATTER_SUM, SCATTER_SUM, SCATTER_REPLACE };
  static u32 const selfs[3]  = { 1, 0, 0 };
  static u32 const stores[3] = { 3, 3, 2 };
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  u32 id[2] = { SE, SD };
  f32 src[SE * SD];
  fill(src, SE * SD, 0.37f, 0.11f);
  for (u32 f = 0; f < BASE_FORMS; f++) {
    for (u32 k = 0; k < 3; k++) {
      f32 eff[SV * SD], want[SV * SD], got[SV * SD];
      Term b = base_form(f, eff);
      if (kinds[k] == SCATTER_SUM) {
        ref_sum(eff, &bs, IDX0, src, &is, 0, (int)selfs[k], want);
      } else {
        ref_replace(eff, &bs, IDX0, src, &is, 0, want);
      }
      Term s = uop_scatter(b, ten(mk(DT_INT32, 2, id, IDX0)), ten(mk(DT_FP32, 2, id, src)),
                           0, kinds[k], selfs[k]);
      CHECK(s != 0);
      char what[64];
      snprintf(what, sizeof what, "base %s, kind %u self %u", names[f], kinds[k], selfs[k]);
      u32 k0 = KERNELS_NEXT;
      u32 tid = realize_into(s, got, SV * SD);
      check_bytes(got, want, SV * SD, what);
      CHECK_EQ(KERNELS_NEXT - k0, 1u);
      check_dispatch(k0);
      check_scatter_kernel(tid, k0, stores[k], SV * SD, SE * SD);
    }
  }
}

// Message passing: out[dst[e]] += h[src[e]] over the valid edges, a gather
// feeding a scatter (the gathered messages realize first: 2 kernels).
static void test_message_passing(void) {
  enum { V = 6, D = 4, E = 13 };
  static i32 const srcs[E] = { 0, 1, 2, 3, 4, 5, 0, 2, 4, -1, 6, 1, 3 };
  static i32 const dsts[E] = { 1, 2, 3, 4, 5, 0, 5, 5, 5, 2, 1, -3, 0 };
  f32 h[V * D];
  fill(h, V * D, 0.37f, 0.11f);
  u32 vd[2] = { V, D }, e1[1] = { E };
  Term msgs = uop_gather(ten(mk(DT_FP32, 2, vd, h)),
                         rows(ten(mk(DT_INT32, 1, e1, srcs)), E, D), 0);
  Term agg = uop_scatter(fconst(2, vd, 0.0f), rows(ten(mk(DT_INT32, 1, e1, dsts)), E, D),
                         msgs, 0, SCATTER_SUM, 1);
  CHECK(msgs != 0 && agg != 0);
  f32 m[E * D];
  i32 grid[E * D];
  for (u32 p = 0; p < E * D; p++) {
    i32 s = srcs[p / D];
    m[p] = (s >= 0 && s < V) ? h[(u32)s * D + p % D] : 0.0f;
    grid[p] = dsts[p / D];
  }
  f32 zero[V * D], want[V * D], got[V * D];
  memset(zero, 0, sizeof zero);
  Shape bs = shape2(V, D), is = shape2(E, D);
  ref_sum(zero, &bs, grid, m, &is, 0, 1, want);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(agg, got, V * D);
  check_bytes(got, want, V * D, "message passing");
  CHECK_EQ(KERNELS_NEXT - k0, 2u);
  check_dispatch(k0);
  check_scatter_kernel(tid, k0, 2, V * D, E * D);
}

// A src viewing a realized view: the adjoints of three gathers from one
// table, as the backward of a dim-1 cat over a dim-0 cat of gathered pieces
// builds them.  dE = C[:, :4] of a computed C is read through three
// different row views, so it realizes; two pieces scale their rows, the
// third feeds its scatter as a bare view of dE.  That src reads dE's
// buffer, the storage its kernel binds and the arena keeps alive: an
// address composed on down to C read bytes the arena had already handed to
// a later kernel.  Integer data keeps every sum exact in any order.
static void test_src_view_of_realized_view(void) {
  enum { V = 8, D = 4, Q = 4, W = 6, N = 3 * Q };
  static i32 const ids[3][Q] = { { 2, 7, 2, 6 }, { 1, 3, 1, 1 }, { 7, 6, 0, 7 } };
  f32 x[N * W];
  for (u32 q = 0; q < N * W; q++) {
    x[q] = (f32)(q + 1);
  }
  u32 nw[2] = { N, W }, vd[2] = { V, D }, qd[2] = { Q, D }, q1[1] = { Q };
  Term c = uop_binary(UOP_MUL, ten(mk(DT_FP32, 2, nw, x)), fconst(2, nw, 1.0f));
  u32 cols[4] = { 0, N, 0, D };
  Term de = uop_shrink(c, 2, cols);
  Term sum = 0;
  f32 want[V * D];
  memset(want, 0, sizeof want);
  for (u32 j = 0; j < 3; j++) {
    u32 be[4] = { j * Q, (j + 1) * Q, 0, D };
    Term piece = uop_shrink(de, 2, be);
    f32 scale = 1.0f;
    if (j < 2) {
      piece = uop_binary(UOP_MUL, piece, fconst(2, qd, 2.0f));
      scale = 2.0f;
    }
    Term s = uop_scatter(fconst(2, vd, 0.0f), rows(ten(mk(DT_INT32, 1, q1, ids[j])), Q, D),
                         piece, 0, SCATTER_SUM, 1);
    CHECK(s != 0);
    sum = (sum == 0) ? s : uop_binary(UOP_ADD, sum, s);
    for (u32 r = 0; r < Q; r++) {
      for (u32 d = 0; d < D; d++) {
        want[(u32)ids[j][r] * D + d] += x[(j * Q + r) * W + d] * scale;
      }
    }
  }
  f32 got[V * D];
  u32 k0 = KERNELS_NEXT;
  realize_into(sum, got, V * D);
  check_bytes(got, want, V * D, "src view of a realized view");
  check_dispatch(k0);
}

// === 5. the scatter as an intermediate ===
static void test_intermediate(void) {
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  u32 bd[2] = { SV, SD }, id[2] = { SE, SD };
  f32 base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  fill(src, SE * SD, 0.37f, 0.11f);
  // A released tensor of the scatter's size leaves a recyclable buffer
  // holding large garbage values.
  f32 junk[SV * SD];
  for (u32 q = 0; q < SV * SD; q++) {
    junk[q] = 1.0e30f;
  }
  f32 tmp[SV * SD];
  u32 jr = realize_into(uop_unary(UOP_NEG, ten(mk(DT_FP32, 2, bd, junk))), tmp,
                        SV * SD);
  if (jr != 0) {
    tensor_release(jr);
  }
  f32 sum[SV * SD], want[SV * SD], got[SV * SD];
  ref_sum(base, &bs, IDX0, src, &is, 0, 0, sum);
  for (u32 q = 0; q < SV * SD; q++) {
    want[q] = sum[q] * 2.0f;
  }
  Term s = uop_scatter(ten(mk(DT_FP32, 2, bd, base)), ten(mk(DT_INT32, 2, id, IDX0)),
                       ten(mk(DT_FP32, 2, id, src)), 0, SCATTER_SUM, 0);
  u32 k0 = KERNELS_NEXT;
  realize_into(uop_binary(UOP_MUL, s, fconst(2, bd, 2.0f)), got, SV * SD);
  check_bytes(got, want, SV * SD, "scatter * 2");
  CHECK_EQ(KERNELS_NEXT - k0, 2u);
  check_dispatch(k0);
}

// === 7. JIT capture, replays with the index rewritten in place ===
static void test_jit_replay(void) {
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  u32 bd[2] = { SV, SD }, id[2] = { SE, SD };
  f32 base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  fill(src, SE * SD, 0.37f, 0.11f);
  i32 idx[4][SE * SD];
  for (u32 rep = 0; rep < 4; rep++) {
    for (u32 p = 0; p < SE * SD; p++) {
      idx[rep][p] = (i32)((p * (3u + rep) + rep) % (SV + 2)) - 1;
    }
  }
  u32 it = mk(DT_INT32, 2, id, idx[0]);
  Term s = uop_scatter(ten(mk(DT_FP32, 2, bd, base)), ten(it),
                       ten(mk(DT_FP32, 2, id, src)), 0, SCATTER_SUM, 0);
  u32 slot = jit_capture_begin();
  CHECK(slot != 0);
  Term r = term_resolve(thvm_realize(s));
  jit_capture_end_with_result(r);
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  u32 tid = (u32)term_val(r);
  f32 want[SV * SD], got[SV * SD];
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, SV * SD * 4);
  ref_sum(base, &bs, idx[0], src, &is, 0, 0, want);
  check_bytes(got, want, SV * SD, "capture");
  for (u32 rep = 1; rep < 4; rep++) {
    CPU_BACKEND.buf_write(TENS[it].buf_id, idx[rep], SE * SD * 4);
    f32 poison[SV * SD];
    for (u32 q = 0; q < SV * SD; q++) {
      poison[q] = -999.0f;
    }
    TENS[tid].backend->buf_write(TENS[tid].buf_id, poison, SV * SD * 4);
    jit_replay(slot);
    TENS[tid].backend->buf_read(TENS[tid].buf_id, got, SV * SD * 4);
    ref_sum(base, &bs, idx[rep], src, &is, 0, 0, want);
    char what[32];
    snprintf(what, sizeof what, "replay %u", rep);
    check_bytes(got, want, SV * SD, what);
  }
  jit_capture_drop(slot);
}

// === 8. PROD, tinygrad's scatter_reduce "prod" (mixin/op.py:1128) ===
static void test_prod(void) {
  Shape bs = shape2(SV, SD), is = shape2(SE, SD);
  f32 base[SV * SD], src[SE * SD];
  fill(base, SV * SD, 0.5f, 0.25f);
  fill(src, SE * SD, 0.37f, 0.11f);
  scatter_case(&bs, &is, 0, SCATTER_PROD, 1, base, IDX0, src, 3, "prod self dim 0");
  scatter_case(&bs, &is, 0, SCATTER_PROD, 0, base, IDX0, src, 3, "prod no-self dim 0");
  Shape bs1 = shape2(3, 6), is1 = shape2(3, 4);
  static i32 const idx1[12] = { 5, 0, 5, 6,  -1, 2, 2, 2,  3, 1, 0, 3 };
  f32 base1[18], src1[12];
  fill(base1, 18, 0.5f, 0.0f);
  fill(src1, 12, 0.25f, 0.0f);
  scatter_case(&bs1, &is1, 1, SCATTER_PROD, 1, base1, idx1, src1, 3, "prod self dim 1");
  scatter_case(&bs1, &is1, 1, SCATTER_PROD, 0, base1, idx1, src1, 3, "prod no-self dim 1");
}

int main(void) {
  thvm_init();
  TEST_BEGIN("scatter/sum");
  test_sum();
  TEST_BEGIN("scatter/identity-base");
  test_identity_base();
  TEST_BEGIN("scatter/invalid-and-replace");
  test_invalid_and_replace();
  TEST_BEGIN("scatter/operand-forms");
  test_operand_forms();
  TEST_BEGIN("scatter/base-views");
  test_base_views();
  TEST_BEGIN("scatter/message-passing");
  test_message_passing();
  TEST_BEGIN("scatter/src-view-of-a-realized-view");
  test_src_view_of_realized_view();
  TEST_BEGIN("scatter/intermediate");
  test_intermediate();
  TEST_BEGIN("scatter/jit-replay");
  test_jit_replay();
  TEST_BEGIN("scatter/prod");
  test_prod();
  TEST_BEGIN("scatter/dispatched");
  CHECK(DISPATCHED >= 12);
  thvm_free();
  TEST_REPORT();
}
