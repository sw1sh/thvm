fn u64 heap_alloc(u64 size) {
  u64 at   = HEAP_NEXT;
  u64 next = at + size;
  // Bound = GC's from-space end when GC is active, else HEAP_CAP.
  u64 cap  = gc_enabled() ? gc_from_end() : HEAP_CAP;
  if (next > cap) {
    fprintf(stderr, "heap_alloc: from-space exhausted (need %llu cells, "
            "HEAP_NEXT=%llu, cap=%llu); call gc_collect or grow GC_SPACE_SZ\n",
            (unsigned long long)size,
            (unsigned long long)at,
            (unsigned long long)cap);
    exit(1);
  }
  HEAP_NEXT = next;
  return at;
}
