// ! &L{x0, x1} = ANY
// ------------------- DUP-ANY (annihilate)
// x0 <- ANY
// x1 <- ANY
//
// ANY is atomic with no payload, so each projection is a fresh ANY
// term -- copying the constant into both sides.
fn Term interact_dup_any(u8 side, u64 loc, Term any) {
  ITRS++;
  multi_emit(RULE_DUP_ANY, MULTI_DIST, loc, 0, 0);
  return heap_subst_cop(side, loc, any, any);
}
