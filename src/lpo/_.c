// thvm_lpo - Lexicographic Path Ordering on first-order terms.
//
// Compares two terms over a signature of TAG_CTR (function symbols)
// and TAG_FVR (variables) under an LpoConfig (per-symbol precedence
// table).  Stage 8.5 of the IC-native ATP roadmap; per the
// `docs/plans/lpo_design.md` decision memo.
//
// Returns:
//   LPO_EQ : s and t are structurally identical
//   LPO_GT : s > t under LPO
//   LPO_LT : s < t under LPO
//   LPO_UN : incomparable
//
// Algorithm (Dershowitz, "Orderings for term-rewriting systems",
// 1982).  For terms `s = f(s_1..s_m)` and `t = g(t_1..t_n)`:
//
//   s >_lpo t  iff  one of:
//     (1) s_i >=_lpo t  for some i in 1..m   (subterm domination)
//     (2) f >_F g       AND s >_lpo t_j  for all j in 1..n
//     (3) f == g        AND (s_1..s_m) >_lex_lpo (t_1..t_n)
//                       AND s >_lpo t_j  for all j in 1..n
//
// Variable cases:
//   x >_lpo t  iff false (no FVR can dominate any term)
//   s >_lpo x  iff x occurs as a strict subterm of s (and s != x)
//   x >_lpo x  is false (use LPO_EQ on identical FVR ids)
//
// No per-sort awareness in v0; same conservative single-sort
// treatment as KBO.

// === structural equality (local copy of kbo_eq's pattern) ============

static u8 lpo_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;  // same id (ext) already checked
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!lpo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// === variable subterm-occurrence check ==============================
// Returns 1 iff FVR `var` (with ext = `var_id`) occurs anywhere in `t`,
// strictly or otherwise.  The strict-subterm caller handles the equality
// case separately.

static u8 lpo_var_occurs_in(Term t, u32 var_id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (lpo_var_occurs_in(term_ctr_at(t, i), var_id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// === pretests (Waldmeister `LPOVortests`) ============================
// Fast pre-checks run before the full recursive descent.  Ported from
// Waldmeister's `LPOVortests` module ("Vortests" = pretests): the
// variable-set comparison ("Variablenmengenvergleich") that cheaply
// rules out incomparable pairs.
//
// Soundness: LPO is a simplification ordering, so `s >lpo t` requires
// every variable occurring in `t` to also occur in `s` (the standard
// LPO variable condition: a variable in t but not s could be
// instantiated to make t arbitrarily large).  Hence if neither
// vars(s) contains vars(t) nor vice versa, the terms are LPO_UN, and
// if one set is a strict superset only that direction can be GT/LT.
// This is a pure optimization: the full recursion reaches the same
// verdict, only slower.

#define LPO_MAX_VAR 64

// OR-accumulate a presence bit per variable id into `seen`.
static void lpo_var_set_acc(Term t, u32 *seen) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < LPO_MAX_VAR) seen[id] = 1;
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) lpo_var_set_acc(term_ctr_at(t, i), seen);
      return;
    }
    default: return;
  }
}

// True iff `var_id < LPO_MAX_VAR` for every FVR reachable in `t`.
// The pretest is only sound when both terms' variable ids are
// bitset-representable; out-of-range ids fall back to the full
// recursion.
static u8 lpo_vars_in_range(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) < LPO_MAX_VAR;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!lpo_vars_in_range(term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// Variable-set pretest.  Writes one of LPO_GT / LPO_LT / LPO_EQ /
// LPO_UN into `*out` and returns 1 iff the pretest is conclusive
// enough to skip the full comparison; returns 0 when the full
// recursion is still required.
//
// Returns conclusive 1 only when the verdict is forced:
//   - vars(s) and vars(t) incomparable  -> LPO_UN forced.
// Returns 0 (full recursion needed) for the equal / subset / superset
// cases, because set inclusion alone does not pin down GT vs UN; it
// only rules the opposite direction out, which the full recursion
// then resolves quickly.
static u8 lpo_pretest_varset(Term s, Term t, LpoCmp *out) {
  if (!lpo_vars_in_range(s) || !lpo_vars_in_range(t)) return 0;
  u32 vs[LPO_MAX_VAR] = {0};
  u32 vt[LPO_MAX_VAR] = {0};
  lpo_var_set_acc(s, vs);
  lpo_var_set_acc(t, vt);
  u8 s_has_extra = 0;   // some var in s not in t
  u8 t_has_extra = 0;   // some var in t not in s
  for (u32 i = 0; i < LPO_MAX_VAR; i++) {
    if (vs[i] && !vt[i]) s_has_extra = 1;
    if (vt[i] && !vs[i]) t_has_extra = 1;
  }
  if (s_has_extra && t_has_extra) {
    // Incomparable variable sets: neither s >lpo t nor t >lpo s can
    // hold, and s != t -- so the pair is definitively LPO_UN.
    *out = LPO_UN;
    return 1;
  }
  return 0;
}

// === main comparator =================================================

static LpoCmp lpo_rec(Term s, Term t, const LpoConfig *cfg);

// True iff every t_j satisfies s >_lpo t_j (case (2) and (3) condition).
static u8 lpo_dominates_all_args(Term s, Term t_parent,
                                 const LpoConfig *cfg) {
  if (term_tag(t_parent) != TAG_CTR) return 1;
  u32 n = term_ctr_n(t_parent);
  for (u32 j = 0; j < n; j++) {
    if (lpo_rec(s, term_ctr_at(t_parent, j), cfg) != LPO_GT) return 0;
  }
  return 1;
}

// True iff some s_i >=_lpo t (case (1) condition).
static u8 lpo_some_arg_dominates(Term s_parent, Term t,
                                 const LpoConfig *cfg) {
  if (term_tag(s_parent) != TAG_CTR) return 0;
  u32 m = term_ctr_n(s_parent);
  for (u32 i = 0; i < m; i++) {
    Term s_i = term_ctr_at(s_parent, i);
    LpoCmp c = lpo_rec(s_i, t, cfg);
    if (c == LPO_EQ || c == LPO_GT) return 1;
  }
  return 0;
}

// Lexicographic argument comparison: returns LPO_GT, LPO_LT, LPO_EQ
// (mirrors LpoCmp but limited to the three outcomes).  Assumes equal
// arity.
static LpoCmp lpo_lex(Term s_parent, Term t_parent,
                      const LpoConfig *cfg) {
  u32 n = term_ctr_n(s_parent);
  for (u32 i = 0; i < n; i++) {
    LpoCmp c = lpo_rec(term_ctr_at(s_parent, i),
                       term_ctr_at(t_parent, i), cfg);
    if (c != LPO_EQ) return c;
  }
  return LPO_EQ;
}

fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg) {
  if (lpo_eq(s, t)) return LPO_EQ;
  // Variable-set pretest (Waldmeister `LPOVortests`): a single
  // bitset pass over both terms rules out the incomparable-var-set
  // case before the recursive descent.  Conclusive only for the
  // forced LPO_UN verdict; otherwise the full recursion runs.
  LpoCmp pre;
  if (lpo_pretest_varset(s, t, &pre)) return pre;
  return lpo_rec(s, t, cfg);
}

static LpoCmp lpo_rec(Term s, Term t, const LpoConfig *cfg) {
  if (lpo_eq(s, t)) return LPO_EQ;

  u8 s_is_fvr = (term_tag(s) == TAG_FVR);
  u8 t_is_fvr = (term_tag(t) == TAG_FVR);

  // Variable cases.
  if (s_is_fvr && t_is_fvr) {
    // Distinct FVR ids: incomparable.
    return LPO_UN;
  }
  if (s_is_fvr) {
    // s is FVR, t is CTR.  s >_lpo t is false; check t >_lpo s
    // via subterm-occurrence: t > x iff x occurs in t.
    if (lpo_var_occurs_in(t, term_ext(s))) return LPO_LT;
    return LPO_UN;
  }
  if (t_is_fvr) {
    // s is CTR, t is FVR.  s >_lpo x iff x occurs strictly in s.
    if (lpo_var_occurs_in(s, term_ext(t))) return LPO_GT;
    return LPO_UN;
  }

  // Both are CTRs (or non-CTR/non-FVR which we treat as UN).
  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return LPO_UN;

  // Case (1) check both directions: subterm domination.
  if (lpo_some_arg_dominates(s, t, cfg)) return LPO_GT;
  if (lpo_some_arg_dominates(t, s, cfg)) return LPO_LT;

  // Cases (2) and (3): compare top symbols.
  u32 lab_s = term_ext(s);
  u32 lab_t = term_ext(t);
  u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0;
  u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0;

  if (prec_s > prec_t) {
    // f >_F g: case (2), s >_lpo t iff s >_lpo every t_j.
    if (lpo_dominates_all_args(s, t, cfg)) return LPO_GT;
    return LPO_UN;
  }
  if (prec_s < prec_t) {
    // g >_F f: symmetric.
    if (lpo_dominates_all_args(t, s, cfg)) return LPO_LT;
    return LPO_UN;
  }

  // Equal head symbols: case (3).  Lex compare args; the winning
  // direction must dominate the loser's args.
  if (term_ctr_n(s) != term_ctr_n(t)) return LPO_UN;   // arity mismatch
  LpoCmp lex = lpo_lex(s, t, cfg);
  if (lex == LPO_EQ) return LPO_EQ;   // can't happen if lpo_eq fired earlier
  if (lex == LPO_GT) {
    if (lpo_dominates_all_args(s, t, cfg)) return LPO_GT;
    return LPO_UN;
  }
  // lex == LPO_LT
  if (lpo_dominates_all_args(t, s, cfg)) return LPO_LT;
  return LPO_UN;
}
