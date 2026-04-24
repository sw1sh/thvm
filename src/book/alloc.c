// book/alloc.c - bump-allocate cells in the static def-template heap.
//
// BOOK_HEAP is parallel to HEAP but immutable once written -- it
// stores the snapshot of named definitions registered via
// thvm_def_register().  Cell index 0 is reserved as a "no template"
// sentinel.

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
