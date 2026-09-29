// test_grad_gather_scatter.c -- the chain rule through UOP_GATHER and
// UOP_SCATTER(SUM).
//
// tinygrad differentiates gather and scatter_reduce through their one-hot
// formulations (mixin/op.py:1023-1043 and :1126-1129 through
// mixin/gradient.py); the native rules give the same values:
//   GATHER(src, idx)                g_src  = gy summed into zeros(src.shape)
//                                            at the positions idx reads
//   SCATTER(SUM, base, idx, src, s) g_src  = gather(gy, idx)
//                                   g_base = gy when s, else gy with every
//                                            position idx writes zeroed
// and idx gets no gradient.  Each case differentiates y with a cotangent
// seed C (a vector-Jacobian product), so the gradients are exact sums the
// host loops reproduce bit for bit:
//
//   1. gather gradients vs an np.add.at-style host loop: duplicate ids,
//      invalid ids {-1, V, 10982377, -19238}, rows no id selects, dims 0/1/2
//      with an index smaller than src off-axis, a scalar cotangent;
//   2. scatter-sum gradients for include_self 0 and 1, dims 0 and 1;
//   3. central finite differences (host f64) of quadratic losses;
//   4. every case runs in the requires_grad single walk (no stuck or excess
//      slots), the target-aware walk and the legacy SUP/DUP projection;
//   5. second order: the gradient of the gather adjoint (a scatter-add)
//      with respect to its cotangent is a gather;
//   6. the backward kernels hold no REDUCE and loop at most max(|src|,
//      |idx|) times per store;
//   7. JIT capture of the gather gradient, then replays with the index
//      rewritten in place;
//   8. differentiating a PROD / MAX / MIN / REPLACE scatter raises the grad
//      error (grad_err_take) instead of leaving the cell unfired.
//
// Run twice, as test_uop_gather: as is (C JIT) and with
// THVM_CPU_JIT_WARMUP=1000000000 THVM_CPU_JIT_PARALLEL=0
// THVM_CPU_JIT_OPT=O0 (the UOp walker).
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

// idx (n, d) that repeats ids (n) along axis 1: the embedding index.
static Term rows(Term ids, u32 n, u32 d) {
  u32 n1[2] = { n, 1 };
  u32 nd[2] = { n, d };
  return uop_expand(uop_reshape(ids, 2, n1), 2, nd);
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

// The position of `out` (shape os) that index position p writes or reads,
// or -1 when idx[p] is out of range.
static i32 target_of(u32 p, Shape const *is, i32 const *idx, u32 dim,
                     Shape const *os) {
  u32 c[MAX_DIM];
  unravel(p, is, c);
  if (idx[p] < 0 || (u32)idx[p] >= os->dims[dim]) {
    return -1;
  }
  c[dim] = (u32)idx[p];
  return (i32)ravel(c, os);
}

// out (shape os) = zeros, then out[t(p)] += v[p] in row-major p order: the
// gather gradient, accumulated in the order the scatter kernel runs.
static void ref_scatter_add(f32 const *v, Shape const *is, i32 const *idx,
                            u32 dim, Shape const *os, f32 *out) {
  memset(out, 0, (size_t)numel_of(os) * 4);
  for (u32 p = 0; p < numel_of(is); p++) {
    i32 t = target_of(p, is, idx, dim, os);
    if (t >= 0) {
      out[t] += v[p];
    }
  }
}

// out[p] = v[t(p)], or 0 for an invalid index: the scatter-sum gradient of
// src.
static void ref_gather(f32 const *v, Shape const *vs, i32 const *idx,
                       Shape const *is, u32 dim, f32 *out) {
  for (u32 p = 0; p < numel_of(is); p++) {
    i32 t = target_of(p, is, idx, dim, vs);
    out[p] = t >= 0 ? v[t] : 0.0f;
  }
}

// The scatter-sum gradient of base: gy, zeroed where idx writes unless
// include_self.
static void ref_base_grad(f32 const *gy, Shape const *bs, i32 const *idx,
                          Shape const *is, u32 dim, u32 include_self, f32 *out) {
  memcpy(out, gy, (size_t)numel_of(bs) * 4);
  if (include_self) {
    return;
  }
  for (u32 p = 0; p < numel_of(is); p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t >= 0) {
      out[t] = 0.0f;
    }
  }
}

static void fill(f32 *v, u32 n, u32 seed) {
  for (u32 q = 0; q < n; q++) {
    v[q] = 0.125f * (f32)((i32)((q * 7u + seed * 13u) % 29u) - 14);
  }
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

// Every kernel emitted since k0 that ran did so on the backend this run
// asked for.  DISPATCHED counts them, so a run that dispatched nothing
// cannot pass.
static u32 DISPATCHED = 0;

static void check_dispatch(u32 k0) {
  u32 want = interp_mode() ? KDISPATCH_INTERPRETER : KDISPATCH_JIT;
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    u32 kind = cg_kernel_dispatch_kind(k);
    if (kind == KDISPATCH_NONE
        || (kind >= KDISPATCH_BLAS_DOT && kind <= KDISPATCH_BLAS_GEMM)) {
      continue;
    }
    CHECK_EQ(kind, want);
    DISPATCHED++;
  }
}

// Realize `t` into `out` (n f32 words).  Returns the result tid, 0 when the
// realize produced no dense tensor of n elements.
static u32 realize_into(Term t, f32 *out, u32 n) {
  u32 k0 = KERNELS_NEXT;
  Term r = term_resolve(thvm_realize(t));
  int ok = term_tag(r) == TAG_TEN;
  CHECK(ok);
  u32 tid = ok ? (u32)term_val(r) : 0;
  if (ok) {
    View const *v = &TENS[tid].view;
    ok = v->numel == n && v->contiguous && v->offset == 0 && TENS[tid].nviews == 0;
    CHECK(ok);
  }
  if (!ok) {
    memset(out, 0, (size_t)n * 4);
    return 0;
  }
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, (u64)n * 4);
  check_dispatch(k0);
  return tid;
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
  return ok;
}

// === 1. gather gradients ===
#define EV 6
#define ED 4
#define EN 8

// y = gather(src, idx, dim) differentiated at a seed of idx's shape; the
// src gradient in every mode equals the host scatter-add of the seed.
static void gather_grad_case(Shape const *ss, Shape const *is, u32 dim,
                             i32 const *idx_data, Term idx, char const *what) {
  u32 sn = numel_of(ss), in = numel_of(is);
  f32 *src  = (f32 *)malloc((size_t)sn * 4);
  f32 *seed = (f32 *)malloc((size_t)in * 4);
  f32 *want = (f32 *)malloc((size_t)sn * 4);
  f32 *got  = (f32 *)malloc((size_t)sn * 4);
  fill(src, sn, 1);
  fill(seed, in, 2);
  ref_scatter_add(seed, is, idx_data, dim, ss, want);
  u32 st = mk(DT_FP32, ss, src);
  u32 ct = mk(DT_FP32, is, seed);
  if (idx == 0) {
    idx = ten(mk(DT_INT32, is, idx_data));
  }
  Term y = uop_gather(ten(st), idx, dim);
  CHECK(y != 0);
  for (u32 m = 0; m < MODE_COUNT; m++) {
    char name[96];
    snprintf(name, sizeof name, "%s (%s)", what, MODE_NAME[m]);
    f32 *outs[1] = { got };
    int ok = vjp(y, ten(ct), m, 1, &st, outs);
    CHECK(ok);
    if (ok) {
      check_eq(got, want, sn, name);
    }
  }
  free(src);
  free(seed);
  free(want);
  free(got);
}

static void test_gather_grad(void) {
  // Embedding rows: ids 2 and 0 twice, four invalid ids, rows 1 and 3..5
  // never selected.
  static i32 const ids[EN] = { 2, -1, 0, EV, 2, 10982377, 0, -19238 };
  u32 wd[2] = { EV, ED }, nd[1] = { EN }, gd[2] = { EN, ED };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  i32 grid[EN * ED];
  for (u32 p = 0; p < EN * ED; p++) {
    grid[p] = ids[p / ED];
  }
  gather_grad_case(&ws, &gs, 0, grid, rows(ten(mk(DT_INT32, &ns, ids)), EN, ED),
                   "embedding rows");
  // dims 0 / 1 / 2 with idx (3, 4, 5) smaller than src (4, 5, 6) off-axis.
  u32 sd[3] = { 4, 5, 6 }, id[3] = { 3, 4, 5 };
  Shape ss = shape_of(3, sd), is = shape_of(3, id);
  i32 idx[60];
  for (u32 dim = 0; dim < 3; dim++) {
    for (u32 p = 0; p < 60; p++) {
      idx[p] = (i32)((p * 7u + dim * 3u) % (ss.dims[dim] + 3)) - 1;
    }
    char what[32];
    snprintf(what, sizeof what, "gather dim %u", dim);
    gather_grad_case(&ss, &is, dim, idx, 0, what);
  }
}

// A numel-1 cotangent broadcasts to the gather's shape: the src gradient
// counts how often each element is read.
static void test_gather_scalar_seed(void) {
  static i32 const ids[EN] = { 2, -1, 0, EV, 2, 3, 0, 2 };
  u32 wd[2] = { EV, ED }, nd[1] = { EN }, gd[2] = { EN, ED };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  f32 w[EV * ED], ones[EN * ED], want[EV * ED], got[EV * ED];
  fill(w, EV * ED, 3);
  i32 grid[EN * ED];
  for (u32 p = 0; p < EN * ED; p++) {
    grid[p] = ids[p / ED];
    ones[p] = 1.0f;
  }
  ref_scatter_add(ones, &gs, grid, 0, &ws, want);
  u32 wt = mk(DT_FP32, &ws, w);
  Term y = uop_gather(ten(wt), rows(ten(mk(DT_INT32, &ns, ids)), EN, ED), 0);
  f32 one = 1.0f;
  u32 bits;
  memcpy(&bits, &one, sizeof bits);
  Term seed = uop_const(DT_FP32, bits);
  realize_into(uop_grad_with_target(y, seed, ten(wt)), got, EV * ED);
  check_eq(got, want, EV * ED, "scalar cotangent");
}

// === 2. scatter-sum gradients ===
static void scatter_grad_case(Shape const *bs, Shape const *is, u32 dim,
                              i32 const *idx_data, u32 include_self,
                              char const *what) {
  u32 bn = numel_of(bs), in = numel_of(is);
  f32 *base = (f32 *)malloc((size_t)bn * 4);
  f32 *src  = (f32 *)malloc((size_t)in * 4);
  f32 *seed = (f32 *)malloc((size_t)bn * 4);
  f32 *wb   = (f32 *)malloc((size_t)bn * 4);
  f32 *ws   = (f32 *)malloc((size_t)in * 4);
  f32 *gb   = (f32 *)malloc((size_t)bn * 4);
  f32 *gsr  = (f32 *)malloc((size_t)in * 4);
  fill(base, bn, 4);
  fill(src, in, 5);
  fill(seed, bn, 6);
  ref_base_grad(seed, bs, idx_data, is, dim, include_self, wb);
  ref_gather(seed, bs, idx_data, is, dim, ws);
  u32 bt = mk(DT_FP32, bs, base);
  u32 st = mk(DT_FP32, is, src);
  u32 ct = mk(DT_FP32, bs, seed);
  Term y = uop_scatter(ten(bt), ten(mk(DT_INT32, is, idx_data)), ten(st), dim,
                       SCATTER_SUM, include_self);
  CHECK(y != 0);
  u32 tids[2] = { bt, st };
  for (u32 m = 0; m < MODE_COUNT; m++) {
    char nb[96], ns[96];
    snprintf(nb, sizeof nb, "%s base (%s)", what, MODE_NAME[m]);
    snprintf(ns, sizeof ns, "%s src (%s)", what, MODE_NAME[m]);
    f32 *outs[2] = { gb, gsr };
    int ok = vjp(y, ten(ct), m, 2, tids, outs);
    CHECK(ok);
    if (ok) {
      check_eq(gb, wb, bn, nb);
      check_eq(gsr, ws, in, ns);
    }
  }
  free(base);
  free(src);
  free(seed);
  free(wb);
  free(ws);
  free(gb);
  free(gsr);
}

static void test_scatter_grad(void) {
  // dim 0: 7 rows into 5, duplicates and invalid ids.
  u32 bd[2] = { 5, 3 }, id[2] = { 7, 3 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx0[21] = { 1, 4, 0,  1, -1, 0,  5, 2, 2,  3, 4, 0,
                                1, 1, 10982377,  -19238, 2, 4,  0, 0, 3 };
  // dim 1: idx (3, 5) smaller than base (4, 6) off-axis.
  u32 bd1[2] = { 4, 6 }, id1[2] = { 3, 5 };
  Shape bs1 = shape_of(2, bd1), is1 = shape_of(2, id1);
  i32 idx1[15];
  for (u32 p = 0; p < 15; p++) {
    idx1[p] = (i32)((p * 5u) % 9u) - 1;
  }
  for (u32 s = 0; s < 2; s++) {
    char w0[48], w1[48];
    snprintf(w0, sizeof w0, "scatter dim 0 include_self %u", s);
    snprintf(w1, sizeof w1, "scatter dim 1 include_self %u", s);
    scatter_grad_case(&bs, &is, 0, idx0, s, w0);
    scatter_grad_case(&bs1, &is1, 1, idx1, s, w1);
  }
}

// === 3. central finite differences (host f64) ===
static void f64_gather(double const *src, Shape const *ss, i32 const *idx,
                       Shape const *is, u32 dim, double *out) {
  for (u32 p = 0; p < numel_of(is); p++) {
    i32 t = target_of(p, is, idx, dim, ss);
    out[p] = t >= 0 ? src[t] : 0.0;
  }
}

static void f64_scatter_sum(double const *base, Shape const *bs, i32 const *idx,
                            Shape const *is, double const *src, u32 dim,
                            u32 include_self, double *out) {
  u32 bn = numel_of(bs);
  u8 *touched = (u8 *)calloc(bn, 1);
  double *acc = (double *)calloc(bn, sizeof(double));
  for (u32 p = 0; p < numel_of(is); p++) {
    i32 t = target_of(p, is, idx, dim, bs);
    if (t >= 0) {
      acc[t] += src[p];
      touched[t] = 1;
    }
  }
  for (u32 j = 0; j < bn; j++) {
    out[j] = acc[j] + ((include_self || !touched[j]) ? base[j] : 0.0);
  }
  free(touched);
  free(acc);
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

static void test_fd_gather(void) {
  static i32 const ids[EN] = { 2, -1, 0, EV, 2, 5, 0, 1 };
  u32 wd[2] = { EV, ED }, nd[1] = { EN }, gd[2] = { EN, ED };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  i32 grid[EN * ED];
  f32 w[EV * ED], c[EN * ED], g[EV * ED];
  double wx[EV * ED], y[EN * ED], fd[EV * ED];
  fill(w, EV * ED, 7);
  fill(c, EN * ED, 8);
  for (u32 p = 0; p < EN * ED; p++) {
    grid[p] = ids[p / ED];
  }
  double const eps = 1e-3;
  for (u32 k = 0; k < EV * ED; k++) {
    for (u32 q = 0; q < EV * ED; q++) {
      wx[q] = (double)w[q];
    }
    wx[k] += eps;
    f64_gather(wx, &ws, grid, &gs, 0, y);
    double hi = quad(y, c, EN * ED);
    wx[k] -= 2.0 * eps;
    f64_gather(wx, &ws, grid, &gs, 0, y);
    double lo = quad(y, c, EN * ED);
    fd[k] = (hi - lo) / (2.0 * eps);
  }
  u32 wt = mk(DT_FP32, &ws, w);
  Term gth = uop_gather(ten(wt), rows(ten(mk(DT_INT32, &ns, ids)), EN, ED), 0);
  Term y2 = uop_binary(UOP_MUL, gth, gth);
  u32 ct = mk(DT_FP32, &gs, c);
  for (u32 m = 0; m < MODE_COUNT; m++) {
    char name[64];
    snprintf(name, sizeof name, "fd gather (%s)", MODE_NAME[m]);
    f32 *outs[1] = { g };
    if (vjp(y2, ten(ct), m, 1, &wt, outs)) {
      check_fd(g, fd, EV * ED, name);
    }
  }
}

static void test_fd_scatter(void) {
  u32 bd[2] = { 5, 3 }, id[2] = { 6, 3 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx[18] = { 1, 4, 0,  1, -1, 0,  5, 2, 2,
                               3, 4, 0,  1, 1, 7,  -3, 2, 4 };
  f32 base[15], src[18], c[15], gb[15], gs[18];
  fill(base, 15, 9);
  fill(src, 18, 10);
  fill(c, 15, 11);
  double const eps = 1e-3;
  for (u32 s = 0; s < 2; s++) {
    double bx[15], sx[18], y[15], fdb[15], fds[18];
    for (u32 k = 0; k < 15 + 18; k++) {
      for (u32 q = 0; q < 15; q++) {
        bx[q] = (double)base[q];
      }
      for (u32 q = 0; q < 18; q++) {
        sx[q] = (double)src[q];
      }
      double *x = k < 15 ? &bx[k] : &sx[k - 15];
      *x += eps;
      f64_scatter_sum(bx, &bs, idx, &is, sx, 0, s, y);
      double hi = quad(y, c, 15);
      *x -= 2.0 * eps;
      f64_scatter_sum(bx, &bs, idx, &is, sx, 0, s, y);
      double lo = quad(y, c, 15);
      double d = (hi - lo) / (2.0 * eps);
      if (k < 15) {
        fdb[k] = d;
      } else {
        fds[k - 15] = d;
      }
    }
    u32 bt = mk(DT_FP32, &bs, base);
    u32 st = mk(DT_FP32, &is, src);
    Term sc = uop_scatter(ten(bt), ten(mk(DT_INT32, &is, idx)), ten(st), 0,
                          SCATTER_SUM, s);
    Term y2 = uop_binary(UOP_MUL, sc, sc);
    u32 ct = mk(DT_FP32, &bs, c);
    u32 tids[2] = { bt, st };
    for (u32 m = 0; m < MODE_COUNT; m++) {
      char nb[64], ns[64];
      snprintf(nb, sizeof nb, "fd scatter base s=%u (%s)", s, MODE_NAME[m]);
      snprintf(ns, sizeof ns, "fd scatter src s=%u (%s)", s, MODE_NAME[m]);
      f32 *outs[2] = { gb, gs };
      if (vjp(y2, ten(ct), m, 2, tids, outs)) {
        check_fd(gb, fdb, 15, nb);
        check_fd(gs, fds, 18, ns);
      }
    }
  }
}

// === 5. second order ===
// g1 = the gather gradient of W at seed U (a scatter-add of U); its
// gradient with respect to U at seed Q is gather(Q, idx).
static void test_second_order(void) {
  static i32 const ids[EN] = { 2, -1, 0, EV, 2, 10982377, 0, 4 };
  u32 wd[2] = { EV, ED }, nd[1] = { EN }, gd[2] = { EN, ED };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  f32 w[EV * ED], u[EN * ED], q[EV * ED], want[EN * ED], got[EN * ED];
  fill(w, EV * ED, 12);
  fill(u, EN * ED, 13);
  fill(q, EV * ED, 14);
  i32 grid[EN * ED];
  for (u32 p = 0; p < EN * ED; p++) {
    grid[p] = ids[p / ED];
  }
  ref_gather(q, &ws, grid, &gs, 0, want);
  u32 wt = mk(DT_FP32, &ws, w);
  u32 ut = mk(DT_FP32, &gs, u);
  u32 qt = mk(DT_FP32, &ws, q);
  Term y = uop_gather(ten(wt), rows(ten(mk(DT_INT32, &ns, ids)), EN, ED), 0);
  // Target-aware.
  Term g1 = uop_grad_with_target(y, ten(ut), ten(wt));
  realize_into(uop_grad_with_target(g1, ten(qt), ten(ut)), got, EN * ED);
  check_eq(got, want, EN * ED, "second order (target)");
  // Single walk: W's lazy gradient, then that term differentiated for U.
  TENS[wt].requires_grad = 1;
  TENS[wt].grad = 0;
  grad_req_ncount_inc();
  wnf(uop_grad(y, ten(ut)));
  Term gw = TENS[wt].grad;
  TENS[wt].grad = 0;
  TENS[wt].requires_grad = 0;
  grad_req_ncount_dec();
  CHECK(gw != 0);
  if (gw == 0) {
    return;
  }
  TENS[ut].requires_grad = 1;
  TENS[ut].grad = 0;
  grad_req_ncount_inc();
  wnf(uop_grad(gw, ten(qt)));
  CHECK_EQ(grad_slot_stuck_get(), 0u);
  CHECK_EQ(grad_slot_excess_get(), 0u);
  Term gu = TENS[ut].grad;
  TENS[ut].grad = 0;
  TENS[ut].requires_grad = 0;
  grad_req_ncount_dec();
  CHECK(gu != 0);
  if (gu != 0) {
    realize_into(gu, got, EN * ED);
    check_eq(got, want, EN * ED, "second order (single-walk)");
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

static void test_linear_kernels(void) {
  enum { V = 4096, D = 16, N = 64 };
  u32 wd[2] = { V, D }, nd[1] = { N }, gd[2] = { N, D };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  f32 *w = (f32 *)malloc((size_t)V * D * 4);
  f32 *c = (f32 *)malloc((size_t)N * D * 4);
  f32 *want = (f32 *)malloc((size_t)V * D * 4);
  f32 *got = (f32 *)malloc((size_t)V * D * 4);
  i32 ids[N], grid[N * D];
  fill(w, V * D, 15);
  fill(c, N * D, 16);
  for (u32 r = 0; r < N; r++) {
    ids[r] = (i32)((r * 2654435761u) % (V + 8)) - 4;
  }
  for (u32 p = 0; p < N * D; p++) {
    grid[p] = ids[p / D];
  }
  // Gather gradient: a scatter-add over (V, D).
  u32 wt = mk(DT_FP32, &ws, w);
  Term idx = rows(ten(mk(DT_INT32, &ns, ids)), N, D);
  Term y = uop_gather(ten(wt), idx, 0);
  u32 ct = mk(DT_FP32, &gs, c);
  ref_scatter_add(c, &gs, grid, 0, &ws, want);
  u32 k0 = KERNELS_NEXT;
  realize_into(uop_grad_with_target(y, ten(ct), ten(wt)), got, V * D);
  check_eq(got, want, V * D, "gather grad V=4096");
  CHECK(check_linear_kernels(k0, (u64)V * D, "gather grad") >= 1);
  // Scatter-sum gradients: src is a gather over (N, D), base a REPLACE
  // over (V, D).
  u32 bt = mk(DT_FP32, &ws, w);
  u32 st = mk(DT_FP32, &gs, c);
  u32 qt = mk(DT_FP32, &ws, w);
  Term sc = uop_scatter(ten(bt), idx, ten(st), 0, SCATTER_SUM, 0);
  f32 *gsrc = (f32 *)malloc((size_t)N * D * 4);
  f32 *wsrc = (f32 *)malloc((size_t)N * D * 4);
  ref_gather(w, &ws, grid, &gs, 0, wsrc);
  k0 = KERNELS_NEXT;
  realize_into(uop_grad_with_target(sc, ten(qt), ten(st)), gsrc, N * D);
  check_eq(gsrc, wsrc, N * D, "scatter src grad V=4096");
  CHECK(check_linear_kernels(k0, (u64)N * D, "scatter src grad") >= 1);
  ref_base_grad(w, &ws, grid, &gs, 0, 0, want);
  k0 = KERNELS_NEXT;
  realize_into(uop_grad_with_target(sc, ten(qt), ten(bt)), got, V * D);
  check_eq(got, want, V * D, "scatter base grad V=4096");
  CHECK(check_linear_kernels(k0, (u64)V * D, "scatter base grad") >= 1);
  free(w);
  free(c);
  free(want);
  free(got);
  free(gsrc);
  free(wsrc);
}

// === 7. JIT capture of the gather gradient, replays with fresh ids ===
static void test_jit_replay(void) {
  static i32 const ids[4][EN] = {
    { 2, -1, 0, 4, 3, 5, 1, 2 },
    { 5, 5, 5, 5, 0, 0, 0, 0 },
    { -19238, 3, 10982377, 2, 1, EV, 4, 0 },
    { 1, 2, 3, 4, 5, 0, -1, 2 },
  };
  u32 wd[2] = { EV, ED }, nd[1] = { EN }, gd[2] = { EN, ED };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd), gs = shape_of(2, gd);
  f32 w[EV * ED], c[EN * ED], want[EV * ED], got[EV * ED];
  fill(w, EV * ED, 17);
  fill(c, EN * ED, 18);
  u32 wt = mk(DT_FP32, &ws, w);
  u32 it = mk(DT_INT32, &ns, ids[0]);
  u32 ct = mk(DT_FP32, &gs, c);
  Term y = uop_gather(ten(wt), rows(ten(it), EN, ED), 0);
  Term g = uop_grad_with_target(y, ten(ct), ten(wt));
  u32 slot = jit_capture_begin();
  CHECK(slot != 0);
  Term r = term_resolve(thvm_realize(g));
  jit_capture_end_with_result(r);
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  u32 tid = (u32)term_val(r);
  i32 grid[EN * ED];
  for (u32 rep = 0; rep < 4; rep++) {
    if (rep > 0) {
      CPU_BACKEND.buf_write(TENS[it].buf_id, (void *)ids[rep], EN * 4);
      f32 poison[EV * ED];
      for (u32 q = 0; q < EV * ED; q++) {
        poison[q] = -999.0f;
      }
      TENS[tid].backend->buf_write(TENS[tid].buf_id, poison, EV * ED * 4);
      jit_replay(slot);
    }
    TENS[tid].backend->buf_read(TENS[tid].buf_id, got, EV * ED * 4);
    for (u32 p = 0; p < EN * ED; p++) {
      grid[p] = ids[rep][p / ED];
    }
    ref_scatter_add(c, &gs, grid, 0, &ws, want);
    char what[32];
    snprintf(what, sizeof what, "jit replay %u", rep);
    check_eq(got, want, EV * ED, what);
  }
  jit_capture_drop(slot);
}

// === 8. kinds without a gradient rule raise the grad error ===
static void test_grad_err(void) {
  CHECK(grad_err_take() == NULL);
  u32 bd[2] = { 5, 3 }, id[2] = { 4, 3 };
  Shape bs = shape_of(2, bd), is = shape_of(2, id);
  static i32 const idx[12] = { 1, 4, 0,  1, 2, 0,  3, 2, 2,  0, 4, 1 };
  f32 base[15], src[12], c[15];
  fill(base, 15, 19);
  fill(src, 12, 20);
  fill(c, 15, 21);
  u32 kinds[4] = { SCATTER_PROD, SCATTER_MAX, SCATTER_MIN, SCATTER_REPLACE };
  for (u32 k = 0; k < 4; k++) {
    u32 bt = mk(DT_FP32, &bs, base);
    u32 st = mk(DT_FP32, &is, src);
    u32 ct = mk(DT_FP32, &bs, c);
    Term y = uop_scatter(ten(bt), ten(mk(DT_INT32, &is, idx)), ten(st), 0, kinds[k], 1);
    CHECK(y != 0);
    // Single walk.
    TENS[bt].requires_grad = 1;
    TENS[st].requires_grad = 1;
    grad_req_ncount_inc();
    grad_req_ncount_inc();
    wnf(uop_grad(y, ten(ct)));
    char const *e = grad_err_take();
    CHECK(e != NULL && strstr(e, "scatter") != NULL);
    CHECK(grad_err_take() == NULL);
    TENS[bt].grad = 0;
    TENS[st].grad = 0;
    TENS[bt].requires_grad = 0;
    TENS[st].requires_grad = 0;
    grad_req_ncount_dec();
    grad_req_ncount_dec();
    // Target-aware.
    wnf(uop_grad_with_target(y, ten(ct), ten(st)));
    e = grad_err_take();
    CHECK(e != NULL && strstr(e, "scatter") != NULL);
    CHECK(grad_err_take() == NULL);
  }
}

// === 9. a half gather accumulates its gradient in float32 ===
// tinygrad sums the one-hot's broadcast axis in sum_acc_dtype (float32 for
// half) and casts back (mixin/gradient.py:133): the adjoint is a float32
// scatter-add cast to half.  Three cotangents 2048, 1, 1 on one row sum to
// 2050 in float32 but stay 2048 in half (its spacing there is 2).
static void test_half_accumulates_in_f32(void) {
  u32 wd[2] = { 4, 1 }, nd[1] = { 3 }, gd[2] = { 3, 1 };
  Shape ws = shape_of(2, wd), ns = shape_of(1, nd);
  u16 w[4] = { 0, 0, 0, 0 };
  u16 c[3] = { f32_to_fp16(2048.0f), f32_to_fp16(1.0f), f32_to_fp16(1.0f) };
  static i32 const ids[3] = { 2, 2, 2 };
  u32 wt = tensor_alloc(&CPU_BACKEND, ws, DT_FP16);
  CPU_BACKEND.buf_write(TENS[wt].buf_id, w, sizeof w);
  Shape gs = shape_of(2, gd);
  u32 ct = tensor_alloc(&CPU_BACKEND, gs, DT_FP16);
  CPU_BACKEND.buf_write(TENS[ct].buf_id, c, sizeof c);
  Term y = uop_gather(ten(wt), rows(ten(mk(DT_INT32, &ns, ids)), 3, 1), 0);
  CHECK(y != 0);
  Term g = term_resolve(wnf(uop_grad_with_target(y, ten(ct), ten(wt))));
  u32 gdt = 0, sdt = 0;
  CHECK(term_tag(g) == TAG_UOP && term_ext(g) == UOP_CAST);
  CHECK(term_dtype_in(g, 0, &gdt) && gdt == DT_FP16);
  Term s = term_resolve(heap_read(term_val(g)));
  CHECK(term_tag(s) == TAG_UOP && term_ext(s) == UOP_SCATTER);
  CHECK(term_dtype_in(s, 0, &sdt) && sdt == DT_FP32);
  Term r = term_resolve(thvm_realize(g));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  u32 tid = (u32)term_val(r);
  u16 got[4] = { 0 };
  CHECK_EQ(TENS[tid].dtype, DT_FP16);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, sizeof got);
  CHECK(fp16_to_f32(got[0]) == 0.0f && fp16_to_f32(got[1]) == 0.0f
        && fp16_to_f32(got[3]) == 0.0f);
  if (fp16_to_f32(got[2]) != 2050.0f) {
    fprintf(stderr, "  half gather grad: got=%g want=2050\n", fp16_to_f32(got[2]));
  }
  CHECK(fp16_to_f32(got[2]) == 2050.0f);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("grad-gather-scatter/gather");
  test_gather_grad();
  TEST_BEGIN("grad-gather-scatter/gather-scalar-seed");
  test_gather_scalar_seed();
  TEST_BEGIN("grad-gather-scatter/scatter-sum");
  test_scatter_grad();
  TEST_BEGIN("grad-gather-scatter/fd-gather");
  test_fd_gather();
  TEST_BEGIN("grad-gather-scatter/fd-scatter");
  test_fd_scatter();
  TEST_BEGIN("grad-gather-scatter/second-order");
  test_second_order();
  TEST_BEGIN("grad-gather-scatter/linear-kernels");
  test_linear_kernels();
  TEST_BEGIN("grad-gather-scatter/jit-replay");
  test_jit_replay();
  TEST_BEGIN("grad-gather-scatter/grad-err");
  test_grad_err();
  TEST_BEGIN("grad-gather-scatter/half-accumulates-in-f32");
  test_half_accumulates_in_f32();
  TEST_BEGIN("grad-gather-scatter/dispatched");
  CHECK(DISPATCHED >= 20);
  CHECK(grad_err_take() == NULL);
  thvm_free();
  TEST_REPORT();
}
