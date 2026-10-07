// Atomic exchange-with-zero on a heap cell.  Used by DUP-* and other
// active-pair interactions that "consume" the body cell: only one
// worker may successfully claim a non-zero value; concurrent callers
// see 0 and bail to the stuck path.
//
// A plain read-and-clear (`t = HEAP[loc]; HEAP[loc] = 0`) would be a torn
// read-modify-write under MT.
fn Term heap_take_atomic(u64 loc) {
  return (Term)__atomic_exchange_n(&HEAP[loc], (u64)0, __ATOMIC_ACQ_REL);
}
