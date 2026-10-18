// BOOK_HEAP holds the static def templates (thvm_def_register), immutable once
// written; loc 0 is reserved as "no template", so BOOK_NEXT starts at 1.
fn u64 book_alloc(u64 size) {
  u64 at   = BOOK_NEXT;
  u64 next = at + size;
  if (next > BOOK_CAP) {
    fprintf(stderr, "book_alloc: out of memory (need %llu cells)\n",
            (unsigned long long)size);
    exit(1);
  }
  BOOK_NEXT = next;
  return at;
}
