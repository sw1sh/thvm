// ===================================================================
// Otter-loop passive demodulators (THVM_ATP_OTTER / --otter)
// ===================================================================
//
// Vampire's Otter and LRS saturation loops keep PASSIVE clauses in the
// simplifying container (Saturation/Otter.cpp onPassiveAdded ->
// _simplCont.add): a retained unit equality demodulates from the moment
// it enters passive, long before it is selected, and every retained
// clause backward-simplifies active + passive (Otter::onClauseRetained
// -> backwardSimplify).  Under fd=preordered / bd=preordered only the
// equations the ordering orients join the demodulation index
// (EqHelper::getDemodulationLHSIterator: GREATER / LESS).  Prover9 is
// the same loop: every kept oriented equation becomes a demodulator at
// once (New_demodulators / Back_demodulated).
//
// thvm's WM loop is DISCOUNT-shaped: a queued CP rewrites nothing until
// it is selected.  The RNG Moufang cluster separates the two classes:
// Vampire's winning slot on RNG027-5 solves in 91 activations as lrs and
// as ott and fails as dis (2273 activations in 30s); 19 of the proof's
// 84 demodulation steps use never-activated clauses (the reassociation
// law f712 alone 11 times); E and Twee (both DISCOUNT) time out on the
// whole cluster officially.
//
// Port.  After a step's CPs are queued, replay them newest first
// (Vampire moves the _newClauses stack into the FIFO unprocessed
// container, SaturationAlgorithm::newClausesToUnprocessed, so a batch is
// processed in reverse generation order), re-normalize each against the
// oriented rules -- including the passives adopted earlier in the
// replay, the intra-batch forward demodulation -- and adopt every
// orientable survivor as a PASSIVE rule: it rewrites wherever R rewrites
// (CP treatment, selection normalize, interreduction, goal check) and
// backward-simplifies R at adoption (thvm_atp_interreduce, the
// bd=preordered leg), but forms no overlaps (atp_gen_one skips it).
// Selecting the CP retires its passive rule first, so the CP is
// normalized against R without itself and re-enters R as an ordinary
// active rule; an LRS prune retires it too (Otter::onPassiveRemoved).
// Passive status is keyed by rule uid, which rides every slot
// compaction; the CP -> passive link is a seq -> uid map.
//
// Raw overlap (--otter-raw).  Vampire demodulates a generated clause
// once, from the generating inference's conclusion, in the unprocessed
// loop (SaturationAlgorithm.cpp:1186-1212 at v5.0.1: forwardSimplify
// rewrites one step against the simplification set as it stands, the
// replacement re-enters the FIFO, so clauses retained earlier in the
// batch already demodulate it), and activates the retained clause as
// it stands.  thvm treats a CP at push against R, and the replay then
// normalizes that treated pair again: the batch's own passive rules
// never see the raw overlap.  With use_otter_raw each queued CP keeps
// its raw pair (cp_raw); the replay and the selection normalize start
// from it, so a selected CP re-derives the form its passive rule was
// adopted from.  The queue weight stays the treated pair's.
//
// Scope: the batch overlap former (atp_gen_one).  The WM single-walk
// former (use_emission_order) and the IC former do not consult passive
// status, so the layer stays inert under them.  Deferred raw-descriptor
// slots are never adopted.

static void atp_otter_record(AtpState *s, u32 row, u32 seq) {
  if (s->otter_n == s->otter_cap) {
    u32 cap = s->otter_cap ? s->otter_cap * 2u : 1024u;
    u32 *nr = (u32 *)realloc(s->otter_rows, (size_t)cap * sizeof *nr);
    if (nr == NULL) return;            // OOM: the CP just never turns passive
    s->otter_rows = nr;
    u32 *ns = (u32 *)realloc(s->otter_seqs, (size_t)cap * sizeof *ns);
    if (ns == NULL) return;
    s->otter_seqs = ns;
    s->otter_cap = cap;
  }
  s->otter_rows[s->otter_n] = row;
  s->otter_seqs[s->otter_n] = seq;
  s->otter_n++;
}

static void atp_otter_mark(AtpState *s, u32 uid, u8 on) {
  if (uid >= s->uid_passive_cap) {
    if (!on) return;
    u32 cap = s->uid_passive_cap ? s->uid_passive_cap : 1024u;
    while (cap <= uid) cap *= 2u;
    u8 *np = (u8 *)realloc(s->uid_passive, cap);
    if (np == NULL) return;
    memset(np + s->uid_passive_cap, 0, cap - s->uid_passive_cap);
    s->uid_passive = np;
    s->uid_passive_cap = cap;
  }
  s->uid_passive[uid] = on;
}

// seq -> passive uid, open addressing.  A key holds seq + 1 (0 = empty);
// a retired entry keeps its key with uid 0 until the next rehash.
static void atp_otter_map_put(AtpState *s, u32 seq, u32 uid);

static void atp_otter_map_grow(AtpState *s) {
  u32 ocap = s->otter_map_cap;
  u32 *okey = s->otter_map_key, *ouid = s->otter_map_uid;
  u32 live = 0u;
  for (u32 h = 0; h < ocap; h++) if (okey[h] != 0u && ouid[h] != 0u) live++;
  u32 cap = 1024u;
  while (cap < 4u * (live + 1u)) cap *= 2u;
  u32 *nkey = (u32 *)calloc(cap, sizeof *nkey);
  u32 *nuid = (u32 *)calloc(cap, sizeof *nuid);
  if (nkey == NULL || nuid == NULL) {
    free(nkey);
    free(nuid);
    return;                            // OOM: keep the old table
  }
  s->otter_map_key = nkey;
  s->otter_map_uid = nuid;
  s->otter_map_cap = cap;
  s->otter_map_used = 0u;
  for (u32 h = 0; h < ocap; h++) {
    if (okey[h] != 0u && ouid[h] != 0u) atp_otter_map_put(s, okey[h] - 1u, ouid[h]);
  }
  free(okey);
  free(ouid);
}

static void atp_otter_map_put(AtpState *s, u32 seq, u32 uid) {
  if (2u * (s->otter_map_used + 1u) > s->otter_map_cap) {
    atp_otter_map_grow(s);
    if (2u * (s->otter_map_used + 1u) > s->otter_map_cap) return;
  }
  u32 mask = s->otter_map_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (s->otter_map_key[h] != 0u && s->otter_map_key[h] != seq + 1u) {
    h = (h + 1u) & mask;
  }
  if (s->otter_map_key[h] == 0u) {
    s->otter_map_key[h] = seq + 1u;
    s->otter_map_used++;
  }
  s->otter_map_uid[h] = uid;
}

// Remove and return the passive uid linked to `seq` (0 = none).
static u32 atp_otter_map_take(AtpState *s, u32 seq) {
  if (s->otter_map_cap == 0u) return 0u;
  u32 mask = s->otter_map_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (s->otter_map_key[h] != 0u) {
    if (s->otter_map_key[h] == seq + 1u) {
      u32 uid = s->otter_map_uid[h];
      s->otter_map_uid[h] = 0u;
      return uid;
    }
    h = (h + 1u) & mask;
  }
  return 0u;
}

// Remove rule slot i from R, shifting every slot-keyed array in
// lockstep (the same block as the interreduce drops in wm_drain.c).
static void atp_otter_drop_slot(AtpState *s, u32 i) {
  atp_rj_death(s, i);
  if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[i]);
  if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_uid[i]);
  if (!s->r_orient[i]) s->n_unorient--;
  for (u32 k = i + 1u; k < s->n_rules; k++) {
    s->lhs[k - 1u]                 = s->lhs[k];
    s->rhs[k - 1u]                 = s->rhs[k];
    s->r_trace[k - 1u]             = s->r_trace[k];
    s->r_uid[k - 1u]               = s->r_uid[k];
    atp_uid_idx_set(s, s->r_uid[k], k - 1u);
    s->r_orient[k - 1u]            = s->r_orient[k];
    s->r_gd[k - 1u]                = s->r_gd[k];
    s->r_depth[k - 1u]             = s->r_depth[k];
    s->r_gj_status[k - 1u]         = s->r_gj_status[k];
    s->r_overlap_done[k - 1u]      = s->r_overlap_done[k];
    s->r_dead[k - 1u]              = s->r_dead[k];
    s->r_dead_lhs_save[k - 1u]     = s->r_dead_lhs_save[k];
    s->r_dead_rhs_save[k - 1u]     = s->r_dead_rhs_save[k];
    s->r_dead_subsumer_lhs[k - 1u] = s->r_dead_subsumer_lhs[k];
    s->r_dead_subsumer_rhs[k - 1u] = s->r_dead_subsumer_rhs[k];
    s->r_rederive_cut[k - 1u]      = s->r_rederive_cut[k];
#ifdef THVM_ATPFT_RULES
    s->lhs_ft[k - 1u]                 = s->lhs_ft[k];
    s->rhs_ft[k - 1u]                 = s->rhs_ft[k];
    s->r_dead_lhs_save_ft[k - 1u]     = s->r_dead_lhs_save_ft[k];
    s->r_dead_rhs_save_ft[k - 1u]     = s->r_dead_rhs_save_ft[k];
    s->r_dead_subsumer_lhs_ft[k - 1u] = s->r_dead_subsumer_lhs_ft[k];
    s->r_dead_subsumer_rhs_ft[k - 1u] = s->r_dead_subsumer_rhs_ft[k];
#endif
  }
  s->n_rules--;
  s->r_revision++;
#ifdef ATP_RULE_INDEX
  s->rule_index_dirty = 1u;
  s->wmfpa_dirty = 1u;
#endif
}

// The queued CP `seq` leaves passive (selected or LRS-discarded): retire
// its demodulator.  A passive rule an interreduction already removed has
// a dead uid; a soft-deleted slot keeps its proof-reconstruction saves.
// `why` = ATP_HX_OT_RETIRE_SEL / _LRS, for the history export.
static void atp_otter_popkill(AtpState *s, u32 seq, u8 why) {
  u32 uid = atp_otter_map_take(s, seq);
  if (uid == 0u) return;
  atp_otter_mark(s, uid, 0u);
  u32 i = atp_uid_is_dead(s, uid) ? 0xffffffffu : sw_uid_to_idx(s, uid);
  if (i == 0xffffffffu || s->r_dead[i]) {
    atp_hx_otter(s, why, seq, 0xffffffffu, uid, ATP_TRACE_NONE, 1u);
    return;
  }
  atp_hx_otter(s, why, seq, 0xffffffffu, uid, s->r_trace[i], 0u);
  atp_otter_drop_slot(s, i);
  s->n_otter_retired++;
}

// Payload row of the queued CP `seq`, or 0xffffffff when it left the
// queue.  Rows are dense in [0, n_cps); a recorded row can move under a
// pop backfill or a densify sweep, hence the scan fallback.
static u32 atp_otter_row_of(const AtpState *s, u32 row, u32 seq) {
  if (row < s->n_cps && s->cp_seq[row] == seq) return row;
  for (u32 r = 0; r < s->n_cps; r++) {
    if (s->cp_seq[r] == seq) return r;
  }
  return 0xffffffffu;
}

static void atp_otter_one(AtpState *s, u32 row, u32 seq) {
#if !(defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES))
  (void)s; (void)row; (void)seq;       // the FT normalizer is required
  return;
#else
  row = atp_otter_row_of(s, row, seq);
  if (row == 0xffffffffu) {
    atp_hx_otter(s, ATP_HX_OT_GONE, seq, row, 0u, ATP_TRACE_NONE, 0u);
    return;
  }
  if (atp_cp_slot_implicit(s, row) || s->cp_packed[row] == NULL) {
    atp_hx_otter(s, ATP_HX_OT_DEFERRED, seq, row, 0u, ATP_TRACE_NONE, 0u);
    return;
  }
  // Vampire LRS discards a new clause beyond the limits at retention
  // (it never enters passive); ours waits for the next horizon prune,
  // so do not adopt it meanwhile.
  if (s->use_lrs && s->lrs_horizon != 0u && s->cp_pri[row] > s->lrs_horizon &&
      (!s->use_lrs_age ||
       atp_lrs_age_key(s, s->cp_pos[row], row) > s->lrs_age_horizon)) {
    atp_hx_otter(s, ATP_HX_OT_LRS, seq, row, 0u, ATP_TRACE_NONE, 0u);
    return;
  }
  // FT-native normalize (the push-time treatment's normalizer): its
  // pattern index appends new rules incrementally, where the Term-side
  // rule index rebuilds after every push.
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  u64 hc = thvm_atp_heap_checkpoint();
  u32 trace_mark = s->n_trace;
  const u32 rj0 = s->rj_n;   // the rule set this CP normalizes on
  u32 chain = s->cp_trace[row];
  AtpFtCell *fl, *fr;
  Term nl = 0, nr = 0;
  if (s->record_norm_steps) {
    // Proof mode: normalize the RAW overlap (the TRACE_CP pair) with
    // recorded NORM_STEPs -- the selection-time record discipline -- so
    // the passive rule's ORIENT cites a complete chain from its CP.
    Term raw_l = 0, raw_r = 0;
    if (!s->cp_trace_lean && chain != ATP_TRACE_NONE && chain < s->n_trace) {
      Term te = s->trace[chain];
      if (term_tag(te) == TAG_CTR && term_ext(te) == TRACE_CP &&
          term_ctr_n(te) >= 4u &&
          !atp_trace_eq_load(s, chain, &raw_l, &raw_r)) {
        raw_l = term_ctr_at(te, 2);
        raw_r = term_ctr_at(te, 3);
      }
    }
    if (raw_l == 0 || raw_r == 0) atp_cp_slot_read(s, row, &raw_l, &raw_r);
    fl = atp_rules_only_normalize_ft_record(s, ft_from_term(a, raw_l, 0),
                                            64u, raw_r, 0u, &chain);
    nl = ft_to_term(fl);
    fr = atp_rules_only_normalize_ft_record(s, ft_from_term(a, raw_r, 0),
                                            64u, nl, 1u, &chain);
  } else {
    // --otter-raw: normalize the raw overlap (cp_raw), not the
    // push-treated pair.
    const u8 *pp = (s->use_otter_raw && s->cp_raw != NULL &&
                    s->cp_raw[row] != NULL) ? s->cp_raw[row]
                                            : s->cp_packed[row];
    fl = atp_rules_only_normalize_ft(s, acp_unpack_term_ft(a, &pp, 0), 64u);
    fr = atp_rules_only_normalize_ft(s, acp_unpack_term_ft(a, &pp, 0), 64u);
  }
  u8 joined = (u8)ft_eq(fl, fr);
  if (!joined) {
    if (nl == 0) nl = ft_to_term(fl);
    nr = ft_to_term(fr);
  }
  ft_free_span(a, fl, fl->end);
  ft_free_span(a, fr, fr->end);
  KboCmp c = joined ? KBO_EQ : atp_compare(s, nl, nr);
  if (c != KBO_GT && c != KBO_LT) {
    // Joined or unorientable: no demodulator.  Drop the recorded chain
    // and the scratch Terms (the selection path's joined rewind).
    if (joined) s->n_otter_joined++;
    else        s->n_otter_unorient++;
    s->n_trace = trace_mark;
    thvm_atp_heap_reset(hc);
    atp_hx_otter(s, joined ? ATP_HX_OT_JOINED : ATP_HX_OT_UNORIENT, seq, row,
                 0u, ATP_TRACE_NONE, 0u);
    return;
  }
  Term big   = (c == KBO_GT) ? nl : nr;
  Term small = (c == KBO_GT) ? nr : nl;
  u32 n0 = s->n_rules;
  // atp_push_rule stamps r_depth / r_gd from the last pop; the passive
  // rule inherits its own CP's instead.
  u32 sv_depth = s->last_popped_depth;
  u8  sv_gd    = s->last_popped_gd;
  s->last_popped_depth = (s->cp_depth != NULL) ? s->cp_depth[row] : 0u;
  s->last_popped_gd    = (s->cp_gd != NULL) ? s->cp_gd[row] : 0u;
  u8 ok = atp_push_rule(s, big, small);
  s->last_popped_depth = sv_depth;
  s->last_popped_gd    = sv_gd;
  if (!ok || s->n_rules == n0) {
    s->n_otter_refused++;
    atp_hx_otter(s, ATP_HX_OT_REFUSED, seq, row, 0u, ATP_TRACE_NONE, 0u);
    return;
  }
  u32 i = s->n_rules - 1u;
  if (!s->r_orient[i]) {
    // Orientation is renaming-invariant, so this cannot happen; keep the
    // invariant (passive rules are oriented) regardless.
    atp_hx_otter(s, ATP_HX_OT_DROP, seq, row, s->r_uid[i], ATP_TRACE_NONE, 0u);
    atp_otter_drop_slot(s, i);
    s->n_otter_refused++;
    return;
  }
  atp_otter_mark(s, s->r_uid[i], 1u);
  atp_otter_map_put(s, seq, s->r_uid[i]);
  if (s->cp_trace_lean) {
    Term l = 0, r = 0;
    atp_cp_slot_read(s, row, &l, &r);
    atp_trace_cp_rematerialize(s, s->cp_trace[row], l, r);
  }
  s->rj_hold = rj0 + 1u;
  s->r_trace[i] = atp_trace_push(s, TRACE_ORIENT, chain,
                                 ATP_TRACE_NONE, s->lhs[i], s->rhs[i]);
  s->rj_hold = 0u;
  atp_rj_content(s, i);
  atp_hx_rule(s, i, ATP_HX_RULE_PASSIVE, seq);
  atp_hx_otter(s, ATP_HX_OT_ADOPT, seq, row, s->r_uid[i], s->r_trace[i], 0u);
  s->n_otter_adopted++;
  // bd=preordered: the new demodulator backward-simplifies R.  May
  // collect garbage -- no Term above is read past this point.
  AtpAddedRange ar = { i, 1u, 0u };
  thvm_atp_interreduce(s, ar);
#endif
}

// Heap discipline inside one replay, which can adopt thousands of
// passive rules in a single step: the packed-trace threshold collects
// as everywhere else; with the trace on-heap (--proof) collect on heap
// pressure like the overlap-pair loops (atp_pressure_collect).
static void atp_otter_heap_gate(AtpState *s) {
  if (s->trace_pack) {
    if (atp_transient_gc_due(s)) {
      thvm_atp_gc_collect(s);
    }
    return;
  }
  atp_pressure_collect(s);
}

static int atp_otter_seq_cmp(const void *a, const void *b) {
  u32 x = ((const u32 *)a)[1], y = ((const u32 *)b)[1];
  return (x > y) - (x < y);
}

// Replay the recorded batch newest first; CPs queued during the replay
// (interreduction victims of an adoption) follow, newest first again.
// The first call primes: the whole input queue, oldest-to-newest order.
static void atp_otter(AtpState *s) {
  s->otter_rec = 0u;
  if (s->use_emission_order || s->use_ic_cp_gen) {
    s->otter_n = 0u;
    s->otter_primed = 1u;
    return;
  }
  if (!s->otter_primed) {
    s->otter_primed = 1u;
    s->otter_n = 0u;
    u32 n = s->n_cps;
    u32 *pairs = (u32 *)malloc((size_t)(n ? n : 1u) * 2u * sizeof(u32));
    if (pairs == NULL) return;
    for (u32 r = 0; r < n; r++) {
      pairs[2u * r]      = r;
      pairs[2u * r + 1u] = s->cp_seq[r];
    }
    qsort(pairs, n, 2u * sizeof(u32), atp_otter_seq_cmp);
    for (u32 k = 0; k < n; k++) atp_otter_record(s, pairs[2u * k], pairs[2u * k + 1u]);
    free(pairs);
  }
  u32 lo = 0u;
  while (lo < s->otter_n) {
    u32 hi = s->otter_n;
    s->otter_rec = 1u;
    for (u32 k = hi; k-- > lo; ) {
      atp_otter_heap_gate(s);
      if (s->heap_full) break;
      atp_otter_one(s, s->otter_rows[k], s->otter_seqs[k]);
    }
    if (s->heap_full) {
      s->otter_rec = 0u;
      s->otter_n = 0u;
      return;
    }
    s->otter_rec = 0u;
    lo = hi;
  }
  s->otter_n = 0u;
  if (s->n_irv > 0u) atp_wm_demote_drain(s);
}
