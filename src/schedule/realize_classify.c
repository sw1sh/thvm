// schedule/realize_classify.c - decide which UOPs in a graph
//                                must "realize" into a backing
//                                buffer (a tinygrad CONTIGUOUS).
//
// A UOp realizes when ANY of:
//   (a) it is the root the caller asked for,
//   (b) it has 2+ distinct UOp parents (multi-consumer),
//   (c) it is a REDUCE (REDUCE outputs always escape into a
//       buffer; one reduce per kernel),
//   (d) it is a movement op whose source can't be aliased.
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
  REALIZE_INFO[idx].op             = op;
  REALIZE_INFO[idx].realized       = 0;
  realize_info_hash_insert(loc, idx);
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
  return 30;
}

static int realize_fanin_split_child_op(u8 op) {
  return op == UOP_ADD || op == UOP_MUL
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
    child_info->realized = 1;
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
    info->realized = 0;
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
    hits++;
    info->realized = 0;
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

static u32 realize_rule_inline_constants(Term root) {
  (void)root;
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_CONST) continue;
    if (info->realized) hits++;
    info->realized = 0;
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
      REALIZE_INFO[cidx].realized = 0;
    }
  }
  return hits;
}

static u32 realize_rule_inline_softmax_broadcast_reduce(Term root) {
  (void)root;
  if (realize_reduce_count() != 1) return 0;
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_REDUCE)    continue;
    if (info->consumer_count != 1) continue;
    if (!realize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
    if (info->realized) hits++;
    info->realized = 0;
  }
  return hits;
}

static u32 realize_rule_inline_reduce_scalar_tail(Term root) {
  if (term_tag(root) != TAG_UOP) return 0;
  if (!realize_rangeify_enabled()) return 0;
  if (realize_reduce_count() != 1) return 0;
  u64 root_loc = term_val(root);
  u32 hits = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *info = &REALIZE_INFO[i];
    if (info->op != UOP_REDUCE)    continue;
    if (info->consumer_count != 1) continue;
    if (!realize_reduce_consumer_is_scalar_tail(info->loc, root_loc)) continue;
    if (info->realized) hits++;
    info->realized = 0;
  }
  return hits;
}

fn void realize_classify(Term root) {
  realize_info_clear();
  realize_rewrite_stats_clear();
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
  RealizeRewriteRule rules[] = {
    {"inline-constants",              realize_rule_inline_constants},
    {"inline-adjacent-reduce-chains", realize_rule_inline_adjacent_reduce_chains},
    {"inline-softmax-broadcast-reduce", realize_rule_inline_softmax_broadcast_reduce},
    {"inline-reduce-scalar-tail",     realize_rule_inline_reduce_scalar_tail},
    {"inline-large-expand-fanout",    realize_rule_inline_large_expand_fanout},
    {"inline-pure-fanout-probe",      realize_rule_inline_pure_fanout_probe},
    {"metal-tile-fanin-cap",          realize_rule_metal_tile_fanin_cap},
  };
  realize_rewrite_apply(root, rules, (u32)(sizeof(rules) / sizeof(rules[0])));
  realize_rewrite_stats_dump();
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
