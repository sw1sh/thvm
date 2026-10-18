// Heap layout: [cond, body]; strict on cond: NUM(0) or ERA -> ERA, other NUM ->
// wnf(body), a SUP commutes (body is DUPed), else stuck: a filter for collapse.
fn Term term_new_when(Term cond, Term body) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, cond);
  heap_set(loc + 1, body);
  return term_new(0, TAG_WHEN, 0, loc);
}
