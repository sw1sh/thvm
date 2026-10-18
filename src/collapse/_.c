// cnf, not wnf, so a DP-headed child (Levy-opaque under wnf) fires its DUP
// before the walk branches on its head.
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

// The leaves of t's SUP tree in DFS order, ERA branches dropped; returns how
// many were written to `out` (at most `cap`).
fn u64 thvm_collapse(Term t, Term *out, u64 cap) {
  return collapse_walk(t, out, cap, 0);
}
