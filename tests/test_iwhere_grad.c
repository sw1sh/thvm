// test_iwhere_grad.c -- the chain rule through a value-level select.
//
// tinygrad mixin/gradient.py:81: WHERE(c, t, e) sends the cotangent to t
// where c holds and to e elsewhere, and none to c.  Checked against central
// finite differences of
//   f(x) = where(x < 0, x*x, 3x)
//   h(x) = where(x < 0, where(x < -1, -x, 3x), 3x)
// in both chain-rule modes: the requires_grad single walk (target 0, the
// cotangents accumulate into TenDesc.grad), where every pre-walked slot must
// see exactly its predicted arrivals (none stuck, none in excess), and the
// target-aware walk (uop_grad_with_target).
//
// uop_where builds a select without the index-layer folds: h's inner and
// outer selects share an else branch, which uop_iwhere collapses into a
// tensor-level IAND that materialize cannot schedule.
//
// The WL lambda tables see a select's three children: the book snapshot
// owns them, ALO realize wraps each in an ALO, and a binder used only
// inside a branch counts as used.
#include "../src/thvm.c"
#include "test.h"

#define N 8
static f32 const XS[N] = { -2.5f, -1.25f, -0.75f, -0.125f, 0.25f, 0.5f, 1.5f, 3.0f };

static double f_host(double x) {
  return x < 0 ? x * x : 3.0 * x;
}

static double h_host(double x) {
  if (x < 0) {
    return x < -1 ? -x : 3.0 * x;
  }
  return 3.0 * x;
}

static Term fconst(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return uop_const(DT_FP32, b);
}

static Term x_ten(u32 *tid_out) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = N;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)XS, sizeof XS);
  *tid_out = tid;
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static Term ones_n(void) {
  u32 dims[1] = { N };
  return uop_expand(fconst(1.0f), 1, dims);
}

static Term f_graph(Term x) {
  Term c = uop_binary(UOP_CMPLT, x, fconst(0.0f));
  return uop_where(c, uop_binary(UOP_MUL, x, x), uop_binary(UOP_MUL, fconst(3.0f), x));
}

static Term h_graph(Term x) {
  Term three_x = uop_binary(UOP_MUL, fconst(3.0f), x);
  Term c1 = uop_binary(UOP_CMPLT, x, fconst(0.0f));
  Term c2 = uop_binary(UOP_CMPLT, x, fconst(-1.0f));
  return uop_where(c1, uop_where(c2, uop_unary(UOP_NEG, x), three_x), three_x);
}

// 1 iff an IAND/IOR node is reachable from `t`.
static int has_iand(Term t, u32 depth) {
  if (depth > 64 || term_tag(t) != TAG_UOP) {
    return 0;
  }
  u32 op = term_ext(t);
  if (op == UOP_IAND || op == UOP_IOR) {
    return 1;
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    if (has_iand(heap_read(term_val(t) + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int read_f32(Term t, f32 *out) {
  Term r = term_resolve(thvm_realize(t));
  if (term_tag(r) != TAG_TEN) {
    return 0;
  }
  u32 tid = (u32)term_val(r);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, N * sizeof(f32));
  return 1;
}

static void check_fd(f32 const *g, double (*host)(double), char const *what) {
  double const eps = 1e-4;
  for (u32 i = 0; i < N; i++) {
    double x  = (double)XS[i];
    double fd = (host(x + eps) - host(x - eps)) / (2.0 * eps);
    int ok = fabs((double)g[i] - fd) <= 1e-4 * (1.0 + fabs(fd));
    if (!ok) {
      fprintf(stderr, "  %s: d/dx at x=%g got=%.9g fd=%.9g\n", what, x, g[i], fd);
    }
    CHECK(ok);
  }
}

static void test_uop_where_folds(void) {
  u32 tid;
  Term x = x_ten(&tid);
  Term a = uop_binary(UOP_MUL, x, x);
  Term b = uop_unary(UOP_NEG, x);
  CHECK_EQ(uop_where(fconst(1.0f), a, b), a);
  CHECK_EQ(uop_where(fconst(0.0f), a, b), b);
  CHECK_EQ(uop_where(uop_const(DT_INT32, 2), a, b), a);
  Term c = uop_binary(UOP_CMPLT, x, fconst(0.0f));
  CHECK_EQ(uop_where(c, a, a), a);
  Term w = uop_where(c, a, b);
  CHECK_EQ(term_ext(w), UOP_IWHERE);
  CHECK_EQ(uop_where(c, a, b), w);
  // A nested select sharing the else branch stays two IWHEREs.
  Term h = h_graph(x);
  CHECK_EQ(term_ext(h), UOP_IWHERE);
  CHECK_EQ(term_ext(heap_read(term_val(h) + 1)), UOP_IWHERE);
  CHECK(!has_iand(h, 0));
  // uop_iwhere folds the same shape into a tensor-level IAND.
  Term three_x = uop_binary(UOP_MUL, fconst(3.0f), x);
  Term c2 = uop_binary(UOP_CMPLT, x, fconst(-1.0f));
  CHECK(has_iand(uop_iwhere(c, uop_iwhere(c2, b, three_x), three_x), 0));
}

static void test_forward(void) {
  u32 tid;
  Term x = x_ten(&tid);
  f32 got[N];
  int ok = read_f32(h_graph(x), got);
  CHECK(ok);
  for (u32 i = 0; ok && i < N; i++) {
    CHECK(got[i] == (f32)h_host((double)XS[i]));
  }
}

static void grad_single_walk(Term (*build)(Term), double (*host)(double),
                             char const *what) {
  u32  tid;
  Term x  = x_ten(&tid);
  Term gt = 0;
  CHECK(uop_backward(build(x), ones_n(), &tid, 1, &gt, NULL, 0, NULL) == NULL);
  CHECK_EQ(TENS[tid].grad, 0);
  CHECK_EQ(TENS[tid].requires_grad, 0);
  CHECK_EQ(GRAD_REQ_NCOUNT, 0u);
  CHECK_EQ(grad_slot_stuck_get(), 0u);
  CHECK_EQ(grad_slot_excess_get(), 0u);
  CHECK(gt != 0);
  f32 g[N];
  if (gt != 0) {
    int ok = read_f32(gt, g);
    CHECK(ok);
    if (ok) {
      check_fd(g, host, what);
    }
  }
  TENS[tid].grad = 0;
}

static void grad_target(Term (*build)(Term), double (*host)(double),
                        char const *what) {
  u32 tid;
  Term x = x_ten(&tid);
  f32 g[N];
  int ok = read_f32(uop_grad_with_target(build(x), ones_n(), x), g);
  CHECK(ok);
  if (ok) {
    check_fd(g, host, what);
  }
}

static void test_lambda_tables(void) {
  u32  tid;
  Term w = f_graph(x_ten(&tid));
  CHECK_EQ(term_ext(w), UOP_IWHERE);
  Term bw = thvm_book_from_dynamic(w);
  CHECK_EQ(term_ext(bw), UOP_IWHERE);
  CHECK_EQ(term_ext(book_read(term_val(bw) + 0)), UOP_CMPLT);
  CHECK_EQ(term_ext(book_read(term_val(bw) + 1)), UOP_MUL);
  CHECK_EQ(term_ext(book_read(term_val(bw) + 2)), UOP_MUL);
  Term dw = alo_realize(bw, 0);
  CHECK_EQ(term_ext(dw), UOP_IWHERE);
  for (u32 k = 0; k < 3; k++) {
    CHECK_EQ(term_tag(heap_read(term_val(dw) + k)), TAG_ALO);
  }
  Shape vs = {0};
  vs.ndim    = 1;
  vs.dims[0] = N;
  u64 lam = heap_alloc(1);
  heap_set(lam, 0);
  lam_shape_set(lam, &vs);
  Term c = uop_binary(UOP_CMPLT, x_ten(&tid), fconst(0.0f));
  heap_set(lam, uop_where(c, term_new(0, TAG_VAR, 0, lam), fconst(0.0f)));
  CHECK_EQ(term_ext(heap_read(lam)), UOP_IWHERE);
  CHECK(lam_body_uses_var(lam));
}

int main(void) {
  thvm_init();
  TEST_BEGIN("iwhere-grad/uop-where-folds");
  test_uop_where_folds();
  TEST_BEGIN("iwhere-grad/nested-forward");
  test_forward();
  TEST_BEGIN("iwhere-grad/single-walk");
  grad_single_walk(f_graph, f_host, "f single-walk");
  grad_single_walk(h_graph, h_host, "h single-walk");
  TEST_BEGIN("iwhere-grad/target");
  grad_target(f_graph, f_host, "f target");
  grad_target(h_graph, h_host, "h target");
  TEST_BEGIN("iwhere-grad/lambda-tables");
  test_lambda_tables();
  thvm_free();
  TEST_REPORT();
}
