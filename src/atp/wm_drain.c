// Faithful Waldmeister interreduction phase order.  Gated by
// s->use_wm_interred (THVM_ATP_WM_INTERRED).  Mirrors
// ArbeitsAufnahme/IR_Interreduktion{Links,Rechts}: drop every E-member
// EITHER of whose faces the new rule reduces (Phase A = GMInterred, WM
// indexes both directed twins), then drop every R-member whose LHS the
// new rule reduces (Phase B = RMLinksInterred), then compose the RHS of
// every surviving oriented rule in place (Phase C = RMRechtsInterred).
// thvm interleaves drop-or-compose per slot; WM strictly phases, yielding
// a different surviving rule set and CP stream (the root cause of
// WolframAxioms/OrAssoc saturation).  Returns the number of rules dropped
// (Phase A + Phase B), matching the existing function's contract.
// === WM drain-key SNAPSHOT (pass-start leaf-list ranks) =============
//
// WM discovers ALL of an activation's IR victims in single walks over
// the PASS-START discrimination-tree leaf lists: GMInterred =
// BK_ReferenzDurchlauf over the Gleichungsbaum, then RMLinksInterred =
// BK_forRegelnRobust over the Regelbaum (Interreduktion.c:380-392;
// DSBaumKnoten.h:482-514).  A walk's yield order is the leaf-list order
// at walk time -- pulls only remove already-visited entries -- so every
// victim's REPuffer position is a rank in the SAME snapshot.
//
// thvm captures each victim's (leafrank, chainpos) key one drop at a
// time.  If earlier victims' wmo evictions run eagerly, later captures
// rank against a SHRUNKEN leaf list, and cross-victim order inverts on
// dense passes (OA -auto @8597: a 96-victim avalanche where thvm's
// rank baselines skewed the rule-victim segment ordering, interleaving
// the heavy re-adds among WM's early small ones).  Defer the eviction
// of victim faces to the END of the interreduce pass so all keys rank
// against the pass-start tree.  CP generation runs after interreduce,
// so the flush still precedes every tops/overlap walk.
//
// Active only where the keys matter (use_wm_demote && use_emission_
// order); THVM_ATP_DRAIN_SNAPSHOT=0 is the kill switch (restores the
// eager per-drop eviction).
typedef struct {
  u32 *uids;
  u32  n, cap;
  u8   active;
} AtpWmoDefer;

static u8 atp_drain_snapshot_on(void) {
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("THVM_ATP_DRAIN_SNAPSHOT");
    on = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return (u8)on;
}

static void atp_wmo_defer_or_remove(AtpState *s, AtpWmoDefer *d, u32 uid) {
  if (!s->use_emission_order) return;
  if (!d->active) {
    atp_wmo_remove_trace(s, uid);
    return;
  }
  if (d->n == d->cap) {
    u32 cap = d->cap ? d->cap * 2u : 32u;
    u32 *nu = (u32 *)realloc(d->uids, cap * sizeof(u32));
    if (nu == NULL) {
      fprintf(stderr, "atp_wmo_defer: realloc to %u failed\n", cap);
      exit(1);
    }
    d->uids = nu;
    d->cap  = cap;
  }
  d->uids[d->n++] = uid;
}

static void atp_wmo_defer_flush(AtpState *s, AtpWmoDefer *d) {
  for (u32 k = 0; k < d->n; k++) atp_wmo_remove_trace(s, d->uids[k]);
  free(d->uids);
  d->uids = NULL;
  d->n = d->cap = 0;
  d->active = 0;
}

static u32 thvm_atp_interreduce_wm(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0 || added.first == 0) return 0;
  AtpWmoDefer wd = {0};
  wd.active = (s->use_wm_demote && s->use_emission_order &&
               atp_drain_snapshot_on()) ? 1u : 0u;
#ifdef ATP_ORPHAN_KILL
  // 9b orphan collection (same as the legacy fn): gather dropped rules'
  // trace ids across BOTH drop phases, kill descendant CPs once after
  // Phase C.  Disarmed when use_eager_orphan_sweep is OFF (WM layout).
  u32 *atp_dead   = s->use_eager_orphan_sweep
                      ? (u32 *)malloc(added.first * sizeof(u32)) : NULL;
  u32  atp_n_dead = 0;
#endif

  // Copy the new rules' Terms by value so we can compact R beneath them.
  // (Term is 64-bit; the heap cells they point to do not move under a
  // shift, only under a GC collection, after which we re-read.)
  //
  // n_new is the slice WIDTH passed to the normalizers; clamp to 2 (the
  // principal rule + at most one inline sibling carried in the slice).
  // n_new_slots is the FULL new-rule run length used ONLY for the Phase C
  // self-skip window -- added.count can be 3-4 when use_fvi pushes FVI
  // sibling rules (atp_try_ground_and_push bumps r->count, _.c:22256), so
  // every one of those slots must be skipped in Phase C even though the
  // slice arrays only carry the first two.
  Term new_lhs[2];
  Term new_rhs[2];
  u32  new_traces[2];
  u32  n_new       = added.count;
  u32  n_new_slots = added.count;
  if (n_new > 2) n_new = 2;
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k]    = s->lhs[added.first + k];
    new_rhs[k]    = s->rhs[added.first + k];
    new_traces[k] = s->r_trace[added.first + k];
  }

  u32 dropped = 0;

  // ===============================================================
  // SHARED ARRAY-COMPACTION + BOOKKEEPING for a dropped slot IDX.
  //
  // r_overlap_done rides the slot like r_gj_status, so every compaction
  // must shift it in lockstep (atp_overlap_done_shift below); without the
  // shift the flags misalign after any rule removal and the
  // use_overlap_exhaust pair gate (atp_wmo_collect_pair) fires on GARBAGE
  // state -- the pinned WolframAxioms +12-CP walk-vs-batch divergence at
  // pick 51667 (a NEW doubly-unfree equation read done=1, OLD comm read
  // done=0, so the batch dropped 4 comm overlaps WM forms;
  // Unifikation1.c:1595-1648 has no such partner skip).  Unconditional
  // (the one-round THVM_ATP_OVERLAP_DONE_FIX kill switch is retired);
  // the gate baselines are pinned to the fixed trajectory, which is the
  // WM-faithful one (walk==batch byte-identical under the fix).
  //
  // Factored so both the LHS-collapse drop and the E-RHS-face drop use
  // the BYTE-IDENTICAL 9-array + FT-mirror compaction / trace-dead /
  // wmo-evict / n_unorient / n_rules / r_revision / index-dirty block
  // from the legacy fn (_.c:13456-13500 and :13760-13808).  Caller has
  // already done the requeue (atp_irv_push / atp_add_equation_simplified)
  // and the ATP_ORPHAN_KILL_COLLECT for slot IDX.  On return the next
  // member has shifted down into IDX, so the caller must NOT advance IDX.
  // ===============================================================
#define WM_IR_COMPACT_SLOT(IDX)                                               \
  do {                                                                        \
    atp_rj_death(s, (IDX));                                                   \
    if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[IDX]);        \
    if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[IDX]);                \
    atp_wmo_defer_or_remove(s, &wd, s->r_uid[IDX]);                           \
    if (!s->r_orient[IDX]) s->n_unorient--;                                   \
    for (u32 _j = (IDX) + 1; _j < s->n_rules; _j++) {                         \
      s->lhs[_j - 1]      = s->lhs[_j];                                       \
      s->rhs[_j - 1]      = s->rhs[_j];                                       \
      s->r_trace[_j - 1]  = s->r_trace[_j];                                   \
      s->r_uid[_j - 1]    = s->r_uid[_j];                                     \
      atp_uid_idx_set(s, s->r_uid[_j], _j - 1);                               \
      s->r_orient[_j - 1] = s->r_orient[_j];                                  \
      s->r_dead[_j - 1]           = s->r_dead[_j];                            \
      s->r_dead_lhs_save[_j - 1]  = s->r_dead_lhs_save[_j];                   \
      s->r_dead_rhs_save[_j - 1]  = s->r_dead_rhs_save[_j];                   \
      s->r_dead_subsumer_lhs[_j - 1] = s->r_dead_subsumer_lhs[_j];            \
      s->r_dead_subsumer_rhs[_j - 1] = s->r_dead_subsumer_rhs[_j];            \
      s->r_rederive_cut[_j - 1]      = s->r_rederive_cut[_j];                 \
      s->r_gj_status[_j - 1]      = s->r_gj_status[_j];                       \
      s->r_overlap_done[_j - 1] = s->r_overlap_done[_j];                      \
      /* Formation depth rides the slot (Twee getDepth); without the   */     \
      /* shift every rule above a collapse reads its NEIGHBOR's depth  */     \
      /* and the TWEE weight mode scores garbage (the LAT224-10 depth  */     \
      /* ladder).  r_gd is shifted by the open-coded loops but was     */     \
      /* MISSING here too -- same misalignment class for spb=goal.     */     \
      s->r_depth[_j - 1] = s->r_depth[_j];                                    \
      s->r_gd[_j - 1]    = s->r_gd[_j];                                       \
      WM_IR_SHIFT_FT(_j)                                                      \
    }                                                                         \
    s->n_rules--;                                                             \
    s->r_revision++;                                                          \
    ATP_RULE_INDEX_DIRTY();                                                   \
    dropped++;                                                                \
  } while (0)

  // ===============================================================
  // LHS-COLLAPSE DROP (Phase A E-LHS face + Phase B R-LHS).
  //
  // Faithful transcription of the legacy LHS-collapse block
  // (_.c:13412-13500).  `IDX` is the enclosing loop's slot var; sets
  // `did_drop`.  Under use_wm_demote the slice reduction is DETECTION
  // ONLY (WM NF_ObjektAnwendbar, Interreduktion.c:291/322): the reduct is
  // discarded, the victim re-enters with ORIGINAL sides.
  // ===============================================================
#define WM_IR_DROP_SLOT(IDX)                                                  \
  do {                                                                        \
    Term  _old_lhs = s->lhs[IDX];                                             \
    Term  _old_rhs = s->rhs[IDX];                                             \
    Term  _reduced;                                                           \
    u32   _simplify_parent = s->r_trace[IDX];                                 \
    if (s->record_norm_steps && !s->use_wm_demote) {                          \
      _reduced = atp_rewrite_normalize_slice_record(                          \
          s, _old_lhs, new_lhs, new_rhs, new_traces, n_new, 16,               \
          &_simplify_parent, 0u, _old_rhs);                                   \
    } else {                                                                  \
      _reduced = atp_rewrite_normalize(s, _old_lhs, new_lhs, new_rhs,         \
                                       n_new, 16);                            \
    }                                                                         \
    if (!s->keep_lhs_reducible && !kbo_eq(_reduced, _old_lhs)) {              \
      /* LHS reducible -> drop + requeue (WM PU_REObjektEinfuegen then    */  \
      /* the IR_PufferAuslesen drain re-derives ORIGINAL sides).          */  \
      if (s->use_wm_demote) {                                                 \
        u32 _wmo_key = s->use_emission_order                                  \
            ? atp_wmo_victim_drain_key(s, s->r_uid[IDX], 0u) : 0u;            \
        atp_irv_stage_ft(s, (IDX), 1u);                                       \
        atp_irv_push(s, _old_lhs, _old_rhs, _simplify_parent, _wmo_key);      \
      } else {                                                                \
        g_enq_gd_pending = (s->r_gd != NULL) ? s->r_gd[IDX] : 0u;             \
        g_enq_depth_pending = (s->r_depth != NULL) ? s->r_depth[IDX] : 0u;    \
        atp_add_equation_simplified(s, _reduced, _old_rhs, _simplify_parent); \
      }                                                                       \
      if (atp_rule_trace_on()) {                                             \
        char _la[2048];                                                       \
        atp_pretty_term(s->lhs[IDX], _la, sizeof _la);                        \
        fprintf(stderr, "  RETIRE rule (slot %u, trace %u): LHS %s "          \
                "collapsed; re-queued for re-orientation [WM]\n",             \
                (IDX), s->r_trace[IDX], _la);                                 \
      }                                                                       \
      ATP_ORPHAN_KILL_COLLECT(IDX)                                            \
      WM_IR_COMPACT_SLOT(IDX);                                                \
      did_drop = 1u;                                                          \
    } else {                                                                  \
      did_drop = 0u;                                                          \
    }                                                                         \
  } while (0)

  // ===============================================================
  // E-MEMBER RHS-FACE DROP (Phase A second-direction GMInterred).
  //
  // FIX-1 (review D1): WM's RE_Gleichungsbaum indexes BOTH directed twins
  // of every equation (RUndEVerwaltung.h:239-240), so GMInterred's
  // NF_ObjektAnwendbar(Objekt, TP_LinkeSeite) is applied to each
  // direction's LHS -- i.e. to BOTH the stored LHS and the stored RHS of
  // an unorientable E-member.  thvm stores one slot per equation, so the
  // RHS face is tested separately HERE.  Faithful transcription of the
  // legacy pass-2 `rhs_changed` block (_.c:13752-13808), E-members only,
  // gated on use_rhs_interreduce (the existence test against the new
  // object alone).  Drop + requeue ORIGINAL sides under use_wm_demote.
  // `IDX` is the enclosing loop's slot var; sets `did_drop`.
  //
  // Only reached for slots that survived WM_IR_DROP_SLOT (LHS face not
  // reducible), so the two never double-drop the same slot.
  // ===============================================================
#define WM_IR_DROP_SLOT_RHS(IDX)                                              \
  do {                                                                        \
    did_drop = 0u;                                                            \
    if (s->use_rhs_interreduce && !s->r_dead[IDX]) {                          \
      Term _old_lhs = s->lhs[IDX];                                            \
      Term _old_rhs = s->rhs[IDX];                                            \
      Term _reduced = atp_rewrite_normalize(s, _old_rhs, new_lhs, new_rhs,    \
                                            n_new, 16);                       \
      if (!kbo_eq(_reduced, _old_rhs)) {                                      \
        u32 _simplify_parent = s->r_trace[IDX];                              \
        if (s->use_wm_demote) {                                               \
          /* The RHS face (thvm side 1) is the reducible one here. */         \
          u32 _wmo_key = s->use_emission_order                               \
              ? atp_wmo_victim_drain_key(s, s->r_uid[IDX], 1u) : 0u;          \
          atp_irv_stage_ft(s, (IDX), 2u);                                     \
          atp_irv_push(s, _old_lhs, _old_rhs, _simplify_parent, _wmo_key);    \
        } else {                                                              \
          g_enq_gd_pending = (s->r_gd != NULL) ? s->r_gd[IDX] : 0u;           \
          g_enq_depth_pending = (s->r_depth != NULL) ? s->r_depth[IDX] : 0u;  \
          atp_add_equation_simplified(s, _old_lhs, _reduced,                  \
                                      _simplify_parent);                      \
        }                                                                     \
        if (atp_rule_trace_on()) {                                           \
          char _la[2048];                                                     \
          atp_pretty_term(_old_lhs, _la, sizeof _la);                         \
          fprintf(stderr, "  RETIRE eq-rhs (slot %u, trace %u): %s [WM]\n",   \
                  (IDX), s->r_trace[IDX], _la);                               \
        }                                                                     \
        ATP_ORPHAN_KILL_COLLECT(IDX)                                          \
        WM_IR_COMPACT_SLOT(IDX);                                              \
        did_drop = 1u;                                                        \
      }                                                                       \
    }                                                                         \
  } while (0)

  // FT-slot shift, only present under THVM_ATPFT_RULES (Stage 4).
#ifdef THVM_ATPFT_RULES
#  define WM_IR_SHIFT_FT(J)                                                   \
      s->lhs_ft[(J) - 1] = s->lhs_ft[(J)];                                    \
      s->rhs_ft[(J) - 1] = s->rhs_ft[(J)];                                    \
      s->r_dead_lhs_save_ft[(J) - 1] = s->r_dead_lhs_save_ft[(J)];            \
      s->r_dead_rhs_save_ft[(J) - 1] = s->r_dead_rhs_save_ft[(J)];            \
      s->r_dead_subsumer_lhs_ft[(J) - 1] = s->r_dead_subsumer_lhs_ft[(J)];    \
      s->r_dead_subsumer_rhs_ft[(J) - 1] = s->r_dead_subsumer_rhs_ft[(J)];
#else
#  define WM_IR_SHIFT_FT(J) /* no AtpFt mirror */
#endif

#ifdef ATP_ORPHAN_KILL
#  define ATP_ORPHAN_KILL_COLLECT(IDX)                                        \
      if (atp_dead != NULL && s->r_trace[IDX] != ATP_TRACE_NONE) {            \
        atp_dead[atp_n_dead++] = s->r_trace[IDX];                             \
      }
#else
#  define ATP_ORPHAN_KILL_COLLECT(IDX) /* orphan-kill not built */
#endif

#ifdef ATP_RULE_INDEX
#  define ATP_RULE_INDEX_DIRTY() do { s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u; } while (0)
#else
#  define ATP_RULE_INDEX_DIRTY() do { } while (0)
#endif

  // Bounded-transient GC re-read of the new-rule Terms after a collection
  // (see the legacy loop's note at _.c:13362-13376).  Used at each phase
  // loop head.  After GC the new rules sit at the compacted slot
  // `added.first - dropped`.
#define WM_IR_MAYBE_GC()                                                      \
  do {                                                                        \
    if (s->trace_pack &&                                                      \
        (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {                      \
      thvm_atp_gc_collect(s);                                                 \
      for (u32 _k = 0; _k < n_new; _k++) {                                    \
        new_lhs[_k] = s->lhs[added.first - dropped + _k];                     \
        new_rhs[_k] = s->rhs[added.first - dropped + _k];                     \
      }                                                                       \
    }                                                                         \
  } while (0)

  // ===============================================================
  // PHASE A -- GMInterred (Interreduktion.c:285-307).
  // Iterate OLD E-members (slots i < added.first - dropped, !r_orient[i]).
  // WM indexes both directed twins, so an E-member leaves E for the IR
  // buffer if EITHER face is reducible by the new object: test the LHS
  // face first (WM_IR_DROP_SLOT), then -- if it survived -- the RHS face
  // (WM_IR_DROP_SLOT_RHS, FIX-1).  R-members are skipped here (Phase B).
  // The drain re-derives the ORIGINAL sides in both cases.
  // ===============================================================
  g_thvm_phase = "ir_wm_phaseA";
  {
    u32 i = 0;
    while (i < added.first - dropped) {
      WM_IR_MAYBE_GC();
      if (s->r_orient[i]) { i++; continue; }   // R-member: defer to Phase B
      u8 did_drop = 0u;
      WM_IR_DROP_SLOT(i);                 // LHS face
      if (!did_drop) WM_IR_DROP_SLOT_RHS(i);  // RHS face (second twin)
      if (!did_drop) i++;   // a drop shifts the next member down into slot i
    }
  }

  // ===============================================================
  // PHASE B -- RMLinksInterred (Interreduktion.c:314-340).
  // Iterate OLD R-members (oriented slots) whose LHS is reducible by the
  // new object.  Same LHS-collapse drop+requeue.  (rechts_IR_nach_P
  // default FALSE, so we test ONLY the LHS face -- exactly as the legacy
  // fn does.  An oriented rule's RHS is COMPOSED in place in Phase C, not
  // tested for a drop: WM -irrp default FALSE = "modify rule".)
  // ===============================================================
  g_thvm_phase = "ir_wm_phaseB";
  {
    u32 i = 0;
    while (i < added.first - dropped) {
      WM_IR_MAYBE_GC();
      if (!s->r_orient[i]) { i++; continue; } // E-member: handled in Phase A
      u8 did_drop = 0u;
      WM_IR_DROP_SLOT(i);
      if (!did_drop) i++;
    }
  }

  // ===============================================================
  // PHASE C -- RMRechtsInterred (Interreduktion.c:343-374), !rechts_IR_nach_P.
  // The new rule is already live in R (RE_FaktumEinfuegen precedes
  // IR_InterreduktionRechts).  For every SURVIVING ORIENTED rule (!= any
  // new object), normalize its RHS in place against the new object and,
  // when use_rhs_interreduce, finish to full R+E normal form.  RULES
  // ONLY -- E-member RHS faces are handled by the DROP path in Phase A
  // (FIX-1), not composed here.  No drop, no CP in this phase.
  //
  // This mirrors the legacy right-reduction block (_.c:13502-13638)
  // verbatim, restricted to oriented live rules and skipping the NEW
  // rules themselves (the WM `Objekt != BehRegel` guard, :353/364).
  // ===============================================================
  if (s->right_reduce) {
    g_thvm_phase = "ir_wm_phaseC";
    u32 new_first = added.first - dropped;   // first NEW-rule slot post-drops
    for (u32 i = 0; i < s->n_rules; i++) {
      WM_IR_MAYBE_GC();
      // WM `Objekt != BehRegel`: never compose ANY new rule against itself
      // or a sibling.  FIX-2: use n_new_slots (= added.count, un-clamped)
      // so all FVI siblings are covered, not just the first two.  Phase C
      // never drops, so added.first - dropped is invariant here and
      // new_first stays valid across WM_IR_MAYBE_GC re-reads.
      if (i >= new_first && i < new_first + n_new_slots) continue;
      // RMRechtsInterred composes ORIENTED rules only (the Regelbaum
      // walk RE_forRegelnRobust).  Skip unorientable E-members and dead
      // (backward-subsumed) slots.
      if (!s->r_orient[i] || s->r_dead[i]) continue;

      Term old_lhs = s->lhs[i];
      Term old_rhs = s->rhs[i];
      Term r_reduced = old_rhs;
      u32  rr_parent = s->r_trace[i];

      if (s->use_rhs_interreduce) {
        // step_cap 1 against the new-object slice (WM ObjektAngewendet:
        // one application of the new object anywhere in the RHS), then
        // full NF (NF_NormalformstRE = doR=doE=TRUE over live R+E).
        if (s->record_norm_steps) {
          Term stepped = atp_rewrite_normalize_slice_record(
              s, old_rhs, new_lhs, new_rhs, new_traces, n_new, 1u,
              &rr_parent, 1u, old_lhs);
          if (!kbo_eq(stepped, old_rhs)) {
            g_thvm_phase = "ir_wm_rhs64_rec";
            r_reduced = atp_rewrite_normalize_record(
                s, stepped, old_lhs, 1u, &rr_parent, 64u);
          }
        } else {
          Term stepped = atp_rewrite_normalize(s, old_rhs, new_lhs,
                                               new_rhs, n_new, 1u);
          if (!kbo_eq(stepped, old_rhs)) {
            g_thvm_phase = "ir_wm_rhs64_all";
            r_reduced = atp_rewrite_normalize(s, stepped, s->lhs,
                                              s->rhs, s->n_rules, 64u);
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
        // Orientation guard l > r' must still hold; legacy size guard for
        // the non-rhs-interreduce (slice-only) composition path.  Under
        // use_rhs_interreduce the size guard is bypassed (matches WM,
        // which commits NF_NormalformstRE without a size re-check); the
        // KBO_GT guard is a tautology there (NF of old_rhs < old_lhs stays
        // < old_lhs) and only ever protects against a pathological order.
        if (atp_compare(s, old_lhs, r_reduced) == KBO_GT &&
            (s->use_rhs_interreduce ||
             atp_symbol_count(r_reduced) <= atp_symbol_count(old_rhs))) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          // Sentinel compose: both records' eq pairs pack off the spans
          // (the bracket is consume-once -- arm per push).
          if (g_ir_rhs_nf_ft != NULL && s->lhs_ft[i] != NULL) {
            g_trace_eq_ft_l = s->lhs_ft[i];
            g_trace_eq_ft_r = g_ir_rhs_nf_ft;
          }
#endif
          u32 new_t = atp_trace_push(s, TRACE_ORIENT, rr_parent,
                                     ATP_TRACE_NONE, old_lhs, r_reduced);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          if (g_ir_rhs_nf_ft != NULL && s->lhs_ft[i] != NULL) {
            g_trace_eq_ft_l = s->lhs_ft[i];
            g_trace_eq_ft_r = g_ir_rhs_nf_ft;
          }
#endif
          atp_trace_push(s, TRACE_SIMPLIFY, s->r_trace[i],
                         ATP_TRACE_NONE, old_lhs, r_reduced);
          s->rhs[i] = r_reduced;
          // WM order mirror: NO update.  The mirror keys facts by birth
          // uid (r_uid, stable across the RHS compose) and indexes the
          // rule's LHS face only, so the in-place RHS edit leaves its
          // tree identity untouched -- exactly WM's RMRechtsInterred
          // (Interreduktion.c:329-360), which right-reduces the rule in
          // place without touching the DSBaum.  A former
          // atp_wmo_rename_trace(s, s->r_trace[i], new_t) here renamed
          // in TRACE space over the uid-keyed chains: whenever
          // r_trace[i] numerically collided with a LIVE rule's uid it
          // hijacked that rule's mirror identity, silently dropping it
          // from the CP-partner index (BooleanAxioms OrAssociativity
          // @300: r_trace[slot10]=51 collided with uid 51 = WM rule 45,
          // whose or(x,and(x,y)) CP family then never formed).
          s->r_trace[i] = new_t;
          atp_rj_content(s, i);
          s->n_right_reduced++;
          if (atp_rule_trace_on()) {
            char la[2048], ra[2048];
            atp_pretty_term(old_lhs, la, sizeof la);
            atp_pretty_term(r_reduced, ra, sizeof ra);
            fprintf(stderr, "  COMPOSE rule (slot %u): %s -> %s [WM]\n",
                    i, la, ra);
          }
#ifdef THVM_ATPFT_RULES
          if (g_ir_rhs_nf_ft != NULL &&
              (term_tag(r_reduced) == TAG_NUM ||
               ft_eq_term(g_ir_rhs_nf_ft, r_reduced))) {
            s->rhs_ft[i] = g_ir_rhs_nf_ft;   // adopt the composed NF span
          } else {
            s->rhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                        r_reduced, 0);
          }
          g_ir_rhs_nf_ft = NULL;
          atp_ft_rules_verify_push(s, i);
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          // WM-policy FT mirror: the RHS compose is the one rule-set
          // mutation without an r_revision bump, so the flat splice
          // template must be re-encoded here (see atp_wf_mm_note_rhs_edit)
          // and the simpler-join rule-encode cache slot retired.
          atp_wf_mm_note_rhs_edit(s, i);
          {
            extern void thvm_ftsj_rulecache_note_edit(u32 slot);
            thvm_ftsj_rulecache_note_edit(i);
          }
#endif
#ifdef ATP_RULE_INDEX
          if (!wf_eng_cache_update_rhs(s, i)) s->wmfpa_dirty = 1u;
#endif
        }
      }
    }
  }

  // Snapshot flush: evict the buffered victims' wmo faces now that every
  // drain key was ranked against the pass-start tree (see AtpWmoDefer).
  atp_wmo_defer_flush(s, &wd);

#ifdef ATP_ORPHAN_KILL
  g_thvm_phase = "ir_wm_orphan";
  if (atp_dead != NULL) {
    if (atp_n_dead > 0) atp_cp_kill_orphans(s, atp_dead, atp_n_dead);
    free(atp_dead);
  }
#endif

#undef WM_IR_DROP_SLOT
#undef WM_IR_DROP_SLOT_RHS
#undef WM_IR_COMPACT_SLOT
#undef WM_IR_SHIFT_FT
#undef ATP_ORPHAN_KILL_COLLECT
#undef ATP_RULE_INDEX_DIRTY
#undef WM_IR_MAYBE_GC
  return dropped;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT-native interreduction reduct: re-encode `subj` to a fresh flat tree
// and normalize it against ONLY the live rule slice
// [slice_first, slice_first + slice_count) via the outermost-leftmost FT
// loop -- the byte-for-byte equivalent of the Term-side
// atp_rewrite_normalize (atp_ordered_try_top order) the legacy IC
// interreduce used, but with the order gate applied on the FLAT subject
// (thvm_kbo_ft) instead of a per-redex IC atp_compare over a re-
// materialised deep Term.  On the ifeq / deeply-nested workloads that
// path was ~90% of the completion wall (interreduce -> IC normalize ->
// atp_ordered_try_top -> atp_compare).  The slice's FT patterns live in
// s->lhs_ft / s->rhs_ft, shifted in lockstep with R through interreduce
// compaction (RE_RegelEntfernen), and the new-rule slice is never
// modified in-loop, so the mirror stays valid across the pass.
static Term atp_ir_normalize_ft_slice(AtpState *s, Term subj,
                                      const AtpFtCell *subj_ft,
                                      u32 slice_first, u32 slice_count,
                                      u32 step_cap) {
  // Fast path: the caller passes the row's live FT mirror (lockstep
  // with subj by the THVM_ATPFT_RULES invariant).  A read-only probe
  // proving no slice face matches anywhere in the mirror proves the
  // slice normalize is the identity -- return subj itself, ZERO
  // ft_from_term/ft_to_term.  In interreduce the overwhelming majority
  // of rows are untouched by the new-rule slice, so the conversion
  // round-trip below runs only for actual reductions (plus the rare
  // order-gated false positive, which normalizes to an equal NF).
  if (subj_ft != NULL &&
      !atp_ft_slice_fires(s, subj_ft, slice_first, slice_count)) {
    return subj;
  }
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Probe hit (a real reduction): the destructive normalize gets a
  // CLONE of the row's mirror -- bit-identical to the ft_from_term
  // re-encode, no Term hop.  ft_to_term of the reduced NF is the
  // genuine Term-spine boundary (the reduct enters R as a Term).
  AtpFtCell *sub = (subj_ft != NULL) ? ft_clone_span(a, subj_ft, 0)
                                     : ft_from_term(a, subj, 0);
  AtpFtCell *nf  = atp_rewrite_normalize_ft_slice(s, sub, slice_first,
                                                  slice_count, step_cap);
  // Order-gated probe false positive: the normalize made no step (NF
  // equals the pristine mirror) -- the callers' kbo_eq(reduced, subj)
  // sees equality either way, so return subj itself and skip the
  // decode.  Only a REAL reduction pays ft_to_term.
  if (subj_ft != NULL && ft_eq(nf, subj_ft)) return subj;
  return ft_to_term(nf);
}

// Detection-only sibling: does the slice reduce the mirror at all?
// Zero decode -- WM's NF_ObjektAnwendbar existence test for the demote
// path, where the reduct is discarded and the victim re-enters with
// its ORIGINAL sides.
static u8 atp_ir_slice_reduces_ft(AtpState *s, const AtpFtCell *subj_ft,
                                  u32 slice_first, u32 slice_count,
                                  u32 step_cap) {
  if (!atp_ft_slice_fires(s, subj_ft, slice_first, slice_count)) return 0;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *sub = ft_clone_span(a, subj_ft, 0);
  AtpFtCell *nf  = atp_rewrite_normalize_ft_slice(s, sub, slice_first,
                                                  slice_count, step_cap);
  return (u8)!ft_eq(nf, subj_ft);
}
// THVM_ATP_IR_FT=0 forces the legacy IC interreduce normalize; default ON.
static int atp_ir_ft_on(void) {
  static int c = -1;
  if (c < 0) { const char *e = getenv("THVM_ATP_IR_FT");
    c = (e != NULL && e[0] == '0') ? 0 : 1; }
  return c;
}
// THVM_ATP_IR_FT_DIFF=1 runs BOTH normalizers and aborts on any NF
// divergence -- the byte-parity self-check that certifies the FT path.
static int atp_ir_ft_diff(void) {
  static int c = -1;
  if (c < 0) c = (getenv("THVM_ATP_IR_FT_DIFF") != NULL) ? 1 : 0;
  return c;
}
#endif

// ── Twee full-R interreduction (Twee.hs:774 interreduce/interreduce1) ──
// Re-test each live oriented rule for JOINABILITY against the whole current
// rule set EXCLUDING itself: if the rule's two sides both normalize to the
// SAME term under R\{i}, its equation is a consequence of the other rules
// (which arrived across many steps -- the incremental thvm_atp_interreduce
// never re-checks this), so the rule is redundant and gets DELETED (Twee's
// interreduce1 `Right` branch -> deleteActive).  thvm ALREADY keeps LHSs
// left-reduced, so a plain "is the LHS reducible" test is a no-op; the
// useful redundancy is full-sides-join.  Uses the TESTED joinability oracle
// atp_cp_trivially_joinable (the same one the push loop uses to drop
// joinable CPs -- it handles the shared-variable namespace correctly, so
// the earlier hand-rolled two-normalize + var-255 exclusion, which produced
// a spurious universal `X=Y`, is gone).  Self-exclusion is r_dead[i]=1 (the
// FT normalize skips r_dead rules, ft_norm.c:655/...).  Deletion is a pure
// soft-delete (no requeue: the equation is already derivable from R\{i}).
// Opt-in (use_full_interreduce); the WM preset never sets it, so test_atp
// byte-parity is preserved.  Requires THVM_ATP_JOIN_CACHE off (default) --
// the cache is keyed on n_rules, which the r_dead toggle does not change.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static void thvm_atp_interreduce_full(AtpState *s) {
  if (s == NULL || s->n_rules < 2u) return;
  Term dead_sentinel = term_new(0, TAG_FVR, 255u, 0);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *sent_ft = ftnew_var(a, 255u, 0);
  u32 n = s->n_rules;   // snapshot
  for (u32 i = 0; i < n; i++) {
    if (s->r_dead[i]) continue;
    if (!s->r_orient[i]) continue;     // oriented rules only
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    // Copies: atp_cp_trivially_joinable mutates its args to the NFs on the
    // not-joined branch; keep the rule's stored faces untouched.
    Term l = old_lhs, r = old_rhs;
    AtpFtCell *save_lft = s->lhs_ft[i], *save_rft = s->rhs_ft[i];
    s->r_dead[i] = 1;                              // exclude rule i
    u8 joined = atp_cp_trivially_joinable(s, &l, &r);
    s->r_dead[i] = 0;
    if (!joined) continue;                         // still an essential rule
    // Sides join under R\{i} => the equation is derivable from the others
    // => rule i is redundant.  Delete it (soft-delete, dead-sentinel recipe).
    if (atp_rule_trace_on()) {
      char la[8192], ra[8192];
      atp_pretty_term(old_lhs, la, sizeof la);
      atp_pretty_term(old_rhs, ra, sizeof ra);
      fprintf(stderr, "  FULL-IR delete (slot %u, joinable): %s = %s\n",
              i, la, ra);
    }
    atp_rj_death(s, i);
    s->r_dead_lhs_save[i] = old_lhs;   // sentinel rides; save_ft authoritative
    s->r_dead_rhs_save[i] = old_rhs;
    s->lhs[i] = dead_sentinel;  s->rhs[i] = dead_sentinel;
    s->r_dead[i] = 1;
    s->r_dead_lhs_save_ft[i] = save_lft;
    s->r_dead_rhs_save_ft[i] = save_rft;
    s->lhs_ft[i] = sent_ft;  s->rhs_ft[i] = sent_ft;
    s->r_revision++;
    s->n_full_ir_collapsed++;
    if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[i]);
    if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[i]);
    if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_uid[i]);
#ifdef ATP_RULE_INDEX
    s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
  }
}
#endif

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Function-symbol set of a flat span, one bit per label mod 64.  A
// pattern can match inside a term only if the term carries every
// function symbol of the pattern, so a missing bit proves no redex --
// the Otter replay's per-adoption interreduce scan skips on it.
static u64 atp_ft_symmask(const AtpFtCell *c) {
  u64 m = 0u;
  const AtpFtCell *e = c->end->next;
  for (; c != e; c = c->next) {
    if ((c->sym & WF_VAR_BIT) == 0u) m |= (u64)1 << (c->sym & 63u);
  }
  return m;
}
#endif

fn u32 thvm_atp_interreduce(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0 || added.first == 0) return 0;
  // Vampire bd=preordered (EqHelper::getDemodulationLHSIterator with
  // onlyPreordered): only an equation the ordering orients demodulates
  // backward.  An unorientable new equation (commutativity, the AC
  // permutation laws) would otherwise ordered-rewrite older rules --
  // e.g. retire -X+X -> 0 into X+-X under commutativity -- and orphan
  // their queued descendants.
  if (s->use_bd_preordered) {
    for (u32 k = 0; k < added.count; k++) {
      if (!s->r_orient[added.first + k]) return 0;
    }
  }
  // SENTINEL_STRICT localization: report any rule slot still holding a
  // lazy sentinel at interreduce entry (post materialize-at-birth this
  // must be empty; a hit names the bypassing store path).
  {
    static int sst = -1;
    if (sst < 0) {
      const char *e = getenv("THVM_ATP_SENTINEL_STRICT");
      sst = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
    }
    if (sst) {
      for (u32 q = 0; q < s->n_rules; q++) {
        if (term_tag(s->lhs[q]) == TAG_NUM || term_tag(s->rhs[q]) == TAG_NUM) {
          fprintf(stderr, "SENTINEL-STRICT: slot %u holds sentinel at "
                          "interreduce entry (lhs=%u rhs=%u)\n",
                  q, term_tag(s->lhs[q]), term_tag(s->rhs[q]));
        }
      }
    }
  }
  atp_rj_slice(s, added.first, added.count > 2u ? 2u : added.count);
  if (s->use_wm_interred) {
    u32 wm_dropped = thvm_atp_interreduce_wm(s, added);
    atp_rj_slice(s, 0u, 0u);
    return wm_dropped;
  }
  // Pass-start drain-key snapshot (see AtpWmoDefer above the _wm fn).
  AtpWmoDefer wd = {0};
  wd.active = (s->use_wm_demote && s->use_emission_order &&
               atp_drain_snapshot_on()) ? 1u : 0u;
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // FT interreduce reads the new-rule slice's flat patterns (s->lhs_ft /
  // s->rhs_ft); make sure the mirror is materialised before the pass.
  // A no-op when already current; the in-loop compaction keeps it valid
  // (lockstep slot shift), so this single ensure suffices.
  if (atp_ir_ft_on()) atp_ft_mirror_ensure(s);
  // Otter replay: symbol-subset prefilter over the new rules' pattern
  // faces (the LHS; both faces of an unorientable rule).  Skips exactly
  // the older rules no pattern can match in, so the pass is unchanged.
  u64 ir_pm[4];
  u32 ir_npm = 0u;
  u8  ir_skip = 0u;
  if (s->use_otter) {
    ir_skip = 1u;
    for (u32 k = 0; k < n_new && ir_skip; k++) {
      u32 r = added.first + k;
      if (s->lhs_ft[r] == NULL || s->rhs_ft[r] == NULL) { ir_skip = 0u; break; }
      ir_pm[ir_npm++] = atp_ft_symmask(s->lhs_ft[r]);
      if (!s->r_orient[r]) ir_pm[ir_npm++] = atp_ft_symmask(s->rhs_ft[r]);
    }
  }
#endif
  while (i < added.first - dropped) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (ir_skip && s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL) {
      u64 ml = atp_ft_symmask(s->lhs_ft[i]);
      u64 mr = s->right_reduce ? atp_ft_symmask(s->rhs_ft[i]) : 0u;
      u8 may = 0u;
      for (u32 q = 0; q < ir_npm && !may; q++) {
        if ((ir_pm[q] & ~ml) == 0u) may = 1u;
        if (s->right_reduce && (ir_pm[q] & ~mr) == 0u) may = 1u;
      }
      if (!may) { i++; continue; }
    }
#endif
    // Bound the interreduce transient: this loop runs O(n_rules) normalizes
    // whose scratch is not reclaimed until step end, so on deep completions
    // one interreduce pass alone overruns the from-space.  At the iteration
    // boundary the only live Terms are the GC-rooted rule/trace arrays plus
    // new_lhs/new_rhs (local copies), so a Cheney collection here is safe as
    // long as we re-read the relocated new-rule Terms afterwards.  Gated on
    // trace_pack (the off-heap-trace path that makes deep runs reachable).
    if (s->trace_pack &&
        (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {
      thvm_atp_gc_collect(s);
      for (u32 k = 0; k < n_new; k++) {
        new_lhs[k] = s->lhs[added.first - dropped + k];
        new_rhs[k] = s->rhs[added.first - dropped + k];
      }
    }
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    { static int dl = -1; static u64 mark = 0;
      if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
      if (dl) { u64 d = HEAP_NEXT - mark;
        u64 used = HEAP_NEXT - gc_from_start();
        if (d > 500000ull || used > (100ull<<20)) fprintf(stderr,
          "IRLOG i=%u lhs=%u rhs=%u used=%lluM delta=%lluK n_rules=%u n_new=%u\n",
          i, atp_symbol_count(old_lhs), atp_symbol_count(old_rhs),
          (unsigned long long)(used >> 20), (unsigned long long)(d >> 10),
          s->n_rules, n_new);
        mark = HEAP_NEXT; } }
    Term reduced;
    u8 lhs_red_known = 0u, lhs_red = 0u;
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      if (atp_ir_ft_on() && s->use_wm_demote && !s->keep_lhs_reducible &&
          !atp_ir_ft_diff() && s->lhs_ft[i] != NULL) {
        // Demote = detection only (the reduct is discarded; the victim
        // re-enters with ORIGINAL sides): changed-flag on the mirror,
        // zero decode.
        lhs_red_known = 1u;
        lhs_red = atp_ir_slice_reduces_ft(s, s->lhs_ft[i],
                                          added.first - dropped, n_new, 16);
        reduced = old_lhs;   // unread on the demote branch
      } else if (atp_ir_ft_on()) {
        Term red_ft = atp_ir_normalize_ft_slice(s, old_lhs, s->lhs_ft[i],
                                                added.first - dropped,
                                                n_new, 16);
        if (atp_ir_ft_diff()) {
          Term red_ic = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs,
                                              n_new, 16);
          if (!kbo_eq(red_ft, red_ic)) {
            char sb[4096], fb[4096], ib[4096];
            atp_pretty_term(old_lhs, sb, sizeof sb);
            atp_pretty_term(red_ft,  fb, sizeof fb);
            atp_pretty_term(red_ic,  ib, sizeof ib);
            fprintf(stderr, "[IR_FT DIFF lhs] slice=[%u,%u)\n subj=%s\n"
                    " ft=%s\n ic=%s\n", added.first - dropped, n_new,
                    sb, fb, ib);
            abort();
          }
        }
        reduced = red_ft;
      } else
#endif
      reduced = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs,
                                      n_new, 16);
    }
    if (!s->keep_lhs_reducible &&
        (lhs_red_known ? lhs_red : (u8)!kbo_eq(reduced, old_lhs))) {
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
            ? atp_wmo_victim_drain_key(s, s->r_uid[i], 0u) : 0u;
        atp_irv_stage_ft(s, i, 3u);
        atp_irv_push(s, old_lhs, old_rhs, simplify_parent, wmo_key);
      } else {
        g_enq_gd_pending = (s->r_gd != NULL) ? s->r_gd[i] : 0u;
        g_enq_depth_pending = (s->r_depth != NULL) ? s->r_depth[i] : 0u;
        atp_add_equation_simplified(s, reduced, old_rhs, simplify_parent);
      }
      if (atp_rule_trace_on()) {
        char la[8192], ra[8192];
        atp_pretty_term(s->lhs[i], la, sizeof la);
        atp_pretty_term(s->rhs[i], ra, sizeof ra);
        fprintf(stderr, "  RETIRE rule (slot %u, trace %u): LHS %s = RHS %s collapsed; "
                "re-queued for re-orientation\n", i, s->r_trace[i], la, ra);
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
      if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[i]);
      // WM order mirror: the fact left R/E (RE_RegelEntfernen).
      // Snapshot-deferred so later victims' drain keys rank against the
      // pass-start leaf list (see AtpWmoDefer).
      atp_wmo_defer_or_remove(s, &wd, s->r_uid[i]);
      atp_rj_death(s, i);
      // Keep the unorientable-rule count live: the dropped rule leaves
      // R here (it re-enters as a queued equation, re-counted only if
      // re-oriented unorientable at its next atp_push_rule).
      if (!s->r_orient[i]) s->n_unorient--;
      for (u32 j = i + 1; j < s->n_rules; j++) {
        s->lhs[j - 1]      = s->lhs[j];
        s->rhs[j - 1]      = s->rhs[j];
        s->r_trace[j - 1]  = s->r_trace[j];
        s->r_uid[j - 1]    = s->r_uid[j];
        atp_uid_idx_set(s, s->r_uid[j], j - 1);
        s->r_orient[j - 1] = s->r_orient[j];
        s->r_gd[j - 1]     = s->r_gd[j];
        // Formation depth rides the slot (Twee getDepth) -- see
        // WM_IR_COMPACT_SLOT for the misalignment this shift fixes.
        s->r_depth[j - 1]  = s->r_depth[j];
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
        s->r_overlap_done[j - 1] = s->r_overlap_done[j];
#ifdef THVM_ATPFT_RULES
        // Stage 4: shift the AtpFt slot pointers in lockstep -- no
        // re-conversion, the cells themselves are address-stable in
        // the slab pool, only the slot index changes.
        s->lhs_ft[j - 1] = s->lhs_ft[j];
        s->rhs_ft[j - 1] = s->rhs_ft[j];
        s->r_dead_lhs_save_ft[j - 1] = s->r_dead_lhs_save_ft[j];
        s->r_dead_rhs_save_ft[j - 1] = s->r_dead_rhs_save_ft[j];
        s->r_dead_subsumer_lhs_ft[j - 1] = s->r_dead_subsumer_lhs_ft[j];
        s->r_dead_subsumer_rhs_ft[j - 1] = s->r_dead_subsumer_rhs_ft[j];
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
                g_thvm_phase = "ir_rhs64_rec";
                r_reduced = atp_rewrite_normalize_record(
                    s, stepped, old_lhs, 1u, &rr_parent, 64u);
              }
            } else {
              // step_cap 1 = exactly one ordered rewrite against the
              // new-object slice (the ObjektAngewendet existence-and-
              // apply); 64 matches the engine's full-NF NORM_CAP.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
              if (atp_ir_ft_on() && s->use_mixmost_nf &&
                  s->rhs_ft[i] != NULL) {
                // FT composition pipeline: probe the slice on the RHS
                // mirror; on a hit run the 1-step slice rewrite + the
                // full-NF fixpoint on a mirror clone (the same FT
                // normalizers the Term route reaches via the mixmost
                // reroute), decode the committed NF ONCE, and stash
                // the span so the commit below adopts it as the new
                // mirror instead of re-encoding.
                if (atp_ft_slice_fires(s, s->rhs_ft[i],
                                       added.first - dropped, n_new)) {
                  AtpFt *cfa = (AtpFt *)s->ft_arena_ptr;
                  AtpFtCell *st = atp_rewrite_normalize_ft_slice(
                      s, ft_clone_span(cfa, s->rhs_ft[i], 0),
                      added.first - dropped, n_new, 1u);
                  if (!ft_eq(st, s->rhs_ft[i])) {
                    g_thvm_phase = "ir_rhs64_all";
                    atp_ft_mirror_ensure(s);
                    AtpFtCell *nf = atp_rewrite_normalize_ft(s, st, 64u);
                    // The composed RHS commits DECODED: a sentinel here
                    // lands in the victim's rhs slot outside any birth
                    // bracket -- one of the 15 audited raw-read hazards
                    // (materialize-at-birth policy, 2026-07-11).  The
                    // SIMPLIFY trace's eq pair still packs off the NF
                    // span, which the commit adopts as the mirror.
                    r_reduced = ft_to_term(nf);
                    g_ir_rhs_nf_ft = nf;   // commit adopts as the mirror
                  }
                }
              } else
#endif
              {
              Term stepped = atp_rewrite_normalize(s, old_rhs, new_lhs,
                                                   new_rhs, n_new, 1u);
              if (!kbo_eq(stepped, old_rhs)) {
                g_thvm_phase = "ir_rhs64_all";
                r_reduced = atp_rewrite_normalize(s, stepped, s->lhs,
                                                  s->rhs, s->n_rules, 64u);
              }
              }
            }
          }
        } else if (s->record_norm_steps) {
          r_reduced = atp_rewrite_normalize_slice_record(
              s, old_rhs, new_lhs, new_rhs, new_traces, n_new, 16,
              &rr_parent, 1u, old_lhs);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        } else if (atp_ir_ft_on()) {
          // FT-native RHS composition (same slice as the LHS reduction).
          r_reduced = atp_ir_normalize_ft_slice(s, old_rhs, s->rhs_ft[i],
                                                added.first - dropped,
                                                n_new, 16);
          if (atp_ir_ft_diff()) {
            Term r_ic = atp_rewrite_normalize(s, old_rhs, new_lhs, new_rhs,
                                              n_new, 16);
            if (!kbo_eq(r_reduced, r_ic)) {
              char sb[4096], fb[4096], ib[4096];
              atp_pretty_term(old_rhs,   sb, sizeof sb);
              atp_pretty_term(r_reduced, fb, sizeof fb);
              atp_pretty_term(r_ic,      ib, sizeof ib);
              fprintf(stderr, "[IR_FT DIFF rhs] slice=[%u,%u)\n subj=%s\n"
                      " ft=%s\n ic=%s\n", added.first - dropped, n_new,
                      sb, fb, ib);
              abort();
            }
          }
#endif
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
            if (g_ir_rhs_nf_ft != NULL && s->lhs_ft[i] != NULL) {
              g_trace_eq_ft_l = s->lhs_ft[i];
              g_trace_eq_ft_r = g_ir_rhs_nf_ft;
            }
#endif
            atp_trace_push(s, TRACE_SIMPLIFY, s->r_trace[i],
                           ATP_TRACE_NONE, old_lhs, r_reduced);
            s->rhs[i]     = r_reduced;
            // WM order mirror: NO update -- the mirror keys facts by
            // birth uid (stable across the compose) and indexes the LHS
            // face only; see the WM-path compose above for the trace-
            // space rename hazard this used to carry.
            s->r_trace[i] = new_t;
            atp_rj_content(s, i);
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
            // AtpFt mirror adopts the FT composition pipeline's NF
            // span when armed (ft_eq_term-proven), else re-converts.
            // (Old AtpFt RHS cells become unreachable in the arena,
            // freed wholesale at ft_destroy time.  LHS unchanged so
            // lhs_ft[i] stays.)
            if (g_ir_rhs_nf_ft != NULL &&
                (term_tag(r_reduced) == TAG_NUM ||
                 ft_eq_term(g_ir_rhs_nf_ft, r_reduced))) {
              s->rhs_ft[i] = g_ir_rhs_nf_ft;
            } else {
              s->rhs_ft[i] = ft_from_term((AtpFt *)s->ft_arena_ptr,
                                          r_reduced, 0);
            }
            g_ir_rhs_nf_ft = NULL;
            atp_ft_rules_verify_push(s, i);
#endif
            // RHS does not feed the LHS discrimination tree, so the
            // rule-LHS index stays valid; no dirty flag needed.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
            // WM-policy FT mirror: re-encode its flat splice template
            // (the compose does not bump r_revision, so the lazy
            // watermark alone would miss this edit); retire the
            // simpler-join rule-encode cache slot for the same reason.
            atp_wf_mm_note_rhs_edit(s, i);
            {
              extern void thvm_ftsj_rulecache_note_edit(u32 slot);
              thvm_ftsj_rulecache_note_edit(i);
            }
#endif
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
    g_thvm_phase = "ir_pass2";
    u32 j = 0;
    while (j < added.first - dropped) {
      // Same bounded-transient GC as the first interreduce pass (see there).
      { static int dl=-1; if(dl<0) dl=(getenv("THVM_ATP_NORMLOG")!=NULL)?1:0;
        if(dl){ u64 used = HEAP_NEXT - gc_from_start();
          if (used > (60ull<<20)) fprintf(stderr,
            "IR2 j=%u used=%lluM nr=%u press=%u\n", j,
            (unsigned long long)(used>>20), s->n_rules,
            (unsigned)atp_heap_under_pressure()); } }
      if (s->trace_pack &&
          (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {
        thvm_atp_gc_collect(s);
        for (u32 k = 0; k < n_new; k++) {
          new_lhs[k] = s->lhs[added.first - dropped + k];
          new_rhs[k] = s->rhs[added.first - dropped + k];
        }
      }
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
              ? atp_wmo_victim_drain_key(s, s->r_uid[j], 0u) : 0u;
          atp_irv_stage_ft(s, j, 4u);
          atp_irv_push(s, old_lhs, old_rhs, simplify_parent, wmo_key);
        } else {
          g_enq_gd_pending = (s->r_gd != NULL) ? s->r_gd[j] : 0u;
          g_enq_depth_pending = (s->r_depth != NULL) ? s->r_depth[j] : 0u;
          atp_add_equation_simplified(s, reduced_lhs, reduced,
                                      simplify_parent);
        }
#ifdef ATP_ORPHAN_KILL
        if (atp_dead != NULL && s->r_trace[j] != ATP_TRACE_NONE) {
          atp_dead[atp_n_dead++] = s->r_trace[j];
        }
#endif
        if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[j]);
        if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[j]);
        atp_wmo_defer_or_remove(s, &wd, s->r_uid[j]);
        atp_rj_death(s, j);
        if (!s->r_orient[j]) s->n_unorient--;
        for (u32 k = j + 1; k < s->n_rules; k++) {
          s->lhs[k - 1]              = s->lhs[k];
          s->rhs[k - 1]              = s->rhs[k];
          s->r_trace[k - 1]          = s->r_trace[k];
          s->r_uid[k - 1]            = s->r_uid[k];
          atp_uid_idx_set(s, s->r_uid[k], k - 1);
          s->r_orient[k - 1]         = s->r_orient[k];
          s->r_gd[k - 1]             = s->r_gd[k];
          s->r_depth[k - 1]          = s->r_depth[k];   /* Twee getDepth rides the slot */
          s->r_gj_status[k - 1]      = s->r_gj_status[k];   /* GJ status rides the slot */
          s->r_dead[k - 1]           = s->r_dead[k];
          s->r_dead_lhs_save[k - 1]  = s->r_dead_lhs_save[k];
          s->r_dead_rhs_save[k - 1]  = s->r_dead_rhs_save[k];
          // Remove-and-re-derive subsumer shapes + flag ride the slot
          // (see the LHS-drop loop above for why all must shift).
          s->r_dead_subsumer_lhs[k - 1] = s->r_dead_subsumer_lhs[k];
          s->r_dead_subsumer_rhs[k - 1] = s->r_dead_subsumer_rhs[k];
          s->r_rederive_cut[k - 1]      = s->r_rederive_cut[k];
          s->r_overlap_done[k - 1] = s->r_overlap_done[k];
#ifdef THVM_ATPFT_RULES
          // Stage 4: shift the parallel AtpFt slot pointers.
          s->lhs_ft[k - 1]              = s->lhs_ft[k];
          s->rhs_ft[k - 1]              = s->rhs_ft[k];
          s->r_dead_lhs_save_ft[k - 1]  = s->r_dead_lhs_save_ft[k];
          s->r_dead_rhs_save_ft[k - 1]  = s->r_dead_rhs_save_ft[k];
          s->r_dead_subsumer_lhs_ft[k - 1] = s->r_dead_subsumer_lhs_ft[k];
          s->r_dead_subsumer_rhs_ft[k - 1] = s->r_dead_subsumer_rhs_ft[k];
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
              ? atp_wmo_victim_drain_key(s, s->r_uid[j], 1u) : 0u;
          atp_irv_stage_ft(s, j, 5u);
          atp_irv_push(s, old_lhs, old_rhs, simplify_parent, wmo_key);
        } else {
          g_enq_depth_pending = (s->r_depth != NULL) ? s->r_depth[j] : 0u;
          atp_add_equation_simplified(s, old_lhs, reduced, simplify_parent);
        }
        if (atp_rule_trace_on()) {
          char la[8192], ra[8192];
          atp_pretty_term(old_lhs, la, sizeof la);
          atp_pretty_term(old_rhs, ra, sizeof ra);
          fprintf(stderr, "  RETIRE eq-rhs (slot %u, trace %u): %s = %s\n",
                  j, s->r_trace[j], la, ra);
        }
#ifdef ATP_ORPHAN_KILL
        if (atp_dead != NULL && s->r_trace[j] != ATP_TRACE_NONE) {
          atp_dead[atp_n_dead++] = s->r_trace[j];
        }
#endif
        if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[j]);
        // Unconditional kill: a one-side interreduction pull in WM is
        // followed by the REPuffer pass removing the Gegenseite too
        // ("Gegenseite ggf. aus E entfernen", Interreduktion.c:308),
        // so the DISTINGUISHED direction always leaves E during the
        // event and KPV_KillParent fires exactly once regardless of
        // which thvm face was reduced (verified on OrAssociativity:
        // eq -1 kill at cp=209784 needs dist_rhs=0 killed, eq -3 kill
        // at cp=769962 needs dist_rhs=1 killed -- no single polarity
        // covers both, WM kills both).
        if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[j]);
        atp_wmo_defer_or_remove(s, &wd, s->r_uid[j]);
        atp_rj_death(s, j);
        if (!s->r_orient[j]) s->n_unorient--;
        for (u32 k = j + 1; k < s->n_rules; k++) {
          s->lhs[k - 1]              = s->lhs[k];
          s->rhs[k - 1]              = s->rhs[k];
          s->r_trace[k - 1]          = s->r_trace[k];
          s->r_uid[k - 1]            = s->r_uid[k];
          atp_uid_idx_set(s, s->r_uid[k], k - 1);
          s->r_orient[k - 1]         = s->r_orient[k];
          s->r_gd[k - 1]             = s->r_gd[k];
          s->r_depth[k - 1]          = s->r_depth[k];   /* Twee getDepth rides the slot */
          s->r_gj_status[k - 1]      = s->r_gj_status[k];   /* GJ status rides the slot */
          s->r_dead[k - 1]           = s->r_dead[k];
          s->r_dead_lhs_save[k - 1]  = s->r_dead_lhs_save[k];
          s->r_dead_rhs_save[k - 1]  = s->r_dead_rhs_save[k];
          // Remove-and-re-derive subsumer shapes + flag ride the slot
          // (see the LHS-drop loop above for why all must shift).
          s->r_dead_subsumer_lhs[k - 1] = s->r_dead_subsumer_lhs[k];
          s->r_dead_subsumer_rhs[k - 1] = s->r_dead_subsumer_rhs[k];
          s->r_rederive_cut[k - 1]      = s->r_rederive_cut[k];
          s->r_overlap_done[k - 1] = s->r_overlap_done[k];
#ifdef THVM_ATPFT_RULES
          // Stage 4: shift the parallel AtpFt slot pointers.
          s->lhs_ft[k - 1]              = s->lhs_ft[k];
          s->rhs_ft[k - 1]              = s->rhs_ft[k];
          s->r_dead_lhs_save_ft[k - 1]  = s->r_dead_lhs_save_ft[k];
          s->r_dead_rhs_save_ft[k - 1]  = s->r_dead_rhs_save_ft[k];
          s->r_dead_subsumer_lhs_ft[k - 1] = s->r_dead_subsumer_lhs_ft[k];
          s->r_dead_subsumer_rhs_ft[k - 1] = s->r_dead_subsumer_rhs_ft[k];
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

  // Snapshot flush (see AtpWmoDefer): evict the buffered victims' wmo
  // faces now that every drain key was ranked against the pass-start
  // tree.  Runs before CP generation (the caller's next step).
  atp_wmo_defer_flush(s, &wd);

#ifdef ATP_ORPHAN_KILL
  g_thvm_phase = "ir_orphan";
  if (atp_dead != NULL) {
    if (atp_n_dead > 0) atp_cp_kill_orphans(s, atp_dead, atp_n_dead);
    free(atp_dead);
  }
#endif
  atp_rj_slice(s, 0u, 0u);
  return dropped;
}

// === Waldmeister IR-victim drain (use_wm_demote) ====================

// WM doR normalize: bring `t` to normal form using ORIENTED rules only
// (the Regelbaum) over the full current R.  Unorientable equations (the
// Gleichungsbaum) do not rewrite -- the `-kg r` strength WM applies at
// generation-time / requeue treatment (RUN/Parameter.c:397).
static Term atp_rules_only_normalize(AtpState *s, Term t, u32 cap) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // WM normalizes with MIXMOST redex order at EVERY site (Parameter.c:419
  // default `-nf mixmost`), including CP normalization.  thvm's Term-side
  // indexed scan is outermost-leftmost -> a different (smaller) NF on the
  // still-non-confluent rule set, so an equal overlap gets an over-reduced
  // lighter CP than WM keeps (the rule-52/56 selection divergence).  Route
  // through the FT rules-only normalizer, which honors use_mixmost_nf, so CP
  // normalization matches WM.  Off unless the WM preset set use_mixmost_nf.
  if (s->use_mixmost_nf) {
    atp_ft_mirror_ensure(s);
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fc = atp_rules_only_normalize_ft(s, ft_from_term(a, t, 0), cap);
    return ft_to_term(fc);
  }
#endif
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
    u32  d = s->irv_depth[a];
#ifdef THVM_ATPFT_RULES
    AtpFtCell *fl = s->irv_lhs_ft[a], *fr = s->irv_rhs_ft[a];
#endif
    u32  b = a;
    while (b > 0u && s->irv_wmo_key[b - 1u] > k) {
      s->irv_lhs[b]     = s->irv_lhs[b - 1u];
      s->irv_rhs[b]     = s->irv_rhs[b - 1u];
      s->irv_parent[b]  = s->irv_parent[b - 1u];
      s->irv_wmo_key[b] = s->irv_wmo_key[b - 1u];
      s->irv_depth[b]   = s->irv_depth[b - 1u];
#ifdef THVM_ATPFT_RULES
      s->irv_lhs_ft[b]  = s->irv_lhs_ft[b - 1u];
      s->irv_rhs_ft[b]  = s->irv_rhs_ft[b - 1u];
#endif
      b--;
    }
    s->irv_lhs[b] = l; s->irv_rhs[b] = r;
    s->irv_parent[b] = p; s->irv_wmo_key[b] = k;
    s->irv_depth[b] = d;
#ifdef THVM_ATPFT_RULES
    s->irv_lhs_ft[b] = fl; s->irv_rhs_ft[b] = fr;
#endif
  }
}

static void atp_wm_demote_drain(AtpState *s) {
  const u32 WM_BEHANDELN_GATE = 50u;   // KPVerwaltung.c:437
  const u32 NORM_CAP = 64u;            // matches the pop-normalize cap
  // WM drains the IR buffer in discrimination-tree leaf-list order
  // (equation victims before rule victims), not thvm's slot-scan order;
  // reorder to match before stamping fresh FIFO ages.
  if (s->use_emission_order) atp_irv_sort_wm_order(s);
  // Forensic drain dump: THVM_ATP_IRV_DUMP=<seqlo>:<seqhi> prints every
  // buffered victim (post-sort key + original sides) for drains whose
  // next FIFO age falls in the window.  Diagnostic only; zero cost unset.
  {
    static long irv_lo = -2, irv_hi = -2;
    if (irv_lo == -2) {
      const char *e = getenv("THVM_ATP_IRV_DUMP");
      if (e != NULL && e[0] != '\0') {
        irv_lo = strtol(e, NULL, 10);
        const char *c = strchr(e, ':');
        irv_hi = (c != NULL) ? strtol(c + 1, NULL, 10) : irv_lo + 64;
      } else {
        irv_lo = -1;
        irv_hi = -1;
      }
    }
    if (irv_lo >= 0 && (long)s->cp_seq_next >= irv_lo &&
        (long)s->cp_seq_next <= irv_hi) {
      for (u32 v = 0; v < s->n_irv; v++) {
        char la[2048], ra[2048];
        atp_pretty_term(s->irv_lhs[v], la, sizeof la);
        atp_pretty_term(s->irv_rhs[v], ra, sizeof ra);
        fprintf(stderr,
                "IRVDUMP seq_next=%u v=%u key=%08x (tree=%u leafrank=%u "
                "chain=%u) parent=%u %s = %s\n",
                s->cp_seq_next, v, s->irv_wmo_key[v],
                (u32)(s->irv_wmo_key[v] >> 31),
                (s->irv_wmo_key[v] >> 8) & 0x7fffffu,
                s->irv_wmo_key[v] & 0xffu, s->irv_parent[v], la, ra);
      }
    }
  }
  for (u32 v = 0; v < s->n_irv; v++) {
    Term l      = s->irv_lhs[v];
    Term r      = s->irv_rhs[v];
    u32  parent = s->irv_parent[v];
    u32  trace_mark = s->n_trace;
    u8   treated = 0u;
    u32 vsz;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if ((term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) &&
        s->irv_lhs_ft[v] != NULL && s->irv_rhs_ft[v] != NULL) {
      // Sentinel victim: the KPBehandeln size gate reads the staged
      // clones (a sentinel would count as one node and misroute the
      // raw class into treatment).
      vsz = ft_symbol_count_span(s->irv_lhs_ft[v]) +
            ft_symbol_count_span(s->irv_rhs_ft[v]);
    } else
#endif
    vsz = atp_symbol_count(l) + atp_symbol_count(r);
    if (vsz < WM_BEHANDELN_GATE) {
      treated = 1u;
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        AtpFtCell *vfl = s->irv_lhs_ft[v];
        AtpFtCell *vfr = s->irv_rhs_ft[v];
        // FT doR treatment on the victim's captured spans -- exactness
        // is proven per victim (ft_eq_term against the buffered Terms
        // guards any capture-site staleness); a proven pair normalizes
        // FT-native, joins via ft_eq (no decode), and reuses the
        // buffered Terms when the NF is the identity.
        {
          static int ddbg = -1;
          if (ddbg < 0) ddbg = atp_env_on("THVM_ATP_DRAIN_DBG");
          if (ddbg) {
            extern u8 *g_irv_site_dbg; extern u32 g_irv_site_dbg_cap;
            fprintf(stderr,
                    "DRAINDBG v=%u site=%u mix=%u vfl=%d vfr=%d eq_l=%d eq_r=%d\n",
                    v,
                    (g_irv_site_dbg != NULL && v < g_irv_site_dbg_cap)
                        ? g_irv_site_dbg[v] : 0u,
                    s->use_mixmost_nf, vfl != NULL, vfr != NULL,
                    vfl != NULL ? ft_eq_term(vfl, l) : -1,
                    vfr != NULL ? ft_eq_term(vfr, r) : -1);
          }
        }
        u8 v_lazy = (u8)(term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM);
        if (s->use_mixmost_nf && vfl != NULL && vfr != NULL &&
            (v_lazy || (ft_eq_term(vfl, l) && ft_eq_term(vfr, r)))) {
          // A SENTINEL victim's staged clones are authoritative by the
          // lockstep-at-push invariant (the ft_eq_term guard protects
          // only real-Term captures).  Normalize WORKING COPIES so the
          // pristine clones survive for the requeue decode; a joined
          // victim decodes nothing.
          atp_ft_mirror_ensure(s);
          AtpFt *va_ = (AtpFt *)s->ft_arena_ptr;
          AtpFtCell *lf = atp_rules_only_normalize_ft(
              s, ft_clone_span(va_, vfl, 0), NORM_CAP);
          AtpFtCell *rf = atp_rules_only_normalize_ft(
              s, ft_clone_span(va_, vfr, 0), NORM_CAP);
          if (ft_eq(lf, rf)) {
            ft_free_span(va_, vfl, vfl->end);
            ft_free_span(va_, vfr, vfr->end);
            s->irv_lhs_ft[v] = NULL;
            s->irv_rhs_ft[v] = NULL;
            s->n_trace = trace_mark;
            s->n_wm_demote_joined++;
            continue;
          }
          // Requeue: park the NF spans for the span-native enqueue --
          // no decode here; the enqueue's fallback decodes from the
          // parked spans if its fused arm declines.  The buffered
          // Terms ride as-is (possibly sentinels).
          g_enq_ft_l = lf;
          g_enq_ft_r = rf;
          ft_free_span(va_, vfl, vfl->end);
          ft_free_span(va_, vfr, vfr->end);
          s->irv_lhs_ft[v] = NULL;
          s->irv_rhs_ft[v] = NULL;
        } else {
          if (vfl != NULL) {
            ft_free_span((AtpFt *)s->ft_arena_ptr, vfl, vfl->end);
            s->irv_lhs_ft[v] = NULL;
          }
          if (vfr != NULL) {
            ft_free_span((AtpFt *)s->ft_arena_ptr, vfr, vfr->end);
            s->irv_rhs_ft[v] = NULL;
          }
          l = atp_rules_only_normalize(s, l, NORM_CAP);
          r = atp_rules_only_normalize(s, r, NORM_CAP);
        }
#else
        l = atp_rules_only_normalize(s, l, NORM_CAP);
        r = atp_rules_only_normalize(s, r, NORM_CAP);
#endif
      }
      if (kbo_eq(l, r)) {
        // Joined: the victim is redundant.  Rewind any NORM_STEPs just
        // recorded for it (nothing else references them yet).
        s->n_trace = trace_mark;
        s->n_wm_demote_joined++;
        continue;
      }
    }
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Raw-class / record-path victims: park the pristine clones for
    // the span-native enqueue (ownership transfers; the enqueue's
    // fallback decodes from them when its fused arm declines).
    if (s->irv_lhs_ft[v] != NULL && s->irv_rhs_ft[v] != NULL) {
      g_enq_ft_l = s->irv_lhs_ft[v];
      g_enq_ft_r = s->irv_rhs_ft[v];
      s->irv_lhs_ft[v] = NULL;
      s->irv_rhs_ft[v] = NULL;
    }
#endif
    // A victim too big for the KPBehandeln gate is WM's RAW (untreated)
    // class: KPV_IROpferBehandeln keeps it at its raw sides and weight in
    // the SUE heap -- WM never defers a KilledRE victim, so mark the push
    // raw so the auto-MaxWeight stash cannot swallow it (the OA -auto
    // fact-1378 divergence: thvm stashed the heavy w1=2141/1937/2437
    // victims that WM's FIFO lane later popped).
    if (!treated) g_enq_raw_untreated = 1u;
    // Twee depth semantics: the requeued victim keeps its rule's
    // formation depth (captured at drop time in irv_depth).
    g_enq_depth_pending = s->irv_depth[v];
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

