// term/new_inc.c - construct a TAG_INC priority wrapper.
//
// Heap layout: [body].  INC is a WNF atom: the reducer leaves it
// alone.  `thvm_collapse_ordered` observes INC depth as a priority
// key (lower depth = higher priority = emitted first).  Used to
// implement the SupGen-style "enumerate cheapest candidates first"
// pattern for ATP CP selection.

fn Term term_new_inc(Term body) {
  u64 loc = heap_alloc(1);
  heap_set(loc, body);
  return term_new(0, TAG_INC, 0, loc);
}
