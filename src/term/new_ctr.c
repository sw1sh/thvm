// term/new_ctr.c - construct + read a TAG_CTR labelled constructor.
//
// Heap layout: [NUM(arity), c_0, ..., c_{n-1}].
//   ext = constructor label (0 = anonymous tuple).
//   val = heap loc of the layout cell.
//
// CTR is the standard HVM4 way to express a labelled product type --
// pairs, tuples, sums-tagged-by-label, etc.  This codebase had no
// CTR primitive before k0a; multi-output operations (the immediate
// driver) and future ATP-style ADT pattern matching all want the
// same passive aggregate.
//
// Today CTR is PASSIVE in the IC reducer: there's no DUP-CTR /
// ERA-CTR interaction yet.  When a future consumer DUPs or ERAs a
// CTR, those rules can land in interact/dup_ctr.c / interact/era_ctr.c
// without disturbing the existing semantics.

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

fn Term term_ctr_at(Term ctr_term, u32 i) {
  u32 n = term_ctr_n(ctr_term);
  if (i >= n) return 0;
  return heap_read(term_val(ctr_term) + 1 + i);
}
