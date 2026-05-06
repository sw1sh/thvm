// schedule/bufferize_classify.c - decide which UOPs in a graph
//                                  must "realize" into a backing
//                                  buffer (a tinygrad CONTIGUOUS).
//                                  This is the seed/walk pass of
//                                  the bufferize schedule IR.
//
// A UOp is first seeded as a boundary when ANY of:
//   (a) it is the root the caller asked for,
//   (b) it has 2+ distinct UOp parents (multi-consumer),
//   (c) it is a REDUCE (REDUCE outputs always escape into a
//       buffer; one reduce per kernel),
//   (d) it is a movement op whose source can't be aliased.
//
// Named realize-map rewrites then clear boundaries that are legal to
// recompute or inline.  This deliberately mirrors tinygrad's
// BUFFERIZE/INDEX schedule-rewrite model: seed conservatively, then
// remove bufferization with explicit legality/cost rules.
//
// Output: a small table indexed by the UOp's heap loc; the
// selective materializer consults bufferize_is_realized to decide
// whether to emit a UOP_KERNEL or inline the compute into its
// consumer kernel's program.
//
// Note: this pass DOES NOT mutate the heap or the kernel
// table.  It only reads the UOp DAG and populates the table.
// Aliasing of duplicate child references inside a single
// parent (e.g., MUL[x, x]) counts as ONE consumer of x, since
// the materializer dedups.

// BUFFERIZE_NODES_CAP + UOpInfo struct + BUFFERIZE_NODES/BUFFERIZE_NODES_LEN
// declared in thvm.h so materialize.c can iterate the table.

UOpInfo BUFFERIZE_NODES    [BUFFERIZE_NODES_CAP];
u32     BUFFERIZE_NODES_LEN = 0;

// Open-addressed hash table mapping loc -> BUFFERIZE_NODES index.
// Without this, bufferize_info_find did a linear scan of BUFFERIZE_NODES,
// which made bufferize_classify O(N^2) for the N UOPs in a recursive
// training-loop graph and the dominant cost of long bound-w realizes.
// Cap is the next power of two >= BUFFERIZE_NODES_CAP for cheap masking.
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
  // Hash table full -- silently drop; lookups for this loc will
  // miss but caller has the linear cap to fall back to.
}

static u32 bufferize_node_get_or_add(u64 loc, u8 op) {
  u32 idx = bufferize_info_find(loc);
  if (idx != 0xFFFFFFFFu) return idx;
  if (BUFFERIZE_NODES_LEN >= BUFFERIZE_NODES_CAP) return 0xFFFFFFFFu;
  idx = BUFFERIZE_NODES_LEN++;
  BUFFERIZE_NODES[idx].loc            = loc;
  BUFFERIZE_NODES[idx].consumer_count = 0;
  BUFFERIZE_NODES[idx].reasons        = 0;
  BUFFERIZE_NODES[idx].op             = op;
  BUFFERIZE_NODES[idx].realized       = 0;
  bufferize_node_hash_insert(loc, idx);
  return idx;
}

static void bufferize_node_mark(UOpInfo *info, u32 reason) {
  if (info == NULL) {
    return;
  }
  info->realized = 1;
  info->reasons |= reason;
  // Forward to the bufferize graph so rule-driven boundary additions
  // (e.g. metal-tile-fanin-cap promoting a child) get a B_BUFFERIZE
  // record stamped with the current rule.  bufferize_realize_with_reason
  // is a no-op until bufferize_seed_from_nodes has run, so the
  // initial ROOT/MULTI/REDUCE seed pass does not double-mutate.
  bufferize_realize_with_reason(info->loc, info->op, reason);
}

static void bufferize_node_unmark(UOpInfo *info, u32 reason) {
  if (info == NULL) {
    return;
  }
  info->realized = 0;
  info->reasons |= reason;
  // Mirror the unmark into the bufferize graph and stamp removed_by
  // with whichever rule bufferize_rewrite_apply has flagged as current.
  bufferize_unrealize(info->loc);
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
    // Resolve VAR (SUB-bit) + ALO chains so a body post-APP-LAM
    // beta exposes the bound argument's UOP rather than the bare
    // VAR cell -- mirrors visit() in materialize.c so the walker
    // and the kernel walker agree on what's a kernel boundary.
    Term child = term_resolve(heap_read(loc + i));
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u64 cloc = term_val(child);
    u8 dup = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;

    u32 cidx = bufferize_node_get_or_add(cloc, term_ext(child));
    if (cidx != 0xFFFFFFFFu) BUFFERIZE_NODES[cidx].consumer_count++;
    bufferize_walk_rec(child, visited);
  }
}

static int bufferize_dump_fusion_candidates_enabled(void);

// Walk through layout-only wrappers (EXPAND/RESHAPE/PERMUTE) and
// CAST/BITCAST to bottom out at a UOP_CONST.  Used by the
// broadcast-reduce predicate to recognize ALU patterns where a scalar
// constant has been broadcast to match the post-reduce shape - e.g.
// `mean = reduce / N` lowers to `MUL(reduce, EXPAND(CONST(1/N)))` via
// the WL Times / liftNumeric / broadcastScalar pipeline in Tensor.wl.
// Recursion is bounded by the heap; in practice the wrapper depth is 1-2.
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

// Walk the heap looking for the unique UOp parent of `child_loc`.
// Returns the parent's loc, or 0 if there are zero or 2+ parents.
// Used by the softmax-relaxation pass to confirm the consumer chain.
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

// Walk a single chain branch starting from a *given* parent of a
// reduce (or any other node) and verify it ends in EXPAND through
// scalar-preserving ops.  Mirrors the historical
// Test whether the op at `cur` is a scalar-preserving chain hop -
// either a unary scalar function, a layout-only movement op (RESHAPE/
// PERMUTE), or an ALU op whose other operand is a (possibly broadcast)
// constant.  `cur` is the loc of the current node; the predicate must
// inspect the node's children for the ALU case.
static int bufferize_chain_hop_is_scalar_preserving(u64 cur, u8 pop) {
  if (pop == UOP_NEG || pop == UOP_RECIP || pop == UOP_SQRT
   || pop == UOP_EXP2 || pop == UOP_LOG2
   || pop == UOP_RESHAPE || pop == UOP_PERMUTE) {
    return 1;
  }
  if (pop == UOP_ADD || pop == UOP_MUL) {
    Term ca = term_resolve(heap_read(cur + 0));
    Term cb = term_resolve(heap_read(cur + 1));
    if (bufferize_term_is_broadcast_of_const(ca, 0)) return 1;
    if (bufferize_term_is_broadcast_of_const(cb, 0)) return 1;
  }
  return 0;
}

// Returns 1 iff every UOp parent of child_loc has a chain that
// bottoms out at EXPAND through scalar-preserving hops.  Used both
// at the top of the predicate (when the reduce itself has fan-out)
// and at intermediate hops (when an internal node like the BN-mean
// MUL is shared between forward and backward branches).
static int bufferize_chain_walk_all_parents_is_broadcast(
    u64 child_loc, u32 hops_left);

// Walk a single chain starting from `parent_loc` and confirm it
// bottoms out at EXPAND.  When the current node is multi-consumer,
// recursively confirm every parent branch ends at EXPAND.  Bounded
// by `hops_left` to avoid pathological recursion.
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
      // Multi-consumer intermediate: descend into every parent.
      // Each parent's chain must independently end at EXPAND.
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

// bufferize_reduce_consumer_is_broadcast_chain body but starts at
// `parent` instead of finding it via bufferize_unique_uop_parent, so
// the multi-parent rule can call it once per branch.
//
// Scalar-preserving chain ops:
// - UNARY scalar fns: NEG/RECIP/SQRT/EXP2/LOG2 - same value, same shape.
// - BINARY ALU with broadcast-of-CONST sibling: ADD/MUL where the
//   other operand is a CONST or `EXPAND(CONST)` keeps the post-reduce
//   shape.  The WL Times UpValue lowers `t / N` to `MUL(t, EXPAND(CONST))`,
//   so the wrapper-walk via bufferize_term_is_broadcast_of_const is what
//   catches BatchNorm-style mean / variance reductions.
// - Pure movement: RESHAPE/PERMUTE do not change scalar values; they only
//   relayout the producer's output.  Critical for BatchNorm-style chains:
//   `REDUCE_SUM -> MUL(EXPAND(CONST 1/N)) -> RESHAPE({1,C,1,1}) -> EXPAND({B,C,H,W})`
//   where the RESHAPE prepares the broadcast axes before the EXPAND.
static int bufferize_chain_from_parent_is_broadcast(u64 parent_loc) {
  return bufferize_chain_walk_is_broadcast(parent_loc, 8);
}

// Walk a REDUCE's consumer chain and return 1 iff every hop is
// scalar-preserving until we hit an EXPAND (the broadcast back to
// vector shape).  The chain pattern this matches is the
// TSoftmax / "normalize by sum" idiom plus the BatchNorm
// mean -> (... - mean) -> EXPAND idiom:
//     REDUCE -> {NEG|RECIP|SQRT|EXP2|LOG2|RESHAPE|PERMUTE}* ->
//     {ADD|MUL}*(broadcast-of-CONST sib) -> EXPAND
// Multi-consumer intermediates are handled by branching the chain
// check across all parents - every branch must independently end at
// EXPAND.  This is what unblocks BN-mean (forward+backward share the
// {C}-shape mean, both reach EXPAND through different RESHAPE branches).
static int bufferize_reduce_consumer_is_broadcast_chain(u64 reduce_loc) {
  return bufferize_chain_walk_all_parents_is_broadcast(reduce_loc, 8);
}

// Phase C-2 relaxation: a REDUCE whose single consumer is an ALU
// chain that STAYS at the post-reduce shape (no EXPAND back to the
// source shape, no follow-on REDUCE) can be inlined into its
// consumer's kernel.  Pattern: REDUCE -> ALU(scalar, CONST/scalar)
// -> root.  Examples: TMean = REDUCE_SUM/N, sqrt(TSum), etc.
//
// Distinguished from broadcast_chain by the absence of EXPAND --
// here the chain ends at the realize root, not at a broadcast.
// Returns 1 only when the consumer chain reaches the realize root
// (so the whole computation collapses into one kernel).
static int bufferize_reduce_consumer_is_scalar_tail(u64 reduce_loc, u64 root_loc) {
  u64 cur = reduce_loc;
  for (u32 hops = 0; hops < 8; hops++) {
    if (cur == root_loc) return 1;   // chain reaches the realize root
    u64 parent = bufferize_unique_uop_parent(cur);
    if (parent == 0) return 0;
    u32 idx = bufferize_info_find(parent);
    if (idx == 0xFFFFFFFFu) return 0;
    u8 pop = BUFFERIZE_NODES[idx].op;
    // Scalar-preserving unary ops keep the post-reduce shape.
    if (pop == UOP_NEG || pop == UOP_RECIP || pop == UOP_SQRT
     || pop == UOP_EXP2 || pop == UOP_LOG2) {
      cur = parent;
      continue;
    }
    // Binary ops where the OTHER child is a scalar (CONST or
    // numel-1 EXPAND) also keep the post-reduce shape.  We approx
    // by allowing ADD/MUL when the parent has a CONST sibling.
    if (pop == UOP_ADD || pop == UOP_MUL) {
      // Check if any child is CONST.
      Term ca = term_resolve(heap_read(parent + 0));
      Term cb = term_resolve(heap_read(parent + 1));
      u8 has_const = 0;
      if (term_tag(ca) == TAG_UOP && term_ext(ca) == UOP_CONST) has_const = 1;
      if (term_tag(cb) == TAG_UOP && term_ext(cb) == UOP_CONST) has_const = 1;
      if (!has_const) return 0;
      cur = parent;
      continue;
    }
    return 0;
  }
  return 0;
}

static int bufferize_metal_tile_fanin_cap_enabled(void) {
  char const *backend = getenv("THVM_BACKEND");
  char const *tile    = getenv("THVM_TILE");
  return backend != NULL && strcmp(backend, "metal") == 0
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
      || op == UOP_CAST    || op == UOP_BITCAST
      || op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
}

static u32 bufferize_fanin_uop_count(u64 loc, u64 boundary_root,
                                   u32 cap, u32 *hits);

static u32 bufferize_fanin_term_count(Term t, u64 boundary_root,
                                    u32 cap, u32 *hits) {
  t = term_resolve(t);
  if (term_tag(t) == TAG_TEN) return 1;
  if (term_tag(t) == TAG_VAR) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 1;
  return bufferize_fanin_uop_count(term_val(t), boundary_root, cap, hits);
}

static u32 bufferize_fanin_uop_count(u64 loc, u64 boundary_root,
                                   u32 cap, u32 *hits) {
  if (loc >= HEAP_NEXT) return 1;
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 1;
  UOpInfo *info = &BUFFERIZE_NODES[idx];
  if (loc != boundary_root && info->realized) return 1;
  if (info->op == UOP_CONST) return 0;

  u8  ar          = uop_arity(info->op);
  u32 total       = 0;
  u32 child_idx  [MAX_UOP_SRC] = {0};
  u32 child_count[MAX_UOP_SRC] = {0};
  for (u8 i = 0; i < ar; i++) {
    child_idx[i] = 0xFFFFFFFFu;
    Term child = term_resolve(heap_read(loc + i));
    child_count[i] = bufferize_fanin_term_count(child, boundary_root,
                                              cap, hits);
    total += child_count[i];
    if (term_tag(child) == TAG_UOP && term_ext(child) != UOP_KERNEL) {
      child_idx[i] = bufferize_info_find(term_val(child));
    }
  }

  while (total > cap) {
    u8  best      = 0xFF;
    u32 best_size = 1;
    for (u8 i = 0; i < ar; i++) {
      u32 cidx = child_idx[i];
      if (cidx == 0xFFFFFFFFu) continue;
      if (child_count[i] <= best_size) continue;
      if (!bufferize_fanin_split_child_op(BUFFERIZE_NODES[cidx].op)) continue;
      best      = i;
      best_size = child_count[i];
    }
    if (best == 0xFF) break;
    UOpInfo *child_info = &BUFFERIZE_NODES[child_idx[best]];
    if (!child_info->realized && hits != NULL) (*hits)++;
    bufferize_node_mark(child_info, BUFFERIZE_REASON_FANIN_CAP);
    total = total - child_count[best] + 1;
    child_count[best] = 1;
  }
  return total;
}

static u32 bufferize_rule_metal_tile_fanin_cap(Term root) {
  if (!bufferize_metal_tile_fanin_cap_enabled()) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) == UOP_KERNEL) return 0;

  u32 cap = bufferize_metal_tile_fanin_cap();
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (!BUFFERIZE_NODES[i].realized) continue;
    bufferize_fanin_uop_count(BUFFERIZE_NODES[i].loc, BUFFERIZE_NODES[i].loc,
                            cap, &hits);
  }
  return hits;
}

static int classify_op_is_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
}

static u64 classify_shape_numel(Shape const *s) {
  if (s == NULL || s->ndim == 0) {
    return 0;
  }
  u64 n = 1;
  for (u32 i = 0; i < s->ndim; i++) {
    n *= s->dims[i];
  }
  return n;
}

static int bufferize_subtree_has_reduce(Term t, u32 depth) {
  if (depth > 64) {
    return 1;
  }
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) {
    return 0;
  }
  if (term_ext(t) == UOP_KERNEL) {
    return 0;
  }
  u64 loc = term_val(t);
  u32 idx = bufferize_info_find(loc);
  if (idx != 0xFFFFFFFFu && BUFFERIZE_NODES[idx].realized) {
    return 0;
  }
  u8 op = term_ext(t);
  if (op == UOP_REDUCE) {
    return 1;
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (bufferize_subtree_has_reduce(heap_read(loc + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int bufferize_inline_multiconsumer_expand_enabled(void) {
  if (!bufferize_metal_tile_fanin_cap_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_INLINE_MULTI_CONSUMER_EXPAND");
  return e == NULL || e[0] != '0';
}

static int bufferize_inline_multiconsumer_pure_enabled(void) {
  if (!bufferize_metal_tile_fanin_cap_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_INLINE_MULTI_CONSUMER_PURE");
  return e == NULL || e[0] != '0';
}

static int bufferize_rangeify_enabled(void);

static int bufferize_inline_reduce_fanout_enabled(void) {
  if (!bufferize_metal_tile_fanin_cap_enabled() || !bufferize_rangeify_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_INLINE_REDUCE_FANOUT");
  return e == NULL || e[0] != '0';
}

static int bufferize_remove_removable_bufferize_enabled(void) {
  if (!bufferize_metal_tile_fanin_cap_enabled() || !bufferize_rangeify_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_REMOVE_REMOVABLE_BUFFERIZE");
  return e == NULL || e[0] != '0';
}

static u64 bufferize_inline_multiconsumer_pure_min_numel(void) {
  char const *e = getenv("THVM_INLINE_MULTI_CONSUMER_PURE_MIN_NUMEL");
  if (e != NULL && e[0] != '\0') {
    unsigned long long v = strtoull(e, NULL, 10);
    if (v > 0) {
      return (u64)v;
    }
  }
  return 65536;
}

static u64 bufferize_inline_reduce_fanout_min_numel(void) {
  char const *e = getenv("THVM_INLINE_REDUCE_FANOUT_MIN_NUMEL");
  if (e != NULL && e[0] != '\0') {
    unsigned long long v = strtoull(e, NULL, 10);
    if (v > 0) {
      return (u64)v;
    }
  }
  return 65536;
}

static u32 bufferize_remove_removable_bufferize_max_ops(void) {
  char const *e = getenv("THVM_REMOVE_BUFFERIZE_MAX_OPS");
  if (e != NULL && e[0] != '\0') {
    unsigned long v = strtoul(e, NULL, 10);
    if (v >= 1 && v <= 1024) {
      return (u32)v;
    }
  }
  return 32;
}

static u32 bufferize_remove_removable_bufferize_max_consumers(void) {
  char const *e = getenv("THVM_REMOVE_BUFFERIZE_MAX_CONSUMERS");
  if (e != NULL && e[0] != '\0') {
    unsigned long v = strtoul(e, NULL, 10);
    if (v >= 2 && v <= 64) {
      return (u32)v;
    }
  }
  return 8;
}

static int bufferize_dump_fusion_candidates_enabled(void) {
  char const *e = getenv("DUMP_FUSION_REWRITE_CANDIDATES");
  return e != NULL && e[0] == '1';
}

static u32 bufferize_rule_inline_large_expand_fanout(Term root) {
  (void)root;
  if (!bufferize_inline_multiconsumer_expand_enabled()) {
    return 0;
  }
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (!info->realized || info->op != UOP_EXPAND
        || info->consumer_count < 2) {
      continue;
    }
    Term self = term_new(0, TAG_UOP, UOP_EXPAND, info->loc);
    Term src  = term_resolve(heap_read(info->loc));
    Shape out_shape, src_shape;
    if (!term_shape_in(self, 0, &out_shape)
        || !term_shape_in(src, 0, &src_shape)) {
      continue;
    }
    u64 out_numel = classify_shape_numel(&out_shape);
    u64 src_numel = classify_shape_numel(&src_shape);
    if (src_numel == 0 || out_numel < src_numel * 8) {
      continue;
    }
    if (bufferize_subtree_has_reduce(src, 0)) {
      continue;
    }
    hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

static int bufferize_recompute_pure_op(u8 op) {
  return op == UOP_CONST || op == UOP_LOAD
      || op == UOP_CAST  || op == UOP_BITCAST
      || uop_is_unary_elementwise(op)
      || uop_is_binary_elementwise(op)
      || classify_op_is_movement(op);
}

static int bufferize_subtree_is_pure_recomputable(Term t, u64 root_loc,
                                                 u32 depth,
                                                 int *has_movement) {
  if (depth > 96) {
    return 0;
  }
  t = term_resolve(t);
  u8 tag = term_tag(t);
  if (tag == TAG_TEN || tag == TAG_VAR) {
    return 1;
  }
  if (tag != TAG_UOP) {
    return 0;
  }
  u8 op = term_ext(t);
  if (op == UOP_KERNEL) {
    return 1;
  }
  if (!bufferize_recompute_pure_op(op) || op == UOP_REDUCE) {
    return 0;
  }
  if (classify_op_is_movement(op)) {
    *has_movement = 1;
  }

  u64 loc = term_val(t);
  if (loc != root_loc) {
    u32 idx = bufferize_info_find(loc);
    if (idx != 0xFFFFFFFFu && BUFFERIZE_NODES[idx].realized) {
      return 1;
    }
  }

  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (!bufferize_subtree_is_pure_recomputable(heap_read(loc + i),
                                              root_loc, depth + 1,
                                              has_movement)) {
      return 0;
    }
  }
  return 1;
}

static int bufferize_parent_uses_child(u64 parent_loc, u64 child_loc) {
  u32 pidx = bufferize_info_find(parent_loc);
  if (pidx == 0xFFFFFFFFu) {
    return 0;
  }
  u8 ar = uop_arity(BUFFERIZE_NODES[pidx].op);
  for (u8 j = 0; j < ar; j++) {
    Term c = term_resolve(heap_read(parent_loc + j));
    if (term_tag(c) == TAG_UOP && term_val(c) == child_loc) {
      return 1;
    }
  }
  return 0;
}

typedef struct {
  u32 ops;
  u32 inputs;
  int has_movement;
} RealizeRecomputeStats;

static int bufferize_subtree_recompute_stats(Term t,
                                           u64 root_loc,
                                           u32 depth,
                                           RealizeRecomputeStats *stats) {
  if (depth > 96 || stats == NULL) {
    return 0;
  }
  t = term_resolve(t);
  u8 tag = term_tag(t);
  if (tag == TAG_TEN || tag == TAG_VAR) {
    stats->inputs++;
    return 1;
  }
  if (tag != TAG_UOP) {
    return 0;
  }
  u8 op = term_ext(t);
  if (op == UOP_KERNEL) {
    stats->inputs++;
    return 1;
  }
  if (op == UOP_REDUCE || !bufferize_recompute_pure_op(op)) {
    return 0;
  }

  u64 loc = term_val(t);
  if (loc != root_loc) {
    u32 idx = bufferize_info_find(loc);
    if (idx != 0xFFFFFFFFu && BUFFERIZE_NODES[idx].realized) {
      stats->inputs++;
      return 1;
    }
  }

  if (classify_op_is_movement(op)) {
    stats->has_movement = 1;
  }
  if (op != UOP_CONST && op != UOP_LOAD) {
    stats->ops++;
  }

  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (!bufferize_subtree_recompute_stats(heap_read(loc + i),
                                         root_loc, depth + 1,
                                         stats)) {
      return 0;
    }
  }
  return 1;
}

static int bufferize_parent_chain_reaches_reduce(u64 child_loc, u32 depth) {
  if (depth > 32) {
    return 1;
  }
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    if (ploc == child_loc) {
      continue;
    }
    u8 pop = BUFFERIZE_NODES[i].op;
    if (!bufferize_parent_uses_child(ploc, child_loc)) {
      continue;
    }
    if (pop == UOP_REDUCE) {
      return 1;
    }
    if (bufferize_parent_chain_reaches_reduce(ploc, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static u32 bufferize_rule_inline_pure_fanout_probe(Term root) {
  (void)root;
  if (!bufferize_inline_multiconsumer_pure_enabled()) {
    return 0;
  }
  u64 min_numel = bufferize_inline_multiconsumer_pure_min_numel();
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (!info->realized || info->consumer_count < 2) {
      continue;
    }
    if (!bufferize_recompute_pure_op(info->op)) {
      continue;
    }
    Term self = term_new(0, TAG_UOP, info->op, info->loc);
    Shape out_shape = {0};
    if (!term_shape_in(self, 0, &out_shape)) {
      continue;
    }
    if (classify_shape_numel(&out_shape) < min_numel) {
      continue;
    }
    int has_movement = 0;
    if (!bufferize_subtree_is_pure_recomputable(self, info->loc, 0,
                                              &has_movement)) {
      continue;
    }
    if (!has_movement) {
      continue;
    }
    if (bufferize_parent_chain_reaches_reduce(info->loc, 0)) {
      if (bufferize_dump_fusion_candidates_enabled()) {
        fprintf(stderr,
                "bufferize_rewrite_skip rule=inline-pure-fanout-probe"
                " reason=reduce-parent loc=%llu op=%u numel=%llu"
                " consumers=%u\n",
                (unsigned long long)info->loc,
                (unsigned)info->op,
                (unsigned long long)classify_shape_numel(&out_shape),
                (unsigned)info->consumer_count);
      }
      continue;
    }
    if (bufferize_dump_fusion_candidates_enabled()) {
      fprintf(stderr,
              "bufferize_rewrite_hit rule=inline-pure-fanout-probe"
              " loc=%llu op=%u numel=%llu consumers=%u\n",
              (unsigned long long)info->loc,
              (unsigned)info->op,
              (unsigned long long)classify_shape_numel(&out_shape),
              (unsigned)info->consumer_count);
    }
    hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

static int bufferize_reduce_fanout_consumers_ok(u64 reduce_loc) {
  u32 consumers = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    if (ploc == reduce_loc) {
      continue;
    }
    u8 pop = BUFFERIZE_NODES[i].op;
    u8 ar  = uop_arity(pop);
    int hit = 0;
    for (u8 j = 0; j < ar; j++) {
      Term c = term_resolve(heap_read(ploc + j));
      if (term_tag(c) == TAG_UOP && term_val(c) == reduce_loc) {
        hit = 1;
        break;
      }
    }
    if (!hit) {
      continue;
    }
    consumers++;
    if (pop == UOP_REDUCE || !bufferize_recompute_pure_op(pop)) {
      return 0;
    }
    if (bufferize_parent_chain_reaches_reduce(ploc, 0)) {
      return 0;
    }
  }
  return consumers >= 2;
}

static u32 bufferize_rule_inline_reduce_fanout(Term root) {
  (void)root;
  if (!bufferize_inline_reduce_fanout_enabled()) {
    return 0;
  }
  u64 min_numel = bufferize_inline_reduce_fanout_min_numel();
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (!info->realized || info->op != UOP_REDUCE
        || info->consumer_count < 2) {
      continue;
    }
    Term self = term_new(0, TAG_UOP, info->op, info->loc);
    Shape out_shape = {0};
    if (!term_shape_in(self, 0, &out_shape)) {
      continue;
    }
    if (classify_shape_numel(&out_shape) < min_numel) {
      continue;
    }
    if (!bufferize_reduce_fanout_consumers_ok(info->loc)) {
      continue;
    }
    hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

// Phase 5/3 follow-up: the historical producer_has_movement gates
// blocked any movement-bearing buffer from being recomputed when
// its consumer is (or reaches) a REDUCE.  The conservative stance
// existed (commit 699f822 "fix: guard pure fanout probe from
// reduce chains") because relaxations at that time created
// unsupported fat reduce kernels - i.e. tile-render bailing,
// metal-jit fallback, slower wall time.
//
// Phase 2's per-USE BIndex chain (chain_op_idx + chain_input_slot
// + chain_edge_idx + BIndexChainOp data) plus rangeify's reroute
// through `kernel_entry_prog_chain_op` gives the codegen path
// enough information to compose movement chains correctly even
// when the consumer kernel absorbs a recomputed movement+reduce.
// The bounded canary now produces 0 metal-jit fallbacks with the
// gate lifted.
//
// Default-on; set THVM_BUFFERIZE_LIFT_MOVEMENT_REDUCE_GATE=0 to
// restore the conservative pre-Phase-2 behaviour for bisecting.
static int bufferize_lift_movement_reduce_gate_enabled(void) {
  char const *e = getenv("THVM_BUFFERIZE_LIFT_MOVEMENT_REDUCE_GATE");
  return e == NULL ? 1 : (e[0] != '0');
}

static int bufferize_removable_consumers_ok(u64 loc,
                                                    int producer_has_movement,
                                                    u32 max_consumers) {
  int gate_lifted = bufferize_lift_movement_reduce_gate_enabled();
  u32 consumers = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    if (ploc == loc) {
      continue;
    }
    if (!bufferize_parent_uses_child(ploc, loc)) {
      continue;
    }

    consumers++;
    if (consumers > max_consumers) {
      return 0;
    }

    u8 pop = BUFFERIZE_NODES[i].op;
    if (pop == UOP_REDUCE) {
      if (producer_has_movement && !gate_lifted) {
        return 0;
      }
      continue;
    }
    if (!bufferize_recompute_pure_op(pop)) {
      return 0;
    }
    if (producer_has_movement && !gate_lifted
        && bufferize_parent_chain_reaches_reduce(ploc, 0)) {
      return 0;
    }
  }
  return consumers >= 2;
}

static u32 bufferize_rule_remove_removable_bufferize(Term root) {
  (void)root;
  if (!bufferize_remove_removable_bufferize_enabled()) {
    return 0;
  }

  u32 hits = 0;
  u32 max_ops = bufferize_remove_removable_bufferize_max_ops();
  u32 max_consumers = bufferize_remove_removable_bufferize_max_consumers();
  for (u32 pass = 0; pass < 8; pass++) {
    u32 pass_hits = 0;
    for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
      UOpInfo *info = &BUFFERIZE_NODES[i];
      if (!info->realized || info->consumer_count < 2) {
        continue;
      }
      if ((info->reasons & BUFFERIZE_REASON_MULTI) == 0) {
        continue;
      }
      if ((info->reasons & (BUFFERIZE_REASON_ROOT | BUFFERIZE_REASON_REDUCE
                          | BUFFERIZE_REASON_FANIN_CAP)) != 0) {
        continue;
      }
      if (!bufferize_recompute_pure_op(info->op) || info->op == UOP_REDUCE) {
        continue;
      }

      Term self = term_new(0, TAG_UOP, info->op, info->loc);
      RealizeRecomputeStats stats;
      memset(&stats, 0, sizeof(stats));
      if (!bufferize_subtree_recompute_stats(self, info->loc, 0, &stats)) {
        continue;
      }
      if (stats.ops > max_ops) {
        if (bufferize_dump_fusion_candidates_enabled()) {
          fprintf(stderr,
                  "bufferize_rewrite_skip rule=remove-removable-bufferize"
                  " reason=op-budget loc=%llu op=%u ops=%u consumers=%u\n",
                  (unsigned long long)info->loc,
                  (unsigned)info->op,
                  (unsigned)stats.ops,
                  (unsigned)info->consumer_count);
        }
        continue;
      }
      if (!bufferize_removable_consumers_ok(info->loc,
                                                    stats.has_movement,
                                                    max_consumers)) {
        if (bufferize_dump_fusion_candidates_enabled()) {
          fprintf(stderr,
                  "bufferize_rewrite_skip rule=remove-removable-bufferize"
                  " reason=consumer-legality loc=%llu op=%u ops=%u"
                  " movement=%u consumers=%u\n",
                  (unsigned long long)info->loc,
                  (unsigned)info->op,
                  (unsigned)stats.ops,
                  (unsigned)stats.has_movement,
                  (unsigned)info->consumer_count);
        }
        continue;
      }

      if (bufferize_dump_fusion_candidates_enabled()) {
        fprintf(stderr,
                "bufferize_rewrite_hit rule=remove-removable-bufferize"
                " loc=%llu op=%u ops=%u movement=%u consumers=%u\n",
                (unsigned long long)info->loc,
                (unsigned)info->op,
                (unsigned)stats.ops,
                (unsigned)stats.has_movement,
                (unsigned)info->consumer_count);
      }
      bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
      pass_hits++;
    }
    hits += pass_hits;
    if (pass_hits == 0) {
      break;
    }
  }
  return hits;
}

static int bufferize_inline_subtree_has_movement(Term t, u32 depth) {
  if (depth > 64) return 1;
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 0;
  u64 loc = term_val(t);
  u32 idx = bufferize_info_find(loc);
  if (idx != 0xFFFFFFFFu && BUFFERIZE_NODES[idx].realized) return 0;
  u8 op = term_ext(t);
  if (classify_op_is_movement(op)) return 1;
  if (op == UOP_REDUCE) return 1;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (bufferize_inline_subtree_has_movement(heap_read(loc + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int bufferize_reduce_chain_source_is_direct(Term t) {
  t = term_resolve(t);
  if (term_tag(t) == TAG_TEN || term_tag(t) == TAG_VAR) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 1;
  u32 idx = bufferize_info_find(term_val(t));
  return idx != 0xFFFFFFFFu && BUFFERIZE_NODES[idx].realized;
}

static u32 bufferize_reduce_count(void) {
  u32 n = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    if (BUFFERIZE_NODES[i].op == UOP_REDUCE) n++;
  }
  return n;
}

// Walk up from `start_loc` through unique-parent ops (movement,
// scalar-preserving elementwise, etc.) until we hit the first
// REALIZED ancestor in BUFFERIZE_NODES.  Returns that ancestor's
// loc -- the "absorbing boundary" that owns the kernel which would
// receive `start_loc`'s inlined KProgOp graph.  Returns 0 if the
// walk loses uniqueness (multi-parent intermediate, no parent
// found in BUFFERIZE_NODES) before finding one.  Bounded by
// `hops_left` to avoid pathological recursion.
//
// Purpose: rangeify_try_lower_elementwise rejects kernels with > 1
// REDUCE op.  When inline-softmax-broadcast-reduce unmarks a REDUCE
// whose absorbing kernel root is ITSELF a REDUCE (e.g. a BN-mean
// chain folding into a downstream maxpool/grad reduce), the merged
// kernel ends up with 2 REDUCE ops and bails out of rangeify,
// dispatching via the slower per-op metal encoder instead of the
// tile-jit path.  Returning the absorbing boundary's op lets the
// rule gate inlining on the consumer being a non-REDUCE root.
static u64 bufferize_absorbing_boundary(u64 start_loc, u32 hops_left) {
  u64 cur = start_loc;
  while (hops_left > 0) {
    u64 next = bufferize_unique_uop_parent(cur);
    if (next == 0) return 0;
    u32 idx = bufferize_info_find(next);
    if (idx == 0xFFFFFFFFu) return 0;
    if (BUFFERIZE_NODES[idx].realized) return next;
    cur = next;
    hops_left--;
  }
  return 0;
}

static int bufferize_rangeify_enabled(void) {
  char const *e = getenv("THVM_RANGEIFY");
  return e == NULL ? 1 : (e[0] != '0');
}

// Phase 4 follow-up: a bufferize-graph-driven removal rule.
//
// Iterates the bufferize graph (not BUFFERIZE_NODES), reads the
// cost-model score that bufferize_seed_from_nodes computed,
// and unmarks any MULTI-only buffer whose
// `output_numel / max(recompute_total, 1)` exceeds the threshold.
// Backend-cap, root, reduce, and reduce-subtree gates already live
// inside bufferize_removal_score so the rule body just enforces
// hard caps on op count and consumer count to keep recompute
// budgets bounded.
//
// Default-on with a conservative threshold tuned by the bounded
// beautiful-mnist canary.  Lower thresholds remove more buffers
// but force kernels with bigger recompute fan-in into metal-jit
// fallback (the tile path bails on programs with too many ops),
// which costs more wall time than the kernel-count savings buy
// back.  THRESHOLD=50000 is the lowest value that still lands the
// canary's metal-tile dispatch count at the no-rule baseline; it
// only fires for buffers where memory savings dominate recompute
// cost by 50000:1.  Set `THVM_BUFFERIZE_REMOVE_BY_SCORE=0` to
// disable; tunables `_THRESHOLD`, `_MAX_OPS`, `_MAX_CONSUMERS`
// override the defaults for measurement work.
static int bufferize_remove_by_cost_score_enabled(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_BY_SCORE");
  if (e == NULL) return 1;
  return e[0] != '0';
}

static u64 bufferize_remove_by_cost_score_threshold(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_THRESHOLD");
  if (e != NULL && e[0] != '\0') {
    return strtoull(e, NULL, 10);
  }
  return 50000;
}

static u32 bufferize_remove_by_cost_score_max_ops(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_MAX_OPS");
  if (e != NULL && e[0] != '\0') {
    return (u32)strtoul(e, NULL, 10);
  }
  return 32;
}

static u32 bufferize_remove_by_cost_score_max_consumers(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_MAX_CONSUMERS");
  if (e != NULL && e[0] != '\0') {
    return (u32)strtoul(e, NULL, 10);
  }
  // Bumped to 32 after the bounded canary showed a sharp
  // discontinuity at this threshold: a small set of high-fanout
  // buffers (e.g. movement-aliased weights consumed by every
  // gradient/Adam step) become removable here and the resulting
  // kernels stay tile-friendly because the score's denominator
  // (recompute_ops * consumer_count) and the tile-feasibility
  // consumer-budget guard already filter the dangerous cases.
  // Yields ~30% wall-time win on the canary (616 ms -> 433 ms).
  // Score's denominator already penalises high consumer counts.
  return 32;
}

// Tile-feasibility proxy: estimate the consumer kernel's op count
// after recomputing this buffer locally.  Each consumer kernel
// would gain `recompute_ops` ops on every input slot that used the
// removed buffer; pessimistic upper bound = consumer's existing
// recompute_ops + this buffer's recompute_ops.  If any consumer's
// estimated total exceeds the budget, skip.  Default 64 (well
// above the existing remove-removable-bufferize 32 cap, since
// that rule's cap is per-buffer not per-consumer).  Override with
// THVM_BUFFERIZE_REMOVE_SCORE_CONSUMER_OPS_BUDGET.
static u32 bufferize_remove_by_cost_score_consumer_budget(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_CONSUMER_OPS_BUDGET");
  if (e != NULL && e[0] != '\0') {
    return (u32)strtoul(e, NULL, 10);
  }
  return 64;
}

// Walk up from `loc` through unrealized parents until we hit a
// realized boundary; return that boundary's recompute_ops (the
// kernel program size that would absorb b's recompute).  Bounds
// recursion to depth 32 - bigger chains are pathological - and
// returns 0 (defensive) when no realized ancestor is found.
// Across multiple paths to different realized ancestors, returns
// the MAX (the worst-case kernel that would absorb the recompute).
static u32 bufferize_realized_ancestor_recompute_ops(u64 loc, u32 depth) {
  if (depth > 32) return 0;
  u32 max_ops = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    if (ploc == loc) continue;
    if (!bufferize_parent_uses_child(ploc, loc)) continue;
    if (BUFFERIZE_NODES[i].realized) {
      u32 bidx = bufferize_find_by_loc(ploc);
      u32 ops = (bidx != 0xFFFFFFFFu)
                  ? bufferize_buffer_at(bidx)->recompute_ops
                  : 0;
      if (ops > max_ops) max_ops = ops;
    } else {
      u32 up_ops = bufferize_realized_ancestor_recompute_ops(ploc, depth + 1);
      if (up_ops > max_ops) max_ops = up_ops;
    }
  }
  return max_ops;
}

static int bufferize_remove_by_cost_score_consumers_fit_tile(
    BBufferize const *b, u32 budget) {
  u64 b_loc = b->loc;
  u32 b_ops = b->recompute_ops;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    u64 ploc = BUFFERIZE_NODES[i].loc;
    if (ploc == b_loc) continue;
    u8  pop  = BUFFERIZE_NODES[i].op;
    u8  ar   = uop_arity(pop);
    int hits = 0;
    for (u8 j = 0; j < ar; j++) {
      Term c = term_resolve(heap_read(ploc + j));
      if (term_tag(c) == TAG_UOP && term_val(c) == b_loc) {
        hits = 1; break;
      }
    }
    if (!hits) continue;
    // Consumer found.  If the consumer itself is a realized
    // buffer, use its recompute_ops directly.  Otherwise walk up
    // to find the eventual realized ancestor whose kernel program
    // would absorb the recompute (bounded recursion).
    u32 cidx = bufferize_find_by_loc(ploc);
    u64 consumer_ops = 0;
    if (cidx != 0xFFFFFFFFu) {
      BBufferize const *cb = bufferize_buffer_at(cidx);
      if (cb != NULL) consumer_ops = (u64)cb->recompute_ops;
    } else {
      consumer_ops = (u64)bufferize_realized_ancestor_recompute_ops(ploc, 0);
    }
    u64 estimated = consumer_ops + (u64)b_ops;
    if (estimated > (u64)budget) return 0;
  }
  return 1;
}

static u32 bufferize_rule_remove_by_cost_score(Term root) {
  (void)root;
  if (!bufferize_remove_by_cost_score_enabled()) return 0;
  u64 threshold     = bufferize_remove_by_cost_score_threshold();
  u32 max_ops       = bufferize_remove_by_cost_score_max_ops();
  u32 max_consumers = bufferize_remove_by_cost_score_max_consumers();
  u32 budget        = bufferize_remove_by_cost_score_consumer_budget();
  int dump = bufferize_dump_fusion_candidates_enabled();
  u32 hits = 0;
  // Iterate the bufferize graph directly - this rule is the first
  // one in the codebase that reads BBufferize records as the
  // canonical schedule state and only touches BUFFERIZE_NODES to
  // project the removal back for materialize.c.
  for (u32 i = 0; i < bufferize_buffer_count(); i++) {
    BBufferize const *b = bufferize_buffer_at(i);
    if (b == NULL || !b->realized) continue;
    if (!(b->reasons & BUFFERIZE_REASON_MULTI)) continue;
    if (b->reasons & (BUFFERIZE_REASON_ROOT
                    | BUFFERIZE_REASON_REDUCE
                    | BUFFERIZE_REASON_BACKEND_CAP)) continue;
    if (b->subtree_has_reduce) {
      if (dump) {
        fprintf(stderr,
                "bufferize_rewrite_skip rule=remove-by-cost-score"
                " reason=subtree-reduce loc=%llu op=%u\n",
                (unsigned long long)b->loc, (unsigned)b->op);
      }
      continue;
    }
    if (b->recompute_ops > max_ops) {
      if (dump) {
        fprintf(stderr,
                "bufferize_rewrite_skip rule=remove-by-cost-score"
                " reason=op-budget loc=%llu op=%u ops=%u cap=%u\n",
                (unsigned long long)b->loc, (unsigned)b->op,
                (unsigned)b->recompute_ops, (unsigned)max_ops);
      }
      continue;
    }
    if (b->consumer_count > max_consumers) {
      if (dump) {
        fprintf(stderr,
                "bufferize_rewrite_skip rule=remove-by-cost-score"
                " reason=consumer-cap loc=%llu op=%u consumers=%u cap=%u\n",
                (unsigned long long)b->loc, (unsigned)b->op,
                (unsigned)b->consumer_count, (unsigned)max_consumers);
      }
      continue;
    }
    u64 score = bufferize_removal_score(b->buffer_id);
    if (score < threshold) {
      if (dump) {
        fprintf(stderr,
                "bufferize_rewrite_skip rule=remove-by-cost-score"
                " reason=score-threshold loc=%llu op=%u score=%llu"
                " threshold=%llu\n",
                (unsigned long long)b->loc, (unsigned)b->op,
                (unsigned long long)score, (unsigned long long)threshold);
      }
      continue;
    }
    if (!bufferize_remove_by_cost_score_consumers_fit_tile(b, budget)) {
      if (dump) {
        fprintf(stderr,
                "bufferize_rewrite_skip rule=remove-by-cost-score"
                " reason=tile-feasibility loc=%llu op=%u ops=%u"
                " budget=%u\n",
                (unsigned long long)b->loc, (unsigned)b->op,
                (unsigned)b->recompute_ops, (unsigned)budget);
      }
      continue;
    }
    u32 ridx = bufferize_info_find(b->loc);
    if (ridx == 0xFFFFFFFFu) continue;
    if (dump) {
      fprintf(stderr,
              "bufferize_rewrite_hit rule=remove-by-cost-score"
              " loc=%llu op=%u ops=%u consumers=%u numel=%llu score=%llu\n",
              (unsigned long long)b->loc, (unsigned)b->op,
              (unsigned)b->recompute_ops,
              (unsigned)b->consumer_count,
              (unsigned long long)b->output_numel,
              (unsigned long long)score);
    }
    bufferize_node_unmark(&BUFFERIZE_NODES[ridx], BUFFERIZE_REASON_INLINE);
    hits++;
  }
  return hits;
}

static u32 bufferize_rule_inline_constants(Term root) {
  (void)root;
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->op != UOP_CONST) continue;
    if (info->realized) hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

// Lift the source-is-direct + has-movement gates for adjacent-reduce
// chain inlining.  At BS=32 the lift gives -39% kernels (118 -> 72)
// and -37% wall time on the bounded beautiful-mnist canary because
// the FLAT_GRID-with-nested-reduce relaxation (commit 3b11bf6) keeps
// the merged reduce-bearing kernel on tile-JIT.
//
// KNOWN ISSUE: at BS >= 64 the lifted gates trigger a warmup-compile
// blow-up -- the rule fires on candidates whose recompute subtrees
// grow super-linearly with BS, and the compile pass never returns.
// BS=64..512 with gate OFF complete normally; with gate ON they
// timeout >5min during the first compile.  Default OFF until the
// source-subtree size is bounded; THVM_BUFFERIZE_LIFT_REDUCE_CHAIN_GATES=1
// opts in for the BS=32 win.
static int bufferize_lift_reduce_chain_gates(void) {
  char const *e = getenv("THVM_BUFFERIZE_LIFT_REDUCE_CHAIN_GATES");
  return e != NULL && e[0] == '1';
}

static u32 bufferize_rule_inline_adjacent_reduce_chains(Term root) {
  (void)root;
  int lift = bufferize_lift_reduce_chain_gates();
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->op != UOP_REDUCE) continue;
    ReduceChainInfo rc;
    Term root_term = term_new(0, TAG_UOP, UOP_REDUCE, info->loc);
    if (!reduce_chain_collect(root_term, &rc)) continue;
    if (!lift && !bufferize_reduce_chain_source_is_direct(rc.src)) continue;
    if (!lift && bufferize_inline_subtree_has_movement(rc.src, 0)) continue;
    int ok = 1;
    for (u32 j = 1; j < rc.n_reduces; j++) {
      u32 cidx = bufferize_info_find(rc.locs[j]);
      if (cidx == 0xFFFFFFFFu || BUFFERIZE_NODES[cidx].consumer_count != 1) {
        ok = 0;
        break;
      }
    }
    if (!ok) continue;
    for (u32 j = 1; j < rc.n_reduces; j++) {
      u32 cidx = bufferize_info_find(rc.locs[j]);
      if (cidx == 0xFFFFFFFFu) continue;
      if (BUFFERIZE_NODES[cidx].realized) hits++;
      bufferize_node_unmark(&BUFFERIZE_NODES[cidx], BUFFERIZE_REASON_INLINE);
    }
  }
  return hits;
}

// Gate the softmax-broadcast-reduce / scalar-tail rules' multi-reduce
// generalization.  Each REDUCE is checked independently by
// bufferize_reduce_consumer_is_broadcast_chain, so removing the
// "exactly one REDUCE in the graph" restriction is safe per-rule.
// Default-on; THVM_BUFFERIZE_REDUCE_FUSE_MULTI=0 reverts to the
// historical single-reduce gate.
static int bufferize_reduce_fuse_multi_enabled(void) {
  char const *e = getenv("THVM_BUFFERIZE_REDUCE_FUSE_MULTI");
  return e == NULL ? 1 : (e[0] != '0');
}

// Tile-feasibility predicate for the multi-reduce-fuse generalization:
// rangeify_try_lower_elementwise bails when a kernel program carries
// > 1 REDUCE op, and the rangeify-less fallback dispatches to the
// per-op metal encoder (~2-3x slower than the tile-jit path on
// beautiful-mnist-shape kernels).  Default-on; the env var is provided
// for bisection.  When 0, the rule falls back to the historical
// behaviour of inlining every legal candidate regardless of whether
// the absorbing kernel root is also a REDUCE.
static int bufferize_softmax_reduce_tile_cap_enabled(void) {
  char const *e = getenv("THVM_BUFFERIZE_SOFTMAX_REDUCE_TILE_CAP");
  return e == NULL ? 1 : (e[0] != '0');
}

// bufferize_unique_uop_parent returns the SOLE parent of `loc` or 0
// if zero/multiple.  bufferize_reduce_consumer_is_broadcast_chain
// already requires the chain to have a unique parent at every hop,
// so a REDUCE with consumer_count > 1 fails the chain check
// independently and the consumer_count == 1 fast-reject is just
// an optimisation.  Without it, multi-consumer REDUCEs whose
// branches all bottom out at the same EXPAND would be considered.

static u32 bufferize_rule_inline_softmax_broadcast_reduce(Term root) {
  (void)root;
  if (!bufferize_reduce_fuse_multi_enabled() && bufferize_reduce_count() != 1) {
    return 0;
  }
  int tile_cap_on = bufferize_softmax_reduce_tile_cap_enabled();
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->op != UOP_REDUCE)    continue;
    if (!bufferize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
    if (tile_cap_on) {
      // Tile-feasibility gate: the materializer's "single REDUCE
      // per kernel" rule lets the kernel ROOT be a REDUCE
      // unconditionally, but bails any non-root REDUCE encountered
      // during visit().  When the absorbing kernel root for THIS
      // REDUCE is itself a REDUCE (BN-mean -> EXPAND -> ... ->
      // outer REDUCE), inlining produces a kernel program with
      // two REDUCE ops; rangeify_try_lower_elementwise then bails
      // ("> 1 reduce") and dispatch falls off the tile-jit path
      // onto the per-op metal encoder.  Keep this REDUCE realized
      // when the absorbing root is itself a REDUCE so the
      // resulting kernels stay tile-feasible.
      u64 abs_loc = bufferize_absorbing_boundary(info->loc, 16);
      if (abs_loc != 0) {
        u32 abs_idx = bufferize_info_find(abs_loc);
        if (abs_idx != 0xFFFFFFFFu
            && BUFFERIZE_NODES[abs_idx].op == UOP_REDUCE) {
          if (getenv("THVM_DUMP_SOFTMAX_REDUCE_CAP")) {
            fprintf(stderr,
                    "softmax-reduce-cap: kept realized loc=%llu"
                    " abs=%llu (REDUCE root)\n",
                    (unsigned long long)info->loc,
                    (unsigned long long)abs_loc);
          }
          continue;
        }
      }
    }
    if (info->realized) hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

// Detect UOP_REDUCE(SUM, UOP_MUL(A, B)) with A != B -- matmul-shape
// reduce.  Used to protect matmul outputs from inline-* rules so the
// matmul kernel stays bufferized as a clean 2-input/2-op kernel that
// tile_analyze_gemm accepts (-> metal-gemm dispatch with TC).
// Diagnosed in Levels 27-34 of docs/plans/autotune_beam_profile.md
// (40x wall gap on transformer FFN kids 14/16).
static int bufferize_uop_is_matmul(u64 reduce_loc) {
  Term mul = term_resolve(heap_read(reduce_loc + 0));
  int is_mul = (term_tag(mul) == TAG_UOP) && (term_ext(mul) == UOP_MUL);
  int distinct = 0;
  if (is_mul) {
    u64 mul_loc = term_val(mul);
    Term a = term_resolve(heap_read(mul_loc + 0));
    Term b = term_resolve(heap_read(mul_loc + 1));
    distinct = (term_val(a) != term_val(b));
  }
  char const *e = getenv("THVM_DUMP_MATMUL_DETECT");
  if (e != NULL && e[0] == '1') {
    fprintf(stderr,
            "matmul-detect: reduce_loc=%llu src0_op=%u is_mul=%d distinct=%d -> %d\n",
            (unsigned long long)reduce_loc,
            (unsigned)(term_tag(mul) == TAG_UOP ? term_ext(mul) : 0xFF),
            is_mul, distinct, is_mul && distinct);
  }
  return is_mul && distinct;
}

static u32 bufferize_rule_inline_reduce_scalar_tail(Term root) {
  if (term_tag(root) != TAG_UOP) return 0;
  if (!bufferize_rangeify_enabled()) return 0;
  if (!bufferize_reduce_fuse_multi_enabled() && bufferize_reduce_count() != 1) {
    return 0;
  }
  u64 root_loc = term_val(root);
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->op != UOP_REDUCE)    continue;
    if (info->consumer_count != 1) continue;
    if (bufferize_uop_is_matmul(info->loc)) continue;
    if (!bufferize_reduce_consumer_is_scalar_tail(info->loc, root_loc)) continue;
    if (info->realized) hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

fn void bufferize_classify(Term root) {
  bufferize_info_clear();
  bufferize_rewrite_stats_clear();
  // Always touch the bufferize graph so its state stays in sync with
  // BUFFERIZE_NODES.  Both seed/finalize short-circuit on non-UOp roots
  // after zeroing their own tables.
  if (term_tag(root) != TAG_UOP) {
    bufferize_seed_from_nodes(root);
    bufferize_finalize_stores(root);
    return;
  }
  if (term_ext(root) == UOP_KERNEL) {
    bufferize_seed_from_nodes(root);
    bufferize_finalize_stores(root);
    return;
  }

  // Bitmap sized to HEAP_NEXT (current high-water of the dyn
  // heap) instead of HEAP_CAP -- per-call cost stays
  // proportional to live work, not the 16 MiB max.
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;
  bufferize_walk_rec(root, visited);
  free(visited);

  // Rule (a): the root itself realizes.
  u32 root_idx = bufferize_info_find(term_val(root));
  if (root_idx != 0xFFFFFFFFu) {
    bufferize_node_mark(&BUFFERIZE_NODES[root_idx], BUFFERIZE_REASON_ROOT);
  }

  // Rules (b) + (c): multi-consumer or REDUCE.  Later named rules
  // can clear the final `realized` bit while preserving these
  // reason bits for diagnostics and tests.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->consumer_count >= 2) {
      bufferize_node_mark(info, BUFFERIZE_REASON_MULTI);
    }
    if (info->op == UOP_REDUCE) {
      bufferize_node_mark(info, BUFFERIZE_REASON_REDUCE);
    }
  }
  RealizeRewriteRule rules[] = {
    {"inline-constants",              bufferize_rule_inline_constants},
    {"inline-adjacent-reduce-chains", bufferize_rule_inline_adjacent_reduce_chains},
    {"inline-softmax-broadcast-reduce", bufferize_rule_inline_softmax_broadcast_reduce},
    {"inline-reduce-scalar-tail",     bufferize_rule_inline_reduce_scalar_tail},
    {"inline-large-expand-fanout",    bufferize_rule_inline_large_expand_fanout},
    {"inline-reduce-fanout",          bufferize_rule_inline_reduce_fanout},
    {"remove-removable-bufferize",    bufferize_rule_remove_removable_bufferize},
    {"remove-by-cost-score",          bufferize_rule_remove_by_cost_score},
    {"inline-pure-fanout-probe",      bufferize_rule_inline_pure_fanout_probe},
    {"metal-tile-fanin-cap",          bufferize_rule_metal_tile_fanin_cap},
  };
  // Phase 1: snapshot the seeded BUFFERIZE_NODES into the bufferize
  // graph, then run the named rewrite rules.  bufferize_node_mark and
  // bufferize_node_unmark forward into bufferize_realize_with_reason and
  // bufferize_unrealize so each rule's effect is recorded as
  // added_by/removed_by on the explicit graph.  Finalize the store
  // table after rewrites land.
  bufferize_seed_from_nodes(root);
  bufferize_rewrite_apply(root, rules, (u32)(sizeof(rules) / sizeof(rules[0])));
  bufferize_rewrite_stats_dump();
  bufferize_finalize_stores(root);
}

fn u8 bufferize_is_realized(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) return 0;
  if (term_ext(uop_term) == UOP_KERNEL) return 1;   // already realized
  u32 idx = bufferize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) return 0;
  return BUFFERIZE_NODES[idx].realized;
}

fn u32 bufferize_consumer_count(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) return 0;
  u32 idx = bufferize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) return 0;
  return BUFFERIZE_NODES[idx].consumer_count;
}

fn u32 bufferize_reasons(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) {
    return 0;
  }
  u32 idx = bufferize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) {
    return 0;
  }
  return BUFFERIZE_NODES[idx].reasons;
}
