// &(&L{x, y}){a, b}
// -------------------------- DSU-SUP
// ! A &L = a
// ! B &L = b
// &L{&(x){A0, B0}, &(y){A1, B1}}
//
// The DSU's label term reduced to a SUP.  Clone (a, b) under that
// SUP's label L into A=(A0,A1) and B=(B0,B1) projections; build two
// inner DSUs (one with the SUP's left child x, one with right y);
// the outer result is SUP^L of those two DSUs.  Mirrors HVM4's
// wnf_dsu_sup in TinyHVM/HVM4/clang/wnf/dsu_sup.c.
//
// Clone shape -- a 2-cell shared body block + four projection cells:
//   c+0  shared dup body for a
//   c+1  shared dup body for b
//   c+2  DP0_L over c+0 (= A0)
//   c+3  DP0_L over c+1 (= B0)
//   c+4  DP1_L over c+0 (= A1)
//   c+5  DP1_L over c+1 (= B1)
//
// Inner DSUs each take 3 cells (lab_term, child0, child1):
//   d0+0 = SUP's x at sup_loc+0
//   d0+1 = c+2 (= A0)
//   d0+2 = c+3 (= B0)
//   d1+0 = SUP's y at sup_loc+1
//   d1+1 = c+4 (= A1)
//   d1+2 = c+5 (= B1)
//
// Outer SUP^L block (2 cells) holds the two DSUs.
fn Term interact_dsu_sup(u64 dsu_loc, Term lab_sup) {
  u32 lab     = term_ext(lab_sup);
  u64 sup_loc = term_val(lab_sup);
  Term x      = heap_read(sup_loc + 0);
  Term y      = heap_read(sup_loc + 1);
  Term a      = heap_read(dsu_loc + 1);
  Term b      = heap_read(dsu_loc + 2);

  // Shared body + 4 projections.
  u64 c = heap_alloc(6);
  heap_set(c + 0, a);
  heap_set(c + 1, b);
  Term A0 = term_new(0, TAG_DP0, lab, c + 0);
  Term B0 = term_new(0, TAG_DP0, lab, c + 1);
  Term A1 = term_new(0, TAG_DP1, lab, c + 0);
  Term B1 = term_new(0, TAG_DP1, lab, c + 1);
  heap_set(c + 2, A0);
  heap_set(c + 3, B0);
  heap_set(c + 4, A1);
  heap_set(c + 5, B1);

  // Inner DSU(x, A0, B0).
  u64 d0 = heap_alloc(3);
  heap_set(d0 + 0, x);
  heap_set(d0 + 1, A0);
  heap_set(d0 + 2, B0);
  Term ds0 = term_new(0, TAG_DSU, 0, d0);

  // Inner DSU(y, A1, B1).
  u64 d1 = heap_alloc(3);
  heap_set(d1 + 0, y);
  heap_set(d1 + 1, A1);
  heap_set(d1 + 2, B1);
  Term ds1 = term_new(0, TAG_DSU, 0, d1);

  // Outer SUP^L.
  u64 r = heap_alloc(2);
  heap_set(r + 0, ds0);
  heap_set(r + 1, ds1);
  ITRS++;
  return term_new(0, TAG_SUP, lab, r);
}
