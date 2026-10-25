// schedule/bufferize_classify.c - decide which UOps realize into a buffer
// (their own kernel) ahead of the unified rangeify pass.
//
// The walk records each UOp's consumers (BUFFERIZE_NODES, CMAP_LL).  The root,
// multi-consumer nodes, REDUCEs and a few correctness cases are seeded, then
// named rules unmark the REDUCEs that may fuse: realize conservatively and
// remove by rule, as tinygrad's remove_bufferize does
// (schedule/rangeify.py:50-102).  materialize reads each node's `realized`
// bit.  The classification only reads the heap; run_rangeify_unified, called
// last, builds BUFFERIZE terms.

// UOpInfo, CMapNode and the table caps are declared in thvm.h.

UOpInfo BUFFERIZE_NODES    [BUFFERIZE_NODES_CAP];
u32     BUFFERIZE_NODES_LEN = 0;

CMapNode CMAP_LL    [CMAP_LL_CAP];
u32      CMAP_LL_LEN = 0;

// Open-addressed loc -> BUFFERIZE_NODES index, so a lookup stays O(1) on a
// large training graph.
#define BUFFERIZE_NODES_HASH_CAP (1u << 16)   // 64K slots, BUFFERIZE_NODES_CAP = 16K
#define BUFFERIZE_NODES_HASH_EMPTY 0xFFFFFFFFu
static u32 BUFFERIZE_NODES_HASH[BUFFERIZE_NODES_HASH_CAP];

static inline u32 bufferize_node_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (BUFFERIZE_NODES_HASH_CAP - 1);
}

fn void bufferize_info_clear(void) {
  BUFFERIZE_NODES_LEN = 0;
  CMAP_LL_LEN = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_HASH_CAP; i++)
    BUFFERIZE_NODES_HASH[i] = BUFFERIZE_NODES_HASH_EMPTY;
}

fn u32 bufferize_info_find(u64 loc) {
  u32 h = bufferize_node_hash(loc);
  for (u32 probe = 0; probe < BUFFERIZE_NODES_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BUFFERIZE_NODES_HASH_CAP - 1);
    u32 idx = BUFFERIZE_NODES_HASH[i];
    if (idx == BUFFERIZE_NODES_HASH_EMPTY) return 0xFFFFFFFFu;
    if (BUFFERIZE_NODES[idx].loc == loc) return idx;
  }
  return 0xFFFFFFFFu;
}

static void bufferize_node_hash_insert(u64 loc, u32 idx) {
  u32 h = bufferize_node_hash(loc);
  for (u32 probe = 0; probe < BUFFERIZE_NODES_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BUFFERIZE_NODES_HASH_CAP - 1);
    if (BUFFERIZE_NODES_HASH[i] == BUFFERIZE_NODES_HASH_EMPTY) {
      BUFFERIZE_NODES_HASH[i] = idx;
      return;
    }
  }
  // Unreachable: 64K slots hold at most BUFFERIZE_NODES_CAP entries.
}

static u32 bufferize_node_get_or_add(u64 loc, u8 op) {
  u32 idx = bufferize_info_find(loc);
  if (idx != 0xFFFFFFFFu) return idx;
  if (BUFFERIZE_NODES_LEN >= BUFFERIZE_NODES_CAP) return 0xFFFFFFFFu;
  idx = BUFFERIZE_NODES_LEN++;
  BUFFERIZE_NODES[idx].loc            = loc;
  BUFFERIZE_NODES[idx].consumer_count = 0;
  BUFFERIZE_NODES[idx].reasons        = 0;
  BUFFERIZE_NODES[idx].cmap_head      = 0xFFFFFFFFu;
  BUFFERIZE_NODES[idx].op             = op;
  BUFFERIZE_NODES[idx].realized       = 0;
  bufferize_node_hash_insert(loc, idx);
  return idx;
}

static void bufferize_cmap_add(u32 producer_idx, u64 consumer_loc) {
  if (producer_idx == 0xFFFFFFFFu) return;
  // On overflow the edge is dropped; consumer_count stays exact.
  if (CMAP_LL_LEN >= CMAP_LL_CAP) return;
  u32 slot = CMAP_LL_LEN++;
  CMAP_LL[slot].consumer_loc = consumer_loc;
  CMAP_LL[slot].next         = BUFFERIZE_NODES[producer_idx].cmap_head;
  BUFFERIZE_NODES[producer_idx].cmap_head = slot;
}

// Fills up to `cap` consumer locs of `producer_loc` and returns the full
// count, which may exceed cap.
fn u32 bufferize_consumers_for_loc(u64 producer_loc, u64 *out_locs, u32 cap) {
  u32 idx = bufferize_info_find(producer_loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u32 cur = BUFFERIZE_NODES[idx].cmap_head;
  u32 n = 0;
  while (cur != 0xFFFFFFFFu) {
    if (out_locs != NULL && n < cap) out_locs[n] = CMAP_LL[cur].consumer_loc;
    n++;
    cur = CMAP_LL[cur].next;
  }
  return n;
}

static void bufferize_node_mark(UOpInfo *info, u32 reason) {
  if (info == NULL) {
    return;
  }
  info->realized = 1;
  info->reasons |= reason;
}

static void bufferize_node_unmark(UOpInfo *info, u32 reason) {
  if (info == NULL) {
    return;
  }
  // A matmul REDUCE stays realized: as its own kernel it reaches the GEMM
  // dispatch (BLAS, tensor cores).
  if (info->reasons & BUFFERIZE_REASON_MATMUL) {
    info->reasons |= reason;
    return;
  }
  info->realized = 0;
  info->reasons |= reason;
}

// The first UOp reachable from t through TAG_DP0/TAG_DP1 cells (slot 0, then
// slot 1), or 0 past `bound` hops or on a cycle (visited_dls holds up to cap
// seen cell locs).  It lets a cmap edge cross TGrad's grad-projection cells,
// so consumer walks see reduce -> DP -> reduce links in a backward graph.
static Term bufferize_unwrap_dp(Term t, u64 *visited_dls, u32 *n_visited,
                                 u32 cap, u32 bound) {
  for (u32 hops = 0; hops < bound; hops++) {
    t = term_resolve(t);
    u8 tag = term_tag(t);
    if (tag == TAG_UOP) return t;
    if (tag != TAG_DP0 && tag != TAG_DP1) return 0;
    u64 dl = term_val(t);
    if (dl >= HEAP_NEXT) return 0;
    int seen = 0;
    for (u32 v = 0; v < *n_visited; v++) if (visited_dls[v] == dl) { seen = 1; break; }
    if (seen) return 0;
    if (*n_visited < cap) visited_dls[(*n_visited)++] = dl;
    Term inner0 = bufferize_unwrap_dp(heap_read(dl + 0), visited_dls,
                                       n_visited, cap, bound - hops - 1);
    if (inner0 != 0) return inner0;
    t = heap_read(dl + 1);
  }
  return 0;
}

static void bufferize_walk_rec(Term t, u8 *visited) {
  if (term_tag(t) != TAG_UOP) return;
  u8 op = term_ext(t);
  if (op == UOP_KERNEL) return;     // already kernelized, opaque
  u64 loc = term_val(t);
  if (loc >= HEAP_NEXT) return;
  if (visited[loc]) return;
  visited[loc] = 1;

  bufferize_node_get_or_add(loc, op);

  u8 ar = uop_arity(op);
  // Dedup children by loc -- repeated refs (MUL[x, x]) count as
  // a single consumer.
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 i = 0; i < ar; i++) {
    // Resolve as visit() (materialize.c) does, so both walks agree on the
    // boundaries.
    Term raw = heap_read(loc + i);
    Term child = term_resolve(raw);
    u8 ctag = term_tag(child);
    // A DP child adds only a cmap edge to the UOp behind it, no consumer
    // count, so it never triggers the MULTI seed.
    if (ctag == TAG_DP0 || ctag == TAG_DP1) {
      u64 dp_vis[32];
      u32 dp_n = 0;
      Term inner = bufferize_unwrap_dp(child, dp_vis, &dp_n, 32, 16);
      if (inner != 0 && term_tag(inner) == TAG_UOP
          && term_ext(inner) != UOP_KERNEL) {
        u64 iloc = term_val(inner);
        u32 iidx = bufferize_node_get_or_add(iloc, term_ext(inner));
        if (iidx != 0xFFFFFFFFu) bufferize_cmap_add(iidx, loc);
        bufferize_walk_rec(inner, visited);
      }
      continue;
    }
    if (ctag != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u64 cloc = term_val(child);
    u8 dup = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;

    u32 cidx = bufferize_node_get_or_add(cloc, term_ext(child));
    if (cidx != 0xFFFFFFFFu) {
      BUFFERIZE_NODES[cidx].consumer_count++;
      bufferize_cmap_add(cidx, loc);
    }
    bufferize_walk_rec(child, visited);
  }
}

// True iff t is a CONST under at most 8 EXPAND / RESHAPE / PERMUTE / CAST /
// BITCAST wrappers, as in `mean = reduce / N`, which Tensor.wl (liftNumeric,
// broadcastScalar) lowers to MUL(reduce, EXPAND(CONST(1/N))).
static int bufferize_term_is_broadcast_of_const(Term t, u32 depth) {
  if (depth > 8) return 0;
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  u8 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_CONST) return 1;
  if (op == UOP_EXPAND || op == UOP_RESHAPE || op == UOP_PERMUTE
   || op == UOP_CAST   || op == UOP_BITCAST) {
    Term src = term_resolve(heap_read(loc));
    return bufferize_term_is_broadcast_of_const(src, depth + 1);
  }
  return 0;
}

// The loc of child_loc's only UOp parent in BUFFERIZE_NODES, or 0 for none
// or several (a linear scan).
static u64 bufferize_unique_uop_parent(u64 child_loc) {
  u64 found = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    u8  pop  = BUFFERIZE_NODES[i].op;
    if (ploc == child_loc) continue;
    u8 ar = uop_arity(pop);
    int hits = 0;
    for (u8 j = 0; j < ar; j++) {
      Term c = term_resolve(heap_read(ploc + j));
      if (term_tag(c) != TAG_UOP) continue;
      if (term_val(c) == child_loc) { hits = 1; break; }
    }
    if (hits) {
      if (found != 0) return 0;     // 2+ parents: bail
      found = ploc;
    }
  }
  return found;
}

// A scalar unary: NEG, RECIP, SQRT, EXP2 or LOG2.
static u8 const bufferize_upat_scalar_unary_alt[] = {
  UOP_NEG, UOP_RECIP, UOP_SQRT, UOP_EXP2, UOP_LOG2, UOP_SIN, 0
};
static UPat const bufferize_upat_scalar_unary = {
  0, 1, 0, -1, NULL, bufferize_upat_scalar_unary_alt
};

// A layout-only movement (RESHAPE, PERMUTE), which keeps the value at every
// coordinate.
static u8 const bufferize_upat_movement_passthrough_alt[] = {
  UOP_RESHAPE, UOP_PERMUTE, 0
};
static UPat const bufferize_upat_movement_passthrough = {
  0, 1, 0, -1, NULL, bufferize_upat_movement_passthrough_alt
};

// Any movement op; each keeps its source at heap[loc + 0].
static u8 const bufferize_upat_movement_any_alt[] = {
  UOP_RESHAPE, UOP_PERMUTE, UOP_EXPAND, UOP_PAD, UOP_SHRINK, UOP_FLIP, 0
};
static UPat const bufferize_upat_movement_any = {
  0, 1, 0, -1, NULL, bufferize_upat_movement_any_alt
};

// ADD or MUL with both operands bound, for the const-sibling test.
static u8 const bufferize_upat_alu2_alt[] = {UOP_ADD, UOP_MUL, 0};
static UPat const bufferize_upat_alu2_children[2] = {
  {0, 0xFF, 0, 0, NULL, NULL},
  {0, 0xFF, 0, 1, NULL, NULL},
};
static UPat const bufferize_upat_alu2 = {
  0, 2, 0, -1, bufferize_upat_alu2_children, bufferize_upat_alu2_alt
};

// True iff the node at `cur` keeps the post-reduce value per coordinate: a
// scalar unary, a RESHAPE / PERMUTE, or an ADD / MUL with a (broadcast)
// CONST operand.
static int bufferize_chain_hop_is_scalar_preserving(u64 cur, u8 pop) {
  Term cur_term = term_new(0, TAG_UOP, pop, cur);
  if (upat_match(&bufferize_upat_scalar_unary, cur_term, NULL)) return 1;
  if (upat_match(&bufferize_upat_movement_passthrough, cur_term, NULL)) return 1;
  Term bindings[UPAT_NUM_BINDINGS] = {0};
  if (upat_match(&bufferize_upat_alu2, cur_term, bindings)) {
    if (bufferize_term_is_broadcast_of_const(bindings[0], 0)) return 1;
    if (bufferize_term_is_broadcast_of_const(bindings[1], 0)) return 1;
  }
  return 0;
}

// 1 iff child_loc has a parent and every parent's chain reaches an EXPAND
// through scalar-preserving hops; asked at the reduce and at a shared
// mid-chain node (a BatchNorm mean feeds forward and backward).
static int bufferize_chain_walk_all_parents_is_broadcast(
    u64 child_loc, u32 hops_left);

// Follows the chain up from parent_loc to an EXPAND, branching at a
// multi-consumer node; hops_left bounds it.
static int bufferize_chain_walk_is_broadcast(u64 parent_loc, u32 hops_left) {
  u64 cur = parent_loc;
  while (hops_left > 0) {
    u32 idx = bufferize_info_find(cur);
    if (idx == 0xFFFFFFFFu) return 0;
    u8 pop = BUFFERIZE_NODES[idx].op;
    if (pop == UOP_EXPAND) return 1;
    if (!bufferize_chain_hop_is_scalar_preserving(cur, pop)) return 0;
    u64 next = bufferize_unique_uop_parent(cur);
    if (next == 0) {
      return bufferize_chain_walk_all_parents_is_broadcast(cur, hops_left - 1);
    }
    cur = next;
    hops_left--;
  }
  return 0;
}

static int bufferize_chain_walk_all_parents_is_broadcast(
    u64 child_loc, u32 hops_left) {
  if (hops_left == 0) return 0;
  u32 n_parents = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    if (ploc == child_loc) continue;
    u8  pop  = BUFFERIZE_NODES[i].op;
    u8  ar   = uop_arity(pop);
    int hits = 0;
    for (u8 j = 0; j < ar; j++) {
      Term c = term_resolve(heap_read(ploc + j));
      if (term_tag(c) == TAG_UOP && term_val(c) == child_loc) {
        hits = 1; break;
      }
    }
    if (!hits) continue;
    if (!bufferize_chain_walk_is_broadcast(ploc, hops_left)) return 0;
    n_parents++;
  }
  return n_parents > 0;
}

// 1 iff every consumer chain of the REDUCE broadcasts it back, the softmax
// sum and BatchNorm mean idiom:
//   REDUCE -> (scalar unary | RESHAPE | PERMUTE | ADD/MUL by const)* -> EXPAND
static int bufferize_reduce_consumer_is_broadcast_chain(u64 reduce_loc) {
  return bufferize_chain_walk_all_parents_is_broadcast(reduce_loc, 8);
}


// 1 iff the REDUCE at loc leads, through single-consumer non-matmul REDUCEs,
// to a still-realized REDUCE (the chain's kernel root).  The reduce-chain
// unmark asks this, not the consumer's own flag, which it may already have
// cleared in a chain of 3+ reduces.
static int bufferize_reduce_consumer_in_realized_chain(u64 loc, u32 hops_left) {
  while (hops_left-- > 0) {
    u32 idx = bufferize_info_find(loc);
    if (idx == 0xFFFFFFFFu) return 0;
    if (BUFFERIZE_NODES[idx].op != UOP_REDUCE) return 0;
    if (BUFFERIZE_NODES[idx].reasons & BUFFERIZE_REASON_MATMUL) return 0;
    if (BUFFERIZE_NODES[idx].realized) return 1;        // reached chain root
    if (BUFFERIZE_NODES[idx].consumer_count != 1) return 0;
    u64 cons[1];
    if (bufferize_consumers_for_loc(loc, cons, 1) != 1) return 0;
    loc = cons[0];                                      // mid-chain: keep walking
  }
  return 0;
}

// True iff the node at loc is an epilogue that may fuse into the REDUCE at
// reduce_loc: a unary elementwise op, or a binary one whose other operand is
// a broadcast scalar, so it iterates exactly the reduce's output ranges (the
// optimizer's `(1-b1)*g`).  Reduce and epilogue then share one kernel, as in
// tinygrad; visit() (materialize.c) takes a REDUCE mid-kernel.
static int bufferize_is_scalar_preserving_epilogue(u64 loc, u64 reduce_loc) {
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u8 op = BUFFERIZE_NODES[idx].op;
  if (uop_is_unary_elementwise(op)) return 1;
  if (!uop_is_binary_elementwise(op)) return 0;
  Term a = term_resolve(heap_read(loc + 0));
  Term b = term_resolve(heap_read(loc + 1));
  u64 a_loc = (term_tag(a) == TAG_UOP) ? term_val(a) : 0;
  u64 b_loc = (term_tag(b) == TAG_UOP) ? term_val(b) : 0;
  int a_is_reduce = (a_loc == reduce_loc);
  int b_is_reduce = (b_loc == reduce_loc);
  if (a_is_reduce == b_is_reduce) return 0;          // both or neither
  Term other = a_is_reduce ? b : a;
  // The other operand is a CONST, or a 1-element TEN behind movement ops (a
  // scalar the py bridge realizes before broadcasting): one load either way.
  int saw_movement = 0;
  for (u32 hops = 0; hops < 8; hops++) {
    if (term_tag(other) == TAG_TEN) {
      Shape lsh = {0};
      if (!saw_movement) return 0;               // a same-shape buffer, not scalar
      if (!term_shape_in(other, 0, &lsh)) return 0;
      u64 numel = 1;
      for (u32 d = 0; d < lsh.ndim; d++) numel *= lsh.dims[d];
      return numel == 1;
    }
    if (term_tag(other) != TAG_UOP) return 0;
    u8 oop = term_ext(other);
    if (oop == UOP_CONST) return 1;
    if (!uop_is_movement(oop)) return 0;
    saw_movement = 1;
    other = term_resolve(heap_read(term_val(other) + 0));   // movement src
  }
  return 0;
}

// Fan-in cap: the Metal renderer declines a kernel with more than 30 inputs
// (render_metal.c cg_emit_via_uop), so a wide ADD/MUL tree (an Adam update, a
// BatchNorm-train backward) is split: its largest elementwise or movement
// child realizes until the boundary has at most `cap` input leaves or no such
// child is left.
static int bufferize_metal_tile_fanin_cap_enabled(void) {
#ifdef THVM_HAS_CUDA
  // Opt-in on CUDA (THVM_CUDA_FANIN_CAP=1): its renderer takes any number of
  // inputs, and a split has faulted there (CUDA_ERROR_ILLEGAL_ADDRESS).
  if (CUDA_READY) {
    char const *ce = getenv("THVM_CUDA_FANIN_CAP");
    return ce != NULL && ce[0] == '1';
  }
#endif
  char const *tile = getenv("THVM_TILE");
  return thvm_dev_name_is(getenv("DEV"), "metal")
      && tile != NULL && tile[0] == '1';
}

static u32 bufferize_metal_tile_fanin_cap(void) {
  char const *e = getenv("THVM_METAL_FUSION_MAX_INPUTS");
  if (e != NULL && e[0] != '\0') {
    unsigned long v = strtoul(e, NULL, 10);
    if (v >= 2 && v <= 30) {
      return (u32)v;
    }
  }
  return 24;
}

static int bufferize_fanin_split_child_op(u8 op) {
  return uop_is_unary_elementwise(op) || uop_is_binary_elementwise(op)
      || uop_is_ternary_elementwise(op)
      || op == UOP_CAST    || op == UOP_BITCAST
      || op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
}

static u32 bufferize_fanin_uop_count(u64 loc, u64 boundary_root, u32 cap);

static u32 bufferize_fanin_term_count(Term t, u64 boundary_root, u32 cap) {
  t = term_resolve(t);
  if (term_tag(t) == TAG_TEN) return 1;
  if (term_tag(t) == TAG_VAR) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 1;
  return bufferize_fanin_uop_count(term_val(t), boundary_root, cap);
}

static u32 bufferize_fanin_uop_count(u64 loc, u64 boundary_root, u32 cap) {
  if (loc >= HEAP_NEXT) return 1;
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 1;
  UOpInfo *info = &BUFFERIZE_NODES[idx];
  if (loc != boundary_root && info->realized) return 1;
  if (info->op == UOP_CONST) return 0;

  u8  ar = uop_arity(info->op);
  u32 total = 0;
  u32 child_idx  [MAX_UOP_SRC] = {0};
  u32 child_count[MAX_UOP_SRC] = {0};
  for (u8 i = 0; i < ar; i++) {
    child_idx[i] = 0xFFFFFFFFu;
    Term child = term_resolve(heap_read(loc + i));
    child_count[i] = bufferize_fanin_term_count(child, boundary_root, cap);
    total += child_count[i];
    if (term_tag(child) == TAG_UOP && term_ext(child) != UOP_KERNEL) {
      child_idx[i] = bufferize_info_find(term_val(child));
    }
  }

  while (total > cap) {
    u8  best = 0xFF;
    u32 best_size = 1;
    for (u8 i = 0; i < ar; i++) {
      u32 cidx = child_idx[i];
      if (cidx == 0xFFFFFFFFu) continue;
      if (child_count[i] <= best_size) continue;
      if (!bufferize_fanin_split_child_op(BUFFERIZE_NODES[cidx].op)) continue;
      best = i;
      best_size = child_count[i];
    }
    if (best == 0xFF) break;
    UOpInfo *child_info = &BUFFERIZE_NODES[child_idx[best]];
    bufferize_node_mark(child_info, BUFFERIZE_REASON_FANIN_CAP);
    total = total - child_count[best] + 1;
    child_count[best] = 1;
  }
  return total;
}

static void bufferize_rule_metal_tile_fanin_cap(Term root) {
  if (!bufferize_metal_tile_fanin_cap_enabled()) return;
  if (term_tag(root) != TAG_UOP || term_ext(root) == UOP_KERNEL) return;
  u32 cap = bufferize_metal_tile_fanin_cap();
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (!BUFFERIZE_NODES[i].realized) continue;
    bufferize_fanin_uop_count(BUFFERIZE_NODES[i].loc, BUFFERIZE_NODES[i].loc,
                              cap);
  }
}

// MUL with both operands bound, for the matmul test.
static UPat const bufferize_upat_mul_children[2] = {
  {0, 0xFF, 0, 0, NULL},
  {0, 0xFF, 0, 1, NULL},
};
static UPat const bufferize_upat_mul = {
  UOP_MUL, 2, 0, -1, bufferize_upat_mul_children
};

// True iff the movement chain at t carries the signature of tinygrad's
// `_pool` unfold (mixin/movement.py:598): a rank-reducing RESHAPE directly
// over an EXPAND.  A conv contraction has it on one MUL operand, a Linear
// matmul does not.
static int bufferize_chain_has_pool_merge(Term t) {
  Term cur = term_resolve(t);
  for (u32 hops = 0; hops < 64; hops++) {
    if (term_tag(cur) != TAG_UOP) return 0;
    u8 op = term_ext(cur);
    if (!uop_is_movement(op)) return 0;
    u64 loc = term_val(cur);
    Term src = term_resolve(heap_read(loc + 0));
    if (op == UOP_RESHAPE && term_tag(src) == TAG_UOP
        && term_ext(src) == UOP_EXPAND) {
      Shape rs_out, ex_out;
      if (term_shape_in(cur, 0, &rs_out) && term_shape_in(src, 0, &ex_out)
          && rs_out.ndim < ex_out.ndim) {
        return 1;
      }
    }
    cur = src;
  }
  return 0;
}

// True iff the node is an EXPAND or one sits within hop_cap movement hops
// under its slot 0, as on a conv backward's `_pool` unfold, which must stay a
// view so the downstream reduce can index it.
static int bufferize_node_has_expand_in_movement_chain(u64 node_loc,
                                                       u8 node_op,
                                                       u32 hop_cap) {
  if (node_op == UOP_EXPAND) return 1;
  Term cur = term_resolve(heap_read(node_loc + 0));
  for (u32 hops = 0; hops < hop_cap; hops++) {
    if (term_tag(cur) != TAG_UOP) return 0;
    u8 op = term_ext(cur);
    if (op == UOP_EXPAND) return 1;
    if (!uop_is_movement(op)) return 0;
    cur = term_resolve(heap_read(term_val(cur) + 0));
  }
  return 0;
}

// True iff the REDUCE's output reaches a non-matmul REDUCE through movement
// and elementwise hops that cross a PAD or SHRINK: a conv-backward
// contraction feeding the `_pool` overlap-add scatter, which fuses into that
// scatter-reduce.  A Linear's GEMM crosses no PAD / SHRINK.  hop_cap bounds
// the BFS, whose entries record whether their path crossed one.
static int bufferize_reduce_feeds_scatter_reduce(u64 reduce_loc, u32 hop_cap) {
  u64 q_loc[128];
  u8  q_saw[128];
  u32 qh = 0, qt = 0;
  u64 vis[128];
  u32 nvis = 0;
  {
    u64 cons[4];
    u32 nc = bufferize_consumers_for_loc(reduce_loc, cons, 4);
    for (u32 c = 0; c < nc && qt < 128; c++) { q_loc[qt] = cons[c]; q_saw[qt] = 0; qt++; }
  }
  u32 budget = hop_cap;
  while (qh < qt && budget-- > 0) {
    u64 cur = q_loc[qh];
    u8  saw = q_saw[qh];
    qh++;
    u8 seen = 0;
    for (u32 v = 0; v < nvis; v++) if (vis[v] == cur) { seen = 1; break; }
    if (seen) continue;
    if (nvis < 128) vis[nvis++] = cur;
    u32 cidx = bufferize_info_find(cur);
    if (cidx == 0xFFFFFFFFu) continue;
    u8 walk_op = BUFFERIZE_NODES[cidx].op;
    if (walk_op == UOP_REDUCE) {
      if (saw && !(BUFFERIZE_NODES[cidx].reasons & BUFFERIZE_REASON_MATMUL))
        return 1;
      continue;  // a matmul or unscattered reduce ends this path
    }
    if (walk_op == UOP_PAD || walk_op == UOP_SHRINK) saw = 1;
    int is_movement = (walk_op == UOP_RESHAPE || walk_op == UOP_PERMUTE
                    || walk_op == UOP_EXPAND  || walk_op == UOP_SHRINK
                    || walk_op == UOP_PAD     || walk_op == UOP_FLIP);
    int is_elementwise = uop_is_unary_elementwise(walk_op)
                      || uop_is_binary_elementwise(walk_op)
                      || uop_is_ternary_elementwise(walk_op);
    if (!is_movement && !is_elementwise) continue;
    u64 next_cons[4];
    u32 nnc = bufferize_consumers_for_loc(cur, next_cons, 4);
    for (u32 c = 0; c < nnc && qt < 128; c++) {
      q_loc[qt] = next_cons[c]; q_saw[qt] = saw; qt++;
    }
  }
  return 0;
}

// True iff the contraction REDUCE reads a realized activation through a
// `_pool` unfold (an EXPAND, then a realized node, on the path).  Fused, that
// read decodes the unfold window with ranges independent of the
// contraction's kh, kw, a cross-product that hangs the reduce, so such a
// contraction realizes instead.
static int bufferize_reduce_reads_realized_pool(u64 reduce_loc, u32 hop_cap) {
  Term mul = term_resolve(heap_read(reduce_loc + 0));
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  // A DFS through elementwise and movement nodes: a relu MUL or a bias ADD
  // may sit on the path.
  Term stk[64];
  u8   se[64];  // saw_expand flag per stack entry
  u32  sp = 0;
  stk[sp] = heap_read(term_val(mul) + 0); se[sp] = 0; sp++;
  stk[sp] = heap_read(term_val(mul) + 1); se[sp] = 0; sp++;
  u32 budget = hop_cap * 8;
  while (sp > 0 && budget-- > 0) {
    Term cur = term_resolve(stk[--sp]);
    u8 saw_expand = se[sp];
    if (term_tag(cur) != TAG_UOP) continue;
    u8 op = term_ext(cur);
    if (op == UOP_KERNEL) continue;
    u32 cidx = bufferize_info_find(term_val(cur));
    int rz = (cidx != 0xFFFFFFFFu && BUFFERIZE_NODES[cidx].realized);
    if (rz && saw_expand && op != UOP_EXPAND) return 1;
    if (op == UOP_EXPAND) saw_expand = 1;
    // A realized node before any EXPAND is another kernel's output.
    if (rz && !saw_expand) continue;
    int is_movement = uop_is_movement(op);
    int is_ew = uop_is_unary_elementwise(op)
             || uop_is_binary_elementwise(op)
             || uop_is_ternary_elementwise(op);
    if (!is_movement && !is_ew) continue;
    u8 ar = uop_arity(op);
    for (u8 s = 0; s < ar && sp < 64; s++) {
      stk[sp] = heap_read(term_val(cur) + s); se[sp] = saw_expand; sp++;
    }
  }
  return 0;
}

// True iff the REDUCE at reduce_loc is a matmul to keep realized for the GEMM
// dispatch: a REDUCE of MUL(a, b) with a != b, unless it is a conv contraction.
static int bufferize_uop_is_matmul(u64 reduce_loc) {
  Term mul = term_resolve(heap_read(reduce_loc + 0));
  Term bindings[UPAT_NUM_BINDINGS] = {0};
  int is_mul   = upat_match(&bufferize_upat_mul, mul, bindings);
  int distinct = is_mul && (term_val(bindings[0]) != term_val(bindings[1]));
  // A conv-backward contraction, whose operands lack the unfold signature,
  // fuses into its scatter-reduce.
  if (is_mul && distinct
      && bufferize_reduce_feeds_scatter_reduce(reduce_loc, 32)) {
    return 0;
  }
  // A forward conv contraction fuses as a reduce over the strided unfold
  // view on every backend, as in tinygrad: a contiguous im2col would be the
  // dominant peak-memory cost.
  if (is_mul && distinct
      && (bufferize_chain_has_pool_merge(bindings[0])
          || bufferize_chain_has_pool_merge(bindings[1]))) {
    return 0;
  }

  return is_mul && distinct;
}

// True iff the node's sources reach a REDUCE through elementwise and movement
// ops before any realized node: a reduce epilogue, which tinygrad recomputes
// in each consumer rather than realizing.
static int bufferize_elementwise_src_has_reduce(u64 loc, u32 depth) {
  if (depth > 32) return 0;
  if (loc >= HEAP_NEXT) return 0;
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u8 op = BUFFERIZE_NODES[idx].op;
  u8 ar = uop_arity(op);
  for (u8 s = 0; s < ar; s++) {
    Term ch = term_resolve(heap_read(loc + s));
    if (term_tag(ch) != TAG_UOP) continue;
    u8 cop = term_ext(ch);
    if (cop == UOP_KERNEL) continue;
    u64 cloc = term_val(ch);
    if (cop == UOP_REDUCE) return 1;
    // A realized child is another kernel's output.
    u32 cidx = bufferize_info_find(cloc);
    if (cidx != 0xFFFFFFFFu && BUFFERIZE_NODES[cidx].realized) continue;
    int child_ew = uop_is_unary_elementwise(cop)
                || uop_is_binary_elementwise(cop)
                || uop_is_ternary_elementwise(cop);
    if (child_ew || uop_is_movement(cop)) {
      if (bufferize_elementwise_src_has_reduce(cloc, depth + 1)) return 1;
    }
  }
  return 0;
}

// The first compute node under loc's movement chain (the activation a window
// view reads), or 0 at a leaf, KERNEL or TEN.  Realizing it, not the view,
// lets every view (the maxpool MAX and its backward CMPEQ) index one buffer,
// so argmax ties stay bit-exact.
static u64 bufferize_movement_compute_source(u64 loc, u32 depth) {
  if (depth > 16 || loc >= HEAP_NEXT) return 0;
  Term src = term_resolve(heap_read(loc + 0));
  if (term_tag(src) != TAG_UOP) return 0;
  u8 sop = term_ext(src);
  if (sop == UOP_KERNEL) return 0;
  u64 sloc = term_val(src);
  if (uop_is_movement(sop)) return bufferize_movement_compute_source(sloc, depth + 1);
  return sloc;
}

// Realize what an indexed op's storage operand `t` reads (see
// BUFFERIZE_REASON_INDEXED_SRC): nothing when the operand is structural,
// else the compute source under its view chain -- never the view itself, as
// the MAXPOOL_INPUT seed -- or, when the chain holds a PAD, the operand node.
static void bufferize_seed_indexed_operand(Term t) {
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP || uop_indexed_operand_is_structural(r)) {
    return;
  }
  u64 target = term_val(r);
  int has_pad = 0;
  for (Term v = r; term_tag(v) == TAG_UOP && uop_is_movement(term_ext(v));
       v = term_resolve(heap_read(term_val(v)))) {
    has_pad |= (term_ext(v) == UOP_PAD);
  }
  if (!has_pad && uop_is_movement(term_ext(r))) {
    u64 src = bufferize_movement_compute_source(target, 0);
    if (src != 0) {
      target = src;
    }
  }
  u32 idx = bufferize_info_find(target);
  if (idx == 0xFFFFFFFFu) {
    return;
  }
  bufferize_node_mark(&BUFFERIZE_NODES[idx], BUFFERIZE_REASON_MULTI);
  bufferize_node_mark(&BUFFERIZE_NODES[idx], BUFFERIZE_REASON_INDEXED_SRC);
}

// A GATHER reads slot 0 as storage; a SCATTER is its own kernel and reads
// slots 1 and 2 as storage.  Runs after every rule that can unmark, so no
// later rule drops these correctness realizes.
static void bufferize_seed_indexed_operands(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->op == UOP_GATHER) {
      bufferize_seed_indexed_operand(heap_read(info->loc + 0));
    } else if (info->op == UOP_SCATTER) {
      bufferize_node_mark(info, BUFFERIZE_REASON_MULTI);
      bufferize_node_mark(info, BUFFERIZE_REASON_SCATTER);
      bufferize_seed_indexed_operand(heap_read(info->loc + 1));
      bufferize_seed_indexed_operand(heap_read(info->loc + 2));
    }
  }
}

// True iff t is a REDUCE_MAX behind movement ops: the lift(MAX(a)) operand of
// the maxpool backward mask CMPEQ(a, lift(MAX(a))) (tinygrad
// mixin/gradient.py:10-13).
static int bufferize_term_is_reduce_max(Term t, u32 depth) {
  if (depth > 16) return 0;
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  u8 op = term_ext(t);
  if (op == UOP_KERNEL) return 0;
  u64 tloc = term_val(t);
  if (op == UOP_REDUCE)
    return (u32)term_val(heap_read(tloc + 1)) == REDUCE_MAX;
  if (uop_is_movement(op))
    return bufferize_term_is_reduce_max(heap_read(tloc + 0), depth + 1);
  return 0;
}

// True iff the node is the maxpool backward argmax mask CMPEQ(a, lift(MAX(a))).
// It realizes so its SUM count and RECIP get their own kernel: fused into an
// N=1 conv-weight backward reduce, the count is read over the size-1 axis as 0.
static int bufferize_node_is_maxpool_mask(u64 loc, u8 op) {
  if (op != UOP_CMPEQ) return 0;
  if (bufferize_term_is_reduce_max(heap_read(loc + 0), 0)) return 1;
  if (bufferize_term_is_reduce_max(heap_read(loc + 1), 0)) return 1;
  return 0;
}

// True iff t is the maxpool argmax mask behind movement ops.
static int bufferize_term_is_maxpool_mask(Term t, u32 depth) {
  if (depth > 16) return 0;
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  u8 op = term_ext(t);
  if (op == UOP_KERNEL) return 0;
  u64 tloc = term_val(t);
  if (op == UOP_CMPEQ) return bufferize_node_is_maxpool_mask(tloc, op);
  if (uop_is_movement(op))
    return bufferize_term_is_maxpool_mask(heap_read(tloc + 0), depth + 1);
  return 0;
}

// True iff the node is the normalized mask MUL(mask, RECIP(SUM(mask))) of the
// maxpool gradient (tinygrad mixin/gradient.py:10-13), which realizes for the
// same reason as the mask.
static int bufferize_node_is_maxpool_mask_norm(u64 loc, u8 op) {
  if (op != UOP_MUL) return 0;
  Term a = term_resolve(heap_read(loc + 0));
  Term b = term_resolve(heap_read(loc + 1));
  int a_mask = bufferize_term_is_maxpool_mask(a, 0);
  int b_mask = bufferize_term_is_maxpool_mask(b, 0);
  if (!a_mask && !b_mask) return 0;
  // The other operand is the broadcast 1/count, RECIP(SUM(...)).
  Term other = a_mask ? b : a;
  other = term_resolve(other);
  while (term_tag(other) == TAG_UOP && uop_is_movement(term_ext(other)))
    other = term_resolve(heap_read(term_val(other) + 0));
  if (term_tag(other) != TAG_UOP || term_ext(other) != UOP_RECIP) return 0;
  Term rsrc = term_resolve(heap_read(term_val(other) + 0));
  while (term_tag(rsrc) == TAG_UOP && uop_is_movement(term_ext(rsrc)))
    rsrc = term_resolve(heap_read(term_val(rsrc) + 0));
  return term_tag(rsrc) == TAG_UOP && term_ext(rsrc) == UOP_REDUCE
      && (u32)term_val(heap_read(term_val(rsrc) + 1)) == REDUCE_SUM;
}

// Accept and decline counts of bufferize_fp8_matmul_tiled_eligible, printed by
// thvm_fp8_elig_dump (the Metal memory census calls it).
static u64 FP8_ELIG_ACCEPT      = 0;   // eligible -> lazy in-tile (no transient)
static u64 FP8_ELIG_DECL_NAXES  = 0;   // reduce n_axes != 1
static u64 FP8_ELIG_DECL_NOTMUL = 0;   // reduce src not MUL
static u64 FP8_ELIG_DECL_NOTEXP = 0;   // MUL operand 0 not a bare EXPAND
static u64 FP8_ELIG_DECL_NDIM   = 0;   // EXPAND ndim != 3 (reshapes/bias EXPAND)
static u64 FP8_ELIG_DECL_RAGGED = 0;   // M/N/K not all %8
static u64 FP8_ELIG_DECL_PICK   = 0;   // tile picker declined
static u32 FP8_ELIG_LAST_NDIM   = 0;   // last-seen EXPAND ndim (for a decline)
static u32 FP8_ELIG_LAST_OP     = 0;   // last-seen non-EXPAND operand op

void thvm_fp8_elig_dump(void) {
  fprintf(stderr,
    "[fp8elig] ACCEPT(lazy)=%llu | DECLINE(transient): naxes=%llu notmul=%llu "
    "notexpand=%llu(lastop=%u) ndim!=3=%llu(lastndim=%u) ragged=%llu pick=%llu\n",
    (unsigned long long)FP8_ELIG_ACCEPT, (unsigned long long)FP8_ELIG_DECL_NAXES,
    (unsigned long long)FP8_ELIG_DECL_NOTMUL,
    (unsigned long long)FP8_ELIG_DECL_NOTEXP, FP8_ELIG_LAST_OP,
    (unsigned long long)FP8_ELIG_DECL_NDIM, FP8_ELIG_LAST_NDIM,
    (unsigned long long)FP8_ELIG_DECL_RAGGED, (unsigned long long)FP8_ELIG_DECL_PICK);
}

// True iff the matmul REDUCE at reduce_loc takes Metal's tiled tensor-core path
// (rmu_emit_matmul_tc), which decodes an fp8 B operand in-tile: a non-batched
// GEMM with M, N and K all multiples of 8 that rmu_tc_pick_tile accepts.  Its
// fp8 weight CAST then stays lazy, with no bf16 transient.
static int bufferize_fp8_matmul_tiled_eligible(u64 reduce_loc) {
  if (CURRENT_BACKEND != &METAL_BACKEND) return 0;
  {
    static int known = 0, on = 1;
    if (!known) {
      char const *e = getenv("THVM_FP8_IN_TILE_LAZY");
      on = !(e != NULL && e[0] == '0');   // default ON; =0 reverts to transient
      known = 1;
    }
    if (!on) return 0;
  }
  // Before the lift the MUL's operands are EXPANDs to the {M,N,K} space: K is
  // the reduce axis, M and N the other two dims.
  Term reduce = term_new(0, TAG_UOP, UOP_REDUCE, reduce_loc);
  if (uop_reduce_n_axes(reduce) != 1) { FP8_ELIG_DECL_NAXES++; return 0; }
  u32 red_axis = uop_reduce_axis(reduce, 0);
  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) {
    FP8_ELIG_DECL_NOTMUL++; return 0;
  }
  Term opnd = heap_read(term_val(mul) + 0);
  if (term_tag(opnd) != TAG_UOP || term_ext(opnd) != UOP_EXPAND) {
    FP8_ELIG_LAST_OP = (term_tag(opnd) == TAG_UOP) ? term_ext(opnd) : 999;
    FP8_ELIG_DECL_NOTEXP++; return 0;
  }
  u32 ndim = (u32)term_val(heap_read(term_val(opnd) + 1));
  if (ndim != 3 || red_axis >= ndim) {   // batched/collapsed: transient
    FP8_ELIG_LAST_NDIM = ndim; FP8_ELIG_DECL_NDIM++; return 0;
  }
  u32 dims[3];
  for (u32 d = 0; d < 3; d++) dims[d] = (u32)term_val(heap_read(term_val(opnd) + 2 + d));
  u32 k_ext = dims[red_axis];
  u32 mn[2], mni = 0;
  for (u32 d = 0; d < 3; d++) if (d != red_axis) mn[mni++] = dims[d];
  u32 m_ext = mn[0], n_ext = mn[1];
  if (m_ext == 0 || n_ext == 0 || k_ext == 0) { FP8_ELIG_DECL_RAGGED++; return 0; }
  if ((m_ext % 8u) != 0 || (n_ext % 8u) != 0 || (k_ext % 8u) != 0) {
    FP8_ELIG_DECL_RAGGED++; return 0;
  }
  RmuTcTile tile;
  if (rmu_tc_pick_tile(m_ext, n_ext, k_ext, 0u, &tile)) { FP8_ELIG_ACCEPT++; return 1; }
  FP8_ELIG_DECL_PICK++;
  return 0;
}

// True iff the node is a CAST or BITCAST of an fp8 value that reaches a matmul
// MUL through movement and cast hops.  Fused, the matmul would decode each fp8
// weight once per output row; realized into a transient bf16 buffer, each
// element is decoded once.
static int bufferize_fp8_cast_feeds_matmul(UOpInfo const *info) {
  static int known = 0, enabled = 1;
  if (!known) {
    char const *e = getenv("THVM_NO_FP8_CAST_REALIZE");
    enabled = !(e != NULL && e[0] == '1');   // default ON; opt out for A/B
    known = 1;
  }
  if (!enabled) return 0;
  if (info->op != UOP_CAST && info->op != UOP_BITCAST) return 0;
  // The source is the resident fp8 weight.
  u32 src_dt = DT_FP32;
  if (!term_dtype_in(heap_read(info->loc + 0), 0, &src_dt)) return 0;
  if (src_dt != DT_FP8E4M3 && src_dt != DT_FP8E5M2) return 0;
  // BFS the consumers through movement and cast hops to a matmul's MUL.
  u64 queue[64]; u32 qh = 0, qt = 0;
  queue[qt++] = info->loc;
  u32 budget = 64;
  while (qh < qt && budget-- > 0) {
    u64 cur = queue[qh++];
    u64 cons[8];
    u32 nc = bufferize_consumers_for_loc(cur, cons, 8);
    for (u32 c = 0; c < nc; c++) {
      Term ct = term_resolve(term_new(0, TAG_UOP, 0, cons[c]));
      u32 cidx = bufferize_info_find(cons[c]);
      if (cidx == 0xFFFFFFFFu) continue;
      u8 cop = BUFFERIZE_NODES[cidx].op;
      (void)ct;
      if (cop == UOP_MUL) {
        u64 mcons[8];
        u32 mnc = bufferize_consumers_for_loc(cons[c], mcons, 8);
        for (u32 m = 0; m < mnc; m++) {
          u32 midx = bufferize_info_find(mcons[m]);
          if (midx != 0xFFFFFFFFu && BUFFERIZE_NODES[midx].op == UOP_REDUCE
              && bufferize_uop_is_matmul(mcons[m])) {
            // A tiled tensor-core matmul decodes the fp8 B tile once for all
            // its rows, so the cast stays lazy there.
            if (bufferize_fp8_matmul_tiled_eligible(mcons[m])) return 0;
            return 1;
          }
        }
        continue;
      }
      if (uop_is_movement(cop) || cop == UOP_CAST || cop == UOP_BITCAST) {
        if (qt < 64) queue[qt++] = cons[c];
      }
    }
  }
  return 0;
}

// True iff t derives from a REDUCE through movement, unary and ADD / MUL hops
// (either operand), as the RMSNorm normalizer RECIP(SQRT(REDUCE(x*x)/d + eps))
// does.
static int bufferize_derives_from_reduce(Term t, u32 depth) {
  if (depth > 24 || term_tag(t) != TAG_UOP) return 0;
  u8 op = term_ext(t);
  if (op == UOP_REDUCE) return 1;
  if (uop_is_movement(op) || uop_is_unary_elementwise(op)
      || op == UOP_RECIP || op == UOP_SQRT || op == UOP_NEG) {
    return bufferize_derives_from_reduce(
        term_resolve(heap_read(term_val(t) + 0)), depth + 1);
  }
  if (op == UOP_ADD || op == UOP_MUL) {
    return bufferize_derives_from_reduce(
               term_resolve(heap_read(term_val(t) + 0)), depth + 1)
        || bufferize_derives_from_reduce(
               term_resolve(heap_read(term_val(t) + 1)), depth + 1);
  }
  return 0;
}

// True iff t's subtree holds an EXPAND of a value derived from a REDUCE: the
// RMSNorm broadcast-back EXPAND(RESHAPE(RECIP(SQRT(REDUCE(x*x)/d + eps))))
// (NN/Norm.wl TRMSNorm).  Fused into a batched matmul reduce on Metal, its
// per-row normalizer is indexed wrong through the matmul's rank-grow EXPAND.
static int bufferize_subtree_has_reduce_broadcast(Term t, u32 depth) {
  if (depth > 24 || term_tag(t) != TAG_UOP) return 0;
  u8 op = term_ext(t);
  if (op == UOP_EXPAND
      && bufferize_derives_from_reduce(
             term_resolve(heap_read(term_val(t) + 0)), 0)) {
    return 1;
  }
  if (uop_is_movement(op) || uop_is_unary_elementwise(op)
      || op == UOP_RECIP || op == UOP_SQRT || op == UOP_NEG) {
    return bufferize_subtree_has_reduce_broadcast(
        term_resolve(heap_read(term_val(t) + 0)), depth + 1);
  }
  if (op == UOP_ADD || op == UOP_MUL) {
    return bufferize_subtree_has_reduce_broadcast(
               term_resolve(heap_read(term_val(t) + 0)), depth + 1)
        || bufferize_subtree_has_reduce_broadcast(
               term_resolve(heap_read(term_val(t) + 1)), depth + 1);
  }
  return 0;
}

// THVM_NO_RMSNORM_MATMUL_REALIZE=1 turns the RMSNorm realize off.
static int bufferize_rmsnorm_matmul_realize_enabled(void) {
  static int known = 0, enabled = 1;
  if (!known) {
    char const *e = getenv("THVM_NO_RMSNORM_MATMUL_REALIZE");
    enabled = !(e != NULL && e[0] == '1');
    known = 1;
  }
  return enabled;
}

// True iff the operand, behind movement and cast ops, is not a CONST (an
// EXPAND of a CONST scale is no contraction operand).
static int bufferize_operand_is_nonconst(Term operand, u32 depth) {
  if (depth > 24 || term_tag(operand) != TAG_UOP) return 0;
  u8 op = term_ext(operand);
  if (op == UOP_CONST) return 0;
  if (uop_is_movement(op) || op == UOP_CAST || op == UOP_BITCAST) {
    return bufferize_operand_is_nonconst(
        term_resolve(heap_read(term_val(operand) + 0)), depth + 1);
  }
  return 1;
}

// True iff the REDUCE's source is a MUL of two distinct non-const operands (a
// matmul or batched matmul: q@k, @V, x@W).  It looks at the MUL alone, not at
// bufferize_uop_is_matmul's test.
static int bufferize_reduce_is_contraction(u64 reduce_loc) {
  Term mul = term_resolve(heap_read(reduce_loc + 0));
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term a = term_resolve(heap_read(term_val(mul) + 0));
  Term b = term_resolve(heap_read(term_val(mul) + 1));
  if (term_val(a) == term_val(b)) return 0;           // x*x, the RMSNorm mean
  return bufferize_operand_is_nonconst(a, 0) && bufferize_operand_is_nonconst(b, 0);
}

// The loc of the first non-movement node under a contraction operand (the
// matmul input: the RMSNorm output, or a rope of it) when its subtree holds the
// RMSNorm reduce-broadcast; else 0.
static u64 bufferize_rmsnorm_under_operand(Term operand, u32 depth) {
  if (depth > 24 || term_tag(operand) != TAG_UOP) return 0;
  u8 op = term_ext(operand);
  if (uop_is_movement(op) || op == UOP_CAST || op == UOP_BITCAST) {
    return bufferize_rmsnorm_under_operand(
        term_resolve(heap_read(term_val(operand) + 0)), depth + 1);
  }
  if (bufferize_subtree_has_reduce_broadcast(operand, 0)) return term_val(operand);
  return 0;
}

// For a contraction REDUCE, realizes each operand that is an RMSNorm output,
// so the norm is its own kernel and cannot mis-stage on Metal (see
// bufferize_subtree_has_reduce_broadcast), as tinygrad gives a norm feeding a
// matmul its own kernel.  Returns the number of boundaries marked.
static u32 bufferize_rmsnorm_feeds_matmul(UOpInfo const *info) {
  if (!bufferize_rmsnorm_matmul_realize_enabled()) return 0;
  if (info->op != UOP_REDUCE) return 0;
  if (!bufferize_reduce_is_contraction(info->loc)) return 0;
  Term mul = term_resolve(heap_read(info->loc + 0));
  u32 marked = 0;
  for (u32 i = 0; i < 2; i++) {
    u64 rms = bufferize_rmsnorm_under_operand(
        term_resolve(heap_read(term_val(mul) + i)), 0);
    if (rms != 0) {
      u32 ridx = bufferize_info_find(rms);
      if (ridx != 0xFFFFFFFFu) {
        bufferize_node_mark(&BUFFERIZE_NODES[ridx], BUFFERIZE_REASON_REDUCE);
        marked++;
      }
    }
  }
  return marked;
}

// No backend computes a packed int4 / uint4 buffer (docs/NN/dtypes.md): the C
// renderer would type it as float and write past its nibbles.
static void bufferize_reject_packed(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo const *info = &BUFFERIZE_NODES[i];
    u32 dt = 0;
    if (info->realized
        && term_dtype_in(term_new(0, TAG_UOP, info->op, info->loc), 0, &dt)
        && dtype_is_packed(dt)) {
      char msg[96];
      snprintf(msg, sizeof msg, "bufferize: no kernel computes a buffer of dtype %s",
               dtype_name(dt));
      thvm_fatal(msg);
    }
  }
}

static int bufferize_reads_no_buffer_rec(Term t, u32 *budget) {
  if (term_tag(t) == TAG_NUM) return 1;
  if (term_tag(t) != TAG_UOP || *budget == 0) return 0;
  --*budget;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_KERNEL: case UOP_BUFFERIZE: case UOP_BUFFER: case UOP_DEFINE_VAR:
    case UOP_ASSIGN: case UOP_GATHER:    case UOP_SCATTER: case UOP_COPY:
    case UOP_LOAD:   case UOP_INDEX_E:   case UOP_STORE:
      return 0;
  }
  u64 loc = term_val(t);
  for (u8 i = 0; i < uop_arity((u8)op); i++) {
    if (!bufferize_reads_no_buffer_rec(term_resolve(heap_read(loc + i)), budget)) return 0;
  }
  return 1;
}

// 1 iff the node at `loc` reads no buffer: every leaf is a constant, so it is
// index math (an arange's running sum).  A walk past 1024 nodes of a shared
// DAG reads as a buffer.
fn int bufferize_reads_no_buffer(u64 loc, u8 op) {
  u32 budget = 1024;
  return bufferize_reads_no_buffer_rec(term_new(0, TAG_UOP, op, loc), &budget);
}

// tinygrad keeps a bufferize only while a reduce under it reads a buffer
// (remove_bufferize, schedule/rangeify.py:281-288), so a reduce over constants,
// such as an arange's running sum, fuses into each consumer, where
// pm_reduce_simplify folds it to index math.  A node seeded only as a REDUCE or
// a multi-consumer one that reads no buffer stays unrealized for the
// per-kernel fold (uop_reduce_arange_collapse); run_rangeify_unified skips
// its ending-range realize too.
static void bufferize_rule_bufferless(Term root) {
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (!info->realized || info->loc == term_val(root)) continue;
    if (info->reasons & ~(BUFFERIZE_REASON_MULTI | BUFFERIZE_REASON_REDUCE | BUFFERIZE_REASON_INLINE)) continue;
    if (bufferize_reads_no_buffer(info->loc, info->op)) {
      bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
    }
  }
}

// The schedule entry point: walk the DAG at root, seed the boundaries, run
// the unmark rules, then the unified rangeify pass.
fn void bufferize_classify(Term root) {
  bufferize_info_clear();
  if (term_tag(root) != TAG_UOP || term_ext(root) == UOP_KERNEL) {
    return;
  }

  // The visited bitmap spans HEAP_NEXT, not the heap capacity, so its cost
  // tracks live work.
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;
  bufferize_walk_rec(root, visited);
  free(visited);

  u32 root_idx = bufferize_info_find(term_val(root));
  if (root_idx != 0xFFFFFFFFu) {
    bufferize_node_mark(&BUFFERIZE_NODES[root_idx], BUFFERIZE_REASON_ROOT);
  }

    // Seeds.  tinygrad's pm_generate_realize_map realizes only STOREs and a
    // few srcs (schedule/indexing.py:45-54) and derives the rest in
    // run_rangeify; thvm seeds more, and ru_seed_boundary_holds
    // (rangeify_unified.c) names the seeds the unified walk starts from.
    for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
      UOpInfo *info = &BUFFERIZE_NODES[i];
      if (bufferize_fp8_cast_feeds_matmul(info)) {
        // The cast realizes into a single-use bf16 transient; FP8_TRANSIENT
        // has emit record its tid so the realize frees it, else
        // mark_heap_rooted_preserve would pin it for good.
        bufferize_node_mark(info, BUFFERIZE_REASON_REDUCE | BUFFERIZE_REASON_FP8_TRANSIENT);
      }
      // For a contraction REDUCE, its RMSNorm operands realize.
      (void)bufferize_rmsnorm_feeds_matmul(info);
      if (info->consumer_count >= 2) {
        // MULTI, except for a movement op or a reduce epilogue with an EXPAND
        // on it or under its slot 0: realized, that broadcast (the `_pool`
        // unfold) materializes the whole expanded tensor, while tinygrad keeps
        // it a view and recomputes the epilogue per consumer.  Other shared
        // nodes realize, so a backward `_pool` reads a buffer, not a recompute.
        int ew = uop_is_unary_elementwise(info->op)
              || uop_is_binary_elementwise(info->op)
              || uop_is_ternary_elementwise(info->op);
        int src_has_reduce = ew
            && bufferize_elementwise_src_has_reduce(info->loc, 0);
        int skip_movement = uop_is_movement(info->op);
        int fuse_realize_boundary =
            !bufferize_node_has_expand_in_movement_chain(info->loc, info->op, 6);
        if (fuse_realize_boundary
            || (!(ew && src_has_reduce) && !skip_movement)) {
          bufferize_node_mark(info, BUFFERIZE_REASON_MULTI);
        }
      }
      // Each REDUCE_MAX realizes the activation under its window view: the
      // forward max and the backward argmax mask both read it, and two fused
      // recomputes can disagree at a tie (count 0, then RECIP(0) is NaN).
      // MAXPOOL_INPUT makes the unified walk keep it.
      if (info->op == UOP_REDUCE
          && (u32)term_val(heap_read(info->loc + 1)) == REDUCE_MAX) {
        Term wsrc = term_resolve(heap_read(info->loc + 0));
        if (term_tag(wsrc) == TAG_UOP && term_ext(wsrc) != UOP_KERNEL) {
          u64 wloc = term_val(wsrc);
          u64 act_loc = uop_is_movement(term_ext(wsrc))
              ? bufferize_movement_compute_source(wloc, 0)
              : wloc;
          if (act_loc != 0) {
            u32 aidx = bufferize_info_find(act_loc);
            if (aidx != 0xFFFFFFFFu) {
              bufferize_node_mark(&BUFFERIZE_NODES[aidx], BUFFERIZE_REASON_MULTI);
              bufferize_node_mark(&BUFFERIZE_NODES[aidx],
                                  BUFFERIZE_REASON_MAXPOOL_INPUT);
            }
          }
        }
      }
      if (info->op == UOP_REDUCE) {
        // A matmul REDUCE realizes for good (BUFFERIZE_REASON_MATMUL); any
        // other gets a seed the unmark rules below may drop.
        if (bufferize_uop_is_matmul(info->loc)) {
          bufferize_node_mark(info, BUFFERIZE_REASON_REDUCE);
          bufferize_node_mark(info, BUFFERIZE_REASON_MATMUL);
        } else {
          // A conv-backward contraction gets no seed and fuses into its
          // scatter-reduce, unless it reads a realized activation through
          // the unfold (bufferize_reduce_reads_realized_pool).
          int skip_seed = 0;
          if (!skip_seed
              && bufferize_reduce_feeds_scatter_reduce(info->loc, 32)) {
            if (!bufferize_reduce_reads_realized_pool(info->loc, 12))
              skip_seed = 1;
          }
          if (!skip_seed) {
            bufferize_node_mark(info, BUFFERIZE_REASON_REDUCE);
          }
        }
      }
      // The maxpool argmax mask and its normalized form realize whatever
      // their consumer count (bufferize_node_is_maxpool_mask); MAXPOOL_MASK
      // makes the unified walk keep them.
      if (bufferize_node_is_maxpool_mask(info->loc, info->op)
          || bufferize_node_is_maxpool_mask_norm(info->loc, info->op)) {
        bufferize_node_mark(info, BUFFERIZE_REASON_MULTI);
        bufferize_node_mark(info, BUFFERIZE_REASON_MAXPOOL_MASK);
      }
    }
    // Softmax-style unmark: a REDUCE every consumer chain of which broadcasts
    // it back (bufferize_reduce_consumer_is_broadcast_chain) fuses inline.
    // tinygrad seeds no REDUCE; a reduce realizes there only where ranges end
    // (schedule/indexing.py:270-277).
    for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
      UOpInfo *info = &BUFFERIZE_NODES[i];
      if (info->op != UOP_REDUCE) continue;
      if (!info->realized) continue;
      if (info->reasons & BUFFERIZE_REASON_MATMUL) continue;
      if (!bufferize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
      bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
    }
    // Reduce-chain unmark: a REDUCE whose only consumer is a same-kind REDUCE
    // of a realized chain fuses into it.
    for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
      UOpInfo *info = &BUFFERIZE_NODES[i];
      if (info->op != UOP_REDUCE) continue;
      if (!info->realized) continue;
      if (info->reasons & BUFFERIZE_REASON_MATMUL) continue;
      if (info->consumer_count != 1) continue;
      if (split_reduceop_keeps(info->loc)) continue;
      u64 consumer_locs[1];
      u32 n_cons = bufferize_consumers_for_loc(info->loc, consumer_locs, 1);
      if (n_cons != 1) continue;
      u32 cidx = bufferize_info_find(consumer_locs[0]);
      if (cidx == 0xFFFFFFFFu) continue;
      if (BUFFERIZE_NODES[cidx].op != UOP_REDUCE) continue;
      // The consumer may be a mid-chain reduce this loop already unmarked.
      if (!BUFFERIZE_NODES[cidx].realized
          && !bufferize_reduce_consumer_in_realized_chain(consumer_locs[0], 16))
        continue;
      // The outer REDUCE must read this one, possibly behind fired DP cells,
      // which the CPU walker resolves (term_resolve in uwalk_eval_*).
      u64 visited[8];
      u32 n_visited = 0;
      Term outer_src = bufferize_unwrap_dp(heap_read(consumer_locs[0] + 0),
                                            visited, &n_visited, 8, 16);
      if (outer_src == 0 || term_tag(outer_src) != TAG_UOP) continue;
      if (term_val(outer_src) != info->loc) continue;
      // Only a same-kind chain fuses: inlined into a SUM, a MAX loses its own
      // accumulator.  tinygrad fuses no such chain, as it keeps every reduce
      // that reads a buffer realized (remove_bufferize,
      // schedule/rangeify.py:87-96).
      u32 inner_kind = (u32)term_val(heap_read(info->loc + 1));
      u32 outer_kind = (u32)term_val(heap_read(consumer_locs[0] + 1));
      if (inner_kind != outer_kind) continue;
      bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
    }
    // Reduce-epilogue unmark: a non-matmul REDUCE whose only consumer is a
    // realized epilogue of the same shape fuses into it, saving a store, a
    // reload and a launch (bufferize_is_scalar_preserving_epilogue).
    for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
      UOpInfo *info = &BUFFERIZE_NODES[i];
      if (info->op != UOP_REDUCE) continue;
      if (!info->realized) continue;
      if (info->reasons & BUFFERIZE_REASON_MATMUL) continue;
      if (info->consumer_count != 1) continue;
      u64 consumer_locs[1];
      u32 n_cons = bufferize_consumers_for_loc(info->loc, consumer_locs, 1);
      if (n_cons != 1) continue;
      u32 cidx = bufferize_info_find(consumer_locs[0]);
      if (cidx == 0xFFFFFFFFu) continue;
      if (!BUFFERIZE_NODES[cidx].realized) continue;
      if (!bufferize_is_scalar_preserving_epilogue(consumer_locs[0],
                                                    info->loc))
        continue;
      Shape rsh = {0};
      Shape csh = {0};
      Term rt = term_new(0, TAG_UOP, info->op, info->loc);
      Term ct = term_new(0, TAG_UOP, BUFFERIZE_NODES[cidx].op,
                         consumer_locs[0]);
      if (!term_shape_in(rt, 0, &rsh) || !term_shape_in(ct, 0, &csh))
        continue;
      if (rsh.ndim != csh.ndim) continue;
      int same = 1;
      for (u32 d = 0; d < rsh.ndim; d++)
        if (rsh.dims[d] != csh.dims[d]) { same = 0; break; }
      if (!same) continue;
      bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
    }
    bufferize_rule_bufferless(root);
    // After the unmark rules, so the fan-in split sees the final boundaries.
    bufferize_rule_metal_tile_fanin_cap(root);
    bufferize_seed_indexed_operands();
    bufferize_reject_packed();
    // The unified walk writes RU_RANGE_MAP / RU_REALIZE_MAP and the
    // main-heap UOP_BUFFERIZE Terms (one per realize boundary).
    run_rangeify_unified(root);
    return;
}
