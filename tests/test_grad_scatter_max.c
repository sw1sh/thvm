// test_grad_scatter_max.c -- the chain rule through UOP_SCATTER MAX and MIN.
//
// tinygrad differentiates scatter_reduce amax through its dense formulation
// (mixin/op.py:1129):
//   out = M.maximum(T),  M = W.max(-1),  W = mask.where(src, I),
//   T   = self if include_self else _inv_mask(self, I)
// with MAX's rule (mixin/gradient.py:78-79), the REDUCE MAX rule (:10-13)
// and WHERE's (:81).  With gy the cotangent of out, that is
//   g_M[j]    = M > T ? gy : M == T ? gy * 0.5 : 0
//   g_T[j]    = M < T ? gy : M == T ? gy * 0.5 : 0
//   g_base[j] = g_T[j] where base reaches T (include_self, or j untouched)
//   cnt[j]    = #{k < K : W[j, k] == M[j]}, K = idx.shape[dim]: the hits
//               equal to M, plus K - hits when M is I (the one-hot's
//               non-hits are I too)
//   g_src[p]  = ((src[p] == M[t(p)]) * (1 / cnt[t(p)])) * g_M[t(p)] for a
//               valid p, else 0
// so tied maxima split the cotangent evenly, the base takes half of a tie
// with M, and an untouched base of I keeps half (its M is I too).  amin is
// the mirror image, down to the -0.0 of a losing branch.  The host reference computes exactly this in f32, the
// order tinygrad's kernels round in.
//
//   1. a case pinned to tinygrad master 365f87a8a's own gradients: a
//      three-way tie that also ties the base, hits equal to -inf, an
//      untouched base of -inf, invalid ids, include_self 0 and 1;
//   2. tie-rich data (values from {I, -1, 0, 0.5, 2}) with duplicate and
//      invalid ids {-1, V, 10982377, -19238}, dims 0 and 1, include_self 0
//      and 1, against the reference;
//   3. central finite differences (host f64) of a quadratic loss on data
//      without ties;
//   4. a computed src (its gradient chains through the rule);
//   5. every case runs in the requires_grad single walk (no stuck or excess
//      slots), the target-aware walk and the legacy SUP/DUP projection;
//   6. under a JIT capture, which keeps every kernel's lift through the
//      end-of-realize kernel sweep, every backward kernel holds no REDUCE
//      and loops at most max(|base|, |idx|) times per store, and replays
//      with the ids rewritten in place match the reference for the new ids;
//   7. a half scatter: the tie count accumulates in float32 (sum_acc_dtype)
//      and the cotangent comes back in half;
//   8. a copying GC between firing the chain rule and realizing it keeps
//      the backward graph (its MAX, SUM and REPLACE scatters and gathers);
//   9. no grad error is raised for MAX / MIN.
//
// Run twice, as test_grad_gather_scatter: as is (C JIT) and with
// THVM_CPU_JIT_WARMUP=1000000000 (the UOp walker).
#include "../src/thvm.c"
#include "test.h"

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

static void unravel(u32 p, Shape const *s, u32 *c) {
  for (i32 d = (i32)s->ndim - 1; d >= 0; d--) {
    c[d] = p % s->dims[d];
    p /= s->dims[d];
  }
}

static u32 ravel(u32 const *c, Shape const *s) {
  u32 q = 0;
  for (u32 d = 0; d < s->ndim; d++) {
    q = q * s->dims[d] + c[d];
  }
  return q;
}

// The position of base (shape bs) that index position p writes, or -1 when
// idx[p] is out of range.
static i32 target_of(u32 p, Shape const *is, i32 const *idx, u32 dim,
                     Shape const *bs) {
  u32 c[MAX_DIM];
  unravel(p, is, c);
  if (idx[p] < 0 || (u32)idx[p] >= bs->dims[dim]) {
    return -1;
  }
  c[dim] = (u32)idx[p];
  return (i32)ravel(c, bs);
}

// === host reference (f32, tinygrad's rounding order) ===
static f32 comb(u32 kind, f32 acc, f32 x) {
  if (kind == SCATTER_MAX) {
    return (acc < x) ? x : acc;
  }
  return (x < acc) ? x : acc;
}

static void ref_forward(u32 kind, f32 const *base, Shape const *bs,
                        i32 const *idx, f32 const *src, Shape const *is,
                        u32 dim, u32 s, f32 *out) {
  u32  bn  = numel_of(bs), in = numel_of(is);
  f32  idv = (kind == SCATTER_MAX) ? -INFINITY : INFINITY;
  f32 *m   = (f32 *)malloc((size_t)bn * 4);
  u8  *hit = (u8 *)calloc(bn, 1);
  for (u32 j = 0; j < bn; j++) {
    m[j] = idv;
  }
  for (u32 p = 0; p < in; p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t >= 0) {
      m[t] = comb(kind, m[t], src[p]);
      hit[t] = 1;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    out[j] = comb(kind, m[j], (s || !hit[j]) ? base[j] : idv);
  }
  free(m);
  free(hit);
}

static void ref_grad(u32 kind, f32 const *base, Shape const *bs, i32 const *idx,
                     f32 const *src, Shape const *is, u32 dim, u32 s,
                     f32 const *gy, f32 *gb, f32 *gs) {
  u32  bn   = numel_of(bs), in = numel_of(is), k = is->dims[dim];
  f32  idv  = (kind == SCATTER_MAX) ? -INFINITY : INFINITY;
  f32 *m    = (f32 *)malloc((size_t)bn * 4);
  f32 *cnt  = (f32 *)malloc((size_t)bn * 4);
  f32 *gm   = (f32 *)malloc((size_t)bn * 4);
  u32 *hits = (u32 *)calloc(bn, sizeof(u32));
  for (u32 j = 0; j < bn; j++) {
    m[j] = idv;
  }
  for (u32 p = 0; p < in; p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t >= 0) {
      m[t] = comb(kind, m[t], src[p]);
      hits[t]++;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    cnt[j] = (m[j] == idv) ? (f32)(k - hits[j]) : 0.0f;
  }
  for (u32 p = 0; p < in; p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t >= 0 && src[p] == m[t]) {
      cnt[t] += 1.0f;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    int reach = s || hits[j] == 0;
    f32 t     = reach ? base[j] : idv;
    int m_win = (kind == SCATTER_MAX) ? (t < m[j]) : (m[j] < t);
    int t_win = (kind == SCATTER_MAX) ? (m[j] < t) : (t < m[j]);
    int tie   = (m[j] == t);
    f32 half  = gy[j] * 0.5f;
    gm[j] = m_win ? gy[j] : (tie ? half : 0.0f);
    f32 gt = t_win ? gy[j] : (tie ? half : 0.0f);
    gb[j] = reach ? gt : 0.0f;
  }
  for (u32 p = 0; p < in; p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t < 0) {
      gs[p] = 0.0f;
      continue;
    }
    f32 c = (src[p] == m[t]) ? 1.0f : 0.0f;
    gs[p] = (c * (1.0f / cnt[t])) * gm[t];
  }
  free(m);
  free(cnt);
  free(gm);
  free(hits);
}

static int check_eq(f32 const *got, f32 const *want, u32 n, char const *what) {
  u32 shown = 0, bad = 0;
  for (u32 i = 0; i < n; i++) {
    if (!(got[i] == want[i])) {
      bad++;
      if (shown < 8) {
        fprintf(stderr, "  %s: [%u] got=%.9g want=%.9g\n", what, i, got[i], want[i]);
        shown++;
      }
    }
  }
  CHECK(bad == 0);
  return bad == 0;
}

static u32 DISPATCHED = 0;

// The CPU JIT declines a kernel with a narrow-float buffer
// (cpu_jit_narrow_float, backend/cpu/jit.c), so the walker runs it in
// either mode.
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
        || kind == KDISPATCH_BLAS_GEMM) {
      continue;
    }
    u32 want = (cpu_jit_walker_only() || jit_declines(k)) ? KDISPATCH_INTERPRETER
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

// Realize t into out (n words of `bytes` each).  Returns the result tid, 0
// when the realize produced no dense tensor of n elements.  With a non-NULL
// slot the realize runs under a JIT capture, left open for replays: the
// capture pins every buffer it records, so the end-of-realize kernel sweep
// (kernel_gc_sweep) leaves each kernel's lift and inputs for inspection.
static u32 realize_bytes_in(Term t, void *out, u32 n, u32 bytes, u32 *slot) {
  u32 k0 = KERNELS_NEXT;
  if (slot != NULL) {
    *slot = jit_capture_begin();
    CHECK(*slot != 0);
  }
  Term r  = term_resolve(thvm_realize(t));
  if (slot != NULL) {
    jit_capture_end_with_result(r);
  }
  int  ok = term_tag(r) == TAG_TEN;
  CHECK(ok);
  u32 tid = ok ? (u32)term_val(r) : 0;
  if (ok) {
    View const *v = &TENS[tid].view;
    ok = v->numel == n && v->contiguous && v->offset == 0 && TENS[tid].nviews == 0;
    CHECK(ok);
  }
  if (!ok) {
    memset(out, 0, (size_t)n * bytes);
    return 0;
  }
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, (u64)n * bytes);
  check_dispatch(k0);
  return tid;
}

static u32 realize_bytes(Term t, void *out, u32 n, u32 bytes) {
  return realize_bytes_in(t, out, n, bytes, NULL);
}

static u32 realize_into(Term t, f32 *out, u32 n) {
  return realize_bytes(t, out, n, 4);
}

// The legacy walk (no target, no requires_grad leaf) leaves SUP^{tid}(0, g)
// at every leaf it reaches; the WL DUP^{tid} nest projects them.  Take the
// match side for `tid` and the zero side for every other leaf, rebuilding
// the UOp nodes above.
static Term project(Term t, u32 tid, u32 depth) {
  Term r = term_resolve(t);
  if (depth > 256) {
    return r;
  }
  if (term_tag(r) == TAG_SUP) {
    u32 side = (term_ext(r) == tid) ? 1u : 0u;
    return project(heap_read(term_val(r) + side), tid, depth + 1);
  }
  if (term_tag(r) != TAG_UOP) {
    return r;
  }
  u8   op = term_ext(r);
  u8   ar = uop_arity(op);
  Term srcs[MAX_UOP_SRC] = {0};
  int  changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term c = heap_read(term_val(r) + i);
    srcs[i] = project(c, tid, depth + 1);
    if (srcs[i] != c) {
      changed = 1;
    }
  }
  if (!changed) {
    return r;
  }
  if (op == UOP_IWHERE) {
    return uop_where(srcs[0], srcs[1], srcs[2]);
  }
  return uop_graph_rebuild_with_srcs(r, srcs);
}

enum { MODE_WALK, MODE_TARGET, MODE_LEGACY, MODE_COUNT };
static char const *const MODE_NAME[MODE_COUNT] = { "single-walk", "target", "legacy" };

// The vector-Jacobian product of y at cotangent `seed` with respect to each
// leaf tids[i], realized into out[i].  Returns 1 when every gradient
// realized.
static int vjp(Term y, Term seed, u32 mode, u32 n, u32 const *tids, f32 **out) {
  int ok = 1;
  switch (mode) {
    case MODE_WALK: {
      for (u32 i = 0; i < n; i++) {
        TENS[tids[i]].requires_grad = 1;
        TENS[tids[i]].grad = 0;
        grad_req_ncount_inc();
      }
      wnf(uop_grad(y, seed));
      CHECK_EQ(grad_slot_stuck_get(), 0u);
      CHECK_EQ(grad_slot_excess_get(), 0u);
      for (u32 i = 0; i < n; i++) {
        Term g = TENS[tids[i]].grad;
        CHECK(g != 0);
        ok = ok && g != 0
          && realize_into(g, out[i], TENS[tids[i]].view.numel) != 0;
      }
      for (u32 i = 0; i < n; i++) {
        TENS[tids[i]].grad = 0;
        TENS[tids[i]].requires_grad = 0;
        grad_req_ncount_dec();
      }
      break;
    }
    case MODE_TARGET:
      for (u32 i = 0; i < n; i++) {
        Term g = uop_grad_with_target(y, seed, ten(tids[i]));
        ok = ok && realize_into(g, out[i], TENS[tids[i]].view.numel) != 0;
      }
      break;
    case MODE_LEGACY: {
      CHECK_EQ(GRAD_REQ_NCOUNT, 0u);
      Term r = wnf(uop_grad(y, seed));
      for (u32 i = 0; i < n; i++) {
        Term g = project(r, tids[i], 0);
        ok = ok && realize_into(g, out[i], TENS[tids[i]].view.numel) != 0;
      }
      break;
    }
  }
  CHECK(grad_err_take() == NULL);
  return ok;
}

static char const *kname(u32 kind) {
  return kind == SCATTER_MAX ? "max" : "min";
}

// y = scatter(base, idx, src) differentiated at seed gy in every mode: the
// base and src gradients equal the reference (or want_b / want_s when
// given).
static void grad_case(u32 kind, Shape const *bs, Shape const *is, u32 dim,
                      u32 s, f32 const *base, i32 const *idx, f32 const *src,
                      f32 const *gy, f32 const *want_b, f32 const *want_s,
                      char const *what) {
  u32  bn = numel_of(bs), in = numel_of(is);
  f32 *wb = (f32 *)malloc((size_t)bn * 4);
  f32 *ws = (f32 *)malloc((size_t)in * 4);
  f32 *gb = (f32 *)malloc((size_t)bn * 4);
  f32 *gs = (f32 *)malloc((size_t)in * 4);
  ref_grad(kind, base, bs, idx, src, is, dim, s, gy, wb, ws);
  if (want_b != NULL) {
    char name[96];
    snprintf(name, sizeof name, "%s reference vs tinygrad base", what);
    check_eq(wb, want_b, bn, name);
    snprintf(name, sizeof name, "%s reference vs tinygrad src", what);
    check_eq(ws, want_s, in, name);
  }
  u32  bt = mk(DT_FP32, bs, base);
  u32  st = mk(DT_FP32, is, src);
  u32  ct = mk(DT_FP32, bs, gy);
  Term y  = uop_scatter(ten(bt), ten(mk(DT_INT32, is, idx)), ten(st), dim, kind, s);
  CHECK(y != 0);
  u32 tids[2] = { bt, st };
  for (u32 m = 0; m < MODE_COUNT; m++) {
    char nb[112], ns[112];
    snprintf(nb, sizeof nb, "%s base (%s)", what, MODE_NAME[m]);
    snprintf(ns, sizeof ns, "%s src (%s)", what, MODE_NAME[m]);
    f32 *outs[2] = { gb, gs };
    int ok = vjp(y, ten(ct), m, 2, tids, outs);
    CHECK(ok);
    if (ok) {
      check_eq(gb, want_b != NULL ? want_b : wb, bn, nb);
      check_eq(gs, want_s != NULL ? want_s : ws, in, ns);
    }
  }
  free(wb);
  free(ws);
  free(gb);
  free(gs);
}

// === 1. pinned to tinygrad master ===
// base (4, 2), idx and src (5, 2), dim 0.  Column 0: position 1 is hit by
// three 3s and holds a base of 3; position 2 by one 1.5; the 7 has id -1.
// Column 1: position 0 by two -infs, position 2 by a 5, position 3 is
// untouched with base -inf; the 9 (id 4) and the 8 (id -1) are invalid.
// tinygrad master 365f87a8a (Tensor.gradient of (y * gy).sum()) gives the
// gradients below; its amin case, run on the negated data, matches them
// except that a few zeros come back as -0.0.
static void test_pinned(void) {
  u32   bd[2] = { 4, 2 }, id[2] = { 5, 2 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx[10] = { 1, 0,  1, 0,  1, 2,  -1, 4,  2, -1 };
  static f32 const gy[8]   = { 1, 2, 3, 4, 5, 6, 7, 8 };
  f32 const src[10] = { 3, -INFINITY, 3, -INFINITY, 3, 5, 7, 9, 1.5f, 8 };
  f32 const base[8] = { 0.5f, 2, 3, 1, -2, 6, 4, -INFINITY };
  static f32 const gb1[8]  = { 1, 2, 1.5f, 4, 0, 6, 7, 4 };
  static f32 const gs1[10] = { 0.5f, 0, 0.5f, 0, 0.5f, 0, 0, 0, 5, 0 };
  static f32 const gb0[8]  = { 1, 0, 0, 4, 0, 0, 7, 4 };
  f32 const fifth = 1.0f / 5.0f;
  f32 const gs0[10] = { 1, fifth, 1, fifth, 1, 6, 0, 0, 5, 0 };
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    f32 b[8], sv[10];
    for (u32 q = 0; q < 8; q++) {
      b[q] = (k == SCATTER_MAX) ? base[q] : -base[q];
    }
    for (u32 q = 0; q < 10; q++) {
      sv[q] = (k == SCATTER_MAX) ? src[q] : -src[q];
    }
    char w1[48], w0[48];
    snprintf(w1, sizeof w1, "pinned %s self 1", kname(k));
    snprintf(w0, sizeof w0, "pinned %s self 0", kname(k));
    grad_case(k, &bs, &is, 0, 1, b, idx, sv, gy, gb1, gs1, w1);
    grad_case(k, &bs, &is, 0, 0, b, idx, sv, gy, gb0, gs0, w0);
  }
}

// === 2. tie-rich data ===
static f32 tie_value(u32 kind, u32 h) {
  static f32 const pool[5] = { 0.0f, -1.0f, 0.0f, 0.5f, 2.0f };
  u32 q = h % 5u;
  if (q == 0) {
    return (kind == SCATTER_MAX) ? -INFINITY : INFINITY;
  }
  return pool[q];
}

static void tie_case(u32 kind, Shape const *bs, Shape const *is, u32 dim,
                     i32 const *idx, u32 seed) {
  u32 bn = numel_of(bs), in = numel_of(is);
  f32 base[64], src[64], gy[64];
  for (u32 q = 0; q < bn; q++) {
    base[q] = tie_value(kind, q * 7u + seed * 3u + 1u);
    gy[q]   = 0.125f * (f32)((i32)((q * 11u + seed) % 13u) - 6) + 0.0625f;
  }
  for (u32 q = 0; q < in; q++) {
    src[q] = tie_value(kind, q * 3u + seed);
  }
  for (u32 s = 0; s < 2; s++) {
    char what[64];
    snprintf(what, sizeof what, "ties %s dim %u self %u", kname(kind), dim, s);
    grad_case(kind, bs, is, dim, s, base, idx, src, gy, NULL, NULL, what);
  }
}

static void test_ties(void) {
  // dim 0: 7 rows into 5, duplicates and invalid ids.
  u32   bd[2] = { 5, 3 }, id[2] = { 7, 3 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx0[21] = { 1, 4, 0,  1, -1, 0,  5, 2, 2,  3, 4, 0,
                                1, 1, 10982377,  -19238, 2, 4,  0, 0, 3 };
  // dim 1: idx (3, 5) smaller than base (4, 6) off-axis.
  u32   bd1[2] = { 4, 6 }, id1[2] = { 3, 5 };
  Shape bs1 = shape_of(2, bd1), is1 = shape_of(2, id1);
  i32 idx1[15];
  for (u32 p = 0; p < 15; p++) {
    idx1[p] = (i32)((p * 5u) % 9u) - 1;
  }
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 seed = 0; seed < 3; seed++) {
      tie_case(k, &bs, &is, 0, idx0, seed);
      tie_case(k, &bs1, &is1, 1, idx1, seed);
    }
  }
}

// === 3. central finite differences (host f64) ===
static double comb_d(u32 kind, double acc, double x) {
  if (kind == SCATTER_MAX) {
    return (acc < x) ? x : acc;
  }
  return (x < acc) ? x : acc;
}

static void f64_forward(u32 kind, double const *base, Shape const *bs,
                        i32 const *idx, double const *src, Shape const *is,
                        u32 dim, u32 s, double *out) {
  u32     bn  = numel_of(bs), in = numel_of(is);
  double  idv = (kind == SCATTER_MAX) ? -INFINITY : INFINITY;
  double *m   = (double *)malloc((size_t)bn * sizeof(double));
  u8     *hit = (u8 *)calloc(bn, 1);
  for (u32 j = 0; j < bn; j++) {
    m[j] = idv;
  }
  for (u32 p = 0; p < in; p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t >= 0) {
      m[t] = comb_d(kind, m[t], src[p]);
      hit[t] = 1;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    out[j] = comb_d(kind, m[j], (s || !hit[j]) ? base[j] : idv);
  }
  free(m);
  free(hit);
}

// sum_j c[j] * y[j]^2
static double quad(double const *y, f32 const *c, u32 n) {
  double s = 0.0;
  for (u32 j = 0; j < n; j++) {
    s += (double)c[j] * y[j] * y[j];
  }
  return s;
}

static void check_fd(f32 const *g, double const *fd, u32 n, char const *what) {
  u32 shown = 0, bad = 0;
  for (u32 i = 0; i < n; i++) {
    if (fabs((double)g[i] - fd[i]) > 1e-4 * (1.0 + fabs(fd[i]))) {
      bad++;
      if (shown < 8) {
        fprintf(stderr, "  %s: [%u] got=%.9g fd=%.9g\n", what, i, g[i], fd[i]);
        shown++;
      }
    }
  }
  CHECK(bad == 0);
}

static void fd_case(u32 kind, Shape const *bs, Shape const *is, u32 dim,
                    i32 const *idx, u32 s) {
  u32 bn = numel_of(bs), in = numel_of(is);
  // src from 0.05 k - 2.5, base from the same grid shifted by 0.025: every
  // value distinct, at least 0.025 from any other.
  f32 base[64], src[64], c[64], gb[64], gs[64];
  for (u32 q = 0; q < bn; q++) {
    base[q] = 0.05f * (f32)((q * 53u + 7u) % 97u) - 2.475f;
    c[q]    = 0.125f * (f32)((i32)((q * 5u + 3u) % 11u) - 5) + 0.0625f;
  }
  for (u32 q = 0; q < in; q++) {
    src[q] = 0.05f * (f32)((q * 37u + 11u) % 101u) - 2.5f;
  }
  double const eps = 1e-3;
  double bx[64], sx[64], y[64], fdb[64], fds[64];
  for (u32 k = 0; k < bn + in; k++) {
    for (u32 q = 0; q < bn; q++) {
      bx[q] = (double)base[q];
    }
    for (u32 q = 0; q < in; q++) {
      sx[q] = (double)src[q];
    }
    double *x = (k < bn) ? &bx[k] : &sx[k - bn];
    *x += eps;
    f64_forward(kind, bx, bs, idx, sx, is, dim, s, y);
    double hi = quad(y, c, bn);
    *x -= 2.0 * eps;
    f64_forward(kind, bx, bs, idx, sx, is, dim, s, y);
    double lo = quad(y, c, bn);
    double d = (hi - lo) / (2.0 * eps);
    if (k < bn) {
      fdb[k] = d;
    } else {
      fds[k - bn] = d;
    }
  }
  u32  bt = mk(DT_FP32, bs, base);
  u32  st = mk(DT_FP32, is, src);
  Term sc = uop_scatter(ten(bt), ten(mk(DT_INT32, is, idx)), ten(st), dim, kind, s);
  Term y2 = uop_binary(UOP_MUL, sc, sc);
  u32  ct = mk(DT_FP32, bs, c);
  u32  tids[2] = { bt, st };
  for (u32 m = 0; m < MODE_COUNT; m++) {
    char nb[80], ns[80];
    snprintf(nb, sizeof nb, "fd %s dim %u self %u base (%s)", kname(kind), dim, s,
             MODE_NAME[m]);
    snprintf(ns, sizeof ns, "fd %s dim %u self %u src (%s)", kname(kind), dim, s,
             MODE_NAME[m]);
    f32 *outs[2] = { gb, gs };
    if (vjp(y2, ten(ct), m, 2, tids, outs)) {
      check_fd(gb, fdb, bn, nb);
      check_fd(gs, fds, in, ns);
    }
  }
}

static void test_fd(void) {
  u32   bd[2] = { 5, 3 }, id[2] = { 6, 3 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx0[18] = { 1, 4, 0,  1, -1, 0,  5, 2, 2,
                                3, 4, 0,  1, 1, 7,  -3, 2, 4 };
  u32   bd1[2] = { 4, 6 }, id1[2] = { 3, 5 };
  Shape bs1 = shape_of(2, bd1), is1 = shape_of(2, id1);
  i32 idx1[15];
  for (u32 p = 0; p < 15; p++) {
    idx1[p] = (i32)((p * 5u) % 9u) - 1;
  }
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 s = 0; s < 2; s++) {
      fd_case(k, &bs, &is, 0, idx0, s);
      fd_case(k, &bs1, &is1, 1, idx1, s);
    }
  }
}

// === 4. a computed src ===
// y = scatter(base, idx, -x): x's gradient is minus the src gradient of the
// scatter at src = -x.
static void test_computed_src(void) {
  u32   bd[2] = { 4, 2 }, id[2] = { 5, 2 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx[10] = { 1, 0,  1, 0,  1, 2,  -1, 3,  2, 1 };
  f32 const x[10]   = { -3, 1, -3, 1, -2.5f, -5, -7, 9, -1.5f, 0.25f };
  f32 const base[8] = { 0.5f, -2, 3, 1, -2, 6, 4, -INFINITY };
  f32 const gy[8]   = { 1, 2, 3, 4, 5, 6, 7, 8 };
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 s = 0; s < 2; s++) {
      f32 negx[10], wb[8], ws[10], wx[10], gb[8], gx[10];
      for (u32 q = 0; q < 10; q++) {
        negx[q] = -x[q];
      }
      ref_grad(k, base, &bs, idx, negx, &is, 0, s, gy, wb, ws);
      for (u32 q = 0; q < 10; q++) {
        wx[q] = -ws[q];
      }
      u32  bt = mk(DT_FP32, &bs, base);
      u32  xt = mk(DT_FP32, &is, x);
      u32  ct = mk(DT_FP32, &bs, gy);
      Term y  = uop_scatter(ten(bt), ten(mk(DT_INT32, &is, idx)),
                            uop_unary(UOP_NEG, ten(xt)), 0, k, s);
      u32 tids[2] = { bt, xt };
      for (u32 m = 0; m < MODE_COUNT; m++) {
        char nb[80], nx[80];
        snprintf(nb, sizeof nb, "computed src %s self %u base (%s)", kname(k), s,
                 MODE_NAME[m]);
        snprintf(nx, sizeof nx, "computed src %s self %u x (%s)", kname(k), s,
                 MODE_NAME[m]);
        f32 *outs[2] = { gb, gx };
        if (vjp(y, ten(ct), m, 2, tids, outs)) {
          check_eq(gb, wb, 8, nb);
          check_eq(gx, wx, 10, nx);
        }
      }
    }
  }
}

// === 6. backward kernels: no REDUCE, linear loops ===
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

static u64 max_store_loops(Term root) {
  if (term_tag(root) == TAG_UOP && term_ext(root) == UOP_AFTER) {
    u64 a = max_store_loops(uop_after_node(root));
    u64 b = max_store_loops(uop_after_after_node(root));
    return a > b ? a : b;
  }
  u32 aid[64], ext[64], n = 0;
  collect_ranges(root, aid, ext, &n, 0);
  u64 prod = 1;
  for (u32 i = 0; i < n; i++) {
    prod *= ext[i];
  }
  return prod;
}

// Every kernel emitted since k0: no REDUCE, at most `bound` iterations per
// store.  Returns the number of kernels checked.
static u32 check_linear_kernels(u32 k0, u64 bound, char const *what) {
  u32 n = 0;
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    Term root = KERNELS[k].cached_lift.store_root;
    if (root == 0) {
      continue;
    }
    n++;
    int no_reduce = !has_op(root, UOP_REDUCE, 0);
    u64 loops = max_store_loops(root);
    if (!no_reduce || loops > bound) {
      fprintf(stderr, "  %s: kernel %u reduce=%d loops=%llu bound=%llu\n", what, k,
              !no_reduce, (unsigned long long)loops, (unsigned long long)bound);
    }
    CHECK(no_reduce);
    CHECK(loops <= bound);
  }
  return n;
}

// One gradient of the linear-kernel case, realized under a JIT capture:
// every backward kernel (all of them keep their lift under the capture)
// holds no REDUCE and loops at most `bound` times per store, at least
// `min_kernels` ran, and replays with the ids rewritten in place match the
// reference for the new ids.
static void linear_grad(Term g, u32 n_out, u32 want_src, u64 bound, u32 min_kernels,
                        u32 kind, u32 s, f32 const *base, Shape const *bs,
                        f32 const *src, Shape const *gs, f32 const *gy, u32 it,
                        i32 const (*ids)[96], u32 n_ids, u32 d, char const *what) {
  f32 *got  = (f32 *)malloc((size_t)n_out * 4);
  f32 *wb   = (f32 *)malloc((size_t)numel_of(bs) * 4);
  f32 *ws   = (f32 *)malloc((size_t)numel_of(gs) * 4);
  i32 *grid = (i32 *)malloc((size_t)numel_of(gs) * 4);
  u32  k0   = KERNELS_NEXT;
  u32  slot = 0;
  u32  tid  = realize_bytes_in(g, got, n_out, 4, &slot);
  u32  n    = check_linear_kernels(k0, bound, what);
  CHECK_EQ(n, KERNELS_NEXT - k0);
  CHECK(n >= min_kernels);
  for (u32 rep = 0; rep < 3; rep++) {
    if (rep > 0) {
      CPU_BACKEND.buf_write(TENS[it].buf_id, (void *)ids[rep], (u64)n_ids * 4);
      if (tid != 0) {
        for (u32 q = 0; q < n_out; q++) {
          got[q] = -999.0f;
        }
        TENS[tid].backend->buf_write(TENS[tid].buf_id, got, (u64)n_out * 4);
        jit_replay(slot);
        TENS[tid].backend->buf_read(TENS[tid].buf_id, got, (u64)n_out * 4);
      }
    }
    for (u32 p = 0; p < numel_of(gs); p++) {
      grid[p] = ids[rep][p / d];
    }
    ref_grad(kind, base, bs, grid, src, gs, 0, s, gy, wb, ws);
    char name[96];
    snprintf(name, sizeof name, "%s replay %u", what, rep);
    check_eq(got, want_src ? ws : wb, n_out, name);
  }
  if (slot != 0) {
    jit_capture_drop(slot);
  }
  CPU_BACKEND.buf_write(TENS[it].buf_id, (void *)ids[0], (u64)n_ids * 4);
  free(got);
  free(wb);
  free(ws);
  free(grid);
}

static void test_linear_kernels(void) {
  enum { V = 1024, D = 8, N = 96 };
  u32   bd[2] = { V, D }, nd[1] = { N }, gd[2] = { N, D };
  Shape bs = shape_of(2, bd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  f32 *base = (f32 *)malloc((size_t)V * D * 4);
  f32 *gy   = (f32 *)malloc((size_t)V * D * 4);
  f32 *src  = (f32 *)malloc((size_t)N * D * 4);
  i32  ids[3][N];
  for (u32 q = 0; q < V * D; q++) {
    base[q] = 0.25f * (f32)((i32)((q * 7u) % 17u) - 8);
    gy[q]   = 0.125f * (f32)((i32)((q * 5u) % 11u) - 5);
  }
  for (u32 q = 0; q < N * D; q++) {
    src[q] = 0.25f * (f32)((i32)((q * 3u) % 19u) - 9);
  }
  for (u32 rep = 0; rep < 3; rep++) {
    for (u32 r = 0; r < N; r++) {
      ids[rep][r] = (i32)(((r + 17u * rep) * 2654435761u) % (V / 8 + 8)) - 4;
    }
  }
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    for (u32 s = 0; s < 2; s++) {
      u32  bt  = mk(DT_FP32, &bs, base);
      u32  st  = mk(DT_FP32, &gs, src);
      u32  ct  = mk(DT_FP32, &bs, gy);
      u32  it  = mk(DT_INT32, &ns, ids[0]);
      Term idx = uop_expand(uop_reshape(ten(it), 2, (u32[2]){ N, 1 }), 2,
                            (u32[2]){ N, D });
      Term y   = uop_scatter(ten(bt), idx, ten(st), 0, k, s);
      char what[64];
      snprintf(what, sizeof what, "linear %s self %u src", kname(k), s);
      linear_grad(uop_grad_with_target(y, ten(ct), ten(st)), N * D, 1, (u64)V * D, 3,
                  k, s, base, &bs, src, &gs, gy, it, (i32 const (*)[96])ids, N, D, what);
      snprintf(what, sizeof what, "linear %s self %u base", kname(k), s);
      linear_grad(uop_grad_with_target(y, ten(ct), ten(bt)), V * D, 0, (u64)V * D, 1,
                  k, s, base, &bs, src, &gs, gy, it, (i32 const (*)[96])ids, N, D, what);
    }
  }
  free(base);
  free(gy);
  free(src);
}

// === 7. a half scatter ===
// A three-way tie: each tied src gets f16((1 * (1/3f)) * 1) of the f16
// cotangent through the f32 count, then that times g_M in half.
static void test_half(void) {
  u32   bd[2] = { 3, 1 }, id[2] = { 4, 1 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx[4] = { 1, 1, 1, 0 };
  f32 const srcf[4] = { 2.5f, 2.5f, 2.5f, -1.0f };
  f32 const basef[3] = { 0.5f, 1.0f, -INFINITY };
  f32 const gyf[3]   = { 3.0f, 6.0f, 10.0f };
  u16 src[4], base[3], gy[3];
  for (u32 q = 0; q < 4; q++) {
    src[q] = f32_to_fp16(srcf[q]);
  }
  for (u32 q = 0; q < 3; q++) {
    base[q] = f32_to_fp16(basef[q]);
    gy[q]   = f32_to_fp16(gyf[q]);
  }
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    // MIN on the negated data mirrors MAX.
    u16 b[3], sv[4];
    for (u32 q = 0; q < 3; q++) {
      b[q] = f32_to_fp16((k == SCATTER_MAX) ? basef[q] : -basef[q]);
    }
    for (u32 q = 0; q < 4; q++) {
      sv[q] = f32_to_fp16((k == SCATTER_MAX) ? srcf[q] : -srcf[q]);
    }
    for (u32 s = 0; s < 2; s++) {
      // Reference: positions 1 (three 2.5s; base 1 or I) and 0 (-1 vs 0.5).
      // g_M at 1 is the full cotangent 6, each tied src gets
      // f16(1/3f) * 6 rounded to half.
      f32 third = fp16_to_f32(f32_to_fp16(1.0f / 3.0f));
      f32 wsrc[4];
      wsrc[0] = wsrc[1] = wsrc[2] = fp16_to_f32(f32_to_fp16(third * 6.0f));
      wsrc[3] = (s == 1) ? 0.0f : 3.0f;
      f32 wbase[3] = { (s == 1) ? 3.0f : 0.0f, (s == 1) ? 0.0f : 0.0f, 5.0f };
      u32 bt = tensor_alloc(&CPU_BACKEND, bs, DT_FP16);
      CPU_BACKEND.buf_write(TENS[bt].buf_id, b, sizeof b);
      u32 st = tensor_alloc(&CPU_BACKEND, is, DT_FP16);
      CPU_BACKEND.buf_write(TENS[st].buf_id, sv, sizeof sv);
      u32 ct = tensor_alloc(&CPU_BACKEND, bs, DT_FP16);
      CPU_BACKEND.buf_write(TENS[ct].buf_id, gy, sizeof gy);
      Term y = uop_scatter(ten(bt), ten(mk(DT_INT32, &is, idx)), ten(st), 0, k, s);
      CHECK(y != 0);
      u16 gb16[3], gs16[4];
      u32 slot = 0;
      realize_bytes_in(uop_grad_with_target(y, ten(ct), ten(st)), gs16, 4, 2, &slot);
      if (slot != 0) {
        jit_capture_drop(slot);
      }
      realize_bytes_in(uop_grad_with_target(y, ten(ct), ten(bt)), gb16, 3, 2, &slot);
      if (slot != 0) {
        jit_capture_drop(slot);
      }
      f32 gbf[3], gsf[4];
      for (u32 q = 0; q < 3; q++) {
        gbf[q] = fp16_to_f32(gb16[q]);
      }
      for (u32 q = 0; q < 4; q++) {
        gsf[q] = fp16_to_f32(gs16[q]);
      }
      char what[48];
      snprintf(what, sizeof what, "half %s self %u src", kname(k), s);
      check_eq(gsf, wsrc, 4, what);
      snprintf(what, sizeof what, "half %s self %u base", kname(k), s);
      check_eq(gbf, wbase, 3, what);
    }
  }
}

// === 8. a GC between firing the chain rule and realizing it ===
static void test_gc_backward(void) {
  u32   bd[2] = { 5, 3 }, id[2] = { 7, 3 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx[21] = { 1, 4, 0,  1, -1, 0,  5, 2, 2,  3, 4, 0,
                               1, 1, 10982377,  -19238, 2, 4,  0, 0, 3 };
  for (u32 k = SCATTER_MAX; k <= SCATTER_MIN; k++) {
    f32 base[15], src[21], gy[15], wb[15], ws[21], got_b[15], got_s[21];
    for (u32 q = 0; q < 15; q++) {
      base[q] = tie_value(k, q * 7u + 2u);
      gy[q]   = 0.125f * (f32)((i32)((q * 11u) % 13u) - 6) + 0.0625f;
    }
    for (u32 q = 0; q < 21; q++) {
      src[q] = tie_value(k, q * 3u + 1u);
    }
    ref_grad(k, base, &bs, idx, src, &is, 0, 0, gy, wb, ws);
    u32  bt = mk(DT_FP32, &bs, base);
    u32  st = mk(DT_FP32, &is, src);
    u32  ct = mk(DT_FP32, &bs, gy);
    Term y  = uop_scatter(ten(bt), ten(mk(DT_INT32, &is, idx)), ten(st), 0, k, 0);
    Term roots[2];
    roots[0] = wnf(uop_grad_with_target(y, ten(ct), ten(bt)));
    roots[1] = wnf(uop_grad_with_target(y, ten(ct), ten(st)));
    CHECK(gc_enabled());
    gc_collect(roots, 2);
    char what[48];
    realize_into(roots[0], got_b, 15);
    snprintf(what, sizeof what, "after GC %s base", kname(k));
    check_eq(got_b, wb, 15, what);
    realize_into(roots[1], got_s, 21);
    snprintf(what, sizeof what, "after GC %s src", kname(k));
    check_eq(got_s, ws, 21, what);
    CHECK(grad_err_take() == NULL);
  }
}

int main(void) {
  thvm_init();
  TEST_BEGIN("grad-scatter-max/pinned-tinygrad");
  test_pinned();
  TEST_BEGIN("grad-scatter-max/ties");
  test_ties();
  TEST_BEGIN("grad-scatter-max/fd");
  test_fd();
  TEST_BEGIN("grad-scatter-max/computed-src");
  test_computed_src();
  TEST_BEGIN("grad-scatter-max/linear-kernels");
  test_linear_kernels();
  TEST_BEGIN("grad-scatter-max/half");
  test_half();
  TEST_BEGIN("grad-scatter-max/gc-backward");
  test_gc_backward();
  TEST_BEGIN("grad-scatter-max/dispatched");
  CHECK(DISPATCHED >= 100);
  CHECK(grad_err_take() == NULL);
  thvm_free();
  TEST_REPORT();
}
