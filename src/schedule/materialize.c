// schedule/materialize.c - tinygrad-style scheduler.
//
// g2a: bufferize_classify + topo_sort_boundaries populate
// BOUNDARY_ORDER (kernel emit order).
// g2b: build_kernel emits one KernelEntry per boundary by visiting
//      its UOp subgraph and inlining every non-boundary upstream
//      elementwise / CONST / LOAD / REDUCE-as-tail op into the
//      kernel's lifted UOp DAG.  The visit walk records input
//      bindings; the lift (kernel_lift_to_uop) packages the
//      bufferize-rooted subtree into cached_lift.store_root.
//      Movement ops (g2c) and GRAD (g2d) walks return 0xDEADBEEF
//      on unsupported shapes, making the boundary's emit bail and
//      thvm_materialize fall back to returning the input unchanged.

// Forward decls into jit/capture.c (included AFTER this file in the unity
// build).  The movement-op-root gather needs to know whether a TJit
// closure is mid-record, and to record a replayable JIT_OP_GATHER so the
// one-shot host gather re-runs off the live source on every replay.
fn int  jit_is_capturing(void);
fn void jit_capture_record_gather(u32 src_tid, u32 dst_buf);

#define BOUNDARY_ORDER_CAP 16384
static u64  BOUNDARY_ORDER[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_TID  [BOUNDARY_ORDER_CAP];   // emitted output TenDesc id
static Term BOUNDARY_TERM [BOUNDARY_ORDER_CAP];   // emitted UOP_KERNEL term

// Bypass-substitution telemetry: per-kernel counts for each safety
// gate.  Read via bypass_kernel_*_count() from thvm.c's coverage dump.
static u64 BYPASS_KERNEL_TOTAL         = 0;
static u64 BYPASS_KERNEL_USED_UNIFIED  = 0;
static u64 BYPASS_GATE_RESID           = 0;
static u64 BYPASS_GATE_STRANDED        = 0;
static u64 BYPASS_GATE_BCAST           = 0;
fn u64  bypass_kernel_total_count       (void) { return BYPASS_KERNEL_TOTAL;        }
fn u64  bypass_kernel_used_unified_count(void) { return BYPASS_KERNEL_USED_UNIFIED; }
fn u64  bypass_gate_resid_count         (void) { return BYPASS_GATE_RESID;          }
fn u64  bypass_gate_stranded_count      (void) { return BYPASS_GATE_STRANDED;       }
fn u64  bypass_gate_bcast_count         (void) { return BYPASS_GATE_BCAST;          }
// Per-BOUNDARY_ORDER slot, the UOP_BUFFERIZE Term the unified
// rangeify pass emitted at this boundary (0 when the boundary has
// no unified-pass record).  Populated by topo_sort_boundaries when
// the unified pass produced a non-zero RU_BUFFERIZE_TERM;
// emit_kernel_for_boundary stashes this onto ke->compute_bufferize.
// Mirror: tinygrad/schedule/indexing.py:77 lands a UOp(Ops.BUFFERIZE,
// ...) per realize boundary; the scheduler downstream walks those.
static Term BOUNDARY_BUFFERIZE_TERM[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_ORDER_LEN = 0;

// Open-addressed loc -> BOUNDARY_ORDER index hash.  Without it,
// boundary_index_for_loc was an O(BOUNDARY_ORDER_LEN) scan per
// visited UOP child, called from every emit_kernel_for_boundary's
// visit() recursion -- the dominant cost above n~12 in the bound-w
// SGD pattern (O(N^3) materialize).
#define BOUNDARY_HASH_CAP   (1u << 16)        // 64K slots, BOUNDARY_ORDER_CAP = 16384
#define BOUNDARY_HASH_EMPTY 0xFFFFFFFFu
static u32 BOUNDARY_HASH[BOUNDARY_HASH_CAP];

static inline u32 boundary_hash_of(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (BOUNDARY_HASH_CAP - 1);
}

static void boundary_hash_clear(void) {
  for (u32 i = 0; i < BOUNDARY_HASH_CAP; i++) BOUNDARY_HASH[i] = BOUNDARY_HASH_EMPTY;
}

static void boundary_hash_insert(u64 loc, u32 idx) {
  u32 h = boundary_hash_of(loc);
  for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
    if (BOUNDARY_HASH[i] == BOUNDARY_HASH_EMPTY) {
      BOUNDARY_HASH[i] = idx;
      return;
    }
  }
}

// Persistent loc -> tid cache: once emit_kernel_for_boundary realizes
// a UOP at heap address `loc` into a kernel whose output is TENS[tid],
// remember the mapping so a subsequent realize call hitting the same
// UOP loc (typical for forward intermediates shared across multiple
// backward grad targets) short-circuits to TAG_TEN(tid) instead of
// re-bufferizing + re-emitting the same kernel.  Mirror context:
// tinygrad/schedule/indexing.py:pm_generate_realize_map memoizes
// realized UOps via the per-UOp .buffer attribute on the lazy graph
// (engine/realize.py:lower_uop -> sched.metadata['_realize_cache']);
// once a UOp is scheduled, downstream schedules read its assigned
// Buffer instead of re-lowering.  thvm has no per-UOp .buffer field --
// the source UOPs live in the read-only dyn heap -- so we keep the
// mapping in a side hash keyed by heap loc.
//
// Bounded scan; the cap above absorbs hash collisions.  Cleared on
// kernel-GC sweep (kernel_gc.c) so a recycled tid never aliases an
// orphaned UOP loc, and on tens-arena rewind (gc_collect) so a
// reclaimed loc never lives in the table referencing a stale tid.
#define MATERIALIZED_LOC_CAP    (1u << 17)        // 128K slots
#define MATERIALIZED_LOC_EMPTY  0
typedef struct {
  u64 loc;
  u32 tid;
} MaterializedLocEntry;
static MaterializedLocEntry MATERIALIZED_LOC_TABLE[MATERIALIZED_LOC_CAP];
static u32                  MATERIALIZED_LOC_LEN = 0;

// Heap-rewrite journal.  materialize_subst_cached_rec replaces a parent
// UOP cell that points at a cached child subtree with a TAG_TEN leaf
// (heap_set), so the same pass doesn't re-materialize the shared subtree.
// That mutation is PERMANENT on the global (hash-consed) heap, but the
// loc->tid cache backing it is per-realize (cleared on every realize
// boundary -- see thvm_realize).  Across realizes the substituted tid's
// buffer is reclaimed by the pool rollback, so a later realize that
// re-walks the SAME hash-consed node would read a TAG_TEN leaf naming a
// tid whose buffer is dead -- tensor_view_of then re-increfs a drained
// CpuBuf, the producer kernel dispatches into a NULL data pointer, and
// the consumer segfaults.  To keep the rewrite per-realize (matching the
// cache lifetime), every substitution is journaled here and restored to
// the original child term when the cache is cleared.  The substitution
// only ever targets UOP-produced intermediates (params are already
// TAG_TEN and never get rewritten), so reverting them all at the realize
// boundary is exactly correct -- the next realize re-runs the
// substitution against its own live cache.
#define SUBST_JOURNAL_CAP  (1u << 16)
typedef struct {
  u64  cell;     // heap cell that was overwritten (loc + child index)
  Term orig;     // original child term to restore
} SubstJournalEntry;
static SubstJournalEntry SUBST_JOURNAL[SUBST_JOURNAL_CAP];
static u32               SUBST_JOURNAL_LEN = 0;

// Record a heap rewrite so materialized_loc_clear can undo it.  Returns
// 1 if journaled (caller may proceed with heap_set), 0 if the journal is
// full (caller must NOT rewrite -- recurse instead so correctness holds
// even without the dedup).
static int subst_journal_record(u64 cell, Term orig) {
  if (SUBST_JOURNAL_LEN >= SUBST_JOURNAL_CAP) return 0;
  SUBST_JOURNAL[SUBST_JOURNAL_LEN].cell = cell;
  SUBST_JOURNAL[SUBST_JOURNAL_LEN].orig = orig;
  SUBST_JOURNAL_LEN++;
  return 1;
}

static void subst_journal_restore(void) {
  // Reverse order: a cell rewritten more than once (shouldn't happen --
  // the visited bitmap bounds it -- but be safe) restores to its
  // earliest original.
  for (u32 i = SUBST_JOURNAL_LEN; i > 0; i--) {
    heap_set(SUBST_JOURNAL[i - 1].cell, SUBST_JOURNAL[i - 1].orig);
  }
  SUBST_JOURNAL_LEN = 0;
}

// === cross-pass consumer count ======================================
//
// bufferize_classify runs per inner-materialize SUBGRAPH (one CTR child,
// one ASSIGN src), wiping BUFFERIZE_NODES each call.  So its
// consumer_count is LOCAL to that subgraph.  But a forward activation is
// shared across sibling subgraphs (a residual read by two grad targets,
// or a loss read by the loss-assign AND a param-update): the FIRST
// subgraph that materializes it sees consumer_count==1 (only its own one
// reader) and the arena planner recycles its slot at that subgraph's
// local last_use; a later sibling subgraph then reuses the cached tid
// (materialized_loc_lookup) and reads the recycled-over bytes -> NaN.
//
// tinygrad has no such gap: schedule/memory.py plans lifetimes over ONE
// linearized schedule of the WHOLE step (memory.py:28 enumerates every
// linear.src; last_appearance spans every consumer across forward +
// backward + optimizer).  thvm fragments into per-subgraph passes, so
// the per-subgraph consumer_count can't see cross-subgraph readers.
//
// XPASS_CC bridges that: each materialize-loop iteration we walk EVERY
// top-level root (each CTR child / single root) SEPARATELY, marking every
// UOp loc it reaches.  A loc reached by >= 2 distinct roots is shared
// across sub-passes (a forward activation read by two grad targets); the
// FIRST sub-pass to materialize it caches the tid, and a later sibling
// reads that cached tid via the cache.  The arena planner consults
// xpass_is_shared() and EXTENDS such a boundary's arena lifetime to the
// end of its pass (arena_compute) so its offset is never recycled within
// the pass -- the cached bytes stay valid for every sibling.  The buffer
// stays in the arena (it still shares bytes with non-overlapping
// lifetimes), so non-shared recycling is untouched and peak is unchanged.
// Counting distinct ROOTS (not parent edges) catches the share even when
// each sub-pass references the loc exactly once -- the cache, not a
// structural edge, is what unifies them.  Hash-consing keeps locs stable
// within a realize.  Tinygrad mirror: schedule/memory.py last_appearance
// spans the whole linearized schedule, so a buf live across multiple
// consumers is never freed early.
#define XPASS_CC_CAP   (1u << 17)        // 128K slots; loc-keyed open-addr
typedef struct { u64 loc; u32 count; } XPassCCEntry;
static XPassCCEntry XPASS_CC[XPASS_CC_CAP];
static u32          XPASS_CC_LEN    = 0;
static u8           XPASS_CC_ACTIVE = 0;  // 1 once a realize populated it

static inline u32 xpass_cc_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (XPASS_CC_CAP - 1);
}

// Bump loc's root-reach count by one (called once per root that reaches
// it -- the per-root visited bitmap dedups within a root).
static void xpass_cc_bump(u64 loc) {
  if (XPASS_CC_LEN * 2 > XPASS_CC_CAP) return;   // table full: leave as-is
  u32 h = xpass_cc_hash(loc);
  for (u32 probe = 0; probe < XPASS_CC_CAP; probe++) {
    u32 i = (h + probe) & (XPASS_CC_CAP - 1);
    if (XPASS_CC[i].loc == 0 && XPASS_CC[i].count == 0) {
      XPASS_CC[i].loc = loc; XPASS_CC[i].count = 1; XPASS_CC_LEN++; return;
    }
    if (XPASS_CC[i].loc == loc) { XPASS_CC[i].count++; return; }
  }
}

static u32 xpass_root_reach(u64 loc) {
  if (!XPASS_CC_ACTIVE) return 0;
  u32 h = xpass_cc_hash(loc);
  for (u32 probe = 0; probe < XPASS_CC_CAP; probe++) {
    u32 i = (h + probe) & (XPASS_CC_CAP - 1);
    if (XPASS_CC[i].loc == 0 && XPASS_CC[i].count == 0) return 0;
    if (XPASS_CC[i].loc == loc) return XPASS_CC[i].count;
  }
  return 0;
}

// True iff `loc` is reached by >= 2 top-level roots -> shared across
// sub-passes, unsafe for the per-pass arena planner to recycle.
static int xpass_is_shared(u64 loc) { return xpass_root_reach(loc) >= 2; }

// Non-mutating resolve of SUB-bit VAR / DP indirection.  Unlike
// term_resolve it does NOT call alo_force: this walk runs BEFORE
// materialize and forcing an ALO layer mutates the heap, which perturbs
// the faithful-seed realize's downstream graph (loss drift).  An ALO not
// yet forced is returned as-is and the wrapper-descent below stops there
// -- a conservative MISS, which only means a shared loc behind an unforced
// ALO is treated as non-shared (loses the safety extension for that loc,
// never adds a spurious one).  In practice the share-relevant references
// (forward activations read by grad targets) are already resolved by the
// time the populate walk runs.
static Term xpass_resolve_nf(Term t) {
  for (int hops = 0; hops < 256; hops++) {
    u8 tag = term_tag(t);
    if (tag == TAG_VAR) {
      Term cell = heap_read(term_val(t));
      if (!term_sub_get(cell)) return t;
      t = term_sub_set(cell, 0);
      continue;
    }
    if (tag == TAG_DP0 || tag == TAG_DP1) {
      Term cell = heap_read(term_val(t));
      if (term_sub_get(cell)) { t = term_sub_set(cell, 0); continue; }
      if (tag == TAG_DP0 && (term_ext(t) & DUP_GRAD_FLAG)) {
        t = heap_read(term_val(t)); continue;
      }
      return t;
    }
    return t;
  }
  return t;
}

// Resolve a term to the first UOP underneath any structural wrapper
// (DP0/DP1 grad projections, APP/LAM/SUP) the grad chain rule introduces.
// Uses the non-mutating resolve so the pre-materialize walk doesn't force
// ALO layers.  Returns the UOP term, or 0 if none.
static Term xpass_resolve_uop(Term t, int depth) {
  if (depth > 64) return 0;
  t = xpass_resolve_nf(t);
  u8 tag = term_tag(t);
  if (tag == TAG_UOP) return (term_ext(t) == UOP_KERNEL) ? 0 : t;
  u32 ar = 0;
  switch (tag) {
    case TAG_DP0: case TAG_DP1: case TAG_LAM: case TAG_DUP: ar = 1; break;
    case TAG_APP: case TAG_SUP: case TAG_OP2: case TAG_MAT: ar = 2; break;
    default: return 0;
  }
  u64 loc = term_val(t);
  for (u32 i = 0; i < ar; i++) {
    Term u = xpass_resolve_uop(heap_read(loc + i), depth + 1);
    if (u != 0) return u;
  }
  return 0;
}

// Walk ONE root's UOp DAG, marking each reached loc as reached-by-this-
// root (bump once via the per-root visited bitmap).  `visited` is sized
// to HEAP_NEXT and reset per root by the caller.
static void xpass_cc_walk_rec(Term t, u8 *visited, u64 cap) {
  if (term_tag(t) != TAG_UOP) {
    Term u = xpass_resolve_uop(t, 0);
    if (u == 0) return;
    t = u;
  }
  u32 op = term_ext(t);
  if (op == UOP_KERNEL) return;
  u64 loc = term_val(t);
  if (loc >= cap) return;
  if (visited[loc]) return;
  visited[loc] = 1;
  xpass_cc_bump(loc);
  u8 ar = uop_arity((u8)op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 i = 0; i < ar; i++) {
    Term child = xpass_resolve_uop(heap_read(loc + i), 0);
    if (child == 0) continue;
    u64 cloc = term_val(child);
    u8 dup = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    xpass_cc_walk_rec(child, visited, cap);
  }
}

fn void xpass_cc_reset(void) {
  for (u32 i = 0; i < XPASS_CC_CAP; i++) { XPASS_CC[i].loc = 0; XPASS_CC[i].count = 0; }
  XPASS_CC_LEN = 0;
  XPASS_CC_ACTIVE = 0;
}

// Populate XPASS_CC from a realize root (single Term or a TAG_CTR
// bundle).  Each top-level root gets a FRESH visited bitmap so a loc
// reached by two roots is counted twice (-> shared).  Called each
// materialize-loop iteration (resets first) so a backward graph that
// grows across the fixpoint loop is fully covered.
fn void xpass_cc_populate(Term root) {
  xpass_cc_reset();
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;
  if (term_tag(root) == TAG_CTR) {
    u32 n = term_ctr_n(root);
    for (u32 i = 0; i < n && i < 256; i++) {
      memset(visited, 0, cap);
      xpass_cc_walk_rec(term_ctr_at(root, i), visited, cap);
    }
  } else {
    xpass_cc_walk_rec(root, visited, cap);
  }
  free(visited);
  XPASS_CC_ACTIVE = 1;
}

// In-realize scope counter: thvm_realize / thvm_realize_many bracket
// their bodies with materialized_loc_scope_{enter,leave}.  The cache
// is consulted by thvm_materialize ONLY when depth > 0 -- tests that
// call thvm_materialize / bufferize_classify directly (without going
// through a realize wrapper) skip the cross-realize substitution so
// their fresh terms aren't unexpectedly rewritten by stale cache
// entries left over from earlier tests in the same binary.
static u32 MATERIALIZE_SCOPE_DEPTH = 0;
fn void materialized_loc_scope_enter(void) { MATERIALIZE_SCOPE_DEPTH++; }
fn void materialized_loc_scope_leave(void) {
  if (MATERIALIZE_SCOPE_DEPTH > 0) MATERIALIZE_SCOPE_DEPTH--;
}
fn u32  materialized_loc_scope_depth(void) { return MATERIALIZE_SCOPE_DEPTH; }

// JIT-capture realize-dedup span.  A single JIT-captured training step
// calls thvm_realize / thvm_realize_many several times (loss.realize,
// then the grad bundle, then the optimizer-step bundle).  Normally the
// loc->tid cache is cleared at every realize boundary, so the forward +
// grad kernels shared across those realizes get RE-emitted (fresh kids)
// and re-recorded into the capture -- the documented dispatch
// redundancy.  Across realizes the clear is needed because pool-rollback
// reclaims a non-preserved intermediate's buffer.  But JIT capture pins
// every recorded kernel output (jit_capture_retain_dispatch_bufs ->
// buf_jit_pin), and rollback skips pinned bufs, so those outputs survive
// the boundary.  Persisting the cache across the span therefore lets a
// later realize's materialize substitute the EXISTING tid (one kernel,
// one dispatch) instead of re-emitting.  Safety net: materialized_loc_lookup
// already treats a dead-buffer tid as a miss, so a non-pinned (rolled-back)
// intermediate falls back to a fresh re-emit -- correctness is preserved
// even if a buffer didn't survive.  Mirror: tinygrad schedules the whole
// `loss.realize(*opt.schedule_step())` step in ONE run_rangeify pass, so a
// shared kernel is scheduled exactly once.
//
// Gated by THVM_JIT_REALIZE_DEDUP (default OFF -- the env-knob discipline
// for an aliasing-sensitive path).  When the span is active and enabled,
// materialized_loc_clear() defers the wipe; the deferred clear runs at
// materialized_loc_jit_span_end().
static u32 MATERIALIZE_JIT_SPAN_DEPTH = 0;

// Set the first time a maxpool MAX-reduce vjp is built (interact/uop_grad.c
// REDUCE_MAX branch).  The /count argmax-tie split (gradient.py:11-14) is only
// correct if the maxpool-input activation, the forward MAX, and the backward
// mask read ONE materialized buffer.  Across the captured step's separate
// realize calls (BN running-stat realizes, loss.realize, the grad bundle) that
// requires the cross-realize materialized_loc span to persist -- otherwise each
// realize re-materializes the activation into a fresh buffer and the two copies
// fp-disagree at an argmax tie -> CMPEQ miss -> count 0 -> RECIP(0) NaN.  So a
// maxpool grad in the live graph IMPLIES the dedup span, exactly as the
// faithful seed does (verified: without the span the default-seed stacked-
// maxpool training NaNs at step 2; with it the loss curve matches faithful).
static int MAXPOOL_GRAD_SEEN = 0;
fn void materialize_note_maxpool_grad(void) { MAXPOOL_GRAD_SEEN = 1; }
static int MATERIALIZE_JIT_DEDUP_ENABLED(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_JIT_REALIZE_DEDUP");
    on = (e != NULL && e[0] != '0' && e[0] != '\0');
    known = 1;
  }
  // The faithful realize-seed (THVM_RU_FAITHFUL_SEED) produces FEWER
  // boundaries, so a forward intermediate is shared across realizes (forward
  // in realize-1, read by backward in realize-2) WITHOUT a unifying boundary.
  // The cross-realize materialized_loc dedup is what unifies the producer's
  // output tid with the consumer's input tid; without it the JIT capture
  // records divergent buf_ids (producer reallocs across realizes) and replay
  // reads stale intermediates -> divergence/explosion.  So faithful IMPLIES
  // the dedup (verified: faithful+dedup converges on the JIT replay, faithful
  // alone diverges).  A maxpool grad implies it for the same reason (see
  // MAXPOOL_GRAD_SEEN).
  return on || ru_faithful_seed_on() || MAXPOOL_GRAD_SEEN;
}
static int materialized_loc_span_holds(void) {
  // The cross-realize dedup keeps one realize's materialized boundary so a
  // later realize in the same captured step substitutes it (one kernel,
  // one recorded dispatch) instead of re-emitting + re-firing it.  Its
  // load-bearing correctness contract is kernel_gc_sweep detaching every
  // preserved boundary buffer's producer kid (backend-aware), so the
  // depth-first fire reads the materialized leaf rather than re-firing the
  // producer chain across realizes.  With that GC fix the span is correct
  // on every backend the GC sweeps (CPU + CUDA) and follows the single
  // THVM_JIT_REALIZE_DEDUP master gate.
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0 || !MATERIALIZE_JIT_DEDUP_ENABLED()) {
    return 0;
  }
  if (CURRENT_BACKEND == &CPU_BACKEND) return 1;
#ifdef THVM_HAS_CUDA
  if (CURRENT_BACKEND == &CUDA_BACKEND) return 1;
#endif
  return 0;
}
fn void materialized_loc_clear(void);   // fwd decl (span_end calls it)
fn void materialized_loc_jit_span_begin(void) {
  // Track the captured-step bracket UNCONDITIONALLY (the depth marks "inside a
  // JIT capture").  Whether the span actually DEFERS the cross-realize clear is
  // decided lazily by materialized_loc_span_holds, which re-checks
  // MATERIALIZE_JIT_DEDUP_ENABLED() at clear time.  This matters for the
  // maxpool auto-enable (MAXPOOL_GRAD_SEEN): the flag is set DURING the step's
  // realize (when the MAX-reduce vjp is built), AFTER this begin runs, so an
  // early gate here would miss it and let the activation re-materialize per
  // realize -> the stacked-maxpool NaN.  When the gate is off at clear time the
  // span behaves exactly as the old per-realize clear.
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) materialized_loc_clear();
  MATERIALIZE_JIT_SPAN_DEPTH++;
}
fn void materialized_loc_jit_span_end(void) {
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) return;
  MATERIALIZE_JIT_SPAN_DEPTH--;
  // Real clear (restore journal + wipe) now that the span's realizes are
  // all done.  The heap rewrites that the span accumulated are undone so
  // the hash-consed graph is back to its original shape for replay /
  // future realizes.
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) materialized_loc_clear();
}

static inline u32 materialized_loc_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (MATERIALIZED_LOC_CAP - 1);
}

fn void materialized_loc_clear(void) {
  // Undo this realize's heap rewrites BEFORE wiping the cache so the
  // hash-consed UOP graph is back to its original shape for the next
  // realize (the substituted tids' buffers don't survive the pool
  // rollback -- see SUBST_JOURNAL).  The journal restore ALWAYS runs,
  // even inside a JIT-capture span: it returns the in-place TAG_TEN
  // substitutions to their original UOP children so the NEXT realize in
  // the span re-derives every substitution through materialized_loc_lookup
  // (whose dead-buffer guard re-validates that the cached tid's buffer is
  // still live).  Without that re-validation a stale, recycled tid could
  // be re-bound blind.
  subst_journal_restore();
  // Inside an active JIT-capture realize-dedup span KEEP the loc->tid map
  // so a later realize in the same captured step can substitute the
  // kernel this one emitted (one kernel, one recorded dispatch) instead
  // of re-emitting it.  Buffers stay live because JIT capture pins every
  // recorded kernel output (buf_jit_pin); a non-pinned intermediate that
  // got rolled back fails the lookup's dead-buffer guard -> safe re-emit.
  // Deferred full wipe runs at materialized_loc_jit_span_end().
  if (materialized_loc_span_holds()) return;
  for (u32 i = 0; i < MATERIALIZED_LOC_CAP; i++) {
    MATERIALIZED_LOC_TABLE[i].loc = MATERIALIZED_LOC_EMPTY;
    MATERIALIZED_LOC_TABLE[i].tid = 0;
  }
  MATERIALIZED_LOC_LEN = 0;
}

fn u32 materialized_loc_lookup(u64 loc) {
  if (loc == 0) return 0;
  u32 h = materialized_loc_hash(loc);
  for (u32 probe = 0; probe < MATERIALIZED_LOC_CAP; probe++) {
    u32 i = (h + probe) & (MATERIALIZED_LOC_CAP - 1);
    if (MATERIALIZED_LOC_TABLE[i].loc == MATERIALIZED_LOC_EMPTY) return 0;
    if (MATERIALIZED_LOC_TABLE[i].loc == loc) {
      u32 tid = MATERIALIZED_LOC_TABLE[i].tid;
      // Defensive: a cleared/recycled tid (TENS_NEXT shrank past it)
      // must not be returned -- caller would build a dangling TAG_TEN.
      if (tid == 0 || tid >= TENS_NEXT) return 0;
      // This cache is NON-OWNING: it records loc -> tid but does not pin
      // the tid's backing buffer.  A prior realize's intermediate can have
      // its buffer reclaimed by cpu_buf_pool_rollback_with_preserve (the
      // reference graph no longer needs it) while this stale entry lingers.
      // The tid stays in range, so the TENS_NEXT guard above passes, but
      // its buffer is dead (refcount 0 / unallocated).  Returning it makes
      // the consumer view-resolve onto a drained slot: tensor_view_of
      // re-increfs a dead CpuBuf, the producer kernel then dispatches into
      // a NULL data pointer, and the downstream read segfaults.  Treat a
      // dead-buffer tid as a miss so the caller re-materializes fresh
      // (correctness intact; only the cross-realize cache hit is lost).
      Backend *b   = TENS[tid].backend;
      u32      bid = TENS[tid].buf_id;
      if (bid == 0) return 0;
      if (b != NULL && b->buf_refcount != NULL && b->buf_refcount(bid) == 0)
        return 0;
      return tid;
    }
  }
  return 0;
}

fn void materialized_loc_insert(u64 loc, u32 tid) {
  if (loc == 0 || tid == 0) return;
  // Bail when the table is mostly full; the lookup degrades to a long
  // miss probe otherwise.  Caller falls back to re-emit (correctness
  // intact, just no cross-realize savings).
  if (MATERIALIZED_LOC_LEN * 2 > MATERIALIZED_LOC_CAP) return;
  u32 h = materialized_loc_hash(loc);
  for (u32 probe = 0; probe < MATERIALIZED_LOC_CAP; probe++) {
    u32 i = (h + probe) & (MATERIALIZED_LOC_CAP - 1);
    if (MATERIALIZED_LOC_TABLE[i].loc == MATERIALIZED_LOC_EMPTY) {
      MATERIALIZED_LOC_TABLE[i].loc = loc;
      MATERIALIZED_LOC_TABLE[i].tid = tid;
      MATERIALIZED_LOC_LEN++;
      return;
    }
    if (MATERIALIZED_LOC_TABLE[i].loc == loc) {
      // Already present: update tid (a re-emit in the same pass replaced
      // the prior kernel; keep the newer mapping).
      MATERIALIZED_LOC_TABLE[i].tid = tid;
      return;
    }
  }
}

// === UOP_COPY persistent device-upload cache ========================
// Maps (copy_node_heap_loc, target_backend_id) -> uploaded TenDesc tid.
// Unlike MATERIALIZED_LOC_TABLE (per-realize), this survives ACROSS
// realizes: a UOP_COPY node is hash-consed by its src, so its heap loc
// is stable, and the uploaded weight buffer is static (the src is an
// immutable host leaf).  Re-realize / JIT-replay therefore reuses the
// already-uploaded device buffer instead of re-staging the bytes every
// realize.  The uploaded buffer is jit-pinned (sticky retain) so the
// per-realize pool rollback in realize.c does not reclaim it.  Mirror:
// tinygrad caches the COPY's dest Buffer on the lazy graph
// (engine/realize.py exec_copy reuses dest.ensure_allocated()).
#define COPY_UPLOAD_CACHE_CAP (1u << 12)        // 4K slots
typedef struct {
  u64 loc;                                       // copy-node heap loc, 0 = empty
  u32 backend_id;                                // target backend
  u32 tid;                                        // uploaded TenDesc
} CopyUploadSlot;
static CopyUploadSlot COPY_UPLOAD_CACHE[COPY_UPLOAD_CACHE_CAP];

fn void copy_upload_cache_reset(void) {
  memset(COPY_UPLOAD_CACHE, 0, sizeof(COPY_UPLOAD_CACHE));
}

static inline u32 copy_upload_hash(u64 loc, u32 backend_id) {
  u64 k = loc * 0x9e3779b97f4a7c15ULL + backend_id;
  k ^= k >> 33;
  return (u32)k & (COPY_UPLOAD_CACHE_CAP - 1);
}

static u32 copy_upload_lookup(u64 loc, u32 backend_id) {
  u32 h = copy_upload_hash(loc, backend_id);
  for (u32 probe = 0; probe < COPY_UPLOAD_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (COPY_UPLOAD_CACHE_CAP - 1);
    if (COPY_UPLOAD_CACHE[i].loc == 0) return 0;
    if (COPY_UPLOAD_CACHE[i].loc == loc
        && COPY_UPLOAD_CACHE[i].backend_id == backend_id)
      return COPY_UPLOAD_CACHE[i].tid;
  }
  return 0;
}

static void copy_upload_insert(u64 loc, u32 backend_id, u32 tid) {
  if (loc == 0 || tid == 0) return;
  u32 h = copy_upload_hash(loc, backend_id);
  for (u32 probe = 0; probe < COPY_UPLOAD_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (COPY_UPLOAD_CACHE_CAP - 1);
    if (COPY_UPLOAD_CACHE[i].loc == 0
        || (COPY_UPLOAD_CACHE[i].loc == loc
            && COPY_UPLOAD_CACHE[i].backend_id == backend_id)) {
      COPY_UPLOAD_CACHE[i].loc        = loc;
      COPY_UPLOAD_CACHE[i].backend_id = backend_id;
      COPY_UPLOAD_CACHE[i].tid        = tid;
      return;
    }
  }
}

// Materialize a UOP_COPY[src] node onto the realize backend
// (CURRENT_BACKEND).  Returns a TAG_TEN resident on that backend.
//
//   - IDENTITY when CURRENT_BACKEND already matches src's backend
//     (e.g. CPU realize of a CPU host leaf): the COPY contributes no
//     kernel and no buffer; the src's materialized TenDesc is reused.
//   - Cross-backend (src on CPU, realize backend X): host-stage the
//     src bytes (src->buf_read into a temp host buffer) and upload to
//     a fresh X buffer (X->buf_write).  The uploaded TenDesc is cached
//     on (copy_loc, X->id) and jit-pinned so it survives pool rollback
//     and is reused on re-realize.
//
// Mirror: tinygrad/uop/ops.py:660 copy_to_device builds
// UOp(Ops.COPY, dtype, (src, DEVICE)); engine/realize.py:158 exec_copy
// allocates the dest on the target device and `dest.copyin(src
// as_memoryview())` -- a host-staged upload, identity when devices match.
static Term materialize_copy(Term term) {
  u64  loc = term_val(term);
  Term src = heap_read(loc);
  // Capture the src sub-graph's device / shape / dtype BEFORE materialize
  // rewrites it: a compute src becomes a UOP_KERNEL whose own node no
  // longer carries the device-bearing leaves.
  i32   src_dev   = term_device_in(src);
  Shape src_shape = {0};
  int   shape_ok  = term_shape_in(src, 0, &src_shape);
  u32   src_dtype = DT_FP32;
  term_dtype_in(src, 0, &src_dtype);
  i32   cpy_dev   = uop_copy_device(loc);

  // Materialize the src.  A host-leaf weight returns a TAG_TEN (its bytes
  // exist now -> eager upload below).  A compute src returns a UOP_KERNEL
  // not yet fired.
  Term src_mat = thvm_materialize(src);
  if (src_mat != src) heap_set(loc, src_mat);

  // Target = the COPY's EXPLICIT device if it carries one, else (generic
  // sentinel) the realize backend.  An explicit device installs its
  // backend on demand; if unavailable (Metal on a non-Metal build) degrade
  // to the realize backend so the COPY stays a sound pass-through.
  Backend *target      = (cpy_dev < 0) ? CURRENT_BACKEND : ctx_ensure_backend(cpy_dev);
  if (target == NULL) target = CURRENT_BACKEND;
  Backend *src_backend = (src_dev >= 0) ? ctx_ensure_backend(src_dev) : NULL;

  // Compute src (a UOP_KERNEL not yet fired): the transfer must run AFTER
  // the src kernel computes.  Cross-device -> defer to a fire-time
  // cross-backend ASSIGN (interact_assign reads src's backend, writes
  // dst's).  wnf drives src -> TEN first, so the memcpy the COPY boundary
  // lowers to runs in dependency order (tinygrad engine/realize exec_copy).
  // Same device (or unknown) -> the COPY is a transparent pass-through:
  // the downstream kernel reads src as any other input.
  if (term_tag(src_mat) != TAG_TEN) {
    if (shape_ok && src_backend != NULL && src_backend != target) {
      u32 d_tid = tensor_alloc(target, src_shape, src_dtype);
      if (d_tid != 0)
        return uop_binary(UOP_ASSIGN,
                          term_new(0, TAG_TEN, TENS[d_tid].dtype, d_tid),
                          src_mat);
    }
    return src_mat;
  }
  u32 src_tid = (u32)term_val(src_mat);
  if (src_tid == 0 || src_tid >= TENS_NEXT) return src_mat;
  Backend *srcb = TENS[src_tid].backend;
  // Identity: target already holds the src.
  if (target == NULL || target == srcb) return src_mat;
  // A COMPUTED-output TEN on a different device (producer_kid != 0) is also
  // computed at fire, not now -- defer like the UOP_KERNEL case above.
  if (TENS[src_tid].producer_kid != 0) {
    u32 d_tid = tensor_alloc(target, TENS[src_tid].view.shape, TENS[src_tid].dtype);
    if (d_tid == 0) return src_mat;
    return uop_binary(UOP_ASSIGN,
                      term_new(0, TAG_TEN, TENS[d_tid].dtype, d_tid),
                      src_mat);
  }
  // Cross-backend cache hit: reuse the prior upload if its buffer is
  // still live (jit-pinned, so pool rollback kept it).
  u32 cached = copy_upload_lookup(loc, target->id);
  if (cached != 0 && cached < TENS_NEXT && TENS[cached].buf_id != 0
      && TENS[cached].backend == target) {
    return term_new(0, TAG_TEN, TENS[cached].dtype, cached);
  }
  // Host-stage + upload.  Allocate the dest on the target backend, read
  // the src bytes into a temp host buffer, write them across.
  TenDesc *sd     = &TENS[src_tid];
  u32      dtype  = sd->dtype;
  u64      numel  = sd->view.numel;
  u64      nbytes = dtype_storage_bytes(dtype, numel);
  void    *stage  = malloc((size_t)nbytes);
  if (stage == NULL) return src_mat;            // OOM: degrade to pass-through
  if (srcb->buf_read(sd->buf_id, stage, nbytes) != 0) {
    free(stage);
    return src_mat;
  }
  u32 dst_tid = tensor_alloc(target, sd->view.shape, dtype);
  target->buf_write(TENS[dst_tid].buf_id, stage, nbytes);
  free(stage);
  // Sticky retain so the per-realize pool rollback does not reclaim the
  // uploaded weight buffer; cache for re-realize / JIT-replay reuse.
  if (target->buf_jit_pin != NULL) target->buf_jit_pin(TENS[dst_tid].buf_id);
  copy_upload_insert(loc, target->id, dst_tid);
  return term_new(0, TAG_TEN, TENS[dst_tid].dtype, dst_tid);
}

// Recursive in-place substitution of cached UOP descendants with
// TAG_TEN leaves.  Walks parent UOPs (NEVER UOP_KERNEL, NEVER
// UOP_ASSIGN -- those are stop points for the materialize entry
// path) and for each child cell, if the child is a UOP at a cached
// loc, heap_set the parent cell to TAG_TEN.  Otherwise recurse.
// Visited bitmap bounds revisits per call.
static void materialize_subst_cached_rec(Term term, u8 *visited, u64 cap) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  if (op == UOP_ASSIGN) {
    // ASSIGN(dst, src): walk only the src; dst is a TAG_TEN reference
    // and shouldn't be substituted.  ASSIGN's own loc isn't subject
    // to the cache (we don't cache ASSIGN emits).
    u64 loc = term_val(term);
    if (loc >= cap || visited[loc]) return;
    visited[loc] = 1;
    Term src = heap_read(loc + 1);
    if (term_tag(src) == TAG_UOP) {
      u32 cached_tid = materialized_loc_lookup(term_val(src));
      if (cached_tid != 0 && subst_journal_record(loc + 1, src)) {
        heap_set(loc + 1, term_new(0, TAG_TEN, TENS[cached_tid].dtype, cached_tid));
      } else {
        materialize_subst_cached_rec(src, visited, cap);
      }
    }
    return;
  }
  u64 loc = term_val(term);
  if (loc >= cap || visited[loc]) return;
  visited[loc] = 1;
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u32 cached_tid = materialized_loc_lookup(term_val(child));
    if (cached_tid != 0 && subst_journal_record(loc + i, child)) {
      heap_set(loc + i, term_new(0, TAG_TEN, TENS[cached_tid].dtype, cached_tid));
    } else {
      materialize_subst_cached_rec(child, visited, cap);
    }
  }
}

static void materialize_subst_cached_inplace(Term root) {
  if (term_tag(root) != TAG_UOP) return;
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;
  materialize_subst_cached_rec(root, visited, cap);
  free(visited);
}

#define BOUNDARY_DEPTH_INVALID 0xFFFFFFFFu
static u32 BOUNDARY_DEPTH    [BUFFERIZE_NODES_CAP];
// Per-boundary maximum-consumer depth.  Filled after topo by
// boundary_compute_last_use; consumed by the depth-aware mem planner
// to recycle output bufs once their LAST consumer has emitted.  0 =
// "no consumer is itself a realize boundary"; that includes the
// realize root (the caller reads it; never recycle) and any orphan
// preserved tensor.
static u32 BOUNDARY_LAST_USE [BUFFERIZE_NODES_CAP];
// Per-boundary fire-order sequence (post-order DFS position from the
// realize sink).  kernel_fire_by_id (interact/uop_kernel.c:207) fires a
// kernel's input producers first, in slot order, then dispatches the
// kernel itself -- a post-order DFS over producer->consumer edges.  The
// arena lifetime planner recycles a slot once its boundary-order
// position passes, so that position MUST equal the actual dispatch
// order; the (depth, loc) topo sort can diverge from it (a low-depth
// node whose only consumer sits in a late DFS branch fires LATE, after
// a higher-depth sibling that took its recycled slot -> clobber).
// Sorting boundaries by this sequence makes BOUNDARY_ORDER the linear
// execution order, matching tinygrad's lifetimes-over-linearized-
// schedule invariant (schedule/memory.py:28 enumerates linear.src).
// 0xFFFFFFFF = unvisited (orphan preserved tensor; falls back to depth).
static u32 BOUNDARY_FIRE_SEQ [BUFFERIZE_NODES_CAP];
// Per-boundary maximum-consumer BOUNDARY_ORDER position (fire order).
// boundary_compute_last_use stores a consumer DEPTH; the arena planner
// then maps that depth back to "the last position at that depth" via
// arena_last_pos_at_depth -- a lossy mapping that UNDERCUTS the real
// lifetime when a buf's deepest consumer is NOT its latest-firing one
// (a consumer at a shallower depth can fire LATER in the post-order DFS).
// BOUNDARY_LAST_USE_POS records the true latest consumer's fire-order
// position directly (the max BOUNDARY_ORDER index over all realized
// parents), so the arena recycles a slot only after its genuinely-last
// reader fires.  Tinygrad mirror: schedule/memory.py last_appearance is
// the linearized-schedule index of the buf's last consumer.  Computed by
// boundary_compute_last_use_pos AFTER BOUNDARY_ORDER + the loc->pos hash
// are built.  0 = no realized-boundary consumer (sink / orphan).
static u32 BOUNDARY_LAST_USE_POS [BUFFERIZE_NODES_CAP];

#define VISIT_BAIL 0xDEADBEEFu
// VISIT_OK signals "subgraph visited successfully".  Visit walks
// the UOp graph to populate the kernel's input bindings; the body
// lives on the lifted UOp DAG (cached_lift.store_root), not on a
// per-op array, so visit() never emits per-op slots.  Since
// KSRC_AS_INPUT(slot) sets the 0x80000000 input bit, VISIT_OK (0)
// is distinguishable from any input-slot ref via KSRC_IS_INPUT().
#define VISIT_OK   0u

typedef struct {
  u64 *locs;
  u32 *refs;
  u32  len;
  u32  cap;
} VisitMemo;

static void visit_memo_free(VisitMemo *m) {
  if (m == NULL) return;
  free(m->locs);
  free(m->refs);
  m->locs = NULL;
  m->refs = NULL;
  m->len  = 0;
  m->cap  = 0;
}

static u32 visit_memo_lookup(VisitMemo *m, u64 loc) {
  if (m == NULL) return VISIT_BAIL;
  for (u32 i = 0; i < m->len; i++) {
    if (m->locs[i] == loc) return m->refs[i];
  }
  return VISIT_BAIL;
}

static void visit_memo_store(VisitMemo *m, u64 loc, u32 ref) {
  if (m == NULL || ref == VISIT_BAIL) return;
  for (u32 i = 0; i < m->len; i++) {
    if (m->locs[i] == loc) {
      m->refs[i] = ref;
      return;
    }
  }
  if (m->len >= m->cap) {
    u32 new_cap = m->cap == 0 ? 64 : m->cap * 2;
    m->locs = (u64 *)realloc(m->locs, (size_t)new_cap * sizeof(u64));
    m->refs = (u32 *)realloc(m->refs, (size_t)new_cap * sizeof(u32));
    m->cap  = new_cap;
  }
  m->locs[m->len] = loc;
  m->refs[m->len] = ref;
  m->len++;
}

// === per-realize memory planner =====================================
//
// After topo_sort + last_use computation, the emit loop walks
// boundaries in alloc-depth order; before each kernel allocates its
// output buf, the planner pushes any earlier-emitted kernel's
// output buf whose last_use_depth has already passed onto the
// backend's free-list.  The next tensor_alloc -> backend->buf_alloc
// then pops a same-nbytes match instead of growing the buffer
// table -- tinygrad's MemoryPlanner, scoped to one materialize
// pass.
//
// Metal safety: the push + pop only ever happen between kernels
// emitted in the *same* materialize pass, and no kernel is dispatched
// during the emit loop (wnf fires kernels in a later realize-loop
// iteration).  So a buffer handed to the free-list and recycled mid-
// emit has never been touched by a Metal command buffer; the contents-
// memset in metal_buf_freelist_try_pop can't race in-flight GPU work.
// end-of-pass mem_plan_drain_freelist pulls any survivors back off the
// list so the next pass's allocations can't recycle a buf whose
// TenDesc is still referenced by the chain rule's fresh UOPs.

#define MEM_PLAN_CAP BOUNDARY_ORDER_CAP
typedef struct {
  u32      buf_id;
  u32      last_use_depth;
  Backend *backend;
  u8       pushed;
} MemPlanEntry;

static MemPlanEntry MEM_PLAN[MEM_PLAN_CAP];
static u32          MEM_PLAN_LEN = 0;

// CPU planner: default-on; THVM_REUSE_BUFS=0 opts out.  The earlier
// concern (DUP/SUP-aware lifetime tracking) hasn't materialized as a
// correctness regression on the unified-rangeify path; verified on
// beautiful_mnist BS=2/16 STEPS=4 (peak -7%, loss curves byte-equivalent
// up to BLAS-summation fp-order noise) + full C suite + numpy conv+BN
// grad ref.  Brings CPU's recycling behavior in line with Metal/CUDA,
// where the planner has been default-on for a while.
static int mem_plan_cpu_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_REUSE_BUFS");
    enabled       = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
    known         = 1;
  }
  return enabled;
}

// Metal planner: default-ON.  The earlier rationale for default-off
// ("inert on beautiful_mnist -- peak is im2col intermediates") was
// invalidated by the conv strided-view _pool rework (391a0d08 zero-
// materialization composed INDEX): conv no longer materializes
// im2col, kernel-output buffers ARE the dominant retained cost at
// BS>=32, and the within-pass single-consumer recycling is exactly
// what shrinks peak retained from ~7x the working set toward
// tinygrad's ~flat profile.  THVM_METAL_REUSE_BUFS=0 opts out.
static int mem_plan_metal_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_METAL_REUSE_BUFS");
    enabled       = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
    known         = 1;
  }
  return enabled;
}

// CUDA per-realize buffer reuse: default ON (mirrors Metal).  Without it
// every kernel output in a realize stays live to end-of-pass -- the full
// beautiful_mnist forward+backward keeps ~all activations resident (GBs)
// vs tinygrad's flat working set, because each buffer is freed only by the
// post-realize rollback, never recycled mid-pass.  The synchronous CUDA
// dispatch (cuCtxSynchronize after each launch) means a buffer freed after
// its last consumer can't race in-flight work, so recycling is safe.
static int mem_plan_cuda_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_CUDA_REUSE_BUFS");
    enabled       = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
    known         = 1;
  }
  return enabled;
}

// Per-backend gate the recorded buf actually obeys.
static int mem_plan_backend_enabled(Backend const *b) {
  if (b == NULL) return 0;
  if (b->id == 1) return mem_plan_cpu_enabled();
  if (b->id == 2) return mem_plan_metal_enabled();
  if (b->id == 3) return mem_plan_cuda_enabled();
  return 0;
}

static void mem_plan_reset(void) { MEM_PLAN_LEN = 0; }

// Pull every entry the planner left on a backend free-list that hasn't
// been re-issued by an in-pass buf_alloc back to its prior "live"
// state.  thvm_realize loops materialize+wnf to fixed-point; if a
// planner push from pass N survived into pass N+1's free-list, pass
// N+1's allocations could reuse a buf whose original TenDesc is still
// referenced by the chain rule's freshly-emitted UOPs, corrupting the
// read.  Drain at end-of-pass so the planner's free-list scope stays
// strictly per-pass; within-pass reuse (alloc-then-pop within the same
// emit loop) still works.
static void mem_plan_drain_freelist(void) {
  for (u32 i = 0; i < MEM_PLAN_LEN; i++) {
    MemPlanEntry *e = &MEM_PLAN[i];
    if (!e->pushed)                       continue;
    Backend *b = e->backend;
    if (b == NULL || b->buf_freelist_remove == NULL) continue;
    if (!mem_plan_backend_enabled(b))     continue;
    b->buf_freelist_remove(e->buf_id);
  }
}

static void mem_plan_record(u32 buf_id, u32 last_use_depth, Backend *b) {
  if (b == NULL || buf_id == 0)              return;
  if (b->buf_freelist_push == NULL)          return;   // backend opts out
  if (!mem_plan_backend_enabled(b))          return;
  if (MEM_PLAN_LEN >= MEM_PLAN_CAP)          return;
  MemPlanEntry *e = &MEM_PLAN[MEM_PLAN_LEN++];
  e->buf_id         = buf_id;
  e->last_use_depth = last_use_depth;
  e->backend        = b;
  e->pushed         = 0;
}

static void mem_plan_push_dead(u32 current_depth) {
  for (u32 i = 0; i < MEM_PLAN_LEN; i++) {
    MemPlanEntry *e = &MEM_PLAN[i];
    if (e->pushed)                              continue;
    // last_use_depth == 0 is the "no realize-boundary consumer" sentinel
    // (see boundary_compute_last_use's comment around line 3478): the
    // sink / realize root / preserved orphan -- the caller will read this
    // buf AFTER realize returns.  Don't push it to the freelist or the
    // next allocate will hand its dptr to a fresh kernel intermediate,
    // overwriting the caller's content.  CUDA bit: cuda_buf_alloc's
    // best-fit freelist would hand a 4-byte popper the loss-scalar's
    // dptr; the caller's later loss.numpy() then reads the popper's
    // value (the symptom: BS=128 CUDA loss goes 2.76 -> 3.00 across an
    // opt-step realize call).  CPU was unaffected because the TLSF arena
    // planner skips sinks via its arena-plannability gate, but the
    // legacy freelist push path here applied to ALL backends.
    if (e->last_use_depth == 0)                 continue;
    if (e->last_use_depth >= current_depth)     continue;
    if (e->buf_id == 0)                         continue;
    Backend *b = e->backend;
    if (b == NULL || b->buf_freelist_push == NULL || !mem_plan_backend_enabled(b)) {
      e->pushed = 1;
      continue;
    }
    // Refcount > 1 means another TenDesc aliases this buf (typically a
    // view-only RESHAPE / EXPAND chain).  Recycling would yank the
    // bytes from the alias too, so skip -- the post-realize preserve
    // walk + rollback releases these via the refcount-driven path.
    if (b->buf_refcount != NULL && b->buf_refcount(e->buf_id) > 1) {
      e->pushed = 1;
      continue;
    }
    // External / WL-shared bufs (NumericArray imports) own no backing
    // storage; cpu_buf_freelist_push self-guards (owns_data check), so
    // an unconditional push here is safe for both backends.
    b->buf_freelist_push(e->buf_id);
    e->pushed = 1;
  }
}

// === per-realize ARENA memory planner =================================
//
// Direct port of tinygrad/schedule/memory.py memory_plan_rewrite
// (lines 20-65).  Stronger than the legacy freelist above: instead of
// recycling individual buf_ids after their last consumer fires, the
// arena planner pre-computes a non-overlapping byte offset for every
// plannable output by feeding their open/close events to a TLSF
// suballocator -- the same algorithm tinygrad runs on its lowered
// schedule.  Plannable outputs become VIEWS into a single arena
// CpuBuf; non-overlapping lifetimes share bytes -> peak ~= max
// concurrent live, not sum-of-all.
//
// Plannability gate: same as the legacy planner (BOUNDARY_LAST_USE > 0
// AND consumer_count == 1).  Multi-consumer / orphan / root bufs take
// the legacy cpu_buf_alloc path.  Arena allocator runs per-realize;
// the arena CpuBuf is freed by pool_rollback at end-of-realize.
//
// CPU + CUDA today (Metal stays on the freelist push/pop scope so its
// MTLCommandBuffer dispatch-time synchronisation invariants aren't
// broken).  CUDA mirrors CPU: ARENA_BUF_ID indexes the backend's
// per-realize arena buf; arena_data returns a host pointer for CPU and
// arena_dptr returns a CUdeviceptr for CUDA; arena_tensor_alloc emits
// the per-view backend buf and zero-inits the slot (memset for CPU,
// cuMemsetD8 for CUDA).

typedef struct {
  u64 offset;
  u64 nbytes;
  u8  in_arena;        // 1 if this boundary's output lives in the arena
} ArenaSlot;

#define ARENA_SLOTS_CAP BOUNDARY_ORDER_CAP
static ArenaSlot ARENA_SLOTS[ARENA_SLOTS_CAP];
static u32       ARENA_SLOTS_LEN     = 0;
static u64       ARENA_SIZE          = 0;
static u32       ARENA_BUF_ID        = 0;
static u8       *ARENA_DATA          = NULL;   // CPU arena base
#ifdef THVM_HAS_CUDA
static CUdeviceptr ARENA_DPTR        = 0;      // CUDA arena base
#endif

// Telemetry: counts arena vs legacy allocs per realize for THVM_ARENA_DUMP.
static u32 ARENA_ALLOCS_ARENA  = 0;
static u32 ARENA_ALLOCS_LEGACY = 0;
static u64 ARENA_PEAK_BYTES    = 0;

fn u64 mem_plan_arena_peak_bytes(void)  { return ARENA_PEAK_BYTES;    }
fn u32 mem_plan_arena_alloc_count(void) { return ARENA_ALLOCS_ARENA;  }
fn u32 mem_plan_legacy_alloc_count(void){ return ARENA_ALLOCS_LEGACY; }

static int arena_plan_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_ARENA_PLAN");
    enabled = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
    known = 1;
  }
  return enabled;
}

static void arena_reset(void) {
  for (u32 i = 0; i < ARENA_SLOTS_LEN; i++) {
    ARENA_SLOTS[i].offset    = 0;
    ARENA_SLOTS[i].nbytes    = 0;
    ARENA_SLOTS[i].in_arena  = 0;
  }
  ARENA_SLOTS_LEN = 0;
  ARENA_SIZE      = 0;
  ARENA_BUF_ID    = 0;
  ARENA_DATA      = NULL;
#ifdef THVM_HAS_CUDA
  ARENA_DPTR      = 0;
#endif
}

typedef struct {
  u32 pos;
  u32 ord_idx;
  u8  is_open;
} ArenaEvent;

static void arena_event_swap(ArenaEvent *a, ArenaEvent *b) {
  ArenaEvent t = *a; *a = *b; *b = t;
}

// Sort by (pos asc, is_open desc): at the same position, opens come
// before closes so a freshly-allocated buf doesn't release its
// offset back to TLSF before the alloc that uses it.  Tinygrad
// memory.py:43-44 achieves the same via True > False sort on the
// is_open flag.
static void arena_sort_events(ArenaEvent *ev, u32 n) {
  // Simple insertion sort: n is small (<= 2 * 16k worst-case, typical
  // a few hundred), and the per-realize cost is < 1% of emit time.
  for (u32 i = 1; i < n; i++) {
    for (u32 j = i; j > 0; j--) {
      ArenaEvent *a = &ev[j-1];
      ArenaEvent *b = &ev[j];
      int swap = (a->pos > b->pos)
              || (a->pos == b->pos && a->is_open < b->is_open);
      if (!swap) break;
      arena_event_swap(a, b);
    }
  }
}

static int arena_boundary_is_plannable(u32 ord_idx) {
  // During a JIT-capture dedup span, a kernel output materialized in one
  // realize may be REUSED by a later realize of the same step (the whole
  // point of the span -- forward activations read back by the grad/optim
  // realizes).  The per-realize arena plans lifetimes WITHIN one realize,
  // so it would recycle such a buffer's slot mid-step and corrupt the
  // cross-realize read (the JIT pin keeps the buf_id alive but the arena
  // already reused its bytes).  Force every boundary to a legacy
  // (non-arena, non-recycled) alloc for the span; with the jit-capture pin
  // (survives pool-rollback) the shared outputs stay live AND valid across
  // all of the step's realizes.  Trades within-step arena recycling for
  // correctness; the extra peak is bounded and the GPU has the headroom.
  if (materialized_loc_span_holds()) return 0;
  u64 loc = BOUNDARY_ORDER[ord_idx];
  u32 binfo = bufferize_info_find(loc);
  if (binfo == 0xFFFFFFFFu) return 0;
  // last_use == 0 means "no realize-boundary consumer" -- this is the
  // realize root or an orphan preserved tensor.  Tinygrad's
  // equivalent: held_bufs excludes result bufs from
  // memory_plan_rewrite (schedule/memory.py:22).
  if (BOUNDARY_LAST_USE[binfo] == 0) return 0;
  // Single-consumer gate (matches mem_plan_record at line 596):
  // multi-consumer outputs may be aliased through DUP/SUP / read by
  // a NEXT realize pass's chain rule.  thvm_realize runs wnf +
  // materialize in a loop; a buf produced in pass N can be read in
  // pass N+1.  BOUNDARY_LAST_USE is per-pass, so it can't see the
  // N+1 reader.  Restricting to consumer_count==1 keeps arena-recycle
  // safe for the current iterative-realize model.  Tinygrad's
  // single-pass schedule doesn't have this constraint, but porting
  // that would require collapsing thvm_realize's loop.
  if (BUFFERIZE_NODES[binfo].consumer_count != 1) return 0;
  return 1;
}

static u64 arena_boundary_nbytes(u32 ord_idx) {
  u64 loc = BOUNDARY_ORDER[ord_idx];
  u32 binfo = bufferize_info_find(loc);
  if (binfo == 0xFFFFFFFFu) return 0;
  u8 op = BUFFERIZE_NODES[binfo].op;
  Term root_term = term_new(0, TAG_UOP, op, loc);
  Shape sh = {0};
  if (!term_shape_in(root_term, 0, &sh)) return 0;
  u32 dtype = DT_FP32;
  term_dtype_in(root_term, 0, &dtype);
  u64 numel = (u64)shape_numel(sh);
  return dtype_storage_bytes(dtype, numel);
}

// Build per-boundary lifetimes + run TLSF to assign offsets.  Mirror
// of tinygrad/schedule/memory.py:25-53.  Sets up ARENA_SLOTS[] and
// ARENA_SIZE.  No arena CpuBuf allocation here -- the first
// arena-slot read lazily allocates the bytes (so a no-plannable pass
// doesn't allocate at all).
static void arena_compute(void) {
  arena_reset();
  ARENA_ALLOCS_ARENA  = 0;
  ARENA_ALLOCS_LEGACY = 0;
  if (!arena_plan_enabled())     return;
  if (BOUNDARY_ORDER_LEN == 0)   return;
  if (CURRENT_BACKEND == NULL)   return;
  int is_cpu  = (CURRENT_BACKEND == &CPU_BACKEND);
  int is_cuda = 0;
#ifdef THVM_HAS_CUDA
  is_cuda = (CURRENT_BACKEND == &CUDA_BACKEND);
#endif
  if (!is_cpu && !is_cuda)       return;

  ARENA_SLOTS_LEN = BOUNDARY_ORDER_LEN;
  static u32 first_pos[ARENA_SLOTS_CAP];
  static u32 last_pos [ARENA_SLOTS_CAP];
  static u64 nbytes   [ARENA_SLOTS_CAP];
  static u8  planned  [ARENA_SLOTS_CAP];
  u32 n_planned = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    first_pos[i] = i; last_pos[i] = i; nbytes[i] = 0; planned[i] = 0;
    if (!arena_boundary_is_plannable(i)) continue;
    u64 nb = arena_boundary_nbytes(i);
    if (nb == 0) continue;
    u64 loc = BOUNDARY_ORDER[i];
    u32 binfo = bufferize_info_find(loc);
    // True last-consumer fire position (BOUNDARY_LAST_USE_POS), not the
    // depth->position remap arena_last_pos_at_depth used to do -- that
    // remap undercut the lifetime when a buf's deepest consumer wasn't
    // its latest-firing one, recycling the slot before the real last
    // read fired (the 2-layer-transformer NaN).
    u32 last_at = BOUNDARY_LAST_USE_POS[binfo];
    if (last_at < i) last_at = i;
    // Cross-PASS extension: a boundary reached by >= 2 top-level roots is
    // shared across sub-passes (a forward activation read by two grad
    // targets).  The FIRST sub-pass to materialize it caches the tid; a
    // SIBLING sub-pass reads that cached tid via materialized_loc_lookup.
    // If this pass recycled its arena offset (handed it to a later
    // boundary) the sibling would read the recycled-over bytes -> NaN.
    // bufferize_classify's local consumer_count can't see the sibling
    // reader; xpass_is_shared (whole-realize root walk) can.  Keep the buf
    // in the arena (it still shares bytes with non-overlapping lifetimes)
    // but extend its lifetime to END OF PASS so its offset is never reused
    // within the pass -- the cached bytes stay valid for every sibling.
    // Non-shared buffers recycle normally (no peak cost).  Tinygrad
    // mirror: schedule/memory.py last_appearance spans the whole
    // linearized schedule, so a buf live across sub-passes is never freed
    // early.
    if (xpass_is_shared(loc) && BOUNDARY_ORDER_LEN > 0)
      last_at = BOUNDARY_ORDER_LEN - 1;
    nbytes[i]   = nb;
    last_pos[i] = last_at;
    planned[i]  = 1;
    n_planned++;
  }
  if (getenv("THVM_ARENA_DUMP_BUFS")) {
    for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
      u64 nb = arena_boundary_nbytes(i);
      if (nb < 1048576) continue;
      u64 loc = BOUNDARY_ORDER[i];
      u32 binfo = bufferize_info_find(loc);
      u32 op = (binfo != 0xFFFFFFFFu) ? BUFFERIZE_NODES[binfo].op : 0xFFu;
      u64 kind = 0, naxes = 0, a0 = 0;
      if (op == UOP_REDUCE) {
        kind  = term_val(heap_read(loc + 1));
        naxes = term_val(heap_read(loc + 2));
        if (naxes >= 1) a0 = term_val(heap_read(loc + 3));
      }
      u64 cons[4];
      u32 nc = bufferize_consumers_for_loc(loc, cons, 4);
      u32 cop0 = 0xFFu, cop1 = 0xFFu;
      if (nc >= 1) {
        u32 ci = bufferize_info_find(cons[0]);
        if (ci != 0xFFFFFFFFu) cop0 = BUFFERIZE_NODES[ci].op;
      }
      if (nc >= 2) {
        u32 ci = bufferize_info_find(cons[1]);
        if (ci != 0xFFFFFFFFu) cop1 = BUFFERIZE_NODES[ci].op;
      }
      fprintf(stderr,
              "  buf[%u] op=%u bytes=%.2fMB plannable=%d kind=%llu naxes=%llu a0=%llu n_cons=%u cop0=%u cop1=%u life=[%u,%u]\n",
              i, op, (double)nb / 1048576.0,
              arena_boundary_is_plannable(i),
              (unsigned long long)kind, (unsigned long long)naxes,
              (unsigned long long)a0, nc, cop0, cop1,
              first_pos[i], last_pos[i]);
    }
  }
  if (n_planned == 0) return;

  static ArenaEvent events[ARENA_SLOTS_CAP * 2];
  u32 n_ev = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    if (!planned[i]) continue;
    events[n_ev].pos = first_pos[i]; events[n_ev].ord_idx = i; events[n_ev].is_open = 1; n_ev++;
    events[n_ev].pos = last_pos [i] + 1u; events[n_ev].ord_idx = i; events[n_ev].is_open = 0; n_ev++;
  }
  arena_sort_events(events, n_ev);

  u64 total = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) total += nbytes[i];
  if (total == 0) return;
  u64 tlsf_total = total * 2u;
  tlsf_total = tlsf_round_up(tlsf_total, TLSF_BLOCK_SIZE);

  static TlsfAllocator tlsf;
  tlsf_init(&tlsf, tlsf_total);

  u64 peak = 0;
  for (u32 e = 0; e < n_ev; e++) {
    u32 i = events[e].ord_idx;
    if (events[e].is_open) {
      u64 want = tlsf_round_up(nbytes[i], TLSF_BLOCK_SIZE);
      u64 off = tlsf_alloc(&tlsf, want);
      if (off == (u64)-1) {
        // OOM in a 2x-sized arena shouldn't happen; if it does, bail
        // and let every emit fall back to the legacy alloc path.
        arena_reset();
        tlsf_dispose(&tlsf);
        return;
      }
      ARENA_SLOTS[i].offset    = off;
      ARENA_SLOTS[i].nbytes    = nbytes[i];
      ARENA_SLOTS[i].in_arena  = 1;
      u64 end = off + nbytes[i];
      if (end > peak) peak = end;
    } else {
      tlsf_free(&tlsf, ARENA_SLOTS[i].offset);
    }
  }
  tlsf_dispose(&tlsf);
  ARENA_SIZE = tlsf_round_up(peak, TLSF_BLOCK_SIZE);
  ARENA_PEAK_BYTES = ARENA_SIZE;
  if (getenv("THVM_ARENA_DUMP")) {
    fprintf(stderr,
            "arena: %u/%u plannable, sum=%.2fMB arena=%.2fMB peak_off=%.2fMB\n",
            n_planned, BOUNDARY_ORDER_LEN,
            (double)total / 1048576.0,
            (double)ARENA_SIZE / 1048576.0,
            (double)peak / 1048576.0);
  }
}

// Lazily allocate the arena backend buf on the first arena-slot read.
// Single block participates in CPU_MEM_LIVE / CUDA_MEM_LIVE +
// pool_rollback like any other buf.  Returns 1 if an arena exists (or
// was just created), 0 otherwise.
static int arena_ensure(void) {
  if (ARENA_BUF_ID != 0)        return 1;
  if (ARENA_SIZE == 0)          return 0;
  if (CURRENT_BACKEND == &CPU_BACKEND) {
    ARENA_BUF_ID = cpu_buf_alloc(ARENA_SIZE);
    if (ARENA_BUF_ID == 0)      return 0;
    ARENA_DATA = (u8 *)CPU_BUFS[ARENA_BUF_ID].data;
    // Per-realize arena: this block is sized to fit this pass's
    // max-live working set (computed by arena_compute via TLSF
    // events).  At end of realize the entire arena is released; we
    // want pool_rollback to real-free it, NOT park it on the
    // freelist where best-fit might snag a 646MB slot for a 16MB
    // request (or, worse, leave it parked while every subsequent
    // realize calloc's a fresh arena -- the cross-step leak this
    // flag fixes).
    CPU_BUFS[ARENA_BUF_ID].skip_freelist = 1;
    return 1;
  }
#ifdef THVM_HAS_CUDA
  if (CURRENT_BACKEND == &CUDA_BACKEND) {
    ARENA_BUF_ID = cuda_buf_alloc(ARENA_SIZE);
    if (ARENA_BUF_ID == 0)      return 0;
    ARENA_DPTR = cuda_buf_dptr(ARENA_BUF_ID);
    CUDA_BUFS[ARENA_BUF_ID].skip_freelist = 1;
    return 1;
  }
#endif
  return 0;
}

// Allocate a TenDesc whose buf is an external view into the arena.
// Returns 0 on miss (caller falls back to tensor_alloc).
static u32 arena_tensor_alloc(u32 ord_idx, Shape shape, u32 dtype) {
  if (ord_idx >= ARENA_SLOTS_LEN)        return 0;
  if (!ARENA_SLOTS[ord_idx].in_arena)    return 0;
  int is_cpu  = (CURRENT_BACKEND == &CPU_BACKEND);
  int is_cuda = 0;
#ifdef THVM_HAS_CUDA
  is_cuda = (CURRENT_BACKEND == &CUDA_BACKEND);
#endif
  if (!is_cpu && !is_cuda)               return 0;
  if (!arena_ensure())                   return 0;
  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "arena_tensor_alloc: out of descriptor slots\n");
    exit(1);
  }
  u32 tid = (u32)TENS_NEXT++;
  TenDesc *d = &TENS[tid];
  d->dtype        = dtype;
  d->refcount     = 1;
  d->view         = view_create(shape);
  d->prior_views  = NULL;
  d->nviews       = 0;
  d->requires_grad = 0;
  d->grad         = 0;
  d->assign_kvar_id = 0;
  d->backend      = CURRENT_BACKEND;
  d->producer_kid = 0;
  u64 off    = ARENA_SLOTS[ord_idx].offset;
  u64 nbytes = dtype_storage_bytes(dtype, (u64)d->view.numel);
  if (nbytes == 0) nbytes = 1;
  // Zero-init the slot: a previous lifetime's bytes still occupy it
  // (TLSF only tracks block ownership, not zeroing).  buf_alloc's
  // calloc/cuMemsetD8/freelist-pop guaranteed zero, so kernels writing
  // through REDUCE_ADD / accumulate-style paths depend on it.
  //
  // Arena view: ties this backend buf's lifetime to ARENA_BUF_ID via
  // the parent_buf_id chain in buf_incref / buf_decref so the arena
  // bytes outlive every view.  Mirror: tinygrad schedule/memory.py:60
  // BUFFER_VIEW(arena, nbytes, offset).
  if (is_cpu) {
    memset(ARENA_DATA + off, 0, (size_t)nbytes);
    d->buf_id = cpu_buf_alloc_arena_view(ARENA_DATA + off, nbytes,
                                         ARENA_BUF_ID);
  }
#ifdef THVM_HAS_CUDA
  else if (is_cuda) {
    cuMemsetD8(ARENA_DPTR + off, 0, (size_t)nbytes);
    d->buf_id = cuda_buf_alloc_arena_view(ARENA_DPTR + off, nbytes,
                                          ARENA_BUF_ID);
  }
#endif
  ARENA_ALLOCS_ARENA++;
  return tid;
}


// === topo-sort over realize boundaries (g2a) ===

static u32 boundary_depth_rec(u64 loc) {
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  if (BOUNDARY_DEPTH[idx] != BOUNDARY_DEPTH_INVALID) return BOUNDARY_DEPTH[idx];
  BOUNDARY_DEPTH[idx] = 0;            // cycle guard

  u32 max_up = 0;
  u8  ar     = uop_arity(BUFFERIZE_NODES[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 i = 0; i < ar; i++) {
    // term_resolve to follow VAR/ALO chains -- match bufferize_walk_rec
    // and visit() so the topo-sort sees the same boundary set.
    Term child = term_resolve(heap_read(loc + i));
    if (term_tag(child) != TAG_UOP)        continue;
    if (term_ext(child) == UOP_KERNEL)     continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    u32 cd = boundary_depth_rec(cloc);
    if (cd > max_up) max_up = cd;
  }
  u32 d = BUFFERIZE_NODES[idx].realized ? max_up + 1 : max_up;
  BOUNDARY_DEPTH[idx] = d;
  return d;
}

// Walk DOWN from `from_loc` through non-realized intermediates.  For
// each realized boundary B encountered along the way, set
// BOUNDARY_LAST_USE[B] = max(BOUNDARY_LAST_USE[B], visiting_depth).
// `visited` is a bitmap sized to HEAP_NEXT to dedup the recursion.
//
// The walk has to descend through non-realized UOps because the
// emit loop INLINES them into the parent's lifted UOp DAG (visit()
// in emit_kernel_for_boundary recurses through them); the boundary
// that the kernel eventually reads is the realized kid, so its
// true last_use_depth is the realized PARENT'S depth, not the
// non-realized intermediate's "depth" (which equals the
// child's, see boundary_depth_rec where non-realized just inherits).
static void boundary_last_use_descend(u64 from_loc, u32 visiting_depth,
                                      u8 *visited) {
  // Heap loc 0 is a valid allocation; only HEAP_NEXT bounds gates
  // the read.  (Earlier code treated 0 as a sentinel which silently
  // dropped last-use updates for the first-allocated boundary in
  // any heap-clean tests.)
  if (from_loc >= HEAP_NEXT) return;
  if (visited[from_loc]) return;
  visited[from_loc] = 1;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return;
  if (BUFFERIZE_NODES[idx].realized) {
    if (visiting_depth > BOUNDARY_LAST_USE[idx]) {
      BOUNDARY_LAST_USE[idx] = visiting_depth;
    }
    return;     // stop at the boundary -- its OWN children get
                // handled when boundary_compute_last_use walks
                // them as realized parents.
  }
  // Non-realized intermediate: recurse through its UOp children.
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    if (term_tag(child) != TAG_UOP)         continue;
    if (term_ext(child) == UOP_KERNEL)      continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    boundary_last_use_descend(cloc, visiting_depth, visited);
  }
}

// For each realized parent at depth D, walk its UOp subtree (through
// any non-realized intermediates) and bump BOUNDARY_LAST_USE on every
// realized child it reaches to D.  After this, the planner can
// safely freelist-push a buf at depth = last_use + 1 because every
// realized parent that consumes it has already emitted by then.
static void boundary_compute_last_use(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++) BOUNDARY_LAST_USE[i] = 0;
  if (HEAP_NEXT == 0) return;
  u8 *visited = (u8 *)calloc(HEAP_NEXT, 1);
  if (visited == NULL) return;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *p = &BUFFERIZE_NODES[i];
    if (!p->realized)                              continue;
    u32 p_depth = BOUNDARY_DEPTH[i];
    if (p_depth == BOUNDARY_DEPTH_INVALID)         continue;
    u8 ar = uop_arity(p->op);
    u64 seen[MAX_UOP_SRC] = {0};
    u8  n_seen = 0;
    // Reset the visited bitmap per-parent so the walk doesn't
    // collapse across parents (each parent independently roots
    // its own consumer-depth update).
    memset(visited, 0, HEAP_NEXT);
    for (u8 c = 0; c < ar; c++) {
      Term child = term_resolve(heap_read(p->loc + c));
      if (term_tag(child) != TAG_UOP)         continue;
      if (term_ext(child) == UOP_KERNEL)      continue;
      u64 cloc = term_val(child);
      u8  dup  = 0;
      for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
      if (dup) continue;
      seen[n_seen++] = cloc;
      boundary_last_use_descend(cloc, p_depth, visited);
    }
  }
  free(visited);
}

// Position variant of boundary_last_use_descend: walk DOWN from
// `from_loc` through non-realized intermediates, and for each realized
// boundary B reached, bump BOUNDARY_LAST_USE_POS[B] to `visiting_pos`
// (the consuming parent's BOUNDARY_ORDER index).  Same descent shape;
// only the recorded quantity differs (fire-order position, not depth).
static void boundary_last_use_pos_descend(u64 from_loc, u32 visiting_pos,
                                          u8 *visited) {
  if (from_loc >= HEAP_NEXT) return;
  if (visited[from_loc]) return;
  visited[from_loc] = 1;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return;
  if (BUFFERIZE_NODES[idx].realized) {
    if (visiting_pos > BOUNDARY_LAST_USE_POS[idx]) {
      BOUNDARY_LAST_USE_POS[idx] = visiting_pos;
    }
    return;
  }
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    if (term_tag(child) != TAG_UOP)         continue;
    if (term_ext(child) == UOP_KERNEL)      continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    boundary_last_use_pos_descend(cloc, visiting_pos, visited);
  }
}

// Compute BOUNDARY_LAST_USE_POS[]: for each realized parent (at
// BOUNDARY_ORDER position P), descend its UOp subtree and bump every
// realized child it reads to P.  After this, each boundary's value is
// the fire-order position of its LATEST-firing consumer -- the lifetime
// the arena planner closes on.  MUST run AFTER topo_sort_boundaries has
// built BOUNDARY_ORDER + the loc->pos hash.  Mirrors
// boundary_compute_last_use but in fire-order, not depth.
static void boundary_compute_last_use_pos(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++) BOUNDARY_LAST_USE_POS[i] = 0;
  if (HEAP_NEXT == 0) return;
  u8 *visited = (u8 *)calloc(HEAP_NEXT, 1);
  if (visited == NULL) return;
  for (u32 pos = 0; pos < BOUNDARY_ORDER_LEN; pos++) {
    u64 ploc = BOUNDARY_ORDER[pos];
    u32 i = bufferize_info_find(ploc);
    if (i == 0xFFFFFFFFu)                          continue;
    UOpInfo *p = &BUFFERIZE_NODES[i];
    if (!p->realized)                              continue;
    u8 ar = uop_arity(p->op);
    u64 seen[MAX_UOP_SRC] = {0};
    u8  n_seen = 0;
    memset(visited, 0, HEAP_NEXT);
    for (u8 c = 0; c < ar; c++) {
      Term child = term_resolve(heap_read(p->loc + c));
      if (term_tag(child) != TAG_UOP)         continue;
      if (term_ext(child) == UOP_KERNEL)      continue;
      u64 cloc = term_val(child);
      u8  dup  = 0;
      for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
      if (dup) continue;
      seen[n_seen++] = cloc;
      boundary_last_use_pos_descend(cloc, pos, visited);
    }
  }
  free(visited);
}

// Post-order DFS from a UOp loc, descending into UOp children (in
// arity/slot order) before assigning the node its own fire-order
// sequence number.  Mirrors kernel_fire_by_id (interact/uop_kernel.c:
// 207): a kernel fires every input producer first, then itself.  We
// assign a sequence to EVERY visited bufferize node (not just the
// realized ones) so the resulting order is a single global post-order;
// the topo sort reads back only the boundary nodes' sequences, whose
// relative order is preserved.  `*next_seq` runs across the whole walk;
// `visited` dedups (each node fires once).
static void boundary_fire_seq_rec(u64 from_loc, u8 *visited, u32 *next_seq) {
  if (from_loc >= HEAP_NEXT) return;
  if (visited[from_loc]) return;
  visited[from_loc] = 1;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return;
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    if (term_tag(child) != TAG_UOP)    continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    boundary_fire_seq_rec(cloc, visited, next_seq);
  }
  if (BOUNDARY_FIRE_SEQ[idx] == 0xFFFFFFFFu) {
    BOUNDARY_FIRE_SEQ[idx] = (*next_seq)++;
  }
}

// Compute BOUNDARY_FIRE_SEQ[] by one post-order DFS rooted at the
// realize sink.  Nodes unreachable from the root keep 0xFFFFFFFF and
// fall back to depth-order in the topo sort.
static void boundary_compute_fire_seq(Term root) {
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++)
    BOUNDARY_FIRE_SEQ[i] = 0xFFFFFFFFu;
  if (HEAP_NEXT == 0) return;
  u8 *visited = (u8 *)calloc(HEAP_NEXT, 1);
  if (visited == NULL) return;
  u32 next_seq = 0;
  boundary_fire_seq_rec(term_val(root), visited, &next_seq);
  free(visited);
}

// Forward decl: input_slot_dedup adds (tid, term) to ke->input_tids[]
// (or returns the existing slot).  Defined in the build_kernel section.
static u32 input_slot_dedup(KernelEntry *ke, u32 tid, Term term);

// The unified-pass store_root carries TAG_TEN leaves (wrapped inside
// UOP_INDEX_E.buffer slots; see ru_rewrite_subtree in
// rangeify_unified.c).  cpu_uop_walk binds the kernel's runtime input
// table to UOP_BUFFER leaves matched by the (slot+1) instance
// disambiguator, so substitute every TAG_TEN whose tid appears in
// ke->input_tids[] with the matching UOP_BUFFER -- the rewritten
// subtree becomes structurally compatible with the walker's identity
// binding.

#define UNIFIED_REWRITE_MEMO_CAP 4096
typedef struct {
  Term key;
  Term value;
} UnifiedRewriteMemoSlot;

typedef struct {
  KernelEntry           *ke;
  UnifiedRewriteMemoSlot memo[UNIFIED_REWRITE_MEMO_CAP];
  u32                    memo_used;
} UnifiedRewriteState;

static u32 unified_rewrite_hash(Term t) {
  u64 x = t;
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return (u32)x & (UNIFIED_REWRITE_MEMO_CAP - 1);
}

static int unified_rewrite_memo_lookup(UnifiedRewriteState *st, Term key,
                                       Term *out) {
  u32 h = unified_rewrite_hash(key);
  for (u32 p = 0; p < UNIFIED_REWRITE_MEMO_CAP; p++) {
    u32 i = (h + p) & (UNIFIED_REWRITE_MEMO_CAP - 1);
    if (st->memo[i].key == 0) return 0;
    if (st->memo[i].key == key) { *out = st->memo[i].value; return 1; }
  }
  return 0;
}

static void unified_rewrite_memo_insert(UnifiedRewriteState *st, Term key,
                                        Term value) {
  if (st->memo_used * 2 >= UNIFIED_REWRITE_MEMO_CAP) return;
  u32 h = unified_rewrite_hash(key);
  for (u32 p = 0; p < UNIFIED_REWRITE_MEMO_CAP; p++) {
    u32 i = (h + p) & (UNIFIED_REWRITE_MEMO_CAP - 1);
    if (st->memo[i].key == 0) {
      st->memo[i].key = key;
      st->memo[i].value = value;
      st->memo_used++;
      return;
    }
    if (st->memo[i].key == key) {
      st->memo[i].value = value;
      return;
    }
  }
}

// Build the UOP_BUFFER replacement for a TAG_TEN leaf if `tid` is an
// input slot in `ke`.  Returns 0 when no match (caller keeps the
// original TAG_TEN; the unified pass may carry tensor handles that
// aren't kernel inputs, e.g. constants or output backrefs).
//
// View-aliased fallback: when the rewriter sees a TAG_TEN whose tid
// predates the consumer's view_resolve aliasing (e.g. matmul's inner W
// reference, before EXPAND folded into an alias tid), the initial
// exact-tid scan misses. tensor_view_of clones share buf_id, so a
// second pass matches by underlying buffer -- the consumer reads the
// same bytes regardless of which alias-tid the input slot tracks.
static Term unified_rewrite_buffer_for_tid(KernelEntry const *ke, u32 tid) {
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  if (ke->input_tids == NULL) return 0;
  for (u32 slot = 0; slot < ke->n_inputs; slot++) {
    if (ke->input_tids[slot] != tid) continue;
    u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
    TenDesc const *td = &TENS[tid];
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                           td->view.shape.ndim, td->view.shape.dims,
                           slot + 1);
  }
  // Fall back to buf_id match for view-aliased tids.
  u32 want_buf = TENS[tid].buf_id;
  if (want_buf != 0) {
    for (u32 slot = 0; slot < ke->n_inputs; slot++) {
      u32 in_tid = ke->input_tids[slot];
      if (in_tid == 0 || in_tid >= TENS_NEXT) continue;
      if (TENS[in_tid].buf_id != want_buf) continue;
      u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
      TenDesc const *td = &TENS[in_tid];
      return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                             td->view.shape.ndim, td->view.shape.dims,
                             slot + 1);
    }
  }
  return 0;
}

// Same lookup as unified_rewrite_buffer_for_tid, but when no slot
// matches, dynamically extend ke->input_tids[] with the new tid and
// mirror the slot into cached_lift.in_bufs[] so cpu_uop_walk binds a
// runtime buffer for it.  Synthesizes an input slot for every TAG_TEN
// referenced inside the lifted value subtree.  Used by the TAG_TEN
// handler in unified_rewrite_rec_sub when the consumer's input_tids[]
// doesn't carry the tid (the unified pass inlined a producer BUFFERIZE
// whose value subtree references a TAG_TEN that's not in the consumer's
// iter scope).
static Term unified_rewrite_buffer_for_tid_extend(KernelEntry *ke, u32 tid) {
  Term repl = unified_rewrite_buffer_for_tid(ke, tid);
  if (repl != 0) return repl;
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  if (ke->n_inputs >= KERNEL_LIFT_MAX_INPUT) return 0;
  TenDesc const *td = &TENS[tid];
  Term ten_term = term_new(0, TAG_TEN, td->dtype, tid);
  u32 slot = input_slot_dedup(ke, tid, ten_term);
  if (slot >= KERNEL_LIFT_MAX_INPUT) return 0;
  u32 dtype = ke->input_dtypes[slot];
  Term buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                             td->view.shape.ndim, td->view.shape.dims,
                             slot + 1);
  // Mirror into cached_lift.in_bufs[] / n_inputs so cpu_uop_walk's
  // ctx.in_terms[slot] resolves to the same BUFFER inst we just
  // emitted into the rewritten value tree.  in_chain_composed defaults
  // to 0 for the new slot (kernel_inputs_reserve zero-pads); per-slot
  // view-aware addressing kicks in only for tids wired through
  // ru_pass's input_chain logic.
  if (slot < KERNEL_LIFT_MAX_INPUT) {
    ke->cached_lift.in_bufs[slot] = buf;
    if (ke->cached_lift.n_inputs < ke->n_inputs) {
      ke->cached_lift.n_inputs = ke->n_inputs;
    }
  }
  return buf;
}

// Resolve a UOP_BUFFERIZE Term (an upstream realized boundary) back to
// its producer tid via BOUNDARY_BUFFERIZE_TERM[] / BOUNDARY_TID[], then
// build the matching UOP_BUFFER for that input slot.  Returns 0 when
// the bufferize doesn't correspond to one of this kernel's inputs (it
// may be the kernel's own output bufferize, or a sibling boundary that
// hasn't been wired through input_tids[]).
static Term unified_rewrite_buffer_for_bufferize(KernelEntry const *ke,
                                                 Term buf) {
  for (u32 bi = 0; bi < BOUNDARY_ORDER_LEN; bi++) {
    if (BOUNDARY_BUFFERIZE_TERM[bi] != buf) continue;
    u32 tid = BOUNDARY_TID[bi];
    return unified_rewrite_buffer_for_tid(ke, tid);
  }
  // Fallback: identity match missed (often because addrspace or
  // closed_ranges differ between the consumer-side INDEX_E.buf and
  // the boundary's stored bufferize term).  Match by producer value
  // instead -- two bufferize terms wrapping the same value subtree
  // refer to the same realized buffer.
  if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFERIZE) {
    Term want_value = uop_bufferize_value(buf);
    if (want_value != 0) {
      for (u32 bi = 0; bi < BOUNDARY_ORDER_LEN; bi++) {
        Term cand = BOUNDARY_BUFFERIZE_TERM[bi];
        if (cand == 0 || cand == buf) continue;
        if (term_tag(cand) != TAG_UOP || term_ext(cand) != UOP_BUFFERIZE) continue;
        if (uop_bufferize_value(cand) != want_value) continue;
        u32 tid = BOUNDARY_TID[bi];
        return unified_rewrite_buffer_for_tid(ke, tid);
      }
    }
  }
  return 0;
}

// Mutating variant: when the boundary lookup finds a producer tid but
// unified_rewrite_buffer_for_tid declines (tid not yet in this kernel's
// input_tids[]), EXTEND ke->input_tids[] with the new tid and mint a
// UOP_BUFFER for the new slot.  Mirrors unified_rewrite_buffer_for_tid_extend's
// TAG_TEN path.  Without this, bench-train backward kernels (where the
// unified pass produces BUFFERIZE Terms whose producer tid isn't in the
// consumer's static input_tids[]) leave bare UOP_BUFFERIZE leaves in the
// rendered DAG; rmu_buf_name then falls through to `buf{loc}` and Metal
// fails to compile on the undeclared identifier.
static Term unified_rewrite_buffer_for_bufferize_extend(KernelEntry *ke,
                                                        Term buf) {
  Term repl = unified_rewrite_buffer_for_bufferize(ke, buf);
  if (repl != 0) return repl;
  // Look up boundary's producer tid and extend.
  u32 tid = 0;
  for (u32 bi = 0; bi < BOUNDARY_ORDER_LEN; bi++) {
    if (BOUNDARY_BUFFERIZE_TERM[bi] == buf) { tid = BOUNDARY_TID[bi]; break; }
  }
  if (tid == 0 && term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFERIZE) {
    Term want_value = uop_bufferize_value(buf);
    if (want_value != 0) {
      for (u32 bi = 0; bi < BOUNDARY_ORDER_LEN; bi++) {
        Term cand = BOUNDARY_BUFFERIZE_TERM[bi];
        if (cand == 0 || cand == buf) continue;
        if (term_tag(cand) != TAG_UOP || term_ext(cand) != UOP_BUFFERIZE) continue;
        if (uop_bufferize_value(cand) != want_value) continue;
        tid = BOUNDARY_TID[bi];
        break;
      }
    }
  }
  if (tid == 0) return 0;
  return unified_rewrite_buffer_for_tid_extend(ke, tid);
}

// TAG_VAR substitute: visit() registers a TLam-bound TVAR (with a
// shape annotation in the lam_shape side table) as an input slot with
// input_tids[slot]==0 and input_terms[slot]==var_term.  Match that
// slot here and synthesize the corresponding UOP_BUFFER so cpu_uop_walk
// binds the runtime buffer (resolved at fire time through term_resolve
// on the symbolic var) to the (slot+1) inst the walker expects.
static Term unified_rewrite_buffer_for_var(KernelEntry const *ke,
                                           Term var_term) {
  if (ke->input_tids == NULL || ke->input_terms == NULL) return 0;
  for (u32 slot = 0; slot < ke->n_inputs; slot++) {
    if (ke->input_tids[slot] != 0) continue;
    if (ke->input_terms[slot] != var_term) continue;
    u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
    u32 numel = (ke->input_numels != NULL) ? ke->input_numels[slot] : 1;
    Shape s = {0}; s.ndim = 1; s.dims[0] = numel;
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, s.ndim, s.dims, slot + 1);
  }
  return 0;
}

// Substitution map: replace any Term-identity match of from[i] with to[i]
// while rewriting `t`.  Mirrors tinygrad's `UOp.substitute(map)` over a
// closed-range scope: a BUFFERIZE's closed_ranges (the producer's iter
// axes) get re-bound to the consumer's INDEX_E.addr components so the
// rewritten subtree iterates in the consumer's range scope.  The TAG_TEN
// / TAG_VAR / kernel-input-BUFFERIZE rewrites still apply on the way down.
//
// Capacity is NOT MAX_DIM: a single BUFFERIZE closes over at most MAX_DIM
// axes, but the substitution table ACCUMULATES across a nested-BUFFERIZE
// chain (each level appends its closed_ranges to the parent's table).  A
// stacked-conv backward inlines BUFFERIZE(3)->BUFFERIZE(3)->BUFFERIZE(4)
// = 10 entries; sizing this at MAX_DIM==8 silently dropped the last two
// (the append guards stop at the cap), leaving those closed_ranges
// unbound -> stranded RANGE leaves -> the walker read iter=0 and the
// gradient came out zero.  Sized to hold several conv levels of nesting;
// overflow now bails the inline rather than truncating to a wrong result.
#define UNIFIED_SUBST_CAP 64
typedef struct {
  Term from[UNIFIED_SUBST_CAP];
  Term to  [UNIFIED_SUBST_CAP];
  u32  n;
} UnifiedSubst;

static Term unified_rewrite_rec_sub(UnifiedRewriteState *st,
                                    UnifiedSubst const *sub,
                                    Term t, u32 depth);
static u64 uop_buffer_numel(Term t);

static Term unified_rewrite_rec(UnifiedRewriteState *st, Term t, u32 depth) {
  return unified_rewrite_rec_sub(st, NULL, t, depth);
}

// Walk a producer value subtree and return 1 if it contains any
// CMPEQ / CMPLT / IWHERE / INVALID op -- the family whose semantics
// depend on which consumer axis the producer's closed_range gets
// bound to.  When a producer's value is mask-free, binding its
// closed_range to a consumer REDUCE-type RANGE is safe: the value is
// the same numeric expression regardless of whether the axis is
// looped over or reduce-accumulated.  When a CMPEQ/IWHERE is present
// (pool/reshape-MAX gradient one-hot indicator), pairing the wrong
// axis routes the mask through the reduce iter and decodes wrong.
// Used to relax the type=0 gate in try_inline_bufferize_1axis_via_decomp
// for plain-arithmetic producers such as BN's vector broadcast or the
// non-matmul-REDUCE-feed broadcast subtract.
#define MASK_SCAN_VISITED_CAP 256
typedef struct {
  Term keys[MASK_SCAN_VISITED_CAP];
  u32  n;
} MaskScanVisited;
static int mask_scan_seen(MaskScanVisited *v, Term t) {
  for (u32 i = 0; i < v->n; i++) if (v->keys[i] == t) return 1;
  if (v->n < MASK_SCAN_VISITED_CAP) v->keys[v->n++] = t;
  return 0;
}
static int uop_value_subtree_has_mask_op_rec(MaskScanVisited *v,
                                              Term t, u32 depth) {
  if (depth > 256) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  if (mask_scan_seen(v, r)) return 0;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_CMPEQ || op == UOP_CMPLT || op == UOP_IWHERE
      || op == UOP_INVALID) return 1;
  // Stop at boundaries: a nested BUFFERIZE is opaque (its value subtree
  // gets evaluated under its OWN closed_ranges, independent of the
  // outer producer's axis bindings).  Same for KERNEL/BUFFER leaves.
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return 0;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (uop_value_subtree_has_mask_op_rec(v, heap_read(loc + i), depth + 1)) return 1;
  }
  return 0;
}
static int uop_value_subtree_has_mask_op(Term root) {
  if (root == 0) return 0;
  MaskScanVisited v;
  v.n = 0;
  return uop_value_subtree_has_mask_op_rec(&v, root, 0);
}

// Try inlining a 1-axis BUFFERIZE by decomposing the consumer's flat
// addr expression into (stride, iter_expr) leaves and binding the
// closed_range to the unique iter_expr whose extent matches.  Two
// sub-cases:
//
//   * dn >= 2: addr decomposes into multiple stride*expr pairs.  Find
//     the unique bare-RANGE leaf with extent matching closed_range[0];
//     bail on type=1 (reduce-axis) collisions to avoid routing the
//     pool/reshape-MAX gradient's CMPEQ mask through the wrong axis.
//
//   * dn == 0 && has_unknown_extent: addr is pure-swizzler (IDIV/IMOD
//     with no bare RANGE leaf).  Substitute closed_range[0] with the
//     full addr_term provided addr_term's free RANGE leaves are all
//     type=0 (no reduce-axis); a type=1 leaf would bind closed_range
//     to a value varying across consumer's REDUCE iter.
//
// Returns the substituted Term on hit, 0 to signal no match.
static Term try_inline_bufferize_1axis_via_decomp(
    UnifiedRewriteState *st, UnifiedSubst const *sub,
    Term inner_buf, Term v, Term addr_term, u32 depth) {
  Term old_r = uop_bufferize_range_at(inner_buf, 0);
  if (old_r == 0
      || term_tag(old_r) != TAG_UOP
      || term_ext(old_r) != UOP_RANGE) return 0;
  u32 want_ext = (u32)term_val(heap_read(term_val(old_r) + 2));
  Term decomp_stack[UNIFIED_SUBST_CAP * 2];
  Term decomp_exprs[UNIFIED_SUBST_CAP];
  u32  dtop = 0;
  u32  dn   = 0;
  decomp_stack[dtop++] = addr_term;
  int decomp_ok = 1;
  int has_unknown_extent = 0;
  while (dtop > 0 && decomp_ok) {
    Term cur = term_resolve(decomp_stack[--dtop]);
    u8   cop = (term_tag(cur) == TAG_UOP) ? term_ext(cur) : 0xFF;
    if (cop == UOP_IADD && dtop + 2 <= UNIFIED_SUBST_CAP * 2) {
      decomp_stack[dtop++] = heap_read(term_val(cur) + 0);
      decomp_stack[dtop++] = heap_read(term_val(cur) + 1);
      continue;
    }
    if (cop == UOP_CONST
        && term_val(heap_read(term_val(cur) + 0)) == 0) continue;
    Term e = cur;
    if (cop == UOP_IMUL) {
      Term a = term_resolve(heap_read(term_val(cur) + 0));
      Term b = term_resolve(heap_read(term_val(cur) + 1));
      if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) e = a;
      else if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) e = b;
    }
    if (term_tag(e) != TAG_UOP || term_ext(e) != UOP_RANGE) {
      has_unknown_extent = 1;
      continue;
    }
    if (dn >= UNIFIED_SUBST_CAP) { decomp_ok = 0; break; }
    decomp_exprs[dn++] = e;
  }
  // Sub-case 0: dn == 1 -- the addr is one bare RANGE (possibly with a
  // stride coeff) plus broadcast/swizzler components the BUFFERIZE
  // doesn't close over.  When that single RANGE's extent matches the
  // closed_range extent and it's a regular (non-reduce) axis, bind
  // closed_range[0] to it.  The consumer's other addr components index
  // axes this 1-range BUFFERIZE replicates over (the producer's value
  // only references its own closed_range), so dropping them is sound.
  // This is the conv2d input-gradient / PAD-over-compute reverse: the
  // cotangent BUFFERIZE closed over one axis, read at a compound
  // (axis*stride + WHERE-shifted-other-axis) addr.  Without this case
  // the residual BUFFERIZE leaks and cpu_uop_walk reads zeros
  // (project_thvm_mul_shared_subgraph_zero_grad).
  if (decomp_ok && dn == 1) {
    u32 e_ext = (u32)term_val(heap_read(term_val(decomp_exprs[0]) + 2));
    u32 atype = (u32)term_val(heap_read(term_val(decomp_exprs[0]) + 1));
    if (e_ext == want_ext && atype == 0) {
      UnifiedSubst new_sub;
      new_sub.n = 0;
      if (sub != NULL) {
        for (u32 i = 0; i < sub->n && new_sub.n < UNIFIED_SUBST_CAP; i++) {
          new_sub.from[new_sub.n] = sub->from[i];
          new_sub.to  [new_sub.n] = sub->to  [i];
          new_sub.n++;
        }
      }
      if (new_sub.n < UNIFIED_SUBST_CAP) {
        new_sub.from[new_sub.n] = old_r;
        new_sub.to  [new_sub.n] = decomp_exprs[0];
        new_sub.n++;
      }
      return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
    }
  }
  // Sub-case 1: dn >= 2 stride-match by extent.
  // When the producer's value subtree is mask-free (no CMPEQ/CMPLT/
  // IWHERE/INVALID), pairing with a type=1 (reduce) RANGE is safe --
  // the producer's value is the same numeric expression regardless of
  // whether the consumer treats the axis as a loop iter or a reduce
  // accumulator.  This covers BN's vector broadcast and the non-matmul-
  // REDUCE-feed broadcast subtract that hangs beautiful_mnist when the
  // conservative REDUCE-as-boundary seed is dropped (env-toggle
  // THVM_BUFFERIZE_KEEP_NONMATMUL_REDUCE=0).  Pool/reshape-MAX
  // gradient's CMPEQ one-hot indicator keeps the strict gate.
  int producer_mask_free = !uop_value_subtree_has_mask_op(v);
  if (decomp_ok && !has_unknown_extent && dn >= 2) {
    int found = -1;
    for (u32 j = 0; j < dn; j++) {
      Term e = decomp_exprs[j];
      u32 e_ext = (u32)term_val(heap_read(term_val(e) + 2));
      if (e_ext != want_ext) continue;
      u32 atype = (u32)term_val(heap_read(term_val(e) + 1));
      if (atype != 0 && !producer_mask_free) { found = -1; break; }
      if (found >= 0) { found = -1; break; }
      found = (i32)j;
    }
    if (found >= 0) {
      UnifiedSubst new_sub;
      new_sub.n = 0;
      if (sub != NULL) {
        for (u32 i = 0; i < sub->n && new_sub.n < UNIFIED_SUBST_CAP; i++) {
          new_sub.from[new_sub.n] = sub->from[i];
          new_sub.to  [new_sub.n] = sub->to  [i];
          new_sub.n++;
        }
      }
      if (new_sub.n < UNIFIED_SUBST_CAP) {
        new_sub.from[new_sub.n] = old_r;
        new_sub.to  [new_sub.n] = decomp_exprs[found];
        new_sub.n++;
      }
      return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
    }
  }
  // Sub-case 2: pure-swizzler addr with no bare-RANGE subcomponent.
  if (decomp_ok && has_unknown_extent && dn == 0) {
    int safe = 1;
    Term sscan[UNIFIED_SUBST_CAP * 4];
    u32  stop = 0;
    sscan[stop++] = addr_term;
    while (stop > 0 && safe) {
      Term cur = term_resolve(sscan[--stop]);
      if (term_tag(cur) != TAG_UOP) continue;
      u8  scop = term_ext(cur);
      u64 sloc = term_val(cur);
      if (scop == UOP_RANGE) {
        u32 atype = (u32)term_val(heap_read(sloc + 1));
        if (atype != 0) { safe = 0; break; }
        continue;
      }
      if (scop == UOP_BUFFER || scop == UOP_BUFFERIZE || scop == UOP_KERNEL) continue;
      u8 ar = uop_arity(scop);
      for (u8 i = 0; i < ar && stop < UNIFIED_SUBST_CAP * 4; i++) {
        sscan[stop++] = heap_read(sloc + i);
      }
    }
    if (safe) {
      UnifiedSubst new_sub;
      new_sub.n = 0;
      if (sub != NULL) {
        for (u32 i = 0; i < sub->n && new_sub.n < UNIFIED_SUBST_CAP; i++) {
          new_sub.from[new_sub.n] = sub->from[i];
          new_sub.to  [new_sub.n] = sub->to  [i];
          new_sub.n++;
        }
      }
      if (new_sub.n < UNIFIED_SUBST_CAP) {
        new_sub.from[new_sub.n] = old_r;
        new_sub.to  [new_sub.n] = addr_term;
        new_sub.n++;
      }
      return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
    }
  }
  return 0;
}

// Legacy multi-axis BUFFERIZE inline: when the per-axis side table
// has no entry (or its lookup misses), decompose the consumer's flat
// addr into (stride, expr) pairs and try two matching passes:
//
//   1. Stride match: requires got_n == n_ranges after same-stride
//      merge; pairs closed_range[i] (row-major stride want_strides[i])
//      with the unique got expr at matching stride.
//   2. Extent match: when got_n > n_ranges (consumer addressed
//      through its full iter incl. reduce / broadcast axes the
//      producer dropped), pair closed_range[i] (extent dims[i]) with
//      the unique bare-RANGE got entry of matching extent.  Bails on
//      reduce-axis (type=1) or non-RANGE got_exprs to keep pool/
//      reshape-MAX gradients on the safe path.
//
// Returns the substituted Term on hit, 0 to signal no match.
static Term try_inline_bufferize_multi_via_stride_match(
    UnifiedRewriteState *st, UnifiedSubst const *sub,
    Term inner_buf, Term v, Term addr_term, u32 n_ranges, u32 depth) {
  u32 dims[UNIFIED_SUBST_CAP] = {0};
  Term old_ranges[UNIFIED_SUBST_CAP] = {0};
  for (u32 i = 0; i < n_ranges; i++) {
    Term cr = uop_bufferize_range_at(inner_buf, i);
    if (cr == 0 || term_tag(cr) != TAG_UOP || term_ext(cr) != UOP_RANGE) return 0;
    u32 ext = (u32)term_val(heap_read(term_val(cr) + 2));
    if (ext == 0) return 0;
    old_ranges[i] = cr;
    dims[i] = ext;
  }
  u32 want_strides[UNIFIED_SUBST_CAP] = {0};
  want_strides[n_ranges - 1] = 1;
  for (i32 i = (i32)n_ranges - 2; i >= 0; i--) {
    want_strides[i] = want_strides[i + 1] * dims[i + 1];
  }
  // Decompose addr_term into (stride, expr) pairs via IADD-walk.
  u32 got_strides[UNIFIED_SUBST_CAP] = {0};
  Term got_exprs  [UNIFIED_SUBST_CAP] = {0};
  u32 got_n = 0;
  Term stack[UNIFIED_SUBST_CAP * 2];
  u32  top = 0;
  stack[top++] = addr_term;
  int decompose_ok = 1;
  while (top > 0 && decompose_ok) {
    Term cur = term_resolve(stack[--top]);
    u8  cop = (term_tag(cur) == TAG_UOP) ? term_ext(cur) : 0xFF;
    if (cop == UOP_IADD && top + 2 <= UNIFIED_SUBST_CAP * 2) {
      stack[top++] = heap_read(term_val(cur) + 0);
      stack[top++] = heap_read(term_val(cur) + 1);
      continue;
    }
    if (cop == UOP_CONST && term_val(heap_read(term_val(cur) + 0)) == 0) continue;
    u32 s = 1;
    Term e = cur;
    if (cop == UOP_IMUL) {
      Term a = term_resolve(heap_read(term_val(cur) + 0));
      Term b = term_resolve(heap_read(term_val(cur) + 1));
      if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) {
        s = (u32)term_val(heap_read(term_val(b) + 0)); e = a;
      } else if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) {
        s = (u32)term_val(heap_read(term_val(a) + 0)); e = b;
      }
    }
    if (got_n >= UNIFIED_SUBST_CAP) { decompose_ok = 0; break; }
    got_strides[got_n] = s;
    got_exprs  [got_n] = e;
    got_n++;
  }
  if (!decompose_ok) return 0;
  // Merge same-stride entries (e.g. im2col kernel_row + patch_row at
  // stride 1 in the same source axis) so the first-pass stride match
  // sees got_n == n_ranges.
  if (got_n > n_ranges) {
    u32 m_strides[UNIFIED_SUBST_CAP] = {0};
    Term m_exprs[UNIFIED_SUBST_CAP] = {0};
    u32 m_n = 0;
    int merge_ok = 1;
    for (u32 j = 0; j < got_n; j++) {
      u32 s = got_strides[j];
      int found = -1;
      for (u32 k = 0; k < m_n; k++) {
        if (m_strides[k] == s) { found = (i32)k; break; }
      }
      if (found < 0) {
        if (m_n >= UNIFIED_SUBST_CAP) { merge_ok = 0; break; }
        m_strides[m_n] = s; m_exprs[m_n] = got_exprs[j]; m_n++;
      } else {
        m_exprs[found] = uop_int_binary(UOP_IADD, m_exprs[found], got_exprs[j]);
      }
    }
    if (merge_ok && m_n == n_ranges) {
      for (u32 j = 0; j < m_n; j++) {
        got_strides[j] = m_strides[j]; got_exprs[j] = m_exprs[j];
      }
      got_n = m_n;
    }
  }
  int matched = 0;
  Term to_terms[UNIFIED_SUBST_CAP] = {0};
  // First pass: stride match.
  if (got_n == n_ranges) {
    int match_ok = 1;
    for (u32 i = 0; i < n_ranges && match_ok; i++) {
      if (dims[i] == 1) { to_terms[i] = uop_const(DT_INT32, 0); continue; }
      int found = -1;
      for (u32 j = 0; j < got_n; j++) {
        if (got_strides[j] == want_strides[i]) {
          if (found >= 0) { found = -1; break; }
          found = (i32)j;
        }
      }
      if (found < 0) { match_ok = 0; break; }
      to_terms[i] = got_exprs[found];
    }
    if (match_ok) matched = 1;
  }
  // Second pass: extent match when got_n > n_ranges.
  if (!matched && got_n > n_ranges) {
    int safe = 1;
    for (u32 j = 0; j < got_n && safe; j++) {
      Term ge = got_exprs[j];
      if (term_tag(ge) != TAG_UOP || term_ext(ge) != UOP_RANGE) { safe = 0; break; }
      u32 atype = (u32)term_val(heap_read(term_val(ge) + 1));
      if (atype != 0) { safe = 0; break; }
    }
    u8  used[UNIFIED_SUBST_CAP] = {0};
    int match_ok = safe;
    for (u32 i = 0; i < n_ranges && match_ok; i++) {
      if (dims[i] == 1) { to_terms[i] = uop_const(DT_INT32, 0); continue; }
      int found = -1;
      for (u32 j = 0; j < got_n; j++) {
        if (used[j]) continue;
        Term ge = got_exprs[j];
        u32 ge_ext = (u32)term_val(heap_read(term_val(ge) + 2));
        if (ge_ext != dims[i]) continue;
        if (found >= 0) { found = -1; break; }
        found = (i32)j;
      }
      if (found < 0) { match_ok = 0; break; }
      to_terms[i] = got_exprs[found];
      used[found] = 1;
    }
    if (match_ok) matched = 1;
  }
  if (!matched) return 0;
  // Bail (decline the inline) rather than silently truncate: dropping a
  // closed_range substitution leaves that RANGE unbound in the body,
  // which surfaces as a stranded-range gate trip downstream.
  if ((sub ? sub->n : 0) + n_ranges > UNIFIED_SUBST_CAP) return 0;
  UnifiedSubst new_sub;
  new_sub.n = 0;
  if (sub != NULL) {
    for (u32 i = 0; i < sub->n; i++) {
      new_sub.from[new_sub.n] = sub->from[i];
      new_sub.to  [new_sub.n] = sub->to  [i];
      new_sub.n++;
    }
  }
  for (u32 i = 0; i < n_ranges; i++) {
    new_sub.from[new_sub.n] = old_ranges[i];
    new_sub.to  [new_sub.n] = to_terms[i];
    new_sub.n++;
  }
  return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
}

// Try inlining a BUFFERIZE via the per-axis index side table populated
// in rangeify_unified.c::ru_index_axes_register.  Mirror: tinygrad's
// `BUFFERIZE.index(*[r for i,r in enumerate(ctx.range_map[x][0]) if i
// in realized_ranges])` (indexing.py:78) carries per-axis range terms
// in the INDEX node's args.  thvm's INDEX_E uses a flat addr; the side
// table preserves the per-axis info at construction so the inline
// rewriter can substitute closed_range[i] -> consumer in_rng at the
// producer's realized_axes[i] WITHOUT decomposing the flat addr (which
// fails on movement-op swizzlers).
//
// Returns the substituted Term on hit (caller short-circuits), or 0
// to signal "table miss, try other inline strategies".
static Term try_inline_bufferize_via_axis_table(
    UnifiedRewriteState *st, UnifiedSubst const *sub,
    Term resolved, Term inner_buf, Term v, u32 n_ranges, u32 depth) {
  Term ax_rngs[MAX_DIM];
  u8 ax_n = rangeify_unified_index_axes_lookup(
      term_val(resolved), ax_rngs, MAX_DIM);
  if (ax_n < n_ranges) return 0;
  // First scan BOUNDARY_BUFFERIZE_TERM[] (the realized-as-kernel
  // boundaries); fall back to RU_BUFFERIZE_TERM[] for orphan
  // BUFFERIZEs whose node didn't become a separate kernel.
  u32 prod_idx = 0xFFFFFFFFu;
  for (u32 bi = 0; bi < BOUNDARY_ORDER_LEN; bi++) {
    if (BOUNDARY_BUFFERIZE_TERM[bi] == inner_buf) {
      prod_idx = bufferize_info_find(BOUNDARY_ORDER[bi]);
      break;
    }
  }
  if (prod_idx == 0xFFFFFFFFu) {
    prod_idx = rangeify_unified_node_idx_for_bufferize(inner_buf);
  }
  if (prod_idx == 0xFFFFFFFFu) return 0;
  u32 prod_ndim = rangeify_unified_out_ndim_at(prod_idx);
  // realized_axes[i] = the i-th producer output axis position that was
  // CLOSED (became an actual UOP_RANGE closed_range) in this BUFFERIZE.
  // Mirror tinygrad indexing.py:66 `closed_ranges = tuple([r for i,r in
  // enumerate(range_map[s][1]) if i in realized_ranges])` and thvm's own
  // ru_collect_closed_ranges (rangeify_unified.c): a realized output axis
  // whose out_rng collapsed to CONST(0) (a keepdim size-1 axis) carries no
  // stride and is NOT a closed_range, so it is skipped here.  Filtering on
  // the actual UOP_RANGE out_rng (not just the axes_mask popcount) keeps
  // n_realized == n_ranges for the keepdim-reduce broadcast case, so the
  // i-th closed_range binds POSITIONALLY to the i-th surviving output axis
  // -- never disambiguated by extent.
  int prod_full = rangeify_unified_realized_full_at(prod_idx);
  u8 axes_mask = rangeify_unified_axes_mask_at(prod_idx);
  u32 realized_axes[MAX_DIM] = {0};
  u32 n_realized = 0;
  for (u32 a = 0; a < prod_ndim && a < MAX_DIM; a++) {
    int axis_realized = prod_full || (axes_mask & (u8)(1u << a));
    if (!axis_realized) continue;
    Term r = rangeify_unified_out_rng_at(prod_idx, a);
    if (r == 0 || term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    if (n_realized < MAX_DIM) realized_axes[n_realized] = a;
    n_realized++;
  }
  if (n_realized != n_ranges || ax_n < prod_ndim) return 0;
  // Bail rather than truncate (see stride_match note): every closed_range
  // must get a binding or the body strands an unbound RANGE.
  if ((sub ? sub->n : 0) + n_ranges > UNIFIED_SUBST_CAP) return 0;
  UnifiedSubst new_sub;
  new_sub.n = 0;
  if (sub != NULL) {
    for (u32 i = 0; i < sub->n; i++) {
      new_sub.from[new_sub.n] = sub->from[i];
      new_sub.to  [new_sub.n] = sub->to  [i];
      new_sub.n++;
    }
  }
  for (u32 i = 0; i < n_ranges; i++) {
    Term cr = uop_bufferize_range_at(inner_buf, i);
    if (cr == 0 || term_tag(cr) != TAG_UOP
        || term_ext(cr) != UOP_RANGE) return 0;
    u32 ax = realized_axes[i];
    if (ax >= ax_n) return 0;
    // The side-table ax_rngs[] are the consumer's in_rngs as they stood
    // when THIS INDEX_E was built.  When this BUFFERIZE is itself being
    // inlined into a grandparent consumer (a chain of inlines composing,
    // e.g. the data-grad mul reads a rsqrt-cube correction whose value
    // again reads the realized rsqrt buffer), an outer subst is active
    // that maps those construction-time ranges to the grandparent's live
    // iteration ranges.  Thread the side-table range through the active
    // subst FIRST so the binding lands on the grandparent's ranges, not
    // the stale construction-time ones.  This mirrors tinygrad
    // indexing.py:78 where `ctx.range_map[x][0]` is always the LIVE
    // consumer iteration: as the bottom-up rewrite composes nested
    // BUFFERIZE.index() reads, every read uses the current consumer's
    // ranges -- the multi-outer-axis general case the bare positional
    // bind missed.
    Term to_rng = ax_rngs[ax];
    if (sub != NULL) {
      for (u32 si = 0; si < sub->n; si++) {
        if (sub->from[si] == to_rng) { to_rng = sub->to[si]; break; }
      }
    }
    new_sub.from[new_sub.n] = cr;
    new_sub.to  [new_sub.n] = to_rng;
    new_sub.n++;
  }
  return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
}

static Term unified_rewrite_rec_sub(UnifiedRewriteState *st,
                                    UnifiedSubst const *sub,
                                    Term t, u32 depth) {
  if (depth > 256) return t;
  Term resolved = term_resolve(t);
  // Substitution map: when active, replace any Term-identity hit with
  // the matching `to` Term (a consumer-iter expression bound to a
  // BUFFERIZE's closed_range).  Checked first so a RANGE leaf inside
  // the inlined value subtree gets rebound before any other branch
  // (TAG_TEN -> UOP_BUFFER, BUFFERIZE -> kernel-input slot, etc.).
  if (sub != NULL) {
    for (u32 i = 0; i < sub->n; i++) {
      if (sub->from[i] == resolved) return sub->to[i];
    }
  }
  u8 tag = term_tag(resolved);
  if (tag == TAG_TEN) {
    Term repl = unified_rewrite_buffer_for_tid_extend(st->ke, (u32)term_val(resolved));
    return repl != 0 ? repl : resolved;
  }
  if (tag == TAG_VAR) {
    Term repl = unified_rewrite_buffer_for_var(st->ke, resolved);
    return repl != 0 ? repl : resolved;
  }
  if (tag != TAG_UOP) return resolved;
  if (term_ext(resolved) == UOP_KERNEL) return resolved;

  // UOP_BUFFERIZE leaf: an upstream realized-boundary's output buffer.
  // Replace with the kernel's input-slot UOP_BUFFER when the bufferize
  // term maps to one of ke->input_tids[].  Treat as leaf (do not
  // recurse into its src tree) so the consumer's INDEX expression
  // reads against the BUFFER inst that cpu_uop_walk binds to in_ptrs[].
  // Fall through to the generic recurser when no boundary match: the
  // BUFFERIZE may wrap an in-kernel intermediate whose value subtree
  // still references TAG_TEN leaves we need to rewrite into UOP_BUFFER
  // input slots for the cpu_uop_walk to bind correctly.
  if (term_ext(resolved) == UOP_BUFFERIZE) {
    // Use the extend variant: when boundary lookup finds a producer tid
    // but the consumer's static input_tids[] doesn't carry it (common
    // in TGrad backward kernels where ru_pass emits BUFFERIZE refs to
    // tids that weren't pre-registered as kernel inputs), dynamically
    // extend input_tids[] and mint a UOP_BUFFER for the new slot.  Without
    // this the BUFFERIZE leaks to the Metal renderer's rmu_buf_name
    // fallback (`buf{loc}`) and MSL compile fails on the undeclared
    // identifier, which was what blocked beautiful_mnist bench-train
    // at every backward step.
    Term repl = unified_rewrite_buffer_for_bufferize_extend(st->ke, resolved);
    if (repl != 0) return repl;
  }

  // INDEX_E(BUFFERIZE(value, closed_ranges), addr) where the BUFFERIZE
  // doesn't map to a kernel-input boundary -> inline value, rebinding
  // each closed_range RANGE leaf inside `value` to the matching
  // component of `addr`.  Some unified-pass BUFFERIZE wraps an
  // in-kernel intermediate (a constant scalar, a reduce-to-scalar,
  // a product of reduced scalars, a per-row softmax denominator)
  // that isn't a realize boundary.  Such producers must be inlined into
  // their consumer; cpu_uop_walk's value dispatcher has no BUFFERIZE
  // handler and would read 0 if left in place.
  //
  // Inline cases handled here:
  //
  //   * n_ranges == 0: 1-element buffer.  Every addr maps to the same
  //     scalar value; inline `value` and recursively rewrite.
  //
  //   * n_ranges == 1 && addr is a single UOP_RANGE Term: bind the
  //     closed_range to `addr` (a Term-identity rewrite over the value
  //     subtree).  This is the BUFFERIZE(REDUCE(...))-fed-by-consumer-
  //     range pattern that nn cross-entropy backward, layer-norm grad,
  //     and softmax forward all produce.
  //
  // Multi-range BUFFERIZE intermediates with compound addrs (IADD/IMUL
  // strides over multiple consumer ranges) still need an addr-folding
  // rewrite that splits addr into per-axis indices; until that lands
  // the resid gate leaves the BUFFERIZE node in place for those kernels.
  if (term_ext(resolved) == UOP_INDEX_E) {
    Term inner_buf = term_resolve(heap_read(term_val(resolved) + 0));
    Term addr_term = term_resolve(heap_read(term_val(resolved) + 1));
    // Run the addr through the active subst (and TAG_TEN/VAR/BUFFERIZE
    // rewrites) so the rebound expression carries forward into nested
    // inline.  This handles BUFFERIZE(BUFFERIZE(...)) chains where the
    // outer's value subtree contains an inner INDEX_E with a closed_
    // range leaf in its addr -- the outer subst has to fire on that
    // inner addr before the inner inline kicks in.
    if (sub != NULL) {
      addr_term = unified_rewrite_rec_sub(st, sub, addr_term, depth + 1);
    }
    // Use the _extend lookup (boundary-tid + value-match fallback), not
    // the bare one: an INDEX_E reading a REALIZED boundary's BUFFERIZE
    // must read that boundary's buffer (wire it as a kernel input), NOT
    // inline + recompute the producer.  The bare lookup only sees
    // already-registered direct inputs, so a boundary reached through an
    // inlined intermediate (e.g. a stacked conv's relu(conv1) feeding
    // dw2: relu is non-realized so it inlines, dragging conv1's
    // BUFFERIZE in) fell through to the inline block and re-derived the
    // whole producer.  Gating on _extend lets the fall-through read path
    // (leaf-BUFFERIZE -> INDEX_E(BUFFER)) fire for realized boundaries.
    if (term_tag(inner_buf) == TAG_UOP
        && term_ext(inner_buf) == UOP_BUFFERIZE
        && unified_rewrite_buffer_for_bufferize_extend(st->ke, inner_buf) == 0) {
      Term v = uop_bufferize_value(inner_buf);
      // Fast path for CONST: bypass the recursive rewrite (the
      // inner is a literal scalar with no children).
      if (v != 0 && term_tag(v) == TAG_UOP && term_ext(v) == UOP_CONST) {
        return v;
      }
      u32 n_ranges = uop_bufferize_n_ranges(inner_buf);
      if (v != 0 && n_ranges == 0) {
        // Scalar 1-element buffer: any read returns the same value.
        // Recurse with the SAME subst so any RANGE leaves bound by
        // the active subst keep their bindings.
        return unified_rewrite_rec_sub(st, sub, v, depth + 1);
      }
      // Single closed-range, addr is a fresh RANGE Term: rebind the
      // closed_range to addr while rewriting the value subtree.
      // This covers per-row reduce-to-scalar BUFFERIZE intermediates
      // (cross-entropy forward, softmax denominator, layer-norm mean)
      // whose value subtree's only free RANGE is the producer's row
      // axis and whose consumer reads them at the same row iter.
      // Compound IADD/IMUL addrs would also be valid substitutions
      // semantically (the value tree treats closed_range[0] as an
      // opaque i32), but in practice they encode strided / transposed
      // views where the BUFFERIZE's extent doesn't match the consumer
      // iter footprint -- the pool/reshape-reduce-max-then-sum gradient
      // hits this and decodes wrong results.
      if (v != 0 && n_ranges == 1
          && term_tag(addr_term) == TAG_UOP
          && term_ext(addr_term) == UOP_RANGE) {
        Term old_r = uop_bufferize_range_at(inner_buf, 0);
        if (old_r == addr_term) {
          // Closed range already coincides with consumer's addr Term;
          // no substitution needed, just recursively rewrite.
          return unified_rewrite_rec_sub(st, sub, v, depth + 1);
        }
        if (old_r != 0) {
          // Compose: if a subst is already active, extend it with the
          // new (old_r -> addr_term) entry so a chain of BUFFERIZEs
          // each rebinds its own closed_range without losing the
          // outer scope's bindings.  Bounded by UNIFIED_SUBST_CAP.
          UnifiedSubst new_sub;
          new_sub.n = 0;
          if (sub != NULL) {
            for (u32 i = 0; i < sub->n && new_sub.n < UNIFIED_SUBST_CAP; i++) {
              new_sub.from[new_sub.n] = sub->from[i];
              new_sub.to  [new_sub.n] = sub->to  [i];
              new_sub.n++;
            }
          }
          if (new_sub.n < UNIFIED_SUBST_CAP) {
            new_sub.from[new_sub.n] = old_r;
            new_sub.to  [new_sub.n] = addr_term;
            new_sub.n++;
          }
          return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
        }
      }
      // n_ranges==1 broadcast inline: addr is a compound IADD/IMUL
      // tree (the consumer's full row-major addr) but the BUFFERIZE
      // only closed over one axis (the value is replicated along the
      // dropped axes -- a per-row REDUCE-to-scalar fed to a row*col
      // consumer, e.g. softmax denom or layer-norm mean).  Decompose
      // addr into (stride, expr) leaves and look for a UNIQUE bare
      // UOP_RANGE leaf whose extent matches closed_range[0]'s extent
      // AND whose axis_type is 0 (regular iter, not REDUCE).  Bail
      // when the matched got_expr would be a type=1 reduce-axis or
      // when any got_expr is a non-RANGE swizzler whose extent we
      // can't read from a leaf: the pool/reshape-MAX gradient hits
      // an IDIV(R0,2) variant where the only bare RANGE with matching
      // extent is the type=1 reduce axis and pairing it routes the
      // CMPEQ mask through the wrong axis (all-ones instead of one-
      // hot).
      if (v != 0 && n_ranges == 1
          && (term_tag(addr_term) != TAG_UOP
              || term_ext(addr_term) != UOP_RANGE)) {
        // Tinygrad-spec POSITIONAL inline first (indexing.py:66,78): bind
        // the single closed_range to the consumer's in_rng at the
        // producer's realized output axis via the per-axis side table --
        // NEVER by extent equality.  This is the keepdim-reduce broadcast
        // (softmax `f - s.max(kd)`, layer-norm `t - mean`): the producer
        // realized a 1-axis BUFFERIZE (the row reduce-to-scalar), the
        // keepdim col axis collapsed to CONST(0), and the consumer reads
        // it at a compound row*col addr.  The extent-keyed decomp below
        // mis-binds (BAILs -> value collapses to +0.0f) whenever two
        // consumer axes share an extent; the side-table path is
        // unambiguous because it keys on the positional realized axis.
        Term axis_hit = try_inline_bufferize_via_axis_table(
            st, sub, resolved, inner_buf, v, n_ranges, depth);
        if (axis_hit != 0) return axis_hit;
        Term hit = try_inline_bufferize_1axis_via_decomp(
            st, sub, inner_buf, v, addr_term, depth);
        if (hit != 0) return hit;
      }
      // Multi-axis BUFFERIZE: addr is IADD/IMUL tree encoding row-major
      // (per-axis stride * iter) sum.  Mirror ru_build_addr_with_dims
      // (rangeify_unified.c) to derive per-closed_range strides from
      // each closed_range's extent, then decompose addr into
      // (stride -> iter_expr) bindings and substitute each closed_
      // range[i] with the iter_expr whose stride matches strides[i].
      // The producer is row-major over its closed_ranges, so axis i
      // pairs with the i-th decreasing stride.  Used for n_r=2 softmax-
      // CE intermediates and n_r=4 conv2d-grad-input rerolls.
      //
      // When got_n > n_ranges (the consumer's iter has more axes than
      // the producer realized, because the producer dropped reduce /
      // broadcast axes), the stride-match falls back to extent-match
      // on bare-RANGE got_exprs: each closed_range[i] = RANGE.extent
      // E_i pairs to the unique got_exprs[j] that is a bare UOP_RANGE
      // leaf with the same extent.  The remaining got_exprs are
      // broadcast axes the producer's value doesn't reference, so
      // dropping them is sound.  Used for partial-realize chains where
      // the unified pass wraps INDEX_E(BUFFERIZE, addr) with the
      // consumer's full iter addr but the BUFFERIZE only closes over
      // a subset of axes (softmax denom replicated across cols).
      if (v != 0 && n_ranges >= 2 && n_ranges <= UNIFIED_SUBST_CAP) {
        // Tinygrad-spec inline first (indexing.py:78 per-axis index);
        // falls through to stride/extent decomposition when the side
        // table has no entry for this INDEX_E.
        Term axis_hit = try_inline_bufferize_via_axis_table(
            st, sub, resolved, inner_buf, v, n_ranges, depth);
        if (axis_hit != 0) return axis_hit;
        Term stride_hit = try_inline_bufferize_multi_via_stride_match(
            st, sub, inner_buf, v, addr_term, n_ranges, depth);
        if (stride_hit != 0) return stride_hit;
      }
    }
    // Fall through: rebuild the INDEX_E with possibly-substituted
    // addr (the buffer side gets the standard recursion below).
    if (sub != NULL && addr_term != term_resolve(heap_read(term_val(resolved) + 1))) {
      Term buf_rw = unified_rewrite_rec_sub(st, sub, heap_read(term_val(resolved) + 0),
                                            depth + 1);
      // numel-1 BUFFER: reading at any addr other than 0 is OOB.  The
      // unified pass builds addr from raw consumer ranges and leaks
      // the multi-axis expression through; substitute CONST(0) here so
      // the rewritten INDEX_E reads in-bounds regardless of the addr
      // the substitution produced.
      if (term_tag(buf_rw) == TAG_UOP && term_ext(buf_rw) == UOP_BUFFER
          && uop_buffer_numel(buf_rw) == 1) {
        addr_term = uop_const(DT_INT32, 0);
      }
      Term new_index = uop_index_e(buf_rw, addr_term);
      // Propagate per-axis side-table info from old INDEX_E to new.
      // The rebuilt INDEX_E has substituted buf+addr but the per-axis
      // info (consumer's in_rngs) carries through unchanged at THIS
      // boundary -- consumer doesn't reshape just because we promoted
      // BUFFERIZE to BUFFER.  Without this, the rebuilt INDEX_E loses
      // its side-table entry and any further inline attempts can't
      // find per-axis info.
      if (new_index != resolved) {
        Term old_ax[MAX_DIM];
        u8 old_n = rangeify_unified_index_axes_lookup(
            term_val(resolved), old_ax, MAX_DIM);
        if (old_n > 0) {
          rangeify_unified_index_axes_register(new_index, old_ax, old_n);
        }
      }
      return new_index;
    }
  }

  u8  op  = term_ext(resolved);
  u64 loc = term_val(resolved);

  // Memo is only valid when no subst is active: the memo is keyed on
  // the input Term, and an active subst would cache the substituted
  // result against the unsubstituted key, poisoning later lookups.
  if (sub == NULL) {
    Term hit = 0;
    if (unified_rewrite_memo_lookup(st, resolved, &hit)) return hit;
  }

  // UOP_LOAD is a structural marker (mirrors tinygrad's Ops.LOAD): at
  // the value layer it's an identity around its single src.  The
  // kernel_lift drops it when assembling store_root, so the lifted
  // value tree never wraps an INDEX_E/CONST/etc. inside a LOAD.
  // cpu_uop_walk's value dispatcher has no UOP_LOAD case,
  // so a stray LOAD wrapping an INDEX_E reads as 0 and the kernel zeroes
  // its output.  Strip it here so the rewritten subtree feeds the walker
  // directly.
  if (op == UOP_LOAD) {
    Term inner = unified_rewrite_rec_sub(st, sub, heap_read(loc), depth + 1);
    if (sub == NULL) unified_rewrite_memo_insert(st, resolved, inner);
    return inner;
  }

  // Movement ops (RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP) inside a value
  // subtree.  The unified pass keeps them as structural markers (their
  // dims params describe the producer's iter shape, not a runtime read),
  // but cpu_uop_walk's value dispatcher has no movement-op cases.  At
  // the value layer they're identities around their single src: the
  // surrounding INDEX_E.addr already encodes the linear read, and any
  // RANGE leaves inside use axis_ids that map to ctx slots regardless
  // of the producer's shape annotation.  The unified pass leaves
  // movement ops in place; strip them here so the rewritten subtree
  // is walkable.  Pool-style reshape-MAX gradients reach this when an
  // n_r=1 BUFFERIZE inline produces RESHAPE(INDEX_E(...)) around the
  // LHS of a CMPEQ.
  if (op == UOP_RESHAPE || op == UOP_PERMUTE
      || op == UOP_EXPAND  || op == UOP_PAD
      || op == UOP_SHRINK  || op == UOP_FLIP) {
    Term inner = unified_rewrite_rec_sub(st, sub, heap_read(loc), depth + 1);
    if (sub == NULL) unified_rewrite_memo_insert(st, resolved, inner);
    return inner;
  }

  u8 ar = uop_arity(op);
  if (ar == 0) {
    if (sub == NULL) unified_rewrite_memo_insert(st, resolved, resolved);
    return resolved;
  }
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = unified_rewrite_rec_sub(st, sub, old_child, depth + 1);
    srcs[i] = new_child;
    if (new_child != old_child) changed = 1;
  }
  // INDEX_E(numel-1 UOP_BUFFER, addr): the only legal addr is 0.  Legacy
  // kernel_lift encodes stride-0 broadcasts on a 1-element backing store
  // by consulting ke->input_views[slot].strides; the unified pass
  // builds addrs from the consumer's iter ranges and leaks a multi-axis
  // RANGE/IADD expression through when the consumer iter footprint
  // exceeds the producer's realized numel.  Folding to CONST(0) here
  // keeps the read in-bounds.
  if (op == UOP_INDEX_E
      && term_tag(srcs[0]) == TAG_UOP && term_ext(srcs[0]) == UOP_BUFFER
      && uop_buffer_numel(srcs[0]) == 1) {
    Term czero = uop_const(DT_INT32, 0);
    if (srcs[1] != czero) { srcs[1] = czero; changed = 1; }
  }
  Term out = changed ? uop_graph_rebuild_with_srcs(resolved, srcs) : resolved;
  if (sub == NULL) unified_rewrite_memo_insert(st, resolved, out);
  return out;
}

// === Post-inline broadcast-collapsed REDUCE repair ===
//
// thvm's UOP_REDUCE stores only its axis_ids; cpu_uop_walk's
// uwalk_run_reduce recovers each axis's loop extent by scanning the
// REDUCE body for a UOP_RANGE leaf with that axis_id.  When the body
// reads a producer whose VALUE is invariant over a reduce axis (a
// per-channel mean broadcast over the spatial/window axes -- e.g. the
// detached-mean live adjoint `sum_{H,W}(m[c]*gy)` at N=1, or any
// `bias.expand(...).sum()` grad), the materialize inline collapses the
// producer BUFFERIZE down to its invariant value and the reduce-axis
// RANGE leaves vanish from the body.  uwalk_run_reduce then sees
// cext==0 and bails to the reduce identity (0 for SUM), zeroing the
// adjoint -- the same failure ru_reduce_repair_broadcast_body fixes at
// rangeify time, but that repair runs BEFORE the inline (when the body
// still references the axis through the not-yet-inlined BUFFERIZE addr)
// so it can't fire here.  Mirror tinygrad, where REDUCE carries its
// RANGE srcs explicitly (uop/ops.py + schedule/indexing.py:94) so the
// extent is never lost: re-apply the body-invariant repair AFTER the
// inline.  A SUM reduce of a body invariant over axis a equals
// body * extent(a); MAX/MIN equals body.
//
// The lost extents are recovered from the PRE-inline tree (where the
// reduce-axis RANGE leaves still exist in the BUFFERIZE addrs / movement
// swizzles): build an axis_id -> extent map by walking every RANGE leaf.

#define RU_AXEXT_CAP 256
typedef struct { u32 aid[RU_AXEXT_CAP]; u32 ext[RU_AXEXT_CAP]; u32 n; } AxExtMap;

static void axext_add(AxExtMap *m, u32 aid, u32 ext) {
  if (ext == 0) return;
  for (u32 i = 0; i < m->n; i++) if (m->aid[i] == aid) return;
  if (m->n < RU_AXEXT_CAP) { m->aid[m->n] = aid; m->ext[m->n] = ext; m->n++; }
}

static u32 axext_lookup(AxExtMap const *m, u32 aid) {
  for (u32 i = 0; i < m->n; i++) if (m->aid[i] == aid) return m->ext[i];
  return 0;
}

static void axext_collect_rec(AxExtMap *m, Term t, u8 *visited, u64 cap,
                              u32 depth) {
  if (depth > 256) return;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return;
  // Memoize per node: the map `m` is a SET keyed by axis_id (axext_add is
  // idempotent + dedups), so the result is independent of how many paths
  // reach a node.  A DAG node shared by N parents (residual fan-in,
  // stacked-backward) must be walked ONCE, not once per incoming edge --
  // else exponential in depth (a 2-layer transformer backward never
  // finishes).  Same loc-indexed bitmap as materialize_subst_cached_rec.
  u64 loc = term_val(r);
  if (loc < cap && visited[loc]) return;
  if (loc < cap) visited[loc] = 1;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) {
    axext_add(m, uop_range_axis_id(r), uop_range_extent(r));
    return;
  }
  // Pick up the extents a BUFFERIZE encodes in its closed_ranges (these
  // are the producer's realized output-axis ranges -- the spatial axes a
  // partial-realize keeps that the consumer reduce then contracts).
  if (op == UOP_BUFFERIZE) {
    u32 nr = uop_bufferize_n_ranges(r);
    for (u32 i = 0; i < nr; i++) {
      Term cr = uop_bufferize_range_at(r, i);
      if (cr != 0 && term_tag(cr) == TAG_UOP && term_ext(cr) == UOP_RANGE) {
        axext_add(m, uop_range_axis_id(cr), uop_range_extent(cr));
      }
    }
  }
  if (op == UOP_BUFFER) return;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++)
    axext_collect_rec(m, heap_read(loc + i), visited, cap, depth + 1);
}

// Allocate the loc-indexed visited bitmap (cap = HEAP_NEXT) once, then walk.
// On OOM, fall back to the unmemoized walk (correct, just slow on a shared
// DAG -- the pre-fix behavior).
static void axext_collect(AxExtMap *m, Term t) {
  u64 cap = HEAP_NEXT;
  u8 *visited = (cap > 0) ? (u8 *)calloc(cap, 1) : NULL;
  axext_collect_rec(m, t, visited, visited != NULL ? cap : 0, 0);
  free(visited);
}

// Return 1 if subtree `t` references a UOP_RANGE leaf with axis_id `aid`.
// Does not descend into UOP_BUFFER (opaque) but DOES descend into the
// already-inlined body (no BUFFERIZE survives post-inline on the walker
// path).
static int subtree_uses_axis_mat(Term t, u32 aid, u32 depth) {
  if (depth > 256) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) return uop_range_axis_id(r) == aid;
  if (op == UOP_BUFFER) return 0;
  u8 ar = uop_arity(op);
  u64 loc = term_val(r);
  for (u8 i = 0; i < ar; i++)
    if (subtree_uses_axis_mat(heap_read(loc + i), aid, depth + 1)) return 1;
  return 0;
}

static u32 f32_to_bits(f32 v) { u32 b; memcpy(&b, &v, sizeof b); return b; }

static Term repair_collapsed_reduces_rec(Term t, AxExtMap const *m, u32 depth) {
  if (depth > 256) return t;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return t;
  u8 op = term_ext(r);
  if (op == UOP_BUFFER) return r;
  u8 ar = uop_arity(op);
  u64 loc = term_val(r);
  // Rebuild recursable children first.
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term c  = heap_read(loc + i);
    Term nc = repair_collapsed_reduces_rec(c, m, depth + 1);
    srcs[i] = nc;
    if (nc != c) changed = 1;
  }
  if (op != UOP_REDUCE) {
    return changed ? uop_graph_rebuild_with_srcs(r, srcs) : r;
  }
  // For a REDUCE: detect axes the (rebuilt) body no longer references.
  // SUM contributes a *extent factor per such axis; MAX/MIN drops them
  // (the reduce of a body constant over an axis == body for MAX/MIN).
  u32 n_axes = uop_reduce_n_axes(r);
  u32 kind   = uop_reduce_kind(r);
  Term body  = srcs[0];   // the recursively-repaired body
  u32 kept_axes[MAX_DIM];
  u32 n_kept = 0;
  u64 prod   = 1;
  int any_collapsed = 0;
  int bail = 0;
  for (u32 a = 0; a < n_axes && a < MAX_DIM; a++) {
    u32 ax = uop_reduce_axis(r, a);
    if (subtree_uses_axis_mat(body, ax, 0)) {
      if (n_kept < MAX_DIM) kept_axes[n_kept++] = ax;
    } else {
      u32 ext = axext_lookup(m, ax);
      // No recoverable extent -> bail entirely (keep the original reduce
      // verbatim; we never make things worse than the pre-repair walker).
      if (ext == 0) { bail = 1; break; }
      any_collapsed = 1;
      prod *= (u64)ext;
    }
  }
  if (bail || !any_collapsed) {
    // Body may still have changed (a nested reduce deeper down was
    // repaired); rebuild the reduce shell over the original axes.
    if (!changed) return r;
    u32 all_axes[MAX_DIM];
    for (u32 a = 0; a < n_axes && a < MAX_DIM; a++) all_axes[a] = uop_reduce_axis(r, a);
    return uop_reduce_multi(kind, n_axes, all_axes, body);
  }
  Term new_body = body;
  if (kind == REDUCE_SUM && prod != 1) {
    Term k = uop_const(DT_FP32, f32_to_bits((f32)prod));
    new_body = uop_binary(UOP_MUL, body, k);
  }
  // Rebuild the reduce over only the surviving (still-referenced) axes;
  // if none survive, the reduce degenerates to its (scaled) body.
  if (n_kept == 0) return new_body;
  return uop_reduce_multi(kind, n_kept, kept_axes, new_body);
}

static Term unified_store_root_for_walker(KernelEntry *ke, Term root) {
  if (root == 0 || ke == NULL) return root;
  UnifiedRewriteState st;
  memset(&st, 0, sizeof(st));
  st.ke = ke;
  return unified_rewrite_rec(&st, root, 0);
}

// Walk a (post-rewrite) UOp subtree and return 1 if any UOP_BUFFERIZE
// survives.  Used as the unified-bypass safety gate: cpu_uop_walk's
// INDEX_E handler only resolves UOP_BUFFER leaves, so a residual
// BUFFERIZE means the kernel cannot execute via the bypass.
//
// Visited-set: the terms are hash-consed, so a resolved UOP `r` has a
// stable heap loc `term_val(r) < HEAP_NEXT`.  We key a generation-stamped
// loc-indexed array on that loc -> O(1) seen/mark, unbounded over the
// whole DAG.  A shared `BufferizeScanArena` (one calloc'd `stamp` array of
// size HEAP_NEXT + a monotonic `next_gen`) is opened once at each
// top-level scan entry.  A `BufferizeScanVisited` is a lightweight handle
// holding the arena + its own `gen`.
//
// Several of these walks are PATH-SENSITIVE on a bound axis set (`iter`):
// at every BUFFERIZE / REDUCE descent the bound set changes, so cached
// answers are context-specific and the old code started a fresh visited
// (`iv.n = 0`).  That reset is preserved exactly by `bufferize_scan_fresh`,
// which assigns a brand-new generation -- invalidating every prior mark in
// O(1) (no array clear) without sharing marks across `iter` contexts.  The
// within-context memoization (passing the same handle down the `ar` loop)
// stays O(1) and unbounded, which is what kills the exponential re-walk.
typedef struct {
  u32  *stamp;     // loc-indexed; stamp[loc] == gen iff loc marked in gen
  u32   cap;       // == HEAP_NEXT at open time
  u32   next_gen;  // monotonic; bumped per fresh context
} BufferizeScanArena;

typedef struct {
  BufferizeScanArena *a;
  u32                 gen;
} BufferizeScanVisited;

// Open a top-level scan: allocate the shared stamp array + first gen.
// On OOM, cap=0 so seen() always returns 0 (no memoization, still correct
// -- the walk just re-scans shared subtrees, as the legacy >1024 overflow
// path did, but every scan is small enough at OOM scale).
static void bufferize_scan_open(BufferizeScanVisited *v, BufferizeScanArena *a) {
  u64 cap = HEAP_NEXT;
  a->stamp = (cap > 0) ? (u32 *)calloc(cap, sizeof(u32)) : NULL;
  a->cap   = (a->stamp != NULL) ? (u32)cap : 0;
  a->next_gen = 1;
  v->a   = a;
  v->gen = a->next_gen;
}

static void bufferize_scan_close(BufferizeScanArena *a) {
  free(a->stamp);
  a->stamp = NULL;
  a->cap = 0;
}

// Start a fresh context sharing the parent's arena but with a new gen,
// invalidating all prior marks in O(1).  Replaces every legacy `iv.n = 0`.
static void bufferize_scan_fresh(BufferizeScanVisited *child,
                                 BufferizeScanVisited const *parent) {
  child->a   = parent->a;
  child->gen = ++parent->a->next_gen;
}

static int bufferize_scan_seen(BufferizeScanVisited *v, Term t) {
  BufferizeScanArena *a = v->a;
  u64 loc = term_val(t);
  if (loc >= a->cap) return 0;          // OOM arena or out-of-range: never memoized
  if (a->stamp[loc] == v->gen) return 1;
  a->stamp[loc] = v->gen;
  return 0;
}

static int bufferize_scan_rec(BufferizeScanVisited *v, Term t, u32 depth) {
  if (depth > 256) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  if (bufferize_scan_seen(v, r)) return 0;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_BUFFERIZE) return 1;
  if (op == UOP_KERNEL || op == UOP_BUFFER) return 0;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (bufferize_scan_rec(v, heap_read(loc + i), depth + 1)) return 1;
  }
  return 0;
}

static int uop_subtree_has_residual_bufferize(Term root) {
  if (root == 0) return 0;
  BufferizeScanArena arena;
  BufferizeScanVisited v;
  bufferize_scan_open(&v, &arena);
  int rc = bufferize_scan_rec(&v, root, 0);
  bufferize_scan_close(&arena);
  return rc;
}

// === input_chain_composed bookkeeping for the unified-bypass path ===
//
// Mirror of rangeify.c's input_chain_composed flag-set for slots whose
// tid carries non-trivial layout (prior_views chain or a non-contig
// public view).  The unified pass's apply_movement_op_* swizzler
// embedded the movement chain into INDEX_E.addr over the UNDERLYING
// buffer; visit() separately routed the same movement through
// view_resolve so the slot's tid points at a chained / non-contig
// descriptor.  Without the flag the backend pre-mats via the public
// view (wrong numel) while the kernel reads at the swizzled
// underlying-buffer index -- the addr lands in the wrong region of the
// pre-mat buffer.  Setting input_chain_composed=1 tells the backend to
// skip pre-mat and bind the underlying buffer directly.
//
// Two code paths reach the flag:
//   - ru_rewrite_subtree's TAG_TEN branch sees a chained tid at
//     unified-pass time, composes via ru_compose_view_chain, and sets
//     RU_TID_CHAIN_COMPOSED.  input_slot_dedup reads that flag and
//     mirrors it into ke->input_chain_composed[slot].
//   - The pass below covers the other case: the unified pass saw the
//     pre-chain TAG_TEN (the swizzler ran on a movement op in the
//     kernel DAG) and visit() later minted a chained / non-contig slot
//     tid.  RU_TID_CHAIN_COMPOSED is NOT set for that tid; we scan the
//     rewritten store_root for INDEX_E(UOP_BUFFER slot, addr) cells and
//     flag those slots so the backend skips pre-mat.
typedef struct {
  u32 slot_mask_lo;       // bit i set iff slot i was flagged (slots 0..31)
  u32 slot_mask_hi;       // slot 32..63
} ChainFoldMarks;

static int slot_needs_chain_skip(u32 tid) {
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  return (td->nviews > 0
       || !td->view.contiguous
       || td->view.offset != 0);
}

static void unified_fold_chain_scan(KernelEntry const *ke, Term t,
                                    ChainFoldMarks *marks,
                                    BufferizeScanVisited *v, u32 depth) {
  if (depth > 256) return;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return;
  if (bufferize_scan_seen(v, r)) return;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_KERNEL || op == UOP_BUFFER) return;
  if (op == UOP_INDEX_E && ke->input_tids != NULL) {
    Term buf = term_resolve(heap_read(loc + 0));
    if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFER) {
      u32 inst = uop_buffer_inst_get(buf);
      if (inst > 0 && (u32)(inst - 1) < ke->n_inputs) {
        u32 slot = inst - 1;
        u32 tid  = ke->input_tids[slot];
        int already = (ke->input_chain_composed != NULL
                       && ke->input_chain_composed[slot] != 0);
        if (slot_needs_chain_skip(tid)
            && !already
            && !rangeify_unified_tid_chain_composed(tid)) {
          if (slot < 32) marks->slot_mask_lo |= (1u << slot);
          else if (slot < 64) marks->slot_mask_hi |= (1u << (slot - 32));
        }
      }
    }
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    unified_fold_chain_scan(ke, heap_read(loc + i), marks, v, depth + 1);
  }
}

static Term unified_fold_chain(KernelEntry *ke, Term root, ChainFoldMarks *marks) {
  marks->slot_mask_lo = 0;
  marks->slot_mask_hi = 0;
  if (root == 0 || ke == NULL || ke->input_tids == NULL) return root;
  BufferizeScanArena arena;
  BufferizeScanVisited v;
  bufferize_scan_open(&v, &arena);
  unified_fold_chain_scan(ke, root, marks, &v, 0);
  bufferize_scan_close(&arena);
  return root;
}

static void unified_fold_chain_commit_flags(KernelEntry *ke,
                                             ChainFoldMarks const *marks) {
  if (ke == NULL || ke->input_chain_composed == NULL) return;
  for (u32 slot = 0; slot < ke->n_inputs && slot < 64; slot++) {
    u32 bit = (slot < 32)
              ? ((marks->slot_mask_lo >> slot) & 1u)
              : ((marks->slot_mask_hi >> (slot - 32)) & 1u);
    if (bit) ke->input_chain_composed[slot] = 1;
  }
}

// Per-kernel bypass safety gate: collect every UOP_RANGE axis_id
// that appears in a STORE's addr or as the axis of a UOP_REDUCE
// found anywhere in the value subtree.  Then verify every UOP_RANGE
// leaf in the value subtree has an axis_id in that set.  A "stranded
// range" -- one whose axis_id isn't iterated by the cpu_uop_walk loop
// scaffolding -- silently reads only iter=0 and the kernel produces
// wrong results.  Mirror: cpu_uop_walk only sets up loop slots for
// ranges that appear in the STORE's addr (via uwalk_split_ranges) plus
// the REDUCE axis (pushed in uwalk_run_reduce).  Anything else has no
// home and reads as 0.
#define RANGE_AXIS_CAP 256
typedef struct {
  u32   axes[RANGE_AXIS_CAP];
  u32   n;
} RangeAxisSet;

static int range_axis_has(RangeAxisSet const *s, u32 aid) {
  for (u32 i = 0; i < s->n; i++) if (s->axes[i] == aid) return 1;
  return 0;
}

static void range_axis_add(RangeAxisSet *s, u32 aid) {
  if (range_axis_has(s, aid)) return;
  if (s->n < RANGE_AXIS_CAP) s->axes[s->n++] = aid;
}

static void stranded_range_collect_addr(RangeAxisSet *iter_axes,
                                        BufferizeScanVisited *v, Term t,
                                        u32 depth) {
  if (depth > 256) return;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return;
  if (bufferize_scan_seen(v, r)) return;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_RANGE) {
    u32 aid = (u32)term_val(heap_read(loc + 0));
    range_axis_add(iter_axes, aid);
    return;
  }
  // UOP_BUFFERIZE is a closed cross-realize unit: the stored value has its
  // own closed_ranges scope, never the consumer's iter.  Stop descent (the
  // walker treats it as a memory leaf via uwalk_resolve_buf).
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    stranded_range_collect_addr(iter_axes, v, heap_read(loc + i), depth + 1);
  }
}

static int stranded_range_check_value(RangeAxisSet const *iter_axes,
                                      BufferizeScanVisited *v, Term t,
                                      u32 depth) {
  if (depth > 256) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  if (bufferize_scan_seen(v, r)) return 0;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_RANGE) {
    u32 aid = (u32)term_val(heap_read(loc + 0));
    if (range_axis_has(iter_axes, aid)) return 0;
    // THVM_FUSE_CONV_BWD: a hash-cons-aliased REDUCE / realized-scope axis
    // from a disjoint scope is bound when the fusing conv-bwd product
    // splices into its contraction reduce; mirror the rangeify-side
    // covered-check so it is not re-flagged stranded here.  (No-op when
    // the fuse flag is off -- the fuse-bound set is empty.)
    if (rangeify_unified_aid_is_fuse_bound(aid)) return 0;
    return 1;
  }
  // UOP_BUFFERIZE is opaque: its stored value subtree carries closed_ranges
  // (its own iter scope), not the consumer's.  cpu_uop_walk's INDEX_E case
  // dispatches via uwalk_resolve_buf for a BUFFERIZE leaf (either a kernel
  // input slot or, if unresolved, caught by has_resid before this gate
  // matters) and never iterates the stored-value subtree at this consumer's
  // iter.  Descending here yielded false-positive stranded reports for any
  // unresolved BUFFERIZE residual whose body referenced producer-side
  // RANGE leaves -- entirely unobservable from the walker's perspective.
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return 0;
  if (op == UOP_REDUCE) {
    // Enter every reduce axis into the iterated set for the body walk.
    Term t = term_new(0, TAG_UOP, op, loc);
    u32 n_axes = uop_reduce_n_axes(t);
    RangeAxisSet inner = *iter_axes;
    for (u32 i = 0; i < n_axes; i++) range_axis_add(&inner, uop_reduce_axis(t, i));
    BufferizeScanVisited iv;
    bufferize_scan_fresh(&iv, v);
    return stranded_range_check_value(&inner, &iv, uop_reduce_src(t),
                                      depth + 1);
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (stranded_range_check_value(iter_axes, v, heap_read(loc + i),
                                   depth + 1)) return 1;
  }
  return 0;
}

// Deep strand check: unlike stranded_range_check_value (which stops at
// any BUFFERIZE), this DESCENDS into nested bufferizes that would be
// inlined, accumulating each one's closed_ranges into the bound set
// (the inline substitutes those by the consumer's addr, so they are NOT
// stranded).  A UOP_RANGE that survives -- not a closed_range of any
// inlined bufferize on the path, nor an enclosing REDUCE axis -- is the
// stranding leak.  Used only to DECIDE realization (promote a partial
// bufferize to a boundary); the over-approximation of treating every
// nested bufferize as inlined is safe here (worst case: realize one
// extra buffer that could have inlined).
//
// The recursive "carry an accumulating bound set down each path" form is
// PATH-SENSITIVE on `iter`: the same shared subtree is reached under many
// distinct bound sets (residual fan-in nests BUFFERIZE/REDUCE wrappers),
// so a per-context visited set is reset on every wrapper and the walk goes
// exponential (a 2-layer transformer backward never finishes).
//
// Equivalent path-INSENSITIVE reformulation, memoized once per node:
// `free_aids(node)` = the RANGE axis_ids reachable below `node` that are
// NOT bound by a BUFFERIZE.closed_range or REDUCE.axis encountered strictly
// *inside* the subtree rooted at `node`:
//   RANGE(aid)      -> { aid }
//   KERNEL/BUFFER   -> { }
//   BUFFERIZE       -> free_aids(value)  \  closed_ranges
//   REDUCE          -> free_aids(src)    \  reduce_axes
//   other           -> union over children
// Then a node would strand iff some aid in free_aids(top value) is neither
// in the consumer's bound `iter` set nor fuse-bound.  free_aids depends
// only on the node (not the path), so one loc-keyed cache makes the whole
// scan O(nodes) -- identical answer to the old recursion (each wrapper's
// local set-difference reproduces the old per-path bound accumulation).
typedef struct {
  RangeAxisSet **sets;    // loc-indexed; sets[loc] = node's free-aid set (owned)
  u8            *done;    // loc-indexed; 1 once computed (cache "no free aids" too)
  u32            cap;
} StrandFreeCache;

static RangeAxisSet const *strand_free_aids(StrandFreeCache *c, Term t,
                                            u32 depth, RangeAxisSet *out) {
  // `out` is scratch the caller owns; on a cache hit we return the cached
  // pointer, else we fill `out`, cache a copy, and return the cache entry.
  out->n = 0;
  if (depth > 256) return out;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return out;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (loc < c->cap && c->done[loc]) return c->sets[loc];
  if (op == UOP_RANGE) {
    range_axis_add(out, (u32)term_val(heap_read(loc + 0)));
  } else if (op == UOP_KERNEL || op == UOP_BUFFER) {
    // empty
  } else if (op == UOP_BUFFERIZE) {
    Term val = uop_bufferize_value(r);
    if (val != 0) {
      RangeAxisSet sub;
      RangeAxisSet const *cs = strand_free_aids(c, val, depth + 1, &sub);
      // Subtract this BUFFERIZE's closed_ranges (locally bound on inline).
      RangeAxisSet bound;
      bound.n = 0;
      u32 nr = uop_bufferize_n_ranges(r);
      for (u32 i = 0; i < nr; i++) {
        Term cr = uop_bufferize_range_at(r, i);
        if (term_tag(cr) == TAG_UOP && term_ext(cr) == UOP_RANGE)
          range_axis_add(&bound, (u32)term_val(heap_read(term_val(cr) + 0)));
      }
      for (u32 i = 0; i < cs->n; i++)
        if (!range_axis_has(&bound, cs->axes[i])) range_axis_add(out, cs->axes[i]);
    }
  } else if (op == UOP_REDUCE) {
    Term rt = term_new(0, TAG_UOP, op, loc);
    Term src = uop_reduce_src(rt);
    RangeAxisSet sub;
    RangeAxisSet const *cs = strand_free_aids(c, src, depth + 1, &sub);
    RangeAxisSet bound;
    bound.n = 0;
    u32 n_axes = uop_reduce_n_axes(rt);
    for (u32 i = 0; i < n_axes; i++) range_axis_add(&bound, uop_reduce_axis(rt, i));
    for (u32 i = 0; i < cs->n; i++)
      if (!range_axis_has(&bound, cs->axes[i])) range_axis_add(out, cs->axes[i]);
  } else {
    u8 ar = uop_arity(op);
    for (u8 i = 0; i < ar; i++) {
      RangeAxisSet sub;
      RangeAxisSet const *cs = strand_free_aids(c, heap_read(loc + i),
                                                depth + 1, &sub);
      for (u32 j = 0; j < cs->n; j++) range_axis_add(out, cs->axes[j]);
    }
  }
  if (loc < c->cap && !c->done[loc]) {
    RangeAxisSet *saved = (RangeAxisSet *)malloc(sizeof(RangeAxisSet));
    if (saved != NULL) {
      *saved = *out;
      c->sets[loc] = saved;
      c->done[loc] = 1;
      return saved;
    }
  }
  return out;
}

static int bufferize_strand_check_deep(RangeAxisSet const *iter,
                                       StrandFreeCache *c, Term t) {
  RangeAxisSet scratch;
  RangeAxisSet const *free = strand_free_aids(c, t, 0, &scratch);
  for (u32 i = 0; i < free->n; i++) {
    u32 aid = free->axes[i];
    if (range_axis_has(iter, aid)) continue;
    // THVM_FUSE_CONV_BWD: hash-cons-aliased foreign reduce/realized axis
    // (see stranded_range_check_value) -- bound at splice, not stranded.
    if (rangeify_unified_aid_is_fuse_bound(aid)) continue;
    return 1;
  }
  return 0;
}

// Open / close the loc-keyed free-aid cache used by bufferize_strand_check_deep.
static void strand_free_cache_open(StrandFreeCache *c) {
  u64 cap = HEAP_NEXT;
  c->sets = (cap > 0) ? (RangeAxisSet **)calloc(cap, sizeof(RangeAxisSet *)) : NULL;
  c->done = (cap > 0) ? (u8 *)calloc(cap, 1) : NULL;
  c->cap  = (c->sets != NULL && c->done != NULL) ? (u32)cap : 0;
  if (c->cap == 0) { free(c->sets); free(c->done); c->sets = NULL; c->done = NULL; }
}

static void strand_free_cache_close(StrandFreeCache *c) {
  if (c->sets != NULL)
    for (u32 i = 0; i < c->cap; i++) free(c->sets[i]);
  free(c->sets);
  free(c->done);
  c->sets = NULL;
  c->done = NULL;
  c->cap = 0;
}

// Would INLINING this BUFFERIZE strand a range in its consumer?  If the
// stored value has a free UOP_RANGE leaf that is neither a closed_range
// nor bound by an enclosing REDUCE, the inline leaves it free in the
// consumer (the partial conv _pool reshapes).  Such a bufferize must be
// REALIZED (computed once, read via INDEX_E(BUFFER)) rather than inlined.
// `c` is the caller-owned free-aid cache, shared across the boundary scan
// so each node's free_aids is computed once total.
static int bufferize_value_would_strand_c(Term buf, u32 node_idx,
                                          StrandFreeCache *c) {
  if (term_tag(buf) != TAG_UOP || term_ext(buf) != UOP_BUFFERIZE) return 0;
  Term v = uop_bufferize_value(buf);
  if (v == 0) return 0;
  BufferizeScanArena arena;
  BufferizeScanVisited av;
  bufferize_scan_open(&av, &arena);
  RangeAxisSet iter_axes;
  iter_axes.n = 0;
  // Bound set = the iteration the CONSUMER threads in when this node
  // inlines: every RANGE leaf across the node's full out_rngs (not just
  // its realized closed_ranges).  The `_pool` reshape's value carries the
  // consumer's output-position ranges (oh/ow) free; the consuming conv
  // reduce kernel iterates exactly those, so they are not stranded.  Only
  // a free range OUTSIDE the consumer iteration and not bound by a nested
  // REDUCE is a true strand that forces realization.
  u32 ond = rangeify_unified_out_ndim_at(node_idx);
  for (u32 a = 0; a < ond; a++) {
    Term r = rangeify_unified_out_rng_at(node_idx, a);
    if (r == 0) continue;
    BufferizeScanVisited iv;
    bufferize_scan_fresh(&iv, &av);
    stranded_range_collect_addr(&iter_axes, &iv, r, 0);
  }
  u32 nr = uop_bufferize_n_ranges(buf);
  for (u32 i = 0; i < nr; i++) {
    Term cr = uop_bufferize_range_at(buf, i);
    if (term_tag(cr) == TAG_UOP && term_ext(cr) == UOP_RANGE)
      range_axis_add(&iter_axes, (u32)term_val(heap_read(term_val(cr) + 0)));
  }
  int rc = bufferize_strand_check_deep(&iter_axes, c, v);
  bufferize_scan_close(&arena);
  return rc;
}

// Returns 1 when `store_root` has a UOP_RANGE leaf in its value
// subtree whose axis_id is neither in the STORE's addr nor inside the
// scope of an enclosing UOP_REDUCE.  cpu_uop_walk's loop scaffolding
// has no slot for such a "stranded" range, so it reads iter=0 forever
// and the kernel writes only the slice-0 result.
static int uop_subtree_has_stranded_range(Term store_root) {
  if (store_root == 0) return 0;
  if (term_tag(store_root) != TAG_UOP) return 0;
  if (term_ext(store_root) != UOP_STORE) return 0;
  u64 sloc = term_val(store_root);
  Term s_addr  = heap_read(sloc + 1);
  Term s_value = heap_read(sloc + 2);
  BufferizeScanArena arena;
  BufferizeScanVisited v;
  bufferize_scan_open(&v, &arena);
  RangeAxisSet iter_axes;
  iter_axes.n = 0;
  stranded_range_collect_addr(&iter_axes, &v, s_addr, 0);
  BufferizeScanVisited vv;
  bufferize_scan_fresh(&vv, &v);
  int rc = stranded_range_check_value(&iter_axes, &vv, s_value, 0);
  bufferize_scan_close(&arena);
  return rc;
}

// Safety gate: scan the rewritten subtree for any UOP_INDEX_E reading
// from a UOP_BUFFER input slot whose static numel is smaller than the
// consumer's iter footprint (output STORE numel + any enclosing REDUCE
// extents).  This indicates a stride-0 broadcast view where the
// per-slot input_views[slot].strides would fold the broadcast axis to
// CONST(0).  The unified bypass builds addr expressions from per-axis
// ranges without consulting strides and reads out-of-bounds on the
// 1-element backing store.  Until ru_pass threads view strides through
// INDEX_E address construction, decline the bypass for these inputs.
//
// Compute the static numel of a UOP_BUFFER (product of dims).  Returns
// 0 if t is not a UOP_BUFFER.
static u64 uop_buffer_numel(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  u32 ndim = uop_buffer_ndim(t);
  u64 n = 1;
  for (u32 d = 0; d < ndim; d++) n *= (u64)uop_buffer_dim(t, d);
  return n;
}

// Upper bound on the value an addr expression can produce, evaluated
// statically from its UOP tree.  Returns UINT64_MAX when the expression
// uses an op the estimator doesn't model (in which case the caller must
// treat the read as potentially out-of-bounds).  The estimator's
// purpose is to let `broadcast_input_scan_rec` decide whether an
// INDEX_E read can statically exceed its BUFFER's numel.  If it can't,
// the read is safe even though the consumer's REDUCE-multiplied iter
// footprint exceeds buf_numel -- the addr just doesn't span the full
// iter cube.
#define ADDR_MAX_UNKNOWN ((u64)~0ull)
static u64 addr_max_value(Term t, u32 depth) {
  if (depth > 64) return ADDR_MAX_UNKNOWN;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return ADDR_MAX_UNKNOWN;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_CONST) {
    u64 bits = term_val(heap_read(loc + 0));
    return bits;
  }
  if (op == UOP_RANGE) {
    u32 ext = (u32)term_val(heap_read(loc + 2));
    if (ext == 0) return 0;
    return (u64)(ext - 1);
  }
  if (op == UOP_IADD) {
    u64 a = addr_max_value(heap_read(loc + 0), depth + 1);
    u64 b = addr_max_value(heap_read(loc + 1), depth + 1);
    if (a == ADDR_MAX_UNKNOWN || b == ADDR_MAX_UNKNOWN) return ADDR_MAX_UNKNOWN;
    if (a + b < a) return ADDR_MAX_UNKNOWN;
    return a + b;
  }
  if (op == UOP_IMUL) {
    u64 a = addr_max_value(heap_read(loc + 0), depth + 1);
    u64 b = addr_max_value(heap_read(loc + 1), depth + 1);
    if (a == ADDR_MAX_UNKNOWN || b == ADDR_MAX_UNKNOWN) return ADDR_MAX_UNKNOWN;
    if (a != 0 && b > ADDR_MAX_UNKNOWN / a) return ADDR_MAX_UNKNOWN;
    return a * b;
  }
  if (op == UOP_IDIV) {
    // a / b: max <= max(a) / min(b).  When b is CONST > 0, that's
    // max(a) / b_val.  Otherwise fall back to max(a) (b >= 1).
    Term b_t = term_resolve(heap_read(loc + 1));
    if (term_tag(b_t) == TAG_UOP && term_ext(b_t) == UOP_CONST) {
      u64 bv = term_val(heap_read(term_val(b_t) + 0));
      if (bv > 0) {
        u64 a = addr_max_value(heap_read(loc + 0), depth + 1);
        if (a == ADDR_MAX_UNKNOWN) return ADDR_MAX_UNKNOWN;
        return a / bv;
      }
    }
    return addr_max_value(heap_read(loc + 0), depth + 1);
  }
  if (op == UOP_IMOD) {
    // a % b: max <= b - 1 when b is CONST > 0.
    Term b_t = term_resolve(heap_read(loc + 1));
    if (term_tag(b_t) == TAG_UOP && term_ext(b_t) == UOP_CONST) {
      u64 bv = term_val(heap_read(term_val(b_t) + 0));
      if (bv > 0) return bv - 1;
    }
    return ADDR_MAX_UNKNOWN;
  }
  return ADDR_MAX_UNKNOWN;
}

static int broadcast_input_scan_rec(KernelEntry const *ke,
                                    BufferizeScanVisited *v, Term t,
                                    u64 out_numel,
                                    u32 depth) {
  if (depth > 256) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 0;
  if (bufferize_scan_seen(v, r)) return 0;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  // INDEX_E(BUFFER, addr): the read is safe iff the addr expression's
  // statically-bounded max value is < BUFFER.numel.  A bare
  // numel-vs-iter-footprint check fires false positives on
  //   - INDEX_E inside a REDUCE body whose addr is a bare reduce-axis
  //     RANGE (max addr = extent-1, fully in-bounds of dims=[extent])
  //   - INDEX_E whose addr uses fewer axes than the enclosing iter
  //     cube (broadcast over the missing axes -- addr stays small)
  // When `addr_max_value` returns UINT64_MAX (estimator doesn't model
  // some op in the addr tree), fall back to the conservative
  // numel-vs-footprint check.
  //
  // INDEX_E(TAG_TEN, addr): the unified pass inlines a producer
  // BUFFERIZE's value subtree into the consumer.  When that subtree
  // referenced a TAG_TEN that the consumer's input_tids[] does NOT
  // carry, materialize.c's unified_rewrite_buffer_for_tid returns 0
  // and the TAG_TEN is left in the rewritten tree -- cpu_uop_walk
  // can't bind a runtime buffer for it and the kernel reads garbage.
  // Decline so the bypass substitution skips this kernel.
  if (op == UOP_INDEX_E) {
    Term buf_t = term_resolve(heap_read(loc + 0));
    if (term_tag(buf_t) == TAG_TEN) {
      return 1;
    }
    if (term_tag(buf_t) == TAG_UOP && term_ext(buf_t) == UOP_BUFFER) {
      u32 inst = uop_buffer_inst_get(buf_t);
      // Output buf (inst=0) is the writer's own slot; skip the size check.
      if (inst >= 1) {
        u64 buf_numel = uop_buffer_numel(buf_t);
        Term addr_t = term_resolve(heap_read(loc + 1));
        u64 max_addr = addr_max_value(addr_t, 0);
        if (max_addr != ADDR_MAX_UNKNOWN) {
          if (buf_numel > 0 && max_addr >= buf_numel) {
            return 1;
          }
          // addr is bounded < buf_numel -> safe regardless of out_numel.
        } else {
          int addr_is_zero = (term_tag(addr_t) == TAG_UOP
                           && term_ext(addr_t) == UOP_CONST
                           && term_val(heap_read(term_val(addr_t) + 0)) == 0);
          if (!addr_is_zero
              && buf_numel > 0 && out_numel > 0 && buf_numel < out_numel) {
            return 1;
          }
        }
      }
    }
  }
  if (op == UOP_KERNEL || op == UOP_BUFFERIZE) return 0;
  // IWHERE(cond, then, else) where one branch is UOP_INVALID is the
  // PAD/OOB-mask pattern (conv2d-grad uses it to guard windowed reads
  // against shifted-window OOB).  The walker only evaluates the picked
  // branch at runtime, so the surviving INDEX_E reads are runtime-
  // guarded by the cond.  Scan the cond (no buffer reads there) but
  // skip address-bound checks inside the guarded branch -- the static
  // addr_max_value can't see the IWHERE guard and would conservatively
  // flag the read as OOB on the iter cube where cond=0.  The other
  // INVALID-paired branch is skipped entirely (returns 0 at runtime).
  if (op == UOP_IWHERE) {
    Term then_t = term_resolve(heap_read(loc + 1));
    Term else_t = term_resolve(heap_read(loc + 2));
    int then_invalid = (term_tag(then_t) == TAG_UOP
                     && term_ext(then_t) == UOP_INVALID);
    int else_invalid = (term_tag(else_t) == TAG_UOP
                     && term_ext(else_t) == UOP_INVALID);
    if (then_invalid || else_invalid) {
      // Scan the cond (no INDEX_E in here for conv2d grads, but stay
      // strict for the rare case where a cond pulls from a kernel
      // input).  Skip the guarded branch -- runtime cond keeps its
      // reads in-bounds; only the non-INVALID branch needs scanning.
      if (broadcast_input_scan_rec(ke, v, heap_read(loc + 0),
                                   out_numel, depth + 1)) {
        return 1;
      }
      return 0;
    }
  }
  // Multiply REDUCE extent (every reduce axis) into out_numel when
  // descending into a reduce body so the broadcast check counts the full
  // iteration footprint.  Multi-axis REDUCE folds N axes simultaneously
  // (mirrors tinygrad's REDUCE.src[1:] range list).
  if (op == UOP_REDUCE) {
    Term t = term_new(0, TAG_UOP, op, loc);
    u64 inner_out = out_numel;
    Term body = uop_reduce_src(t);
    u32 n_axes = uop_reduce_n_axes(t);
    // Find each reduce range's extent by scanning the body for UOP_RANGE
    // with each axis_id, then multiply all extents into the footprint.
    for (u32 ai = 0; ai < n_axes; ai++) {
      u32 r_aid = uop_reduce_axis(t, ai);
      u32 r_ext = 0;
      Term stack[64];
      u32  top = 0;
      stack[top++] = body;
      while (top > 0 && r_ext == 0) {
        Term cur = term_resolve(stack[--top]);
        if (term_tag(cur) != TAG_UOP) continue;
        u8  cop = term_ext(cur);
        u64 cloc = term_val(cur);
        if (cop == UOP_RANGE) {
          u32 aid = (u32)term_val(heap_read(cloc + 0));
          if (aid == r_aid) {
            r_ext = (u32)term_val(heap_read(cloc + 2));
            break;
          }
          continue;
        }
        if (cop == UOP_BUFFER || cop == UOP_BUFFERIZE || cop == UOP_KERNEL) continue;
        u8 car = uop_arity(cop);
        for (u8 i = 0; i < car && top < 64; i++) {
          stack[top++] = heap_read(cloc + i);
        }
      }
      if (r_ext > 0) inner_out *= (u64)r_ext;
    }
    return broadcast_input_scan_rec(ke, v, body, inner_out, depth + 1);
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (broadcast_input_scan_rec(ke, v, heap_read(loc + i),
                                 out_numel, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

static int uop_subtree_has_broadcast_input(KernelEntry const *ke,
                                            Term root) {
  if (root == 0 || ke == NULL) return 0;
  // root is a UOP_STORE; compute its iter footprint from the output BUFFER.
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  u64 sloc = term_val(root);
  Term out_buf = heap_read(sloc + 0);
  Term s_value = heap_read(sloc + 2);
  u64 out_numel = uop_buffer_numel(out_buf);
  if (out_numel == 0) return 0;
  BufferizeScanArena arena;
  BufferizeScanVisited v;
  bufferize_scan_open(&v, &arena);
  int rc = broadcast_input_scan_rec(ke, &v, s_value, out_numel, 0);
  bufferize_scan_close(&arena);
  return rc;
}

// === Debug dumper for THVM_DEBUG_BYPASS_LAST=1 ===
// Pretty-prints a UOp subtree with indent + opcode name + key fields.
// Used to bisect bypass-substitution divergences when a gate trips.
static char const *bypass_dbg_op_name(u8 op) {
  switch (op) {
    case UOP_CONST:       return "CONST";
    case UOP_RESHAPE:     return "RESHAPE";
    case UOP_PERMUTE:     return "PERMUTE";
    case UOP_EXPAND:      return "EXPAND";
    case UOP_PAD:         return "PAD";
    case UOP_SHRINK:      return "SHRINK";
    case UOP_FLIP:        return "FLIP";
    case UOP_ADD:         return "ADD";
    case UOP_MUL:         return "MUL";
    case UOP_NEG:         return "NEG";
    case UOP_RECIP:       return "RECIP";
    case UOP_EXP2:        return "EXP2";
    case UOP_LOG2:        return "LOG2";
    case UOP_SQRT:        return "SQRT";
    case UOP_CMPLT:       return "CMPLT";
    case UOP_CMPEQ:       return "CMPEQ";
    case UOP_REDUCE:      return "REDUCE";
    case UOP_LOAD:        return "LOAD";
    case UOP_ASSIGN:      return "ASSIGN";
    case UOP_CAST:        return "CAST";
    case UOP_BITCAST:     return "BITCAST";
    case UOP_RANGE:       return "RANGE";
    case UOP_INDEX_E:     return "INDEX_E";
    case UOP_IADD:        return "IADD";
    case UOP_IMUL:        return "IMUL";
    case UOP_ILT:         return "ILT";
    case UOP_IWHERE:      return "IWHERE";
    case UOP_INVALID:     return "INVALID";
    case UOP_BUFFER:      return "BUFFER";
    case UOP_STORE:       return "STORE";
    case UOP_BUFFERIZE:   return "BUFFERIZE";
    case UOP_DETACH:      return "DETACH";
    default:              return "?";
  }
}

// Debug-only: product of the extents of every LOOP-typed RANGE leaf in
// `t` that is NOT in `bound` (the STORE addr axes + any enclosing
// REDUCE axes).  A stranded reduce window rendered as outer LOOP loops
// shows up here as a huge product (the conv-bwd 24x24 window strand).
static u64 strand_loop_product_rec(RangeAxisSet const *bound,
                                   BufferizeScanVisited *v, Term t,
                                   u32 depth) {
  if (depth > 256) return 1;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return 1;
  if (bufferize_scan_seen(v, r)) return 1;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_RANGE) {
    u32 aid   = (u32)term_val(heap_read(loc + 0));
    u32 atype = (u32)term_val(heap_read(loc + 1));
    u32 ext   = (u32)term_val(heap_read(loc + 2));
    if (atype == 0 /*LOOP*/ && !range_axis_has(bound, aid) && ext > 1)
      return (u64)ext;
    return 1;
  }
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return 1;
  if (op == UOP_REDUCE) {
    RangeAxisSet inner = *bound;
    u32 n_axes = uop_reduce_n_axes(r);
    for (u32 i = 0; i < n_axes; i++) range_axis_add(&inner, uop_reduce_axis(r, i));
    BufferizeScanVisited iv;
    bufferize_scan_fresh(&iv, v);
    return strand_loop_product_rec(&inner, &iv, uop_reduce_src(r), depth + 1);
  }
  u64 prod = 1;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    u64 c = strand_loop_product_rec(bound, v, heap_read(loc + i), depth + 1);
    if (c > 1 && prod > (u64)~0ull / c) return (u64)~0ull;
    prod *= c;
  }
  return prod;
}

static u64 strand_loop_product(Term store_root) {
  if (store_root == 0 || term_tag(store_root) != TAG_UOP
      || term_ext(store_root) != UOP_STORE) return 0;
  u64 sloc = term_val(store_root);
  Term s_addr  = heap_read(sloc + 1);
  Term s_value = heap_read(sloc + 2);
  BufferizeScanArena arena;
  BufferizeScanVisited av;
  bufferize_scan_open(&av, &arena);
  RangeAxisSet bound;
  bound.n = 0;
  stranded_range_collect_addr(&bound, &av, s_addr, 0);
  BufferizeScanVisited vv;
  bufferize_scan_fresh(&vv, &av);
  u64 prod = strand_loop_product_rec(&bound, &vv, s_value, 0);
  bufferize_scan_close(&arena);
  return prod;
}

// Debug-only: total iteration product of every DISTINCT RANGE leaf (LOOP
// and REDUCE) in the store's value subtree, regardless of addr-binding.
// A correct-but-infeasible reduce (the conv data-grad's 144-padded window
// absorbed into the reduce -> ~6.8e12) surfaces here even though its
// stranded-LOOP product is 1.  Distinct by axis_id so a leaf referenced
// in both legs of a MUL isn't double-counted.
static void total_iter_collect_rec(BufferizeScanVisited *seen,
                                   u32 *aids, u32 *exts, u32 *n, u32 cap,
                                   Term t, u32 depth) {
  if (depth > 256 || *n >= cap) return;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return;
  if (bufferize_scan_seen(seen, r)) return;
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  if (op == UOP_RANGE) {
    u32 aid = (u32)term_val(heap_read(loc + 0));
    u32 ext = (u32)term_val(heap_read(loc + 2));
    for (u32 i = 0; i < *n; i++) if (aids[i] == aid) return;
    if (ext > 1) { aids[*n] = aid; exts[*n] = ext; (*n)++; }
    return;
  }
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++)
    total_iter_collect_rec(seen, aids, exts, n, cap, heap_read(loc + i),
                           depth + 1);
}

static u64 total_iter_product(Term store_root) {
  if (store_root == 0 || term_tag(store_root) != TAG_UOP
      || term_ext(store_root) != UOP_STORE) return 0;
  u64 sloc = term_val(store_root);
  Term s_value = heap_read(sloc + 2);
  u32 aids[64], exts[64], n = 0;
  BufferizeScanArena arena;
  BufferizeScanVisited seen;
  bufferize_scan_open(&seen, &arena);
  total_iter_collect_rec(&seen, aids, exts, &n, 64, s_value, 0);
  bufferize_scan_close(&arena);
  u64 prod = 1;
  for (u32 i = 0; i < n; i++) {
    if (prod > (u64)~0ull / exts[i]) return (u64)~0ull;
    prod *= exts[i];
  }
  return prod;
}

static void bypass_dbg_dump_rec(Term t, u32 indent, u32 depth) {
  if (depth > 40) {
    for (u32 i = 0; i < indent; i++) fputc(' ', stderr);
    fputs("...max-depth...\n", stderr);
    return;
  }
  Term r = term_resolve(t);
  for (u32 i = 0; i < indent; i++) fputc(' ', stderr);
  u8  tag = term_tag(r);
  if (tag == TAG_TEN) {
    fprintf(stderr, "TEN tid=%u\n", (u32)term_val(r));
    return;
  }
  if (tag == TAG_NUM) {
    fprintf(stderr, "NUM %llu\n", (unsigned long long)term_val(r));
    return;
  }
  if (tag != TAG_UOP) {
    fprintf(stderr, "TAG=%u val=%llx\n", tag, (unsigned long long)term_val(r));
    return;
  }
  u8  op  = term_ext(r);
  u64 loc = term_val(r);
  char const *name = bypass_dbg_op_name(op);
  if (op == UOP_CONST) {
    u64 bits = term_val(heap_read(loc + 0));
    union { u64 u; f64 f; } u;
    u.u = bits;
    f32 f = 0.0f;
    memcpy(&f, &bits, sizeof(f));
    fprintf(stderr, "CONST dtype=%u bits=%llx f32=%g\n",
            term_ext(r), (unsigned long long)bits, (double)f);
    return;
  }
  if (op == UOP_RANGE) {
    u32 aid    = (u32)term_val(heap_read(loc + 0));
    u32 atype  = (u32)term_val(heap_read(loc + 1));
    u32 extent = (u32)term_val(heap_read(loc + 2));
    fprintf(stderr, "RANGE aid=%u type=%u extent=%u\n", aid, atype, extent);
    return;
  }
  if (op == UOP_BUFFER) {
    u32 scope = (u32)term_val(heap_read(loc + 0));
    u32 dtype = (u32)term_val(heap_read(loc + 1));
    u32 ndim  = (u32)term_val(heap_read(loc + 2));
    u32 inst  = uop_buffer_inst_get(r);
    fprintf(stderr, "BUFFER scope=%u dtype=%u ndim=%u inst=%u dims=[",
            scope, dtype, ndim, inst);
    for (u32 d = 0; d < ndim && d < 8; d++) {
      fprintf(stderr, "%s%u", d ? "," : "", uop_buffer_dim(r, d));
    }
    fputs("]\n", stderr);
    return;
  }
  if (op == UOP_BUFFERIZE) {
    // Print closed_ranges as identity-only summary (axis_id/type/ext)
    // so the bypass-bisect log captures the producer's row-major
    // layout context that uop_arity(BUFFERIZE)==1 would otherwise
    // drop.  Then recurse into the value subtree for the body.
    u32 n_r = uop_bufferize_n_ranges(r);
    fprintf(stderr, "BUFFERIZE n_r=%u closed=[", n_r);
    for (u32 i = 0; i < n_r && i < 8; i++) {
      Term cr = uop_bufferize_range_at(r, i);
      if (term_tag(cr) == TAG_UOP && term_ext(cr) == UOP_RANGE) {
        u32 aid = (u32)term_val(heap_read(term_val(cr) + 0));
        u32 atp = (u32)term_val(heap_read(term_val(cr) + 1));
        u32 ext = (u32)term_val(heap_read(term_val(cr) + 2));
        fprintf(stderr, "%said=%u/t=%u/ext=%u", i ? "," : "", aid, atp, ext);
      } else {
        fprintf(stderr, "%s?", i ? "," : "");
      }
    }
    fputs("]\n", stderr);
    Term v = uop_bufferize_value(r);
    if (v != 0) bypass_dbg_dump_rec(v, indent + 2, depth + 1);
    return;
  }
  if (op == UOP_REDUCE) {
    u32 kind = uop_reduce_kind(r);
    u32 n_axes = uop_reduce_n_axes(r);
    fprintf(stderr, "REDUCE kind=%u axes=[", kind);
    for (u32 i = 0; i < n_axes; i++) {
      fprintf(stderr, "%s%u", i ? "," : "", uop_reduce_axis(r, i));
    }
    fputs("]\n", stderr);
    bypass_dbg_dump_rec(uop_reduce_src(r), indent + 2, depth + 1);
    return;
  }
  fprintf(stderr, "%s (ext=%u, loc=%llx)\n", name, term_ext(r),
          (unsigned long long)loc);
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    bypass_dbg_dump_rec(heap_read(loc + i), indent + 2, depth + 1);
  }
}

static void bypass_dbg_dump(char const *label, u32 kid, Term root) {
  fprintf(stderr, "=== BYPASS_DBG kid=%u %s root=%llx ===\n",
          kid, label, (unsigned long long)root);
  if (root == 0) {
    fputs("<null>\n", stderr);
    return;
  }
  bypass_dbg_dump_rec(root, 0, 0);
}

// A `realized_full` view boundary whose closed_range count is below its
// out_ndim (a unit axis collapsed to CONST(0)) carries its only nonzero
// VIEW offset in its consumer's INDEX_E addr.  When such a node is INLINED
// (the effectively-full `nr == ond` gate fails because the unit axis isn't
// a RANGE), the inline re-decode drops a consumer SHRINK's begin offset on
// the collapsed axis -- the GPT2 head-split `qkv[:,:,i].transpose @ ...`
// reads slice i but the inlined addr re-decodes to slice 0 for every i.
// Returns 1 iff a DIRECT consumer of `producer_loc` is a UOP_SHRINK with a
// non-zero begin on some axis: that consumer needs the producer realized so
// it can INDEX_E the materialized buffer at its own offset (mirrors
// tinygrad/schedule/indexing.py:63 -- a realize_map node is always a
// boundary its consumers `.index()` into, never re-inlined).  A keepdim
// reduce / pure broadcast (softmax denom, layer-norm mean) has no such
// offset-bearing SHRINK consumer, so it stays inlined and its fusion is
// preserved.
static int boundary_has_offset_shrink_consumer(u64 producer_loc) {
  u64 cons[8];
  u32 nc = bufferize_consumers_for_loc(producer_loc, cons, 8);
  if (nc > 8) nc = 8;
  for (u32 c = 0; c < nc; c++) {
    u32 cidx = bufferize_info_find(cons[c]);
    if (cidx == 0xFFFFFFFFu || BUFFERIZE_NODES[cidx].op != UOP_SHRINK) continue;
    u64 cloc = cons[c];
    u32 ndim = (u32)term_val(heap_read(cloc + 1));
    if (ndim > MAX_DIM) ndim = MAX_DIM;
    for (u32 a = 0; a < ndim; a++) {
      if ((u32)term_val(heap_read(cloc + 2 + 2 * a)) != 0) return 1;  // nonzero begin
    }
  }
  return 0;
}

// 1 iff some out-axis of node `i` is a collapsed unit axis: its rangeify
// out_rng is NOT a UOP_RANGE (it became CONST(0) because the dim is size 1 --
// ru_new_range, rangeify_unified.c:236).  Such a node has n_closed_ranges <
// out_ndim purely from the collapse, so the `nr == ond` effectively-full gate
// spuriously fails even though the node is a genuine full realize.
static int boundary_has_collapsed_unit_axis(u32 node_idx) {
  u32 ond = rangeify_unified_out_ndim_at(node_idx);
  for (u32 a = 0; a < ond; a++) {
    Term r = rangeify_unified_out_rng_at(node_idx, a);
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 1;
  }
  return 0;
}

// Forward-walk the consumer chain from `producer_loc` through movement /
// elementwise UOPs and return 1 iff it reaches a UOP_REDUCE, reporting that
// reduce's loc via out_reduce_loc.  Such a reduce READS this producer's value
// as (part of) its reduce body; if the producer is inlined rather than
// realized, the reduce RE-COMPUTES it per output element.  Mirrors tinygrad's
// `buffer_in_reduce` rule (schedule/rangeify.py:276-285): a bufferize whose
// value chain feeds a reduce that reaches a buffer/param is NOT removed.
static int boundary_feeds_reduce_consumer(u64 producer_loc, u32 depth,
                                          u64 *out_reduce_loc) {
  if (depth > 32) return 0;
  u64 cons[8];
  u32 nc = bufferize_consumers_for_loc(producer_loc, cons, 8);
  if (nc > 8) nc = 8;
  for (u32 c = 0; c < nc; c++) {
    u32 cidx = bufferize_info_find(cons[c]);
    if (cidx == 0xFFFFFFFFu) continue;
    u8 cop = BUFFERIZE_NODES[cidx].op;
    if (cop == UOP_REDUCE) { if (out_reduce_loc) *out_reduce_loc = cons[c]; return 1; }
    // Recurse through layout-preserving / elementwise ops only (movement
    // reshape/permute/expand/pad/shrink/flip, or add/mul/where/...) whose
    // result is still inlined into the same reduce body.  A KERNEL ends the
    // walk (it is its own realized boundary).
    if (cop == UOP_KERNEL) continue;
    if (boundary_feeds_reduce_consumer(cons[c], depth + 1, out_reduce_loc)) return 1;
  }
  return 0;
}

// 1 iff the consuming reduce carries the 2-D conv WEIGHT-GRADIENT fingerprint
// against a B==1 (collapsed-batch) activation:
//   (a) its BODY rank exceeds the producer node's out rank by >= 3 -- the
//       _pool UNFOLD of a 2-D conv adds the full window (C_in, kH, kW = 3
//       axes), so e.g. a [1,C,H,W] (rank 4) activation feeds a rank-7 body
//       [1,2,6,6,3,3,3]; and
//   (b) the body still carries a size-1 axis (the genuine collapsed batch).
// This isolates the d/dw2 conv weight-grad reduce (REALIZE) from every other
// collapsed-unit-axis-feeds-reduce shape at B==1 (all of which must INLINE):
//   - The conv weight-grad reduce sums grad_out against the _pool UNFOLD of
//     the activation (an OVERLAPPING-window read), so inlining the activation
//     re-computes it at every window offset; at B==1 the collapsed CONST(0)
//     batch is baked into that recompute -> mis-reduce (~2x).  Realizing the
//     activation makes the reduce INDEX_E one materialized buffer.
//   - A dense matmul adds exactly ONE contraction axis (rank growth 1) and a
//     multi-head attention reduce (gpt2 q@k, scores@v) adds at most TWO (head
//     + contraction), so both fail the >= 3 test and stay inlined -- no
//     head-split / decode regression.
//   - A statistics reduce (softmax denom, layer-norm / batch-norm mean+var and
//     their backward) reduces WITHIN the activation's axes (no rank growth),
//     also inlined -- preserving fusion and the BN-backward zero cancellation.
// The 3-axis unfold is intrinsic to a 2-D conv regardless of kernel size; a
// 1x1 conv (rank growth 1, no overlap) behaves like a matmul and correctly
// stays inlined.  Matches tinygrad: a size-1 RANGE collapses to CONST(0)
// (indexing.py:53); the conv unfold's 3 extra window ranges are the growth.
static int reduce_is_conv_weight_grad(u64 reduce_loc, u32 node_ndim) {
  if (reduce_loc == 0) return 0;
  Term rterm = term_new(0, TAG_UOP, UOP_REDUCE, reduce_loc);
  Term body = uop_reduce_src(rterm);
  Shape bsh = {0};
  if (!term_shape_in(body, 0, &bsh)) return 0;
  if (bsh.ndim < node_ndim + 3) return 0;       // not a 2-D conv unfold (C_in,kH,kW)
  for (u32 d = 0; d < bsh.ndim; d++)
    if (bsh.dims[d] == 1) return 1;              // genuine size-1 (collapsed) axis threaded in
  return 0;
}

static void topo_sort_boundaries(Term root) {
  BOUNDARY_ORDER_LEN = 0;
  boundary_hash_clear();
  for (u32 i = 0; i < BOUNDARY_ORDER_CAP; i++) BOUNDARY_BUFFERIZE_TERM[i] = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++)
    BOUNDARY_DEPTH[i] = BOUNDARY_DEPTH_INVALID;
  boundary_depth_rec(term_val(root));
  boundary_compute_last_use();
  // Fire-order sequence: the arena lifetime planner recycles a buffer's
  // slot once its boundary-order position passes, so that position MUST
  // be the actual kernel dispatch order (a post-order DFS from the
  // sink), not the (depth, loc) topo sort -- else a producer that fires
  // late (after a sibling that recycled its slot) clobbers still-live
  // data.  Sorting by this sequence makes BOUNDARY_ORDER == execution
  // order, matching tinygrad (schedule/memory.py:28 plans lifetimes over
  // the linearized schedule, which IS the execution order).
  boundary_compute_fire_seq(root);

  struct { u64 loc; u32 depth; u32 seq; Term buf; } items[BOUNDARY_ORDER_CAP];
  u32 n = 0;
  // One free-aid cache for the whole boundary scan: free_aids(node) is
  // consumer-independent, so a node shared across many candidates' deep
  // checks is computed once total (loc-keyed), making the loop O(nodes).
  StrandFreeCache strand_cache;
  strand_free_cache_open(&strand_cache);
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN && n < BOUNDARY_ORDER_CAP; i++) {
    // Select on the unified pass's UOP_BUFFERIZE Term.  Skip nodes
    // the unified rewrite didn't surface; those are either non-
    // boundary intermediates (consumer-divergence walk inlined
    // them) or movement-only nodes whose substitute forwards to a
    // producer's BUFFERIZE.  Either way, no separate kernel emit.
    //
    // Intersect with BUFFERIZE_NODES.realized so the
    // inline-softmax-broadcast-reduce prune still trims the realize
    // set.  Without this, we emit a kernel per UOP_BUFFERIZE
    // including those the prune dropped, inflating count and breaking
    // softmax / attention nn.wlt tests that depend on the pruned-set
    // fusion.
    Term buf = rangeify_unified_bufferize_at(i);
    if (buf == 0) continue;
    // A node escapes into its own kernel when it's a full realize, OR
    // when it's a partial-realize (LOCAL via the unified pass) that must
    // be READ rather than inlined:
    //   - effectively-full: all of the node's axes are closed_ranges
    //     (n_ranges == out_ndim).  Such a "partial" is a full realize
    //     mis-tagged LOCAL by the consumer-divergence walk; inlining it
    //     re-derives the producer (the conv2 backward's inter-layer
    //     activation h1 = relu(conv1)) and lets the bypass value-match
    //     drag in the realized _pool EXPAND's window ranges -> strand.
    //   - would-strand: its inlined value carries a free RANGE not
    //     covered by its closed_ranges (the partial _pool reshapes).
    // Partial realizes that genuinely drop axes and inline cleanly
    // (softmax denom / layer-norm mean broadcasts) are NOT promoted, so
    // their fusion is preserved.
    // THVM_RU_FAITHFUL_SEED: this boundary gate must agree with the
    // rangeify realize-map seed (rangeify_unified.c).  In faithful mode the
    // CLASSIFY realized bit (MULTI/REDUCE/MATMUL heuristics) is NOT a
    // boundary by itself -- only ROOT (== tinygrad STORE) is.  Every other
    // node falls through to the rangeify-realized gate below, so a node the
    // unified walk fused (single-consumer inherit, no ending-ranges) is
    // inlined into its consumer instead of escaping into its own kernel
    // (the conv-backward 6-D MUL was emitted as a 327M-element kernel
    // otherwise).  The effectively-full / would-strand checks still apply,
    // so a genuinely stranding inline (the _pool col2im) is still realized.
    int classify_real = BUFFERIZE_NODES[i].realized
                     && ru_seed_boundary_holds(BUFFERIZE_NODES[i].reasons);
    if (!classify_real) {
      u32 nr  = uop_bufferize_n_ranges(buf);
      u32 ond = rangeify_unified_out_ndim_at(i);
      // effectively-full: all axes closed (nr == ond), OR a realize whose
      // unit axis collapsed the closed-range count below out_ndim (nr < ond):
      //   1. a full realize read by a consumer SHRINK with a non-zero begin
      //      (the GPT2 head-split view offset) -- realize so the consumer
      //      indexes the buffer at its own offset rather than inlining and
      //      re-decoding the addr (which drops the begin).  See
      //      boundary_has_offset_shrink_consumer.
      //   2. a collapsed-unit-axis activation feeding a conv WEIGHT-GRADIENT
      //      reduce (reduce_is_conv_weight_grad) -- realize so the reduce
      //      INDEX_Es the materialized buffer instead of re-computing the
      //      activation at every overlapping conv-window offset.  At B==1 the
      //      collapsed batch is baked into that recompute and mis-reduces the
      //      d/dw2 weight gradient (~2x).  Mirrors tinygrad's buffer_in_reduce
      //      (schedule/rangeify.py:276-285).  The >= 3 unfold-rank + size-1
      //      gates keep dense matmul / attention / statistics reduces inlined.
      u64 rloc = 0;
      int effectively_full = (ond > 0 && nr == ond)
                          || (rangeify_unified_realized_full_at(i)
                              && boundary_has_offset_shrink_consumer(
                                   BUFFERIZE_NODES[i].loc))
                          || (nr > 0 && nr < ond
                              && boundary_has_collapsed_unit_axis(i)
                              && boundary_feeds_reduce_consumer(
                                   BUFFERIZE_NODES[i].loc, 0, &rloc)
                              && reduce_is_conv_weight_grad(rloc, ond));
      if (!rangeify_unified_is_realized(i)) continue;
      if (!effectively_full
          && !bufferize_value_would_strand_c(buf, i, &strand_cache)) continue;
    }
    items[n].loc   = BUFFERIZE_NODES[i].loc;
    items[n].depth = BOUNDARY_DEPTH[i];
    items[n].seq   = BOUNDARY_FIRE_SEQ[i];
    items[n].buf   = buf;
    n++;
  }
  strand_free_cache_close(&strand_cache);
  // Sort by fire sequence (execution order).  Boundaries the DFS didn't
  // reach (orphan preserved tensors) keep seq==0xFFFFFFFF and sort last
  // by their (depth, loc) tiebreak so they stay deterministic.
  for (u32 i = 1; i < n; i++) {
    for (u32 j = i; j > 0; j--) {
      u8 swap;
      if (items[j].seq != items[j-1].seq) {
        swap = (items[j].seq < items[j-1].seq);
      } else if (items[j].depth != items[j-1].depth) {
        swap = (items[j].depth < items[j-1].depth);
      } else {
        swap = (items[j].loc < items[j-1].loc);
      }
      if (!swap) break;
      u64  lt = items[j].loc;   items[j].loc   = items[j-1].loc;   items[j-1].loc   = lt;
      u32  dt = items[j].depth; items[j].depth = items[j-1].depth; items[j-1].depth = dt;
      u32  st = items[j].seq;   items[j].seq   = items[j-1].seq;   items[j-1].seq   = st;
      Term bt = items[j].buf;   items[j].buf   = items[j-1].buf;   items[j-1].buf   = bt;
    }
  }
  for (u32 i = 0; i < n; i++) {
    u32 idx = BOUNDARY_ORDER_LEN++;
    BOUNDARY_ORDER[idx] = items[i].loc;
    BOUNDARY_BUFFERIZE_TERM[idx] = items[i].buf;
    boundary_hash_insert(items[i].loc, idx);
    {
      char const *e = getenv("THVM_DUMP_BOUNDARY_ORDER");
      if (e != NULL && e[0] == '1') {
        u32 binfo = bufferize_info_find(items[i].loc);
        u8 op = (binfo != 0xFFFFFFFFu) ? BUFFERIZE_NODES[binfo].op : 0xFF;
        u32 reasons = (binfo != 0xFFFFFFFFu) ? BUFFERIZE_NODES[binfo].reasons : 0;
        fprintf(stderr,
                "boundary-order: idx=%u loc=%llu op=%u reasons=0x%x buf=%llu\n",
                idx, (unsigned long long)items[i].loc,
                (unsigned)op, reasons,
                (unsigned long long)items[i].buf);
        if (getenv("THVM_DUMP_STORE_TREE"))
          bypass_dbg_dump("boundary-value", idx, items[i].buf);
      }
    }
  }
  // Fire-order consumer lifetimes for the arena planner.  Runs here
  // (not in topo_sort_boundaries' depth phase) because it needs the
  // finalized BOUNDARY_ORDER + loc->pos hash.
  boundary_compute_last_use_pos();
  {
    char const *e = getenv("THVM_DUMP_DIRECT_COUNT");
    if (e != NULL && e[0] == '1') {
      u32 realized_n = 0;
      u32 unified_n  = 0;
      for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
        if (BUFFERIZE_NODES[i].realized) realized_n++;
        if (rangeify_unified_bufferize_at(i) != 0) unified_n++;
      }
      fprintf(stderr,
              "direct-count: realized=%u unified_buf=%u ordered=%u\n",
              realized_n, unified_n, BOUNDARY_ORDER_LEN);
    }
  }
}

// Unified-rangeify boundary walker.  Runs topo_sort_boundaries (which
// keys off BUFFERIZE_NODES.realized -- the canonical post-
// bufferize_classify realize set) and captures the UOP_BUFFERIZE Term
// the unified rangeify pass emitted at each boundary into
// BOUNDARY_BUFFERIZE_TERM[].  Mirror: tinygrad's scheduler walks
// BUFFERIZE+STORE pairs in the lowered tsink
// (tinygrad/engine/realize.py); thvm reuses the boundary-order topo
// and attaches the unified-pass Term per slot for KernelEntry wiring.
//
// Does NOT mutate BUFFERIZE_NODES.realized -- bufferize_classify's
// pre-seed pass (including the inline softmax-broadcast-reduce unmark)
// has already settled the realize set before the unified pass runs.
// Capturing RU_BUFFERIZE_TERM[i] as a side attribute (BOUNDARY_BUFFERIZE_TERM)
// gives emit_kernel_for_boundary a handle on the lowered-DAG boundary
// Term without altering the realize bits.
static void topo_sort_buffers_unified(Term root) {
  topo_sort_boundaries(root);
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    u32 idx = bufferize_info_find(BOUNDARY_ORDER[i]);
    BOUNDARY_BUFFERIZE_TERM[i] = (idx != 0xFFFFFFFFu)
                                    ? rangeify_unified_bufferize_at(idx)
                                    : 0;
  }
}

fn u32 materialize_boundary_count(void)         { return BOUNDARY_ORDER_LEN; }
fn u64 materialize_boundary_at(u32 i)           { return i < BOUNDARY_ORDER_LEN ? BOUNDARY_ORDER[i] : 0; }

// Single-output kernel accessors.  Every kernel writes exactly one
// output buffer (output_tid family); slot 0 maps to the per-kernel
// fields, all other slots return 0 / sentinels.
fn u32 kernel_entry_output_count(u32 kid) {
  if (kid >= KERNELS_NEXT) return 0;
  return 1u;
}

fn u32 kernel_entry_output_tid_at(u32 kid, u32 idx) {
  if (kid >= KERNELS_NEXT || idx != 0) return 0;
  return KERNELS[kid].output_tid;
}

fn u32 kernel_entry_output_dtype_at(u32 kid, u32 idx) {
  if (kid >= KERNELS_NEXT || idx != 0) return 0;
  return KERNELS[kid].output_dtype;
}

fn int kernel_entry_output_shape_at(u32 kid, u32 idx, Shape *out) {
  if (kid >= KERNELS_NEXT || out == NULL || idx != 0) return 0;
  *out = KERNELS[kid].output_shape;
  return 1;
}

fn u32 materialize_boundary_depth_at(u32 i) {
  if (i >= BOUNDARY_ORDER_LEN) return 0;
  u32 ridx = bufferize_info_find(BOUNDARY_ORDER[i]);
  if (ridx == 0xFFFFFFFFu) return 0;
  return BOUNDARY_DEPTH[ridx];
}

fn u32 materialize_boundary_last_use_at(u32 i) {
  if (i >= BOUNDARY_ORDER_LEN) return 0;
  u32 ridx = bufferize_info_find(BOUNDARY_ORDER[i]);
  if (ridx == 0xFFFFFFFFu) return 0;
  return BOUNDARY_LAST_USE[ridx];
}

// Accessors: read the per-input-slot bufferize source id and look
// up the BIndex chain summary for the (this kernel's loc, source
// buffer's loc) edge.
fn u32 kernel_entry_input_source_buffer_id(u32 kid, u32 slot) {
  if (kid >= KERNELS_NEXT) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (slot >= ke->n_inputs || ke->input_source_buffer_ids == NULL) return 0;
  return ke->input_source_buffer_ids[slot];
}

fn int kernel_entry_input_edge_summary(u32 kid, u32 slot, BIndex *out) {
  // Convenience wrapper: returns the first matching edge.
  return kernel_entry_input_edge_at(kid, slot, 0, out);
}

fn int kernel_entry_input_edge_at(u32 kid, u32 slot, u32 edge_idx,
                                  BIndex *out) {
  if (kid >= KERNELS_NEXT) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (slot >= ke->n_inputs) return 0;
  if (ke->input_source_buffer_ids == NULL) return 0;
  u32 src_id = ke->input_source_buffer_ids[slot];
  if (src_id == 0) return 0;
  Term src_uop = ke->source_uop;
  if (term_tag(src_uop) != TAG_UOP) return 0;
  u64 consumer_loc = term_val(src_uop);
  u32 consumer_idx = bufferize_find_by_loc(consumer_loc);
  if (consumer_idx == 0xFFFFFFFFu) return 0;
  BBufferize const *consumer_buf = bufferize_buffer_at(consumer_idx);
  if (consumer_buf == NULL) return 0;
  u32 consumer_id = consumer_buf->buffer_id;
  // Walk all edges and pick the edge_idx-th one whose
  // (consumer, source) pair matches.
  u32 seen = 0;
  for (u32 i = 0; i < bufferize_index_count(); i++) {
    BIndex const *e = bufferize_index_at(i);
    if (e == NULL) continue;
    if (e->consumer_buffer_id != consumer_id) continue;
    if (e->source_buffer_id != src_id) continue;
    if (seen == edge_idx) {
      if (out != NULL) *out = *e;
      return 1;
    }
    seen++;
  }
  return 0;
}

// Diagnostic: print the bufferize edge summary for every input
// slot of every emitted kernel.  Gated by
// DUMP_BUFFERIZE_KERNEL_EDGES=1; useful for verifying
// input_source_buffer_ids wiring against the bufferize edge table.
static int materialize_dump_kernel_edges_enabled(void) {
  char const *e = getenv("DUMP_BUFFERIZE_KERNEL_EDGES");
  return e != NULL && e[0] == '1';
}

static void materialize_dump_kernel_edges(u32 kernels_start) {
  if (!materialize_dump_kernel_edges_enabled()) return;
  for (u32 kid = kernels_start; kid < KERNELS_NEXT; kid++) {
    KernelEntry const *ke = &KERNELS[kid];
    fprintf(stderr,
            "kernel_edges kid=%u inputs=%u source_uop=%llu\n",
            (unsigned)kid,
            (unsigned)ke->n_inputs,
            (unsigned long long)(term_tag(ke->source_uop) == TAG_UOP
                                   ? term_val(ke->source_uop) : 0));
    for (u32 slot = 0; slot < ke->n_inputs; slot++) {
      u32 src_id = kernel_entry_input_source_buffer_id(kid, slot);
      if (src_id == 0) {
        fprintf(stderr, "  slot=%u source=leaf tid=%u\n",
                (unsigned)slot, (unsigned)ke->input_tids[slot]);
        continue;
      }
      BIndex edge;
      if (!kernel_entry_input_edge_summary(kid, slot, &edge)) {
        fprintf(stderr, "  slot=%u source_buffer=%u (no edge summary)\n",
                (unsigned)slot, (unsigned)src_id);
        continue;
      }
      fprintf(stderr,
              "  slot=%u source_buffer=%u chain_len=%u ops=%u%s%s%s%s%s%s\n",
              (unsigned)slot,
              (unsigned)src_id,
              (unsigned)edge.movement_chain_len,
              (unsigned)edge.chain_op_count,
              edge.has_reshape ? " reshape" : "",
              edge.has_permute ? " permute" : "",
              edge.has_expand  ? " expand"  : "",
              edge.has_pad     ? " pad"     : "",
              edge.has_shrink  ? " shrink"  : "",
              edge.has_flip    ? " flip"    : "");
    }
  }
}

// === view-only path for movement ops (g2c1) ===
//
// RESHAPE / EXPAND / PERMUTE / SHRINK / FLIP rewrite a TenDesc's
// View instead of allocating a kernel.  Each helper computes the
// target View from the source View; view_resolve walks a movement-
// op chain rooted at a TAG_TEN, allocating an alias TenDesc per
// layer via tensor_view_of (which inherits buf_id, dtype, backend,
// producer_kid).  PAD intentionally falls through (g2c2) -- a
// view-only PAD would have to read bytes outside the alloc.

// _merge_dims: collapse runs of stride-compatible axes in `src` into
// (merged_dim, stride, expand_real_dim) triples.  Mirrors tinygrad's
// View._merge_dims at tinygrad/shape/view.py:19.  Two adjacent axes
// (size s_a, stride st_a) and (size s_b, stride st_b) are
// stride-compatible iff st_a == s_b * st_b -- the outer axis steps
// over exactly one inner block, so logically the pair acts as a
// single axis of size s_a*s_b with the inner stride.
//
// Unit axes (size 1) are treated as "merging" placeholders and join
// the next axis without affecting the merged stride.  Stride-0
// (broadcast) axes form their own block; expand_real_dim is set to 0
// for them so the reshape fitter knows that block doesn't carry
// memory width.  No-mask version: thvm's View has no `mask` field.
//
// Output count <= src->shape.ndim; caller pre-allocates MAX_DIM slots.
typedef struct {
  u32 merged_dim;          // logical size of merged block
  i32 stride;              // stride of the inner-most axis in the block
  u32 expand_real_dim;     // merged_dim if stride!=0 else 0; tracks
                           //   how much memory width the block spans
} MergedDim;

static u32 view_merge_dims(View const *v, MergedDim *out) {
  if (v->shape.ndim == 0) return 0;
  out[0].merged_dim      = v->shape.dims[0];
  out[0].stride          = v->strides[0];
  out[0].expand_real_dim = v->strides[0] ? v->shape.dims[0] : 0;
  u32 n = 1;
  u8 merging = (v->shape.dims[0] == 1);
  for (u32 i = 1; i < v->shape.ndim; i++) {
    u32 s  = v->shape.dims[i];
    i32 st = v->strides[i];
    if (s == 1) continue;                                  // unit axes always merge
    MergedDim *last = &out[n - 1];
    if (merging || last->stride == (i32)s * st) {
      last->merged_dim     *= s;
      last->stride          = st;
      last->expand_real_dim = st ? (merging ? s : last->expand_real_dim * s) : 0;
    } else {
      out[n].merged_dim      = s;
      out[n].stride          = st;
      out[n].expand_real_dim = st ? s : 0;
      n++;
    }
    merging = (s == 1);
  }
  return n;
}

// Tinygrad-faithful reshape that absorbs into a single (possibly non-
// contig) view via _merge_dims when the new shape's axis decomposition
// aligns with the source's contig sub-blocks.  Mirrors
// View.reshape (tinygrad/shape/view.py:267).  Returns 1 on success,
// 0 when no single-view absorb exists (caller chains views).
static int view_apply_reshape(View const *src, u64 expr_loc, View *out) {
  u32 t_ndim  = (u32)term_val(heap_read(expr_loc + 1));
  Shape ts = {0}; ts.ndim = t_ndim;
  u32 t_numel = 1;
  for (u32 i = 0; i < t_ndim; i++) {
    u32 d = (u32)term_val(heap_read(expr_loc + 2 + i));
    ts.dims[i] = d;                       // shape keeps the kvar-packed extent
    t_numel *= kvar_extent_static(d);     // numel is the worst-case (hi) product
  }
  if (t_numel != src->numel) return 0;
  if (t_ndim > MAX_DIM) return 0;

  // Fast path: contig source -- canonical strides for new shape.
  if (src->contiguous) {
    *out = view_create(ts);
    return 1;
  }

  // Non-contig source: try to express the reshape as new strides
  // walking through src's merged contig sub-blocks, in REVERSE
  // (tinygrad walks from the trailing axis inward).
  MergedDim merged[MAX_DIM];
  u32 n_merged = view_merge_dims(src, merged);

  i32 strides_rev[MAX_DIM];   // collected back-to-front
  u32 strides_n = 0;
  i32 r_idx = (i32)t_ndim - 1;

  for (i32 mi = (i32)n_merged - 1; mi >= 0 && r_idx >= 0; mi--) {
    u32 acc        = 1;
    i32 new_stride = merged[mi].stride;
    u32 real_dim   = merged[mi].expand_real_dim;
    while (acc < merged[mi].merged_dim
        && acc != merged[mi].merged_dim
        && r_idx >= 0) {
      // A symbolic (kvar) dim contributes its upper bound to the merge math
      // (the buffer layout is worst-case); the shape itself keeps the kvar.
      u32 new_dim = kvar_extent_static(ts.dims[r_idx]);
      r_idx--;
      strides_rev[strides_n++] = new_stride;
      if (new_dim != 1) {
        acc        *= new_dim;
        new_stride *= (acc < real_dim) ? (i32)new_dim : 0;
      }
    }
    if (acc != merged[mi].merged_dim) return 0;   // mismatch -- caller falls back
  }
  // Pad any remaining outer axes with stride 0 (leading 1-dims).
  while ((u32)strides_n < t_ndim) strides_rev[strides_n++] = 0;
  if (strides_n != t_ndim) return 0;

  out->shape = ts;
  out->numel = t_numel;
  out->offset = src->offset;
  for (u32 i = 0; i < t_ndim; i++) out->strides[i] = strides_rev[t_ndim - 1 - i];
  for (u32 i = t_ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  // Contig iff the resulting strides are canonical row-major.
  out->contiguous = 1;
  i32 cs = 1;
  for (i32 i = (i32)t_ndim - 1; i >= 0; i--) {
    if (out->strides[i] != cs) { out->contiguous = 0; break; }
    cs *= (i32)ts.dims[i];
  }
  if (out->offset != 0) out->contiguous = 0;
  return 1;
}

static int view_apply_expand(View const *src, u64 expr_loc, View *out) {
  u32 t_ndim = (u32)term_val(heap_read(expr_loc + 1));
  if (t_ndim < src->shape.ndim) return 0;       // can't drop axes via EXPAND
  Shape ts = {0}; ts.ndim = t_ndim;
  u32 t_numel = 1;
  for (u32 i = 0; i < t_ndim; i++) {
    u32 td = (u32)term_val(heap_read(expr_loc + 2 + i));
    ts.dims[i] = td;                          // shape keeps the kvar-packed extent
    t_numel  *= kvar_extent_static(td);       // hi product (raw kvar would overflow u32)
    // Existing axis: must match exactly or be 1 (broadcast).
    if (i < src->shape.ndim
        && src->shape.dims[i] != td && src->shape.dims[i] != 1) return 0;
  }
  out->shape      = ts;
  out->numel      = t_numel;
  out->offset     = src->offset;
  out->contiguous = (t_numel == src->numel) ? src->contiguous : 0;
  for (u32 i = 0; i < t_ndim; i++) {
    if (i >= src->shape.ndim) out->strides[i] = 0;     // new trailing broadcast axis
    else out->strides[i] = (src->shape.dims[i] == ts.dims[i]) ? src->strides[i] : 0;
  }
  for (u32 i = t_ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  return 1;
}

static int view_apply_permute(View const *src, u64 expr_loc, View *out) {
  // Permute on a non-contig source is mathematically fine: just
  // reorder strides + dims to match the new axis order.  The output
  // is contig only if (a) src was contig AND (b) the permutation is
  // identity; the existing `contiguous = identity ? src->contiguous : 0`
  // assignment below already reflects that.
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  out->offset = src->offset;
  u8 used[MAX_DIM] = {0};
  u8 identity = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 p = (u32)term_val(heap_read(expr_loc + 2 + i));
    if (p >= src->shape.ndim || used[p]) return 0;
    used[p] = 1;
    ts.dims[i]      = src->shape.dims[p];
    out->strides[i] = src->strides[p];
    if (p != i) identity = 0;
  }
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  out->shape      = ts;
  out->numel      = src->numel;
  out->contiguous = identity ? src->contiguous : 0;
  return 1;
}

static int view_apply_shrink(View const *src, u64 expr_loc, View *out) {
  // Shrink on a non-contig source is mathematically fine: bump
  // offset by `b * src->strides[i]` per axis, keep src strides.
  // The output is contig only if (a) src was contig AND (b) the
  // shrink doesn't actually drop any element (numel preserved).
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  i32 add_off = 0;
  u32 t_numel = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 b = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
    u32 e = (u32)term_val(heap_read(expr_loc + 3 + 2 * i));
    // KV-cache append: a kvar-packed begin (KVAR_FLAG set) is a RUNTIME
    // row offset -- the symbolic `start_pos` of tinygrad's
    // `cache[..., start_pos:start_pos+T, ...].assign(k)` (gpt2.py:201).
    // Resolve it to the value bound via kvar_set_runtime BEFORE it
    // enters the offset arithmetic.  The slice end is `start_pos + T`
    // for a literal length T: the caller packs it as `pack(begin)+T`,
    // which still has KVAR_FLAG set, so `e - b` (on the raw packed
    // u32s) recovers T and the resolved end is `begin_runtime + T`.
    // (Decoding the end's id directly would read a DIFFERENT kvar -- the
    // +T bumps the id field, not the value.)  A LITERAL bound (flag
    // clear) is identity through kvar_extent_runtime, so every existing
    // compile-time SHRINK is byte-unchanged.
    if (kvar_extent_is_var(b)) {
      u32 t_len = kvar_extent_is_var(e) ? (e - b) : 0;  // raw delta = slice length
      b = kvar_extent_runtime(b);
      e = b + t_len;
    }
    // else (literal begin): keep b, e RAW.  A literal e is unchanged (e - b is
    // the literal dim).  A `{0, pack(S)}` slice over a SYMBOLIC axis (the
    // per-head multi-head-attention seq shrink) keeps its kvar-packed end, so
    // `e - b` = pack(S) preserves the symbolic dim.  Decoding the end to its
    // runtime bound here collapsed that symbolic axis to a literal and broke the
    // symbolic attention (a sub-slice of a literal axis at a kvar length is a
    // separate, not-yet-needed case).
    if (e <= b || e > src->shape.dims[i]) return 0;
    ts.dims[i] = e - b;
    // numel sizes at the static (hi) bound, matching view_create /
    // view_apply_reshape / view_apply_expand.  For a `{0, pack(S)}`
    // full-extent slice over a SYMBOLIC axis (the multi-head-attention
    // per-head seq shrink) `e - b` keeps the KVAR_FLAG, so the raw packed
    // value would overflow the u32 accumulator (1*pack(S)*dH wraps mod 2^32);
    // kvar_extent_static recovers the hi bound.  For a literal slice it is
    // the identity, so every compile-time SHRINK is byte-unchanged.
    // ts.dims[i] keeps the packed extent so the symbolic axis survives.
    t_numel  *= kvar_extent_static(e - b);
    add_off  += (i32)b * src->strides[i];
  }
  out->shape  = ts;
  out->numel  = t_numel;
  out->offset = src->offset + add_off;
  for (u32 i = 0; i < src->shape.ndim; i++) out->strides[i] = src->strides[i];
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  // An identity SHRINK (t_numel == src->numel; only possible when every
  // axis kept its full extent) inherits the source's contig flag.  A
  // real SHRINK drops elements, so the kept slice has non-canonical
  // strides for its new shape and is never contig.  The earlier
  // `contiguous = 1` blanket flipped non-contig identity shrinks to
  // contig and let materialize_root_alias skip the strided gather,
  // returning the raw underlying buffer (project_thvm_composed_grad_bug
  // -- May 20 follow-up; surfaced by tinygrad-port _pool chain).
  out->contiguous = (t_numel == src->numel) ? src->contiguous : 0;
  return 1;
}

static int view_apply_flip(View const *src, u64 expr_loc, View *out) {
  // Flip on a non-contig source is mathematically fine: negate
  // the per-axis stride and bump offset by (dim-1)*src_stride.
  // The output is contig only if no axis was actually flipped AND
  // src was contig.  `out->contiguous = any ? 0 : src->contiguous`
  // below already captures that.
  u32 mask = (u32)term_val(heap_read(expr_loc + 1));
  out->shape  = src->shape;
  out->numel  = src->numel;
  out->offset = src->offset;
  u8 any = 0;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    if (mask & (1u << i)) {
      out->strides[i] = -src->strides[i];
      out->offset += (i32)(src->shape.dims[i] - 1) * src->strides[i];
      any = 1;
    } else {
      out->strides[i] = src->strides[i];
    }
  }
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  out->contiguous = any ? 0 : src->contiguous;
  return 1;
}

// Materialize a UOP_CONST to a 1-element TenDesc filled with the
// const value.  Used by view_resolve when a movement-op chain
// (typically EXPAND from interact_grad's leaf rule that lifts a
// scalar cotangent to the target's shape) bottoms out at a CONST
// instead of a TenDesc.
static u32 const_to_tendesc(u64 const_loc) {
  Term num   = heap_read(const_loc);
  u32  dtype = term_ext(num);
  u32  bits  = (u32)term_val(num);
  Shape s = {0}; s.ndim = 1; s.dims[0] = 1;
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, dtype);
  // Write the dtype-correctly-sized scalar.  For F32 the bits field
  // already carries the IEEE-754 layout; integer dtypes interpret
  // `bits` as an i32 (sign-extended at the WL bridge) and pack down
  // to the narrow width.
  u8  buf8;  u16 buf16;  u32 buf32;  u64 buf64;
  void *src = NULL;  u64 nbytes = 0;
  switch (dtype) {
    case DT_BOOL:   buf8  = (u8)(bits & 1);                     src = &buf8;  nbytes = 1; break;
    case DT_INT8:   buf8  = (u8)(i8)(int32_t)bits;              src = &buf8;  nbytes = 1; break;
    case DT_UINT8:  buf8  = (u8)bits;                           src = &buf8;  nbytes = 1; break;
    case DT_INT16:  buf16 = (u16)(i16)(int32_t)bits;            src = &buf16; nbytes = 2; break;
    case DT_UINT16: buf16 = (u16)bits;                          src = &buf16; nbytes = 2; break;
    case DT_INT32:
    case DT_UINT32:
    case DT_FP32:   buf32 = bits;                               src = &buf32; nbytes = 4; break;
    case DT_INT64:  buf64 = (u64)(i64)(int32_t)bits;            src = &buf64; nbytes = 8; break;
    case DT_UINT64: buf64 = (u64)bits;                          src = &buf64; nbytes = 8; break;
    case DT_FP16:
    case DT_BF16:
    case DT_FP64:
    case DT_FP8E4M3:
    case DT_FP8E5M2: {
      // Promote f32 bits -> target float (lossy for 64-bit beyond
      // f32 precision; precise enough for the common 0.0 / 1.0 /
      // log(2) literals the grad chain rule emits).
      f32 v; memcpy(&v, &bits, sizeof(v));
      static u8 buf_bytes[8];
      from_fp32_lane(buf_bytes, dtype, &v, 1);
      src = buf_bytes; nbytes = dtype_storage_bytes(dtype, 1);
      break;
    }
    case DT_INT4: {
      static u8 nibble_byte;
      i8 v8 = (i8)((i32)bits);
      pack_int4(&nibble_byte, &v8, 1);
      src = &nibble_byte; nbytes = 1;
      break;
    }
    case DT_UINT4: {
      static u8 nibble_byte;
      u8 v8 = (u8)(bits & 0xFu);
      pack_uint4(&nibble_byte, &v8, 1);
      src = &nibble_byte; nbytes = 1;
      break;
    }
    default:
      // Larger / packed dtypes need a 64-bit payload (Phase D/F).
      // Fall back to a zero-fill so we don't write garbage past the
      // buffer end; caller will see all-zeros and fail visibly.
      buf64 = 0; src = &buf64; nbytes = dtype_storage_bytes(dtype, 1);
      break;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src, nbytes);
  return tid;
}

// Dispatcher: walk a movement-op chain, allocating one alias
// TenDesc per layer; return the final tid (0 on bail).  The
// source must resolve to a TenDesc (TAG_TEN, UOP_KERNEL, UOP_CONST
// materialized to a 1-element TenDesc, or a recursive view chain
// rooted at one).  Backend must be view-aware -- otherwise returns
// 0 so caller falls through.
static u32 boundary_index_for_loc(u64 loc);   // forward decl (Level 54)
static u8  op_is_view_movement(u8 op);        // forward decl (defined below)

// True iff the movement-op chain rooted at `t` carries the
// tinygrad-`_pool` im2col unfold signature -- a RESHAPE that reduces
// rank sitting directly over an EXPAND -- AND bottoms out at an
// already-emitted bufferize boundary (not a leaf/kernel, which
// view_resolve already handles directly).  Only that combination
// needs the boundary-base relaxation in view_resolve_inner: the
// rank-merge RESHAPE is the one view_apply_reshape can't absorb, so
// the chain MUST go through tensor_view_chain_append + dispatch
// pre-mat (the kernel-op fallback bails in rangeify and reads
// zeros), and that requires a buffer-backed source.
static int view_chain_needs_boundary_base(Term t) {
  int saw_pool_merge = 0;
  Term cur = term_resolve(t);
  for (u32 hops = 0; hops < 64; hops++) {
    if (term_tag(cur) != TAG_UOP) return 0;            // hit a TAG_TEN/etc -- handled directly
    u8 op = term_ext(cur);
    if (op == UOP_KERNEL) return 0;                    // buffer-backed already
    if (!op_is_view_movement(op) && op != UOP_PAD) {
      // chain source -- a non-movement compute UOP.  Qualify only
      // if it's an emitted boundary AND we passed the `_pool` merge.
      if (!saw_pool_merge) return 0;
      u32 bi = boundary_index_for_loc(term_val(cur));
      return (bi != 0xFFFFFFFFu && bi < BOUNDARY_ORDER_CAP
              && BOUNDARY_TID[bi] != 0);
    }
    u64 loc = term_val(cur);
    Term src = term_resolve(heap_read(loc + 0));
    if (op == UOP_RESHAPE && term_tag(src) == TAG_UOP
        && term_ext(src) == UOP_EXPAND) {
      Shape rs_out, ex_out;
      if (term_shape_in(cur, 0, &rs_out) && term_shape_in(src, 0, &ex_out)
          && rs_out.ndim < ex_out.ndim)
        saw_pool_merge = 1;
    }
    cur = src;
  }
  return 0;
}

static u32 view_resolve_inner(Term t, int boundary_base);

static u32 view_resolve(Term t) {
  if (CURRENT_BACKEND == NULL || !CURRENT_BACKEND->view_aware) return 0;
  return view_resolve_inner(t, view_chain_needs_boundary_base(t));
}

static u32 view_resolve_inner(Term t, int boundary_base) {
  if (CURRENT_BACKEND == NULL || !CURRENT_BACKEND->view_aware) return 0;
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    // Packed nibble dtypes can't ride the view-only path: every
    // gather step in materialize_root_alias and the cpu_interpret
    // pre-mat loop is byte-aligned (1/2/4/8) and packed itemsize
    // is 0.  Force a kernel emit so cpu_op_run_via_i8 handles the
    // unpack/repack for movement ops.
    if (tid != 0 && tid < TENS_NEXT && dtype_is_packed(TENS[tid].dtype)) return 0;
    return tid;
  }
  if (tag != TAG_UOP) return 0;

  u8  op  = term_ext(t);
  u64 loc = term_val(t);

  if (op == UOP_KERNEL) {
    Term outbuf = heap_read(loc);
    if (term_tag(outbuf) != TAG_TEN) return 0;
    return (u32)term_val(outbuf);
  }

  // CONST source: materialize to a fresh 1-element TenDesc.
  // interact_grad's leaf cotangent lift produces EXPAND(CONST(0|1))
  // -> target.shape; without this branch the grad chain stalls as
  // a UOP that never fires.
  if (op == UOP_CONST) return const_to_tendesc(loc);

  // A movement-op chain bottoming out at an ALREADY-EMITTED bufferize
  // boundary, when `view_resolve` was entered with boundary_base set
  // (the wrapper pre-scanned the chain and found the tinygrad-`_pool`
  // im2col signature -- a rank-merging RESHAPE over an EXPAND -- which
  // can't be emitted as kernel ops: rangeify refuses the rank-merge
  // RESHAPE and the fallback dispatch reads zeros).  Acting like a
  // TAG_TEN base case here lets the rest of the chain compose as a
  // ShapeTracker view over the boundary's buffer; the dispatch-time
  // pre-mat then gathers it once.  The boundary TenDesc carries
  // producer_kid (set in emit_kernel_for_boundary), so the chained
  // alias stays reachable -- kernel_fire_by_id chases the producer
  // before the gather runs, and boundaries emit in topo order so an
  // upstream boundary's BOUNDARY_TID is already populated.  Mirrors
  // what the (now-removed) WL `input = TMaterialize[...]` guard did,
  // minus the fresh TAG_TEN that broke TGrad on the original leaf.
  //
  // Level 54 (REVERTED) made this unconditional for ANY non-movement
  // op, which (a) aliased *pending* boundaries (BOUNDARY_TID == 0) on
  // uninitialised buffers and (b) re-routed simple `Reshape(Shrink)`
  // / `Pad` chains over a realized boundary onto a strided alias that
  // downstream kernel-op consumers mis-read.  The boundary_base gate
  // + BOUNDARY_TID-populated check is the safe subset.
  if (boundary_base) {
    u32 bi = boundary_index_for_loc(loc);
    if (bi != 0xFFFFFFFFu && bi < BOUNDARY_ORDER_CAP && BOUNDARY_TID[bi] != 0) {
      u32 btid = BOUNDARY_TID[bi];
      if (btid != 0 && btid < TENS_NEXT && !dtype_is_packed(TENS[btid].dtype))
        return btid;
    }
  }

  // Source recurses (could be another movement op chain or CONST).
  u32 src_tid = view_resolve_inner(heap_read(loc), boundary_base);
  if (src_tid == 0) return 0;
  View const *src_view = &TENS[src_tid].view;

  View nv = {0};
  int  ok = 0;
  switch (op) {
    case UOP_RESHAPE: ok = view_apply_reshape(src_view, loc, &nv); break;
    case UOP_EXPAND:  ok = view_apply_expand (src_view, loc, &nv); break;
    case UOP_PERMUTE: ok = view_apply_permute(src_view, loc, &nv); break;
    case UOP_SHRINK:  ok = view_apply_shrink (src_view, loc, &nv); break;
    case UOP_FLIP:    ok = view_apply_flip   (src_view, loc, &nv); break;
    default: return 0;                      // PAD + non-movement ops bail
  }
  if (ok) {
    u32 alias = tensor_view_of(src_tid, nv);
    // KV-cache append: a SHRINK whose LEADING-axis begin is a kvar-packed
    // value (start_pos) is the dst of an in-place cache write at a RUNTIME
    // row offset.  Stamp that kvar id on the alias so the JIT ASSIGN replay
    // re-resolves the row from kvar_runtime at FIRE (view.offset bakes only
    // the capture step's row).  Read-side `{0, pack(S)}` shrinks have a
    // LITERAL begin -> not flagged.  Harmless on non-assign aliases: only the
    // ASSIGN replay path consults assign_kvar_id.
    if (op == UOP_SHRINK && alias != 0) {
      u32 b0 = (u32)term_val(heap_read(loc + 2));   // axis-0 begin
      if (kvar_extent_is_var(b0)) {
        TENS[alias].assign_kvar_id = kvar_extent_var_id(b0);
      }
    }
    return alias;
  }
  // Single-view absorb failed.  For RESHAPE this is one of:
  //   (a) malformed (t_numel != src numel, or t_ndim > MAX_DIM) -- no
  //       chain can help; fall through to the kernel-op-emit path.
  //   (b) a valid reshape that genuinely isn't expressible as a single
  //       View (merging a stride-0 broadcast axis with a real-stride
  //       axis -- tinygrad's `_pool` im2col reshape, and the
  //       EXPAND->RESHAPE-merge in the abort repro).  The kernel-op-
  //       emit path would have to materialise a reshape over a non-
  //       contig source, which the post-F6 runtime can't do for these
  //       shapes (-> SIGABRT).  Instead: APPEND a fresh canonical
  //       outer View onto the source's ShapeTracker chain
  //       (tensor_view_chain_append), exactly as tinygrad's
  //       ShapeTracker.reshape appends a View when the merge fails.
  //       The dispatch-time pre-mat (cpu_dispatch_kernel /
  //       metal_dispatch_kernel) and materialize_root_alias walk the
  //       chain via tendesc_strided_index to gather the strided view
  //       into a contiguous temp once before reading it.
  //
  // Gate: ONLY case (b) chain-appends -- the merge-walk in
  // view_apply_reshape already absorbs the single-View-expressible
  // reshapes (those return ok=1 above and never reach here), and
  // malformed reshapes can't be helped.  This is the narrow re-enable
  // the LeNet-regression note carved out for: a stride-trick view no
  // kernel-op-emit path can produce.  PERMUTE/SHRINK/FLIP failures are
  // always malformed input, so they keep falling through.
  if (op == UOP_RESHAPE) {
    u32 t_ndim = (u32)term_val(heap_read(loc + 1));
    if (t_ndim <= MAX_DIM) {
      u32 t_numel = 1;
      for (u32 i = 0; i < t_ndim; i++)
        t_numel *= (u32)term_val(heap_read(loc + 2 + i));
      if (t_numel == src_view->numel) {
        Shape ts = {0}; ts.ndim = t_ndim;
        for (u32 i = 0; i < t_ndim; i++)
          ts.dims[i] = (u32)term_val(heap_read(loc + 2 + i));
        View outer = view_create(ts);          // canonical contig outer face
        u32 chained = tensor_view_chain_append(src_tid, outer);
        if (chained != 0) return chained;
      }
    }
  }
  // Malformed reshape, chain wraparound, or non-RESHAPE absorb
  // failure: fall back to emitting a kernel op.
  return 0;
}

// True when a UOp opcode is one of the 5 view-only-path movement ops.
static u8 op_is_view_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_EXPAND || op == UOP_PERMUTE
      || op == UOP_SHRINK  || op == UOP_FLIP;
}

// Flatten a non-contig TenDesc into a fresh contiguous copy via
// view_strided_index.  Used by thvm_materialize when the root is a
// movement-op chain that resolves to a view alias the caller will
// read through (wnf expects flat-buffer reads).
static Term materialize_root_alias_rec(Term t, int record_replay) {
  if (term_tag(t) != TAG_TEN) return t;
  u32 tid = (u32)term_val(t);
  if (tid == 0 || tid >= TENS_NEXT) return t;
  TenDesc *d = &TENS[tid];
  // Skip the gather only when EVERYTHING is contig: public view is
  // contig with no offset AND no ShapeTracker chain (which would
  // map a contig outer view through non-contig inner views).
  if (d->view.contiguous && d->view.offset == 0 && d->nviews == 0) return t;

  u32 dst_tid = tensor_alloc(d->backend, d->view.shape, d->dtype);
  if (dst_tid == 0) return t;

  // Bytes to read = max element index reachable + 1.  When there's
  // no chain we can compute it from strides analytically (cheap).
  // With a chain we'd need the full per-element walk -- defer that
  // to the gather loop below by allocating enough to cover the
  // underlying buffer's true size if the backend reports it.
  u32 max_idx = 0;
  if (d->nviews == 0) {
    i32 m = d->view.offset;
    for (u32 i = 0; i < d->view.shape.ndim; i++) {
      // Resolve a symbolic (kvar) dim to its hi bound: the gather buffer is
      // sized at the worst case; the raw kvar-packed extent would make
      // (dim-1)*stride ~= INT_MAX*stride and corrupt the allocation size.
      u32 dim_s = kvar_extent_static(d->view.shape.dims[i]);
      if (dim_s > 1 && d->view.strides[i] > 0)
        m += (i32)(dim_s - 1) * d->view.strides[i];
    }
    max_idx = (u32)m;
  } else {
    for (u32 k = 0; k < d->view.numel; k++) {
      u32 bidx = tendesc_strided_index(d, k);
      if (bidx > max_idx) max_idx = bidx;
    }
  }
  size_t src_bytes = (size_t)dtype_storage_bytes(d->dtype, (u64)(max_idx + 1));
  void  *raw       = malloc(src_bytes);
  d->backend->buf_read(d->buf_id, raw, src_bytes);
  size_t dst_bytes = (size_t)dtype_storage_bytes(d->dtype, d->view.numel);
  void *dst_host = malloc(dst_bytes);
  if (d->dtype == DT_FP32) {
    f32 *o = (f32 *)dst_host; f32 *s = (f32 *)raw;
    for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)];
  } else {
    switch (dtype_itemsize(d->dtype)) {
      case 1: { u8  *o = (u8  *)dst_host, *s = (u8  *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      case 2: { u16 *o = (u16 *)dst_host, *s = (u16 *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      case 4: { u32 *o = (u32 *)dst_host, *s = (u32 *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      case 8: { u64 *o = (u64 *)dst_host, *s = (u64 *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      default:
        free(raw); free(dst_host); tensor_release(dst_tid); return t;
    }
  }
  d->backend->buf_write(TENS[dst_tid].buf_id, dst_host, dst_bytes);
  free(raw);
  free(dst_host);
  // Under TJit capture, this gather is a one-shot host memcpy that never
  // enters the recorded dispatch stream -- so a per-step replay keeps the
  // capture-step bytes even when the source is recomputed (a re-firing
  // reduce broadcast) or mutated (a KV cache an append rewrites).  Record
  // it as a replayable JIT_OP_GATHER off the (now committed) dst buffer so
  // each replay re-gathers the live source.
  if (record_replay) {
    jit_capture_record_gather(tid, TENS[dst_tid].buf_id);
  }
  return term_new(0, TAG_TEN, d->dtype, dst_tid);
}

static Term materialize_root_alias(Term t) {
  return materialize_root_alias_rec(t, 0);
}

// === build_kernel: visit() recursion (g2b) ===

static u32 boundary_index_for_loc(u64 loc) {
  u32 h = boundary_hash_of(loc);
  for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
    u32 idx = BOUNDARY_HASH[i];
    if (idx == BOUNDARY_HASH_EMPTY) return 0xFFFFFFFFu;
    if (idx < BOUNDARY_ORDER_LEN && BOUNDARY_ORDER[idx] == loc) return idx;
  }
  return 0xFFFFFFFFu;
}

static u32 input_slot_dedup(KernelEntry *ke, u32 tid, Term term) {
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_tids[i] == tid && ke->input_terms[i] == term) return i;
  kernel_inputs_reserve(ke, ke->n_inputs + 1);
  u32 slot = ke->n_inputs++;
  ke->input_tids   [slot] = tid;
  ke->input_dtypes [slot] = TENS[tid].dtype;
  ke->input_numels [slot] = TENS[tid].view.numel;
  ke->input_terms  [slot] = term;
  ke->input_views  [slot] = TENS[tid].view;     // codegen consumes for strided reads
  // Mirror rangeify.c's input_chain_composed[] bookkeeping for the
  // unified-pass path: when the unified pass folded prior_views into
  // the TAG_TEN-wrapped INDEX_E.addr, the backend pre-mat must SKIP
  // the gather (the kernel reads the underlying buffer directly with
  // the chain-composed addr).  The matching addr rewrite for chained
  // tids minted by visit()/view_resolve INSIDE materialize lives in
  // unified_rewrite_rec_sub's INDEX_E case, which also flips this
  // flag at that point.
  if (ke->input_chain_composed != NULL
      && rangeify_unified_tid_chain_composed(tid)) {
    ke->input_chain_composed[slot] = 1;
  }
  return slot;
}

// Symbolic input slot: a TVAR whose binding LAM has a shape
// annotation but no concrete TEN yet (pre APP-LAM beta).
// `tid = 0` flags the slot as needing fire-time resolution
// (interact_kernel sees tid==0 + term!=0 and term_resolves).
static u32 input_slot_dedup_var(KernelEntry *ke, Term var_term,
                                 u32 dtype, u32 numel) {
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_tids[i] == 0 && ke->input_terms[i] == var_term) return i;
  kernel_inputs_reserve(ke, ke->n_inputs + 1);
  u32 slot = ke->n_inputs++;
  ke->input_tids   [slot] = 0;          // resolved at fire time
  ke->input_dtypes [slot] = dtype;
  ke->input_numels [slot] = numel;
  ke->input_terms  [slot] = var_term;
  // Symbolic input: shape-annotated but no concrete strides until
  // fire time.  Synthesize a canonical contig view at the annotated
  // shape so codegen doesn't try to emit strided reads against
  // garbage stride bytes.
  Shape s = {0}; s.ndim = 1; s.dims[0] = numel;
  ke->input_views  [slot] = view_create(s);
  return slot;
}

static int materialize_dump_big_input_source_enabled(void) {
  char const *e = getenv("DUMP_BIG_INPUT_SOURCE");
  return e != NULL && e[0] == '1';
}

static void materialize_dump_source_child(Term child, u32 depth) {
  child = term_resolve(child);
  for (u32 i = 0; i < depth; i++) fprintf(stderr, "  ");
  if (term_tag(child) != TAG_UOP) {
    fprintf(stderr, "leaf tag=%u ext=%u val=%llu\n",
            term_tag(child), term_ext(child),
            (unsigned long long)term_val(child));
    return;
  }
  u64 loc = term_val(child);
  u32 idx = bufferize_info_find(loc);
  fprintf(stderr, "uop op=%u loc=%llu realized=%u consumers=%u reasons=0x%x\n",
          term_ext(child), (unsigned long long)loc,
          idx == 0xFFFFFFFFu ? 0 : BUFFERIZE_NODES[idx].realized,
          idx == 0xFFFFFFFFu ? 0 : BUFFERIZE_NODES[idx].consumer_count,
          idx == 0xFFFFFFFFu ? 0 : BUFFERIZE_NODES[idx].reasons);
  if (depth >= 2 || term_ext(child) == UOP_KERNEL) return;
  u8 ar = uop_arity(term_ext(child));
  for (u8 i = 0; i < ar; i++) {
    materialize_dump_source_child(heap_read(loc + i), depth + 1);
  }
}

static void materialize_dump_big_input_source(KernelEntry *ke,
                                              u64 boundary_loc) {
  if (!materialize_dump_big_input_source_enabled()) return;
  if (ke == NULL || ke->n_inputs <= 30) return;
  fprintf(stderr,
          "big_input_source kid=%u n_inputs=%u out=%u source_op=%u loc=%llu\n",
          (u32)(ke - KERNELS), ke->n_inputs,
          ke->output_numel, term_ext(ke->source_uop),
          (unsigned long long)boundary_loc);
  materialize_dump_source_child(ke->source_uop, 0);
}

// Recursive visit.  Returns VISIT_OK on success (the per-op
// program is gone; visit now exists only to populate ke's input
// bindings) or VISIT_BAIL on any unsupported op.
static u32 visit(Term t, KernelEntry *ke, u64 root_loc, VisitMemo *memo) {
  // Resolve VAR (SUB-bit) + ALO (one-layer force) chains so a body
  // post-APP-LAM-beta exposes its bound argument's TEN/UOP rather
  // than the bare VAR cell that visit() would otherwise bail on.
  // term_resolve is a pure pointer hop -- no allocation, no firing.
  t = term_resolve(t);
  u8 tag = term_tag(t);

  // DP1_GRAD projections are driven by wnf's uop_drive_inner_actives
  // before materialize ever sees them.  If one survives into visit
  // (e.g. shape inference happening too early), fall through to the
  // VISIT_BAIL at line 641 -- the realize loop will iterate, wnf
  // will fire it next pass, and re-enter materialize.  Materialize
  // is graph -> kernel compile, NEVER fires interactions.

  if (tag == TAG_TEN) {
    u32 tid  = (u32)term_val(t);
    u32 slot = input_slot_dedup(ke, tid, t);
    if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
    return KSRC_AS_INPUT(slot);
  }
  // Shape-annotated TVAR: bound by a TLamShape whose annotation
  // sits in the lam_shape side table.  Treat as a symbolic input
  // slot: the kernel program references KSRC_AS_INPUT(slot), and
  // at fire time interact_kernel resolves input_terms[slot]
  // (the VAR Term) through SUB to whatever APP-LAM beta has
  // bound it to -- typically the recursive-loop iter's current
  // weight tensor.  Lets a lambda body materialize ONCE without
  // waiting for substitution; the kernel-program cache then
  // dedups this kernel against future structurally identical
  // emissions from re-instantiations of the same body.
  if (tag == TAG_VAR) {
    Shape s;
    if (lam_shape_lookup(term_val(t), &s)) {
      u32 numel = 1;
      for (u32 i = 0; i < s.ndim; i++) numel *= s.dims[i];
      u32 slot = input_slot_dedup_var(ke, t, DT_FP32, numel);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      return KSRC_AS_INPUT(slot);
    }
    return VISIT_BAIL;          // no shape annotation -- can't compile
  }
  if (tag != TAG_UOP) return VISIT_BAIL;

  u8  op  = term_ext(t);
  u64 loc = term_val(t);
  u32 memo_ref = visit_memo_lookup(memo, loc);
  if (memo_ref != VISIT_BAIL) {
    return memo_ref;
  }

  if (op == UOP_KERNEL) {
    Term outbuf = heap_read(loc);
    if (term_tag(outbuf) != TAG_TEN) return VISIT_BAIL;
    u32 tid  = (u32)term_val(outbuf);
    u32 slot = input_slot_dedup(ke, tid, t);
    if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
    // No bufferize source id: UOP_KERNEL is post-materialize and
    // the bufferize graph operates on pre-kernel locs, so we leave
    // this slot's source_buffer_id at 0 (the leaf-input sentinel).
    u32 ref = KSRC_AS_INPUT(slot);
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  // Cross-realize alias: if a previous realize pass already produced
  // a TenDesc for this UOP loc, plug it in as an input slot here too.
  // Matches the materialize-entry short-circuit so a forward
  // intermediate realized in pass N stays reachable in pass N+1's
  // kernels (and is never re-emitted).  Mirror: tinygrad's
  // pm_generate_realize_map memoizes UOp -> Buffer on the lazy graph;
  // each subsequent schedule sees the prior Buffer attached.
  // Boundary that isn't this kernel's root: become an input.  The
  // upstream boundary was emitted earlier in topo order, so its
  // BOUNDARY_TID slot is populated.
  if (loc != root_loc) {
    u32 bi = boundary_index_for_loc(loc);
    if (bi != 0xFFFFFFFFu) {
      u32 tid = BOUNDARY_TID[bi];
      Term boundary_term = BOUNDARY_TERM[bi];
      if (tid == 0) return VISIT_BAIL;
      u32 slot = input_slot_dedup(ke, tid, boundary_term);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      // Record the source buffer id so rangeify and other consumers
      // can call bufferize_edge_summary with (root_loc, loc).
      // bufferize_find_by_loc returns the index (0-based); store the
      // buffer_id (1-based) here.  When the source isn't in the
      // bufferize graph (defensive case for boundaries inserted
      // post-classify), leave the sentinel 0.
      u32 sidx = bufferize_find_by_loc(loc);
      if (sidx != 0xFFFFFFFFu && ke->input_source_buffer_ids != NULL) {
        BBufferize const *src_buf = bufferize_buffer_at(sidx);
        if (src_buf != NULL) {
          ke->input_source_buffer_ids[slot] = src_buf->buffer_id;
        }
      }
      u32 ref = KSRC_AS_INPUT(slot);
      visit_memo_store(memo, loc, ref);
      return ref;
    }
  }

  if (op == UOP_CONST) {
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  if (uop_is_unary_elementwise(op) || op == UOP_LOAD) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  if (op == UOP_CAST || op == UOP_BITCAST) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    Term num = heap_read(loc + 1);
    if (term_tag(num) != TAG_NUM) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  if (uop_is_binary_elementwise(op)) {
    u32 li = visit(heap_read(loc + 0), ke, root_loc, memo);
    if (li == VISIT_BAIL) return VISIT_BAIL;
    u32 ri = visit(heap_read(loc + 1), ke, root_loc, memo);
    if (ri == VISIT_BAIL) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  if (uop_is_ternary_elementwise(op)) {
    // IWHERE(cond, then, else): float ternary select.  Visit all three
    // value srcs (the renderer emits `cond ? then : else`).
    u32 ci = visit(heap_read(loc + 0), ke, root_loc, memo);
    if (ci == VISIT_BAIL) return VISIT_BAIL;
    u32 ti = visit(heap_read(loc + 1), ke, root_loc, memo);
    if (ti == VISIT_BAIL) return VISIT_BAIL;
    u32 ei = visit(heap_read(loc + 2), ke, root_loc, memo);
    if (ei == VISIT_BAIL) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  // GRAD is a stop point in materialize -- the architecture is
  // wnf-fires-grad + materialize-compiles-uops, with thvm_realize
  // looping the pair until no fresh kernels are emitted.  Bailing
  // here makes the enclosing kernel emission abort so the caller
  // (thvm_realize) loops back through wnf to fire interact_grad.
  // (UOP_GRAD/UOP_FWD moved to TAG_DP{0,1}+DUP_GRAD_FLAG -- the
  // visit-time term_resolve at the top of this function already
  // bails on TAG_DP* via the TAG_UOP-only filter.)

  // Movement ops as a child of the kernel: try view-only resolve
  // first.  If the source isn't a contig TenDesc-resolvable chain
  // (e.g., EXPAND wrapping a MUL from interact_grad), fall through
  // to a recursive visit that walks the source for input-slot
  // tracking + bail propagation.
  if (op_is_view_movement(op)) {
    u32 alias_tid = view_resolve(t);
    if (alias_tid != 0) {
      u32 slot = input_slot_dedup(ke, alias_tid, t);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      u32 ref = KSRC_AS_INPUT(slot);
      visit_memo_store(memo, loc, ref);
      return ref;
    }
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  if (op == UOP_PAD) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  // REDUCE -- as the kernel root (tail-fuse) or as an intermediate
  // op whose result is consumed elementwise (broadcast) by later
  // program ops.
  if (op == UOP_REDUCE) {
    ReduceChainInfo rc;
    if (reduce_chain_collect(t, &rc)) {
      int chain_inlined = 1;
      for (u32 j = 1; j < rc.n_reduces; j++) {
        u32 cidx = bufferize_info_find(rc.locs[j]);
        if (cidx != 0xFFFFFFFFu && BUFFERIZE_NODES[cidx].realized) {
          chain_inlined = 0;
          break;
        }
      }
      if (!chain_inlined) goto single_reduce_emit;
      u32 src_idx = visit(rc.src, ke, root_loc, memo);
      if (src_idx == VISIT_BAIL) return VISIT_BAIL;
      visit_memo_store(memo, loc, VISIT_OK);
      return VISIT_OK;
    }

  single_reduce_emit:
    {
      // REDUCE over a size-1 axis is data-identity (output numel ==
      // input numel; only the shape loses the size-1 axis).  Emitting
      // an S_REDUCE for it forces a degenerate extent-1 reduce range,
      // and the UOp-DAG walker can't recover that extent when the body
      // doesn't reference the range -> it returns the reduce identity
      // (0 for SUM).  Short-circuit to the source value; the kernel's
      // output_shape (set in emit_kernel_for_boundary) already drops
      // the axis, so the bytes flow through unchanged.
      // Multi-axis REDUCE: identity-shortcut fires when EVERY reduce
      // axis is size-1 AND the output shape equals the input shape
      // (i.e. shape inference dropped no real dims).
      u32 n_axes = uop_reduce_n_axes(t);
      Shape src_sh0 = {0};
      Shape out_sh0 = {0};
      int same_shape = term_shape_in(uop_reduce_src(t), 0, &src_sh0)
                    && term_shape_in(t, 0, &out_sh0)
                    && src_sh0.ndim == out_sh0.ndim;
      if (same_shape) {
        for (u32 d = 0; d < src_sh0.ndim; d++) {
          if (src_sh0.dims[d] != out_sh0.dims[d]) { same_shape = 0; break; }
        }
      }
      int all_size_one = same_shape;
      for (u32 ai = 0; ai < n_axes && all_size_one; ai++) {
        u32 ax = uop_reduce_axis(t, ai);
        if (!(ax < src_sh0.ndim && src_sh0.dims[ax] == 1)) all_size_one = 0;
      }
      if (all_size_one) {
        u32 sidx = visit(uop_reduce_src(t), ke, root_loc, memo);
        if (sidx == VISIT_BAIL) return VISIT_BAIL;
        visit_memo_store(memo, loc, sidx);
        return sidx;
      }
    }
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  return VISIT_BAIL;
}

// Build one kernel rooted at the boundary at index bi.  Returns
// the emitted UOP_KERNEL term, or 0 on bail.
static Term emit_kernel_for_boundary(u32 bi) {
  u64 boundary_loc = BOUNDARY_ORDER[bi];
  u32 idx = bufferize_info_find(boundary_loc);
  if (idx == 0xFFFFFFFFu) return 0;

  // If this boundary was already aliased to an earlier kernel's
  // output (degenerate movement-op chain producing the same
  // TenDesc), BOUNDARY_TID/BOUNDARY_TERM are populated -- re-return
  // the alias so the emit loop sees a non-zero result.
  if (BOUNDARY_TID[bi] != 0 && BOUNDARY_TERM[bi] != 0) {
    return BOUNDARY_TERM[bi];
  }

  // Cross-pass boundary memoization (mirror tinygrad pm_generate_realize_map
  // memoizing UOp -> Buffer; indexing.py:28-35).  A boundary at this loc may
  // have already been emitted by an EARLIER materialize -- a prior fixpoint
  // iteration of THIS realize, or an earlier CTR child (the loss) whose
  // forward graph shares the loc with a later child's backward graph.  The
  // canonical case: the maxpool-input activation, realized once but reached by
  // the forward window-MAX (loss child) AND the backward argmax mask CMPEQ
  // (grad child).  Without reuse each consumer re-emits the activation into a
  // SEPARATE buffer; the two copies fp-disagree at an argmax tie, the CMPEQ
  // misses, the /count divide sees 0, and RECIP(0) NaNs (the stacked-maxpool
  // default-seed NaN).  Consulting MATERIALIZED_LOC here ties every consumer
  // to the ONE emitted buffer.  Gated by scope>0 (inside a realize wrapper);
  // the lookup's own dead-buffer guard makes a rolled-back tid a safe miss.
  if (materialized_loc_scope_depth() > 0) {
    u32 cached_tid = materialized_loc_lookup(boundary_loc);
    if (cached_tid != 0) {
      Term ten = term_new(0, TAG_TEN, TENS[cached_tid].dtype, cached_tid);
      BOUNDARY_TID[bi]  = cached_tid;
      BOUNDARY_TERM[bi] = ten;
      return ten;
    }
  }

  u8   op        = BUFFERIZE_NODES[idx].op;
  Term root_term = term_new(0, TAG_UOP, op, boundary_loc);

  Shape out_shape = {0};
  if (!term_shape_in(root_term, 0, &out_shape)) return 0;
  u32 out_dtype = DT_FP32;
  term_dtype_in(root_term, 0, &out_dtype);

  // Memory planner: push any earlier kernel's output buf onto the
  // backend freelist if its last consumer (in alloc-depth terms)
  // has already emitted.  cpu_buf_alloc / metal_buf_alloc inside
  // tensor_alloc below then pop a same-nbytes match instead of
  // growing the buf table.
  u32 this_depth = BOUNDARY_DEPTH[idx];
  mem_plan_push_dead(this_depth);

  // Arena planner (port of tinygrad/schedule/memory.py
  // memory_plan_rewrite line 47-52): if this boundary's output was
  // assigned an arena slot by arena_compute(), use a view-buf into
  // the shared arena.  Non-overlapping lifetimes share bytes, so 40
  // same-depth conv outputs occupy only ~3x one output's size in the
  // arena (live set), not 40x.  Fall through to tensor_alloc on miss
  // (multi-consumer / root / arena-disabled).
  // Per-op device: this boundary's output lives on the device its uop
  // places it on (term_device_in), NOT a single realize-wide backend --
  // the device is in the graph (tinygrad).  Dispatch follows
  // TENS[output_tid].backend (uop_kernel.c), so allocating the output on
  // op_backend is all it takes to run this kernel there; a COPY uop is
  // the natural boundary that moves data between devices.  When op_backend
  // is the realize backend the arena planner applies as before; a kernel
  // on a DIFFERENT device skips the arena (which is keyed to
  // CURRENT_BACKEND) and allocates directly on its backend.
  i32      op_dev     = term_device_in(root_term);
  Backend *op_backend = (op_dev < 0) ? CURRENT_BACKEND : ctx_ensure_backend(op_dev);
  if (op_backend == NULL) op_backend = CURRENT_BACKEND;
  u32 out_tid = (op_backend == CURRENT_BACKEND)
                  ? arena_tensor_alloc(bi, out_shape, out_dtype) : 0;
  if (out_tid == 0) {
    out_tid = tensor_alloc(op_backend, out_shape, out_dtype);
    if (out_tid != 0 && op_backend == &CPU_BACKEND) ARENA_ALLOCS_LEGACY++;
#ifdef THVM_HAS_CUDA
    if (out_tid != 0 && op_backend == &CUDA_BACKEND) ARENA_ALLOCS_LEGACY++;
#endif
  }
  u32 kid     = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->output_tid    = out_tid;
  ke->output_dtype  = out_dtype;
  ke->output_shape  = out_shape;
  ke->output_numel  = TENS[out_tid].view.numel;
  ke->source_uop    = root_term;
  // topo_sort_boundaries selects this boundary from
  // RU_BUFFERIZE_TERM[]; stash the UOP_BUFFERIZE node here so later
  // consumers (cached_lift wiring, debug dumps) can walk the lowered
  // subtree via uop_bufferize_value(b).
  ke->compute_bufferize = BOUNDARY_BUFFERIZE_TERM[bi];
  TENS[out_tid].producer_kid = kid;

  VisitMemo memo = {0};
  u32 result = visit(root_term, ke, boundary_loc, &memo);
  visit_memo_free(&memo);
  if (result == VISIT_BAIL) {
    kernel_dealloc_last(kid);
    TENS[out_tid].producer_kid = 0;
    return 0;
  }

  // Degenerate case: visit() consumed the whole boundary subgraph as
  // a single input slot (result == KSRC_AS_INPUT).  This happens when
  // the boundary's root is a movement-op chain whose view_resolve
  // found a direct alias TenDesc -- no compute to do.  Without this
  // branch the kernel commits with an empty program; cpu_interpret
  // runs nothing; the alloc'd output buffer stays zero-initialized,
  // silently zeroing whatever signal was supposed to flow through
  // (the gy=CONST(1.0) seed in MSE backward, etc).  Skip kernel
  // emission and alias the boundary's output to the input tid
  // directly.
  if (KSRC_IS_INPUT(result)) {
    u32 alias_tid = ke->input_tids[KSRC_INDEX(result)];
    if (alias_tid != 0 && alias_tid < TENS_NEXT) {
      // A realized movement-op boundary that resolved to a NON-CONTIGUOUS
      // view-alias (a transpose / strided getitem of a computed source)
      // must NOT be aliased to the underlying buffer.  The boundary exists
      // because the rangeify consumer-divergence walk realized this view
      // for two consumers whose swizzles diverge (Newton-Schulz `Gw` vs
      // `Gw.T`, q.reshape(M,N,1) vs q.reshape(M,1,N)); those consumers read
      // the boundary with a ROW-MAJOR flat addr over the boundary's OUTPUT
      // shape (they assume the realized buffer is contiguous, mirroring
      // tinygrad's BUFFERIZE.index(*consumer_ranges) over a contiguous
      // store -- indexing.py:75-78).  Aliasing the strided source view
      // instead leaks the source's stride into the flat read -> wrong
      // element (the transpose is silently dropped).  Fall through to emit
      // the copy/gather kernel so the boundary materializes contiguous
      // data, exactly as a realized tinygrad BUFFERIZE does.  A CONTIGUOUS
      // alias (offset/identity view, the gy=CONST(1.0) MSE-backward seed)
      // is still aliased -- same bytes, no copy needed.
      int alias_contig = TENS[alias_tid].view.contiguous
                      && TENS[alias_tid].view.offset == 0
                      && TENS[alias_tid].nviews == 0;
      int boundary_is_movement = uop_is_movement(op);
      if (!(boundary_is_movement && !alias_contig)) {
        // Release the unused output_tid we speculatively allocated.
        tensor_release(out_tid);
        kernel_dealloc_last(kid);
        Term alias_term = term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
        BOUNDARY_TID [bi] = alias_tid;
        BOUNDARY_TERM[bi] = alias_term;
        return alias_term;
      }
    }
  }

  materialize_dump_big_input_source(ke, boundary_loc);

  ke->schedule = &ke->_local_schedule;

  // Cache kernel_lift_to_uop output on the KernelEntry: the lifter
  // resolves the unified-pass store_root and packages it as
  // KernelUopLift.  When the lift declines (no source_uop /
  // bufferize_info miss / n_inputs > KERNEL_LIFT_MAX_INPUT)
  // cached_lift stays zero-initialized and the dispatch ladder
  // falls back to whatever paths each backend exposes for unlifted
  // kernels (today: cpu_blas_dispatch by pattern, cpu_jit_dispatch
  // declines, cpu_uop_walk declines, Metal dispatch returns -1).
  //
  // Dispatch-time consumers (cpu_jit_build, cg_emit_via_uop,
  // cpu_uop_walk) read store_root / out_buf / in_bufs[] from
  // cached_lift without re-running the lifter.
  if (kernel_lift_to_uop(ke, &ke->cached_lift)) {
    // Rewrite the unified store_root for the walker (substitute
    // TAG_TEN/BUFFERIZE leaves with UOP_BUFFER input slots).  The
    // cached_lift.in_bufs[] / n_inputs / out_buf fields from
    // kernel_lift_to_uop bind the runtime input/output tables; the
    // rewritten store_root drives compute.  Three per-kernel safety
    // gates (residual-BUFFERIZE, stranded-RANGE, broadcast-input)
    // decide whether the chain-fold flag commits are saved.
    //
    // kernel_lift_to_uop sets cached_lift.store_root = ru_root and
    // returns 0 if ru_root is 0 -- so inside this branch we already
    // know cached_lift.store_root holds the unified pass's root.
    Term ru_rewritten = unified_store_root_for_walker(ke, ke->cached_lift.store_root);
    // Re-apply the broadcast-collapsed REDUCE repair AFTER the inline:
    // a per-channel-mean-style producer fused into a reduce collapses its
    // value to the invariant per-channel scalar, dropping the reduce-axis
    // RANGE leaves so uwalk_run_reduce would bail to the SUM identity 0
    // (the detached-mean live-adjoint at N=1, bias.expand grads, etc.).
    // The lost axis extents are recovered from the PRE-inline store_root.
    {
      AxExtMap _axm; _axm.n = 0;
      axext_collect(&_axm, ke->cached_lift.store_root);
      ru_rewritten = repair_collapsed_reduces_rec(ru_rewritten, &_axm, 0);
    }
    // Scan the rewritten store_root for INDEX_E reads against slots
    // whose tid carries non-trivial layout (chain or non-contig
    // view).  The flag commit is deferred until the bypass-succeeded
    // branch below so a gate-declined kernel doesn't get its pre-mat
    // skipped while still reading via the unfolded store_root.
    ChainFoldMarks _cf_marks;
    _cf_marks.slot_mask_lo = 0;
    _cf_marks.slot_mask_hi = 0;
    ru_rewritten = unified_fold_chain(ke, ru_rewritten, &_cf_marks);
    // Per-kernel safety gates for the unified-bypass:
    //
    //   has_resid:   the cpu_uop_walk INDEX_E handler only resolves
    //                UOP_BUFFER leaves; a residual UOP_BUFFERIZE in
    //                the value tree (an in-kernel intermediate the
    //                rangeify_unified pass didn't realize) reads as
    //                0.0 default and the kernel zeroes its output.
    //
    //   has_stranded: the value subtree references a UOP_RANGE
    //                whose axis_id is neither in the STORE addr nor
    //                in scope of an enclosing UOP_REDUCE.  The
    //                walker sets up loop slots only for addr ranges
    //                + REDUCE axes, so any other RANGE leaf reads
    //                iter=0 forever -- producing only the slice-0
    //                result.  This happens when ru_rewrite_subtree
    //                splices a non-realized producer's RU_SUBST
    //                verbatim into a consumer whose iter space has
    //                different RANGE.axis_ids (the producer's
    //                ru_build_input_addr_for ranges leak through
    //                into the consumer's value tree).  Conv2d
    //                grad-w hits this in the dY*X matmul-back
    //                kernel where one INDEX_E feeds from a non-
    //                realized PERMUTE/RESHAPE chain.
    //
    //   has_bcast:   the rewritten subtree reads from a UOP_BUFFER
    //                input slot whose backing View has a stride-0
    //                (broadcast) axis, negative stride (FLIP) or
    //                a non-row-major stride pattern (PERMUTE /
    //                SHRINK view).  ru_pass builds addresses from
    //                per-axis ranges without consulting strides,
    //                and the input_views[slot].strides per-slot
    //                stride table that would fold the broadcast
    //                axis to CONST(0) isn't reflected in the
    //                rewritten subtree.  Until ru_pass threads
    //                view strides through INDEX_E address
    //                construction, decline the bypass for these
    //                inputs.  Softmax-CE backward hits this where
    //                a 1-element scalar feeds a 3-element consumer
    //                via stride-0 broadcast.
    //
    // When any gate trips, skip the chain-fold commit; store_root
    // still gets the rewritten subtree (the renderer can't name
    // raw TAG_TEN / BUFFERIZE leaves regardless of gate outcome).
    int has_resid    = uop_subtree_has_residual_bufferize(ru_rewritten);
    int has_stranded = uop_subtree_has_stranded_range(ru_rewritten);
    int has_bcast    = uop_subtree_has_broadcast_input(ke, ru_rewritten);
    if (getenv("THVM_DEBUG_BYPASS_LAST")) {
      fprintf(stderr,
              "BYPASS_DBG kid=%u resid=%d stranded=%d bcast=%d\n",
              kid, has_resid, has_stranded, has_bcast);
      bypass_dbg_dump("lift_root", kid, ke->cached_lift.store_root);
      bypass_dbg_dump("ru_rewrit", kid, ru_rewritten);
    }
    BYPASS_KERNEL_TOTAL++;
    if (has_resid)    BYPASS_GATE_RESID++;
    if (has_stranded) BYPASS_GATE_STRANDED++;
    if (has_bcast)    BYPASS_GATE_BCAST++;
    // Always substitute the rewritten store_root.  The raw unified
    // ru_root carries TAG_TEN / UOP_BUFFERIZE leaves that the
    // renderer can't name (falls through to the buf%llu fallback
    // -> undeclared identifier in MSL).  The rewriter replaces
    // those with proper UOP_BUFFER terms carrying input-slot
    // instance, which is benign regardless of gate outcome.
    ke->cached_lift.store_root = ru_rewritten;
    if (!has_resid && !has_stranded && !has_bcast) {
      BYPASS_KERNEL_USED_UNIFIED++;
      unified_fold_chain_commit_flags(ke, &_cf_marks);
    }
    // Post-lift UPatRule pass.  Reads applied_opts via the tile_anno
    // facade (single read site so the eventual KpSchedule -> KernelEntry
    // ownership move is a one-file change).
    KOpt const *m_opts   = tile_anno_applied_opts(ke);
    u32         m_n_app  = tile_anno_applied_opts_count(ke);
    if (ke->cached_lift.store_root && m_opts != NULL && m_n_app > 0) {
      // uop_apply_split_dag composes the split-class entries
      // (UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP) at the UOp DAG level via
      // the uop_range_split primitive, replacing each pre-split
      // UOP_RANGE leaf with the (outer * k + inner) sub-expression.
      //
      // Order: split-DAG runs FIRST (rewires axis-id space + extents),
      // then uop_apply_kernel_opts stamps axis_types via the simulator
      // that accounts for SPLIT shifts.  Running stamp first would
      // stamp pre-split leaves that split-DAG would then replace --
      // losing the stamps.
      Term root_after_split = uop_apply_split_dag(ke->cached_lift.store_root,
                                                  m_opts, m_n_app);
      Term post = uop_apply_kernel_opts(root_after_split, m_opts, m_n_app);
      ke->cached_lift.store_root = post;
    }
  }

  // Dense-renumber axis_ids to 0..n per kernel (the lifter assigns
  // global, sparse, possibly-large ids; downstream wants a fresh 0..n
  // axis space like tinygrad).  Runs here -- after every materialize-
  // time rewrite, before the init snapshot + fire-time hand_opts -- so
  // hand_opts' `axis_id+1` splits stay contiguous on a dense base and
  // the renderer emits dense ids directly (no post-render canonicalize).
  if (ke->cached_lift.store_root != 0) {
    ke->cached_lift.store_root =
        uop_dag_renumber_axes(ke->cached_lift.store_root);
  }

  // Schedule-time explosion guard (debug-only): walk the finalized
  // store_root, multiply every LOOP-typed RANGE leaf in the value
  // subtree that is NOT in the STORE addr (i.e. a stranded loop), and
  // report when the product blows past a threshold.  Lets us diagnose
  // the conv-bwd window-strand WITHOUT dispatching the ~6.8e12-iter
  // kernel that otherwise hangs.  Gated behind THVM_DUMP_STRAND_GUARD.
  if (ke->cached_lift.store_root != 0 && getenv("THVM_DUMP_STRAND_GUARD")) {
    u64 strand_prod = strand_loop_product(ke->cached_lift.store_root);
    u64 total_prod  = total_iter_product(ke->cached_lift.store_root);
    if (strand_prod >= 100ull || total_prod >= 100000000ull) {
      fprintf(stderr,
              "STRAND_GUARD kid=%u op=%s strand_loop_product=%llu "
              "total_iter_product=%llu\n",
              kid, bypass_dbg_op_name(op), (unsigned long long)strand_prod,
              (unsigned long long)total_prod);
      bypass_dbg_dump("strand_root", kid, ke->cached_lift.store_root);
    }
  }

  // Snapshot the post-materialize / pre-runtime-opt cached_lift state
  // so axes_reset_to_default can revert kernel_apply_opt's DAG
  // mutations during autotune's bench-each-variant flow.
  ke->cached_lift_init_root = ke->cached_lift.store_root;

  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, out_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, DT_INT32, kid));
  Term kernel_term = term_new(0, TAG_UOP, UOP_KERNEL, kloc);

  // Pin a heap cell carrying the UOP_KERNEL Term itself so heap-walk
  // discovery (e.g. THeapDiagram, gc_mark) sees every emitted kernel,
  // not only the sink that gets returned to the WL handle.  Without
  // this pin, only the sink kernel_term is reachable (via the WL
  // surface Term); the non-sink kernels' Terms live in BOUNDARY_TERM[]
  // C-side scratch and never become heap-resident.  Cost: one Term
  // (~8B) per emitted kernel; the pinned cell is read-only (the Term
  // it holds is identical to BOUNDARY_TERM[bi], no aliasing concern).
  u64 pin = heap_alloc(1);
  heap_set(pin, kernel_term);

  BOUNDARY_TID [bi] = out_tid;
  BOUNDARY_TERM[bi] = kernel_term;

  // Record this output buf so a later-depth emit can recycle it.
  // last_use_depth = 0 means "no consumer is itself a realize
  // boundary" -- typically the realize root + any preserved orphan;
  // those never get pushed (the threshold mem_plan_push_dead checks
  // is `last_use < current_depth`, which 0 satisfies for any
  // current_depth >= 1, so we'd freelist-push the root and the
  // caller would read freed bytes).  Skip recording in that case.
  // Recycle only single-consumer outputs.  Multi-consumer ones may
  // be aliased through DUP/SUP / read by interactions outside the
  // realize-info-tracked DAG (e.g. UOP_ASSIGN in optimizer loops),
  // and their true last_use isn't always equal to BOUNDARY_LAST_USE.
  // The Phase-3 fusion relaxation also lets some non-realized
  // intermediates feed multiple consumers without a shared buf;
  // restricting recycling here keeps those cases safe.  Lifting
  // this guard requires explicit ASSIGN + DUP-aware lifetime
  // tracking in the planner.
  if (BOUNDARY_LAST_USE[idx] > 0
      && BUFFERIZE_NODES[idx].consumer_count == 1) {
    mem_plan_record(TENS[out_tid].buf_id,
                    BOUNDARY_LAST_USE[idx],
                    CURRENT_BACKEND);
  }

  if (getenv("THVM_DUMP_KERNEL_SHAPE")) {
    fprintf(stderr, "KSHAPE kid=%u op=%s shape=[", kid, bypass_dbg_op_name(op));
    for (u32 i = 0; i < out_shape.ndim; i++) {
      fprintf(stderr, "%s%u", i ? "," : "", out_shape.dims[i]);
    }
    fprintf(stderr, "] depth=%u\n", this_depth);
  }

  // Persist loc -> tid so a later realize pass that walks the same
  // UOP loc (a shared forward intermediate referenced by multiple
  // grad targets) substitutes to a TAG_TEN leaf instead of
  // re-emitting an identical kernel.  See MATERIALIZED_LOC_TABLE
  // comment above for full rationale.
  materialized_loc_insert(boundary_loc, out_tid);

  return kernel_term;
}

// Direct kernelize entry called by the surviving view tests:
// for the 5 view-only movement ops, return the alias TenDesc as a
// TAG_TEN; for everything else (PAD, elementwise, reduce, kernel)
// fall through to the normal kernel-emit path.
fn Term materialize_uop_in_env(Term t, u32 env_id) {
  (void)env_id;
  if (term_tag(t) == TAG_UOP) {
    u8 op = term_ext(t);
    if (op_is_view_movement(op)) {
      u32 alias_tid = view_resolve(t);
      if (alias_tid != 0)
        return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
    }
  }
  return thvm_materialize(t);
}

// Recursive descent: walk a UOP DAG looking for UOP_ASSIGN nodes
// at any depth.  Each ASSIGN's src subgraph is materialized in
// place (heap_set on cell+1) so the surrounding kernel-emission
// pass sees a kernel chain producing a TEN, not a raw UOP graph.
// Bottoms out at non-UOP tags and at UOP_KERNEL (already materialized).
// Idempotent across re-entries via the early returns inside
// thvm_materialize.
static void materialize_inner_assigns_rec(Term term, u8 *visited, u64 cap) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  u8 ar = uop_arity((u8)op);
  if (ar == 0) return;
  u64 loc = term_val(term);
  // Memoize per node: shared DAG nodes (residual fan-in, stacked-backward)
  // must be walked once, not once per incoming edge -- else exponential in
  // depth.  ASSIGN-src materialization is idempotent, so a second visit is
  // pure redundant re-walk.  Same bitmap pattern as strip_detach / subst.
  if (loc < cap && visited[loc]) return;
  if (loc < cap) visited[loc] = 1;
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_ASSIGN) {
      u64  cloc     = term_val(child);
      Term csrc     = heap_read(cloc + 1);
      Term csrc_mat = thvm_materialize(csrc);
      if (csrc_mat != csrc) heap_set(cloc + 1, csrc_mat);
    } else {
      materialize_inner_assigns_rec(child, visited, cap);
    }
  }
}

static void materialize_inner_assigns(Term term) {
  if (term_tag(term) != TAG_UOP) return;
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;  // OOM: skip (re-entries stay idempotent)
  materialize_inner_assigns_rec(term, visited, cap);
  free(visited);
}

// Pre-walk: replace every UOP_COPY child embedded in the DAG with its
// materialized TAG_TEN (identity src TEN on same-backend realize, or
// the uploaded device buffer on cross-backend).  Runs BEFORE
// bufferize_classify so the boundary classifier + kernel emitter see a
// plain external tensor input wherever a weight was wrapped in a COPY
// -- exactly as they would for a bare TTensorCreate input.  Same
// per-node memoized walk as materialize_inner_assigns.  Mirror:
// tinygrad lowers COPY to a buffer the downstream kernel reads as an
// input (engine/realize.py exec_copy -> buffers[dest]).
static void materialize_inner_copies_rec(Term term, u8 *visited, u64 cap) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  u8 ar = uop_arity((u8)op);
  if (ar == 0) return;
  u64 loc = term_val(term);
  if (loc < cap && visited[loc]) return;
  if (loc < cap) visited[loc] = 1;
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_COPY) {
      Term ten = materialize_copy(child);
      if (ten != child) heap_set(loc + i, ten);
    } else {
      materialize_inner_copies_rec(child, visited, cap);
    }
  }
}

static void materialize_inner_copies(Term term) {
  if (term_tag(term) != TAG_UOP) return;
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;  // OOM: skip (re-entries stay idempotent)
  materialize_inner_copies_rec(term, visited, cap);
  free(visited);
}

// Recursively strip UOP_DETACH(x) -> x in place across a UOP DAG, so the
// stop-gradient marker never reaches kernel lifting / render / the walker
// (the gated uop_graph_simplify can't be relied on).  Detach is identity
// at the value level; the backward was already built before materialize.
// Resolves chains of DETACH and rewrites each parent slot in place.
static Term materialize_strip_detach_rec(Term term, u8 *visited, u64 cap) {
  for (int hops = 0; hops < 64 && term_tag(term) == TAG_UOP
                     && term_ext(term) == UOP_DETACH; hops++) {
    term = heap_read(term_val(term) + 0);
  }
  if (term_tag(term) != TAG_UOP) return term;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return term;
  u64 loc = term_val(term);
  // Memoize the child-strip per node: a DAG node shared by N parents
  // (residual activations, stacked-backward fan-in) must be walked ONCE,
  // not once per incoming edge -- else the walk is exponential in graph
  // depth (a 2-layer transformer backward never finishes).  Children are
  // stripped in place, so a second visit is pure redundant re-walk.
  // Mirrors materialize_subst_cached_rec's visited bitmap.
  if (loc < cap && visited[loc]) return term;
  if (loc < cap) visited[loc] = 1;
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    Term stripped = materialize_strip_detach_rec(child, visited, cap);
    if (stripped != child) heap_set(loc + i, stripped);
  }
  return term;
}

static Term materialize_strip_detach(Term term) {
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) {  // OOM: skip memoization, still de-DETACH the root
    for (int hops = 0; hops < 64 && term_tag(term) == TAG_UOP
                       && term_ext(term) == UOP_DETACH; hops++)
      term = heap_read(term_val(term) + 0);
    return term;
  }
  Term r = materialize_strip_detach_rec(term, visited, cap);
  free(visited);
  return r;
}

fn Term thvm_materialize(Term term) {
  term = materialize_strip_detach(term);
  HOT_MATERIALIZE_CALLS++;
  // REF / ALO transparency: jump (don't unfold) into the body cell.
  // term_resolve walks VAR-SUB and ALO chains -- pure pointer hops,
  // no heap allocation.  TAG_REF jumps directly to DEFS[name], the
  // book-heap pointer registered by TDef -- still no allocation, no
  // rewriting of the original term, just reading where the body
  // lives.  Cap at 8 hops as a safety net against degenerate
  // self-referencing REF chains; in practice 1-2 suffices.
  for (int hops = 0; hops < 8; hops++) {
    Term resolved = term_resolve(term);
    if (term_tag(resolved) == TAG_REF) {
      u32 name = term_ext(resolved);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) break;
      term = book;
      continue;
    }
    if (resolved == term) break;
    term = resolved;
  }

  // TAG_CTR (multi-target grad bundle): materialize each child
  // independently and rebuild the CTR.  Pure structural recursion;
  // the children themselves are normal UOp graphs (or already TAG_TEN
  // for grad components that wnf already reduced).
  if (term_tag(term) == TAG_CTR) {
    u32 n = term_ctr_n(term);
    if (n > 256) return term;
    Term children[256];
    for (u32 i = 0; i < n; i++)
      children[i] = thvm_materialize(term_ctr_at(term, i));
    return term_new_ctr(term_ext(term), children, n);
  }
  // Compound IC nodes (APP/LAM/SUP/DUP/OP2/MAT/ALO): walk children
  // in-place so a single TMaterialize call on a full recursive
  // training term -- e.g. TPri[loss, ASSIGN(...)] = APP(APP(APP(PRI),
  // loss), step) at root, or TLam[k, body] for the loop wrapper --
  // descends to find the embedded UOP graphs and compiles each one.
  // Children are materialized in place (heap_set on the original
  // cell) so the surrounding structure stays intact for wnf to drive
  // at fire time.  Atoms (NUM/TEN/REF/ERA/VAR) are leaves -- nothing
  // to materialize, recursion bottoms out via the early returns above.
  {
    u8 t = term_tag(term);
    u32 ar = 0;
    switch (t) {
      case TAG_APP: case TAG_SUP: case TAG_OP2: case TAG_MAT:
      case TAG_ALO:
        ar = 2; break;
      case TAG_LAM: case TAG_DUP:
        ar = 1; break;
      default: ar = 0; break;
    }
    if (ar > 0) {
      u64 loc = term_val(term);
      for (u32 i = 0; i < ar; i++) {
        Term child = heap_read(loc + i);
        Term child_mat = thvm_materialize(child);
        if (child_mat != child) heap_set(loc + i, child_mat);
      }
      return term;
    }
  }
  if (term_tag(term) != TAG_UOP)        return term;
  if (term_ext(term) == UOP_KERNEL)     return term;

  // Cross-realize alias short-circuit.  If a previous realize pass
  // already emitted a kernel for the UOP at this heap loc (a shared
  // forward intermediate referenced by multiple grad targets, the
  // dominant cross-realize over-emission pattern), return the prior
  // kernel's output as a TAG_TEN leaf so we don't re-bufferize or
  // re-emit an identical kernel.  See MATERIALIZED_LOC_TABLE.
  // Cross-realize cache short-circuits only fire when called from
  // inside a realize wrapper (thvm_realize / thvm_realize_many).
  // Tests that drive thvm_materialize directly want the unsubstituted
  // path; the scope counter gates this.
  if (materialized_loc_scope_depth() > 0) {
    u32 cached_tid = materialized_loc_lookup(term_val(term));
    if (cached_tid != 0) {
      return term_new(0, TAG_TEN, TENS[cached_tid].dtype, cached_tid);
    }
    // Pre-substitute cached UOP descendants with TAG_TEN leaves so
    // the bufferize_classify walk that follows sees an "external"
    // tensor wherever a previous realize pass produced one.  Without
    // this, an interior UOP at a cached loc would otherwise be
    // re-classified / re-emitted as a fresh boundary even though its
    // kernel and output buffer already exist from the prior pass.
    // Mirror: tinygrad's pm_generate_realize_map memoizes UOp ->
    // Buffer on the lazy graph and downstream schedules read the
    // prior Buffer.
    materialize_subst_cached_inplace(term);
  }
  // DETACH is a stop-gradient marker with identity runtime semantics;
  // by materialize time the backward is already built, so unwrap it to
  // its src (no kernel/render/walker path ever needs to know about it).
  if (term_ext(term) == UOP_DETACH) {
    return thvm_materialize(heap_read(term_val(term) + 0));
  }
  // UOP_COPY: lazy device transfer.  Identity on same-backend realize
  // (CPU realize of a CPU host leaf -> returns the src TEN, no kernel),
  // host-staged upload + cache on cross-backend.  See materialize_copy.
  if (term_ext(term) == UOP_COPY) {
    return materialize_copy(term);
  }
  // GRAD is a stop point in materialize -- wnf fires interact_grad,
  // then thvm_realize loops back here to compile the unrolled UOps.
  // ASSIGN is a wnf-fired primitive (interact_assign) -- not a kernel.
  // Materialize the SRC subgraph so its kernels are compiled, then
  // re-wrap as ASSIGN(dst, materialized_src).  Wnf later fires the
  // src kernels, lands a TEN in heap[loc+1], and interact_assign
  // memcpys it into dst.buf.
  if (term_ext(term) == UOP_ASSIGN) {
    u64  loc        = term_val(term);
    // KV-cache append: the ASSIGN's DST is a SHRUNK view of a persistent
    // cache (tinygrad's `cache[..., start_pos:start_pos+T, ...]`).  A
    // movement-op dst never reduces on its own (interact_assign bails
    // forever -- dst isn't a TAG_TEN), so resolve the movement chain to
    // a TAG_TEN view-alias here: tensor_view_of shares the cache buffer
    // and stamps view.offset/strides from the SHRINK.  interact_assign
    // then writes the src bytes at that offset (FIX-2).  Scoped to a
    // movement-op dst, so an in-place weight ASSIGN (TAdam etc., dst
    // already TAG_TEN) is byte-unaffected.
    Term dst_cell = heap_read(loc + 0);
    if (term_tag(dst_cell) == TAG_UOP && op_is_view_movement(term_ext(dst_cell))) {
      u32 dst_alias = view_resolve(dst_cell);
      if (dst_alias != 0) {
        heap_set(loc + 0, term_new(0, TAG_TEN, TENS[dst_alias].dtype, dst_alias));
      }
    }
    Term src_cell   = heap_read(loc + 1);
    Term src_mat    = thvm_materialize(src_cell);
    if (src_mat != src_cell) heap_set(loc + 1, src_mat);
    return term;
  }
  // Pre-walk: recursively scan the UOP DAG for NESTED ASSIGNs and
  // materialize each one's src subgraph in place.  Without this, an
  // ASSIGN buried inside the src of an outer ASSIGN (or several
  // levels deep in a compound UOP -- e.g. Adam's
  // `w - lr * mAfter / denom`) keeps a raw UOP src that wnf can't
  // reduce to a TEN, so the ASSIGN never fires.
  materialize_inner_assigns(term);
  // Resolve UOP_COPY children to their (uploaded) TAG_TEN before the
  // boundary classifier runs -- see materialize_inner_copies.
  materialize_inner_copies(term);

  Term simplified = uop_graph_simplify_materialize(term, 0);
  if (simplified != term) {
    return thvm_materialize(simplified);
  }

  if (term_ext(term) == UOP_CONST) {
    u32 tid = const_to_tendesc(term_val(term));
    return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
  }

  // Movement-op root: resolve the chain to an alias TenDesc, then
  // flatten to a contig copy so wnf-side flat reads work.
  if (op_is_view_movement(term_ext(term))) {
    u32 alias_tid = view_resolve(term);
    // A ShapeTracker-CHAINED alias (nviews > 0, from a merge-reshape
    // that view_apply_reshape couldn't absorb) whose underlying buffer
    // is a kernel output is NOT safe to eagerly gather here:
    // materialize_root_alias reads the buffer immediately, but the
    // producer kernel only fires later (in wnf), so the gather would
    // read uninitialised bytes (the f4db9637 "movement-op-over-kernel-
    // output evaluated to zero" failure mode).  For that case, fall
    // through to the kernel-op-emit path -- it emits a kernel that
    // wnf fires AFTER the producer, keeping the producer reachable.
    // Static-data sources (producer_kid == 0 -- external tensors, the
    // abort-repro case) gather safely.  Non-chained aliases (nviews ==
    // 0) keep the existing behaviour.
    if (alias_tid != 0) {
      TenDesc const *ad = &TENS[alias_tid];
      int unsafe_chain_root = (ad->nviews > 0 && ad->producer_kid != 0);
      if (!unsafe_chain_root) {
        Term alias_term = term_new(0, TAG_TEN, ad->dtype, alias_tid);
        // Under TJit capture, record the host gather as a replayable
        // JIT_OP_GATHER so it re-runs off the live source each step
        // (the one-shot capture gather otherwise bakes the capture-step
        // bytes -- stale on replay when the source is a re-firing reduce
        // broadcast or an append-mutated KV cache).  Idempotent for
        // genuinely static sources (re-copies identical bytes).
        return materialize_root_alias_rec(alias_term, jit_is_capturing());
      }
    }
  }

  bufferize_classify(term);
  // The unified rangeify pass projects its UOP_BUFFERIZE Terms back
  // onto BUFFERIZE_NODES.realized; we capture them into
  // BOUNDARY_BUFFERIZE_TERM[] here.  Mirror: tinygrad walks the
  // lowered tsink for BUFFERIZE+STORE pairs (tinygrad/engine/realize.py).
  topo_sort_buffers_unified(term);
  mem_plan_reset();
  // Arena planner: pre-compute per-boundary offsets into a shared
  // arena buf.  Mirror: tinygrad/schedule/memory.py memory_plan_rewrite
  // (called inside lower_schedule_item before kernel dispatch).
  arena_compute();
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    BOUNDARY_TID [i] = 0;
    BOUNDARY_TERM[i] = 0;
  }
  // All-or-nothing emission: if any boundary fails to compile,
  // rewind KERNELS_NEXT and TENS_NEXT to their pre-call values
  // and return the input unchanged.  Without this rewind, partial
  // emission accumulates orphan kernels per call -- thvm_realize's
  // loop sees the same UOp graph each iteration and re-emits the
  // same successful boundaries, growing KERNELS_NEXT linearly with
  // iter count (the symptom that produced 1k+ kernels for a
  // softmax+CE backward).
  u32 kernels_at_start = KERNELS_NEXT;
  u32 tens_at_start    = TENS_NEXT;
  Term sink_kernel = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    Term k = emit_kernel_for_boundary(i);
    if (k == 0) {
      // Rewind KERNELS_NEXT, freeing per-kernel heap arrays for the
      // kernels emitted in this attempt (each kernel_alloc grew the
      // input/program arrays via realloc; without this loop they'd
      // leak).
      for (u32 r = kernels_at_start; r < KERNELS_NEXT; r++)
        kernel_free_arrays(&KERNELS[r]);
      KERNELS_NEXT = kernels_at_start;
      TENS_NEXT    = tens_at_start;
      mem_plan_drain_freelist();
      // Arena views allocated during the partial emit are dropped by
      // the TENS_NEXT rewind; the arena CpuBuf itself (allocated
      // lazily) survives in CPU_BUFS until pool_rollback reclaims it.
      // Drop the +1 sentinel here too so the arena can be reclaimed
      // (the view-refcount adds 1 per view; rewinding TENS doesn't
      // decref those views, so without the sentinel drop the arena
      // would leak permanently on every bail).  Note: TENS_NEXT
      // rewind discards the view CpuBufs along with their TenDescs --
      // but those views' parent_buf_id->arena refcounts were already
      // bumped when they were allocated and we don't have a per-view
      // decref hook in the rewind loop.  This is a transient leak per
      // failed pass (rare); the arena_reset below clears the static
      // pointers so the next pass starts fresh.
      if (ARENA_BUF_ID != 0) {
        if (CURRENT_BACKEND == &CPU_BACKEND
            && ARENA_BUF_ID < CPU_BUFS_NEXT) {
          cpu_buf_decref(ARENA_BUF_ID);
        }
#ifdef THVM_HAS_CUDA
        else if (CURRENT_BACKEND == &CUDA_BACKEND
                 && ARENA_BUF_ID < CUDA_BUFS_NEXT) {
          cuda_buf_decref(ARENA_BUF_ID);
        }
#endif
      }
      arena_reset();
      return term;
    }
    if (BOUNDARY_ORDER[i] == term_val(term)) sink_kernel = k;
  }
  // End-of-pass: pop any planner-pushed bufs still on CPU_FREELIST so
  // a subsequent thvm_realize -> materialize iteration doesn't pull
  // from them (the chain rule's freshly-emitted UOPs may still
  // reference those tids).
  mem_plan_drain_freelist();
  // Arena: drop the +1 sentinel refcount that buf_alloc gave the
  // arena buf at first-view time, so the arena is released the moment
  // its last view decrefs.  Each arena view incref'd it by 1; total
  // refcount is (1 + N_live_views).  After this decref it's exactly
  // N_live_views, which drops naturally as TenDescs release.  Without
  // this drop the arena leaks until session shutdown (every realize
  // accumulates a permanent arena).
  if (ARENA_BUF_ID != 0) {
    if (CURRENT_BACKEND == &CPU_BACKEND
        && ARENA_BUF_ID < CPU_BUFS_NEXT) {
      cpu_buf_decref(ARENA_BUF_ID);
    }
#ifdef THVM_HAS_CUDA
    else if (CURRENT_BACKEND == &CUDA_BACKEND
             && ARENA_BUF_ID < CUDA_BUFS_NEXT) {
      cuda_buf_decref(ARENA_BUF_ID);
    }
#endif
  }
  if (getenv("THVM_ARENA_DUMP")) {
    fprintf(stderr,
            "arena: end-of-pass arena_allocs=%u legacy_allocs=%u\n",
            ARENA_ALLOCS_ARENA, ARENA_ALLOCS_LEGACY);
  }
  ARENA_DATA   = NULL;
  ARENA_BUF_ID = 0;
#ifdef THVM_HAS_CUDA
  ARENA_DPTR   = 0;
#endif
  // Diagnostic: print per-kernel input edge data for every kernel
  // emitted in this pass.  No-op unless
  // DUMP_BUFFERIZE_KERNEL_EDGES=1.
  materialize_dump_kernel_edges(kernels_at_start);
  return sink_kernel != 0 ? sink_kernel : term;
}
