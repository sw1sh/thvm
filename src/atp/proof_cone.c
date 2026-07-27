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
    if (birth_dead[b] != ATP_TRACE_NONE && birth_dead[b] < tw) continue;
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
      }
    }
    if (birth_state[b] != 1u) continue;
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

