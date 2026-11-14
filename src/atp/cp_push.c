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
// TEMPORARY uid-keyed CP-formation probe (THVM_CP_TRACE_UIDA/UIDB, default
// OFF): logs, on BOTH formers (batch + single-walk), every formation attempt
// and every gate decision for the targeted rule PAIR, keyed by the stable
// r_uid identity (slot indexes are unstable across picks).  Pure stderr
// diagnostics; zero engine effect when the envs are unset (one cached probe).
static u32 g_cptr_uida = 0, g_cptr_uidb = 0;
static u8  g_cptr_active = 0u;   // armed while a matched pair is being formed
static u8 atp_cptr_enabled(void) {
  static int on = -1;
  if (on < 0) {
    const char *a = getenv("THVM_CP_TRACE_UIDA");
    const char *b = getenv("THVM_CP_TRACE_UIDB");
    g_cptr_uida = a ? (u32)strtoul(a, NULL, 10) : 0u;
    g_cptr_uidb = b ? (u32)strtoul(b, NULL, 10) : 0u;
    on = (g_cptr_uida != 0u && g_cptr_uidb != 0u) ? 1 : 0;
  }
  return (u8)on;
}
static u8 atp_cptr_pair(const AtpState *s, u32 i, u32 j) {
  if (!atp_cptr_enabled() || s->r_uid == NULL) return 0u;
  if (i >= s->n_rules || j >= s->n_rules) return 0u;
  u32 a = s->r_uid[i], b = s->r_uid[j];
  return ((a == g_cptr_uida && b == g_cptr_uidb) ||
          (a == g_cptr_uidb && b == g_cptr_uida)) ? 1u : 0u;
}
static void atp_cptr_pos(const u8 *pos, u8 len, char *out, u32 cap) {
  u32 pn = 0;
  out[pn++] = 'L';
  for (u32 d = 0; d < len && pn + 8u < cap; d++)
    pn += (u32)snprintf(out + pn, cap - pn, ".%u", pos[d] + 1u);
  out[pn] = '\0';
}
static void atp_cptr_cp(const char *tag, const CriticalPair *cp) {
  char posb[64];
  atp_cptr_pos(cp->pos, cp->pos_len, posb, sizeof posb);
  fprintf(stderr, "CPTR %s pos=%s lhs=", tag, posb);
  atp_dbg_print_term(stderr, cp->lhs);
  fprintf(stderr, " rhs=");
  atp_dbg_print_term(stderr, cp->rhs);
  if (cp->peak != 0) {
    fprintf(stderr, " peak=");
    atp_dbg_print_term(stderr, cp->peak);
  }
  fputc('\n', stderr);
}

// May the CP push loop take the fused single-pass pipeline
// (acp_pack_fused: varnorm + pack + weigh in one walk)?  Requires that
// NOTHING downstream consumes the renumbered pair as Term cells: every
// push-side filter off, the weigh on the plain-KBO route, no
// goal/sos/w2/learned weighing, no implicit-CP descriptor path, and no
// Term-printing debug env.  The env/compile legs are cached; the
// AtpState legs are re-checked per push batch (cheap flag reads).
static u8 atp_cp_push_fuse_ok(const AtpState *s) {
#if !defined(ATP_VAR_NORM) || defined(ATP_CP_DIAG)
  // Without the renumber the classic pack IS the raw pack; the DIAG
  // build consumes the renumbered Terms unconditionally.  (The CPQ
  // build used to sit in this disable too -- which silently kept the
  // certified FT-direct formation arm OFF under THVM_ATPFT_CPQ,
  // running the classic trajectory instead; its slot mirror now
  // decodes routed pushes from the authoritative packed bytes, so the
  // fused route is CPQ-safe.)
  (void)s;
  return 0u;
#else
  static int env_ok = -1;
  if (env_ok < 0) {
    env_ok = !atp_env_on("THVM_ATP_NO_FUSE_PACK")
          && getenv("THVM_ATP_CP_FORM_TRACE")  == NULL
          && getenv("THVM_ATP_W367_TRACE")     == NULL
          && getenv("THVM_ATP_CPGEN_DEBUG")    == NULL
          && getenv("THVM_ATP_CPRAW_DEBUG")    == NULL
          && getenv("THVM_ATP_KBO_NOMEMO_MAX") == NULL;
  }
  if (!env_ok) return 0u;
  if (s->kbo == NULL || s->wpo != NULL || s->rpo != NULL || s->lpo != NULL) {
    return 0u;
  }
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) return 0u;
#endif
  u32 mode = s->cp_weight_mode;
  if (mode != ATP_CP_WEIGHT_ADD && mode != ATP_CP_WEIGHT_MAX &&
      mode != ATP_CP_WEIGHT_GT  && mode != ATP_CP_WEIGHT_MIX &&
      mode != ATP_CP_WEIGHT_MIX2) {
    // Twee-score mode rides the fused pipeline too: the treated class
    // consumes ftp_pack_fused's stashed per-side scores and the raw
    // >= 50 class keeps its classic Term build (the emit arms decline
    // it -- see sw_form_push), scored via atp_cp_twee_fused_base.
    // THVM_ATP_TWEE_FUSE=0 is the same-binary kill switch back to the
    // classic (unfused) mode-8 pipeline.
    static int twee_fuse = -1;
    if (twee_fuse < 0) {
      const char *e = getenv("THVM_ATP_TWEE_FUSE");
      twee_fuse = (e != NULL && e[0] == '0') ? 0 : 1;
    }
    if (!(mode == ATP_CP_WEIGHT_TWEE && twee_fuse)) return 0u;
  }
  if (s->use_mix_heuristic) return 0u;
  if (s->goal_lhs != 0 && (s->use_sos || s->use_goal_interleave > 0u)) {
    return 0u;
  }
  if (s->w2_modulo > 0u || s->gnn_coop) return 0u;
  if (s->use_connectedness) {
    return 0u;
  }
#ifdef ATP_CP_GROUND_JOIN
  if (s->use_ground_join) return 0u;
#endif
  return 1u;
#endif
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Treated FT-direct joinability (phase 2b): the exact inner sequence
// of atp_cp_trivially_joinable's FT path -- mirror ensure, doR-only
// normalize x2, ft_eq verdict -- run on the stashed formation-time
// cells instead of ft_from_term(apply(...)) images.  Cell-identical
// input (vcp_treat_certify) + the same normalize = the same verdict
// and the same NF cells; the stash is updated in place so the
// survivor's trace / pack consumers read the NFs.  The AC-eq and
// join-cache branches of the Term-side caller are structurally off on
// this route (fuse gate: ac_mask == 0; route gate: join-cache env
// unset).
static u8 vcp_treat_joinable(AtpState *s) {
  int _dl = (g_atp_phase_detail && g_atp_pushn_active);
  u64 _dt = _dl ? atp_now_us() : 0;
  if (_dl) g_atp_pushn_calls++;
  // drc=off keep lane: the normalize below rewrites the raw stash cells
  // IN PLACE, so a folded-class candidate's raw pair is cloned first --
  // the push gate swaps the clones back into the stash when the join
  // verdict would otherwise drop the CP (see atp_push_cps_traced).
  if (atp_drc_armed(s) && atp_drc_push_on() && g_drc_raw_l == NULL &&
      (!atp_drc_gd_gate() || g_cp_gd) &&
      atp_drc_size_ok_ft(g_vcp_treat.fl, g_vcp_treat.fr) &&
      !ft_eq(g_vcp_treat.fl, g_vcp_treat.fr) &&
      (atp_ft_has_fold(s, g_vcp_treat.fl) ||
       atp_ft_has_fold(s, g_vcp_treat.fr)) &&
      ft_distinct_var_count(g_vcp_treat.fl, g_vcp_treat.fr) <
          REWRITE_MAX_VAR) {
    AtpFt *da = (AtpFt *)s->ft_arena_ptr;
    g_drc_raw_l = ft_clone_span(da, g_vcp_treat.fl, 0);
    g_drc_raw_r = ft_clone_span(da, g_vcp_treat.fr, 0);
    // Var-canonical clones: the dedup hash must identify alpha-variant
    // re-derivations (the raw formation ids are arbitrary), and the
    // kept CP is the same equation modulo renaming.
    ft_normalize_vars_pair(g_drc_raw_l, g_drc_raw_r);
  }
  if (!g_vcp_treat.prenorm) {
    atp_ft_mirror_ensure(s);
    if (_dl) {
      u64 _n = atp_now_us();
      g_atp_pushn_us_mirror += _n - _dt;
      _dt = _n;
    }
    g_vcp_treat.fl = atp_rules_only_normalize_ft(s, g_vcp_treat.fl, 64u);
    g_vcp_treat.fr = atp_rules_only_normalize_ft(s, g_vcp_treat.fr, 64u);
  }
  if (_dl) {
    u64 _n = atp_now_us();
    g_atp_pushn_us_norm += _n - _dt;
    _dt = _n;
  }
  u8 joined = (u8)ft_eq(g_vcp_treat.fl, g_vcp_treat.fr);
  // Simpler-join second chance (opt-in --simpler-join): verdict-only
  // gated R+E pass on the doR NFs -- the stash keeps the engine NFs.
  // Mirrors the hook in atp_cp_trivially_joinable_ft (ft_cpq.c).
  // Mode 2 (pop-only) skips the generation-time arm but still builds
  // the peak: its sj runs at pop, where the stash supplies it.
  if (!joined && s->use_simpler_join != 0u) {
    // Peak for checkOrder/step3 (Twee Join.hs): sigma(lo) rebuilt from
    // the stashed formation operands -- same raw variable-id space as
    // the treated fl/fr cells.  A SURVIVOR keeps it on the stash for
    // the push loop's pop-peak pack (released with the stash); a
    // joined CP frees it here.
    AtpFt *pa = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *pk =
        (g_vcp_treat.lo != NULL && g_vcp_treat.subst != NULL)
            ? ft_subst_apply(pa, g_vcp_treat.lo, g_vcp_treat.subst, 0)
            : NULL;
    if (s->use_simpler_join == 1u) {
      if (pk != NULL) atp_sj_peak_arm_ft(pk);
      joined = atp_cp_sj_joinable_ft(s, g_vcp_treat.fl, g_vcp_treat.fr);
      atp_sj_peak_disarm();
      if (joined) s->n_cps_dropped_simpler_join++;
    }
    if (pk != NULL) {
      if (joined) ft_free_span(pa, pk, pk->end);
      else        g_vcp_treat.peak = pk;
    }
  }
  if (_dl) {
    g_atp_pushn_us_eq += atp_now_us() - _dt;
    if (joined) g_atp_pushn_joined++;
  }
  return joined;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Raw-overlap stash for the CP about to be queued (use_otter_raw): the
// pair's bytes BEFORE the push-time treatment, armed for the heap
// insert.  Nothing is stored where the queued pair already is the raw
// pair (the untreated class, the FT-direct emit route) or on a pack
// anomaly; the slot then normalizes from its queued pair.
static void atp_cp_raw_pend(AtpState *s, Term raw_lhs, Term raw_rhs,
                            u8 raw_untreated) {
  atp_cp_raw_pending_drop();
  if (raw_untreated) return;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (g_vcp_emit.armed) return;
  if (g_vcp_treat.armed) {
    // The treated FT-direct route keeps its raw pair as formation
    // operands; the trace store may already have emitted the bytes.
    u32 n = g_vcp_treat.raw_len;
    if (n == 0u) n = vcp_ft_treat_emit_raw(s);
    if (n == 0u) return;
    u8 *b = (u8 *)malloc(n);
    if (b == NULL) thvm_fatal("atp_cp_raw_pend: OOM");
    memcpy(b, g_acp_scratch, n);
    g_cp_raw_pending = b;
    return;
  }
#endif
  if (raw_lhs == 0 || raw_rhs == 0 ||
      term_tag(raw_lhs) == TAG_NUM || term_tag(raw_rhs) == TAG_NUM) {
    return;
  }
  Term l = raw_lhs, r = raw_rhs;
  thvm_normalize_vars(&l, &r);
  g_cp_raw_pending = acp_pack(l, r, NULL, NULL);
}

// Treated CPs the walk former has formed but not yet pushed, and whether
// atp_push_batch_flush is pushing them (push_batch.c).
static u32 g_pb_n        = 0u;
static u8  g_pb_flushing = 0u;

static u32 atp_push_cps_traced(AtpState *s, const CriticalPair *cps,
                               u32 ncps, u32 parent_a, u32 parent_b,
                               u32 rule_a, u32 rule_b) {
  // A push while treated CPs wait in the batch would queue this CP ahead
  // of CPs formed before it: every such route flushes first.
  if (g_pb_n != 0u && !g_pb_flushing) {
    thvm_fatal("atp_push_cps_traced: a push batch is pending");
  }
  u32 pushed = 0;
  u8 fuse = atp_cp_push_fuse_ok(s);
  // Consume the singleton size hint (see g_cp_rawsz_hint): valid for
  // exactly the one-CP call the walk former armed immediately before
  // calling; cleared here unconditionally so it can never leak onto
  // a later CP (batch buf callers never set it and read 0).
  u32 rawsz_hint = (ncps == 1u) ? g_cp_rawsz_hint : 0u;
  g_cp_rawsz_hint = 0u;
  // Twee getDepth: this CP's formation depth = max(parent rule depths) + 1
  // (axioms are depth 0).  Consumed by the heap insert (stamps cp_depth[i])
  // and the ATP_CP_WEIGHT_TWEE score.  Guarded on r_depth so any pre-init
  // path is depth 0.  Set once per call (parents are fixed for the batch).
  {
    u32 da = (s->r_depth != NULL && rule_a < s->n_rules) ? s->r_depth[rule_a] : 0u;
    u32 db = (s->r_depth != NULL && rule_b < s->n_rules) ? s->r_depth[rule_b] : 0u;
    g_cp_formation_depth = ((da > db) ? da : db) + 1u;
    // spb=goal: derived CP is goal-derived iff EITHER parent is.
    u8 ga = (s->r_gd != NULL && rule_a < s->n_rules) ? s->r_gd[rule_a] : 0u;
    u8 gb = (s->r_gd != NULL && rule_b < s->n_rules) ? s->r_gd[rule_b] : 0u;
    g_cp_gd = (u8)(ga | gb);
  }
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
    g_cp_push_deprio = 0u;   // per-CP: set only by the GJ-keep block below
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
    // (raw_lhs, raw_rhs) are var-normalized LAZILY -- below, after the
    // drop gates -- since a dropped CP (joinable / subsumed) never
    // reaches the trace push that consumes the normalized raw pair.
    // The snapshot handles stay valid: the treatment writes back only
    // through cp_lhs/cp_rhs (fresh locals), never into the stored raw
    // terms.
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
    // ifeq-strip emit hijack (see atp_ifeq_strip_hijack_emit): this CP's
    // treat stash is OWNED HERE, not by the walk former -- released
    // after the push (or at the joined drop) below.
    u8 strip_hijack  = 0u;
    u64 _ph_push_t0 = atp_phase_now();
    g_atp_pushn_active = 1u;
    // MNF gate (kept even though lazy defaults to off now): when the WL
    // surface enables lazy via Method "LazyNormalize" -> True on a config
    // that also runs MNF, the gate routes around the front search's
    // direct-cell-reference Bus error.
    if (s->use_lazy_normalize) {   // MNF coupling removed (probe)
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
      // DISCOUNT lane forces the gate to 0 (atp_behandeln_gate): every CP is
      // at/over the gate, so nothing is treated at push -- all queue RAW
      // (passive rawness).  Plain lazy keeps WM's <50 treatment.  MUST match
      // the FT formers' gate (they arm the treated stash below the gate).
      const u32 WM_BEHANDELN_GATE = atp_behandeln_gate(s);   // KPVerwaltung.c:437
      u64 _dg_t0 = g_atp_phase_detail ? atp_now_us() : 0;
      // Capped count: the gate consumes only the (< 50) verdict, so
      // the walk stops the moment the running total reaches the gate
      // -- identical verdict, and the raw >= 50 giants (the class the
      // gate exists to skip) no longer pay a full O(|CP|) count.
      // Size verdict: prefer the walk former's exact formation-time
      // count (rawsz_hint; identical verdict by exactness -- the hint
      // is the uncapped true total, so hint < 50 iff the capped
      // re-walk stays < 50).  Hint 0 / kill switch / certifier mode
      // run the historic capped counting walk.
      static int szhint_off = -1, szhint_check = -1;
      if (szhint_off < 0) {
        const char *e = getenv("THVM_NO_CP_SZHINT");
        szhint_off = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
        const char *c = getenv("THVM_ATP_SZHINT_CHECK");
        szhint_check = (c != NULL && c[0] != '0' && c[0] != '\0') ? 1 : 0;
      }
      u32 raw_sz = 0u;
      if (s->use_discount) {
        // Gate 0: skip the size walk, force the RAW arm (raw_sz >= gate 0).
        raw_sz = 0u;
      } else if (rawsz_hint != 0u && !szhint_off && !szhint_check) {
        raw_sz = (rawsz_hint < WM_BEHANDELN_GATE) ? rawsz_hint
                                                  : WM_BEHANDELN_GATE;
      } else {
        atp_symbol_count_upto(cp_lhs, WM_BEHANDELN_GATE, &raw_sz);
        if (raw_sz < WM_BEHANDELN_GATE) {
          atp_symbol_count_upto(cp_rhs, WM_BEHANDELN_GATE, &raw_sz);
        }
        // Certifier: under the cap the walk total is exact, so the
        // hint must EQUAL it; at/over the cap the hint must be >= it.
        if (szhint_check && rawsz_hint != 0u &&
            ((raw_sz < WM_BEHANDELN_GATE) ? (rawsz_hint != raw_sz)
                                          : (rawsz_hint < raw_sz))) {
          fprintf(stderr, "[szhint MISMATCH] hint=%u walk=%u\n",
                  rawsz_hint, raw_sz);
          thvm_fatal("szhint: size-gate verdict diverges from the walk");
        }
      }
      if (g_atp_phase_detail) g_atp_pushn_us_gate += atp_now_us() - _dg_t0;
      if (raw_sz < WM_BEHANDELN_GATE) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        if (g_vcp_treat.armed) {
          // Treated FT-direct route: the raw pair exists only as the
          // stashed FT cells (cp_lhs/cp_rhs are 0); treat them
          // directly.
          i64 _jb = ftnet_now();
          joinable = vcp_treat_joinable(s);
          g_ftnet_join += ftnet_now() - _jb;
        } else {
          atp_sj_peak_arm_term(cps[i].peak);   // sj checkOrder/step3 peak
          joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
          atp_sj_peak_disarm();
        }
#else
        atp_sj_peak_arm_term(cps[i].peak);   // sj checkOrder/step3 peak
        joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
        atp_sj_peak_disarm();
#endif
        s->n_cps_push_normalized++;
      } else {
        joinable = 0u;
        raw_untreated = 1u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        // Formation-time resolved-guard stripping (THVM_ATP_IFEQ_STRIP,
        // doc block at atp_ifeq_strip_env).  Emit-routed raw CPs are
        // hijacked onto the treated-stash route with stripped spans;
        // classic-build raw CPs strip their Terms in place.  A strip
        // below the KPBehandeln gate reclassifies the CP into the
        // treated class (full doR treatment + joined-drop).
        if (atp_ifeq_strip_armed(s) &&
            !s->use_perm_subsume && !s->use_rule_subsume_drop &&
            !s->use_queue_subsume) {
          u32 ssz = 0u;
          if (g_vcp_emit.armed) {
            ssz = atp_ifeq_strip_hijack_emit(s);
            if (ssz != 0u) strip_hijack = 1u;
          } else if (!g_vcp_treat.armed && cp_lhs != 0 && cp_rhs != 0 &&
                     term_tag(cp_lhs) != TAG_NUM &&
                     term_tag(cp_rhs) != TAG_NUM) {
            ssz = atp_ifeq_strip_terms(s, &cp_lhs, &cp_rhs);
          }
          if (ssz != 0u) {
            g_ifeq_strip_n_hit++;
            if (ssz < WM_BEHANDELN_GATE) {
              g_ifeq_strip_n_reclass++;
              raw_untreated = 0u;
              s->n_cps_push_normalized++;
              if (strip_hijack) {
                i64 _jb = ftnet_now();
                joinable = vcp_treat_joinable(s);
                g_ftnet_join += ftnet_now() - _jb;
                // Joined hijacked stash: consumed here -- the generic
                // joined drop below reads nothing from it, and the
                // former's tail only disarms the emit.
                if (joinable) {
                  vcp_treat_release(s);
                  strip_hijack = 0u;
                }
              } else {
                atp_sj_peak_arm_term(cps[i].peak);   // sj checkOrder/step3 peak
                joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
                atp_sj_peak_disarm();
              }
            }
          }
        }
#endif
      }
    } else {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      if (g_vcp_treat.armed) {
        // Treated FT-direct route, non-lazy stream (every CP treated):
        // the raw pair exists only as the stashed FT cells.
        i64 _jb = ftnet_now();
        joinable = vcp_treat_joinable(s);
        g_ftnet_join += ftnet_now() - _jb;
      } else {
        atp_sj_peak_arm_term(cps[i].peak);   // sj checkOrder/step3 peak
        joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
        atp_sj_peak_disarm();
      }
#else
      atp_sj_peak_arm_term(cps[i].peak);   // sj checkOrder/step3 peak
      joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
      atp_sj_peak_disarm();
#endif
      s->n_cps_push_normalized++;
    }
    g_atp_pushn_active = 0u;
    if (g_atp_phase_enabled) g_atp_phase_us_push_normalize += atp_now_us() - _ph_push_t0;
    // drc=off keep lane (see atp_drc_off_on): a folded-class CP whose
    // sides JOIN is not dropped -- its RAW pair becomes the queued
    // content, so the demod-redundant folded lemma can become a rule.
    if (joinable && atp_drc_armed(s) && atp_drc_push_on()) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      if (g_vcp_treat.armed) {
        if (g_drc_raw_l != NULL &&
            !atp_drc_seen_probe(atp_drc_pair_hash_ft(g_drc_raw_l,
                                                     g_drc_raw_r)) &&
            atp_drc_needle_gate(s, ft_to_term(g_drc_raw_l),
                                    ft_to_term(g_drc_raw_r))) {
          // Swap the raw clones back into the stash (the treatment
          // normalized fl/fr to the -- now equal -- NF cells in place).
          AtpFt *da = (AtpFt *)s->ft_arena_ptr;
          if (g_vcp_treat.fl != NULL)
            ft_free_span(da, g_vcp_treat.fl, g_vcp_treat.fl->end);
          if (g_vcp_treat.fr != NULL)
            ft_free_span(da, g_vcp_treat.fr, g_vcp_treat.fr->end);
          g_vcp_treat.fl = g_drc_raw_l;
          g_vcp_treat.fr = g_drc_raw_r;
          g_drc_raw_l = g_drc_raw_r = NULL;
          joinable = 0u;
          s->n_cps_drc_kept_push++;
        }
      } else
#endif
      if ((!atp_drc_gd_gate() || g_cp_gd) &&
          raw_lhs != 0 && !kbo_eq(raw_lhs, raw_rhs) &&
          atp_drc_size_ok_term(raw_lhs, raw_rhs) &&
          (atp_term_has_fold(s, raw_lhs) || atp_term_has_fold(s, raw_rhs))) {
        // Var-canonical dedup key (alpha-variant re-derivations must
        // collide); the kept content is the renumbered pair too.
        Term hl = raw_lhs, hr = raw_rhs;
        thvm_normalize_vars(&hl, &hr);
        if (!atp_drc_seen_probe(atp_drc_pair_hash_term(hl, hr)) &&
            atp_drc_needle_gate(s, hl, hr)) {
          cp_lhs = hl;
          cp_rhs = hr;
          joinable = 0u;
          s->n_cps_drc_kept_push++;
        }
      }
    }
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Survivor / non-kept candidate: drop the unused raw clones.
    if (g_drc_raw_l != NULL) {
      AtpFt *da = (AtpFt *)s->ft_arena_ptr;
      ft_free_span(da, g_drc_raw_l, g_drc_raw_l->end);
      ft_free_span(da, g_drc_raw_r, g_drc_raw_r->end);
      g_drc_raw_l = g_drc_raw_r = NULL;
    }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Simpler-join pop-peak map basis: the queued pair BEFORE the
    // canonical renumber.  The dense first-occurrence map built over
    // this pair is the exact numbering thvm_normalize_vars /
    // acp_pack_fused assign at pack time, and cps[i].peak shares its
    // raw var space -- so a peak packed through this map lands in the
    // slot's byte-string var space (see atp_sj_peak_pack_term).
    Term _pk_base_l = cp_lhs, _pk_base_r = cp_rhs;
#endif
#ifdef ATP_VAR_NORM
    // 7c: canonically renumber the CP's variables -- AFTER reduction,
    // since rewriting can drop variables, so the dense [0, k) set is
    // computed on the form actually queued.  The CP enumerator bakes
    // CP_RENAME_OFFSET into the stored term, carrying ids past
    // REWRITE_MAX_VAR where the matcher goes dead; renumbering keeps
    // every stored var matchable AND makes alpha-equivalent CPs
    // byte-identical so the subsumption filters below actually fire.
    //
    // Joinable fast-drop: a treated-and-joined CP is discarded at the
    // `if (joinable)` gate below before anything consumes the canonical
    // numbering, so the renumber is skipped -- UNLESS a push-side
    // filter that reads the treated pair sits between here and that
    // gate (those rely on the canonical ids to fire; all off in the WM
    // presets) or the cpraw fate trace prints the treated pair.
    u8 _nv_need = !joinable
                || s->use_perm_subsume || s->use_rule_subsume_drop
                || s->use_queue_subsume || s->use_connectedness
                || cpraw_debug != 0;
#ifdef ATP_CP_GROUND_JOIN
    if (s->use_ground_join) _nv_need = 1u;
#endif
#ifdef ATP_CP_DIAG
    _nv_need = 1u;
#endif
    // Fused pipeline: the heap push renumbers during its own byte
    // emit (acp_pack_fused), so the treated pair keeps its raw-id
    // handles here.  Every gate between this point and the push that
    // could consume the renumbered pair is off under fuse.
    if (fuse) _nv_need = 0u;
    // Pre-treatment handles: when the treatment wrote nothing back
    // (the raw >= 50 untreated class, or a no-op normalize returning
    // the same cells), the treated pair IS the raw pair, so the
    // deferred raw-pair renumber below can reuse this normalize's
    // result instead of re-walking the giant raw terms.
    Term _nv_pre_l = cp_lhs, _nv_pre_r = cp_rhs;
    u8   _nv_ran   = _nv_need;
    if (_nv_need) {
      u64 _dvn_t0 = g_atp_phase_detail ? atp_now_us() : 0;
      thvm_normalize_vars(&cp_lhs, &cp_rhs);
      if (g_atp_phase_detail) g_atp_cpg_us_varnorm += atp_now_us() - _dvn_t0;
    }
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
    u64 _dfl_t0 = g_atp_phase_detail ? atp_now_us() : 0;
    u8 _drop_perm = (s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull &&
                     atp_cp_perm_subsumed(cp_lhs, cp_rhs)) ? 1u : 0u;
    if (g_atp_phase_detail) g_atp_cpg_us_filters += atp_now_us() - _dfl_t0;
    if (_drop_perm) {
      s->n_cps_dropped_perm_subsumed++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP perm-subsumed\n", i);
      if (g_cptr_active) fprintf(stderr, "CPTR PUSH DROP perm-subsumed\n");
      continue;
    }
    // WM dokgS port: push-time rule-subsumption drop (KPVerwaltung.c:451
    // KPBehandelt's SS_TermpaarSubsummiertVonGM branch).  Strict subset
    // of trivial-join (rule_subsumed -> joinable) so dropping is sound;
    // catches CPs that would normalize away WITHOUT running the full
    // joinability normalize.  Gated by use_rule_subsume_drop.
    _dfl_t0 = g_atp_phase_detail ? atp_now_us() : 0;
    u8 _drop_rule = (s->use_rule_subsume_drop &&
                     atp_cp_rule_subsumed(s, cp_lhs, cp_rhs)) ? 1u : 0u;
    if (g_atp_phase_detail) g_atp_cpg_us_filters += atp_now_us() - _dfl_t0;
    if (_drop_rule) {
      s->n_cps_dropped_rule_subsumed++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP rule-subsumed\n", i);
      if (g_cptr_active) fprintf(stderr, "CPTR PUSH DROP rule-subsumed\n");
      continue;
    }
    // thvm-native filter, no WM counterpart (see AtpState.use_queue_
    // subsume): WM's recentCPinsert queues every treated survivor and
    // stamps it a fresh w2 = ++CPNr FIFO age; the WM presets run with
    // the gate OFF so queue composition and ages match WM exactly.
    _dfl_t0 = g_atp_phase_detail ? atp_now_us() : 0;
    u8 q_subsmd    = s->use_queue_subsume
                   ? atp_cp_queue_subsumed(s, cp_lhs, cp_rhs) : 0u;
    if (g_atp_phase_detail) g_atp_cpg_us_filters += atp_now_us() - _dfl_t0;
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
    // NOTE: the emission-order fix (WM side rule) swapped cp_lhs/cp_rhs;
    // this verdict was corpus-validated consuming (reduct, other), so feed
    // it the pre-fix order (experiment: localizing side-consumers).
    if (s->use_ground_join && atp_cp_ground_joinable(s, cp_rhs, cp_lhs)) {
      s->n_cps_ground_joinable++;
      // THVM_ATP_GJ_KEEP: instead of DROPPING the ground-joinable CP, KEEP
      // it but mark it never-weight-selectable (Waldmeister keeps its 168k
      // unselected CPs to age the FIFO dimension faithfully; dropping them
      // makes thvm's FIFO over-select a heavier non-GJ CP -- the firstdiv-
      // 251 over-gen).  The deprio flag flows into insert_packed below.
      static int gjkeep = -1;
      if (gjkeep < 0) gjkeep = atp_env_on("THVM_ATP_GJ_KEEP");
      if (gjkeep) g_cp_push_deprio = 1u;
      else continue;
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
      if (g_cptr_active) fprintf(stderr, "CPTR PUSH DROP joinable\n");
      continue;
    }
    if (q_subsmd) {
      s->n_cps_dropped_queue_subsumed++;
      if (cpraw_debug)
        fprintf(stderr, "[cpfate] %u: DROP queue-subsumed\n", i);
      if (g_cptr_active) fprintf(stderr, "CPTR PUSH DROP queue-subsumed\n");
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
#ifdef ATP_VAR_NORM
    // Deferred raw-pair var-normalize (see the snapshot comment above):
    // runs only for CPs that SURVIVED every drop gate, i.e. exactly the
    // ones whose normalized raw pair the trace push / debug dump below
    // consume.  Same alpha-canonical result as normalizing eagerly.
    {
      u64 _dvr_t0 = g_atp_phase_detail ? atp_now_us() : 0;
      if (fuse) {
        // Fused: an off-heap trace store renumbers during its own
        // byte emit (g_trace_eq_nv below), a lean trace stores the
        // pair nowhere; only a Term-materialized trace still needs
        // the renumbered raw Terms.
        if (!s->trace_pack && !s->cp_trace_lean) {
          thvm_normalize_vars(&raw_lhs, &raw_rhs);
        }
      } else if (_nv_ran && raw_lhs == _nv_pre_l && raw_rhs == _nv_pre_r) {
        // Treatment wrote nothing back: normalize(raw) == the treated
        // renumber already computed above.  Reuse it.
        raw_lhs = cp_lhs;
        raw_rhs = cp_rhs;
      } else {
        thvm_normalize_vars(&raw_lhs, &raw_rhs);
      }
      if (g_atp_phase_detail) g_atp_cpg_us_varnorm += atp_now_us() - _dvr_t0;
    }
#endif
    static int cpgen_debug = -1;
    if (cpgen_debug < 0) cpgen_debug = getenv("THVM_ATP_CPGEN_DEBUG") != NULL;
    if (cpgen_debug) {
      char la[8192], ra[8192], la_raw[256], ra_raw[256];
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
    u64 _dtr_t0 = g_atp_phase_detail ? atp_now_us() : 0;
    g_trace_eq_nv = (u8)(fuse && s->trace_pack);
    u32 t = atp_trace_push_cp(s, parent_a, parent_b, raw_lhs, raw_rhs,
                              cps[i].pos, cps[i].pos_len, cps[i].combo);
    g_trace_eq_nv = 0u;
    if (g_atp_phase_detail) g_atp_cpg_us_trace += atp_now_us() - _dtr_t0;
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
    u64 _dpk_t0 = g_atp_phase_detail ? atp_now_us() : 0;
    if (s->use_implicit_cp != 0u && t != ATP_TRACE_NONE && t < s->t_vis
        && parent_a != ATP_TRACE_NONE && parent_b != ATP_TRACE_NONE) {
      deferred = atp_cp_implicit_push(s, cp_lhs, cp_rhs,
                                      parent_a, parent_b, t, cp_ult);
    }
    // WM Waisenmord identity stamps (see AtpState.r_uid): the parent
    // slots' uids ride into cp_uid_a/b so the orphan test works past
    // the proof-trace soft cap.  Guard on live slot indices -- axiom
    // intake / requeued facts pass sentinels and stamp 0 (= no parent).
    g_cp_uid_a = (s->r_uid != NULL && rule_a < s->n_rules)
               ? s->r_uid[rule_a] : 0u;
    g_cp_uid_b = (s->r_uid != NULL && rule_b < s->n_rules)
               ? s->r_uid[rule_b] : 0u;
    g_cp_push_fused = fuse;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Simpler-join pop-peak stash: pack the formation peak through the
    // queued pair's dense var map and arm it for the heap insert (Twee
    // re-derives the overlap top at pop; we stash it at push).  Routes
    // with no space-consistent peak leave the pending empty and the
    // slot degrades to the peakless pop verdict: the raw FT-emit lane
    // (bytes already in the shared scratch, peak wiring future work),
    // drc keep lanes (content re-renumbered), deferred descriptors,
    // and pack anomalies.
    if (s->use_simpler_join != 0u && !deferred && !g_vcp_emit.armed &&
        !atp_drc_armed(s)) {
      if (g_vcp_treat.armed) {
        if (g_vcp_treat.peak != NULL && g_vcp_treat.fl != NULL &&
            g_vcp_treat.fr != NULL) {
          g_sj_peak_pending = atp_sj_peak_pack_ft(g_vcp_treat.peak,
                                                  g_vcp_treat.fl,
                                                  g_vcp_treat.fr);
        }
      } else if (cps[i].peak != 0 && cp_lhs != 0 && cp_rhs != 0 &&
                 term_tag(cp_lhs) != TAG_NUM &&
                 term_tag(cp_rhs) != TAG_NUM) {
        g_sj_peak_pending = atp_sj_peak_pack_term(cps[i].peak,
                                                  _pk_base_l, _pk_base_r);
      }
    }
#endif
    if (!deferred) {
      if (s->use_otter_raw) {
        atp_cp_raw_pend(s, raw_lhs, raw_rhs, raw_untreated);
      }
#ifdef THVM_ATPFT_ALLOC
      i64 _hb = ftnet_now();
#endif
      atp_cp_heap_push(s, cp_lhs, cp_rhs, t, cp_ult, raw_untreated);
#ifdef THVM_ATPFT_ALLOC
      g_ftnet_push += ftnet_now() - _hb;
#endif
    }
    g_cp_push_fused = 0u;
    atp_sj_peak_pending_drop();   // consumed by the insert; defensive
    atp_cp_raw_pending_drop();
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Hijack-owned stash (ifeq-strip): free un-adopted spans + disarm.
    // A CPQ insert NULLed fl/fr on adopt; the packed-byte queue leaves
    // them here.  The walk former's tail only disarms its emit.
    if (strip_hijack) vcp_treat_release(s);
#endif
    if (g_atp_phase_detail) g_atp_cpg_us_pack += atp_now_us() - _dpk_t0;
    g_cp_uid_a = g_cp_uid_b = 0u;
    if (g_cptr_active)
      fprintf(stderr, "CPTR PUSH QUEUED seq=%u deferred=%u\n",
              s->cp_seq_next - 1u, deferred);
    pushed++;
  }
  return pushed;
}

