// schedule/materialize.c - tinygrad-style scheduler.
//
// g2a (current): scheduler skeleton.  Calls realize_classify to mark
// kernel-boundary UOps (root + multi-consumer + REDUCE), then topo-
// sorts the boundary set by producer-to-consumer depth so g2b's
// build_kernel can iterate in dependency order.  No code emission
// yet -- thvm_materialize returns the input term unchanged.

#define BOUNDARY_ORDER_CAP 1024
static u64 BOUNDARY_ORDER[BOUNDARY_ORDER_CAP];
static u32 BOUNDARY_ORDER_LEN = 0;

#define BOUNDARY_DEPTH_INVALID 0xFFFFFFFFu
static u32 BOUNDARY_DEPTH[REALIZE_INFO_CAP];

// Depth of a UOp loc in terms of upstream BOUNDARY count: 1 + max
// upstream boundary depth if `loc` itself is a boundary; otherwise
// just the max upstream depth (so non-boundary intermediates pass
// the count through to their downstream consumer).  Returns 0 for
// locs not in REALIZE_INFO (TEN leaves, KERNELs, anything outside
// the classified subgraph).
static u32 boundary_depth_rec(u64 loc) {
  u32 idx = realize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  if (BOUNDARY_DEPTH[idx] != BOUNDARY_DEPTH_INVALID) return BOUNDARY_DEPTH[idx];
  // Pre-set to 0 so any (impossible) cycle terminates without
  // infinite recursion.
  BOUNDARY_DEPTH[idx] = 0;

  u32 max_up = 0;
  u8  ar     = uop_arity(REALIZE_INFO[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP)        continue;
    if (term_ext(child) == UOP_KERNEL)     continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    u32 cd = boundary_depth_rec(cloc);
    if (cd > max_up) max_up = cd;
  }
  u32 d = REALIZE_INFO[idx].realized ? max_up + 1 : max_up;
  BOUNDARY_DEPTH[idx] = d;
  return d;
}

// Populate BOUNDARY_ORDER with realized UOp locs sorted by depth
// ascending; tie-break by loc to make the order deterministic.
static void topo_sort_boundaries(Term root) {
  BOUNDARY_ORDER_LEN = 0;
  for (u32 i = 0; i < REALIZE_INFO_CAP; i++)
    BOUNDARY_DEPTH[i] = BOUNDARY_DEPTH_INVALID;
  // Trigger depth computation across the whole reachable subgraph.
  boundary_depth_rec(term_val(root));

  // Collect (loc, depth) for every realized entry.
  struct { u64 loc; u32 depth; } items[BOUNDARY_ORDER_CAP];
  u32 n = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN && n < BOUNDARY_ORDER_CAP; i++) {
    if (!REALIZE_INFO[i].realized) continue;
    items[n].loc   = REALIZE_INFO[i].loc;
    items[n].depth = BOUNDARY_DEPTH[i];
    n++;
  }
  // Insertion sort: depth ascending, then loc ascending.
  for (u32 i = 1; i < n; i++) {
    for (u32 j = i; j > 0; j--) {
      u8 swap = (items[j].depth <  items[j-1].depth)
            || (items[j].depth == items[j-1].depth && items[j].loc < items[j-1].loc);
      if (!swap) break;
      u64 lt = items[j].loc;   items[j].loc   = items[j-1].loc;   items[j-1].loc   = lt;
      u32 dt = items[j].depth; items[j].depth = items[j-1].depth; items[j-1].depth = dt;
    }
  }
  for (u32 i = 0; i < n; i++) BOUNDARY_ORDER[BOUNDARY_ORDER_LEN++] = items[i].loc;
}

fn u32 materialize_boundary_count(void)         { return BOUNDARY_ORDER_LEN; }
fn u64 materialize_boundary_at(u32 i)           { return i < BOUNDARY_ORDER_LEN ? BOUNDARY_ORDER[i] : 0; }

// Stub: view tests still call this directly to exercise the
// movement-op kernelize path.  g2c will replace with the real
// view-rewrite path.  Returns the input unchanged for now.
fn Term materialize_uop_in_env(Term t, u32 env_id) { (void)env_id; return t; }

fn Term thvm_materialize(Term term) {
  if (term_tag(term) != TAG_UOP)        return term;
  if (term_ext(term) == UOP_KERNEL)     return term;
  realize_classify(term);
  topo_sort_boundaries(term);
  // g2b will iterate BOUNDARY_ORDER and build a kernel per boundary,
  // then return a Term referencing the sink kernel.
  return term;
}
