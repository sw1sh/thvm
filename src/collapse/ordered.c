// thvm_collapse_ordered: priority-aware enumeration.
//
// Same shallow walk as thvm_collapse, but each leaf is tagged with
// the number of TAG_INC wrappers seen on the path from the root.
// Lower INC-depth = higher priority = emitted first.  Ties are
// broken by original DFS position (i.e. the sort is stable).
//
// Implementation: collect (Term, priority, dfs_idx) triples, then
// stable-sort by (priority asc, dfs_idx asc), then write the Terms
// out.  Caller-supplied buffer; capped at `cap`.

typedef struct {
  Term term;
  u32  pri;
  u32  idx;   // tie-breaker: original DFS order
} CollapseLeafPri;

static u64 collapse_walk_pri(Term t, CollapseLeafPri *buf, u64 cap,
                             u64 count, u32 pri) {
  if (count >= cap) return count;
  Term w = wnf(t);
  switch (term_tag(w)) {
    case TAG_SUP: {
      u64  loc = term_val(w);
      Term l   = heap_read(loc + 0);
      Term r   = heap_read(loc + 1);
      count    = collapse_walk_pri(l, buf, cap, count, pri);
      count    = collapse_walk_pri(r, buf, cap, count, pri);
      return count;
    }
    case TAG_ERA: {
      return count;
    }
    case TAG_INC: {
      u64  loc  = term_val(w);
      Term body = heap_read(loc);
      return collapse_walk_pri(body, buf, cap, count, pri + 1);
    }
    default: {
      buf[count].term = w;
      buf[count].pri  = pri;
      buf[count].idx  = (u32)count;
      return count + 1;
    }
  }
}

static int collapse_pri_cmp(const void *a, const void *b) {
  const CollapseLeafPri *x = (const CollapseLeafPri *)a;
  const CollapseLeafPri *y = (const CollapseLeafPri *)b;
  if (x->pri != y->pri) return (x->pri < y->pri) ? -1 : 1;
  if (x->idx != y->idx) return (x->idx < y->idx) ? -1 : 1;
  return 0;
}

fn u64 thvm_collapse_ordered(Term t, Term *out, u64 cap) {
  if (cap == 0) return 0;
  CollapseLeafPri *buf = (CollapseLeafPri *)malloc(cap * sizeof(CollapseLeafPri));
  if (!buf) return 0;
  u64 n = collapse_walk_pri(t, buf, cap, 0, 0);
  qsort(buf, n, sizeof(CollapseLeafPri), collapse_pri_cmp);
  for (u64 i = 0; i < n; i++) out[i] = buf[i].term;
  free(buf);
  return n;
}
