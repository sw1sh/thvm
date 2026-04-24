fn Term heap_take(u64 loc) {
  Term t = HEAP[loc];
  HEAP[loc] = 0;
  return t;
}
