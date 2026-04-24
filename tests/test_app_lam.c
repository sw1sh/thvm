// test_app_lam.c - APP-LAM beta reduction.
//
// (lam x.x) y          -> y
// (lam x.x) (lam y.y)  -> (lam y.y)

#include "../src/thvm.c"
#include "test.h"

// Build (λx.x) - heap[lam_loc] = VAR(lam_loc). Returns the LAM term.
static Term build_id_lam(void) {
  u64  lam_loc = heap_alloc(1);
  Term var     = term_new(0, TAG_VAR, 0, lam_loc);
  heap_set(lam_loc, var);
  return term_new(0, TAG_LAM, 0, lam_loc);
}

// Build APP(f, x) - allocates 2 cells.
static Term build_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("app-lam/identity-applied-to-era");
  Term era  = term_new(0, TAG_ERA, 0, 0);
  Term id1  = build_id_lam();
  Term app1 = build_app(id1, era);
  u64  itrs_before = ITRS;
  Term out1 = wnf(app1);
  CHECK_EQ(term_tag(out1), TAG_ERA);
  CHECK_EQ(ITRS - itrs_before, 1);  // exactly one APP-LAM fired

  TEST_BEGIN("app-lam/identity-applied-to-identity");
  Term id2  = build_id_lam();
  Term id3  = build_id_lam();
  Term app2 = build_app(id2, id3);
  Term out2 = wnf(app2);
  CHECK_EQ(term_tag(out2), TAG_LAM);
  // The substituted lambda survives -- its binder loc is id3's loc.
  CHECK_EQ(term_val(out2), term_val(id3));

  thvm_free();
  TEST_REPORT();
}
