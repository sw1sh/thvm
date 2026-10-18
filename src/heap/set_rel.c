// Release store for a Term another worker may read concurrently (with
// heap_read_acq); heap_subst_var already release-stores its SUB cells.
fn void heap_set_rel(u64 loc, Term t) {
  __atomic_store_n(&HEAP[loc], (u64)t, __ATOMIC_RELEASE);
  WIRE_PROV_BUMP(loc);
}
