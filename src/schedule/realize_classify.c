// schedule/realize_classify.c - decide which UOPs in a graph
//                                must "realize" into a backing
//                                buffer (a tinygrad CONTIGUOUS).
//
// A UOp realizes when ANY of:
//   (a) it is the root the caller asked for,
//   (b) it has 2+ distinct UOp parents (multi-consumer),
//   (c) it is a REDUCE (REDUCE outputs always escape into a
//       buffer; one reduce per kernel),
//   (d) f1d may add: movement op whose source can't be aliased.
//
// Output: a small table indexed by the UOp's heap loc; f1d's
// selective materializer will consult realize_is_realized to
// decide whether to emit a UOP_KERNEL or inline the compute
// into its consumer kernel's program.
//
// Note: this pass DOES NOT mutate the heap or the kernel
// table.  It only reads the UOp DAG and populates the table.
// Aliasing of duplicate child references inside a single
// parent (e.g., MUL[x, x]) counts as ONE consumer of x, since
// the materializer dedups.

#define REALIZE_INFO_CAP 4096

typedef struct {
  u64 loc;             // heap loc identifying the UOp instance
  u32 consumer_count;  // # of distinct UOp parents that reference it
  u8  op;              // UOP_*
  u8  realized;        // set in classify_apply
} UOpInfo;

static UOpInfo REALIZE_INFO    [REALIZE_INFO_CAP];
static u32     REALIZE_INFO_LEN = 0;

fn void realize_info_clear(void) {
  REALIZE_INFO_LEN = 0;
}

static u32 realize_info_find(u64 loc) {
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (REALIZE_INFO[i].loc == loc) return i;
  }
  return 0xFFFFFFFFu;
}

static u32 realize_info_get_or_add(u64 loc, u8 op) {
  u32 idx = realize_info_find(loc);
  if (idx != 0xFFFFFFFFu) return idx;
  if (REALIZE_INFO_LEN >= REALIZE_INFO_CAP) return 0xFFFFFFFFu;
  idx = REALIZE_INFO_LEN++;
  REALIZE_INFO[idx].loc            = loc;
  REALIZE_INFO[idx].consumer_count = 0;
  REALIZE_INFO[idx].op             = op;
  REALIZE_INFO[idx].realized       = 0;
  return idx;
}

static void realize_walk_rec(Term t, u8 *visited) {
  if (term_tag(t) != TAG_UOP) return;
  u8 op = term_ext(t);
  if (op == UOP_KERNEL) return;     // already kernelized, opaque
  u64 loc = term_val(t);
  if (loc >= HEAP_NEXT) return;
  if (visited[loc]) return;
  visited[loc] = 1;

  realize_info_get_or_add(loc, op);

  u8 ar = uop_arity(op);
  // Dedup children by loc -- repeated refs (MUL[x, x]) count as
  // a single consumer.
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u64 cloc = term_val(child);
    u8 dup = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;

    u32 cidx = realize_info_get_or_add(cloc, term_ext(child));
    if (cidx != 0xFFFFFFFFu) REALIZE_INFO[cidx].consumer_count++;
    realize_walk_rec(child, visited);
  }
}

fn void realize_classify(Term root) {
  realize_info_clear();
  if (term_tag(root) != TAG_UOP) return;
  if (term_ext(root) == UOP_KERNEL) return;

  // Bitmap sized to HEAP_NEXT (current high-water of the dyn
  // heap) instead of HEAP_CAP -- per-call cost stays
  // proportional to live work, not the 16 MiB max.
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;
  realize_walk_rec(root, visited);
  free(visited);

  // Rule (a): the root itself realizes.
  u32 root_idx = realize_info_find(term_val(root));
  if (root_idx != 0xFFFFFFFFu) REALIZE_INFO[root_idx].realized = 1;

  // Rules (b) + (c): multi-consumer or REDUCE.
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->consumer_count >= 2) info->realized = 1;
    if (info->op == UOP_REDUCE)    info->realized = 1;
  }
}

fn u8 realize_is_realized(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) return 0;
  if (term_ext(uop_term) == UOP_KERNEL) return 1;   // already realized
  u32 idx = realize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) return 0;
  return REALIZE_INFO[idx].realized;
}

fn u32 realize_consumer_count(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) return 0;
  u32 idx = realize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) return 0;
  return REALIZE_INFO[idx].consumer_count;
}
