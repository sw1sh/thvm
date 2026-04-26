// schedule/materialize_memo.c - per-realize dedup memo for the
//                                materialize entry points.
//
// Without this memo, a UOp that's reachable through multiple
// parents (typical in grad chains, where the same forward
// activation feeds many backward partial-sums) gets re-emitted
// once per reaching path: each visit allocates a fresh
// KernelEntry with its own output buffer.  Under
// MATERIALIZE_USE_REALIZE_INFO this caused LeNet-class graphs to
// blow KERNELS_CAP even at 64K entries (see f1d-d4b1c diagnosis).
//
// Memo keys on the source UOp's heap loc (term_val(uop_term)) --
// stable across visits within a single thvm_materialize call.
// Cleared at the top of thvm_materialize so subsequent realizes
// start fresh (the dyn pool may roll back and reuse heap locs
// between realizes).

#define MATERIALIZE_MEMO_CAP  (1u << 15)        // 32768 slots
#define MATERIALIZE_MEMO_MASK (MATERIALIZE_MEMO_CAP - 1u)

// f1d-d4b2d: per-realize counters for diagnosing where kernels
// come from under toggle ON vs OFF.  Active only when THVM_MAT_STATS
// env var is set; otherwise zero-cost (just integer increments
// against unread counters).
u64 MAT_STATS_HELPER_OK    = 0;
u64 MAT_STATS_HELPER_BAIL  = 0;
u64 MAT_STATS_LEGACY_EXPR  = 0;
u64 MAT_STATS_LEGACY_WALK  = 0;
u64 MAT_STATS_MEMO_HITS    = 0;
u64 MAT_STATS_MEMO_STORES  = 0;

// Per-realize label for the THVM_MAT_STATS log.  Caller (WL probe via
// thvm_wl_mat_stats_label) sets it before calling thvm_realize; the
// dump line in thvm_materialize prints it then clears the buffer for
// the next realize.  Empty string -> no label printed.
char MAT_STATS_LABEL[64] = {0};

typedef struct {
  u64  loc;       // 0 = empty slot
  Term cached;
} MaterializeMemoSlot;

static MaterializeMemoSlot MATERIALIZE_MEMO[MATERIALIZE_MEMO_CAP];

fn void materialize_memo_clear(void) {
  memset(MATERIALIZE_MEMO, 0, sizeof(MATERIALIZE_MEMO));
}

// Knuth multiplicative hash: spreads sequential heap locs across
// the table so consecutive UOps don't pile up at adjacent slots
// and trigger long probe chains.
static u32 materialize_memo_index(u64 loc) {
  return (u32)((loc * 2654435761ull) & MATERIALIZE_MEMO_MASK);
}

fn Term materialize_memo_lookup(u64 loc) {
  if (loc == 0) return 0;
  u32 i = materialize_memo_index(loc);
  for (u32 probe = 0; probe < MATERIALIZE_MEMO_CAP; probe++) {
    MaterializeMemoSlot *s = &MATERIALIZE_MEMO[i];
    if (s->loc == 0)   return 0;
    if (s->loc == loc) { MAT_STATS_MEMO_HITS++; return s->cached; }
    i = (i + 1u) & MATERIALIZE_MEMO_MASK;
  }
  return 0;
}

fn void materialize_memo_store(u64 loc, Term t) {
  if (loc == 0) return;
  // Only cache "fully materialized" terms.  An un-materialized
  // TAG_UOP (e.g., the toggle-ON inlinable un-realized branch
  // returns the input unchanged so a downstream realized parent
  // can absorb it) must not be cached: a later materialize_expr
  // pass would memo-hit and skip emitting the legacy per-UOp
  // kernel that the toggle-OFF path needs.
  u8 tag = term_tag(t);
  if (tag == TAG_UOP && term_ext(t) != UOP_KERNEL) return;
  u32 i = materialize_memo_index(loc);
  for (u32 probe = 0; probe < MATERIALIZE_MEMO_CAP; probe++) {
    MaterializeMemoSlot *s = &MATERIALIZE_MEMO[i];
    if (s->loc == 0 || s->loc == loc) {
      s->loc    = loc;
      s->cached = t;
      MAT_STATS_MEMO_STORES++;
      return;
    }
    i = (i + 1u) & MATERIALIZE_MEMO_MASK;
  }
  // Table full -- silently drop the entry; correctness is
  // preserved (caller still gets the freshly computed term),
  // we just lose the dedup opportunity for this loc.
}
