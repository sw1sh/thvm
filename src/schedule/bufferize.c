// schedule/bufferize.c - Phase 0 of docs/plans/bufferize.md.
//
// Project the REALIZE_INFO table that realize_classify produced into
// an explicit B_BUFFERIZE/B_STORE schedule graph.  This is the bridge
// representation: behaviour is unchanged, materialize.c still reads
// REALIZE_INFO directly, but every realized UOp now also has a stable
// buffer id, projected reason bits, and a visible store entry.
//
// Future phases will own the rewrite rules (remove / insert / split /
// merge / alias) and edge-local B_INDEX records.  Keeping the graph
// in this file means later changes can move boundaries by editing the
// graph instead of layering more side channels onto REALIZE_INFO.

static BBufferize BUFFERIZE_BUFS [BUFFERIZE_GRAPH_CAP];
static u32        BUFFERIZE_BUFS_LEN = 0;

// One store per realize root for now.  Phase 1+ will introduce
// multiple stores per kernel, accumulator stores, and so on.
#define BUFFERIZE_STORE_CAP 64
static BStore BUFFERIZE_STORES[BUFFERIZE_STORE_CAP];
static u32    BUFFERIZE_STORES_LEN = 0;

// Reason mirror.  Kept narrow at Phase 0 - only the bits already in
// REALIZE_INFO map across.  Removal/inline outcomes (REASON_INLINE)
// only matter once the bufferize pass owns the rewrite, so they are
// intentionally not projected; an inlined UOp simply has no buffer.
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
          "bufferize_summary buffers=%u stores=%u root_loc=%llu\n",
          (unsigned)BUFFERIZE_BUFS_LEN,
          (unsigned)BUFFERIZE_STORES_LEN,
          (unsigned long long)(term_tag(root) == TAG_UOP ? term_val(root) : 0));
  for (u32 i = 0; i < BUFFERIZE_BUFS_LEN; i++) {
    BBufferize const *b = &BUFFERIZE_BUFS[i];
    fprintf(stderr,
            "  bufferize id=%u loc=%llu op=%u consumers=%u reasons=0x%x%s\n",
            (unsigned)b->buffer_id,
            (unsigned long long)b->loc,
            (unsigned)b->op,
            (unsigned)b->consumer_count,
            (unsigned)b->reasons,
            b->is_root ? " root" : "");
  }
  for (u32 i = 0; i < BUFFERIZE_STORES_LEN; i++) {
    BStore const *s = &BUFFERIZE_STORES[i];
    fprintf(stderr,
            "  store buffer_id=%u loc=%llu\n",
            (unsigned)s->buffer_id,
            (unsigned long long)s->loc);
  }
}

fn void bufferize_build(Term root) {
  BUFFERIZE_BUFS_LEN   = 0;
  BUFFERIZE_STORES_LEN = 0;

  // Non-UOp roots are not schedulable; leave the graph empty.
  // Heap loc 0 is a valid allocation, so the only safe gate is the
  // tag check, not a loc-zero sentinel.
  if (term_tag(root) != TAG_UOP) return;
  if (term_ext(root) == UOP_KERNEL) return;
  u64 root_loc = term_val(root);

  // Phase 0: walk REALIZE_INFO in its natural insertion order and
  // mint one B_BUFFERIZE per realized UOp.  The 1-based ids are
  // contiguous so callers can use them as dense table keys.
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
    BUFFERIZE_BUFS_LEN++;
  }

  // Phase 0: exactly one store, the realize root.  Phase 1 will let
  // rewrite rules add stores for split kernels and accumulators.
  u32 idx = bufferize_find_by_loc(root_loc);
  if (idx != 0xFFFFFFFFu && BUFFERIZE_STORES_LEN < BUFFERIZE_STORE_CAP) {
    BStore *s = &BUFFERIZE_STORES[BUFFERIZE_STORES_LEN++];
    s->buffer_id = BUFFERIZE_BUFS[idx].buffer_id;
    s->loc       = root_loc;
  }

  bufferize_dump(root);
}

fn u32 bufferize_buffer_count(void) {
  return BUFFERIZE_BUFS_LEN;
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
