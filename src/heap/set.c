fn void heap_set(u64 loc, Term t) {
  HEAP[loc] = t;
  WIRE_PROV_BUMP(loc);
}
