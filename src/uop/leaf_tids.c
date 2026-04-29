// uop/leaf_tids.c - walk a UOP DAG and collect distinct TAG_TEN
// leaf tids.  Pure tree walk, nothing grad-specific (TGrad happens
// to be the first caller; future BEAM / kernel introspection passes
// will use the same primitive).
//
// Iterative + heap-loc-keyed visited bitmap so shared sub-UOPs are
// walked once.  The recursive WL equivalent re-walked them
// exponentially -- LeNet's 8-weight forward measured 1.6 s in WL
// and < 2 ms here.
//
// `out_tids` receives up to `cap` tid values; `*n_out` returns the
// actual count written (0 on empty walk).  `visited[loc]` indexes
// up to HEAP_NEXT (the live region) -- HEAP_CAP would be wasteful.

fn void uop_leaf_tids(Term root, u32 *out_tids, u32 cap, u32 *n_out) {
  *n_out = 0;
  if (cap == 0 || HEAP_NEXT == 0) return;

  u8   *visited = (u8   *)calloc(HEAP_NEXT, sizeof(u8));
  Term *stack   = (Term *)malloc(HEAP_NEXT * sizeof(Term));
  if (visited == NULL || stack == NULL) {
    if (visited) free(visited);
    if (stack)   free(stack);
    return;
  }
  u64 sp = 0;
  stack[sp++] = root;

  while (sp > 0 && *n_out < cap) {
    Term t   = stack[--sp];
    u8   tag = term_tag(t);
    u64  loc = term_val(t);

    if (tag == TAG_TEN) {
      u32 tid = (u32)loc;
      int dup = 0;
      for (u32 i = 0; i < *n_out; i++) {
        if (out_tids[i] == tid) { dup = 1; break; }
      }
      if (!dup) out_tids[(*n_out)++] = tid;
      continue;
    }

    if (loc >= HEAP_NEXT) continue;
    if (visited[loc]) continue;
    visited[loc] = 1;

    if (tag == TAG_UOP) {
      u32 op = term_ext(t);
      u8 ar  = uop_arity((u8)op);
      for (u8 i = 0; i < ar; i++) {
        if (sp + 1 >= HEAP_NEXT) break;
        stack[sp++] = heap_read(loc + i);
      }
    } else if (tag == TAG_DP0 || tag == TAG_DP1) {
      if (sp + 1 < HEAP_NEXT) stack[sp++] = heap_read(loc);
    }
    // Other tags: leaves with no recursable children.
  }

  free(visited);
  free(stack);
}
