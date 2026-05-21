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
  if (s == t) return 1;   // pointer-identical -- rewriting/subst_apply
                          // share subterm cells, so this fires often
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

// === single-pass weight + variable difference ========================

// Add term `t` entirely to one side of the balance: weight*sign into
// *wb, per-variable occurrence counts*sign into bal[].  kbo_diff uses
// this for the subtrees where the two compared terms diverge.
static void kbo_addto(Term t, int sign, const KboConfig *cfg,
                      long long *wb, int *bal) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) bal[id] += sign;
      *wb += (long long)sign * (long long)cfg->var_weight;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      *wb += (long long)sign *
             (long long)((lab < cfg->n_labels) ? cfg->weights[lab] : 0);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        kbo_addto(term_ctr_at(t, i), sign, cfg, wb, bal);
      return;
    }
    default: return;
  }
}

// Simultaneous diff-traversal of `s` and `t` -- Waldmeister's Vortest
// (ORD/KBO.c).  One pass accumulates weight(s)-weight(t) into *wb and
// count(s,v)-count(t,v) into bal[]: matching nodes contribute nothing
// and cancel exactly; only the subtrees where s and t diverge are
// summed.  Returns 1 iff s and t are structurally identical.  Replaces
// the old kbo_eq + two weight/var walks -- three traversals collapse
// to one, and a pointer-equal subtree is skipped whole.
static int kbo_diff(Term s, Term t, const KboConfig *cfg,
                    long long *wb, int *bal) {
  if (s == t) return 1;                              // identical subtree
  u32 tg = term_tag(s);
  if (tg == term_tag(t) && term_ext(s) == term_ext(t)) {
    if (tg == TAG_FVR) return 1;                     // same variable
    if (tg == TAG_CTR) {
      u32 ns = term_ctr_n(s), nt = term_ctr_n(t);
      if (ns == nt) {
        int ident = 1;
        for (u32 i = 0; i < ns; i++)
          ident &= kbo_diff(term_ctr_at(s, i), term_ctr_at(t, i),
                            cfg, wb, bal);
        return ident;
      }
    } else if (term_val(s) == term_val(t)) {
      return 1;                                      // equal atom (NUM, ...)
    }
  }
  // diverge: s wholly on the + side, t wholly on the - side.
  kbo_addto(s, +1, cfg, wb, bal);
  kbo_addto(t, -1, cfg, wb, bal);
  return 0;
}

// === main comparator =================================================

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg);

fn KboCmp thvm_kbo(Term s, Term t, const KboConfig *cfg) {
  return kbo_rec(s, t, cfg);
}

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg) {
  long long wb = 0;                       // weight(s) - weight(t)
  int bal[KBO_MAX_VAR] = {0};              // count(s,v) - count(t,v)
  if (kbo_diff(s, t, cfg, &wb, bal)) return KBO_EQ;

  u8 s_dom = 1, t_dom = 1;                 // s/t dominates: balance never -/+
  for (u32 i = 0; i < KBO_MAX_VAR; i++) {
    if (bal[i] < 0) s_dom = 0;
    if (bal[i] > 0) t_dom = 0;
  }
  if (!s_dom && !t_dom) return KBO_UN;

  if (wb > 0) return s_dom ? KBO_GT : KBO_UN;
  if (wb < 0) return t_dom ? KBO_LT : KBO_UN;

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
