// test_app_lam.c — APP-LAM beta reduction.
//
// (λx.x) y          → y
// (λx.x) (λy.y)     → (λy.y)
//
// Spec test. Implementation lands in step 6 (wnf stack machine + the
// real interact_app_lam). Until then, the body is gated by PENDING().

#include "../src/thvm.c"
#include "test.h"

// Build (λx.x) — heap[lam_loc] = VAR(lam_loc). Returns the LAM term.
static Term build_id_lam(void) {
  u64  lam_loc = heap_alloc(1);
  Term var     = term_new(0, TAG_VAR, 0, lam_loc);
  heap_set(lam_loc, var);
  return term_new(0, TAG_LAM, 0, lam_loc);
}

// Build APP(f, x) — allocates 2 cells.
static Term build_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
  thvm_init();

  PENDING("APP-LAM — wnf stack machine + interact_app_lam land in step 6");

  // The body below is the spec. It runs once PENDING is removed.

  TEST_BEGIN("app-lam/identity-applied-to-era");
  Term era = term_new(0, TAG_ERA, 0, 0);
  Term id1 = build_id_lam();
  Term app = build_app(id1, era);
  Term out = wnf(app);
  CHECK_EQ(term_tag(out), TAG_ERA);

  TEST_BEGIN("app-lam/identity-applied-to-identity");
  Term id2 = build_id_lam();
  Term id3 = build_id_lam();
  Term app2 = build_app(id2, id3);
  Term out2 = wnf(app2);
  CHECK_EQ(term_tag(out2), TAG_LAM);

  thvm_free();
  TEST_REPORT();
}
