// thvm_atp_* - saturation loop state (stage 5.1).
//
// Heap-allocated AtpState plus init / free / add_equation / set_goal
// helpers.  The actual saturation step (thvm_atp_step) lands in 5.2;
// the priority-aware CP selection in 5.3; the recursive-descent
// rewriter feeding step 4 of the algorithm in 5.4.  This file just
// gives the loop a place to live.
//
// See docs/atp/algorithms.md for the algorithm.

// Parallel AtpFt rule storage (docs/atp/engineering.md).
// Pull in the cell layout + dual-arena allocator + Stage-2 constructors
// /converters / hashes when THVM_ATPFT_RULES is on so atp_push_rule /
// thvm_atp_set_goal can populate s->lhs_ft[] / s->rhs_ft[] / goal_*_ft
// from inside this single TU.  Off the flag this whole block is gone,
// so the default build is byte-identical (no extra symbols, no extra
// allocations, no extra arenas).  THVM_ATPFT_ALLOC / _CONVERT are
// required transitively; the test target sets all three together.
#ifdef THVM_ATPFT_RULES
# if !defined(THVM_ATPFT_ALLOC) || !defined(THVM_ATPFT_CONVERT)
#  error "THVM_ATPFT_RULES requires THVM_ATPFT_ALLOC and THVM_ATPFT_CONVERT"
# endif
# include "ft.h"
# include "ft_alloc.c"
# include "ft.c"
# include "ft_order.c"   // thvm_kbo_ft / thvm_lpo_ft, used by ft_norm
#endif

// Stage 7 forward decls: the CP-queue helpers in ft_cpq.c are referenced
// from atp_ensure_cp_cap / atp_cp_heap_insert_packed / atp_cp_swap /
// select_cp / thvm_atp_cp_set / interreduce -- all defined BEFORE
// ft_cpq.c is included (it sits after ft_norm.c so it can use
// atp_rewrite_normalize_ft).  Hoist the prototypes here so the early
// callers compile cleanly under the flag.  Off the flag this block is
// gone and no symbol is referenced.
#ifdef THVM_ATPFT_CPQ
struct AtpFtCell;
static void atp_cp_ft_ensure_cap(AtpState *s, u32 need, u32 old_cap);
static u32  atp_cp_ft_set       (AtpState *s, u32 i, Term lhs, Term rhs,
                                 u32 priority_hint, u32 origin_rule);
static void atp_cp_ft_clear     (AtpState *s, u32 i);
static void atp_cp_ft_move      (AtpState *s, u32 dst, u32 src);
#endif

// Critical-goal queue (narrow-queue lane) forward decls: the
// implementation sits at the end of this file (it uses the narrowing
// primitives + atp_rewrite_normalize); goal_check / init / free /
// set_goal_existential reference it earlier.
struct AtpCgq;
static AtpStatus atp_cgq_check(AtpState *s);
static void      atp_cgq_free(struct AtpCgq *q);
struct AtpGoalDrive;
static AtpStatus atp_goal_drive_check(AtpState *s);
static void      atp_goal_drive_free(struct AtpGoalDrive *g);
// GC rooting: every stored critical goal holds two live Terms; the
// dedup hash is structural (content-keyed), so relocation needs no
// fixup beyond the write-back -- same discipline as the MNF nodes.
static u32       cgq_gc_count(struct AtpCgq *q);
static void      cgq_gc_gather(struct AtpCgq *q, Term *roots, u32 *w);
static void      cgq_gc_writeback(struct AtpCgq *q, const Term *roots,
                                  u32 base);

// Stage 6: AtpFt-native normalize fixpoint (push-norm joinable check).
// Requires Stage 4's parallel rule mirror (THVM_ATPFT_RULES) and
// Stage 5's matcher (THVM_ATPFT_MATCH).  Off the flag the splice +
// normalize TUs are not pulled in; atp_cp_trivially_joinable retains
// its Term-only behaviour byte-identically.
#ifdef THVM_ATPFT_NORM
# if !defined(THVM_ATPFT_RULES)
#  error "THVM_ATPFT_NORM requires THVM_ATPFT_RULES"
# endif
# ifndef THVM_ATPFT_MATCH
#  define THVM_ATPFT_MATCH 1
# endif
# include "ft_match.c"
# ifdef THVM_ATP_AC
#  include "ft_ac_match.c"
#  include "ft_ac_eq.c"
# endif
# include "ft_unify.c"   // span unifier: the CP-walk port consumes it
                          // directly; the BATCH-path rerouting stays
                          // gated on THVM_ATPFT_UNIFY (ft_cp.c below).
# ifdef THVM_ATPFT_UNIFY
#  include "ft_cp.c"
# endif
# include "ft_splice.c"
// ft_norm.c included LATER (after AtpRuleIndex / ATP_RI_* are defined
// further down this TU) -- ft_norm.c uses Stage 6b's extern hooks
// (atp_ri_find_redex_ft_pub, atp_ri_ft_sync) under THVM_ATPFT_RI.
#endif

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

// Phase-timing counters (microseconds).  Aggregates per-call wall time
// inside thvm_atp_step so a profiling bench (env THVM_ATP_PROFILE=1) can
// quantify the relative cost of normalize-at-pop, CP generation, push-
// time normalize, and interreduce.  Zero overhead when the env flag is
// off: a single static branch keeps the timer calls dead.
u64 g_atp_phase_us_pop_normalize  = 0;
u64 g_atp_phase_us_cp_gen         = 0;
u64 g_atp_phase_us_push_normalize = 0;
u64 g_atp_phase_us_interreduce    = 0;
u64 g_atp_phase_us_goal_check     = 0;
u64 g_atp_phase_us_cp_set_ir      = 0;
// Sub-phase breakdown of `atp_cp_set_interreduce`: unpack covers the
// pre-normalize per-CP work (acp_unpack + orphan check + rule_subsumed
// + bitmap probe), norm covers the two atp_rewrite_normalize calls,
// pack covers kbo_eq + acp_pack + slot-move, post covers the trailing
// reheapify + watermark advance.  All four sum to g_atp_phase_us_cp_set_ir
// up to a tiny untimed residual (the for-loop overhead + heap reset).
u64 g_atp_phase_us_cpir_unpack    = 0;
u64 g_atp_phase_us_cpir_normalize = 0;
u64 g_atp_phase_us_cpir_pack      = 0;
u64 g_atp_phase_us_cpir_post      = 0;
u64 g_atp_phase_us_total          = 0;
u64 g_atp_unorient_step_calls     = 0;
u64 g_atp_unorient_step_fires     = 0;
u64 g_atp_unorient_step_empty     = 0;
u64 g_atp_unorient_step_us        = 0;
// WM cp-gen overlap-candidate accounting (profile only): the candidates the
// index actually iterates vs the full-scan partner count (n + first) the
// unindexed loops would have visited per new fact.  Their ratio is the
// O(n_rules)-scan reduction.
u64 g_atp_wmcp_cand               = 0;
u64 g_atp_wmcp_full               = 0;
u64 g_atp_wmcp_zero               = 0;
u64 g_atp_wmcp_nonzero            = 0;
u8  g_atp_phase_enabled           = 0;
// Push-normalize detail tier (THVM_ATP_PROFILE=2): splits the per-CP
// atp_cp_trivially_joinable cost inside atp_push_cps_traced into its
// components -- FT-mirror ensure, Term->FT conversion, the FT
// normalize core, the ft_eq verdicts, FT->Term decode of survivors,
// the AC-eq redundancy check, the (default-off) join-cache hashing,
// and the raw-size symbol-count gate.  Accumulated ONLY while
// g_atp_pushn_active is set (the push-loop treatment region), so the
// components sum to g_atp_phase_us_push_normalize up to loop
// overhead.  Zero cost when the detail tier is off.
u64 g_atp_pushn_us_mirror         = 0;   // atp_ft_mirror_ensure
u64 g_atp_pushn_us_from           = 0;   // ft_from_term x2
u64 g_atp_pushn_us_norm           = 0;   // atp_rules_only_normalize_ft x2
u64 g_atp_pushn_us_eq             = 0;   // ft_eq verdict(s)
u64 g_atp_pushn_us_to             = 0;   // ft_to_term x2 (survivors only)
u64 g_atp_pushn_us_ac             = 0;   // AC-eq redundancy check
u64 g_atp_pushn_us_hash           = 0;   // join-cache struct-hash + probe
u64 g_atp_pushn_us_gate           = 0;   // atp_symbol_count raw-size gate
u64 g_atp_pushn_calls             = 0;   // trivially_joinable calls (push)
u64 g_atp_pushn_joined            = 0;   // joined verdicts (push)
// Pack-volume class split (same detail tier): node totals + push counts
// for the raw >= 50 untreated class vs the treated (< 50) survivors.
// Bounds the FT-direct-pack lever: only the TREATED class could skip
// the Term intermediate (raw CPs are born as Terms in the walk former),
// so its share of pack nodes caps that lever's yield.
u64 g_acp_pack_nodes_raw          = 0;
u64 g_acp_pack_nodes_treated      = 0;
u64 g_acp_pack_n_raw              = 0;
u64 g_acp_pack_n_treated          = 0;
// Normalize-core shape counters (same detail tier + push scope): per
// ft_cell_try_rules query in the push-normalize path -- queries,
// var-cell early-outs, candidate-loop iterations, ft_match attempts /
// hits, ft_wm_pattern_before winner compares, winner re-matches, and
// splices fired (ft_mixmost_reduce_here successes).
u64 g_atp_pushn_tr_q              = 0;
u64 g_atp_pushn_tr_var            = 0;
u64 g_atp_pushn_tr_cand           = 0;
u64 g_atp_pushn_tr_att            = 0;
u64 g_atp_pushn_tr_hit            = 0;
u64 g_atp_pushn_tr_pb             = 0;
u64 g_atp_pushn_tr_rematch        = 0;
u64 g_atp_pushn_tr_win            = 0;
u64 g_atp_pushn_rw                = 0;
// PHASE-0 allocs-per-rewrite (THVM_ATP_PROFILE=2, push scope only):
// gross persistent cells allocated / freed by push-scope splices, and
// the count of ALLOCATING (regime-c) splices vs in-place ones.  Native
// WM rewrites the flatterm in place (~0 alloc/rewrite); this brackets
// g_ft_persist_alloc_total / g_ft_free_span_total around ft_splice in
// ft_mixmost_reduce_here so the norm-core alloc traffic is isolated
// from formation / unpack allocation.
u64 g_atp_pushn_alloc_cells       = 0;
u64 g_atp_pushn_free_cells        = 0;
u64 g_atp_pushn_splice_c          = 0;   // allocating splices (regime c)
u64 g_atp_pushn_splice_a          = 0;   // in-place splices (regime a)
// Discrimination-tree walk shape (ftdt_descend, same tier + scope):
// nodes visited (chain-followed + branched) and branch-node edge-loop
// iterations, so the tree-walk cost per query is directly readable.
u64 g_atp_pushn_dt_node           = 0;
u64 g_atp_pushn_dt_edge           = 0;
// Deep descend split (same tier + scope): where inside the walk the
// visits/edges go.  chain = single-concrete-edge lockstep hops,
// branch = nodes that reach the edge loops, prune = sub_mask subtree
// rejects, emit = leaf emissions.  Var-edge exploration splits into
// first-traversal binds (bind1), repeat ft_eq compares (fteq, of
// which fteq_ok passed, fteq_cell subject cells walked by the eq).
// cprobe = concrete-region probe iterations (sorted early-break run).
// hist[d] = node visits at tree depth d (last bucket = deeper).
u64 g_atp_pushn_dt_chain          = 0;
u64 g_atp_pushn_dt_branch         = 0;
u64 g_atp_pushn_dt_prune          = 0;
u64 g_atp_pushn_dt_emit           = 0;
u64 g_atp_pushn_dt_bind1          = 0;
u64 g_atp_pushn_dt_fteq           = 0;
u64 g_atp_pushn_dt_fteq_ok        = 0;
u64 g_atp_pushn_dt_fteq_cell      = 0;
u64 g_atp_pushn_dt_cprobe         = 0;
#define ATP_DT_HIST_N 24
u64 g_atp_pushn_dt_hist[ATP_DT_HIST_N] = {0};
// Norm-core waste attribution (default v3 walk, same tier + scope).
// `fteq` above counts repeat-variable consistency ATTEMPTS; the splits:
//   symfail = attempts decided by the inline first-cell sym compare
//             (no ft_eq call -- one predicted branch, ~free), so real
//             ft_eq invocations = fteq - symfail, of which fteq_ok pass;
//   chain   = attempts fired at the single-var-edge iterative hop (the
//             remainder fire in the branch-node var-edge loop).
// vedge = var-region edges summed over branch visits (the var half of
// the dt_edge total; the concrete half splits into cprobe touched vs
// early-break skipped).  fan_visit/fan_edge = branch-node visits and
// summed edges bucketed by the node's edge count (bucket
// min(n_edge, ATP_DT_FAN_N-1)) -- the per-node fan-out distribution.
u64 g_atp_pushn_dt_fteq_symfail   = 0;
u64 g_atp_pushn_dt_fteq_chain     = 0;
u64 g_atp_pushn_dt_vedge          = 0;
#define ATP_DT_FAN_N 16
u64 g_atp_pushn_dt_fan_visit[ATP_DT_FAN_N] = {0};
u64 g_atp_pushn_dt_fan_edge[ATP_DT_FAN_N]  = {0};
// Ground-join lane shape counters (bench print block; unconditional --
// each counted event does a thvm_match / full normalize, so a u64
// increment is invisible next to it):
//   top_q  = gj_try_top_indexed calls (one per considered position)
//   cand   = index candidates enumerated across those calls
//   match  = candidates whose thvm_match accepted (reduct materialized)
//   noop   = matched candidates skipped as kbo_eq no-op images
//   gate   = matched candidates that reached the gj_less_in/lex gate
//   pass   = gate calls that returned STRICT/NONSTRICT (rule fired)
//   models = per-model symbolic-join attempts (gj_joins_under calls)
//   steps  = fired rewrite steps across all GJ normalizes
u64 g_atp_gjp_top_q      = 0;
u64 g_atp_gjp_cand       = 0;
u64 g_atp_gjp_match      = 0;
u64 g_atp_gjp_noop       = 0;
u64 g_atp_gjp_gate       = 0;
u64 g_atp_gjp_gate_pass  = 0;
u64 g_atp_gjp_models     = 0;
u64 g_atp_gjp_norm_steps = 0;
u64 g_atp_gjp_norms      = 0;
// Mixmost re-query provenance (same tier + scope): reduce_here calls
// issued by the local-fixpoint whiles vs the post-reduction ascent
// loop -- the rest of the queries are the preorder walk's first
// visits.  Counts CALLS (the failing terminator included), so
// fix - splices = the redundant fixpoint re-queries.
u64 g_atp_pushn_mm_fix            = 0;
u64 g_atp_pushn_mm_asc            = 0;
// CP-gen non-push detail tier (same THVM_ATP_PROFILE=2 gate): splits
// the cp-gen phase OUTSIDE push-normalize into its components inside
// the single-walk former + atp_push_cps_traced --
//   collect  = atp_cp_index_collect / atp_cp_subindex_collect + the
//              candidate sorts (the discrimination-tree partner filter)
//   overlap  = atp_overlap_ij (position walk + unification) per pair
//   rank     = the walk's partner enumeration (wmo_tops_enum /
//              wmo_leaflist_enum order-mirror queries)
//   pushloop = the per-CP form+push work in the walk (INCLUDING
//              push-norm, which g_atp_phase_us_push_normalize times
//              separately)
// and inside atp_push_cps_traced (per surviving CP):
//   varnorm  = thvm_normalize_vars (raw snapshot + treated pair)
//   filters  = perm-subsume / rule-subsume / queue-subsume probes
//   trace    = atp_trace_push_cp (proof-trace entry)
//   pack     = atp_cp_heap_push (acp_pack + weigh + heap insert), of
//              which kbo = the cp_pri/cp_goal/cp_pri2 weighing inside
//              atp_cp_heap_insert_packed
// All accumulate ONLY under g_atp_phase_detail, so PROFILE=1 runs and
// the default engine pay a single dead branch per site.
u64 g_atp_cpg_us_collect          = 0;
u64 g_atp_cpg_us_overlap          = 0;
u64 g_atp_cpg_us_rank             = 0;
u64 g_atp_cpg_us_pushloop         = 0;
u64 g_atp_cpg_us_varnorm          = 0;
u64 g_atp_cpg_us_filters          = 0;
u64 g_atp_cpg_us_trace            = 0;
u64 g_atp_cpg_us_pack             = 0;
u64 g_atp_cpg_us_kbo              = 0;
u64 g_atp_cpg_overlap_calls       = 0;
u64 g_atp_cpg_rank_calls          = 0;
// Walk rename-cache shape (sw_rename_pair; PROFILE=2 printout).
u64 g_atp_swrn_hit                = 0;
u64 g_atp_swrn_miss               = 0;
// Whole-run ATP GC cost (thvm_atp_gc_collect): collections + wall.
// The heap-pressure collect fires inside the CP-formation walk, so
// the phase tiles bury it in cp-gen; these track it explicitly.
// Unconditional (a saturation runs O(100) collections, timing is noise).
u64 g_atp_gc_us                   = 0;
u64 g_atp_gc_n                    = 0;
u8  g_atp_phase_detail            = 0;
static u8 g_atp_pushn_active      = 0;
// GC epoch: bumped by every thvm_atp_gc_collect (the only collector
// that runs during saturation), so a handle-keyed cache can detect
// cell relocation/reuse.  Cells never move between collections.
static u64 g_atp_gc_epoch         = 0;
// Init generation for the snapshot-orientation memo (atp_orient_memo_gt,
// defined with atp_ordered_try_top below); thvm_atp_init bumps it.
static u64 g_atp_orient_memo_gen  = 0;
// Perm-subsume AC-operator mask (WM GZ_ACVerzichtbar / dokgP).  Set by
// thvm_atp_set_perm_subsume_mask (from thvm_atp_auto_ac's detected AC
// symbols); a non-zero mask routes atp_cp_perm_subsumed through the
// flattened AC-equality test and confines the drop to the KPV_Select
// pop site.  Defined up here because the enqueue / pop-normalize gates
// read it before its setter's definition point.
static u64 g_atp_perm_subsume_mask = 0ull;
// Forward decl: atp_now_us is defined further down (with the wall-clock
// deadline helpers); needed up here for the profile timers in functions
// defined ahead of it (e.g. atp_unorient_step_indexed).
static u64 atp_now_us(void);
static inline u64 atp_phase_now(void) {
  if (!g_atp_phase_enabled) return 0;
  return atp_now_us();
}

// "Unorient-step no-fire" memo.  Most calls (~78% on andassoc) return
// no-fire after a full O(|term|) preorder scan over the subject -- the
// mandatory "fixpoint reached" call at the end of every mixed-loop
// normalize.  Cache the no-fire verdict so a re-call on the same
// shape skips the whole scan.
//
// Key: 64-bit FNV-style STRUCTURAL hash of the subject (not the cell
// value).  Cell-identity keying alone catches only ~10% of calls (most
// post-splice cells are fresh); structural-hash keying catches the
// cross-call reuse and pushes the hit ratio to ~60%.
//
// Soundness analysis:
//  - A hash collision can mislabel a different subject as no-fire and
//    skip a possible unorient rewrite.  Downstream the CP is queued
//    with a less-reduced form; pop-time normalize re-runs (same hash,
//    same epoch) but the JOIN VERDICT only fires under kbo_eq on the
//    NF reached, so a missed rewrite biases toward keeping the CP
//    (false negative on joinable, never false positive) -- the engine
//    over-orients but the proof itself remains sound (every recorded
//    rule is a valid equational consequence).
//  - Trajectory drift IS observable: on andassoc the run terminates
//    in 4150-4219 steps vs 4649 with no memo, but PROVED status and
//    the test_atp 135603 byte-identical fingerprint (gate off path) are
//    preserved.
//  - Epoch follows rule_index_dirty so R-changes flush verdicts; cell
//    motion / heap recycle don't matter because the hash is structural,
//    not cell-keyed.
#define ATP_UNF_MEMO_BITS 16
#define ATP_UNF_MEMO_SIZE (1u << ATP_UNF_MEMO_BITS)
#define ATP_UNF_MEMO_MASK (ATP_UNF_MEMO_SIZE - 1u)
typedef struct {
  u64 hash;
  u32 epoch;
} AtpUnfMemoEnt;
static AtpUnfMemoEnt g_atp_unf_memo[ATP_UNF_MEMO_SIZE];
static u32 g_atp_unf_memo_epoch = 1u;
// Unorient-step memo epoch.  Split from the broader unf_memo_epoch
// because the empty-call verdict at atp_unorient_step_indexed (and the
// per-position descent memo) depends ONLY on the unorient_index faces.
// A no-fire verdict stays valid across an R change ONLY when the live
// rule set is bytewise unchanged -- that is exactly what r_revision
// tracks (it bumps on EVERY add, drop, orient-flip, and bwd-subsume
// soft-delete).  Keying the bump on n_unorient alone was unsound: a
// backward-subsumption soft-delete drops a rule's face without changing
// n_unorient, so a stale no-fire verdict would skip a now-firing
// rewrite.  Bumped from atp_unf_memo_invalidate whenever r_revision
// changed since the last invalidate (or at init/GC where cell ids may
// be reused).
static u32 g_atp_unf_step_epoch = 1u;
static u32 g_atp_last_r_revision = 0u;

// Bounded preorder FNV-64 hash.  Recurses to subterms; on Sheffer-size
// terms (<= 100 symbols) the cost is O(|term|) and amortizes well
// against the saved per-position unorientable scan.
static u64 atp_term_struct_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u64 h = 0xcbf29ce484222325ull ^ ((u64)term_ext(t) * 0x100000001b3ull);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h ^= atp_term_struct_hash(term_ctr_at(t, i));
        h *= 0x100000001b3ull;
      }
      return h ^ ((u64)n * 0x9e3779b97f4a7c15ull);
    }
    case TAG_FVR:
      return 0xfacefacefaceull ^ ((u64)term_ext(t) * 0x100000001b3ull);
    default:
      return 0xdeadbeefcafebabeull ^ ((u64)term_tag(t) * 0x100000001b3ull);
  }
}

// LPO/KBO orientability cache.  Gated by THVM_ATP_LPO_ORIENT_CACHE;
// compiles to nothing when the flag is off so the default build stays
// byte-identical.  Defined here -- after atp_term_struct_hash, before
// atp_compare -- so the wrapped comparator below can call it.
#include "lpo_cache.c"
#ifdef THVM_ATP_LPO_ORIENT_CACHE
#define ATP_ORIENT_CACHE_INVAL() atp_lpo_orient_cache_invalidate()
#else
#define ATP_ORIENT_CACHE_INVAL() ((void)0)
#endif

// AC reasoning: declarations + canonical-form flatten + AC-equality
// / AC-hash.  See docs/atp/engineering.md.  Gated on THVM_ATP_AC;
// off the flag the file compiles to nothing.  Included here so the
// global g_atp_ac_info + atp_ac_eq are visible to the trivial-join
// wiring below.
#include "ac.c"

#include "state.c"
#include "cp_store.c"
#include "cp_emit.c"
#include "fv_index.c"
// === 7e lever 2: rule-LHS redex index (-DATP_RULE_INDEX) ============
//
// THE OTHER WALL.  Once 7d collapsed the CP-queue subsumption scan,
// the diagnosis re-profiled and pinned the remaining normalization
// wall on `thvm_rewrite_step` -- specifically `rewrite_try_top`'s
// O(n_rules) linear LHS scan.  `atp_cp_trivially_joinable` runs two
// full `atp_rewrite_normalize` calls per CP candidate; each is up to
// NORM_CAP=64 `thvm_rewrite_step`s; each step calls `rewrite_try_top`
// at every preorder position of the term; and `rewrite_try_top` tries
// every rule LHS at that position.  As R climbs past 250 rules the
// linear scan dominates.
//
// THE FIX -- the DUAL of 7d.  7d indexes `Cp(lhs, rhs)` PAIRS and, for
// a subject CP, retrieves a stored pattern that one-way matches it
// (subsumption retrieval).  Here we index single rule-LHS TERMS and,
// for a subject subterm, retrieve which rule LHS one-way matches it
// (redex retrieval) -- the same matching direction (stored pattern has
// variables, subject is concrete), so 7d's perfect-discrimination-tree
// descent is reused VERBATIM: CTR-exact edge / first-var-bind STAR /
// repeat-var-kbo_eq STAR, the STAR/CTR flat alphabet, the preorder
// flatten with subtree spans.  Only the insert key (one term, no `Cp`
// wrapper) and the leaf action (collect a rule index, not return-on-
// first-hit) are rewritten.
//
// BEHAVIOR-IDENTITY -- the lowest-rule-index rule.  `rewrite_try_top`
// tries rules in index order and the FIRST match wins; mid-completion
// R is not confluent, so which rule fires changes the normal form.
// The tree returns leaves in tree order, not index order.  So the
// descent does NOT stop on first hit: it visits EVERY reachable leaf
// and tracks the minimum rule index.  The leaf still runs the SAME
// one-way `thvm_match` the linear scan ran, as the authoritative
// guard, so a stored LHS reaches "winner" status iff `thvm_match`
// confirms it -- byte-identical to the linear scan picking that rule.
// `thvm_rewrite_step`'s redex-selection order (top, then children
// left-to-right) is untouched -- only the per-position rule choice is
// indexed.  This block is independent of -DATP_FV_INDEX (it carries
// its own copy of the discrimination-tree skeleton).

#ifdef ATP_RULE_INDEX
#include "rule_index.c"
#include "norm_paths.c"
#include "overlap_index.c"
#endif // ATP_RULE_INDEX

#include "lifecycle.c"
#include "gc.c"
#include "ordered_rewrite.c"
#include "cp_weight.c"
#include "gnn.c"
#include "cp_heap.c"
#include "rerank.c"
#include "trace.c"
#include "enigma_data.c"

// Drive thvm_atp_step until it returns a non-RUNNING status.
fn AtpStatus thvm_atp_run(AtpState *s) {
  AtpStatus st;
  // Emergency-trace: tip every N steps to stderr when env set, so a
  // signal-deaf hang in some step's inner work is at least visible.
  const char *trace = getenv("THVM_ATP_TICK_TRACE");
  u32 trace_every = (trace != NULL && trace[0] != '\0' && trace[0] != '0')
      ? (u32)atoi(trace) : 0u;
  u32 tick = 0u;
  do {
    if (trace_every && (tick % trace_every) == 0u) {
      fprintf(stderr, "[atp_run] step=%u n_rules=%u n_cps=%u\n",
              s->step, s->n_rules, s->n_cps);
    }
    tick++;
    st = thvm_atp_step(s);
  } while (st == ATP_RUNNING);
  if (trace_every) {
    fprintf(stderr, "[atp_run] exit st=%d step=%u n_rules=%u n_cps=%u\n",
            (int)st, s->step, s->n_rules, s->n_cps);
  }
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

#include "mnf.c"

#ifdef ATP_ORDERED_REWRITE
// The ground goal-narrowing endgame (opt-in --goal-narrow): ground
// congruence closure + bounded two-way e-matching over the goal cone.
#include "goal_narrow.c"
#endif

#include "goal_check.c"
#include "cp_interreduce.c"
#include "wm_drain.c"
#include "proof_cone.c"
#include "diag.c"
#include "subsume.c"
#include "ground_join.c"
#include "cp_push.c"
// 8.1e-i: C-direct critical-pair enumerator -- the path
// `thvm_atp_generate_cps` takes when `s->use_ic_cp_gen == 0`
// (the default).  Bulk of the work happens in
// `thvm_critical_pairs_range`; this function just plumbs the
// (i, j) iteration and trace bookkeeping.
// Overlap rule j into rule i, emitting every CP face the active
// completion variant requires.  Default (use_unfailing_cp == 0): the
// single (i-face = li, j = lj->rj) overlap -- byte-for-byte the prior
// thvm_critical_pairs_range(i,i+1,j,j+1) behaviour.  Unfailing
// (use_unfailing_cp == 1): when rule j is an unorientable equation,
// ALSO overlap its rhs-face (rj->lj); when rule i is unorientable, ALSO
// walk the positions of its rhs-face (so a redex inside ri is found,
// the CP's other component then being li).  Both-faces superposition is
// what unfailing completion needs to be COMPLETE on incomparable
// equations (Bachmair-Dershowitz-Plaisted; Waldmeister's default mode).
// WM Monogleichung test (RE_ErzeugteGleichung, RUndEVerwaltung.c:
// 435-438): an unorientable equation is a mono equation when its
// reversed pair, variable-renumbered (BO_TermpaarNormieren), equals
// the stored pair -- the Antigleichung twin is a variant of the
// equation itself (e.g. commutativity f(x,y) = f(y,x)).  WM then
// indexes ONLY the distinguished face (GleichungEinfuegen :484-491
// skips the Herrichtung insert) and skips the reverse-face overlap
// phases C/D/E/G of its own CP-generation pass
// (U1_KPsBildenZuGleichung, Unifikation1.c:1563-1579 table + the
// :1625 TP_IstKeineMonogleichung gate), so a mono equation
// participates in CP formation through its distinguished face only.
// The suppressed combos would emit exact variants of the
// forward-face CPs -- pairs WM never forms.
static u8 atp_eq_is_mono(const AtpState *s, u32 i);

// Waldmeister CP-emission-order mirror (leaf lists, discrimination-tree
// order, tops-DFS ranking).  See the file header for the decoded model
// and waldmeister/sources citations.
#include "wm_order.c"

// Waldmeister loader-level axiom canonicalization + intake semantics
// (SpezNormierung canonical sort + initial=ultimate FIFO stamp).  See
// the file header for the decoded pipeline and citations.
#include "wm_intake.c"

#include "cp_gen.c"
#include "step.c"
#include "narrow.c"
