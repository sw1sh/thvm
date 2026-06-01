// thvm_rpo - Recursive Path Ordering on first-order terms.
//
// Generalises LPO (lpo/_.c) with per-symbol status: each function symbol
// is either LEX (lexicographic argument comparison; LPO's behaviour) or
// MUL (multiset argument comparison).  Strict-precedence only -- no
// per-symbol weights (KBO covers the weighted case).  Reference:
// Dershowitz, "Orderings for term-rewriting systems" (1982).
//
// For s = f(s_1..s_m) and t = g(t_1..t_n):
//
//   s >_rpo t iff one of:
//     (1) s_i >=_rpo t for some i in 1..m  (subterm domination)
//     (2) f >_F g       AND s >_rpo t_j  for all j in 1..n
//     (3) f == g, equal arity, AND:
//           - status(f) = LEX: (s_1..s_m) >_lex_rpo (t_1..t_n)
//                              AND s >_rpo t_j  for all j
//           - status(f) = MUL: {s_1..s_m} >_mul_rpo {t_1..t_n}
//
// Variable cases identical to LPO.  Memo same shape as the LPO
// fallback path -- the flatrec / Vortest optimisations stay LPO-only
// in v0; RPO's heavy lever is the multiset extension.

// Local structural equality (independent of lpo_eq's epoch).
static u8 rpo_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!rpo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

static u8 rpo_var_occurs_in(Term t, u32 var_id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (rpo_var_occurs_in(term_ctr_at(t, i), var_id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// Memo: (s, t) -> RpoCmp, epoch-tagged.  Same shape as g_lpo_memo --
// keeps the per-call lifetime + lpo_eq-style structural identity.
#define RPO_MEMO_SIZE 4096u
#define RPO_MEMO_MASK (RPO_MEMO_SIZE - 1u)
typedef struct { Term s, t; u32 epoch; u8 cmp; } RpoMemoEnt;
static RpoMemoEnt g_rpo_memo[RPO_MEMO_SIZE];
static u32        g_rpo_epoch = 1u;

fn void thvm_rpo_invalidate(void) {
  if (++g_rpo_epoch == 0u) {
    for (u32 i = 0; i < RPO_MEMO_SIZE; i++) g_rpo_memo[i].epoch = 0u;
    g_rpo_epoch = 1u;
  }
}

static RpoCmp rpo_rec(Term s, Term t, const RpoConfig *cfg);
static RpoCmp rpo_rec_compute(Term s, Term t, const RpoConfig *cfg);

static RpoCmp rpo_rec(Term s, Term t, const RpoConfig *cfg) {
  u32 idx = (u32)((u64)s * 0x9E3779B97F4A7C15ull
                ^ (u64)t * 0xBF58476D1CE4E5B9ull) & RPO_MEMO_MASK;
  RpoMemoEnt *e = &g_rpo_memo[idx];
  if (e->epoch == g_rpo_epoch && e->s == s && e->t == t) {
    return (RpoCmp)(i8)e->cmp;
  }
  RpoCmp c = rpo_rec_compute(s, t, cfg);
  e->s = s; e->t = t; e->epoch = g_rpo_epoch; e->cmp = (u8)c;
  return c;
}

// Case (2) and (3) lex helper: every t_j satisfies s >_rpo t_j.
static u8 rpo_dominates_all_args(Term s, Term t_parent,
                                 const RpoConfig *cfg) {
  if (term_tag(t_parent) != TAG_CTR) return 1;
  u32 n = term_ctr_n(t_parent);
  for (u32 j = 0; j < n; j++) {
    if (rpo_rec(s, term_ctr_at(t_parent, j), cfg) != RPO_GT) return 0;
  }
  return 1;
}

// Case (1): some s_i >=_rpo t.
static u8 rpo_some_arg_dominates(Term s_parent, Term t,
                                 const RpoConfig *cfg) {
  if (term_tag(s_parent) != TAG_CTR) return 0;
  u32 m = term_ctr_n(s_parent);
  for (u32 i = 0; i < m; i++) {
    Term s_i = term_ctr_at(s_parent, i);
    RpoCmp c = rpo_rec(s_i, t, cfg);
    if (c == RPO_EQ || c == RPO_GT) return 1;
  }
  return 0;
}

// Lex argument comparison (same-arity, status(f) = LEX).
static RpoCmp rpo_lex(Term s_parent, Term t_parent, const RpoConfig *cfg) {
  u32 n = term_ctr_n(s_parent);
  for (u32 i = 0; i < n; i++) {
    RpoCmp c = rpo_rec(term_ctr_at(s_parent, i),
                       term_ctr_at(t_parent, i), cfg);
    if (c != RPO_EQ) return c;
  }
  return RPO_EQ;
}

// Multiset extension >_mul_rpo (Dershowitz; status(f) = MUL).
//
// Algorithm: pair up RPO-equal elements between the two argument lists
// (greedy first-fit; any pairing works since RPO_EQ is structural
// identity, an equivalence relation).  Then:
//   - If both sides fully paired: EQ.
//   - If only one side has leftovers: that side dominates (GT or LT).
//   - If both have leftovers: check if every t-leftover is dominated by
//     some s-leftover under >_rpo (GT); symmetric for LT.
//
// RPO_MAX_KIDS bounds: at term-CTR build sites the kid count is
// REWRITE_MAX_ARITY (= 8) in practice; cap here matches.
#define RPO_MAX_KIDS 16u

static RpoCmp rpo_multi(Term s_parent, Term t_parent, const RpoConfig *cfg) {
  if (term_tag(s_parent) != TAG_CTR || term_tag(t_parent) != TAG_CTR) {
    return RPO_UN;
  }
  u32 ns = term_ctr_n(s_parent);
  u32 nt = term_ctr_n(t_parent);
  if (ns > RPO_MAX_KIDS || nt > RPO_MAX_KIDS) return RPO_UN;

  Term S[RPO_MAX_KIDS], T[RPO_MAX_KIDS];
  u8   sUsed[RPO_MAX_KIDS] = {0}, tUsed[RPO_MAX_KIDS] = {0};
  for (u32 i = 0; i < ns; i++) S[i] = term_ctr_at(s_parent, i);
  for (u32 j = 0; j < nt; j++) T[j] = term_ctr_at(t_parent, j);

  // Phase 1: greedy multiset-equality cancellation.
  for (u32 i = 0; i < ns; i++) {
    if (sUsed[i]) continue;
    for (u32 j = 0; j < nt; j++) {
      if (tUsed[j]) continue;
      if (rpo_rec(S[i], T[j], cfg) == RPO_EQ) {
        sUsed[i] = 1u;
        tUsed[j] = 1u;
        break;
      }
    }
  }

  u32 sRem = 0, tRem = 0;
  for (u32 i = 0; i < ns; i++) if (!sUsed[i]) sRem++;
  for (u32 j = 0; j < nt; j++) if (!tUsed[j]) tRem++;

  if (sRem == 0u && tRem == 0u) return RPO_EQ;
  if (sRem >  0u && tRem == 0u) return RPO_GT;
  if (sRem == 0u && tRem >  0u) return RPO_LT;

  // Phase 2: both have leftovers.  GT iff every unused t_j is
  // strictly dominated by some unused s_i.
  u8 all_t_dominated = 1u;
  for (u32 j = 0; j < nt; j++) {
    if (tUsed[j]) continue;
    u8 found = 0u;
    for (u32 i = 0; i < ns; i++) {
      if (sUsed[i]) continue;
      if (rpo_rec(S[i], T[j], cfg) == RPO_GT) { found = 1u; break; }
    }
    if (!found) { all_t_dominated = 0u; break; }
  }
  if (all_t_dominated) return RPO_GT;

  u8 all_s_dominated = 1u;
  for (u32 i = 0; i < ns; i++) {
    if (sUsed[i]) continue;
    u8 found = 0u;
    for (u32 j = 0; j < nt; j++) {
      if (tUsed[j]) continue;
      if (rpo_rec(T[j], S[i], cfg) == RPO_GT) { found = 1u; break; }
    }
    if (!found) { all_s_dominated = 0u; break; }
  }
  if (all_s_dominated) return RPO_LT;

  return RPO_UN;
}

static u8 rpo_status_is_mul(const RpoConfig *cfg, u32 label) {
  if (cfg == NULL || cfg->status == NULL) return 0u;
  if (label >= cfg->n_labels) return 0u;
  return (cfg->status[label] == RPO_STATUS_MUL) ? 1u : 0u;
}

static RpoCmp rpo_rec_compute(Term s, Term t, const RpoConfig *cfg) {
  if (rpo_eq(s, t)) return RPO_EQ;

  u8 s_is_fvr = (term_tag(s) == TAG_FVR);
  u8 t_is_fvr = (term_tag(t) == TAG_FVR);

  if (s_is_fvr && t_is_fvr) return RPO_UN;  // distinct FVR ids
  if (s_is_fvr) {
    // s = x.  s >_rpo t is false; check t >_rpo x via subterm-occurrence.
    if (rpo_var_occurs_in(t, term_ext(s))) return RPO_LT;
    return RPO_UN;
  }
  if (t_is_fvr) {
    if (rpo_var_occurs_in(s, term_ext(t))) return RPO_GT;
    return RPO_UN;
  }

  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return RPO_UN;

  // (1) subterm domination.
  if (rpo_some_arg_dominates(s, t, cfg)) return RPO_GT;
  if (rpo_some_arg_dominates(t, s, cfg)) return RPO_LT;

  // (2) and (3) -- compare top symbols.
  u32 lab_s = term_ext(s);
  u32 lab_t = term_ext(t);
  u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0u;
  u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0u;

  if (prec_s > prec_t) {
    if (rpo_dominates_all_args(s, t, cfg)) return RPO_GT;
    return RPO_UN;
  }
  if (prec_s < prec_t) {
    if (rpo_dominates_all_args(t, s, cfg)) return RPO_LT;
    return RPO_UN;
  }

  // Equal head symbols: case (3).
  if (term_ctr_n(s) != term_ctr_n(t)) return RPO_UN;

  if (rpo_status_is_mul(cfg, lab_s)) {
    // Multiset status -- delegate to rpo_multi entirely.  Note: this
    // already enforces the "dominates_all_args" property for GT since
    // every leftover t_j is required to be RPO_GT-dominated by some
    // leftover s_i (which transitively gives RPO_GT vs t_j).
    return rpo_multi(s, t, cfg);
  }

  // Lex status.
  RpoCmp lex = rpo_lex(s, t, cfg);
  if (lex == RPO_EQ) return RPO_EQ;
  if (lex == RPO_GT) {
    if (rpo_dominates_all_args(s, t, cfg)) return RPO_GT;
    return RPO_UN;
  }
  if (lex == RPO_LT) {
    if (rpo_dominates_all_args(t, s, cfg)) return RPO_LT;
    return RPO_UN;
  }
  // lex == RPO_UN
  return RPO_UN;
}

fn RpoCmp thvm_rpo(Term s, Term t, const RpoConfig *cfg) {
  thvm_rpo_invalidate();
  return rpo_rec(s, t, cfg);
}
