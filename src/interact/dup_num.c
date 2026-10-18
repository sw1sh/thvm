// ! &L{x0, x1} = NUM(v)
// ---------------------- DUP-NUM (annihilate)
// x0 <- NUM(v)
// x1 <- NUM(v)
fn Term interact_dup_num(u8 side, u64 loc, Term num) {
  ITRS++;
  // The trace carrier is `loc`: a NUM's term_val is its value, not a heap loc.
  multi_emit(RULE_DUP_NUM, MULTI_DIST, loc, 0, 0);
  return heap_subst_cop(side, loc, num, num);
}
