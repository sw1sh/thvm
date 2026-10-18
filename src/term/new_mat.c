// Heap layout: [handler, fallback]; an applied argument takes the handler when
// it is NUM(ext) or a CTR labelled ext (interact/app_mat.c), else the fallback.
fn Term term_new_mat(u32 match_val, Term handler, Term fallback) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, handler);
  heap_set(loc + 1, fallback);
  return term_new(0, TAG_MAT, match_val, loc);
}
