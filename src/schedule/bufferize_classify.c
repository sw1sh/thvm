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

CMapNode CMAP_LL    [CMAP_LL_CAP];
u32      CMAP_LL_LEN = 0;

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
  BUFFERIZE_NODES[idx].cmap_head      = 0xFFFFFFFFu;
  BUFFERIZE_NODES[idx].op             = op;
  BUFFERIZE_NODES[idx].realized       = 0;
  bufferize_node_hash_insert(loc, idx);
  return idx;
}

static void bufferize_cmap_add(u32 producer_idx, u64 consumer_loc) {
  if (producer_idx == 0xFFFFFFFFu) return;
  if (CMAP_LL_LEN >= CMAP_LL_CAP) return;     // cap overflow: dropped silently; consumer_count is still authoritative
  u32 slot = CMAP_LL_LEN++;
  CMAP_LL[slot].consumer_loc = consumer_loc;
  CMAP_LL[slot].next         = BUFFERIZE_NODES[producer_idx].cmap_head;
  BUFFERIZE_NODES[producer_idx].cmap_head = slot;
}

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
  // Matmul reduces stay realized: we want them as their own kernel so
  // dispatch routes through metal-gemm-with-TC.
  if (info->reasons & BUFFERIZE_REASON_MATMUL) {
    info->reasons |= reason;
    return;
  }
  info->realized = 0;
  info->reasons |= reason;
}

// Find the first UOP node reachable from a Term, descending through any
// chain of TAG_DP0/TAG_DP1 cells (cell[0]/cell[1] slots).  Returns 0 if
// no UOP is found within bound or a cycle is hit.  Used so cmap can
// connect a UOP-child's location across grad-projection cells (TGrad's
// reverse-mode chain bridges REDUCE -> DP1+GRAD -> ... -> REDUCE; without
// this, the chain-guard's BFS can't trace inter-reduce iter deps in the
// backward pass).  visited_dls is an array of seen DUP-cell locs to
// break cycles; cap is its capacity.
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
    // Resolve VAR (SUB-bit) + ALO chains so a body post-APP-LAM
    // beta exposes the bound argument's UOP rather than the bare
    // VAR cell -- mirrors visit() in materialize.c so the walker
    // and the kernel walker agree on what's a kernel boundary.
    Term raw = heap_read(loc + i);
    Term child = term_resolve(raw);
    u8 ctag = term_tag(child);
    // When the child is a DP projection (grad-flavored or otherwise),
    // chase through cell[0]/cell[1] to find the first UOP and add a
    // cmap edge from THIS node to it.  No bump to consumer_count --
    // only the cmap edge (the chain-guard's BFS reader) needs to see
    // the connection; OLD-path MULTI-seed thresholds stay unchanged.
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

// Shared UPat: scalar-preserving unary (1-arity op in the {NEG,
// RECIP, SQRT, EXP2, LOG2} set).  Used by every chain-walker hop
// predicate that asks "does this hop keep the post-reduce shape?".
// No bindings, no nested src; structural test is the op-set match
// alone.
static u8 const bufferize_upat_scalar_unary_alt[] = {
  UOP_NEG, UOP_RECIP, UOP_SQRT, UOP_EXP2, UOP_LOG2, 0
};
static UPat const bufferize_upat_scalar_unary = {
  0, 1, 0, -1, NULL, bufferize_upat_scalar_unary_alt
};

// Shared UPat: movement-passthrough (RESHAPE/PERMUTE) -- layout-only
// movement ops that keep the scalar value at every coordinate.
// Used by chain-walker hop predicates as a TERMINAL "yes, this hop
// is shape-passthrough" check.
static u8 const bufferize_upat_movement_passthrough_alt[] = {
  UOP_RESHAPE, UOP_PERMUTE, 0
};
static UPat const bufferize_upat_movement_passthrough = {
  0, 1, 0, -1, NULL, bufferize_upat_movement_passthrough_alt
};

// Shared UPat: any movement op (the full set including value-
// preserving and value-shaping ops).  Used to walk THROUGH the LN
// broadcast layout (EXPAND(RESHAPE(...))) when looking upstream for
// the elementwise op that should be split off the matmul kernel.
// Each of these 6 ops stores its source at heap[loc + 0], so the
// walker can step `arg = heap_read(term_val(arg))` regardless of
// which movement op matched.
static u8 const bufferize_upat_movement_any_alt[] = {
  UOP_RESHAPE, UOP_PERMUTE, UOP_EXPAND, UOP_PAD, UOP_SHRINK, UOP_FLIP, 0
};
static UPat const bufferize_upat_movement_any = {
  0, 1, 0, -1, NULL, bufferize_upat_movement_any_alt
};

// Shared UPat: {UOP_ADD, UOP_MUL}(?0, ?1) -- "ALU with two children",
// both captured.  Used by the chain-walker hop predicates that need
// to test the sibling against a const-y wrapper (broadcast-of-CONST
// vs plain CONST -- both callers have their own predicate on the
// captured operands).  op_alt encodes the disjunction so the same
// pattern catches ADD and MUL in one match call.
static u8 const bufferize_upat_alu2_alt[] = {UOP_ADD, UOP_MUL, 0};
static UPat const bufferize_upat_alu2_children[2] = {
  {0, 0xFF, 0, 0, NULL, NULL},
  {0, 0xFF, 0, 1, NULL, NULL},
};
static UPat const bufferize_upat_alu2 = {
  0, 2, 0, -1, bufferize_upat_alu2_children, bufferize_upat_alu2_alt
};

// Walk a single chain branch starting from a *given* parent of a
// reduce (or any other node) and verify it ends in EXPAND through
// scalar-preserving ops.  Mirrors the historical
// Test whether the op at `cur` is a scalar-preserving chain hop -
// either a unary scalar function, a layout-only movement op (RESHAPE/
// PERMUTE), or an ALU op whose other operand is a (possibly broadcast)
// constant.  `cur` is the loc of the current node; the predicate must
// inspect the node's children for the ALU case.
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

// === Metal fanin-cap split ==========================================
// The metal renderer declines kernels with > 30 inputs (render_metal.c
// gate at n_inputs > 30 -> NULL).  Without an upstream split, a wide
// ADD/MUL tree (Adam updates folding 30+ tensors, BN-train backward
// reducing many feature groups) lands at the render decline and falls
// onto the per-op interpreter.  This rule walks each realized
// boundary's expression tree, counts distinct realized inputs, and if
// the total exceeds `cap` greedily marks the largest split-class
// (elementwise / movement) child as realized to split the fan-in.
// Iterates until either the boundary fits under cap or no eligible
// child remains.
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

static u32 bufferize_rule_inline_softmax_broadcast_reduce(Term root) {
  (void)root;
  if (!bufferize_reduce_fuse_multi_enabled() && bufferize_reduce_count() != 1) {
    return 0;
  }
  int tile_cap_on = bufferize_softmax_reduce_tile_cap_enabled();
  // Track per-absorbing-boundary how many REDUCEs we've already un-marked
  // for inlining into that same kernel root.  The materializer's visit()
  // bails when a non-root REDUCE is added to a kernel that already
  // contains one.  Two sibling branches in an ADD/MUL root that each
  // host a broadcast-fed REDUCE chain (the canonical BN-train BWD
  // shape: ADD(reduce_chain_left, reduce_chain_right)) would otherwise
  // both lose their realized bit and produce a kernel with two REDUCE
  // ops -> materialize BAIL -> result returned as un-compiled UOP ->
  // TRealize NotATensor.  Cap at 1 un-mark per absorbing boundary;
  // the rest stay realized as their own boundary kernels.
  // abs_locs[] tracks distinct absorbing-boundary roots we've already
  // un-marked at least once.  Deep backward chains (BN-on-BN + MaxPool
  // backward through Conv2D) emit far more than the previous 64-slot
  // budget -- W2-grad in beautiful_mnist BS=15 needs 500+ distinct
  // boundaries with REDUCE children.  When the table overflows,
  // `slot` comes back 0xFFFFFFFFu and the per-boundary cap check
  // silently no-ops -- letting all sibling REDUCEs un-mark into the
  // same kernel root and tripping the materializer's "one REDUCE per
  // kernel" invariant (VISIT_BAIL @ materialize.c:2110), which makes
  // the whole gradient kernel return Missing[NotATensor, UOP].
  // 1024 slots = 16 KB stack, comfortable for the stack frame.
  u32 abs_cap = 16;
  u64 abs_locs[1024];
  u32 abs_unmark_count[1024];
  u32 n_abs = 0;
  (void)abs_cap;
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (info->op != UOP_REDUCE)    continue;
    if (!bufferize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
    u64 abs_loc = 0;
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
      abs_loc = bufferize_absorbing_boundary(info->loc, 16);
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
      // Multi-REDUCE per absorbing-boundary cap: same kernel root cannot
      // host more than one inlined REDUCE.  Look up `abs_loc`'s slot in
      // the per-call counter table; if its count is already >= 1, skip
      // un-marking THIS reduce (it stays realized as its own boundary).
      if (abs_loc != 0) {
        u32 slot = 0xFFFFFFFFu;
        for (u32 k = 0; k < n_abs; k++) {
          if (abs_locs[k] == abs_loc) { slot = k; break; }
        }
        if (slot == 0xFFFFFFFFu) {
          if (n_abs < (sizeof abs_locs) / sizeof abs_locs[0]) {
            slot = n_abs++;
            abs_locs[slot] = abs_loc;
            abs_unmark_count[slot] = 0;
          }
        }
        // Cap was originally 1 per absorbing boundary to dodge the
        // materializer's "one REDUCE per kernel" gate (now removed).
        // With multi-REDUCE permitted in a single kernel
        // (materialize.c, rangeify scalar_uops), the cap is obsolete:
        // any number of broadcast-fed REDUCE chains in the same
        // absorbing-boundary root can fuse.  Default to UINT32_MAX
        // (effectively unlimited); THVM_REDUCE_UNMARK_CAP=0..9 caps it
        // explicitly for bisecting if a regression appears.
        u32 cap_limit = (u32)-1;
        char const *_cap_e = getenv("THVM_REDUCE_UNMARK_CAP");
        if (_cap_e != NULL && _cap_e[0] >= '0' && _cap_e[0] <= '9') {
          cap_limit = (u32)(_cap_e[0] - '0');
        }
        if (slot != 0xFFFFFFFFu && abs_unmark_count[slot] >= cap_limit) {
          if (getenv("THVM_DUMP_SOFTMAX_REDUCE_CAP")) {
            fprintf(stderr,
                    "softmax-reduce-cap: kept realized loc=%llu"
                    " abs=%llu (sibling REDUCE already inlined, cap=%u)\n",
                    (unsigned long long)info->loc,
                    (unsigned long long)abs_loc, cap_limit);
          }
          continue;
        }
        if (slot != 0xFFFFFFFFu) abs_unmark_count[slot]++;
      }
    }
    if (info->realized) hits++;
    bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
  }
  return hits;
}

// Shared UPat: UOP_MUL(?0, ?1) -- both operands captured.  Used by
// the matmul recognizer (distinctness check on bindings) and by the
// matmul-input-protect marker (gate-and-mark on bindings).
static UPat const bufferize_upat_mul_children[2] = {
  {0, 0xFF, 0, 0, NULL},
  {0, 0xFF, 0, 1, NULL},
};
static UPat const bufferize_upat_mul = {
  UOP_MUL, 2, 0, -1, bufferize_upat_mul_children
};

// Detect UOP_REDUCE(SUM, UOP_MUL(A, B)) with A != B -- matmul-shape
// reduce.  Used to protect matmul outputs from inline-* rules so the
// matmul kernel stays bufferized as a clean 2-input/2-op kernel that
// tile_analyze_gemm accepts (-> metal-gemm dispatch with TC).
// Diagnosed in Levels 27-34 of docs/plans/autotune_beam_profile.md
// (40x wall gap on transformer FFN kids 14/16).  The structural part
// (UOP_MUL with 2 children) goes through the declarative UPat layer;
// the distinctness check remains a post-match guard so the
// diagnostic can keep its prior structure.
static int bufferize_uop_is_matmul(u64 reduce_loc) {
  Term mul = term_resolve(heap_read(reduce_loc + 0));
  Term bindings[UPAT_NUM_BINDINGS] = {0};
  int is_mul   = upat_match(&bufferize_upat_mul, mul, bindings);
  int distinct = is_mul && (term_val(bindings[0]) != term_val(bindings[1]));

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

  // When the unified rangeify pass is enabled, we replace the OLD-path
  // seeds (multi-consumer / REDUCE / matmul / matmul-input-protect) AND
  // the 11 named rewrite rules with one call to run_rangeify_unified.
  // The unified pass mirrors tinygrad/schedule/indexing.py:run_rangeify
  // which only seeds COPY/CONTIGUOUS/STORE (none of which exist in
  // thvm today) -- everything else is decided by the consumer-divergence
  // walk. The root is already seeded above.
  //
  // After the walk we project RU_REALIZE_MAP back onto
  // BUFFERIZE_NODES.realized so downstream consumers (materialize.c,
  // bufferize.c, the kernel walker) observe the new decisions through
  // the same side-table they already read.
    // OLD-path multi-consumer / REDUCE / matmul seeds stay.  Dropping
    // REDUCE here without lowered-DAG materialize.c support breaks every
    // reduce-bearing topology (MaxPool/Softmax/BN-train/CE); the seed
    // removal lands together with the materialize.c rewrite that walks
    // UOP_BUFFERIZE directly.
    //
    // Mirror context: tinygrad/schedule/indexing.py:28-35
    // (pm_generate_realize_map realizes COPY/CONTIGUOUS/STORE only;
    //  REDUCE realize emerges from ending_ranges + consumer-divergence
    //  inside run_rangeify).
    for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
      UOpInfo *info = &BUFFERIZE_NODES[i];
      if (info->consumer_count >= 2) {
        bufferize_node_mark(info, BUFFERIZE_REASON_MULTI);
      }
      if (info->op == UOP_REDUCE) {
        // Only MATMUL REDUCEs stay as boundaries (mirrors tinygrad's
        // matmul-protect).  Other REDUCEs fuse inline via the unified
        // pass's REDUCE-via-RANGE expansion + render_uop's _accN
        // accumulator hoist.
        //
        // Probe_w2 (synthetic BN-train backward) showed the cmap-based
        // chain-guard misses some inter-reduce iter dependencies in
        // larger backward graphs -- grad came out 0 with the chain-guard
        // active, correct with always-keep.  Since the kernel-count
        // savings of the chain-guard (~13%) don't outweigh the
        // correctness risk, conservatively keep ALL non-matmul REDUCEs
        // as boundaries too.  The fix path is in render_uop's
        // nested-reduce-iter handler (file as task #21).
        if (bufferize_uop_is_matmul(info->loc)) {
          bufferize_node_mark(info, BUFFERIZE_REASON_REDUCE);
          bufferize_node_mark(info, BUFFERIZE_REASON_MATMUL);
        } else {
          bufferize_node_mark(info, BUFFERIZE_REASON_REDUCE);
        }
      }
    }
    // Softmax-style REDUCE unmark: a REDUCE whose every consumer chain
    // bottoms out at EXPAND through scalar-preserving hops is the
    // broadcast-back-to-vector pattern (softmax sum, BN-mean, mean/N
    // -- see bufferize_reduce_consumer_is_broadcast_chain for the
    // exact chain shape). Tinygrad's pm_generate_realize_map only
    // realizes REDUCEs whose ranges include AxisType.OUTER; thvm has
    // no OUTER axis tag, so we approximate the same predicate
    // structurally here: drop the REDUCE seed for chains that re-broadcast
    // through EXPAND, leaving the unified pass's REDUCE-via-RANGE
    // expansion + render_uop's _accN accumulator hoist to fuse them
    // inline. Skip when the absorbing kernel root is itself a REDUCE
    // (rangeify_try_lower_elementwise bails > 1 REDUCE per kernel) and
    // honour the per-boundary unmark cap so BN-train backward's
    // sibling-REDUCE branches don't all collapse into one kernel.
    // Mirror context: tinygrad/schedule/indexing.py:31 (REDUCE realize
    // predicate). The chain-walk + tile-cap + per-boundary cap helpers
    // (bufferize_reduce_consumer_is_broadcast_chain,
    // bufferize_absorbing_boundary,
    // bufferize_softmax_reduce_tile_cap_enabled,
    // bufferize_reduce_fuse_multi_enabled) stay as-is.
    int tile_cap_on = bufferize_softmax_reduce_tile_cap_enabled();
    int fuse_multi_on = bufferize_reduce_fuse_multi_enabled();
    if (fuse_multi_on || bufferize_reduce_count() == 1) {
      u64 abs_locs[1024];
      u32 abs_unmark_count[1024];
      u32 n_abs = 0;
      for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
        UOpInfo *info = &BUFFERIZE_NODES[i];
        if (info->op != UOP_REDUCE) continue;
        if (!info->realized) continue;
        if (info->reasons & BUFFERIZE_REASON_MATMUL) continue;
        if (!bufferize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
        u64 abs_loc = 0;
        if (tile_cap_on) {
          abs_loc = bufferize_absorbing_boundary(info->loc, 16);
          if (abs_loc != 0) {
            u32 abs_idx = bufferize_info_find(abs_loc);
            if (abs_idx != 0xFFFFFFFFu
                && BUFFERIZE_NODES[abs_idx].op == UOP_REDUCE) {
              continue;
            }
          }
          if (abs_loc != 0) {
            u32 slot = 0xFFFFFFFFu;
            for (u32 k = 0; k < n_abs; k++) {
              if (abs_locs[k] == abs_loc) { slot = k; break; }
            }
            if (slot == 0xFFFFFFFFu) {
              if (n_abs < (sizeof abs_locs) / sizeof abs_locs[0]) {
                slot = n_abs++;
                abs_locs[slot] = abs_loc;
                abs_unmark_count[slot] = 0;
              }
            }
            u32 cap_limit = (u32)-1;
            char const *_cap_e = getenv("THVM_REDUCE_UNMARK_CAP");
            if (_cap_e != NULL && _cap_e[0] >= '0' && _cap_e[0] <= '9') {
              cap_limit = (u32)(_cap_e[0] - '0');
            }
            if (slot != 0xFFFFFFFFu && abs_unmark_count[slot] >= cap_limit) {
              continue;
            }
            if (slot != 0xFFFFFFFFu) abs_unmark_count[slot]++;
          }
        }
        bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
      }
    }
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
      if (BUFFERIZE_NODES[cidx].op != UOP_REDUCE) continue;
      if (!BUFFERIZE_NODES[cidx].realized) continue;
      // bufferize_unwrap_dp follows DP cells across SUB-marked fired
      // dups too -- accept any chain whose outer.src[0] resolves to
      // this inner UOP after DUP traversal.  The CPU walker handles
      // DUP-traversed addr / body expressions correctly because
      // term_resolve in uwalk_eval_* unwraps SUB-marked cells; the
      // chain-guard's old cmap-BFS limitation no longer applies once
      // execution is via uwalk_run_reduce (which re-runs the inner per
      // outer iteration) rather than the legacy hoist-cache lookup.
      u64 visited[8];
      u32 n_visited = 0;
      Term outer_src = bufferize_unwrap_dp(heap_read(consumer_locs[0] + 0),
                                            visited, &n_visited, 8, 16);
      if (outer_src == 0 || term_tag(outer_src) != TAG_UOP) continue;
      if (term_val(outer_src) != info->loc) continue;
      bufferize_node_unmark(info, BUFFERIZE_REASON_INLINE);
    }
    // Fanin-cap split: mark wide-fanin elementwise/movement children
    // as realized boundaries so the metal renderer's n_inputs > 30
    // decline doesn't trigger.  Runs after softmax-unmark so the
    // boundary set reflects every other rule first.
    bufferize_rule_metal_tile_fanin_cap(root);
    bufferize_seed_from_nodes(root);
    // The unified walk writes RU_RANGE_MAP / RU_REALIZE_MAP and the
    // main-heap UOP_BUFFERIZE Terms (one per realize boundary).
    run_rangeify_unified(root);
    bufferize_finalize_stores(root);
    return;
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
