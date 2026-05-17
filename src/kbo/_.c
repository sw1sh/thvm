// thvm_kbo - Knuth-Bendix ordering on first-order terms.
//
// Compares two terms over a signature of TAG_CTR (function symbols)
// and TAG_FVR (variables) under a KBO config (per-symbol weights +
// total precedence + scalar variable weight w0).  Stage 2 of the
// IC-native ATP roadmap (docs/plans/waldmeister_ic_atp.md).
//
// Returns:
//   KBO_EQ : s and t are structurally identical
//   KBO_GT : s > t under KBO
//   KBO_LT : s < t under KBO
//   KBO_UN : incomparable
//
// Algorithm (Baader-Nipkow):
//   1. Domination check: for each variable y, count(s,y) and count(t,y).
//      If neither side dominates, UN.
//   2. Compare total weights.
//   3. On weight tie, compare top symbols by precedence; on precedence
//      tie, recurse lexicographically on arguments.
//   4. Variables compare equal only to themselves; UN against anything
//      else of equal weight.
//
// The implementation uses fixed-size arrays sized by KboConfig.n_labels
// and KBO_MAX_VAR (declared below).  No heap allocation, no IC reduction.
// IC-as-pure-program port is stage 2.4 (optional).

#define KBO_MAX_VAR 64

// === structural equality ============================================

static u8 kbo_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;  // same id (ext) already checked
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!kbo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// === weight + variable counts ========================================

// Single bottom-up pass: returns the KBO weight of `t` and accumulates
// its per-variable occurrence counts into `counts`.  kbo_rec needs both
// for every term it compares; fusing them halves the term traversals --
// the weight walk and the variable walk visit the same nodes.
static u64 kbo_stats(Term t, const KboConfig *cfg, u32 *counts) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) counts[id]++;
      return cfg->var_weight;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u64 w   = (lab < cfg->n_labels) ? cfg->weights[lab] : 0;
      u32 n   = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        w += kbo_stats(term_ctr_at(t, i), cfg, counts);
      }
      return w;
    }
    default: return 0;
  }
}

// returns 1 iff for all i, lhs[i] >= rhs[i].
static u8 kbo_dominates(const u32 *lhs, const u32 *rhs) {
  for (u32 i = 0; i < KBO_MAX_VAR; i++) {
    if (lhs[i] < rhs[i]) return 0;
  }
  return 1;
}

// === main comparator =================================================

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg);

fn KboCmp thvm_kbo(Term s, Term t, const KboConfig *cfg) {
  return kbo_rec(s, t, cfg);
}

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg) {
  if (kbo_eq(s, t)) return KBO_EQ;

  u32 vc_s[KBO_MAX_VAR] = {0};
  u32 vc_t[KBO_MAX_VAR] = {0};
  u64 ws = kbo_stats(s, cfg, vc_s);
  u64 wt = kbo_stats(t, cfg, vc_t);
  u8 s_dom = kbo_dominates(vc_s, vc_t);
  u8 t_dom = kbo_dominates(vc_t, vc_s);
  if (!s_dom && !t_dom) return KBO_UN;

  if (ws > wt) return s_dom ? KBO_GT : KBO_UN;
  if (ws < wt) return t_dom ? KBO_LT : KBO_UN;

  // weights equal: compare top symbols
  u8 s_is_fvr = (term_tag(s) == TAG_FVR);
  u8 t_is_fvr = (term_tag(t) == TAG_FVR);
  if (s_is_fvr || t_is_fvr) return KBO_UN;
  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return KBO_UN;

  u32 lab_s = term_ext(s);
  u32 lab_t = term_ext(t);
  u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0;
  u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0;
  if (prec_s > prec_t) return s_dom ? KBO_GT : KBO_UN;
  if (prec_s < prec_t) return t_dom ? KBO_LT : KBO_UN;

  // same top symbol: lex comparison of args
  u32 ns = term_ctr_n(s);
  u32 nt = term_ctr_n(t);
  if (ns != nt) return KBO_UN;
  for (u32 i = 0; i < ns; i++) {
    KboCmp c = kbo_rec(term_ctr_at(s, i), term_ctr_at(t, i), cfg);
    if (c == KBO_EQ) continue;
    if (c == KBO_GT) return s_dom ? KBO_GT : KBO_UN;
    if (c == KBO_LT) return t_dom ? KBO_LT : KBO_UN;
    return KBO_UN;
  }
  return KBO_EQ;
}
