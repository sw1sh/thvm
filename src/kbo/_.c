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
// Two implementations live here:
//   * thvm_kbo_naive: the original Baader-Nipkow recursion.  Each
//     lexicographic level re-runs a full weight/var-balance traversal
//     and zeroes a fixed array -- O(n^2) on a left/right spine.  Kept
//     as the reference oracle for differential testing.
//   * thvm_kbo (production): Loechner's linear-time comparison
//     ("Things to Know when Implementing KBO", JAR 36(4):289-310,
//     2006), ported from Waldmeister's sources/ORD/KBO.c
//     (Vortest / KBOEntscheidungRek / KBOEntscheidungLex).  Weight
//     balance (phidiff) and variable balance are threaded through a
//     single combined traversal; the lexicographic tie-break reuses
//     the already-computed balances and only inspects the first
//     differing argument.  Touched variable ids are tracked so the
//     balance table is cleared in O(#touched) rather than memset.
//
// Both use fixed-size arrays sized by KBO_MAX_VAR.  No heap
// allocation, no IC reduction.

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

// === naive reference oracle =========================================

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
// summed.  Returns 1 iff s and t are structurally identical.
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

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg);

// Naive Baader-Nipkow comparator (reference oracle).
static KboCmp thvm_kbo_naive(Term s, Term t, const KboConfig *cfg) {
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

// === Loechner linear comparator =====================================
//
// State for one comparison.  `bal[v]` is count(s,v) - count(t,v) for
// the *current* Vortest scope; `touched[0..n_touched)` lists the var
// ids with a nonzero entry so the table clears in O(#touched).  A var
// id is pushed onto `touched` the first time its balance leaves 0.

typedef struct {
  const KboConfig *cfg;
  int  bal[KBO_MAX_VAR];
  u32  touched[KBO_MAX_VAR];
  u32  n_touched;
} KboLin;

static inline u32 kbo_weight(const KboConfig *cfg, Term t) {
  u32 lab = term_ext(t);
  return (lab < cfg->n_labels) ? cfg->weights[lab] : 0;
}
static inline u32 kbo_prec(const KboConfig *cfg, Term t) {
  u32 lab = term_ext(t);
  return (lab < cfg->n_labels) ? cfg->precedence[lab] : 0;
}

// Bump variable `id`'s balance by `delta`, recording first touch.
static inline void kbo_bump(KboLin *st, u32 id, int delta) {
  if (id >= KBO_MAX_VAR) return;
  int prev = st->bal[id];
  if (prev == 0) st->touched[st->n_touched++] = id;
  st->bal[id] = prev + delta;
}

// Add the whole subtree `t` to one side of the current balance scope:
// weight*sign into *phidiff, var occurrences*sign into bal[].
static void kbo_lin_addto(KboLin *st, Term t, int sign,
                          long long *phidiff) {
  switch (term_tag(t)) {
    case TAG_FVR:
      kbo_bump(st, term_ext(t), sign);
      *phidiff += (long long)sign * (long long)st->cfg->var_weight;
      return;
    case TAG_CTR: {
      *phidiff += (long long)sign * (long long)kbo_weight(st->cfg, t);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        kbo_lin_addto(st, term_ctr_at(t, i), sign, phidiff);
      return;
    }
    default: return;
  }
}

// Combined single-pass traversal of s,t (Waldmeister's Vortest).
// Accumulates weight(s)-weight(t) into *phidiff and the variable
// balance into st->bal (touched ids recorded).  Matching subtrees
// cancel and are skipped.  Returns 1 iff s and t are structurally
// identical.  The caller is responsible for clearing the balance
// (kbo_lin_clear) once it has read the decision out.
static int kbo_vortest(KboLin *st, Term s, Term t, long long *phidiff) {
  if (s == t) return 1;                              // identical subtree
  u32 tg = term_tag(s);
  if (tg == term_tag(t) && term_ext(s) == term_ext(t)) {
    if (tg == TAG_FVR) return 1;                     // same variable
    if (tg == TAG_CTR) {
      u32 ns = term_ctr_n(s), nt = term_ctr_n(t);
      if (ns == nt) {
        int ident = 1;
        for (u32 i = 0; i < ns; i++)
          ident &= kbo_vortest(st, term_ctr_at(s, i),
                               term_ctr_at(t, i), phidiff);
        return ident;
      }
    } else if (term_val(s) == term_val(t)) {
      return 1;                                      // equal atom
    }
  }
  // diverge: s wholly on +, t wholly on -.
  kbo_lin_addto(st, s, +1, phidiff);
  kbo_lin_addto(st, t, -1, phidiff);
  return 0;
}

// Read the variable-balance verdict and clear the touched entries.
// Returns one of KBO_EQ/KBO_GT/KBO_LT/KBO_UN describing whether s
// dominates t variable-wise (Waldmeister's VarlisteLoeschenMitEntscheidung):
//   KBO_EQ : all balances zero
//   KBO_GT : some +, no -    (s dominates)
//   KBO_LT : some -, no +    (t dominates)
//   KBO_UN : both signs present
static KboCmp kbo_lin_decide_clear(KboLin *st) {
  KboCmp erg = KBO_EQ;
  for (u32 k = 0; k < st->n_touched; k++) {
    u32 id = st->touched[k];
    int v = st->bal[id];
    st->bal[id] = 0;
    if (v == 0) continue;
    switch (erg) {
      case KBO_EQ: erg = (v < 0) ? KBO_LT : KBO_GT; break;
      case KBO_LT: if (v > 0) erg = KBO_UN; break;
      case KBO_GT: if (v < 0) erg = KBO_UN; break;
      default: break;
    }
  }
  st->n_touched = 0;
  return erg;
}

// Just clear the touched balance entries (when s,t were identical, so
// the entries are already all zero, this is a no-op reset of n_touched).
static void kbo_lin_clear(KboLin *st) {
  for (u32 k = 0; k < st->n_touched; k++) st->bal[st->touched[k]] = 0;
  st->n_touched = 0;
}

static KboCmp kbo_lin_rek(KboLin *st, Term s, Term t, long long phidiff,
                          KboCmp varcmp);
static KboCmp kbo_lin_lex(KboLin *st, Term s, Term t);

// Decision after Vortest: given the weight balance phidiff and the
// variable-set verdict varcmp, decide s vs t (Waldmeister's
// KBOEntscheidungRek).  Precondition: s,t are not structurally
// identical and the balance table is already cleared.
static KboCmp kbo_lin_rek(KboLin *st, Term s, Term t, long long phidiff,
                          KboCmp varcmp) {
  switch (varcmp) {
    case KBO_GT:                                     // s dominates t
      if (phidiff < 0) return KBO_UN;
      if (phidiff > 0) return KBO_GT;
      break;                                          // phi tie -> top symbols
    case KBO_LT:                                     // t dominates s
      if (phidiff < 0) return KBO_LT;
      if (phidiff > 0) return KBO_UN;
      break;
    case KBO_EQ:                                     // same variables
      if (phidiff < 0) return KBO_LT;
      if (phidiff > 0) return KBO_GT;
      break;
    default: return KBO_UN;                          // incomparable var sets
  }

  // phi tie: compare top symbols by precedence.
  u8 s_fvr = (term_tag(s) == TAG_FVR);
  u8 t_fvr = (term_tag(t) == TAG_FVR);
  if (s_fvr || t_fvr) return KBO_UN;
  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return KBO_UN;

  u32 ps = kbo_prec(st->cfg, s), pt = kbo_prec(st->cfg, t);
  if (ps > pt) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (ps < pt) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;

  // equal precedence: only same label with matching arity recurses
  // lexicographically; anything else is incomparable.
  if (term_ext(s) != term_ext(t)) return KBO_UN;
  u32 ns = term_ctr_n(s), nt = term_ctr_n(t);
  if (ns != nt) return KBO_UN;

  KboCmp lex = kbo_lin_lex(st, s, t);
  if (lex == KBO_EQ) return KBO_EQ;
  // map the lex outcome through the (global) variable domination.
  if (lex == KBO_GT) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (lex == KBO_LT) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;
  return KBO_UN;
}

// Lexicographic comparison of the argument lists of s,t (same head,
// same arity).  Walks children left to right; the first non-identical
// child pair decides via a fresh Vortest + kbo_lin_rek over just that
// pair (Waldmeister's KBOEntscheidungLex).  Balance is local to each
// child pair, matching the naive recursion's per-call zeroed array.
static KboCmp kbo_lin_lex(KboLin *st, Term s, Term t) {
  u32 n = term_ctr_n(s);
  for (u32 i = 0; i < n; i++) {
    Term cs = term_ctr_at(s, i), ct = term_ctr_at(t, i);
    long long phi = 0;
    int ident = kbo_vortest(st, cs, ct, &phi);
    if (ident) {
      // identical child: balance untouched/zero, just reset and go on.
      kbo_lin_clear(st);
      continue;
    }
    KboCmp varcmp = kbo_lin_decide_clear(st);
    return kbo_lin_rek(st, cs, ct, phi, varcmp);
  }
  return KBO_EQ;   // all children identical
}

// Production comparator: Loechner's linear-time KBO.
fn KboCmp thvm_kbo(Term s, Term t, const KboConfig *cfg) {
  KboLin st;
  st.cfg = cfg;
  st.n_touched = 0;
  // bal[] is only ever written via kbo_bump (which records the id) and
  // cleared via the touched list, so it stays all-zero between scopes;
  // initialise once for the first use.
  for (u32 i = 0; i < KBO_MAX_VAR; i++) st.bal[i] = 0;

  long long phidiff = 0;
  if (kbo_vortest(&st, s, t, &phidiff)) {
    kbo_lin_clear(&st);
    return KBO_EQ;
  }
  KboCmp varcmp = kbo_lin_decide_clear(&st);
  return kbo_lin_rek(&st, s, t, phidiff, varcmp);
}
