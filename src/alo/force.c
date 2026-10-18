// Realizes a TAG_ALO [book_term, NUM(state_id)] one layer deep; alo_realize
// wraps the children in fresh ALOs, realized only when wnf enters them.
Term alo_force(Term alo_term) {
  u64 alo_loc = term_val(alo_term);
  Term book_term = heap_read(alo_loc + 0);
  Term sid_cell  = heap_read(alo_loc + 1);
  // Memoised: cell 0 takes the result and cell 1 an ERA, so all entries to a
  // shared ALO see one copy; fresh copies would fail grad's leaf identity test.
  if (term_tag(sid_cell) != TAG_NUM) return book_term;
  u32  state_id  = (u32)term_val(sid_cell);
  Term realised  = alo_realize(book_term, state_id);
  heap_set(alo_loc + 0, realised);
  heap_set(alo_loc + 1, term_new(0, TAG_ERA, 0, 0));
  return realised;
}
