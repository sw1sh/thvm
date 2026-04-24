fn u64 heap_alloc(u64 size) {
  u64 at   = HEAP_NEXT;
  u64 next = at + size;
  if (next > HEAP_CAP) {
    fprintf(stderr, "heap_alloc: out of memory (need %llu cells)\n",
            (unsigned long long)size);
    exit(1);
  }
  HEAP_NEXT = next;
  return at;
}
