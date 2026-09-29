// === Post-PROVED proof-cone norm re-derivation ======================
//
// With whole-run RecordNorm off (the only memory-viable mode on deep
// completions), two trace-entry kinds carry an UNRECORDED engine
// normalization between their parent's equation and their own:
//   * TRACE_ORIENT whose parent is the popped CP: the pop-time doR+doE
//     normalize between the raw CP pair and the oriented rule;
//   * TRACE_SIMPLIFY: the interreduce-victim renormalize (WM demote
//     drain doR, or the RHS-composition doR+doE) between the dropped
//     fact and its requeued / composed form.
// The WL lift bridges such gaps by BFS (emitNorm) over rules alive at
// extraction -- which fails when the bridge fired a rule retired
// mid-run or an ordered-equation step whose grounding BFS cannot
// invert (OrAssociativity ti=356685; project_atp_wm_preset_tuning
// 2026-07-02).  This pass repairs the goal-connected bridges AFTER the
// engine terminates (zero engine-loop impact): re-run the engine's own
// FT normalizer over the HISTORICAL rule set R(ti), recording
// TRACE_NORM_STEP entries, and re-thread the entry's parent onto the
// chain tail when the replay lands on the recorded equation.  Entries
// whose replay misses are left untouched -- the WL BFS fallback sees
// them exactly as before.
//
// Scope: EVERY goal-run ORIENT / SIMPLIFY entry whose stored pair
// differs from its parent's (up to orientation swap / renaming).
// The OrAssociativity blocker ti=356685 is a plain VARIABLE rule
// (the WL debug prints rendered its var ids with the conjecture's
// names, which mimicked a skolem-constant family) -- there is no
// goal-symbol discriminator in the C trace, so the pass repairs the
// whole bridge population (~1.4k entries on OrAssociativity, each a
// bounded innermost replay).  Runs only when a goal is set (a pure
// completion run lifts no goal chain).
//
// R(ti) is derived from the trace alone, mirroring the WL lift's
// aliveRulesAt model: births are TRACE_ORIENT / TRACE_FVI entries;
// a birth dies at the first TRACE_SIMPLIFY entry citing it as
// parent_a.  Alive at ti = born before ti, not dead before ti.

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Defined in ft_norm.c (included above); slice + record entry used by
// the cone replay below.
AtpFtCell *atp_rewrite_normalize_ft_slice_record(AtpState *s, AtpFtCell *t,
                                                 u32 slice_first,
                                                 u32 slice_count,
                                                 u32 step_cap, u8 doE,
                                                 Term eq_other, u8 side,
                                                 u32 *chain_tail);

// Load trace entry `idx`'s equation: the off-heap packed pair when
// trace_pack is on, else the entry's children 2/3.  Returns 0 when the
// entry carries no real pair (NUM sentinels without a packed record,
// e.g. a leaned TRACE_CP that never became a rule).
static int atp_cone_eq(AtpState *s, u32 idx, Term *lhs, Term *rhs) {
  if (atp_trace_eq_load(s, idx, lhs, rhs)) return 1;
  Term e = s->trace[idx];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) return 0;
  Term l = term_ctr_at(e, 2);
  Term r = term_ctr_at(e, 3);
  if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) return 0;
  *lhs = l;
  *rhs = r;
  return 1;
}

// Pair equality up to variable renaming: exact structural first, then
// jointly var-canonicalized.  The replayed chain may re-number
// variables relative to the stored entry (orient_and_add canonicalizes
// rule sides); the WL chain consumers compare sides via cplCanonVars,
// so an alpha-variant landing is a valid re-derivation.
static int atp_cone_pair_eq(Term al, Term ar, Term bl, Term br) {
  if (kbo_eq(al, bl) && kbo_eq(ar, br)) return 1;
  Term xl = al, xr = ar, yl = bl, yr = br;
  thvm_normalize_vars(&xl, &xr);
  thvm_normalize_vars(&yl, &yr);
  return (kbo_eq(xl, yl) && kbo_eq(xr, yr)) ? 1 : 0;
}

// Span sibling of atp_cone_eq: the packed pair loads DIRECTLY as
// spans (acp_unpack_ft, zero interchange).  Unpacked traces fall
// back to the entry children through ft_from_term -- only the
// non-trace-pack configs pay that.  Caller owns both spans.
static int atp_cone_eq_ft(AtpState *s, u32 idx,
                          AtpFtCell **lhs, AtpFtCell **rhs) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  if (s->trace_pack && s->trace_eq_off != NULL) {
    u64 o = s->trace_eq_off[idx];
    if (o != 0u) {
      const u8 *pp = s->trace_eq_buf + (o - 1u);
      *lhs = acp_unpack_ft(a, &pp);
      *rhs = acp_unpack_ft(a, &pp);
      return 1;
    }
  }
  Term e = s->trace[idx];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) return 0;
  Term l = term_ctr_at(e, 2);
  Term r = term_ctr_at(e, 3);
  if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) return 0;
  *lhs = ft_from_term(a, l, 0);
  *rhs = ft_from_term(a, r, 0);
  return 1;
}

static void atp_cone_free_pair(AtpFt *a, AtpFtCell *l, AtpFtCell *r) {
  if (l != NULL) ft_free_span(a, l, l->end);
  if (r != NULL) ft_free_span(a, r, r->end);
}

// Span sibling of atp_cone_pair_eq: ft_eq first, then a jointly
// var-canonicalized compare on throwaway clones.
static int atp_cone_pair_eq_ft(AtpFt *a,
                               const AtpFtCell *al, const AtpFtCell *ar,
                               const AtpFtCell *bl, const AtpFtCell *br) {
  if (ft_eq(al, bl) && ft_eq(ar, br)) return 1;
  AtpFtCell *xl = ft_clone_span(a, al, 0), *xr = ft_clone_span(a, ar, 0);
  AtpFtCell *yl = ft_clone_span(a, bl, 0), *yr = ft_clone_span(a, br, 0);
  ft_normalize_vars_pair(xl, xr);
  ft_normalize_vars_pair(yl, yr);
  int eq = (ft_eq(xl, yl) && ft_eq(xr, yr)) ? 1 : 0;
  atp_cone_free_pair(a, xl, xr);
  atp_cone_free_pair(a, yl, yr);
  return eq;
}

// Span-side orientation verdict for a replayed birth: the plain-KBO /
// LPO streaming compares; any other order materializes the pair once
// (non-default configs only).
static u8 atp_cone_orient_gt_ft(AtpState *s, const AtpFtCell *bl,
                                const AtpFtCell *br) {
  if (atp_order_is_plain_kbo(s))
    return (u8)(thvm_kbo_ft(bl, br, s->kbo) == KBO_GT);
  if (s->lpo != NULL && s->wpo == NULL && s->rpo == NULL)
    return (u8)(thvm_lpo_ft(bl, br, s->lpo) == LPO_GT);
  Term l = ft_to_term(bl), r = ft_to_term(br);
  return (u8)(atp_compare(s, l, r) == KBO_GT);
}

// Install the historical rule window "R alive at trace time tw" into
// the temporary slots past n_rules0: born (TRACE_ORIENT / TRACE_FVI)
// before tw, not dead (first citing TRACE_SIMPLIFY) before tw.  Birth
// pairs load lazily into the caller's caches; incl_parent == 0 skips
// the birth at p_a (the SIMPLIFY victim's own rule).  Returns the
// installed slot count; *win_unor_out gets the window's unorientable
// count.  Caller owns setting/restoring s->n_rules / s->n_unorient.
// Wide windows (emission re-derivation's last resort): every birth
// before the window time, dead or not -- a derived equation stays true
// after the engine retired it, and the death model misses retirements
// that leave no SIMPLIFY record.
static u8 g_cone_win_wide = 0u;
// Symbol filter (emission re-derivation's fast pass): a nonzero mask
// admits only births whose rewriting face uses function symbols inside
// it (a 64-bit bloom by symbol id), cutting a thousands-rule window to
// the rules a chain between two known endpoints can plausibly apply.
// The per-birth face masks are the caller's (NULL = no filter).
static u64  g_cone_win_mask = 0u;
static u64 *g_cone_birth_lmask = NULL;
static u64 *g_cone_birth_rmask = NULL;

static u64 atp_cone_span_mask(const AtpFtCell *t) {
  u64 m = 0u;
  const AtpFtCell *end_after = (t->end != NULL) ? t->end->next : NULL;
  for (const AtpFtCell *c = t; c != NULL && c != end_after; c = c->next) {
    if ((c->sym & WF_VAR_BIT) == 0u) m |= 1ull << (c->sym & 63u);
  }
  return m;
}

static u32 atp_cone_install_window(AtpState *s, u32 n_rules0,
                                   const u32 *birth_ti,
                                   const u32 *birth_dead,
                                   u8 *birth_state, u8 *birth_or,
                                   AtpFtCell **birth_lft,
                                   AtpFtCell **birth_rft,
                                   u32 n_births, u32 tw, u8 incl_parent,
                                   u32 p_a, u32 *win_unor_out) {
  u32 k = 0u, win_unor = 0u;
  for (u32 b = 0; b < n_births; b++) {
    if (birth_ti[b] >= tw) break;
    if (!g_cone_win_wide && birth_dead[b] != ATP_TRACE_NONE &&
        birth_dead[b] < tw)
      continue;
    if (!incl_parent && birth_ti[b] == p_a) continue;
    if (birth_state[b] == 0u) {
      AtpFtCell *bl = NULL, *br = NULL;
      if (!atp_cone_eq_ft(s, birth_ti[b], &bl, &br)) {
        birth_state[b] = 2u;
      } else {
        birth_lft[b]   = bl;
        birth_rft[b]   = br;
        birth_or[b]    = atp_cone_orient_gt_ft(s, bl, br);
        birth_state[b] = 1u;
        if (g_cone_birth_lmask != NULL) {
          g_cone_birth_lmask[b] = atp_cone_span_mask(bl);
          g_cone_birth_rmask[b] = atp_cone_span_mask(br);
        }
      }
    }
    if (birth_state[b] != 1u) continue;
    if (g_cone_win_mask != 0u && g_cone_birth_lmask != NULL) {
      u8 lhs_in = (u8)((g_cone_birth_lmask[b] & ~g_cone_win_mask) == 0u);
      u8 rhs_in = (u8)((g_cone_birth_rmask[b] & ~g_cone_win_mask) == 0u);
      if (!(lhs_in || (!birth_or[b] && rhs_in))) continue;
    }
    atp_ensure_rule_cap(s, n_rules0 + k + 1u);
    u32 w = n_rules0 + k;
    s->lhs[w]      = 0;   // FT path never reads the Term slots
    s->rhs[w]      = 0;
    s->lhs_ft[w]   = birth_lft[b];
    s->rhs_ft[w]   = birth_rft[b];
    s->r_orient[w] = birth_or[b];
    s->r_dead[w]   = 0u;
    s->r_trace[w]  = birth_ti[b];
    if (!birth_or[b]) win_unor++;
    k++;
  }
  *win_unor_out = win_unor;
  return k;
}
#endif

fn void thvm_atp_record_goal_cone(AtpState *s) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (s == NULL || s->trace == NULL || s->n_trace == 0u) return;
  if (s->conj_sym_mask == 0ull) return;
  static int dbg = -1;
  if (dbg < 0) dbg = atp_env_on("THVM_ATP_CONE_DEBUG");
  // Treatment-at-emission (the ORIENT two-phase rungs + their extra
  // chain headroom).  THVM_ATP_EMIT_TREAT=0 is the kill switch: the
  // 4-rung ladder and 160-step headroom, byte-identical to the
  // pre-two-phase emission.
  static int emit_treat = -1;
  if (emit_treat < 0) emit_treat = atp_env_off("THVM_ATP_EMIT_TREAT");
  const u32 n0_trace = s->n_trace;

  // Threshold GC between candidates (trace_pack only, mirroring the
  // interreduce WM_IR_MAYBE_GC gate).  Safe: no live Term is held in a
  // C local across candidates -- the FT rule cache is address-stable
  // across a collection, and the birth/death arrays are plain u32.
#define ATP_CONE_MAYBE_GC()                                                   \
  do {                                                                        \
    if (s->trace_pack && gc_enabled() &&                                      \
        (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {           \
      thvm_atp_gc_collect(s);                                                 \
    }                                                                         \
  } while (0)

  // Pass 1a: births (TRACE_ORIENT / TRACE_FVI), ascending trace order.
  u32 n_births = 0u;
  for (u32 i = 0; i < n0_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR) continue;
    u32 reason = term_ext(e);
    if (reason == TRACE_ORIENT || reason == TRACE_FVI) n_births++;
  }
  if (n_births == 0u) return;
  u32 *birth_ti    = (u32 *)malloc((size_t)n_births * sizeof(u32));
  u32 *birth_dead  = (u32 *)malloc((size_t)n_births * sizeof(u32));
  u8  *birth_state = (u8  *)calloc(n_births, 1u);   // 0 unloaded / 1 ok / 2 bad
  u8  *birth_or    = (u8  *)calloc(n_births, 1u);
  AtpFtCell **birth_lft = (AtpFtCell **)calloc(n_births, sizeof(AtpFtCell *));
  AtpFtCell **birth_rft = (AtpFtCell **)calloc(n_births, sizeof(AtpFtCell *));
  if (birth_ti == NULL || birth_dead == NULL || birth_state == NULL ||
      birth_or == NULL || birth_lft == NULL || birth_rft == NULL) {
    free(birth_ti); free(birth_dead); free(birth_state); free(birth_or);
    free(birth_lft); free(birth_rft);
    return;
  }
  {
    u32 w = 0u;
    for (u32 i = 0; i < n0_trace; i++) {
      Term e = s->trace[i];
      if (term_tag(e) != TAG_CTR) continue;
      u32 reason = term_ext(e);
      if (reason != TRACE_ORIENT && reason != TRACE_FVI) continue;
      birth_ti[w]   = i;
      birth_dead[w] = ATP_TRACE_NONE;
      w++;
    }
  }

  // Pass 1b: deaths (first TRACE_SIMPLIFY citing the birth as
  // parent_a) + bridge candidates (ORIENT / SIMPLIFY entries whose
  // pair differs from their parent's).  birth_ti is ascending, so a
  // cited parent resolves by binary search.
  u32 *cand = (u32 *)malloc((size_t)4096u * sizeof(u32));
  u32 n_cand = 0u;
  if (cand == NULL) {
    free(birth_ti); free(birth_dead); free(birth_state); free(birth_or);
    free(birth_lft); free(birth_rft);
    return;
  }
  u32 n_os = 0u, n_par = 0u, n_eq = 0u, n_sym = 0u;
  for (u32 i = 0; i < n0_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR) continue;
    u32 reason = term_ext(e);
    if (reason != TRACE_ORIENT && reason != TRACE_SIMPLIFY) continue;
    n_os++;
    u32 p_a = (u32)term_val(term_ctr_at(e, 0));
    if (reason == TRACE_SIMPLIFY && p_a != ATP_TRACE_NONE) {
      // Death mark: first SIMPLIFY citing birth p_a.
      u32 lo = 0u, hi = n_births;
      while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2u;
        if (birth_ti[mid] < p_a) lo = mid + 1u; else hi = mid;
      }
      if (lo < n_births && birth_ti[lo] == p_a &&
          birth_dead[lo] == ATP_TRACE_NONE) {
        birth_dead[lo] = i;
      }
    }
    if (p_a == ATP_TRACE_NONE || p_a >= n0_trace) continue;
    Term pe = s->trace[p_a];
    if (term_tag(pe) != TAG_CTR) continue;
    if (term_ext(pe) == TRACE_NORM_STEP) continue;   // already chained
    n_par++;
    AtpFt *fa = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *el = NULL, *er = NULL, *pl = NULL, *pr = NULL;
    if (!atp_cone_eq_ft(s, i, &el, &er)) continue;
    n_eq++;
    if (!atp_cone_eq_ft(s, p_a, &pl, &pr)) {
      atp_cone_free_pair(fa, el, er);
      continue;
    }
    // Replay-cost guard: a monster parent pair (a raw CP that later
    // avalanche phases can produce) makes the innermost replay itself
    // heavy; such an entry keeps the WL BFS fallback.
    u8 keep = 0u;
    if (ft_symbol_count_span(pl) + ft_symbol_count_span(pr) <= 4096u) {
      n_sym++;
      // A bridge exists only when the entry's pair differs from the
      // parent's (up to orientation swap / renaming) -- equal pairs
      // resolve through the WL cplEqSetQ check as before.
      if (!atp_cone_pair_eq_ft(fa, pl, pr, el, er) &&
          !atp_cone_pair_eq_ft(fa, pl, pr, er, el)) {
        keep = 1u;
      }
    }
    atp_cone_free_pair(fa, el, er);
    atp_cone_free_pair(fa, pl, pr);
    if (keep && n_cand < 4096u) cand[n_cand++] = i;
    if ((i & 0xffffu) == 0u) ATP_CONE_MAYBE_GC();
  }
  ATP_CONE_MAYBE_GC();
  if (dbg) {
    fprintf(stderr, "[cone] births=%u candidates=%u n_trace=%u t_max=%u "
            "os=%u par=%u eq=%u sized=%u\n",
            n_births, n_cand, s->n_trace, s->t_max,
            n_os, n_par, n_eq, n_sym);
  }

  // Pass 2: per-candidate re-derivation over R(ti), installed as a
  // temporary rule-slot window past the live set.  The engine loop is
  // finished; n_rules / n_unorient are restored before every return
  // path and before any collection, so extraction below sees the live
  // R untouched.
  const u32 n_rules0   = s->n_rules;
  const u32 n_unor0    = s->n_unorient;
  AtpFt *arena = (AtpFt *)s->ft_arena_ptr;
  u32 n_fixed = 0u;
  for (u32 c = 0; c < n_cand && arena != NULL; c++) {
    ATP_CONE_MAYBE_GC();
    // Chain headroom: a repair pushes at most 4 * 64 NORM_STEPs (the
    // two-phase rungs run two windows, each normalizing both sides;
    // 2 * 64 with the two-phase rungs switched off).
    if (s->n_trace + (emit_treat ? 320u : 160u) > s->t_max) break;
    u32 ti = cand[c];
    Term e = s->trace[ti];
    u32 reason = term_ext(e);
    u32 p_a    = (u32)term_val(term_ctr_at(e, 0));
    AtpFtCell *pl = NULL, *pr = NULL, *el = NULL, *er = NULL;
    if (!atp_cone_eq_ft(s, ti, &el, &er)) continue;
    if (!atp_cone_eq_ft(s, p_a, &pl, &pr)) {
      atp_cone_free_pair(arena, el, er);
      continue;
    }

    int success = 0;
    // Attempt ladder: (do_lhs, doE, incl_parent, two_phase) tuples
    // ordered by the historical shape most likely to have produced
    // this entry.
    //   ORIENT   -- pop-normalize (both sides, doR+doE) first;
    //   SIMPLIFY -- demote-drain (both sides, doR, victim excluded)
    //               first, then the RHS-composition shape (rhs only,
    //               doR+doE, victim's own rule still live).
    //   ORIENT two-phase (rungs 4/5) -- the recorded rule composes the
    //     FORMATION-time treatment (KPBehandelt: doR over R(p_a), the
    //     parent TRACE_CP's window) with the POP-time normalize
    //     (doR/doE over R(ti)).  A treatment demodulator interreduced
    //     away between formation and pop is invisible to the
    //     single-window rungs (GRP706-1 t765068's f/ld swap) -- replay
    //     both windows in sequence, threading ONE chain across them.
    u8 att_lhs[6], att_doE[6], att_inc[6], att_2ph[6];
    u32 n_att;
    if (reason == TRACE_ORIENT) {
      att_lhs[0] = 1u; att_doE[0] = 1u; att_inc[0] = 0u; att_2ph[0] = 0u;
      att_lhs[1] = 1u; att_doE[1] = 0u; att_inc[1] = 0u; att_2ph[1] = 0u;
      att_lhs[2] = 0u; att_doE[2] = 1u; att_inc[2] = 1u; att_2ph[2] = 0u;
      att_lhs[3] = 0u; att_doE[3] = 0u; att_inc[3] = 1u; att_2ph[3] = 0u;
      att_lhs[4] = 1u; att_doE[4] = 1u; att_inc[4] = 1u; att_2ph[4] = 1u;
      att_lhs[5] = 1u; att_doE[5] = 0u; att_inc[5] = 1u; att_2ph[5] = 1u;
      n_att = emit_treat ? 6u : 4u;
    } else {
      att_lhs[0] = 1u; att_doE[0] = 0u; att_inc[0] = 0u; att_2ph[0] = 0u;
      att_lhs[1] = 1u; att_doE[1] = 1u; att_inc[1] = 0u; att_2ph[1] = 0u;
      att_lhs[2] = 0u; att_doE[2] = 1u; att_inc[2] = 1u; att_2ph[2] = 0u;
      att_lhs[3] = 0u; att_doE[3] = 0u; att_inc[3] = 1u; att_2ph[3] = 0u;
      n_att = 4u;
    }
    for (u32 a_i = 0; a_i < n_att && !success; a_i++) {
      // Install the window (per attempt: the parent-birth inclusion
      // and the window time differ between shapes -- a two-phase rung
      // starts in the FORMATION window R(p_a)).
      u32 win_unor = 0u;
      u32 k = atp_cone_install_window(s, n_rules0, birth_ti, birth_dead,
                                      birth_state, birth_or,
                                      birth_lft, birth_rft, n_births,
                                      att_2ph[a_i] ? p_a : ti,
                                      att_inc[a_i], p_a, &win_unor);
      if (k == 0u) continue;
      s->n_rules    = n_rules0 + k;
      s->n_unorient = n_unor0 + win_unor;

      u32 mark  = s->n_trace;
      u32 chain = p_a;
      // Non-pack configs record Term pairs: hand the recorder real
      // eq_other Terms (decoded once per attempt).  Pack configs (the
      // default) ride the span bracket -- zero decodes.
      AtpFtCell *l2 = pl, *r2 = pr;
      AtpFtCell *l2_own = NULL, *r2_own = NULL;
      Term eq_other_l = s->trace_pack ? 0 : ft_to_term(pr);
      // Phase-1 doE: a two-phase rung's first window replays the
      // formation-time treatment, which is doR-only by definition
      // (KPBehandelt `-kg "r"`); att_doE applies to its phase 2.
      u8 doE1 = att_2ph[a_i] ? 0u : att_doE[a_i];
      if (att_lhs[a_i]) {
        g_rec_eq_other_ft = pr;
        AtpFtCell *lf = atp_rewrite_normalize_ft_slice_record(
            s, ft_clone_span(arena, pl, 0), n_rules0, k, 64u,
            doE1, eq_other_l, 0u, &chain);
        g_rec_eq_other_ft = NULL;
        l2 = lf;
        l2_own = lf;
      }
      {
        g_rec_eq_other_ft = l2;
        AtpFtCell *rf = atp_rewrite_normalize_ft_slice_record(
            s, ft_clone_span(arena, pr, 0), n_rules0, k, 64u,
            doE1, s->trace_pack ? 0 : ft_to_term(l2), 1u, &chain);
        g_rec_eq_other_ft = NULL;
        r2 = rf;
        r2_own = rf;
      }
      if (att_2ph[a_i]) {
        // Phase 2: swap in the ENTRY-time window R(ti) and run the
        // pop-normalize shape on the treated pair, continuing the SAME
        // chain -- the emitted record chain crosses both windows as a
        // seamless sequence of single rewrites.
        s->n_rules    = n_rules0;
        s->n_unorient = n_unor0;
        k = atp_cone_install_window(s, n_rules0, birth_ti, birth_dead,
                                    birth_state, birth_or,
                                    birth_lft, birth_rft, n_births,
                                    ti, att_inc[a_i], p_a, &win_unor);
        if (k > 0u) {
          s->n_rules    = n_rules0 + k;
          s->n_unorient = n_unor0 + win_unor;
          g_rec_eq_other_ft = r2;
          AtpFtCell *lf2 = atp_rewrite_normalize_ft_slice_record(
              s, l2, n_rules0, k, 64u, att_doE[a_i],
              s->trace_pack ? 0 : ft_to_term(r2), 0u, &chain);
          g_rec_eq_other_ft = NULL;
          l2 = lf2;
          l2_own = lf2;
          g_rec_eq_other_ft = l2;
          AtpFtCell *rf2 = atp_rewrite_normalize_ft_slice_record(
              s, r2, n_rules0, k, 64u, att_doE[a_i],
              s->trace_pack ? 0 : ft_to_term(l2), 1u, &chain);
          g_rec_eq_other_ft = NULL;
          r2 = rf2;
          r2_own = rf2;
        }
      }
      s->n_rules    = n_rules0;
      s->n_unorient = n_unor0;

      int hit = atp_cone_pair_eq_ft(arena, l2, r2, el, er) ||
                atp_cone_pair_eq_ft(arena, l2, r2, er, el);
      if (hit && chain != p_a && s->n_trace > mark &&
          s->n_trace < s->t_max) {
        // Re-thread: the entry's parent becomes the chain tail.  The
        // WL resolver's ORIENT/SIMPLIFY branch then inherits the
        // NORM_STEP chain and never BFS-bridges this entry.  Children
        // 1..3 (parent_b slot / stored pair) stay as-is; nothing cites
        // this entry's parent slot structurally.
        u32 n = term_ctr_n(e);
        if (n >= 4u && n <= 8u) {
          Term ch[8];
          for (u32 q = 0; q < n; q++) ch[q] = term_ctr_at(e, q);
          ch[0] = term_new(0, TAG_NUM, 0, chain);
          s->trace[ti] = term_new_ctr(reason, ch, n);
          if (ti >= ATP_MAX_TRACE) s->trace_x_heap = 1u;
          success = 1;
          n_fixed++;
        } else {
          s->n_trace = mark;
        }
      } else {
        s->n_trace = mark;
      }
      if (dbg && (success || a_i + 1u == n_att)) {
        fprintf(stderr, "[cone] ti=%u reason=%u pa=%u k=%u att=%u "
                "(lhs=%u doE=%u inclp=%u) %s steps=%u\n",
                ti, reason, p_a, k, a_i,
                att_lhs[a_i], att_doE[a_i], att_inc[a_i],
                success ? "FIXED" : "miss",
                success ? (s->n_trace - mark) : 0u);
      }
      if (l2_own != NULL) ft_free_span(arena, l2_own, l2_own->end);
      if (r2_own != NULL) ft_free_span(arena, r2_own, r2_own->end);
    }
    atp_cone_free_pair(arena, el, er);
    atp_cone_free_pair(arena, pl, pr);
  }
  if (dbg) {
    fprintf(stderr, "[cone] fixed=%u/%u n_trace %u -> %u\n",
            n_fixed, n_cand, n0_trace, s->n_trace);
  }
  if (arena != NULL) {
    for (u32 b = 0; b < n_births; b++) {
      if (birth_state[b] == 1u)
        atp_cone_free_pair(arena, birth_lft[b], birth_rft[b]);
    }
  }
  free(birth_ti); free(birth_dead); free(birth_state); free(birth_or);
  free(birth_lft); free(birth_rft); free(cand);
#undef ATP_CONE_MAYBE_GC
#else
  (void)s;
#endif
}


// === Emission-time treatment re-derivation ===========================
//
// A --proof search records no treatment steps: the treatment recorder
// (record_norm_steps) and the unpacked trace both changed the search
// they were meant to explain.  The emitter asks instead, for each
// printed ORIENT / FVI / SIMPLIFY record whose pair is not its
// parent's: thvm_atp_rederive replays the engine's normalizers over the
// historical rule window R(ti) of the goal-cone model above and returns
// the rewrite chain parent -> record as fresh Terms.  The trace and the
// live rule set are left as they were.  Rungs, in the order a record's
// pair most likely came to differ from its parent's:
//   pop normalize over R(ti), both sides (doR+doE, then doR only --
//     the Otter adoption and the WM demote drain are rules-only);
//   the formation treatment (doR over R(p_a)) then the pop normalize;
//   the same shapes Term-side (leftmost-outermost, oriented rules first
//     at each position) for the non-mixmost normalizers;
//   SIMPLIFY: the victim without its own rule, or its RHS composed with
//     the rule still live.
// A rung counts only when its last pair IS the record's pair (either
// orientation, up to renaming; an FVI record may be a grounded instance
// of it), so a returned chain is a real derivation -- a missing one
// only costs the emitter's fallbacks.

static int atp_rd_late(void);

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
typedef struct {
  const AtpState *s;
  u32   n_trace;          // trace length the index describes
  u32   n_births;
  u32  *birth_ti;         // ORIENT / FVI entries, ascending
  u32  *birth_dead;       // first SIMPLIFY citing the birth, or NONE
  u8   *birth_state;      // 0 unloaded / 1 spans loaded / 2 no pair
  u8   *birth_or;
  AtpFtCell **birth_lft;
  AtpFtCell **birth_rft;
  u64  *birth_lmask;      // face symbol masks (atp_cone_span_mask)
  u64  *birth_rmask;
} AtpRdHist;
static AtpRdHist g_rd_hist;

static void atp_rd_hist_drop(void) {
  AtpRdHist *h = &g_rd_hist;
  if (h->s != NULL && h->birth_state != NULL && h->s->ft_arena_ptr != NULL) {
    AtpFt *a = (AtpFt *)h->s->ft_arena_ptr;
    for (u32 b = 0; b < h->n_births; b++) {
      if (h->birth_state[b] == 1u)
        atp_cone_free_pair(a, h->birth_lft[b], h->birth_rft[b]);
    }
  }
  free(h->birth_ti); free(h->birth_dead); free(h->birth_state);
  free(h->birth_or); free(h->birth_lft); free(h->birth_rft);
  free(h->birth_lmask); free(h->birth_rmask);
  memset(h, 0, sizeof *h);
  g_cone_birth_lmask = NULL;
  g_cone_birth_rmask = NULL;
}

// A rule slot is born by an ORIENT / FVI record, and REBORN in place by
// a SIMPLIFY record when interreduction rewrote a live rule without
// removing it (RHS composition): such a SIMPLIFY is the slot's current
// record -- still live at the end, or itself simplified later.  A
// SIMPLIFY whose victim left R (requeued) is no rule.
static u8 *atp_rd_rebirths(AtpState *s) {
  u8 *rb = (u8 *)calloc(s->n_trace ? s->n_trace : 1u, 1u);
  if (rb == NULL) return NULL;
  for (u32 i = 0; i < s->n_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR || term_ext(e) != TRACE_SIMPLIFY) continue;
    u32 p_a = (u32)term_val(term_ctr_at(e, 0));
    if (p_a == ATP_TRACE_NONE || p_a >= i) continue;
    Term pe = s->trace[p_a];
    if (term_tag(pe) == TAG_CTR && term_ext(pe) == TRACE_SIMPLIFY)
      rb[p_a] = 1u;
  }
  for (u32 k = 0; k < s->n_rules; k++) {
    u32 t = s->r_trace[k];
    if (t == ATP_TRACE_NONE || t >= s->n_trace || s->r_dead[k]) continue;
    Term e = s->trace[t];
    if (term_tag(e) == TAG_CTR && term_ext(e) == TRACE_SIMPLIFY) rb[t] = 1u;
  }
  return rb;
}

static int atp_rd_hist_ensure(AtpState *s) {
  AtpRdHist *h = &g_rd_hist;
  if (h->s == s && h->n_trace == s->n_trace && h->birth_ti != NULL) return 1;
  atp_rd_hist_drop();
  u8 *reborn = atp_rd_rebirths(s);
  if (reborn == NULL) return 0;
#define ATP_RD_IS_BIRTH(I, REASON)                                            \
  ((REASON) == TRACE_ORIENT || (REASON) == TRACE_FVI ||                       \
   ((REASON) == TRACE_SIMPLIFY && reborn[(I)]))
  u32 nb = 0u;
  for (u32 i = 0; i < s->n_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR) continue;
    u32 reason = term_ext(e);
    if (ATP_RD_IS_BIRTH(i, reason)) nb++;
  }
  if (nb == 0u) { free(reborn); return 0; }
  h->birth_ti    = (u32 *)malloc((size_t)nb * sizeof(u32));
  h->birth_dead  = (u32 *)malloc((size_t)nb * sizeof(u32));
  h->birth_state = (u8 *)calloc(nb, 1u);
  h->birth_or    = (u8 *)calloc(nb, 1u);
  h->birth_lft   = (AtpFtCell **)calloc(nb, sizeof(AtpFtCell *));
  h->birth_rft   = (AtpFtCell **)calloc(nb, sizeof(AtpFtCell *));
  h->birth_lmask = (u64 *)calloc(nb, sizeof(u64));
  h->birth_rmask = (u64 *)calloc(nb, sizeof(u64));
  if (h->birth_ti == NULL || h->birth_dead == NULL || h->birth_state == NULL ||
      h->birth_or == NULL || h->birth_lft == NULL || h->birth_rft == NULL ||
      h->birth_lmask == NULL || h->birth_rmask == NULL) {
    free(reborn);
    atp_rd_hist_drop();
    return 0;
  }
  u32 w = 0u;
  for (u32 i = 0; i < s->n_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR) continue;
    u32 reason = term_ext(e);
    if (!ATP_RD_IS_BIRTH(i, reason)) continue;
    h->birth_ti[w]   = i;
    h->birth_dead[w] = ATP_TRACE_NONE;
    w++;
  }
#undef ATP_RD_IS_BIRTH
  free(reborn);
  for (u32 i = 0; i < s->n_trace; i++) {
    Term e = s->trace[i];
    if (term_tag(e) != TAG_CTR || term_ext(e) != TRACE_SIMPLIFY) continue;
    u32 p_a = (u32)term_val(term_ctr_at(e, 0));
    if (p_a == ATP_TRACE_NONE) continue;
    u32 lo = 0u, hi = nb;
    while (lo < hi) {
      u32 mid = lo + (hi - lo) / 2u;
      if (h->birth_ti[mid] < p_a) lo = mid + 1u; else hi = mid;
    }
    if (lo < nb && h->birth_ti[lo] == p_a &&
        h->birth_dead[lo] == ATP_TRACE_NONE) {
      h->birth_dead[lo] = i;
    }
  }
  h->s        = s;
  h->n_trace  = s->n_trace;
  h->n_births = nb;
  return 1;
}

// Lands: the replayed pair IS the record's (either orientation, up to
// renaming), or -- an FVI record -- a substitution instance of it.
static int atp_rd_lands(u32 reason, Term fl, Term fr, Term el, Term er) {
  if (atp_cone_pair_eq(fl, fr, el, er) || atp_cone_pair_eq(fl, fr, er, el))
    return 1;
  if (reason != TRACE_FVI) return 0;
  RewriteSubst sub;
  memset(&sub, 0, sizeof sub);
  if (thvm_match(fl, el, &sub) && thvm_match(fr, er, &sub)) return 1;
  memset(&sub, 0, sizeof sub);
  return (thvm_match(fl, er, &sub) && thvm_match(fr, el, &sub)) ? 1 : 0;
}

#define ATP_RD_PSTEPS 256u

// Mixmost replay of one phase: normalize the sides in `sides` (bit 0
// lhs, bit 1 rhs) of (*cl, *cr) over R(tw), appending one out[] step per
// splice.  Returns 0 when the window is empty, the chain overflows, or
// a step's rule has no trace id.
static int atp_rd_phase_ft(AtpState *s, Term *cl, Term *cr, u32 tw,
                           u8 doE, u8 incl_parent, u32 p_a, u8 sides,
                           AtpRederiveStep *out, u32 cap, u32 *n_out) {
  AtpRdHist *h = &g_rd_hist;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  const u32 n_rules0 = s->n_rules;
  const u32 n_unor0  = s->n_unorient;
  u32 win_unor = 0u;
  u32 k = atp_cone_install_window(s, n_rules0, h->birth_ti, h->birth_dead,
                                  h->birth_state, h->birth_or,
                                  h->birth_lft, h->birth_rft, h->n_births,
                                  tw, incl_parent, p_a, &win_unor);
  if (k == 0u) return 0;
  s->n_rules    = n_rules0 + k;
  s->n_unorient = n_unor0 + win_unor;
  static AtpProofStep ps[ATP_RD_PSTEPS];
  int ok = 1;
  for (u8 side = 0u; side < 2u && ok; side++) {
    if (!(sides & (1u << side))) continue;
    u32 pn = 0u;
    AtpFtCell *t = ft_from_term(a, side ? *cr : *cl, 0);
    ft_clear_subst_fresh(t);
    AtpFtSubst subst;
    memset(&subst, 0, sizeof(subst));
    AtpFtCell *nf = atp_normalize_mixmost_ft(s, t, n_rules0, n_rules0 + k,
                                             64u, doE, 0, (Term)0, side,
                                             NULL, &subst, ps, &pn,
                                             ATP_RD_PSTEPS);
    Term nft = ft_to_term(nf);
    ft_free_span(a, nf, nf->end);
    if (pn >= ATP_RD_PSTEPS) { ok = 0; break; }
    for (u32 j = 0; j < pn; j++) {
      u32 eq = (ps[j].rule < s->n_rules) ? s->r_trace[ps[j].rule]
                                         : ATP_TRACE_NONE;
      if (eq == ATP_TRACE_NONE || *n_out >= cap) { ok = 0; break; }
      if (side) *cr = ps[j].after; else *cl = ps[j].after;
      out[*n_out].l  = *cl;
      out[*n_out].r  = *cr;
      out[*n_out].eq = eq;
      (*n_out)++;
    }
    if (ok && !kbo_eq(side ? *cr : *cl, nft)) ok = 0;   // log incomplete
  }
  s->n_rules    = n_rules0;
  s->n_unorient = n_unor0;
  return ok;
}

// Term-side replay of one phase: leftmost-outermost single steps over
// R(tw) (oriented rules first at each position, then order-gated
// unorientable faces -- the atp_proof_rewrite_step_slice pick), after an
// oriented-only fixpoint when `oriented_first` (the record-mode shape of
// the non-mixmost pop normalize).
static int atp_rd_phase_term(AtpState *s, Term *cl, Term *cr, u32 tw,
                             u8 doE, u8 incl_parent, u32 p_a, u8 sides,
                             u8 oriented_first,
                             AtpRederiveStep *out, u32 cap, u32 *n_out) {
  AtpRdHist *h = &g_rd_hist;
  u32 nwin = 0u;
  for (u32 b = 0; b < h->n_births && h->birth_ti[b] < tw; b++) nwin++;
  if (nwin == 0u) return 0;
  Term *wl = (Term *)malloc((size_t)nwin * sizeof(Term));
  Term *wr = (Term *)malloc((size_t)nwin * sizeof(Term));
  u32  *wt = (u32 *)malloc((size_t)nwin * sizeof(u32));
  u8   *wo = (u8 *)malloc((size_t)nwin);
  if (wl == NULL || wr == NULL || wt == NULL || wo == NULL) {
    free(wl); free(wr); free(wt); free(wo);
    return 0;
  }
  u32 k = 0u;
  for (u32 b = 0; b < h->n_births; b++) {
    if (h->birth_ti[b] >= tw) break;
    if (h->birth_dead[b] != ATP_TRACE_NONE && h->birth_dead[b] < tw) continue;
    if (!incl_parent && h->birth_ti[b] == p_a) continue;
    Term l = 0, r = 0;
    if (!atp_cone_eq(s, h->birth_ti[b], &l, &r)) continue;
    wl[k] = l;
    wr[k] = r;
    wt[k] = h->birth_ti[b];
    wo[k] = (u8)(atp_compare(s, l, r) == KBO_GT);
    k++;
  }
  // Oriented subset first (the oriented-only fixpoint phase).
  u32 ko = 0u;
  Term *ol = (Term *)malloc((size_t)(k ? k : 1u) * sizeof(Term));
  Term *orr = (Term *)malloc((size_t)(k ? k : 1u) * sizeof(Term));
  u32  *ot = (u32 *)malloc((size_t)(k ? k : 1u) * sizeof(u32));
  int ok = (ol != NULL && orr != NULL && ot != NULL && k > 0u);
  if (ok) {
    for (u32 i = 0; i < k; i++) {
      if (!wo[i]) continue;
      ol[ko] = wl[i]; orr[ko] = wr[i]; ot[ko] = wt[i]; ko++;
    }
  }
  for (u8 side = 0u; side < 2u && ok; side++) {
    if (!(sides & (1u << side))) continue;
    for (u8 ph = oriented_first ? 0u : 1u; ph < 2u && ok; ph++) {
      if (ph == 1u && !doE && oriented_first) break;
      const Term *al = (ph == 0u || !doE) ? ol : wl;
      const Term *ar = (ph == 0u || !doE) ? orr : wr;
      const u32 *at = (ph == 0u || !doE) ? ot : wt;
      u32 an = (ph == 0u || !doE) ? ko : k;
      for (u32 it = 0; it < 64u; it++) {
        u8  pos[ATP_PROOF_MAX_DEPTH];
        u32 rule = 0u;
        u8  pos_len = 0u, fwd = 1u, fired = 0u;
        Term t = side ? *cr : *cl;
        Term t2 = atp_proof_rewrite_step_slice(s, t, pos, 0u, al, ar, an,
                                               &rule, &pos_len, &fwd,
                                               &fired);
        if (!fired || kbo_eq(t, t2)) break;
        if (*n_out >= cap || rule >= an) { ok = 0; break; }
        if (side) *cr = t2; else *cl = t2;
        out[*n_out].l  = *cl;
        out[*n_out].r  = *cr;
        out[*n_out].eq = at[rule];
        (*n_out)++;
      }
    }
  }
  free(ol); free(orr); free(ot);
  free(wl); free(wr); free(wt); free(wo);
  return ok;
}

// Normalize ONE term over R(tw) (mixmost), returning its NF and the
// intermediate terms + cited equations in st_t[] / st_eq[] (st_t[j] is
// the term after step j).  0 on an empty window, a step without a trace
// id, or overflow.
static int atp_rd_nf_ft(AtpState *s, Term t, u32 tw, u8 doE, u8 incl_parent,
                        u32 p_a, Term *nf_out, Term *st_t, u32 *st_eq,
                        u32 cap, u32 *n_out) {
  AtpRdHist *h = &g_rd_hist;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  const u32 n_rules0 = s->n_rules;
  const u32 n_unor0  = s->n_unorient;
  u32 win_unor = 0u;
  u32 k = atp_cone_install_window(s, n_rules0, h->birth_ti, h->birth_dead,
                                  h->birth_state, h->birth_or,
                                  h->birth_lft, h->birth_rft, h->n_births,
                                  tw, incl_parent, p_a, &win_unor);
  if (k == 0u) return 0;
  s->n_rules    = n_rules0 + k;
  s->n_unorient = n_unor0 + win_unor;
  static AtpProofStep ps[ATP_RD_PSTEPS];
  u32 pn = 0u;
  AtpFtCell *ft = ft_from_term(a, t, 0);
  ft_clear_subst_fresh(ft);
  AtpFtSubst subst;
  memset(&subst, 0, sizeof(subst));
  AtpFtCell *nf = atp_normalize_mixmost_ft(s, ft, n_rules0, n_rules0 + k,
                                           64u, doE, 0, (Term)0, 0u, NULL,
                                           &subst, ps, &pn, ATP_RD_PSTEPS);
  *nf_out = ft_to_term(nf);
  ft_free_span(a, nf, nf->end);
  int ok = (pn < ATP_RD_PSTEPS && pn <= cap);
  for (u32 j = 0; ok && j < pn; j++) {
    u32 eq = (ps[j].rule < s->n_rules) ? s->r_trace[ps[j].rule]
                                       : ATP_TRACE_NONE;
    if (eq == ATP_TRACE_NONE) { ok = 0; break; }
    st_t[j]  = ps[j].after;
    st_eq[j] = eq;
  }
  *n_out = ok ? pn : 0u;
  s->n_rules    = n_rules0;
  s->n_unorient = n_unor0;
  return ok;
}

// Meet in the middle: normalize the parent's sides AND the record's
// sides over the same window; when the two pairs meet at one normal-form
// pair (up to renaming), the chain runs forward from the parent to the
// normal forms and back to the record, each record-side step read in
// reverse (a rewrite by the same equation).  Robust to a window holding
// more rules than the engine had (retired passive demodulators, rules
// subsumed away): extra rules normalize both ends alike.  The record is
// renamed apart first and its side mapped onto the parent's variables
// at the meeting point, so every chain pair is one equation.
static u32 atp_rd_meet(AtpState *s, Term pl, Term pr, Term el, Term er,
                       u32 ti, u32 p_a, u8 doE, AtpRederiveStep *out,
                       u32 cap) {
  enum { M = ATP_RD_PSTEPS };
  static Term ta[4][M];
  static u32  qa[4][M];
  u32 na[4];
  for (u32 swap = 0; swap < 2u; swap++) {
    if (atp_rd_late()) return 0u;
    Term xl = thvm_rename_vars(swap ? er : el, CP_RENAME_OFFSET);
    Term xr = thvm_rename_vars(swap ? el : er, CP_RENAME_OFFSET);
    Term src[4] = { pl, pr, xl, xr };
    Term nf4[4];
    int ok = 1;
    for (u32 i = 0; i < 4u && ok; i++)
      ok = !atp_rd_late() &&
           atp_rd_nf_ft(s, src[i], ti, doE, 0u, p_a, &nf4[i], ta[i], qa[i],
                        M, &na[i]);
    if (!ok) continue;
    u32 total = na[0] + na[1] + na[2] + na[3];
    if (total == 0u || total > cap) continue;
    // Record normal forms onto the parent's: a variable renaming only.
    RewriteSubst rho;
    memset(&rho, 0, sizeof rho);
    if (!thvm_match(nf4[2], nf4[0], &rho) || !thvm_match(nf4[3], nf4[1], &rho))
      continue;
    int renaming = 1;
    for (u32 v = 0; v < REWRITE_MAX_VAR && renaming; v++) {
      Term b = rho.bindings[v];
      if (b == 0) continue;
      if (term_tag(b) != TAG_FVR) { renaming = 0; break; }
      for (u32 w = v + 1u; w < REWRITE_MAX_VAR; w++)
        if (rho.bindings[w] != 0 && kbo_eq(rho.bindings[w], b)) {
          renaming = 0;
          break;
        }
    }
    if (!renaming) continue;
    u32 n = 0u;
    Term cl = pl, cr = pr;
    for (u32 j = 0; j < na[0]; j++) {                // parent lhs -> NF
      cl = ta[0][j];
      out[n].l = cl; out[n].r = cr; out[n].eq = qa[0][j]; n++;
    }
    for (u32 j = 0; j < na[1]; j++) {                // parent rhs -> NF
      cr = ta[1][j];
      out[n].l = cl; out[n].r = cr; out[n].eq = qa[1][j]; n++;
    }
    for (u32 j = na[2]; j-- > 0u; ) {                // NF -> record lhs
      cl = thvm_subst_apply((j == 0u) ? xl : ta[2][j - 1u], &rho);
      out[n].l = cl; out[n].r = cr; out[n].eq = qa[2][j]; n++;
    }
    for (u32 j = na[3]; j-- > 0u; ) {                // NF -> record rhs
      cr = thvm_subst_apply((j == 0u) ? xr : ta[3][j - 1u], &rho);
      out[n].l = cl; out[n].r = cr; out[n].eq = qa[3][j]; n++;
    }
    return n;
  }
  return 0u;
}

#endif

// Emission deadline (wall us, 0 = none): past it the replays stop and
// the emitter's cheaper fallbacks take the remaining records, so a slow
// re-derivation costs proof quality, never the run's status.
static u64 g_atp_rd_deadline_us = 0u;
fn void thvm_atp_rederive_deadline(u64 wall_us) { g_atp_rd_deadline_us = wall_us; }
// One record's replays get at most this long (a record no rung can
// reach would otherwise sweep every window at full size).
#define ATP_RD_CALL_US 1500000ull
static u64 g_atp_rd_call_deadline_us = 0u;
static int atp_rd_late(void) {
  u64 now = atp_now_us();
  if (g_atp_rd_deadline_us != 0u && now >= g_atp_rd_deadline_us) return 1;
  return g_atp_rd_call_deadline_us != 0u && now >= g_atp_rd_call_deadline_us;
}

// THVM_ATP_RD_DEBUG=1: one line per re-derivation (rung 0 = miss;
// 1+r mixmost-or-term rung r of the first pass, 101+r second pass,
// 1000+k meet).
static u64 g_atp_rd_t0 = 0u;
static void atp_rd_note(u32 ti, u32 reason, u32 rung, u32 n) {
  static int dbg = -1;
  if (dbg < 0) dbg = atp_env_on("THVM_ATP_RD_DEBUG");
  if (dbg)
    fprintf(stderr, "[rederive] ti=%u reason=%u rung=%u steps=%u us=%llu\n",
            ti, reason, rung, n,
            (unsigned long long)(atp_now_us() - g_atp_rd_t0));
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// The replay ladder, then the meet-in-the-middle, over the windows the
// current filter (g_cone_win_mask) admits.  Returns the chain length or 0.
static u32 atp_rd_ladder(AtpState *s, u32 ti, u32 reason, u32 p_a,
                         Term pl, Term pr, Term el, Term er,
                         AtpRederiveStep *out, u32 cap, u32 tag) {
  // Rung table: {ft, doE, incl_parent, sides, two-phase, oriented_first}.
  typedef struct { u8 ft, doE, inc, sides, two, ofirst; } Rung;
  static const Rung orient_rungs[] = {
    {1, 1, 0, 3, 0, 0}, {1, 0, 0, 3, 0, 0},
    {1, 1, 0, 3, 1, 0}, {1, 0, 0, 3, 1, 0},
    {0, 1, 0, 3, 0, 1}, {0, 1, 0, 3, 0, 0}, {0, 0, 0, 3, 0, 0},
    {0, 1, 0, 3, 1, 1},
    {1, 1, 1, 2, 0, 0}, {1, 0, 1, 2, 0, 0},
  };
  static const Rung simp_rungs[] = {
    {1, 0, 0, 3, 0, 0}, {1, 1, 0, 3, 0, 0},
    {1, 1, 1, 2, 0, 0}, {1, 0, 1, 2, 0, 0},
    {0, 0, 0, 3, 0, 0}, {0, 1, 0, 3, 0, 1},
    {0, 1, 1, 2, 0, 0}, {0, 0, 1, 2, 0, 0},
  };
  const Rung *rungs = (reason == TRACE_SIMPLIFY) ? simp_rungs : orient_rungs;
  u32 n_rungs = (reason == TRACE_SIMPLIFY)
      ? (u32)(sizeof simp_rungs / sizeof simp_rungs[0])
      : (u32)(sizeof orient_rungs / sizeof orient_rungs[0]);
  // The engine's own normalizer first: the mixmost rungs for a mixmost
  // run, the Term-side rungs otherwise.
  u8 first_ft = s->use_mixmost_nf ? 1u : 0u;
  for (u32 pass = 0; pass < 2u; pass++) {
  for (u32 ri = 0; ri < n_rungs; ri++) {
    const Rung *rg = &rungs[ri];
    if ((rg->ft == first_ft) != (pass == 0u)) continue;
    if (atp_rd_late()) return 0u;
    u64 hp = thvm_atp_heap_checkpoint();
    u64 g0 = gc_count();
    u32 n = 0u;
    Term cl = pl, cr = pr;
    int ok;
    if (rg->ft) {
      ok = 1;
      if (rg->two)   // formation-time treatment first (doR over R(p_a))
        ok = atp_rd_phase_ft(s, &cl, &cr, p_a, 0u, 1u, p_a, rg->sides,
                             out, cap, &n);
      if (ok)
        ok = atp_rd_phase_ft(s, &cl, &cr, ti, rg->doE, rg->inc, p_a,
                             rg->sides, out, cap, &n);
    } else {
      ok = 1;
      if (rg->two)
        ok = atp_rd_phase_term(s, &cl, &cr, p_a, 0u, 1u, p_a, rg->sides,
                               1u, out, cap, &n);
      if (ok)
        ok = atp_rd_phase_term(s, &cl, &cr, ti, rg->doE, rg->inc, p_a,
                               rg->sides, rg->ofirst, out, cap, &n);
    }
    if (ok && n > 0u && atp_rd_lands(reason, cl, cr, el, er)) {
      atp_rd_note(ti, reason, tag + 1u + pass * 100u + ri, n);
      return n;
    }
    {
      static int dbg2 = -1;
      if (dbg2 < 0) {
        const char *e = getenv("THVM_ATP_RD_DEBUG");
        dbg2 = (e != NULL && e[0] == '2') ? 1 : 0;
      }
      if (dbg2) {
        fprintf(stderr, "[rd-miss] ti=%u rung=%u ok=%d n=%u got: ", ti,
                tag + 1u + pass * 100u + ri, ok, n);
        atp_dbg_print_term(stderr, cl);
        fputs(" = ", stderr);
        atp_dbg_print_term(stderr, cr);
        fputs("  want: ", stderr);
        atp_dbg_print_term(stderr, el);
        fputs(" = ", stderr);
        atp_dbg_print_term(stderr, er);
        fputs("  from: ", stderr);
        atp_dbg_print_term(stderr, pl);
        fputs(" = ", stderr);
        atp_dbg_print_term(stderr, pr);
        fputc('\n', stderr);
        for (u32 j = 0; j < n; j++) {
          Term ql = 0, qr = 0;
          fprintf(stderr, "   step %u eq=t%u: ", j, out[j].eq);
          if (atp_cone_eq(s, out[j].eq, &ql, &qr)) {
            atp_dbg_print_term(stderr, ql);
            fputs(" = ", stderr);
            atp_dbg_print_term(stderr, qr);
            Term te = s->trace[out[j].eq];
            fprintf(stderr, "  (reason %u parent t%u)",
                    term_ext(te), (u32)term_val(term_ctr_at(te, 0)));
          }
          fputc('\n', stderr);
        }
      }
    }
    if (gc_count() == g0) thvm_atp_heap_reset(hp);
  }
  }
  if (reason != TRACE_FVI) {
    for (u32 k = 0; k < 2u; k++) {             // doR+doE, then doR
      u64 hp = thvm_atp_heap_checkpoint();
      u64 g0 = gc_count();
      if (atp_rd_late()) return 0u;
      u32 n = atp_rd_meet(s, pl, pr, el, er, ti, p_a, (u8)(1u - k), out,
                          cap);
      if (n > 0u) {
        atp_rd_note(ti, reason, tag + 1000u + k, n);
        return n;
      }
      if (gc_count() == g0) thvm_atp_heap_reset(hp);
    }
  }
  return 0u;
}
#endif

fn u32 thvm_atp_rederive(AtpState *s, u32 ti, AtpRederiveStep *out,
                         u32 cap) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (s == NULL || s->trace == NULL || ti >= s->n_trace || cap == 0u)
    return 0u;
  if (s->ft_arena_ptr == NULL) return 0u;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) return 0u;
  u32 reason = term_ext(e);
  if (reason != TRACE_ORIENT && reason != TRACE_FVI &&
      reason != TRACE_SIMPLIFY)
    return 0u;
  u32 p_a = (u32)term_val(term_ctr_at(e, 0));
  if (p_a == ATP_TRACE_NONE || p_a >= ti) return 0u;
  g_atp_rd_t0 = atp_now_us();
  g_atp_rd_call_deadline_us = g_atp_rd_t0 + ATP_RD_CALL_US;
  // Headroom: a failed rung rolls its Terms back, a found chain stays
  // live until the emitter prints it.
  if (gc_from_end() - HEAP_NEXT < (u64)(4u << 20)) return 0u;
  Term el = 0, er = 0, pl = 0, pr = 0;
  if (!atp_cone_eq(s, ti, &el, &er) || !atp_cone_eq(s, p_a, &pl, &pr))
    return 0u;
  if (atp_cone_pair_eq(pl, pr, el, er) || atp_cone_pair_eq(pl, pr, er, el))
    return 0u;   // no gap
  if (!atp_rd_hist_ensure(s)) return 0u;
  atp_ft_mirror_ensure(s);
  // Fast pass: windows filtered to the rules whose faces use only the
  // endpoints' function symbols; then the full windows.
  u64 allowed = 0u;
  {
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    Term ends[4] = { pl, pr, el, er };
    for (u32 i = 0; i < 4u; i++) {
      AtpFtCell *c = ft_from_term(a, ends[i], 0);
      allowed |= atp_cone_span_mask(c);
      ft_free_span(a, c, c->end);
    }
  }
  g_cone_birth_lmask = g_rd_hist.birth_lmask;
  g_cone_birth_rmask = g_rd_hist.birth_rmask;
  for (u32 fp = 0; fp < 2u; fp++) {
    g_cone_win_mask = (fp == 0u) ? (allowed ? allowed : ~0ull) : 0u;
    u32 n = atp_rd_ladder(s, ti, reason, p_a, pl, pr, el, er, out, cap,
                          fp == 0u ? 5000u : 0u);
    g_cone_win_mask = 0u;
    if (n > 0u) return n;
    if (atp_rd_late()) return 0u;
  }
  // Last resort: the same attempts over WIDE windows (every earlier
  // birth, retired or not).
  g_cone_win_wide = 1u;
  for (u32 k = 0; k < 2u; k++) {
    if (atp_rd_late()) break;
    u64 hp = thvm_atp_heap_checkpoint();
    u64 g0 = gc_count();
    u32 n = 0u;
    Term cl = pl, cr = pr;
    int ok = atp_rd_phase_ft(s, &cl, &cr, ti, (u8)(1u - k), 0u, p_a, 3u,
                             out, cap, &n);
    if (ok && n > 0u && atp_rd_lands(reason, cl, cr, el, er)) {
      g_cone_win_wide = 0u;
      atp_rd_note(ti, reason, 2000u + k, n);
      return n;
    }
    if (reason != TRACE_FVI) {
      n = atp_rd_meet(s, pl, pr, el, er, ti, p_a, (u8)(1u - k), out, cap);
      if (n > 0u) {
        g_cone_win_wide = 0u;
        atp_rd_note(ti, reason, 2010u + k, n);
        return n;
      }
    }
    if (gc_count() == g0) thvm_atp_heap_reset(hp);
  }
  g_cone_win_wide = 0u;
  atp_rd_note(ti, reason, 0u, 0u);
  return 0u;
#else
  (void)s; (void)ti; (void)out; (void)cap;
  return 0u;
#endif
}

fn void thvm_atp_rederive_end(AtpState *s) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (g_rd_hist.s == s) atp_rd_hist_drop();
#else
  (void)s;
#endif
}
