// ! &L{x0, x1} = TEN(t)
// ---------------------- DUP-TEN (annihilate)
// x0 <- TEN(t)
// x1 <- TEN(t)
//
// A TEN is a tid handle: both sides name one tensor, and no buffer is copied.
fn Term interact_dup_ten(u8 side, u64 loc, Term ten) {
  ITRS++;
  multi_emit(RULE_DUP_TEN, MULTI_DIST, loc, 0, 0);
  return heap_subst_cop(side, loc, ten, ten);
}
