// term/new_ann.c - construct a TAG_ANN annotation {val : typ}.
//
// Heap[loc]   = val
// Heap[loc+1] = typ
//
// Reduces by inspecting `typ` (strict reduce, dispatch on tag).  The
// rules live inline in src/wnf/_.c TAG_ANN case + src/interact/ann_lam.c
// + src/interact/ann_bri.c.

fn Term term_new_ann(Term val, Term typ) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, val);
  heap_set(loc + 1, typ);
  return term_new(0, TAG_ANN, 0, loc);
}
