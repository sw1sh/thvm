// ! &L{x0, x1} = *
// ---------------- DUP-ERA
// x0 <- *
// x1 <- *
//
// Both projections receive the eraser.  We substitute one side and
// return the other so the active dup-frame's result is also the
// eraser.
fn Term interact_dup_era(u8 side, u64 loc, Term era) {
  ITRS++;
  multi_emit(RULE_DUP_ERA, MULTI_PRUNE, loc, 0, 0);
  return heap_subst_cop(side, loc, era, era);
}
