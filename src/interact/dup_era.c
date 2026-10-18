// ! &L{x0, x1} = *
// ---------------- DUP-ERA
// x0 <- *
// x1 <- *
fn Term interact_dup_era(u8 side, u64 loc, Term era) {
  ITRS++;
  multi_emit(RULE_DUP_ERA, MULTI_PRUNE, loc, 0, 0);
  return heap_subst_cop(side, loc, era, era);
}
