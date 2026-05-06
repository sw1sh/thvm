// Compare-and-swap on a heap cell.  Returns 1 if the swap committed
// (the cell held `expected` and now holds `desired`), 0 otherwise --
// in which case the cell's current value is written back to *expected
// for the caller to retry against.
//
// Strong CAS: no spurious failures, suitable for one-shot-claim
// patterns (DUP-* taking the body, parent-promotion, etc.) where the
// caller doesn't want to retry on contention.
fn _Bool heap_cas(u64 loc, Term *expected, Term desired) {
  return __atomic_compare_exchange_n(
      &HEAP[loc],
      (u64 *)expected,
      (u64)desired,
      /*weak=*/0,
      /*success_mo=*/__ATOMIC_ACQ_REL,
      /*failure_mo=*/__ATOMIC_ACQUIRE);
}
