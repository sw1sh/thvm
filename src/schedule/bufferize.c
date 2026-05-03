// schedule/bufferize.c - Phase 0 + Phase 1 of docs/plans/bufferize.md.
//
// Phase 0 was a post-rewrite mirror of REALIZE_INFO.  Phase 1 makes
// the bufferize graph live during the rewrite pass: every named
// realize-map rule that mutates REALIZE_INFO via realize_mark or
// realize_unmark is forwarded into bufferize_realize_with_reason or
// bufferize_unrealize, which stamps added_by / removed_by from the
// current rule pointer set by realize_rewrite_apply.
//
// Materialize.c still reads REALIZE_INFO directly, so the kernel
// schedule is unchanged.  The bufferize graph is the canonical
// rule-history record; future phases will let materialize consume it
// directly and let new rules edit only the graph.

static BBufferize BUFFERIZE_BUFS [BUFFERIZE_GRAPH_CAP];
static u32        BUFFERIZE_BUFS_LEN = 0;

#define BUFFERIZE_STORE_CAP 64
static BStore BUFFERIZE_STORES[BUFFERIZE_STORE_CAP];
static u32    BUFFERIZE_STORES_LEN = 0;

#define BUFFERIZE_INDEX_CAP REALIZE_INFO_CAP
#define BUFFERIZE_EDGE_DEPTH_CAP 96
static BIndex BUFFERIZE_INDEXES[BUFFERIZE_INDEX_CAP];
static u32    BUFFERIZE_INDEXES_LEN = 0;

// Phase 3: one entry per named index rewrite rule, hit count
// recomputed from the BUFFERIZE_INDEXES table after every
// realize_classify pass.  Order is fixed so callers can address by
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

// realize_rewrite_apply sets this around each rule->apply call so
// realize_mark/realize_unmark can stamp the rule that decided.
// NULL means "outside any named rule" (seeding, post-pass cleanup).
static char const *BUFFERIZE_CURRENT_RULE = NULL;

// Reason mirror: only the bits already on REALIZE_INFO map across.
// Phase 1 still uses inline REALIZE_REASON_INLINE to mark "a rule
// touched this", but the bufferize graph records the rule by name
// directly via removed_by, so we do not project INLINE.
static u32 bufferize_project_reasons(u32 r) {
  u32 out = 0;
  if (r & REALIZE_REASON_ROOT)      out |= BUFFERIZE_REASON_ROOT;
  if (r & REALIZE_REASON_MULTI)     out |= BUFFERIZE_REASON_MULTI;
  if (r & REALIZE_REASON_REDUCE)    out |= BUFFERIZE_REASON_REDUCE;
  if (r & REALIZE_REASON_FANIN_CAP) out |= BUFFERIZE_REASON_BACKEND_CAP;
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
  u32 idx = realize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return;
  u8 op = REALIZE_INFO[idx].op;
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
    bufferize_extend_chain(&chain_out, term_ext(child));
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

// Phase 4: count UOps in the producer subtree of `loc` for use as a
// recompute-cost estimate.  Stops at any other realized buffer
// (those would still cache the recomputed value), at REDUCE
// (recompute crosses a fusion boundary we can't cheaply replicate),
// at TEN/VAR leaves, and at any non-pure op.  Const and load are
// counted as zero-cost; other UOps each cost 1.  The walk is bounded
// by depth so pathological graphs cannot blow up the estimate.
static u32 bufferize_count_recompute_ops(u64 loc, u64 self_loc, u32 depth) {
  if (depth > 64) return 0;
  if (loc >= HEAP_NEXT) return 0;
  u32 ridx = realize_info_find(loc);
  if (ridx == 0xFFFFFFFFu) return 0;
  u8 op = REALIZE_INFO[ridx].op;
  // Stop at other realized buffers - their cost is amortised.
  if (loc != self_loc) {
    u32 bidx = bufferize_find_by_loc(loc);
    if (bidx != 0xFFFFFFFFu && BUFFERIZE_BUFS[bidx].realized) return 0;
  }
  if (op == UOP_REDUCE) return 1;     // count the reduce, don't descend
  u32 self_cost = (op == UOP_CONST || op == UOP_LOAD) ? 0 : 1;
  u8 ar = uop_arity(op);
  u32 total = self_cost;
  for (u8 i = 0; i < ar; i++) {
    Term child = term_resolve(heap_read(loc + i));
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    total += bufferize_count_recompute_ops(term_val(child), self_loc,
                                           depth + 1);
  }
  return total;
}

static u64 bufferize_shape_numel(Shape const *s) {
  if (s == NULL || s->ndim == 0) return 0;
  u64 n = 1;
  for (u32 i = 0; i < s->ndim; i++) n *= s->dims[i];
  return n;
}

// Populate the recompute_ops / output_numel / recompute_total cost
// fields for every realized B_BUFFERIZE.  Runs once per
// bufferize_finalize_stores after the index table has settled.
static void bufferize_compute_costs(void) {
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize *b = &BUFFERIZE_BUFS[i];
    if (!b->realized) {
      b->recompute_ops   = 0;
      b->output_numel    = 0;
      b->recompute_total = 0;
      continue;
    }
    b->recompute_ops = bufferize_count_recompute_ops(b->loc, b->loc, 0);
    Term self = term_new(0, TAG_UOP, b->op, b->loc);
    Shape sh = {0};
    b->output_numel = term_shape_in(self, 0, &sh)
                        ? bufferize_shape_numel(&sh)
                        : 0;
    u32 mult = b->consumer_count > 0 ? b->consumer_count : 1;
    b->recompute_total = (u64)b->recompute_ops * (u64)mult;
  }
}

// Phase 3: recompute hit counts for the named index-* rules from
// the freshly-built B_INDEX table.  Each edge carrying a movement
// flag counts as one hit for the corresponding rule, mirroring how
// realize_rewrite_apply tracks hits for boundary rules.  This makes
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
            "  bufferize_cost id=%u ops=%u numel=%llu recompute_total=%llu\n",
            (unsigned)b->buffer_id,
            (unsigned)b->recompute_ops,
            (unsigned long long)b->output_numel,
            (unsigned long long)b->recompute_total);
  }
}

fn void bufferize_seed_from_realize_info(Term root) {
  BUFFERIZE_BUFS_LEN     = 0;
  BUFFERIZE_STORES_LEN   = 0;
  BUFFERIZE_INDEXES_LEN  = 0;
  BUFFERIZE_CURRENT_RULE = NULL;

  // Non-UOp roots are not schedulable; leave the graph empty.
  if (term_tag(root) != TAG_UOP) return;
  if (term_ext(root) == UOP_KERNEL) return;
  u64 root_loc = term_val(root);

  // Snapshot every realized REALIZE_INFO entry as a bufferize node
  // with realized=1, removed_by=added_by=NULL.  Insertion order in
  // REALIZE_INFO is the walk order, so buffer ids stay deterministic.
  for (u32 i = 0; i < REALIZE_INFO_LEN
                  && BUFFERIZE_BUFS_LEN < BUFFERIZE_GRAPH_CAP; i++) {
    UOpInfo const *info = &REALIZE_INFO[i];
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
  bufferize_update_index_rule_stats();
  bufferize_compute_costs();

  bufferize_dump(root);
  bufferize_dump_candidates();
}

fn void bufferize_set_current_rule(char const *name) {
  BUFFERIZE_CURRENT_RULE = name;
}

fn char const *bufferize_current_rule(void) {
  return BUFFERIZE_CURRENT_RULE;
}

fn void bufferize_unrealize(u64 loc) {
  // Forwarders only take effect inside realize_rewrite_apply, which
  // sets the current-rule pointer.  Calls outside that window
  // (e.g. the ROOT/MULTI/REDUCE seeding pass that runs before the
  // graph has been (re)snapshotted) would otherwise mutate stale
  // state from a previous realize_classify.
  if (BUFFERIZE_CURRENT_RULE == NULL) return;
  u32 idx = bufferize_find_by_loc(loc);
  if (idx == 0xFFFFFFFFu) return;
  BBufferize *b = &BUFFERIZE_BUFS[idx];
  if (!b->realized) return;     // already removed; first rule wins
  b->realized   = 0;
  b->removed_by = BUFFERIZE_CURRENT_RULE;
}

fn void bufferize_realize_with_reason(u64 loc, u8 op, u32 reason) {
  if (BUFFERIZE_CURRENT_RULE == NULL) return;
  u32 idx = bufferize_find_by_loc(loc);
  if (idx != 0xFFFFFFFFu) {
    BBufferize *b = &BUFFERIZE_BUFS[idx];
    b->reasons |= bufferize_project_reasons(reason);
    if (!b->realized) {
      // A previously-removed buffer that a later rule re-realizes
      // (e.g. fanin-cap promoting a child).  Drop the removed_by
      // stamp - the new rule owns it now.
      b->realized   = 1;
      b->removed_by = NULL;
      b->added_by   = BUFFERIZE_CURRENT_RULE;
    }
    return;
  }
  // Brand-new buffer introduced by a rule (fanin-cap is the only
  // current example).  Look up consumer_count from REALIZE_INFO so
  // the bufferize record stays consistent with the projection.
  if (BUFFERIZE_BUFS_LEN >= BUFFERIZE_GRAPH_CAP) return;
  u32 ri = realize_info_find(loc);
  u32 cc = (ri != 0xFFFFFFFFu) ? REALIZE_INFO[ri].consumer_count : 0;
  u32 rb = (ri != 0xFFFFFFFFu) ? REALIZE_INFO[ri].reasons        : reason;
  BBufferize *b = &BUFFERIZE_BUFS[BUFFERIZE_BUFS_LEN];
  b->loc            = loc;
  b->buffer_id      = BUFFERIZE_BUFS_LEN + 1;
  b->reasons        = bufferize_project_reasons(rb);
  b->consumer_count = cc;
  b->op             = op;
  b->is_root        = 0;
  b->realized       = 1;
  b->removed_by     = NULL;
  b->added_by       = BUFFERIZE_CURRENT_RULE;
  BUFFERIZE_BUFS_LEN++;
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

fn u32 bufferize_index_rule_hits_at(u32 i) {
  if (i >= bufferize_index_rule_count()) return 0;
  return BUFFERIZE_INDEX_RULES[i].hits;
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
  if (b->output_numel == 0) return 0;
  u64 denom = b->recompute_total > 0 ? b->recompute_total : 1;
  return b->output_numel / denom;
}
