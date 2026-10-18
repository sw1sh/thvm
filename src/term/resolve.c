// Follows a VAR or DP over a SUB cell, a grad DP0 (y passed through) and an
// ALO (one memoised layer); unlike wnf it fires no kernel, materialize or grad.
fn Term term_resolve(Term t) {
  for (;;) {
    u8 tag = term_tag(t);
    if (tag == TAG_VAR) {
      Term cell = heap_read(term_val(t));
      if (!term_sub_get(cell)) return t;
      t = term_sub_set(cell, 0);
      continue;
    }
    // The DUP fired, and heap_subst_cop left this side's value in the cell.
    if (tag == TAG_DP0 || tag == TAG_DP1) {
      Term cell = heap_read(term_val(t));
      if (term_sub_get(cell)) {
        t = term_sub_set(cell, 0);
        continue;
      }
      // A grad DP0 never fires; unwrap it to y so a higher-order chain rule
      // can match the term under it.
      if (tag == TAG_DP0 && (term_ext(t) & DUP_GRAD_FLAG)) {
        t = heap_read(term_val(t));
        continue;
      }
      return t;
    }
    if (tag == TAG_ALO) {
      t = alo_force(t);
      continue;
    }
    return t;
  }
}
