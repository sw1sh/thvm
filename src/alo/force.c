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
  u32  state_id  = (u32)term_val(sid_cell);
  return alo_realize(book_term, state_id);
}
