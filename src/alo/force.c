// alo/force.c - resolve one TAG_ALO into a dynamic term.
//
// The ALO term's val points at a 2-cell dyn block holding
// [book_term, NUM(state_id)].  Force reads them and asks
// alo_realize to walk one layer.
//
// This is intentionally non-recursive on the children: those are
// re-wrapped in fresh ALOs by alo_realize and only realised when
// the wnf later enters them.

Term alo_force(Term alo_term) {
  u64 alo_loc = term_val(alo_term);
  Term book_term = heap_read(alo_loc + 0);
  Term sid_cell  = heap_read(alo_loc + 1);
  // Memoise: once realised, replace the second cell with a non-NUM
  // marker (TAG_ERA) so subsequent fires of the same ALO term
  // return the cached realised value instead of allocating a fresh
  // copy.  Without this, repeated wnf entries to a shared ALO --
  // the typical case in a recursive REF body where multiple uses
  // of the bound `w` go through the same wrapper -- each produce
  // distinct dyn allocations and the GRAD's pointer-equality
  // leaf check breaks.
  if (term_tag(sid_cell) != TAG_NUM) return book_term;
  u32  state_id  = (u32)term_val(sid_cell);
  Term realised  = alo_realize(book_term, state_id);
  heap_set(alo_loc + 0, realised);
  heap_set(alo_loc + 1, term_new(0, TAG_ERA, 0, 0));
  return realised;
}
