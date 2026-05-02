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
  return op == UOP_ADD || op == UOP_MUL;
}

static u32 realize_fanin_term_count(Term t, u64 boundary_root, u32 cap);

static u32 realize_fanin_uop_count(u64 loc, u64 boundary_root, u32 cap) {
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
    child_count[i] = realize_fanin_term_count(child, boundary_root, cap);
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
    REALIZE_INFO[child_idx[best]].realized = 1;
    total = total - child_count[best] + 1;
    child_count[best] = 1;
  }
  return total;
}

static u32 realize_fanin_term_count(Term t, u64 boundary_root, u32 cap) {
  t = term_resolve(t);
  if (term_tag(t) == TAG_TEN) return 1;
  if (term_tag(t) == TAG_VAR) return 1;
  if (term_tag(t) != TAG_UOP) return 0;
  if (term_ext(t) == UOP_KERNEL) return 1;
  return realize_fanin_uop_count(term_val(t), boundary_root, cap);
}

static void realize_apply_metal_tile_fanin_cap(Term root) {
  if (!realize_metal_tile_fanin_cap_enabled()) return;
  if (term_tag(root) != TAG_UOP || term_ext(root) == UOP_KERNEL) return;

  u32 cap = realize_metal_tile_fanin_cap();
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (!REALIZE_INFO[i].realized) continue;
    realize_fanin_uop_count(REALIZE_INFO[i].loc, REALIZE_INFO[i].loc, cap);
  }
}

static int realize_op_is_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
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

  // Adjacent reductions over a contiguous axis span can stay inside
  // one KProgOp REDUCE.  Keep the outer reduce as the boundary and
  // drop the inner reduce boundaries when every inner hop is private
  // to this chain.  materialize.c re-packs the chain as one wider
  // reduce using the existing (kind << 24) | inner encoding.
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
      if (cidx != 0xFFFFFFFFu) REALIZE_INFO[cidx].realized = 0;
    }
  }

  // Softmax-style relaxation: a REDUCE whose single consumer is a
  // scalar-preserving ALU chain (RECIP / NEG / SQRT / ...) ending in
  // an EXPAND back to the reduce SOURCE's shape can be inlined into
  // the EXPAND's consumer kernel without recomputation cost (the
  // REDUCE result is a scalar that gets broadcast-elementwise into
  // the elementwise tail).  Walk the consumer chain: if every hop is
  // scalar-preserving until an EXPAND, drop the REDUCE's realize bit
  // so visit() inlines it.
  //
  // Conservative additional guard: only relax when this is the SOLE
  // REDUCE in the whole realize-info graph.  Multi-REDUCE patterns
  // (cross-entropy + softmax, autograd through reductions, ...)
  // create complex consumer trees where blindly inlining one REDUCE
  // can cascade into a kernel-emit bail downstream.  When more than
  // one REDUCE shows up, fall back to the original "REDUCE always
  // realizes" rule for everything; that costs the softmax-fusion
  // win for those graphs but keeps correctness.  A future pass can
  // count REDUCEs PER consumer-kernel boundary and relax per-kernel
  // instead of globally.
  u32 reduce_count = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    if (REALIZE_INFO[i].op == UOP_REDUCE) reduce_count++;
  }
  if (reduce_count == 1) {
    for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
      UOpInfo *info = &REALIZE_INFO[i];
      if (info->op != UOP_REDUCE)    continue;
      if (info->consumer_count != 1) continue;
      if (!realize_reduce_consumer_is_broadcast_chain(info->loc)) continue;
      info->realized = 0;
    }

    // Phase C-2: scalar-tail relaxation.  When rangeify is enabled
    // (THVM_RANGEIFY default-on as of Phase E) AND the REDUCE's
    // single-consumer chain stays at the post-reduce shape and
    // reaches the realize root (no EXPAND), the consumer kernel can
    // absorb the REDUCE inline.  Gated on rangeify because the legacy
    // KProgOp[] dispatch hasn't been audited for the wider scalar-
    // tail patterns this opens up; THVM_RANGEIFY=0 disables both
    // this relaxation and the rangeify lower path together.
    {
      const char *e = getenv("THVM_RANGEIFY");
      int rangeify_on = (e == NULL) ? 1 : (e[0] != '0');
      if (rangeify_on) {
        u64 root_loc = term_val(root);
        for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
          UOpInfo *info = &REALIZE_INFO[i];
          if (info->op != UOP_REDUCE)    continue;
          if (info->consumer_count != 1) continue;
          if (!realize_reduce_consumer_is_scalar_tail(info->loc, root_loc))
            continue;
          info->realized = 0;
        }
      }
    }
  }

  realize_apply_metal_tile_fanin_cap(root);
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
