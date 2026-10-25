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
    // Snapshot in heap-POSITION order (the historical slot order).
    u32 vv = s->cp_heap[i].row;
    Term l = 0, r = 0;
    atp_cp_slot_read(s, vv, &l, &r);
    if (lhs_out != NULL) lhs_out[i] = l;
    if (rhs_out != NULL) rhs_out[i] = r;
    if (seq_out != NULL) seq_out[i] = s->cp_seq[vv];
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
        u32 vv = s->cp_heap[i].row;
        if (s->cp_seq[vv] == seq[j]) {
          s->cp_pri[vv] = pri[j];
          // Queued-row cp_pri write: refresh the packed node caches
          // before the trailing Floyd compares against them.
          atp_cp_heap_set(s, i, vv);
          break;
        }
      }
    }
    for (u32 i = s->n_cps / 2u; i > 0; ) { i--; atp_cp_sift_down(s, i); }
    return;
  }
  // Key the map on the stationary payload ROW (cp_seq is per-CP; the
  // write below targets the same CP wherever its heap position is).
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 vv = s->cp_heap[i].row;
    u32 h = (s->cp_seq[vv] * 2654435761u) & mask;
    while (map_seq[h] != 0u) h = (h + 1u) & mask;
    map_seq[h]  = s->cp_seq[vv] + 1u;
    map_slot[h] = vv;
  }
  for (u32 j = 0; j < n; j++) {
    u32 h = (seq[j] * 2654435761u) & mask;
    while (map_seq[h] != 0u) {
      if (map_seq[h] == seq[j] + 1u) {
        u32 row = map_slot[h];
        s->cp_pri[row] = pri[j];
        // Queued-row cp_pri write: refresh the packed node caches at
        // the row's position before the trailing Floyd.
        atp_cp_heap_set(s, s->cp_pos[row], row);
        break;
      }
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
        u32 vv = s->cp_heap[i].row;
        // cp_pri2 is NOT cached in the packed node -- no refresh owed.
        if (s->cp_seq[vv] == seq[j]) { s->cp_pri2[vv] = pri[j]; break; }
      }
    }
    return;
  }
  // Row-keyed like thvm_atp_set_cp_pri_by_seq (see there).  cp_pri2 is
  // NOT cached in the packed node -- no refresh owed.
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 vv = s->cp_heap[i].row;
    u32 h = (s->cp_seq[vv] * 2654435761u) & mask;
    while (map_seq[h] != 0u) h = (h + 1u) & mask;
    map_seq[h]  = s->cp_seq[vv] + 1u;
    map_slot[h] = vv;
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
    // Transient unpack: a post-saturation queue holds millions of CPs
    // (OrAssociativity PROVED with 2.64M queued), so reclaim the
    // just-measured terms between iterations or this diagnostic walk
    // exhausts the from-space AFTER the proof already succeeded.
    // Two triggers: the cheap threshold one under trace_pack (small
    // live set), and the half-full pressure mark for the ON-heap-trace
    // config -- whose passes are expensive (each copies the whole
    // proof trace) but whose alternative was heap_alloc's hard fatal:
    // the walk's ~50 transient cells per queued CP times a multi-
    // million post-saturation queue overran the remaining from-space
    // right after the summary prints (exit 1 on a PROVED run).  The
    // pressure mark is throttled (atp_pressure_collect): a live set above
    // half the space -- a full on-heap trace -- collected on EVERY queued
    // CP here, a post-run stall of thousands of whole-heap copies.
    if (atp_transient_gc_due(s)) {
      thvm_atp_gc_collect((AtpState *)s);
    } else {
      atp_pressure_collect((AtpState *)s);
    }
    atp_cp_slot_read(s, s->cp_heap[i].row, &l, &r);
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Lazy commit: canonicalize + dup-scan on span clones; the sentinel
  // handles pass through untouched and become the stored Term slots.
  AtpFtCell *lazy_canon_l = NULL;
  AtpFtCell *lazy_canon_r = NULL;
  {
    const AtpFtCell *bl = atp_lazy_span_of(lhs);
    const AtpFtCell *br = atp_lazy_span_of(rhs);
    if (bl != NULL && br != NULL) {
      AtpFt *ra = (AtpFt *)s->ft_arena_ptr;
      lazy_canon_l = ft_clone_span(ra, bl, 0);
      lazy_canon_r = ft_clone_span(ra, br, 0);
      ft_normalize_vars_pair(lazy_canon_l, lazy_canon_r);
      for (u32 i = 0; i < s->n_rules; i++) {
        if (s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL &&
            ft_eq(s->lhs_ft[i], lazy_canon_l) &&
            ft_eq(s->rhs_ft[i], lazy_canon_r)) {
          ft_free_span(ra, lazy_canon_l, lazy_canon_l->end);
          ft_free_span(ra, lazy_canon_r, lazy_canon_r->end);
          return 0;  // duplicate rule -- already in R
        }
      }
      goto lazy_skip_varnorm;
    }
  }
#endif
#ifdef ATP_VAR_NORM
  thvm_normalize_vars(&lhs, &rhs);
  for (u32 i = 0; i < s->n_rules; i++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // A lazy row's slot is a sentinel: compare through its mirror
    // (exact FT-vs-Term) so duplicates are never missed behind it.
    if (term_tag(s->lhs[i]) == TAG_NUM || term_tag(s->rhs[i]) == TAG_NUM) {
      if (s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL &&
          ft_eq_term(s->lhs_ft[i], lhs) && ft_eq_term(s->rhs_ft[i], rhs)) {
        return 0;  // duplicate rule -- already in R
      }
      continue;
    }
#endif
    if (kbo_eq(s->lhs[i], lhs) && kbo_eq(s->rhs[i], rhs)) {
      return 0;  // duplicate rule -- already in R
    }
  }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
lazy_skip_varnorm: ;
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
  // A compaction may have vacated this slot with a stale GJ status;
  // every fresh fact starts untested (WM: a fresh termpair object).
  s->r_gj_status[s->n_rules] = ATP_GJ_ST_UNKNOWN;
  // WM Waisenmord identity: every fact entering R/E gets a FRESH uid --
  // a simplified+requeued victim re-enters as a NEW WM rule number, so
  // CPs of the old identity die with it (see AtpState.r_uid).
  s->r_uid[s->n_rules] = ++s->next_rule_uid;
  atp_uid_idx_set(s, s->r_uid[s->n_rules], s->n_rules);
  // Twee getDepth: a rule oriented from the selected CP inherits that CP's
  // formation depth (captured at pop as last_popped_depth; 0 for the
  // initial axioms, which are added before any pop).
  s->r_depth[s->n_rules] = s->last_popped_depth;
  // spb=goal: the rule inherits the popped CP's goal-derived bit.
  s->r_gd[s->n_rules] = s->last_popped_gd;
  s->r_overlap_done[s->n_rules] = 0u;
#ifdef THVM_ATPFT_RULES
  // Stage 4: mirror the Term pair into the parallel AtpFt slots
  // immediately so every later observer sees the two paths in lockstep
  // (slot[i] occupancy on the ft side is exactly slot[i] occupancy on
  // the Term side).  Persistent allocation (scratch=0) so the cells
  // live as long as the rule slot does -- ft_destroy at thvm_atp_free
  // releases them all.  thvm_normalize_vars already settled the
  // variable canonicalization above, so the ft image is built from the
  // canonical form.  When the step function bracketed this add with the
  // popped CP's NF spans (g_rule_ft_l/r) and one of them IS this side's
  // exact image (ft_eq_term proof -- covers both the straight and the
  // orient-swapped pairing), clone it instead of re-encoding the Term;
  // the clone is bit-identical to the conversion by construction.  A
  // var-renumbered or reduced side fails the proof and falls back.
  // VERIFY probe runs after both writes are committed.
  {
    AtpFt *ra = (AtpFt *)s->ft_arena_ptr;
    u8 adopted = 0u;
    if (lazy_canon_l != NULL && lazy_canon_r != NULL) {
      // Lazy commit: the canonical clones ARE the mirror (ownership
      // moves to the rule slot); the Term slots keep the sentinels.
      s->lhs_ft[s->n_rules] = lazy_canon_l;
      s->rhs_ft[s->n_rules] = lazy_canon_r;
      adopted = 1u;
    } else if (g_rule_ft_l != NULL && g_rule_ft_r != NULL) {
      // Try both pairings; each candidate clone is renumbered to the
      // canonical dense var set (the FT sibling of the
      // thvm_normalize_vars run above) and PROVEN against the stored
      // Terms before adoption -- a failed proof frees the clones and
      // falls through (the fvi grounded variants land here).
      const AtpFtCell *cand[2][2] = {
        { g_rule_ft_l, g_rule_ft_r },
        { g_rule_ft_r, g_rule_ft_l },
      };
      for (u32 o = 0; o < 2u && !adopted; o++) {
        AtpFtCell *cl = ft_clone_span(ra, cand[o][0], 0);
        AtpFtCell *cr = ft_clone_span(ra, cand[o][1], 0);
        ft_normalize_vars_pair(cl, cr);
        if (ft_eq_term(cl, lhs) && ft_eq_term(cr, rhs)) {
          s->lhs_ft[s->n_rules] = cl;
          s->rhs_ft[s->n_rules] = cr;
          adopted = 1u;
        } else {
          ft_free_span(ra, cl, cl->end);
          ft_free_span(ra, cr, cr->end);
        }
      }
    }
    if (!adopted) {
      s->lhs_ft[s->n_rules] = ft_from_term(ra, lhs, 0);
      s->rhs_ft[s->n_rules] = ft_from_term(ra, rhs, 0);
    }
  }
  atp_ft_rules_verify_push(s, s->n_rules);
  {
    // Content-true rule dump (THVM_ATP_RULE_TRACE2): decodes the just-
    // stored MIRRORS, so lazy sentinels / packed-trace placeholders in
    // the Term slots never blind the print.
    static int rt2 = -1;
    if (rt2 < 0) rt2 = atp_env_on("THVM_ATP_RULE_TRACE2");
    if (rt2 && s->lhs_ft[s->n_rules] != NULL) {
      char lb[512], rb[512];
      atp_pretty_term(ft_to_term(s->lhs_ft[s->n_rules]), lb, sizeof lb);
      atp_pretty_term(ft_to_term(s->rhs_ft[s->n_rules]), rb, sizeof rb);
      fprintf(stderr, "RULEADD2 slot=%u %s -> %s\n", s->n_rules, lb, rb);
    }
  }
  // Materialize-at-birth: decode the just-adopted mirrors into the Term
  // slots so NO consumer outside the birth bracket ever sees a NUM
  // sentinel.  The 2026-07-11 sentinel-consumer audit confirmed 15 raw
  // slot-read sites (batch former false SATURATION under
  // --preset=none --mixmost-nf, ground-join drops 22014 -> 0, blind
  // interreduce victims, MNF scan, WM -gj fact test, TFindProof rule
  // exports, ...) -- a whack-a-mole surface.  The span-native fast
  // paths all read the MIRRORS and keep their wins; the slots' laziness
  // only ever saved these two decodes per rule birth (~microseconds),
  // which is noise against the correctness surface.  The sentinel
  // machinery INSIDE the bracket (orient verdicts off spans) is
  // untouched, and lazy == eager everywhere else by construction --
  // exactly the kill-switch representation the suites certify.
  if (term_tag(s->lhs[s->n_rules]) == TAG_NUM && s->lhs_ft[s->n_rules] != NULL) {
    s->lhs[s->n_rules] = ft_to_term(s->lhs_ft[s->n_rules]);
  }
  if (term_tag(s->rhs[s->n_rules]) == TAG_NUM && s->rhs_ft[s->n_rules] != NULL) {
    s->rhs[s->n_rules] = ft_to_term(s->rhs_ft[s->n_rules]);
  }
#endif
  // Gated rule-add trace (THVM_ATP_RULE_TRACE): slot + S-expr LHS/RHS, so a
  // rule can be matched by term to Waldmeister's rule set (sidesteps WM's
  // ElternNr/w2 counter confusion -- correlate by term, not number).
  // Emitted AFTER materialize-at-birth so the slots hold real Terms on
  // the lazy path -- the pre-materialize args are NUM sentinels there,
  // and printing them blinded the recorded WM-parity comparison method
  // (every RULEADD read "lhs=#0 rhs=#0").
  {
    static int rt = -1;
    if (rt < 0) rt = (getenv("THVM_ATP_RULE_TRACE") != NULL) ? 1 : 0;
    if (rt) {
      fprintf(stderr, "RULEADD slot=%u uid=%u lhs=", s->n_rules,
              s->r_uid[s->n_rules]);
      atp_dbg_print_term(stderr, s->lhs[s->n_rules]);
      fprintf(stderr, " rhs=");
      atp_dbg_print_term(stderr, s->rhs[s->n_rules]);
      fputc('\n', stderr);
    }
  }
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      // Lazy commit: the candidate is the canonical span pair; the
      // saves are real Terms (persist sites materialize sentinels),
      // so the exact FT-vs-Term compare decides.  Live-rule faces in
      // the resolution loop below go through the mirrors the same way.
      if (lazy_canon_l != NULL) {
        if (s->r_dead_lhs_save_ft[k] != NULL &&
            s->r_dead_rhs_save_ft[k] != NULL) {
          if (!ft_eq(s->r_dead_lhs_save_ft[k], lazy_canon_l) ||
              !ft_eq(s->r_dead_rhs_save_ft[k], lazy_canon_r))
            continue;
        } else if (term_tag(s->r_dead_lhs_save[k]) == TAG_NUM ||
                   term_tag(s->r_dead_rhs_save[k]) == TAG_NUM) {
          continue;   // sentinel save without a span: cannot match
        } else if (!ft_eq_term(lazy_canon_l, s->r_dead_lhs_save[k]) ||
                   !ft_eq_term(lazy_canon_r, s->r_dead_rhs_save[k])) {
          continue;
        }
      } else
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      // A lazily-dropped row's save may be a sentinel: compare through
      // its span (exact FT-vs-Term).
      if (term_tag(s->r_dead_lhs_save[k]) == TAG_NUM ||
          term_tag(s->r_dead_rhs_save[k]) == TAG_NUM) {
        if (s->r_dead_lhs_save_ft[k] == NULL ||
            s->r_dead_rhs_save_ft[k] == NULL)
          continue;
        if (!ft_eq_term(s->r_dead_lhs_save_ft[k], lhs) ||
            !ft_eq_term(s->r_dead_rhs_save_ft[k], rhs))
          continue;
      } else
#endif
      if (!kbo_eq(s->r_dead_lhs_save[k], lhs) ||
          !kbo_eq(s->r_dead_rhs_save[k], rhs))
        continue;
      // Resolve the cutoff: the live rule whose shape equals the saved
      // subsumer, read its now-final r_trace.  Match either orientation
      // (the subsumer is unorientable, stored in one face).
      Term sub_l = s->r_dead_subsumer_lhs[k];
      Term sub_r = s->r_dead_subsumer_rhs[k];
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      const AtpFtCell *sub_lf = s->r_dead_subsumer_lhs_ft[k];
      const AtpFtCell *sub_rf = s->r_dead_subsumer_rhs_ft[k];
#endif
      u32 cutoff = ATP_TRACE_NONE;
      for (u32 m = 0; m < s->n_rules; m++) {
        if (s->r_dead[m]) continue;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        if (sub_lf != NULL && sub_rf != NULL) {
          // Span-vs-span shape compare (both canonical); the only
          // valid read when the subsumer was a lazy commit.
          if (s->lhs_ft[m] != NULL && s->rhs_ft[m] != NULL &&
              ((ft_eq(s->lhs_ft[m], sub_lf) &&
                ft_eq(s->rhs_ft[m], sub_rf)) ||
               (ft_eq(s->lhs_ft[m], sub_rf) &&
                ft_eq(s->rhs_ft[m], sub_lf)))) {
            cutoff = s->r_trace[m];
            break;
          }
          continue;
        }
        if (s->lhs_ft[m] != NULL && s->rhs_ft[m] != NULL) {
          // Exact FT-vs-Term compare -- immune to lazy sentinels in
          // the live slots (subsumer shapes are real Terms).
          if ((ft_eq_term(s->lhs_ft[m], sub_l) &&
               ft_eq_term(s->rhs_ft[m], sub_r)) ||
              (ft_eq_term(s->lhs_ft[m], sub_r) &&
               ft_eq_term(s->rhs_ft[m], sub_l))) {
            cutoff = s->r_trace[m];
            break;
          }
          continue;
        }
#endif
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (lazy_canon_l != NULL) {
    // Lazy commit: the verdict off the just-stored mirrors (the pop
    // pre-checked GT on the pre-canonical spans; renumbering is
    // KBO-invariant, so this is the same verdict the Term compare
    // would cache).
    s->r_orient[s->n_rules] =
        (u8)(thvm_kbo_ft(s->lhs_ft[s->n_rules], s->rhs_ft[s->n_rules],
                         s->kbo) == KBO_GT);
  } else
#endif
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
#ifdef ATP_MNF
  // WM ArbeitsAufnahme notifies open goals with THE KEPT FACT
  // (Z_ZieleErledigtMitNeuemFaktum); queue it for the next mnf_step --
  // a slot-range diff misses facts re-slotted by interreduction churn
  // between the keep and the goal check.
  if (s->use_mnf && s->mnf != NULL) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // The front search consumes the pend pair through its SPANS now
    // (match + instantiate off the mirrors); the Term slots ride along
    // for the mirror-less fallback and may be sentinels (never walked
    // when the spans are staged).
    g_mnf_pend_ft_l = s->lhs_ft[s->n_rules];
    g_mnf_pend_ft_r = s->rhs_ft[s->n_rules];
    if ((g_mnf_pend_ft_l == NULL || g_mnf_pend_ft_r == NULL) &&
        lazy_canon_l != NULL) {
      lhs = atp_rule_term(s, s->n_rules, 0u);   // defensive: no mirror
      rhs = atp_rule_term(s, s->n_rules, 1u);
    }
#endif
    mnf_notify_fact(s->mnf, lhs, rhs, s->r_orient[s->n_rules]);
  }
#endif
  // ifeq guard-DECIDING registry (see atp_ifeq_guard_register): the
  // slot's Term mirrors are guaranteed real (materialize-at-birth
  // above converted any TAG_NUM sentinel) regardless of build config,
  // so this always walks real content.  No-op unless use_ifeq_decide
  // is armed.
  atp_ifeq_guard_register(s, s->lhs[s->n_rules], s->rhs[s->n_rules]);
  atp_rj_birth(s, s->n_rules);
  s->n_rules++;
  // Env-gated live-rule-set dump (THVM_ATP_DUMP_RULES_AT=N): when the rule
  // count first reaches N, print every live rule so the live set can be diffed
  // against -auto's first N rules (finds a MISSING rule, the firstdiv-11 cause).
  {
    static int dump_at = -2;
    if (dump_at == -2) { const char *e = getenv("THVM_ATP_DUMP_RULES_AT"); dump_at = e ? atoi(e) : -1; }
    if (dump_at > 0 && (int)s->n_rules == dump_at) {
      for (u32 k = 0; k < s->n_rules; k++) {
        fprintf(stderr, "LIVERULE[%u] lhs=", k);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        atp_dbg_print_term(stderr, atp_rule_term(s, k, 0u));
        fprintf(stderr, " rhs=");
        atp_dbg_print_term(stderr, atp_rule_term(s, k, 1u));
#else
        atp_dbg_print_term(stderr, s->lhs[k]);
        fprintf(stderr, " rhs="); atp_dbg_print_term(stderr, s->rhs[k]);
#endif
        fputc('\n', stderr);
      }
    }
  }
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
        atp_rj_death(s, i);
        s->r_dead_lhs_save[i] = s->lhs[i];   // sentinel rides; save_ft is authoritative
        s->r_dead_rhs_save[i] = s->rhs[i];
        s->lhs[i] = dead_sentinel;
        s->rhs[i] = dead_sentinel;
        s->r_dead[i] = 1;
        // Soft-deleted rule -- the active R changed, so the IR-normalize
        // cookie keyed on r_revision must invalidate.
        s->r_revision++;
        s->n_rules_bwd_subsumed++;
        // WM order mirror: the subsumed fact leaves the tree.
        if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_uid[i]);
#ifdef THVM_ATPFT_RULES
        // Stage 4: mirror the slot-save on the AtpFt side.  The dead
        // sentinel is a TAG_FVR with var id 255 (out of range); convert
        // it the same way ft_from_term handles any FVR so a later
        // sentinel-check on the ft path matches the Term path.
        s->r_dead_lhs_save_ft[i] = s->lhs_ft[i];
        s->r_dead_rhs_save_ft[i] = s->rhs_ft[i];
        s->lhs_ft[i] = ftnew_var((AtpFt *)s->ft_arena_ptr, 255u, 0);
        s->rhs_ft[i] = ftnew_var((AtpFt *)s->ft_arena_ptr, 255u, 0);
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
  if (on) { s->use_raw_desc = 0u; s->use_raw_desc_cert = 0u; }
}

// Raw-descriptor lane (see AtpState.use_raw_desc): the WM raw >= 50
// FT-direct emit class queues as 20-byte re-derivation descriptors,
// materialized byte-exactly from the parent face spans at pop.
// Mutually exclusive with the trace-backed implicit lane -- the two
// interpret cp_implicit[] differently.
fn void thvm_atp_set_use_raw_desc(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_raw_desc = on ? 1u : 0u;
  if (on) { s->use_implicit_cp = 0u; s->use_raw_desc_cert = 0u; }
}

// Certifier mode (see AtpState.use_raw_desc_cert): store both the
// eager bytes and the descriptor; memcmp at every pop, fatal on any
// divergence.  The slot behaves fully eager, so the trajectory is the
// eager one by construction.
fn void thvm_atp_set_use_raw_desc_cert(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_raw_desc_cert = on ? 1u : 0u;
  if (on) { s->use_implicit_cp = 0u; s->use_raw_desc = 0u; }
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

// DISCOUNT given-clause discipline (Vampire Saturation/Discount.cpp, E's
// DISCOUNT loop).  Passive critical pairs are NOT inter-simplified: each CP
// queues in its formation-time (raw) form and is R+E normalized / joined
// only when it is selected as the given clause (pop-time treatment, already
// the engine default).  This is the completion analog of Vampire's discount
// slice: only the given/active clauses drive simplification; the age-weight
// pick ratio (--awr / selection_ratio) then keeps old shallow CPs surfacing
// by fairness instead of the best-first weight queue burying them.
// Implemented by riding the lazy-push raw class (use_lazy_normalize) with
// the WM KPBehandeln gate forced to 0, so no CP is treated at push.
// Completeness-preserving (no push-time CP drop; the FIFO dimension keeps
// selection fair), so cli_strategy_complete needs no discount marker.
fn void thvm_atp_set_discount(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_discount = on ? 1u : 0u;
  if (on) s->use_lazy_normalize = 1u;
}

fn void thvm_atp_set_use_force_def_orient(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_force_def_orient = on ? 1u : 0u;
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

// Re-point the FVI / unorientable-instance grounding constant
// (`s->min_const`, WM `SO_minimaleKonstante`) at an arbitrary 0-arity
// label.  WM resolves SO_minimaleKonstante as the bottom-most CONSTANT
// of the total precedence chain (`minimaleKonstanteBestimmen`,
// WASIC/SymbolOperationen.c:112-126).  In the INITIAL precedence the
// chain carries `f > const1 > const2` for every signature symbol f
// (`NeueFunktionenEinbauen`, :210-216), so plain wmcli always grounds
// with the extra constant const2 -- thvm's reserved
// ATP_RESERVED_LABEL_MIN_CONST is that mirror and stays the default.
// `wmcli -auto` (= FindEquationalProof), however, installs its chosen
// precedence as a NEW chain (`PraezedenzAnalyse`, :456-517, which per
// the :464-466 comment does NOT re-run NeueFunktionenEinbauen) and
// re-resolves SO_minimaleKonstante over it (:515), so grounding uses
// the problem's own minimal signature constant instead (observed:
// zero const1/const2 tokens in the `wmcli -auto -a 4`
// OrAssociativity reference; its grounded instances carry skC3).
// label 0 restores the reserved default.
fn void thvm_atp_set_min_const_label(AtpState *s, u32 label) {
  if (s == NULL) return;
  if (label == 0u) label = ATP_RESERVED_LABEL_MIN_CONST;
  s->min_const = term_new_ctr(label, NULL, 0);
}

// Vampire-style Limited Resource Strategy (Riazanov & Voronkov, JSC 36,
// 2003).  See AtpState.use_lrs / thvm_atp_select_cp / atp_lrs_recompute_
// horizon for the algorithm.  Resets the per-run horizon state so a
// fresh saturation starts in warmup (no horizon yet); leaves the epoch
// stamps zero so the first selection latches them.
fn void thvm_atp_set_use_lrs(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_lrs = on ? 1u : 0u;
  s->lrs_start_us = 0u;
  s->lrs_start_wall_us = 0u;
  s->lrs_last_recompute_at = 0u;
  s->lrs_horizon = 0u;
}

// Vampire Otter/LRS simplification set (src/atp/otter.c).  Set before
// the first step; the input queue is replayed at the first selection.
fn void thvm_atp_set_use_otter(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_otter = on ? 1u : 0u;
  s->otter_primed = 0u;
}

// Raw-overlap demodulation (src/atp/otter.c).  Set before the first
// step: only CPs pushed while it is on carry a raw stash.
fn void thvm_atp_set_use_otter_raw(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_otter_raw = on ? 1u : 0u;
}

fn void thvm_atp_set_use_ir_run_order(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_ir_run_order = on ? 1u : 0u;
}

fn void thvm_atp_set_use_bd_preordered(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_bd_preordered = on ? 1u : 0u;
}

fn void thvm_atp_set_use_age_depth(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_age_depth = on ? 1u : 0u;
}

fn void thvm_atp_set_use_lrs_age(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_lrs_age = on ? 1u : 0u;
  s->lrs_age_horizon = UINT64_MAX;
}

// Walk a Term collecting its CTR symbol-label bits into mask[].
// goal_sym_mask is a fixed-size bit-set of 8 * 32 = 256 labels (the
// most common range); larger labels mod into the same buckets, so the
// "touches goal" check is a sound over-approximation -- it never rules
// out a CP that genuinely shares a symbol with the goal.
static void atp_sym_mask_collect(Term t, u32 mask[8]) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 lbl = term_ext(t);
      mask[(lbl >> 5) & 7u] |= 1u << (lbl & 31u);
      u32 n = term_ctr_n(t);
      if (n <= REWRITE_MAX_ARITY) {
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) {
          st[nst++] = term_ctr_at(t, i);
        }
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}

static int atp_term_touches_goal(const AtpState *s, Term t) {
  if (s == NULL || !s->use_sos) return 0;
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  int   hit = 0;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 lbl = term_ext(t);
      if (s->goal_sym_mask[(lbl >> 5) & 7u] & (1u << (lbl & 31u))) {
        hit = 1;
        break;
      }
      u32 n = term_ctr_n(t);
      if (n <= REWRITE_MAX_ARITY) {
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) {
          st[nst++] = term_ctr_at(t, i);
        }
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
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

fn void thvm_atp_set_use_goal_subterm(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_goal_subterm = on ? 1u : 0u;
  s->goal_sub_ready = 0u;   // recollected lazily against the current goal
  s->goal_sub_n = 0u;
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

// Waldmeister CP-formation FIFO lineage (see AtpState.use_formation_fifo):
// the faithful-stack master knob.  CP formation itself now emits natively in
// WM's U1 order (thvm_atp_generate_cps_singlewalk), so the historical batch
// re-key corrections this knob used to arm are gone; what remains shared is
// the WM IR-victim drain-order refinements (drain_chainpos / drain_revface)
// and the WM distinguished-direction E-set subsumption (eset_distdir).
// DEFAULT OFF (opt-in via the Waldmeister presets).
fn void thvm_atp_set_use_formation_fifo(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_formation_fifo = on ? 1u : 0u;
  if (on) {
    s->use_drain_chainpos   = 1u;
    s->use_drain_revface    = 1u;
    s->use_eset_distdir     = 1u;
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

// WM-faithful distinguished-direction E-set subsumption (see
// AtpState.use_eset_distdir): test each old equation only in its distinguished
// (stored) orientation, dropping the two subject-swapped match attempts of the
// general 4-way flat subsumer.  DEFAULT OFF; also turned ON under
// use_formation_fifo.
fn void thvm_atp_set_use_eset_distdir(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_eset_distdir = on ? 1u : 0u;
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

// Victim FT spans staged by atp_irv_stage_ft just before atp_irv_push
// (clones of the row's still-live mirrors); consumed into the irv span
// slots by the push.  NULL when the pushing site had no mirror.
#ifdef THVM_ATPFT_RULES
static AtpFtCell *g_irv_ft_l = NULL;
static AtpFtCell *g_irv_ft_r = NULL;
// Diagnostic: which push site staged this victim (DRAINDBG only).
// 1 = WM_IR_DROP_SLOT macro, 2 = WM_IR_DROP_SLOT_RHS macro,
// 3 = plain-IR LHS demote, 4 = ir_pass2 bwd-demod, 5 = ir_pass2 RHS.
static u8 g_irv_site = 0u;
u8  *g_irv_site_dbg = NULL;
u32  g_irv_site_dbg_cap = 0u;
#endif
// Victim's r_depth staged by atp_irv_stage_ft (which every irv push
// site calls with the victim's slot index) and consumed by
// atp_irv_push into s->irv_depth[] -- outside the FT ifdef since the
// depth bookkeeping is span-independent.
static u32 g_irv_depth_stage = 0u;
#ifdef THVM_ATPFT_RULES
// RHS-composition NF span: the FT composition pipeline parks the
// committed NF here so the mirror commit adopts it (ownership moves to
// the rule slot) instead of re-encoding r_reduced via ft_from_term.
// Guarded by an ft_eq_term proof at the adopt; NULL outside the
// composition bracket.
static AtpFtCell *g_ir_rhs_nf_ft = NULL;
#endif

// Both-build helper so the shared WM_IR_DROP_SLOT macro (and the plain
// interreduce site) can stage the victim row's FT mirrors without
// preprocessor forks at the call sites.  No-op off the flag.
static void atp_irv_stage_ft(AtpState *s, u32 idx, u8 site) {
  g_irv_depth_stage = (s->r_depth != NULL && idx < s->n_rules)
                        ? s->r_depth[idx] : 0u;
#ifdef THVM_ATPFT_RULES
  g_irv_site = site;
  // Lazy slots ride as sentinels: the staged clones are authoritative
  // for them (cloned in lockstep at the push; the drain treats working
  // copies and decodes only what a requeue actually needs -- a joined
  // victim costs nothing).
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  g_irv_ft_l = (s->lhs_ft[idx] != NULL)
                 ? ft_clone_span(a, s->lhs_ft[idx], 0) : NULL;
  g_irv_ft_r = (s->rhs_ft[idx] != NULL)
                 ? ft_clone_span(a, s->rhs_ft[idx], 0) : NULL;
#else
  (void)s;
  (void)idx;
  (void)site;
#endif
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
    u32  *nd = (u32  *)realloc(s->irv_depth,  cap * sizeof(u32));
    if (nl == NULL || nr == NULL || np == NULL || nk == NULL || nd == NULL) {
      fprintf(stderr, "atp_irv_push: realloc to %u victims failed\n", cap);
      exit(1);
    }
    s->irv_lhs = nl; s->irv_rhs = nr; s->irv_parent = np;
    s->irv_wmo_key = nk;
    s->irv_depth = nd;
    s->irv_cap = cap;
#ifdef THVM_ATPFT_RULES
    AtpFtCell **nfl = (AtpFtCell **)realloc(s->irv_lhs_ft,
                                            cap * sizeof(AtpFtCell *));
    AtpFtCell **nfr = (AtpFtCell **)realloc(s->irv_rhs_ft,
                                            cap * sizeof(AtpFtCell *));
    extern u8 *g_irv_site_dbg; extern u32 g_irv_site_dbg_cap;
    u8 *nsd = (u8 *)realloc(g_irv_site_dbg, cap * sizeof(u8));
    if (nfl == NULL || nfr == NULL || nsd == NULL) {
      fprintf(stderr, "atp_irv_push: span realloc to %u failed\n", cap);
      exit(1);
    }
    s->irv_lhs_ft = nfl;
    s->irv_rhs_ft = nfr;
    g_irv_site_dbg = nsd;
    g_irv_site_dbg_cap = cap;
#endif
  }
  s->irv_lhs[s->n_irv]     = lhs;
  s->irv_rhs[s->n_irv]     = rhs;
  s->irv_parent[s->n_irv]  = parent;
  s->irv_wmo_key[s->n_irv] = wmo_key;
  s->irv_depth[s->n_irv]   = g_irv_depth_stage;
#ifdef THVM_ATPFT_RULES
  s->irv_lhs[s->n_irv] = lhs;             // may be a lazy sentinel: the
  s->irv_rhs[s->n_irv] = rhs;             // staged clones are authoritative
  s->irv_lhs_ft[s->n_irv] = g_irv_ft_l;   // staged clone (or NULL)
  s->irv_rhs_ft[s->n_irv] = g_irv_ft_r;
  { extern u8 *g_irv_site_dbg; extern u32 g_irv_site_dbg_cap;
    if (g_irv_site_dbg != NULL && s->n_irv < g_irv_site_dbg_cap)
      g_irv_site_dbg[s->n_irv] = g_irv_site; }
  g_irv_ft_l = NULL;
  g_irv_ft_r = NULL;
#endif
  s->n_irv++;
}

// Mark a rule's birthing trace id as dead so descendant CPs are skipped
// at pop time.  Grows the bitset on demand (8 trace ids per byte).
static void atp_trace_mark_dead(AtpState *s, u32 trace_id) {
  // Ids past the default trace cap exist only in a proof run: the
  // search treats them as absent, as the no-proof run must.
  if (trace_id == ATP_TRACE_NONE || trace_id >= s->t_vis) return;
  atp_hx_kill(s, 'k', trace_id);
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
  if (cp_trace == ATP_TRACE_NONE || cp_trace >= s->n_trace ||
      cp_trace >= s->t_vis)   // proof-only entry: absent to the search
    return 0;
  Term te = s->trace[cp_trace];
  if (term_tag(te) != TAG_CTR || term_ext(te) != TRACE_CP) return 0;
  u32 pa = (u32)term_val(term_ctr_at(te, 0));
  u32 pb = (u32)term_val(term_ctr_at(te, 1));
  return atp_trace_is_dead(s, pa) || atp_trace_is_dead(s, pb);
}

// Host abort hook (see thvm.h): NULL unless a host (e.g. the WL glue)
// installs a poll into Abort[] / TimeConstrained[].
int (*thvm_atp_abort_hook)(void) = NULL;

// Wall-clock microseconds from CLOCK_REALTIME -- portable across linux /
// macOS / freebsd and good enough for a >=1 second deadline budget.
// Every deadline and wall-limit test reads this clock: the competition
// kills on wall.
#ifdef THVM_ATP_TEST_CLOCK
// Test builds set the wall and CPU clocks by hand (tests/test_atp_guide.c
// drives --lrs to a deterministic prune with it).
static u64 g_atp_test_clock_us = 1u;
#endif

static u64 atp_now_us(void) {
#ifdef THVM_ATP_TEST_CLOCK
  return g_atp_test_clock_us;
#else
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
  return (u64)ts.tv_sec * 1000000ull + (u64)(ts.tv_nsec / 1000);
#endif
}

// Process CPU microseconds (CLOCK_PROCESS_CPUTIME_ID) -- the clock of
// the search disciplines ported from provers that time themselves in
// CPU: Twee's Task.hs reads getCPUTime, which GHC implements with this
// exact clock id.  Time spent descheduled on a loaded machine advances
// the wall clock but not this one, so a discipline gated on it fires at
// the same point of the search whatever else shares the machine.  The
// engine is single-threaded, so the process clock is the prover's own
// CPU.  Falls back to the wall clock where the CPU clock is missing.
static u64 atp_cpu_now_us(void) {
#if defined(CLOCK_PROCESS_CPUTIME_ID) && !defined(THVM_ATP_TEST_CLOCK)
  struct timespec ts;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
    return (u64)ts.tv_sec * 1000000ull + (u64)(ts.tv_nsec / 1000);
  }
#endif
  return atp_now_us();
}

#if defined(__linux__)
#include <linux/perf_event.h>   // atp_work_now_us instruction counter
#include <sys/syscall.h>
#endif

// Scale of the instruction work clock: user-space instructions per
// nominal microsecond.  Calibrated on the reference pod (AMD EPYC Genoa
// vCPU, equals-twee lane on LCL927-10: 8.9-9.1 G instructions per CPU
// second over whole runs, 9.5 G over the first 10 s), so the instruction
// clock ticks like the CPU clock there.  It sets only the clock's scale
// -- the twee discipline's 10 s cadence and the LRS simulated budget --
// never its determinism.
#define ATP_WORK_INSTR_PER_US 9000ull

// Clock of the load-sensitive search disciplines (the cp-set-ir budget
// discipline and the LRS horizon).  Default: process CPU, Twee's clock.
// A CPU clock ignores time spent descheduled but still drifts with how
// fast the host executes the same work (SMT siblings, shared caches,
// other tenants), so a discipline checking a sampled statistic at
// clock-chosen selection indices still forks otherwise identical runs.
// THVM_ATP_WORK_CLOCK=instr counts this process's user-space
// instructions through perf_event -- the counter behind Vampire's
// instruction limits (Lib/Timer.cpp) -- which is a function of the work
// done alone: the disciplines then fire at the same selection indices
// on every run, loaded or not, on any host running the same binary.
// Linux only; where the counter cannot be opened the CPU clock is used.
// THVM_ATP_WORK_CLOCK=steps is the gate clock (G2.1 / G2.2 in
// docs/ATP/enigma_portfolio.md): the engine's disciplines read a
// logical clock, ATP_WORK_STEP_US nominal microseconds per search event
// (g_atp_work_ticks: CP queue inserts and selection attempts), which code
// that only observes the search -- the history export, a guide's scoring
// -- cannot advance, so an observer's run and the plain run take every
// clock-gated decision at the same point by construction.  Its scale is
// arbitrary (it is not a deployment clock); the front-end budgets, which
// see no search events, keep the instruction counter (or the CPU clock).
// Never returns 0: the disciplines use 0 as their unlatched sentinel.
// The clock's mode, probed once: 1 = instructions (the counter opened),
// 0 = CPU; g_atp_work_steps = the step clock drives the engine.
#define ATP_WORK_STEP_US 10ull
static int g_atp_work_mode = -1;
static u8  g_atp_work_steps = 0u;
#if defined(__linux__)
static int g_atp_work_fd   = -1;
#endif

static int atp_work_mode(void) {
  if (g_atp_work_mode < 0) {
    g_atp_work_mode = 0;
    const char *e = getenv("THVM_ATP_WORK_CLOCK");
    if (e != NULL && strcmp(e, "steps") == 0) {
      g_atp_work_steps = 1u;
    }
#if defined(__linux__)
    if (e != NULL && (strcmp(e, "instr") == 0 || g_atp_work_steps)) {
      struct perf_event_attr pe;
      memset(&pe, 0, sizeof pe);
      pe.type           = PERF_TYPE_HARDWARE;
      pe.size           = sizeof pe;
      pe.config         = PERF_COUNT_HW_INSTRUCTIONS;
      pe.exclude_kernel = 1;
      pe.exclude_hv     = 1;
      g_atp_work_fd = (int)syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
      if (g_atp_work_fd >= 0) {
        g_atp_work_mode = 1;
      }
    }
#endif
  }
  return g_atp_work_mode;
}

// Raw work-clock reading in the clock's own unit: user-space
// instructions (mode 1) or CPU microseconds (mode 0).  *mode_out gets
// the mode.
static u64 atp_work_raw(int *mode_out) {
#if defined(__linux__)
  if (atp_work_mode() == 1) {
    u64 n = 0;
    if (read(g_atp_work_fd, &n, sizeof n) == (ssize_t)sizeof n) {
      *mode_out = 1;
      return n;
    }
  }
#endif
  *mode_out = 0;
  return atp_cpu_now_us();
}

// Exclusion windows (atp_work_pause / atp_work_resume): work a --proof
// run does only to keep its proof -- the trace entries past the default
// cap -- is taken off the clock, so every clock-gated decision fires at
// the same point of the search as in the no-proof run.  Windows nest;
// they cost nothing until a discipline has read the clock (its epoch:
// work before it never enters a delta).
static u64 g_atp_work_excl[2]  = { 0u, 0u };   // excluded, per mode
static u64 g_atp_work_pause0   = 0u;
static int g_atp_work_pause_mode = 0;
static u32 g_atp_work_depth    = 0u;
static u8  g_atp_work_live     = 0u;

static void atp_work_pause(void) {
  if (!g_atp_work_live) return;
  if (g_atp_work_depth++ == 0u)
    g_atp_work_pause0 = atp_work_raw(&g_atp_work_pause_mode);
}

static void atp_work_resume(void) {
  if (!g_atp_work_live || g_atp_work_depth == 0u) return;
  if (--g_atp_work_depth == 0u) {
    int mode = 0;
    u64 now = atp_work_raw(&mode);
    if (mode == g_atp_work_pause_mode && now >= g_atp_work_pause0)
      g_atp_work_excl[mode] += now - g_atp_work_pause0;
  }
}

// The work clock without arming the windows: a reader outside the
// engine's disciplines (the FOF front end's budgets, goal-narrow's
// endgame) reads it here.  Arming makes every trace entry past the cap
// pay two clock syscalls; a front-end read that armed them cost
// LCL556+1's fof-sos lane 6.7M reads, a fifth of its CPU.
static u64 atp_work_peek_us(void) {
  int mode = 0;
  u64 raw = atp_work_raw(&mode);
  u64 eff = (raw > g_atp_work_excl[mode]) ? raw - g_atp_work_excl[mode] : 0u;
  if (mode == 1) {
    return eff / ATP_WORK_INSTR_PER_US + 1u;
  }
  return eff ? eff : 1u;
}

// The clock-gated disciplines' own reading.  Under the step clock the
// exclusion windows stay unarmed: there is no proof-only work on it.
static u64 atp_work_now_us(void) {
  (void)atp_work_mode();
  if (g_atp_work_steps) {
    return g_atp_work_ticks * ATP_WORK_STEP_US + 1u;
  }
  g_atp_work_live = 1u;
  return atp_work_peek_us();
}

// The engine's clock without arming the windows (the LRS budget's start).
static u64 atp_work_engine_peek_us(void) {
  (void)atp_work_mode();
  if (g_atp_work_steps) {
    return g_atp_work_ticks * ATP_WORK_STEP_US + 1u;
  }
  return atp_work_peek_us();
}

// 1 when the engine's work clock is a function of the work done alone
// (instructions or steps): the LRS budget is then derived from it too.
static u8 atp_work_fixed(void) {
  return (u8)(atp_work_mode() == 1 || g_atp_work_steps);
}

// THVM_ATP_CLOCK_TRACE=<dir>: one line per clock-gated search decision
// (cp-set-ir sample check and sweep, LRS horizon) appended to
// <dir>/clock.<pid>.tsv, stamped with the selection index and both
// clocks, so repeated runs of one configuration can be diffed for WHERE
// in the search each decision fired.  The header names the schedule
// slice (FINDPROOF_SLICE) the process runs.  Unset: no file is opened
// and no clock is read beyond the gates' own.
static FILE *atp_clock_trace(void) {
  static int   state = 0;     // 0 = unprobed, 1 = open, 2 = off
  static FILE *fp    = NULL;
  if (state == 0) {
    state = 2;
    const char *dir = getenv("THVM_ATP_CLOCK_TRACE");
    if (dir != NULL && dir[0] != '\0') {
      char path[1024];
      snprintf(path, sizeof path, "%s/clock.%ld.tsv", dir, (long)getpid());
      fp = fopen(path, "a");
      if (fp != NULL) {
        const char *slice = getenv("FINDPROOF_SLICE");
        fprintf(fp, "# pid=%ld slice=%s\n", (long)getpid(),
                slice != NULL ? slice : "-");
        fflush(fp);
        state = 1;
      }
    }
  }
  return state == 1 ? fp : NULL;
}

// Close one trace line with the three clocks and flush it (a schedule
// kills losing slices with SIGKILL, so buffered lines would be lost).
static void atp_clock_trace_end(FILE *tf) {
  fprintf(tf, " wall=%llu cpu=%llu work=%llu\n",
          (unsigned long long)atp_now_us(),
          (unsigned long long)atp_cpu_now_us(),
          (unsigned long long)atp_work_now_us());
  fflush(tf);
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

// Absolute wall deadline (us) exposed to the LPO comparator via the
// zero-arg hook below.  The comparator has no AtpState handle, so the
// deadline lives here as a file-global that thvm_atp_arm_lpo_wall sets;
// it is independent of s->wall_deadline_us (the engine's own deadline).
static u64 g_atp_lpo_wall_deadline_us = 0u;

// Zero-arg deadline hook installed into src/lpo/_.c (thvm_lpo_set_deadline
// _hook).  lpo_rec / lpo_flat_rec poll it every 8192 recursion entries so
// a single huge-term LPO comparison self-aborts once the wall has passed
// -- the completion loop's between-steps wall check alone lets one such
// comparison overrun --wall by tens of seconds.  Mirrors
// atp_norm_deadline_fired's deadline + host-abort test.
static int atp_lpo_wall_hook(void) {
  if (g_atp_lpo_wall_deadline_us != 0u) {
    u64 now = atp_now_us();
    if (now != 0u && now >= g_atp_lpo_wall_deadline_us) return 1;
  }
  if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 1;
  return 0;
}

fn void thvm_atp_set_wall_deadline(AtpState *s, double seconds_from_now) {
  if (s == NULL) return;
  if (seconds_from_now <= 0.0) {
    s->wall_deadline_us = 0u;
    s->wall_budget_us   = 0u;
    return;
  }
  u64 now = atp_now_us();
  if (now == 0u) return;  // clock_gettime failed; leave deadline off
  s->wall_deadline_us = now + (u64)(seconds_from_now * 1e6);
  s->wall_budget_us   = (u64)(seconds_from_now * 1e6);
  s->wall_set_work_us = atp_work_fixed() ? atp_work_engine_peek_us() : 0u;
}

// Arm (seconds>0) or disarm (seconds<=0) the LPO comparator's mid-
// comparison wall self-abort, WITHOUT touching s->wall_deadline_us -- the
// engine's per-step and per-normalize deadline polls stay exactly as they
// were, and only the LPO recursion gains the bound.  The completion driver
// arms this unconditionally: an --ord=lpo run can spend tens of seconds in
// ONE huge-term comparison inside a single step, and the driver polls the
// wall only BETWEEN steps, so without this a slow comparison overruns
// --wall (RC=124).  KBO comparisons are O(|s|+|t|) and never trip it, so a
// default (KBO) run is unaffected.
fn void thvm_atp_arm_lpo_wall(double seconds_from_now) {
  if (seconds_from_now <= 0.0) {
    g_atp_lpo_wall_deadline_us = 0u;
    thvm_lpo_set_deadline_hook(NULL);   // disarm: no stale deadline leaks
    return;
  }
  u64 now = atp_now_us();
  if (now == 0u) return;  // clock_gettime failed; leave the hook off
  g_atp_lpo_wall_deadline_us = now + (u64)(seconds_from_now * 1e6);
  thvm_lpo_set_deadline_hook(&atp_lpo_wall_hook);
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

// WM KPV_KillParent face discipline for the uid layer, ONE-SIDE
// removals only (Interreduktion.c:272 "aber nur diese eine Seite
// entfernen"): RE_GleichungEntfernen kills children ONLY for the
// DISTINGUISHED direction (RUndEVerwaltung.c:592-598).  The full-pull
// sites (equation LHS reducible, Interreduktion.c:293-301) pass the
// distinguished object itself and kill UNCONDITIONALLY -- those keep
// the direct atp_uid_mark_dead (face-scoping them regressed
// DoubleNegation 2849 -> 1005).  removed_side = the thvm side whose
// reduction fired; WM's distinguished face = thvm side dist_rhs (the
// wm_order mirror label, read BEFORE atp_wmo_remove_trace evicts the
// registration).  The @2926 OrAssociativity FIFO victim was a child
// thvm kept alive because the eq-rhs retire skipped the kill even when
// dist_rhs = 1 made that face WM's distinguished one.
static void atp_uid_kill_face(AtpState *s, u32 slot, u8 removed_side) {
  if (s->r_uid == NULL) return;
  if (s->r_orient[slot]) { atp_uid_mark_dead(s, s->r_uid[slot]); return; }
  u8 dr = (s->use_emission_order && s->wmo != NULL)
        ? atp_wmo_dist_rhs_of(s, s->r_uid[slot]) : 0u;
  if (removed_side == dr) atp_uid_mark_dead(s, s->r_uid[slot]);
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Forward decl: Twee full-R interreduction pass (defined with the
// interreduce machinery below); called from the step tail above it.
static void thvm_atp_interreduce_full(AtpState *s);
// Forward decl: the joinability oracle (defined ~_.c:18422); the full-R
// interreduction re-uses it under an r_dead[i] self-exclusion.
static u8 atp_cp_trivially_joinable(AtpState *s, Term *lhs, Term *rhs);
#endif

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Forward decl: Term-side simpler-join wrapper for the pop path
// (defined next to atp_cp_trivially_joinable below).
static u8 atp_cp_sj_joinable_term(AtpState *s, Term l, Term r);
#endif

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
  u64 eq_off = 0u;
  Term c_lhs = lhs, c_rhs = rhs;
  if (s->trace_pack) {
    eq_off = atp_trace_eq_store(s, lhs, rhs);
    c_lhs = term_new(0, TAG_NUM, 0, 0);   // sentinel; real pair off-heap
    c_rhs = c_lhs;
  }
  Term children[7 + ATP_PROOF_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, rule_idx);
  children[2] = c_lhs;
  children[3] = c_rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5u + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len]      = term_new(0, TAG_NUM, 0, side);
  children[5u + pos_len + 1u] = term_new(0, TAG_NUM, 0, fwd);
  if (s->n_trace >= s->t_vis) s->trace_x_heap = 1u;
  s->trace[s->n_trace] = term_new_ctr(TRACE_NORM_STEP, children,
                                      7u + pos_len);
  u32 idx = s->n_trace;
  if (s->trace_pack) s->trace_eq_off[idx] = eq_off;
  s->n_trace++;
  return idx;
}

// Packed-offset sibling of atp_trace_push_norm_step: the pair is
// already in the off-heap store (span-sourced pack); the entry's
// children carry the usual sentinels.  trace_pack-only by contract.
static u32 atp_trace_push_norm_step_off(AtpState *s, u32 p_a, u32 rule_idx,
                                        u64 eq_off, u8 side, u8 fwd,
                                        const u8 *pos, u8 pos_len) {
  if (s == NULL || !s->trace_pack || eq_off == 0u) return ATP_TRACE_NONE;
  if (!atp_trace_ensure(s)) return ATP_TRACE_NONE;
  Term sent = term_new(0, TAG_NUM, 0, 0);
  Term children[7 + ATP_PROOF_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, rule_idx);
  children[2] = sent;
  children[3] = sent;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5u + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len]      = term_new(0, TAG_NUM, 0, side);
  children[5u + pos_len + 1u] = term_new(0, TAG_NUM, 0, fwd);
  if (s->n_trace >= s->t_vis) s->trace_x_heap = 1u;
  s->trace[s->n_trace] = term_new_ctr(TRACE_NORM_STEP, children,
                                      7u + pos_len);
  u32 idx = s->n_trace;
  s->trace_eq_off[idx] = eq_off;
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
  static int dl = -1;
  if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
  u32 in_sz = dl ? atp_symbol_count(t) : 0;
  for (u32 it = 0; it < step_cap; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                     &fwd, &fired);
    if (!fired) break;
    if (dl) { u32 z = atp_symbol_count(t2);
      if (z > 100000u) fprintf(stderr,
        "RECBIG in=%u step=%u t2=%u rule=%u fwd=%u nr=%u phase=%s\n",
        in_sz, it, z, rule, fwd, s->n_rules, g_thvm_phase); }
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
  static int dl = -1;
  if (dl < 0) dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
  u32 in_sz = dl ? atp_symbol_count(t) : 0;
  for (u32 it = 0; it < step_cap; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step_slice(s, t, pos, 0u,
                                           lhs_arr, rhs_arr, n,
                                           &rule, &pos_len, &fwd, &fired);
    if (!fired) break;
    if (dl) { u32 z = atp_symbol_count(t2);
      if (z > 100000u) { fprintf(stderr,
        "SLICEBIG in=%u step=%u t2=%u rule=%u fwd=%u phase=%s\n",
        in_sz, it, z, rule, fwd, g_thvm_phase); } }
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
static AtpStatus thvm_atp_step_impl(AtpState *s);

// Mem-census bracket around one whole step (see g_ftnet_step).
fn AtpStatus thvm_atp_step(AtpState *s) {
#ifdef THVM_ATPFT_ALLOC
  i64 _sb = ftnet_now();
#endif
  AtpStatus rv = thvm_atp_step_impl(s);
#ifdef THVM_ATPFT_ALLOC
  g_ftnet_step += ftnet_now() - _sb;
#endif
  if (atp_rj_check_mode()) atp_rj_verify(s);
  return rv;
}

// THVM_ATP_RECYCLE_DEBUG diagnostics: attribute WHICH abort producer
// fired (host abort hook vs heap-frac guard vs RSS guard) -- the three
// sites collapse to one silent ATP_ABORTED otherwise.
static void atp_dbg_abort_site(const char *site, const AtpState *s) {
  static int v = -1;
  if (v < 0) v = getenv("THVM_ATP_RECYCLE_DEBUG") ? 1 : 0;
  if (v) {
    fprintf(stderr, "[atp-abort] %s step=%u\n", site, s->step);
    fflush(stderr);
  }
}

// Live (current, NOT peak) process RSS in bytes; 0 when unmeasurable.
// ru_maxrss is the high-water mark and would latch a guard forever once
// any earlier work in the process touched the cap -- see the RSS guard
// in thvm_atp_step_impl.
fn long long thvm_atp_live_rss_bytes(void) {
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
  return rss;
}

static AtpStatus thvm_atp_step_impl(AtpState *s) {
  if (s == NULL) return ATP_QUEUE_EMPTY;

  // WM loader intake: before the first pop, permute the queued axiom
  // set into Waldmeister's canonical sort order and stamp it ultimate
  // (SpezNormierung + initial=ultimate; see wm_intake.c).
  if (s->use_intake_order && !s->wm_intake_done) {
    s->wm_intake_done = 1u;
    atp_wm_intake_canonicalize(s);
    atp_hx_restamp_queue(s);
    atp_topk_on_intake(s);
    if (s->guide != NULL) {
      atp_guide_on_intake(s);
    }
  }

  AtpStatus goal = thvm_atp_goal_check(s);
  // The goal check may move the goal's normal form: the selection layer
  // scores later pushes against the new one.
  if (s->guide != NULL) {
    atp_guide_on_goal(s);
  }
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
  if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) {
    atp_dbg_abort_site("abort-hook", s);
    return ATP_ABORTED;
  }

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
    // Tunable via THVM_ATP_HEAP_ABORT_FRAC (atp_heap_abort_frac).
    double frac = atp_heap_abort_frac();
    u64 lo, cap;
    if (gc_enabled()) {
      lo  = gc_from_start();
      cap = gc_from_end() - lo;
    } else {
      lo  = 0;
      cap = thvm_heap_cells();
    }
    if (cap > 0 && (double)(HEAP_NEXT - lo) > frac * (double)cap) {
      atp_dbg_abort_site("heap-frac", s);
      return ATP_ABORTED;
    }
  }

  // Process-RSS memory guard, GROWTH-relative.  The heap-frac guard
  // above only sees the dyn heap; trace buffers, CP arena, FV/RI/
  // discrim indices, the WL ProofObject reconstruction state, and the
  // kernel/paclet overhead all live OUTSIDE it.  A run can exhaust
  // system RAM with a perfectly happy semi-space.  Poll the *current*
  // RSS every 1024 steps (live, not ru_maxrss -- the high-water mark
  // would latch forever once any earlier work touched the cap).
  //
  // The bound applies to the RUN'S OWN GROWTH: live RSS minus the
  // baseline sampled at this run's FIRST poll (step 0).  An absolute
  // cap misattributed the HOST's ambient memory to the proof run --
  // inside WolframKernel, a cold-JIT TNetTrain transiently lifts
  // ambient RSS past 8 GB and every subsequent TFindProof insta-
  // aborted at its step-0 poll until the ambient decayed (the silent
  // post-train $Failed wedge; see AtpState.rss_guard_baseline).  A
  // genuinely diverging saturation still allocates multi-GB across its
  // own life and trips the same 8 GB default; THVM_ATP_RSS_ABORT_MB
  // overrides (now bounding growth), 0 disables.
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
      long long rss = thvm_atp_live_rss_bytes();
      // Proof-only trace bytes are not the search's (atp_trace_node).
      if (rss > 0) rss -= (long long)s->trace_x_bytes;
      // Nor are the selection layer's own allocations (src/atp/guide.c).
      if (rss > 0 && s->guide != NULL) rss -= atp_guide_bytes(s);
      if (rss > 0 && s->rss_guard_baseline == 0) {
        s->rss_guard_baseline = rss;   // first successful poll: baseline
      } else if (rss > 0 && rss - s->rss_guard_baseline > rss_cap_bytes) {
        { static int v = -1;
          if (v < 0) v = getenv("THVM_ATP_RECYCLE_DEBUG") ? 1 : 0;
          if (v) {
            fprintf(stderr,
                    "[atp-abort] rss-guard step=%u rss=%lldMB base=%lldMB\n",
                    s->step, rss >> 20, s->rss_guard_baseline >> 20);
            fflush(stderr);
          } }
        return ATP_ABORTED;
      }
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

  // Otter loop: the input clauses pass through the replay too (Vampire
  // routes them through the same unprocessed loop), once, before the
  // first selection.
  if (s->use_otter && !s->otter_primed) atp_otter(s);
  if (s->heap_full) {
    atp_dbg_abort_site("heap-full", s);
    return ATP_ABORTED;
  }

  Term cp_lhs = 0, cp_rhs = 0;
  if (!thvm_atp_select_cp(s, &cp_lhs, &cp_rhs)) {
    return ATP_QUEUE_EMPTY;
  }
  // Otter loop: the selected CP stops being a passive demodulator
  // before its selection-time normalize (else it joins against itself).
  if (s->use_otter) {
    atp_otter_popkill(s, s->last_popped_seq, ATP_HX_OT_RETIRE_SEL);
  }
  // The rule set this CP normalizes on: its orient records' view.
  const u32 rj_pop = s->rj_n;

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
  // drc=off keep lane, selection-time content: the pair the kept CP
  // re-enters R/E with when its pop normalize joins.  Non-record: the
  // queued (push-treated) sides; record mode: the trace RAW pair (the
  // recorded chain is rewound on a keep, so the rule content must be
  // the chain start -- no dangling NORM_STEPs).
  Term drc_pop_l = cp_lhs, drc_pop_r = cp_rhs;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Pop simpler-join verdict already decided on the FT-direct branch
  // (skip the Term-side re-check at the common joined gate below).
  u8 sj_checked = 0u;
#endif
#ifdef THVM_ATPFT_RULES
  // Survivor NF spans from the FT-direct pop treatment (set only on the
  // mixmost non-record path); bracket thvm_atp_orient_and_add below so
  // atp_push_rule can adopt them as the new rule's mirror.
  AtpFtCell *pop_nf_l = NULL;
  AtpFtCell *pop_nf_r = NULL;
  u8 lazy_commit = 0u;   // survivor committed via sentinels (no decode)
#endif
#ifdef ATP_CP_GROUND_JOIN
  // Lazy (selection-time) ground-joinability -- Twee's redundancy
  // discipline.  The expensive ordered-set-partition GJ check runs on the
  // SELECTED, RAW popped CP (cp_lhs/cp_rhs == the push-normalized form
  // stored in the queue -- the SAME input push-side GJ consumes) BEFORE
  // the pop R+E normalize, so a ground-joinable pick is dropped without
  // paying its normalize AND without the per-generated-CP push tax (push
  // GJ was O(generated CPs) and dominated GRP766 cp-gen at 8.6s/92%).
  // Sound: a ground-joinable CP is redundant (Martin-Nipkow / Twee), so
  // dropping before orientation preserves completeness.  Side order
  // (cp_rhs, cp_lhs) mirrors the push-side call, corpus-validated.  Skips
  // the normalize entirely, so no trace/heap chain was recorded yet --
  // just bump the step and continue.  Gated by use_lazy_ground_join
  // (default off -> byte-identical); push-side use_ground_join is off in
  // this mode so GJ is paid once, at pop.
  if (s->use_lazy_ground_join &&
      atp_cp_ground_joinable(s, cp_rhs, cp_lhs)) {
    s->n_cps_ground_joinable++;
    atp_hx_outcome(s, ATP_HX_OUT_LAZYGJ);
#ifdef THVM_ATPFT_RULES
    // The select-time byte->FT stash was decoded for the pop treatment
    // this drop skips -- return the spans so they don't leak.
    if (g_pop_ft_l != NULL) {
      ft_free_span((AtpFt *)s->ft_arena_ptr, g_pop_ft_l, g_pop_ft_l->end);
      g_pop_ft_l = NULL;
    }
    if (g_pop_ft_r != NULL) {
      ft_free_span((AtpFt *)s->ft_arena_ptr, g_pop_ft_r, g_pop_ft_r->end);
      g_pop_ft_r = NULL;
    }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    atp_pop_sj_peak_drop(s);   // pop-peak stash: same skipped-treatment drop
#endif
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }
#endif
  u64 _ph_pop_t0 = atp_phase_now(); g_thvm_phase = "pop_normalize";
  g_ft_region = 1u;
  // THVM_ATP_POPALLOC_TRACE: report pops whose normalize allocates more
  // than ~1M Term cells (the avalanche heap-bomb diagnostic; see
  // project_atp_wm_speed_profiled 2026-07-02).  Behaviour-neutral.
  static int _popalloc_trace = -1;
  if (_popalloc_trace < 0) _popalloc_trace = atp_env_on("THVM_ATP_POPALLOC_TRACE");
  static int pop_trace = -1;
  u64 _popalloc_h0 = _popalloc_trace ? HEAP_NEXT : 0u;
  if (_popalloc_trace && s->step >= 2800u) {
    fprintf(stderr, "POPENTER step=%u heap=%llu raw_sz=%u+%u rules=%u\n",
            s->step, (unsigned long long)_popalloc_h0,
            atp_symbol_count(cp_lhs), atp_symbol_count(cp_rhs), s->n_rules);
  }
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
    // Under cp_trace_lean the TRACE_CP children 2/3 are NUM sentinels, so the
    // correct raw pair is the just-unpacked cp_lhs/cp_rhs (from cp_packed) --
    // skip the trace override entirely.  (trace_pack still overrides via the
    // off-heap eq_load; the plain path reads the materialized children.)
    if (!s->cp_trace_lean &&
        src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
      Term cp_te = s->trace[src_trace];
      if (term_tag(cp_te) == TAG_CTR && term_ext(cp_te) == TRACE_CP &&
          term_ctr_n(cp_te) >= 4u) {
        if (!atp_trace_eq_load(s, src_trace, &raw_lhs, &raw_rhs)) {
          raw_lhs = term_ctr_at(cp_te, 2);
          raw_rhs = term_ctr_at(cp_te, 3);
        }
      }
    }
    drc_pop_l = raw_lhs;
    drc_pop_r = raw_rhs;
    // The replay phases the two WM flag sets exactly as the engine
    // applied them: first the generation-time treatment (KPBehandelt,
    // `-kg "r"` -> doR only -- atp_cp_trivially_joinable's rules-only
    // normalize), then the selection-time one (KPV_Select, `-ks
    // "r:e:s:p"` -> doR+doE).  A single interleaved doR+doE pass from
    // the raw CP records grounded-template equation steps ahead of the
    // oriented fixpoint -- steps the engine never fired and WM's
    // protocol never contains.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (s->use_mixmost_nf) {
      // WM innermost order (NormalformInnermost) WITH norm-step
      // recording: the FT mixmost record path yields the same join
      // verdict as the non-record reroute in atp_rewrite_normalize_
      // ordered AND emits a sound TRACE_NORM_STEP chain (each splice is
      // a real rule/equation rewrite) the ProofObject extractor replays.
      // The Term-side two-phase below uses outermost order, whose verdict
      // over-joins the CPs WM keeps -- the goal-closer is lost and the
      // proof never closes.  doE=1 full pass per side matches the
      // selection-time `-ks "r:e:s:p"` flag set.
      atp_ft_mirror_ensure(s);
      AtpFt *a = (AtpFt *)s->ft_arena_ptr;
      // use_pop_rules_only: Twee base discipline in record mode too --
      // search and proof replay must walk the same trajectory.
      AtpFtCell *(*pop_nf_rec)(AtpState *, AtpFtCell *, u32, Term, u8,
                               u32 *) =
          s->use_pop_rules_only ? atp_rules_only_normalize_ft_record
                                : atp_rewrite_normalize_ft_record;
      AtpFtCell *lf = pop_nf_rec(
          s, ft_from_term(a, raw_lhs, 0), NORM_CAP, raw_rhs, 0u,
          &chain_tail);
      l = ft_to_term(lf);
      AtpFtCell *rf = pop_nf_rec(
          s, ft_from_term(a, raw_rhs, 0), NORM_CAP, l, 1u, &chain_tail);
      r = ft_to_term(rf);
    } else
#endif
    {
    g_atp_proof_oriented_only = 1u;
    Term l_r = atp_rewrite_normalize_record(s, raw_lhs, raw_rhs, 0u,
                                            &chain_tail, NORM_CAP);
    g_atp_proof_oriented_only = 0u;
    // use_pop_rules_only skips the doE stage (Twee base discipline):
    // the oriented pass above is the whole normalize.
    l = s->use_pop_rules_only
        ? l_r
        : atp_rewrite_normalize_record(s, l_r, raw_rhs, 0u,
                                       &chain_tail, NORM_CAP);
    g_atp_proof_oriented_only = 1u;
    Term r_r = atp_rewrite_normalize_record(s, raw_rhs, l, 1u,
                                            &chain_tail, NORM_CAP);
    g_atp_proof_oriented_only = 0u;
    r = s->use_pop_rules_only
        ? r_r
        : atp_rewrite_normalize_record(s, r_r, l, 1u,
                                       &chain_tail, NORM_CAP);
    }
  } else {
    // Per-pick pop-treatment verdict trace (THVM_ATP_POP_TRACE), mirroring
    // WM's WM_POPDUMP (KPV_Select) so the two engines' keep/drop sequences
    // can be aligned programmatically.  Default-off, behaviour-neutral.
    // (Probed here, before the normalize, so the FT-direct joined path
    // below knows whether the POPV print needs real NF Terms.)
    if (pop_trace < 0) pop_trace = atp_env_on("THVM_ATP_POP_TRACE");
#if defined(ATP_ORDERED_REWRITE) && defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (s->use_mixmost_nf) {
      // FT-direct pop treatment (KPV_Select R+E normalize): the input
      // spans come from the select-time byte->FT stash (zero Term hop;
      // ft_from_term only on the implicit-slot fallback), the join
      // verdict is taken on the FLAT NFs, and heap Terms materialise
      // only for survivors.  A joined pop -- the dominant outcome of a
      // deep completion -- aliases l/r to cp_lhs purely to satisfy the
      // kbo_eq(l, r) joined gate below (nothing else reads them on that
      // path: the branch rewinds trace + heap and returns), so it costs
      // ZERO interchange conversions.  Byte-identical: ft_eq on NF spans
      // is the Term-side kbo_eq verdict (structural iso, the
      // atp_cp_trivially_joinable_ft precedent), and the POPV trace
      // still prints the true NF pair when enabled.
      atp_ft_mirror_ensure(s);
      AtpFt *a = (AtpFt *)s->ft_arena_ptr;
      // Twee base discipline (use_pop_rules_only): oriented rules only;
      // the unorientable tree joins only via the gated sj pass below.
      AtpFtCell *(*pop_nf)(AtpState *, AtpFtCell *, u32) =
          s->use_pop_rules_only ? atp_rules_only_normalize_ft
                                : atp_rewrite_normalize_ft;
      AtpFtCell *lf = pop_nf(
          s, g_pop_ft_l != NULL ? g_pop_ft_l : ft_from_term(a, cp_lhs, 0),
          NORM_CAP);
      AtpFtCell *rf = pop_nf(
          s, g_pop_ft_r != NULL ? g_pop_ft_r : ft_from_term(a, cp_rhs, 0),
          NORM_CAP);
      g_pop_ft_l = NULL;
      g_pop_ft_r = NULL;
      u8 pop_ft_joined = (u8)ft_eq(lf, rf);
      // Simpler-join at selection (opt-in --simpler-join): a popped CP
      // whose R+E NFs stay distinct gets the Twee-discipline gated join
      // pass -- the same redundancy verdict as the ft_eq drop.  The
      // queue's pop-peak stash (decoded by select_cp, same dense var
      // space as the byte-decoded pop spans) arms the checkOrder/step3
      // legs -- Twee's consider re-joins WITH the overlap top at pop;
      // a NULL stash keeps the peakless degrade.
      if (!pop_ft_joined && s->use_simpler_join) {
        sj_checked = 1u;
        if (g_pop_sj_peak != NULL) atp_sj_peak_arm_ft(g_pop_sj_peak);
        pop_ft_joined = atp_cp_sj_joinable_ft(s, lf, rf);
        atp_sj_peak_disarm();
        if (pop_ft_joined) s->n_cps_dropped_simpler_join++;
        {
          static int ppd2 = -1;
          if (ppd2 < 0) ppd2 = atp_env_on("THVM_ATP_POP_PEAK_DEBUG");
          if (ppd2) {
            fprintf(stderr, "[popsj] arm=ft peak=%s joined=%u\n",
                    (g_pop_sj_peak != NULL) ? "yes" : "no", pop_ft_joined);
          }
        }
      }
      atp_pop_sj_peak_drop(s);
      if (pop_ft_joined) {
        if (pop_trace) {
          l = ft_to_term(lf);
          r = l;                     // NF pair joins; print the lhs twice
        } else {
          l = cp_lhs;
          r = cp_lhs;                // only kbo_eq(l, r) == 1 is observed
        }
      } else {
        // FT-native -ks "p"/"s" drop gates, run in the legacy order
        // (perm THEN pop-subsume) on the flat NFs so a dropped pop
        // never materializes heap Terms.  Negative verdicts fall
        // through to the decode + legacy gates, which re-check
        // Term-side -- only an FT false POSITIVE could diverge, and
        // THVM_ATP_POPSUB_FT_DIFF certifies every FT drop against the
        // full Term verdict chain.  Non-plain order configs skip the
        // pop-subsume half (its KBO_UN test has a certified FT sibling
        // only for plain KBO); non-AC builds skip the perm half (the
        // legacy env-mask path stays Term-side).
        static int psdiff = -1;
        if (psdiff < 0) psdiff = atp_env_on("THVM_ATP_POPSUB_FT_DIFF");
        int popsub_ft = -1;           // -1 = gate not evaluated
        u8 ft_drop = 0u;              // 1 = PERMSUB, 2 = SUBSUMED
#ifdef THVM_ATP_AC
        if (s->use_perm_subsume && g_atp_perm_subsume_mask != 0ull &&
            atp_cp_perm_subsumed_ft(lf, rf)) {
          ft_drop = 1u;
        }
#endif
        if (ft_drop == 0u &&
            s->use_pop_subsume && s->n_unorient > 0u &&
            atp_order_is_plain_kbo(s) &&
            !(s->use_perm_subsume && g_atp_perm_subsume_mask != 0ull
#ifdef THVM_ATP_AC
              && 0  /* perm verdict decided above */
#endif
              ) &&
            thvm_kbo_ft(lf, rf, s->kbo) == KBO_UN &&
            (popsub_ft = atp_pop_eq_subsumed_ft(s, lf, rf)) == 1) {
          ft_drop = 2u;
        }
        if (ft_drop != 0u) {
          if (psdiff || pop_trace) {
            Term pl = ft_to_term(lf);
            Term pr = ft_to_term(rf);
            if (psdiff) {
              u8 term_perm = (u8)(s->use_perm_subsume &&
                                  g_atp_perm_subsume_mask != 0ull &&
                                  atp_cp_perm_subsumed(pl, pr));
              u8 ok = (ft_drop == 1u)
                        ? term_perm
                        : (u8)(!term_perm &&
                               atp_compare(s, pl, pr) == KBO_UN &&
                               atp_pop_eq_subsumed(s, pl, pr));
              if (!ok) {
                fprintf(stderr,
                        "POPSUB_FT DIFF: FT drop kind=%u, Term disagrees\n",
                        ft_drop);
                abort();
              }
            }
            if (pop_trace) {
              fprintf(stderr, "POPV pick=%u verdict=%s lhs=",
                      s->cp_select_count,
                      ft_drop == 1u ? "PERMSUB" : "SUBSUMED");
              atp_dbg_print_term(stderr, pl);
              fputs(" rhs=", stderr);
              atp_dbg_print_term(stderr, pr);
              fputc('\n', stderr);
            }
          }
          if (ft_drop == 1u) s->n_cps_dropped_perm_subsumed++;
          else               s->n_cps_dropped_pop_subsumed++;
          atp_hx_outcome(s, ft_drop == 1u ? ATP_HX_OUT_PERMSUB
                                          : ATP_HX_OUT_SUBSUMED);
          g_ft_region = 0u;
          if (g_atp_phase_enabled)
            g_atp_phase_us_pop_normalize += atp_now_us() - _ph_pop_t0;
          thvm_atp_heap_reset(hcp_norm);
          s->step++;
          return ATP_RUNNING;
        }
        // FT-first orient refusal: a survivor atp_push_rule would REFUSE
        // as a var-normalized duplicate decodes nothing.  Decided only
        // for the plain-KBO orientable cases with every special orient
        // mode off (force-def, legacy mask-0 perm, fwd-subsume) and only
        // when the canonical renumber is exact (distinct-var count under
        // the REWRITE_MAX_VAR cliff, where the FT and Term renumbers
        // coincide by construction); everything else falls through to
        // the decode + full Term orient.  An ACCEPTED rule also falls
        // through -- push_rule re-verifies Term-side, so only the
        // refusal is decided here.
        {
          if (atp_order_is_plain_kbo(s) && !atp_force_def_orient_env() &&
              !s->use_force_def_orient && !s->use_fwd_subsume &&
              !(s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull)) {
            KboCmp oc = thvm_kbo_ft(lf, rf, s->kbo);
            if (oc == KBO_GT || oc == KBO_LT) {
              AtpFt *da = (AtpFt *)s->ft_arena_ptr;
              AtpFtCell *dl = ft_clone_span(da, oc == KBO_GT ? lf : rf, 0);
              AtpFtCell *dr = ft_clone_span(da, oc == KBO_GT ? rf : lf, 0);
              u32 nv_l = ft_distinct_var_count(dl, dr);
              if (nv_l < REWRITE_MAX_VAR) {
                ft_normalize_vars_pair(dl, dr);
                u8 dup = 0u, unknown = 0u;
                for (u32 k = 0; k < s->n_rules; k++) {
                  if (s->lhs_ft[k] == NULL || s->rhs_ft[k] == NULL) {
                    unknown = 1u;
                    break;
                  }
                  if (ft_eq(s->lhs_ft[k], dl) && ft_eq(s->rhs_ft[k], dr)) {
                    dup = 1u;
                    break;
                  }
                }
                if (!unknown && dup) {
                  if (pop_trace) {
                    Term pl = ft_to_term(lf);
                    Term pr = ft_to_term(rf);
                    fprintf(stderr, "POPV pick=%u verdict=%s lhs=",
                            s->cp_select_count, "REFUSED");
                    atp_dbg_print_term(stderr, pl);
                    fputs(" rhs=", stderr);
                    atp_dbg_print_term(stderr, pr);
                    fputc('\n', stderr);
                  }
                  ft_free_span(da, dl, dl->end);
                  ft_free_span(da, dr, dr->end);
                  g_ft_region = 0u;
                  if (g_atp_phase_enabled)
                    g_atp_phase_us_pop_normalize += atp_now_us() - _ph_pop_t0;
                  atp_hx_outcome(s, ATP_HX_OUT_REFUSED);
                  s->step++;
                  return ATP_RUNNING;
                }
              }
              ft_free_span(da, dl, dl->end);
              ft_free_span(da, dr, dr->end);
            }
          }
        }
        // Lazy commit (THVM_ATP_LAZY_RULE_TERMS + TRACE_LEAN_RULES):
        // the survivor's rule is born from the spans -- orient receives
        // per-side NUM sentinel handles and every content step
        // intercepts on the bracket; nothing is decoded.  Any special
        // orient mode falls back to the decode below.
        static int lazy_dbg = -1;
        if (lazy_dbg < 0) lazy_dbg = getenv("THVM_ATP_LAZY_DBG") != NULL;
        if (lazy_dbg) {
          fprintf(stderr,
                  "LAZYDBG on=%d plain=%d fdo=%u fwd=%u flat=%u mnf=%u "
                  "rec=%u permz=%d nv=%u\n",
                  atp_lazy_rules_on(), atp_order_is_plain_kbo(s),
                  s->use_force_def_orient, s->use_fwd_subsume,
                  s->use_flat_subsume, s->use_mnf, s->record_norm_steps,
                  (s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull),
                  ft_distinct_var_count(lf, rf));
        }
        if (atp_lazy_rules_on() && atp_order_is_plain_kbo(s) &&
            !s->use_force_def_orient &&
            !s->use_fwd_subsume && !s->record_norm_steps &&
            !(s->use_perm_subsume && g_atp_perm_subsume_mask == 0ull) &&
            !atp_force_def_orient_env() &&
            ft_distinct_var_count(lf, rf) < REWRITE_MAX_VAR) {
          KboCmp loc = thvm_kbo_ft(lf, rf, s->kbo);
          u8 un_lazy = 0u;
          if (loc == KBO_UN) {
            // UN (unfailing) lazy ride: every Term reader in the UN
            // orient arm has a span sibling (lr_sortieren dispatches,
            // push_rule verdicts off the mirrors, eset patterns +
            // subjects + subsumer shapes read spans, fvi grounds
            // spans).  Decline the modes whose readers stay Term-side
            // (cp-side geometry, comm widening, descent/non-flat
            // eset) and require the FT pop-subsume gate to have
            // decided cleanly: verdict 0 (2 = a NULL mirror made it
            // undecidable; -1 with live E-members = gate skipped).
            static int eset_descent2 = -1;
            if (eset_descent2 < 0)
              eset_descent2 =
                  getenv("THVM_ATP_ESET_DESCENT") != NULL ? 1 : 0;
            un_lazy = (u8)(!s->use_cp_side && !s->use_comm_subsume &&
                           (!s->use_eset_subsume ||
                            (s->use_flat_subsume && !eset_descent2)) &&
                           (!s->use_pop_subsume ||
                            s->n_unorient == 0u || popsub_ft == 0));
#ifndef THVM_ATP_AC
            // Non-AC build: a nonzero perm mask keeps the perm gate
            // Term-side (the FT sibling is compiled out).
            if (s->use_perm_subsume && g_atp_perm_subsume_mask != 0ull)
              un_lazy = 0u;
#endif
          }
          if (loc == KBO_GT || loc == KBO_LT || un_lazy) {
            l = term_new(0, TAG_NUM, 0, 0xFA151u);
            r = term_new(0, TAG_NUM, 0, 0xFA152u);
            g_lazy_rule.span_l = lf;
            g_lazy_rule.span_r = rf;
            g_lazy_rule.sent_l = l;
            g_lazy_rule.sent_r = r;
            lazy_commit = 1u;
          }
        }
        if (!lazy_commit) {
          l = ft_to_term(lf);
          r = ft_to_term(rf);
          pop_nf_l = lf;             // survivor NFs: rule-mirror adoption
          pop_nf_r = rf;
        }
      }
    } else
#endif
    {
    // Live-rule-set call: atp_rewrite_normalize -> atp_rewrite_normalize_
    // ordered, which routes through the FT mixmost (WM innermost) when
    // use_mixmost_nf is set -- so the pop-time join verdict matches WM.
    // use_pop_rules_only: Twee base discipline -- oriented rules only,
    // the unorientable tree joins only via the gated sj pass below.
    if (s->use_pop_rules_only) {
      l = atp_rules_only_normalize(s, cp_lhs, NORM_CAP);
      r = atp_rules_only_normalize(s, cp_rhs, NORM_CAP);
    } else {
      l = atp_rewrite_normalize(s, cp_lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
      r = atp_rewrite_normalize(s, cp_rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
    }
    }
  }
  g_ft_region = 0u;
  if (g_atp_phase_enabled) g_atp_phase_us_pop_normalize += atp_now_us() - _ph_pop_t0;
  if (_popalloc_trace) {
    u64 d = HEAP_NEXT - _popalloc_h0;
    if (d > 1000000u) {
      fprintf(stderr, "POPALLOC step=%u cells=%llu raw_sz=%u+%u rules=%u\n",
              s->step, (unsigned long long)d,
              atp_symbol_count(cp_lhs), atp_symbol_count(cp_rhs), s->n_rules);
    }
  }

  if (pop_trace < 0) pop_trace = atp_env_on("THVM_ATP_POP_TRACE");
#define ATP_POPV(verdict) do { \
    if (pop_trace) { \
      Term _pl = l, _pr = r; \
      if (term_tag(_pl) == TAG_NUM && lazy_commit) \
        _pl = ft_to_term(g_lazy_rule.span_l); \
      if (term_tag(_pr) == TAG_NUM && lazy_commit) \
        _pr = ft_to_term(g_lazy_rule.span_r); \
      fprintf(stderr, "POPV pick=%u verdict=%s lhs=", s->cp_select_count, (verdict)); \
      atp_dbg_print_term(stderr, _pl); fputs(" rhs=", stderr); \
      atp_dbg_print_term(stderr, _pr); fputc('\n', stderr); \
    } } while (0)

  u8 pop_joined = kbo_eq(l, r);
  // drc=off keep lane at selection: a folded-class CP whose pop
  // normalize joins re-enters R/E with its pre-normalize content
  // instead of being discarded (Vampire drc=off keeps the
  // demodulation-redundant clause).  The recorded NORM_STEP chain (if
  // any) is rewound exactly like the joined drop -- the kept content
  // IS the chain start, so nothing dangles.
  if (pop_joined && atp_drc_armed(s) &&
      (!atp_drc_gd_gate() || s->last_popped_gd) &&
      drc_pop_l != 0 && drc_pop_r != 0 &&
      term_tag(drc_pop_l) != TAG_NUM && term_tag(drc_pop_r) != TAG_NUM &&
      !kbo_eq(drc_pop_l, drc_pop_r) &&
      atp_drc_size_ok_term(drc_pop_l, drc_pop_r) &&
      (atp_term_has_fold(s, drc_pop_l) || atp_term_has_fold(s, drc_pop_r)) &&
      atp_drc_needle_gate(s, drc_pop_l, drc_pop_r)) {
    if (s->record_norm_steps) s->n_trace = trace_mark;
    l = drc_pop_l;
    r = drc_pop_r;
#ifdef THVM_ATPFT_RULES
    // The (equal) NF spans are not the kept content -- no mirror adoption.
    pop_nf_l = NULL;
    pop_nf_r = NULL;
#endif
    pop_joined = 0u;
    s->n_cps_drc_kept_pop++;
    ATP_POPV("DRCKEEP");
  }
  if (pop_joined) {
    ATP_POPV("JOINED");
    atp_hx_outcome(s, ATP_HX_OUT_JOINED);
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

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Simpler-join at selection, Term-side arm (opt-in --simpler-join):
  // covers the record and legacy pop-normalize paths (the FT-direct
  // branch above decided already -- sj_checked).  A join is the same
  // redundancy verdict as the kbo_eq drop above: the CP feeds no rule,
  // its NORM_STEP chain and scratch Terms are dead.  lazy_commit pops
  // keep sentinel l/r and were decided on the FT branch.  The pop-peak
  // stash arms the legacy arm too (select_cp only decodes it when the
  // popped l/r derive from the slot's bytes -- record replay keeps it
  // NULL, so the var spaces always agree).
  if (s->use_simpler_join && !sj_checked && !lazy_commit) {
    if (g_pop_sj_peak != NULL) atp_sj_peak_arm_ft(g_pop_sj_peak);
    u8 pop_sj_joined = atp_cp_sj_joinable_term(s, l, r);
    atp_sj_peak_disarm();
    {
      static int ppd3 = -1;
      if (ppd3 < 0) ppd3 = atp_env_on("THVM_ATP_POP_PEAK_DEBUG");
      if (ppd3) {
        fprintf(stderr, "[popsj] arm=term peak=%s joined=%u\n",
                (g_pop_sj_peak != NULL) ? "yes" : "no", pop_sj_joined);
      }
    }
    atp_pop_sj_peak_drop(s);
    if (pop_sj_joined) {
      s->n_cps_dropped_simpler_join++;
      ATP_POPV("SJJOINED");
      atp_hx_outcome(s, ATP_HX_OUT_SJOINED);
      if (s->record_norm_steps) {
        s->n_trace = trace_mark;
      }
      thvm_atp_heap_reset(hcp_norm);
      s->step++;
      return ATP_RUNNING;
    }
  }
#endif

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
#ifdef THVM_ATPFT_RULES
      !lazy_commit &&   // FT gates already decided; l/r are sentinels
#endif
      atp_cp_perm_subsumed(l, r)) {
    ATP_POPV("PERMSUB");
    atp_hx_outcome(s, ATP_HX_OUT_PERMSUB);
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
  if (s->use_pop_subsume &&
#ifdef THVM_ATPFT_RULES
      !lazy_commit &&   // FT gates already decided; l/r are sentinels
#endif
      atp_compare(s, l, r) == KBO_UN &&
      atp_pop_eq_subsumed(s, l, r)) {
    ATP_POPV("SUBSUMED");
    atp_hx_outcome(s, ATP_HX_OUT_SUBSUMED);
    s->n_cps_dropped_pop_subsumed++;
    if (s->record_norm_steps) {
      s->n_trace = trace_mark;
    }
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }

  // Twee Join.hs pop-time ground-joinability CASE-SPLIT (opt-in
  // --pop-gj / THVM_ATP_POP_GJ): after the pop R+E normalize and the
  // cheaper joined / simpler-join / subsume drops, a popped CP whose
  // sides are PROVABLY ground-joinable under the branch-model case-split
  // (atp_pop_gj_joinable -> the faithful groundJoin/groundJoinFrom loop)
  // is redundant (Bachmair-Dershowitz / Martin-Nipkow / Twee) and
  // dropped before it becomes a rule.  Runs on the R+E normal forms --
  // exactly atp_pop_gj_level's depth-0 input contract.  Under
  // lazy_commit the FT-direct pop left l/r as TAG_NUM sentinels and
  // stashed the survivor NFs as FT spans in g_lazy_rule; decode them to
  // Terms (the ATP_POPV readout idiom) so the case-split sees the same
  // normal forms the non-lazy branch would.  The decoded Terms are
  // pop-heap scratch reclaimed by the drop's heap reset (or the next
  // one on a keep); the FT spans themselves are reclaimed wholesale
  // like every joined/subsume pop drop, which never per-frees lf/rf.
  // Same dead-CP cleanup as the joined / subsume branches.  Default OFF
  // (byte-identical engine); a wrong KEEP costs speed only, a DROP is
  // certified by the case-split.
  if (s->use_pop_gj) {
    Term gj_l = l, gj_r = r;
#ifdef THVM_ATPFT_RULES
    if (lazy_commit) {
      gj_l = ft_to_term(g_lazy_rule.span_l);
      gj_r = ft_to_term(g_lazy_rule.span_r);
    }
#endif
    if (atp_pop_gj_joinable(s, gj_l, gj_r)) {
      ATP_POPV("POPGJ");
      atp_hx_outcome(s, ATP_HX_OUT_POPGJ);
      s->n_cps_dropped_pop_gj++;
      if (s->record_norm_steps) {
        s->n_trace = trace_mark;
      }
      thvm_atp_heap_reset(hcp_norm);
      s->step++;
      return ATP_RUNNING;
    }
  }

#ifdef THVM_ATPFT_RULES
  g_rule_ft_l = pop_nf_l;   // NULL unless the FT-direct pop produced NFs
  g_rule_ft_r = pop_nf_r;
  if (lazy_commit) g_lazy_rule.armed = 1u;
#endif
  g_ft_region = 5u;
  s->rj_hold = rj_pop + 1u;
  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  g_ft_region = 0u;
#ifdef THVM_ATPFT_RULES
  g_lazy_rule.armed = 0u;
  g_rule_ft_l = NULL;
  g_rule_ft_r = NULL;
#endif
  if (added.count == 0) {
    s->rj_hold = 0u;
    ATP_POPV("REFUSED");
    atp_hx_outcome(s, ATP_HX_OUT_REFUSED);
    s->step++;
    return ATP_RUNNING;
  }
  ATP_POPV("KEPT");
  atp_hx_outcome(s, ATP_HX_OUT_KEPT);
  // This CP became a rule/equation -> restore its (leaned) TRACE_CP terms.
  // Store the RAW overlap pair (cp_lhs/cp_rhs, unpacked from cp_packed at
  // selection -- the exact content the non-lean push planted in children
  // 2/3), NOT the oriented/normalized rule, so the proof builder's
  // parent->CP->rule derivation chain reconstructs and verifies.  cp_lhs/
  // cp_rhs were allocated before the hcp_norm checkpoint, so the pop-time
  // heap reset keeps them and the trace entry roots them across GC.
  atp_trace_cp_rematerialize(s, src_trace, cp_lhs, cp_rhs);
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Lean-rules eq pack from the rule's mirrors (bit-identical bytes;
    // under the lazy commit rl/rr are sentinels and the mirror is the
    // only real pair).
    if (s->lhs_ft[rid] != NULL && s->rhs_ft[rid] != NULL) {
      g_trace_eq_ft_l = s->lhs_ft[rid];
      g_trace_eq_ft_r = s->rhs_ft[rid];
    }
#endif
    u32  t  = atp_trace_push(s, reason, chain_tail,
                             ATP_TRACE_NONE, rl, rr);
    s->r_trace[rid] = t;
    atp_rj_content(s, rid);
    atp_hx_rule(s, rid, (reason == TRACE_FVI) ? ATP_HX_RULE_FVI
                                              : ATP_HX_RULE_BORN,
                s->last_popped_seq);
  }
  s->rj_hold = 0u;

#ifdef ATP_CP_GROUND_JOIN
  // WM -gj forward fact test at creation (RUndEVerwaltung.c:182-183 /
  // :457-460): runs BEFORE interreduction + CP generation, matching
  // WM's RE_Erzeugte* -> ArbeitsAufnahme order.  The status rides the
  // slot through the interreduce compaction below.
  if (s->use_bwd_ground_join) atp_gj_classify_added(s, added);
#endif

  // Otter loop: record every CP this step queues (interreduction
  // victims, the new fact's overlaps, IR-victim drains) for the
  // post-batch replay below.
  if (s->use_otter) { s->otter_rec = 1u; s->otter_n = 0u; }

  // Interreduce shifts new-rule indices down by `dropped`.
  u64 _ph_ir_t0 = atp_phase_now(); g_thvm_phase = "interreduce";
  g_ft_region = 2u;
  u32 dropped = thvm_atp_interreduce(s, added);
  g_ft_region = 0u;
  if (g_atp_phase_enabled) g_atp_phase_us_interreduce += atp_now_us() - _ph_ir_t0;
  AtpAddedRange post = added;
  post.first = (dropped > post.first) ? 0 : (post.first - dropped);

  // WM order mirror: register the new fact's tree faces
  // (RE_RegelEinfuegen / GleichungEinfuegen) only NOW -- after the
  // lhs-interreduction retires evicted their victims' faces.  WM's
  // pipeline is IR_InterreduktionLinks THEN RE_FaktumEinfuegen THEN
  // IR_InterreduktionRechts (Hauptkomponenten.c:311-313), so a new fact
  // whose face shares a leaf-path with a victim plain-hangs into the
  // just-vacated slot and its enclosing-subterm jump HEAD-inserts via
  // the NeuesBlattEinhaengen pending pop (RumpfSprungeintragSetzen).
  // Registering at activation (before the interreduce) instead SPLIT
  // the still-live victim leaf, leaving the new leaf's jump spliced
  // after the survivor -- ranked too late (BooleanAxioms
  // OrAssociativity @300: WM rule 45 must plain-hang into retired rule
  // 36's slot so its depth-2 jump heads the exit list ahead of twin 44;
  // WMJUMP ground truth shows exactly one HEAD event, no AFTER splice).
  // The interreduce phases themselves only EVICT victim faces and rank
  // victims among themselves (drain keys) -- relative victim order is
  // insensitive to the new fact's presence, matching WM's
  // pre-FaktumEinfuegen state.  IR_InterreduktionRechts (the RHS
  // compose) performs no mirror operation, so post-interreduce
  // registration is WM-order equivalent.
  //
  // The WM-distinguished face is `selRec.lhs = KPLinks`: a genuine
  // two-parent superposition lands KPLinks on thvm's stored RHS
  // (dist_rhs=1, the default); a CP that re-derives an existing fact
  // through a single parent keeps the natural left side on thvm's
  // stored LHS (dist_rhs=0).  atp_wmo_eq_dist_rhs returns that flag
  // from the selected CP's parents; insert_fact_ex's cp_derived arg
  // carries dist_rhs directly (1 -> use stored RHS as distinguished).
  if (s->use_emission_order) {
    u8 dist_rhs = atp_wmo_eq_dist_rhs(s, src_trace);
    for (u32 k = 0; k < post.count; k++) {
      atp_wmo_insert_fact_ex(s, post.first + k, dist_rhs);
    }
  }

  // Auto-MaxWeight: refresh the bound against the now-current rule set
  // before this step's CPs are generated/pushed (the just-oriented rule
  // may have deepened or, via interreduce, shrunk R).
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_recompute(s);

  u64 _ph_gen_t0 = atp_phase_now(); g_thvm_phase = "cp_gen";
  g_ft_region = 3u;
  thvm_atp_generate_cps(s, post);
  g_ft_region = 0u;
  if (g_atp_phase_enabled) g_atp_phase_us_cp_gen += atp_now_us() - _ph_gen_t0;

  // Re-admit any stashed CP now within the refreshed bound.
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_drain(s, 0u);

  // Periodic full-rule-set CP-queue interreduction (Waldmeister
  // KPV_KPMengeInterreduzieren).  Gated behind cp_set_interreduce and run
  // every cp_set_ir_period-th rule addition so the per-CP full-R sweep's
  // cost is amortized.  Default off -> the call is skipped and the engine
  // is byte-identical.
  //
  // cp_set_ir_budget > 0 swaps the rule-count period for Twee's task
  // discipline (atp_cp_set_ir_due): the every-16-rules full sweep costs
  // 80%+ of the wall on hard UEQ targets (LAT138-1: 24.5s of 30s), where
  // Twee holds the same renormalisation to ~5% of the run and still
  // reaches the proofs -- the budget buys back the completion time.  The
  // sweep's cost accrues on the discipline's clock (atp_work_now_us).
  if (s->cp_set_interreduce && added.count > 0u) {
    if (s->cp_set_ir_budget > 0u) {
      if (atp_cp_set_ir_due(s)) {
        u32 cps0 = s->n_cps;
        u64 t0 = atp_work_now_us();
        u64 _ph_cpir_t0 = atp_phase_now(); g_thvm_phase = "cp_set_ir";
        atp_cp_set_interreduce(s);
        if (g_atp_phase_enabled) g_atp_phase_us_cp_set_ir += atp_now_us() - _ph_cpir_t0;
        u64 t1 = atp_work_now_us();
        s->cp_set_ir_spent_us += t1 - t0;
        s->cp_set_ir_last_us = t1;
        FILE *tf = atp_clock_trace();
        if (tf != NULL) {
          fprintf(tf, "cpir-sweep sel=%u step=%u rules=%u cps0=%u cps1=%u "
                  "cost=%llu", s->cp_select_count, s->step, s->n_rules, cps0,
                  s->n_cps, (unsigned long long)(t1 - t0));
          atp_clock_trace_end(tf);
        }
      }
    } else {
      u32 period = s->cp_set_ir_period ? s->cp_set_ir_period
                                       : ATP_CP_SET_IR_PERIOD;
      if (s->n_rules % period == 0u) {
        u64 _ph_cpir_t0 = atp_phase_now(); g_thvm_phase = "cp_set_ir";
        atp_cp_set_interreduce(s);
        if (g_atp_phase_enabled) g_atp_phase_us_cp_set_ir += atp_now_us() - _ph_cpir_t0;
      }
    }
  }

  // Waldmeister IR_PufferAuslesen (Interreduktion.c:387-392): the
  // victims thvm_atp_interreduce buffered under use_wm_demote re-enter
  // the queue only NOW -- after this fact's CPs were generated and the
  // periodic CP-set sweep ran -- as the step's LAST queue mutation, so
  // each victim's FIFO age is younger than every CP the new fact
  // produced (WM's ArbeitsAufnahme work order, Hauptkomponenten.c:
  // 308-331; McCune-II's `ues 32` carries age 32, after CPs 14-31).
  if (s->n_irv > 0u) {
    g_ft_region = 6u;
    atp_wm_demote_drain(s);
    g_ft_region = 0u;
  }

  // Otter loop: replay this step's queued CPs, adopting the orientable
  // survivors as passive demodulators (src/atp/otter.c).
  if (s->use_otter) atp_otter(s);
  if (s->heap_full) {
    atp_dbg_abort_site("heap-full", s);
    return ATP_ABORTED;
  }

#ifdef ATP_CP_GROUND_JOIN
  // WM RueckwaertsGrundzusammenfuehrbarkeit (ArbeitsAufnahme,
  // Hauptkomponenten.c:329): AFTER this fact's CPs were generated and
  // the IR-victim drain ran, re-test every EXISTING fact against the
  // extended system and sterilize those that became ground-joinable.
  // Skips the just-added range (Faktum != Neues, :266/:286).
  if (s->use_bwd_ground_join) {
    // WM re-tests every step (RueckwaertsGrundzusammenfuehrbarkeit runs
    // each ArbeitsAufnahme).  In the twee rule-delete mode the fact test is
    // UNPROTECTED (see atp_gj_fact_test) and thus far more expensive, and it
    // re-tests every still-FAILED rule against the growing system on every
    // step -- O(n_rules * gj_cover) per step, the dominant cost.  Twee's
    // interreduce is periodic, not every-step, so under the delete flag run
    // the walk every THVM_ATP_GJ_DELETE_PERIOD-th step (default 16).  The
    // WM path (delete off) keeps the every-step cadence for byte-parity.
    u8 do_walk = 1u;
    if (s->use_gj_delete_rule) {
      static int gjd_period = -1;
      if (gjd_period < 0) {
        const char *e = getenv("THVM_ATP_GJ_DELETE_PERIOD");
        gjd_period = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 16;
        if (gjd_period < 1) gjd_period = 1;
      }
      do_walk = (s->step % (u32)gjd_period == 0u) ? 1u : 0u;
    }
    if (do_walk) atp_bwd_ground_join_walk(s, post.first, post.first + post.count);
  }
#endif

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Twee full-R interreduction (opt-in): periodic whole-set re-normalize +
  // collapse of no-longer-left-reduced rules.  O(n_rules^2) per pass, so
  // run every THVM_ATP_FULL_INTERREDUCE_PERIOD-th step (default 16).
  if (s->use_full_interreduce) {
    static int fir_period = -1;
    if (fir_period < 0) {
      const char *e = getenv("THVM_ATP_FULL_INTERREDUCE_PERIOD");
      fir_period = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 16;
      if (fir_period < 1) fir_period = 1;
    }
    if (s->step % (u32)fir_period == 0u) thvm_atp_interreduce_full(s);
  }
#endif

  u64 _ph_gc_t0 = atp_phase_now(); g_thvm_phase = "goal_check";
  g_ft_region = 4u;
  goal = thvm_atp_goal_check(s);
  g_ft_region = 0u;
  if (s->guide != NULL) {
    atp_guide_on_goal(s);
  }
  if (g_atp_phase_enabled) g_atp_phase_us_goal_check += atp_now_us() - _ph_gc_t0;
  if (goal != ATP_RUNNING) return goal;

  // Step-boundary Cheney pass: the FV-index rebuild was the only
  // collector call site, so a long saturation accumulated all its
  // per-step garbage until the arena ran dry (OrAssociativity died in
  // goal_check at step ~278.8k with 185M cells allocated, live set far
  // smaller).  Same threshold discipline as the rebuild-loop call.
  // Gated on trace_pack exactly like that call: with the trace
  // off-heap the live set is small and the pass is cheap; with the
  // on-heap trace every pass copies the whole proof trace (~8x
  // slowdown observed: 5160 -> 642 steps/s) for no reclaim.
  if (atp_transient_gc_due(s)) {
    thvm_atp_gc_collect(s);
  }

  s->step++;
  return ATP_RUNNING;
}

