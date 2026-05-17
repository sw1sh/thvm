// thvm_atp_* - saturation loop state (stage 5.1).
//
// Heap-allocated AtpState plus init / free / add_equation / set_goal
// helpers.  The actual saturation step (thvm_atp_step) lands in 5.2;
// the priority-aware CP selection in 5.3; the recursive-descent
// rewriter feeding step 4 of the algorithm in 5.4.  This file just
// gives the loop a place to live.
//
// See docs/plans/saturation_loop.md for the algorithm.

// === 8.1c: ATP primitives registered into the TAG_PRI table ========
//
// `prim_unify_apply` is the first primitive: takes two terms (s, t),
// tries `thvm_unify`, and on success returns `thvm_unify_apply(s,
// &subst)` -- the unified term that both s and t collapse to under
// σ.  On failure, returns ERA so the surrounding APP-PRI structure
// short-circuits via APP-ERA when consumed by SUP-encoded CP
// enumeration in 8.1d.
//
// Linear matching: thvm_unify uses a stack-allocated RewriteSubst,
// so the primitive is reentrant and stateless w.r.t. the caller.
static Term prim_unify_apply(Term *args) {
  Term s = args[0];
  Term t = args[1];
  RewriteSubst subst = {{0}};
  if (!thvm_unify(s, t, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_unify_apply(s, &subst);
}

// 8.1e-ii: arity-3 variant.  Takes (s, t, target); returns
// `thvm_unify_apply(target, &subst)` where σ = mgu(s, t), or ERA
// if (s, t) fails to unify.  Lets the IC-routed CP enumerator
// build sigma from one pair and apply it to a different term --
// the workflow `thvm_critical_pairs_range` performs internally.
static Term prim_unify_apply3(Term *args) {
  Term s      = args[0];
  Term t      = args[1];
  Term target = args[2];
  RewriteSubst subst = {{0}};
  if (!thvm_unify(s, t, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_unify_apply(target, &subst);
}

// 8.2b: process-global KboConfig registry.  Pointers don't fit
// cleanly in a Term's `val` field, so IC code invokes the KBO
// comparator with a NUM-encoded cfg_id and the registry resolves
// the actual `KboConfig *` at fire time.
//
// `kbo_cfg_register` is idempotent for tests; saturation code
// typically calls it once during setup.  `kbo_cfg_get` returns
// NULL for unregistered ids so `prim_kbo` can return ERA
// defensively.
static const KboConfig *KBO_CFG_TABLE[KBO_CFG_TABLE_CAP];

fn u32 kbo_cfg_register(u32 cfg_id, const KboConfig *cfg) {
  if (cfg_id >= KBO_CFG_TABLE_CAP) return 0;
  KBO_CFG_TABLE[cfg_id] = cfg;
  return cfg_id;
}

fn const KboConfig *kbo_cfg_get(u32 cfg_id) {
  if (cfg_id >= KBO_CFG_TABLE_CAP) return NULL;
  return KBO_CFG_TABLE[cfg_id];
}

// 8.2b: arity-3 KBO primitive.  Takes (s, t, cfg_id_NUM); returns
// NUM(KboCmp) -- the 4-valued comparison result -- or ERA if
// cfg_id is bogus / no config registered.  Lets IC code invoke
// the KBO comparator from inside an APP-PRI chain.
static Term prim_kbo(Term *args) {
  Term s   = args[0];
  Term t   = args[1];
  Term cid = args[2];
  if (term_tag(cid) != TAG_NUM) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  const KboConfig *cfg = kbo_cfg_get((u32)term_val(cid));
  if (cfg == NULL) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  KboCmp r = thvm_kbo(s, t, cfg);
  return term_new(0, TAG_NUM, DT_INT32, (u64)r);
}

// 8.2c: pure-IC structural-equality on terms.  The C body handles
// the leaf cases (tag mismatch, FVR with same id, NUM with same
// val); for CTR with arity n it BUILDS an AND chain of n
// self-recursive APP-PRI calls and returns the unfired chain --
// the wnf reducer then evaluates the AND, firing each child
// comparison through APP-PRI saturation, short-circuiting on the
// first NUM(0).  This is "IC-driven control flow with C base
// cases" -- closer to the design memo's option (2) than option
// (3), but a real proof point that recursive structural code
// runs through our reducer end-to-end.
//
// Build a single recursive call: APP(APP(PRI(id), child_s), child_t).
static Term kbo_eq_build_call(Term cs, Term ct) {
  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_KBO_EQ_IC));
  heap_set(l1 + 1, cs);
  Term step1 = term_new(0, TAG_APP, 0, l1);

  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, ct);
  return term_new(0, TAG_APP, 0, l2);
}

static Term prim_kbo_eq_ic(Term *args) {
  Term s = args[0];
  Term t = args[1];

  if (term_tag(s) != term_tag(t)) return term_new(0, TAG_NUM, DT_INT32, 0);
  if (term_ext(s) != term_ext(t)) return term_new(0, TAG_NUM, DT_INT32, 0);

  switch (term_tag(s)) {
    case TAG_FVR:
      // Same ext means same FVR id; equality follows.
      return term_new(0, TAG_NUM, DT_INT32, 1);

    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return term_new(0, TAG_NUM, DT_INT32, 0);
      if (ns == 0) return term_new(0, TAG_NUM, DT_INT32, 1);

      // Build AND(c_0, AND(c_1, ..., c_{n-1})).  Right-fold so the
      // last child is the innermost; AND is right-strict so this
      // evaluates left-to-right from the reducer's perspective.
      Term chain = kbo_eq_build_call(term_ctr_at(s, ns - 1),
                                     term_ctr_at(t, ns - 1));
      for (u32 j = ns - 1; j > 0; j--) {
        u32 idx = j - 1;
        Term call_idx = kbo_eq_build_call(term_ctr_at(s, idx),
                                          term_ctr_at(t, idx));
        chain = term_new_and(call_idx, chain);
      }
      return chain;
    }

    default:
      return term_new(0, TAG_NUM, DT_INT32,
                      (term_val(s) == term_val(t)) ? 1 : 0);
  }
}

// 8.3b: IC-native rule dispatch primitive.  Takes `(lhs, rhs,
// target)`; runs `thvm_match` to bind LHS variables against the
// target; on success returns `thvm_subst_apply(rhs, &subst)` --
// the rewritten term -- on failure returns ERA.
//
// Equivalent to one step of the C-side `thvm_rewrite_step` at
// the top position.  Combined with APP-SUP fan-out (8.3c) it
// lets a SUP of partial-PRI rules dispatch in parallel.
static Term prim_rewrite_step(Term *args) {
  Term lhs    = args[0];
  Term rhs    = args[1];
  Term target = args[2];
  RewriteSubst subst = {{0}};
  if (!thvm_match(lhs, target, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_subst_apply(rhs, &subst);
}

// Idempotent: tests / saturation init both call this; the registry
// just overwrites with the same function pointer.
static void atp_register_primitives(void) {
  prim_register(ATP_PRIM_UNIFY_APPLY,  prim_unify_apply,  2);
  prim_register(ATP_PRIM_UNIFY_APPLY3, prim_unify_apply3, 3);
  prim_register(ATP_PRIM_KBO,          prim_kbo,          3);
  prim_register(ATP_PRIM_KBO_EQ_IC,    prim_kbo_eq_ic,    2);
  prim_register(ATP_PRIM_REWRITE_STEP, prim_rewrite_step, 3);
}

// 8.3e-ii: IC-routed top-only rewrite try.  For each rule, builds
// APP(APP(APP(PRI(REWRITE_STEP), lhs_i), rhs_i), t) and reduces
// via wnf.  Returns the rewritten term on first non-ERA result;
// sets *fired = 1.  Otherwise *fired = 0 and returns t.
//
// Mirrors the C-side `rewrite_try_top` (in `src/rewrite/_.c`)
// with the per-rule matching going through APP-PRI evaluation
// instead of direct `thvm_match` calls.
static Term atp_ic_rewrite_try_top(Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules, u8 *fired) {
  for (u32 i = 0; i < n_rules; i++) {
    u64 l1 = heap_alloc(2);
    heap_set(l1 + 0, term_new_pri(ATP_PRIM_REWRITE_STEP));
    heap_set(l1 + 1, lhs[i]);
    Term step1 = term_new(0, TAG_APP, 0, l1);

    u64 l2 = heap_alloc(2);
    heap_set(l2 + 0, step1);
    heap_set(l2 + 1, rhs[i]);
    Term step2 = term_new(0, TAG_APP, 0, l2);

    u64 l3 = heap_alloc(2);
    heap_set(l3 + 0, step2);
    heap_set(l3 + 1, t);
    Term step3 = term_new(0, TAG_APP, 0, l3);

    Term result = wnf(step3);
    if (term_tag(result) != TAG_ERA) {
      *fired = 1;
      return result;
    }
  }
  *fired = 0;
  return t;
}

// 8.3e-ii: IC-routed analog of `thvm_rewrite_step`.  Same outermost-
// leftmost strategy (try top, else descend into CTR children
// left-to-right); per-rule matching dispatches through APP-PRI
// evaluation via `prim_rewrite_step`.  Same outputs as the C path
// (parity-tested in `tests/test_atp.c`).
static Term atp_ic_rewrite_step(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules) {
  u8 fired = 0;
  Term r = atp_ic_rewrite_try_top(t, lhs, rhs, n_rules, &fired);
  if (fired) return r;

  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return t;
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      Term original = children[i];
      Term rewritten = atp_ic_rewrite_step(original, lhs, rhs, n_rules);
      if (!kbo_eq(rewritten, original)) {
        children[i] = rewritten;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }

  return t;
}

// 8.3e-ii: IC-routed rewrite normalization.  Iterates
// `atp_ic_rewrite_step` until fixpoint or step_cap exhausted -- same
// shape as `thvm_rewrite_normalize` but with the per-step
// matching routed through APP-PRI.
static Term atp_rewrite_normalize_ic(Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u32 step_cap) {
  for (u32 i = 0; i < step_cap; i++) {
    Term t2 = atp_ic_rewrite_step(t, lhs, rhs, n_rules);
    if (kbo_eq(t, t2)) return t;
    t = t2;
  }
  return t;
}

// 8.3e-i: AtpState-aware shim.  Dispatches between the C-direct
// and IC-routed normalize paths based on s->use_ic_rewrite.
// Replaces direct `thvm_rewrite_normalize` calls in
// AtpState-internal callers (saturation step, goal-check,
// interreduce, joinability/connectedness filters).
static Term atp_rewrite_normalize(AtpState *s, Term t,
                                  const Term *lhs, const Term *rhs,
                                  u32 n_rules, u32 step_cap) {
  if (s != NULL && s->use_ic_rewrite) {
    return atp_rewrite_normalize_ic(t, lhs, rhs, n_rules, step_cap);
  }
  return thvm_rewrite_normalize(t, lhs, rhs, n_rules, step_cap);
}

// Grow the rule arrays (lhs / rhs / r_trace) to hold at least
// `need` entries.  No-op when capacity already suffices.  Doubles
// from the current capacity so amortized push cost stays O(1).
// New r_trace slots are filled with ATP_TRACE_NONE so a manually
// written rule that bypasses orient_and_add still has a defined
// trace index.  Aborts the process on OOM (matches heap_alloc's
// fatal policy -- there is no meaningful recovery for the caller).
static void atp_ensure_rule_cap(AtpState *s, u32 need) {
  if (need <= s->r_cap) return;
  u32 cap = s->r_cap ? s->r_cap : ATP_INIT_RULES;
  while (cap < need) cap *= 2;
  Term *nl = (Term *)realloc(s->lhs,     cap * sizeof(Term));
  Term *nr = (Term *)realloc(s->rhs,     cap * sizeof(Term));
  u32  *nt = (u32  *)realloc(s->r_trace, cap * sizeof(u32));
  if (nl == NULL || nr == NULL || nt == NULL) {
    fprintf(stderr, "atp_ensure_rule_cap: realloc to %u rules failed\n",
            cap);
    exit(1);
  }
  s->lhs = nl; s->rhs = nr; s->r_trace = nt;
  for (u32 i = s->r_cap; i < cap; i++) s->r_trace[i] = ATP_TRACE_NONE;
  s->r_cap = cap;
}

// Grow the CP arrays (cp_lhs / cp_rhs / cp_trace / cp_pri / cp_seq)
// to hold at least `need` entries.  Same doubling discipline as
// atp_ensure_rule_cap.
static void atp_ensure_cp_cap(AtpState *s, u32 need) {
  if (need <= s->cp_cap) return;
  u32 cap = s->cp_cap ? s->cp_cap : ATP_INIT_CPS;
  while (cap < need) cap *= 2;
  Term *nl = (Term *)realloc(s->cp_lhs,   cap * sizeof(Term));
  Term *nr = (Term *)realloc(s->cp_rhs,   cap * sizeof(Term));
  u32  *nt = (u32  *)realloc(s->cp_trace, cap * sizeof(u32));
  u32  *np = (u32  *)realloc(s->cp_pri,   cap * sizeof(u32));
  u32  *nq = (u32  *)realloc(s->cp_seq,   cap * sizeof(u32));
  if (nl == NULL || nr == NULL || nt == NULL || np == NULL || nq == NULL) {
    fprintf(stderr, "atp_ensure_cp_cap: realloc to %u CPs failed\n", cap);
    exit(1);
  }
  s->cp_lhs = nl; s->cp_rhs = nr; s->cp_trace = nt;
  s->cp_pri = np; s->cp_seq = nq;
  for (u32 i = s->cp_cap; i < cap; i++) s->cp_trace[i] = ATP_TRACE_NONE;
  s->cp_cap = cap;
}

// 7c': push one CP onto the binary min-heap CP queue.  Defined
// below (after atp_cp_priority); forward-declared here so the
// earlier add_equation push site can call it.
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace);

// === 8a: IC-native CP-set graph (-DATP_CP_GRAPH) ====================
//
// Under the flag the CP queue is ALSO held as one shared Term:
// `cp_graph = CpSet[Cp[l0,r0], Cp[l1,r1], ...]`, the leaves in the
// same slot order as the cp_lhs[] / cp_rhs[] mirror arrays.  Every
// CP mutation (heap push, select pop, heap reorder, reheapify)
// rebuilds cp_graph from the arrays so the two stay in lockstep; a
// debug assertion checks decode(cp_graph) equals the mirror.  This
// is a pure representation swap -- selection, priority, and search
// are the unchanged milestone-7 array engine.  8b makes cp_graph
// the thing reductions act on.
#ifdef ATP_CP_GRAPH

// Encode one CP as a 2-child `Cp[lhs,rhs]` CTR leaf.
static Term atp_cp_encode_leaf(Term lhs, Term rhs) {
  Term children[2] = { lhs, rhs };
  return term_new_ctr(ATP_CP_LABEL, children, 2);
}

// Decode a `Cp[lhs,rhs]` leaf back into its two terms.  Returns 1
// on a well-formed leaf, 0 otherwise.
static int atp_cp_decode_leaf(Term leaf, Term *lhs_out, Term *rhs_out) {
  if (term_tag(leaf) != TAG_CTR) return 0;
  if (term_ext(leaf) != ATP_CP_LABEL) return 0;
  if (term_ctr_n(leaf) != 2) return 0;
  *lhs_out = term_ctr_at(leaf, 0);
  *rhs_out = term_ctr_at(leaf, 1);
  return 1;
}

// Rebuild s->cp_graph from the cp_lhs[] / cp_rhs[] mirror arrays.
// Called at the end of every CP mutation so the graph stays in
// lockstep.  The container is a fresh CTR each rebuild, but the
// Cp[] leaves carry the already-hash-consed lhs/rhs cells, so two
// CPs sharing a subterm still share its heap cells.
static void atp_cp_graph_rebuild(AtpState *s) {
  if (s == NULL) return;
  if (s->n_cps == 0) {
    s->cp_graph = term_new_ctr(ATP_CPSET_LABEL, NULL, 0);
    return;
  }
  Term *leaves = (Term *)malloc((size_t)s->n_cps * sizeof(Term));
  if (leaves == NULL) {
    fprintf(stderr, "atp_cp_graph_rebuild: malloc for %u leaves failed\n",
            s->n_cps);
    exit(1);
  }
  for (u32 i = 0; i < s->n_cps; i++) {
    leaves[i] = atp_cp_encode_leaf(s->cp_lhs[i], s->cp_rhs[i]);
  }
  s->cp_graph = term_new_ctr(ATP_CPSET_LABEL, leaves, s->n_cps);
  free(leaves);
}

// Debug assertion: decode(cp_graph) must equal the cp_lhs[] /
// cp_rhs[] mirror, slot for slot.  Run after every mutation so a
// drift between the two representations aborts immediately rather
// than corrupting a proof silently.
static void atp_cp_graph_assert(const AtpState *s) {
  if (s == NULL) return;
  assert(term_tag(s->cp_graph) == TAG_CTR
         && "cp_graph must be a CTR");
  assert(term_ext(s->cp_graph) == ATP_CPSET_LABEL
         && "cp_graph must carry the CpSet label");
  assert(term_ctr_n(s->cp_graph) == s->n_cps
         && "cp_graph leaf count must equal n_cps");
  for (u32 i = 0; i < s->n_cps; i++) {
    Term gl = 0, gr = 0;
    int ok = atp_cp_decode_leaf(term_ctr_at(s->cp_graph, i), &gl, &gr);
    assert(ok && "cp_graph child must be a well-formed Cp[lhs,rhs] leaf");
    assert(gl == s->cp_lhs[i]
           && "cp_graph leaf lhs must equal cp_lhs[] mirror");
    assert(gr == s->cp_rhs[i]
           && "cp_graph leaf rhs must equal cp_rhs[] mirror");
  }
}

// Maintain cp_graph after a CP mutation: rebuild + assert lockstep.
static void atp_cp_graph_sync(AtpState *s) {
  atp_cp_graph_rebuild(s);
  atp_cp_graph_assert(s);
}

// === 8b: shared whole-graph CP normalization ========================
//
// Today (8a) normalization is lazy: thvm_atp_step rewrites only the
// CP it pops.  8b adds atp_normalize_graph -- when a rule is oriented
// it normalizes EVERY CP term in cp_graph in ONE sweep that threads a
// single `input cell -> normal-form cell` memo across all CPs.
//
// thvm hash-conses every cell, so a subterm shared by k CPs is the
// SAME Term value in all k.  The memo is keyed by that Term value, so
// the shared subterm's normal form is computed once total instead of
// once per CP -- this cross-CP memo IS the optimal-sharing win.  Per
// the spec target, per-step normalization cost drops from
// O(n_cps * |term|) toward O(distinct redexes).
//
// This is a deliberate SEMANTIC change (not bit-identical to 8a):
// eagerly normalizing queued CPs changes which become trivially
// joined when.  The memoized normalizer is bottom-up + top-fixpoint
// (innermost then the existing outermost-leftmost thvm_rewrite_step
// loop) -- it reaches a normal form under R just as the lazy path
// does; for the confluent rule sets KB completion drives toward, the
// normal form is the same one.

// Open-addressing Term -> Term memo for one normalization sweep.
// 0 is not a valid Term, so a 0 key marks an empty slot.
typedef struct {
  Term *keys;
  Term *vals;
  u32   cap;     // power of two
  u32   count;
} AtpNormMemo;

static void atp_norm_memo_init(AtpNormMemo *m, u32 hint) {
  u32 cap = 64;
  while (cap < hint * 2u) cap *= 2u;
  m->keys  = (Term *)calloc(cap, sizeof(Term));
  m->vals  = (Term *)calloc(cap, sizeof(Term));
  m->cap   = cap;
  m->count = 0;
  if (m->keys == NULL || m->vals == NULL) {
    fprintf(stderr, "atp_norm_memo_init: calloc for %u slots failed\n", cap);
    exit(1);
  }
}

static void atp_norm_memo_free(AtpNormMemo *m) {
  free(m->keys);
  free(m->vals);
  m->keys = NULL;
  m->vals = NULL;
}

// 64-bit mix (splitmix64 finalizer) -- a Term is a packed u64, so
// hashing the whole word spreads tag/ext/val bits across the table.
static u64 atp_term_hash(Term t) {
  u64 x = (u64)t;
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27; x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

static void atp_norm_memo_grow(AtpNormMemo *m);

// Look up t; returns 1 and sets *out if present.
static int atp_norm_memo_get(const AtpNormMemo *m, Term t, Term *out) {
  u32 mask = m->cap - 1u;
  u32 i = (u32)atp_term_hash(t) & mask;
  for (;;) {
    Term k = m->keys[i];
    if (k == 0) return 0;
    if (k == t) { *out = m->vals[i]; return 1; }
    i = (i + 1u) & mask;
  }
}

static void atp_norm_memo_put(AtpNormMemo *m, Term t, Term v) {
  if ((m->count + 1u) * 4u >= m->cap * 3u) atp_norm_memo_grow(m);
  u32 mask = m->cap - 1u;
  u32 i = (u32)atp_term_hash(t) & mask;
  for (;;) {
    Term k = m->keys[i];
    if (k == 0) { m->keys[i] = t; m->vals[i] = v; m->count++; return; }
    if (k == t) { m->vals[i] = v; return; }
    i = (i + 1u) & mask;
  }
}

static void atp_norm_memo_grow(AtpNormMemo *m) {
  u32   old_cap  = m->cap;
  Term *old_keys = m->keys;
  Term *old_vals = m->vals;
  m->cap  *= 2u;
  m->keys  = (Term *)calloc(m->cap, sizeof(Term));
  m->vals  = (Term *)calloc(m->cap, sizeof(Term));
  m->count = 0;
  if (m->keys == NULL || m->vals == NULL) {
    fprintf(stderr, "atp_norm_memo_grow: calloc for %u slots failed\n", m->cap);
    exit(1);
  }
  for (u32 i = 0; i < old_cap; i++) {
    if (old_keys[i] != 0) atp_norm_memo_put(m, old_keys[i], old_vals[i]);
  }
  free(old_keys);
  free(old_vals);
}

#ifdef ATP_NORM_STATS
// 8b: instrumentation -- memo hits vs misses, summed over a run.  A
// hit means a subterm shared by an already-visited CP: that node's
// normal form was reused instead of recomputed.  hits/(hits+misses)
// is the optimal-sharing ratio.  g_atp_norm_secs accumulates the
// wall time spent inside atp_normalize_graph so the sweep cost can be
// reported as a fraction of total runtime.
#include <time.h>
static u64    g_atp_norm_hits   = 0;
static u64    g_atp_norm_misses = 0;
static double g_atp_norm_secs   = 0.0;
#endif

// Memoized normal form of t under (lhs, rhs).  Bottom-up: each child
// is normalized through the SAME memo first (so a shared subterm is
// done once), the children-normalized term is rebuilt, then the
// existing outermost-leftmost thvm_rewrite_step loop runs to fixpoint
// at the top.  The memo is keyed by the input cell so every CP that
// carries that cell reuses the result for free.
static Term atp_norm_memo(AtpNormMemo *m, Term t,
                          const Term *lhs, const Term *rhs,
                          u32 n_rules, u32 step_cap) {
  Term cached = 0;
  if (atp_norm_memo_get(m, t, &cached)) {
#ifdef ATP_NORM_STATS
    g_atp_norm_hits++;
#endif
    return cached;
  }
#ifdef ATP_NORM_STATS
  g_atp_norm_misses++;
#endif

  Term cur = t;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n <= REWRITE_MAX_ARITY) {
      Term children[REWRITE_MAX_ARITY];
      int changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term ci = term_ctr_at(t, i);
        Term ni = atp_norm_memo(m, ci, lhs, rhs, n_rules, step_cap);
        children[i] = ni;
        if (ni != ci) changed = 1;
      }
      if (changed) cur = term_new_ctr(term_ext(t), children, n);
    }
  }

  // Outermost-leftmost fixpoint at the top -- identical loop shape to
  // thvm_rewrite_normalize, just applied after the children settled.
  for (u32 i = 0; i < step_cap; i++) {
    Term t2 = thvm_rewrite_step(cur, lhs, rhs, n_rules);
    if (kbo_eq(cur, t2)) break;
    cur = t2;
  }

  atp_norm_memo_put(m, t, cur);
  return cur;
}

// 8b: simplify every CP term in cp_graph in one shared sweep.  Called
// from thvm_atp_step once a rule is oriented, with `added` = the
// just-oriented rule range.
//
// Why only the new rule(s), not full R: every queued CP was already
// normalized under R BEFORE this step -- old CPs were normalized by an
// earlier sweep, fresh CPs are trivial-join-filtered against full R in
// atp_push_cps_traced.  Orienting one rule changes R by exactly that
// rule, so the queue only needs that rule (the unfailing fallback may
// add two) applied to reach normal form under R-new.  Re-running all
// of R against all n_cps CPs every step is the O(n_cps*|term|*n_rules)
// trap; restricting to the new rules makes the per-step sweep
// O(n_cps*|term|*n_new), n_new <= 2.
//
// The shared memo is the optimal-sharing win WITHIN the sweep: a
// subterm common to many CPs is the SAME hash-consed Term, so its
// rewrite under the new rule is computed once and reused across every
// CP carrying it -- cost is O(distinct subterm cells), not
// O(sum of CP sizes).  A CP untouched by the new rule memo-resolves to
// itself after one traversal and stays put.
//
// Trivially-joined CPs (both sides converge under the new rule) drop
// out here -- 8c will route that through an Eql[x,x] -> ERA
// reflexivity rule during the sweep; for 8b the kbo_eq check does the
// same pruning.  Updates the cp_lhs[]/cp_rhs[] mirror, then reheapify
// recomputes priorities and rebuilds cp_graph + the heap.
static void atp_normalize_graph(AtpState *s, AtpAddedRange added) {
  if (s == NULL || s->n_cps == 0 || added.count == 0) return;
  const u32 NORM_CAP = 64;
#ifdef ATP_NORM_STATS
  clock_t g_t0 = clock();
#endif

  // The newly-oriented rules.  added.first/.count index s->lhs/s->rhs;
  // copy by value so a later compaction can't move them under us.
  u32 n_new = added.count;
  if (n_new > 2) n_new = 2;
  Term new_lhs[2], new_rhs[2];
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
  }

  AtpNormMemo memo;
  atp_norm_memo_init(&memo, s->n_cps * 4u);

  u32 w = 0;
  int touched = 0;   // any CP term rewritten or dropped this sweep?
  for (u32 i = 0; i < s->n_cps; i++) {
    Term l = atp_norm_memo(&memo, s->cp_lhs[i], new_lhs, new_rhs,
                           n_new, NORM_CAP);
    Term r = atp_norm_memo(&memo, s->cp_rhs[i], new_lhs, new_rhs,
                           n_new, NORM_CAP);
    // Trivially-joined: both sides converged.  Drop the CP -- it adds
    // no equational consequence.
    if (kbo_eq(l, r)) {
      s->n_cps_dropped_joinable++;
      touched = 1;
      continue;
    }
    if (l != s->cp_lhs[i] || r != s->cp_rhs[i]) touched = 1;
    s->cp_lhs[w]   = l;
    s->cp_rhs[w]   = r;
    s->cp_trace[w] = s->cp_trace[i];
    w++;
  }
  s->n_cps = w;

  atp_norm_memo_free(&memo);

  // The common case: the new rule(s) reduced no queued CP -- the queue
  // was already in normal form under R-new (newly-pushed CPs are
  // trivial-join-filtered against full R in atp_push_cps_traced, old
  // CPs were swept under every earlier rule).  Then cp_pri / heap
  // order / cp_graph are all still valid; skip the O(n_cps) rebuild.
  // Only when a CP actually changed or dropped does reheapify run --
  // it recomputes cp_pri/cp_seq and rebuilds cp_graph from the mirror.
  if (touched) {
    thvm_atp_cp_reheapify(s);
  }
#ifdef ATP_NORM_STATS
  g_atp_norm_secs += (double)(clock() - g_t0) / CLOCKS_PER_SEC;
#endif
}

#ifdef ATP_NORM_STATS
// 8b instrumentation accessor: total memoized-normalize node visits
// that hit vs missed the shared memo, summed over the run.  Let a
// bench print the optimal-sharing ratio.
fn void thvm_atp_norm_stats(u64 *hits, u64 *misses, double *secs) {
  if (hits   != NULL) *hits   = g_atp_norm_hits;
  if (misses != NULL) *misses = g_atp_norm_misses;
  if (secs   != NULL) *secs   = g_atp_norm_secs;
}
#endif

// === 8e: shared-traversal multi-match (the 91%-killer) ==============
//
// Milestone 7 located the wall: atp_cp_queue_subsumed scans EVERY
// queued CP (~64k) calling thvm_match per leaf -- 7b measured ~16M
// match calls / step, 91% of runtime.  The scan asks, for a freshly
// generated candidate CP (lhs, rhs): is there a queued CP (qs, qt)
// and a substitution sigma with (lhs, rhs) = (sigma qs, sigma qt)?
// The QUEUED CP is the PATTERN (it carries the FVR variables); the
// CANDIDATE is the SUBJECT.  This is FORWARD subsumption -- the new
// candidate is subsumed by an existing queued CP and dropped before
// it ever reaches the queue.
//
// 8e routes that scan through ONE thvm_match_multi traversal of
// cp_graph instead of an explicit O(n_cps) loop.  cp_graph is the
// flat CTR `CpSet[Cp[qs0,qt0], Cp[qs1,qt1], ...]`; the CpSet
// container is the fan-out point -- thvm_match_multi forks over its
// n leaves against the one shared (lhs, rhs) subject.
//
// THE THESIS-FAILING RESULT -- documented honestly, per the plan's
// "8e's payoff depends on CPs actually sharing subterms ... report
// it honestly" instruction.
//
// 8e's intended win was a (pattern_cell, subject_cell) -> match
// memo: a subterm shared by many CPs would be matched against a
// given subject subterm once.  That memo was BUILT, INSTRUMENTED
// (-DATP_MATCH_STATS) and MEASURED.  Verdict on the Wolfram axiom
// (cpl1): 0.0% memo hit rate -- 0 hits over 7.7M subterm-pair
// lookups, three independent measurements.  Diagnosis:
//
//  - thvm has a BUMP allocator, NOT a hash-cons table.  term_new_ctr
//    (src/term/new_ctr.c) always heap_allocs a fresh cell, so two
//    structurally-equal CTR subterms are the SAME Term value ONLY
//    when the literal same cell is reused.  The plan's premise
//    "thvm hash-conses every cell" is factually wrong.
//  - The memo key is the (pattern, subject) cell PAIR.  A hit needs
//    BOTH cells to recur together.  On the flat CpSet every CP leaf
//    has a DISTINCT top cell, so thvm_match fails fast at the leaf
//    root -- the discrimination happens at the root, before the
//    traversal ever descends to any deep subterm CPs might share.
//  - A discrimination tree shares term PREFIXES (the root-anchored
//    test).  A flat hash-consed DAG of disjoint-headed CPs shares
//    term SUBTERMS but no prefix.  The flat CpSet provides zero
//    prefix sharing for a root-anchored match -- so the fan-out
//    re-traverses the subject in full per leaf.  Real prefix
//    sharing needs the SUP fan-out CONTAINER of workstream 8f, not
//    the flat 8a container.
//
// The memo, with its per-node hash + probe + partial-subst
// bookkeeping, was pure overhead at 0% hits -- it ran cpl1 ~16x
// SLOWER than 8b.  Per the plan's "degrades gracefully to the
// per-CP scan -- no worse than today" requirement, 8e ships WITHOUT
// the memo: thvm_match_multi is the fan-out over plain thvm_match,
// which IS the per-CP scan -- behavior-identical and 8b-cost.  8e
// is the milestone go/no-go signal and the signal is: the
// flat-CpSet shared-traversal does not beat the scan; revisit at 8f
// with the SUP container that actually shares prefixes.

#ifdef ATP_MATCH_STATS
#include <time.h>
static u64    g_atp_mm_calls       = 0;  // thvm_match_multi invocations
static u64    g_atp_mm_node_visits = 0;  // CP leaves walked
static u64    g_atp_mm_memo_hits   = 0;  // structurally 0 -- no memo
static u64    g_atp_mm_memo_miss   = 0;  // thvm_match calls issued
static double g_atp_mm_secs        = 0.0;
#endif

// 8e: forward subsumption of candidate (lhs, rhs) against the whole
// queued CP set in ONE traversal of cp_graph -- the replacement for
// atp_cp_queue_subsumed's explicit O(n_cps) thvm_match loop.
//
// `graph` is cp_graph: `CpSet[Cp[qs,qt], ...]`.  The CpSet container
// is the fan-out point: thvm_match_multi forks over its n Cp[] leaves
// against the one shared (lhs, rhs) subject.  Each leaf runs the same
// two-sided match the per-CP scan did -- forward (sigma qs = lhs AND
// sigma qt = rhs, one sigma threaded through both) then symmetric --
// via plain thvm_match.  Returns 1 on the first subsuming leaf.
//
// The verdict is IDENTICAL to the per-CP loop, leaf for leaf: 8e is
// a routing change, not a semantic one.  The (P,S) memo that was
// meant to share per-subterm work is absent BY MEASUREMENT, not
// oversight -- see the block comment above.
static u8 thvm_match_multi(Term graph, Term lhs, Term rhs) {
#ifdef ATP_MATCH_STATS
  g_atp_mm_calls++;
#endif
  if (term_tag(graph) != TAG_CTR) return 0;
  if (term_ext(graph) != ATP_CPSET_LABEL) return 0;
  u32 n = term_ctr_n(graph);
  for (u32 k = 0; k < n; k++) {
#ifdef ATP_MATCH_STATS
    g_atp_mm_node_visits++;
#endif
    Term qs = 0, qt = 0;
    if (!atp_cp_decode_leaf(term_ctr_at(graph, k), &qs, &qt)) continue;
    // Forward: sigma qs = lhs AND sigma qt = rhs, one sigma threaded
    // through both matches (equational subsumption).
    {
      RewriteSubst subst = {{0}};
#ifdef ATP_MATCH_STATS
      g_atp_mm_memo_miss += 2u;
#endif
      if (thvm_match(qs, lhs, &subst) &&
          thvm_match(qt, rhs, &subst)) {
        return 1;
      }
    }
    // Symmetric: sigma qs = rhs AND sigma qt = lhs.
    {
      RewriteSubst subst = {{0}};
#ifdef ATP_MATCH_STATS
      g_atp_mm_memo_miss += 2u;
#endif
      if (thvm_match(qs, rhs, &subst) &&
          thvm_match(qt, lhs, &subst)) {
        return 1;
      }
    }
  }
  return 0;
}

#ifdef ATP_MATCH_STATS
// 8e instrumentation accessor: thvm_match_multi calls, CP leaves
// walked, and thvm_match calls issued.  memo_hits is structurally 0
// -- 8e ships memo-free (the (P,S) memo measured 0% on the flat
// CpSet; see the 8e block comment).  Kept so the bench can report
// the scan size and confirm thvm_match is still the hot spot.
fn void thvm_atp_match_stats(u64 *calls, u64 *node_visits,
                             u64 *memo_hits, u64 *memo_miss,
                             double *secs) {
  if (calls       != NULL) *calls       = g_atp_mm_calls;
  if (node_visits != NULL) *node_visits = g_atp_mm_node_visits;
  if (memo_hits   != NULL) *memo_hits   = g_atp_mm_memo_hits;
  if (memo_miss   != NULL) *memo_miss   = g_atp_mm_memo_miss;
  if (secs        != NULL) *secs        = g_atp_mm_secs;
}
#endif

#else  // !ATP_CP_GRAPH -- the milestone-7 array engine, byte-for-byte.

// No-op so mutation sites carry one unconditional call site instead
// of #ifdef'd blocks; the compiler elides it with the flag off.
static inline void atp_cp_graph_sync(AtpState *s) { (void)s; }

#endif // ATP_CP_GRAPH

// === 7d: CP-queue subsumption index (-DATP_FV_INDEX) ===============
//
// THE WALL.  7b profiling pinned ~91% of completion runtime on
// `thvm_match`, every call under `atp_push_cps_traced` ->
// `atp_cp_queue_subsumed`.  That function asks, for each freshly
// generated candidate CP (lhs, rhs): is there a queued CP (qs, qt)
// and a substitution sigma with (lhs, rhs) = (sigma qs, sigma qt)
// (forward) or = (sigma qt, sigma qs) (symmetric)?  The milestone-7
// engine answers it with a flat O(n_cps) loop -- on the deep Wolfram
// axiom, n_cps climbs to ~64k and the loop issues ~16M recursive
// `thvm_match` calls per step.  The query almost always answers "no"
// (over a 200-step cpl1 run only ONE CP is ever queue-subsumed), so
// the cost is entirely in proving the negative -- ruling out every
// queued CP.
//
// Milestone 8 bet thvm's structural sharing would let the CP set act
// as a free discrimination tree.  8e REFUTED that: thvm has a bump
// allocator (src/heap/alloc.c), NOT hash-consing -- a fresh
// subsumption query shares no cells with stored CPs, and the match is
// root-anchored, so the flat-CpSet shared traversal is exactly the
// per-CP scan.  7d is the proven fix every serious completion prover
// uses: a real term index.
//
// WHY A DISCRIMINATION TREE, NOT A FEATURE VECTOR.  7d was first
// built as a feature-vector (FV) index -- the structure the plan
// recommended -- on cheap monotone integer features (symbol count,
// per-depth CTR profile, term depth) where a more-general term is
// componentwise <=.  It was sound, GC-trivial, and MEASURED: on the
// single-symbol Wolfram nand axiom it plateaued at ~47% false-
// positive survival (18.8k of 40.2k queued CPs surviving the filter
// per query) and adding depth-profile features did not move it.  The
// reason is structural: a CP whose one side is a bare variable -- a
// large fraction of the queue -- has the size profile of its other
// side alone, so its FV dominates almost every larger CP's FV.  A
// size-based FV simply cannot exclude a small term that "could
// generalize" a large one by shape but does not.
//
// Excluding by SHAPE needs a position-keyed symbol test, and that is
// exactly a discrimination tree.  The plan permitted the deviation
// "with a strong reason, justified against the GC-stability point".
// The reason: the measured FV plateau.  The GC point still holds --
// the 8b worry was MOVING CELL POINTERS in the index.  This tree is
// keyed entirely on integer LABEL ids (a CTR's label, or a wildcard
// marker for a variable); label ids are not heap addresses and do
// not move under the Cheney collector.  The only Term-valued storage
// is each leaf record's (lhs, rhs) mirror, rooted in
// thvm_atp_gc_collect.  So the index is as GC-trivial as the FV trie
// was -- the flag is still spelled -DATP_FV_INDEX.
//
// THE STRUCTURE -- a PERFECT discrimination tree.  The tree spans
// the PREORDER traversal of the CP viewed as one synthetic term
// `Cp(lhs, rhs)` (a binary node `Cp` so one tree covers both sides).
// A plain discrimination tree treats every variable as one wildcard
// `*`; that was MEASURED and plateaued at ~47% retrieval, because
// the deep nand-trees have many REPEATED variables and a one-`*`
// tree cannot tell `nand(x,x)` from `nand(x,y)`.  This is the
// PERFECT variant: it numbers a pattern's variables by first-
// appearance order, so `nand(x,x)` flattens to `nand *0 *0` and
// `nand(x,y)` to `nand *0 *1`.  Each tree edge is keyed on a flat
// symbol:
//   ATP_DT_NUM             a TAG_NUM atom
//   ATP_DT_STAR_BASE + k   the k-th DISTINCT pattern variable
//   ATP_DT_CTR_BASE  + lab a TAG_CTR with label `lab`
//
// INSERT renumbers the stored CP's variables (first occurrence of a
// var -> the next free k) and walks the renumbered preorder string,
// descending / creating an edge per symbol; the node reached after
// the whole string gets a leaf record.
//
// RETRIEVAL ("find every stored pattern that one-way MATCHES subject
// T") flattens T to a preorder subterm array and walks it in
// lockstep with the tree, carrying a binding array
// `star_bind[k] -> subject subterm`.  At the tree node for the
// current subject subterm `t`:
//   - a CTR edge equal to `t`'s own label -- follow it; `t`'s
//     children are the next preorder positions.
//   - a STAR_BASE+k edge -- the k-th pattern variable:
//       * k unbound  -> bind star_bind[k] := t, descend past t's
//                       whole subtree, unbind on backtrack;
//       * k bound    -> follow only if kbo_eq(star_bind[k], t)
//                       (the variable's earlier occurrence pinned
//                       a value; a repeat must equal it), descend
//                       past t's subtree.
// Every other edge is pruned.  This folds full one-way matching --
// structure AND variable consistency -- into the descent: a stored
// CP reaches a leaf IFF it matches T.  The leaf still runs the SAME
// two-sided thvm_match for byte-identical verdicts (and as a guard),
// but it now essentially always confirms.
//
// SOUNDNESS (never misses a subsumer).  thvm_match(pattern, subject)
// succeeds iff at every preorder position the pattern has a CTR
// equal to the subject's there, or a variable whose every occurrence
// binds a kbo_eq subterm.  The descent above follows exactly the
// edge that case takes -- CTR-equal, first-var-bind, or repeat-var-
// kbo_eq -- so it reaches a subsuming CP's leaf and never prunes it.
// The symmetric orientation is covered by a second retrieval over
// `Cp(rhs,lhs)`.  The tree's verdict is therefore identical to the
// array scan, CP for CP: same drops, same proof, same step/CP
// counts.

#ifdef ATP_FV_INDEX

// Flat-symbol alphabet for a perfect-discrimination-tree edge.
// Ordering matters: NUM < every STAR(k) < every CTR(lab), so the
// sym-ascending child list lets a descent stop scanning early.
#define ATP_DT_NUM        0u                 // TAG_NUM atom
#define ATP_DT_MAXVARS    64u                // distinct vars per CP
#define ATP_DT_STAR_BASE  1u                 // STAR(k) = BASE + k
#define ATP_DT_CTR_BASE   (ATP_DT_STAR_BASE + ATP_DT_MAXVARS)
#define ATP_DT_NIL        0xFFFFFFFFu

// Preorder-flattened subject cap.  A CP on the Wolfram axiom stays
// well under this; an over-deep term aborts retrieval to the full
// scan (atp_dt_query_orient) -- never a silent under-retrieval.
#define ATP_DT_FLAT_CAP   4096u

// Per-term variable renumbering: maps a raw TAG_FVR id to its
// first-appearance index 0,1,2,...  `slot[id]` holds (index+1), 0 =
// not yet seen.  Reset per CP at insert and per orientation at
// retrieval.
typedef struct { u32 slot[REWRITE_MAX_VAR]; u32 n; } AtpDtVarMap;

static void atp_dt_varmap_reset(AtpDtVarMap *vm) {
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) vm->slot[i] = 0;
  vm->n = 0;
}

// First-appearance index of variable id `vid` (assigns the next free
// index on first sight).  Ids >= REWRITE_MAX_VAR (which thvm_match
// itself refuses to bind) fold onto the last slot -- still sound:
// such a CP could never subsume anyway, and folding only makes the
// tree COARSER, never drops a real candidate.
static u32 atp_dt_var_index(AtpDtVarMap *vm, u32 vid) {
  if (vid >= REWRITE_MAX_VAR) vid = REWRITE_MAX_VAR - 1u;
  if (vm->slot[vid] == 0) {
    u32 idx = (vm->n < ATP_DT_MAXVARS) ? vm->n : (ATP_DT_MAXVARS - 1u);
    vm->slot[vid] = idx + 1u;
    if (vm->n < ATP_DT_MAXVARS) vm->n++;
  }
  return vm->slot[vid] - 1u;
}

// flatsym of one term node under variable renumbering `vm`.
static u32 atp_dt_flatsym(Term t, AtpDtVarMap *vm) {
  switch (term_tag(t)) {
    case TAG_CTR: return ATP_DT_CTR_BASE + term_ext(t);
    case TAG_NUM: return ATP_DT_NUM;
    case TAG_FVR:
    default:      return ATP_DT_STAR_BASE + atp_dt_var_index(vm, term_ext(t));
  }
}

// A discrimination-tree node: a left-child / right-sibling tree in a
// flat realloc-grown pool addressed by u32 INDEX (no pointers, so a
// pool realloc never invalidates the structure).  `sym` is the flat
// symbol of the edge INTO this node.  `rec_head` is the head of this
// node's leaf record list.
typedef struct {
  u32 sym;        // flat symbol of the in-edge
  u32 child;      // first child node index, or ATP_DT_NIL
  u32 sibling;    // next sibling node index, or ATP_DT_NIL
  u32 rec_head;   // first record index, or ATP_DT_NIL
} AtpDtNode;

// One indexed CP.  `lhs`/`rhs` mirror cp_lhs[..]/cp_rhs[..]; they are
// GC roots so the collector keeps them current.  `live` is cleared
// when the CP is popped / dropped -- a dead record is skipped by
// retrieval, reclaimed by the next index rebuild.  `seq` is the CP's
// stable id (the seq->record map key).  `next` links the leaf list.
typedef struct {
  Term lhs;
  Term rhs;
  u32  seq;
  u32  next;
  u8   live;
} AtpDtRec;

// seq -> record-index open-addressing hash entry (NIL = empty).
typedef struct { u32 seq; u32 rec; } AtpDtSeqEnt;

// The index.  Named `struct AtpFvIndex` -- the flag and the opaque
// thvm.h forward declaration are spelled that way; the structure
// inside is the discrimination tree the measurement settled on.
struct AtpFvIndex {
  AtpDtNode   *nodes;
  u32          n_nodes, cap_nodes;
  AtpDtRec    *recs;
  u32          n_recs, cap_recs;        // n_recs == GC-rooted span
  u32          n_live;                  // live record count (== n_cps)
  AtpDtSeqEnt *seqmap;                   // seq -> rec index
  u32          seqmap_cap;               // power of two
  u32          root;                     // tree root node index
  // Instrumentation (cheap counters, always compiled).
  u64 q_calls;            // atp_cp_queue_subsumed queries
  u64 q_candidates;       // leaf records reached by retrieval
  u64 q_matchcalls;       // thvm_match calls issued on candidates
  u64 q_nodevisits;       // discrimination-tree nodes touched
};
typedef struct AtpFvIndex AtpFvIndex;

static u32 atp_dt_node_new(AtpFvIndex *ix, u32 sym) {
  if (ix->n_nodes == ix->cap_nodes) {
    u32 cap = ix->cap_nodes ? ix->cap_nodes * 2u : 1024u;
    AtpDtNode *p = (AtpDtNode *)realloc(ix->nodes, cap * sizeof(AtpDtNode));
    if (p == NULL) { fprintf(stderr, "atp_dt: node pool OOM\n"); exit(1); }
    ix->nodes = p;
    ix->cap_nodes = cap;
  }
  u32 i = ix->n_nodes++;
  ix->nodes[i].sym      = sym;
  ix->nodes[i].child    = ATP_DT_NIL;
  ix->nodes[i].sibling  = ATP_DT_NIL;
  ix->nodes[i].rec_head = ATP_DT_NIL;
  return i;
}

static u32 atp_dt_rec_new(AtpFvIndex *ix) {
  if (ix->n_recs == ix->cap_recs) {
    u32 cap = ix->cap_recs ? ix->cap_recs * 2u : 1024u;
    AtpDtRec *p = (AtpDtRec *)realloc(ix->recs, cap * sizeof(AtpDtRec));
    if (p == NULL) { fprintf(stderr, "atp_dt: rec pool OOM\n"); exit(1); }
    ix->recs = p;
    ix->cap_recs = cap;
  }
  return ix->n_recs++;
}

// Find `parent`'s child reached by edge `sym`, creating it absent.
// Children kept in ascending-sym order -- deterministic across runs.
static u32 atp_dt_child(AtpFvIndex *ix, u32 parent, u32 sym) {
  u32 prev = ATP_DT_NIL;
  u32 cur  = ix->nodes[parent].child;
  while (cur != ATP_DT_NIL && ix->nodes[cur].sym < sym) {
    prev = cur;
    cur  = ix->nodes[cur].sibling;
  }
  if (cur != ATP_DT_NIL && ix->nodes[cur].sym == sym) return cur;
  u32 nn = atp_dt_node_new(ix, sym);          // may realloc the pool
  ix->nodes[nn].sibling = cur;
  if (prev == ATP_DT_NIL) ix->nodes[parent].child = nn;
  else                    ix->nodes[prev].sibling = nn;
  return nn;
}

// --- seq -> record map (open addressing, linear probe) -------------

static void atp_dt_seqmap_init(AtpFvIndex *ix, u32 cap) {
  ix->seqmap_cap = cap;
  ix->seqmap = (AtpDtSeqEnt *)malloc(cap * sizeof(AtpDtSeqEnt));
  if (ix->seqmap == NULL) { fprintf(stderr, "atp_dt: seqmap OOM\n"); exit(1); }
  for (u32 i = 0; i < cap; i++) ix->seqmap[i].seq = ATP_DT_NIL;
}

static void atp_dt_seqmap_put(AtpFvIndex *ix, u32 seq, u32 rec);

static void atp_dt_seqmap_grow(AtpFvIndex *ix) {
  u32 old_cap = ix->seqmap_cap;
  AtpDtSeqEnt *old = ix->seqmap;
  atp_dt_seqmap_init(ix, old_cap * 2u);
  for (u32 i = 0; i < old_cap; i++) {
    if (old[i].seq != ATP_DT_NIL) atp_dt_seqmap_put(ix, old[i].seq, old[i].rec);
  }
  free(old);
}

static void atp_dt_seqmap_put(AtpFvIndex *ix, u32 seq, u32 rec) {
  if ((ix->n_live + 1u) * 2u > ix->seqmap_cap) atp_dt_seqmap_grow(ix);
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DT_NIL) h = (h + 1u) & mask;
  ix->seqmap[h].seq = seq;
  ix->seqmap[h].rec = rec;
}

static u32 atp_dt_seqmap_get(const AtpFvIndex *ix, u32 seq) {
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DT_NIL) {
    if (ix->seqmap[h].seq == seq) return ix->seqmap[h].rec;
    h = (h + 1u) & mask;
  }
  return ATP_DT_NIL;
}

// Delete `seq` (Knuth back-shift, keeps probe chains intact).
static void atp_dt_seqmap_del(AtpFvIndex *ix, u32 seq) {
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DT_NIL && ix->seqmap[h].seq != seq) {
    h = (h + 1u) & mask;
  }
  if (ix->seqmap[h].seq == ATP_DT_NIL) return;
  u32 j = h;
  for (;;) {
    ix->seqmap[h].seq = ATP_DT_NIL;
    u32 k;
    do {
      j = (j + 1u) & mask;
      if (ix->seqmap[j].seq == ATP_DT_NIL) return;
      k = (ix->seqmap[j].seq * 2654435761u) & mask;
    } while ((h <= j) ? (h < k && k <= j) : (h < k || k <= j));
    ix->seqmap[h] = ix->seqmap[j];
    h = j;
  }
}

// --- index lifecycle -----------------------------------------------

static AtpFvIndex *atp_fv_index_new(void) {
  AtpFvIndex *ix = (AtpFvIndex *)calloc(1, sizeof(AtpFvIndex));
  if (ix == NULL) { fprintf(stderr, "atp_dt: index OOM\n"); exit(1); }
  atp_dt_seqmap_init(ix, 1024u);
  ix->root = atp_dt_node_new(ix, ATP_DT_NIL);  // root edge unused
  return ix;
}

static void atp_fv_index_free(AtpFvIndex *ix) {
  if (ix == NULL) return;
  free(ix->nodes);
  free(ix->recs);
  free(ix->seqmap);
  free(ix);
}

// --- insert --------------------------------------------------------
//
// Walk term `t` in preorder, descending the tree by flatsym per
// node, creating edges as needed.  `vm` renumbers variables by
// first appearance.  Returns the node reached after `t`'s whole
// preorder string.  A TAG_CTR's children extend the string in
// left-to-right order; a TAG_FVR / TAG_NUM is one symbol.
static u32 atp_dt_insert_term(AtpFvIndex *ix, u32 node, Term t,
                              AtpDtVarMap *vm) {
  node = atp_dt_child(ix, node, atp_dt_flatsym(t, vm));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      node = atp_dt_insert_term(ix, node, term_ctr_at(t, i), vm);
    }
  }
  return node;
}

// Insert CP (lhs, rhs) with stable id `seq`.  The CP is indexed as
// the synthetic term `Cp(lhs, rhs)` so a single tree spans both
// sides; ATP_CP_LABEL is the `Cp` head used elsewhere in the engine.
// One variable renumbering spans the WHOLE CP -- a variable shared
// between lhs and rhs (the common case for an equation) keeps one
// star index across both sides.
static void atp_fv_index_insert(AtpFvIndex *ix, Term lhs, Term rhs, u32 seq) {
  AtpDtVarMap vm;
  atp_dt_varmap_reset(&vm);
  u32 node = ix->root;
  node = atp_dt_child(ix, node, ATP_DT_CTR_BASE + ATP_CP_LABEL);  // Cp head
  node = atp_dt_insert_term(ix, node, lhs, &vm);
  node = atp_dt_insert_term(ix, node, rhs, &vm);
  u32 rec = atp_dt_rec_new(ix);
  ix->recs[rec].lhs  = lhs;
  ix->recs[rec].rhs  = rhs;
  ix->recs[rec].seq  = seq;
  ix->recs[rec].live = 1u;
  ix->recs[rec].next = ix->nodes[node].rec_head;
  ix->nodes[node].rec_head = rec;
  atp_dt_seqmap_put(ix, seq, rec);
  ix->n_live++;
}

// Drop CP `seq`: clear the record's live flag, unhook from the seq
// map.  The dead record stays in its leaf list (cheap); a rebuild
// reclaims the pool slot.
static void atp_fv_index_remove(AtpFvIndex *ix, u32 seq) {
  u32 rec = atp_dt_seqmap_get(ix, seq);
  if (rec == ATP_DT_NIL) return;
  if (ix->recs[rec].live) {
    ix->recs[rec].live = 0u;
    ix->n_live--;
  }
  atp_dt_seqmap_del(ix, seq);
}

// Discard every record / node and rebuild the tree from the live CP
// arrays.  Used when a wholesale CP-set mutation (reheapify after an
// atp_normalize_graph compaction; a test populating the arrays
// directly) reshuffles seqs out from under the incremental path.
static void atp_fv_index_rebuild(AtpState *s) {
  AtpFvIndex *ix = s->fv_index;
  if (ix == NULL) return;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->n_live  = 0;
  ix->root    = atp_dt_node_new(ix, ATP_DT_NIL);
  for (u32 i = 0; i < ix->seqmap_cap; i++) ix->seqmap[i].seq = ATP_DT_NIL;
  for (u32 i = 0; i < s->n_cps; i++) {
    atp_fv_index_insert(ix, s->cp_lhs[i], s->cp_rhs[i], s->cp_seq[i]);
  }
}

// --- retrieval -----------------------------------------------------

// Preorder-flatten `t` into `flat[]` from index `*pos`, recording
// per position the SUBTREE SIZE (preorder-position span) in
// `subsz[]`.  Returns 1 on success, 0 if the cap is hit (caller
// falls back to the full scan -- never silently under-retrieves).
static u8 atp_dt_flatten(Term t, Term *flat, u32 *subsz, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flat[here] = t;
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!atp_dt_flatten(term_ctr_at(t, i), flat, subsz, cap, pos)) return 0;
    }
  }
  subsz[here] = *pos - here;          // positions covered by t's subtree
  return 1;
}

// One retrieval's immutable parameters, threaded through the
// recursion without a wide signature.  The saturation engine is
// single-threaded (one AtpState per run), so file-static query
// scratch is safe and keeps the hot descent lean.  `g_atp_dt_star`
// is the descent's per-path binding array: star_bind[k] is the
// subject subterm the k-th pattern variable was first bound to (0 =
// unbound on the current path).
static AtpFvIndex *g_atp_dt_ix      = NULL;
static const Term *g_atp_dt_flat    = NULL;
static const u32  *g_atp_dt_subsz   = NULL;
static u32         g_atp_dt_flatlen = 0;
static Term        g_atp_dt_qlhs    = 0;
static Term        g_atp_dt_qrhs    = 0;
static Term        g_atp_dt_star[ATP_DT_MAXVARS];

// Confirm with the SAME two-sided thvm_match the array scan ran, for
// the records on leaf `node`, against query (q_lhs, q_rhs) in the
// orientation the descent encoded.  The perfect-tree descent already
// proved structure + variable consistency, so this essentially
// always succeeds -- it is kept as the authoritative byte-identity
// guard.  Returns 1 on a real subsumer.
static u8 atp_dt_leaf_match(u32 node) {
  AtpFvIndex *ix = g_atp_dt_ix;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DT_NIL;
       r = ix->recs[r].next) {
    AtpDtRec *rc = &ix->recs[r];
    if (!rc->live) continue;
    ix->q_candidates++;
    RewriteSubst subst = {{0}};
    ix->q_matchcalls += 2u;
    if (thvm_match(rc->lhs, g_atp_dt_qlhs, &subst) &&
        thvm_match(rc->rhs, g_atp_dt_qrhs, &subst)) {
      return 1;
    }
  }
  return 0;
}

// Walk the flattened subject from preorder index `pos` in lockstep
// with tree node `node`, threading the per-path variable bindings in
// g_atp_dt_star.  Returns 1 as soon as a reachable leaf yields a
// genuine subsumer.
static u8 atp_dt_descend(u32 node, u32 pos) {
  g_atp_dt_ix->q_nodevisits++;
  if (pos == g_atp_dt_flatlen) {
    // Whole subject consumed -- this node's records are leaves.
    return atp_dt_leaf_match(node);
  }
  AtpFvIndex *ix = g_atp_dt_ix;
  Term t   = g_atp_dt_flat[pos];
  u32  sz  = g_atp_dt_subsz[pos];        // preorder span of t's subtree
  u8   tc  = (term_tag(t) == TAG_CTR);
  u32  csym_exact = tc ? (ATP_DT_CTR_BASE + term_ext(t))
                       : (term_tag(t) == TAG_NUM ? ATP_DT_NUM : 0xFFFFFFFFu);
  for (u32 c = ix->nodes[node].child; c != ATP_DT_NIL;
       c = ix->nodes[c].sibling) {
    u32 csym = ix->nodes[c].sym;
    if (csym >= ATP_DT_STAR_BASE && csym < ATP_DT_CTR_BASE) {
      // Stored variable, the (csym-STAR_BASE)-th distinct pattern
      // var.  One-way match: the FIRST occurrence binds it to t; a
      // REPEAT occurrence applies only if t equals that binding.
      u32 k = csym - ATP_DT_STAR_BASE;
      Term bound = g_atp_dt_star[k];
      if (bound == 0) {
        g_atp_dt_star[k] = t;                 // first occurrence: bind
        u8 hit = atp_dt_descend(c, pos + sz);
        g_atp_dt_star[k] = 0;                 // unbind on backtrack
        if (hit) return 1;
      } else if (kbo_eq(bound, t)) {
        if (atp_dt_descend(c, pos + sz)) return 1;
      }
    } else if (csym == csym_exact) {
      // Stored CTR/NUM equal to t's own symbol -- consume t's head;
      // t's children are the next preorder positions.
      if (atp_dt_descend(c, pos + 1u)) return 1;
    }
  }
  return 0;
}

// Retrieve over one orientation: descend the tree for the synthetic
// subject `Cp(o_lhs, o_rhs)`.  Returns 1 if a queued CP subsumes the
// candidate in this orientation.
static u8 atp_dt_query_orient(AtpFvIndex *ix, Term o_lhs, Term o_rhs) {
  Term  flat_s[ATP_DT_FLAT_CAP];
  u32   subsz_s[ATP_DT_FLAT_CAP];
  Term *flat  = flat_s;
  u32  *subsz = subsz_s;
  // Reserve flat[0] for the synthetic `Cp` head: it spans the whole
  // subject, so subsz[0] = total positions.
  u32 pos = 1u;
  u8  ok  = atp_dt_flatten(o_lhs, flat, subsz, ATP_DT_FLAT_CAP, &pos)
         && atp_dt_flatten(o_rhs, flat, subsz, ATP_DT_FLAT_CAP, &pos);
  if (!ok) {
    // Cap hit -- fall back to a full scan so a deep CP can never be
    // silently under-retrieved (which would drop a real subsumer).
    for (u32 r = 0; r < ix->n_recs; r++) {
      if (!ix->recs[r].live) continue;
      ix->q_candidates++;
      RewriteSubst subst = {{0}};
      ix->q_matchcalls += 2u;
      if (thvm_match(ix->recs[r].lhs, o_lhs, &subst) &&
          thvm_match(ix->recs[r].rhs, o_rhs, &subst)) {
        return 1;
      }
    }
    return 0;
  }
  flat[0]  = 0;                           // Cp head: placeholder term
  subsz[0] = pos;                         // whole subject span
  g_atp_dt_ix      = ix;
  g_atp_dt_flat    = flat;
  g_atp_dt_subsz   = subsz;
  g_atp_dt_flatlen = pos;
  g_atp_dt_qlhs    = o_lhs;
  g_atp_dt_qrhs    = o_rhs;
  for (u32 i = 0; i < ATP_DT_MAXVARS; i++) g_atp_dt_star[i] = 0;
  // Descend from the root: its single real edge is the `Cp` head,
  // which lines up with flat[0] (also a Cp-head symbol).
  u32 cp_sym = ATP_DT_CTR_BASE + ATP_CP_LABEL;
  for (u32 c = ix->nodes[ix->root].child; c != ATP_DT_NIL;
       c = ix->nodes[c].sibling) {
    if (ix->nodes[c].sym == cp_sym) return atp_dt_descend(c, 1u);
  }
  return 0;
}

// 7d: forward subsumption of candidate (lhs, rhs) against the queued
// CP set via the discrimination tree.  Behaviour-identical to the
// array scan: the tree is a sound over-approximation, so any CP it
// surfaces is then confirmed by the SAME thvm_match the scan used,
// and any CP it prunes provably could not have matched.  Two
// orientations: forward Cp(lhs,rhs), symmetric Cp(rhs,lhs).
static u8 atp_fv_index_query(AtpFvIndex *ix, Term lhs, Term rhs) {
  ix->q_calls++;
  if (atp_dt_query_orient(ix, lhs, rhs)) return 1;
  if (atp_dt_query_orient(ix, rhs, lhs)) return 1;
  return 0;
}

// 7d instrumentation accessor: per-run retrieval stats so a bench can
// confirm the O(n_cps) scan collapsed.  `calls` is the number of
// atp_cp_queue_subsumed queries; `node_visits` the discrimination-
// tree nodes touched; `candidates` the leaf records reached; and
// `matchcalls` the thvm_match calls those candidates triggered.
// candidates / calls is the false-positive volume; the array scan
// would have touched n_cps records per query.
fn void thvm_atp_fv_stats(const AtpState *s, u64 *calls, u64 *node_visits,
                          u64 *candidates, u64 *matchcalls, u32 *nodes) {
  AtpFvIndex *ix = (s != NULL) ? s->fv_index : NULL;
  if (calls       != NULL) *calls       = (ix != NULL) ? ix->q_calls : 0;
  if (node_visits != NULL) *node_visits = (ix != NULL) ? ix->q_nodevisits : 0;
  if (candidates  != NULL) *candidates  = (ix != NULL) ? ix->q_candidates : 0;
  if (matchcalls  != NULL) *matchcalls  = (ix != NULL) ? ix->q_matchcalls : 0;
  if (nodes       != NULL) *nodes       = (ix != NULL) ? ix->n_nodes : 0;
}

#endif // ATP_FV_INDEX

fn AtpState *thvm_atp_init(const KboConfig *cfg, u32 step_cap) {
  AtpState *s = (AtpState *)calloc(1, sizeof(AtpState));
  if (s == NULL) return NULL;
  s->kbo      = cfg;
  s->step_cap = step_cap;
  atp_register_primitives();
  // Allocate the growable rule / CP arrays at their initial
  // capacity.  ensure_*_cap fills the trace slots with
  // ATP_TRACE_NONE (0 is a valid trace index, so explicit fill
  // is required); a fresh array starts with r_cap == 0 so the
  // helper treats the whole span as new.
  atp_ensure_rule_cap(s, ATP_INIT_RULES);
  atp_ensure_cp_cap(s, ATP_INIT_CPS);
#ifdef ATP_CP_GRAPH
  // 8a: start cp_graph as the empty CpSet[] -- n_cps is 0 here, so
  // the array mirror and the graph agree from the first instant.
  atp_cp_graph_sync(s);
#endif
#ifdef ATP_FV_INDEX
  // 7d: an empty FV subsumption index; CPs enter it on enqueue.
  s->fv_index = atp_fv_index_new();
#endif
  return s;
}

fn void thvm_atp_free(AtpState *s) {
  if (s == NULL) return;
  free(s->lhs);
  free(s->rhs);
  free(s->r_trace);
  free(s->cp_lhs);
  free(s->cp_rhs);
  free(s->cp_trace);
  free(s->cp_pri);
  free(s->cp_seq);
#ifdef ATP_FV_INDEX
  atp_fv_index_free(s->fv_index);
#endif
  free(s);
}

// === 9.3: heap checkpoint/reset =====================================
fn u64 thvm_atp_heap_checkpoint(void) {
  return HEAP_NEXT;
}

fn void thvm_atp_heap_reset(u64 checkpoint) {
  // Only allow popping back; never advance (callers should use
  // term_new_* for that).  Silent no-op on out-of-range to make
  // the API safe to sprinkle in step paths.
  if (checkpoint <= HEAP_NEXT) HEAP_NEXT = checkpoint;
}

// === 7a: in-loop GC for the saturation engine ======================
//
// thvm_atp_heap_checkpoint/reset only reclaims the per-step
// normalization scratch when a CP is trivially joined.  Every
// rule / CP / trace Term that survives a step is a heap-resident
// cell that lives forever, so a long completion run otherwise
// bumps HEAP_NEXT until heap_alloc reports "from-space exhausted".
//
// The fix: gather every live Term reachable from the AtpState into
// a root array and hand it to the Cheney collector (gc_collect).
// The collector evacuates each root to to-space, rewrites the root
// slot with the moved location, and swaps semi-spaces -- exactly
// the discipline `thvm_realize` uses for the WL session.  Writing
// the moved Terms back into the AtpState arrays keeps the engine
// pointing at the live copies.
//
// Live Term fields rooted here:
//   - lhs[0..n_rules), rhs[0..n_rules)        -- the rule set R
//   - cp_lhs[0..n_cps), cp_rhs[0..n_cps)      -- the CP queue
//   - goal_lhs, goal_rhs                      -- the conjecture
//   - trace[0..n_trace)                       -- TAG_CTR entries
//                                                (each holds lhs/rhs)
//   - witness_subst.bindings[0..REWRITE_MAX_VAR) -- narrowing σ
//
// Returns 1 if a collection ran, 0 if GC is disabled / no state.
fn u8 thvm_atp_gc_collect(AtpState *s) {
  if (s == NULL || !gc_enabled()) return 0;

  // Count the root slots so we can size the array exactly.
  u32 n_roots = 2u * s->n_rules + 2u * s->n_cps + 2u /* goal */
              + s->n_trace + REWRITE_MAX_VAR;
#ifdef ATP_CP_GRAPH
  // 8b: cp_graph is now a thing reductions act on (atp_normalize_graph
  // rewrites it), so its CTR cells must be relocated by the collector,
  // not rebuilt afterward.  One extra root slot for the CpSet term.
  n_roots += 1u;
#endif
#ifdef ATP_FV_INDEX
  // 7d: each FV-index record mirrors a queued CP's (lhs, rhs).  The
  // FV keys are ints (GC-invariant), but the mirror Terms move with
  // the heap -- root them so the collector relocates them in
  // lockstep with cp_lhs[]/cp_rhs[].  2 slots per record (live AND
  // dead -- a dead record's stale Term is harmless to relocate and
  // skipping it would need a live-count pre-pass).
  if (s->fv_index != NULL) n_roots += 2u * s->fv_index->n_recs;
#endif
  Term *roots = (Term *)malloc((size_t)n_roots * sizeof(Term));
  if (roots == NULL) return 0;

  u32 w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    roots[w++] = s->lhs[i];
    roots[w++] = s->rhs[i];
  }
  for (u32 i = 0; i < s->n_cps; i++) {
    roots[w++] = s->cp_lhs[i];
    roots[w++] = s->cp_rhs[i];
  }
  roots[w++] = s->goal_lhs;
  roots[w++] = s->goal_rhs;
  for (u32 i = 0; i < s->n_trace; i++) roots[w++] = s->trace[i];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    roots[w++] = s->witness_subst.bindings[i];
  }
#ifdef ATP_CP_GRAPH
  u32 cp_graph_root = w;
  roots[w++] = s->cp_graph;
#endif
#ifdef ATP_FV_INDEX
  u32 fv_rec_root = w;
  if (s->fv_index != NULL) {
    for (u32 i = 0; i < s->fv_index->n_recs; i++) {
      roots[w++] = s->fv_index->recs[i].lhs;
      roots[w++] = s->fv_index->recs[i].rhs;
    }
  }
#endif

  gc_collect(roots, w);

  // Write the relocated Terms back into the AtpState in the same
  // order they were gathered.
  w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    s->lhs[i] = roots[w++];
    s->rhs[i] = roots[w++];
  }
  for (u32 i = 0; i < s->n_cps; i++) {
    s->cp_lhs[i] = roots[w++];
    s->cp_rhs[i] = roots[w++];
  }
  s->goal_lhs = roots[w++];
  s->goal_rhs = roots[w++];
  for (u32 i = 0; i < s->n_trace; i++) s->trace[i] = roots[w++];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    s->witness_subst.bindings[i] = roots[w++];
  }
#ifdef ATP_CP_GRAPH
  // 8b: cp_graph was rooted, so the collector relocated its CpSet +
  // Cp[] cells in place.  Hash-consing means each leaf's lhs/rhs cell
  // is the SAME cell as the mirror's cp_lhs[i]/cp_rhs[i], which the
  // Cheney forwarding pointers relocate consistently.  Write the moved
  // CpSet back; a debug assertion confirms it still mirrors the arrays
  // -- no rebuild-everything pass.
  s->cp_graph = roots[cp_graph_root];
  atp_cp_graph_assert(s);
#endif
#ifdef ATP_FV_INDEX
  // 7d: write the relocated mirror Terms back into the FV records.
  // The FV keys / trie / seq map are pure ints -- a structural
  // feature is invariant under a Cheney copy -- so they need no
  // fixup: a relocated CP keeps its FV and stays in its trie slot.
  if (s->fv_index != NULL) {
    u32 rw = fv_rec_root;
    for (u32 i = 0; i < s->fv_index->n_recs; i++) {
      s->fv_index->recs[i].lhs = roots[rw++];
      s->fv_index->recs[i].rhs = roots[rw++];
    }
  }
#endif

  free(roots);

  return 1;
}

// Trigger threshold: collect when the from-space bump cursor has
// crossed this fraction of the live semi-space.  Half-full mirrors
// the default trigger in `thvm_realize` (realize.c).
static u8 atp_heap_under_pressure(void) {
  if (!gc_enabled()) return 0;
  u64 lo   = gc_from_start();
  u64 hi   = gc_from_end();
  u64 half = lo + (hi - lo) / 2;
  return HEAP_NEXT > half;
}

// Push a trace entry as a TAG_CTR with label = reason and children
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Returns the entry's
// index in s->trace, or ATP_TRACE_NONE if the buffer is full.
//
// 6.1b/c will wire this into add_equation / orient_and_add /
// generate_cps; for 6.1a the helper just exists, and the storage is
// init'd to zero by thvm_atp_init's calloc.
static u32 atp_trace_push(AtpState *s, u32 reason, u32 p_a, u32 p_b,
                          Term lhs, Term rhs) {
  if (s == NULL || s->n_trace >= ATP_MAX_TRACE) return ATP_TRACE_NONE;
  Term children[4] = {
    term_new(0, TAG_NUM, 0, p_a),
    term_new(0, TAG_NUM, 0, p_b),
    lhs,
    rhs,
  };
  s->trace[s->n_trace] = term_new_ctr(reason, children, 4);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Also records a TRACE_AXIOM
// entry so the proof trace (stage 6.1) can identify this CP's
// origin downstream.  The CP queue is growable, so this never
// rejects for being full; returns 1 on success, 0 only on NULL
// state or a sort-check rejection.
fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  // 8.4d: when a WaldSpec is attached, reject ill-sorted inputs
  // before mutating state.  Each side must be well-sorted AND
  // both sides must share the same sort (an equation l = r in
  // a sorted signature requires `sort(l) == sort(r)`).
  // Homogeneous-mode (NULL spec or n_sorts == 0) returns sort 0
  // from wald_term_sort unconditionally so the gate is a no-op.
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
  u32 trace_idx = atp_trace_push(s, TRACE_AXIOM,
                                 ATP_TRACE_NONE, ATP_TRACE_NONE,
                                 lhs, rhs);
  atp_cp_heap_push(s, lhs, rhs, trace_idx);
  return 1;
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
// Returns 1 on success, 0 if 8.4d's sort-check rejected the goal.
fn u8 thvm_atp_set_goal(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  // Clearing the goal: lhs == 0 means "completion mode", always
  // accepted regardless of sort-check.
  if (lhs == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    return 1;
  }
  // 8.4d: gate on sort-check when a spec is attached -- both
  // sides must be well-sorted AND share the same sort.
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
  return 1;
}

// 8.4d: attach a WaldSpec for sort-check gating.
fn void thvm_atp_set_spec(AtpState *s, const struct WaldSpec *spec) {
  if (s == NULL) return;
  s->spec = spec;
}

// 8.5c: attach an LpoConfig.  When non-NULL, orient_and_add
// dispatches to thvm_lpo instead of thvm_kbo.
fn void thvm_atp_set_lpo(AtpState *s, const LpoConfig *lpo) {
  if (s == NULL) return;
  s->lpo = lpo;
}

// 8.5c: order-aware compare.  Picks LPO (if attached) or KBO,
// returning a unified KboCmp-shaped result.  The two enums share
// numeric values (EQ=0, GT=1, LT=-1, UN=2), so the cast is safe.
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  if (s->lpo != NULL) {
    return (KboCmp)thvm_lpo(lhs, rhs, s->lpo);
  }
  return thvm_kbo(lhs, rhs, s->kbo);
}

// Total symbol count: TAG_FVR / atoms count as 1; TAG_CTR counts
// itself + the symbols of its children.  This is the "size" used
// by Waldmeister's `--add` heuristic in `ClasHeuristics.c`
// ("classification heuristics") -- the simplest CP-priority
// function: cheapest-by-size wins.
static u32 atp_symbol_count(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      u32 c = 1;
      for (u32 i = 0; i < n; i++) {
        c += atp_symbol_count(term_ctr_at(t, i));
      }
      return c;
    }
    default: return 1;   // FVR / atoms / NUM / etc.
  }
}

// 8.8: priority weight for a CP.  Default `--add` heuristic is
// the symbol-count sum.  When `s->use_mix_heuristic` is set, add
// a penalty for CPs that fail to orient cleanly (KBO_UN or
// KBO_EQ) -- mirrors Waldmeister's `--mix` heuristic in
// `ClasHeuristics.c`.  The penalty (`MIX_UNORIENTED_PENALTY`)
// is conservative; experiments may want to tune it.
#define MIX_UNORIENTED_PENALTY 4u
static u32 atp_cp_priority(AtpState *s, Term lhs, Term rhs) {
  u32 base = atp_symbol_count(lhs) + atp_symbol_count(rhs);
  if (s != NULL && s->use_mix_heuristic) {
    KboCmp c = atp_compare(s, lhs, rhs);
    if (c != KBO_GT && c != KBO_LT) {
      // KBO_EQ / KBO_UN -- penalize.
      base += MIX_UNORIENTED_PENALTY;
    }
  }
  return base;
}

// === 7c': CP-queue binary min-heap ==================================
//
// The CP queue (cp_lhs/cp_rhs/cp_trace/cp_pri/cp_seq) is kept as a
// binary min-heap ordered by (cp_pri, cp_seq): cheapest priority
// first, insertion order breaking ties.  This reproduces the old
// `--add` selection order (collapse_ordered sorted by INC depth,
// ties by queue index) but at O(log n) per push/pop instead of
// rebuilding an n-leaf INC-SUP tree + collapse on every step.

// Ordering predicate: does queue slot i sort strictly before j?
static int atp_cp_before(const AtpState *s, u32 i, u32 j) {
  if (s->cp_pri[i] != s->cp_pri[j]) return s->cp_pri[i] < s->cp_pri[j];
  return s->cp_seq[i] < s->cp_seq[j];
}

// Swap all five parallel CP arrays at slots i, j.
static void atp_cp_swap(AtpState *s, u32 i, u32 j) {
  Term tl = s->cp_lhs[i];   s->cp_lhs[i]   = s->cp_lhs[j];   s->cp_lhs[j]   = tl;
  Term tr = s->cp_rhs[i];   s->cp_rhs[i]   = s->cp_rhs[j];   s->cp_rhs[j]   = tr;
  u32  tt = s->cp_trace[i]; s->cp_trace[i] = s->cp_trace[j]; s->cp_trace[j] = tt;
  u32  tp = s->cp_pri[i];   s->cp_pri[i]   = s->cp_pri[j];   s->cp_pri[j]   = tp;
  u32  tq = s->cp_seq[i];   s->cp_seq[i]   = s->cp_seq[j];   s->cp_seq[j]   = tq;
}

static void atp_cp_sift_up(AtpState *s, u32 i) {
  while (i > 0) {
    u32 parent = (i - 1) / 2;
    if (!atp_cp_before(s, i, parent)) break;
    atp_cp_swap(s, i, parent);
    i = parent;
  }
}

static void atp_cp_sift_down(AtpState *s, u32 i) {
  for (;;) {
    u32 l = 2 * i + 1, r = 2 * i + 2, m = i;
    if (l < s->n_cps && atp_cp_before(s, l, m)) m = l;
    if (r < s->n_cps && atp_cp_before(s, r, m)) m = r;
    if (m == i) break;
    atp_cp_swap(s, i, m);
    i = m;
  }
}

// Push one CP onto the heap.  Computes its priority once (the cost
// the old select_cp paid n times per step) and sifts up.  O(log n).
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  s->cp_lhs[i]   = lhs;
  s->cp_rhs[i]   = rhs;
  s->cp_trace[i] = trace;
  s->cp_pri[i]   = atp_cp_priority(s, lhs, rhs);
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  s->n_cps++;
  atp_cp_sift_up(s, i);
  // 8a: keep cp_graph in lockstep with the array mirror.
  atp_cp_graph_sync(s);
#ifdef ATP_FV_INDEX
  // 7d: index the new CP under its (GC-stable) seq id.  The trie
  // keys on the FV, not the heap slot, so the sift-up above does not
  // touch the index.
  atp_fv_index_insert(s->fv_index, lhs, rhs, seq);
#endif
}

// Pop the cheapest CP from the queue (lowest (cp_pri, cp_seq) --
// the `--add` heuristic, ties by insertion order).
//
// 7c': the CP queue is a binary min-heap (atp_cp_heap_push keeps it
// so).  Selection is heap pop-min: take the root, move the last
// element into the root slot, sift down.  O(log n) per call --
// replacing the old per-step rebuild of an n-leaf INC-SUP tree +
// thvm_collapse_ordered, which was O(n) per step => O(n^2) over a
// run and the dominant cost past ~24 steps on hard problems.
//
// Returns 1 on success (out-params populated), 0 on empty queue.
fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL || s->n_cps == 0) return 0;
#ifdef ATP_CP_GRAPH
  // 8a: read the popped CP's terms from the IC-native cp_graph
  // (decode the slot-0 leaf), not the array.  The array is the
  // synced mirror -- atp_cp_graph_assert guarantees the decode
  // yields exactly cp_lhs[0] / cp_rhs[0], so this is bit-identical
  // to the array engine while exercising the graph read path.
  {
    Term gl = 0, gr = 0;
    int ok = atp_cp_decode_leaf(term_ctr_at(s->cp_graph, 0), &gl, &gr);
    assert(ok && "select_cp: cp_graph slot 0 must be a Cp[] leaf");
    *lhs_out = gl;
    *rhs_out = gr;
  }
#else
  *lhs_out = s->cp_lhs[0];
  *rhs_out = s->cp_rhs[0];
#endif
  s->last_popped_trace = s->cp_trace[0];
#ifdef ATP_FV_INDEX
  // 7d: the popped CP leaves the queue -- drop it from the index so a
  // later subsumption query never matches a stale, no-longer-queued
  // CP (which would diverge from the array-scan verdict).
  u32 popped_seq = s->cp_seq[0];
#endif
  s->n_cps--;
  if (s->n_cps > 0) {
    u32 last = s->n_cps;
    s->cp_lhs[0]   = s->cp_lhs[last];
    s->cp_rhs[0]   = s->cp_rhs[last];
    s->cp_trace[0] = s->cp_trace[last];
    s->cp_pri[0]   = s->cp_pri[last];
    s->cp_seq[0]   = s->cp_seq[last];
    atp_cp_sift_down(s, 0);
  }
#ifdef ATP_FV_INDEX
  atp_fv_index_remove(s->fv_index, popped_seq);
#endif
  // 8a: the pop shrank / reordered the mirror -- resync cp_graph.
  atp_cp_graph_sync(s);
  return 1;
}

// 7c': re-establish the CP-queue heap invariant over cp_lhs[0..n_cps)
// / cp_rhs[0..n_cps).  The normal path keeps the queue a heap via
// atp_cp_heap_push, but a caller (chiefly tests) that populates
// cp_lhs/cp_rhs/n_cps directly must call this so cp_pri / cp_seq are
// filled and the array satisfies the heap order before select / peek.
fn void thvm_atp_cp_reheapify(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  atp_ensure_cp_cap(s, s->n_cps);
  for (u32 i = 0; i < s->n_cps; i++) {
    s->cp_pri[i] = atp_cp_priority(s, s->cp_lhs[i], s->cp_rhs[i]);
    s->cp_seq[i] = s->cp_seq_next++;
  }
  // Floyd build-heap: sift down every internal node, last to first.
  for (u32 i = s->n_cps / 2; i > 0; ) {
    i--;
    atp_cp_sift_down(s, i);
  }
  // 8a: a caller populated the arrays directly -- resync cp_graph.
  atp_cp_graph_sync(s);
#ifdef ATP_FV_INDEX
  // 7d: reheapify reassigned every cp_seq[] (the index's stable key)
  // and a normalize-graph compaction may have dropped CPs.  The
  // incremental insert/remove path can no longer track the set, so
  // rebuild the index wholesale from the live CP arrays.
  atp_fv_index_rebuild(s);
#endif
}

// Push one rule onto R; the rule array is growable, so this always
// succeeds (returns 1) unless the state pointer is NULL.
static u8 atp_push_rule(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  atp_ensure_rule_cap(s, s->n_rules + 1);
  s->lhs[s->n_rules] = lhs;
  s->rhs[s->n_rules] = rhs;
  s->n_rules++;
  return 1;
}

// One full saturation step.  See docs/plans/saturation_loop.md
// sec.2 for the algorithm.  Order:
//   1. goal_check   -- cheap; may close if a prior step proved
//   2. step_cap     -- TIMEOUT if exceeded
//   3. select_cp    -- QUEUE_EMPTY if exhausted
//   4. normalize    -- both sides under current R (NORM_CAP = 64)
//   5. trivialize   -- skip if sides become kbo_eq
//   6. orient + add -- KBO + unfailing fallback
//   7. interreduce  -- drop subsumed older rules
//   8. generate_cps -- (new x R) + (old x new), adjusted for
//                      dropped old rules
//   9. goal_check   -- may close after new rule(s) integrated
//  10. step++       -- only on a "real" step that didn't close
//
// Returns one of: ATP_RUNNING (continue), ATP_PROVED (goal hit),
// ATP_TIMEOUT (step cap), ATP_QUEUE_EMPTY (saturation reached
// without proving the goal).
fn AtpStatus thvm_atp_step(AtpState *s) {
  if (s == NULL) return ATP_QUEUE_EMPTY;

  AtpStatus goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  if (s->step >= s->step_cap) return ATP_TIMEOUT;

  // 7a: in-loop GC.  When the dyn heap has crossed the half-full
  // mark, run a Cheney collection BEFORE allocating this step's
  // normalization / CP-enumeration scratch.  Done here -- before
  // select_cp pops -- so every live Term is still parked in the
  // AtpState arrays and gets rooted by thvm_atp_gc_collect.  A
  // long completion run otherwise exhausts from-space; with this
  // the heap floats around the live working set instead of
  // climbing monotonically.
  if (atp_heap_under_pressure()) {
    thvm_atp_gc_collect(s);
  }

  Term cp_lhs = 0, cp_rhs = 0;
  if (!thvm_atp_select_cp(s, &cp_lhs, &cp_rhs)) {
    return ATP_QUEUE_EMPTY;
  }

  // 9.3: snapshot the heap before the (potentially heavy) IC-routed
  // rewrite cells are allocated.  When the CP is trivially joined
  // (kbo_eq(l, r) below), neither l nor r is referenced downstream
  // and the entire normalize block is dead -- pop back.
  u64 hcp_norm = thvm_atp_heap_checkpoint();

  const u32 NORM_CAP = 64;
  Term l = atp_rewrite_normalize(s, cp_lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, cp_rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);

  if (kbo_eq(l, r)) {
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }

  u32 src_trace = s->last_popped_trace;
  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  if (added.count == 0) {
    // R full, or some other refusal.  Count the work and continue.
    s->step++;
    return ATP_RUNNING;
  }

  // Trace each newly-added rule with its source CP as parent_a.
  // For unfailing 2-way fallback both directions get separate
  // entries so PCL output can identify each rule individually.
  // Stash the trace index in r_trace[] so generate_cps can
  // record TRACE_CP parents for any CP born from this rule.
  for (u32 k = 0; k < added.count; k++) {
    Term rl = s->lhs[added.first + k];
    Term rr = s->rhs[added.first + k];
    u32  t  = atp_trace_push(s, TRACE_ORIENT, src_trace,
                             ATP_TRACE_NONE, rl, rr);
    s->r_trace[added.first + k] = t;
  }

  // Interreduce shifts new-rule indices down by `dropped`.
  u32 dropped = thvm_atp_interreduce(s, added);
  AtpAddedRange post = added;
  post.first = (dropped > post.first) ? 0 : (post.first - dropped);

  thvm_atp_generate_cps(s, post);

#ifdef ATP_CP_GRAPH
  // 8b: a rule was oriented (R changed), so every queued CP may now
  // simplify under it.  Sweep the WHOLE cp_graph once, applying the
  // newly-oriented rule(s) with a memo shared across all CPs -- a
  // subterm common to many CPs is rewritten once.  Trivially-joined
  // CPs collapse and drop out here.  The lazy per-CP normalize at the
  // top of the next step still runs (full R, catching any cascaded
  // redex); 8b makes the popped CP cheaper because it is already
  // simplified under every rule oriented since it was queued.
  atp_normalize_graph(s, post);
#endif

  goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  s->step++;
  return ATP_RUNNING;
}

// === stage 6.2: PCL-shaped trace serializer ===========================
//
// Walks the trace[] array and emits human-readable text in the shape
// of Waldmeister's PCL ("Proof Construction Language") output.  Each
// line:
//
//   <idx> (<reason> [from <p_a>[, <p_b>]]): <lhs> = <rhs>
//
// Term printer handles TAG_CTR (as "C<lab>(args...)"), TAG_FVR (as
// "x_<id>"), TAG_NUM (as "#<val>"), TAG_ERA (as "ERA"), with a
// "?T<tag>" fallback for any other tag.  Truncates silently on
// buffer overflow.

static u32 atp_pretty_term(Term t, char *buf, u32 cap);

static u32 atp_pretty_ctr(Term t, char *buf, u32 cap) {
  if (cap <= 1) return 0;
  u32 lab = term_ext(t);
  u32 n   = term_ctr_n(t);
  int n_w = snprintf(buf, cap, "C%u", lab);
  if (n_w < 0) return 0;
  u32 w = (u32)n_w;
  if (w >= cap) return cap - 1;
  if (n == 0) return w;
  if (w + 1 >= cap) return w;
  w += (u32)snprintf(buf + w, cap - w, "(");
  for (u32 i = 0; i < n; i++) {
    if (w + 2 >= cap) break;
    if (i > 0) w += (u32)snprintf(buf + w, cap - w, ", ");
    if (w >= cap) return cap - 1;
    w += atp_pretty_term(term_ctr_at(t, i), buf + w, cap - w);
    if (w >= cap) return cap - 1;
  }
  if (w + 1 < cap) w += (u32)snprintf(buf + w, cap - w, ")");
  return w;
}

static u32 atp_pretty_term(Term t, char *buf, u32 cap) {
  if (cap == 0) return 0;
  switch (term_tag(t)) {
    case TAG_FVR: return (u32)snprintf(buf, cap, "x_%u", term_ext(t));
    case TAG_NUM: return (u32)snprintf(buf, cap, "#%u", (u32)term_val(t));
    case TAG_ERA: return (u32)snprintf(buf, cap, "ERA");
    case TAG_CTR: return atp_pretty_ctr(t, buf, cap);
    default:      return (u32)snprintf(buf, cap, "?T%u", term_tag(t));
  }
}

fn u32 thvm_atp_trace_serialize(const AtpState *s, char *buf, u32 cap) {
  if (s == NULL || buf == NULL || cap == 0) return 0;
  buf[0] = '\0';
  u32 w = 0;
  for (u32 i = 0; i < s->n_trace; i++) {
    if (w + 1 >= cap) break;
    Term entry  = s->trace[i];
    u32  reason = term_ext(entry);
    u32  p_a    = (u32)term_val(term_ctr_at(entry, 0));
    u32  p_b    = (u32)term_val(term_ctr_at(entry, 1));
    Term lhs    = term_ctr_at(entry, 2);
    Term rhs    = term_ctr_at(entry, 3);

    const char *type_str = "?";
    switch (reason) {
      case TRACE_AXIOM:  type_str = "axiom";  break;
      case TRACE_ORIENT: type_str = "orient"; break;
      case TRACE_CP:     type_str = "cp";     break;
    }

    int n;
    if (p_a == ATP_TRACE_NONE) {
      n = snprintf(buf + w, cap - w, "%u (%s): ", i, type_str);
    } else if (p_b == ATP_TRACE_NONE) {
      n = snprintf(buf + w, cap - w, "%u (%s from %u): ", i, type_str, p_a);
    } else {
      n = snprintf(buf + w, cap - w, "%u (%s from %u, %u): ", i, type_str,
                   p_a, p_b);
    }
    if (n < 0) break;
    w += (u32)n;
    if (w + 1 >= cap) break;

    w += atp_pretty_term(lhs, buf + w, cap - w);
    if (w + 4 >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, " = ");

    w += atp_pretty_term(rhs, buf + w, cap - w);
    if (w + 1 >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, "\n");
  }
  if (w >= cap) w = cap - 1;
  buf[w] = '\0';
  return w;
}

// Drive thvm_atp_step until it returns a non-RUNNING status.
fn AtpStatus thvm_atp_run(AtpState *s) {
  AtpStatus st;
  do {
    st = thvm_atp_step(s);
  } while (st == ATP_RUNNING);
  return st;
}

// Goal check: normalize both sides of the conjecture under the
// current R; if they're now structurally equal, the goal is
// proved.  Returns ATP_PROVED on a hit, ATP_RUNNING otherwise.
// Skips cleanly (returns ATP_RUNNING) when no goal is set
// (goal_lhs == 0) -- the completion-mode case.
//
// Top-only rewriting today via thvm_rewrite_normalize; stage 5.4's
// recursive descent will widen coverage to sub-positions.
//
// Step cap NORM_CAP = 64 bounds the normalization (matches the
// ballpark used in tests/test_rewrite.c's headline demo); tune
// once we have benchmark data.
// 8.9c: per-iteration narrow budget.  Each goal_check call tries
// up to this many narrow_step iterations before giving up; the
// outer saturation loop calls goal_check again next iteration
// with the (potentially larger) rule set.
#define ATP_NARROW_BUDGET 8

fn AtpStatus thvm_atp_goal_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;
  const u32 NORM_CAP = 64;

  // 8.9c: existential goals use narrowing; universal goals stay
  // on the rewrite-and-compare path.
  if (s->goal_existential) {
    Term lhs = s->goal_lhs;
    Term rhs = s->goal_rhs;
    if (kbo_eq(lhs, rhs)) return ATP_PROVED;
    for (u32 i = 0; i < ATP_NARROW_BUDGET; i++) {
      Term new_lhs = 0, new_rhs = 0;
      u8 ok = thvm_atp_narrow_step(s, lhs, rhs,
                                   &new_lhs, &new_rhs,
                                   &s->witness_subst);
      if (!ok) return ATP_RUNNING;   // no more narrows; wait for new R
      lhs = new_lhs;
      rhs = new_rhs;
      if (kbo_eq(lhs, rhs)) {
        // Capture the converged terms back into the goal slots
        // so successive goal_check calls (after full saturation)
        // still report PROVED.
        s->goal_lhs = lhs;
        s->goal_rhs = rhs;
        return ATP_PROVED;
      }
    }
    // Budget exhausted; let the outer loop add more rules and retry.
    s->goal_lhs = lhs;
    s->goal_rhs = rhs;
    return ATP_RUNNING;
  }

  Term l = atp_rewrite_normalize(s, s->goal_lhs, s->lhs, s->rhs,
                                 s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, s->goal_rhs, s->lhs, s->rhs,
                                 s->n_rules, NORM_CAP);
  return kbo_eq(l, r) ? ATP_PROVED : ATP_RUNNING;
}

// 8.9c: set an existential conjecture.  Mirrors thvm_atp_set_goal
// but flips s->goal_existential so the narrow path runs in
// goal_check.
fn u8 thvm_atp_set_goal_existential(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  if (lhs == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    s->goal_existential = 0;
    return 1;
  }
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
  s->goal_existential = 1;
  return 1;
}

// Walk the older rules (indices [0, added.first)) and drop any
// whose LHS reduces under the freshly-added rule(s).  Each dropped
// rule's simplified equation goes back onto the CP queue so the
// saturation loop has a chance to re-orient it under the smaller R.
//
// Today this uses the top-position-only `thvm_rewrite_normalize`;
// stage 5.4's recursive-descent rewriter will automatically widen
// the coverage to sub-positions without changing this function.
//
// Returns the number of older rules that were dropped.
fn u32 thvm_atp_interreduce(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0 || added.first == 0) return 0;

  // Copy the new rules' Terms by value so we can safely compact the
  // R array beneath them.  Term is 64-bit; the heap cells they point
  // to don't move.
  Term new_lhs[2];
  Term new_rhs[2];
  u32  n_new = added.count;
  if (n_new > 2) n_new = 2;
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
  }

  u32 dropped = 0;
  u32 i       = 0;
  while (i < added.first - dropped) {
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    Term reduced = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs, n_new, 16);
    if (!kbo_eq(reduced, old_lhs)) {
      // The older rule's LHS simplified -- drop it and requeue
      // (reduced, old_rhs) for re-orientation.
      thvm_atp_add_equation(s, reduced, old_rhs);
      for (u32 j = i + 1; j < s->n_rules; j++) {
        s->lhs[j - 1]     = s->lhs[j];
        s->rhs[j - 1]     = s->rhs[j];
        s->r_trace[j - 1] = s->r_trace[j];
      }
      s->n_rules--;
      dropped++;
      // Don't increment i; the next older rule shifted down to slot i.
    } else {
      i++;
    }
  }
  return dropped;
}

// Generate fresh CPs from the freshly-added rules `added` against
// the current rule set R, push survivors onto the CP queue.
// Drops overflow silently (queue cap or temp-buffer cap).  Returns
// the number of CPs successfully pushed.
//
// To avoid recomputing CPs already in the queue, the enumeration
// is restricted to (new x all_R) + (old x new), where the old x
// old slice is exactly the work we already did before the add.
//
// Temp buffer sized for one batch; large rule sets may produce
// more CPs than fit and silently drop them (matches Waldmeister's
// drop-on-overflow policy in `KPVerwaltung.c` -- *Kritische-Paare-
// Verwaltung*, "critical-pair management").

#define ATP_CP_BATCH 1024

// Stage 7.1: trivial-joinability check.  Normalize both sides of a
// candidate CP under the current rule set R and compare; if they
// collapse to the same term, the CP is joinable-by-R and adds no
// new equational consequences -- it can be discarded.
//
// This is the Waldmeister `Grundzusammenfuehrung` ("ground-merging")
// criterion at its weakest, equivalent to Twee's "joinable-by-current-
// R" pruning.  Stronger variants (ground-joinability over a sample
// of substitutions, AC-aware joinability) are deferred to 7.2+.
//
// Cost: two `thvm_rewrite_normalize` calls per CP candidate.  Worth
// it when the saturation produces many joinable CPs (group axioms
// generate ~hundreds of trivially-joinable overlaps per round).
static u8 atp_cp_trivially_joinable(AtpState *s, Term lhs, Term rhs) {
  const u32 NORM_CAP = 64;
  Term l = atp_rewrite_normalize(s, lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  return kbo_eq(l, r);
}

// Stage 7.2b: source-rule-disjoint connectedness check.  Returns 1
// if (lhs, rhs) is joinable under R \ {rule_a, rule_b} -- the two
// rules that birthed this CP via overlap unification.  Bachmair-
// Dershowitz-Plaisted-style redundancy: if the join can be done
// without using either parent rule, the parent rules' interaction
// was redundant.
//
// Per the domination lemma in `docs/plans/connectedness_design.md`,
// this is strictly weaker pruning than 7.1's full-R joinability
// (since reducing the rule set cannot uncover joins that the full
// set misses).  We compute it for measurement: the resulting
// counter `n_cps_dropped_connected` is bounded above by
// `n_cps_dropped_joinable`, and the gap will become meaningful
// when AC matching or extended joinability lands in 7.4+.
//
// `rule_a`/`rule_b` are indices into `s->lhs[] / s->rhs[]`.  Pass
// ATP_RULE_NONE (or any value >= n_rules) to mean "no rule
// excluded" -- equivalent to running 7.1.  The filtered rule
// arrays are heap-allocated, sized to the live n_rules, since the
// rule set is unbounded.
static u8 atp_cp_source_disjoint_connected(AtpState *s, Term lhs, Term rhs,
                                           u32 rule_a, u32 rule_b) {
  const u32 NORM_CAP = 64;
  u32 n = s->n_rules;
  Term *filt_l = (n > 0) ? (Term *)malloc((size_t)n * sizeof(Term)) : NULL;
  Term *filt_r = (n > 0) ? (Term *)malloc((size_t)n * sizeof(Term)) : NULL;
  if (n > 0 && (filt_l == NULL || filt_r == NULL)) {
    free(filt_l); free(filt_r);
    return 0;
  }
  u32 n_filt = 0;
  for (u32 k = 0; k < n; k++) {
    if (k == rule_a || k == rule_b) continue;
    filt_l[n_filt] = s->lhs[k];
    filt_r[n_filt] = s->rhs[k];
    n_filt++;
  }
  Term l = atp_rewrite_normalize(s, lhs, filt_l, filt_r, n_filt, NORM_CAP);
  Term r = atp_rewrite_normalize(s, rhs, filt_l, filt_r, n_filt, NORM_CAP);
  free(filt_l);
  free(filt_r);
  return kbo_eq(l, r);
}

// Stage 7.3a: rule subsumption check.  Returns 1 if there exist a
// rule `(l_k, r_k) ∈ R` and a substitution σ such that
// `(lhs, rhs) = (σ l_k, σ r_k)` (forward) or `(lhs, rhs) =
// (σ r_k, σ l_k)` (symmetric).  Equational subsumption: the σ must
// be CONSISTENT across both sides simultaneously, so we extend the
// same `RewriteSubst` across the two `thvm_match` calls.
//
// Per the domination lemma in `docs/plans/connectedness_design.md`:
// if (lhs, rhs) is rule-subsumed by (l_k, r_k), then rule
// (l_k, r_k) rewrites lhs to rhs in one step under σ, so
// `thvm_rewrite_normalize` collapses the pair too.  Hence
// `n_cps_dropped_rule_subsumed <= n_cps_dropped_joinable` always.
// We tick the counter for empirical measurement; the filtering
// itself stays in 7.1.
//
// 7.3b will add queue subsumption -- which IS orthogonal to 7.1.
static u8 atp_cp_rule_subsumed(AtpState *s, Term lhs, Term rhs) {
  for (u32 k = 0; k < s->n_rules; k++) {
    // Forward: σl_k = lhs AND σr_k = rhs (one σ extended through
    // both matches).
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[k], lhs, &subst) &&
          thvm_match(s->rhs[k], rhs, &subst)) {
        return 1;
      }
    }
    // Symmetric: σl_k = rhs AND σr_k = lhs.
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[k], rhs, &subst) &&
          thvm_match(s->rhs[k], lhs, &subst)) {
        return 1;
      }
    }
  }
  return 0;
}

// Stage 7.3b: queue subsumption check.  Returns 1 if the candidate
// `(lhs, rhs)` is a substitution instance of some queued CP
// `(s->cp_lhs[k], s->cp_rhs[k])` -- i.e., there is σ such that
// `(lhs, rhs) = (σ cp_lhs[k], σ cp_rhs[k])` (or symmetric).
//
// Genuinely orthogonal to 7.1: the queue does not participate in
// `thvm_rewrite_normalize`, so a CP can be queue-subsumed without
// being trivially-joinable (and vice versa).  Used as a real
// filter: the candidate is dropped before reaching the queue.
//
// Cost: O(|queue| * |term|) per candidate; cheap relative to a
// `thvm_rewrite_normalize` because matching has no fixed-point
// loop.
//
// 8e: with -DATP_CP_GRAPH the explicit per-CP loop is replaced by
// ONE thvm_match_multi traversal of cp_graph (the CpSet container is
// the fan-out point).  The verdict is IDENTICAL to the loop, leaf
// for leaf -- 8e is a routing change, not a semantic one.  It is
// memo-free: the (pattern_cell, subject_cell) memo measured 0%
// sharing on the flat CpSet, so shipping it would be pure overhead
// (see the 8e block comment at thvm_match_multi).
//
// 7d: with -DATP_FV_INDEX the O(n_cps) scan is replaced by an
// FV-index retrieval -- componentwise-dominated FVs are pulled from
// the trie and the SAME two-sided thvm_match runs only on those.
// The FV filter is a sound over-approximation, so the verdict is
// IDENTICAL to the scan, CP for CP.  The FV path takes priority over
// the -DATP_CP_GRAPH thvm_match_multi traversal (they are the same
// verdict; FV is the faster one).
#if defined(ATP_FV_INDEX)
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_cps == 0) return 0;
  return atp_fv_index_query(s->fv_index, lhs, rhs);
}
#elif defined(ATP_CP_GRAPH)
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_cps == 0) return 0;
  return thvm_match_multi(s->cp_graph, lhs, rhs);
}
#else
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  for (u32 k = 0; k < s->n_cps; k++) {
    Term qs = s->cp_lhs[k];
    Term qt = s->cp_rhs[k];
    // Forward: σqs = lhs AND σqt = rhs (one σ).
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(qs, lhs, &subst) &&
          thvm_match(qt, rhs, &subst)) {
        return 1;
      }
    }
    // Symmetric.
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(qs, rhs, &subst) &&
          thvm_match(qt, lhs, &subst)) {
        return 1;
      }
    }
  }
  return 0;
}
#endif

// Helper: push a batch of CPs onto the queue with TRACE_CP entries
// pointing at the two source rules' trace indices.  Drops overflow
// silently.  Filters and counters fire on each CP:
//   - 7.1:  trivially-joinable under R         -> drop, tick `n_cps_dropped_joinable`
//   - 7.2b: source-rule-disjoint connected     -> tick `n_cps_dropped_connected`
//                                                 (counter only, -DATP_CP_DIAG)
//   - 7.3a: rule-subsumed by some `(l, r) ∈ R` -> tick `n_cps_dropped_rule_subsumed`
//                                                 (counter only, -DATP_CP_DIAG)
//   - 7.3b: queue-subsumed by some queued CP   -> drop, tick `n_cps_dropped_queue_subsumed`
//                                                 (real filter, orthogonal to 7.1)
// `rule_a`/`rule_b` are the rule indices that birthed this CP batch
// (passed through to the connectedness check); `parent_a`/`parent_b`
// are their trace indices.  Returns count of CPs pushed.
//
// 7e lever 1: the 7.2b connectedness and 7.3a rule-subsumption checks
// are COUNTER-ONLY -- their verdicts feed `n_cps_dropped_connected` /
// `n_cps_dropped_rule_subsumed` and never drop a CP (only `joinable`
// and `q_subsmd` `continue`).  Each is two full `atp_rewrite_normalize`
// passes (plus a malloc) / an O(n_rules) two-sided match scan -- about
// half the per-CP filter cost.  They run only under -DATP_CP_DIAG;
// the default hot loop skips them.  Skipping is behavior-identical:
// same CPs queued, same proof, identical step/rule/cps trajectory.
static u32 atp_push_cps_traced(AtpState *s, const CriticalPair *cps,
                               u32 ncps, u32 parent_a, u32 parent_b,
                               u32 rule_a, u32 rule_b) {
  u32 pushed = 0;
#if defined(ATP_CP_GRAPH) && defined(ATP_MATCH_STATS)
  clock_t mm_t0 = clock();
#endif
#ifndef ATP_CP_DIAG
  (void)rule_a;
  (void)rule_b;
#endif
  for (u32 i = 0; i < ncps; i++) {
    u8 joinable    = atp_cp_trivially_joinable(s, cps[i].lhs, cps[i].rhs);
    // 8e: under -DATP_CP_GRAPH this runs ONE thvm_match_multi
    // traversal of cp_graph; off the flag it is the array scan.
    u8 q_subsmd    = atp_cp_queue_subsumed(s, cps[i].lhs, cps[i].rhs);
#ifdef ATP_CP_DIAG
    if (atp_cp_source_disjoint_connected(s, cps[i].lhs, cps[i].rhs,
                                         rule_a, rule_b)) {
      s->n_cps_dropped_connected++;
    }
    if (atp_cp_rule_subsumed(s, cps[i].lhs, cps[i].rhs)) {
      s->n_cps_dropped_rule_subsumed++;
    }
#endif
    if (joinable) {
      s->n_cps_dropped_joinable++;
      continue;
    }
    if (q_subsmd) {
      s->n_cps_dropped_queue_subsumed++;
      continue;
    }
    u32 t = atp_trace_push(s, TRACE_CP, parent_a, parent_b,
                           cps[i].lhs, cps[i].rhs);
    atp_cp_heap_push(s, cps[i].lhs, cps[i].rhs, t);
    pushed++;
  }
#if defined(ATP_CP_GRAPH) && defined(ATP_MATCH_STATS)
  g_atp_mm_secs += (double)(clock() - mm_t0) / CLOCKS_PER_SEC;
#endif
  return pushed;
}

// 8.1e-i: C-direct critical-pair enumerator -- the path
// `thvm_atp_generate_cps` takes when `s->use_ic_cp_gen == 0`
// (the default).  Bulk of the work happens in
// `thvm_critical_pairs_range`; this function just plumbs the
// (i, j) iteration and trace bookkeeping.
static u32 thvm_atp_generate_cps_c(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;

  // (new x all_R): the new rule is i (outer), j ranges over all
  // existing rules (including the new ones for new x new self-overlap).
  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      u32 nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                           i, i + 1, j, j + 1,
                                           buf, ATP_CP_BATCH);
      pushed += atp_push_cps_traced(s, buf, nbuf,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
    }
  }

  // (old x new): old rule on the outside, new rule fed as inner.
  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      u32 nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                           i, i + 1, j, j + 1,
                                           buf, ATP_CP_BATCH);
      pushed += atp_push_cps_traced(s, buf, nbuf,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
    }
  }

  return pushed;
}

// 8.1e-ii: invoke `prim_unify_apply3` via APP-PRI evaluation.
// Builds the saturated chain APP(APP(APP(PRI(id), s), t), target)
// and reduces it via `wnf`.  Returns either σ(target) on success
// or ERA on unify failure -- the same shape as the underlying
// primitive returns.
static Term ic_unify_apply3(Term s, Term t, Term target) {
  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_UNIFY_APPLY3));
  heap_set(l1 + 1, s);
  Term step1 = term_new(0, TAG_APP, 0, l1);

  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, t);
  Term step2 = term_new(0, TAG_APP, 0, l2);

  u64 l3 = heap_alloc(2);
  heap_set(l3 + 0, step2);
  heap_set(l3 + 1, target);
  Term step3 = term_new(0, TAG_APP, 0, l3);

  return wnf(step3);
}

// 8.1e-ii: per-position visitor that mirrors `cp_visit` (in
// `src/cp/_.c`) but routes the unify+apply step through the
// TAG_PRI machinery via `ic_unify_apply3`.  Same outputs as the
// C path; the IC contribution is the per-position unify call
// going through APP-PRI evaluation.
typedef struct {
  Term         li, ri;
  Term         lj, rj;
  CriticalPair *out;
  u32           cap;
  u32           count;
} CpCtxIc;

static u32 cp_visit_ic(const u32 *p, u32 p_len, void *raw) {
  CpCtxIc *ctx = (CpCtxIc *)raw;
  if (ctx->count >= ctx->cap) return ctx->count;

  Term sub = cp_subterm_at(ctx->li, p, p_len);
  if (sub == 0) return ctx->count;
  if (term_tag(sub) == TAG_FVR) return ctx->count;

  // Build replaced = li[p ← rj] (still in C; `cp_replace_at` is
  // a small pure helper).
  Term replaced = cp_replace_at(ctx->li, p, p_len, ctx->rj);

  // IC-routed unify+apply.  Two PRI calls because
  // prim_unify_apply3 takes a single target; recomputing σ each
  // time is wasteful but correct.  8.1e-iii will measure the
  // overhead.
  Term cp_lhs = ic_unify_apply3(sub, ctx->lj, replaced);
  if (term_tag(cp_lhs) == TAG_ERA) return ctx->count;
  Term cp_rhs = ic_unify_apply3(sub, ctx->lj, ctx->ri);
  if (term_tag(cp_rhs) == TAG_ERA) return ctx->count;

  ctx->out[ctx->count].lhs = cp_lhs;
  ctx->out[ctx->count].rhs = cp_rhs;
  ctx->count++;
  return ctx->count;
}

// 8.1e-ii: IC-routed CP enumeration.  Same iteration pattern as
// the C path -- (i, j) cross-product over the new-vs-old rule
// rectangles -- but the per-position unify+apply flows through
// the TAG_PRI machinery via `cp_visit_ic` / `ic_unify_apply3`.
// Output CPs are structurally identical to the C path (verified
// by parity tests in `tests/test_atp.c`).
static u32 thvm_atp_generate_cps_ic(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;
  CpCtxIc ctx;
  u32 path[CP_MAX_DEPTH];

  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      ctx.li    = s->lhs[i];
      ctx.ri    = s->rhs[i];
      ctx.lj    = thvm_rename_vars(s->lhs[j], CP_RENAME_OFFSET);
      ctx.rj    = thvm_rename_vars(s->rhs[j], CP_RENAME_OFFSET);
      ctx.out   = buf;
      ctx.cap   = ATP_CP_BATCH;
      ctx.count = 0;
      (void)cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                              cp_visit_ic, &ctx, 0);
      pushed += atp_push_cps_traced(s, buf, ctx.count,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
    }
  }

  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      ctx.li    = s->lhs[i];
      ctx.ri    = s->rhs[i];
      ctx.lj    = thvm_rename_vars(s->lhs[j], CP_RENAME_OFFSET);
      ctx.rj    = thvm_rename_vars(s->rhs[j], CP_RENAME_OFFSET);
      ctx.out   = buf;
      ctx.cap   = ATP_CP_BATCH;
      ctx.count = 0;
      (void)cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                              cp_visit_ic, &ctx, 0);
      pushed += atp_push_cps_traced(s, buf, ctx.count,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
    }
  }

  return pushed;
}

fn u32 thvm_atp_generate_cps(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0) return 0;
  if (s->use_ic_cp_gen) return thvm_atp_generate_cps_ic(s, added);
  return thvm_atp_generate_cps_c(s, added);
}

// Orient via KBO and push the rule(s).  See header comment for the
// dispatch table.  Atomic: if the unfailing fallback can't fit both
// orientations, neither is added.
fn AtpAddedRange thvm_atp_orient_and_add(AtpState *s, Term lhs, Term rhs) {
  AtpAddedRange r = {0, 0};
  if (s == NULL) return r;

  // 8.5c: dispatch between KBO and LPO based on which config is
  // attached.  See `atp_compare`.
  KboCmp c = atp_compare(s, lhs, rhs);
  switch (c) {
    case KBO_GT: {
      u32 idx = s->n_rules;
      if (atp_push_rule(s, lhs, rhs)) { r.first = idx; r.count = 1; }
      return r;
    }
    case KBO_LT: {
      u32 idx = s->n_rules;
      if (atp_push_rule(s, rhs, lhs)) { r.first = idx; r.count = 1; }
      return r;
    }
    case KBO_UN: {
      // Unfailing fallback: reserve 2 slots up front so the pair is
      // added atomically (the array is growable, so this can't fail).
      atp_ensure_rule_cap(s, s->n_rules + 2);
      u32 idx = s->n_rules;
      atp_push_rule(s, lhs, rhs);
      atp_push_rule(s, rhs, lhs);
      r.first = idx;
      r.count = 2;
      return r;
    }
    case KBO_EQ:
    default:
      return r;
  }
}

// === 8.10b: top-K CP peek ==========================================
//
// Copies the top K cheapest CPs' (lhs, rhs) into the caller's
// buffers WITHOUT modifying the queue.  "Cheapest" is the same
// (cp_pri, cp_seq) order the heap pops in, so peek[0] always
// equals the next `thvm_atp_select_cp` result.
//
// Caller-side use case: see top-K candidates before deciding
// which (if any) to commit; useful for branching CP selectors,
// multi-CP batch heuristics, lookahead.

// (pri, seq, idx) triple sorted to realize the peek order.
typedef struct { u32 pri; u32 seq; u32 idx; } AtpPeekEnt;
static int atp_peek_cmp(const void *a, const void *b) {
  const AtpPeekEnt *x = (const AtpPeekEnt *)a;
  const AtpPeekEnt *y = (const AtpPeekEnt *)b;
  if (x->pri != y->pri) return (x->pri < y->pri) ? -1 : 1;
  if (x->seq != y->seq) return (x->seq < y->seq) ? -1 : 1;
  return 0;
}

fn u32 thvm_atp_peek_top_k(AtpState *s, u32 k,
                           Term *out_lhs, Term *out_rhs) {
  if (s == NULL || s->n_cps == 0) return 0;
  if (k > s->n_cps) k = s->n_cps;
  if (k == 0) return 0;

  // The heap array is ordered by the heap invariant, not fully
  // sorted -- so copy the (pri, seq, idx) triples and sort them
  // by the queue's selection key to read off the top K.
  AtpPeekEnt *ent = (AtpPeekEnt *)malloc((size_t)s->n_cps * sizeof(AtpPeekEnt));
  if (ent == NULL) return 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    ent[i].pri = s->cp_pri[i];
    ent[i].seq = s->cp_seq[i];
    ent[i].idx = i;
  }
  qsort(ent, s->n_cps, sizeof(AtpPeekEnt), atp_peek_cmp);
  for (u32 i = 0; i < k; i++) {
    out_lhs[i] = s->cp_lhs[ent[i].idx];
    out_rhs[i] = s->cp_rhs[ent[i].idx];
  }
  free(ent);
  return k;
}

// === 8.9b: narrowing primitives ====================================

// One-shot narrow visitor: tries each rule at the current position,
// commits the first successful unification.  `success` flag stops
// the walk after a hit.
typedef struct {
  AtpState     *s;
  Term          side;        // currently being narrowed
  Term          other;       // the other side (sigma is applied here too)
  Term         *out_side;
  Term         *out_other;
  RewriteSubst *witness;
  u8            success;
} NarrowCtx;

static u32 narrow_visit(const u32 *p, u32 p_len, void *raw) {
  NarrowCtx *ctx = (NarrowCtx *)raw;
  if (ctx->success) return 0;

  Term sub = cp_subterm_at(ctx->side, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;

  for (u32 k = 0; k < ctx->s->n_rules; k++) {
    Term lj = thvm_rename_vars(ctx->s->lhs[k], CP_RENAME_OFFSET);
    Term rj = thvm_rename_vars(ctx->s->rhs[k], CP_RENAME_OFFSET);
    RewriteSubst subst = {{0}};
    if (!thvm_unify(sub, lj, &subst)) continue;

    // Narrow: replace the subterm at p with the rule's RHS,
    // then sigma-apply across both sides.
    Term replaced  = cp_replace_at(ctx->side, p, p_len, rj);
    *ctx->out_side  = thvm_unify_apply(replaced, &subst);
    *ctx->out_other = thvm_unify_apply(ctx->other, &subst);

    // Accumulate sigma into witness.  No composition step
    // needed: previous-step sigmas have already been applied
    // to side/other before this call, so each new binding lives
    // in the post-sigma universe.
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
      if (subst.bindings[i] != 0) {
        ctx->witness->bindings[i] = subst.bindings[i];
      }
    }
    ctx->success = 1;
    return 0;
  }
  return 0;
}

fn u8 thvm_atp_narrow_step(AtpState *s, Term lhs, Term rhs,
                           Term *out_lhs, Term *out_rhs,
                           RewriteSubst *witness) {
  if (s == NULL || s->n_rules == 0) return 0;
  if (out_lhs == NULL || out_rhs == NULL || witness == NULL) return 0;

  NarrowCtx ctx;
  ctx.s        = s;
  ctx.witness  = witness;
  ctx.success  = 0;

  u32 path[CP_MAX_DEPTH];

  // Try narrowing on lhs first.
  ctx.side       = lhs;
  ctx.other      = rhs;
  ctx.out_side   = out_lhs;
  ctx.out_other  = out_rhs;
  cp_walk_positions(lhs, path, 0, CP_MAX_DEPTH, narrow_visit, &ctx, 0);
  if (ctx.success) return 1;

  // Then rhs.
  ctx.side       = rhs;
  ctx.other      = lhs;
  ctx.out_side   = out_rhs;
  ctx.out_other  = out_lhs;
  cp_walk_positions(rhs, path, 0, CP_MAX_DEPTH, narrow_visit, &ctx, 0);
  return ctx.success;
}

fn Term thvm_atp_get_witness(const AtpState *s, u32 var_id) {
  if (s == NULL || var_id >= REWRITE_MAX_VAR) return 0;
  return s->witness_subst.bindings[var_id];
}

// === Stage 9.1b: bounded DFS multi-witness narrowing ================
// Enumerates up to N witnesses by recursively trying every (position,
// rule) choice at each node.  Stateless w.r.t. AtpState; populates
// the caller's RewriteSubst array directly.

typedef struct {
  AtpState     *s;
  RewriteSubst *witnesses;
  u32           max_witnesses;
  u32           found;
  u32           max_depth;
} NarrowAllCtx;

typedef struct {
  NarrowAllCtx *ctx;
  Term          side;       // currently being narrowed
  Term          other;      // the other side (sigma is applied here too)
  RewriteSubst  acc;        // accumulator at this DFS frame
  u32           depth;
  u8            narrowing_lhs;  // 1 = narrow side==lhs; 0 = narrow side==rhs
} NarrowAllVisitor;

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc,
                           u32 depth);

static u32 narrow_all_visit(const u32 *p, u32 p_len, void *raw) {
  NarrowAllVisitor *v = (NarrowAllVisitor *)raw;
  if (v->ctx->found >= v->ctx->max_witnesses) return 0;

  Term sub = cp_subterm_at(v->side, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;

  for (u32 k = 0; k < v->ctx->s->n_rules; k++) {
    if (v->ctx->found >= v->ctx->max_witnesses) return 0;

    Term lj = thvm_rename_vars(v->ctx->s->lhs[k], CP_RENAME_OFFSET);
    Term rj = thvm_rename_vars(v->ctx->s->rhs[k], CP_RENAME_OFFSET);
    RewriteSubst subst = {{0}};
    if (!thvm_unify(sub, lj, &subst)) continue;

    Term replaced  = cp_replace_at(v->side, p, p_len, rj);
    Term new_side  = thvm_unify_apply(replaced,  &subst);
    Term new_other = thvm_unify_apply(v->other,  &subst);

    // Compose sigma into a fresh accumulator copy: each branch sees
    // its own bindings, siblings stay independent.
    RewriteSubst new_acc = v->acc;
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
      if (subst.bindings[i] != 0) new_acc.bindings[i] = subst.bindings[i];
    }

    if (v->narrowing_lhs)
      narrow_all_dfs(v->ctx, new_side, new_other, &new_acc, v->depth + 1);
    else
      narrow_all_dfs(v->ctx, new_other, new_side, &new_acc, v->depth + 1);
  }
  return 0;
}

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc,
                           u32 depth) {
  if (ctx->found >= ctx->max_witnesses) return;
  if (kbo_eq(lhs, rhs)) {
    ctx->witnesses[ctx->found++] = *acc;
    return;
  }
  if (depth >= ctx->max_depth) return;

  u32 path[CP_MAX_DEPTH];
  NarrowAllVisitor v;
  v.ctx   = ctx;
  v.acc   = *acc;
  v.depth = depth;

  // Narrow on lhs first.
  v.side          = lhs;
  v.other         = rhs;
  v.narrowing_lhs = 1;
  cp_walk_positions(lhs, path, 0, CP_MAX_DEPTH, narrow_all_visit, &v, 0);
  if (ctx->found >= ctx->max_witnesses) return;

  // Then rhs.
  v.side          = rhs;
  v.other         = lhs;
  v.narrowing_lhs = 0;
  cp_walk_positions(rhs, path, 0, CP_MAX_DEPTH, narrow_all_visit, &v, 0);
}

fn u32 thvm_atp_narrow_all(AtpState *s,
                           Term lhs, Term rhs,
                           u32 max_depth, u32 max_witnesses,
                           RewriteSubst *witnesses) {
  if (s == NULL || witnesses == NULL || max_witnesses == 0) return 0;

  NarrowAllCtx ctx;
  ctx.s             = s;
  ctx.witnesses     = witnesses;
  ctx.max_witnesses = max_witnesses;
  ctx.found         = 0;
  ctx.max_depth     = max_depth;

  RewriteSubst empty = {{0}};
  narrow_all_dfs(&ctx, lhs, rhs, &empty, 0);
  return ctx.found;
}
