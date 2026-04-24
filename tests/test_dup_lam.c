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
    Term out         = wnf(dp0);
    CHECK_EQ(term_tag(out), TAG_LAM);
    CHECK_EQ(ITRS - itrs_before, 1);  // exactly one DUP-LAM firing

    // The new LAM's body cell holds DP0(val=shared, lab) -- the body
    // term is itself duplicated so DP1 (the inactive projection)
    // shares it.  Verifying the structural invariant: body's tag is
    // DP0 with the same dup label.
    u64  new_lam_loc  = term_val(out);
    Term new_lam_body = heap_read(new_lam_loc);
    CHECK_EQ(term_tag(new_lam_body), TAG_DP0);
    CHECK_EQ(term_ext(new_lam_body), 9);
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

    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  thvm_free();
  TEST_REPORT();
}
