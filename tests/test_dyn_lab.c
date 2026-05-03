// test_dyn_lab.c - dynamic-label SUP/DUP (TAG_DSU / TAG_DDU).
//
// Verifies the strict-on-label semantics: DSU/DDU descend into the
// label term; once it resolves to NUM/ERA/SUP, the matching DSU-X /
// DDU-X interaction fires.  Mirrors HVM4's wnf_dsu_* / wnf_ddu_*.

#include "../src/thvm.c"
#include "test.h"

static Term mk_dsu(Term lab, Term a, Term b) {
  return term_new_dsu(lab, a, b);
}

static Term mk_ddu(Term lab, Term v, Term bod) {
  return term_new_ddu(lab, v, bod);
}

int main(void) {
  thvm_init();

  // === DSU-NUM: &(#7){a, b} -> SUP^7{a, b} ============================
  TEST_BEGIN("dyn-lab/dsu-num");
  {
    Term lab = build_num(7);
    Term a   = build_num(11);
    Term b   = build_num(22);
    Term r   = wnf(mk_dsu(lab, a, b));
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 7);
    Term ra  = heap_read(term_val(r) + 0);
    Term rb  = heap_read(term_val(r) + 1);
    CHECK_EQ(term_tag(ra), TAG_NUM);
    CHECK_EQ(term_val(ra), 11);
    CHECK_EQ(term_tag(rb), TAG_NUM);
    CHECK_EQ(term_val(rb), 22);
  }

  // === DSU-ERA: &(ERA){a, b} -> ERA ===================================
  TEST_BEGIN("dyn-lab/dsu-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term r   = wnf(mk_dsu(era, build_num(1), build_num(2)));
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  // === DSU-SUP: &(SUP^L{x,y}){a,b} -> SUP^L{DSU(x,A0,B0), DSU(y,A1,B1)}
  // After wnf, outer is SUP^L; each inner DSU still has a non-NUM
  // label, so they remain DSU.  Verify the structural shape.
  TEST_BEGIN("dyn-lab/dsu-sup-commute-shape");
  {
    Term lab_sup = build_sup(5, build_num(8), build_num(9));
    Term a       = build_num(100);
    Term b       = build_num(200);
    Term r       = wnf(mk_dsu(lab_sup, a, b));
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 5);
    Term left  = heap_read(term_val(r) + 0);
    Term right = heap_read(term_val(r) + 1);
    CHECK_EQ(term_tag(left),  TAG_DSU);
    CHECK_EQ(term_tag(right), TAG_DSU);
    // Each inner DSU now has NUM label (8 / 9); a follow-up wnf
    // should resolve them to plain SUP^8 / SUP^9.
    Term left_w  = wnf(left);
    Term right_w = wnf(right);
    CHECK_EQ(term_tag(left_w),  TAG_SUP);
    CHECK_EQ(term_ext(left_w),  8);
    CHECK_EQ(term_tag(right_w), TAG_SUP);
    CHECK_EQ(term_ext(right_w), 9);
  }

  // === DDU-NUM: ! X &(#3) = NUM(42); body -> b applied to (X0, X1)
  // body = LAM x. LAM y. OP2(ADD, x, y) so result = NUM(42 + 42) = 84
  // (the two DUP^3 projections of NUM(42) both reduce to NUM(42)).
  TEST_BEGIN("dyn-lab/ddu-num-add");
  {
    Term lab = build_num(3);
    Term v   = build_num(42);
    // Inner LAM: \y -> x + y.  Outer LAM: \x -> inner.
    u64 inner_lam = heap_alloc(1);
    u64 outer_lam = heap_alloc(1);
    u64 op_loc    = heap_alloc(2);
    heap_set(op_loc + 0, term_new(0, TAG_VAR, 0, outer_lam));
    heap_set(op_loc + 1, term_new(0, TAG_VAR, 0, inner_lam));
    heap_set(inner_lam, term_new(0, TAG_OP2, OP_ADD, op_loc));
    heap_set(outer_lam, term_new(0, TAG_LAM, 0, inner_lam));
    Term body = term_new(0, TAG_LAM, 0, outer_lam);
    Term r    = wnf(mk_ddu(lab, v, body));
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 84);
  }

  // === DDU-ERA: ! X &(ERA) = v; body -> ERA ============================
  TEST_BEGIN("dyn-lab/ddu-era");
  {
    Term era = term_new(0, TAG_ERA, 0, 0);
    Term r   = wnf(mk_ddu(era, build_num(99), term_new(0, TAG_LAM, LAM_ERA_MASK, 0)));
    CHECK_EQ(term_tag(r), TAG_ERA);
  }

  // === DDU-SUP: ! X &(SUP^L{x,y}) = v; b -> SUP^L{DDU(x,V0,B0), DDU(y,V1,B1)}
  // Verify the wnf-reduces-to-SUP^L shape; inner DDUs still need wnf
  // again to resolve their labels.
  TEST_BEGIN("dyn-lab/ddu-sup-commute-shape");
  {
    Term lab_sup = build_sup(11, build_num(0), build_num(0));
    // body must duplicate so the cnf-clone of v doesn't leak; use a
    // body that erases its args -- LAM_ERA_MASK + ERA body for both.
    u64 inner_lam = heap_alloc(1);
    u64 outer_lam = heap_alloc(1);
    heap_set(inner_lam, term_new(0, TAG_ERA, 0, 0));
    heap_set(outer_lam, term_new(0, TAG_LAM, LAM_ERA_MASK, inner_lam));
    Term body = term_new(0, TAG_LAM, LAM_ERA_MASK, outer_lam);
    Term r    = wnf(mk_ddu(lab_sup, build_num(7), body));
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 11);
    Term left  = heap_read(term_val(r) + 0);
    Term right = heap_read(term_val(r) + 1);
    CHECK_EQ(term_tag(left),  TAG_DDU);
    CHECK_EQ(term_tag(right), TAG_DDU);
  }

  // === DSU label = OP2(ADD, NUM(2), NUM(3)) -> NUM(5) -> SUP^5{a,b}
  // Confirms strict-on-label drives full reduction of a compound
  // label term, not just one step.
  TEST_BEGIN("dyn-lab/dsu-label-reduces-op2");
  {
    u64 op = heap_alloc(2);
    heap_set(op + 0, build_num(2));
    heap_set(op + 1, build_num(3));
    Term lab = term_new(0, TAG_OP2, OP_ADD, op);
    Term r   = wnf(mk_dsu(lab, build_num(10), build_num(20)));
    CHECK_EQ(term_tag(r), TAG_SUP);
    CHECK_EQ(term_ext(r), 5);
  }

  thvm_free();
  TEST_REPORT();
}
