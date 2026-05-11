// Release-ordered heap_subst_var.  Same logical write as
// heap/subst_var.c -- install `value` with the SUB flag set so a
// subsequent VAR/DP0/DP1 enter on `loc` picks it up -- but with a
// release fence so a concurrent reader using heap_read_acq sees a
// fully-formed Term rather than a half-published cell.
//
// Pair with heap_subst_cop_rel for the DUP-style two-side substitution
// (one fresh result returned, the other side resolved on next entry
// via the SUB-flagged cell).
fn void heap_subst_var_rel(u64 loc, Term value) {
  __atomic_store_n(&HEAP[loc],
                   (u64)term_sub_set(value, 1),
                   __ATOMIC_RELEASE);
  WIRE_PROV_BUMP(loc);
}
