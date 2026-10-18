// Stores `value` at `loc` with the SUB flag, which VAR/DP0/DP1 entry follows;
// released, so a worker that sees the flag (heap_read_acq) sees the whole Term.
fn void heap_subst_var(u64 loc, Term value) {
  __atomic_store_n(&HEAP[loc],
                   (u64)term_sub_set(value, 1),
                   __ATOMIC_RELEASE);
  WIRE_PROV_BUMP(loc);
}
