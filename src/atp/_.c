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
static void atp_cp_ft_swap      (AtpState *s, u32 i, u32 j);
static void atp_cp_ft_move      (AtpState *s, u32 dst, u32 src);
#endif

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
# ifdef THVM_ATPFT_UNIFY
#  include "ft_unify.c"
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
static u32  atp_wmo_victim_drain_key(AtpState *s, u32 trace,
                                     u8 reduced_thvm_side);
static void atp_wmo_rename_trace(AtpState *s, u32 old_t, u32 new_t);
static u64  atp_wmo_rank(AtpState *s, u32 f, u32 i, u32 j, u8 combo,
                         const CriticalPair *cp);
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
// after the wm_order.c include; forward-declared so the emission-rank
// mirror (atp_wmo_rank) can exclude commutativity partners from the
// re-derived proper-overlap phase deferral.
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
    Term out = atp_rewrite_normalize_ordered(s, t, lhs, rhs, n_rules, step_cap);
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
  if (nl == NULL || nr == NULL || nt == NULL || no == NULL ||
      nd == NULL || nls == NULL || nrs == NULL || ngj == NULL ||
      nod == NULL || nsl == NULL || nsr == NULL || nrc == NULL) {
    fprintf(stderr, "atp_ensure_rule_cap: realloc to %u rules failed\n",
            cap);
    exit(1);
  }
  s->lhs = nl; s->rhs = nr; s->r_trace = nt; s->r_orient = no;
  s->r_dead = nd; s->r_dead_lhs_save = nls; s->r_dead_rhs_save = nrs;
  s->r_gj_status = ngj; s->r_overlap_done = nod;
  s->r_dead_subsumer_lhs = nsl; s->r_dead_subsumer_rhs = nsr;
  s->r_rederive_cut = nrc;
  for (u32 i = s->r_cap; i < cap; i++) {
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
  if (nlf == NULL || nrf == NULL || nlsf == NULL || nrsf == NULL) {
    fprintf(stderr, "atp_ensure_rule_cap: ft realloc to %u rules failed\n",
            cap);
    exit(1);
  }
  s->lhs_ft = nlf; s->rhs_ft = nrf;
  s->r_dead_lhs_save_ft = nlsf; s->r_dead_rhs_save_ft = nrsf;
  for (u32 i = s->r_cap; i < cap; i++) {
    s->lhs_ft[i]             = NULL;
    s->rhs_ft[i]             = NULL;
    s->r_dead_lhs_save_ft[i] = NULL;
    s->r_dead_rhs_save_ft[i] = NULL;
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

// Gated classification-order trace (env THVM_ATP_CP_FORM_TRACE).  Emits
// one CPFORM line per CP at the moment cp_seq is stamped, so thvm's
// CP-classification order can be aligned against Waldmeister's verbose
// `... added to SUE: w, age` sequence and the first cross-batch
// age-order divergence localized.  Off by default; one cached env probe.
static void atp_cp_form_trace(u32 seq, u32 w, Term lhs, Term rhs) {
  static int on = -1;
  if (on < 0) on = (getenv("THVM_ATP_CP_FORM_TRACE") != NULL) ? 1 : 0;
  if (!on) return;
  fprintf(stderr, "CPFORM seq=%u w=%u lhs=", seq, w);
  atp_dbg_print_term(stderr, lhs);
  fprintf(stderr, " rhs=");
  atp_dbg_print_term(stderr, rhs);
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
  u32  *ng = (u32  *)realloc(s->cp_goal,  cap * sizeof(u32));
  u32  *np2 = (u32 *)realloc(s->cp_pri2,  cap * sizeof(u32));
  u32  *nlnr = (u32 *)realloc(s->cp_last_norm_r_revision,
                              cap * sizeof(u32));
  if (nc == NULL || nt == NULL || np == NULL || nq == NULL || ng == NULL
      || np2 == NULL || nlnr == NULL) {
    fprintf(stderr, "atp_ensure_cp_cap: realloc to %u CPs failed\n", cap);
    exit(1);
  }
  s->cp_packed = nc; s->cp_trace = nt;
  s->cp_pri = np; s->cp_seq = nq; s->cp_goal = ng; s->cp_pri2 = np2;
  s->cp_last_norm_r_revision = nlnr;
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

// Swap the descriptors + tag bits at slots i, j (heap sift).
static inline void atp_cp_implicit_swap(AtpState *s, u32 i, u32 j) {
  if (s->cp_is_implicit == NULL) return;
  if (s->cp_implicit != NULL) {
    AtpCpImplicit t = s->cp_implicit[i];
    s->cp_implicit[i] = s->cp_implicit[j];
    s->cp_implicit[j] = t;
  }
  u8 bi = atp_cp_slot_implicit(s, i);
  u8 bj = atp_cp_slot_implicit(s, j);
  if (bj) atp_cp_implicit_set(s, i); else atp_cp_implicit_clear(s, i);
  if (bi) atp_cp_implicit_set(s, j); else atp_cp_implicit_clear(s, j);
}

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

// Worst-case packed bytes for term `t`: 1 discriminator + 2 varints,
// each varint <= 10 bytes -> 21 bytes per node is a safe bound.
static u32 acp_packed_bound(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t), c = 21u;
      for (u32 i = 0; i < n; i++) c += acp_packed_bound(term_ctr_at(t, i));
      return c;
    }
    default: return 21u;
  }
}

// Append `t`'s preorder packing to `*pp`.  CTR: 'C', label, arity,
// then children; FVR: 'V', var id; NUM: 'N', dtype, raw value;
// anything else: 'E' (era placeholder -- ATP terms are first-order, so
// this is unreachable in practice but keeps unpack total).  `*nodes`
// is bumped once per node visited -- the pack walks every node anyway,
// so the CP's symbol count (its selection weight) falls out for free,
// sparing atp_cp_priority a second full traversal.
static void acp_pack_term(Term t, u8 **pp, u32 *nodes) {
  (*nodes)++;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      *(*pp)++ = (u8)'C';
      acp_put_varint(pp, term_ext(t));
      acp_put_varint(pp, n);
      for (u32 i = 0; i < n; i++) acp_pack_term(term_ctr_at(t, i), pp, nodes);
      return;
    }
    case TAG_NUM:
      *(*pp)++ = (u8)'N';
      acp_put_varint(pp, term_ext(t));
      acp_put_varint(pp, term_val(t));
      return;
    case TAG_FVR:
      *(*pp)++ = (u8)'V';
      acp_put_varint(pp, term_ext(t));
      return;
    default:
      *(*pp)++ = (u8)'E';
      return;
  }
}

// Rebuild one preorder-packed term, advancing `*pp` past it.
static Term acp_unpack_term(const u8 **pp) {
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

// Pack a critical pair (lhs, rhs) into a fresh malloc'd byte string;
// `*out_len` receives its length, `*out_nodes` the total symbol count
// of lhs+rhs (the CP's selection weight).  Either out-param may be
// NULL.  The two terms pack back to back -- the preorder records are
// self-delimiting via arity, so acp_unpack reads lhs then rhs with no
// separator.
static u8 *acp_pack(Term lhs, Term rhs, u32 *out_len, u32 *out_nodes) {
  u32 bound = acp_packed_bound(lhs) + acp_packed_bound(rhs);
  u8 *buf = (u8 *)malloc(bound);
  if (buf == NULL) thvm_fatal("acp_pack: OOM");
  u8 *p = buf;
  u32 nodes = 0u;
  acp_pack_term(lhs, &p, &nodes);
  acp_pack_term(rhs, &p, &nodes);
  if (out_len   != NULL) *out_len   = (u32)(p - buf);
  if (out_nodes != NULL) *out_nodes = nodes;
  return buf;
}

// Inverse of acp_pack: rebuild both heap Terms from the byte string.
static void acp_unpack(const u8 *buf, Term *lhs, Term *rhs) {
  const u8 *p = buf;
  Term l = acp_unpack_term(&p);
  Term r = acp_unpack_term(&p);
  if (lhs != NULL) *lhs = l;
  if (rhs != NULL) *rhs = r;
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
  free(s->cp_packed[i]);                 // free(NULL) is a no-op
  // The slot now holds packed bytes -- drop any deferred-CP tag so the
  // `cp_is_implicit[i] <=> cp_packed[i] == NULL` invariant holds.
  atp_cp_implicit_clear(s, i);
  s->cp_packed[i] = acp_pack(lhs, rhs, NULL, NULL);
  // Insertion age, stamped once here (mirroring the heap-push path) and
  // preserved across every later sweep / reheapify -- WM keeps the w2
  // FIFO key untouched on reweight (C_ReClassify, CLAS/
  // NewClassification.c:399-406 "w2 wird nicht geaendert").
  s->cp_seq[i] = s->cp_seq_next++;
#ifdef THVM_ATPFT_CPQ
  // Stage 7: mirror the slot into the parallel FT queue.  Test
  // harnesses populate the queue via thvm_atp_cp_set + reheapify;
  // keep the two views in lockstep so the heap swap routines stay
  // sound (they swap entries on both arrays unconditionally).
  atp_cp_ft_clear(s, i);
  atp_cp_ft_set(s, i, lhs, rhs, /*priority_hint=*/0u, /*origin=*/0xffffu);
#endif
}

// Read CP queue slot i back into two Terms -- the read counterpart of
// thvm_atp_cp_set.  A packed slot unpacks fresh transient heap Terms;
// a deferred (implicit) slot returns its trace-resident raw pair
// zero-copy.  Read-only either way.
static void atp_cp_slot_read(const AtpState *s, u32 i,
                             Term *lhs, Term *rhs);
fn void thvm_atp_cp_get(const AtpState *s, u32 i, Term *lhs, Term *rhs) {
  atp_cp_slot_read(s, i, lhs, rhs);
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
    // Deferred-CP slots are never indexed: there are no packed bytes to
    // borrow, and queue-subsumption deliberately skips the implicit
    // passive set (WM has no queue-vs-queue subsumption at all --
    // SS_TermpaarSubsummiertVonGM matches only the ACTIVE rule set).
    if (atp_cp_slot_implicit(s, i)) continue;
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[i], &l, &r);
    atp_fv_index_insert(ix, l, r, s->cp_packed[i], s->cp_seq[i]);
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
  if (s->wmfpa_check) wf_eng_check_incremental(s, a, alen);
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
  if (s->wmfpa_check) {
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
  }
  return s;
}

#ifdef ATP_MNF
static void mnf_destroy(struct AtpMnf *m);   // defined with the MNF module below
// GC support: the MNF coloured nodes hold reached Terms on the heap, so
// they are collector roots.  Defined with the MNF module; declared here
// for thvm_atp_gc_collect (which precedes the module).
static u32  mnf_gc_count(struct AtpMnf *m);
static void mnf_gc_gather(struct AtpMnf *m, Term *roots, u32 *w);
static void mnf_gc_writeback(struct AtpMnf *m, const Term *roots, u32 base);
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
  free(s->cp_pri);
  free(s->cp_goal);
  free(s->cp_seq);
  free(s->cp_pri2);
  free(s->cp_ultimate);
  free(s->cp_last_norm_r_revision);
  // Deferred-CP (`implicit_pair`) arc commit 1: the descriptor array and
  // the per-slot tag bitset are plain malloc'd blocks (no per-slot owned
  // resource), so a single free per array is enough.  free(NULL) is a
  // no-op -- handles the flag-OFF case where both pointers stay NULL.
  free(s->cp_implicit);
  free(s->cp_is_implicit);
  // Auto-MaxWeight overflow stash: each packed byte string is owned
  // here too (free(NULL) is a no-op for slots already drained).
  if (s->cp_stash_packed != NULL) {
    for (u32 i = 0; i < s->n_cp_stash; i++) free(s->cp_stash_packed[i]);
    free(s->cp_stash_packed);
  }
  free(s->cp_stash_trace);
  free(s->cp_stash_nodes);
  free(s->cp_stash_ultimate);
  // IR-victim buffer (use_wm_demote; NULL when the flag never fired).
  free(s->irv_lhs);
  free(s->irv_rhs);
  free(s->irv_parent);
  free(s->irv_wmo_key);
  // ENIGMA training-data arrays (NULL unless recording was enabled).
  free(s->cp_feat_rows);
  free(s->cp_feat_trace);
  free(s->cp_feat_label);
#ifdef ATP_FV_INDEX
  atp_fv_index_free(s->fv_index);
#endif
#ifdef ATP_RULE_INDEX
  atp_ri_free(s->rule_index);
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
#ifdef ATP_MNF
  mnf_destroy(s->mnf);
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
              + 2u * s->n_irv /* buffered IR victims (use_wm_demote) */;
#ifdef ATP_MNF
  // Milestone 10: every MNF coloured node holds a reached Term.  Root
  // them so the collector relocates them; the hash table (structural
  // hashes, node indices) is GC-invariant and needs no fixup.
  n_roots += mnf_gc_count(s->mnf);
#endif
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
#ifdef ATP_MNF
  u32 mnf_node_root = w;
  mnf_gc_gather(s->mnf, roots, &w);
#endif

  gc_collect(roots, w);

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
#ifdef ATP_MNF
  // Write the relocated reached-Terms back into the MNF nodes.  No
  // hash-table fixup: mnf_hash is structural, so a relocated term
  // keeps its hash and stays in its bucket.
  mnf_gc_writeback(s->mnf, roots, mnf_node_root);
#endif

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
  s->t_cap = cap;
  return 1;
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
  Term children[6 + CP_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, p_b);
  children[2] = lhs;
  children[3] = rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5 + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len] = term_new(0, TAG_NUM, 0, combo);
  s->trace[s->n_trace] = term_new_ctr(TRACE_CP, children, 6u + pos_len);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
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
static u8 atp_wmo_eq_dist_rhs_base(const AtpState *s, u32 src_trace) {
  if (s == NULL || src_trace == ATP_TRACE_NONE || src_trace >= s->n_trace) {
    return 1u;
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
  // genuine two-parent superposition and an initial axiom (TRACE_AXIOM,
  // both parents absent) take the dist_rhs=1 default.
  if (reason == TRACE_SIMPLIFY) {
    return (p_a != ATP_TRACE_NONE) ? 0u : 1u;
  }
  if (reason == TRACE_CP) {
    return (p_a != ATP_TRACE_NONE && p_b == ATP_TRACE_NONE) ? 0u : 1u;
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
  if (s != NULL && s->last_cp_wm_side_swapped) return (u8)(1u - base);
  return base;
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

static u8 atp_enqueue_equation(AtpState *s, Term lhs, Term rhs,
                               u32 reason, u32 parent_a) {
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
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
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
  u32 trace_idx = atp_trace_push(s, reason, parent_a,
                                 ATP_TRACE_NONE, lhs, rhs);
  // Waldmeister history-driven Act_ultimate (NewClassification.c:314,
  // DEF=1 block `initial = ultimate`): tag input axioms so they pop
  // before any derived CP regardless of heuristic weight.  Effective
  // only when s->use_initial_ultimate is set; off by default.
  u8 is_ultimate = (reason == TRACE_AXIOM && parent_a == ATP_TRACE_NONE)
                     ? 1u : 0u;
  atp_cp_heap_push(s, lhs, rhs, trace_idx, is_ultimate, 0u);
  return 1;
}

fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
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
  s->n_goals = 0;
  s->goals_joined_mask = 0;
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
// Returns 1 on success, 0 if 8.4d's sort-check rejected the goal.
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
#ifdef THVM_ATPFT_RULES
    s->goal_lhs_ft = NULL;
    s->goal_rhs_ft = NULL;
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
  if (s->goals_lhs == NULL || s->goals_rhs == NULL ||
      s->goals_lhs_nf == NULL || s->goals_rhs_nf == NULL) {
    atp_goals_release(s);
    return 0;
  }
  for (u32 g = 0; g < n; g++) {
    s->goals_lhs[g] = lhs[g];
    s->goals_rhs[g] = rhs[g];
  }
  s->n_goals = n;
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
#endif
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

fn void thvm_atp_set_use_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_ground_join = on ? 1u : 0u;
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

// 8.5c: order-aware compare.  Picks LPO (if attached) or KBO,
// returning a unified KboCmp-shaped result.  The two enums share
// numeric values (EQ=0, GT=1, LT=-1, UN=2), so the cast is safe.
//
// Stage 8: a `THVM_ATP_LPO_ORIENT_CACHE` wrapper sits on top of this
// body (see below) and memoises the verdict by structural-hash pair.
// With the flag off, the macro alias below collapses back to this
// function and the default build is byte-identical.
static KboCmp atp_compare_uncached(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
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
                           : (u8)(atp_compare(s, lhs[i], rhs[i]) == KBO_GT);
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
                         : (u8)(atp_compare(s, lhs[i], rhs[i]) == KBO_GT);
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
static Term atp_rewrite_normalize_ordered(AtpState *s, Term t,
                                          const Term *lhs, const Term *rhs,
                                          u32 n_rules, u32 step_cap) {
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
      // Gated faithful WM-FPA path (flatterm + DSBaum + NormalformInner-
      // most).  Byte-identical NF to the indexed path; OFF by default.
      if (s->use_wmfpa) return atp_rewrite_normalize_wmfpa(s, t, step_cap);
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
      u32 m = atp_symbol_count_dedup(lhs);
      u32 n = atp_symbol_count_dedup(rhs);
      u32 large = m > n ? m : n;
      u32 small = m + n - large;
      u32 d = atp_term_depth(lhs);
      u32 dr = atp_term_depth(rhs);
      if (dr > d) d = dr;
      return 4u * large + 1u * small + 2u * d;
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
        u32 wi = atp_symbol_count(s->lhs[i]) + atp_symbol_count(s->rhs[i]);
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
//   Einfachmatch : one CP side matches.  Weight = residual x 5.
//   Nullmatch    : neither.  Weight = the CP's own size x 50.
// A goal-resembling CP thus scores small and is selected first,
// steering completion at the goal -- Waldmeister's key to fast proofs.
#define ATP_GOAL_SINGLE_FACTOR 5u
#define ATP_GOAL_NONE_FACTOR   50u

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
// Priority weight for a CP whose symbol-count sum is already known
// (`base`) -- e.g. counted for free during acp_pack.  Identical
// verdict to atp_cp_priority; only the redundant size walk is
// skipped.  The precomputed `base` is the ADD-mode value; a
// non-default cp_weight_mode recomputes the base from the terms.
static u32 atp_cp_priority_sized(AtpState *s, Term lhs, Term rhs, u32 base) {
  // Goal-directed mode (Waldmeister CPinGoal): weight by structural
  // match to the goal.  Opt-in via cp_weight_mode so completion-mode
  // and the other weights are unaffected.
  if (s != NULL && s->cp_weight_mode == ATP_CP_WEIGHT_GOAL &&
      s->goal_lhs != 0) {
    return atp_goal_weight(s, lhs, rhs);
  }
  u32 mode = (s != NULL) ? s->cp_weight_mode : ATP_CP_WEIGHT_ADD;
  if (mode != ATP_CP_WEIGHT_ADD) {
    // The caller's `base` is the ADD-mode symbol-count sum; for any
    // other mode it must be recomputed from the CP terms.
    base = atp_cp_weight_base(s, lhs, rhs, mode);
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
  return base;
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
    if (!thvm_atp_cp_graph(lhs[i], rhs[i], &gs[i])) { free(gs); return 0; }
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

// M3: re-rank the live CP queue with the loaded GNN.  Pull -> score ->
// map to priority -> push back.  Only permutes order (completeness safe).
fn u32 thvm_atp_gnn_rerank(AtpState *s) {
  if (s == NULL || !g_atp_gnn_active) return 0u;
  u32 nq = thvm_atp_queued_cp_count(s);
  if (nq == 0u) return 0u;

  Term *lhs  = (Term *)malloc((size_t)nq * sizeof(Term));
  Term *rhs  = (Term *)malloc((size_t)nq * sizeof(Term));
  u32  *seq  = (u32 *)malloc((size_t)nq * sizeof(u32));
  float *sc  = (float *)malloc((size_t)nq * sizeof(float));
  u32  *pri  = (u32 *)malloc((size_t)nq * sizeof(u32));
  if (lhs == NULL || rhs == NULL || seq == NULL || sc == NULL || pri == NULL) {
    free(lhs); free(rhs); free(seq); free(sc); free(pri);
    return 0u;
  }
  u32 got = thvm_atp_queued_cps(s, lhs, rhs, seq, nq);
  if (got == 0u || !thvm_atp_gnn_score_batch(lhs, rhs, got, sc)) {
    free(lhs); free(rhs); free(seq); free(sc); free(pri);
    return 0u;
  }
  // Higher score -> lower priority, clamped to a safe u32 band - the
  // same map atp_cp_learned_priority + atpRerankPriority use.  Neutral
  // (score 0) maps to 1.0e6, the cp_pri2 push-time default in coop mode.
  for (u32 i = 0u; i < got; i++) {
    float pr = 1.0e6f - 1.0e4f * sc[i];
    if (pr < 0.0f) pr = 0.0f;
    if (pr > 2.0e9f) pr = 2.0e9f;
    pri[i] = (u32)pr;
  }
  // Coop mode: write the SECONDARY dimension (cp_pri2) so the GNN drives
  // the w2 coop pick while the primary heap stays the hand heuristic
  // (e.g. Waldmeister).  Otherwise overwrite the primary heap (GT-mode).
  if (s->gnn_coop) {
    thvm_atp_set_cp_pri2_by_seq(s, seq, pri, got);
  } else {
    thvm_atp_set_cp_pri_by_seq(s, seq, pri, got);
  }
  s->n_gnn_reranks++;

  free(lhs); free(rhs); free(seq); free(sc); free(pri);
  return got;
}

fn void thvm_atp_set_gnn_rerank_period(AtpState *s, u32 period) {
  if (s == NULL) return;
  s->gnn_rerank_period = period;
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

// Ordering predicate: does queue slot i sort strictly before j?
static int atp_cp_before(const AtpState *s, u32 i, u32 j) {
  // Waldmeister history-driven Act_ultimate: a CP tagged "ultimate"
  // ranks strictly before every non-ultimate CP, regardless of
  // heuristic weight (NewClassification.c sets w1 = minimalWeight()
  // = INT32_MIN for the `initial = ultimate` action; we encode it as
  // an out-of-band bit so reheapify cannot scramble the order).  The
  // flag is OFF by default -- engine byte-identical.
  if ((s->use_initial_ultimate || s->use_database_ultimate)
      && s->cp_ultimate != NULL) {
    u8 ui = s->cp_ultimate[i], uj = s->cp_ultimate[j];
    if (ui != uj) return ui > uj;
    // Both ultimate: WM stamps EVERY Act_ultimate CP w1 =
    // minimalWeight() = INT32_MIN (NewClassification.c:330), so the
    // (w1, w2) heap key degenerates to w2 alone -- pure FIFO within
    // the ultimate class.  The computed cp_pri is deliberately
    // ignored here; for axioms the FIFO stamps carry the canonical
    // loader-sort order (atp_wm_intake_canonicalize).
    if (ui) return s->cp_seq[i] < s->cp_seq[j];
  }
  if (s->cp_pri[i] != s->cp_pri[j]) return s->cp_pri[i] < s->cp_pri[j];
  return s->cp_seq[i] < s->cp_seq[j];
}

// Swap all four parallel CP arrays at slots i, j.  cp_packed swaps the
// pointer only -- the byte string itself does not move, so a
// subsumption-index record still borrows a valid buffer after a sift.
static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr);

static void atp_cp_swap(AtpState *s, u32 i, u32 j) {
  u8  *tc = s->cp_packed[i];s->cp_packed[i]= s->cp_packed[j];s->cp_packed[j]= tc;
  u32  tt = s->cp_trace[i]; s->cp_trace[i] = s->cp_trace[j]; s->cp_trace[j] = tt;
  u32  tp = s->cp_pri[i];   s->cp_pri[i]   = s->cp_pri[j];   s->cp_pri[j]   = tp;
  u32  tq = s->cp_seq[i];   s->cp_seq[i]   = s->cp_seq[j];   s->cp_seq[j]   = tq;
  u32  tg = s->cp_goal[i];  s->cp_goal[i]  = s->cp_goal[j];  s->cp_goal[j]  = tg;
  u32  t2 = s->cp_pri2[i];  s->cp_pri2[i]  = s->cp_pri2[j];  s->cp_pri2[j]  = t2;
  // The IR-normalize cookie is per-CP, not per-slot: a sift moves the
  // CP wholesale, so its NF-witness travels with it.
  u32  tn = s->cp_last_norm_r_revision[i];
  s->cp_last_norm_r_revision[i] = s->cp_last_norm_r_revision[j];
  s->cp_last_norm_r_revision[j] = tn;
  if (s->cp_ultimate != NULL) {
    u8 tu = s->cp_ultimate[i];
    s->cp_ultimate[i] = s->cp_ultimate[j];
    s->cp_ultimate[j] = tu;
  }
  // Deferred-CP descriptor + tag bit travel with the slot: a fresh
  // implicit push sifts up immediately, so missing this swap would
  // desync the descriptor from its slot on the FIRST sift.
  atp_cp_implicit_swap(s, i, j);
#ifdef THVM_ATPFT_CPQ
  // Stage 7: the parallel FT queue indexes identically; swap entries
  // alongside the legacy arrays so heap sift keeps both views in
  // lockstep.
  atp_cp_ft_swap(s, i, j);
#endif
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

// Insert an already-packed CP byte string onto the heap.  Takes
// ownership of `packed` (the queue frees it on pop/drop).  `lhs`/`rhs`
// are the live Terms (for the FV index + goal weight); `cp_nodes` is
// the precomputed node count.  `is_ultimate`=1 forces the CP to the
// heap front (Waldmeister Act_ultimate) -- effective only when
// s->use_initial_ultimate is set; otherwise the bit is recorded but
// atp_cp_before ignores it.  Shared by atp_cp_heap_push (fresh CP)
// and the auto-MaxWeight stash drain (re-admitting a deferred CP).
static void atp_cp_heap_insert_packed(AtpState *s, u8 *packed, u32 cp_nodes,
                                      Term lhs, Term rhs, u32 trace,
                                      u8 is_ultimate) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  // Eager packed slot: make sure no stale deferred-CP tag aliases the
  // packed bytes as a descriptor (every drop/move site clears its bit,
  // so this is a backstop that keeps the invariant local).
  atp_cp_implicit_clear(s, i);
  s->cp_packed[i]= packed;
  s->cp_trace[i] = trace;
  // Fresh CP: no IR-side normalize witness yet -- force the IR sweep to
  // normalize this CP at least once before the cookie can short-circuit.
  s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  s->cp_pri[i]   = atp_cp_priority_sized(s, lhs, rhs, cp_nodes);
  if (s->cp_ultimate != NULL) {
    s->cp_ultimate[i] = is_ultimate;
    if (is_ultimate) s->n_cps_ultimate++;
  }
  s->cp_goal[i]  = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                     ? atp_goal_weight(s, lhs, rhs) : 0u;
#ifdef THVM_ATPFT_CPQ
  // Stage 7: dual-store -- mirror the CP into Arena-A FT cells.  The
  // FT spans live until the slot is freed (pop / drop / destroy).
  atp_cp_ft_set(s, i, lhs, rhs, s->cp_pri[i], /*origin=*/0xffffu);
#endif
  // K-D Heap secondary dimension: compute cp_pri2 with the alternate
  // weight mode (w2_mode, set via thvm_atp_set_w2).  Cheap -- ~one
  // symbol-count walk per CP -- and only filled when w2_modulo > 0.  In
  // GNN coop the GNN owns cp_pri2, so a fresh CP gets the neutral band
  // value until the next re-rank scores it (see ATP_GNN_COOP_NEUTRAL_PRI).
  s->cp_pri2[i]  = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                 : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, lhs, rhs, s->w2_mode)
                 :                        0u;
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  atp_cp_form_trace(seq, cp_nodes, lhs, rhs);
  s->n_cps++;
  atp_cp_sift_up(s, i);
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
  u32 deepest = 0u;
  for (u32 k = 0; k < s->n_rules; k++) {
    u32 w = atp_symbol_count(s->lhs[k]);
    if (w > deepest) deepest = w;
  }
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
                              u32 trace, u8 is_ultimate) {
  if (s->n_cp_stash >= s->cp_stash_cap) {
    u32 ncap = s->cp_stash_cap ? s->cp_stash_cap * 2u : 256u;
    u8 **np  = (u8 **)realloc(s->cp_stash_packed, ncap * sizeof(u8 *));
    u32 *nt  = (u32 *)realloc(s->cp_stash_trace,  ncap * sizeof(u32));
    u32 *nn  = (u32 *)realloc(s->cp_stash_nodes,  ncap * sizeof(u32));
    // Lazy-grow cp_stash_ultimate only when the flag is on; engine
    // byte-identical when off.
    u8  *nu  = s->use_initial_ultimate
                 ? (u8 *)realloc(s->cp_stash_ultimate, ncap * sizeof(u8))
                 : NULL;
    if (np == NULL || nt == NULL || nn == NULL
        || (s->use_initial_ultimate && nu == NULL)) {
      // Allocation failure: rather than leak or lose the CP, admit it
      // directly (slow path, but sound -- never drops a proof CP).
      free(np); free(nt); free(nn); free(nu);
      Term l = 0, r = 0;
      acp_unpack(packed, &l, &r);
      atp_cp_heap_insert_packed(s, packed, cp_nodes, l, r, trace,
                                is_ultimate);
      return;
    }
    s->cp_stash_packed = np; s->cp_stash_trace = nt; s->cp_stash_nodes = nn;
    if (s->use_initial_ultimate) s->cp_stash_ultimate = nu;
    s->cp_stash_cap = ncap;
  }
  s->cp_stash_packed[s->n_cp_stash]  = packed;
  s->cp_stash_trace[s->n_cp_stash]   = trace;
  s->cp_stash_nodes[s->n_cp_stash]   = cp_nodes;
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
                                s->cp_stash_trace[r], ult);
    } else {
      s->cp_stash_packed[w]   = s->cp_stash_packed[r];
      s->cp_stash_trace[w]    = s->cp_stash_trace[r];
      s->cp_stash_nodes[w]    = s->cp_stash_nodes[r];
      if (s->cp_stash_ultimate != NULL) {
        s->cp_stash_ultimate[w] = s->cp_stash_ultimate[r];
      }
      w++;
    }
  }
  s->n_cp_stash = w;
}

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
  // Hard queue-size cap (memory leash): drop before packing (saves the
  // acp_pack malloc) once the live queue is full.  Lossy; the periodic
  // FIFO/priority selection still fires over the kept CPs.
  if (s->max_cp_queue > 0u && s->n_cps >= s->max_cp_queue) {
    s->n_cps_dropped_qcap++;
    return;
  }
  // Pack the CP into a byte string outside the managed heap.
  u32  cp_nodes  = 0u;
  u8  *packed    = acp_pack(lhs, rhs, NULL, &cp_nodes);
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
      return;
    }
    // Auto-MaxWeight (completeness-preserving): defer an over-bound CP to
    // the overflow stash rather than dropping it.  Disabled when base==0.
    // The WM raw>=50 class never defers (see the header comment).
    if (s->auto_max_cp_weight_base > 0u && !raw_untreated) {
      u32 bound = atp_auto_maxw_bound(s);
      if (bound > 0u && cp_nodes > bound) {
        atp_cp_stash_push(s, packed, cp_nodes, trace, is_ultimate);
        return;
      }
    }
  }
  atp_cp_heap_insert_packed(s, packed, cp_nodes, lhs, rhs, trace,
                            is_ultimate);
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
  u32 cp_nodes = atp_symbol_count(lhs) + atp_symbol_count(rhs);
  // Hard queue-size cap (memory leash): same lossy drop as atp_cp_heap_push.
  if (s->max_cp_queue > 0u && s->n_cps >= s->max_cp_queue) {
    s->n_cps_dropped_qcap++;
    return 1u;   // consumed (dropped) -- identical verdict to the MaxWeight cap
  }
  // Waldmeister MaxWeight hard cap: same lossy drop as atp_cp_heap_push.
  if (s->max_cp_weight > 0u && cp_nodes > s->max_cp_weight) return 1u;
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
  s->cp_packed[i] = NULL;          // deferred slot marker
  s->cp_trace[i]  = cp_trace_id;
  s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  s->cp_pri[i]    = atp_cp_priority_sized(s, lhs, rhs, cp_nodes);
  if (s->cp_ultimate != NULL) {
    s->cp_ultimate[i] = is_ultimate;
    if (is_ultimate) s->n_cps_ultimate++;
  }
  s->cp_goal[i]   = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, lhs, rhs) : 0u;
  s->cp_pri2[i]   = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                  : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, lhs, rhs,
                                                              s->w2_mode)
                  :                        0u;
  s->cp_seq[i]    = s->cp_seq_next++;
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
  s->n_cps++;
  s->n_cps_implicit++;
  atp_cp_sift_up(s, i);
  return 1u;
}

// Waldmeister CP-queue interleaving (a port of KPVerwaltung.c's
// `CPdimension`).  Waldmeister keeps the set of unselected equations
// in a K-D heap with TWO keys -- a weight key and a FIFO insertion
// key -- and `CPdimension` returns the FIFO dimension for `thresholdCP`
// of every `moduloCP` selections, the weight dimension otherwise.
//
// CRUCIAL WM fact: the FIFO interleave is OFF in WM's default proof
// configuration -- the one wmcli runs and the alignment matrix compares
// against.  WM's `CPdimension()` (INF/KPVerwaltung.c:582-606) returns
// the FIFO dimension when `AnzAktivierterRE % moduloCP < thresholdCP`,
// and (moduloCP, thresholdCP) are derived from the `-pq interleave=f.h`
// token: `moduloCP = f + h`, `thresholdCP = f` (KPVerwaltung.c:1210).
// WM's default `-pq` is "heap:e1:domain=30:statistics=on"
// (RUN/Parameter.c:210-211) -- it carries NO `interleave=` token, so
// `PI_ParseInterleave` returns FALSE (WASIC/ParseInterleave.c:58-86) and
// the else-branch sets `moduloCP = 1, thresholdCP = 0`
// (KPVerwaltung.c:1216-1219).  With thresholdCP == 0 the test
// `AnzAktivierterRE % 1 < 0` is FALSE at every selection: WM NEVER takes
// a FIFO pick -- the queue is a pure smallest-weight heap.  The analysis
// ratios {1:10,1:50,1:100,1:200} (YFiles.c `Schrittweiten`) only apply
// when the spec or a `-pq interleave=` override requests them, which the
// NotableTheorems corpus does not.
//
// So the faithful WM preset takes NO FIFO pick (thresholdCP 0).  thvm's
// hardcoded threshold 1 fired a spurious FIFO pick at `n_activated_re ==
// 0` (every run) and again at each `n_activated_re == modulo`; on a
// large-rule-set saturation (CombinatorAxioms SKIToBCKW, ~201 activations
// by selection 303) that mid-run FIFO grabbed the oldest stale CP and
// forked the trajectory at selection 303.  On the small-rule-set AC and
// Wolfram rows the only FIFO pick was at activation 0, which happened to
// coincide with the weight root, so they stayed identical either way --
// dropping the pick keeps them identical and advances SKIToBCKW.
//
// The legacy (non-WM) path keeps the raw-selection-count end-of-window
// behaviour the weight-order unit tests pin.
#define ATP_CP_FIFO_MODULO     11u
#define ATP_CP_FIFO_THRESHOLD   1u

// WM `CPdimension()` under the faithful WM preset (use_intake_order):
// thresholdCP is 0 (no `-pq interleave=` in the default config), so the
// FIFO dimension never fires.  Outside the preset thvm keeps the legacy
// raw-selection-count interleave the weight-order unit tests rely on.
static inline u8 atp_cp_fifo_dimension(const AtpState *s) {
  u32 modulo = s->fifo_modulo ? s->fifo_modulo : ATP_CP_FIFO_MODULO;
  if (s->use_intake_order) {
    // thresholdCP == 0 in WM's default `-pq`: FIFO dimension off.  The
    // THVM_ATP_FIFO_THRESHOLD env knob restores a non-zero thresholdCP
    // for callers that request a `-pq interleave=` ratio.
    static int thr = -2;
    if (thr == -2) {
      const char *e = getenv("THVM_ATP_FIFO_THRESHOLD");
      thr = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 0;
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
  for (u32 i = 0; i < n; i++) scratch[i] = s->cp_pri[i];
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
    if (s->cp_pri[i] <= horizon) continue;
    free(s->cp_packed[i]);
    s->cp_packed[i] = NULL;
    atp_cp_implicit_clear(s, i);
#ifdef THVM_ATPFT_CPQ
    atp_cp_ft_clear(s, i);
#endif
#ifdef ATP_FV_INDEX
    // No-op for deferred slots: their seq was never inserted.
    atp_fv_index_remove(s->fv_index, s->cp_seq[i]);
#endif
    u32 last = s->n_cps - 1u;
    if (i != last) {
      s->cp_packed[i] = s->cp_packed[last];
      s->cp_packed[last] = NULL;
      s->cp_trace[i] = s->cp_trace[last];
      s->cp_pri[i]   = s->cp_pri[last];
      s->cp_seq[i]   = s->cp_seq[last];
      s->cp_goal[i]  = s->cp_goal[last];
      s->cp_pri2[i]  = s->cp_pri2[last];
      s->cp_last_norm_r_revision[i] = s->cp_last_norm_r_revision[last];
      if (s->cp_ultimate != NULL) s->cp_ultimate[i] = s->cp_ultimate[last];
      atp_cp_implicit_move(s, /*dst=*/i, /*src=*/last);
#ifdef THVM_ATPFT_CPQ
      atp_cp_ft_move(s, /*dst=*/i, /*src=*/last);
#endif
    }
    s->n_cps--;
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
    for (u32 k = s->n_cps / 2u; k > 0u; ) {
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
  *out_lhs = term_ctr_at(te, 2);
  *out_rhs = term_ctr_at(te, 3);
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
    atp_cp_implicit_materialize(s, i, lhs, rhs);
  } else {
    acp_unpack(s->cp_packed[i], lhs, rhs);
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
  if (s->w2_modulo > 0u &&
      (s->cp_select_count % s->w2_modulo) == 0u) {
    // K-D Heap secondary dimension (WM CPdimension d=1): every
    // w2_modulo-th selection picks the min-cp_pri2 entry instead of
    // the primary heap root.  O(n_cps) scan; the period gates cost.
    // Surfaces structurally simple CPs buried under primary weight.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      if (s->cp_pri2[i] < s->cp_pri2[best]
          || (s->cp_pri2[i] == s->cp_pri2[best] && s->cp_seq[i] < s->cp_seq[best])) {
        best = i;
      }
    }
    j = best;
    s->n_cps_w2_picks++;
  } else if (s->use_goal_interleave > 0u &&
      (s->cp_select_count % s->use_goal_interleave) == 0u) {
    // Goal-directed pick: the most goal-relevant queued CP (min
    // cp_goal).  E-style ratio -- the other picks are weight-based,
    // building the system; this steers toward the goal.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      if (s->cp_goal[i] < s->cp_goal[best]) best = i;
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
    // FIFO dimension: the oldest queued CP is the lowest cp_seq.
    // O(n_cps) scan, but only 1 call in `fifo_modulo` takes this branch.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      if (s->cp_seq[i] < s->cp_seq[best]) best = i;
    }
    j = best;
  }

  // Lazy orphan check.  If the chosen CP descends from an interreduced-
  // away rule, extract+discard it without counting a selection and
  // re-pick.  WM IncAnzKPEntfernt ticks per discard (KPVerwaltung.c:539).
  int orphan = s->use_orphan_murder && atp_cp_is_orphan(s, s->cp_trace[j]);
  if (!orphan) s->cp_select_count++;

  // Unpack the chosen CP from its byte string into two fresh heap
  // Terms for the caller to normalize.  An implicit slot has no byte
  // string -- it materializes the raw pair off its TRACE_CP entry
  // instead (zero-copy; see atp_cp_implicit_materialize for the
  // liveness/aliasing argument).  The orphan check above already ran
  // on cp_trace[j], which the push side sets for implicit slots too,
  // so a dead-parent implicit CP is discarded below exactly like an
  // eager one (WM selectNonOrphan covers the implicit passive set).
  atp_cp_slot_read(s, j, lhs_out, rhs_out);
  s->last_popped_trace = s->cp_trace[j];
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
    static int hd_at = -2;
    if (hd_at == -2) { const char *e = getenv("THVM_ATP_HEAPDUMP_AT"); hd_at = e ? atoi(e) : -1; }
    if (hd_at >= 0 && (u32)hd_at == s->cp_select_count) {
      u32 band = s->cp_pri[j], n_band = 0;
      fprintf(stderr, "==THVMHEAP pick=%u band_pri=%u n_cps=%u==\n", s->cp_select_count, band, s->n_cps);
      for (u32 q = 0; q < s->n_cps; q++) {
        if (s->cp_pri[q] != band) continue;
        Term ql = 0, qr = 0;
        atp_cp_slot_read(s, q, &ql, &qr);
        fprintf(stderr, "  HCP seq=%u pri=%u lhs=", s->cp_seq[q], s->cp_pri[q]);
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
    if (cp_pick_trace) {
      fprintf(stderr, "CPSEL pick=%u j=%u seq=%u pri=%u rules=%u lhs=",
              s->cp_select_count, j, s->cp_seq[j], s->cp_pri[j], s->n_rules);
      atp_dbg_print_term(stderr, *lhs_out);
      fputs(" rhs=", stderr);
      atp_dbg_print_term(stderr, *rhs_out);
      if (orphan) {
        Term te = s->trace[s->cp_trace[j]];
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
  atp_fv_index_remove(s->fv_index, s->cp_seq[j]);
#endif
  free(s->cp_packed[j]);
  s->cp_packed[j] = NULL;
  atp_cp_implicit_clear(s, j);
#ifdef THVM_ATPFT_CPQ
  // Stage 7: the popped slot's FT spans return to Arena A's free list
  // BEFORE backfill -- the slot is about to be overwritten with the
  // last slot's entry (still owned).  No-op when the entry was never
  // populated (NULL slots are idempotent on clear).
  atp_cp_ft_clear(s, j);
#endif
  s->n_cps--;
  if (j != s->n_cps) {
    // Backfill slot j from the (ex-)last slot, then repair the heap.
    u32 last = s->n_cps;
    s->cp_packed[j]    = s->cp_packed[last];
    s->cp_packed[last] = NULL;          // vacated slot: leave it empty
    s->cp_trace[j] = s->cp_trace[last];
    s->cp_pri[j]   = s->cp_pri[last];
    s->cp_seq[j]   = s->cp_seq[last];
    s->cp_goal[j]  = s->cp_goal[last];
    s->cp_pri2[j]  = s->cp_pri2[last];
    s->cp_last_norm_r_revision[j] = s->cp_last_norm_r_revision[last];
    if (s->cp_ultimate != NULL) s->cp_ultimate[j] = s->cp_ultimate[last];
    atp_cp_implicit_move(s, /*dst=*/j, /*src=*/last);
#ifdef THVM_ATPFT_CPQ
    // Move the (still-owned) FT entry from the last slot into j; zero
    // the now-vacated tail so a later destroy / clear does not
    // double-free the transferred spans.
    atp_cp_ft_move(s, /*dst=*/j, /*src=*/last);
#endif
    atp_cp_sift_up(s, j);
    atp_cp_sift_down(s, j);
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
    if (atp_cp_slot_implicit(s, i)) {
      // Deferred slot: no packed bytes to unpack.  Reuse the push-time
      // cached priority (this is what the descriptor caches it for);
      // cp_goal / cp_pri2 keep their push-time values, which travel
      // with every slot move.
      s->cp_pri[i] = s->cp_implicit[i].priority;
      continue;
    }
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[i], &l, &r);
    s->cp_pri[i] = atp_cp_priority(s, l, r);
    s->cp_goal[i] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, l, r) : 0u;
    s->cp_pri2[i] = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                  : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, l, r, s->w2_mode)
                  :                        0u;
    // cp_seq is NOT touched: insertion age is stamped once (heap push /
    // thvm_atp_cp_set) and survives every reweight, matching WM's
    // C_ReClassify which only recomputes w1 ("w2 wird nicht geaendert"
    // / w2 is not changed, CLAS/NewClassification.c:399-406).
  }
}

static void atp_cp_floyd_only(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  // Floyd build-heap: sift down every internal node, last to first.
  for (u32 i = s->n_cps / 2; i > 0; ) {
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
// w-slot commit site the IR sweep already has (l, r) in scope (either
// the pre-normalize (ol, orr) on the fast-path skip branches or the
// post-normalize (l, r) on the reweight branch).  Inlined here so the
// post-loop reheapify only has to do the FULL-array work (Floyd + FV).
// Soundness: the three weight functions are pure in (s, l, r) + state
// fields immutable during the IR sweep (cp_weight_mode, w2_*, gnn_coop,
// use_*, goal_*, sym_level[], conj_sym_mask, s->lhs/rhs); evaluating at
// slot w inside the loop yields bit-identical values to evaluating
// post-loop in atp_cp_rebuild_priorities.
static inline void atp_cp_commit_priorities(AtpState *s, u32 w,
                                            Term l, Term r) {
  s->cp_pri[w]  = atp_cp_priority(s, l, r);
  s->cp_goal[w] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                    ? atp_goal_weight(s, l, r) : 0u;
  s->cp_pri2[w] = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, l, r, s->w2_mode)
                :                        0u;
}

fn void thvm_atp_cp_reheapify(AtpState *s) {
  atp_cp_rebuild_priorities(s);
  atp_cp_floyd_only(s);
}

// === Live CP-queue re-rank seam (WL-side GNN scorer) ================
// The CP heap is a compact array: slots [0, n_cps) all hold a live
// packed CP (a pop backfills the vacated slot from the tail), so there
// are no holes and the live count is exactly s->n_cps.

fn u32 thvm_atp_queued_cp_count(const AtpState *s) {
  if (s == NULL) return 0u;
  return s->n_cps;
}

// Snapshot the live queue: read each slot's CP (packed slots unpack
// fresh transient heap Terms; implicit slots alias their trace-resident
// raw pair) and copy lhs/rhs/seq into the caller's arrays (up to
// `cap`).  Any out pointer may be NULL to skip that column.
// Pure read -- no engine state is mutated.
fn u32 thvm_atp_queued_cps(const AtpState *s, Term *lhs_out, Term *rhs_out,
                           u32 *seq_out, u32 cap) {
  if (s == NULL) return 0u;
  u32 n = s->n_cps < cap ? s->n_cps : cap;
  for (u32 i = 0; i < n; i++) {
    Term l = 0, r = 0;
    atp_cp_slot_read(s, i, &l, &r);
    if (lhs_out != NULL) lhs_out[i] = l;
    if (rhs_out != NULL) rhs_out[i] = r;
    if (seq_out != NULL) seq_out[i] = s->cp_seq[i];
  }
  return n;
}

// Re-key the live queue by cp_seq, then rebuild the heap.  For each
// (seq[j], pri[j]) the live slot whose cp_seq == seq[j] gets cp_pri =
// pri[j]; unknown / already-popped seqs are ignored.  After all keys
// are applied the heap is rebuilt with the same Floyd build-heap loop
// thvm_atp_cp_reheapify uses (atp_cp_sift_down over every internal node,
// last to first), so the new priorities take effect on the next select.
//
// Soundness: this only PERMUTES selection order.  No CP is added or
// dropped and cp_seq is left untouched, so (a) completeness is
// unaffected -- the periodic FIFO pick + auto-MaxWeight force-drain
// still surface every CP eventually -- and (b) the FV-index records
// (which borrow cp_packed[] and key on cp_seq, never on slot position)
// stay valid through the slot swaps, so no index rebuild is required.
//
// seq->slot resolution is an O(n + m) pass: build a seq->slot open-
// addressing map over the n live slots once, then do m O(1) lookups,
// avoiding the O(n*m) scan the queue can grow large enough (~64k) to
// punish.
fn void thvm_atp_set_cp_pri_by_seq(AtpState *s, const u32 *seq,
                                   const u32 *pri, u32 n) {
  if (s == NULL || s->n_cps == 0u || n == 0u
      || seq == NULL || pri == NULL) {
    return;
  }
  // Power-of-two table sized >= 2*n_cps for a <50% load factor.
  u32 cap = 16u;
  while (cap < s->n_cps * 2u) cap <<= 1;
  u32 mask = cap - 1u;
  // map[h] packs (seq+1, slot) so 0 marks an empty bucket (cp_seq can
  // be 0; the +1 bias keeps the empty sentinel distinct).
  u32 *map_seq  = (u32 *)calloc(cap, sizeof(u32));
  u32 *map_slot = (u32 *)malloc(cap * sizeof(u32));
  if (map_seq == NULL || map_slot == NULL) {
    // Allocation failure: fall back to an O(n*m) scan rather than skip
    // the re-key (the queue is small here or we would not have failed).
    free(map_seq); free(map_slot);
    for (u32 j = 0; j < n; j++) {
      for (u32 i = 0; i < s->n_cps; i++) {
        if (s->cp_seq[i] == seq[j]) { s->cp_pri[i] = pri[j]; break; }
      }
    }
    for (u32 i = s->n_cps / 2u; i > 0; ) { i--; atp_cp_sift_down(s, i); }
    return;
  }
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 h = (s->cp_seq[i] * 2654435761u) & mask;
    while (map_seq[h] != 0u) h = (h + 1u) & mask;
    map_seq[h]  = s->cp_seq[i] + 1u;
    map_slot[h] = i;
  }
  for (u32 j = 0; j < n; j++) {
    u32 h = (seq[j] * 2654435761u) & mask;
    while (map_seq[h] != 0u) {
      if (map_seq[h] == seq[j] + 1u) { s->cp_pri[map_slot[h]] = pri[j]; break; }
      h = (h + 1u) & mask;
    }
  }
  free(map_seq);
  free(map_slot);
  // Floyd build-heap: sift down every internal node, last to first.
  // cp_seq is preserved, so atp_cp_before's tie-break is unchanged and
  // the FV index (keyed on cp_seq) needs no rebuild.
  for (u32 i = s->n_cps / 2u; i > 0; ) {
    i--;
    atp_cp_sift_down(s, i);
  }
}

// Write the SECONDARY priority cp_pri2[slot] for the CPs named by `seq`.
// Unlike cp_pri (the heap-ordered primary), cp_pri2 is scanned linearly by
// select_cp's w2 coop branch, so there is no heap to rebuild -- just the
// seq -> slot remap.  This is the GNN-coop write path (gnn_coop): the GNN
// drives the coop dimension while the primary heap stays the hand
// heuristic.  Mirrors thvm_atp_set_cp_pri_by_seq's hash map.
fn void thvm_atp_set_cp_pri2_by_seq(AtpState *s, const u32 *seq,
                                    const u32 *pri, u32 n) {
  if (s == NULL || s->n_cps == 0u || n == 0u || seq == NULL || pri == NULL) {
    return;
  }
  u32 cap = 16u;
  while (cap < s->n_cps * 2u) cap <<= 1;
  u32 mask = cap - 1u;
  u32 *map_seq  = (u32 *)calloc(cap, sizeof(u32));
  u32 *map_slot = (u32 *)malloc(cap * sizeof(u32));
  if (map_seq == NULL || map_slot == NULL) {
    free(map_seq); free(map_slot);
    for (u32 j = 0; j < n; j++) {
      for (u32 i = 0; i < s->n_cps; i++) {
        if (s->cp_seq[i] == seq[j]) { s->cp_pri2[i] = pri[j]; break; }
      }
    }
    return;
  }
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 h = (s->cp_seq[i] * 2654435761u) & mask;
    while (map_seq[h] != 0u) h = (h + 1u) & mask;
    map_seq[h]  = s->cp_seq[i] + 1u;
    map_slot[h] = i;
  }
  for (u32 j = 0; j < n; j++) {
    u32 h = (seq[j] * 2654435761u) & mask;
    while (map_seq[h] != 0u) {
      if (map_seq[h] == seq[j] + 1u) { s->cp_pri2[map_slot[h]] = pri[j]; break; }
      h = (h + 1u) & mask;
    }
  }
  free(map_seq);
  free(map_slot);
}

// Measurement-only: walk the live CP queue, reporting min/max/mean
// node count (the acp_pack symbol count; an implicit slot counts its
// raw trace-resident pair, the form selection will materialize) and a
// coarse size histogram into the caller's `bins` array (bins[k] counts
// CPs with node-count in [k*bucket, (k+1)*bucket); the last bin is the
// overflow tail).  `nbins`/`bucket` are caller-chosen.  Pure read; no
// engine state mutated.  Returns the queue length.
fn u32 thvm_atp_cp_size_stats(const AtpState *s, u32 *min_out, u32 *max_out,
                              double *mean_out, u32 *bins, u32 nbins,
                              u32 bucket) {
  if (bins != NULL && nbins > 0) {
    for (u32 k = 0; k < nbins; k++) bins[k] = 0u;
  }
  if (s == NULL || s->n_cps == 0) {
    if (min_out)  *min_out  = 0u;
    if (max_out)  *max_out  = 0u;
    if (mean_out) *mean_out = 0.0;
    return 0u;
  }
  u32 mn = 0xffffffffu, mx = 0u;
  u64 sum = 0u;
  for (u32 i = 0; i < s->n_cps; i++) {
    Term l = 0, r = 0;
    atp_cp_slot_read(s, i, &l, &r);
    u32 nodes = atp_symbol_count(l) + atp_symbol_count(r);
    if (nodes < mn) mn = nodes;
    if (nodes > mx) mx = nodes;
    sum += nodes;
    if (bins != NULL && nbins > 0 && bucket > 0) {
      u32 b = nodes / bucket;
      if (b >= nbins) b = nbins - 1u;
      bins[b]++;
    }
  }
  if (min_out)  *min_out  = mn;
  if (max_out)  *max_out  = mx;
  if (mean_out) *mean_out = (double)sum / (double)s->n_cps;
  return s->n_cps;
}

// Push one rule onto R; the rule array is growable, so this always
// succeeds (returns 1) unless the state pointer is NULL.
//
// 7c: under -DATP_VAR_NORM the rule's variables are canonically
// renumbered before storage (dense [0, k), shared across both
// sides) -- alpha-renaming that keeps every stored variable below
// the REWRITE_MAX_VAR matcher cliff -- and an identical rule
// already in R (both sides `kbo_eq`) is rejected (returns 0,
// nothing stored).  The renumbering makes alpha-equivalent rules
// byte-identical, so the duplicate guard catches the "add the same
// rule 300x" pathology that interreduction's subsumption misses
// while the matcher is dead on out-of-range variables.
// Try to match existing pattern (pat_lhs, pat_rhs) into target (tg_lhs, tg_rhs)
// under a single substitution \sigma so pat_lhs*\sigma = tg_lhs AND
// pat_rhs*\sigma = tg_rhs.  Returns 1 on success.  Used by forward
// subsumption to decide whether an existing rule logically implies a
// proposed new rule.
static u8 atp_rule_subsumes_oriented(Term pat_lhs, Term pat_rhs,
                                     Term tg_lhs, Term tg_rhs) {
  RewriteSubst subst = {{0}};
  if (!thvm_match(pat_lhs, tg_lhs, &subst)) return 0;
  Term applied_rhs = thvm_subst_apply(pat_rhs, &subst);
  return kbo_eq(applied_rhs, tg_rhs);
}

// Unit-equation subsumption: existing rule (l_i, r_i) subsumes
// proposed new rule (lhs, rhs) iff EITHER orientation works.
// Equations are unoriented so we try both pairings.
static u8 atp_rule_subsumes_unit(Term l_i, Term r_i, Term lhs, Term rhs) {
  return atp_rule_subsumes_oriented(l_i, r_i, lhs, rhs)
      || atp_rule_subsumes_oriented(l_i, r_i, rhs, lhs);
}

static u8 atp_push_rule(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
#ifdef ATP_VAR_NORM
  thvm_normalize_vars(&lhs, &rhs);
  for (u32 i = 0; i < s->n_rules; i++) {
    if (kbo_eq(s->lhs[i], lhs) && kbo_eq(s->rhs[i], rhs)) {
      return 0;  // duplicate rule -- already in R
    }
  }
#endif
  // Forward subsumption pruning (Vampire --forward_subsumption analog,
  // unit-only): drop the rule if some already-stored rule generalizes
  // it.  Sound + completeness-preserving: the new equation is a
  // substitution instance of the existing one, so it adds no deductive
  // power that an instance-of-the-existing-rule rewrite step cannot
  // already produce.  Gated by use_fwd_subsume.
  if (s->use_fwd_subsume) {
    for (u32 i = 0; i < s->n_rules; i++) {
      if (atp_rule_subsumes_unit(s->lhs[i], s->rhs[i], lhs, rhs)) {
        s->n_rules_fwd_subsumed++;
        return 0;
      }
    }
  }
  atp_ensure_rule_cap(s, s->n_rules + 1);
  s->lhs[s->n_rules] = lhs;
  s->rhs[s->n_rules] = rhs;
  // Gated rule-add trace (THVM_ATP_RULE_TRACE): slot + S-expr LHS/RHS, so a
  // rule can be matched by term to Waldmeister's rule set (sidesteps WM's
  // ElternNr/w2 counter confusion -- correlate by term, not number).
  {
    static int rt = -1;
    if (rt < 0) rt = (getenv("THVM_ATP_RULE_TRACE") != NULL) ? 1 : 0;
    if (rt) {
      fprintf(stderr, "RULEADD slot=%u lhs=", s->n_rules);
      atp_dbg_print_term(stderr, lhs);
      fprintf(stderr, " rhs=");
      atp_dbg_print_term(stderr, rhs);
      fputc('\n', stderr);
    }
  }
  // A compaction may have vacated this slot with a stale GJ status;
  // every fresh fact starts untested (WM: a fresh termpair object).
  s->r_gj_status[s->n_rules] = ATP_GJ_ST_UNKNOWN;
  s->r_overlap_done[s->n_rules] = 0u;
#ifdef THVM_ATPFT_RULES
  // Stage 4: mirror the Term pair into the parallel AtpFt slots
  // immediately so every later observer sees the two paths in lockstep
  // (slot[i] occupancy on the ft side is exactly slot[i] occupancy on
  // the Term side).  Persistent allocation (scratch=0) so the cells
  // live as long as the rule slot does -- ft_destroy at thvm_atp_free
  // releases them all.  thvm_normalize_vars already settled the
  // variable canonicalization above, so the ft image is built from the
  // canonical form.  VERIFY probe runs after both writes are committed.
  s->lhs_ft[s->n_rules] = ft_from_term((AtpFt *)s->ft_arena_ptr, lhs, 0);
  s->rhs_ft[s->n_rules] = ft_from_term((AtpFt *)s->ft_arena_ptr, rhs, 0);
  atp_ft_rules_verify_push(s, s->n_rules);
#endif
  // Remove-and-re-derive detection (flat-subsume only).  If this fresh
  // fact has the exact shape of an equation that E-set subsumption
  // removed, it is a RE-DERIVATION: resolve the subsumer's current birth
  // trace as the cutoff so atp_overlap_ij can suppress the root overlaps
  // WM already owned in the subsumer's original batch.  Default NONE =
  // not a re-derivation (ordinary fresh fact).  Scoped to
  // use_flat_subsume so non-flat-subsume runs never pay the scan.
  s->r_rederive_cut[s->n_rules] = ATP_TRACE_NONE;
  if (s->use_flat_subsume) {
    for (u32 k = 0; k < s->n_rules; k++) {
      if (!s->r_dead[k]) continue;
      if (s->r_dead_subsumer_lhs[k] == 0) continue;
      if (!kbo_eq(s->r_dead_lhs_save[k], lhs) ||
          !kbo_eq(s->r_dead_rhs_save[k], rhs))
        continue;
      // Resolve the cutoff: the live rule whose shape equals the saved
      // subsumer, read its now-final r_trace.  Match either orientation
      // (the subsumer is unorientable, stored in one face).
      Term sub_l = s->r_dead_subsumer_lhs[k];
      Term sub_r = s->r_dead_subsumer_rhs[k];
      u32 cutoff = ATP_TRACE_NONE;
      for (u32 m = 0; m < s->n_rules; m++) {
        if (s->r_dead[m]) continue;
        if ((kbo_eq(s->lhs[m], sub_l) && kbo_eq(s->rhs[m], sub_r)) ||
            (kbo_eq(s->lhs[m], sub_r) && kbo_eq(s->rhs[m], sub_l))) {
          cutoff = s->r_trace[m];
          break;
        }
      }
      s->r_rederive_cut[s->n_rules] = cutoff;
      if (atp_rule_trace_on()) {
        fprintf(stderr,
                "  REDERIVE slot %u from dead slot %u cutoff=%u\n",
                s->n_rules, k, cutoff);
      }
      break;
    }
  }
  // Cache the rule's orientation once -- atp_ordered_try_top reads this
  // instead of recomputing a full KBO compare per rewrite position.
  s->r_orient[s->n_rules] = (u8)(atp_compare(s, lhs, rhs) == KBO_GT);
  if (!s->r_orient[s->n_rules]) s->n_unorient++;
  // KBO self-consistency probe (THVM_ATP_ORIENT_KBO_CHECK=1): if we
  // oriented this fact (r_orient==1) yet the memo-free Baader-Nipkow
  // oracle thvm_kbo_naive does NOT agree it is strictly GT, that is an
  // orient-despite-incomparable bug (the fact must stay an unoriented
  // bidirectional E-equation for unfailing completion).  Pure thvm-
  // internal; needs no WM data.
  {
    static int okc = -1;
    if (okc < 0) okc = (getenv("THVM_ATP_ORIENT_KBO_CHECK") != NULL) ? 1 : 0;
    if (okc && s->r_orient[s->n_rules]) {
      KboCmp ref = thvm_kbo_naive(lhs, rhs, s->kbo);
      if (ref != KBO_GT) {
        char la[2048], ra[2048];
        atp_pretty_term(lhs, la, sizeof la);
        atp_pretty_term(rhs, ra, sizeof ra);
        fprintf(stderr,
                "ORIENT_KBO_INCONSISTENT slot=%u naive=%d (lhs > rhs?)\n"
                "  lhs=%s\n  rhs=%s\n", s->n_rules, (int)ref, la, ra);
      }
    }
  }
  s->n_rules++;
  // Rule-set mutated -- bump the monotone revision the IR-normalize
  // cookie keys on (see AtpState.cp_last_norm_r_revision).
  s->r_revision++;
  // Env-gated derivation trace.  Prints each rule at orientation time
  // in derivation order, mirroring Waldmeister's `-a 4` "... added as
  // new rule N:" output.
  if (atp_rule_trace_on()) {
    char la[2048], ra[2048];
    atp_pretty_term(lhs, la, sizeof la);
    atp_pretty_term(rhs, ra, sizeof ra);
    fprintf(stderr, "RULE %u (trace %u): %s -> %s%s\n", s->n_rules - 1u,
            s->r_trace[s->n_rules - 1u], la, ra,
            s->r_orient[s->n_rules - 1u] ? "" : "  (unorientable)");
  }
  // Backward subsumption (Vampire bs=unit_only analog).  When
  // use_bwd_subsume is set, scan rules 0..N-1 and soft-delete any rule
  // subsumed by the newly-added rule at slot N = s->n_rules - 1.  The
  // sentinel-LHS (TAG_FVR with id 255 -- out of REWRITE_MAX_VAR == 64)
  // makes thvm_match and thvm_unify return 0 for the slot, so every
  // rewrite / CP-generation path naturally skips dead rules without
  // touching the 14+ rule-iteration sites.  Originals are saved in
  // r_dead_*_save[] so proof reconstruction (which cites rule indices
  // that may have been killed mid-search) can still read them.
  if (s->use_bwd_subsume) {
    u32 new_i = s->n_rules - 1u;
    Term new_lhs = s->lhs[new_i];
    Term new_rhs = s->rhs[new_i];
    Term dead_sentinel = term_new(0, TAG_FVR, 255u, 0);
    for (u32 i = 0; i < new_i; i++) {
      if (s->r_dead[i]) continue;
      if (atp_rule_subsumes_unit(new_lhs, new_rhs, s->lhs[i], s->rhs[i])) {
        s->r_dead_lhs_save[i] = s->lhs[i];
        s->r_dead_rhs_save[i] = s->rhs[i];
        s->lhs[i] = dead_sentinel;
        s->rhs[i] = dead_sentinel;
        s->r_dead[i] = 1;
        // Soft-deleted rule -- the active R changed, so the IR-normalize
        // cookie keyed on r_revision must invalidate.
        s->r_revision++;
        s->n_rules_bwd_subsumed++;
        // WM order mirror: the subsumed fact leaves the tree.
        if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_trace[i]);
#ifdef THVM_ATPFT_RULES
        // Stage 4: mirror the slot-save on the AtpFt side.  The dead
        // sentinel is a TAG_FVR with var id 255 (out of range); convert
        // it the same way ft_from_term handles any FVR so a later
        // sentinel-check on the ft path matches the Term path.
        s->r_dead_lhs_save_ft[i] = s->lhs_ft[i];
        s->r_dead_rhs_save_ft[i] = s->rhs_ft[i];
        s->lhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                    dead_sentinel, 0);
        s->rhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                    dead_sentinel, 0);
#endif
      }
    }
  }
#ifdef ATP_RULE_INDEX
  // 7e lever 2: R grew -- the rule-LHS index no longer reflects it.
  s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
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

fn void thvm_atp_set_record_norm_steps(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_norm_steps = on ? 1u : 0u;
}

// Toggle interreduction right-reduction (RHS composition).  On by
// default (see thvm_atp_init); set 0 to recover left-reduction-only
// interreduction for A/B measurement or proof-extraction fallback.
fn void thvm_atp_set_right_reduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->right_reduce = on ? 1u : 0u;
}

// Toggle periodic critical-pair-set interreduction (a port of
// Waldmeister KPV_KPMengeInterreduzieren, KPVerwaltung.c:1032, whose
// per-CP AP_generic callback re-normalizes / joinable-deletes / reweights
// each queued CP).  Default OFF; flipped on by Method->"Waldmeister".
fn void thvm_atp_set_cp_set_interreduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->cp_set_interreduce = on ? 1u : 0u;
}

// Toggle lazy orphan murder (Waldmeister "Waisenmord",
// KPVerwaltung.c:535 selectNonOrphan + the per-rule `lebtNoch` bit).
// When a rule is interreduced away its descendant queued CPs are
// redundant; with this on they are discarded FOR FREE at pop time
// (no queue sweep / reheapify).  Default OFF -> byte-identical engine.
fn void thvm_atp_set_use_orphan_murder(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_orphan_murder = on ? 1u : 0u;
}

// Toggle the eager interreduce-time orphan sweep (the ATP_ORPHAN_KILL
// pass at the tail of thvm_atp_interreduce).  Default ON -- the
// historical thvm behavior.  WM has no such sweep: its only orphan
// mechanism is the lazy at-pop discard (-ocrit, selectNonOrphan), so
// Method->"Waldmeister" turns this OFF + use_orphan_murder ON to match
// WM's live-queue composition.  No-op unless built -DATP_ORPHAN_KILL.
fn void thvm_atp_set_use_eager_orphan_sweep(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_eager_orphan_sweep = on ? 1u : 0u;
}

// Toggle the deferred-CP (`implicit_pair`) path.  With the flag on,
// atp_push_cps_traced queues rule-x-rule CPs as 20-byte trace-backed
// descriptors (atp_cp_implicit_push) instead of packed byte strings,
// and selection materializes the raw pair off the slot's TRACE_CP
// entry (atp_cp_implicit_materialize).  Lazy allocation of
// `cp_implicit` / `cp_is_implicit` happens at first push, not here, so
// toggling at runtime stays cheap.  Default OFF -> engine byte-identical.
fn void thvm_atp_set_use_implicit_cp(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_implicit_cp = on ? 1u : 0u;
}

fn void thvm_atp_set_use_perm_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_perm_subsume = on ? 1u : 0u;
}

// Waldmeister history-driven Act_ultimate (NewClassification.c:314, the
// `initial = ultimate` DEF action).  When on, CPs enqueued as input
// axioms (TRACE_AXIOM, no parent) rank strictly before every derived
// CP regardless of heuristic weight -- they ALWAYS pop first until
// exhausted, exactly mirroring WM's w1 = minimalWeight() = INT32_MIN.
// Off by default; engine byte-identical when the flag is off.
fn void thvm_atp_set_use_initial_ultimate(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_initial_ultimate = on ? 1u : 0u;
  // Lazy-allocate cp_ultimate on first enable.  Initialise every
  // existing slot to 0 so already-queued CPs default to non-ultimate;
  // only axioms enqueued AFTER the flag is set rank ultimate.  Off-
  // by-default keeps the engine byte-identical when the flag is
  // never toggled.
  if (on && s->cp_ultimate == NULL && s->cp_cap > 0u) {
    s->cp_ultimate = (u8 *)calloc(s->cp_cap, sizeof(u8));
    if (s->cp_ultimate == NULL) {
      fprintf(stderr, "thvm_atp_set_use_initial_ultimate: calloc failed\n");
      exit(1);
    }
  }
}

fn void thvm_atp_set_use_rule_subsume_drop(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_rule_subsume_drop = on ? 1u : 0u;
}

// Waldmeister loader-level axiom canonicalization + intake semantics
// (src/atp/wm_intake.c: SpezNormierung canonical sort of the initial
// equation set + the Initial = Act_ultimate MIN_INT/FIFO stamp).  The
// flush runs once, at the first thvm_atp_step call, so both intake
// surfaces (bench .pr loader, WL TFindProof encoder) canonicalize
// identically.  Off by default; engine byte-identical when off.
fn void thvm_atp_set_use_intake_order(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_intake_order = on ? 1u : 0u;
}

// Waldmeister `database=ultimate` (NewClassification.c:711, Parameter.c
// default of `initial=ultimate:database=ultimate`).  When on, derived
// CPs from rule-database overlap (atp_push_cps_traced) ALSO rank
// ultimate -- the depth-first bias that lets WM crack wolfram
// commutativity in 2.5s.  The atp_cp_before comparator already inspects
// cp_ultimate iff use_initial_ultimate is set, so this flag composes
// with INITIAL_ULTIMATE -- enable both for full WM-default parity.
// Off = engine byte-identical.
fn void thvm_atp_set_use_database_ultimate(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_database_ultimate = on ? 1u : 0u;
  // Lazy-allocate cp_ultimate on first enable (mirrors the INITIAL
  // setter -- the comparator dereferences cp_ultimate iff non-NULL).
  if (on && s->cp_ultimate == NULL && s->cp_cap > 0u) {
    s->cp_ultimate = (u8 *)calloc(s->cp_cap, sizeof(u8));
    if (s->cp_ultimate == NULL) {
      fprintf(stderr, "thvm_atp_set_use_database_ultimate: calloc failed\n");
      exit(1);
    }
  }
}

fn void thvm_atp_set_w2(AtpState *s, u32 modulo, u8 mode) {
  if (s == NULL) return;
  s->w2_modulo = modulo;
  s->w2_mode   = (mode < ATP_CP_WEIGHT_LAST) ? mode : ATP_CP_WEIGHT_MAX;
}

fn void thvm_atp_set_use_unorient_index(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_unorient_index = on ? 1u : 0u;
}

fn void thvm_atp_set_use_lazy_normalize(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_lazy_normalize = on ? 1u : 0u;
}

// Waldmeister `RechtsUnfreiErzeugen` FVI rule emission (RUndEVerwaltung.c:
// 366-397).  When ON, an unorientable equation also generates a
// grounded sibling rule substituting ATP_RESERVED_LABEL_MIN_CONST for
// every free RHS variable absent from the LHS.  Default OFF; turning
// it on is required to crack ExcludedMiddle / Noncontradiction /
// EqualityOfInverses under Method->"Waldmeister".
fn void thvm_atp_set_use_fvi(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_fvi = on ? 1u : 0u;
}

// Vampire-style Limited Resource Strategy (Riazanov & Voronkov, JSC 36,
// 2003).  See AtpState.use_lrs / thvm_atp_select_cp / atp_lrs_recompute_
// horizon for the algorithm.  Resets the per-run horizon state so a
// fresh saturation starts in warmup (no horizon yet); leaves the start
// timestamp NULL so the first selection latches it.
fn void thvm_atp_set_use_lrs(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_lrs = on ? 1u : 0u;
  s->lrs_start_us = 0u;
  s->lrs_last_recompute_at = 0u;
  s->lrs_horizon = 0u;
}

// Walk a Term collecting its CTR symbol-label bits into mask[].
// goal_sym_mask is a fixed-size bit-set of 8 * 32 = 256 labels (the
// most common range); larger labels mod into the same buckets, so the
// "touches goal" check is a sound over-approximation -- it never rules
// out a CP that genuinely shares a symbol with the goal.
static void atp_sym_mask_collect(Term t, u32 mask[8]) {
  if (term_tag(t) == TAG_CTR) {
    u32 lbl = term_ext(t);
    mask[(lbl >> 5) & 7u] |= 1u << (lbl & 31u);
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return;
    for (u32 i = 0; i < n; i++) {
      atp_sym_mask_collect(term_ctr_at(t, i), mask);
    }
  }
}

static int atp_term_touches_goal(const AtpState *s, Term t) {
  if (s == NULL || !s->use_sos) return 0;
  if (term_tag(t) == TAG_CTR) {
    u32 lbl = term_ext(t);
    if (s->goal_sym_mask[(lbl >> 5) & 7u] & (1u << (lbl & 31u))) return 1;
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return 0;
    for (u32 i = 0; i < n; i++) {
      if (atp_term_touches_goal(s, term_ctr_at(t, i))) return 1;
    }
  }
  return 0;
}

// Set-of-Support: a CP whose sides share symbols with the goal gets a
// priority bonus (smaller cp_pri = earlier in the min-heap), nudging
// the saturator to explore goal-relevant CPs first.  Sound -- only the
// heap ordering changes; no CP is dropped.  Mirrors Vampire's `--sos`
// and E-prover's `-S sos` in spirit; the equational-completion variant
// preserves completeness because we still process all CPs eventually.
fn void thvm_atp_set_use_sos(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_sos = on ? 1u : 0u;
  if (on) {
    for (u32 i = 0; i < 8u; i++) s->goal_sym_mask[i] = 0u;
    if (s->n_goals > 1u) {
      // Multi-goal: the support set is the union over every conjunct's
      // symbols, so all goals keep their relevance bias run-wide.
      for (u32 g = 0; g < s->n_goals; g++) {
        atp_sym_mask_collect(s->goals_lhs[g], s->goal_sym_mask);
        atp_sym_mask_collect(s->goals_rhs[g], s->goal_sym_mask);
      }
    } else {
      if (s->goal_lhs) atp_sym_mask_collect(s->goal_lhs, s->goal_sym_mask);
      if (s->goal_rhs) atp_sym_mask_collect(s->goal_rhs, s->goal_sym_mask);
    }
  }
}

// Forward subsumption: when adding a new rule, drop it if some existing
// rule already subsumes it.  Vampire's --forward_subsumption analog,
// unit-only.  Sound + completeness-preserving; default off.
fn void thvm_atp_set_use_fwd_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_fwd_subsume = on ? 1u : 0u;
}

// Backward subsumption: when adding a new rule, soft-delete any
// existing rule subsumed by it.  Vampire's bs=unit_only analog.
// Sound + completeness-preserving; default off.
fn void thvm_atp_set_use_bwd_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_bwd_subsume = on ? 1u : 0u;
}

// Backward demodulation (LHS half): after the new-batch interreduce,
// also normalize each older rule's LHS against the new rule(s); if it
// reduces, re-queue the simplified equation (reduced_lhs, old_rhs).
// Vampire's bd=all analog.  Sound + completeness-preserving (the
// rewritten equation is a logical consequence of the original).
// Default off.
fn void thvm_atp_set_use_bwd_demod(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_bwd_demod = on ? 1u : 0u;
}

// Waldmeister-faithful IR-victim demotion (KPV_IROpferBehandeln /
// IR_PufferAuslesen; see AtpState.use_wm_demote in thvm.h).
fn void thvm_atp_set_use_wm_demote(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_wm_demote = on ? 1u : 0u;
}

// Overlap-exhausted-equation gate (see AtpState.use_overlap_exhaust): a
// newly-derived commutativity overlaps an equation's fresh re-derivation,
// not the stale exhausted original.  WM-faithful; ON in the WL Waldmeister
// preset (option "OverlapExhaust").
fn void thvm_atp_set_use_overlap_exhaust(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_overlap_exhaust = on ? 1u : 0u;
}

// WM -ks "s" pop-time E-subsumption drop (KPV_Select branch,
// INF/KPVerwaltung.c:667; see AtpState.use_pop_subsume in thvm.h).
fn void thvm_atp_set_use_pop_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_pop_subsume = on ? 1u : 0u;
}

// WM E-set subsumption destroy on new-equation entry
// (GMSubsummierenMitGleichung, INF/Interreduktion.c:251-274; see
// AtpState.use_eset_subsume in thvm.h).
// WM backward ground-joinability sterilization, -gj
// (RueckwaertsGrundzusammenfuehrbarkeit, INF/Hauptkomponenten.c:260-306;
// see AtpState.use_bwd_ground_join in thvm.h).  Default OFF = WM's -gj
// CLI default (RUN/Parameter.c:317).
fn void thvm_atp_set_use_bwd_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_bwd_ground_join = on ? 1u : 0u;
}

// WM -einsstern (EinsSternUeberlappung, Unifikation1.c:1039-1055): keep
// only CPs whose overlap position lies on the "1*" leftmost-argument
// spine of the overlapped LHS.  Default OFF = WM's -einsstern default
// (RUN/Parameter.c:250); NOT in the "Waldmeister"* presets.  See
// AtpState.use_einsstern.
fn void thvm_atp_set_use_einsstern(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_einsstern = on ? 1u : 0u;
}

// WM -nusfu (NusfUeberlappung, Unifikation1.c:1082-1090): skip overlap
// positions inside a skolem-function subterm.  Default OFF on the thvm
// Method (so the engine stays byte-identical); with the skolem-label
// registry empty it is a no-op even when ON.  See
// AtpState.use_no_overlap_below_skolem.
fn void thvm_atp_set_use_no_overlap_below_skolem(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_no_overlap_below_skolem = on ? 1u : 0u;
}

fn void thvm_atp_add_skolem_label(AtpState *s, u32 label) {
  if (s == NULL) return;
  for (u32 k = 0; k < s->n_skolem_labels; k++)
    if (s->skolem_labels[k] == label) return;   // already registered
  if (s->n_skolem_labels >= ATP_MAX_SKOLEM_LABELS) return;   // cap
  s->skolem_labels[s->n_skolem_labels++] = label;
}

// WM -reclas (C_ReClassify, CLAS/NewClassification.c:398-430): reweight
// CPs touched by the CP-set IR sweep.  Inert unless CPSetInterreduce is
// also enabled (the sweep is default OFF) and the criteria list is
// non-empty (-reclas default "").  Exposed for coverage; OFF by default.
fn void thvm_atp_set_use_reclassify(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_reclassify = on ? 1u : 0u;
}

// WM -kern (KernUeberlappung, Unifikation1.c:1243-1268): reversed /
// head-stand completion on the combinator-consultation lane.  Vacuous on
// the ground-goal surface.  Exposed for coverage; OFF by default.
fn void thvm_atp_set_use_reversed_completion(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_reversed_completion = on ? 1u : 0u;
}

// WM -sue (SUEM): selects the SUE statistics module only; carries no
// trajectory-affecting parameter.  Exposed for coverage; OFF by default.
fn void thvm_atp_set_use_sue_management(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_sue_management = on ? 1u : 0u;
}

// WM -cg (KPV_CGMengeInterreduzieren, KPVerwaltung.c:835-849): CG-set
// interreduction.  Inert on ground goals (CG heap empty,
// PM_Existenzziele-gated).  Exposed for coverage; OFF by default.
fn void thvm_atp_set_use_critical_goal_interreduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_critical_goal_interreduce = on ? 1u : 0u;
}

// WM -cgclas: classification of critical goals.  Inert on ground goals
// (CG heap empty).  Exposed for coverage; OFF by default.
fn void thvm_atp_set_use_critical_goal_weight(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_critical_goal_weight = on ? 1u : 0u;
}

// WM -back (RueckwartigeUeberlappung, Unifikation1.c:1313): backward-
// argue critical goals.  Existential / CG-paramodulation lane, inert on
// the universal/ground-goal surface.  Exposed for coverage; OFF by
// default.
fn void thvm_atp_set_use_backward_goal_argue(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_backward_goal_argue = on ? 1u : 0u;
}

fn void thvm_atp_set_use_eset_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_eset_subsume = on ? 1u : 0u;
}

fn void thvm_atp_set_use_flat_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_flat_subsume = on ? 1u : 0u;
}

fn void thvm_atp_set_use_comm_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_comm_subsume = on ? 1u : 0u;
}

// Commutativity-DEFER overlap gate (see AtpState.use_comm_defer):
// skip the over-enumerated non-canonical comm-side overlap in an oriented
// rule's birth batch, WITHOUT removing the equation.  DEFAULT OFF.
fn void thvm_atp_set_use_comm_defer(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_comm_defer = on ? 1u : 0u;
}

// Commutativity-REAGE overlap re-rank (see AtpState.use_comm_reage): the
// INVERSE of comm-defer.  Promote thvm's single seq564-sibling CP (rule13 x
// eqn-10) to the head of eqn-10's birth batch so it is selected at WM's
// faithful early age (pick-126) rather than buried at the eTT batch tail
// (pick-135).  DEFAULT OFF.
fn void thvm_atp_set_use_comm_reage(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_comm_reage = on ? 1u : 0u;
}

// Commutativity DROP-DUP re-age (see AtpState.use_comm_drop_dup): re-age the
// single DUPLICATE re-derivation of slot15's term `x.(y.x) = (y.y).x` one FIFO
// slot later (past its in-batch `x.(x.x) = y.(y.y)` successor) so it lands at
// WM's faithful pick-289 instead of pick-288.  Gated on slot15 being a live
// rule (a re-derivation, not a birth).  DEFAULT OFF.
fn void thvm_atp_set_use_comm_drop_dup(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_comm_drop_dup = on ? 1u : 0u;
}

// Inner-swap anchor gate for the DROP-DUP re-age (see
// AtpState.use_comm_drop_dup_class_gate): skip the slot15-term re-age when its
// smallest-keyed successor is a Meredith-harmful anchor WM emits AFTER the
// slot15-term -- the permutation class `(x.y).y = (y.x).y` (rule-51) or the
// slot15-rotate `x.(y.x) = (x.y).x` (rule-59).  DEFAULT OFF.
fn void thvm_atp_set_use_comm_drop_dup_class_gate(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_comm_drop_dup_class_gate = on ? 1u : 0u;
}

// Leaf-arrival tiebreak for the adjacent-leaf oriented-vs-permutation family
// (see AtpState.use_leaf_tiebreak): re-key a var-differ==1 (WM-oriented)
// equation CP that landed exactly one k3 leaf-step above a var-differ==0 (WM
// two-faced permutation) sibling at the SAME overlap geometry, so the
// oriented-scan copy sorts FIRST as WM emits it.  DEFAULT OFF.
fn void thvm_atp_set_use_leaf_tiebreak(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_leaf_tiebreak = on ? 1u : 0u;
}

// Leaf-tiebreak face gate (see AtpState.use_leaf_tiebreak_facegate): skip the
// var-differ==1-first flip when the oriented partner is overlapped on its WM-
// distinguished face but the permutation partner on its WM-reverse face -- a
// configuration where thvm's DFS arrival already matches WM's formation order.
// DEFAULT OFF.
fn void thvm_atp_set_use_leaf_tiebreak_facegate(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_leaf_tiebreak_facegate = on ? 1u : 0u;
}

// Reverse-face shape-group tiebreak (see AtpState.use_revface_group):
// re-key a var-differ==1 (WM-oriented) equation partner's reverse-face CP
// to sort immediately after the largest-keyed same-group CP it alpha-matches
// (same reduced equation), so WM's adjacent same-shape emission is restored.
// DEFAULT OFF.
fn void thvm_atp_set_use_revface_group(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_revface_group = on ? 1u : 0u;
}

// Overlap-position raw-arrival grouping (see AtpState.use_posgroup):
// un-group a vd=0 permutation partner CP that REVFACE re-keyed onto a vd=1
// oriented anchor (restoring raw arrival), and defer a vd=0 permutation
// partner's reverse face past the higher-arrival same-group cluster, so the
// A-phase batch matches WM's raw discrimination-tree arrival order.
// DEFAULT OFF.
fn void thvm_atp_set_use_posgroup(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_posgroup = on ? 1u : 0u;
}

// Cube-arrival tiebreak (see AtpState.use_cube_arrival): re-key the
// double-cube CP `(x.(x.x)).y = (z.(z.z)).y` to sort immediately below its
// same-group slot15-wrapped predecessor `(x.(y.x)).z = ((y.y).x).z`, swapping
// the adjacent A-phase tops pair to WM's `ue (19,-7)` before `ue (19,-2)`
// emission order (soa f=28, weight 224).  DEFAULT OFF.
fn void thvm_atp_set_use_cube_arrival(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_cube_arrival = on ? 1u : 0u;
}

fn void thvm_atp_set_use_band_interleave(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_band_interleave = on ? 1u : 0u;
}

// Waldmeister CP-formation FIFO lineage (see AtpState.use_formation_fifo):
// the SINGLE knob enabling the faithful WM CP-formation order -- it turns on
// the four scoped k3-arrival re-key passes (leaf_tiebreak / revface_group /
// posgroup / cube_arrival) that together reproduce WM's combined-superposition-
// scan emission order (per overlap position, every rule-tree partner in
// discrimination-tree leaf-arrival order precedes every equation-tree partner;
// the surviving copy of a multiply-formed term inherits WM's CPNr age, w2 =
// ++CPNr at insertion, NewClassification.c C_Classify:325 <- recentCPinsert).
// Atop the base CP_SIDE/FLAT_SUBSUME/COMM_REAGE/COMM_DROP_DUP knobs this
// reaches soa firstdiv 1505.  DEFAULT OFF (opt-in only); ON it is exactly
// equivalent to setting the four individual correction flags.
fn void thvm_atp_set_use_formation_fifo(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_formation_fifo = on ? 1u : 0u;
  if (on) {
    s->use_leaf_tiebreak    = 1u;
    s->use_leaf_tiebreak_facegate = 1u;
    s->use_revface_group    = 1u;
    s->use_posgroup         = 1u;
    s->use_cube_arrival     = 1u;
    s->use_band_interleave  = 1u;
    s->use_drain_chainpos   = 1u;
    s->use_drain_revface    = 1u;
    s->use_revface_cubeorder = 1u;
    s->use_mered_dmgu       = 1u;
    s->use_comm_drop_dup_class_gate = 1u;
    // NOTE: use_wm_trie_faithful is NOT auto-enabled here.  The WM-faithful
    // AltesBlattPolieren splice-after construction correctly reproduces WM's
    // discrimination-tree leaf-arrival order for soa's rule-35 tops batch
    // (the firstdiv-1953 divergence: it moves the B-leaf from DFS arrival 6
    // to WM's arrival 8), but STANDALONE it regresses firstdiv 1953 -> 1884
    // by exposing a separate latent divergence at pick-1884 that the historical
    // head-insert special case happened to mask: there the recursive tops DFS
    // over the now-faithful tree orders a partner-face pair (the two faces of
    // `dot(v,dot(dot(v,w),v))`) opposite to WM, swapping their CPNr ages
    // (cp_seq 9012/9013).  Clearing 1884 needs WM's single-cursor backtracking
    // walk (Unifikation1.c RumpfTermMitDSBaumUnifizieren + Delta*) so the leaf
    // visit order matches WM exactly at that pair; the construction fix is the
    // gated, byte-identical-OFF foundation that work builds on.  Enable both
    // together via THVM_ATP_WM_TRIE_FAITHFUL once the cursor walk lands.
    atp_wmo_sync_trie_faithful(s);
  }
}

// WM IR-victim drain within-leaf chain tiebreak (see
// AtpState.use_drain_chainpos): fold a re-derivation victim's chain index
// within its discrimination-tree leaf into the drain-order key, so two
// victims sharing a leaf re-enter the queue in WM's BK_Regeln -> TP_Nachf
// head-first chain order (DSBaumKnoten.h:482-495) rather than thvm's
// slot-scan push order.  DEFAULT OFF.
fn void thvm_atp_set_use_drain_chainpos(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_drain_chainpos = on ? 1u : 0u;
}

// WM GMInterred reducible-face drain order (see AtpState.use_drain_revface):
// rank an IR-victim by the leaf-list position of the face the new rule
// actually reduced (the GMInterred BK_ReferenzDurchlauf pull face), not its
// distinguished face.  DEFAULT OFF; also turned ON under use_formation_fifo.
fn void thvm_atp_set_use_drain_revface(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_drain_revface = on ? 1u : 0u;
}

// Reverse-face cube emission order (see AtpState.use_revface_cubeorder):
// sort a tops-batch's forward-cube CPs `x.(y.(y.y)) = x.x` before its
// reverse-cube CPs `(x.(x.x)).y = y.y` within one A-tops k2=1 group, matching
// WM's forward-before-reverse single-scan emission.  DEFAULT OFF; also turned
// ON under use_formation_fifo.
fn void thvm_atp_set_use_revface_cubeorder(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_revface_cubeorder = on ? 1u : 0u;
}

// Shared-reverse-face double-MGU defer (see AtpState.use_mered_dmgu): in a
// weight-120 tops-A equation-tree band, defer the chain-head (newest-equation)
// combo=0 CP that shares a reverse-face leaf with an older equation's combo=0
// CP -- WM ages that content as the older equation's late second MGU, not at
// the band head.  DEFAULT OFF; also turned ON under use_formation_fifo.
fn void thvm_atp_set_use_mered_dmgu(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_mered_dmgu = on ? 1u : 0u;
}

fn void thvm_atp_set_use_wm_trie_faithful(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_wm_trie_faithful = on ? 1u : 0u;
  atp_wmo_sync_trie_faithful(s);
}

// WM Gleichungsbaum insertion order (lhs face first, rhs face second; see
// AtpState.use_wmo_insert_lr).  Default OFF; opt-in via THVM_ATP_WMO_INSERT_LR.
fn void thvm_atp_set_use_wmo_insert_lr(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_wmo_insert_lr = on ? 1u : 0u;
}

// Push-time queue-vs-queue subsumption gate (no WM counterpart; see
// AtpState.use_queue_subsume in thvm.h).  Default ON = the historical
// thvm engine; the "Waldmeister"* presets turn it OFF.
fn void thvm_atp_set_use_queue_subsume(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_queue_subsume = on ? 1u : 0u;
}

fn void thvm_atp_set_use_mixmost_nf(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_mixmost_nf = on ? 1u : 0u;
}

fn void thvm_atp_set_use_emission_order(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_emission_order = on ? 1u : 0u;
  if (on && s->wmo == NULL) {
    s->wmo = (void *)atp_wmo_new();
    atp_wmo_sync_trie_faithful(s);
    // Register already-live facts in slot order (creation order on the
    // preset path, where the flag is set before the axioms install).
    for (u32 i = 0; i < s->n_rules; i++) {
      if (s->r_dead != NULL && s->r_dead[i]) continue;
      atp_wmo_insert_fact(s, i);
    }
  }
}

// Buffer one interreduction victim's ORIGINAL sides + the TRACE_SIMPLIFY
// parent captured at drop time -- the analog of WM's IR buffer that
// `GMInterred` / `RMLinksInterred` fill per victim (Interreduktion.c:
// 280-326) for `IR_PufferAuslesen` to drain.  Drained here by
// atp_wm_demote_drain after CP generation.
static void atp_irv_push(AtpState *s, Term lhs, Term rhs, u32 parent,
                         u32 wmo_key) {
  if (s->n_irv == s->irv_cap) {
    u32 cap = s->irv_cap ? s->irv_cap * 2u : 8u;
    Term *nl = (Term *)realloc(s->irv_lhs,    cap * sizeof(Term));
    Term *nr = (Term *)realloc(s->irv_rhs,    cap * sizeof(Term));
    u32  *np = (u32  *)realloc(s->irv_parent, cap * sizeof(u32));
    u32  *nk = (u32  *)realloc(s->irv_wmo_key, cap * sizeof(u32));
    if (nl == NULL || nr == NULL || np == NULL || nk == NULL) {
      fprintf(stderr, "atp_irv_push: realloc to %u victims failed\n", cap);
      exit(1);
    }
    s->irv_lhs = nl; s->irv_rhs = nr; s->irv_parent = np;
    s->irv_wmo_key = nk;
    s->irv_cap = cap;
  }
  s->irv_lhs[s->n_irv]     = lhs;
  s->irv_rhs[s->n_irv]     = rhs;
  s->irv_parent[s->n_irv]  = parent;
  s->irv_wmo_key[s->n_irv] = wmo_key;
  s->n_irv++;
}

// Mark a rule's birthing trace id as dead so descendant CPs are skipped
// at pop time.  Grows the bitset on demand (8 trace ids per byte).
static void atp_trace_mark_dead(AtpState *s, u32 trace_id) {
  if (trace_id == ATP_TRACE_NONE) return;
  u32 byte = trace_id >> 3;
  if (byte >= s->r_trace_dead_cap >> 3) {
    u32 new_bits = (trace_id + 1u + 4096u) & ~4095u;  // round up
    u32 new_bytes = (new_bits + 7u) >> 3;
    u8 *grown = (u8 *)realloc(s->r_trace_dead, new_bytes);
    if (grown == NULL) return;  // out of memory: skip the mark (still sound)
    u32 old_bytes = s->r_trace_dead_cap >> 3;
    memset(grown + old_bytes, 0, new_bytes - old_bytes);
    s->r_trace_dead = grown;
    s->r_trace_dead_cap = new_bytes << 3;
  }
  s->r_trace_dead[trace_id >> 3] |= (u8)(1u << (trace_id & 7u));
}

// Is a rule's birthing trace id marked dead?
static int atp_trace_is_dead(const AtpState *s, u32 trace_id) {
  if (trace_id == ATP_TRACE_NONE || trace_id >= s->r_trace_dead_cap) return 0;
  return (s->r_trace_dead[trace_id >> 3] >> (trace_id & 7u)) & 1u;
}

// Is a queued CP an orphan?  Its TRACE_CP entry names its two parent
// rules' trace ids in children 0/1; the CP is an orphan iff either is
// dead.  CPs whose trace entry is not a TRACE_CP (axioms, simplifies)
// are never orphaned.  Mirrors WM selectNonOrphan's
// `aP->lebtNoch && oP->lebtNoch` test (KPVerwaltung.c:540).
static int atp_cp_is_orphan(const AtpState *s, u32 cp_trace) {
  if (cp_trace == ATP_TRACE_NONE || cp_trace >= s->n_trace) return 0;
  Term te = s->trace[cp_trace];
  if (term_tag(te) != TAG_CTR || term_ext(te) != TRACE_CP) return 0;
  u32 pa = (u32)term_val(term_ctr_at(te, 0));
  u32 pb = (u32)term_val(term_ctr_at(te, 1));
  return atp_trace_is_dead(s, pa) || atp_trace_is_dead(s, pb);
}

// Read wall-clock microseconds from CLOCK_REALTIME -- portable
// across linux / macOS / freebsd and good enough for a >=1 second
// deadline budget.
// Host abort hook (see thvm.h): NULL unless a host (e.g. the WL glue)
// installs a poll into Abort[] / TimeConstrained[].
int (*thvm_atp_abort_hook)(void) = NULL;

static u64 atp_now_us(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
  return (u64)ts.tv_sec * 1000000ull + (u64)(ts.tv_nsec / 1000);
}

// Throttled poll (every 256 calls) of the wall deadline + host abort,
// for the inner rewrite loops.  atp_now_us is a clock_gettime syscall,
// so it is gated behind the tick mask to stay off the hot path.  Only
// fires once the deadline has actually passed or a host Abort[] is
// pending, so a normal (non-aborting) normalize is never cut short.
static int atp_norm_deadline_fired(AtpState *s) {
  static u32 tick = 0u;
  if ((++tick & 0xFFu) != 0u) return 0;
  if (s->wall_deadline_us != 0u) {
    u64 now = atp_now_us();
    if (now != 0u && now >= s->wall_deadline_us) return 1;
  }
  if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 1;
  return 0;
}

fn void thvm_atp_set_wall_deadline(AtpState *s, double seconds_from_now) {
  if (s == NULL) return;
  if (seconds_from_now <= 0.0) { s->wall_deadline_us = 0u; return; }
  u64 now = atp_now_us();
  if (now == 0u) return;  // clock_gettime failed; leave deadline off
  s->wall_deadline_us = now + (u64)(seconds_from_now * 1e6);
}

// Forward decl: defined after the proof-extract machinery further
// down; atp_rewrite_normalize_record below reuses it as the one-step
// metadata-recording rewriter.
static Term atp_proof_rewrite_step(AtpState *s, Term t, u8 *pos, u8 depth,
                                   u32 *out_rule, u8 *out_pos_len,
                                   u8 *out_fwd, u8 *fired);

// When set, atp_proof_rewrite_step considers ONLY oriented rules
// (skips unorientable equations) -- the recording analog of WM's
// doE=FALSE normalize leg.  Set around recorder calls that replay a
// doR-only engine phase: the pop normalize's generation-treatment
// replay, the WM-demote drain, and atp_proof_record_side's
// orientable-fixpoint phase.
static u8 g_atp_proof_oriented_only;

// Forward decl: the Waldmeister IR-victim drain (use_wm_demote), defined
// next to thvm_atp_interreduce which fills the buffer it drains.
static void atp_wm_demote_drain(AtpState *s);

#ifdef ATP_CP_GROUND_JOIN
// Forward decls: the WM -gj fact-level ground-joinability halves
// (use_bwd_ground_join), defined with the groundJoin driver.
static void atp_gj_classify_added(AtpState *s, AtpAddedRange added);
static void atp_bwd_ground_join_walk(AtpState *s, u32 skip_lo, u32 skip_hi);
#endif

// Slice-aware companion: same one-step metadata-recording shape as
// atp_proof_rewrite_step but the rule set is a passed-in slice
// (lhs_arr / rhs_arr / n) rather than the live R.  Used by the
// interreduce path so the NORM_STEPs it records cite the (slice-)
// local rule that fired -- the dropped rule's normalization through
// the just-added rules.  `out_rule` is the slice index; the caller
// maps it to a TRACE_ORIENT trace id via its own trace-id array.
static Term atp_proof_rewrite_step_slice(AtpState *s, Term t,
                                         u8 *pos, u8 depth,
                                         const Term *lhs_arr,
                                         const Term *rhs_arr,
                                         u32 n,
                                         u32 *out_rule, u8 *out_pos_len,
                                         u8 *out_fwd, u8 *fired);

// Push a TRACE_NORM_STEP entry recording one rewrite step the CP-
// normalize loop just applied.  Children layout (see thvm.h for the
// schema mirrored by the WL decoder):
//   [NUM(parent_a), NUM(rule_idx), lhs (after step), rhs (after step),
//    NUM(pos_len), NUM(pos_0), ..., NUM(side), NUM(fwd)]
static u32 atp_trace_push_norm_step(AtpState *s, u32 p_a, u32 rule_idx,
                                    Term lhs, Term rhs,
                                    u8 side, u8 fwd,
                                    const u8 *pos, u8 pos_len) {
  if (s == NULL || !atp_trace_ensure(s)) return ATP_TRACE_NONE;
  Term children[7 + ATP_PROOF_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, rule_idx);
  children[2] = lhs;
  children[3] = rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5u + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len]      = term_new(0, TAG_NUM, 0, side);
  children[5u + pos_len + 1u] = term_new(0, TAG_NUM, 0, fwd);
  s->trace[s->n_trace] = term_new_ctr(TRACE_NORM_STEP, children,
                                      7u + pos_len);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
}

// CP-normalize chain recorder: iterate one rewrite step at a time
// via the proof-extracter (atp_proof_rewrite_step), pushing a
// TRACE_NORM_STEP per fire so the WL extractor walks the chain
// linearly.  `eq_other` is the equation's other side (unchanged
// during this side's normalization) -- recorded in each step's
// (lhs, rhs) tuple per `side` (0 lhs / 1 rhs).  Returns the final
// term and updates *chain_tail to the last pushed step's trace
// index (or leaves it at the caller's prev_trace if no step fires).
static Term atp_rewrite_normalize_record(AtpState *s,
                                         Term t, Term eq_other, u8 side,
                                         u32 *chain_tail, u32 step_cap) {
  for (u32 it = 0; it < step_cap; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                     &fwd, &fired);
    if (!fired) break;
    Term step_lhs = (side == 0u) ? t2 : eq_other;
    Term step_rhs = (side == 0u) ? eq_other : t2;
    // Store the rule's TRACE_ORIENT trace index (not mainRules
    // position): mainRules shifts as interreduction drops rules, but
    // each TRACE_ORIENT entry stays at a fixed trace index for the
    // life of the run, so WL can read the rule's stored sides off
    // trace[] regardless of later interreduction.
    u32 rule_trace = (rule < s->n_rules) ? s->r_trace[rule]
                                         : ATP_TRACE_NONE;
    u32 ti = atp_trace_push_norm_step(s, *chain_tail, rule_trace,
                                      step_lhs, step_rhs, side, fwd,
                                      pos, pos_len);
    if (ti != ATP_TRACE_NONE) *chain_tail = ti;
    t = t2;
  }
  return t;
}

// Slice-based companion to atp_rewrite_normalize_record: rewrites
// `t` against the (lhs_arr / rhs_arr / n) slice, pushing one
// TRACE_NORM_STEP per fire.  `trace_arr[i]` is the TRACE_ORIENT
// trace id for slice rule i (so WL can locate the rule entry
// regardless of later compaction).  Used by interreduce so the
// dropped rule's normalization through the just-added rules is
// recorded as a chain of NORM_STEPs -- the resulting TRACE_SIMPLIFY
// parents on the last NORM_STEP, and WL's chain extractor emits
// each step as a SubstitutionLemma directly (no emitNorm BFS).
static Term atp_rewrite_normalize_slice_record(AtpState *s, Term t,
                                               const Term *lhs_arr,
                                               const Term *rhs_arr,
                                               const u32  *trace_arr,
                                               u32 n,
                                               u32 step_cap,
                                               u32 *chain_tail,
                                               u8 side, Term eq_other) {
  for (u32 it = 0; it < step_cap; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step_slice(s, t, pos, 0u,
                                           lhs_arr, rhs_arr, n,
                                           &rule, &pos_len, &fwd, &fired);
    if (!fired) break;
    Term step_lhs = (side == 0u) ? t2 : eq_other;
    Term step_rhs = (side == 0u) ? eq_other : t2;
    u32 rule_trace = (rule < n) ? trace_arr[rule] : ATP_TRACE_NONE;
    u32 ti = atp_trace_push_norm_step(s, *chain_tail, rule_trace,
                                      step_lhs, step_rhs, side, fwd,
                                      pos, pos_len);
    if (ti != ATP_TRACE_NONE) *chain_tail = ti;
    t = t2;
  }
  return t;
}

#include <sys/resource.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif
fn AtpStatus thvm_atp_step(AtpState *s) {
  if (s == NULL) return ATP_QUEUE_EMPTY;

  // WM loader intake: before the first pop, permute the queued axiom
  // set into Waldmeister's canonical sort order and stamp it ultimate
  // (SpezNormierung + initial=ultimate; see wm_intake.c).
  if (s->use_intake_order && !s->wm_intake_done) {
    s->wm_intake_done = 1u;
    atp_wm_intake_canonicalize(s);
  }

  AtpStatus goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  if (s->step >= s->step_cap) return ATP_TIMEOUT;

  // Wall-clock deadline check.  Polled per outer step; fine grain
  // enough to defend against runaway recursive-axiom expansions
  // (Y combinator's `Y x == x (Y x)` saturates with unbounded CP
  // fan-out) without showing up in the hot loop's profile.
  if (s->wall_deadline_us != 0u) {
    u64 now = atp_now_us();
    if (now != 0u && now >= s->wall_deadline_us) return ATP_TIMEOUT;
  }
  // Host abort (WL Abort[] / TimeConstrained[]): polled per outer step.
  if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return ATP_ABORTED;

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
    // Hard heap-cap memory guard.  After the in-loop GC has had a
    // chance to reclaim, if the live set STILL occupies more than
    // THVM_ATP_HEAP_ABORT_FRAC of the semi-space, the saturation has
    // genuinely outgrown the heap and continuing only thrashes (each
    // subsequent step pushes a few more cells, hits pressure again,
    // GC reclaims almost nothing).  Abort cleanly so wall-clock /
    // memory budget is respected on hard cases like the WolframAxioms
    // class that historically hung past TimeConstraint at multi-GB
    // RSS (project_tfindproof_timeout_no_kill).
    //
    // Default 0.85 -- aggressive enough to trip BEFORE the OS starts
    // paging on a typical 16GB box (with a multi-GB semi-space).
    // Tunable via THVM_ATP_HEAP_ABORT_FRAC env var; valid range
    // (0.5, 1.0); a 0 or out-of-range value disables the guard.
    static double frac = -1.0;
    if (frac < 0.0) {
      frac = 0.85;
      const char *e = getenv("THVM_ATP_HEAP_ABORT_FRAC");
      if (e != NULL && *e != '\0') {
        char *end = NULL;
        double v = strtod(e, &end);
        if (end != NULL && *end == '\0' && v > 0.5 && v < 1.0) {
          frac = v;
        } else if (end != NULL && *end == '\0' && v == 0.0) {
          frac = 2.0;     // sentinel "guard disabled" (HEAP_NEXT
                          // can never exceed semi-space capacity)
        }
      }
    }
    u64 lo, cap;
    if (gc_enabled()) {
      lo  = gc_from_start();
      cap = gc_from_end() - lo;
    } else {
      lo  = 0;
      cap = thvm_heap_cells();
    }
    if (cap > 0 && (double)(HEAP_NEXT - lo) > frac * (double)cap) {
      return ATP_ABORTED;
    }
  }

  // Process-RSS memory guard.  The heap-frac guard above only sees the
  // dyn heap; trace buffers, CP arena, FV/RI/discrim indices, the WL
  // ProofObject reconstruction state, and the kernel/paclet overhead
  // all live OUTSIDE it.  A run can exhaust system RAM with a perfectly
  // happy semi-space.  Poll the *current* RSS every 1024 steps.
  //
  // ru_maxrss is the HIGH-WATER MARK (peak ever) -- useless here; once
  // any earlier test touched the cap, every subsequent run aborts.  We
  // need the live RSS:
  //   Darwin: task_info(TASK_BASIC_INFO).resident_size
  //   Linux:  /proc/self/statm field 2 * page-size
  // 0 disables; default 8 GB (lenient enough to never trip a normal
  // run; THVM_ATP_RSS_ABORT_MB tightens it for memory-constrained
  // benches or paclet builds).
  if ((s->step & 0x3ffu) == 0u) {
    static long long rss_cap_bytes = -1;
    if (rss_cap_bytes < 0) {
      rss_cap_bytes = 8LL * 1024 * 1024 * 1024;
      const char *e = getenv("THVM_ATP_RSS_ABORT_MB");
      if (e != NULL && *e != '\0') {
        char *end = NULL;
        long long mb = strtoll(e, &end, 10);
        if (end != NULL && *end == '\0' && mb >= 0) {
          rss_cap_bytes = mb * 1024 * 1024;
        }
      }
    }
    if (rss_cap_bytes > 0) {
      long long rss = 0;
#if defined(__APPLE__)
      struct mach_task_basic_info info;
      mach_msg_type_number_t cnt = MACH_TASK_BASIC_INFO_COUNT;
      if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                    (task_info_t)&info, &cnt) == KERN_SUCCESS) {
        rss = (long long)info.resident_size;
      }
#else
      FILE *fp = fopen("/proc/self/statm", "r");
      if (fp != NULL) {
        long pages_total = 0, pages_resident = 0;
        if (fscanf(fp, "%ld %ld", &pages_total, &pages_resident) == 2) {
          long pgsz = sysconf(_SC_PAGESIZE);
          if (pgsz > 0) rss = (long long)pages_resident * (long long)pgsz;
        }
        fclose(fp);
      }
#endif
      if (rss > 0 && rss > rss_cap_bytes) return ATP_ABORTED;
    }
  }

  // Auto-MaxWeight: refresh the bound against the current rule set at
  // the top of the step so this step's first pushes (and the empty-
  // queue force-drain inside select_cp) see a bound consistent with R.
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_recompute(s);

  // ENIGMA Tier 2: GNN-guided re-rank.  Every gnn_rerank_period
  // SELECTIONS (and only while a model is loaded), permute the live CP
  // queue by the network's proof-relevance score before the next pick.
  // The whole forward runs on thvm's tensor runtime in C, no WL in the
  // loop.  Completeness holds (re-ranking only permutes order).  Default
  // OFF (period 0) so the unguided step is byte-identical.
  if (s->gnn_rerank_period > 0u && thvm_atp_gnn_loaded()
      && (s->cp_select_count % s->gnn_rerank_period) == 0u) {
    thvm_atp_gnn_rerank(s);
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
  u32 src_trace  = s->last_popped_trace;
  u32 chain_tail = src_trace;
  // Trace high-water mark before this CP's normalize chain is recorded.
  // A trivially-joined CP (kbo_eq(l, r) below) adds no rule and feeds no
  // proof, so its TRACE_NORM_STEP entries -- and the intermediate Terms
  // they reference -- are dead weight.  Joined CPs dominate a deep
  // completion (hundreds of thousands dropped), so retaining their
  // chains is what made record_norm_steps blow the heap.  Rewinding the
  // trace to here on a join lets the heap reset run too (nothing live
  // points past hcp_norm), keeping recording memory-bounded.
  u32 trace_mark = s->n_trace;
  Term l, r;
  u64 _ph_pop_t0 = atp_phase_now();
  if (s->record_norm_steps) {
    // Record each CP-normalize rewrite as a TRACE_NORM_STEP, chained
    // from the CP -- WL walks the chain linearly when extracting the
    // ProofObject.  Start from the trace entry's RAW (un-reduced)
    // CP form, not select_cp's already-queue-reduced form: queue-
    // time reduction (atp_cp_trivially_joinable) is off-trace, so the
    // resulting NORM_STEPs reach from the literal CP the verifier
    // expects all the way to the form orient sees -- no chain gap.
    Term raw_lhs = cp_lhs;
    Term raw_rhs = cp_rhs;
    if (src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
      Term cp_te = s->trace[src_trace];
      if (term_tag(cp_te) == TAG_CTR && term_ext(cp_te) == TRACE_CP &&
          term_ctr_n(cp_te) >= 4u) {
        raw_lhs = term_ctr_at(cp_te, 2);
        raw_rhs = term_ctr_at(cp_te, 3);
      }
    }
    // The replay phases the two WM flag sets exactly as the engine
    // applied them: first the generation-time treatment (KPBehandelt,
    // `-kg "r"` -> doR only -- atp_cp_trivially_joinable's rules-only
    // normalize), then the selection-time one (KPV_Select, `-ks
    // "r:e:s:p"` -> doR+doE).  A single interleaved doR+doE pass from
    // the raw CP records grounded-template equation steps ahead of the
    // oriented fixpoint -- steps the engine never fired and WM's
    // protocol never contains.
    g_atp_proof_oriented_only = 1u;
    Term l_r = atp_rewrite_normalize_record(s, raw_lhs, raw_rhs, 0u,
                                            &chain_tail, NORM_CAP);
    g_atp_proof_oriented_only = 0u;
    l = atp_rewrite_normalize_record(s, l_r, raw_rhs, 0u,
                                     &chain_tail, NORM_CAP);
    g_atp_proof_oriented_only = 1u;
    Term r_r = atp_rewrite_normalize_record(s, raw_rhs, l, 1u,
                                            &chain_tail, NORM_CAP);
    g_atp_proof_oriented_only = 0u;
    r = atp_rewrite_normalize_record(s, r_r, l, 1u,
                                     &chain_tail, NORM_CAP);
  } else {
    l = atp_rewrite_normalize(s, cp_lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
    r = atp_rewrite_normalize(s, cp_rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  }
  if (g_atp_phase_enabled) g_atp_phase_us_pop_normalize += atp_now_us() - _ph_pop_t0;

  // Per-pick pop-treatment verdict trace (THVM_ATP_POP_TRACE), mirroring
  // WM's WM_POPDUMP (KPV_Select) so the two engines' keep/drop sequences
  // can be aligned programmatically.  Default-off, behaviour-neutral.
  static int pop_trace = -1;
  if (pop_trace < 0) pop_trace = atp_env_on("THVM_ATP_POP_TRACE");
#define ATP_POPV(verdict) do { \
    if (pop_trace) { \
      fprintf(stderr, "POPV pick=%u verdict=%s lhs=", s->cp_select_count, (verdict)); \
      atp_dbg_print_term(stderr, l); fputs(" rhs=", stderr); \
      atp_dbg_print_term(stderr, r); fputc('\n', stderr); \
    } } while (0)

  if (kbo_eq(l, r)) {
    ATP_POPV("JOINED");
    // Trivially joined: this CP adds no rule.  Drop the NORM_STEP
    // entries just recorded for it (rewind the trace to trace_mark) so
    // no surviving trace entry references a Term past hcp_norm, then
    // reset the heap.  Without the trace rewind the heap reset would
    // dangle the dropped chain's intermediate Terms; with it, recording
    // a joined CP costs nothing -- the lever that keeps record_norm_steps
    // memory-bounded over a deep saturation.  The rules that DO get
    // added (the else-branch below) keep their full chains.
    if (s->record_norm_steps) {
      s->n_trace = trace_mark;
    }
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }

  // WM -ks "p" stage (KPV_Select, INF/KPVerwaltung.c:663): after the
  // selection-time R+E normalize and the joined drop, a popped CP that
  // is AC-redundant (its two sides AC-equal, excluding the generating
  // assoc/comm/ext-comm axioms) is perm-subsumed
  // (KPV_IncAnzKPspaeterPermSubsummiert -- GZ_ACVerzichtbar).  This is
  // the SELECTION-time half of WM's default-ON dokgP/Permsub filter;
  // all 65 perm-subsumptions on Huntington DoubleNegation happen here
  // (none at formation), so the queue still receives every AC-perm and
  // WM still SELECTS the ones it keeps in E -- only the residual-AC-
  // equal duplicates drop at pop after R+E normalization canonicalizes
  // them.  Gated by use_perm_subsume; the perm-subsume mask carries the
  // detected AC operators (thvm_atp_auto_ac).  Same dead-CP cleanup as
  // the joined / pop-subsume branches.
  if (s->use_perm_subsume && g_atp_perm_subsume_mask != 0ull &&
      atp_cp_perm_subsumed(l, r)) {
    ATP_POPV("PERMSUB");
    s->n_cps_dropped_perm_subsumed++;
    if (s->record_norm_steps) {
      s->n_trace = trace_mark;
    }
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }
  // WM -ks "s" stage (KPV_Select, INF/KPVerwaltung.c:667): after the
  // selection-time normalize and the joined drop, an UNORIENTABLE pair
  // (Unvergleichbar -- thvm KBO_UN) that an existing unorientable
  // equation subsumes is dropped before orientation
  // (KPV_IncAnzKPspaeterSubsummiert).  Same cleanup as the joined
  // branch: the pair feeds no rule, so its NORM_STEP chain and scratch
  // Terms are dead.
  if (s->use_pop_subsume && atp_compare(s, l, r) == KBO_UN &&
      atp_pop_eq_subsumed(s, l, r)) {
    ATP_POPV("SUBSUMED");
    s->n_cps_dropped_pop_subsumed++;
    if (s->record_norm_steps) {
      s->n_trace = trace_mark;
    }
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }

  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  if (added.count == 0) {
    ATP_POPV("REFUSED");
    s->step++;
    return ATP_RUNNING;
  }
  ATP_POPV("KEPT");
#undef ATP_POPV
  // WM `KPV_IncAktivierterRE` (SUE_selectedCPRE, KPVerwaltung.c:640): the
  // popped CP survived joined/perm/pop-subsume and became a rule or
  // equation.  This is the counter WM's CPdimension() reads to decide
  // the FIFO-vs-heuristic dimension; advancing it only on activation
  // (not on every pop) keeps thvm's FIFO-dimension pick on WM's cadence.
  s->n_activated_re++;

  // Trace each newly-added rule with its source CP as parent_a (or
  // the chain tail when norm-step recording is on: the chain ends at
  // the last NORM_STEP, which the ORIENT inherits from).  For
  // unfailing 2-way fallback both directions get separate entries so
  // PCL output can identify each rule individually.  Stash the trace
  // index in r_trace[] so generate_cps can record TRACE_CP parents
  // for any CP born from this rule.
  //
  // Waldmeister `RechtsUnfreiErzeugen` (RUndEVerwaltung.c:366-397):
  // trailing `fvi_count` slots in [first, first+count) are grounded
  // free-variable instances of the leading orientation, not direct
  // orientations.  Stamp them with TRACE_FVI so the WL proof renderer
  // labels them "FreeVarInstance"; downstream chain-walking treats
  // TRACE_FVI as a TRACE_ORIENT sibling (same children layout, same
  // tolerant decoder fallback).
  u32 fvi_lo = added.first + added.count - added.fvi_count;
  for (u32 k = 0; k < added.count; k++) {
    u32 rid = added.first + k;
    Term rl = s->lhs[rid];
    Term rr = s->rhs[rid];
    u32  reason = (rid >= fvi_lo) ? TRACE_FVI : TRACE_ORIENT;
    u32  t  = atp_trace_push(s, reason, chain_tail,
                             ATP_TRACE_NONE, rl, rr);
    s->r_trace[rid] = t;
    // WM order mirror: the fact is now in R/E with its identity --
    // register its tree faces (RE_RegelEinfuegen / GleichungEinfuegen).
    // The WM-distinguished face is `selRec.lhs = KPLinks`: a genuine
    // two-parent superposition lands KPLinks on thvm's stored RHS
    // (dist_rhs=1, the default); a CP that re-derives an existing fact
    // through a single parent keeps the natural left side on thvm's
    // stored LHS (dist_rhs=0).  atp_wmo_eq_dist_rhs returns that flag
    // from the selected CP's parents; insert_fact_ex's cp_derived arg
    // carries dist_rhs directly (1 -> use stored RHS as distinguished).
    if (s->use_emission_order) {
      u8 dist_rhs = atp_wmo_eq_dist_rhs(s, src_trace);
      atp_wmo_insert_fact_ex(s, rid, dist_rhs);
    }
  }

#ifdef ATP_CP_GROUND_JOIN
  // WM -gj forward fact test at creation (RUndEVerwaltung.c:182-183 /
  // :457-460): runs BEFORE interreduction + CP generation, matching
  // WM's RE_Erzeugte* -> ArbeitsAufnahme order.  The status rides the
  // slot through the interreduce compaction below.
  if (s->use_bwd_ground_join) atp_gj_classify_added(s, added);
#endif

  // Interreduce shifts new-rule indices down by `dropped`.
  u64 _ph_ir_t0 = atp_phase_now();
  u32 dropped = thvm_atp_interreduce(s, added);
  if (g_atp_phase_enabled) g_atp_phase_us_interreduce += atp_now_us() - _ph_ir_t0;
  AtpAddedRange post = added;
  post.first = (dropped > post.first) ? 0 : (post.first - dropped);

  // Auto-MaxWeight: refresh the bound against the now-current rule set
  // before this step's CPs are generated/pushed (the just-oriented rule
  // may have deepened or, via interreduce, shrunk R).
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_recompute(s);

  u64 _ph_gen_t0 = atp_phase_now();
  thvm_atp_generate_cps(s, post);
  if (g_atp_phase_enabled) g_atp_phase_us_cp_gen += atp_now_us() - _ph_gen_t0;

  // Re-admit any stashed CP now within the refreshed bound.
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_drain(s, 0u);

  // Periodic full-rule-set CP-queue interreduction (Waldmeister
  // KPV_KPMengeInterreduzieren).  Gated behind cp_set_interreduce and run
  // every cp_set_ir_period-th rule addition so the per-CP full-R sweep's
  // cost is amortized.  Default off -> the call is skipped and the engine
  // is byte-identical.
  if (s->cp_set_interreduce && added.count > 0u) {
    u32 period = s->cp_set_ir_period ? s->cp_set_ir_period
                                     : ATP_CP_SET_IR_PERIOD;
    if (s->n_rules % period == 0u) {
      u64 _ph_cpir_t0 = atp_phase_now();
      atp_cp_set_interreduce(s);
      if (g_atp_phase_enabled) g_atp_phase_us_cp_set_ir += atp_now_us() - _ph_cpir_t0;
    }
  }

  // Waldmeister IR_PufferAuslesen (Interreduktion.c:387-392): the
  // victims thvm_atp_interreduce buffered under use_wm_demote re-enter
  // the queue only NOW -- after this fact's CPs were generated and the
  // periodic CP-set sweep ran -- as the step's LAST queue mutation, so
  // each victim's FIFO age is younger than every CP the new fact
  // produced (WM's ArbeitsAufnahme work order, Hauptkomponenten.c:
  // 308-331; McCune-II's `ues 32` carries age 32, after CPs 14-31).
  if (s->n_irv > 0u) atp_wm_demote_drain(s);

#ifdef ATP_CP_GROUND_JOIN
  // WM RueckwaertsGrundzusammenfuehrbarkeit (ArbeitsAufnahme,
  // Hauptkomponenten.c:329): AFTER this fact's CPs were generated and
  // the IR-victim drain ran, re-test every EXISTING fact against the
  // extended system and sterilize those that became ground-joinable.
  // Skips the just-added range (Faktum != Neues, :266/:286).
  if (s->use_bwd_ground_join) {
    atp_bwd_ground_join_walk(s, post.first, post.first + post.count);
  }
#endif

  u64 _ph_gc_t0 = atp_phase_now();
  goal = thvm_atp_goal_check(s);
  if (g_atp_phase_enabled) g_atp_phase_us_goal_check += atp_now_us() - _ph_gc_t0;
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
      case TRACE_AXIOM:    type_str = "axiom";    break;
      case TRACE_ORIENT:   type_str = "orient";   break;
      case TRACE_CP:       type_str = "cp";       break;
      case TRACE_SIMPLIFY: type_str = "simplify"; break;
      case TRACE_FVI:      type_str = "fvi";      break;
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

// === proof extraction =================================================
//
// Reconstruct the equational rewrite chain that closes a single-NF
// goal.  thvm_atp_trace_serialize (above) emits the COMPLETION trace
// -- the CP/rule derivation DAG; this emits the orthogonal object: the
// chain of rewrites taking each conjecture side to the shared normal
// form.
//
// The recording rewriter mirrors the WL-driven engine's normalizer so
// the recorded chain reproduces goal_check's single-NF result:
// leftmost-outermost, lowest-index rule.  An oriented rule fires
// forward (lhs->rhs); under ordered rewriting an unorientable equation
// fires whichever direction strictly decreases the redex -- so the
// recorded chain descends a well-founded order and cannot bounce
// between the two faces of a symmetric equation.

// One leftmost-outermost rewrite of `t`, recording the redex path and
// the rule index.  `pos` is caller-owned scratch holding the path so
// far in pos[0..depth); on a hit the full path is left in
// pos[0..*out_pos_len).  *out_fwd is 1 when the rule fired lhs->rhs, 0
// when an unorientable equation fired rhs->lhs.  Returns the rewritten
// term; *fired = 1 on a hit, 0 at a fixpoint.
static Term atp_proof_rewrite_step(AtpState *s, Term t, u8 *pos, u8 depth,
                                   u32 *out_rule, u8 *out_pos_len,
                                   u8 *out_fwd, u8 *fired) {
  for (u32 i = 0; i < s->n_rules; i++) {
#ifdef ATP_ORDERED_REWRITE
    // An unorientable equation (r_orient[i] == 0) is stored once and
    // rewrites in whichever direction is order-decreasing for the
    // redex at hand -- the same both-directions, order-gated rule as
    // atp_ordered_try_top, so the recorded step matches the normalizer.
    if (!s->r_orient[i]) {
      if (g_atp_proof_oriented_only) continue;   // oriented-fixpoint phase
      {                                                     // l -> r
        RewriteSubst sub = {{0}};
        if (thvm_match(s->lhs[i], t, &sub)) {
          Term tmpl = atp_unorient_template(s, s->lhs[i], s->rhs[i]);
          if (tmpl != 0) {
            Term repl = thvm_subst_apply(tmpl, &sub);
            if (atp_compare(s, t, repl) == KBO_GT) {
              *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
              return repl;
            }
          }
        }
      }
      {                                                     // r -> l
        RewriteSubst sub = {{0}};
        if (thvm_match(s->rhs[i], t, &sub)) {
          Term tmpl = atp_unorient_template(s, s->rhs[i], s->lhs[i]);
          if (tmpl != 0) {
            Term repl = thvm_subst_apply(tmpl, &sub);
            if (atp_compare(s, t, repl) == KBO_GT) {
              *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 0;
              return repl;
            }
          }
        }
      }
      continue;
    }
#endif
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[i], t, &sub)) {
      *fired = 1;
      *out_rule = i;
      *out_pos_len = depth;
      *out_fwd = 1;
      return thvm_subst_apply(s->rhs[i], &sub);
    }
  }
  if (term_tag(t) == TAG_CTR && depth < ATP_PROOF_MAX_DEPTH) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n; i++) {
      pos[depth] = (u8)i;
      u8 cf = 0;
      Term nch = atp_proof_rewrite_step(s, term_ctr_at(t, i), pos,
                                        (u8)(depth + 1u), out_rule,
                                        out_pos_len, out_fwd, &cf);
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

// Slice-aware port of atp_proof_rewrite_step above: same outermost-
// leftmost rule pick (with the ORDERED_REWRITE both-directions/order-
// gated branch for unorientable equations), but the candidate rule
// list is the (lhs_arr / rhs_arr / n) slice rather than s->lhs.  The
// per-rule cached orientation s->r_orient does not apply to a custom
// slice, so the unorientable / orientable split falls back to a per-
// rule atp_compare here -- in the interreduce caller's case n <= 2 so
// the cost is trivial.
static Term atp_proof_rewrite_step_slice(AtpState *s, Term t,
                                         u8 *pos, u8 depth,
                                         const Term *lhs_arr,
                                         const Term *rhs_arr,
                                         u32 n,
                                         u32 *out_rule, u8 *out_pos_len,
                                         u8 *out_fwd, u8 *fired) {
  // WM per-position redex priority (NF/NFBildung.c:503-531): oriented
  // rules first at this position, unorientable equations only when no
  // oriented rule matched here -- the same two-pass order as
  // atp_ordered_try_top / find_redex_ft, so the recorded chain replays
  // the slice normalizers' redex picks exactly.
  for (u32 i = 0; i < n; i++) {
#ifdef ATP_ORDERED_REWRITE
    if (atp_compare(s, lhs_arr[i], rhs_arr[i]) != KBO_GT) continue;
#endif
    RewriteSubst sub = {{0}};
    if (thvm_match(lhs_arr[i], t, &sub)) {
      *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
      return thvm_subst_apply(rhs_arr[i], &sub);
    }
  }
#ifdef ATP_ORDERED_REWRITE
  for (u32 i = 0; i < n; i++) {
    if (atp_compare(s, lhs_arr[i], rhs_arr[i]) == KBO_GT) continue;
    {                                                         // l -> r
      RewriteSubst sub = {{0}};
      if (thvm_match(lhs_arr[i], t, &sub)) {
        Term tmpl = atp_unorient_template(s, lhs_arr[i], rhs_arr[i]);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
            return repl;
          }
        }
      }
    }
    {                                                         // r -> l
      RewriteSubst sub = {{0}};
      if (thvm_match(rhs_arr[i], t, &sub)) {
        Term tmpl = atp_unorient_template(s, rhs_arr[i], lhs_arr[i]);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 0;
            return repl;
          }
        }
      }
    }
  }
#endif
  if (term_tag(t) == TAG_CTR && depth < ATP_PROOF_MAX_DEPTH) {
    u32 n_ctr = term_ctr_n(t);
    if (n_ctr > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n_ctr; i++) {
      pos[depth] = (u8)i;
      u8 cf = 0;
      Term nch = atp_proof_rewrite_step_slice(s, term_ctr_at(t, i), pos,
                                              (u8)(depth + 1u),
                                              lhs_arr, rhs_arr, n,
                                              out_rule, out_pos_len,
                                              out_fwd, &cf);
      if (cf) {
        Term children[REWRITE_MAX_ARITY];
        for (u32 j = 0; j < n_ctr; j++) {
          children[j] = (j == i) ? nch : term_ctr_at(t, j);
        }
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n_ctr);
      }
    }
  }
  *fired = 0;
  return t;
}

// Append one recorded rewrite of `t` to out[*n) and return the result;
// `*progressed` is set when a step fired.  `oriented_only` selects the
// orientable-fixpoint phase (skip unorientable equations).
static Term atp_proof_record_one(AtpState *s, Term t, u32 side,
                                 u8 oriented_only,
                                 AtpProofStep *out, u32 cap, u32 *n,
                                 u8 *progressed) {
  u8  pos[ATP_PROOF_MAX_DEPTH];
  u32 rule = 0;
  u8  pos_len = 0, fwd = 1u, fired = 0;
  g_atp_proof_oriented_only = oriented_only;
  Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                   &fwd, &fired);
  g_atp_proof_oriented_only = 0u;
  if (!fired || kbo_eq(t, t2)) { *progressed = 0u; return t; }
  if (*n < cap) {
    AtpProofStep *st = &out[*n];
    st->side    = side;
    st->rule    = rule;
    st->fwd     = fwd;
    st->pos_len = pos_len;
    for (u8 k = 0; k < pos_len; k++) st->pos[k] = pos[k];
    st->before  = t;
    st->after   = t2;
    (*n)++;
  }
  *progressed = 1u;
  return t2;
}

// Normalize one conjecture side, appending each rewrite to out[*n).
// `side` tags every recorded step; returns the side's normal form.
//
// Mirrors atp_rewrite_normalize_ordered (the goal-check normalizer):
// run the orientable rules to a fixpoint, then take ONE unorientable
// (both-directions, order-gated) step, and repeat to a joint fixpoint.
// A single interleaved pass reaches a DIFFERENT normal form on a
// non-confluent rule set (completion stopped at the goal-join), so the
// recorded chain would not meet the other side -- thvm_atp_proof_extract
// would then return 0 and the ProofObject reconstruction fail even
// though the engine proved the goal.
static Term atp_proof_record_side(AtpState *s, Term t, u32 side,
                                  AtpProofStep *out, u32 cap, u32 *n) {
  for (u32 it = 0; it < ATP_PROOF_MAX_STEPS; it++) {
    // Wall-deadline / host-abort poll.  This routine runs AFTER the
    // saturation loop returns (called from thvm_wl_atp_run_proof's
    // post-engine proof-extraction step), so without a poll here a
    // timed-out saturation followed by a long proof-extract rewrite
    // ignores TimeConstraint and hangs the kernel past wall_deadline.
    if ((it & 0x7Fu) == 0u) {
      if (s->wall_deadline_us != 0u) {
        u64 now = atp_now_us();
        if (now != 0u && now >= s->wall_deadline_us) return t;
      }
      if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return t;
    }
    u8 progressed = 0u;
    // Orientable indexed-fixpoint phase: drain every oriented rewrite.
    for (u32 j = 0; j < ATP_PROOF_MAX_STEPS; j++) {
      u8 ofired = 0u;
      t = atp_proof_record_one(s, t, side, 1u, out, cap, n, &ofired);
      if (!ofired) break;
      progressed = 1u;
      if ((j & 0x7Fu) == 0u) {
        if (s->wall_deadline_us != 0u) {
          u64 now = atp_now_us();
          if (now != 0u && now >= s->wall_deadline_us) return t;
        }
        if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return t;
      }
    }
    // One unorientable step (skipped when n_unorient == 0).
    u8 ufired = 0u;
    t = atp_proof_record_one(s, t, side, 0u, out, cap, n, &ufired);
    if (ufired) progressed = 1u;
    if (!progressed) return t;            // joint fixpoint
  }
  return t;
}

// The term reached on `side` after keeping `k` of that side's recorded
// rewrites: k == 0 is the side's seed (the first step's `before`, or the
// seed `seed` when the side recorded no steps), otherwise the k-th step's
// `after`.  `lo`/`hi` bound the side's [lo, hi) slice in out[].
static Term atp_chain_term(const AtpProofStep *out, u32 lo, u32 hi,
                           Term seed, u32 k) {
  if (lo >= hi) return seed;             // no steps recorded this side
  if (k == 0u) return out[lo].before;
  return out[lo + (k - 1u)].after;
}

// Goal-chain meet trim (Option B): the two sides are each recorded to
// their INDEPENDENT joint fixpoint, but FindEquationalProof stops the
// moment the sides coincide.  On a non-confluent completion that join is
// reached BEFORE either fixpoint (e.g. the unorientable a.a == b.b rule,
// grounded to a reserved min-const, fires once more on each side after
// they already met -- a 2-step out-and-back that nets to a no-op).  Find
// the earliest (i, j), minimizing i + j, where keeping i lhs steps and j
// rhs steps lands both sides on kbo-equal terms, then drop every step
// past that meet on either side.  The kept chain still closes the goal:
// lhs walks to the meet, rhs walks to the same meet, meet == meet.
static u32 atp_proof_trim_to_meet(const AtpProofStep *out, u32 n0, u32 n,
                                  Term seed_lhs, Term seed_rhs,
                                  AtpProofStep *trimmed) {
  u32 best_i = n0, best_j = n - n0, best_sum = (n0) + (n - n0);
  u8  found = 0u;
  for (u32 i = 0; i <= n0; i++) {
    Term tl = atp_chain_term(out, 0u, n0, seed_lhs, i);
    for (u32 j = 0; j <= n - n0; j++) {
      if (i + j >= best_sum) continue;   // can't beat the current best
      Term tr = atp_chain_term(out, n0, n, seed_rhs, j);
      if (kbo_eq(tl, tr)) {
        best_i = i; best_j = j; best_sum = i + j; found = 1u;
      }
    }
  }
  if (!found) {                           // sides never coincide on a prefix
    for (u32 k = 0; k < n; k++) trimmed[k] = out[k];
    return n;
  }
  // Compact: keep out[0, best_i) (lhs) then out[n0, n0+best_j) (rhs).
  u32 w = 0u;
  for (u32 k = 0; k < best_i; k++)            trimmed[w++] = out[k];
  for (u32 k = 0; k < best_j; k++)            trimmed[w++] = out[n0 + k];
  return w;
}

fn u32 thvm_atp_proof_extract(AtpState *s, AtpProofStep *out, u32 cap) {
  if (s == NULL || out == NULL || cap == 0u) return 0;
  // Single-NF extraction only: no goal, or an existential (narrowing)
  // goal, has no two-sided rewrite chain to reconstruct here.
  if (s->goal_lhs == 0 || s->goal_existential) return 0;

  // Record the goal_lhs chain (side 0) then the goal_rhs chain
  // (side 1), both forward, appended into out[].  The assembled
  // proof rewrites the equation L == R: first L down to its normal
  // form, then R down to the same normal form, ending at NF == NF.
  u32  n = 0;
  Term nf_lhs = atp_proof_record_side(s, s->goal_lhs, 0u, out, cap, &n);
  u32  n0 = n;                            // boundary: side-0 steps in [0, n0)
  Term nf_rhs = atp_proof_record_side(s, s->goal_rhs, 1u, out, cap, &n);

  // Not single-NF provable -- the two sides never meet under R.  A
  // symmetric goal closed only by the MNF search lands here.
  if (!kbo_eq(nf_lhs, nf_rhs)) return 0;

  // Trim both chains back to the EARLIEST cross-side meet so the
  // reconstructed proof stops where FindEquationalProof stops, instead
  // of trailing the post-meet round-trip to each side's fixpoint.
  static AtpProofStep trimmed[ATP_PROOF_MAX_STEPS * 2u];
  u32 cmin = cap < (u32)(ATP_PROOF_MAX_STEPS * 2u)
                 ? cap : (u32)(ATP_PROOF_MAX_STEPS * 2u);
  if (n <= cmin) {
    u32 m = atp_proof_trim_to_meet(out, n0, n, s->goal_lhs, s->goal_rhs,
                                   trimmed);
    for (u32 k = 0; k < m; k++) out[k] = trimmed[k];
    n = m;
  }

  return n;
}

fn u32 thvm_atp_proof_serialize(const AtpProofStep *steps, u32 n_steps,
                                char *buf, u32 cap) {
  if (steps == NULL || buf == NULL || cap == 0u) return 0;
  buf[0] = '\0';
  u32 w = 0;
  for (u32 i = 0; i < n_steps; i++) {
    if (w + 1u >= cap) break;
    const AtpProofStep *st = &steps[i];
    int nw = snprintf(buf + w, cap - w, "%c rule %u %s @",
                      st->side == 0u ? 'L' : 'R', st->rule,
                      st->fwd ? "fwd" : "rev");
    if (nw < 0) break;
    w += (u32)nw;
    if (w + 1u >= cap) break;
    if (st->pos_len == 0u) {
      w += (u32)snprintf(buf + w, cap - w, "top");
    } else {
      for (u8 k = 0; k < st->pos_len && w + 2u < cap; k++) {
        w += (u32)snprintf(buf + w, cap - w, "%s%u",
                           k == 0 ? "" : ".", st->pos[k]);
      }
    }
    if (w + 3u >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, ": ");
    w += atp_pretty_term(st->before, buf + w, cap - w);
    if (w + 5u >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, " => ");
    w += atp_pretty_term(st->after, buf + w, cap - w);
    if (w + 1u >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, "\n");
  }
  if (w >= cap) w = cap - 1;
  buf[w] = '\0';
  return w;
}

// === ENIGMA-style CP feature extraction + labelled dataset ==========
// Data-foundation step for a LEARNED critical-pair selector.  See
// thvm.h for the feature schema; training (logistic regression / GBDT)
// and the resulting fast C scorer in select_cp are a later step.

// Count distinct FVR ids and total FVR occurrences across (l, r) in a
// single pair of walks.  `seen` is a small membership bitset keyed on
// the var id modulo its width (var ids are dense [0,k) after
// ATP_VAR_NORM, so collisions don't happen on the canonical CP forms
// this records; the count is exact for k <= 64).
static void atp_cp_var_stats_rec(Term t, u64 *seen, u32 *distinct,
                                 u32 *occ) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      (*occ)++;
      u32 bit = id & 63u;
      if (!((*seen >> bit) & 1u)) {
        *seen |= (1ull << bit);
        (*distinct)++;
      }
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        atp_cp_var_stats_rec(term_ctr_at(t, i), seen, distinct, occ);
      }
      return;
    }
    default: return;
  }
}

// Top CTR label of a term (the feature's "top symbol"); 0 for a
// variable / atom / non-CTR root.
static u32 atp_cp_top_symbol(Term t) {
  return (term_tag(t) == TAG_CTR) ? term_ext(t) : 0u;
}

// Does any top-symbol-headed subterm of `t` also occur (by top symbol)
// as a subterm of the goal term `g`?  A cheap structural-overlap proxy
// for ENIGMA's goal-distance feature: 1 if the CP touches a function
// symbol the goal uses at some position, else 0.  Variables carry no
// goal signal (a var generalizes everything), mirroring the MinStruct
// spirit of the CPinGoal classifier.
static int atp_term_label_set(Term t, u64 *labels) {
  if (term_tag(t) == TAG_CTR) {
    u32 lab = term_ext(t);
    *labels |= (1ull << (lab & 63u));
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) atp_term_label_set(term_ctr_at(t, i), labels);
  }
  return 0;
}
static int atp_cp_shares_goal_symbol(const AtpState *s, Term l, Term r) {
  if (s == NULL || s->goal_lhs == 0) return 0;
  Term gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
  Term gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  u64 goal_labels = 0u;
  atp_term_label_set(gl, &goal_labels);
  atp_term_label_set(gr, &goal_labels);
  u64 cp_labels = 0u;
  atp_term_label_set(l, &cp_labels);
  atp_term_label_set(r, &cp_labels);
  return (goal_labels & cp_labels) != 0u;
}

// === ENIGMA Tier 2: anonymised CP hypergraph builder ================
// Build state threaded through the preorder walk.  The dedup map keys a
// SYMBOL/VAR node by (kind, identity): SYM_CTR uses the CTR label,
// SYM_NUM uses the raw numeric value, and VAR uses the FVR id.  These
// share one positional key table (distinct `kind` keeps a CTR label `3`
// and a NUM value `3` from colliding).  `node_idx` is the graph node the
// key maps to; `occ` counts occurrences so we can write the structural
// occurrence_count feature after the walk.  Crucially the key VALUE is
// only ever used for dedup equality -- it never reaches node_feat, so
// the graph is invariant under any consistent renaming.
typedef enum {
  ATP_CPG_KEY_CTR = 0,   // a CTR label
  ATP_CPG_KEY_NUM = 1,   // a NUM raw value
  ATP_CPG_KEY_VAR = 2,   // an FVR id
} AtpCpgKeyKind;

typedef struct {
  u8         kind;        // AtpCpgKeyKind
  u64        key;         // the concrete identity (label / value / var id)
  u32        node_idx;    // the SYMBOL/VAR graph node for this key
  u32        occ;         // occurrence count across both CP sides
} AtpCpgKeyEnt;

typedef struct {
  AtpCpGraph   *g;
  AtpCpgKeyEnt  keys[ATP_CPG_MAX_NODES];   // one entry per distinct sym/var
  u32           n_keys;
} AtpCpgBuild;

// Append a node of the given type; returns its index, or U32_MAX on cap
// overflow (b->g->overflow is set).  node_feat is cleared here; the
// caller patches in the type-specific structural columns.
static u32 atp_cpg_add_node(AtpCpgBuild *b, u8 type) {
  AtpCpGraph *g = b->g;
  if (g->n_nodes >= ATP_CPG_MAX_NODES) { g->overflow = 1u; return 0xffffffffu; }
  u32 idx = g->n_nodes++;
  g->node_type[idx] = type;
  g->node_label[idx] = 0u;   // TERM / CP-super carry no symbol; intern overwrites
  float *f = g->node_feat + (size_t)idx * ATP_CPG_FEAT_DIM;
  for (u32 j = 0; j < ATP_CPG_FEAT_DIM; j++) f[j] = 0.0f;
  return idx;
}

// Append a directed edge; sets overflow on cap.
static void atp_cpg_add_edge(AtpCpgBuild *b, u32 src, u32 dst, u8 type) {
  AtpCpGraph *g = b->g;
  if (g->n_edges >= ATP_CPG_MAX_EDGES) { g->overflow = 1u; return; }
  u32 e = g->n_edges++;
  g->edge_src[e]  = src;
  g->edge_dst[e]  = dst;
  g->edge_type[e] = type;
}

// Find-or-create the SYMBOL/VAR node for (kind, key) in first-appearance
// order; bumps its occurrence count.  Returns the node index, or
// U32_MAX on overflow.  `node_type` is ATP_CPG_SYMBOL or ATP_CPG_VAR;
// `arity` is the CTR arity for a symbol (0 otherwise) and is written
// only when the node is first created.
static u32 atp_cpg_intern(AtpCpgBuild *b, u8 kind, u64 key,
                          u8 node_type, u32 arity) {
  for (u32 i = 0; i < b->n_keys; i++) {
    if (b->keys[i].kind == kind && b->keys[i].key == key) {
      b->keys[i].occ++;
      return b->keys[i].node_idx;
    }
  }
  u32 idx = atp_cpg_add_node(b, node_type);
  if (idx == 0xffffffffu) return idx;
  // Record the concrete symbol identity (CTR label / FVR id / NUM value)
  // for exact reconstruction; the feature columns below stay anonymised.
  b->g->node_label[idx] = key;
  // Set the structural type columns now; occurrence_count is patched in
  // after the whole walk so repeat occurrences are all counted.
  float *f = b->g->node_feat + (size_t)idx * ATP_CPG_FEAT_DIM;
  if (node_type == ATP_CPG_SYMBOL) { f[1] = 1.0f; f[3] = (float)arity; }
  else /* ATP_CPG_VAR */           { f[2] = 1.0f; }
  if (b->n_keys < ATP_CPG_MAX_NODES) {
    AtpCpgKeyEnt *ke = &b->keys[b->n_keys++];
    ke->kind = kind; ke->key = key; ke->node_idx = idx; ke->occ = 1u;
  }
  return idx;
}

// Preorder walk: create a TERM node for `t`, wire it to its root
// symbol/var node (E_TERM_SYM), recurse into children wiring
// E_TERM_CHILD edges.  Returns the TERM node index for `t`, or U32_MAX
// on overflow (which short-circuits the rest of the walk).
static u32 atp_cpg_walk(AtpCpgBuild *b, Term t) {
  u32 tn = atp_cpg_add_node(b, ATP_CPG_TERM);
  if (tn == 0xffffffffu) return tn;
  // A TERM node is a single occurrence; mark is_term + occ 1.
  float *f = b->g->node_feat + (size_t)tn * ATP_CPG_FEAT_DIM;
  f[0] = 1.0f; f[4] = 1.0f;

  u8  tag = term_tag(t);
  u32 root;
  if (tag == TAG_CTR) {
    u32 n = term_ctr_n(t);
    root = atp_cpg_intern(b, ATP_CPG_KEY_CTR, (u64)term_ext(t),
                          ATP_CPG_SYMBOL, n);
    if (root == 0xffffffffu) return root;
    atp_cpg_add_edge(b, tn, root, ATP_CPG_E_TERM_SYM);
    for (u32 i = 0; i < n; i++) {
      u32 cn = atp_cpg_walk(b, term_ctr_at(t, i));
      if (cn == 0xffffffffu) return cn;
      atp_cpg_add_edge(b, tn, cn, ATP_CPG_E_TERM_CHILD);
    }
  } else if (tag == TAG_FVR) {
    root = atp_cpg_intern(b, ATP_CPG_KEY_VAR, (u64)term_ext(t),
                          ATP_CPG_VAR, 0u);
    if (root == 0xffffffffu) return root;
    atp_cpg_add_edge(b, tn, root, ATP_CPG_E_TERM_SYM);
  } else { /* TAG_NUM and any other atom: a nullary constant-like symbol */
    root = atp_cpg_intern(b, ATP_CPG_KEY_NUM, (u64)term_val(t),
                          ATP_CPG_SYMBOL, 0u);
    if (root == 0xffffffffu) return root;
    atp_cpg_add_edge(b, tn, root, ATP_CPG_E_TERM_SYM);
  }
  return tn;
}

fn int thvm_atp_cp_graph(Term lhs, Term rhs, AtpCpGraph *out) {
  if (out == NULL) return 0;
  out->n_nodes  = 0u;
  out->n_edges  = 0u;
  out->overflow = 0u;

  AtpCpgBuild b;
  b.g = out;
  b.n_keys = 0u;

  // Node 0 is always the CP super-node.
  u32 super = atp_cpg_add_node(&b, ATP_CPG_CPSUPER);
  if (super == 0xffffffffu) return 0;     // cap of 0 -- defensive
  {
    float *f = out->node_feat + (size_t)super * ATP_CPG_FEAT_DIM;
    f[5] = 1.0f; f[4] = 1.0f;             // is_cpsuper + single occurrence
  }

  // Walk lhs then rhs in preorder.
  u32 lroot = atp_cpg_walk(&b, lhs);
  if (lroot == 0xffffffffu) return 0;
  u32 rroot = atp_cpg_walk(&b, rhs);
  if (rroot == 0xffffffffu) return 0;

  atp_cpg_add_edge(&b, super, lroot, ATP_CPG_E_CP_LHS);
  atp_cpg_add_edge(&b, super, rroot, ATP_CPG_E_CP_RHS);
  if (out->overflow) return 0;

  // Patch each SYMBOL/VAR node's occurrence_count from the dedup table.
  for (u32 i = 0; i < b.n_keys; i++) {
    float *f = out->node_feat
             + (size_t)b.keys[i].node_idx * ATP_CPG_FEAT_DIM;
    f[4] = (float)b.keys[i].occ;
  }
  return 1;
}

fn void thvm_atp_cp_features(const AtpState *s, Term lhs, Term rhs,
                             u32 age, float *out) {
  if (out == NULL) return;
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) out[i] = 0.0f;

  u32 sl = atp_symbol_count(lhs), sr = atp_symbol_count(rhs);
  u32 dl = atp_term_depth(lhs),   dr = atp_term_depth(rhs);
  u64 seen = 0u; u32 distinct = 0u, occ = 0u;
  atp_cp_var_stats_rec(lhs, &seen, &distinct, &occ);
  atp_cp_var_stats_rec(rhs, &seen, &distinct, &occ);

  // `s` is logically const for feature reads but the weight helpers
  // take a non-const AtpState (they only read the rule set / config).
  AtpState *sm = (AtpState *)s;

  out[0]  = (float)(sl + sr);
  out[1]  = (float)(dl > dr ? dl : dr);
  out[2]  = (float)distinct;
  out[3]  = (float)occ;
  out[4]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_ADD);
  out[5]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_GT);
  out[6]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_MIX2);
  out[7]  = (float)atp_goal_weight(s, lhs, rhs);
  out[8]  = (float)age;
  out[9]  = (float)atp_cp_top_symbol(lhs);
  out[10] = (float)atp_cp_top_symbol(rhs);
  out[11] = (float)atp_cp_shares_goal_symbol(s, lhs, rhs);
  KboCmp c = atp_compare(sm, lhs, rhs);
  out[12] = (float)((c == KBO_GT || c == KBO_LT) ? 1 : 0);
  out[13] = (float)atp_unif_measure(lhs, rhs);
}

fn void thvm_atp_set_record_cp_features(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_cp_features = on ? 1u : 0u;
}

// Grow the recording arrays to hold at least `need` rows.
static void atp_cp_feat_ensure(AtpState *s, u32 need) {
  if (need <= s->cp_feat_cap) return;
  u32 cap = s->cp_feat_cap ? s->cp_feat_cap * 2u : 256u;
  while (cap < need) cap *= 2u;
  s->cp_feat_rows  = (float *)realloc(s->cp_feat_rows,
                                      (size_t)cap * ATP_CP_FEATURE_DIM
                                        * sizeof(float));
  s->cp_feat_trace = (u32 *)realloc(s->cp_feat_trace, cap * sizeof(u32));
  s->cp_feat_label = (u8 *)realloc(s->cp_feat_label, cap * sizeof(u8));
  s->cp_feat_cap   = cap;
}

// Record one PROCESSED CP (called from thvm_atp_select_cp under the
// flag).  `trace_id` is the popped CP's trace-entry index.
static void atp_cp_feat_record(AtpState *s, Term lhs, Term rhs,
                               u32 trace_id) {
  atp_cp_feat_ensure(s, s->n_cp_feat + 1u);
  float *row = s->cp_feat_rows + (size_t)s->n_cp_feat * ATP_CP_FEATURE_DIM;
  thvm_atp_cp_features(s, lhs, rhs, s->n_cp_feat, row);
  s->cp_feat_trace[s->n_cp_feat] = trace_id;
  s->cp_feat_label[s->n_cp_feat] = 0u;
  s->n_cp_feat++;
}

// Parents (in the trace DAG) of trace entry `ti`: for the 4-child
// reasons (AXIOM/ORIENT/CP/SIMPLIFY) children 0 and 1 are parent trace
// ids; for TRACE_NORM_STEP only child 0 is a parent trace id (child 1
// is a RULE index, not a trace id).  Writes up to 2 parents into
// out[0..2) and returns the count.
static u32 atp_trace_parents(const AtpState *s, u32 ti, u32 *out) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace) return 0u;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) return 0u;
  u32 reason = term_ext(e);
  u32 n = 0u;
  u32 p0 = term_val(term_ctr_at(e, 0));
  if (p0 != ATP_TRACE_NONE) out[n++] = p0;
  if (reason != TRACE_NORM_STEP) {
    u32 p1 = term_val(term_ctr_at(e, 1));
    if (p1 != ATP_TRACE_NONE) out[n++] = p1;
  }
  return n;
}

fn u32 thvm_atp_cp_label(AtpState *s) {
  if (s == NULL || s->n_cp_feat == 0u) return 0u;

  // Proof set seed: the RULES that join the goal, via the existing
  // single-NF proof extractor.  Each fired rule's r_trace[] is its
  // TRACE_ORIENT entry; the proof-relevant CPs are the transitive
  // trace-DAG ancestors of those entries.
  static AtpProofStep steps[ATP_PROOF_MAX_STEPS * 2u];
  u32 n_steps = thvm_atp_proof_extract(s, steps,
                                       ATP_PROOF_MAX_STEPS * 2u);
  if (n_steps == 0u) return 0u;   // not single-NF extractable

  // Reachable-trace bitset over [0, n_trace).  Marked entries are the
  // proof set: ancestors of every fired rule's TRACE_ORIENT.
  u8 *reach = (u8 *)calloc(s->n_trace, 1u);
  if (reach == NULL) return 0u;

  // Worklist DFS from each proof rule's TRACE_ORIENT trace id.
  u32 *stack = (u32 *)malloc((size_t)s->n_trace * sizeof(u32));
  if (stack == NULL) { free(reach); return 0u; }
  u32 sp = 0u;
  for (u32 k = 0; k < n_steps; k++) {
    u32 ru = steps[k].rule;
    if (ru >= s->n_rules) continue;
    u32 ti = s->r_trace[ru];
    if (ti != ATP_TRACE_NONE && ti < s->n_trace && !reach[ti]) {
      reach[ti] = 1u;
      stack[sp++] = ti;
    }
  }
  while (sp > 0u) {
    u32 ti = stack[--sp];
    u32 par[2];
    u32 np = atp_trace_parents(s, ti, par);
    for (u32 i = 0; i < np; i++) {
      u32 p = par[i];
      if (p < s->n_trace && !reach[p]) {
        reach[p] = 1u;
        stack[sp++] = p;
      }
    }
  }

  // Label each recorded selected-CP: 1 iff its trace id is in the
  // proof set.  Count distinct proof-relevant selected CPs.
  u32 n_pos = 0u;
  for (u32 i = 0; i < s->n_cp_feat; i++) {
    u32 ti = s->cp_feat_trace[i];
    u8 lab = (ti != ATP_TRACE_NONE && ti < s->n_trace && reach[ti]) ? 1u : 0u;
    s->cp_feat_label[i] = lab;
    if (lab) n_pos++;
  }

  free(stack);
  free(reach);
  return n_pos;
}

fn u32 thvm_atp_cp_dataset_append(const AtpState *s, const char *path,
                                  u8 header) {
  if (s == NULL || path == NULL || s->n_cp_feat == 0u) return 0u;
  FILE *f = fopen(path, "a");
  if (f == NULL) return 0u;
  if (header) {
    fprintf(f, "label");
    static const char *names[ATP_CP_FEATURE_DIM] = {
      "size_sum", "max_depth", "n_distinct_vars", "n_var_occ",
      "weight_add", "weight_gt", "weight_mix2", "goal_weight",
      "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub",
      "orientable", "unif_measure",
    };
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) fprintf(f, "\t%s", names[j]);
    fprintf(f, "\n");
  }
  for (u32 i = 0; i < s->n_cp_feat; i++) {
    const float *row = s->cp_feat_rows + (size_t)i * ATP_CP_FEATURE_DIM;
    fprintf(f, "%u", (unsigned)s->cp_feat_label[i]);
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) fprintf(f, "\t%.6g", row[j]);
    fprintf(f, "\n");
  }
  fclose(f);
  return s->n_cp_feat;
}

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

#ifdef ATP_MNF
// === Milestone 10: MNF -- the Multiple-Normal-Forms goal search =====
//
// A port of Waldmeister's MNF module.  The conjecture stops being a
// passive single-normal-form check and becomes a goal search: goal_lhs
// seeds a GREEN front, goal_rhs a RED one.  Each front *normalises* its
// term forward (l->r) with the current rule set R; because R is not
// confluent a term has many forward reducts, so each front is a set,
// held in a hash table.  When a reduct collides with a term already in
// the table of the OPPOSITE colour the fronts have met: goal_lhs and
// goal_rhs share a rewrite path, so the goal is proved.
//
// The set is fed incrementally -- each rule completion derives is
// applied to the already-reached terms -- so progress comes from
// completion growing R until the fronts' forward reducts coincide.
//
// Search order is Waldmeister's irreducible-adaptive deque: expand
// depth-first (newest node) while reductions stay productive, switch
// to breadth-first (oldest node) the moment the last node expanded was
// irreducible -- a normal form, a dead end (see mnf_pop / mnf_step).
//
// Backward "anti" steps (r->l): see MNF_MAX_ANTI.

#define MNF_RED        0u
#define MNF_GREEN      1u
// Backward "anti" steps (r->l) -- a port of Waldmeister's antiWOVar:
// variable-safe backward rewriting through unorientable equations,
// capped at MNF_MAX_ANTI per lineage.  Forward-only fronts (anti=0)
// cannot close a symmetric goal and rely on completion alone, which
// for NAND commutativity from the single Wolfram axiom diverges.
// anti=2 is the minimum that closes wolfram.pr (anti=1 does not;
// anti>=2 does), and is the default.  Overridable with
// -DMNF_MAX_ANTI=N.
#ifndef MNF_MAX_ANTI
#define MNF_MAX_ANTI   2u
#endif
#define MNF_MAX_NODES  400000u
#define MNF_SUCC_CAP   2048u
// First-expansion nodes per goal_check.  The collision step is gated
// by completion (the fronts cannot join before completion derives the
// enabling rule), NOT by this budget: an A/B sweep 8..384 on wolfram
// moved the proving step only 363..377 while wall time scaled
// linearly with the budget.  A large budget is therefore pure wasted
// MNF work -- it just grows the front (and the cost of re-expanding
// it against every new rule) faster than completion can use it.  16
// keeps the front growth proportional to completion's pace; wolfram
// 15.2 s -> 1.8 s.
#define MNF_BUDGET     16u
#define MNF_N_BUCKETS  (1u << 21)        // 2097152, power of two
#define MNF_ROOT_PARENT 0xFFFFFFFFu      // mnf_insert: this term is a seed

typedef struct {
  Term term;
  u32  hash;
  u32  parent;     // node this term was first reached from (root: self)
  u8   colour;     // MNF_RED / MNF_GREEN
  u8   anti;       // backward steps used on the path to this term
  u8   expanded;   // successors already generated against [0, n_rules_seen)
  u8   irred;      // 1 until a forward (size-reducing) successor is found
} MnfNode;

typedef struct AtpMnf {
  MnfNode *nodes;
  u32      n_nodes;
  u32      cap_nodes;
  // Each front is a deque of pending node indices.  Successors are
  // pushed on the left; mnf_pop takes the left (newest, depth-first)
  // or right (oldest, breadth-first) end per the irred-adaptive policy.
  u32     *qred;
  u32     *qgreen;
  u32      qred_head, qred_tail;       // live deque span is q[head, tail)
  u32      qgreen_head, qgreen_tail;
  u32      q_cap;
  u8       red_last_irred;             // last RED node expanded was a NF
  u8       green_last_irred;
  u32     *buckets;          // open addressing: bucket -> node_idx + 1
  u32      n_buckets;
  u32      n_rules_seen;     // rules already fed in
  Term     seed_red;         // goal_rhs -- the RED front's origin
  Term     seed_green;       // goal_lhs -- the GREEN front's origin
  u32      n_red, n_green;   // nodes reached per colour
  u32      n_anti;           // nodes reached via a backward (r->l) step
  u32      n_dup;            // same-colour duplicate terms (dropped)
  u32      n_trunc;          // node expansions that hit MNF_SUCC_CAP
  u8       full;             // node table reached MNF_MAX_NODES
  u8       joined;
  // The join witness (also used by the proof extractor, so always
  // present, not behind ATP_MNF_DIAG).  Side A is the existing table
  // node a fresh reduct collided with; side B is that fresh reduct --
  // never created as a node, so it is identified by its parent and the
  // colliding term.  meet_term == nodes[meet_a].term == the reduct.
  u32      meet_a;           // existing table node the join collided with
  u32      meet_b_parent;    // parent of the colliding (uncreated) node
  Term     meet_term;        // the term both fronts reached
  u8       meet_b_col;       // colour of the colliding (uncreated) node
} AtpMnf;

// Structural hash of a term (FNV-ish mix over the preorder).
static u32 mnf_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ mnf_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// Find a node whose term is kbo_eq to `t` (hash `h`); MNF_MAX_NODES if
// absent.  The table holds one node per distinct term -- the first
// colour to reach it.
static u32 mnf_lookup(const AtpMnf *m, Term t, u32 h) {
  u32 mask = m->n_buckets - 1u;
  for (u32 probe = h & mask; ; probe = (probe + 1u) & mask) {
    u32 slot = m->buckets[probe];
    if (slot == 0u) return MNF_MAX_NODES;
    u32 idx = slot - 1u;
    if (m->nodes[idx].hash == h && kbo_eq(m->nodes[idx].term, t)) return idx;
  }
}

// Insert `t` (colour `col`, lineage anti-count `anti`, reached from node
// `parent` -- MNF_ROOT_PARENT for a seed).  Returns 1 if this insertion
// JOINED the fronts (an opposite-colour node already held `t`).  Same-
// colour duplicate -> dropped.  Fresh term -> added, indexed, enqueued.
static int mnf_insert(AtpMnf *m, Term t, u8 col, u8 anti, u32 parent) {
  u32 h   = mnf_hash(t);
  u32 idx = mnf_lookup(m, t, h);
  if (idx != MNF_MAX_NODES) {
    if (m->nodes[idx].colour != col) {
      m->meet_a        = idx;
      m->meet_b_parent = parent;
      m->meet_term     = t;
      m->meet_b_col    = col;
      m->joined = 1u;
      return 1;
    }
    m->n_dup++;
    return 0;
  }
  if (m->n_nodes >= MNF_MAX_NODES) { m->full = 1u; return 0; }   // set full
  u32 ni = m->n_nodes++;
  m->nodes[ni].term     = t;
  m->nodes[ni].hash     = h;
  m->nodes[ni].parent   = (parent == MNF_ROOT_PARENT) ? ni : parent;
  m->nodes[ni].colour   = col;
  m->nodes[ni].anti     = anti;
  m->nodes[ni].expanded = 0u;
  m->nodes[ni].irred    = 1u;   // until a forward successor proves otherwise
  if (col == MNF_RED) m->n_red++; else m->n_green++;
  if (anti > 0u) m->n_anti++;
  u32 mask = m->n_buckets - 1u;
  u32 probe = h & mask;
  while (m->buckets[probe] != 0u) probe = (probe + 1u) & mask;
  m->buckets[probe] = ni + 1u;
  // Push left onto the colour's deque (each node enqueued exactly once).
  if (col == MNF_RED) m->qred[--m->qred_head]     = ni;
  else                m->qgreen[--m->qgreen_head] = ni;
  return 0;
}

// Per-rule caches, refreshed once per mnf_step over the whole rule
// set so the successor recursion reads them instead of recomputing:
//   g_mnf_vc   -- vars(lhs) subset of vars(rhs) (variable-safe backward)
//   g_mnf_ln   -- nodes(lhs[j]) -- the forward-match size pre-filter
//   g_mnf_rn   -- nodes(rhs[j]) -- the backward-match size pre-filter
// A one-way match needs nodes(pattern) <= nodes(subject), so a rule
// whose matched side outsizes the term cannot fire -- the integer
// compare skips the thvm_match entirely.
static u8  *g_mnf_vc     = NULL;
static u32 *g_mnf_ln     = NULL;
static u32 *g_mnf_rn     = NULL;
static u32  g_mnf_vc_cap = 0u;

// One-step rewrites of `t` (all positions, rules [rule_lo, rule_hi),
// forward + -- when allow_anti -- variable-safe backward) collected
// into mnf_succ_buf / mnf_succ_anti.
static Term mnf_succ_buf[MNF_SUCC_CAP];
static u8   mnf_succ_anti[MNF_SUCC_CAP];

// Returns nodes(t).  Children are expanded first so the size is known
// when rules are tried at t -- the size pre-filter then skips a rule
// whose matched side has more nodes than t (a one-way match needs
// nodes(pattern) <= nodes(subject), so such a rule provably fails).
// Successor order in the buffer differs from a rules-first walk, but
// every successor is still inserted, so the MNF set is unchanged.
static u32 mnf_successors(AtpState *s, Term t, u8 allow_anti,
                          u32 rule_lo, u32 rule_hi, u32 *n) {
  // Throttled wall-deadline + host-abort poll.  mnf_successors recurses
  // into subterms and iterates n_rules per subterm; a single call can
  // run for seconds on a deep term against a large rule set, well past
  // any mnf_step / mnf_loop_guard polling cadence.  Cap call count via
  // a static tick (every 1024th call probes the clock so the poll is
  // sub-microsecond amortized) and short-circuit on deadline by
  // returning the current count -- the caller treats truncated
  // successors as a terminal expansion, so the front search bails and
  // thvm_atp_step returns ATP_TIMEOUT on the next outer iteration.
  // Without this, the engine ignores TimeConstraint on hard Sheffer
  // cross-axiom Implies-X goals where MNF expands a degenerate front.
  {
    static u32 mnf_succ_tick = 0u;
    if ((++mnf_succ_tick & 0x3FFu) == 0u) {
      if (s->wall_deadline_us != 0u) {
        u64 now = atp_now_us();
        if (now != 0u && now >= s->wall_deadline_us) return 0u;
      }
      if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 0u;
    }
  }
  u32 size = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 m = term_ctr_n(t);
    if (m > REWRITE_MAX_ARITY) return 0xFFFFFFu;  // unreachable; disable filter
    for (u32 i = 0; i < m; i++) {
      u32 base = *n;
      size += mnf_successors(s, term_ctr_at(t, i), allow_anti,
                             rule_lo, rule_hi, n);
      for (u32 k = base; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < m; c++) {
          ch[c] = (c == i) ? mnf_succ_buf[k] : term_ctr_at(t, c);
        }
        mnf_succ_buf[k] = term_new_ctr(term_ext(t), ch, m);
      }
    }
  }
  // `size` is now nodes(t) -- try every rule at t, size-filtered.
  for (u32 j = rule_lo; j < rule_hi && *n < MNF_SUCC_CAP; j++) {
    if (g_mnf_ln[j] <= size) {
      RewriteSubst sub = {{0}};
      if (thvm_match(s->lhs[j], t, &sub)) {
        mnf_succ_buf[*n]  = thvm_subst_apply(s->rhs[j], &sub);
        mnf_succ_anti[*n] = 0u;
        (*n)++;
        if (*n >= MNF_SUCC_CAP) return size;
      }
    }
    if (allow_anti && g_mnf_vc[j] && g_mnf_rn[j] <= size) {
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        mnf_succ_buf[*n]  = thvm_subst_apply(s->lhs[j], &sb);
        mnf_succ_anti[*n] = 1u;
        (*n)++;
      }
    }
  }
  return size;
}

// Generate node `ni`'s successors against rules [rule_lo, rule_hi) and
// insert each (same colour, anti-count bumped for a backward step).  A
// node that yields at least one forward (size-reducing) rewrite is
// reducible -- its `irred` flag is cleared; one that yields none is a
// normal form and stays irreducible.
static void mnf_expand_node(AtpState *s, AtpMnf *m, u32 ni,
                            u32 rule_lo, u32 rule_hi) {
  u8   col  = m->nodes[ni].colour;
  u8   anti = m->nodes[ni].anti;
  Term t    = m->nodes[ni].term;
  u32  n    = 0u;
  (void)mnf_successors(s, t, (u8)(anti < MNF_MAX_ANTI), rule_lo, rule_hi, &n);
  if (n >= MNF_SUCC_CAP) m->n_trunc++;   // successor buffer overflowed
  for (u32 k = 0; k < n && !m->joined; k++) {
    if (mnf_succ_anti[k] == 0u) m->nodes[ni].irred = 0u;   // forward redex
    mnf_insert(m, mnf_succ_buf[k], col, (u8)(anti + mnf_succ_anti[k]), ni);
  }
}

// Create the MNF set for the current goal: goal_lhs -> GREEN front,
// goal_rhs -> RED front.  An immediate collision (goal_lhs kbo_eq
// goal_rhs) already joins.
static AtpMnf *mnf_create(AtpState *s) {
  AtpMnf *m = (AtpMnf *)calloc(1, sizeof(AtpMnf));
  if (m == NULL) return NULL;
  m->cap_nodes = MNF_MAX_NODES;
  m->q_cap     = MNF_MAX_NODES;
  m->n_buckets = MNF_N_BUCKETS;
  m->nodes   = (MnfNode *)calloc(m->cap_nodes, sizeof(MnfNode));
  m->qred    = (u32 *)calloc(m->q_cap, sizeof(u32));
  m->qgreen  = (u32 *)calloc(m->q_cap, sizeof(u32));
  m->buckets = (u32 *)calloc(m->n_buckets, sizeof(u32));
  if (m->nodes == NULL || m->qred == NULL || m->qgreen == NULL ||
      m->buckets == NULL) {
    free(m->nodes); free(m->qred); free(m->qgreen);
    free(m->buckets); free(m);
    return NULL;
  }
  // Deques start empty at the right end -- mnf_insert pushes left.
  m->qred_head   = m->qred_tail   = m->q_cap;
  m->qgreen_head = m->qgreen_tail = m->q_cap;
  m->seed_green = s->goal_lhs;     // the GREEN front's origin
  m->seed_red   = s->goal_rhs;     // the RED front's origin
  mnf_insert(m, s->goal_lhs, MNF_GREEN, 0u, MNF_ROOT_PARENT);
  mnf_insert(m, s->goal_rhs, MNF_RED,   0u, MNF_ROOT_PARENT);
  return m;
}

static void mnf_destroy(struct AtpMnf *m) {
  if (m == NULL) return;
  free(m->nodes); free(m->qred); free(m->qgreen); free(m->buckets); free(m);
}

// GC support: the coloured nodes' terms are collector roots.  Gather
// them for thvm_atp_gc_collect and write the relocated terms back; the
// hash table (structural hashes, node indices) is GC-invariant.
static u32 mnf_gc_count(struct AtpMnf *m) {
  return (m == NULL) ? 0u : m->n_nodes;
}
static void mnf_gc_gather(struct AtpMnf *m, Term *roots, u32 *w) {
  if (m == NULL) return;
  for (u32 i = 0; i < m->n_nodes; i++) roots[(*w)++] = m->nodes[i].term;
}
static void mnf_gc_writeback(struct AtpMnf *m, const Term *roots, u32 base) {
  if (m == NULL) return;
  for (u32 i = 0; i < m->n_nodes; i++) m->nodes[i].term = roots[base + i];
}

// Take a node from a colour's deque.  Waldmeister's irreducible-
// adaptive policy: if the last node expanded for this colour was
// irreducible (a normal form -- a dead end), pop the OLDEST node
// (right end, breadth-first -- go try elsewhere); otherwise pop the
// NEWEST (left end, depth-first -- keep driving the reduction down).
static u32 mnf_pop(u32 *q, u32 *head, u32 *tail, u8 last_irred) {
  if (last_irred) return q[--(*tail)];   // FIFO end: oldest
  return q[(*head)++];                   // LIFO end: newest
}

#ifdef ATP_MNF_DIAG
// Independent join verifier.  A join is sound because every MNF node is,
// by construction, a one-step equational rewrite of its parent (forward
// l->r or variable-safe backward r->l) -- so goal_lhs ->* meet <-* goal_rhs
// is a closed equational chain.  This re-checks that claim from the parent
// pointers: walk `ni` up to its root and confirm each child term is a
// genuine one-step rewrite of its parent under the FINAL rule set.  Steps
// taken with a rule that interreduction has since retired will not replay
// (still equationally valid -- just no longer in R); `*replayed` counts
// the ones that do.  Returns the root node index.
static u32 mnf_verify_chain(AtpState *s, AtpMnf *m, u32 ni,
                            u32 *len, u32 *replayed) {
  u32 guard = 0u;
  while (m->nodes[ni].parent != ni && guard++ < m->n_nodes) {
    u32 p = m->nodes[ni].parent;
    u32 n = 0u;
    mnf_successors(s, m->nodes[p].term, 1u, 0u, s->n_rules, &n);
    for (u32 k = 0; k < n; k++) {
      if (kbo_eq(mnf_succ_buf[k], m->nodes[ni].term)) { (*replayed)++; break; }
    }
    (*len)++;
    ni = p;
  }
  return ni;
}

// Re-check the join captured by mnf_insert and report to stderr.
static void mnf_verify(AtpState *s, AtpMnf *m) {
  u32 lenA = 0u, repA = 0u;
  u32 rootA = mnf_verify_chain(s, m, m->meet_a, &lenA, &repA);
  // Side B's node was never created -- meet_term is a one-step rewrite of
  // meet_b_parent; verify that link, then walk meet_b_parent to its root.
  u32 nB = 0u, lenB = 1u, repB = 0u;
  mnf_successors(s, m->nodes[m->meet_b_parent].term, 1u, 0u, s->n_rules, &nB);
  for (u32 k = 0; k < nB; k++) {
    if (kbo_eq(mnf_succ_buf[k], m->meet_term)) { repB++; break; }
  }
  u32 rootB = mnf_verify_chain(s, m, m->meet_b_parent, &lenB, &repB);
  u8   colA      = m->nodes[m->meet_a].colour;
  Term grootA    = m->nodes[rootA].term;
  Term grootB    = m->nodes[rootB].term;
  // colA owns side A; meet_b_col (opposite) owns side B.
  int roots_ok = (colA == MNF_GREEN)
      ? (kbo_eq(grootA, m->seed_green) && kbo_eq(grootB, m->seed_red))
      : (kbo_eq(grootA, m->seed_red)   && kbo_eq(grootB, m->seed_green));
  u32 green_len = (colA == MNF_GREEN) ? lenA : lenB;
  u32 red_len   = (colA == MNF_GREEN) ? lenB : lenA;
  u32 green_rep = (colA == MNF_GREEN) ? repA : repB;
  u32 red_rep   = (colA == MNF_GREEN) ? repB : repA;
  fprintf(stderr,
      "[mnf] JOIN: meet has %u symbols; green-side chain %u step(s) "
      "(%u replay under final R), red-side chain %u step(s) (%u replay); "
      "chain roots == goal: %s\n",
      atp_symbol_count(m->meet_term),
      green_len, green_rep, red_len, red_rep,
      roots_ok ? "YES" : "NO -- BUG");
}
#endif /* ATP_MNF_DIAG */

// One MNF advance: (a) feed any rules completion derived since the last
// call to every already-expanded node; (b) expand up to `budget` queued
// (first-expansion) nodes against the full current R.  Returns 1 once
// the fronts have joined.
// Per-node guard for the MNF expansion loops.  Returns 1 when the loop
// must stop, for either of two reasons:
//
//  - Wall deadline.  A single mnf_step can re-expand a large node table
//    against many newly-derived rules without ever returning to
//    thvm_atp_step's per-step wall check, so a hard goal can run far
//    past TimeConstraint inside one mnf_step.  Poll the deadline here
//    (throttled to every 256th call, since clock_gettime per node over
//    a 400k-node table is pure overhead) and bail; goal_check returns
//    and the next step iteration reports ATP_TIMEOUT.
//  - Heap pressure.  Between node expansions every live MNF term is
//    parked in m->nodes (rooted by thvm_atp_gc_collect) and no
//    expand-local term is allocated yet, so a Cheney collection here is
//    safe.  If heap is still under pressure after a collection -- the
//    live working set alone exceeds the half-space -- the front must
//    stop growing or it would exhaust from-space and abort the process;
//    set m->full so the caller stops.  Completion keeps running and the
//    goal still closes by single-NF or a later collision.
static int mnf_loop_guard(AtpState *s, AtpMnf *m) {
  static u32 tick = 0u;
  if ((++tick & 0xFFu) == 0u) {
    if (s->wall_deadline_us != 0u) {
      u64 now = atp_now_us();
      if (now != 0u && now >= s->wall_deadline_us) return 1;
    }
    if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 1;
  }
  if (!atp_heap_under_pressure()) return 0;
  thvm_atp_gc_collect(s);
  if (atp_heap_under_pressure()) { m->full = 1u; return 1; }
  return 0;
}

static int mnf_step(AtpState *s, AtpMnf *m, u32 budget) {
  if (m->joined) return 1;
  // Refresh the per-rule caches (vars-contained flag + lhs/rhs node
  // counts) for the whole current rule set -- mnf_successors reads
  // them instead of recomputing at every node it expands this call.
  if (s->n_rules > g_mnf_vc_cap) {
    u32 cap = g_mnf_vc_cap ? g_mnf_vc_cap : 256u;
    while (cap < s->n_rules) cap *= 2u;
    u8  *nv = (u8  *)realloc(g_mnf_vc, cap);
    u32 *nl = (u32 *)realloc(g_mnf_ln, cap * sizeof(u32));
    u32 *nr = (u32 *)realloc(g_mnf_rn, cap * sizeof(u32));
    if (nv == NULL || nl == NULL || nr == NULL) thvm_fatal("mnf_step: rule cache OOM");
    g_mnf_vc = nv; g_mnf_ln = nl; g_mnf_rn = nr; g_mnf_vc_cap = cap;
  }
  for (u32 j = 0; j < s->n_rules; j++) {
    g_mnf_vc[j] = (u8)atp_vars_contained(s->lhs[j], s->rhs[j]);
    g_mnf_ln[j] = atp_symbol_count(s->lhs[j]);
    g_mnf_rn[j] = atp_symbol_count(s->rhs[j]);
  }
#ifdef ATP_MNF_DIAG
  { static u32 c = 0;
    if (c++ % 16u == 0u) {
      fprintf(stderr,
        "[mnf] call=%-6u nodes=%-7u red=%-7u green=%-7u "
        "queue(r=%u g=%u) anti=%u dup=%u trunc=%u full=%u rules=%u/%u\n",
        c, m->n_nodes, m->n_red, m->n_green,
        m->qred_tail - m->qred_head, m->qgreen_tail - m->qgreen_head,
        m->n_anti, m->n_dup, m->n_trunc, m->full,
        m->n_rules_seen, s->n_rules);
    } }
#endif
  if (s->n_rules > m->n_rules_seen) {
    u32 lo = m->n_rules_seen, hi = s->n_rules, upto = m->n_nodes;
    for (u32 ni = 0; ni < upto && !m->joined; ni++) {
      if (m->nodes[ni].expanded) {
        if (mnf_loop_guard(s, m)) break;
        mnf_expand_node(s, m, ni, lo, hi);
      }
    }
    m->n_rules_seen = hi;
  }
  // (b) expand the two fronts in alternation -- one node each per
  // round -- taking each colour's node by the irred-adaptive deque
  // policy (mnf_pop).  A node's `irred` is settled by mnf_expand_node;
  // it feeds the next pop of the same colour.
  for (u32 b = 0; b < budget && !m->joined; b++) {
    if (mnf_loop_guard(s, m)) break;
    int did = 0;
    if (m->qred_head < m->qred_tail) {
      u32 ni = mnf_pop(m->qred, &m->qred_head, &m->qred_tail,
                       m->red_last_irred);
      if (!m->nodes[ni].expanded) {
        mnf_expand_node(s, m, ni, 0u, s->n_rules);
        m->nodes[ni].expanded = 1u;
      }
      m->red_last_irred = m->nodes[ni].irred;
      did = 1;
    }
    if (!m->joined && m->qgreen_head < m->qgreen_tail) {
      u32 ni = mnf_pop(m->qgreen, &m->qgreen_head, &m->qgreen_tail,
                       m->green_last_irred);
      if (!m->nodes[ni].expanded) {
        mnf_expand_node(s, m, ni, 0u, s->n_rules);
        m->nodes[ni].expanded = 1u;
      }
      m->green_last_irred = m->nodes[ni].irred;
      did = 1;
    }
    if (!did) break;
  }
#ifdef ATP_MNF_DIAG
  if (m->joined) mnf_verify(s, m);
#endif
  return m->joined;
}

// === MNF proof extraction ============================================
//
// A join is goal_lhs ->* meet <-* goal_rhs: every MNF node is a one-step
// equational rewrite of its parent (forward l->r, or variable-safe
// backward r->l), so walking the two parent chains up from `meet` to the
// two seeds reconstructs a closed equational chain.  This is the
// goal-directed analog of atp_proof_record_side: it fills AtpProofStep[]
// with the same {side, rule, fwd, pos, before, after} shape the WL
// ProofObject builder already consumes, so the symmetric goal joins the
// same dataset machinery as a single-NF proof.
//
// GREEN side terms are emitted as side 0 (the goal_lhs chain), RED as
// side 1 (the goal_rhs chain); both chains run goal -> meet, so the
// assembled equation reaches `meet == meet` -- exactly the NF == NF
// tautology the single-NF path reaches, with `meet` as the common form.

// Find the one-step rewrite of `before` that yields `after` using the
// rule slice (rl[j] -> rr[j], j in [0,nr)): try every rule (forward
// l->r, and -- gated by vars-contained -- backward r->l) at every
// position, returning the redex path / rule / direction of the first
// match.  Mirrors mnf_successors' search but stops at the edge whose
// result is kbo_eq to `after`.  `out_rule` indexes into the slice.
// Returns 1 on success.
static int mnf_edge_step(Term before, Term after,
                         const Term *rl, const Term *rr, u32 nr,
                         u8 *pos, u8 depth,
                         u32 *out_rule, u8 *out_pos_len, u8 *out_fwd) {
  for (u32 j = 0; j < nr; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(rl[j], before, &sub)) {                     // forward l->r
      Term repl = thvm_subst_apply(rr[j], &sub);
      if (kbo_eq(repl, after)) {
        *out_rule = j; *out_pos_len = depth; *out_fwd = 1u; return 1;
      }
    }
    if (atp_vars_contained(rl[j], rr[j])) {                    // backward r->l
      RewriteSubst sb = {{0}};
      if (thvm_match(rr[j], before, &sb)) {
        Term repl = thvm_subst_apply(rl[j], &sb);
        if (kbo_eq(repl, after)) {
          *out_rule = j; *out_pos_len = depth; *out_fwd = 0u; return 1;
        }
      }
    }
  }
  // Descend: the rewrite was below the top.  `before` and `after` agree
  // everywhere except the one child holding the redex.
  if (term_tag(before) == TAG_CTR && term_tag(after) == TAG_CTR &&
      term_ext(before) == term_ext(after) &&
      term_ctr_n(before) == term_ctr_n(after) &&
      depth < ATP_PROOF_MAX_DEPTH) {
    u32 nc = term_ctr_n(before);
    for (u32 i = 0; i < nc; i++) {
      Term cb = term_ctr_at(before, i);
      Term ca = term_ctr_at(after, i);
      if (kbo_eq(cb, ca)) continue;     // unchanged child: not the redex
      pos[depth] = (u8)i;
      if (mnf_edge_step(cb, ca, rl, rr, nr, pos, (u8)(depth + 1u),
                        out_rule, out_pos_len, out_fwd)) {
        return 1;
      }
    }
  }
  return 0;
}

// Build the HISTORICAL rule set: every equation that ever entered R,
// not just the rules surviving in s->lhs/s->rhs after interreduction.
// An MNF path step taken with a rule interreduction later retired only
// replays against this superset.  Sources: the live rules plus every
// TRACE_ORIENT / TRACE_AXIOM / TRACE_SIMPLIFY trace entry's (lhs, rhs).
// Caller-owned out_l / out_r arrays of capacity `cap`; returns count.
// Build the historical rule set (live rules + every ORIENT/AXIOM/
// SIMPLIFY equation that ever entered R via the trace).  out_trace[i]
// records the TRACE index that resolves slot i, so the WL extractor can
// resolve an MNF step's cited rule through resolveTrace exactly as it
// resolves a completion MainStep: a live derived rule lands on its
// critical-pair lemma, a retired rule on its own trace node.
static u32 mnf_historical_rules(AtpState *s, Term *out_l, Term *out_r,
                                u32 *out_trace, u32 cap) {
  u32 n = 0u;
  for (u32 i = 0; i < s->n_rules && n < cap; i++) {
    out_l[n] = s->lhs[i]; out_r[n] = s->rhs[i];
    out_trace[n] = s->r_trace[i];
    n++;
  }
  for (u32 i = 0; i < s->n_trace && n < cap; i++) {
    Term e = s->trace[i];
    u32  reason = term_ext(e);
    if (reason == TRACE_ORIENT || reason == TRACE_AXIOM ||
        reason == TRACE_SIMPLIFY || reason == TRACE_FVI) {
      out_l[n] = term_ctr_at(e, 2);
      out_r[n] = term_ctr_at(e, 3);
      out_trace[n] = i;
      n++;
    }
  }
  return n;
}

// Walk node `ni` up to its seed, collecting the chain of terms
// root..ni into buf[0..*len) (root first).  Returns the root index.
static u32 mnf_collect_lineage(AtpMnf *m, u32 ni, u32 *buf, u32 *len,
                               u32 cap) {
  u32 stack[ATP_PROOF_MAX_STEPS + 2u];
  u32 sp = 0u;
  u32 guard = 0u;
  while (sp < cap && sp < (ATP_PROOF_MAX_STEPS + 2u) &&
         guard++ < m->n_nodes + 1u) {
    stack[sp++] = ni;
    if (m->nodes[ni].parent == ni) break;
    ni = m->nodes[ni].parent;
  }
  // stack holds ni..root (leaf first); reverse into buf (root first).
  for (u32 i = 0; i < sp; i++) buf[i] = stack[sp - 1u - i];
  *len = sp;
  return ni;
}

// Emit the steps along a lineage (terms[0..n_terms), seed/root first)
// into out[], tagged with `side`.  Each consecutive pair is one
// rewrite, reconstructed by mnf_edge_step against the (rl, rr, nr)
// rule slice.  Both fronts are emitted seed -> meet, so the GREEN and
// RED chains meet at the join term.  Returns 1 if every edge replayed.
static int mnf_emit_lineage(AtpMnf *m, const u32 *terms,
                            u32 n_terms, u32 side,
                            const Term *rl, const Term *rr,
                            const u32 *rtr, u32 nr,
                            AtpProofStep *out, u32 cap, u32 *n) {
  for (u32 k = 0; k + 1u < n_terms; k++) {
    Term before = m->nodes[terms[k]].term;
    Term after  = m->nodes[terms[k + 1u]].term;
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0u; u8 pos_len = 0u, fwd = 1u;
    if (!mnf_edge_step(before, after, rl, rr, nr,
                       pos, 0u, &rule, &pos_len, &fwd)) {
      return 0;   // an edge replays against no historical rule
    }
    if (*n >= cap) return 0;
    AtpProofStep *st = &out[*n];
    st->side    = side;
    // rtr[rule] is the TRACE index of the cited historical rule, so WL
    // resolves it via resolveTrace just like a completion MainStep.
    st->rule    = rtr[rule];
    st->fwd     = fwd;
    st->pos_len = pos_len;
    for (u8 p = 0; p < pos_len; p++) st->pos[p] = pos[p];
    st->before  = before;
    st->after   = after;
    (*n)++;
  }
  return 1;
}

// Reconstruct the MNF join as a two-sided AtpProofStep chain.  Side A is
// the existing table node meet_a; side B is the fresh reduct (meet_term)
// of meet_b_parent that collided with it.  Both A and B equal meet_term,
// so each chain runs seed -> meet.  GREEN -> side 0, RED -> side 1.
// Returns the step count, 0 if no join / not reconstructable.
fn u32 thvm_atp_mnf_proof_extract(AtpState *s, AtpProofStep *out, u32 cap) {
  if (s == NULL || out == NULL || cap == 0u) return 0;
  AtpMnf *m = s->mnf;
  if (m == NULL || !m->joined) return 0;

  // Lineage A: meet_a up to its seed.
  static u32 lin_a[ATP_PROOF_MAX_STEPS + 2u];
  static u32 lin_b[ATP_PROOF_MAX_STEPS + 2u];
  u32 len_a = 0u, len_b = 0u;
  mnf_collect_lineage(m, m->meet_a, lin_a, &len_a, ATP_PROOF_MAX_STEPS + 2u);

  // Lineage B: the colliding reduct was never created as a node.  Walk
  // its PARENT to the seed, then append a synthetic final node holding
  // meet_term.  We reuse meet_b_parent's node slot terms; the final
  // edge (meet_b_parent.term -> meet_term) is emitted explicitly.
  mnf_collect_lineage(m, m->meet_b_parent, lin_b, &len_b,
                      ATP_PROOF_MAX_STEPS + 1u);

  u8 col_a = m->nodes[m->meet_a].colour;     // colour that owns lineage A
  // meet_b_col is the opposite colour (the fresh reduct's colour).
  u32 side_a = (col_a == MNF_GREEN) ? 0u : 1u;
  u32 side_b = (m->meet_b_col == MNF_GREEN) ? 0u : 1u;

  // Reconstruct each edge against the HISTORICAL rule set (live rules +
  // every equation that ever entered R via the trace DAG), so an edge
  // taken with a since-retired rule still replays.
  u32 hist_cap = s->n_rules + s->n_trace;
  Term *hist_l = (Term *)malloc((size_t)hist_cap * sizeof(Term));
  Term *hist_r = (Term *)malloc((size_t)hist_cap * sizeof(Term));
  u32  *hist_t = (u32  *)malloc((size_t)hist_cap * sizeof(u32));
  if (hist_l == NULL || hist_r == NULL || hist_t == NULL) {
    free(hist_l); free(hist_r); free(hist_t); return 0; }
  u32 nr = mnf_historical_rules(s, hist_l, hist_r, hist_t, hist_cap);

  u32 n = 0u;
#ifdef ATP_MNF_DIAG
  fprintf(stderr, "[mnf-extract] meet_a=%u len_a=%u meet_b_parent=%u "
          "len_b=%u col_a=%u meet_b_col=%u hist_rules=%u\n",
          m->meet_a, len_a, m->meet_b_parent, len_b, col_a, m->meet_b_col,
          nr);
#endif
  // An edge that replays against no historical rule leaves the lineage
  // not reconstructable as a forward/backward chain.  Return 0 (no
  // extractable proof) rather than emit a partial, unsound chain.
  if (!mnf_emit_lineage(m, lin_a, len_a, side_a, hist_l, hist_r, hist_t, nr,
                        out, cap, &n)) {
#ifdef ATP_MNF_DIAG
    fprintf(stderr, "[mnf-extract] lineage A unreplayable at step %u/%u\n",
            n, len_a > 0u ? len_a - 1u : 0u);
#endif
    free(hist_l); free(hist_r); free(hist_t); return 0;
  }
  if (!mnf_emit_lineage(m, lin_b, len_b, side_b, hist_l, hist_r, hist_t, nr,
                        out, cap, &n)) {
#ifdef ATP_MNF_DIAG
    fprintf(stderr, "[mnf-extract] lineage B unreplayable at step %u\n", n);
#endif
    free(hist_l); free(hist_r); free(hist_t); return 0;
  }
  // Final B edge: meet_b_parent.term -> meet_term (the collision).
  {
    Term before = m->nodes[m->meet_b_parent].term;
    Term after  = m->meet_term;
    if (!kbo_eq(before, after)) {
      u8  pos[ATP_PROOF_MAX_DEPTH];
      u32 rule = 0u; u8 pos_len = 0u, fwd = 1u;
      if (!mnf_edge_step(before, after, hist_l, hist_r, nr,
                         pos, 0u, &rule, &pos_len, &fwd)) {
        free(hist_l); free(hist_r); free(hist_t); return 0;
      }
      if (n >= cap) return 0;
      AtpProofStep *st = &out[n];
      st->side    = side_b;
      st->rule    = hist_t[rule];   // TRACE index, see mnf_emit_lineage
      st->fwd     = fwd;
      st->pos_len = pos_len;
      for (u8 p = 0; p < pos_len; p++) st->pos[p] = pos[p];
      st->before  = before;
      st->after   = after;
      n++;
    }
  }
  free(hist_l); free(hist_r); free(hist_t);
  return n;
}
#endif /* ATP_MNF */

// Goal-side normalization cap.  Generous (not the per-step CP cap
// of 64): a goal can close purely by normalization when the axioms
// generate no critical pairs -- e.g. combinatory-logic identities
// (B/C/W <-> S/K), whose rules are non-overlapping, so completion's
// CP queue is empty and the ONLY way the goal joins is by reducing
// both sides to a common normal form.  Deep combinator terms like
// S(S(K(S(KS)K))S)(KK) x y z take many hundreds of rewrites to
// normalize; a cap of 64 left them un-joined (reported QUEUE_EMPTY
// with the goal actually provable).  A terminating rewrite system
// reaches fixpoint and returns early regardless of the bound, so
// the only risk is a non-terminating R (e.g. the Y axiom in scope),
// which the wall deadline already bounds.
#define ATP_GOAL_NORM_CAP (1u << 16)

// Universal-goal join check for ONE conjecture pair (gl, gr):
// normalize both sides under the current R and compare (flatterm
// fixpoint equality on the non-AC path, then kbo_eq, then AC-equality
// when an AC mask is live).  Writes the normal forms to *nf_l/*nf_r in
// every case; returns 1 iff the pair joins.  Shared by the single-goal
// path and the multi-goal conjunct loop -- the MNF front search is NOT
// part of this check (it is per-alias-goal and stateful).
static u8 atp_goal_pair_joined(AtpState *s, Term gl, Term gr,
                               Term *nf_l, Term *nf_r) {
  // FT path for AC-mask=0; Term path otherwise.  Earlier commit
  // 2133702a tried to unify on FT via ft_ac_eq, but ac-bool-idem-embed
  // still regresses to QUEUE_EMPTY iters=2 because the FT NF reaches
  // a different fixpoint than the Term NF on the f(x,x)=x reduction
  // trajectory.  Keep the Term-side path for AC workloads until the
  // FT normalize semantics match the Term-side leftmost-outermost
  // descent under AC.
  Term l, r;
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) {
    l = atp_rewrite_normalize(s, gl, s->lhs, s->rhs,
                              s->n_rules, ATP_GOAL_NORM_CAP);
    r = atp_rewrite_normalize(s, gr, s->lhs, s->rhs,
                              s->n_rules, ATP_GOAL_NORM_CAP);
  } else
#endif
  {
    atp_ft_mirror_ensure(s);
    AtpFt *gft = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fl_in = ft_from_term(gft, gl, 0);
    AtpFtCell *fr_in = ft_from_term(gft, gr, 0);
    AtpFtCell *fl = atp_rewrite_normalize_ft(s, fl_in, ATP_GOAL_NORM_CAP);
    AtpFtCell *fr = atp_rewrite_normalize_ft(s, fr_in, ATP_GOAL_NORM_CAP);
    l = ft_to_term(fl);
    r = ft_to_term(fr);
    if (ft_eq(fl, fr)) {
      *nf_l = l;
      *nf_r = r;
      return 1;
    }
  }
  *nf_l = l;
  *nf_r = r;
  if (kbo_eq(l, r)) return 1;
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) {
    AtpAcInfo ac = { .ac_mask = thvm_atp_get_ac_mask() };
    if (atp_ac_eq(l, r, &ac)) return 1;
  }
#endif
  return 0;
}

// Point the single-goal alias (goal_lhs/goal_rhs, their NFs, the FT
// mirror) at conjunct g.  When the alias actually MOVES to a different
// conjunct, the MNF front set is dropped: its GREEN/RED fronts were
// seeded from the previous alias goal and would keep searching for a
// join of the wrong conjunct.  goal_check recreates it lazily.
static void atp_goals_alias(AtpState *s, u32 g) {
  u8 moved = (s->goal_lhs != s->goals_lhs[g] ||
              s->goal_rhs != s->goals_rhs[g]);
  s->goal_lhs    = s->goals_lhs[g];
  s->goal_rhs    = s->goals_rhs[g];
  s->goal_lhs_nf = s->goals_lhs_nf[g];
  s->goal_rhs_nf = s->goals_rhs_nf[g];
  if (!moved) return;
#ifdef THVM_ATPFT_RULES
  s->goal_lhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, s->goal_lhs, 0);
  s->goal_rhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, s->goal_rhs, 0);
#endif
#ifdef ATP_MNF
  if (s->mnf != NULL) {
    mnf_destroy(s->mnf);
    s->mnf = NULL;
  }
#endif
}

// First conjunct whose goals_joined_mask bit is still clear.  Callers
// guarantee at least one unjoined goal (mask != all-set).
static u32 atp_goals_first_unjoined(const AtpState *s) {
  for (u32 g = 0; g < s->n_goals; g++) {
    if (!(s->goals_joined_mask & ((u64)1 << g))) return g;
  }
  return 0;
}

fn AtpStatus thvm_atp_goal_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;

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

  // Multi-goal conjunction: check every conjunct not yet joined.  A
  // join is LATCHED in goals_joined_mask -- R only grows under
  // completion, so a once-joined goal stays joinable; re-checking it
  // would only burn normalization work.  PROVED only when every bit
  // is set.
  if (s->n_goals > 1u) {
    const u64 all_mask = (s->n_goals >= 64u)
        ? ~0ull : (((u64)1 << s->n_goals) - 1ull);
    for (u32 g = 0; g < s->n_goals; g++) {
      if (s->goals_joined_mask & ((u64)1 << g)) continue;
      if (atp_goal_pair_joined(s, s->goals_lhs[g], s->goals_rhs[g],
                               &s->goals_lhs_nf[g], &s->goals_rhs_nf[g])) {
        s->goals_joined_mask |= (u64)1 << g;
      }
    }
    if (s->goals_joined_mask == all_mask) return ATP_PROVED;
    // Re-point the alias at the first unjoined conjunct so every
    // goal-directed heuristic (CPinGoal, goal-interleave, MNF) steers
    // toward a goal that still needs closing.
    atp_goals_alias(s, atp_goals_first_unjoined(s));
#ifdef ATP_MNF
    // The MNF front search runs against the CURRENT alias conjunct
    // only (its fronts are seeded from goal_lhs/goal_rhs);
    // atp_goals_alias above dropped a front set seeded from a
    // previously-aliased goal.  LOOP across conjuncts: each join
    // re-aliases to the next unjoined one (dropping the consumed
    // front set) and gives IT an MNF budget in the same call --
    // otherwise a conjunction whose every conjunct is MNF-only and
    // whose CP queue is already empty dies QUEUE_EMPTY after closing
    // just one conjunct per step.  Terminates: every successful
    // mnf_step latches one more mask bit; a failed one breaks.
    if (s->use_mnf) {
      for (;;) {
        if (s->mnf == NULL) s->mnf = mnf_create(s);
        if (s->mnf == NULL || !mnf_step(s, s->mnf, MNF_BUDGET)) break;
        s->goals_joined_mask |= (u64)1 << atp_goals_first_unjoined(s);
        if (s->goals_joined_mask == all_mask) return ATP_PROVED;
        atp_goals_alias(s, atp_goals_first_unjoined(s));
      }
    }
#endif
    return ATP_RUNNING;
  }

  if (atp_goal_pair_joined(s, s->goal_lhs, s->goal_rhs,
                           &s->goal_lhs_nf, &s->goal_rhs_nf)) {
    if (s->n_goals == 1u) {
      // Keep the (size-1) conjunct mirror coherent for consumers that
      // read per-goal state uniformly (wire reporting, proof side).
      s->goals_lhs_nf[0] = s->goal_lhs_nf;
      s->goals_rhs_nf[0] = s->goal_rhs_nf;
      s->goals_joined_mask = 1ull;
    }
    return ATP_PROVED;
  }
#ifdef ATP_MNF
  // Milestone 10: the MNF bidirectional search AUGMENTS the single-NF
  // check -- it does not replace it.  goal_lhs seeds a GREEN front,
  // goal_rhs a RED one; each rewrites with R; an opposite-colour
  // collision is the join.  This is the only detector that can prove a
  // goal whose two sides never share one normal form -- a symmetric
  // goal like commutativity nand(x,y)=nand(y,x), where neither side
  // rewrites to the other, so the single-NF check above structurally
  // cannot fire.  (An earlier revision had MNF REPLACE the single-NF
  // check; that regressed thm, which MNF's front search did not close
  // but the single-NF check does.  Both are sound -- run both.)
  //
  // Gated on s->use_mnf so the dylib can be COMPILED with -DATP_MNF
  // (MNF linked in) yet stay completion-only by default: the front
  // search runs only when the WL surface flips the flag for
  // Method -> "GoalDirected".  Off, this block is a single branch test.
  if (s->use_mnf) {
    if (s->mnf == NULL) s->mnf = mnf_create(s);
    if (s->mnf != NULL && mnf_step(s, s->mnf, MNF_BUDGET)) {
      if (s->n_goals == 1u) s->goals_joined_mask = 1ull;
      return ATP_PROVED;
    }
  }
#endif
  return ATP_RUNNING;
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
    atp_goals_release(s);
#ifdef THVM_ATPFT_RULES
    s->goal_lhs_ft = NULL;
    s->goal_rhs_ft = NULL;
#endif
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
  // Existential goals are single-conjecture only: drop any multi-goal
  // state a prior thvm_atp_set_goals left behind so goal_check takes
  // the narrow path, not the conjunct loop.
  atp_goals_release(s);
#ifdef THVM_ATPFT_RULES
  // Stage 4: mirror the goal on the AtpFt side (same persistence /
  // arena story as thvm_atp_set_goal).
  s->goal_lhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, lhs, 0);
  s->goal_rhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, rhs, 0);
#endif
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
#ifdef ATP_ORPHAN_KILL
// 9b: orphan deletion (Waldmeister's "Waisenmord", orphan murder).
// When interreduce drops a rule, the queued CPs descended from it are
// redundant -- the re-queued reduced equation regenerates whatever
// they would contribute.  `dead` holds the trace ids of the dropped
// rules; a queued CP is an orphan iff its TRACE_CP entry names a dead
// rule as a parent.  Survivors are compacted down and the queue heap
// / FV index rebuilt via thvm_atp_cp_reheapify.
static void atp_cp_kill_orphans(AtpState *s, const u32 *dead, u32 n_dead) {
  if (s == NULL || n_dead == 0 || s->n_cps == 0) return;
  u32 w = 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    int orphan = 0;
    u32 ti = s->cp_trace[i];
    if (ti != ATP_TRACE_NONE && ti < s->n_trace) {
      Term te = s->trace[ti];
      if (term_tag(te) == TAG_CTR && term_ext(te) == TRACE_CP) {
        u32 pa = (u32)term_val(term_ctr_at(te, 0));
        u32 pb = (u32)term_val(term_ctr_at(te, 1));
        for (u32 d = 0; d < n_dead; d++) {
          if (pa == dead[d] || pb == dead[d]) { orphan = 1; break; }
        }
      }
    }
    if (orphan) {
      // Drop the orphan -- free its byte string (the queue owns it).
      // free(NULL) no-op for deferred slots; their tag bit drops too.
      free(s->cp_packed[i]);
      s->cp_packed[i] = NULL;
      atp_cp_implicit_clear(s, i);
      continue;
    }
    if (w != i) {
      s->cp_packed[w] = s->cp_packed[i];
      s->cp_packed[i] = NULL;
      s->cp_trace[w]  = s->cp_trace[i];
      s->cp_pri[w]    = s->cp_pri[i];
      s->cp_seq[w]    = s->cp_seq[i];
      s->cp_goal[w]   = s->cp_goal[i];
      // cp_pri2 must travel too: the trailing reheapify recomputes it
      // for packed slots but keeps the carried value for deferred ones.
      s->cp_pri2[w]   = s->cp_pri2[i];
      // The NF-witness cookie travels with the CP (per-CP, not per-slot).
      s->cp_last_norm_r_revision[w] = s->cp_last_norm_r_revision[i];
      if (s->cp_ultimate != NULL) s->cp_ultimate[w] = s->cp_ultimate[i];
      atp_cp_implicit_move(s, /*dst=*/w, /*src=*/i);
    }
    w++;
  }
  if (w != s->n_cps) {
    s->n_cps = w;
    thvm_atp_cp_reheapify(s);   // rebuilds heap + FV index
  }
}
#endif /* ATP_ORPHAN_KILL */

// Periodic critical-pair-set interreduction against the FULL rule set --
// a port of Waldmeister's KPV_KPMengeInterreduzieren (KPVerwaltung.c:1032)
// and its per-CP AP_generic callback (KPVerwaltung.c, the doR/doE branch).
// A queued CP that became joinable through an OLDER rule (e.g. an
// interreduce cascade) stays on the heap until it is finally popped and
// dies in thvm_atp_step's pop-time normalize.  Until then it pollutes the
// heap-min selection -- the engine keeps picking light CPs that normalize
// to nothing while the heavier proof-relevant overlaps wait.  Waldmeister
// purges those dead CPs from the queue eagerly, so its heap-min always
// reflects a live, irreducible CP.  This pass reproduces that: walk the
// whole queue, normalize each CP against the full current rule set, DELETE
// the joinable ones, repack the reduced ones, then reheapify (which
// recomputes every priority -- the AP_generic C_ReClassify reweight).
// Default OFF; the engine is byte-identical unless cp_set_interreduce is
// set (Method->"Waldmeister").
// Compute the set of TOP CTR symbols present in `term`'s subterm tree.
// Returns a 64-bit bitmap (bit k = label k).  Label >= 64 sets the
// fallback bit 63 ("any").  Used by atp_cp_set_interreduce's incremental
// fast-path to skip CPs that cannot be rewritten by recently-added rules.
static u64 atp_term_sym_bitmap(Term t) {
  if (t == 0u) return 0u;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u64 m = (lab < 63u) ? ((u64)1 << lab) : ((u64)1 << 63u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) m |= atp_term_sym_bitmap(term_ctr_at(t, i));
      return m;
    }
    default: return 0u;
  }
}

static void atp_cp_set_interreduce(AtpState *s) {
  if (s == NULL || s->n_cps == 0u || s->n_rules == 0u) return;
  const u32 NORM_CAP = 64u;
  u32 w = 0u;
  int touched = 0;
  // Sub-phase timer accumulators -- flushed to the four
  // g_atp_phase_us_cpir_* globals at the end of the function.  Cheap when
  // g_atp_phase_enabled is off (the `atp_phase_now` call is a single
  // load+branch; the `atp_now_us` syscall sits behind it).
  u64 acc_unpack = 0, acc_norm = 0, acc_pack = 0, acc_post = 0;
  // Snapshot the rule-set revision for the per-CP NF-witness cookie
  // fast-path.  The sweep itself never mutates r_revision (rule
  // add/drop only happens via orient_and_add / thvm_atp_interreduce,
  // between sweeps), so the snapshot is constant inside the loop and
  // uniquely identifies the active rewrite system this pass observes.
  // n_rules alone is INSUFFICIENT: an interreduce that drops K rules
  // and adds K back leaves n_rules unchanged but the rule set
  // different.  r_revision bumps on every rule mutation, so two
  // matching observations witness a byte-identical rule set.  When a
  // CP's cp_last_norm_r_revision[i] matches, the CP is still in NF
  // under R and we skip both atp_rewrite_normalize calls plus the
  // kbo_eq / acp_pack work behind them -- the dominant cp-set-ir cost.
  //
  // Currently dormant in the AndAssoc-class workloads the lever
  // targeted: the IR sweep is gated on `n_rules % period == 0u`, so
  // by construction at least one rule has been added between any two
  // consecutive sweeps, and r_revision has advanced -- the cookie
  // never matches a fresh entry, and any survivors from prior sweeps
  // carry stale cookies.  Kept in place as cheap infrastructure for a
  // future top-symbol-keyed partial invalidation (only flush cookies
  // whose CP can be reached by newly-added rules' LHS top symbols --
  // analogous to the existing incremental-IR bitmap skip but
  // CP-side instead of sweep-side).
  const u32 r_rev_snap = s->r_revision;

  // Incremental IR fast-path: rebuild the "new rule top symbols" bitmap
  // from rules added since the last IR pass.  A CP whose subterm bitmap
  // doesn't intersect this set cannot fire any new rule, so its
  // normalize is a guaranteed no-op -- skip it.  Engine byte-identical
  // when use_incr_ir is off; soundness contract on: only CPs proved to
  // have no possible new-rule rewrite are skipped, so the same NFs land
  // on the survivor set as a full re-normalize.
  u64 new_top_syms = 0u;
  if (s->use_incr_ir && s->n_rules > s->ir_rule_watermark) {
    for (u32 ri = s->ir_rule_watermark; ri < s->n_rules; ri++) {
      Term lhs_ri = s->lhs[ri];
      if (lhs_ri != 0u && term_tag(lhs_ri) == TAG_CTR) {
        u32 lab = term_ext(lhs_ri);
        new_top_syms |= (lab < 63u) ? ((u64)1 << lab) : ((u64)1 << 63u);
      } else {
        // Variable / unknown rule LHS top -- fall back to always re-norm.
        new_top_syms = ~(u64)0;
        break;
      }
    }
    s->ir_new_rule_top_syms = new_top_syms;
  }
  // Populate the FT mirror once across the whole sweep so the per-CP
  // FT normalize sees every live rule; the cached probe at
  // atp_ft_mirror_ensure makes repeat calls O(1).  Mirrors the
  // amortized site at atp_cp_trivially_joinable (_.c:10902).
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  atp_ft_mirror_ensure(s);
#endif
  for (u32 i = 0; i < s->n_cps; i++) {
    u64 _cpir_unpack_t0 = atp_phase_now();
    // Eager Waisenmord (WM AP_generic head): a CP whose parent rule has
    // been retired is redundant under the surviving R, so we can drop it
    // without paying the per-CP normalize.  Mirrors the lazy orphan check
    // at CP-select time -- doing it inside the IR sweep means a single
    // KPV_KPMengeInterreduzieren pass shrinks the queue by the dead-parent
    // set in linear time, instead of waiting for select-side amortization.
    if (s->use_orphan_murder && atp_cp_is_orphan(s, s->cp_trace[i])) {
      s->n_cps_dropped_orphan++;
      s->n_cp_set_ir_deleted++;
      free(s->cp_packed[i]);   // free(NULL) no-op for deferred slots
      s->cp_packed[i] = NULL;
      atp_cp_implicit_clear(s, i);
      touched = 1;
      if (g_atp_phase_enabled) acc_unpack += atp_now_us() - _cpir_unpack_t0;
      continue;
    }
    // Deferred slot: route it through the same materialize-then-
    // normalize the packed survivors get (WM materializes via
    // TPR_TP2ParIntermed, KPVerwaltung.c:975, before NF_Normalform2 --
    // the implicit passive set is NOT exempt from AP_generic).  The
    // slot's payload is the RAW trace pair, never normalized, so the
    // incremental-IR bitmap skip below is gated off for it (old rules
    // can still fire); the NF-witness cookie stays valid (set only by
    // a prior full normalize of this exact pair under this exact R).
    u8 is_impl = atp_cp_slot_implicit(s, i);
    // Per-CP heap checkpoint: the normalize allocates scratch cells; the
    // reduced terms are copied out by acp_pack, so the scratch is dead
    // after the (re)pack.  Reset each iteration so a long queue cannot
    // exhaust the dyn heap.  (An implicit slot's materialized pair lives
    // in the trace, which predates the checkpoint -- the reset cannot
    // pop it.)
    u64 hcp = thvm_atp_heap_checkpoint();
    Term ol = 0, orr = 0;
    atp_cp_slot_read(s, i, &ol, &orr);
    // WM `dokgS` (KPV `AP_generic` `SS_TermpaarSubsummiertVonGM` branch):
    // a CP whose two sides are directly subsumed by an existing rule's
    // pattern is one-step-joinable via that rule.  The trivial-join
    // normalize below catches a strict superset (`n_cps_dropped_rule_
    // subsumed <= n_cps_dropped_joinable`, see atp_cp_rule_subsumed
    // header), but rule-subsume tests just match -- no rewrite -- so we
    // short-circuit the per-CP normalize cost on the subsumed subset.
    // Gated on `use_rule_subsume_drop`, matching the same fast-path's
    // push-time wiring (WL Method options).
    if (s->use_rule_subsume_drop && atp_cp_rule_subsumed(s, ol, orr)) {
      s->n_cps_dropped_rule_subsumed++;
      s->n_cp_set_ir_deleted++;
      free(s->cp_packed[i]);   // free(NULL) no-op for deferred slots
      s->cp_packed[i] = NULL;
      atp_cp_implicit_clear(s, i);
      touched = 1;
      thvm_atp_heap_reset(hcp);
      if (g_atp_phase_enabled) acc_unpack += atp_now_us() - _cpir_unpack_t0;
      continue;
    }
    // Incremental IR fast-path: skip normalize if no new rule (since
    // last IR watermark) has a TOP SYMBOL appearing in either CP side.
    // Conservative: the bitmap union may cover symbols a normalize
    // would touch but that no new rule rewrites; harmless (just less
    // skipping).  Soundness: if no new top sym is present, no new
    // rule's LHS can match anywhere in the CP, so normalize is a no-op.
    // A FRESH implicit slot is excluded: its raw trace pair was never
    // normalized (push-norm ran on a transient reduced copy), so OLD
    // rules can still fire and the "normalize is a no-op" claim fails.
    // Once a sweep has NF-confirmed the slot, though, the claim holds
    // again: a non-NONE cookie on an implicit slot can ONLY come from
    // a previous sweep's full-R no-rewrite witness (the implicit push
    // stamps COOKIE_NONE and the pair is never repacked while it stays
    // deferred), so the raw pair is in NF wrt every rule below the
    // watermark and the bitmap test covers everything added since.
    if (s->use_incr_ir && new_top_syms != 0u
        && s->ir_rule_watermark > 0u
        && (!is_impl
            || s->cp_last_norm_r_revision[i] != ATP_CP_NORM_COOKIE_NONE)) {
      u64 cp_syms = atp_term_sym_bitmap(ol) | atp_term_sym_bitmap(orr);
      if ((cp_syms & new_top_syms) == 0u) {
        // No new rule can fire on this CP -- skip the normalize.  Repack
        // unchanged (just move the slot, no reweight).
        if (w != i) {
          s->cp_packed[w] = s->cp_packed[i];   // NULL deferred-slot marker
          s->cp_packed[i] = NULL;
        }
        s->cp_trace[w] = s->cp_trace[i];
        s->cp_last_norm_r_revision[w] = s->cp_last_norm_r_revision[i];
        if (s->cp_ultimate != NULL) s->cp_ultimate[w] = s->cp_ultimate[i];
        if (is_impl) {
          // Deferred survivor: carry the push-time heap keys (see the
          // cookie path below for why recomputing on the raw pair
          // would flip-flop the key against the descriptor cache).
          s->cp_pri[w]  = s->cp_pri[i];
          s->cp_goal[w] = s->cp_goal[i];
          s->cp_pri2[w] = s->cp_pri2[i];
          atp_cp_implicit_move(s, /*dst=*/w, /*src=*/i);
        } else {
          // Fold the per-survivor priority recompute into the IR walk
          // so the post-loop reheapify only has to do Floyd + FV.
          // Sides are byte-preserved (no normalize), so (ol, orr) is
          // still the packed payload; priority is pure in (s, l, r)
          // (see header).
          atp_cp_commit_priorities(s, w, ol, orr);
        }
        s->cp_seq[w] = s->cp_seq[i];
        w++;
        s->n_cp_set_ir_skipped++;
        thvm_atp_heap_reset(hcp);
        if (g_atp_phase_enabled) acc_unpack += atp_now_us() - _cpir_unpack_t0;
        continue;
      }
    }
    // NF-witness cookie fast-path: if this CP was last observed in NF
    // under EXACTLY the current rule set, both atp_rewrite_normalize
    // calls would return bitwise-identical ol / orr (`atp_rewrite_
    // normalize` is a pure function of subject + rule set + step_cap),
    // so kbo_eq(l, r) would not fire, the slot-move path runs unchanged,
    // and the cookie is still valid -- skip the normalize entirely.
    // Conservative: cookie is set only on a no-rewrite witness in the
    // slow path below, so a step_cap-truncated NF (where the second
    // pass might rewrite further) NEVER gets cached.  Soundness follows
    // from `atp_rewrite_normalize`'s purity in (subject, R, step_cap):
    // identical inputs return bytewise-identical outputs.
    if (s->cp_last_norm_r_revision[i] == r_rev_snap) {
      if (w != i) {
        s->cp_packed[w] = s->cp_packed[i];   // NULL deferred-slot marker
        s->cp_packed[i] = NULL;
      }
      s->cp_trace[w] = s->cp_trace[i];
      s->cp_last_norm_r_revision[w] = r_rev_snap;
      if (s->cp_ultimate != NULL) s->cp_ultimate[w] = s->cp_ultimate[i];
      if (is_impl) {
        // Implicit NF-confirmed survivor: stay deferred, carry the
        // push-time heap keys (the descriptor design -- the cached
        // priority was computed on the push-time reduced form and a
        // reheapify restores it from the descriptor, so recomputing on
        // the raw pair here would just flip-flop the key).
        s->cp_pri[w]  = s->cp_pri[i];
        s->cp_goal[w] = s->cp_goal[i];
        s->cp_pri2[w] = s->cp_pri2[i];
        atp_cp_implicit_move(s, /*dst=*/w, /*src=*/i);
      } else {
        // Fold the per-survivor priority recompute (NF-witness cookie
        // path: sides are bit-preserved, no normalize was needed).
        atp_cp_commit_priorities(s, w, ol, orr);
      }
      s->cp_seq[w] = s->cp_seq[i];
      w++;
      s->n_cp_set_ir_skipped++;
      thvm_atp_heap_reset(hcp);
      if (g_atp_phase_enabled) acc_unpack += atp_now_us() - _cpir_unpack_t0;
      continue;
    }
    if (g_atp_phase_enabled) acc_unpack += atp_now_us() - _cpir_unpack_t0;

    u64 _cpir_norm_t0 = atp_phase_now();
    Term l, r;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // FT-path normalize: ft_from_term once on each side, then run the
    // FT-native normalize (byte-identical to the Term-side path on the
    // workloads this sweep exercises -- see project_ftnorm_andassoc_
    // verify_break for the residual McCune-complementarity drift that
    // the IR sweep does NOT touch).  Lever 5 mirror: pre-norm ft_eq
    // short-circuits CPs whose sides are already syntactically equal
    // after acp_unpack (saves two normalize calls + the post-norm eq).
    // doR+doE here = WM AP_generic (KPVerwaltung.c:952-1014) run at
    // `-ki` strength "re"; the sweep itself is opt-in (CpSetInterreduce
    // / the WM preset), mirroring `-ki`'s default-off trigger.
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fl = ft_from_term(a, ol,  0);
    AtpFtCell *fr = ft_from_term(a, orr, 0);
    u8 joined_pre = (u8)ft_eq(fl, fr);
    if (!joined_pre) {
      fl = atp_rewrite_normalize_ft(s, fl, NORM_CAP);
      fr = atp_rewrite_normalize_ft(s, fr, NORM_CAP);
    }
    u8 joined_ft = joined_pre ? 1u : (u8)ft_eq(fl, fr);
    // Decode back to Term: the joined branch only needs ol/orr for the
    // existing acp_free, but staying Term-side keeps the cookie / NF
    // witness comparison byte-identical to the legacy path.
    l = joined_ft ? ol  : ft_to_term(fl);
    r = joined_ft ? orr : ft_to_term(fr);
#else
    l = atp_rewrite_normalize(s, ol,  s->lhs, s->rhs, s->n_rules, NORM_CAP);
    r = atp_rewrite_normalize(s, orr, s->lhs, s->rhs, s->n_rules, NORM_CAP);
    u8 joined_ft = (u8)kbo_eq(l, r);
#endif
    if (g_atp_phase_enabled) acc_norm += atp_now_us() - _cpir_norm_t0;

    u64 _cpir_pack_t0 = atp_phase_now();
    if (joined_ft) {
      // Joinable under R -- the CP adds no equational consequence.  Drop
      // it (WM AP_generic returns WTI_Delete).  Deferred slot: nothing
      // packed to free; the tag bit drops and the trace entry stays (it
      // is a GC root and may back other diagnostics).
      s->n_cps_dropped_joinable++;
      s->n_cp_set_ir_deleted++;
      free(s->cp_packed[i]);   // free(NULL) no-op for deferred slots
      s->cp_packed[i] = NULL;
      atp_cp_implicit_clear(s, i);
      touched = 1;
      thvm_atp_heap_reset(hcp);
      if (g_atp_phase_enabled) acc_pack += atp_now_us() - _cpir_pack_t0;
      continue;
    }
    // Capture the NF-witness status BEFORE the slot move overwrites
    // cp_last_norm_r_revision[w].  A no-rewrite outcome (l == ol && r == orr)
    // is a sound fixpoint witness for the current rule set: the next IR
    // sweep can skip this CP if n_rules has not changed.  A rewrite
    // outcome invalidates the cookie -- the new packed bytes have not
    // been confirmed in NF.
    u8 nf_witness = (l == ol && r == orr) ? 1u : 0u;
    // A deferred slot needs a STRUCTURAL unchanged-witness: the FT
    // decode (ft_to_term) always rebuilds fresh Terms, so pointer
    // identity against the trace-resident raw pair can never hold
    // and every raw-NF implicit slot would wrongly eagerify.  Packed
    // slots keep the pointer test -- byte-identical legacy behavior.
    if (is_impl && !nf_witness && kbo_eq(l, ol) && kbo_eq(r, orr)) {
      nf_witness = 1u;
    }
    if (!nf_witness) {
      // Reduced.  A deferred slot must become EAGER here: the reduced
      // pair no longer matches the TRACE_CP raw form its descriptor
      // points at, so the trace can no longer back a materialize.  Pack
      // the reduced pair and drop the tag bit -- from now on the slot
      // is an ordinary packed CP (FV-indexed again by the trailing
      // atp_fv_index_rebuild, reweighted below like every survivor).
      free(s->cp_packed[i]);   // free(NULL) no-op for deferred slots
      s->cp_packed[i] = NULL;
      s->cp_packed[w] = acp_pack(l, r, NULL, NULL);
      atp_cp_implicit_clear(s, i);
      s->n_cp_set_ir_reweighted++;
      touched = 1;
    } else if (w != i) {
      s->cp_packed[w] = s->cp_packed[i];   // NULL deferred-slot marker
      s->cp_packed[i] = NULL;
    }
    s->cp_trace[w] = s->cp_trace[i];
    s->cp_last_norm_r_revision[w] = nf_witness ? r_rev_snap
                                            : ATP_CP_NORM_COOKIE_NONE;
    // Preserve Act_ultimate across the WM AP_generic reweight: an
    // axiom whose normalized form is still non-trivial must keep its
    // front-rank (WM's `C_ReClassify` short-circuits on the original
    // history tag, never re-applying the heuristic to an `initial`
    // CP).  Without this carry-over the next reheapify resorts the
    // axiom by Mix weight and loses the ultimate front.
    if (s->cp_ultimate != NULL) s->cp_ultimate[w] = s->cp_ultimate[i];
    if (is_impl && nf_witness) {
      // Implicit survivor confirmed in NF under the full current R:
      // stay deferred (the raw trace pair is still the slot's exact
      // payload).  Carry the push-time heap keys -- the descriptor
      // caches the priority of the push-time reduced form, and
      // atp_cp_rebuild_priorities restores from that cache, so the
      // cached value is the one coherent key.
      s->cp_pri[w]  = s->cp_pri[i];
      s->cp_goal[w] = s->cp_goal[i];
      s->cp_pri2[w] = s->cp_pri2[i];
      atp_cp_implicit_move(s, /*dst=*/w, /*src=*/i);
    } else {
      // Fold the per-survivor priority recompute into the IR walk.  On
      // the reweight branch (l, r) is the post-normalize pair we just
      // packed (for a freshly-eagerified slot too); on the packed
      // no-rewrite branch it's the original pair -- either way, the
      // right input to the priority functions.
      atp_cp_commit_priorities(s, w, l, r);
    }
    // WM AP_generic reweight preserves the FIFO age: C_ReClassify only
    // recomputes w1 ("w2 wird nicht geaendert" / w2 is not changed,
    // CLAS/NewClassification.c:399-406).  cp_seq is the w2 analog, so
    // every survivor keeps its insertion age across the sweep.
    s->cp_seq[w] = s->cp_seq[i];
    w++;
    thvm_atp_heap_reset(hcp);
    if (g_atp_phase_enabled) acc_pack += atp_now_us() - _cpir_pack_t0;
  }
  u64 _cpir_post_t0 = atp_phase_now();
  s->n_cps = w;
  if (touched) {
    s->n_cp_set_ir_passes++;
    // Per-survivor priorities + cp_seq were folded into the IR walk at
    // each w-commit site, so the post-loop pass only owes us the FULL-
    // array work (Floyd build-heap + FV-index rebuild).
    atp_cp_floyd_only(s);
  }
  // Advance the incremental-IR watermark to the current rule count so
  // the next pass only checks rules added since this one.
  if (s->use_incr_ir) s->ir_rule_watermark = s->n_rules;
  if (g_atp_phase_enabled) {
    acc_post += atp_now_us() - _cpir_post_t0;
    g_atp_phase_us_cpir_unpack    += acc_unpack;
    g_atp_phase_us_cpir_normalize += acc_norm;
    g_atp_phase_us_cpir_pack      += acc_pack;
    g_atp_phase_us_cpir_post      += acc_post;
  }
}

fn void thvm_atp_set_use_incr_ir(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_incr_ir = on ? 1u : 0u;
  // Seed the watermark at current n_rules so the first pass under the
  // flag re-normalizes nothing (no "new" rules yet from its POV).  The
  // next rule addition increments n_rules past the watermark and the
  // IR pass will re-check.
  if (on) s->ir_rule_watermark = s->n_rules;
}

fn u32 thvm_atp_interreduce(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0 || added.first == 0) return 0;
#ifdef ATP_ORPHAN_KILL
  // 9b: collect the trace ids of rules dropped below, then kill the
  // queued CPs descended from them once the compaction loop is done.
  // Runtime-gated on use_eager_orphan_sweep (default ON): a NULL
  // atp_dead disarms every collection site below and the tail sweep.
  // The WM layout (Method->"Waldmeister") gates this OFF -- WM never
  // sweeps the queue on a rule drop; its descendant CPs die lazily at
  // pop via use_orphan_murder / atp_cp_is_orphan instead.
  u32 *atp_dead   = s->use_eager_orphan_sweep
                      ? (u32 *)malloc(added.first * sizeof(u32)) : NULL;
  u32  atp_n_dead = 0;
#endif

  // Copy the new rules' Terms by value so we can safely compact the
  // R array beneath them.  Term is 64-bit; the heap cells they point
  // to don't move.
  Term new_lhs[2];
  Term new_rhs[2];
  u32  new_traces[2];
  u32  n_new = added.count;
  if (n_new > 2) n_new = 2;
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
    new_traces[k] = s->r_trace[added.first + k];
  }

  u32 dropped = 0;
  u32 i       = 0;
  while (i < added.first - dropped) {
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    Term reduced;
    // When NORM_STEP recording is on (WL chain extraction needs it),
    // walk the LHS normalization step-by-step and push a TRACE_NORM_
    // STEP per fire chained from the dropped rule's trace.  The
    // TRACE_SIMPLIFY then parents on the chain tail, so its WL info
    // inherits from the last NORM_STEP whose recorded {lhs, rhs} is
    // exactly the simplified equation -- the cplEqSetQ check at the
    // ORIENT/SIMPLIFY resolveTrace branch passes directly and no
    // emitNorm BFS is needed to bridge the simplification gap.
    u32 simplify_parent = s->r_trace[i];
    if (s->record_norm_steps && !s->use_wm_demote) {
      reduced = atp_rewrite_normalize_slice_record(
          s, old_lhs, new_lhs, new_rhs, new_traces, n_new, 16,
          &simplify_parent, 0u, old_rhs);
    } else {
      // Under use_wm_demote the slice reduction is DETECTION only
      // (WM NF_ObjektAnwendbar, Interreduktion.c:308: an existence
      // test; the reduct is discarded and the victim re-enters with
      // its original sides), so no NORM_STEPs are recorded here --
      // the drain records its own oriented-only chain.
      reduced = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs,
                                      n_new, 16);
    }
    if (!kbo_eq(reduced, old_lhs)) {
      // The older rule's LHS simplified -- drop it and requeue for
      // re-orientation.  Legacy path: (reduced, old_rhs) immediately,
      // recorded as a TRACE_SIMPLIFY entry parented on the dropped
      // rule's trace index (or the NORM_STEP chain tail when
      // recording is on) so the proof DAG stays connected through
      // interreduction (a fresh TRACE_AXIOM would sever it).
      // use_wm_demote path: buffer the ORIGINAL sides; the victim
      // re-enters the queue only after this fact's CPs are generated
      // (WM KPV_IROpferBehandeln, KPVerwaltung.c:517-518 copies the
      // untouched pair).
      if (s->use_wm_demote) {
        // Capture the WM drain-order key while the victim is still in the
        // wmo tree (the atp_wmo_remove_trace below evicts it).  The LHS
        // face (thvm side 0) is the one the new rule reduced here.
        u32 wmo_key = s->use_emission_order
            ? atp_wmo_victim_drain_key(s, s->r_trace[i], 0u) : 0u;
        atp_irv_push(s, old_lhs, old_rhs, simplify_parent, wmo_key);
      } else {
        atp_add_equation_simplified(s, reduced, old_rhs, simplify_parent);
      }
      if (atp_rule_trace_on()) {
        char la[2048];
        atp_pretty_term(s->lhs[i], la, sizeof la);
        fprintf(stderr, "  RETIRE rule (slot %u, trace %u): LHS %s collapsed; "
                "re-queued for re-orientation\n", i, s->r_trace[i], la);
      }
#ifdef ATP_ORPHAN_KILL
      // Capture the dropped rule's trace id before the shift below
      // overwrites r_trace[i].  Its descendant CPs are now orphans.
      if (atp_dead != NULL && s->r_trace[i] != ATP_TRACE_NONE) {
        atp_dead[atp_n_dead++] = s->r_trace[i];
      }
#endif
      // Lazy orphan murder: mark the dropped rule's birthing trace id
      // dead so its descendant CPs are skipped at pop time (WM
      // selectNonOrphan).  No queue sweep here.
      if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[i]);
      // WM order mirror: the fact left R/E (RE_RegelEntfernen).
      if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_trace[i]);
      // Keep the unorientable-rule count live: the dropped rule leaves
      // R here (it re-enters as a queued equation, re-counted only if
      // re-oriented unorientable at its next atp_push_rule).
      if (!s->r_orient[i]) s->n_unorient--;
      for (u32 j = i + 1; j < s->n_rules; j++) {
        s->lhs[j - 1]      = s->lhs[j];
        s->rhs[j - 1]      = s->rhs[j];
        s->r_trace[j - 1]  = s->r_trace[j];
        s->r_orient[j - 1] = s->r_orient[j];
        // Shift the backward-subsumption sentinel state in lockstep:
        // without it, dropping a rule below a soft-deleted slot leaves
        // r_dead[] marking the WRONG slot (the dead bit stays at the
        // old index while the rule it tagged shifted down one).
        s->r_dead[j - 1]           = s->r_dead[j];
        s->r_dead_lhs_save[j - 1]  = s->r_dead_lhs_save[j];
        s->r_dead_rhs_save[j - 1]  = s->r_dead_rhs_save[j];
        // Remove-and-re-derive subsumer shapes + re-derive flag ride the
        // slot in lockstep with the dead-shape saves -- all are keyed by
        // slot, so a drop below them must shift them too (else a dead
        // slot's saved subsumer or a re-derived rule's suppression flag
        // lands on the wrong rule).
        s->r_dead_subsumer_lhs[j - 1] = s->r_dead_subsumer_lhs[j];
        s->r_dead_subsumer_rhs[j - 1] = s->r_dead_subsumer_rhs[j];
        s->r_rederive_cut[j - 1]      = s->r_rederive_cut[j];
        // Ground-joinability status rides the slot (WM: per-object).
        s->r_gj_status[j - 1]      = s->r_gj_status[j];
#ifdef THVM_ATPFT_RULES
        // Stage 4: shift the AtpFt slot pointers in lockstep -- no
        // re-conversion, the cells themselves are address-stable in
        // the slab pool, only the slot index changes.
        s->lhs_ft[j - 1] = s->lhs_ft[j];
        s->rhs_ft[j - 1] = s->rhs_ft[j];
        s->r_dead_lhs_save_ft[j - 1] = s->r_dead_lhs_save_ft[j];
        s->r_dead_rhs_save_ft[j - 1] = s->r_dead_rhs_save_ft[j];
#endif
      }
      s->n_rules--;
      // Bump the rule-set revision counter -- a drop+add cycle can
      // leave n_rules unchanged, so r_revision is what the IR-normalize
      // cookie keys on (see AtpState.cp_last_norm_r_revision).
      s->r_revision++;
#ifdef ATP_RULE_INDEX
      // 7e lever 2: a rule was dropped and the array compacted -- the
      // rule-LHS index's index->LHS mapping is stale even if a later
      // re-add restores n_rules.  Force a rebuild on the next query.
      s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
      dropped++;
      // Don't increment i; the next older rule shifted down to slot i.
    } else {
      // === RIGHT-REDUCTION (composition) ============================
      // The older rule i's LHS did NOT collapse, so the rule stays in
      // R as `l -> old_rhs`.  Now try to reduce its RHS: rewrite
      // old_rhs to a normal form r'.  If it changes (and l > r' still
      // holds for the reduction order), update s->rhs[i] in place --
      // l = r' is still an equational consequence (l = old_rhs ->* r')
      // and the CPs born from rule i now use the normalized RHS.  This
      // is the DISCOUNT-loop right-reduction / composition step;
      // without it the RHSs (and every CP overlapping them) bloat
      // across the run.
      if (s->right_reduce) {
        Term r_reduced = old_rhs;
        // Thread the proof DAG: record each RHS rewrite as a NORM_STEP
        // chained off rule i's own TRACE_ORIENT (parent), side = 1
        // (the RHS), with the LHS as the unchanged other side.  The
        // chain tail then carries the equation {l, r'} exactly, so a
        // new TRACE_ORIENT parented on it inherits directly under chain
        // extraction, and emitNorm bridges it under chain-off.
        u32 rr_parent = s->r_trace[i];
        if (s->use_rhs_interreduce) {
          // WM RMRechtsInterred (INF/Interreduktion.c:329-360) under the
          // -irrp CLI default FALSE = "modify rule" (RUN/Parameter.c:
          // 334-343): only LIVE ORIENTED rules are composed (the
          // Regelbaum walk RE_forRegelnRobust at :338; E-members take
          // the GMInterred requeue in the loop below), the trigger is
          // ONE application of the new object anywhere in the RHS
          // (ObjektAngewendet, NF/NFBildung.c:686-713), and the stepped
          // term then goes to FULL R+E normal form -- NF_NormalformstRE
          // (:715-724) calls NF_NormalformRE = NF_Normalform(TRUE, TRUE,
          // .) (NFBildung.h:78), doR = doE = TRUE over the live system
          // (which already contains the new rule: RE_FaktumEinfuegen
          // precedes IR_InterreduktionRechts, Hauptkomponenten.c:
          // 311-313).  In place: no requeue, no CP, and NO symbol-count
          // guard -- WM commits the full NF unconditionally.
          if (s->r_orient[i] && !s->r_dead[i]) {
            if (s->record_norm_steps) {
              Term stepped = atp_rewrite_normalize_slice_record(
                  s, old_rhs, new_lhs, new_rhs, new_traces, n_new, 1u,
                  &rr_parent, 1u, old_lhs);
              if (!kbo_eq(stepped, old_rhs)) {
                r_reduced = atp_rewrite_normalize_record(
                    s, stepped, old_lhs, 1u, &rr_parent, 64u);
              }
            } else {
              // step_cap 1 = exactly one ordered rewrite against the
              // new-object slice (the ObjektAngewendet existence-and-
              // apply); 64 matches the engine's full-NF NORM_CAP.
              Term stepped = atp_rewrite_normalize(s, old_rhs, new_lhs,
                                                   new_rhs, n_new, 1u);
              if (!kbo_eq(stepped, old_rhs)) {
                r_reduced = atp_rewrite_normalize(s, stepped, s->lhs,
                                                  s->rhs, s->n_rules, 64u);
              }
            }
          }
        } else if (s->record_norm_steps) {
          r_reduced = atp_rewrite_normalize_slice_record(
              s, old_rhs, new_lhs, new_rhs, new_traces, n_new, 16,
              &rr_parent, 1u, old_lhs);
        } else {
          r_reduced = atp_rewrite_normalize(s, old_rhs, new_lhs, new_rhs,
                                            n_new, 16);
        }
        if (!kbo_eq(r_reduced, old_rhs)) {
          // Orientation guard: l -> r' must remain a valid reduction
          // rule (l strictly greater than r').  r' is a reduct of
          // old_rhs <= l, so this holds in the standard case; verify it
          // and skip the in-place update if some pathological order
          // makes l NOT > r' (keep the rule as `l -> old_rhs`).
          // Size guard (legacy mode only): a reduction order (KBO/LPO)
          // guarantees r' is smaller than old_rhs in the ORDER but not
          // necessarily in raw symbol count -- a rule a -> g(b,c)
          // rewrites a constant into a deeper term.  The slice-only
          // composition skips the in-place update when r' has more
          // symbols than old_rhs so the compact RHS keeps feeding CPs.
          // WM mode has no such guard (RMRechtsInterred commits the
          // NF_NormalformRE result unconditionally), so
          // use_rhs_interreduce bypasses it.
          if (atp_compare(s, old_lhs, r_reduced) == KBO_GT &&
              (s->use_rhs_interreduce ||
               atp_symbol_count(r_reduced) <= atp_symbol_count(old_rhs))) {
            // Record the post-reduction rule as a fresh TRACE_ORIENT
            // parented on the NORM_STEP chain tail (or directly on the
            // old ORIENT when norm-step recording is off).  Repointing
            // r_trace[i] keeps resolveRule(i) -> the entry whose stored
            // (lhs, rhs) equals the live (l, r') pair.
            u32 new_t = atp_trace_push(s, TRACE_ORIENT, rr_parent,
                                       ATP_TRACE_NONE, old_lhs, r_reduced);
            // Retire the old `l -> old_rhs` ORIENT for the chain-off
            // aliveRulesAt model: a TRACE_SIMPLIFY whose ParentA names
            // the old trace marks it inactive from this point forward,
            // so emitNorm replays against `l -> r'`.  Push the marker
            // straight onto the trace (NOT atp_add_equation_simplified,
            // which would also enqueue a redundant CP -- the rule is
            // already live in R as l -> r').
            atp_trace_push(s, TRACE_SIMPLIFY, s->r_trace[i],
                           ATP_TRACE_NONE, old_lhs, r_reduced);
            s->rhs[i]     = r_reduced;
            // WM order mirror: the rule keeps its tree position; only
            // its identity tag moves (RMRechtsInterred in-place modify).
            if (s->use_emission_order) {
              atp_wmo_rename_trace(s, s->r_trace[i], new_t);
            }
            s->r_trace[i] = new_t;
            s->n_right_reduced++;
            if (atp_rule_trace_on()) {
              char la[2048], ra[2048];
              atp_pretty_term(old_lhs, la, sizeof la);
              atp_pretty_term(r_reduced, ra, sizeof ra);
              fprintf(stderr, "  COMPOSE rule (slot %u): %s -> %s\n",
                      i, la, ra);
            }
#ifdef THVM_ATPFT_RULES
            // Stage 4: the in-place RHS edit replaces the Term; the
            // AtpFt mirror must re-convert (the old AtpFt RHS cells
            // become unreachable in the arena, freed wholesale at
            // ft_destroy time).  LHS unchanged so lhs_ft[i] stays.
            s->rhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                        r_reduced, 0);
            atp_ft_rules_verify_push(s, i);
#endif
            // RHS does not feed the LHS discrimination tree, so the
            // rule-LHS index stays valid; no dirty flag needed.
#ifdef ATP_RULE_INDEX
            // The WM-FPA cache, by contrast, holds a flat COPY of each
            // rule's RHS (wf_step reads it for the splice), so an in-place
            // RHS edit must update it -- otherwise the cached tree would
            // splice the stale, larger RHS and diverge from the IC normal
            // form.  Re-encode just this rule's RHS in O(rule); only when
            // that in-place update can't be done do we force a full rebuild.
            if (!wf_eng_cache_update_rhs(s, i)) s->wmfpa_dirty = 1u;
#endif
          }
        }
      }
      i++;
    }
  }

  // === Waldmeister GMInterred E-face sweep + Vampire bd=all ==========
  // Under use_rhs_interreduce the RHS-face check here applies to
  // UNORIENTABLE slots only: WM's GMInterred (Interreduktion.c:280-293)
  // walks every E-member's directed twins (RE_forGMReferenzen), so an
  // equation with EITHER face reducible by the new object leaves E for
  // the IR buffer and re-enters the queue.  thvm stores one slot per
  // equation; the LHS face was handled by the first loop above, the RHS
  // face is handled here.  ORIENTED rules are NOT dropped for a
  // reducible RHS -- that is WM's -irrp TRUE "delete rule" mode
  // (Interreduktion.c:309/318), and the CLI default is FALSE = "modify
  // rule" (Parameter.c:337-343): their RHSs were composed in place
  // against the full R+E system in the loop above (RMRechtsInterred).
  // Runtime-gated: the default engine (use_rhs_interreduce == 0 &&
  // use_bwd_demod == 0) skips this entirely.
  if (s->use_rhs_interreduce || s->use_bwd_demod) {
    u32 j = 0;
    while (j < added.first - dropped) {
      Term old_lhs = s->lhs[j];
      Term old_rhs = s->rhs[j];
      // Optional LHS demodulation (Vampire bd=all): try normalizing the
      // LHS first.  When it reduces, the original rule is no longer in
      // normal form -- drop and re-queue (reduced_lhs, old_rhs).
      // Skipped when the slot is already dead (BS sentinel) since
      // atp_rewrite_normalize on a sentinel-LHS would no-op anyway.
      Term reduced_lhs = old_lhs;
      if (s->use_bwd_demod && !s->r_dead[j]) {
        reduced_lhs = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs,
                                             n_new, 16);
      }
      // Normalize the RHS face against only the new rule(s) -- WM's
      // GMInterred trigger is NF_ObjektAnwendbar(Objekt, .) per twin
      // direction (Interreduktion.c:286), an existence test against the
      // new object alone.  Unorientable slots only (see the header
      // comment); oriented rules' RHSs were composed in place above.
      Term reduced = (s->use_rhs_interreduce && !s->r_dead[j] &&
                      !s->r_orient[j])
          ? atp_rewrite_normalize(s, old_rhs, new_lhs, new_rhs, n_new, 16)
          : old_rhs;
      u8 lhs_changed = (s->use_bwd_demod && !kbo_eq(reduced_lhs, old_lhs));
      u8 rhs_changed = (s->use_rhs_interreduce && !kbo_eq(reduced, old_rhs));
      if (lhs_changed) {
        // LHS reduced -- drop the rule and re-queue with the rewritten
        // LHS (and possibly-rewritten RHS).  This is the bd=all branch.
        // Under use_wm_demote the victim's ORIGINAL sides are buffered
        // instead and drained after CP generation (WM IR buffer).
        u32 simplify_parent = s->r_trace[j];
        if (s->use_wm_demote) {
          // bwd_demod reduced the LHS face (thvm side 0).
          u32 wmo_key = s->use_emission_order
              ? atp_wmo_victim_drain_key(s, s->r_trace[j], 0u) : 0u;
          atp_irv_push(s, old_lhs, old_rhs, simplify_parent, wmo_key);
        } else {
          atp_add_equation_simplified(s, reduced_lhs, reduced,
                                      simplify_parent);
        }
#ifdef ATP_ORPHAN_KILL
        if (atp_dead != NULL && s->r_trace[j] != ATP_TRACE_NONE) {
          atp_dead[atp_n_dead++] = s->r_trace[j];
        }
#endif
        if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[j]);
        if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_trace[j]);
        if (!s->r_orient[j]) s->n_unorient--;
        for (u32 k = j + 1; k < s->n_rules; k++) {
          s->lhs[k - 1]              = s->lhs[k];
          s->rhs[k - 1]              = s->rhs[k];
          s->r_trace[k - 1]          = s->r_trace[k];
          s->r_orient[k - 1]         = s->r_orient[k];
          s->r_dead[k - 1]           = s->r_dead[k];
          s->r_dead_lhs_save[k - 1]  = s->r_dead_lhs_save[k];
          s->r_dead_rhs_save[k - 1]  = s->r_dead_rhs_save[k];
          // Remove-and-re-derive subsumer shapes + flag ride the slot
          // (see the LHS-drop loop above for why all must shift).
          s->r_dead_subsumer_lhs[k - 1] = s->r_dead_subsumer_lhs[k];
          s->r_dead_subsumer_rhs[k - 1] = s->r_dead_subsumer_rhs[k];
          s->r_rederive_cut[k - 1]      = s->r_rederive_cut[k];
#ifdef THVM_ATPFT_RULES
          // Stage 4: shift the parallel AtpFt slot pointers.
          s->lhs_ft[k - 1]              = s->lhs_ft[k];
          s->rhs_ft[k - 1]              = s->rhs_ft[k];
          s->r_dead_lhs_save_ft[k - 1]  = s->r_dead_lhs_save_ft[k];
          s->r_dead_rhs_save_ft[k - 1]  = s->r_dead_rhs_save_ft[k];
#endif
        }
        s->n_rules--;
        s->r_revision++;   // rule-set mutated (see r_revision header)
#ifdef ATP_RULE_INDEX
        s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
        s->n_rules_bwd_demodulated++;
        dropped++;
        continue;
      }
      if (rhs_changed) {
        u32 simplify_parent = s->r_trace[j];
        // Drop the E-member with the reducible RHS face and re-queue
        // the simplified equation; orient will re-admit it -- or join
        // it away if it became trivial.  Under use_wm_demote the
        // ORIGINAL sides are buffered instead (WM GMInterred victims
        // enter the PU_REPuffer untouched, Interreduktion.c:290, and
        // drain after CP generation via IR_PufferAuslesen).
        if (s->use_wm_demote) {
          // The RHS face (thvm side 1) is the reducible one here.
          u32 wmo_key = s->use_emission_order
              ? atp_wmo_victim_drain_key(s, s->r_trace[j], 1u) : 0u;
          atp_irv_push(s, old_lhs, old_rhs, simplify_parent, wmo_key);
        } else {
          atp_add_equation_simplified(s, old_lhs, reduced, simplify_parent);
        }
        if (atp_rule_trace_on()) {
          char la[2048];
          atp_pretty_term(old_lhs, la, sizeof la);
          fprintf(stderr, "  RETIRE eq-rhs (slot %u, trace %u): %s\n",
                  j, s->r_trace[j], la);
        }
#ifdef ATP_ORPHAN_KILL
        if (atp_dead != NULL && s->r_trace[j] != ATP_TRACE_NONE) {
          atp_dead[atp_n_dead++] = s->r_trace[j];
        }
#endif
        if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[j]);
        if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_trace[j]);
        if (!s->r_orient[j]) s->n_unorient--;
        for (u32 k = j + 1; k < s->n_rules; k++) {
          s->lhs[k - 1]              = s->lhs[k];
          s->rhs[k - 1]              = s->rhs[k];
          s->r_trace[k - 1]          = s->r_trace[k];
          s->r_orient[k - 1]         = s->r_orient[k];
          s->r_dead[k - 1]           = s->r_dead[k];
          s->r_dead_lhs_save[k - 1]  = s->r_dead_lhs_save[k];
          s->r_dead_rhs_save[k - 1]  = s->r_dead_rhs_save[k];
          // Remove-and-re-derive subsumer shapes + flag ride the slot
          // (see the LHS-drop loop above for why all must shift).
          s->r_dead_subsumer_lhs[k - 1] = s->r_dead_subsumer_lhs[k];
          s->r_dead_subsumer_rhs[k - 1] = s->r_dead_subsumer_rhs[k];
          s->r_rederive_cut[k - 1]      = s->r_rederive_cut[k];
#ifdef THVM_ATPFT_RULES
          // Stage 4: shift the parallel AtpFt slot pointers.
          s->lhs_ft[k - 1]              = s->lhs_ft[k];
          s->rhs_ft[k - 1]              = s->rhs_ft[k];
          s->r_dead_lhs_save_ft[k - 1]  = s->r_dead_lhs_save_ft[k];
          s->r_dead_rhs_save_ft[k - 1]  = s->r_dead_rhs_save_ft[k];
#endif
        }
        s->n_rules--;
        s->r_revision++;   // rule-set mutated (see r_revision header)
#ifdef ATP_RULE_INDEX
        s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
        dropped++;
        // The next older rule shifted down to slot j; don't advance.
      } else {
        j++;
      }
    }
  }

#ifdef ATP_ORPHAN_KILL
  if (atp_dead != NULL) {
    if (atp_n_dead > 0) atp_cp_kill_orphans(s, atp_dead, atp_n_dead);
    free(atp_dead);
  }
#endif
  return dropped;
}

// === Waldmeister IR-victim drain (use_wm_demote) ====================

// WM doR normalize: bring `t` to normal form using ORIENTED rules only
// (the Regelbaum) over the full current R.  Unorientable equations (the
// Gleichungsbaum) do not rewrite -- the `-kg r` strength WM applies at
// generation-time / requeue treatment (RUN/Parameter.c:397).
static Term atp_rules_only_normalize(AtpState *s, Term t, u32 cap) {
#ifdef ATP_RULE_INDEX
  // The discrimination-tree normalizer indexes exactly the oriented
  // (and non-dead) slots when unorientable equations are present, and
  // every slot when all of R is oriented -- WM doR either way.
  return atp_rewrite_normalize_indexed(s, t, cap);
#else
  for (u32 it = 0; it < cap; it++) {
    if (atp_norm_deadline_fired(s)) return t;
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    g_atp_proof_oriented_only = 1u;
    Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                     &fwd, &fired);
    g_atp_proof_oriented_only = 0u;
    if (!fired) break;
    t = t2;
  }
  return t;
#endif
}

// Port of `IR_PufferAuslesen` -> `Anwendungsprozedur` ->
// `KPV_IROpferBehandeln` (INF/Interreduktion.c:387-392 / 175-207;
// INF/KPVerwaltung.c:514-528): every victim thvm_atp_interreduce
// buffered re-enters the queue HERE, after the new fact's CPs were
// generated, with its ORIGINAL sides and WM's late FIFO age.  Each
// victim gets the `KPBehandelt` treatment under WM's default `-kg r`
// flags (KPVerwaltung.c:435-467):
//   * lohntSichBehandlung gate (combined size < 50, line 437): only a
//     small victim is treated at all; a big one re-queues RAW.
//   * NF_Normalform2(doR = TRUE, doE = FALSE): both sides normalized
//     against the full CURRENT rule set, oriented rules only.  A
//     victim demoted BY an unorientable equation therefore re-enters
//     with its sides intact (McCune-II `ues 32`: the original rule 2
//     pair, re-weighted 505, joined away only when re-selected).
//   * joined victims are discarded outright (line 443-446).
// Survivors enqueue through the normal TRACE_SIMPLIFY path: fresh
// heuristic weight from the live weight mode + fresh cp_seq -- WM's
// recentCPinsert stamps fresh w1 and w2 = ++CPNr
// (CLAS/NewClassification.c:300-325); the requeued CP's otherParent is
// NULL there, so like thvm's TRACE_SIMPLIFY entries it is never
// orphan-killed.
// Stable insertion sort of the IR-victim buffer by WM drain-order key.
// (n_irv is tiny -- at most a handful of victims per interreduction pass.)
static void atp_irv_sort_wm_order(AtpState *s) {
  for (u32 a = 1; a < s->n_irv; a++) {
    Term l = s->irv_lhs[a], r = s->irv_rhs[a];
    u32  p = s->irv_parent[a], k = s->irv_wmo_key[a];
    u32  b = a;
    while (b > 0u && s->irv_wmo_key[b - 1u] > k) {
      s->irv_lhs[b]     = s->irv_lhs[b - 1u];
      s->irv_rhs[b]     = s->irv_rhs[b - 1u];
      s->irv_parent[b]  = s->irv_parent[b - 1u];
      s->irv_wmo_key[b] = s->irv_wmo_key[b - 1u];
      b--;
    }
    s->irv_lhs[b] = l; s->irv_rhs[b] = r;
    s->irv_parent[b] = p; s->irv_wmo_key[b] = k;
  }
}

static void atp_wm_demote_drain(AtpState *s) {
  const u32 WM_BEHANDELN_GATE = 50u;   // KPVerwaltung.c:437
  const u32 NORM_CAP = 64u;            // matches the pop-normalize cap
  // WM drains the IR buffer in discrimination-tree leaf-list order
  // (equation victims before rule victims), not thvm's slot-scan order;
  // reorder to match before stamping fresh FIFO ages.
  if (s->use_emission_order) atp_irv_sort_wm_order(s);
  for (u32 v = 0; v < s->n_irv; v++) {
    Term l      = s->irv_lhs[v];
    Term r      = s->irv_rhs[v];
    u32  parent = s->irv_parent[v];
    u32  trace_mark = s->n_trace;
    if (atp_symbol_count(l) + atp_symbol_count(r) < WM_BEHANDELN_GATE) {
      if (s->record_norm_steps) {
        // Record the oriented-only renormalize as a NORM_STEP chain
        // off the dropped rule's trace so the TRACE_SIMPLIFY below
        // stays connected in the proof DAG (same shape as the
        // pop-normalize recorder; g_atp_proof_oriented_only = doR).
        g_atp_proof_oriented_only = 1u;
        Term l2 = atp_rewrite_normalize_record(s, l, r, 0u, &parent,
                                               NORM_CAP);
        Term r2 = atp_rewrite_normalize_record(s, r, l2, 1u, &parent,
                                               NORM_CAP);
        g_atp_proof_oriented_only = 0u;
        l = l2;
        r = r2;
      } else {
        l = atp_rules_only_normalize(s, l, NORM_CAP);
        r = atp_rules_only_normalize(s, r, NORM_CAP);
      }
      if (kbo_eq(l, r)) {
        // Joined: the victim is redundant.  Rewind any NORM_STEPs just
        // recorded for it (nothing else references them yet).
        s->n_trace = trace_mark;
        s->n_wm_demote_joined++;
        continue;
      }
    }
    atp_add_equation_simplified(s, l, r, parent);
    s->n_wm_demote_requeued++;
    if (atp_rule_trace_on()) {
      char la[2048], ra[2048];
      atp_pretty_term(l, la, sizeof la);
      atp_pretty_term(r, ra, sizeof ra);
      fprintf(stderr, "  REQUEUE victim (WM drain): %s = %s\n", la, ra);
    }
  }
  s->n_irv = 0;
}

// === DIAGNOSTIC (Step 1) ============================================
// Measure the interreduction deficit and term-size growth of R.  For
// each rule i, normalize lhs[i] / rhs[i] against R \ {i} (linear
// oracle, full re-scan).  A side that changes is reducible by another
// rule -- the rule is NOT interreduced.  Reports counts + max/total
// symbol footprint.  Read-only; no engine state mutated.  Used by the
// bench to confirm whether R is genuinely under-interreduced.
fn void thvm_atp_interreduce_deficit(AtpState *s,
                                     u32 *n_lhs_reducible,
                                     u32 *n_rhs_reducible,
                                     u32 *n_any_reducible,
                                     u32 *max_side_symbols,
                                     u64 *total_symbols) {
  u32 nl = 0, nr = 0, na = 0, maxside = 0;
  u32 n_red_unorient = 0;
  u64 total = 0;
  if (s == NULL || s->n_rules == 0) goto done;
  u32 n = s->n_rules;
  Term *tl = (Term *)malloc((size_t)n * sizeof(Term));
  Term *tr = (Term *)malloc((size_t)n * sizeof(Term));
  if (tl == NULL || tr == NULL) { free(tl); free(tr); goto done; }
  for (u32 i = 0; i < n; i++) {
    u32 ls = atp_symbol_count(s->lhs[i]);
    u32 rs = atp_symbol_count(s->rhs[i]);
    if (ls > maxside) maxside = ls;
    if (rs > maxside) maxside = rs;
    total += (u64)ls + (u64)rs;
    // Build R \ {i}.
    u32 m = 0;
    for (u32 j = 0; j < n; j++) {
      if (j == i) continue;
      tl[m] = s->lhs[j];
      tr[m] = s->rhs[j];
      m++;
    }
    Term ln = thvm_rewrite_normalize(s->lhs[i], tl, tr, m, 256);
    Term rn = thvm_rewrite_normalize(s->rhs[i], tl, tr, m, 256);
    int lred = !kbo_eq(ln, s->lhs[i]);
    int rred = !kbo_eq(rn, s->rhs[i]);
    if (lred) nl++;
    if (rred) nr++;
    if (lred || rred) {
      na++;
      if (!s->r_orient[i]) n_red_unorient++;
      if (getenv("THVM_ATP_DIAG_DUMP") != NULL) {
        char la[1024], ra[1024], na_[1024];
        atp_pretty_term(s->lhs[i], la, sizeof la);
        atp_pretty_term(s->rhs[i], ra, sizeof ra);
        atp_pretty_term(lred ? ln : rn, na_, sizeof na_);
        // Which single rule reduces side i at the top?  (single-rule
        // collapse vs multi-rule chain.)
        int single = -1;
        Term side = lred ? s->lhs[i] : s->rhs[i];
        for (u32 j = 0; j < n; j++) {
          if (j == i) continue;
          Term one = thvm_rewrite_normalize(side, &s->lhs[j], &s->rhs[j], 1u, 4u);
          if (!kbo_eq(one, side)) { single = (int)j; break; }
        }
        fprintf(stderr, "    REDUCIBLE rule %u%s: %s -> %s   [%s side -> %s] "
                "(single-rule reducer: %d, orient_unred=%u)\n",
                i, s->r_orient[i] ? "" : "(un)", la, ra,
                lred ? "lhs" : "rhs", na_, single, n_red_unorient);
      }
    }
  }
  free(tl);
  free(tr);
done:
  if (n_lhs_reducible)  *n_lhs_reducible  = nl;
  if (n_rhs_reducible)  *n_rhs_reducible  = nr;
  if (n_any_reducible)  *n_any_reducible  = na;
  if (max_side_symbols) *max_side_symbols = maxside;
  if (total_symbols)    *total_symbols    = total;
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

#define ATP_CP_BATCH 4096

// Stage 7.1: trivial-joinability check AND critical-pair reduction.
// Normalize both sides of a candidate CP under the current rule set R
// and write the reduced pair back through `*lhs`/`*rhs`: standard
// completion queues the NORMALIZED CP, not the raw overlap.  The
// un-normalized overlap of two deep rules blows past thousands of
// nodes -- dragging every later KBO compare / match / index descent
// and overrunning the retrieval flatten cap into a full O(n) scan.
// Returns 1 iff the two normal forms collapse to one term: the CP is
// joinable-by-R, adds no new consequence, and the caller discards it.
//
// This is the Waldmeister `Grundzusammenfuehrung` ("ground-merging")
// criterion at its weakest, equivalent to Twee's "joinable-by-current-
// R" pruning.  Stronger variants (ground-joinability over a sample
// of substitutions, AC-aware joinability) are deferred to 7.2+.
//
// Cost: two `thvm_rewrite_normalize` calls per CP candidate.  Worth
// it when the saturation produces many joinable CPs (group axioms
// generate ~hundreds of trivially-joinable overlaps per round).
// Joinability cache: maps (lhs_hash ^ rhs_hash, R-epoch, n_rules) to
// the trivially-joinable verdict.  Stores only the boolean, not Term
// cells -- safe across heap_reset (which doesn't bump unf_memo_epoch
// directly anymore since the norm-result-cache port was reverted).
// On hit when the verdict is 0 (NOT joinable), we still have to
// normalize for the caller's *lhs/*rhs write-back so the cache
// short-circuit only helps when joined.  When the verdict is 1
// (joined), the caller drops the CP and doesn't need the NFs, so
// we skip the normalize entirely on hit -- the structural lever.
#define ATP_JOIN_CACHE_BITS 16
#define ATP_JOIN_CACHE_SIZE (1u << ATP_JOIN_CACHE_BITS)
#define ATP_JOIN_CACHE_MASK (ATP_JOIN_CACHE_SIZE - 1u)
typedef struct { u64 key; u32 epoch; u32 n_rules; u8 joined; } AtpJoinCacheEnt;
static AtpJoinCacheEnt g_atp_join_cache[ATP_JOIN_CACHE_SIZE];
u64 g_atp_join_cache_hits   = 0;
u64 g_atp_join_cache_misses = 0;

// Ensure the FT mirror is populated for every live rule before any
// caller of atp_rewrite_normalize_ft / atp_cp_trivially_joinable_ft.
// Production saturations (atp_push_rule) populate in lockstep with
// s->lhs[] writes, so this loop is empty after the first probe; test
// harnesses that write s->lhs[] directly get the mirror built on
// demand here so the FT normalize sees the whole rule set instead of
// silently skipping NULL-mirror slots.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static inline void atp_ft_mirror_ensure(AtpState *s) {
  if (s->ft_mirror_full && s->n_rules <= s->ft_mirror_probed_n_rules) return;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  u32 start = s->ft_mirror_full ? s->ft_mirror_probed_n_rules : 0u;
  for (u32 i = start; i < s->n_rules; i++) {
    if (s->lhs[i] != 0 && s->lhs_ft[i] == NULL) {
      s->lhs_ft[i] = ft_from_term(a, s->lhs[i], 0);
      s->rhs_ft[i] = ft_from_term(a, s->rhs[i], 0);
    }
  }
  s->ft_mirror_full = 1u;
  s->ft_mirror_probed_n_rules = s->n_rules;
}
#endif

// FT-only trivial-joinable check.  Term cells in (*lhs / *rhs), the work
// happens entirely in FT (ft_from_term / normalize / ft_eq), and the NFs
// are decoded back to Term only when the NOT-joined path's downstream
// filters (perm_sub, queue_sub, AC-eq) need them.  The Term-side
// `atp_rewrite_normalize` legacy path used to live behind a default-off
// VERIFY toggle but only ever lagged the FT verdict (and aborted spuriously
// on AndAssoc post-1d0a8035, where Term-side stops one rewrite step short
// of the FT-side NF -- see project_ftnorm_andassoc_verify_break); deleted.
//
// This is the generation-time CP treatment -- WM's `KPBehandelt`
// (INF/KPVerwaltung.c:439-467).  WM's default proof configuration runs
// it with NF_Normalform2(doR=TRUE, doE=FALSE) (`-kg` default "r",
// RUN/Parameter.c:397): oriented rules only, the unorientable-equation
// tree (and its grounded free-variable instances) never rewrites here.
// Hence atp_rules_only_normalize_ft, not atp_rewrite_normalize_ft.  The
// deferred full doR+doE normalize and join verdict happen at selection
// (thvm_atp_step's pop normalize = WM `KPV_Select`, `-ks` "r:e:s:p").
static u8 atp_cp_trivially_joinable(AtpState *s, Term *lhs, Term *rhs) {
  static int dbg_join_cache = -1;
  if (dbg_join_cache < 0) dbg_join_cache = atp_env_on("THVM_ATP_JOIN_CACHE");
  u64 join_key = 0;
  u8  join_cache_eligible = 0;
  if (dbg_join_cache) {
    u64 lh = atp_term_struct_hash(*lhs);
    u64 rh = atp_term_struct_hash(*rhs);
    // Symmetric: joinable(l, r) == joinable(r, l), so the mix is symmetric.
    join_key = (lh + rh) ^ (lh * rh + 0x9e3779b97f4a7c15ull);
    AtpJoinCacheEnt *e = &g_atp_join_cache[(u32)join_key & ATP_JOIN_CACHE_MASK];
    if (e->key == join_key && e->epoch == g_atp_unf_memo_epoch
        && e->n_rules == s->n_rules) {
      // Cached verdict is sound under (epoch + n_rules) gating: any R change
      // bumps one of them.  Short-circuit both branches, not just joined=1.
      g_atp_join_cache_hits++;
      return e->joined;
    }
    join_cache_eligible = 1u;
  }
  u8 joined = 0u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = ft_from_term(a, *lhs, 0);
  AtpFtCell *fr = ft_from_term(a, *rhs, 0);
#ifdef THVM_ATPFT_CPQ
  joined = atp_cp_trivially_joinable_ft(s, &fl, &fr);
#else
  fl = atp_rules_only_normalize_ft(s, fl, 64u);
  fr = atp_rules_only_normalize_ft(s, fr, 64u);
  joined = (u8)ft_eq(fl, fr);
#endif
  // Decode NFs back to Term only on the NOT-joined branch -- downstream
  // filters (perm_sub, queue_sub, AC-eq) need them; the joined branch
  // drops the CP and never reads *lhs/*rhs again.
  if (!joined) {
    Term l = ft_to_term(fl);
    Term r = ft_to_term(fr);
    *lhs = l;
    *rhs = r;
#ifdef THVM_ATP_AC
    // AC-equality redundancy: when an AC bitmask is registered, treat
    // AC-equal NFs as joinable.  Sound iff the AC axioms for every masked
    // label are present in R (normalize would have closed any AC-equal
    // pair via those axioms eventually, so the CP is redundant).
    if (thvm_atp_get_ac_mask() != 0ull) {
      AtpAcInfo ac = { .ac_mask = thvm_atp_get_ac_mask() };
      if (atp_ac_eq(l, r, &ac)) joined = 1u;
    }
#endif
  }
#else
  // No FT compiled in: caller must use a Term-only joinable check.  This
  // function isn't usable, so the cache write is skipped too.
  (void)lhs; (void)rhs;
#endif
  if (join_cache_eligible) {
    g_atp_join_cache_misses++;
    AtpJoinCacheEnt *e = &g_atp_join_cache[(u32)join_key & ATP_JOIN_CACHE_MASK];
    e->key     = join_key;
    e->epoch   = g_atp_unf_memo_epoch;
    e->n_rules = s->n_rules;
    e->joined  = joined;
  }
  return joined;
}

// Permutation-subsumption (port of WM `GZ_ACVerzichtbar` in
// Grundzusammenfuehrung.c:137).  Returns 1 iff (lhs, rhs) are
// AC-equivalent: they have the same top symbol AND the same multiset
// of immediate children, where two children are "the same" if they
// are themselves AC-equivalent or structurally equal (kbo_eq).
//
// Catches `nand(x, y) = nand(y, x)` and its nested variants, e.g.:
//   nand(nand(x,y), z)     = nand(nand(y,x), z)
//   nand(nand(x,y), nand(x,y)) = nand(nand(y,x), nand(x,y))
// WM drops such CPs at push + orient time when the top symbol is not
// AC-marked, preventing the cascade of commutativity-derived rules
// that dominates the AndAssoc faithful-port trajectory.
//
// Arity-2 only (Sheffer signature).  Higher arities would need a
// multiset matching with backtracking; defer.
// Per-symbol AC mask: bit i = "label i's CTRs are subject to the
// permutation-subsumption filter".  Set globally by the bench harness
// (env THVM_ATP_PERM_SUB_MASK) and queried inline below.  Mask of 0
// means "no per-symbol restriction" -- legacy behaviour where every
// AC-equal CP is dropped.  Mask != 0 means "only drop when the top
// symbol is in the mask" -- lets the WL paclet enable perm_sub for
// problems like AndAssoc (set bit 1 = nand) without breaking mccune
// (whose `and` symbol is intentionally outside the mask so its
// commutativity-shaped goal CP survives).  The global itself is
// defined up with the early ATP state (the enqueue / pop gates read
// it before this point); only the setter lives here.
fn void thvm_atp_set_perm_subsume_mask(u64 mask) {
  g_atp_perm_subsume_mask = mask;
}

// WM GZ_ACVerzichtbar's `!IstAssoz && !IstKommut && !IstErwKommut`
// exclusion lives below (atp_gj_perm_valuable: the A/C/C' shapes the
// ground-join fact test also protects).  Forward-declared so the
// AC-dispensability path can keep the generating axioms.
static u8 atp_gj_perm_valuable(Term l, Term r);

// Port of WM GZ_ACVerzichtbar (INF/Grundzusammenfuehrung.c:137), the
// AC-permutation-redundancy filter `dokgP`/`Permsub` runs at CP
// formation (KPVerwaltung.c:447) and at selection (:663), default-ON
// in the unconfigured Orkus run.  A CP whose two sides are AC-equal
// (equal as flattened+sorted multisets under the AC operator) says
// nothing the AC equations have not already said, so WM discards it.
// The two generating shapes -- commutativity and associativity (and
// extended commutativity) -- are kept (they ARE the AC theory).
//
//   if TO_ACGleich(l,r):                  // atp_ac_eq, flatten+sort
//     if !IstACSymbol(top):               // AC-equal but non-AC top
//       return TRUE                       // (e.g. not(or(..))=not(or(..)))
//     else: return !IstAssoz && !IstKommut && !IstErwKommut
//   else: return FALSE
//
// thvm's binary-only swap recursion (used when no AC mask is set, the
// legacy env-driven THVM_ATP_PERM_SUB_MASK path) cannot see that
// `or(a,or(b,c))` and `or(b,or(c,a))` are AC-equal: it never flattens
// the associative spine.  That gap is exactly the Huntington/Boolean/
// Ring/Meredith over-formation -- thvm kept thousands of 4-ary
// `or(a,or(b,or(c,d)))#<perm>` CPs WM perm-subsumes.
//
// Built only under THVM_ATP_AC (atp_ac_eq + the AcInfo live in ac.c).
// The default `bin/test_atp` build omits the flag; there the perm-
// subsume mask can never be set (the AC detector is unavailable), so
// the dispatch below is dead and the function need not exist.
#ifdef THVM_ATP_AC
static u8 atp_cp_ac_dispensable(Term lhs, Term rhs, const AtpAcInfo *ac) {
  if (kbo_eq(lhs, rhs)) return 0;               // t = t handled upstream
  // TO_ACGleich: AC-equal under the perm-subsume AC operators
  // (atp_ac_eq flattens + hash-sorts + structurally compares).  This
  // `ac` is the perm-subsume mask, kept SEPARATE from the engine-wide
  // g_atp_ac_info that gates AC-unification/AC-matching: WM does plain
  // KB completion (no AC-unification) and only filters CPs with this
  // test, so activating thvm's AC-saturation here would diverge.
  if (!atp_ac_eq(lhs, rhs, ac)) return 0;
  // !SO_IstACSymbol(top): AC-equal but the top symbol is not itself
  // an AC operator (the inner AC structure permuted under a non-AC
  // head, e.g. not(or(a,b)) = not(or(b,a))).  Drop unconditionally.
  if (!atp_ac_is_ac_top(ac, lhs)) return 1;
  // Top is an AC operator: keep the generating assoc/comm/ext-comm
  // axioms, drop every other AC-equal pair.
  return !atp_gj_perm_valuable(lhs, rhs);
}
#endif

static u8 atp_cp_perm_subsumed(Term lhs, Term rhs) {
  if (lhs == rhs) return 0;                      // identical handled elsewhere
#ifdef THVM_ATP_AC
  // WM-faithful path: when the AC operators have been detected and
  // registered in the perm-subsume mask (thvm_atp_set_perm_subsume_mask
  // from thvm_atp_auto_ac), use the full flattened AC-dispensability
  // test (GZ_ACVerzichtbar).  This catches the n-ary associativity
  // permutations the binary swap below misses.  The mask is a LOCAL
  // AtpAcInfo, NOT the engine-global g_atp_ac_info -- so the engine
  // stays in standard (non-AC) completion exactly like Waldmeister.
  if (g_atp_perm_subsume_mask != 0ull) {
    AtpAcInfo ac = { .ac_mask = g_atp_perm_subsume_mask };
    return atp_cp_ac_dispensable(lhs, rhs, &ac);
  }
#endif
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
  if (term_ext(lhs) != term_ext(rhs)) return 0;
  // Per-symbol mask gate: when set, only fire perm_sub for symbols in
  // the mask.  Top-symbol bit outside the mask -> keep (return 0).
  if (g_atp_perm_subsume_mask != 0ull) {
    u32 lab = term_ext(lhs);
    if (lab >= 64u || ((g_atp_perm_subsume_mask >> lab) & 1ull) == 0ull) {
      return 0;
    }
  }
  u32 nl = term_ctr_n(lhs), nr = term_ctr_n(rhs);
  if (nl != nr || nl != 2u) return 0;
  Term l0 = term_ctr_at(lhs, 0), l1 = term_ctr_at(lhs, 1);
  Term r0 = term_ctr_at(rhs, 0), r1 = term_ctr_at(rhs, 1);
  // Two children are AC-same iff structurally equal OR they themselves
  // are an AC-permutation -- recurse.  This catches deep commutativity.
  u8 l0_r0 = kbo_eq(l0, r0) || atp_cp_perm_subsumed(l0, r0);
  u8 l1_r1 = kbo_eq(l1, r1) || atp_cp_perm_subsumed(l1, r1);
  if (l0_r0 && l1_r1) {
    // Identical (as multisets, same order):  caller (trivial-join /
    // duplicate-rule guard) handles this; we don't drop it because
    // the equation is `t = t` which should have been spotted upstream.
    // But pure structural equality (l0 == r0 && l1 == r1 with no
    // perm) is already caught earlier; this branch fires only when
    // the children are AC-equal under deeper permutation.  Treat it
    // as perm-subsumed: the equation says nothing new beyond what
    // its (already-derived) children-AC-equality said.
    return 1;
  }
  u8 l0_r1 = kbo_eq(l0, r1) || atp_cp_perm_subsumed(l0, r1);
  u8 l1_r0 = kbo_eq(l1, r0) || atp_cp_perm_subsumed(l1, r0);
  return l0_r1 && l1_r0;
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

// Bachmair-Dershowitz connectedness (Twee section 6.2): a critical
// pair (s, t) born from the peak `p = sigma(li)` is REDUNDANT if s and
// t can be joined through a rewrite proof in which every term is
// STRICTLY BELOW the peak in the reduction order.  Soundness: such a
// proof witnesses that the local confluence of `p` follows from
// strictly smaller overlaps already (or to be) handled, so dropping
// the CP cannot lose a needed consequence -- this is the standard
// connectedness redundancy of unfailing completion.
//
// This is STRONGER than trivial-joinability (7.1): plain normalization
// only follows order-DECREASING steps to a normal form, so two sides
// with distinct normal forms are kept; connectedness additionally
// admits an unorientable equation applied in its non-reducing local
// direction, PROVIDED the result stays strictly below the peak.  Such a
// "detour below the peak" joins CPs that 7.1 misses, which is where the
// extra pruning comes from.
//
// Implementation: a bounded forward reachability below `peak`.  Seed a
// small hash set with s and t's ordered normal forms (always < peak
// since they are proper reducts).  Repeatedly expand a frontier term by
// every one-step rewrite -- both rule directions, variable-safe -- whose
// RESULT u satisfies peak >_C u; insert u, tagged by which seed it
// descends from (s-side / t-side / both).  A term reached from BOTH
// sides is the join point -> redundant.  Bounded by ATP_CONN_MAX_NODES
// (return 0 = KEEP on overflow, so the criterion never wrongly deletes).
#define ATP_CONN_MAX_NODES 256u
#define ATP_CONN_SIDE_S 1u
#define ATP_CONN_SIDE_T 2u

// Structural hash over the preorder (FNV-ish): structurally-equal terms
// with distinct heap cells hash identically.  Fast pre-filter before a
// kbo_eq in the connectedness reachability set.
static u32 atp_struct_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ atp_struct_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// Collect every one-step rewrite of `t` (all positions, both
// variable-safe directions of each rule) whose result is strictly
// below `peak`, into out_buf[*n .. ).  Mirrors mnf_successors' shape
// but order-gated against the peak rather than the redex.
static u32 atp_conn_successors(AtpState *s, Term t, Term peak,
                               Term *out_buf, u32 *n, u32 cap) {
  u32 size = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 m = term_ctr_n(t);
    if (m > REWRITE_MAX_ARITY) return size;
    for (u32 i = 0; i < m; i++) {
      u32 base = *n;
      (void)atp_conn_successors(s, term_ctr_at(t, i), peak, out_buf, n, cap);
      for (u32 k = base; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < m; c++) {
          ch[c] = (c == i) ? out_buf[k] : term_ctr_at(t, c);
        }
        out_buf[k] = term_new_ctr(term_ext(t), ch, m);
      }
    }
  }
  for (u32 j = 0; j < s->n_rules && *n < cap; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[j], t, &sub)) {                 // l -> r
      Term repl = thvm_subst_apply(s->rhs[j], &sub);
      if (atp_compare(s, peak, repl) == KBO_GT) out_buf[(*n)++] = repl;
    }
    if (*n >= cap) break;
    if (atp_vars_contained(s->lhs[j], s->rhs[j])) {       // r -> l (var-safe)
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        Term repl = thvm_subst_apply(s->lhs[j], &sb);
        if (atp_compare(s, peak, repl) == KBO_GT) out_buf[(*n)++] = repl;
      }
    }
  }
  return size;
}

static u8 atp_cp_connected_below_peak(AtpState *s, Term lhs, Term rhs,
                                      Term peak) {
  if (s == NULL || peak == 0) return 0;
  // The ordered normal forms of both sides are proper reducts of the
  // peak, hence strictly below it; they are the natural seeds.
  Term l = atp_rewrite_normalize(s, lhs, s->lhs, s->rhs, s->n_rules, 64u);
  Term r = atp_rewrite_normalize(s, rhs, s->lhs, s->rhs, s->n_rules, 64u);
  if (kbo_eq(l, r)) return 1;                 // trivially joined below peak
  // A seed equal to the peak (no decreasing step taken) cannot stay
  // strictly below it -- bail (KEEP) to preserve soundness.
  if (atp_compare(s, peak, l) != KBO_GT || atp_compare(s, peak, r) != KBO_GT) {
    return 0;
  }
  Term  terms[ATP_CONN_MAX_NODES];
  u32   hash [ATP_CONN_MAX_NODES];
  u8    side [ATP_CONN_MAX_NODES];
  u32   n = 0u;
  terms[n] = l; hash[n] = atp_struct_hash(l); side[n] = ATP_CONN_SIDE_S; n++;
  terms[n] = r; hash[n] = atp_struct_hash(r); side[n] = ATP_CONN_SIDE_T; n++;
  Term succ[64];
  for (u32 cur = 0; cur < n; cur++) {
    u32 ns = 0u;
    (void)atp_conn_successors(s, terms[cur], peak, succ, &ns, 64u);
    for (u32 k = 0; k < ns; k++) {
      Term u  = succ[k];
      u32  hu = atp_struct_hash(u);
      u32  found = ATP_CONN_MAX_NODES;
      for (u32 i = 0; i < n; i++) {
        if (hash[i] == hu && kbo_eq(terms[i], u)) { found = i; break; }
      }
      if (found != ATP_CONN_MAX_NODES) {
        if ((side[found] | side[cur]) == (ATP_CONN_SIDE_S | ATP_CONN_SIDE_T)) {
          return 1;                          // join point reached from both
        }
        side[found] = (u8)(side[found] | side[cur]);
        continue;
      }
      if (n >= ATP_CONN_MAX_NODES) return 0;  // overflow -> KEEP (sound)
      terms[n] = u; hash[n] = hu; side[n] = side[cur]; n++;
    }
  }
  return 0;
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
#if defined(THVM_ATPFT_RULES) && defined(THVM_ATPFT_MATCH)
  // FT path: ft_match against the lhs_ft[]/rhs_ft[] mirror.
  // Per-CP-push this was ~200 thvm_match samples in the post-FT-skip
  // profile; ft_match avoids the heap-cell descent.  Requires the
  // mirror to be fully populated (test setups that bypass
  // atp_push_rule write s->lhs[] directly and leave lhs_ft[] NULL).
  // Mirror status is cached on AtpState by atp_cp_trivially_joinable.
  if (s->ft_mirror_full && s->n_rules <= s->ft_mirror_probed_n_rules) {
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *lhs_ft_in = ft_from_term(a, lhs, /*scratch=*/1);
    AtpFtCell *rhs_ft_in = ft_from_term(a, rhs, /*scratch=*/1);
    AtpFtSubst subst;
    u8 hit = 0u;
    for (u32 k = 0; k < s->n_rules; k++) {
      if (s->r_dead != NULL && s->r_dead[k]) continue;
      AtpFtCell *rl = s->lhs_ft[k];
      AtpFtCell *rr = s->rhs_ft[k];
      if (rl == NULL || rr == NULL) continue;  // defensive
      // Forward: σ rl = lhs AND σ rr = rhs (one σ extended via the
      // thread-through semantics of ft_match -- it leaves bindings
      // from a successful match in place for the next call).
      memset(&subst, 0, sizeof(subst));
      if (ft_match(rl, lhs_ft_in, &subst) &&
          ft_match(rr, rhs_ft_in, &subst)) { hit = 1u; break; }
      // Symmetric.
      memset(&subst, 0, sizeof(subst));
      if (ft_match(rl, rhs_ft_in, &subst) &&
          ft_match(rr, lhs_ft_in, &subst)) { hit = 1u; break; }
    }
    ft_scratch_reset(a);
    return hit;
  }
#endif
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

// WM SubsumptionBody core (INF/Subsumption.c:67-89 wrapping
// SS_TermpaarSubsummiertTermpaar's test, :104-110): does the equation
// (p_lhs, p_rhs) subsume the pair (lhs, rhs) "at some position"?  The
// pair itself, or the subpair reached by repeatedly descending into
// the UNIQUE differing immediate subterm (both sides must share the
// top symbol and agree on every other child, else FAIL --
// TO_TopSymboleGleich / NachfolgendeTeiltermeGleich), is an instance
// of the pattern pair under ONE substitution covering both sides
// (MO_TermpaarSubsummiertZweites, MatchOperationen.c:1452-1505:
// SubstAufIdentitaetSetzen runs once and the binding persists across
// the left- and right-side loops).  Both orientations of the PATTERN
// are tried (Subsumption.c:108-109).
static u8 atp_eq_subsumes_pair(Term p_lhs, Term p_rhs, Term lhs, Term rhs) {
  for (;;) {
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(p_lhs, lhs, &subst) &&
          thvm_match(p_rhs, rhs, &subst)) {
        return 1;
      }
    }
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(p_lhs, rhs, &subst) &&
          thvm_match(p_rhs, lhs, &subst)) {
        return 1;
      }
    }
    // Descend one level: same top symbol + exactly one differing
    // child, else no position can be subsumed.
    if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
    if (term_ext(lhs) != term_ext(rhs)) return 0;
    u32 n = term_ctr_n(lhs);
    if (term_ctr_n(rhs) != n) return 0;
    u32 diff = n;
    for (u32 i = 0; i < n; i++) {
      if (!kbo_eq(term_ctr_at(lhs, i), term_ctr_at(rhs, i))) {
        if (diff != n) return 0;        // a second differing child
        diff = i;
      }
    }
    if (diff == n) return 0;            // identical pair: unreachable
    lhs = term_ctr_at(lhs, diff);
    rhs = term_ctr_at(rhs, diff);
  }
}

// --- WM flatterm-faithful subsumption (firstdiv-19) -----------------------
// thvm's atp_eq_subsumes_pair above uses tree matching (thvm_match), which is
// the CORRECT subsumption.  WM's MO_TermpaarSubsummiertZweites
// (MatchOperationen.c:1452) is NOT correct: it binds a pattern variable to the
// xM slot named by an APPEARANCE COUNTER (SubstErgaenzenSubsumption :1445,
// xMletztes++) but unbound-checks / reads it back by the variable's OWN symbol
// (VarUngebunden / SubstitutVon).  For a pattern whose variables are not
// numbered in first-appearance order (the Gegenrichtung of commutativity,
// `x2*x1`), those two index spaces CROSS: the pattern's later variable stays
// "unbound" into the second side and binds to a whole compound with no
// consistency recheck -- an OVER-EAGER match.  That quirk is exactly why WM
// removes axiom2 `x*x = x*(y*(y*y))` the moment commutativity enters, then
// re-derives it later; reproducing WM's CP-selection sequence byte-for-byte
// requires reproducing the quirk.  EsFolgtSubstitutVon's flatterm-prefix test
// (:169) reduces to structural equality for complete-subterm bindings (a
// complete preorder subterm is self-delimiting), so kbo_eq stands in for it.
#define ATP_WMFLAT_MAX 1024u
typedef struct {
  Term node[ATP_WMFLAT_MAX];
  u32  end[ATP_WMFLAT_MAX];        // exclusive preorder end of subtree at i
  u32  n;
  u8   overflow;
} AtpWmFlat;

static void atp_wmflat_preorder(Term t, AtpWmFlat *f) {
  if (f->n >= ATP_WMFLAT_MAX) { f->overflow = 1u; return; }
  u32 self = f->n++;
  f->node[self] = t;
  if (term_tag(t) == TAG_CTR) {
    u32 k = term_ctr_n(t);
    for (u32 c = 0; c < k; c++) atp_wmflat_preorder(term_ctr_at(t, c), f);
  }
  f->end[self] = f->n;
}

// One side of MO_TermpaarSubsummiertZweites: walk pattern preorder against the
// subject preorder.  `subst`/`setf` (indexed 0..maxslot) and `*counter` PERSIST
// across the left and right calls (WM resets only once, at MO entry).
static u8 atp_wmflat_phase(const AtpWmFlat *pat, const AtpWmFlat *sub,
                           Term *subst, u8 *setf, u32 *counter, u32 maxslot) {
  u32 i = 0, j = 0;
  while (i < pat->n) {
    if (j >= sub->n) return 0;           // subject exhausted, pattern not
    Term pn = pat->node[i];
    if (term_tag(pn) == TAG_FVR) {
      u32 v = (u32)term_ext(pn);         // READ slot = variable's own symbol
      if (v >= maxslot) return 0;
      if (!setf[v]) {                    // VarUngebunden(v)
        u32 slot = *counter;             // WRITE slot = appearance counter
        if (slot >= maxslot) return 0;
        (*counter)++;
        subst[slot] = sub->node[j];
        setf[slot] = 1u;                 // the counter-vs-symbol CROSS
      } else if (!kbo_eq(subst[v], sub->node[j])) {
        return 0;                        // EsFolgt == structural equality
      }
      i = pat->end[i];                   // pattern var leaf -> next symbol
      j = sub->end[j];                   // subject -> skip whole subterm
    } else {                             // function symbol: must agree
      Term sn = sub->node[j];
      if (term_tag(sn) != TAG_CTR ||
          term_ext(sn) != term_ext(pn) ||
          term_ctr_n(sn) != term_ctr_n(pn)) return 0;
      i++;                               // descend (advance one symbol each)
      j++;
    }
  }
  return 1;
}

static u8 atp_wmflat_mo(Term pl, Term pr, Term sl, Term sr) {
  AtpWmFlat fpl, fpr, fsl, fsr;
  fpl.n = fpr.n = fsl.n = fsr.n = 0u;
  fpl.overflow = fpr.overflow = fsl.overflow = fsr.overflow = 0u;
  atp_wmflat_preorder(pl, &fpl);
  atp_wmflat_preorder(pr, &fpr);
  atp_wmflat_preorder(sl, &fsl);
  atp_wmflat_preorder(sr, &fsr);
  if (fpl.overflow || fpr.overflow || fsl.overflow || fsr.overflow) return 0;
  enum { WMFLAT_SLOTS = 64u };
  Term subst[WMFLAT_SLOTS];
  u8   setf[WMFLAT_SLOTS];
  for (u32 t = 0; t < WMFLAT_SLOTS; t++) setf[t] = 0u;
  u32 counter = 0u;
  if (!atp_wmflat_phase(&fpl, &fsl, subst, setf, &counter, WMFLAT_SLOTS))
    return 0;
  if (!atp_wmflat_phase(&fpr, &fsr, subst, setf, &counter, WMFLAT_SLOTS))
    return 0;
  return 1;
}

// WM SubsumptionBody (Subsumption.c:67) over the counter-cross matcher.  Tries
// both PATTERN orientations (Subsumption.c:108-109); thvm orients unorientable
// equations arbitrarily, so both SUBJECT orientations are covered too (WM picks
// the one distinguished direction -- TP_RichtungAusgezeichnet -- which thvm's
// storage may not match).  Peels the subject pair into its unique differing
// subterm, like atp_eq_subsumes_pair.
static u8 atp_wm_flat_subsumes_pair(Term p_lhs, Term p_rhs, Term lhs, Term rhs) {
  for (;;) {
    if (atp_wmflat_mo(p_lhs, p_rhs, lhs, rhs)) return 1;
    if (atp_wmflat_mo(p_rhs, p_lhs, lhs, rhs)) return 1;
    if (atp_wmflat_mo(p_lhs, p_rhs, rhs, lhs)) return 1;
    if (atp_wmflat_mo(p_rhs, p_lhs, rhs, lhs)) return 1;
    if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
    if (term_ext(lhs) != term_ext(rhs)) return 0;
    u32 n = term_ctr_n(lhs);
    if (term_ctr_n(rhs) != n) return 0;
    u32 diff = n;
    for (u32 i = 0; i < n; i++) {
      if (!kbo_eq(term_ctr_at(lhs, i), term_ctr_at(rhs, i))) {
        if (diff != n) return 0;
        diff = i;
      }
    }
    if (diff == n) return 0;
    lhs = term_ctr_at(lhs, diff);
    rhs = term_ctr_at(rhs, diff);
  }
}

// WM -ks "s" pop-time E-subsumption test (SS_TermpaarSubsummiertVonGM,
// INF/Subsumption.c:91-104).  Returns 1 if `(lhs, rhs)` is subsumed by
// a live unorientable equation under the SubsumptionBody semantics
// above.  Both orientations of each equation are tried, mirroring WM's
// Gleichung/Antigleichung twin storage in the Gleichungsbaum
// (RUndEVerwaltung.c:407-470: both directions are separately indexed,
// so MO_SubsummierendeGleichungGefunden sees each equation twice).
// Rules (r_orient[k] == 1) never participate -- WM consults
// RE_Gleichungsbaum only.  Caller guarantees lhs != rhs (the joined
// drop ran) and compare(lhs, rhs) == KBO_UN (WM gates the stage on
// Unvergleichbar, KPVerwaltung.c:667).
static u8 atp_pop_eq_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_unorient == 0u) return 0;       // RE_GleichungsmengeLeer
  for (u32 k = 0; k < s->n_rules; k++) {
    if (s->r_orient[k]) continue;                    // E only, never R
    if (s->r_dead != NULL && s->r_dead[k]) continue;
    if (atp_eq_subsumes_pair(s->lhs[k], s->rhs[k], lhs, rhs)) {
      static int sub_trace = -1;
      if (sub_trace < 0) sub_trace = atp_env_on("THVM_ATP_POP_TRACE");
      if (sub_trace) {
        fprintf(stderr, "POPSUBR pick=%u subsumer_slot=%u orient=%u lhs=",
                s->cp_select_count, k, s->r_orient[k]);
        atp_dbg_print_term(stderr, s->lhs[k]); fputs(" rhs=", stderr);
        atp_dbg_print_term(stderr, s->rhs[k]);
        fputs(" query_lhs=", stderr); atp_dbg_print_term(stderr, lhs);
        fputs(" query_rhs=", stderr); atp_dbg_print_term(stderr, rhs);
        fputc('\n', stderr);
      }
      return 1;
    }
  }
  return 0;
}

// --- WM commutativity-aware E-set subsumption (THVM_ATP_COMM_SUBSUME) ---
// Two terms are equal modulo ONE top-level commutativity swap: either they
// are structurally identical (compared via atp_term_struct_hash, the same
// fingerprint kbo_eq short-circuits on), OR both are 2-ary constructor
// applications of the SAME constructor whose children match when the
// candidate's two children are transposed (a.b == b.a -- a single top swap,
// no deeper / AC enumeration).  Mirrors WM's commutativity-normalised
// Interreduktion, NOT AC saturation.
static u8 atp_term_eq_mod_top_comm(Term a, Term b) {
  if (atp_term_struct_hash(a) == atp_term_struct_hash(b) && kbo_eq(a, b))
    return 1;
  if (term_tag(a) != TAG_CTR || term_tag(b) != TAG_CTR) return 0;
  if (term_ext(a) != term_ext(b)) return 0;
  if (term_ctr_n(a) != 2u || term_ctr_n(b) != 2u) return 0;
  return (u8)(kbo_eq(term_ctr_at(a, 0), term_ctr_at(b, 1)) &&
              kbo_eq(term_ctr_at(a, 1), term_ctr_at(b, 0)));
}

// Is constructor `ctr` live-commutative in the current rule/equation set?
// True iff some LIVE (non-dead) equation slot stores `ctr(V0,V1)=ctr(V1,V0)`
// in EITHER orientation (the commutativity axiom for `ctr`).  Walks the same
// per-slot eset/rule arrays atp_eset_subsume_by_new and atp_pop_eq_subsumed
// iterate (n_rules x lhs/rhs, r_orient/r_dead).
static u8 atp_comm_eq_for_ctr(Term l, Term r, u32 ctr) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0;
  if (term_ext(l) != ctr || term_ext(r) != ctr) return 0;
  if (term_ctr_n(l) != 2u || term_ctr_n(r) != 2u) return 0;
  Term l0 = term_ctr_at(l, 0), l1 = term_ctr_at(l, 1);
  Term r0 = term_ctr_at(r, 0), r1 = term_ctr_at(r, 1);
  if (term_tag(l0) != TAG_FVR || term_tag(l1) != TAG_FVR) return 0;
  if (term_tag(r0) != TAG_FVR || term_tag(r1) != TAG_FVR) return 0;
  // f(a,b) = f(b,a) with a != b, a/b matched across the swap.
  if (term_ext(l0) == term_ext(l1)) return 0;
  return (u8)(term_ext(l0) == term_ext(r1) && term_ext(l1) == term_ext(r0));
}

static u8 atp_op_is_live_commutative(AtpState *s, u32 ctr) {
  for (u32 k = 0; k < s->n_rules; k++) {
    if (s->r_dead != NULL && s->r_dead[k]) continue;
    // Either stored orientation expresses commutativity.
    if (atp_comm_eq_for_ctr(s->lhs[k], s->rhs[k], ctr)) return 1;
    if (atp_comm_eq_for_ctr(s->rhs[k], s->lhs[k], ctr)) return 1;
  }
  return 0;
}

// Commutativity-aware pair subsumption: candidate (cl,cr) is subsumed by the
// new equation (rl,rr) when, in EITHER orientation of the candidate, rl
// matches the candidate's lhs EXACTLY (struct hash / kbo_eq) AND rr equals
// the candidate's rhs modulo ONE top-comm swap, AND the candidate's top
// constructor is live-commutative.  This drops soa slot15
// `x.(y.x)=(y.y).x` once eqn-10 `x.(y.x)=x.(y.y)` enters -- same LHS, RHS
// one top-`.`-swap apart, with `.` live-commutative.
static u8 atp_eq_subsumes_pair_mod_top_comm(AtpState *s, Term rl, Term rr,
                                            Term cl, Term cr) {
  // Forward orientation: candidate stored as (cl, cr).
  if (term_tag(cl) == TAG_CTR && term_ctr_n(cl) == 2u &&
      atp_op_is_live_commutative(s, term_ext(cl)) &&
      kbo_eq(rl, cl) && atp_term_eq_mod_top_comm(rr, cr))
    return 1;
  // Swapped orientation: candidate stored as (cr, cl).
  if (term_tag(cr) == TAG_CTR && term_ctr_n(cr) == 2u &&
      atp_op_is_live_commutative(s, term_ext(cr)) &&
      kbo_eq(rl, cr) && atp_term_eq_mod_top_comm(rr, cl))
    return 1;
  return 0;
}

// WM E-set subsumption destroy on new-equation entry
// (GMSubsummierenMitGleichung, INF/Interreduktion.c:251-274; reached
// from IR_InterreduktionLinks :371-373 only when the new fact is an
// EQUATION, BEFORE RE_FaktumEinfuegen inserts it into GM --
// ArbeitsAufnahme, INF/Hauptkomponenten.c:311-312, so it can never
// subsume itself).  Every existing live E-member whose pair the new
// equation subsumes (SS_TermpaarSubsummiertTermpaar at :262, gated on
// the distinguished direction TP_RichtungAusgezeichnet at :261 -- one
// test per stored equation, both pattern orientations inside the
// matcher) is removed from GM and physically destroyed, twin included
// (FinaleKillprozSubsumption, :236-245).  NO requeue, no CP made.
//
// thvm stores ONE slot per equation (no Gleichung/Antigleichung twin
// under ATP_ORDERED_REWRITE), so a single soft-delete covers both
// directions: the bwd-subsume dead-sentinel recipe (sentinel faces
// make thvm_match / thvm_unify return 0, so every rewrite /
// CP-generation site skips the slot; originals go to the save slots
// for proof reconstruction).  n_unorient is NOT decremented --
// matching the bwd-subsume convention, it stays an upper bound on
// live unorientables and dead slots are skipped via r_dead/sentinel.
static void atp_eset_subsume_by_new(AtpState *s, u32 new_i) {
  Term new_lhs = s->lhs[new_i];
  Term new_rhs = s->rhs[new_i];
  Term dead_sentinel = term_new(0, TAG_FVR, 255u, 0);
  for (u32 i = 0; i < new_i; i++) {
    if (s->r_orient[i]) continue;   // E only (RE_forGleichungenRobust)
    if (s->r_dead[i]) continue;
    u8 subsumed = s->use_flat_subsume
        ? atp_wm_flat_subsumes_pair(new_lhs, new_rhs, s->lhs[i], s->rhs[i])
        : atp_eq_subsumes_pair(new_lhs, new_rhs, s->lhs[i], s->rhs[i]);
    // Commutativity-aware widening (THVM_ATP_COMM_SUBSUME, DEFAULT OFF):
    // after the plain test, also drop the candidate when the new equation
    // subsumes it modulo ONE top-comm swap under a live commutativity axiom.
    u8 comm_subsumed = 0;
    if (!subsumed && s->use_comm_subsume &&
        atp_eq_subsumes_pair_mod_top_comm(s, new_lhs, new_rhs,
                                          s->lhs[i], s->rhs[i])) {
      subsumed = 1;
      comm_subsumed = 1;
    }
    if (!subsumed)
      continue;
    s->r_dead_lhs_save[i] = s->lhs[i];
    s->r_dead_rhs_save[i] = s->rhs[i];
    // Remove-and-re-derive root-overlap ownership: save the subsuming
    // equation's SHAPE on this slot.  Its final birth trace is not yet
    // stamped here (the step loop stamps r_trace after orient_and_add
    // returns), so the cutoff is resolved lazily at re-derivation time
    // by locating the live rule of this shape.  When an equation of the
    // removed slot's shape is later re-derived (atp_push_rule), it then
    // suppresses root overlaps against any rule born at or before the
    // subsumer -- WM already enumerated those roots in the subsumer's
    // original batch against this (now-removed) equation.
    s->r_dead_subsumer_lhs[i] = new_lhs;
    s->r_dead_subsumer_rhs[i] = new_rhs;
    s->lhs[i] = dead_sentinel;
    s->rhs[i] = dead_sentinel;
    s->r_dead[i] = 1;
    // WM destroys the subsumed equation outright, and the removal
    // (RE_GleichungEntfernen, RUndEVerwaltung.c:503) runs
    // KPV_KillParent on the distinguished direction -- the victim's
    // queued CPs die as orphans at pop (selectNonOrphan), consuming
    // no selection.  Mirror via the lazy trace-dead mark.
    if (atp_rule_trace_on()) {
      fprintf(stderr, "  ESET-SUBSUME kill (slot %u, trace %u) lhs=",
              i, s->r_trace[i]);
      atp_dbg_print_term(stderr, s->r_dead_lhs_save[i]);
      fputs(" rhs=", stderr);
      atp_dbg_print_term(stderr, s->r_dead_rhs_save[i]);
      fputc('\n', stderr);
    }
    if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[i]);
    // WM order mirror: the subsumed equation's faces leave the tree
    // (RE_GleichungEntfernen via FinaleKillprozSubsumption).
    if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_trace[i]);
    // The active E set changed: invalidate the IR-normalize cookie
    // and force the rule/unorient index rebuild (a revision delta
    // that exceeds the rule-count delta is never pure-append).
    s->r_revision++;
    s->n_eqs_dropped_eset_subsumed++;
    if (comm_subsumed) s->n_eqs_dropped_comm_subsumed++;
#ifdef THVM_ATPFT_RULES
    s->r_dead_lhs_save_ft[i] = s->lhs_ft[i];
    s->r_dead_rhs_save_ft[i] = s->rhs_ft[i];
    s->lhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                dead_sentinel, 0);
    s->rhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                dead_sentinel, 0);
#endif
#ifdef ATP_RULE_INDEX
    s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
  }
}

// Stage 7.3b: queue subsumption check.  Returns 1 if the candidate
// `(lhs, rhs)` is a substitution instance of some queued CP
// the CP unpacked from `s->cp_packed[k]` -- i.e., there is σ such
// that `(lhs, rhs) = (σ qs[k], σ qt[k])` (or symmetric).
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
// 7d: with -DATP_FV_INDEX the O(n_cps) scan is replaced by an
// FV-index retrieval -- componentwise-dominated FVs are pulled from
// the trie and the SAME two-sided thvm_match runs only on those.
// The FV filter is a sound over-approximation, so the verdict is
// IDENTICAL to the scan, CP for CP.
#if defined(ATP_FV_INDEX)
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_cps == 0) return 0;
  return atp_fv_index_query(s->fv_index, lhs, rhs);
}
#else
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  for (u32 k = 0; k < s->n_cps; k++) {
    // Deferred-CP slot (cp_packed[k] == NULL): never matched against --
    // WM has no queue-vs-queue subsumption, so its implicit passive set
    // never participates; the FV-index variant gets the same verdict by
    // never indexing implicit slots.
    if (s->cp_packed[k] == NULL) continue;
    // Match the queued CP straight off its packed byte string.
    // Forward: σqs = lhs AND σqt = rhs (one σ).
    RewriteSubst fwd = {{0}};
    if (acp_match_pair(s->cp_packed[k], lhs, rhs, &fwd)) return 1;
    // Symmetric.
    RewriteSubst sym = {{0}};
    if (acp_match_pair(s->cp_packed[k], rhs, lhs, &sym)) return 1;
  }
  return 0;
}
#endif


// === Ground-joinability redundancy criterion ========================
// Martin-Nipkow (1990) / Twee (CADE 2021 sec 3.1) / Avenhaus-Hillen-
// brand-Loechner (2003).  A critical pair is GROUND-JOINABLE if every
// ground instance of it joins; such a CP is redundant and may be
// deleted without losing refutational completeness.  We implement
// Twee's SYMBOLIC order-parameterised version (gj_spec.md sections C+D):
// a constraint C is a total preorder on the CP's atoms (variables AND
// the relevant constants), represented as a Model that assigns each atom
// a (major,minor) rank.  Under C we rewrite SYMBOLICALLY: a rule l->r
// fires at a matched position iff lessIn(C, r-image, l-image) is defined
// -- i.e. (l-image) >=_C (r-image) holds for ALL grounding sigma that
// satisfy C -- and the two images are syntactically distinct.  lessIn
// (section D) decides >=_C for KBO by minimising the weight-difference
// linear form over all such sigma (the suffix-sum `minimumIn`), then a
// lexicographic comparison.  Crucially lessIn NEVER over-approximates
// >=_C (soundness condition P2): if it returns "defined" the inequality
// truly holds for every grounding, so no illegal rewrite ever fires.
//
// The groundJoin driver maintains a worklist of branches that together
// COVER every total preorder of the atoms.  Each branch is solved to a
// Model (or, when it forces ties, an equality substitution); the CP is
// normalised under the Model and the two normal forms compared.  On a
// successful join the Model is GENERALISED (weakenModel: drop an atom or
// merge two adjacent strict groups into a tie, NEVER merging two
// constants), and the complementary region is re-queued via diag().
// Equality branches unify the tied variables and recurse on the smaller
// CP.  Only when the worklist is fully exhausted (every ground ordering
// covered by a successful join -- soundness condition P3) do we DELETE.
// Any budget overflow, OOM, or unjoined branch -> KEEP (sound fallback).
//
// KBO is the supported order.  For LPO (s->lpo != NULL) we KEEP
// unconditionally: lessIn is implemented for KBO only, so GJ is a no-op
// (always sound) under LPO for now.
//
// Deletion is gated at the call site behind the runtime flag
// s->use_ground_join (Method -> {... "GroundJoin" -> True}); the
// n_cps_ground_joinable counter ticks regardless for measurement.  The
// whole region compiles out unless ATP_CP_GROUND_JOIN is defined (the
// shipped paclet defines it; see WL_ATP_DEFINES).
#ifdef ATP_CP_GROUND_JOIN

// Cap on distinct variables of the CP.  The branch worklist still
// terminates for larger counts, but small CPs are the norm and a tight
// cap bounds worst-case branching; over the cap -> KEEP (sound).
#define ATP_GJ_MAX_VARS 6
// Max atoms in a Model = CP variables + relevant constants.
#define ATP_GJ_MAX_ATOMS 16
// Per-side symbolic-rewrite step cap during a single join attempt.
#define ATP_GJ_NORM_CAP 64
// Cap on the number of branches processed by the groundJoin driver
// before bailing to KEEP (sound budget fallback, gj_spec.md C).
#define ATP_GJ_BRANCH_CAP 4096

// Collect distinct TAG_FVR var ids appearing in `t` into ids[] (size
// cap).  *n is the running count; returns 0 if it would overflow `cap`
// (caller treats overflow as "too many vars -> keep").
static u8 atp_gj_collect_vars(Term t, u32 *ids, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      for (u32 i = 0; i < *n; i++) {
        if (ids[i] == id) return 1;       // already seen
      }
      if (*n >= cap) return 0;            // too many distinct vars
      ids[*n] = id;
      (*n)++;
      return 1;
    }
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      for (u32 i = 0; i < k; i++) {
        if (!atp_gj_collect_vars(term_ctr_at(t, i), ids, n, cap)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// ── Atoms ───────────────────────────────────────────────────────────
// An atom is a variable (TAG_FVR id) or a nullary constant (TAG_CTR
// label, arity 0).  We compare atom SIZES under KBO: a constant's size
// is its fixed weight; a variable's size is an unknown integer >= the
// minimal admissible ground size.  The constraint C orders atoms by size.
typedef struct {
  u8  is_const;   // 1 = constant (TAG_CTR/0), 0 = variable
  u32 id;         // var id (is_const=0) or label (is_const=1)
} GjAtom;

static inline u8 gj_atom_eq(GjAtom a, GjAtom b) {
  return a.is_const == b.is_const && a.id == b.id;
}

// Size of a constant atom under the (real) KBO config.  Constants have
// no children, so their size is just their per-symbol weight.
static inline u32 gj_const_size(const KboConfig *cfg, u32 label) {
  return (label < cfg->n_labels) ? cfg->weights[label] : 0u;
}

// The minimal admissible ground-term size (KBO w0): any ground term has
// size >= var_weight (a single variable's image is at least one minimal
// symbol).  Used as the universal lower bound for an UNCONSTRAINED
// variable's size.
static inline u32 gj_min_size(const KboConfig *cfg) {
  // The smallest possible ground term is a single minimal-weight
  // constant; if every constant outweighs var_weight, a 1-arg term over
  // a 0-weight unary symbol could still be var_weight.  var_weight (w0)
  // is the safe, KBO-standard lower bound (P1: weights >= w0 > 0 except
  // possibly one unary symbol of weight 0, whose argument is >= w0).
  return cfg->var_weight;
}

// ── Model ───────────────────────────────────────────────────────────
// A Model is a total preorder on a fixed atom set: each atom carries a
// `major` size-rank.  a < b  iff  major(a) < major(b);  a == b (a tie,
// same SIZE) iff major(a) == major(b).  Atoms with the same major form
// one "tie group" (all forced to equal size).  (Twee carries a second
// `minor` field to make the preorder a strict total order on atoms for
// its solver; the size-reasoning here needs only `major`, so we drop it.)
typedef struct {
  GjAtom atom[ATP_GJ_MAX_ATOMS];
  u32    major[ATP_GJ_MAX_ATOMS];   // size-rank; equal major = equal size
  u32    n;
} GjModel;

// Find atom `a` in the model; return its index or -1.
static int gj_model_find(const GjModel *m, GjAtom a) {
  for (u32 i = 0; i < m->n; i++) if (gj_atom_eq(m->atom[i], a)) return (int)i;
  return -1;
}

// Compare two atoms' SIZES under the model: -1 (a<b), 0 (tie), 1 (a>b),
// or 2 = UNKNOWN (one or both atoms absent from the model -> C says
// nothing about them).
static int gj_model_cmp(const GjModel *m, GjAtom a, GjAtom b) {
  if (gj_atom_eq(a, b)) return 0;
  int ia = gj_model_find(m, a), ib = gj_model_find(m, b);
  if (ia < 0 || ib < 0) return 2;
  u32 ma = m->major[ia], mb = m->major[ib];
  if (ma < mb) return -1;
  if (ma > mb) return 1;
  return 0;
}

// ── KBO size as a linear form, minimised under C (gj_spec.md D) ──────
// We compare KBO sizes of t and u.  D = size(u) - size(t) is a linear
// form  c0 + sum_v coeff[v] * size(v)  where c0 is the net weight of
// FUNCTION-SYMBOL occurrences (variables excluded) and coeff[v] =
// count(u,v) - count(t,v).  size(v) is the unknown ground size of v.
//
// gj_size_form accumulates D's pieces by a diff-traversal of (t,u):
//   *c0          += net constant/funcsym weight  (u on +, t on -)
//   var_coeff[id] += net variable count          (u on +, t on -)
// Returns 0 if a variable id exceeds the table; caller treats that as
// "cannot decide" (Incomparable -> KEEP-safe).
static void gj_form_addto(Term t, int sign, const KboConfig *cfg,
                          long long *c0, long long *var_coeff) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) var_coeff[id] += sign;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      *c0 += (long long)sign *
             (long long)((lab < cfg->n_labels) ? cfg->weights[lab] : 0);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        gj_form_addto(term_ctr_at(t, i), sign, cfg, c0, var_coeff);
      return;
    }
    default: return;
  }
}

// minimumIn (gj_spec.md D, note "kbo under assumptions").  Compute the
// MINIMUM of D = c0 + sum_v coeff[v]*size(v) over every assignment of
// variable sizes consistent with the model `m` (and every grounding of
// unconstrained variables).  *finite is set 0 if the minimum is
// unbounded below (-infinity).  On *finite==0 the returned value is
// meaningless.  This is the load-bearing soundness primitive: an
// UNDER-estimate of the true minimum is the unsafe direction (it could
// claim D>0 strict when some sigma drives D<=0 -- an OVER-approximation
// of >=_C).  We therefore compute the EXACT minimum; any case we cannot
// bound exactly is reported as -infinity (KEEP-safe).
//
// Method: variables constrained by C lie in tie/order groups separated
// by constants along the size order.  We process the model's atoms in
// ascending size (major) order.  Constants act as fixed lower/upper
// pivots; the variables between two consecutive constants form a group
// with lower bound lo = size(lower constant) (or w0 if none) and upper
// bound hi = size(upper constant) (or none).  Within a group the
// variables are size-ordered by their major: x in the group satisfies
// lo <= size(x) and (if hi exists) size(x) <= hi, and the group's
// variables are nondecreasing in size.  Per spec D, with coeffs listed
// in ascending group order and sums = scanr1(+) (suffix sums):
//   all suffix sums >= 0:  min = (sum coeffs) * lo
//   some suffix sum < 0, no hi:  -infinity
//   some suffix sum < 0, hi exists: min = (sum coeffs)*lo
//                                      + (-min_suffix)*(lo - hi)
// Variables NOT in the model ("orphans") with coeff k: k<0 -> -infinity;
// else contribute 0 (size driven to its own minimum w0, but coeff>=0 so
// the minimal contribution toward D is taking size as small as possible;
// orphan min-contribution per spec = 0 for k>=0 since unconstrained
// orphan can be made equal to the global infimum which is folded into
// c0 already via... ) -- see note below.
//
// Returns the (finite) minimum; sets *finite.
static long long gj_minimum_in(const GjModel *m, const KboConfig *cfg,
                               const long long *var_coeff,
                               const u32 *cp_vars, u32 n_cp_vars,
                               long long c0, u8 *finite) {
  *finite = 1;
  long long total = c0;
  long long w0 = (long long)gj_min_size(cfg);

  // Orphan variables: present in the CP but absent from the model.
  // Their size is any value >= w0, unconstrained relative to others.
  //   coeff < 0  -> size can grow without bound -> -infinity.
  //   coeff > 0  -> minimised by size = w0 -> contributes coeff*w0.
  //   coeff == 0 -> contributes 0.
  for (u32 i = 0; i < n_cp_vars; i++) {
    u32 id = cp_vars[i];
    GjAtom va = { .is_const = 0, .id = id };
    if (gj_model_find(m, va) >= 0) continue;          // not an orphan
    long long k = (id < KBO_MAX_VAR) ? var_coeff[id] : 0;
    if (k < 0) { *finite = 0; return 0; }
    total += k * w0;
  }

  // Walk the model's atoms in ascending size order.  Group variables
  // between constants.  We sort indices by major (stable; minor breaks
  // ties but same-major atoms are one group regardless of minor).
  u32 order[ATP_GJ_MAX_ATOMS];
  for (u32 i = 0; i < m->n; i++) order[i] = i;
  for (u32 i = 1; i < m->n; i++) {                    // insertion sort
    u32 key = order[i];
    u32 km = m->major[key];
    int j = (int)i - 1;
    while (j >= 0 && m->major[order[j]] > km) { order[j + 1] = order[j]; j--; }
    order[j + 1] = key;
  }

  // Scan groups.  A group is a maximal run of consecutive (in size
  // order) VARIABLE atoms; the constant immediately before sets lo, the
  // constant immediately after sets hi.  Multiple variables tied with a
  // constant (same major as a constant) are pinned to that constant's
  // size -- handled by treating a same-major constant as both bound.
  u32 i = 0;
  long long cur_lo = w0;   // running lower bound (last constant size, or w0)
  while (i < m->n) {
    u32 ai = order[i];
    if (m->atom[ai].is_const) {
      cur_lo = (long long)gj_const_size(cfg, m->atom[ai].id);
      i++;
      continue;
    }
    // Start of a variable group at size-order position i.  Collect the
    // group: consecutive variables.  Note variables tied (same major)
    // with each other share a size; we still list each coeff (they all
    // share one unknown, but the suffix-sum derivation treats them as a
    // nondecreasing chain which a tie satisfies, so listing each is
    // correct -- ties just collapse y_i deltas to 0, never changing the
    // suffix-sum sign test).
    long long coeffs[ATP_GJ_MAX_ATOMS];
    u32 ng = 0;
    while (i < m->n && !m->atom[order[i]].is_const) {
      u32 vid = m->atom[order[i]].id;
      long long k = (vid < KBO_MAX_VAR) ? var_coeff[vid] : 0;
      coeffs[ng++] = k;
      i++;
    }
    // Upper bound: the next atom (if a constant) gives hi; if the next
    // atom is past the end, no upper bound.  (The next atom can only be
    // a constant here, since we consumed all consecutive variables.)
    u8 has_hi = 0;
    long long hi = 0;
    if (i < m->n && m->atom[order[i]].is_const) {
      has_hi = 1;
      hi = (long long)gj_const_size(cfg, m->atom[order[i]].id);
    }
    // suffix sums
    long long sum = 0;
    long long min_suffix = 0;       // minimum over suffix sums (incl. empty=+inf guard)
    u8 have_min = 0;
    long long suff[ATP_GJ_MAX_ATOMS];
    long long acc = 0;
    for (int g = (int)ng - 1; g >= 0; g--) { acc += coeffs[g]; suff[g] = acc; }
    for (u32 g = 0; g < ng; g++) {
      sum += coeffs[g];
      if (!have_min || suff[g] < min_suffix) { min_suffix = suff[g]; have_min = 1; }
    }
    // sum == total coeff of the group; min_suffix == min suffix sum.
    if (ng == 0) { cur_lo = has_hi ? hi : cur_lo; continue; }
    if (min_suffix >= 0) {
      total += sum * cur_lo;
    } else if (!has_hi) {
      *finite = 0; return 0;        // unbounded below
    } else {
      total += sum * cur_lo + (-min_suffix) * (cur_lo - hi);
    }
    cur_lo = has_hi ? hi : cur_lo;
  }
  return total;
}

// sizeLessIn: decide size(t) vs size(u) under C.
//   returns  -1 : size(t) <  size(u) for all sigma  (Strict on size)
//             0 : size(t) == size(u) for all sigma  (fall through to lex)
//             1 : not provable either way            (Incomparable)
// Per spec D: L = minimum of D = size(u)-size(t) over sigma|=C.
//   L >  0 -> Strict;  L == 0 -> Nonstrict (lex);  L < 0 / -inf -> Incomp.
static int gj_size_less_in(const GjModel *m, const KboConfig *cfg,
                           Term t, Term u,
                           const u32 *cp_vars, u32 n_cp_vars) {
  long long c0 = 0;
  long long var_coeff[KBO_MAX_VAR];
  for (u32 i = 0; i < KBO_MAX_VAR; i++) var_coeff[i] = 0;
  // D = size(u) - size(t): u on +, t on -.
  gj_form_addto(u, +1, cfg, &c0, var_coeff);
  gj_form_addto(t, -1, cfg, &c0, var_coeff);
  u8 finite = 0;
  long long L = gj_minimum_in(m, cfg, var_coeff, cp_vars, n_cp_vars, c0,
                              &finite);
  if (!finite) return 1;            // unbounded below -> Incomparable
  if (L > 0) return -1;             // size(t) < size(u) always
  if (L == 0) return 0;             // equal size for all sigma -> lex
  return 1;                         // L < 0: not provable -> Incomparable
}

// ── lexLessIn (gj_spec.md D step 2), reached only when sizes are
// FORCED equal.  Returns one of GJ_STRICT / GJ_NONSTRICT / GJ_INCOMP
// meaning t < u strictly / t <= u (and == for some sigma) / unknown.
enum { GJ_STRICT = 0, GJ_NONSTRICT = 1, GJ_INCOMP = 2 };

static GjAtom gj_term_atom(Term t, u8 *ok) {
  GjAtom a = {0};
  if (term_tag(t) == TAG_FVR) { a.is_const = 0; a.id = term_ext(t); *ok = 1; }
  else if (term_tag(t) == TAG_CTR && term_ctr_n(t) == 0) {
    a.is_const = 1; a.id = term_ext(t); *ok = 1;
  } else *ok = 0;
  return a;
}

// lessEqInModel for two atoms: is atom(t) <= atom(u) (size) under C?
// returns GJ_STRICT (t<u), GJ_NONSTRICT (t==u forced), or GJ_INCOMP.
static int gj_atom_less_eq(const GjModel *m, GjAtom at, GjAtom au) {
  if (gj_atom_eq(at, au)) return GJ_NONSTRICT;
  int c = gj_model_cmp(m, at, au);
  if (c == -1) return GJ_STRICT;
  if (c == 0)  return GJ_NONSTRICT;  // tie group: equal size
  return GJ_INCOMP;                  // 1 (a>b) or 2 (unknown)
}

static int gj_lex_less_in(const GjModel *m, const KboConfig *cfg,
                          Term t, Term u,
                          const u32 *cp_vars, u32 n_cp_vars, u32 depth);

// lessIn(cond, t, u): size-first then lex.  Returns the strictness of
// "t <= u under C" or GJ_INCOMP.
//   GJ_STRICT    : tσ <  uσ for ALL σ |= C
//   GJ_NONSTRICT : tσ <= uσ for ALL σ |= C
//   GJ_INCOMP    : not provable
static int gj_less_in(const GjModel *m, const KboConfig *cfg,
                      Term t, Term u, const u32 *cp_vars, u32 n_cp_vars,
                      u32 depth) {
  if (kbo_eq(t, u)) return GJ_NONSTRICT;
  int sz = gj_size_less_in(m, cfg, t, u, cp_vars, n_cp_vars);
  if (sz == -1) return GJ_STRICT;     // size(t) < size(u) strictly
  if (sz == 1)  return GJ_INCOMP;     // size not provably <=
  // sizes forced equal -> lexicographic
  return gj_lex_less_in(m, cfg, t, u, cp_vars, n_cp_vars, depth);
}

static int gj_lex_less_in(const GjModel *m, const KboConfig *cfg,
                          Term t, Term u,
                          const u32 *cp_vars, u32 n_cp_vars, u32 depth) {
  if (depth > 64) return GJ_INCOMP;   // recursion guard -> KEEP-safe
  if (kbo_eq(t, u)) return GJ_NONSTRICT;

  u8 t_atom = 0, u_atom = 0;
  GjAtom at = gj_term_atom(t, &t_atom);
  GjAtom au = gj_term_atom(u, &u_atom);

  // Both atoms: direct order from C.
  if (t_atom && u_atom) return gj_atom_less_eq(m, at, au);

  // t atom, u compound: if some atomic proper subterm v of u has t <= v
  // under C (strictly or tied), then t < u strictly (since |u| has the
  // same size and a larger structure dominating t).  Conservative:
  // require an atomic subterm v with gj_atom_less_eq(t,v) != INCOMP.
  if (t_atom && !u_atom && term_tag(u) == TAG_CTR) {
    u32 n = term_ctr_n(u);
    for (u32 i = 0; i < n; i++) {
      Term v = term_ctr_at(u, i);
      u8 vok = 0; GjAtom av = gj_term_atom(v, &vok);
      if (vok) {
        int r = gj_atom_less_eq(m, at, av);
        if (r != GJ_INCOMP) return GJ_STRICT;
      }
    }
    return GJ_INCOMP;
  }

  // t compound, u atom: symmetric -- t > u, so "t <= u" is not provable.
  if (!t_atom && u_atom) return GJ_INCOMP;

  // Both compound: compare heads.
  if (term_tag(t) == TAG_CTR && term_tag(u) == TAG_CTR) {
    u32 lt = term_ext(t), lu = term_ext(u);
    if (lt == lu) {
      u32 nt = term_ctr_n(t), nu = term_ctr_n(u);
      if (nt != nu) return GJ_INCOMP;
      // left-to-right; at the first differing pair recurse with lessIn.
      for (u32 i = 0; i < nt; i++) {
        Term ct = term_ctr_at(t, i), cu = term_ctr_at(u, i);
        if (kbo_eq(ct, cu)) continue;
        int r = gj_less_in(m, cfg, ct, cu, cp_vars, n_cp_vars, depth + 1);
        if (r == GJ_STRICT)    return GJ_STRICT;
        if (r == GJ_NONSTRICT) {
          // The spec unifies the tied arg pair and continues; we can
          // only continue safely if the remaining args make the verdict.
          // Conservatively, a Nonstrict (could be equal) at this arg
          // means we need the SAME pair to also satisfy the converse to
          // continue; without a unifier we report INCOMP unless this is
          // the last differing arg AND it is provably tied.  Since
          // kbo_eq already filtered equal args, a Nonstrict here that is
          // not Strict means "<= but maybe =", so continuing requires the
          // arg to be forced-equal.  We treat Nonstrict as "continue only
          // if no later arg differs"; otherwise INCOMP (KEEP-safe).
          u8 later_diff = 0;
          for (u32 j = i + 1; j < nt; j++) {
            if (!kbo_eq(term_ctr_at(t, j), term_ctr_at(u, j))) {
              later_diff = 1; break;
            }
          }
          if (later_diff) return GJ_INCOMP;
          return GJ_NONSTRICT;
        }
        return GJ_INCOMP;   // INCOMP at first differing arg
      }
      return GJ_NONSTRICT;  // all args equal (shouldn't reach: kbo_eq above)
    }
    // different heads, sizes equal: precedence decides.
    u32 pt = (lt < cfg->n_labels) ? cfg->precedence[lt] : 0;
    u32 pu = (lu < cfg->n_labels) ? cfg->precedence[lu] : 0;
    if (pt < pu) return GJ_STRICT;     // t's head precedes u's -> t < u
    return GJ_INCOMP;
  }

  return GJ_INCOMP;
}

// Does variable id `id` occur in `t`?  (Local copy so GJ does not depend
// on the ATP_ORDERED_REWRITE/ATP_MNF build flags.)
static int gj_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) if (gj_has_var(term_ctr_at(t, i), id)) return 1;
      return 0;
    }
    default: return 0;
  }
}

// Is every TAG_FVR variable of `sub` also a variable of `sup`?  Used to
// reject malformed (extra-variable) rule applications during GJ rewrite.
static u8 gj_vars_subset(Term sub, Term sup) {
  switch (term_tag(sub)) {
    case TAG_FVR: return gj_has_var(sup, term_ext(sub)) ? 1 : 0;
    case TAG_CTR: {
      u32 n = term_ctr_n(sub);
      for (u32 i = 0; i < n; i++)
        if (!gj_vars_subset(term_ctr_at(sub, i), sup)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// ── C-parameterised rewriting (gj_spec.md C/D) ──────────────────────
// A rule l->r fires under model C at a matched position iff
// (l-image) >=_C (r-image) -- i.e. gj_less_in(C, r-image, l-image) is
// GJ_STRICT or GJ_NONSTRICT -- AND the two images are syntactically
// distinct.  Oriented rules (l > r unconditionally) also satisfy this,
// so the single test suffices for both.  Returns the rewritten term;
// sets *fired on success.
// WM DreieckOK (Grundzusammenfuehrung.c:235-240): rule LHS `l` is
// STRICTLY more general than the protection anchor `t0` -- some sigma
// maps l onto t0, and none maps t0 onto l (proper encompassment at the
// root).  Root steps on a protected side of a fact-level GZ test are
// admissible only through such rules, the well-foundedness side
// condition of the AHL-2003 ground-joinability redundancy proof.
static u8 atp_gj_strictly_more_general(Term rule_lhs, Term anchor) {
  RewriteSubst onto = {{0}};
  if (!thvm_match(rule_lhs, anchor, &onto)) return 0;
  RewriteSubst back = {{0}};
  return thvm_match(anchor, rule_lhs, &back) ? 0u : 1u;
}

// `protect`: nonzero only at the ROOT of a protected side during a
// fact-level (backward/forward -gj) GZ test -- the victim's ORIGINAL
// side, anchoring the Dreieck gate (WM MO_schuetzeTerm +
// NF_geschuetzteNormalformRE, NFBildung.c:767-779: "bei toplevel
// werden nur Regeln/Gleichungen verwendet, die in der Dreiecksordnung
// kleiner sind").  Child recursion clears it: only root steps are
// gated.  The forward CP-drop path always passes 0.
static Term gj_rewrite_step(AtpState *s, const GjModel *m, Term t,
                            const u32 *cp_vars, u32 n_cp_vars, u8 *fired,
                            Term protect) {
  const KboConfig *cfg = s->kbo;
  for (u32 i = 0; i < s->n_rules; i++) {
    // A well-formed rewrite rule never has a bare-variable LHS (it would
    // match and "rewrite" everything).  Skip such malformed rules so the
    // GJ test is robust even if handed an ill-formed rule set.
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;
    // Fact-level GZ test: the victim may not reduce itself (WM
    // DarfNichtReduzieren, Hauptkomponenten.c:267/:287, consumed in
    // MatchOperationen.c:641/742/1660/1697).
    if (i == s->gj_exclude) continue;
    // Dreieck root gate on a protected side.
    if (protect != 0 && !atp_gj_strictly_more_general(s->lhs[i], protect)) {
      continue;
    }
    RewriteSubst subst = {{0}};
    if (thvm_match(s->lhs[i], t, &subst)) {
      Term reduct = thvm_subst_apply(s->rhs[i], &subst);
      // Defense-in-depth: a well-formed rewrite rule has vars(rhs) subset
      // of vars(lhs), so a successful match binds every rhs variable and
      // `reduct` is variable-free over the rule's own variables.  If the
      // rule introduces an extra variable (malformed input), `reduct`
      // would carry an unbound variable and corrupt the join comparison.
      // Skip such a step: every variable of `reduct` must also occur in
      // `t` (the term we are rewriting).  No-op for well-formed rules.
      if (!gj_vars_subset(reduct, t)) continue;
      if (kbo_eq(reduct, t)) continue;     // no-op image
      // fire iff t (=l-image) >=_C reduct (=r-image), i.e.
      // gj_less_in(reduct, t) is defined (reduct <= t under C).
      int r = gj_less_in(m, cfg, reduct, t, cp_vars, n_cp_vars, 0);
      if (r == GJ_STRICT || r == GJ_NONSTRICT) {
        *fired = 1;
        return reduct;
      }
    }
  }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      u8 sub_fired = 0;
      Term rc = gj_rewrite_step(s, m, children[i], cp_vars, n_cp_vars,
                                &sub_fired, 0);
      if (sub_fired) {
        children[i] = rc;
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

// Normalise `t` under model C with C-parameterised steps (step_cap).
// `protect` (0 = none) anchors the Dreieck root gate for the whole
// pass -- WM's MO_schuetzeTerm spans the entire NF_NormalformRE call.
// (WM additionally LIFTS protection for deeper instantiation levels
// once a pass reduced the side -- a permissive refinement; keeping the
// gate for the whole fact test only under-sterilizes, never over.)
static Term gj_normalize(AtpState *s, const GjModel *m, Term t,
                         const u32 *cp_vars, u32 n_cp_vars, u32 step_cap,
                         Term protect) {
  for (u32 k = 0; k < step_cap; k++) {
    u8 fired = 0;
    Term t2 = gj_rewrite_step(s, m, t, cp_vars, n_cp_vars, &fired, protect);
    if (!fired) return t;
    t = t2;
  }
  return t;
}

// Join the two CP sides under model C: normalise both and compare.
// The per-side protection anchors live on AtpState (constant for one
// fact-level GZ test; 0 on the forward CP-drop path): the recursion in
// gj_cover never swaps side order, so side 0 is always the victim-LHS
// face and side 1 the victim-RHS face.
static u8 gj_joins_under(AtpState *s, const GjModel *m, Term lhs, Term rhs,
                         const u32 *cp_vars, u32 n_cp_vars) {
  Term nl = gj_normalize(s, m, lhs, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP,
                         s->gj_protect_l);
  Term nr = gj_normalize(s, m, rhs, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP,
                         s->gj_protect_r);
  return kbo_eq(nl, nr);
}

// ── Coverage driver (gj_spec.md C) ──────────────────────────────────
// We must verify joinability under EVERY total preorder of the CP's
// atoms (variables + relevant constants).  Coverage (P3) is realised by
// EXHAUSTIVE enumeration, structured exactly as Twee's groundJoin:
//
//  * The variables are partitioned by a total preorder (ordered set
//    partition).  When two variables fall in the SAME class they are
//    forced EQUAL in SIZE; for KBO that is genuinely an `=` case, and
//    Twee step 5 handles it by UNIFYING the equal variables and
//    recursing on the smaller CP.  We do exactly that: collapse each
//    multi-variable class to a single representative (lowest id), apply
//    the var->representative substitution to both CP sides, and recurse
//    gj_cover on the reduced CP.  The recursion re-covers every sub-
//    ordering (incl. further ties and constant placements) of the
//    representatives, so the `=` region is fully covered.  Termination:
//    each recursion strictly drops the variable count.
//
//  * When all variables are in DISTINCT classes (a strict total order on
//    the variables), there is no `=` to discharge.  We then interleave
//    the relevant CONSTANTS into that strict variable chain in every
//    order (constants among themselves are pinned by their fixed
//    weights), build the resulting Model, and run the SYMBOLIC join:
//    gj_less_in reasons about ALL grounding sigma consistent with the
//    order (including the `<=` no-op steps), which is what lets the AC
//    pair join where a single ground representative could not (F.1).
//
// Soundness: gj_less_in never OVER-approximates >=_C (P2), so no illegal
// rewrite ever fires; the enumeration covers every ground ordering of
// vars (ties via unify-recurse, strict via models) and every constant
// placement (P3); any budget overflow / OOM / unjoined case returns 0
// (KEEP).  Constants of different weight are never asserted equal (the
// model rejects such an unsatisfiable preorder), so the unsound
// "merge two constants" never happens.

// Collect distinct nullary-constant labels in `t`.
static u8 gj_collect_consts(Term t, u32 *labs, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      if (k == 0) {
        u32 lab = term_ext(t);
        for (u32 i = 0; i < *n; i++) if (labs[i] == lab) return 1;
        if (*n >= cap) return 0;
        labs[(*n)++] = lab;
        return 1;
      }
      for (u32 i = 0; i < k; i++)
        if (!gj_collect_consts(term_ctr_at(t, i), labs, n, cap)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// Build a Model from a STRICT order on variables interleaved with the
// CP's constants.  `atoms[0..n_atoms)` lists the model atoms; `cls[i]`
// is atom i's size-rank (class).  Distinct VARIABLES must have distinct
// classes here (the caller guarantees a strict variable order).
// Returns 0 (reject this preorder as unsatisfiable) if it ties two
// constants of different weight, or orders two constants against their
// fixed-weight order -- no ground sigma realises such a C, so skipping
// it loses no coverage (it vacuously joins).
static u8 gj_build_model(GjModel *m, const GjAtom *atoms, u32 n_atoms,
                         const u32 *cls, const KboConfig *cfg) {
  for (u32 i = 0; i < n_atoms; i++) {
    if (!atoms[i].is_const) continue;
    for (u32 j = 0; j < n_atoms; j++) {
      if (!atoms[j].is_const) continue;
      u32 si = gj_const_size(cfg, atoms[i].id);
      u32 sj = gj_const_size(cfg, atoms[j].id);
      if (cls[i] < cls[j] && si > sj) return 0;
      if (cls[i] > cls[j] && si < sj) return 0;
      if (cls[i] == cls[j] && si != sj) return 0;
    }
  }
  m->n = n_atoms;
  for (u32 i = 0; i < n_atoms; i++) {
    m->atom[i]  = atoms[i];
    m->major[i] = cls[i];
  }
  return 1;
}

// Given a STRICT total order on the variables (var at atom-index i has
// rank var_rank[i]), interleave the constants among the variables in
// every position and run the symbolic join under each resulting Model.
// Returns 1 iff all join.
//
// Coarse placement: variable at rank r gets coarse slot 2r+1.  Each
// constant is independently placed into a coarse slot in [0, 2*nvars]:
//   even slot 2k  = strictly between var-rank-(k-1) and var-rank-k
//                   (slot 0 = below all vars, 2*nvars = above all);
//   odd slot 2k+1 = tied (equal size) with the variable at rank k.
// This covers every position of a constant relative to the strict
// variable chain.  Two or more constants landing in the SAME even slot
// (the same gap) are then SUB-ORDERED by their fixed weights: distinct
// weights -> distinct majors in weight order; equal weights -> tied
// (the true ground fact).  This closes the coverage gap that a single
// shared even slot would otherwise create (two different-weight
// constants in one gap must still be tested in their forced order).
// Constants tied with a variable (odd slot) keep that exact major (a
// ground term genuinely can have a variable image's size).
// gj_build_model rejects placements that contradict the constants'
// fixed weight order (an unsatisfiable preorder -> skipped, vacuously
// joinable, no coverage lost).
//
// Final majors are produced by sorting all atoms on the key
// (coarse_slot, weight_if_const_in_even_slot) and assigning a dense
// rank that increments only when the key strictly increases, so equal
// keys share a major (a tie) and strict keys get distinct majors.
static u8 gj_cover_consts(AtpState *s, Term lhs, Term rhs,
                          const GjAtom *atoms, u32 n_atoms,
                          const u32 *var_rank,  // var_rank[i] for var atoms
                          u32 n_vars_model,
                          const u32 *cp_vars, u32 n_cp_vars, u32 *budget) {
  const KboConfig *cfg = s->kbo;
  u32 const_idx[ATP_GJ_MAX_ATOMS];
  u32 nc = 0;
  for (u32 i = 0; i < n_atoms; i++) if (atoms[i].is_const) const_idx[nc++] = i;

  u32 grid = 2u * n_vars_model + 1u;     // coarse const slot range [0, grid)
  u32 place[ATP_GJ_MAX_ATOMS];
  for (u32 c = 0; c < nc; c++) place[c] = 0;

  for (;;) {
    if (*budget == 0) return 0;
    (*budget)--;

    // coarse slot per atom
    u32 coarse[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++)
      if (!atoms[i].is_const) coarse[i] = 2u * var_rank[i] + 1u;
    for (u32 c = 0; c < nc; c++) coarse[const_idx[c]] = place[c];

    // sub-order key: for a constant in an EVEN coarse slot, break ties
    // between same-slot constants by weight; everything else keeps a 0
    // secondary key (vars, and constants tied with a var, are pinned to
    // their slot).
    long long key2[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++) {
      if (atoms[i].is_const && (coarse[i] % 2u) == 0u)
        key2[i] = (long long)gj_const_size(cfg, atoms[i].id);
      else
        key2[i] = 0;
    }

    // dense-rank by (coarse, key2): sort indices, assign majors.
    u32 ord[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++) ord[i] = i;
    for (u32 i = 1; i < n_atoms; i++) {
      u32 k = ord[i]; int j = (int)i - 1;
      while (j >= 0 &&
             (coarse[ord[j]] > coarse[k] ||
              (coarse[ord[j]] == coarse[k] && key2[ord[j]] > key2[k]))) {
        ord[j + 1] = ord[j]; j--;
      }
      ord[j + 1] = k;
    }
    u32 cls[ATP_GJ_MAX_ATOMS];
    u32 cur = 0;
    for (u32 r = 0; r < n_atoms; r++) {
      if (r > 0) {
        u32 a = ord[r - 1], b = ord[r];
        if (coarse[a] != coarse[b] || key2[a] != key2[b]) cur++;
      }
      cls[ord[r]] = cur;
    }

    GjModel m;
    if (gj_build_model(&m, atoms, n_atoms, cls, cfg)) {
      if (!gj_joins_under(s, &m, lhs, rhs, cp_vars, n_cp_vars)) return 0;
    }

    if (nc == 0) return 1;
    u32 c = 0;
    for (; c < nc; c++) {
      if (++place[c] < grid) break;
      place[c] = 0;
    }
    if (c == nc) return 1;
  }
}

// Recursively cover all total preorders of the CP's variables.  Strategy
// (gj_spec.md C steps 1-5):
//   * Enumerate ordered set partitions of the variables.
//   * A partition with two variables in one class is an `=` case: unify
//     (substitute each tied variable to the class representative) and
//     RECURSE on the smaller CP.
//   * A strict (all-singleton) partition: interleave constants and run
//     the symbolic join (gj_cover_consts).
// Returns 1 iff every case joins.
static u8 gj_cover(AtpState *s, Term lhs, Term rhs,
                   const u32 *var_ids, u32 n_vars, u32 *budget) {
  // Re-collect the constants of the (current, possibly substituted) CP.
  u32 const_labs[ATP_GJ_MAX_ATOMS];
  u32 n_consts = 0;
  if (!gj_collect_consts(lhs, const_labs, &n_consts, ATP_GJ_MAX_ATOMS)) return 0;
  if (!gj_collect_consts(rhs, const_labs, &n_consts, ATP_GJ_MAX_ATOMS)) return 0;

  if (n_vars == 0) {
    // Ground CP (after all unifications): build a model over constants
    // only (their order is fixed by weight) and join.  No variable ranks.
    GjAtom atoms[ATP_GJ_MAX_ATOMS];
    u32 n_atoms = 0;
    for (u32 i = 0; i < n_consts; i++) {
      if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
      atoms[n_atoms].is_const = 1; atoms[n_atoms].id = const_labs[i]; n_atoms++;
    }
    u32 dummy_rank[ATP_GJ_MAX_ATOMS] = {0};
    return gj_cover_consts(s, lhs, rhs, atoms, n_atoms, dummy_rank, 0,
                           var_ids, n_vars, budget);
  }

  // Enumerate ordered set partitions of the variables via restricted-
  // growth string cls[] + a permutation of the classes.
  u32 cls[ATP_GJ_MAX_VARS];
  u32 perm[ATP_GJ_MAX_VARS];
  for (u32 i = 0; i < n_vars; i++) cls[i] = 0;

  for (;;) {
    u32 n_cls = 0;
    for (u32 i = 0; i < n_vars; i++) if (cls[i] + 1 > n_cls) n_cls = cls[i] + 1;

    for (u32 i = 0; i < n_cls; i++) perm[i] = i;
    for (;;) {
      if (*budget == 0) return 0;
      u32 ocls[ATP_GJ_MAX_VARS];
      for (u32 i = 0; i < n_vars; i++) ocls[i] = perm[cls[i]];

      if (n_cls < n_vars) {
        // Some class holds >=2 variables -> `=` case.  Unify each class
        // to its representative (lowest-id var in the class) and recurse.
        (*budget)--;
        RewriteSubst sub = {{0}};
        u32 rep_of_class[ATP_GJ_MAX_VARS];
        for (u32 c = 0; c < n_cls; c++) rep_of_class[c] = (u32)-1;
        // Representative = the smallest var id in the class.
        for (u32 i = 0; i < n_vars; i++) {
          u32 c = ocls[i];
          if (rep_of_class[c] == (u32)-1 || var_ids[i] < rep_of_class[c])
            rep_of_class[c] = var_ids[i];
        }
        u32 new_vars[ATP_GJ_MAX_VARS];
        u32 n_new = 0;
        u8 sub_ok = 1;
        for (u32 i = 0; i < n_vars; i++) {
          u32 rep = rep_of_class[ocls[i]];
          if (var_ids[i] != rep) {
            if (var_ids[i] >= REWRITE_MAX_VAR) { sub_ok = 0; break; }
            sub.bindings[var_ids[i]] = term_new_fvr(rep);
          }
        }
        // The reduced variable set = the representatives (one per class).
        for (u32 c = 0; c < n_cls && sub_ok; c++) {
          u8 seen = 0;
          for (u32 k = 0; k < n_new; k++) if (new_vars[k] == rep_of_class[c]) seen = 1;
          if (!seen) new_vars[n_new++] = rep_of_class[c];
        }
        if (sub_ok) {
          Term sl = thvm_subst_apply(lhs, &sub);
          Term sr = thvm_subst_apply(rhs, &sub);
          if (!gj_cover(s, sl, sr, new_vars, n_new, budget)) return 0;
        } else {
          return 0;   // can't form the substitution -> KEEP (sound)
        }
      } else {
        // Strict order on the variables: var i sits at rank ocls[i].
        // Build the atom list (vars + consts) and interleave constants.
        GjAtom atoms[ATP_GJ_MAX_ATOMS];
        u32 var_rank[ATP_GJ_MAX_ATOMS];
        u32 n_atoms = 0;
        for (u32 i = 0; i < n_vars; i++) {
          if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
          atoms[n_atoms].is_const = 0; atoms[n_atoms].id = var_ids[i];
          var_rank[n_atoms] = ocls[i];
          n_atoms++;
        }
        for (u32 i = 0; i < n_consts; i++) {
          if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
          atoms[n_atoms].is_const = 1; atoms[n_atoms].id = const_labs[i];
          var_rank[n_atoms] = 0;
          n_atoms++;
        }
        if (!gj_cover_consts(s, lhs, rhs, atoms, n_atoms, var_rank, n_vars,
                             var_ids, n_vars, budget))
          return 0;
      }

      // next permutation of classes
      if (n_cls < 2) break;
      u32 a = n_cls - 1;
      while (a > 0 && perm[a - 1] >= perm[a]) a--;
      if (a == 0) break;
      u32 b = n_cls - 1;
      while (perm[b] <= perm[a - 1]) b--;
      u32 tmp = perm[a - 1]; perm[a - 1] = perm[b]; perm[b] = tmp;
      u32 lo = a, hi = n_cls - 1;
      while (lo < hi) { tmp = perm[lo]; perm[lo] = perm[hi]; perm[hi] = tmp; lo++; hi--; }
    }

    // next restricted-growth partition
    u32 i = n_vars;
    while (i > 0) {
      i--;
      u32 max_prefix = 0;
      for (u32 j = 0; j < i; j++) if (cls[j] + 1 > max_prefix) max_prefix = cls[j] + 1;
      if (cls[i] < max_prefix) {
        cls[i]++;
        for (u32 j = i + 1; j < n_vars; j++) cls[j] = 0;
        break;
      }
      if (i == 0) return 1;              // exhausted -> all cases joined
      cls[i] = 0;
    }
  }
}

// Top-level: return 1 iff (lhs, rhs) is PROVABLY ground-joinable
// (would-DELETE), 0 otherwise (would-KEEP).  Sound (no false DELETE):
// gj_less_in never over-approximates >=_C (P2), the recursion + strict-
// order/constant-interleave enumeration covers every ground ordering
// (P3), and any uncertainty / budget / overflow returns 0 (KEEP).
static int atp_cp_ground_joinable(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  // No ordering => keep (no GJ verdict can be made).
  if (s->kbo == NULL && s->lpo == NULL && s->rpo == NULL && s->wpo == NULL) return 0;
  // GJ's gj_less_in is implemented for KBO only; under LPO/RPO/WPO the
  // ground-join check is off (caller falls back to keep).
  if (s->lpo != NULL || s->rpo != NULL || s->wpo != NULL) return 0;

  u32 var_ids[ATP_GJ_MAX_VARS];
  u32 n_vars = 0;
  if (!atp_gj_collect_vars(lhs, var_ids, &n_vars, ATP_GJ_MAX_VARS)) return 0;
  if (!atp_gj_collect_vars(rhs, var_ids, &n_vars, ATP_GJ_MAX_VARS)) return 0;

  u32 budget = ATP_GJ_BRANCH_CAP;
  return gj_cover(s, lhs, rhs, var_ids, n_vars, &budget) ? 1 : 0;
}

// ── Fact-level -gj: WM RueckwaertsGrundzusammenfuehrbarkeit ─────────
// (INF/Hauptkomponenten.c:260-306 + the forward halves
// RUndEVerwaltung.c:182-183/:457-460.)  Default OFF, matching WM's
// -gj CLI default (RUN/Parameter.c:317; no strategy table emits the
// YFiles.c:125 `gj()` primitive).  Opt-in: Method
// {... "BackwardGroundJoin" -> True} / THVM_ATP_BWD_GROUND_JOIN.

// Binary CTR destructure: t = f(a, b)?
static u8 atp_gj_bin(Term t, u32 f, Term *a, Term *b) {
  if (term_tag(t) != TAG_CTR || term_ext(t) != f || term_ctr_n(t) != 2) {
    return 0;
  }
  *a = term_ctr_at(t, 0);
  *b = term_ctr_at(t, 1);
  return 1;
}

static u8 atp_gj_var_id(Term t, u32 *id) {
  if (term_tag(t) != TAG_FVR) return 0;
  *id = term_ext(t);
  return 1;
}

// t = f(x, f(y, z)) with all-variable leaves (right-nested).
static u8 atp_gj_shape_rr(Term t, u32 f, u32 *x, u32 *y, u32 *z) {
  Term a, b, c, d;
  if (!atp_gj_bin(t, f, &a, &b) || !atp_gj_bin(b, f, &c, &d)) return 0;
  return atp_gj_var_id(a, x) && atp_gj_var_id(c, y) && atp_gj_var_id(d, z);
}

// t = f(f(x, y), z) with all-variable leaves (left-nested).
static u8 atp_gj_shape_ll(Term t, u32 f, u32 *x, u32 *y, u32 *z) {
  Term a, b, c, d;
  if (!atp_gj_bin(t, f, &a, &b) || !atp_gj_bin(a, f, &c, &d)) return 0;
  return atp_gj_var_id(c, x) && atp_gj_var_id(d, y) && atp_gj_var_id(b, z);
}

// WM PROTECT_3_PERMS (Grundzusammenfuehrung.c:76, consumed :808-814):
// the -gj fact test refuses to sterilize associativity, commutativity,
// and extended-commutativity facts -- GZ_wertvoll, valuable for the
// permutative cleanup even when provably ground-joinable.  Shapes per
// WASIC/TermOperationen.c:1207-1248 (TO_IstAssoziativitaet /
// TO_IstKommutativitaet / TO_IstErweiterteKommutativitaet); thvm rules
// are var-normalized at push, so plain id equality realizes WM's
// positional -1/-2/-3 placeholders.
static u8 atp_gj_perm_valuable(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_ctr_n(l) != 2) return 0;
  u32 f = term_ext(l);
  Term p, q, p2, q2;
  u32 a, b, c, d, x, y, z, u, v, w;
  // C: f(x1, x2) = f(x2, x1)
  if (atp_gj_bin(l, f, &p, &q) && atp_gj_bin(r, f, &p2, &q2) &&
      atp_gj_var_id(p, &a) && atp_gj_var_id(q, &b) &&
      atp_gj_var_id(p2, &c) && atp_gj_var_id(q2, &d) &&
      a != b && c == b && d == a) {
    return 1;
  }
  // A: f(f(x1,x2),x3) = f(x1,f(x2,x3)), either nesting on either side.
  if (atp_gj_shape_ll(l, f, &x, &y, &z) && atp_gj_shape_rr(r, f, &u, &v, &w) &&
      x == u && y == v && z == w && x != y && y != z && x != z) {
    return 1;
  }
  if (atp_gj_shape_rr(l, f, &x, &y, &z) && atp_gj_shape_ll(r, f, &u, &v, &w) &&
      x == u && y == v && z == w && x != y && y != z && x != z) {
    return 1;
  }
  // C': f(x1,f(x2,x3)) = one of the four rotated right-nested perms.
  if (atp_gj_shape_rr(l, f, &x, &y, &z) && atp_gj_shape_rr(r, f, &u, &v, &w) &&
      x != y && y != z && x != z) {
    if ((u == z && v == x && w == y) ||      // f(x3, f(x1, x2))
        (u == y && v == x && w == z) ||      // f(x2, f(x1, x3))
        (u == z && v == y && w == x) ||      // f(x3, f(x2, x1))
        (u == y && v == z && w == x)) {      // f(x2, f(x3, x1))
      return 1;
    }
  }
  return 0;
}

// Classify fact slot `i` per WM's fact-level -gj test:
//   * HOPELESS: <= 1 distinct variable, covering both-sides-ground
//     (MN90Check noVar gate, Grundzusammenfuehrung.c:729-733, + the
//     both-ground precheck :767-771).  Sticky.
//   * VALUABLE: PROTECT_3_PERMS A/C/C' shape.  Sticky.  (WM tags these
//     in the FORWARD test; under -gj every fact passes that test at
//     creation, so the backward walk's `gzfbStatus <= GZ_aussichtslos`
//     skip :824 never re-reaches them -- thvm classifies in the shared
//     path for the same effect.)
//   * JOINABLE: the groundJoin driver proves every ground instance of
//     lhs = rhs joinable under R u E MINUS the victim (gj_exclude =
//     WM DarfNichtReduzieren), root steps on the protected face(s)
//     Dreieck-gated.  Sticky -- the fact is sterile.
//   * FAILED: not shown joinable (also WM's GZ_zuTeuer var-overflow
//     case); retested on later facts.
// Protection per WM Grundzusammenfuehrung.c:841: rules protect the
// LHS face only (sSch -- the RHS face is strictly below in the
// reduction order); unorientable equations protect BOTH faces
// (allesSch; the per-branch comparability resolution that would
// unprotect the smaller instance face is skipped -- conservative,
// see gj_normalize).
static u8 atp_gj_fact_test(AtpState *s, u32 i) {
  Term l = s->lhs[i];
  Term r = s->rhs[i];
  u32 var_ids[ATP_GJ_MAX_VARS];
  u32 n_vars = 0;
  u8 var_overflow =
      !atp_gj_collect_vars(l, var_ids, &n_vars, ATP_GJ_MAX_VARS) ||
      !atp_gj_collect_vars(r, var_ids, &n_vars, ATP_GJ_MAX_VARS);
  if (!var_overflow && n_vars <= 1u) return ATP_GJ_ST_HOPELESS;
  if (atp_gj_perm_valuable(l, r)) return ATP_GJ_ST_VALUABLE;
  if (var_overflow) return ATP_GJ_ST_FAILED;
  s->gj_exclude   = i;
  s->gj_protect_l = l;
  s->gj_protect_r = s->r_orient[i] ? 0 : r;
  int joinable = atp_cp_ground_joinable(s, l, r);
  s->gj_exclude   = ATP_GJ_NO_EXCLUDE;
  s->gj_protect_l = 0;
  s->gj_protect_r = 0;
  return joinable ? ATP_GJ_ST_JOINABLE : ATP_GJ_ST_FAILED;
}

// The driver reasons in KBO sizes only; under LPO/RPO/WPO (or no
// order) every fact test would return KEEP, so skip the walks.
static u8 atp_gj_active(const AtpState *s) {
  return s->kbo != NULL && s->lpo == NULL && s->rpo == NULL &&
         s->wpo == NULL;
}

// Forward -gj fact test at creation (WM RUndEVerwaltung.c:182-183 for
// rules, :457-460 for equations -- Grundzusammenfuehrbar is decided
// BEFORE ArbeitsAufnahme interreduces and generates CPs, so a fact
// ground-joinable at birth never forms a CP at all (Weggefiltert,
// Unifikation1.c:1518/:1588 + :967-972)).  No KillParent here: a
// newborn has no queued children to orphan.
static void atp_gj_classify_added(AtpState *s, AtpAddedRange added) {
  if (!atp_gj_active(s)) return;
  for (u32 k = 0; k < added.count; k++) {
    u32 i = added.first + k;
    if (i >= s->n_rules || s->r_dead[i]) continue;
    u8 st = atp_gj_fact_test(s, i);
    s->r_gj_status[i] = st;
    if (st == ATP_GJ_ST_JOINABLE) s->n_facts_bwd_ground_joinable++;
  }
}

// WM RueckwaertsGrundzusammenfuehrbarkeit (Hauptkomponenten.c:260-306),
// run from the step tail AFTER CP generation + the IR-victim drain
// (ArbeitsAufnahme :327-329).  Walks every EXISTING live fact -- rules
// (:265 RE_forRegelnRobust) and equations (:286, the distinguished
// direction = thvm's single unorientable slot) -- skipping the
// just-added range (`Faktum != Neues`) and the sticky statuses
// (`gzfbStatus <= GZ_aussichtslos`, :824), and re-tests ground
// joinability against the system now extended by the new fact.  A fact
// shown joinable is STERILIZED per the compiled GZ_ZSFB_BEHALTEN=1
// (Grundzusammenfuehrung.h:52): the JOINABLE status excludes it from
// all future CP formation (atp_gen_one's Weggefiltert check) and
// atp_trace_mark_dead orphans its already-queued CPs (KPV_KillParent
// lebtNoch=FALSE, KPVerwaltung.c:343-351 -> selectNonOrphan :540;
// thvm's lazy at-pop discard reads the same bitmap when
// use_orphan_murder is on, the WM -ocrit layout).  The fact itself
// STAYS in R/E and keeps rewriting.  Walks NEITHER queued CPs NOR
// goals -- WM re-tests active facts only.
static void atp_bwd_ground_join_walk(AtpState *s, u32 skip_lo, u32 skip_hi) {
  if (!atp_gj_active(s)) return;
  for (u32 i = 0; i < s->n_rules; i++) {
    if (i >= skip_lo && i < skip_hi) continue;
    if (s->r_dead[i]) continue;
    u8 st = s->r_gj_status[i];
    if (st == ATP_GJ_ST_JOINABLE || st == ATP_GJ_ST_VALUABLE ||
        st == ATP_GJ_ST_HOPELESS) {
      continue;
    }
    st = atp_gj_fact_test(s, i);
    s->r_gj_status[i] = st;
    if (st == ATP_GJ_ST_JOINABLE) {
      s->n_facts_bwd_ground_joinable++;
      atp_trace_mark_dead(s, s->r_trace[i]);
    }
  }
}

#endif  // ATP_CP_GROUND_JOIN

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
#ifndef ATP_CP_DIAG
  (void)rule_a;
  (void)rule_b;
#endif
  // Env-gated CP-formation fate trace: one [cpraw] line per FORMED CP
  // (pre-gates) and one [cpfate] line per treatment verdict / drop --
  // the per-birth diagnostic that pinned the duplicate-CP multiplicity
  // class to the NF-strategy join-verdict flips.
  static int cpraw_debug = -1;
  if (cpraw_debug < 0) cpraw_debug = atp_env_on("THVM_ATP_CPRAW_DEBUG");
  if (cpraw_debug && ncps > 0) {
    fprintf(stderr, "[cpraw] atp_push_cps_traced(parent_a=%u parent_b=%u rule_a=%u rule_b=%u): %u raw CPs\n",
            parent_a, parent_b, rule_a, rule_b, ncps);
    for (u32 i = 0; i < ncps; i++) {
      char la[256], ra[256];
      atp_pretty_term(cps[i].lhs, la, sizeof la);
      atp_pretty_term(cps[i].rhs, ra, sizeof ra);
      fprintf(stderr, "[cpraw]   %u: %s = %s\n", i, la, ra);
    }
  }
  for (u32 i = 0; i < ncps; i++) {
    Term cp_lhs = cps[i].lhs;
    Term cp_rhs = cps[i].rhs;
    // Snapshot the RAW superposition result -- the un-reduced
    // `(σ(l_i[p←r_j]), σ(r_i))` -- var-normalized for clean
    // alpha-canonical ids.  The trace records this (not the reduced
    // CP queued below): a Waldmeister-PCL CriticalPairLemma's
    // Statement is the raw overlap, which the proof verifier
    // re-derives from the two parent rules at the recorded position.
    Term raw_lhs = cp_lhs;
    Term raw_rhs = cp_rhs;
#ifdef ATP_VAR_NORM
    thvm_normalize_vars(&raw_lhs, &raw_rhs);
#endif
    // Reduce the CP w.r.t. R before it lands in the queue: standard
    // completion adds the NORMALIZED critical pair.  atp_cp_trivially_-
    // joinable writes the two normal forms back through cp_lhs/cp_rhs
    // and returns whether they collapsed (joinable -> drop).  A reduced
    // CP is dramatically smaller than the raw overlap, so the KBO
    // priority, the subsumption query, the index insert, and every
    // later retrieval against it all stay cheap.
    u8 joinable;
    // WM KPBehandelt >=50 raw class: queued untreated, weighed on the
    // raw pair, bypasses the auto-MaxWeight stash (see below).
    u8 raw_untreated = 0u;
    u64 _ph_push_t0 = atp_phase_now();
    // MNF gate (kept even though lazy defaults to off now): when the WL
    // surface enables lazy via Method "LazyNormalize" -> True on a config
    // that also runs MNF, the gate routes around the front search's
    // direct-cell-reference Bus error.
    if (s->use_lazy_normalize && !s->use_mnf) {
      // Lazy push = WM `KPBehandelt` (KPVerwaltung.c:439-467) under the
      // `lohntSichBehandlung` gate (:435-438, combined RAW size < 50).
      // BELOW the gate the CP is treated: doR-only full-R normalize
      // (-kg "r") + joined-drop; a survivor queues and weighs on the
      // TREATED form.  AT OR ABOVE the gate WM applies NO treatment --
      // KPBehandelt returns FALSE without touching the pair, the caller
      // (KPV_GebildetesKPBehandelnMitVater/Mutter :478/:501) queues the
      // RAW pair, and recentCPinsert's C_Classify (:396) weighs the RAW
      // sides.  Deep-overlap instances therefore stay buried at raw
      // weight until the heap -- weight order or the FIFO dimension --
      // genuinely reaches them.  Shrinking-and-reweighing them instead
      // pulls mini-CPs thousands of selections early and detonates an
      // interreduction avalanche (the andassoc picks-313-332 trajectory
      // fork: rules 128 -> 71 where WM's whole run removes ~5).
      // Joinability for the raw class is decided at selection (the
      // kbo_eq check after the pop normalize), so no non-joinable CP is
      // dropped -- completeness preserved.  The <50 treatment is what
      // keeps the queue bounded: small CPs are cheap to normalize AND
      // have a high joinable-rate, so joining them at push drops them
      // before the queue/index/subsumption work fires.
      const u32 WM_BEHANDELN_GATE = 50u;   // KPVerwaltung.c:437
      u32 raw_sz = atp_symbol_count(cp_lhs) + atp_symbol_count(cp_rhs);
      if (raw_sz < WM_BEHANDELN_GATE) {
        joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
        s->n_cps_push_normalized++;
      } else {
        joinable = 0u;
        raw_untreated = 1u;
      }
    } else {
      joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
      s->n_cps_push_normalized++;
    }
    if (g_atp_phase_enabled) g_atp_phase_us_push_normalize += atp_now_us() - _ph_push_t0;
#ifdef ATP_VAR_NORM
    // 7c: canonically renumber the CP's variables -- AFTER reduction,
    // since rewriting can drop variables, so the dense [0, k) set is
    // computed on the form actually queued.  The CP enumerator bakes
    // CP_RENAME_OFFSET into the stored term, carrying ids past
    // REWRITE_MAX_VAR where the matcher goes dead; renumbering keeps
    // every stored var matchable AND makes alpha-equivalent CPs
    // byte-identical so the subsumption filters below actually fire.
    thvm_normalize_vars(&cp_lhs, &cp_rhs);
#endif
    // Permutation-subsumption (WM GZ_ACVerzichtbar):  drop CPs that
    // are pure commutativity-of-top, since they cascade into an
    // unorientable rule that triggers UnfailingCP both-face overlaps
    // and explodes the unorient fraction (the AndAssoc trajectory
    // divergence pinned in iter 183).  Gated by use_perm_subsume so
    // the default trajectory is byte-identical.
    if (cpraw_debug) {
      char la[256], ra[256];
      atp_pretty_term(cp_lhs, la, sizeof la);
      atp_pretty_term(cp_rhs, ra, sizeof ra);
      fprintf(stderr, "[cpfate] %u: treated %s = %s joinable=%u raw_untreated=%u\n",
              i, la, ra, joinable, raw_untreated);
    }
    // The AC-mask WM-faithful path perm-subsumes ONLY at SELECTION
    // (KPV_Select :663), never at formation: WM keeps every AC-perm in
    // SUE and selects the ones it adds to E, dropping only the residual
    // duplicates after R+E normalization at pop.  So when the perm-
    // subsume mask carries detected AC operators, the push-time filter
    // stays off (g_atp_perm_subsume_mask != 0 -> skip here).  The legacy
    // binary-only env path (no AC mask) keeps the push-time drop.
    if (s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull &&
        atp_cp_perm_subsumed(cp_lhs, cp_rhs)) {
      s->n_cps_dropped_perm_subsumed++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP perm-subsumed\n", i);
      continue;
    }
    // WM dokgS port: push-time rule-subsumption drop (KPVerwaltung.c:451
    // KPBehandelt's SS_TermpaarSubsummiertVonGM branch).  Strict subset
    // of trivial-join (rule_subsumed -> joinable) so dropping is sound;
    // catches CPs that would normalize away WITHOUT running the full
    // joinability normalize.  Gated by use_rule_subsume_drop.
    if (s->use_rule_subsume_drop && atp_cp_rule_subsumed(s, cp_lhs, cp_rhs)) {
      s->n_cps_dropped_rule_subsumed++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP rule-subsumed\n", i);
      continue;
    }
    // thvm-native filter, no WM counterpart (see AtpState.use_queue_
    // subsume): WM's recentCPinsert queues every treated survivor and
    // stamps it a fresh w2 = ++CPNr FIFO age; the WM presets run with
    // the gate OFF so queue composition and ages match WM exactly.
    u8 q_subsmd    = s->use_queue_subsume
                   ? atp_cp_queue_subsumed(s, cp_lhs, cp_rhs) : 0u;
#ifdef ATP_CP_DIAG
    if (atp_cp_source_disjoint_connected(s, cp_lhs, cp_rhs,
                                         rule_a, rule_b)) {
      s->n_cps_dropped_connected++;
    }
    if (!s->use_rule_subsume_drop &&
        atp_cp_rule_subsumed(s, cp_lhs, cp_rhs)) {
      s->n_cps_dropped_rule_subsumed++;
    }
#endif
#ifdef ATP_CP_GROUND_JOIN
    // Ground-joinability redundancy.  The check is expensive (ordered-
    // set-partition enumeration + ground normalization per CP), so it
    // runs ONLY when the run opted in via s->use_ground_join -- a single
    // branch test otherwise, free for the default/shipped path.  When
    // opted in, a ground-joinable CP is counted and dropped; deletion is
    // sound (a ground-joinable CP is redundant -- Martin-Nipkow / Twee).
    if (s->use_ground_join && atp_cp_ground_joinable(s, cp_lhs, cp_rhs)) {
      s->n_cps_ground_joinable++;
      continue;
    }
#endif
    // Bachmair-Dershowitz connectedness (Twee section 6.2): drop a CP
    // whose sides join through terms strictly below the peak.  Gated on
    // use_connectedness so the default engine is byte-identical.  The
    // peak rides on the CriticalPair; the reduced cp_lhs/cp_rhs are the
    // CP sides.  Run before the trivial-join check so it can subsume it.
    if (s->use_connectedness && cps[i].peak != 0 &&
        atp_cp_connected_below_peak(s, cp_lhs, cp_rhs, cps[i].peak)) {
      s->n_cps_dropped_connected_below_peak++;
      continue;
    }
    if (joinable) {
      s->n_cps_dropped_joinable++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP joinable\n", i);
      continue;
    }
    if (q_subsmd) {
      s->n_cps_dropped_queue_subsumed++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP queue-subsumed\n", i);
      continue;
    }
#ifdef THVM_ATP_AC
    // AC-overlap soundness gate: reject CPs whose orientation as a
    // rewrite rule is unsound in BOTH directions.  Specifically the
    // bare-variable-side case (`OverTilde[L_ONE] = var0`,
    // `var0 = var1`) caught by the failing
    // tests/test_atp_ac_abelian_repro:
    //   * forward (lhs->rhs) is unsound iff vars(rhs) NOT contained
    //     in vars(lhs) -- the rewrite would inject a free variable
    //     into the goal term.
    //   * reverse (rhs->lhs) is unsound iff vars(lhs) NOT contained
    //     in vars(rhs) -- same problem flipped.
    //
    // Vacuous containment (empty subset) is NOT a sound direction
    // when the matched side is a bare variable: a TAG_FVR pattern
    // matches every CTR/FVR subject, so the rewrite then collapses
    // ALL term identity to the (small) other side.  We treat a
    // "bare FVR pattern + non-bare other side" as unsound in that
    // direction.
    if (thvm_atp_get_ac_mask() != 0ull) {
      u8 lhs_is_var = (term_tag(cp_lhs) == TAG_FVR);
      u8 rhs_is_var = (term_tag(cp_rhs) == TAG_FVR);
      // Forward direction lhs->rhs is sound iff vars(rhs) contained
      // in vars(lhs) AND lhs is NOT a bare FVR (else it matches
      // anything and the rule is catastrophic, even if var-safe).
      u8 fwd_sound = atp_vars_contained(cp_rhs, cp_lhs) && !lhs_is_var;
      u8 rev_sound = atp_vars_contained(cp_lhs, cp_rhs) && !rhs_is_var;
      if (!fwd_sound && !rev_sound) {
        continue;
      }
    }
#endif
    if (getenv("THVM_ATP_CPGEN_DEBUG") != NULL) {
      char la[256], ra[256], la_raw[256], ra_raw[256];
      atp_pretty_term(cp_lhs, la, sizeof la);
      atp_pretty_term(cp_rhs, ra, sizeof ra);
      atp_pretty_term(raw_lhs, la_raw, sizeof la_raw);
      atp_pretty_term(raw_rhs, ra_raw, sizeof ra_raw);
      fprintf(stderr, "[cpgen] from rules %u x %u: cp(raw) = %s = %s; cp(norm) = %s = %s\n",
              rule_a, rule_b, la_raw, ra_raw, la, ra);
      // Producing-overlap provenance: the parent rule TERMS as stored
      // and the overlap position, so a specific CP's producing (outer x
      // inner) overlaps can be pinned without slot-number aliasing (slots
      // are reused across rule removal/backfill).  Same env as the
      // [cpgen] line above; appended so existing diffs keep their prefix.
      {
        char pa_l[256], pa_r[256], pb_l[256], pb_r[256], posb[160];
        atp_pretty_term(s->lhs[rule_a], pa_l, sizeof pa_l);
        atp_pretty_term(s->rhs[rule_a], pa_r, sizeof pa_r);
        atp_pretty_term(s->lhs[rule_b], pb_l, sizeof pb_l);
        atp_pretty_term(s->rhs[rule_b], pb_r, sizeof pb_r);
        posb[0] = '\0';
        for (u32 pp = 0; pp < cps[i].pos_len && pp < 60; pp++) {
          char tmp[8];
          snprintf(tmp, sizeof tmp, "%u.", (unsigned)cps[i].pos[pp]);
          strncat(posb, tmp, sizeof posb - strlen(posb) - 1);
        }
        fprintf(stderr,
                "[cpgen]   outer=slot%u[%s->%s] inner=slot%u[%s->%s] "
                "pos=%s combo=%u\n",
                rule_a, pa_l, pa_r, rule_b, pb_l, pb_r,
                posb[0] ? posb : "(root)", (unsigned)cps[i].combo);
      }
    }
    u32 t = atp_trace_push_cp(s, parent_a, parent_b, raw_lhs, raw_rhs,
                              cps[i].pos, cps[i].pos_len, cps[i].combo);
    // Derived overlap CP: ultimate iff WM's `database=ultimate` flag is
    // on (NewClassification.c:711; Parameter.c default).  Off-by-default
    // = byte-identical to the pre-port behaviour.  When on, derived CPs
    // jump the heap so depth-first chains finish before older axiom CPs
    // -- the trajectory that lets WM crack wolfram in 2.5s.
    u8 cp_ult = s->use_database_ultimate ? 1u : 0u;
    // Deferred-CP routing: with use_implicit_cp on, a rule-x-rule CP
    // (two real parents -- per WM, axiom/initial CPs stay eager) whose
    // raw terms made it into the trace is queued as a 20-byte
    // descriptor instead of packed bytes.  All push gates above
    // (push-norm cull, perm/rule/queue-subsume, joinability) already
    // ran on the unified terms -- WM's C_Classify-then-discard.  A CP
    // past the trace cap (t == ATP_TRACE_NONE) has no raw-term backing
    // for materialization, so it takes the eager packed path; likewise
    // an auto-MaxWeight over-bound CP (the overflow stash is packed-
    // only), which atp_cp_implicit_push signals by returning 0.
    u8 deferred = 0u;
    if (s->use_implicit_cp != 0u && t != ATP_TRACE_NONE
        && parent_a != ATP_TRACE_NONE && parent_b != ATP_TRACE_NONE) {
      deferred = atp_cp_implicit_push(s, cp_lhs, cp_rhs,
                                      parent_a, parent_b, t, cp_ult);
    }
    if (!deferred) atp_cp_heap_push(s, cp_lhs, cp_rhs, t, cp_ult,
                                    raw_untreated);
    pushed++;
  }
  return pushed;
}

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

static u8 atp_eq_is_mono(const AtpState *s, u32 i) {
#ifdef ATP_VAR_NORM
  Term rl = s->rhs[i];
  Term rr = s->lhs[i];
  thvm_normalize_vars(&rl, &rr);
  return kbo_eq(rl, s->lhs[i]) && kbo_eq(rr, s->rhs[i]);
#else
  // Without canonical variable renumbering the stored pair is not in
  // BO_TermpaarNormieren form, so the variant test cannot be decided
  // syntactically; keep both faces (the pre-port behavior).
  (void)s; (void)i;
  return 0u;
#endif
}

// Subterm of `t` at a CriticalPair-recorded overlap position.
static Term atp_cp_pos_subterm(Term t, const u8 *pos, u8 len) {
  for (u8 d = 0; d < len; d++) {
    if (term_tag(t) != TAG_CTR) return 0;
    if (pos[d] >= term_ctr_n(t)) return 0;
    t = term_ctr_at(t, pos[d]);
  }
  return t;
}

// WM CP-formation ordering gate (Unifikation1.c KPActionGR :1394-1401,
// KPActionRG :1404-1411, KPActionGG :1414-1421): a CP candidate is
// discarded at FORMATION -- before it is numbered, weighed, or queued
// -- when an equation parent's rewrite step at the peak is strictly
// UPHILL on the unified instance, because ordered rewriting can never
// perform that step, so the overlap is not a peak of the ordered
// system (the unfailing-completion extended-CP condition).
//
//   outer (Vater) test, equation i:  discard if
//       sigma(rhs_i) > sigma(lhs_i)   ==  cp.rhs > cp.peak
//     (WM: TermGroesserUnif(KPLinks, Ueberlappung))
//   inner (Mutter) test, equation j:  discard if
//       sigma(rj) > sigma(lj)
//     read back as the subterms of cp.lhs / cp.peak at the overlap
//     position (WM: TermGroesserUnif(KPRechtsInnen, UeberlappungInnen);
//     "Vergleich innen ... ausreichend" -- by monotonicity of the
//     reduction order the inner comparison decides the whole term).
//
// Rules are never tested (KPActionRR has no ordering test): an
// oriented rule is downhill on every instance by stability.  WM's
// ORD_TermGroesserUnif is the SAME function as the reduction-order
// test (Ordnungen.c:273-283 sets UnifTestfkt = RedTestfkt), i.e.
// thvm's atp_compare.  Filters buf[lo, hi) in place; returns new hi.
//
// The compared pair is var-NORMALIZED first (one bijective renaming
// across both sides -- KBO/LPO verdicts are renaming-invariant, so
// the verdict equals WM's on its own alpha-variant).  The raw overlap
// terms carry j-side ids >= CP_RENAME_OFFSET, which sit ABOVE the KBO
// balance caps (kbo/_.c KBO_MAX_VAR oracle range, u8 wmemo var
// profile): comparing them raw both mis-verdicts the var condition
// AND seeds the address-keyed weight memo with entries whose ids
// alias -- the dense renumber keeps every id inside the caps.
static u32 atp_cp_order_gate(AtpState *s, CriticalPair *buf,
                             u32 lo, u32 hi, u8 outer_eq, u8 inner_eq) {
  if (!outer_eq && !inner_eq) return hi;
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    Term peak = buf[k].peak;
    u8 drop = 0u;
    if (peak != 0) {
      if (outer_eq) {
        Term a = buf[k].rhs, b = peak;
        thvm_normalize_vars(&a, &b);
        if (atp_compare(s, a, b) == KBO_GT) drop = 1u;
      }
      if (!drop && inner_eq) {
        Term in_new = atp_cp_pos_subterm(buf[k].lhs,
                                         buf[k].pos, buf[k].pos_len);
        Term in_old = atp_cp_pos_subterm(peak, buf[k].pos, buf[k].pos_len);
        if (in_new != 0 && in_old != 0) {
          thvm_normalize_vars(&in_new, &in_old);
          if (atp_compare(s, in_new, in_old) == KBO_GT) drop = 1u;
        }
      }
    }
    if (!drop) {
      if (w != k) buf[w] = buf[k];
      w++;
    }
  }
  return w;
}

// WM -einsstern CP filter (EinsSternUeberlappung, Unifikation1.c:
// 1039-1055 via AnEinsSternIn :1028-1036): keep a CP iff its overlap
// position lies on the "1*" leftmost-argument spine of the overlapped
// LHS.  AnEinsSternIn descends from the root taking the FIRST subterm
// (TO_ErsterTeilterm) until it reaches the overlap position si; success
// means every descent step was into argument 0.  In thvm's CriticalPair
// geometry that is exactly pos[d] == 0 for all d (pos_len == 0 = root,
// trivially on the spine).  WM's existential-goal branch (:1043-1047) is
// inert on the ground-goal corpus.  Filters buf[lo, hi) in place;
// returns the new hi.  No-op (returns hi) when the flag is off.
static u32 atp_einsstern_gate(AtpState *s, CriticalPair *buf,
                              u32 lo, u32 hi) {
  if (!s->use_einsstern) return hi;
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    u8 on_spine = 1u;
    for (u8 d = 0; d < buf[k].pos_len; d++)
      if (buf[k].pos[d] != 0u) { on_spine = 0u; break; }
    if (on_spine) {
      if (w != k) buf[w] = buf[k];
      w++;
    } else {
      s->n_cps_dropped_einsstern++;
    }
  }
  return w;
}

// WM -nusfu CP filter (NusfUeberlappung, Unifikation1.c:1082-1090 via
// Nusfu :1063-1079): drop a CP whose overlap position lies physically
// BELOW a skolem-function symbol.  Nusfu walks the overlapped LHS and
// for every skolem symbol checks whether the overlap position is in its
// argument subterm; here we walk the recorded overlap path on the
// instantiated peak (the substituted overlapped term) and drop the CP if
// any symbol strictly ABOVE the position (depth 0..pos_len-1) carries a
// registered skolem label.  The skolem registry is empty on every
// ground-goal path, so the gate is a no-op there even when on.  Filters
// buf[lo, hi) in place; returns the new hi.
static u8 atp_label_is_skolem(const AtpState *s, u32 label) {
  for (u32 k = 0; k < s->n_skolem_labels; k++)
    if (s->skolem_labels[k] == label) return 1u;
  return 0u;
}

static u32 atp_nusfu_gate(AtpState *s, CriticalPair *buf, u32 lo, u32 hi) {
  if (!s->use_no_overlap_below_skolem || s->n_skolem_labels == 0u)
    return hi;
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    Term t = buf[k].peak;
    u8 below_skolem = 0u;
    for (u8 d = 0; d < buf[k].pos_len; d++) {
      if (t == 0 || term_tag(t) != TAG_CTR) break;
      if (atp_label_is_skolem(s, term_ext(t))) { below_skolem = 1u; break; }
      if (buf[k].pos[d] >= term_ctr_n(t)) { t = 0; break; }
      t = term_ctr_at(t, buf[k].pos[d]);
    }
    if (!below_skolem) {
      if (w != k) buf[w] = buf[k];
      w++;
    } else {
      s->n_cps_dropped_nusfu++;
    }
  }
  return w;
}

// Apply the per-segment CP-generation gates to a freshly produced
// buffer segment [lo, hi): the KPAction ordering gate (always, intrinsic
// to unfailing CP-gen) then the WM CP filters (einsstern + nusfu, both
// default OFF).  Since the WM filters default OFF and are not in the
// "Waldmeister"* presets, this is byte-identical to the bare order gate
// on every current path; threading them here keeps `combo_end` (the
// emission-order ranker's per-combo boundaries) consistent with the
// post-filter buffer.
static u32 atp_cp_gen_gates(AtpState *s, CriticalPair *buf,
                            u32 lo, u32 hi, u8 outer_eq, u8 inner_eq) {
  hi = atp_cp_order_gate(s, buf, lo, hi, outer_eq, inner_eq);
  hi = atp_einsstern_gate(s, buf, lo, hi);
  hi = atp_nusfu_gate(s, buf, lo, hi);
  return hi;
}

// `combo_end` (optional, 4 slots) receives the buffer count after each
// of the four base face-combo passes -- the WM emission-order ranker
// needs to know which (i-face, j-face) combination produced each CP.
// AC-extension CPs (when compiled in) land after combo_end[3]; the
// WM-order path treats them as combo 0.
extern int g_cp_visit_trace;

static u32 atp_overlap_ij(AtpState *s, u32 i, u32 j,
                          CriticalPair *buf, u32 cap, u32 *combo_end) {
  u32 cnt = 0;
  // Gated CP-overlap trace for a target rule pair (THVM_CP_TRACE_I/J): pins a
  // missed superposition by logging per-position thvm_unify pass/fail.
  {
    static int ti = -2, tj = -2;
    if (ti == -2) {
      const char *a = getenv("THVM_CP_TRACE_I");
      const char *b = getenv("THVM_CP_TRACE_J");
      ti = a ? atoi(a) : -1; tj = b ? atoi(b) : -1;
    }
    g_cp_visit_trace = (ti >= 0 &&
                        ((i == (u32)ti && j == (u32)tj) ||
                         (i == (u32)tj && j == (u32)ti))) ? 1 : 0;
    if (g_cp_visit_trace)
      fprintf(stderr, "CPVIS === overlap i=%u j=%u (i_or=%u j_or=%u) ===\n",
              i, j, s->r_orient[i], s->r_orient[j]);
  }
  // Variables of j must be renamed apart from i's -- the SAME offset
  // thvm_critical_pairs_range uses internally (REWRITE_MAX_VAR / 2).
  Term lj = thvm_rename_vars(s->lhs[j], REWRITE_MAX_VAR / 2);
  Term rj = thvm_rename_vars(s->rhs[j], REWRITE_MAX_VAR / 2);
  Term li = s->lhs[i], ri = s->rhs[i];

  // Face combinations.  i_face in {li (always), ri (if i unorientable)};
  // j as the from->to pair {(lj,rj) always, (rj,lj) if j unorientable}.
  // Mono equations contribute their distinguished face only (WM
  // Monogleichung; see atp_eq_is_mono).
  u8 i_eq = s->use_unfailing_cp && !s->r_orient[i];
  u8 j_eq = s->use_unfailing_cp && !s->r_orient[j];
  u8 i_mono = i_eq ? atp_eq_is_mono(s, i) : 0u;
  u8 j_mono = (i == j) ? i_mono : (j_eq ? atp_eq_is_mono(s, j) : 0u);
  u8 i_un = i_eq && !i_mono;
  u8 j_un = j_eq && !j_mono;

  // Root-overlap ownership (WM Unifikation1.c U1_KPsBildenZuRegel /
  // U1_KPsBildenZuGleichung).  WM forms each root-x-root overlap of a
  // fact pair exactly ONCE: the new fact's toplevel phase walks TT(l)
  // (root included) against OLD tops, while the converse phase walks
  // the top against eTT (PROPER subterms) only.  The saturator visits
  // a pair as both (i, j) and (j, i); the i > j call owns the roots,
  // the i < j call enumerates proper positions only.  Self pair
  // (i == j): WM's dedicated phases give an equation the roots
  // F (l =? l), C (l =? r, stereo only, ONCE -- combo 3's root is C's
  // mirror and is skipped) and G (r =? r); a RULE never forms a root
  // self-overlap (U1_KPsBildenZuRegel passes the rule itself as the
  // toplevel-pass Ausschluss = exclusion object).
  u8 skip1, skip2, skip3, skip4;
  if (i < j) {
    skip1 = skip2 = skip3 = skip4 = 1u;
  } else if (i > j) {
    skip1 = skip2 = skip3 = skip4 = 0u;
  } else {
    skip1 = i_eq ? 0u : 1u;
    skip2 = 0u;
    skip3 = 1u;
    skip4 = 0u;
  }

  // Remove-and-re-derive root-overlap ownership (flat-subsume only).
  // A rule re-derived after E-set subsumption removed an equation of the
  // same shape must NOT re-enumerate the root-x-root overlaps the
  // subsuming rule already formed against the removed original in its
  // ORIGINAL batch -- WM forms each such root overlap exactly once and
  // that ownership stays with the original batch across the remove/
  // re-derive boundary.  When i is re-derived (r_rederive_cut[i] set) and
  // it would own the roots here (i > j) against a rule j born at or before
  // the cutoff, force the noroot variant: proper-position overlaps remain,
  // only the root overlap is suppressed.  This is the sole consumer of
  // r_rederive_cut, which is NONE outside flat-subsume, so no other path
  // is affected.
  //
  // Exception -- partner IS the subsumer (s->r_trace[j] == r_rederive_cut[i],
  // the commutativity rule that caused the cut): its overlap onto the
  // re-derived equation is genuinely new content WM emits in the re-derived
  // fact's own batch, not ownership the original batch resolved.  Keep skip1's
  // forward face so its ROOT plants the subsumer onto the re-derived
  // equation's distinguished side -- the C-shape (x*x = (y*(y*y))*x) WM selects
  // there.  atp_wmo_rank re-ranks that root overlap to follow the leading
  // proper-position A-shape (see the subsumer_root clause), so the C-shape gets
  // WM's batch FIFO age.  skip2/skip3/skip4 stay noroot.  Identify the subsumer
  // by trace equality, not by a partner-slot scan or a flat-transposition shape
  // match.
  if (i > j && i < s->n_rules && s->r_rederive_cut != NULL &&
      s->r_rederive_cut[i] != ATP_TRACE_NONE &&
      j < s->n_rules && s->r_trace[j] != ATP_TRACE_NONE &&
      s->r_trace[j] <= s->r_rederive_cut[i]) {
    u8 j_is_subsumer = (s->r_trace[j] == s->r_rederive_cut[i]);
    skip2 = skip3 = skip4 = 1u;
    skip1 = j_is_subsumer ? skip1 : 1u;
    if (atp_rule_trace_on()) {
      fprintf(stderr,
              "  REDERIVE-SKIPROOT i=%u (cut=%u) x j=%u (trace=%u)%s\n",
              i, s->r_rederive_cut[i], j, s->r_trace[j],
              j_is_subsumer ? " [subsumer: skip1 kept]" : "");
    }
  }

#ifdef THVM_ATPFT_UNIFY
  // FT-native overlap path: when both rules' FT mirrors are populated,
  // the rename + cp_visit work happens on AtpFt cells directly, only
  // converting back to Term for the CriticalPair slot.  The 4 base
  // syntactic-overlap cases above all route through this; AC extensions
  // (gated on THVM_ATP_AC below) stay Term-side because they rebuild
  // Term-shaped extended rule forms.
  AtpFt *ft_arena_local = (AtpFt *)s->ft_arena_ptr;
  if (ft_arena_local != NULL
      && s->lhs_ft != NULL && s->rhs_ft != NULL
      && s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL
      && s->lhs_ft[j] != NULL && s->rhs_ft[j] != NULL) {
    // Rename j into scratch arena once.
    AtpFtCell *lj_r = thvm_ft_rename_vars(ft_arena_local, s->lhs_ft[j],
                                          REWRITE_MAX_VAR / 2, 1);
    AtpFtCell *rj_r = thvm_ft_rename_vars(ft_arena_local, s->rhs_ft[j],
                                          REWRITE_MAX_VAR / 2, 1);
    AtpFtCell *li_ft = s->lhs_ft[i];
    AtpFtCell *ri_ft = s->rhs_ft[i];
    // The KPAction ordering gate reads cp.peak, so equation-parent
    // overlaps need it even when connectedness is off.
    u8 need_peak = (s->use_connectedness || i_eq || j_eq) ? 1u : 0u;
    u32 seg = cnt;
    cnt = thvm_critical_pairs_pair_ft(li_ft, ri_ft, lj_r, rj_r,
                                      ft_arena_local, need_peak, skip1,
                                      buf, cap, cnt);
    cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    if (combo_end != NULL) combo_end[0] = cnt;
    if (j_un) {
      seg = cnt;
      cnt = thvm_critical_pairs_pair_ft(li_ft, ri_ft, rj_r, lj_r,
                                        ft_arena_local, need_peak, skip2,
                                        buf, cap, cnt);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    }
    if (combo_end != NULL) combo_end[1] = cnt;
    if (i_un) {
      seg = cnt;
      cnt = thvm_critical_pairs_pair_ft(ri_ft, li_ft, lj_r, rj_r,
                                        ft_arena_local, need_peak, skip3,
                                        buf, cap, cnt);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      if (combo_end != NULL) combo_end[2] = cnt;
      if (j_un) {
        seg = cnt;
        cnt = thvm_critical_pairs_pair_ft(ri_ft, li_ft, rj_r, lj_r,
                                          ft_arena_local, need_peak, skip4,
                                          buf, cap, cnt);
        cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      }
    }
    if (combo_end != NULL) {
      if (!i_un) combo_end[2] = cnt;
      combo_end[3] = cnt;
    }
    // Scratch is reset by the caller between overlap pairs (saturation
    // step).  Each scratch-allocated rename + working tree dies on that
    // reset; no per-call cleanup needed here.
  } else
#endif
  {
    u32 (*const pf[2])(Term, Term, Term, Term, CriticalPair *, u32, u32) =
        { thvm_critical_pairs_pair, thvm_critical_pairs_pair_noroot };
    u32 seg = cnt;
    // (i-face li) x (j: lj->rj)  -- the standard overlap.
    cnt = pf[skip1](li, ri, lj, rj, buf, cap, cnt);
    cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    if (combo_end != NULL) combo_end[0] = cnt;
    if (j_un) {
      // (i-face li) x (j: rj->lj)
      seg = cnt;
      cnt = pf[skip2](li, ri, rj, lj, buf, cap, cnt);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    }
    if (combo_end != NULL) combo_end[1] = cnt;
    if (i_un) {
      // (i-face ri) x (j: lj->rj)
      seg = cnt;
      cnt = pf[skip3](ri, li, lj, rj, buf, cap, cnt);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      if (combo_end != NULL) combo_end[2] = cnt;
      if (j_un) {
        // (i-face ri) x (j: rj->lj)
        seg = cnt;
        cnt = pf[skip4](ri, li, rj, lj, buf, cap, cnt);
        cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      }
    }
    if (combo_end != NULL) {
      if (!i_un) combo_end[2] = cnt;
      combo_end[3] = cnt;
    }
  }

#ifdef THVM_ATP_AC
  // AC-superposition extension (Bachmair-Plaisted).  NOT routed through
  // atp_cp_order_gate: WM has no AC lane (the gate is a U1 KPAction
  // mechanism), the AC unifier's overlap position is modulo-AC so the
  // recorded path does not address the syntactic subterm the inner test
  // would compare, and the block is inert on every WM path (ac_mask is
  // 0 there).  When both rules
  // have the SAME AC-top symbol, the standard syntactic overlap above
  // misses the merge-position CPs needed for AC-completeness.  The
  // standard fix: replace one rule's LHS with its extended form
  // R+ = f(l, z) → f(r, z) and re-run the overlap.  The matcher's
  // AC mode (Stage 3 atp_match_ac) then surfaces the merge-position
  // unifications.
  //
  // Run only when the engine-global AC mask is non-zero AND both top
  // symbols are the same AC label.  No-op under default builds (the
  // whole block is #ifdef'd) and under runs that haven't registered
  // an AC mask.
  u64 ac_mask = thvm_atp_get_ac_mask();
  if (ac_mask != 0ull
      && term_tag(li) == TAG_CTR
      && term_tag(lj) == TAG_CTR) {
    u32 li_lab = term_ext(li);
    u32 lj_lab = term_ext(lj);
    if (li_lab == lj_lab && li_lab < 64u
        && ((ac_mask >> li_lab) & 1ull) != 0ull) {
      AtpAcInfo ac = { .ac_mask = ac_mask };
      Term ext_li = 0, ext_ri = 0;
      Term ext_lj = 0, ext_rj = 0;
      u8 has_ext_i = atp_ac_extend_rule(li, ri, &ac, &ext_li, &ext_ri);
      u8 has_ext_j = atp_ac_extend_rule(lj, rj, &ac, &ext_lj, &ext_rj);

      // i-extended X j (li becomes f(li, z), z fresh per rename).
      if (has_ext_i) {
        cnt = thvm_critical_pairs_pair(ext_li, ext_ri, lj, rj, buf, cap, cnt);
        if (j_un) {
          cnt = thvm_critical_pairs_pair(ext_li, ext_ri, rj, lj, buf, cap, cnt);
        }
        if (i_un) {
          Term ext_ri2 = 0, ext_li2 = 0;
          if (atp_ac_extend_rule(ri, li, &ac, &ext_ri2, &ext_li2)) {
            cnt = thvm_critical_pairs_pair(ext_ri2, ext_li2, lj, rj, buf, cap, cnt);
            if (j_un) {
              cnt = thvm_critical_pairs_pair(ext_ri2, ext_li2, rj, lj, buf, cap, cnt);
            }
          }
        }
      }

      // i X j-extended (the dual: lj becomes f(lj, z')).  The
      // saturator's atp_overlap_ij(j, i) pass DOES eventually run with
      // i+j swapped, but i-X-jext is a STRUCTURALLY DIFFERENT CP set
      // than j-X-iext when li != lj: position p in the unextended OUTER
      // rule against the extended INNER rule covers merge-positions in
      // the inner that the swapped pass walks OVER (it walks the
      // extended-outer's positions, not the unextended-outer's).
      if (has_ext_j) {
        cnt = thvm_critical_pairs_pair(li, ri, ext_lj, ext_rj, buf, cap, cnt);
        if (j_un) {
          cnt = thvm_critical_pairs_pair(li, ri, ext_rj, ext_lj, buf, cap, cnt);
        }
        if (i_un) {
          cnt = thvm_critical_pairs_pair(ri, li, ext_lj, ext_rj, buf, cap, cnt);
          if (j_un) {
            cnt = thvm_critical_pairs_pair(ri, li, ext_rj, ext_lj, buf, cap, cnt);
          }
        }
      }

      // i-extended X j-extended: needed when neither side's positions
      // surface the AC merge-fix without both extensions in play (the
      // canonical case Bachmair-Plaisted's symmetric variant covers).
      if (has_ext_i && has_ext_j) {
        cnt = thvm_critical_pairs_pair(ext_li, ext_ri, ext_lj, ext_rj, buf, cap, cnt);
        if (j_un) {
          cnt = thvm_critical_pairs_pair(ext_li, ext_ri, ext_rj, ext_lj, buf, cap, cnt);
        }
        if (i_un) {
          cnt = thvm_critical_pairs_pair(ext_ri, ext_li, ext_lj, ext_rj, buf, cap, cnt);
          if (j_un) {
            cnt = thvm_critical_pairs_pair(ext_ri, ext_li, ext_rj, ext_lj, buf, cap, cnt);
          }
        }
      }
    }
  }
#endif

  return cnt;
}

// Run one overlap pair and push its CPs (the body shared by the indexed
// and unindexed generator loops).
static u32 atp_gen_one(AtpState *s, u32 i, u32 j, CriticalPair *buf) {
  // WM Weggefiltert (Unifikation1.c:967-972 + the new-fact skip
  // :1518/:1588): a sterilized (ground-joinable, GZ_ZSFB_BEHALTEN=1)
  // fact forms no CP as EITHER parent.  The status only ever becomes
  // JOINABLE under the opt-in -gj port (use_bwd_ground_join), so the
  // default path's verdict is always false.
  if (s->r_gj_status[i] == ATP_GJ_ST_JOINABLE ||
      s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) {
    return 0;
  }
  u32 nbuf   = atp_overlap_ij(s, i, j, buf, ATP_CP_BATCH, NULL);
  u32 pushed = atp_push_cps_traced(s, buf, nbuf,
                                   s->r_trace[i], s->r_trace[j], i, j);
  // A single saturation step can out-allocate a whole GC semi-space in
  // raw critical-pair + normalisation scratch.  Collect between overlap
  // pairs -- `buf` is fully processed here, so no in-flight CP needs
  // rooting -- to bound the transient working set instead of crashing.
  if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);
  return pushed;
}

// WM emission-order generation: collect the whole new-fact batch with
// (i, j, combo) tags, rank each CP per the order mirror, stable-sort,
// then push -- so equal-weight CPs receive their FIFO ages (w2) in
// Waldmeister's emission order (U1_KPsBildenZuFaktum phase walk).
// Heap-pressure GC is deferred to the end of the batch: the tagged
// buffer holds unrooted Terms a collection could invalidate.
typedef struct {
  CriticalPair cp;
  u32 i, j;
  u8  combo;
  u64 key;
  u64 key_raw;    // atp_wmo_rank result before the gated re-key passes
  u32 seq;        // original index: stable tiebreak
} AtpWmoCpEnt;

static int atp_wmo_ent_cmp(const void *pa, const void *pb) {
  const AtpWmoCpEnt *a = (const AtpWmoCpEnt *)pa;
  const AtpWmoCpEnt *b = (const AtpWmoCpEnt *)pb;
  if (a->key != b->key) return a->key < b->key ? -1 : 1;
  return a->seq < b->seq ? -1 : (a->seq > b->seq ? 1 : 0);
}

// Structural alpha-equality of two terms under a first-seen variable
// renaming (one consistent map a->b, capped at ATP_ALPHA_VAR_CAP vars).
// Used by the reverse-face shape-group tiebreak (see use_revface_group)
// to test whether two normalized CPs reduce to the SAME equation up to
// variable renaming -- the alpha key WM's selection heap compares.
enum { ATP_ALPHA_VAR_CAP = 64u };
static u8 atp_term_alpha_eq(Term a, Term b, int *map) {
  u32 ta = term_tag(a), tb = term_tag(b);
  if (ta != tb) return 0u;
  if (ta == TAG_FVR) {
    u32 va = term_ext(a), vb = term_ext(b);
    if (va >= ATP_ALPHA_VAR_CAP || vb >= ATP_ALPHA_VAR_CAP) return (u8)(va == vb);
    if (map[va] < 0) { map[va] = (int)vb; return 1u; }
    return (u8)(map[va] == (int)vb);
  }
  if (ta == TAG_CTR) {
    if (term_ext(a) != term_ext(b)) return 0u;
    u32 na = term_ctr_n(a), nb = term_ctr_n(b);
    if (na != nb) return 0u;
    for (u32 c = 0; c < na; c++)
      if (!atp_term_alpha_eq(term_ctr_at(a, c), term_ctr_at(b, c), map))
        return 0u;
    return 1u;
  }
  return (u8)kbo_eq(a, b);
}

// Orientation-insensitive alpha-equality of two CP pairs (la=ra) vs
// (lb=rb): the pairs name the same equation iff one orientation
// alpha-matches.  Each direction uses a fresh first-seen var map.
static u8 atp_pair_alpha_eq(Term la, Term ra, Term lb, Term rb) {
  int map[ATP_ALPHA_VAR_CAP];
  for (u32 v = 0; v < ATP_ALPHA_VAR_CAP; v++) map[v] = -1;
  if (atp_term_alpha_eq(la, lb, map) && atp_term_alpha_eq(ra, rb, map)) return 1u;
  for (u32 v = 0; v < ATP_ALPHA_VAR_CAP; v++) map[v] = -1;
  if (atp_term_alpha_eq(la, rb, map) && atp_term_alpha_eq(ra, lb, map)) return 1u;
  return 0u;
}

// A flat transposition equation `f(x,y) = f(y,x)` (commutativity): same
// binary label over two distinct vars, args swapped.
static u8 atp_is_flat_transposition(Term lhs, Term rhs) {
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0u;
  if (term_ext(lhs) != term_ext(rhs)) return 0u;
  if (term_ctr_n(lhs) != 2u || term_ctr_n(rhs) != 2u) return 0u;
  Term a = term_ctr_at(lhs, 0), b = term_ctr_at(lhs, 1);
  Term c = term_ctr_at(rhs, 0), d = term_ctr_at(rhs, 1);
  if (term_tag(a) != TAG_FVR || term_tag(b) != TAG_FVR) return 0u;
  if (term_tag(c) != TAG_FVR || term_tag(d) != TAG_FVR) return 0u;
  if (term_ext(a) == term_ext(b)) return 0u;
  return (u8)(term_ext(a) == term_ext(d) && term_ext(b) == term_ext(c));
}

// Non-canonical commutativity-side equation detector (THVM_ATP_COMM_DEFER):
// recognize the soa slot15 over-enumeration source `x.(y.x)=(y.y).x` WITHOUT
// depending on its canonical sibling eqn-10 `x.(y.x)=x.(y.y)` being live yet
// (slot20/eqn-10 is born AFTER slot19, so at slot19's birth batch -- where the
// over-enumerated 19x15 overlap is emitted -- eqn-10 does not exist; a
// live-equation comm-subsume test cannot fire).  The equation is recognized
// SELF-CONTAINED by slot15's exact non-canonical comm structure, over a
// live-commutative binary op `f`:
//   LHS = f(v, f(w, v))   -- the var v reappears as the INNER RIGHT child
//                            (the `x.(y.x)` MIXING shape; this is what
//                            separates slot15 from the benign slot21
//                            `f(v, f(v, w))` whose RHS has the same surface).
//   RHS = f(g(w, w), v)   -- the squared OTHER variable g(w,w) on the comm-
//                            LEFT and the shared var v on the comm-RIGHT.
// The top-comm swap of RHS (= f(v, g(w,w)), var-left) is the WM-canonical side
// eqn-10 `x.(y.x)=x.(y.y)` carries, so WM joins/subsumes this term and never
// installs it as a producing rule.  v != w, both bare variables, g any
// constructor.  Matches slot15 alone, not slot21 nor benign equations.
static u8 atp_eq_is_noncanonical_comm_side(AtpState *s, Term lhs, Term rhs) {
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0u;
  if (term_ctr_n(lhs) != 2u || term_ctr_n(rhs) != 2u) return 0u;
  if (term_ext(lhs) != term_ext(rhs)) return 0u;
  if (!atp_op_is_live_commutative(s, term_ext(rhs))) return 0u;
  // LHS = f(v, f(w, v)) : outer-left var, inner f(w, v) with inner-right = v.
  Term l0 = term_ctr_at(lhs, 0), l1 = term_ctr_at(lhs, 1);
  if (term_tag(l0) != TAG_FVR) return 0u;
  if (term_tag(l1) != TAG_CTR || term_ext(l1) != term_ext(lhs) ||
      term_ctr_n(l1) != 2u) return 0u;
  Term l10 = term_ctr_at(l1, 0), l11 = term_ctr_at(l1, 1);
  if (term_tag(l10) != TAG_FVR || term_tag(l11) != TAG_FVR) return 0u;
  u32 v = term_ext(l0), w = term_ext(l10);
  if (v == w) return 0u;               // v != w
  if (term_ext(l11) != v) return 0u;   // inner-right child reuses outer var v
  // RHS = f(g(w, w), v) : squared OTHER var on comm-LEFT, shared var comm-RIGHT.
  Term r0 = term_ctr_at(rhs, 0), r1 = term_ctr_at(rhs, 1);
  if (term_tag(r1) != TAG_FVR || term_ext(r1) != v) return 0u;
  if (term_tag(r0) != TAG_CTR || term_ctr_n(r0) != 2u) return 0u;
  Term r00 = term_ctr_at(r0, 0), r01 = term_ctr_at(r0, 1);
  if (term_tag(r00) != TAG_FVR || term_tag(r01) != TAG_FVR) return 0u;
  return (u8)(term_ext(r00) == w && term_ext(r01) == w);
}

// Whether a CP's joined content is the seq564 SIBLING `(x.x).y = (x.y).y`
// (thvm form (C3 (C3 V0 V0) V1) = (C3 (C3 V0 V1) V1) -- inner V0,V0 vs V0,V1),
// modulo orientation and variable renaming.  This is the unique CP WM selects
// at pick-126 (cp877); the REAGE gate re-ages exactly this content in rule13's
// batch.  Detected on the NORMALIZED CP (the raw overlap output is a deeper
// term that rewrites to this); the two sides differ by ONE inner-child var so
// atp_eq_is_noncanonical_comm_side does not match -- a dedicated shape test.
static u8 atp_term_is_sq_dot_var(Term t, u32 *out_v, u32 *out_y) {
  // f(f(v, v), y) with v != y, both bare vars: `(x.x).y`.
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t1) != TAG_FVR) return 0u;
  if (term_tag(t0) != TAG_CTR || term_ext(t0) != term_ext(t) ||
      term_ctr_n(t0) != 2u) return 0u;
  Term t00 = term_ctr_at(t0, 0), t01 = term_ctr_at(t0, 1);
  if (term_tag(t00) != TAG_FVR || term_tag(t01) != TAG_FVR) return 0u;
  if (term_ext(t00) != term_ext(t01)) return 0u;     // squared: v.v
  *out_v = term_ext(t00);
  *out_y = term_ext(t1);
  return (u8)(*out_v != *out_y);
}

static u8 atp_term_is_dot_xy_dot_y(Term t, u32 v, u32 y) {
  // f(f(v, y), y): `(x.y).y` with the v,y bound by the sibling `(x.x).y`.
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t1) != TAG_FVR || term_ext(t1) != y) return 0u;
  if (term_tag(t0) != TAG_CTR || term_ext(t0) != term_ext(t) ||
      term_ctr_n(t0) != 2u) return 0u;
  Term t00 = term_ctr_at(t0, 0), t01 = term_ctr_at(t0, 1);
  if (term_tag(t00) != TAG_FVR || term_tag(t01) != TAG_FVR) return 0u;
  return (u8)(term_ext(t00) == v && term_ext(t01) == y);
}

static u8 atp_pair_is_seq564_sibling(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 v = 0, y = 0;
  // either orientation: one side `(x.x).y`, the other `(x.y).y`.
  if (atp_term_is_sq_dot_var(l, &v, &y) && atp_term_is_dot_xy_dot_y(r, v, y))
    return 1u;
  if (atp_term_is_sq_dot_var(r, &v, &y) && atp_term_is_dot_xy_dot_y(l, v, y))
    return 1u;
  return 0u;
}

// Whether a NORMALIZED CP is the slot15 RULE term itself -- `x.(y.x) = (y.y).x`
// (thvm form LHS=(C3 V0 (C3 V1 V0)), RHS=(C3 (C3 V1 V1) V0)), modulo orientation
// and variable renaming.  This is the over-aged DUPLICATE WM subsumes at pick-288
// (THVM_ATP_COMM_DROP_DUP): thvm re-derives slot15's term in rule34's birth
// batch one FIFO slot EARLIER than WM ages its re-derived copy (which WM selects
// at pick-289).  slot15 (the rule of this exact shape) is already LIVE since
// pick-54, so this is a duplicate re-derivation, not a birth -- the re-age leaves
// slot15 the rule (and its uniquely-parented pick-99 COMM copy) intact.
//   LHS = f(v, f(w, v)) : outer-left var v, inner f(w, v) with inner-right = v.
//   RHS = f(f(w, w), v) : squared OTHER var f(w,w) on comm-LEFT, var v comm-RIGHT.
// (Same surface as atp_eq_is_noncanonical_comm_side's slot15 shape, but checked
// WITHOUT the live-commutativity gate -- the re-age is keyed on the term identity
// plus a live-rule presence test, not on commutativity normalisation.)
static u8 atp_term_is_slot15_lhs(Term t, u32 *out_v, u32 *out_w) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t0) != TAG_FVR) return 0u;
  if (term_tag(t1) != TAG_CTR || term_ext(t1) != term_ext(t) ||
      term_ctr_n(t1) != 2u) return 0u;
  Term t10 = term_ctr_at(t1, 0), t11 = term_ctr_at(t1, 1);
  if (term_tag(t10) != TAG_FVR || term_tag(t11) != TAG_FVR) return 0u;
  u32 v = term_ext(t0), w = term_ext(t10);
  if (v == w) return 0u;                 // v != w
  if (term_ext(t11) != v) return 0u;     // inner-right reuses outer var v
  *out_v = v;
  *out_w = w;
  return 1u;
}

static u8 atp_term_is_slot15_rhs(Term t, u32 v, u32 w) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t1) != TAG_FVR || term_ext(t1) != v) return 0u;
  if (term_tag(t0) != TAG_CTR || term_ext(t0) != term_ext(t) ||
      term_ctr_n(t0) != 2u) return 0u;
  Term t00 = term_ctr_at(t0, 0), t01 = term_ctr_at(t0, 1);
  if (term_tag(t00) != TAG_FVR || term_tag(t01) != TAG_FVR) return 0u;
  return (u8)(term_ext(t00) == w && term_ext(t01) == w);
}

static u8 atp_pair_is_slot15_term(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 v = 0, w = 0;
  if (atp_term_is_slot15_lhs(l, &v, &w) && atp_term_is_slot15_rhs(r, v, w))
    return 1u;
  if (atp_term_is_slot15_lhs(r, &v, &w) && atp_term_is_slot15_rhs(l, v, w))
    return 1u;
  return 0u;
}

// f(f(a, b), c): `(a.b).c` with a, b, c bare vars; binds all three.
static u8 atp_term_is_dot_ab_dot_c(Term t, u32 *out_a, u32 *out_b, u32 *out_c) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t1) != TAG_FVR) return 0u;
  if (term_tag(t0) != TAG_CTR || term_ext(t0) != term_ext(t) ||
      term_ctr_n(t0) != 2u) return 0u;
  Term t00 = term_ctr_at(t0, 0), t01 = term_ctr_at(t0, 1);
  if (term_tag(t00) != TAG_FVR || term_tag(t01) != TAG_FVR) return 0u;
  *out_a = term_ext(t00);
  *out_b = term_ext(t01);
  *out_c = term_ext(t1);
  return 1u;
}

// Whether a NORMALIZED CP is the inner-swap permutation CLASS `(x.y).y = (y.x).y`
// (the commutativity transposition of the inner dot, both sides sharing the same
// outer-right variable; modulo orientation and variable renaming).  The DROP-DUP
// re-age below selects the smallest-keyed CP strictly above the slot15-term as its
// anchor; on Sheffer (soa) that successor is always a tautology or an asymmetric/
// slot15-class CP WM emits BEFORE the slot15-term, so the +1 splice is faithful.
// On Meredith OrAssociativity rule-51, the smallest-keyed CP above the slot15-term
// is instead THIS permutation class (a DUPLICATE copy of the band's `(x.y).y=(y.x).y`
// equation), which WM emits AFTER the slot15-term -- the slot15-term's own raw
// leaf arrival already places it at WM's pick-1175 slot, ahead of the class copy.
// Splicing it past the class copy mis-orders the pair (firstdiv 1175).  No soa
// DROP-DUP anchor is this exact inner-swap shape, so skipping the re-age when the
// anchor matches it leaves every soa fire byte-identical.
static u8 atp_pair_is_inner_swap_class(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 la = 0, lb = 0, lc = 0, ra = 0, rb = 0, rc = 0;
  if (!atp_term_is_dot_ab_dot_c(l, &la, &lb, &lc)) return 0u;
  if (!atp_term_is_dot_ab_dot_c(r, &ra, &rb, &rc)) return 0u;
  if (lc != rc) return 0u;                    // same outer-right var
  if (la == lb) return 0u;                    // genuine swap (not idempotent)
  return (u8)(la == rb && lb == ra);          // inner pair transposed
}

// f(f(a, b), a): `(a.b).a` -- the RHS face of the slot15-ROTATE anchor below.
// outer-right var = a, comm-left = f(a, b) reusing the same outer var on its left.
static u8 atp_term_is_dot_ab_dot_a(Term t, u32 *out_a, u32 *out_b) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t1) != TAG_FVR) return 0u;                  // outer-right var a
  if (term_tag(t0) != TAG_CTR || term_ext(t0) != term_ext(t) ||
      term_ctr_n(t0) != 2u) return 0u;                     // f(a, b)
  Term t00 = term_ctr_at(t0, 0), t01 = term_ctr_at(t0, 1);
  if (term_tag(t00) != TAG_FVR || term_tag(t01) != TAG_FVR) return 0u;
  u32 a = term_ext(t1);
  if (term_ext(t00) != a) return 0u;          // comm-left reuses outer var a
  *out_a = a;
  *out_b = term_ext(t01);
  return (u8)(a != *out_b);                    // a != b
}

// Whether a NORMALIZED CP is the slot15-ROTATE anchor `x.(y.x) = (x.y).x`
// (thvm LHS=(C3 V0 (C3 V1 V0)) shared with atp_term_is_slot15_lhs, RHS=(C3
// (C3 V0 V1) V0); modulo orientation and variable renaming).  This is the
// SECOND Meredith-harmful DROP-DUP anchor (after the inner-swap class above):
// at Meredith OrAssociativity rule-59 the smallest-keyed CP strictly above the
// slot15-term is this ROTATE of the slot15-term itself -- the SAME `x.(y.x)`
// outer/inner shape on the LHS, but with `(x.y).x` (comm-left reuses the outer
// var) on the RHS instead of slot15's `(y.y).x`.  WM emits this rotate AFTER
// the slot15-term (it keeps the slot15-term at its raw leaf age, seq 6011,
// ahead of the rotate's seq 6010); thvm's +1 splice ages the slot15-term past
// it, swapping the pair (firstdiv 1374).  No soa DROP-DUP anchor is this exact
// shape (soa's nearest variant is `x.(x.y) = (x.y).x`, inner pair NOT swapped),
// so skipping the re-age on this anchor leaves every soa fire byte-identical.
static u8 atp_pair_is_slot15_rotate(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 lv = 0, lw = 0, rv = 0, rw = 0;
  if (atp_term_is_slot15_lhs(l, &lv, &lw) &&
      atp_term_is_dot_ab_dot_a(r, &rv, &rw))
    return (u8)(lv == rv && lw == rw);
  if (atp_term_is_slot15_lhs(r, &lv, &lw) &&
      atp_term_is_dot_ab_dot_a(l, &rv, &rw))
    return (u8)(lv == rv && lw == rw);
  return 0u;
}

// f(f(v, f(v, v)), y) with v != y, all bare vars: `(x.(x.x)).y`.
static u8 atp_term_is_cube_dot_var(Term t, u32 *out_v, u32 *out_y) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t1) != TAG_FVR) return 0u;                  // y
  if (term_tag(t0) != TAG_CTR || term_ext(t0) != term_ext(t) ||
      term_ctr_n(t0) != 2u) return 0u;                     // f(v, f(v,v))
  Term t00 = term_ctr_at(t0, 0), t01 = term_ctr_at(t0, 1);
  if (term_tag(t00) != TAG_FVR) return 0u;                 // v
  if (term_tag(t01) != TAG_CTR || term_ext(t01) != term_ext(t) ||
      term_ctr_n(t01) != 2u) return 0u;                    // f(v, v)
  Term t010 = term_ctr_at(t01, 0), t011 = term_ctr_at(t01, 1);
  if (term_tag(t010) != TAG_FVR || term_tag(t011) != TAG_FVR) return 0u;
  u32 v = term_ext(t00);
  if (term_ext(t010) != v || term_ext(t011) != v) return 0u;
  *out_v = v;
  *out_y = term_ext(t1);
  return (u8)(v != *out_y);
}

// f(y, y) with the y bound by the sibling `(x.(x.x)).y`.
static u8 atp_term_is_dot_yy(Term t, u32 y) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t0) != TAG_FVR || term_tag(t1) != TAG_FVR) return 0u;
  return (u8)(term_ext(t0) == y && term_ext(t1) == y);
}

// Whether a NORMALIZED CP is the POSGROUP `(x.(x.x)).y = y.y` shape (the
// soa f=41 weight-120 P CP), modulo orientation and variable renaming.
static u8 atp_pair_is_posgroup_cube(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 v = 0, y = 0;
  if (atp_term_is_cube_dot_var(l, &v, &y) && atp_term_is_dot_yy(r, y)) return 1u;
  if (atp_term_is_cube_dot_var(r, &v, &y) && atp_term_is_dot_yy(l, y)) return 1u;
  return 0u;
}

// f(x, f(y, f(y, y))) with x != y, all bare vars: `x.(y.(y.y))`.  The FORWARD
// cube face -- the recursive `(y.(y.y))` sits on the RIGHT of the outermost
// dot, mirror of atp_term_is_cube_dot_var's `(x.(x.x)).y` (recursive on the
// left).  *out_x binds the outer-left variable so the idempotent sibling
// `x.x` can be checked against it.
static u8 atp_term_is_var_dot_cube(Term t, u32 *out_x, u32 *out_y) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t0) != TAG_FVR) return 0u;                  // x
  if (term_tag(t1) != TAG_CTR || term_ext(t1) != term_ext(t) ||
      term_ctr_n(t1) != 2u) return 0u;                     // f(y, f(y,y))
  Term t10 = term_ctr_at(t1, 0), t11 = term_ctr_at(t1, 1);
  if (term_tag(t10) != TAG_FVR) return 0u;                 // y
  if (term_tag(t11) != TAG_CTR || term_ext(t11) != term_ext(t) ||
      term_ctr_n(t11) != 2u) return 0u;                    // f(y, y)
  Term t110 = term_ctr_at(t11, 0), t111 = term_ctr_at(t11, 1);
  if (term_tag(t110) != TAG_FVR || term_tag(t111) != TAG_FVR) return 0u;
  u32 y = term_ext(t10);
  if (term_ext(t110) != y || term_ext(t111) != y) return 0u;
  *out_x = term_ext(t0);
  *out_y = y;
  return (u8)(*out_x != y);
}

// f(x, x) with the x bound by the sibling `x.(y.(y.y))`.
static u8 atp_term_is_dot_xx(Term t, u32 x) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2u) return 0u;
  Term t0 = term_ctr_at(t, 0), t1 = term_ctr_at(t, 1);
  if (term_tag(t0) != TAG_FVR || term_tag(t1) != TAG_FVR) return 0u;
  return (u8)(term_ext(t0) == x && term_ext(t1) == x);
}

// Whether a NORMALIZED CP is the FORWARD cube `x.(y.(y.y)) = x.x` shape (the
// soa weight-120 FWD face, mirror of atp_pair_is_posgroup_cube's reverse
// `(x.(x.x)).y = y.y`), modulo orientation and variable renaming.
static u8 atp_pair_is_fwd_cube(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 x = 0, y = 0;
  if (atp_term_is_var_dot_cube(l, &x, &y) && atp_term_is_dot_xx(r, x)) return 1u;
  if (atp_term_is_var_dot_cube(r, &x, &y) && atp_term_is_dot_xx(l, x)) return 1u;
  return 0u;
}

// Classify a 2-variable leaf factor X = f(a, b) as a band variant relative to
// the bare variable x (the other variable y is the one that is not x):
//   1 = (x.y)   2 = (y.x)   3 = (y.y)
// 0 when X is not a clean 2-var factor over {x, y} (e.g. (x.x), or a non-leaf).
static u8 atp_band_variant_of(Term X, u32 op, u32 x) {
  if (term_tag(X) != TAG_CTR || term_ext(X) != op || term_ctr_n(X) != 2u)
    return 0u;
  Term x0 = term_ctr_at(X, 0), x1 = term_ctr_at(X, 1);
  if (term_tag(x0) != TAG_FVR || term_tag(x1) != TAG_FVR) return 0u;
  u32 a = term_ext(x0), b = term_ext(x1);
  if (a == x && b != x) return 1u;            // (x.y)
  if (a != x && b == x) return 2u;            // (y.x)
  if (a != x && b != x && a == b) return 3u;  // (y.y)
  return 0u;
}

// Weight-109 "band" CP variant detector for the soa rule-batch L.1/L.2 overlap
// positions (see use_band_interleave).  GENERAL over the band's outer shape: a
// band CP normalizes to `f(P, Q) = x` (equal to a bare variable x) where one of
// the two children is the VARIANT-bearing factor `f(x, X)` -- X = f(a, b) the
// 2-var leaf whose structure round-robins across the batch -- and the OTHER
// child is a CONSTANT 2-var leaf factor (the band's fixed "tail").  WM's single
// superposition scan reaches the variant-producing equations round-robin and
// emits the variants interleaved (x.y),(y.x),(y.y),(x.y),...; the cursor-walk at
// the overlap query position revisits the variant-distinguishing leaf across
// positions (Unifikation1.c RumpfTermMitDSBaumUnifizieren + the Delta*/jump
// continuations).  ONE rule covers every clean interleaving family:
//   `(x.X).(y.x) = x`        variant at [0,1], tail (y.x)  (vater 36/46, L.1)
//   `(x.X).(x.x) = x`        variant at [0,1], tail (x.x)  (vater 37,    L.1)
//   `(x.x).(x.X) = x`        variant at [1,1], tail (x.x)  (vater 38,    L.2)
// The variant-bearing child is whichever of P, Q is f(x, 2-var-leaf); the sibling
// must itself be a constant 2-var leaf factor (so non-band shapes -- a bare
// child, a deeper tower, or a second varying factor -- are rejected).  Returns
// 1/2/3 for the variant class, 0 otherwise.  Orientation-insensitive (the bare-
// var side is either l or r); variable identities matched structurally so the
// detector is renaming-stable.  vc_side selects WHICH child of big bears the
// variant: 0 = variant at [0,1] (left child's right, the L.1 families), 1 =
// variant at [1,1] (right child's right, the L.2 family).  Fixing the side per
// overlap position is essential -- the AMBIGUOUS double-mixed-leaf shapes (e.g.
// `(x.y).(x.(x.y))`, where BOTH children are varying mixed factors) must NOT be
// pulled into the wrong position's interleave pool, so the variant child is
// pinned to the position's structural path.  The L.2.2 cursor-walk GROUPS the
// same shapes (handled separately), so the deeper positions never call this.
static u8 atp_pair_band_variant_side(Term l, Term r, u32 vc_side) {
  Term big, var;
  if (term_tag(l) == TAG_FVR && term_tag(r) == TAG_CTR) { var = l; big = r; }
  else if (term_tag(r) == TAG_FVR && term_tag(l) == TAG_CTR) { var = r; big = l; }
  else return 0u;
  if (term_ctr_n(big) != 2u) return 0u;
  u32 op = term_ext(big), x = term_ext(var);
  // Variant-bearing child f(x, X) with X a 2-var leaf, at the position's side.
  Term vc = term_ctr_at(big, vc_side), sib = term_ctr_at(big, 1u - vc_side);
  if (term_tag(vc) != TAG_CTR || term_ext(vc) != op || term_ctr_n(vc) != 2u)
    return 0u;
  Term vc0 = term_ctr_at(vc, 0), X = term_ctr_at(vc, 1);
  if (term_tag(vc0) != TAG_FVR || term_ext(vc0) != x) return 0u;   // (x . X)
  u8 v = atp_band_variant_of(X, op, x);
  if (v == 0u) return 0u;
  // Sibling must be a constant 2-var leaf factor (both children variables).
  if (term_tag(sib) != TAG_CTR || term_ext(sib) != op || term_ctr_n(sib) != 2u)
    return 0u;
  Term s0 = term_ctr_at(sib, 0), s1 = term_ctr_at(sib, 1);
  if (term_tag(s0) != TAG_FVR || term_tag(s1) != TAG_FVR) return 0u;
  return v;
}

// Weight-109 "band" CP variant detector for the soa rule-36 L.2.2 batch (one
// overlap position DEEPER than the L.1/L.2 bands atp_pair_band_variant_side
// handles).
// The L.2.2 band CP is `((U.c).(d.e)) = g` (a depth-3 left tower dotted with a
// two-var factor, equal to a bare variable), with three variants WM emits
// GROUPED (NOT interleaved -- the OPPOSITE arrival relationship to L.1):
//   A: ((x.x).y).(x.y) = y   -> f(f(f(x,x),y), f(x,y)) = y
//   B: ((x.y).y).(x.y) = y   -> f(f(f(x,y),y), f(x,y)) = y
//   C: ((x.y).x).(y.x) = x   -> f(f(f(x,y),x), f(y,x)) = x
// One side of the normalized CP is a bare variable (the `= g`); the other is
// `f(f(f(a,b),c), f(d,e))` with f the dot operator.  Returns 1/2/3 for A/B/C, 0
// otherwise.  Orientation-insensitive (bare-var side is either l or r); the
// variable identities are matched structurally so the detector is renaming-
// stable.  The bare var (g) coincides with one of the two distinct variables in
// the big side, exactly as in shapes A/B/C above.
static u8 atp_pair_band_l22_variant(Term l, Term r) {
  Term big, var;
  if (term_tag(l) == TAG_FVR && term_tag(r) == TAG_CTR) { var = l; big = r; }
  else if (term_tag(r) == TAG_FVR && term_tag(l) == TAG_CTR) { var = r; big = l; }
  else return 0u;
  // big = f(f(f(a,b), c), f(d,e)).
  if (term_ctr_n(big) != 2u) return 0u;
  u32 op = term_ext(big);
  Term lo = term_ctr_at(big, 0), hi = term_ctr_at(big, 1);     // (U.c) , (d.e)
  if (term_tag(lo) != TAG_CTR || term_ext(lo) != op || term_ctr_n(lo) != 2u)
    return 0u;
  if (term_tag(hi) != TAG_CTR || term_ext(hi) != op || term_ctr_n(hi) != 2u)
    return 0u;
  Term U = term_ctr_at(lo, 0), c = term_ctr_at(lo, 1);          // ((a.b) . c)
  if (term_tag(U) != TAG_CTR || term_ext(U) != op || term_ctr_n(U) != 2u)
    return 0u;
  Term a = term_ctr_at(U, 0), b = term_ctr_at(U, 1);            // (a . b)
  Term d = term_ctr_at(hi, 0), e = term_ctr_at(hi, 1);          // (d . e)
  if (term_tag(a) != TAG_FVR || term_tag(b) != TAG_FVR) return 0u;
  if (term_tag(c) != TAG_FVR) return 0u;
  if (term_tag(d) != TAG_FVR || term_tag(e) != TAG_FVR) return 0u;
  u32 va = term_ext(a), vb = term_ext(b), vc = term_ext(c);
  u32 vd = term_ext(d), ve = term_ext(e), vg = term_ext(var);
  // Two distinct variables x, y; the bare var g is one of them.
  // A: a=x b=x c=y d=x e=y, g=y
  if (va == vb && vc != va && vd == va && ve == vc && vg == vc) return 1u;
  // B: a=x b=y c=y d=x e=y, g=y   (b=c=e=y, a=d=x)
  if (va != vb && vb == vc && vd == va && ve == vb && vg == vb) return 2u;
  // C: a=x b=y c=x d=y e=x, g=x   (a=c=e=x, b=d=y)
  if (va != vb && vc == va && vd == vb && ve == va && vg == va) return 3u;
  return 0u;
}

// Is the slot15 rule term `x.(y.x) -> (y.y).x` already a LIVE rule?  The
// duplicate re-age (THVM_ATP_COMM_DROP_DUP) only fires when slot15 is live,
// so the re-aged CP is a redundant re-derivation of an existing fact (WM ages
// its re-derived copy one slot later); when slot15 is NOT live the CP is a
// birth and is left at its natural age.
static u8 atp_slot15_rule_is_live(AtpState *s) {
  for (u32 k = 0; k < s->n_rules; k++) {
    if (s->r_dead != NULL && s->r_dead[k]) continue;
    if (atp_pair_is_slot15_term(s->lhs[k], s->rhs[k])) return 1u;
  }
  return 0u;
}

// Whether a NORMALIZED CP is the DOUBLE-CUBE `(x.(x.x)).y = (z.(z.z)).y`
// (thvm LHS=(C3 (C3 V0 (C3 V0 V0)) V1), RHS=(C3 (C3 V2 (C3 V2 V2)) V1) modulo
// orientation/renaming): BOTH sides are a cube `f(v, f(v, v))` dotted with the
// SAME trailing variable y, with the two cubes on DIFFERENT inner vars.  This
// is the soa f=28 weight-224 A-phase CP WM emits at selection 1320 (`ue (19,
// -7)`, rule19 x eqn7), one weight band up from the POSGROUP `(x.(x.x)).y =
// y.y` cube.  Reuses atp_term_is_cube_dot_var (already validates the cube
// shape + v != y); the extra constraint is the two trailing vars match and the
// two inner cube vars differ.
static u8 atp_pair_is_double_cube(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  u32 vl = 0, yl = 0, vr = 0, yr = 0;
  if (!atp_term_is_cube_dot_var(l, &vl, &yl)) return 0u;
  if (!atp_term_is_cube_dot_var(r, &vr, &yr)) return 0u;
  if (yl != yr) return 0u;                       // same trailing variable y
  if (vl == vr) return 0u;                        // distinct cube vars x != z
  return 1u;
}

// Whether a NORMALIZED CP is the slot15-term `x.(y.x) = (y.y).x` WRAPPED with
// a trailing `. z` on BOTH sides: `(x.(y.x)).z = ((y.y).x).z` (thvm LHS=(C3
// (C3 V0 (C3 V1 V0)) V2), RHS=(C3 (C3 (C3 V1 V1) V0) V2) modulo orientation/
// renaming).  This is the soa f=28 weight-224 A-phase CP WM emits at selection
// 1321 (`ue (19, -2)`, rule19 x eqn2); it is the double-cube's immediate
// same-group k3-arrival predecessor in thvm's batch ordering and serves as the
// re-key anchor (atp_pair_is_double_cube sorts just below it).  Reuses the
// slot15-term inner-shape checks on the two `. z` subterms.
static u8 atp_pair_is_slot15_wrapped(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_ctr_n(l) != 2u) return 0u;
  if (term_tag(r) != TAG_CTR || term_ctr_n(r) != 2u) return 0u;
  if (term_ext(l) != term_ext(r)) return 0u;
  Term lz = term_ctr_at(l, 1), rz = term_ctr_at(r, 1);
  if (term_tag(lz) != TAG_FVR || term_tag(rz) != TAG_FVR) return 0u;
  if (term_ext(lz) != term_ext(rz)) return 0u;   // same trailing variable z
  Term li = term_ctr_at(l, 0), ri = term_ctr_at(r, 0);
  u32 v = 0, w = 0;
  if (atp_term_is_slot15_lhs(li, &v, &w) && atp_term_is_slot15_rhs(ri, v, w))
    return 1u;
  if (atp_term_is_slot15_lhs(ri, &v, &w) && atp_term_is_slot15_rhs(li, v, w))
    return 1u;
  return 0u;
}

// The oriented superposition partner that WM's seq564 producer rule13 carries:
//   f(v, f(v, f(v, w))) -> f(w, v)   (`x.(x.(x.y)) -> y.x`, WM rule13, soa
// slot19).  In WM this is the rule whose batch forms the seq564 CP from
// eqn-10 (NOT from slot15, which WM never installs); thvm forms BOTH the
// WM-faithful late copy (rule13 x eqn-10) and the over-enumerated early copy
// (rule13 x slot15).  The COMM-DEFER gate suppresses only the latter, so the
// gate pairs THIS oriented shape with the slot15 non-canonical equation shape.
static u8 atp_rule_is_seq564_producer(AtpState *s, u32 ri) {
  if (!s->r_orient[ri]) return 0u;
  Term lhs = s->lhs[ri], rhs = s->rhs[ri];
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0u;
  if (term_ext(lhs) != term_ext(rhs)) return 0u;
  if (term_ctr_n(lhs) != 2u || term_ctr_n(rhs) != 2u) return 0u;
  // LHS = f(v, f(v, f(v, w)))
  Term l0 = term_ctr_at(lhs, 0), l1 = term_ctr_at(lhs, 1);
  if (term_tag(l0) != TAG_FVR) return 0u;
  if (term_tag(l1) != TAG_CTR || term_ext(l1) != term_ext(lhs) ||
      term_ctr_n(l1) != 2u) return 0u;
  Term l10 = term_ctr_at(l1, 0), l11 = term_ctr_at(l1, 1);
  if (term_tag(l10) != TAG_FVR || term_ext(l10) != term_ext(l0)) return 0u;
  if (term_tag(l11) != TAG_CTR || term_ext(l11) != term_ext(lhs) ||
      term_ctr_n(l11) != 2u) return 0u;
  Term l110 = term_ctr_at(l11, 0), l111 = term_ctr_at(l11, 1);
  if (term_tag(l110) != TAG_FVR || term_ext(l110) != term_ext(l0)) return 0u;
  if (term_tag(l111) != TAG_FVR || term_ext(l111) == term_ext(l0)) return 0u;
  u32 v = term_ext(l0), w = term_ext(l111);
  // RHS = f(w, v)
  Term r0 = term_ctr_at(rhs, 0), r1 = term_ctr_at(rhs, 1);
  if (term_tag(r0) != TAG_FVR || term_ext(r0) != w) return 0u;
  return (u8)(term_tag(r1) == TAG_FVR && term_ext(r1) == v);
}

static u32 atp_wmo_collect_pair(AtpState *s, u32 i, u32 j,
                                AtpWmoCpEnt **big, u32 *n_big, u32 *cap_big,
                                CriticalPair *buf) {
  if (s->r_gj_status[i] == ATP_GJ_ST_JOINABLE ||
      s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) {
    return 0;
  }
  // Overlap-exhausted-equation gate (default OFF): when a NEWLY-added
  // commutativity (flat transposition `x|y=y|x`) would re-superpose against
  // an OLD unorientable equation whose birth-batch is already enumerated
  // (r_overlap_done), skip it -- WM overlaps commutativity with the equation's
  // fresh re-derivation (late age), not the stale original (early age).  The
  // exhausted+transposition pairing keeps comm x freshly-derived equations.
  if (s->use_overlap_exhaust) {
    u8 i_exh = s->r_overlap_done[i] && !s->r_orient[i];
    u8 j_exh = s->r_overlap_done[j] && !s->r_orient[j];
    if ((i_exh && !s->r_overlap_done[j]
         && atp_is_flat_transposition(s->lhs[j], s->rhs[j]))
        || (j_exh && !s->r_overlap_done[i]
            && atp_is_flat_transposition(s->lhs[i], s->rhs[i]))) {
      return 0;
    }
  }
  // Commutativity-DEFER gate (default OFF): skip the over-enumerated overlap
  // of a NEWLY-born ORIENTED rule re-superposed against an OLDER NON-CANONICAL
  // comm-side equation (soa slot19=rule13 x slot15) without removing slot15 --
  // WM never installs slot15 as a producing rule, so its slot19 batch never
  // emits this overlap; thvm's WM-faithful late seq564 copy (slot19 x eqn-10)
  // then wins the early window.  The index guard (oriented rule index >
  // equation index, i.e. born AFTER) restricts the skip to the new-oriented-
  // rule-re-overlaps-old-equation direction, so the equation's OWN birth batch
  // (where it produces its uniquely-parented pick-99 COMM copy via slot5) is
  // untouched.
  if (s->use_comm_defer) {
    if ((i > j && atp_rule_is_seq564_producer(s, i) && !s->r_orient[j]
         && atp_eq_is_noncanonical_comm_side(s, s->lhs[j], s->rhs[j]))
        || (j > i && atp_rule_is_seq564_producer(s, j) && !s->r_orient[i]
            && atp_eq_is_noncanonical_comm_side(s, s->lhs[i], s->rhs[i]))) {
      return 0;
    }
  }
  u32 combo_end[4] = {0, 0, 0, 0};
  u32 nbuf = atp_overlap_ij(s, i, j, buf, ATP_CP_BATCH, combo_end);
  if (g_atp_phase_enabled) {
    if (nbuf == 0u) g_atp_wmcp_zero++; else g_atp_wmcp_nonzero++;
  }
  if (nbuf == 0u) return 0;
  if (*n_big + nbuf > *cap_big) {
    u32 cap = *cap_big ? *cap_big : 256u;
    while (cap < *n_big + nbuf) cap *= 2u;
    AtpWmoCpEnt *grown =
        (AtpWmoCpEnt *)realloc(*big, cap * sizeof(AtpWmoCpEnt));
    if (grown == NULL) return 0;   // degrade: drop this pair's CPs
    *big = grown;
    *cap_big = cap;
  }
  for (u32 k = 0; k < nbuf; k++) {
    AtpWmoCpEnt *e = &(*big)[(*n_big)++];
    e->cp = buf[k];
    e->i = i;
    e->j = j;
    e->combo = (k < combo_end[0]) ? 0u
             : (k < combo_end[1]) ? 1u
             : (k < combo_end[2]) ? 2u : 3u;
    e->cp.combo = e->combo;  // tag the CP so the combo survives to the trace
    e->seq = *n_big - 1u;
  }
  return nbuf;
}

static u32 thvm_atp_generate_cps_wm(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;
#ifdef ATP_RULE_INDEX
  // Overlap-partner index (default ON for the WM presets, gated by
  // use_cp_index).  The two collect loops below superpose a new fact `f`
  // against EVERY existing rule -- an O(n_rules)/step scan that profiles at
  // ~95% of the WM saturation, yet most pairs collect zero CPs (no
  // unifiable subterm).  The discrimination-tree filter returns a sound
  // SUPERSET of the truly-overlapping partners (both faces indexed +
  // queried, so every unfailing combo is covered), and the authoritative
  // thvm_unify in cp_visit gates emission, so the CP SET is identical.  The
  // batch is re-keyed by atp_wmo_rank and re-sorted, and each direction's
  // candidates are visited in ASCENDING rule order (atp_cp_cand_sort),
  // exactly as the full scans did -- so the per-CP FIFO `seq` tiebreak is
  // byte-identical too.  On a candidate-buffer / subject-depth overflow the
  // affected direction falls back to the exact n_rules scan, preserving the
  // set even when the index over-runs its scratch.
  u8 cp_ix = s->use_cp_index ? 1u : 0u;
  if (cp_ix) {
    if (s->cp_index == NULL)    s->cp_index    = atp_ri_new();
    if (s->cp_subindex == NULL) s->cp_subindex = atp_ri_new();
    // Stale iff EITHER the rule count or the rule-set revision moved (an
    // interreduce drop+add keeps n_rules constant while renumbering slots).
    if (s->cp_index->n_rules_built != n ||
        s->cp_index->built_revision != s->r_revision) {
      atp_cp_index_rebuild(s);
      atp_cp_subindex_rebuild(s);
    }
  }
#endif
  for (u32 f = first; f < last; f++) {
    AtpWmoCpEnt *big = NULL;
    u32 n_big = 0, cap_big = 0;
#ifdef ATP_RULE_INDEX
    if (cp_ix) {
      // tops + self: f as the OUTER.  Candidates j (ascending) are rules
      // whose stored face unifies with a non-var subterm of one of f's
      // faces.  f itself is in the index (whole-LHS at its root), so the
      // self pair is collected.
      u32 nc = atp_cp_index_collect(s, f);
      if (g_atp_cp_overflow) {
        if (g_atp_phase_enabled) g_atp_wmcp_cand += n;
        for (u32 j = 0; j < n; j++)
          atp_wmo_collect_pair(s, f, j, &big, &n_big, &cap_big, buf);
      } else {
        if (g_atp_phase_enabled) g_atp_wmcp_cand += nc;
        atp_cp_cand_sort();
        for (u32 c = 0; c < nc; c++)
          atp_wmo_collect_pair(s, f, g_atp_cp_cand[c], &big, &n_big,
                               &cap_big, buf);
      }
      // eTT: f as the INNER, planted into OLD facts (i < first).  Candidates
      // i (ascending) are OLD rules whose stored face has a non-var subterm
      // unifiable with one of f's faces.
      nc = atp_cp_subindex_collect(s, f);
      if (g_atp_cp_overflow) {
        if (g_atp_phase_enabled) g_atp_wmcp_cand += first;
        for (u32 i = 0; i < first; i++)
          atp_wmo_collect_pair(s, i, f, &big, &n_big, &cap_big, buf);
      } else {
        atp_cp_cand_sort();
        for (u32 c = 0; c < nc; c++) {
          u32 i = g_atp_cp_cand[c];
          if (i < first) {
            if (g_atp_phase_enabled) g_atp_wmcp_cand += 1u;
            atp_wmo_collect_pair(s, i, f, &big, &n_big, &cap_big, buf);
          }
        }
      }
      if (g_atp_phase_enabled) g_atp_wmcp_full += (u64)n + (u64)first;
    } else
#endif
    {
    // tops + self: f as the outer (positions in f's faces)
    for (u32 j = 0; j < n; j++) {
      atp_wmo_collect_pair(s, f, j, &big, &n_big, &cap_big, buf);
    }
    // eTT: f as the inner, planted into the OLD facts' positions
    for (u32 i = 0; i < first; i++) {
      atp_wmo_collect_pair(s, i, f, &big, &n_big, &cap_big, buf);
    }
    }
    for (u32 k = 0; k < n_big; k++) {
      big[k].key = atp_wmo_rank(s, f, big[k].i, big[k].j, big[k].combo,
                                &big[k].cp);
      big[k].key_raw = big[k].key;
    }
    // Commutativity-REAGE re-rank (default OFF; INVERSE of comm-defer).  WM
    // selects the seq564-sibling CP `(x.x).y = (x.y).y` at pick-126.  thvm
    // forms it in rule13's (slot19's) OWN birth batch from rule13 x slot15
    // (the non-canonical comm-side equation WM never installs), where slot15's
    // small leaf-list rank keys it ONE slot too early (cp_seq 564, selected at
    // pick-125) -- ahead of the rule13 x eqn-6 copy (cp_seq 565, the CP WM
    // selects at pick-125).  COMM_DEFER over-corrected by deleting this copy
    // entirely (-> first seq564 at pick-135).  Instead, RE-AGE it: bump its
    // batch key just past the rule13 x eqn-6 copy so it sorts ONE slot later,
    // landing at cp_seq 565 -> selected at WM's faithful pick-126.  Scoped to
    // the (rule13-producer x non-canonical comm-side slot15) pair confirmed by
    // the normalized joined content; only the FIRST match per batch is re-aged
    // (exactly one early copy).  The anchor is the SMALLEST-keyed other CP in
    // this batch that already out-keys the sibling (the rule13 x eqn-6 copy WM
    // ages just before its cp877); +1 slots the sibling immediately after it.
    if (s->use_comm_reage) {
      u32 sib = 0xffffffffu;
      for (u32 k = 0; k < n_big; k++) {
        u32 i = big[k].i, j = big[k].j;
        if (i != f || !atp_rule_is_seq564_producer(s, i)) continue;
        if (!atp_eq_is_noncanonical_comm_side(s, s->lhs[j], s->rhs[j])) continue;
        Term nl = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
        Term nr = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
        if (!atp_pair_is_seq564_sibling(nl, nr)) continue;
        sib = k;
        break;
      }
      if (sib != 0xffffffffu) {
        // Smallest batch key strictly greater than the sibling's = the CP WM
        // ages right before its cp877; re-age the sibling to one past it.
        u64 anchor = 0xffffffffffffffffull;
        for (u32 k = 0; k < n_big; k++) {
          if (k == sib) continue;
          if (big[k].key > big[sib].key && big[k].key < anchor)
            anchor = big[k].key;
        }
        if (anchor != 0xffffffffffffffffull) big[sib].key = anchor + 1u;
      }
    }
    // Commutativity DROP-DUP re-age (default OFF; see use_comm_drop_dup).
    // atop COMM_REAGE, the residual firstdiv=288 is the DUPLICATE re-derivation
    // of slot15's term `x.(y.x) = (y.y).x` that thvm forms in rule34's birth
    // batch.  slot15 (the rule of this exact shape) is already LIVE (since
    // pick-54), so this copy is redundant; its batch key sorts it ONE FIFO slot
    // EARLIER (cp_seq 1523) than WM ages its own re-derived copy.  WM does NOT
    // drop it -- it RE-SELECTS a slot15-class copy, one slot later, at pick-289
    // (its in-batch `x.(x.x) = y.(y.y)` successor is WM's pick-288).  RE-AGE the
    // duplicate to one slot past that successor (the smallest batch key strictly
    // greater than its own), so it lands at WM's faithful pick-289.  Scoped to
    // the FIRST in-batch slot15-term CP (normalized) and gated on slot15 being
    // live, so slot15 the rule -- and its uniquely-parented pick-99 COMM copy --
    // stay intact.  Advances soa firstdiv 288 -> 290.
    //
    // Anchor skip (use_comm_drop_dup_class_gate): the smallest-keyed CP
    // strictly above the slot15-term is the re-age anchor.  On soa that
    // successor is always a tautology or an asymmetric/slot15-class CP WM emits
    // BEFORE the slot15-term, so +1 is faithful.  On Meredith OrAssociativity
    // TWO anchor shapes are instead emitted AFTER the slot15-term, so splicing
    // past them mis-orders the pair:
    //   rule-51: the permutation CLASS `(x.y).y = (y.x).y`
    //            (atp_pair_is_inner_swap_class) -- a duplicate copy of the
    //            band's class equation.  The slot15-term's raw leaf arrival
    //            (k3=4) already places it at WM's pick-1175 slot, ahead of the
    //            class copy (k3=15); +1 splices it past, firstdiv 1175.
    //   rule-59: the slot15-ROTATE `x.(y.x) = (x.y).x`
    //            (atp_pair_is_slot15_rotate) -- the SAME `x.(y.x)` LHS as the
    //            slot15-term, but `(x.y).x` (comm-left reuses the outer var) on
    //            the RHS instead of `(y.y).x`.  WM keeps the slot15-term at its
    //            raw leaf age (seq 6011) ahead of the rotate (seq 6010); +1 ages
    //            it past, firstdiv 1374.
    // Skip the re-age when the anchor is EITHER shape: no soa anchor is either
    // exact shape (soa's nearest rotate variant is `x.(x.y) = (x.y).x`, inner
    // pair NOT swapped), so every soa fire stays byte-identical.  Advances
    // Meredith firstdiv 1175 -> 1374 (class) -> 4190 (rotate, = the
    // COMM_DROP_DUP-OFF ceiling).
    if (s->use_comm_drop_dup && atp_slot15_rule_is_live(s)) {
      u32 dup = 0xffffffffu;
      for (u32 k = 0; k < n_big; k++) {
        if (big[k].i != f) continue;
        Term nl = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
        Term nr = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
        if (!atp_pair_is_slot15_term(nl, nr)) continue;
        dup = k;
        break;
      }
      if (dup != 0xffffffffu) {
        u64 anchor = 0xffffffffffffffffull;
        u32 anc_k = 0xffffffffu;
        for (u32 k = 0; k < n_big; k++) {
          if (k == dup) continue;
          if (big[k].key > big[dup].key && big[k].key < anchor) {
            anchor = big[k].key;
            anc_k = k;
          }
        }
        u8 anchor_is_class = 0u;
        if (s->use_comm_drop_dup_class_gate && anc_k != 0xffffffffu) {
          Term al = atp_rewrite_normalize_indexed(s, big[anc_k].cp.lhs, 4096u);
          Term ar = atp_rewrite_normalize_indexed(s, big[anc_k].cp.rhs, 4096u);
          // Skip the re-age when the anchor is a Meredith-harmful shape WM
          // emits AFTER the slot15-term: the inner-swap permutation class
          // `(x.y).y = (y.x).y` (rule-51) OR the slot15-ROTATE `x.(y.x) =
          // (x.y).x` (rule-59).  Neither shape occurs as a soa anchor, so soa
          // fires stay byte-identical.
          anchor_is_class = (u8)(atp_pair_is_inner_swap_class(al, ar) ||
                                 atp_pair_is_slot15_rotate(al, ar));
        }
        if (anchor != 0xffffffffffffffffull && !anchor_is_class)
          big[dup].key = anchor + 1u;
      }
    }
    // Leaf-arrival tiebreak (default OFF; see use_leaf_tiebreak).  Two CPs
    // that overlap the new fact at the SAME position with two DIFFERENT
    // equation partners -- a var-differ==0 partner (a permutation equation WM
    // keeps two-faced) and a var-differ==1 partner (the asymmetric equation WM
    // ORIENTED) -- age reversed in thvm vs WM at one specific configuration.
    // WM's single oriented scan surfaces the var-differ==1 copy at the EARLIER
    // discrimination-tree leaf, so it ages ONE slot before the permutation
    // copy; thvm indexes the oriented equation's stored face at the
    // ADJACENT-HIGHER leaf, keying the var-differ==1 copy EXACTLY one k3 unit
    // (1<<28) ABOVE the permutation copy (every other key field -- phase, k1,
    // k2, k4, k5 -- identical, i.e. the SAME overlap geometry), so the two emit
    // reversed.  Detect exactly that configuration -- a var-differ==1 equation
    // CP B whose key is (A.key + (1<<28)) above a var-differ==0 equation CP A
    // in this batch at the same combo and overlap-position length -- and re-key
    // B to A.key-1 so the oriented-scan copy sorts FIRST, matching WM's
    // KPVerwaltung emission.  The one-k3-step delta + var-differ split + same
    // overlap geometry IS the WM emission signature; the gate fires ONLY on
    // that precise divergent configuration, never on a generic equal-weight
    // batch.  Clears the soa 290<->292 / 303<->305 / 351<->353 swap-pairs
    // (firstdiv 290 -> trace end); the byte-identical 1..289 prefix is
    // preserved (the early-batch fires re-age CPs WM already agrees on, so the
    // selection content is unchanged there).
    //
    // Face gate (use_leaf_tiebreak_facegate): the flip above assumes thvm's
    // tree DFS keys the var-differ==1 partner ADJACENT-HIGHER than the var-
    // differ==0 partner -- i.e. inverted vs WM's single oriented scan, which
    // reaches the var-differ==1 distinguished-face leaf first -- so the re-key
    // restores WM's order.  That holds only when the new fact overlaps the
    // var-differ==1 partner on its WM-DISTINGUISHED face (jb_face_wm==0): WM's
    // scan reaches that same face, and the inversion is real.  When instead the
    // var-differ==1 partner is overlapped on its DISTINGUISHED face but the
    // var-differ==0 partner on its WM-REVERSE face (ja_face_wm==1), thvm's DFS
    // arrival ALREADY matches WM's formation order (WM forms the permutation
    // partner's CP from its earlier-reached distinguished-face leaf FIRST, the
    // oriented partner's second), so flipping is wrong.  At Meredith
    // OrAssociativity rule-45's weight-120 band that exact (jb_fwm==0,
    // ja_fwm==1) pair (`x.(y.y) = (y.x).x` over the oriented `(x.(x.x)).y = y.y`
    // vs `(x.y).y = (x.x).y` over the permutation `(x.(y.x)).z = z.(x.(z.y))`)
    // is WM-faithful unflipped: WM forms the permutation CP at CPNr 4563 before
    // the oriented CP at 4573.  Skip the re-key on that face configuration.
    // Scoped to the i==f tops phase (the partner is j); a no-op when the flip
    // condition itself does not fire.  OFF byte-identical; advances Meredith
    // firstdiv 1040 -> 1047, soa firstdiv 2808 unchanged.
    if (s->use_leaf_tiebreak) {
      AtpWmOrder *w = (AtpWmOrder *)s->wmo;
      for (u32 kb = 0; kb < n_big; kb++) {
        u32 jb = (big[kb].i == f) ? big[kb].j : big[kb].i;
        if (s->r_orient[jb]) continue;                 // equation partner only
        if (!wmo_eq_sides_var_differ(s->lhs[jb], s->rhs[jb])) continue;
        if (big[kb].key < (1ull << 28)) continue;
        u64 want = big[kb].key - (1ull << 28);
        u32 ka = 0xffffffffu;
        for (u32 k = 0; k < n_big; k++) {
          if (k == kb || big[k].key != want) continue;
          u32 ja = (big[k].i == f) ? big[k].j : big[k].i;
          if (s->r_orient[ja]) continue;
          if (wmo_eq_sides_var_differ(s->lhs[ja], s->rhs[ja])) continue;
          if (big[k].combo != big[kb].combo) continue;
          if (big[k].cp.pos_len != big[kb].cp.pos_len) continue;
          ka = k;
          break;
        }
        if (ka != 0xffffffffu && big[ka].key >= 1ull) {
          if (s->use_leaf_tiebreak_facegate) {
            u32 ja = (big[ka].i == f) ? big[ka].j : big[ka].i;
            u8 jb_face = (big[kb].i == f) ? (big[kb].combo & 1u)
                                          : ((big[kb].combo >> 1) & 1u);
            u8 ja_face = (big[ka].i == f) ? (big[ka].combo & 1u)
                                          : ((big[ka].combo >> 1) & 1u);
            u8 jb_fwm = jb_face ^ wmo_trace_dist_rhs(w, s->r_trace[jb]);
            u8 ja_fwm = ja_face ^ wmo_trace_dist_rhs(w, s->r_trace[ja]);
            // WM-faithful unflipped: oriented partner on its distinguished face,
            // permutation partner on its reverse face -- thvm's arrival already
            // matches WM's formation order.
            if (jb_fwm == 0u && ja_fwm == 1u) continue;
          }
          big[kb].key = big[ka].key - 1ull;
        }
      }
    }
    // Reverse-face shape-group tiebreak (default OFF; see
    // use_revface_group).  Within one tops overlap-position group (D
    // phase, i == f, identical phase/k1/k2 prefix), thvm sorts competing
    // partner CPs by the partner's discrimination-tree leaf-arrival rank
    // (k3).  A PERMUTATION (var-differ==0) equation partner whose REVERSE
    // face overlaps the new fact reduces to the SAME equation as an
    // EARLIER-arriving CP in the group (soa f=36, w=209: the permutation
    // `(x.(x.x)).y = y.(x.(x.x))` reverse overlap reduces to the Cshape
    // `(x.(x.x)).y = z.(z.z) # y`, the same equation an earlier partner's
    // overlap already produced).  WM's single superposition scan emits that
    // reverse-face copy ADJACENT to the earlier same-shape CP (its
    // KPVerwaltung surfaces the shared distinguished face together), AHEAD of
    // the group's var-differ==1 (WM-oriented) partner CPs; but thvm's
    // independent leaf DFS keys the permutation copy far later (arr 6 vs 1),
    // scattering it past those other-shape CPs (its Cshape lands at pick-780,
    // the two `#x1` oriented-partner forms at 778/779, where WM emits the
    // Cshape at 778).  Re-key the permutation reverse-face copy to sort
    // immediately after the largest-keyed same-group CP it alpha-matches, so
    // the same-shape copies stay consecutive as WM emits them.  Scoped HARD:
    // combo carries the partner reverse face (bit0, partner = j when i == f),
    // the partner is a var-differ==0 PERMUTATION equation, D phase, the group
    // prefix (phase/k1/k2) is identical, and the NORMALIZED joined pairs
    // alpha-match (orientation-insensitive) -- never a generic equal-weight
    // reorder.  Advances soa firstdiv 778 -> 966 (clears multiple w=209/w=189
    // Cshape clusters); see tools/baselines/wm_align_reports/soa.txt.
    if (s->use_revface_group) {
      const u64 grp_mask = ~((1ull << 42) - 1ull);   // phase | k1 | k2 bits
      for (u32 kb = 0; kb < n_big; kb++) {
        if (big[kb].i != f) continue;                // D-phase tops, outer = f
        u32 jb = big[kb].j;
        if (jb == f) continue;
        if (s->r_orient[jb]) continue;               // equation partner only
        if (wmo_eq_sides_var_differ(s->lhs[jb], s->rhs[jb])) continue; // permutation
        u8 jb_face = big[kb].combo & 1u;             // 1 = partner reverse face
        if (jb_face == 0u) continue;
        if ((big[kb].key >> 58) != 0u) continue;     // D phase only
        Term nlb = atp_rewrite_normalize_indexed(s, big[kb].cp.lhs, 4096u);
        Term nrb = atp_rewrite_normalize_indexed(s, big[kb].cp.rhs, 4096u);
        if (kbo_eq(nlb, nrb)) continue;              // trivially joined
        u32 ka = 0xffffffffu;
        for (u32 k = 0; k < n_big; k++) {
          if (k == kb || big[k].i != f || big[k].j == f) continue;
          if (big[k].key >= big[kb].key) continue;   // strictly earlier
          if ((big[k].key & grp_mask) != (big[kb].key & grp_mask)) continue;
          Term nla = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
          Term nra = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
          if (!atp_pair_alpha_eq(nla, nra, nlb, nrb)) continue;
          if (ka == 0xffffffffu || big[k].key > big[ka].key) ka = k;
        }
        if (ka != 0xffffffffu) {
          // POSGROUP un-group (default OFF; see use_posgroup part a).  WM
          // groups a permutation partner's reverse face beside an EARLIER copy
          // of the same re-derived equation only when the two are genuinely one
          // equation surfaced by WM's single scan.  At soa f=41 (weight 120)
          // REVFACE instead pulls the `(x.(x.x)).y = y.y` forward-face CP of a
          // vd=0 permutation partner (j33, raw arrival 7) up beside the SAME-
          // shape CP of a vd=1 ORIENTED partner (j7, arrival 1), jumping 16
          // intervening same-group CPs -- but WM keeps j33 at its own raw
          // arrival because j7 and j33 are independent leaves.  Skip the group
          // for that exact shape so kb keeps its raw arrival key.  Scoped to the
          // `(x.(x.x)).y = y.y` cube CP and a vd=1 oriented anchor.
          if (s->use_posgroup && atp_pair_is_posgroup_cube(nlb, nrb)) {
            u32 ja = (big[ka].i == f) ? big[ka].j : big[ka].i;
            // ka alpha-matches kb (loop invariant), so ka is the same cube
            // shape; un-group only when its partner is a vd=1 oriented equation.
            if (ja != f && !s->r_orient[ja] &&
                wmo_eq_sides_var_differ(s->lhs[ja], s->rhs[ja]))
              continue;
          }
          big[kb].key = big[ka].key + 1ull;
        }
      }
    }
    // POSGROUP reverse-face deferral (default OFF; see use_posgroup part
    // b).  A vd=0 permutation partner's REVERSE face (thvm combo bit0 == 0)
    // for the `(x.x).y = (x.y).y` shape (soa f=41 j19 face0, raw arrival 6)
    // indexes at a discrimination-tree leaf whose raw arrival precedes the
    // higher-arrival `(x.(x.x)).y = y.y` cube CPs WM emits first (the arr-7
    // oriented-partner copies).  WM brackets the permutation partner's two
    // faces around those intervening cube CPs -- forward face early (arr 5),
    // reverse face only AFTER the cube cluster -- so its single scan reaches
    // the reverse leaf last.  Re-key the reverse face just past the largest
    // raw arrival among the same-group cube CPs so it sorts after that
    // cluster.  Scoped HARD to the A phase, identical group prefix, the
    // `(x.x).y = (x.y).y` permutation reverse face, and a strictly-higher
    // raw-arrival same-group cube CP -- never a generic equal-weight reorder.
    if (s->use_posgroup) {
      const u64 grp_mask = ~((1ull << 42) - 1ull);     // phase | k1 | k2 bits
      for (u32 kb = 0; kb < n_big; kb++) {
        if (big[kb].i != f || big[kb].j == f) continue;
        u32 jb = big[kb].j;
        if (s->r_orient[jb]) continue;                 // equation partner only
        if (wmo_eq_sides_var_differ(s->lhs[jb], s->rhs[jb])) continue; // perm
        if ((big[kb].combo & 1u) != 0u) continue;      // face 0 (reverse) only
        if ((big[kb].key_raw >> 58) != 0u) continue;   // A phase only
        Term nlb = atp_rewrite_normalize_indexed(s, big[kb].cp.lhs, 4096u);
        Term nrb = atp_rewrite_normalize_indexed(s, big[kb].cp.rhs, 4096u);
        if (!atp_pair_is_seq564_sibling(nlb, nrb)) continue; // `(x.x).y=(x.y).y`
        u32 arr_kb = (u32)((big[kb].key_raw >> 28) & 0x3fffu);
        // Largest raw-arrival key among same-group cube CPs that arrive
        // strictly later than this reverse face: WM emits the reverse face
        // right after them.  Skip if none.
        u64 anchor = 0u;
        for (u32 k = 0; k < n_big; k++) {
          if (k == kb || big[k].i != f || big[k].j == f) continue;
          if ((big[k].key_raw & grp_mask) != (big[kb].key_raw & grp_mask))
            continue;
          u32 arr_k = (u32)((big[k].key_raw >> 28) & 0x3fffu);
          if (arr_k <= arr_kb) continue;               // not a later arrival
          Term nlk = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
          Term nrk = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
          if (!atp_pair_is_posgroup_cube(nlk, nrk)) continue; // cube cluster
          if (big[k].key > anchor) anchor = big[k].key;
        }
        if (anchor != 0u && anchor + 1ull > big[kb].key) big[kb].key = anchor + 1ull;
      }
    }
    // Cube-arrival tiebreak (default OFF; see use_cube_arrival).  One weight
    // band up from POSGROUP (soa f=28, weight 224): the DOUBLE-CUBE CP
    // `(x.(x.x)).y = (z.(z.z)).y` (rule28 x slot8 = WM rule19 x eqn7) and its
    // immediate same-group k3-arrival predecessor, the slot15-wrapped CP
    // `(x.(y.x)).z = ((y.y).x).z` (rule28 x slot2 = WM rule19 x eqn2), share
    // the entire A-phase tops group prefix (phase=0, k1=3, k2=1) and differ
    // ONLY in k3, the partner equation's discrimination-tree arrival rank:
    // thvm assigns the eqn2 partner an EARLIER arrival (8) than the cube eqn7
    // partner (10), so it sorts the slot15-wrapped CP first; but WM's single
    // superposition scan surfaces eqn7 first (`ue (19, -7)` before `ue (19,
    // -2)`), emitting the double-cube AHEAD of the slot15-wrapped CP.  Re-key
    // the double-cube to sort immediately BELOW its slot15-wrapped same-group
    // predecessor (the largest-keyed strictly-earlier same-group CP whose
    // normalized shape is the slot15-wrapped form), swapping the adjacent
    // pair to WM's emission order.  Scoped HARD -- both shapes exact
    // (orientation-insensitive), the A phase, an identical group prefix
    // (phase|k1|k2), and the anchor strictly earlier-keyed -- never a generic
    // equal-weight reorder.  OFF byte-identical; the prior-8-knobs 1..1319
    // prefix is preserved.  Advances soa firstdiv 1320 -> beyond.
    if (s->use_cube_arrival) {
      const u64 grp_mask = ~((1ull << 42) - 1ull);     // phase | k1 | k2 bits
      for (u32 kb = 0; kb < n_big; kb++) {
        if (big[kb].i != f || big[kb].j == f) continue;
        if ((big[kb].key_raw >> 58) != 0u) continue;   // A phase only
        Term nlb = atp_rewrite_normalize_indexed(s, big[kb].cp.lhs, 4096u);
        Term nrb = atp_rewrite_normalize_indexed(s, big[kb].cp.rhs, 4096u);
        if (!atp_pair_is_double_cube(nlb, nrb)) continue;
        u32 ka = 0xffffffffu;
        for (u32 k = 0; k < n_big; k++) {
          if (k == kb || big[k].i != f || big[k].j == f) continue;
          if (big[k].key >= big[kb].key) continue;     // strictly earlier
          if ((big[k].key & grp_mask) != (big[kb].key & grp_mask)) continue;
          Term nlk = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
          Term nrk = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
          if (!atp_pair_is_slot15_wrapped(nlk, nrk)) continue;
          if (ka == 0xffffffffu || big[k].key > big[ka].key) ka = k;
        }
        if (ka != 0xffffffffu && big[ka].key >= 1ull)
          big[kb].key = big[ka].key - 1ull;
      }
    }
    // Band-interleave re-key (default OFF; see use_band_interleave).  The soa
    // firstdiv-1953 divergence: a rule's tops batch forms weight-109 band CPs at
    // an interleaving overlap position (`f(P, Q) = x`, one child the variant-
    // bearing `f(x, X)` with X in {(x.y), (y.x), (y.y)}, the other a constant
    // 2-var tail).  WM's single superposition scan reaches the variant-producing
    // equations round-robin and emits the variants interleaved
    // (x.y),(y.x),(y.y),(x.y),...; thvm sorts by the partner equation's
    // discrimination-tree arrival (k3), grouping the variants.  Re-key the band
    // CPs onto a (round, variant) interleave -- round = count of EARLIER same-
    // variant band CPs in this batch's current key order, variant rank
    // (x.y)<(y.x)<(y.y) -- so they sort (x.y),(y.x),(y.y),(x.y),...  The band
    // CPs keep their original key SLOTS (the multiset of keys is permuted only
    // among themselves), so non-band CPs and the surrounding batch order are
    // untouched.  GENERAL over the band's outer shape (atp_pair_band_variant_side
    // is tail-agnostic, pinning only the position's variant-child path), so ONE
    // rule clears every clean interleaving band family at its position:
    // `(x.X).(y.x)` (vater 36/46, L.1, firstdiv 1953->1967), the idempotent-tail
    // `(x.X).(x.x)` (vater 37, L.1), and the L.2 `(x.x).(x.X)` (vater 38) --
    // advancing firstdiv past 1985.  Scoped HARD
    // to the L.1 (pos[0]==0) and L.2 (pos[0]==1) overlap positions, run
    // independently per position: those cursor-walks interleave, whereas the
    // L.2.2 position GROUPS the same shapes (handled separately below) -- so the
    // re-key must not reach the deeper positions.  OFF byte-identical.
    if (s->use_band_interleave) {
      // Interleave each interleaving overlap position INDEPENDENTLY: the L.1
      // (pos[0]==0) and L.2 (pos[0]==1) bands are distinct cursor-walk families
      // with disjoint key ranges, so each position's variants round-robin among
      // themselves.  A single shared pool would conflate the two batches' round
      // counters; iterate the positions separately.
      enum { BAND_CAP = 64u };
      for (u32 ovp = 0u; ovp <= 1u; ovp++) {
        // Index this position's band CPs (entry index + variant), capped at a
        // small fixed window -- the band families run to ~11 CPs; a generous cap
        // covers any multiplicity without unbounded scratch.
        u32 band_idx[BAND_CAP];
        u8  band_var[BAND_CAP];
        u64 band_key[BAND_CAP];
        u32 n_band = 0;
        for (u32 k = 0; k < n_big && n_band < BAND_CAP; k++) {
          // Length-1 pos path [ovp]: L.1 (ovp==0) and L.2 (ovp==1).  The
          // variant-bearing child side matches the position: L.1 -> [0,1],
          // L.2 -> [1,1] (the minimal clean-family path set).
          if (big[k].cp.pos_len != 1u || big[k].cp.pos[0] != ovp) continue;
          Term nl = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
          Term nr = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
          u8 var = atp_pair_band_variant_side(nl, nr, ovp);
          if (var == 0u) continue;
          band_idx[n_band] = k;
          band_var[n_band] = var;
          band_key[n_band] = big[k].key;
          n_band++;
        }
        if (n_band < 2u) continue;
        // Sort the band entries by current key (ascending), so "round" = the
        // count of same-variant band CPs that currently precede each one.
        for (u32 a = 0; a + 1u < n_band; a++) {
          for (u32 b = a + 1u; b < n_band; b++) {
            if (band_key[b] < band_key[a]) {
              u64 tk = band_key[a]; band_key[a] = band_key[b]; band_key[b] = tk;
              u32 ti = band_idx[a]; band_idx[a] = band_idx[b]; band_idx[b] = ti;
              u8  tv = band_var[a]; band_var[a] = band_var[b]; band_var[b] = tv;
            }
          }
        }
        // Compute each band CP's (round, variant) rank, then re-assign the
        // sorted key slots in that order.  variant rank: (x.y)(1)->0,
        // (y.x)(2)->1, (y.y)(3)->2.  round = number of earlier same-variant CPs.
        u32 round[BAND_CAP];
        u32 seen[4] = {0, 0, 0, 0};
        for (u32 a = 0; a < n_band; a++) {
          round[a] = seen[band_var[a]];
          seen[band_var[a]]++;
        }
        // order = the band index permutation sorted by (round, variant_rank).
        u32 order[BAND_CAP];
        for (u32 a = 0; a < n_band; a++) order[a] = a;
        for (u32 a = 0; a + 1u < n_band; a++) {
          for (u32 b = a + 1u; b < n_band; b++) {
            u32 pa = order[a], pb = order[b];
            u32 ka = round[pa] * 3u + (band_var[pa] - 1u);
            u32 kb = round[pb] * 3u + (band_var[pb] - 1u);
            if (kb < ka) { order[a] = pb; order[b] = pa; }
          }
        }
        // band_key[] holds the sorted key slots; assign them to the band CPs in
        // (round, variant) order.
        for (u32 a = 0; a < n_band; a++) {
          big[band_idx[order[a]]].key = band_key[a];
        }
      }
      // Position-aware sub-logic: the SAME tops batch forms a SECOND weight-109
      // band one overlap position DEEPER (ovPos=L.2.2, thvm pos[0]==1 &&
      // pos[1]==1), the depth-3 tower CPs `((U.c).(d.e)) = g` (variants A/B/C in
      // atp_pair_band_l22_variant).  Here WM's single superposition scan emits
      // the variants GROUPED -- all A, then all B, then all C (cpform.out cpnr
      // 8145<8153<8162 = inner-partner-leaf tops-arrival order 5<6<10) -- the
      // OPPOSITE of the L.1 round-robin.  thvm's k3 (new-fact query-subterm leaf
      // arrival) sorts them C,B,A (the shared early leaf k3=4 holds the C+B
      // groups ahead of A at k3=5), scattering the groups.  Re-key the L.2.2
      // band CPs onto a STABLE (variant_rank, original_key) order: each variant's
      // members keep their relative key order, the groups sort A<B<C.  The band
      // CPs keep their own key SLOTS (the multiset is permuted only among
      // themselves), so non-band CPs are untouched.  CRITICALLY scoped to ovPos
      // L.2.2: the L.1 batch ALSO forms the same A/B/C shapes (combo=1, pos=L.2,
      // pos_len==1, picks ~1961-1963), already WM-aligned; touching those by
      // shape alone regresses firstdiv to 1961.  OFF byte-identical (shares the
      // use_band_interleave gate).  Advances soa firstdiv 1967 -> 1985.
      u32 l22_idx[BAND_CAP];
      u8  l22_var[BAND_CAP];
      u64 l22_key[BAND_CAP];
      u32 n_l22 = 0;
      for (u32 k = 0; k < n_big && n_l22 < BAND_CAP; k++) {
        // ovPos L.2.2: 0-indexed pos path [1, 1] of length 2.
        if (big[k].cp.pos_len != 2u) continue;
        if (big[k].cp.pos[0] != 1u || big[k].cp.pos[1] != 1u) continue;
        Term nl = atp_rewrite_normalize_indexed(s, big[k].cp.lhs, 4096u);
        Term nr = atp_rewrite_normalize_indexed(s, big[k].cp.rhs, 4096u);
        u8 var = atp_pair_band_l22_variant(nl, nr);
        if (var == 0u) continue;
        l22_idx[n_l22] = k;
        l22_var[n_l22] = var;
        l22_key[n_l22] = big[k].key;
        n_l22++;
      }
      if (n_l22 >= 2u) {
        // Sort the band entries by current key (ascending) -- so the assignment
        // permutes only the key MULTISET among themselves and preserves the
        // within-variant relative order under the stable variant-rank sort.
        for (u32 a = 0; a + 1u < n_l22; a++) {
          for (u32 b = a + 1u; b < n_l22; b++) {
            if (l22_key[b] < l22_key[a]) {
              u64 tk = l22_key[a]; l22_key[a] = l22_key[b]; l22_key[b] = tk;
              u32 ti = l22_idx[a]; l22_idx[a] = l22_idx[b]; l22_idx[b] = ti;
              u8  tv = l22_var[a]; l22_var[a] = l22_var[b]; l22_var[b] = tv;
            }
          }
        }
        // order2 = entry permutation grouped by variant rank A(1)<B(2)<C(3),
        // each group keeping its key-ascending relative order (STABLE: the
        // l22_* arrays are already key-ascending, so emitting variant 1 then 2
        // then 3 in array order preserves the within-variant sequence).
        u32 order2[BAND_CAP];
        u32 n_ord = 0;
        for (u8 v = 1u; v <= 3u; v++) {
          for (u32 a = 0; a < n_l22; a++) {
            if (l22_var[a] == v) order2[n_ord++] = a;
          }
        }
        // Assign the ascending key slots to the band CPs in grouped order.
        for (u32 a = 0; a < n_l22; a++) {
          big[l22_idx[order2[a]]].key = l22_key[a];
        }
      }
    }
    // Shared-reverse-face double-MGU defer (default OFF; see use_mered_dmgu).
    // The Meredith OrAssociativity firstdiv-809 divergence: a weight-120 tops-A
    // equation-tree band at pos L.2.2 holds two equations (E6 `a.(a.b)=a.(b.b)`,
    // E7 `a.(b.a)=a.(b.b)`) that WM stores oriented at DISTINCT distinguished-face
    // leaves but which SHARE one reverse face `a.(b.b)`.  thvm overlaps the new
    // fact's subterm onto that shared reverse face, so BOTH equations' combo=0
    // overlaps land at the same discrimination-tree leaf (k3=arr==0): one
    // normalizes to a tautology (dropped at push), the other to the surviving
    // content `(x.y).x = (y.y).x`.  thvm keys the surviving combo=0 CP at the
    // band HEAD; WM never scans the shared reverse face -- its single
    // distinguished-face scan surfaces E6's overlap as TWO MGUs aged far apart
    // (the first at the band's natural slot, the second at CPNr 2832, just before
    // the band's final weight-120 CP E9), so the surviving content belongs at
    // that LATE second-MGU slot, not the head.  Defer it to the band's
    // penultimate same-weight slot (the largest same-weight same-(phase|k1|k2)
    // key below the band max, +1 -- the use_comm_reage anchor+1 splice), so the
    // combo=1 CPs lead the band.  Scoped HARD to phase A (key>>58==0), equation
    // tree (k2==1), pos L.2.2, arr==0, combo==0, a NON-tautology normal form, AND
    // a sibling combo=0 arr==0 L.2.2 overlap from a DIFFERENT equation at the same
    // shared leaf (the tautology twin); a lone shared-leaf overlap or any
    // non-L.2.2 band is left untouched.  OFF byte-identical; advances Meredith
    // firstdiv 809 -> 1040, soa firstdiv 2808 unchanged.
    if (s->use_mered_dmgu) {
      const u64 grp_mask = ~((1ull << 42) - 1ull);   // phase | k1 | k2 bits
      for (u32 kd = 0; kd < n_big; kd++) {
        if ((big[kd].key >> 58) != 0u) continue;          // A phase only
        if (((big[kd].key >> 42) & 3u) != 1u) continue;   // equation tree k2==1
        if (((big[kd].key >> 28) & 0x3fffu) != 0u) continue;  // arr==0
        if (big[kd].combo != 0u) continue;
        if (big[kd].cp.pos_len != 2u ||
            big[kd].cp.pos[0] != 1u || big[kd].cp.pos[1] != 1u) continue;
        // The deferred CP must be a NON-TRIVIAL equation: a shared-reverse-face
        // overlap that normalizes to a tautology is discarded at push (it never
        // claims a FIFO slot), so only the surviving content `(x.y).x = (y.y).x`
        // -- the equation WM ages as a LATE second MGU -- is re-aged.  Compare
        // the joined/reduced sides (atp_rewrite_normalize_indexed, as the other
        // re-key gates do); skip a tautology.
        Term ndl = atp_rewrite_normalize_indexed(s, big[kd].cp.lhs, 4096u);
        Term ndr = atp_rewrite_normalize_indexed(s, big[kd].cp.rhs, 4096u);
        if (kbo_eq(ndl, ndr)) continue;                       // tautology: dropped
        // Require a SIBLING combo=0 arr==0 L.2.2 CP from a DIFFERENT equation at
        // the same shared reverse-face leaf (the second equation E7 whose own
        // overlap is the tautology twin).  This shared-leaf multiplicity is what
        // WM resolves by scanning a single distinguished face and surfacing the
        // overlap as TWO MGUs aged far apart -- so the surviving content must
        // defer.  A lone combo=0 shared-leaf overlap (no twin equation) keeps its
        // head slot.
        //
        // The twin must be the SAME equation WM derives as the second MGU --
        // either the DROPPED tautology (E7's own overlap reduces to `x=x`) or a
        // copy alpha-equal to this CP's surviving content (a genuine repeated-
        // variable double MGU, f=59/f=61: both faces reduce to the same
        // `x.(y.x) = x.(y.(x.x))`).  A twin that survives to DIFFERENT,
        // alpha-distinct content is NOT a shared-MGU partner: the two overlaps
        // are genuinely independent CPs WM emits in their natural arrival order
        // (Meredith rule-48 f=39: j=18 `...(x.x) = ...` and j=25 `...(y.x) = ...`
        // both survive, distinct -- WM picks j=25's ch=0 leaf before j=18's ch=1,
        // its raw co-rank order).  Deferring a survivor whose twin is itself a
        // survivor would re-key BOTH into the band and REVERSE their raw order.
        // Gate on the twin being a tautology OR alpha-equal to kd's content.
        u8 has_twin = 0u;
        for (u32 ks = 0; ks < n_big && !has_twin; ks++) {
          if (ks == kd) continue;
          if (big[ks].j == big[kd].j) continue;             // different equation
          if ((big[ks].key >> 58) != 0u) continue;
          if (((big[ks].key >> 42) & 3u) != 1u) continue;
          if (((big[ks].key >> 28) & 0x3fffu) != 0u) continue;   // arr==0
          if (big[ks].combo != 0u) continue;
          if (big[ks].cp.pos_len != 2u ||
              big[ks].cp.pos[0] != 1u || big[ks].cp.pos[1] != 1u) continue;
          Term tsl = atp_rewrite_normalize_indexed(s, big[ks].cp.lhs, 4096u);
          Term tsr = atp_rewrite_normalize_indexed(s, big[ks].cp.rhs, 4096u);
          if (kbo_eq(tsl, tsr)                                 // dropped tautology
              || atp_pair_alpha_eq(tsl, tsr, ndl, ndr)) {     // same MGU content
            has_twin = 1u;
          }
        }
        if (!has_twin) continue;
        // Re-age to just before the band's LAST same-weight CP (WM ages E6's
        // second MGU at CPNr 2832, immediately before the band's final weight-120
        // CP E9 at 2833).  Anchor on same-(phase|k1|k2) CPs of the SAME priority
        // as the deferred CP -- the higher-arrival (k3>5) CPs in this group are
        // heavier (they introduce extra overlap variables) and are NOT part of
        // the weight-120 band, so they must not set the target.  The deferred
        // CP's key becomes (largest same-weight band key, +1), landing it as the
        // band's penultimate entry just before that final CP.
        // Find the two largest same-weight band keys in one pass: band_max is
        // the final CP (E9); band_pen the one before it.  The deferred CP splices
        // just before E9 -> band_pen + 1.
        u32 d_pri = atp_cp_priority(s, ndl, ndr);
        u64 grp = big[kd].key & grp_mask;
        u64 band_max = 0u, band_pen = 0u;
        for (u32 ks = 0; ks < n_big; ks++) {
          if (ks == kd) continue;
          if ((big[ks].key & grp_mask) != grp) continue;
          Term nsl = atp_rewrite_normalize_indexed(s, big[ks].cp.lhs, 4096u);
          Term nsr = atp_rewrite_normalize_indexed(s, big[ks].cp.rhs, 4096u);
          if (kbo_eq(nsl, nsr)) continue;                  // tautology: dropped
          if (atp_cp_priority(s, nsl, nsr) != d_pri) continue;  // same weight only
          if (big[ks].key > band_max) { band_pen = band_max; band_max = big[ks].key; }
          else if (big[ks].key > band_pen) band_pen = big[ks].key;
        }
        // Splice just before the band's final same-weight CP (E9): the largest
        // same-weight key below the band max, +1.  This places the deferred CP at
        // the band's penultimate slot, matching WM's CPNr 2832 < 2833 (E9).
        if (band_pen != 0u) big[kd].key = band_pen + 1u;
      }
    }
    // Reverse-face cube emission order (default OFF; see use_revface_cubeorder).
    // The soa firstdiv-2540 divergence: a tops batch forms the FORWARD cube
    // `x.(y.(y.y)) = x.x` (atp_pair_is_fwd_cube) by overlapping the new fact
    // onto an asymmetric var-differ partner equation's REVERSE face.  WM stores
    // that partner ORIENTED; its single discrimination-tree scan reaches the
    // overlap on the partner's EARLY distinguished-face leaf, so WM ages the
    // forward cube there (ahead of the reverse cubes it emits later -- picks
    // 2538-2542 FWD, then 2543+ REV).  thvm's co-rank (atp_wmo_rank, the
    // wmo_eq_sides_var_differ branch) normally pulls the cube onto that early
    // forward leaf too, BUT suppresses it when the cube reproduces a live
    // equation (the soa pick-113 case: `atp_pop_eq_subsumed` zeroes the forward
    // anchor), leaving the cube at its LATE reverse-face arrival.  At rule-60/
    // 64 that suppression is wrong -- WM still co-ranks the cube early -- so it
    // scatters past the reverse cubes (firstdiv 2540).  This gate re-applies
    // the un-suppressed co-rank: it re-keys exactly the cubes the suppression
    // mis-aged (arr_o < cur_arr AND subsumed) onto WM's early forward-leaf slot
    // (arrival arr_o, chain ch_o*2+1, mirroring atp_wmo_rank's anchor branch).
    // Scoped HARD: A phase (key>>58==0), the equation tree (k2 == 1), the exact
    // normalized forward-cube shape, a var-differ partner, and the suppressed-
    // co-rank condition -- never a generic equal-weight reorder.  A cube whose
    // forward face is NOT subsumed-suppressed (its k3 is its genuine arrival,
    // soa rule-37 picks 349-355) is left untouched.  OFF byte-identical.
    if (s->use_revface_cubeorder) {
      for (u32 kb = 0; kb < n_big; kb++) {
        if ((big[kb].key >> 58) != 0u) continue;     // A phase only
        if (((big[kb].key >> 42) & 3u) != 1u) continue; // equation-tree k2 == 1
        // L.2 overlap position only (pos path [1]).  At L.1 (pos[0]==0) the
        // co-rank suppression IS WM-faithful (the new fact's L.1 subterm
        // genuinely reaches the cube on the late leaf), so re-applying the
        // anchor there mis-orders the L.1 cube cluster (soa picks ~965).  Only
        // the L.2 cursor walk surfaces the cube on the early forward leaf.
        if (big[kb].cp.pos_len != 1u || big[kb].cp.pos[0] != 1u) continue;
        Term nlb = atp_rewrite_normalize_indexed(s, big[kb].cp.lhs, 4096u);
        Term nrb = atp_rewrite_normalize_indexed(s, big[kb].cp.rhs, 4096u);
        if (!atp_pair_is_fwd_cube(nlb, nrb)) continue;
        AtpWmOrder *w = (AtpWmOrder *)s->wmo;
        u32 jt = s->r_trace[big[kb].j];
        if (!wmo_eq_sides_var_differ(s->lhs[big[kb].j], s->rhs[big[kb].j]))
          continue;
        u8  j_dr_b = wmo_trace_dist_rhs(w, jt);
        u8  j_face_wm_b = (big[kb].combo & 1u) ^ j_dr_b;
        u8  i_face_b = (big[kb].combo >> 1) & 1u;
        Term qsub_b = i_face_b ? s->rhs[big[kb].i] : s->lhs[big[kb].i];
        for (u32 d = 0; d < big[kb].cp.pos_len; d++) {
          if (term_tag(qsub_b) != TAG_CTR) break;
          qsub_b = term_ctr_at(qsub_b, big[kb].cp.pos[d]);
        }
        u32 cur_arr = 0, cur_ch = 0, arr_o = 0, ch_o = 0;
        if (!wmo_tops_rank(w, 1u, qsub_b, jt, j_face_wm_b, &cur_arr, &cur_ch))
          continue;
        if (!wmo_tops_rank(w, 1u, qsub_b, jt, (u8)(j_face_wm_b ^ 1u),
                           &arr_o, &ch_o))
          continue;
        if (arr_o >= cur_arr) continue;              // forward not earlier
        if (kbo_eq(nlb, nrb) || !atp_pop_eq_subsumed(s, nlb, nrb))
          continue;                                  // not a suppressed cube
        // Re-key the cube to the slot the un-suppressed co-rank would have
        // given it: its forward-leaf arrival arr_o, chain ch_o*2+1 (so it
        // sorts right after the partner's distinguished-face CP, exactly as
        // atp_wmo_rank's `arr = arr_o; ch = ch_o*2+1` branch does).  This
        // places the cube in WM's single-scan leaf order -- not blindly ahead
        // of every reverse cube, so a partial-interleave batch (soa rule-61:
        // reverse cubes both before AND after the forward leaf) keeps its
        // surrounding order.  Replace only the k3|k4|k5 (arrival|chain) bits;
        // phase|k1|k2 stay.
        u32 new_arr = arr_o > 0x3fffu ? 0x3fffu : arr_o;
        u32 new_ch  = (ch_o * 2u + 1u) > 0x3fffu ? 0x3fffu : (ch_o * 2u + 1u);
        big[kb].key = (big[kb].key & ~((1ull << 42) - 1ull)) |
                      ((u64)new_arr << 28) | ((u64)new_ch << 14);
      }
    }
    qsort(big, n_big, sizeof(AtpWmoCpEnt), atp_wmo_ent_cmp);
    // Gated batch-order trace (THVM_ATP_BATCH_TRACE): emit the sorted
    // (outer f, i, j, combo, packed key) in push order so thvm's
    // per-fact CP enumeration order can be diffed against Waldmeister's
    // `critical pair N built with parents X and Y` formation sequence.
    static int batch_trace = -1;
    if (batch_trace < 0) batch_trace = (getenv("THVM_ATP_BATCH_TRACE") != NULL) ? 1 : 0;
    if (batch_trace) {
      // Outer (new) rule identity per batch: its stored lhs/rhs + WM-face,
      // so a thvm batch can be matched to a WM cpform.out vaterNr/vaterL.
      char fla[1024], fra[1024];
      atp_pretty_term(s->lhs[f], fla, sizeof fla);
      atp_pretty_term(s->rhs[f], fra, sizeof fra);
      fprintf(stderr, "BATCHRULE f=%u ftr=%u f_or=%u dist_rhs=%u lhs=%s rhs=%s\n",
              f, s->r_trace[f], s->r_orient[f],
              (unsigned)wmo_trace_dist_rhs((AtpWmOrder *)s->wmo, s->r_trace[f]),
              fla, fra);
    }
    for (u32 k = 0; k < n_big; k++) {
      if (batch_trace) {
        char bla[1024], bra[1024];
        atp_pretty_term(big[k].cp.lhs, bla, sizeof bla);
        atp_pretty_term(big[k].cp.rhs, bra, sizeof bra);
        // pos= the overlap-position child-index path (WM cpform.out ovPos
        // analogue: WM prints 1-indexed L.idx; thvm pos[] is 0-indexed, so
        // L.2 == pos[0]=1). seq= the FIFO age (cp_seq) this CP is about to
        // receive, so a BATCH line aligns to a CPSEL pick by seq.
        char posbuf[64];
        u32 pn = 0;
        posbuf[pn++] = 'L';
        for (u32 d = 0; d < big[k].cp.pos_len && pn < 60u; d++)
          pn += (u32)snprintf(posbuf + pn, sizeof posbuf - pn, ".%u",
                              big[k].cp.pos[d] + 1u);
        posbuf[pn] = '\0';
        fprintf(stderr,
                "BATCH f=%u i=%u j=%u itr=%u jtr=%u i_or=%u j_or=%u combo=%u "
                "poslen=%u pos=%s seq=%u key=%llu cp=%s # %s\n",
                f, big[k].i, big[k].j, s->r_trace[big[k].i], s->r_trace[big[k].j],
                s->r_orient[big[k].i], s->r_orient[big[k].j],
                big[k].combo, big[k].cp.pos_len, posbuf, s->cp_seq_next,
                (unsigned long long)big[k].key, bla, bra);
      }
      pushed += atp_push_cps_traced(s, &big[k].cp, 1u,
                                    s->r_trace[big[k].i],
                                    s->r_trace[big[k].j],
                                    big[k].i, big[k].j);
    }
    free(big);
    // Mark f overlap-exhausted: its full superposition lane is now
    // enumerated, so a later-added fact will not re-superpose against it
    // (WM forms a fact's lane once; default-OFF gate).
    if (s->use_overlap_exhaust && !s->r_orient[f]) s->r_overlap_done[f] = 1u;
    s->n_wmo_rank_misses = ((AtpWmOrder *)s->wmo)->rank_misses;
    if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);
  }
  return pushed;
}

static u32 thvm_atp_generate_cps_c(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  if (s->use_emission_order && s->wmo != NULL) {
    return thvm_atp_generate_cps_wm(s, added);
  }

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;

#ifdef ATP_RULE_INDEX
  // Indexed overlap-partner retrieval (opt-in, byte-identical CP set).
  // Rebuild both overlap indices over the post-add rule set, then for
  // each new rule overlap only the candidate partners the index returns.
  // On a candidate-buffer / subject-depth OVERFLOW the call falls back to
  // the exact n_rules scan for that rule, so the CP set is preserved even
  // when the index over-runs its scratch.
  if (s->use_cp_index) {
    if (s->cp_index == NULL)    s->cp_index    = atp_ri_new();
    if (s->cp_subindex == NULL) s->cp_subindex = atp_ri_new();
    // Stale iff EITHER the rule count or the rule-set revision moved:
    // an interreduce drop+add cycle keeps n_rules constant while the
    // compaction renumbers slots, so an n_rules-only check leaves leaf
    // recs naming the wrong live rules and silently loses CPs.
    if (s->cp_index->n_rules_built != n ||
        s->cp_index->built_revision != s->r_revision) {
      atp_cp_index_rebuild(s);          // grows g_atp_cp_seen to cover n
      atp_cp_subindex_rebuild(s);
    }
    // (new x all_R): for each new rule i, candidates j are rules whose
    // stored face unifies with a non-var subterm of one of i's faces --
    // the whole-face index query (both faces, for the unfailing overlap).
    for (u32 i = first; i < last; i++) {
      u32 nc = atp_cp_index_collect(s, i);
      if (g_atp_cp_overflow) {
        for (u32 j = 0; j < n; j++) pushed += atp_gen_one(s, i, j, buf);
      } else {
        atp_cp_cand_sort();
        for (u32 c = 0; c < nc; c++) {
          pushed += atp_gen_one(s, i, g_atp_cp_cand[c], buf);
        }
      }
    }
    // (old x new): for each new rule j, candidates i are OLD rules whose
    // stored face has a non-var subterm unifiable with one of j's faces --
    // the subterm index query (both faces).  Only i < first (old) are this
    // loop's partners.
    for (u32 j = first; j < last; j++) {
      u32 nc = atp_cp_subindex_collect(s, j);
      if (g_atp_cp_overflow) {
        for (u32 i = 0; i < first; i++) pushed += atp_gen_one(s, i, j, buf);
      } else {
        atp_cp_cand_sort();
        for (u32 c = 0; c < nc; c++) {
          u32 i = g_atp_cp_cand[c];
          if (i < first) pushed += atp_gen_one(s, i, j, buf);
        }
      }
    }
    return pushed;
  }
#endif

  // (new x all_R): the new rule is i (outer), j ranges over all
  // existing rules (including the new ones for new x new self-overlap).
  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      pushed += atp_gen_one(s, i, j, buf);
    }
  }

  // (old x new): old rule on the outside, new rule fed as inner.
  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      pushed += atp_gen_one(s, i, j, buf);
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
  u8            skip_root;   // 1 = PROPER positions only (WM eTT)
} CpCtxIc;

static u32 cp_visit_ic(const u32 *p, u32 p_len, void *raw) {
  CpCtxIc *ctx = (CpCtxIc *)raw;
  if (ctx->skip_root && p_len == 0u) return ctx->count;
  if (ctx->count >= ctx->cap) {
    g_cp_dropped_capped++;
    return ctx->count;
  }

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
  Term cp_peak = ic_unify_apply3(sub, ctx->lj, ctx->li);
  if (term_tag(cp_peak) == TAG_ERA) return ctx->count;

  CriticalPair *slot = &ctx->out[ctx->count];
  slot->lhs = cp_lhs;
  slot->rhs = cp_rhs;
  slot->peak = cp_peak;
  slot->pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len; d++) slot->pos[d] = (u8)p[d];
  slot->combo = 0xffu;  // unknown until the WM-order batch tags it
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
      // Root-overlap ownership: same discipline as atp_overlap_ij
      // (the i > j call owns the pair's root; a rule never forms a
      // root self-overlap).  The IC lane has no unfailing face
      // combos, so the i == j equation case is combo 1 (phase F).
      ctx.skip_root = (i < j) ||
                      (i == j &&
                       !(s->use_unfailing_cp && !s->r_orient[i]));
      (void)cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                              cp_visit_ic, &ctx, 0);
      ctx.count = atp_cp_gen_gates(s, buf, 0, ctx.count,
                                   s->use_unfailing_cp && !s->r_orient[i],
                                   s->use_unfailing_cp && !s->r_orient[j]);
      pushed += atp_push_cps_traced(s, buf, ctx.count,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
      // Bound the per-step transient: a step can out-allocate a GC
      // semi-space in raw-CP + normalisation scratch.  `buf` is fully
      // processed here, so a collection between overlap pairs is safe.
      if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);
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
      ctx.skip_root = 1u;   // i < j: roots belong to the (j, i) visit
      (void)cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                              cp_visit_ic, &ctx, 0);
      ctx.count = atp_cp_gen_gates(s, buf, 0, ctx.count,
                                   s->use_unfailing_cp && !s->r_orient[i],
                                   s->use_unfailing_cp && !s->r_orient[j]);
      pushed += atp_push_cps_traced(s, buf, ctx.count,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
      if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);   // see above
    }
  }

  return pushed;
}

fn u32 thvm_atp_generate_cps(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0) return 0;
  if (s->use_ic_cp_gen) return thvm_atp_generate_cps_ic(s, added);
  return thvm_atp_generate_cps_c(s, added);
}

#ifdef ATP_ORDERED_REWRITE
// Walk `t` and substitute `min_const` for every TAG_FVR cell whose
// var-id does not occur in `lhs`.  Port of the variable-binding loop
// in Waldmeister's `PCL_FreieVarInstanz` (PCL/pcl.c:458-472): binds
// every "free" RHS variable to `SO_minimaleKonstante` before the rule
// is grounded.  Returns `t` unchanged when no substitution fires.
//
// Used by `thvm_atp_orient_and_add` to emit a grounded sibling for a
// KBO_UN equation whose RHS introduces variables not on the LHS.  The
// ordered-rewrite gate at the call site requires the result to be
// KBO_GT against `lhs`, so a no-op substitution (the result equals the
// original unorientable RHS) drops cleanly with no extra rule push.
static Term atp_grounded_instance(AtpState *s, Term t, Term lhs,
                                  Term min_const) {
  switch (term_tag(t)) {
    case TAG_FVR:
      // Variable: ground it iff it does not occur on the LHS.  The
      // not-on-LHS guard mirrors PCL_FreieVarInstanz's "frei in r"
      // condition -- a bound variable is left alone, so the resulting
      // rule still unifies with redexes that previously matched lhs.
      return atp_term_has_var(lhs, term_ext(t)) ? t : min_const;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n == 0) return t;
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term ch = term_ctr_at(t, i);
        Term sub = atp_grounded_instance(s, ch, lhs, min_const);
        children[i] = sub;
        if (sub != ch) changed = 1;
      }
      if (!changed) return t;
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// One leg of the Waldmeister FVI two-direction wrapper.  Grounds the
// vars of `rhs` that do NOT occur in `lhs` against `s->min_const`,
// gates the resulting pair on KBO_GT(lhs, g_rhs), and pushes it as a
// regular rule when accepted.  On success bumps both `r->count` and
// `r->fvi_count` (the trailing-N FVI sibling-count consumed by the
// trace stamp loop at `thvm_atp_step`).
//
// Returns 1 when a new rule was pushed, 0 otherwise.
static u8 atp_try_ground_and_push(AtpState *s, Term lhs, Term rhs,
                                  AtpAddedRange *r) {
  Term g_rhs = atp_grounded_instance(s, rhs, lhs, s->min_const);
  if (g_rhs == rhs) return 0;
  if (atp_compare(s, lhs, g_rhs) != KBO_GT) return 0;
  if (!atp_push_rule(s, lhs, g_rhs)) return 0;
  r->count++;
  r->fvi_count++;
  return 1;
}

// Waldmeister's `RechtsUnfreiErzeugen` is called on BOTH the principal
// direction (`linkeSeite -> rechteSeite`) AND the reversed direction
// (the `Antigleichung`, `rechteSeite -> linkeSeite`); see
// `RUndEVerwaltung.c:425, 430, 439, 445`.  thvm has no separate
// reversed-direction slot, so we emulate by trying both legs here.
// Each leg is gated independently: the grounded RHS must (a) actually
// change (some var present on the dropped side is absent from the kept
// side) and (b) remain KBO_GT under the kept side after grounding.
//
// May push 0, 1, or 2 sibling rules into the rule array; bumps
// `r->count` and `r->fvi_count` accordingly.  The trace-stamp loop at
// `thvm_atp_step` reads the final `fvi_count` to label the trailing N
// slots as TRACE_FVI.
static void atp_emit_fvi_pair(AtpState *s, Term lhs, Term rhs,
                              AtpAddedRange *r) {
  // Principal direction: ground free vars of rhs absent from lhs,
  // emit (lhs -> g_rhs).
  atp_try_ground_and_push(s, lhs, rhs, r);
  // Reversed direction (Antigleichung): ground free vars of lhs absent
  // from rhs, emit (rhs -> g_lhs).
  atp_try_ground_and_push(s, rhs, lhs, r);
}
#endif /* ATP_ORDERED_REWRITE */

// Waldmeister LRSortieren side-canonicalisation (SpezNormierung.c
// :517-534 LRSortierenRek).  WM stores every E-set equation (oriented OR
// unorientable) with a canonical left/right side order: treat variables
// as jokers (variable < non-variable), compare functors by the intake
// SymbolVergleich precedence (degenerate = Gleich on a single-operator
// signature), recurse argument-wise left-to-right; on Groesser (lhs >
// rhs structurally) the sides are swapped so the structurally-smaller
// side becomes the LHS.  A derived unorientable CP enters with whichever
// face the superposition geometry produced as its lhs; without this
// canonicalisation thvm and WM can store the SAME equation under opposite
// side orders (e.g. the Sheffer C-shape `(x*(x*x))*y=y*y` vs WM's
// `x*x=(y*(y*y))*x`), and the differing stored term STRUCTURE makes the
// equation's own CP batch overlap a different set of redexes -> a
// selection-order fork downstream.  Returns 1 if the sides should swap.
//   <0 (Kleiner): lhs structurally smaller -> keep
//    0 (Gleich):  indistinguishable -> keep
//   >0 (Groesser): lhs structurally larger -> swap
static int atp_lr_sortieren_rec(Term lhs, Term rhs) {
  u8 lv = (term_tag(lhs) == TAG_FVR);
  u8 rv = (term_tag(rhs) == TAG_FVR);
  if (lv && rv) return 0;          // both variables -> Gleich
  if (lv) return -1;               // variable < non-variable -> Kleiner
  if (rv) return 1;                // non-variable > variable -> Groesser
  // Both constructors.  Single-operator Sheffer signature: SymbolVergleich
  // is Gleich for equal functors; differing functors fall back to the
  // label code (a stable total order mirroring the precedence index).
  u32 lf = term_ext(lhs), rf = term_ext(rhs);
  if (lf != rf) return (lf < rf) ? -1 : 1;
  u32 ln = term_ctr_n(lhs), rn = term_ctr_n(rhs);
  if (ln != rn) return (ln < rn) ? -1 : 1;
  for (u32 i = 0; i < ln; i++) {
    int c = atp_lr_sortieren_rec(term_ctr_at(lhs, i), term_ctr_at(rhs, i));
    if (c != 0) return c;
  }
  return 0;
}

// Orient via KBO and push the rule(s).  See header comment for the
// dispatch table.  Atomic: if the unfailing fallback can't fit both
// orientations, neither is added.
fn AtpAddedRange thvm_atp_orient_and_add(AtpState *s, Term lhs, Term rhs) {
  AtpAddedRange r = {0, 0, 0};
  if (s == NULL) return r;

  // Permutation-subsumption: drop AC-equal-at-top equations before
  // they become rules.  This catches the post-normalize simple-
  // commutativity form (e.g. nand(x_0, x_1) = nand(x_1, x_0)) that the
  // push-time filter missed because the CP entered the queue in a
  // larger, non-canonical shape and reduced to commutativity only at
  // pop-time normalize.  AC-mask WM-faithful path handles this at the
  // KPV_Select pop site (the orient call comes AFTER that drop), so it
  // stays off here; the legacy binary env path keeps the orient drop.
  if (s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull
      && atp_cp_perm_subsumed(lhs, rhs)) {
    s->n_cps_dropped_perm_subsumed++;
    return r;
  }

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
#ifdef ATP_ORDERED_REWRITE
      // 9c-foundation: ordered rewriting drives an unorientable
      // equation in whichever direction decreases, so store it ONCE
      // (no looping u->v / v->u pair, no doubled CP generation).  The
      // stored lhs/rhs orientation is left as the popped CP produced it
      // -- the WM emission mirror records the WM-distinguished-face flip
      // per-trace instead (atp_wmo_insert_fact_ex), keeping the CP set
      // and the formation-time KPAction order gate untouched.
      //
      // Waldmeister LRSortieren (SpezNormierung.c:517-534): WM stores the
      // equation with the structurally-smaller side as LHS (variable <
      // non-variable, preorder).  thvm's popped CP can carry the opposite
      // side order; that differing stored structure makes the equation's
      // OWN CP batch overlap a different set of redexes, forking the
      // selection sequence.  Canonicalise to WM's side order so the
      // stored E-member -- and its batch -- match.
      if (s->use_lr_sortieren && atp_lr_sortieren_rec(lhs, rhs) > 0) {
        Term tmp = lhs; lhs = rhs; rhs = tmp;
      }
      // Waldmeister CP-formation side geometry (Unifikation1.c:916-917):
      // WM stores the unorientable equation with KPLinks =
      // sigma(TP_RechteSeite(Vater)) = sigma(r_i) as its LEFT side and
      // KPRechts = sigma(l_i[p<-r_j]) (the reduct) as its RIGHT.  When
      // thvm's popped CP carries the OPPOSITE order (cp_visit lands the
      // reduct on lhs, sigma(r_i) on rhs) its own batch overlaps a
      // different redex set -- the Sheffer OrAssociativity C-shape forks
      // here (thvm long-side-left vs WM short-side-left).  Reorder to WM's
      // geometry so the stored LHS becomes thvm's cp_rhs.
      //
      // PARENT-OVERLAP-AWARE (atp_cp_wm_side_swaps): NOT a blanket reorder
      // of every KBO_UN equation.  Which face WM stores sigma(r_Vater)-left
      // depends on the overlap geometry; the CP's face-combo index (tagged
      // at formation, carried on the TRACE_CP record) selects it.  Only the
      // reversed-i-face combos (2/3 in atp_overlap_ij, where the overlap
      // produced thvm's reduct-on-lhs vs WM's sigma(r)-on-lhs) need the
      // swap; combos 0/1 already match WM's stored geometry, so swapping
      // them mis-times the equation's OWN batch -- that mistiming was the
      // CombinatorAxioms__BCKWToSKI__c2 regression (a blanket reorder
      // mis-timed one downstream w1=41 cCombinatorI CP by one selection:
      // picks 55-57 transiently 119,119,41 vs 119,41,119, content-
      // identical, re-converging at pick 58).  An INITIAL AXIOM (untagged)
      // has no superposition Vater -- WM stores it by LRSortieren, so
      // atp_cp_wm_side_swaps canonicalises an axiom with LRSortieren rather
      // than the CP-formation swap (reproducing the soa Sheffer axiom
      // order).  The combo gate keeps the soa OrAssociativity prefix at 124
      // AND, when on, keeps the other broad theorems byte-identical (it no
      // longer forks McCune as the old blanket reorder did).  BCKWToSKI__c2
      // has an irreducible axiom-orientation tension (it needs the same
      // unorientable axiom NOT swapped, see atp_cp_wm_side_swaps), so the
      // bench DEFAULTS the swap OFF to keep that baseline clean; soa is
      // measured opt-in.  The emission mirror (atp_wmo_eq_dist_rhs) reads
      // last_cp_wm_side_swapped so the WM distinguished-face flag matches
      // the physical orientation even for the axiom case.  DEFAULT OFF
      // (THVM_ATP_CP_SIDE opts in).
      s->last_cp_wm_side_swapped =
          atp_cp_wm_side_swaps(s, s->last_popped_trace, lhs, rhs);
      if (s->last_cp_wm_side_swapped) {
        Term tmp = lhs; lhs = rhs; rhs = tmp;
      }
      u32 idx = s->n_rules;
      u8 pushed = atp_push_rule(s, lhs, rhs);
      if (pushed) { r.first = idx; r.count = 1; }
      // WM E-set subsumption (GMSubsummierenMitGleichung,
      // INF/Interreduktion.c:251-274): the new unorientable equation
      // destroys every existing E-member it subsumes -- no requeue,
      // no CP made.  Runs before the FVI hook, matching WM's order
      // (IR_InterreduktionLinks precedes RE_FaktumEinfuegen, whose
      // insertion path hosts RechtsUnfreiErzeugen).
      if (pushed && s->use_eset_subsume) atp_eset_subsume_by_new(s, idx);
      // Waldmeister `RechtsUnfreiErzeugen` (RUndEVerwaltung.c:366-397):
      // an unorientable equation whose RHS has variables not on its
      // LHS gates ExcludedMiddle / Noncontradiction / EqualityOfInverses
      // -- the rewriter cannot orient it, but the GROUNDED instance
      // (replace each such free RHS variable with `SO_minimaleKonstante`)
      // is KBO-decidable and unblocks the proof.  WM fires the helper on
      // BOTH the principal and reversed (`Antigleichung`) directions
      // (RUndEVerwaltung.c:425, 430, 439, 445); the two-direction
      // wrapper `atp_emit_fvi_pair` emulates this since thvm has no
      // per-equation reversed slot.  Each direction is gated on
      // (a) the grounding actually changing the term and (b) KBO_GT
      // after grounding.  Emitted rules are tagged TRACE_FVI by the
      // step trace loop (using `added.fvi_count`).
      // Gated by AtpState.use_fvi (Method "FreeVarInstance" -> True on
      // the WL side, or THVM_ATP_FVI env on the C side).  Default OFF
      // for byte-identical pre-port behavior on the OK_OK baseline;
      // turning it on unblocks the FVI-gated theorems (ExcludedMiddle,
      // Noncontradiction, EqualityOfInverses) at the cost of a slight
      // trajectory shift on other Booleans.
      if (pushed && s->use_fvi) atp_emit_fvi_pair(s, lhs, rhs, &r);
      return r;
#else
      // Unfailing fallback: reserve 2 slots up front so the pair is
      // added atomically (the array is growable, so this can't fail).
      // 7c: atp_push_rule may reject either orientation as a
      // duplicate -- r must span exactly the rules actually stored,
      // so generate_cps overlaps only the freshly-added range.
      atp_ensure_rule_cap(s, s->n_rules + 2);
      u32 idx = s->n_rules;
      u32 added = 0;
      added += atp_push_rule(s, lhs, rhs) ? 1u : 0u;
      added += atp_push_rule(s, rhs, lhs) ? 1u : 0u;
      r.first = idx;
      r.count = added;
      // WM E-set subsumption (GMSubsummierenMitGleichung): scan only
      // slots below the freshly-added pair -- the matcher tries both
      // pattern orientations, so one sweep off the first slot covers
      // the looping u->v / v->u twins (and kills BOTH slots of an
      // older subsumed pair, WM's FinaleKillprozSubsumption twin
      // destruction).
      if (added > 0 && s->use_eset_subsume) atp_eset_subsume_by_new(s, idx);
      return r;
#endif
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
    atp_cp_slot_read(s, ent[i].idx, &out_lhs[i], &out_rhs[i]);
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
