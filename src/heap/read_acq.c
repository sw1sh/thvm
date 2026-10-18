// Acquire load paired with the release stores of heap_subst_var and
// heap_set_rel; plain heap_read stays the default elsewhere.
fn Term heap_read_acq(u64 loc) {
  return (Term)__atomic_load_n(&HEAP[loc], __ATOMIC_ACQUIRE);
}
