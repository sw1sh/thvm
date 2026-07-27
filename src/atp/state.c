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

