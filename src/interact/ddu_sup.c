// ! X &(&L{x, y}) = v; b
// ------------------------------ DDU-SUP
// ! V &L = v
// ! B &L = b
// &L{! X &(x) = V0; B0, ! X &(y) = V1; B1}
//
// The DDU's label term reduced to a SUP.  Clone (v, b) under that
// SUP's label L into V=(V0,V1) and B=(B0,B1) projections; build two
// inner DDUs (one with the SUP's left child x and (V0, B0), one
// with right y and (V1, B1)); the outer result is SUP^L of those
// two DDUs.  Mirrors HVM4's wnf_ddu_sup in
// TinyHVM/HVM4/clang/wnf/ddu_sup.c.
fn Term interact_ddu_sup(u64 ddu_loc, Term lab_sup) {
  u32 lab     = term_ext(lab_sup);
  u64 sup_loc = term_val(lab_sup);
  Term x      = heap_read(sup_loc + 0);
  Term y      = heap_read(sup_loc + 1);
  Term v      = heap_read(ddu_loc + 1);
  Term bod    = heap_read(ddu_loc + 2);

  // Shared body cells for v, b + 4 projections.
  u64 c = heap_alloc(6);
  heap_set(c + 0, v);
  heap_set(c + 1, bod);
  Term V0 = term_new(0, TAG_DP0, lab, c + 0);
  Term B0 = term_new(0, TAG_DP0, lab, c + 1);
  Term V1 = term_new(0, TAG_DP1, lab, c + 0);
  Term B1 = term_new(0, TAG_DP1, lab, c + 1);
  heap_set(c + 2, V0);
  heap_set(c + 3, B0);
  heap_set(c + 4, V1);
  heap_set(c + 5, B1);

  // Inner DDU(x, V0, B0).
  u64 d0 = heap_alloc(3);
  heap_set(d0 + 0, x);
  heap_set(d0 + 1, V0);
  heap_set(d0 + 2, B0);
  Term dd0 = term_new(0, TAG_DDU, 0, d0);

  // Inner DDU(y, V1, B1).
  u64 d1 = heap_alloc(3);
  heap_set(d1 + 0, y);
  heap_set(d1 + 1, V1);
  heap_set(d1 + 2, B1);
  Term dd1 = term_new(0, TAG_DDU, 0, d1);

  // Outer SUP^L.
  u64 r = heap_alloc(2);
  heap_set(r + 0, dd0);
  heap_set(r + 1, dd1);
  ITRS++;
  multi_emit(RULE_DDU_SUP, MULTI_SLIDE, 0, (u64)lab_sup, 0);
  return term_new(0, TAG_SUP, lab, r);
}
