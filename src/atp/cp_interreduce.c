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
  // Position-order walk: survivors compact to heap positions [0, w) in
  // visit order (the historical payload compaction order -- the Floyd
  // input inside the trailing reheapify depends on it).  Payload rows
  // stay put; only cp_heap entries compact, then the row densify slides
  // survivor rows >= w into the freed rows.
  u32 w = 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 vv = s->cp_heap[i].row;
    int orphan = 0;
    u32 ti = s->cp_trace[vv];
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
      free(s->cp_packed[vv]);
      s->cp_packed[vv] = NULL;
      atp_cp_implicit_clear(s, vv);
#ifdef THVM_ATPFT_CPQ
      atp_cp_ft_clear(s, vv);        // keep the FT queue in lockstep
#endif
      continue;
    }
    // w <= i: never overtakes the read cursor.  Keys untouched on this
    // path; atp_cp_heap_set restamps the (already-exact) caches.
    atp_cp_heap_set(s, w, vv);
    w++;
  }
  if (w != s->n_cps) {
    atp_cp_rows_densify(s, w);
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

// Should the budget-disciplined CP-set sweep run now?  A faithful port
// of Twee's renormalise task (Twee.hs:978 `newTask 10 0.05` wrapping
// `shouldSimplifyQueue`; Task.hs `taskDue`):
//   1. frequency  -- at most once per ATP_CP_SET_IR_FREQ_US (Twee: the
//      task runs at most every 10 s);
//   2. budget     -- only while cumulative sweep time is under
//      cp_set_ir_budget% of the elapsed run (Twee task_budget, from
//      cfg_renormalise_percent; strict `>` so a zero budget never runs);
//   3. threshold  -- only when >= ATP_CP_SET_IR_THRESHOLD % of an
//      ATP_CP_SET_IR_SAMPLE-sized random sample of the queue is DEAD:
//      joinable under the current R (the sweep's delete criterion,
//      tested with the same atp_cp_trivially_joinable the push path
//      uses) or orphaned (gated on use_orphan_murder exactly like the
//      sweep's own orphan branch).  Twee keeps a reservoir sample of
//      live overlaps and re-simplifies it after each interreduction
//      (st_cp_sample / simplifySample); sampling at check time observes
//      the same statistic without the persistent mirror.
// The sample cost is charged to the budget (Twee's task_spent accrues
// the body, cheap checks included).  Deterministic: the sample indices
// come from a splitmix64 stream seeded by run-state counters, so a
// fixed trajectory re-samples identically.
static u8 atp_cp_set_ir_due(AtpState *s) {
  if (s->n_cps == 0u || s->n_rules == 0u) return 0;
  u64 now = atp_now_us();
  if (s->cp_set_ir_start_us == 0u) {
    // Discipline epoch: first check after the queue came alive.  Twee
    // stamps task_start/task_last at task creation (prover start).
    s->cp_set_ir_start_us = now;
    s->cp_set_ir_last_us  = now;
    return 0;
  }
  // 1. frequency gate.
  if (now - s->cp_set_ir_last_us < ATP_CP_SET_IR_FREQ_US) return 0;
  // 2. budget gate (strict >, Twee taskDue).
  if ((now - s->cp_set_ir_start_us) * (u64)s->cp_set_ir_budget
      <= s->cp_set_ir_spent_us * 100ull) {
    return 0;
  }
  // 3. sampled dead-fraction threshold.
  u32 n = s->n_cps;
  u32 sample = n < ATP_CP_SET_IR_SAMPLE ? n : ATP_CP_SET_IR_SAMPLE;
  u64 seed = ((u64)s->cp_select_count << 32) ^ ((u64)s->r_revision << 16)
           ^ (u64)n;
  u32 dead = 0;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  atp_ft_mirror_ensure(s);
#endif
  for (u32 k = 0; k < sample; k++) {
    seed += 0x9e3779b97f4a7c15ull;
    u64 x = seed;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
    x ^= x >> 27; x *= 0x94d049bb133111ebull;
    x ^= x >> 31;
    u32 vv = s->cp_heap[(u32)(x % (u64)n)].row;
    if (s->use_orphan_murder &&
        (atp_cp_is_orphan(s, s->cp_trace[vv]) || atp_cp_uid_orphan(s, vv))) {
      dead++;
      continue;
    }
    u64 hcp = thvm_atp_heap_checkpoint();
    Term l = 0, r = 0;
    atp_cp_slot_read(s, vv, &l, &r);
    if (atp_cp_trivially_joinable(s, &l, &r)) dead++;
    thvm_atp_heap_reset(hcp);
  }
  u64 after = atp_now_us();
  s->cp_set_ir_spent_us += after - now;
  s->cp_set_ir_last_us   = after;
  return dead * 100u >= ATP_CP_SET_IR_THRESHOLD * sample;
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
    // i is the sweep's heap POSITION cursor; vv the CP's payload row.
    // Survivors compact to positions [0, w) in visit order (w <= i, so
    // the cp_heap[w] writes never overtake this read); rows stay put
    // until the post-loop densify.
    u32 vv = s->cp_heap[i].row;
    u64 _cpir_unpack_t0 = atp_phase_now();
    // Eager Waisenmord (WM AP_generic head): a CP whose parent rule has
    // been retired is redundant under the surviving R, so we can drop it
    // without paying the per-CP normalize.  Mirrors the lazy orphan check
    // at CP-select time -- doing it inside the IR sweep means a single
    // KPV_KPMengeInterreduzieren pass shrinks the queue by the dead-parent
    // set in linear time, instead of waiting for select-side amortization.
    if (s->use_orphan_murder &&
        (atp_cp_is_orphan(s, s->cp_trace[vv]) || atp_cp_uid_orphan(s, vv))) {
      atp_cp_watch_seq(s->cp_seq[vv], "IR-DROP orphan");
      s->n_cps_dropped_orphan++;
      s->n_cp_set_ir_deleted++;
      free(s->cp_packed[vv]);   // free(NULL) no-op for deferred slots
      s->cp_packed[vv] = NULL;
      atp_cp_implicit_clear(s, vv);
#ifdef THVM_ATPFT_CPQ
      atp_cp_ft_clear(s, vv);        // keep the FT queue in lockstep
#endif
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
    u8 is_impl = atp_cp_slot_implicit(s, vv);
    // Per-CP heap checkpoint: the normalize allocates scratch cells; the
    // reduced terms are copied out by acp_pack, so the scratch is dead
    // after the (re)pack.  Reset each iteration so a long queue cannot
    // exhaust the dyn heap.  (An implicit slot's materialized pair lives
    // in the trace, which predates the checkpoint -- the reset cannot
    // pop it.)
    u64 hcp = thvm_atp_heap_checkpoint();
    Term ol = 0, orr = 0;
    atp_cp_slot_read(s, vv, &ol, &orr);
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
      free(s->cp_packed[vv]);   // free(NULL) no-op for deferred slots
      s->cp_packed[vv] = NULL;
      atp_cp_implicit_clear(s, vv);
#ifdef THVM_ATPFT_CPQ
      atp_cp_ft_clear(s, vv);        // keep the FT queue in lockstep
#endif
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
            || s->cp_last_norm_r_revision[vv] != ATP_CP_NORM_COOKIE_NONE)) {
      u64 cp_syms = atp_term_sym_bitmap(ol) | atp_term_sym_bitmap(orr);
      if ((cp_syms & new_top_syms) == 0u) {
        // No new rule can fire on this CP -- skip the normalize.
        // Survive unchanged: the payload (packed bytes / descriptor /
        // trace / kind / cookie / seq / push-time keys) stays on the
        // stationary row; only the heap entry compacts.
        if (!is_impl) {
          // Fold the per-survivor priority recompute into the IR walk
          // so the post-loop reheapify only has to do Floyd + FV.
          // Sides are byte-preserved (no normalize), so (ol, orr) is
          // still the packed payload; priority is pure in (s, l, r)
          // (see header).  Deferred survivors instead keep the
          // push-time keys already on the row (see the cookie path
          // below for why recomputing on the raw pair would flip-flop
          // the key against the descriptor cache).
          atp_cp_commit_priorities(s, w, vv, ol, orr);
        }
        // Compact the heap entry AFTER the key commit so the packed
        // node caches pick up the fresh cp_pri (refresh invariant).
        atp_cp_heap_set(s, w, vv);
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
    if (s->cp_last_norm_r_revision[vv] == r_rev_snap) {
      // NF-confirmed survivor: everything (cookie included -- it
      // already equals r_rev_snap) stays on the stationary row; only
      // the heap entry compacts.  Implicit slots keep the push-time
      // keys (the descriptor design -- the cached priority was
      // computed on the push-time reduced form and a reheapify
      // restores it from the descriptor, so recomputing on the raw
      // pair here would just flip-flop the key).
      if (!is_impl) {
        // Fold the per-survivor priority recompute (NF-witness cookie
        // path: sides are bit-preserved, no normalize was needed).
        atp_cp_commit_priorities(s, w, vv, ol, orr);
      }
      // Compact the heap entry AFTER the key commit so the packed
      // node caches pick up the fresh cp_pri (refresh invariant).
      atp_cp_heap_set(s, w, vv);
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
    // Packed slot: decode the byte string STRAIGHT to cells (the same
    // bit-identical spans ft_from_term of the just-unpacked Terms
    // would build -- the pop-stash decoder).  Implicit slots keep the
    // Term re-encode.
    AtpFtCell *fl, *fr;
    if (!atp_cp_slot_implicit(s, vv) && s->cp_packed[vv] != NULL) {
      const u8 *pp = s->cp_packed[vv];
      fl = acp_unpack_term_ft(a, &pp, 0);
      fr = acp_unpack_term_ft(a, &pp, 0);
    } else {
      fl = ft_from_term(a, ol,  0);
      fr = ft_from_term(a, orr, 0);
    }
    u8 joined_pre = (u8)ft_eq(fl, fr);
    if (!joined_pre) {
      // use_pop_rules_only: Twee simplifyPassive is oriented-only --
      // the sweep's delete criterion follows the pop discipline.
      if (s->use_pop_rules_only) {
        fl = atp_rules_only_normalize_ft(s, fl, NORM_CAP);
        fr = atp_rules_only_normalize_ft(s, fr, NORM_CAP);
      } else {
        fl = atp_rewrite_normalize_ft(s, fl, NORM_CAP);
        fr = atp_rewrite_normalize_ft(s, fr, NORM_CAP);
      }
    }
    u8 joined_ft = joined_pre ? 1u : (u8)ft_eq(fl, fr);
    // Decode back to Term: the joined branch only needs ol/orr for the
    // existing acp_free, but staying Term-side keeps the cookie / NF
    // witness comparison byte-identical to the legacy path.
    l = joined_ft ? ol  : ft_to_term(fl);
    r = joined_ft ? orr : ft_to_term(fr);
#else
    if (s->use_pop_rules_only) {
      l = atp_rules_only_normalize(s, ol,  NORM_CAP);
      r = atp_rules_only_normalize(s, orr, NORM_CAP);
    } else {
      l = atp_rewrite_normalize(s, ol,  s->lhs, s->rhs, s->n_rules, NORM_CAP);
      r = atp_rewrite_normalize(s, orr, s->lhs, s->rhs, s->n_rules, NORM_CAP);
    }
    u8 joined_ft = (u8)kbo_eq(l, r);
#endif
    if (g_atp_phase_enabled) acc_norm += atp_now_us() - _cpir_norm_t0;

    u64 _cpir_pack_t0 = atp_phase_now();
    if (joined_ft) {
      // Joinable under R -- the CP adds no equational consequence.  Drop
      // it (WM AP_generic returns WTI_Delete).  Deferred slot: nothing
      // packed to free; the tag bit drops and the trace entry stays (it
      // is a GC root and may back other diagnostics).
      atp_cp_watch_seq(s->cp_seq[vv], "IR-DROP joinable");
      s->n_cps_dropped_joinable++;
      s->n_cp_set_ir_deleted++;
      free(s->cp_packed[vv]);   // free(NULL) no-op for deferred slots
      s->cp_packed[vv] = NULL;
      atp_cp_implicit_clear(s, vv);
#ifdef THVM_ATPFT_CPQ
      atp_cp_ft_clear(s, vv);        // keep the FT queue in lockstep
#endif
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
      free(s->cp_packed[vv]);   // free(NULL) no-op for deferred slots
      s->cp_packed[vv] = acp_pack(l, r, NULL, NULL);
      atp_cp_implicit_clear(s, vv);
      // Raw-desc lane: the slot's bytes were REWRITTEN in place, so any
      // descriptor no longer derives them -- retire it (cert mode would
      // otherwise memcmp the stale birth pair against the reduced one).
      if (s->cp_implicit != NULL) {
        s->cp_implicit[vv].out_face = ATP_RDESC_NONE;
      }
#ifdef THVM_ATPFT_CPQ
      // Reduced pair re-images into the FT queue too: the slot becomes an
      // ordinary packed CP, so cp_packed_ft[vv] must hold (l, r) as well.
      atp_cp_ft_clear(s, vv);
      atp_cp_ft_set(s, vv, l, r, s->cp_pri[vv], /*origin=*/0xffffu);
#endif
      s->n_cp_set_ir_reweighted++;
      touched = 1;
    }
    // Survivor bookkeeping on the stationary row (trace / kind /
    // ultimate / seq stay put -- WM AP_generic reweight preserves the
    // FIFO age: C_ReClassify only recomputes w1, "w2 wird nicht
    // geaendert" / w2 is not changed, CLAS/NewClassification.c:399-406;
    // Act_ultimate's front-rank likewise rides the row untouched).
    s->cp_last_norm_r_revision[vv] = nf_witness ? r_rev_snap
                                            : ATP_CP_NORM_COOKIE_NONE;
    if (!(is_impl && nf_witness)) {
      // Fold the per-survivor priority recompute into the IR walk.  On
      // the reweight branch (l, r) is the post-normalize pair we just
      // packed (for a freshly-eagerified slot too); on the packed
      // no-rewrite branch it's the original pair -- either way, the
      // right input to the priority functions.  An implicit survivor
      // confirmed in NF instead keeps the push-time keys already on
      // the row (the descriptor caches the priority of the push-time
      // reduced form; atp_cp_rebuild_priorities restores from that
      // cache, so the cached value is the one coherent key).
      atp_cp_commit_priorities(s, w, vv, l, r);
    }
    // Compact the heap entry AFTER the key commit so the packed node
    // caches pick up the fresh cp_pri (refresh invariant).
    atp_cp_heap_set(s, w, vv);
    w++;
    thvm_atp_heap_reset(hcp);
    if (g_atp_phase_enabled) acc_pack += atp_now_us() - _cpir_pack_t0;
  }
  u64 _cpir_post_t0 = atp_phase_now();
  // Commit the compaction: densify the payload rows into [0, w) and
  // set n_cps = w (the heap prefix cp_heap[0..w) already holds the
  // survivors in the historical compaction order).
  atp_cp_rows_densify(s, w);
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

