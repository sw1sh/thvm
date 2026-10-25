// test_assign_grad.c -- an assign passes the cotangent to the value it writes
// (tinygrad mixin/gradient.py:98-99), a copy of a buffer included: the chain
// rule's descent must not fire the copy into its dst first, or it would
// differentiate a bare buffer to zero.  d/dx sum(assign(dst, v(x)) * w) is w
// for v(x) = x (a clone) and v(x) = x * ones (a buffer, so the product does
// not fold back to x), in the requires_grad single walk.
#include "../src/thvm.c"
#include "test.h"

#define N 4
static f32 const XS[N] = { 1.0f, 2.0f, 3.0f, 4.0f };
static f32 const WS[N] = { 1.0f, 2.0f, 3.0f, 4.0f };

static Term ten_of(f32 const *v, u32 *tid_out) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = N;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  if (v != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)v, N * sizeof(f32));
  }
  *tid_out = tid;
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static f32 const ONES[N] = { 1.0f, 1.0f, 1.0f, 1.0f };

static Term fconst(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return uop_const(DT_FP32, b);
}

static Term copy_of(Term x) { return x; }
static Term times_ones(Term x) {
  u32 ot;
  return uop_binary(UOP_MUL, x, ten_of(ONES, &ot));
}

static void grad_through_assign(Term (*value)(Term), char const *what) {
  u32  xt, wt, dt;
  Term x   = ten_of(XS, &xt);
  Term w   = ten_of(WS, &wt);
  Term dst = ten_of(NULL, &dt);
  u32  dims[1] = { N };
  Term y   = uop_binary(UOP_MUL, uop_binary(UOP_ASSIGN, dst, value(x)), w);
  Term gt  = 0;
  CHECK(uop_backward(y, uop_expand(fconst(1.0f), 1, dims), &xt, 1, &gt, NULL, 0, NULL) == NULL);
  CHECK_EQ(grad_slot_stuck_get(), 0u);
  CHECK(gt != 0);
  if (gt == 0) {
    return;
  }
  Term r = term_resolve(thvm_realize(gt));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  f32 g[N];
  TENS[term_val(r)].backend->buf_read(TENS[term_val(r)].buf_id, g, sizeof g);
  for (u32 i = 0; i < N; i++) {
    if (g[i] != WS[i]) {
      fprintf(stderr, "  %s: d/dx[%u] got=%g want=%g\n", what, i, g[i], WS[i]);
    }
    CHECK(g[i] == WS[i]);
  }
}

int main(void) {
  thvm_init();
  TEST_BEGIN("assign-grad/copy-passes-to-src");
  grad_through_assign(copy_of, "copy");
  TEST_BEGIN("assign-grad/compute-passes-to-src");
  grad_through_assign(times_ones, "x*ones");
  thvm_free();
  TEST_REPORT();
}
