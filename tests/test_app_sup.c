// test_app_sup.c -- stage 8.1d-i: APP-SUP commutation.
//
// Verifies that APP(&L{f, g}, arg) commutes to &L{APP(f, arg_0),
// APP(g, arg_1)} with the argument shared via a DUP node.
//
// Caveat for 8.1d-ii readers: APP-SUP shares the argument via a
// DUP, which fires only for tags with DUP-* interactions.  Today
// our IC supports DUP-{ERA, LAM, NUM, SUP, BRI, ANY}; CTR and
// FVR are passive (no DUP-CTR / DUP-FVR yet).  When CP enumeration
// uses CTR-shaped rule LHSs as APP-SUP arguments, the DP0/DP1 will
// stick.  Mitigation for 8.1d-ii: pass the rule pair through the
// SUP rather than as the APP arg, OR add DUP-CTR (separate task).

#include "../src/thvm.c"
#include "test.h"

// Build APP(fun, arg) on a fresh heap cell.
static Term mk_app(Term fun, Term arg) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, fun);
  heap_set(loc + 1, arg);
  return term_new(0, TAG_APP, 0, loc);
}

// Build &L{a, b} on a fresh heap cell.
static Term mk_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

// Identity primitive used to verify that APP-SUP fans out the
// argument correctly: each branch's APP fires and returns the
// arg unchanged.
static Term id_prim(Term *args) { return args[0]; }

int main(void) {
  thvm_init();

  // Register the identity primitive once for use across cases.
  prim_register(40u, id_prim, 1u);

  TEST_BEGIN("app-sup/single-fanout-with-pri-children");
  {
    // SUP{PRI(40), PRI(40)} applied to a NUM arg -- DUP-NUM
    // copies the NUM to both branches, each branch's identity
    // primitive returns it.  Result: SUP{NUM(99), NUM(99)}.
    // (We use NUM rather than CTR because DUP-CTR is not yet
    // implemented; CTR is passive in the IC reducer.)
    Term f0  = term_new_pri(40u);
    Term f1  = term_new_pri(40u);
    Term sup = mk_sup(7u, f0, f1);
    Term arg = term_new(0, TAG_NUM, DT_INT32, 99);
    Term app = mk_app(sup, arg);

    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_SUP);
    CHECK_EQ(term_ext(out), 7u);
    // SUP children carry DP-wrapped values from APP-SUP's arg-share;
    // cnf is the readback that drives them through DUP-XXX (Levy-
    // opaque under wnf since the Phase 1 + 2 split).
    Term lhs = cnf(heap_read(term_val(out) + 0));
    Term rhs = cnf(heap_read(term_val(out) + 1));
    CHECK_EQ(term_tag(lhs), TAG_NUM);
    CHECK_EQ((u32)term_val(lhs), 99u);
    CHECK_EQ(term_tag(rhs), TAG_NUM);
    CHECK_EQ((u32)term_val(rhs), 99u);
  }

  TEST_BEGIN("app-sup/preserves-sup-label");
  {
    // Use a non-trivial label to confirm it propagates.
    Term f0  = term_new_pri(40u);
    Term f1  = term_new_pri(40u);
    Term sup = mk_sup(123u, f0, f1);
    Term arg = term_new_ctr(0u, NULL, 0);
    Term app = mk_app(sup, arg);

    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_SUP);
    CHECK_EQ(term_ext(out), 123u);
  }

  TEST_BEGIN("app-sup/asymmetric-children");
  {
    // SUP{PRI(40), ERA} applied to NUM arg.  Left branch fires
    // identity (returns NUM); right branch is APP-ERA which
    // produces ERA regardless of the (DUP'd) arg.
    Term f0  = term_new_pri(40u);
    Term f1  = term_new(0, TAG_ERA, 0, 0);
    Term sup = mk_sup(7u, f0, f1);
    Term arg = term_new(0, TAG_NUM, DT_INT32, 42);
    Term app = mk_app(sup, arg);

    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_SUP);
    Term lhs = cnf(heap_read(term_val(out) + 0));
    Term rhs = cnf(heap_read(term_val(out) + 1));
    CHECK_EQ(term_tag(lhs), TAG_NUM);
    CHECK_EQ((u32)term_val(lhs), 42u);
    CHECK_EQ(term_tag(rhs), TAG_ERA);
  }

  TEST_BEGIN("app-sup/era-arg-fans-out");
  {
    // APP-SUP shares the arg via a DUP.  When the arg is ERA,
    // each branch sees a DUP of ERA -- DUP-ERA simply produces
    // ERA on both sides.  Each branch is APP(PRI(40), ERA),
    // which calls the identity primitive on ERA: returns ERA.
    Term f0  = term_new_pri(40u);
    Term f1  = term_new_pri(40u);
    Term sup = mk_sup(5u, f0, f1);
    Term arg = term_new(0, TAG_ERA, 0, 0);
    Term app = mk_app(sup, arg);

    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_SUP);
    Term lhs = cnf(heap_read(term_val(out) + 0));
    Term rhs = cnf(heap_read(term_val(out) + 1));
    CHECK_EQ(term_tag(lhs), TAG_ERA);
    CHECK_EQ(term_tag(rhs), TAG_ERA);
  }

  TEST_BEGIN("app-sup/iterations-counter-bumped");
  {
    // Spot-check: ITRS counter must increase when APP-SUP fires.
    u64 before = ITRS;
    Term f0  = term_new_pri(40u);
    Term f1  = term_new_pri(40u);
    Term sup = mk_sup(11u, f0, f1);
    Term arg = term_new_ctr(5u, NULL, 0);
    Term app = mk_app(sup, arg);
    (void)wnf(app);
    CHECK(ITRS > before);
  }

  thvm_free();
  TEST_REPORT();
}
