// ! &L{x0, x1} = TEN(t)
// ---------------------- DUP-TEN (annihilate)
// x0 <- TEN(t)
// x1 <- TEN(t)
//
// TAG_TEN is an atomic handle (just a tid).  Both projections reference
// the same tensor by id; sharing is implicit.  No heap copy.
fn Term interact_dup_ten(u8 side, u64 loc, Term ten) {
  ITRS++;
  multi_emit(RULE_DUP_TEN, MULTI_PLUMB, (u64)ten, 0, 0);
  return heap_subst_cop(side, loc, ten, ten);
}
