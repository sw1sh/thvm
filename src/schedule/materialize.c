// schedule/materialize.c - tinygrad-style scheduler.
//
// g2a: bufferize_classify + topo_sort_boundaries populate
// BOUNDARY_ORDER (kernel emit order).
// g2b: build_kernel emits one KernelEntry per boundary by visiting
//      its UOp subgraph and inlining every non-boundary upstream
//      elementwise / CONST / LOAD / REDUCE-as-tail op into the
//      kernel's program[].  Movement ops (g2c) and GRAD (g2d) are
//      not yet supported -- visit() returns 0xDEADBEEF on those,
//      which makes the boundary's emit bail and thvm_materialize
//      fall back to returning the input unchanged.

#define BOUNDARY_ORDER_CAP 16384
static u64  BOUNDARY_ORDER[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_TID  [BOUNDARY_ORDER_CAP];   // emitted output TenDesc id
static Term BOUNDARY_TERM [BOUNDARY_ORDER_CAP];   // emitted UOP_KERNEL term
// Per-BOUNDARY_ORDER slot, the UOP_BUFFERIZE Term the unified
// rangeify pass emitted at this boundary (0 when the boundary has
// no unified-pass record).  Mirror: tinygrad/schedule/indexing.py:77
// lands a UOp(Ops.BUFFERIZE, ...) per realize boundary; the
// scheduler downstream walks those.  thvm projects the term onto a
// per-boundary slot so emit_kernel_for_boundary can read it without
// re-doing the bufferize_info_find lookup.
static Term BOUNDARY_BUFFERIZE_TERM[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_ORDER_LEN = 0;
// Parallel UOP_BUFFERIZE Term per slot in BOUNDARY_ORDER, populated
// by topo_sort_boundaries when the unified pass produced a non-zero
// RU_BUFFERIZE_TERM for that boundary. emit_kernel_for_boundary
// stashes this onto ke->compute_bufferize after kernel_alloc.
// 0 = "no UOP_BUFFERIZE term for this boundary" (realized boundary
// the unified pass didn't surface).
static Term BOUNDARY_BUFFERIZE_TERM[BOUNDARY_ORDER_CAP];

// Multi-output kernel merge planning (Step 2 of multi-output groundwork).
// After topo_sort_boundaries fills BOUNDARY_ORDER, plan_kernel_merges
// scans for pairs (A, B) where:
//   - same output shape (same iter rank+dims),
//   - both pure-elementwise (no UOP_REDUCE, by BUFFERIZE_NODES walk),
//   - no data-flow dependency between them,
//   - input set overlap (shared parents, after dedup),
//   - merged fingerprint stays within tile-feasibility op budget.
// When the merge is enabled (env THVM_KERNEL_MERGE=1, default OFF),
// emit_kernel_for_boundary walks each candidate B with merge_into_idx[B] = A
// and emits B's program ops as part of A's KernelEntry, then assigns
// B's output as a kernel_entry_set_extra_output(A) entry.  When the
// merge is OFF (default), the planning still runs and counts candidates
// for diagnostic visibility -- BOUNDARY_MERGE_INTO stays at sentinel
// 0xFFFFFFFFu and emit_kernel_for_boundary follows the legacy single-
// output path for every boundary.
//
// 0xFFFFFFFFu  = "this boundary is its own host (no merge)" or "no plan".
// other index  = "this boundary should be merged into BOUNDARY_ORDER[m]".
#define BOUNDARY_MERGE_NONE 0xFFFFFFFFu
static u32  BOUNDARY_MERGE_INTO  [BOUNDARY_ORDER_CAP];
static u32  KERNEL_MERGE_CANDIDATES = 0;   // count of pairs flagged by the
                                            // last plan_kernel_merges run

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

#define BOUNDARY_DEPTH_INVALID 0xFFFFFFFFu
static u32 BOUNDARY_DEPTH    [BUFFERIZE_NODES_CAP];
// Per-boundary maximum-consumer depth.  Filled after topo by
// boundary_compute_last_use; consumed by the depth-aware mem planner
// to recycle output bufs once their LAST consumer has emitted.  0 =
// "no consumer is itself a realize boundary"; that includes the
// realize root (the caller reads it; never recycle) and any orphan
// preserved tensor.
static u32 BOUNDARY_LAST_USE [BUFFERIZE_NODES_CAP];

#define VISIT_BAIL 0xDEADBEEFu

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

// CPU planner: default-off opt-in via THVM_REUSE_BUFS=1.  The chain-
// rule + Phase-3 fusion-relaxation cases on the CPU interpreter want
// DUP/SUP-aware lifetime tracking that's a follow-up; until then the
// CPU path stays gated.
static int mem_plan_cpu_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_REUSE_BUFS");
    enabled       = (e && e[0] == '1');
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

// Per-backend gate the recorded buf actually obeys.
static int mem_plan_backend_enabled(Backend const *b) {
  if (b == NULL) return 0;
  if (b->id == 1) return mem_plan_cpu_enabled();
  if (b->id == 2) return mem_plan_metal_enabled();
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
// emit loop INLINES them into the parent's program (visit() in
// emit_kernel_for_boundary recurses through them as KProgOp slots);
// the boundary that the program eventually reads is the realized
// kid, so its true last_use_depth is the realized PARENT'S depth,
// not the non-realized intermediate's "depth" (which equals the
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

// THVM_LIFT_FROM_UNIFIED=1 helper.  The unified-pass store_root carries
// TAG_TEN leaves (wrapped inside UOP_INDEX_E.buffer slots; see
// ru_rewrite_subtree in rangeify_unified.c).  The legacy kernel_lift
// path replaces those tensor handles with hash-consed UOP_BUFFER nodes
// keyed by (slot+1) instance disambiguator, and cpu_uop_walk binds the
// kernel's runtime input table to UOP_BUFFER leaves matched by that
// same instance number.  Substitute every TAG_TEN whose tid appears in
// ke->input_tids[] with the matching UOP_BUFFER so the unified subtree
// becomes structurally compatible with the walker's identity binding.
// Mirror source: kernel_lift.c:1540-1558 lift_input_buffer.

#define UNIFIED_REWRITE_MEMO_CAP 4096
typedef struct {
  Term key;
  Term value;
} UnifiedRewriteMemoSlot;

typedef struct {
  KernelEntry const     *ke;
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
// predates the legacy visit()'s view_resolve aliasing (e.g. matmul's
// inner W reference, before EXPAND folded into an alias tid), the
// initial exact-tid scan misses. tensor_view_of clones share buf_id,
// so a second pass matches by underlying buffer -- the consumer reads
// the same bytes regardless of which alias-tid the input slot tracks.
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
  return 0;
}

static Term unified_rewrite_rec(UnifiedRewriteState *st, Term t, u32 depth) {
  if (depth > 256) return t;
  Term resolved = term_resolve(t);
  u8 tag = term_tag(resolved);
  if (tag == TAG_TEN) {
    Term repl = unified_rewrite_buffer_for_tid(st->ke, (u32)term_val(resolved));
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
    Term repl = unified_rewrite_buffer_for_bufferize(st->ke, resolved);
    if (repl != 0) return repl;
  }

  // INDEX_E(BUFFERIZE(CONST(v)), addr) -> CONST(v).  Some unified-pass
  // BUFFERIZE wraps a scalar producer (e.g. the `mean_count` reciprocal
  // baked into a 1-elem buffer) whose value tree is a single UOP_CONST.
  // The legacy kernel_lift_to_uop inlines such constant-broadcast
  // producers into their consumers; the cpu_uop_walk value dispatcher
  // otherwise has no BUFFERIZE handler at the value layer and stalls,
  // yielding zeros.  Detect the pattern here so the consumer subtree
  // sees a plain CONST in place of the INDEX_E wrap.
  if (term_ext(resolved) == UOP_INDEX_E) {
    Term inner_buf = term_resolve(heap_read(term_val(resolved) + 0));
    if (term_tag(inner_buf) == TAG_UOP
        && term_ext(inner_buf) == UOP_BUFFERIZE
        && unified_rewrite_buffer_for_bufferize(st->ke, inner_buf) == 0) {
      Term v = uop_bufferize_value(inner_buf);
      if (v != 0 && term_tag(v) == TAG_UOP && term_ext(v) == UOP_CONST) {
        unified_rewrite_memo_insert(st, resolved, v);
        return v;
      }
    }
  }

  Term hit = 0;
  if (unified_rewrite_memo_lookup(st, resolved, &hit)) return hit;

  u8  op  = term_ext(resolved);
  u64 loc = term_val(resolved);

  u8 ar = uop_arity(op);
  if (ar == 0) {
    unified_rewrite_memo_insert(st, resolved, resolved);
    return resolved;
  }
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = unified_rewrite_rec(st, old_child, depth + 1);
    srcs[i] = new_child;
    if (new_child != old_child) changed = 1;
  }
  Term out = changed ? uop_graph_rebuild_with_srcs(resolved, srcs) : resolved;
  unified_rewrite_memo_insert(st, resolved, out);
  return out;
}

static Term unified_store_root_for_walker(KernelEntry const *ke, Term root) {
  if (root == 0 || ke == NULL) return root;
  UnifiedRewriteState st;
  memset(&st, 0, sizeof(st));
  st.ke = ke;
  return unified_rewrite_rec(&st, root, 0);
}

// Walk a (post-rewrite) UOp subtree and return 1 if any UOP_BUFFERIZE
// survives.  Used as the unified-bypass safety gate: cpu_uop_walk's
// INDEX_E handler only resolves UOP_BUFFER leaves, so a residual
// BUFFERIZE means the kernel cannot execute via the bypass.  Uses a
// small visited stack keyed on Term identity to keep the walk O(N)
// over the hash-consed DAG without re-scanning shared subtrees.
#define BUFFERIZE_SCAN_VISITED_CAP 1024
typedef struct {
  Term  keys[BUFFERIZE_SCAN_VISITED_CAP];
  u32   n;
} BufferizeScanVisited;

static int bufferize_scan_seen(BufferizeScanVisited *v, Term t) {
  for (u32 i = 0; i < v->n; i++) if (v->keys[i] == t) return 1;
  if (v->n < BUFFERIZE_SCAN_VISITED_CAP) v->keys[v->n++] = t;
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
  BufferizeScanVisited v;
  v.n = 0;
  return bufferize_scan_rec(&v, root, 0);
}

// Safety gate for THVM_LIFT_FROM_UNIFIED=1: collect every UOP_RANGE
// axis_id that appears in a STORE's addr or as the axis of a UOP_REDUCE
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
  if (op == UOP_KERNEL || op == UOP_BUFFER) return;
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
    return range_axis_has(iter_axes, aid) ? 0 : 1;
  }
  if (op == UOP_KERNEL || op == UOP_BUFFER) return 0;
  if (op == UOP_REDUCE) {
    // Enter the reduce's axis into the iterated set for the body walk.
    u32 r_aid = (u32)term_val(heap_read(loc + 2));
    RangeAxisSet inner = *iter_axes;
    range_axis_add(&inner, r_aid);
    BufferizeScanVisited iv;
    iv.n = 0;
    return stranded_range_check_value(&inner, &iv, heap_read(loc + 0),
                                      depth + 1);
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (stranded_range_check_value(iter_axes, v, heap_read(loc + i),
                                   depth + 1)) return 1;
  }
  return 0;
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
  RangeAxisSet iter_axes;
  iter_axes.n = 0;
  BufferizeScanVisited v;
  v.n = 0;
  stranded_range_collect_addr(&iter_axes, &v, s_addr, 0);
  BufferizeScanVisited vv;
  vv.n = 0;
  return stranded_range_check_value(&iter_axes, &vv, s_value, 0);
}

// Safety gate: scan the rewritten subtree for any UOP_INDEX_E reading
// from a UOP_BUFFER input slot whose static numel is smaller than the
// consumer's iter footprint (output STORE numel + any enclosing REDUCE
// extents).  This indicates a stride-0 broadcast view: the legacy
// kernel_lift consults ke->input_views[slot].strides and emits CONST(0)
// for the broadcast axis; the unified bypass builds addr expressions from
// per-axis ranges without consulting strides and reads out-of-bounds on
// the 1-element backing store.  Until ru_pass threads view strides
// through INDEX_E address construction, decline the bypass for these
// inputs.
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
  // INDEX_E(BUFFER, addr): check that BUFFER numel matches the
  // consumer's iter footprint (out_numel for the elementwise case;
  // reduce-axes are handled by descending under the REDUCE handler
  // below).  When BUFFER numel < footprint AND the input view has a
  // matching stride-0 pattern, the legacy lift would emit CONST(0)
  // for the broadcast axis; the bypass instead reads at the consumer
  // iter and goes out-of-bounds.  Decline.
  if (op == UOP_INDEX_E) {
    Term buf_t = term_resolve(heap_read(loc + 0));
    if (term_tag(buf_t) == TAG_UOP && term_ext(buf_t) == UOP_BUFFER) {
      u32 inst = uop_buffer_inst_get(buf_t);
      // Output buf (inst=0) is the writer's own slot; skip the size check.
      if (inst >= 1) {
        u64 buf_numel = uop_buffer_numel(buf_t);
        if (buf_numel > 0 && out_numel > 0 && buf_numel < out_numel) {
          return 1;
        }
      }
    }
  }
  if (op == UOP_KERNEL || op == UOP_BUFFERIZE) return 0;
  // Multiply REDUCE extent into out_numel when descending into a reduce
  // body so the broadcast check counts the full iteration footprint.
  if (op == UOP_REDUCE) {
    u64 inner_out = out_numel;
    Term body = heap_read(loc + 0);
    u32 r_aid = (u32)term_val(heap_read(loc + 2));
    // Find the reduce range's extent by scanning the body for a
    // UOP_RANGE with this axis_id.
    u32 r_ext = 0;
    BufferizeScanVisited iv;
    iv.n = 0;
    // small inline scan
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
  BufferizeScanVisited v;
  v.n = 0;
  return broadcast_input_scan_rec(ke, &v, s_value, out_numel, 0);
}

// === Debug dumper for THVM_DEBUG_BYPASS_LAST=1 ===
// Pretty-prints a UOp subtree with indent + opcode name + key fields.
// Used to bisect bypass divergences against the legacy lifter root.
static char const *bypass_dbg_op_name(u8 op) {
  switch (op) {
    case UOP_CONST:       return "CONST";
    case UOP_RESHAPE:     return "RESHAPE";
    case UOP_PERMUTE:     return "PERMUTE";
    case UOP_EXPAND:      return "EXPAND";
    case UOP_PAD:         return "PAD";
    case UOP_SHRINK:      return "SHRINK";
    case UOP_FLIP:        return "FLIP";
    case UOP_CMPLT:       return "CMPLT";
    case UOP_REDUCE:      return "REDUCE";
    case UOP_LOAD:        return "LOAD";
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
    default:              return "?";
  }
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
  if (op == UOP_REDUCE) {
    u32 kind = (u32)term_val(heap_read(loc + 1));
    u32 axis = (u32)term_val(heap_read(loc + 2));
    fprintf(stderr, "REDUCE kind=%u axis=%u\n", kind, axis);
    bypass_dbg_dump_rec(heap_read(loc + 0), indent + 2, depth + 1);
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

static void topo_sort_boundaries(Term root) {
  BOUNDARY_ORDER_LEN = 0;
  boundary_hash_clear();
  for (u32 i = 0; i < BOUNDARY_ORDER_CAP; i++) BOUNDARY_BUFFERIZE_TERM[i] = 0;
  for (u32 i = 0; i < BUFFERIZE_NODES_CAP; i++)
    BOUNDARY_DEPTH[i] = BOUNDARY_DEPTH_INVALID;
  boundary_depth_rec(term_val(root));
  boundary_compute_last_use();

  struct { u64 loc; u32 depth; Term buf; } items[BOUNDARY_ORDER_CAP];
  u32 n = 0;
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
    if (!BUFFERIZE_NODES[i].realized) continue;
    items[n].loc   = BUFFERIZE_NODES[i].loc;
    items[n].depth = BOUNDARY_DEPTH[i];
    items[n].buf   = buf;
    n++;
  }
  for (u32 i = 1; i < n; i++) {
    for (u32 j = i; j > 0; j--) {
      u8 swap = (items[j].depth <  items[j-1].depth)
            || (items[j].depth == items[j-1].depth && items[j].loc < items[j-1].loc);
      if (!swap) break;
      u64 lt = items[j].loc;   items[j].loc   = items[j-1].loc;   items[j-1].loc   = lt;
      u32 dt = items[j].depth; items[j].depth = items[j-1].depth; items[j-1].depth = dt;
      Term bt = items[j].buf;  items[j].buf   = items[j-1].buf;   items[j-1].buf   = bt;
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
      }
    }
  }
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
// (tinygrad/engine/realize.py); thvm reuses the legacy topo and
// attaches the unified-pass Term per slot for KernelEntry wiring.
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

// === Step 2: kernel-merging plan ====================================
// Walks BOUNDARY_ORDER pairwise looking for boundaries that could be
// fused into one multi-output kernel.  See the BOUNDARY_MERGE_INTO
// comment block above for the predicate semantics and the env-flag
// gating.
//
// Predicates (kept narrow on first landing):
//   - boundary's UOP is elementwise (uop_is_unary_elementwise OR
//     uop_is_binary_elementwise).  Movement-op roots and REDUCE
//     roots are excluded -- merging across reductions or shape-
//     reinterpret is harder and lands later.
//   - both boundaries' iter shapes match exactly.  We use Shape
//     inferred via term_shape_in.
//   - inputs of A and B overlap by at least one TenDesc (otherwise
//     fusing them just spreads cache pressure).  Identical input
//     sets is the strict subset; we currently require non-empty
//     intersection only.
//   - no data-flow dependency: B's UOP subgraph (via heap walk of
//     children) does not reach A's loc, and vice versa.
//
// The plan stops at one merge per host (1->1 fusion, no chains),
// which keeps the surface area of the codegen change in step 3
// bounded.  If a host A merges with B, the same A cannot merge
// with C in the same pass; A's slot 1 is filled, slot 2 stays open
// for a future relaxation.
//
// Programs are NOT mutated here -- only metadata.  The actual merge
// (program splice + kernel_entry_set_extra_output) runs in
// emit_kernel_for_boundary when THVM_KERNEL_MERGE=1.
static int kernel_merge_enabled(void) {
  char const *e = getenv("THVM_KERNEL_MERGE");
  return e != NULL && e[0] != '0';
}
static int kernel_merge_dump_enabled(void) {
  char const *e = getenv("DUMP_KERNEL_MERGE");
  return e != NULL && e[0] != '0';
}

// Pure-elementwise predicate at boundary granularity.  Reads the
// boundary's UOP opcode from BUFFERIZE_NODES and returns 1 only for
// unary/binary elementwise roots.  Movement / REDUCE / KERNEL roots
// return 0.  CONST/LOAD also return 0 (those are leaves, not fusable
// hosts in the merge sense).
static u8 merge_boundary_is_elementwise(u64 loc) {
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u8 op = BUFFERIZE_NODES[idx].op;
  return uop_is_unary_elementwise(op) || uop_is_binary_elementwise(op);
}

// Quick `from` -> `target` reachability test.  Walks the UOP DAG
// under heap[from + 0..arity-1] looking for `target_loc`.  Stops at
// realized boundaries other than `from` itself (those are kernel
// inputs, so anything past them is consumed via TenDesc, not via
// re-traversal).  Used to detect data-flow dependency between two
// boundary candidates.
//
// Returns 1 iff `from`'s subtree reaches `target_loc`.
static u8 merge_subtree_reaches_loc(u64 from_loc, u64 target_loc,
                                    u8 *visited) {
  if (from_loc == target_loc) return 1;
  if (from_loc >= HEAP_NEXT) return 0;
  if (visited[from_loc]) return 0;
  visited[from_loc] = 1;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return 0;
  // Stop at any realized boundary that isn't the starting one;
  // its children are reached via the kernel's input slots, not
  // directly via heap traversal.  (We pass from_loc as the start
  // marker by not setting a boundary check on the very first call.)
  // The realized check kicks in for descendants below.
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u64 cloc = term_val(child);
    u32 cidx = bufferize_info_find(cloc);
    if (cidx != 0xFFFFFFFFu && BUFFERIZE_NODES[cidx].realized
        && cloc != target_loc) {
      // Boundary -- stop walking; if it's the target, we'd have
      // matched the early return above.
      continue;
    }
    if (merge_subtree_reaches_loc(cloc, target_loc, visited)) return 1;
  }
  return 0;
}

// Collect the set of TenDesc-input ids reachable from a boundary's
// subtree.  Stops at realized boundaries (their output TenDescs are
// what fed into the kernel as inputs).  Returns count.
//
// `out_tids` is sized to `cap`; overflow returns cap (we treat that
// as "too many inputs, reject the merge for safety").
static u32 merge_collect_input_tids(u64 from_loc, u32 *out_tids, u32 cap,
                                    u8 *visited) {
  if (from_loc >= HEAP_NEXT) return 0;
  if (visited[from_loc]) return 0;
  visited[from_loc] = 1;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  u32 n = 0;
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    u8 ctag = term_tag(child);
    if (ctag == TAG_TEN) {
      u32 tid = (u32)term_val(child);
      // Linear dedup -- input sets are tiny (typically 2-8 entries).
      u8 seen = 0;
      for (u32 i = 0; i < n; i++) if (out_tids[i] == tid) { seen = 1; break; }
      if (!seen) {
        if (n >= cap) return cap;
        out_tids[n++] = tid;
      }
      continue;
    }
    if (ctag != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) {
      Term outbuf = heap_read(term_val(child));
      if (term_tag(outbuf) == TAG_TEN) {
        u32 tid = (u32)term_val(outbuf);
        u8 seen = 0;
        for (u32 i = 0; i < n; i++) if (out_tids[i] == tid) { seen = 1; break; }
        if (!seen) {
          if (n >= cap) return cap;
          out_tids[n++] = tid;
        }
      }
      continue;
    }
    u64 cloc = term_val(child);
    u32 cidx = bufferize_info_find(cloc);
    if (cidx != 0xFFFFFFFFu && BUFFERIZE_NODES[cidx].realized) {
      // Realized boundary: it produces a TenDesc the consumer kernel
      // reads.  We can't pull that tid out of BUFFERIZE_NODES yet (the
      // boundary may not have emitted), so we record the loc as a
      // pseudo-tid using a high bit.  For the merge predicate we
      // just need a stable identifier; we hash loc into the upper
      // 31 bits.
      u32 pseudo = 0x80000000u | (u32)(cloc & 0x7FFFFFFFu);
      u8 seen = 0;
      for (u32 i = 0; i < n; i++) if (out_tids[i] == pseudo) { seen = 1; break; }
      if (!seen) {
        if (n >= cap) return cap;
        out_tids[n++] = pseudo;
      }
      continue;
    }
    // Recurse into non-realized intermediate.
    u32 sub = merge_collect_input_tids(cloc, out_tids + n, cap - n, visited);
    n += sub;
    if (n >= cap) return cap;
  }
  return n;
}

// Number of input ids in `a` that also appear in `b`.
static u32 merge_input_overlap(u32 const *a, u32 na,
                               u32 const *b, u32 nb) {
  u32 hit = 0;
  for (u32 i = 0; i < na; i++) {
    for (u32 j = 0; j < nb; j++) {
      if (a[i] == b[j]) { hit++; break; }
    }
  }
  return hit;
}

// Get the boundary's output Shape via term_shape_in on the UOP_<op>
// term we'd construct from BUFFERIZE_NODES.  Returns 1 on success.
static int merge_boundary_shape(u64 loc, Shape *out) {
  u32 idx = bufferize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  Term t = term_new(0, TAG_UOP, BUFFERIZE_NODES[idx].op, loc);
  return term_shape_in(t, 0, out);
}

static int merge_shapes_equal(Shape const *a, Shape const *b) {
  if (a->ndim != b->ndim) return 0;
  for (u32 i = 0; i < a->ndim; i++) {
    if (a->dims[i] != b->dims[i]) return 0;
  }
  return 1;
}

// Mirrors THVM_BUFFERIZE_REMOVE_SCORE_CONSUMER_OPS_BUDGET as the
// initial cap; merging fewer-but-larger kernels is fine but only
// up to the tile path's op budget.
#define KERNEL_MERGE_OP_BUDGET 64

// Consumer-op count proxy: count UOP children reachable from a
// boundary's subtree (stopping at realized boundaries).  Cheap
// pre-emit estimate of how many KProgOp's the visit() pass would
// emit; merged total must stay below KERNEL_MERGE_OP_BUDGET to
// keep the kernel tile-feasible.
static u32 merge_op_count_estimate(u64 from_loc, u8 *visited) {
  if (from_loc >= HEAP_NEXT) return 0;
  if (visited[from_loc]) return 0;
  visited[from_loc] = 1;
  u32 idx = bufferize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return 0;
  u8 ar = uop_arity(BUFFERIZE_NODES[idx].op);
  u32 n = 1;     // self
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_KERNEL) continue;
    u64 cloc = term_val(child);
    u32 cidx = bufferize_info_find(cloc);
    if (cidx != 0xFFFFFFFFu && BUFFERIZE_NODES[cidx].realized) continue;
    n += merge_op_count_estimate(cloc, visited);
  }
  return n;
}

static void plan_kernel_merges(void) {
  KERNEL_MERGE_CANDIDATES = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_CAP; i++) {
    BOUNDARY_MERGE_INTO[i] = BOUNDARY_MERGE_NONE;
  }
  if (BOUNDARY_ORDER_LEN < 2) return;
  if (HEAP_NEXT == 0) return;

  // Reusable scratch buffers (sized once, reused across pairs).
  u8 *visited = (u8 *)calloc(HEAP_NEXT, 1);
  if (visited == NULL) return;

  for (u32 a = 0; a < BOUNDARY_ORDER_LEN; a++) {
    u64 a_loc = BOUNDARY_ORDER[a];
    if (!merge_boundary_is_elementwise(a_loc)) continue;
    Shape a_shape = {0};
    if (!merge_boundary_shape(a_loc, &a_shape)) continue;
    if (a_shape.ndim == 0) continue;

    // Already a host with a child -- skip (1->1 fusion only).
    u8 a_already_host = 0;
    for (u32 j = 0; j < a; j++) {
      if (BOUNDARY_MERGE_INTO[j] == a) { a_already_host = 1; break; }
    }
    if (a_already_host) continue;

    // Skip boundaries already merged into someone else.
    if (BOUNDARY_MERGE_INTO[a] != BOUNDARY_MERGE_NONE) continue;

    u32 a_inputs[16];
    memset(visited, 0, HEAP_NEXT);
    u32 na = merge_collect_input_tids(a_loc, a_inputs, 16, visited);
    if (na == 0 || na >= 16) continue;

    memset(visited, 0, HEAP_NEXT);
    u32 a_ops = merge_op_count_estimate(a_loc, visited);
    if (a_ops == 0 || a_ops > KERNEL_MERGE_OP_BUDGET) continue;

    for (u32 b = a + 1; b < BOUNDARY_ORDER_LEN; b++) {
      if (BOUNDARY_MERGE_INTO[b] != BOUNDARY_MERGE_NONE) continue;
      u64 b_loc = BOUNDARY_ORDER[b];
      if (!merge_boundary_is_elementwise(b_loc)) continue;
      Shape b_shape = {0};
      if (!merge_boundary_shape(b_loc, &b_shape)) continue;
      if (!merge_shapes_equal(&a_shape, &b_shape)) continue;

      // Data-flow dependency check, both directions.
      memset(visited, 0, HEAP_NEXT);
      if (merge_subtree_reaches_loc(b_loc, a_loc, visited)) continue;
      memset(visited, 0, HEAP_NEXT);
      if (merge_subtree_reaches_loc(a_loc, b_loc, visited)) continue;

      u32 b_inputs[16];
      memset(visited, 0, HEAP_NEXT);
      u32 nb = merge_collect_input_tids(b_loc, b_inputs, 16, visited);
      if (nb == 0 || nb >= 16) continue;
      if (merge_input_overlap(a_inputs, na, b_inputs, nb) == 0) continue;

      memset(visited, 0, HEAP_NEXT);
      u32 b_ops = merge_op_count_estimate(b_loc, visited);
      if (b_ops == 0) continue;
      if (a_ops + b_ops > KERNEL_MERGE_OP_BUDGET) continue;

      // Mark and stop at one merge per host.
      BOUNDARY_MERGE_INTO[b] = a;
      KERNEL_MERGE_CANDIDATES++;

      if (kernel_merge_dump_enabled()) {
        fprintf(stderr,
                "thvm: kernel-merge candidate: A[%u]@loc=%llu (ops=%u) "
                "<- B[%u]@loc=%llu (ops=%u) shape.ndim=%u\n",
                a, (unsigned long long)a_loc, a_ops,
                b, (unsigned long long)b_loc, b_ops,
                a_shape.ndim);
      }
      break;
    }
  }
  free(visited);
}

// Public accessor: number of (host, child) pairs the last plan
// flagged.  Used by tests (and `DUMP_KERNEL_MERGE=1` diagnostics).
fn u32 materialize_kernel_merge_candidate_count(void) {
  return KERNEL_MERGE_CANDIDATES;
}

// Public accessor: BOUNDARY_MERGE_INTO[bi].  Returns BOUNDARY_MERGE_NONE
// (0xFFFFFFFFu) when bi is a host or has no plan, otherwise the host's
// boundary index.
fn u32 materialize_kernel_merge_into(u32 bi) {
  if (bi >= BOUNDARY_ORDER_LEN) return BOUNDARY_MERGE_NONE;
  return BOUNDARY_MERGE_INTO[bi];
}

fn u32 materialize_boundary_count(void)         { return BOUNDARY_ORDER_LEN; }
fn u64 materialize_boundary_at(u32 i)           { return i < BOUNDARY_ORDER_LEN ? BOUNDARY_ORDER[i] : 0; }

// Multi-output kernel accessors.  Output index 0 reads the legacy
// single-output fields; 1..n_extra_outputs from extras arrays.
fn u32 kernel_entry_output_count(u32 kid) {
  if (kid >= KERNELS_NEXT) return 0;
  return 1u + (u32)KERNELS[kid].n_extra_outputs;
}

fn u32 kernel_entry_output_tid_at(u32 kid, u32 idx) {
  if (kid >= KERNELS_NEXT) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (idx == 0) return ke->output_tid;
  u32 ei = idx - 1;
  if (ei >= ke->n_extra_outputs) return 0;
  return ke->extra_output_tids[ei];
}

fn u32 kernel_entry_output_dtype_at(u32 kid, u32 idx) {
  if (kid >= KERNELS_NEXT) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (idx == 0) return ke->output_dtype;
  u32 ei = idx - 1;
  if (ei >= ke->n_extra_outputs) return 0;
  return ke->extra_output_dtypes[ei];
}

fn u32 kernel_entry_output_numel_at(u32 kid, u32 idx) {
  if (kid >= KERNELS_NEXT) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (idx == 0) return ke->output_numel;
  u32 ei = idx - 1;
  if (ei >= ke->n_extra_outputs) return 0;
  return ke->extra_output_numels[ei];
}

fn int kernel_entry_output_shape_at(u32 kid, u32 idx, Shape *out) {
  if (kid >= KERNELS_NEXT || out == NULL) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (idx == 0) { *out = ke->output_shape; return 1; }
  u32 ei = idx - 1;
  if (ei >= ke->n_extra_outputs) return 0;
  *out = ke->extra_output_shapes[ei];
  return 1;
}

fn int kernel_entry_set_extra_output(u32 kid, u32 idx,
                                     u32 tid, u32 dtype,
                                     Shape const *shape, u32 numel) {
  if (kid >= KERNELS_NEXT || shape == NULL) return 0;
  if (idx == 0) return 0;     // slot 0 is the legacy output_tid path
  u32 ei = idx - 1;
  if (ei >= KERNEL_MAX_EXTRA_OUTPUTS) return 0;
  KernelEntry *ke = &KERNELS[kid];
  ke->extra_output_tids   [ei] = tid;
  ke->extra_output_dtypes [ei] = dtype;
  ke->extra_output_shapes [ei] = *shape;
  ke->extra_output_numels [ei] = numel;
  if (ei + 1 > (u32)ke->n_extra_outputs) {
    ke->n_extra_outputs = (u8)(ei + 1);
  }
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

// Forward decl: kprog_op_is_identity sits below
// op_is_chain_movement in this file.  Both helpers are file-static
// so the accessor can reference them ahead of their definitions.
static u8 kprog_op_is_identity(KProgOp const *p);

fn int kernel_entry_prog_chain_op(u32 kid, u32 prog_idx, BIndexChainOp *out) {
  if (kid >= KERNELS_NEXT) return 0;
  KernelEntry const *ke = &KERNELS[kid];
  if (prog_idx >= ke->n_ops) return 0;
  KProgOp const *p = &ke->program[prog_idx];
  // Only movement ops have a meaningful BIndex chain entry.
  // Inline the chain-movement check so the accessor can live before
  // the materialize-internal op_is_chain_movement helper.
  u8 op = p->opcode;
  int is_movement = (op == UOP_RESHAPE || op == UOP_EXPAND
                  || op == UOP_PERMUTE || op == UOP_SHRINK
                  || op == UOP_FLIP    || op == UOP_PAD);
  if (!is_movement) return 0;
  // Identity movement ops are elided from the BIndex chain (see
  // bufferize_apply_identity_reshape) so their lookup must miss
  // even though chain_op_idx might fall in range.
  if (kprog_op_is_identity(p)) return 0;

  // Per-USE: pick the chain_edge_idx-th BIndex record for this
  // (kernel, source) pair so multiple paths to the same source
  // resolve to distinct chain entries.
  if (p->chain_input_slot == 0xFFFFFFFFu) return 0;
  if (p->chain_input_slot >= ke->n_inputs) return 0;
  BIndex edge;
  if (!kernel_entry_input_edge_at(kid, p->chain_input_slot,
                                  (u32)p->chain_edge_idx, &edge)) {
    return 0;
  }
  if (edge.chain_op_count == 0) return 0;
  if ((u32)p->chain_op_idx >= edge.chain_op_count) return 0;
  // KProgOp counts source-to-consumer (0 = bottom of chain), BIndex
  // stores consumer-to-source (0 = top of chain).
  u32 b_idx = edge.chain_op_count - 1u - (u32)p->chain_op_idx;
  if (out != NULL) *out = edge.chain_ops[b_idx];
  return 1;
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
    ts.dims[i] = d;
    t_numel *= d;
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
      u32 new_dim = ts.dims[r_idx];
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
    ts.dims[i] = td;
    t_numel  *= td;
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
    if (e <= b || e > src->shape.dims[i]) return 0;
    ts.dims[i] = e - b;
    t_numel  *= (e - b);
    add_off  += (i32)b * src->strides[i];
  }
  out->shape  = ts;
  out->numel  = t_numel;
  out->offset = src->offset + add_off;
  for (u32 i = 0; i < src->shape.ndim; i++) out->strides[i] = src->strides[i];
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  out->contiguous = (t_numel == src->numel) ? 1 : 0;
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
// (typically EXPAND from interact_grad's expand_to_target leaf
// rule) bottoms out at a CONST instead of a TenDesc.
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
  // interact_grad's expand_to_target leaf rule produces
  // EXPAND(CONST(0|1)) -> target.shape; without this branch the
  // grad chain stalls as a UOP that never fires.
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
  if (ok) return tensor_view_of(src_tid, nv);
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
  // failure: fall back to emitting a kernel op (the legacy path).
  return 0;
}

// True when a UOp opcode is one of the 5 view-only-path movement ops.
static u8 op_is_view_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_EXPAND || op == UOP_PERMUTE
      || op == UOP_SHRINK  || op == UOP_FLIP;
}

// True for any movement op the bufferize chain tracks.
static u8 op_is_chain_movement(u8 op) {
  return op_is_view_movement(op) || op == UOP_PAD;
}

// Identity check that mirrors bufferize_chain_op_is_identity in
// schedule/bufferize.c.  Used by prog_chain_propagate to skip src
// ops that bufferize_apply_identity_reshape elided from the
// B_INDEX chain so KProgOp chain depth stays aligned with the
// post-elision BIndex chain length.  Identity covers RESHAPE/EXPAND
// with src_dims == out_dims and PERMUTE with axis_perm[i] == i.
static u8 kprog_op_is_identity(KProgOp const *p) {
  if (p->src0_ndim == 0) return 0;
  if (p->opcode == UOP_RESHAPE || p->opcode == UOP_EXPAND) {
    if (p->src0_ndim != p->out_ndim) return 0;
    for (u32 d = 0; d < p->src0_ndim; d++) {
      if (p->src0_dims[d] != p->out_dims[d]) return 0;
    }
    return 1;
  }
  if (p->opcode == UOP_PERMUTE) {
    if (p->src0_ndim != p->out_ndim) return 0;
    for (u32 d = 0; d < p->src0_ndim; d++) {
      if (p->axis_perm[d] != d) return 0;
    }
    return 1;
  }
  return 0;
}

// Propagate chain_op_idx + chain_input_slot + chain_edge_idx from
// a single src ksrc reference to a freshly-zeroed KProgOp `p`.
// See KProgOp definition in thvm.h for the chain semantics.
//
// Movement src that is also an identity is invisible to the
// bufferize chain (bufferize_apply_identity_reshape elides it), so
// we treat it as non-movement here to keep chain_op_idx aligned
// with B_INDEX chain entries.
//
// chain_edge_idx is taken from `ke->input_visit_counts[slot] - 1`
// when src is a fresh INPUT leaf, then propagated unchanged through
// every parent in the chain.
static void prog_chain_propagate(KernelEntry *ke, KProgOp *p, u32 src_idx) {
  if (KSRC_IS_INPUT(src_idx)) {
    u32 slot = KSRC_INDEX(src_idx);
    p->chain_op_idx     = 0;
    p->chain_input_slot = slot;
    u32 count = (ke->input_visit_counts != NULL && slot < ke->n_inputs)
                  ? ke->input_visit_counts[slot] : 0;
    if (count == 0) count = 1;          // defensive: at least one visit
    p->chain_edge_idx = (count - 1) > 255u ? 255u : (u8)(count - 1);
    return;
  }
  u32 si = KSRC_INDEX(src_idx);
  if (si >= ke->n_ops) {
    p->chain_op_idx     = 0;
    p->chain_input_slot = 0xFFFFFFFFu;
    p->chain_edge_idx   = 0;
    return;
  }
  KProgOp const *src_p = &ke->program[si];
  if (src_p->chain_input_slot == 0xFFFFFFFFu) {
    p->chain_op_idx     = 0;
    p->chain_input_slot = 0xFFFFFFFFu;
    p->chain_edge_idx   = 0;
    return;
  }
  int src_counts =
      op_is_chain_movement(src_p->opcode) && !kprog_op_is_identity(src_p);
  u32 depth = (u32)src_p->chain_op_idx + (src_counts ? 1u : 0u);
  if (depth > 255u) depth = 255u;
  p->chain_op_idx     = (u8)depth;
  p->chain_input_slot = src_p->chain_input_slot;
  p->chain_edge_idx   = src_p->chain_edge_idx;
}

// For chain-breaking ops (binary, REDUCE, etc.) - explicitly mark
// the chain as broken.  The 0xFFFFFFFF sentinel matches the default
// when no source has been visited yet.
static void prog_chain_break(KProgOp *p) {
  p->chain_op_idx     = 0;
  p->chain_input_slot = 0xFFFFFFFFu;
  p->chain_edge_idx   = 0;
}

// Flatten a non-contig TenDesc into a fresh contiguous copy via
// view_strided_index.  Used by thvm_materialize when the root is a
// movement-op chain that resolves to a view alias the caller will
// read through (wnf expects flat-buffer reads).
static Term materialize_root_alias(Term t) {
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
      if (d->view.shape.dims[i] > 1 && d->view.strides[i] > 0)
        m += (i32)(d->view.shape.dims[i] - 1) * d->view.strides[i];
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
  return term_new(0, TAG_TEN, d->dtype, dst_tid);
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

static u32 src_dtype(KernelEntry *ke, u32 src_idx) {
  return KSRC_IS_INPUT(src_idx) ? ke->input_dtypes[KSRC_INDEX(src_idx)]
                                 : ke->program[src_idx].dtype;
}
static u64 src_numel(KernelEntry *ke, u32 src_idx) {
  return KSRC_IS_INPUT(src_idx) ? ke->input_numels[KSRC_INDEX(src_idx)]
                                 : ke->program[src_idx].numel;
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
          "big_input_source kid=%u n_inputs=%u n_ops=%u out=%u source_op=%u loc=%llu\n",
          (u32)(ke - KERNELS), ke->n_inputs, ke->n_ops,
          ke->output_numel, term_ext(ke->source_uop),
          (unsigned long long)boundary_loc);
  materialize_dump_source_child(ke->source_uop, 0);
}

// Recursive visit.  Returns a program-index (0..n_ops-1) or
// VISIT_BAIL on any unsupported op.
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
    if (ke->input_visit_counts != NULL) ke->input_visit_counts[slot]++;
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
      if (ke->input_visit_counts != NULL) ke->input_visit_counts[slot]++;
      return KSRC_AS_INPUT(slot);
    }
    return VISIT_BAIL;          // no shape annotation -- can't compile
  }
  if (tag != TAG_UOP) return VISIT_BAIL;

  u8  op  = term_ext(t);
  u64 loc = term_val(t);
  u32 memo_ref = visit_memo_lookup(memo, loc);
  if (memo_ref != VISIT_BAIL) {
    // If the memo cached an input slot, bump the per-slot visit
    // counter so each path through this UOp loc still gets its own
    // chain_edge_idx.  Without this, two distinct movement chains
    // that both bottom out at the same boundary would receive the
    // same chain_edge_idx and pick the same BIndex record.
    if (KSRC_IS_INPUT(memo_ref) && ke->input_visit_counts != NULL) {
      u32 slot = KSRC_INDEX(memo_ref);
      if (slot < ke->n_inputs) ke->input_visit_counts[slot]++;
    }
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
    if (ke->input_visit_counts != NULL) ke->input_visit_counts[slot]++;
    u32 ref = KSRC_AS_INPUT(slot);
    visit_memo_store(memo, loc, ref);
    return ref;
  }

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
      // Increment the per-slot visit counter so the next
      // prog_chain_propagate call (for whatever movement op sits
      // immediately above this leaf) gets a unique chain_edge_idx.
      // Note: visit_memo_store below caches the ksrc_idx at this
      // loc, so a subsequent visit() to the SAME boundary loc via
      // a different path won't reach this code; the memo cache is
      // an artifact of the current dedup model and a known
      // limitation for the multi-path case.  See the
      // bufferize.md plan for the per-USE rerouting follow-up.
      if (ke->input_visit_counts != NULL) ke->input_visit_counts[slot]++;
      u32 ref = KSRC_AS_INPUT(slot);
      visit_memo_store(memo, loc, ref);
      return ref;
    }
  }

  if (op == UOP_CONST) {
    kernel_program_reserve(ke, ke->n_ops + 1);
    Term num = heap_read(loc);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode = UOP_CONST;
    p->dtype  = term_ext(num);            // dtype on the NUM cell
    p->arg    = (u32)term_val(num);
    p->n_src  = 0;
    p->numel  = 1;
    prog_chain_break(p);                  // CONST has no src chain
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  if (uop_is_unary_elementwise(op) || op == UOP_LOAD) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode = (u8)op;
    p->dtype  = src_dtype(ke, src_idx);
    p->numel  = src_numel(ke, src_idx);
    p->n_src  = 1;
    p->src[0] = src_idx;
    prog_chain_propagate(ke, p, src_idx); // unary passthrough
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  if (op == UOP_CAST || op == UOP_BITCAST) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    Term num = heap_read(loc + 1);
    if (term_tag(num) != TAG_NUM) return VISIT_BAIL;
    u32 dst_dtype = (u32)term_val(num);
    kernel_program_reserve(ke, ke->n_ops + 1);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode = (u8)op;
    p->dtype  = dst_dtype;
    // arg carries the source dtype so the kernel can route through
    // to_fp32_lane / from_fp32_lane (see backend/cpu/op/cast.c).
    p->arg    = src_dtype(ke, src_idx);
    p->numel  = src_numel(ke, src_idx);
    p->n_src  = 1;
    p->src[0] = src_idx;
    prog_chain_propagate(ke, p, src_idx); // CAST/BITCAST passthrough
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  if (uop_is_binary_elementwise(op)) {
    u32 li = visit(heap_read(loc + 0), ke, root_loc, memo);
    if (li == VISIT_BAIL) return VISIT_BAIL;
    u32 ri = visit(heap_read(loc + 1), ke, root_loc, memo);
    if (ri == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode = (u8)op;
    // p->numel is the op's *output* element count.  Compute it from
    // the term's broadcast output shape as a u64 product -- NOT as
    // max(operand .numel), because an operand resolved to a strided
    // input view stores its (possibly > 2^32) logical numel in a u32
    // View.numel / KernelEntry.input_numels[] slot, which truncates.
    // An im2col-matmul's MUL operand {cOut, cIn*kh*kw, B*hOut*wOut} =
    // {32,800,204800} has 5.24e9 logical elems at BS=512; the
    // truncated 947912704 then poisons the downstream REDUCE's
    // numel (= mul.numel / reduce_extent) and rangeify's
    // divisibility gate -> spurious bail -> per-op fallback ->
    // 3.8 GB alloc -> ceiling refusal.  Falls back to max(operand
    // numels) only if term_shape_in fails (degenerate / unshaped).
    {
      Shape out_shape = {0};
      if (term_shape_in(t, 0, &out_shape) && out_shape.ndim > 0) {
        u64 onum = 1;
        for (u32 i = 0; i < out_shape.ndim; i++) onum *= (u64)out_shape.dims[i];
        p->numel = onum;
      } else {
        u64 ln = src_numel(ke, li), rn = src_numel(ke, ri);
        p->numel = (ln >= rn) ? ln : rn;
      }
    }
    p->dtype  = src_dtype(ke, li);
    p->n_src  = 2;
    p->src[0] = li;
    p->src[1] = ri;
    prog_chain_break(p);                  // binary: chain has two paths, breaks
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
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
  // to kernel-op emit with the appropriate metadata.
  if (op_is_view_movement(op)) {
    u32 alias_tid = view_resolve(t);
    if (alias_tid != 0) {
      u32 slot = input_slot_dedup(ke, alias_tid, t);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      u32 ref = KSRC_AS_INPUT(slot);
      visit_memo_store(memo, loc, ref);
      return ref;
    }
    // Fallback: emit as a kernel op.  Recurse into source, look up
    // shapes, populate the metadata cpu_op_<op> + Metal shaders need.
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
    Shape src_shape = {0};
    if (!term_shape_in(heap_read(loc), 0, &src_shape)) return VISIT_BAIL;
    Shape out_shape = {0};
    if (!term_shape_in(t, 0, &out_shape)) return VISIT_BAIL;
    // KProgOp.numel is u64 (so big conv-bwd-dInput shapes don't overflow);
    // accumulate in u64 too so the multiply can't lose high bits.
    u64 out_numel = 1;
    for (u32 i = 0; i < out_shape.ndim; i++) out_numel *= (u64)out_shape.dims[i];
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode    = op;
    p->dtype     = src_dtype(ke, src_idx);
    p->numel     = out_numel;
    p->n_src     = 1;
    p->src[0]    = src_idx;
    p->src0_ndim = (u8)(src_shape.ndim & 0xFF);
    p->out_ndim  = (u8)(out_shape.ndim & 0xFF);
    for (u32 i = 0; i < src_shape.ndim; i++) p->src0_dims[i] = src_shape.dims[i];
    for (u32 i = 0; i < out_shape.ndim; i++) p->out_dims [i] = out_shape.dims[i];
    if (op == UOP_PERMUTE) {
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 pi = (u32)term_val(heap_read(loc + 2 + i));
        p->axis_perm[i] = (u8)(pi & 0xFF);
      }
    }
    if (op == UOP_SHRINK) {
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
        u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
        p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
        p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
      }
    }
    if (op == UOP_FLIP) {
      // axes_mask sits in the wrapping UOP cell's ext field
      // (uop_flip stuffs it via term_new's ext).  Wait -- actually
      // uop_flip puts the mask in heap[loc+1] as a NUM cell.  Check
      // the constructor.
      Term mask_num = heap_read(loc + 1);
      if (term_tag(mask_num) == TAG_NUM) p->arg = (u32)term_val(mask_num);
    }
    prog_chain_propagate(ke, p, src_idx);   // movement op (view-path)
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  // PAD as a kernel emit (g2c2): allocate a fresh buf, run
  // cpu_op_pad / metal pad shader.  Unlike SHRINK/PERMUTE/etc, PAD
  // can't be a view-only alias because reading bytes outside the
  // alloc is UB even when calloc'd.
  if (op == UOP_PAD) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
    // Source shape: from the PAD's source term (TenDesc lookup).
    Shape src_shape = {0};
    if (!term_shape_in(heap_read(loc), 0, &src_shape)) return VISIT_BAIL;
    // Output shape: src.dim[i] + b_i + e_i per axis.
    Shape out_shape = src_shape;
    u64   out_numel = 1;
    for (u32 i = 0; i < src_shape.ndim; i++) {
      u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
      out_shape.dims[i] = src_shape.dims[i] + b + e;
      out_numel        *= (u64)out_shape.dims[i];
    }
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode    = UOP_PAD;
    p->dtype     = src_dtype(ke, src_idx);
    p->numel     = out_numel;
    p->n_src     = 1;
    p->src[0]    = src_idx;
    p->src0_ndim = (u8)(src_shape.ndim & 0xFF);
    p->out_ndim  = (u8)(out_shape.ndim & 0xFF);
    for (u32 i = 0; i < src_shape.ndim; i++) {
      p->src0_dims[i] = src_shape.dims[i];
      p->out_dims [i] = out_shape.dims[i];
      u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
      p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
      p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
    }
    prog_chain_propagate(ke, p, src_idx);   // PAD movement op
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  // REDUCE -- as the kernel root (tail-fuse) or as an intermediate
  // op whose result is consumed elementwise (broadcast) by later
  // program ops.  The "at most one REDUCE per kernel" invariant
  // (used by render_uop's reduce-tail accumulator hoist and by
  // cpu_op_reduce's per-output indexing) is enforced by counting
  // REDUCEs already in the program.
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
      // Multi-REDUCE per kernel is permitted: rangeify's scalar layer
      // supports multiple reduce ranges per kernel
      // (docs/plans/rewrite_fusion.md:161), and the legacy
      // cpu_op_reduce path that the old "one REDUCE per kernel" rule
      // protected was deleted in the F6 cleanup (src/thvm.c:173).
      u32 src_idx = visit(rc.src, ke, root_loc, memo);
      if (src_idx == VISIT_BAIL) return VISIT_BAIL;
      kernel_program_reserve(ke, ke->n_ops + 1);
      KProgOp *p = &ke->program[ke->n_ops++];
      memset(p, 0, sizeof(*p));
      p->source_uop = t;
      p->opcode = UOP_REDUCE;
      p->dtype  = src_dtype(ke, src_idx);
      p->arg    = (rc.kind << 24) | (rc.inner & 0x00FFFFFFu);
      p->numel  = rc.out_numel;
      p->n_src  = 1;
      p->src[0] = src_idx;
      p->src0_ndim      = rc.src_ndim;
      p->out_ndim       = rc.out_ndim;
      p->n_reduce_axes  = rc.n_reduce_axes;
      for (u32 i = 0; i < rc.src_ndim && i < MAX_DIM; i++) {
        p->src0_dims[i] = rc.src_dims[i];
      }
      for (u32 i = 0; i < rc.out_ndim && i < MAX_DIM; i++) {
        p->out_dims[i] = rc.out_dims[i];
      }
      for (u32 i = 0; i < rc.n_reduce_axes && i < MAX_DIM; i++) {
        p->reduce_axes[i] = rc.reduce_axes[i];
      }
      prog_chain_break(p);                  // REDUCE breaks the chain
      u32 ref = ke->n_ops - 1;
      visit_memo_store(memo, loc, ref);
      return ref;
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
      u32 axis0 = (u32)term_val(heap_read(loc + 2));
      Shape src_sh0 = {0};
      Shape out_sh0 = {0};
      int same_shape = term_shape_in(heap_read(loc), 0, &src_sh0)
                    && term_shape_in(t, 0, &out_sh0)
                    && src_sh0.ndim == out_sh0.ndim;
      if (same_shape) {
        for (u32 d = 0; d < src_sh0.ndim; d++) {
          if (src_sh0.dims[d] != out_sh0.dims[d]) { same_shape = 0; break; }
        }
      }
      if (same_shape && axis0 < src_sh0.ndim && src_sh0.dims[axis0] == 1) {
        u32 sidx = visit(heap_read(loc), ke, root_loc, memo);
        if (sidx == VISIT_BAIL) return VISIT_BAIL;
        visit_memo_store(memo, loc, sidx);
        return sidx;
      }
    }
    // Multi-REDUCE per kernel is permitted; see the comment in the
    // multi-reduce-chain branch above.
    u32 src_idx = visit(heap_read(loc), ke, root_loc, memo);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
    u32 kind = (u32)term_val(heap_read(loc + 1));
    u32 axis = (u32)term_val(heap_read(loc + 2));
    // arg encoding (cpu_op_reduce / Metal reduce shader):
    //   bits 24..31 = kind, bits 0..23 = inner = prod(dims[axis+1..]).
    // Packing axis here (the round-1 bug) gave inner = 0 -> fallback
    // to 1 -> axis treated as innermost, which is correct only when
    // axis IS innermost.  Compute inner from the source shape.
    Shape src_shape = {0};
    u32 inner = 1;
    u64 src_numel_total = src_numel(ke, src_idx);
    if (term_shape_in(heap_read(loc), 0, &src_shape)) {
      for (u32 i = axis + 1; i < src_shape.ndim; i++) inner *= src_shape.dims[i];
    }
    // Output numel of THIS REDUCE op (not necessarily the kernel's
    // output): src_numel / axis_size.  axis_size = src_shape.dims[axis]
    // when shape is known; otherwise fall back to the kernel's
    // output_numel (root-REDUCE case).
    u64 reduce_numel = ke->output_numel;
    if (src_shape.ndim > axis) {
      u32 axis_size = src_shape.dims[axis] ? src_shape.dims[axis] : 1;
      reduce_numel = src_numel_total / axis_size;
    }
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->source_uop = t;
    p->opcode = UOP_REDUCE;
    p->dtype  = src_dtype(ke, src_idx);
    p->arg    = (kind << 24) | (inner & 0x00FFFFFFu);
    p->numel  = reduce_numel;
    p->n_src  = 1;
    p->src[0] = src_idx;
    if (src_shape.ndim > 0 && src_shape.ndim <= MAX_DIM) {
      p->src0_ndim     = (u8)src_shape.ndim;
      p->n_reduce_axes = 1;
      p->reduce_axes[0] = (u8)(axis & 0xFFu);
      for (u32 i = 0; i < src_shape.ndim; i++) {
        p->src0_dims[i] = src_shape.dims[i];
      }
      Shape out_shape = {0};
      if (term_shape_in(t, 0, &out_shape)) {
        p->out_ndim = (u8)out_shape.ndim;
        for (u32 i = 0; i < out_shape.ndim && i < MAX_DIM; i++) {
          p->out_dims[i] = out_shape.dims[i];
        }
      }
    }
    prog_chain_break(p);                    // REDUCE breaks the chain
    u32 ref = ke->n_ops - 1;
    visit_memo_store(memo, loc, ref);
    return ref;
  }

  return VISIT_BAIL;
}

// === Multi-output kernel splice (Step 6 of multi-output groundwork) ===
//
// When THVM_KERNEL_MERGE=1 and the active backend's dispatcher
// honors KProgOp.store_extra_plus_one (today: CPU's cpu_interpret
// post-pass and Metal's per-op encoder), emit_kernel_for_boundary
// calls splice_child_into_host on every host A whose plan flagged
// a child B with merge_into[B]=A.
//
// Sequencing inside A's emit_kernel_for_boundary:
//   1. Detect a planned merge child B (via find_merge_child).
//   2. PREMERGE: visit B's term against the SAME (still-empty)
//      KernelEntry so B's KProgOps land at [0, b_n).  Sharing the
//      KernelEntry's input_tids[] table means common boundary
//      inputs dedup automatically across A and B's subgraphs.
//   3. Mark B's last op (b_n - 1) with store_extra_plus_one = 1,
//      so cpu_interpret's post-pass / metal_dispatch_kernel's
//      per-op encoder routes its value to the kernel's first
//      extra output buffer instead of (or in addition to) the
//      legacy scratch slot.
//   4. The caller then visits A's term normally, appending A's
//      KProgOps at [b_n, total).  The LAST op of the combined
//      program is A's last op, which the legacy "last op writes
//      to out_buf_id" cpu_interpret semantics correctly route to
//      A's primary output tid.
//   5. Allocate B's output TenDesc on the host backend, register
//      it via kernel_entry_set_extra_output(slot=1, ...), and
//      rebind BOUNDARY_TID[B] / BOUNDARY_TERM[B] so downstream
//      visit() lookups + sink-kernel resolution see the new tid.
//   6. Set TENS[extra_out_tid].producer_kid = host_kid so
//      kernel_fire_by_id chases the merged kernel when wnf
//      consumes B's output.
//
// emit_kernel_for_boundary(B) (later in the BOUNDARY_ORDER walk)
// detects BOUNDARY_TID[B] is already populated and short-circuits
// via the alias-Term early return at the top of the function.
//
// Returns 1 on successful splice, 0 if the splice was skipped or
// bailed.  When 0, the caller proceeds with A's normal emit and
// the planner's flag is silently downgraded.
static int splice_child_into_host_premerge(KernelEntry *ke,
                                            u32 host_bi,
                                            u32 child_bi) {
  (void)host_bi;
  if (child_bi >= BOUNDARY_ORDER_LEN) return 0;
  if (ke->n_extra_outputs >= KERNEL_MAX_EXTRA_OUTPUTS) return 0;
  // Splice runs BEFORE A's visit; ke must be empty of program ops
  // so B lands at indices [0, b_n).
  if (ke->n_ops != 0) return 0;

  u64 child_loc = BOUNDARY_ORDER[child_bi];
  u32 child_ridx = bufferize_info_find(child_loc);
  if (child_ridx == 0xFFFFFFFFu) return 0;

  Shape child_shape = {0};
  u8    child_op    = BUFFERIZE_NODES[child_ridx].op;
  Term  child_term  = term_new(0, TAG_UOP, child_op, child_loc);
  if (!term_shape_in(child_term, 0, &child_shape)) return 0;
  u32 child_dtype = DT_FP32;
  term_dtype_in(child_term, 0, &child_dtype);

  VisitMemo b_memo = {0};
  u32 b_result = visit(child_term, ke, child_loc, &b_memo);
  visit_memo_free(&b_memo);
  if (b_result == VISIT_BAIL) {
    ke->n_ops = 0;
    return 0;
  }
  if (ke->n_ops == 0 || KSRC_IS_INPUT(b_result)) {
    // Degenerate movement-op-alias case (child consumed as a
    // single input slot).  Bail rather than synthesize a store-
    // only op; the unmerged emit can still produce a valid kernel
    // for the child later.
    ke->n_ops = 0;
    return 0;
  }

  u32 b_last_op_idx = KSRC_INDEX(b_result);
  if (b_last_op_idx >= ke->n_ops) {
    ke->n_ops = 0;
    return 0;
  }
  ke->program[b_last_op_idx].store_extra_plus_one = 1;

  // Allocate B's output TenDesc on the host backend; register it
  // as the kernel's first extra output.
  u32 host_kid = (u32)(ke - KERNELS);
  u32 child_out_tid = tensor_alloc(CURRENT_BACKEND, child_shape, child_dtype);
  if (child_out_tid == 0) {
    ke->n_ops = 0;
    return 0;
  }
  u32 child_numel = TENS[child_out_tid].view.numel;
  if (!kernel_entry_set_extra_output(host_kid, 1, child_out_tid,
                                     child_dtype, &child_shape, child_numel)) {
    tensor_release(child_out_tid);
    ke->n_ops = 0;
    return 0;
  }
  TENS[child_out_tid].producer_kid = host_kid;

  Term child_alias_term = term_new(0, TAG_TEN, child_dtype, child_out_tid);
  BOUNDARY_TID [child_bi] = child_out_tid;
  BOUNDARY_TERM[child_bi] = child_alias_term;

  return 1;
}

// Find the (single) child boundary B such that BOUNDARY_MERGE_INTO[B]
// = host_bi, returning B's index or BOUNDARY_MERGE_NONE.  The
// planner stops at one merge per host (1->1 fusion), so the linear
// scan is O(N) but typically returns early.
static u32 find_merge_child(u32 host_bi) {
  for (u32 b = host_bi + 1; b < BOUNDARY_ORDER_LEN; b++) {
    if (BOUNDARY_MERGE_INTO[b] == host_bi) return b;
  }
  return BOUNDARY_MERGE_NONE;
}

// Predicate: does the active backend's dispatcher honor
// kernel_entry_set_extra_output / KProgOp.store_extra_plus_one?
// As of Step 6+7 (this commit), CPU's cpu_interpret post-pass and
// Metal's per-op encoder both route the marked KProgOp's value to
// the extra output buffer.  JIT and tile-JIT paths still bail at
// cg_kernel_has_extra_outputs guards (those backends would need a
// per-output JIT signature to land first).  Returning 1 here makes
// the splice fire on either backend; when it fails to fire (e.g.
// custom Backend that doesn't yet honor extras), the splice is
// silently skipped and the kernel stays single-output.
static int splice_target_backend_supports_multi_output(void) {
  return CURRENT_BACKEND != NULL;
}

// Build one kernel rooted at the boundary at index bi.  Returns
// the emitted UOP_KERNEL term, or 0 on bail.
static Term emit_kernel_for_boundary(u32 bi) {
  u64 boundary_loc = BOUNDARY_ORDER[bi];
  u32 idx = bufferize_info_find(boundary_loc);
  if (idx == 0xFFFFFFFFu) return 0;

  // Multi-output kernel splice (Step 6): if this boundary is a
  // child that an earlier-emitted host already absorbed via
  // splice_child_into_host, BOUNDARY_TID/BOUNDARY_TERM are already
  // populated.  Skip emission and re-return the alias Term so the
  // emit-loop sees a non-zero result and so a sink-as-child case
  // (the realize root being merged into a sibling) returns the
  // right surface Term to wnf.
  if (BOUNDARY_TID[bi] != 0 && BOUNDARY_TERM[bi] != 0) {
    return BOUNDARY_TERM[bi];
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

  u32 out_tid = tensor_alloc(CURRENT_BACKEND, out_shape, out_dtype);
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

  // Multi-output kernel splice (Step 6 of multi-output groundwork).
  // PREMERGE: when THVM_KERNEL_MERGE=1 and the planner flagged a
  // child boundary B with merge_into[B]=bi, visit B's program
  // FIRST so its KProgOps occupy program indices [0, b_n) -- this
  // way A's last op stays the LAST op overall and cpu_interpret /
  // metal_dispatch_kernel's "last op -> primary out_buf_id" path
  // continues to write A's value to A's tid.  B's last op is
  // marked with store_extra_plus_one = 1 so the dispatcher's post-
  // pass / encoder routes B's computed value into the kernel's
  // first extra output buffer.  Skipped when:
  //   - THVM_KERNEL_MERGE=0 (default; find_merge_child returns
  //     NONE),
  //   - active backend can't dispatch multi-output kernels (today
  //     CPU + Metal both can; future Backend impls without
  //     store_extra_plus_one support fall back here),
  //   - the splice itself bails (visit on B returned VISIT_BAIL,
  //     cap full, or extra output slot exhausted) -- the kernel
  //     stays single-output and B emits separately later.
  int spliced_ok = 0;
  if (kernel_merge_enabled()
      && splice_target_backend_supports_multi_output()) {
    u32 child_bi = find_merge_child(bi);
    if (child_bi != BOUNDARY_MERGE_NONE) {
      spliced_ok = splice_child_into_host_premerge(ke, bi, child_bi);
    }
  }

  VisitMemo memo = {0};
  u32 result = visit(root_term, ke, boundary_loc, &memo);
  visit_memo_free(&memo);
  if (result == VISIT_BAIL) {
    kernel_dealloc_last(kid);
    TENS[out_tid].producer_kid = 0;
    return 0;
  }

  // Degenerate case: visit() consumed the whole boundary subgraph as
  // a single input slot (n_ops == 0, result == KSRC_AS_INPUT).  This
  // happens when the boundary's root is a movement-op chain whose
  // view_resolve found a direct alias TenDesc -- no compute to do.
  // Without this branch the kernel commits with an empty program;
  // cpu_interpret runs nothing; the alloc'd output buffer stays
  // zero-initialized, silently zeroing whatever signal was supposed
  // to flow through (the gy=CONST(1.0) seed in MSE backward, etc).
  // Skip kernel emission and alias the boundary's output to the
  // input tid directly.
  if (ke->n_ops == 0 && KSRC_IS_INPUT(result)) {
    u32 alias_tid = ke->input_tids[KSRC_INDEX(result)];
    if (alias_tid != 0 && alias_tid < TENS_NEXT) {
      // Release the unused output_tid we speculatively allocated.
      tensor_release(out_tid);
      kernel_dealloc_last(kid);
      Term alias_term = term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
      BOUNDARY_TID [bi] = alias_tid;
      BOUNDARY_TERM[bi] = alias_term;
      return alias_term;
    }
  }

  materialize_dump_big_input_source(ke, boundary_loc);

  ke->schedule = &ke->_local_schedule;

  // Default-init the axis-typed scheduling plan now that the program
  // and output_shape are finalized.  Idempotent: a cached slot whose
  // axes were already populated by an earlier kid sharing this
  // program is a no-op, so opts already applied to the program shape
  // survive across new kid emissions.
  axes_default_for(ke);

  // Rangeify lowering: produce a parallel scalar-UOp form alongside
  // the legacy KProgOp[].  When the lowering succeeds, seed the
  // TileUop schedule plan above it; opt-in tile dispatch can consume
  // that plan while default dispatch still routes through the existing
  // scalar/KProgOp paths.  When rangeify bails (op not yet supported,
  // broadcast pattern not handled, etc.), the legacy KProgOp[]
  // dispatch runs and scalar_uops/tile_uops stay empty.  Default ON;
  // THVM_RANGEIFY=0 disables and reverts to the legacy emit path.
  // Reads getenv per emit (cheap; ~1us) so test harnesses can flip
  // the flag mid-session without restarting the runtime.
  // Multi-output kernels (spliced_ok above) can't go through
  // rangeify today: rangeify_try_lower_elementwise emits a single
  // S_BUFFERIZE / S_STORE pair against output slot 0, with no
  // facility for a second BUFFERIZE rooted at the extra-output
  // slot.  Skip lowering on those kernels so dispatch falls through
  // to the legacy emit path, which honors
  // KProgOp.store_extra_plus_one.  Rangeify multi-output is
  // tracked as a separate follow-up under
  // docs/plans/rewrite_fusion.md.
  if (!spliced_ok) {
    const char *e = getenv("THVM_RANGEIFY");
    int rangeify_on = (e == NULL) ? 1 : (e[0] != '0');
    int lowered = rangeify_on && rangeify_try_lower_elementwise(ke);
    if (lowered) {
      rangeify_cse(ke);
      rangeify_dce(ke);
      axes_ensure_scalar_reduce(ke);
      // (Scalar-UOp divandmod simplification was deleted with
      // src/scalar/simplify.c.  An A/B test on lenet-mnist
      // bench-train showed the lift-reject count was identical with
      // THVM_SCALAR_SIMPLIFY=0 -- the simplifier wasn't load-bearing
      // for the post-L54 view-folding path.)
    }
    if (lowered) {
      axes_ensure_scalar_reduce(ke);
    }
  }

  // Cache the full kernel_lift_to_uop output on the KernelEntry
  // alongside the program[] / scalar_uops[] outputs.  The lifter
  // handles three shapes: (a) gemm-only kernels that bypass rangeify
  // (scalar_uops == NULL but kernel_lift_from_gemm succeeds),
  // (b) conv2d-only kernels (kernel_lift_from_conv2d), and
  // (c) rangeified kernels (the ScalarUop walker).  When the lift
  // declines (multi-output spliced, unsupported shape, n_inputs >
  // KERNEL_LIFT_MAX_INPUT), cached_lift stays zero-initialized and
  // the program[] path remains primary.
  //
  // Dispatch-time consumers (cpu_jit_build, cg_emit_via_uop,
  // cpu_uop_walk) read store_root / out_buf / in_bufs[] from
  // cached_lift without re-running the lifter.  compute_root is
  // kept populated as a redundant view of cached_lift.store_root.
  if (kernel_lift_to_uop(ke, &ke->cached_lift)) {
    ke->compute_root = ke->cached_lift.store_root;
    // THVM_LIFT_FROM_UNIFIED=1: substitute the lifter's store_root
    // with the unified-pass output where available.  Diagnostic
    // bypass; the lifter's other cached_lift fields (in_bufs[],
    // n_inputs) stay populated from the legacy path, so downstream
    // CPU walker / Metal renderer use the unified root for compute
    // but the legacy buffer table for I/O binding.
    if (getenv("THVM_LIFT_FROM_UNIFIED")) {
      Term ru_root = rangeify_unified_store_root_at(idx);
      if (ru_root != 0) {
        Term ru_rewritten = unified_store_root_for_walker(ke, ru_root);
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
        //                per-axis ranges without consulting strides;
        //                the legacy kernel_lift consults
        //                ke->input_views[slot].strides and emits
        //                CONST(0) for broadcast axes.  Until ru_pass
        //                threads view strides through INDEX_E address
        //                construction, decline the bypass for these
        //                inputs.  Softmax-CE backward hits this where
        //                a 1-element scalar feeds a 3-element consumer
        //                via stride-0 broadcast.
        //
        // When any gate trips, leave store_root as the legacy
        // lifter's output for this kernel; the rest of the schedule
        // still gets the bypass.
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
        if (!has_resid && !has_stranded && !has_bcast) {
          ke->cached_lift.store_root = ru_rewritten;
          ke->compute_root           = ru_rewritten;
        }
      }
    }
    // Identity check: rangeify_unified_store_root_at(idx) is the
    // unified-pass UOP_STORE for this boundary (when dtype inference
    // succeeded).  Under THVM_LIFT_BUFFERIZE_TRACE=1 emit a stderr
    // line on every mismatch so the eventual lifter bypass has a
    // clear bisect signal for which kernel shapes still diverge.
    if (getenv("THVM_LIFT_BUFFERIZE_TRACE")) {
      Term ru_root = rangeify_unified_store_root_at(idx);
      Term l_root  = ke->cached_lift.store_root;
      // Under THVM_LIFT_BUFFERIZE_SIMPLIFY=1, run uop_graph_simplify
      // on both before comparing.  Catches canonicalization-only
      // divergences (IADD/IMUL nesting order, identity folds, etc.).
      if (getenv("THVM_LIFT_BUFFERIZE_SIMPLIFY")) {
        if (l_root != 0)  l_root  = uop_graph_simplify(l_root);
        if (ru_root != 0) ru_root = uop_graph_simplify(ru_root);
      }
      if (ru_root != 0 && ru_root != l_root) {
        Term l_buf   = uop_store_buf  (l_root);
        Term l_addr  = uop_store_addr (l_root);
        Term l_value = uop_store_value(l_root);
        Term r_buf   = uop_store_buf  (ru_root);
        Term r_addr  = uop_store_addr (ru_root);
        Term r_value = uop_store_value(ru_root);
        char const *which =
            (l_buf   != r_buf)   ? "buf"   :
            (l_addr  != r_addr)  ? "addr"  :
            (l_value != r_value) ? "value" : "shape";
        if (l_buf != r_buf) {
          u32 l_nd = uop_buffer_ndim(l_buf);
          u32 r_nd = uop_buffer_ndim(r_buf);
          u32 l_dt = uop_buffer_dtype(l_buf);
          u32 r_dt = uop_buffer_dtype(r_buf);
          fprintf(stderr,
                  "THVM_LIFT_BUFFERIZE_MISMATCH kid=%u diverges=buf "
                  "lift_dtype=%u unified_dtype=%u lift_ndim=%u unified_ndim=%u "
                  "lift_dims=[", kid, l_dt, r_dt, l_nd, r_nd);
          for (u32 d = 0; d < l_nd; d++) fprintf(stderr, "%s%u", d ? "," : "", uop_buffer_dim(l_buf, d));
          fputs("] unified_dims=[", stderr);
          for (u32 d = 0; d < r_nd; d++) fprintf(stderr, "%s%u", d ? "," : "", uop_buffer_dim(r_buf, d));
          fputs("]\n", stderr);
        } else {
          fprintf(stderr,
                  "THVM_LIFT_BUFFERIZE_MISMATCH kid=%u diverges=%s "
                  "lift_addr=%016llx unified_addr=%016llx "
                  "lift_value=%016llx unified_value=%016llx\n",
                  kid, which,
                  (unsigned long long)l_addr, (unsigned long long)r_addr,
                  (unsigned long long)l_value, (unsigned long long)r_value);
        }
      }
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
      ke->compute_root           = post;
    }
    // Dual-write: when the lift succeeds, cached_lift.store_root is
    // the canonical UOp DAG and program[] is the legacy KProgOp side
    // table.  cpu_blas_dispatch and tile.c's gemm/gemv recognisers
    // still consume program[]; backprop through UOP_KERNEL reads it
    // too.  NULL program[] -> all-zero gradients -> training breaks.
    // THVM_PHASE_C7_FREE_PROGRAM=1 frees program[] post-lift for
    // memory/perf experiments once every consumer is DAG-only.
    char const *free_e = getenv("THVM_PHASE_C7_FREE_PROGRAM");
    int free_program_on = (free_e != NULL) && (free_e[0] == '1');
    if (free_program_on) {
      if (ke->program != NULL) {
        free(ke->program);
      }
      ke->program        = NULL;
      ke->n_ops          = 0;
      ke->ops_cap        = 0;
    }
  }

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
static void materialize_inner_assigns(Term term) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  u8 ar = uop_arity((u8)op);
  if (ar == 0) return;
  u64 loc = term_val(term);
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_ASSIGN) {
      u64  cloc     = term_val(child);
      Term csrc     = heap_read(cloc + 1);
      Term csrc_mat = thvm_materialize(csrc);
      if (csrc_mat != csrc) heap_set(cloc + 1, csrc_mat);
    } else {
      materialize_inner_assigns(child);
    }
  }
}

fn Term thvm_materialize(Term term) {
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
  // GRAD is a stop point in materialize -- wnf fires interact_grad,
  // then thvm_realize loops back here to compile the unrolled UOps.
  // ASSIGN is a wnf-fired primitive (interact_assign) -- not a kernel.
  // Materialize the SRC subgraph so its kernels are compiled, then
  // re-wrap as ASSIGN(dst, materialized_src).  Wnf later fires the
  // src kernels, lands a TEN in heap[loc+1], and interact_assign
  // memcpys it into dst.buf.
  if (term_ext(term) == UOP_ASSIGN) {
    u64  loc        = term_val(term);
    Term dst_cell   = heap_read(loc + 0);
    Term src_cell   = heap_read(loc + 1);
    Term src_mat    = thvm_materialize(src_cell);
    if (src_mat != src_cell) heap_set(loc + 1, src_mat);
    (void)dst_cell;
    return term;
  }
  // Pre-walk: recursively scan the UOP DAG for NESTED ASSIGNs and
  // materialize each one's src subgraph in place.  Without this, an
  // ASSIGN buried inside the src of an outer ASSIGN (or several
  // levels deep in a compound UOP -- e.g. Adam's
  // `w - lr * mAfter / denom`) keeps a raw UOP src that wnf can't
  // reduce to a TEN, so the ASSIGN never fires.
  materialize_inner_assigns(term);

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
        return materialize_root_alias(alias_term);
      }
    }
  }

  bufferize_classify(term);
  // The unified rangeify pass projects its UOP_BUFFERIZE Terms back
  // onto BUFFERIZE_NODES.realized; we capture them into
  // BOUNDARY_BUFFERIZE_TERM[] here.  Mirror: tinygrad walks the
  // lowered tsink for BUFFERIZE+STORE pairs (tinygrad/engine/realize.py).
  topo_sort_buffers_unified(term);
  plan_kernel_merges();
  mem_plan_reset();
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
      return term;
    }
    if (BOUNDARY_ORDER[i] == term_val(term)) sink_kernel = k;
  }
  // End-of-pass: pop any planner-pushed bufs still on CPU_FREELIST so
  // a subsequent thvm_realize -> materialize iteration doesn't pull
  // from them (the chain rule's freshly-emitted UOPs may still
  // reference those tids).
  mem_plan_drain_freelist();
  // Diagnostic: print per-kernel input edge data for every kernel
  // emitted in this pass.  No-op unless
  // DUMP_BUFFERIZE_KERNEL_EDGES=1.
  materialize_dump_kernel_edges(kernels_at_start);
  return sink_kernel != 0 ? sink_kernel : term;
}
