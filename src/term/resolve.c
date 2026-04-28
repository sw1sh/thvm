// term/resolve.c - cheap, lazy outermost-layer resolver.
//
// Follows two kinds of indirection without firing reductions:
//   - TAG_VAR with a SUB-bit cell: take the substituted value and
//     keep walking (so a chain of beta substitutions collapses).
//   - TAG_ALO: force one realisation layer.  alo_force itself is
//     memoised so subsequent walks of the same ALO are cheap.
//
// Anything else (TAG_TEN / TAG_NUM / TAG_UOP / TAG_LAM / TAG_APP /
// TAG_REF / TAG_MAT / TAG_OP2 / ...) is returned unchanged.  In
// particular this DOES NOT fire materialize / kernel / grad
// reductions -- callers that need full WHNF should still use wnf().
//
// Used by interact_grad and materialize_expr to expose the
// outermost layer of `y` / `target` / `expr` so they can pattern-
// match without invoking the wnf stack-machine.

fn Term term_resolve(Term t) {
  for (;;) {
    u8 tag = term_tag(t);
    if (tag == TAG_VAR) {
      Term cell = heap_read(term_val(t));
      if (!term_sub_get(cell)) return t;
      t = term_sub_set(cell, 0);
      continue;
    }
    // DP0 / DP1 over a SUB-bit cell: the DUP fired earlier and
    // heap_subst_cop wrote this projection's value into the cell.
    // Pick it up so callers (interact_grad, materialize) see the
    // resolved value rather than a stale projection term.
    if (tag == TAG_DP0 || tag == TAG_DP1) {
      Term cell = heap_read(term_val(t));
      if (term_sub_get(cell)) {
        t = term_sub_set(cell, 0);
        continue;
      }
      // Grad-flag DP0 = pure passthrough: cell[0] holds y, no DUP
      // ever fires.  Unwrap eagerly so higher-order chain rules
      // (which see fwd-projections from the inner pass embedded in
      // gy_for_a expressions) can pattern-match on the underlying
      // term instead of getting stuck on the DP0 wrapper.
      if (tag == TAG_DP0 && (term_ext(t) & DUP_GRAD_FLAG)) {
        t = heap_read(term_val(t));   // = cell[0] = y
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
