// test_gc_roots.c - gc1 of the tracing-GC arc.  Verifies
// gc_collect_roots picks up Terms from the result + the
// pending wnf stack + DEFS[].

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  Term roots[256];
  u32 n;

  TEST_BEGIN("gc-roots/result-only");
  // Empty wnf stack + empty DEFS: only the result lands in the
  // root set.
  Term r = term_new(0, TAG_ERA, 0, 0);
  gc_collect_roots(r, roots, 256, &n);
  CHECK_EQ(n, 1);
  CHECK_EQ(roots[0], r);

  TEST_BEGIN("gc-roots/skips-zero-result");
  // result = 0 means "no result" (e.g., before any wnf call);
  // collect returns an empty set when nothing else is live.
  gc_collect_roots(0, roots, 256, &n);
  CHECK_EQ(n, 0);

  TEST_BEGIN("gc-roots/picks-up-wnf-stack-frames");
  // Simulate a wnf bail by writing into WNF_LAST_STACK.
  WNF_LAST_STACK[0] = term_new(0, TAG_APP, 0, 100);
  WNF_LAST_STACK[1] = term_new(0, TAG_DP0, 0, 200);
  WNF_LAST_STACK_LEN = 2;
  Term r2 = term_new(0, TAG_ERA, 0, 0);
  gc_collect_roots(r2, roots, 256, &n);
  CHECK_EQ(n, 3);   // result + 2 stack frames
  CHECK_EQ(roots[0], r2);
  CHECK_EQ(roots[1], WNF_LAST_STACK[0]);
  CHECK_EQ(roots[2], WNF_LAST_STACK[1]);
  WNF_LAST_STACK_LEN = 0;   // reset for following tests

  TEST_BEGIN("gc-roots/picks-up-defs");
  // Register a couple of defs; gc_collect_roots includes them.
  DEFS[5]  = term_new(0, TAG_LAM, 0, 42);
  DEFS[10] = term_new(0, TAG_REF, 0, 7);
  Term r3 = term_new(0, TAG_ERA, 0, 0);
  gc_collect_roots(r3, roots, 256, &n);
  CHECK_EQ(n, 3);                 // result + 2 defs
  CHECK_EQ(roots[0], r3);
  // Order: defs are appended in name-index order.
  CHECK_EQ(roots[1], DEFS[5]);
  CHECK_EQ(roots[2], DEFS[10]);
  DEFS[5] = 0; DEFS[10] = 0;       // cleanup

  TEST_BEGIN("gc-roots/cap-truncates");
  // Tight cap: only the first 2 roots fit.  No crash, no
  // overrun; out_n reflects what was actually written.
  WNF_LAST_STACK[0] = term_new(0, TAG_APP, 0, 1);
  WNF_LAST_STACK[1] = term_new(0, TAG_APP, 0, 2);
  WNF_LAST_STACK[2] = term_new(0, TAG_APP, 0, 3);
  WNF_LAST_STACK_LEN = 3;
  Term r4 = term_new(0, TAG_ERA, 0, 0);
  gc_collect_roots(r4, roots, 2, &n);
  CHECK_EQ(n, 2);
  CHECK_EQ(roots[0], r4);
  CHECK_EQ(roots[1], WNF_LAST_STACK[0]);
  WNF_LAST_STACK_LEN = 0;

  TEST_BEGIN("gc-roots/null-args-safe");
  // Defensive: NULL out / out_n returns without crashing.
  gc_collect_roots(0, NULL, 256, &n);   // out_n still set
  CHECK_EQ(n, 0);
  gc_collect_roots(0, roots, 256, NULL);  // doesn't crash
  gc_collect_roots(0, roots, 0, &n);    // cap = 0 -> no writes
  CHECK_EQ(n, 0);

  thvm_free();
  TEST_REPORT();
}
