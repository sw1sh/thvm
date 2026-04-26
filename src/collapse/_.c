// thvm_collapse: enumerate the SUP-tree of a term.
//
// Walks the SUP at the head of `t`.  At each step:
//   - WNF the term to surface the head.
//   - If the head is TAG_SUP, recurse on heap[loc+0] and heap[loc+1].
//   - If the head is TAG_ERA, drop the branch (failed search).
//   - Otherwise, append the term to `out`.
//
// Returns the number of leaves written to `out`, capped at `cap`.
//
// This is the "shallow" collapse: it only follows SUPs that have been
// surfaced to the head by WNF.  Sub-terms whose outer constructor blocks
// reduction are returned as-is.  Deeper collapse (pushing SUP up
// through APP, OP2, EQL, ...) lands as those interactions are added.
static u64 collapse_walk(Term t, Term *out, u64 cap, u64 count) {
  if (count >= cap) return count;
  Term w = wnf(t);
  switch (term_tag(w)) {
    case TAG_SUP: {
      u64  loc = term_val(w);
      Term l   = heap_read(loc + 0);
      Term r   = heap_read(loc + 1);
      count    = collapse_walk(l, out, cap, count);
      count    = collapse_walk(r, out, cap, count);
      return count;
    }
    case TAG_ERA: {
      return count;
    }
    default: {
      out[count++] = w;
      return count;
    }
  }
}

fn u64 thvm_collapse(Term t, Term *out, u64 cap) {
  return collapse_walk(t, out, cap, 0);
}
