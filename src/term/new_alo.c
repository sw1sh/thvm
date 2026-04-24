// term/new_alo.c - allocate a fresh ALO node.
//
// Heap layout: [book_term, NUM(state_id)].  alo_force reads both
// cells when it fires.

fn Term term_new_alo(Term book_term, u32 state_id) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, book_term);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, state_id));
  return term_new(0, TAG_ALO, 0, loc);
}
