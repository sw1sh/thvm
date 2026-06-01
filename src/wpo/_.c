// thvm_wpo - Weighted Path Ordering on first-order terms.
//
// Generalises KBO and RPO: assigns each term a non-negative integer
// weight via a KBO-style sum-of-symbol-weights and falls back to an
// RPO-style precedence + status comparison when the weights tie.
// Reference: Yamada-Kusakari-Sakabata, "A Unified Ordering for
// Termination Proving" (2014).  Strict-precedence + per-symbol status
// (LEX or MUL), plus integer weights -- a single ordering that
// subsumes KBO (precedence ties / pure weights), LPO (uniform weights +
// all-LEX) and RPO (uniform weights + mixed status).
//
// Variant: the "WPO_SUM" / "admissible" variant.  Function weights
// must be > 0 for unary symbols (the standard admissibility condition
// to keep WPO well-founded); constants and var_weight may be 0.  This
// implementation does not enforce admissibility -- it is the caller's
// problem to choose weights that make the ordering terminate.
//
// For s and t with weights w(s), w(t):
//
//   s >_wpo t iff
//     vars(t) is a subset of vars(s) (multiplicity-aware: standard
//                                      KBO variable-count condition)
//     AND  (w(s) > w(t)
//          OR  (w(s) == w(t)
//               AND s, t are CTRs of same head f, equal arity, and the
//                   status(f)-comparator returns GT
//          OR  (w(s) == w(t)
//               AND s = f(...), t = g(...), prec(f) > prec(g),
//                   AND s >_wpo every t_j)
//          OR  (w(s) == w(t)
//               AND s_i >=_wpo t for some i in 1..arity(s)))
//
// Symmetric for LT; UN otherwise.

#define WPO_MAX_VAR 64u
#define WPO_MAX_KIDS 16u
#define WPO_MEMO_SIZE 4096u
#define WPO_MEMO_MASK (WPO_MEMO_SIZE - 1u)

static u8 wpo_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!wpo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// Multiplicity-aware variable accumulator -- count occurrences of each
// FVR id.  WPO_MAX_VAR caps the range; out-of-range vars get encoded as
// "spilled" and force the comparison to UN as a safe fallback.
typedef struct {
  u32 count[WPO_MAX_VAR];
  u8  spilled;
} WpoVarMult;

static void wpo_var_mult_acc(Term t, WpoVarMult *vm) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id >= WPO_MAX_VAR) { vm->spilled = 1u; return; }
      vm->count[id]++;
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) wpo_var_mult_acc(term_ctr_at(t, i), vm);
      return;
    }
    default: return;
  }
}

// Returns 1 iff `s` dominates `t` on every variable id (multiplicity-
// aware).  Spilled IDs force conservative 0.
static u8 wpo_vars_subset(const WpoVarMult *vs, const WpoVarMult *vt) {
  if (vs->spilled || vt->spilled) return 0;
  for (u32 i = 0; i < WPO_MAX_VAR; i++) {
    if (vt->count[i] > vs->count[i]) return 0;
  }
  return 1;
}

// Compute total weight: Σ w_f over function symbols + var_weight * #var
// occurrences.  Identical shape to KBO's term-weight definition, so
// thvm_kbo_term_weight can be reused.
static long long wpo_weight(const WpoConfig *cfg, Term t) {
  KboConfig kbo = {
    .weights = cfg->weights,
    .precedence = cfg->precedence,
    .n_labels = cfg->n_labels,
    .var_weight = cfg->var_weight,
  };
  return thvm_kbo_term_weight(&kbo, t);
}

// === recursive comparator =============================================

typedef struct { Term s, t; u32 epoch; u8 cmp; } WpoMemoEnt;
static WpoMemoEnt g_wpo_memo[WPO_MEMO_SIZE];
static u32        g_wpo_epoch = 1u;

fn void thvm_wpo_invalidate(void) {
  if (++g_wpo_epoch == 0u) {
    for (u32 i = 0; i < WPO_MEMO_SIZE; i++) g_wpo_memo[i].epoch = 0u;
    g_wpo_epoch = 1u;
  }
}

static WpoCmp wpo_rec(Term s, Term t, const WpoConfig *cfg);

// Lex / Mul argument comparisons used at equal-head + equal-weight.

static WpoCmp wpo_lex(Term s_parent, Term t_parent, const WpoConfig *cfg) {
  u32 n = term_ctr_n(s_parent);
  for (u32 i = 0; i < n; i++) {
    WpoCmp c = wpo_rec(term_ctr_at(s_parent, i),
                       term_ctr_at(t_parent, i), cfg);
    if (c != WPO_EQ) return c;
  }
  return WPO_EQ;
}

static WpoCmp wpo_multi(Term s_parent, Term t_parent, const WpoConfig *cfg) {
  u32 ns = term_ctr_n(s_parent);
  u32 nt = term_ctr_n(t_parent);
  if (ns > WPO_MAX_KIDS || nt > WPO_MAX_KIDS) return WPO_UN;

  Term S[WPO_MAX_KIDS], T[WPO_MAX_KIDS];
  u8   sUsed[WPO_MAX_KIDS] = {0}, tUsed[WPO_MAX_KIDS] = {0};
  for (u32 i = 0; i < ns; i++) S[i] = term_ctr_at(s_parent, i);
  for (u32 j = 0; j < nt; j++) T[j] = term_ctr_at(t_parent, j);

  // Cancel WPO_EQ pairs (greedy first-fit).
  for (u32 i = 0; i < ns; i++) {
    if (sUsed[i]) continue;
    for (u32 j = 0; j < nt; j++) {
      if (tUsed[j]) continue;
      if (wpo_rec(S[i], T[j], cfg) == WPO_EQ) {
        sUsed[i] = 1u;
        tUsed[j] = 1u;
        break;
      }
    }
  }
  u32 sRem = 0u, tRem = 0u;
  for (u32 i = 0; i < ns; i++) if (!sUsed[i]) sRem++;
  for (u32 j = 0; j < nt; j++) if (!tUsed[j]) tRem++;
  if (sRem == 0u && tRem == 0u) return WPO_EQ;
  if (sRem >  0u && tRem == 0u) return WPO_GT;
  if (sRem == 0u && tRem >  0u) return WPO_LT;

  u8 all_t_dom = 1u;
  for (u32 j = 0; j < nt; j++) {
    if (tUsed[j]) continue;
    u8 found = 0u;
    for (u32 i = 0; i < ns; i++) {
      if (sUsed[i]) continue;
      if (wpo_rec(S[i], T[j], cfg) == WPO_GT) { found = 1u; break; }
    }
    if (!found) { all_t_dom = 0u; break; }
  }
  if (all_t_dom) return WPO_GT;

  u8 all_s_dom = 1u;
  for (u32 i = 0; i < ns; i++) {
    if (sUsed[i]) continue;
    u8 found = 0u;
    for (u32 j = 0; j < nt; j++) {
      if (tUsed[j]) continue;
      if (wpo_rec(T[j], S[i], cfg) == WPO_GT) { found = 1u; break; }
    }
    if (!found) { all_s_dom = 0u; break; }
  }
  if (all_s_dom) return WPO_LT;

  return WPO_UN;
}

static u8 wpo_status_is_mul(const WpoConfig *cfg, u32 label) {
  if (cfg == NULL || cfg->status == NULL) return 0u;
  if (label >= cfg->n_labels) return 0u;
  return (cfg->status[label] == WPO_STATUS_MUL) ? 1u : 0u;
}

static u8 wpo_dominates_all_args(Term s, Term t_parent,
                                 const WpoConfig *cfg) {
  if (term_tag(t_parent) != TAG_CTR) return 1u;
  u32 n = term_ctr_n(t_parent);
  for (u32 j = 0; j < n; j++) {
    if (wpo_rec(s, term_ctr_at(t_parent, j), cfg) != WPO_GT) return 0u;
  }
  return 1u;
}

static u8 wpo_some_arg_dominates(Term s_parent, Term t,
                                 const WpoConfig *cfg) {
  if (term_tag(s_parent) != TAG_CTR) return 0u;
  u32 m = term_ctr_n(s_parent);
  for (u32 i = 0; i < m; i++) {
    WpoCmp c = wpo_rec(term_ctr_at(s_parent, i), t, cfg);
    if (c == WPO_EQ || c == WPO_GT) return 1u;
  }
  return 0u;
}

static WpoCmp wpo_rec_compute(Term s, Term t, const WpoConfig *cfg) {
  if (wpo_eq(s, t)) return WPO_EQ;

  // Variable-multiplicity prerequisite.  Both directions evaluated up
  // front so we can short-circuit when neither holds.
  WpoVarMult vs = {{0}, 0u}, vt = {{0}, 0u};
  wpo_var_mult_acc(s, &vs);
  wpo_var_mult_acc(t, &vt);
  u8 s_dominates_t_vars = wpo_vars_subset(&vs, &vt);  // vars(t) ⊆ vars(s)
  u8 t_dominates_s_vars = wpo_vars_subset(&vt, &vs);  // vars(s) ⊆ vars(t)

  // Pure variable comparisons.
  u8 s_is_fvr = (term_tag(s) == TAG_FVR);
  u8 t_is_fvr = (term_tag(t) == TAG_FVR);
  if (s_is_fvr && t_is_fvr) return WPO_UN;
  if (s_is_fvr) {
    // s = x; s >_wpo t false; check t >_wpo x via subterm-occurrence.
    // (Var-multiplicity already gives the safe "must contain x".)
    if (vt.count[term_ext(s)] > 0u) return WPO_LT;
    return WPO_UN;
  }
  if (t_is_fvr) {
    if (vs.count[term_ext(t)] > 0u) return WPO_GT;
    return WPO_UN;
  }

  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return WPO_UN;

  long long ws = wpo_weight(cfg, s);
  long long wt = wpo_weight(cfg, t);

  if (ws > wt) {
    if (!s_dominates_t_vars) return WPO_UN;
    return WPO_GT;
  }
  if (ws < wt) {
    if (!t_dominates_s_vars) return WPO_UN;
    return WPO_LT;
  }

  // ws == wt -- precedence + status RPO-style.  The var condition
  // still gates both directions.

  // (1) subterm domination at equal weight.
  if (wpo_some_arg_dominates(s, t, cfg)) {
    if (s_dominates_t_vars) return WPO_GT;
    return WPO_UN;
  }
  if (wpo_some_arg_dominates(t, s, cfg)) {
    if (t_dominates_s_vars) return WPO_LT;
    return WPO_UN;
  }

  u32 lab_s = term_ext(s);
  u32 lab_t = term_ext(t);
  u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0u;
  u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0u;

  if (prec_s > prec_t) {
    if (s_dominates_t_vars && wpo_dominates_all_args(s, t, cfg)) return WPO_GT;
    return WPO_UN;
  }
  if (prec_s < prec_t) {
    if (t_dominates_s_vars && wpo_dominates_all_args(t, s, cfg)) return WPO_LT;
    return WPO_UN;
  }

  // Same head + same weight + same precedence -- status decides.
  if (term_ctr_n(s) != term_ctr_n(t)) return WPO_UN;

  if (wpo_status_is_mul(cfg, lab_s)) {
    WpoCmp mc = wpo_multi(s, t, cfg);
    if (mc == WPO_GT && !s_dominates_t_vars) return WPO_UN;
    if (mc == WPO_LT && !t_dominates_s_vars) return WPO_UN;
    return mc;
  }

  WpoCmp lex = wpo_lex(s, t, cfg);
  if (lex == WPO_EQ) return WPO_EQ;
  if (lex == WPO_GT) {
    if (s_dominates_t_vars && wpo_dominates_all_args(s, t, cfg)) return WPO_GT;
    return WPO_UN;
  }
  if (lex == WPO_LT) {
    if (t_dominates_s_vars && wpo_dominates_all_args(t, s, cfg)) return WPO_LT;
    return WPO_UN;
  }
  return WPO_UN;
}

static WpoCmp wpo_rec(Term s, Term t, const WpoConfig *cfg) {
  u32 idx = (u32)((u64)s * 0x9E3779B97F4A7C15ull
                ^ (u64)t * 0xBF58476D1CE4E5B9ull) & WPO_MEMO_MASK;
  WpoMemoEnt *e = &g_wpo_memo[idx];
  if (e->epoch == g_wpo_epoch && e->s == s && e->t == t) {
    return (WpoCmp)(i8)e->cmp;
  }
  WpoCmp c = wpo_rec_compute(s, t, cfg);
  e->s = s; e->t = t; e->epoch = g_wpo_epoch; e->cmp = (u8)c;
  return c;
}

fn WpoCmp thvm_wpo(Term s, Term t, const WpoConfig *cfg) {
  thvm_wpo_invalidate();
  return wpo_rec(s, t, cfg);
}
