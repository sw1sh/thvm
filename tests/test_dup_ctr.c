// test_dup_ctr.c - DUP-CTR commute clones a labelled constructor.
//
// ! &L{X0,X1} = #K{a, b, ...}
// ----------------------------
// X0 <- #K{ DP0[L,a], DP0[L,b], ... }
// X1 <- #K{ DP1[L,a], DP1[L,b], ... }
//
// The two projections each see a fresh CTR with the same label and
// arity; each child position holds a DUP projection of the original
// child.  Reading both projections fires DUP-CTR exactly once
// (subsequent reads resolve via SUB on the shared dup body cell).

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("dup-ctr/arity-0-atomic");
  {
    // Nullary CTR (e.g. #ZER{}) is atomic; DUP copies the term value.
    Term ctr = term_new_ctr(0 /*label*/, NULL, 0);
    u64  dup = heap_alloc(1);
    heap_set(dup, ctr);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    u64  itrs_before = ITRS;
    Term out = wnf(dp0);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), 0);
    CHECK_EQ(term_ctr_n(out), 0);
    CHECK_EQ(ITRS - itrs_before, 1);
  }

  TEST_BEGIN("dup-ctr/arity-1-#SUC{#ZER}");
  {
    // ! &7{x0,x1} = #SUC{ #ZER }
    // Each xN should be a fresh #SUC whose child reduces to #ZER.
    Term zer    = term_new_ctr(0 /*ZER*/, NULL, 0);
    Term suc[1] = { zer };
    Term ctr    = term_new_ctr(1 /*SUC*/, suc, 1);
    u64  dup    = heap_alloc(1);
    heap_set(dup, ctr);
    Term dp0 = term_new(0, TAG_DP0, 7, dup);
    Term dp1 = term_new(0, TAG_DP1, 7, dup);
    u64  itrs_before = ITRS;
    Term r0 = wnf(dp0);
    Term r1 = wnf(dp1);
    CHECK_EQ(term_tag(r0), TAG_CTR);
    CHECK_EQ(term_ext(r0), 1);     // #SUC label
    CHECK_EQ(term_ctr_n(r0), 1);
    CHECK_EQ(term_tag(r1), TAG_CTR);
    CHECK_EQ(term_ext(r1), 1);
    CHECK_EQ(term_ctr_n(r1), 1);
    // Each child is itself a DP (or already-resolved CTR via SUB).
    Term r0_child = wnf(term_ctr_at(r0, 0));
    Term r1_child = wnf(term_ctr_at(r1, 0));
    CHECK_EQ(term_tag(r0_child), TAG_CTR);
    CHECK_EQ(term_ext(r0_child), 0);   // #ZER
    CHECK_EQ(term_tag(r1_child), TAG_CTR);
    CHECK_EQ(term_ext(r1_child), 0);
    // Total interactions: 1 outer DUP-CTR (#SUC) + 1 inner DUP-CTR (#ZER
    // atomic, fires once on first read; second read hits SUB).
    CHECK_EQ(ITRS - itrs_before, 2);
  }

  TEST_BEGIN("dup-ctr/arity-2-pair");
  {
    // ! &3{x0,x1} = #P{ #ZER, #SUC{#ZER} }
    Term zer1   = term_new_ctr(0, NULL, 0);
    Term zer2   = term_new_ctr(0, NULL, 0);
    Term suc[1] = { zer2 };
    Term suc_z  = term_new_ctr(1, suc, 1);
    Term pair[2] = { zer1, suc_z };
    Term ctr     = term_new_ctr(2 /*#P*/, pair, 2);
    u64  dup     = heap_alloc(1);
    heap_set(dup, ctr);
    Term dp0 = term_new(0, TAG_DP0, 3, dup);
    Term dp1 = term_new(0, TAG_DP1, 3, dup);
    Term r0 = wnf(dp0);
    Term r1 = wnf(dp1);
    CHECK_EQ(term_tag(r0), TAG_CTR);
    CHECK_EQ(term_ext(r0), 2);
    CHECK_EQ(term_ctr_n(r0), 2);
    CHECK_EQ(term_tag(r1), TAG_CTR);
    CHECK_EQ(term_ext(r1), 2);
    CHECK_EQ(term_ctr_n(r1), 2);
    // First field: #ZER on both projections.
    Term r0_a = wnf(term_ctr_at(r0, 0));
    Term r1_a = wnf(term_ctr_at(r1, 0));
    CHECK_EQ(term_tag(r0_a), TAG_CTR);
    CHECK_EQ(term_ext(r0_a), 0);
    CHECK_EQ(term_tag(r1_a), TAG_CTR);
    CHECK_EQ(term_ext(r1_a), 0);
    // Second field: #SUC{#ZER} on both projections.
    Term r0_b = wnf(term_ctr_at(r0, 1));
    Term r1_b = wnf(term_ctr_at(r1, 1));
    CHECK_EQ(term_tag(r0_b), TAG_CTR);
    CHECK_EQ(term_ext(r0_b), 1);
    CHECK_EQ(term_tag(r1_b), TAG_CTR);
    CHECK_EQ(term_ext(r1_b), 1);
  }

  thvm_free();
  TEST_REPORT();
}
