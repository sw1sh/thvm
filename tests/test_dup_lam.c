// test_dup_lam.c -- DUP-LAM (lambda cloning).
//
// ! &L{F0, F1} = lam x.x   ->   F0 <- lam x0.x0,  F1 <- lam x1.x1
//
// We construct an identity lambda, dup it, and reduce DP0 to verify
// the active projection comes back as a fresh LAM whose body is a
// VAR pointing at its own binder.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("dup-lam/identity-cloned");
  {
    // Build (lam x. x) at args base lam_loc.
    u64  lam_loc = heap_alloc(1);
    heap_set(lam_loc, term_new(0, TAG_VAR, 0, lam_loc));
    Term lam     = term_new(0, TAG_LAM, 0, lam_loc);

    // Build a DUP cell holding the identity lambda.
    u64  dup_loc = heap_alloc(1);
    heap_set(dup_loc, lam);
    Term dp0     = term_new(0, TAG_DP0, 9, dup_loc);

    u64  itrs_before = ITRS;
    // Plain DPs are Levy-opaque under wnf; cnf is the readback layer
    // that fires DUP-LAM.  cnf is fully-resolving, so the inner
    // DP0(VAR(orig_lam_loc)) chain that DUP-LAM creates also resolves
    // -- the orig_lam_loc cell holds a SUB-flagged SUP from the
    // heap_subst_var call inside interact_dup_lam, so the inner DP-SUP
    // same-label annihilation fires automatically.  Two ITRS total:
    // DUP-LAM + DUP-SUP (annihilate).
    Term out         = cnf(dp0);
    CHECK_EQ(term_tag(out), TAG_LAM);
    CHECK_EQ(ITRS - itrs_before, 2);

    // The new LAM's body cell, fully CNF'd, holds VAR(new_binder).
    // (The intermediate DP0 wrapper was resolved during cnf readback.)
    u64  new_lam_loc  = term_val(out);
    Term new_lam_body = heap_read(new_lam_loc);
    CHECK_EQ(term_tag(new_lam_body), TAG_VAR);
    CHECK_EQ(term_val(new_lam_body), new_lam_loc);
  }

  TEST_BEGIN("dup-lam/applying-cloned-identity-yields-input");
  {
    // !&L{f0, f1} = lam x.x;  apply f0 to ERA  ->  ERA.
    // Tests that DUP-LAM produces a usable lambda end-to-end.
    u64  lam_loc = heap_alloc(1);
    heap_set(lam_loc, term_new(0, TAG_VAR, 0, lam_loc));
    Term lam     = term_new(0, TAG_LAM, 0, lam_loc);

    u64  dup_loc = heap_alloc(1);
    heap_set(dup_loc, lam);
    Term dp0     = term_new(0, TAG_DP0, 11, dup_loc);

    Term era     = term_new(0, TAG_ERA, 0, 0);
    u64  app_loc = heap_alloc(2);
    heap_set(app_loc + 0, dp0);
    heap_set(app_loc + 1, era);
    Term app     = term_new(0, TAG_APP, 0, app_loc);

    // The APP head is a DP0 (Levy-opaque under wnf).  cnf realizes
    // the APP-LAM after firing the DUP-LAM during readback.
    Term out = cnf(app);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  thvm_free();
  TEST_REPORT();
}
