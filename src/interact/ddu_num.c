// ! X &(#n) = v; b
// ---------------- DDU-NUM
// ! X &n = v
// b(X0, X1)
//
// The DDU's label term reduced to NUM(n).  Build a plain DUP at
// label n on a fresh body cell holding v; create the two
// projections X0=DP0_n, X1=DP1_n; APP the body to (X0, X1) in
// left-to-right order.  Mirrors HVM4's wnf_ddu_num in
// TinyHVM/HVM4/clang/wnf/ddu_num.c.
//
// Cell layout:
//   c (1 cell)        body holding v -- DP0_n / DP1_n project off it
//   app0 (2 cells)    (body, X0)
//   app1 (2 cells)    (app0, X1)
fn Term interact_ddu_num(u64 ddu_loc, Term lab_num) {
  u32 lab  = (u32)term_val(lab_num);
  Term v   = heap_read(ddu_loc + 1);
  Term bod = heap_read(ddu_loc + 2);

  u64 c = heap_alloc(1);
  heap_set(c, v);
  Term X0 = term_new(0, TAG_DP0, lab, c);
  Term X1 = term_new(0, TAG_DP1, lab, c);

  u64 a0 = heap_alloc(2);
  heap_set(a0 + 0, bod);
  heap_set(a0 + 1, X0);
  Term app0 = term_new(0, TAG_APP, 0, a0);

  u64 a1 = heap_alloc(2);
  heap_set(a1 + 0, app0);
  heap_set(a1 + 1, X1);
  ITRS++;
  multi_emit(RULE_DDU_NUM, MULTI_SLIDE, 0, (u64)lab_num, 0);
  return term_new(0, TAG_APP, 0, a1);
}
