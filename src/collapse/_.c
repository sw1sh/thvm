// thvm_collapse: enumerate the SUP-tree of a term.
//
// Walks the SUP at the head of `t`.  At each step:
//   - CNF the term to surface the head (lifts SUPs through compound
//     nodes, fires DUP-XXX during readback).
//   - If the head is TAG_SUP, recurse on heap[loc+0] and heap[loc+1].
//   - If the head is TAG_ERA, drop the branch (failed search).
//   - Otherwise, append the term to `out`.
//
// Returns the number of leaves written to `out`, capped at `cap`.
//
// Uses cnf rather than plain wnf so children with DP-rooted heads
// (Levy-opaque under wnf since the Phase 1 + 2 readback split) get
// driven through their dup interactions before the walk decides.
static u64 collapse_walk(Term t, Term *out, u64 cap, u64 count) {
  if (count >= cap) return count;
  Term w = cnf(t);
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
