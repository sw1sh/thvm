// Acquire-load wrapper around HEAP[loc].  Pairs with heap_set_rel /
// heap_subst_var_rel on the producer side so the reader sees a fully
// formed Term (val + tag + sub bits) rather than a half-written cell.
//
// Single-thread cost is identical to plain heap_read on x86 / arm64 --
// __atomic_load_n with __ATOMIC_ACQUIRE compiles to a regular load
// followed by an acquire fence (or no fence on TSO platforms).  We
// keep heap_read as the relaxed default; callers explicitly switch
// to heap_read_acq at the few SUB-bit synchronisation points.
fn Term heap_read_acq(u64 loc) {
  return (Term)__atomic_load_n(&HEAP[loc], __ATOMIC_ACQUIRE);
}
