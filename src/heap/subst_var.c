// Install `value` at the binder's heap cell with the SUB flag set, so a
// subsequent VAR/DP0/DP1 enter on `loc` will pick it up and clear the flag.
fn void heap_subst_var(u64 loc, Term value) {
  HEAP[loc] = term_sub_set(value, 1);
}
