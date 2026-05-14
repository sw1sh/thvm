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
  // Matmul reduces stay realized: we want them as their own kernel so
  // tile_analyze_gemm passes (clean 2-input/2-op program) and dispatch
  // routes through metal-gemm-with-TC instead of metal-tile (~40x).
  // Diagnosed in Levels 27-36 of docs/plans/autotune_beam_profile.md.
  if (info->reasons & BUFFERIZE_REASON_MATMUL) {
    info->reasons |= reason;
    return;
  }
  info->realized = 0;
  info->reasons |= reason;
  bufferize_unrealize(info->loc);
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

// Project the unified pass's UOP_BUFFERIZE emission onto
// BUFFERIZE_NODES.realized so materialize.c's BOUNDARY_ORDER walker
// (topo_sort_boundaries) picks up the new boundary set without changing
// its read API.
//
// Mirror source: tinygrad/schedule/indexing.py:56-81
// (create_bufferize_and_index_based_on_ranges produces the BUFFERIZE
//  Term; the scheduler consumes its presence in tsink as the "realize
//  boundary" signal).
//
// We additionally mark the boundary with BUFFERIZE_REASON_UNIFIED so the
// removal rules can introspect "this realize came from the unified pass"
// vs "carried over from a multi-consumer seed".  We do NOT clear
// pre-existing .realized bits: the MULTI seed and any ROOT seed must
// survive; the removal rules below handle the unmarking.
//
// Disabled: the projection over-realizes broadcast-after-reduce patterns
// (softmax / attention regress under this projection because the unified
// pass marks every UOP_BUFFERIZE Term as a boundary, but the OLD-path
// emit walker can't render the resulting kernel-graph topology for those
// shapes).  Until the materialize.c walker reads UOP_BUFFERIZE Terms
// directly off the lowered DAG, the OLD-path heuristics
// (multi-consumer / REDUCE / matmul + named removal rules) remain
// authoritative.  The substrate (UOP_BUFFERIZE allocator, run_rangeify_
// unified, topo_sort_buffers_unified, KernelEntry.compute_bufferize)
// stays alive for the eventual cut.
static void bufferize_classify_project_unified(void) {
  (void)rangeify_unified_bufferize_at;
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
  if (rangeify_unified_enabled()) {
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
    bufferize_seed_from_nodes(root);
    // The unified walk writes RU_RANGE_MAP / RU_REALIZE_MAP and the
    // main-heap UOP_BUFFERIZE Terms (one per realize boundary).
    run_rangeify_unified(root);
    // Project every UOP_BUFFERIZE the unified pass emitted onto
    // BUFFERIZE_NODES.realized so materialize.c's existing
    // BOUNDARY_ORDER walker picks them up.  Partial-realize cases come
    // from consumer-divergence + ending-ranges
    // (run_rangeify_unified.c:493-560); RU_BUFFERIZE_TERM != 0 means
    // "kernel boundary".
    bufferize_classify_project_unified();
    // The unified pass's consumer-divergence + ending-ranges machinery
    // + the chain-guards above already encode most of the realize-set
    // tinygrad would emit.  inline-softmax-broadcast-reduce stays as
    // the one keep -- the unified pass doesn't yet sharpen the softmax
    // max -> exp -> sum fusion into 2 kernels (fusion_count test 1/8
    // regresses without this).
    RealizeRewriteRule direct_rules[] = {
      {"inline-softmax-broadcast-reduce", bufferize_rule_inline_softmax_broadcast_reduce},
    };
    bufferize_rewrite_apply(root, direct_rules,
        (u32)(sizeof(direct_rules) / sizeof(direct_rules[0])));
    // Re-run pm_apply_rangeify so RU_BUFFERIZE_TERM / RU_STORE_ROOT /
    // RU_SUBST observe the post-prune realize set. Without this rerun,
    // realized consumers' store-root DAGs still reference BUFFERIZE Terms
    // for producers whose realized bit was cleared by the prune rules
    // (e.g. inline-softmax-broadcast-reduce). The unified-bypass walker
    // (materialize.c:unified_rewrite_buffer_for_bufferize) then fails to
    // map those stale BUFFERIZE leaves to any kernel-input slot and
    // cpu_uop_walk's uwalk_resolve_buf inst-zero fallback silently aliases
    // them to the output buffer, producing NaN / garbage in dispatch.
    //
    // Only the THVM_LIFT_FROM_UNIFIED bypass observes RU_STORE_ROOT; the
    // default cached_lift path is unaffected (its kernel_lift_to_uop walk
    // never sees these BUFFERIZE Terms because it lifts the per-kernel
    // scalar arena, which is already inlined).
    if (getenv("THVM_LIFT_FROM_UNIFIED")) {
      rangeify_unified_resync_realize_from_nodes();
      pm_apply_rangeify(root);
    }
    bufferize_finalize_stores(root);
    return;
  }

  // === OLD path (opt-out via THVM_UNIFIED_RANGEIFY=0) ===

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
      if (bufferize_uop_is_matmul(info->loc)) {
        bufferize_node_mark(info, BUFFERIZE_REASON_MATMUL);
      }
    }
  }
  // Matmul-input-protect: walk upstream one hop from each matmul
  // reduce's MUL operands.  When an input is an elementwise op
  // (ADD/SUB/MUL/etc) that would otherwise get inlined into the
  // matmul kernel, mark it realized so it becomes its own boundary
  // and visit() routes it as input.  Without this, materialize's
  // visit() absorbs upstream bias-add chains into the matmul kernel,
  // bloating n_ops/n_inputs past the strict tile_analyze_gemm gates
  // and forcing the slow metal-tile dispatch.  Structural recognition
  // (UOP_MUL with 2 children, captured at slots 0/1) goes through
  // upat_match; the ADD-or-MUL gate + side-effect marking on
  // BUFFERIZE_NODES stay imperative because they touch the side
  // channel rather than the heap DAG.
  //
  // SIZE GATE (Level 56): the protect pays off only when the
  // resulting matmul is big enough to amortize the launch overhead
  // of the +1 split-off elementwise kernel (~150-200us metal-gemm
  // launch on M3 Max).  Use the MUL's shape numel = M*N*K (matmul
  // flops) as the gate -- not the REDUCE output (M*N) -- because
  // it captures the actual GEMM work that metal-gemm-with-TC
  // accelerates.  Transformer kids 4/5/6/14 are 32*64*64 = 131k;
  // kid 19 is 32*64*256 = 524k.  Lenet's fc layers are
  // 1*120*400 = 48k (fc1), 1*84*120 = 10k (fc2), 1*10*84 = 840
  // (fc3).  Threshold 100000 keeps all transformer matmuls but
  // excludes lenet's tiny fc layers.
  // Override via THVM_MATMUL_PROTECT_MIN_FLOPS.
  u64 protect_min_flops = 100000;
  {
    char const *e = getenv("THVM_MATMUL_PROTECT_MIN_FLOPS");
    if (e != NULL) protect_min_flops = (u64)strtoull(e, NULL, 10);
  }
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *info = &BUFFERIZE_NODES[i];
    if (!(info->reasons & BUFFERIZE_REASON_MATMUL)) continue;
    {
      // The MUL is heap[reduce_loc + 0]; its shape numel = M*N*K.
      Term mul_term = term_resolve(heap_read(info->loc + 0));
      Shape s; u64 mul_numel = 0;
      if (term_shape_in(mul_term, 0, &s)) {
        mul_numel = 1;
        for (u32 d = 0; d < s.ndim; d++) mul_numel *= s.dims[d];
      }
      if (mul_numel < protect_min_flops) continue;
    }
    Term mul = term_resolve(heap_read(info->loc + 0));
    Term bindings[UPAT_NUM_BINDINGS] = {0};
    if (!upat_match(&bufferize_upat_mul, mul, bindings)) continue;
    for (u32 a = 0; a < 2; a++) {
      Term arg = bindings[a];
      // Walk past movement ops (RESHAPE/EXPAND/PERMUTE/SHRINK/PAD/
      // FLIP) to find the underlying elementwise parent.  Without
      // this, an LN-style `EXPAND(RESHAPE(ADD(...)))` operand hides
      // the ADD from the protect pass; the ADD then gets absorbed
      // into the matmul kernel and inflates n_inputs past the
      // tile_analyze_gemm gate (Level 47 transformer kid 14
      // diagnosis).  Each movement op stores src at heap[loc+0]
      // so the step is uniform.  Bounded by 8 hops to match the
      // existing chain walker.
      for (u32 hops = 0; hops < 8; hops++) {
        if (term_tag(arg) != TAG_UOP) break;
        if (!upat_match(&bufferize_upat_movement_any, arg, NULL)) break;
        arg = term_resolve(heap_read(term_val(arg) + 0));
      }
      if (term_tag(arg) != TAG_UOP) continue;
      u8 arg_op = term_ext(arg);
      if (arg_op != UOP_ADD && arg_op != UOP_MUL) continue;
      u32 arg_idx = bufferize_info_find(term_val(arg));
      if (arg_idx == 0xFFFFFFFFu) continue;
      bufferize_node_mark(&BUFFERIZE_NODES[arg_idx],
                          BUFFERIZE_REASON_MATMUL);
    }
  }
  RealizeRewriteRule rules[] = {
    {"inline-softmax-broadcast-reduce", bufferize_rule_inline_softmax_broadcast_reduce},
  };
  // Snapshot the seeded BUFFERIZE_NODES into the bufferize graph,
  // then run the named rewrite rules.  bufferize_node_mark and
  // bufferize_node_unmark forward into bufferize_realize_with_reason
  // and bufferize_unrealize so each rule's effect is recorded as
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
