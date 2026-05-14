// schedule/bufferize.c - Phase 0 + Phase 1 of docs/plans/bufferize.md.
//
// Phase 0 was a post-rewrite mirror of BUFFERIZE_NODES.  Phase 1 makes
// the bufferize graph live during the rewrite pass: every named
// realize-map rule that mutates BUFFERIZE_NODES via bufferize_node_mark or
// bufferize_node_unmark is forwarded into bufferize_realize_with_reason or
// bufferize_unrealize, which stamps added_by / removed_by from the
// current rule pointer set by bufferize_rewrite_apply.
//
// Materialize.c still reads BUFFERIZE_NODES directly, so the kernel
// schedule is unchanged.  The bufferize graph is the canonical
// rule-history record; future phases will let materialize consume it
// directly and let new rules edit only the graph.

static BBufferize BUFFERIZE_BUFS [BUFFERIZE_GRAPH_CAP];
static u32        BUFFERIZE_BUFS_LEN = 0;

#define BUFFERIZE_STORE_CAP 64
static BStore BUFFERIZE_STORES[BUFFERIZE_STORE_CAP];
static u32    BUFFERIZE_STORES_LEN = 0;

#define BUFFERIZE_INDEX_CAP BUFFERIZE_NODES_CAP
#define BUFFERIZE_EDGE_DEPTH_CAP 96
static BIndex BUFFERIZE_INDEXES[BUFFERIZE_INDEX_CAP];
static u32    BUFFERIZE_INDEXES_LEN = 0;

// Phase 3: one entry per named index rewrite rule, hit count
// recomputed from the BUFFERIZE_INDEXES table after every
// bufferize_classify pass.  Order is fixed so callers can address by
// index.  Names mirror the plan's index-* family.
typedef struct {
  char const *name;
  u32         hits;
} BIndexRule;
static BIndexRule BUFFERIZE_INDEX_RULES[6] = {
  {"index-reshape",  0},
  {"index-permute",  0},
  {"index-expand",   0},
  {"index-pad-mask", 0},
  {"index-shrink",   0},
  {"index-flip",     0},
};

// bufferize_rewrite_apply sets this around each rule->apply call so
// bufferize_node_mark/bufferize_node_unmark can stamp the rule that decided.
// NULL means "outside any named rule" (seeding, post-pass cleanup).

// Reason mirror: only the bits already on BUFFERIZE_NODES map across.
// Phase 1 still uses inline BUFFERIZE_REASON_INLINE to mark "a rule
// touched this", but the bufferize graph records the rule by name
// directly via removed_by, so we do not project INLINE.
static u32 bufferize_project_reasons(u32 r) {
  u32 out = 0;
  if (r & BUFFERIZE_REASON_ROOT)      out |= BUFFERIZE_REASON_ROOT;
  if (r & BUFFERIZE_REASON_MULTI)     out |= BUFFERIZE_REASON_MULTI;
  if (r & BUFFERIZE_REASON_REDUCE)    out |= BUFFERIZE_REASON_REDUCE;
  if (r & BUFFERIZE_REASON_FANIN_CAP) out |= BUFFERIZE_REASON_BACKEND_CAP;
  return out;
}

static int bufferize_dump_enabled(void) {
  char const *e = getenv("DUMP_BUFFERIZE");
  return e != NULL && e[0] == '1';
}

static int bufferize_dump_candidates_enabled(void) {
  char const *e = getenv("DUMP_BUFFERIZE_CANDIDATES");
  return e != NULL && e[0] == '1';
}

#define BUFFERIZE_CANDIDATE_TOP 20

// Print the top removal candidates sorted by descending
// bufferize_removal_score.  Phase 4: this is the user-visible
// surface that drives manual / autotune decisions, and the
// telemetry future cost-model rules will read.  Selection sort over
// at most BUFFERIZE_CANDIDATE_TOP keeps the dump O(N * top).
static void bufferize_dump_candidates(void) {
  if (!bufferize_dump_candidates_enabled()) return;
  u32 n = BUFFERIZE_BUFS_LEN < BUFFERIZE_CANDIDATE_TOP
            ? BUFFERIZE_BUFS_LEN
            : BUFFERIZE_CANDIDATE_TOP;
  u32 picked[BUFFERIZE_CANDIDATE_TOP];
  for (u32 k = 0; k < n; k++) picked[k] = 0;
  u32 picked_n = 0;
  for (u32 k = 0; k < n; k++) {
    u32 best_idx = 0xFFFFFFFFu;
    u64 best_sc  = 0;
    for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
      // Skip already-picked indices.
      int seen = 0;
      for (u32 j = 0; j < picked_n; j++) if (picked[j] == i + 1) { seen = 1; break; }
      if (seen) continue;
      u64 sc = bufferize_removal_score(i + 1);
      if (sc > best_sc) { best_sc = sc; best_idx = i; }
    }
    if (best_idx == 0xFFFFFFFFu || best_sc == 0) break;
    picked[picked_n++] = best_idx + 1;
    BBufferize const *b = &BUFFERIZE_BUFS[best_idx];
    fprintf(stderr,
            "bufferize_candidate rank=%u id=%u op=%u consumers=%u"
            " ops=%u numel=%llu recompute_total=%llu score=%llu\n",
            (unsigned)(k + 1),
            (unsigned)b->buffer_id,
            (unsigned)b->op,
            (unsigned)b->consumer_count,
            (unsigned)b->recompute_ops,
            (unsigned long long)b->output_numel,
            (unsigned long long)b->recompute_total,
            (unsigned long long)best_sc);
  }
}

static u8 bufferize_op_is_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_PERMUTE || op == UOP_EXPAND
      || op == UOP_PAD     || op == UOP_SHRINK  || op == UOP_FLIP;
}

static void bufferize_extend_chain(BIndex *chain, u8 op) {
  if (!bufferize_op_is_movement(op)) return;
  chain->movement_chain_len++;
  if (op == UOP_RESHAPE) chain->has_reshape = 1;
  if (op == UOP_PERMUTE) chain->has_permute = 1;
  if (op == UOP_EXPAND)  chain->has_expand  = 1;
  if (op == UOP_PAD)     chain->has_pad     = 1;
  if (op == UOP_SHRINK)  chain->has_shrink  = 1;
  if (op == UOP_FLIP)    chain->has_flip    = 1;
}

// Record one movement-op into the chain_ops array, capturing the
// per-op source and output shapes via term_shape_in plus the
// op-specific data (pad widths, axis perm, flip mask) read from the
// op's heap cells.  Bails silently if the chain is already full
// (BUFFERIZE_INDEX_CHAIN_MAX) so deeply-stacked movement chains
// keep working at the cost of losing trailing detail.
//
// Heap layouts (see uop/<op>.c):
//   RESHAPE: [src, NUM(ndim), NUM(d0), ..., NUM(d_n-1)]
//   EXPAND : same as RESHAPE
//   PAD    : [src, NUM(ndim), NUM(b0), NUM(e0), NUM(b1), NUM(e1), ...]
//   SHRINK : same as PAD
//   PERMUTE: [src, NUM(ndim), NUM(p0), ..., NUM(p_n-1)]
//   FLIP   : [src, NUM(axes_bitmask)]
static void bufferize_record_chain_op(BIndex *chain, Term op_term, u8 op,
                                      Term src_term) {
  if (!bufferize_op_is_movement(op)) return;
  if (chain->chain_op_count >= BUFFERIZE_INDEX_CHAIN_MAX) return;
  BIndexChainOp *slot = &chain->chain_ops[chain->chain_op_count++];
  *slot = (BIndexChainOp){0};
  slot->op = op;
  Shape s = {0};
  if (term_shape_in(op_term, 0, &s) && s.ndim <= MAX_DIM) {
    slot->out_ndim = (u8)s.ndim;
    for (u32 d = 0; d < s.ndim; d++) slot->out_dims[d] = s.dims[d];
  }
  Shape ss = {0};
  if (term_shape_in(src_term, 0, &ss) && ss.ndim <= MAX_DIM) {
    slot->src_ndim = (u8)ss.ndim;
    for (u32 d = 0; d < ss.ndim; d++) slot->src_dims[d] = ss.dims[d];
  }
  // Op-specific heap reads.
  u64 op_loc = (term_tag(op_term) == TAG_UOP) ? term_val(op_term) : 0;
  if (op_loc == 0 && term_tag(op_term) != TAG_UOP) return;
  if (op == UOP_PAD || op == UOP_SHRINK) {
    Term n_cell = heap_read(op_loc + 1);
    u32 n = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0;
    if (n > MAX_DIM) n = MAX_DIM;
    for (u32 d = 0; d < n; d++) {
      Term b = heap_read(op_loc + 2 + 2 * d + 0);
      Term e = heap_read(op_loc + 2 + 2 * d + 1);
      slot->pad_widths[2 * d + 0] =
          (term_tag(b) == TAG_NUM) ? (u32)term_val(b) : 0;
      slot->pad_widths[2 * d + 1] =
          (term_tag(e) == TAG_NUM) ? (u32)term_val(e) : 0;
    }
  } else if (op == UOP_PERMUTE) {
    Term n_cell = heap_read(op_loc + 1);
    u32 n = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0;
    if (n > MAX_DIM) n = MAX_DIM;
    for (u32 d = 0; d < n; d++) {
      Term p = heap_read(op_loc + 2 + d);
      slot->axis_perm[d] =
          (term_tag(p) == TAG_NUM) ? (u8)(term_val(p) & 0xFFu) : 0;
    }
  } else if (op == UOP_FLIP) {
    Term m = heap_read(op_loc + 1);
    slot->flip_mask =
        (term_tag(m) == TAG_NUM) ? (u8)(term_val(m) & 0xFFu) : 0;
  }
}

static void bufferize_emit_edge(u32 source_id, u32 consumer_id,
                                BIndex const *chain) {
  if (BUFFERIZE_INDEXES_LEN >= BUFFERIZE_INDEX_CAP) return;
  BIndex *out = &BUFFERIZE_INDEXES[BUFFERIZE_INDEXES_LEN++];
  *out = *chain;
  out->source_buffer_id   = source_id;
  out->consumer_buffer_id = consumer_id;
}

// Walk the consumer's compute tree starting at `loc`, descending
// through non-boundary UOps and accumulating movement-op context on
// the way down.  Whenever we hit another *realized* B_BUFFERIZE we
// emit one B_INDEX and stop descending; an unrealized buffer (one
// that a rule recomputed) is transparent and we keep walking past
// it as if it were any other UOp.
static void bufferize_walk_edge(u64 loc, u32 consumer_id,
                                BIndex const *chain_in, u8 depth) {
  if (depth > BUFFERIZE_EDGE_DEPTH_CAP) return;
  if (loc >= HEAP_NEXT) return;
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return;
  u8 op = BUFFERIZE_NODES[idx].op;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    Term child = term_resolve(heap_read(loc + i));
    if (term_tag(child) != TAG_UOP) continue;       // skip TEN/VAR leaves
    if (term_ext(child) == UOP_KERNEL) continue;    // already realized opaquely
    u64 cloc = term_val(child);
    u32 cbidx = bufferize_find_by_loc(cloc);
    if (cbidx != 0xFFFFFFFFu && BUFFERIZE_BUFS[cbidx].realized) {
      bufferize_emit_edge(BUFFERIZE_BUFS[cbidx].buffer_id, consumer_id,
                          chain_in);
      continue;
    }
    BIndex chain_out = *chain_in;
    u8 cop = term_ext(child);
    bufferize_extend_chain(&chain_out, cop);
    if (bufferize_op_is_movement(cop)) {
      Term src_term = term_resolve(heap_read(cloc + 0));
      bufferize_record_chain_op(&chain_out, child, cop, src_term);
    }
    bufferize_walk_edge(cloc, consumer_id, &chain_out, depth + 1);
  }
}

static void bufferize_build_indexes(void) {
  BUFFERIZE_INDEXES_LEN = 0;
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    BIndex chain = {0};
    bufferize_walk_edge(b->loc, b->buffer_id, &chain, 0);
  }
}

// Phase 3 edge transform: detect identity movement ops in each
// chain and elide them.
//   identity reshape: src_ndim == out_ndim and src_dims == out_dims
//   identity permute: axis_perm[i] == i for every i
//   identity expand : src_ndim == out_ndim and src_dims == out_dims
// Updates movement_chain_len and clears the matching has_* flag
// when no surviving op of that kind remains.  Returns the number of
// identities elided across all edges.
static u8 bufferize_chain_op_is_identity(BIndexChainOp const *o) {
  if (o->src_ndim == 0) return 0;
  if (o->op == UOP_RESHAPE || o->op == UOP_EXPAND) {
    if (o->src_ndim != o->out_ndim) return 0;
    for (u32 d = 0; d < o->src_ndim; d++) {
      if (o->src_dims[d] != o->out_dims[d]) return 0;
    }
    return 1;
  }
  if (o->op == UOP_PERMUTE) {
    if (o->src_ndim != o->out_ndim) return 0;
    for (u32 d = 0; d < o->src_ndim; d++) {
      if (o->axis_perm[d] != d) return 0;
    }
    return 1;
  }
  return 0;
}

static u32 bufferize_apply_identity_reshape(void) {
  u32 hits = 0;
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    BIndex *e = &BUFFERIZE_INDEXES[i];
    u32 keep = 0;
    int still_has_reshape = 0;
    int still_has_permute = 0;
    int still_has_expand  = 0;
    for (u32 j = 0; j < e->chain_op_count; j++) {
      BIndexChainOp const *o = &e->chain_ops[j];
      if (bufferize_chain_op_is_identity(o)) {
        hits++;
        if (e->movement_chain_len > 0) e->movement_chain_len--;
        continue;
      }
      if (o->op == UOP_RESHAPE) still_has_reshape = 1;
      if (o->op == UOP_PERMUTE) still_has_permute = 1;
      if (o->op == UOP_EXPAND)  still_has_expand  = 1;
      e->chain_ops[keep++] = *o;
    }
    e->chain_op_count = (u8)keep;
    if (!still_has_reshape) e->has_reshape = 0;
    if (!still_has_permute) e->has_permute = 0;
    if (!still_has_expand)  e->has_expand  = 0;
  }
  return hits;
}

// Counter for identity-op elisions across reshape/permute/expand,
// surfaced through bufferize_identity_reshape_elision_hits for
// continuity (the name predates the broader scope).
static u32 BUFFERIZE_IDENTITY_RESHAPE_HITS = 0;
fn u32 bufferize_identity_reshape_elision_hits(void) {
  return BUFFERIZE_IDENTITY_RESHAPE_HITS;
}

// Phase 4: count UOps in the producer subtree of `loc` for use as a
// recompute-cost estimate.  Stops at any other realized buffer
// (those would still cache the recomputed value), at REDUCE
// (recompute crosses a fusion boundary we can't cheaply replicate),
// at TEN/VAR leaves, and at any non-pure op.  Const and load are
// counted as zero-cost; other UOps each cost 1.  The walk is bounded
// by depth so pathological graphs cannot blow up the estimate.
// Phase 5: also reports whether the subtree contained a REDUCE via
// the `*has_reduce` out-param, so reduce-aware rules can gate
// recompute removal.
static u32 bufferize_count_recompute_ops(u64 loc, u64 self_loc, u32 depth,
                                         u8 *has_reduce) {
  if (depth > 64) return 0;
  if (loc >= HEAP_NEXT) return 0;
  u32 ridx = bufferize_info_find(loc);
  if (ridx == 0xFFFFFFFFu) return 0;
  u8 op = BUFFERIZE_NODES[ridx].op;
  // Stop at other realized buffers - their cost is amortised.
  if (loc != self_loc) {
    u32 bidx = bufferize_find_by_loc(loc);
    if (bidx != 0xFFFFFFFFu && BUFFERIZE_BUFS[bidx].realized) return 0;
  }
  if (op == UOP_REDUCE) {
    if (has_reduce != NULL) *has_reduce = 1;
    return 1;     // count the reduce, don't descend
  }
  u32 self_cost = (op == UOP_CONST || op == UOP_LOAD) ? 0 : 1;
  u8 ar = uop_arity(op);
  u32 total = self_cost;
  for (u8 i = 0; i < ar; i++) {
    Term child = term_resolve(heap_read(loc + i));
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    total += bufferize_count_recompute_ops(term_val(child), self_loc,
                                           depth + 1, has_reduce);
  }
  return total;
}

static u64 bufferize_shape_numel(Shape const *s) {
  if (s == NULL || s->ndim == 0) return 0;
  u64 n = 1;
  for (u32 i = 0; i < s->ndim; i++) n *= s->dims[i];
  return n;
}

// Populate the recompute_ops / output_numel / recompute_total /
// subtree_has_reduce cost fields for every realized B_BUFFERIZE.
// Runs once per bufferize_finalize_stores after the index table
// has settled.
static void bufferize_compute_costs(void) {
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) {
      b->recompute_ops      = 0;
      b->output_numel       = 0;
      b->recompute_total    = 0;
      b->subtree_has_reduce = 0;
      b->lifetime_start     = 0;
      b->lifetime_end       = 0;
      b->output_bytes       = 0;
      continue;
    }
    u8 has_red = 0;
    b->recompute_ops = bufferize_count_recompute_ops(b->loc, b->loc, 0,
                                                     &has_red);
    b->subtree_has_reduce = has_red;
    Term self = term_new(0, TAG_UOP, b->op, b->loc);
    Shape sh = {0};
    b->output_numel = term_shape_in(self, 0, &sh)
                        ? bufferize_shape_numel(&sh)
                        : 0;
    u32 dt = 0;
    b->output_bytes = (term_dtype_in(self, 0, &dt) && b->output_numel > 0)
                        ? b->output_numel * (u64)dtype_itemsize(dt)
                        : 0;
    u32 mult = b->consumer_count > 0 ? b->consumer_count : 1;
    b->recompute_total = (u64)b->recompute_ops * (u64)mult;
    // Phase 5: reduce metadata for UOP_REDUCE buffers.  Heap layout
    // for UOP_REDUCE is [src, NUM(kind), NUM(axis)], and the source
    // shape gives us the axis extent.
    b->reduce_kind      = 0;
    b->reduce_axis      = 0;
    b->reduce_axis_size = 0;
    if (b->op == UOP_REDUCE) {
      Term kind_cell = heap_read(b->loc + 1);
      Term axis_cell = heap_read(b->loc + 2);
      if (term_tag(kind_cell) == TAG_NUM) {
        b->reduce_kind = (u8)(term_val(kind_cell) & 0xFFu);
      }
      if (term_tag(axis_cell) == TAG_NUM) {
        b->reduce_axis = (u8)(term_val(axis_cell) & 0xFFu);
      }
      Term src = term_resolve(heap_read(b->loc + 0));
      Shape src_shape = {0};
      if (term_shape_in(src, 0, &src_shape)
          && b->reduce_axis < src_shape.ndim) {
        b->reduce_axis_size = src_shape.dims[b->reduce_axis];
      }
    }
  }
}

// Phase 6: per-buffer topological depth from B_INDEX edges.
// depth[i] = max(depth[s] for s in incoming sources) + 1; depth=1
// for buffers with no source-buffer edges (leaves).  Memoised via
// `visited[]`; bufferize is small (<= BUFFERIZE_GRAPH_CAP) so
// recursion depth is bounded by the buffer count.
static u32 BUFFERIZE_DEPTHS [BUFFERIZE_GRAPH_CAP];
static u8  BUFFERIZE_VISITED[BUFFERIZE_GRAPH_CAP];

static u32 bufferize_buffer_depth_rec(u32 idx) {
  if (idx >= BUFFERIZE_BUFS_LEN) return 0;
  if (BUFFERIZE_VISITED[idx]) return BUFFERIZE_DEPTHS[idx];
  BUFFERIZE_VISITED[idx] = 1;
  BUFFERIZE_DEPTHS [idx] = 1;     // cycle guard / default leaf depth
  if (!BUFFERIZE_BUFS[idx].realized) return BUFFERIZE_DEPTHS[idx];
  u32 cid = BUFFERIZE_BUFS[idx].buffer_id;
  u32 max_d = 0;
  for (u32 e = 0; e < BUFFERIZE_INDEXES_LEN; e++) {
    BIndex const *be = &BUFFERIZE_INDEXES[e];
    if (be->consumer_buffer_id != cid) continue;
    if (be->source_buffer_id == 0) continue;
    u32 sidx = be->source_buffer_id - 1;
    u32 sd = bufferize_buffer_depth_rec(sidx);
    if (sd > max_d) max_d = sd;
  }
  BUFFERIZE_DEPTHS[idx] = max_d + 1;
  return BUFFERIZE_DEPTHS[idx];
}

static void bufferize_compute_lifetimes(void) {
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BUFFERIZE_VISITED[i] = 0;
    BUFFERIZE_DEPTHS [i] = 0;
  }
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    if (!BUFFERIZE_BUFS[i].realized) continue;
    bufferize_buffer_depth_rec(i);
  }
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    b->lifetime_start = BUFFERIZE_DEPTHS[i];
    u32 last = BUFFERIZE_DEPTHS[i];
    u32 sid  = b->buffer_id;
    for (u32 e = 0; e < BUFFERIZE_INDEXES_LEN; e++) {
      BIndex const *be = &BUFFERIZE_INDEXES[e];
      if (be->source_buffer_id != sid) continue;
      if (be->consumer_buffer_id == 0) continue;
      u32 cidx = be->consumer_buffer_id - 1;
      if (cidx < BUFFERIZE_BUFS_LEN
          && BUFFERIZE_DEPTHS[cidx] > last) {
        last = BUFFERIZE_DEPTHS[cidx];
      }
    }
    b->lifetime_end = last;
  }
}

// Phase 3: recompute hit counts for the named index-* rules from
// the freshly-built B_INDEX table.  Each edge carrying a movement
// flag counts as one hit for the corresponding rule, mirroring how
// bufferize_rewrite_apply tracks hits for boundary rules.  This makes
// the implicit rangeify movement-folding visible as named rules in
// DUMP_BUFFERIZE without changing codegen behavior.
static void bufferize_update_index_rule_stats(void) {
  for (u32 i = 0; i < 6; i++) BUFFERIZE_INDEX_RULES[i].hits = 0;
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    BIndex const *e = &BUFFERIZE_INDEXES[i];
    if (e->has_reshape) BUFFERIZE_INDEX_RULES[0].hits++;
    if (e->has_permute) BUFFERIZE_INDEX_RULES[1].hits++;
    if (e->has_expand)  BUFFERIZE_INDEX_RULES[2].hits++;
    if (e->has_pad)     BUFFERIZE_INDEX_RULES[3].hits++;
    if (e->has_shrink)  BUFFERIZE_INDEX_RULES[4].hits++;
    if (e->has_flip)    BUFFERIZE_INDEX_RULES[5].hits++;
  }
}

static void bufferize_dump(Term root) {
  if (!bufferize_dump_enabled()) return;
  fprintf(stderr,
          "bufferize_summary buffers=%u realized=%u stores=%u root_loc=%llu\n",
          (unsigned)BUFFERIZE_BUFS_LEN,
          (unsigned)bufferize_realized_count(),
          (unsigned)BUFFERIZE_STORES_LEN,
          (unsigned long long)(term_tag(root) == TAG_UOP ? term_val(root) : 0));
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    fprintf(stderr,
            "  bufferize id=%u loc=%llu op=%u consumers=%u reasons=0x%x"
            " realized=%u%s%s%s%s%s\n",
            (unsigned)b->buffer_id,
            (unsigned long long)b->loc,
            (unsigned)b->op,
            (unsigned)b->consumer_count,
            (unsigned)b->reasons,
            (unsigned)b->realized,
            b->is_root ? " root" : "",
            b->added_by   ? " added_by="   : "",
            b->added_by   ? b->added_by    : "",
            b->removed_by ? " removed_by=" : "",
            b->removed_by ? b->removed_by  : "");
  }
  for (u32 i = 0; i < BUFFERIZE_STORES_LEN; i++) {
    BStore const *s = &BUFFERIZE_STORES[i];
    fprintf(stderr,
            "  store buffer_id=%u loc=%llu\n",
            (unsigned)s->buffer_id,
            (unsigned long long)s->loc);
  }
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    BIndex const *e = &BUFFERIZE_INDEXES[i];
    fprintf(stderr,
            "  index source=%u consumer=%u chain_len=%u%s%s%s%s%s%s\n",
            (unsigned)e->source_buffer_id,
            (unsigned)e->consumer_buffer_id,
            (unsigned)e->movement_chain_len,
            e->has_reshape ? " reshape" : "",
            e->has_permute ? " permute" : "",
            e->has_expand  ? " expand"  : "",
            e->has_pad     ? " pad"     : "",
            e->has_shrink  ? " shrink"  : "",
            e->has_flip    ? " flip"    : "");
  }
  for (u32 i = 0; i < bufferize_index_rule_count(); i++) {
    u32 hits = BUFFERIZE_INDEX_RULES[i].hits;
    if (hits == 0) continue;
    fprintf(stderr,
            "  index_rule %s hits=%u\n",
            BUFFERIZE_INDEX_RULES[i].name,
            (unsigned)hits);
  }
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    fprintf(stderr,
            "  bufferize_cost id=%u ops=%u numel=%llu bytes=%llu"
            " recompute_total=%llu lifetime=%u..%u%s\n",
            (unsigned)b->buffer_id,
            (unsigned)b->recompute_ops,
            (unsigned long long)b->output_numel,
            (unsigned long long)b->output_bytes,
            (unsigned long long)b->recompute_total,
            (unsigned)b->lifetime_start,
            (unsigned)b->lifetime_end,
            b->subtree_has_reduce ? " has_reduce" : "");
  }
}

fn void bufferize_seed_from_nodes(Term root) {
  BUFFERIZE_BUFS_LEN     = 0;
  BUFFERIZE_STORES_LEN   = 0;
  BUFFERIZE_INDEXES_LEN  = 0;

  // Non-UOp roots are not schedulable; leave the graph empty.
  if (term_tag(root) != TAG_UOP) return;
  if (term_ext(root) == UOP_KERNEL) return;
  u64 root_loc = term_val(root);

  // Snapshot every realized BUFFERIZE_NODES entry as a bufferize node
  // with realized=1, removed_by=added_by=NULL.  Insertion order in
  // BUFFERIZE_NODES is the walk order, so buffer ids stay deterministic.
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN
                  && BUFFERIZE_BUFS_LEN < BUFFERIZE_GRAPH_CAP; i++) {
    UOpInfo const *info = &BUFFERIZE_NODES[i];
    if (!info->realized) continue;
    BBufferize *b = &BUFFERIZE_BUFS[BUFFERIZE_BUFS_LEN];
    b->loc            = info->loc;
    b->buffer_id      = BUFFERIZE_BUFS_LEN + 1;
    b->reasons        = bufferize_project_reasons(info->reasons);
    b->consumer_count = info->consumer_count;
    b->op             = info->op;
    b->is_root        = (info->loc == root_loc) ? 1 : 0;
    b->realized       = 1;
    b->removed_by     = NULL;
    b->added_by       = NULL;
    BUFFERIZE_BUFS_LEN++;
  }

  // Phase 4 follow-up: compute the cost-model fields immediately so
  // rules running inside bufferize_rewrite_apply can read
  // bufferize_removal_score on the seed-time realized set.
  // bufferize_finalize_stores recomputes after rules so the dump
  // and post-rule callers see the final state.
  bufferize_compute_costs();
}

fn void bufferize_finalize_stores(Term root) {
  if (term_tag(root) != TAG_UOP) { bufferize_dump(root); return; }
  if (term_ext(root) == UOP_KERNEL) { bufferize_dump(root); return; }
  u64 root_loc = term_val(root);

  // One store per realize root for now; only emit if the root is
  // still realized (a rule could have promoted/aliased it later).
  u32 idx = bufferize_find_by_loc(root_loc);
  if (idx != 0xFFFFFFFFu
      && BUFFERIZE_BUFS[idx].realized
      && BUFFERIZE_STORES_LEN < BUFFERIZE_STORE_CAP) {
    BStore *s = &BUFFERIZE_STORES[BUFFERIZE_STORES_LEN++];
    s->buffer_id = BUFFERIZE_BUFS[idx].buffer_id;
    s->loc       = root_loc;
  }

  // Build the producer-buffer to consumer-buffer edge table once the
  // realized set has settled; this is the seed for Phase 4+ removal
  // and Phase 5 reduce rules and the data rangeify will eventually
  // consume in Phase 2's rangeify follow-up.
  bufferize_build_indexes();
  // Phase 3 transform: collapse identity reshapes before stats so
  // index-reshape hit counts reflect only meaningful folds.
  BUFFERIZE_IDENTITY_RESHAPE_HITS = bufferize_apply_identity_reshape();
  bufferize_update_index_rule_stats();
  bufferize_compute_costs();
  bufferize_compute_lifetimes();

  bufferize_dump(root);
  bufferize_dump_candidates();
}

fn u32 bufferize_buffer_count(void) {
  return BUFFERIZE_BUFS_LEN;
}

fn u32 bufferize_realized_count(void) {
  u32 n = 0;
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    if (BUFFERIZE_BUFS[i].realized) n++;
  }
  return n;
}

fn BBufferize const *bufferize_buffer_at(u32 i) {
  if (i >= BUFFERIZE_BUFS_LEN) return NULL;
  return &BUFFERIZE_BUFS[i];
}

fn u32 bufferize_find_by_loc(u64 loc) {
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    if (BUFFERIZE_BUFS[i].loc == loc) return i;
  }
  return 0xFFFFFFFFu;
}

fn u32 bufferize_store_count(void) {
  return BUFFERIZE_STORES_LEN;
}

fn BStore const *bufferize_store_at(u32 i) {
  if (i >= BUFFERIZE_STORES_LEN) return NULL;
  return &BUFFERIZE_STORES[i];
}

fn u32 bufferize_index_count(void) {
  return BUFFERIZE_INDEXES_LEN;
}

fn BIndex const *bufferize_index_at(u32 i) {
  if (i >= BUFFERIZE_INDEXES_LEN) return NULL;
  return &BUFFERIZE_INDEXES[i];
}

fn u32 bufferize_indexes_for_consumer(u32 consumer_buffer_id,
                                      u32 *out, u32 cap) {
  u32 n = 0;
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    if (BUFFERIZE_INDEXES[i].consumer_buffer_id != consumer_buffer_id) continue;
    if (out != NULL && n < cap) out[n] = i;
    n++;
  }
  return n;
}

fn u32 bufferize_indexes_for_source(u32 source_buffer_id,
                                    u32 *out, u32 cap) {
  u32 n = 0;
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    if (BUFFERIZE_INDEXES[i].source_buffer_id != source_buffer_id) continue;
    if (out != NULL && n < cap) out[n] = i;
    n++;
  }
  return n;
}

fn int bufferize_edge_summary(u64 consumer_loc, u64 source_loc, BIndex *out) {
  u32 cidx = bufferize_find_by_loc(consumer_loc);
  u32 sidx = bufferize_find_by_loc(source_loc);
  if (cidx == 0xFFFFFFFFu || sidx == 0xFFFFFFFFu) return 0;
  u32 cid = BUFFERIZE_BUFS[cidx].buffer_id;
  u32 sid = BUFFERIZE_BUFS[sidx].buffer_id;
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    BIndex const *e = &BUFFERIZE_INDEXES[i];
    if (e->consumer_buffer_id == cid && e->source_buffer_id == sid) {
      if (out != NULL) *out = *e;
      return 1;
    }
  }
  return 0;
}

fn u32 bufferize_index_rule_count(void) {
  return (u32)(sizeof(BUFFERIZE_INDEX_RULES) / sizeof(BUFFERIZE_INDEX_RULES[0]));
}

fn char const *bufferize_index_rule_name(u32 i) {
  if (i >= bufferize_index_rule_count()) return "";
  return BUFFERIZE_INDEX_RULES[i].name;
}

fn u32 bufferize_index_rule_hits(char const *name) {
  if (name == NULL) return 0;
  for (u32 i = 0; i < bufferize_index_rule_count(); i++) {
    if (strcmp(BUFFERIZE_INDEX_RULES[i].name, name) == 0) {
      return BUFFERIZE_INDEX_RULES[i].hits;
    }
  }
  return 0;
}

fn int bufferize_buffer_lifetime(u32 buffer_id,
                                 u32 *lifetime_start,
                                 u32 *lifetime_end) {
  if (buffer_id == 0 || buffer_id > BUFFERIZE_BUFS_LEN) return 0;
  BBufferize const *b = &BUFFERIZE_BUFS[buffer_id - 1];
  if (!b->realized) return 0;
  if (lifetime_start != NULL) *lifetime_start = b->lifetime_start;
  if (lifetime_end   != NULL) *lifetime_end   = b->lifetime_end;
  return 1;
}

// Phase 7: deterministic schedule key + aggregates.
fn u64 bufferize_schedule_key(void) {
  u64 h = 0xcbf29ce484222325ULL;
  u64 const prime = 0x100000001b3ULL;
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    u64 v = (u64)b->op
          ^ ((u64)b->reasons        <<  8)
          ^ ((u64)b->recompute_ops  << 24)
          ^ ((u64)b->output_numel   << 40);
    h = (h ^ v) * prime;
    // Step 5 of multi-output kernel groundwork: per-output fields
    // that distinguish merged-vs-unmerged schedules even when the
    // realized boundary set is identical.  Output bytes folds dtype
    // size into the key (numel alone collapses fp16 vs fp32 over
    // the same shape).  Lifetime widens the key so a schedule that
    // moves a buffer's last-use to a later stage (because of a
    // multi-output merge keeping it alive longer) gets a different
    // hash.  Reduce metadata + subtree_has_reduce ensure two
    // schedules whose reduce structure differs (and whose autotune
    // plans must therefore differ) cache separately.
    u64 v2 = (u64)b->output_bytes
           ^ ((u64)b->lifetime_start    << 32)
           ^ ((u64)b->lifetime_end      << 40)
           ^ ((u64)b->subtree_has_reduce << 48)
           ^ ((u64)b->reduce_kind       << 56)
           ^ ((u64)b->reduce_axis       << 60)
           ^ ((u64)b->reduce_axis_size  << 16);
    h = (h ^ v2) * prime;
  }
  for (u32 i = 0; i < BUFFERIZE_INDEXES_LEN; i++) {
    BIndex const *e = &BUFFERIZE_INDEXES[i];
    u64 flags = (u64)e->has_reshape
              | ((u64)e->has_permute << 1)
              | ((u64)e->has_expand  << 2)
              | ((u64)e->has_pad     << 3)
              | ((u64)e->has_shrink  << 4)
              | ((u64)e->has_flip    << 5);
    u64 v = (u64)e->source_buffer_id
          ^ ((u64)e->consumer_buffer_id << 16)
          ^ ((u64)e->movement_chain_len << 32)
          ^ (flags << 48);
    h = (h ^ v) * prime;
  }
  return h;
}

fn u64 bufferize_total_realized_bytes(void) {
  u64 total = 0;
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    total += b->output_bytes;
  }
  return total;
}

fn u32 bufferize_max_lifetime_depth(void) {
  u32 m = 0;
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    if (b->lifetime_end > m) m = b->lifetime_end;
  }
  return m;
}

fn u64 bufferize_total_recompute_ops(void) {
  u64 total = 0;
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) continue;
    total += b->recompute_ops;
  }
  return total;
}

fn u64 bufferize_removal_score(u32 buffer_id) {
  // 1-based ids; index = id - 1.
  if (buffer_id == 0 || buffer_id > BUFFERIZE_BUFS_LEN) return 0;
  BBufferize const *b = &BUFFERIZE_BUFS[buffer_id - 1];
  if (!b->realized) return 0;
  // Reason gates: only buffers seeded purely for MULTI (or unmarked
  // outright) are removal candidates.  ROOT is the realize output;
  // REDUCE has accumulator semantics that need explicit handling;
  // BACKEND_CAP was added to satisfy a hardware limit.
  if (b->reasons & (BUFFERIZE_REASON_ROOT
                  | BUFFERIZE_REASON_REDUCE
                  | BUFFERIZE_REASON_BACKEND_CAP)) {
    return 0;
  }
  // Phase 5 reduce-aware gate: was conservative because pre-FLAT_GRID
  // lift, kernels with nested reduces fell off the tile-JIT path onto
  // the per-op metal-op encoder, regressing wall time.  Commit 3b11bf6
  // lifted that path; consumers absorbing a reduce now stay on tile-JIT.
  // THVM_BUFFERIZE_REMOVE_SCORE_LIFT_REDUCE_GATE=0 reverts.
  char const *e_red = getenv("THVM_BUFFERIZE_REMOVE_SCORE_LIFT_REDUCE_GATE");
  int lift_reduce = (e_red == NULL) || (e_red[0] != '0');
  if (b->subtree_has_reduce && !lift_reduce) return 0;
  if (b->output_numel == 0) return 0;
  u64 denom = b->recompute_total > 0 ? b->recompute_total : 1;
  return b->output_numel / denom;
}
