// DUP-style pair substitution: the entered projection `side` (DP0=0, DP1=1)
// gets its result back; the other result goes into `loc` as a SUB.
fn Term heap_subst_cop(u8 side, u64 loc, Term r0, Term r1) {
  heap_subst_var(loc, side == 0 ? r1 : r0);
  return side == 0 ? r0 : r1;
}
