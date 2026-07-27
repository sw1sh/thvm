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
