// term/new_mat.c - construct a TAG_MAT (numeric switch atom).
//
// Heap layout: [handler, fallback].  ext = NUM value to match
// against the argument when applied (APP-MAT-NUM).

fn Term term_new_mat(u32 match_val, Term handler, Term fallback) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, handler);
  heap_set(loc + 1, fallback);
  return term_new(0, TAG_MAT, match_val, loc);
}
