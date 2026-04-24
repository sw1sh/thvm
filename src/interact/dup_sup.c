// ! &L{x0, x1} = &R{a, b}
// ----------------------- DUP-SUP
// if L == R:                     (annihilate)
//   x0 <- a
//   x1 <- b
// else:                          (commute -- not yet implemented)
//   ! &L{A0, A1} = a
//   ! &L{B0, B1} = b
//   x0 <- &R{A0, B0}
//   x1 <- &R{A1, B1}
//
// Only the same-label (annihilating) case is implemented today.  The
// commuting case is deferred until a test exercises it -- when stuck
// we put the SUP back into the dup cell so the result is a well-formed
// (but unreduced) DP0/DP1 term.
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup) {
  u64 sup_loc = term_val(sup);
  u32 sup_lab = term_ext(sup);
  if (lab == sup_lab) {
    ITRS++;
    Term tm0 = heap_read(sup_loc + 0);
    Term tm1 = heap_read(sup_loc + 1);
    return heap_subst_cop(side, loc, tm0, tm1);
  }
  // commute case: leave stuck (rebuild DP node holding the SUP)
  heap_set(loc, sup);
  return term_new(0, side == 0 ? TAG_DP0 : TAG_DP1, lab, loc);
}
