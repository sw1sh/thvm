// Proof-trace soft cap (entries).  Resolved once from THVM_ATP_TRACE_MAX,
// falling back to ATP_MAX_TRACE so an unset env keeps the historical drop
// point.  A bad/zero/oversize value falls back to the default.  The trace
// buffer grows on demand (atp_trace_ensure) until this cap is reached.
static u32 g_atp_trace_cap_req = 0;
// In-process request (see thvm_heap_request_perm_cells): takes effect
// before the first engine init; an explicit THVM_ATP_TRACE_MAX wins.
fn void thvm_atp_request_trace_cap(u32 cap) { g_atp_trace_cap_req = cap; }

fn u32 thvm_atp_trace_cap(void) {
  static u32 cap = 0;
  if (cap == 0) {
    cap = ATP_MAX_TRACE;
    if (g_atp_trace_cap_req >= ATP_MAX_TRACE &&
        g_atp_trace_cap_req <= (1u << 28))
      cap = g_atp_trace_cap_req;
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
  atp_hx_open(s);   // THVM_ATP_HISTORY_EXPORT (observation-only)
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
  thvm_atp_history_finish(s, 0xffu);   // no-op unless still recording
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
  free(s->rj);
  free(s->rj_x);
  if (g_rj_sh.owner == s) g_rj_sh.owner = NULL;
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
  // Simpler-join pop-peak and raw-overlap stashes: same per-slot
  // ownership as cp_packed.
  if (s->cp_peak != NULL) {
    for (u32 i = 0; i < s->cp_cap; i++) free(s->cp_peak[i]);
    free(s->cp_peak);
  }
  if (s->cp_raw != NULL) {
    for (u32 i = 0; i < s->cp_cap; i++) free(s->cp_raw[i]);
    free(s->cp_raw);
  }
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // A leftover pop-decoded peak span lived in this state's Arena A
  // (already torn down wholesale above) -- drop the handle, never free.
  g_pop_sj_peak       = NULL;
  g_pop_sj_peak_owner = NULL;
#endif
  atp_sj_peak_pending_drop();
  atp_cp_raw_pending_drop();
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
  free(s->otter_rows);
  free(s->otter_seqs);
  free(s->uid_passive);
  free(s->otter_map_key);
  free(s->otter_map_uid);
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
  free(s->irv_depth);
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
  thvm_atp_nrec_free(s);
  atp_goal_drive_free(s->gdrive);
  free(s);
}

