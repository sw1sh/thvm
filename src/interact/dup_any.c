// ! &L{x0, x1} = ANY
// ------------------- DUP-ANY (annihilate)
// x0 <- ANY
// x1 <- ANY
fn Term interact_dup_any(u8 side, u64 loc, Term any) {
  ITRS++;
  multi_emit(RULE_DUP_ANY, MULTI_DIST, loc, 0, 0);
  return heap_subst_cop(side, loc, any, any);
}
