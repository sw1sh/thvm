// Heap layout: [NUM(n), c_0, ..., c_{n-1}]; ext = label (0 = anonymous tuple).
fn Term term_new_ctr(u32 label, const Term *children, u32 n) {
  u64 loc = heap_alloc(1 + n);
  heap_set(loc, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) {
    heap_set(loc + 1 + i, children[i]);
  }
  return term_new(0, TAG_CTR, label, loc);
}

fn u32 term_ctr_n(Term ctr_term) {
  if (term_tag(ctr_term) != TAG_CTR) return 0;
  Term n_cell = heap_read(term_val(ctr_term));
  if (term_tag(n_cell) != TAG_NUM)   return 0;
  return (u32)term_val(n_cell);
}

// 0, not a valid Term, when i is out of range or ctr_term is not a CTR.
fn Term term_ctr_at(Term ctr_term, u32 i) {
  u32 n = term_ctr_n(ctr_term);
  if (i >= n) return 0;
  return heap_read(term_val(ctr_term) + 1 + i);
}
