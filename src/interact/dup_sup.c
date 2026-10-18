// ! &L{x0, x1} = &R{a, b}
// ----------------------- DUP-SUP
// if L == R:                     (annihilate)
//   x0 <- a
//   x1 <- b
// else:                          (commute)
//   ! &L{A0, A1} = a
//   ! &L{B0, B1} = b
//   x0 <- &R{A0, B0}
//   x1 <- &R{A1, B1}
//
// Commute block [a, b, A0, B0, A1, B1]: x0 = &R{A0, B0} at c+2, x1 at c+4.
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup) {
  u64 sup_loc = term_val(sup);
  u32 sup_lab = term_ext(sup);
  ITRS++;
  // Trace carriers: `loc`, whose wire_prov names the slide (OP2-SUP, ...) that
  // allocated this DUP cell, then the SUP, often a pre-trace literal.
  if (lab == sup_lab) {
    multi_emit(RULE_DUP_SUP_ANN, MULTI_MERGE, loc, (u64)sup, lab);
    Term tm0 = heap_read(sup_loc + 0);
    Term tm1 = heap_read(sup_loc + 1);
    return heap_subst_cop(side, loc, tm0, tm1);
  }
  multi_emit(RULE_DUP_SUP_COM, MULTI_SPLIT, loc, (u64)sup, lab);

  Term a = heap_read(sup_loc + 0);
  Term b = heap_read(sup_loc + 1);

  u64 c = heap_alloc(6);
  heap_set(c + 0, a);
  heap_set(c + 1, b);
  heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
  heap_set(c + 3, term_new(0, TAG_DP0, lab, c + 1));
  heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
  heap_set(c + 5, term_new(0, TAG_DP1, lab, c + 1));

  Term x0 = term_new(0, TAG_SUP, sup_lab, c + 2);
  Term x1 = term_new(0, TAG_SUP, sup_lab, c + 4);
  return heap_subst_cop(side, loc, x0, x1);
}
