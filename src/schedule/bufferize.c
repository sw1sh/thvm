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
}

fn void bufferize_seed_from_realize_info(Term root) {
  BUFFERIZE_BUFS_LEN     = 0;
  BUFFERIZE_STORES_LEN   = 0;
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

  bufferize_dump(root);
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
