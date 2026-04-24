// Pair-substitution helper for DUP-style interactions: substitutes
// one of {r0, r1} into `loc` and returns the other.  The caller picks
// `side` based on which projection (DP0=0, DP1=1) is currently active
// so the active side gets the fresh result and the other side picks up
// its value via the substitution-flagged cell on its next entry.
fn Term heap_subst_cop(u8 side, u64 loc, Term r0, Term r1) {
  heap_subst_var(loc, side == 0 ? r1 : r0);
  return side == 0 ? r0 : r1;
}
