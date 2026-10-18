// Exchange with 0: of workers racing for a DP body, one gets it, the rest 0.
fn Term heap_take_atomic(u64 loc) {
  return (Term)__atomic_exchange_n(&HEAP[loc], (u64)0, __ATOMIC_ACQ_REL);
}
