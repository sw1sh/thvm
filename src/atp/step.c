// ===========================================================================
// Single-walk native CP formation -- the FAITHFUL Waldmeister port.
// Emits critical pairs in WM's U1_KPsBildenZuFaktum order, so cp_seq == WM's
// cpnr BY CONSTRUCTION: no rank reconstruction, no per-band re-key
// corrections, no selection-time detectors.  The ONLY emission-order former
// (the partner-major batch former + its correction tower were deleted after
// the walk was proven full-CPSEL-stream byte-identical to them on OA under
// the r_overlap_done fix).
// WM order (Unifikation1.c:1486/1561):
//   - Vater/toplevel sweep FIRST, POSITION-MAJOR over the new fact's face:
//     each non-var position p -> old partners (rules tree then equation tree)
//     in tops-DFS arrival order -> form a CP at p (the old fact rewrites f@p).
//   - Mutter/proper-subterm sweep, partner-major: each old fact in leaf-list
//     order -> f's whole LHS planted into the old fact's PROPER subterms.
//   - the new fact is already in the wmo trie, so self-overlaps appear as
//     partners (WM R'=R u {l} trick); the rule root-self overlap is excluded.
// cpnr is stamped only on SURVIVORS after the joinable/ordering drop, so the
// gate runs BEFORE the per-CP push/seq.
// The wmo mirror registers facts under their r_uid (stable identity; r_trace
// is NONE past the proof-trace cap -- see atp_wmo_insert_fact_ex), so the
// walk's partner hits carry UIDs, not trace ids.  Map back via r_uid.
static u32 sw_uid_to_idx(AtpState *s, u32 uid) {
  u64 _dco_t0 = g_atp_phase_detail ? atp_now_us() : 0;
  u32 r = 0xffffffffu;
  if (uid != 0u && s->r_uid != NULL) {
    // O(1) probe via the maintained uid_idx reverse map, verified against
    // r_uid so the answer is exactly the scan's.  THVM_ATP_NO_UIDIDX=1
    // forces the reference scan (same-binary kill switch).
    static int no_map = -1;
    if (no_map < 0) {
      const char *e = getenv("THVM_ATP_NO_UIDIDX");
      no_map = (e != NULL && e[0] == '1') ? 1 : 0;
    }
    if (!no_map && uid < s->uid_idx_cap) {
      u32 h = s->uid_idx[uid];
      if (h < s->n_rules && s->r_uid[h] == uid) r = h;
    }
    if (r == 0xffffffffu) {
      for (u32 i = 0; i < s->n_rules; i++)
        if (s->r_uid[i] == uid) { r = i; break; }
      if (!no_map && r != 0xffffffffu) atp_uid_idx_set(s, uid, r);
    }
  }
  if (g_atp_phase_detail) g_atp_cpg_us_collect += atp_now_us() - _dco_t0;
  return r;
}

// Walk phase tag for the CPFORM trace's combo field (A=10 B=11 F=12 C=13
// D=14 E=15 G=16); trace-only, no engine effect.
static u8 g_sw_phase = 0xffu;

// Walk-side rename cache -- the batch former's machinery ported to the
// single-walk CP formation.  thvm_rename_vars(t, CP_RENAME_OFFSET) is a
// pure content function of the source handle, and cells neither move nor
// get reused between collections (g_atp_gc_epoch bumps on every
// thvm_atp_gc_collect), so a (source pair, epoch)-keyed entry returns
// exactly what the rename would rebuild -- the same witness as
// atp_overlap_ij's one-entry cache.  The walk needs a WIDER cache: the
// batch path renames a partner once per PAIR, but the Vater sweep
// re-renamed the same old partner at EVERY non-var position of the new
// fact (two thvm_rename_vars per hit) and the Mutter/self sweeps
// re-renamed the same new-fact face once per partner hit.  Direct-mapped;
// a collision just recomputes.  Cached out-terms are not GC roots: any
// collection bumps the epoch, so a stale handle is never returned.
// Rename provenance stash: sw_rename_pair records its last call's
// (sources, outputs) so the walk former can recover the SPAN twins of
// the renamed pair -- the consumer proves the pairing by comparing the
// output handles before trusting the sources (every sweep renames
// immediately before forming, and the Mutter/Self walks reuse one
// rename across many positions, so "last rename" always corresponds
// to the (lin, rin) in flight; the handle proof makes that exact
// rather than assumed).
static Term g_sw_rn_src_l = 0, g_sw_rn_src_r = 0;
static Term g_sw_rn_out_l = 0, g_sw_rn_out_r = 0;
typedef struct { Term src_l, src_r, out_l, out_r; u64 epoch; } SwRnEnt;
enum { SW_RN_SLOTS = 8192 };
static SwRnEnt g_sw_rn[SW_RN_SLOTS];
static void sw_rename_pair(Term src_l, Term src_r, Term *lin, Term *rin) {
  u64 h = ((u64)src_l ^ ((u64)src_r << 1)) * 0x9e3779b97f4a7c15ull;
  u32 slot = (u32)(h >> 49) & (u32)(SW_RN_SLOTS - 1);
  SwRnEnt *e = &g_sw_rn[slot];
  if (e->epoch == g_atp_gc_epoch && e->src_l == src_l && e->src_r == src_r) {
    if (g_atp_phase_detail) g_atp_swrn_hit++;
    *lin = e->out_l;
    *rin = e->out_r;
    g_sw_rn_src_l = src_l;
    g_sw_rn_src_r = src_r;
    g_sw_rn_out_l = *lin;
    g_sw_rn_out_r = *rin;
    return;
  }
  if (g_atp_phase_detail) g_atp_swrn_miss++;
  e->src_l = src_l;
  e->src_r = src_r;
  e->out_l = *lin = thvm_rename_vars(src_l, CP_RENAME_OFFSET);
  e->out_r = *rin = thvm_rename_vars(src_r, CP_RENAME_OFFSET);
  e->epoch = g_atp_gc_epoch;
  g_sw_rn_src_l = src_l;
  g_sw_rn_src_r = src_r;
  g_sw_rn_out_l = *lin;
  g_sw_rn_out_r = *rin;
}

// THVM_ATP_FT_EMIT_CHECK for the virtual order gate: rebuild the three
// CP Terms classically and replicate atp_cp_order_gate's compare
// verbatim; abort on any verdict divergence.  Runs while the unifier
// is still armed.
static void vcp_gate_certify(AtpState *s, Term lo, const u32 *p, u32 p_len,
                             Term rin, Term ro, const RewriteSubst *subst,
                             u8 o_eq, u8 in_eq, u8 vdrop) {
  Term cl   = thvm_unify_apply(cp_replace_at(lo, p, p_len, rin), subst);
  Term cr   = thvm_unify_apply(ro, subst);
  Term peak = thvm_unify_apply(lo, subst);
  u8 drop = 0u;
  if (peak != 0) {
    if (o_eq) {
      Term a = cr, b = peak;
      thvm_normalize_vars(&a, &b);
      if (atp_compare(s, a, b) == KBO_GT) drop = 1u;
    }
    if (!drop && in_eq) {
      u8 pos8[CP_MAX_DEPTH];
      for (u32 d = 0; d < p_len && d < (u32)CP_MAX_DEPTH; d++) {
        pos8[d] = (u8)p[d];
      }
      Term in_new = atp_cp_pos_subterm(cl, pos8, (u8)p_len);
      Term in_old = atp_cp_pos_subterm(peak, pos8, (u8)p_len);
      if (in_new != 0 && in_old != 0) {
        thvm_normalize_vars(&in_new, &in_old);
        if (atp_compare(s, in_new, in_old) == KBO_GT) drop = 1u;
      }
    }
  }
  if (drop != vdrop) {
    fprintf(stderr,
            "ATP_FT_EMIT_CHECK GATE MISMATCH classic=%u virtual=%u "
            "o_eq=%u in_eq=%u\n", drop, vdrop, o_eq, in_eq);
    abort();
  }
}

#ifdef THVM_ATPFT_NORM
// Cell-by-cell identity of two FT spans (sym + arity + flags -- ft_eq
// plus the flag bits the treatment's fast paths read).
static u8 vcp_ft_cells_identical(const AtpFtCell *x, const AtpFtCell *y) {
  const AtpFtCell *xe = (x->end != NULL) ? x->end->next : NULL;
  const AtpFtCell *px = x;
  const AtpFtCell *py = y;
  while (px != xe) {
    if (py == NULL) return 0u;
    if (px->sym != py->sym || px->arity != py->arity ||
        px->flags != py->flags) {
      return 0u;
    }
    px = px->next;
    py = py->next;
  }
  return 1u;
}

// THVM_ATP_FT_EMIT_CHECK for the treated formation: classic rebuild of
// the raw pair, byte-compare the virtual raw trace bytes, cell-compare
// the virtual FT image against ft_from_term of the classic pair, and
// re-affirm the exact size-gate count.  Runs while the unifier is
// still armed; restores the virtual bytes into the scratch (the trace
// store reads them later).
static void vcp_treat_certify(AtpState *s, Term lo, const u32 *p, u32 p_len,
                              Term rin, Term ro, const RewriteSubst *subst,
                              u32 vtot) {
  u32 vlen = g_vcp_treat.raw_len;
  u8 *vbytes = (u8 *)malloc(vlen ? vlen : 1u);
  if (vbytes == NULL) thvm_fatal("vcp_treat_certify: OOM");
  memcpy(vbytes, g_acp_scratch, vlen);
  u64 c0 = g_unify_apply_cells, t0 = g_unify_apply_taint;
  // WM order: sigma(ro) is the pair's LHS (see the classic arm).
  Term cl = thvm_unify_apply(ro, subst);
  Term cr = thvm_unify_apply(cp_replace_at(lo, p, p_len, rin), subst);
  u32 ctot = (g_unify_apply_taint == t0)
                 ? (u32)(g_unify_apply_cells - c0) : 0u;
  u8 bad = 0u;
  // Exact-count agreement: lazy routing only (the non-lazy stream
  // skips the count walk and passes vtot 0 = no verdict to certify).
  if (vtot != 0u && ctot != 0u && ctot != vtot) bad = 1u;
  u32 rpos = 0u;
  if (!bad && (!acp_trace_pack_fused(cl, cr, &rpos) || rpos != vlen ||
               memcmp(g_acp_scratch, vbytes, vlen) != 0)) {
    bad = 2u;
  }
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *ffl = ft_from_term(a, cl, 0);
  AtpFtCell *ffr = ft_from_term(a, cr, 0);
  if (!bad && (!vcp_ft_cells_identical(g_vcp_treat.fl, ffl) ||
               !vcp_ft_cells_identical(g_vcp_treat.fr, ffr))) {
    bad = 3u;
  }
  ft_free_span(a, ffl, ffl->end);
  ft_free_span(a, ffr, ffr->end);
  if (bad) {
    fprintf(stderr,
            "ATP_FT_EMIT_CHECK TREAT MISMATCH kind=%u len=%u/%u "
            "ctot=%u vtot=%u\n", bad, vlen, rpos, ctot, vtot);
    abort();
  }
  memcpy(g_acp_scratch, vbytes, vlen);
  free(vbytes);
}
#endif // THVM_ATPFT_NORM

// Form ONE CP: outer face (lo,ro) overlapped at position p by the already-
// renamed inner face (lin,rin); push it on its own so cp_seq is the next FIFO
// slot.  atp_cp_gen_gates (joinable/ordering) runs BEFORE the push == WM's
// "drop before ++CPNr".  Returns 1 if a CP survived and was pushed.
// `sub_pre` carries the caller's already-addressed subterm of lo at p
// (the Vater visitor computes it once per position and shares it across
// every partner hit there); 0 = address it here.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT rename cache: (mirror span, CP_RENAME_OFFSET) -> renamed clone.
// Keyed by the span pointer + r_revision: mirror spans are never
// returned to the free list (drops transfer them to the dead-save
// slots; composes leak the old span into the arena), so a pointer key
// cannot alias recycled content within a revision, and any rule-set
// mutation bumps the revision and refreshes the entry.  Renamed
// clones are persistent arena cells -- never freed, so a cached span
// is always safe to hand out.
typedef struct {
  const AtpFtCell *src;
  AtpFtCell *out;
  u64 rev;
} SwRnFtEnt;
enum { SW_RN_FT_SLOTS = 8192 };
static SwRnFtEnt g_sw_rn_ft[SW_RN_FT_SLOTS];
// Renamed-clone epoch reclamation: every clone handed out in the
// CURRENT r_revision epoch is parked here, and the first rename of the
// NEXT revision returns the whole prior epoch to Arena A's free list.
// Sound because a clone is referenced only across formations of one
// revision epoch: a cache hit requires rev equality, no formation is
// in flight across a rule-set mutation, and the r_revision bump is
// exactly the moment every cached entry goes permanently stale.
// Abandoning the clones instead ("never freed") retained ~10 Arena-A
// cells per QUEUED CP -- with the treated-span abandonment fixed, THE
// remaining per-CP Arena share of the 6GB RSS aborts on deep GRP/RNG
// twee-slice runs.  Parked pointers are valid only for the arena
// INSTANCE that allocated them: on an arena change (fresh AtpState)
// the list RESETS without freeing (ft_destroy already dropped the old
// slab pool).  The instance test MUST be AtpFt.session, never the
// AtpFt address: the struct is malloc'd per state and a later session
// regularly lands on the freed address, so an address match would
// carry the dead session's parked spans into the new arena and free
// them there on the first r_revision bump -- the cross-run free_span
// underflow / SIGBUS (WolframAxioms Commutativity -> DoubleNegation,
// AbelianGroup -> Sheffer OrAssociativity in one WL kernel;
// tests/test_atp_session_reuse.c is the in-process regression).
static AtpFtCell **g_sw_rn_park         = NULL;
static u32         g_sw_rn_park_n       = 0;
static u32         g_sw_rn_park_cap     = 0;
static u64         g_sw_rn_park_rev     = ~0ull;
static u64         g_sw_rn_park_session = 0;   // no AtpFt.session is 0
static AtpFtCell *sw_rename_ft(AtpState *s, const AtpFtCell *src) {
  AtpFt *a  = (AtpFt *)s->ft_arena_ptr;
  u64   rev = (u64)s->r_revision;
  if (a->session != g_sw_rn_park_session) {
    g_sw_rn_park_n       = 0;   // stale arena: pointers already released
    g_sw_rn_park_session = a->session;
    g_sw_rn_park_rev     = rev;
    // The rename cache aliases across sessions too: mirror-span
    // ADDRESSES repeat once the old slab pool is freed, and r_revision
    // restarts, so a (src, rev)-matched survivor would hand out a
    // clone span inside the DEAD arena.  New session -> cold cache.
    memset(g_sw_rn_ft, 0, sizeof g_sw_rn_ft);
  } else if (rev != g_sw_rn_park_rev) {
    for (u32 k = 0; k < g_sw_rn_park_n; k++) {
      ft_free_span(a, g_sw_rn_park[k], g_sw_rn_park[k]->end);
    }
    g_sw_rn_park_n   = 0;
    g_sw_rn_park_rev = rev;
  }
  u64 h = ((u64)(uintptr_t)src) * 0x9e3779b97f4a7c15ull;
  u32 slot = (u32)(h >> 51) & (u32)(SW_RN_FT_SLOTS - 1);
  SwRnFtEnt *e = &g_sw_rn_ft[slot];
  if (e->src == src && e->rev == rev) return e->out;
  AtpFtCell *out = ft_rename_span(a, src, CP_RENAME_OFFSET, 0);
  if (g_sw_rn_park_n == g_sw_rn_park_cap) {
    u32 ncap = g_sw_rn_park_cap ? g_sw_rn_park_cap * 2u : 256u;
    AtpFtCell **np = (AtpFtCell **)realloc(g_sw_rn_park,
                                           (size_t)ncap * sizeof *np);
    if (np == NULL) thvm_fatal("sw_rename_ft: park list OOM");
    g_sw_rn_park     = np;
    g_sw_rn_park_cap = ncap;
  }
  g_sw_rn_park[g_sw_rn_park_n++] = out;
  e->src = src;
  e->out = out;
  e->rev = rev;
  return out;
}

// Recover the mirror span of a stored rule FACE by handle identity --
// exact: `t` is one of the two stored Terms of row `i`, so the
// lockstep invariant pins its mirror.  NULL when the handle matches
// neither face (caller falls back to the Term arm).
static const AtpFtCell *sw_face_ft(const AtpState *s, u32 i, Term t) {
  if (i >= s->n_rules) return NULL;
  if (t == s->lhs[i]) return s->lhs_ft[i];
  if (t == s->rhs[i]) return s->rhs_ft[i];
  return NULL;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// THVM_ATP_SW_FT: the walk former's FT arm.  Default ON -- certified
// byte-complete (the full parity suite passes with the arm active,
// coverage total on the WM preset, and it is faster than the Term
// walk).  "0" is the one-round kill switch.
static int sw_ft_on(void) {
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("THVM_ATP_SW_FT");
    on = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return on;
}

// The walk former's FT arm: subterm addressing, head pre-filter,
// unification, size classification, the eq-parent order gate and the
// CP serialization all run on the SPAN twins of the Term faces --
// recovered by handle identity (sw_face_ft / the rename provenance
// stash), so every twin is either exact or the arm DECLINES (returns
// 0, Term arm untouched, nothing armed).  Verdict-for-verdict mirror
// of the Term flow's routed path; returns 1 with *rv set when the
// attempt was fully handled (dropped or pushed).
static u8 sw_form_push_ft_try(AtpState *s, Term lo, Term ro, Term sub_pre,
                              Term lin, Term rin, const u32 *p, u32 p_len,
                              u32 t_out, u32 t_in, u32 i_out, u32 i_in,
                              u8 o_eq, u8 in_eq, u32 *rv) {
  if (!sw_ft_on()) return 0u;
  // The exact ft_considered route conditions -- off-route attempts
  // need the classic Term build (cp.lhs/rhs/peak dereferenced).
  if (!atp_ft_emit_on() || !s->trace_pack || atp_kapur_on() ||
      (s->use_no_overlap_below_skolem && s->n_skolem_labels != 0u) ||
      !atp_cp_push_fuse_ok(s)) {
    return 0u;
  }
  if (atp_cptr_pair(s, i_out, i_in)) return 0u;  // per-pair debug probe
  // Twin recovery, handle-proven.
  const AtpFtCell *lo_ft = sw_face_ft(s, i_out, lo);
  const AtpFtCell *ro_ft = sw_face_ft(s, i_out, ro);
  if (lo_ft == NULL || ro_ft == NULL) return 0u;
  if (lin != g_sw_rn_out_l || rin != g_sw_rn_out_r) return 0u;
  const AtpFtCell *lin_src = sw_face_ft(s, i_in, g_sw_rn_src_l);
  const AtpFtCell *rin_src = sw_face_ft(s, i_in, g_sw_rn_src_r);
  if (lin_src == NULL || rin_src == NULL) return 0u;
  AtpFtCell *lin_ft = sw_rename_ft(s, lin_src);
  AtpFtCell *rin_ft = sw_rename_ft(s, rin_src);
  // Subterm + pre-filter, the Term arm's exact verdicts.  sub_pre, when
  // the Vater sweep passes it, IS the subterm of lo at p (its contract),
  // so the span twin is always addressable from the path.
  (void)sub_pre;
  const AtpFtCell *sub_ft = ft_subterm_at(lo_ft, p, p_len);
  if (sub_ft == NULL || ft_is_var(sub_ft)) {
    *rv = 0u;
    return 1u;
  }
  if (ft_ctr_sym(sub_ft) == ATPFT_NUM_MARKER ||
      (!ft_is_var(lin_ft) && ft_ctr_sym(lin_ft) == ATPFT_NUM_MARKER)) {
    return 0u;   // TAG_NUM face: the Term filter's tag semantics differ
  }
  if (!ft_is_var(lin_ft) && ft_ctr_sym(sub_ft) != ft_ctr_sym(lin_ft)) {
    *rv = 0u;    // head mismatch: thvm_unify would fail identically
    return 1u;
  }
  static AtpFtSubst fsubst;   // BSS zero; watermark-reset per attempt
  ft_subst_reset(&fsubst);
  if (!ft_unify(sub_ft, lin_ft, &fsubst)) {
    *rv = 0u;
    return 1u;
  }
  if (g_atp_phase_detail) g_atp_cpg_overlap_calls++;
  u32 vtot = s->use_lazy_normalize
                 ? vcp_ft_count_pair(lo_ft, sub_ft, rin_ft, ro_ft, &fsubst,
                                     (u32)ATP_WM_BEHANDELN_GATE)
                 : 0u;
  if (vtot == VCP_COUNT_ANOMALY) return 0u;   // classic build decides
  if (o_eq || in_eq) {
    u8 vdrop = 0u;
    if (!vcp_ft_order_gate(s, lo_ft, rin_ft, ro_ft, sub_ft, &fsubst,
                           o_eq, in_eq, &vdrop)) {
      return 0u;                              // anomaly: classic gate decides
    }
    g_vcp_gate_n_test++;
    if (vdrop) {
      g_vcp_gate_n_drop++;
      *rv = 0u;
      return 1u;
    }
  }
  u8 ft_route = 0u, ft_treat = 0u;
  if (s->use_lazy_normalize && vtot >= atp_behandeln_gate(s)) {
    // Twee-score mode: the raw >= 50 class's classic Term image carries
    // subst-shared subterms whose Term-identity discounts the score
    // (atp_twee_term_score dedup) -- an FT emit has no cells to replay
    // that against, so route the class to the classic Term build (its
    // push still takes the fused pack + atp_cp_twee_fused_base).
    if (s->cp_weight_mode == ATP_CP_WEIGHT_TWEE) return 0u;
    if (!vcp_ft_emit_fused(s->kbo, lo_ft, sub_ft, rin_ft, ro_ft, &fsubst)) {
      return 0u;
    }
    ft_route = 1u;
    g_vcp_emit_n_ok++;
  } else {
    static int join_cache_env = -1;
    if (join_cache_env < 0) join_cache_env = atp_env_on("THVM_ATP_JOIN_CACHE");
    if (join_cache_env) return 0u;
    i64 _tb = ftnet_now();
    u8 _tf = vcp_ft_treat_form(s, lo_ft, sub_ft, rin_ft, ro_ft, &fsubst);
    g_ftnet_treatform += ftnet_now() - _tb;
    if (!_tf) return 0u;
    ft_treat = 1u;
    g_vcp_treat_n_ok++;
  }
  // --- the routed push tail, verbatim from the Term arm ---
  CriticalPair cp;
  cp.lhs  = 0;
  cp.rhs  = 0;
  cp.peak = 0;
  cp.pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len; d++) cp.pos[d] = (u8)p[d];
  cp.combo = 0xffu;
  if (atp_cp_gen_gates(s, &cp, 0u, 1u, o_eq, in_eq) == 0u) {
    if (ft_route) g_acp_fuse.valid = 0u;
    if (ft_treat) vcp_treat_release(s);
    *rv = 0u;
    return 1u;
  }
  g_cpform_i = i_out; g_cpform_j = i_in;
  g_cpform_lin = lin; g_cpform_rin = rin; g_cpform_sub = sub_pre;
  g_cpform_itr = t_out; g_cpform_jtr = t_in;
  g_cpform_ilhs = (i_out < s->n_rules) ? s->lhs[i_out] : 0;
  g_cpform_jlhs = (i_in  < s->n_rules) ? s->lhs[i_in]  : 0;
  g_cpform_i_or = (i_out < s->n_rules) ? s->r_orient[i_out] : 0xffu;
  g_cpform_j_or = (i_in  < s->n_rules) ? s->r_orient[i_in]  : 0xffu;
  g_cpform_combo = g_sw_phase;
  g_cpform_pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len && d < CP_MAX_DEPTH; d++) {
    g_cpform_pos[d] = (u8)p[d];
  }
  g_cp_rawsz_hint = vtot;
  if (ft_route) g_vcp_emit.armed = 1u;
  if (ft_route && (s->use_raw_desc | s->use_raw_desc_cert) != 0u &&
      atp_lazy_scorenf_cap() == 0u) {
    // Raw-desc lane: capture the re-derivation coordinates while the
    // operands are in hand.  The refs alias the rule faces' immortal
    // mirror spans (lin_src/rin_src are the PRE-rename sources; the
    // materializer re-renames by the same fixed offset).  sub_ft is an
    // interior cell of lo_ft's preorder ->next chain, so its index is
    // an exact re-address.  (scorenf runs score-time byte probes the
    // deferred slot cannot serve identically -- that env combination
    // keeps the eager arm.)
    u32 si = 0u;
    u8  si_ok = 1u;
    for (const AtpFtCell *pc = lo_ft; pc != sub_ft; pc = pc->next) {
      if (pc == lo_ft->end || pc->next == NULL) { si_ok = 0u; break; }
      si++;
    }
    if (si_ok) {
      g_rdesc_push.out_face = atp_rdesc_intern(s, lo_ft, ro_ft);
      g_rdesc_push.in_face  = atp_rdesc_intern(s, lin_src, rin_src);
      g_rdesc_push.sub_pre  = si;
      g_rdesc_push.len      = 0u;
      g_rdesc_push.armed    = 1u;
    }
  }
  if (ft_treat) g_vcp_treat.armed = 1u;
  *rv = atp_push_cps_traced(s, &cp, 1u, t_out, t_in, i_out, i_in);
  if (ft_route) {
    g_vcp_emit.armed = 0u;
    g_acp_fuse.valid = 0u;
    g_rdesc_push.armed = 0u;
  }
  if (ft_treat) {
    // Return the treated spans to Arena A's free list.  On a CPQ build
    // the queue-insert ADOPTED them (fl/fr are NULL -> pure disarm);
    // everywhere else -- queued via the packed-byte pipeline, joined,
    // or dropped by a push gate -- the stash still owns them, and
    // abandoning them retained ~31 Arena-A cells per FORMED CP (the
    // 4.2GB / 68%-of-RSS flood that aborted deep GRP/RNG runs at ~57s
    // under the 6GB guard).  Content-untouched: only the allocator's
    // free list changes, so the trajectory stays byte-identical.
    i64 _rb = ftnet_now();
    vcp_treat_release(s);
    g_ftnet_release += ftnet_now() - _rb;
    g_acp_fuse.valid  = 0u;
  }
  return 1u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

static u32 sw_form_push_impl(AtpState *s, Term lo, Term ro, Term sub_pre,
                             Term lin, Term rin,
                             const u32 *p, u32 p_len, u32 t_out, u32 t_in,
                             u32 i_out, u32 i_in, u8 o_eq, u8 in_eq);

// Mem-census bracket around one formation+treat+push (see g_ftnet_form).
static u32 sw_form_push(AtpState *s, Term lo, Term ro, Term sub_pre,
                        Term lin, Term rin,
                        const u32 *p, u32 p_len, u32 t_out, u32 t_in,
                        u32 i_out, u32 i_in, u8 o_eq, u8 in_eq) {
#ifdef THVM_ATPFT_ALLOC
  i64 _fb = ftnet_now();
#endif
  u32 rv = sw_form_push_impl(s, lo, ro, sub_pre, lin, rin, p, p_len,
                             t_out, t_in, i_out, i_in, o_eq, in_eq);
#ifdef THVM_ATPFT_ALLOC
  g_ftnet_form += ftnet_now() - _fb;
#endif
  return rv;
}

static u32 sw_form_push_impl(AtpState *s, Term lo, Term ro, Term sub_pre,
                             Term lin, Term rin,
                             const u32 *p, u32 p_len, u32 t_out, u32 t_in,
                             u32 i_out, u32 i_in, u8 o_eq, u8 in_eq) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  {
    u32 _ft_rv = 0u;
    if (sw_form_push_ft_try(s, lo, ro, sub_pre, lin, rin, p, p_len,
                            t_out, t_in, i_out, i_in, o_eq, in_eq,
                            &_ft_rv)) {
      return _ft_rv;
    }
  }
  // FT-arm decline with lazy sentinel operands: resolve each face's
  // SIDE from the sentinel's value tag (0xFA151 = lhs, 0xFA152 = rhs;
  // stale-proof -- a mid-sweep materialization by an earlier position
  // leaves the ctx holding old handles), materialize the two rules,
  // and rebuild the operands so the classic Term arm below sees real
  // faces.
  if (term_tag(lo) == TAG_NUM || term_tag(ro) == TAG_NUM ||
      term_tag(lin) == TAG_NUM || term_tag(rin) == TAG_NUM) {
    u8 lo_is_l = (term_tag(lo) == TAG_NUM)
                     ? (u8)(term_val(lo) == 0xFA151u)
                     : (u8)(lo == s->lhs[i_out]);
    u8 lin_src_is_l =
        (term_tag(g_sw_rn_src_l) == TAG_NUM)
            ? (u8)(term_val(g_sw_rn_src_l) == 0xFA151u)
            : (u8)(g_sw_rn_src_l == s->lhs[i_in]);
    (void)atp_rule_term(s, i_out, 0u);
    (void)atp_rule_term(s, i_out, 1u);
    (void)atp_rule_term(s, i_in, 0u);
    (void)atp_rule_term(s, i_in, 1u);
    lo = lo_is_l ? s->lhs[i_out] : s->rhs[i_out];
    ro = lo_is_l ? s->rhs[i_out] : s->lhs[i_out];
    sw_rename_pair(lin_src_is_l ? s->lhs[i_in] : s->rhs[i_in],
                   lin_src_is_l ? s->rhs[i_in] : s->lhs[i_in],
                   &lin, &rin);
    sub_pre = 0;   // re-address from the real face below
  }
#endif
  // PROFILE=2 detail: everything up to the push (subterm addressing, unify,
  // apply, the gen gates) is the walk's "overlap" component, matching the
  // batch path's atp_overlap_ij span; the atp_push_cps_traced call is its
  // "pushloop" (which times push-norm/varnorm/filters/trace/pack inside).
  int _dl = g_atp_phase_detail;
  u64 _dov_t0 = _dl ? atp_now_us() : 0;
  // uid-keyed probe: arm for this attempt so the gen gates + push path log.
  u8 _cptr_sw = atp_cptr_pair(s, i_out, i_in);
  if (_cptr_sw) {
    g_cptr_active = 1u;
    fprintf(stderr,
            "CPTR SW ATTEMPT phase=%u i_out=%u i_in=%u uid_out=%u uid_in=%u "
            "o_eq=%u in_eq=%u pick=%u pos=L",
            g_sw_phase, i_out, i_in, s->r_uid[i_out], s->r_uid[i_in],
            o_eq, in_eq, s->cp_select_count);
    for (u32 d = 0; d < p_len; d++) fprintf(stderr, ".%u", p[d] + 1u);
    fprintf(stderr, " lo=");
    atp_dbg_print_term(stderr, lo);
    fprintf(stderr, " ro=");
    atp_dbg_print_term(stderr, ro);
    fprintf(stderr, " lin=");
    atp_dbg_print_term(stderr, lin);
    fprintf(stderr, " rin=");
    atp_dbg_print_term(stderr, rin);
    fputc('\n', stderr);
  }
  Term sub = (sub_pre != 0) ? sub_pre : cp_subterm_at(lo, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) {
    if (_dl) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
    if (_cptr_sw) {
      fprintf(stderr, "CPTR SW SKIP var-or-null-subterm\n");
      g_cptr_active = 0u;
    }
    return 0u;
  }
  // Head-symbol pre-filter: two constructors with different head symbols can
  // never unify (thvm_unify returns 0 at its TAG_CTR `term_ext(s)!=term_ext(t)`
  // check).  Skipping here is behaviour-identical -- only true-fail pairs are
  // rejected -- and avoids the 512-byte RewriteSubst zero-init, two unify_walks
  // and the kbo_eq descent for the majority of subterms that can't match.
  if (term_tag(sub) == TAG_CTR && term_tag(lin) == TAG_CTR &&
      term_ext(sub) != term_ext(lin)) {
    if (_dl) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
    if (_cptr_sw) {
      fprintf(stderr, "CPTR SW SKIP head-mismatch\n");
      g_cptr_active = 0u;
    }
    return 0u;
  }
  // Persistent all-zero subst + bound-id undo log (unify/_.c): byte-
  // identical to a fresh 512-byte zero-init per attempt, without the
  // memset.  The undo runs after the last read of `subst` on every
  // path out of the armed window.
  static RewriteSubst subst;  // BSS: all-zero at first use
  thvm_unify_undo_begin();
  if (!thvm_unify(sub, lin, &subst)) {
    thvm_unify_undo(&subst);
    if (_dl) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
    if (_cptr_sw) {
      fprintf(stderr, "CPTR SW SKIP unify-fail\n");
      g_cptr_active = 0u;
    }
    return 0u;
  }
  if (_dl) g_atp_cpg_overlap_calls++;
  CriticalPair cp;
  // FT-direct emission routing (always on where admissible): under
  // the fused pipeline + packed trace the formed CP's (lhs, rhs)
  // Terms are never dereferenced past (1) the eq-parent order gate --
  // computed VIRTUALLY here (vcp_order_gate_virtual) -- and (2) the
  // byte serializers / the FT treatment boundary (see the vcp_*
  // block).  So serialize / build FT straight from (lo, p, rin, ro,
  // subst) and skip the Term build: under the lazy KPBehandelt gate
  // the raw >= 50 class emits its packed pair directly (phase 1) and
  // the treated < 50 class builds its FT cells directly (phase 2b);
  // without lazy EVERY CP is treated, so the whole stream takes the
  // treated arm (the WL "Waldmeister" preset's configuration).  Route
  // conditions: no armed nusfu/kapur (peak readers the virtual gate
  // does not cover), non-MNF, packed trace, the fused pipeline
  // admissible, no per-pair debug probe; the treated arm additionally
  // requires the FT stages and no Term-keyed join cache.  Anomalies
  // fall back to the classic build below, unchanged.
  u8  ft_route      = 0u;   // lazy raw >= 50: serialized straight to bytes
  u8  ft_treat      = 0u;   // treated: FT cells straight from parents
  u8  ft_considered = 0u;
  u32 vtot          = 0u;
  if (atp_ft_emit_on() && !_cptr_sw &&
      s->trace_pack &&   // MNF coupling removed (probe)
      !atp_kapur_on() &&
      !(s->use_no_overlap_below_skolem && s->n_skolem_labels != 0u) &&
      atp_cp_push_fuse_ok(s)) {
    ft_considered = 1u;
    // The capped size count routes raw-vs-treated ONLY under the lazy
    // KPBehandelt gate; the non-lazy stream is all-treated and skips
    // the walk (vtot 0 = no hint, exactly the classic non-lazy hint
    // consumption: none).
    vtot = s->use_lazy_normalize
               ? vcp_count_pair(lo, p, p_len, rin, ro, &subst,
                                (u32)ATP_WM_BEHANDELN_GATE)
               : 0u;
    // PROFILE=2 probe for the phase-2c lower-bound idea: how many raw
    // (>= gate) CPs would an UNSUBSTITUTED size bound
    // (|rin| + |ro| + p_len >= gate) already classify, skipping the
    // capped composite count walk?  Measurement only -- no routing
    // reads it.
    if (g_atp_phase_detail && vtot != VCP_COUNT_ANOMALY) {
      extern u64 g_vcp_lb_hit, g_vcp_lb_raw;
      if (vtot >= (u32)ATP_WM_BEHANDELN_GATE) {
        g_vcp_lb_raw++;
        u32 lb = p_len;
        atp_symbol_count_upto(rin, (u32)ATP_WM_BEHANDELN_GATE, &lb);
        if (lb < (u32)ATP_WM_BEHANDELN_GATE) {
          atp_symbol_count_upto(ro, (u32)ATP_WM_BEHANDELN_GATE, &lb);
        }
        if (lb >= (u32)ATP_WM_BEHANDELN_GATE) g_vcp_lb_hit++;
      }
    }
    u8 gate_ok = (vtot != VCP_COUNT_ANOMALY);
    if (gate_ok && (o_eq || in_eq)) {
      // Eq-parent KPAction ordering gate, virtual (phase 2a): drop at
      // formation exactly where the classic gate would, without
      // building cp.lhs / cp.rhs / cp.peak.
      u8 vdrop = 0u;
      if (vcp_order_gate_virtual(s, lo, rin, ro, sub, &subst,
                                 o_eq, in_eq, &vdrop)) {
        g_vcp_gate_n_test++;
        if (atp_ft_emit_check_on()) {
          vcp_gate_certify(s, lo, p, p_len, rin, ro, &subst,
                           o_eq, in_eq, vdrop);
        }
        if (vdrop) {
          // The classic order gate's drop: before numbering, weighing
          // or queueing -- return with no side effects, like the
          // atp_cp_gen_gates == 0 path below.
          g_vcp_gate_n_drop++;
          thvm_unify_undo(&subst);
          if (_dl) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
          return 0u;
        }
      } else {
        g_vcp_gate_n_fb++;
        gate_ok = 0u;   // anomaly: classic build + classic gate decide
      }
    }
    // Twee-score mode routes the raw >= 50 class to the classic Term
    // build (see the sw_form_push_ft_try emit decline): neither the
    // emit nor the treat arm may take it.
    u8 twee_raw = (s->cp_weight_mode == ATP_CP_WEIGHT_TWEE &&
                   s->use_lazy_normalize &&
                   vtot >= (u32)ATP_WM_BEHANDELN_GATE) ? 1u : 0u;
    if (gate_ok && !twee_raw && s->use_lazy_normalize &&
        vtot >= atp_behandeln_gate(s)) {
      if (vcp_emit_fused(s->kbo, lo, p, p_len, rin, ro, &subst)) {
        ft_route = 1u;
        g_vcp_emit_n_ok++;
        if (atp_ft_emit_check_on()) {
          vcp_emit_certify(s->kbo, lo, p, p_len, rin, ro, &subst);
        }
      } else {
        g_vcp_emit_n_fb++;
      }
    } else if (gate_ok && !twee_raw) {
#ifdef THVM_ATPFT_NORM
      static int join_cache_env = -1;
      if (join_cache_env < 0) {
        join_cache_env = atp_env_on("THVM_ATP_JOIN_CACHE");
      }
      if (!join_cache_env) {
        if (vcp_treat_form(s, lo, p, p_len, rin, ro, &subst)) {
          ft_treat = 1u;
          g_vcp_treat_n_ok++;
          if (atp_ft_emit_check_on()) {
            vcp_treat_certify(s, lo, p, p_len, rin, ro, &subst, vtot);
          }
        } else {
          g_vcp_treat_n_fb++;
        }
      }
#endif
    } else if (vtot == VCP_COUNT_ANOMALY) {
      g_vcp_emit_n_fb++;
    }
  }
  u32 hint_val = 0u;
  if (ft_route || ft_treat) {
    // The serialized bytes / counts / FT cells stand in for the Terms;
    // nothing on this route reads the CP slots (the eq-parent order
    // gate already ran virtually above, so cp.peak is never needed).
    cp.lhs  = 0;
    cp.rhs  = 0;
    cp.peak = 0;
    hint_val = vtot;   // exact under the gate; capped == raw verdict over it
    thvm_unify_undo(&subst);
  } else {
    // Bracket the two side-builds with the unify-apply node tally: the
    // delta is the EXACT combined node count of (cp.lhs, cp.rhs) -- the
    // same count the push-side KPBehandelt size gate re-walks the pair
    // for (atp_symbol_count_upto x2).  Handed to the gate as
    // g_cp_rawsz_hint below; a taint move (defensive apply early-return
    // under-counted a subtree) discards the delta, so a bad count can
    // never reach the gate -- it degrades to the old counting walk.
    u64 _szc0 = g_unify_apply_cells;
    u64 _szt0 = g_unify_apply_taint;
    // WM CP side order (Unifikation1.c:916-917): KPLinks = sigma(host.RHS)
    // -- the UNREWRITTEN side -- and KPRechts = sigma(host.LHS[p <- rin]),
    // the reduct.  thvm emitted (reduct, other) historically; for ORIENTED
    // keeps the orientation hides it, but an UNORIENTABLE keep stores the
    // emission order verbatim, and 42/56 OA + 539/823 Hillman kept E's
    // were side-swapped vs WM (side-sensitive audit 2026-07-07).
    cp.lhs  = thvm_unify_apply(ro, &subst);
    cp.rhs  = thvm_unify_apply(cp_replace_at(lo, p, p_len, rin), &subst);
    u64 _szc1 = g_unify_apply_cells;   // before the (optional) peak build
    // cp.peak is read only by the connectedness gate, the equation-parent
    // KPAction ordering gate, and the armed nusfu gate -- the batch
    // former's g_cp_need_peak discipline (cp/_.c:137-146, set per pair at
    // atp_overlap_ij).  For the dominant rule x rule overlap it is never
    // read; skipping the third unify_apply build is the same pure
    // allocation cut.
    cp.peak = ((s->use_connectedness || s->use_simpler_join != 0u ||
                o_eq || in_eq) || atp_kapur_on() ||
               (s->use_no_overlap_below_skolem && s->n_skolem_labels != 0u))
                  ? thvm_unify_apply(lo, &subst) : 0;
    thvm_unify_undo(&subst);
    hint_val = (g_unify_apply_taint == _szt0 &&
                _szc1 - _szc0 < 0xFFFFFFFFull)
                   ? (u32)(_szc1 - _szc0) : 0u;
    // FT_EMIT_CHECK also certifies the router on the classic side:
    // the capped virtual count must agree with the untainted apply
    // tally (equal under the gate, both capped at it otherwise).
    // vtot 0 = the non-lazy stream, which never counts.
    if (ft_considered && atp_ft_emit_check_on() && vtot != 0u &&
        vtot != VCP_COUNT_ANOMALY && hint_val != 0u &&
        vtot != ((hint_val < (u32)ATP_WM_BEHANDELN_GATE)
                     ? hint_val : (u32)ATP_WM_BEHANDELN_GATE)) {
      fprintf(stderr,
              "ATP_FT_EMIT_CHECK ROUTE MISMATCH vcount=%u hint=%u\n",
              vtot, hint_val);
      abort();
    }
  }
  cp.pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len; d++) cp.pos[d] = (u8)p[d];
  cp.combo = 0xffu;
  if (atp_cp_gen_gates(s, &cp, 0u, 1u, o_eq, in_eq) == 0u) {
    // Unreachable under ft_route / ft_treat (the order gate ran
    // virtually and every other dropping gate is routed off), but
    // never leave the armed weigh state / stashed cells for a later
    // push.
    if (ft_route) g_acp_fuse.valid = 0u;
#ifdef THVM_ATPFT_NORM
    if (ft_treat) vcp_treat_release(s);
#endif
    if (_dl) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
    if (_cptr_sw) {
      fprintf(stderr, "CPTR SW DROP gen-gates\n");
      g_cptr_active = 0u;
    }
    return 0u;
  }
  // Stamp the CP_FORM_TRACE geometry: the per-CP push bypasses the batch loop
  // that normally sets g_cpform_*, so the walk's CPFORM lines would show i=-1.
  g_cpform_i = i_out; g_cpform_j = i_in;
  g_cpform_lin = lin; g_cpform_rin = rin; g_cpform_sub = sub;
  g_cpform_itr = t_out; g_cpform_jtr = t_in;
  g_cpform_ilhs = (i_out < s->n_rules) ? s->lhs[i_out] : 0;
  g_cpform_jlhs = (i_in  < s->n_rules) ? s->lhs[i_in]  : 0;
  g_cpform_i_or = (i_out < s->n_rules) ? s->r_orient[i_out] : 0xffu;
  g_cpform_j_or = (i_in  < s->n_rules) ? s->r_orient[i_in]  : 0xffu;
  g_cpform_combo = g_sw_phase;
  g_cpform_pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len && d < CP_MAX_DEPTH; d++) g_cpform_pos[d] = (u8)p[d];
  u64 _dpl_t0 = 0;
  if (_dl) {
    u64 _now = atp_now_us();
    g_atp_cpg_us_overlap += _now - _dov_t0;
    _dpl_t0 = _now;
  }
  // Size hint for the KPBehandelt gate (consumed-and-cleared at
  // atp_push_cps_traced entry; only this singleton call site sets it,
  // immediately before the call, so it can never leak onto another
  // CP).  The order/einsstern/nusfu/kapur gates above only drop or
  // SWAP sides -- the combined count is invariant.  0 = no hint.
  // FT route: hint = the capped virtual count (>= the gate, so the
  // verdict is the raw-untreated class either way).
  g_cp_rawsz_hint = hint_val;
  // Arm the FT-direct stash for exactly this push (the two byte
  // consumers -- trace store + heap pack -- read it); cleared on
  // EVERY exit of the push, including the pre-pack drop paths, so a
  // stale weigh state can never leak onto a later CP.  The treated
  // stash arms its own flag: the push loop treats the stashed cells
  // in place, the trace store takes the raw bytes, the heap push
  // packs the NF cells; dropped-or-consumed either way, the cells are
  // abandoned exactly like the classic path's NF cells.
  if (ft_route) g_vcp_emit.armed = 1u;
  if (ft_treat) g_vcp_treat.armed = 1u;
  u32 rv = atp_push_cps_traced(s, &cp, 1u, t_out, t_in, i_out, i_in);
  if (ft_route) {
    g_vcp_emit.armed = 0u;
    g_acp_fuse.valid = 0u;
  }
  if (ft_treat) {
    // Free un-adopted treated spans (see the sw_form_push_ft_try tail:
    // the Arena-A per-formed-CP retention flood); no-op disarm when a
    // CPQ insert took ownership.
#ifdef THVM_ATPFT_NORM
    vcp_treat_release(s);
#endif
    g_vcp_treat.armed = 0u;
    g_acp_fuse.valid  = 0u;
  }
  if (_dl) g_atp_cpg_us_pushloop += atp_now_us() - _dpl_t0;
  if (_cptr_sw) g_cptr_active = 0u;
  return rv;
}

// Overlap-exhausted-equation pair skip (mirror of atp_wmo_collect_pair's
// use_overlap_exhaust gate): a flat-transposition NEW fact does not
// re-superpose against an OLD unorientable equation whose birth batch is
// already enumerated (r_overlap_done) -- WM overlaps commutativity with the
// equation's fresh re-derivation, not the stale original.  `f_is_ft` is the
// new fact's atp_is_flat_transposition, computed once per batch.
static u8 sw_pair_exhaust_skip(AtpState *s, u32 f, u32 j, u8 f_is_ft) {
  if (!s->use_overlap_exhaust || j == f) return 0u;
  u8 j_exh = s->r_overlap_done[j] && !s->r_orient[j];
  return (j_exh && !s->r_overlap_done[f] && f_is_ft) ? 1u : 0u;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span sibling of cp_walk_positions: identical preorder position
// enumeration (visit the root CTR, then children left-to-right;
// variables skipped; the TAG_NUM marker maps to the Term walk's
// non-CTR skip), including the depth-cap diagnostic.
static u32 ft_walk_positions(const AtpFtCell *c, u32 *path, u32 depth,
                             u32 max_depth, CpVisitor visit, void *ctx,
                             u32 count) {
  if ((c->sym & WF_VAR_BIT) != 0u) return count;
  if (c->sym == ATPFT_NUM_MARKER) return count;
  count = visit(path, depth, ctx);
  if (depth >= max_depth) {
    if (c->arity > 0u) g_cp_positions_depth_capped++;
    return count;
  }
  u32 n = (u32)c->arity;
  const AtpFtCell *kid = c->next;
  for (u32 i = 0; i < n; i++) {
    path[depth] = i;
    count = ft_walk_positions(kid, path, depth + 1u, max_depth, visit,
                              ctx, count);
    kid = kid->end->next;
  }
  return count;
}

// Face-walk dispatch: walk the mirror span whenever the caller has it
// (identical positions for real faces; the ONLY valid walk for a lazy
// sentinel face).
static u32 sw_walk_face(Term face, const AtpFtCell *face_ft, u32 *path,
                        CpVisitor visit, void *ctx) {
  if (face_ft != NULL) {
    return ft_walk_positions(face_ft, path, 0u, CP_MAX_DEPTH, visit, ctx,
                             0u);
  }
  return cp_walk_positions(face, path, 0u, CP_MAX_DEPTH, visit, ctx, 0u);
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

typedef struct {
  AtpState *s; Term lo, ro; u32 i_out, t_out; u8 o_eq, f_is_ft, rev_tree; u32 pushed;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  const AtpFtCell *lo_ft;   // mirror of lo (the walked face)
#endif
} SwVaterCtx;

// Emit the CP of one Vater partner hit (outer face at p overlapped by the
// hit's stored face).  Shared by the in-order emission and the co-rank
// pull-up below.  `sub` is the caller's per-position subterm of c->lo.
static u32 sw_vater_emit_hit(SwVaterCtx *c, const u32 *p, u32 p_len, Term sub,
                             const WmoPartnerHit *hit, u32 j) {
  AtpState *s = c->s;
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  u64 _dov_t0 = g_atp_phase_detail ? atp_now_us() : 0;
  u8 jdr = wmo_trace_dist_rhs(w, hit->trace);
  u8 tf  = (u8)(hit->face ^ jdr);
  Term lin, rin;
  sw_rename_pair(tf ? s->rhs[j] : s->lhs[j],
                 tf ? s->lhs[j] : s->rhs[j], &lin, &rin);
  if (g_atp_phase_detail) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
  u8 in_eq = s->use_unfailing_cp && !s->r_orient[j];
  return sw_form_push(s, c->lo, c->ro, sub, lin, rin, p, p_len,
                      c->t_out, s->r_trace[j], c->i_out, j,
                      c->o_eq, in_eq);
}

// Two-face co-rank SUPPRESSION (the soa-pick-113 forward-anchor test,
// inherited from the retired batch ranker): a later-leaf reverse-face CP that merely
// REPRODUCES a live equation keeps its own arrival instead of being pulled up
// beside the partner's earlier face.  Forms the candidate CP's raw sides and
// asks whether their normal forms are an already-stored equation.
static u8 sw_corank_suppressed(SwVaterCtx *c, const u32 *p, u32 p_len,
                               Term sub, const WmoPartnerHit *hit, u32 j) {
  AtpState *s = c->s;
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  u8 jdr = wmo_trace_dist_rhs(w, hit->trace);
  u8 tf  = (u8)(hit->face ^ jdr);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // The suppression test runs Term-side: materialize lazy faces first
  // (sides resolved from the sentinel value tag, stale-proof), and
  // re-address `sub` off the real outer face.
  if (term_tag(c->lo) == TAG_NUM) {
    u8 lo_is_l = (u8)(term_val(c->lo) == 0xFA151u);
    (void)atp_rule_term(s, c->i_out, 0u);
    (void)atp_rule_term(s, c->i_out, 1u);
    c->lo = lo_is_l ? s->lhs[c->i_out] : s->rhs[c->i_out];
    c->ro = lo_is_l ? s->rhs[c->i_out] : s->lhs[c->i_out];
  }
  (void)atp_rule_term(s, j, 0u);
  (void)atp_rule_term(s, j, 1u);
  if (sub == 0 || term_tag(sub) == TAG_NUM) {
    sub = cp_subterm_at(c->lo, p, p_len);
    if (sub == 0) return 0u;
  }
#endif
  Term lin, rin;
  sw_rename_pair(tf ? s->rhs[j] : s->lhs[j],
                 tf ? s->lhs[j] : s->rhs[j], &lin, &rin);
  if (term_tag(sub) == TAG_CTR && term_tag(lin) == TAG_CTR &&
      term_ext(sub) != term_ext(lin)) return 0u;   // no CP forms: no-op either way
  RewriteSubst subst = {{0}};
  if (!thvm_unify(sub, lin, &subst)) return 0u;
  Term cl = thvm_unify_apply(cp_replace_at(c->lo, p, p_len, rin), &subst);
  Term cr = thvm_unify_apply(c->ro, &subst);
  Term nl = atp_rewrite_normalize_indexed(s, cl, 4096u);
  Term nr = atp_rewrite_normalize_indexed(s, cr, 4096u);
  return (!kbo_eq(nl, nr) && atp_pop_eq_subsumed(s, nl, nr)) ? 1u : 0u;
}

// Co-rank collapse discriminator (WM KPActionRG/GR/GG order test, applied to the
// SIBLING face).  The min-anchor collapse pulls a var-differ equation's later
// face up to sit beside its earlier face, modelling WM's "one scan yields both
// unifiers at consecutive ages".  But if the earlier face's CP is DROPPED by the
// per-sigma reduction-order gate (its instantiated rewrite is uphill --
// sigma(rin) > sigma(lin)), no CP forms there, so there is nothing for the later
// face to collapse onto: WM numbers the surviving face at its OWN scan age.  This
// returns 1 iff the sibling hit's CP would be order-gate-dropped -- exactly the
// case where the pull-up must be suppressed.  Reuses the identical atp_cp_order_gate
// so the verdict matches the real formation-time drop bit-for-bit.
static u8 sw_corank_h2_gate_drops(SwVaterCtx *c, const u32 *p, u32 p_len,
                                  Term sub, const WmoPartnerHit *hit, u32 j) {
  AtpState *s = c->s;
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  u8 jdr = wmo_trace_dist_rhs(w, hit->trace);
  u8 tf  = (u8)(hit->face ^ jdr);
  Term lin, rin;
  sw_rename_pair(tf ? s->rhs[j] : s->lhs[j],
                 tf ? s->lhs[j] : s->rhs[j], &lin, &rin);
  if (term_tag(sub) == TAG_CTR && term_tag(lin) == TAG_CTR &&
      term_ext(sub) != term_ext(lin)) return 0u;   // no CP forms: not a drop
  RewriteSubst subst = {{0}};
  if (!thvm_unify(sub, lin, &subst)) return 0u;     // no CP forms: not a drop
  CriticalPair cp;
  cp.lhs  = thvm_unify_apply(c->ro, &subst);
  cp.rhs  = thvm_unify_apply(cp_replace_at(c->lo, p, p_len, rin), &subst);
  cp.peak = thvm_unify_apply(c->lo, &subst);
  cp.pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len && d < CP_MAX_DEPTH; d++) cp.pos[d] = (u8)p[d];
  cp.combo = 0xffu;
  u8 in_eq = s->use_unfailing_cp && !s->r_orient[j];
  return (atp_cp_order_gate(s, &cp, 0u, 1u, c->o_eq, in_eq) == 0u) ? 1u : 0u;
}

// Historical min-anchor collapse kill switch (THVM_ATP_CORANK_COLLAPSE=1).
// Default OFF = raw emission: every face-entry emits at its raw
// (leaf-arrival, chain) position, matching WM's U1_KPsBildenZuGleichung
// (one CP per tree entry in leaf-list + chain order, no face pairing).
static int sw_corank_collapse_on(void) {
  static int ccollapse = -1;
  if (ccollapse < 0) {
    const char *e = getenv("THVM_ATP_CORANK_COLLAPSE");
    ccollapse = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return ccollapse;
}

// True iff ANY corank experiment env is armed -- the guard for the
// equation-tree face-pairing override block in sw_vater_visit.  In the
// default config (nothing armed) that block computes overrides that no
// branch consumes (raw emission is the resolved ground truth), yet pays
// one wmo_eq_sides_var_differ walk plus an O(nh) sibling scan per hit;
// the guard skips it wholesale.  THVM_ATP_CORANK_GATE is deliberately
// NOT probed: it only modulates behavior inside the collapse-on branch,
// so it is irrelevant while the collapse itself is off.  getenv is
// probed once and cached (never in the hot loop).
static int sw_corank_active(void) {
  static int cactive = -1;
  if (cactive < 0) {
    int on = sw_corank_collapse_on();
    static const char *const names[] = {
      "THVM_ATP_CORANK_DIST", "THVM_ATP_CORANK_MAX",
      "THVM_ATP_CORANK_DINV", "THVM_ATP_CORANK_WMFACE",
      "THVM_ATP_CORANK_KBO",
    };
    for (u32 i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
      const char *e = getenv(names[i]);
      if (e != NULL && e[0] == '1') on = 1;
    }
    if (getenv("THVM_ATP_CORANK_TRACE") != NULL) on = 1;
    cactive = on;
  }
  return cactive;
}

static u32 sw_vater_visit(const u32 *p, u32 p_len, void *raw) {
  SwVaterCtx *c = (SwVaterCtx *)raw;
  AtpState *s = c->s;
  Term sub = cp_subterm_at(c->lo, p, p_len);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  const AtpFtCell *sub_ft = (c->lo_ft != NULL)
                                ? ft_subterm_at(c->lo_ft, p, p_len) : NULL;
  if (sub_ft != NULL) {
    // Span verdicts (identical to the Term checks; valid for lazy
    // sentinel faces where `sub` is 0 or a NUM).
    if (ft_is_var(sub_ft)) return c->pushed;
  } else
#endif
  if (sub == 0 || term_tag(sub) == TAG_FVR) return c->pushed;
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  static WmoPartnerHit hits[4096];
  static u32 eff_arr[4096], eff_ch[4096], jidx[4096], ord[4096];
  // forward face queries rule tree then equation tree; the reverse face (WM D)
  // queries equations BEFORE rules (Unifikation1.c:1642 vs :1601).
  for (u8 t = 0u; t < 2u; t++) {
    u8 tree = c->rev_tree ? (u8)(1u - t) : t;
    u64 _drk_t0 = g_atp_phase_detail ? atp_now_us() : 0;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    g_wmo_query_ft = sub_ft;   // consumed by the DFS fill (NULL = Term)
#endif
    u32 nh = wmo_tops_enum(w, tree, sub, hits, 4096u);
    if (g_atp_phase_detail) {
      g_atp_cpg_us_rank += atp_now_us() - _drk_t0;
      g_atp_cpg_rank_calls += nh;
    }
    if (tree == 0u) {
      // Rule tree: one chain entry per leaf, no co-rank -- emit in DFS order.
      for (u32 h = 0; h < nh; h++) {
        u32 j = sw_uid_to_idx(s, hits[h].trace);
        if (j == 0xffffffffu) continue;
        // Self-overlaps are NEVER formed in the partner Vater sweep: WM (and
        // the byte-identical rank path) ranks the i==j self-overlaps AFTER the
        // A/B partner sweeps, emitted by the explicit F/C/G steps -- for rules
        // too, not just equations.  Skip self entirely here.
        if (j == c->i_out) continue;
        // WM -gj: a ground-joinable fact gets no CP lane (the batch path's
        // atp_wmo_collect_pair returns 0 for either parent joinable).
        if (s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) {
          if (atp_cptr_pair(s, c->i_out, j))
            fprintf(stderr, "CPTR SW VATER-SKIP gj (rule tree)\n");
          continue;
        }
        if (sw_pair_exhaust_skip(s, c->i_out, j, c->f_is_ft)) {
          if (atp_cptr_pair(s, c->i_out, j))
            fprintf(stderr, "CPTR SW VATER-SKIP exhaust (rule tree)\n");
          continue;
        }
        c->pushed += sw_vater_emit_hit(c, p, p_len, sub, &hits[h], j);
      }
      continue;
    }
    // Equation tree: compute WM's tops-phase (arr, ch) key (the two-face
    // co-rank collapse), then emit in that key order.
    // WM stores an asymmetric (var-differ) equation ORIENTED, so ONE scan of
    // its distinguished face yields both unifiers at consecutive ages; thvm's
    // two stored faces index at different leaves.  The rank collapses them:
    //   - both faces reach, other face at an EARLIER leaf: this face FOLLOWS
    //     the other at (arr_o, 2*ch_o+1) -- unless its CP merely reproduces a
    //     live equation (suppression: keeps its own raw slot);
    //   - other face at a LATER leaf: this face ANCHORS at (arr, 2*ch);
    //   - same leaf or no other-face hit: raw (arr, ch).
    // Key ties break by the batch collect order = partner slot index (old
    // facts ascend), then thvm face.  eff_ch chain index = position within
    // the hit's arrival group (the leaf's chain order).
    {
      static u32 raw_ch[4096];
      u32 grp_start = 0;
      for (u32 h = 0; h < nh; h++) {
        if (h > 0 && hits[h].arrival != hits[h - 1u].arrival) grp_start = h;
        raw_ch[h]  = h - grp_start;
        eff_arr[h] = hits[h].arrival;
        eff_ch[h]  = raw_ch[h];
        jidx[h]    = sw_uid_to_idx(s, hits[h].trace);
      }
      _drk_t0 = g_atp_phase_detail ? atp_now_us() : 0;
      // Face-pairing override block for the historical corank-collapse
      // experiments (min/max/dist/dinv/wmface/kbo anchor zoo).  Raw
      // emission is the resolved ground truth (see the collapse-off
      // comment inside), so with no THVM_ATP_CORANK_* env armed the
      // block writes nothing -- eff_arr/eff_ch keep their raw (leaf-
      // arrival, chain) values -- and is skipped entirely, saving the
      // per-hit wmo_eq_sides_var_differ walk + O(nh) sibling scan.
      if (sw_corank_active()) {
        for (u32 h = 0; h < nh; h++) {
          u32 j = jidx[h];
          if (j == 0xffffffffu || j == c->i_out) continue;
          if (s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) continue;
          if (!wmo_eq_sides_var_differ(s->lhs[j], s->rhs[j])) continue;
          u32 h2 = 0xffffffffu;
          for (u32 g = 0; g < nh; g++)
            if (g != h && hits[g].trace == hits[h].trace &&
                hits[g].face != hits[h].face) { h2 = g; break; }
          if (h2 == 0xffffffffu) continue;
          u32 o_arr = hits[h2].arrival;
          {
            static int ct = -1;
            if (ct < 0) ct = getenv("THVM_ATP_CORANK_TRACE") != NULL ? 1 : 0;
            if (ct) {
              fprintf(stderr,
                      "CORANK i=%u trace=%u face=%u arr=%u o_arr=%u dist_rhs=%u "
                      "raw_ch=%u o_raw_ch=%u supp=%u sub=",
                      c->i_out, hits[h].trace, hits[h].face, hits[h].arrival,
                      o_arr, wmo_trace_dist_rhs(w, hits[h].trace), raw_ch[h],
                      raw_ch[h2], sw_corank_suppressed(c, p, p_len, sub,
                                                       &hits[h], j));
              atp_dbg_print_term(stderr, sub);
              fputc('\n', stderr);
            }
          }
          // EXPERIMENTAL, OPT-IN (THVM_ATP_CORANK_DIST=1), default OFF.
          // Hypothesis: WM stores a var-differ equation ORIENTED and indexes ONE
          // (distinguished) face; a single scan yields both unifiers at
          // consecutive ages, so the co-rank collapse should anchor the pair at
          // the DISTINGUISHED face's arrival, not the earlier-arriving face.
          // This is bit-identical to the old min-arrival heuristic whenever the
          // distinguished face arrives first; it changes only the "distinguished
          // arrives LATER" cases.  It DOES fix WolframAxioms OrAssociativity
          // firstdiv 26388 (eq 787's distinguished LHS at leaf C arr=8 vs its
          // reverse y*y face at leaf A arr=1, which min-anchoring mis-ranked
          // ahead of eq 784).  But it REGRESSES an earlier w=209 co-rank case
          // (@24820), so the true anchor condition is more nuanced than
          // "distinguished vs earlier" -- both are within the rules=263 dense
          // band.  Left opt-in for refinement; default keeps the 26387 baseline.
          // Same-leaf (o_arr==arr) stays raw.
          static int cdist = -1;
          if (cdist < 0) {
            const char *e = getenv("THVM_ATP_CORANK_DIST");
            cdist = (e != NULL && e[0] == '1') ? 1 : 0;
          }
          // THVM_ATP_CORANK_MAX=1: EXPERIMENTAL, REFUTED, default OFF.
          // Anchors a var-differ equation's CP-age pair at the LATER-arriving
          // face (max) instead of the earlier (the historical min-anchor).
          // It matched 3 ground-truth cases (OA 26388/24820, SKI @547) but
          // REGRESSED SKI @269 (prefix 546->268): min-anchor is right there,
          // max is wrong -- so the anchor is CASE-DEPENDENT, not min/max.
          // Root: the true anchor is WM's DISTINGUISHED (indexed) face's
          // arrival, but dist_rhs + the face bit do NOT encode it consistently
          // (787 wants face1@8, uid18 wants face0@15 -- both dist_rhs=1,
          // opposite bits).  Matching needs WM's canonical-direction choice for
          // unorientable equations, not a face heuristic.  Kept opt-in as a
          // documented negative.
          static int cmax = -1;
          if (cmax < 0) {
            const char *e = getenv("THVM_ATP_CORANK_MAX");
            cmax = (e != NULL && e[0] == '1') ? 1 : 0;
          }
          // THVM_ATP_CORANK_DINV=1: anchor at the DISTINGUISHED (WM-indexed)
          // face, using the INVERTED face-bit map (face != dist_rhs).  Ground
          // truth: uid18's distinguished (WI)x side is at face!=dist_rhs; the
          // CORANK_DIST (face==dist_rhs) map is inverted (it reproduced the
          // min-anchor).  The distinguished face is neither always-earlier nor
          // always-later -- it is WM's canonical/heavier side -- so this should
          // unify @547 (distinguished later) and @269 (distinguished earlier).
          static int cdinv = -1;
          if (cdinv < 0) {
            const char *e = getenv("THVM_ATP_CORANK_DINV");
            cdinv = (e != NULL && e[0] == '1') ? 1 : 0;
          }
          static int cwmf = -1;
          if (cwmf < 0) {
            const char *e = getenv("THVM_ATP_CORANK_WMFACE");
            cwmf = (e != NULL && e[0] == '1') ? 1 : 0;
          }
          // THVM_ATP_CORANK_KBO=1: anchor the var-differ pair at the REDUCIBLE
          // (KBO-larger = WM's indexed) face, computed directly from the
          // equation sides via thvm_kbo -- NOT the dist_rhs heuristic.  WM
          // indexes an equation at its reducible LHS; for uid18 (x*x=(WI)x)
          // that is (WI)x (KBO-larger, @arr15) which is what SKI @547 needs.
          // Unlike wmface (which trusts dist_rhs and mis-marks some equations,
          // regressing @269), this recomputes the indexed face per equation.
          static int ckbo = -1;
          if (ckbo < 0) {
            const char *e = getenv("THVM_ATP_CORANK_KBO");
            ckbo = (e != NULL && e[0] == '1') ? 1 : 0;
          }
          KboCmp ckbo_kc = ckbo ? thvm_kbo(s->lhs[j], s->rhs[j], s->kbo) : KBO_EQ;
          if (ckbo && o_arr != hits[h].arrival &&
              (ckbo_kc == KBO_GT || ckbo_kc == KBO_LT)) {
            // ORIENTABLE var-differ: WM indexes ONE face (the reducible LHS =
            // KBO-larger) -> collapse both faces onto it.  UNORIENTABLE
            // (KBO_UN/EQ) is indexed at BOTH faces by WM (like thvm), so it
            // falls through to the default (no collapse) -- this is the
            // orientability discriminator (@547 orientable -> anchor larger;
            // @269 unorientable -> keep raw/min).
            u8 dr = wmo_trace_dist_rhs(w, hits[h].trace);
            u8 want_rhs = (ckbo_kc == KBO_LT) ? 1u : 0u;   // rhs larger iff LT
            u8 anchor_face = (want_rhs == dr) ? 0u : 1u;
            if (hits[h].face == anchor_face) {
              eff_ch[h] = raw_ch[h] * 2u;           // reducible (indexed): anchor
            } else if (!sw_corank_suppressed(c, p, p_len, sub, &hits[h], j)) {
              eff_arr[h] = o_arr;                    // other face: follow indexed
              eff_ch[h]  = raw_ch[h2] * 2u + 1u;
            }
          } else if (cdinv && o_arr != hits[h].arrival) {
            if (hits[h].face != wmo_trace_dist_rhs(w, hits[h].trace)) {
              eff_ch[h] = raw_ch[h] * 2u;              // distinguished (inv map): anchor
            } else if (!sw_corank_suppressed(c, p, p_len, sub, &hits[h], j)) {
              eff_arr[h] = o_arr;
              eff_ch[h]  = raw_ch[h2] * 2u + 1u;
            }
          } else if (cwmf && o_arr != hits[h].arrival) {
            // hits[h].face IS the WM-face already (0 = distinguished, 1 =
            // reverse; see atp_wmo_insert_fact_ex: lhs leaf carries dist_rhs,
            // rhs carries !dist_rhs).  Anchor at the DISTINGUISHED (face==0)
            // face -- per-equation earlier or later, unlike min/max/dinv.
            if (hits[h].face == 0u) {
              eff_ch[h] = raw_ch[h] * 2u;              // distinguished: anchor
            } else if (!sw_corank_suppressed(c, p, p_len, sub, &hits[h], j)) {
              eff_arr[h] = o_arr;                       // reverse: follow distinguished
              eff_ch[h]  = raw_ch[h2] * 2u + 1u;
            }
          } else if (cmax && o_arr != hits[h].arrival) {
            if (o_arr < hits[h].arrival) {
              eff_ch[h] = raw_ch[h] * 2u;              // later face: anchor
            } else if (!sw_corank_suppressed(c, p, p_len, sub, &hits[h], j)) {
              eff_arr[h] = o_arr;                       // earlier face: follow later
              eff_ch[h]  = raw_ch[h2] * 2u + 1u;
            }
          } else if (cdist && o_arr != hits[h].arrival) {
            if (hits[h].face == wmo_trace_dist_rhs(w, hits[h].trace)) {
              eff_ch[h] = raw_ch[h] * 2u;             // distinguished: anchor
            } else if (!sw_corank_suppressed(c, p, p_len, sub, &hits[h], j)) {
              eff_arr[h] = o_arr;                      // reverse: follow dist
              eff_ch[h]  = raw_ch[h2] * 2u + 1u;
            }
          } else if (o_arr < hits[h].arrival) {
            // Suppress the pull-up when the EARLIER sibling face's CP is
            // order-gate-dropped (its instantiated rewrite is uphill --
            // sigma(rin) > sigma(lin)), so nothing forms at o_arr for this face to
            // sit beside: the surviving face keeps its own (later) arrival, matching
            // WM's scan-age numbering.  Grounded in the per-sigma KPActionRG test
            // (Unifikation1.c:1407): SKI @547 uid18's (WI)x@15 must stay late
            // because its x*x@0 sibling is uphill.  Default ON; THVM_ATP_CORANK_GATE=0
            // restores the unconditional min-anchor collapse (same-binary kill switch).
            static int cgate = -1;
            if (cgate < 0) {
              const char *e = getenv("THVM_ATP_CORANK_GATE");
              cgate = (e != NULL && e[0] == '0') ? 0 : 1;
            }
            // DEFAULT: NO pull-up at all -- every face-entry emits at its raw
            // (leaf-arrival, chain) position.  RESOLVED ground truth from the
            // instrumented `wmcli -auto` WM_CPFORMDUMP on the WolframAxioms
            // OrAssociativity E19 activation batch (fact 1088): WM's twelve
            // w1=224 eTT keepers emit in plain leaf order -- eq 787's x*x face
            // at the arr=1 leaf (chain [787, 784] newest-first) and its
            // (y.(y.y)).x face at the LAST leaf (arr=11), with NO pairing
            // collapse; thvm's raw (arr, ch) match WM's leaf order exactly,
            // and the min-anchor pull-up is what displaced the 787 keeper
            // (thvm fact 1088 vs WM 1097).  WM has no face-pairing mechanism
            // AT ALL (U1_KPsBildenZuGleichung emits one CP per tree entry in
            // leaf-list + chain order; each equation simply has two directed
            // entries) -- the whole min/max/dist/kbo anchor zoo above was an
            // approximation of raw emission.  Measured with raw emission:
            // OA -auto fact-prefix 1087 -> 1377, plain SKIToBCKW c1 prefix
            // 3516 -> 6840+ (the @3517 frontier was collapse-caused too),
            // combinator -auto 6/6 identical, test_atp 136264.
            // THVM_ATP_CORANK_COLLAPSE=1 restores the historical min-anchor
            // pull-up (with the @547 gate) as a bisection kill switch.
            if (sw_corank_collapse_on() &&
                !sw_corank_suppressed(c, p, p_len, sub, &hits[h], j) &&
                !(cgate && sw_corank_h2_gate_drops(c, p, p_len, sub,
                                                   &hits[h2], j))) {
              eff_arr[h] = o_arr;
              eff_ch[h]  = raw_ch[h2] * 2u + 1u;
            }
          } else if (o_arr > hits[h].arrival) {
            // Anchor-side doubling belongs to the collapse pairing (it opens
            // odd slots 2*ch_o+1 for pulled-up follow entries).  Under raw
            // emission (collapse OFF, the default) it must NOT fire: doubling
            // both-face anchors while o_arr<arr and single-face entries keep
            // raw eff_ch creates spurious (arr, eff_ch) collisions that break
            // ties by partner slot age instead of WM's leaf CHAIN order --
            // the WolframAxioms OrAssociativity fact-1501 batch (owner rule
            // 1050, sub x3.(x3.x1)): the shared x1.(x1.x2) leaf's chain
            // [-26,-24,-21,-18,-13,-11] emitted as [-26,-21,-24,-13,-11,-18]
            // (eq -24 doubled 1->2 collided with eq -21's raw 2; eq -18
            // doubled 3->6 fell behind the single-face -13/-11 at raw 4/5),
            // where WM's WM_CPFORMDUMP shows plain chain order.
            if (sw_corank_collapse_on()) eff_ch[h] = raw_ch[h] * 2u;
          }
        }
      }
      // Same-arrival tie order = (eff_arr, eff_ch, jidx).  eff_ch is
      // leaf-chain-derived; jidx (partner slot age) breaks only exact eff_ch
      // ties.  A THVM_ATP_CORANK_AGE experiment promoting jidx above eff_ch
      // was CATASTROPHIC (OA -auto prefix 26387 -> 1140 + spurious CPs),
      // proving WM orders same-arrival ties by leaf CHAIN order, not age --
      // so firstdiv 26388 is a narrow exception, not a general age effect.
      for (u32 h = 0; h < nh; h++) ord[h] = h;
      for (u32 a = 1; a < nh; a++) {          // insertion sort, stable
        u32 v = ord[a];
        u32 b = a;
        while (b > 0) {
          u32 u = ord[b - 1u];
          u8 gt = (eff_arr[u] > eff_arr[v]) ||
                  (eff_arr[u] == eff_arr[v] && eff_ch[u] > eff_ch[v]) ||
                  (eff_arr[u] == eff_arr[v] && eff_ch[u] == eff_ch[v] &&
                   jidx[u] != 0xffffffffu && jidx[v] != 0xffffffffu &&
                   jidx[u] > jidx[v]);
          if (!gt) break;
          ord[b] = u; b--;
        }
        ord[b] = v;
      }
      if (g_atp_phase_detail) g_atp_cpg_us_rank += atp_now_us() - _drk_t0;
      for (u32 a = 0; a < nh; a++) {
        u32 h = ord[a];
        u32 j = jidx[h];
        if (j == 0xffffffffu) continue;
        if (j == c->i_out) continue;          // self: explicit F/C/G steps
        if (s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) {
          if (atp_cptr_pair(s, c->i_out, j))
            fprintf(stderr, "CPTR SW VATER-SKIP gj (eq tree)\n");
          continue;
        }
        if (sw_pair_exhaust_skip(s, c->i_out, j, c->f_is_ft)) {
          if (atp_cptr_pair(s, c->i_out, j))
            fprintf(stderr, "CPTR SW VATER-SKIP exhaust (eq tree)\n");
          continue;
        }
        c->pushed += sw_vater_emit_hit(c, p, p_len, sub, &hits[h], j);
      }
    }
  }
  return c->pushed;
}

typedef struct {
  AtpState *s; Term lo, ro, lin, rin; u32 t_out, t_in, i_out, i_in;
  u8 o_eq, in_eq; u32 pushed;
} SwMutterCtx;
static u32 sw_mutter_visit(const u32 *q, u32 q_len, void *raw) {
  SwMutterCtx *c = (SwMutterCtx *)raw;
  if (q_len == 0u) return c->pushed;                 // eTT: PROPER subterms only
  c->pushed += sw_form_push(c->s, c->lo, c->ro, 0, c->lin, c->rin, q, q_len,
                            c->t_out, c->t_in, c->i_out, c->i_in,
                            c->o_eq, c->in_eq);
  return c->pushed;
}

// Mutter / proper-subterm phase: plant the new fact's chosen face (f_in_l ->
// f_in_r) into each old fact's PROPER subterms, partner-major in WM leaf-list
// order.  rev_tree flips the tree order (forward B: rules then eqns; reverse E:
// eqns then rules, Unifikation1.c:1654 vs :1613).
static void sw_mutter_phase(AtpState *s, AtpWmOrder *w, u32 f,
                            Term f_in_l, Term f_in_r, u8 f_in_eq, u8 f_is_ft,
                            u8 rev_tree, u32 *path, u32 *pushed) {
  static WmoPartnerHit lhits[8192];
  for (u8 t = 0u; t < 2u; t++) {
    u8 tree = rev_tree ? (u8)(1u - t) : t;
    u64 _drk_t0 = g_atp_phase_detail ? atp_now_us() : 0;
    u32 nh = wmo_leaflist_enum(w, tree, lhits, 8192u);
    if (g_atp_phase_detail) {
      g_atp_cpg_us_rank += atp_now_us() - _drk_t0;
      g_atp_cpg_rank_calls += nh;
    }
    for (u32 h = 0; h < nh; h++) {
      u32 j = sw_uid_to_idx(s, lhits[h].trace);
      if (j == 0xffffffffu) continue;   // INCLUDE self: WM's subterm sweep has no
      if (s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) {
        if (atp_cptr_pair(s, f, j))
          fprintf(stderr, "CPTR SW MUTTER-SKIP gj\n");
        continue;
      }
      if (sw_pair_exhaust_skip(s, f, j, f_is_ft)) {
        if (atp_cptr_pair(s, f, j))
          fprintf(stderr, "CPTR SW MUTTER-SKIP exhaust\n");
        continue;
      }
      u64 _dov_t0 = g_atp_phase_detail ? atp_now_us() : 0;
      u8 jdr = wmo_trace_dist_rhs(w, lhits[h].trace);  // MitAllenAusser -> l/r =? eTT(self)
      u8 tf  = (u8)(lhits[h].face ^ jdr);
      Term lo = tf ? s->rhs[j] : s->lhs[j];
      Term ro = tf ? s->lhs[j] : s->rhs[j];
      u8 o_eq = s->use_unfailing_cp && !s->r_orient[j];
      Term lin, rin;
      sw_rename_pair(f_in_l, f_in_r, &lin, &rin);
      if (g_atp_phase_detail) g_atp_cpg_us_overlap += atp_now_us() - _dov_t0;
      SwMutterCtx mc = { s, lo, ro, lin, rin, s->r_trace[j], s->r_trace[f],
                         j, f, o_eq, f_in_eq, 0u };
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      sw_walk_face(lo, tf ? s->rhs_ft[j] : s->lhs_ft[j], path,
                   sw_mutter_visit, &mc);
#else
      cp_walk_positions(lo, path, 0u, CP_MAX_DEPTH, sw_mutter_visit, &mc, 0u);
#endif
      *pushed += mc.pushed;
    }
  }
}

// WM self-overlap step F (l=?l, Unifikation1.c:1620) / C (l=?r, :1631) /
// G (r=?r, :1661): overlap the new equation's `outer` face (rhs `outer_r`) onto
// its own `inner` face (rhs `inner_r`) at ALL positions; the trivial root
// identities (F/G) are dropped by the joinable gate.  Done as explicit steps so
// they land at WM's F/C/G slots -- the A/B/D/E partner sweeps EXCLUDE self for an
// equation (WM MitAllenAusser), otherwise the trie-trick interleaves the self
// overlap too early (the Absorption picks-67/68 tie-break swap).
typedef struct {
  AtpState *s; Term lo, ro, lin, rin; u32 t_self, i_self; u8 f_eq; u32 pushed;
} SwSelfCtx;
static u32 sw_self_visit(const u32 *p, u32 p_len, void *raw) {
  SwSelfCtx *c = (SwSelfCtx *)raw;
  c->pushed += sw_form_push(c->s, c->lo, c->ro, 0, c->lin, c->rin, p, p_len,
                            c->t_self, c->t_self, c->i_self, c->i_self,
                            c->f_eq, c->f_eq);
  return c->pushed;
}
static void sw_self_phase(AtpState *s, u32 f, Term outer, Term outer_r,
                          Term inner, Term inner_r, u8 f_eq, u8 root_only,
                          u32 *path, u32 *pushed) {
  Term rn_in, rn_in_r;
  sw_rename_pair(inner, inner_r, &rn_in, &rn_in_r);
  SwSelfCtx sc = { s, outer, outer_r, rn_in, rn_in_r,
                   s->r_trace[f], f, f_eq, 0u };
  if (root_only)                       // F/C/G are toplevel-only; subterm self
    sc.pushed += sw_form_push(s, outer, outer_r, 0, sc.lin, sc.rin, path, 0u,
                              s->r_trace[f], s->r_trace[f], f, f, f_eq, f_eq);
  else {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Resolve the walked face's mirror by handle (outer is one of the
    // two stored faces of f, sentinel or real).
    sw_walk_face(outer, sw_face_ft(s, f, outer), path, sw_self_visit, &sc);
#else
    cp_walk_positions(outer, path, 0u, CP_MAX_DEPTH, sw_self_visit, &sc, 0u);
#endif
  }
  *pushed += sc.pushed;
}

static u32 thvm_atp_generate_cps_singlewalk(AtpState *s, AtpAddedRange added) {
  u32 first = added.first, last = added.first + added.count, n = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0u;
  AtpWmOrder *w = (AtpWmOrder *)s->wmo;
  if (w == NULL) return 0u;   // dispatch guarantees s->wmo != NULL
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Classic (fuse-off) arm: the sweeps and their per-position Term arm
  // consume rule faces as TERMS (the fl/fr capture, sw_rename_pair
  // sources, cp_subterm_at).  Under the lazy rule store those slots
  // hold NUM sentinels whose value tags (0xFA151 = lhs) LIE for
  // orient-swapped births -- push_rule stores the pair swapped, the
  // born-lhs sentinel lands in the rhs slot -- so the per-position
  // sentinel resolution picked the wrong face (a bare variable), the
  // walk positions found no subterms, and the sweeps formed almost
  // nothing: the lazy+classic collapse (OA classic 79 steps/5 rules vs
  // the certified 200/86, bisected to the lazy default flip 3f8b3952).
  // Materialize every rule's Terms from the mirrors (side-exact by
  // construction) BEFORE any face capture: the sentinel block never
  // fires and no value-tag guessing happens.  Fused arm untouched;
  // eager kill-config a no-op (slots already real).
  if (atp_lazy_rules_on() && !atp_cp_push_fuse_ok(s)) {
    for (u32 mi = 0; mi < s->n_rules; mi++) {
      (void)atp_rule_term(s, mi, 0u);
      (void)atp_rule_term(s, mi, 1u);
    }
  }
#endif
  u32 path[CP_MAX_DEPTH];
  u32 pushed = 0u;
  for (u32 f = first; f < last; f++) {
    // WM -gj: a ground-joinable NEW fact generates no CPs at all (every
    // batch-path collect_pair with it as a parent returns 0).
    if (s->r_gj_status[f] == ATP_GJ_ST_JOINABLE) continue;
    u8 f_eq = s->use_unfailing_cp && !s->r_orient[f];
    // A two-faced (non-mono) equation re-runs the toplevel+subterm sweeps on its
    // reverse face; a mono-equation (alpha-equivalent faces) and a rule do not
    // (WM TP_IstKeineMonogleichung, Unifikation1.c:1630).
    u8 two_faced = f_eq && !atp_eq_is_mono(s, f);
    // The new fact's WM-DISTINGUISHED (forward) face is its stored lhs only when
    // dist_rhs==0; for a dist_rhs==1 equation WM's forward face is the stored RHS,
    // so the A/B/F forward sweeps must run on `fl` (= rhs there) and D/E/G on `fr`
    // -- otherwise the forward/reverse sweeps swap and same-pri CPs get the wrong
    // age (the Absorption pick-100 L.2-vs-L.2.1 reorder).
    u8 f_dr = wmo_trace_dist_rhs(w, s->r_uid[f]);
    Term fl = f_dr ? s->rhs[f] : s->lhs[f];   // WM forward (distinguished) face
    Term fr = f_dr ? s->lhs[f] : s->rhs[f];   // WM reverse face
    // WM U1 per-fact order: A toplevel(l), B subterm(l), F self l=?l, then for a
    // two-faced equation C self r=?l, D toplevel(r), E subterm(r), G self r=?r.
    // The self-overlaps ALWAYS follow the partner sweeps (WM ranks i==j last).
    u8 f_is_ft = s->use_overlap_exhaust
                 ? atp_is_flat_transposition(s->lhs[f], s->rhs[f]) : 0u;
    SwVaterCtx va = { s, fl, fr, f, s->r_trace[f], f_eq, f_is_ft, 0u, 0u };
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    va.lo_ft = f_dr ? s->rhs_ft[f] : s->lhs_ft[f];
#endif
    g_sw_phase = 10u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    sw_walk_face(fl, va.lo_ft, path, sw_vater_visit, &va);
#else
    cp_walk_positions(fl, path, 0u, CP_MAX_DEPTH, sw_vater_visit, &va, 0u);
#endif
    pushed += va.pushed;                                       // A toplevel(l)
    g_sw_phase = 11u;
    sw_mutter_phase(s, w, f, fl, fr, f_eq, f_is_ft, 0u, path, &pushed); // B subterm(l)
    // F/C/G are ROOT-ONLY toplevel self-steps on the WM-distinguished faces fl/fr
    // (Term1MitTerm2Unifizieren on the Elter1 copy, Unifikation1.c:1620/1631/1661).
    // The SUBTERM self-overlaps come from the B/E Mutter sweeps (which now include
    // self).  C is l=?r (combo1) after Elter1Andersherum, NOT r=?l -- combo2 (r=?l)
    // is produced only by the E Mutter-self, never by an explicit step.
    // EQUATIONS ONLY: a RULE never forms a root self-overlap (WM passes the
    // rule itself as U1_KPsBildenZuRegel's toplevel Ausschluss; the batch
    // path's skip1 = i_eq ? 0 : 1).  A mono equation still runs F.
    if (f_eq) {
      g_sw_phase = 12u;
      sw_self_phase(s, f, fl, fr, fl, fr, f_eq, 1u, path, &pushed);  // F root l=?l (combo0)
    }
    if (two_faced) {
      g_sw_phase = 13u;
      // C root self uses the STORED faces (outer = stored l->r, inner =
      // stored r->l), NOT the dist_rhs-adjusted fl/fr: the batch former's
      // combo1 self-root is built on stored faces (atp_overlap_ij li x
      // rj->lj), and for a dist_rhs==1 equation the fl/fr construction
      // queues the C CP with its sides SWAPPED (the OA pick-274591
      // walk-vs-batch side swap, self CP seq 2891339).  For the
      // dist_rhs==1 case the stored order is also WM's: after
      // Elter1Andersherum the Vater copy is (r', l') and the CP is
      // (KPLinks, KPRechts) = (sigma(l'), sigma(r)) (Unifikation1.c:
      // 916-917, 1628-1633), which maps to thvm (cp.lhs, cp.rhs) =
      // (KPRechts, KPLinks) = the stored-face construction.
      sw_self_phase(s, f, s->lhs[f], s->rhs[f], s->rhs[f], s->lhs[f],
                    f_eq, 1u, path, &pushed);  // C root l=?r (combo1)
      SwVaterCtx vd = { s, fr, fl, f, s->r_trace[f], f_eq, f_is_ft, 1u, 0u };
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      vd.lo_ft = f_dr ? s->lhs_ft[f] : s->rhs_ft[f];
#endif
      g_sw_phase = 14u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      sw_walk_face(fr, vd.lo_ft, path, sw_vater_visit, &vd);
#else
      cp_walk_positions(fr, path, 0u, CP_MAX_DEPTH, sw_vater_visit, &vd, 0u);
#endif
      pushed += vd.pushed;                                     // D toplevel(r)
      g_sw_phase = 15u;
      sw_mutter_phase(s, w, f, fr, fl, f_eq, f_is_ft, 1u, path, &pushed); // E subterm(r)
      g_sw_phase = 16u;
      sw_self_phase(s, f, fr, fl, fr, fl, f_eq, 1u, path, &pushed); // G root r=?r (combo3)
    }
    g_sw_phase = 0xffu;
    // Mark f overlap-exhausted (mirror of the batch path): its superposition
    // lane is fully enumerated, so a later flat-transposition fact will not
    // re-superpose against it.
    if (s->use_overlap_exhaust && !s->r_orient[f]) s->r_overlap_done[f] = 1u;
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
    // The single-walk former IS the emission-order CP-formation path
    // (native WM U1 order; the retired batch former was proven
    // full-CPSEL-stream byte-identical to it on OA under the
    // r_overlap_done compaction fix before its deletion).
    return thvm_atp_generate_cps_singlewalk(s, added);
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span grounder: rebuild `t` with every var ABSENT from `keep` swapped
// for the minimal constant -- through ftnew_* so the GROUND flags fold
// correctly (an in-place var->const swap would leave stale ancestor
// flags).  Sets *changed when any swap happened.
static AtpFtCell *ft_ground_absent_vars(AtpFt *a, const AtpFtCell *t,
                                        const AtpFtCell *keep, u32 mc,
                                        u8 *changed) {
  if ((t->sym & WF_VAR_BIT) != 0u) {
    u32 id = t->sym & ~WF_VAR_BIT;
    if (!ft_span_has_var(keep, id)) {
      *changed = 1u;
      return ftnew_const(a, mc, 0);
    }
    return ftnew_var(a, id, 0);
  }
  u32 n = (u32)t->arity;
  if (n == 0u) return ftnew_const(a, ft_ctr_sym(t), 0);
  // Term sibling returns the subtree UNCHANGED above REWRITE_MAX_ARITY
  // (atp_grounded_instance's stack-children cap): clone verbatim, no
  // grounding below it, no changed mark.
  if (n > REWRITE_MAX_ARITY) return ft_clone_span(a, t, 0);
  AtpFtCell *kids[REWRITE_MAX_ARITY];
  const AtpFtCell *kid = t->next;
  for (u32 i = 0; i < n; i++) {
    kids[i] = ft_ground_absent_vars(a, kid, keep, mc, changed);
    kid = kid->end->next;
  }
  return ftnew_ctr(a, ft_ctr_sym(t), (u16)n, kids, 0);
}

// Span sibling of atp_try_ground_and_push: verdicts off the spans, the
// commit through a nested lazy bracket (fresh per-push sentinels).
static u8 atp_try_ground_and_push_ft(AtpState *s, const AtpFtCell *lhs_ft,
                                     const AtpFtCell *rhs_ft,
                                     AtpAddedRange *r) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  u8 changed = 0u;
  u32 mc = (u32)term_ext(s->min_const);
  AtpFtCell *g_rhs = ft_ground_absent_vars(a, rhs_ft, lhs_ft, mc, &changed);
  if (!changed || thvm_kbo_ft(lhs_ft, g_rhs, s->kbo) != KBO_GT) {
    ft_free_span(a, g_rhs, g_rhs->end);
    return 0;
  }
  Term sl = term_new(0, TAG_NUM, 0, 0xFA151u);
  Term sr = term_new(0, TAG_NUM, 0, 0xFA152u);
  AtpLazyRule saved = g_lazy_rule;
  g_lazy_rule.span_l = lhs_ft;
  g_lazy_rule.span_r = g_rhs;
  g_lazy_rule.sent_l = sl;
  g_lazy_rule.sent_r = sr;
  g_lazy_rule.armed  = 1u;
  u8 pushed = atp_push_rule(s, sl, sr);
  g_lazy_rule = saved;
  ft_free_span(a, g_rhs, g_rhs->end);   // mirror adoption cloned it
  if (!pushed) return 0;
  r->count++;
  r->fvi_count++;
  return 1;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  {
    const AtpFtCell *bl = atp_lazy_span_of(lhs);
    const AtpFtCell *br = atp_lazy_span_of(rhs);
    if (bl != NULL && br != NULL) {
      // Lazy UN commit: both grounded legs off the spans.
      atp_try_ground_and_push_ft(s, bl, br, r);
      atp_try_ground_and_push_ft(s, br, bl, r);
      return;
    }
  }
#endif
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

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span sibling of atp_lr_sortieren_rec: identical verdicts off the
// mirror cells (var < non-var, label code, arity, argument-wise).
static int atp_lr_sortieren_rec_ft(const AtpFtCell *lhs,
                                   const AtpFtCell *rhs) {
  u8 lv = (u8)((lhs->sym & WF_VAR_BIT) != 0u);
  u8 rv = (u8)((rhs->sym & WF_VAR_BIT) != 0u);
  if (lv && rv) return 0;
  if (lv) return -1;
  if (rv) return 1;
  u32 lf = lhs->sym, rf = rhs->sym;
  if (lf != rf) return (lf < rf) ? -1 : 1;
  u32 ln = (u32)lhs->arity, rn = (u32)rhs->arity;
  if (ln != rn) return (ln < rn) ? -1 : 1;
  const AtpFtCell *lk = lhs->next, *rk = rhs->next;
  for (u32 i = 0; i < ln; i++) {
    int c = atp_lr_sortieren_rec_ft(lk, rk);
    if (c != 0) return c;
    lk = lk->end->next;
    rk = rk->end->next;
  }
  return 0;
}

// Bracket dispatch: verdict off the spans when the operands are lazy
// sentinels (the orient arm under an armed g_lazy_rule), else the Term
// recursion.  Handle-identity mapping, so it stays correct after the
// LRSortieren swap exchanges the two locals.
static int atp_lr_sortieren_disp(Term lhs, Term rhs) {
  const AtpFtCell *bl = atp_lazy_span_of(lhs);
  const AtpFtCell *br = atp_lazy_span_of(rhs);
  if (bl != NULL && br != NULL) return atp_lr_sortieren_rec_ft(bl, br);
  return atp_lr_sortieren_rec(lhs, rhs);
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// True iff term `t` contains a constructor with external id `ext` anywhere.
static u8 atp_term_has_ctr(Term t, u32 ext) {
  if (term_tag(t) != TAG_CTR) return 0u;
  if (term_ext(t) == ext) return 1u;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (atp_term_has_ctr(term_ctr_at(t, i), ext)) return 1u;
  return 0u;
}

// True iff `a` is a combinator REDEX relative to reduct `b`: descend a's
// leftmost application spine to a 0-ary constructor (the combinator
// constant) that is ABSENT from `b`, and `b` introduces no fresh variable
// (vars(b) subset vars(a)).  The directed rule a -> b is then a terminating
// combinator reduction.  Used by the force-orient site to detect the redex
// side regardless of which way the equation is stored (bench .pr keeps the
// redex on the LHS; the WL wire SpezNormierung flips it to the RHS).
static u8 atp_comb_redex_side(Term a, Term b) {
  Term head = a;
  while (term_tag(head) == TAG_CTR && term_ctr_n(head) > 0u)
    head = term_ctr_at(head, 0);
  return (u8)(term_tag(head) == TAG_CTR && term_ctr_n(head) == 0u &&
              gj_vars_subset(b, a) && !atp_term_has_ctr(b, term_ext(head)));
}

// Orient via KBO and push the rule(s).  See header comment for the
// dispatch table.  Atomic: if the unfailing fallback can't fit both
// orientations, neither is added.
fn AtpAddedRange thvm_atp_orient_and_add(AtpState *s, Term lhs, Term rhs) {
  AtpAddedRange r = {0, 0, 0};
  if (s == NULL) return r;

  // -auto combinator-definition force-orientation (THVM_ATP_FORCE_DEF_ORIENT=1).
  // WM -auto orients a variable-DUPLICATING combinator definition
  // ((Sx)y z = (xz)(yz), (Wx)y = (xy)y) as a directed rule L->R, which no
  // reduction order can (the leading CONSTANT is order-incomparable with the
  // duplicated VARIABLE, and the multiset var-condition fails).  It is
  // equationally SOUND (lhs->rhs is a valid consequence; see
  // thvm_atp_install_oriented_rule).  Restricted to a TERMINATING reduction:
  //   (1) genuinely KBO-unorientable (never overrides an orientable eq),
  //   (2) var-safe: vars(rhs) subset vars(lhs) (no fresh vars), and
  //   (3) the LHS's leading operator constant is ABSENT from the RHS -- so a
  //       self-referential fixpoint (Y = x(Yx)) stays an equation (rewriter
  //       cannot loop) and a symmetric axiom (Wolfram/Sheffer/Boolean, whose
  //       op appears on BOTH sides) is untouched, keeping OA -auto 26387 etc.
  // Off by default; the combinator -auto bench path opts in.
  {
    static int fdo_env = -1;
    if (fdo_env < 0) fdo_env = getenv("THVM_ATP_FORCE_DEF_ORIENT") ? 1 : 0;
    if ((fdo_env || s->use_force_def_orient)
        && atp_compare(s, lhs, rhs) == KBO_UN) {
      // Orientation-agnostic: the combinator REDEX may be on either side --
      // the bench/.pr stores `S x y z = (xz)(yz)` (redex LHS) but the WL
      // wire SpezNormierung stores it flipped `(xz)(yz) = S x y z` (redex
      // RHS).  Detect which side is the redex (leftmost-spine leading is a
      // 0-ary constant ABSENT from the other side, var-safe) and orient
      // redex -> reduct.
      Term rl = 0, rr = 0;
      if (atp_comb_redex_side(lhs, rhs)) { rl = lhs; rr = rhs; }
      else if (atp_comb_redex_side(rhs, lhs)) { rl = rhs; rr = lhs; }
      if (rl != 0) {
        u32 idx = s->n_rules;
        if (atp_push_rule(s, rl, rr)) {
          r.first = idx; r.count = 1;
          // atp_push_rule recomputes r_orient via atp_compare (KBO_UN here)
          // -> it would store this as an unoriented E-equation (applied only
          // on order-DECREASING ground instances).  But WM -auto orients the
          // combinator definition as a DIRECTED rule ("orient(7,x)") and
          // applies it unconditionally -- so the heavy variable-duplicating
          // S-redex reduces during goal normalization instead of being left
          // unreduced (which forces thvm to over-complete).  Force it.
          if (!s->r_orient[idx]) { s->r_orient[idx] = 1u; s->n_unorient--; }
        }
        return r;
      }
    }
  }

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
  KboCmp c;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  {
    const AtpFtCell *bl = atp_lazy_span_of(lhs);
    const AtpFtCell *br = atp_lazy_span_of(rhs);
    if (bl != NULL && br != NULL) {
      // Lazy commit: the verdict off the spans (the pop already
      // pre-checked GT/LT admissibility; plain KBO by the arm gate).
      c = thvm_kbo_ft(bl, br, s->kbo);
    } else {
      c = atp_compare(s, lhs, rhs);
    }
  }
#else
  c = atp_compare(s, lhs, rhs);
#endif
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
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      if (s->use_lr_sortieren && atp_lr_sortieren_disp(lhs, rhs) > 0) {
#else
      if (s->use_lr_sortieren && atp_lr_sortieren_rec(lhs, rhs) > 0) {
#endif
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
      // WM SpezNormierung intake canonicalisation (SpezNormierung.c:
      // 517-534), AXIOM class only: WM STORES an initial unorientable
      // equation LRSortieren-swapped (structurally smaller side left),
      // so thvm's kept-E byte order must follow (Sheffer
      // OrAssociativity fact 2 stored reversed vs WM).  Superposition /
      // SIMPLIFY kept-Es keep the emission order -- WM never re-sorts
      // those.  The physical swap trips last_cp_wm_side_swapped, and
      // the dist wrapper's XOR then lands the distinguished face on
      // the (now canonical) stored LHS: sweep geometry is unchanged,
      // only the stored bytes and what store-order readers (order
      // gates, face registration) see.
      if (s->use_emission_order
          && !s->last_cp_wm_side_swapped && !s->use_lr_sortieren
          && s->last_popped_kind == (u8)TRACE_AXIOM
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          && atp_lr_sortieren_disp(lhs, rhs) > 0) {
#else
          && atp_lr_sortieren_rec(lhs, rhs) > 0) {
#endif
        s->last_cp_wm_side_swapped = 1u;
      }
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

