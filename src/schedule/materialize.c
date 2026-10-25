// schedule/materialize.c - the scheduler: a realize root's UOp graph to
// kernels.
//
// bufferize_classify (which runs the unified rangeify pass) picks the realize
// boundaries; topo_sort_boundaries puts them in fire order in BOUNDARY_ORDER;
// emit_kernel_for_boundary emits a KernelEntry per boundary (visit() records
// the input bindings, kernel_lift_to_uop builds cached_lift.store_root, the
// walker rewrite binds its leaves to input slots).  An unsupported op makes
// visit return VISIT_BAIL, and thvm_materialize then rewinds and returns its
// input unchanged.  The per-pass memory planners and the cross-pass and
// cross-realize caches live here too.

// Recursion budget of the graph walks and rewrites below.  Past it a walk
// returns a wrong result (the term unchanged, 0 or an empty set) instead of
// failing, so it must exceed the depth of any graph that gets fused.
#define MAT_MAX_WALK_DEPTH 65536u

// Defined in jit/capture.c, which the unity build includes after this file.
// The movement-root gather asks whether a capture is recording and records a
// replayable JIT_OP_GATHER, so a replay re-runs the host gather off the live
// source.
fn int  jit_is_capturing(void);
fn void jit_capture_record_gather(u32 src_tid, u32 dst_buf);
// The capture-recycle pool (jit/capture.c, Metal only): push parks a dead
// captured intermediate for same-size reuse, pop adopts one for a captured
// kernel output, drain forgets the survivors at each pass end, as
// mem_plan_drain_freelist does for the freelist.  Outside a capture all three
// do nothing.
fn int  jit_caprecycle_push(Backend *b, u32 buf_id);
fn u32  jit_caprecycle_pop(Backend *b, u64 nbytes);
fn void jit_caprecycle_drain(void);

#define BOUNDARY_ORDER_CAP 16384
static u64  BOUNDARY_ORDER[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_TID  [BOUNDARY_ORDER_CAP];   // emitted output TenDesc id
static Term BOUNDARY_TERM [BOUNDARY_ORDER_CAP];   // emitted UOP_KERNEL term

// Per boundary, the UOP_BUFFERIZE the rangeify pass emitted there
// (RU_BUFFERIZE_TERM), or 0.  Set by topo_sort_boundaries;
// emit_kernel_for_boundary copies it to ke->compute_bufferize.  tinygrad puts a
// UOp(Ops.STAGE) at each realize (schedule/indexing.py:92).
static Term BOUNDARY_BUFFERIZE_TERM[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_ORDER_LEN = 0;

// fp8-cast transients (BUFFERIZE_REASON_FP8_TRANSIENT): the bf16 weight a
// force-realized fp8 -> bf16 cast produces, read only by the matmul reduce that
// forced it.  Recorded at emit and freed at the end of the realize
// (thvm_free_fp8_transients); otherwise mark_heap_rooted_preserve's raw heap
// scan finds the kloc+0 TAG_TEN cell and keeps the buffer across realizes.
#define FP8_TRANSIENT_CAP 4096
static u32 FP8_TRANSIENT_TIDS [FP8_TRANSIENT_CAP];
static u64 FP8_TRANSIENT_KLOCS[FP8_TRANSIENT_CAP];  // kloc+0 holds TAG_TEN(tid)
static u64 FP8_TRANSIENT_PINS [FP8_TRANSIENT_CAP];  // kernel-term pin cell
static u32 FP8_TRANSIENT_LEN = 0;

// Open-addressed loc -> BOUNDARY_ORDER index hash behind
// boundary_index_for_loc, which visit() calls for every child it reaches.
#define BOUNDARY_HASH_CAP   (1u << 16)        // 4x BOUNDARY_ORDER_CAP
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

// Open-addressed BUFFERIZE Term -> BOUNDARY_ORDER index hashes for
// unified_rewrite_buffer_for_bufferize{,_extend} and the axis-table inline,
// filled by topo_sort_boundaries:
//   BOUNDARY_BUF_TERM_HASH   keyed by the BUFFERIZE Term itself
//   BOUNDARY_BUF_VALUE_HASH  keyed by its value subtree (two BUFFERIZEs of one
//                            value are one buffer)
static u32 BOUNDARY_BUF_TERM_HASH [BOUNDARY_HASH_CAP];
static u32 BOUNDARY_BUF_VALUE_HASH[BOUNDARY_HASH_CAP];

static inline u32 boundary_term_hash_of(Term t) {
  u64 v = (u64)t;
  v ^= v >> 33; v *= 0xff51afd7ed558ccdULL;
  v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL;
  v ^= v >> 33;
  return (u32)v & (BOUNDARY_HASH_CAP - 1);
}

static void boundary_buf_hash_clear(void) {
  for (u32 i = 0; i < BOUNDARY_HASH_CAP; i++) {
    BOUNDARY_BUF_TERM_HASH[i]  = BOUNDARY_HASH_EMPTY;
    BOUNDARY_BUF_VALUE_HASH[i] = BOUNDARY_HASH_EMPTY;
  }
}

// Each key keeps the first idx inserted.  value_term 0 (no value subtree) skips
// the value index.
static void boundary_buf_hash_insert(Term buf_term, Term value_term, u32 idx) {
  if (buf_term != 0) {
    u32 h = boundary_term_hash_of(buf_term);
    for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
      u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
      if (BOUNDARY_BUF_TERM_HASH[i] == BOUNDARY_HASH_EMPTY) {
        BOUNDARY_BUF_TERM_HASH[i] = idx; break;
      }
      // Already present: keep the first index.
      if (BOUNDARY_BUFFERIZE_TERM[BOUNDARY_BUF_TERM_HASH[i]] == buf_term) break;
    }
  }
  if (value_term != 0) {
    u32 h = boundary_term_hash_of(value_term);
    for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
      u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
      if (BOUNDARY_BUF_VALUE_HASH[i] == BOUNDARY_HASH_EMPTY) {
        BOUNDARY_BUF_VALUE_HASH[i] = idx; break;
      }
      // Another boundary of the same value is already mapped: keep the first.
      Term existing = BOUNDARY_BUFFERIZE_TERM[BOUNDARY_BUF_VALUE_HASH[i]];
      if (existing != 0 && uop_bufferize_value(existing) == value_term) break;
    }
  }
}

// The BOUNDARY_ORDER index whose BUFFERIZE Term is buf, or 0xFFFFFFFF.
static u32 boundary_buf_term_index(Term buf) {
  if (buf == 0) return 0xFFFFFFFFu;
  u32 h = boundary_term_hash_of(buf);
  for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
    u32 idx = BOUNDARY_BUF_TERM_HASH[i];
    if (idx == BOUNDARY_HASH_EMPTY) return 0xFFFFFFFFu;
    if (idx < BOUNDARY_ORDER_LEN && BOUNDARY_BUFFERIZE_TERM[idx] == buf) return idx;
  }
  return 0xFFFFFFFFu;
}

// The BOUNDARY_ORDER index of a BUFFERIZE other than exclude whose value is
// want_value, or 0xFFFFFFFF.
static u32 boundary_buf_value_index(Term want_value, Term exclude) {
  if (want_value == 0) return 0xFFFFFFFFu;
  u32 h = boundary_term_hash_of(want_value);
  for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
    u32 idx = BOUNDARY_BUF_VALUE_HASH[i];
    if (idx == BOUNDARY_HASH_EMPTY) return 0xFFFFFFFFu;
    if (idx < BOUNDARY_ORDER_LEN) {
      Term cand = BOUNDARY_BUFFERIZE_TERM[idx];
      if (cand != 0 && cand != exclude
          && term_tag(cand) == TAG_UOP && term_ext(cand) == UOP_BUFFERIZE
          && uop_bufferize_value(cand) == want_value)
        return idx;
    }
  }
  return 0xFFFFFFFFu;
}

// loc -> tid cache: once emit_kernel_for_boundary realizes the UOp at heap loc
// into TENS[tid], a later sub-pass of the realize (or a later realize in a JIT
// span) that reaches loc substitutes TAG_TEN(tid) instead of re-emitting.
// tinygrad rewrites each live Tensor's realized UOp to its storage instead
// (tensor.py:204-208).  Cleared at each realize's start and end (realize.c),
// re-keyed across a heap collection (materialized_loc_gc_after).
#define MATERIALIZED_LOC_CAP    (1u << 17)
#define MATERIALIZED_LOC_EMPTY  0
typedef struct {
  u64 loc;
  u32 tid;
} MaterializedLocEntry;
static MaterializedLocEntry MATERIALIZED_LOC_TABLE[MATERIALIZED_LOC_CAP];
static u32                  MATERIALIZED_LOC_LEN = 0;

// Cross-sub-pass shared buffers.  A TAG_CTR bundle materializes one child per
// thvm_materialize sub-pass.  A boundary two or more roots reach
// (xpass_is_shared) is cached by the first sub-pass and recorded there with
// MEM_PLAN_LAST_USE_NEVER, so that pass never recycles bytes a sibling still
// reads; the last consuming sub-pass frees it after its last reader
// (xpass_shared_input_plan), as tinygrad frees after a buffer's last appearance
// in the whole schedule (schedule/memory.py:28-32, 44).  This table holds those
// tids until materialized_loc_clear wipes the cache: last_root is 1 + the
// highest CTR child index reaching the boundary (only that root's pass frees
// it), loc is the boundary loc.
#define XPASS_SHARED_TID_CAP  4096
typedef struct { u32 tid; u32 last_root; u64 loc; } XPassSharedTid;
static XPassSharedTid XPASS_SHARED_TIDS[XPASS_SHARED_TID_CAP];
static u32 XPASS_SHARED_TID_LEN = 0;

static void xpass_shared_tid_add(u32 tid, u32 last_root, u64 loc) {
  if (tid == 0 || XPASS_SHARED_TID_LEN >= XPASS_SHARED_TID_CAP) return;
  for (u32 i = 0; i < XPASS_SHARED_TID_LEN; i++)
    if (XPASS_SHARED_TIDS[i].tid == tid) return;
  XPASS_SHARED_TIDS[XPASS_SHARED_TID_LEN].tid       = tid;
  XPASS_SHARED_TIDS[XPASS_SHARED_TID_LEN].last_root = last_root;
  XPASS_SHARED_TIDS[XPASS_SHARED_TID_LEN].loc       = loc;
  XPASS_SHARED_TID_LEN++;
}

static void xpass_shared_tid_reset(void) { XPASS_SHARED_TID_LEN = 0; }

// Heap-rewrite journal.  materialize_subst_cached_rec overwrites cells of the
// shared hash-consed heap with TAG_TEN leaves, but the cache behind them lasts
// one realize, after which the leaves would name tids whose buffers the
// rollback reclaimed.  Every rewrite is journaled and undone when the cache is
// cleared; only UOp-produced intermediates are substituted, so undoing all of
// them is exact.
#define SUBST_JOURNAL_CAP  (1u << 16)
typedef struct {
  u64  cell;     // heap cell that was overwritten (loc + child index)
  Term orig;     // original child term to restore
} SubstJournalEntry;
static SubstJournalEntry SUBST_JOURNAL[SUBST_JOURNAL_CAP];
static u32               SUBST_JOURNAL_LEN = 0;

// Journal a heap rewrite for materialized_loc_clear to undo.  0 when the
// journal is full: the caller must then not rewrite the cell.
static int subst_journal_record(u64 cell, Term orig) {
  if (SUBST_JOURNAL_LEN >= SUBST_JOURNAL_CAP) return 0;
  SUBST_JOURNAL[SUBST_JOURNAL_LEN].cell = cell;
  SUBST_JOURNAL[SUBST_JOURNAL_LEN].orig = orig;
  SUBST_JOURNAL_LEN++;
  return 1;
}

static void subst_journal_restore(void) {
  // Newest first, so a cell rewritten twice ends at its first original.
  for (u32 i = SUBST_JOURNAL_LEN; i > 0; i--) {
    heap_set(SUBST_JOURNAL[i - 1].cell, SUBST_JOURNAL[i - 1].orig);
  }
  SUBST_JOURNAL_LEN = 0;
}

// Cross-pass consumer count.  bufferize_classify runs per subgraph (a CTR
// child, an ASSIGN src), so a forward activation that two sibling subgraphs
// read looks single-use to the first, whose planner would recycle it while a
// later sibling reads the cached tid.  tinygrad plans over one linear schedule
// of the whole step (schedule/memory.py:28-32).  XPASS_CC counts the distinct
// top-level roots that reach each UOp loc (a fresh bitmap per root), and
// arena_compute keeps a shared boundary's slot until its pass ends.
#define XPASS_CC_CAP   (1u << 17)        // open-addressed by loc
//   count      top-level roots that reach loc; 2 or more = shared
//   last_root  1 + the highest CTR child index that reaches loc (0 = unset),
//              the last consuming sub-pass
//   held       a realize root, or a buffer a root's movement chain views: the
//              caller gets it back, so no planner recycles it (tinygrad's
//              held_bufs: schedule/memory.py:12-13, schedule/__init__.py:303).
//              A root another root reads has an in-pass consumer, which the
//              last_use == 0 sink test misses.
typedef struct { u64 loc; u32 count; u32 last_root; u8 held; } XPassCCEntry;
static XPassCCEntry XPASS_CC[XPASS_CC_CAP];
static u32          XPASS_CC_LEN    = 0;
static u8           XPASS_CC_ACTIVE = 0;  // 1 once a realize populated it

static inline u32 xpass_cc_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (XPASS_CC_CAP - 1);
}

// Count one more root reaching loc (the caller's per-root bitmap dedups within
// a root) and raise its last_root to root_idx + 1.
static void xpass_cc_bump(u64 loc, u32 root_idx) {
  if (XPASS_CC_LEN * 2 > XPASS_CC_CAP) return;   // past half full: stop counting
  u32 h = xpass_cc_hash(loc);
  u32 rr = root_idx + 1;
  for (u32 probe = 0; probe < XPASS_CC_CAP; probe++) {
    u32 i = (h + probe) & (XPASS_CC_CAP - 1);
    if (XPASS_CC[i].loc == 0 && XPASS_CC[i].count == 0) {
      XPASS_CC[i].loc = loc; XPASS_CC[i].count = 1;
      XPASS_CC[i].last_root = rr; XPASS_CC_LEN++; return;
    }
    if (XPASS_CC[i].loc == loc) {
      XPASS_CC[i].count++;
      if (rr > XPASS_CC[i].last_root) XPASS_CC[i].last_root = rr;
      return;
    }
  }
}

static XPassCCEntry *xpass_cc_find(u64 loc) {
  if (!XPASS_CC_ACTIVE) return NULL;
  u32 h = xpass_cc_hash(loc);
  for (u32 probe = 0; probe < XPASS_CC_CAP; probe++) {
    u32 i = (h + probe) & (XPASS_CC_CAP - 1);
    if (XPASS_CC[i].loc == 0 && XPASS_CC[i].count == 0) return NULL;
    if (XPASS_CC[i].loc == loc) return &XPASS_CC[i];
  }
  return NULL;
}

static u32 xpass_root_reach(u64 loc) {
  XPassCCEntry *e = xpass_cc_find(loc);
  return e ? e->count : 0;
}

static u32 xpass_last_root(u64 loc) {
  XPassCCEntry *e = xpass_cc_find(loc);
  return e ? e->last_root : 0;
}

// Reached by two or more roots: the producing sub-pass must not recycle it,
// since a sibling reads its cached tid.  xpass_shared_input_plan frees it in
// the consuming sub-pass.
static int xpass_is_shared(u64 loc) { return xpass_root_reach(loc) >= 2; }

static int xpass_is_held(u64 loc) {
  XPassCCEntry *e = xpass_cc_find(loc);
  return e ? e->held : 0;
}

// Follow SUB-bit VAR and DP indirections, and a grad DP0 to its DUP's cell,
// without term_resolve's alo_force: this walk runs before materialize, and
// forcing an ALO layer mutates the heap.  An unforced ALO ends the walk, a
// conservative miss that treats a loc shared behind it as unshared.
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

// The first UOp under the structural wrappers the grad chain rule introduces
// (DP0/DP1, LAM, DUP, APP, SUP, OP2, MAT), resolved without forcing ALO layers;
// 0 for none or a UOP_KERNEL.
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

// Walk one root's UOp DAG, counting each loc it reaches once.  visited is
// HEAP_NEXT long; the caller resets it per root.
static void xpass_cc_walk_rec(Term t, u8 *visited, u64 cap, u32 root_idx) {
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
  xpass_cc_bump(loc, root_idx);
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
    xpass_cc_walk_rec(child, visited, cap, root_idx);
  }
}

fn void xpass_cc_reset(void) {
  for (u32 i = 0; i < XPASS_CC_CAP; i++) {
    XPASS_CC[i].loc = 0; XPASS_CC[i].count = 0; XPASS_CC[i].last_root = 0;
    XPASS_CC[i].held = 0;
  }
  XPASS_CC_LEN = 0;
  XPASS_CC_ACTIVE = 0;
}

static u8 op_is_view_movement(u8 op);

// Mark a realize root's loc held, and each loc down its movement chain (a view
// root returns a view of that buffer).  The walk already counted these locs, so
// their entries exist.
static void xpass_cc_mark_held(Term root) {
  Term t = xpass_resolve_uop(root, 0);
  for (int hops = 0; t != 0 && hops < 256; hops++) {
    XPassCCEntry *e = xpass_cc_find(term_val(t));
    if (e == NULL) {
      return;
    }
    e->held = 1;
    if (!op_is_view_movement((u8)term_ext(t))) {
      return;
    }
    t = xpass_resolve_uop(heap_read(term_val(t)), 0);
  }
}

// Fill XPASS_CC from a realize root, one Term or a TAG_CTR bundle, with a fresh
// visited bitmap per top-level root so a loc two roots reach counts twice.
// realize.c calls it before each materialize of its fixpoint loop, as the
// backward graph can grow between iterations.
fn void xpass_cc_populate(Term root) {
  xpass_cc_reset();
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL) return;
  if (term_tag(root) == TAG_CTR) {
    u32 n = term_ctr_n(root);
    for (u32 i = 0; i < n; i++) {
      memset(visited, 0, cap);
      xpass_cc_walk_rec(term_ctr_at(root, i), visited, cap, i);
    }
  } else {
    xpass_cc_walk_rec(root, visited, cap, 0);
  }
  free(visited);
  XPASS_CC_ACTIVE = 1;
  if (term_tag(root) == TAG_CTR) {
    u32 n = term_ctr_n(root);
    for (u32 i = 0; i < n; i++) {
      xpass_cc_mark_held(term_ctr_at(root, i));
    }
  } else {
    xpass_cc_mark_held(root);
  }
}

// The CTR child index being materialized (0 for a single root), set by
// thvm_materialize's CTR loop: xpass_shared_input_plan frees a shared
// boundary's buffer only in the pass of its last root (xpass_last_root - 1).
static u32 MATERIALIZE_CURRENT_ROOT = 0;

// Realize scope depth, entered and left by thvm_realize and thvm_realize_many.
// thvm_materialize consults the loc -> tid cache only inside a realize, so a
// test that calls it or bufferize_classify directly is never rewritten by
// entries an earlier test left behind.
static u32 MATERIALIZE_SCOPE_DEPTH = 0;
fn void materialized_loc_scope_enter(void) { MATERIALIZE_SCOPE_DEPTH++; }
fn void materialized_loc_scope_leave(void) {
  if (MATERIALIZE_SCOPE_DEPTH > 0) MATERIALIZE_SCOPE_DEPTH--;
}
fn u32  materialized_loc_scope_depth(void) { return MATERIALIZE_SCOPE_DEPTH; }

// JIT-capture realize-dedup span.  A captured training step realizes several
// times (loss, grads, optimizer step), and clearing the loc -> tid cache at
// each would re-emit and re-record the kernels they share, so inside the span
// the clear waits for materialized_loc_jit_span_end.  The capture pins every
// recorded output (buf_jit_pin), which the rollback skips, and
// materialized_loc_lookup misses on a dead buffer.  tinygrad realizes the step
// at once (`loss.realize(*opt.schedule_step())`).  A maxpool gradient's tie
// split (mixin/gradient.py:10-13) needs it: the pooled input, the forward MAX
// and the backward mask must read one buffer.
static u32 MATERIALIZE_JIT_SPAN_DEPTH = 0;

static int materialized_loc_span_holds(void) {
  // The span holds only on the backends kernel_gc_sweep covers (CPU, CUDA): it
  // relies on the sweep detaching each preserved boundary buffer's producer
  // kid, so the depth-first fire reads the materialized leaf instead of
  // re-firing the producer chain.
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) {
    return 0;
  }
  if (CURRENT_BACKEND == &CPU_BACKEND) return 1;
#ifdef THVM_HAS_CUDA
  if (CURRENT_BACKEND == &CUDA_BACKEND) return 1;
#endif
  return 0;
}
fn void materialized_loc_clear(void);
fn void materialized_loc_jit_span_begin(void) {
  // The depth marks being inside a capture; materialized_loc_span_holds decides
  // per backend whether the clear is deferred.
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) materialized_loc_clear();
  MATERIALIZE_JIT_SPAN_DEPTH++;
}
fn void materialized_loc_jit_span_end(void) {
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) return;
  MATERIALIZE_JIT_SPAN_DEPTH--;
  // The span's realizes are done: restore the journal and wipe.
  if (MATERIALIZE_JIT_SPAN_DEPTH == 0) materialized_loc_clear();
}

static inline u32 materialized_loc_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (MATERIALIZED_LOC_CAP - 1);
}

fn void materialized_loc_clear(void) {
  // Undo this realize's heap rewrites before wiping, so the next realize sees
  // the original graph.  This runs inside a JIT span too: the next realize then
  // re-derives every substitution through materialized_loc_lookup, whose
  // dead-buffer guard re-validates each cached tid.
  subst_journal_restore();
  // Inside a JIT span keep the map; materialized_loc_jit_span_end wipes it.
  if (materialized_loc_span_holds()) return;
  for (u32 i = 0; i < MATERIALIZED_LOC_CAP; i++) {
    MATERIALIZED_LOC_TABLE[i].loc = MATERIALIZED_LOC_EMPTY;
    MATERIALIZED_LOC_TABLE[i].tid = 0;
  }
  MATERIALIZED_LOC_LEN = 0;
  xpass_shared_tid_reset();
}

// Free this realize's fp8 -> bf16 cast transients: their one reader, a matmul
// reduce, has fired, but the kloc+0 TAG_TEN cell would make
// mark_heap_rooted_preserve keep the buffer every later realize.  Zero the
// kloc+0 and pin cells and decref the buffer (Metal buffers only).  Skipped
// during a JIT capture, whose pins keep replay-read buffers; result_tid is
// never freed.
fn void thvm_free_fp8_transients(u32 result_tid) {
  if (jit_is_capturing()) {
    FP8_TRANSIENT_LEN = 0;
    return;
  }
  for (u32 i = 0; i < FP8_TRANSIENT_LEN; i++) {
    u32 tid  = FP8_TRANSIENT_TIDS[i];
    u64 kloc = FP8_TRANSIENT_KLOCS[i];
    u64 pin  = FP8_TRANSIENT_PINS[i];
    // Unhook the buffer from the heap so the next preserve scan cannot re-mark
    // it.  Both cells are emit scratch (the kernel's output cell and its
    // THeapDiagram pin), unread after the fire.
    if (kloc != 0) heap_set(kloc + 0, 0);
    if (pin  != 0) heap_set(pin,      0);
    if (tid == 0 || tid >= TENS_NEXT || tid == result_tid) continue;
    u32 bid = TENS[tid].buf_id;
    if (bid == 0) continue;
    if (TENS[tid].backend == &METAL_BACKEND) thvm_metal_buf_decref_one(bid);
  }
  FP8_TRANSIENT_LEN = 0;
}

fn u32 materialized_loc_lookup(u64 loc) {
  if (loc == 0) return 0;
  u32 h = materialized_loc_hash(loc);
  for (u32 probe = 0; probe < MATERIALIZED_LOC_CAP; probe++) {
    u32 i = (h + probe) & (MATERIALIZED_LOC_CAP - 1);
    if (MATERIALIZED_LOC_TABLE[i].loc == MATERIALIZED_LOC_EMPTY) return 0;
    if (MATERIALIZED_LOC_TABLE[i].loc == loc) {
      u32 tid = MATERIALIZED_LOC_TABLE[i].tid;
      // A tid at or past TENS_NEXT was recycled: no dangling TAG_TEN.
      if (tid == 0 || tid >= TENS_NEXT) return 0;
      // The cache does not own the tid's buffer: the pool rollback
      // (cpu_buf_pool_rollback_with_preserve) can reclaim it while the entry
      // lingers.  A dead buffer is a miss, so the caller re-materializes
      // instead of binding a drained buffer.
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
  // Past half full stop inserting, or lookups degrade to long miss probes.  A
  // missing entry only costs a re-emit.
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
      // A re-emit in the same pass replaced the kernel: keep the newer tid.
      MATERIALIZED_LOC_TABLE[i].tid = tid;
      return;
    }
  }
}

// A heap collection (gc_collect) moves every live cell.  This half runs before
// the evacuation, undoing this realize's TAG_TEN substitutions while their
// cells sit at the journaled locs, so the collector copies the original graph;
// restored after the copy, the writes would land in dead from-space.
fn void materialized_loc_gc_before(void) {
  subst_journal_restore();
}

// After the evacuation, before the space swap: re-key the loc -> tid map and
// the shared-tid locs to the moved cells and drop the dead ones.  A JIT span
// keeps the map across realizes, where a stale key would hand an unrelated cell
// a tid.
fn void materialized_loc_gc_after(void) {
  u32 n = 0;
  MaterializedLocEntry *keep = NULL;
  if (MATERIALIZED_LOC_LEN > 0) {
    keep = (MaterializedLocEntry *)malloc(sizeof(MaterializedLocEntry) * MATERIALIZED_LOC_LEN);
  }
  for (u32 i = 0; i < MATERIALIZED_LOC_CAP; i++) {
    if (MATERIALIZED_LOC_TABLE[i].loc == MATERIALIZED_LOC_EMPTY) {
      continue;
    }
    u64 nl = gc_forward_loc(MATERIALIZED_LOC_TABLE[i].loc);
    if (keep != NULL && nl != ~0ull && n < MATERIALIZED_LOC_LEN) {
      keep[n].loc = nl;
      keep[n].tid = MATERIALIZED_LOC_TABLE[i].tid;
      n++;
    }
    MATERIALIZED_LOC_TABLE[i].loc = MATERIALIZED_LOC_EMPTY;
    MATERIALIZED_LOC_TABLE[i].tid = 0;
  }
  MATERIALIZED_LOC_LEN = 0;
  for (u32 i = 0; i < n; i++) {
    materialized_loc_insert(keep[i].loc, keep[i].tid);
  }
  free(keep);
  for (u32 i = 0; i < XPASS_SHARED_TID_LEN; i++) {
    u64 nl = gc_forward_loc(XPASS_SHARED_TIDS[i].loc);
    if (nl == ~0ull) {
      XPASS_SHARED_TIDS[i].tid = 0;
    } else {
      XPASS_SHARED_TIDS[i].loc = nl;
    }
  }
}

// UOP_COPY device-upload cache: (copy-node loc, target backend) -> uploaded
// tid.  It lives across realizes: a COPY node is hash-consed by its src (a
// stable loc) and the uploaded weight is immutable, so re-realize and JIT
// replay reuse the device buffer.  materialize_copy jit-pins what it caches, so
// the per-realize rollback keeps it.  tinygrad's exec_copy copies into a dest
// Buffer that stays allocated (engine/realize.py:147).
#define COPY_UPLOAD_CACHE_CAP (1u << 12)
typedef struct {
  u64 loc;                                       // copy-node heap loc, 0 = empty
  u32 backend_id;                                // target backend
  u32 tid;                                        // uploaded TenDesc
} CopyUploadSlot;
static CopyUploadSlot COPY_UPLOAD_CACHE[COPY_UPLOAD_CACHE_CAP];

fn void copy_upload_cache_reset(void) {
  memset(COPY_UPLOAD_CACHE, 0, sizeof(COPY_UPLOAD_CACHE));
}

// The copy-node loc and the uploaded tid are per context (the heap and tensor
// counters restart in each), so the context slot is folded into backend_id
// above the backend's own id: the same loc in another context must not hit.
static inline u32 copy_upload_ctx_backend(u32 backend_id) {
  return backend_id | (thvm_context_current() << 16);
}

static inline u32 copy_upload_hash(u64 loc, u32 backend_id) {
  u64 k = loc * 0x9e3779b97f4a7c15ULL + backend_id;
  k ^= k >> 33;
  return (u32)k & (COPY_UPLOAD_CACHE_CAP - 1);
}

static u32 copy_upload_lookup(u64 loc, u32 backend_id) {
  backend_id = copy_upload_ctx_backend(backend_id);
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
  backend_id = copy_upload_ctx_backend(backend_id);
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

// Resident weight-movement cache (THVM_TC_TRANSPOSE, under which fxLinear feeds
// the matmul `TRealize@Transpose[w]` so the CUDA tiled-WMMA GEMM gets a
// contiguous operand).  That permute is static and rebound by every replay, so
// it is realized once, jit-pinned and read as a stable leaf.  Keyed by
// weight_move_key (upload tid, shape, dtype) and backend: a warmup fills it,
// and a capture reuses the buffer instead of recording a dispatch.
static CopyUploadSlot WEIGHT_MOVE_CACHE[COPY_UPLOAD_CACHE_CAP];

// Parallel to WEIGHT_MOVE_CACHE: the source upload tid each cached transpose
// came from (0 = none), so weight_move_trim_sources can free the source once
// the captured kernels read only the transposed copy.
static u32 WEIGHT_MOVE_SRC[COPY_UPLOAD_CACHE_CAP];

// The source identity again, kept through the trim (which zeroes
// WEIGHT_MOVE_SRC): once the trim has purged the upload's COPY_UPLOAD_CACHE
// entry and freed its buffer, the WL-cached weight term still names the tid,
// and a boundary over it must hit the cached transposed leaf, since a re-emit
// would read the freed upload.
static u32 WEIGHT_MOVE_SRCID[COPY_UPLOAD_CACHE_CAP];

static int weight_move_srcid_has(u32 tid) {
  if (tid == 0) return 0;
  for (u32 i = 0; i < COPY_UPLOAD_CACHE_CAP; i++)
    if (WEIGHT_MOVE_SRCID[i] == tid) return 1;
  return 0;
}

fn void weight_move_cache_reset(void) {
  memset(WEIGHT_MOVE_CACHE, 0, sizeof(WEIGHT_MOVE_CACHE));
  memset(WEIGHT_MOVE_SRC,   0, sizeof(WEIGHT_MOVE_SRC));
  memset(WEIGHT_MOVE_SRCID, 0, sizeof(WEIGHT_MOVE_SRCID));
}

static u32 weight_move_lookup(u64 loc, u32 backend_id) {
  backend_id = copy_upload_ctx_backend(backend_id);
  u32 h = copy_upload_hash(loc, backend_id);
  for (u32 probe = 0; probe < COPY_UPLOAD_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (COPY_UPLOAD_CACHE_CAP - 1);
    if (WEIGHT_MOVE_CACHE[i].loc == 0) return 0;
    if (WEIGHT_MOVE_CACHE[i].loc == loc
        && WEIGHT_MOVE_CACHE[i].backend_id == backend_id)
      return WEIGHT_MOVE_CACHE[i].tid;
  }
  return 0;
}

// Marks a purged entry: a loc = 0 clear would cut the linear-probe chains
// through it.
#define WEIGHT_MOVE_TOMBSTONE (~0ULL)

static void weight_move_insert(u64 loc, u32 backend_id, u32 tid, u32 src_tid) {
  if (loc == 0 || tid == 0) return;
  backend_id = copy_upload_ctx_backend(backend_id);
  u32 h = copy_upload_hash(loc, backend_id);
  for (u32 probe = 0; probe < COPY_UPLOAD_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (COPY_UPLOAD_CACHE_CAP - 1);
    if (WEIGHT_MOVE_CACHE[i].loc == 0
        || (WEIGHT_MOVE_CACHE[i].loc == loc
            && WEIGHT_MOVE_CACHE[i].backend_id == backend_id)) {
      WEIGHT_MOVE_CACHE[i].loc        = loc;
      WEIGHT_MOVE_CACHE[i].backend_id = backend_id;
      WEIGHT_MOVE_CACHE[i].tid        = tid;
      WEIGHT_MOVE_SRC[i]              = src_tid;
      if (src_tid != 0) WEIGHT_MOVE_SRCID[i] = src_tid;
      return;
    }
  }
}

// Purge the entries whose tid an emission-bail TENS_NEXT rewind invalidated
// (thvm_materialize's all-or-nothing rewind): the retry re-issues the tid to
// another tensor, so a stale hit would hand a consumer the wrong buffer and
// shape.
static void weight_move_purge_from(u32 tens_wm) {
  for (u32 i = 0; i < COPY_UPLOAD_CACHE_CAP; i++) {
    if (WEIGHT_MOVE_CACHE[i].loc != 0
        && WEIGHT_MOVE_CACHE[i].loc != WEIGHT_MOVE_TOMBSTONE
        && WEIGHT_MOVE_CACHE[i].tid >= tens_wm) {
      WEIGHT_MOVE_CACHE[i].loc = WEIGHT_MOVE_TOMBSTONE;
      WEIGHT_MOVE_CACHE[i].tid = 0;
      WEIGHT_MOVE_SRC[i]       = 0;
      WEIGHT_MOVE_SRCID[i]     = 0;  // entry invalid: drop the identity too
    }
  }
}

// Remove `tid` from COPY_UPLOAD_CACHE so a later realize that reaches the
// same copy node re-uploads instead of returning a trimmed (freed) tensor.
static void copy_upload_purge_tid(u32 tid) {
  if (tid == 0) return;
  for (u32 i = 0; i < COPY_UPLOAD_CACHE_CAP; i++)
    if (COPY_UPLOAD_CACHE[i].loc != 0 && COPY_UPLOAD_CACHE[i].tid == tid)
      COPY_UPLOAD_CACHE[i].tid = 0;   // hit path treats tid 0 as a miss
}

// Free the source uploads behind every cached transposed weight; call after the
// warmup and the captures, whose kernels read only the pinned transposed leaves
// (a hit skips the transpose kernel, the sources' only reader).  Unpins and
// decrefs each buffer, marks its TenDesc dead (buf_id 0) so every cache guard
// misses, and purges its upload-cache entry so a later miss re-uploads.
// Returns the bytes freed.
fn u64 weight_move_trim_sources(void) {
  u64      freed   = 0;
  Backend *drained = NULL;
  for (u32 i = 0; i < COPY_UPLOAD_CACHE_CAP; i++) {
    u32 src = WEIGHT_MOVE_SRC[i];
    if (src == 0 || WEIGHT_MOVE_CACHE[i].loc == 0
        || WEIGHT_MOVE_CACHE[i].loc == WEIGHT_MOVE_TOMBSTONE) continue;
    WEIGHT_MOVE_SRC[i] = 0;
    if (src >= TENS_NEXT) continue;
    Backend *b   = TENS[src].backend;
    u32      bid = TENS[src].buf_id;
    if (b == NULL || bid == 0) continue;
    // A source that a recorded dispatch reads (a transpose re-derived at a
    // fresh loc during the capture misses the cache and records the upload as
    // an input) is leaf-retained by jit_capture_retain_buf, so a refcount above
    // 1 means a replay reads it.
    if (b->buf_refcount != NULL && b->buf_refcount(bid) > 1) continue;
    u64 nbytes = dtype_storage_bytes(TENS[src].dtype, (u64)TENS[src].view.numel);
    if (b->buf_jit_unpin != NULL) b->buf_jit_unpin(bid);
    if (b->buf_decref   != NULL) b->buf_decref(bid);
    TENS[src].buf_id = 0;              // dead: guards miss, re-upload on demand
    copy_upload_purge_tid(src);
    freed  += nbytes;
    drained = b;
  }
  // CUDA frees are stream-ordered (cuMemFreeAsync on the legacy stream): until
  // the stream drains, the next cuMemAlloc still sees a full device.
  // dispatch_end drains it (a cuCtxSynchronize on CUDA).
  if (freed != 0 && drained != NULL && drained->dispatch_end != NULL)
    drained->dispatch_end();
  return freed;
}

// THVM_WM_TRACE=1: one stderr line per weight-move pin or hit.
static int wm_trace_enabled(void) {
  static int v = -1;
  if (v < 0) { char const *e = getenv("THVM_WM_TRACE"); v = (e && e[0] == '1') ? 1 : 0; }
  return v;
}

// THVM_TC_TRANSPOSE gate.  fxLinear realizes the weight transpose only under
// it, so gating the pin on it keeps realized weight movements elsewhere
// planner-managed.  Read per call, not latched: the WL side sets it per model
// component (off for the text encoder, on for the DiT).
static int tc_weight_pin_enabled(void) {
  char const *e = getenv("THVM_TC_TRANSPOSE");
  return (e != NULL && e[0] == '1');
}

// tid is a live COPY_UPLOAD_CACHE entry.  A realized movement whose input is
// one is a transposed weight, not a per-step activation, so pinning it does not
// leak.
static int copy_upload_has_tid(u32 tid) {
  if (tid == 0) return 0;
  for (u32 i = 0; i < COPY_UPLOAD_CACHE_CAP; i++)
    if (COPY_UPLOAD_CACHE[i].loc != 0 && COPY_UPLOAD_CACHE[i].tid == tid)
      return 1;
  return 0;
}

// Chase a movement boundary's UOp tree (src cell loc+0) through movement ops,
// PAD and CAST to the weight upload under it: a live one (fxLinear's
// Transpose[TUOpCast[upload]]) or a trimmed one (WEIGHT_MOVE_SRCID); anything
// else gives 0, so the chase is also the purity gate.  Chasing the tree, not
// the kernel inputs, works before visit() (a hit skips emission) and survives
// the WL side re-deriving the movement at fresh locs: the upload tid is stable,
// so (upload, shape, dtype) is the durable key.
static u8 op_is_view_movement(u8 op);
static u32 weight_move_chase_upload(Term t) {
  for (u32 hops = 0; hops < 12; hops++) {
    if (term_tag(t) == TAG_TEN) {
      u32 tid = (u32)term_val(t);
      // A live upload, or a trimmed one whose identity is registered:
      // boundaries over the WL-cached weight term still chase to the tid and
      // must hit.
      return (copy_upload_has_tid(tid) || weight_move_srcid_has(tid)) ? tid : 0;
    }
    if (term_tag(t) != TAG_UOP) return 0;
    u8 op = term_ext(t);
    if (!op_is_view_movement(op) && op != UOP_CAST && op != UOP_PAD
        && op != UOP_FLIP) return 0;
    t = heap_read(term_val(t));           // src is the first heap cell
  }
  return 0;
}

// Durable key of a transposed weight: the upload tid, the movement's output
// shape and dtype (a transpose and a reshape of one weight must not collide).
static u64 weight_move_key(u32 src_tid, Shape const *shape, u32 dtype) {
  u64 k = ((u64)src_tid << 20) ^ ((u64)dtype << 1);
  for (u32 i = 0; i < shape->ndim; i++)
    k = k * 0x100000001b3ULL + shape->dims[i];
  if (k == 0 || k == WEIGHT_MOVE_TOMBSTONE) k = 1;
  return k;
}

// Materialize UOP_COPY[src] onto the COPY's device (else the realize backend)
// as a TAG_TEN.  On the src's own backend it is the identity.  Otherwise the
// bytes are staged through a host buffer into a fresh target buffer, cached by
// (copy loc, backend) and jit-pinned when captured or a disk weight; a
// disk-mmap weight bound for Metal is wrapped in place instead.  tinygrad:
// copy_to_device (uop/ops.py:766-773), the same-device fold
// (schedule/prepare.py:215), exec_copy (engine/realize.py:145-157).
static Term materialize_copy(Term term) {
  u64  loc = term_val(term);
  Term src = heap_read(loc);
  // Read the src's device, shape and dtype before materializing it: a compute
  // src turns into a UOP_KERNEL that no longer carries the device-bearing
  // leaves.
  i32   src_dev   = term_device_in(src);
  Shape src_shape = {0};
  int   shape_ok  = term_shape_in(src, 0, &src_shape);
  u32   src_dtype = DT_FP32;
  term_dtype_in(src, 0, &src_dtype);
  i32   cpy_dev   = uop_copy_device(loc);

  // A host-leaf weight materializes to a TAG_TEN (its bytes exist, so it is
  // uploaded below); a compute src to a UOP_KERNEL not yet fired.
  Term src_mat = thvm_materialize(src);
  if (src_mat != src) heap_set(loc, src_mat);

  // The target is the COPY's explicit device, else the realize backend.  An
  // explicit device's backend is installed on demand; when it is unavailable
  // (Metal on a non-Metal build) the COPY passes through on the realize
  // backend.
  Backend *target      = (cpy_dev < 0) ? CURRENT_BACKEND : ctx_ensure_backend(cpy_dev);
  if (target == NULL) target = CURRENT_BACKEND;
  Backend *src_backend = (src_dev >= 0) ? ctx_ensure_backend(src_dev) : NULL;

  // A compute src: the transfer must run after its kernel.  Across devices it
  // becomes a fire-time cross-backend ASSIGN (interact_assign reads the src's
  // backend and writes the dst's), which wnf fires after the src kernel, as
  // tinygrad's exec_copy runs in schedule order.  On the same device, or an
  // unknown one, the COPY passes through.
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
  // A computed-output TEN (producer_kid != 0) on another device is computed at
  // fire too: defer like the UOP_KERNEL case.
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
  // Zero-copy wrap of a disk-mmap weight for Metal: on unified memory
  // newBufferWithBytesNoCopy wraps the mmap pages in place.  The weight's byte
  // offset within the page-aligned map becomes the buffer's byte_offset,
  // applied at every kernel input bind.  Any mismatch (not Metal, not a disk
  // src, a failed wrap, an offset that is no element multiple) stages instead.
  {
    TenDesc *sd = &TENS[src_tid];
    void *map_base = NULL;
    u64   maplen = 0, minor = 0;
    u32   isz = dtype_itemsize(sd->dtype);
    // Default on; THVM_ZEROCOPY=0 disables.  thvm_metal_buf_wrap_external wraps
    // each region once (dedup by host base) and refcounts the borrowed slot.
    // getenv is re-read until it reads disabled, so a WL SetEnvironment before
    // the first upload is honored.
    static int zc_enabled = -1;
    if (zc_enabled != 0) {
      const char *e = getenv("THVM_ZEROCOPY");
      zc_enabled = (e == NULL || e[0] != '0') ? 1 : 0;
    }
    if (zc_enabled && target == &METAL_BACKEND && srcb == &CPU_BACKEND && isz != 0
        && thvm_disk_buf_map_info(sd->buf_id, &map_base, &maplen, &minor)
        && (minor % isz) == 0) {
      u32 wbid = thvm_metal_buf_wrap_external(map_base, maplen, minor);
      if (wbid != 0) {
        // Copy the src's offset-0 leaf view unchanged: the byte offset rides on
        // the wrapped buffer, because the matmul codegen rebuilds a fresh
        // offset-0 leaf view over the raw buffer and would drop an offset baked
        // into the view.
        if (TENS_NEXT >= TENS_CAP) {
          fprintf(stderr, "materialize_copy: out of TenDesc slots for wrap\n");
        } else {
          u32 dwid = TENS_NEXT++;
          TenDesc *dw = &TENS[dwid];
          dw->dtype         = sd->dtype;
          dw->refcount      = 1;
          dw->view          = sd->view;
          dw->prior_views   = NULL;
          dw->nviews        = 0;
          dw->requires_grad = 0;
          dw->grad          = 0;
          dw->assign_kvar_id = 0;
          dw->backend       = target;
          dw->producer_kid  = 0;
          dw->buf_id        = wbid;
          // Under a JIT capture pin and cache the wrap like the staged upload
          // below: every replay rebinds it.  An eager wrap holds no device
          // bytes, so leaving it unpinned only means a re-wrap next pass.
          if (jit_is_capturing()) {
            if (target->buf_jit_pin != NULL) target->buf_jit_pin(wbid);
            copy_upload_insert(loc, target->id, dwid);
          }
          return term_new(0, TAG_TEN, dw->dtype, dwid);
        }
      }
      // Wrap declined: stage the upload.
    }
  }
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
  int is_disk_weight = (srcb == &CPU_BACKEND)
                    && thvm_disk_buf_is_mapped(sd->buf_id);
  u32 dst_tid = tensor_alloc(target, sd->view.shape, dtype);
  target->buf_write(TENS[dst_tid].buf_id, stage, nbytes);
  free(stage);
  // The device owns the bytes now: drop a disk-mapped src's host pages
  // (MADV_DONTNEED) so the weight set is not resident twice.  The mapping stays
  // valid and re-faults on a later CPU read; thvm_disk_buf_dontneed is a no-op
  // on a non-disk buffer.
  if (srcb == &CPU_BACKEND) thvm_disk_buf_dontneed(sd->buf_id);
  // Pin and cache the upload under a JIT capture (every replay rebinds it) and
  // for a disk-mmap weight even when eager: an unpinned weight is reclaimable
  // by the rollback, and the size-keyed Metal freelist would hand its buffer to
  // a later same-size weight under a binding that still names the first.  The
  // zero-copy wrap is immune (borrowed buffers skip the freelist).  A plain
  // eager intermediate stays unpinned, so the rollback bounds a forward's
  // resident set.
  if (jit_is_capturing() || is_disk_weight) {
    if (target->buf_jit_pin != NULL) target->buf_jit_pin(TENS[dst_tid].buf_id);
    copy_upload_insert(loc, target->id, dst_tid);
  }
  return term_new(0, TAG_TEN, TENS[dst_tid].dtype, dst_tid);
}

// Substitute cached UOp descendants in place with TAG_TEN leaves: a child cell
// holding a UOp at a cached loc is overwritten (and journaled), any other UOp
// child is recursed into.  Stops at a UOP_KERNEL; under an ASSIGN only the src
// is walked.  visited bounds the walk.
static void materialize_subst_cached_rec(Term term, u8 *visited, u64 cap) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  if (op == UOP_ASSIGN) {
    // ASSIGN(dst, src): dst is a TAG_TEN reference and ASSIGN emits are not
    // cached, so only the src is substituted.
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
// Per boundary, the deepest BOUNDARY_DEPTH among its consumers, filled by
// boundary_compute_last_use; the freelist planner recycles the output once
// emission passes it.  0 = no consumer is a realize boundary: the realize root
// (the caller reads it) or a preserved orphan, never recycled.
static u32 BOUNDARY_LAST_USE [BUFFERIZE_NODES_CAP];
// Per boundary, its post-order DFS position from the realize sink, the order
// kernel_fire_by_id dispatches in (producers first, in slot order).
// BOUNDARY_ORDER is sorted by it because the arena recycles a slot once its
// boundary's position passes, and a (depth, loc) order can diverge from
// dispatch.  tinygrad plans over the linear schedule (schedule/memory.py:28).
// 0xFFFFFFFF = unvisited (an orphan preserved tensor, ordered by depth).
static u32 BOUNDARY_FIRE_SEQ [BUFFERIZE_NODES_CAP];
// Per boundary, the latest BOUNDARY_ORDER position among its realized
// consumers, so the arena recycles a slot only after its last reader fires (a
// shallower consumer can fire after the deepest one, so BOUNDARY_LAST_USE's
// depth cannot say).  tinygrad's last_appearance (schedule/memory.py:32).  0 =
// no realized-boundary consumer.
static u32 BOUNDARY_LAST_USE_POS [BUFFERIZE_NODES_CAP];

#define VISIT_BAIL 0xDEADBEEFu
// visit() returns VISIT_OK, an input-slot ref, or VISIT_BAIL.  KSRC_AS_INPUT
// sets bit 31, so no input ref equals VISIT_OK (0).  visit only records input
// bindings: the kernel body is the lifted UOp DAG (cached_lift.store_root).
#define VISIT_OK   0u

typedef struct {
  u64 *locs;
  u32 *refs;
  u32  len;
  u32  cap;
  u8   indexed;   // 1 once the kernel holds a gather or scatter (CPU only)
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

// Per-pass freelist planner, the freelist analogue of tinygrad's
// memory_plan_rewrite: before each kernel allocates its output, every recorded
// output whose last_use_depth has passed goes onto its backend's free-list, and
// the next same-size tensor_alloc pops it.  Safe on Metal: the pass dispatches
// nothing while it emits (wnf fires the kernels afterwards), so
// metal_buf_freelist_try_pop's memset races no GPU work.
// mem_plan_drain_freelist pulls the leftovers back at the pass end, so the next
// pass cannot recycle a buffer the chain rule's fresh UOps still reference.

#define MEM_PLAN_CAP BOUNDARY_ORDER_CAP
// last_use_depth of a cross-pass shared buffer in its producing sub-pass: above
// any BOUNDARY_DEPTH, so mem_plan_push_dead never frees it there.  The last
// consuming sub-pass frees it (xpass_shared_input_plan), the freelist
// counterpart of arena_compute extending a shared slot to the pass end.
#define MEM_PLAN_LAST_USE_NEVER 0xFFFFFFFEu
typedef struct {
  u32      buf_id;
  u32      last_use_depth;
  Backend *backend;
  u8       pushed;
  // 1 for a cross-pass shared input freed in its last consuming sub-pass
  // (xpass_shared_input_plan): no later pass reads it, so
  // mem_plan_drain_freelist leaves it on the freelist, unlike an in-pass output
  // whose TenDesc the next fixpoint iteration may still reference.
  u8       shared_free;
} MemPlanEntry;

static MemPlanEntry MEM_PLAN[MEM_PLAN_CAP];
static u32          MEM_PLAN_LEN = 0;

// Metal: default on; THVM_METAL_REUSE_BUFS=0 opts out.  Read on every call, not
// latched, so a caller that sets it after earlier plans ran (FluxGenerate's
// fxBoundMemory) still takes effect.
static int mem_plan_metal_enabled(void) {
  char const *e = getenv("THVM_METAL_REUSE_BUFS");
  return (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
}

// CUDA: default on; THVM_CUDA_REUSE_BUFS=0 opts out.  Kernels run in fire order
// on one stream, so a recycled buffer's next writer runs after its last reader.
static int mem_plan_cuda_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_CUDA_REUSE_BUFS");
    enabled       = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');
    known         = 1;
  }
  return enabled;
}

// The gate a recorded buffer obeys, by backend id (1 CPU, 2 Metal, 3 CUDA).
static int mem_plan_backend_enabled(Backend const *b) {
  if (b == NULL) return 0;
  if (b->id == 1) return 1;
  if (b->id == 2) return mem_plan_metal_enabled();
  if (b->id == 3) return mem_plan_cuda_enabled();
  return 0;
}

static void mem_plan_reset(void) { MEM_PLAN_LEN = 0; }

// Pull every planner push that no in-pass buf_alloc re-issued back off its
// free-list.  thvm_realize loops materialize and wnf to a fixed point, and a
// push surviving into the next pass could hand that pass a buffer whose TenDesc
// the chain rule's fresh UOps still reference.  Reuse within one emit loop is
// unaffected.
static void mem_plan_drain_freelist(void) {
  for (u32 i = 0; i < MEM_PLAN_LEN; i++) {
    MemPlanEntry *e = &MEM_PLAN[i];
    if (!e->pushed)                       continue;
    // Dead after this, its last consuming sub-pass: leave it on the freelist.
    if (e->shared_free)                   continue;
    Backend *b = e->backend;
    if (b == NULL || b->buf_freelist_remove == NULL) continue;
    if (!mem_plan_backend_enabled(b))     continue;
    b->buf_freelist_remove(e->buf_id);
  }
  // Likewise forget the capture-recycle pool's unadopted entries (they stay
  // pinned and allocated for the replay).
  jit_caprecycle_drain();
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
  e->shared_free    = 0;
}

// Record a cross-pass shared input for recycling in its last consuming
// sub-pass: mem_plan_push_dead frees it once current_depth passes its last
// reader there, and mem_plan_drain_freelist does not pull it back.
static void mem_plan_record_shared_free(u32 buf_id, u32 last_use_depth, Backend *b) {
  if (b == NULL || buf_id == 0)              return;
  if (b->buf_freelist_push == NULL)          return;
  if (!mem_plan_backend_enabled(b))          return;
  if (MEM_PLAN_LEN >= MEM_PLAN_CAP)          return;
  MemPlanEntry *e = &MEM_PLAN[MEM_PLAN_LEN++];
  e->buf_id         = buf_id;
  e->last_use_depth = last_use_depth;
  e->backend        = b;
  e->pushed         = 0;
  e->shared_free    = 1;
}

// Free a cross-pass shared buffer now: it is dead on entry to this sub-pass,
// read only by earlier siblings.  Its producer and last reader fire before any
// kernel of this pass (which depends on them), so a same-size output here may
// reuse its storage.  The refcount guard matches mem_plan_push_dead's.
static void mem_plan_push_shared_dead(u32 buf_id, Backend *b) {
  if (b == NULL || buf_id == 0)              return;
  if (b->buf_freelist_push == NULL)          return;
  if (!mem_plan_backend_enabled(b))          return;
  if (b->buf_refcount != NULL && b->buf_refcount(buf_id) > 1) return;
  // A captured (jit-pinned) buffer refuses the freelist: park it on the
  // capture-recycle pool instead.
  if (jit_caprecycle_push(b, buf_id)) {
    if (MEM_PLAN_LEN >= MEM_PLAN_CAP)        return;
    MemPlanEntry *pe = &MEM_PLAN[MEM_PLAN_LEN++];
    pe->buf_id         = buf_id;
    pe->last_use_depth = MEM_PLAN_LAST_USE_NEVER;
    pe->backend        = b;
    pe->pushed         = 1;
    pe->shared_free    = 1;
    return;
  }
  b->buf_freelist_push(buf_id);
  if (MEM_PLAN_LEN >= MEM_PLAN_CAP)          return;
  MemPlanEntry *e = &MEM_PLAN[MEM_PLAN_LEN++];
  e->buf_id         = buf_id;
  e->last_use_depth = MEM_PLAN_LAST_USE_NEVER;   // already pushed
  e->backend        = b;
  e->pushed         = 1;
  e->shared_free    = 1;                          // drain must not pull back
}

static void mem_plan_push_dead(u32 current_depth) {
  for (u32 i = 0; i < MEM_PLAN_LEN; i++) {
    MemPlanEntry *e = &MEM_PLAN[i];
    if (e->pushed)                              continue;
    // 0: no realize-boundary consumer (the sink, the realize root, a preserved
    // orphan).  The caller reads it after the realize, so it is never pushed:
    // the next alloc would hand its storage to a fresh intermediate.
    if (e->last_use_depth == 0)                 continue;
    if (e->last_use_depth >= current_depth)     continue;
    if (e->buf_id == 0)                         continue;
    Backend *b = e->backend;
    if (b == NULL || b->buf_freelist_push == NULL || !mem_plan_backend_enabled(b)) {
      e->pushed = 1;
      continue;
    }
    // Refcount > 1: another TenDesc aliases the buffer (a view chain), and
    // recycling would pull the bytes from under it.  The post-realize preserve
    // walk and rollback release these.
    if (b->buf_refcount != NULL && b->buf_refcount(e->buf_id) > 1) {
      e->pushed = 1;
      continue;
    }
    // Under a JIT capture the buffer is jit-pinned and the freelist refuses it:
    // park it on the capture-recycle pool, where the next same-size captured
    // output adopts it and the stream records the reuse (tinygrad memory-plans
    // a captured schedule too: jit_lower, engine/jit.py:35).
    if (jit_caprecycle_push(b, e->buf_id)) {
      e->pushed = 1;
      continue;
    }
    // An external buffer (a WL NumericArray import, owns_data 0) may be pushed:
    // cpu_buf_freelist_try_pop never hands one out.
    b->buf_freelist_push(e->buf_id);
    e->pushed = 1;
  }
}

// Per-pass arena planner, a port of tinygrad's memory_plan_rewrite
// (schedule/memory.py:20-62): every plannable output gets a byte offset in one
// arena buffer from a TLSF suballocator fed the open and close events, so
// outputs with disjoint lifetimes share bytes and the peak is the live set.
// The others take a fresh buffer.  The arena lives one realize.  CPU and CUDA
// by default, Metal opt-in (metal_arena_opt_in); arena_tensor_alloc makes each
// view's backend buffer and zeroes its slot.

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

// The Metal arena (offset slices of one shared MTLBuffer) is opt-in,
// THVM_METAL_ARENA=1, unlike the CPU and CUDA ones.  On Metal its payoff is a
// recycling JIT capture that is ICB-safe, so the indirect command buffer can be
// cached and replayed, and that also needs the JIT input uploads in stable
// buffers; without them the arena only adds per-replay planning and zeroing.
static int metal_arena_opt_in(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_METAL_ARENA");
    enabled = (e != NULL && e[0] == '1');
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

// Sort by position, opens before closes at the same position, so a boundary
// never takes the bytes of one whose last reader is the boundary just before
// it.  tinygrad sorts closes first (schedule/memory.py:43-44), one step
// tighter.
static void arena_sort_events(ArenaEvent *ev, u32 n) {
  // Insertion sort: n is typically a few hundred events.
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
  // Inside a JIT-capture dedup span a later realize of the step can reuse this
  // output, but the arena plans within one realize and would recycle its bytes
  // mid-step (the jit pin keeps the buf_id alive, not its bytes), so the span
  // allocates every boundary outside the arena.
  if (materialized_loc_span_holds()) return 0;
  u64 loc = BOUNDARY_ORDER[ord_idx];
  u32 binfo = bufferize_info_find(loc);
  if (binfo == 0xFFFFFFFFu) return 0;
  // last_use 0: no realize-boundary consumer (the realize root, a preserved
  // orphan).  tinygrad's held_bufs keep result buffers out of
  // memory_plan_rewrite (schedule/memory.py:12-13).
  if (BOUNDARY_LAST_USE[binfo] == 0) return 0;
  // A root that another root also reads has an in-pass consumer; it is
  // still held (see XPassCCEntry.held).
  if (xpass_is_held(loc)) {
    return 0;
  }
  // Single consumer only.  A multi-consumer output can be aliased through
  // DUP/SUP or read by the next pass of the realize loop's chain rule, which
  // the per-pass BOUNDARY_LAST_USE cannot see.  tinygrad's one-pass schedule
  // has no such limit.
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

// Give every plannable boundary a lifetime and a TLSF offset
// (schedule/memory.py:24-53), filling ARENA_SLOTS and ARENA_SIZE.  The arena
// buffer itself comes with the first arena_tensor_alloc, so a pass with nothing
// to plan allocates nothing.
static void arena_compute(void) {
  arena_reset();
  if (BOUNDARY_ORDER_LEN == 0)   return;
  if (CURRENT_BACKEND == NULL)   return;
  int is_cpu   = (CURRENT_BACKEND == &CPU_BACKEND);
  int is_metal = (CURRENT_BACKEND == &METAL_BACKEND) && metal_arena_opt_in();
  int is_cuda  = 0;
#ifdef THVM_HAS_CUDA
  is_cuda = (CURRENT_BACKEND == &CUDA_BACKEND);
#endif
  if (!is_cpu && !is_cuda && !is_metal) return;

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
    // The last reader's fire position, not its depth (see
    // BOUNDARY_LAST_USE_POS).
    u32 last_at = BOUNDARY_LAST_USE_POS[binfo];
    if (last_at < i) last_at = i;
    // A boundary two or more top-level roots reach is cached by the first
    // sub-pass and read by a sibling through materialized_loc_lookup, a reader
    // consumer_count cannot see but xpass_is_shared can.  It stays in the arena
    // but lives to the end of the pass, so its bytes hold for every sibling.
    if (xpass_is_shared(loc) && BOUNDARY_ORDER_LEN > 0)
      last_at = BOUNDARY_ORDER_LEN - 1;
    nbytes[i]   = nb;
    last_pos[i] = last_at;
    planned[i]  = 1;
    n_planned++;
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
        // TLSF over twice the total should not run out; if it does, plan
        // nothing and let every emit take a fresh buffer.
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
}

// Allocate the arena buffer on the first arena-slot request.  It counts toward
// the backend's live bytes and the pool rollback like any buffer.  1 when an
// arena exists.
static int arena_ensure(void) {
  if (ARENA_BUF_ID != 0)        return 1;
  if (ARENA_SIZE == 0)          return 0;
  if (CURRENT_BACKEND == &CPU_BACKEND) {
    // The arena is the planner-bounded working set, not a runaway im2col:
    // exempt from the per-buffer im2col ceiling, still under cpu_buf_alloc's
    // total-live ceiling.
    cpu_arena_alloc_inflight_set(1);
    ARENA_BUF_ID = cpu_buf_alloc(ARENA_SIZE);
    cpu_arena_alloc_inflight_set(0);
    if (ARENA_BUF_ID == 0)      return 0;
    ARENA_DATA = (u8 *)CPU_BUFS[ARENA_BUF_ID].data;
    // skip_freelist: the end-of-realize rollback really frees the arena instead
    // of parking it, where best-fit would hand a pass-sized block to a small
    // request and every later realize would allocate a fresh arena.
    CPU_BUFS[ARENA_BUF_ID].skip_freelist = 1;
    return 1;
  }
#ifdef THVM_HAS_CUDA
  if (CURRENT_BACKEND == &CUDA_BACKEND) {
    cpu_arena_alloc_inflight_set(1);
    ARENA_BUF_ID = cuda_buf_alloc(ARENA_SIZE);
    cpu_arena_alloc_inflight_set(0);
    if (ARENA_BUF_ID == 0)      return 0;
    ARENA_DPTR = cuda_buf_dptr(ARENA_BUF_ID);
    CUDA_BUFS[ARENA_BUF_ID].skip_freelist = 1;
    return 1;
  }
#endif
  if (CURRENT_BACKEND == &METAL_BACKEND) {
    // One shared MTLBuffer for the pass's working set, sliced by the views
    // below; thvm_metal_buf_arena_alloc sets skip_freelist like the CPU and
    // CUDA branches, and shared storage mode lets the view zeroing memset it.
    // The im2col-ceiling exemption matches the CPU branch.  tinygrad:
    // UOp.new_buffer for the arena (schedule/memory.py:56).
    cpu_arena_alloc_inflight_set(1);
    ARENA_BUF_ID = thvm_metal_buf_arena_alloc(ARENA_SIZE);
    cpu_arena_alloc_inflight_set(0);
    if (ARENA_BUF_ID == 0)      return 0;
    return 1;
  }
  return 0;
}

// Allocate a TenDesc whose buf is an external view into the arena.
// Returns 0 on miss (caller falls back to tensor_alloc).
static u32 arena_tensor_alloc(u32 ord_idx, Shape shape, u32 dtype) {
  if (ord_idx >= ARENA_SLOTS_LEN)        return 0;
  if (!ARENA_SLOTS[ord_idx].in_arena)    return 0;
  int is_cpu   = (CURRENT_BACKEND == &CPU_BACKEND);
  int is_metal = (CURRENT_BACKEND == &METAL_BACKEND) && metal_arena_opt_in();
  int is_cuda  = 0;
#ifdef THVM_HAS_CUDA
  is_cuda = (CURRENT_BACKEND == &CUDA_BACKEND);
#endif
  if (!is_cpu && !is_cuda && !is_metal)  return 0;
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
  // Zero the slot, as a fresh buf_alloc is zeroed (calloc, cuMemsetD8, a
  // freelist pop), for kernels that accumulate into their output; this runs at
  // emit, before any kernel of the pass fires.  The view's buffer holds the
  // arena through parent_buf_id (buf_incref, buf_decref), so the arena outlives
  // every view.  tinygrad slices the arena per buffer (schedule/memory.py:57).
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
  else if (is_metal) {
    // The view shares ARENA_BUF_ID's MTLBuffer at byte_offset off and zeroes
    // its window.  Every kernel bind applies the byte_offset, and Metal
    // hazard-tracks aliasing accesses within one MTLBuffer, so ICB-replayed
    // recycling is correct.  tinygrad: MetalAllocator._offset
    // (runtime/ops_metal.py:185), the arena slice (schedule/memory.py:57).
    d->buf_id = thvm_metal_buf_arena_view(ARENA_BUF_ID, off, nbytes);
    if (d->buf_id == 0) {
      // View alloc failed (table full): roll back the descriptor and miss so
      // the caller falls through to a plain tensor_alloc.
      TENS_NEXT--;
      return 0;
    }
  }
  return tid;
}


// Realize-boundary depth, last use and fire order.

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
    // term_resolve, as bufferize_walk_rec and visit() do, so the topo sort sees
    // the same boundary set.
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

// Raise BOUNDARY_LAST_USE of each realized boundary below from_loc to
// visiting_depth, descending through non-realized intermediates: visit()
// inlines those into the consuming kernel, so the buffer it reads is the
// realized node below them.  visited holds per-loc epoch stamps; the caller
// bumps epoch per parent instead of clearing it.
static void boundary_last_use_descend(u64 from_loc, u32 visiting_depth,
                                      u32 *visited, u32 epoch) {
  if (from_loc >= HEAP_NEXT) return;
  if (visited[from_loc] == epoch) return;
  visited[from_loc] = epoch;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return;
  if (BUFFERIZE_NODES[idx].realized) {
    if (visiting_depth > BOUNDARY_LAST_USE[idx]) {
      BOUNDARY_LAST_USE[idx] = visiting_depth;
    }
    return;     // its children are walked as a parent
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
    boundary_last_use_descend(cloc, visiting_depth, visited, epoch);
  }
}

// For each realized parent at depth D, raise BOUNDARY_LAST_USE of every
// realized boundary it reads (through non-realized intermediates) to D.  The
// freelist planner may then recycle a buffer at depth last_use + 1, when every
// reader has emitted.
static void boundary_compute_last_use(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++) BOUNDARY_LAST_USE[i] = 0;
  if (HEAP_NEXT == 0) return;
  // Epoch stamps, one u32 per loc: calloc starts them at 0 (never visited), so
  // epochs start at 1.
  u32 *visited = (u32 *)calloc(HEAP_NEXT, sizeof(u32));
  if (visited == NULL) return;
  u32 epoch = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN; i++) {
    UOpInfo *p = &BUFFERIZE_NODES[i];
    if (!p->realized)                              continue;
    u32 p_depth = BOUNDARY_DEPTH[i];
    if (p_depth == BOUNDARY_DEPTH_INVALID)         continue;
    u8 ar = uop_arity(p->op);
    u64 seen[MAX_UOP_SRC] = {0};
    u8  n_seen = 0;
    // A fresh epoch per parent: each parent's walk stands alone.
    epoch++;
    for (u8 c = 0; c < ar; c++) {
      Term child = term_resolve(heap_read(p->loc + c));
      if (term_tag(child) != TAG_UOP)         continue;
      if (term_ext(child) == UOP_KERNEL)      continue;
      u64 cloc = term_val(child);
      u8  dup  = 0;
      for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
      if (dup) continue;
      seen[n_seen++] = cloc;
      boundary_last_use_descend(cloc, p_depth, visited, epoch);
    }
  }
  free(visited);
}

// The position form of boundary_last_use_descend: raise BOUNDARY_LAST_USE_POS
// of each buffer reached to visiting_pos, the consuming parent's BOUNDARY_ORDER
// index.
static u32 boundary_index_for_loc(u64 loc);
static void boundary_last_use_pos_descend(u64 from_loc, u32 visiting_pos,
                                          u32 *visited, u32 epoch) {
  if (from_loc >= HEAP_NEXT) return;
  if (visited[from_loc] == epoch) return;
  visited[from_loc] = epoch;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return;
  // Stop only at a node that exists as a buffer: a boundary of this pass
  // (boundary_index_for_loc) or a cross-pass shared buffer (xpass_is_shared).
  // A node classify realized but the rangeify walk fused away is neither, so
  // the walk goes past it to the boundary behind it, whose slot the arena would
  // otherwise recycle while it is still read.
  if (BUFFERIZE_NODES[idx].realized
      && (boundary_index_for_loc(from_loc) != 0xFFFFFFFFu
          || xpass_is_shared(from_loc))) {
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
    boundary_last_use_pos_descend(cloc, visiting_pos, visited, epoch);
  }
}

// Fill BOUNDARY_LAST_USE_POS: for each realized parent at BOUNDARY_ORDER
// position P, raise every realized boundary it reads to P, so each boundary
// ends at its latest-firing reader, where the arena closes its lifetime.  Runs
// after topo_sort_boundaries has built BOUNDARY_ORDER and the loc hash.
static void boundary_compute_last_use_pos(void) {
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++) BOUNDARY_LAST_USE_POS[i] = 0;
  if (HEAP_NEXT == 0) return;
  // Epoch stamps, as in boundary_compute_last_use.
  u32 *visited = (u32 *)calloc(HEAP_NEXT, sizeof(u32));
  if (visited == NULL) return;
  u32 epoch = 0;
  for (u32 pos = 0; pos < BOUNDARY_ORDER_LEN; pos++) {
    u64 ploc = BOUNDARY_ORDER[pos];
    u32 i = bufferize_info_find(ploc);
    if (i == 0xFFFFFFFFu)                          continue;
    UOpInfo *p = &BUFFERIZE_NODES[i];
    if (!p->realized)                              continue;
    u8 ar = uop_arity(p->op);
    u64 seen[MAX_UOP_SRC] = {0};
    u8  n_seen = 0;
    epoch++;
    for (u8 c = 0; c < ar; c++) {
      Term child = term_resolve(heap_read(p->loc + c));
      if (term_tag(child) != TAG_UOP)         continue;
      if (term_ext(child) == UOP_KERNEL)      continue;
      u64 cloc = term_val(child);
      u8  dup  = 0;
      for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
      if (dup) continue;
      seen[n_seen++] = cloc;
      boundary_last_use_pos_descend(cloc, pos, visited, epoch);
    }
  }
  free(visited);
}

// Post-order DFS from a UOp loc, children in slot order before the node, as
// kernel_fire_by_id fires a kernel's input producers before the kernel.  Every
// visited bufferize node gets a number, so the order is one global post-order;
// the topo sort reads only the boundaries'.  visited makes each node fire once.
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

// Fill BOUNDARY_FIRE_SEQ by one post-order DFS from the realize sink.  A node
// it does not reach keeps 0xFFFFFFFF and is ordered by depth.
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

// input_slot_dedup (defined with visit()) adds (tid, term) to ke->input_tids,
// or returns the slot it already has.
static u32 input_slot_dedup(KernelEntry *ke, u32 tid, Term term);

// The unified pass's store_root holds TAG_TEN leaves (in UOP_INDEX_E buffer
// slots, ru_rewrite_subtree).  cpu_uop_walk binds a kernel's inputs to
// UOP_BUFFER leaves by instance (slot + 1), so the rewrite below replaces each
// TAG_TEN that is a kernel input with its slot's UOP_BUFFER.

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

// The UOP_BUFFER of TAG_TEN leaf tid if it is one of ke's input slots, else 0
// (the unified pass can carry tensor handles that are no kernel inputs, such as
// constants).  A tid that predates the consumer's view_resolve aliasing misses
// the exact match; tensor_view_of clones share buf_id, so a second scan matches
// by buffer.
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

// As unified_rewrite_buffer_for_tid, but a tid no slot holds gets a new input
// slot, mirrored into cached_lift.in_bufs so cpu_uop_walk binds a buffer for
// it: the unified pass can inline a producer BUFFERIZE whose value reads a
// TAG_TEN the consumer's inputs lack.
static Term unified_rewrite_buffer_for_tid_extend(KernelEntry *ke, u32 tid) {
  Term repl = unified_rewrite_buffer_for_tid(ke, tid);
  if (repl != 0) return repl;
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  Term ten_term = term_new(0, TAG_TEN, td->dtype, tid);
  u32 slot = input_slot_dedup(ke, tid, ten_term);
  u32 dtype = ke->input_dtypes[slot];
  Term buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                             td->view.shape.ndim, td->view.shape.dims,
                             slot + 1);
  // Mirror into cached_lift.in_bufs and n_inputs, so cpu_uop_walk's
  // ctx.in_terms[slot] resolves to the BUFFER instance just emitted.
  kernel_lift_reserve(&ke->cached_lift, ke->n_inputs);
  ke->cached_lift.in_bufs[slot] = buf;
  if (ke->cached_lift.n_inputs < ke->n_inputs) {
    ke->cached_lift.n_inputs = ke->n_inputs;
  }
  return buf;
}

// Map a UOP_BUFFERIZE (an upstream realized boundary) to its producer tid
// through BOUNDARY_BUFFERIZE_TERM and BOUNDARY_TID and build that input slot's
// UOP_BUFFER.  0 when it is none of this kernel's inputs (its own output, or a
// boundary not wired into input_tids).
static Term unified_rewrite_buffer_for_bufferize(KernelEntry const *ke,
                                                 Term buf) {
  u32 bi = boundary_buf_term_index(buf);
  if (bi != 0xFFFFFFFFu) {
    return unified_rewrite_buffer_for_tid(ke, BOUNDARY_TID[bi]);
  }
  // No identity match (the consumer-side INDEX_E's buffer can differ from the
  // stored BUFFERIZE in addrspace or closed ranges): match by value, since two
  // BUFFERIZEs of one value subtree are one buffer.
  if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFERIZE) {
    Term want_value = uop_bufferize_value(buf);
    u32 vbi = boundary_buf_value_index(want_value, buf);
    if (vbi != 0xFFFFFFFFu) {
      return unified_rewrite_buffer_for_tid(ke, BOUNDARY_TID[vbi]);
    }
  }
  return 0;
}

// As unified_rewrite_buffer_for_bufferize, but a producer tid that is not yet a
// kernel input gets a new slot, the BUFFERIZE counterpart of
// unified_rewrite_buffer_for_tid_extend.  Otherwise a bare UOP_BUFFERIZE leaf
// reaches the renderer, whose rmu_buf_name falls back to `buf<loc>`, an
// undeclared identifier.
static Term unified_rewrite_buffer_for_bufferize_extend(KernelEntry *ke,
                                                        Term buf) {
  Term repl = unified_rewrite_buffer_for_bufferize(ke, buf);
  if (repl != 0) return repl;
  u32 tid = 0;
  u32 bi = boundary_buf_term_index(buf);
  if (bi != 0xFFFFFFFFu) tid = BOUNDARY_TID[bi];
  if (tid == 0 && term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFERIZE) {
    Term want_value = uop_bufferize_value(buf);
    u32 vbi = boundary_buf_value_index(want_value, buf);
    if (vbi != 0xFFFFFFFFu) tid = BOUNDARY_TID[vbi];
  }
  if (tid == 0) return 0;
  return unified_rewrite_buffer_for_tid_extend(ke, tid);
}

// A TLam-bound TVAR that visit() registered as an input slot (input_tids[slot]
// 0, input_terms[slot] the var, its shape from the lam_shape side table): build
// its UOP_BUFFER (instance slot + 1), so cpu_uop_walk binds the buffer the var
// resolves to at fire time.
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

// A substitution map applied while rewriting (tinygrad's UOp.substitute over a
// closed-range scope): a Term identical to from[i] becomes to[i], so an inlined
// BUFFERIZE's closed ranges iterate in the consumer's ranges.  It accumulates
// across a nested BUFFERIZE chain, so it holds more than MAX_DIM; the
// axis-table inline declines rather than drop a binding when it is full.
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
static int slot_needs_chain_skip(u32 tid);

// addr with every RANGE at 0.
static Term unified_addr_at_zero(Term t, u32 depth) {
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP || depth > MAT_MAX_WALK_DEPTH) {
    return r;
  }
  if (term_ext(r) == UOP_RANGE) {
    return uop_const(DT_INT32, 0);
  }
  u8   ar = uop_arity(term_ext(r));
  Term srcs[MAX_UOP_SRC] = {0};
  int  changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old = heap_read(term_val(r) + i);
    srcs[i]  = unified_addr_at_zero(old, depth + 1);
    changed |= srcs[i] != old;
  }
  return changed ? uop_graph_rebuild_with_srcs(r, srcs) : r;
}

// The address that reads a one-element BUFFER: 0, the unified addr being the
// consumer's multi-axis expression, which can pass the one element.  A slot
// whose tid is an offset or chained view binds the raw buffer
// (unified_fold_chain_scan), and several such views of one buffer share it, so
// there the addr keeps its composed view, at the coordinates 0.
static Term unified_one_elem_addr(UnifiedRewriteState const *st, Term buf, Term addr) {
  KernelEntry const *ke   = st->ke;
  u32                inst = uop_buffer_inst_get(buf);
  if (ke != NULL && ke->input_tids != NULL && inst > 0 && inst - 1 < ke->n_inputs
      && slot_needs_chain_skip(ke->input_tids[inst - 1])) {
    return unified_addr_at_zero(addr, 0);
  }
  return uop_const(DT_INT32, 0);
}

static Term unified_rewrite_rec(UnifiedRewriteState *st, Term t, u32 depth) {
  return unified_rewrite_rec_sub(st, NULL, t, depth);
}

// Inline a BUFFERIZE through the per-axis index side table (filled by
// ru_index_axes_register): closed_range[i] binds to the consumer's in_rng at
// the producer's i-th realized axis, without decomposing the flat INDEX_E addr.
// tinygrad's INDEX carries one range per axis (`new_src.index(*src_rngs)`,
// schedule/indexing.py:93); thvm's INDEX_E holds a flat addr, so the table
// keeps them.  0 for a table miss, which both callers treat as fatal.
__attribute__((noinline))
static Term try_inline_bufferize_via_axis_table(
    UnifiedRewriteState *st, UnifiedSubst const *sub,
    Term resolved, Term inner_buf, Term v, u32 n_ranges, u32 depth) {
  Term ax_rngs[MAX_DIM];
  u8 ax_n = rangeify_unified_index_axes_lookup(
      term_val(resolved), ax_rngs, MAX_DIM);
  if (ax_n < n_ranges) return 0;
  // The producer node: through BOUNDARY_BUFFERIZE_TERM for a boundary that
  // became a kernel, else RU_BUFFERIZE_TERM for an orphan BUFFERIZE that did
  // not.
  u32 prod_idx = 0xFFFFFFFFu;
  {
    u32 bi = boundary_buf_term_index(inner_buf);
    if (bi != 0xFFFFFFFFu) prod_idx = bufferize_info_find(BOUNDARY_ORDER[bi]);
  }
  if (prod_idx == 0xFFFFFFFFu) {
    prod_idx = rangeify_unified_node_idx_for_bufferize(inner_buf);
  }
  if (prod_idx == 0xFFFFFFFFu) return 0;
  u32 prod_ndim = rangeify_unified_out_ndim_at(prod_idx);
  // realized_axes[i]: the producer output axis of the i-th closed range.  A
  // realized axis whose out_rng collapsed to CONST(0) (a keepdim size-1 axis)
  // has no closed range (as in ru_collect_closed_ranges), so filtering on a
  // real UOP_RANGE keeps n_realized == n_ranges and binds by position, not
  // extent.
  int prod_full = rangeify_unified_realized_full_at(prod_idx);
  u32 axes_mask = rangeify_unified_axes_mask_at(prod_idx);
  u32 realized_axes[MAX_DIM] = {0};
  u32 n_realized = 0;
  for (u32 a = 0; a < prod_ndim && a < MAX_DIM; a++) {
    int axis_realized = prod_full || (axes_mask & (1u << a));
    if (!axis_realized) continue;
    Term r = rangeify_unified_out_rng_at(prod_idx, a);
    if (r == 0 || term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) continue;
    if (n_realized < MAX_DIM) realized_axes[n_realized] = a;
    n_realized++;
  }
  if (n_realized != n_ranges || ax_n < prod_ndim) return 0;
  // Decline rather than truncate, as the stride match does: every closed range
  // needs a binding.
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
    // ax_rngs are the consumer's in_rngs as of this INDEX_E's construction.
    // When this BUFFERIZE is itself inlined into a further consumer, the active
    // subst maps them to that consumer's live ranges, so the range goes through
    // it first, as tinygrad always indexes at the current consumer's ranges
    // (schedule/indexing.py:79).  An in_rng may be an expression over ranges (a
    // PAD's gated index), so the subst rewrites inside it.
    Term to_rng = ax_rngs[ax];
    if (sub != NULL && sub->n > 0) {
      to_rng = uop_substitute(to_rng, sub->from, sub->to, sub->n);
    }
    new_sub.from[new_sub.n] = cr;
    new_sub.to  [new_sub.n] = to_rng;
    new_sub.n++;
  }
  return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
}

static int bufferize_holds_scatter(Term b) {
  Term v = uop_bufferize_value(b);
  return term_tag(v) == TAG_UOP && term_ext(v) == UOP_SCATTER;
}

// Recurse into v under sub extended by (from -> to).  Out of line so the
// UnifiedSubst (about 1 KB) sits only on this path's frames: inlined, the
// compiler hoists it and the bufferize helpers' arrays into every level of
// unified_rewrite_rec_sub, and a deep elementwise chain overflows the stack.
__attribute__((noinline))
static Term unified_rewrite_rec_extend(UnifiedRewriteState *st,
                                       UnifiedSubst const *sub,
                                       Term from, Term to, Term v, u32 depth) {
  // A dropped binding would leave its closed range unbound, read as iteration 0.
  if (sub != NULL && sub->n >= UNIFIED_SUBST_CAP) {
    thvm_fatal("materialize: a nested inline chain binds more than UNIFIED_SUBST_CAP ranges");
  }
  UnifiedSubst new_sub;
  new_sub.n = 0;
  if (sub != NULL) {
    for (u32 i = 0; i < sub->n; i++) {
      new_sub.from[new_sub.n] = sub->from[i];
      new_sub.to  [new_sub.n] = sub->to  [i];
      new_sub.n++;
    }
  }
  new_sub.from[new_sub.n] = from;
  new_sub.to  [new_sub.n] = to;
  new_sub.n++;
  return unified_rewrite_rec_sub(st, &new_sub, v, depth + 1);
}

static Term unified_rewrite_rec_sub(UnifiedRewriteState *st,
                                    UnifiedSubst const *sub,
                                    Term t, u32 depth) {
  if (depth > MAT_MAX_WALK_DEPTH) return t;
  Term resolved = term_resolve(t);
  // The substitution first, so a RANGE leaf inside an inlined value is rebound
  // before any other branch (TAG_TEN -> UOP_BUFFER, BUFFERIZE -> input slot)
  // sees it.
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

  // A UOP_BUFFERIZE leaf is an upstream boundary's output buffer: replace it
  // with its input slot's UOP_BUFFER and do not recurse, so the consumer's
  // INDEX reads the BUFFER that cpu_uop_walk binds.  With no boundary match
  // fall through: the BUFFERIZE may wrap an in-kernel intermediate whose value
  // still holds TAG_TEN leaves to rewrite.
  if (term_ext(resolved) == UOP_BUFFERIZE) {
    // The extend variant adds the slot when the consumer's input_tids lack the
    // producer tid (common in backward kernels); otherwise the BUFFERIZE
    // reaches the renderer's `buf<loc>` fallback, an undeclared identifier.
    Term repl = unified_rewrite_buffer_for_bufferize_extend(st->ke, resolved);
    if (repl != 0) return repl;
  }

  // INDEX_E(BUFFERIZE(value, closed_ranges), addr) whose BUFFERIZE is no
  // kernel-input boundary wraps an in-kernel intermediate (a scalar, a reduce
  // to scalar, a softmax denominator), and cpu_uop_walk has no BUFFERIZE case:
  // inline value, rebinding its closed ranges to the matching parts of addr.
  //   n_ranges 0         a one-element buffer: inline the value
  //   n_ranges 1, RANGE  bind the closed range to that RANGE
  //   otherwise          bind each closed range positionally through the
  //                      per-axis side table; a missing entry is fatal
  if (term_ext(resolved) == UOP_INDEX_E) {
    Term inner_buf = term_resolve(heap_read(term_val(resolved) + 0));
    Term addr_term = term_resolve(heap_read(term_val(resolved) + 1));
    // Run the addr through the active subst first, so a closed-range leaf an
    // outer inline bound carries into this nested one (chains of nested
    // BUFFERIZEs).
    if (sub != NULL) {
      addr_term = unified_rewrite_rec_sub(st, sub, addr_term, depth + 1);
    }
    // The _extend lookup: a realized boundary's BUFFERIZE is read as a kernel
    // input, not inlined and recomputed, even when reached through an inlined
    // intermediate (a stacked conv's relu(conv1) drags conv1's BUFFERIZE in).
    // A SCATTER's buffer is never inlined: its value is a sequence of stores.
    if (term_tag(inner_buf) == TAG_UOP
        && term_ext(inner_buf) == UOP_BUFFERIZE
        && !bufferize_holds_scatter(inner_buf)
        && unified_rewrite_buffer_for_bufferize_extend(st->ke, inner_buf) == 0) {
      Term v = uop_bufferize_value(inner_buf);
      // A CONST value is a literal: nothing to rewrite.
      if (v != 0 && term_tag(v) == TAG_UOP && term_ext(v) == UOP_CONST) {
        return v;
      }
      u32 n_ranges = uop_bufferize_n_ranges(inner_buf);
      if (v != 0 && n_ranges == 0) {
        // Every read of a one-element buffer gets the same value.  Recurse
        // under the same subst, so RANGE leaves it binds keep their bindings.
        return unified_rewrite_rec_sub(st, sub, v, depth + 1);
      }
      // One closed range read at a RANGE: rebind the closed range to it while
      // rewriting the value (a per-row reduce to scalar: cross-entropy, a
      // softmax denominator, a layer-norm mean).  A compound addr takes the
      // side-table path below: substituting it whole decodes strided or
      // transposed views wrong (a pool MAX-then-sum gradient).  The side table
      // goes first here too: a partly realized producer's addr spans its open
      // axes as well, and folds to one of their RANGEs when the closed
      // coordinate is 0 (x[0:1] + x[1:2] of a lazy 2-D x).
      if (v != 0 && n_ranges == 1
          && term_tag(addr_term) == TAG_UOP
          && term_ext(addr_term) == UOP_RANGE) {
        Term axis_hit = try_inline_bufferize_via_axis_table(st, sub, resolved, inner_buf, v, 1,
                                                            depth);
        if (axis_hit != 0) return axis_hit;
        Term old_r = uop_bufferize_range_at(inner_buf, 0);
        if (old_r == addr_term) {
          // Already the consumer's range: no substitution.
          return unified_rewrite_rec_sub(st, sub, v, depth + 1);
        }
        if (old_r != 0) {
          // Extend any active subst with (old_r -> addr_term), so each
          // BUFFERIZE of a chain rebinds its own closed range under the outer
          // bindings.
          return unified_rewrite_rec_extend(st, sub, old_r, addr_term, v, depth);
        }
      }
      // One closed range read at a compound addr: a per-row reduce to scalar
      // read by a row x col consumer (softmax `f - s.max(kd)`, layer-norm
      // `t - mean`), its keepdim col axis collapsed to CONST(0).
      if (v != 0 && n_ranges == 1
          && (term_tag(addr_term) != TAG_UOP
              || term_ext(addr_term) != UOP_RANGE)) {
        // Bind it positionally, to the consumer's range at the producer's
        // realized output axis, through the per-axis side table, never by
        // extent, which is ambiguous whenever two consumer axes share an
        // extent.
        Term axis_hit = try_inline_bufferize_via_axis_table(
            st, sub, resolved, inner_buf, v, n_ranges, depth);
        if (axis_hit != 0) return axis_hit;
        thvm_fatal("BUFFERIZE has no per-axis side-table entry");
      }
      // Several closed ranges: bind each through the side table the same way.
      if (v != 0 && n_ranges >= 2 && n_ranges <= UNIFIED_SUBST_CAP) {
        Term axis_hit = try_inline_bufferize_via_axis_table(
            st, sub, resolved, inner_buf, v, n_ranges, depth);
        if (axis_hit != 0) return axis_hit;
        thvm_fatal("BUFFERIZE has no per-axis side-table entry");
      }
    }
    // Not inlined: rebuild the INDEX_E with the substituted addr; the buffer
    // side takes the generic recursion.
    if (sub != NULL && addr_term != term_resolve(heap_read(term_val(resolved) + 1))) {
      Term buf_rw = unified_rewrite_rec_sub(st, sub, heap_read(term_val(resolved) + 0),
                                            depth + 1);
      if (term_tag(buf_rw) == TAG_UOP && term_ext(buf_rw) == UOP_BUFFER
          && uop_buffer_numel(buf_rw) == 1) {
        addr_term = unified_one_elem_addr(st, buf_rw, addr_term);
      }
      Term new_index = uop_index_e(buf_rw, addr_term);
      // Carry the per-axis side-table entry (the consumer's in_rngs) over to
      // the rebuilt INDEX_E, so a later inline still finds it.
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

  // The memo is keyed by the input Term, so it holds only without a subst: a
  // substituted result would poison the unsubstituted key.
  if (sub == NULL) {
    Term hit = 0;
    if (unified_rewrite_memo_lookup(st, resolved, &hit)) return hit;
  }

  // UOP_LOAD (tinygrad's Ops.LOAD) is an identity around its src at the value
  // layer, and cpu_uop_walk has no LOAD case (a LOAD around an INDEX_E reads
  // 0): strip it.
  if (op == UOP_LOAD) {
    Term inner = unified_rewrite_rec_sub(st, sub, heap_read(loc), depth + 1);
    if (sub == NULL) unified_rewrite_memo_insert(st, resolved, inner);
    return inner;
  }

  // Movement ops in a value subtree are structural markers: their dims describe
  // the producer's iteration shape, the surrounding INDEX_E addr already
  // encodes the read, and RANGE leaves map to walker slots by axis id.
  // cpu_uop_walk has no movement cases, so strip them.
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
  if (op == UOP_INDEX_E
      && term_tag(srcs[0]) == TAG_UOP && term_ext(srcs[0]) == UOP_BUFFER
      && uop_buffer_numel(srcs[0]) == 1) {
    Term elem = unified_one_elem_addr(st, srcs[0], srcs[1]);
    if (srcs[1] != elem) { srcs[1] = elem; changed = 1; }
  }
  Term out = changed ? uop_graph_rebuild_with_srcs(resolved, srcs) : resolved;
  if (sub == NULL) unified_rewrite_memo_insert(st, resolved, out);
  return out;
}

// Broadcast-collapsed REDUCE repair.  A UOP_REDUCE stores only axis ids, and
// uwalk_run_reduce takes each extent from a RANGE with that id in the body
// (fatal if none).  Inlining a producer invariant over a reduce axis (a
// per-channel mean broadcast, a bias.expand(...).sum() gradient, a replicate
// pad's gradient) can drop that axis's RANGEs.  ru_reduce_unparented repairs
// this before the inline and repair_collapsed_reduces_rec after it, tinygrad's
// reduce_unparented (codegen/simplify.py:81-91), which runs on the final
// kernel: a SUM becomes `body * extent`, a MAX, MIN or bool SUM the body, with
// extents from the pre-inline tree.  tinygrad's REDUCE carries its ranges
// (schedule/indexing.py:107-113).

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

static void axext_collect_rec(AxExtMap *m, Term t, u32 *visited, u64 cap,
                              u32 epoch, u32 depth) {
  if (depth > MAT_MAX_WALK_DEPTH) return;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return;
  // Each node once: m is a set keyed by axis id (axext_add dedups), so the
  // result does not depend on how many paths reach a node, and walking a shared
  // node once per incoming edge is exponential in depth.  visited holds
  // per-call epoch stamps (AXEXT_VISITED).
  u64 loc = term_val(r);
  if (loc < cap && visited[loc] == epoch) return;
  if (loc < cap) visited[loc] = epoch;
  u8 op = term_ext(r);
  if (op == UOP_RANGE) {
    axext_add(m, uop_range_axis_id(r), uop_range_extent(r));
    return;
  }
  // A BUFFERIZE's closed ranges carry the producer's realized output-axis
  // extents, the axes a consumer reduce contracts.
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
    axext_collect_rec(m, heap_read(loc + i), visited, cap, epoch, depth + 1);
}

// The epoch-stamped visited buffer of axext_collect: grown to HEAP_NEXT and
// reused across calls, an epoch bump replacing a clear.  Never freed.
static u32 *AXEXT_VISITED     = NULL;
static u64  AXEXT_VISITED_CAP = 0;
static u32  AXEXT_EPOCH       = 0;

// Collect the axis extents under t.  If growing the buffer fails, cap 0 walks
// unmemoized: correct, but exponential on a shared DAG.
static void axext_collect(AxExtMap *m, Term t) {
  u64 want = HEAP_NEXT;
  if (want > AXEXT_VISITED_CAP) {
    u32 *grown = (u32 *)realloc(AXEXT_VISITED, want * sizeof(u32));
    if (grown != NULL) {
      // Zero only the grown tail: older stamps are below the next epoch, so
      // they read as unvisited.
      memset(grown + AXEXT_VISITED_CAP, 0,
             (want - AXEXT_VISITED_CAP) * sizeof(u32));
      AXEXT_VISITED = grown;
      AXEXT_VISITED_CAP = want;
    }
  }
  u64 cap = (AXEXT_VISITED != NULL) ? AXEXT_VISITED_CAP : 0;
  // An epoch wrapping to 0 (never visited) clears the buffer and restarts at 1.
  if (++AXEXT_EPOCH == 0) {
    if (AXEXT_VISITED != NULL)
      memset(AXEXT_VISITED, 0, AXEXT_VISITED_CAP * sizeof(u32));
    AXEXT_EPOCH = 1;
  }
  axext_collect_rec(m, t, AXEXT_VISITED, cap, AXEXT_EPOCH, 0);
}

// 1 if t reads a UOP_RANGE with axis id aid.  UOP_BUFFER is opaque; no
// BUFFERIZE survives the inline, so everything else is descended.
static int subtree_uses_axis_mat(Term t, u32 aid, u32 depth) {
  if (depth > MAT_MAX_WALK_DEPTH) return 0;
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

static Term repair_collapsed_reduces_rec(Term t, AxExtMap const *m, u32 depth) {
  if (depth > MAT_MAX_WALK_DEPTH) return t;
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) return t;
  u8 op = term_ext(r);
  if (op == UOP_BUFFER) return r;
  u8 ar = uop_arity(op);
  u64 loc = term_val(r);
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
  // Axes the rebuilt body no longer reads: a SUM gains each one's extent as a
  // factor, a PROD as a power, a MAX or MIN drops it (reducing a body constant
  // over an axis gives the body).
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
      // No recoverable extent: keep the original reduce.
      if (ext == 0) { bail = 1; break; }
      any_collapsed = 1;
      prod *= (u64)ext;
    }
  }
  if (bail || !any_collapsed) {
    // A nested reduce may still have been repaired: rebuild the shell over the
    // original axes.
    if (!changed) return r;
    u32 all_axes[MAX_DIM];
    for (u32 a = 0; a < n_axes && a < MAX_DIM; a++) all_axes[a] = uop_reduce_axis(r, a);
    return uop_reduce_multi(kind, n_axes, all_axes, body);
  }
  Term new_body = body;
  u32  dt       = DT_FP32;
  (void)term_dtype_in(r, 0, &dt);
  if (kind == REDUCE_SUM && prod != 1 && !dtype_is_bool(dt)) {
    new_body = uop_binary(UOP_MUL, body, ru_const_of(dt, prod));
  } else if (kind == REDUCE_PROD && !dtype_is_bool(dt)) {
    new_body = uop_reduce_pow(body, prod);
  }
  // Rebuild the reduce over only the surviving (still-referenced) axes;
  // if none survive, the reduce degenerates to its (scaled) body.
  if (n_kept == 0) return new_body;
  return uop_reduce_multi(kind, n_kept, kept_axes, new_body);
}

// Walker rewrite of one store: substitute its TAG_TEN and BUFFERIZE leaves,
// then repair each REDUCE that lost a reduce axis's RANGE leaves to the inline
// (repair_collapsed_reduces_rec, with extents from the pre-inline store).
static Term unified_rewrite_store_for_walker(Term store, void *ctx) {
  UnifiedRewriteState *st = (UnifiedRewriteState *)ctx;
  Term rewritten = unified_rewrite_rec(st, store, 0);
  AxExtMap axm;
  axm.n = 0;
  axext_collect(&axm, store);
  return repair_collapsed_reduces_rec(rewritten, &axm, 0);
}

static Term unified_store_root_for_walker(KernelEntry *ke, Term root) {
  if (root == 0 || ke == NULL) return root;
  UnifiedRewriteState st;
  memset(&st, 0, sizeof(st));
  st.ke = ke;
  return uop_root_map_stores(root, unified_rewrite_store_for_walker, &st);
}

// Visited set of the emit-time DAG scans: a resolved UOp's loc is stable
// (hash-consing), so a generation-stamped loc-indexed array marks and tests in
// O(1).  A BufferizeScanArena binds the shared stamp buffer; a
// BufferizeScanVisited holds it and its generation.
typedef struct {
  u32  *stamp;     // loc-indexed; stamp[loc] == gen iff loc marked in gen
  u32   cap;       // stamp length (0: memoize nothing)
  u32   next_gen;  // monotonic; bumped per fresh context
} BufferizeScanArena;

typedef struct {
  BufferizeScanArena *a;
  u32                 gen;
} BufferizeScanVisited;

// The stamp buffer shared by every bufferize scan: grown to HEAP_NEXT and
// reused, each open taking a new generation instead of clearing it.  The u32
// generation advances globally, so a prior scan's marks read as unvisited; a
// wrap clears the buffer.
static u32 *BUFSCAN_STAMP     = NULL;
static u64  BUFSCAN_STAMP_CAP = 0;
static u32  BUFSCAN_GEN       = 0;

// Open a top-level scan on the shared stamp buffer (grown to HEAP_NEXT if
// needed) with a fresh generation.  If growing fails, cap 0: seen() always
// answers 0, unmemoized but correct.
static void bufferize_scan_open(BufferizeScanVisited *v, BufferizeScanArena *a) {
  u64 want = HEAP_NEXT;
  if (want > BUFSCAN_STAMP_CAP) {
    u32 *grown = (u32 *)realloc(BUFSCAN_STAMP, want * sizeof(u32));
    if (grown != NULL) {
      memset(grown + BUFSCAN_STAMP_CAP, 0,
             (want - BUFSCAN_STAMP_CAP) * sizeof(u32));
      BUFSCAN_STAMP = grown;
      BUFSCAN_STAMP_CAP = want;
    }
  }
  // A generation wrapping to 0 (never visited) clears the buffer.
  if (++BUFSCAN_GEN == 0) {
    if (BUFSCAN_STAMP != NULL)
      memset(BUFSCAN_STAMP, 0, BUFSCAN_STAMP_CAP * sizeof(u32));
    BUFSCAN_GEN = 1;
  }
  a->stamp    = BUFSCAN_STAMP;
  a->cap      = (BUFSCAN_STAMP != NULL) ? (u32)BUFSCAN_STAMP_CAP : 0;
  a->next_gen = BUFSCAN_GEN;
  v->a   = a;
  v->gen = a->next_gen;
}

static void bufferize_scan_close(BufferizeScanArena *a) {
  // The stamp buffer is shared: just detach the arena from it.
  a->stamp = NULL;
  a->cap = 0;
}

// A child context on the parent's arena with a new generation, invalidating its
// marks in O(1).  It advances the global generation, not only the arena's copy,
// so it cannot collide with a later bufferize_scan_open's.
static void bufferize_scan_fresh(BufferizeScanVisited *child,
                                 BufferizeScanVisited const *parent) {
  child->a   = parent->a;
  if (++BUFSCAN_GEN == 0) {
    if (BUFSCAN_STAMP != NULL)
      memset(BUFSCAN_STAMP, 0, BUFSCAN_STAMP_CAP * sizeof(u32));
    BUFSCAN_GEN = 1;
  }
  parent->a->next_gen = BUFSCAN_GEN;
  child->gen = BUFSCAN_GEN;
}

static int bufferize_scan_seen(BufferizeScanVisited *v, Term t) {
  BufferizeScanArena *a = v->a;
  u64 loc = term_val(t);
  if (loc >= a->cap) return 0;          // OOM arena or out-of-range: never memoized
  if (a->stamp[loc] == v->gen) return 1;
  a->stamp[loc] = v->gen;
  return 0;
}

// 1 iff a store of a gather / scatter kernel still holds a TAG_TEN, a
// BUFFERIZE, or a GATHER / SCATTER: a leaf the walker rewrite did not bind
// to an input slot, or an indexed op the lowering did not replace.  The
// backends read such a node as 0.
static int unbound_leaf_scan_rec(BufferizeScanVisited *v, Term t, u32 depth) {
  if (depth > MAT_MAX_WALK_DEPTH) return 0;
  Term r = term_resolve(t);
  if (term_tag(r) == TAG_TEN) return 1;
  if (term_tag(r) != TAG_UOP) return 0;
  if (bufferize_scan_seen(v, r)) return 0;
  u8 op = term_ext(r);
  if (op == UOP_BUFFERIZE || op == UOP_GATHER || op == UOP_SCATTER) return 1;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (unbound_leaf_scan_rec(v, heap_read(term_val(r) + i), depth + 1)) return 1;
  }
  return 0;
}

static int unbound_leaf_in_store(Term store, void *ctx) {
  return unbound_leaf_scan_rec((BufferizeScanVisited *)ctx, store, 0);
}

static int indexed_kernel_has_unbound_leaf(Term root) {
  BufferizeScanArena arena;
  BufferizeScanVisited v;
  bufferize_scan_open(&v, &arena);
  int rc = uop_root_each_store(root, unbound_leaf_in_store, &v);
  bufferize_scan_close(&arena);
  return rc;
}

// input_chain_composed, for slots whose tid has a non-trivial layout (a
// prior_views chain, a non-contiguous or offset view).  The unified pass put
// the movement chain into the INDEX_E addr over the underlying buffer, while
// visit() also routed the movement through view_resolve, so the slot's tid is a
// chained or strided descriptor; unflagged, the backend would pre-gather the
// public view or offset the pointer, applying the movement twice.
// ru_rewrite_subtree flags a chain it composed (RU_TID_CHAIN_COMPOSED, copied
// by input_slot_dedup); the scan below flags the rest, the slots an INDEX_E in
// the rewritten store_root reads whose tid visit() minted later.
typedef struct {
  u8  *flagged;           // flagged[i] = 1 iff input slot i was flagged
  u32  n_slots;
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
  if (depth > MAT_MAX_WALK_DEPTH) return;
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
            && !rangeify_unified_tid_chain_composed(tid)
            && slot < marks->n_slots) {
          marks->flagged[slot] = 1;
        }
      }
    }
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    unified_fold_chain_scan(ke, heap_read(loc + i), marks, v, depth + 1);
  }
}

typedef struct {
  KernelEntry const    *ke;
  ChainFoldMarks       *marks;
  BufferizeScanVisited *v;
} FoldChainScan;

static int fold_chain_scan_store(Term store, void *ctx) {
  FoldChainScan *s = (FoldChainScan *)ctx;
  unified_fold_chain_scan(s->ke, store, s->marks, s->v, 0);
  return 0;
}

// The marks hold one flag per input slot; unified_fold_chain_commit_flags
// consumes them.
static Term unified_fold_chain(KernelEntry *ke, Term root, ChainFoldMarks *marks) {
  marks->flagged = NULL;
  marks->n_slots = 0;
  if (root == 0 || ke == NULL || ke->input_tids == NULL || ke->n_inputs == 0) {
    return root;
  }
  marks->flagged = (u8 *)calloc(ke->n_inputs, sizeof(u8));
  if (marks->flagged == NULL) {
    thvm_fatal("unified_fold_chain: out of memory");
  }
  marks->n_slots = ke->n_inputs;
  BufferizeScanArena arena;
  BufferizeScanVisited v;
  bufferize_scan_open(&v, &arena);
  FoldChainScan scan = { ke, marks, &v };
  uop_root_each_store(root, fold_chain_scan_store, &scan);
  bufferize_scan_close(&arena);
  return root;
}

static void unified_fold_chain_commit_flags(KernelEntry *ke,
                                             ChainFoldMarks *marks) {
  if (ke != NULL && ke->input_chain_composed != NULL) {
    for (u32 slot = 0; slot < ke->n_inputs && slot < marks->n_slots; slot++) {
      if (marks->flagged[slot]) {
        ke->input_chain_composed[slot] = 1;
      }
    }
  }
  free(marks->flagged);
  marks->flagged = NULL;
  marks->n_slots = 0;
}

// A "stranded" range is a UOP_RANGE whose axis_id cpu_uop_walk does not
// iterate: it sets up loop slots only for the ranges in the STORE's addr
// (uwalk_split_ranges) and the REDUCE axes (uwalk_run_reduce), so any
// other range reads iter=0 and the kernel computes wrong results.
// stranded_range_collect_addr gathers the addr axes into a RangeAxisSet.
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
  if (depth > MAT_MAX_WALK_DEPTH) return;
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
  // A BUFFERIZE is a closed unit with its own closed-range scope, never the
  // consumer's; the walker reads it as a memory leaf (uwalk_resolve_buf).
  if (op == UOP_KERNEL || op == UOP_BUFFER || op == UOP_BUFFERIZE) return;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    stranded_range_collect_addr(iter_axes, v, heap_read(loc + i), depth + 1);
  }
}

// Deep strand check, for the inline of a partial realize (topo_sort_boundaries
// makes a strand fatal).  It descends into nested BUFFERIZEs as if each were
// inlined, their closed ranges bound, so a free range inside a realized one
// counts too; a UOP_RANGE that is no such closed range and no enclosing REDUCE
// axis would strand.  Carrying the bound set down each path is exponential on
// shared subtrees; the per-node form is memoizable:
//   RANGE(aid)      { aid }
//   KERNEL, BUFFER  { }
//   BUFFERIZE       free_aids(value) minus its closed ranges
//   REDUCE          free_aids(src) minus its reduce axes
//   other           the union over the children
// A node strands iff some aid in free_aids(value) is neither in the consumer's
// iter set nor fuse-bound.
typedef struct {
  RangeAxisSet **sets;    // loc-indexed; sets[loc] = node's free-aid set (owned)
  u8            *done;    // loc-indexed; 1 once computed (cache "no free aids" too)
  u32            cap;
} StrandFreeCache;

static RangeAxisSet const *strand_free_aids(StrandFreeCache *c, Term t,
                                            u32 depth, RangeAxisSet *out) {
  // out is the caller's scratch: a cache hit returns the cached set; otherwise
  // out is filled, cached as a copy, and the copy returned.
  out->n = 0;
  if (depth > MAT_MAX_WALK_DEPTH) return out;
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
    // A hash-cons-aliased REDUCE or realized-scope axis from a disjoint scope
    // is bound when the fusing conv-backward product splices into its
    // contraction reduce: not stranded.
    if (rangeify_unified_aid_is_fuse_bound(aid)) continue;
    return 1;
  }
  return 0;
}

// The loc-keyed free-aid cache of bufferize_strand_check_deep.
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

// Would inlining this BUFFERIZE strand a range in its consumer?  A free
// UOP_RANGE in its value that is no closed range and no enclosing REDUCE axis
// stays free after the inline (the partial conv _pool reshapes).  c is the
// caller's free-aid cache, shared across the boundary scan so each node is
// computed once.
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
  // The bound set is the iteration the consumer threads in on inline: every
  // RANGE across the node's out_rngs, not only its closed ranges.  A _pool
  // reshape's value carries the consumer's output-position ranges free, and the
  // consuming conv reduce iterates exactly those, so they are not stranded.
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

// Compute the static numel of a UOP_BUFFER (product of dims).  Returns
// 0 if t is not a UOP_BUFFER.
static u64 uop_buffer_numel(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  u32 ndim = uop_buffer_ndim(t);
  u64 n = 1;
  for (u32 d = 0; d < ndim; d++) n *= (u64)uop_buffer_dim(t, d);
  return n;
}

// Opcode name for the stderr diagnostics.
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
    case UOP_POW:         return "POW";
    case UOP_THREEFRY:    return "THREEFRY";
    case UOP_NEG:         return "NEG";
    case UOP_RECIP:       return "RECIP";
    case UOP_EXP2:        return "EXP2";
    case UOP_SIN:         return "SIN";
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
    case UOP_GATHER:      return "GATHER";
    case UOP_SCATTER:     return "SCATTER";
    default:              return "?";
  }
}

// 1 iff a direct consumer of producer_loc is a UOP_SHRINK with a nonzero begin.
// A realized_full view boundary with a collapsed unit axis fails the nr == ond
// gate and would be inlined, and the inline drops the SHRINK's begin on the
// collapsed axis (a head split `qkv[:,:,i]` reads slice 0 for every i).
// Realized, the consumer indexes it at its own offset, as tinygrad indexes a
// realized STAGE (schedule/indexing.py:82-93).
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

// 1 iff some out axis of node i is a collapsed unit axis: its out_rng is no
// UOP_RANGE (ru_new_range makes a size-1 axis CONST(0)).  Such a node has fewer
// closed ranges than out_ndim purely from the collapse, so the nr == ond
// effectively-full gate fails although it is a full realize.
static int boundary_has_collapsed_unit_axis(u32 node_idx) {
  u32 ond = rangeify_unified_out_ndim_at(node_idx);
  for (u32 a = 0; a < ond; a++) {
    Term r = rangeify_unified_out_rng_at(node_idx, a);
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 1;
  }
  return 0;
}

// 1 iff node i's only collapsed unit axis is its leading one: the M == 1 matmul
// input `{1, K}` (a decode LM head's input).  A trailing keepdim unit axis
// (`{..., 1}`, a softmax or layer-norm denominator) is excluded and keeps its
// realize treatment; `{1, K}` is the only collapse shape whose store_root and
// consumer read are exercised here.
static int boundary_collapsed_unit_is_leading_only(u32 node_idx) {
  u32 ond = rangeify_unified_out_ndim_at(node_idx);
  if (ond < 2) return 0;
  Term r0 = rangeify_unified_out_rng_at(node_idx, 0);
  if (term_tag(r0) == TAG_UOP && term_ext(r0) == UOP_RANGE) return 0; // axis0 not collapsed
  for (u32 a = 1; a < ond; a++) {
    Term r = rangeify_unified_out_rng_at(node_idx, a);
    if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;  // a trailing collapse
  }
  return 1;
}

// Walk the consumer chain forward from producer_loc and return 1 iff it reaches
// a UOP_REDUCE (its loc in out_reduce_loc).  That reduce reads the producer's
// value in its body, so inlining the producer recomputes it per output element.
// Compare tinygrad's buffer_in_reduce (schedule/rangeify.py:87-96), which keeps
// a STAGE whose value holds a reduce that reads a buffer.
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
    // Recurse through every other consumer but a KERNEL, which is its own
    // realized boundary.
    if (cop == UOP_KERNEL) continue;
    if (boundary_feeds_reduce_consumer(cons[c], depth + 1, out_reduce_loc)) return 1;
  }
  return 0;
}

// 1 iff the consuming reduce is a 2-D conv weight gradient against a
// collapsed-batch (B == 1) activation: its body rank exceeds the producer's by
// 3 or more (the _pool unfold adds C_in, kH, kW) and still has a size-1 axis.
// That reduce reads overlapping windows of the activation, so inlining
// recomputes it at every window offset with the CONST(0) batch baked in and
// mis-reduces.  Every other collapsed-axis reduce must inline: a matmul adds
// one axis, attention at most two, a statistics reduce none, a 1x1 conv one.
// tinygrad also collapses a size-1 range to CONST(0)
// (schedule/indexing.py:18-21).
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
  boundary_buf_hash_clear();
  for (u32 i = 0; i < BOUNDARY_ORDER_CAP; i++) BOUNDARY_BUFFERIZE_TERM[i] = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++)
    BOUNDARY_DEPTH[i] = BOUNDARY_DEPTH_INVALID;
  boundary_depth_rec(term_val(root));
  boundary_compute_last_use();
  // BOUNDARY_ORDER follows the fire sequence, the dispatch order (see
  // BOUNDARY_FIRE_SEQ).
  boundary_compute_fire_seq(root);

  struct { u64 loc; u32 depth; u32 seq; Term buf; } items[BOUNDARY_ORDER_CAP];
  u32 n = 0;
  // One free-aid cache for the whole scan: free_aids does not depend on the
  // consumer, so a node shared by many candidates is computed once.
  StrandFreeCache strand_cache;
  strand_free_cache_open(&strand_cache);
  for (u32 i = 0; i < BUFFERIZE_NODES_LEN && n < BOUNDARY_ORDER_CAP; i++) {
    // Only a node the unified pass gave a UOP_BUFFERIZE can be a boundary; the
    // rest were inlined by the consumer-divergence walk or are movement nodes
    // forwarding to a producer's BUFFERIZE.
    Term buf = rangeify_unified_bufferize_at(i);
    if (buf == 0) continue;
    // A boundary is a node classify realized for a reason
    // ru_seed_boundary_holds keeps (agreeing with the rangeify realize-map
    // seed), or a rangeify-realized node that is effectively full and must be
    // read, since inlining it re-derives the producer (a conv backward's
    // relu(conv1)).  Any other rangeify-realized node inlines into its
    // consumer, and one that would strand a range is fatal.  Clean partial
    // realizes (softmax denominator, layer-norm mean broadcasts) keep their
    // fusion.
    int classify_real = BUFFERIZE_NODES[i].realized
                     && ru_seed_boundary_holds(BUFFERIZE_NODES[i].reasons);
    if (!classify_real) {
      u32 nr  = uop_bufferize_n_ranges(buf);
      u32 ond = rangeify_unified_out_ndim_at(i);
      // Effectively full: every axis closed (nr == ond), or a realize whose
      // collapsed unit axis puts nr below ond and that must still be read:
      //   0. an activation collapsed only on its leading axis (`{1, K}`)
      //      feeding a matmul reduce: the matmul reads a clean `{1, K}` buffer
      //      (BLAS) instead of recomputing the producer chain per output
      //      column.  A trailing keepdim unit axis stays fused (realizing it
      //      breaks attention's backward parity).
      //   1. a full realize a SHRINK with a nonzero begin reads (a head-split
      //      offset), which an inline would drop
      //      (boundary_has_offset_shrink_consumer).
      //   2. a collapsed-axis activation feeding a conv weight-gradient reduce
      //      (reduce_is_conv_weight_grad).
      // tinygrad keeps a STAGE whose value holds a reduce that reads a buffer
      // (buffer_in_reduce, schedule/rangeify.py:87-96).
      u64 rloc = 0, mmloc = 0;
      int unitmm = (nr > 0 && nr < ond
                    && boundary_collapsed_unit_is_leading_only(i)
                    && boundary_feeds_reduce_consumer(
                         BUFFERIZE_NODES[i].loc, 0, &mmloc)
                    && bufferize_uop_is_matmul(mmloc));
      int effectively_full = (ond > 0 && nr == ond)
                          || unitmm
                          || (rangeify_unified_realized_full_at(i)
                              && boundary_has_offset_shrink_consumer(
                                   BUFFERIZE_NODES[i].loc))
                          || (nr > 0 && nr < ond
                              && boundary_has_collapsed_unit_axis(i)
                              && boundary_feeds_reduce_consumer(
                                   BUFFERIZE_NODES[i].loc, 0, &rloc)
                              && reduce_is_conv_weight_grad(rloc, ond));
      if (!rangeify_unified_is_realized(i)) continue;
      if (!effectively_full) {
        if (bufferize_value_would_strand_c(buf, i, &strand_cache)) {
          thvm_fatal("boundary scan: inlining a partial realize would strand a range");
        }
        continue;
      }
    }
    items[n].loc   = BUFFERIZE_NODES[i].loc;
    items[n].depth = BOUNDARY_DEPTH[i];
    items[n].seq   = BOUNDARY_FIRE_SEQ[i];
    items[n].buf   = buf;
    n++;
  }
  strand_free_cache_close(&strand_cache);
  // Sort by fire sequence (execution order).  Boundaries the DFS did not reach
  // (orphan preserved tensors) keep 0xFFFFFFFF and sort last, by depth and loc.
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
    // Index the BUFFERIZE Term and its value for the per-kernel rewrite's
    // lookups.
    {
      Term bt  = items[i].buf;
      Term val = (bt != 0 && term_tag(bt) == TAG_UOP && term_ext(bt) == UOP_BUFFERIZE)
               ? uop_bufferize_value(bt) : 0;
      boundary_buf_hash_insert(bt, val, idx);
    }
  }
  // Last-reader positions for the arena planner, once BOUNDARY_ORDER and the
  // loc hash are final.
  boundary_compute_last_use_pos();
}

// Sort the boundaries, then record each one's unified-pass UOP_BUFFERIZE
// (RU_BUFFERIZE_TERM) in BOUNDARY_BUFFERIZE_TERM, the value
// topo_sort_boundaries already stored, for emit_kernel_for_boundary.
// BUFFERIZE_NODES.realized is left unchanged.
static void topo_sort_buffers_unified(Term root) {
  topo_sort_boundaries(root);
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    u32 idx = bufferize_info_find(BOUNDARY_ORDER[i]);
    BOUNDARY_BUFFERIZE_TERM[i] = (idx != 0xFFFFFFFFu)
                                    ? rangeify_unified_bufferize_at(idx)
                                    : 0;
  }
}

// The boundary order, for tests.
fn u32 materialize_boundary_count(void)         { return BOUNDARY_ORDER_LEN; }
fn u64 materialize_boundary_at(u32 i)           { return i < BOUNDARY_ORDER_LEN ? BOUNDARY_ORDER[i] : 0; }

// View-only movement ops.  RESHAPE, EXPAND, PERMUTE, SHRINK and FLIP rewrite a
// TenDesc's View instead of allocating a kernel; view_resolve walks a chain
// rooted at a TAG_TEN, one alias TenDesc per layer (tensor_view_of inherits
// buf_id, dtype, backend and producer_kid).  PAD has no view form: it would
// read outside the allocation.

// Collapse runs of stride-compatible axes of v into (merged_dim, stride,
// expand_real_dim) blocks: (s_a, st_a) and (s_b, st_b) merge iff
// `st_a == s_b * st_b`, acting as one axis of s_a * s_b at the inner stride.  A
// unit axis joins its neighbour; a stride-0 block has expand_real_dim 0, so the
// reshape fitter knows it spans no memory.  At most ndim blocks.
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

// Reshape into one, possibly non-contiguous, view when the new shape's axes
// split the source's merged blocks (view_merge_dims).  1 on success, 0 when no
// single view exists (the caller then chains views).
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

  // A contiguous source takes canonical strides.
  if (src->contiguous) {
    *out = view_create(ts);
    return 1;
  }

  // A non-contiguous source: express the new shape as strides through the
  // merged blocks, from the trailing axis inward.
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
    if (acc != merged[mi].merged_dim) return 0;   // mismatch: the caller falls back
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
  // Valid on a non-contiguous source too: only dims and strides reorder.
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
  // Valid on a non-contiguous source too: the offset grows by begin * stride
  // per axis and the strides stay.
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  i32 add_off = 0;
  u32 t_numel = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 b = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
    u32 e = (u32)term_val(heap_read(expr_loc + 3 + 2 * i));
    // A kvar-packed begin (KVAR_FLAG) is a runtime row offset: start_pos of a
    // KV-cache append (`cache_kv[:, :, start_pos:start_pos+seqlen]`, tinygrad's
    // examples/gpt2.py:35).  Resolve it (kvar_set_runtime) before the offset
    // arithmetic.  The caller packs the end as pack(begin) + T, so `e - b` on
    // the raw values is T; decoding the end would read another kvar, since + T
    // bumps the id field.
    if (kvar_extent_is_var(b)) {
      u32 t_len = kvar_extent_is_var(e) ? (e - b) : 0;  // raw delta = slice length
      b = kvar_extent_runtime(b);
      e = b + t_len;
    }
    // A literal begin keeps b and e raw: a `{0, pack(S)}` slice over a symbolic
    // axis (the per-head attention seq shrink) keeps its packed end, so `e - b`
    // stays the symbolic dim, which decoding the end would collapse to a
    // literal.  A sub-slice of a literal axis at a kvar length is not handled.
    if (e <= b || e > src->shape.dims[i]) return 0;
    ts.dims[i] = e - b;
    // numel at the static (hi) bound, as view_create, view_apply_reshape and
    // view_apply_expand size it: `e - b` of a `{0, pack(S)}` slice keeps
    // KVAR_FLAG, and the raw packed value would overflow the u32 product.
    // ts.dims[i] keeps the packed extent, so the symbolic axis survives.
    t_numel  *= kvar_extent_static(e - b);
    add_off  += (i32)b * src->strides[i];
  }
  out->shape  = ts;
  out->numel  = t_numel;
  out->offset = src->offset + add_off;
  for (u32 i = 0; i < src->shape.ndim; i++) out->strides[i] = src->strides[i];
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  // An identity SHRINK (every axis at full extent) keeps the source's
  // contiguous flag; a real one is marked non-contiguous.  A slice marked
  // contiguous would let materialize_root_alias skip the strided gather and
  // return the raw underlying buffer.
  out->contiguous = (t_numel == src->numel) ? src->contiguous : 0;
  return 1;
}

static int view_apply_flip(View const *src, u64 expr_loc, View *out) {
  // Valid on a non-contiguous source too: a flipped axis negates its stride and
  // adds (dim - 1) * stride to the offset.
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

// A UOP_CONST as a 1-element TenDesc holding its value, for a view_resolve
// chain that bottoms out at a CONST (an EXPAND lifting a scalar cotangent to
// the target's shape, from interact_grad's leaf rule).
static u32 const_to_tendesc(u64 const_loc) {
  Term num   = heap_read(const_loc);
  u32  dtype = term_ext(num);
  u32  bits  = (u32)term_val(num);
  Shape s = {0}; s.ndim = 1; s.dims[0] = 1;
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, dtype);
  // Write the scalar at its dtype's width.  F32 bits are already IEEE-754;
  // integer dtypes read bits as an i32 (sign-extended at the WL bridge) and
  // narrow it.
  u8  buf8;  u16 buf16;  u32 buf32;  u64 buf64;
  f32 fv;
  memcpy(&fv, &bits, sizeof fv);
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
    // tinygrad realizes a constant in a kernel, and a kernel's fp8 store
    // saturates (codegen/decomp/dtype.py:127-134), so an e5m2 inf payload
    // fills as maxnorm, not inf.
    case DT_FP8E4M3: buf8 = f32_to_fp8e4m3_sat(fv);             src = &buf8;  nbytes = 1; break;
    case DT_FP8E5M2: buf8 = f32_to_fp8e5m2_sat(fv);             src = &buf8;  nbytes = 1; break;
    case DT_FP16:
    case DT_BF16:
    case DT_FP64: {
      // Widen the f32 bits to the target float: lossy for f64 past f32
      // precision, precise enough for the 0.0, 1.0 and log(2) literals the grad
      // chain rule emits.
      static u8 buf_bytes[8];
      from_fp32_lane(buf_bytes, dtype, &fv, 1);
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
      // Other dtypes would need a 64-bit payload: zero-fill rather than write
      // past the buffer end.
      buf64 = 0; src = &buf64; nbytes = dtype_storage_bytes(dtype, 1);
      break;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src, nbytes);
  return tid;
}

static u32 boundary_index_for_loc(u64 loc);
static u8  op_is_view_movement(u8 op);

// 1 iff the movement chain under t has tinygrad's _pool im2col signature (a
// rank-reducing RESHAPE directly over an EXPAND) and bottoms out at an emitted
// boundary.  Only then does view_resolve_inner take the boundary as a base:
// view_apply_reshape cannot absorb the rank merge, the kernel-op path reads
// zeros for it, and tensor_view_chain_append plus the dispatch pre-mat need a
// buffer-backed source.
static int view_chain_needs_boundary_base(Term t) {
  int saw_pool_merge = 0;
  Term cur = term_resolve(t);
  for (u32 hops = 0; hops < 64; hops++) {
    if (term_tag(cur) != TAG_UOP) return 0;            // a TAG_TEN or other leaf: handled directly
    u8 op = term_ext(cur);
    if (op == UOP_KERNEL) return 0;                    // buffer-backed already
    if (!op_is_view_movement(op) && op != UOP_PAD) {
      // The chain's source, a non-movement UOp: it qualifies only as an emitted
      // boundary under a _pool merge.
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

// view_resolve walks a movement chain, allocating one alias TenDesc per layer,
// and returns the final tid, or 0 for the caller to emit a kernel.  The source
// must resolve to a TenDesc: a TAG_TEN, a UOP_KERNEL, a UOP_CONST (as a
// 1-element TenDesc) or a view chain rooted at one.  The backend must be
// view-aware.
static u32 view_resolve(Term t) {
  if (CURRENT_BACKEND == NULL || !CURRENT_BACKEND->view_aware) return 0;
  return view_resolve_inner(t, view_chain_needs_boundary_base(t));
}

static u32 view_resolve_inner(Term t, int boundary_base) {
  if (CURRENT_BACKEND == NULL || !CURRENT_BACKEND->view_aware) return 0;
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    // Packed nibble dtypes have no view path: materialize_root_alias's gather
    // moves whole 1, 2, 4 or 8-byte elements.  Declining sends the chain to the
    // kernel path, which lacks packed-nibble support on the CPU backend too
    // (docs/NN/dtypes.md).
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

  // A CONST source becomes a 1-element TenDesc: interact_grad's leaf cotangent
  // lift produces EXPAND(CONST(0|1)) to the target shape, and without this the
  // grad chain stalls as a UOp that never fires.
  if (op == UOP_CONST) return const_to_tendesc(loc);

  // Under boundary_base, an emitted boundary is the chain's base: the rest
  // composes as a view over its buffer, gathered once by the dispatch pre-mat.
  // Its TenDesc carries producer_kid, so kernel_fire_by_id fires the producer
  // first.  Only with boundary_base and a set BOUNDARY_TID: unconditionally,
  // pending boundaries would alias uninitialised buffers and plain
  // Reshape(Shrink) or Pad chains would become strided aliases kernel consumers
  // misread.
  if (boundary_base) {
    u32 bi = boundary_index_for_loc(loc);
    if (bi != 0xFFFFFFFFu && bi < BOUNDARY_ORDER_CAP && BOUNDARY_TID[bi] != 0) {
      u32 btid = BOUNDARY_TID[bi];
      if (btid != 0 && btid < TENS_NEXT && !dtype_is_packed(TENS[btid].dtype))
        return btid;
    }
  }

  // The source: another movement chain, a CONST, a leaf.
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
    // KV-cache append: a SHRINK whose leading begin is kvar-packed (start_pos)
    // is an in-place cache write's dst.  Stamp the kvar id on the alias, so the
    // JIT ASSIGN replay re-resolves the row at fire (view.offset holds the
    // capture step's).  Read-side `{0, pack(S)}` shrinks have a literal begin.
    if (op == UOP_SHRINK && alias != 0) {
      u32 b0 = (u32)term_val(heap_read(loc + 2));   // axis-0 begin
      if (kvar_extent_is_var(b0)) {
        TENS[alias].assign_kvar_id = kvar_extent_var_id(b0);
      }
    }
    return alias;
  }
  // No single view.  A malformed RESHAPE (a numel mismatch, ndim past MAX_DIM)
  // falls through; a valid one that is not one View (merging a stride-0 axis
  // with a strided one: the _pool im2col reshape), which the kernel-op path
  // cannot materialize over a non-contiguous source, appends a canonical outer
  // View to the source's chain (tensor_view_chain_append), gathered through
  // tendesc_strided_index by the dispatch pre-mat and materialize_root_alias.
  // PERMUTE, SHRINK and FLIP failures are malformed input.
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
  // A malformed reshape, a chain wraparound, a failed non-RESHAPE absorb: emit
  // a kernel op.
  return 0;
}

// True when a UOp opcode is one of the 5 view-only-path movement ops.
static u8 op_is_view_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_EXPAND || op == UOP_PERMUTE
      || op == UOP_SHRINK  || op == UOP_FLIP;
}

// Gather a non-contiguous TenDesc into a fresh contiguous copy
// (tendesc_strided_index, or the backend's gather_strided), for a movement root
// that resolves to a view alias the caller reads (wnf expects flat reads).
// record_replay records the gather for a JIT capture's replay.
static Term materialize_root_alias(Term t, int record_replay) {
  if (term_tag(t) != TAG_TEN) return t;
  u32 tid = (u32)term_val(t);
  if (tid == 0 || tid >= TENS_NEXT) return t;
  TenDesc *d = &TENS[tid];
  // Skip the gather only when everything is contiguous: the public view at
  // offset 0 and no view chain, which could map a contiguous outer view through
  // non-contiguous inner ones.
  if (d->view.contiguous && d->view.offset == 0 && d->nviews == 0) return t;

  u32 dst_tid = tensor_alloc(d->backend, d->view.shape, d->dtype);
  if (dst_tid == 0) return t;

  // On-device strided gather where the backend has gather_strided (Metal),
  // avoiding the host read, gather and write, which force a GPU-to-host sync
  // per realize.  It declines (-1) 8-byte elements and chains longer than
  // GATHER_MAX_VIEWS, which take the host loop; the replay tail runs either
  // way.
  int did_device_gather = 0;
  if (d->backend->gather_strided != NULL
      && d->backend->gather_strided(TENS[dst_tid].buf_id, d->buf_id, d) == 0) {
    did_device_gather = 1;
  }

  if (!did_device_gather) {
  // Read up to the largest element index reached: analytic from the strides
  // without a chain, a walk over every element with one.
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
  }
  // Under a TJit capture this host gather never enters the recorded dispatch
  // stream, so a replay would keep the capture step's bytes when the source is
  // recomputed (a re-firing reduce broadcast) or mutated (a KV cache an append
  // rewrites).  Record a replayable JIT_OP_GATHER into the dst buffer, so each
  // replay re-gathers the live source.
  if (record_replay) {
    jit_capture_record_gather(tid, TENS[dst_tid].buf_id);
  }
  return term_new(0, TAG_TEN, d->dtype, dst_tid);
}

// Kernel input bindings: visit() and its slot helpers.

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
  // When the unified pass folded this tid's prior_views into the INDEX_E addr
  // (RU_TID_CHAIN_COMPOSED), the backend must skip the pre-mat gather and read
  // the underlying buffer.  Slots that visit() or view_resolve mint for a chain
  // the unified pass already swizzled into the addr are flagged by
  // unified_fold_chain_scan.
  if (ke->input_chain_composed != NULL
      && rangeify_unified_tid_chain_composed(tid)) {
    ke->input_chain_composed[slot] = 1;
  }
  return slot;
}

// A symbolic input slot: a TVAR whose binding LAM has a shape annotation but no
// TEN yet (before APP-LAM).  tid 0 marks it for fire-time resolution:
// interact_kernel term_resolves a slot with tid 0 and a term.
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
  // No strides until fire time: a 1-D contiguous view of numel, so codegen
  // reads no garbage strides.
  Shape s = {0}; s.ndim = 1; s.dims[0] = numel;
  ke->input_views  [slot] = view_create(s);
  return slot;
}

static u32 visit_indexed_leaf(Term t, KernelEntry *ke, u64 root_loc,
                              VisitMemo *memo);
static u32 visit_operand_at_ranges(Term t, KernelEntry *ke, u64 root_loc,
                                   VisitMemo *memo);

// Record ke's input bindings for the subgraph under t, inlining every
// non-boundary op: VISIT_OK, an input-slot ref (KSRC_AS_INPUT), or VISIT_BAIL
// on an unsupported op, which fails the kernel.
static u32 visit(Term t, KernelEntry *ke, u64 root_loc, VisitMemo *memo) {
  // Resolve VAR (SUB-bit) and ALO chains, so a body after APP-LAM exposes its
  // bound argument instead of a bare VAR.  term_resolve forces ALO layers but
  // fires nothing.
  t = term_resolve(t);
  u8 tag = term_tag(t);

  // A grad projection that survives into visit is no UOp and bails
  // (term_resolve unwraps a grad DP0 to its value; a pending DP1 stays): the
  // realize loop then runs wnf, which fires it, and materializes again.
  // Materialize compiles kernels; it never fires interactions.

  if (tag == TAG_TEN) {
    u32 tid  = (u32)term_val(t);
    u32 slot = input_slot_dedup(ke, tid, t);
    if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
    return KSRC_AS_INPUT(slot);
  }
  // A shape-annotated TVAR (a TLamShape binding, its shape in the lam_shape
  // side table) is a symbolic input slot: at fire time interact_kernel resolves
  // input_terms[slot] through SUB to the APP-LAM argument (a loop iteration's
  // weight).  The body materializes once, and the kernel-program cache dedups
  // later identical emissions.
  if (tag == TAG_VAR) {
    Shape s;
    if (lam_shape_lookup(term_val(t), &s)) {
      u32 numel = 1;
      for (u32 i = 0; i < s.ndim; i++) numel *= s.dims[i];
      u32 slot = input_slot_dedup_var(ke, t, DT_FP32, numel);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      return KSRC_AS_INPUT(slot);
    }
    return VISIT_BAIL;          // no shape annotation: cannot compile
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
    u32 ref = KSRC_AS_INPUT(slot);
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  // A boundary other than this kernel's root becomes an input.  Boundaries emit
  // in topo order, so an upstream one's BOUNDARY_TID is set; 0 means it was not
  // emitted, and the kernel bails.
  if (loc != root_loc) {
    u32 bi = boundary_index_for_loc(loc);
    if (bi != 0xFFFFFFFFu) {
      u32 tid = BOUNDARY_TID[bi];
      Term boundary_term = BOUNDARY_TERM[bi];
      if (tid == 0) return VISIT_BAIL;
      u32 slot = input_slot_dedup(ke, tid, boundary_term);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
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

  // GATHER: src is read at the lowered address, which rangeify composed
  // against its storage; idx is bound as rangeify reads it.
  if (op == UOP_GATHER) {
    memo->indexed = 1;
    u32 ii = visit_operand_at_ranges(heap_read(loc + 1), ke, root_loc, memo);
    if (ii == VISIT_BAIL) {
      return VISIT_BAIL;
    }
    u32 si = visit_indexed_leaf(heap_read(loc + 0), ke, root_loc, memo);
    if (si == VISIT_BAIL) {
      return VISIT_BAIL;
    }
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  // SCATTER (always its own kernel root): idx and src are read at lowered
  // addresses composed against their storage; base is bound as rangeify reads
  // it.
  if (op == UOP_SCATTER) {
    memo->indexed = 1;
    u32 bi = visit_operand_at_ranges(heap_read(loc + 0), ke, root_loc, memo);
    if (bi == VISIT_BAIL) {
      return VISIT_BAIL;
    }
    for (u32 s = 1; s <= 2; s++) {
      if (visit_indexed_leaf(heap_read(loc + s), ke, root_loc, memo) == VISIT_BAIL) {
        return VISIT_BAIL;
      }
    }
    visit_memo_store(memo, loc, VISIT_OK);
    return VISIT_OK;
  }

  // A movement op: a view-only alias when the chain resolves to a TenDesc;
  // otherwise (an EXPAND over a MUL from interact_grad) visit its source.
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

  // REDUCE, as the kernel root or as an intermediate whose result later ops
  // read broadcast.
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
      // A keepdim REDUCE over only size-1 axes (output shape equal to the
      // input's) is the identity on data: visit its source.  As a reduce, its
      // extent-1 axes would be CONST(0) with no RANGE in the body, which the
      // walker cannot loop over.
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

// Input slot for an indexed operand: the storage under its views (a TEN, a
// VAR, a KERNEL's output or a realized boundary), never a view alias.  The
// lowered address is composed against that storage (ru_index_leaf); an
// alias slot would need the chain-fold mark to keep its own offset and
// pre-materialization off the read.
static u32 visit_indexed_leaf(Term t, KernelEntry *ke, u64 root_loc,
                              VisitMemo *memo) {
  for (;;) {
    t = term_resolve(t);
    if (term_tag(t) != TAG_UOP || !op_is_view_movement((u8)term_ext(t))) {
      break;
    }
    u64 loc = term_val(t);
    if (loc != root_loc && boundary_index_for_loc(loc) != 0xFFFFFFFFu) {
      break;
    }
    t = heap_read(loc);
  }
  return visit(t, ke, root_loc, memo);
}

// The operand an indexed op reads at the node's own ranges (GATHER idx,
// SCATTER base), bound as rangeify reads it (ru_read_operand_at_ranges):
// a structural one as its storage, a computed one as any fused operand.
static u32 visit_operand_at_ranges(Term t, KernelEntry *ke, u64 root_loc,
                                   VisitMemo *memo) {
  if (uop_indexed_operand_is_structural(t)) {
    return visit_indexed_leaf(t, ke, root_loc, memo);
  }
  return visit(t, ke, root_loc, memo);
}

// Does the UOp subtree at loc read TAG_TEN(tid)?  visited makes a shared node
// walked once; stops at UOP_KERNEL (a prior boundary) and TAG_TEN leaves.
static int boundary_refs_tid_rec(u64 loc, u32 tid, u8 *visited, u64 cap) {
  if (loc >= cap) return 0;
  if (visited[loc]) return 0;
  visited[loc] = 1;
  // The node's op (and thus arity) comes from the classifier, not a tag at
  // heap[loc]; walk its slots via that arity.
  u32 binfo = bufferize_info_find(loc);
  if (binfo == 0xFFFFFFFFu) return 0;
  u8 op = BUFFERIZE_NODES[binfo].op;
  if (op == UOP_KERNEL) return 0;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    Term child = term_resolve(heap_read(loc + i));
    u8 ct = term_tag(child);
    if (ct == TAG_TEN) {
      if (term_val(child) == tid) return 1;
      continue;
    }
    if (ct != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    if (boundary_refs_tid_rec(term_val(child), tid, visited, cap)) return 1;
  }
  return 0;
}

// Free shared buffers in their last consuming sub-pass, after the topo sort and
// before the emit loop.  For each cached shared tid whose last root is this
// sub-pass (MATERIALIZE_CURRENT_ROOT + 1 == last_root): if this pass reads it,
// record it to be freed after the deepest BOUNDARY_DEPTH that reads it; if not,
// only earlier siblings did, so free it now.  Either way it goes right after
// its last cross-root reader, tinygrad's last_appearance
// (schedule/memory.py:32, 44).
static void xpass_shared_input_plan(void) {
  if (XPASS_SHARED_TID_LEN == 0)   return;
  if (BOUNDARY_ORDER_LEN == 0)     return;
  if (CURRENT_BACKEND == NULL)     return;
  u64 cap = HEAP_NEXT > 0 ? HEAP_NEXT : 1;
  u8 *visited = (u8 *)calloc(cap, 1);
  if (visited == NULL)             return;
  for (u32 s = 0; s < XPASS_SHARED_TID_LEN; s++) {
    XPassSharedTid *st = &XPASS_SHARED_TIDS[s];
    if (st->last_root != MATERIALIZE_CURRENT_ROOT + 1) continue;
    if (st->tid == 0) continue;
    u32 best_depth = 0;
    int referenced = 0;
    for (u32 bi = 0; bi < BOUNDARY_ORDER_LEN; bi++) {
      u64 bloc = BOUNDARY_ORDER[bi];
      u32 binfo = bufferize_info_find(bloc);
      if (binfo == 0xFFFFFFFFu) continue;
      memset(visited, 0, cap);
      if (!boundary_refs_tid_rec(bloc, st->tid, visited, cap)) continue;
      referenced = 1;
      u32 d = BOUNDARY_DEPTH[binfo];
      if (d != BOUNDARY_DEPTH_INVALID && d > best_depth) best_depth = d;
    }
    if (referenced) {
      // Freed after its last reader here.  A depth-0 reader (a sink) would
      // never be pushed (mem_plan_push_dead's sink guard), so that buffer is
      // left to the pool rollback.
      if (best_depth > 0)
        mem_plan_record_shared_free(TENS[st->tid].buf_id, best_depth, CURRENT_BACKEND);
    } else {
      // No reader in this last sub-pass or after it: free it now.
      mem_plan_push_shared_dead(TENS[st->tid].buf_id, CURRENT_BACKEND);
    }
  }
  free(visited);
}

// Emit the kernel for the boundary at BOUNDARY_ORDER index bi.  Returns its
// UOP_KERNEL term, a TAG_TEN when a cache or an alias already holds the output,
// or 0 on bail.
static Term emit_kernel_for_boundary(u32 bi) {
  u64 boundary_loc = BOUNDARY_ORDER[bi];
  u32 idx = bufferize_info_find(boundary_loc);
  if (idx == 0xFFFFFFFFu) return 0;

  // A boundary already aliased to an earlier kernel's output (a degenerate
  // movement chain onto the same TenDesc) returns that alias.
  if (BOUNDARY_TID[bi] != 0 && BOUNDARY_TERM[bi] != 0) {
    return BOUNDARY_TERM[bi];
  }

  // A boundary an earlier materialize emitted (an earlier fixpoint iteration,
  // or an earlier CTR child sharing the loc) reuses that buffer, so every
  // consumer reads one copy: the maxpool input that the forward window MAX and
  // the backward argmax mask read must be one buffer, or the copies disagree at
  // a tie.  Only inside a realize; a rolled-back tid misses.  tinygrad rewrites
  // a realized UOp to its buffer (tensor.py:204-208).
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

  // Freelist planner: push every earlier output whose last reader has emitted,
  // so tensor_alloc below pops a same-size buffer instead of growing the table.
  u32 this_depth = BOUNDARY_DEPTH[idx];
  mem_plan_push_dead(this_depth);

  // The arena slot arena_compute assigned (memory_plan_rewrite's suballocation,
  // schedule/memory.py:47-52), else a fresh tensor_alloc.  The output lives on
  // the device its UOp names (term_device_in): the device is in the graph, as
  // in tinygrad, and dispatch follows TENS[output_tid].backend, so allocating
  // there runs the kernel there.  The arena is keyed to CURRENT_BACKEND, so a
  // kernel on another device skips it.
  i32      op_dev     = term_device_in(root_term);
  Backend *op_backend = (op_dev < 0) ? CURRENT_BACKEND : ctx_ensure_backend(op_dev);
  if (op_backend == NULL) op_backend = CURRENT_BACKEND;
  // Resident weight-movement hit: a TRealize@Transpose of a resident weight
  // that a warmup or an earlier replay already realized and pinned is reused as
  // a pre-capture leaf instead of re-emitting the permute as a recorded
  // dispatch.  A TenDesc whose buffer is gone (buf_id 0, the trim) misses.
  u32 wm_src = 0;
  if (tc_weight_pin_enabled() && uop_is_movement(op)) {
    wm_src = weight_move_chase_upload(root_term);
    if (wm_src != 0) {
      u64 wm_k    = weight_move_key(wm_src, &out_shape, out_dtype);
      u32 wm_tid  = weight_move_lookup(wm_k, op_backend->id);
      // No refcount guard, unlike materialized_loc_lookup: the cached buffer is
      // jit-pinned, so it is alive even at refcount 0 (the warmup's matmul
      // consumers released it), and the next consumer increfs it.  The
      // COPY_UPLOAD_CACHE hit trusts the pin the same way.
      if (wm_tid != 0 && wm_tid < TENS_NEXT && TENS[wm_tid].buf_id != 0) {
        if (wm_trace_enabled()) {
          fprintf(stderr, "[wm] HIT  src=%u tid=%u buf=%u\n",
                  wm_src, wm_tid, TENS[wm_tid].buf_id);
        }
        Term ten = term_new(0, TAG_TEN, TENS[wm_tid].dtype, wm_tid);
        BOUNDARY_TID [bi] = wm_tid;
        BOUNDARY_TERM[bi] = ten;
        return ten;
      }
    }
  }
  u32 out_tid = (op_backend == CURRENT_BACKEND)
                  ? arena_tensor_alloc(bi, out_shape, out_dtype) : 0;
  // Under a capture, adopt a dead same-size captured intermediate that
  // mem_plan_push_dead parked (the jit pin refused its freelist push) instead
  // of growing the pinned set.  Outside a capture the pop finds nothing.
  if (out_tid == 0) {
    u32 rbid = jit_caprecycle_pop(
        op_backend, dtype_storage_bytes(out_dtype, (u64)shape_numel(out_shape)));
    if (rbid != 0) {
      out_tid = tensor_alloc_adopt(op_backend, out_shape, out_dtype, rbid);
    }
  }
  if (out_tid == 0) {
    out_tid = tensor_alloc(op_backend, out_shape, out_dtype);
  }

  u32 kid     = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->output_tid    = out_tid;
  ke->output_dtype  = out_dtype;
  ke->output_shape  = out_shape;
  ke->output_numel  = TENS[out_tid].view.numel;
  ke->source_uop    = root_term;
  // The UOP_BUFFERIZE the unified pass emitted for this boundary (tests inspect
  // it).
  ke->compute_bufferize = BOUNDARY_BUFFERIZE_TERM[bi];
  TENS[out_tid].producer_kid = kid;

  VisitMemo memo = {0};
  u32 result;
  result = visit(root_term, ke, boundary_loc, &memo);
  u8 indexed = memo.indexed;
  visit_memo_free(&memo);
  if (result == VISIT_BAIL) {
    kernel_dealloc_last(kid);
    TENS[out_tid].producer_kid = 0;
    return 0;
  }
  // A gather or scatter kernel is lowered for, and verified on, the serial
  // CPU backends only: a GPU renderer would spread its loops over threads
  // (racing a scatter's read-modify-write).  Decline loudly; the realize
  // leaves the graph unrealized.
  if (indexed && op_backend != &CPU_BACKEND) {
    fprintf(stderr, "thvm: gather/scatter kernels run on the CPU backend "
                    "only; the kernel for a %s boundary was not emitted\n",
            bypass_dbg_op_name(op));
    kernel_dealloc_last(kid);
    TENS[out_tid].producer_kid = 0;
    return 0;
  }

  // visit() consumed the whole subgraph as one input slot: the boundary is a
  // movement chain view_resolve aliased, with nothing to compute.  An empty
  // kernel would leave the output zeroed (losing the gy = CONST(1.0) seed of an
  // MSE backward), so the boundary aliases the input tid instead.
  if (KSRC_IS_INPUT(result)) {
    u32 alias_tid = ke->input_tids[KSRC_INDEX(result)];
    if (alias_tid != 0 && alias_tid < TENS_NEXT) {
      // Except a realized movement boundary whose alias is not contiguous (a
      // transpose of a computed source): it was realized for consumers whose
      // swizzles diverge (Newton-Schulz `Gw` vs `Gw.T`), which read it
      // row-major over its output shape, as tinygrad indexes a contiguous STAGE
      // (schedule/indexing.py:92-93).  Aliasing the strided source would drop
      // the transpose, so the copy kernel is emitted.
      int alias_contig = TENS[alias_tid].view.contiguous
                      && TENS[alias_tid].view.offset == 0
                      && TENS[alias_tid].nviews == 0;
      int boundary_is_movement = uop_is_movement(op);
      if (!(boundary_is_movement && !alias_contig)) {
        // Release the speculatively allocated output.
        tensor_release(out_tid);
        kernel_dealloc_last(kid);
        Term alias_term = term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
        BOUNDARY_TID [bi] = alias_tid;
        BOUNDARY_TERM[bi] = alias_term;
        return alias_term;
      }
    }
  }

  ke->schedule = &ke->_local_schedule;

  // Cache kernel_lift_to_uop's KernelUopLift on the entry.  When the lift
  // declines, cached_lift.store_root stays 0 and each backend falls back to its
  // unlifted path (cpu_blas_dispatch by pattern; cpu_jit_dispatch and
  // cpu_uop_walk decline; Metal returns -1).  The dispatch-time readers
  // (cpu_jit_build, cg_emit_via_uop, cpu_uop_walk) use cached_lift without
  // re-running the lifter.
  if (kernel_lift_to_uop(ke, &ke->cached_lift)) {
    // store_root is the unified pass's root here (the lift returns 0 without
    // one).  First the arange collapse, tinygrad's pm_reduce_simplify
    // (codegen/simplify.py:147), which runs right after the rangeify walk
    // (schedule/rangeify.py:356-358): a single-axis SUM REDUCE over a pure
    // RANGE (a triangular-mask arange, cumsum, one-hot) folds to its closed
    // form on the raw RANGE arithmetic.  Each pass below runs on every store of
    // a multi-store (AFTER) root.
    ke->cached_lift.store_root =
        uop_reduce_arange_collapse(ke->cached_lift.store_root);
    Term ru_rewritten = unified_store_root_for_walker(ke, ke->cached_lift.store_root);
    // Again on the walker's root, with the gated-load rule: the walker inlines
    // removable BUFFERIZEs, whose values (a one-hot over an arange) the pass
    // above saw only as buffer reads.
    ru_rewritten = uop_reduce_load_collapse(ru_rewritten);
    // Flag the slots whose tid has a non-trivial layout (a chain, a
    // non-contiguous view) that INDEX_E reads; the marks commit below.
    ChainFoldMarks _cf_marks;
    ru_rewritten = unified_fold_chain(ke, ru_rewritten, &_cf_marks);
    ke->cached_lift.store_root = ru_rewritten;
    // A gather / scatter address holds loads, so an unbound leaf read as 0
    // would misplace data, not just misvalue it.  Stop rather than run it.
    if (indexed && indexed_kernel_has_unbound_leaf(ru_rewritten)) {
      thvm_fatal("emit: a gather/scatter kernel kept an unbound TEN, "
                 "BUFFERIZE or indexed op after the walker rewrite");
    }
    // The marks describe this store_root's loads, so they commit with it: an
    // unflagged slot whose load is already composed over the raw buffer would
    // get its view gathered again (or, in the walker, its pointer offset),
    // applying the movement chain twice.
    unified_fold_chain_commit_flags(ke, &_cf_marks);
  }

  // Renumber axis ids densely from 0 per kernel (the lifter's ids are global
  // and sparse): after every materialize-time rewrite and before the init
  // snapshot and the fire-time hand_opts, so their axis_id + 1 splits stay
  // contiguous.  A multi-store root or a data-dependent address turns the opt
  // passes off (KernelEntry.no_opts).
  if (ke->cached_lift.store_root != 0) {
    Term root = uop_sym(ke->cached_lift.store_root);
    // tinygrad merges ranges before its opts; CPU only until a GPU run checks
    // the Metal and CUDA renderers on merged axes.
    if (CURRENT_BACKEND == &CPU_BACKEND) root = uop_simplify_ranges(root);
    root = uop_dag_renumber_axes(root);
    ke->cached_lift.store_root = root;
    ke->no_opts = (term_tag(root) == TAG_UOP && term_ext(root) == UOP_AFTER)
               || uop_dag_has_dynamic_index(root);
  }

  // Trim the dispatch input count to the rendered signature: the walker rewrite
  // can collapse visit()'s slots (view clones of one buffer fold to one
  // UOP_BUFFER instance) or leave a fused-away producer's slot unread, and the
  // renderer emits parameters only for the instances in store_root, so extra
  // slots would bind buffers the function never reads.  tinygrad derives a
  // kernel's buffers from its final sink (ProgramInfo.from_sink,
  // uop/ops.py:1361-1378).  Instances are slot + 1 of this store_root, so only
  // trailing slots go.
  if (ke->cached_lift.store_root != 0 && ke->n_inputs > 0) {
    u32 rendered = cg_render_input_inst_count(ke->cached_lift.store_root);
    if (rendered < ke->n_inputs) {
      ke->n_inputs = rendered;
      ke->cached_lift.n_inputs = rendered;
    }
  }

  // Snapshot cached_lift before runtime opts, so axes_reset_to_default can undo
  // kernel_apply_opt's DAG changes while autotune benches each variant.
  ke->cached_lift_init_root = ke->cached_lift.store_root;

  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, out_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, DT_INT32, kid));
  Term kernel_term = term_new(0, TAG_UOP, UOP_KERNEL, kloc);

  // Pin a heap cell holding the UOP_KERNEL term, so a whole-heap walk
  // (THeapDiagram[]) sees every emitted kernel, not only the sink the WL handle
  // reaches; the others live only in BOUNDARY_TERM.  One read-only cell per
  // kernel.
  u64 pin = heap_alloc(1);
  heap_set(pin, kernel_term);

  BOUNDARY_TID [bi] = out_tid;
  BOUNDARY_TERM[bi] = kernel_term;

  // An fp8-cast transient: record its bf16 tid so the end of the realize frees
  // it (its only reader is the matmul reduce that fires this realize), before
  // mark_heap_rooted_preserve keeps it through the kloc+0 cell.
  {
    u32 tbinfo = bufferize_info_find(boundary_loc);
    if (tbinfo != 0xFFFFFFFFu
        && (BUFFERIZE_NODES[tbinfo].reasons & BUFFERIZE_REASON_FP8_TRANSIENT)
        && out_tid != 0 && FP8_TRANSIENT_LEN < FP8_TRANSIENT_CAP) {
      FP8_TRANSIENT_TIDS [FP8_TRANSIENT_LEN] = out_tid;
      FP8_TRANSIENT_KLOCS[FP8_TRANSIENT_LEN] = kloc;
      FP8_TRANSIENT_PINS [FP8_TRANSIENT_LEN] = pin;
      FP8_TRANSIENT_LEN++;
    }
  }

  // Record the output for freelist recycling, except with BOUNDARY_LAST_USE 0
  // (the realize root or a preserved orphan, which the caller reads
  // afterwards), a multi-consumer output (aliased through DUP/SUP, or read
  // outside the tracked DAG by an optimizer's UOP_ASSIGN) or a held root.  A
  // cross-pass shared boundary looks single-consumer here while a sibling reads
  // its cached tid: record it with MEM_PLAN_LAST_USE_NEVER and hand its tid to
  // the consuming sub-pass (xpass_shared_input_plan), as tinygrad closes a
  // lifetime after its last appearance in the whole schedule
  // (schedule/memory.py:28-32, 44).
  if (BOUNDARY_LAST_USE[idx] > 0
      && BUFFERIZE_NODES[idx].consumer_count == 1
      && !xpass_is_held(boundary_loc)) {
    if (xpass_is_shared(boundary_loc)) {
      mem_plan_record(TENS[out_tid].buf_id, MEM_PLAN_LAST_USE_NEVER,
                      CURRENT_BACKEND);
      xpass_shared_tid_add(out_tid, xpass_last_root(boundary_loc), boundary_loc);
    } else {
      mem_plan_record(TENS[out_tid].buf_id, BOUNDARY_LAST_USE[idx],
                      CURRENT_BACKEND);
    }
  }

  // Pin and cache a realized movement whose source is a resident weight upload
  // (wm_src, set only under THVM_TC_TRANSPOSE when the chase proves a pure
  // movement or cast of an upload), so a capture reads it as a stable leaf (see
  // WEIGHT_MOVE_CACHE).  An activation movement (Transpose[attn @ Wv]) fails
  // the chase at its compute UOp and stays planner-managed.
  if (wm_src != 0 && op_backend->buf_jit_pin != NULL) {
    {
      // An arena view output (owns_data 0) would not protect the arena, which
      // the rollback frees at the end of the realize, so the pinned view would
      // dangle on the next realize's hit.  The kernel has not fired and its
      // fire re-reads TENS[out_tid].buf_id, so swap in a fresh owned buffer for
      // the pin.
      u32 wm_bid  = TENS[out_tid].buf_id;
      u32 wm_root = (op_backend->buf_storage_root != NULL)
                      ? op_backend->buf_storage_root(wm_bid) : wm_bid;
      int wm_ok = 1;
      if (wm_root != wm_bid) {
        u64 wm_nbytes = dtype_storage_bytes(out_dtype,
                                            (u64)shape_numel(out_shape));
        u32 nb = op_backend->buf_alloc(wm_nbytes);
        if (nb != 0) {
          if (op_backend->buf_decref != NULL) op_backend->buf_decref(wm_bid);
          TENS[out_tid].buf_id = nb;
        } else {
          wm_ok = 0;                       // OOM: stay planner-managed
        }
      }
      if (wm_ok) {
        op_backend->buf_jit_pin(TENS[out_tid].buf_id);
        // Hold a TenDesc reference: the buffer is pinned, but once the warmup's
        // matmul consumer releases the tid, its refcount-0 slot could go to
        // another tensor under the cache.  Uploads are WL-referenced weights; a
        // realized transpose is an anonymous intermediate, so the cache holds
        // its ref.
        TENS[out_tid].refcount++;
        // Record the source upload for weight_move_trim_sources: with the
        // durable key every re-derivation of this transpose hits, even inside a
        // capture, so no recorded dispatch binds the upload and trimming it is
        // safe.  Never for a pin made during a capture: that first derivation
        // is a recorded dispatch whose replay reads the upload.
        weight_move_insert(weight_move_key(wm_src, &out_shape, out_dtype),
                           op_backend->id, out_tid,
                           jit_is_capturing() ? 0 : wm_src);
        if (wm_trace_enabled()) {
          fprintf(stderr,
              "[wm] PIN  src=%u tid=%u buf=%u swap=%d nbytes=%llu ndim=%u d0=%u d1=%u cap=%d\n",
              wm_src, out_tid, TENS[out_tid].buf_id,
              (wm_root != wm_bid),
              (unsigned long long)dtype_storage_bytes(out_dtype, (u64)shape_numel(out_shape)),
              out_shape.ndim, out_shape.ndim > 0 ? out_shape.dims[0] : 0,
              out_shape.ndim > 1 ? out_shape.dims[1] : 0,
              jit_is_capturing());
        }
      }
    }
  }

  // Cache loc -> tid for later sub-passes (MATERIALIZED_LOC_TABLE).
  materialized_loc_insert(boundary_loc, out_tid);

  return kernel_term;
}

// Walk a UOp DAG for UOP_ASSIGN nodes at any depth and materialize each one's
// src in place (heap_set on cell + 1), so the surrounding emission sees a
// kernel chain producing a TEN, not a raw UOp graph.  Stops at non-UOp tags and
// at UOP_KERNEL.
static void materialize_inner_assigns_rec(Term term, u8 *visited, u64 cap) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  u8 ar = uop_arity((u8)op);
  if (ar == 0) return;
  u64 loc = term_val(term);
  // Each node once: walking a shared node once per incoming edge is exponential
  // in depth, and materializing an ASSIGN src twice only repeats work.
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

// Replace every UOP_COPY child with what materialize_copy returns (usually a
// TAG_TEN), before bufferize_classify, so the classifier and the emitter see a
// plain external input wherever a weight was wrapped in a COPY.  tinygrad's
// COPY likewise becomes a buffer the kernel reads (engine/realize.py
// exec_copy).
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

// Strip UOP_DETACH(x) -> x in place across a UOp DAG, chains included, so the
// stop-gradient marker never reaches kernel lifting, the renderer or the
// walker: it is the identity on values, and the backward was built before
// materialize.
static Term materialize_strip_detach_rec(Term term, u8 *visited, u64 cap) {
  for (int hops = 0; hops < 64 && term_tag(term) == TAG_UOP
                     && term_ext(term) == UOP_DETACH; hops++) {
    term = heap_read(term_val(term) + 0);
  }
  if (term_tag(term) != TAG_UOP) return term;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return term;
  u64 loc = term_val(term);
  // Each node once, as in materialize_subst_cached_rec: walking a shared node
  // once per incoming edge is exponential in depth, and its children are
  // already stripped.
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
  // A REF to an atom def (a TEN, a NUM, another REF) stands for it.  Any other
  // def is a book node, compiled when wnf unfolds it: its cells index
  // BOOK_HEAP, and a book template is never rewritten in place.  The cap stops
  // a cycle of REF aliases.
  for (int hops = 0; hops < 8; hops++) {
    Term resolved = term_resolve(term);
    if (term_tag(resolved) == TAG_REF) {
      u32 name = term_ext(resolved);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0 || term_cells(book, BOOK_HEAP) != 0) {
        term = resolved;
        break;
      }
      term = book;
      continue;
    }
    if (resolved == term) break;
    term = resolved;
  }

  // TAG_CTR (a multi-target grad bundle): materialize each child as its own
  // sub-pass and rebuild the CTR.  The children are UOp graphs or TAG_TENs wnf
  // already reduced.
  if (term_tag(term) == TAG_CTR) {
    u32   n        = term_ctr_n(term);
    Term *children = (Term *)malloc((n ? n : 1) * sizeof(Term));
    if (children == NULL) thvm_fatal("materialize: out of memory for a bundle's roots");
    u32 saved_root = MATERIALIZE_CURRENT_ROOT;
    for (u32 i = 0; i < n; i++) {
      MATERIALIZE_CURRENT_ROOT = i;
      children[i] = thvm_materialize(term_ctr_at(term, i));
    }
    MATERIALIZE_CURRENT_ROOT = saved_root;
    Term bundle = term_new_ctr(term_ext(term), children, n);
    free(children);
    return bundle;
  }
  // Compound IC nodes (APP, LAM, SUP, DUP, OP2, MAT, ALO): materialize the
  // children in place (heap_set on the original cell), so one TMaterialize of a
  // whole recursive training term (TPri[loss, ASSIGN(...)], a TLam loop
  // wrapper) compiles every embedded UOp graph while wnf still drives the
  // structure.  Atoms are leaves.
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

  // A UOp an earlier sub-pass, or a realize in the same JIT span, already
  // emitted returns its output as a TAG_TEN (see MATERIALIZED_LOC_TABLE).  Only
  // inside a realize: tests that call thvm_materialize directly take the
  // unsubstituted path.
  if (materialized_loc_scope_depth() > 0) {
    u32 cached_tid = materialized_loc_lookup(term_val(term));
    if (cached_tid != 0) {
      return term_new(0, TAG_TEN, TENS[cached_tid].dtype, cached_tid);
    }
    // Substitute cached UOp descendants with TAG_TEN leaves first, so
    // bufferize_classify sees an external tensor wherever an earlier pass
    // produced one instead of re-emitting its kernel.
    materialize_subst_cached_inplace(term);
  }
  // DETACH is the identity at runtime and the backward is already built: unwrap
  // it.
  if (term_ext(term) == UOP_DETACH) {
    return thvm_materialize(heap_read(term_val(term) + 0));
  }
  // UOP_COPY: a lazy device transfer (materialize_copy).
  if (term_ext(term) == UOP_COPY) {
    return materialize_copy(term);
  }
  // ASSIGN is fired by wnf (interact_assign), not a kernel: materialize its src
  // in place (heap[loc + 1]), so wnf fires the src kernels, a TEN lands there
  // and interact_assign copies it into the dst buffer.
  if (term_ext(term) == UOP_ASSIGN) {
    u64  loc        = term_val(term);
    // A KV-cache append's dst is a shrunk view of a persistent cache, which
    // interact_assign never reduces: resolve it to a TAG_TEN alias sharing the
    // cache buffer at the SHRINK's offset, where interact_assign then writes.
    // An in-place weight update's dst is already a TAG_TEN.
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
  // Materialize the src of every nested ASSIGN in place.  An ASSIGN inside an
  // outer ASSIGN's src, or deep in a compound UOp (Adam's
  // `w - lr * mAfter / denom`), would otherwise keep a raw UOp src that wnf
  // cannot reduce to a TEN, and never fire.
  materialize_inner_assigns(term);
  // Resolve the UOP_COPY children before the boundary classifier runs
  // (materialize_inner_copies).
  materialize_inner_copies(term);

  if (term_ext(term) == UOP_CONST) {
    u32 tid = const_to_tendesc(term_val(term));
    return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
  }

  // A movement-op root: resolve it to an alias TenDesc and gather it into a
  // contiguous copy for wnf's flat reads.
  if (op_is_view_movement(term_ext(term))) {
    u32 alias_tid = view_resolve(term);
    // A non-plain alias (strided, offset or chained) of a kernel output is not
    // gathered here: the producer fires later in wnf, so the gather would copy
    // unwritten bytes.  It takes the kernel path instead, as does any non-plain
    // alias on the CPU, a compiled copy like tinygrad's where the host gather
    // decodes each element's index.  Elsewhere an external source
    // (producer_kid 0) gathers safely; a plain contiguous alias needs no gather.
    if (alias_tid != 0) {
      TenDesc const *ad = &TENS[alias_tid];
      int plain = ad->view.contiguous && ad->view.offset == 0 && ad->nviews == 0;
      int kernel_path = !plain && (ad->producer_kid != 0
                                   || CURRENT_BACKEND == &CPU_BACKEND);
      if (!kernel_path) {
        Term alias_term = term_new(0, TAG_TEN, ad->dtype, alias_tid);
        // Under a capture, record the gather as a replayable JIT_OP_GATHER, so
        // it re-runs off the live source each step instead of baking the
        // capture step's bytes (a re-firing reduce broadcast, a KV cache an
        // append mutates).
        return materialize_root_alias(alias_term, jit_is_capturing());
      }
    }
  }

  split_reduceop(term);
  bufferize_classify(term);
  topo_sort_buffers_unified(term);
  mem_plan_reset();
  // Arena offsets for the plannable boundaries (tinygrad runs
  // memory_plan_rewrite on the linear schedule before it executes,
  // schedule/__init__.py:304).
  arena_compute();
  // In the last consuming sub-pass of a CTR bundle, schedule each shared input
  // (cached by the producing sub-pass and recorded there with
  // MEM_PLAN_LAST_USE_NEVER) to be freed after its last reader here.
  xpass_shared_input_plan();
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    BOUNDARY_TID [i] = 0;
    BOUNDARY_TERM[i] = 0;
  }
  // All-or-nothing emission: if any boundary fails, rewind KERNELS_NEXT and
  // TENS_NEXT and return the input unchanged.  Otherwise every realize-loop
  // iteration would re-emit the boundaries that succeeded, and KERNELS_NEXT
  // would grow with the iteration count.
  u32 kernels_at_start = KERNELS_NEXT;
  u32 tens_at_start    = TENS_NEXT;
  Term sink_kernel = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    Term k = emit_kernel_for_boundary(i);
    if (k == 0) {
      // Free the per-kernel arrays of the kernels this attempt emitted.
      for (u32 r = kernels_at_start; r < KERNELS_NEXT; r++)
        kernel_free_arrays(&KERNELS[r]);
      KERNELS_NEXT = kernels_at_start;
      TENS_NEXT    = tens_at_start;
      // The rewind invalidates the weight-move entries this attempt minted: the
      // retry re-issues their tids to other tensors.
      weight_move_purge_from(tens_at_start);
      mem_plan_drain_freelist();
      // Drop the arena allocation's +1 here too (see the end of the pass).  The
      // rewound view TenDescs never decref their arena parent, so the arena
      // waits for the pool rollback, a transient leak per failed pass;
      // arena_reset clears the static pointers for the next pass.
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
        else if (CURRENT_BACKEND == &METAL_BACKEND) {
          thvm_metal_buf_arena_release(ARENA_BUF_ID);
        }
      }
      arena_reset();
      return term;
    }
    if (BOUNDARY_ORDER[i] == term_val(term)) sink_kernel = k;
  }
  // End of pass: pull the planner's leftover pushes back off the free-lists, so
  // the next materialize of the realize loop cannot recycle a buffer whose tid
  // the chain rule's fresh UOps still reference.
  mem_plan_drain_freelist();
  // Drop the +1 the arena's allocation holds: each live view holds one ref, so
  // the arena is freed when its last view is released instead of leaking until
  // shutdown.
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
    else if (CURRENT_BACKEND == &METAL_BACKEND) {
      // Drop the ref thvm_metal_buf_arena_alloc gave the arena.  skip_freelist
      // keeps it off the recycle list, so it is freed whole once its last view
      // (each of which increfs it) is released.
      thvm_metal_buf_arena_release(ARENA_BUF_ID);
    }
  }
  ARENA_DATA   = NULL;
  ARENA_BUF_ID = 0;
#ifdef THVM_HAS_CUDA
  ARENA_DPTR   = 0;
#endif
  return sink_kernel != 0 ? sink_kernel : term;
}
