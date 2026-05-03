// schedule/realize_classify.c - decide which UOPs in a graph
//                                must "realize" into a backing
//                                buffer (a tinygrad CONTIGUOUS).
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
// selective materializer consults realize_is_realized to decide
// whether to emit a UOP_KERNEL or inline the compute into its
// consumer kernel's program.
//
// Note: this pass DOES NOT mutate the heap or the kernel
// table.  It only reads the UOp DAG and populates the table.
// Aliasing of duplicate child references inside a single
// parent (e.g., MUL[x, x]) counts as ONE consumer of x, since
// the materializer dedups.

// REALIZE_INFO_CAP + UOpInfo struct + REALIZE_INFO/REALIZE_INFO_LEN
// declared in thvm.h so materialize.c can iterate the table.

UOpInfo REALIZE_INFO    [REALIZE_INFO_CAP];
u32     REALIZE_INFO_LEN = 0;

// Open-addressed hash table mapping loc -> REALIZE_INFO index.
// Without this, realize_info_find did a linear scan of REALIZE_INFO,
// which made realize_classify O(N^2) for the N UOPs in a recursive
// training-loop graph and the dominant cost of long bound-w realizes.
// Cap is the next power of two >= REALIZE_INFO_CAP for cheap masking.
#define REALIZE_INFO_HASH_CAP (1u << 16)   // 64K slots, REALIZE_INFO_CAP = 16K
#define REALIZE_INFO_HASH_EMPTY 0xFFFFFFFFu
static u32 REALIZE_INFO_HASH[REALIZE_INFO_HASH_CAP];

static inline u32 realize_info_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (REALIZE_INFO_HASH_CAP - 1);
}

fn void realize_info_clear(void) {
  REALIZE_INFO_LEN = 0;
  for (u32 i = 0; i < REALIZE_INFO_HASH_CAP; i++)
    REALIZE_INFO_HASH[i] = REALIZE_INFO_HASH_EMPTY;
}

fn u32 realize_info_find(u64 loc) {
  u32 h = realize_info_hash(loc);
  for (u32 probe = 0; probe < REALIZE_INFO_HASH_CAP; probe++) {
    u32 i = (h + probe) & (REALIZE_INFO_HASH_CAP - 1);
    u32 idx = REALIZE_INFO_HASH[i];
    if (idx == REALIZE_INFO_HASH_EMPTY) return 0xFFFFFFFFu;
    if (REALIZE_INFO[idx].loc == loc) return idx;
  }
  return 0xFFFFFFFFu;
}

static void realize_info_hash_insert(u64 loc, u32 idx) {
  u32 h = realize_info_hash(loc);
  for (u32 probe = 0; probe < REALIZE_INFO_HASH_CAP; probe++) {
    u32 i = (h + probe) & (REALIZE_INFO_HASH_CAP - 1);
    if (REALIZE_INFO_HASH[i] == REALIZE_INFO_HASH_EMPTY) {
      REALIZE_INFO_HASH[i] = idx;
      return;
    }
  }
  // Hash table full -- silently drop; lookups for this loc will
  // miss but caller has the linear cap to fall back to.
}

static u32 realize_info_get_or_add(u64 loc, u8 op) {
  u32 idx = realize_info_find(loc);
  if (idx != 0xFFFFFFFFu) return idx;
  if (REALIZE_INFO_LEN >= REALIZE_INFO_CAP) return 0xFFFFFFFFu;
  idx = REALIZE_INFO_LEN++;
  REALIZE_INFO[idx].loc            = loc;
  REALIZE_INFO[idx].consumer_count = 0;
  REALIZE_INFO[idx].reasons        = 0;
  REALIZE_INFO[idx].op             = op;
  REALIZE_INFO[idx].realized       = 0;
  realize_info_hash_insert(loc, idx);
  return idx;
}

static void realize_mark(UOpInfo *info, u32 reason) {
  if (info == NULL) {
    return;
  }
  info->realized = 1;
  info->reasons |= reason;
  // Forward to the bufferize graph so rule-driven boundary additions
  // (e.g. metal-tile-fanin-cap promoting a child) get a B_BUFFERIZE
  // record stamped with the current rule.  bufferize_realize_with_reason
  // is a no-op until bufferize_seed_from_realize_info has run, so the
  // initial ROOT/MULTI/REDUCE seed pass does not double-mutate.
  bufferize_realize_with_reason(info->loc, info->op, reason);
}

static void realize_unmark(UOpInfo *info, u32 reason) {
  if (info == NULL) {
    return;
  }
  info->realized = 0;
  info->reasons |= reason;
  // Mirror the unmark into the bufferize graph and stamp removed_by
  // with whichever rule realize_rewrite_apply has flagged as current.
  bufferize_unrealize(info->loc);
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

    u32 cidx = realize_info_get_or_add(cloc, term_ext(child));
    if (cidx != 0xFFFFFFFFu) REALIZE_INFO[cidx].consumer_count++;
    realize_walk_rec(child, visited);
  }
}

// Walk the heap looking for the unique UOp parent of `child_loc`.
// Returns the parent's loc, or 0 if there are zero or 2+ parents.
// Used by the softmax-relaxation pass to confirm the consumer chain.
static u64 realize_unique_uop_parent(u64 child_loc) {
  u64 found = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    u64 ploc = REALIZE_INFO[i].loc;
    u8  pop  = REALIZE_INFO[i].op;
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

// Walk a REDUCE's consumer chain and return 1 iff every hop is
// scalar-preserving until we hit an EXPAND (the broadcast back to
// vector shape).  The chain pattern this matches is exactly:
//     REDUCE -> {RECIP|NEG|SQRT|EXP2|LOG2|...} -> EXPAND
// which is what TSoftmax + the "normalize by sum" idiom produce.
// CONST + binary ops where both srcs are scalars also pass.
static int realize_reduce_consumer_is_broadcast_chain(u64 reduce_loc) {
  u64 cur = reduce_loc;
  for (u32 hops = 0; hops < 8; hops++) {     // bounded -- pathological chain bails
    u64 parent = realize_unique_uop_parent(cur);
    if (parent == 0) return 0;
    u32 idx = realize_info_find(parent);
    if (idx == 0xFFFFFFFFu) return 0;
    u8 pop = REALIZE_INFO[idx].op;
    if (pop == UOP_EXPAND) return 1;          // success: chain bottoms out at broadcast
    if (pop == UOP_NEG || pop == UOP_RECIP || pop == UOP_SQRT
     || pop == UOP_EXP2 || pop == UOP_LOG2) {
      cur = parent;
      continue;
    }
    return 0;     // any other op (binary, movement-other, REDUCE) breaks the pattern
  }
  return 0;
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
static int realize_reduce_consumer_is_scalar_tail(u64 reduce_loc, u64 root_loc) {
  u64 cur = reduce_loc;
  for (u32 hops = 0; hops < 8; hops++) {
    if (cur == root_loc) return 1;   // chain reaches the realize root
    u64 parent = realize_unique_uop_parent(cur);
    if (parent == 0) return 0;
    u32 idx = realize_info_find(parent);
    if (idx == 0xFFFFFFFFu) return 0;
    u8 pop = REALIZE_INFO[idx].op;
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

static int realize_metal_tile_fanin_cap_enabled(void) {
  char const *backend = getenv("THVM_BACKEND");
  char const *tile    = getenv("THVM_TILE");
  return backend != NULL && strcmp(backend, "metal") == 0
      && tile != NULL && tile[0] == '1';
}

static u32 realize_metal_tile_fanin_cap(void) {
  char const *e = getenv("THVM_METAL_FUSION_MAX_INPUTS");
  if (e != NULL && e[0] != '\0') {
    unsigned long v = strtoul(e, NULL, 10);
    if (v >= 2 && v <= 30) {
      return (u32)v;
    }
  }
  return 24;
}

static int realize_fanin_split_child_op(u8 op) {
  return uop_is_unary_elementwise(op) || uop_is_binary_elementwise(op)
      || op == UOP_CAST    || op == UOP_BITCAST
      || op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
}

static u32 realize_fanin_uop_count(u64 loc, u64 boundary_root,
                                   u32 cap, u32 *hits);

static u32 realize_fanin_term_count(Term t, u64 boundary_root,
                                    u32 cap, u32 *hits) {
  t = term_resolve(t);
  if (term_tag(t) == TAG_TEN) return 1;
  if (term_tag(t) == TAG_VAR) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 1;
  return realize_fanin_uop_count(term_val(t), boundary_root, cap, hits);
}

static u32 realize_fanin_uop_count(u64 loc, u64 boundary_root,
                                   u32 cap, u32 *hits) {
  if (loc >= HEAP_NEXT) return 1;
  u32 idx = realize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 1;
  UOpInfo *info = &REALIZE_INFO[idx];
  if (loc != boundary_root && info->realized) return 1;
  if (info->op == UOP_CONST) return 0;

  u8  ar          = uop_arity(info->op);
  u32 total       = 0;
  u32 child_idx  [MAX_UOP_SRC] = {0};
  u32 child_count[MAX_UOP_SRC] = {0};
  for (u8 i = 0; i < ar; i++) {
    child_idx[i] = 0xFFFFFFFFu;
    Term child = term_resolve(heap_read(loc + i));
    child_count[i] = realize_fanin_term_count(child, boundary_root,
                                              cap, hits);
    total += child_count[i];
    if (term_tag(child) == TAG_UOP && term_ext(child) != UOP_KERNEL) {
      child_idx[i] = realize_info_find(term_val(child));
    }
  }

  while (total > cap) {
    u8  best      = 0xFF;
    u32 best_size = 1;
    for (u8 i = 0; i < ar; i++) {
      u32 cidx = child_idx[i];
      if (cidx == 0xFFFFFFFFu) continue;
      if (child_count[i] <= best_size) continue;
      if (!realize_fanin_split_child_op(REALIZE_INFO[cidx].op)) continue;
      best      = i;
      best_size = child_count[i];
    }
    if (best == 0xFF) break;
    UOpInfo *child_info = &REALIZE_INFO[child_idx[best]];
    if (!child_info->realized && hits != NULL) (*hits)++;
    realize_mark(child_info, REALIZE_REASON_FANIN_CAP);
    total = total - child_count[best] + 1;
    child_count[best] = 1;
  }
  return total;
}

static u32 realize_rule_metal_tile_fanin_cap(Term root) {
  if (!realize_metal_tile_fanin_cap_enabled()) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) == UOP_KERNEL) return 0;

  u32 cap = realize_metal_tile_fanin_cap();
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (!REALIZE_INFO[i].realized) continue;
    realize_fanin_uop_count(REALIZE_INFO[i].loc, REALIZE_INFO[i].loc,
                            cap, &hits);
  }
  return hits;
}

static int realize_op_is_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
}

static u64 realize_shape_numel(Shape const *s) {
  if (s == NULL || s->ndim == 0) {
    return 0;
  }
  u64 n = 1;
  for (u32 i = 0; i < s->ndim; i++) {
    n *= s->dims[i];
  }
  return n;
}

static int realize_subtree_has_reduce(Term t, u32 depth) {
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
  u32 idx = realize_info_find(loc);
  if (idx != 0xFFFFFFFFu && REALIZE_INFO[idx].realized) {
    return 0;
  }
  u8 op = term_ext(t);
  if (op == UOP_REDUCE) {
    return 1;
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (realize_subtree_has_reduce(heap_read(loc + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int realize_inline_multiconsumer_expand_enabled(void) {
  if (!realize_metal_tile_fanin_cap_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_INLINE_MULTI_CONSUMER_EXPAND");
  return e == NULL || e[0] != '0';
}

static int realize_inline_multiconsumer_pure_enabled(void) {
  if (!realize_metal_tile_fanin_cap_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_INLINE_MULTI_CONSUMER_PURE");
  return e != NULL && e[0] == '1';
}

static int realize_rangeify_enabled(void);

static int realize_inline_reduce_fanout_enabled(void) {
  if (!realize_metal_tile_fanin_cap_enabled() || !realize_rangeify_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_INLINE_REDUCE_FANOUT");
  return e != NULL && e[0] == '1';
}

static int realize_remove_removable_bufferize_enabled(void) {
  if (!realize_metal_tile_fanin_cap_enabled() || !realize_rangeify_enabled()) {
    return 0;
  }
  char const *e = getenv("THVM_REMOVE_REMOVABLE_BUFFERIZE");
  return e == NULL || e[0] != '0';
}

static u64 realize_inline_multiconsumer_pure_min_numel(void) {
  char const *e = getenv("THVM_INLINE_MULTI_CONSUMER_PURE_MIN_NUMEL");
  if (e != NULL && e[0] != '\0') {
    unsigned long long v = strtoull(e, NULL, 10);
    if (v > 0) {
      return (u64)v;
    }
  }
  return 65536;
}

static u64 realize_inline_reduce_fanout_min_numel(void) {
  char const *e = getenv("THVM_INLINE_REDUCE_FANOUT_MIN_NUMEL");
  if (e != NULL && e[0] != '\0') {
    unsigned long long v = strtoull(e, NULL, 10);
    if (v > 0) {
      return (u64)v;
    }
  }
  return 65536;
}

static u32 realize_remove_removable_bufferize_max_ops(void) {
  char const *e = getenv("THVM_REMOVE_BUFFERIZE_MAX_OPS");
  if (e != NULL && e[0] != '\0') {
    unsigned long v = strtoul(e, NULL, 10);
    if (v >= 1 && v <= 1024) {
      return (u32)v;
    }
  }
  return 32;
}

static u32 realize_remove_removable_bufferize_max_consumers(void) {
  char const *e = getenv("THVM_REMOVE_BUFFERIZE_MAX_CONSUMERS");
  if (e != NULL && e[0] != '\0') {
    unsigned long v = strtoul(e, NULL, 10);
    if (v >= 2 && v <= 64) {
      return (u32)v;
    }
  }
  return 8;
}

static int realize_dump_fusion_candidates_enabled(void) {
  char const *e = getenv("DUMP_FUSION_REWRITE_CANDIDATES");
  return e != NULL && e[0] == '1';
}

static u32 realize_rule_inline_large_expand_fanout(Term root) {
  (void)root;
  if (!realize_inline_multiconsumer_expand_enabled()) {
    return 0;
  }
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
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
    u64 out_numel = realize_shape_numel(&out_shape);
    u64 src_numel = realize_shape_numel(&src_shape);
    if (src_numel == 0 || out_numel < src_numel * 8) {
      continue;
    }
    if (realize_subtree_has_reduce(src, 0)) {
      continue;
    }
    hits++;
    realize_unmark(info, REALIZE_REASON_INLINE);
  }
  return hits;
}

static int realize_recompute_pure_op(u8 op) {
  return op == UOP_CONST || op == UOP_LOAD
      || op == UOP_CAST  || op == UOP_BITCAST
      || uop_is_unary_elementwise(op)
      || uop_is_binary_elementwise(op)
      || realize_op_is_movement(op);
}

static int realize_subtree_is_pure_recomputable(Term t, u64 root_loc,
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
  if (!realize_recompute_pure_op(op) || op == UOP_REDUCE) {
    return 0;
  }
  if (realize_op_is_movement(op)) {
    *has_movement = 1;
  }

  u64 loc = term_val(t);
  if (loc != root_loc) {
    u32 idx = realize_info_find(loc);
    if (idx != 0xFFFFFFFFu && REALIZE_INFO[idx].realized) {
      return 1;
    }
  }

  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (!realize_subtree_is_pure_recomputable(heap_read(loc + i),
                                              root_loc, depth + 1,
                                              has_movement)) {
      return 0;
    }
  }
  return 1;
}

static int realize_parent_uses_child(u64 parent_loc, u64 child_loc) {
  u32 pidx = realize_info_find(parent_loc);
  if (pidx == 0xFFFFFFFFu) {
    return 0;
  }
  u8 ar = uop_arity(REALIZE_INFO[pidx].op);
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

static int realize_subtree_recompute_stats(Term t,
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
  if (op == UOP_REDUCE || !realize_recompute_pure_op(op)) {
    return 0;
  }

  u64 loc = term_val(t);
  if (loc != root_loc) {
    u32 idx = realize_info_find(loc);
    if (idx != 0xFFFFFFFFu && REALIZE_INFO[idx].realized) {
      stats->inputs++;
      return 1;
    }
  }

  if (realize_op_is_movement(op)) {
    stats->has_movement = 1;
  }
  if (op != UOP_CONST && op != UOP_LOAD) {
    stats->ops++;
  }

  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (!realize_subtree_recompute_stats(heap_read(loc + i),
                                         root_loc, depth + 1,
                                         stats)) {
      return 0;
    }
  }
  return 1;
}

static int realize_parent_chain_reaches_reduce(u64 child_loc, u32 depth) {
  if (depth > 32) {
    return 1;
  }
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    u64 ploc = REALIZE_INFO[i].loc;
    if (ploc == child_loc) {
      continue;
    }
    u8 pop = REALIZE_INFO[i].op;
    if (!realize_parent_uses_child(ploc, child_loc)) {
      continue;
    }
    if (pop == UOP_REDUCE) {
      return 1;
    }
    if (realize_parent_chain_reaches_reduce(ploc, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static u32 realize_rule_inline_pure_fanout_probe(Term root) {
  (void)root;
  if (!realize_inline_multiconsumer_pure_enabled()) {
    return 0;
  }
  u64 min_numel = realize_inline_multiconsumer_pure_min_numel();
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (!info->realized || info->consumer_count < 2) {
      continue;
    }
    if (!realize_recompute_pure_op(info->op)) {
      continue;
    }
    Term self = term_new(0, TAG_UOP, info->op, info->loc);
    Shape out_shape = {0};
    if (!term_shape_in(self, 0, &out_shape)) {
      continue;
    }
    if (realize_shape_numel(&out_shape) < min_numel) {
      continue;
    }
    int has_movement = 0;
    if (!realize_subtree_is_pure_recomputable(self, info->loc, 0,
                                              &has_movement)) {
      continue;
    }
    if (!has_movement) {
      continue;
    }
    if (realize_parent_chain_reaches_reduce(info->loc, 0)) {
      if (realize_dump_fusion_candidates_enabled()) {
        fprintf(stderr,
                "realize_rewrite_skip rule=inline-pure-fanout-probe"
                " reason=reduce-parent loc=%llu op=%u numel=%llu"
                " consumers=%u\n",
                (unsigned long long)info->loc,
                (unsigned)info->op,
                (unsigned long long)realize_shape_numel(&out_shape),
                (unsigned)info->consumer_count);
      }
      continue;
    }
    if (realize_dump_fusion_candidates_enabled()) {
      fprintf(stderr,
              "realize_rewrite_hit rule=inline-pure-fanout-probe"
              " loc=%llu op=%u numel=%llu consumers=%u\n",
              (unsigned long long)info->loc,
              (unsigned)info->op,
              (unsigned long long)realize_shape_numel(&out_shape),
              (unsigned)info->consumer_count);
    }
    hits++;
    realize_unmark(info, REALIZE_REASON_INLINE);
  }
  return hits;
}

static int realize_reduce_fanout_consumers_ok(u64 reduce_loc) {
  u32 consumers = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    u64 ploc = REALIZE_INFO[i].loc;
    if (ploc == reduce_loc) {
      continue;
    }
    u8 pop = REALIZE_INFO[i].op;
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
    if (pop == UOP_REDUCE || !realize_recompute_pure_op(pop)) {
      return 0;
    }
    if (realize_parent_chain_reaches_reduce(ploc, 0)) {
      return 0;
    }
  }
  return consumers >= 2;
}

static u32 realize_rule_inline_reduce_fanout(Term root) {
  (void)root;
  if (!realize_inline_reduce_fanout_enabled()) {
    return 0;
  }
  u64 min_numel = realize_inline_reduce_fanout_min_numel();
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (!info->realized || info->op != UOP_REDUCE
        || info->consumer_count < 2) {
      continue;
    }
    Term self = term_new(0, TAG_UOP, info->op, info->loc);
    Shape out_shape = {0};
    if (!term_shape_in(self, 0, &out_shape)) {
      continue;
    }
    if (realize_shape_numel(&out_shape) < min_numel) {
      continue;
    }
    if (!realize_reduce_fanout_consumers_ok(info->loc)) {
      continue;
    }
    hits++;
    realize_unmark(info, REALIZE_REASON_INLINE);
  }
  return hits;
}

static int realize_removable_bufferize_consumers_ok(u64 loc,
                                                    int producer_has_movement,
                                                    u32 max_consumers) {
  u32 consumers = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    u64 ploc = REALIZE_INFO[i].loc;
    if (ploc == loc) {
      continue;
    }
    if (!realize_parent_uses_child(ploc, loc)) {
      continue;
    }

    consumers++;
    if (consumers > max_consumers) {
      return 0;
    }

    u8 pop = REALIZE_INFO[i].op;
    if (pop == UOP_REDUCE) {
      if (producer_has_movement) {
        return 0;
      }
      continue;
    }
    if (!realize_recompute_pure_op(pop)) {
      return 0;
    }
    if (producer_has_movement && realize_parent_chain_reaches_reduce(ploc, 0)) {
      return 0;
    }
  }
  return consumers >= 2;
}

static u32 realize_rule_remove_removable_bufferize(Term root) {
  (void)root;
  if (!realize_remove_removable_bufferize_enabled()) {
    return 0;
  }

  u32 hits = 0;
  u32 max_ops = realize_remove_removable_bufferize_max_ops();
  u32 max_consumers = realize_remove_removable_bufferize_max_consumers();
  for (u32 pass = 0; pass < 8; pass++) {
    u32 pass_hits = 0;
    for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
      UOpInfo *info = &REALIZE_INFO[i];
      if (!info->realized || info->consumer_count < 2) {
        continue;
      }
      if ((info->reasons & REALIZE_REASON_MULTI) == 0) {
        continue;
      }
      if ((info->reasons & (REALIZE_REASON_ROOT | REALIZE_REASON_REDUCE
                          | REALIZE_REASON_FANIN_CAP)) != 0) {
        continue;
      }
      if (!realize_recompute_pure_op(info->op) || info->op == UOP_REDUCE) {
        continue;
      }

      Term self = term_new(0, TAG_UOP, info->op, info->loc);
      RealizeRecomputeStats stats;
      memset(&stats, 0, sizeof(stats));
      if (!realize_subtree_recompute_stats(self, info->loc, 0, &stats)) {
        continue;
      }
      if (stats.ops > max_ops) {
        if (realize_dump_fusion_candidates_enabled()) {
          fprintf(stderr,
                  "realize_rewrite_skip rule=remove-removable-bufferize"
                  " reason=op-budget loc=%llu op=%u ops=%u consumers=%u\n",
                  (unsigned long long)info->loc,
                  (unsigned)info->op,
                  (unsigned)stats.ops,
                  (unsigned)info->consumer_count);
        }
        continue;
      }
      if (!realize_removable_bufferize_consumers_ok(info->loc,
                                                    stats.has_movement,
                                                    max_consumers)) {
        if (realize_dump_fusion_candidates_enabled()) {
          fprintf(stderr,
                  "realize_rewrite_skip rule=remove-removable-bufferize"
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

      if (realize_dump_fusion_candidates_enabled()) {
        fprintf(stderr,
                "realize_rewrite_hit rule=remove-removable-bufferize"
                " loc=%llu op=%u ops=%u movement=%u consumers=%u\n",
                (unsigned long long)info->loc,
                (unsigned)info->op,
                (unsigned)stats.ops,
                (unsigned)stats.has_movement,
                (unsigned)info->consumer_count);
      }
      realize_unmark(info, REALIZE_REASON_INLINE);
      pass_hits++;
    }
    hits += pass_hits;
    if (pass_hits == 0) {
      break;
    }
  }
  return hits;
}

static int realize_inline_subtree_has_movement(Term t, u32 depth) {
  if (depth > 64) return 1;
  t = term_resolve(t);
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 0;
  u64 loc = term_val(t);
  u32 idx = realize_info_find(loc);
  if (idx != 0xFFFFFFFFu && REALIZE_INFO[idx].realized) return 0;
  u8 op = term_ext(t);
  if (realize_op_is_movement(op)) return 1;
  if (op == UOP_REDUCE) return 1;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (realize_inline_subtree_has_movement(heap_read(loc + i), depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int realize_reduce_chain_source_is_direct(Term t) {
  t = term_resolve(t);
  if (term_tag(t) == TAG_TEN || term_tag(t) == TAG_VAR) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 1;
  u32 idx = realize_info_find(term_val(t));
  return idx != 0xFFFFFFFFu && REALIZE_INFO[idx].realized;
}

static u32 realize_reduce_count(void) {
  u32 n = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (REALIZE_INFO[i].op == UOP_REDUCE) n++;
  }
  return n;
}

static int realize_rangeify_enabled(void) {
  char const *e = getenv("THVM_RANGEIFY");
  return e == NULL ? 1 : (e[0] != '0');
}

// Phase 4 follow-up: a bufferize-graph-driven removal rule.
//
// Iterates the bufferize graph (not REALIZE_INFO), reads the
// cost-model score that bufferize_seed_from_realize_info computed,
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
static int realize_remove_by_cost_score_enabled(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_BY_SCORE");
  if (e == NULL) return 1;
  return e[0] != '0';
}

static u64 realize_remove_by_cost_score_threshold(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_THRESHOLD");
  if (e != NULL && e[0] != '\0') {
    return strtoull(e, NULL, 10);
  }
  return 50000;
}

static u32 realize_remove_by_cost_score_max_ops(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_MAX_OPS");
  if (e != NULL && e[0] != '\0') {
    return (u32)strtoul(e, NULL, 10);
  }
  return 32;
}

static u32 realize_remove_by_cost_score_max_consumers(void) {
  char const *e = getenv("THVM_BUFFERIZE_REMOVE_SCORE_MAX_CONSUMERS");
  if (e != NULL && e[0] != '\0') {
    return (u32)strtoul(e, NULL, 10);
  }
  return 4;
}

static u32 realize_rule_remove_by_cost_score(Term root) {
  (void)root;
  if (!realize_remove_by_cost_score_enabled()) return 0;
  u64 threshold     = realize_remove_by_cost_score_threshold();
  u32 max_ops       = realize_remove_by_cost_score_max_ops();
  u32 max_consumers = realize_remove_by_cost_score_max_consumers();
  int dump = realize_dump_fusion_candidates_enabled();
  u32 hits = 0;
  // Iterate the bufferize graph directly - this rule is the first
  // one in the codebase that reads BBufferize records as the
  // canonical schedule state and only touches REALIZE_INFO to
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
                "realize_rewrite_skip rule=remove-by-cost-score"
                " reason=subtree-reduce loc=%llu op=%u\n",
                (unsigned long long)b->loc, (unsigned)b->op);
      }
      continue;
    }
    if (b->recompute_ops > max_ops) {
      if (dump) {
        fprintf(stderr,
                "realize_rewrite_skip rule=remove-by-cost-score"
                " reason=op-budget loc=%llu op=%u ops=%u cap=%u\n",
                (unsigned long long)b->loc, (unsigned)b->op,
                (unsigned)b->recompute_ops, (unsigned)max_ops);
      }
      continue;
    }
    if (b->consumer_count > max_consumers) {
      if (dump) {
        fprintf(stderr,
                "realize_rewrite_skip rule=remove-by-cost-score"
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
                "realize_rewrite_skip rule=remove-by-cost-score"
                " reason=score-threshold loc=%llu op=%u score=%llu"
                " threshold=%llu\n",
                (unsigned long long)b->loc, (unsigned)b->op,
                (unsigned long long)score, (unsigned long long)threshold);
      }
      continue;
    }
    u32 ridx = realize_info_find(b->loc);
    if (ridx == 0xFFFFFFFFu) continue;
    if (dump) {
      fprintf(stderr,
              "realize_rewrite_hit rule=remove-by-cost-score"
              " loc=%llu op=%u ops=%u consumers=%u numel=%llu score=%llu\n",
              (unsigned long long)b->loc, (unsigned)b->op,
              (unsigned)b->recompute_ops,
              (unsigned)b->consumer_count,
              (unsigned long long)b->output_numel,
              (unsigned long long)score);
    }
    realize_unmark(&REALIZE_INFO[ridx], REALIZE_REASON_INLINE);
    hits++;
  }
  return hits;
}

static u32 realize_rule_inline_constants(Term root) {
  (void)root;
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_CONST) continue;
    if (info->realized) hits++;
    realize_unmark(info, REALIZE_REASON_INLINE);
  }
  return hits;
}

static u32 realize_rule_inline_adjacent_reduce_chains(Term root) {
  (void)root;
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_REDUCE) continue;
    ReduceChainInfo rc;
    Term root_term = term_new(0, TAG_UOP, UOP_REDUCE, info->loc);
    if (!reduce_chain_collect(root_term, &rc)) continue;
    if (!realize_reduce_chain_source_is_direct(rc.src)) continue;
    if (realize_inline_subtree_has_movement(rc.src, 0)) continue;
    int ok = 1;
    for (u32 j = 1; j < rc.n_reduces; j++) {
      u32 cidx = realize_info_find(rc.locs[j]);
      if (cidx == 0xFFFFFFFFu || REALIZE_INFO[cidx].consumer_count != 1) {
        ok = 0;
        break;
      }
    }
    if (!ok) continue;
    for (u32 j = 1; j < rc.n_reduces; j++) {
      u32 cidx = realize_info_find(rc.locs[j]);
      if (cidx == 0xFFFFFFFFu) continue;
      if (REALIZE_INFO[cidx].realized) hits++;
      realize_unmark(&REALIZE_INFO[cidx], REALIZE_REASON_INLINE);
    }
  }
  return hits;
}

// Gate the softmax-broadcast-reduce / scalar-tail rules' multi-reduce
// generalization.  Each REDUCE is checked independently by
// realize_reduce_consumer_is_broadcast_chain, so removing the
// "exactly one REDUCE in the graph" restriction is safe per-rule.
// Default-on; THVM_BUFFERIZE_REDUCE_FUSE_MULTI=0 reverts to the
// historical single-reduce gate.
static int realize_reduce_fuse_multi_enabled(void) {
  char const *e = getenv("THVM_BUFFERIZE_REDUCE_FUSE_MULTI");
  return e == NULL ? 1 : (e[0] != '0');
}

static u32 realize_rule_inline_softmax_broadcast_reduce(Term root) {
  (void)root;
  if (!realize_reduce_fuse_multi_enabled() && realize_reduce_count() != 1) {
    return 0;
  }
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_REDUCE)    continue;
    if (info->consumer_count != 1) continue;
    if (!realize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
    if (info->realized) hits++;
    realize_unmark(info, REALIZE_REASON_INLINE);
  }
  return hits;
}

static u32 realize_rule_inline_reduce_scalar_tail(Term root) {
  if (term_tag(root) != TAG_UOP) return 0;
  if (!realize_rangeify_enabled()) return 0;
  if (!realize_reduce_fuse_multi_enabled() && realize_reduce_count() != 1) {
    return 0;
  }
  u64 root_loc = term_val(root);
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_REDUCE)    continue;
    if (info->consumer_count != 1) continue;
    if (!realize_reduce_consumer_is_scalar_tail(info->loc, root_loc)) continue;
    if (info->realized) hits++;
    realize_unmark(info, REALIZE_REASON_INLINE);
  }
  return hits;
}

fn void realize_classify(Term root) {
  realize_info_clear();
  realize_rewrite_stats_clear();
  // Always touch the bufferize graph so its state stays in sync with
  // REALIZE_INFO.  Both seed/finalize short-circuit on non-UOp roots
  // after zeroing their own tables.
  if (term_tag(root) != TAG_UOP) {
    bufferize_seed_from_realize_info(root);
    bufferize_finalize_stores(root);
    return;
  }
  if (term_ext(root) == UOP_KERNEL) {
    bufferize_seed_from_realize_info(root);
    bufferize_finalize_stores(root);
    return;
  }

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
  if (root_idx != 0xFFFFFFFFu) {
    realize_mark(&REALIZE_INFO[root_idx], REALIZE_REASON_ROOT);
  }

  // Rules (b) + (c): multi-consumer or REDUCE.  Later named rules
  // can clear the final `realized` bit while preserving these
  // reason bits for diagnostics and tests.
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->consumer_count >= 2) {
      realize_mark(info, REALIZE_REASON_MULTI);
    }
    if (info->op == UOP_REDUCE) {
      realize_mark(info, REALIZE_REASON_REDUCE);
    }
  }
  RealizeRewriteRule rules[] = {
    {"inline-constants",              realize_rule_inline_constants},
    {"inline-adjacent-reduce-chains", realize_rule_inline_adjacent_reduce_chains},
    {"inline-softmax-broadcast-reduce", realize_rule_inline_softmax_broadcast_reduce},
    {"inline-reduce-scalar-tail",     realize_rule_inline_reduce_scalar_tail},
    {"inline-large-expand-fanout",    realize_rule_inline_large_expand_fanout},
    {"inline-reduce-fanout",          realize_rule_inline_reduce_fanout},
    {"remove-removable-bufferize",    realize_rule_remove_removable_bufferize},
    {"remove-by-cost-score",          realize_rule_remove_by_cost_score},
    {"inline-pure-fanout-probe",      realize_rule_inline_pure_fanout_probe},
    {"metal-tile-fanin-cap",          realize_rule_metal_tile_fanin_cap},
  };
  // Phase 1: snapshot the seeded REALIZE_INFO into the bufferize
  // graph, then run the named rewrite rules.  realize_mark and
  // realize_unmark forward into bufferize_realize_with_reason and
  // bufferize_unrealize so each rule's effect is recorded as
  // added_by/removed_by on the explicit graph.  Finalize the store
  // table after rewrites land.
  bufferize_seed_from_realize_info(root);
  realize_rewrite_apply(root, rules, (u32)(sizeof(rules) / sizeof(rules[0])));
  realize_rewrite_stats_dump();
  bufferize_finalize_stores(root);
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

fn u32 realize_reasons(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) {
    return 0;
  }
  u32 idx = realize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) {
    return 0;
  }
  return REALIZE_INFO[idx].reasons;
}
