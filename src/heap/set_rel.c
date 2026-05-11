// Release-store wrapper around HEAP[loc] = t.  Use this for any
// write that publishes a fully-formed Term to a slot another worker
// may read concurrently -- the matching reader uses heap_read_acq
// (or heap_subst_cop's helper, which already release-stores).
fn void heap_set_rel(u64 loc, Term t) {
  __atomic_store_n(&HEAP[loc], (u64)t, __ATOMIC_RELEASE);
  WIRE_PROV_BUMP(loc);
}
