// test_grad_bound_target.c -- the gradient with respect to a lambda-bound
// target past a recursive loop's first step.  sgd.wlt's loop applies
// \w. w - lr * d/dw sum((w - t)^2) to its own result, so from the second step
// on w is a graph, and auto-dup hands the update, the loss and the target three
// projections of one DUP over it.  The rule sees y unreduced (tinygrad's
// compute_gradient never evaluates the graph), so the loss's w must match the
// target before that DUP fires: both factors of (w - t)^2 count, or every step
// past the first is half its size.
#include "../src/thvm.c"
#include "test.h"

#define N 4
static f32 const TS[N] = { 1.0f, 2.0f, 3.0f, 4.0f };
static f32 const W0[N] = { 0.0f, 0.0f, 0.0f, 0.0f };

static Term ten_of(f32 const *v) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = N;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)v, N * sizeof(f32));
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static Term fconst(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return uop_const(DT_FP32, b);
}

static Term app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

// \x. x, so the step lambda's body is an APP: a UOP body gets no DUP chain
// (lam_seal_ext_with_auto_dup), and WL's \w. \n. ... gets one.
static Term id_lam(void) {
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_VAR, 0, loc));
  return term_new(0, TAG_LAM, lam_seal_ext_with_auto_dup(loc, 0), loc);
}

// \w. id (w - lr * grad(d * d, w)), d = w - t, seeded with ones: one SGD step,
// w' = 0.8 w + 0.2 t at lr = 0.1.
static Term step_lam(Term t) {
  u64  loc     = heap_alloc(1);
  Term w       = term_new(0, TAG_VAR, 0, loc);
  u32  dims[1] = { N };
  Term d       = uop_binary(UOP_ADD, w, uop_unary(UOP_NEG, t));
  Term g       = uop_grad_with_target(uop_binary(UOP_MUL, d, d),
                                      uop_expand(fconst(1.0f), 1, dims), w);
  Term step    = uop_binary(UOP_ADD, w,
                            uop_unary(UOP_NEG, uop_binary(UOP_MUL, fconst(0.1f), g)));
  heap_set(loc, app(id_lam(), step));
  return term_new(0, TAG_LAM, lam_seal_ext_with_auto_dup(loc, 0), loc);
}

// n steps from w = 0 give (1 - 0.8^n) t.
static void check_steps(u32 n, f32 scale) {
  Term t = ten_of(TS);
  Term w = ten_of(W0);
  for (u32 i = 0; i < n; i++) {
    w = app(step_lam(t), w);
  }
  Term r = term_resolve(thvm_realize(w));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  f32 got[N];
  TENS[term_val(r)].backend->buf_read(TENS[term_val(r)].buf_id, got, sizeof got);
  for (u32 i = 0; i < N; i++) {
    f32 want = scale * TS[i];
    if (fabsf(got[i] - want) > 1e-5f) {
      fprintf(stderr, "  %u steps: w[%u] got=%g want=%g\n", n, i, got[i], want);
    }
    CHECK(fabsf(got[i] - want) <= 1e-5f);
  }
}

int main(void) {
  thvm_init();
  TEST_BEGIN("grad-bound-target/one-step");
  check_steps(1, 0.2f);
  TEST_BEGIN("grad-bound-target/two-steps");
  check_steps(2, 0.36f);
  TEST_BEGIN("grad-bound-target/three-steps");
  check_steps(3, 0.488f);
  thvm_free();
  TEST_REPORT();
}
