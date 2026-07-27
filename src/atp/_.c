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

// Waldmeister CP-emission-order mirror (src/atp/wm_order.c, included
// after atp_eq_is_mono's declaration block).  Forward declarations so
// the lifecycle hooks (atp_push_rule, interreduce, eset-subsume) and
// the setter can call in before the include point.
struct AtpWmOrder;
static struct AtpWmOrder *atp_wmo_new(void);
static void atp_wmo_free(struct AtpWmOrder *w);
static void atp_wmo_insert_fact(AtpState *s, u32 slot);
static void atp_wmo_insert_fact_ex(AtpState *s, u32 slot, u8 cp_derived);
static void atp_wmo_remove_trace(AtpState *s, u32 trace);
static u8   atp_wmo_dist_rhs_of(AtpState *s, u32 uid);
static u32  atp_wmo_victim_drain_key(AtpState *s, u32 trace,
                                     u8 reduced_thvm_side);
// Gleichungsbaum (equation-tree) leaf-list rank for the unorientable
// equation with trace id `trace`, queried on the rewrite direction
// `thvm_dir` (0 = match stored LHS, 1 = match stored RHS; the WM face is
// thvm_dir XOR dist_rhs).  Returns 1 + fills (*out_ll, *out_chain) with
// the leaf-list position and within-leaf chain index that WM's
// MO_GleichungGefunden DFS reaches the face in; 0 when the face is not
// registered.  Lets the normalize-redex selection (atp_ft_unorient_step)
// rank competing equation candidates in WM's retrieval order instead of
// thvm slot order.  Defined in wm_order.c.
static u8   atp_wmo_eq_leaflist_rank(AtpState *s, u32 trace, u8 thvm_dir,
                                     u32 *out_ll, u32 *out_chain);
// DFS arrival rank of an equation face against a concrete redex subterm,
// the faithful WM MO_GleichungGefunden retrieval order (function-symbol
// branch before variable branch) used to break competing-redex ties in
// the unorientable normalize step.  Defined in wm_order.c.
static u8   atp_wmo_eq_tops_rank(AtpState *s, u32 trace, u8 thvm_dir,
                                 Term redex_sub, u32 *out_arrival,
                                 u32 *out_chain);

// Waldmeister loader-level axiom canonicalization (src/atp/wm_intake.c,
// included next to wm_order.c).  Forward declaration so the first
// thvm_atp_step call can flush the intake before any pop.
static void atp_wm_intake_canonicalize(AtpState *s);

// Sync the WM-faithful trie-construction flag onto the wmo mirror (defined
// in wm_order.c, included below).  Forward-declared so the setter above the
// include can push the flag onto an already-created mirror.
static void atp_wmo_sync_trie_faithful(AtpState *s);

// Flat-transposition (commutativity `f(x,y)=f(y,x)`) predicate, defined
// after the wm_order.c include; forward-declared so the overlap-exhaust
// pair gate can exclude commutativity partners.
static u8 atp_is_flat_transposition(Term lhs, Term rhs);

// Match helper: thvm_match by default; switches to atp_match_ac
// when THVM_ATP_AC is built AND the engine-global AC mask is non-zero.
// Used by the ATP-internal hot match sites (atp_ordered_try_top,
// rewrite-step fallbacks).  The compile-time #ifdef keeps the
// default build (without THVM_ATP_AC) byte-identical to thvm_match.
static inline u8 atp_match_maybe_ac(Term pat, Term subj, RewriteSubst *subst) {
#ifdef THVM_ATP_AC
  u64 m = thvm_atp_get_ac_mask();
  if (m != 0ull) {
    AtpAcInfo ac = { .ac_mask = m };
    return atp_match_ac(pat, subj, &ac, subst);
  }
#endif
  return thvm_match(pat, subj, subst);
}

// Normalize result cache: maps (term_struct_hash, g_atp_unf_memo_epoch)
// to the already-normalized Term.
#define ATP_NORM_CACHE_BITS 16
#define ATP_NORM_CACHE_SIZE (1u << ATP_NORM_CACHE_BITS)
#define ATP_NORM_CACHE_MASK (ATP_NORM_CACHE_SIZE - 1u)
typedef struct { u64 hash; u32 epoch; u32 n_rules; Term nf; } AtpNormCacheEnt;
static AtpNormCacheEnt g_atp_norm_cache[ATP_NORM_CACHE_SIZE];
// Per-position no-fire memo storage (typedef + sizes) lives here so the
// step-epoch invalidate can clear its slots on wrap.  The cache helpers
// (get/put) live below near their natural callers; the soundness
// contract is documented there.
#define ATP_UNF_POS_MEMO_BITS 14
#define ATP_UNF_POS_MEMO_SIZE (1u << ATP_UNF_POS_MEMO_BITS)
#define ATP_UNF_POS_MEMO_MASK (ATP_UNF_POS_MEMO_SIZE - 1u)
typedef struct { u64 hash; u32 epoch; u8 folded; } AtpUnfPosMemoEnt;
static AtpUnfPosMemoEnt g_atp_unf_pos_memo[ATP_UNF_POS_MEMO_SIZE];
// Bump just the step epoch (the unorient-step empty-call memo + the
// per-position descent memo).  Wrap-around clears their slots so a
// stale verdict can't be aliased into the post-wrap epoch.
static void atp_unf_step_memo_invalidate(void) {
  if (++g_atp_unf_step_epoch == 0u) {
    for (u32 i = 0; i < ATP_UNF_MEMO_SIZE; i++)
      g_atp_unf_memo[i].epoch = 0u;
    for (u32 i = 0; i < ATP_UNF_POS_MEMO_SIZE; i++)
      g_atp_unf_pos_memo[i].epoch = 0u;
    g_atp_unf_step_epoch = 1u;
  }
}
// Bump the broader cell-keyed epoch (RHS flat cache, norm-result cache,
// join cache).  Optionally also bumps the step epoch -- callers pass
// `force_step_bump = 1` when cell identity may have shifted (init / GC)
// or when they cannot cheaply prove the rule set is unchanged.  Otherwise
// the step epoch is bumped only when r_revision moved since the last
// invalidate: a no-fire verdict is valid exactly while the live rule set
// is bytewise unchanged, and r_revision is the monotone counter that
// witnesses that (bumps on every add / drop / orient-flip / bwd-subsume).
fn void atp_unf_memo_invalidate(u32 r_revision_now, u8 force_step_bump) {
  if (++g_atp_unf_memo_epoch == 0u) {
    for (u32 i = 0; i < ATP_UNF_MEMO_SIZE; i++)
      g_atp_unf_memo[i].epoch = 0u;
    // Also zero the normalize-result cache so a stale NF doesn't
    // survive the epoch-wrap.
    for (u32 i = 0; i < ATP_NORM_CACHE_SIZE; i++)
      g_atp_norm_cache[i].epoch = 0u;
    g_atp_unf_memo_epoch = 1u;
  }
  if (force_step_bump || r_revision_now != g_atp_last_r_revision) {
    atp_unf_step_memo_invalidate();
    g_atp_last_r_revision = r_revision_now;
  }
}
static inline u8 atp_unf_memo_get(u64 h) {
  AtpUnfMemoEnt *e = &g_atp_unf_memo[(u32)h & ATP_UNF_MEMO_MASK];
  return (e->hash == h && e->epoch == g_atp_unf_step_epoch) ? 1u : 0u;
}
static inline void atp_unf_memo_put(u64 h) {
  AtpUnfMemoEnt *e = &g_atp_unf_memo[(u32)h & ATP_UNF_MEMO_MASK];
  e->hash  = h;
  e->epoch = g_atp_unf_step_epoch;
}
u64 g_atp_unf_memo_hits   = 0;
u64 g_atp_unf_memo_misses = 0;
u64 g_atp_ri_splice_inline_hits   = 0;
u64 g_atp_ri_splice_inline_misses = 0;

// Normalize-result cache counters (struct/storage declared above so
// atp_unf_memo_invalidate can clear on epoch wrap-around).  The
// cache is currently dormant -- the value slot stored Term cells
// which heap_reset recycles; the get/put helpers were removed when
// that storage shape proved unsafe.  Leaving the storage + counters
// in place lets a future port re-introduce a NF cache (e.g. keyed by
// content hash of the result, not the cell ID).
u64 g_atp_norm_cache_hits   = 0;
u64 g_atp_norm_cache_misses = 0;

// Per-position no-fire memo for the unorientable discrimination-tree
// descent at atp_ri_query_pos_unorient.  Keyed by (subtree-phash,
// query_folded).  Soundness: the descent's verdict depends on
// (subtree structure, R's unorient_index, ix->any_folded, query_folded).
// The first two are bound to the step epoch (bumped via
// atp_unf_step_memo_invalidate when r_revision changes, or
// unconditionally at init/GC).  ix->any_folded is per-index, refreshed
// at rebuild.  query_folded is per-CALL (depends on whole subject), so
// MUST be in the key -- otherwise a verdict stored at folded=X is
// looked up at folded=Y and disagrees via leaf_collect_unorient's
// `perfect = !any_folded && !query_folded` flag.  Caches NEGATIVE
// verdicts only (ncand==0): a positive verdict has data (the candidate
// list) we'd have to cache too, and the positive path is rare and
// short-circuits early so the win is in the dominant "no fire here"
// no-op path.  Storage + sizing macros are declared above (near the
// step-epoch invalidate) so the wrap-around clear can touch them.
// Epoch tracks g_atp_unf_step_epoch -- same soundness contract as the
// whole-subject step memo (depends only on unorient_index data).
static inline u8 atp_unf_pos_memo_get(u64 h, u8 folded) {
  AtpUnfPosMemoEnt *e = &g_atp_unf_pos_memo[(u32)h & ATP_UNF_POS_MEMO_MASK];
  return (e->hash == h && e->epoch == g_atp_unf_step_epoch
          && e->folded == folded) ? 1u : 0u;
}
static inline void atp_unf_pos_memo_put(u64 h, u8 folded) {
  AtpUnfPosMemoEnt *e = &g_atp_unf_pos_memo[(u32)h & ATP_UNF_POS_MEMO_MASK];
  e->hash   = h;
  e->epoch  = g_atp_unf_step_epoch;
  e->folded = folded;
}
u64 g_atp_unf_pos_memo_hits   = 0;
u64 g_atp_unf_pos_memo_misses = 0;
// Bottom-up subtree FNV-64 hash over the flat encode.  Each
// flathash[p] is the hash of the SUBTREE rooted at p, computed from
// flatsym[p..p+subsz[p]).  Direct port of atp_term_struct_hash adapted
// to flat-array iteration -- no Term tree walk.
static void atp_unf_flathash(const u32 *flatsym, const u32 *subsz,
                             u32 flatlen, u64 *flathash) {
  for (i64 p = (i64)flatlen - 1; p >= 0; p--) {
    u64 h = 0xcbf29ce484222325ull ^ ((u64)flatsym[p] * 0x100000001b3ull);
    u32 child = (u32)p + 1u;
    u32 end   = (u32)p + subsz[p];
    while (child < end) {
      h ^= flathash[child];
      h *= 0x100000001b3ull;
      child += subsz[child];
    }
    flathash[p] = h;
  }
}

#ifdef ATP_RULE_INDEX
// 7e lever 2: forward declaration -- the rule-LHS redex index and its
// indexed normalizer are defined further down (after the FV-index
// block, where the discrimination-tree skeleton lives); the shim
// below dispatches to it.
static Term atp_rewrite_normalize_indexed(AtpState *s, Term t, u32 step_cap);

// Preorder node count -- defined after atp_compare; the indexed
// normalizer needs it up here to size an incremental-flatten splice.
static u32 atp_symbol_count(Term t);
#ifdef ATP_ORDERED_REWRITE
// Opt-in flatterm fast-path for the mixed normalize loop; defined after
// the indexed normalizer + ordered helpers it builds on.
static Term atp_rewrite_normalize_flatterm_mixed(AtpState *s, Term t,
                                                 u32 step_cap);
#if defined(ATP_FLATTERM_SELFCHECK) || defined(ATP_FLATTERM_DIFF)
static Term atp_rewrite_normalize_flatterm_selfcheck_tree(AtpState *s, Term t,
                                                          u32 step_cap);
#endif
#ifdef ATP_FLATTERM_DIFF
// DIFF test build: live-normalize flat-vs-tree mismatch counter (see the
// in-dispatch check in atp_rewrite_normalize_ordered).  A test CHECKs ==0.
u32 g_atp_ft_diff_mism = 0u;
#endif
#endif
#endif

// Throttled wall-deadline / host-abort poll for the inner rewrite
// loops.  goal-check normalizes with a 65536 step cap, and the mixed
// ordered path nests an indexed normalize inside its own 65536-step
// loop, so a single normalize can run far past TimeConstraint (or a
// host Abort[] / TimeConstrained[]) before thvm_atp_step's per-step
// check is ever reached again.  Defined after the wall-deadline
// machinery; forward-declared here for the normalizers above.  On a
// fire the caller returns the partial term and the next thvm_atp_step
// turns it into ATP_TIMEOUT / ATP_ABORTED.
static int atp_norm_deadline_fired(AtpState *s);

#ifdef ATP_ORDERED_REWRITE
// 9c-foundation: forward declaration -- the ordered normalizer is
// defined after atp_compare (it needs the reduction-order compare).
static Term atp_rewrite_normalize_ordered(AtpState *s, Term t,
                                          const Term *lhs, const Term *rhs,
                                          u32 n_rules, u32 step_cap);
#endif

// 8.3e-i: AtpState-aware shim.  Dispatches between the C-direct
// and IC-routed normalize paths based on s->use_ic_rewrite.
// Replaces direct `thvm_rewrite_normalize` calls in
// AtpState-internal callers (saturation step, goal-check,
// interreduce, joinability/connectedness filters).
//
// 7e lever 2: under -DATP_RULE_INDEX, a normalize call against the
// FULL current rule set (lhs == s->lhs && n_rules == s->n_rules --
// the hot `atp_cp_trivially_joinable` / saturation-step / goal-check
// path) routes to the rule-LHS discrimination index instead of
// `rewrite_try_top`'s linear scan.  Calls against any OTHER rule
// array (interreduce's 1-2 rule slice; the diag connectedness
// filter's filtered set) keep the linear scan -- the index reflects
// s->lhs[] only.  IC-routed rewriting (use_ic_rewrite) takes
// precedence, unchanged.
static Term atp_rewrite_normalize(AtpState *s, Term t,
                                  const Term *lhs, const Term *rhs,
                                  u32 n_rules, u32 step_cap) {
#ifdef ATP_ORDERED_REWRITE
  // 9c-foundation: proper unfailing-completion rewriting.  Supersedes
  // the indexed / IC / linear paths below (which all assume rules are
  // pre-oriented).  Needs s for the reduction-order comparison.
  if (s != NULL) {
    static int dl = -1;
    if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
    u64 h0 = dl ? HEAP_NEXT : 0;
    Term out = atp_rewrite_normalize_ordered(s, t, lhs, rhs, n_rules, step_cap);
    if (dl) { u64 d = HEAP_NEXT - h0;
      if (d > 2000000ull) fprintf(stderr,
        "NORMBIG in=%u out=%u delta=%lluK cap=%u n_rules=%u nun=%u phase=%s\n",
        atp_symbol_count(t), atp_symbol_count(out),
        (unsigned long long)(d>>10), step_cap, n_rules, s->n_unorient,
        g_thvm_phase); }
    return out;
  }
#endif
  if (s != NULL && s->use_ic_rewrite) {
    return atp_rewrite_normalize_ic(t, lhs, rhs, n_rules, step_cap);
  }
#ifdef ATP_RULE_INDEX
  if (s != NULL && lhs == s->lhs && rhs == s->rhs && n_rules == s->n_rules) {
    return atp_rewrite_normalize_indexed(s, t, step_cap);
  }
#endif
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
  Term *nl  = (Term *)realloc(s->lhs,              cap * sizeof(Term));
  Term *nr  = (Term *)realloc(s->rhs,              cap * sizeof(Term));
  u32  *nt  = (u32  *)realloc(s->r_trace,          cap * sizeof(u32));
  u8   *no  = (u8   *)realloc(s->r_orient,         cap * sizeof(u8));
  u8   *nd  = (u8   *)realloc(s->r_dead,           cap * sizeof(u8));
  Term *nls = (Term *)realloc(s->r_dead_lhs_save,  cap * sizeof(Term));
  Term *nrs = (Term *)realloc(s->r_dead_rhs_save,  cap * sizeof(Term));
  u8   *ngj = (u8   *)realloc(s->r_gj_status,      cap * sizeof(u8));
  u8   *nod = (u8   *)realloc(s->r_overlap_done,   cap * sizeof(u8));
  Term *nsl = (Term *)realloc(s->r_dead_subsumer_lhs, cap * sizeof(Term));
  Term *nsr = (Term *)realloc(s->r_dead_subsumer_rhs, cap * sizeof(Term));
  u32  *nrc = (u32  *)realloc(s->r_rederive_cut,       cap * sizeof(u32));
  u32  *nru = (u32  *)realloc(s->r_uid,                cap * sizeof(u32));
  u32  *nrd = (u32  *)realloc(s->r_depth,              cap * sizeof(u32));
  u8   *ngd = (u8   *)realloc(s->r_gd,                 cap * sizeof(u8));
  if (nl == NULL || nr == NULL || nt == NULL || no == NULL ||
      nd == NULL || nls == NULL || nrs == NULL || ngj == NULL ||
      nod == NULL || nsl == NULL || nsr == NULL || nrc == NULL ||
      nru == NULL || nrd == NULL || ngd == NULL) {
    fprintf(stderr, "atp_ensure_rule_cap: realloc to %u rules failed\n",
            cap);
    exit(1);
  }
  s->lhs = nl; s->rhs = nr; s->r_trace = nt; s->r_orient = no;
  s->r_dead = nd; s->r_dead_lhs_save = nls; s->r_dead_rhs_save = nrs;
  s->r_gj_status = ngj; s->r_overlap_done = nod;
  s->r_dead_subsumer_lhs = nsl; s->r_dead_subsumer_rhs = nsr;
  s->r_rederive_cut = nrc;
  s->r_uid = nru;
  s->r_depth = nrd;
  s->r_gd = ngd;
  for (u32 i = s->r_cap; i < cap; i++) {
    s->r_uid[i] = 0u;
    s->r_depth[i] = 0u;
    s->r_gd[i] = 0u;
    s->r_trace[i] = ATP_TRACE_NONE;
    s->r_dead[i] = 0;
    s->r_dead_lhs_save[i] = 0;
    s->r_dead_rhs_save[i] = 0;
    s->r_gj_status[i] = ATP_GJ_ST_UNKNOWN;
    s->r_overlap_done[i] = 0;
    s->r_dead_subsumer_lhs[i] = 0;
    s->r_dead_subsumer_rhs[i] = 0;
    s->r_rederive_cut[i] = ATP_TRACE_NONE;
  }
#ifdef THVM_ATPFT_RULES
  // Stage 4: grow the parallel AtpFt slot arrays in lockstep with the
  // Term arrays above.  Cells themselves live in s->ft_arena_ptr's slab
  // pool (allocated once in thvm_atp_init); these slot arrays only hold
  // POINTERS, so realloc never invalidates an in-flight AtpFtCell *.
  // New tail slots start NULL: a rule's slot is filled by atp_push_rule
  // immediately after the Term writes (or stays NULL for the never-used
  // tail above n_rules).
  struct AtpFtCell **nlf  = (struct AtpFtCell **)realloc(
      s->lhs_ft,              cap * sizeof(struct AtpFtCell *));
  struct AtpFtCell **nrf  = (struct AtpFtCell **)realloc(
      s->rhs_ft,              cap * sizeof(struct AtpFtCell *));
  struct AtpFtCell **nlsf = (struct AtpFtCell **)realloc(
      s->r_dead_lhs_save_ft,  cap * sizeof(struct AtpFtCell *));
  struct AtpFtCell **nrsf = (struct AtpFtCell **)realloc(
      s->r_dead_rhs_save_ft,  cap * sizeof(struct AtpFtCell *));
  struct AtpFtCell **nsuf = (struct AtpFtCell **)realloc(
      s->r_dead_subsumer_lhs_ft, cap * sizeof(struct AtpFtCell *));
  struct AtpFtCell **nsvf = (struct AtpFtCell **)realloc(
      s->r_dead_subsumer_rhs_ft, cap * sizeof(struct AtpFtCell *));
  if (nlf == NULL || nrf == NULL || nlsf == NULL || nrsf == NULL ||
      nsuf == NULL || nsvf == NULL) {
    fprintf(stderr, "atp_ensure_rule_cap: ft realloc to %u rules failed\n",
            cap);
    exit(1);
  }
  s->lhs_ft = nlf; s->rhs_ft = nrf;
  s->r_dead_lhs_save_ft = nlsf; s->r_dead_rhs_save_ft = nrsf;
  s->r_dead_subsumer_lhs_ft = nsuf; s->r_dead_subsumer_rhs_ft = nsvf;
  for (u32 i = s->r_cap; i < cap; i++) {
    s->lhs_ft[i]             = NULL;
    s->rhs_ft[i]             = NULL;
    s->r_dead_lhs_save_ft[i] = NULL;
    s->r_dead_rhs_save_ft[i] = NULL;
    s->r_dead_subsumer_lhs_ft[i] = NULL;
    s->r_dead_subsumer_rhs_ft[i] = NULL;
  }
#endif
  s->r_cap = cap;
}

// Tiny env-gate helpers consolidating ~15 callsites that each rolled the
// same getenv-then-classify expression by hand.  Two flavors:
//
//   atp_env_on(name)  -> default-OFF gate.  Returns 1 iff the env var is
//                        set to a non-empty, non-"0" value.  Matches the
//                        historical idiom `(e != NULL && e[0] != '\0' &&
//                        e[0] != '0')`.
//   atp_env_off(name) -> default-ON  gate.  Returns 0 iff the env var is
//                        set to EXACTLY "0".  Anything else (unset, "1",
//                        "yes", "...") returns 1.  Matches the idiom
//                        `(e != NULL && e[0] == '0' && e[1] == '\0')
//                        ? 0 : 1`.
//
// Callers that want process-once caching still own the `static int gate =
// -1` slot; the helper only standardizes the classification.  Helpers that
// parse numeric values (THVM_ATP_TRACE_MAX, _HEAP_ABORT_FRAC, _RSS_ABORT_MB,
// _TICK_TRACE) and pure existence checks (THVM_ATP_ENQ_DEBUG and friends)
// stay open-coded -- their semantics differ enough that forcing the helper
// would obscure intent.
static int atp_env_on(const char *name) {
  const char *e = getenv(name);
  return (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
}
static int atp_env_off(const char *name) {
  const char *e = getenv(name);
  return (e != NULL && e[0] == '0' && e[1] == '\0') ? 0 : 1;
}

// r_overlap_done compaction (unconditional; the one-round
// THVM_ATP_OVERLAP_DONE_FIX kill switch is retired): the per-slot
// overlap-exhausted flag shifts in lockstep with the other rule arrays
// during slot compaction.  Without the shift the flags misalign after any
// rule removal, and the use_overlap_exhaust pair gate in
// atp_wmo_collect_pair fires on garbage state -- the WolframAxioms
// walk-vs-batch +12-CP divergence (pick 51667): the batch dropped 4
// (doubly-unfree eq x commutativity) overlaps WM forms (WM's A/D toplevel
// sweeps, Unifikation1.c:1595-1648, exclude only the new equation itself via
// MitAllenAusser :1471).  This is the WM-faithful bookkeeping; under it the
// walk and batch formers are full-CPSEL-stream byte-identical on OA.  The
// OA gate baselines are pinned to the fixed trajectory
// (steps=278807 rules=753 cps=2642990).

// S-expression-style term print to FILE *.  Variables print as "Vn",
// constants as "Cs", CTRs as "(s arg1 arg2 ...)".  Used by the env-
// gated CP-trace dump (THVM_ATP_CP_PICK_TRACE) to compare thvm's
// CP-selection trajectory against external provers' verbose dumps.
static void atp_dbg_print_term(FILE *fp, Term t) {
  if (t == 0) { fputs("()", fp); return; }
  u32 tag = term_tag(t);
  if (tag == TAG_FVR) { fprintf(fp, "V%u", term_ext(t)); return; }
  if (tag == TAG_NUM) { fprintf(fp, "#%u", term_ext(t)); return; }
  if (tag == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n == 0u) { fprintf(fp, "C%u", term_ext(t)); return; }
    fprintf(fp, "(C%u", term_ext(t));
    for (u32 i = 0; i < n; i++) {
      fputc(' ', fp);
      atp_dbg_print_term(fp, term_ctr_at(t, i));
    }
    fputc(')', fp);
    return;
  }
  fprintf(fp, "?%u/%u", tag, term_ext(t));
}

// Overlap geometry of the CP currently being pushed, set by the WM batch
// loop (thvm_atp_generate_cps_wm) immediately before atp_push_cps_traced and
// read back by atp_cp_form_trace so the CPFORM dump carries the SAME
// (parent-i, parent-j, face combo, overlap position) lineage Waldmeister's
// WM_CPFORMDUMP prints.  -1 = unset (e.g. the rule-database push path, axiom
// seeding) so the trace prints "-" there.  Single-threaded saturation, so a
// plain static carrier is safe.
static u32 g_cpform_i   = 0xffffffffu;
static u32 g_cpform_j   = 0xffffffffu;
static u32 g_cpform_itr = 0xffffffffu;
static u32 g_cpform_jtr = 0xffffffffu;
static u8  g_cpform_combo   = 0xffu;
static u8  g_cpform_i_or    = 0xffu;
static u8  g_cpform_j_or    = 0xffu;
static u8  g_cpform_pos_len = 0xffu;
static u8  g_cpform_pos[CP_MAX_DEPTH];
// Parent-rule LHS terms of the CP currently being pushed, carried alongside the
// slot/trace ids so a CPFORM line identifies its overlap parents by CONTENT
// (compaction-stable) -- the slots/trace ids are not stable across compaction,
// which blocks mapping thvm CP provenance onto Waldmeister's rule numbers.  0 =
// unset.  Read only by atp_cp_form_trace (early-returns when the trace is off).
static Term g_cpform_ilhs = 0;
static Term g_cpform_jlhs = 0;
// Actual overlap direction (lin = mother FROM face, rin = mother TO face) + the
// Vater subterm the walk unified, so a CPFORM line reveals which mother
// FACE/direction formed the CP.  jpar prints only the partner's stored lhs[j],
// which does NOT distinguish the l->r from the r->l overlap of an unorientable
// equation -- these fields do.  0 = unset (batch/axiom push path).
static Term g_cpform_lin = 0;
static Term g_cpform_rin = 0;
static Term g_cpform_sub = 0;

// WM Waisenmord rule-identity parenthood (see AtpState.r_uid): the two
// parent slots' uids of the CP batch currently being pushed, carried from
// atp_push_cps_traced to the heap/implicit push so they can be stamped
// into cp_uid_a/b.  0 = none (axiom intake, requeued facts).
static u32 g_cp_uid_a = 0u;
static u32 g_cp_uid_b = 0u;

// Mark a rule uid dead (WM KPV_KillParent on RE_RegelEntfernen /
// RE_GleichungEntfernen).  Grows the bitmap on demand; uid 0 = none.
static void atp_uid_mark_dead(AtpState *s, u32 uid) {
  if (uid == 0u || s->r_uid == NULL) return;
  if (uid >= s->uid_dead_cap) {
    u32 ncap = s->uid_dead_cap ? s->uid_dead_cap : 1024u;
    while (ncap <= uid) ncap *= 2u;
    u8 *nd = (u8 *)realloc(s->uid_dead, ncap);
    if (nd == NULL) return;   // OOM: skip the mark (orphan stays alive; sound)
    memset(nd + s->uid_dead_cap, 0, ncap - s->uid_dead_cap);
    s->uid_dead = nd;
    s->uid_dead_cap = ncap;
  }
  s->uid_dead[uid] = 1u;
  {
    static int rt = -1;
    if (rt < 0) rt = (getenv("THVM_ATP_RULE_TRACE") != NULL) ? 1 : 0;
    if (rt) fprintf(stderr, "UIDKILL uid=%u\n", uid);
  }
}

static inline int atp_uid_is_dead(const AtpState *s, u32 uid) {
  return uid != 0u && s->uid_dead != NULL && uid < s->uid_dead_cap
      && s->uid_dead[uid];
}

// uid -> rule-slot reverse map write (see AtpState.uid_idx).  Grows on
// demand; uid 0 = none.  OOM just skips the write -- the read side's
// verify + scan fallback keeps the answer exact either way.
static void atp_uid_idx_set(AtpState *s, u32 uid, u32 idx) {
  if (uid == 0u || s->r_uid == NULL) return;
  if (uid >= s->uid_idx_cap) {
    u32 ncap = s->uid_idx_cap ? s->uid_idx_cap : 1024u;
    while (ncap <= uid) ncap *= 2u;
    u32 *ni = (u32 *)realloc(s->uid_idx, (size_t)ncap * sizeof *ni);
    if (ni == NULL) return;
    memset(ni + s->uid_idx_cap, 0xff,
           (size_t)(ncap - s->uid_idx_cap) * sizeof *ni);
    s->uid_idx = ni;
    s->uid_idx_cap = ncap;
  }
  s->uid_idx[uid] = idx;
}

// Uid-layer orphan test for queue slot j: either stamped parent uid died.
// Default ON (THVM_ATP_UID_ORPHAN=0 disables): the KPV_KillParent
// call-site semantics are pinned from RUndEVerwaltung.c/Interreduktion.c
// (RE_RegelEntfernen kills unconditionally; the eq one-side retire kills
// unconditionally too -- the Gegenseite removal always takes the
// distinguished direction out of E; E-subsume and bwd-ground-join kill
// whole facts).  Gated validation 2026-07-02: WolframAxioms
// DoubleNegation 2849/2849 IDENTICAL and OrAssociativity 2977-prefix
// selection identity vs `wmcli -auto` with the layer on; the historical
// first-scoping regression (DN 795 -> 120) was the RETIRE-site
// over-kill, since corrected.
static int atp_uid_orphan_on(void) {
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("THVM_ATP_UID_ORPHAN");
    on = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return on;
}
static inline int atp_cp_uid_orphan(const AtpState *s, u32 j) {
  return atp_uid_orphan_on()
      && s->cp_uid_a != NULL
      && (atp_uid_is_dead(s, s->cp_uid_a[j]) ||
          atp_uid_is_dead(s, s->cp_uid_b[j]));
}

// THVM_ATP_GJ_KEEP: set just before atp_cp_heap_push for a ground-joinable
// CP that is being KEPT (not dropped) so insert_packed stamps cp_deprio[i]
// = 1.  A deprioritized CP gets priority ATP_CP_PRI_NEVER (heap sinks it,
// weight pick never reaches it) yet keeps its real cp_seq (FIFO ages it).
static u8  g_cp_push_deprio = 0u;
// Origin class the next CP push stamps into cp_kind (see the AtpState
// field).  CP-generation pushes leave it at the TRACE_CP default;
// atp_enqueue_equation arms it with its reason (TRACE_AXIOM /
// TRACE_SIMPLIFY) around its push and restores the default after.
static u8  g_cp_push_kind = (u8)TRACE_CP;
// Exact combined node count of the CP pair the NEXT (singleton)
// atp_push_cps_traced call receives, or 0 = unknown.  Set by
// sw_form_push from the unify-apply node-tally bracket immediately
// before its push call; consumed AND cleared at atp_push_cps_traced
// entry, so it can never apply to any other CP (the batch buf paths
// never set it and see 0 -> the historic counting walk).  Feeds the
// KPBehandelt size gate: verdict (hint < 50) == (capped re-walk < 50)
// by exactness.  THVM_NO_CP_SZHINT=1 ignores the hint (same-binary
// A/B); THVM_ATP_SZHINT_CHECK=1 runs BOTH and fatals on a verdict
// mismatch (certifier).
static u32 g_cp_rawsz_hint = 0u;
// CP FORMATION DEPTH for the CP currently being pushed (Twee getDepth):
// max(parent rule depths) + 1, set by atp_push_cps_traced from its
// rule_a/rule_b before the heap insert, consumed by (1) the insert to
// stamp cp_depth[i] and (2) the ATP_CP_WEIGHT_TWEE score in place of the
// syntactic term depth.  0 = no parents / unknown (an initial/axiom CP).
static u32 g_cp_formation_depth = 0u;
// GOAL-DERIVED bit for the CP currently being pushed (spb=goal
// derivation tracking, see AtpState.cp_gd): OR of the parent rules'
// r_gd, set by atp_push_cps_traced exactly like g_cp_formation_depth;
// axiom/requeue enqueues consume g_enq_gd_pending instead (seeded by
// thvm_atp_mark_gd_next / the interreduce victim's r_gd).
static u8 g_cp_gd = 0u;
static u8 g_enq_gd_pending = 0u;
// "Never-weight-select" priority (Waldmeister Act_never / maximalWeight).
// High but far from u32 overflow so cp_pri arithmetic stays safe.
#define ATP_CP_PRI_NEVER 0x40000000u

static void atp_cpform_geom_clear(void) {
  g_cpform_i = g_cpform_j = g_cpform_itr = g_cpform_jtr = 0xffffffffu;
  g_cpform_combo = g_cpform_i_or = g_cpform_j_or = g_cpform_pos_len = 0xffu;
  g_cp_uid_a = g_cp_uid_b = 0u;
  g_cpform_ilhs = g_cpform_jlhs = 0;
}

// Match the firstdiv-11 carrier face x.((x.x).x) = (op v (op (op v v) v)),
// all one variable.  Read-only diagnostic helper for THVM_ATP_W367_TRACE.
static u8 atp_term_is_x_xx_x(Term t, u32 *out_v) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term a = term_ctr_at(t, 0), b = term_ctr_at(t, 1);
  if (term_tag(a) != TAG_FVR) return 0u;
  if (term_tag(b) != TAG_CTR || term_ext(b) != term_ext(t) || term_ctr_n(b) != 2u) return 0u;
  Term b0 = term_ctr_at(b, 0), b1 = term_ctr_at(b, 1);
  if (term_tag(b1) != TAG_FVR) return 0u;
  if (term_tag(b0) != TAG_CTR || term_ext(b0) != term_ext(t) || term_ctr_n(b0) != 2u) return 0u;
  u32 v = term_ext(a);
  if (term_ext(b1) != v) return 0u;
  if (term_tag(term_ctr_at(b0, 0)) != TAG_FVR || term_ext(term_ctr_at(b0, 0)) != v) return 0u;
  if (term_tag(term_ctr_at(b0, 1)) != TAG_FVR || term_ext(term_ctr_at(b0, 1)) != v) return 0u;
  *out_v = v; return 1u;
}
// Gated classification-order trace (env THVM_ATP_CP_FORM_TRACE).  Emits
// one CPFORM line per CP at the moment cp_seq is stamped, so thvm's
// CP-classification order can be aligned against Waldmeister's verbose
// `... added to SUE: w, age` sequence and the first cross-batch
// age-order divergence localized.  Carries the overlap geometry set by the
// batch push loop (g_cpform_*) so a thvm CPFORM line can be diffed against a
// WM WM_CPFORMDUMP line by (combo, position, parents) on the same rawCP.  Off
// by default; one cached env probe.
// Targeted CP-fate watch (THVM_ATP_CP_WATCH_SEQ="131,220,..."): print a
// line whenever a watched cp_seq is silently deleted (IR-sweep drop
// branches), so a formed-but-never-popped CP's fate is attributable
// without a full queue dump.  Output-only; one cached env probe.
static void atp_cp_watch_seq(u32 seq, const char *what) {
  static long watch[16];
  static int n_watch = -1;
  if (n_watch < 0) {
    n_watch = 0;
    const char *e = getenv("THVM_ATP_CP_WATCH_SEQ");
    while (e != NULL && *e != '\0' && n_watch < 16) {
      while (*e != '\0' && (*e < '0' || *e > '9')) e++;
      if (*e == '\0') break;
      watch[n_watch++] = strtol(e, (char **)&e, 10);
    }
  }
  for (int i = 0; i < n_watch; i++)
    if ((long)seq == watch[i])
      fprintf(stderr, "CPWATCH seq=%u %s\n", seq, what);
}

static void atp_cp_form_trace(u32 seq, u32 w, Term lhs, Term rhs) {
  static int on = -1;
  static int w367 = -1;
  static long seq_lo = -1, seq_hi = -1;
  if (on < 0) {
    on = (getenv("THVM_ATP_CP_FORM_TRACE") != NULL) ? 1 : 0;
    // Optional seq window (THVM_ATP_CP_FORM_SEQ_LO/HI): print only CPs whose
    // cp_seq falls in [lo,hi] so a deep-run batch can be dumped without the
    // full multi-hundred-MB trace.  Output-only; no engine effect.
    const char *lo = getenv("THVM_ATP_CP_FORM_SEQ_LO");
    const char *hi = getenv("THVM_ATP_CP_FORM_SEQ_HI");
    seq_lo = lo ? atol(lo) : -1;
    seq_hi = hi ? atol(hi) : -1;
  }
  if (w367 < 0) w367 = (getenv("THVM_ATP_W367_TRACE") != NULL) ? 1 : 0;
  if (!on && !w367) return;
  if (seq_lo >= 0 && (long)seq < seq_lo) return;
  if (seq_hi >= 0 && (long)seq > seq_hi) return;
  if (w367 && !on) {
    u32 v = 0; u8 hit = atp_term_is_x_xx_x(lhs, &v) || atp_term_is_x_xx_x(rhs, &v);
    if (!hit) return;
  }
  char posbuf[64];
  if (g_cpform_pos_len == 0xffu) {
    posbuf[0] = '-'; posbuf[1] = '\0';
  } else {
    u32 pn = 0;
    posbuf[pn++] = 'L';
    for (u32 d = 0; d < g_cpform_pos_len && pn < 60u; d++)
      pn += (u32)snprintf(posbuf + pn, sizeof posbuf - pn, ".%u",
                          g_cpform_pos[d] + 1u);
    posbuf[pn] = '\0';
  }
  fprintf(stderr,
          "CPFORM seq=%u w=%u i=%d j=%d itr=%d jtr=%d i_or=%d j_or=%d "
          "combo=%d pos=%s lhs=",
          seq, w, (int)g_cpform_i, (int)g_cpform_j,
          (int)g_cpform_itr, (int)g_cpform_jtr,
          (g_cpform_i_or == 0xffu) ? -1 : (int)g_cpform_i_or,
          (g_cpform_j_or == 0xffu) ? -1 : (int)g_cpform_j_or,
          (g_cpform_combo == 0xffu) ? -1 : (int)g_cpform_combo, posbuf);
  atp_dbg_print_term(stderr, lhs);
  fprintf(stderr, " rhs=");
  atp_dbg_print_term(stderr, rhs);
  if (g_cpform_ilhs != 0) {
    fprintf(stderr, " ipar=");
    atp_dbg_print_term(stderr, g_cpform_ilhs);
  }
  if (g_cpform_jlhs != 0) {
    fprintf(stderr, " jpar=");
    atp_dbg_print_term(stderr, g_cpform_jlhs);
  }
  if (g_cpform_lin != 0) {   // actual mother direction (lin->rin) + Vater subterm
    fprintf(stderr, " lin="); atp_dbg_print_term(stderr, g_cpform_lin);
    fprintf(stderr, " rin="); atp_dbg_print_term(stderr, g_cpform_rin);
    fprintf(stderr, " sub="); atp_dbg_print_term(stderr, g_cpform_sub);
  }
  fputc('\n', stderr);
}

#ifdef THVM_ATPFT_RULES
// Stage 4 verification probe (env THVM_ATPFT_VERIFY=1).  After every
// rule slot k is written, re-convert the live Term pair through the
// scratch arena and confirm structural equality + hash agreement with
// the persistent AtpFt mirror written in atp_push_rule.  A divergence
// here means atp_push_rule's slot-write semantics drifted from the
// boundary converter -- abort loudly so a soundness bug surfaces at
// the mutating call, not laundered into a downstream Stage-5 reader.
//
// The probe is cheap when off (one env-probe-cached flag check) and
// O(rule-size) per slot when on, so a flagged 135k-assertion run is
// only ~10-20% slower; off it is byte-identical.
static int atp_ft_rules_verify_on(void) {
  static int on = -1;
  if (on < 0) on = atp_env_on("THVM_ATPFT_VERIFY");
  return on;
}

static void atp_ft_rules_verify_push(AtpState *s, u32 k) {
  if (!atp_ft_rules_verify_on()) return;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Scratch round-trip: ft_from_term into Arena B with scratch=1, then
  // reset.  Scratch cells are short-lived by construction, so this
  // never grows the persistent arena -- the probe is allocation-neutral
  // for the live rule mirror.
  ft_scratch_reset(a);
  AtpFtCell *l_check = ft_from_term(a, s->lhs[k], 1);
  AtpFtCell *r_check = ft_from_term(a, s->rhs[k], 1);
  if (!ft_eq(s->lhs_ft[k], l_check) || !ft_eq(s->rhs_ft[k], r_check)) {
    fprintf(stderr, "ATPFT VERIFY: rule %u ft_eq mismatch\n", k);
    abort();
  }
  u64 hl_ft = ft_hash(s->lhs_ft[k]);
  u64 hr_ft = ft_hash(s->rhs_ft[k]);
  u64 hl_tm = atp_term_struct_hash(s->lhs[k]);
  u64 hr_tm = atp_term_struct_hash(s->rhs[k]);
  if (hl_ft != hl_tm || hr_ft != hr_tm) {
    fprintf(stderr,
            "ATPFT VERIFY: rule %u ft_hash mismatch "
            "(lhs ft=%llx tm=%llx, rhs ft=%llx tm=%llx)\n", k,
            (unsigned long long)hl_ft, (unsigned long long)hl_tm,
            (unsigned long long)hr_ft, (unsigned long long)hr_tm);
    abort();
  }
  ft_scratch_reset(a);
}
#endif // THVM_ATPFT_RULES

// Grow the CP arrays (cp_packed / cp_trace / cp_pri / cp_seq) to hold
// at least `need` entries.  Same doubling discipline as
// atp_ensure_rule_cap.  New cp_packed slots are NULL-initialised so
// thvm_atp_cp_set / thvm_atp_free can tell an unused slot apart from a
// live packed buffer.
static void atp_ensure_cp_cap(AtpState *s, u32 need) {
  if (need <= s->cp_cap) return;
  u32 old_cap = s->cp_cap;
  u32 cap = s->cp_cap ? s->cp_cap : ATP_INIT_CPS;
  while (cap < need) cap *= 2;
  u8  **nc = (u8 **)realloc(s->cp_packed, cap * sizeof(u8 *));
  u32  *nt = (u32  *)realloc(s->cp_trace, cap * sizeof(u32));
  u32  *np = (u32  *)realloc(s->cp_pri,   cap * sizeof(u32));
  u32  *nq = (u32  *)realloc(s->cp_seq,   cap * sizeof(u32));
  u32  *ncd = (u32 *)realloc(s->cp_depth, cap * sizeof(u32));
  u8   *ngd = (u8  *)realloc(s->cp_gd,    cap * sizeof(u8));
  u32  *ng = (u32  *)realloc(s->cp_goal,  cap * sizeof(u32));
  u32  *nsn = (u32 *)realloc(s->cp_sine,  cap * sizeof(u32));
  u32  *np2 = (u32 *)realloc(s->cp_pri2,  cap * sizeof(u32));
  u32  *nlnr = (u32 *)realloc(s->cp_last_norm_r_revision,
                              cap * sizeof(u32));
  u8   *ndp = (u8  *)realloc(s->cp_deprio, cap * sizeof(u8));
  u8   *nk  = (u8  *)realloc(s->cp_kind,   cap * sizeof(u8));
  // Heap-position mirror (cp_heap: pos -> packed node caching the
  // row's sift keys, cp_pos: row -> pos).  No tail init required: only
  // [0, n_cps) is ever read, and every append / direct set stamps its
  // own entry (atp_cp_heap_set).
  AtpCpHeapNode *nh = (AtpCpHeapNode *)realloc(s->cp_heap,
                                               cap * sizeof(AtpCpHeapNode));
  u32  *npo = (u32 *)realloc(s->cp_pos,    cap * sizeof(u32));
  if (nc == NULL || nt == NULL || np == NULL || nq == NULL || ng == NULL
      || nsn == NULL || np2 == NULL || nlnr == NULL || ndp == NULL
      || nk == NULL || nh == NULL || npo == NULL || ncd == NULL
      || ngd == NULL) {
    fprintf(stderr, "atp_ensure_cp_cap: realloc to %u CPs failed\n", cap);
    exit(1);
  }
  s->cp_packed = nc; s->cp_trace = nt;
  s->cp_pri = np; s->cp_seq = nq; s->cp_goal = ng; s->cp_sine = nsn;
  s->cp_pri2 = np2;
  s->cp_depth = ncd;
  s->cp_gd = ngd;
  memset(s->cp_gd + old_cap, 0, (size_t)(cap - old_cap) * sizeof(u8));
  s->cp_last_norm_r_revision = nlnr;
  s->cp_deprio = ndp;
  s->cp_kind = nk;
  s->cp_heap = nh;
  s->cp_pos = npo;
  memset(s->cp_deprio + old_cap, 0, (size_t)(cap - old_cap) * sizeof(u8));
  memset(s->cp_kind + old_cap, TRACE_CP, (size_t)(cap - old_cap) * sizeof(u8));
  // Deferred-CP (`implicit_pair`) arc: lazy-grow the parallel
  // AtpCpImplicit descriptor array + the per-slot tag bitset only when
  // already allocated (atp_cp_implicit_push allocates both on the first
  // deferred push).  Under use_implicit_cp == 0 (default) both stay
  // NULL through the lifetime of the AtpState and this realloc loop is
  // a no-op -- the engine path is byte-identical.
  if (s->cp_implicit != NULL) {
    AtpCpImplicit *ni = (AtpCpImplicit *)realloc(s->cp_implicit,
                                                 cap * sizeof(AtpCpImplicit));
    if (ni == NULL) {
      fprintf(stderr, "atp_ensure_cp_cap: realloc cp_implicit to %u failed\n",
              cap);
      exit(1);
    }
    s->cp_implicit = ni;
    for (u32 i = s->cp_cap; i < cap; i++) {
      s->cp_implicit[i].parent_a_trace_id = ATP_TRACE_NONE;
      s->cp_implicit[i].parent_b_trace_id = ATP_TRACE_NONE;
      s->cp_implicit[i].overlap_position  = 0;
      s->cp_implicit[i].weight            = 0u;
      s->cp_implicit[i].priority          = 0u;
    }
  }
  if (s->cp_is_implicit != NULL) {
    u32 new_bytes = (cap + 7u) / 8u;
    u32 old_bytes = (s->cp_cap + 7u) / 8u;
    u8 *nb = (u8 *)realloc(s->cp_is_implicit, new_bytes);
    if (nb == NULL) {
      fprintf(stderr,
              "atp_ensure_cp_cap: realloc cp_is_implicit to %u bytes failed\n",
              new_bytes);
      exit(1);
    }
    s->cp_is_implicit = nb;
    if (new_bytes > old_bytes) {
      memset(s->cp_is_implicit + old_bytes, 0, new_bytes - old_bytes);
    }
  }
  // Lazy-grow cp_ultimate only when the flag is on; engine byte-
  // identical when off (zero overhead).
  if (s->use_initial_ultimate) {
    u8 *nu = (u8  *)realloc(s->cp_ultimate, cap * sizeof(u8));
    if (nu == NULL) {
      fprintf(stderr, "atp_ensure_cp_cap: realloc cp_ultimate to %u failed\n",
              cap);
      exit(1);
    }
    s->cp_ultimate = nu;
    for (u32 i = s->cp_cap; i < cap; i++) s->cp_ultimate[i] = 0u;
  }
  // Lazy-grow the WM Waisenmord uid stamps only under orphan murder
  // (see AtpState.r_uid); NULL (engine byte-identical) otherwise.
  if (s->use_orphan_murder) {
    u8 was_null = (s->cp_uid_a == NULL);
    u32 *nua = (u32 *)realloc(s->cp_uid_a, cap * sizeof(u32));
    u32 *nub = (u32 *)realloc(s->cp_uid_b, cap * sizeof(u32));
    if (nua == NULL || nub == NULL) {
      fprintf(stderr, "atp_ensure_cp_cap: realloc cp_uid to %u failed\n", cap);
      exit(1);
    }
    s->cp_uid_a = nua;
    s->cp_uid_b = nub;
    for (u32 i = was_null ? 0u : s->cp_cap; i < cap; i++) {
      s->cp_uid_a[i] = 0u;
      s->cp_uid_b[i] = 0u;
    }
  }
  for (u32 i = s->cp_cap; i < cap; i++) {
    s->cp_packed[i] = NULL;
    s->cp_trace[i]  = ATP_TRACE_NONE;
    // Fresh slot has never been normalized -- the IR sweep's
    // last-norm cookie fast-path stays inert until a fixpoint
    // normalize stamps a real n_rules value here.
    s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  }
#ifdef THVM_ATPFT_CPQ
  // Stage 7: grow the parallel AtpFt queue alongside the legacy one.
  // The fresh tail is NULL-initialised by atp_cp_ft_ensure_cap so the
  // slot-occupied invariant matches cp_packed[i].
  atp_cp_ft_ensure_cap(s, cap, old_cap);
#else
  (void)old_cap;
#endif
  s->cp_cap = cap;
}

// Stamp heap position `pos` with payload row `row`, refreshing the
// packed comparator-key caches from the payload arrays (the
// AtpCpHeapNode refresh invariant, thvm.h).  This is THE canonical
// cache-refresh point: every site that (re)binds a position to a row
// after writing that row's cp_pri / cp_seq / cp_ultimate calls it.
// Sites that move a node between positions with the row's keys
// UNTOUCHED instead copy the whole node (swap / backfill) or rewire
// only .row (row moves -- atp_cp_row_move copies the keys verbatim, so
// the caches stay exact).
static inline void atp_cp_heap_set(AtpState *s, u32 pos, u32 row) {
  AtpCpHeapNode *n = &s->cp_heap[pos];
  n->pri = s->cp_pri[row];
  n->seq = s->cp_seq[row];
  n->row = row;
  n->aux = (s->cp_ultimate != NULL) ? (u32)s->cp_ultimate[row] : 0u;
}

// === Deferred-CP (`implicit_pair`) per-slot tag-bit bookkeeping =====
//
// The invariant is `cp_is_implicit[i] set <=> cp_implicit[i] live AND
// cp_packed[i] == NULL` (thvm.h AtpCpImplicit header).  Every slot
// drop / move / swap site below maintains it through these helpers;
// all are NULL-safe so the flag-OFF engine (both arrays NULL) pays a
// single predictable branch.

static inline u8 atp_cp_slot_implicit(const AtpState *s, u32 i) {
  return s->cp_is_implicit != NULL
      && ((s->cp_is_implicit[i >> 3] >> (i & 7u)) & 1u);
}

static inline void atp_cp_implicit_set(AtpState *s, u32 i) {
  s->cp_is_implicit[i >> 3] |= (u8)(1u << (i & 7u));
}

static inline void atp_cp_implicit_clear(AtpState *s, u32 i) {
  if (s->cp_is_implicit != NULL) {
    s->cp_is_implicit[i >> 3] &= (u8)~(1u << (i & 7u));
  }
}

// Move the descriptor + tag bit from slot `src` into slot `dst`,
// vacating `src` (compaction / backfill).  No-op when dst == src.
static inline void atp_cp_implicit_move(AtpState *s, u32 dst, u32 src) {
  if (s->cp_is_implicit == NULL || dst == src) return;
  if (s->cp_implicit != NULL) s->cp_implicit[dst] = s->cp_implicit[src];
  if (atp_cp_slot_implicit(s, src)) atp_cp_implicit_set(s, dst);
  else                              atp_cp_implicit_clear(s, dst);
  atp_cp_implicit_clear(s, src);
}

// (A heap sift no longer touches the descriptors: sifts swap only the
// cp_heap/cp_pos position mirror, so a slot's descriptor + tag bit stay
// put with its stationary payload row.)

// === Raw-descriptor lane (use_raw_desc / use_raw_desc_cert) =========
//
// The WM preset's raw >= 50 KPBehandelt class is pushed through the
// walk former's FT-direct emit (sw_form_push_ft_try -> vcp_ft_emit_
// fused): the packed byte string is serialized straight from the two
// parent face spans + the unifier, no heap Terms.  This lane stores,
// for exactly that class, a 20-byte descriptor (two interned face-pair
// refs + the overlap subterm's preorder index) instead of the malloc'd
// byte string, and re-runs the identical rename/unify/emit pipeline at
// pop to recover byte-exact packed bytes.  Soundness of the pointer
// capture: rule-face mirror spans are content-frozen for the state's
// lifetime -- a drop moves them to the r_dead_*_save_ft slots, an RHS
// compose abandons the old span in Arena A ("freed wholesale at
// ft_destroy time"), and nothing else writes a live face's cells -- so
// the refs re-derive the BIRTH-time pair even after parent mutation.
#define ATP_RDESC_NONE 0xFFFFFFFFu
typedef struct {
  u8  armed;      // consume-once: armed by sw_form_push_ft_try beside g_vcp_emit
  u32 out_face;   // interned (lo, ro) of the outer parent, as consumed
  u32 in_face;    // interned (lin, rin) of the inner parent, pre-rename
  u32 sub_pre;    // preorder cell index of the overlap subterm in lo
  u32 len;        // packed byte length of the emit (cert-mode memcmp bound)
} AtpRdescPush;
static AtpRdescPush g_rdesc_push;   // armed at formation (walk former)
static AtpRdescPush g_rdesc_store;  // handed to atp_cp_heap_insert_packed
static u32 atp_rdesc_intern(AtpState *s, const struct AtpFtCell *lo,
                            const struct AtpFtCell *ro);
static u8 *atp_rdesc_materialize(AtpState *s, const AtpCpImplicit *d,
                                 u32 *len_out, u32 *nodes_out,
                                 u32 *lhs_nodes_out);
static void atp_cp_rdesc_pop_fix(AtpState *s, u32 v);
static void atp_cp_implicit_arrays_ensure(AtpState *s);

// 7c': push one CP onto the binary min-heap CP queue.  Defined
// below (after atp_cp_priority); forward-declared here so the
// earlier add_equation push site can call it.  `is_ultimate`=1
// tags the CP for Waldmeister Act_ultimate (initial-axiom front)
// ranking; effective only when s->use_initial_ultimate is on.
// `raw_untreated`=1 marks the WM KPBehandelt >=50 raw class, which
// bypasses the auto-MaxWeight stash (see the definition).
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace,
                             u8 is_ultimate, u8 raw_untreated);

// Periodic full-rule-set CP-queue interreduction (Waldmeister
// KPV_KPMengeInterreduzieren).  Defined far below (it needs the
// normalizer + reheapify); forward-declared for the thvm_atp_step call.
// The period (run every Nth rule addition) lives here so the call site
// can name it.
#define ATP_CP_SET_IR_PERIOD 16u
static void atp_cp_set_interreduce(AtpState *s);
// Twee task discipline for the sweep (cp_set_ir_budget > 0): frequency,
// budget and sampled-dead-fraction gates (Twee Task.hs taskDue +
// Twee.hs:978 newTask 10 / shouldSimplifyQueue).  Defined next to the
// sweep; forward-declared for the thvm_atp_step call site.
#define ATP_CP_SET_IR_FREQ_US    10000000ull  // Twee newTask freq = 10 s
#define ATP_CP_SET_IR_THRESHOLD  20u  // Twee cfg_renormalise_threshold %
#define ATP_CP_SET_IR_SAMPLE     100u // Twee cfg_cp_sample_size
static u8 atp_cp_set_ir_due(AtpState *s);
// WM doR-only Term-side normalize (defined with the interreduce block
// far below); forward-declared for the use_pop_rules_only pop arm.
static Term atp_rules_only_normalize(AtpState *s, Term t, u32 cap);

// === Waldmeister Stringterms: packed byte-string critical pairs =====
//
// A direct port of Waldmeister's `Stringterms` module (sources/TPR/
// Stringterms.c).  Waldmeister keeps its set of unselected equations
// not as heap term-graphs but as PACKED PREORDER BYTE STRINGS -- one
// `PTermpaarT = byte *` per critical pair.  A 30-symbol CP is a
// ~30-byte string in plain malloc memory: it never enters the IC heap,
// so the copying collector never touches it.  thvm's CP queue stores
// CPs as IC heap terms instead; at `thm` step ~230 that queue is a
// ~62M-cell live set the GC re-copies every collection (the late-game
// wall the loop hit at iters 16-17).
//
// thvm has no global symbol table (Waldmeister's SO_Stelligkeit gives
// a symbol's arity), so each CTR node packs its own arity -- otherwise
// this is Waldmeister's technique verbatim: walk the pair in preorder,
// emit one self-delimiting record per node, rebuild from arity.
//
// `acp_pack` returns a malloc'd buffer (caller frees); `acp_unpack`
// rebuilds the two heap Terms.  The CP queue (`cp_packed[]`) and the
// subsumption-index records hold these byte strings directly; the
// collector roots neither, which is what frees the late-game heap.

// LEB128 varint -- 7 bits/byte, high bit = "more".
static void acp_put_varint(u8 **pp, u64 v) {
  while (v >= 0x80u) { *(*pp)++ = (u8)(v | 0x80u); v >>= 7; }
  *(*pp)++ = (u8)v;
}
static u64 acp_get_varint(const u8 **pp) {
  u64 v = 0; u32 shift = 0; u8 b;
  do { b = *(*pp)++; v |= (u64)(b & 0x7Fu) << shift; shift += 7u; }
  while (b & 0x80u);
  return v;
}

// Persistent pack scratch: the pack walk emits into this geometrically
// grown buffer, then acp_pack copies the EXACT length into the returned
// malloc.  vs the historical acp_packed_bound pre-walk + 21-bytes/node
// over-allocation this saves one full tree walk per CP AND right-sizes
// the queue's resident byte strings (~3 bytes/node typical vs 21 -- the
// multi-million-CP queue was holding ~7x dead malloc slack).
// Single-threaded saturation, so the shared scratch is safe.
static u8 *g_acp_scratch     = NULL;
static u32 g_acp_scratch_cap = 0u;

// Ensure `extra` more bytes fit at scratch position `pos`.
static void acp_scratch_ensure(u32 pos, u32 extra) {
  if (pos + extra <= g_acp_scratch_cap) return;
  u32 ncap = g_acp_scratch_cap ? g_acp_scratch_cap : 4096u;
  while (pos + extra > ncap) ncap *= 2u;
  u8 *nb = (u8 *)realloc(g_acp_scratch, ncap);
  if (nb == NULL) thvm_fatal("acp_scratch_ensure: OOM");
  g_acp_scratch = nb;
  g_acp_scratch_cap = ncap;
}

// Append `t`'s preorder packing to the scratch at `*pos`.  CTR: 'C',
// label, arity, then children; FVR: 'V', var id; NUM: 'N', dtype, raw
// value; anything else: 'E' (era placeholder -- ATP terms are
// first-order, so this is unreachable in practice but keeps unpack
// total).  Each node reserves its own worst case (1 discriminator + 2
// varints <= 21 bytes) before writing.  `*nodes` is bumped once per
// node visited -- the pack walks every node anyway, so the CP's symbol
// count (its selection weight) falls out for free, sparing
// atp_cp_priority a second full traversal.
static void acp_pack_term(Term t, u32 *pos, u32 *nodes) {
  (*nodes)++;
  acp_scratch_ensure(*pos, 21u);
  u8 *p = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      *p++ = (u8)'C';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_acp_scratch);
      for (u32 i = 0; i < n; i++) acp_pack_term(term_ctr_at(t, i), pos, nodes);
      return;
    }
    case TAG_NUM:
      *p++ = (u8)'N';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, term_val(t));
      break;
    case TAG_FVR:
      *p++ = (u8)'V';
      acp_put_varint(&p, term_ext(t));
      break;
    default:
      *p++ = (u8)'E';
      break;
  }
  *pos = (u32)(p - g_acp_scratch);
}

// Rebuild one preorder-packed term, advancing `*pp` past it.
static Term acp_unpack_term(const u8 **pp) {
  if (g_atp_phase_detail) { extern u64 g_acp_unterm_nodes; g_acp_unterm_nodes++; }
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      Term kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) kids[i] = acp_unpack_term(pp);
      return term_new_ctr(label, kids, n);
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      return term_new(0, TAG_NUM, dtype, val);
    }
    case 'V':
    default:
      return term_new_fvr((u32)acp_get_varint(pp));
  }
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span sibling of acp_unpack_term: build the packed preorder record
// directly as ftnew_* cells -- no Term hop, so reading a packed trace
// pair into the span world costs zero interchange.  Total: the NUM
// case builds the ATPFT_NUM_MARKER pair exactly as ft_from_term
// would (never present in a rule pair, but the format allows it).
static AtpFtCell *acp_unpack_ft(AtpFt *a, const u8 **pp);
static AtpFtCell *acp_unpack_ft(AtpFt *a, const u8 **pp) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      if (n == 0u) return ftnew_const(a, label, 0);
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) kids[i] = acp_unpack_ft(a, pp);
      return ftnew_ctr(a, label, (u16)n, kids, 0);
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      AtpFtCell *kids[2] = {
        ftnew_const(a, dtype & ~WF_VAR_BIT, 0),
        ftnew_const(a, (u32)val & ~WF_VAR_BIT, 0),
      };
      return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, 0);
    }
    case 'V':
    default:
      return ftnew_var(a, (u32)acp_get_varint(pp), 0);
  }
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Pack a critical pair (lhs, rhs) into a fresh malloc'd byte string;
// `*out_len` receives its length, `*out_nodes` the total symbol count
// of lhs+rhs (the CP's selection weight).  Either out-param may be
// NULL.  The two terms pack back to back -- the preorder records are
// self-delimiting via arity, so acp_unpack reads lhs then rhs with no
// separator.
// `out_lhs_nodes` (optional) receives the LHS-only node count -- the
// per-side symbol counts the CP-weight modes (MAX/GT/MIX/MIX2/UNIF)
// would otherwise re-walk both terms for (identical values: both count
// one per node).
static u8 *acp_pack_ex(Term lhs, Term rhs, u32 *out_len, u32 *out_nodes,
                       u32 *out_lhs_nodes) {
  u32 pos = 0u, nodes = 0u;
  acp_pack_term(lhs, &pos, &nodes);
  if (out_lhs_nodes != NULL) *out_lhs_nodes = nodes;
  acp_pack_term(rhs, &pos, &nodes);
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("acp_pack: OOM");
  memcpy(buf, g_acp_scratch, pos);
  if (out_len   != NULL) *out_len   = pos;
  if (out_nodes != NULL) *out_nodes = nodes;
  return buf;
}

static u8 *acp_pack(Term lhs, Term rhs, u32 *out_len, u32 *out_nodes) {
  return acp_pack_ex(lhs, rhs, out_len, out_nodes, NULL);
}

// Preorder node count of one term -- the exact tally acp_pack_term
// accrues (one ++ per node: CTR head + every child, NUM, FVR), but
// without touching the scratch buffer.  Used by the byte-queue-
// retirement push path to recover cp_nodes / lhs_nodes when the byte
// pack is skipped (nothing reads cp_packed under that config).
static u32 acp_count_term(Term t) {
  u32 n = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 k = term_ctr_n(t);
    for (u32 i = 0; i < k; i++) n += acp_count_term(term_ctr_at(t, i));
  }
  return n;
}

#ifdef THVM_ATPFT_RULES
// Decode one packed preorder term STRAIGHT into AtpFt cells -- the
// byte-side sibling of ft_from_term(acp_unpack_term(pp)) with the
// heap-Term intermediate removed.  Builds through the same ftnew_*
// constructors ft_from_term_rec uses, so every cell (sym / arity /
// flags / next / end threading, NUM-marker encoding, GROUND folding)
// is bit-identical to the two-hop route; only the heap Terms are
// never materialised.  The pop-time treatment uses this to source
// the mixmost normalize input from the queue's byte string with zero
// ft_from_term.
static AtpFtCell *acp_unpack_term_ft(AtpFt *a, const u8 **pp, int scratch) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n == 0u) return ftnew_const(a, label, scratch);
      enum { KIDS_STACK = 16 };
      AtpFtCell *kids_stack[KIDS_STACK];
      AtpFtCell **kids      = kids_stack;
      AtpFtCell **kids_heap = NULL;
      if (n > KIDS_STACK) {
        kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)n);
        if (kids_heap == NULL) thvm_fatal("acp_unpack_term_ft: kids OOM");
        kids = kids_heap;
      }
      for (u32 i = 0; i < n; i++) kids[i] = acp_unpack_term_ft(a, pp, scratch);
      AtpFtCell *c = ftnew_ctr(a, label, (u16)n, kids, scratch);
      free(kids_heap);
      return c;
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      AtpFtCell *dt_cell  = ftnew_const(a, dtype & ~WF_VAR_BIT, scratch);
      AtpFtCell *raw_cell = ftnew_const(a, ((u32)val) & ~WF_VAR_BIT, scratch);
      AtpFtCell *kids[2]  = { dt_cell, raw_cell };
      return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, scratch);
    }
    case 'V':
    default:
      return ftnew_var(a, (u32)acp_get_varint(pp), scratch);
  }
}

// FT sibling of thvm_normalize_vars: canonically renumber the pair's
// variables IN PLACE to a dense [0, k) set shared across both sides,
// first-occurrence order over the lhs-then-rhs preorder -- which on a
// flatterm is simply the linear span order, so one pass over each span
// suffices.  Callers own the spans (clones); adoption proves the
// result against the Term-side canonical pair with ft_eq_term, so an
// edge divergence (e.g. > REWRITE_MAX_VAR distinct vars, where the
// Term side keeps original ids) can only cause a fallback, never a
// wrong mirror.
static void ft_normalize_vars_pair(AtpFtCell *l, AtpFtCell *r) {
  u32 old_id[REWRITE_MAX_VAR];
  u32 n = 0u;
  AtpFtCell *span[2] = { l, r };
  for (u32 t = 0; t < 2u; t++) {
    AtpFtCell *e = (span[t]->end != NULL) ? span[t]->end->next : NULL;
    for (AtpFtCell *p = span[t]; p != e; p = p->next) {
      if ((p->sym & WF_VAR_BIT) == 0u) continue;
      u32 id = p->sym & ~WF_VAR_BIT;
      u32 k = 0u;
      for (; k < n; k++) {
        if (old_id[k] == id) break;
      }
      if (k == n) {
        if (n >= REWRITE_MAX_VAR) return;   // overflow: leave the rest
        old_id[n++] = id;
      }
      p->sym = WF_VAR_BIT | k;
    }
  }
}

// Distinct-variable count across a span pair (capped at
// REWRITE_MAX_VAR -- callers only test the cliff).
static u32 ft_distinct_var_count(const AtpFtCell *l, const AtpFtCell *r) {
  u32 ids[REWRITE_MAX_VAR];
  u32 n = 0u;
  const AtpFtCell *span[2] = { l, r };
  for (u32 t = 0; t < 2u; t++) {
    const AtpFtCell *e = (span[t]->end != NULL) ? span[t]->end->next : NULL;
    for (const AtpFtCell *p = span[t]; p != e; p = p->next) {
      if ((p->sym & WF_VAR_BIT) == 0u) continue;
      u32 id = p->sym & ~WF_VAR_BIT;
      u32 k = 0u;
      for (; k < n; k++) {
        if (ids[k] == id) break;
      }
      if (k == n) {
        if (n >= REWRITE_MAX_VAR) return REWRITE_MAX_VAR;
        ids[n++] = id;
      }
    }
  }
  return n;
}

// Pop-time FT stash: thvm_atp_select_cp decodes the selected packed
// CP's byte string into two fresh FT spans (bytes -> cells, no Term
// hop) and parks them here for the step function's mixmost pop
// treatment.  NULL when the pop had no byte string (implicit slot,
// orphan, CPQ skip-pack) -- the consumer falls back to ft_from_term
// of the unpacked Terms.  Single-threaded engine; consumed (and
// reset) by exactly the next pop treatment.
static AtpFtCell *g_pop_ft_l = NULL;
static AtpFtCell *g_pop_ft_r = NULL;

// Survivor NF spans: the step function parks the popped CP's flat
// normal forms here around the thvm_atp_orient_and_add call so
// atp_push_rule can ADOPT them (clone, after an exact ft_eq_term
// proof) as the new rule's FT mirror instead of re-encoding the
// canonical Terms via ft_from_term.  NULL outside that bracket --
// every other rule-add path (axiom intake, interreduce requeue)
// falls back to the conversion.
static AtpFtCell *g_rule_ft_l = NULL;
static AtpFtCell *g_rule_ft_r = NULL;

// THVM_ATP_LAZY_RULE_TERMS (requires THVM_ATP_TRACE_LEAN_RULES): the
// pop survivor is committed WITHOUT decoding its NF spans -- orient
// runs on per-side NUM sentinel handles with this bracket carrying the
// spans; atp_compare / normalize_vars / the dup scan / the mirror
// build / the trace pack all intercept on it.  The Term slots hold the
// sentinels; any residual content reader walking a NUM where a CTR
// was expected is a divergence the parity suite flushes out (the
// certification grind).  Armed only across one orient call.
typedef struct {
  const AtpFtCell *span_l;   // pop NF spans, pre-var-norm, (l, r) order
  const AtpFtCell *span_r;
  Term sent_l;               // the sentinel handles orient receives
  Term sent_r;
  u8   armed;
} AtpLazyRule;
static AtpLazyRule g_lazy_rule = {NULL, NULL, 0, 0, 0u};
static int atp_lazy_rules_on(void) {
  static int on = -1;
  if (on < 0) {
    // Default ON (with the lean rule traces below) -- certified: the
    // full parity suites pass and the solve trajectories are
    // byte-identical with the pair active.  "0" on either env is the
    // one-round kill switch; lazy additionally requires lean (the
    // sentinel commit needs the off-heap trace pack).
    const char *lz = getenv("THVM_ATP_LAZY_RULE_TERMS");
    const char *ln = getenv("THVM_ATP_TRACE_LEAN_RULES");
    int lazy_ok = (lz == NULL || lz[0] != '0');
    int lean_ok = (ln == NULL || ln[0] != '0');
    on = lazy_ok && lean_ok;
  }
  return on;
}
// Map one of the two armed sentinels to its span (NULL = not a
// sentinel of the current bracket).
static const AtpFtCell *atp_lazy_span_of(Term t) {
  if (!g_lazy_rule.armed) return NULL;
  if (t == g_lazy_rule.sent_l) return g_lazy_rule.span_l;
  if (t == g_lazy_rule.sent_r) return g_lazy_rule.span_r;
  return NULL;
}

// Span sibling of wald_term_sort: identical sort inference off the
// mirror cells (vars by id from the spec table; constructors by label
// with arity + argument-sort checks; the TAG_NUM marker maps to the
// non-FOL-tag verdict).
static u32 wald_term_sort_ft(const WaldSpec *spec, const AtpFtCell *c) {
  if (spec == NULL || spec->n_sorts == 0) return 0;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    u32 id = c->sym & ~WF_VAR_BIT;
    for (u32 i = 0; i < spec->n_vars; i++) {
      if (spec->vars[i].var_id == id) return spec->vars[i].sort;
    }
    return WALD_MAX_SORTS;
  }
  u32 lab = c->sym;
  if (lab == ATPFT_NUM_MARKER) return WALD_MAX_SORTS;   // non-FOL tag
  u32 n = (u32)c->arity;
  const WaldSym *sym = NULL;
  for (u32 i = 0; i < spec->n_symbols; i++) {
    if (spec->symbols[i].label == lab) {
      sym = &spec->symbols[i];
      break;
    }
  }
  if (sym == NULL)     return WALD_MAX_SORTS;
  if (sym->arity != n) return WALD_MAX_SORTS;
  const AtpFtCell *kid = c->next;
  for (u32 i = 0; i < n; i++) {
    u32 child_sort = wald_term_sort_ft(spec, kid);
    if (child_sort != sym->arg_sorts[i]) return WALD_MAX_SORTS;
    kid = kid->end->next;
  }
  return sym->result_sort;
}

// Decode-on-demand for a lazy rule slot: a TAG_NUM handle in a rule
// slot is always a lazy sentinel (rule terms are CTR/FVR only), so
// materialize the Term from the mirror ONCE, replace the slot, and
// return it.  Every site that PERSISTS rule-Term values into
// Term-typed storage (r_dead saves, subsumer shapes, the irv victim
// buffer, the MNF pending queue) calls this so no sentinel ever
// escapes the slot arrays.
static Term atp_rule_term(AtpState *s, u32 i, u8 side) {
  Term t = side ? s->rhs[i] : s->lhs[i];
  if (term_tag(t) != TAG_NUM) return t;
  const AtpFtCell *m = side ? s->rhs_ft[i] : s->lhs_ft[i];
  if (m == NULL) return t;   // defensive: nothing to decode from
  Term real = ft_to_term(m);
  if (side) s->rhs[i] = real;
  else      s->lhs[i] = real;
  return real;
}

// Mirror-aware rule-side symbol count: a span's cell count equals the
// Term's node count (the TAG_NUM marker subtree counts as the ONE
// node its Term is).  Reads the mirror for lazy slots so weight scans
// never see sentinel sizes.
static u32 ft_symbol_count_span(const AtpFtCell *c) {
  u32 n = 0u;
  const AtpFtCell *e = (c->end != NULL) ? c->end->next : NULL;
  for (const AtpFtCell *p = c; p != e; p = p->next) {
    if ((p->sym & WF_VAR_BIT) == 0u && p->sym == ATPFT_NUM_MARKER) {
      n++;
      p = p->end;   // marker + 2 field kids = one Term node
      continue;
    }
    n++;
  }
  return n;
}
// Span var-containment: every var of `a` occurs in `b` (the exact
// sibling of atp_vars_contained on mirror cells).
static u8 ft_span_has_var(const AtpFtCell *b, u32 id) {
  const AtpFtCell *e = (b->end != NULL) ? b->end->next : NULL;
  for (const AtpFtCell *q = b; q != e; q = q->next) {
    if ((q->sym & WF_VAR_BIT) != 0u && (q->sym & ~WF_VAR_BIT) == id) {
      return 1u;
    }
  }
  return 0u;
}
static u8 ft_vars_contained_span(const AtpFtCell *a, const AtpFtCell *b) {
  const AtpFtCell *e = (a->end != NULL) ? a->end->next : NULL;
  for (const AtpFtCell *q = a; q != e; q = q->next) {
    if ((q->sym & WF_VAR_BIT) != 0u &&
        !ft_span_has_var(b, q->sym & ~WF_VAR_BIT)) {
      return 0u;
    }
  }
  return 1u;
}

static u32 atp_rule_symbol_count(AtpState *s, u32 i, u8 side) {
  Term t = side ? s->rhs[i] : s->lhs[i];
  if (term_tag(t) == TAG_NUM) {
    const AtpFtCell *m = side ? s->rhs_ft[i] : s->lhs_ft[i];
    if (m != NULL) return ft_symbol_count_span(m);
  }
  return atp_symbol_count(t);
}

// Keep-site staging for the MNF pend spans (set right before
// mnf_notify_fact; consumed by it).
static const AtpFtCell *g_mnf_pend_ft_l = NULL;
static const AtpFtCell *g_mnf_pend_ft_r = NULL;

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static u8 atp_order_is_plain_kbo(const AtpState *s);
#endif

// Requeue bracket: the demote drain parks the victim's (possibly NF)
// spans here around atp_add_equation_simplified so the enqueue can
// canonicalize / trace / fused-pack straight from them -- the queued
// CP is bytes + trace, so no Term ever materializes on the requeue.
// Read-only for the enqueue (it clones internally); cleared by it.
static const AtpFtCell *g_enq_ft_l = NULL;
static const AtpFtCell *g_enq_ft_r = NULL;

// Trace-pack span bracket: the ORIENT/FVI trace loop arms this with
// the new rule's mirrors so atp_trace_push's lean arm packs the pair
// from spans (atp_trace_eq_store_ft) instead of walking the (possibly
// sentinel) Terms.  Cleared by the push.
static const AtpFtCell *g_trace_eq_ft_l = NULL;
static const AtpFtCell *g_trace_eq_ft_r = NULL;
#endif // THVM_ATPFT_RULES

// Inverse of acp_pack: rebuild both heap Terms from the byte string.
static void acp_unpack(const u8 *buf, Term *lhs, Term *rhs) {
  const u8 *p = buf;
  Term l = acp_unpack_term(&p);
  Term r = acp_unpack_term(&p);
  if (lhs != NULL) *lhs = l;
  if (rhs != NULL) *rhs = r;
}

// === Fused varnorm + pack + KBO-weigh (one walk per CP) =============
//
// The CP push pipeline historically walked every surviving CP three
// times: thvm_normalize_vars rebuilt the pair with dense var ids (one
// walk + a full Term-cell rebuild), acp_pack walked the rebuilt pair
// to serialize it, and the GT/MIX weigh (thvm_kbo_nomemo via
// kbo_lin_addto) walked it a third time for the weight/variable
// balances.  The three are fused here into ONE preorder pass over the
// PRE-renumber pair that simultaneously
//   (a) assigns canonically dense var ids by first occurrence -- the
//       exact thvm_normalize_vars numbering (lhs fully, then rhs; see
//       unify/_.c nv_rewrite) -- WITHOUT materializing renumbered
//       Term cells,
//   (b) emits the packed byte string of the renumbered pair into
//       g_acp_scratch -- byte-identical to normalize-then-acp_pack,
//   (c) encodes each side as a flat preorder node array (dense ids)
//       and accumulates the whole-pair weight balance (phidiff) and
//       per-variable occurrence balance -- the exact sums
//       thvm_kbo_nomemo's Vortest computes for the top-level compare
//       (matching subtrees cancel there, so the net sums are equal;
//       kbo_lin_decide_clear's sign classification is accumulation-
//       order-invariant).
// The weigh (acp_fuse_kbo) then decides GT/LT/UN/EQ from the flat
// form: identity is a memcmp, the weight/var verdict comes from the
// accumulated balances, and only a genuine phi tie descends -- via a
// mirror of kbo_lin_rek over the flat arrays (kbo_flat_rek's decision
// core with the Term path's arity guard restored, since a flat node
// carries its arity here).  The renumber's KBO-cap property is what
// makes this sound where weighing the RAW pair is not: raw overlap
// terms carry j-side ids >= CP_RENAME_OFFSET, above the KBO balance
// caps (see atp_cp_order_gate); the dense ids stay inside them.
//
// Anomalies -- a NUM/ERA atom, a non-NUM arity cell, arity >
// REWRITE_MAX_ARITY, or > REWRITE_MAX_VAR distinct variables (the
// classes where nv_rewrite bails or the flat encoding cannot mirror
// thvm_kbo) -- abort the fused walk; the caller falls back to the
// classic normalize + pack + Term-weigh pipeline, so the fused path
// never has to replicate the bail semantics.  Unreachable on ATP
// first-order signatures; the fallback counter should stay 0.
//
// Kill switch: THVM_ATP_NO_FUSE_PACK=1 (same-binary A/B).  Certifier:
// THVM_ATP_FUSE_CHECK=1 re-runs the classic pipeline per CP and
// asserts byte-equal packing + verdict-equal weighing.

// Dense-id map: old id -> position == dense id, first-occurrence
// ordered (the exact nv_rewrite assignment).
typedef struct {
  u32 old_id[REWRITE_MAX_VAR];
  u32 n;
} AcpNvMap;

// Flat preorder node.  sym >= 0 is a CTR label, sym < 0 encodes a
// (dense) variable id as -(id+1); w is the node's own KBO weight; sz
// the subtree span in nodes; ar the CTR arity (0 for a variable) --
// the field kbo/_.c's KboFlatNode lacks, restored here so the lex
// descent keeps thvm_kbo's ns != nt incomparability guard.
typedef struct {
  i32 sym;
  u32 w;
  u32 sz;
  u32 ar;
} AcpFlatNode;

// Per-CP flat pool: lhs at [0, fl_n), rhs at [fl_n, fl_n + fr_n).
// Grown geometrically, persistent across CPs (single-threaded
// saturation, like g_acp_scratch).
static AcpFlatNode *g_acpf_pool = NULL;
static u32          g_acpf_cap  = 0u;

static void acpf_ensure(u32 need) {
  if (need <= g_acpf_cap) return;
  u32 ncap = g_acpf_cap ? g_acpf_cap : 1024u;
  while (need > ncap) ncap *= 2u;
  AcpFlatNode *np = (AcpFlatNode *)realloc(g_acpf_pool,
                                           (size_t)ncap * sizeof *np);
  if (np == NULL) thvm_fatal("acpf_ensure: OOM");
  g_acpf_pool = np;
  g_acpf_cap  = ncap;
}

// Fused-walk state for the CP being packed.  `valid` marks the flat
// encoding + balances live for exactly one weigh (atp_weigh_compare
// consumes it; atp_cp_heap_push clears it on every exit path).
typedef struct {
  AcpNvMap  map;
  long long phidiff;                 // w(lhs) - w(rhs)
  int       bal[REWRITE_MAX_VAR];    // count(lhs,v) - count(rhs,v), dense v
  u32       fl_n, fr_n;              // per-side flat node counts
  u8        valid;
  // ATP_CP_WEIGHT_TWEE per-side scores, accumulated by ftp_pack_fused
  // for the treated FT-direct push.  Exact because the treated
  // survivor's classic Term image is ft_to_term(NF) -- fresh unshared
  // cells, so atp_twee_term_score's Term-identity dedup can never fire
  // and the score is the plain weighted count 7*|side| - #vars(side),
  // provided the side stays under the classic walk's 256-node stack
  // cap (twee_valid=0 above it; the consumer then materializes and
  // scores classically).  Consumed by atp_cp_priority_sized_ex.
  u32       twee_l, twee_r;
  u8        twee_valid;
} AcpFuse;
static AcpFuse g_acp_fuse;

// Fused-pipeline shape counters (PROFILE=2 printout; fallbacks should
// be 0 on ATP signatures).
u64 g_acp_fuse_n_ok = 0;
u64 g_acp_fuse_n_fb = 0;

// One preorder step of the fused walk: pack `t` (renumbered on emit)
// at scratch position *pos, bump *nodes once per node, and -- when
// `fz` is non-NULL -- append the flat node at *fpos and accumulate the
// weight/var balances with `sign` (+1 lhs, -1 rhs).  Returns 0 on an
// anomaly (see the header comment); the caller falls back whole.
// Reference recursive form -- the default entry is the explicit-stack
// iteration below (acp_fuse_walk), which keeps pos/nodes/fpos in
// registers instead of chasing three out-pointers per node and pays no
// call bracket per child.  THVM_ATP_NO_FUSE_ITER=1 routes back here
// (same-binary kill switch); both emit the identical preorder bytes /
// flat nodes / balances.
static u8 acp_fuse_walk_rec(Term t, const KboConfig *cfg, AcpNvMap *m,
                            u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                            u32 *fpos) {
  (*nodes)++;
  acp_scratch_ensure(*pos, 21u);
  u8 *p = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      // nv_rewrite's defensive guards (non-NUM arity cell, over-arity
      // node) leave the subtree un-renumbered there; abort to the
      // classic pipeline instead of mirroring the bail.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return 0;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return 0;
      u32 lab = term_ext(t);
      *p++ = (u8)'C';
      acp_put_varint(&p, lab);
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_acp_scratch);
      u32 here = 0u;
      if (fz != NULL) {
        acpf_ensure(*fpos + 1u);
        here = (*fpos)++;
        u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
        g_acpf_pool[here].sym = (i32)lab;
        g_acpf_pool[here].w   = w;
        g_acpf_pool[here].ar  = n;
        fz->phidiff += (long long)sign * (long long)w;
      }
      for (u32 i = 0; i < n; i++) {
        if (!acp_fuse_walk_rec(heap_read(base + 1u + (u64)i), cfg, m, pos,
                               nodes, fz, sign, fpos)) return 0;
      }
      if (fz != NULL) g_acpf_pool[here].sz = *fpos - here;
      return 1;
    }
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;   // nv keeps ids past a
                                                 // full map; fall back
        m->old_id[m->n++] = old;
        if (fz != NULL) fz->bal[d] = 0;
      }
      *p++ = (u8)'V';
      acp_put_varint(&p, d);
      *pos = (u32)(p - g_acp_scratch);
      if (fz != NULL) {
        acpf_ensure(*fpos + 1u);
        g_acpf_pool[*fpos].sym = -(i32)(d + 1u);
        g_acpf_pool[*fpos].w   = cfg->var_weight;
        g_acpf_pool[*fpos].sz  = 1u;
        g_acpf_pool[*fpos].ar  = 0u;
        (*fpos)++;
        fz->phidiff += (long long)sign * (long long)cfg->var_weight;
        fz->bal[d]  += sign;
      }
      return 1;
    }
    default:
      return 0;   // NUM / ERA atom: the flat compare cannot mirror
                  // thvm_kbo's term_val identity; classic path handles
  }
}

// Explicit-stack form of the fused walk -- the default entry.  Same
// preorder, same emissions, same anomaly classes as the recursive
// reference above; pos/nodes/fpos live in locals across the whole
// tree (the recursive form dereferences three out-pointers at every
// node) and a child costs one frame push instead of an 8-argument
// call bracket.  A tree deeper than the frame stack returns 0 -- the
// caller's classic fallback emits the identical bytes, so the cap is
// a performance edge, not a semantic one (OA's deepest CP sides run
// ~1000 frames; the cap is 4x that).
enum { ACP_FUSE_MAX_DEPTH = 4096 };
typedef struct { u64 base; u32 n; u32 i; u32 here; } AcpFuseFrame;
static AcpFuseFrame g_acp_fuse_stk[ACP_FUSE_MAX_DEPTH];
static u8 acp_fuse_walk_iter(Term t, const KboConfig *cfg, AcpNvMap *m,
                             u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                             u32 *fpos) {
  u32 lpos = *pos, lnodes = *nodes, lfpos = *fpos;
  u32 sp = 0u;
  Term cur = t;
  for (;;) {
    lnodes++;
    acp_scratch_ensure(lpos, 21u);
    u8 *p = g_acp_scratch + lpos;
    switch (term_tag(cur)) {
      case TAG_CTR: {
        u64 base = term_val(cur);
        Term n_cell = heap_read(base);
        if (term_tag(n_cell) != TAG_NUM) return 0;
        u32 n = (u32)term_val(n_cell);
        if (n > REWRITE_MAX_ARITY) return 0;
        u32 lab = term_ext(cur);
        *p++ = (u8)'C';
        acp_put_varint(&p, lab);
        acp_put_varint(&p, n);
        lpos = (u32)(p - g_acp_scratch);
        u32 here = 0u;
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          here = lfpos++;
          u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
          g_acpf_pool[here].sym = (i32)lab;
          g_acpf_pool[here].w   = w;
          g_acpf_pool[here].ar  = n;
          fz->phidiff += (long long)sign * (long long)w;
        }
        if (n > 0u) {
          if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;   // classic fallback
          g_acp_fuse_stk[sp].base = base;
          g_acp_fuse_stk[sp].n    = n;
          g_acp_fuse_stk[sp].i    = 0u;
          g_acp_fuse_stk[sp].here = here;
          sp++;
          cur = heap_read(base + 1u);
          continue;
        }
        if (fz != NULL) g_acpf_pool[here].sz = lfpos - here;
        break;
      }
      case TAG_FVR: {
        u32 old = term_ext(cur);
        u32 d = 0u;
        for (; d < m->n; d++) {
          if (m->old_id[d] == old) break;
        }
        if (d == m->n) {
          if (m->n >= REWRITE_MAX_VAR) return 0;   // full map; fall back
          m->old_id[m->n++] = old;
          if (fz != NULL) fz->bal[d] = 0;
        }
        *p++ = (u8)'V';
        acp_put_varint(&p, d);
        lpos = (u32)(p - g_acp_scratch);
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          g_acpf_pool[lfpos].sym = -(i32)(d + 1u);
          g_acpf_pool[lfpos].w   = cfg->var_weight;
          g_acpf_pool[lfpos].sz  = 1u;
          g_acpf_pool[lfpos].ar  = 0u;
          lfpos++;
          fz->phidiff += (long long)sign * (long long)cfg->var_weight;
          fz->bal[d]  += sign;
        }
        break;
      }
      default:
        return 0;   // NUM / ERA atom (see the recursive reference)
    }
    // Ascend: advance to the next pending sibling, fixing up each
    // completed CTR's subtree span on the way out.
    for (;;) {
      if (sp == 0u) {
        *pos = lpos; *nodes = lnodes; *fpos = lfpos;
        return 1;
      }
      AcpFuseFrame *f = &g_acp_fuse_stk[sp - 1u];
      if (++f->i < f->n) {
        cur = heap_read(f->base + 1u + (u64)f->i);
        break;
      }
      if (fz != NULL) g_acpf_pool[f->here].sz = lfpos - f->here;
      sp--;
    }
  }
}

// Default dispatcher: the iterative walk unless THVM_ATP_NO_FUSE_ITER=1
// pins the recursive reference (kill switch; both are byte-identical).
static u8 acp_fuse_walk(Term t, const KboConfig *cfg, AcpNvMap *m,
                        u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                        u32 *fpos) {
  static int no_iter = -1;
  if (no_iter < 0) {
    const char *e = getenv("THVM_ATP_NO_FUSE_ITER");
    no_iter = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  if (no_iter) return acp_fuse_walk_rec(t, cfg, m, pos, nodes, fz, sign, fpos);
  return acp_fuse_walk_iter(t, cfg, m, pos, nodes, fz, sign, fpos);
}

// Fused renumber+pack of a raw pair into the scratch (no flat encode,
// no balances): bytes identical to thvm_normalize_vars-then-
// acp_pack_term x2.  Serves the off-heap trace store.  Returns 0 on
// an anomaly (caller renumbers + packs classically).
static u8 acp_trace_pack_fused(Term lhs, Term rhs, u32 *pos) {
  AcpNvMap m;
  m.n = 0u;
  u32 nodes = 0u, fpos = 0u;
  if (!acp_fuse_walk(lhs, NULL, &m, pos, &nodes, NULL, +1, &fpos)) return 0;
  if (!acp_fuse_walk(rhs, NULL, &m, pos, &nodes, NULL, -1, &fpos)) return 0;
  return 1;
}

// Fused counterpart of thvm_normalize_vars + acp_pack_ex: packs the
// renumbered pair into a fresh exact-size malloc and leaves the flat
// encoding + balances armed in g_acp_fuse for the weigh.  NULL on an
// anomaly (nothing armed; caller runs the classic pipeline).
static u8 *acp_pack_fused(const KboConfig *cfg, Term lhs, Term rhs,
                          u32 *out_len, u32 *out_nodes,
                          u32 *out_lhs_nodes) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  if (!acp_fuse_walk(lhs, cfg, &fz->map, &pos, &nodes, fz, +1, &fpos)) {
    return NULL;
  }
  u32 lhs_nodes = nodes, fl = fpos;
  if (!acp_fuse_walk(rhs, cfg, &fz->map, &pos, &nodes, fz, -1, &fpos)) {
    return NULL;
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("acp_pack_fused: OOM");
  memcpy(buf, g_acp_scratch, pos);
  if (out_len != NULL) *out_len = pos;
  *out_nodes     = nodes;
  *out_lhs_nodes = lhs_nodes;
  return buf;
}

// Add the flat subtree at a[pos] to one side of the current balance
// scope (mirror of kbo_lin_addto_walk / kbo_flat_addto): one linear
// sweep over the contiguous slice.
static void acpf_addto(KboLin *st, const AcpFlatNode *a, u32 pos, int sign,
                       long long *phidiff) {
  u32 end = pos + a[pos].sz;
  for (u32 i = pos; i < end; i++) {
    *phidiff += (long long)sign * (long long)a[i].w;
    if (a[i].sym < 0) kbo_bump(st, (u32)(-a[i].sym) - 1u, sign);
  }
}

// Combined Vortest over two flat slices (mirror of kbo_vortest):
// matching subtrees cancel and are skipped, divergent subtrees go
// wholly onto + / -.  Returns 1 iff structurally identical.  The
// arity test mirrors kbo_vortest's ns == nt gate exactly.
static int acpf_vortest(KboLin *st, const AcpFlatNode *a, u32 pa,
                        const AcpFlatNode *b, u32 pb, long long *phidiff) {
  if (a[pa].sym == b[pb].sym) {
    if (a[pa].sym < 0) return 1;                     // same variable
    if (a[pa].ar == b[pb].ar) {
      // Identical-slice fast path: byte-equal flat slices are
      // structurally equal subtrees -- no divergence, no contribution.
      if (a[pa].sz == b[pb].sz &&
          memcmp(&a[pa], &b[pb],
                 (size_t)a[pa].sz * sizeof(AcpFlatNode)) == 0) {
        return 1;
      }
      u32 ca = pa + 1u, cb = pb + 1u;
      int ident = 1;
      for (u32 i = 0; i < a[pa].ar; i++) {
        ident &= acpf_vortest(st, a, ca, b, cb, phidiff);
        ca += a[ca].sz;
        cb += b[cb].sz;
      }
      return ident;
    }
  }
  // diverge: a[pa] wholly on +, b[pb] wholly on -.
  acpf_addto(st, a, pa, +1, phidiff);
  acpf_addto(st, b, pb, -1, phidiff);
  return 0;
}

// Decision after Vortest -- mirror of kbo_lin_rek's per-level logic
// (KBOEntscheidungRek + KBOEntscheidungLex) over the flat slices,
// including the Term path's arity guard.  Precondition: the subtrees
// are not identical and the balance table is cleared.
static KboCmp acpf_rek(KboLin *st, const AcpFlatNode *a, u32 pa,
                       const AcpFlatNode *b, u32 pb,
                       long long phidiff, KboCmp varcmp) {
  switch (varcmp) {
    case KBO_GT:
      if (phidiff < 0) return KBO_UN;
      if (phidiff > 0) return KBO_GT;
      break;
    case KBO_LT:
      if (phidiff < 0) return KBO_LT;
      if (phidiff > 0) return KBO_UN;
      break;
    case KBO_EQ:
      if (phidiff < 0) return KBO_LT;
      if (phidiff > 0) return KBO_GT;
      break;
    default:
      return KBO_UN;
  }
  // phi tie: compare top symbols by precedence.
  if (a[pa].sym < 0 || b[pb].sym < 0) return KBO_UN;
  const KboConfig *cfg = st->cfg;
  u32 la = (u32)a[pa].sym, lb = (u32)b[pb].sym;
  u32 ps = (la < cfg->n_labels) ? cfg->precedence[la] : 0;
  u32 pt = (lb < cfg->n_labels) ? cfg->precedence[lb] : 0;
  if (ps > pt) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (ps < pt) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;
  if (la != lb) return KBO_UN;
  if (a[pa].ar != b[pb].ar) return KBO_UN;   // kbo_lin_rek's ns != nt
  // KBOEntscheidungLex: first non-identical child pair decides; the
  // deeper result maps back through this level's varcmp.
  u32 ca = pa + 1u, cb = pb + 1u;
  for (u32 i = 0; i < a[pa].ar; i++) {
    long long phi = 0;
    int ident = acpf_vortest(st, a, ca, b, cb, &phi);
    if (ident) {
      kbo_lin_clear(st);
      ca += a[ca].sz;
      cb += b[cb].sz;
      continue;
    }
    KboCmp cvar  = kbo_lin_decide_clear(st);
    KboCmp inner = acpf_rek(st, a, ca, b, cb, phi, cvar);
    return kbo_map_through(varcmp, inner);
  }
  return KBO_EQ;
}

// The fused CP weigh: verdict-identical to
// thvm_kbo_nomemo(lhs', rhs', cfg) on the renumbered pair the fused
// walk encoded.  Identity = flat memcmp (== Vortest's ident); the
// top-level phidiff/var balances come pre-accumulated from the pack
// walk; only a phi tie descends via the flat Rek/Lex mirror above.
// `_at` takes an explicit fuse state + flat pool base so a caller with
// a LOCAL encoding (the FT-emit virtual order gate) can weigh without
// touching the armed global; the bare entry reads the globals.
static KboCmp acp_fuse_kbo_at(const KboConfig *cfg, const AcpFuse *fz,
                              const AcpFlatNode *pool) {
  const AcpFlatNode *a = pool;
  const AcpFlatNode *b = pool + fz->fl_n;
  if (fz->fl_n == fz->fr_n &&
      memcmp(a, b, (size_t)fz->fl_n * sizeof(AcpFlatNode)) == 0) {
    return KBO_EQ;
  }
  // Variable-domination verdict from the accumulated balances --
  // kbo_lin_decide_clear's sign classification over the dense ids.
  KboCmp varcmp = KBO_EQ;
  for (u32 k = 0; k < fz->map.n; k++) {
    int v = fz->bal[k];
    if (v == 0) continue;
    switch (varcmp) {
      case KBO_EQ: varcmp = (v < 0) ? KBO_LT : KBO_GT; break;
      case KBO_LT: if (v > 0) varcmp = KBO_UN; break;
      case KBO_GT: if (v < 0) varcmp = KBO_UN; break;
      default: break;
    }
  }
  KboLin *st = &g_kbo_st;
  st->cfg       = cfg;
  st->n_touched = 0;
  return acpf_rek(st, a, 0u, b, 0u, fz->phidiff, varcmp);
}

static KboCmp acp_fuse_kbo(const KboConfig *cfg) {
  return acp_fuse_kbo_at(cfg, &g_acp_fuse, g_acpf_pool);
}

// Set around atp_trace_push_cp by the fused push loop: the off-heap
// trace store renumbers the raw pair during its own byte emit.
static u8 g_trace_eq_nv = 0u;
// Set around atp_cp_heap_push by the fused push loop: the heap push
// takes the fused pack (its caller skipped the treated renumber).
static u8 g_cp_push_fused = 0u;

// === FT-direct CP emission (always on; certifier THVM_ATP_FT_EMIT_CHECK) ==
//
// The walk former's raw >= 50 KPBehandelt class -- ~84% of all packed
// CP nodes on OrAssociativity -- is queued UNTREATED: the (lhs, rhs)
// heap Terms that two thvm_unify_apply passes build for it are
// consumed ONLY by two byte serializers (the off-heap trace store and
// the fused queue pack) and then abandoned as garbage.  Under the
// route conditions (see sw_form_push) nothing else ever dereferences
// them: the eq-parent order gate runs VIRTUALLY (phase 2a), every
// push-side filter is off under the fused pipeline, and the packed
// queue + the packed trace serve every later reader (a surviving CP
// materializes at pop from cp_packed, a cited trace entry from
// trace_eq_buf).  So for that class the Term build is pure
// serialization overhead.  The treated < 50 class dies too (phase 2b
// below): its cells build straight from the composite.
//
// This path serializes the formed CP (sigma(lo[p<-rin]), sigma(ro))
// STRAIGHT from the parent terms + the unifier: one virtual preorder
// walk that resolves variables through the substitution on the fly,
// producing the identical scratch bytes, the identical fused-KBO flat
// encoding/balances, and the identical node counts, without
// allocating a single heap cell.  Byte-identity argument:
// thvm_unify_apply emits exactly one output node per resolved input
// node (unify_walk chase, then CTR children recurse / unbound FVR
// stays), and cp_replace_at only swaps the subtree at the overlap
// position p -- the virtual walk visits the same resolved nodes in
// the same preorder, so bytes / dense renumbering / flat nodes /
// balances / counts all coincide with the classic apply-then-
// acp_pack_fused route.  Certified per CP by THVM_ATP_FT_EMIT_CHECK=1
// (vcp_emit_certify, defined next to sw_form_push).
//
// This is NOT the retired THVM_ATPFT_UNIFY former (later-8 refutation:
// 30% slower, 2x RSS): that one built AtpFtCell trees in a scratch
// arena as an ADDITIONAL representation and still decoded them back
// through ft_to_term into Term-typed CP slots.  Here no second tree
// exists -- the byte string IS the output, and the raw class never
// becomes Terms at all.
//
// Anomalies (torn arity cell, over-arity, NUM/ERA atom,
// > REWRITE_MAX_VAR distinct vars, depth cap) bail to the classic
// Term build, mirroring the fused-pack fallback discipline.
enum { ATP_WM_BEHANDELN_GATE = 50 };     // WM KPVerwaltung.c:437
// KPBehandeln treat-vs-raw SIZE GATE (used at every treat/raw DECISION site:
// the two FT formers + the classic push branch).  Default = 50: a CP with
// combined raw size < gate is treated (doR normalize + joined-drop) at push;
// bigger CPs queue RAW.  The DISCOUNT lane (use_discount) forces it to 0, so
// EVERY CP queues raw (passive rawness).  ALL decision sites MUST read this
// one value: if a former arms the treated FT stash (size < gate) while the
// push branch expects raw (its gate says >= gate), the stashed cells strand
// on the raw arm.  Count-cap uses of ATP_WM_BEHANDELN_GATE stay 50 (they only
// bound a walk; the decision reads atp_behandeln_gate).
static inline u32 atp_behandeln_gate(const AtpState *s) {
  return (s != NULL && s->use_discount) ? 0u : (u32)ATP_WM_BEHANDELN_GATE;
}
#define VCP_SPD_NONE      0xffffffffu
#define VCP_COUNT_ANOMALY 0xffffffffu

typedef struct {
  u8  armed;       // stash live for exactly the one armed CP push
  u32 len;         // packed pair bytes at g_acp_scratch[0, len)
  u32 nodes;       // combined node count (== the unify_apply tally)
  u32 lhs_nodes;   // lhs-only node count
} VcpEmit;
static VcpEmit g_vcp_emit;

// FT-emit shape counters (PROFILE=2 printout; fb should stay 0 on ATP
// signatures).
u64 g_vcp_emit_n_ok = 0;
u64 g_vcp_emit_n_fb = 0;

// unify_walk mirror (src/unify/_.c): chase FVR binding chains to the
// final unbound variable / constructor.
static inline Term vcp_resolve(Term t, const RewriteSubst *subst) {
  while (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return t;
    Term b = subst->bindings[id];
    if (b == 0) return t;
    t = b;
  }
  return t;
}

typedef struct { u64 base; u32 n; u32 i; u32 here; u32 spd; } VcpFrame;
static VcpFrame g_vcp_stk[ACP_FUSE_MAX_DEPTH];

// Child ci of a frame: the raw heap child, spine-substituted -- along
// the overlap path the child at p[spd] is the next spine node, and at
// the path's end it IS the replacement term rin (cp_replace_at's
// splice, virtualized).
static inline Term vcp_child(const VcpFrame *f, u32 ci, const u32 *p,
                             u32 p_len, Term rin, u32 *cspd) {
  Term c = heap_read(f->base + 1u + (u64)ci);
  *cspd = VCP_SPD_NONE;
  if (f->spd != VCP_SPD_NONE && p[f->spd] == ci) {
    if (f->spd + 1u == p_len) c = rin;
    else *cspd = f->spd + 1u;
  }
  return c;
}

// Node count of one virtual side, capped at `cap` (the walk stops
// there -- the caller consumes only the < gate verdict plus the exact
// under-cap total, exactly atp_symbol_count_upto's contract).  `acc`
// carries the count across sides.  Returns VCP_COUNT_ANOMALY on a
// torn arity cell / over-arity node / frame overflow -- the classes
// where thvm_unify_apply taints its tally -- so the caller falls back
// to the classic route.  A NUM/ERA atom counts one node and does not
// descend, matching the apply tally (the EMIT walk bails on it later;
// the count stays comparable either way).
static u32 vcp_count_side(Term root, const u32 *p, u32 p_len, Term rin,
                          const RewriteSubst *subst, u32 cap, u32 acc) {
  u32 sp = 0u;
  Term cur     = (rin != 0 && p_len == 0u) ? rin : root;
  u32  cur_spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  if (g_atp_phase_detail) { extern u64 g_vcp_cnt_calls; g_vcp_cnt_calls++; }
  for (;;) {
    cur = vcp_resolve(cur, subst);
    if (g_atp_phase_detail) { extern u64 g_vcp_cnt_nodes; g_vcp_cnt_nodes++; }
    if (++acc >= cap) return cap;
    if (term_tag(cur) == TAG_CTR) {
      u64 base = term_val(cur);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return VCP_COUNT_ANOMALY;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return VCP_COUNT_ANOMALY;
      if (n > 0u) {
        if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return VCP_COUNT_ANOMALY;
        g_vcp_stk[sp].base = base;
        g_vcp_stk[sp].n    = n;
        g_vcp_stk[sp].i    = 0u;
        g_vcp_stk[sp].spd  = cur_spd;
        sp++;
        cur = vcp_child(&g_vcp_stk[sp - 1u], 0u, p, p_len, rin, &cur_spd);
        continue;
      }
    }
    // Atom (leaf CTR / FVR / NUM): ascend to the next pending sibling.
    for (;;) {
      if (sp == 0u) return acc;
      VcpFrame *f = &g_vcp_stk[sp - 1u];
      if (++f->i < f->n) {
        cur = vcp_child(f, f->i, p, p_len, rin, &cur_spd);
        break;
      }
      sp--;
    }
  }
}

static u32 vcp_count_pair(Term lo, const u32 *p, u32 p_len, Term rin,
                          Term ro, const RewriteSubst *subst, u32 cap) {
  u32 acc = vcp_count_side(lo, p, p_len, rin, subst, cap, 0u);
  if (acc == VCP_COUNT_ANOMALY || acc >= cap) return acc;
  return vcp_count_side(ro, NULL, 0u, 0, subst, cap, acc);
}

// One virtual side of the fused emit: acp_fuse_walk_iter's exact byte
// / flat-node / balance emission, with the node source swapped from
// materialized Term cells to the resolve-through-subst composite
// above.  `fz` == NULL emits BYTES ONLY (no flat encode, no balances)
// -- the acp_trace_pack_fused analogue the treated FT route uses for
// its raw trace pair.
static u8 vcp_emit_side(Term root, const u32 *p, u32 p_len, Term rin,
                        const RewriteSubst *subst, const KboConfig *cfg,
                        AcpNvMap *m, u32 *pos, u32 *nodes, AcpFuse *fz,
                        int sign, u32 *fpos) {
  u32 lpos = *pos, lnodes = *nodes, lfpos = *fpos;
  u32 sp = 0u;
  Term cur     = (rin != 0 && p_len == 0u) ? rin : root;
  u32  cur_spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  if (g_atp_phase_detail) { extern u64 g_vcp_emt_calls; g_vcp_emt_calls++; }
  for (;;) {
    cur = vcp_resolve(cur, subst);
    lnodes++;
    if (g_atp_phase_detail) { extern u64 g_vcp_emt_nodes; g_vcp_emt_nodes++; }
    acp_scratch_ensure(lpos, 21u);
    u8 *bp = g_acp_scratch + lpos;
    switch (term_tag(cur)) {
      case TAG_CTR: {
        u64 base = term_val(cur);
        Term n_cell = heap_read(base);
        if (term_tag(n_cell) != TAG_NUM) return 0;
        u32 n = (u32)term_val(n_cell);
        if (n > REWRITE_MAX_ARITY) return 0;
        u32 lab = term_ext(cur);
        *bp++ = (u8)'C';
        acp_put_varint(&bp, lab);
        acp_put_varint(&bp, n);
        lpos = (u32)(bp - g_acp_scratch);
        u32 here = 0u;
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          here = lfpos++;
          u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
          g_acpf_pool[here].sym = (i32)lab;
          g_acpf_pool[here].w   = w;
          g_acpf_pool[here].ar  = n;
          fz->phidiff += (long long)sign * (long long)w;
        }
        if (n > 0u) {
          if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
          g_vcp_stk[sp].base = base;
          g_vcp_stk[sp].n    = n;
          g_vcp_stk[sp].i    = 0u;
          g_vcp_stk[sp].here = here;
          g_vcp_stk[sp].spd  = cur_spd;
          sp++;
          cur = vcp_child(&g_vcp_stk[sp - 1u], 0u, p, p_len, rin, &cur_spd);
          continue;
        }
        if (fz != NULL) g_acpf_pool[here].sz = 1u;
        break;
      }
      case TAG_FVR: {
        u32 old = term_ext(cur);
        u32 d = 0u;
        for (; d < m->n; d++) {
          if (m->old_id[d] == old) break;
        }
        if (d == m->n) {
          if (m->n >= REWRITE_MAX_VAR) return 0;   // full map; fall back
          m->old_id[m->n++] = old;
          if (fz != NULL) fz->bal[d] = 0;
        }
        *bp++ = (u8)'V';
        acp_put_varint(&bp, d);
        lpos = (u32)(bp - g_acp_scratch);
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          g_acpf_pool[lfpos].sym = -(i32)(d + 1u);
          g_acpf_pool[lfpos].w   = cfg->var_weight;
          g_acpf_pool[lfpos].sz  = 1u;
          g_acpf_pool[lfpos].ar  = 0u;
          lfpos++;
          fz->phidiff += (long long)sign * (long long)cfg->var_weight;
          fz->bal[d]  += sign;
        }
        break;
      }
      default:
        return 0;   // NUM / ERA atom: classic path handles
    }
    // Ascend: advance to the next pending sibling, fixing up each
    // completed CTR's subtree span on the way out.
    for (;;) {
      if (sp == 0u) {
        *pos = lpos; *nodes = lnodes; *fpos = lfpos;
        return 1;
      }
      VcpFrame *f = &g_vcp_stk[sp - 1u];
      if (++f->i < f->n) {
        cur = vcp_child(f, f->i, p, p_len, rin, &cur_spd);
        break;
      }
      if (fz != NULL) g_acpf_pool[f->here].sz = lfpos - f->here;
      sp--;
    }
  }
}

// Full fused virtual emit of the formed CP.  On success the packed
// bytes / counts sit in g_vcp_emit (NOT yet armed -- the caller arms
// around the push, exactly the g_cp_rawsz_hint discipline) and the
// fused-KBO weigh state is live in g_acp_fuse.  0 on an anomaly
// (nothing armed; the caller runs the classic Term build).
static u8 vcp_emit_fused(const KboConfig *cfg, Term lo, const u32 *p,
                         u32 p_len, Term rin, Term ro,
                         const RewriteSubst *subst) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
  if (!vcp_emit_side(ro, NULL, 0u, 0, subst, cfg, &fz->map,
                     &pos, &nodes, fz, +1, &fpos)) {
    return 0u;
  }
  u32 lhs_nodes = nodes, fl = fpos;
  if (!vcp_emit_side(lo, p, p_len, rin, subst, cfg, &fz->map,
                     &pos, &nodes, fz, -1, &fpos)) {
    return 0u;
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  g_vcp_emit.armed     = 0u;
  g_vcp_emit.len       = pos;
  g_vcp_emit.nodes     = nodes;
  g_vcp_emit.lhs_nodes = lhs_nodes;
  return 1u;
}

// --- Treated (< 50) FT-direct route (phase 2b) ----------------------
//
// The treated class flows through the WM KPBehandelt push-norm
// treatment: classic pipeline = thvm_unify_apply x2 (heap Terms) ->
// ft_from_term x2 -> FT normalize -> [joined? drop : ft_to_term x2 ->
// trace pack + queue pack].  Under the same route conditions as the
// raw class, the ONLY consumers of the built Terms are (a) the
// ft_from_term boundary into the FT treatment and (b) the two byte
// serializers on the survivor path.  So: build the FT cells STRAIGHT
// from the parents + unifier (vcp_ft_build_rec -- the ft_from_term
// image of the composite, cell-identical by the same resolve
// argument as the raw emit), serialize the renumbered RAW pair bytes
// for the trace at formation (vcp_emit_side, bytes-only), treat the
// cells directly (the atp_cp_trivially_joinable FT core), and pack a
// survivor's NF cells with ftp_pack_fused below -- the heap Term
// intermediate dies for the whole class, along with its
// ft_from_term / ft_to_term boundary walks.
//
// Compiled only with the FT stages (the treatment itself needs them);
// without THVM_ATPFT_NORM a never-armed stub keeps the two byte
// consumers' guards compiling.
#ifdef THVM_ATPFT_NORM
typedef struct {
  u8         armed;     // stash live for exactly the one armed CP push
  AtpFtCell *fl, *fr;   // raw pair FT cells; the treatment rewrites
                        // them to the NF cells in place (same stash)
  u32        raw_len;   // renumbered raw pair bytes at g_acp_scratch[0,raw_len)
                        // (0 = not emitted yet: the raw-pair emit is
                        // LAZY, run by the trace store for survivors
                        // only -- a joined CP never pays it)
  // Lazy-emit operands: the formation faces + the walk subst, all
  // stable through the one armed push (the subst is the former's
  // static, reset per attempt; the spans are rule mirrors / the
  // subject, untouched by the CP's own treatment).
  const AtpFtCell   *lo, *sub, *rin, *ro;
  const AtpFtSubst  *subst;
} VcpTreat;
#else
typedef struct {
  u8  armed;            // never set without the FT stages
  u32 raw_len;
} VcpTreat;
#endif
static VcpTreat g_vcp_treat;

// Arena-A net-retention attribution (mem-census): net persistent-cell
// growth (allocs - frees) bracketed by phase.  Unconditional i64
// arithmetic on existing counters -- no branch, no behavior change.
// Consumed by the bench's THVM_ATP_MEM_CENSUS report.
#ifdef THVM_ATPFT_ALLOC
i64 g_ftnet_form = 0;   // sw_form_push (formation + treat + queue push)
i64 g_ftnet_step = 0;   // whole thvm_atp_step
i64 g_ftnet_treatform = 0;   // vcp_ft_treat_form / vcp_treat_form / emit
i64 g_ftnet_join      = 0;   // vcp_treat_joinable (treat normalize + SJ)
i64 g_ftnet_push      = 0;   // atp_cp_heap_push
i64 g_ftnet_release   = 0;   // tail vcp_treat_release
static inline i64 ftnet_now(void) {
  return (i64)(g_ft_persist_alloc_total - g_ft_free_span_total);
}
#endif

u64 g_vcp_treat_n_ok   = 0;   // treated CPs formed FT-direct
u64 g_vcp_treat_n_fb   = 0;   // anomaly fallbacks to the classic build
u64 g_vcp_treat_n_pkfb = 0;   // NF-pack anomaly fallbacks (heap push)
u64 g_vcp_gate_n_test  = 0;   // eq-parent CPs whose order gate ran virtually
u64 g_vcp_gate_n_drop  = 0;   // ... of which dropped at formation
u64 g_vcp_gate_n_fb    = 0;   // ... anomaly fallbacks to the classic gate
u64 g_vcp_lb_hit       = 0;   // PROFILE=2 probe: raw CPs the unsubst LB catches
u64 g_vcp_lb_raw       = 0;   // PROFILE=2 probe: raw CPs total

// later-25 per-CP walk-count attribution (PROFILE=2 only; g_atp_phase_detail
// gated so timed runs are unperturbed).  Decompose CP formation into its
// separate composite traversals: the KPBehandelt size-gate count walk
// (vcp_count_side, capped at the gate), the raw-class emit walk
// (vcp_emit_side, bytes + flat-KBO), and the treated-class fused build+emit
// walk (vcp_ft_build_emit_rec, bytes + FT cells).  `_calls` = walk
// invocations (one per side), `_nodes` = node-visits summed across all walks.
u64 g_vcp_cnt_calls = 0, g_vcp_cnt_nodes = 0;   // count-side walk (size gate)
u64 g_vcp_emt_calls = 0, g_vcp_emt_nodes = 0;   // raw emit-side walk
u64 g_vcp_bem_calls = 0, g_vcp_bem_nodes = 0;   // treated build+emit walk

// later-25 heap-Term interchange counters (PROFILE=2 only): the dual-rep
// round-trips.  ft_from_term (Term -> FT) + ft_to_term (FT -> Term) +
// acp_unpack_term (packed bytes -> Term) invocations and node-visits, so the
// steady-state Term<->FT<->packed interchange volume can be sized against the
// tiles.  Defined here, incremented from ft.c (extern) + acp_unpack_term.
u64 g_ft_from_calls = 0, g_ft_from_nodes = 0;
u64 g_ft_to_calls   = 0, g_ft_to_nodes   = 0;
// Region-tagged attribution of the two interchange counters: the step
// loop stamps g_ft_region at each phase bracket (same spots as the
// phase timers) so the PROFILE=2 dump can say WHERE the conversions
// happen, not just how many.  0=other 1=pop-norm 2=interreduce
// 3=cp-gen/push 4=goal-check.
u8  g_ft_region = 0u;
u64 g_ft_from_by_region[8] = {0};
u64 g_ft_to_by_region[8]   = {0};
u64 g_acp_unterm_calls = 0, g_acp_unterm_nodes = 0;

#ifdef THVM_ATPFT_NORM
// FT-cell image of one virtual side: ft_from_term's exact recursive
// construction (children first, then ftnew_ctr stitches -- same arena
// allocation order, same GROUND fold, same var-id encoding), with the
// node source swapped to the resolve-through-subst composite.  NULL on
// an anomaly (NUM/ERA atom, torn arity cell, over-arity); partially
// built child spans are returned to the free list so a bail leaves the
// arena exactly as the classic path (which never built cells for the
// dropped candidate) would.
static AtpFtCell *vcp_ft_build_rec(AtpFt *a, Term t, u32 spd,
                                   const u32 *p, u32 p_len, Term rin,
                                   const RewriteSubst *subst) {
  t = vcp_resolve(t, subst);
  switch (term_tag(t)) {
    case TAG_FVR:
      return ftnew_var(a, term_ext(t), 0);
    case TAG_CTR: {
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return NULL;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return NULL;
      u32 lab = term_ext(t);
      if (n == 0u) return ftnew_const(a, lab, 0);
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        Term c = heap_read(base + 1u + (u64)i);
        u32 cspd = VCP_SPD_NONE;
        if (spd != VCP_SPD_NONE && p[spd] == i) {
          if (spd + 1u == p_len) c = rin;
          else cspd = spd + 1u;
        }
        kids[i] = vcp_ft_build_rec(a, c, cspd, p, p_len, rin, subst);
        if (kids[i] == NULL) {
          for (u32 k = 0; k < i; k++) {
            ft_free_span(a, kids[k], kids[k]->end);
          }
          return NULL;
        }
      }
      return ftnew_ctr(a, lab, (u16)n, kids, 0);
    }
    default:
      return NULL;   // NUM / ERA atom: classic path handles
  }
}

static AtpFtCell *vcp_ft_build_side(AtpFt *a, Term root, const u32 *p,
                                    u32 p_len, Term rin,
                                    const RewriteSubst *subst) {
  Term t  = (rin != 0 && p_len == 0u) ? rin : root;
  u32 spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  return vcp_ft_build_rec(a, t, spd, p, p_len, rin, subst);
}

// Fused build+emit of one virtual side: vcp_ft_build_rec's exact cell
// construction with vcp_emit_side's bytes-only emission folded into
// the SAME resolve walk -- the treated formation's two composite
// traversals (raw trace bytes, then FT cells) become one.  Bytes are
// emitted preorder at node entry ('C'/'V' before children), so the
// byte stream and the AcpNvMap first-occurrence renumbering coincide
// exactly with the two-walk form; the cells keep ORIGINAL var ids
// (the byte side renumbers, matching acp_trace_pack_fused).  NULL on
// an anomaly (the union of both walks' classes: NUM/ERA atom, torn
// arity, over-arity, full renumber map, depth cap); partially built
// child spans are freed and the scratch is left dirty -- the caller
// falls back to the classic build, which never reads these bytes.
// Certified per CP by vcp_treat_certify (raw-byte equality + cell
// identity vs the classic rebuild), unchanged.
static AtpFtCell *vcp_ft_build_emit_rec(AtpFt *a, Term t, u32 spd,
                                        const u32 *p, u32 p_len, Term rin,
                                        const RewriteSubst *subst,
                                        AcpNvMap *m, u32 *pos, u32 depth) {
  t = vcp_resolve(t, subst);
  if (g_atp_phase_detail) { extern u64 g_vcp_bem_nodes; g_vcp_bem_nodes++; }
  acp_scratch_ensure(*pos, 21u);
  u8 *bp = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return NULL;
        m->old_id[m->n++] = old;
      }
      *bp++ = (u8)'V';
      acp_put_varint(&bp, d);
      *pos = (u32)(bp - g_acp_scratch);
      return ftnew_var(a, old, 0);
    }
    case TAG_CTR: {
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return NULL;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return NULL;
      u32 lab = term_ext(t);
      *bp++ = (u8)'C';
      acp_put_varint(&bp, lab);
      acp_put_varint(&bp, n);
      *pos = (u32)(bp - g_acp_scratch);
      if (n == 0u) return ftnew_const(a, lab, 0);
      if (depth >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        Term c = heap_read(base + 1u + (u64)i);
        u32 cspd = VCP_SPD_NONE;
        if (spd != VCP_SPD_NONE && p[spd] == i) {
          if (spd + 1u == p_len) c = rin;
          else cspd = spd + 1u;
        }
        kids[i] = vcp_ft_build_emit_rec(a, c, cspd, p, p_len, rin, subst,
                                        m, pos, depth + 1u);
        if (kids[i] == NULL) {
          for (u32 k = 0; k < i; k++) {
            ft_free_span(a, kids[k], kids[k]->end);
          }
          return NULL;
        }
      }
      return ftnew_ctr(a, lab, (u16)n, kids, 0);
    }
    default:
      return NULL;   // NUM / ERA atom: classic path handles
  }
}

static AtpFtCell *vcp_ft_build_emit_side(AtpFt *a, Term root, const u32 *p,
                                         u32 p_len, Term rin,
                                         const RewriteSubst *subst,
                                         AcpNvMap *m, u32 *pos) {
  Term t  = (rin != 0 && p_len == 0u) ? rin : root;
  u32 spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  if (g_atp_phase_detail) { extern u64 g_vcp_bem_calls; g_vcp_bem_calls++; }
  return vcp_ft_build_emit_rec(a, t, spd, p, p_len, rin, subst, m, pos, 0u);
}

// Renumbered raw pair bytes of the virtual CP (no flat encode, no
// balances): the acp_trace_pack_fused image of the composite.  Serves
// the treated route's trace store.  0 on an anomaly.
static u8 vcp_emit_pair_bytes(Term lo, const u32 *p, u32 p_len, Term rin,
                              Term ro, const RewriteSubst *subst,
                              u32 *len) {
  AcpNvMap m;
  m.n = 0u;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  // WM side order: sigma(ro) first (see the classic arm).
  if (!vcp_emit_side(ro, NULL, 0u, 0, subst, NULL, &m,
                     &pos, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (!vcp_emit_side(lo, p, p_len, rin, subst, NULL, &m,
                     &pos, &nodes, NULL, -1, &fpos)) {
    return 0u;
  }
  *len = pos;
  return 1u;
}

// FT-side fused pack: acp_pack_fused's exact byte / flat-node /
// balance emission with the node source swapped from Term cells to an
// AtpFt span -- the treated survivor's NF cells go straight to packed
// bytes + the armed fused-KBO weigh state, no ft_to_term.  The span's
// `next` chain IS the preorder, so the walk is a linear scan with an
// arity-countdown frame stack for the subtree-span fixups.  NULL on an
// anomaly (converted NUM / foreign-tag marker syms, over-arity, depth
// cap, > REWRITE_MAX_VAR distinct vars); nothing armed and the caller
// decodes the cells and runs the classic pipeline.  want_buf == 0
// skips the byte-string malloc (bytes stay in the scratch) and returns
// the scratch pointer as a NOT-owned success sentinel -- the mode-8
// byte-queue-retirement caller stores NULL and the adopted FT spans
// carry the queue payload.
static u8 *ftp_pack_fused(const KboConfig *cfg, const AtpFtCell *lhs,
                          const AtpFtCell *rhs, u32 *out_len,
                          u32 *out_nodes, u32 *out_lhs_nodes,
                          u8 want_buf) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u, lhs_nodes = 0u, fl = 0u;
  u32 nvar = 0u, nvar_l = 0u;   // var occurrences (total / lhs), twee score
  static u32 here_stk[ACP_FUSE_MAX_DEPTH];
  static u32 rem_stk[ACP_FUSE_MAX_DEPTH];
  for (u32 side = 0u; side < 2u; side++) {
    const AtpFtCell *c = (side == 0u) ? lhs : rhs;
    int sign = (side == 0u) ? +1 : -1;
    u32 sp = 0u;
    for (;;) {
      nodes++;
      acp_scratch_ensure(pos, 21u);
      u8 *bp = g_acp_scratch + pos;
      u8 leaf;
      if (ft_is_var(c)) {
        u32 old = ft_var_id(c);
        u32 d = 0u;
        for (; d < fz->map.n; d++) {
          if (fz->map.old_id[d] == old) break;
        }
        if (d == fz->map.n) {
          if (fz->map.n >= REWRITE_MAX_VAR) return NULL;
          fz->map.old_id[fz->map.n++] = old;
          fz->bal[d] = 0;
        }
        *bp++ = (u8)'V';
        acp_put_varint(&bp, d);
        pos = (u32)(bp - g_acp_scratch);
        acpf_ensure(fpos + 1u);
        g_acpf_pool[fpos].sym = -(i32)(d + 1u);
        g_acpf_pool[fpos].w   = cfg->var_weight;
        g_acpf_pool[fpos].sz  = 1u;
        g_acpf_pool[fpos].ar  = 0u;
        fpos++;
        fz->phidiff += (long long)sign * (long long)cfg->var_weight;
        fz->bal[d]  += sign;
        nvar++;
        leaf = 1u;
      } else {
        u32 lab = ft_ctr_sym(c);
        // Converted-NUM / foreign-tag marker namespace (ft.c): the
        // classic Term pack would emit 'N' / bail here -- classic path.
        if (lab >= 0x20000000u) return NULL;
        u32 n = c->arity;
        if (n > REWRITE_MAX_ARITY) return NULL;
        *bp++ = (u8)'C';
        acp_put_varint(&bp, lab);
        acp_put_varint(&bp, n);
        pos = (u32)(bp - g_acp_scratch);
        acpf_ensure(fpos + 1u);
        u32 here = fpos++;
        u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
        g_acpf_pool[here].sym = (i32)lab;
        g_acpf_pool[here].w   = w;
        g_acpf_pool[here].ar  = n;
        fz->phidiff += (long long)sign * (long long)w;
        if (n > 0u) {
          if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
          here_stk[sp] = here;
          rem_stk[sp]  = n;
          sp++;
          leaf = 0u;
        } else {
          g_acpf_pool[here].sz = 1u;
          leaf = 1u;
        }
      }
      if (leaf) {
        // Complete enclosing subtrees whose last child this was.
        u8 done = 0u;
        for (;;) {
          if (sp == 0u) { done = 1u; break; }
          if (--rem_stk[sp - 1u] > 0u) break;
          g_acpf_pool[here_stk[sp - 1u]].sz = fpos - here_stk[sp - 1u];
          sp--;
        }
        if (done) break;
      }
      c = c->next;
      if (c == NULL) return NULL;   // torn span; classic path
    }
    if (side == 0u) {
      lhs_nodes = nodes;
      fl        = fpos;
      nvar_l    = nvar;
    }
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  // ATP_CP_WEIGHT_TWEE per-side scores (see the AcpFuse field comment):
  // plain weighted count over the unshared treated NF -- 7 per function
  // symbol / constant, 6 per variable occurrence.  Exact vs the classic
  // atp_twee_term_score(nv(ft_to_term(side))) whenever the side stays
  // under the classic walk's 256-entry stack (a <=256-node side can
  // never overflow it, nor saturate the 512-slot seen table).
  fz->twee_l     = 7u * fz->fl_n - (nvar_l);
  fz->twee_r     = 7u * fz->fr_n - (nvar - nvar_l);
  fz->twee_valid = (fz->fl_n <= 256u && fz->fr_n <= 256u) ? 1u : 0u;
  *out_len       = pos;
  *out_nodes     = nodes;
  *out_lhs_nodes = lhs_nodes;
  if (!want_buf) return g_acp_scratch;   // success sentinel, not owned
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("ftp_pack_fused: OOM");
  memcpy(buf, g_acp_scratch, pos);
  return buf;
}

// Treated (< 50) FT-direct formation: emit the renumbered raw pair
// bytes into the scratch (they sit at g_acp_scratch[0, raw_len) until
// the trace store consumes them -- audited: nothing between formation
// and the trace push touches the scratch on the routed path) and
// build the raw pair's FT cells straight from the composite.  On
// success the stash holds both, NOT yet armed (the caller arms around
// the push).  0 on an anomaly: nothing allocated, nothing armed, the
// caller runs the classic Term build.
static u8 vcp_treat_form(AtpState *s, Term lo, const u32 *p, u32 p_len,
                         Term rin, Term ro, const RewriteSubst *subst) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Fused single-walk formation (default; THVM_ATP_NO_VCP_FUSEWALK=1
  // restores the two-walk form for one bake round): bytes + cells from
  // ONE composite traversal per side.
  static int no_fusewalk = -1;
  if (no_fusewalk < 0) no_fusewalk = atp_env_on("THVM_ATP_NO_VCP_FUSEWALK");
  u32 raw_len = 0u;
  AtpFtCell *fl, *fr;
  if (!no_fusewalk) {
    AcpNvMap m;
    m.n = 0u;
    // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
    fl = vcp_ft_build_emit_side(a, ro, NULL, 0u, 0, subst, &m, &raw_len);
    if (fl == NULL) return 0u;
    fr = vcp_ft_build_emit_side(a, lo, p, p_len, rin, subst, &m, &raw_len);
    if (fr == NULL) {
      ft_free_span(a, fl, fl->end);
      return 0u;
    }
  } else {
    if (!vcp_emit_pair_bytes(lo, p, p_len, rin, ro, subst, &raw_len)) {
      return 0u;
    }
    fl = vcp_ft_build_side(a, ro, NULL, 0u, 0, subst);
    if (fl == NULL) return 0u;
    fr = vcp_ft_build_side(a, lo, p, p_len, rin, subst);
    if (fr == NULL) {
      ft_free_span(a, fl, fl->end);
      return 0u;
    }
  }
  g_vcp_treat.armed   = 0u;
  g_vcp_treat.fl      = fl;
  g_vcp_treat.fr      = fr;
  g_vcp_treat.raw_len = raw_len;
  return 1u;
}

// Release a formed-but-unpushed treated stash (a post-formation gate
// dropped the CP): the classic path never built cells for a dropped
// candidate, so return the spans to the free list.
static void vcp_treat_release(AtpState *s) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  if (g_vcp_treat.fl != NULL) ft_free_span(a, g_vcp_treat.fl, g_vcp_treat.fl->end);
  if (g_vcp_treat.fr != NULL) ft_free_span(a, g_vcp_treat.fr, g_vcp_treat.fr->end);
  g_vcp_treat.fl = g_vcp_treat.fr = NULL;
  g_vcp_treat.armed = 0u;
}
#endif // THVM_ATPFT_NORM

// --- FT-emit virtual order gate (phase 2a) ---------------------------
//
// The WM KPAction ordering gate (atp_cp_order_gate) is the only peak
// consumer on the routed path: for an equation parent it drops the CP
// at formation when
//   outer (o_eq):  sigma(ro)  >kbo  sigma(lo)      (cp.rhs vs cp.peak)
//   inner (in_eq): sigma(rin) >kbo  sigma(sub)     (the overlap-position
//                                    subterms of cp.lhs / cp.peak)
// after a joint thvm_normalize_vars.  Every operand is a plain
// resolve-through-subst composite of a term the walk former already
// holds, so the verdict is computable without building ANY of the
// three CP Terms: two bytes+flat walks into a LOCAL fuse state, then
// the flat-KBO mirror (acp_fuse_kbo_at == thvm_kbo_nomemo on the
// jointly-renumbered pair == the classic gate's normalize+compare;
// route conditions pin atp_compare to plain KBO).  Returns 1 with
// *drop set; 0 on an anomaly (caller falls back to the classic Term
// build, whose classic gate then decides).
static u8 vcp_order_gate_virtual(AtpState *s, Term lo, Term rin, Term ro,
                                 Term sub, const RewriteSubst *subst,
                                 u8 o_eq, u8 in_eq, u8 *drop) {
  *drop = 0u;
  AcpFuse fz;
  u32 pos, nodes, fpos, fl;
  if (o_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_emit_side(ro, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_emit_side(lo, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) {
      *drop = 1u;
      return 1u;
    }
  }
  if (in_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_emit_side(rin, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_emit_side(sub, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) *drop = 1u;
  }
  return 1u;
}

// Composite fused emit on spans: the FT sibling of vcp_emit_side --
// walk `c`'s composite (splice `rin` at cell `*at_io`, resolve vars
// through `subst`, chains followed) emitting the same 'C'/'V' byte
// stream into g_acp_scratch and the same flat-KBO pool entries /
// balances into `fz` (joint appearance-order renumber via `m`).  A
// TAG_NUM marker or a full var map returns 0 -- the same anomaly
// cliffs that send the Term emitter to the classic build.  Recursive
// (composite depth); the Term version's frame cap plays the same
// role.  The CP-walk port's serializer; no caller yet.
#if defined(THVM_ATPFT_NORM)
// Iterative emit core: the preorder chain IS the recursive visit
// order, so CTR descent needs no calls at all -- only a bound-var
// jump (emit the binding's span, resume after the var) and the one
// splice point push a resume frame.  Byte- and pool-identical to the
// recursive walk it replaces (same appearance-order var map, same
// scratch bytes, same fuse entries; the CTR sz backfill runs as a
// linear post-pass over the new flat entries).  The splice test is
// scoped to SUBJECT frames only, exactly like the recursion's
// at_io=NULL hand-off into bindings (a binding can BE the splice
// cell when the inner lhs is a bare variable).
static u8 vcp_ft_emit_cell(const AtpFtCell *c, const AtpFtCell **at_io,
                           const AtpFtCell *rin, const AtpFtSubst *subst,
                           const KboConfig *cfg, AcpNvMap *m,
                           u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                           u32 *fpos) {
  static struct { const AtpFtCell *next, *stop; u8 subj; }
      rs[ACP_FUSE_MAX_DEPTH];   // single-threaded engine: static, not stack
  u32 sp = 0u;
  const u32 f0 = (fz != NULL) ? *fpos : 0u;
  const AtpFtCell *cur  = c;
  const AtpFtCell *stop = (c->end != NULL) ? c->end->next : NULL;
  u8 subj = 1u;
  for (;;) {
    while (cur == stop) {
      if (sp == 0u) goto done;
      --sp;
      cur  = rs[sp].next;
      stop = rs[sp].stop;
      subj = rs[sp].subj;
    }
    if (subj && at_io != NULL && *at_io == cur) {
      *at_io = NULL;                     // single splice point
      if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
      rs[sp].next = (cur->end != NULL) ? cur->end->next : stop;
      rs[sp].stop = stop;
      rs[sp].subj = 1u;
      sp++;
      cur  = rin;
      stop = (rin->end != NULL) ? rin->end->next : NULL;
      subj = 0u;
      continue;
    }
    if ((cur->sym & WF_VAR_BIT) != 0u) {
      u32 id = cur->sym & ~WF_VAR_BIT;
      const AtpFtCell *b =
          (subst != NULL && id < ATPFT_MAX_VARS) ? subst->bind[id] : NULL;
      if (b != NULL) {
        if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
        rs[sp].next = cur->next;         // var is a leaf: end == self
        rs[sp].stop = stop;
        rs[sp].subj = subj;
        sp++;
        cur  = b;
        stop = (b->end != NULL) ? b->end->next : NULL;
        subj = 0u;
        continue;
      }
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == id) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;   // full map; fall back
        m->old_id[m->n++] = id;
        if (fz != NULL) fz->bal[d] = 0;
      }
      (*nodes)++;
      acp_scratch_ensure(*pos, 21u);
      u8 *bp = g_acp_scratch + *pos;
      *bp++ = (u8)'V';
      acp_put_varint(&bp, d);
      *pos = (u32)(bp - g_acp_scratch);
      if (fz != NULL) {
        acpf_ensure(*fpos + 1u);
        g_acpf_pool[*fpos].sym = -(i32)(d + 1u);
        g_acpf_pool[*fpos].w   = cfg->var_weight;
        g_acpf_pool[*fpos].sz  = 1u;
        g_acpf_pool[*fpos].ar  = 0u;
        (*fpos)++;
        fz->phidiff += (long long)sign * (long long)cfg->var_weight;
        fz->bal[d]  += sign;
      }
      cur = cur->next;
      continue;
    }
    if (ft_ctr_sym(cur) == ATPFT_NUM_MARKER) return 0;  // NUM: classic path
    u32 lab = ft_ctr_sym(cur);
    u32 n   = (u32)cur->arity;
    if (n > REWRITE_MAX_ARITY) return 0;
    (*nodes)++;
    acp_scratch_ensure(*pos, 21u);
    u8 *bp = g_acp_scratch + *pos;
    *bp++ = (u8)'C';
    acp_put_varint(&bp, lab);
    acp_put_varint(&bp, n);
    *pos = (u32)(bp - g_acp_scratch);
    if (fz != NULL) {
      acpf_ensure(*fpos + 1u);
      u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
      g_acpf_pool[*fpos].sym = (i32)lab;
      g_acpf_pool[*fpos].w   = w;
      g_acpf_pool[*fpos].ar  = n;
      (*fpos)++;
      fz->phidiff += (long long)sign * (long long)w;
    }
    cur = cur->next;                     // preorder: first kid (or next)
  }
done:
  if (fz != NULL) {
    // sz backfill: preorder + arity determine every CTR's flat-subtree
    // length (identical to the recursion's *fpos - here).
    static struct { u32 idx; u32 kids; } open[ACP_FUSE_MAX_DEPTH];
    u32 on = 0u;
    for (u32 i = f0; i < *fpos; i++) {
      u32 ar = g_acpf_pool[i].ar;
      if (ar > 0u) {
        if (on >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
        open[on].idx = i;
        open[on].kids = ar;
        on++;
        continue;
      }
      // Leaf (var or 0-ary CTR): sz = 1, then cascade completions.
      g_acpf_pool[i].sz = 1u;
      while (on > 0u && --open[on - 1u].kids == 0u) {
        on--;
        g_acpf_pool[open[on].idx].sz = i + 1u - open[on].idx;
      }
    }
  }
  return 1;
}

static u8 vcp_ft_emit_side(const AtpFtCell *root, const AtpFtCell *at,
                           const AtpFtCell *rin, const AtpFtSubst *subst,
                           const KboConfig *cfg, AcpNvMap *m,
                           u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                           u32 *fpos) {
  const AtpFtCell *at_local = at;
  // Top-level splice (p_len == 0): the whole side IS the inner face.
  const AtpFtCell *start = (at != NULL && at == root) ? rin : root;
  const AtpFtCell **aio  = (at != NULL && at == root) ? NULL : &at_local;
  return vcp_ft_emit_cell(start, aio, rin, subst, cfg, m, pos, nodes,
                          fz, sign, fpos);
}
#endif // THVM_ATPFT_NORM

// FT sibling of vcp_order_gate_virtual: the WM KPAction eq-parent
// ordering gate decided entirely on span composites -- two
// vcp_ft_emit_side walks into a LOCAL fuse state, then the flat-KBO
// mirror, per active parent face.  Returns 1 with *drop set; 0 on an
// anomaly (caller falls back to the Term route).  The CP-walk port's
// gate; no caller yet.
#if defined(THVM_ATPFT_NORM)
static u8 vcp_ft_order_gate(AtpState *s, const AtpFtCell *lo,
                            const AtpFtCell *rin, const AtpFtCell *ro,
                            const AtpFtCell *sub, const AtpFtSubst *subst,
                            u8 o_eq, u8 in_eq, u8 *drop) {
  *drop = 0u;
  AcpFuse fz;
  u32 pos, nodes, fpos, fl;
  if (o_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_ft_emit_side(ro, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_ft_emit_side(lo, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) {
      *drop = 1u;
      return 1u;
    }
  }
  if (in_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_ft_emit_side(rin, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_ft_emit_side(sub, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) *drop = 1u;
  }
  return 1u;
}
#endif // THVM_ATPFT_NORM

// Composite CELL builder on spans: the FT sibling of
// vcp_ft_build_emit_rec -- build the formed CP side's AtpFt cells
// straight from the span composite (splice `rin` at the overlap cell,
// resolve vars through the AtpFtSubst, chains followed), applying the
// same joint appearance-order renumber the byte emit uses (shared
// AcpNvMap), through the same ftnew_* constructors.  NULL on an
// anomaly (TAG_NUM marker / full var map / arity cliff) -- caller
// falls back to the Term build.  The CP-walk port's treated-class
// builder; no caller yet.
#if defined(THVM_ATPFT_NORM)
// Preorder-STREAMING build: cells allocate in visit order (the slab
// bump makes the fresh CP span physically adjacent -- WM's
// SigmaRGanzNeu discipline; every downstream walk on it gets the
// cache locality), next-threaded as emitted, end pointers + the
// GROUND fold patched by a completion cascade.  Field-identical to
// the recursive ftnew_* build it replaces; addresses differ only in
// adjacency.  Splice test scoped to subject frames (a binding can BE
// the splice cell).  NULL on the same anomaly set (NUM marker, full
// map, arity cliff, frame cliff); partials leak into the arena.
static AtpFtCell *vcp_ft_build_cell(AtpFt *a, const AtpFtCell *c,
                                    const AtpFtCell **at_io,
                                    const AtpFtCell *rin,
                                    const AtpFtSubst *subst,
                                    AcpNvMap *m) {
  static struct { const AtpFtCell *next, *stop; u8 subj; }
      rs[ACP_FUSE_MAX_DEPTH];
  static struct { AtpFtCell *cell; u32 kids; } open[ACP_FUSE_MAX_DEPTH];
  u32 sp = 0u, on = 0u;
  AtpFtCell *root_out = NULL, *prev = NULL;
  const AtpFtCell *cur = c;
  const AtpFtCell *stop = (c->end != NULL) ? c->end->next : NULL;
  u8 subj = 1u;
  for (;;) {
    while (cur == stop) {
      if (sp == 0u) goto done;
      --sp;
      cur  = rs[sp].next;
      stop = rs[sp].stop;
      subj = rs[sp].subj;
    }
    if (subj && at_io != NULL && *at_io == cur) {
      *at_io = NULL;
      if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
      rs[sp].next = (cur->end != NULL) ? cur->end->next : stop;
      rs[sp].stop = stop;
      rs[sp].subj = 1u;
      sp++;
      cur  = rin;
      stop = (rin->end != NULL) ? rin->end->next : NULL;
      subj = 0u;
      continue;
    }
    AtpFtCell *nc;
    u8 leaf_flags;
    if ((cur->sym & WF_VAR_BIT) != 0u) {
      u32 id = cur->sym & ~WF_VAR_BIT;
      const AtpFtCell *b =
          (subst != NULL && id < ATPFT_MAX_VARS) ? subst->bind[id] : NULL;
      if (b != NULL) {
        if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
        rs[sp].next = cur->next;
        rs[sp].stop = stop;
        rs[sp].subj = subj;
        sp++;
        cur  = b;
        stop = (b->end != NULL) ? b->end->next : NULL;
        subj = 0u;
        continue;
      }
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == id) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return NULL;
        m->old_id[m->n++] = id;
      }
      nc = ft_alloc_one(a, 0);
      nc->sym   = WF_VAR_BIT | d;
      nc->arity = 0;
      nc->flags = 0;
      nc->next  = NULL;
      nc->end   = nc;
      ft_raw_check(nc);
      leaf_flags = 0u;
      cur = cur->next;
    } else {
      if (ft_ctr_sym(cur) == ATPFT_NUM_MARKER) return NULL;
      u32 lab = ft_ctr_sym(cur);
      u32 n   = (u32)cur->arity;
      if (n > REWRITE_MAX_ARITY) return NULL;
      nc = ft_alloc_one(a, 0);
      nc->sym   = lab & ~WF_VAR_BIT;
      nc->arity = (u16)n;
      nc->flags = ATPFT_FLAG_GROUND;   // fold clears on a var below
      nc->next  = NULL;
      nc->end   = nc;
      ft_raw_check(nc);
      if (n > 0u) {
        if (on >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
        if (prev != NULL) prev->next = nc;
        else              root_out = nc;
        prev = nc;
        open[on].cell = nc;
        open[on].kids = n;
        on++;
        cur = cur->next;               // preorder: first kid
        continue;
      }
      leaf_flags = ATPFT_FLAG_GROUND;
      cur = cur->next;
    }
    // Leaf emitted: thread + cascade completions upward.
    if (prev != NULL) prev->next = nc;
    else              root_out = nc;
    prev = nc;
    u8 child_flags = leaf_flags;
    while (on > 0u) {
      AtpFtCell *pc = open[on - 1u].cell;
      if ((child_flags & ATPFT_FLAG_GROUND) == 0u)
        pc->flags &= (u8)~ATPFT_FLAG_GROUND;
      if (--open[on - 1u].kids != 0u) break;
      on--;
      pc->end = nc;                    // subtree's last preorder cell
      child_flags = pc->flags;
    }
  }
done:
  return root_out;
}
#endif // THVM_ATPFT_NORM

// Composite-size count on spans: the node count of `root` with the
// subterm at cell `at` replaced by `rin` and every variable resolved
// through `subst` (chains followed) -- the FT sibling of
// vcp_count_side/vcp_count_pair.  Iterative over span segments; an
// unbound variable counts 1.  Returns the accumulated count, saturated
// at `cap` (callers only compare against the KPBehandeln gate).  The
// CP-walk port's size classifier; no caller yet.
#if defined(THVM_ATPFT_NORM)
enum { VCP_FT_STK = 96 };
static u32 vcp_ft_count_side(const AtpFtCell *root, const AtpFtCell *at,
                             const AtpFtCell *rin, const AtpFtSubst *subst,
                             u32 cap, u32 acc) {
  struct { const AtpFtCell *c, *e; } stk[VCP_FT_STK];
  u32 sp = 0u;
  const AtpFtCell *c = root;
  const AtpFtCell *e = (root->end != NULL) ? root->end->next : NULL;
  for (;;) {
    while (c == NULL || c == e) {
      if (sp == 0u) return acc;
      sp--;
      c = stk[sp].c;
      e = stk[sp].e;
    }
    if (c == at) {
      // Splice point: count rin's composite instead of this subtree,
      // then resume after the replaced subtree.
      if (sp >= (u32)VCP_FT_STK) return VCP_COUNT_ANOMALY;
      stk[sp].c = c->end->next;
      stk[sp].e = e;
      sp++;
      at = NULL;                                  // single occurrence
      c = rin;
      e = (rin->end != NULL) ? rin->end->next : NULL;
      continue;
    }
    if ((c->sym & WF_VAR_BIT) != 0u) {
      u32 id = c->sym & ~WF_VAR_BIT;
      const AtpFtCell *b =
          (subst != NULL && id < ATPFT_MAX_VARS) ? subst->bind[id] : NULL;
      if (b != NULL) {
        // Resolve through the binding (its own vars resolve on the way).
        if (sp >= (u32)VCP_FT_STK) return VCP_COUNT_ANOMALY;
        stk[sp].c = c->next;
        stk[sp].e = e;
        sp++;
        c = b;
        e = (b->end != NULL) ? b->end->next : NULL;
        continue;
      }
      acc++;                                      // unbound var: one node
      if (acc >= cap) return acc;
      c = c->next;
      continue;
    }
    if (ft_ctr_sym(c) == ATPFT_NUM_MARKER) {
      return VCP_COUNT_ANOMALY;   // TAG_NUM counts 1 Term-side: fall back
    }
    acc++;                                        // constructor cell
    if (acc >= cap) return acc;
    c = c->next;
  }
}

// Pair wrapper -- the FT sibling of vcp_count_pair (same early-outs,
// same anomaly propagation).
static u32 vcp_ft_count_pair(const AtpFtCell *lo_ft, const AtpFtCell *sub_ft,
                             const AtpFtCell *rin_ft, const AtpFtCell *ro_ft,
                             const AtpFtSubst *subst, u32 cap) {
  u32 acc = vcp_ft_count_side(lo_ft, sub_ft, rin_ft, subst, cap, 0u);
  if (acc == VCP_COUNT_ANOMALY || acc >= cap) return acc;
  return vcp_ft_count_side(ro_ft, NULL, NULL, subst, cap, acc);
}
#endif // THVM_ATPFT_NORM

// FT sibling of vcp_emit_fused: arm g_vcp_emit / g_acp_fuse from span
// composites (same side order, same signs).  0 on an anomaly -- caller
// falls back to the Term route.
#if defined(THVM_ATPFT_NORM)
static u8 vcp_ft_emit_fused(const KboConfig *cfg, const AtpFtCell *lo_ft,
                            const AtpFtCell *sub_ft,
                            const AtpFtCell *rin_ft,
                            const AtpFtCell *ro_ft,
                            const AtpFtSubst *subst) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
  if (!vcp_ft_emit_side(ro_ft, NULL, NULL, subst, cfg, &fz->map,
                        &pos, &nodes, fz, +1, &fpos)) {
    return 0u;
  }
  u32 lhs_nodes = nodes, fl = fpos;
  if (!vcp_ft_emit_side(lo_ft, sub_ft, rin_ft, subst, cfg, &fz->map,
                        &pos, &nodes, fz, -1, &fpos)) {
    return 0u;
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  g_vcp_emit.armed     = 0u;
  g_vcp_emit.len       = pos;
  g_vcp_emit.nodes     = nodes;
  g_vcp_emit.lhs_nodes = lhs_nodes;
  return 1u;
}

// FT sibling of vcp_treat_form: the treated raw pair's bytes (into
// g_acp_scratch[0, raw_len), fz == NULL walk) and cells, one shared
// appearance-order map across both sides -- identical map content to
// the Term fused walk (the byte walk of a side appends; the cell walk
// of the same composite only looks up).  Arms the g_vcp_treat stash on
// success; 0 on an anomaly with any built span returned.
static u8 vcp_ft_treat_form(AtpState *s, const AtpFtCell *lo_ft,
                            const AtpFtCell *sub_ft,
                            const AtpFtCell *rin_ft,
                            const AtpFtCell *ro_ft,
                            const AtpFtSubst *subst) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AcpNvMap m;
  m.n = 0u;
  // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
  // The raw-pair BYTE emit is deferred to the trace store (see
  // vcp_ft_treat_emit_raw): ~a third of treated CPs join at push and
  // never consume the bytes.  The builder shares the same appearance-
  // order var map the emit would use, so the deferred bytes are
  // byte-identical to the formation-time emit they replace.
  AtpFtCell *fl = vcp_ft_build_cell(a, ro_ft, NULL, NULL, subst, &m);
  if (fl == NULL) return 0u;
  const AtpFtCell *at2 = sub_ft;
  AtpFtCell *fr = vcp_ft_build_cell(a, lo_ft, &at2, rin_ft, subst, &m);
  if (fr == NULL) {
    ft_free_span(a, fl, fl->end);
    return 0u;
  }
  g_vcp_treat.armed   = 0u;
  g_vcp_treat.fl      = fl;
  g_vcp_treat.fr      = fr;
  g_vcp_treat.raw_len = 0u;
  g_vcp_treat.lo      = lo_ft;
  g_vcp_treat.sub     = sub_ft;
  g_vcp_treat.rin     = rin_ft;
  g_vcp_treat.ro      = ro_ft;
  g_vcp_treat.subst   = subst;
  return 1u;
}

// The deferred raw-pair emit: exactly the two vcp_ft_emit_side calls
// treat_form ran at formation, from the stashed operands + the still-
// armed walk subst.  Returns the byte length at g_acp_scratch[0,len)
// or 0 on an emit anomaly (the caller stores the no-pair sentinel;
// never observed -- the count walk screens the same cliffs).
static u32 vcp_ft_treat_emit_raw(AtpState *s) {
  AcpNvMap m;
  m.n = 0u;
  u32 raw_len = 0u, nodes = 0u, fpos = 0u;
  if (!vcp_ft_emit_side(g_vcp_treat.ro, NULL, NULL, g_vcp_treat.subst,
                        s->kbo, &m, &raw_len, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (!vcp_ft_emit_side(g_vcp_treat.lo, g_vcp_treat.sub, g_vcp_treat.rin,
                        g_vcp_treat.subst, s->kbo, &m, &raw_len, &nodes,
                        NULL, -1, &fpos)) {
    return 0u;
  }
  return raw_len;
}
#endif // THVM_ATPFT_NORM

// Route gate for the FT-direct emission: ALWAYS ON where the route
// conditions admit it (the THVM_ATP_FT_EMIT=0 one-round kill switch
// was retired 2026-07-04 after the bake round -- corpus certifier
// sweeps clean and every pin byte-identical either way).  The classic
// Term-build arm in sw_form_push is NOT deletable: it still serves
// every non-routed configuration (MNF, kapur, on-heap trace, fuse-off,
// batch former, anomaly fallbacks) AND it is the per-CP rebuild
// reference for the THVM_ATP_FT_EMIT_CHECK certifier -- deleting it
// would orphan the certification.  The size-hint kill/certifier envs
// force the historic Term-walk size gate, which needs the Terms --
// route off under either.
static int atp_ft_emit_on(void) {
  static int v = -1;
  if (v < 0) {
    v = !atp_env_on("THVM_NO_CP_SZHINT") &&
        !atp_env_on("THVM_ATP_SZHINT_CHECK");
  }
  return v;
}
static int atp_ft_emit_check_on(void) {
  static int v = -1;
  if (v < 0) v = atp_env_on("THVM_ATP_FT_EMIT_CHECK");
  return v;
}

// THVM_ATP_FT_EMIT_CHECK=1: per-CP differential certifier for the
// FT-direct emission (the FUSE_CHECK discipline).  Rebuilds the CP
// classically -- cp_replace_at + thvm_unify_apply x2, the exact Term
// route -- and asserts the virtual emit produced byte-identical
// packed bytes, identical node counts (== the size-gate verdict),
// an identical fused-KBO state (renumber map / balances / flat
// nodes) and the identical weigh verdict.  Aborts on any mismatch.
// Must run while the unifier is still armed.
static void vcp_emit_certify(const KboConfig *kbo, Term lo, const u32 *p,
                             u32 p_len, Term rin, Term ro,
                             const RewriteSubst *subst) {
  // Snapshot the virtual results (bytes + fuse state + verdict).
  u32 vlen = g_vcp_emit.len, vnodes = g_vcp_emit.nodes;
  u32 vlhs = g_vcp_emit.lhs_nodes;
  u8 *vbytes = (u8 *)malloc(vlen ? vlen : 1u);
  if (vbytes == NULL) thvm_fatal("vcp_emit_certify: OOM");
  memcpy(vbytes, g_acp_scratch, vlen);
  AcpFuse vfz = g_acp_fuse;   // struct copy: map, phidiff, bal, counts
  u32 vfn = vfz.fl_n + vfz.fr_n;
  AcpFlatNode *vflat = (AcpFlatNode *)malloc((size_t)vfn * sizeof *vflat);
  if (vflat == NULL) thvm_fatal("vcp_emit_certify: OOM");
  memcpy(vflat, g_acpf_pool, (size_t)vfn * sizeof *vflat);
  KboCmp vk = acp_fuse_kbo(kbo);
  // Classic rebuild on the same armed subst (WM order: sigma(ro) left).
  u64 c0 = g_unify_apply_cells, t0 = g_unify_apply_taint;
  Term cl = thvm_unify_apply(ro, subst);
  Term cr = thvm_unify_apply(cp_replace_at(lo, p, p_len, rin), subst);
  u32 ctot = (g_unify_apply_taint == t0)
                 ? (u32)(g_unify_apply_cells - c0) : 0u;
  u32 rlen = 0u, rn = 0u, rl = 0u;
  u8 *ref = acp_pack_fused(kbo, cl, cr, &rlen, &rn, &rl);
  u8 bad = 0u;
  if (ref == NULL || rlen != vlen || memcmp(ref, vbytes, vlen) != 0) bad = 1u;
  if (rn != vnodes || rl != vlhs) bad = 1u;
  // Size-gate verdict: the FT route fired on vcount >= gate, so the
  // untainted classic tally must be at/over the gate too.
  if (ctot != 0u && ctot < (u32)ATP_WM_BEHANDELN_GATE) bad = 1u;
  if (!bad) {
    const AcpFuse *cf = &g_acp_fuse;
    if (cf->map.n != vfz.map.n || cf->phidiff != vfz.phidiff ||
        cf->fl_n != vfz.fl_n || cf->fr_n != vfz.fr_n) {
      bad = 1u;
    }
    for (u32 k = 0; !bad && k < vfz.map.n; k++) {
      if (cf->map.old_id[k] != vfz.map.old_id[k] ||
          cf->bal[k] != vfz.bal[k]) bad = 1u;
    }
    if (!bad &&
        memcmp(g_acpf_pool, vflat, (size_t)vfn * sizeof *vflat) != 0) {
      bad = 1u;
    }
    if (!bad && acp_fuse_kbo(kbo) != vk) bad = 1u;
  }
  if (bad) {
    fprintf(stderr,
            "ATP_FT_EMIT_CHECK MISMATCH len=%u/%u nodes=%u/%u lhs=%u/%u "
            "ctot=%u kbo=%d\n",
            vlen, rlen, vnodes, rn, vlhs, rl, ctot, (int)vk);
    abort();
  }
  free(ref);
  free(vbytes);
  free(vflat);
  // g_acp_fuse + the scratch now hold the classic (identical) state;
  // the FT route proceeds on them.
}

// Count nodes in one packed preorder term, advancing `*pp` past it.
// Non-allocating (unlike acp_unpack_term) so it is safe to call from the
// hot CP-selection comparator without touching the term heap.
static u32 acp_term_nodes_skip(const u8 **pp) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      (void)acp_get_varint(pp);           // label
      u32 n = (u32)acp_get_varint(pp);    // arity
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      u32 tot = 1u;
      for (u32 i = 0; i < n; i++) tot += acp_term_nodes_skip(pp);
      return tot;
    }
    case 'N':
      (void)acp_get_varint(pp);           // dtype
      (void)acp_get_varint(pp);           // val
      return 1u;
    case 'V':
    default:
      (void)acp_get_varint(pp);           // ext
      return 1u;
  }
}

// Total node count of a packed CP's rhs (skip lhs, size rhs).  Used by the
// weight-tie tiebreak to distinguish compound CPs (large rhs) from
// collapsers (bare-variable rhs) at selection time.
static u32 acp_rhs_nodes(const u8 *buf) {
  const u8 *p = buf;
  (void)acp_term_nodes_skip(&p);          // skip lhs
  return acp_term_nodes_skip(&p);         // size rhs
}

// One-way match of a PACKED pattern term against a heap Term subject --
// the Stringterms counterpart of thvm_match (rewrite/_.c:27), and the
// Waldmeister technique of matching on the packed representation
// directly.  The pattern is never unpacked to a heap tree: the matcher
// walks the preorder byte string and the subject term in lockstep, so
// a head-symbol mismatch fast-fails after one discriminator byte with
// zero allocation.  This is what keeps the subsumption index off the
// per-candidate acp_unpack that an unpack-then-thvm_match would pay.
//
// `*pp` advances past the pattern term on a full match; on a mismatch
// it is left mid-record (the caller aborts the whole match, so the
// stale cursor is never used).  Verdict is bit-identical to
// thvm_match: a NUM / ERA pattern matches nothing (thvm_match's
// default branch returns 0); a variable id past the REWRITE_MAX_VAR
// matcher cliff fails; a repeated variable is confirmed with kbo_eq.
static u8 acp_match_term(const u8 **pp, Term subj, RewriteSubst *sub) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (term_tag(subj) != TAG_CTR) return 0;
      if (term_ext(subj) != label)   return 0;
      if (term_ctr_n(subj) != n)     return 0;
      for (u32 i = 0; i < n; i++) {
        if (!acp_match_term(pp, term_ctr_at(subj, i), sub)) return 0;
      }
      return 1;
    }
    case 'V': {
      u32 id = (u32)acp_get_varint(pp);
      if (id >= REWRITE_MAX_VAR) return 0;
      if (sub->bindings[id] == 0) { sub->bindings[id] = subj; return 1; }
      return kbo_eq(sub->bindings[id], subj);
    }
    default:         // 'N' (NUM) / 'E' -- thvm_match's default: no match
      return 0;
  }
}

// Two-sided one-way match: does the packed CP `pack(plhs)++pack(prhs)`
// match (qlhs, qrhs) under a single shared substitution?  Equivalent
// to `thvm_match(plhs,qlhs,&s) && thvm_match(prhs,qrhs,&s)` with the
// pattern kept packed.  The lhs match advances the cursor exactly past
// `pack(plhs)` (preorder is self-delimiting), so the rhs match resumes
// at `pack(prhs)`.
static u8 acp_match_pair(const u8 *packed, Term qlhs, Term qrhs,
                         RewriteSubst *sub) {
  const u8 *p = packed;
  if (!acp_match_term(&p, qlhs, sub)) return 0;
  return acp_match_term(&p, qrhs, sub);
}

// One-time port self-check, run at engine init: a hand-built pair --
// nested CTRs, repeated variable, a NUM -- must survive a pack/unpack
// round-trip structurally intact, so a broken Stringterms port fails
// loudly and immediately rather than corrupting the CP queue later.
static void acp_selftest(void) {
  static u8 done = 0;
  if (done) return;
  done = 1;
  Term v0 = term_new_fvr(0u), v1 = term_new_fvr(1u);
  Term inner[2]  = { v0, v1 };
  Term f         = term_new_ctr(7u, inner, 2u);          // f(v0, v1)
  Term spine[2]  = { f, v0 };                            // var v0 repeats
  Term lhs       = term_new_ctr(7u, spine, 2u);          // f(f(v0,v1), v0)
  Term rhs       = term_new(0, TAG_NUM, 0u, 42u);        // NUM 42
  u32  len = 0;
  u8  *packed = acp_pack(lhs, rhs, &len, NULL);
  Term ul = 0, ur = 0;
  acp_unpack(packed, &ul, &ur);
  free(packed);
  if (!kbo_eq(lhs, ul) || !kbo_eq(rhs, ur)) {
    fprintf(stderr, "acp_selftest: Stringterms round-trip FAILED\n");
    exit(1);
  }
}

// Pack (lhs, rhs) into CP queue slot i, freeing any byte string
// already there.  Callers that build the queue directly -- chiefly
// tests -- use this instead of writing the (now packed) queue slots
// by hand.  The insertion age (cp_seq) is stamped here; the slot's
// priority is filled by a subsequent thvm_atp_cp_reheapify.
fn void thvm_atp_cp_set(AtpState *s, u32 i, Term lhs, Term rhs) {
  if (s == NULL) return;
  atp_ensure_cp_cap(s, i + 1u);
  // Position-indirection mapping: overwriting a LIVE queue slot means
  // "replace the CP at heap position i" (the historical position ==
  // payload-index semantics); a fresh build slot beyond n_cps stamps
  // the identity mapping -- callers populate [0, k) then set n_cps and
  // reheapify, and identity reproduces the historical fresh layout.
  u32 row;
  if (i < s->n_cps) {
    row = s->cp_heap[i].row;
  } else {
    // Fresh build slot: identity mapping.  cp_pri[row] is not defined
    // yet (the contract fills it via thvm_atp_cp_reheapify, whose
    // rebuild restamps the packed caches per position before any
    // compare), so stamp a zero pri cache rather than read garbage.
    s->cp_heap[i].pri = 0u;
    s->cp_heap[i].seq = 0u;
    s->cp_heap[i].row = i;
    s->cp_heap[i].aux = 0u;
    s->cp_pos[i]  = i;
    row = i;
  }
  free(s->cp_packed[row]);               // free(NULL) is a no-op
  // The slot now holds packed bytes -- drop any deferred-CP tag so the
  // `cp_is_implicit[i] <=> cp_packed[i] == NULL` invariant holds, and
  // retire any raw-desc descriptor (the bytes are being replaced).
  atp_cp_implicit_clear(s, row);
  if (s->cp_implicit != NULL) {
    s->cp_implicit[row].out_face = ATP_RDESC_NONE;
  }
  s->cp_packed[row] = acp_pack(lhs, rhs, NULL, NULL);
  // Insertion age, stamped once here (mirroring the heap-push path) and
  // preserved across every later sweep / reheapify -- WM keeps the w2
  // FIFO key untouched on reweight (C_ReClassify, CLAS/
  // NewClassification.c:399-406 "w2 wird nicht geaendert").
  s->cp_seq[row] = s->cp_seq_next++;
  // Refresh the packed seq cache at position i (cp_seq[row] just
  // changed for a possibly-QUEUED row).  The pri cache is left to the
  // contract-mandated reheapify, exactly like cp_pri[row] itself.
  s->cp_heap[i].seq = s->cp_seq[row];
#ifdef THVM_ATPFT_CPQ
  // Stage 7: mirror the slot into the parallel FT queue.  Test
  // harnesses populate the queue via thvm_atp_cp_set + reheapify;
  // keep the two views in lockstep (entries live at the same
  // stationary rows as the legacy arrays).
  atp_cp_ft_clear(s, row);
  atp_cp_ft_set(s, row, lhs, rhs, /*priority_hint=*/0u, /*origin=*/0xffffu);
#endif
}

// Read CP queue slot i back into two Terms -- the read counterpart of
// thvm_atp_cp_set.  A packed slot unpacks fresh transient heap Terms;
// a deferred (implicit) slot returns its trace-resident raw pair
// zero-copy.  Read-only either way.  `i` is a heap POSITION (the
// public queue-slot notion); the payload row is mapped internally.
static void atp_cp_slot_read(const AtpState *s, u32 i,
                             Term *lhs, Term *rhs);
fn void thvm_atp_cp_get(const AtpState *s, u32 i, Term *lhs, Term *rhs) {
  atp_cp_slot_read(s, s->cp_heap[i].row, lhs, rhs);
}


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
//   ATP_DTREE_NUM             a TAG_NUM atom
//   ATP_DTREE_STAR_BASE + k   the k-th DISTINCT pattern variable
//   ATP_DTREE_CTR_BASE  + lab a TAG_CTR with label `lab`
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

#if defined(ATP_FV_INDEX) || defined(ATP_RULE_INDEX)

// Shared discrim-tree symbol encoding (used by both the FV-index for
// CP subsumption and the RI-index for rule-LHS redex matching).  The
// two indices store records of different types but use the same flat-
// symbol space + per-term variable renumbering.
//
// Flat-symbol alphabet for a discrimination-tree edge.  Ordering
// matters: NUM < every STAR(k) < every CTR(lab), so the sym-ascending
// child list lets a descent stop scanning early.
#define ATP_DTREE_NUM        0u                                 // TAG_NUM atom
#define ATP_DTREE_MAXVARS    64u                                // distinct vars per term
#define ATP_DTREE_STAR_BASE  1u                                 // STAR(k) = STAR_BASE + k
#define ATP_DTREE_CTR_BASE   (ATP_DTREE_STAR_BASE + ATP_DTREE_MAXVARS)
#define ATP_DTREE_NIL        0xFFFFFFFFu

// Per-term variable renumbering: maps a raw TAG_FVR id to its
// first-appearance index 0,1,2,...  `slot[id]` holds (index+1), 0 =
// not yet seen.  Reset per term at insert and per orientation at
// retrieval.  `folded` records whether any variable hit an imperfect
// case -- an id >= REWRITE_MAX_VAR, or more than ATP_DTREE_MAXVARS
// distinct vars -- where two distinct variables collapse onto one star
// slot.  A flatten with folded == 0 distinguishes every variable, so a
// discrimination-tree descent that reaches a leaf is then an exact
// match proof; otherwise the leaf re-confirms via thvm_match.
typedef struct { u32 slot[REWRITE_MAX_VAR]; u32 n; u8 folded; } AtpDTreeVarMap;

static void atp_dtree_varmap_reset(AtpDTreeVarMap *vm) {
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) vm->slot[i] = 0;
  vm->n = 0;
  vm->folded = 0;
}

// First-appearance index of variable id `vid`.  Ids >= REWRITE_MAX_VAR,
// or more than ATP_DTREE_MAXVARS distinct vars, fold onto the last slot
// -- sound (folding only coarsens the tree, never drops a candidate).
static u32 atp_dtree_var_index(AtpDTreeVarMap *vm, u32 vid) {
  if (vid >= REWRITE_MAX_VAR) { vid = REWRITE_MAX_VAR - 1u; vm->folded = 1u; }
  if (vm->slot[vid] == 0) {
    u32 idx;
    if (vm->n < ATP_DTREE_MAXVARS) { idx = vm->n; vm->n++; }
    else                           { idx = ATP_DTREE_MAXVARS - 1u; vm->folded = 1u; }
    vm->slot[vid] = idx + 1u;
  }
  return vm->slot[vid] - 1u;
}

// flatsym of one term node under variable renumbering `vm`.
static u32 atp_dtree_flatsym(Term t, AtpDTreeVarMap *vm) {
  switch (term_tag(t)) {
    case TAG_CTR: return ATP_DTREE_CTR_BASE + term_ext(t);
    case TAG_NUM: return ATP_DTREE_NUM;
    case TAG_FVR:
    default:      return ATP_DTREE_STAR_BASE + atp_dtree_var_index(vm, term_ext(t));
  }
}

// A discrimination-tree node: left-child / right-sibling in a flat
// realloc-grown pool addressed by u32 INDEX (no pointers, so a pool
// realloc never invalidates the structure).  `sym` is the flat symbol
// of the edge INTO this node.  `rec_head` heads the leaf record list.
// Shared layout between the FV-index (CP subsumption) and the RI-index
// (rule-LHS redex matching) -- the only difference at the leaf is the
// RecT type, threaded through ATP_DTREE_DEFINE_OPS below.
typedef struct {
  u32 sym;        // flat symbol of the in-edge
  u32 child;      // first child node index, or ATP_DTREE_NIL
  u32 sibling;    // next sibling node index, or ATP_DTREE_NIL
  u32 rec_head;   // first record index, or ATP_DTREE_NIL
} AtpDTreeNode;

// Template macro for the four tree-management fns shared by the FV
// and RI indices.  Expands to:
//   atp_<stem>_node_new(IndexT *ix, u32 sym) -> u32     (pool alloc)
//   atp_<stem>_rec_new (IndexT *ix)          -> u32     (pool alloc)
//   atp_<stem>_child   (IndexT *ix, u32 par, u32 sym) -> u32
//   atp_<stem>_insert_term(IndexT *ix, u32 node, Term t, AtpDTreeVarMap *)
//
// The four fns are TEXTUALLY identical apart from the IndexT / RecT
// types and the err-print stem; macro-expanding to two specialized
// `static` definitions keeps the call sites direct (no fn-pointer
// indirection or void* round-trip) so the inliner stays free on the
// hot path -- atp_dt_descend_rec reads ix->nodes[] directly and does
// not go through these helpers at all, but the insert/rebuild paths
// (and any other future helper added under one expansion) inline
// identically to the hand-written form.
#define ATP_DTREE_DEFINE_OPS(stem, IndexT, RecT)                              \
  static u32 atp_##stem##_node_new(IndexT *ix, u32 sym) {                     \
    if (ix->n_nodes == ix->cap_nodes) {                                       \
      u32 cap = ix->cap_nodes ? ix->cap_nodes * 2u : 1024u;                   \
      AtpDTreeNode *p = (AtpDTreeNode *)realloc(                              \
        ix->nodes, cap * sizeof(AtpDTreeNode));                               \
      if (p == NULL) thvm_fatal("atp_" #stem ": node pool OOM"); \
      ix->nodes = p;                                                          \
      ix->cap_nodes = cap;                                                    \
    }                                                                         \
    u32 i = ix->n_nodes++;                                                    \
    ix->nodes[i].sym      = sym;                                              \
    ix->nodes[i].child    = ATP_DTREE_NIL;                                    \
    ix->nodes[i].sibling  = ATP_DTREE_NIL;                                    \
    ix->nodes[i].rec_head = ATP_DTREE_NIL;                                    \
    return i;                                                                 \
  }                                                                           \
                                                                              \
  static u32 atp_##stem##_rec_new(IndexT *ix) {                               \
    if (ix->n_recs == ix->cap_recs) {                                         \
      u32 cap = ix->cap_recs ? ix->cap_recs * 2u : 1024u;                     \
      RecT *p = (RecT *)realloc(ix->recs, cap * sizeof(RecT));                \
      if (p == NULL) thvm_fatal("atp_" #stem ": rec pool OOM"); \
      ix->recs = p;                                                           \
      ix->cap_recs = cap;                                                     \
    }                                                                         \
    return ix->n_recs++;                                                      \
  }                                                                           \
                                                                              \
  /* Find `parent`'s child reached by edge `sym`, creating it absent.    */   \
  /* Children kept in ascending-sym order -- deterministic across runs. */    \
  static u32 atp_##stem##_child(IndexT *ix, u32 parent, u32 sym) {            \
    u32 prev = ATP_DTREE_NIL;                                                 \
    u32 cur  = ix->nodes[parent].child;                                       \
    while (cur != ATP_DTREE_NIL && ix->nodes[cur].sym < sym) {                \
      prev = cur;                                                             \
      cur  = ix->nodes[cur].sibling;                                          \
    }                                                                         \
    if (cur != ATP_DTREE_NIL && ix->nodes[cur].sym == sym) return cur;        \
    u32 nn = atp_##stem##_node_new(ix, sym);  /* may realloc the pool */      \
    ix->nodes[nn].sibling = cur;                                              \
    if (prev == ATP_DTREE_NIL) ix->nodes[parent].child = nn;                  \
    else                       ix->nodes[prev].sibling = nn;                  \
    return nn;                                                                \
  }                                                                           \
                                                                              \
  /* Walk term `t` in preorder, descending the tree by flatsym per node, */   \
  /* creating edges as needed.  `vm` renumbers variables by first        */   \
  /* appearance.  Returns the node reached after `t`'s whole preorder    */   \
  /* string.  A TAG_CTR's children extend the string in left-to-right    */   \
  /* order; a TAG_FVR / TAG_NUM is one symbol.                           */   \
  static u32 atp_##stem##_insert_term(IndexT *ix, u32 node, Term t,           \
                                      AtpDTreeVarMap *vm) {                   \
    node = atp_##stem##_child(ix, node, atp_dtree_flatsym(t, vm));            \
    if (term_tag(t) == TAG_CTR) {                                             \
      u32 n = term_ctr_n(t);                                                  \
      for (u32 i = 0; i < n; i++) {                                           \
        node = atp_##stem##_insert_term(ix, node, term_ctr_at(t, i), vm);     \
      }                                                                       \
    }                                                                         \
    return node;                                                              \
  }

// Shared recursion-depth cap for the three DT-descent functions
// (atp_dt_descend, atp_ri_descend, atp_ri_descend_unorient) -- lives
// in the shared block because the latter two compile under
// ATP_RULE_INDEX alone.  The descent can develop a STAR-edge cycle
// under Waldmeister + LRS + RandomRatio on andassoc -- empirical
// bisection on that combo:
//   cap=1024  -> 1294 bails / 30s, engine exits cleanly       (sweet spot)
//   cap=4096  -> 591 bails / 30s, still clean
//   cap=8192  -> wall-cap timeout (deep descents dominate compute)
//   cap=16384 -> SIGSEGV on the macOS main-thread 8MB stack
// Past ~4096 depth the search is super-linear: each level branches over
// multiple STAR alternatives so the cost grows fast.  1024 keeps queries
// bounded while still surfacing the cycles via q_depth_capped for the
// upstream fv_index_insert / atp_dt_flatten investigation.
#define ATP_DT_DESCENT_DEPTH_CAP 1024u

#endif  // ATP_FV_INDEX || ATP_RULE_INDEX

#ifdef ATP_FV_INDEX

// Preorder-flattened subject cap.  Sized to hold a deep-saturation
// critical pair: at thm step ~230 a ~1% tail of queued CPs flattens
// past the old 4096, and each such query spilled to the O(n_recs)
// full scan -- ~500M thvm_match calls, the late-game wall.  32768
// keeps that tail on the perfect-tree descent; the descent's STAR
// recursion is then bounded by half the cap (atp_dt_descend loops the
// CTR spine), well within the stack.  A still-bigger term aborts to
// the full scan (correct, never a silent under-retrieval).
#define ATP_DT_FLAT_CAP   32768u

// One indexed CP.  `packed` BORROWS the queue's cp_packed[] byte
// string for this CP -- the queue owns and frees it; the index only
// reads it (and only while `live`).  Storing the packed pointer, not a
// pair of heap Terms, keeps the collector out of the index entirely.
// `live` is cleared when the CP is popped / dropped -- a dead record
// is skipped by retrieval (its `packed` may by then dangle, but a dead
// record is never dereferenced) and reclaimed by the next index
// rebuild.  `seq` is the CP's stable id (the seq->record map key);
// `next` links the leaf list.
typedef struct {
  u8  *packed;
  u32  seq;
  u32  next;
  u8   live;
  u8   folded;    // this CP folded a var -> descent inexact for it
} AtpDtRec;

// seq -> record-index open-addressing hash entry (NIL = empty).
typedef struct { u32 seq; u32 rec; } AtpDtSeqEnt;

// The index.  Named `struct AtpFvIndex` -- the flag and the opaque
// thvm.h forward declaration are spelled that way; the structure
// inside is the discrimination tree the measurement settled on.  The
// node pool uses the shared AtpDTreeNode layout (see above); the leaf
// records carry CP-specific lifecycle (packed pointer + live flag).
struct AtpFvIndex {
  AtpDTreeNode *nodes;
  u32           n_nodes, cap_nodes;
  AtpDtRec     *recs;
  u32           n_recs, cap_recs;        // n_recs == GC-rooted span
  u32           n_live;                  // live record count (== n_cps)
  AtpDtSeqEnt  *seqmap;                   // seq -> rec index
  u32           seqmap_cap;               // power of two
  u32           root;                     // tree root node index
  // Instrumentation (cheap counters, always compiled).
  u64 q_calls;            // atp_cp_queue_subsumed queries
  u64 q_candidates;       // leaf records reached by retrieval
  u64 q_matchcalls;       // thvm_match calls issued on candidates
  u64 q_nodevisits;       // discrimination-tree nodes touched
  u64 q_depth_capped;     // recursion-depth bails (see atp_dt_descend);
                          // nonzero indicates the latent DT-cycle bug
                          // fired -- worth investigating upstream.
};
typedef struct AtpFvIndex AtpFvIndex;

// Specialize the tree-management template (atp_dt_node_new / _rec_new /
// _child / _insert_term) over AtpFvIndex + AtpDtRec.
ATP_DTREE_DEFINE_OPS(dt, AtpFvIndex, AtpDtRec)

// --- seq -> record map (open addressing, linear probe) -------------

static void atp_dt_seqmap_init(AtpFvIndex *ix, u32 cap) {
  ix->seqmap_cap = cap;
  ix->seqmap = (AtpDtSeqEnt *)malloc(cap * sizeof(AtpDtSeqEnt));
  if (ix->seqmap == NULL) thvm_fatal("atp_dt: seqmap OOM");
  for (u32 i = 0; i < cap; i++) ix->seqmap[i].seq = ATP_DTREE_NIL;
}

static void atp_dt_seqmap_put(AtpFvIndex *ix, u32 seq, u32 rec);

static void atp_dt_seqmap_grow(AtpFvIndex *ix) {
  u32 old_cap = ix->seqmap_cap;
  AtpDtSeqEnt *old = ix->seqmap;
  atp_dt_seqmap_init(ix, old_cap * 2u);
  for (u32 i = 0; i < old_cap; i++) {
    if (old[i].seq != ATP_DTREE_NIL) atp_dt_seqmap_put(ix, old[i].seq, old[i].rec);
  }
  free(old);
}

static void atp_dt_seqmap_put(AtpFvIndex *ix, u32 seq, u32 rec) {
  if ((ix->n_live + 1u) * 2u > ix->seqmap_cap) atp_dt_seqmap_grow(ix);
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DTREE_NIL) h = (h + 1u) & mask;
  ix->seqmap[h].seq = seq;
  ix->seqmap[h].rec = rec;
}

static u32 atp_dt_seqmap_get(const AtpFvIndex *ix, u32 seq) {
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DTREE_NIL) {
    if (ix->seqmap[h].seq == seq) return ix->seqmap[h].rec;
    h = (h + 1u) & mask;
  }
  return ATP_DTREE_NIL;
}

// Delete `seq` (Knuth back-shift, keeps probe chains intact).
static void atp_dt_seqmap_del(AtpFvIndex *ix, u32 seq) {
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DTREE_NIL && ix->seqmap[h].seq != seq) {
    h = (h + 1u) & mask;
  }
  if (ix->seqmap[h].seq == ATP_DTREE_NIL) return;
  u32 j = h;
  for (;;) {
    ix->seqmap[h].seq = ATP_DTREE_NIL;
    u32 k;
    do {
      j = (j + 1u) & mask;
      if (ix->seqmap[j].seq == ATP_DTREE_NIL) return;
      k = (ix->seqmap[j].seq * 2654435761u) & mask;
    } while ((h <= j) ? (h < k && k <= j) : (h < k || k <= j));
    ix->seqmap[h] = ix->seqmap[j];
    h = j;
  }
}

// --- index lifecycle -----------------------------------------------

static AtpFvIndex *atp_fv_index_new(void) {
  AtpFvIndex *ix = (AtpFvIndex *)calloc(1, sizeof(AtpFvIndex));
  if (ix == NULL) thvm_fatal("atp_dt: index OOM");
  atp_dt_seqmap_init(ix, 1024u);
  ix->root = atp_dt_node_new(ix, ATP_DTREE_NIL);  // root edge unused
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

// Insert CP (lhs, rhs) with stable id `seq`.  The CP is indexed as
// the synthetic term `Cp(lhs, rhs)` so a single tree spans both
// sides; ATP_CP_LABEL is the synthetic `Cp` head.
// One variable renumbering spans the WHOLE CP -- a variable shared
// between lhs and rhs (the common case for an equation) keeps one
// star index across both sides.
// `packed` is the queue's cp_packed[] byte string for this CP; the
// record borrows it (queue owns and frees).  `lhs`/`rhs` are the
// unpacked terms, needed only to descend the discrimination tree.
static void atp_fv_index_insert(AtpFvIndex *ix, Term lhs, Term rhs,
                                u8 *packed, u32 seq) {
  AtpDTreeVarMap vm;
  atp_dtree_varmap_reset(&vm);
  u32 node = ix->root;
  node = atp_dt_child(ix, node, ATP_DTREE_CTR_BASE + ATP_CP_LABEL);  // Cp head
  node = atp_dt_insert_term(ix, node, lhs, &vm);
  node = atp_dt_insert_term(ix, node, rhs, &vm);
  u32 rec = atp_dt_rec_new(ix);
  ix->recs[rec].packed = packed;
  ix->recs[rec].seq    = seq;
  ix->recs[rec].live   = 1u;
  ix->recs[rec].folded = vm.folded;
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
  if (rec == ATP_DTREE_NIL) return;
  if (ix->recs[rec].live) {
    ix->recs[rec].live = 0u;
    ix->n_live--;
  }
  atp_dt_seqmap_del(ix, seq);
}

// In-loop GC trigger threshold in bytes.  Default 24MB (historical);
// THVM_ATP_GC_MB=<n> lowers it to run the Cheney collector more often,
// bounding the term-heap peak on deep over-generating searches (Wolfram)
// so the completion can reach saturation without exhausting the heap.
static u64 atp_gc_threshold_bytes(void) {
  static long long v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_GC_MB");
    v = e ? (long long)strtoull(e, NULL, 10) : 24;
    if (v < 1) v = 1;
  }
  return (u64)v << 20;
}

// Discard every record / node and rebuild the tree from the live CP
// arrays.  Used when a wholesale CP-set mutation (e.g. a test
// populating the arrays directly) reshuffles seqs out from under
// the incremental path.
static void atp_fv_index_rebuild(AtpState *s) {
  AtpFvIndex *ix = s->fv_index;
  if (ix == NULL) return;
  // The WM presets disable use_queue_subsume (the queue must match WM's
  // FIFO composition), so the index is never queried -- rebuilding it is
  // pure waste, and its tree dominates the GC heap on large saturations.
  // The insert is gated the same way, so the index stays empty; skipping
  // the rebuild is byte-identical to the saturation (nothing reads it).
  if (!s->use_queue_subsume) return;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->n_live  = 0;
  ix->root    = atp_dt_node_new(ix, ATP_DTREE_NIL);
  for (u32 i = 0; i < ix->seqmap_cap; i++) ix->seqmap[i].seq = ATP_DTREE_NIL;
  for (u32 i = 0; i < s->n_cps; i++) {
    // Heap-position walk (row mapped per slot) -- the historical
    // payload-index iteration visited CPs in position order.
    u32 vv = s->cp_heap[i].row;
    // Deferred-CP slots are never indexed: there are no packed bytes to
    // borrow, and queue-subsumption deliberately skips the implicit
    // passive set (WM has no queue-vs-queue subsumption at all --
    // SS_TermpaarSubsummiertVonGM matches only the ACTIVE rule set).
    if (atp_cp_slot_implicit(s, vv)) continue;
    // Bound the transient unpack (see atp_cp_rebuild_priorities): l,r are
    // consumed by the feature insert and not kept by the index, so a Cheney
    // pass between iterations reclaims them without touching the live index.
    if (s->trace_pack && (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {
      thvm_atp_gc_collect(s);
    }
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[vv], &l, &r);
    atp_fv_index_insert(ix, l, r, s->cp_packed[vv], s->cp_seq[vv]);
  }
}

// --- retrieval -----------------------------------------------------

// Preorder-flatten `t` from index `*pos`, recording per position the
// SUBTREE SIZE (preorder-position span) in `subsz[]` and the flat
// SYMBOL CODE in `flatsym[]` -- CTR_BASE+lab / NUM / STAR+idx under
// the variable renumbering `vm`.  The flatsym string drives the
// perfect-tree descent: a CTR/NUM symbol matches exactly, a repeat
// pattern variable is confirmed by a flatsym-slice memcmp.  Returns 1
// on success, 0 if the cap is hit (caller falls back to the full scan
// -- never silently under-retrieves).
static u8 atp_dt_flatten(Term t, u32 *subsz, u32 *flatsym,
                         AtpDTreeVarMap *vm, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flatsym[here] = atp_dtree_flatsym(t, vm);
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!atp_dt_flatten(term_ctr_at(t, i), subsz, flatsym, vm, cap, pos))
        return 0;
    }
  }
  subsz[here] = *pos - here;          // positions covered by t's subtree
  return 1;
}

// One retrieval's immutable parameters, threaded through the
// recursion without a wide signature.  The saturation engine is
// single-threaded (one AtpState per run), so file-static query
// scratch is safe and keeps the hot descent lean.  `g_atp_dt_star`
// is the descent's per-path binding array: star[k] is the PREORDER
// POSITION the k-th pattern variable was first bound to (0 = unbound;
// real positions are >= 1 since flat[0] is the synthetic Cp head).
static AtpFvIndex *g_atp_dt_ix      = NULL;
static const u32  *g_atp_dt_subsz   = NULL;
static const u32  *g_atp_dt_flatsym = NULL;  // per-position flat-symbol code
static u32         g_atp_dt_flatlen = 0;
static Term        g_atp_dt_qlhs    = 0;
static Term        g_atp_dt_qrhs    = 0;
static u32         g_atp_dt_star[ATP_DTREE_MAXVARS];
static u8          g_atp_dt_query_folded = 0;  // subject flatten folded a var

// A leaf reached by the descent: report whether a live CP sits here.
// When neither THIS CP nor the subject folded a variable (the common
// case after thvm_normalize_vars), the flatsym descent has already
// proved both sides exactly -- CTR/NUM symbols matched exactly,
// variable consistency by flatsym-slice memcmp -- so the live record
// IS the subsumer and the two-sided thvm_match is skipped.  A fold
// makes that record's path coarser; it alone re-runs the SAME
// thvm_match the array scan used as the authoritative guard -- one
// oversized CP no longer poisons the fast path for its leaf-mates.
static u8 atp_dt_leaf_match(u32 node) {
  AtpFvIndex *ix = g_atp_dt_ix;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DTREE_NIL;
       r = ix->recs[r].next) {
    AtpDtRec *rc = &ix->recs[r];
    if (!rc->live) continue;
    ix->q_candidates++;
    if (!rc->folded && !g_atp_dt_query_folded) return 1;  // exact descent
    // Match the stored CP straight off its packed byte string -- no
    // per-candidate acp_unpack (the late-game FV-index hot path).
    RewriteSubst subst = {{0}};
    ix->q_matchcalls += 2u;
    if (acp_match_pair(rc->packed, g_atp_dt_qlhs, g_atp_dt_qrhs, &subst)) {
      return 1;
    }
  }
  return 0;
}

// Walk the flattened subject from preorder index `pos` in lockstep
// with tree node `node`, threading the per-path variable bindings in
// g_atp_dt_star.  Returns 1 as soon as a reachable leaf yields a
// genuine subsumer.
//
// A CTR/NUM subject head matches at most ONE child, so that branch is
// a tail continuation followed by LOOPING (advance node/pos in place)
// rather than recursing -- only the STAR branches recurse.  This both
// drops call overhead on the CTR spine and bounds the recursion depth
// to the path's STAR-edge count, so a deep (ATP_DT_FLAT_CAP-long)
// subject cannot overflow the stack.
//
static u8 atp_dt_descend_rec(u32 node, u32 pos, u32 depth);
static u8 atp_dt_descend(u32 node, u32 pos) {
  return atp_dt_descend_rec(node, pos, 0u);
}
static u8 atp_dt_descend_rec(u32 node, u32 pos, u32 depth) {
  AtpFvIndex *ix = g_atp_dt_ix;
  // Hard recursion-depth cap.  Under the Waldmeister + LRS + Random
  // combo the DT can develop a STAR-edge cycle (5000+ frames observed
  // before stack overflow on andassoc).  Cap well below the 512KB
  // thread-stack guard at ~100 bytes/frame.  Returning no-match is
  // sound: any CP genuinely subsuming the subject would be reached
  // before this depth in a well-formed DT.
  if (depth >= ATP_DT_DESCENT_DEPTH_CAP) {
    ix->q_depth_capped++;
    // THVM_ATP_DT_TRACE=1 dumps the (node, pos, flatlen) of the first 4
    // cap hits per process so an investigator can spot whether the cap
    // fires repeatedly at the same node (= true cycle) or scatters
    // across nodes (= just deep search).  4 samples is enough to tell
    // the patterns apart while staying near-silent on production runs.
    {
      static u32 dumped = 0u;
      if (dumped < 4u && atp_env_on("THVM_ATP_DT_TRACE")) {
        fprintf(stderr,
                "atp_dt_descend depth-cap hit #%u: node=%u pos=%u flatlen=%u\n",
                dumped + 1u, node, pos, g_atp_dt_flatlen);
        dumped++;
      }
    }
    return 0;
  }
  for (;;) {
    ix->q_nodevisits++;
    if (pos == g_atp_dt_flatlen) {
      // Whole subject consumed -- this node's records are leaves.
      return atp_dt_leaf_match(node);
    }
    if (pos > g_atp_dt_flatlen) return 0;   // overshoot safety
    u32  sz         = g_atp_dt_subsz[pos];   // preorder span of t's subtree
    u32  csym_exact = g_atp_dt_flatsym[pos]; // CTR_BASE+lab / NUM / STAR+idx
    if (sz == 0u) return 0;                  // would stall at same pos
    u32  ctr_next   = ATP_DTREE_NIL;            // the lone CTR/NUM-match child
    for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_DTREE_STAR_BASE && csym < ATP_DTREE_CTR_BASE) {
        // Stored variable, the (csym-STAR_BASE)-th distinct pattern
        // var.  One-way match: the FIRST occurrence binds it to this
        // subterm's preorder position; a REPEAT applies only if the two
        // subterms' flatsym slices are byte-identical.
        u32 k = csym - ATP_DTREE_STAR_BASE;
        u32 bound = g_atp_dt_star[k];
        if (bound == 0) {
          g_atp_dt_star[k] = pos;             // first occurrence: bind
          u8 hit = atp_dt_descend_rec(c, pos + sz, depth + 1u);
          g_atp_dt_star[k] = 0;               // unbind on backtrack
          if (hit) return 1;
        } else if (g_atp_dt_subsz[bound] == sz &&
                   memcmp(&g_atp_dt_flatsym[bound], &g_atp_dt_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          if (atp_dt_descend_rec(c, pos + sz, depth + 1u)) return 1;
        }
      } else if (csym == csym_exact) {
        // Stored CTR/NUM equal to t's own symbol -- consume t's head;
        // t's children are the next preorder positions.
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_DTREE_NIL) return 0;     // no CTR continuation: done
    node = ctr_next;                          // tail-continue without a call
    pos  = pos + 1u;
  }
}

// Retrieve over one orientation: descend the tree for the synthetic
// subject `Cp(o_lhs, o_rhs)`.  Returns 1 if a queued CP subsumes the
// candidate in this orientation.
static u8 atp_dt_query_orient(AtpFvIndex *ix, Term o_lhs, Term o_rhs) {
  // Static (not stack): ATP_DT_FLAT_CAP is large enough that a 512 KB
  // pair of arrays would overflow the frame.  The saturation engine is
  // single-threaded and atp_dt_query_orient does not recurse, so one
  // shared scratch pair is safe -- each call refills it before use.
  static u32 subsz_s[ATP_DT_FLAT_CAP];
  static u32 flatsym_s[ATP_DT_FLAT_CAP];
  // One variable renumbering spans the whole synthetic Cp(o_lhs,o_rhs)
  // -- exactly as atp_fv_index_insert renumbers the stored CP.
  AtpDTreeVarMap vm;
  atp_dtree_varmap_reset(&vm);
  // Reserve position 0 for the synthetic `Cp` head: it spans the whole
  // subject, so subsz[0] = total positions.
  u32 pos = 1u;
  u8  ok  = atp_dt_flatten(o_lhs, subsz_s, flatsym_s, &vm, ATP_DT_FLAT_CAP, &pos)
         && atp_dt_flatten(o_rhs, subsz_s, flatsym_s, &vm, ATP_DT_FLAT_CAP, &pos);
  if (!ok) {
    // Cap hit -- fall back to a full scan so a deep CP can never be
    // silently under-retrieved (which would drop a real subsumer).
    for (u32 r = 0; r < ix->n_recs; r++) {
      if (!ix->recs[r].live) continue;
      ix->q_candidates++;
      RewriteSubst subst = {{0}};
      ix->q_matchcalls += 2u;
      if (acp_match_pair(ix->recs[r].packed, o_lhs, o_rhs, &subst)) {
        return 1;
      }
    }
    return 0;
  }
  flatsym_s[0] = ATP_DTREE_CTR_BASE + ATP_CP_LABEL;  // synthetic Cp head
  subsz_s[0]   = pos;                             // whole subject span
  g_atp_dt_ix           = ix;
  g_atp_dt_subsz        = subsz_s;
  g_atp_dt_flatsym      = flatsym_s;
  g_atp_dt_flatlen      = pos;
  g_atp_dt_qlhs         = o_lhs;
  g_atp_dt_qrhs         = o_rhs;
  g_atp_dt_query_folded = vm.folded;
  for (u32 i = 0; i < ATP_DTREE_MAXVARS; i++) g_atp_dt_star[i] = 0;
  // Descend from the root: its single real edge is the `Cp` head,
  // which lines up with flat[0] (also a Cp-head symbol).
  u32 cp_sym = ATP_DTREE_CTR_BASE + ATP_CP_LABEL;
  for (u32 c = ix->nodes[ix->root].child; c != ATP_DTREE_NIL;
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

// The unorientable-faces index inserts a face only when the direction
// can fire at all -- replacement-side variables contained in the
// matched side, OR the order is ground-total so the WM free-variable
// instance applies (see atp_unorient_template); declared here, defined
// with the ordered-rewrite helpers.
static int  atp_vars_contained(Term a, Term b);
static int  atp_unorient_dir_usable(AtpState *s, Term from, Term to);
static Term atp_unorient_template(AtpState *s, Term from, Term to);

// Preorder-flatten capacity for the indexed normalizer.  Sized to
// hold a raw (un-reduced) critical-pair side -- the deep overlap of
// two rules can run to tens of thousands of nodes before it is
// normalized down; an over-deep subject still works (the normalizer
// takes linear steps and re-flattens once rewriting shrinks it back
// under the cap) but pays the linear rate while it does.
#define ATP_RI_FLAT_CAP   65536u

// Flat symbol under RAW variable ids -- no first-appearance renumbering.
// The subject-side flatten uses this (rule-LHS inserts keep the
// first-appearance scheme above).  Raw ids make a flatsym position
// independent of the rest of the term, so an incremental re-flatten can
// SPLICE a rewritten subtree without re-deriving the whole string.  For
// a var-normalised subject (dense [0,k) ids, first appearance == id) it
// is bit-identical to atp_dtree_flatsym; the variable-consistency relation
// the descent's repeat-var memcmp depends on is preserved either way
// (both are consistent global encodings).  `*folded` is raised if a raw
// id crosses ATP_DTREE_MAXVARS (then the descent re-confirms via thvm_match).
static u32 atp_ri_flatsym_raw(Term t, u8 *folded) {
  switch (term_tag(t)) {
    case TAG_CTR: return ATP_DTREE_CTR_BASE + term_ext(t);
    case TAG_NUM: return ATP_DTREE_NUM;
    case TAG_FVR:
    default: {
      u32 id = term_ext(t);
      if (id >= ATP_DTREE_MAXVARS) { *folded = 1u; id = ATP_DTREE_MAXVARS - 1u; }
      return ATP_DTREE_STAR_BASE + id;
    }
  }
}

// One indexed rule: `rule` is the index into s->lhs[]/s->rhs[].
// `next` links the leaf list.  No Term mirror -- the index is rebuilt
// from s->lhs[] whenever R mutates, so it never outlives a GC move.
typedef struct {
  u32 rule;
  u32 next;
} AtpRiRec;

struct AtpRuleIndex {
  AtpDTreeNode *nodes;
  u32           n_nodes, cap_nodes;
  AtpRiRec     *recs;
  u32           n_recs, cap_recs;
  u32           root;
  u32           n_rules_built;  // R size the tree currently reflects
  // s->r_revision at build time.  n_rules alone cannot detect a
  // drop+add cycle that leaves the count unchanged while interreduce
  // compaction renumbers the surviving slots -- a leaf rec's `rule`
  // index then names the WRONG live rule and the retrieval silently
  // loses critical pairs (the GroupAxioms/InverseOfComposite +1-lemma
  // divergence vs Waldmeister).  A build is current iff BOTH n_rules
  // and r_revision match; it is extendable (pure append) iff the
  // revision delta equals the rule-count delta, since every push bumps
  // r_revision exactly once and every drop / soft-delete / orient-flip
  // bumps it without growing n_rules.
  u32           built_revision;
  // Retrieval instrumentation (cheap counters, always compiled).  The
  // decisive Sheffer measurement: q_candidates / q_queries is the
  // candidates-returned-per-query -- if it tracks n_rules the tree does
  // not prune; if bounded it does.
  u64           q_queries;      // atp_ri_query_pos calls
  u64           q_candidates;   // leaf records reached by retrieval
  u64           q_matchcalls;   // thvm_match calls issued on candidates
  u64           q_nodevisits;   // discrimination-tree nodes touched
  u64           q_depth_capped; // recursion-depth bails in atp_ri_descend /
                                // _unorient -- nonzero = latent DT-cycle
                                // bug worth investigating.
  u8            any_folded;     // some rule LHS folded a var -> imperfect
  // 1 for the unorientable-faces index: a leaf rec's `rule` field then
  // carries the direction in its high bit (ATP_RI_DIR_BIT) -- bit set =
  // r->l (matched face is rhs[i], replacement is lhs[i]); clear = l->r.
  u8            is_unorient;
};
// Direction bit packed into a leaf rec's `rule` field for the
// unorientable index.  Rule indices never approach 2^31 in completion,
// so the top bit is free as a per-face direction tag.
#define ATP_RI_DIR_BIT  0x80000000u
typedef struct AtpRuleIndex AtpRuleIndex;

// Specialize the tree-management template (atp_ri_node_new / _rec_new /
// _child / _insert_term) over AtpRuleIndex + AtpRiRec.
ATP_DTREE_DEFINE_OPS(ri, AtpRuleIndex, AtpRiRec)

static AtpRuleIndex *atp_ri_new(void) {
  AtpRuleIndex *ix = (AtpRuleIndex *)calloc(1, sizeof(AtpRuleIndex));
  if (ix == NULL) thvm_fatal("atp_ri: index OOM");
  ix->root = atp_ri_node_new(ix, ATP_DTREE_NIL);
  return ix;
}

static void atp_ri_free(AtpRuleIndex *ix) {
  if (ix == NULL) return;
  free(ix->nodes);
  free(ix->recs);
  free(ix);
}

// Discard every node / record and rebuild the tree from s->lhs[0..
// n_rules).  Rules are inserted in ascending index order -- the leaf
// list at a node is then index-descending (push-front), which the
// retrieval's min-tracking does not depend on but keeps deterministic.
// Incremental discrimination-tree extension: insert only the rules in
// [ix->n_rules_built, s->n_rules) -- the just-added tail.  Callable only
// when the existing tree is sound (no rule removed / killed since the
// last build).  Returns 1 on success, 0 if the index needs a full rebuild
// (n_rules_built > n_rules, indicating the rule set shrunk, or a special
// state the caller should treat as dirty).  Engine byte-identical: the
// inserted leaves carry the same rule indices as a full rebuild would.
static u8 atp_ri_extend(AtpState *s) {
  AtpRuleIndex *ix = s->rule_index;
  if (ix == NULL || ix->root == ATP_DTREE_NIL) return 0;
  if (ix->n_rules_built > s->n_rules) return 0;  // shrunk -- caller forces full
  // Pure-append witness: r_revision bumps exactly once per appended rule
  // and once more on every drop / orient-flip / bwd-subsume soft-delete.
  // The tail is a clean append iff the revision delta equals the rule-
  // count delta; any excess revision means an existing rule was revised
  // (e.g. a face soft-deleted), so the existing tree is stale and the
  // caller must do a full rebuild (return 0).
  if (s->r_revision - ix->built_revision != s->n_rules - ix->n_rules_built)
    return 0;
  if (ix->n_rules_built == s->n_rules) return 1; // up to date
  for (u32 i = ix->n_rules_built; i < s->n_rules; i++) {
    if (s->n_unorient > 0u && !s->r_orient[i]) continue;
    if (s->r_dead != NULL && s->r_dead[i]) continue;
    AtpDTreeVarMap vm;
    atp_dtree_varmap_reset(&vm);
    u32 node = atp_ri_insert_term(ix, ix->root, s->lhs[i], &vm);
    if (vm.folded) ix->any_folded = 1u;
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule = i;
    ix->recs[rec].next = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
  }
  ix->n_rules_built  = s->n_rules;
  ix->built_revision = s->r_revision;

  AtpRuleIndex *ux = s->unorient_index;
  if (ux != NULL && ux->root != ATP_DTREE_NIL && ux->n_rules_built <= s->n_rules) {
    if (s->n_unorient > 0u) {
      for (u32 i = ux->n_rules_built; i < s->n_rules; i++) {
        if (s->r_orient[i]) continue;
        if (s->r_dead != NULL && s->r_dead[i]) continue;
        if (atp_unorient_dir_usable(s, s->lhs[i], s->rhs[i])) {
          AtpDTreeVarMap vm; atp_dtree_varmap_reset(&vm);
          u32 node = atp_ri_insert_term(ux, ux->root, s->lhs[i], &vm);
          if (vm.folded) ux->any_folded = 1u;
          u32 rec  = atp_ri_rec_new(ux);
          ux->recs[rec].rule = i;
          ux->recs[rec].next = ux->nodes[node].rec_head;
          ux->nodes[node].rec_head = rec;
        }
        if (atp_unorient_dir_usable(s, s->rhs[i], s->lhs[i])) {
          AtpDTreeVarMap vm; atp_dtree_varmap_reset(&vm);
          u32 node = atp_ri_insert_term(ux, ux->root, s->rhs[i], &vm);
          if (vm.folded) ux->any_folded = 1u;
          u32 rec  = atp_ri_rec_new(ux);
          ux->recs[rec].rule = i | ATP_RI_DIR_BIT;
          ux->recs[rec].next = ux->nodes[node].rec_head;
          ux->nodes[node].rec_head = rec;
        }
      }
    }
    ux->n_rules_built  = s->n_rules;
    ux->built_revision = s->r_revision;
  }
  return 1;
}

static void atp_ri_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->rule_index;
  if (ix == NULL) return;
  // Incremental fast-path: when no rule has been killed / unorient flipped
  // since the last build (dirty bit clear at caller), and only new rules
  // were appended, just insert the tail.  Cuts O(R) rebuild to O(rules-
  // added).  Soundness: atp_ri_extend re-checks the pure-append witness
  // (revision delta == count delta) and returns 0 if any existing rule
  // was revised, so the full rebuild below runs whenever the tail is not
  // a clean append.  Skipped here when the rule set shrunk
  // (n_rules_built > n_rules) -- atp_ri_extend signals via return 0.
  if (ix->n_rules_built > 0u && ix->n_rules_built < s->n_rules
      && !s->rule_index_dirty) {
    if (atp_ri_extend(s)) return;
  }
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->any_folded = 0;
  ix->root    = atp_ri_node_new(ix, ATP_DTREE_NIL);
  for (u32 i = 0; i < s->n_rules; i++) {
    // When unorientable equations are present, index only the
    // orientable rules (always forward-decreasing, applied without an
    // order check); the few unorientable ones go to the linear
    // KBO-gated pass.  When n_unorient == 0 every rule is orientable, so
    // index all -- and r_orient[] may be a stale default for rules a
    // caller installed by hand, which is fine in that all-orientable
    // regime.
    if (s->n_unorient > 0u && !s->r_orient[i]) continue;
    if (s->r_dead != NULL && s->r_dead[i]) continue;  // bwd-subsumed: sentinel LHS
    AtpDTreeVarMap vm;
    atp_dtree_varmap_reset(&vm);
    u32 node = atp_ri_insert_term(ix, ix->root, s->lhs[i], &vm);
    if (vm.folded) ix->any_folded = 1u;
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule = i;
    ix->recs[rec].next = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
  }
  ix->n_rules_built  = s->n_rules;
  ix->built_revision = s->r_revision;

  // Companion index over the UNORIENTABLE equations' faces.  For each
  // unorientable rule i, the matched side may be either face; index a
  // face only when its direction can fire at all -- replacement-side
  // variables contained in the matched side, or the ground-total WM
  // free-variable instance applies (atp_unorient_dir_usable; the
  // linear scan's atp_unorient_template guard mirrors it).  The leaf
  // rec's `rule` field carries the direction in ATP_RI_DIR_BIT: clear =
  // l->r (match lhs[i], replace by rhs[i]); set = r->l (match rhs[i],
  // replace by lhs[i]).  Faces are inserted in (rule asc, l->r before
  // r->l) order so the retrieval's candidate list is built deterministic.
  AtpRuleIndex *ux = s->unorient_index;
  if (ux != NULL) {
    ux->n_nodes = 0;
    ux->n_recs  = 0;
    ux->any_folded = 0;
    ux->is_unorient = 1u;
    ux->root    = atp_ri_node_new(ux, ATP_DTREE_NIL);
    if (s->n_unorient > 0u) {
      for (u32 i = 0; i < s->n_rules; i++) {
        if (s->r_orient[i]) continue;                 // oriented: rule_index
        if (s->r_dead != NULL && s->r_dead[i]) continue;  // bwd-subsumed: sentinel face
        // l->r face: match lhs[i], replace by rhs[i].
        if (atp_unorient_dir_usable(s, s->lhs[i], s->rhs[i])) {
          AtpDTreeVarMap vm;
          atp_dtree_varmap_reset(&vm);
          u32 node = atp_ri_insert_term(ux, ux->root, s->lhs[i], &vm);
          if (vm.folded) ux->any_folded = 1u;
          u32 rec  = atp_ri_rec_new(ux);
          ux->recs[rec].rule = i;                     // dir bit clear
          ux->recs[rec].next = ux->nodes[node].rec_head;
          ux->nodes[node].rec_head = rec;
        }
        // r->l face: match rhs[i], replace by lhs[i].
        if (atp_unorient_dir_usable(s, s->rhs[i], s->lhs[i])) {
          AtpDTreeVarMap vm;
          atp_dtree_varmap_reset(&vm);
          u32 node = atp_ri_insert_term(ux, ux->root, s->rhs[i], &vm);
          if (vm.folded) ux->any_folded = 1u;
          u32 rec  = atp_ri_rec_new(ux);
          ux->recs[rec].rule = i | ATP_RI_DIR_BIT;
          ux->recs[rec].next = ux->nodes[node].rec_head;
          ux->nodes[node].rec_head = rec;
        }
      }
    }
    ux->n_rules_built  = s->n_rules;
    ux->built_revision = s->r_revision;
  }

  s->rule_index_dirty = 0u;
}

// --- retrieval -----------------------------------------------------

// Preorder-flatten `t` into `flat[]` from `*pos`, recording each
// position's subtree span in `subsz[]` and its raw flat-symbol in
// `flatsym[]`.  Returns 1 on success, 0 on cap.  Used both for the
// whole-subject flatten at the start of a normalize and -- with `*pos`
// set to a redex position -- to splice a rewritten subtree in place.
static u8 atp_ri_flatten(Term t, Term *flat, u32 *subsz, u32 *flatsym,
                         u8 *folded, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flat[here]    = t;
  flatsym[here] = atp_ri_flatsym_raw(t, folded);
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!atp_ri_flatten(term_ctr_at(t, i), flat, subsz, flatsym, folded,
                          cap, pos)) return 0;
    }
  }
  subsz[here] = *pos - here;
  return 1;
}

// One retrieval's immutable parameters (single-threaded saturation, so
// file-static query scratch is safe).  `g_atp_ri_flat`/`subsz` span
// the WHOLE subject of the current `atp_ri_rewrite_step`; a query at
// preorder position `p` walks the slice flat[p .. p+subsz[p]).
// `g_atp_ri_best` is the lowest rule index a reachable leaf has
// confirmed so far -- ATP_DTREE_NIL = none.
static AtpRuleIndex *g_atp_ri_ix      = NULL;
static const Term   *g_atp_ri_flat    = NULL;
static const u32    *g_atp_ri_subsz   = NULL;
static const u32    *g_atp_ri_flatsym = NULL;  // per-position flat-symbol code
static const Term   *g_atp_ri_lhs     = NULL;  // s->lhs[] for the leaf guard
static const Term   *g_atp_ri_rhs     = NULL;  // s->rhs[] for r->l face guards
static u32           g_atp_ri_qend    = 0;     // end of the queried slice
static u32           g_atp_ri_star[ATP_DTREE_MAXVARS];  // first-bind positions
static u32           g_atp_ri_best    = ATP_DTREE_NIL;
static u32           g_atp_ri_best_star[ATP_DTREE_MAXVARS]; // star[] at the win
static Term          g_atp_ri_qsubj   = 0;     // subject at the query position
static u8            g_atp_ri_query_folded = 0; // subject flatten folded a var

// At a leaf node: update g_atp_ri_best with the minimum rule index
// whose LHS genuinely one-way matches the query subject.  The
// perfect-tree descent proves structure + variable consistency for
// the var ids the tree distinguishes, but `atp_dtree_var_index` folds
// ids >= REWRITE_MAX_VAR onto one slot (coarser tree), so the leaf
// re-runs `thvm_match` -- the authoritative guard, exactly the test
// `rewrite_try_top`'s linear scan applies.
static void atp_ri_leaf_collect(u32 node) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  // When neither the rule LHSs nor this subject folded a variable (the
  // common case after thvm_normalize_vars), the flatsym descent has
  // already proved the match exactly -- CTR symbols matched exactly,
  // variable consistency by flatsym memcmp -- so reaching this leaf IS
  // the proof and thvm_match is skipped.  A fold makes the tree coarser
  // and the leaf re-runs thvm_match as the authoritative guard.
  u8 perfect = !ix->any_folded && !g_atp_ri_query_folded;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DTREE_NIL;
       r = ix->recs[r].next) {
    ix->q_candidates++;
    u32 rule = ix->recs[r].rule;
    if (rule >= g_atp_ri_best) continue;          // cannot lower the min
    if (perfect) {
      g_atp_ri_best = rule;
      // Snapshot the path's variable bindings: with a perfect descent
      // these ARE the match substitution, so atp_ri_rewrite_step reads
      // them off directly instead of re-running thvm_match.
      for (u32 k = 0; k < ATP_DTREE_MAXVARS; k++) {
        g_atp_ri_best_star[k] = g_atp_ri_star[k];
      }
      continue;
    }
    RewriteSubst subst = {{0}};
    ix->q_matchcalls++;
    if (thvm_match(g_atp_ri_lhs[rule], g_atp_ri_qsubj, &subst)) {
      g_atp_ri_best = rule;
    }
  }
}

// Walk the flattened subject slice from preorder index `pos` in
// lockstep with tree node `node`, threading per-path variable
// bindings in g_atp_ri_star.  Visits EVERY reachable leaf (does not
// stop early) so g_atp_ri_best ends at the global minimum matching
// rule index for this position.
//
// At most ONE child can match a CTR/NUM subject head (the children
// carry distinct symbols), so that branch is a tail continuation: it
// is followed by LOOPING -- advancing `node`/`pos` in place -- rather
// than recursing.  Only the STAR (rule-variable) branches, which can
// fan out, recurse.  For the typical CTR spine of a rule LHS the whole
// descent is then one loop with no call overhead.
static void atp_ri_descend_rec(u32 node, u32 pos, u32 depth);
static void atp_ri_descend(u32 node, u32 pos) {
  atp_ri_descend_rec(node, pos, 0u);
}
static void atp_ri_descend_rec(u32 node, u32 pos, u32 depth) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  if (depth >= ATP_DT_DESCENT_DEPTH_CAP) { ix->q_depth_capped++; return; }  // depth-cap safety (see atp_dt_descend)
  for (;;) {
    ix->q_nodevisits++;
    if (pos == g_atp_ri_qend) {
      atp_ri_leaf_collect(node);
      return;
    }
    if (pos > g_atp_ri_qend) return;           // overshoot safety
    u32 sz         = g_atp_ri_subsz[pos];
    u32 csym_exact = g_atp_ri_flatsym[pos];   // CTR_BASE+lab / NUM / STAR+idx
    if (sz == 0u) return;                      // would stall at same pos
    u32 ctr_next   = ATP_DTREE_NIL;              // the lone CTR-match child
    for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_DTREE_STAR_BASE && csym < ATP_DTREE_CTR_BASE) {
        // Stored rule variable: FIRST occurrence binds it to this
        // subterm's preorder position; a REPEAT applies only if the two
        // subterms' flatsym slices are byte-identical (one-way matching).
        u32 k = csym - ATP_DTREE_STAR_BASE;
        u32 bound = g_atp_ri_star[k];
        if (bound == ATP_DTREE_NIL) {
          g_atp_ri_star[k] = pos;
          atp_ri_descend_rec(c, pos + sz, depth + 1u);
          g_atp_ri_star[k] = ATP_DTREE_NIL;
        } else if (g_atp_ri_subsz[bound] == sz &&
                   memcmp(&g_atp_ri_flatsym[bound], &g_atp_ri_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          atp_ri_descend_rec(c, pos + sz, depth + 1u);
        }
      } else if (csym == csym_exact) {
        // Stored CTR/NUM equal to the subject's head: consume the head;
        // the subject's children are the next preorder positions.
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_DTREE_NIL) return;       // no CTR continuation: done
    node = ctr_next;                          // tail-continue without a call
    pos  = pos + 1u;
  }
}

// Retrieve the lowest rule index whose LHS one-way matches the subject
// subterm at preorder position `qpos` of the shared flat array.
// Returns ATP_DTREE_NIL if no rule LHS matches there.
static u32 atp_ri_query_pos(u32 qpos) {
  g_atp_ri_ix->q_queries++;
  g_atp_ri_qend  = qpos + g_atp_ri_subsz[qpos];
  g_atp_ri_qsubj = g_atp_ri_flat[qpos];
  g_atp_ri_best  = ATP_DTREE_NIL;
  // g_atp_ri_star is NOT reset here: atp_ri_descend pairs every
  // `star[k] = pos` with a `star[k] = NIL` on backtrack and never
  // returns early, so star[] is all-NIL on entry and exit of every
  // descent.  The one reset is done once per normalize.
  atp_ri_descend(g_atp_ri_ix->root, qpos);
  return g_atp_ri_best;
}

// --- unorientable-faces retrieval (candidate collection) -----------
//
// The orientable retrieval above tracks a single MINIMUM rule index: the
// linear scan it replaces always fires the lowest-index rule with no
// order check, so the structural match alone names the winner.  The
// unorientable pass cannot collapse to a min: at a position, the linear
// scan tries each unorientable equation (rule asc; l->r then r->l) and
// the FIRST direction that is strictly order-DECREASING for the matched
// instance wins -- the structurally-lowest face may be order-rejected.
// So this descent COLLECTS every structurally matching face (rule, dir)
// at the position; the caller applies the LPO gate in (rule asc, l->r
// before r->l) order and fires the first that passes -- identical to the
// linear scan, but the LPO compare runs only on candidates.
// Candidate buffer cap.  At a position, the matching unorientable faces
// are bounded by the count of indexed faces (<= 2 * n_unorient); a deep
// completion's unorientable subset runs to a few hundred, and a shallow
// face (e.g. a commutativity-style nand(x,y)=nand(y,x)) matches every
// nand-headed subterm, so size generously to keep the indexed path live
// (overflow only forces the exact linear fallback at that position --
// correct, just slow).
#define ATP_RI_MAXCAND 8192u
static u32 g_atp_ri_cand[ATP_RI_MAXCAND];   // rule | dir-bit, structural hits
static u32 g_atp_ri_ncand = 0;

// Leaf action for the unorientable index: append each face whose stored
// pattern one-way matches the query subject.  Same match proof as the
// orientable leaf (perfect descent OR thvm_match on a fold) -- only the
// stored side (lhs[i] for l->r, rhs[i] for r->l) is the match pattern.
static void atp_ri_leaf_collect_unorient(u32 node) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  u8 perfect = !ix->any_folded && !g_atp_ri_query_folded;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DTREE_NIL;
       r = ix->recs[r].next) {
    u32 packed = ix->recs[r].rule;
    if (g_atp_ri_ncand >= ATP_RI_MAXCAND) return;   // buffer full: caller bails
    if (perfect) {
      g_atp_ri_cand[g_atp_ri_ncand++] = packed;
      continue;
    }
    u32 rule = packed & ~ATP_RI_DIR_BIT;
    Term pat = (packed & ATP_RI_DIR_BIT) ? g_atp_ri_rhs[rule]
                                         : g_atp_ri_lhs[rule];
    RewriteSubst subst = {{0}};
    if (thvm_match(pat, g_atp_ri_qsubj, &subst)) {
      g_atp_ri_cand[g_atp_ri_ncand++] = packed;
    }
  }
}

// Candidate-collecting twin of atp_ri_descend.  Identical traversal,
// different leaf action (collect every face, not the min).  Kept
// separate so the hot orientable descent stays a tight single-callback
// loop with no per-node branch on index kind.
static void atp_ri_descend_unorient_rec(u32 node, u32 pos, u32 depth);
static void atp_ri_descend_unorient(u32 node, u32 pos) {
  atp_ri_descend_unorient_rec(node, pos, 0u);
}
static void atp_ri_descend_unorient_rec(u32 node, u32 pos, u32 depth) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  if (depth >= ATP_DT_DESCENT_DEPTH_CAP) { ix->q_depth_capped++; return; }  // depth-cap safety
  for (;;) {
    if (pos == g_atp_ri_qend) {
      atp_ri_leaf_collect_unorient(node);
      return;
    }
    if (pos > g_atp_ri_qend) return;           // overshoot safety
    u32 sz         = g_atp_ri_subsz[pos];
    u32 csym_exact = g_atp_ri_flatsym[pos];
    if (sz == 0u) return;                      // would stall at same pos
    u32 ctr_next   = ATP_DTREE_NIL;
    for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_DTREE_STAR_BASE && csym < ATP_DTREE_CTR_BASE) {
        u32 k = csym - ATP_DTREE_STAR_BASE;
        u32 bound = g_atp_ri_star[k];
        if (bound == ATP_DTREE_NIL) {
          g_atp_ri_star[k] = pos;
          atp_ri_descend_unorient_rec(c, pos + sz, depth + 1u);
          g_atp_ri_star[k] = ATP_DTREE_NIL;
        } else if (g_atp_ri_subsz[bound] == sz &&
                   memcmp(&g_atp_ri_flatsym[bound], &g_atp_ri_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          atp_ri_descend_unorient_rec(c, pos + sz, depth + 1u);
        }
      } else if (csym == csym_exact) {
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_DTREE_NIL) return;
    node = ctr_next;
    pos  = pos + 1u;
  }
}

// Collect into g_atp_ri_cand[] every unorientable face whose stored
// pattern one-way matches the subject subterm at preorder position
// `qpos`.  Returns the candidate count (g_atp_ri_ncand).  Order within
// the buffer is leaf-list / tree order -- NOT priority -- so the caller
// must apply the (rule asc, l->r before r->l) priority itself.
static u32 atp_ri_query_pos_unorient(u32 qpos) {
  g_atp_ri_qend  = qpos + g_atp_ri_subsz[qpos];
  g_atp_ri_qsubj = g_atp_ri_flat[qpos];
  g_atp_ri_ncand = 0;
  atp_ri_descend_unorient(g_atp_ri_ix->root, qpos);
  return g_atp_ri_ncand;
}

// --- all-rules retrieval (ground-join candidate collection) --------
//
// The GJ ground-join rewrite (gj_rewrite_step) applies every rule in its
// stored l->r direction gated by the MODEL order (gj_less_in), not the
// global KBO -- so, like the unorientable pass, it cannot collapse to a
// single min-index redex: a structurally-lower rule may be model-rejected
// while a higher one fires.  This descent collects every rule whose LHS
// structurally matches the subject at `qpos` into g_atp_ri_cand[]; the
// caller (gj_rewrite_step) applies the gj_less_in gate in ascending rule
// order and fires the first that passes -- identical to the linear scan
// it replaces, but the model compare runs only on structural candidates.
// Built over ALL rules (no orientable filter), so it lives in a SEPARATE
// index (s->gj_rule_index): s->rule_index drops unorientable equations
// when any are present, but the GJ rewrite must see every rule.  Records
// store a plain rule id (no ATP_RI_DIR_BIT -- only the stored l->r face).
static void atp_ri_leaf_collect_all(u32 node) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  u8 perfect = !ix->any_folded && !g_atp_ri_query_folded;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DTREE_NIL;
       r = ix->recs[r].next) {
    u32 rule = ix->recs[r].rule;
    if (g_atp_ri_ncand >= ATP_RI_MAXCAND) return;   // buffer full: caller bails
    if (perfect) {
      g_atp_ri_cand[g_atp_ri_ncand++] = rule;
      continue;
    }
    RewriteSubst subst = {{0}};
    if (thvm_match(g_atp_ri_lhs[rule], g_atp_ri_qsubj, &subst)) {
      g_atp_ri_cand[g_atp_ri_ncand++] = rule;
    }
  }
}

static void atp_ri_descend_all_rec(u32 node, u32 pos, u32 depth);
static void atp_ri_descend_all(u32 node, u32 pos) {
  atp_ri_descend_all_rec(node, pos, 0u);
}
static void atp_ri_descend_all_rec(u32 node, u32 pos, u32 depth) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  if (depth >= ATP_DT_DESCENT_DEPTH_CAP) { ix->q_depth_capped++; return; }
  for (;;) {
    if (pos == g_atp_ri_qend) { atp_ri_leaf_collect_all(node); return; }
    if (pos > g_atp_ri_qend) return;
    u32 sz         = g_atp_ri_subsz[pos];
    u32 csym_exact = g_atp_ri_flatsym[pos];
    if (sz == 0u) return;
    u32 ctr_next   = ATP_DTREE_NIL;
    for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_DTREE_STAR_BASE && csym < ATP_DTREE_CTR_BASE) {
        u32 k = csym - ATP_DTREE_STAR_BASE;
        u32 bound = g_atp_ri_star[k];
        if (bound == ATP_DTREE_NIL) {
          g_atp_ri_star[k] = pos;
          atp_ri_descend_all_rec(c, pos + sz, depth + 1u);
          g_atp_ri_star[k] = ATP_DTREE_NIL;
        } else if (g_atp_ri_subsz[bound] == sz &&
                   memcmp(&g_atp_ri_flatsym[bound], &g_atp_ri_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          atp_ri_descend_all_rec(c, pos + sz, depth + 1u);
        }
      } else if (csym == csym_exact) {
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_DTREE_NIL) return;
    node = ctr_next;
    pos  = pos + 1u;
  }
}

static u32 atp_ri_query_pos_all(u32 qpos) {
  g_atp_ri_qend  = qpos + g_atp_ri_subsz[qpos];
  g_atp_ri_qsubj = g_atp_ri_flat[qpos];
  g_atp_ri_ncand = 0;
  atp_ri_descend_all(g_atp_ri_ix->root, qpos);
  return g_atp_ri_ncand;
}

// Build/refresh the GJ all-rules index (s->gj_rule_index) over every
// live rule's stored LHS (l->r face), no orientable filter.  Full
// rebuild keyed on (n_rules, r_revision) -- the GJ check runs in bursts
// per saturation step, so a rebuild amortizes over the burst's CPs.
static void atp_ri_gj_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->gj_rule_index;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->any_folded = 0;
  ix->root    = atp_ri_node_new(ix, ATP_DTREE_NIL);
  for (u32 i = 0; i < s->n_rules; i++) {
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;     // malformed bare-var LHS
    if (s->r_dead != NULL && s->r_dead[i]) continue;  // bwd-subsumed: sentinel LHS
    AtpDTreeVarMap vm;
    atp_dtree_varmap_reset(&vm);
    u32 node = atp_ri_insert_term(ix, ix->root, s->lhs[i], &vm);
    if (vm.folded) ix->any_folded = 1u;
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule = i;
    ix->recs[rec].next = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
  }
  ix->n_rules_built  = s->n_rules;
  ix->built_revision = s->r_revision;
}

// Find the FIRST preorder position of the shared subject that is a
// redex (some rule LHS matches there).  Preorder = outermost-leftmost
// = exactly `thvm_rewrite_step`'s "try top, then children left-to-
// right" order.  `clean_before` is the previous step's rewrite
// position: every position strictly before it that is NOT one of its
// ancestors (q + subsz[q] <= clean_before) was unchanged by that
// rewrite and was already found non-redex, so it is skipped -- the
// search re-examines only the ancestors and the changed subtree.  This
// returns the identical redex a full scan would (incremental
// outermost normalisation, Waldmeister's ascend-from-last-redex).
static u8 atp_ri_find_redex(u32 flatlen, u32 clean_before,
                            u32 *redex_pos, u32 *redex_rule) {
  for (u32 p = 0; p < flatlen; p++) {
    if (p < clean_before && p + g_atp_ri_subsz[p] <= clean_before) {
      continue;                              // unchanged, known non-redex
    }
    // Leaf positions (FVR / NUM) can never match a rule LHS top (rule
    // LHSs are CTR-rooted; an FVR-rooted LHS would be a non-decreasing
    // x->t and orient would have rejected it).  Skip without paying
    // the discrim-tree descent.  O(1) flat lookup vs the descent's
    // STAR-edge walk that always returns NIL at the leaves.
    if (g_atp_ri_flatsym[p] < ATP_DTREE_CTR_BASE) continue;
    u32 m = atp_ri_query_pos(p);
    if (m != ATP_DTREE_NIL) {
      *redex_pos  = p;
      *redex_rule = m;
      return 1;
    }
  }
  return 0;
}

// Materialise a tree Term from the flat arrays at preorder position
// `pos`.  Called ONCE per normalize (at fixpoint) -- the per-step
// rewrites only SPLICE the flat arrays, never the tree.  `flatsym`
// distinguishes a CTR (>= CTR_BASE) from a leaf; `flat[pos]` carries
// each node's pre-splice Term, so a subtree no splice touched rebuilds
// child-for-child equal to its original cell and is returned as-is --
// untouched subtrees cost zero allocation, only the union of the
// rewrite paths gets fresh CTR blocks.
static Term atp_ri_build(const Term *flat, const u32 *subsz,
                         const u32 *flatsym, u32 pos) {
  if (flatsym[pos] < ATP_DTREE_CTR_BASE) return flat[pos];  // NUM / variable leaf
  Term src = flat[pos];
  u32  n   = term_ctr_n(src);
  Term children[REWRITE_MAX_ARITY];
  u8   changed = 0u;
  u32  c = pos + 1u;
  for (u32 i = 0; i < n; i++) {
    children[i] = atp_ri_build(flat, subsz, flatsym, c);
    if (children[i] != term_ctr_at(src, i)) changed = 1u;
    c += subsz[c];
  }
  if (!changed) return src;                       // subtree untouched -- share
  return term_new_ctr(term_ext(src), children, n);
}

// Splice scratch: `repl` is flattened here first so its preorder
// length is the advanced cursor -- no separate atp_symbol_count walk.
static Term g_atp_ri_repl_flat   [ATP_RI_FLAT_CAP];
static u32  g_atp_ri_repl_subsz  [ATP_RI_FLAT_CAP];
static u32  g_atp_ri_repl_flatsym[ATP_RI_FLAT_CAP];

// Per-rule RHS flatterm cache.  Filled lazily when atp_subst_apply_to_flat
// is first called for a rule in the current epoch.  Invalidated when
// rule_index_dirty fires (R changed -> cached encodings may be stale).
// Stores rule_rhs's preorder flatterm as (cached_flat, cached_subsz,
// cached_flatsym, cached_len) so subsequent calls iterate the cached
// arrays linearly (no Term-tree recursion + heap reads).  Memory is
// O(sum |rhs|) -- small relative to the rule set.
#define ATP_RHS_FLAT_CAP 256u
typedef struct {
  u32  epoch;          // 0 = uncached; matches g_atp_unf_memo_epoch when valid
  u32  len;
  Term flat[ATP_RHS_FLAT_CAP];
  u32  subsz[ATP_RHS_FLAT_CAP];
  u32  flatsym[ATP_RHS_FLAT_CAP];
  u8   folded;
} AtpRhsFlatCacheEnt;
#define ATP_RHS_FLAT_CACHE_BITS 12
#define ATP_RHS_FLAT_CACHE_SIZE (1u << ATP_RHS_FLAT_CACHE_BITS)
#define ATP_RHS_FLAT_CACHE_MASK (ATP_RHS_FLAT_CACHE_SIZE - 1u)
static AtpRhsFlatCacheEnt g_atp_rhs_flat_cache[ATP_RHS_FLAT_CACHE_SIZE];
u64 g_atp_rhs_cache_hits   = 0;
u64 g_atp_rhs_cache_misses = 0;

// Emit the substitution-applied rule rhs directly into `dst_flat` /
// `dst_subsz` / `dst_flatsym` in flatterm form, WITHOUT round-tripping
// through a Term tree.  Mirrors the structure of WM's MA/SubstApply on
// TermzellenT cells: walk rule_rhs preorder, emit CTR cells, inline
// subject-flatterm slices for each var binding (star[var_id] = subject
// preorder position).  Allocation-free -- pure memcpy from
// subject_flat / subject_subsz / subject_flatsym into dst arrays.
// Returns 1 on success; 0 on cap overrun OR on a var binding that
// doesn't fit ATP_DTREE_MAXVARS (caller falls back to the Term-tree path).
// Look up (or fill) the cached flat encoding of `rule_rhs`.  Returns
// the entry's slot; caller checks `entry->epoch == g_atp_unf_memo_epoch`
// and uses (flat, subsz, flatsym, len) directly.  On cache miss,
// flattens rule_rhs once via atp_ri_flatten and stores it.  Returns
// NULL on encoding overrun (caller falls back to the recursive path).
static AtpRhsFlatCacheEnt *atp_rhs_flat_cache_get(Term rule_rhs) {
  u64 h = (u64)rule_rhs * 0x9E3779B97F4A7C15ull;
  h ^= h >> 29;
  u32 idx = (u32)h & ATP_RHS_FLAT_CACHE_MASK;
  AtpRhsFlatCacheEnt *e = &g_atp_rhs_flat_cache[idx];
  if (e->epoch == g_atp_unf_memo_epoch &&
      e->len > 0u && e->flat[0] == rule_rhs) {
    g_atp_rhs_cache_hits++;
    return e;
  }
  // Miss: re-encode.  Use atp_ri_flatten into the slot's own buffers
  // (Term, subsz, flatsym).
  u32 len = 0u;
  u8 folded_local = 0u;
  if (!atp_ri_flatten(rule_rhs, e->flat, e->subsz, e->flatsym,
                      &folded_local, ATP_RHS_FLAT_CAP, &len)) {
    e->epoch = 0u;
    return NULL;
  }
  e->epoch  = g_atp_unf_memo_epoch;
  e->len    = len;
  e->folded = folded_local;
  g_atp_rhs_cache_misses++;
  return e;
}

// Cached variant: iterate the pre-encoded rule_rhs flat linearly and
// emit cells to dst, replacing FVR cells with the bound subject slice
// (via star[var_id]).  Skips the recursive Term-tree walk +
// per-cell heap_reads that the original atp_subst_apply_to_flat
// pays on every call.  Returns 1 on success; 0 on cap overrun OR
// out-of-range var binding (caller falls back).
static u8 atp_subst_apply_to_flat_cached(Term rule_rhs,
                                         const u32 *star,
                                         const Term *subj_flat,
                                         const u32 *subj_subsz,
                                         const u32 *subj_flatsym,
                                         Term *dst_flat,
                                         u32 *dst_subsz,
                                         u32 *dst_flatsym,
                                         u32 *dst_pos,
                                         u8 *folded,
                                         u32 cap) {
  AtpRhsFlatCacheEnt *e = atp_rhs_flat_cache_get(rule_rhs);
  if (e == NULL) return 0;
  if (e->folded) return 0;                       // folded rhs: bail to safe path
  // Two-stack walk: linear pass over the cached rhs flat.  Maintain a
  // parallel `here` array for ancestor positions so each cell's subsz
  // (a function of dst positions) can be computed in one pass.
  u32 stack_here[ATP_RHS_FLAT_CAP];
  u32 stack_end [ATP_RHS_FLAT_CAP];
  u32 sp = 0u;
  for (u32 i = 0; i < e->len; i++) {
    while (sp > 0u && i >= stack_end[sp - 1u]) {
      u32 anc_here = stack_here[sp - 1u];
      dst_subsz[anc_here] = *dst_pos - anc_here;
      sp--;
    }
    u32 cell_sym = e->flatsym[i];
    if (cell_sym >= ATP_DTREE_CTR_BASE) {
      if (*dst_pos >= cap) return 0;
      dst_flat   [*dst_pos] = e->flat[i];
      dst_flatsym[*dst_pos] = cell_sym;
      stack_here[sp] = *dst_pos;
      stack_end [sp] = i + e->subsz[i];
      sp++;
      *dst_pos = *dst_pos + 1u;
    } else if (cell_sym >= ATP_DTREE_STAR_BASE) {
      u32 vid = cell_sym - ATP_DTREE_STAR_BASE;
      if (vid >= ATP_DTREE_MAXVARS) return 0;
      u32 spos = star[vid];
      if (spos == ATP_DTREE_NIL) return 0;
      u32 ssz = subj_subsz[spos];
      if (*dst_pos + ssz > cap) return 0;
      memcpy(&dst_flat   [*dst_pos], &subj_flat   [spos],
             (size_t)ssz * sizeof(Term));
      memcpy(&dst_subsz  [*dst_pos], &subj_subsz  [spos],
             (size_t)ssz * sizeof(u32));
      memcpy(&dst_flatsym[*dst_pos], &subj_flatsym[spos],
             (size_t)ssz * sizeof(u32));
      *dst_pos = *dst_pos + ssz;
    } else {
      // NUM / other leaf -- emit as-is.
      if (*dst_pos >= cap) return 0;
      dst_flat   [*dst_pos] = e->flat[i];
      dst_flatsym[*dst_pos] = cell_sym;
      dst_subsz  [*dst_pos] = 1u;
      *dst_pos = *dst_pos + 1u;
    }
  }
  while (sp > 0u) {
    u32 anc_here = stack_here[sp - 1u];
    dst_subsz[anc_here] = *dst_pos - anc_here;
    sp--;
  }
  (void)folded;   // cached path doesn't set caller's folded; safe since
                  // we already bailed when e->folded was 1.
  return 1;
}

static u8 atp_subst_apply_to_flat(Term rule_rhs,
                                  const u32 *star,
                                  const Term *subj_flat,
                                  const u32 *subj_subsz,
                                  const u32 *subj_flatsym,
                                  Term *dst_flat,
                                  u32 *dst_subsz,
                                  u32 *dst_flatsym,
                                  u32 *dst_pos,
                                  u8 *folded,
                                  u32 cap) {
  u32 here = *dst_pos;
  if (here >= cap) return 0;
  switch (term_tag(rule_rhs)) {
    case TAG_FVR: {
      u32 vid = term_ext(rule_rhs);
      if (vid >= ATP_DTREE_MAXVARS) return 0;     // folded: fall back to tree path
      u32 spos = star[vid];
      if (spos == ATP_DTREE_NIL) return 0;        // unbound: fall back
      u32 ssz = subj_subsz[spos];
      if (here + ssz > cap) return 0;
      memcpy(&dst_flat[here],    &subj_flat[spos],
             (size_t)ssz * sizeof(Term));
      memcpy(&dst_subsz[here],   &subj_subsz[spos],
             (size_t)ssz * sizeof(u32));
      memcpy(&dst_flatsym[here], &subj_flatsym[spos],
             (size_t)ssz * sizeof(u32));
      *dst_pos = here + ssz;
      return 1;
    }
    case TAG_CTR: {
      dst_flat[here]    = rule_rhs;
      dst_flatsym[here] = atp_ri_flatsym_raw(rule_rhs, folded);
      *dst_pos = here + 1u;
      u64 base = term_val(rule_rhs);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) {
        dst_subsz[here] = 1u;
        return 1;
      }
      u32 n = (u32)term_val(n_cell);
      for (u32 i = 0; i < n; i++) {
        Term child = heap_read(base + 1u + (u64)i);
        if (!atp_subst_apply_to_flat(child, star, subj_flat, subj_subsz,
                                     subj_flatsym, dst_flat, dst_subsz,
                                     dst_flatsym, dst_pos, folded, cap)) {
          return 0;
        }
      }
      dst_subsz[here] = *dst_pos - here;
      return 1;
    }
    default: {
      dst_flat[here]    = rule_rhs;
      dst_flatsym[here] = atp_ri_flatsym_raw(rule_rhs, folded);
      dst_subsz[here]   = 1u;
      *dst_pos = here + 1u;
      return 1;
    }
  }
}

// Splice a rewrite into the persistent flat arrays.  A normalize step
// rewrote preorder position `redex_pos` (old subtree span
// `subsz[redex_pos]`) into `repl`.  Rather than re-flatten the whole
// new term, replace the redex region in place: shift the tail, write
// `repl`'s flattening at `redex_pos`, and fan the size delta into the
// ancestors' subtree spans.  Raw-id flat symbols make every untouched
// position's flatsym splice-stable, so this is exact.  Returns 1 on
// success, 0 if the spliced length would overrun `cap` (caller then
// re-flattens from scratch).
static u8 atp_ri_splice(Term *flat, u32 *subsz, u32 *flatsym, u32 *flatlen,
                        u8 *folded, u32 redex_pos, Term repl, u32 cap) {
  u32 oldsz = subsz[redex_pos];
  // Flatten repl into the scratch arrays: its preorder length `rlen`
  // is the cursor the flatten advances, so the old separate
  // atp_symbol_count(repl) walk (one per rewrite step) is gone.
  u32 rlen  = 0u;
  if (!atp_ri_flatten(repl, g_atp_ri_repl_flat, g_atp_ri_repl_subsz,
                      g_atp_ri_repl_flatsym, folded, cap, &rlen)) {
    return 0;                                    // repl alone overruns
  }
  u32 tail  = *flatlen - redex_pos - oldsz;      // positions after the redex
  if (redex_pos + rlen + tail > cap) return 0;   // would overrun
  // Fan the size delta into every ancestor of redex_pos.  Their flatsym
  // is unchanged (the rewrite replaces a subtree, not an ancestor head)
  // but their span grows/shrinks by rlen-oldsz.  Walk the path with the
  // OLD spans; modular u32 arithmetic carries a negative delta exactly.
  u32 a = 0u;
  while (a != redex_pos) {
    u32 c = a + 1u;                              // first child position
    while (c + subsz[c] <= redex_pos) c += subsz[c];
    subsz[a] = subsz[a] + rlen - oldsz;
    a = c;
  }
  // Shift the tail [redex_pos+oldsz, flatlen) to [redex_pos+rlen, ...).
  if (tail > 0u && rlen != oldsz) {
    memmove(&flat[redex_pos + rlen],    &flat[redex_pos + oldsz],
            (size_t)tail * sizeof(Term));
    memmove(&subsz[redex_pos + rlen],   &subsz[redex_pos + oldsz],
            (size_t)tail * sizeof(u32));
    memmove(&flatsym[redex_pos + rlen], &flatsym[redex_pos + oldsz],
            (size_t)tail * sizeof(u32));
  }
  // Place the pre-flattened repl into the freed [redex_pos,
  // redex_pos+rlen) region.  flat / subsz / flatsym entries are
  // position-independent (Term cells, self-relative spans, raw
  // symbols), so a contiguous copy is exact -- and far cheaper than
  // re-walking repl's tree.
  memcpy(&flat[redex_pos],    g_atp_ri_repl_flat,
         (size_t)rlen * sizeof(Term));
  memcpy(&subsz[redex_pos],   g_atp_ri_repl_subsz,
         (size_t)rlen * sizeof(u32));
  memcpy(&flatsym[redex_pos], g_atp_ri_repl_flatsym,
         (size_t)rlen * sizeof(u32));
  *flatlen = redex_pos + rlen + tail;
  return 1;
}

// Flatterm-native splice: same as atp_ri_splice but produces the
// rewrite's replacement directly in the scratch flat arrays via
// atp_subst_apply_to_flat (no Term-tree round-trip).  Caller passes
// rule_rhs (the rule's RHS Term, traversed as a template) + star (the
// var bindings as subject preorder positions).  Returns 1 on success;
// 0 on cap overrun OR fold-incompatible binding -- caller falls back
// to the Term-tree atp_ri_splice path.
static u8 atp_ri_splice_inline(Term *flat, u32 *subsz, u32 *flatsym,
                               u32 *flatlen, u8 *folded, u32 redex_pos,
                               Term rule_rhs, const u32 *star, u32 cap) {
  u32 oldsz = subsz[redex_pos];
  u32 rlen = 0u;
  // Try the cache-driven linear path first (skips per-cell Term-tree
  // recursion + heap_reads).  Falls back to the recursive walker on
  // cache overrun / folded rhs.
  if (!atp_subst_apply_to_flat_cached(rule_rhs, star, flat, subsz, flatsym,
                                      g_atp_ri_repl_flat, g_atp_ri_repl_subsz,
                                      g_atp_ri_repl_flatsym, &rlen,
                                      folded, cap)) {
    rlen = 0u;
    if (!atp_subst_apply_to_flat(rule_rhs, star, flat, subsz, flatsym,
                                 g_atp_ri_repl_flat, g_atp_ri_repl_subsz,
                                 g_atp_ri_repl_flatsym, &rlen, folded, cap)) {
      return 0;
    }
  }
  u32 tail = *flatlen - redex_pos - oldsz;
  if (redex_pos + rlen + tail > cap) return 0;
  u32 a = 0u;
  while (a != redex_pos) {
    u32 c = a + 1u;
    while (c + subsz[c] <= redex_pos) c += subsz[c];
    subsz[a] = subsz[a] + rlen - oldsz;
    a = c;
  }
  if (tail > 0u && rlen != oldsz) {
    memmove(&flat[redex_pos + rlen],    &flat[redex_pos + oldsz],
            (size_t)tail * sizeof(Term));
    memmove(&subsz[redex_pos + rlen],   &subsz[redex_pos + oldsz],
            (size_t)tail * sizeof(u32));
    memmove(&flatsym[redex_pos + rlen], &flatsym[redex_pos + oldsz],
            (size_t)tail * sizeof(u32));
  }
  memcpy(&flat[redex_pos],    g_atp_ri_repl_flat,
         (size_t)rlen * sizeof(Term));
  memcpy(&subsz[redex_pos],   g_atp_ri_repl_subsz,
         (size_t)rlen * sizeof(u32));
  memcpy(&flatsym[redex_pos], g_atp_ri_repl_flatsym,
         (size_t)rlen * sizeof(u32));
  *flatlen = redex_pos + rlen + tail;
  return 1;
}

// Indexed analog of `thvm_rewrite_normalize`: rewrite `t` to fixpoint
// (or step_cap) via the rule-LHS discrimination index.  The subject is
// flattened ONCE; each step SPLICES the rewrite into the persistent
// flat arrays (atp_ri_splice) -- no per-step tree rebuild, no
// re-flatten.  The tree Term is materialised ONCE, at fixpoint, by
// atp_ri_build.  A normalize is thus O(subject + sum repl + final tree)
// rather than O(steps * subject).  The incremental redex search resumes
// from the last redex (atp_ri_find_redex's `clean_before`).  Behavior-
// identical to the linear ordered scan: same outermost-leftmost redex,
// same lowest-index rule, same substitution.
static Term atp_rewrite_normalize_indexed(AtpState *s, Term t, u32 step_cap) {
  if (s->rule_index == NULL) s->rule_index = atp_ri_new();
  if (s->rule_index_dirty || s->rule_index->n_rules_built != s->n_rules
      || s->rule_index->built_revision != s->r_revision) {
    atp_ri_rebuild(s);
    // R changed -> always bump norm-cache/rhs/join epoch.  Step epoch
    // is bumped only when r_revision moved since the last invalidate;
    // a stable revision witnesses a bytewise-unchanged rule set.
    atp_unf_memo_invalidate(s->r_revision, 0u);
  }
  static Term flat[ATP_RI_FLAT_CAP];
  static u32  subsz[ATP_RI_FLAT_CAP];
  static u32  flatsym[ATP_RI_FLAT_CAP];
  g_atp_ri_ix      = s->rule_index;
  g_atp_ri_flat    = flat;
  g_atp_ri_subsz   = subsz;
  g_atp_ri_flatsym = flatsym;
  g_atp_ri_lhs     = s->lhs;
  // Reset the descent's variable-binding array ONCE per normalize.  Every
  // atp_ri_descend leaves it all-NIL (each bind is unwound on backtrack),
  // so the per-query reset that used to live in atp_ri_query_pos was
  // redundant -- a 64-store loop on every one of millions of queries.
  for (u32 k = 0; k < ATP_DTREE_MAXVARS; k++) g_atp_ri_star[k] = ATP_DTREE_NIL;

  u32 flatlen = 0u;
  u8  folded  = 0u;
  // Flatten the subject into the persistent arrays.  An over-deep term
  // (> ATP_RI_FLAT_CAP nodes -- a raw critical-pair side can be) cannot
  // be flattened: the loop then takes a single linear rewrite step and
  // RE-FLATTENS, so the fast splice path resumes the instant rewriting
  // shrinks the term back under the cap (a whole-normalize linear
  // fallback would never resume).
  u8  flattened   = atp_ri_flatten(t, flat, subsz, flatsym, &folded,
                                   ATP_RI_FLAT_CAP, &flatlen);
  u32 prev_redex  = 0u;                 // 0 on the first step -> full scan
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    if (!flattened) {
      // Over-deep: one linear rewrite, then retry the flatten.
      { static int dl = -1; static u32 mx = 0u;
        if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
        if (dl) { u32 sz = atp_symbol_count(t);
          if (sz > mx) { mx = sz;
            fprintf(stderr, "NORMLOG linear t_size=%u step=%u\n", sz, i); } } }
      Term t2 = thvm_rewrite_step(t, s->lhs, s->rhs, s->n_rules);
      if (kbo_eq(t, t2)) return t;       // fixpoint
      t = t2;
      flatlen = 0u; folded = 0u;
      flattened = atp_ri_flatten(t, flat, subsz, flatsym, &folded,
                                 ATP_RI_FLAT_CAP, &flatlen);
      prev_redex = 0u;
      continue;
    }
    g_atp_ri_query_folded = folded;
    u32 redex_pos = 0u, redex_rule = 0u;
    if (!atp_ri_find_redex(flatlen, prev_redex, &redex_pos, &redex_rule)) {
      return atp_ri_build(flat, subsz, flatsym, 0u);   // no redex: fixpoint
    }
    // Build the chosen rule's substitution.  A perfect descent already
    // bound every rule variable -- g_atp_ri_best_star[k] is the preorder
    // position of the subterm bound to rule variable k -- so read it
    // straight off flat[].  thvm_match runs only when folding made the
    // descent inexact.  Rule LHS vars are dense [0,k) after
    // thvm_normalize_vars, so the star index IS the variable id.
    RewriteSubst subst = {{0}};
    if (!g_atp_ri_ix->any_folded && !folded) {
      for (u32 k = 0; k < ATP_DTREE_MAXVARS; k++) {
        if (g_atp_ri_best_star[k] != ATP_DTREE_NIL) {
          // See atp_ft_indexed_fixpoint: a star at an INTERIOR position has
          // a STALE cached flat[] cell after an in-place splice rewrote a
          // descendant, so rebuild the bound subtree rather than read the
          // stale cell.  Untouched subtrees rebuild to their shared
          // original at zero allocation.
          subst.bindings[k] =
              atp_ri_build(flat, subsz, flatsym, g_atp_ri_best_star[k]);
        }
      }
    } else if (!thvm_match(s->lhs[redex_rule],
                           atp_ri_build(flat, subsz, flatsym, redex_pos),
                           &subst)) {
      return atp_ri_build(flat, subsz, flatsym, 0u);   // unreachable: confirmed
    }
    Term repl = thvm_subst_apply(s->rhs[redex_rule], &subst);
    if (atp_ri_splice(flat, subsz, flatsym, &flatlen, &folded,
                      redex_pos, repl, ATP_RI_FLAT_CAP)) {
      prev_redex = redex_pos;
    } else {
      // The rewrite would grow the term past the cap.  Materialise the
      // current (pre-rewrite) tree -- atp_ri_splice left the flat arrays
      // untouched on overrun -- and drop to the linear branch, which
      // re-finds and applies this same redex, then re-flattens once the
      // term shrinks back under the cap.
      t = atp_ri_build(flat, subsz, flatsym, 0u);
      flattened = 0u;
    }
  }
  return flattened ? atp_ri_build(flat, subsz, flatsym, 0u) : t;
}

// === Stage 6b: AtpFt-native discrim-tree descent + ft_norm wiring =====
//
// Both files reuse the AtpRuleIndex / ATP_RI_* / atp_ri_rebuild
// definitions above; they are included here so those references
// resolve.  ft_ri.c is gated on THVM_ATPFT_RI; ft_norm.c is gated on
// THVM_ATPFT_NORM (and conditionally pulls in ft_ri.c's externs).
//
// ft_norm.c needs `atp_compare` (reduction-order KBO compare) for the
// unorientable-equation gate in find_redex_ft.  atp_compare is defined
// further down this TU; forward-declare it here so the include sees a
// valid prototype.
#ifdef THVM_ATPFT_NORM
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs);
# ifdef THVM_ATPFT_RI
#  include "ft_ri.c"
# endif
# include "ft_norm.c"
#endif

// Stage 7: AtpFt-native CP queue (dual-store).  Requires Stages 4 + 6
// (rule mirror + ft-norm fixpoint).  Off the flag this header is gone
// and the cp_packed_ft field is absent from AtpState, so the engine
// is byte-identical to the legacy packed-byte queue.
#ifdef THVM_ATPFT_CPQ
# if !defined(THVM_ATPFT_RULES) || !defined(THVM_ATPFT_NORM)
#  error "THVM_ATPFT_CPQ requires THVM_ATPFT_RULES and THVM_ATPFT_NORM"
# endif
# include "ft_cpq.c"
#endif

// === Faithful Waldmeister-FPA normalize path (gated, default OFF) =====
//
// Wires src/wmfpa/wmfpa.h (flatterm rep + DSBaum discrimination tree +
// NormalformInnermost retrieval) into the orientable normalize fixpoint
// behind s->use_wmfpa.  The subject is encoded to a flatterm once, the
// redex-retrieving tree is built from s->lhs[] (orientable rules) and
// cached on AtpState, and the normal form is decoded back.  Byte-
// identical to atp_rewrite_normalize_indexed (asserted by the bench
// differential).  Used only when EVERY rule is orientable (n_unorient
// == 0), exactly like the indexed path.
#include "../wmfpa/wmfpa.h"

static u32 atp_pretty_term(Term t, char *buf, u32 cap);   // defined in 5.x

// IC Term -> wmfpa flatterm boundary adapters.
static int wf_eng_is_var(WfTermH t, u32 *id) {
  if (term_tag((Term)t) == TAG_FVR) { *id = term_ext((Term)t); return 1; }
  return 0;
}
static u32     wf_eng_ctr_label(WfTermH t) { return term_ext((Term)t); }
static u32     wf_eng_ctr_arity(WfTermH t) { return term_ctr_n((Term)t); }
static WfTermH wf_eng_ctr_child(WfTermH t, u32 i) {
  return (WfTermH)term_ctr_at((Term)t, i);
}
static const WfEnc g_wf_eng_enc = {
  wf_eng_is_var, wf_eng_ctr_label, wf_eng_ctr_arity, wf_eng_ctr_child
};

// Decode a flatterm subtree rooted at `p` back to an IC Term.
static Term wf_eng_decode_rec(const WfNode *a, u32 *p) {
  u32 self = (*p)++;
  if (WF_IS_VAR(a[self].sym)) return term_new_fvr(WF_VAR_ID(a[self].sym));
  u32 n = a[self].arity;
  if (n == 0u) { Term none[1]; return term_new_ctr(a[self].sym, none, 0); }
  Term kids[REWRITE_MAX_ARITY];
  for (u32 i = 0; i < n; i++) kids[i] = wf_eng_decode_rec(a, p);
  return term_new_ctr(a[self].sym, kids, n);
}

// Per-rule flat-buffer storage for the cached tree.  Rebuilt whenever R
// changes; bounded by the live orientable rule count.
typedef struct {
  WfTree   tree;
  WfNode  *lhs_buf;    // packed flat LHS arenas
  WfNode  *rhs_buf;    // packed flat RHS arenas
  WfRule  *rules;
  u32      cap_rules;
  u32      cap_buf;    // node capacity of each of lhs_buf / rhs_buf
} WfEngCache;

#define WF_ENG_RULE_NODES 1024u    // per-side flat node cap (rule sides
                                   // are small; a Sheffer LHS is < 64)

static void wf_eng_cache_rebuild(AtpState *s) {
  WfEngCache *wc = (WfEngCache *)s->wmfpa_tree;
  if (wc == NULL) {
    wc = (WfEngCache *)calloc(1, sizeof(WfEngCache));
    s->wmfpa_tree = wc;
  } else {
    wf_tree_free(&wc->tree);
  }
  u32 nr = s->n_rules;
  if (nr > wc->cap_rules) {
    wc->cap_rules = nr;
    wc->rules   = (WfRule *)realloc(wc->rules, nr * sizeof(WfRule));
    wc->lhs_buf = (WfNode *)realloc(wc->lhs_buf,
                                    (size_t)nr * WF_ENG_RULE_NODES * sizeof(WfNode));
    wc->rhs_buf = (WfNode *)realloc(wc->rhs_buf,
                                    (size_t)nr * WF_ENG_RULE_NODES * sizeof(WfNode));
    wc->cap_buf = WF_ENG_RULE_NODES;
  }
  // Include EVERY rule in [0, n_rules), exactly as the indexed path
  // (atp_ri_rebuild) does: this path engages only when n_unorient == 0,
  // so the caller already guarantees R is orientable.  Filtering on
  // r_orient[] here would silently drop rules a caller set directly
  // (tests set s->lhs[i] + s->n_rules without r_orient[]), giving a
  // wrong -- too-large -- normal form.  The WfRule array index IS the
  // rule index (the tree's lowest-index-rule policy depends on it), so
  // keep a 1:1 mapping: an over-deep rule side gets llen 0 and simply
  // never inserts (absent from the tree), the same as the indexed path
  // which cannot flatten it either.
  for (u32 r = 0; r < nr; r++) {
    WfNode *lb = wc->lhs_buf + (size_t)r * WF_ENG_RULE_NODES;
    WfNode *rb = wc->rhs_buf + (size_t)r * WF_ENG_RULE_NODES;
    u32 ll = wf_encode((WfTermH)s->lhs[r], lb, WF_ENG_RULE_NODES, &g_wf_eng_enc);
    u32 rl = wf_encode((WfTermH)s->rhs[r], rb, WF_ENG_RULE_NODES, &g_wf_eng_enc);
    if (rl == 0u) ll = 0u;                  // over-deep RHS: drop the rule
    wc->rules[r].lhs = lb; wc->rules[r].llen = ll;
    wc->rules[r].rhs = rb; wc->rules[r].rlen = rl;
  }
  wf_tree_init(&wc->tree);
  wf_tree_build(&wc->tree, wc->rules, nr);
  s->wmfpa_built = s->n_rules;
}

// Incrementally bring the cached tree from `wmfpa_built` rules up to the
// current `n_rules` by encoding and inserting ONLY the appended rules
// [wmfpa_built, n_rules) -- O(sum of new-rule sizes), not O(R).  This is
// the dominant completion mutation (atp_push_rule appends one rule per
// step), so an O(rule) insert here is exactly what lets the per-op
// normalize win flow through instead of being swamped by O(R) rebuilds.
// Mirrors a loop of BO_ObjektEinfuegen over the new objects.  Returns 0
// (signalling "fall back to a full rebuild") when the append would exceed
// the cached buffer capacity -- the WfRule pointers index into lhs_buf/
// rhs_buf, so a realloc there would dangle every existing rule pointer;
// the full rebuild re-points them all.
static int wf_eng_cache_append(AtpState *s) {
  WfEngCache *wc = (WfEngCache *)s->wmfpa_tree;
  u32 nr = s->n_rules;
  if (nr > wc->cap_rules || wc->cap_buf < WF_ENG_RULE_NODES) return 0;
  for (u32 r = s->wmfpa_built; r < nr; r++) {
    WfNode *lb = wc->lhs_buf + (size_t)r * WF_ENG_RULE_NODES;
    WfNode *rb = wc->rhs_buf + (size_t)r * WF_ENG_RULE_NODES;
    u32 ll = wf_encode((WfTermH)s->lhs[r], lb, WF_ENG_RULE_NODES, &g_wf_eng_enc);
    u32 rl = wf_encode((WfTermH)s->rhs[r], rb, WF_ENG_RULE_NODES, &g_wf_eng_enc);
    if (rl == 0u) ll = 0u;                  // over-deep RHS: drop the rule
    wc->rules[r].lhs = lb; wc->rules[r].llen = ll;
    wc->rules[r].rhs = rb; wc->rules[r].rlen = rl;
    wc->tree.rules    = wc->rules;          // keep the alias live across grows
    wc->tree.n_rules  = r + 1u;
    wf_tree_insert(&wc->tree, r);           // BO_ObjektEinfuegen for rule r
  }
  s->wmfpa_built = nr;
  return 1;
}

// Update the cached flat RHS of rule `r` in place after an interreduction
// right-reduction edited s->rhs[r] (composition: l -> r becomes l -> r').
// The DSBaum keys only on the LHS and rule indices are unchanged, so the
// tree spine stays valid -- only the splice template the leaf points at
// needs re-encoding.  This keeps the right-reduction path O(rule) instead
// of forcing a full O(R) tree rebuild (right-reduction fires often on the
// Mix workload, so a rebuild there reintroduces the very O(R^2) swamp this
// package removes).  Returns 0 (caller marks wmfpa_dirty for a rebuild) if
// the cache is absent, not yet built up to r, or the new RHS overflows the
// per-rule buffer -- correctness via rebuild over an in-place fast path.
static int wf_eng_cache_update_rhs(AtpState *s, u32 r) {
  WfEngCache *wc = (WfEngCache *)s->wmfpa_tree;
  if (wc == NULL || r >= s->wmfpa_built || wc->cap_buf < WF_ENG_RULE_NODES) {
    return 0;
  }
  WfNode *rb = wc->rhs_buf + (size_t)r * WF_ENG_RULE_NODES;
  u32 rl = wf_encode((WfTermH)s->rhs[r], rb, WF_ENG_RULE_NODES, &g_wf_eng_enc);
  if (rl == 0u) return 0;       // over-deep new RHS: rebuild drops the rule
  wc->rules[r].rhs = rb; wc->rules[r].rlen = rl;
  return 1;
}

#define WF_ENG_SUBJ_CAP 8192u

// Gated correctness probe: assert the incrementally-maintained tree is
// byte-identical to a tree freshly rebuilt from the current rule set, by
// comparing the lowest-index applicable rule at every preorder position of
// the subject `a` (the exact retrieval wf_step consumes).  ON only when
// THVM_ATP_WMFPA_CHECK is set non-"0".  Aborts on the first divergence so a
// stale/inconsistent tree (== a wrong normal form == a silent soundness
// bug) is caught at the mutation that introduced it, not laundered into a
// proof.
static void wf_eng_check_incremental(AtpState *s, const WfNode *a, u32 alen) {
  WfEngCache *wc = (WfEngCache *)s->wmfpa_tree;
  // Build a throwaway reference tree over the SAME WfRule array (same
  // lhs/rhs buffers, same indices) -- only the node/rec spine differs.
  WfTree ref;
  wf_tree_init(&ref);
  wf_tree_build(&ref, wc->rules, s->n_rules);
  for (u32 p = 0; p < alen; p++) {
    if (WF_IS_VAR(a[p].sym)) continue;
    WfBind bi[WF_MAX_VARS], br[WF_MAX_VARS];
    u32 ri = WF_NIL, rr = WF_NIL;
    int ie = 0, ir = 0;
    u32 hi = wf_descend(&wc->tree, a, p, bi, &ri, &ie) ? ri : WF_NIL;
    u32 hr = wf_descend(&ref,      a, p, br, &rr, &ir) ? rr : WF_NIL;
    // An inexact descent on either tree makes the comparison meaningless
    // (both fall back to the indexed path in the real loop); skip it.
    if (ie || ir) continue;
    if (hi != hr) {
      fprintf(stderr,
              "WMFPA INCREMENTAL-TREE MISMATCH at pos %u: incremental rule "
              "%u vs rebuilt %u (n_rules=%u, built=%u)\n",
              p, hi, hr, s->n_rules, s->wmfpa_built);
      abort();
    }
  }
  wf_tree_free(&ref);
}

static Term atp_rewrite_normalize_wmfpa(AtpState *s, Term t, u32 step_cap) {
  // Correctness probes for THIS (Term-side) path, re-bound from
  // s->wmfpa_check to THVM_ATP_WMFPA_TERM_CHECK alongside the
  // THVM_ATP_WMFPA_TERM re-gate: s->wmfpa_check now arms the WM-policy
  // FT hook's live differential (ft_norm.c atp_rules_only_normalize_wf).
  static int wmfpa_term_check = -1;
  if (wmfpa_term_check < 0) {
    wmfpa_term_check = atp_env_on("THVM_ATP_WMFPA_TERM_CHECK");
  }
  // Bring the cached tree in sync with the current rule set.  The common
  // completion mutation is a single rule APPEND (atp_push_rule); maintain
  // the tree incrementally for that -- encode + BO_ObjektEinfuegen the new
  // rules into the existing spine in O(rule), the win this package unlocks.
  // Fall back to a full rebuild for every case the incremental path cannot
  // safely handle (correctness over speed, never the reverse):
  //   - wmfpa_dirty: an in-place rule edit (interreduction right-reduction)
  //     or an array compaction (a dropped rule renumbers every later index,
  //     which the index-keyed leaf recs and the lowest-index NF policy
  //     cannot absorb in place) -- those sites set wmfpa_dirty.
  //   - wmfpa_built > n_rules: rules vanished without the dirty bit (defensive).
  //   - an append that would outgrow the cached buffers (would dangle the
  //     WfRule pointers): wf_eng_cache_append returns 0 and we rebuild.
  if (s->wmfpa_tree == NULL || s->wmfpa_dirty ||
      s->wmfpa_built > s->n_rules) {
    wf_eng_cache_rebuild(s);
    s->wmfpa_dirty = 0u;
  } else if (s->wmfpa_built < s->n_rules) {
    if (!wf_eng_cache_append(s)) wf_eng_cache_rebuild(s);
  }
  WfEngCache *wc = (WfEngCache *)s->wmfpa_tree;
  static WfNode a[WF_ENG_SUBJ_CAP], b[WF_ENG_SUBJ_CAP];
  u32 alen = wf_encode((WfTermH)t, a, WF_ENG_SUBJ_CAP, &g_wf_eng_enc);
  if (alen == 0u) {
    // over-deep subject: fall back to the indexed path (correctness > speed).
    return atp_rewrite_normalize_indexed(s, t, step_cap);
  }
  if (wmfpa_term_check) wf_eng_check_incremental(s, a, alen);
  WfNode *nfp = NULL;
  int overflow = 0;
  wf_normalize(&wc->tree, a, alen, b, WF_ENG_SUBJ_CAP, step_cap, &nfp,
               &overflow);
  if (overflow) {
    // A rewrite would grow the subject past the flat arena: decode the
    // partial form and finish on the unbounded indexed path (correctness
    // over speed; rare on the AndAssociativity workload).
    u32 pp = 0u;
    Term partial = wf_eng_decode_rec(nfp, &pp);
    return atp_rewrite_normalize_indexed(s, partial, step_cap);
  }
  u32 p = 0u;
  Term got = wf_eng_decode_rec(nfp, &p);
  if (wmfpa_term_check) {
    // Live NF differential against the ground-truth linear normalizer
    // (repeated thvm_rewrite_step -- leftmost-outermost, lowest-index rule,
    // the definition of the IC normal form).  The WM-FPA path is also
    // leftmost-outermost from scratch, so it must equal it on every
    // subject; a divergence is the silent soundness bug this probe exists
    // to catch.  NB: this deliberately does NOT compare against the indexed
    // path -- that path's prev_redex resume can return a DIFFERENT (under-
    // reduced) reduct than a full re-scan on a non-confluent (mid-
    // completion) R, so it is not a sound reference for the true NF.
    Term lin = thvm_rewrite_normalize(t, s->lhs, s->rhs, s->n_rules, step_cap);
    if (!kbo_eq(got, lin)) {
      char ib[4096], gb[4096], lb[4096];
      atp_pretty_term(t, ib, sizeof ib);
      atp_pretty_term(got, gb, sizeof gb);
      atp_pretty_term(lin, lb, sizeof lb);
      fprintf(stderr, "WMFPA NF MISMATCH vs linear (n_rules=%u)\n in=%s\n"
              " wmfpa=%s\n linear=%s\n", s->n_rules, ib, gb, lb);
      abort();
    }
  }
  return got;
}

// Rule-index retrieval stats accessor (the Sheffer-pruning measurement).
// `queries` is the number of atp_ri_query_pos calls; `candidates` the
// leaf records reached; `matchcalls` the thvm_match calls those records
// triggered; `node_visits` the tree nodes touched.  candidates/queries
// is the candidates-returned-per-query: if it tracks n_rules the tree
// does NOT prune on single-symbol Sheffer; if bounded it does.
fn void thvm_atp_ri_stats(const AtpState *s, u64 *queries, u64 *candidates,
                          u64 *matchcalls, u64 *node_visits, u32 *nodes,
                          u32 *n_rules_built) {
  AtpRuleIndex *ix = (s != NULL) ? s->rule_index : NULL;
  if (queries       != NULL) *queries       = (ix != NULL) ? ix->q_queries : 0;
  if (candidates    != NULL) *candidates    = (ix != NULL) ? ix->q_candidates : 0;
  if (matchcalls    != NULL) *matchcalls    = (ix != NULL) ? ix->q_matchcalls : 0;
  if (node_visits   != NULL) *node_visits   = (ix != NULL) ? ix->q_nodevisits : 0;
  if (nodes         != NULL) *nodes         = (ix != NULL) ? ix->n_nodes : 0;
  if (n_rules_built != NULL) *n_rules_built = (ix != NULL) ? ix->n_rules_built : 0;
}

#ifdef ATP_ORDERED_REWRITE
// The flatterm mixed path builds on the ordered-rewrite helpers + the
// reduction-order compare, all defined further down (after atp_compare);
// forward-declare them here so the flat fast-path can call them.
static int    atp_vars_contained(Term a, Term b);
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs);
static u8     g_atp_skip_oriented;   // tentative def; initialised below
static u32    atp_pretty_term(Term t, char *buf, u32 cap);

// Env-gated derivation trace (THVM_ATP_RULE_TRACE=1).  Probes the env
// once; default builds are silent and behaviorally byte-identical.
static int atp_rule_trace_on(void) {
  static int trace_on = -1;
  if (trace_on < 0) trace_on = atp_env_on("THVM_ATP_RULE_TRACE");
  return trace_on;
}
static Term   atp_ordered_rewrite_step(AtpState *s, Term t,
                                       const Term *lhs, const Term *rhs,
                                       u32 n_rules, u8 *fired);
// === flatterm fast-path for the MIXED (orientable + unorientable)
// normalize loop (opt-in, THVM_ATP_FLATTERM=1, default OFF) ===========
//
// The default mixed path (atp_rewrite_normalize_ordered) alternates a
// flat indexed fixpoint (orientable rules) with ONE pointer-tree
// outermost-leftmost unorientable step (atp_ordered_rewrite_step),
// MATERIALISING a tree and RE-FLATTENING the whole subject on every
// unorientable rewrite.  On a self-overlapping axiom with ~10% of R
// unorientable, the per-step tree walk + re-flatten dominates (profiled
// at ~70% of the completion wall in atp_ordered_rewrite_step/_try_top).
//
// This variant keeps the subject in the SAME flat arrays across BOTH
// passes.  The orientable redexes are found+spliced by the existing
// discrimination-tree descent (atp_ri_find_redex / atp_ri_splice); the
// unorientable redex is found by a LINEAR PREORDER SCAN of the flat
// array (flatterm.TO_Schwanz-style next-pointer walk via subsz[]),
// rebuilding the subterm at each CTR position (atp_ri_build at pos) for
// the order-gated thvm_match.  The replacement is spliced in place --
// no per-step re-flatten of the whole subject.  The full tree is built
// ONCE, at the global fixpoint.
//
// Semantics are byte-identical to the default mixed loop:
//   * orientable fixpoint first (lowest-index rule, outermost-leftmost),
//   * then ONE unorientable step (lowest-index equation, outermost-
//     leftmost position, both directions order-gated + variable-safe),
//   * repeat to a joint fixpoint.
// The differential test (tests/test_atp.c, ATP_FLATTERM_DIFF) asserts
// equality against the tree path over random terms + rule sets.

// Run the orientable indexed fixpoint over the ALREADY-FLATTENED shared
// arrays (g_atp_ri_*), leaving the subject flat.  Mirrors the inner loop
// of atp_rewrite_normalize_indexed but takes/returns the flat state by
// reference so the unorientable pass can resume on the same array.
// `*flatlen` / `*folded` are updated in place.  Returns 1 if any
// orientable rewrite fired.
static u8 atp_ft_indexed_fixpoint(AtpState *s, Term *flat, u32 *subsz,
                                  u32 *flatsym, u32 *flatlen, u8 *folded,
                                  u32 step_cap, u32 *min_pos) {
  u8  any = 0u;
  u32 prev_redex = 0u;
  // Lowest preorder position any orientable splice touched this call.  A
  // splice at p modifies p and its subtree, shifts the tail, and grows
  // the ancestor spans -- the leftmost position whose normal-form status
  // could change is p itself.  Track the minimum across the fixpoint so
  // the caller can advance the unorientable resume watermark conservatively.
  u32 lo = *min_pos;
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) { *min_pos = lo; return any; }
    g_atp_ri_query_folded = *folded;
    u32 redex_pos = 0u, redex_rule = 0u;
    if (!atp_ri_find_redex(*flatlen, prev_redex, &redex_pos, &redex_rule)) {
      *min_pos = lo;
      return any;                                   // orientable fixpoint
    }
    RewriteSubst subst = {{0}};
    if (!g_atp_ri_ix->any_folded && !*folded) {
      for (u32 k = 0; k < ATP_DTREE_MAXVARS; k++) {
        if (g_atp_ri_best_star[k] != ATP_DTREE_NIL) {
          // The star position may be an INTERIOR node (a rule var binds a
          // whole subtree, not just a leaf).  An earlier in-place splice
          // updates that subtree's child cells + subsz/flatsym but NOT the
          // ancestor's cached flat[] tree cell, so flat[star] can be STALE.
          // Rebuild the bound subtree from the flat arrays (an untouched
          // subtree rebuilds to its shared original at zero allocation).
          subst.bindings[k] =
              atp_ri_build(flat, subsz, flatsym, g_atp_ri_best_star[k]);
        }
      }
    } else if (!thvm_match(s->lhs[redex_rule],
                           atp_ri_build(flat, subsz, flatsym, redex_pos),
                           &subst)) {
      return any;                                   // unreachable: confirmed
    }
    // Flatterm-native splice port (WM TermzellenT MA/SubstApply
    // analog): when the rule's var bindings are all in-range
    // (g_atp_ri_best_star[k] != ATP_DTREE_NIL and < ATP_DTREE_MAXVARS) AND
    // neither the index nor the subject folded, splice DIRECTLY from
    // the rule rhs Term + star positions into the persistent flat
    // arrays -- no thvm_subst_apply, no atp_ri_build, no
    // atp_ri_flatten round-trip.  Falls back to the Term-tree
    // atp_ri_splice on overrun / fold.  Gated by THVM_ATP_SUBST_FLAT
    // so the default trajectory stays byte-identical.
    u8 spliced = 0u;
    static int dbg_subst_flat = -1;
    if (dbg_subst_flat < 0) dbg_subst_flat = atp_env_on("THVM_ATP_SUBST_FLAT");
    if (dbg_subst_flat && !g_atp_ri_ix->any_folded && !*folded) {
      spliced = atp_ri_splice_inline(flat, subsz, flatsym, flatlen, folded,
                                     redex_pos, s->rhs[redex_rule],
                                     g_atp_ri_best_star, ATP_RI_FLAT_CAP);
      if (spliced) g_atp_ri_splice_inline_hits++;
      else         g_atp_ri_splice_inline_misses++;
    }
    if (!spliced) {
      Term repl = thvm_subst_apply(s->rhs[redex_rule], &subst);
      spliced = atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                              redex_pos, repl, ATP_RI_FLAT_CAP);
    }
    if (spliced) {
      if (redex_pos < lo) lo = redex_pos;
      prev_redex = redex_pos;
      any = 1u;
    } else {
      *min_pos = lo;
      return any;                                   // overrun: caller bails
    }
  }
  *min_pos = lo;
  return any;
}

// Linear-scan unorientable step at a single rebuilt subterm `sub` at
// preorder position `p`: tries each unorientable equation (rule asc,
// l->r then r->l), fires the first strictly order-decreasing instance.
// Returns 2 = fired (spliced), 1 = matched-firable but splice overran
// (caller bails), 0 = nothing fired here.  This is the exact fallback
// the indexed path defers to when the candidate buffer overflows or the
// index is unavailable -- byte-identical verdicts to atp_ordered_try_top.
static u8 atp_ft_unorient_at_linear(AtpState *s, Term *flat, u32 *subsz,
                                    u32 *flatsym, u32 *flatlen, u8 *folded,
                                    u32 p, Term sub) {
  for (u32 i = 0; i < s->n_rules; i++) {
    if (s->r_orient[i]) continue;                 // oriented: indexed pass
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[i], sub, &subst)) {
        Term tmpl = atp_unorient_template(s, s->lhs[i], s->rhs[i]);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &subst);
          if (atp_compare(s, sub, repl) == KBO_GT) {
            return atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                                 p, repl, ATP_RI_FLAT_CAP) ? 2u : 1u;
          }
        }
      }
    }
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->rhs[i], sub, &subst)) {
        Term tmpl = atp_unorient_template(s, s->rhs[i], s->lhs[i]);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &subst);
          if (atp_compare(s, sub, repl) == KBO_GT) {
            return atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                                 p, repl, ATP_RI_FLAT_CAP) ? 2u : 1u;
          }
        }
      }
    }
  }
  return 0u;
}

// Scan the shared flat array preorder for the leftmost-outermost
// position where some UNORIENTABLE equation fires (either direction,
// variable-safe + strictly order-decreasing for the instance).  At each
// CTR position the candidate faces are retrieved from the unorientable
// discrimination index (atp_ri_query_pos_unorient) instead of an
// O(n_rules) linear scan; the LPO order-gate (atp_compare) then runs
// ONLY on those structurally-matching candidates, in the linear scan's
// own priority (rule asc, l->r before r->l).  The first order-decreasing
// face fires -- byte-identical redex/rule/direction to the linear scan.
// On a hit, splices the replacement in place and returns 1.
//
// The candidate buffer (g_atp_ri_cand) is bounded; if a position has
// more matching faces than it holds, OR the index is absent, this falls
// back to the exact linear scan at that position (atp_ft_unorient_at_
// linear) so the verdict is never weakened.
// `resume` is the leftmost preorder position whose unorientable-NF status
// may have changed (or was never examined) since the previous unorient
// scan -- every position q strictly before it that is not one of its
// ancestors (q + subsz[q] <= resume) was examined-and-rejected last scan
// and unchanged since, so it cannot host a firable face and is skipped.
// `fire_pos` (out) reports the preorder position where a face fired, so
// the caller can fold it into the next resume watermark; left untouched
// when nothing fires.  Mirrors atp_ri_find_redex's `clean_before`.
static u8 atp_ft_unorient_step(AtpState *s, Term *flat, u32 *subsz,
                               u32 *flatsym, u32 *flatlen, u8 *folded,
                               u32 resume, u32 *fire_pos) {
  AtpRuleIndex *ux = s->unorient_index;
  AtpRuleIndex *saved_ix = g_atp_ri_ix;
  // The KBO weight memo (g_kbo_wmemo) persists across compares and is
  // keyed by Term cell integer.  The flatterm path splices in place and
  // atp_ri_build returns the original (now content-stale) cell for a
  // subtree it judges unchanged, so a cell integer can carry NEW logical
  // content with no heap move -- silently re-using a stale memo entry and
  // returning a WRONG verdict.  Invalidate once here so every order-gate
  // in this scan weighs the CURRENT term; entries built within the scan
  // are valid (no splice fires until a redex is found, then we return).
  thvm_kbo_invalidate();
  ATP_ORIENT_CACHE_INVAL();
  // Per-position no-fire memo: precompute bottom-up subtree FNV-64
  // hashes once for this subject so the per-position cache key (subtree
  // hash) is a single array read.  Computing flathash[] is O(|flatlen|)
  // -- cheap relative to the n-positions x m-rules descent it skips.
  // Verifier mode (THVM_DEBUG_UNF_POS_MEMO=1) runs BOTH paths and
  // aborts on disagreement -- catches any soundness regression in the
  // memo at the call that exposes it.
  static u64 g_flathash_buf[ATP_RI_FLAT_CAP];
  atp_unf_flathash(flatsym, subsz, *flatlen, g_flathash_buf);
  static int dbg_verify = -1;
  if (dbg_verify < 0) dbg_verify = atp_env_on("THVM_DEBUG_UNF_POS_MEMO");
  for (u32 p = 0; p < *flatlen; p++) {
    if (p < resume && p + subsz[p] <= resume) {
      continue;                              // unchanged, known non-redex
    }
    // A leaf position (variable / NUM -- flatsym below CTR_BASE) can
    // never head a rule LHS that is a non-trivial term, so the only
    // unorientable equation that could fire is a bare `x = y`-style one,
    // which is never order-decreasing (and the orient check would have
    // dropped it).  Skip on every leaf -- O(1) flat lookup.
    if (flatsym[p] < ATP_DTREE_CTR_BASE) continue;
    if (ux == NULL) {
      // No unorient index -- the linear scan needs the materialised
      // subterm.  Rebuild it from the flat arrays (the un-spliced
      // ancestor cell at flat[p] may be stale).
      Term sub = atp_ri_build(flat, subsz, flatsym, p);
      u8 r = atp_ft_unorient_at_linear(s, flat, subsz, flatsym, flatlen,
                                       folded, p, sub);
      if (r == 2u) { *fire_pos = p; return 1u; }
      if (r == 1u) return 0u;                       // overrun: caller bails
      continue;
    }
    // Query the unorient discrimination tree FIRST, against the flat
    // arrays directly -- no per-position subtree materialisation.  Only
    // when the index returns at least one candidate (the rare case --
    // ~0.06 candidates/query on Sheffer) do we pay for atp_ri_build
    // below.  This drops the "rebuild for every CTR position" cost on
    // the dominant NO-CANDIDATE path that the no-fire memo can't catch
    // (e.g. the first call on a freshly-constructed subject).
    g_atp_ri_ix = ux;
    g_atp_ri_query_folded = *folded;  // leaf-collect's perfect-match guard
    u64 phash = g_flathash_buf[p];
    u32 ncand;
    if (!dbg_verify && atp_unf_pos_memo_get(phash, *folded)) {
      g_atp_unf_pos_memo_hits++;
      g_atp_ri_ix = saved_ix;
      continue;                                     // memoed: no fire here
    }
    ncand = atp_ri_query_pos_unorient(p);
    if (dbg_verify) {
      u8 memo_says_nofire = atp_unf_pos_memo_get(phash, *folded);
      if (memo_says_nofire && ncand != 0u) {
        fprintf(stderr, "POS_MEMO UNSOUND at pos=%u phash=0x%llx ncand=%u\n",
                p, (unsigned long long)phash, ncand);
        fprintf(stderr, "  flatlen=%u  folded=%u  qend=%u\n",
                *flatlen, *folded, p + subsz[p]);
        fprintf(stderr, "  n_rules=%u  n_unorient=%u  dirty=%u\n",
                s->n_rules, s->n_unorient, s->rule_index_dirty);
        abort();
      }
    }
    g_atp_ri_ix = saved_ix;
    if (ncand == 0u) {                              // no firable face here
      atp_unf_pos_memo_put(phash, *folded);
      g_atp_unf_pos_memo_misses++;
      continue;
    }
    // Now we have candidate face(s) -- materialise the subterm for the
    // match/order checks below.
    Term sub = atp_ri_build(flat, subsz, flatsym, p);
    if (ncand >= ATP_RI_MAXCAND) {
      // Buffer saturated -- the index dropped faces; redo this position
      // with the exact linear scan so no candidate is missed.
      u8 r = atp_ft_unorient_at_linear(s, flat, subsz, flatsym, flatlen,
                                       folded, p, sub);
      if (r == 2u) { *fire_pos = p; return 1u; }
      if (r == 1u) return 0u;
      continue;
    }
    // Sort the candidates into the redex-selection priority order.
    //
    // Default engine: the linear scan tries each rule ascending, l->r
    // before r->l, and fires the first order-decreasing instance, so the
    // priority key is (rule << 1) | dir (dir: l->r = 0, r->l = 1) --
    // ascending in this key reproduces (rule asc, l->r first) EXACTLY.
    // (The leaf rec packs dir in the high bit, which is a fine encoding
    // but the WRONG sort order across rules -- r->l of rule i must
    // precede l->r of rule i+1, which the high-bit packing inverts -- so
    // re-key here.)
    //
    // WM-emission-order mode (use_emission_order, the Waldmeister
    // presets): WM's NormalformMixMost consults the Gleichungsbaum via
    // MO_GleichungGefunden (MatchOperationen.c:658-763), a DFS that
    // reaches the equation leaf in WM's discrimination-tree order, then
    // walks that leaf's GleichungsT chain and fires the FIRST chain
    // member whose grounded RHS is order-decreasing.  thvm slot order is
    // NOT that order: two AC-permutation equations sharing the leaf
    // `or(x,or(y,z))` differ only by which rotation fires, so slot order
    // picks a different rotation than WM and bakes a swapped commutative
    // argument order into the resulting rule (Huntington DN rule12 vs WM
    // rule9 inner-or args).  Re-key by the equation's leaf-list rank +
    // within-leaf chain index (atp_wmo_eq_leaflist_rank, the same wm_order
    // mirror that orders CP emission); unranked faces (mirror miss) sort
    // AFTER the ranked ones, by slot, so the fallback is the legacy order.
    //
    // N is tiny (faces matching one subterm); insertion sort, stable on
    // equal keys (duplicates cannot occur -- one rec per (rule, dir)).
    // `cand_key[k]` is the 64-bit sort key; `cand_face[k]` carries the
    // FULL packed (rule | dir-bit) face so a rule index past the sort
    // key's slot field is never truncated (rule ids run to several
    // thousand, far past a 10-bit field).
    static u64 cand_key[ATP_RI_MAXCAND];
    static u32 cand_face[ATP_RI_MAXCAND];
    for (u32 k = 0; k < ncand; k++) {
      u32 packed = g_atp_ri_cand[k];
      u32 rule   = packed & ~ATP_RI_DIR_BIT;
      u32 rl     = (packed & ATP_RI_DIR_BIT) ? 1u : 0u;
      u32 slot_key = (rule << 1) | rl;
      cand_face[k] = packed;
      if (s->use_emission_order) {
        u32 ll = 0u, ch = 0u;
        // Key layout (high -> low): [unranked flag:1][arrival:13][ch:8]
        // [slot_key:full].  Ranked faces (flag 0) sort before unranked
        // (flag 1); ties on (arrival, ch) fall back to slot order.
        //
        // The rank is the DFS ARRIVAL of this equation face against the
        // concrete redex `sub` (atp_wmo_eq_tops_rank), the faithful WM
        // MO_GleichungGefunden retrieval order: the Gleichungsbaum descent
        // visits the matching function-symbol branch before the variable
        // branch (MatchOperationen.c:678-704), so a specific
        // function-prefix pattern fires before a more-general
        // variable-prefix pattern that also matches the redex.  The
        // depth-ordered leaf list inverts that for competing positions.
        //
        // ID-SPACE NOTE (2026-07-05): r_trace[rule] is a TRACE id, but
        // the wmo chains key on BIRTH UIDS (r_uid) -- this lookup has
        // been an effective always-miss (slot-order fallback) since the
        // mirror switched to uid keying, and the full gate battery
        // (16/16 plain byte-identity + OA -auto prefix 8581) is pinned
        // to that fallback.  Switching to s->r_uid[rule] ACTIVATES the
        // dormant arrival ranking and changes normalize-redex choices;
        // do that only with a fresh wmcli parity study, not as a
        // drive-by (see project_atp_boolean_twin_leaf_miss).
        if (atp_wmo_eq_tops_rank(s, s->r_trace[rule], rl, sub, &ll, &ch)) {
          if (ll > 0x1fffu) ll = 0x1fffu;
          if (ch > 0xffu)   ch = 0xffu;
          cand_key[k] = ((u64)ll << 32) | ((u64)ch << 24) | (u64)slot_key;
        } else {
          cand_key[k] = (1ull << 63) | (u64)slot_key;
        }
      } else {
        cand_key[k] = (u64)slot_key;
      }
    }
    for (u32 a = 1u; a < ncand; a++) {
      u64 vk = cand_key[a];
      u32 vf = cand_face[a];
      u32 b = a;
      while (b > 0u && cand_key[b - 1u] > vk) {
        cand_key[b] = cand_key[b - 1u];
        cand_face[b] = cand_face[b - 1u];
        b--;
      }
      cand_key[b]  = vk;
      cand_face[b] = vf;
    }
    for (u32 k = 0; k < ncand; k++) {
      u32 packed = cand_face[k];
      u32 rule   = packed & ~ATP_RI_DIR_BIT;
      u8  rl     = (packed & ATP_RI_DIR_BIT) ? 1u : 0u;
      Term pat   = rl ? s->rhs[rule] : s->lhs[rule];
      Term other = rl ? s->lhs[rule] : s->rhs[rule];
      RewriteSubst subst = {{0}};
      if (!thvm_match(pat, sub, &subst)) continue;  // index over-approx
      Term tmpl = atp_unorient_template(s, pat, other);
      if (tmpl == 0) continue;        // unusable direction (defensive --
                                      // insertion already filters these)
      Term repl = thvm_subst_apply(tmpl, &subst);
      if (atp_compare(s, sub, repl) == KBO_GT) {
        if (atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                          p, repl, ATP_RI_FLAT_CAP)) { *fire_pos = p; return 1u; }
        return 0u;                                  // overrun: caller bails
      }
    }
  }
  return 0u;
}


// Indexed single unorientable-rewrite step on a TREE subject (default
// engine).  Flattens `t` once, runs the indexed unorient retrieval
// (atp_ft_unorient_step) to find AND splice the first outermost-leftmost
// strictly-order-decreasing unorientable face, then rebuilds the tree.
// This replaces the mixed loop's `atp_ordered_rewrite_step` with
// g_atp_skip_oriented -- an O(n_rules) linear scan over every unorientable
// equation at every position, the measured normalize hotspot.  The redex
// chosen is byte-identical (atp_ft_unorient_step mirrors the linear scan's
// preorder + (rule asc, l->r then r->l) priority + KBO gate), so the
// normal form -- and the whole saturation trajectory -- is unchanged.
// `*fired` reports whether a rewrite happened.  On an index-absent / over-
// deep / candidate-overflow / splice-overrun fallback it returns `t`
// with `*fired` per the linear-scan verdict so completeness is preserved.
static Term atp_unorient_step_indexed(AtpState *s, Term t, u8 *fired) {
  *fired = 0;
  u64 _ust_t0 = 0;
  if (g_atp_phase_enabled) { g_atp_unorient_step_calls++; _ust_t0 = atp_now_us(); }
  if (s->rule_index == NULL) s->rule_index = atp_ri_new();
  if (s->unorient_index == NULL) s->unorient_index = atp_ri_new();
  if (s->rule_index_dirty || s->rule_index->n_rules_built != s->n_rules ||
      s->unorient_index->n_rules_built != s->n_rules ||
      s->unorient_index->built_revision != s->r_revision) {
    atp_ri_rebuild(s);
    atp_unf_memo_invalidate(s->r_revision, 0u);  // step epoch bumps on r_revision delta
  }
  // No-fire structural-hash memo: catches the mandatory "fixpoint
  // reached, no more unorient redex" call at the end of every mixed-
  // loop normalize.  See the AtpUnfMemo header for the soundness
  // contract.
  u64 _t_hash = atp_term_struct_hash(t);
  if (atp_unf_memo_get(_t_hash)) {
    if (g_atp_phase_enabled) {
      g_atp_unf_memo_hits++;
      g_atp_unorient_step_empty++;
      g_atp_unorient_step_us += atp_now_us() - _ust_t0;
    }
    return t;
  }
  if (g_atp_phase_enabled) g_atp_unf_memo_misses++;
  static Term uflat[ATP_RI_FLAT_CAP];
  static u32  usubsz[ATP_RI_FLAT_CAP];
  static u32  uflatsym[ATP_RI_FLAT_CAP];
  u32 flatlen = 0u;
  u8  folded  = 0u;
  if (!atp_ri_flatten(t, uflat, usubsz, uflatsym, &folded,
                      ATP_RI_FLAT_CAP, &flatlen)) {
    // Over-deep subject: the index cannot represent it.  Fall back to the
    // proven linear KBO-gated step for this one step (same verdict).
    g_atp_skip_oriented = 1u;
    Term t2 = atp_ordered_rewrite_step(s, t, s->lhs, s->rhs, s->n_rules, fired);
    g_atp_skip_oriented = 0u;
    return t2;
  }
  g_atp_ri_flat    = uflat;
  g_atp_ri_subsz   = usubsz;
  g_atp_ri_flatsym = uflatsym;
  g_atp_ri_lhs     = s->lhs;
  g_atp_ri_rhs     = s->rhs;  // r->l face records dereference rhs[rule] in leaf_collect_unorient
  u32 fire_pos = 0u;
  u8  hit = atp_ft_unorient_step(s, uflat, usubsz, uflatsym, &flatlen,
                                 &folded, 0u, &fire_pos);
  if (!hit) {
    // Cache the negative verdict so a structurally-identical subject in
    // a later call skips the full preorder scan.
    atp_unf_memo_put(_t_hash);
    if (g_atp_phase_enabled) {
      g_atp_unorient_step_empty++;
      g_atp_unorient_step_us += atp_now_us() - _ust_t0;
    }
    return t;            // no firable unorientable face: caller stops
  }
  if (g_atp_phase_enabled) {
    g_atp_unorient_step_fires++;
    g_atp_unorient_step_us += atp_now_us() - _ust_t0;
  }
  *fired = 1;
  return atp_ri_build(uflat, usubsz, uflatsym, 0u);
}

// Flatterm mixed normalizer (opt-in).  Keeps the subject flat across the
// orientable indexed fixpoint AND the unorientable pass, splicing every
// rewrite in place; the tree is built ONCE at the joint fixpoint.
// Equivalent to atp_rewrite_normalize_ordered's mixed branch.  Returns
// the SAME normal form (asserted by ATP_FLATTERM_DIFF).
static Term atp_rewrite_normalize_flatterm_mixed(AtpState *s, Term t,
                                                 u32 step_cap) {
  if (s->rule_index == NULL) s->rule_index = atp_ri_new();
  // The unorientable-faces index is needed only on this (flatterm) path;
  // allocate it lazily here so the default engine never carries it.
  // atp_ri_rebuild populates it when it (re)builds rule_index.
  if (s->unorient_index == NULL) s->unorient_index = atp_ri_new();
  if (s->rule_index_dirty || s->rule_index->n_rules_built != s->n_rules ||
      s->unorient_index->n_rules_built != s->n_rules ||
      s->unorient_index->built_revision != s->r_revision) {
    atp_ri_rebuild(s);
    atp_unf_memo_invalidate(s->r_revision, 0u);  // step epoch bumps on r_revision delta
  }
  static Term flat[ATP_RI_FLAT_CAP];
  static u32  subsz[ATP_RI_FLAT_CAP];
  static u32  flatsym[ATP_RI_FLAT_CAP];
  g_atp_ri_ix      = s->rule_index;
  g_atp_ri_flat    = flat;
  g_atp_ri_subsz   = subsz;
  g_atp_ri_flatsym = flatsym;
  g_atp_ri_lhs     = s->lhs;
  g_atp_ri_rhs     = s->rhs;
  for (u32 k = 0; k < ATP_DTREE_MAXVARS; k++) g_atp_ri_star[k] = ATP_DTREE_NIL;

  u32 flatlen = 0u;
  u8  folded  = 0u;
  if (!atp_ri_flatten(t, flat, subsz, flatsym, &folded,
                      ATP_RI_FLAT_CAP, &flatlen)) {
    // Over-deep subject: cannot flatten -- fall back to the proven tree
    // mixed loop for this call (no semantic difference, just slower).
    for (u32 i = 0; i < step_cap; i++) {
      if (atp_norm_deadline_fired(s)) return t;
      t = atp_rewrite_normalize_indexed(s, t, step_cap);
      u8 fired = 0;
      g_atp_skip_oriented = 1u;
      Term t2 = atp_ordered_rewrite_step(s, t, s->lhs, s->rhs,
                                         s->n_rules, &fired);
      g_atp_skip_oriented = 0u;
      if (!fired) break;
      t = t2;
    }
    return t;
  }

  // Incremental resume watermark for the unorientable preorder scan,
  // mirroring atp_ri_find_redex's `clean_before` for the orientable side.
  // It is the leftmost position whose unorientable-NF status may have
  // changed since the previous unorient scan: the min of (a) the position
  // the previous unorient step fired at and (b) the leftmost orientable
  // splice the interleaved indexed fixpoint then made.  Positions whose
  // whole subtree lies before it were examined-and-rejected last scan and
  // are untouched, so they cannot host a firable face.  Reset to 0 (full
  // scan) when resume is disabled.  A rewrite from either pass can only
  // enable a new redex at or after its own (leftmost) site, never to the
  // left in an untouched sibling -- so the min watermark is sound.
  u32 un_resume = 0u;
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) break;
    // The indexed fixpoint may overrun on a splice; on overrun it stops
    // having done partial work, the unorientable pass would too, so we
    // materialise and hand the rest to the tree loop.  Detect overrun by
    // re-checking after: a clean fixpoint leaves no orientable redex.
    u32 or_min = flatlen;                       // sentinel: no orientable change
    atp_ft_indexed_fixpoint(s, flat, subsz, flatsym, &flatlen, &folded,
                            step_cap, &or_min);
    // The orientable splices since the last unorient scan also invalidate
    // positions from or_min onward; fold them into the watermark.  (On the
    // first iteration un_resume is still 0, so the first scan is full.)
    if (or_min < un_resume) un_resume = or_min;     // iter 0: un_resume==0
    if (!s->ft_unorient_resume) un_resume = 0u;
    u32 fire_pos = flatlen;
    u8 fired = atp_ft_unorient_step(s, flat, subsz, flatsym, &flatlen,
                                    &folded, un_resume, &fire_pos);
    if (!fired) break;          // joint fixpoint (or splice overrun: rare)
    // Next scan may resume past everything before this fire site (it was
    // examined-and-rejected); the interleaved indexed fixpoint's or_min is
    // folded in at the top of the next iteration.
    un_resume = fire_pos;
  }
  return atp_ri_build(flat, subsz, flatsym, 0u);
}

#if defined(ATP_FLATTERM_SELFCHECK) || defined(ATP_FLATTERM_DIFF)
// The exact tree mixed loop (a copy of atp_rewrite_normalize_ordered's
// mixed branch), used only by the build-time self-check / DIFF test to
// confirm the flatterm path's normal form matches.  Defeats the speedup;
// never compiled into a release build.
static Term atp_rewrite_normalize_flatterm_selfcheck_tree(AtpState *s, Term t,
                                                          u32 step_cap) {
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    t = atp_rewrite_normalize_indexed(s, t, step_cap);
    u8 fired = 0;
    g_atp_skip_oriented = 1u;
    Term t2 = atp_ordered_rewrite_step(s, t, s->lhs, s->rhs,
                                       s->n_rules, &fired);
    g_atp_skip_oriented = 0u;
    if (!fired) break;
    t = t2;
  }
  return t;
}
#endif
#endif // ATP_ORDERED_REWRITE

// === CP-generation overlap-partner index (WM U1_KPsBildenZuRegel) ===
//
// THE CP-GEN CROSS-PRODUCT.  thvm_atp_generate_cps_c overlaps a new rule
// i into every existing rule j -- for j over ALL n_rules it calls
// atp_overlap_ij -> thvm_critical_pairs_pair, which walks the positions
// of li and thvm_unify's rule-j's lhs lj there.  A CP is emitted only
// when lj unifies with a non-variable subterm of li, but the scan PAYS
// for every j whether or not any overlap exists -- an O(n_rules)-per-rule
// scan.  Waldmeister avoids it via the discrimination tree: for a new
// rule it forms overlaps only against the rules the tree retrieves as
// unifiable (sources/INF/Unifikation1.c:1480 U1_KPsBildenZuRegel ->
// TermMitDSBaumUnifizieren / TermMitDSBaumTeiltermenUnifizieren on
// RE_Regelbaum).
//
// MEASURED SCOPE -- single-symbol theories defeat the filter.  On the
// Sheffer-stroke axiom set (one binary symbol `nand`; AndAssociativity /
// DoubleNegation over WolframAxioms) EVERY rule LHS is nand-headed and
// every overlap subterm is nand-headed, so the discrimination tree cannot
// discriminate at the shallow positions -- the candidate set equals the
// full rule set (cand == n, measured) and the filter saves nothing.  The
// profiled cost on that problem is NOT the cross-product scan but the
// per-generated-CP work: CP reduction (atp_rewrite_normalize) plus the
// KBO atp_compare in the ordered-rewrite order-gate, both proportional to
// CPs GENERATED, which an overlap filter cannot reduce while preserving
// the CP set.  The index is therefore a no-op-cost, CP-set-identical
// speedup ONLY for MULTI-symbol theories, where distinct LHS heads let
// the tree prune the cross-product.  Kept gated off; reported honestly.
//
// THE PORT -- a candidate FILTER, CP-set-preserving.  cp_index is a
// discrimination tree over the WHOLE rule-LHS terms (reusing atp_ri_*'s
// node/rec pools, atp_dtree_flatsym first-appearance scheme, atp_ri_child
// insert).  For the new rule i, the partners j the cross-product can
// emit a CP for are exactly { j : lj unifies with some non-var subterm
// of li }.  atp_cp_index_collect descends cp_index in UNIFICATION mode
// against a query subterm and gathers every rule whose stored LHS could
// unify -- a SUPERSET of the true partners (a discrimination tree on the
// flat symbol string cannot decide unifiability exactly, only filter).
// thvm_atp_generate_cps_c then runs the EXACT atp_overlap_ij on each
// candidate; cp_visit's thvm_unify is the authoritative gate, so the CP
// set is byte-identical to the unindexed scan -- the index only skips
// the j's that provably cannot overlap.
//
// UNIFICATION DESCENT (vs atp_ri_descend's one-way match).  Both the
// stored LHS and the query subterm carry variables, so at each position:
//   - stored STAR (rule var): unifies with the whole query subterm ->
//     descend the STAR child, skipping the query subterm.
//   - query STAR (subject var): unifies with anything stored -> the
//     query consumed one position; the stored side may be ANY subtree,
//     so descend EVERY child (CTR children with their subtree skipped,
//     and the STAR child).
//   - stored CTR == query CTR: descend (head consumed on both sides).
// No variable-consistency check is applied (that would need an occurs-
// /binding-aware unify; the filter stays a sound superset without it).

#define ATP_CP_MAXCAND 65536u
static u32 g_atp_cp_cand[ATP_CP_MAXCAND];
static u32 g_atp_cp_ncand     = 0;
static u8  g_atp_cp_overflow  = 0;     // candidate buffer overflowed -> full scan
// De-dup: a rule reached via several positions/branches is collected once.
// g_atp_cp_seen[rule] == g_atp_cp_epoch marks "already in g_atp_cp_cand".
static u32 *g_atp_cp_seen   = NULL;
static u32  g_atp_cp_seencap = 0;
static u32  g_atp_cp_epoch  = 0;

static AtpRuleIndex *g_atp_cp_ix      = NULL;
static const Term   *g_atp_cp_qflat   = NULL;
static const u32    *g_atp_cp_qsubsz  = NULL;
static const u32    *g_atp_cp_qflatsym = NULL;

static void atp_cp_cand_add(u32 rule) {
  if (rule < g_atp_cp_seencap && g_atp_cp_seen[rule] == g_atp_cp_epoch) return;
  if (rule < g_atp_cp_seencap) g_atp_cp_seen[rule] = g_atp_cp_epoch;
  if (g_atp_cp_ncand >= ATP_CP_MAXCAND) { g_atp_cp_overflow = 1u; return; }
  g_atp_cp_cand[g_atp_cp_ncand++] = rule;
}

static int atp_cp_cand_cmp(const void *a, const void *b) {
  u32 x = *(const u32 *)a, y = *(const u32 *)b;
  return (x > y) - (x < y);
}

// Sort the collected candidates ascending so the indexed generator
// processes overlap pairs in the SAME (j ascending) order the unindexed
// n_rules scan did -- a CP's FIFO trace tiebreak then matches, keeping
// the derived-rule sequence byte-identical.
static void atp_cp_cand_sort(void) {
  qsort(g_atp_cp_cand, g_atp_cp_ncand, sizeof(u32), atp_cp_cand_cmp);
}

static void atp_cp_index_leaf(u32 node) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DTREE_NIL;
       r = ix->recs[r].next) {
    atp_cp_cand_add(ix->recs[r].rule);
  }
}

// Collect EVERY rule in the whole subtree rooted at `node` -- the action
// when a query variable consumes the corresponding stored subtree (a var
// unifies with anything, so any stored continuation is a candidate).
static void atp_cp_index_collect_subtree(u32 node) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  atp_cp_index_leaf(node);
  for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
       c = ix->nodes[c].sibling) {
    if (g_atp_cp_overflow) return;
    atp_cp_index_collect_subtree(c);
  }
}

// Descend cp_index from `node` against the flat query subject slice
// starting at preorder position `pos` (which spans the query subterm).
// Collects every reachable leaf's rules (unification-compatible filter).
static void atp_cp_index_descend(u32 node, u32 pos, u32 qend) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  if (pos == qend) { atp_cp_index_leaf(node); return; }
  u32 qsym = g_atp_cp_qflatsym[pos];
  u8  q_is_star = (qsym >= ATP_DTREE_STAR_BASE && qsym < ATP_DTREE_CTR_BASE);
  u32 sz   = g_atp_cp_qsubsz[pos];
  for (u32 c = ix->nodes[node].child; c != ATP_DTREE_NIL;
       c = ix->nodes[c].sibling) {
    if (g_atp_cp_overflow) return;
    u32 csym = ix->nodes[c].sym;
    if (csym >= ATP_DTREE_STAR_BASE && csym < ATP_DTREE_CTR_BASE) {
      // Stored rule var unifies with the whole query subterm at `pos`:
      // the stored side consumed one tree edge (the STAR), the query
      // consumed its whole subterm (sz positions).
      atp_cp_index_descend(c, pos + sz, qend);
    } else if (csym == qsym) {
      // Stored CTR/NUM head equals the query head: consume both heads
      // (one tree edge, one query position).
      atp_cp_index_descend(c, pos + 1u, qend);
    }
    // A non-matching stored CTR vs a concrete query CTR cannot unify --
    // pruned (the discrimination-tree win).
  }
  if (q_is_star) {
    // Query var unifies with ANY stored subtree at this node: collect
    // every rule reachable below `node` (var-vs-anything).  The query
    // already advanced past the var (a single position) at the caller's
    // continuation; here we simply harvest the whole stored remainder.
    atp_cp_index_collect_subtree(node);
  }
}

// Insert one whole-face term `t` (a rule's lhs OR rhs) into the cp_index,
// keyed to `rule`.  Both faces of every rule are inserted: an UNFAILING
// overlap (atp_overlap_ij combos 1-3) plants the inner rule's RHS face
// (or walks the outer's RHS face), so a candidate filter that indexed the
// LHS face only would MISS the j whose RHS unifies with the query subterm
// -- dropping a CP and breaking WM-trace parity.  Indexing both faces
// keeps the candidate set a sound SUPERSET of every overlap combo; the
// de-dup in atp_cp_cand_add collapses the two face records of one rule.
static void atp_cp_index_insert_face(AtpRuleIndex *ix, Term t, u32 rule) {
  AtpDTreeVarMap vm;
  atp_dtree_varmap_reset(&vm);
  u32 node = atp_ri_insert_term(ix, ix->root, t, &vm);
  u32 rec  = atp_ri_rec_new(ix);
  ix->recs[rec].rule       = rule;
  ix->recs[rec].next       = ix->nodes[node].rec_head;
  ix->nodes[node].rec_head = rec;
}

static void atp_cp_seen_ensure(AtpState *s) {
  if (g_atp_cp_seencap < s->n_rules) {
    u32 cap = g_atp_cp_seencap ? g_atp_cp_seencap : 1024u;
    while (cap < s->n_rules) cap *= 2u;
    u32 *p = (u32 *)realloc(g_atp_cp_seen, (size_t)cap * sizeof(u32));
    if (p == NULL) thvm_fatal("atp_cp_index: seen OOM");
    g_atp_cp_seen   = p;
    for (u32 k = g_atp_cp_seencap; k < cap; k++) g_atp_cp_seen[k] = 0u;
    g_atp_cp_seencap = cap;
  }
}

// Build cp_index from s->lhs/rhs[0..n_rules): both whole faces per rule,
// keyed by the atp_dtree_flatsym first-appearance symbol string.  Rebuilt
// (like rule_index) whenever R mutates.
static void atp_cp_index_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->cp_index;
  // Incremental fast-path: when only new rules were appended, insert
  // only the tail.  Same soundness logic as atp_ri_extend on
  // rule_index: existing leaves carry correct indices.  Cuts O(R)
  // rebuild to O(rules-added) per cp-gen call.  Pure-append is
  // detected by the revision delta matching the count delta (see the
  // built_revision field comment); the shared rule_index_dirty flag is
  // NOT a usable signal here -- the rewrite-index rebuild clears it
  // between interreduce and CP generation.
  if (ix->n_rules_built > 0u && ix->n_rules_built < s->n_rules
      && ix->root != ATP_DTREE_NIL
      && s->r_revision - ix->built_revision
             == s->n_rules - ix->n_rules_built) {
    for (u32 i = ix->n_rules_built; i < s->n_rules; i++) {
      atp_cp_index_insert_face(ix, s->lhs[i], i);
      atp_cp_index_insert_face(ix, s->rhs[i], i);
    }
    ix->n_rules_built  = s->n_rules;
    ix->built_revision = s->r_revision;
    atp_cp_seen_ensure(s);
    return;
  }
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->root    = atp_ri_node_new(ix, ATP_DTREE_NIL);
  for (u32 i = 0; i < s->n_rules; i++) {
    atp_cp_index_insert_face(ix, s->lhs[i], i);
    atp_cp_index_insert_face(ix, s->rhs[i], i);
  }
  ix->n_rules_built  = s->n_rules;
  ix->built_revision = s->r_revision;
  atp_cp_seen_ensure(s);
}

// Begin a candidate-collection pass: clear the buffer + de-dup epoch so a
// fresh query rule's candidates accumulate from empty.  Both faces of the
// query rule descend into the SAME pass, so a rule reachable via either
// face is collected once.
static void atp_cp_collect_begin(void) {
  g_atp_cp_ncand    = 0;
  g_atp_cp_overflow = 0;
  if (++g_atp_cp_epoch == 0u) {        // wrapped: clear so stale != epoch
    for (u32 k = 0; k < g_atp_cp_seencap; k++) g_atp_cp_seen[k] = 0u;
    g_atp_cp_epoch = 1u;
  }
}

// Descend cp_index against every non-var subterm position of one query
// FACE `qf` (a rule's lhs or rhs), accumulating into the current pass.
// cp_visit walks exactly the non-var positions; querying each gathers the
// rules whose stored face could unify there.  Sets g_atp_cp_overflow on a
// too-deep subject.
static void atp_cp_index_collect_face(AtpState *s, Term qf) {
  static Term qflat[ATP_RI_FLAT_CAP];
  static u32  qsubsz[ATP_RI_FLAT_CAP];
  static u32  qflatsym[ATP_RI_FLAT_CAP];
  u8  folded = 0;
  u32 pos    = 0;
  if (!atp_ri_flatten(qf, qflat, qsubsz, qflatsym, &folded,
                      ATP_RI_FLAT_CAP, &pos)) {
    g_atp_cp_overflow = 1u;            // subject too deep -> exact scan
    return;
  }
  g_atp_cp_ix       = s->cp_index;
  g_atp_cp_qflat    = qflat;
  g_atp_cp_qsubsz   = qsubsz;
  g_atp_cp_qflatsym = qflatsym;
  for (u32 p = 0; p < pos; p++) {
    u32 qsym = qflatsym[p];
    if (qsym >= ATP_DTREE_STAR_BASE && qsym < ATP_DTREE_CTR_BASE) continue; // var
    u32 qend = p + qsubsz[p];
    atp_cp_index_descend(s->cp_index->root, p, qend);
    if (g_atp_cp_overflow) return;
  }
}

// Collect the candidate partner rules for overlapping a new rule `f` as
// the OUTER face: every rule whose stored face unifies with a non-var
// subterm of either of f's faces.  Both faces of f are queried because an
// unfailing outer overlaps from its RHS face too (atp_overlap_ij combos
// 2/3).  Returns the count (in g_atp_cp_cand) or sets g_atp_cp_overflow.
static u32 atp_cp_index_collect(AtpState *s, u32 f) {
  atp_cp_collect_begin();
  atp_cp_index_collect_face(s, s->lhs[f]);
  if (g_atp_cp_overflow) return 0;
  atp_cp_index_collect_face(s, s->rhs[f]);
  if (g_atp_cp_overflow) return 0;
  return g_atp_cp_ncand;
}

// Insert every NON-VAR subterm position of `t` (under one shared
// first-appearance varmap, so a rule's repeated var is consistent across
// its subterms) as a separate tree path keyed to `rule`.  A var-headed
// subterm is skipped (cp_visit never overlaps at a variable position).
static void atp_cp_subindex_insert(AtpRuleIndex *ix, Term t, u32 rule,
                                   AtpDTreeVarMap *vm) {
  if (term_tag(t) == TAG_CTR) {
    u32 node = atp_ri_insert_term(ix, ix->root, t, vm);
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule       = rule;
    ix->recs[rec].next       = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      atp_cp_subindex_insert(ix, term_ctr_at(t, i), rule, vm);
    }
  }
}

// Build cp_subindex: every non-var subterm of BOTH faces of every rule ->
// rule.  Both faces are inserted for the same unfailing-overlap reason as
// atp_cp_index_rebuild: the new rule's face plants into an OLD rule's RHS
// face too (atp_overlap_ij combos 1/2 in the old-i x new-j direction), so
// indexing only the LHS subterms would drop those partners.
static void atp_cp_subindex_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->cp_subindex;
  // Incremental fast-path: same shape as atp_cp_index_rebuild,
  // including the revision-delta pure-append discriminator (see the
  // built_revision field comment).
  if (ix->n_rules_built > 0u && ix->n_rules_built < s->n_rules
      && ix->root != ATP_DTREE_NIL
      && s->r_revision - ix->built_revision
             == s->n_rules - ix->n_rules_built) {
    AtpDTreeVarMap vm;
    for (u32 i = ix->n_rules_built; i < s->n_rules; i++) {
      atp_dtree_varmap_reset(&vm);
      atp_cp_subindex_insert(ix, s->lhs[i], i, &vm);
      atp_dtree_varmap_reset(&vm);
      atp_cp_subindex_insert(ix, s->rhs[i], i, &vm);
    }
    ix->n_rules_built  = s->n_rules;
    ix->built_revision = s->r_revision;
    return;
  }
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->root    = atp_ri_node_new(ix, ATP_DTREE_NIL);
  AtpDTreeVarMap vm;
  for (u32 i = 0; i < s->n_rules; i++) {
    atp_dtree_varmap_reset(&vm);
    atp_cp_subindex_insert(ix, s->lhs[i], i, &vm);
    atp_dtree_varmap_reset(&vm);
    atp_cp_subindex_insert(ix, s->rhs[i], i, &vm);
  }
  ix->n_rules_built  = s->n_rules;
  ix->built_revision = s->r_revision;
}

// Descend cp_subindex against one whole query face `qf` (a rule's lhs or
// rhs), accumulating into the current pass: gather every rule whose stored
// face has a non-var subterm unifiable with `qf`.
static void atp_cp_subindex_collect_face(AtpState *s, Term qf) {
  static Term qflat[ATP_RI_FLAT_CAP];
  static u32  qsubsz[ATP_RI_FLAT_CAP];
  static u32  qflatsym[ATP_RI_FLAT_CAP];
  u8  folded = 0;
  u32 pos    = 0;
  if (!atp_ri_flatten(qf, qflat, qsubsz, qflatsym, &folded,
                      ATP_RI_FLAT_CAP, &pos)) {
    g_atp_cp_overflow = 1u;
    return;
  }
  g_atp_cp_ix       = s->cp_subindex;
  g_atp_cp_qflat    = qflat;
  g_atp_cp_qsubsz   = qsubsz;
  g_atp_cp_qflatsym = qflatsym;
  atp_cp_index_descend(s->cp_subindex->root, 0u, pos);
}

// Collect candidate partner rules for the (old i x new f) direction: every
// old rule i whose stored face has a non-var subterm unifiable with one of
// f's whole faces.  Both faces of f are queried because an unfailing new
// rule plants from its RHS face too (atp_overlap_ij combo 1 in this
// direction).  Returns the count (in g_atp_cp_cand) or sets overflow.
static u32 atp_cp_subindex_collect(AtpState *s, u32 f) {
  atp_cp_collect_begin();
  atp_cp_subindex_collect_face(s, s->lhs[f]);
  if (g_atp_cp_overflow) return 0;
  atp_cp_subindex_collect_face(s, s->rhs[f]);
  if (g_atp_cp_overflow) return 0;
  return g_atp_cp_ncand;
}

#endif // ATP_RULE_INDEX

// Proof-trace soft cap (entries).  Resolved once from THVM_ATP_TRACE_MAX,
// falling back to ATP_MAX_TRACE so an unset env keeps the historical drop
// point.  A bad/zero/oversize value falls back to the default.  The trace
// buffer grows on demand (atp_trace_ensure) until this cap is reached.
fn u32 thvm_atp_trace_cap(void) {
  static u32 cap = 0;
  if (cap == 0) {
    cap = ATP_MAX_TRACE;
    const char *e = getenv("THVM_ATP_TRACE_MAX");
    if (e != NULL && *e != '\0') {
      char *end = NULL;
      unsigned long long v = strtoull(e, &end, 0);
      if (end != NULL && *end == '\0' && v >= ATP_MAX_TRACE && v <= (1ULL << 28)) {
        cap = (u32)v;
      }
    }
  }
  return cap;
}

fn AtpState *thvm_atp_init(const KboConfig *cfg, u32 step_cap) {
  AtpState *s = (AtpState *)calloc(1, sizeof(AtpState));
  if (s == NULL) return NULL;
  // A new engine may carry a different ordering config: retire every
  // snapshot-orientation memo entry (atp_orient_memo_gt) and the
  // simpler-join rule-encode cache (ft_order.c).
  g_atp_orient_memo_gen++;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  {
    extern void thvm_ftsj_rulecache_invalidate_all(void);
    thvm_ftsj_rulecache_invalidate_all();
  }
#endif
  // Engine sessions share this TU's file-scope hand-off registers and
  // handle-keyed rename caches.  A prior session's FT spans died with
  // its arena (thvm_atp_free -> ft_destroy) and its Term handles may be
  // recycled by this session, so retire everything that could smuggle a
  // dead pointer across: bump the handle-cache epoch (the walk former's
  // Term rename caches and the overlap witness key on it -- a bump is a
  // cold cache, recomputed byte-identically) and DISARM the bracket
  // registers (drop, never free: the owning arena already reclaimed the
  // cells wholesale).  The walk former's span-keyed park/rename caches
  // reset themselves on AtpFt.session change (sw_rename_ft).
  g_atp_gc_epoch++;
#ifdef THVM_ATPFT_RULES
  g_pop_ft_l  = NULL;  g_pop_ft_r  = NULL;
  g_rule_ft_l = NULL;  g_rule_ft_r = NULL;
  g_lazy_rule.span_l = NULL; g_lazy_rule.span_r = NULL;
  g_lazy_rule.sent_l = 0;    g_lazy_rule.sent_r = 0;
  g_lazy_rule.armed  = 0u;
  g_mnf_pend_ft_l = NULL; g_mnf_pend_ft_r = NULL;
  g_enq_ft_l      = NULL; g_enq_ft_r      = NULL;
  g_trace_eq_ft_l = NULL; g_trace_eq_ft_r = NULL;
#endif
#ifdef THVM_ATPFT_NORM
  g_vcp_treat.fl = NULL; g_vcp_treat.fr = NULL;
#endif
  g_vcp_treat.armed     = 0u;
  g_vcp_treat.raw_len   = 0u;
  g_acp_fuse.valid      = 0u;
  g_acp_fuse.twee_valid = 0u;
  // Cross-call hygiene: a prior thvm_atp_free released the previous
  // atp's rule_index / cp_index, but the globals g_atp_ri_ix /
  // g_atp_cp_ix still point at the freed memory.  Reading them before
  // the new state's lazily-built indexes are installed (which happens
  // in atp_rewrite_normalize_indexed / generate_cps) returns a stale
  // pointer; on a TimedOut->next-call sequence that landed reads of
  // freed AtpRuleIndex memory and a macOS-malloc SIGTRAP.  NULL them
  // on init so any unguarded read crashes loudly instead of silently.
#ifdef ATP_RULE_INDEX
  g_atp_ri_ix      = NULL;
  g_atp_ri_flat    = NULL;
  g_atp_ri_subsz   = NULL;
  g_atp_ri_flatsym = NULL;
  g_atp_ri_lhs     = NULL;
  g_atp_ri_rhs     = NULL;
  g_atp_cp_ix      = NULL;
  g_atp_cp_qflat   = NULL;
  g_atp_cp_qsubsz  = NULL;
  g_atp_cp_qflatsym= NULL;
#endif
  // Defensive companion NULL'ing for the AtpFvIndex globals the
  // discrim-tree-fallback FV index path keeps pointing at: a
  // companion ri_ix/cp_ix hygiene pattern.  Harmless when nothing
  // else clobbers them; prevents the same dangling-pointer class
  // of bug as the ri_ix/cp_ix entries above.
#ifdef ATP_FV_INDEX
  g_atp_dt_ix      = NULL;
  g_atp_dt_subsz   = NULL;
  g_atp_dt_flatsym = NULL;
#endif
  // Persistent LPO memo: a completion compares the same subterm pairs
  // millions of times.  Opt in, and drop any entries from a prior run
  // (a static LpoConfig pointer may be reused with new precedence).
  thvm_lpo_set_persist(1u);
  thvm_lpo_invalidate();
  // Persistent per-term KBO weight memo: kbo_lin_addto re-walks the same
  // divergent subtrees millions of times during a completion.  Opt in and
  // drop any entries from a prior run (cfg pointer may be reused).
  thvm_kbo_set_persist(1u);
  thvm_kbo_invalidate();
  // Stage 8: drop the structural-hash orient cache too -- a fresh state
  // may reuse Term cell ids with new precedence/operator content.
  ATP_ORIENT_CACHE_INVAL();
  // Persistent unf_memo / norm-cache: an init may inherit stale entries
  // keyed by Terms reused across distinct atp states (e.g. test harness
  // builds two AtpStates with different rule sets but shared Term IDs
  // when the cell allocator recycles slots).  Bump both epochs (incl.
  // the step epoch) so no survivor from a prior atp state's
  // unorient_index is keyed in -- s is fresh, r_revision == 0 isn't
  // proof the unorient_index hasn't changed since the last invalidate.
  atp_unf_memo_invalidate(s->r_revision, 1u);
#ifdef THVM_ATPFT_NORM
  // Persistent FT normal-form memo (g_ftnfm): structure-keyed and reduction-
  // ordering INDEPENDENT, only gen-bumped on FT-tree rebuild -- so a fresh
  // AtpState inherits a prior run's cached NFs.  Across runs with different
  // orderings (a portfolio's LPO configs vs a following Waldmeister KBO run)
  // the same structural key denotes a DIFFERENT true normal form, so a
  // survivor hands the new run a WRONG NF -> a non-decreasing order-gated
  // rewrite that never joins -> the cross-run hang in
  // docs/plans/atp_heap_leak.md (init already drops LPO/KBO/orient/unf above;
  // this closes the one loc/structure-keyed memo it missed).  Byte-neutral on
  // a single run (the memo starts empty; only the gen's starting value moves).
  ftnfm_bump_gen();
#endif
  s->kbo      = cfg;
  s->step_cap = step_cap;
  // CP-priority weight: the ordering-directed GT heuristic is the
  // engine default -- it cuts the corpus saturation step count and
  // proves the distance-1 CriticalPairLemma cpl1 in a single step
  // (the symbol-count ADD heuristic took 21).  ADD stays reachable
  // via thvm_atp_set_cp_weight_mode for callers that want the bare
  // symbol-count sum.
  s->cp_weight_mode = ATP_CP_WEIGHT_GT;
  // Right-reduction (DISCOUNT-loop composition) is on by default:
  // interreduce keeps surviving rules' RHSs in normal form so the
  // CPs born from them stay small.  calloc zeroed it; set explicitly.
  s->right_reduce = 1u;
  // Eager interreduce-time orphan sweep is on by default (the historical
  // ATP_ORPHAN_KILL behavior).  Method->"Waldmeister" gates it OFF and
  // relies on the lazy at-pop discard alone (use_orphan_murder), matching
  // WM's -ocrit live-queue composition.
  s->use_eager_orphan_sweep = 1u;
  // Push-time queue-vs-queue subsumption is on by default (the
  // historical thvm engine).  No WM counterpart -- the "Waldmeister"*
  // presets gate it OFF so the passive queue and its FIFO ages match
  // WM's recentCPinsert exactly (see AtpState.use_queue_subsume).
  s->use_queue_subsume = 1u;
  // WolframAxioms-seed liveness cache sentinel (r_revision starts at 0, so a
  // zeroed cache_revision would spuriously match before the first real scan).
  // use_lazy_normalize stays OFF by default (calloc zeroed): the deferred-
  // selection CP normalization is an opt-in via thvm_atp_set_use_lazy_normalize
  // (Method suboption "LazyNormalize" on the WL side, wired through the
  // structure-aware tuner where appropriate).  It is a strict throughput win
  // on the deep Waldmeister/Completion path but an inert/negative shift on
  // easy proofs (it changes the trajectory whether or not it helps), so
  // engine-wide default-on caused a measurable atp.wlt slowdown.
  atp_register_primitives();
  acp_selftest();   // verify the Stringterms pack/unpack round-trip
  // Waldmeister `SO_minimaleKonstante` (SymbolOperationen.c:121): cache
  // the engine-reserved 0-arity CTR at ATP_RESERVED_LABEL_MIN_CONST so
  // the FVI hook in `thvm_atp_orient_and_add` can substitute it for
  // free variables in unorientable equations.  The WL encoder
  // (`encodeAtpTermInit` in ATP.wl) pre-seeds its symbol table with
  // this label; max_label flowing through thvmlink_atp.c already
  // covers it.  Cheap: one heap cell allocated at init, reused for
  // every FVI emission.
  s->min_const = term_new_ctr(ATP_RESERVED_LABEL_MIN_CONST, NULL, 0);
  // FVI rule emission (Waldmeister `RechtsUnfreiErzeugen`, default OFF):
  // env knob THVM_ATP_FVI=1 turns it on for CLI bench runs.  WL side
  // calls thvm_atp_set_use_fvi when Method "FreeVarInstance" -> True.
  s->use_fvi = (u8)atp_env_on("THVM_ATP_FVI");
  // WM backward ground-joinability sterilization (-gj, default OFF in
  // WM's CLI -- RUN/Parameter.c:317 -- and so here): env knob
  // THVM_ATP_BWD_GROUND_JOIN=1 for CLI runs; WL side calls
  // thvm_atp_set_use_bwd_ground_join on Method "BackwardGroundJoin".
  s->use_bwd_ground_join = (u8)atp_env_on("THVM_ATP_BWD_GROUND_JOIN");
  // Twee interreduce1 rule-delete port (opt-in; diverges from WM).
  s->use_gj_delete_rule = (u8)atp_env_on("THVM_ATP_GJ_DELETE_RULE");
  // Twee full-R interreduction port (opt-in; diverges from WM).
  s->use_full_interreduce = (u8)atp_env_on("THVM_ATP_FULL_INTERREDUCE");
  // Encompassment demodulation (Twee reducesSkolem; see thvm.h).
  s->use_encompass_demod = (u8)atp_env_on("THVM_ATP_ENCOMPASS");
  // Simpler-join CP discipline (Twee Join.hs allSteps; see thvm.h).
  // Value 1 (--simpler-join): generation-time treatment AND selection.
  // Value 2 (--simpler-join=2): selection only -- Twee's own placement
  // (consider runs at dequeue), skipping the per-generated-CP push tax.
  {
    const char *e = getenv("THVM_ATP_SIMPLER_JOIN");
    s->use_simpler_join = 0u;
    if (e != NULL && e[0] != '\0' && e[0] != '0') {
      s->use_simpler_join = (e[0] == '2') ? 2u : 1u;
    }
  }
  // Twee pop-time ground-joinability case-split (Join.hs groundJoin;
  // see thvm.h).  THVM_ATP_POP_GJ=1 / --pop-gj for CLI runs; the limit
  // mirrors Twee --ground-joining-limit (0 = unlimited, Twee default).
  s->use_pop_gj = (u8)atp_env_on("THVM_ATP_POP_GJ");
  {
    const char *e = getenv("THVM_ATP_POP_GJ_LIMIT");
    s->pop_gj_limit = 0u;
    if (e != NULL && e[0] != '\0') {
      long v = strtol(e, NULL, 10);
      s->pop_gj_limit = (v > 0) ? (u32)v : 0u;
    }
  }
  // Goal-narrow endgame (ground CC + goal-cone e-matching; opt-in,
  // see src/atp/goal_narrow.c).  THVM_ATP_GOAL_NARROW=1 for CLI runs;
  // WL callers use thvm_atp_set_use_goal_narrow.
  s->use_goal_narrow = (u8)atp_env_on("THVM_ATP_GOAL_NARROW");
  s->gnarrow = NULL;
  // Critical-goal queue (best-first existential narrowing; opt-in,
  // see the lane at the end of this file).  THVM_ATP_NARROW_QUEUE for
  // CLI runs; WL callers use thvm_atp_set_use_narrow_queue.
  s->use_narrow_queue = (u8)atp_env_on("THVM_ATP_NARROW_QUEUE");
  s->cgq = NULL;
  // Refutational goal driver (Phase 2; opt-in, see atp_goal_drive_*
  // at the end of this file).  THVM_ATP_GOAL_DRIVE for CLI runs; WL
  // callers use thvm_atp_set_use_goal_drive.
  s->use_goal_drive = (u8)atp_env_on("THVM_ATP_GOAL_DRIVE");
  s->gdrive = NULL;
  // Twee pop-join base discipline (opt-in; diverges from WM).  Twee has
  // NO ungated R+E join anywhere on the CP path: dequeue simplification
  // is oriented-rules-only (Rule.hs `simplify` via simplifyOverlap) and
  // every unorientable-equation rewrite in the join verdict runs behind
  // the `simplerThan` gate (Join.hs step1/step2 `ok`).  WM's pop
  // treatment (`-ks "r:e:..."`) instead joins with the UNGATED doR+doE
  // normalize -- on comm-heavy theories that deletes proof-critical
  // lemmas Twee keeps as rules: RNG021-6's L25
  //   add(associator(X,Y,Z), m(X,m(Y,Z))) = m(m(X,Y),Z)
  // AC-joins under the fixed KBO through the (larger) associator
  // definition, which Twee's gate excludes (def !simplerThan L25).
  // With the flag set the pop-time BASE normalize is rules-only; the
  // unorientable tree participates only through the gated simpler-join
  // pass (atp_cp_sj_joinable_ft) -- compose with --simpler-join for the
  // full Twee verdict.  The CP-set-ir sweep follows the same discipline
  // (Twee simplifyPassive is oriented-only).
  s->use_pop_rules_only = (u8)atp_env_on("THVM_ATP_POP_RULES_ONLY");
  // firstdiv-11 probe (THVM_ATP_KEEP_LHS_REDUCIBLE): keep an LHS-reducible
  // oriented rule live instead of dropping it at the always-on interreduce
  // site (_.c:13066), to test whether thvm dropping -auto rule 3 is what
  // ranks the rule-11 carrier late.  Default OFF = byte-identical.
  s->keep_lhs_reducible = (u8)atp_env_on("THVM_ATP_KEEP_LHS_REDUCIBLE");
  // GJ-driver victim exclusion + root protection: inactive unless a
  // fact-level GZ test is in flight (the forward CP-drop path never
  // sets them, staying byte-identical).
  s->gj_exclude = ATP_GJ_NO_EXCLUDE;
#ifdef THVM_ATPFT_RULES
  // Stage 4: stand up the AtpFt slab pool BEFORE atp_ensure_rule_cap
  // grows the parallel slot arrays (those start NULL; cells get
  // allocated lazily from this arena in atp_push_rule).  One arena
  // per AtpState, freed wholesale in thvm_atp_free.
  s->ft_arena_ptr = (struct AtpFt *)malloc(sizeof(AtpFt));
  if (s->ft_arena_ptr == NULL) {
    fprintf(stderr, "thvm_atp_init: ft_arena malloc failed\n");
    exit(1);
  }
  ft_init((AtpFt *)s->ft_arena_ptr);
#endif
  // Allocate the growable rule / CP arrays at their initial
  // capacity.  ensure_*_cap fills the trace slots with
  // ATP_TRACE_NONE (0 is a valid trace index, so explicit fill
  // is required); a fresh array starts with r_cap == 0 so the
  // helper treats the whole span as new.
  atp_ensure_rule_cap(s, ATP_INIT_RULES);
  atp_ensure_cp_cap(s, ATP_INIT_CPS);
  // Proof-trace soft cap.  Default ATP_MAX_TRACE keeps the drop
  // behavior byte-identical; THVM_ATP_TRACE_MAX raises it so a deep
  // (1601-rule) completion can still record every rule's lineage.  The
  // trace buffer itself starts NULL and grows on demand (atp_trace_ensure)
  // up to t_max; thvm_atp_trace_cap resolves and bounds the env value.
  s->t_max = thvm_atp_trace_cap();
#ifdef ATP_FV_INDEX
  // 7d: an empty FV subsumption index; CPs enter it on enqueue.
  s->fv_index = atp_fv_index_new();
#endif
#ifdef ATP_RULE_INDEX
  // 7e lever 2: the rule-LHS redex index is built lazily on the first
  // indexed normalize -- start it NULL, dirty so the first build
  // fires.  (calloc already zeroed both, the explicit set documents
  // intent.)
  s->rule_index       = NULL;
  s->gj_rule_index    = NULL;     // lazily built for the indexed GJ rewrite
  s->unorient_index   = NULL;     // lazily built on the flatterm path
  s->cp_index         = NULL;     // lazily built on the first indexed CP gen
  s->cp_subindex      = NULL;     // companion subterm index (old i x new j)
  s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
  // Opt-in CP-generation overlap-partner index.  OFF unless
  // THVM_ATP_CP_INDEX is set non-"0"; the default engine scans all
  // n_rules per new rule (byte-identical CP set either way).
  s->use_cp_index = (u8)atp_env_on("THVM_ATP_CP_INDEX");
  // Opt-in flatterm fast-path for the mixed normalize loop.  OFF unless
  // THVM_ATP_FLATTERM is set to a non-"0" value -- the default engine
  // stays byte-identical (the tree mixed loop).
  s->use_flatterm = (u8)atp_env_on("THVM_ATP_FLATTERM");
  s->trace_pack = (u8)atp_env_on("THVM_ATP_TRACE_PACK");
  s->cp_trace_lean = (u8)atp_env_on("THVM_ATP_CP_TRACE_LEAN");
  s->use_wm_interred = (u8)atp_env_on("THVM_ATP_WM_INTERRED");
  // Opt-in faithful Waldmeister-FPA normalize path.  OFF unless
  // THVM_ATP_WMFPA is set to a non-"0" value -- default stays byte-
  // identical to the discrimination-tree indexed path.
  s->use_wmfpa    = (u8)atp_env_on("THVM_ATP_WMFPA");
  s->wmfpa_check  = (u8)atp_env_on("THVM_ATP_WMFPA_CHECK");
  // Incremental resume for the flatterm unorientable scan.  ON by default
  // (mirrors the orientable `clean_before` resume); cleared only by setting
  // THVM_ATP_UNORIENT_RESUME=0, used by the resume-ON==OFF differential.
  s->ft_unorient_resume = (u8)atp_env_off("THVM_ATP_UNORIENT_RESUME");
#endif
  // Iter 133 (workflow plan step 3): the unorientable-side rewrite step
  // has a discrimination-tree retrieval (atp_unorient_step_indexed) that
  // is byte-identical-redex with the linear O(n_rules) scan but skips it
  // when the index can prune.  Replaces a per-position O(n_rules) thvm_match
  // sweep that dominates Sheffer/AndAssoc completion.  Default ON; set
  // THVM_ATP_UNORIENT_INDEX=0 to opt out for A/B.
  s->use_unorient_index = (u8)atp_env_off("THVM_ATP_UNORIENT_INDEX");
  // Opt-in Vampire Limited Resource Strategy.  OFF unless THVM_ATP_LRS is
  // set to a non-"0" value, or thvm_atp_set_use_lrs flips it on later (the
  // WL Method -> {... "LRS" -> True} surface does the latter).  Sound only
  // when a wall deadline is also set; select_cp checks both gates.
  s->use_lrs = (u8)atp_env_on("THVM_ATP_LRS");
  // LRS knobs (named so the code is auditable, not magic numbers):
  //   warmup  = 256 selections before the first horizon is computed
  //             (Riazanov & Voronkov suggest a brief warmup so the
  //             selection rate stabilizes before the predicted-remaining
  //             count drives a horizon).
  //   period  = 128 selections between horizon recomputes (amortizes the
  //             O(n_queue) heap walk -- about one recompute per ~1ms on
  //             andassoc-scale runs, two orders of magnitude under the
  //             default deadline).
  s->lrs_warmup_selections = 256u;
  s->lrs_recompute_period  = 128u;
  // ENIGMA Tier 2 GNN re-rank period.  OFF (0) unless THVM_ATP_GNN_RERANK_PERIOD
  // names a positive count, or thvm_atp_set_gnn_rerank_period sets it later
  // (the WL "RerankPeriod" surface does the latter).  Only fires while a GNN
  // model is loaded, so an unset model leaves the step path byte-identical.
  {
    const char *e = getenv("THVM_ATP_GNN_RERANK_PERIOD");
    s->gnn_rerank_period = (e != NULL) ? (u32)strtoul(e, NULL, 10) : 0u;
    // THVM_ATP_GNN_COOP_RATIO routes the GNN re-rank to the secondary
    // (cp_pri2) coop dimension at this ratio, so the GNN guides selection
    // alongside the primary preset (e.g. Waldmeister) on the normal atomic
    // run -- the WL surface sets it before TFindProof, no bridge change.
    const char *c = getenv("THVM_ATP_GNN_COOP_RATIO");
    if (c != NULL) {
      u32 ratio = (u32)strtoul(c, NULL, 10);
      if (ratio > 0u) thvm_atp_set_gnn_coop(s, ratio);
    }
    // THVM_ATP_GNN_TOPK caps how many CPs each re-rank GNN-scores (the
    // parental pre-filter); default 512, 0 = whole queue.  Only consulted
    // when a re-rank fires, so it never touches the default engine.
    const char *tk = getenv("THVM_ATP_GNN_TOPK");
    s->gnn_topk = (tk != NULL && tk[0] != '\0') ? (u32)strtoul(tk, NULL, 10)
                                                : 512u;
  }
  return s;
}

#ifdef ATP_MNF
static void mnf_destroy(struct AtpMnf *m);   // defined with the MNF module below
static void mnf_notify_fact(struct AtpMnf *m, Term l, Term r, u8 orient);
static u8   mnf_in_sweep_mode(struct AtpMnf *m);
// GC support: the MNF coloured nodes hold reached Terms on the heap, so
// they are collector roots.  Defined with the MNF module; declared here
// for thvm_atp_gc_collect (which precedes the module).
static u32  mnf_gc_count(struct AtpMnf *m);
static void mnf_gc_gather(struct AtpMnf *m, Term *roots, u32 *w);
static void mnf_gc_writeback(struct AtpMnf *m, const Term *roots, u32 base);
#endif

#ifdef ATP_ORDERED_REWRITE
// Goal-narrow endgame (src/atp/goal_narrow.c, included before
// thvm_atp_goal_check).  Its node table holds heap Terms -- collector
// roots, same discipline as the MNF nodes above.
static void gn_free(struct AtpGoalNarrow *g);
static u32  gn_gc_count(struct AtpGoalNarrow *g);
static void gn_gc_gather(struct AtpGoalNarrow *g, Term *roots, u32 *w);
static void gn_gc_writeback(struct AtpGoalNarrow *g, const Term *roots,
                            u32 base);
#endif

fn void thvm_atp_free(AtpState *s) {
  if (s == NULL) return;
  atp_wmo_free((struct AtpWmOrder *)s->wmo);
  free(s->trace);
  free(s->lhs);
  free(s->rhs);
  // Multi-goal conjunct arrays (NULL until thvm_atp_set_goals).
  free(s->goals_lhs);
  free(s->goals_rhs);
  free(s->goals_lhs_nf);
  free(s->goals_rhs_nf);
  free(s->r_trace);
  free(s->r_orient);
  free(s->r_dead);
  free(s->r_dead_lhs_save);
  free(s->r_dead_rhs_save);
  free(s->r_dead_subsumer_lhs);
  free(s->r_dead_subsumer_rhs);
  free(s->r_rederive_cut);
  free(s->r_gj_status);
  free(s->r_trace_dead);
#ifdef THVM_ATPFT_RULES
  // Stage 4: release the parallel pointer arrays + drop the slab pool
  // (one ft_destroy frees every AtpFtCell handed out in this state's
  // lifetime; the slot arrays themselves are plain malloc'd pointer
  // tables).
  free(s->lhs_ft);
  free(s->rhs_ft);
  free(s->r_dead_lhs_save_ft);
  free(s->r_dead_rhs_save_ft);
  free(s->r_dead_subsumer_lhs_ft);
  free(s->r_dead_subsumer_rhs_ft);
# ifdef THVM_ATPFT_CPQ
  // Stage 7: drop the parallel CP queue array.  The FT spans inside
  // each entry are slab-backed -- ft_destroy below releases the whole
  // slab pool in one pass, so we skip the per-slot ft_free_span walk
  // (it would push onto a free list that's about to be destroyed).
  if (s->cp_packed_ft != NULL) {
    free((void *)s->cp_packed_ft);
    s->cp_packed_ft = NULL;
  }
# endif
  if (s->ft_arena_ptr != NULL) {
    ft_destroy((AtpFt *)s->ft_arena_ptr);
    free(s->ft_arena_ptr);
    s->ft_arena_ptr = NULL;
  }
#endif
  // Each cp_packed[] slot is a malloc'd byte string the queue owns;
  // free every non-NULL slot (free(NULL) is a no-op) then the array.
  if (s->cp_packed != NULL) {
    for (u32 i = 0; i < s->cp_cap; i++) free(s->cp_packed[i]);
    free(s->cp_packed);
  }
  free(s->cp_trace);
  free(s->cp_kind);
  free(s->cp_pri);
  free(s->cp_goal);
  free(s->cp_sine);
  free(s->s2a_ax_mask);
  free(s->cp_seq);
  free(s->cp_depth);
  free(s->cp_gd);
  free(s->cp_pri2);
  free(s->cp_heap);
  free(s->cp_pos);
  free(s->cp_ultimate);
  free(s->cp_uid_a);
  free(s->cp_uid_b);
  free(s->r_uid);
  free(s->r_depth);
  free(s->r_gd);
  free(s->uid_dead);
  free(s->uid_idx);
  free(s->cp_last_norm_r_revision);
  // Deferred-CP (`implicit_pair`) arc commit 1: the descriptor array and
  // the per-slot tag bitset are plain malloc'd blocks (no per-slot owned
  // resource), so a single free per array is enough.  free(NULL) is a
  // no-op -- handles the flag-OFF case where both pointers stay NULL.
  free(s->cp_implicit);
  free(s->cp_is_implicit);
  // Raw-desc face-pair intern table (entries alias arena spans -- no
  // per-entry resource).
  free(s->rdesc_faces);
  free(s->rdesc_hash);
  // Auto-MaxWeight overflow stash: each packed byte string is owned
  // here too (free(NULL) is a no-op for slots already drained).
  if (s->cp_stash_packed != NULL) {
    for (u32 i = 0; i < s->n_cp_stash; i++) free(s->cp_stash_packed[i]);
    free(s->cp_stash_packed);
  }
  free(s->cp_stash_trace);
  free(s->cp_stash_nodes);
  free(s->cp_stash_ultimate);
  free(s->cp_stash_kind);
  // IR-victim buffer (use_wm_demote; NULL when the flag never fired).
  free(s->irv_lhs);
  free(s->irv_rhs);
  free(s->irv_parent);
  free(s->irv_wmo_key);
#ifdef THVM_ATPFT_RULES
  // Victim span slots (cells themselves are arena-owned).
  free(s->irv_lhs_ft);
  free(s->irv_rhs_ft);
#endif
  // ENIGMA training-data arrays (NULL unless recording was enabled).
  free(s->cp_feat_rows);
  free(s->cp_feat_trace);
  free(s->cp_feat_label);
  free(s->cp_graph_arena);
  free(s->cp_graph_off);
  free(s->cp_graph_scratch);
#ifdef ATP_FV_INDEX
  atp_fv_index_free(s->fv_index);
#endif
#ifdef ATP_RULE_INDEX
  atp_ri_free(s->rule_index);
  atp_ri_free(s->gj_rule_index);
  atp_ri_free(s->unorient_index);
  atp_ri_free(s->cp_index);
  atp_ri_free(s->cp_subindex);
  if (s->wmfpa_tree != NULL) {
    WfEngCache *wc = (WfEngCache *)s->wmfpa_tree;
    wf_tree_free(&wc->tree);
    free(wc->rules); free(wc->lhs_buf); free(wc->rhs_buf);
    free(wc);
    s->wmfpa_tree = NULL;
  }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // WM-policy FT mirror: drop the dying state's ownership so a future
  // AtpState reallocated at the same address (with a coincidentally
  // matching revision/count) can never validate against stale content.
  // Buffers are reused across states, exactly like g_ftpi.
  atp_wf_mm_disown(s);
#endif
#ifdef ATP_MNF
  mnf_destroy(s->mnf);
#endif
#ifdef ATP_ORDERED_REWRITE
  gn_free(s->gnarrow);
#endif
  atp_cgq_free(s->cgq);
  atp_goal_drive_free(s->gdrive);
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
  if (checkpoint <= HEAP_NEXT) {
    if (checkpoint < HEAP_NEXT) {
      // Popping the bump pointer recycles every heap loc in
      // [checkpoint, HEAP_NEXT): the next allocation reuses those loc
      // integers for new content.  The KBO weight memo (g_kbo_wmemo) is
      // keyed by (epoch, Term loc) and assumes a loc denotes one logical
      // term per epoch -- exactly the invariant a GC relocation breaks,
      // which is why thvm_atp_gc_collect invalidates the memo.  A reset
      // recycles locs the same way WITHOUT moving cells, so a memo entry
      // for a popped loc would survive and return the OLD term's weight
      // for the NEW term reallocated at that loc -- a wrong KBO verdict
      // that flips an order-gated rewrite (observed: the flatterm mixed
      // path's unorientable order-gate firing a non-decreasing step,
      // diverging from the tree NF).  Bump the epoch so every entry for a
      // recycled loc is stale and recomputed.
      thvm_kbo_invalidate();
    }
    HEAP_NEXT = checkpoint;
  }
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
//   - goal_lhs, goal_rhs                      -- the conjecture
//   - goals_lhs/rhs[/..._nf][0..n_goals)      -- multi-goal conjuncts
//   - trace[0..n_trace)                       -- TAG_CTR entries
//                                                (each holds lhs/rhs)
//   - witness_subst.bindings[0..REWRITE_MAX_VAR) -- narrowing σ
//
// The CP queue is NOT rooted: after the Waldmeister Stringterms port
// each queued CP is a packed byte string in plain malloc memory
// (cp_packed[]), outside the managed heap, so the collector never
// touches it.  This is the structural fix for the late-game GC wall.
// The subsumption-index records borrow those byte strings (u8 *, not
// Term), so the index needs no rooting either.
//
// Returns 1 if a collection ran, 0 if GC is disabled / no state.
fn u8 thvm_atp_gc_collect(AtpState *s) {
  if (s == NULL || !gc_enabled()) return 0;
  u64 gc_t0 = atp_now_us();

  // needle (THVM_ATP_DRC_NEEDLE): fold_def_body[0..fold_width) and
  // needle_ax_l/r[0..n_needle_ax) are long-lived Term arrays (set once
  // at problem intake, read by every DRC keep decision thereafter), so
  // every GC must relocate them exactly like the rule set.  fold_width
  // is clamped defensively even though thvm_atp_set_fold_band /
  // thvm_atp_set_fold_defs never exceed ATP_NEEDLE_MAX_DEFS together.
  u32 needle_fold_width = (s->fold_hi > s->fold_lo)
                           ? (s->fold_hi - s->fold_lo) : 0u;
  if (needle_fold_width > ATP_NEEDLE_MAX_DEFS) needle_fold_width = ATP_NEEDLE_MAX_DEFS;
  // Count the root slots so we can size the array exactly.
  // 2 per rule for lhs/rhs, plus 2 per rule for dead-save (originals
  // are kept live so proof reconstruction can read them when the slot
  // has been soft-deleted by backward subsumption).  +1 for
  // s->min_const (the FVI reserved constant; allocated once in
  // thvm_atp_init and reused across orient_and_add calls, so a GC
  // mid-completion would otherwise leave a dangling cell).
  u32 n_roots = 6u * s->n_rules /* lhs,rhs,dead l/r save,subsumer l/r */
              + 4u /* goal + goal_nf */
              + 4u * s->n_goals /* multi-goal conjuncts + NFs */
              + s->n_trace + REWRITE_MAX_VAR
              + 1u /* min_const */
              + 2u * s->n_irv /* buffered IR victims (use_wm_demote) */
              + needle_fold_width + 2u * s->n_needle_ax /* needle */
              + (s->gc_extra_roots != NULL ? s->n_gc_extra_roots : 0u);
#ifdef ATP_MNF
  // Milestone 10: every MNF coloured node holds a reached Term.  Root
  // them so the collector relocates them; the hash table (structural
  // hashes, node indices) is GC-invariant and needs no fixup.
  n_roots += mnf_gc_count(s->mnf);
#endif
#ifdef ATP_ORDERED_REWRITE
  // Goal-narrow E-graph nodes each hold a representative Term.  The
  // congruence machinery (labels, kid ids, union-find, signature
  // hashes over class ids) is GC-invariant and needs no fixup.
  n_roots += gn_gc_count(s->gnarrow);
#endif
  // Critical-goal queue: two Terms per stored CG.
  n_roots += cgq_gc_count(s->cgq);
  Term *roots = (Term *)malloc((size_t)n_roots * sizeof(Term));
  if (roots == NULL) return 0;

  u32 w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    roots[w++] = s->lhs[i];
    roots[w++] = s->rhs[i];
  }
  // Dead-rule originals are appended after the live-rule block so the
  // collector relocates them too.  0 (unused slot for never-dead rules)
  // is treated as a null root by gc_collect.
  for (u32 i = 0; i < s->n_rules; i++) {
    roots[w++] = s->r_dead_lhs_save[i];
    roots[w++] = s->r_dead_rhs_save[i];
    roots[w++] = s->r_dead_subsumer_lhs[i];
    roots[w++] = s->r_dead_subsumer_rhs[i];
  }
  roots[w++] = s->goal_lhs;
  roots[w++] = s->goal_rhs;
  roots[w++] = s->goal_lhs_nf;
  roots[w++] = s->goal_rhs_nf;
  // Multi-goal conjuncts.  goals_lhs[0] usually VALUE-aliases goal_lhs
  // (same cell); the collector's forwarding pointers relocate both
  // copies to the same to-space address, so duplicate roots are safe
  // and cost no extra copying.  0 entries (unset NFs) are null roots.
  for (u32 g = 0; g < s->n_goals; g++) {
    roots[w++] = s->goals_lhs[g];
    roots[w++] = s->goals_rhs[g];
    roots[w++] = s->goals_lhs_nf[g];
    roots[w++] = s->goals_rhs_nf[g];
  }
  for (u32 i = 0; i < s->n_trace; i++) roots[w++] = s->trace[i];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    roots[w++] = s->witness_subst.bindings[i];
  }
  // FVI reserved constant: cached in init, used in orient_and_add.
  // Must be relocated by every GC so the cached Term stays valid.
  u32 min_const_root = w;
  roots[w++] = s->min_const;
  // Buffered IR victims (use_wm_demote): live only between interreduce
  // and the post-CP-gen drain, but the heap-pressure GC can run inside
  // generate_cps in exactly that window.
  for (u32 i = 0; i < s->n_irv; i++) {
    roots[w++] = s->irv_lhs[i];
    roots[w++] = s->irv_rhs[i];
  }
  // needle (THVM_ATP_DRC_NEEDLE): fold-band definition bodies + the
  // pre-tgt axiom list (see needle_fold_width above).  0 entries when
  // needle was never armed for this problem -- null roots, same as
  // every other unset-slot loop here.
  u32 needle_root = w;
  for (u32 i = 0; i < needle_fold_width; i++) roots[w++] = s->fold_def_body[i];
  for (u32 i = 0; i < s->n_needle_ax; i++) {
    roots[w++] = s->needle_ax_l[i];
    roots[w++] = s->needle_ax_r[i];
  }
  // Caller-owned extra roots (see the thvm.h field comment): evacuated
  // like every other root and written back in place below, so the
  // caller's handles stay live across the collection.  NULL/0 keeps the
  // historical root set byte-identical.
  u32 extra_root_base = w;
  if (s->gc_extra_roots != NULL) {
    for (u32 i = 0; i < s->n_gc_extra_roots; i++) {
      roots[w++] = s->gc_extra_roots[i];
    }
  }
#ifdef ATP_MNF
  u32 mnf_node_root = w;
  mnf_gc_gather(s->mnf, roots, &w);
#endif
#ifdef ATP_ORDERED_REWRITE
  u32 gn_node_root = w;
  gn_gc_gather(s->gnarrow, roots, &w);
#endif
  u32 cgq_node_root = w;
  cgq_gc_gather(s->cgq, roots, &w);

  gc_collect(roots, w);
  g_atp_gc_epoch++;   // invalidate handle-keyed caches (cells moved)

  // Per-collect swap probe (THVM_ATP_RECYCLE_DEBUG): each gc_collect
  // swaps the Cheney semi-spaces, so after this call from_start
  // toggles between 0 (lower) and space_words (upper).  Confirms the
  // odd-swap parity a following ATP run inherits.
  { static int sw = -1;
    if (sw < 0) sw = (getenv("THVM_ATP_RECYCLE_DEBUG") != NULL) ? 1 : 0;
    if (sw)
      fprintf(stderr, "[atp-gccollect] gc_count=%llu from_start=%llu "
                      "HEAP_NEXT=%llu s=%p step=%u n_rules=%u n_cps=%u\n",
        (unsigned long long)gc_count(),
        (unsigned long long)gc_from_start(),
        (unsigned long long)HEAP_NEXT, (void *)s, s->step, s->n_rules,
        s->n_cps); fflush(stderr); }

  // DIAG: post-GC live-set breakdown (NORMLOG).  Sum the trace-term
  // node counts vs the rule-term node counts to see which dominates.
  { static int dl = -1;
    if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
    if (dl) {
      u64 rule_n = 0, trace_n = 0;
      for (u32 i = 0; i < s->n_rules; i++)
        rule_n += atp_symbol_count(roots[2u*i]) + atp_symbol_count(roots[2u*i+1u]);
      u32 tbase = 6u*s->n_rules + 4u + 4u*s->n_goals;
      for (u32 i = 0; i < s->n_trace; i++) trace_n += atp_symbol_count(roots[tbase + i]);
      fprintf(stderr, "GCLOG n_rules=%u n_trace=%u live=%lluK rule_nodes=%lluK trace_nodes=%lluK\n",
        s->n_rules, s->n_trace, (unsigned long long)((HEAP_NEXT - gc_from_start())>>10),
        (unsigned long long)(rule_n>>10), (unsigned long long)(trace_n>>10)); } }
  // Write the relocated Terms back into the AtpState in the same
  // order they were gathered.
  w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    s->lhs[i] = roots[w++];
    s->rhs[i] = roots[w++];
  }
  for (u32 i = 0; i < s->n_rules; i++) {
    s->r_dead_lhs_save[i] = roots[w++];
    s->r_dead_rhs_save[i] = roots[w++];
    s->r_dead_subsumer_lhs[i] = roots[w++];
    s->r_dead_subsumer_rhs[i] = roots[w++];
  }
  s->goal_lhs = roots[w++];
  s->goal_rhs = roots[w++];
  s->goal_lhs_nf = roots[w++];
  s->goal_rhs_nf = roots[w++];
  for (u32 g = 0; g < s->n_goals; g++) {
    s->goals_lhs[g]    = roots[w++];
    s->goals_rhs[g]    = roots[w++];
    s->goals_lhs_nf[g] = roots[w++];
    s->goals_rhs_nf[g] = roots[w++];
  }
  for (u32 i = 0; i < s->n_trace; i++) s->trace[i] = roots[w++];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    s->witness_subst.bindings[i] = roots[w++];
  }
  s->min_const = roots[w++];
  (void)min_const_root;
  for (u32 i = 0; i < s->n_irv; i++) {
    s->irv_lhs[i] = roots[w++];
    s->irv_rhs[i] = roots[w++];
  }
  (void)needle_root;
  for (u32 i = 0; i < needle_fold_width; i++) s->fold_def_body[i] = roots[w++];
  for (u32 i = 0; i < s->n_needle_ax; i++) {
    s->needle_ax_l[i] = roots[w++];
    s->needle_ax_r[i] = roots[w++];
  }
  if (s->gc_extra_roots != NULL) {
    for (u32 i = 0; i < s->n_gc_extra_roots; i++) {
      s->gc_extra_roots[i] = roots[extra_root_base + i];
    }
  }
  (void)extra_root_base;
#ifdef ATP_MNF
  // Write the relocated reached-Terms back into the MNF nodes.  No
  // hash-table fixup: mnf_hash is structural, so a relocated term
  // keeps its hash and stays in its bucket.
  mnf_gc_writeback(s->mnf, roots, mnf_node_root);
#endif
#ifdef ATP_ORDERED_REWRITE
  gn_gc_writeback(s->gnarrow, roots, gn_node_root);
#endif
  cgq_gc_writeback(s->cgq, roots, cgq_node_root);

  free(roots);

  // Cells moved: the persistent LPO (s,t)->verdict memo and the per-term
  // KBO weight memo are both keyed on cell addresses, now stale -- drop them.
  thvm_lpo_invalidate();
  thvm_kbo_invalidate();
  // Stage 8 orient cache is keyed on atp_term_struct_hash (GC-stable by
  // construction), so the entries would still be sound -- but the
  // workload changes at this boundary (rules churned, GC mid-saturation)
  // and the cache's contents are no longer representative of the next
  // phase.  Bump the epoch; cheap (one counter inc).
  ATP_ORIENT_CACHE_INVAL();
  // The normalize-result cache stores Term values (cell addresses), so
  // GC relocation makes its entries stale.  Bump the unf_memo_epoch so
  // every cached NF is invalidated.  Whole-subject + per-position
  // no-fire memos are hash-keyed (cell-relocation-safe), but we still
  // force-bump the step epoch here: GC is a coarse phase boundary
  // (workload shifts post-GC) and the rare-event cost is negligible.
  atp_unf_memo_invalidate(s->r_revision, 1u);
  g_atp_gc_us += atp_now_us() - gc_t0;
  g_atp_gc_n  += 1u;
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

// Grow the heap-allocated trace[] to hold at least one more entry,
// honoring the s->t_max soft cap.  Returns 1 if there is room for the
// next push, 0 if the cap is hit (caller returns ATP_TRACE_NONE).
static int atp_trace_ensure(AtpState *s) {
  if (s->n_trace >= s->t_max) return 0;
  if (s->n_trace < s->t_cap) return 1;
  u32 cap = s->t_cap ? s->t_cap * 2u : 4096u;
  if (cap > s->t_max) cap = s->t_max;
  Term *nt = (Term *)realloc(s->trace, (size_t)cap * sizeof(Term));
  if (nt == NULL) return 0;
  s->trace = nt;
  if (s->trace_pack) {
    u64 *no = (u64 *)realloc(s->trace_eq_off, (size_t)cap * sizeof(u64));
    if (no == NULL) return 0;
    // Zero the freshly grown tail (0 = "no packed pair").
    for (u32 k = s->t_cap; k < cap; k++) no[k] = 0u;
    s->trace_eq_off = no;
  }
  s->t_cap = cap;
  return 1;
}

// Off-heap trace-equation store (THVM_ATP_TRACE_PACK).  Append the packed
// (lhs,rhs) pair to s->trace_eq_buf and return its byte offset + 1 (so 0
// stays the "none" sentinel).  acp_pack_term is self-delimiting via arity,
// so the loader reads lhs then rhs back-to-back.
static u64 atp_trace_eq_store(AtpState *s, Term lhs, Term rhs) {
  // Pack into the shared scratch first (acp_pack_term emits there),
  // then append the exact bytes -- no 21-bytes/node bound pre-walk.
  u32 pos = 0u, nodes = 0u;
  u8 stored = 0u;
  if (g_trace_eq_nv) {
    if (g_vcp_emit.armed) {
      // FT-direct push: the armed CP's packed pair
      // already sits in the scratch (vcp_emit_fused) -- the exact
      // bytes the renumber-during-emit walk below would produce; the
      // raw>=50 class stores its raw pair == its queued pair.
      pos = g_vcp_emit.len;
      stored = 1u;
    } else if (g_vcp_treat.armed) {
      // Treated FT-direct push: emit the renumbered RAW pair bytes NOW
      // (deferred from formation -- only survivors reach this store;
      // the joined third never pays the emit).  Byte-identical to the
      // formation-time emit: same operands, same still-armed subst.
      // A stash with raw_len != 0 already carries its raw bytes at
      // g_acp_scratch[0, raw_len) -- the Term-arm treat form's eager
      // emit and the ifeq-strip emit hijack (no formation operands to
      // defer to) -- so consume them instead of re-emitting from the
      // then-unset lazy-emit operands.
      pos = g_vcp_treat.raw_len;
      if (pos == 0u) {
        pos = vcp_ft_treat_emit_raw(s);
        if (pos == 0u) return 0u;   // emit anomaly: no-pair sentinel
        g_vcp_treat.raw_len = pos;
      }
      stored = 1u;
    } else {
      // Fused push loop: the raw pair arrives un-renumbered; renumber
      // during the byte emit (bytes identical to normalize-then-pack).
      stored = acp_trace_pack_fused(lhs, rhs, &pos);
      if (!stored) {
        pos = 0u;
        thvm_normalize_vars(&lhs, &rhs);   // anomaly: classic pipeline
      }
    }
  }
  if (!stored) {
    acp_pack_term(lhs, &pos, &nodes);
    acp_pack_term(rhs, &pos, &nodes);
  }
  if (s->trace_eq_len + pos > s->trace_eq_cap) {
    u64 ncap = s->trace_eq_cap ? s->trace_eq_cap * 2u : (1u << 20);
    while (s->trace_eq_len + pos > ncap) ncap *= 2u;
    u8 *nb = (u8 *)realloc(s->trace_eq_buf, (size_t)ncap);
    if (nb == NULL) thvm_fatal("atp_trace_eq_store: OOM");
    s->trace_eq_buf = nb;
    s->trace_eq_cap = ncap;
  }
  u64 off = s->trace_eq_len;
  memcpy(s->trace_eq_buf + off, g_acp_scratch, pos);
  s->trace_eq_len = off + pos;
  return off + 1u;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span-sourced sibling of atp_trace_eq_store: pack the pair's acp bytes
// straight from spans (vcp_ft_emit_side with a fresh shared map -- on
// var-normed spans the appearance-order renumber is the identity, so
// the bytes equal acp_pack_term of the canonical pair; on raw spans it
// equals the renumber-during-emit walk atp_trace_eq_store itself uses
// for the fused push).  0 on an emit anomaly -- caller falls back to a
// Term-sourced store.
static u64 atp_trace_eq_store_ft(AtpState *s, const AtpFtCell *lhs,
                                 const AtpFtCell *rhs) {
  AcpNvMap m;
  m.n = 0u;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  if (!vcp_ft_emit_side(lhs, NULL, NULL, NULL, s->kbo, &m,
                        &pos, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (!vcp_ft_emit_side(rhs, NULL, NULL, NULL, s->kbo, &m,
                        &pos, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (s->trace_eq_len + pos > s->trace_eq_cap) {
    u64 ncap = s->trace_eq_cap ? s->trace_eq_cap * 2u : (1u << 20);
    while (s->trace_eq_len + pos > ncap) ncap *= 2u;
    u8 *nb = (u8 *)realloc(s->trace_eq_buf, (size_t)ncap);
    if (nb == NULL) thvm_fatal("atp_trace_eq_store_ft: OOM");
    s->trace_eq_buf = nb;
    s->trace_eq_cap = ncap;
  }
  u64 off = s->trace_eq_len;
  memcpy(s->trace_eq_buf + off, g_acp_scratch, pos);
  s->trace_eq_len = off + pos;
  return off + 1u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Reconstruct entry `idx`'s (lhs,rhs) from the off-heap store as live IC
// Terms.  Returns 0 if the entry has no packed pair (sentinel 0).
static int atp_trace_eq_load(AtpState *s, u32 idx, Term *lhs, Term *rhs) {
  if (!s->trace_pack || s->trace_eq_off == NULL) return 0;
  u64 o = s->trace_eq_off[idx];
  if (o == 0u) return 0;
  const u8 *p = s->trace_eq_buf + (o - 1u);
  *lhs = acp_unpack_term(&p);
  *rhs = acp_unpack_term(&p);
  return 1;
}

// Materialize the off-heap packed trace: rebuild packed entries with
// their real (lhs,rhs) loaded from the byte store, replacing the
// NUM sentinels in children[2..3].  The trace is kept PACKED during
// saturation (small live set -> no proof-trace GC-heap blowup, the
// [[project_atp_wm_trace_memory_blocker]] bottleneck) and materialized
// exactly ONCE here, after thvm_atp_run terminates, so the C proof
// extractor + the WL trace reader see live Term entries.
//
// CONE-SCOPED (read-set audited against ATP_ProofGraph.wl): the WL
// lift decodes a trace entry's (lhs, rhs) Term handles in exactly two
// ways --
//   (a) population-wide over TRACE_ORIENT / TRACE_FVI entries only
//       (aliveRulesAt + the two allRules emitNorm retries build rule
//       tables from every ORIENT/FVI ever born), and
//   (b) the resolveTrace recursion, whose roots are live-rule birth
//       entries (resolveRule -> rTrace[k]) and which otherwise only
//       moves to an already-visited entry's parents (children 0/1;
//       a NORM_STEP's child 1 is a rule's TRACE_ORIENT index).
// Reason / parents / pos ship as wire ints and never touch the term
// handles, so every OTHER entry -- the ~millions of queued-CP
// TRACE_CP records plus uncited SIMPLIFY entries -- never has its
// pair decoded, and rebuilding it into live Terms is pure waste
// (the 0.48s materialize-trace tile on OrAssociativity: 1.98M
// entries vs a ~thousands-entry read set).  Mark the read set (all
// ORIENT/FVI + all rule births + the parent closure) and materialize
// only it; unmarked entries keep their NUM sentinels, which the wire
// encoder, GC root walk, and orphan scan already handle (they touch
// only tags/handles, not the pair content).  An MNF-closed goal cites
// TRACE indices directly from the goal chain (usingMnf), so use_mnf
// keeps the historical full materialization.
// Kill switch: THVM_ATP_NO_CONE_MAT=1 -> full materialization.
//
// GC-safe: runs OUTSIDE the step loop, so no mid-loop collection moves
// the children snapshot.  One compaction up front frees dead CPs so the
// rebuilt trace fits in the from-space alongside the live set; the loop
// then only bumps (the live set + materialized trace stays well under
// the semispace, and mid-op heap_alloc never auto-collects).
fn void thvm_atp_materialize_trace(AtpState *s) {
  if (s == NULL || !s->trace_pack) return;
  if (gc_enabled()) thvm_atp_gc_collect(s);
  static int no_cone = -1;
  if (no_cone < 0) {
    const char *ev = getenv("THVM_ATP_NO_CONE_MAT");
    no_cone = (ev != NULL && ev[0] == '1') ? 1 : 0;
  }
  u8 *mark = NULL;
  if (!no_cone && !s->use_mnf && s->n_trace > 0u) {
    mark = (u8 *)calloc(s->n_trace, 1u);
    u32 *stk = (mark != NULL)
      ? (u32 *)malloc((size_t)s->n_trace * sizeof(u32)) : NULL;
    if (stk == NULL) {
      free(mark);
      mark = NULL;                      // OOM: degrade to full materialize
    } else {
      u32 sp = 0u;
      // Seed (a): every ORIENT / FVI entry (population-wide WL reads).
      for (u32 i = 0; i < s->n_trace; i++) {
        Term e = s->trace[i];
        if (term_tag(e) != TAG_CTR) continue;
        u32 reason = term_ext(e);
        if (reason != TRACE_ORIENT && reason != TRACE_FVI) continue;
        mark[i] = 1u;
        stk[sp++] = i;
      }
      // Seed (b): live-rule birth entries (resolveRule roots).
      for (u32 r = 0; r < s->n_rules; r++) {
        u32 ti = s->r_trace[r];
        if (ti != ATP_TRACE_NONE && ti < s->n_trace && !mark[ti]) {
          mark[ti] = 1u;
          stk[sp++] = ti;
        }
      }
      // Parent closure: children 0/1 of every marked entry are trace
      // indices for the 4-child reasons; a NORM_STEP's child 1 is a
      // rule's TRACE_ORIENT trace index (already seeded, followed
      // anyway -- over-marking is harmless).
      while (sp > 0u) {
        u32 ti = stk[--sp];
        Term e = s->trace[ti];
        if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) continue;
        for (u32 k = 0; k < 2u; k++) {
          u32 p = (u32)term_val(term_ctr_at(e, k));
          if (p != ATP_TRACE_NONE && p < s->n_trace && !mark[p]) {
            mark[p] = 1u;
            stk[sp++] = p;
          }
        }
      }
      free(stk);
    }
  }
  u32 n_mat = 0u;
  for (u32 i = 0; i < s->n_trace; i++) {
    if (mark != NULL && !mark[i]) continue;
    Term lhs, rhs;
    if (!atp_trace_eq_load(s, i, &lhs, &rhs)) continue;   // no packed pair
    Term e = s->trace[i];
    u32 reason = term_ext(e);
    u32 n = term_ctr_n(e);
    if (n < 4u || n > 63u) continue;                      // defensive
    Term ch[64];
    for (u32 k = 0; k < n; k++) ch[k] = term_ctr_at(e, k);
    ch[2] = lhs;
    ch[3] = rhs;
    s->trace[i] = term_new_ctr(reason, ch, n);
    s->trace_eq_off[i] = 0u;                              // now materialized
    n_mat++;
  }
  if (getenv("THVM_ATP_TIME_SPLIT") != NULL ||
      getenv("THVM_ATP_PHASETIME") != NULL) {
    fprintf(stderr, "[atp-phase] cone-mat: materialized=%u of %u (cone=%s)\n",
            n_mat, s->n_trace, mark != NULL ? "on" : "off");
  }
  free(mark);
  s->trace_pack = 0u;
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
  if (s == NULL || !atp_trace_ensure(s)) return ATP_TRACE_NONE;
  // Rule/axiom/ORIENT/SIMPLIFY entries stay MATERIALIZED (real pair on
  // heap) even under trace_pack.  These are only ~n_rules entries (the
  // millions that drive the GC live set are the CP pushes, packed in
  // atp_trace_push_cp), so keeping them real costs no meaningful RSS --
  // and it MUST be real: off-heap-packing the axioms plants NUM
  // sentinels for the pre-first-CP trace, which shifts the heap layout
  // the CP former reads and perturbs CP FORMATION ORDER from seq=0 (the
  // whole BooleanAxioms theory diverged from wmcli at the first pick
  // once TracePack became the default).  This mirrors the cp_trace_lean
  // split (rules materialized, CPs leaned), which is selection-faithful.
  u64 eq_off = 0u;
  Term c_lhs = lhs, c_rhs = rhs;
  // THVM_ATP_TRACE_LEAN_RULES=1 (experiment): pack the rule-birth pair
  // off-heap like the CP entries, sentinels in the children.  The
  // historical reason these stay materialized -- the TERM CP former
  // read the heap layout, and leaning the axioms perturbed formation
  // order from seq=0 -- may have dissolved now that formation runs on
  // spans (the FT arm); the parity suite under this env is the test.
  static int lean_rules = -1;
  if (lean_rules < 0) {
    // Default ON (see atp_lazy_rules_on); "0" is the kill switch.
    const char *ln = getenv("THVM_ATP_TRACE_LEAN_RULES");
    lean_rules = (ln == NULL || ln[0] != '0');
  }
  if (s->trace_pack && lean_rules) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (g_trace_eq_ft_l != NULL && g_trace_eq_ft_r != NULL) {
      eq_off = atp_trace_eq_store_ft(s, g_trace_eq_ft_l, g_trace_eq_ft_r);
    }
    if (eq_off == 0u)
#endif
    {
      eq_off = atp_trace_eq_store(s, lhs, rhs);
    }
    c_lhs = term_new(0, TAG_NUM, 0, 0);   // sentinel; real pair off-heap
    c_rhs = c_lhs;
  }
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  g_trace_eq_ft_l = NULL;
  g_trace_eq_ft_r = NULL;
#endif
  Term children[4] = {
    term_new(0, TAG_NUM, 0, p_a),
    term_new(0, TAG_NUM, 0, p_b),
    c_lhs,
    c_rhs,
  };
  s->trace[s->n_trace] = term_new_ctr(reason, children, 4);
  u32 idx = s->n_trace;
  if (s->trace_pack) s->trace_eq_off[idx] = eq_off;   // 0 = real pair on heap
  s->n_trace++;
  return idx;
}

// Push a TRACE_CP entry that also carries the superposition position
// -- pos[0..pos_len) is the path into parent_a's rule lhs where
// parent_b's rule lhs overlapped (CriticalPair.pos).  The entry is a
// TAG_CTR(TRACE_CP) with children
//   [NUM(p_a), NUM(p_b), lhs, rhs, NUM(pos_len), NUM(pos_0), ...,
//    NUM(combo)].
// Children 0..3 match a plain atp_trace_push entry, so the trace
// serializer, GC root walk, and orphan-kill scan -- all of which
// touch only the first four children -- are unaffected; the proof
// DAG reads the overlap geometry off children 4..4+pos_len.  The final
// child (index 5+pos_len) is the overlap face-combo index (0..3, or
// 0xff = unknown) -- the only consumer is atp_wmo_cp_combo, which the
// CP-formation side geometry (use_cp_side) reads at the orient site.
static u32 atp_trace_push_cp(AtpState *s, u32 p_a, u32 p_b,
                             Term lhs, Term rhs,
                             const u8 *pos, u8 pos_len, u8 combo) {
  if (s == NULL || !atp_trace_ensure(s)) return ATP_TRACE_NONE;
  u64 eq_off = 0u;
  Term c_lhs = lhs, c_rhs = rhs;
  if (s->trace_pack) {
    eq_off = atp_trace_eq_store(s, lhs, rhs);
    c_lhs = term_new(0, TAG_NUM, 0, 0);   // sentinel; real pair off-heap
    c_rhs = c_lhs;
  } else if (s->cp_trace_lean) {
    // Lean CP trace: plant sentinels and store the pair NOWHERE.  The
    // queued CP keeps its terms in cp_packed (selection unpacks them
    // there); the GC no longer roots the millions of CP-term pairs that
    // dominate the live set on a deep completion.  Only the CP push is
    // leaned -- rule (ORIENT/AXIOM) trace terms stay materialized.
    c_lhs = term_new(0, TAG_NUM, 0, 0);
    c_rhs = c_lhs;
  }
  Term children[6 + CP_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, p_b);
  children[2] = c_lhs;
  children[3] = c_rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5 + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len] = term_new(0, TAG_NUM, 0, combo);
  s->trace[s->n_trace] = term_new_ctr(TRACE_CP, children, 6u + pos_len);
  u32 idx = s->n_trace;
  if (s->trace_pack) s->trace_eq_off[idx] = eq_off;
  s->n_trace++;
  return idx;
}

// When a CP_TRACE_LEAN'd CP survives to become a rule/equation, restore its
// TRACE_CP entry's children 2/3 (planted as sentinels at push) to the real
// term pair, so the WL proof builder -- which reads children 2/3 for the
// CriticalPairLemma statement -- renders a valid, verifiable proof.  Only the
// ~n_rules proof-relevant CPs are re-materialized here; the millions of
// joinable CPs that never enter a proof keep their sentinels.  The pair
// stored is the added rule's PERSISTENT terms (GC-rooted via s->lhs/s->rhs),
// never the transient pop-time scratch, so no dangling across a heap reset.
// No-op unless cp_trace_lean (the push planted real terms otherwise).
static void atp_trace_cp_rematerialize(AtpState *s, u32 tid, Term lhs, Term rhs) {
  if (s == NULL || !s->cp_trace_lean) return;
  if (tid == ATP_TRACE_NONE || tid >= s->n_trace) return;
  Term old = s->trace[tid];
  if (term_tag(old) != TAG_CTR || term_ext(old) != TRACE_CP) return;
  u32 n = term_ctr_n(old);
  if (n < 6u) return;
  u8 pos_len = (u8)term_val(term_ctr_at(old, 4));
  if ((u32)(6u + pos_len) != n) return;             // layout guard
  Term children[6 + CP_MAX_DEPTH];
  children[0] = term_ctr_at(old, 0);                // p_a
  children[1] = term_ctr_at(old, 1);                // p_b
  children[2] = lhs;                                // real terms now
  children[3] = rhs;
  children[4] = term_ctr_at(old, 4);                // pos_len
  for (u8 k = 0; k < pos_len; k++) children[5 + k] = term_ctr_at(old, 5 + k);
  children[5u + pos_len] = term_ctr_at(old, 5u + pos_len);  // combo
  s->trace[tid] = term_new_ctr(TRACE_CP, children, 6u + pos_len);
}

// WM-distinguished-face flag (dist_rhs) for a newly-added unorientable
// equation selected from the CP queue at trace `src_trace`.
//
// WM's distinguished face is `selRec.lhs = KPLinks` (RUndEVerwaltung.c:
// 413, RE_ErzeugteGleichung, kept through BO_TermpaarNormieren with no
// side swap).  For a genuine two-parent superposition KPLinks =
// sigma(TP_RechteSeite(Vater)) (Unifikation1.c:916) -- the OUTER rule's
// RHS -- which thvm's CP constructor lands on cp.rhs, so WM's
// distinguished face = thvm's stored RHS: the dist_rhs=1 default.
//
// The exception is a CP that re-derives an EXISTING fact through a
// SINGLE parent (WM `ue (-X, 0)`: actualParent = -X real, otherParent
// absent).  There is no outer-rule-RHS role, so KPLinks keeps the pair's
// natural left side, which thvm also stores as cp.lhs: WM's distinguished
// face = thvm's stored LHS, dist_rhs=0.  This returns 0 ONLY for that
// single-real-parent CP; an initial axiom (WM `ue (0, 0)`, both parents
// absent, src = TRACE_AXIOM) and a genuine two-parent superposition both
// keep the dist_rhs=1 default.  The MeredithAnd/OrAssociativity pick-72
// divergence was exactly the single-parent case: WM's E-number-3 (the
// re-derived axiom 2, parents (-1, 0)) emits its CP batch from the
// natural LHS face (phase A first), but the dist_rhs=1 default flipped
// phases A<->D, sorting the partner-6 phase-A CP ahead of the partner-9
// phase-D CP and inverting their FIFO ages.
// Base relationship (no use_cp_side swap applied): is WM's
// distinguished face (KPLinks) thvm's natural stored RHS?  1 = KPLinks
// = sigma(r_i) on thvm's cp.rhs (two-parent superposition / axiom);
// 0 = KPLinks on thvm's cp.lhs (single-parent re-derivation).
static int atp_lr_sortieren_rec(Term lhs, Term rhs);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static int atp_lr_sortieren_rec_ft(const struct AtpFtCell *lhs,
                                   const struct AtpFtCell *rhs);
#endif

static u8 atp_wmo_eq_dist_rhs_base(const AtpState *s, u32 src_trace) {
  if (s == NULL) return 1u;
  if (src_trace == ATP_TRACE_NONE || src_trace >= s->n_trace) {
    // Past the proof-trace soft cap the trace entry is gone, but the
    // popped CP's origin class survives on the queue (cp_kind ->
    // last_popped_kind).  Apply the SAME class rule as the tagged
    // branches below: a superposition CP stores WM's (KPLinks,
    // KPRechts) AND a SIMPLIFY re-derivation keeps the natural left
    // side -- distinguished face = stored LHS (0) for both.  An axiom
    // never outlives the cap (pushed at init); it keeps the historical
    // default.  (WolframAxioms OrAssociativity fact 318: n_trace
    // crosses the 131072 cap near fact 300, so the trace-less default
    // walked the B/E sweep faces reversed and flipped the fact-533
    // weight tie; fact 1531, a trace-less SIMPLIFY, needed the same
    // stored-LHS verdict as the traced branch -- the A-vs-D root-twin
    // formation-order flip behind the fact-1549 kept-E side swap.)
    return (s->last_popped_kind == (u8)TRACE_CP ||
            s->last_popped_kind == (u8)TRACE_SIMPLIFY) ? 0u : 1u;
  }
  Term e = s->trace[src_trace];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) return 1u;
  u32 reason = term_ext(e);
  u32 p_a = (u32)term_val(term_ctr_at(e, 0));
  u32 p_b = (u32)term_val(term_ctr_at(e, 1));
  // A TRACE_SIMPLIFY victim (an interreduced fact re-queued as its
  // reduced equation) is WM's `ue (-X, 0)` re-derivation with a single
  // real parent (the dropped fact); a TRACE_CP with the otherParent
  // absent is the same shape from the superposition lane.  Both keep
  // the natural left side as the distinguished face -> dist_rhs=0.  A
  // genuine two-parent superposition takes the dist_rhs=1 default.
  if (reason == TRACE_SIMPLIFY) {
    // An IR-victim re-derivation keeps the natural left side as the
    // distinguished face, same as the header comment's single-parent
    // rule (MeredithAxioms AndAssociativity fact 31, a SIMPLIFY-kept E:
    // WM forms the fact-34 tie CP from the stored-LHS pattern first;
    // dist=1 walked thvm's B-phase on the stored RHS and flipped the
    // 34/36 weight tie).  A briefly-live dist=1 variant here was gated
    // only by the loose pre-RULEADD-anchored checker -- a phantom.
    return 0u;
  }
  if (reason == TRACE_CP) {
    // Post-4df4f2ab the stored raw pair is WM's (KPLinks, KPRechts): the
    // distinguished face is the stored LHS for every superposition CP.
    return 0u;
  }
  // INITIAL AXIOM: WM stores an input equation LRSortieren-canonicalised
  // (SpezNormierung.c :517-534) and U1_KPsBildenZuGleichung sweeps its
  // stored LinkeSeite first (A/B on l, then D/E on r -- no per-face
  // adjustment; Unifikation1.c :1596/:1637).  So WM's distinguished face
  // is thvm's stored RHS exactly when LRSortieren would SWAP thvm's
  // stored pair.  The former blanket dist_rhs=1 walked the wrong face
  // first whenever thvm's stored orientation already matched WM's
  // canonical one: ShefferAxioms OrAssociativity eq -2 (axiom 3,
  // (x1.(x2.x3))^2 # ((x2.x2).x1).((x3.x3).x1), stored A#B by BOTH
  // engines) had its activation sweep B-side subterms {L.1.1, L.2.1}
  // where WM's WM_CPFORMDUMP walks A-side {L.1.2, L.2.2} -- the CPNr/
  // FIFO-age fork behind the fact-52 divergence.
  if (reason == TRACE_AXIOM && term_ctr_n(e) >= 4u) {
    Term al = term_ctr_at(e, 2), ar = term_ctr_at(e, 3);
    return (atp_lr_sortieren_rec(al, ar) > 0) ? 1u : 0u;
  }
  return 1u;
}

// Overlap face-combo index (0..3) of the CP at `src_trace`, or 0xff if
// the trace entry is not a combo-tagged TRACE_CP (an axiom, a simplify,
// or a CP from a non-WM-order path).  Decoded from the last child of the
// TRACE_CP record (atp_trace_push_cp stores it at index 5 + pos_len).
static u8 atp_wmo_cp_combo(const AtpState *s, u32 src_trace) {
  if (s == NULL || src_trace == ATP_TRACE_NONE || src_trace >= s->n_trace) {
    return 0xffu;
  }
  Term e = s->trace[src_trace];
  if (term_tag(e) != TAG_CTR || term_ext(e) != TRACE_CP) return 0xffu;
  u32 n = term_ctr_n(e);
  if (n < 6u) return 0xffu;                 // no pos_len/combo tail
  u32 pos_len = (u32)term_val(term_ctr_at(e, 4));
  u32 combo_idx = 5u + pos_len;
  if (combo_idx >= n) return 0xffu;         // malformed; stay neutral
  return (u8)term_val(term_ctr_at(e, combo_idx));
}

// Does the CP-formation side geometry swap (use_cp_side) physically
// fire for the unorientable equation popped at `src_trace`?  PARENT-
// OVERLAP-AWARE, mirroring WM's reduct assignment (Unifikation1.c:916,
// KPLinks = sigma(TP_RechteSeite(Vater))): which face WM stores
// sigma(r_Vater)-left depends on the overlap geometry, so the swap is
// gated on the CP's face-combo index (tagged at formation, carried on the
// TRACE_CP record) rather than applied to every KBO_UN equation.  Only the
// reversed-i-face combos (2/3 in atp_overlap_ij -- where the overlap
// produced thvm's reduct-on-lhs vs WM's sigma(r)-on-lhs) need the swap;
// combos 0/1 already match WM's stored geometry, so swapping them
// mis-times the equation's own batch (the FIFO-age fork behind the
// blanket-swap CombinatorAxioms__BCKWToSKI__c2 regression).  This was
// established empirically against the WM `-a 4` reference: the combo>=2
// gate keeps the soa Sheffer OrAssociativity prefix at 124 AND, when on,
// keeps McCune/Hillman/Huntington/CommRing/AbelianTarski byte-identical
// (the old blanket reorder forked McCune).
static int atp_lr_sortieren_rec(Term lhs, Term rhs);

static u8 atp_cp_wm_side_swaps(const AtpState *s, u32 src_trace,
                               Term lhs, Term rhs) {
  if (s == NULL || !s->use_cp_side) return 0u;
  u8 combo = atp_wmo_cp_combo(s, src_trace);
  if (combo != 0xffu) return (combo >= 2u) ? 1u : 0u;  // reversed-i face only
  // Untagged (combo == 0xff): an INITIAL AXIOM has no superposition Vater,
  // so WM's KPLinks = sigma(r_Vater) CP-side geometry never applies -- WM
  // canonicalises an initial equation by LRSortieren (SpezNormierung.c:
  // 517-534), not the CP-formation swap.  The default bench leaves
  // use_lr_sortieren OFF, so mirror WM here by side-canonicalising an
  // axiom with LRSortieren.  NOTE: this reproduces the axiom swap soa's
  // Sheffer C-shape needs (soa prefix 124), but does NOT reconcile the
  // BCKWToSKI__c2 axiom -- that theorem needs the SAME unorientable axiom
  // NOT swapped (a downstream FIFO-age cascade forks selection at pick 55
  // whichever stored order WM and thvm agree on), an irreducible tension
  // that is why use_cp_side defaults OFF in the bench (see the bench
  // gate).  A TRACE_SIMPLIFY re-derivation or a non-WM-order superposition
  // CP keeps the legacy blanket swap.
  u32 reason = 99u;
  if (src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
    Term e = s->trace[src_trace];
    if (term_tag(e) == TAG_CTR) reason = term_ext(e);
  }
  if (reason == TRACE_AXIOM) {
    return (atp_lr_sortieren_rec(lhs, rhs) > 0) ? 1u : 0u;
  }
  return 1u;
}

static u8 atp_wmo_eq_dist_rhs_dbg(const AtpState *s, u32 src_trace, u8 verdict) {
  static int on = -1;
  if (on < 0) on = getenv("THVM_ATP_DIST_TRACE") ? 1 : 0;
  if (on) {
    u32 reason = 999u;
    if (src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
      Term e = s->trace[src_trace];
      if (term_tag(e) == TAG_CTR) reason = term_ext(e);
    }
    fprintf(stderr,
            "DISTREG trace=%u reason=%u kind=%u dist=%u n_rules=%u "
            "n_trace=%u swapped=%u\n",
            src_trace, reason, s->last_popped_kind, verdict, s->n_rules,
            s->n_trace, s->last_cp_wm_side_swapped);
  }
  return verdict;
}

static u8 atp_wmo_eq_dist_rhs(const AtpState *s, u32 src_trace) {
  // The flag answers: is WM's distinguished face (KPLinks) thvm's stored
  // RHS?  The physical CP_WM_SIDE swap reorders an unorientable equation
  // to (cp_rhs, cp_lhs) when it fires -- that moves KPLinks from thvm's
  // natural rhs (base == 1) onto the stored lhs, so the flag flips.  The
  // orient site records whether the swap actually fired in
  // last_cp_wm_side_swapped (the single source of truth -- the axiom
  // case's decision depends on the pre-swap term order, not recoverable
  // here from the stored sides), so read it back rather than recompute.
  u8 base = atp_wmo_eq_dist_rhs_base(s, src_trace);
  if (s != NULL && s->last_cp_wm_side_swapped) {
    return atp_wmo_eq_dist_rhs_dbg(s, src_trace, (u8)(1u - base));
  }
  return atp_wmo_eq_dist_rhs_dbg(s, src_trace, base);
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Also records a TRACE_AXIOM
// entry so the proof trace (stage 6.1) can identify this CP's
// origin downstream.  The CP queue is growable, so this never
// rejects for being full; returns 1 on success, 0 only on NULL
// state or a sort-check rejection.
// Shared body: sort-check, var-normalize, push a trace entry with
// the given `reason` / `parent_a`, and enqueue the equation as a CP.
// thvm_atp_add_equation enqueues an input axiom (TRACE_AXIOM, no
// parent); the interreduce re-queue path uses TRACE_SIMPLIFY with
// the dropped rule's trace index so the proof DAG stays connected.
static u8 atp_cp_perm_subsumed(Term lhs, Term rhs);

// Consume-once flag: the next atp_enqueue_equation push is the WM raw
// (untreated, size >= KPBehandeln gate) class -- bypasses the auto-
// MaxWeight stash exactly like the overlap raw class.  Set only by
// atp_wm_demote_drain; cleared on every enqueue.
static u8 g_enq_raw_untreated = 0u;

// s2a: forward-declared so the enqueue can snapshot axiom symbol masks
// (definition is next to thvm_atp_set_goals, far below).
static void atp_collect_symbols(Term t, u64 *mask);

static u8 atp_enqueue_equation(AtpState *s, Term lhs, Term rhs,
                               u32 reason, u32 parent_a) {
  // Consume the raw-untreated hint at ENTRY (the drop gates below early-
  // return; a leaked hint would mis-tag an unrelated later push).
  u8 enq_raw = g_enq_raw_untreated;
  g_enq_raw_untreated = 0u;
  // Consume the goal-derived seed the same way: the heap insert below
  // stamps cp_gd from g_cp_gd (a stale overlap-push value must not
  // leak onto an axiom/requeued equation, and vice versa).
  g_cp_gd = g_enq_gd_pending;
  g_enq_gd_pending = 0u;
  if (s == NULL) return 0;
  if (getenv("THVM_ATP_ENQ_DEBUG") != NULL) {
    fprintf(stderr,
        "[enq] reason=%u  lhs:t%u/e%u",
        reason, term_tag(lhs), term_ext(lhs));
    if (term_tag(lhs) == TAG_CTR) {
      u32 n = term_ctr_n(lhs);
      fprintf(stderr, "/n%u", n);
      for (u32 c = 0; c < n && c < 3; c++) {
        Term ch = term_ctr_at(lhs, c);
        fprintf(stderr, " c%u(t%u/e%u)", c, term_tag(ch), term_ext(ch));
      }
    }
    fprintf(stderr, "  rhs:t%u/e%u",
        term_tag(rhs), term_ext(rhs));
    if (term_tag(rhs) == TAG_CTR) {
      u32 n = term_ctr_n(rhs);
      fprintf(stderr, "/n%u", n);
      for (u32 c = 0; c < n && c < 3; c++) {
        Term ch = term_ctr_at(rhs, c);
        fprintf(stderr, " c%u(t%u/e%u)", c, term_tag(ch), term_ext(ch));
      }
    }
    fprintf(stderr, "\n");
  }
  // 8.4d: when a WaldSpec is attached, reject ill-sorted inputs
  // before mutating state.  Each side must be well-sorted AND
  // both sides must share the same sort (an equation l = r in
  // a sorted signature requires `sort(l) == sort(r)`).
  // Homogeneous-mode (NULL spec or n_sorts == 0) returns sort 0
  // from wald_term_sort unconditionally so the gate is a no-op.
  if (s->spec != NULL) {
    u32 sl, sr;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    const AtpFtCell *bsl = atp_lazy_span_of(lhs);
    const AtpFtCell *bsr = atp_lazy_span_of(rhs);
    if (bsl != NULL && bsr != NULL) {
      sl = wald_term_sort_ft(s->spec, bsl);
      sr = wald_term_sort_ft(s->spec, bsr);
    } else
#endif
    {
      sl = wald_term_sort(s->spec, lhs);
      sr = wald_term_sort(s->spec, rhs);
    }
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
#ifdef ATP_VAR_NORM
  // 7c: an equation enters the engine as a queued CP.  Canonicalize
  // its variables here so the very first CP, like every later-
  // derived one, carries a dense [0, k) variable set.
  thvm_normalize_vars(&lhs, &rhs);
#endif
  // s2a: snapshot the input axiom's symbol mask for the SInE D-relation.
  // Axioms only (parent_a == ATP_TRACE_NONE) -- derived/requeued
  // equations are not part of the intake signature.  Gathered when s2a
  // drives selection OR when the goal_closeness node feature needs the
  // level table (graph export / GNN re-rank); the default engine leaves
  // this array untouched, so its step path stays byte-identical.
  if ((s->s2a_coeff > 0u || s->record_cp_graphs || s->gnn_rerank_period > 0u)
      && parent_a == ATP_TRACE_NONE) {
    u64 m = 0;
    atp_collect_symbols(lhs, &m);
    atp_collect_symbols(rhs, &m);
    if (s->n_s2a_ax >= s->cap_s2a_ax) {
      u32 nc = s->cap_s2a_ax ? s->cap_s2a_ax * 2u : 64u;
      u64 *nm = (u64 *)realloc(s->s2a_ax_mask, (size_t)nc * sizeof(u64));
      if (nm != NULL) { s->s2a_ax_mask = nm; s->cap_s2a_ax = nc; }
    }
    if (s->n_s2a_ax < s->cap_s2a_ax) s->s2a_ax_mask[s->n_s2a_ax++] = m;
  }
  // Permutation-subsumption (WM GZ_ACVerzichtbar): same filter as the
  // overlap-CP push.  Catches commutativity-shaped equations re-queued
  // by interreduce / RHS-composition (atp_add_equation_simplified)
  // before they enter the queue.  Skip for axioms (parent_a ==
  // ATP_TRACE_NONE) -- those are user-supplied and we must not drop.
  // AC-mask path perm-subsumes only at SELECTION (see KPV_Select port);
  // the legacy binary env path (no mask) keeps the enqueue-time drop.
  if (s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull
      && parent_a != ATP_TRACE_NONE
      && atp_cp_perm_subsumed(lhs, rhs)) {
    s->n_cps_dropped_perm_subsumed++;
    return 0;
  }
  // AC soundness gate for derived equations (skip user axioms).
  // Mirrors the CP-push path's gate at atp_overlap_ij's heap-push
  // site (see comment there).  Catches the simplify (TRACE_SIMPLIFY)
  // re-enqueue path where interreduce produces an ill-formed
  // equation by rewriting a rule's side via a degenerate rule that
  // shouldn't have been there in the first place.
#ifdef THVM_ATP_AC
  if (parent_a != ATP_TRACE_NONE && thvm_atp_get_ac_mask() != 0ull) {
    u8 lhs_is_var = (term_tag(lhs) == TAG_FVR);
    u8 rhs_is_var = (term_tag(rhs) == TAG_FVR);
    u8 fwd_sound = atp_vars_contained(rhs, lhs) && !lhs_is_var;
    u8 rev_sound = atp_vars_contained(lhs, rhs) && !rhs_is_var;
    if (!fwd_sound && !rev_sound) {
      return 0;
    }
  }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Span-native requeue (the drain's bracket): canonicalize CLONES of
  // the parked spans, lean-trace and fused-pack straight from them --
  // the queued CP is bytes + trace + the armed weigh state, so no Term
  // materializes.  Conditions mirror the classic path's active gates:
  // the mask-0 perm drop and the AC soundness gate (both Term-side)
  // must be off-route, the trace must be packing (lean children), and
  // the var cliff guards the renumber.  Anything else falls through to
  // the classic Term arm (the caller decoded real Terms then).
  const AtpFtCell *enq_l = g_enq_ft_l;
  const AtpFtCell *enq_r = g_enq_ft_r;
  g_enq_ft_l = NULL;
  g_enq_ft_r = NULL;
  if (enq_l != NULL && enq_r != NULL && atp_lazy_rules_on() &&
      s->trace_pack && atp_order_is_plain_kbo(s) &&
      !(s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull &&
        parent_a != ATP_TRACE_NONE) &&
      ft_distinct_var_count(enq_l, enq_r) < REWRITE_MAX_VAR) {
    AtpFt *ea = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *cl = ft_clone_span(ea, enq_l, 0);
    AtpFtCell *cr = ft_clone_span(ea, enq_r, 0);
    ft_normalize_vars_pair(cl, cr);
    // Lean trace: sentinel children; the eq pair packs off the
    // canonical spans.
    g_trace_eq_ft_l = cl;
    g_trace_eq_ft_r = cr;
    u32 t_idx = atp_trace_push(s, reason, parent_a,
                               ATP_TRACE_NONE, lhs, rhs);
    // Fused pack + weigh state from the canonical spans (identity
    // renumber -- the emit's appearance order IS the canonical order),
    // then the push's fused arm consumes bytes/counts/weigh with no
    // Term reads, exactly like the walk former's routed pushes.
    AcpFuse *fz = &g_acp_fuse;
    fz->valid   = 0u;
    fz->twee_valid = 0u;
    fz->map.n   = 0u;
    fz->phidiff = 0;
    u32 pos = 0u, nodes = 0u, fpos = 0u;
    u8 ok = vcp_ft_emit_side(cl, NULL, NULL, NULL, s->kbo, &fz->map,
                             &pos, &nodes, fz, +1, &fpos);
    u32 lhs_nodes_ft = nodes, flh = fpos;
    if (ok) {
      ok = vcp_ft_emit_side(cr, NULL, NULL, NULL, s->kbo, &fz->map,
                            &pos, &nodes, fz, -1, &fpos);
    }
    if (ok) {
      fz->fl_n  = flh;
      fz->fr_n  = fpos - flh;
      fz->valid = 1u;
      g_vcp_emit.len       = pos;
      g_vcp_emit.nodes     = nodes;
      g_vcp_emit.lhs_nodes = lhs_nodes_ft;
      g_vcp_emit.armed     = 1u;
      u8 saved_fused = g_cp_push_fused;
      g_cp_push_fused = 1u;
      u8 is_ult = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                      ? 1u : 0u;
      g_cp_push_kind = (u8)reason;
      atp_cp_heap_push(s, 0, 0, t_idx, is_ult, enq_raw);
      g_cp_push_kind  = (u8)TRACE_CP;
      g_cp_push_fused = saved_fused;
      g_vcp_emit.armed = 0u;
      g_acp_fuse.valid = 0u;
      return 1;
    }
    // Emit anomaly: fall through to the classic arm with REAL Terms --
    // decode from the parked spans.  The spans are the AUTHORITATIVE
    // content (the drain's NF); the buffered Term may be a sentinel OR
    // the stale pre-normalization pair, so decode whenever the span's
    // content differs (ft_eq_term is read-only and rejects sentinels).
    g_acp_fuse.valid = 0u;
    if (!ft_eq_term(enq_l, lhs)) lhs = ft_to_term(enq_l);
    if (!ft_eq_term(enq_r, rhs)) rhs = ft_to_term(enq_r);
#ifdef ATP_VAR_NORM
    thvm_normalize_vars(&lhs, &rhs);
#endif
    // trace already pushed (t_idx) -- reuse it for the classic push.
    u8 is_ult2 = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                     ? 1u : 0u;
    g_cp_push_kind = (u8)reason;
    atp_cp_heap_push(s, lhs, rhs, t_idx, is_ult2, enq_raw);
    g_cp_push_kind = (u8)TRACE_CP;
    return 1;
  }
  // Fused arm declined (route conditions, e.g. the eager kill switch
  // atp_lazy_rules_on()==0): the parked spans are still the drain's
  // AUTHORITATIVE NF content.  The buffered Terms may be the stale
  // pre-normalization pair (real Terms on the eager path -- decoding
  // only TAG_NUM sentinels here requeued the UN-normalized victim and
  // drifted the eager trajectory from the lazy default: OA
  // WolframAxioms firstdiv=635, 1680 vs 1671 rules), so decode
  // whenever the span content differs -- byte-equal to the
  // pre-span-native drain, which decoded changed sides itself.
  if (enq_l != NULL && !ft_eq_term(enq_l, lhs)) lhs = ft_to_term(enq_l);
  if (enq_r != NULL && !ft_eq_term(enq_r, rhs)) rhs = ft_to_term(enq_r);
#endif
  u32 trace_idx = atp_trace_push(s, reason, parent_a,
                                 ATP_TRACE_NONE, lhs, rhs);
  // Waldmeister history-driven Act_ultimate (NewClassification.c:314,
  // DEF=1 block `initial = ultimate`): tag input axioms so they pop
  // before any derived CP regardless of heuristic weight.  Effective
  // only when s->use_initial_ultimate is set; off by default.
  u8 is_ultimate = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                     ? 1u : 0u;
  // enq_raw: set only by the IR-victim drain for victims whose pair size
  // >= the KPBehandeln gate, i.e. WM's raw class (KPV_IROpferBehandeln's
  // lohntSichBehandlung skips treatment for them and recentCPinsert keeps
  // them in the SUE heap at raw weight -- WM never defers a KilledRE
  // victim, so the auto-MaxWeight stash must not swallow them here either;
  // the OA -auto fact-1378 class: the w1=2141 rule-487 victim sat in
  // thvm's stash while WM's FIFO popped it).
  // Stamp the origin class onto the queued CP (survives the proof-trace
  // soft cap; see cp_kind).  All other push sites are superposition CPs
  // and keep the TRACE_CP default.
  g_cp_push_kind = (u8)reason;
  atp_cp_heap_push(s, lhs, rhs, trace_idx, is_ultimate, enq_raw);
  g_cp_push_kind = (u8)TRACE_CP;
  return 1;
}

// --- ifeq (encoded-clause dispatcher) guard-aware CP selection --------
//
// See the AtpState.ifeq_syms doc block (thvm.h) for the mechanism.
// Detection runs here at axiom intake; the weight scale itself is
// applied in atp_cp_priority_sized_ex (Term route: every re-score
// path) and atp_cp_heap_insert_packed (FT-routed pushes whose Term
// args are sentinels).

// THVM_ATP_IFEQ (default on): the detector kill switch.
static int atp_ifeq_env_on(void) {
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("THVM_ATP_IFEQ");
    on = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return on;
}
// THVM_ATP_IFEQ_GUARD (default 0 = mechanism disarmed).
static int atp_ifeq_guard_env(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_GUARD");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
  }
  return v;
}
// THVM_ATP_IFEQ_PENALTY (default 8): per-undecided-guard weight factor.
static u32 atp_ifeq_penalty(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_PENALTY");
    v = (e != NULL) ? atoi(e) : 8;
    if (v < 2) v = 2;
  }
  return (u32)v;
}

// THVM_ATP_IFEQ_DECIDE: float multiplier applied to a guard-DECIDING
// CP's priority (see the AtpState.use_ifeq_decide doc block, thvm.h).
// Unset or an explicit "0" leaves the mechanism DISARMED (base
// unchanged -- the default, byte-identical engine).  The bare-flag
// CLI default ("1", from `--ifeq-decide` with no `=value`) maps to
// the recommended 0.25 (4x cheaper); any other value is used directly
// as the multiplier, clamped to (0,1].  Cached fixed-point x1000 once
// so the hot CP-scoring path does no floating point.
static u32 atp_ifeq_decide_x1000(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_DECIDE");
    double d;
    if (e == NULL || e[0] == '\0') d = 0.0;
    else if (strcmp(e, "1") == 0)  d = 0.25;
    else                           d = strtod(e, NULL);
    if (d < 0.0) d = 0.0;
    if (d > 1.0) d = 1.0;
    v = (int)(d * 1000.0 + 0.5);
  }
  return (u32)v;
}
static u8 atp_ifeq_decide_armed(void) {
  return atp_ifeq_decide_x1000() > 0u;
}

// Dispatcher axiom shape: l = f(A,A,B[,C]) with every argument a
// variable, args 1,2 the SAME variable, B (and C) distinct from it,
// and r the bare variable B.
static u8 atp_ifeq_axiom_shape(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_FVR) return 0;
  u32 n = term_ctr_n(l);
  if (n != 3u && n != 4u) return 0;
  Term a0 = term_ctr_at(l, 0), a1 = term_ctr_at(l, 1);
  Term a2 = term_ctr_at(l, 2);
  if (term_tag(a0) != TAG_FVR || term_tag(a1) != TAG_FVR ||
      term_tag(a2) != TAG_FVR) return 0;
  if (term_ext(a0) != term_ext(a1)) return 0;
  if (term_ext(a2) == term_ext(a0) || term_ext(a2) != term_ext(r)) return 0;
  if (n == 4u) {
    Term a3 = term_ctr_at(l, 3);
    if (term_tag(a3) != TAG_FVR) return 0;
    if (term_ext(a3) == term_ext(a0) || term_ext(a3) == term_ext(a2))
      return 0;
  }
  return 1;
}

static void atp_ifeq_detect(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || !atp_ifeq_env_on()) return;
  Term f = 0;
  if      (atp_ifeq_axiom_shape(lhs, rhs)) f = lhs;
  else if (atp_ifeq_axiom_shape(rhs, lhs)) f = rhs;
  if (f == 0) return;
  u32 sym = term_ext(f);
  u32 cap = (u32)(sizeof s->ifeq_syms / sizeof s->ifeq_syms[0]);
  for (u32 i = 0; i < s->n_ifeq_syms; i++)
    if (s->ifeq_syms[i] == sym) return;
  if (s->n_ifeq_syms >= cap) return;
  s->ifeq_syms[s->n_ifeq_syms++] = sym;
  if (atp_ifeq_guard_env() >= 1) s->use_ifeq_guard = 1u;
  if (atp_ifeq_decide_armed()) s->use_ifeq_decide = 1u;
  if (getenv("THVM_ATP_IFEQ_DEBUG") != NULL) {
    fprintf(stderr, "[ifeq] dispatcher sym=%u detected (n=%u armed=%u)\n",
            sym, s->n_ifeq_syms, (u32)s->use_ifeq_guard);
  }
}

static u8 atp_ifeq_sym_is(const AtpState *s, u32 sym) {
  for (u32 i = 0; i < s->n_ifeq_syms; i++)
    if (s->ifeq_syms[i] == sym) return 1u;
  return 0u;
}

// Syntactic Term equality (vars by id, constructors by label + kids).
// Used only for the arg1-vs-arg2 decidedness test on detected ifeq
// nodes; early-exits on the first mismatch (guards differ at the head
// in practice: theorem(...) vs true).
static u8 atp_ifeq_term_eq(Term a, Term b) {
  u8 ta = term_tag(a), tb = term_tag(b);
  if (ta != tb) return 0;
  if (ta == TAG_FVR) return term_ext(a) == term_ext(b);
  if (ta != TAG_CTR) return a == b;
  if (term_ext(a) != term_ext(b)) return 0;
  u32 n = term_ctr_n(a);
  if (n != term_ctr_n(b)) return 0;
  for (u32 i = 0; i < n; i++)
    if (!atp_ifeq_term_eq(term_ctr_at(a, i), term_ctr_at(b, i))) return 0;
  return 1;
}

// Count UNDECIDED guards: detected ifeq nodes whose args 1,2 are not
// syntactically equal (a decided one collapses at the next normalize
// via the dispatcher rule itself, so it must NOT be penalized).
static u32 atp_ifeq_guard_count(const AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 n = term_ctr_n(t);
  u32 c = 0u;
  if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, term_ext(t)) &&
      !atp_ifeq_term_eq(term_ctr_at(t, 0), term_ctr_at(t, 1))) c = 1u;
  for (u32 i = 0; i < n; i++)
    c += atp_ifeq_guard_count(s, term_ctr_at(t, i));
  return c;
}

// Fact test: in the ifeq encoding every semantically loaded derived
// equation has an ATOMIC side (`g = true`, a definitional fold to a
// constant, or a collapse residue that is a bare variable).  A CP with
// two compound sides is theory algebra (or/not identity churn on
// LCL306-10) that can never decide a guard -- it takes one extra
// penalty step (the mission's guard-deciding BONUS, expressed
// order-independently so re-scores agree).
static u8 atp_ifeq_side_atomic(Term t) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) return 1u;
  return (tg == TAG_CTR && term_ctr_n(t) == 0u) ? 1u : 0u;
}

// Scale a CP priority by penalty^guards, saturating below the
// ATP_CP_PRI_NEVER sentinel (a penalized CP still outranks a
// deprioritized-never one).
static u32 atp_ifeq_scale_pri(u32 base, u32 guards) {
  if (guards == 0u) return base;
  if (guards > 8u) guards = 8u;
  u64 b = (base == 0u) ? 1u : (u64)base;
  u64 p = (u64)atp_ifeq_penalty();
  for (u32 i = 0; i < guards; i++) {
    b *= p;
    if (b >= (u64)ATP_CP_PRI_NEVER) return ATP_CP_PRI_NEVER - 1u;
  }
  return (u32)b;
}

// Distinct-variable bitmask (var ids mod 64 -- an over-approximation
// dedupe, exact for the engine's REWRITE_MAX_VAR-bounded rule space).
static void atp_ifeq_var_mask(Term t, u64 *mask) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) { *mask |= 1ull << (term_ext(t) & 63u); return; }
  if (tg != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) atp_ifeq_var_mask(term_ctr_at(t, i), mask);
}
// THVM_ATP_IFEQ_VARCAP (default 3): distinct-variable budget; every
// variable beyond it costs one penalty step.  The oracle proofs'
// general lemmas carry <= 3 distinct variables -- the ever-more-general
// 4-5-var theorem-schema towers (the classic LCL forward-chaining
// flood, cf. the horn-cd lane's --horn-cd-vars) are pure churn.
static u32 atp_ifeq_varcap(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_VARCAP");
    v = (e != NULL) ? atoi(e) : 3;
    if (v < 1) v = 1;
  }
  return (u32)v;
}

// THVM_ATP_IFEQ_FREE (default 1): guards within this budget are not
// penalized.  The oracle derivations run on facts AND one-guard
// conditional lemmas (partially discharged modus ponens: f14631/f105383
// and a dozen siblings in LCL306-10's refutation) -- penalizing the
// first guard buries that whole lemma class behind the fact churn.
static u32 atp_ifeq_guard_free(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_FREE");
    v = (e != NULL) ? atoi(e) : 1;
    if (v < 0) v = 0;
  }
  return (u32)v;
}

// Combined exponent for one CP: undecided guards on both sides beyond
// the free budget, plus one extra step when NEITHER side is atomic
// (see atp_ifeq_side_atomic), plus one step per distinct variable
// beyond the var budget.
static u32 atp_ifeq_weight_scale(const AtpState *s, Term lhs, Term rhs,
                                 u32 base) {
  u32 g = atp_ifeq_guard_count(s, lhs) + atp_ifeq_guard_count(s, rhs);
  u32 fr = atp_ifeq_guard_free();
  g = (g > fr) ? g - fr : 0u;
  if (!atp_ifeq_side_atomic(lhs) && !atp_ifeq_side_atomic(rhs)) g += 1u;
  u64 vm = 0ull;
  atp_ifeq_var_mask(lhs, &vm);
  atp_ifeq_var_mask(rhs, &vm);
  u32 nv = (u32)__builtin_popcountll(vm);
  u32 cap = atp_ifeq_varcap();
  if (nv > cap) g += nv - cap;
  return atp_ifeq_scale_pri(base, g);
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin of atp_ifeq_guard_count, for the walk former's routed
// pushes whose Term args are sentinels (the real content lives in the
// g_vcp_treat NF spans).  Same verdict: prefix walk, one tick per
// detected ifeq cell whose arg-1/arg-2 spans differ (ft_eq).
static u32 atp_ifeq_guard_count_ft(const AtpState *s, const AtpFtCell *t) {
  if (t == NULL) return 0u;
  u32 c = 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    if (!ft_is_var(x) && (x->arity == 3u || x->arity == 4u) &&
        atp_ifeq_sym_is(s, ft_ctr_sym(x))) {
      const AtpFtCell *a0 = x->next;
      const AtpFtCell *a1 = a0->end->next;
      if (!ft_eq(a0, a1)) c++;
    }
    if (x == stop) break;
  }
  return c;
}
// FT-span twin of atp_ifeq_weight_scale (guards + non-fact step +
// var-budget steps), for the routed pushes.
static u32 atp_ifeq_weight_scale_ft(const AtpState *s, const AtpFtCell *fl,
                                    const AtpFtCell *fr, u32 base) {
  u32 g = atp_ifeq_guard_count_ft(s, fl) + atp_ifeq_guard_count_ft(s, fr);
  u32 fr_budget = atp_ifeq_guard_free();
  g = (g > fr_budget) ? g - fr_budget : 0u;
  u8 fl_atomic = ft_is_var(fl) || fl->arity == 0u;
  u8 fr_atomic = ft_is_var(fr) || fr->arity == 0u;
  if (!fl_atomic && !fr_atomic) g += 1u;
  u64 vm = 0ull;
  for (int side = 0; side < 2; side++) {
    const AtpFtCell *t = side ? fr : fl;
    if (t == NULL) continue;
    const AtpFtCell *stop = t->end;
    for (const AtpFtCell *x = t; ; x = x->next) {
      if (ft_is_var(x)) vm |= 1ull << (ft_var_id(x) & 63u);
      if (x == stop) break;
    }
  }
  u32 nv = (u32)__builtin_popcountll(vm);
  u32 cap = atp_ifeq_varcap();
  if (nv > cap) g += nv - cap;
  return atp_ifeq_scale_pri(base, g);
}
#endif

// --- ifeq guard-DECIDING CP boost --------------------------------------
//
// The guard-count PENALTY above deprioritizes CPs that RETAIN
// undecided guards.  This is the complementary discipline: PRIORITIZE
// CPs that DECIDE one -- a CP of shape `g = true` where `g` matches a
// condition subterm some ACTIVE rule (or the goal) uses as an ifeq
// guard.  Deciding such a `g` lets the dispatcher axiom collapse that
// guard site on its very next normalize -- exactly the inference
// Vampire's literal selection performs when it resolves a guard
// literal instead of chasing the whole clause.
//
// Registry: guard condition subterms (arg0 of a detected-ifeq
// application whose arg0 != arg1 syntactically -- an UNDECIDED guard
// at registration time) are collected into a 4096-bucket fingerprint
// membership set (AtpState.ifeq_guard_mask) as they're seen in
// freshly added rules (atp_push_rule) and goal conjuncts
// (thvm_atp_set_goals).  The fingerprint is an ALPHA-INVARIANT
// structural hash (first-occurrence variable numbering, like
// thvm_normalize_vars but scoped to the single extracted subterm)
// folding in the root label and the full shape via the same FNV
// recurrence ft_hash/atp_term_struct_hash use (ft.c:672-718), so two
// occurrences of "the same" guard schema under different rules' local
// variable numbering -- e.g. sum(Y,X,V1) in one distributivity axiom
// and sum(A,B,C) in another -- fingerprint identically, while an
// unrelated term collides only by genuine hash accident.  A collision
// is harmless (a mis-boosted CP is simply selected a little early --
// nothing is dropped, completeness untouched); a stale entry left by
// a dead rule costs the same nothing-worse-than-early-pick, so
// removal on rule death is skipped.  The bucket count IS the cap
// (4096, the mission's ~4k budget) -- past it the false-positive rate
// rises smoothly, no hard ceiling to enforce.
//
// The "true" comparison constant is derived the same way: the first
// ATOMIC (0-ary) arg1 seen at any detected-ifeq site is cached on
// AtpState.ifeq_true_term and reused for an O(1) exact compare (no
// need to fingerprint the decided side, only the guard side).

// Bounded first-occurrence variable remap: canonicalizes a subterm's
// variable ids to 0,1,2,... in left-to-right preorder -- scoped to a
// single extracted guard subterm so two independent occurrences of
// the same schema (different rules, different raw var ids)
// canonicalize identically.  Overflow (never hit in practice: guards
// are small predicate atoms) shares one bucket via the sentinel id.
typedef struct { u32 raw[REWRITE_MAX_VAR]; u32 n; } AtpIfeqVarMap;
static u32 atp_ifeq_canon_var(AtpIfeqVarMap *vm, u32 raw_id) {
  for (u32 i = 0; i < vm->n; i++) if (vm->raw[i] == raw_id) return i;
  if (vm->n < REWRITE_MAX_VAR) { vm->raw[vm->n] = raw_id; return vm->n++; }
  return 0xFFFFu;
}
// FNV-style recurrence, same constants as ft_hash/atp_term_struct_hash;
// the only deviation is the canonical (not raw) variable id, which is
// what makes this hash alpha-invariant instead of alpha-exact.
static u64 atp_ifeq_guard_hash_rec(Term t, AtpIfeqVarMap *vm, u32 depth) {
  if (depth > 48u) return 0x9e3779b97f4a7c15ull;
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) {
    u32 cid = atp_ifeq_canon_var(vm, term_ext(t));
    return 0xfacefacefaceull ^ ((u64)cid * 0x100000001b3ull);
  }
  if (tg != TAG_CTR) return 0xdeadbeefcafebabeull ^ ((u64)t * 0x100000001b3ull);
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) n = 0u;
  u64 h = 0xcbf29ce484222325ull ^ ((u64)term_ext(t) * 0x100000001b3ull);
  for (u32 i = 0; i < n; i++) {
    h ^= atp_ifeq_guard_hash_rec(term_ctr_at(t, i), vm, depth + 1u);
    h *= 0x100000001b3ull;
  }
  return h ^ ((u64)n * 0x9e3779b97f4a7c15ull);
}
static u32 atp_ifeq_guard_bucket(Term t) {
  AtpIfeqVarMap vm; vm.n = 0u;
  u64 h = atp_ifeq_guard_hash_rec(t, &vm, 0u);
  return (u32)(h & 4095ull);
}
static void atp_ifeq_guard_mark(AtpState *s, Term g) {
  u32 b = atp_ifeq_guard_bucket(g);
  s->ifeq_guard_mask[(b >> 6) & 63u] |= 1ull << (b & 63u);
}
static u8 atp_ifeq_guard_hit(const AtpState *s, Term g) {
  u32 b = atp_ifeq_guard_bucket(g);
  return (u8)((s->ifeq_guard_mask[(b >> 6) & 63u] >> (b & 63u)) & 1ull);
}

// Telemetry (env-gated print in the bench stats block): how many CP
// scoring events actually applied the boost, and how many boosted CPs
// were subsequently popped by thvm_atp_select_cp -- see
// atp_ifeq_decide_scale / atp_ifeq_decide_hit below.  Zero for the
// entire run whenever the mechanism is disarmed (the default).
u64 g_ifeq_decide_n_hit    = 0;  // scoring events that applied the boost
u64 g_ifeq_decide_n_popped = 0;  // popped CPs that were guard-deciding

// Walk a freshly-added rule (or goal conjunct) for detected-ifeq
// applications with an UNDECIDED guard (arg0 != arg1); register arg0
// as a live guard fingerprint and, on the first atomic arg1 seen,
// cache it as the encoding's boolean-true constant.  Depth/arity
// bounded like every other recursive term walk in this file.
static void atp_ifeq_guard_register_rec(AtpState *s, Term t, u32 depth) {
  if (depth > 48u || term_tag(t) != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return;
  if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, term_ext(t))) {
    Term a0 = term_ctr_at(t, 0), a1 = term_ctr_at(t, 1);
    if (!atp_ifeq_term_eq(a0, a1)) {
      atp_ifeq_guard_mark(s, a0);
      if (s->ifeq_true_term == 0 && term_tag(a1) == TAG_CTR &&
          term_ctr_n(a1) == 0u) {
        s->ifeq_true_term = a1;
      }
    }
  }
  for (u32 i = 0; i < n; i++)
    atp_ifeq_guard_register_rec(s, term_ctr_at(t, i), depth + 1u);
}
static void atp_ifeq_guard_register(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || !s->use_ifeq_decide) return;
  atp_ifeq_guard_register_rec(s, lhs, 0u);
  atp_ifeq_guard_register_rec(s, rhs, 0u);
}

// True iff {lhs,rhs} is exactly {a registered guard, the true
// constant}, order-independent (equations are unoriented at CP
// formation).  Shared by the Term-route scale below and the
// thvm_atp_select_cp pop-time telemetry.
static u8 atp_ifeq_decide_hit(const AtpState *s, Term lhs, Term rhs) {
  if (s->ifeq_true_term == 0) return 0u;
  if (atp_ifeq_term_eq(rhs, s->ifeq_true_term) &&
      atp_ifeq_guard_hit(s, lhs)) return 1u;
  if (atp_ifeq_term_eq(lhs, s->ifeq_true_term) &&
      atp_ifeq_guard_hit(s, rhs)) return 1u;
  return 0u;
}
// Priority scale: a single-shot multiplier (not a per-guard
// exponential like the penalty -- one guard decided is one guard
// decided) by THVM_ATP_IFEQ_DECIDE when atp_ifeq_decide_hit fires.
// Floors at 1 so a boosted CP never collapses to the always-first
// sentinel 0; never touches an already-NEVER priority.
static u32 atp_ifeq_decide_scale(const AtpState *s, Term lhs, Term rhs,
                                 u32 base) {
  if (base == 0u || base >= ATP_CP_PRI_NEVER) return base;
  if (!atp_ifeq_decide_hit(s, lhs, rhs)) return base;
  g_ifeq_decide_n_hit++;
  u64 b = (u64)base * atp_ifeq_decide_x1000() / 1000ull;
  return (u32)(b == 0ull ? 1ull : b);
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin of atp_ifeq_guard_hash_rec/atp_ifeq_guard_bucket, for
// matching a routed-push CP's NF spans against the SAME registry the
// Term-route populates (ft_ctr_sym/term_ext share one label space
// engine-wide -- see the ft_hash / atp_term_struct_hash mirror this
// walk copies the recurrence shape from -- so the two hashes agree on
// shared content by construction).
static u64 atp_ifeq_guard_hash_ft_rec(const AtpFtCell *t, AtpIfeqVarMap *vm) {
  if (ft_is_var(t)) {
    u32 cid = atp_ifeq_canon_var(vm, ft_var_id(t));
    return 0xfacefacefaceull ^ ((u64)cid * 0x100000001b3ull);
  }
  u32 n = t->arity;
  u64 h = 0xcbf29ce484222325ull ^ ((u64)ft_ctr_sym(t) * 0x100000001b3ull);
  const AtpFtCell *k = t->next;
  for (u32 i = 0; i < n; i++) {
    h ^= atp_ifeq_guard_hash_ft_rec(k, vm);
    h *= 0x100000001b3ull;
    k = k->end->next;
  }
  return h ^ ((u64)n * 0x9e3779b97f4a7c15ull);
}
static u32 atp_ifeq_guard_bucket_ft(const AtpFtCell *t) {
  AtpIfeqVarMap vm; vm.n = 0u;
  u64 h = atp_ifeq_guard_hash_ft_rec(t, &vm);
  return (u32)(h & 4095ull);
}
static u8 atp_ifeq_guard_hit_ft(const AtpState *s, const AtpFtCell *t) {
  u32 b = atp_ifeq_guard_bucket_ft(t);
  return (u8)((s->ifeq_guard_mask[(b >> 6) & 63u] >> (b & 63u)) & 1ull);
}
// FT-span twin of atp_ifeq_decide_scale, for the routed pushes whose
// Term args are sentinels (see atp_ifeq_weight_scale_ft above).
static u32 atp_ifeq_decide_scale_ft(const AtpState *s, const AtpFtCell *fl,
                                    const AtpFtCell *fr, u32 base) {
  if (s->ifeq_true_term == 0 || base == 0u || base >= ATP_CP_PRI_NEVER)
    return base;
  u8 hit = 0u;
  if (ft_eq_term(fr, s->ifeq_true_term) &&
      atp_ifeq_guard_hit_ft(s, fl)) hit = 1u;
  else if (ft_eq_term(fl, s->ifeq_true_term) &&
           atp_ifeq_guard_hit_ft(s, fr)) hit = 1u;
  if (!hit) return base;
  g_ifeq_decide_n_hit++;
  u64 b = (u64)base * atp_ifeq_decide_x1000() / 1000ull;
  return (u32)(b == 0ull ? 1ull : b);
}
#endif

// --- ifeq formation-time resolved-guard stripping ---------------------
//
// THVM_ATP_IFEQ_STRIP (+ CLI --ifeq-strip): under --lazy-normalize the
// raw >= KPBehandeln-gate class is queued UNTREATED and scored on its
// raw size, so a guard-DECIDING superposition -- whose conclusion
// carries ifeq(t, t, A, B) subterms that one dispatcher rewrite
// collapses to A -- is buried at guard-tower weight while its treated
// (< gate) siblings score their normal forms.  When the dispatcher
// detector armed (AtpState.ifeq_syms), strip every RESOLVED guard from
// the raw class at CP formation: a single deterministic bottom-up
// rewrite with the dispatcher axiom (sound -- the axiom is an input
// equation; the dispatcher rule system is orthogonal, so the collapse
// is confluent), done WITHOUT the full R-normalize the lazy gate
// exists to skip.  The queued/scored content becomes the stripped
// pair; the proof trace keeps the RAW overlap (exactly the treated
// class's contract -- the pop-time record normalize re-derives from
// the trace's raw pair, see the record_norm_steps block).  A CP whose
// stripped size falls BELOW the gate is reclassified into the treated
// class (full doR treatment + joined-drop), which is what WM would
// have done had the CP formed at that size.  Off (default): byte-
// identical engine.
//
// THVM_ATP_LAZY_SCORE_NF (+ CLI --lazy-score-nf): the scoring-only
// companion -- estimate the raw CP's normal-form size with a step-
// capped doR normalize on a THROWAWAY clone (value 1 = default cap of
// 4 rewrite steps; N >= 2 = cap N) and feed the estimated per-side
// counts to the CP priority in place of the raw counts.  The queued
// bytes/spans stay the raw pair (the lazy pipeline is untouched);
// selection just stops ranking guard towers by their tower size.

static int atp_ifeq_strip_env(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_IFEQ_STRIP");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
  }
  return v;
}
static u8 atp_ifeq_strip_armed(const AtpState *s) {
  return (s != NULL && s->use_lazy_normalize && s->n_ifeq_syms > 0u &&
          atp_ifeq_strip_env() >= 1) ? 1u : 0u;
}
// 0 = off; 1 = the default 4-step cap; N >= 2 = cap N.
static u32 atp_lazy_scorenf_cap(void) {
  static int v = -2;
  if (v == -2) {
    const char *e = getenv("THVM_ATP_LAZY_SCORE_NF");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
  }
  return (v == 1) ? 4u : (u32)v;
}

u64 g_ifeq_strip_n_hit     = 0;  // raw CPs with >= 1 resolved guard stripped
u64 g_ifeq_strip_n_reclass = 0;  // stripped below the gate -> treated class
u64 g_scorenf_n            = 0;  // raw CPs scored via the capped-NF probe
u64 g_scorenf_shrunk       = 0;  // probe size strictly below the raw size

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static inline void atp_ft_mirror_ensure(AtpState *s);

// Does the span contain a RESOLVED detected-dispatcher guard --
// an ifeq cell whose arg-1/arg-2 spans are syntactically identical?
// (The strip's trigger: with zero resolved guards no bottom-up
// collapse can fire, including the cascade case, whose innermost
// collapse is itself a directly-resolved guard.)
static u8 atp_ifeq_span_has_resolved(const AtpState *s, const AtpFtCell *t) {
  if (t == NULL) return 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    if (!ft_is_var(x) && (x->arity == 3u || x->arity == 4u) &&
        atp_ifeq_sym_is(s, ft_ctr_sym(x))) {
      const AtpFtCell *a0 = x->next;
      const AtpFtCell *a1 = a0->end->next;
      if (ft_eq(a0, a1)) return 1u;
    }
    if (x == stop) break;
  }
  return 0u;
}

// Preorder cell count of a span == the pair's symbol-count nodes (one
// cell per constructor / variable; the NUM marker's 3 cells overcount
// a Term-side 1, acceptable -- the count feeds selection weights and
// the gate reclass, never an exactness contract).
static u32 atp_ifeq_span_cells(const AtpFtCell *t) {
  u32 n = 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    n++;
    if (x == stop) break;
  }
  return n;
}

// Rebuild `c` with every resolved detected-dispatcher guard collapsed
// to its then-branch.  Bottom-up (children stripped BEFORE the parent
// guard equality test), so nested collapses cascade to the fixpoint in
// one pass -- the leftmost-innermost normal form of the orthogonal
// dispatcher rule system.  Returns a fresh Arena-A span (caller frees
// the original); NULL on an arity/depth anomaly (caller keeps the
// original raw pair -- the strip is best-effort selection repair).
static AtpFtCell *atp_ifeq_strip_build(const AtpState *s, AtpFt *a,
                                       const AtpFtCell *c, u32 depth) {
  enum { IFEQ_STRIP_MAX_DEPTH = 200, IFEQ_STRIP_MAX_KIDS = 16 };
  if (depth > (u32)IFEQ_STRIP_MAX_DEPTH) return NULL;
  if (ft_is_var(c)) return ftnew_var(a, ft_var_id(c), 0);
  if (ft_ctr_sym(c) == ATPFT_NUM_MARKER) return ft_clone_span(a, c, 0);
  u32 n = (u32)c->arity;
  if (n == 0u) return ftnew_const(a, ft_ctr_sym(c), 0);
  if (n > (u32)IFEQ_STRIP_MAX_KIDS) return NULL;
  AtpFtCell *kids[IFEQ_STRIP_MAX_KIDS];
  const AtpFtCell *k = c->next;
  for (u32 i = 0; i < n; i++) {
    kids[i] = atp_ifeq_strip_build(s, a, k, depth + 1u);
    if (kids[i] == NULL) {
      for (u32 j = 0; j < i; j++) ft_free_span(a, kids[j], kids[j]->end);
      return NULL;
    }
    k = k->end->next;
  }
  if ((n == 3u || n == 4u) && atp_ifeq_sym_is(s, ft_ctr_sym(c)) &&
      ft_eq(kids[0], kids[1])) {
    AtpFtCell *keep = kids[2];
    for (u32 i = 0; i < n; i++) {
      if (i != 2u) ft_free_span(a, kids[i], kids[i]->end);
    }
    return keep;
  }
  return ftnew_ctr(a, ft_ctr_sym(c), (u16)n, kids, 0);
}

// Strip both sides of a stashed span pair in place (replace + free the
// originals).  Returns the stripped pair's combined cell count, or 0
// when nothing was stripped / an anomaly kept the originals.
static u32 atp_ifeq_strip_spans(const AtpState *s, AtpFt *a,
                                AtpFtCell **pfl, AtpFtCell **pfr) {
  if (*pfl == NULL || *pfr == NULL) return 0u;
  if (!atp_ifeq_span_has_resolved(s, *pfl) &&
      !atp_ifeq_span_has_resolved(s, *pfr)) {
    return 0u;
  }
  AtpFtCell *nl = atp_ifeq_strip_build(s, a, *pfl, 0u);
  if (nl == NULL) return 0u;
  AtpFtCell *nr = atp_ifeq_strip_build(s, a, *pfr, 0u);
  if (nr == NULL) {
    ft_free_span(a, nl, nl->end);
    return 0u;
  }
  ft_free_span(a, *pfl, (*pfl)->end);
  ft_free_span(a, *pfr, (*pfr)->end);
  *pfl = nl;
  *pfr = nr;
  return atp_ifeq_span_cells(nl) + atp_ifeq_span_cells(nr);
}

// Term-route strip (the classic build's raw class: real cp_lhs/cp_rhs
// Terms).  On a strip hit writes the stripped Terms back and returns
// the new combined node count; 0 = untouched.
static u32 atp_ifeq_strip_terms(AtpState *s, Term *lhs, Term *rhs) {
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = ft_from_term(a, *lhs, 0);
  AtpFtCell *fr = ft_from_term(a, *rhs, 0);
  u32 sz = atp_ifeq_strip_spans(s, a, &fl, &fr);
  if (sz != 0u) {
    *lhs = ft_to_term(fl);
    *rhs = ft_to_term(fr);
  }
  ft_free_span(a, fl, fl->end);
  ft_free_span(a, fr, fr->end);
  return sz;
}

// Emit-route strip (the raw >= gate class serialized straight to bytes
// by vcp_emit_fused / vcp_ft_emit_fused): decode the scratch bytes to
// spans, and on a strip hit CONVERT the push to the treated-stash
// route -- the stripped spans become the queue payload (ftp_pack_fused
// at the heap push re-packs + re-weighs them) while the emitted raw
// bytes stay in the scratch as the stash's eager raw-pair record
// (raw_len), exactly what the trace store consumes.  The caller owns
// the stash release after the push (the former's tail only disarms the
// emit).  Returns the stripped size (hijacked) or 0 (emit untouched).
static u32 atp_ifeq_strip_hijack_emit(AtpState *s) {
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  const u8 *pp = g_acp_scratch;
  AtpFtCell *fl = acp_unpack_term_ft(a, &pp, 0);
  AtpFtCell *fr = acp_unpack_term_ft(a, &pp, 0);
  u32 sz = atp_ifeq_strip_spans(s, a, &fl, &fr);
  if (sz == 0u) {
    ft_free_span(a, fl, fl->end);
    ft_free_span(a, fr, fr->end);
    return 0u;
  }
  g_vcp_treat.fl      = fl;
  g_vcp_treat.fr      = fr;
  g_vcp_treat.raw_len = g_vcp_emit.len;   // raw bytes already in scratch
  g_vcp_treat.lo      = NULL;             // no deferred-emit operands
  g_vcp_treat.sub     = NULL;
  g_vcp_treat.rin     = NULL;
  g_vcp_treat.ro      = NULL;
  g_vcp_treat.subst   = NULL;
  g_vcp_treat.armed   = 1u;
  g_vcp_emit.armed    = 0u;
  g_acp_fuse.valid    = 0u;   // the emit's weigh state died with it
  return sz;
}

// Fix-2 probe: per-side normal-form size ESTIMATE for a raw-class CP
// -- step-capped doR normalize on throwaway clones.  Returns 1 with
// *nl/*nr set; 0 = no source (caller keeps the raw counts).
static u8 atp_cp_lazy_scorenf_probe(AtpState *s, const u8 *packed,
                                    Term lhs, Term rhs, u32 cap,
                                    u32 *nl, u32 *nr) {
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = NULL, *fr = NULL;
  if (g_vcp_treat.armed && g_vcp_treat.fl != NULL &&
      g_vcp_treat.fr != NULL) {
    fl = ft_clone_span(a, g_vcp_treat.fl, 0);
    fr = ft_clone_span(a, g_vcp_treat.fr, 0);
  } else if (packed != NULL) {
    const u8 *pp = packed;
    fl = acp_unpack_term_ft(a, &pp, 0);
    fr = acp_unpack_term_ft(a, &pp, 0);
  } else if (lhs != 0 && rhs != 0 && term_tag(lhs) != TAG_NUM &&
             term_tag(rhs) != TAG_NUM) {
    fl = ft_from_term(a, lhs, 0);
    fr = ft_from_term(a, rhs, 0);
  }
  if (fl == NULL || fr == NULL) {
    if (fl != NULL) ft_free_span(a, fl, fl->end);
    if (fr != NULL) ft_free_span(a, fr, fr->end);
    return 0u;
  }
  fl = atp_rules_only_normalize_ft(s, fl, cap);
  fr = atp_rules_only_normalize_ft(s, fr, cap);
  *nl = atp_ifeq_span_cells(fl);
  *nr = atp_ifeq_span_cells(fr);
  ft_free_span(a, fl, fl->end);
  ft_free_span(a, fr, fr->end);
  return 1u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// --- tgt-sel: goal-targeted CP-selection weights ----------------------
//
// A SELECTION-side companion to the tgt preprocessing port (Vampire's
// tgt=full/ground is a preprocessing transformation -- see
// pr_tgt_transform in tests/test_atp_wolfram_bench.c): per-symbol
// CP-selection weights derived from the GOAL, so goal-flavored terms
// are systematically lighter and their CPs float to the queue front.
// Symbols occurring in the conjecture (goal equation + the labels fed
// through thvm_atp_tgt_add_sym) weigh 1; every other symbol weighs
// THVM_ATP_TGT_NONGOAL (default 2).  Mode 2 ('ground') additionally
// raises variables to the non-goal weight, so ground goal-copies
// outrank general terms.  Applied as a multiplier on the CP priority:
//   base' = base * weighted_count / plain_count
// which IS the weighted symbol-count for the ADD weight mode and a
// proportional bias for every other mode (GT/TWEE/...).  The
// reduction-ordering KBO is untouched -- orientation stays
// soundness-clean; this is selection bias only, nothing is dropped,
// completeness untouched.  See AtpState.tgt_sym_mask (thvm.h).
// Applied in atp_cp_priority_sized_ex (Term route: every re-score
// path) and atp_cp_heap_insert_packed (FT-routed pushes whose Term
// args are sentinels), exactly like the ifeq guard scale above.

// THVM_ATP_TGT_SEL: 0 off (default), 1 'full', 2 'ground'.
static u32 atp_tgt_env(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_TGT_SEL");
    v = (e != NULL) ? atoi(e) : 0;
    if (v < 0) v = 0;
    if (v > 2) v = 2;
  }
  return (u32)v;
}
// THVM_ATP_TGT_NONGOAL (default 2): the non-goal symbol weight.
static u32 atp_tgt_nongoal(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_TGT_NONGOAL");
    v = (e != NULL) ? atoi(e) : 2;
    if (v < 2) v = 2;
  }
  return (u32)v;
}

#define ATP_TGT_MASK_WORDS 64u    // 4096 buckets; labels alias mod 4096
static u8 atp_tgt_sym_hit(const AtpState *s, u32 lbl) {
  return (u8)((s->tgt_sym_mask[(lbl >> 6) & (ATP_TGT_MASK_WORDS - 1u)]
               >> (lbl & 63u)) & 1ull);
}
static void atp_tgt_mask_collect(AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return;
  u32 lbl = term_ext(t);
  s->tgt_sym_mask[(lbl >> 6) & (ATP_TGT_MASK_WORDS - 1u)] |=
      1ull << (lbl & 63u);
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return;
  for (u32 i = 0; i < n; i++) atp_tgt_mask_collect(s, term_ctr_at(t, i));
}
// Collect the goal-symbol bitset lazily (union over every conjunct,
// like SOS's goal_sym_mask, plus the externally-declared goal-cone
// labels); thvm_atp_set_goals resets tgt_ready.
static void atp_tgt_ensure(AtpState *s) {
  if (s->tgt_ready) return;
  memset(s->tgt_sym_mask, 0, sizeof s->tgt_sym_mask);
  if (s->n_goals > 1u) {
    for (u32 g = 0; g < s->n_goals; g++) {
      atp_tgt_mask_collect(s, s->goals_lhs[g]);
      atp_tgt_mask_collect(s, s->goals_rhs[g]);
    }
  } else {
    if (s->goal_lhs) atp_tgt_mask_collect(s, s->goal_lhs);
    if (s->goal_rhs) atp_tgt_mask_collect(s, s->goal_rhs);
  }
  for (u32 i = 0; i < ATP_TGT_MASK_WORDS; i++)
    s->tgt_sym_mask[i] |= s->tgt_ext_mask[i];
  s->tgt_ready = 1u;
}
static u8 atp_tgt_armed(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0 || atp_tgt_env() == 0u) return 0u;
  atp_tgt_ensure(s);
  return 1u;
}

// Externally declare a goal-cone symbol label for the tgt scheme.  The
// FindProof CLI feeds the TPTP negated-conjecture units' symbols here:
// Vampire derives its tgt weights from the WHOLE negated conjecture,
// and on the machine-equationalized '-10' class the final goal
// equation is a bare `a != b` while the conjecture's content lives in
// its sibling hypothesis units (without this the multiplier is nearly
// uniform across CPs -- no reordering).  ORed into the live mask at
// the next lazy collection; inert while THVM_ATP_TGT_SEL is off.
fn void thvm_atp_tgt_add_sym(AtpState *s, u32 label) {
  if (s == NULL) return;
  s->tgt_ext_mask[(label >> 6) & (ATP_TGT_MASK_WORDS - 1u)] |=
      1ull << (label & 63u);
  s->tgt_ready = 0u;
}

// Weighted/plain counts -> multiplicative bias on `base`, saturating
// below the ATP_CP_PRI_NEVER sentinel like the ifeq scale.  plain == 0
// (sentinel/empty Term args, the FT-routed pushes) leaves the base
// unchanged; the FT twin below applies the exact span counts instead.
static u32 atp_tgt_scale_counts(u32 base, u32 gc, u32 oc, u32 vc) {
  u64 plain = (u64)gc + oc + vc;
  if (plain == 0ull) return base;
  u64 w = (u64)atp_tgt_nongoal();
  u64 scaled = (u64)gc + w * oc + ((atp_tgt_env() >= 2u) ? w * vc : (u64)vc);
  u64 b = (u64)(base == 0u ? 1u : base) * scaled / plain;
  if (b >= (u64)ATP_CP_PRI_NEVER) return ATP_CP_PRI_NEVER - 1u;
  return (u32)b;
}
static void atp_tgt_count_rec(const AtpState *s, Term t,
                              u32 *gc, u32 *oc, u32 *vc) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) { (*vc)++; return; }
  if (tg != TAG_CTR) return;
  if (atp_tgt_sym_hit(s, term_ext(t))) (*gc)++; else (*oc)++;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return;
  for (u32 i = 0; i < n; i++)
    atp_tgt_count_rec(s, term_ctr_at(t, i), gc, oc, vc);
}
static u32 atp_tgt_weight_scale(const AtpState *s, Term lhs, Term rhs,
                                u32 base) {
  u32 gc = 0u, oc = 0u, vc = 0u;
  atp_tgt_count_rec(s, lhs, &gc, &oc, &vc);
  atp_tgt_count_rec(s, rhs, &gc, &oc, &vc);
  return atp_tgt_scale_counts(base, gc, oc, vc);
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin of atp_tgt_weight_scale, for the routed pushes whose
// Term args are sentinels (the real content lives in the g_vcp_treat
// NF spans).  Same walk shape as atp_ifeq_weight_scale_ft's.
static u32 atp_tgt_weight_scale_ft(const AtpState *s, const AtpFtCell *fl,
                                   const AtpFtCell *fr, u32 base) {
  u32 gc = 0u, oc = 0u, vc = 0u;
  for (int side = 0; side < 2; side++) {
    const AtpFtCell *t = side ? fr : fl;
    if (t == NULL) continue;
    const AtpFtCell *stop = t->end;
    for (const AtpFtCell *x = t; ; x = x->next) {
      if (ft_is_var(x)) vc++;
      else if (atp_tgt_sym_hit(s, ft_ctr_sym(x))) gc++;
      else oc++;
      if (x == stop) break;
    }
  }
  return atp_tgt_scale_counts(base, gc, oc, vc);
}
#endif

// --- spb=goal / nwc: derivation-level goal bias ------------------------
//
// Vampire's shared GRP loop-theory winner carries spb=goal (goal-derived
// clauses get selection priority) and nwc=2 (non-goal clauses' selection
// weight doubled).  The symbol-level twin (--tgt-sel) is landed above;
// this is the DERIVATION-level analog: a per-CP bit (cp_gd) seeded on
// the conjecture-derived input equations (goal-cone tgt definitions +
// conjecture-role units, via thvm_atp_mark_gd_next), propagated
// parent-OR-wise through CP formation (g_cp_gd, the g_cp_formation_depth
// discipline) and rule birth (r_gd).  When THVM_ATP_SPB_GOAL is armed,
// every NON-goal-derived CP's priority is scaled by THVM_ATP_NWC
// (float, default 2.0) -- goal-derived CPs float, everything else sinks
// proportionally.  Selection bias only: nothing is dropped, ordering
// untouched, byte-identical when unarmed.

// THVM_ATP_SPB_GOAL: 0 off (default), 1 armed.
static u8 atp_spb_goal_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_SPB_GOAL");
    v = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return (u8)v;
}
// THVM_ATP_NWC (default 2.0): non-goal-derived CP weight multiplier,
// parsed once to fixed-point x16 (clamped to [1.0, 64.0]).
static u32 atp_nwc_x16(void) {
  static u32 v = 0u;
  if (v == 0u) {
    const char *e = getenv("THVM_ATP_NWC");
    double d = (e != NULL && e[0] != '\0') ? strtod(e, NULL) : 2.0;
    if (d < 1.0) d = 1.0;
    if (d > 64.0) d = 64.0;
    v = (u32)(d * 16.0 + 0.5);
    if (v < 16u) v = 16u;
  }
  return v;
}
// Priority scale: goal-derived (gd=1) CPs keep their base; everything
// else is multiplied by NWC.  Saturates below the never-select
// sentinel; NEVER-class priorities pass through untouched.
static u32 atp_spb_scale(u32 base, u8 gd) {
  if (gd || !atp_spb_goal_on() || base >= ATP_CP_PRI_NEVER) return base;
  u64 b = (u64)base * atp_nwc_x16() / 16u;
  if (b >= (u64)ATP_CP_PRI_NEVER) return ATP_CP_PRI_NEVER - 1u;
  return (u32)b;
}

// --- drc=off analog: keep demod-redundant folded lemmas ----------------
//
// Vampire's winner also runs drc=off (demodulation_redundancy_check):
// clauses that demodulation would discard as redundant are KEPT -- 25
// of its GRP678-1 proof steps unfold to x=x and 56 to axiom instances,
// i.e. the proof passes THROUGH lemmas completion join-discards.  Our
// discard sites are the push-time triviality/join gate and the pop-time
// join gate.  Cheapest faithful approximation: when THVM_ATP_DRC_OFF is
// armed and the fold band is registered (thvm_atp_set_fold_band, the
// sF*/sFlat* definitional symbols), a CP whose sides CARRY a folded
// symbol and differ syntactically is NOT dropped on a join verdict --
// the RAW (pre-normalize) pair is queued / oriented instead, so the
// folded-coherence rewrite content becomes rules.  Default off,
// byte-identical when unarmed.

// THVM_ATP_DRC_OFF: 0 off (default), 1 armed.
static u8 atp_drc_off_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_OFF");
    v = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return (u8)v;
}
// THVM_ATP_DRC_GD (default 1): restrict the drc keep to GOAL-DERIVED
// CPs (the spb=goal derivation bit).  Under tgt=3 every CP carries a
// folded symbol -- the bare fold test is vacuous and the keep lane
// floods (GRP678-1: 3.0M keeps/60s); the conjecture-descended cone is
// the discriminating signal.  =0 reverts to the pure fold test.
static u8 atp_drc_gd_gate(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_GD");
    v = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)v;
}
// THVM_ATP_DRC_PUSH (default 1): also keep at the push-time join gate.
// =0 keeps only at the pop (selection) gate -- the site closest to
// Vampire's activation-time forward-demodulation retention.
static u8 atp_drc_push_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_PUSH");
    v = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)v;
}
// THVM_ATP_DRC_MAXW (default 16, 0 = uncapped): raw-pair node cap on
// the keep.  Even deduped, the folded-redundant class is huge (GRP678:
// 1.1M distinct keeps/60s); the oracle proof's pass-through lemmas are
// TINY folded identities (sF-layer compositions), so the keep targets
// that shape.
static u32 atp_drc_maxw(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_MAXW");
    v = (e != NULL && e[0] != '\0') ? atoi(e) : 16;
    if (v < 0) v = 0;
  }
  return (u32)v;
}
static void atp_symbol_count_upto(Term t, u32 cap, u32 *acc);
static u8 atp_drc_size_ok_term(Term l, Term r) {
  u32 cap = atp_drc_maxw();
  if (cap == 0u) return 1u;
  u32 n = 0u;
  atp_symbol_count_upto(l, cap + 1u, &n);
  if (n <= cap) atp_symbol_count_upto(r, cap + 1u, &n);
  return (u8)(n <= cap);
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static u8 atp_drc_size_ok_ft(const AtpFtCell *l, const AtpFtCell *r) {
  u32 cap = atp_drc_maxw();
  if (cap == 0u) return 1u;
  u32 n = 0u;
  for (int side = 0; side < 2; side++) {
    const AtpFtCell *t = side ? r : l;
    if (t == NULL) continue;
    const AtpFtCell *stop = t->end;
    for (const AtpFtCell *x = t; ; x = x->next) {
      if (++n > cap) return 0u;
      if (x == stop) break;
    }
  }
  return 1u;
}
#endif
static u8 atp_drc_armed(const AtpState *s) {
  return (u8)(s != NULL && s->fold_hi > s->fold_lo && atp_drc_off_on());
}
// Does the Term carry a folded-band symbol?
static u8 atp_term_has_fold(const AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 lbl = term_ext(t);
  if (lbl >= s->fold_lo && lbl < s->fold_hi) return 1u;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return 0u;
  for (u32 i = 0; i < n; i++)
    if (atp_term_has_fold(s, term_ctr_at(t, i))) return 1u;
  return 0u;
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-span twin (prefix walk, the atp_tgt_weight_scale_ft shape).
static u8 atp_ft_has_fold(const AtpState *s, const AtpFtCell *t) {
  if (t == NULL) return 0u;
  const AtpFtCell *stop = t->end;
  for (const AtpFtCell *x = t; ; x = x->next) {
    if (!ft_is_var(x)) {
      u32 lbl = ft_ctr_sym(x);
      if (lbl >= s->fold_lo && lbl < s->fold_hi) return 1u;
    }
    if (x == stop) break;
  }
  return 0u;
}
// drc keep stash for the FT-routed treated class: vcp_treat_joinable
// normalizes the raw stash cells IN PLACE, so a drc candidate's raw
// pair is cloned here BEFORE the normalize; the push gate swaps the
// clones back into the stash on a keep (or frees them on a survivor).
static AtpFtCell *g_drc_raw_l = NULL;
static AtpFtCell *g_drc_raw_r = NULL;
#endif

// Keep-lane dedup: without it the keep floods with RE-DERIVATIONS of
// the same folded lemma (GRP678-1: 1.9M keeps / <5k distinct rules in
// 60s).  Each unique raw-pair structural hash is kept at most once per
// problem (thvm_atp_set_fold_band resets the table); collisions and
// probe overflow err on the ALLOW side.  Hashes are NOT var-canonical
// -- alpha-variant re-derivations may slip through, which only costs a
// duplicate keep.  THVM_ATP_DRC_DEDUP=0 disables (the flood A/B).
#define ATP_DRC_SEEN_BITS 20u
static u64 g_drc_seen[1u << ATP_DRC_SEEN_BITS];
static u8 atp_drc_dedup_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_DEDUP");
    v = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)v;
}
// 1 = this lemma hash was already kept (suppress); 0 = fresh (insert).
static u8 atp_drc_seen_probe(u64 h) {
  if (!atp_drc_dedup_on()) return 0u;
  if (h == 0ull) h = 1ull;
  u32 m = (1u << ATP_DRC_SEEN_BITS) - 1u;
  u32 i = (u32)(h ^ (h >> 32)) & m;
  for (u32 k = 0; k < 64u; k++, i = (i + 1u) & m) {
    if (g_drc_seen[i] == h) return 1u;
    if (g_drc_seen[i] == 0ull) {
      g_drc_seen[i] = h;
      return 0u;
    }
  }
  return 0u;   // probe overflow: allow the keep
}
static u64 atp_drc_pair_hash_term(Term l, Term r) {
  return atp_term_struct_hash(l) * 0x9e3779b97f4a7c15ull
       ^ atp_term_struct_hash(r);
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static u64 atp_drc_pair_hash_ft(const AtpFtCell *l, const AtpFtCell *r) {
  return ft_hash(l) * 0x9e3779b97f4a7c15ull ^ ft_hash(r);
}
#endif

fn void thvm_atp_mark_gd_next(AtpState *s) {
  (void)s;
  g_enq_gd_pending = 1u;
}

fn void thvm_atp_set_fold_band(AtpState *s, u32 lo, u32 hi) {
  if (s == NULL) return;
  s->fold_lo = lo;
  s->fold_hi = hi;
  // Fresh problem intake: reset the drc keep-lane dedup table.
  memset(g_drc_seen, 0, sizeof g_drc_seen);
}

fn void thvm_atp_set_fold_defs(AtpState *s, const u8 *arity,
                               const Term *body, u32 n) {
  if (s == NULL) return;
  if (n > ATP_NEEDLE_MAX_DEFS) n = ATP_NEEDLE_MAX_DEFS;
  for (u32 i = 0; i < n; i++) {
    s->fold_def_arity[i] = arity[i];
    s->fold_def_body[i]  = body[i];
  }
}

fn void thvm_atp_set_orig_axioms(AtpState *s, const Term *lhs,
                                 const Term *rhs, u32 n) {
  if (s == NULL) return;
  if (n > ATP_NEEDLE_MAX_AXIOMS) n = ATP_NEEDLE_MAX_AXIOMS;
  for (u32 i = 0; i < n; i++) {
    s->needle_ax_l[i] = lhs[i];
    s->needle_ax_r[i] = rhs[i];
  }
  s->n_needle_ax = n;
}

// --- needle: narrow the drc=off keep set to instance/x=x shapes ------
//
// Vampire's GRP678-1 proof passes THROUGH ~80 folded-redundant lemmas
// (demodulation_redundancy_check keeps them); wholesale drc=off (above)
// keeps every folded-and-joins CP -- a haystack (GRP678: 1.1M distinct
// keeps/60s even deduped).  Empirically those ~80 lemmas' sF-UNFOLDING
// (substitute every fold-band symbol back to the raw pre-tgt subterm
// it names, recursively) is always one of two shapes: an alpha-instance
// of one of the ORIGINAL axioms, or a trivial (syntactically-equal
// after canonical renumbering) x=x.  The needle keeps ONLY those.
//
// THVM_ATP_DRC_NEEDLE: 0 off (default), 1 armed.  Only takes effect
// where THVM_ATP_DRC_OFF is already armed (needle strictly NARROWS
// that keep set); off is byte-identical to plain drc=off.
static u8 atp_drc_needle_on(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_NEEDLE");
    v = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return (u8)v;
}
// THVM_ATP_DRC_NEEDLE_CAP (default 256, 0 = uncapped): node-count
// budget on the FULLY UNFOLDED pair (both sides combined).  Over-cap
// is a fail-closed drop (not kept) -- the oracle's pass-through lemmas
// are tiny folded identities, so a giant unfold is never the needle's
// target class, and bounding it keeps the per-candidate cost flat.
static u32 atp_drc_needle_cap(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_ATP_DRC_NEEDLE_CAP");
    v = (e != NULL && e[0] != '\0') ? atoi(e) : 256;
    if (v < 0) v = 0;
  }
  return (u32)v;
}

// Recursive unfold of a folded Term back to the original signature:
// bottom-up, substitute every fold-band root label with its registered
// definition body (the occurrence's own children bind the body's FVR
// 0..arity-1 placeholders via thvm_subst_apply).  Definition bodies are
// fold-free BY CONSTRUCTION (pr_tgt_transform's pr_tgt_subs[] are raw
// pre-transform subterms, never rewritten to mention fresher labels),
// so one substitution per occurrence fully resolves it; atp_term_has_fold
// on the result is a cheap defensive check, not a second unfold pass
// (a hit means a corrupt/mismatched def table, not a legitimate nested
// fold -- fails closed rather than looping).  *budget is a combined
// node-count ceiling shared across both CP sides (see
// atp_drc_needle_cap); each substitution's OWN result size is charged
// against it via atp_symbol_count_upto, so a substitution that blows up
// the term is caught even though thvm_subst_apply itself doesn't meter.
// Returns 0 on over-cap / a missing or arity-mismatched definition
// (the caller drops the candidate, same as if needle were never
// consulted); 1 with *out filled otherwise.
static u8 atp_drc_needle_unfold(const AtpState *s, Term t, u32 *budget,
                                Term *out) {
  if (*budget == 0u) return 0u;
  (*budget)--;
  if (term_tag(t) != TAG_CTR) { *out = t; return 1u; }
  u32 lbl = term_ext(t);
  u32 n   = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY) return 0u;
  Term kids[REWRITE_MAX_ARITY];
  u8 changed = 0u;
  for (u32 i = 0; i < n; i++) {
    Term ci = term_ctr_at(t, i);
    Term ko;
    if (!atp_drc_needle_unfold(s, ci, budget, &ko)) return 0u;
    if (ko != ci) changed = 1u;
    kids[i] = ko;
  }
  if (lbl >= s->fold_lo && lbl < s->fold_hi) {
    u32 di = lbl - s->fold_lo;
    Term def = (di < ATP_NEEDLE_MAX_DEFS) ? s->fold_def_body[di] : 0;
    if (def == 0 || s->fold_def_arity[di] != n) return 0u;
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    for (u32 i = 0; i < n; i++) sub.bindings[i] = kids[i];
    Term r = thvm_subst_apply(def, &sub);
    if (atp_term_has_fold(s, r)) return 0u;   // defensive; see above
    u32 rn = 0u;
    atp_symbol_count_upto(r, *budget + 1u, &rn);
    if (rn > *budget) return 0u;
    *budget -= rn;
    *out = r;
    return 1u;
  }
  *out = changed ? term_new_ctr(lbl, kids, n) : t;
  return 1u;
}

// (a) alpha-instance test: does the unfolded pair (ul, ur) match one of
// the registered pre-tgt axioms under a single one-way substitution,
// in either orientation ("axiom lhs->ul, axiom rhs->ur" or "axiom
// lhs->ur, axiom rhs->ul", the SAME subst across both sides of a try)?
static u8 atp_drc_needle_ax_match(const AtpState *s, Term ul, Term ur) {
  for (u32 i = 0; i < s->n_needle_ax; i++) {
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    if (thvm_match(s->needle_ax_l[i], ul, &sub) &&
        thvm_match(s->needle_ax_r[i], ur, &sub)) {
      return 1u;
    }
    memset(&sub, 0, sizeof sub);
    if (thvm_match(s->needle_ax_l[i], ur, &sub) &&
        thvm_match(s->needle_ax_r[i], ul, &sub)) {
      return 1u;
    }
  }
  return 0u;
}

// The needle gate: unfold both sides (shared budget), then keep iff
// (b) the unfolded pair is trivial (syntactically equal once
// canonically renumbered -- the general "x=x" shape) or (a) an
// axiom instance.  Telemetry counters record the verdict; over-cap
// unfolds count separately from a clean instance/x=x miss.
static u8 atp_drc_needle_ok(AtpState *s, Term l, Term r) {
  u32 cap = atp_drc_needle_cap();
  u32 budget = (cap == 0u) ? 0xFFFFFFFFu : cap;
  Term ul, ur;
  if (!atp_drc_needle_unfold(s, l, &budget, &ul) ||
      !atp_drc_needle_unfold(s, r, &budget, &ur)) {
    s->n_cps_drc_needle_overcap++;
    return 0u;
  }
  thvm_normalize_vars(&ul, &ur);
  if (kbo_eq(ul, ur) || atp_drc_needle_ax_match(s, ul, ur)) {
    s->n_cps_drc_needle_kept++;
    return 1u;
  }
  s->n_cps_drc_needle_dropped++;
  return 0u;
}

// Thin gate wrapper: needle off (the default) is a pure pass-through,
// so every drc=off call site composes it with one `&&` and pays
// nothing extra when THVM_ATP_DRC_NEEDLE is unset.
static u8 atp_drc_needle_gate(AtpState *s, Term l, Term r) {
  if (!atp_drc_needle_on()) return 1u;
  return atp_drc_needle_ok(s, l, r);
}

fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  atp_ifeq_detect(s, lhs, rhs);
  return atp_enqueue_equation(s, lhs, rhs, TRACE_AXIOM, ATP_TRACE_NONE);
}

// Forward decls (defined further down the file).
static u8 atp_push_rule(AtpState *s, Term lhs, Term rhs);

// Install a pre-oriented axiom directly as a rewrite rule, bypassing
// the engine's KBO orientation step.  The caller asserts `lhs -> rhs`
// is the intended direction; the engine uses it as-is.  Used for the
// WL Rule[lhs, rhs] axiom surface (TFindProof[..., {a == b, c -> d}]).
//
// Semantics:
//   * No KBO compare.  atp_push_rule still writes r_orient[] based on
//     KBO, but we OVERRIDE to 1 below so ordered-rewrite + indexing
//     treat the rule as forward-only.
//   * Decrements n_unorient back if KBO would have classified it as
//     unorientable (keeps the unorient bookkeeping accurate).
//   * Generates CPs against existing rules immediately, mirroring the
//     post-orient_and_add CP-gen step at thvm_atp_step's tail.
//
// Soundness: forcing an orientation that violates KBO does NOT break
// soundness -- the rewrite `lhs -> rhs` is still a valid equational
// consequence.  But it CAN break completion's termination guarantee:
// a non-decreasing rule may loop the rewriter.  The variable-safety
// check (vars(rhs) subset of vars(lhs)) at the WL surface catches the
// outright-unsafe case (introducing fresh vars); the still-unsafe
// case of a non-terminating direction is the user's responsibility.
fn AtpAddedRange thvm_atp_install_oriented_rule(AtpState *s, Term lhs,
                                                Term rhs) {
  AtpAddedRange r = {0, 0, 0};
  if (s == NULL) return r;
  u32 idx = s->n_rules;
  if (!atp_push_rule(s, lhs, rhs)) return r;
  // atp_push_rule may have rejected as duplicate -- if n_rules didn't
  // bump, nothing to override.
  if (s->n_rules == idx) return r;
  u32 i = s->n_rules - 1u;
  if (!s->r_orient[i]) {
    s->r_orient[i] = 1u;
    if (s->n_unorient > 0u) s->n_unorient--;
    // r_orient flip changes the rewrite system: a rule previously
    // applied two-faces now applies only its lhs face.  Bump the
    // revision the IR cookie keys on.
    s->r_revision++;
  }
  // Trace lineage: the select/orient path stamps every rule's r_trace
  // with a TRACE_ORIENT entry whose parent chains back to the source
  // axiom.  Mirror that here -- without it r_trace[i] stays
  // ATP_TRACE_NONE and every CP overlapping this rule records a NONE
  // parent, which the WL proof lifter indexes as trace[[2^32]] and the
  // ProofObject collapses to $Failed.
  u32 ax_t = atp_trace_push(s, TRACE_AXIOM, ATP_TRACE_NONE,
                            ATP_TRACE_NONE, lhs, rhs);
  if (ax_t != ATP_TRACE_NONE) {
    u32 or_t = atp_trace_push(s, TRACE_ORIENT, ax_t, ATP_TRACE_NONE,
                              s->lhs[i], s->rhs[i]);
    if (or_t != ATP_TRACE_NONE) s->r_trace[i] = or_t;
  }
  if (s->use_emission_order) atp_wmo_insert_fact(s, i);
  r.first = idx;
  r.count = 1;
  // Generate CPs between this newly-installed rule and the rules
  // installed earlier in this init batch.  At step 0 of the engine
  // this catches all the pre-init oriented rules so the saturation
  // can later overlap them with the equation-axiom rules.
  thvm_atp_generate_cps(s, r);
  return r;
}


// Re-queue a simplified older rule (interreduce path).  Records a
// TRACE_SIMPLIFY entry whose parent_a is the dropped rule's trace
// index, so a proof consumer can replay the reduction chain instead
// of treating the equation as an unjustified axiom.
static u8 atp_add_equation_simplified(AtpState *s, Term lhs, Term rhs,
                                      u32 parent_trace) {
  // Goal-connected requeues record their interreduce-time reduction as
  // NORM_STEP chains threaded between the parent and the TRACE_SIMPLIFY
  // entry.  With whole-run RecordNorm off this hop is otherwise the only
  // unrecorded rewrite in a goal chain, and the WL lift cannot re-derive
  // ordered reductions by search (OrAssociativity goal-CP requeue).  The
  // re-derivation runs against the CURRENT R -- the same rule set that
  // just produced (lhs, rhs) -- so the recorded chain lands on the same
  // pair; on any mismatch fall back to the unchained entry.
  return atp_enqueue_equation(s, lhs, rhs, TRACE_SIMPLIFY, parent_trace);
}

// Walk `t` and bit-set every TAG_CTR label encountered into `*mask`.
// Used by thvm_atp_set_goal to populate s->conj_sym_mask for the
// ATP_CP_WEIGHT_CONJSYM weight mode.
static void atp_collect_symbols(Term t, u64 *mask) {
  Term stack[256];
  u32  sp = 0;
  if (sp < 256) stack[sp++] = t;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) == TAG_CTR) {
      u32 lab = term_ext(cur);
      if (lab < 64u) *mask |= ((u64)1 << lab);
      u32 n = term_ctr_n(cur);
      for (u32 i = 0; i < n && sp < 256; i++) {
        stack[sp++] = term_ctr_at(cur, i);
      }
    }
  }
}

// Release the multi-goal arrays (set_goals re-set / clear / free).
static void atp_goals_release(AtpState *s) {
  free(s->goals_lhs);    s->goals_lhs    = NULL;
  free(s->goals_rhs);    s->goals_rhs    = NULL;
  free(s->goals_lhs_nf); s->goals_lhs_nf = NULL;
  free(s->goals_rhs_nf); s->goals_rhs_nf = NULL;
#ifdef THVM_ATPFT_RULES
  free(s->goals_lhs_ft);  s->goals_lhs_ft  = NULL;
  free(s->goals_rhs_ft);  s->goals_rhs_ft  = NULL;
  free(s->goals_nf_ft_l); s->goals_nf_ft_l = NULL;
  free(s->goals_nf_ft_r); s->goals_nf_ft_r = NULL;
#endif
  s->n_goals = 0;
  s->goals_joined_mask = 0;
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
// Returns 1 on success, 0 if 8.4d's sort-check rejected the goal.
// s2a (Vampire sine_to_age): per-symbol SInE level via the Hoder-
// Voronkov D-relation, seeded from the conjecture symbols.  gen(s) =
// number of (live) rules containing s (the conjecture counts once, as in
// tp_sine_pass); a rule C is triggered by a REACHED symbol s iff
// gen(s) <= gt or gen(s) <= tol*min_gen(C), where min_gen(C) is the least
// generality over C's gt-general symbols when that least is still rare
// (<= rc), else the plain least over all of C's symbols -- exactly the
// tp_sine_pass CLI trigger.  A symbol first reached at round k gets level
// k+1 (goal symbols are level 0); symbols never reached stay REMOTE.  The
// BFS grows `reach` per round (proper shortest-path distance, order-
// independent), so a common hub symbol like `mult` does NOT drag every
// axiom to level 1 -- the division/inverse axioms whose only rare symbol
// stays unreachable end up REMOTE, exactly the slice SInE selection would
// drop.  Called from thvm_atp_set_goals only when s2a_coeff > 0, so the
// whole pass never runs for the byte-identical default engine.
static void atp_compute_sine_levels(AtpState *s) {
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++)
    s->sym_slvl[i] = ATP_REL_LEVEL_REMOTE;
  if (s->goal_lhs == 0) return;
  u32 gt = 1u; double tol = 1.5; u32 rc = 64u;
  { const char *e = getenv("THVM_ATP_S2A_GT");
    if (e != NULL && e[0] != '\0') { long v = atol(e); gt = (v < 0) ? 0u : (u32)v; } }
  { const char *e = getenv("THVM_ATP_S2A_TOL");
    if (e != NULL && e[0] != '\0') { tol = atof(e); if (tol < 1.0) tol = 1.0; } }
  { const char *e = getenv("THVM_ATP_S2A_RC");
    if (e != NULL && e[0] != '\0') { long v = atol(e); rc = (v < 0) ? 0u : (u32)v; } }
  // Axiom source: the enqueue-time symbol-mask snapshot (the rule set is
  // still empty here -- input equations enter as queued CPs).
  const u64 *ax = s->s2a_ax_mask;
  u32 nax = s->n_s2a_ax;
  // Generality gen[s] = #(axioms + conjecture) containing s.
  u32 gen[WALD_MAX_SYMBOLS];
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++) gen[i] = 0u;
  for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++)
    if (s->conj_sym_mask & ((u64)1 << lab)) gen[lab]++;
  for (u32 i = 0; i < nax; i++)
    for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++)
      if (ax[i] & ((u64)1 << lab)) gen[lab]++;
  // Seed: conjecture symbols at level 0.
  for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++)
    if (s->conj_sym_mask & ((u64)1 << lab)) s->sym_slvl[lab] = 0u;
  u64 reach = s->conj_sym_mask;
  for (u32 round = 0; round < WALD_MAX_SYMBOLS; round++) {
    u64 next = 0;
    for (u32 i = 0; i < nax; i++) {
      u64 m = ax[i];
      if (!(m & reach)) continue;              // no reached symbol: no trigger
      u32 min_all = 0xffffffffu, min_ex = 0xffffffffu;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
        if (!(m & ((u64)1 << lab))) continue;
        u32 g = gen[lab];
        if (g < min_all) min_all = g;
        if (g > gt && g < min_ex) min_ex = g;
      }
      u32 mingen = (min_ex <= rc) ? min_ex : min_all;
      u8 hit = 0;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS && !hit; lab++) {
        if (!(m & ((u64)1 << lab))) continue;
        if (!(reach & ((u64)1 << lab))) continue;
        if (gen[lab] <= gt || (double)gen[lab] <= tol * (double)mingen) hit = 1;
      }
      if (!hit) continue;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
        if (!(m & ((u64)1 << lab))) continue;
        if (s->sym_slvl[lab] == ATP_REL_LEVEL_REMOTE) {
          s->sym_slvl[lab] = (u8)(round + 1u);
          next |= ((u64)1 << lab);
        }
      }
    }
    if (!next) break;
    reach |= next;
  }
  {
    const char *dmp = getenv("THVM_ATP_S2A_DUMP");
    if (dmp != NULL && dmp[0] != '\0') {
      u32 hist[ATP_S2A_REMOTE_LVL + 2];
      for (u32 i = 0; i < ATP_S2A_REMOTE_LVL + 2; i++) hist[i] = 0u;
      for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
        if (gen[lab] == 0u) continue;
        u32 lv = (s->sym_slvl[lab] == ATP_REL_LEVEL_REMOTE)
                   ? (ATP_S2A_REMOTE_LVL + 1u) : s->sym_slvl[lab];
        if (lv > ATP_S2A_REMOTE_LVL + 1u) lv = ATP_S2A_REMOTE_LVL + 1u;
        hist[lv]++;
        fprintf(stderr, "S2A sym lab=%u gen=%u level=%u\n",
                lab, gen[lab], s->sym_slvl[lab]);
      }
      fprintf(stderr, "S2A level histogram (coeff=%u):", s->s2a_coeff);
      for (u32 i = 0; i <= ATP_S2A_REMOTE_LVL + 1u; i++)
        if (hist[i]) fprintf(stderr, " L%u=%u%s", i, hist[i],
                             (i == ATP_S2A_REMOTE_LVL + 1u) ? "(REMOTE)" : "");
      fprintf(stderr, "\n");
    }
  }
}

// s2a: per-CP SInE level = max over the CP's TAG_CTR symbols of their
// sym_slvl, a REMOTE (goal-unreachable) symbol contributing
// ATP_S2A_REMOTE_LVL.  0 when s2a is off or no goal.  The FIFO/age
// selection adds s2a_coeff*this to the CP's cp_seq (goal-distant -> later).
static u32 atp_cp_sine_level(const AtpState *s, Term lhs, Term rhs) {
  if (s->s2a_coeff == 0u || s->goal_lhs == 0) return 0u;
  Term stack[256];
  u32 sp = 0;
  if (sp < 256) stack[sp++] = lhs;
  if (sp < 256) stack[sp++] = rhs;
  u32 best = 0u;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) == TAG_CTR) {
      u32 lab = term_ext(cur);
      u32 lvl = (lab < WALD_MAX_SYMBOLS &&
                 s->sym_slvl[lab] != ATP_REL_LEVEL_REMOTE)
                  ? (u32)s->sym_slvl[lab] : ATP_S2A_REMOTE_LVL;
      if (lvl > ATP_S2A_REMOTE_LVL) lvl = ATP_S2A_REMOTE_LVL;
      if (lvl > best) best = lvl;
      u32 n = term_ctr_n(cur);
      for (u32 i = 0; i < n && sp < 256; i++) stack[sp++] = term_ctr_at(cur, i);
    }
  }
  return best;
}

fn u8 thvm_atp_set_goal(AtpState *s, Term lhs, Term rhs) {
  if (s != NULL && lhs == 0) return thvm_atp_set_goals(s, NULL, NULL, 0);
  return thvm_atp_set_goals(s, &lhs, &rhs, 1);
}

// Set an n-goal conjunction: every pair (lhs[g], rhs[g]) must join for
// goal_check to report ATP_PROVED.  n == 0 clears the goal (completion
// mode); n == 1 is the single-conjecture case and behaves exactly as
// thvm_atp_set_goal always has.  The conjecture symbol mask and the
// relevance levels are computed over the UNION of all goals' symbols.
// Returns 1 on success, 0 on n > ATP_MAX_GOALS or any sort-check
// rejection (state unmodified on failure).
fn u8 thvm_atp_set_goals(AtpState *s, const Term *lhs, const Term *rhs,
                         u32 n) {
  if (s == NULL) return 0;
  if (n > ATP_MAX_GOALS) return 0;
  // Clearing the goal: n == 0 means "completion mode", always
  // accepted regardless of sort-check.
  if (n == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    s->conj_sym_mask = 0;
    s->rel_lvl1_mask = 0;
    s->tgt_ready = 0;
#ifdef THVM_ATPFT_RULES
    s->goal_lhs_ft = NULL;
    s->goal_rhs_ft = NULL;
    s->goal_nf_ft_l = NULL;
    s->goal_nf_ft_r = NULL;
#endif
    atp_goals_release(s);
    return 1;
  }
  // 8.4d: gate on sort-check when a spec is attached -- both
  // sides of every conjunct must be well-sorted AND share a sort.
  if (s->spec != NULL) {
    for (u32 g = 0; g < n; g++) {
      u32 sl = wald_term_sort(s->spec, lhs[g]);
      u32 sr = wald_term_sort(s->spec, rhs[g]);
      if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
        return 0;
      }
    }
  }
  atp_goals_release(s);
  s->goals_lhs    = (Term *)malloc((size_t)n * sizeof(Term));
  s->goals_rhs    = (Term *)malloc((size_t)n * sizeof(Term));
  s->goals_lhs_nf = (Term *)calloc(n, sizeof(Term));
  s->goals_rhs_nf = (Term *)calloc(n, sizeof(Term));
#ifdef THVM_ATPFT_RULES
  s->goals_lhs_ft  = (struct AtpFtCell **)calloc(n, sizeof(void *));
  s->goals_rhs_ft  = (struct AtpFtCell **)calloc(n, sizeof(void *));
  s->goals_nf_ft_l = (struct AtpFtCell **)calloc(n, sizeof(void *));
  s->goals_nf_ft_r = (struct AtpFtCell **)calloc(n, sizeof(void *));
#endif
  if (s->goals_lhs == NULL || s->goals_rhs == NULL ||
      s->goals_lhs_nf == NULL || s->goals_rhs_nf == NULL) {
    atp_goals_release(s);
    return 0;
  }
  for (u32 g = 0; g < n; g++) {
    s->goals_lhs[g] = lhs[g];
#ifdef THVM_ATPFT_RULES
    if (s->goals_lhs_ft != NULL && s->ft_arena_ptr != NULL) {
      s->goals_lhs_ft[g] =
          ft_from_term((AtpFt *)s->ft_arena_ptr, lhs[g], 0);
      s->goals_rhs_ft[g] =
          ft_from_term((AtpFt *)s->ft_arena_ptr, rhs[g], 0);
    }
#endif
    s->goals_rhs[g] = rhs[g];
  }
  s->n_goals = n;
  // ifeq guard-DECIDING registry (see atp_ifeq_guard_register): a
  // conjecture conjunct can carry ifeq-guarded content on the FOF
  // front-end's own encodings even when a pre-encoded '-10' file's
  // final goal is a bare `a != b` (see the tgt-sel goal-cone doc
  // above).  No-op unless use_ifeq_decide is armed.
  for (u32 g = 0; g < n; g++)
    atp_ifeq_guard_register(s, lhs[g], rhs[g]);
  // goal_lhs/goal_rhs alias the first (unjoined) goal -- the storage
  // every single-goal consumer and goal-directed heuristic reads.
  s->goal_lhs = lhs[0];
  s->goal_rhs = rhs[0];
#ifdef THVM_ATPFT_RULES
  // Stage 4: mirror the alias goal pair into the AtpFt arena.  Goals
  // are persistent (live as long as the conjecture); convert with
  // scratch=0.  Set-goal can be called multiple times (test setup,
  // existential mode flip); the previous goal cells become unreachable
  // in the arena but the slab pool never returns memory mid-state,
  // freed wholesale at thvm_atp_free.
  s->goal_lhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, lhs[0], 0);
  s->goal_rhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, rhs[0], 0);
  s->goal_nf_ft_l = NULL;
  s->goal_nf_ft_r = NULL;
#endif
  // Goal changed: the tgt goal-symbol bitset is recollected lazily.
  s->tgt_ready = 0;
  // Recompute the conjecture symbol mask for ATP_CP_WEIGHT_CONJSYM --
  // the union over every conjunct's symbols.
  s->conj_sym_mask = 0;
  for (u32 g = 0; g < n; g++) {
    atp_collect_symbols(lhs[g], &s->conj_sym_mask);
    atp_collect_symbols(rhs[g], &s->conj_sym_mask);
  }
  // Recompute the level-1 relevance mask for back-compat (legacy
  // CONJSYM consumers).  Now also recompute the per-symbol BFS level
  // array sym_level[] for ATP_CP_WEIGHT_RELLEVEL.  Skips dead rules.
  s->rel_lvl1_mask = 0;
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++) s->sym_level[i] = ATP_REL_LEVEL_REMOTE;
  for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
    if (s->conj_sym_mask & ((u64)1 << lab)) s->sym_level[lab] = 0u;
  }
  // BFS through the symbol/axiom graph: an axiom links every pair of
  // its symbols.  Mark a symbol at level k+1 if it co-occurs in some
  // axiom with any level-k symbol and is not yet marked at <=k.
  u64 frontier = s->conj_sym_mask;
  for (u8 level = 0; level < (u8)ATP_REL_LEVEL_MAX && frontier; level++) {
    u64 next_frontier = 0;
    for (u32 i = 0; i < s->n_rules; i++) {
      if (s->r_dead[i]) continue;
      u64 rule_syms = 0;
      atp_collect_symbols(s->lhs[i], &rule_syms);
      atp_collect_symbols(s->rhs[i], &rule_syms);
      if (rule_syms & frontier) {
        // Find labels in this axiom not yet marked.
        for (u32 lab = 0; lab < WALD_MAX_SYMBOLS; lab++) {
          if ((rule_syms & ((u64)1 << lab)) &&
              s->sym_level[lab] == ATP_REL_LEVEL_REMOTE) {
            s->sym_level[lab] = level + 1u;
            next_frontier |= ((u64)1 << lab);
            if (level == 0u) s->rel_lvl1_mask |= ((u64)1 << lab);
          }
        }
      }
    }
    frontier = next_frontier;
  }
  // s2a (Vampire sine_to_age): the generality-aware SInE level per symbol.
  // Computed when s2a drives selection OR when the GNN re-rank / graph
  // export needs the goal_closeness node feature -- decoupled from
  // selection (it only fills sym_slvl; atp_cp_sine_level still gates the
  // age bias on s2a_coeff).  Byte-identical default engine otherwise.
  s->goal_levels_ready = 0u;
  if (s->s2a_coeff > 0u || s->record_cp_graphs || s->gnn_rerank_period > 0u) {
    atp_compute_sine_levels(s);
    s->goal_levels_ready = 1u;
  }
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

// Attach a RpoConfig.  When non-NULL, atp_compare dispatches to
// thvm_rpo, which generalises LPO via per-symbol status (LEX or MUL).
// RPO takes precedence over LPO when BOTH are set, since RPO subsumes
// LPO via all-LEX status -- callers should set RPO alone for clarity.
fn void thvm_atp_set_rpo(AtpState *s, const RpoConfig *rpo) {
  if (s == NULL) return;
  s->rpo = rpo;
}

// Attach a WpoConfig.  WPO unifies KBO/LPO/RPO via weights + precedence
// + per-symbol status.  Highest-precedence ordering when multiple are
// set: WPO > RPO > LPO > KBO.  Callers should set exactly one.
fn void thvm_atp_set_wpo(AtpState *s, const WpoConfig *wpo) {
  if (s == NULL) return;
  s->wpo = wpo;
}

// Milestone 10: runtime gate for the MNF goal-directed front search.
// No-op effect unless the dylib was compiled with -DATP_MNF -- the
// field is always present, but goal_check only reads it inside its
// `#ifdef ATP_MNF` block.
fn void thvm_atp_set_use_mnf(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_mnf = on ? 1u : 0u;
}

// Runtime gate for the goal-narrow endgame (src/atp/goal_narrow.c).
// Effective only in ATP_ORDERED_REWRITE builds -- the field is always
// present, but goal_check consults it inside that guard.
fn void thvm_atp_set_use_goal_narrow(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_goal_narrow = on ? 1u : 0u;
}

fn void thvm_atp_set_use_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_ground_join = on ? 1u : 0u;
}

// Lazy (selection-time) ground-joinability.  Runs the expensive GJ check
// on the SELECTED, R+E-normalized CP instead of on every generated CP at
// push time -- Twee's redundancy discipline.  Default OFF (byte-identical
// engine); armed via the bench THVM_ATP_LAZY_GJ / FindProof
// --lazy-ground-join.  The push-side use_ground_join should stay off under
// this mode so ground-joinability is paid exactly once, at pop.
fn void thvm_atp_set_use_lazy_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_lazy_ground_join = on ? 1u : 0u;
}

// Bachmair-Dershowitz connectedness CP deletion (Twee section 6.2).
// Default OFF -> the engine is byte-identical; the WL surface flips it
// for Method -> {... "Connectedness" -> True} and the Waldmeister preset.
fn void thvm_atp_set_use_connectedness(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_connectedness = on ? 1u : 0u;
}

// Waldmeister-faithful RHS interreduction (Interreduktion.c
// RMRechtsInterred).  See the AtpState.use_rhs_interreduce comment.
fn void thvm_atp_set_use_rhs_interreduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_rhs_interreduce = on ? 1u : 0u;
}

// Unfailing-completion both-faces superposition.  See the
// AtpState.use_unfailing_cp comment.
fn void thvm_atp_set_use_unfailing_cp(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_unfailing_cp = on ? 1u : 0u;
}

fn void thvm_atp_set_use_flatterm(AtpState *s, u8 on) {
  if (s == NULL) return;
#ifdef ATP_RULE_INDEX
  s->use_flatterm = on ? 1u : 0u;
#else
  (void)on;   // no flat machinery without the rule index
#endif
}

fn void thvm_atp_set_use_cp_index(AtpState *s, u8 on) {
  if (s == NULL) return;
#ifdef ATP_RULE_INDEX
  s->use_cp_index = on ? 1u : 0u;
#else
  (void)on;   // no index machinery without the rule index
#endif
}

fn void thvm_atp_set_selection_ratio(AtpState *s, u32 modulo) {
  if (s == NULL) return;
  s->fifo_modulo = modulo;   // 0 -> default (11) at selection time
}

// Vampire-style random CP-selection mode.  `modulo == 0` disables (engine
// byte-identical); `modulo > 0` makes every nth selection pick a
// uniformly-random queued CP via the deterministic xorshift64 stream
// seeded by `thvm_atp_set_random_seed`.  Pairs with the heap-min default
// the same way `fifo_modulo` does.
fn void thvm_atp_set_random_modulo(AtpState *s, u32 modulo) {
  if (s == NULL) return;
  s->random_modulo = modulo;
}
fn void thvm_atp_set_random_seed(AtpState *s, u64 seed) {
  if (s == NULL) return;
  // xorshift64 forbids state 0; coerce to a nonzero default if the
  // caller passes 0 (which means "default seed" rather than "no
  // randomness"; randomness itself is gated on random_modulo > 0).
  s->rng_state = seed ? seed : 0x9E3779B97F4A7C15ull;
}

// Select the CP-priority weight mode (an `AtpCpWeightMode` value).
// Out-of-range values clamp to ATP_CP_WEIGHT_ADD (0) so a garbage
// mode falls back to the bare symbol-count heuristic.
fn void thvm_atp_set_cp_weight_mode(AtpState *s, u32 mode) {
  if (s == NULL) return;
  s->cp_weight_mode = (mode < ATP_CP_WEIGHT_LAST)
                    ? (u8)mode
                    : (u8)ATP_CP_WEIGHT_ADD;
}

fn void thvm_atp_set_max_cp_weight(AtpState *s, u32 w) {
  if (s != NULL) s->max_cp_weight = w;
}

fn void thvm_atp_set_max_cp_side(AtpState *s, u32 w) {
  if (s != NULL) s->max_cp_side = w;
}

fn void thvm_atp_set_max_cp_queue(AtpState *s, u32 n) {
  if (s != NULL) s->max_cp_queue = n;
}

// Enable the automatic, completeness-preserving growing CP-weight
// bound.  `base` seeds the bound; slope (default 2) scales it by the
// deepest current rule LHS so the bound tracks how complex the rule
// set has become.  base==0 disables (the historical unbounded engine).
fn void thvm_atp_set_auto_max_cp_weight(AtpState *s, u32 base) {
  if (s == NULL) return;
  s->auto_max_cp_weight_base  = base;
  s->auto_max_cp_weight_slope = 2u;
  s->auto_max_cp_weight_cur   = base;   // grown lazily as rules deepen
}

fn void thvm_atp_set_goal_interleave(AtpState *s, u32 ratio) {
  if (s != NULL) s->use_goal_interleave = ratio;
}

// s2a (Vampire sine_to_age): arm the goal-distance age postponement with
// coefficient `coeff` (0 disables, engine byte-identical).  Recomputes the
// per-symbol SInE levels now when a goal is already attached; set_goals
// recomputes them too when the goal is (re)set afterwards.
fn void thvm_atp_set_s2a(AtpState *s, u32 coeff) {
  if (s == NULL) return;
  s->s2a_coeff = coeff;
  if (coeff > 0u && s->goal_lhs != 0) atp_compute_sine_levels(s);
}

// 8.5c: order-aware compare.  Picks LPO (if attached) or KBO,
// returning a unified KboCmp-shaped result.  The two enums share
// numeric values (EQ=0, GT=1, LT=-1, UN=2), so the cast is safe.
//
// Stage 8: a `THVM_ATP_LPO_ORIENT_CACHE` wrapper sits on top of this
// body (see below) and memoises the verdict by structural-hash pair.
// WM Col3 combinator ordering (Ordnungen.c:101-117 Col3Test /
// Col3Vergleich + TermOperationen.c:1284-1315 IstKombinatorTupel).
// s > t iff
//   (a) s is a combinator redex `C x1 .. xn` on the apply spine:
//       peeling `apply` off s's LEFT spine reaches a constant head C,
//       and the spine arguments x1..xn are pairwise-DISTINCT
//       variables; and t is built ONLY from the apply operator plus
//       variables occurring in s -- NO constants at all
//       (SO_SymbIstFkt covers arity-0 symbols, so WM's t-walk rejects
//       any constant != apply: the Y fixpoint contractum x(Yx)
//       carries Y and stays UNORIENTABLE, kept as E); or
//   (b) s is a bare constant and t is ground (WM guards TRUE/FALSE
//       under existential goals; the equational bench has none).
// Not a simplification order: combinator reduction duplicates
// variables.  Completion under it relies on the same step caps WM's
// KombS strategy does.
static int atp_col3_tuple(Term s, Term t, u32 apply) {
  Term args[64];
  u32 n = 0;
  if (!(term_tag(s) == TAG_CTR && term_ext(s) == apply &&
        term_ctr_n(s) == 2u)) {
    return 0;
  }
  Term cur = s;
  while (term_tag(cur) == TAG_CTR && term_ext(cur) == apply &&
         term_ctr_n(cur) == 2u) {
    if (n >= 64u) return 0;
    args[n++] = term_ctr_at(cur, 1);
    cur = term_ctr_at(cur, 0);
  }
  if (!(term_tag(cur) == TAG_CTR && term_ctr_n(cur) == 0u)) return 0;
  for (u32 i = 0; i < n; i++) {
    if (term_tag(args[i]) != TAG_FVR) return 0;
    for (u32 j = i + 1u; j < n; j++) {
      if (term_ext(args[i]) == term_ext(args[j])) return 0;
    }
  }
  // t's preorder: every function symbol (constants included) must be
  // the apply operator; variables must be among the spine args.
  Term stk[256];
  u32 sp = 0u;
  stk[sp++] = t;
  while (sp > 0u) {
    Term c = stk[--sp];
    if (term_tag(c) == TAG_FVR) {
      u32 id = term_ext(c);
      u32 ok = 0u;
      for (u32 i = 0; i < n; i++) {
        if (term_ext(args[i]) == id) { ok = 1u; break; }
      }
      if (!ok) return 0;
      continue;
    }
    if (term_tag(c) != TAG_CTR) return 0;
    if (term_ext(c) != apply || term_ctr_n(c) == 0u) return 0;
    u32 cn = term_ctr_n(c);
    if (sp + cn > 256u) return 0;
    for (u32 i = 0; i < cn; i++) stk[sp++] = term_ctr_at(c, i);
  }
  return 1;
}

static int atp_term_is_ground(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return 0;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!atp_term_is_ground(term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

static int atp_col3_test(Term s, Term t, u32 apply) {
  if (atp_col3_tuple(s, t, apply)) return 1;
  if (term_tag(s) == TAG_CTR && term_ctr_n(s) == 0u &&
      atp_term_is_ground(t)) {
    return 1;
  }
  return 0;
}

static KboCmp atp_col3(Term lhs, Term rhs, u32 apply) {
  if (kbo_eq(lhs, rhs)) return KBO_EQ;
  if (atp_col3_test(lhs, rhs, apply)) return KBO_GT;
  if (atp_col3_test(rhs, lhs, apply)) return KBO_LT;
  return KBO_UN;
}

// Classification probe for the -auto mirror (the bench's
// Kombinatorlogik detection): is (a, b) a combinator-definition
// equation in either direction under `apply`?
fn u8 thvm_atp_col3_pair(Term a, Term b, u32 apply) {
  return (atp_col3_tuple(a, b, apply) || atp_col3_tuple(b, a, apply))
             ? 1u : 0u;
}

// With the flag off, the macro alias below collapses back to this
// function and the default build is byte-identical.
static KboCmp atp_compare_uncached(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  if (s->col3_apply != 0u) return atp_col3(lhs, rhs, s->col3_apply);
#ifdef THVM_ATP_AC
  // AC-aware ordering: when an AC mask is registered, route through
  // atp_kbo_ac / atp_lpo_ac.  They canonicalize both terms modulo
  // AC and apply the syntactic ordering to the canonical forms, so
  // the verdict is AC-invariant by construction.  Without this,
  // AC-permuted siblings flip orientation on every superposition
  // step and the trajectory blows up.
  u64 ac_mask = thvm_atp_get_ac_mask();
  if (ac_mask != 0ull) {
    AtpAcInfo ac = { .ac_mask = ac_mask };
    // WPO/RPO under AC: canonicalize both terms modulo AC, then apply
    // the syntactic ordering.  WPO > RPO > LPO precedence among set
    // configs; callers should set exactly one.
    if (s->wpo != NULL) {
      AtpAcInfo ac2 = ac;
      Term cl = atp_ac_canon(lhs, &ac2);
      Term cr = atp_ac_canon(rhs, &ac2);
      return (KboCmp)thvm_wpo(cl, cr, s->wpo);
    }
    if (s->rpo != NULL) {
      AtpAcInfo ac2 = ac;
      Term cl = atp_ac_canon(lhs, &ac2);
      Term cr = atp_ac_canon(rhs, &ac2);
      return (KboCmp)thvm_rpo(cl, cr, s->rpo);
    }
    if (s->lpo != NULL) {
      return (KboCmp)atp_lpo_ac(lhs, rhs, s->lpo, &ac);
    }
    return atp_kbo_ac(lhs, rhs, s->kbo, &ac);
  }
#endif
  if (s->wpo != NULL) {
    return (KboCmp)thvm_wpo(lhs, rhs, s->wpo);
  }
  if (s->rpo != NULL) {
    return (KboCmp)thvm_rpo(lhs, rhs, s->rpo);
  }
  if (s->lpo != NULL) {
    return (KboCmp)thvm_lpo(lhs, rhs, s->lpo);
  }
#ifdef ATP_RULE_INDEX
  // Optional flatterm KBO: thvm_kbo_flat runs the identical Loechner
  // decision over a cache-dense pre-order node array.  Measured net-
  // NEGATIVE on this engine: thvm's CTR cells already store their
  // children contiguously (term_ctr_at = heap_read(base + i)), so the
  // IC traversal is itself cache-friendly and the flatten-both-operands
  // pass is pure overhead.  The win that DID land is in thvm_kbo's
  // traversal (one child-base read per node instead of a redundant
  // term_ctr_n per child access), so the IC comparator is the default
  // fast path.  thvm_kbo_flat stays opt-in (THVM_ATP_KBO_FLAT=1) behind
  // use_flatterm for A/B and the differential self-check; unset / "0" =
  // the optimized IC KBO.  Cached once; -1 = unread.
  static int kbo_flat_gate = -1;
  if (kbo_flat_gate < 0) {
    const char *e = getenv("THVM_ATP_KBO_FLAT");
    kbo_flat_gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  if (s->use_flatterm && kbo_flat_gate) {
    return thvm_kbo_flat(lhs, rhs, s->kbo);
  }
#endif
  return thvm_kbo(lhs, rhs, s->kbo);
}

// Stage 8 wrapper: structural-hash-keyed verdict cache.  See
// src/atp/lpo_cache.c for the storage + invalidation.  With the flag
// off this collapses to a direct call -- default build byte-identical.
#ifdef THVM_ATP_LPO_ORIENT_CACHE
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  u64 lh = atp_term_struct_hash(lhs);
  u64 rh = atp_term_struct_hash(rhs);
  KboCmp cached;
  int hit = atp_lpo_orient_cache_get(lh, rh, &cached);
  if (hit && !atp_lpo_orient_verify_gate()) return cached;
  KboCmp fresh = atp_compare_uncached(s, lhs, rhs);
  if (hit && cached != fresh) {
    char lb[2048], rb[2048];
    atp_pretty_term(lhs, lb, sizeof lb);
    atp_pretty_term(rhs, rb, sizeof rb);
    fprintf(stderr, "ATP_LPO_ORIENT_CACHE MISMATCH cached=%d fresh=%d\n"
                    " lh=0x%016llx rh=0x%016llx\n lhs=%s\n rhs=%s\n",
            (int)cached, (int)fresh,
            (unsigned long long)lh, (unsigned long long)rh, lb, rb);
    abort();
  }
  if (!hit) atp_lpo_orient_cache_put(lh, rh, fresh);
  return fresh;
}
#else
static inline KboCmp atp_compare(AtpState *s, Term lhs, Term rhs) {
  return atp_compare_uncached(s, lhs, rhs);
}
#endif

#if defined(ATP_ORDERED_REWRITE) || defined(ATP_MNF)
// === variable-occurrence helpers ====================================
// Shared by 9c ordered rewriting (variable-safe rewrite directions)
// and the Milestone-10 MNF search (variable-safe backward steps).

// Does variable id `id` occur in `t`?
static int atp_term_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (atp_term_has_var(term_ctr_at(t, i), id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}
// Every variable of `a` also occurs in `b`?  A rewrite direction is
// variable-safe only when the result side's variables are contained
// in the matched side's -- else the rewrite would introduce variables.
static int atp_vars_contained(Term a, Term b) {
  switch (term_tag(a)) {
    case TAG_FVR: return atp_term_has_var(b, term_ext(a));
    case TAG_CTR: {
      u32 n = term_ctr_n(a);
      for (u32 i = 0; i < n; i++) {
        if (!atp_vars_contained(term_ctr_at(a, i), b)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}
#endif /* ATP_ORDERED_REWRITE || ATP_MNF */

#ifdef ATP_ORDERED_REWRITE
// === 9c-foundation: ordered rewriting ===============================
//
// Proper unfailing-completion rewriting.  The KBO_UN both-ways hack
// stored an unorientable equation u=v as two looping rules u->v and
// v->u.  Here an equation is stored once and the rewrite step tries
// every rule in BOTH directions, applying a direction only when the
// result strictly decreases the redex in the reduction order.  An
// oriented rule l->r (l > r, hence l.sigma > r.sigma for every sigma)
// fires forward only; an unorientable equation fires whichever
// direction is decreasing for the instance at hand.  Every rewrite
// strictly descends a well-founded order, so normalization terminates.

// One ordered rewrite at the top of `t`.
//
// An oriented rule (lhs[i] > rhs[i]) is decreasing for every instance,
// so it fires forward with NO order check and NO discarded `repl` --
// the same cost as the plain rewriter.  Only an unorientable equation
// pays the both-directions order-gated path: each direction is tried
// only when variable-safe and applied only when it strictly decreases
// `t`.  This fast path matters: without it, building a `repl` per
// failed order-check on every rule churns the heap catastrophically.
// Set by the mixed normalize loop: after the indexed (orientable)
// fixpoint, oriented rules cannot fire, so the linear step skips them
// and only tries the unorientable equations.
static u8 g_atp_skip_oriented = 0u;

// Forward decl -- defined with the FVI helpers near
// thvm_atp_orient_and_add.
static Term atp_grounded_instance(AtpState *s, Term t, Term lhs,
                                  Term min_const);

// === WM free-variable-instance ordered rewriting ====================
//
// Waldmeister rewrites with an unorientable equation even when the
// replacement side has EXTRA free variables: `GleichungsrichtungPasst`
// (INF/MatchOperationen.c:923-940) matches one direction, then fires
// with `TP_RechteSeiteUnfrei` -- the instance whose extension
// variables are bound to the minimal constant (`RechtsUnfreiErzeugen`,
// INF/RUndEVerwaltung.c:366-397) -- under the usual strict-decrease
// gate.  Soundness: to[extras -> c_min] = from is an instance of the
// universally quantified equation, so replacing sigma(from) by
// sigma(to[extras -> c_min]) is plain equational reasoning;
// termination stays with the per-rewrite KBO_GT gate.  WM gates the
// mechanism on a ground-total reduction order (`FreieVariablenOK`,
// INF/MatchOperationen.c:247: ORD_OrdnungTotal = SO_PraezedenzTotal
// for kbo/lpo, FALSE for every other ordering); mirror that by
// allowing KBO/LPO -- total precedence by KboConfig/LpoConfig
// contract -- and refusing WPO/RPO.
//
// Can direction from -> to fire at all?  Insertion-time filter for
// the unorientable-faces index.
static int atp_unorient_dir_usable(AtpState *s, Term from, Term to) {
  return atp_vars_contained(to, from)
      || (s->wpo == NULL && s->rpo == NULL);
}
// Replacement template for direction from -> to: `to` itself when the
// direction introduces no variables, otherwise the grounded WM
// free-variable instance (extras bound to s->min_const), or 0 when
// the direction is unusable (non-ground-total order).
static Term atp_unorient_template(AtpState *s, Term from, Term to) {
  if (atp_vars_contained(to, from)) return to;
  if (s->wpo != NULL || s->rpo != NULL) return 0;
  return atp_grounded_instance(s, to, from, s->min_const);
}

// Snapshot-array orientation memo: a normalize against a rule array
// OTHER than the live set (interreduce's new-rule slice, backward
// demodulation's snapshot) has no r_orient cache, so
// atp_ordered_try_top recomputed atp_compare(lhs[i], rhs[i]) at EVERY
// position of every subject -- the entire kbo_compare_core presence in
// the LAT138-1 / RNG021-6 profiles.  The verdict is a pure function of
// the two term VALUES under the run's fixed ordering, so memoize it on
// the (lhs, rhs) handle pair.  Direct-mapped; entries are stamped by
// the gc epoch (a collection can recycle handles) and by an init
// generation (one process can build many engines with different
// orderings -- test harnesses do), so a stale hit is impossible and
// every returned verdict is exactly the atp_compare the scan ran.
enum { ATP_ORIENT_MEMO_N = 2048 };
typedef struct { Term l, r; u64 epoch; u64 gen; u8 gt; u8 valid; } AtpOrientMemoEnt;
static AtpOrientMemoEnt g_atp_orient_memo[ATP_ORIENT_MEMO_N];

static u8 atp_orient_memo_gt(AtpState *s, Term l, Term r) {
  u64 h = ((u64)l * 0x9e3779b97f4a7c15ull) ^ ((u64)r * 0xff51afd7ed558ccdull);
  AtpOrientMemoEnt *e = &g_atp_orient_memo[(u32)(h >> 48) & (ATP_ORIENT_MEMO_N - 1u)];
  if (e->valid && e->epoch == g_atp_gc_epoch &&
      e->gen == g_atp_orient_memo_gen && e->l == l && e->r == r) {
    return e->gt;
  }
  u8 gt = (u8)(atp_compare(s, l, r) == KBO_GT);
  e->l = l;
  e->r = r;
  e->epoch = g_atp_gc_epoch;
  e->gen   = g_atp_orient_memo_gen;
  e->gt = gt;
  e->valid = 1u;
  return gt;
}

static Term atp_ordered_try_top(AtpState *s, Term t,
                                const Term *lhs, const Term *rhs,
                                u32 n_rules, u8 *fired) {
  // A rule's orientation is fixed at creation; for the live rule set it
  // is cached in s->r_orient.  Recomputing it here -- a full KBO compare
  // per rule, per rewrite position -- was ~70% of the completion wall.
  // A custom rule array (interreduction's 2-rule set) is not the live
  // set, so it falls back to the direct compare.
  const u8 *orient = (lhs == s->lhs && rhs == s->rhs) ? s->r_orient : NULL;
  // Top-symbol fast-fail: a CTR pattern cannot match a non-CTR subject
  // (FVR / atom).  Saves the function-call + 16-binding stack-init cost
  // of thvm_match on the common non-matching case.
  u32 t_tag = term_tag(t);
  u32 t_sym = (t_tag == TAG_CTR) ? term_ext(t) : 0u;
  u32 t_n   = (t_tag == TAG_CTR) ? term_ctr_n(t) : 0u;
  // WM per-position redex priority (BL_RegelOderGleichungAngewendet,
  // NF/NFBildung.c:503-531; path-based NFB_ variant :219-238): at this
  // position the rule tree (Regelbaum) is consulted BEFORE the equation
  // tree (Gleichungsbaum) -- an unorientable equation fires only when
  // NO oriented rule matches here.  Pass 1: oriented rules, index order.
  if (!g_atp_skip_oriented) {   // skip flag: already at indexed fixpoint
    for (u32 i = 0; i < n_rules; i++) {
      u8 oriented = orient ? orient[i]
                           : atp_orient_memo_gt(s, lhs[i], rhs[i]);
      if (!oriented) continue;
      // Top-symbol pre-filter: rule lhs must agree on tag, label, arity
      // for thvm_match to succeed.  FVR pattern can match anything --
      // skip the filter then.  Eliminates the recursive entry on the
      // hot fail-fast path; the descent into children fires only on
      // genuine top matches.
      Term li = lhs[i];
      if (term_tag(li) == TAG_CTR) {
        if (t_tag != TAG_CTR || term_ext(li) != t_sym ||
            term_ctr_n(li) != t_n) continue;
      }
      // oriented rule -- forward only, no order check, no waste.
      RewriteSubst subst = {{0}};
      if (atp_match_maybe_ac(li, t, &subst)) {
        *fired = 1;
        return thvm_subst_apply(rhs[i], &subst);
      }
#ifdef THVM_ATP_AC
      // Bachmair-Plaisted extended rewriting: for an oriented AC-top
      // rule l -> r whose LHS doesn't match the whole goal as-is, try
      // the extended form l_ext = f(l, z) -> f(r, z).  z absorbs the
      // leftover AC leaves so a rule like m(x, x) -> x fires on a
      // goal m(a, m(a, b)) (AC-flat {a, a, b}) -- matches the
      // embedded {a, a} sub-multiset, returns the rebuilt term with
      // the remaining {b} intact.  Closes the AC subset-match gap
      // surfaced by tests/test_atp_ac_bench's bool-idem-embed.
      {
        u64 acm = thvm_atp_get_ac_mask();
        if (acm != 0ull
            && term_tag(li) == TAG_CTR
            && term_ext(li) < 64u
            && ((acm >> term_ext(li)) & 1ull) != 0ull
            && t_tag == TAG_CTR
            && term_ext(t) == term_ext(li)) {
          AtpAcInfo ac = { .ac_mask = acm };
          Term ext_li = 0, ext_ri = 0;
          if (atp_ac_extend_rule(li, rhs[i], &ac, &ext_li, &ext_ri)) {
            RewriteSubst esubst = {{0}};
            if (atp_match_maybe_ac(ext_li, t, &esubst)) {
              *fired = 1;
              return thvm_subst_apply(ext_ri, &esubst);
            }
          }
        }
      }
#endif
    }
  }
  // Pass 2: unorientable equations -- reached only when no oriented
  // rule matched at this position.
  for (u32 i = 0; i < n_rules; i++) {
    u8 oriented = orient ? orient[i]
                         : atp_orient_memo_gt(s, lhs[i], rhs[i]);
    if (oriented) continue;
    // Unorientable equation -- both directions, order-gated.  Match
    // FIRST, then pick the replacement template: at most positions the
    // rule has no redex, so the cheap fail-fast thvm_match avoids the
    // full-term atp_unorient_template walk (vars-containment is a
    // measured hot leaf), and the template does not depend on the
    // redex.  A direction whose replacement side carries extension
    // variables fires with the grounded WM free-variable instance.
    Term li = lhs[i];
    Term ri = rhs[i];
    if (term_tag(li) == TAG_CTR &&
        (t_tag != TAG_CTR || term_ext(li) != t_sym ||
         term_ctr_n(li) != t_n)) {
      // l side cannot match -- skip both directions' l->r path; the
      // r->l path may still apply if its top (term_tag(ri)) matches.
    } else {
      RewriteSubst subst = {{0}};
      if (atp_match_maybe_ac(li, t, &subst)) {             // l -> r
        Term tmpl = atp_unorient_template(s, li, ri);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &subst);
          if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
        }
      }
    }
    if (term_tag(ri) == TAG_CTR &&
        (t_tag != TAG_CTR || term_ext(ri) != t_sym ||
         term_ctr_n(ri) != t_n)) {
      continue;
    }
    {
      RewriteSubst subst = {{0}};
      if (atp_match_maybe_ac(ri, t, &subst)) {             // r -> l
        Term tmpl = atp_unorient_template(s, ri, li);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &subst);
          if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
        }
      }
    }
  }
  *fired = 0;
  return t;
}

// One outermost-leftmost ordered rewrite anywhere in `t`: tries the
// top, else descends into TAG_CTR children left-to-right.
static Term atp_ordered_rewrite_step(AtpState *s, Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u8 *fired) {
  // DIAG (THVM_ATP_NORMLOG): trip when a normalize subject grows large, to
  // locate the rule-250 lazy-normalize blowup (grow-per-step vs many-steps).
  {
    static int dl = -1; static u32 mx = 0u;
    if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
    if (dl) { u32 sz = atp_symbol_count(t);
      if (sz > mx && sz > 5000u) { mx = sz;
        fprintf(stderr, "NORMLOG t_size=%u\n", sz); } }
  }
  Term top = atp_ordered_try_top(s, t, lhs, rhs, n_rules, fired);
  if (*fired) return top;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n; i++) {
      u8 cf = 0;
      Term nch = atp_ordered_rewrite_step(s, term_ctr_at(t, i),
                                          lhs, rhs, n_rules, &cf);
      if (cf) {
        Term children[REWRITE_MAX_ARITY];
        for (u32 j = 0; j < n; j++) {
          children[j] = (j == i) ? nch : term_ctr_at(t, j);
        }
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

// Ordered normalization to fixpoint.  step_cap is a safety bound only
// -- ordered rewriting genuinely terminates.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Forward decl: defined far below (with atp_cp_trivially_joinable).  Needed
// here so the use_mixmost_nf reroute can repopulate the FT mirror on demand.
static inline void atp_ft_mirror_ensure(AtpState *s);
#endif
// Forward decl: defined far below.  Needed here so the lazy (pop-time)
// ground-joinability drop in thvm_atp_step can call it before its
// definition (the push-side caller sits after it).
static int atp_cp_ground_joinable(AtpState *s, Term lhs, Term rhs);
// Forward decl: the Twee Join.hs pop-time ground-joinability case-split
// (defined far below with the gj machinery).  The pop-time keep/drop
// gate in thvm_atp_step calls it before its definition.
static u8 atp_pop_gj_joinable(AtpState *s, Term l, Term r);
static Term atp_rewrite_normalize_ordered(AtpState *s, Term t,
                                          const Term *lhs, const Term *rhs,
                                          u32 n_rules, u32 step_cap) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // use_mixmost_nf (the WM "Waldmeister" preset) demands WM's innermost
  // NormalformInnermost order at EVERY normalize site.  The Term-side
  // scan below is outermost-leftmost (top-first); that order divergence
  // both over-joins critical pairs WM keeps (the goal-closer is lost) and
  // over-generates ones WM joins (the queue explodes to millions).  Route
  // the live-rule-set call through the FT mixmost normalizer -- the same
  // one the goal-check uses -- so the whole engine matches WM byte-for-
  // byte.  Only the live rule set has an FT mirror, so snapshot calls
  // (different lhs/rhs/n_rules) fall through to the Term path unchanged.
  if (s->use_mixmost_nf && lhs == s->lhs && rhs == s->rhs &&
      n_rules == s->n_rules) {
    atp_ft_mirror_ensure(s);
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fc = atp_rewrite_normalize_ft(s, ft_from_term(a, t, 0),
                                             step_cap);
    return ft_to_term(fc);
  }
#endif
#ifdef ATP_RULE_INDEX
  // When EVERY rule in R is KBO-oriented (lhs > rhs), every forward
  // rewrite is order-decreasing, so the discrimination-tree normalizer
  // is an exact equivalent of the linear ordered scan (same outermost-
  // leftmost redex, same lowest-index rule) and replaces the per-
  // position O(n_rules) thvm_match scan with a tree descent.  An
  // unorientable equation, while present in R, drops to the linear
  // path (where both rewrite directions are order-gated).  `n_unorient`
  // is a live COUNT, not a sticky flag: a transient unorientable rule
  // -- interreduced away or re-oriented after reduction -- restores the
  // indexed path the moment R is orientable again.
  if (lhs == s->lhs && rhs == s->rhs && n_rules == s->n_rules) {
#ifdef THVM_ATP_AC
    // Under a non-zero AC mask, route through the pure linear
    // ordered_rewrite_step path (the outermost fallback below).
    // Reasons:
    //   - The indexed / flatterm / wmfpa fast paths key their
    //     discrim-tree descents on syntactic preorder and would
    //     miss AC-permuted redexes.
    //   - The "mixed loop" path runs atp_rewrite_normalize_indexed
    //     interleaved with ordered_rewrite_step under
    //     g_atp_skip_oriented = 1, which BYPASSES the oriented-
    //     branch AC match in atp_ordered_try_top.
    // So the AC path takes the linear ordered step directly.  The
    // slowdown applies only when ac_mask != 0; default builds and
    // ac_mask = 0 take the original fast paths unchanged.
    if (thvm_atp_get_ac_mask() != 0ull) {
      for (u32 i = 0; i < step_cap; i++) {
        if (atp_norm_deadline_fired(s)) return t;
        u8 fired = 0;
        Term t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
        if (!fired) break;
        t = t2;
      }
      return t;
    }
#endif
    if (s->n_unorient == 0u) {
      // Iter 133 step 2: keep the subject in flat form when use_flatterm
      // is on.  flatterm_mixed runs indexed_fixpoint + (no-op) unorient
      // step, returning the same NF as the indexed path but avoiding the
      // per-call tree-flatten + rebuild round-trip.
      if (s->use_flatterm)
        return atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
      // Dormant Term-side WM-FPA path (leftmost-outermost, lowest-slot
      // policy: wf_normalize / wf_descend).  Re-gated from
      // THVM_ATP_WMFPA to THVM_ATP_WMFPA_TERM when that flag was
      // repointed at the WM-policy FT hook (ft_norm.c
      // atp_rules_only_normalize_wf) -- one flag must never arm two
      // different redex policies.
      {
        static int wmfpa_term = -1;
        if (wmfpa_term < 0) wmfpa_term = atp_env_on("THVM_ATP_WMFPA_TERM");
        if (wmfpa_term) return atp_rewrite_normalize_wmfpa(s, t, step_cap);
      }
      return atp_rewrite_normalize_indexed(s, t, step_cap);
    }
    // Opt-in flatterm fast-path: keep the subject flat across both the
    // orientable indexed fixpoint and the unorientable pass (no per-step
    // re-flatten / tree rebuild).  Same normal form as the tree loop
    // below; default OFF so the engine is byte-identical.
    if (s->use_flatterm) {
#if defined(ATP_FLATTERM_SELFCHECK) || defined(ATP_FLATTERM_DIFF)
      // In-engine differential: also run the tree path and check every
      // LIVE-saturation normalize for flat-NF == tree-NF.  Build-only
      // (defeats the speedup).  Under SELFCHECK a mismatch aborts (the
      // build-time soundness invariant); under the DIFF test build it
      // bumps g_atp_ft_diff_mism so a test can CHECK it == 0.  This fires
      // on the REAL subjects saturation reduces -- the deep critical-pair
      // terms the offline random battery never constructs.
      {
        Term ref = atp_rewrite_normalize_flatterm_selfcheck_tree(s, t, step_cap);
        Term got = atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
        if (!kbo_eq(ref, got)) {
          char ib[2048], rb[2048], gb[2048];
          atp_pretty_term(t, ib, sizeof ib);
          atp_pretty_term(ref, rb, sizeof rb);
          atp_pretty_term(got, gb, sizeof gb);
          fprintf(stderr, "FLATTERM SELFCHECK MISMATCH\n in=%s\n tree=%s\n flat=%s\n",
                  ib, rb, gb);
          fprintf(stderr, " n_rules=%u n_unorient=%u\n", s->n_rules, s->n_unorient);
#ifdef ATP_FLATTERM_SELFCHECK
          for (u32 ri = 0; ri < s->n_rules; ri++) {
            char la[1024], ra[1024];
            atp_pretty_term(s->lhs[ri], la, sizeof la);
            atp_pretty_term(s->rhs[ri], ra, sizeof ra);
            fprintf(stderr, "  R%u%s: %s = %s\n", ri,
                    s->r_orient[ri] ? "" : "(un)", la, ra);
          }
          abort();   // build-time invariant: flatterm NF == tree NF
#else
          // DIFF test build: record the divergence.  Return the FLAT result
          // (got) -- the same value the un-instrumented engine uses -- so
          // saturation follows the EXACT trajectory the live engine does,
          // reaching the same deep critical pairs where the staleness bites.
          // A correct flat path has got == ref, so the trajectory is the
          // tree-correct one and g_atp_ft_diff_mism stays 0.  The test
          // CHECKs g_atp_ft_diff_mism == 0.
          g_atp_ft_diff_mism++;
#endif
        }
        return got;
      }
#endif
      return atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
    }
    // Mixed rule set: the discrimination tree (orientable rules only)
    // normalizes every orientable rewrite in one fast descent; the few
    // unorientable equations are then applied one outermost, KBO-gated
    // step at a time, re-running the indexed pass between.  After an
    // indexed fixpoint no orientable rule fires, so the linear step
    // finds an unorientable rewrite (or none = done).  Replaces the
    // O(n_rules) linear scan per rewrite that dominated completion on a
    // self-overlapping axiom.
    for (u32 i = 0; i < step_cap; i++) {
      if (atp_norm_deadline_fired(s)) return t;
      // Iterate the indexed orientable fixpoint to convergence -- a single
      // atp_rewrite_normalize_indexed call exits at step_cap regardless of
      // whether the inner loop reached fixpoint, so when the term needs
      // more than step_cap orientable rewrites the caller observed an
      // incomplete NF.  Call again until the term stops changing.  Bounded
      // by the outer step_cap so total orientable work stays
      // O(step_cap * step_cap) -- matches the previous loose bound.
      for (u32 j = 0; j < step_cap; j++) {
        Term t_in = t;
        t = atp_rewrite_normalize_indexed(s, t, step_cap);
        if (kbo_eq(t, t_in)) break;     // true fixpoint
      }
      u8 fired = 0;
      Term t2;
      if (s->use_unorient_index) {
        // Indexed unorientable step: a discrimination-tree retrieval over
        // both faces of every unorientable equation replaces the O(n_rules)
        // linear KBO-gated scan.  Byte-identical redex -> identical NF.
        t2 = atp_unorient_step_indexed(s, t, &fired);
      } else {
        g_atp_skip_oriented = 1u;
        t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
        g_atp_skip_oriented = 0u;
      }
      if (!fired) break;
      t = t2;
    }
    return t;
  }
#endif
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    u8 fired = 0;
    Term t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
    if (!fired) break;
    t = t2;
  }
  return t;
}
#endif /* ATP_ORDERED_REWRITE */

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

// Capped variant: accumulate `t`'s node count into *acc, stopping the
// walk as soon as *acc reaches `cap`.  For a threshold test (is the
// count < cap?) the verdict is identical to the full count -- a capped
// result >= cap iff the true count is -- while an over-cap giant pays
// O(cap) instead of O(|t|).
static void atp_symbol_count_upto(Term t, u32 cap, u32 *acc) {
  if (*acc >= cap) return;
  (*acc)++;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n && *acc < cap; i++) {
      atp_symbol_count_upto(term_ctr_at(t, i), cap, acc);
    }
  }
}

// Same node count as atp_symbol_count, but with Twee's shared-subterm
// discount (src/Twee/CP.hs:259-261, cfg_dupcost=1, cfg_dupfactor=0):
// each unique Term value counts once at full weight; repeat occurrences
// count 1 and the recursion stops -- so a term f(g(a), g(a)) counts
// {f, g(a), a} + 1 for the second g(a) = 4 instead of 5.  An iterative
// walk with a per-call open-addressed Term hash set (capacity 512 -- a
// CP face larger than ~500 nodes would have walled the saturation long
// before reaching this weight).  An overflow into the 513th distinct
// term falls back to counting that node without dedup.
static u32 atp_symbol_count_dedup(Term t) {
  Term stack[256];
  Term seen[512];
  for (u32 i = 0; i < 512; i++) seen[i] = 0;
  u32 sp = 0;
  if (sp < 256) stack[sp++] = t;
  u32 count = 0;
  while (sp > 0) {
    Term cur = stack[--sp];
    // Probe the seen-set with an inlined splitmix64 finalizer over
    // the Term's u64 representation.
    u64 x = (u64)cur;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    u32 h = (u32)x & 511u;
    u8 dup = 0;
    for (u32 step = 0; step < 512u; step++) {
      u32 idx = (h + step) & 511u;
      if (seen[idx] == 0) { seen[idx] = cur; break; }
      if (seen[idx] == cur) { dup = 1; break; }
    }
    if (dup) {
      count += 1u;            // cfg_dupcost = 1; do not recurse.
      continue;
    }
    count += 1u;              // this node's own weight.
    if (term_tag(cur) == TAG_CTR) {
      u32 n = term_ctr_n(cur);
      for (u32 i = 0; i < n && sp < 256; i++) {
        stack[sp++] = term_ctr_at(cur, i);
      }
    }
  }
  return count;
}

// Twee's exact per-term size (Twee.CP.termScore, src/Twee/CP.hs:249).
// Differs from atp_symbol_count_dedup in TWO ways that matter for the
// Twee CP score's selection order:
//   1. varweight = 6/7 (a variable is cheaper than a function symbol),
//      not 1.  We scale the whole score by 7 to stay integer: a variable
//      contributes 6, a function symbol / constant 7.
//   2. the shared-subterm discount (cfg_dupcost=1) fires ONLY for
//      COMPOUND subterms (Twee's `len t > 1`), NEVER for a repeated
//      variable or constant -- those are counted in full every time.
//      atp_symbol_count_dedup deduped every repeat, underweighting terms
//      with repeated constants (e.g. the median goal's skolem u/w x3).
// Caller multiplies the depth term by 7 to keep 2*depth's proportion.
static u64 g_twee_dbg_calls = 0, g_twee_dbg_dups = 0, g_twee_dbg_drops = 0;
static u8  g_twee_dbg_ctx = 0;   // 0=other 1=insert-treated 2=insert-raw 3=rebuild/IR
static u64 g_twee_dbg_ctx_calls[4], g_twee_dbg_ctx_dups[4];
static void atp_twee_dbg_report(void) {
  fprintf(stderr, "[twee-score] calls=%llu dups=%llu stack-drops=%llu\n",
          (unsigned long long)g_twee_dbg_calls,
          (unsigned long long)g_twee_dbg_dups,
          (unsigned long long)g_twee_dbg_drops);
  static const char *nm[4] = {"other", "insert-treated", "insert-raw",
                              "rebuild"};
  for (u32 i = 0; i < 4; i++) {
    fprintf(stderr, "[twee-score]   %-14s calls=%llu dups=%llu\n", nm[i],
            (unsigned long long)g_twee_dbg_ctx_calls[i],
            (unsigned long long)g_twee_dbg_ctx_dups[i]);
  }
}
static u32 atp_twee_term_score(Term t) {
  static int dbg = -1;
  if (dbg < 0) {
    dbg = atp_env_on("THVM_TWEE_SCORE_STATS");
    if (dbg) atexit(atp_twee_dbg_report);
  }
  if (dbg) { g_twee_dbg_calls++; g_twee_dbg_ctx_calls[g_twee_dbg_ctx]++; }
  Term stack[256];
  Term seen[512];
  for (u32 i = 0; i < 512; i++) seen[i] = 0;
  u32 sp = 0;
  if (sp < 256) stack[sp++] = t;
  u32 score = 0;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) != TAG_CTR || term_ctr_n(cur) == 0u) {
      // Variable (FVR) -> varweight 6; constant / atom -> weight 7.
      // Neither is ever deduped (Twee dedups only `len t > 1`).
      score += (term_tag(cur) == TAG_FVR) ? 6u : 7u;
      continue;
    }
    // Compound subterm: dupcost=1 (scaled 7) if seen before, no recurse.
    u64 x = (u64)cur;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    u32 h = (u32)x & 511u;
    u8 dup = 0;
    for (u32 step = 0; step < 512u; step++) {
      u32 idx = (h + step) & 511u;
      if (seen[idx] == 0) { seen[idx] = cur; break; }
      if (seen[idx] == cur) { dup = 1; break; }
    }
    if (dup) {
      score += 7u;   // cfg_dupcost=1 *7, no recurse
      if (dbg) { g_twee_dbg_dups++; g_twee_dbg_ctx_dups[g_twee_dbg_ctx]++; }
      continue;
    }
    score += 7u;                          // the function symbol (weight 1)
    u32 n = term_ctr_n(cur);
    for (u32 i = 0; i < n; i++) {
      if (sp >= 256) { if (dbg) g_twee_dbg_drops++; break; }
      stack[sp++] = term_ctr_at(cur, i);
    }
  }
  return score;
}

// Combine the two per-side Twee term scores and the CP formation depth
// into the final Twee.CP priority (Twee.CP.score, src/Twee/CP.hs:240):
//   lhsweight * size(larger) + rhsweight * size(smaller) + depthweight * depth
// Twee's built-in defaults are lhsweight=4, rhsweight=1, depthweight=2.
// `large`/`small` arrive already *7-scaled (atp_twee_term_score's varweight
// 6/7); the depth coefficient is likewise scaled *7 so depthweight*depth
// stays in the same units, exactly as the historical hardcoded
// `4*large + 1*small + 14*depth`.  Twee's parallel portfolio auto-selects
// per-problem weight configs (LAT138-1 solves under `--lhs-weight 1
// --depth-weight 60`, a much steeper depth penalty that keeps the KB
// derivation shallow and reaches the goal before the ACI CP explosion);
// the env knobs (CLI --cp-lhs-weight / --cp-rhs-weight /
// --cp-depth-weight) expose the same tuning.  Defaults reproduce the
// prior hardcoded formula byte-for-byte, so the WM-parity path is
// unchanged.
static u32 atp_twee_score_combine(u32 large, u32 small, u32 depth) {
  static int lhs_w = -1, rhs_w = -1, depth_w = -1;
  if (lhs_w < 0) {
    const char *e;
    e = getenv("THVM_ATP_TWEE_LHS_W");
    lhs_w   = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 4;
    e = getenv("THVM_ATP_TWEE_RHS_W");
    rhs_w   = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 1;
    e = getenv("THVM_ATP_TWEE_DEPTH_W");
    depth_w = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 2;
    if (lhs_w < 0)   lhs_w   = 4;
    if (rhs_w < 0)   rhs_w   = 1;
    if (depth_w < 0) depth_w = 2;
  }
  return (u32)lhs_w * large + (u32)rhs_w * small + (u32)depth_w * 7u * depth;
}

// (atp_term_depth defined below at line ~4653.)

// === ClasHeuristics: advanced CP-weight term measures ===============
// Helpers feeding the non-default `AtpCpWeightMode` weight modes --
// ports of measures from Waldmeister's `ClasHeuristics`,
// `ClasFunctions` and `Unifikation1` ("unification 1") modules.

// Term depth: a leaf (FVR / atom / nullary CTR) has depth 0; a
// CTR has 1 + max child depth.  Used by the unification-measure
// weight, mirroring Waldmeister's `TO_Termtiefe` ("term depth").
static u32 atp_term_depth(Term t) {
  if (term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  u32 d = 0;
  for (u32 i = 0; i < n; i++) {
    u32 cd = atp_term_depth(term_ctr_at(t, i));
    if (cd > d) d = cd;
  }
  return n == 0 ? 0 : d + 1;
}

// Does variable `var_id` (a TAG_FVR id) occur anywhere in `t`?
// The occur-check used by the unification-measure weight.
static int atp_var_occurs(Term t, u32 var_id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (atp_var_occurs(term_ctr_at(t, i), var_id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// Depth-weighted term-disagreement count -- a port of Waldmeister's
// `U1_Unifikationsmass` ("unification measure"; sources/INF/
// Unifikation1.c).  Walks `a` and `b` in parallel: equal top
// symbols recurse into children; a variable on either side that
// fails the occur-check binds for free (cost 0) and otherwise costs
// `2*d`; a function-symbol clash costs `d`.  `d` is the working
// depth, clamped at 1 as the recursion descends -- mirroring
// Waldmeister's `if (!(--d)) d = 1;` step-down.  Result is 0 iff
// the terms are syntactically unifiable.
static u32 atp_unif_measure_rec(Term a, Term b, u32 d) {
  u8 ta = term_tag(a), tb = term_tag(b);
  if (ta == TAG_CTR && tb == TAG_CTR
      && term_ext(a) == term_ext(b)
      && term_ctr_n(a) == term_ctr_n(b)) {
    u32 nd = d > 1 ? d - 1 : 1;
    u32 n = term_ctr_n(a);
    u32 mass = 0;
    for (u32 i = 0; i < n; i++) {
      mass += atp_unif_measure_rec(term_ctr_at(a, i),
                                   term_ctr_at(b, i), nd);
    }
    return mass;
  }
  if (ta == TAG_FVR) {
    return atp_var_occurs(b, term_ext(a)) ? 2u * d : 0u;
  }
  if (tb == TAG_FVR) {
    return atp_var_occurs(a, term_ext(b)) ? 2u * d : 0u;
  }
  // Function-symbol (or non-CTR) clash.
  return d;
}

static u32 atp_unif_measure(Term lhs, Term rhs) {
  u32 dl = atp_term_depth(lhs);
  u32 dr = atp_term_depth(rhs);
  u32 d  = dl > dr ? dl : dr;
  if (d == 0) d = 1;
  return atp_unif_measure_rec(lhs, rhs, d);
}

// Per-term KBO weight feeding the ORD mode.  Ports Waldmeister's
// `CF_Phi_KBO` ("KBO weight"; sources/CLAS/ClasFunctions.c): sum
// the active signature's per-symbol weights
// (`cfg->weights[label]` for a function symbol, `cfg->var_weight`
// for a variable).  m8's KBO module only exposes the single-pass
// differential balance, so the standalone single-term walk is
// inlined here.  With no KboConfig attached the raw symbol count
// is the fallback.
//
// GT / MIX / MIX2 / UNIF / MAX use CF_Phi (CP-weight table) not
// CF_Phi_KBO; per WM ClasHeuristics.c that's atp_symbol_count
// here (CP table defaults to 1 for all symbols).
static u32 atp_kbo_weight(AtpState *s, Term t) {
  const KboConfig *cfg = (s != NULL) ? s->kbo : NULL;
  if (cfg == NULL) return atp_symbol_count(t);
  // Served from the per-term KBO weight memo (thvm_kbo_term_weight): the
  // CP-weight heuristics weigh both faces of every critical pair, so the
  // same terms are summed repeatedly within an epoch.  Byte-identical to
  // the inlined recursion (same cfg weights), just memoized.
  return (u32)thvm_kbo_term_weight(cfg, t);
}

// CP-weight base for one weight mode -- ports of the `CH_*Weight`
// functions in Waldmeister's `ClasHeuristics` module.  Mode
// ATP_CP_WEIGHT_ADD reproduces the pre-port symbol-count sum.
static u32 atp_cp_weight_base(AtpState *s, Term lhs, Term rhs, u32 mode) {
  switch (mode) {
    case ATP_CP_WEIGHT_MAX: {
      // CH_MaxWeight: max(|lhs|, |rhs|).
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      return wl > wr ? wl : wr;
    }
    case ATP_CP_WEIGHT_ORD: {
      // CH_OrdWeight: KBO-weight sum (CF_Phi_KBO over both sides).
      return atp_kbo_weight(s, lhs) + atp_kbo_weight(s, rhs);
    }
    case ATP_CP_WEIGHT_GT: {
      // CH_GtWeight: ordering-directed -- the greater side's
      // weight when the CP orients, the sum otherwise.
      // CF_Phi (CP-weight table, default symbol-count), not KBO.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      return (c == KBO_GT) ? wl
           : (c == KBO_LT) ? wr
           : wl + wr;
    }
    case ATP_CP_WEIGHT_MIX: {
      // CH_MixWeight: (wl+wr)*g + g + (wl+wr), g = GtWeight value.
      // WM faithfully: side weights are CF_Phi (CP-weight table,
      // default symbol-count per andassoc.pr) NOT CF_Phi_KBO; only
      // the orient discriminator uses ordering.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      u32 sum = wl + wr;
      return sum * g + g + sum;
    }
    case ATP_CP_WEIGHT_MIX2: {
      // CH_MixWeight2: g*10 + (wl+wr).  Same CP-weight base as MIX.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      return g * 10u + (wl + wr);
    }
    case ATP_CP_WEIGHT_UNIF: {
      // CH_Unifikationsmass: (wl+wr) * unification-measure.
      // CF_Phi (CP-weight table, default symbol-count), not KBO.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      return (wl + wr) * atp_unif_measure(lhs, rhs);
    }
    case ATP_CP_WEIGHT_TWEE: {
      // Twee.CP.score (src/Twee/CP.hs:240).  Asymmetric:
      //   lhsweight * size(larger) + rhsweight * size(smaller)
      //   + depthweight * depth
      // Twee defaults: lhsweight=4, rhsweight=1, depthweight=2.
      // The "larger side" is treated as the peak the CP must
      // reduce away, so making the SMALLER side cheap is what
      // accelerates the search.
      //
      // Twee's per-term size also folds in a shared-subterm
      // discount (src/Twee/CP.hs:259-261): cfg_dupcost=1,
      // cfg_dupfactor=0 -- when a subtree has already been seen in
      // the same termScore walk, count it as 1 instead of its full
      // size and do not recurse.  atp_symbol_count_dedup mirrors
      // that with a per-call Term-id hash set.  Integer-arithmetic
      // version (we approximate Twee's varweight 6/7 == 1).
      // Twee-exact per-term size (varweight 6/7, compound-only dedup),
      // scaled *7 so it stays integer; the depth term below is *14
      // (= 2 * 7) to preserve Twee's 2*depth proportion.
      u32 m = atp_twee_term_score(lhs);
      u32 n = atp_twee_term_score(rhs);
      u32 large = m > n ? m : n;
      u32 small = m + n - large;
      // Twee's `depth` is the CP FORMATION depth (getDepth), NOT the
      // syntactic term depth.  On flattened problems the flat-names keep
      // terms shallow (term-depth ~1-2, a useless constant), so the old
      // atp_term_depth gave no derivation penalty and thvm explored the
      // deep-derivation space Twee prunes.  g_cp_formation_depth carries
      // max(parent rule depths)+1, set by atp_push_cps_traced.
      u32 d = g_cp_formation_depth;
      return atp_twee_score_combine(large, small, d);
    }
    case ATP_CP_WEIGHT_STAGGERED: {
      // E StaggeredWeight (HEURISTICS/che_varweights.c::
      // StaggeredWeightCompute).  Lazily compute max axiom weight on
      // first call (one pass over rules; the result is reused for
      // the rest of the run).  Bucket the CP into integer stagger
      // groups via base / stagger_limit.
      u32 base = atp_symbol_count(lhs) + atp_symbol_count(rhs);
      u32 max_aw = 0;
      for (u32 i = 0; i < s->n_rules; i++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        u32 wi = atp_rule_symbol_count(s, i, 0u) +
                 atp_rule_symbol_count(s, i, 1u);
#else
        u32 wi = atp_symbol_count(s->lhs[i]) + atp_symbol_count(s->rhs[i]);
#endif
        if (wi > max_aw) max_aw = wi;
      }
      u32 stagger_limit = (max_aw / 2u);
      if (stagger_limit < 1u) stagger_limit = 1u;
      return base / stagger_limit;
    }
    case ATP_CP_WEIGHT_RELLEVEL: {
      // E RelevanceLevelWeight (HEURISTICS/che_funweights.c:610) +
      // init_relevance_vector.  N-level port: per-symbol BFS distance
      // from the conjecture through the axiom co-occurrence graph,
      // capped at ATP_REL_LEVEL_MAX = 8.  Per-CTR-node weight is
      //   1 + sym_level[label]   (cap at REMOTE penalty = MAX + 2)
      // so level 0 contributes 1, level 1 contributes 2, ..., remote
      // (255) contributes ATP_REL_LEVEL_MAX + 2 = 10.  Variable
      // nodes weight 1.  Sum over both sides.  Level array is
      // precomputed at thvm_atp_set_goal time so this walk is O(|CP|).
      Term stack[256];
      u32 sp = 0;
      if (sp < 256) stack[sp++] = lhs;
      if (sp < 256) stack[sp++] = rhs;
      u32 total = 0;
      while (sp > 0) {
        Term cur = stack[--sp];
        if (term_tag(cur) == TAG_CTR) {
          u32 lab = term_ext(cur);
          u32 level;
          if (lab < WALD_MAX_SYMBOLS &&
              s->sym_level[lab] != ATP_REL_LEVEL_REMOTE) {
            level = s->sym_level[lab];
          } else {
            level = ATP_REL_LEVEL_MAX + 1u;  // remote penalty
          }
          total += 1u + level;
          u32 n = term_ctr_n(cur);
          for (u32 i = 0; i < n && sp < 256; i++) {
            stack[sp++] = term_ctr_at(cur, i);
          }
        } else {
          total += 1u;              // FVR / others: weight 1
        }
      }
      return total;
    }
    case ATP_CP_WEIGHT_DIVERSITY: {
      // E DiversityWeight (HEURISTICS/che_diversityweight.c:161).
      // Iterative walk of both sides; we tally:
      //   base       -- total CTR/FVR node count
      //   f_distinct -- # of distinct CTR head labels (popcount of
      //                 the label-seen bitmask)
      //   v_distinct -- # of distinct FVR ids (popcount of the FVR-seen
      //                 bitmask)
      //   weight = base + f_distinct + v_distinct
      // Linear E-defaults shape (fdiff1=1, fdiff2=0, vdiff1=1, vdiff2=0).
      // Labels and FVR ids are tracked in u64 bitmasks; WALD_MAX_SYMBOLS
      // is 64 so the f-mask covers the whole label space.  FVR ids are
      // capped at 64 since thvm_normalize_vars renumbers each rule into
      // 0..REWRITE_MAX_VAR-1 (== 64).
      u64 f_mask = 0;
      u64 v_mask = 0;
      u32 base = 0;
      Term stack[256];
      u32 sp = 0;
      if (sp < 256) stack[sp++] = lhs;
      if (sp < 256) stack[sp++] = rhs;
      while (sp > 0) {
        Term cur = stack[--sp];
        if (term_tag(cur) == TAG_CTR) {
          u32 lab = term_ext(cur);
          if (lab < 64u) f_mask |= ((u64)1 << lab);
          base++;
          u32 n = term_ctr_n(cur);
          for (u32 i = 0; i < n && sp < 256; i++) {
            stack[sp++] = term_ctr_at(cur, i);
          }
        } else if (term_tag(cur) == TAG_FVR) {
          u32 vid = term_ext(cur);
          if (vid < 64u) v_mask |= ((u64)1 << vid);
          base++;
        } else {
          base++;
        }
      }
      // Portable popcount via Kernighan's bit-clearing loop.
      u32 f_distinct = 0; { u64 m = f_mask; while (m) { f_distinct++; m &= m - 1u; } }
      u32 v_distinct = 0; { u64 m = v_mask; while (m) { v_distinct++; m &= m - 1u; } }
      return base + f_distinct + v_distinct;
    }
    case ATP_CP_WEIGHT_CONJSYM: {
      // E ConjectureSymbolWeight (HEURISTICS/che_funweights.c:550).
      // Walk both sides.  A node whose head symbol appears in the
      // conjecture gets weight 1 (E's conj_fweight=1); a node whose
      // head does not appear gets weight 4 (E's fweight=4, the
      // 4x non-conjecture penalty E defaults to).  Variables count
      // as 1 (E's vweight=1).  Sum over both sides.
      //   conj_count = # CTR nodes whose symbol is in conj_sym_mask
      //   off_count  = # CTR nodes whose symbol is NOT in mask
      //   var_count  = # FVR nodes
      //   weight = conj_count + 4*off_count + var_count
      // When no goal is set (conj_sym_mask == 0), every label misses
      // and the formula degenerates to 4*ctr_count + var_count -- a
      // monotonic alias for ADD, so the mode is still well-defined
      // in pure-completion runs.
      u64 mask = s->conj_sym_mask;
      Term stack[256];
      u32 sp = 0;
      if (sp < 256) stack[sp++] = lhs;
      if (sp < 256) stack[sp++] = rhs;
      u32 conj_count = 0, off_count = 0, var_count = 0;
      while (sp > 0) {
        Term cur = stack[--sp];
        if (term_tag(cur) == TAG_CTR) {
          u32 lab = term_ext(cur);
          if (lab < 64u && (mask & ((u64)1 << lab))) conj_count++;
          else                                       off_count++;
          u32 n = term_ctr_n(cur);
          for (u32 i = 0; i < n && sp < 256; i++) {
            stack[sp++] = term_ctr_at(cur, i);
          }
        } else if (term_tag(cur) == TAG_FVR) {
          var_count++;
        }
      }
      return conj_count + 4u * off_count + var_count;
    }
    case ATP_CP_WEIGHT_WMQ: {
      // Waldmeister -auto (Automodus/StdS) CP measure: weight = S*S + S - 1,
      // S = symcount(l) + symcount(r).  Derived exactly from wmcli -a4 verbose
      // weights on WolframAxioms/OrAssociativity (S=14->209, 26->701, 30->929).
      // Total-size-only (not l/r split): same-S CPs share a bucket -> cp_seq
      // FIFO age tie-break decides, matching WM's best-first.  See thvm.h.
      u64 s = (u64)atp_symbol_count(lhs) + (u64)atp_symbol_count(rhs);
      u64 w = s * s + s - 1u;
      return (w > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (u32)w;
    }
    case ATP_CP_WEIGHT_WMQ2: {
      // Waldmeister -auto CP measure, DERIVED from the full wmcli emission
      // trace (3780 CP/weight pairs): weight = b*b + 2*b + a*(b+1), a = min
      // side symbol count, b = max side symbol count.  Fits 96.5% of WM's real
      // weights exactly; WMQ (S*S+S-1) is only the a==1 collapser special case.
      u64 sl = (u64)atp_symbol_count(lhs);
      u64 sr = (u64)atp_symbol_count(rhs);
      u64 a = sl < sr ? sl : sr;
      u64 b = sl < sr ? sr : sl;
      u64 w = b * b + 2u * b + a * (b + 1u);
      return (w > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (u32)w;
    }
    case ATP_CP_WEIGHT_ADD:
    default:
      // CH_AddWeight: symbol-count sum -- the pre-port default.
      return atp_symbol_count(lhs) + atp_symbol_count(rhs);
  }
}

// === Waldmeister goal-directed CP selection (Clas_CP_Goal.c) ========
// Weight a critical pair by how its two sides structurally match the
// goal (the CPinGoal classifier).  A CP side that is a generalization
// of -- i.e. matches into -- a goal subterm "covers" that subterm.
//   Doppelmatch  : both CP sides match goal subterms under ONE
//                  consistent substitution.  Weight = the goal residual
//                  phi(goal) - covered (minimized over match positions).
//   Einfachmatch : one CP side matches.  Weight = residual x SingleMatchFactor.
//   Nullmatch    : neither.  Weight = the CP's own size x NoMatchFactor.
// A goal-resembling CP thus scores small and is selected first,
// steering completion at the goal -- Waldmeister's key to fast proofs.
// Factors are Waldmeister's -auto mnfanalysis=1000.2000.10 overrides
// (Clas_CP_Goal.c:237-239), NOT the 5/50/4 source DEFAULTS (:242-246):
// FEQ runs the Automodus override, so the large factors strongly separate
// the match levels (Doppel << Einfach << Null) as WM's goal pick requires.
// THVM_ATP_GOAL_FACTORS=def restores the 5/50 defaults for the legacy path.
#define ATP_GOAL_SINGLE_FACTOR atp_goal_single_factor()
#define ATP_GOAL_NONE_FACTOR   atp_goal_none_factor()
static u32 atp_goal_single_factor(void) {
  static int v = -1;
  if (v < 0) { const char *e = getenv("THVM_ATP_GOAL_FACTORS");
               v = (e && e[0] == 'd') ? 5 : 1000; }
  return (u32)v;
}
static u32 atp_goal_none_factor(void) {
  static int v = -1;
  if (v < 0) { const char *e = getenv("THVM_ATP_GOAL_FACTORS");
               v = (e && e[0] == 'd') ? 50 : 2000; }
  return (u32)v;
}

// Best coverage of `pat` matched into `subj` or any subterm, extending
// the (consistent) substitution `base`: returns the largest matched
// subterm's symbol count, 0 if none.  `pat` must be a CTR -- a bare
// variable generalizes everything and carries no goal signal (the
// MinStruct spirit of Clas_CP_Goal).
static u32 atp_goal_match_cov(Term pat, Term subj, RewriteSubst base) {
  if (term_tag(pat) != TAG_CTR) return 0u;
  u32 best = 0u;
  RewriteSubst t = base;
  if (thvm_match(pat, subj, &t)) {
    u32 c = atp_symbol_count(subj);
    if (c > best) best = c;
  }
  if (term_tag(subj) == TAG_CTR) {
    u32 n = term_ctr_n(subj);
    for (u32 i = 0; i < n; i++) {
      u32 c = atp_goal_match_cov(pat, term_ctr_at(subj, i), base);
      if (c > best) best = c;
    }
  }
  return best;
}

// Best (cov_cl + cov_cr) for a Doppelmatch: cl matches a subterm of sx
// (substitution sigma), cr matches a subterm of gy extending sigma.
// Walks every cl-match position so the cr-match is gated by a
// consistent sigma, maximizing total coverage (= minimal residual).
static u32 atp_goal_doppel(Term cl, Term cr, Term sx, Term gy,
                           RewriteSubst base) {
  u32 best = 0u;
  if (term_tag(cl) == TAG_CTR) {
    RewriteSubst t1 = base;
    if (thvm_match(cl, sx, &t1)) {
      u32 cov_a = atp_symbol_count(sx);
      u32 cov_b = atp_goal_match_cov(cr, gy, t1);
      if (cov_b && cov_a + cov_b > best) best = cov_a + cov_b;
    }
  }
  if (term_tag(sx) == TAG_CTR) {
    u32 n = term_ctr_n(sx);
    for (u32 i = 0; i < n; i++) {
      u32 c = atp_goal_doppel(cl, cr, term_ctr_at(sx, i), gy, base);
      if (c > best) best = c;
    }
  }
  return best;
}

// CPinGoal weight of CP (cl, cr) against the conjecture.  No goal
// (completion mode) -> 0.  See the block comment above.
// --- Structural goal-relevance selection (use_goal_subterm) ----------
//
// A genuinely distinct goal-directed CP metric from SOS (symbol-level:
// on a single-operator Sheffer signature EVERY CP touches the one
// operator, so SOS is a no-op) and from CPinGoal/atp_goal_weight
// (whole-side match coverage, an expensive per-CP unify).  Here we
// reward a CP by the SIZE of the largest GROUND subterm it shares
// (structurally) with the goal's ground terms -- i.e. CPs that operate
// on the goal's own substructure surface first, biasing lemma
// derivation toward the goal without a match/unify per CP.  Sound: only
// the heap key changes; no CP is dropped, completeness preserved.
#define ATP_GOAL_SUBTERM_FACTOR 4u
static void atp_goal_sub_collect_rec(AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return;
  if (atp_term_is_ground(t)) {
    u32 sz = atp_symbol_count(t);
    if (sz >= 2u && s->goal_sub_n < 128u) {   // skip bare constants
      u64 h = atp_term_struct_hash(t);
      u8 dup = 0u;
      for (u32 i = 0; i < s->goal_sub_n; i++)
        if (s->goal_sub_hash[i] == h) { dup = 1u; break; }
      if (!dup) {
        s->goal_sub_hash[s->goal_sub_n] = h;
        s->goal_sub_size[s->goal_sub_n] = (u16)(sz > 0xffffu ? 0xffffu : sz);
        s->goal_sub_n++;
      }
    }
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) atp_goal_sub_collect_rec(s, term_ctr_at(t, i));
}
static void atp_goal_sub_ensure(AtpState *s) {
  if (s->goal_sub_ready || s->goal_lhs == 0) return;
  s->goal_sub_n = 0u;
  atp_goal_sub_collect_rec(s, s->goal_lhs);
  atp_goal_sub_collect_rec(s, s->goal_rhs);
  s->goal_sub_ready = 1u;
}
static u32 atp_goal_subterm_overlap_rec(const AtpState *s, Term t, u32 best) {
  if (term_tag(t) != TAG_CTR) return best;
  if (atp_term_is_ground(t)) {
    u32 sz = atp_symbol_count(t);
    if (sz > best) {
      u64 h = atp_term_struct_hash(t);
      for (u32 i = 0; i < s->goal_sub_n; i++)
        if (s->goal_sub_hash[i] == h) { best = sz; break; }
    }
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    best = atp_goal_subterm_overlap_rec(s, term_ctr_at(t, i), best);
  return best;
}

static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr) {
  if (s == NULL || s->goal_lhs == 0) return 0u;
  Term gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
  Term gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  u32 phi_g = atp_symbol_count(gl) + atp_symbol_count(gr);
  RewriteSubst e = {{0}};
  // Doppelmatch over both pairings (cl->gl & cr->gr, cl->gr & cr->gl).
  u32 d1 = atp_goal_doppel(cl, cr, gl, gr, e);
  u32 d2 = atp_goal_doppel(cl, cr, gr, gl, e);
  u32 dcov = d1 > d2 ? d1 : d2;
  if (dcov) return (dcov < phi_g) ? (phi_g - dcov) : 0u;
  // Einfachmatch: best single coverage of either CP side into either
  // goal side.
  u32 ec = atp_goal_match_cov(cl, gl, e);
  { u32 x = atp_goal_match_cov(cl, gr, e); if (x > ec) ec = x; }
  { u32 x = atp_goal_match_cov(cr, gl, e); if (x > ec) ec = x; }
  { u32 x = atp_goal_match_cov(cr, gr, e); if (x > ec) ec = x; }
  if (ec) {
    u32 res = (ec < phi_g) ? (phi_g - ec) : 0u;
    return res * ATP_GOAL_SINGLE_FACTOR;
  }
  // Nullmatch.
  return (atp_symbol_count(cl) + atp_symbol_count(cr)) * ATP_GOAL_NONE_FACTOR;
}
// 8.8: priority weight for a CP.  Default `--add` heuristic is
// the symbol-count sum.  When `s->use_mix_heuristic` is set, add
// a penalty for CPs that fail to orient cleanly (KBO_UN or
// KBO_EQ) -- mirrors Waldmeister's `--mix` heuristic in
// `ClasHeuristics.c`.  The penalty (`MIX_UNORIENTED_PENALTY`)
// is conservative; experiments may want to tune it.  Under
// -DATP_GOAL_HEURISTIC a bounded goal-directed penalty is then
// added (Waldmeister lever 1, above).
//
// `s->cp_weight_mode` selects among the ported `ClasHeuristics`
// weight functions (see the `AtpCpWeightMode` enum); the engine
// default is ATP_CP_WEIGHT_GT.  Selecting ATP_CP_WEIGHT_ADD with
// use_mix_heuristic unset and no goal makes this function the
// bare symbol-count sum.
#define MIX_UNORIENTED_PENALTY 4u

// Forward-decl: atp_cp_priority_sized uses this; the helper is defined
// next to thvm_atp_set_use_sos.
static int atp_term_touches_goal(const AtpState *s, Term t);
// CP-insert weigh comparator: same VERDICT as atp_compare, with the
// plain-KBO case served memo-free (thvm_kbo_nomemo) below a node-count
// threshold.  The GT/MIX/MIX2 weigh compares the two faces of a FRESH
// CP exactly once -- new cells, never re-compared -- so the persistent
// weight memo's hashing (kbo_leaf_hash / kbo_memo_combine over every
// cold node) costs more than the single-pass walk it replaces; the
// runtime profile put the memo machinery at ~70% of the weigh's KBO
// time.  The memo is a pure cache (THVM_KBO_NO_WCACHE is the standing
// A/B witness), so the verdict -- and the engine trajectory -- is
// byte-identical for ANY threshold.  Big CPs (>= the threshold) keep
// the memo path: their subtrees repeat internally and across the
// orient gates, where the cache does pay.  Every non-plain-KBO route
// (AC mask, WPO/RPO/LPO, opt-in flat) falls back to atp_compare.
// THVM_ATP_KBO_NOMEMO_MAX overrides the threshold (0 = disabled).
static KboCmp atp_weigh_compare(AtpState *s, Term lhs, Term rhs, u32 nodes) {
  // Fused CP weigh: the pack walk already encoded both faces + the
  // whole-pair weight/var balances (acp_pack_fused).  Verdict-identical
  // to the thvm_kbo_nomemo route below on the renumbered pair; the
  // fused gate (atp_cp_push_fuse_ok) admits only the plain-KBO route,
  // so this branch never bypasses an AC/WPO/RPO/LPO ordering.
  if (g_acp_fuse.valid) return acp_fuse_kbo(s->kbo);
  static i64 nomemo_max = -1;
  if (nomemo_max < 0) {
    const char *e = getenv("THVM_ATP_KBO_NOMEMO_MAX");
    nomemo_max = (e != NULL) ? (i64)strtoul(e, NULL, 10) : (i64)0xffffffffu;
  }
  if (s != NULL && (i64)nodes <= nomemo_max &&
      s->kbo != NULL && s->wpo == NULL && s->rpo == NULL && s->lpo == NULL) {
#ifdef THVM_ATP_AC
    if (thvm_atp_get_ac_mask() != 0ull) return atp_compare(s, lhs, rhs);
#endif
    return thvm_kbo_nomemo(lhs, rhs, s->kbo);
  }
  return atp_compare(s, lhs, rhs);
}

// Per-side variant of atp_cp_weight_base for the modes that consume
// the two sides' symbol counts (MAX/GT/MIX/MIX2/UNIF): identical
// formulas, with the counts supplied by the caller (acp_pack counts
// exactly one per node, the same value atp_symbol_count returns) so
// the two per-side re-walks are skipped.  Every other mode falls back
// to the general path.
static u32 atp_cp_weight_base_counts(AtpState *s, Term lhs, Term rhs,
                                     u32 mode, u32 wl, u32 wr) {
  switch (mode) {
    case ATP_CP_WEIGHT_MAX:
      return wl > wr ? wl : wr;
    case ATP_CP_WEIGHT_GT: {
      KboCmp c = atp_weigh_compare(s, lhs, rhs, wl + wr);
      return (c == KBO_GT) ? wl
           : (c == KBO_LT) ? wr
           : wl + wr;
    }
    case ATP_CP_WEIGHT_MIX: {
      KboCmp c = atp_weigh_compare(s, lhs, rhs, wl + wr);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      u32 sum = wl + wr;
      return sum * g + g + sum;
    }
    case ATP_CP_WEIGHT_MIX2: {
      KboCmp c = atp_weigh_compare(s, lhs, rhs, wl + wr);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      return g * 10u + (wl + wr);
    }
    case ATP_CP_WEIGHT_UNIF:
      return (wl + wr) * atp_unif_measure(lhs, rhs);
    default:
      return atp_cp_weight_base(s, lhs, rhs, mode);
  }
}

// ATP_CP_WEIGHT_TWEE base for a FUSED push (g_acp_fuse.valid): the
// per-side scores come from the fuse state when the pack walk stashed
// them (ftp_pack_fused, the treated FT-direct route), from a
// materialize-and-score of the treated stash when the pack's size
// guard declined (twee_valid == 0 on an over-256-node side), and from
// the classic renumber+score of the REAL Term pair otherwise (the
// acp_pack_fused Term route arrives un-renumbered here -- the clone
// renumber reproduces the classic pipeline's nv-then-score exactly,
// including the Term-identity shared-subterm discounts of the raw
// >= 50 class, whose images this route still builds as Terms).
// THVM_ATP_TWEE_FUSE_CHECK=1 certifies the stashed-score arm per push
// against the materialized classic computation and aborts on any
// mismatch.
static u32 atp_cp_twee_fused_base(Term lhs, Term rhs) {
  u32 m, n;
  u8 have = 0u;
  if (g_acp_fuse.twee_valid) {
    m = g_acp_fuse.twee_l;
    n = g_acp_fuse.twee_r;
    have = 1u;
#ifdef THVM_ATPFT_NORM
    static int twee_check = -1;
    if (twee_check < 0) twee_check = atp_env_on("THVM_ATP_TWEE_FUSE_CHECK");
    if (twee_check && g_vcp_treat.armed &&
        g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
      Term cl = ft_to_term(g_vcp_treat.fl);
      Term cr = ft_to_term(g_vcp_treat.fr);
      thvm_normalize_vars(&cl, &cr);
      u32 rm = atp_twee_term_score(cl);
      u32 rn = atp_twee_term_score(cr);
      if (rm != m || rn != n) {
        fprintf(stderr,
                "ATP_TWEE_FUSE_CHECK MISMATCH lhs=%u/%u rhs=%u/%u "
                "(fl_n=%u fr_n=%u)\n",
                m, rm, n, rn, g_acp_fuse.fl_n, g_acp_fuse.fr_n);
        abort();
      }
    }
#endif
  }
#ifdef THVM_ATPFT_NORM
  if (!have && g_vcp_treat.armed &&
      g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
    // Treated FT-direct push whose pack declined the plain count (size
    // guard): the Terms here are routed-push sentinels, so materialize
    // the stash and score classically -- ft_to_term is fresh/unshared,
    // exactly the classic write-back image.
    m = atp_twee_term_score(ft_to_term(g_vcp_treat.fl));
    n = atp_twee_term_score(ft_to_term(g_vcp_treat.fr));
    have = 1u;
  }
#endif
  if (!have) {
    // Term-route fused push (raw >= 50 class + anomaly fallbacks): the
    // pair is REAL but un-renumbered.  Clone-renumber and score -- the
    // exact classic computation (nv output is isomorphic, and the
    // score is deterministic on the shared-DAG shape).
    Term cl = lhs, cr = rhs;
    thvm_normalize_vars(&cl, &cr);
    m = atp_twee_term_score(cl);
    n = atp_twee_term_score(cr);
  }
  u32 large = m > n ? m : n;
  u32 small = m + n - large;
  return atp_twee_score_combine(large, small, g_cp_formation_depth);
}

// Priority weight for a CP whose symbol-count sum is already known
// (`base`) -- e.g. counted for free during acp_pack.  Identical
// verdict to atp_cp_priority; only the redundant size walk is
// skipped.  The precomputed `base` is the ADD-mode value; a
// non-default cp_weight_mode recomputes the base from the terms.
// `lhs_nodes` (0xffffffff = unknown) is the LHS-only node count from
// acp_pack_ex; when known, the per-side weight modes reuse it (and
// base - lhs_nodes) instead of re-walking the two sides.
static u32 atp_cp_priority_sized_ex(AtpState *s, Term lhs, Term rhs,
                                    u32 base, u32 lhs_nodes) {
  // ifeq guard discipline (see AtpState.ifeq_syms): scale by
  // penalty^(undecided guards) when armed.  tgt goal-targeted scale
  // (see atp_tgt_weight_scale): multiplicative goal-symbol bias when
  // armed, applied BEFORE the ifeq scale (both multiplicative, so the
  // order only matters at the saturation ceiling).  ifeq guard-
  // DECIDING boost (see atp_ifeq_decide_scale) applied LAST, same
  // reasoning -- a guard-deciding CP with zero undecided guards of its
  // own (the common case: `g = true` has no ifeq subterm to penalize)
  // reaches the boost at its tgt/penalty-scaled base unchanged, so the
  // three disciplines compose without fighting in practice.  Applied
  // on every path out of this function via the wrapper below;
  // sentinel/zero Term args (the FT-routed pushes) count 0 guards / 0
  // symbols / no registry hit here and get their exact FT-span
  // treatment in atp_cp_heap_insert_packed instead.
  u8 ifeq_arm   = (s != NULL && s->use_ifeq_guard) ? 1u : 0u;
  u8 decide_arm = (s != NULL && s->use_ifeq_decide) ? 1u : 0u;
  u8 tgt_arm    = atp_tgt_armed(s);
#define ATP_IFEQ_RET(w)                                                   \
    do {                                                                  \
      u32 w_ret_ = (w);                                                   \
      if (tgt_arm) w_ret_ = atp_tgt_weight_scale(s, lhs, rhs, w_ret_);    \
      if (ifeq_arm) w_ret_ = atp_ifeq_weight_scale(s, lhs, rhs, w_ret_);  \
      if (decide_arm) w_ret_ = atp_ifeq_decide_scale(s, lhs, rhs, w_ret_);\
      return w_ret_;                                                     \
    } while (0)
  // Goal-directed mode (Waldmeister CPinGoal): weight by structural
  // match to the goal.  Opt-in via cp_weight_mode so completion-mode
  // and the other weights are unaffected.
  if (s != NULL && s->cp_weight_mode == ATP_CP_WEIGHT_GOAL &&
      s->goal_lhs != 0) {
    ATP_IFEQ_RET(atp_goal_weight(s, lhs, rhs));
  }
  u32 mode = (s != NULL) ? s->cp_weight_mode : ATP_CP_WEIGHT_ADD;
  if (mode == ATP_CP_WEIGHT_TWEE && g_acp_fuse.valid) {
    // Fused push: consume the pack walk's stashed per-side scores (or
    // their exact fallbacks) -- the routed FT pushes carry sentinel
    // Terms, so the generic recompute below would score garbage.
    base = atp_cp_twee_fused_base(lhs, rhs);
  } else if (mode != ATP_CP_WEIGHT_ADD) {
    // The caller's `base` is the ADD-mode symbol-count sum; for any
    // other mode it must be recomputed from the CP terms.
    base = (lhs_nodes != 0xffffffffu && lhs_nodes <= base)
             ? atp_cp_weight_base_counts(s, lhs, rhs, mode,
                                         lhs_nodes, base - lhs_nodes)
             : atp_cp_weight_base(s, lhs, rhs, mode);
  }
  if (s != NULL && s->use_mix_heuristic) {
    KboCmp c = atp_compare(s, lhs, rhs);
    if (c != KBO_GT && c != KBO_LT) {
      // KBO_EQ / KBO_UN -- penalize.
      base += MIX_UNORIENTED_PENALTY;
    }
  }
  // Set-of-Support bonus: a CP that shares any symbol with the goal
  // gets its priority halved, surfacing it earlier in the min-heap.
  // Sound -- nothing dropped, only ordering perturbed.  Mirrors
  // Vampire's --sos / E-prover's -S sos in spirit; tailored for the
  // equational-completion variant where the "support set" is symbols
  // (the goal isn't a separate clause set in our engine).
  if (s != NULL && s->use_sos && s->goal_lhs != 0) {
    if (atp_term_touches_goal(s, lhs) || atp_term_touches_goal(s, rhs)) {
      base = base >> 1;   // halve -> sort earlier
    }
  }
  // Structural goal-relevance: discount by the largest ground subterm
  // shared with the goal (see atp_goal_subterm_overlap_rec).
  if (s != NULL && s->use_goal_subterm && s->goal_lhs != 0) {
    atp_goal_sub_ensure(s);
    u32 ov = atp_goal_subterm_overlap_rec(s, lhs, 0u);
    ov = atp_goal_subterm_overlap_rec(s, rhs, ov);
    if (ov > 0u) {
      u32 disc = ov * ATP_GOAL_SUBTERM_FACTOR;
      base = (disc < base) ? (base - disc) : 1u;
    }
  }
  ATP_IFEQ_RET(base);
#undef ATP_IFEQ_RET
}
static u32 atp_cp_priority_sized(AtpState *s, Term lhs, Term rhs, u32 base) {
  return atp_cp_priority_sized_ex(s, lhs, rhs, base, 0xffffffffu);
}
// Learned CP-selection scorer (ENIGMA-style).  Logistic-regression
// weights trained on the labelled corpus exported by THVM_ATP_CP_DATASET
// (per-selected-CP features, labelled by trace-DAG reachability from the
// goal-closing step over the 83 provable AxiomaticTheory notable
// theorems; held-out test AUC ~0.85).  score = W.features + B, in the
// RAW feature space (the standardization is folded into W,B).  A higher
// score means more proof-relevant, so it maps to a LOWER heap priority
// (selected sooner).  Completeness is preserved by select_cp's periodic
// FIFO (CPdimension) pick, which fires regardless of the weight mode.
static const float ATP_LEARNED_W[ATP_CP_FEATURE_DIM] = {
  0.003216f, -0.271657f, 0.460614f, -0.096104f, 0.003216f, 0.042247f,
  0.003216f, -0.000402f, -0.005740f, -0.156174f, -0.023514f, 1.121586f,
  1.999360f, -0.012683f};
static const float ATP_LEARNED_B = -1.598045f;

// Runtime-loadable model (default inactive -> baked-in logreg above).
// Process-global: a WL-trained model is pushed once via
// thvm_atp_set_learned_scorer and reused by every subsequent
// ATP_CP_WEIGHT_LEARNED run, mirroring the abort-hook / dataset-env
// globals this bridge already uses.
static AtpLearnedScorer g_atp_learned;
static int g_atp_learned_active = 0;

// Forward pass: standardized features -> raw logit (no sigmoid head).
static float atp_learned_forward(const AtpLearnedScorer *m, const float *feat) {
  float z[ATP_CP_FEATURE_DIM];
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) {
    z[i] = (feat[i] - m->mean[i]) * m->inv_std[i];
  }
  if (m->kind == ATP_LEARNED_LINEAR) {
    float s = m->b2;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) s += m->w1[i] * z[i];
    return s;
  }
  // ATP_LEARNED_MLP: one ReLU hidden layer, single linear output.
  float s = m->b2;
  for (u32 j = 0; j < m->hidden; j++) {
    const float *wrow = m->w1 + (size_t)j * ATP_CP_FEATURE_DIM;
    float h = m->b1[j];
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) h += wrow[i] * z[i];
    if (h < 0.0f) h = 0.0f;
    s += m->w2[j] * h;
  }
  return s;
}

fn int thvm_atp_set_learned_scorer(const double *blob, u32 len) {
  if (blob == NULL || len < 30u) return 0;
  u32 kind   = (u32)blob[0];
  u32 hidden = (u32)blob[1];
  if (kind != ATP_LEARNED_LINEAR && kind != ATP_LEARNED_MLP) return 0;
  u32 want = (kind == ATP_LEARNED_LINEAR)
               ? 45u                              // 2 + 14 + 14 + (14 + 1)
               : 31u + 16u * hidden;              // 2 + 14 + 14 + H*14 + H + H + 1
  if (kind == ATP_LEARNED_MLP &&
      (hidden == 0u || hidden > ATP_LEARNED_MAX_HIDDEN)) {
    return 0;
  }
  if (len != want) return 0;

  AtpLearnedScorer m;
  m.kind   = (u8)kind;
  m.hidden = (kind == ATP_LEARNED_LINEAR) ? 0u : hidden;
  u32 p = 2u;
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) m.mean[i]    = (float)blob[p++];
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) m.inv_std[i] = (float)blob[p++];
  if (kind == ATP_LEARNED_LINEAR) {
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) m.w1[i] = (float)blob[p++];
    m.b2 = (float)blob[p++];
  } else {
    for (u32 i = 0; i < hidden * ATP_CP_FEATURE_DIM; i++) m.w1[i] = (float)blob[p++];
    for (u32 i = 0; i < hidden; i++) m.b1[i] = (float)blob[p++];
    for (u32 i = 0; i < hidden; i++) m.w2[i] = (float)blob[p++];
    m.b2 = (float)blob[p++];
  }
  g_atp_learned        = m;
  g_atp_learned_active = 1;
  return 1;
}

fn void thvm_atp_clear_learned_scorer(void) {
  g_atp_learned_active = 0;
}

// === ENIGMA Tier 2: in-engine GCN critical-pair scorer ==============
// Runs the GCN forward on thvm's OWN tensor runtime: it builds a UOP
// graph out of the documented constructors (uop_reshape / uop_expand /
// uop_binary / uop_reduce / uop_unary), thvm_realize's it, and reads the
// per-graph proof-relevance score back from the realized buffer.  This
// REPLICATES the WL reference forward (atpGnnForwardLogits in
// wl/THVMLink/Kernel/ATP/ATP.wl): the same row-normalised-adjacency
// message passing, masked-mean pool, and two-class readout, so a model
// trained in WL scores identically in the engine (the test_atp_gnn_score
// differential pins this to ~1e-3).

// The loaded model.  Weights live in one heap-allocated f32 blob laid
// out exactly as the on-the-wire f64 blob documents (see thvm.h): per
// round r, W1[r] (in_r*H), Ws[r] (in_r*H), Bh[r] (H), with in_r = 6 on
// round 0 and H thereafter; then Wout (H*2), Bout (2).  Offsets are
// recomputed on the fly from R/H so the struct stays a single owned
// buffer rather than a fixed-cap matrix.
typedef struct {
  u32    rounds;     // R >= 1
  u32    hidden;     // H >= 1
  float *w;          // owned flat parameter buffer
  u32    w_len;      // element count of `w`
} AtpGnnScorer;

static AtpGnnScorer g_atp_gnn = {0u, 0u, NULL, 0u};
static int          g_atp_gnn_active = 0;
// Dedicated thvm context the GCN forward runs in (lazily created), so its
// per-re-rank tensor/UOP/kernel scratch is reclaimed (thvm_reset) without
// disturbing the ATP engine's term heap in the default context.
static u32          g_atp_gnn_ctx = 0u;

// Input dim feeding round r: ATP_CPG_FEAT_DIM on round 0, H thereafter.
static u32 atp_gnn_in_dim(const AtpGnnScorer *m, u32 r) {
  return (r == 0u) ? ATP_CPG_FEAT_DIM : m->hidden;
}

// Round up to the next power of two.  The GCN score-batch buckets its
// batch (B) dim to powers of two (the node dim N is fixed at a cap) so the
// rendered kernel source is identical across re-ranks with different
// live-queue sizes -- which makes the CPU JIT's on-disk dylib cache hit
// (one ~2s compile per distinct B bucket, then ~5ms per re-rank, vs a 2s
// recompile every time the queue size changed).  Padded rows stay zero
// and their scores are discarded, so bucketing is numerically invisible.
static u32 atp_gnn_pow2_ceil(u32 v) {
  u32 p = 1u;
  while (p < v) p <<= 1;
  return p;
}

// Total flat element count for an (R, H) model - the on-wire blob
// length minus the leading 2 header words.
static u32 atp_gnn_param_count(u32 rounds, u32 hidden) {
  u32 n = 0u;
  for (u32 r = 0u; r < rounds; r++) {
    u32 in_r = (r == 0u) ? ATP_CPG_FEAT_DIM : hidden;
    n += 2u * in_r * hidden;   // W1[r] + Ws[r]
    n += hidden;               // Bh[r]
  }
  n += hidden * 2u;            // Wout
  n += 2u;                     // Bout
  return n;
}

// Offset (into m->w) of W1[r] / Ws[r] / Bh[r] / Wout / Bout.
static u32 atp_gnn_off_round(const AtpGnnScorer *m, u32 r) {
  u32 off = 0u;
  for (u32 i = 0u; i < r; i++) {
    u32 in_i = atp_gnn_in_dim(m, i);
    off += 2u * in_i * m->hidden + m->hidden;
  }
  return off;
}
static const float *atp_gnn_w1(const AtpGnnScorer *m, u32 r) {
  return m->w + atp_gnn_off_round(m, r);
}
static const float *atp_gnn_ws(const AtpGnnScorer *m, u32 r) {
  return m->w + atp_gnn_off_round(m, r) + atp_gnn_in_dim(m, r) * m->hidden;
}
static const float *atp_gnn_bh(const AtpGnnScorer *m, u32 r) {
  return m->w + atp_gnn_off_round(m, r) + 2u * atp_gnn_in_dim(m, r) * m->hidden;
}
static const float *atp_gnn_wout(const AtpGnnScorer *m) {
  return m->w + atp_gnn_off_round(m, m->rounds);   // past the last round block
}
static const float *atp_gnn_bout(const AtpGnnScorer *m) {
  return atp_gnn_wout(m) + m->hidden * 2u;
}

fn int thvm_atp_set_gnn_scorer(const double *blob, u32 len) {
  if (blob == NULL || len < 4u) return 0;
  u32 rounds = (u32)blob[0];
  u32 hidden = (u32)blob[1];
  if (rounds == 0u || hidden == 0u) return 0;
  u32 want = 2u + atp_gnn_param_count(rounds, hidden);
  if (len != want) return 0;

  float *w = (float *)malloc((size_t)(want - 2u) * sizeof(float));
  if (w == NULL) return 0;
  for (u32 i = 0u; i < want - 2u; i++) w[i] = (float)blob[2u + i];

  free(g_atp_gnn.w);
  g_atp_gnn.rounds = rounds;
  g_atp_gnn.hidden = hidden;
  g_atp_gnn.w      = w;
  g_atp_gnn.w_len  = want - 2u;
  g_atp_gnn_active = 1;
  return 1;
}

static void atp_gnn_capture_cache_clear(void);

fn void thvm_atp_clear_gnn_scorer(void) {
  free(g_atp_gnn.w);
  g_atp_gnn.w = NULL;
  g_atp_gnn.w_len = 0u;
  g_atp_gnn.rounds = 0u;
  g_atp_gnn.hidden = 0u;
  g_atp_gnn_active = 0;
  // The captured forward bakes the model weights as constants; a reload
  // with different weights must re-capture, so invalidate the cache.  This
  // runs in the engine context, but atp_gnn_capture_cache_clear selects the
  // GNN sandbox context for the drop so the retained-buffer unpin/decref hit
  // the correct context's CPU_BUFS.
  atp_gnn_capture_cache_clear();
}

fn int thvm_atp_gnn_loaded(void) { return g_atp_gnn_active; }

// Read one named f32 tensor (`count` elements) out of a safetensors file
// into `dst` (as doubles).  `json` is the file's JSON header; `data_start`
// is the absolute byte offset of the data section (8 + header length).
// Tolerant scan: find "<name>": then its "data_offsets":[begin,end].
static int atp_st_read(FILE *f, const char *json, u64 data_start,
                       const char *name, u32 count, double *dst) {
  char key[32];
  snprintf(key, sizeof key, "\"%s\":", name);
  const char *p = strstr(json, key);
  if (p == NULL) return 0;
  const char *q = strstr(p, "\"data_offsets\":[");
  if (q == NULL) return 0;
  long begin = 0, end = 0;
  if (sscanf(q + 16, "%ld,%ld", &begin, &end) != 2) return 0;
  if (end < begin || (u32)((end - begin) / 4) != count) return 0;
  if (fseek(f, (long)(data_start + (u64)begin), SEEK_SET) != 0) return 0;
  float *tmp = (float *)malloc((size_t)count * sizeof(float));
  if (tmp == NULL) return 0;
  int ok = (fread(tmp, sizeof(float), count, f) == count);
  for (u32 i = 0u; ok && i < count; i++) dst[i] = (double)tmp[i];
  free(tmp);
  return ok;
}

// Load a GCN scorer from a .safetensors file (the C-side analog of WL's
// TAtpSetGnnScorer[path]) and push it to the engine -- no WL needed.  The
// file is the one TAtpSaveGnnScorer writes / the bundled GCNAtpScorer
// asset: named f32 tensors W1_<r>/Ws_<r>/Bh_<r> (r 0-based) + Wout + Bout,
// with Rounds/Hidden in __metadata__.  Layout is tinygrad's safetensors
// (nn/state.py): an 8-byte LE u64 header length, that many JSON bytes
// {name:{dtype,shape,data_offsets:[begin,end]},...}, then concatenated LE
// tensor data.  The named tensors are packed into the flat blob order the
// engine expects (per round W1,Ws,Bh; then Wout,Bout), mirroring
// serializeGnnModel.  Returns 1 on success, 0 on any error.
fn int thvm_atp_load_gnn_safetensors(const char *path) {
  if (path == NULL) return 0;
  FILE *f = fopen(path, "rb");
  if (f == NULL) return 0;

  u64 hlen = 0u;
  if (fread(&hlen, 8u, 1u, f) != 1u || hlen == 0u || hlen > (1u << 24)) {
    fclose(f); return 0;
  }
  char *json = (char *)malloc((size_t)hlen + 1u);
  if (json == NULL) { fclose(f); return 0; }
  if (fread(json, 1u, (size_t)hlen, f) != (size_t)hlen) {
    free(json); fclose(f); return 0;
  }
  json[hlen] = '\0';
  u64 data_start = 8u + hlen;

  u32 R = 0u, H = 0u;
  { const char *p = strstr(json, "\"Rounds\":\""); if (p) R = (u32)strtoul(p + 10, NULL, 10); }
  { const char *p = strstr(json, "\"Hidden\":\""); if (p) H = (u32)strtoul(p + 10, NULL, 10); }
  if (R == 0u || H == 0u) { free(json); fclose(f); return 0; }
  // Feature-dim version guard: the forward is compiled for exactly
  // ATP_CPG_FEAT_DIM inputs, so a model saved at a different schema (e.g. a
  // legacy F=6 blob against the F=12 engine) cannot be fed the current
  // graphs.  Reject it cleanly rather than mis-reshaping W1_0.  Absent
  // "Feat" (pre-versioning files) falls through to the per-tensor element
  // count check below, which also catches the mismatch.
  { const char *p = strstr(json, "\"Feat\":\"");
    if (p) {
      u32 feat = (u32)strtoul(p + 8, NULL, 10);
      if (feat != ATP_CPG_FEAT_DIM) { free(json); fclose(f); return 0; }
    } }

  u32 want = 2u + atp_gnn_param_count(R, H);
  double *blob = (double *)malloc((size_t)want * sizeof(double));
  if (blob == NULL) { free(json); fclose(f); return 0; }
  blob[0] = (double)R; blob[1] = (double)H;
  u32 bp = 2u;
  int ok = 1;
  char nm[16];
  for (u32 r = 0u; r < R; r++) {
    u32 inr = (r == 0u) ? ATP_CPG_FEAT_DIM : H;
    snprintf(nm, sizeof nm, "W1_%u", r); ok &= atp_st_read(f, json, data_start, nm, inr * H, blob + bp); bp += inr * H;
    snprintf(nm, sizeof nm, "Ws_%u", r); ok &= atp_st_read(f, json, data_start, nm, inr * H, blob + bp); bp += inr * H;
    snprintf(nm, sizeof nm, "Bh_%u", r); ok &= atp_st_read(f, json, data_start, nm, H,       blob + bp); bp += H;
  }
  ok &= atp_st_read(f, json, data_start, "Wout", H * 2u, blob + bp); bp += H * 2u;
  ok &= atp_st_read(f, json, data_start, "Bout", 2u,     blob + bp); bp += 2u;
  fclose(f);
  free(json);

  int set = (ok && bp == want) ? thvm_atp_set_gnn_scorer(blob, want) : 0;
  free(blob);
  return set;
}

// Wrap a host f32 buffer as a fresh CPU-backend TAG_TEN of the given
// shape.  Allocates an owning buffer (tensor_alloc) and copies `data`
// into it, so the caller's buffer can be reused/freed immediately.
static Term atp_gnn_ten(const float *data, u32 ndim, const u32 *dims) {
  Shape sh = {0};
  sh.ndim = ndim;
  for (u32 i = 0u; i < ndim; i++) sh.dims[i] = dims[i];
  u32 tid = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u64 nbytes = dtype_storage_bytes(DT_FP32, (u64)TENS[tid].view.numel);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, nbytes);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

// Batched matmul (B,P,K).(B,K,Q) -> (B,P,Q) via the reshape/expand/
// MUL/reduce-SUM pattern - the exact lowering atpBatchMatMul uses in
// the WL reference.
static Term atp_gnn_bmm(Term a, Term b, u32 B, u32 P, u32 K, u32 Q) {
  u32 a4[4] = {B, P, K, 1u}, e4[4] = {B, P, K, Q}, b4[4] = {B, 1u, K, Q};
  Term ae = uop_expand(uop_reshape(a, 4u, a4), 4u, e4);
  Term be = uop_expand(uop_reshape(b, 4u, b4), 4u, e4);
  return uop_reduce(REDUCE_SUM, 2u, uop_binary(UOP_MUL, ae, be));   // {B,P,Q}
}

// Apply a per-feature weight w {K,H} to node tensor h {B,P,K} -> {B,P,H}.
// The WL reference does a 2-D TMatMul over the flattened (B*P, K); here
// we broadcast w to {B,K,H} and reuse the batched matmul - numerically
// identical (a sum over K of h[b,p,k]*w[k,hh]), keeping every op on the
// thvm runtime with one primitive.
static Term atp_gnn_applyw(Term h, Term w, u32 B, u32 P, u32 K, u32 H) {
  u32 w3[3] = {1u, K, H}, we[3] = {B, K, H};
  Term wb = uop_expand(uop_reshape(w, 3u, w3), 3u, we);
  return atp_gnn_bmm(h, wb, B, P, K, H);
}

// Broadcast a length-D bias to a {B,P,D} tensor.
static Term atp_gnn_bcast_bias(Term bias, u32 B, u32 P, u32 D) {
  u32 b3[3] = {1u, 1u, D}, be[3] = {B, P, D};
  return uop_expand(uop_reshape(bias, 3u, b3), 3u, be);
}

// ReLU(x) = x * (0 < x), the TReLU lowering (MUL[x, CMPLT[const0, x]]).
static Term atp_gnn_relu(Term x) {
  Term zero = uop_const(DT_FP32, 0u);          // 0.0f bits
  return uop_binary(UOP_MUL, x, uop_binary(UOP_CMPLT, zero, x));
}

// Bound the GCN inference shape.  The node dim N is FIXED at a cap (a CP
// term with hundreds of nodes is far outside the trained NMax, and the
// dense {B,N,N} adjacency is O(N^2) -- an 800-node CP at batch 4096 is
// ~17 GB, which calloc overcommits and the kernel then SIGBUSes on touch),
// so graphs larger than the cap are TRUNCATED to the first N_CAP nodes.
// The batch is processed in chunks of <= B_CAP.  Both caps are powers of
// two; with N fixed, only a handful of B buckets ever compile (each once,
// persisted on disk), and peak scratch is bounded by B_CAP * N_CAP^2.
#define ATP_GNN_N_CAP 64u
#define ATP_GNN_B_CAP 1024u
// Neutral secondary priority for a not-yet-GNN-scored CP in coop mode:
// the score-0 point of the GNN priority map (1e6 - 1e4*score), so fresh
// CPs sit mid-band until the next re-rank instead of on the heuristic
// weight scale (~tens), which would otherwise always out-rank GNN picks.
#define ATP_GNN_COOP_NEUTRAL_PRI 1000000u

// Per-batch-shape JIT capture cache.  The GCN forward is structurally
// IDENTICAL across re-ranks at a given batch bucket B (the node dim N is
// fixed; weights are constant for the run), so re-running the scheduler
// (realize_classify / topo-sort / materialize / codegen) on every re-rank
// is ~4-5 ms of pure waste -- the kernels themselves are cheap.  We use
// thvm's JIT capture/replay (src/jit/capture.c): the first re-rank at a
// bucket builds the forward, captures the kernel-dispatch sequence around
// one thvm_realize, and pins the persistent input + output buffers; later
// re-ranks at the same bucket rewrite the input buffer bytes in place and
// jit_replay the recorded sequence -- no re-schedule, no re-codegen.
//
// B is bucketed to a power of two in [4, ATP_GNN_B_CAP=1024], so at most
// 9 distinct buckets ever compile (each captured once).  thvm_reset (fired
// near the descriptor caps) drops every capture slot AND frees the sandbox
// buffers, so it must invalidate this cache; atp_gnn_capture_cache_clear
// does that wherever the reset is issued.
typedef struct {
  u32  B;            // batch bucket this capture serves (0 = empty slot)
  u32  slot;         // jit capture slot id (>= 1 when valid)
  u32  x_tid;        // persistent input TenDescs (stable buf_ids)
  u32  a_tid;
  u32  mask_tid;
  u32  nn_tid;
  u32  x_buf;        // their buf_ids, declared as the jit input baseline
  u32  a_buf;
  u32  mask_buf;
  u32  nn_buf;
  u32  out_tid;      // realized logits tensor (its buf is rewritten by replay)
} AtpGnnCapture;

// One entry per power-of-two bucket from 4 up to ATP_GNN_B_CAP.
#define ATP_GNN_CAP_SLOTS 12
static AtpGnnCapture g_atp_gnn_captures[ATP_GNN_CAP_SLOTS];

// Drop every cached GCN capture.  Called wherever thvm_reset is issued in
// the sandbox context, AND from thvm_atp_clear_gnn_scorer (which runs in the
// engine context).  jit_capture_drop releases each capture's retained
// buffers via buf_jit_unpin / buf_decref, and those hooks index the CURRENT
// context's CPU_BUFS (CPU_BUFS == CURRENT_CTX->cpu_bufs).  The retained
// buf_ids name buffers in the GNN sandbox context (g_atp_gnn_ctx), so the
// drop MUST run with that context selected -- otherwise the unpin/decref
// lands on whatever buffer happens to share that id in the engine context,
// dropping a live engine buffer's refcount to 0 and freeing it under a kernel
// that still reads it (the train-then-rerank SIGSEGV).  Select the sandbox
// context for the drop and restore the caller's context after.
static void atp_gnn_capture_cache_clear(void) {
  int have_slots = 0;
  for (u32 i = 0u; i < ATP_GNN_CAP_SLOTS; i++) {
    if (g_atp_gnn_captures[i].slot != 0u) { have_slots = 1; break; }
  }
  u32 prev = 0u;
  int switched = 0;
  if (have_slots && g_atp_gnn_ctx != 0u
      && thvm_context_current() != g_atp_gnn_ctx) {
    prev = thvm_context_select(g_atp_gnn_ctx);
    switched = 1;
  }
  for (u32 i = 0u; i < ATP_GNN_CAP_SLOTS; i++) {
    if (g_atp_gnn_captures[i].slot != 0u) {
      jit_capture_drop(g_atp_gnn_captures[i].slot);
    }
    g_atp_gnn_captures[i] = (AtpGnnCapture){0};
  }
  if (switched) thvm_context_select(prev);
}

// Find the cache entry for bucket B, or the first empty slot to fill.
// Returns NULL only when every slot is taken by a different bucket (cannot
// happen: at most 9 distinct buckets, 12 slots).
static AtpGnnCapture *atp_gnn_capture_for(u32 B) {
  AtpGnnCapture *empty = NULL;
  for (u32 i = 0u; i < ATP_GNN_CAP_SLOTS; i++) {
    if (g_atp_gnn_captures[i].B == B && g_atp_gnn_captures[i].slot != 0u) {
      return &g_atp_gnn_captures[i];
    }
    if (empty == NULL && g_atp_gnn_captures[i].slot == 0u) {
      empty = &g_atp_gnn_captures[i];
    }
  }
  return empty;
}

// Allocate a persistent f32 input tensor of the given shape in the current
// (sandbox) context.  Unlike atp_gnn_ten this leaves the buffer un-written:
// the caller fills it via atp_gnn_buf_fill before each capture/replay.
static u32 atp_gnn_input_tensor(u32 ndim, const u32 *dims) {
  Shape sh = {0};
  sh.ndim = ndim;
  for (u32 i = 0u; i < ndim; i++) sh.dims[i] = dims[i];
  return tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
}

// Write `n` floats into a tensor's buffer in place (the captured kernels
// read this buf_id, so this refreshes their input bytes for replay).
static void atp_gnn_buf_fill(u32 tid, const float *data, u64 n) {
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (const void *)data,
                        (u64)n * sizeof(float));
}

// Build the host X / A / mask / NNodes arrays for one chunk of `nc` CP
// graphs padded to batch B at the fixed node dim N.  Padded rows keep
// NNodes=1 (finite pooled reciprocal) and all-zero X/A/mask, so they are
// inert and their scores are discarded.
static void atp_gnn_build_host(const AtpCpGraph *gs, u32 nc, u32 B,
                               float *xArr, float *aArr, float *maskArr,
                               float *nnArr) {
  const u32 F = ATP_CPG_FEAT_DIM, N = ATP_GNN_N_CAP;
  memset(xArr,    0, (size_t)B * N * F * sizeof(float));
  memset(aArr,    0, (size_t)B * N * N * sizeof(float));
  memset(maskArr, 0, (size_t)B * N * sizeof(float));
  memset(nnArr,   0, (size_t)B * sizeof(float));
  for (u32 bi = nc; bi < B; bi++) nnArr[bi] = 1.0f;
  for (u32 bi = 0u; bi < nc; bi++) {
    const AtpCpGraph *g = &gs[bi];
    u32 nn = (g->n_nodes < N) ? g->n_nodes : N;   // truncate giant graphs
    nnArr[bi] = (float)nn;
    for (u32 ni = 0u; ni < nn; ni++) {
      maskArr[(size_t)bi * N + ni] = 1.0f;
      for (u32 fj = 0u; fj < F; fj++) {
        xArr[((size_t)bi * N + ni) * F + fj] =
            g->node_feat[(size_t)ni * ATP_CPG_FEAT_DIM + fj];
      }
    }
    float *adj = aArr + (size_t)bi * N * N;
    for (u32 e = 0u; e < g->n_edges; e++) {
      u32 src = g->edge_src[e], dst = g->edge_dst[e];
      if (src >= nn || dst >= nn) continue;        // drop truncated edges
      adj[(size_t)src * N + dst] = 1.0f;
      adj[(size_t)dst * N + src] = 1.0f;
    }
    for (u32 ii = 0u; ii < nn; ii++) adj[(size_t)ii * N + ii] = 1.0f;
    for (u32 ii = 0u; ii < N; ii++) {              // row-normalise
      float rowsum = 0.0f;
      for (u32 jj = 0u; jj < N; jj++) rowsum += adj[(size_t)ii * N + jj];
      if (rowsum > 0.0f) {
        for (u32 jj = 0u; jj < N; jj++) adj[(size_t)ii * N + jj] /= rowsum;
      }
    }
  }
}

// Build the GCN forward UOP graph over the four input Terms and return the
// {B,2} logits node.  No kernels fire here (lazy until thvm_realize).
static Term atp_gnn_forward(const AtpGnnScorer *m, u32 B,
                            Term xT, Term aT, Term maskT, Term nnT) {
  const u32 F = ATP_CPG_FEAT_DIM, H = m->hidden, R = m->rounds;
  const u32 N = ATP_GNN_N_CAP;

  Term h = xT;
  u32 lastDim = F;
  for (u32 r = 0u; r < R; r++) {
    u32 inr = atp_gnn_in_dim(m, r);
    Term w1T = atp_gnn_ten(atp_gnn_w1(m, r), 2u, (u32[]){inr, H});
    Term wsT = atp_gnn_ten(atp_gnn_ws(m, r), 2u, (u32[]){inr, H});
    Term bhT = atp_gnn_ten(atp_gnn_bh(m, r), 1u, (u32[]){H});
    Term ah  = atp_gnn_bmm(aT, h, B, N, N, lastDim);          // A.H {B,N,lastDim}
    Term nb  = atp_gnn_applyw(ah, w1T, B, N, lastDim, H);     // (A.H).W1
    Term sf  = atp_gnn_applyw(h, wsT, B, N, lastDim, H);      // H.Ws
    Term msg = uop_binary(UOP_ADD,
                          uop_binary(UOP_ADD, nb, sf),
                          atp_gnn_bcast_bias(bhT, B, N, H));
    h = atp_gnn_relu(msg);
    lastDim = H;
  }

  // Masked-mean pool: (B,1,N).(B,N,H) -> (B,1,H), divide by NNodes -> (B,H).
  Term pooled3 = atp_gnn_bmm(maskT, h, B, 1u, N, H);          // {B,1,H}
  Term nnInv   = uop_unary(UOP_RECIP, nnT);                   // {B,1,1}
  Term nnInvE  = uop_expand(nnInv, 3u, (u32[]){B, 1u, H});
  Term pooled  = uop_reshape(uop_binary(UOP_MUL, pooled3, nnInvE),
                             2u, (u32[]){B, H});              // {B,H}

  // Readout: pooled.Wout + Bout -> {B,2}.
  Term woutT   = atp_gnn_ten(atp_gnn_wout(m), 2u, (u32[]){H, 2u});
  Term boutT   = atp_gnn_ten(atp_gnn_bout(m), 1u, (u32[]){2u});
  Term pooled3b = uop_reshape(pooled, 3u, (u32[]){B, 1u, H});
  Term woutE   = uop_expand(uop_reshape(woutT, 3u, (u32[]){1u, H, 2u}),
                            3u, (u32[]){B, H, 2u});
  Term logit3  = atp_gnn_bmm(pooled3b, woutE, B, 1u, H, 2u);  // {B,1,2}
  Term logits  = uop_reshape(logit3, 2u, (u32[]){B, 2u});
  Term boutE   = uop_expand(uop_reshape(boutT, 2u, (u32[]){1u, 2u}),
                            2u, (u32[]){B, 2u});
  return uop_binary(UOP_ADD, logits, boutE);                 // {B,2}
}

// Read the {B,2} logits buffer and write nc scores (logit_pos - logit_neg).
static int atp_gnn_read_scores(u32 out_tid, u32 B, u32 nc, float *out) {
  if (out_tid == 0u || out_tid >= TENS_NEXT) return 0;
  float *o = (float *)malloc((size_t)B * 2u * sizeof(float));
  if (o == NULL) return 0;
  TENS[out_tid].backend->buf_read(TENS[out_tid].buf_id, o,
                                  (u64)B * 2u * sizeof(float));
  for (u32 i = 0u; i < nc; i++) {
    out[i] = o[(size_t)i * 2u + 1u] - o[(size_t)i * 2u + 0u];
  }
  free(o);
  return 1;
}

// Score one chunk of <= ATP_GNN_B_CAP CP graphs at the FIXED node dim
// N = ATP_GNN_N_CAP.  Must run inside the sandbox context (g_atp_gnn_ctx).
// Writes `nc` scores to out[0..nc); returns 1 on success.
//
// First call at a given batch bucket B captures the forward's kernel-
// dispatch sequence; every later call at the same bucket rewrites the
// persistent input buffers in place and replays the capture -- bypassing
// the scheduler entirely.
static int atp_gnn_score_chunk(const AtpGnnScorer *m, const AtpCpGraph *gs,
                               u32 nc, float *out) {
  const u32 F = ATP_CPG_FEAT_DIM;
  const u32 N = ATP_GNN_N_CAP;
  const u32 B = atp_gnn_pow2_ceil(nc < 4u ? 4u : nc);   // <= ATP_GNN_B_CAP

  // X (B,N,F), A (B,N,N row-normalised adjacency + self-loops), MaskRow
  // (B,1,N), NNodes (B,1,1).
  float *xArr    = (float *)malloc((size_t)B * N * F * sizeof(float));
  float *aArr    = (float *)malloc((size_t)B * N * N * sizeof(float));
  float *maskArr = (float *)malloc((size_t)B * N * sizeof(float));
  float *nnArr   = (float *)malloc((size_t)B * sizeof(float));
  if (xArr == NULL || aArr == NULL || maskArr == NULL || nnArr == NULL) {
    free(xArr); free(aArr); free(maskArr); free(nnArr);
    return 0;
  }
  atp_gnn_build_host(gs, nc, B, xArr, aArr, maskArr, nnArr);

  int ok = 0;
  AtpGnnCapture *cap = atp_gnn_capture_for(B);

  if (cap != NULL && cap->slot != 0u) {
    // Warm path: rewrite the persistent input buffers in place, replay the
    // recorded dispatch sequence (no re-schedule), read the output buffer.
    atp_gnn_buf_fill(cap->x_tid,    xArr,    (u64)B * N * F);
    atp_gnn_buf_fill(cap->a_tid,    aArr,    (u64)B * N * N);
    atp_gnn_buf_fill(cap->mask_tid, maskArr, (u64)B * N);
    atp_gnn_buf_fill(cap->nn_tid,   nnArr,   (u64)B);
    u32 ids[4] = {cap->x_buf, cap->a_buf, cap->mask_buf, cap->nn_buf};
    jit_replay_with_inputs(cap->slot, ids, 4u);
    ok = atp_gnn_read_scores(cap->out_tid, B, nc, out);
  } else if (cap != NULL) {
    // Cold path: allocate persistent inputs, fill them, build + capture the
    // forward around one realize, declare the input baseline, and cache it.
    u32 xTid    = atp_gnn_input_tensor(3u, (u32[]){B, N, F});
    u32 aTid    = atp_gnn_input_tensor(3u, (u32[]){B, N, N});
    u32 maskTid = atp_gnn_input_tensor(3u, (u32[]){B, 1u, N});
    u32 nnTid   = atp_gnn_input_tensor(3u, (u32[]){B, 1u, 1u});
    atp_gnn_buf_fill(xTid,    xArr,    (u64)B * N * F);
    atp_gnn_buf_fill(aTid,    aArr,    (u64)B * N * N);
    atp_gnn_buf_fill(maskTid, maskArr, (u64)B * N);
    atp_gnn_buf_fill(nnTid,   nnArr,   (u64)B);

    Term xT    = term_new(0, TAG_TEN, DT_FP32, xTid);
    Term aT    = term_new(0, TAG_TEN, DT_FP32, aTid);
    Term maskT = term_new(0, TAG_TEN, DT_FP32, maskTid);
    Term nnT   = term_new(0, TAG_TEN, DT_FP32, nnTid);
    Term logits = atp_gnn_forward(m, B, xT, aT, maskT, nnT);

    u32 slot = jit_capture_begin();
    Term realized = term_resolve(thvm_realize(logits));
    if (slot != 0u) {
      jit_capture_end_with_result(realized);
    }
    if (term_tag(realized) == TAG_TEN) {
      u32 outTid = (u32)term_val(realized);
      ok = atp_gnn_read_scores(outTid, B, nc, out);
      if (ok && slot != 0u) {
        u32 ids[4] = {TENS[xTid].buf_id, TENS[aTid].buf_id,
                      TENS[maskTid].buf_id, TENS[nnTid].buf_id};
        jit_capture_set_inputs(slot, ids, 4u);
        *cap = (AtpGnnCapture){
          .B = B, .slot = slot,
          .x_tid = xTid, .a_tid = aTid, .mask_tid = maskTid, .nn_tid = nnTid,
          .x_buf = ids[0], .a_buf = ids[1], .mask_buf = ids[2], .nn_buf = ids[3],
          .out_tid = outTid
        };
      } else if (slot != 0u) {
        jit_capture_drop(slot);
      }
    } else if (slot != 0u) {
      jit_capture_drop(slot);
    }
  }

  free(xArr); free(aArr); free(maskArr); free(nnArr);
  return ok;
}

// M2: score `n` CPs with the loaded GNN on the thvm UOP runtime.  CP graphs
// are extracted in the CURRENT (engine) context; the tensor forward then
// runs in a dedicated sandbox context (g_atp_gnn_ctx) so its scratch is
// reclaimable without touching the engine's term heap, and the batch is
// chunked at ATP_GNN_B_CAP / capped at ATP_GNN_N_CAP nodes.
fn int thvm_atp_gnn_score_batch(const Term *lhs, const Term *rhs,
                                u32 n, float *out_scores) {
  if (!g_atp_gnn_active || n == 0u || lhs == NULL || rhs == NULL
      || out_scores == NULL) {
    return 0;
  }
  const AtpGnnScorer *m = &g_atp_gnn;

  // Extract every CP graph in the engine context (reads the engine's CP
  // terms).  gs[] is plain memory, so it survives the context switch below.
  AtpCpGraph *gs = (AtpCpGraph *)malloc((size_t)n * sizeof(AtpCpGraph));
  if (gs == NULL) return 0;
  for (u32 i = 0u; i < n; i++) {
    // No goal context here (the M2 anchor scores bare Term pairs); the
    // goal-relevance columns stay 0, matching the offline parity harness.
    if (!thvm_atp_cp_graph(NULL, lhs[i], rhs[i], &gs[i])) {
      free(gs); return 0;
    }
  }

  // Switch to the sandbox context; reclaim prior scratch when near the
  // descriptor caps (amortised to ~one reset per thousands of re-ranks).
  u32 prev = thvm_context_current();
  if (g_atp_gnn_ctx == 0u) g_atp_gnn_ctx = thvm_context_create("cpu");
  if (g_atp_gnn_ctx == 0u) { free(gs); return 0; }
  thvm_context_select(g_atp_gnn_ctx);
  if (TENS_NEXT > TENS_CAP / 2u || KERNELS_NEXT > KERNELS_CAP / 2u) {
    // thvm_reset frees the sandbox buffers + every capture slot, so the
    // cached forward captures are now stale -- drop them before reset so
    // the next re-rank at each bucket re-captures from scratch.
    atp_gnn_capture_cache_clear();
    thvm_reset();
  }

  int ok = 1;
  for (u32 off = 0u; ok && off < n; off += ATP_GNN_B_CAP) {
    u32 nc = (n - off < ATP_GNN_B_CAP) ? (n - off) : ATP_GNN_B_CAP;
    ok = atp_gnn_score_chunk(m, gs + off, nc, out_scores + off);
  }

  thvm_context_select(prev);
  free(gs);
  return ok;
}

// Score a batch of PRE-BUILT graphs with the loaded GNN -- the same
// sandbox-context + chunked forward as thvm_atp_gnn_score_batch, minus
// the per-CP thvm_atp_cp_graph extraction (the caller supplies gs[]).
// The offline parity harness uses this to score exported graphs by the
// exact in-engine forward.
fn int thvm_atp_gnn_score_graphs(const AtpCpGraph *gs, u32 n,
                                 float *out_scores) {
  if (!g_atp_gnn_active || n == 0u || gs == NULL || out_scores == NULL) {
    return 0;
  }
  const AtpGnnScorer *m = &g_atp_gnn;
  u32 prev = thvm_context_current();
  if (g_atp_gnn_ctx == 0u) g_atp_gnn_ctx = thvm_context_create("cpu");
  if (g_atp_gnn_ctx == 0u) return 0;
  thvm_context_select(g_atp_gnn_ctx);
  if (TENS_NEXT > TENS_CAP / 2u || KERNELS_NEXT > KERNELS_CAP / 2u) {
    atp_gnn_capture_cache_clear();
    thvm_reset();
  }
  int ok = 1;
  for (u32 off = 0u; ok && off < n; off += ATP_GNN_B_CAP) {
    u32 nc = (n - off < ATP_GNN_B_CAP) ? (n - off) : ATP_GNN_B_CAP;
    ok = atp_gnn_score_chunk(m, gs + off, nc, out_scores + off);
  }
  thvm_context_select(prev);
  return ok;
}

// Parental pre-filter: pick the heap positions of the CPs to GNN-score
// this re-rank.  When gnn_topk > 0 and the queue is larger, keep only the
// top gnn_topk by the CHEAP score -- the Tier-1 logit if that model is
// loaded, else the classical cp_pri read straight off the slot with NO
// term unpack -- via a bounded top-k min-heap (root = worst kept).  This
// bounds the GNN forward to O(gnn_topk) graphs no matter how large the
// live queue grows.  Fills pos[0..return) with heap positions; returns the
// count.  topk == 0 or nq <= topk keeps the whole queue in heap order.
static u32 atp_gnn_rerank_select(AtpState *s, u32 nq, u32 topk, u32 *pos) {
  if (topk == 0u || nq <= topk) {
    for (u32 i = 0u; i < nq; i++) pos[i] = i;
    return nq;
  }
  int tier1 = g_atp_learned_active;
  float *hg = (float *)malloc((size_t)topk * sizeof(float));  // goodness
  u32   *hp = (u32 *)malloc((size_t)topk * sizeof(u32));       // position
  if (hg == NULL || hp == NULL) {              // OOM: whole queue in order
    free(hg); free(hp);
    for (u32 i = 0u; i < nq; i++) pos[i] = i;
    return nq;
  }
  u32 hn = 0u;
  for (u32 i = 0u; i < nq; i++) {
    u32 vv = s->cp_heap[i].row;
    // Higher goodness = more worth GNN-scoring.  cp_pri is "lower is
    // better", so negate it; the Tier-1 logit is already "higher better".
    float good;
    if (tier1) {
      Term l = 0, r = 0;
      atp_cp_slot_read(s, vv, &l, &r);
      float feat[ATP_CP_FEATURE_DIM];
      thvm_atp_cp_features(s, l, r, s->cp_seq[vv], feat);
      good = atp_learned_forward(&g_atp_learned, feat);
    } else {
      good = -(float)s->cp_pri[vv];
    }
    if (hn < topk) {                           // grow, sift up
      u32 c = hn++;
      hg[c] = good; hp[c] = i;
      while (c > 0u) {
        u32 p = (c - 1u) / 2u;
        if (hg[p] <= hg[c]) break;
        float tg = hg[p]; hg[p] = hg[c]; hg[c] = tg;
        u32   tp = hp[p]; hp[p] = hp[c]; hp[c] = tp;
        c = p;
      }
    } else if (good > hg[0]) {                  // evict worst, sift down
      hg[0] = good; hp[0] = i;
      u32 c = 0u;
      for (;;) {
        u32 l2 = 2u * c + 1u, r2 = 2u * c + 2u, mn = c;
        if (l2 < topk && hg[l2] < hg[mn]) mn = l2;
        if (r2 < topk && hg[r2] < hg[mn]) mn = r2;
        if (mn == c) break;
        float tg = hg[mn]; hg[mn] = hg[c]; hg[c] = tg;
        u32   tp = hp[mn]; hp[mn] = hp[c]; hp[c] = tp;
        c = mn;
      }
    }
  }
  for (u32 i = 0u; i < hn; i++) pos[i] = hp[i];
  free(hg); free(hp);
  return hn;
}

// M3: re-rank the live CP queue with the loaded GNN.  Select -> build ->
// score -> map to priority -> push back.  Only permutes order
// (completeness safe).  The parental pre-filter (atp_gnn_rerank_select)
// caps the GNN forward at gnn_topk graphs regardless of queue size, and
// thvm_atp_cp_graph reads the goal cone via `s` so the online graphs carry
// the same goal-relevance columns the training corpus does.
fn u32 thvm_atp_gnn_rerank(AtpState *s) {
  if (s == NULL || !g_atp_gnn_active) return 0u;
  u32 nq = thvm_atp_queued_cp_count(s);
  if (nq == 0u) return 0u;

  u32 topk = s->gnn_topk;
  u32 m = (topk > 0u && nq > topk) ? topk : nq;   // CPs actually GNN-scored

  u32        *pos = (u32 *)malloc((size_t)nq * sizeof(u32));    // selection
  AtpCpGraph *gs  = (AtpCpGraph *)malloc((size_t)m * sizeof(AtpCpGraph));
  u32        *seq = (u32 *)malloc((size_t)m * sizeof(u32));
  float      *sc  = (float *)malloc((size_t)m * sizeof(float));
  u32        *pri = (u32 *)malloc((size_t)m * sizeof(u32));
  if (pos == NULL || gs == NULL || seq == NULL || sc == NULL || pri == NULL) {
    free(pos); free(gs); free(seq); free(sc); free(pri);
    return 0u;
  }

  u32 nsel = atp_gnn_rerank_select(s, nq, topk, pos);
  // Unpack the selected CPs and build their goal-aware graphs in the
  // engine context; skip any that overflow the node cap, compacting into
  // gs / seq.
  u32 k = 0u;
  for (u32 i = 0u; i < nsel; i++) {
    u32 vv = s->cp_heap[pos[i]].row;
    Term l = 0, r = 0;
    atp_cp_slot_read(s, vv, &l, &r);
    if (thvm_atp_cp_graph(s, l, r, &gs[k])) { seq[k] = s->cp_seq[vv]; k++; }
  }
  if (k == 0u || !thvm_atp_gnn_score_graphs(gs, k, sc)) {
    free(pos); free(gs); free(seq); free(sc); free(pri);
    return 0u;
  }
  // Higher score -> lower priority, clamped to a safe u32 band - the same
  // map atp_cp_learned_priority + atpRerankPriority use.  Neutral (score
  // 0) maps to 1.0e6, the cp_pri2 push-time default in coop mode.
  for (u32 i = 0u; i < k; i++) {
    float pr = 1.0e6f - 1.0e4f * sc[i];
    if (pr < 0.0f) pr = 0.0f;
    if (pr > 2.0e9f) pr = 2.0e9f;
    pri[i] = (u32)pr;
  }
  // Coop mode: write the SECONDARY dimension (cp_pri2) so the GNN drives
  // the w2 coop pick while the primary heap stays the hand heuristic
  // (e.g. Waldmeister).  Otherwise overwrite the primary heap (GT-mode).
  if (s->gnn_coop) {
    thvm_atp_set_cp_pri2_by_seq(s, seq, pri, k);
  } else {
    thvm_atp_set_cp_pri_by_seq(s, seq, pri, k);
  }
  s->n_gnn_reranks++;

  free(pos); free(gs); free(seq); free(sc); free(pri);
  return k;
}

fn void thvm_atp_set_gnn_rerank_period(AtpState *s, u32 period) {
  if (s == NULL) return;
  s->gnn_rerank_period = period;
}

fn void thvm_atp_set_gnn_topk(AtpState *s, u32 topk) {
  if (s == NULL) return;
  s->gnn_topk = topk;
}

// Enable GNN coop: the re-rank writes the secondary dimension (cp_pri2)
// and select_cp picks the GNN's top CP every `ratio`-th selection (the w2
// coop branch), while the primary heap keeps the hand heuristic.  ratio 0
// disables coop (GNN overwrites the primary heap).  Sets w2_modulo so the
// coop pick fires; the caller still sets gnn_rerank_period (how often the
// GNN re-scores) and loads the scorer.
fn void thvm_atp_set_gnn_coop(AtpState *s, u32 ratio) {
  if (s == NULL) return;
  if (ratio == 0u) { s->gnn_coop = 0u; return; }
  s->gnn_coop  = 1u;
  s->w2_modulo = ratio;
}

static u32 atp_cp_learned_priority(AtpState *s, Term lhs, Term rhs) {
  float feat[ATP_CP_FEATURE_DIM];
  thvm_atp_cp_features(s, lhs, rhs, s->cp_seq_next, feat);
  float score;
  if (g_atp_learned_active) {
    score = atp_learned_forward(&g_atp_learned, feat);
  } else {
    score = ATP_LEARNED_B;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) score += ATP_LEARNED_W[i] * feat[i];
  }
  // Map score (typically ~[-6, 4]) to a u32 priority, higher score ->
  // lower priority.  Clamp into a safe positive band.
  float pr = 1.0e6f - 1.0e4f * score;
  if (pr < 0.0f) pr = 0.0f;
  if (pr > 2.0e9f) pr = 2.0e9f;
  return (u32)pr;
}

static u32 atp_cp_priority(AtpState *s, Term lhs, Term rhs) {
  if (s->cp_weight_mode == ATP_CP_WEIGHT_LEARNED) {
    return atp_cp_learned_priority(s, lhs, rhs);
  }
  return atp_cp_priority_sized(s, lhs, rhs,
                               atp_symbol_count(lhs) + atp_symbol_count(rhs));
}

// === 7c': CP-queue binary min-heap ==================================
//
// The CP queue (cp_packed/cp_trace/cp_pri/cp_seq) is kept as a
// binary min-heap ordered by (cp_pri, cp_seq): cheapest priority
// first, insertion order breaking ties.  This reproduces the old
// `--add` selection order (collapse_ordered sorted by INC depth,
// ties by queue index) but at O(log n) per push/pop instead of
// rebuilding an n-leaf INC-SUP tree + collapse on every step.

// Ordering predicate: does the CP at heap POSITION i sort strictly
// before the one at position j?  The sift-hot keys (pri / seq /
// ultimate) are read from the position's packed node -- exact caches
// of the stationary-row payload (AtpCpHeapNode refresh invariant,
// thvm.h), so every verdict is provably identical to the historical
// row loads while a compare touches ONE cache line per node.  Only the
// mode-gated extra keys (collapse-defer / tie-rhs packed-byte walks)
// fall through to row loads via node->row.
static int atp_cp_before(const AtpState *s, u32 i, u32 j) {
  const AtpCpHeapNode *na = &s->cp_heap[i], *nb = &s->cp_heap[j];
  // Waldmeister history-driven Act_ultimate: a CP tagged "ultimate"
  // ranks strictly before every non-ultimate CP, regardless of
  // heuristic weight (NewClassification.c sets w1 = minimalWeight()
  // = INT32_MIN for the `initial = ultimate` action; we encode it as
  // an out-of-band bit so reheapify cannot scramble the order).  The
  // flag is OFF by default -- engine byte-identical.  aux caches
  // cp_ultimate[row] (the WM preset turns this gate ON via the intake
  // canonicalize, so it IS on the default-bench compare path).
  if ((s->use_initial_ultimate || s->use_database_ultimate)
      && s->cp_ultimate != NULL) {
    u32 ui = na->aux, uj = nb->aux;
    if (ui != uj) return ui > uj;
    // Both ultimate: WM stamps EVERY Act_ultimate CP w1 =
    // minimalWeight() = INT32_MIN (NewClassification.c:330), so the
    // (w1, w2) heap key degenerates to w2 alone -- pure FIFO within
    // the ultimate class.  The computed cp_pri is deliberately
    // ignored here; for axioms the FIFO stamps carry the canonical
    // loader-sort order (atp_wm_intake_canonicalize).
    if (ui) return na->seq < nb->seq;
  }
  // Collapser deferral (opt-in, THVM_ATP_COLLAPSE_DEFER=<penalty>): add a
  // priority penalty to bare-variable-rhs CPs (collapsers), deferring their
  // selection past the compound CPs WM forms first.  thvm forms rule x axiom
  // collapser overlaps EARLIER than WM's DSBaum emission order, so its
  // collapser-heavy trajectory over-explores (CP-queue explosion) where WM
  // stays lean.  Penalizing collapsers approximates WM's later timing without
  // reconstructing the emission age.  Off by default (< 0 sentinel).
  static int cdef = -2;
  if (cdef == -2) {
    const char *e = getenv("THVM_ATP_COLLAPSE_DEFER");
    cdef = e ? (int)strtoul(e, NULL, 10) : -1;
  }
  if (cdef >= 0) {
    // Mode-gated (off by default): the rhs-node count lives in the
    // packed bytes -- a row load through node->row.
    u32 pi = na->pri
             + (acp_rhs_nodes(s->cp_packed[na->row]) == 1u ? (u32)cdef : 0u);
    u32 pj = nb->pri
             + (acp_rhs_nodes(s->cp_packed[nb->row]) == 1u ? (u32)cdef : 0u);
    if (pi != pj) return pi < pj;
  } else {
    if (na->pri != nb->pri) return na->pri < nb->pri;
  }
  // Weight-tie tiebreak (opt-in, THVM_ATP_TIE_RHS): among CPs sharing the
  // WMQ weight bucket (S*S+S-1 depends only on total size, so a compound
  // and a collapser of equal total size tie), prefer the LARGER-rhs CP.
  // WM's -auto age order picks the shallow-overlap compound (large rhs)
  // before the deep-overlap collapser (bare-variable rhs) of the same
  // weight; thvm's raw cp_seq does the reverse (the collapser's overlap is
  // formed earlier).  Preferring the larger rhs approximates WM's pick
  // without reconstructing its full DSBaum emission age.  Off by default
  // -> engine/dylib byte-identical unless the flag is set.
  static int tie_rhs = -1;
  if (tie_rhs < 0) tie_rhs = getenv("THVM_ATP_TIE_RHS") ? 1 : 0;
  if (tie_rhs) {
    // Mode-gated (off by default): packed-byte walk via node->row.
    u32 ri = acp_rhs_nodes(s->cp_packed[na->row]);
    u32 rj = acp_rhs_nodes(s->cp_packed[nb->row]);
    if (ri != rj) return ri > rj;   // larger rhs (compound) selected first
  }
  // Twee-faithful passive tie-break (opt-in, THVM_ATP_TWEE_TIEBREAK):
  // Twee's `instance Ord Passive` (Twee.hs:260-266) breaks a score tie by
  // (max parent-rule-id, min parent-rule-id) on the two overlap rules --
  // preferring the CP whose later-born parent is earliest.  thvm's default
  // is cp_seq (FIFO formation order); the two diverge, cascading (via the
  // completion) into different rule sets, so thvm reaches Twee's goal-joining
  // configuration on a different -- and, on the twee-only cluster, later --
  // schedule.  Sound: a passive tie-break only reorders selection, never
  // the equational theory.  Off by default -> byte-identical (WM preset).
  static int twee_tb = -1;
  if (twee_tb < 0) twee_tb = getenv("THVM_ATP_TWEE_TIEBREAK") ? 1 : 0;
  if (twee_tb && s->cp_uid_a != NULL && s->cp_uid_b != NULL) {
    u32 ai = s->cp_uid_a[na->row], bi = s->cp_uid_b[na->row];
    u32 aj = s->cp_uid_a[nb->row], bj = s->cp_uid_b[nb->row];
    u32 mxi = ai > bi ? ai : bi, mni = ai < bi ? ai : bi;
    u32 mxj = aj > bj ? aj : bj, mnj = aj < bj ? aj : bj;
    if (mxi != mxj) return mxi < mxj;
    if (mni != mnj) return mni < mnj;
  }
  return na->seq < nb->seq;
}

// Swap the CPs at heap positions i, j.  Only the position mirror moves
// (cp_heap: pos -> packed node, cp_pos: row -> pos); the ~14 parallel
// payload arrays stay stationary at their rows -- the historical
// whole-payload swap sifted every array on every hop, ~14 scattered
// stores per swap.  Whole 16-byte nodes swap, so the key caches ride
// along with their row -- the refresh invariant is preserved by
// construction.  A subsumption-index record borrowing cp_packed bytes
// stays valid a fortiori (the pointer never moves between slots).
static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr);

static void atp_cp_swap(AtpState *s, u32 i, u32 j) {
  AtpCpHeapNode ni = s->cp_heap[i], nj = s->cp_heap[j];
  s->cp_heap[i] = nj;
  s->cp_heap[j] = ni;
  s->cp_pos[ni.row] = j;
  s->cp_pos[nj.row] = i;
}

// Move payload row `src` into (vacated) row `dst`: the pop-backfill /
// sweep-densify companion of the stationary-row discipline.  Copies
// every per-CP array the historical backfill copied -- cp_deprio is
// deliberately NOT moved (the historical backfill never moved it
// either: it is a position-stamped array, see thvm.h).  The caller
// rewires cp_heap/cp_pos for the moved row.
static inline void atp_cp_row_move(AtpState *s, u32 dst, u32 src) {
  s->cp_packed[dst] = s->cp_packed[src];
  s->cp_packed[src] = NULL;           // vacated row: leave it empty
  s->cp_trace[dst]  = s->cp_trace[src];
  s->cp_kind[dst]   = s->cp_kind[src];
  s->cp_pri[dst]    = s->cp_pri[src];
  s->cp_seq[dst]    = s->cp_seq[src];
  if (s->cp_depth != NULL) s->cp_depth[dst] = s->cp_depth[src];
  if (s->cp_gd != NULL) s->cp_gd[dst] = s->cp_gd[src];
  s->cp_goal[dst]   = s->cp_goal[src];
  s->cp_sine[dst]   = s->cp_sine[src];
  s->cp_pri2[dst]   = s->cp_pri2[src];
  s->cp_last_norm_r_revision[dst] = s->cp_last_norm_r_revision[src];
  if (s->cp_ultimate != NULL) s->cp_ultimate[dst] = s->cp_ultimate[src];
  if (s->cp_uid_a != NULL) {
    s->cp_uid_a[dst] = s->cp_uid_a[src];
    s->cp_uid_b[dst] = s->cp_uid_b[src];
  }
  atp_cp_implicit_move(s, dst, src);
#ifdef THVM_ATPFT_CPQ
  // Move the (still-owned) FT entry alongside; zeroes the vacated src
  // so a later destroy / clear does not double-free the spans.
  atp_cp_ft_move(s, dst, src);
#endif
}

// After a position-order sweep compaction (cp_heap[0..w) holds the
// survivor rows in the sweep's visit order; s->n_cps is still the
// pre-sweep count), densify the payload rows to [0, w): every survivor
// row >= w moves into a freed row < w and the mirror rewires.  Which
// freed row a survivor lands in is NOT observable -- every consumer
// reads payload through cp_heap/cp_pos, positions are untouched here,
// and cp_deprio (the one position-raw array) never moves anyway.
// Rebuilds cp_pos for the surviving positions and commits n_cps = w.
static void atp_cp_rows_densify(AtpState *s, u32 w) {
  u32 old_n = s->n_cps;
  if (w != old_n) {
    u8 *live = (u8 *)calloc((size_t)(old_n ? old_n : 1u), 1u);
    if (live == NULL) {
      fprintf(stderr, "atp_cp_rows_densify: calloc %u failed\n", old_n);
      exit(1);
    }
    for (u32 q = 0; q < w; q++) live[s->cp_heap[q].row] = 1u;
    u32 f = 0;   // next freed row < w (ascending; enough exist: |live| == w)
    for (u32 q = 0; q < w; q++) {
      u32 r = s->cp_heap[q].row;
      if (r < w) continue;
      while (live[f]) f++;
      atp_cp_row_move(s, /*dst=*/f, /*src=*/r);
      live[f] = 1u;
      // Row move: the keys were copied verbatim to row f, so the
      // node's pri/seq/aux caches stay exact -- rewire only .row.
      s->cp_heap[q].row = f;
      f++;
    }
    free(live);
  }
  for (u32 q = 0; q < w; q++) s->cp_pos[s->cp_heap[q].row] = q;
  s->n_cps = w;
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

// ===== Waldmeister 2-D KD-heap port (opt-in THVM_ATP_KDHEAP) =============
// Faithful port of KDHeap.c (k=2) as thvm's CP-queue discipline, reproducing
// itl(mi) selection exactly: w1 = cp_pri (WMQ2 weight), w2 = cp_seq (age).
// KD-heap nodes are 1-indexed (node n <-> array slot n-1); every swap routes
// through atp_cp_swap so all parallel CP arrays stay consistent.  Ref:
// ~/src/wolfram/waldmeister/sources/BASIC/KDHeap.c (level/find_min/normalize_*).
static int atp_kdheap_on(void) {
  static int v = -1;
  if (v < 0) v = getenv("THVM_ATP_KDHEAP") ? 1 : 0;
  return v;
}
// Diagnostic: the CPdimension d of the most recent KD-heap selection
// (0 = weight root, 1 = FIFO/age jump); surfaced in the CPSEL trace.
static u32 g_kdh_last_d = 99u;
// EL_GREATER(d): d==0 -> (w1,then w2); d==1 -> w2 alone (KDHeapElements.h:63).
// na/nb are 1-indexed heap NODES; both keys come from the packed node
// caches (exact copies of cp_pri[row] / cp_seq[row], see the
// AtpCpHeapNode refresh invariant) -- one cache line per compare side.
static int atp_kdh_gt(const AtpState *s, unsigned d, u32 na, u32 nb) {
  const AtpCpHeapNode *a = &s->cp_heap[na - 1u], *b = &s->cp_heap[nb - 1u];
  if (d == 0u) {
    if (a->pri != b->pri) return a->pri > b->pri;
    return a->seq > b->seq;
  }
  return a->seq > b->seq;
}
static unsigned atp_kdh_level(u32 n) {           // floor(log2(n))
  unsigned l = 1u;
  while (n >= (1u << l)) l++;
  return l - 1u;
}
static unsigned atp_kdh_dmodk(unsigned d) { return (d >= 2u) ? d - 2u : d; }
// Min in dimension d among the top `levels`+1 levels of the subheap at `index`.
static void atp_kdh_find_min(const AtpState *s, u32 index, unsigned d,
                             unsigned levels, u32 *min, unsigned *min_level) {
  u32 n_nodes = s->n_cps;
  *min = index; *min_level = 0u;
  for (unsigned ld = 1u; ld <= levels; ld++) {
    for (u32 delta = 0u; delta < (1u << ld); delta++) {
      u32 j = (index << ld) + delta;
      if (j > n_nodes) return;
      if (atp_kdh_gt(s, d, *min, j)) { *min = j; *min_level = ld; }
    }
  }
}
static void atp_kdh_normalize_up(AtpState *s, u32 index, unsigned level_diff,
                                 unsigned d) {
  while (level_diff > 0u) {
    d = atp_kdh_dmodk(d);
    u32 anc = index >> level_diff;
    if (atp_kdh_gt(s, d, anc, index)) atp_cp_swap(s, anc - 1u, index - 1u);
    level_diff--; d++;
  }
}
static void atp_kdh_normalize_down(AtpState *s, u32 index, unsigned level_index) {
  u32 min; unsigned d, min_level;
  d = (level_index < 2u) ? level_index : (level_index % 2u);
  atp_kdh_find_min(s, index, d, 2u, &min, &min_level);
  while (min != index) {
    atp_cp_swap(s, min - 1u, index - 1u);
    atp_kdh_normalize_up(s, min, min_level - 1u, d + 1u);
    index = min;
    d = atp_kdh_dmodk(d + min_level);
    atp_kdh_find_min(s, index, d, 2u, &min, &min_level);
  }
}
// KD-heap insert: the fresh node is slot `slot` (0-indexed) == node slot+1.
static void atp_kdh_insert(AtpState *s, u32 slot) {
  u32 node = slot + 1u;
  atp_kdh_normalize_up(s, node, atp_kdh_level(node), 0u);
}
// KD-heap delete of slot `slot` (0-indexed): the caller has already
// backfilled slot with the ex-last element and decremented n_cps.  Repair
// via normalize_up then normalize_down (deleteElement, KDHeap.c:512-515).
static void atp_kdh_delete_repair(AtpState *s, u32 slot) {
  u32 node = slot + 1u;
  unsigned lvl = atp_kdh_level(node);
  atp_kdh_normalize_up(s, node, lvl, 0u);
  atp_kdh_normalize_down(s, node, lvl);
}
// Rebuild the KD-heap in place after a bulk mutation (interreduce re-weights
// every queued CP).  Sequential normalize_up over nodes 1..n_cps == building
// the heap by repeated insertion; replaces the binary Floyd build-heap.
static void atp_kdh_build(AtpState *s) {
  for (u32 node = 1u; node <= s->n_cps; node++) {
    atp_kdh_normalize_up(s, node, atp_kdh_level(node), 0u);
  }
}

// --- Raw-descriptor lane bodies (see the block comment at the top
// helpers).  atp_rdesc_intern: dedupe one (lo, ro) face-pair into
// s->rdesc_faces, returning its u32 ref.  Open addressing over pointer
// pairs; the table only ever grows (face pairs are never retired --
// their spans are immortal within the state).
static void atp_rdesc_hash_grow(AtpState *s) {
  u32 ncap = s->rdesc_hash_cap ? s->rdesc_hash_cap * 2u : 4096u;
  u32 *nh = (u32 *)malloc((size_t)ncap * sizeof(u32));
  if (nh == NULL) thvm_fatal("atp_rdesc_hash_grow: OOM");
  for (u32 i = 0; i < ncap; i++) nh[i] = ATP_RDESC_NONE;
  for (u32 r = 0; r < s->rdesc_n; r++) {
    u64 h = ((u64)(uintptr_t)s->rdesc_faces[r].lo
             ^ ((u64)(uintptr_t)s->rdesc_faces[r].ro << 17))
            * 0x9e3779b97f4a7c15ull;
    u32 j = (u32)(h >> 32) & (ncap - 1u);
    while (nh[j] != ATP_RDESC_NONE) j = (j + 1u) & (ncap - 1u);
    nh[j] = r;
  }
  free(s->rdesc_hash);
  s->rdesc_hash     = nh;
  s->rdesc_hash_cap = ncap;
}

static u32 atp_rdesc_intern(AtpState *s, const struct AtpFtCell *lo,
                            const struct AtpFtCell *ro) {
  if (s->rdesc_hash == NULL ||
      s->rdesc_n * 4u >= s->rdesc_hash_cap * 3u) {
    atp_rdesc_hash_grow(s);
  }
  u64 h = ((u64)(uintptr_t)lo ^ ((u64)(uintptr_t)ro << 17))
          * 0x9e3779b97f4a7c15ull;
  u32 j = (u32)(h >> 32) & (s->rdesc_hash_cap - 1u);
  while (s->rdesc_hash[j] != ATP_RDESC_NONE) {
    u32 r = s->rdesc_hash[j];
    if (s->rdesc_faces[r].lo == lo && s->rdesc_faces[r].ro == ro) return r;
    j = (j + 1u) & (s->rdesc_hash_cap - 1u);
  }
  if (s->rdesc_n == s->rdesc_cap) {
    u32 ncap = s->rdesc_cap ? s->rdesc_cap * 2u : 1024u;
    AtpRdescFace *nf = (AtpRdescFace *)realloc(
        s->rdesc_faces, (size_t)ncap * sizeof(AtpRdescFace));
    if (nf == NULL) thvm_fatal("atp_rdesc_intern: OOM");
    s->rdesc_faces = nf;
    s->rdesc_cap   = ncap;
  }
  u32 r = s->rdesc_n++;
  s->rdesc_faces[r].lo = lo;
  s->rdesc_faces[r].ro = ro;
  s->rdesc_hash[j] = r;
  return r;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Re-derive a raw-desc slot's packed byte string from its parents:
// the exact push-time pipeline -- rename the inner face by
// CP_RENAME_OFFSET (== sw_rename_ft's clone content), re-address the
// overlap subterm by preorder index (spans are ->next preorder
// chains), ft_unify, then vcp_ft_emit_fused into g_acp_scratch.  Every
// step is a pure function of the frozen face spans, so the bytes are
// identical to the push-time emit (certified by use_raw_desc_cert).
// Returns a malloc'd copy of the bytes; clears the transient fused
// weigh / emit state it armed.  thvm_fatal on any failure: the push
// side only stored a descriptor when this exact pipeline SUCCEEDED on
// these exact inputs, so a failure here is a broken invariant.
static u8 *atp_rdesc_materialize(AtpState *s, const AtpCpImplicit *d,
                                 u32 *len_out, u32 *nodes_out,
                                 u32 *lhs_nodes_out) {
  if (d->out_face >= s->rdesc_n || d->in_face >= s->rdesc_n) {
    thvm_fatal("atp_rdesc_materialize: face ref out of range");
  }
  const AtpRdescFace *fo = &s->rdesc_faces[d->out_face];
  const AtpRdescFace *fi = &s->rdesc_faces[d->in_face];
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *lin_ft = ft_rename_span(a, fi->lo, CP_RENAME_OFFSET, 0);
  AtpFtCell *rin_ft = ft_rename_span(a, fi->ro, CP_RENAME_OFFSET, 0);
  const AtpFtCell *sub = fo->lo;
  for (u32 k = 0; k < d->sub_pre; k++) {
    if (sub == NULL) thvm_fatal("atp_rdesc_materialize: sub_pre walked "
                                "off the outer face span");
    sub = sub->next;
  }
  static AtpFtSubst rsubst;
  ft_subst_reset(&rsubst);
  if (!ft_unify(sub, lin_ft, &rsubst)) {
    fprintf(stderr,
            "rdesc materialize: ft_unify FAILED out_face=%u in_face=%u "
            "sub_pre=%u weight=%u\n",
            d->out_face, d->in_face, d->sub_pre, d->weight);
    thvm_fatal("atp_rdesc_materialize: re-unification failed");
  }
  if (!vcp_ft_emit_fused(s->kbo, fo->lo, sub, rin_ft, fo->ro, &rsubst)) {
    fprintf(stderr,
            "rdesc materialize: emit ANOMALY out_face=%u in_face=%u "
            "sub_pre=%u weight=%u\n",
            d->out_face, d->in_face, d->sub_pre, d->weight);
    thvm_fatal("atp_rdesc_materialize: re-emit anomaly");
  }
  u32 flen = g_vcp_emit.len;
  u8 *bytes = (u8 *)malloc(flen ? flen : 1u);
  if (bytes == NULL) thvm_fatal("atp_rdesc_materialize: OOM");
  memcpy(bytes, g_acp_scratch, flen);
  *len_out       = flen;
  *nodes_out     = g_vcp_emit.nodes;
  *lhs_nodes_out = g_vcp_emit.lhs_nodes;
  // The emit armed the consume-once fused weigh state; nothing at pop
  // consumes it, so disarm both (a stale valid=1 would feed this CP's
  // balances into the NEXT push's weigh compare).
  g_acp_fuse.valid = 0u;
  g_vcp_emit.armed = 0u;
  ft_free_span(a, lin_ft, lin_ft->end);
  ft_free_span(a, rin_ft, rin_ft->end);
  s->n_rawdesc_mat++;
  return bytes;
}
#else
static u8 *atp_rdesc_materialize(AtpState *s, const AtpCpImplicit *d,
                                 u32 *len_out, u32 *nodes_out,
                                 u32 *lhs_nodes_out) {
  (void)s; (void)d; (void)len_out; (void)nodes_out; (void)lhs_nodes_out;
  thvm_fatal("atp_rdesc_materialize: raw-desc lane requires the "
             "THVM_ATPFT_NORM build (descriptors can never be pushed "
             "without it)");
  return NULL;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Cert-mode verify of one eager slot that carries a descriptor:
// re-derive and memcmp against the stored bytes; thvm_fatal with a
// full dump on any mismatch.  Shared by the pop-time check and the
// whole-queue sweep (thvm_atp_rawdesc_cert_sweep).
static void atp_rdesc_cert_verify(AtpState *s, u32 v) {
  const AtpCpImplicit *d = &s->cp_implicit[v];
  u32 len = 0u, nodes = 0u, lhs_nodes = 0u;
  u8 *bytes = atp_rdesc_materialize(s, d, &len, &nodes, &lhs_nodes);
  u32 elen = d->weight;   // cert mode: push-time byte length
  if (len != elen || memcmp(bytes, s->cp_packed[v], len) != 0) {
    u32 firstdiff = 0u;
    u32 lim = len < elen ? len : elen;
    while (firstdiff < lim &&
           bytes[firstdiff] == s->cp_packed[v][firstdiff]) {
      firstdiff++;
    }
    fprintf(stderr,
            "RAW_DESC CERT MISMATCH seq=%u len=%u/%u firstdiff=%u "
            "out_face=%u(lo=%p ro=%p) in_face=%u(lo=%p ro=%p) "
            "sub_pre=%u pri=%u\n",
            s->cp_seq[v], len, elen, firstdiff,
            d->out_face, (const void *)s->rdesc_faces[d->out_face].lo,
            (const void *)s->rdesc_faces[d->out_face].ro,
            d->in_face, (const void *)s->rdesc_faces[d->in_face].lo,
            (const void *)s->rdesc_faces[d->in_face].ro,
            d->sub_pre, d->priority);
    for (u32 k = 0; k < lim && k < 512u; k++) {
      if (k % 32u == 0u) fprintf(stderr, "\n  eager/desc[%4u]:", k);
      fprintf(stderr, " %02x/%02x", s->cp_packed[v][k], bytes[k]);
    }
    fputc('\n', stderr);
    thvm_fatal("RAW_DESC certifier: materialized pair differs from "
               "the eager packed bytes");
  }
  free(bytes);
}

// Whole-queue certifier sweep (cert mode only): verify EVERY live
// queued slot that carries a descriptor, not just the popped ones --
// pop coverage is thin exactly because the raw class floods the queue
// rather than the selections.  Returns the number of slots verified;
// fatal on the first mismatch.  Payload rows are dense in [0, n_cps).
fn u64 thvm_atp_rawdesc_cert_sweep(AtpState *s) {
  if (s == NULL || s->cp_implicit == NULL || s->use_raw_desc_cert == 0u) {
    return 0ull;
  }
  u64 n = 0ull;
  for (u32 v = 0; v < s->n_cps; v++) {
    if (atp_cp_slot_implicit(s, v)) continue;
    if (s->cp_implicit[v].out_face == ATP_RDESC_NONE) continue;
    if (s->cp_packed[v] == NULL) continue;
    atp_rdesc_cert_verify(s, v);
    n++;
  }
  return n;
}

// Pop-time slot fix for the raw-desc lane, called on the selected
// (non-orphan) slot BEFORE the unpack:
// - desc mode: materialize the packed bytes into cp_packed[v] and
//   clear the tag bit -- the slot becomes a plain eager slot and every
//   downstream consumer (unpack, mixmost byte->FT pop stash, the
//   free-on-pop) runs the eager code path bit-for-bit.
// - cert mode: the slot IS eager (bytes stored at push); re-derive
//   from the descriptor and memcmp -- thvm_fatal with a full dump on
//   any mismatch.  desc.weight carries the push-time byte length, so
//   the compare is exact-length, never an over-read.
// No-op when the lane is off (cp_implicit NULL or no descriptor).
static void atp_cp_rdesc_pop_fix(AtpState *s, u32 v) {
  if (s->cp_implicit == NULL) return;
  if (s->use_raw_desc != 0u && atp_cp_slot_implicit(s, v)) {
    u32 len = 0u, nodes = 0u, lhs_nodes = 0u;
    u8 *bytes = atp_rdesc_materialize(s, &s->cp_implicit[v],
                                      &len, &nodes, &lhs_nodes);
    s->cp_packed[v] = bytes;
    atp_cp_implicit_clear(s, v);
  } else if (s->use_raw_desc_cert != 0u && !atp_cp_slot_implicit(s, v)
             && s->cp_implicit[v].out_face != ATP_RDESC_NONE
             && s->cp_packed[v] != NULL) {
    atp_rdesc_cert_verify(s, v);
  }
}

// Insert an already-packed CP byte string onto the heap.  Takes
// ownership of `packed` (the queue frees it on pop/drop).  `lhs`/`rhs`
// are the live Terms (for the FV index + goal weight); `cp_nodes` is
// the precomputed node count.  `is_ultimate`=1 forces the CP to the
// heap front (Waldmeister Act_ultimate) -- effective only when
// s->use_initial_ultimate is set; otherwise the bit is recorded but
// atp_cp_before ignores it.  Shared by atp_cp_heap_push (fresh CP)
// and the auto-MaxWeight stash drain (re-admitting a deferred CP).
// `lhs_nodes` is the LHS-only node count from acp_pack_ex (0xffffffff
// = unknown -> the weight modes re-walk the sides as before).
static void atp_cp_heap_insert_packed(AtpState *s, u8 *packed, u32 cp_nodes,
                                      Term lhs, Term rhs, u32 trace,
                                      u8 is_ultimate, u32 lhs_nodes,
                                      u8 kind) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  // Fresh CP appends at row i == heap position i (identical placement
  // to the historical layout; the sift below then moves only the
  // position mirror).  The packed node is stamped by atp_cp_heap_set
  // just before the sift, AFTER every key (pri / seq / ultimate) is
  // written to the row.
  s->cp_pos[i]  = i;
  // Eager packed slot: make sure no stale deferred-CP tag aliases the
  // packed bytes as a descriptor (every drop/move site clears its bit,
  // so this is a backstop that keeps the invariant local).
  atp_cp_implicit_clear(s, i);
  s->cp_packed[i]= packed;
  s->cp_trace[i] = trace;
  s->cp_kind[i]  = kind;
  // Fresh CP: no IR-side normalize witness yet -- force the IR sweep to
  // normalize this CP at least once before the cookie can short-circuit.
  s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  s->cp_deprio[i] = g_cp_push_deprio;
  u64 _dkb_t0 = g_atp_phase_detail ? atp_now_us() : 0;
  s->cp_pri[i]   = s->cp_deprio[i]
                     ? ATP_CP_PRI_NEVER
                     : atp_cp_priority_sized_ex(s, lhs, rhs, cp_nodes,
                                                lhs_nodes);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // ifeq guard + tgt goal-targeted disciplines, FT-routed-push arm:
  // the treated route's Term args are zero sentinels, so the
  // in-priority Term counts above saw 0 guards / 0 symbols -- count on
  // the treatment's NF spans instead (the exact content this slot
  // stores).  tgt first, then ifeq penalty, then ifeq guard-DECIDING
  // boost, matching the Term-route order in atp_cp_priority_sized_ex.
  // The raw >= 50 emit route (lazy mode only) has no live spans here
  // and stays unscaled: its giant raw weight already buries it, and
  // the mechanisms are selection bias, not gates.
  if (s->cp_pri[i] < ATP_CP_PRI_NEVER &&
      g_vcp_treat.armed && g_vcp_treat.fl != NULL &&
      g_vcp_treat.fr != NULL &&
      (lhs == 0 || term_tag(lhs) == TAG_NUM ||
       rhs == 0 || term_tag(rhs) == TAG_NUM)) {
    if (atp_tgt_armed(s)) {
      s->cp_pri[i] = atp_tgt_weight_scale_ft(s, g_vcp_treat.fl,
                                             g_vcp_treat.fr, s->cp_pri[i]);
    }
    if (s->use_ifeq_guard) {
      s->cp_pri[i] = atp_ifeq_weight_scale_ft(s, g_vcp_treat.fl,
                                              g_vcp_treat.fr, s->cp_pri[i]);
    }
    if (s->use_ifeq_decide) {
      s->cp_pri[i] = atp_ifeq_decide_scale_ft(s, g_vcp_treat.fl,
                                              g_vcp_treat.fr, s->cp_pri[i]);
    }
  }
#endif
  // spb=goal / nwc derivation-level bias (route-independent: reads only
  // the formation-time bit, never the term content).  Before the FT
  // mirror adopt below so both queues see the same priority.
  s->cp_pri[i] = atp_spb_scale(s->cp_pri[i], g_cp_gd);
  s->cp_gd[i] = g_cp_gd;
  s->cp_depth[i] = g_cp_formation_depth;   // Twee getDepth (see g_cp_formation_depth)
  if (g_atp_phase_detail) g_atp_cpg_us_kbo += atp_now_us() - _dkb_t0;
  if (s->cp_ultimate != NULL) {
    s->cp_ultimate[i] = is_ultimate;
    if (is_ultimate) s->n_cps_ultimate++;
  }
  s->cp_goal[i]  = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                     ? atp_goal_weight(s, lhs, rhs) : 0u;
  s->cp_sine[i]  = atp_cp_sine_level(s, lhs, rhs);
#ifdef THVM_ATPFT_CPQ
  // Mirror the CP into Arena-A FT cells.  When the generation-time fuse
  // is armed, its NF spans ARE this CP (lhs/rhs are ft_to_term of them),
  // so ADOPT them and skip the ft_from_term rebuild -- ownership moves
  // to the slot, and we NULL the fuse handles so the treat cleanup
  // (which runs after this push) does not double-free.  Otherwise
  // (emit-class or non-fused push) rebuild from the Terms.
  if (g_vcp_treat.armed && g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
    atp_cp_ft_adopt(s, i, g_vcp_treat.fl, g_vcp_treat.fr,
                    s->cp_pri[i], /*origin=*/0xffffu);
    g_vcp_treat.fl = NULL;
    g_vcp_treat.fr = NULL;
  } else if (s->cp_packed[i] != NULL) {
    // Routed/fused push: the Term args are zeros or ride-along
    // sentinels by the routed-push contract -- the slot's packed
    // bytes are the authoritative content.  Decode the mirror from
    // them (byte->FT); converting the Term args fabricated degenerate
    // spans.
    const u8 *pp = s->cp_packed[i];
    AtpFt *fa = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *dfl = acp_unpack_term_ft(fa, &pp, 0);
    AtpFtCell *dfr = acp_unpack_term_ft(fa, &pp, 0);
    atp_cp_ft_adopt(s, i, dfl, dfr, s->cp_pri[i], /*origin=*/0xffffu);
  } else {
    atp_cp_ft_set(s, i, lhs, rhs, s->cp_pri[i], /*origin=*/0xffffu);
  }
#endif
  // K-D Heap secondary dimension: compute cp_pri2 with the alternate
  // weight mode (w2_mode, set via thvm_atp_set_w2).  Cheap -- ~one
  // symbol-count walk per CP -- and only filled when w2_modulo > 0.  In
  // GNN coop the GNN owns cp_pri2, so a fresh CP gets the neutral band
  // value until the next re-rank scores it (see ATP_GNN_COOP_NEUTRAL_PRI).
  _dkb_t0 = g_atp_phase_detail ? atp_now_us() : 0;
  s->cp_pri2[i]  = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                 : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, lhs, rhs, s->w2_mode)
                 :                        0u;
  if (g_atp_phase_detail) g_atp_cpg_us_kbo += atp_now_us() - _dkb_t0;
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  if (s->cp_uid_a != NULL) { s->cp_uid_a[i] = g_cp_uid_a; s->cp_uid_b[i] = g_cp_uid_b; }
  // Raw-descriptor lane: stamp the descriptor for the FT-direct raw
  // emit class (g_rdesc_store armed by atp_cp_heap_push), or the
  // "no descriptor" sentinel for every other insert -- the pop-time
  // certifier / converter keys on out_face.  desc mode (packed == NULL)
  // sets the tag bit so the slot reads as deferred; cert mode keeps the
  // eager bytes + a clear bit and records the byte length in .weight
  // for the exact-length memcmp.  Both flags off: zero-cost skip.
  if ((s->use_raw_desc | s->use_raw_desc_cert) != 0u) {
    atp_cp_implicit_arrays_ensure(s);
    if (g_rdesc_store.armed != 0u) {
      s->cp_implicit[i].out_face = g_rdesc_store.out_face;
      s->cp_implicit[i].in_face  = g_rdesc_store.in_face;
      s->cp_implicit[i].sub_pre  = g_rdesc_store.sub_pre;
      s->cp_implicit[i].weight   = (packed == NULL) ? cp_nodes
                                                    : g_rdesc_store.len;
      s->cp_implicit[i].priority = s->cp_pri[i];
      if (packed == NULL) atp_cp_implicit_set(s, i);
      s->n_cps_rawdesc++;
      g_rdesc_store.armed = 0u;
    } else {
      s->cp_implicit[i].out_face = ATP_RDESC_NONE;
    }
  }
  // Fuse-neutral insert stream (THVM_ATP_INSERT_TRACE): one line per
  // engine insert with the comparator keys.  Both formation arms funnel
  // through this function, and the env is NOT in atp_cp_push_fuse_ok's
  // list, so the streams are directly diffable across arms -- the probe
  // that pins arm-divergence bugs (the lazy+classic collapse).
  {
    static int instr = -1;
    if (instr < 0) instr = atp_env_on("THVM_ATP_INSERT_TRACE");
    if (instr) {
      fprintf(stderr, "INSTR seq=%u pri=%u nodes=%u kind=%u\n",
              seq, s->cp_pri[i], cp_nodes, kind);
    }
  }
  atp_cp_form_trace(seq, cp_nodes, lhs, rhs);
  // All comparator keys for row i are final -- stamp the packed node
  // (refresh invariant) before the sift compares against it.
  atp_cp_heap_set(s, i, i);
  s->n_cps++;
  if (atp_kdheap_on()) atp_kdh_insert(s, i);
  else atp_cp_sift_up(s, i);
#ifdef ATP_FV_INDEX
  // Only build the CP-subsumption index when the query will actually run.
  // The WM presets disable use_queue_subsume, so the index would be built
  // (102M+ nodes on OrAssociativity) and never queried -- pure waste that
  // exhausts the GC heap.  Byte-identical when skipped: nothing reads it.
  if (s->use_queue_subsume)
    atp_fv_index_insert(s->fv_index, lhs, rhs, packed, seq);
#endif
}

// The LIVE auto-MaxWeight bound: base + slope * (deepest rule-LHS
// weight).  Recomputed against the current rule set so the bound grows
// as completion deepens R -- a CP deferred now becomes admissible once
// the rules it would interact with have themselves entered R.  Returns
// 0 (== unbounded) when the auto bound is disabled.
// Recompute the deepest CURRENT rule LHS and cache the auto bound.
// Called once per step (at the drain), NOT per CP push -- rescanning R
// on every push was a measured throughput sink.  The bound tracks the
// live rule set (it can fall as deep rules are interreduced away),
// which keeps it tight; completeness does NOT rest on bound
// monotonicity but on the overflow stash + force-drain in select_cp,
// so a tighter, fluctuating bound is still complete.
static void atp_auto_maxw_recompute(AtpState *s) {
  if (s->auto_max_cp_weight_base == 0u) { s->auto_max_cp_weight_cur = 0u; return; }
  // The deepest rule LHS only changes when R changes (add/interreduce).
  // Re-scanning all rules (atp_symbol_count over s->lhs[]) every step is
  // O(n_rules * steps) -- a hidden ~10% of wall (runtime profiling).  Skip
  // the scan when r_revision is unchanged since the last recompute.
  if (s->auto_maxw_rev == s->r_revision + 1u) return;
  u32 deepest = 0u;
  for (u32 k = 0; k < s->n_rules; k++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    u32 w = atp_rule_symbol_count(s, k, 0u);
#else
    u32 w = atp_symbol_count(s->lhs[k]);
#endif
    if (w > deepest) deepest = w;
  }
  s->auto_maxw_rev = s->r_revision + 1u;
  s->max_rule_lhs_weight = deepest;
  u32 slope = s->auto_max_cp_weight_slope ? s->auto_max_cp_weight_slope : 2u;
  s->auto_max_cp_weight_cur = s->auto_max_cp_weight_base + slope * deepest;
}

static u32 atp_auto_maxw_bound(AtpState *s) {
  if (s->auto_max_cp_weight_base == 0u) return 0u;
  // Return the cached bound (refreshed once per step at the drain).
  return s->auto_max_cp_weight_cur;
}

// Park an over-bound CP on the overflow stash (auto-MaxWeight).  Takes
// ownership of `packed`.  Drained back into the heap by
// atp_auto_maxw_drain once the bound grows past its weight, so the CP
// is deferred, NEVER discarded -- completeness preserved.
static void atp_cp_stash_push(AtpState *s, u8 *packed, u32 cp_nodes,
                              u32 trace, u8 is_ultimate, u8 kind) {
  if (s->n_cp_stash >= s->cp_stash_cap) {
    u32 ncap = s->cp_stash_cap ? s->cp_stash_cap * 2u : 256u;
    u8 **np  = (u8 **)realloc(s->cp_stash_packed, ncap * sizeof(u8 *));
    u32 *nt  = (u32 *)realloc(s->cp_stash_trace,  ncap * sizeof(u32));
    u32 *nn  = (u32 *)realloc(s->cp_stash_nodes,  ncap * sizeof(u32));
    u8  *nk  = (u8  *)realloc(s->cp_stash_kind,   ncap * sizeof(u8));
    // Lazy-grow cp_stash_ultimate only when the flag is on; engine
    // byte-identical when off.
    u8  *nu  = s->use_initial_ultimate
                 ? (u8 *)realloc(s->cp_stash_ultimate, ncap * sizeof(u8))
                 : NULL;
    if (np == NULL || nt == NULL || nn == NULL || nk == NULL
        || (s->use_initial_ultimate && nu == NULL)) {
      // Allocation failure: rather than leak or lose the CP, admit it
      // directly (slow path, but sound -- never drops a proof CP).
      free(np); free(nt); free(nn); free(nk); free(nu);
      Term l = 0, r = 0;
      acp_unpack(packed, &l, &r);
      atp_cp_heap_insert_packed(s, packed, cp_nodes, l, r, trace,
                                is_ultimate, 0xffffffffu, kind);
      return;
    }
    s->cp_stash_packed = np; s->cp_stash_trace = nt; s->cp_stash_nodes = nn;
    s->cp_stash_kind = nk;
    if (s->use_initial_ultimate) s->cp_stash_ultimate = nu;
    s->cp_stash_cap = ncap;
  }
  s->cp_stash_packed[s->n_cp_stash]  = packed;
  s->cp_stash_trace[s->n_cp_stash]   = trace;
  s->cp_stash_nodes[s->n_cp_stash]   = cp_nodes;
  s->cp_stash_kind[s->n_cp_stash]    = kind;
  if (s->cp_stash_ultimate != NULL) {
    s->cp_stash_ultimate[s->n_cp_stash] = is_ultimate;
  }
  s->n_cp_stash++;
}

// Re-admit every stashed CP now within the (recomputed, possibly
// grown) bound.  Compacts the stash in place.  Called after a rule is
// added (the bound may have grown) and whenever the live queue empties
// (force=1 admits the lightest stashed CP regardless, so selection
// never starves while CPs remain).
static void atp_auto_maxw_drain(AtpState *s, u8 force) {
  if (s->auto_max_cp_weight_base == 0u || s->n_cp_stash == 0u) return;
  u32 bound = atp_auto_maxw_bound(s);
  // When forced and nothing is within bound, raise the working bound to
  // the lightest stashed CP so at least one re-enters -- a monotone
  // growth that guarantees every stashed CP is eventually selected.
  if (force && s->n_cps == 0u) {
    u32 lightest = 0xffffffffu;
    for (u32 k = 0; k < s->n_cp_stash; k++) {
      if (s->cp_stash_nodes[k] < lightest) lightest = s->cp_stash_nodes[k];
    }
    if (lightest != 0xffffffffu && lightest > bound) bound = lightest;
  }
  u32 w = 0;
  for (u32 r = 0; r < s->n_cp_stash; r++) {
    if (s->cp_stash_nodes[r] <= bound) {
      Term l = 0, rr = 0;
      acp_unpack(s->cp_stash_packed[r], &l, &rr);
      u8 ult = (s->cp_stash_ultimate != NULL) ? s->cp_stash_ultimate[r] : 0u;
      atp_cp_heap_insert_packed(s, s->cp_stash_packed[r],
                                s->cp_stash_nodes[r], l, rr,
                                s->cp_stash_trace[r], ult, 0xffffffffu,
                                s->cp_stash_kind[r]);
    } else {
      s->cp_stash_packed[w]   = s->cp_stash_packed[r];
      s->cp_stash_trace[w]    = s->cp_stash_trace[r];
      s->cp_stash_nodes[w]    = s->cp_stash_nodes[r];
      s->cp_stash_kind[w]     = s->cp_stash_kind[r];
      if (s->cp_stash_ultimate != NULL) {
        s->cp_stash_ultimate[w] = s->cp_stash_ultimate[r];
      }
      w++;
    }
  }
  s->n_cp_stash = w;
}

#ifdef THVM_ATPFT_NORM
// THVM_ATP_FT_EMIT_CHECK for the treated survivor's queue pack: the
// FT-side fused pack must byte-match the classic decode-then-pack
// (ft_to_term x2 -> thvm_normalize_vars -> acp_pack_ex) and the fused
// weigh must verdict-match the classic memo-free KBO -- the exact
// FUSE_CHECK discipline, sourced from the NF cells.
static void vcp_treat_pack_certify(const AtpState *s, const u8 *packed,
                                   u32 flen, u32 cp_nodes, u32 lhs_nodes) {
  Term cl = ft_to_term(g_vcp_treat.fl);
  Term cr = ft_to_term(g_vcp_treat.fr);
  KboCmp cf = acp_fuse_kbo(s->kbo);
  thvm_normalize_vars(&cl, &cr);
  u32 rlen = 0u, rn = 0u, rl = 0u;
  u8 *ref = acp_pack_ex(cl, cr, &rlen, &rn, &rl);
  KboCmp cc = thvm_kbo_nomemo(cl, cr, s->kbo);
  if (rlen != flen || rn != cp_nodes || rl != lhs_nodes ||
      memcmp(ref, packed, rlen) != 0 || cf != cc) {
    fprintf(stderr,
            "ATP_FT_EMIT_CHECK TREATPACK MISMATCH len=%u/%u nodes=%u/%u "
            "lhs=%u/%u bytes=%s kbo=%d/%d\n",
            flen, rlen, cp_nodes, rn, lhs_nodes, rl,
            (rlen == flen && memcmp(ref, packed, rlen) == 0) ? "EQ" : "DIFF",
            (int)cf, (int)cc);
    abort();
  }
  free(ref);
}
#endif // THVM_ATPFT_NORM

// Push one CP onto the heap.  Computes its priority once (the cost
// the old select_cp paid n times per step) and sifts up.  O(log n).
// `is_ultimate`=1 propagates the Waldmeister Act_ultimate front-rank
// to the CP slot (effective only when s->use_initial_ultimate is on).
// `raw_untreated`=1 marks the WM KPBehandelt >=50 raw class (lazy push,
// no generation-time treatment): it bypasses the auto-MaxWeight
// deferral stash -- WM has no such lane, and the stash has no FIFO
// dimension, so parking the raw class there would swallow it where WM
// buries it IN the heap at raw weight (recentCPinsert, reachable by
// the periodic FIFO pick).  The lossy caps (max_cp_queue and the WM
// -mw analog max_cp_weight) still apply on the raw weight, exactly as
// WM's hard cap would.
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace,
                             u8 is_ultimate, u8 raw_untreated) {
  g_twee_dbg_ctx = raw_untreated ? 2u : 1u;
  // Hard queue-size cap (memory leash): drop before packing (saves the
  // acp_pack malloc) once the live queue is full.  Lossy; the periodic
  // FIFO/priority selection still fires over the kept CPs.
  if (s->max_cp_queue > 0u && s->n_cps >= s->max_cp_queue) {
    s->n_cps_dropped_qcap++;
    return;
  }
  // Pack the CP into a byte string outside the managed heap.  The pack
  // walk counts per-side nodes as it goes; the weigh below reuses them.
  // Fused push loop (g_cp_push_fused): lhs/rhs arrive UN-renumbered;
  // acp_pack_fused renumbers during the byte emit and arms the flat
  // weigh state (consumed by atp_weigh_compare, cleared on every exit
  // below).  An anomaly falls back to the classic pipeline, starting
  // with the treated renumber the fused caller skipped.
  u32  cp_nodes  = 0u;
  u32  lhs_nodes = 0u;
  u8  *packed    = NULL;
  u8   rdesc     = 0u;   // raw-desc slot: no bytes stored, descriptor instead
  if (g_cp_push_fused && g_vcp_emit.armed) {
    // FT-direct push: consume the virtual emit --
    // bytes from the scratch, counts from the stash, the fused weigh
    // state already armed by vcp_emit_fused.  The FUSE_CHECK certifier
    // below has no Terms to rebuild from here; the FT route's own
    // per-CP certifier is THVM_ATP_FT_EMIT_CHECK (sw_form_push).
    u32 flen = g_vcp_emit.len;
    if (g_rdesc_push.armed != 0u && s->use_raw_desc != 0u) {
      // Raw-desc store: the emit's gates/counts/weigh state are
      // consumed exactly as the eager arm consumes them; only the
      // byte string is NOT copied out -- the descriptor re-derives it
      // at pop.  (Certifier mode takes the eager arm below and stamps
      // the descriptor alongside the bytes.)
      rdesc = 1u;
    } else {
      packed = (u8 *)malloc(flen ? flen : 1u);
      if (packed == NULL) thvm_fatal("atp_cp_heap_push: OOM");
      memcpy(packed, g_acp_scratch, flen);
    }
    if (g_rdesc_push.armed != 0u &&
        (s->use_raw_desc | s->use_raw_desc_cert) != 0u) {
      g_rdesc_store       = g_rdesc_push;
      g_rdesc_store.len   = flen;
      g_rdesc_store.armed = 1u;
    }
    g_rdesc_push.armed = 0u;   // consume-once, mirroring g_vcp_emit
    cp_nodes  = g_vcp_emit.nodes;
    lhs_nodes = g_vcp_emit.lhs_nodes;
    g_acp_fuse_n_ok++;
#ifdef THVM_ATPFT_NORM
  } else if (g_cp_push_fused && g_vcp_treat.armed) {
    // Treated FT-direct push (phase 2b): pack the
    // survivor's NF cells straight to bytes + the armed fused weigh
    // (ftp_pack_fused).  An anomaly (marker syms etc.) decodes the
    // cells and rejoins the classic fused pipeline below -- exactly
    // the pair the classic route would have packed.
    static int treat_check = -1;
    if (treat_check < 0) treat_check = atp_env_on("THVM_ATP_FT_EMIT_CHECK");
    // Twee-score byte-queue retirement: under the exact conditions of
    // the classic skip-pack below (CPQ mirror live, subsumption off, no
    // packed-byte consumer compiled in) plus no weight-cap/stash reader
    // of the bytes, the adopted FT spans ARE the queue payload and the
    // malloc'd byte string is write-only -- classic mode 8 never
    // allocated it (its pushes took the skip-pack), so skipping keeps
    // the fused arm's RSS at the classic arm's level (the 3M-CP-queue
    // regime runs against the memory leash).  Confined to mode 8: the
    // long-certified default arms keep their byte behavior.
    u8 twee_skip_buf = 0u;
#ifdef THVM_ATPFT_CPQ
#if !defined(THVM_ATP_COLLAPSE_DEFER) && !defined(THVM_ATP_TIE_RHS)
    twee_skip_buf = (s->cp_weight_mode == ATP_CP_WEIGHT_TWEE &&
                     !s->use_queue_subsume &&
                     s->auto_max_cp_weight_base == 0u &&
                     s->max_cp_weight == 0u && !treat_check) ? 1u : 0u;
#endif
#endif
    u32 flen = 0u;
    packed = ftp_pack_fused(s->kbo, g_vcp_treat.fl, g_vcp_treat.fr,
                            &flen, &cp_nodes, &lhs_nodes,
                            (u8)!twee_skip_buf);
    if (packed != NULL) {
      g_acp_fuse_n_ok++;
      if (twee_skip_buf) {
        packed = NULL;   // scratch sentinel -- not owned, nothing reads it
      } else if (treat_check) {
        vcp_treat_pack_certify(s, packed, flen, cp_nodes, lhs_nodes);
      }
    } else {
      g_vcp_treat_n_pkfb++;
      lhs = ft_to_term(g_vcp_treat.fl);
      rhs = ft_to_term(g_vcp_treat.fr);
      packed = acp_pack_fused(s->kbo, lhs, rhs, &flen, &cp_nodes,
                              &lhs_nodes);
      if (packed == NULL) {
        g_acp_fuse_n_fb++;
        thvm_normalize_vars(&lhs, &rhs);
      } else {
        g_acp_fuse_n_ok++;
      }
    }
#endif
  } else if (g_cp_push_fused) {
    u32 flen = 0u;
    packed = acp_pack_fused(s->kbo, lhs, rhs, &flen, &cp_nodes, &lhs_nodes);
    if (packed == NULL) {
      g_acp_fuse_n_fb++;
      thvm_normalize_vars(&lhs, &rhs);
    } else {
      g_acp_fuse_n_ok++;
      // THVM_ATP_FUSE_CHECK=1: per-CP differential certifier -- the
      // classic normalize+pack must byte-match the fused emit and the
      // classic memo-free KBO must verdict-match the fused weigh.
      static int fuse_check = -1;
      if (fuse_check < 0) fuse_check = atp_env_on("THVM_ATP_FUSE_CHECK");
      if (fuse_check) {
        Term cl = lhs, cr = rhs;
        thvm_normalize_vars(&cl, &cr);
        u32 rlen = 0u, rn = 0u, rl = 0u;
        u8 *ref = acp_pack_ex(cl, cr, &rlen, &rn, &rl);
        KboCmp cf = acp_fuse_kbo(s->kbo);
        KboCmp cc = thvm_kbo_nomemo(cl, cr, s->kbo);
        if (rlen != flen || rn != cp_nodes || rl != lhs_nodes ||
            memcmp(ref, packed, rlen) != 0 || cf != cc) {
          fprintf(stderr,
                  "ATP_FUSE_CHECK MISMATCH len=%u/%u nodes=%u/%u "
                  "lhs_nodes=%u/%u bytes=%s kbo=%d/%d\n",
                  flen, rlen, cp_nodes, rn, lhs_nodes, rl,
                  (rlen == flen && memcmp(ref, packed, rlen) == 0)
                    ? "EQ" : "DIFF",
                  (int)cf, (int)cc);
          abort();
        }
        free(ref);
      }
    }
  }
  if (packed == NULL && !rdesc) {
    u8 skip_pack = 0u;
#ifdef THVM_ATPFT_CPQ
#if !defined(THVM_ATP_COLLAPSE_DEFER) && !defined(THVM_ATP_TIE_RHS)
    // Byte-queue retirement: when subsumption is off (the WM presets set
    // use_queue_subsume=0, so neither the FV index nor the non-FV scan
    // is built) and no packed-byte heap-compare is compiled in, NOTHING
    // reads cp_packed -- slot_read, the peeks, and the priority rebuild
    // all source the FT queue (atp_cp_ft_set stores the pair at insert).
    // Skip the malloc+pack; recover the node counts it would have
    // returned via the identical preorder tally.  cp_packed[i] stays
    // NULL on a non-implicit slot, inert here (the cp_packed==NULL <=>
    // implicit reliance also sits behind use_queue_subsume).
    if (!s->use_queue_subsume) {
      lhs_nodes = acp_count_term(lhs);
      cp_nodes  = lhs_nodes + acp_count_term(rhs);
      skip_pack = 1u;
    }
#endif
#endif
    if (!skip_pack)
      packed = acp_pack_ex(lhs, rhs, NULL, &cp_nodes, &lhs_nodes);
  }
  // Waldmeister NEVER weight-caps an ultimate CP -- an initial axiom is
  // the spec (Act_ultimate, w1=INT32_MIN) and must always enter the
  // queue.  Deferring a heavy axiom to the auto-MaxWeight stash (or
  // dropping it under the hard cap) delays/loses it and, since the
  // intake canonicalize only tags CPs already in the queue, leaves the
  // heavy axiom un-ultimate -- it then pops hundreds of picks late at
  // its computed weight (the ShefferAxioms__Commutativity firstdiv-3
  // divergence: a 3rd axiom of >20 nodes stashed past the base-20 bound).
  if (!is_ultimate) {
    // Waldmeister MaxWeight (hard cap): drop an over-weight critical pair
    // before it enters the queue.  0 = unbounded.  This is the LOSSY
    // bound (flag-gated by the caller setting max_cp_weight); the default
    // engine leaves it 0.
    if (s->max_cp_weight > 0u && cp_nodes > s->max_cp_weight) {
      free(packed);
      g_acp_fuse.valid = 0u;
      g_rdesc_store.armed = 0u;
      return;
    }
    // Twee --max-term-size (per-side cap): drop the CP when EITHER side
    // exceeds the bound, i.e. max(|lhs|,|rhs|) > cap.  Twee's dequeue
    // applies cfg_accept_term (checkSize n t = KBO.size t <= n) to BOTH
    // overlap sides (Twee.hs:423-424) -- with default weights the raw node
    // count.  Unlike the sum cap above this separates a one-huge-side
    // monster (the assoc-lifted H7xAC rules) from the both-sides-moderate
    // CPs the proof needs; the sum cap floods on LAT138-1 because no total
    // threshold cleanly splits 30+8 from 20+20.  Same lossy drop.
    if (s->max_cp_side > 0u &&
        (lhs_nodes > s->max_cp_side ||
         (cp_nodes - lhs_nodes) > s->max_cp_side)) {
      free(packed);
      g_acp_fuse.valid = 0u;
      g_rdesc_store.armed = 0u;
      return;
    }
    // Auto-MaxWeight (completeness-preserving): defer an over-bound CP to
    // the overflow stash rather than dropping it.  Disabled when base==0.
    // The WM raw>=50 class never defers (see the header comment) -- but that
    // class is exactly what explodes the CP queue on WolframAxioms, and the
    // stash is completeness-preserving (re-admitted as the bound grows), so
    // opt-in THVM_ATP_STASH_RAW lets it cover the raw class too: the queue
    // bounds WITHOUT dropping, unlike the lossy hard cap.  Off by default ->
    // Sheffer/faithful paths keep the raw-never-defer behavior.
    static int stash_raw = -1;
    if (stash_raw < 0) stash_raw = getenv("THVM_ATP_STASH_RAW") ? 1 : 0;
    if (s->auto_max_cp_weight_base > 0u && (!raw_untreated || stash_raw)) {
      u32 bound = atp_auto_maxw_bound(s);
      if (bound > 0u && cp_nodes > bound) {
        if (rdesc) {
          // The overflow stash stores packed bytes only -- reify the
          // deferred emit from the still-live scratch (identical bytes
          // to the eager arm's copy) so the stash verdict matches.
          packed = (u8 *)malloc(g_rdesc_store.len ? g_rdesc_store.len : 1u);
          if (packed == NULL) thvm_fatal("atp_cp_heap_push: OOM");
          memcpy(packed, g_acp_scratch, g_rdesc_store.len);
        }
        atp_cp_stash_push(s, packed, cp_nodes, trace, is_ultimate,
                          g_cp_push_kind);
        g_acp_fuse.valid = 0u;
        g_rdesc_store.armed = 0u;
        return;
      }
    }
  }
  if (g_atp_phase_detail) {
    if (raw_untreated) {
      g_acp_pack_nodes_raw += cp_nodes;
      g_acp_pack_n_raw++;
    } else {
      g_acp_pack_nodes_treated += cp_nodes;
      g_acp_pack_n_treated++;
    }
  }
  // Normalized-size CP scoring under lazy-normalize (THVM_ATP_LAZY_
  // SCORE_NF, doc block at atp_ifeq_strip_env): the raw class weighs
  // on a step-capped-NF per-side size estimate instead of its raw
  // tower size.  Score-only -- the queued bytes/spans and every
  // weight-cap verdict above keep the true raw counts.  (The TWEE
  // weight mode ignores the count base -- atp_cp_twee_fused_base --
  // so the override is inert there.)
  u32 score_nodes = cp_nodes, score_lhs = lhs_nodes;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (raw_untreated && s->use_lazy_normalize) {
    u32 nf_cap = atp_lazy_scorenf_cap();
    if (nf_cap > 0u) {
      u32 nl = 0u, nr = 0u;
      // Raw-desc slot: no malloc'd bytes, but the emit's bytes are
      // still live in the scratch -- probe those (identical content).
      if (atp_cp_lazy_scorenf_probe(s, rdesc ? g_acp_scratch : packed,
                                    lhs, rhs, nf_cap,
                                    &nl, &nr)) {
        g_scorenf_n++;
        if (nl + nr < cp_nodes) g_scorenf_shrunk++;
        score_nodes = nl + nr;
        score_lhs   = nl;
      }
    }
  }
#endif
  atp_cp_heap_insert_packed(s, packed, score_nodes, lhs, rhs, trace,
                            is_ultimate, score_lhs, g_cp_push_kind);
  g_acp_fuse.valid = 0u;   // the fused weigh state is consume-once
  g_twee_dbg_ctx = 0u;
}

// Lazily allocate the deferred-CP descriptor array + tag bitset to the
// current cp_cap (the caller has already run atp_ensure_cp_cap, which
// keeps both in lockstep with cp_packed once they exist).  First-push-
// only; under use_implicit_cp == 0 neither is ever allocated and every
// helper above stays a single-branch no-op.
static void atp_cp_implicit_arrays_ensure(AtpState *s) {
  if (s->cp_is_implicit != NULL) return;
  u32 cap = s->cp_cap;
  s->cp_implicit = (AtpCpImplicit *)malloc((size_t)cap
                                           * sizeof(AtpCpImplicit));
  s->cp_is_implicit = (u8 *)calloc((size_t)(cap + 7u) / 8u, 1u);
  if (s->cp_implicit == NULL || s->cp_is_implicit == NULL) {
    thvm_fatal("atp_cp_implicit_arrays_ensure: OOM");
  }
  for (u32 i = 0; i < cap; i++) {
    s->cp_implicit[i].parent_a_trace_id = ATP_TRACE_NONE;
    s->cp_implicit[i].parent_b_trace_id = ATP_TRACE_NONE;
    s->cp_implicit[i].overlap_position  = 0;
    s->cp_implicit[i].weight            = 0u;
    s->cp_implicit[i].priority          = 0u;
  }
}

// Deferred-CP (`implicit_pair`) push: record the critical pair as a
// 20-byte descriptor instead of an `acp_pack`'d byte string -- the WM
// passive-set discipline (KPVerwaltung.c stores implicit term pairs and
// materializes only on treatment via TPR_TP2ParIntermed).  The slot is
// TRACE-BACKED: its raw unified terms already live unconditionally in
// the TRACE_CP entry at `cp_trace[i]` (children 2/3, pushed by
// atp_trace_push_cp for every queued CP), so materialization at
// selection reads them back for the pop-time normalize that already
// runs unconditionally -- no re-unification, no extra storage.  The
// parent trace ids are cached in the descriptor for the dead-parent /
// orphan check without a trace-entry walk.
//
// All weight bookkeeping runs HERE, while the unified terms are still
// live (WM C_Classify-then-discard): cp_pri / cp_goal / cp_pri2 are
// computed exactly as atp_cp_heap_insert_packed computes them, then the
// terms are dropped (the trace keeps the raw forms alive).  Implicit
// slots are NEVER inserted into the FV subsumption index -- WM has no
// queue-vs-queue subsumption, so the implicit passive set does not
// participate (see atp_fv_index_rebuild).
//
// Returns 1 when the CP was consumed (queued as a descriptor, or
// dropped by the MaxWeight hard cap -- identical verdict to
// atp_cp_heap_push's), 0 when the caller must take the eager packed
// path instead (auto-MaxWeight over-bound CPs go to the packed overflow
// stash, which has no implicit lane).
static u8 atp_cp_implicit_push(AtpState *s, Term lhs, Term rhs,
                               u32 parent_a_trace_id, u32 parent_b_trace_id,
                               u32 cp_trace_id, u8 is_ultimate) {
  // Same structural weight acp_pack counts (one tick per preorder node)
  // -- computed without the pack walk or its malloc.
  u32 cp_lhs_nodes = atp_symbol_count(lhs);
  u32 cp_rhs_nodes = atp_symbol_count(rhs);
  u32 cp_nodes = cp_lhs_nodes + cp_rhs_nodes;
  // Hard queue-size cap (memory leash): same lossy drop as atp_cp_heap_push.
  if (s->max_cp_queue > 0u && s->n_cps >= s->max_cp_queue) {
    s->n_cps_dropped_qcap++;
    return 1u;   // consumed (dropped) -- identical verdict to the MaxWeight cap
  }
  // Waldmeister MaxWeight hard cap: same lossy drop as atp_cp_heap_push.
  if (s->max_cp_weight > 0u && cp_nodes > s->max_cp_weight) return 1u;
  // Twee --max-term-size per-side cap (see atp_cp_heap_push): drop when
  // EITHER side's node count exceeds the bound.  Same lossy consumed verdict.
  if (s->max_cp_side > 0u &&
      (cp_lhs_nodes > s->max_cp_side || cp_rhs_nodes > s->max_cp_side))
    return 1u;
  // Auto-MaxWeight: the overflow stash stores packed byte strings, so an
  // over-bound CP falls back to the eager path (which re-tests the bound
  // and stashes).  Not in the WM preset; deferred-by-construction.
  if (s->auto_max_cp_weight_base > 0u) {
    u32 bound = atp_auto_maxw_bound(s);
    if (bound > 0u && cp_nodes > bound) return 0u;
  }
  atp_ensure_cp_cap(s, s->n_cps + 1);
  atp_cp_implicit_arrays_ensure(s);
  u32 i = s->n_cps;
  // Append at row i == heap position i (see atp_cp_heap_insert_packed;
  // the packed node is stamped pre-sift, after all key writes).
  s->cp_pos[i]  = i;
  s->cp_packed[i] = NULL;          // deferred slot marker
  s->cp_trace[i]  = cp_trace_id;
  s->cp_kind[i]   = g_cp_push_kind;
  s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  s->cp_deprio[i] = g_cp_push_deprio;
  s->cp_pri[i]    = s->cp_deprio[i]
                      ? ATP_CP_PRI_NEVER
                      : atp_cp_priority_sized(s, lhs, rhs, cp_nodes);
  s->cp_pri[i]    = atp_spb_scale(s->cp_pri[i], g_cp_gd);
  s->cp_gd[i]     = g_cp_gd;
  s->cp_depth[i] = g_cp_formation_depth;   // Twee getDepth (see g_cp_formation_depth)
  if (s->cp_ultimate != NULL) {
    s->cp_ultimate[i] = is_ultimate;
    if (is_ultimate) s->n_cps_ultimate++;
  }
  s->cp_goal[i]   = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, lhs, rhs) : 0u;
  s->cp_sine[i]   = atp_cp_sine_level(s, lhs, rhs);
  s->cp_pri2[i]   = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                  : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, lhs, rhs,
                                                              s->w2_mode)
                  :                        0u;
  s->cp_seq[i]    = s->cp_seq_next++;
  if (s->cp_uid_a != NULL) { s->cp_uid_a[i] = g_cp_uid_a; s->cp_uid_b[i] = g_cp_uid_b; }
  atp_cp_form_trace(s->cp_seq[i], cp_nodes, lhs, rhs);
  // No THVM_ATPFT_CPQ mirror: the FT queue's slot-occupied invariant
  // tracks cp_packed[i], and a deferred slot is exactly the NULL case
  // (atp_cp_ft_clear / _move are NULL-idempotent).
  s->cp_implicit[i].parent_a_trace_id = parent_a_trace_id;
  s->cp_implicit[i].parent_b_trace_id = parent_b_trace_id;
  s->cp_implicit[i].overlap_position  = 0;
  s->cp_implicit[i].weight            = cp_nodes;
  s->cp_implicit[i].priority          = s->cp_pri[i];
  atp_cp_implicit_set(s, i);
  // All comparator keys for row i are final -- stamp the packed node
  // (refresh invariant) before the sift compares against it.
  atp_cp_heap_set(s, i, i);
  s->n_cps++;
  s->n_cps_implicit++;
  if (atp_kdheap_on()) atp_kdh_insert(s, i);
  else atp_cp_sift_up(s, i);
  return 1u;
}

// Waldmeister CP-queue interleaving (a port of KPVerwaltung.c's
// `CPdimension`).  Waldmeister keeps the set of unselected equations
// in a K-D heap with TWO keys -- a weight key and a FIFO insertion
// key -- and `CPdimension` returns the FIFO dimension for `thresholdCP`
// of every `moduloCP` selections, the weight dimension otherwise.
//
// WM derives (moduloCP, thresholdCP) from the `-pq interleave=f.h`
// token: `moduloCP = f + h`, `thresholdCP = f` (KPVerwaltung.c:1484-
// 1485); CPdimension() (KPVerwaltung.c:846-848) returns the FIFO
// dimension when `AnzAktivierterRE % moduloCP < thresholdCP`.
//
// TWO reference configurations exist and they DIFFER here:
// - PLAIN `wmcli` (bare `-pq`, no interleave token): thresholdCP 0,
//   FIFO never fires -- a pure smallest-weight heap.  The historical
//   wm_align_matrix baseline rows were recorded against this config
//   (and a hardcoded threshold 1 forked CombinatorAxioms SKIToBCKW at
//   selection 303, which is why the preset once pinned threshold 0).
// - `wmcli -auto` (= FindEquationalProof = Method ->
//   "WaldmeisterProcess"): Automodus Orkus/StdS carries `itl(mi)` =
//   `-pq interleave=1.50` -> thresholdCP 1, moduloCP 51.  THIS is the
//   config the Waldmeister preset targets; the default below follows
//   it.  THVM_ATP_FIFO_THRESHOLD=0 restores the plain-wmcli mode.
//
// The legacy (non-WM) path keeps the raw-selection-count end-of-window
// behaviour the weight-order unit tests pin.
#define ATP_CP_FIFO_MODULO     11u
#define ATP_CP_FIFO_THRESHOLD   1u

// WM `CPdimension()` under the faithful WM preset (use_intake_order):
// the parity reference is `wmcli -auto` (= FindEquationalProof =
// Method -> "WaldmeisterProcess"), whose Orkus/StdS strategy carries
// `itl(mi)` = `-pq interleave=1.50` (ANA/YFiles.c Schrittweiten[mi] =
// {1, 50}) -> thresholdCP = 1, moduloCP = 51 (KPVerwaltung.c:1484-1485):
// ONE oldest-CP (FIFO) selection whenever AnzAktivierterRE % 51 < 1,
// including the run-in at count 0 before the first activation (the
// pinned WolframAxioms__DoubleNegation divergence: WM sel 117 FIFO-pops
// the old rule1xrule1 CP while queued 929-weight duplicates wait; a
// weight pop takes the duplicate instead -> firstdiv 117).
// THVM_ATP_FIFO_THRESHOLD=0 restores the PLAIN-wmcli behaviour (bare
// `-pq` carries no interleave token -> thresholdCP 0, FIFO never
// fires) that the wm_align_matrix baseline rows were recorded against.
// Outside the preset thvm keeps the legacy raw-selection-count
// interleave the weight-order unit tests rely on.
static inline u8 atp_cp_fifo_dimension(const AtpState *s) {
  u32 modulo = s->fifo_modulo ? s->fifo_modulo : ATP_CP_FIFO_MODULO;
  if (s->use_intake_order) {
    static int thr = -2;
    if (thr == -2) {
      const char *e = getenv("THVM_ATP_FIFO_THRESHOLD");
      thr = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 1;
    }
    if (thr <= 0) return 0u;
    return (s->n_activated_re % modulo) < (u32)thr;
  }
  return (modulo - ATP_CP_FIFO_THRESHOLD) <= (s->cp_select_count % modulo);
}

// Select and remove one CP from the queue.  Most calls take the heap
// min (lowest (cp_pri, cp_seq) -- the weight heuristic); ATP_CP_FIFO_-
// THRESHOLD of every ATP_CP_FIFO_MODULO calls instead take the OLDEST
// queued CP (lowest cp_seq) -- Waldmeister's FIFO dimension.
// Extraction works at an arbitrary slot j: backfill from the last
// slot, then sift the backfilled element (one of sift-up / sift-down
// is a no-op).
//
// Returns 1 on success (out-params populated), 0 on empty queue.
// ENIGMA training-data recorder (defined with the feature block below);
// forward-declared here for the gated hook at the end of this function.
static void atp_cp_feat_record(AtpState *s, Term lhs, Term rhs,
                               u32 trace_id);
// Lazy orphan murder: defined with the orphan-murder setters below.
static int atp_cp_is_orphan(const AtpState *s, u32 cp_trace);
static int atp_trace_is_dead(const AtpState *s, u32 trace_id);
// WM `dokgS` test (1-step join via a single existing rule); defined
// alongside the per-CP-push subsume drop a few thousand lines down.
static u8 atp_cp_rule_subsumed(AtpState *s, Term lhs, Term rhs);
// WM -ks "s" pop-time E-subsumption test; defined next to
// atp_cp_rule_subsumed.
static u8 atp_pop_eq_subsumed(AtpState *s, Term lhs, Term rhs);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT siblings (defined next to the Term versions): the pop treatment
// decides the -ks "s" drop on the flat NFs so a subsumed pop never
// materializes heap Terms.  Returns 0/1 verdict; 2 = cannot decide
// (missing rule mirror) -- caller falls back to the Term route.
static int atp_pop_eq_subsumed_ft(AtpState *s, const AtpFtCell *lhs,
                                  const AtpFtCell *rhs);
static u8 atp_order_is_plain_kbo(const AtpState *s);
static u8 ft_sym_multiset_eq(const AtpFtCell *a, const AtpFtCell *b);
#ifdef THVM_ATP_AC
static u8 atp_cp_perm_subsumed_ft(const AtpFtCell *lhs,
                                  const AtpFtCell *rhs);
#endif
#endif
// On-demand FT-mirror population for any caller of atp_rewrite_normalize_ft
// (definition with atp_cp_trivially_joinable, far below).
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static inline void atp_ft_mirror_ensure(AtpState *s);
#endif

// LRS horizon recomputation (Riazanov & Voronkov, JSC 36, 2003).  Given
// the observed selection rate and the remaining wall budget, predict how
// many MORE CPs the saturator will pop, then take the `k`-th smallest
// cp_pri in the queue as the weight horizon: any CP with cp_pri >
// horizon is provably unreachable in budget (the saturator will pop
// `k` CPs and those `k` are the lightest, by the heap order) and is
// pruned.  O(n_queue) selection + O(n_queue) sweep; the period gate in
// the caller amortizes this across thousands of selections.
//
// Soundness: a discarded CP is not in the reach window; the proof, if it
// exists in budget, does not depend on it (same "incomplete in principle,
// complete in budget" tradeoff Vampire ships).  When the deadline has
// already passed `remaining_us` <= 0, leave the horizon untouched -- the
// next deadline check in atp_norm_deadline_fired will trip ATP_TIMEOUT.
static void atp_lrs_recompute_horizon(AtpState *s) {
  if (!s->use_lrs) return;
  if (s->wall_deadline_us == 0u) return;
  if (s->n_cps == 0u) return;
  u64 now = atp_now_us();
  if (now == 0u) return;
  if (s->lrs_start_us == 0u) {
    // first call: latch the start, run the warmup before any horizon
    s->lrs_start_us = now;
    return;
  }
  if (now >= s->wall_deadline_us) return;
  if (now <= s->lrs_start_us) return;
  if (s->cp_select_count < s->lrs_warmup_selections) return;
  u64 elapsed = now - s->lrs_start_us;
  u64 remaining = s->wall_deadline_us - now;
  // predicted_remaining = remaining * (selections_done / elapsed)
  // computed as (sel * remaining) / elapsed to keep the divide last.
  u64 predicted = ((u64)s->cp_select_count * remaining) / elapsed;
  if (predicted == 0u) predicted = 1u;
  // If the predicted-remaining reach window already covers the whole
  // queue, no CP is unreachable yet -- clear the horizon (no prune).
  if (predicted >= (u64)s->n_cps) {
    s->lrs_horizon = 0u;
    s->lrs_last_recompute_at = s->cp_select_count;
    s->n_lrs_recomputes++;
    return;
  }
  // Find the k-th smallest cp_pri.  k == predicted (1-indexed) -- the
  // saturator will pop `predicted` more CPs and those are the lightest
  // `predicted` in the queue under the heap order.  Quickselect over a
  // scratch copy of cp_pri keeps it O(n_queue) average without touching
  // the live heap.
  u32 n = s->n_cps;
  u32 *scratch = (u32 *)malloc((size_t)n * sizeof(u32));
  if (scratch == NULL) return;  // out of memory: skip the recompute (sound)
  // node.pri == cp_pri[node.row] (refresh invariant) -- the packed
  // read spares the dependent payload load per entry.
  for (u32 i = 0; i < n; i++) scratch[i] = s->cp_heap[i].pri;
  u32 k = (u32)predicted - 1u;  // 0-indexed k-th smallest
  // Hoare partition / quickselect loop.
  u32 lo = 0, hi = n - 1u;
  while (lo < hi) {
    u32 pivot = scratch[(lo + hi) >> 1];
    u32 i = lo, j = hi;
    for (;;) {
      while (scratch[i] < pivot) i++;
      while (scratch[j] > pivot) j--;
      if (i >= j) break;
      u32 t = scratch[i]; scratch[i] = scratch[j]; scratch[j] = t;
      i++; if (j == 0u) break; j--;
    }
    if (j < k) lo = j + 1u;
    else       hi = j;
  }
  u32 horizon = scratch[k];
  free(scratch);
  s->lrs_horizon = horizon;
  s->lrs_last_recompute_at = s->cp_select_count;
  s->n_lrs_recomputes++;
  // Prune the queue of CPs above the horizon.  Walk from the tail and
  // backfill from the live last slot; defer sifting until the end so the
  // prune is O(n_queue) instead of O(n_queue * log n_queue).  After
  // pruning, Floyd-build the remaining slots into a heap in one pass.
  if (horizon == UINT32_MAX) return;  // sentinel: never prune everything
  u32 i = s->n_cps;
  while (i > 0u) {
    i--;
    // i is a heap POSITION; the CP's payload row is vv.  The pri test
    // reads the packed cache (== cp_pri[vv], refresh invariant).
    u32 vv = s->cp_heap[i].row;
    if (s->cp_heap[i].pri <= horizon) continue;
    free(s->cp_packed[vv]);
    s->cp_packed[vv] = NULL;
    atp_cp_implicit_clear(s, vv);
#ifdef THVM_ATPFT_CPQ
    atp_cp_ft_clear(s, vv);
#endif
#ifdef ATP_FV_INDEX
    // No-op for deferred slots: their seq was never inserted.
    atp_fv_index_remove(s->fv_index, s->cp_seq[vv]);
#endif
    // Heap-entry backfill from the last position, then row densify
    // (move the last row into the vacated row vv).  The heap shrink
    // MUST precede the cp_pos[last-row] read: the last row may be the
    // very CP that sat at the popped last position.
    u32 last = s->n_cps - 1u;
    if (i != last) {
      // Whole-node move: the last position's key caches ride along
      // with their row (keys untouched -- invariant preserved).
      AtpCpHeapNode moved = s->cp_heap[last];
      s->cp_heap[i] = moved;
      s->cp_pos[moved.row] = i;
    }
    s->n_cps--;
    u32 lr = s->n_cps;             // last (to-densify) payload row
    if (vv != lr) {
      atp_cp_row_move(s, /*dst=*/vv, /*src=*/lr);
      u32 p = s->cp_pos[lr];
      // Row move: keys copied verbatim to row vv -- rewire only .row.
      s->cp_heap[p].row = vv;
      s->cp_pos[vv] = p;
    }
    s->n_cps_dropped_lrs++;
    // The backfilled element at slot i might itself be above horizon;
    // re-check it on the next iteration (compensate the i-- at the
    // top of the loop).  Bound by i < s->n_cps to skip the no-op
    // tail-drop case.
    if (i < s->n_cps) i++;
  }
  // Re-establish the heap order via Floyd build-heap: O(n_queue) sift-
  // downs over internal nodes, last to first.
  if (s->n_cps > 1u) {
    if (atp_kdheap_on()) atp_kdh_build(s);
    else for (u32 k = s->n_cps / 2u; k > 0u; ) {
      k--;
      atp_cp_sift_down(s, k);
    }
  }
}

// Deferred-CP (`implicit_pair`) materialization at selection -- the WM
// TPR_TP2ParIntermed analog (KPVerwaltung.c:975, the read-back before
// NF_Normalform2).  An implicit slot has no packed byte string; its raw
// unified pair lives in the TRACE_CP entry at `cp_trace[slot_idx]` as
// children 2/3 (atp_trace_push_cp), so the pop reads them straight off
// the trace -- no re-unification (WM re-unifies via U1_KPRekonstruieren
// only because it has no always-on trace), no copy, no malloc.
//
// Liveness + aliasing: the trace array is a GC root set (thvm_atp_gc_-
// collect roots every entry and Cheney forwarding preserves sharing),
// so the returned Terms are alive and current across collections; the
// per-step heap reset can never pop them (the entry predates the step's
// checkpoint).  The pop path treats the popped pair as read-only --
// atp_rewrite_normalize / _record build fresh terms and kbo_eq just
// walks -- and the record_norm_steps path already feeds these exact
// trace children into the same chain, so handing out shared trace
// terms is the established discipline, not a new one.
//
// Form: children 2/3 are the RAW overlap (pre queue-reduction), the
// same form the lazy-push flow queues; the unconditional pop-time
// normalize takes it to a normal form just the same.  The descriptor's
// cached weight/priority were computed on the push-time reduced form,
// which only ordered the heap -- ordering is consumed by the time the
// slot is popped.
//
// Returns 1 always: the push side guarantees a live TRACE_CP entry for
// every implicit slot (trace-capped CPs stay eager), so a miss here is
// a broken invariant, not a recoverable state.  Dead-parent (orphan)
// slots are still materializable -- the trace entry outlives its
// parents -- and are discarded by the orphan check in the caller.
static int atp_cp_implicit_materialize(const AtpState *s, u32 slot_idx,
                                        Term *out_lhs, Term *out_rhs) {
  u32 t = s->cp_trace[slot_idx];
  if (t == ATP_TRACE_NONE || t >= s->n_trace) {
    thvm_fatal("atp_cp_implicit_materialize: implicit slot has no live "
               "trace entry (push-side invariant broken)");
  }
  Term te = s->trace[t];
  if (term_tag(te) != TAG_CTR || term_ext(te) != TRACE_CP
      || term_ctr_n(te) < 4u) {
    thvm_fatal("atp_cp_implicit_materialize: trace entry is not a "
               "TRACE_CP (push-side invariant broken)");
  }
  if (!atp_trace_eq_load((AtpState *)s, t, out_lhs, out_rhs)) {
    *out_lhs = term_ctr_at(te, 2);
    *out_rhs = term_ctr_at(te, 3);
  }
  return 1;
}

// Slot-aware CP read shared by every surface that walks the live queue
// (selection plus the read-only diagnostics thvm_atp_cp_get /
// thvm_atp_queued_cps / thvm_atp_cp_size_stats / thvm_atp_peek_top_k):
// a packed slot unpacks fresh transient heap Terms; an implicit slot
// returns the trace-resident raw pair zero-copy.  Callers treat the
// pair as read-only either way.
static void atp_cp_slot_read(const AtpState *s, u32 i,
                             Term *lhs, Term *rhs) {
  if (atp_cp_slot_implicit(s, i)) {
    if (s->use_raw_desc != 0u) {
      // Raw-desc slot outside the pop path (peeks / dumps / sweeps):
      // materialize transiently, unpack the identical Terms an eager
      // slot would yield, release the bytes.  (The pop path converts
      // the slot in place instead -- atp_cp_rdesc_pop_fix -- so the
      // downstream free/mixmost consumers run the eager code.)
      u32 len = 0u, nodes = 0u, lhs_nodes = 0u;
      u8 *bytes = atp_rdesc_materialize((AtpState *)s, &s->cp_implicit[i],
                                        &len, &nodes, &lhs_nodes);
      acp_unpack(bytes, lhs, rhs);
      free(bytes);
    } else {
      atp_cp_implicit_materialize(s, i, lhs, rhs);
    }
  } else {
#ifdef THVM_ATPFT_CPQ
    // Byte-queue retirement: a packed slot sources its term pair from
    // the Arena-A FT image (cp_packed_ft), not the legacy byte string.
    // ft_to_term rebuilds the identical heap Terms acp_unpack would --
    // test_ft_cpq T1 pins the round-trip byte-for-byte on every slot.
    AtpFtCell *fl, *fr;
    atp_cp_ft_slot_get(s, i, &fl, &fr);
    *lhs = ft_to_term(fl);
    *rhs = ft_to_term(fr);
#else
    acp_unpack(s->cp_packed[i], lhs, rhs);
#endif
  }
}

fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL) return 0;
  // Auto-MaxWeight: if the active queue is empty but CPs are deferred
  // on the overflow stash, force-drain the lightest back in -- the
  // search continues on the deferred CPs (raising the bound monotone),
  // so no proof CP is lost.  This is what makes the bound complete.
  if (s->n_cps == 0u && s->n_cp_stash > 0u && s->auto_max_cp_weight_base > 0u) {
    atp_auto_maxw_drain(s, 1u);
  }
  if (s->n_cps == 0) return 0;
  // LRS horizon: latch the start clock on the first call; recompute the
  // horizon + prune at most every lrs_recompute_period selections.
  // With use_lrs off, both branches predict false on the first test so
  // the engine is byte-identical to the historical path.
  if (s->use_lrs && s->wall_deadline_us != 0u) {
    if (s->lrs_start_us == 0u
        || (s->cp_select_count - s->lrs_last_recompute_at)
             >= (s->lrs_recompute_period ? s->lrs_recompute_period : 128u)) {
      atp_lrs_recompute_horizon(s);
      if (s->n_cps == 0u && s->n_cp_stash > 0u
          && s->auto_max_cp_weight_base > 0u) {
        atp_auto_maxw_drain(s, 1u);
      }
      if (s->n_cps == 0u) return 0;
    }
  }

  // WM selectNonOrphan (KPVerwaltung.c:535): loop deleteMin until a
  // non-orphan CP surfaces.  An orphan (a parent rule was interreduced
  // away) is extracted and discarded FOR FREE -- the simplified
  // equation that replaced the dead rule regenerates its contribution,
  // so dropping it preserves completeness.  With use_orphan_murder off
  // the body runs exactly once (the orphan test is a cheap predictable
  // branch) and the engine is byte-identical.
  for (;;) {
  // Orphan-murder above can drain the queue mid-loop (n_cps-- after each
  // discard); the upstream guard only fires before the for(;;) starts, so
  // the inner re-entry needs its own n_cps==0 check.  Without this the
  // FIFO/random branches would compute j over an empty array and the
  // subsequent acp_unpack would dereference a freed slot.
  if (s->n_cps == 0u) return 0;
  // CPdimension: FIFO pick on the last THRESHOLD of every MODULO
  // selections, weight pick (heap root) otherwise.
  u32 j = 0;
  // WM 2-D KD-heap selection (opt-in THVM_ATP_KDHEAP): the queue IS a faithful
  // KDHeap.c port, so selection is KDH_deleteMin(d) with d = CPdimension() =
  // (n_rules % 51 < 1) ? 1 : 0 (itl(mi) 1:50).  find_min(1,d,d) locates the
  // node: d=0 -> root (weight); d=1 -> min-w2 among the top 2 levels (the
  // KD-heap's age dimension, which surfaces WM's specific old CP).
  // WM-faithful itl(mi) interleave (opt-in, THVM_ATP_WM_ITL / THVM_ATP_KDHEAP).
  // Waldmeister StdS uses itl(mi) = Fifo:Heuristik 1:50 (YFiles.c Schrittweiten
  // [mi]), so moduloCP=51, thresholdCP=1, CPdimension = (AnzAktivierterRE % 51
  // < 1) ? d1 : d0 (KPVerwaltung.c:848,1484): AGE dim when the activated-RULE
  // count is a multiple of 51, weight otherwise.  KDHEAP is the faithful 2-D
  // KD-heap port (KDH_deleteMin(d), find_min top d+1 levels); WM_ITL is the
  // legacy binary-heap scan approximation.
  static int kdh = -1;
  if (kdh < 0) kdh = atp_kdheap_on();
  static int wm_itl = -1;
  if (wm_itl < 0) wm_itl = getenv("THVM_ATP_WM_ITL") ? 1 : 0;
  if (kdh) {
    // CPdimension (KPVerwaltung.c:848): d = AnzAktivierterRE % moduloCP <
    // thresholdCP.  WM's AnzAktivierterRE is the CUMULATIVE activation
    // counter (KPV_IncAktivierterRE, monotonic ++), NOT the current-alive
    // rule count -- so gate on s->n_activated_re, not s->n_rules (which
    // interreduction decrements, drifting the FIFO cadence off WM's).
    unsigned d = (s->n_activated_re % 51u < 1u) ? 1u : 0u;
    u32 mn = 1u; unsigned ml = 0u;
    atp_kdh_find_min(s, 1u, d, d, &mn, &ml);
    j = mn - 1u;
    g_kdh_last_d = d;
    if (d == 1u) s->n_cps_w2_picks++;
  } else if (wm_itl && (s->n_rules % 51u) == 0u) {
    // WM KDH_deleteMin(d=1) searches only the TOP d+1 = 2 heap levels
    // (find_min, KDHeap.c:641 "in den obersten d+1 Ebenen suchen"), i.e.
    // the min-w2 (oldest) among the ~3 lightest CPs -- the binary-heap root
    // (slot 0) + its two children (slots 1,2) -- NOT the global oldest.
    u32 topcap = getenv("THVM_ATP_WM_ITL_TOP") ?
                   (u32)strtoul(getenv("THVM_ATP_WM_ITL_TOP"), NULL, 10) : 0u;
    u32 top = (topcap == 0u || topcap > s->n_cps) ? s->n_cps : topcap;
    u32 best = 0;
    for (u32 i = 1; i < top; i++) {
      // node.seq == cp_seq[node.row] (refresh invariant).
      if (s->cp_heap[i].seq < s->cp_heap[best].seq) best = i;
    }
    j = best;
    s->n_cps_w2_picks++;
  } else if (s->w2_modulo > 0u &&
      (s->cp_select_count % s->w2_modulo) == 0u) {
    // K-D Heap secondary dimension (WM CPdimension d=1): every
    // w2_modulo-th selection picks the min-cp_pri2 entry instead of
    // the primary heap root.  O(n_cps) scan over heap positions; the
    // period gates cost.  Surfaces structurally simple CPs buried
    // under primary weight.
    u32 best = 0;
    // cp_pri2 is NOT cached in the node (mode-gated key) -- row loads;
    // seq comes from the node cache (== cp_seq[row], invariant).
    u32 bp2 = s->cp_pri2[s->cp_heap[0].row], bsq = s->cp_heap[0].seq;
    for (u32 i = 1; i < s->n_cps; i++) {
      u32 vv = s->cp_heap[i].row;
      if (s->cp_pri2[vv] < bp2
          || (s->cp_pri2[vv] == bp2 && s->cp_heap[i].seq < bsq)) {
        best = i;
        bp2 = s->cp_pri2[vv];
        bsq = s->cp_heap[i].seq;
      }
    }
    j = best;
    s->n_cps_w2_picks++;
  } else if (s->use_goal_interleave > 0u &&
      (s->cp_select_count % s->use_goal_interleave) == 0u) {
    // Goal-directed pick: the most goal-relevant queued CP (min
    // cp_goal).  E-style ratio -- the other picks are weight-based,
    // building the system; this steers toward the goal.  Ties keep the
    // LOWEST heap position (strict <), so the scan must walk positions.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      // cp_goal is NOT cached in the node (mode-gated key) -- row loads.
      if (s->cp_goal[s->cp_heap[i].row] < s->cp_goal[s->cp_heap[best].row]) {
        best = i;
      }
    }
    j = best;
  } else if (s->random_modulo > 0u &&
             (s->cp_select_count % s->random_modulo) == 0u) {
    // Vampire-style random pick: advance xorshift64 once, take the
    // result mod n_cps.  Deterministic given the seed; trajectory
    // differs from heap-min so the portfolio can sample paths the
    // weight-greedy walk misses (Vampire's distinctive McCune win).
    u64 x = s->rng_state ? s->rng_state : 0x9E3779B97F4A7C15ull;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    s->rng_state = x;
    j = (u32)(x % (u64)s->n_cps);
  } else if (atp_cp_fifo_dimension(s)) {
    // FIFO dimension: the oldest queued LIVE CP is the lowest cp_seq
    // among entries whose parents still live.  Skipping corpses inside
    // this one O(n_cps) scan replaces the old drain (pop + heap-repair
    // + slot-compaction per corpse: the OrAssociativity pick-2925
    // crossing popped ~765k dead entries over minutes) with a single
    // pass; the selected CP is identical -- the drain's sole outcome
    // was the first alive oldest.  Corpses stay queued and are
    // discarded lazily when the weight dimension reaches them, exactly
    // as before for non-FIFO picks.  Only the CHEAP no-unpack death
    // tests run here (trace orphan / uid).
    // Monotone dead-prefix floor (see AtpState.fifo_floor_seq): every
    // death test here is permanent (set-only death marks), so every
    // entry below the floor was proven dead by an earlier scan or has
    // been extracted.
    if (s->s2a_coeff > 0u) {
      // s2a (Vampire sine_to_age): the age dimension orders by effective
      // age = cp_seq + s2a_coeff*cp_sine, so a goal-distant CP (high SInE
      // level) is made younger and the goal-NEAR oldest wins.  The dead-
      // prefix floor is bypassed: effective age is not monotone in the
      // scan order, so no prefix is provably dead.  O(n_cps) like the raw
      // scan below (the floor only ever let it skip a prefix).
      u64 best_eff = UINT64_MAX;
      u32 best = 0xffffffffu;
      for (u32 i = 0; i < s->n_cps; i++) {
        u32 vv = s->cp_heap[i].row;
        u64 eff = (u64)s->cp_heap[i].seq
                + (u64)s->s2a_coeff * (u64)s->cp_sine[vv];
        if (best != 0xffffffffu && eff >= best_eff) continue;
        if (s->use_orphan_murder) {
          if (atp_cp_is_orphan(s, s->cp_trace[vv])) continue;
          if (atp_cp_uid_orphan(s, vv)) continue;
        }
        best = i;
        best_eff = eff;
      }
      j = (best != 0xffffffffu) ? best : 0u;
    } else {
    const u32 floor_seq = s->fifo_floor_seq;
    u32 best = 0xffffffffu;
    u32 best_sq = 0xffffffffu;
    for (u32 i = 0; i < s->n_cps; i++) {
      // seq from the packed node (== cp_seq[row], refresh invariant):
      // the age filters run on the sequential node walk alone; only an
      // improving candidate pays the payload loads (trace / uid).
      u32 sq = s->cp_heap[i].seq;
      if (sq < floor_seq) continue;                 // proven dead prefix
      if (best != 0xffffffffu && sq >= best_sq) continue;
      if (s->use_orphan_murder) {
        u32 vv = s->cp_heap[i].row;
        if (atp_cp_is_orphan(s, s->cp_trace[vv])) continue;
        if (atp_cp_uid_orphan(s, vv)) continue;
      }
      best = i;
      best_sq = sq;
    }
    j = (best != 0xffffffffu) ? best : 0u;
    // The scanned minimum is extracted below on every path (selected,
    // or discarded as orphan and re-picked), and pushes
    // stamp monotonically growing seqs -- so everything at or below its
    // seq is dead or gone from here on.
    if (best != 0xffffffffu) s->fifo_floor_seq = best_sq + 1u;
    }
  }

  // The chosen CP: j is its heap POSITION, v its stationary payload
  // row.  Every payload read/write below routes through v; j survives
  // only as the position (printed in the trace lines + heap repair).
  const u32 v = s->cp_heap[j].row;

  // Lazy orphan check.  If the chosen CP descends from an interreduced-
  // away rule, extract+discard it without counting a selection and
  // re-pick.  WM IncAnzKPEntfernt ticks per discard (KPVerwaltung.c:539).
  int orphan = s->use_orphan_murder && atp_cp_is_orphan(s, s->cp_trace[v]);
  // WM Waisenmord uid layer (see AtpState.r_uid): parent-identity death
  // test that works past the proof-trace soft cap AND for post-cap rules
  // whose r_trace is ATP_TRACE_NONE (where both tests above are blind) --
  // the WolframAxioms DoubleNegation firstdiv-796 extra pop.
  if (!orphan && s->use_orphan_murder && atp_cp_uid_orphan(s, v)) {
    orphan = 1;
  }
  // Gated forensic print for the silent at-pop orphan discard (the only
  // queue-removal path with no CPSEL line): seq/pri identify the victim
  // against CPFORM records.  Diagnostic only; zero cost unset.
  {
    static int otr = -1;
    if (otr < 0) otr = getenv("THVM_ATP_ORPHAN_TRACE") != NULL ? 1 : 0;
    if (otr && orphan) {
      // Decompose the verdict: which parent trace id is marked dead, and
      // whether the uid layer (not the trace layer) fired instead.
      u32 pa = 0xffffffffu, pb = 0xffffffffu;
      u8 pa_dead = 0u, pb_dead = 0u;
      u32 ct = s->cp_trace[v];
      if (ct != ATP_TRACE_NONE && ct < s->n_trace) {
        Term te = s->trace[ct];
        if (term_tag(te) == TAG_CTR && term_ext(te) == TRACE_CP) {
          pa = (u32)term_val(term_ctr_at(te, 0));
          pb = (u32)term_val(term_ctr_at(te, 1));
          pa_dead = (u8)atp_trace_is_dead(s, pa);
          pb_dead = (u8)atp_trace_is_dead(s, pb);
        }
      }
      fprintf(stderr,
              "ORPHDROP seq=%u pri=%u trace=%u pa=%u(dead=%u) pb=%u(dead=%u) "
              "uid_orphan=%u\n",
              s->cp_seq[v], s->cp_pri[v], s->cp_trace[v],
              pa, pa_dead, pb, pb_dead,
              (u8)atp_cp_uid_orphan(s, v));
    }
  }
  if (!orphan) s->cp_select_count++;

  // Unpack the chosen CP from its byte string into two fresh heap
  // Terms for the caller to normalize.  An implicit slot has no byte
  // string -- it materializes the raw pair off its TRACE_CP entry
  // instead (zero-copy; see atp_cp_implicit_materialize for the
  // liveness/aliasing argument).  The orphan check above already ran
  // on cp_trace[v], which the push side sets for implicit slots too,
  // so a dead-parent implicit CP is discarded below exactly like an
  // eager one (WM selectNonOrphan covers the implicit passive set).
  // Orphan pops are discarded without touching the terms (the loop
  // re-picks), so skip the unpack: a FIFO pop draining a long dead
  // prefix would otherwise allocate two fresh Terms per corpse and
  // exhaust the from-space mid-drain (OrAssociativity pick 2925
  // drains ~194k dead entries).  WM's selectNonOrphan likewise tests
  // parent liveness without building the termpair.
  if (orphan) {
    *lhs_out = 0;
    *rhs_out = 0;
  } else {
    // Raw-desc lane: convert a descriptor slot to a plain packed slot
    // (or run the cert-mode memcmp) BEFORE the unpack, so the whole
    // downstream pop path -- unpack, mixmost byte->FT stash, the
    // free-on-pop -- is the eager code path bit-for-bit.  Orphan pops
    // never materialize (the whole point of the descriptor).
    atp_cp_rdesc_pop_fix(s, v);
    atp_cp_slot_read(s, v, lhs_out, rhs_out);
    // ifeq guard-DECIDING telemetry: did the CP the main loop actually
    // picked decide a live guard (see atp_ifeq_decide_hit)?  Counts
    // distinct SELECTIONS, complementing g_ifeq_decide_n_hit's scoring-
    // event count -- see the bench stats print.
    if (s->use_ifeq_decide && atp_ifeq_decide_hit(s, *lhs_out, *rhs_out)) {
      g_ifeq_decide_n_popped++;
    }
#ifdef THVM_ATPFT_RULES
    // Byte->FT pop stash: decode the selected packed CP's byte string
    // straight into FT spans for the mixmost pop treatment (which
    // otherwise re-encodes the just-unpacked Terms via ft_from_term).
    // Only for eager packed slots on the mixmost/non-record path; the
    // consumer falls back to ft_from_term when the stash is empty
    // (implicit slot, non-mixmost preset, record_norm_steps replay).
    g_pop_ft_l = NULL;
    g_pop_ft_r = NULL;
    if (s->use_mixmost_nf && !s->record_norm_steps &&
        !atp_cp_slot_implicit(s, v) && s->cp_packed[v] != NULL) {
      AtpFt *pa = (AtpFt *)s->ft_arena_ptr;
      const u8 *pp = s->cp_packed[v];
      g_pop_ft_l = acp_unpack_term_ft(pa, &pp, 0);
      g_pop_ft_r = acp_unpack_term_ft(pa, &pp, 0);
    }
#endif
  }
  s->last_popped_trace = s->cp_trace[v];
  s->last_popped_kind  = s->cp_kind[v];
  s->last_popped_depth = (s->cp_depth != NULL) ? s->cp_depth[v] : 0u;
  s->last_popped_gd    = (s->cp_gd != NULL) ? s->cp_gd[v] : 0u;
  // Cleared per pop; the KBO_UN orient branch sets it iff the CP-formation
  // side swap physically fires, so the emission mirror never reads a stale
  // verdict from an earlier (e.g. orientable) selection.
  s->last_cp_wm_side_swapped = 0u;

  // Env-gated CP-selection trajectory dump for parity comparison vs
  // external provers (WaldmeisterProcess / VampireProcess).  Emits
  // one line per selected CP: pick number, queue index, sequence id,
  // priority, current rule count, then S-expr LHS/RHS.  Diff against
  // `wmcli -:l0 -P verbose` finds the algorithmic divergence point.
  // Env-gated CP-queue snapshot at a target pick (THVM_ATP_HEAPDUMP_AT):
  // dump every queued CP in the chosen CP's priority band with its seq +
  // terms, the thvm counterpart of Waldmeister WM_HEAPDUMP_AT.  Fires only
  // for the matching non-orphan pick; off (one cached probe) otherwise.
  if (!orphan) {
    // Pick-gated live-rule-set dump (THVM_ATP_DUMP_RULES_AT_PICK=N): the
    // rule-count-gated LIVERULE dump can't target a specific step; this one
    // prints the live set at a chosen selection.  Output-only.
    static int rd_at = -2;
    if (rd_at == -2) { const char *e = getenv("THVM_ATP_DUMP_RULES_AT_PICK"); rd_at = e ? atoi(e) : -1; }
    if (rd_at >= 0 && (u32)rd_at == s->cp_select_count) {
      for (u32 k = 0; k < s->n_rules; k++) {
        fprintf(stderr, "PICKRULE[%u] or=%u uid=%u lhs=", k, s->r_orient[k],
                s->r_uid ? s->r_uid[k] : 0u);
        atp_dbg_print_term(stderr, s->lhs[k]);
        fprintf(stderr, " rhs="); atp_dbg_print_term(stderr, s->rhs[k]);
        fputc('\n', stderr);
      }
    }
    static int hd_at = -2;
    static int hd_all = 0;
    if (hd_at == -2) {
      const char *e = getenv("THVM_ATP_HEAPDUMP_AT"); hd_at = e ? atoi(e) : -1;
      // THVM_ATP_HEAPDUMP_ALL: dump EVERY queued CP at the chosen pick,
      // not just the picked band -- localizes where a known-needed CP
      // (matched by seq or content) sits in the flood.  Output-only.
      hd_all = atp_env_on("THVM_ATP_HEAPDUMP_ALL");
    }
    if (hd_at >= 0 && (u32)hd_at == s->cp_select_count) {
      u32 band = s->cp_pri[v], n_band = 0;
      fprintf(stderr, "==THVMHEAP pick=%u band_pri=%u n_cps=%u==\n", s->cp_select_count, band, s->n_cps);
      for (u32 q = 0; q < s->n_cps; q++) {
        u32 qv = s->cp_heap[q].row;   // heap-position order, exactly as before
        if (!hd_all && s->cp_pri[qv] != band) continue;
        Term ql = 0, qr = 0;
        atp_cp_slot_read(s, qv, &ql, &qr);
        fprintf(stderr, "  HCP seq=%u pri=%u lhs=", s->cp_seq[qv], s->cp_pri[qv]);
        atp_dbg_print_term(stderr, ql);
        fprintf(stderr, " rhs=");
        atp_dbg_print_term(stderr, qr);
        fputc('\n', stderr);
        n_band++;
      }
      fprintf(stderr, "==THVMHEAPEND pick=%u n_band=%u==\n", s->cp_select_count, n_band);
    }
  }

  {
    static int cp_pick_trace = -1;
    if (cp_pick_trace < 0) cp_pick_trace = atp_env_on("THVM_ATP_CP_PICK_TRACE");
    if (cp_pick_trace && orphan) {
      // Discarded pop (orphan): the aligner skips these as
      // non-selections, so spare the term rendering -- a FIFO pop draining
      // a long dead prefix otherwise prints GBs and dominates the wall.
      // j stays the heap POSITION -- trace format byte-identical.
      fprintf(stderr, "CPSKIP pick=%u j=%u seq=%u pri=%u\n",
              s->cp_select_count, j, s->cp_seq[v], s->cp_pri[v]);
    } else if (cp_pick_trace) {
      fprintf(stderr, "CPSEL pick=%u j=%u seq=%u pri=%u rules=%u d=%u actre=%u lhs=",
              s->cp_select_count, j, s->cp_seq[v], s->cp_pri[v], s->n_rules,
              g_kdh_last_d, s->n_activated_re);
      atp_dbg_print_term(stderr, *lhs_out);
      fputs(" rhs=", stderr);
      atp_dbg_print_term(stderr, *rhs_out);
      if (orphan && s->cp_trace[v] == ATP_TRACE_NONE) {
        // Uid-layer orphan past the proof-trace soft cap: the parents live
        // in the uid stamps, not the (absent) trace entry.  Skipped by the
        // aligner exactly like a genuine orphan (no counted selection).
        // (Tag + par_a field kept byte-identical to the historical print;
        // the cp_par_a side-array died with the wolf-dup detector, and in
        // the canonical config it was always absent here.)
        fprintf(stderr, " WOLFDUPORPHAN par_a=%u(%s) uid_a=%u(%s) uid_b=%u(%s)",
                ATP_TRACE_NONE, "live",
                (s->cp_uid_a != NULL) ? s->cp_uid_a[v] : 0u,
                (s->cp_uid_a != NULL && atp_uid_is_dead(s, s->cp_uid_a[v]))
                    ? "dead" : "live",
                (s->cp_uid_b != NULL) ? s->cp_uid_b[v] : 0u,
                (s->cp_uid_b != NULL && atp_uid_is_dead(s, s->cp_uid_b[v]))
                    ? "dead" : "live");
      } else if (orphan) {
        Term te = s->trace[s->cp_trace[v]];
        u32 pa = (u32)term_val(term_ctr_at(te, 0));
        u32 pb = (u32)term_val(term_ctr_at(te, 1));
        fprintf(stderr, " ORPHAN pa=%u(%s) pb=%u(%s)",
                pa, atp_trace_is_dead(s, pa) ? "dead" : "live",
                pb, atp_trace_is_dead(s, pb) ? "dead" : "live");
      }
      fputc('\n', stderr);
    }
  }
#ifdef ATP_FV_INDEX
  // 7d: the popped CP leaves the queue -- drop it from the index so a
  // later subsumption query never matches a stale, no-longer-queued
  // CP.  Mark the record dead BEFORE freeing the byte string it
  // borrows: a dead record is never dereferenced, so the borrow stays
  // sound.
  atp_fv_index_remove(s->fv_index, s->cp_seq[v]);
#endif
  free(s->cp_packed[v]);
  s->cp_packed[v] = NULL;
  atp_cp_implicit_clear(s, v);
#ifdef THVM_ATPFT_CPQ
  // Stage 7: the popped row's FT spans return to Arena A's free list
  // BEFORE the row densify -- the row is about to be overwritten with
  // the last row's entry (still owned).  No-op when the entry was
  // never populated (NULL slots are idempotent on clear).
  atp_cp_ft_clear(s, v);
#endif
  // Extraction at heap position j + payload row v, two independent
  // backfills (the position-level one reproduces the historical
  // move-the-last-slot-into-j layout evolution byte-for-byte; the
  // row-level one keeps the payload rows dense in [0, n_cps)).  The
  // heap shrink MUST run before the cp_pos[last-row] read: the last
  // row may be the very CP that sat at the popped last position.
  s->n_cps--;
  u32 last_pos = s->n_cps;
  u8 need_repair = (j != last_pos);
  if (need_repair) {
    // Whole-node move: the key caches ride along with their row
    // (keys untouched -- invariant preserved).
    AtpCpHeapNode moved = s->cp_heap[last_pos];
    s->cp_heap[j] = moved;
    s->cp_pos[moved.row] = j;
  }
  u32 lr = s->n_cps;               // last (to-densify) payload row
  if (v != lr) {
    atp_cp_row_move(s, /*dst=*/v, /*src=*/lr);
    u32 p = s->cp_pos[lr];
    // Row move: keys copied verbatim to row v -- rewire only .row.
    s->cp_heap[p].row = v;
    s->cp_pos[v] = p;
  }
  if (need_repair) {
    if (atp_kdheap_on()) atp_kdh_delete_repair(s, j);
    else { atp_cp_sift_up(s, j); atp_cp_sift_down(s, j); }
  }

  if (orphan) {
    s->n_cps_dropped_orphan++;
    if (s->n_cps == 0u && s->n_cp_stash > 0u
        && s->auto_max_cp_weight_base > 0u) {
      atp_auto_maxw_drain(s, 1u);
    }
    if (s->n_cps == 0) return 0;  // queue drained to orphans only
    continue;                     // re-pick the next heap-min
  }
  // ENIGMA training-data hook: record the processed CP's feature
  // vector + trace id.  Gated -- with the flag off (the default) this
  // is a single predictable-branch test and the engine is byte-
  // identical to the untracked run.
  if (s->record_cp_features) {
    atp_cp_feat_record(s, *lhs_out, *rhs_out, s->last_popped_trace);
  }
  return 1;
  }  // end selectNonOrphan loop
}

// 7c': re-establish the CP-queue heap invariant over cp_packed[0..n_cps).
// The normal path keeps the queue a heap via atp_cp_heap_push, but a
// caller (chiefly tests) that populates cp_packed / n_cps directly
// (via thvm_atp_cp_set) must call this so cp_pri is filled and the
// array satisfies the heap order before select / peek.
//
// Split into two phases so the IR sweep can fold the per-CP priority
// recompute into its single survivor walk (it already holds the unpacked
// sides), and only run the FULL-array work (Floyd + FV-index rebuild)
// once at the end.  The public entry below stitches both back together
// for orphan-kill and direct cp_set callers.
static void atp_cp_rebuild_priorities(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  atp_ensure_cp_cap(s, s->n_cps);
  for (u32 i = 0; i < s->n_cps; i++) {
    // i is a heap POSITION; vv the CP's payload row.
    u32 vv = s->cp_heap[i].row;
    // Each packed CP unpacks two transient heap Terms below; on a deep
    // queue (~10^5 CPs) those overrun the from-space before the next
    // step-top GC.  Reclaim them in-loop: the unpacked Terms are consumed
    // within the iteration, so nothing live is carried across the boundary
    // (cp_packed bytes + rooted rule/trace arrays survive a Cheney pass).
    if (s->trace_pack && (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {
      thvm_atp_gc_collect(s);
    }
    if (atp_cp_slot_implicit(s, vv)) {
      // Deferred slot: no packed bytes to unpack.  Reuse the push-time
      // cached priority (this is what the descriptor caches it for);
      // cp_goal / cp_pri2 keep their push-time values, which stay put
      // on the stationary row.
      s->cp_pri[vv] = s->cp_implicit[vv].priority;
      // cp_pri[vv] (re)written for a queued row: refresh the packed
      // node caches before the trailing Floyd compares against them.
      atp_cp_heap_set(s, i, vv);
      continue;
    }
    Term l = 0, r = 0;
#ifdef THVM_ATPFT_CPQ
    // Byte-queue retirement: source the term pair from the FT image
    // (packed slot, implicit already handled above).  Same terms
    // acp_unpack would rebuild -> same recomputed priority.
    { AtpFtCell *fl, *fr; atp_cp_ft_slot_get(s, vv, &fl, &fr);
      l = ft_to_term(fl); r = ft_to_term(fr); }
#else
    acp_unpack(s->cp_packed[vv], &l, &r);
#endif
    // cp_deprio stays POSITION-indexed (raw i): the historical sift /
    // backfill never moved it, so its byte at position i -- stamped by
    // whichever append last landed there -- is what the historical
    // rebuild read.  Preserves that quirk bit-exactly (GJ-keep configs).
    g_twee_dbg_ctx = 3u;
    s->cp_pri[vv] = s->cp_deprio[i] ? ATP_CP_PRI_NEVER
                                    : atp_cp_priority(s, l, r);
    g_twee_dbg_ctx = 0u;
    // spb=goal re-score from the row's stored derivation bit (the
    // commit_priorities discipline).
    if (s->cp_gd != NULL)
      s->cp_pri[vv] = atp_spb_scale(s->cp_pri[vv], s->cp_gd[vv]);
    s->cp_goal[vv] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, l, r) : 0u;
    s->cp_sine[vv] = atp_cp_sine_level(s, l, r);
    s->cp_pri2[vv] = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                  : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, l, r, s->w2_mode)
                  :                        0u;
    // cp_seq is NOT touched: insertion age is stamped once (heap push /
    // thvm_atp_cp_set) and survives every reweight, matching WM's
    // C_ReClassify which only recomputes w1 ("w2 wird nicht geaendert"
    // / w2 is not changed, CLAS/NewClassification.c:399-406).
    // cp_pri[vv] (re)written for a queued row: refresh the packed node
    // caches before the trailing Floyd compares against them.  (Also
    // heals the direct-build path: thvm_atp_cp_set leaves node.pri to
    // this rebuild, exactly like cp_pri itself.)
    atp_cp_heap_set(s, i, vv);
  }
}

static void atp_cp_floyd_only(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  // Floyd build-heap: sift down every internal node, last to first.
  if (atp_kdheap_on()) atp_kdh_build(s);
  else for (u32 i = s->n_cps / 2; i > 0; ) {
    i--;
    atp_cp_sift_down(s, i);
  }
#ifdef ATP_FV_INDEX
  // 7d: a sweep / compaction ahead of this reheapify may have dropped
  // CPs, so the incremental insert/remove path can no longer track the
  // set; rebuild the index wholesale from the live CP arrays (cp_seq,
  // the index's stable key, is preserved across the rebuild).
  atp_fv_index_rebuild(s);
#endif
}

// Per-survivor priority commit used by atp_cp_set_interreduce: at each
// w-commit site the IR sweep already has (l, r) in scope (either
// the pre-normalize (ol, orr) on the fast-path skip branches or the
// post-normalize (l, r) on the reweight branch).  Inlined here so the
// post-loop reheapify only has to do the FULL-array work (Floyd + FV).
// Soundness: the three weight functions are pure in (s, l, r) + state
// fields immutable during the IR sweep (cp_weight_mode, w2_*, gnn_coop,
// use_*, goal_*, sym_level[], conj_sym_mask, s->lhs/rhs); evaluating
// inside the loop yields bit-identical values to evaluating post-loop
// in atp_cp_rebuild_priorities.  `w` is the survivor's post-compaction
// heap POSITION (the raw cp_deprio index, matching the rebuild's
// position-quirk); `slot` its stationary payload row.  Payload-only
// writes: every caller stamps the packed node caches AFTERWARDS via
// atp_cp_heap_set(s, w, slot) (the IR sweep's w-commit sites), so the
// refresh invariant is re-established before the post-loop Floyd.
static inline void atp_cp_commit_priorities(AtpState *s, u32 w, u32 slot,
                                            Term l, Term r) {
  // Re-score with THIS CP's formation depth (Twee getDepth), not the last
  // push's stale g_cp_formation_depth -- the ATP_CP_WEIGHT_TWEE score reads
  // the global, so a dynamic re-score (cp_set_interreduce) must restore it
  // from the stored cp_depth or the depth term would be wrong.  A no-op for
  // the WM preset (mode 4 ignores the global).
  if (s->cp_depth != NULL) g_cp_formation_depth = s->cp_depth[slot];
  g_twee_dbg_ctx = 3u;
  s->cp_pri[slot]  = s->cp_deprio[w] ? ATP_CP_PRI_NEVER
                                     : atp_cp_priority(s, l, r);
  // spb=goal re-score: re-apply the derivation-bit scale from THIS
  // CP's stored bit (the g_cp_formation_depth restore discipline).
  if (s->cp_gd != NULL)
    s->cp_pri[slot] = atp_spb_scale(s->cp_pri[slot], s->cp_gd[slot]);
  s->cp_goal[slot] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                    ? atp_goal_weight(s, l, r) : 0u;
  s->cp_sine[slot] = atp_cp_sine_level(s, l, r);
  s->cp_pri2[slot] = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, l, r, s->w2_mode)
                :                        0u;
  g_twee_dbg_ctx = 0u;
}

fn void thvm_atp_cp_reheapify(AtpState *s) {
  atp_cp_rebuild_priorities(s);
  atp_cp_floyd_only(s);
}

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
