// term/new_any.c - construct a TAG_ANY wildcard.
//
// ANY is atomic (no heap cells, no payload).  Under EQL it matches
// anything (returns NUM(1)).  Under DUP it annihilates by copying
// itself into both projections.  Used as the IC-side encoding of
// existential / Skolem variables in the ATP plan.

fn Term term_new_any(void) {
  return term_new(0, TAG_ANY, 0, 0);
}
