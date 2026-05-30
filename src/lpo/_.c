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
static LpoCmp lpo_rec_compute(Term s, Term t, const LpoConfig *cfg);

// Memoization of lpo_rec.  The naive LPO recursion is worst-case
// exponential: lpo_some_arg_dominates(s,t), lpo_some_arg_dominates(t,s),
// lpo_dominates_all_args and lpo_lex all recurse on overlapping
// subterm pairs, so the same (s,t) gets recompared up to ~10^5 times
// for the deeply-nested terms that arise on the Sheffer/Nand axioms.
// lpo_rec depends only on (s, t, cfg); cfg is fixed within one
// thvm_lpo call, so caching the verdict per (s,t) pair collapses the
// blow-up to O(|s|*|t|) distinct pairs.  The table is direct-mapped
// and epoch-stamped: thvm_lpo bumps g_lpo_epoch on entry, so stale
// entries from a previous (possibly different-cfg) call are simply not
// matched -- no clearing.  Term cells are pointer-stable within a
// call, so the (s,t) value pair is a sound key.  Hash collisions just
// overwrite, costing a recomputation, never a wrong verdict.
#define LPO_MEMO_BITS 16
#define LPO_MEMO_SIZE (1u << LPO_MEMO_BITS)
#define LPO_MEMO_MASK (LPO_MEMO_SIZE - 1u)
typedef struct { Term s, t; u32 epoch; u8 cmp; } LpoMemoEnt;
static LpoMemoEnt g_lpo_memo[LPO_MEMO_SIZE];
static u32 g_lpo_epoch = 0;
static const LpoConfig *g_lpo_last_cfg = NULL;
// Persistence mode.  OFF (default): thvm_lpo bumps the epoch every call,
// so the memo is fresh per call -- always sound, the right default for
// arbitrary direct callers (tests) that may reuse cell addresses without
// telling us.  ON: the memo persists across calls (huge hit rate on a
// completion) and the caller PROMISES to invalidate on cell movement;
// the ATP engine turns this on at thvm_atp_init and invalidates on every
// GC.  A non-ATP caller never flips it, so it stays sound for them.
static u8 g_lpo_persist = 0;

static inline u32 lpo_memo_hash(Term s, Term t) {
  u64 h = (s * 0x9E3779B97F4A7C15ull) ^ (t * 0xBF58476D1CE4E5B9ull);
  h ^= h >> 29;
  return (u32)h & LPO_MEMO_MASK;
}

static LpoCmp lpo_rec(Term s, Term t, const LpoConfig *cfg) {
  if (s == t) return LPO_EQ;            // pointer-identical, no memo
  u32 idx = lpo_memo_hash(s, t);
  LpoMemoEnt *e = &g_lpo_memo[idx];
  if (e->epoch == g_lpo_epoch && e->s == s && e->t == t)
    return (LpoCmp)e->cmp;
  LpoCmp c = lpo_rec_compute(s, t, cfg);
  e->s = s; e->t = t; e->epoch = g_lpo_epoch; e->cmp = (u8)c;
  return c;
}

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

// Invalidate the whole (s,t)->verdict memo in O(1) by bumping the epoch.
// The verdict depends only on (s, t, precedence); both s,t cells (until
// the GC moves them) and the precedence (within one run) are stable, so
// the memo may PERSIST across thvm_lpo calls -- which is the whole point:
// on a completion the same subterm pairs are compared millions of times.
// Callers must invalidate when those invariants break: the ATP engine
// calls this on every GC (cells move) and at thvm_atp_init (a new run may
// reuse a static LpoConfig pointer with different precedence content).
fn void thvm_lpo_invalidate(void) {
  if (++g_lpo_epoch == 0) {
    for (u32 i = 0; i < LPO_MEMO_SIZE; i++) g_lpo_memo[i].epoch = 0;
    g_lpo_epoch = 1;
  }
}

// The ATP engine opts in to persistent memoization (and promises to
// invalidate on GC).  Default off keeps thvm_lpo sound for any caller.
fn void thvm_lpo_set_persist(u8 on) { g_lpo_persist = on ? 1u : 0u; }

// === flatterm-spine subterm pretest (Waldmeister LV_VortestLPOGroesser) ====
// Single linear pass over the KboFlatNode encoding of a and b.  Detects the
// two cheap conclusive verdicts that dominate Waldmeister's hot loop:
//
//   (a) b is a strict preorder slice (= subterm) of a              -> LPO_GT
//   (b) a is a strict preorder slice (= subterm) of b              -> LPO_LT
//   (c) one operand is a constant, every symbol on the other side
//       is precedence-dominated (or the other is var-heavy)        -> +/- forced
//
// Returns +1 / -1 if conclusive (caller maps to LPO_GT / LPO_LT), or 0
// to fall through to the full recursive thvm_lpo (then the memoized
// lpo_rec resolves the residual hard cases).  See
// waldmeister/sources/ORD/LPOVortests.c:436 LV_VortestLPOGroesser for the
// reference algorithm (Termliste candidate scan).

#define LPO_FLAT_KAND_CAP 256

static int lpo_pretest_groesser_flat(const KboFlatNode *a, u32 na,
                                     const KboFlatNode *b, u32 nb,
                                     const LpoConfig *cfg) {
  if (na == 0 || nb == 0) return 0;
  // VortestLPOGroesserVar.
  if (a[0].sym < 0) return -1;            // var never strictly dominates
  if (b[0].sym < 0) {
    i32 vs = b[0].sym;
    // b is a variable -- a > b iff that var occurs strictly inside a.
    for (u32 i = 1; i < na; i++) if (a[i].sym == vs) return +1;
    return -1;
  }
  // VortestLPOGroesserKonst -- a is a constant (no children, sz == 1).
  if (a[0].sz == 1u) {
    u32 pa = ((u32)a[0].sym < cfg->n_labels) ? cfg->precedence[(u32)a[0].sym] : 0u;
    for (u32 i = 0; i < nb; i++) {
      if (b[i].sym < 0) return -1;        // var in b -- const cannot dominate
      u32 pb_i = ((u32)b[i].sym < cfg->n_labels) ? cfg->precedence[(u32)b[i].sym] : 0u;
      if (pb_i >= pa) return -1;          // some symbol in b matches or beats a
    }
    return +1;
  }
  if (b[0].sz == 1u) {
    u32 pb = ((u32)b[0].sym < cfg->n_labels) ? cfg->precedence[(u32)b[0].sym] : 0u;
    for (u32 i = 0; i < na; i++) {
      if (a[i].sym < 0) continue;
      u32 pa_i = ((u32)a[i].sym < cfg->n_labels) ? cfg->precedence[(u32)a[i].sym] : 0u;
      if (pa_i >= pb) return +1;
    }
    return -1;
  }

  // General subterm scan.  Candidates track a cursor `cur` into b: each
  // candidate represents an active hypothesis that b's prefix from position
  // 1 onwards is being matched against a's preorder walk.  When cur reaches
  // nb the candidate has matched all of b, so b is a strict subterm of a.
  u32 kand[LPO_FLAT_KAND_CAP];
  u32 nk = 0;
  i32 aelt_cur = -1;                      // aeltester candidate; -1 once dropped
  if (a[0].sym == b[0].sym && a[0].sz == b[0].sz) {
    kand[nk++] = 1u;
    aelt_cur = 1;
  }

  for (u32 i = 1; i < na; i++) {
    i32 sym = a[i].sym;
    u32 wr = 0;
    u8  aelt_kept = 0;
    for (u32 r = 0; r < nk; r++) {
      u32 cur = kand[r];
      // Reaching the right edge means a strict-subterm hit; the entire b
      // walked over a's preorder slice [i-... , i-1].
      if (cur == nb) return +1;
      if (b[cur].sym == sym) {
        if (aelt_cur >= 0 && (u32)aelt_cur == cur) aelt_kept = 1;
        kand[wr++] = cur + 1u;
        if (aelt_kept) aelt_cur = (i32)(cur + 1u);
      } else if (aelt_cur >= 0 && (u32)aelt_cur == cur) {
        aelt_cur = -1;
      }
    }
    nk = wr;
    if (sym == b[0].sym) {
      // Seed a new candidate starting fresh at b[0].  This match consumes
      // the top symbol; cur=1 means "advance past b[0]".
      if (nk < LPO_FLAT_KAND_CAP) kand[nk++] = 1u;
    }
  }
  for (u32 r = 0; r < nk; r++) if (kand[r] == nb) return +1;
  // No subterm hit either direction.  The symmetric VarMenge check (a's
  // var set must contain b's; otherwise a > b is impossible) belongs here
  // but a stricter version of this is already in lpo_pretest_varset which
  // thvm_lpo calls first; leaving 0 here punts to lpo_rec.
  return 0;
}

// Cached encode buffers for the pretest.  Reused across calls in the same
// thvm_lpo invocation; thvm_kbo_flat reuses g_kbo_flat_a/b in its own way
// so we maintain a private pair to avoid stomping on a concurrent KBO call.
static KboFlatNode g_lpo_flat_a[KBO_FLAT_CAP];
static KboFlatNode g_lpo_flat_b[KBO_FLAT_CAP];

// === flatterm recursive LPO body (Waldmeister LPO.c port) =================
// Operates on KboFlatNode slices instead of Term trees: children iteration
// is array-indexed (next child = cur + node[cur].sz) instead of going
// through term_ctr_at which descends via heap reads.  The memo is keyed
// on (pa, pb) -- positions within the per-call encode buffers -- which is
// a much smaller key space than (Term, Term) and stays stable for the
// duration of one thvm_lpo call.

#define LPO_FLAT_MEMO_BITS 14
#define LPO_FLAT_MEMO_SIZE (1u << LPO_FLAT_MEMO_BITS)
#define LPO_FLAT_MEMO_MASK (LPO_FLAT_MEMO_SIZE - 1u)
// `side` encodes which buffer holds the lhs: 0 = g_lpo_flat_a, 1 = g_lpo_flat_b.
// (pa, pb, side) -- without `side`, swapped recursive calls (a-vs-b and b-vs-a)
// collide on (pa, pb) and pollute each other's verdicts.
typedef struct { u32 pa, pb; u32 epoch; u8 cmp; u8 side; } LpoFlatMemoEnt;
static LpoFlatMemoEnt g_lpo_flat_memo[LPO_FLAT_MEMO_SIZE];
static u32 g_lpo_flat_epoch = 0;

static inline u32 lpo_flat_hash(u32 pa, u32 pb, u8 side) {
  u64 h = ((u64)pa * 0x9E3779B97F4A7C15ull) ^ ((u64)pb * 0xBF58476D1CE4E5B9ull);
  h ^= ((u64)side * 0x94D049BB133111EBull);
  h ^= h >> 29;
  return (u32)h & LPO_FLAT_MEMO_MASK;
}

static LpoCmp lpo_flat_rec(const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb,
                           const LpoConfig *cfg);

static inline __attribute__((always_inline))
u8 lpo_flat_slice_eq(const KboFlatNode *a, u32 pa,
                     const KboFlatNode *b, u32 pb) {
  if (a[pa].sym != b[pb].sym) return 0;
  if (a[pa].sz != b[pb].sz)   return 0;
  u32 n = a[pa].sz;
  for (u32 i = 1; i < n; i++) {
    if (a[pa + i].sym != b[pb + i].sym) return 0;
  }
  return 1;
}

static u8 lpo_flat_var_occurs(const KboFlatNode *t, u32 pos, i32 vsym) {
  u32 end = pos + t[pos].sz;
  for (u32 i = pos; i < end; i++) if (t[i].sym == vsym) return 1;
  return 0;
}

static inline __attribute__((always_inline))
u8 lpo_flat_dominates_all_args(const KboFlatNode *a, u32 pa,
                               const KboFlatNode *b, u32 pb,
                               const LpoConfig *cfg) {
  // b[pb] must be a CTR; iterate its children via Ende skip.
  if (b[pb].sym < 0) return 1;
  u32 child = pb + 1u;
  u32 end   = pb + b[pb].sz;
  while (child < end) {
    if (lpo_flat_rec(a, pa, b, child, cfg) != LPO_GT) return 0;
    child += b[child].sz;
  }
  return 1;
}

static inline __attribute__((always_inline))
u8 lpo_flat_some_arg_dominates(const KboFlatNode *a, u32 pa,
                               const KboFlatNode *b, u32 pb,
                               const LpoConfig *cfg) {
  if (a[pa].sym < 0) return 0;
  u32 child = pa + 1u;
  u32 end   = pa + a[pa].sz;
  while (child < end) {
    LpoCmp c = lpo_flat_rec(a, child, b, pb, cfg);
    if (c == LPO_EQ || c == LPO_GT) return 1;
    child += a[child].sz;
  }
  return 0;
}

static LpoCmp lpo_flat_lex(const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb,
                           const LpoConfig *cfg) {
  u32 ca = pa + 1u, cb = pb + 1u;
  u32 ea = pa + a[pa].sz, eb = pb + b[pb].sz;
  while (ca < ea && cb < eb) {
    LpoCmp c = lpo_flat_rec(a, ca, b, cb, cfg);
    if (c != LPO_EQ) return c;
    ca += a[ca].sz;
    cb += b[cb].sz;
  }
  return LPO_EQ;
}

static LpoCmp lpo_flat_rec_compute(const KboFlatNode *a, u32 pa,
                                   const KboFlatNode *b, u32 pb,
                                   const LpoConfig *cfg) {
  if (lpo_flat_slice_eq(a, pa, b, pb)) return LPO_EQ;
  u8 a_is_var = (a[pa].sym < 0);
  u8 b_is_var = (b[pb].sym < 0);
  if (a_is_var && b_is_var) return LPO_UN;
  if (a_is_var) {
    if (lpo_flat_var_occurs(b, pb, a[pa].sym)) return LPO_LT;
    return LPO_UN;
  }
  if (b_is_var) {
    if (lpo_flat_var_occurs(a, pa, b[pb].sym)) return LPO_GT;
    return LPO_UN;
  }
  if (lpo_flat_some_arg_dominates(a, pa, b, pb, cfg)) return LPO_GT;
  if (lpo_flat_some_arg_dominates(b, pb, a, pa, cfg)) return LPO_LT;

  // KboFlatNode.w was loaded with cfg->precedence at encode time.
  i32 prec_a = a[pa].w;
  i32 prec_b = b[pb].w;
  if (prec_a > prec_b) {
    if (lpo_flat_dominates_all_args(a, pa, b, pb, cfg)) return LPO_GT;
    return LPO_UN;
  }
  if (prec_a < prec_b) {
    if (lpo_flat_dominates_all_args(b, pb, a, pa, cfg)) return LPO_LT;
    return LPO_UN;
  }
  if (a[pa].sz != b[pb].sz) return LPO_UN;
  LpoCmp lex = lpo_flat_lex(a, pa, b, pb, cfg);
  if (lex == LPO_EQ) return LPO_EQ;
  if (lex == LPO_GT) {
    if (lpo_flat_dominates_all_args(a, pa, b, pb, cfg)) return LPO_GT;
    return LPO_UN;
  }
  if (lpo_flat_dominates_all_args(b, pb, a, pa, cfg)) return LPO_LT;
  return LPO_UN;
}

static LpoCmp lpo_flat_rec(const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb,
                           const LpoConfig *cfg) {
  if (a == b && pa == pb) return LPO_EQ;
  u8 side = (a == g_lpo_flat_a) ? 0u : 1u;
  u32 idx = lpo_flat_hash(pa, pb, side);
  LpoFlatMemoEnt *e = &g_lpo_flat_memo[idx];
  if (e->epoch == g_lpo_flat_epoch && e->pa == pa && e->pb == pb && e->side == side)
    return (LpoCmp)(i8)e->cmp;
  LpoCmp c = lpo_flat_rec_compute(a, pa, b, pb, cfg);
  e->pa = pa; e->pb = pb; e->epoch = g_lpo_flat_epoch; e->cmp = (u8)c; e->side = side;
  return c;
}

static int lpo_pretest_flat_dispatch(Term s, Term t,
                                     const LpoConfig *cfg, LpoCmp *out) {
  // Borrow KBO's encoder -- LPO does not care about weights, only sym/sz.
  // Use a stub KboConfig (var_weight=0, n_labels=0) so kbo_flat_encode
  // never tries to read an LpoConfig label array.
  static u32 stub_w[1] = {0u};
  KboConfig stub = { .weights = stub_w, .precedence = stub_w,
                     .n_labels = 0u, .var_weight = 0u };
  u32 na = 0u, nb = 0u;
  if (!kbo_flat_encode(s, &stub, g_lpo_flat_a, &na)) return 0;
  if (!kbo_flat_encode(t, &stub, g_lpo_flat_b, &nb)) return 0;
  int r1 = lpo_pretest_groesser_flat(g_lpo_flat_a, na,
                                     g_lpo_flat_b, nb, cfg);
  if (r1 > 0) { *out = LPO_GT; return 1; }
  int r2 = lpo_pretest_groesser_flat(g_lpo_flat_b, nb,
                                     g_lpo_flat_a, na, cfg);
  if (r2 > 0) { *out = LPO_LT; return 1; }
  // Both directions returned non-GT.  If BOTH ruled the strict-dominance
  // out forcibly (-1 / -1) AND neither side is var-only, the terms are
  // LPO_UN.  But the constant-side cases already covered the var-only
  // legs, and the general-scan -1 path means "no subterm hit found", which
  // is NOT a strict UN proof -- there might still be a non-subterm-based
  // LPO_GT/LT via the recursive case (2) precedence path.  So only commit
  // to UN when BOTH legs came back from the const/var branches.
  return 0;
}

fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg) {
  if (g_lpo_persist) {
    // Persist across calls; invalidate only on a config change (distinct
    // precedence -> distinct verdicts) -- the caller handles cell moves.
    if (cfg != g_lpo_last_cfg) { g_lpo_last_cfg = cfg; thvm_lpo_invalidate(); }
  } else {
    // Safe default: fresh memo every call.
    thvm_lpo_invalidate();
  }
  if (lpo_eq(s, t)) return LPO_EQ;
  // Flatterm subterm pretest (LV_VortestLPOGroesser).  Catches the two
  // common conclusive cases (b strict subterm of a, or a strict subterm of
  // b) in O(|s|+|t|) without recursion or memo; only the residual hard
  // cases reach lpo_rec.  Opt-in via THVM_LPO_VORTEST_FLAT until A/B
  // confirms no regression on KBO-mode or non-Sheffer benches.
  static int vortest_gate = -1;
  if (vortest_gate < 0) {
    const char *e = getenv("THVM_LPO_VORTEST_FLAT");
    vortest_gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  LpoCmp pre;
  static int flatrec_gate = -1;
  if (flatrec_gate < 0) {
    const char *e = getenv("THVM_LPO_FLAT_REC");
    flatrec_gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  // When Flatrec is on, the Vortest pretest reuses the same flat encode.
  // When Flatrec is off, Vortest pays its own encode via the dispatch helper.
  if (vortest_gate && !flatrec_gate &&
      lpo_pretest_flat_dispatch(s, t, cfg, &pre)) return pre;
  // Flatterm recursive LPO (WM LPO.c port).  Encodes both operands into
  // cache-dense KboFlatNode arrays and runs the same Dershowitz LPO body
  // over them with O(1) child stepping (cur += node[cur].sz).  Memoized
  // per call on (pa, pb, side) positions.  Opt-in via THVM_LPO_FLAT_REC;
  // falls back to the Term-tree lpo_rec on encode overflow or when disabled.
  if (flatrec_gate) {
    // Top-level memo across calls keyed by (s, t).  When ATP turns on
    // persistence the engine invalidates this on every GC, mirroring
    // g_lpo_memo.  Hits return without paying the encode/recursion cost.
    if (g_lpo_persist) {
      u32 idx = (u32)((u64)s * 0x9E3779B97F4A7C15ull
                    ^ (u64)t * 0xBF58476D1CE4E5B9ull) & LPO_MEMO_MASK;
      LpoMemoEnt *e2 = &g_lpo_memo[idx];
      if (e2->epoch == g_lpo_epoch && e2->s == s && e2->t == t)
        return (LpoCmp)(i8)e2->cmp;
    }
    // Use LpoConfig's precedence as kbo_flat_encode's `weights` so each
    // KboFlatNode.w holds the symbol's precedence directly -- the inner
    // recursion avoids re-indexing cfg->precedence[sym] per node.
    KboConfig stub = { .weights = cfg->precedence, .precedence = cfg->precedence,
                       .n_labels = cfg->n_labels, .var_weight = 0u };
    u32 na = 0u, nb = 0u;
    if (kbo_flat_encode(s, &stub, g_lpo_flat_a, &na) &&
        kbo_flat_encode(t, &stub, g_lpo_flat_b, &nb)) {
      // Vortest reuses the encode (zero extra cost).
      if (vortest_gate) {
        int r1 = lpo_pretest_groesser_flat(g_lpo_flat_a, na,
                                           g_lpo_flat_b, nb, cfg);
        if (r1 > 0) return LPO_GT;
        int r2 = lpo_pretest_groesser_flat(g_lpo_flat_b, nb,
                                           g_lpo_flat_a, na, cfg);
        if (r2 > 0) return LPO_LT;
      }
      // Fast var-set incomparability check directly on the flat spines.
      // Avoids the recursive lpo_var_set_acc / lpo_vars_in_range walk over
      // Term trees that the pre-flatrec lpo_pretest_varset would do.
      u32 vs[LPO_MAX_VAR] = {0};
      u32 vt[LPO_MAX_VAR] = {0};
      u8 in_range = 1u;
      for (u32 i = 0; i < na; i++) {
        if (g_lpo_flat_a[i].sym < 0) {
          u32 vid = (u32)(-g_lpo_flat_a[i].sym - 1);
          if (vid >= LPO_MAX_VAR) { in_range = 0u; break; }
          vs[vid] = 1u;
        }
      }
      if (in_range) for (u32 i = 0; i < nb; i++) {
        if (g_lpo_flat_b[i].sym < 0) {
          u32 vid = (u32)(-g_lpo_flat_b[i].sym - 1);
          if (vid >= LPO_MAX_VAR) { in_range = 0u; break; }
          vt[vid] = 1u;
        }
      }
      if (in_range) {
        u8 s_extra = 0u, t_extra = 0u;
        for (u32 i = 0; i < LPO_MAX_VAR; i++) {
          if (vs[i] && !vt[i]) s_extra = 1u;
          if (vt[i] && !vs[i]) t_extra = 1u;
        }
        if (s_extra && t_extra) return LPO_UN;
      }
      // Bump epoch -- positions are only meaningful for THIS encode pair.
      if (++g_lpo_flat_epoch == 0) {
        for (u32 i = 0; i < LPO_FLAT_MEMO_SIZE; i++) g_lpo_flat_memo[i].epoch = 0;
        g_lpo_flat_epoch = 1;
      }
      // Skip memo for the top-level (0,0) call -- it always hits slot 0 so
      // the memo lookup is pure overhead.  The recursive calls below this
      // still memoize.
      LpoCmp rr = lpo_flat_rec_compute(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u, cfg);
      if (g_lpo_persist) {
        u32 idx2 = (u32)((u64)s * 0x9E3779B97F4A7C15ull
                       ^ (u64)t * 0xBF58476D1CE4E5B9ull) & LPO_MEMO_MASK;
        LpoMemoEnt *e3 = &g_lpo_memo[idx2];
        e3->s = s; e3->t = t; e3->epoch = g_lpo_epoch; e3->cmp = (u8)rr;
      }
      return rr;
    }
  }
  // Term-tree fallback (encode overflow or flatrec disabled): the recursive
  // lpo_pretest_varset is the only remaining safety net for the UN case.
  LpoCmp pre2;
  if (lpo_pretest_varset(s, t, &pre2)) return pre2;
  return lpo_rec(s, t, cfg);
}

static LpoCmp lpo_rec_compute(Term s, Term t, const LpoConfig *cfg) {
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
