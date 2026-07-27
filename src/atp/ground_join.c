// === Ground-joinability redundancy criterion ========================
// Martin-Nipkow (1990) / Twee (CADE 2021 sec 3.1) / Avenhaus-Hillen-
// brand-Loechner (2003).  A critical pair is GROUND-JOINABLE if every
// ground instance of it joins; such a CP is redundant and may be
// deleted without losing refutational completeness.  We implement
// Twee's SYMBOLIC order-parameterised version (gj_spec.md sections C+D):
// a constraint C is a total preorder on the CP's atoms (variables AND
// the relevant constants), represented as a Model that assigns each atom
// a (major,minor) rank.  Under C we rewrite SYMBOLICALLY: a rule l->r
// fires at a matched position iff lessIn(C, r-image, l-image) is defined
// -- i.e. (l-image) >=_C (r-image) holds for ALL grounding sigma that
// satisfy C -- and the two images are syntactically distinct.  lessIn
// (section D) decides >=_C for KBO by minimising the weight-difference
// linear form over all such sigma (the suffix-sum `minimumIn`), then a
// lexicographic comparison.  Crucially lessIn NEVER over-approximates
// >=_C (soundness condition P2): if it returns "defined" the inequality
// truly holds for every grounding, so no illegal rewrite ever fires.
//
// The groundJoin driver maintains a worklist of branches that together
// COVER every total preorder of the atoms.  Each branch is solved to a
// Model (or, when it forces ties, an equality substitution); the CP is
// normalised under the Model and the two normal forms compared.  On a
// successful join the Model is GENERALISED (weakenModel: drop an atom or
// merge two adjacent strict groups into a tie, NEVER merging two
// constants), and the complementary region is re-queued via diag().
// Equality branches unify the tied variables and recurse on the smaller
// CP.  Only when the worklist is fully exhausted (every ground ordering
// covered by a successful join -- soundness condition P3) do we DELETE.
// Any budget overflow, OOM, or unjoined branch -> KEEP (sound fallback).
//
// KBO is the supported order.  For LPO (s->lpo != NULL) we KEEP
// unconditionally: lessIn is implemented for KBO only, so GJ is a no-op
// (always sound) under LPO for now.
//
// Deletion is gated at the call site behind the runtime flag
// s->use_ground_join (Method -> {... "GroundJoin" -> True}); the
// n_cps_ground_joinable counter ticks regardless for measurement.  The
// whole region compiles out unless ATP_CP_GROUND_JOIN is defined (the
// shipped paclet defines it; see WL_ATP_DEFINES).
#ifdef ATP_CP_GROUND_JOIN

// Cap on distinct variables of the CP.  The branch worklist still
// terminates for larger counts, but small CPs are the norm and a tight
// cap bounds worst-case branching; over the cap -> KEEP (sound).
#define ATP_GJ_MAX_VARS 6
// Max atoms in a Model = CP variables + relevant constants.
#define ATP_GJ_MAX_ATOMS 16
// Per-side symbolic-rewrite step cap during a single join attempt.
#define ATP_GJ_NORM_CAP 64
// Cap on the number of branches processed by the groundJoin driver
// before bailing to KEEP (sound budget fallback, gj_spec.md C).
#define ATP_GJ_BRANCH_CAP 4096

// Collect distinct TAG_FVR var ids appearing in `t` into ids[] (size
// cap).  *n is the running count; returns 0 if it would overflow `cap`
// (caller treats overflow as "too many vars -> keep").
static u8 atp_gj_collect_vars(Term t, u32 *ids, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      for (u32 i = 0; i < *n; i++) {
        if (ids[i] == id) return 1;       // already seen
      }
      if (*n >= cap) return 0;            // too many distinct vars
      ids[*n] = id;
      (*n)++;
      return 1;
    }
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      for (u32 i = 0; i < k; i++) {
        if (!atp_gj_collect_vars(term_ctr_at(t, i), ids, n, cap)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// ── Atoms ───────────────────────────────────────────────────────────
// An atom is a variable (TAG_FVR id) or a nullary constant (TAG_CTR
// label, arity 0).  We compare atom SIZES under KBO: a constant's size
// is its fixed weight; a variable's size is an unknown integer >= the
// minimal admissible ground size.  The constraint C orders atoms by size.
typedef struct {
  u8  is_const;   // 1 = constant (TAG_CTR/0), 0 = variable
  u32 id;         // var id (is_const=0) or label (is_const=1)
} GjAtom;

static inline u8 gj_atom_eq(GjAtom a, GjAtom b) {
  return a.is_const == b.is_const && a.id == b.id;
}

// Size of a constant atom under the (real) KBO config.  Constants have
// no children, so their size is just their per-symbol weight.
static inline u32 gj_const_size(const KboConfig *cfg, u32 label) {
  return (label < cfg->n_labels) ? cfg->weights[label] : 0u;
}

// The minimal admissible ground-term size (KBO w0): any ground term has
// size >= var_weight (a single variable's image is at least one minimal
// symbol).  Used as the universal lower bound for an UNCONSTRAINED
// variable's size.
static inline u32 gj_min_size(const KboConfig *cfg) {
  // The smallest possible ground term is a single minimal-weight
  // constant; if every constant outweighs var_weight, a 1-arg term over
  // a 0-weight unary symbol could still be var_weight.  var_weight (w0)
  // is the safe, KBO-standard lower bound (P1: weights >= w0 > 0 except
  // possibly one unary symbol of weight 0, whose argument is >= w0).
  return cfg->var_weight;
}

// ── Model ───────────────────────────────────────────────────────────
// A Model is a total preorder on a fixed atom set: each atom carries a
// `major` size-rank.  a < b  iff  major(a) < major(b);  a == b (a tie,
// same SIZE) iff major(a) == major(b).  Atoms with the same major form
// one "tie group" (all forced to equal size).  (Twee carries a second
// `minor` field to make the preorder a strict total order on atoms for
// its solver; the size-reasoning here needs only `major`, so we drop it.)
typedef struct {
  GjAtom atom[ATP_GJ_MAX_ATOMS];
  u32    major[ATP_GJ_MAX_ATOMS];   // size-rank; equal major = equal size
  u32    n;
} GjModel;

// Find atom `a` in the model; return its index or -1.
static int gj_model_find(const GjModel *m, GjAtom a) {
  for (u32 i = 0; i < m->n; i++) if (gj_atom_eq(m->atom[i], a)) return (int)i;
  return -1;
}

// Compare two atoms' SIZES under the model: -1 (a<b), 0 (tie), 1 (a>b),
// or 2 = UNKNOWN (one or both atoms absent from the model -> C says
// nothing about them).
static int gj_model_cmp(const GjModel *m, GjAtom a, GjAtom b) {
  if (gj_atom_eq(a, b)) return 0;
  int ia = gj_model_find(m, a), ib = gj_model_find(m, b);
  if (ia < 0 || ib < 0) return 2;
  u32 ma = m->major[ia], mb = m->major[ib];
  if (ma < mb) return -1;
  if (ma > mb) return 1;
  return 0;
}

// ── KBO size as a linear form, minimised under C (gj_spec.md D) ──────
// We compare KBO sizes of t and u.  D = size(u) - size(t) is a linear
// form  c0 + sum_v coeff[v] * size(v)  where c0 is the net weight of
// FUNCTION-SYMBOL occurrences (variables excluded) and coeff[v] =
// count(u,v) - count(t,v).  size(v) is the unknown ground size of v.
//
// gj_size_form accumulates D's pieces by a diff-traversal of (t,u):
//   *c0          += net constant/funcsym weight  (u on +, t on -)
//   var_coeff[id] += net variable count          (u on +, t on -)
// Returns 0 if a variable id exceeds the table; caller treats that as
// "cannot decide" (Incomparable -> KEEP-safe).
static void gj_form_addto(Term t, int sign, const KboConfig *cfg,
                          long long *c0, long long *var_coeff) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) var_coeff[id] += sign;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      *c0 += (long long)sign *
             (long long)((lab < cfg->n_labels) ? cfg->weights[lab] : 0);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        gj_form_addto(term_ctr_at(t, i), sign, cfg, c0, var_coeff);
      return;
    }
    default: return;
  }
}

// minimumIn (gj_spec.md D, note "kbo under assumptions").  Compute the
// MINIMUM of D = c0 + sum_v coeff[v]*size(v) over every assignment of
// variable sizes consistent with the model `m` (and every grounding of
// unconstrained variables).  *finite is set 0 if the minimum is
// unbounded below (-infinity).  On *finite==0 the returned value is
// meaningless.  This is the load-bearing soundness primitive: an
// UNDER-estimate of the true minimum is the unsafe direction (it could
// claim D>0 strict when some sigma drives D<=0 -- an OVER-approximation
// of >=_C).  We therefore compute the EXACT minimum; any case we cannot
// bound exactly is reported as -infinity (KEEP-safe).
//
// Method: variables constrained by C lie in tie/order groups separated
// by constants along the size order.  We process the model's atoms in
// ascending size (major) order.  Constants act as fixed lower/upper
// pivots; the variables between two consecutive constants form a group
// with lower bound lo = size(lower constant) (or w0 if none) and upper
// bound hi = size(upper constant) (or none).  Within a group the
// variables are size-ordered by their major: x in the group satisfies
// lo <= size(x) and (if hi exists) size(x) <= hi, and the group's
// variables are nondecreasing in size.  Per spec D, with coeffs listed
// in ascending group order and sums = scanr1(+) (suffix sums):
//   all suffix sums >= 0:  min = (sum coeffs) * lo
//   some suffix sum < 0, no hi:  -infinity
//   some suffix sum < 0, hi exists: min = (sum coeffs)*lo
//                                      + (-min_suffix)*(lo - hi)
// Variables NOT in the model ("orphans") with coeff k: k<0 -> -infinity;
// else contribute 0 (size driven to its own minimum w0, but coeff>=0 so
// the minimal contribution toward D is taking size as small as possible;
// orphan min-contribution per spec = 0 for k>=0 since unconstrained
// orphan can be made equal to the global infimum which is folded into
// c0 already via... ) -- see note below.
//
// Returns the (finite) minimum; sets *finite.
static long long gj_minimum_in(const GjModel *m, const KboConfig *cfg,
                               const long long *var_coeff,
                               const u32 *cp_vars, u32 n_cp_vars,
                               long long c0, u8 *finite) {
  *finite = 1;
  long long total = c0;
  long long w0 = (long long)gj_min_size(cfg);

  // Orphan variables: present in the CP but absent from the model.
  // Their size is any value >= w0, unconstrained relative to others.
  //   coeff < 0  -> size can grow without bound -> -infinity.
  //   coeff > 0  -> minimised by size = w0 -> contributes coeff*w0.
  //   coeff == 0 -> contributes 0.
  for (u32 i = 0; i < n_cp_vars; i++) {
    u32 id = cp_vars[i];
    GjAtom va = { .is_const = 0, .id = id };
    if (gj_model_find(m, va) >= 0) continue;          // not an orphan
    long long k = (id < KBO_MAX_VAR) ? var_coeff[id] : 0;
    if (k < 0) { *finite = 0; return 0; }
    total += k * w0;
  }

  // Walk the model's atoms in ascending size order.  Group variables
  // between constants.  We sort indices by major (stable; minor breaks
  // ties but same-major atoms are one group regardless of minor).
  u32 order[ATP_GJ_MAX_ATOMS];
  for (u32 i = 0; i < m->n; i++) order[i] = i;
  for (u32 i = 1; i < m->n; i++) {                    // insertion sort
    u32 key = order[i];
    u32 km = m->major[key];
    int j = (int)i - 1;
    while (j >= 0 && m->major[order[j]] > km) { order[j + 1] = order[j]; j--; }
    order[j + 1] = key;
  }

  // Scan groups.  A group is a maximal run of consecutive (in size
  // order) VARIABLE atoms; the constant immediately before sets lo, the
  // constant immediately after sets hi.  Multiple variables tied with a
  // constant (same major as a constant) are pinned to that constant's
  // size -- handled by treating a same-major constant as both bound.
  u32 i = 0;
  long long cur_lo = w0;   // running lower bound (last constant size, or w0)
  while (i < m->n) {
    u32 ai = order[i];
    if (m->atom[ai].is_const) {
      cur_lo = (long long)gj_const_size(cfg, m->atom[ai].id);
      i++;
      continue;
    }
    // Start of a variable group at size-order position i.  Collect the
    // group: consecutive variables.  Note variables tied (same major)
    // with each other share a size; we still list each coeff (they all
    // share one unknown, but the suffix-sum derivation treats them as a
    // nondecreasing chain which a tie satisfies, so listing each is
    // correct -- ties just collapse y_i deltas to 0, never changing the
    // suffix-sum sign test).
    long long coeffs[ATP_GJ_MAX_ATOMS];
    u32 ng = 0;
    while (i < m->n && !m->atom[order[i]].is_const) {
      u32 vid = m->atom[order[i]].id;
      long long k = (vid < KBO_MAX_VAR) ? var_coeff[vid] : 0;
      coeffs[ng++] = k;
      i++;
    }
    // Upper bound: the next atom (if a constant) gives hi; if the next
    // atom is past the end, no upper bound.  (The next atom can only be
    // a constant here, since we consumed all consecutive variables.)
    u8 has_hi = 0;
    long long hi = 0;
    if (i < m->n && m->atom[order[i]].is_const) {
      has_hi = 1;
      hi = (long long)gj_const_size(cfg, m->atom[order[i]].id);
    }
    // suffix sums
    long long sum = 0;
    long long min_suffix = 0;       // minimum over suffix sums (incl. empty=+inf guard)
    u8 have_min = 0;
    long long suff[ATP_GJ_MAX_ATOMS];
    long long acc = 0;
    for (int g = (int)ng - 1; g >= 0; g--) { acc += coeffs[g]; suff[g] = acc; }
    for (u32 g = 0; g < ng; g++) {
      sum += coeffs[g];
      if (!have_min || suff[g] < min_suffix) { min_suffix = suff[g]; have_min = 1; }
    }
    // sum == total coeff of the group; min_suffix == min suffix sum.
    if (ng == 0) { cur_lo = has_hi ? hi : cur_lo; continue; }
    if (min_suffix >= 0) {
      total += sum * cur_lo;
    } else if (!has_hi) {
      *finite = 0; return 0;        // unbounded below
    } else {
      total += sum * cur_lo + (-min_suffix) * (cur_lo - hi);
    }
    cur_lo = has_hi ? hi : cur_lo;
  }
  return total;
}

// ── Per-normalize gate session ──────────────────────────────────────
// Every gj_less_in issued under ONE gj_normalize call shares the same
// (model, cp_vars) pair, but gj_minimum_in re-derived three loop-
// invariant structures per call (measured 47% of gj_less_in self time
// on LAT138-1): the size-order sort of the model atoms, the group
// decomposition (variable runs between constant pivots with their
// lo/hi bounds), and the orphan classification of the CP's variables.
// The session precomputes all three ONCE at gj_normalize entry;
// gj_minimum_in_sess then reads only var_coeff.  Pure arithmetic
// identity: the sort is the same stable insertion sort over the same
// fixed keys, the group walk replays the same traversal, and the
// orphan set is pointer-stable for the whole normalize -- so every
// verdict is byte-identical to the unarmed path (which remains as the
// fallback for any caller outside an armed normalize).
typedef struct {
  const GjModel *m;
  const u32     *cp_vars;
  u32            n_cp_vars;
  u32  orphan_ids[ATP_GJ_MAX_VARS];   // cp_vars absent from the model
  u32  n_orphans;
  u32  n_grp;
  struct {
    long long lo;                     // group lower pivot (const size / w0)
    long long hi;                     // upper pivot (valid when has_hi)
    u8        has_hi;
    u32       nv;                     // vars in the group, size order
    u32       vids[ATP_GJ_MAX_ATOMS];
  } grp[ATP_GJ_MAX_ATOMS + 1u];
} GjSess;

static GjSess g_gj_sess;
static u8     g_gj_sess_live = 0;

static void gj_sess_arm(const GjModel *m, const KboConfig *cfg,
                        const u32 *cp_vars, u32 n_cp_vars) {
  GjSess *ss = &g_gj_sess;
  ss->m         = m;
  ss->cp_vars   = cp_vars;
  ss->n_cp_vars = n_cp_vars;
  ss->n_orphans = 0u;
  ss->n_grp     = 0u;
  for (u32 v = 0; v < n_cp_vars; v++) {
    GjAtom va = { .is_const = 0, .id = cp_vars[v] };
    if (gj_model_find(m, va) < 0) ss->orphan_ids[ss->n_orphans++] = cp_vars[v];
  }
  // Stable insertion sort by major -- the exact per-call sort of
  // gj_minimum_in, done once.
  u32 order[ATP_GJ_MAX_ATOMS];
  for (u32 v = 0; v < m->n; v++) order[v] = v;
  for (u32 v = 1; v < m->n; v++) {
    u32 key = order[v];
    u32 km = m->major[key];
    int j = (int)v - 1;
    while (j >= 0 && m->major[order[j]] > km) { order[j + 1] = order[j]; j--; }
    order[j + 1] = key;
  }
  // Replay gj_minimum_in's group walk, recording (lo, hi, vids) per
  // variable group.
  long long w0 = (long long)gj_min_size(cfg);
  u32 i = 0;
  long long cur_lo = w0;
  while (i < m->n) {
    u32 ai = order[i];
    if (m->atom[ai].is_const) {
      cur_lo = (long long)gj_const_size(cfg, m->atom[ai].id);
      i++;
      continue;
    }
    u32 g = ss->n_grp;
    ss->grp[g].lo = cur_lo;
    ss->grp[g].nv = 0u;
    while (i < m->n && !m->atom[order[i]].is_const) {
      ss->grp[g].vids[ss->grp[g].nv++] = m->atom[order[i]].id;
      i++;
    }
    ss->grp[g].has_hi = 0u;
    ss->grp[g].hi     = 0;
    if (i < m->n && m->atom[order[i]].is_const) {
      ss->grp[g].has_hi = 1u;
      ss->grp[g].hi = (long long)gj_const_size(cfg, m->atom[order[i]].id);
    }
    ss->n_grp++;
    cur_lo = ss->grp[g].has_hi ? ss->grp[g].hi : cur_lo;
  }
  g_gj_sess_live = 1u;
}

static void gj_sess_disarm(void) { g_gj_sess_live = 0u; }

// ── Normalize-scoped NF memo ────────────────────────────────────────
// gj_normalize restarts its scan from the ROOT after every fired step
// (measured 2.55 steps / 3.55 root scans per normalize on LAT138-1),
// so every position that precedes the redex is re-queried -- ri query,
// thvm_match, subst_apply, and the gj_less_in gate all re-run with
// unchanged inputs.  Within ONE gj_normalize the verdict function is
// FIXED (rule set, model, cp_vars, g_gj_orient_free, s->gj_exclude are
// all constant, and GC never runs mid-normalize -- heap_alloc never
// auto-collects -- so Term values are stable), which makes "no
// position of t fires" (fired == 0) a pure function of the Term value.
// The memo records exactly those no-fire verdicts and answers repeat
// scans without re-deriving them; a memo hit returns the identical
// (t, fired=0) the rescan would compute, so the first firing position
// of every scan -- and the whole trajectory -- is byte-identical.
// Protected roots (protect != 0: the Dreieck gate changes the verdict
// function at that call only) bypass the memo both ways; their child
// recursion (protect == 0) participates as usual.
// Generation-stamped: gj_normalize bumps the generation instead of
// clearing.  Insertions stop when the table saturates (lookups then
// miss -- pure slowdown, never a wrong verdict).
#define GJ_NF_BITS 12u
#define GJ_NF_SIZE (1u << GJ_NF_BITS)
static Term g_gj_nf_key[GJ_NF_SIZE];
static u32  g_gj_nf_gen[GJ_NF_SIZE];
static u32  g_gj_nf_cur = 0u;
static u32  g_gj_nf_n   = 0u;

static inline u32 gj_nf_slot(Term t) {
  u64 h = (u64)t;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
  return (u32)h & (GJ_NF_SIZE - 1u);
}

static inline u8 gj_nf_hit(Term t) {
  u32 i = gj_nf_slot(t);
  while (g_gj_nf_gen[i] == g_gj_nf_cur) {
    if (g_gj_nf_key[i] == t) return 1u;
    i = (i + 1u) & (GJ_NF_SIZE - 1u);
  }
  return 0u;
}

static inline void gj_nf_add(Term t) {
  if (g_gj_nf_n >= (GJ_NF_SIZE / 2u)) return;   // saturated: stop inserting
  u32 i = gj_nf_slot(t);
  while (g_gj_nf_gen[i] == g_gj_nf_cur) {
    if (g_gj_nf_key[i] == t) return;
    i = (i + 1u) & (GJ_NF_SIZE - 1u);
  }
  g_gj_nf_key[i] = t;
  g_gj_nf_gen[i] = g_gj_nf_cur;
  g_gj_nf_n++;
}

// gj_minimum_in over the armed session: identical arithmetic on the
// precomputed orphan list + group structure.
static long long gj_minimum_in_sess(const GjSess *ss, const KboConfig *cfg,
                                    const long long *var_coeff,
                                    long long c0, u8 *finite) {
  *finite = 1;
  long long total = c0;
  long long w0 = (long long)gj_min_size(cfg);
  for (u32 i = 0; i < ss->n_orphans; i++) {
    u32 id = ss->orphan_ids[i];
    long long k = (id < KBO_MAX_VAR) ? var_coeff[id] : 0;
    if (k < 0) { *finite = 0; return 0; }
    total += k * w0;
  }
  for (u32 g = 0; g < ss->n_grp; g++) {
    u32 ng = ss->grp[g].nv;
    if (ng == 0u) continue;
    long long sum = 0;
    long long min_suffix = 0;
    u8 have_min = 0;
    long long acc = 0;
    // The suffix-sum minimum accumulates back-to-front, exactly as the
    // unarmed path's suff[] pass.
    for (int v = (int)ng - 1; v >= 0; v--) {
      u32 vid = ss->grp[g].vids[v];
      long long k = (vid < KBO_MAX_VAR) ? var_coeff[vid] : 0;
      acc += k;
      sum += k;
      if (!have_min || acc < min_suffix) { min_suffix = acc; have_min = 1; }
    }
    if (min_suffix >= 0) {
      total += sum * ss->grp[g].lo;
    } else if (!ss->grp[g].has_hi) {
      *finite = 0; return 0;
    } else {
      total += sum * ss->grp[g].lo +
               (-min_suffix) * (ss->grp[g].lo - ss->grp[g].hi);
    }
  }
  return total;
}

// sizeLessIn: decide size(t) vs size(u) under C.
//   returns  -1 : size(t) <  size(u) for all sigma  (Strict on size)
//             0 : size(t) == size(u) for all sigma  (fall through to lex)
//             1 : not provable either way            (Incomparable)
// Per spec D: L = minimum of D = size(u)-size(t) over sigma|=C.
//   L >  0 -> Strict;  L == 0 -> Nonstrict (lex);  L < 0 / -inf -> Incomp.
static int gj_size_less_in(const GjModel *m, const KboConfig *cfg,
                           Term t, Term u,
                           const u32 *cp_vars, u32 n_cp_vars) {
  long long c0 = 0;
  long long var_coeff[KBO_MAX_VAR];
  for (u32 i = 0; i < KBO_MAX_VAR; i++) var_coeff[i] = 0;
  // D = size(u) - size(t): u on +, t on -.
  gj_form_addto(u, +1, cfg, &c0, var_coeff);
  gj_form_addto(t, -1, cfg, &c0, var_coeff);
  u8 finite = 0;
  // The gate session serves every call whose (model, cp_vars) match the
  // armed normalize -- including the whole lex recursion, which passes
  // both pointers down unchanged.
  long long L = (g_gj_sess_live && g_gj_sess.m == m &&
                 g_gj_sess.cp_vars == cp_vars)
      ? gj_minimum_in_sess(&g_gj_sess, cfg, var_coeff, c0, &finite)
      : gj_minimum_in(m, cfg, var_coeff, cp_vars, n_cp_vars, c0, &finite);
  if (!finite) return 1;            // unbounded below -> Incomparable
  if (L > 0) return -1;             // size(t) < size(u) always
  if (L == 0) return 0;             // equal size for all sigma -> lex
  return 1;                         // L < 0: not provable -> Incomparable
}

// ── lexLessIn (gj_spec.md D step 2), reached only when sizes are
// FORCED equal.  Returns one of GJ_STRICT / GJ_NONSTRICT / GJ_INCOMP
// meaning t < u strictly / t <= u (and == for some sigma) / unknown.
enum { GJ_STRICT = 0, GJ_NONSTRICT = 1, GJ_INCOMP = 2 };

static GjAtom gj_term_atom(Term t, u8 *ok) {
  GjAtom a = {0};
  if (term_tag(t) == TAG_FVR) { a.is_const = 0; a.id = term_ext(t); *ok = 1; }
  else if (term_tag(t) == TAG_CTR && term_ctr_n(t) == 0) {
    a.is_const = 1; a.id = term_ext(t); *ok = 1;
  } else *ok = 0;
  return a;
}

// lessEqInModel for two atoms: is atom(t) <= atom(u) (size) under C?
// returns GJ_STRICT (t<u), GJ_NONSTRICT (t==u forced), or GJ_INCOMP.
static int gj_atom_less_eq(const GjModel *m, GjAtom at, GjAtom au) {
  if (gj_atom_eq(at, au)) return GJ_NONSTRICT;
  int c = gj_model_cmp(m, at, au);
  if (c == -1) return GJ_STRICT;
  if (c == 0)  return GJ_NONSTRICT;  // tie group: equal size
  return GJ_INCOMP;                  // 1 (a>b) or 2 (unknown)
}

static int gj_lex_less_in(const GjModel *m, const KboConfig *cfg,
                          Term t, Term u,
                          const u32 *cp_vars, u32 n_cp_vars, u32 depth);

// lessIn(cond, t, u): size-first then lex.  Returns the strictness of
// "t <= u under C" or GJ_INCOMP.
//   GJ_STRICT    : tσ <  uσ for ALL σ |= C
//   GJ_NONSTRICT : tσ <= uσ for ALL σ |= C
//   GJ_INCOMP    : not provable
// PRECONDITION (both functions): the caller has already established
// kbo_eq(t, u) == 0.  Every call site pre-checks -- the candidate
// loops' no-op skip, the lex arg loop's kbo_eq filter, and
// gj_less_in's own dispatch into the lex -- so the historical entry
// re-compares were pure waste (one full kbo_eq per gate call, twice
// on the size-neutral path) and dropping them is verdict-identical.
static int gj_less_in(const GjModel *m, const KboConfig *cfg,
                      Term t, Term u, const u32 *cp_vars, u32 n_cp_vars,
                      u32 depth) {
  int sz = gj_size_less_in(m, cfg, t, u, cp_vars, n_cp_vars);
  if (sz == -1) return GJ_STRICT;     // size(t) < size(u) strictly
  if (sz == 1)  return GJ_INCOMP;     // size not provably <=
  // sizes forced equal -> lexicographic
  return gj_lex_less_in(m, cfg, t, u, cp_vars, n_cp_vars, depth);
}

static int gj_lex_less_in(const GjModel *m, const KboConfig *cfg,
                          Term t, Term u,
                          const u32 *cp_vars, u32 n_cp_vars, u32 depth) {
  if (depth > 64) return GJ_INCOMP;   // recursion guard -> KEEP-safe

  u8 t_atom = 0, u_atom = 0;
  GjAtom at = gj_term_atom(t, &t_atom);
  GjAtom au = gj_term_atom(u, &u_atom);

  // Both atoms: direct order from C.
  if (t_atom && u_atom) return gj_atom_less_eq(m, at, au);

  // t atom, u compound: if some atomic proper subterm v of u has t <= v
  // under C (strictly or tied), then t < u strictly (since |u| has the
  // same size and a larger structure dominating t).  Conservative:
  // require an atomic subterm v with gj_atom_less_eq(t,v) != INCOMP.
  if (t_atom && !u_atom && term_tag(u) == TAG_CTR) {
    u32 n = term_ctr_n(u);
    for (u32 i = 0; i < n; i++) {
      Term v = term_ctr_at(u, i);
      u8 vok = 0; GjAtom av = gj_term_atom(v, &vok);
      if (vok) {
        int r = gj_atom_less_eq(m, at, av);
        if (r != GJ_INCOMP) return GJ_STRICT;
      }
    }
    return GJ_INCOMP;
  }

  // t compound, u atom: symmetric -- t > u, so "t <= u" is not provable.
  if (!t_atom && u_atom) return GJ_INCOMP;

  // Both compound: compare heads.
  if (term_tag(t) == TAG_CTR && term_tag(u) == TAG_CTR) {
    u32 lt = term_ext(t), lu = term_ext(u);
    if (lt == lu) {
      u32 nt = term_ctr_n(t), nu = term_ctr_n(u);
      if (nt != nu) return GJ_INCOMP;
      // left-to-right; at the first differing pair recurse with lessIn.
      for (u32 i = 0; i < nt; i++) {
        Term ct = term_ctr_at(t, i), cu = term_ctr_at(u, i);
        if (kbo_eq(ct, cu)) continue;
        int r = gj_less_in(m, cfg, ct, cu, cp_vars, n_cp_vars, depth + 1);
        if (r == GJ_STRICT)    return GJ_STRICT;
        if (r == GJ_NONSTRICT) {
          // The spec unifies the tied arg pair and continues; we can
          // only continue safely if the remaining args make the verdict.
          // Conservatively, a Nonstrict (could be equal) at this arg
          // means we need the SAME pair to also satisfy the converse to
          // continue; without a unifier we report INCOMP unless this is
          // the last differing arg AND it is provably tied.  Since
          // kbo_eq already filtered equal args, a Nonstrict here that is
          // not Strict means "<= but maybe =", so continuing requires the
          // arg to be forced-equal.  We treat Nonstrict as "continue only
          // if no later arg differs"; otherwise INCOMP (KEEP-safe).
          u8 later_diff = 0;
          for (u32 j = i + 1; j < nt; j++) {
            if (!kbo_eq(term_ctr_at(t, j), term_ctr_at(u, j))) {
              later_diff = 1; break;
            }
          }
          if (later_diff) return GJ_INCOMP;
          return GJ_NONSTRICT;
        }
        return GJ_INCOMP;   // INCOMP at first differing arg
      }
      return GJ_NONSTRICT;  // all args equal (shouldn't reach: kbo_eq above)
    }
    // different heads, sizes equal: precedence decides.
    u32 pt = (lt < cfg->n_labels) ? cfg->precedence[lt] : 0;
    u32 pu = (lu < cfg->n_labels) ? cfg->precedence[lu] : 0;
    if (pt < pu) return GJ_STRICT;     // t's head precedes u's -> t < u
    return GJ_INCOMP;
  }

  return GJ_INCOMP;
}

// Does variable id `id` occur in `t`?  (Local copy so GJ does not depend
// on the ATP_ORDERED_REWRITE/ATP_MNF build flags.)
static int gj_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) if (gj_has_var(term_ctr_at(t, i), id)) return 1;
      return 0;
    }
    default: return 0;
  }
}

// Is every TAG_FVR variable of `sub` also a variable of `sup`?  Used to
// reject malformed (extra-variable) rule applications during GJ rewrite.
static u8 gj_vars_subset(Term sub, Term sup) {
  switch (term_tag(sub)) {
    case TAG_FVR: return gj_has_var(sup, term_ext(sub)) ? 1 : 0;
    case TAG_CTR: {
      u32 n = term_ctr_n(sub);
      for (u32 i = 0; i < n; i++)
        if (!gj_vars_subset(term_ctr_at(sub, i), sup)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// ── C-parameterised rewriting (gj_spec.md C/D) ──────────────────────
// A rule l->r fires under model C at a matched position iff
// (l-image) >=_C (r-image) -- i.e. gj_less_in(C, r-image, l-image) is
// GJ_STRICT or GJ_NONSTRICT -- AND the two images are syntactically
// distinct.  Oriented rules (l > r unconditionally) also satisfy this,
// so the single test suffices for both.  Returns the rewritten term;
// sets *fired on success.
// WM DreieckOK (Grundzusammenfuehrung.c:235-240): rule LHS `l` is
// STRICTLY more general than the protection anchor `t0` -- some sigma
// maps l onto t0, and none maps t0 onto l (proper encompassment at the
// root).  Root steps on a protected side of a fact-level GZ test are
// admissible only through such rules, the well-foundedness side
// condition of the AHL-2003 ground-joinability redundancy proof.
static u8 atp_gj_strictly_more_general(Term rule_lhs, Term anchor) {
  RewriteSubst onto = {{0}};
  if (!thvm_match(rule_lhs, anchor, &onto)) return 0;
  RewriteSubst back = {{0}};
  return thvm_match(anchor, rule_lhs, &back) ? 0u : 1u;
}

// `protect`: nonzero only at the ROOT of a protected side during a
// fact-level (backward/forward -gj) GZ test -- the victim's ORIGINAL
// side, anchoring the Dreieck gate (WM MO_schuetzeTerm +
// NF_geschuetzteNormalformRE, NFBildung.c:767-779: "bei toplevel
// werden nur Regeln/Gleichungen verwendet, die in der Dreiecksordnung
// kleiner sind").  Child recursion clears it: only root steps are
// gated.  The forward CP-drop path always passes 0.
// When set (demand-driven driver only), globally ORIENTED rules fire
// without the per-step gj_less_in gate: an oriented instance strictly
// decreases under every ground order (KBO substitution stability), so
// the step is valid under every extension of the partial model.  The
// partial-model lex cannot see this (it reasons from atom relations
// alone), which otherwise blocks precedence-oriented rules until the
// variables are ordered.
static u8 g_gj_orient_free = 0;

// ── Per-rule GJ gate flags (pure-speed, verdict-identical) ──────────
// Two rule-shape facts the per-step gates re-derive on every candidate:
//   VARSOK:   vars(rhs) ⊆ vars(lhs) -- the well-formedness invariant.
//     When it holds, vars(rhsσ) ⊆ vars(lhsσ) = vars(redex) for EVERY
//     match σ, so the per-step gj_vars_subset(reduct, t) walk (it was
//     ~8% of a LAT GJ profile) is provably true and skippable.
//   SIZENEUT: the KBO weight form of lhs minus rhs is identically zero
//     (c0 diff 0, every var coefficient 0 -- e.g. commutativity).
//     Then gj_size_less_in returns 0 ("sizes forced equal") for every
//     instance and model, so the gate can jump straight to
//     gj_lex_less_in, skipping the two gj_form_addto full-term walks
//     that dominate comm-heavy GJ profiles (LAT: 1053/5008 samples).
// Cache keyed exactly like the GJ rule index (owner state, n_rules,
// r_revision -- the established validity key for per-slot rule data).
#define GJ_RF_COMPUTED   1u
#define GJ_RF_VARSOK     2u
#define GJ_RF_SIZENEUT   4u
static u8        *g_gj_rflags       = NULL;
static u32        g_gj_rflags_cap   = 0;
static u32        g_gj_rflags_n     = 0;
static u32        g_gj_rflags_rev   = 0;
static const AtpState *g_gj_rflags_owner = NULL;

// Does `t` contain a variable with id >= KBO_MAX_VAR (outside the
// gj_form_addto coefficient table)?  Such rules are excluded from the
// SIZENEUT shortcut (the form is not fully representable).
static u8 gj_has_big_var(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) >= KBO_MAX_VAR ? 1u : 0u;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        if (gj_has_big_var(term_ctr_at(t, i))) return 1u;
      return 0u;
    }
    default: return 0u;
  }
}

static u8 gj_rule_flags(AtpState *s, u32 i) {
  if (g_gj_rflags_owner != s || g_gj_rflags_n != s->n_rules ||
      g_gj_rflags_rev != s->r_revision) {
    if (s->n_rules > g_gj_rflags_cap) {
      u32 cap = g_gj_rflags_cap ? g_gj_rflags_cap : 256u;
      while (cap < s->n_rules) cap *= 2u;
      u8 *nf = (u8 *)realloc(g_gj_rflags, cap);
      if (nf == NULL) return 0u;      // OOM: no flags -> full gates run
      g_gj_rflags = nf;
      g_gj_rflags_cap = cap;
    }
    memset(g_gj_rflags, 0, s->n_rules);
    g_gj_rflags_owner = s;
    g_gj_rflags_n     = s->n_rules;
    g_gj_rflags_rev   = s->r_revision;
  }
  u8 f = g_gj_rflags[i];
  if (f & GJ_RF_COMPUTED) return f;
  f = GJ_RF_COMPUTED;
  Term l = s->lhs[i], r = s->rhs[i];
  if (gj_vars_subset(r, l)) f |= GJ_RF_VARSOK;
  if (!gj_has_big_var(l) && !gj_has_big_var(r)) {
    long long c0 = 0;
    long long var_coeff[KBO_MAX_VAR];
    for (u32 k = 0; k < KBO_MAX_VAR; k++) var_coeff[k] = 0;
    gj_form_addto(l, +1, s->kbo, &c0, var_coeff);
    gj_form_addto(r, -1, s->kbo, &c0, var_coeff);
    u8 neutral = (c0 == 0);
    for (u32 k = 0; neutral && k < KBO_MAX_VAR; k++)
      if (var_coeff[k] != 0) neutral = 0;
    if (neutral) f |= GJ_RF_SIZENEUT;
  }
  g_gj_rflags[i] = f;
  return f;
}

// Indexed twin of gj_rewrite_step's top-of-`t` rule scan.  Returns
//   1 -> a rule fired (*out = reduct)
//   0 -> NO rule fires at the top of t (index is authoritative)
//   2 -> bail (over-deep flatten / candidate-buffer overflow): caller
//        must run the exact linear top scan instead.
// Matching is model-INDEPENDENT, so the discrimination tree yields the
// same structural candidates the linear scan would thvm_match; gating
// them (exclude / protect / gj_vars_subset / kbo_eq / gj_less_in) in
// ASCENDING rule order fires the IDENTICAL rule -- byte-parity by
// construction, with the per-step cost cut from O(R) to O(candidates).
static u8 gj_try_top_indexed(AtpState *s, const GjModel *m, Term t,
                             const u32 *cp_vars, u32 n_cp_vars,
                             Term protect, Term *out) {
  const KboConfig *cfg = s->kbo;
  g_atp_gjp_top_q++;
  if (s->gj_rule_index == NULL) s->gj_rule_index = atp_ri_new();
  if (s->gj_rule_index->n_rules_built != s->n_rules
      || s->gj_rule_index->built_revision != s->r_revision) {
    atp_ri_gj_rebuild(s);
  }
  static Term gj_flat[ATP_RI_FLAT_CAP];
  static u32  gj_subsz[ATP_RI_FLAT_CAP];
  static u32  gj_flatsym[ATP_RI_FLAT_CAP];
  g_atp_ri_ix      = s->gj_rule_index;
  g_atp_ri_flat    = gj_flat;
  g_atp_ri_subsz   = gj_subsz;
  g_atp_ri_flatsym = gj_flatsym;
  g_atp_ri_lhs     = s->lhs;
  for (u32 k = 0; k < ATP_DTREE_MAXVARS; k++) g_atp_ri_star[k] = ATP_DTREE_NIL;
  u32 flatlen = 0u; u8 folded = 0u;
  if (!atp_ri_flatten(t, gj_flat, gj_subsz, gj_flatsym, &folded,
                      ATP_RI_FLAT_CAP, &flatlen))
    return 2u;                                  // over-deep: linear fallback
  g_atp_ri_query_folded = folded;
  u32 ncand = atp_ri_query_pos_all(0u);
  if (g_atp_ri_ncand >= ATP_RI_MAXCAND) return 2u;   // buffer full: fallback
  // Insertion-sort candidates ascending by rule index = the linear scan's
  // i = 0..n_rules order, so the first that passes the gate is the one the
  // scan would have fired.
  for (u32 a = 1u; a < ncand; a++) {
    u32 v = g_atp_ri_cand[a], b = a;
    while (b > 0u && g_atp_ri_cand[b - 1u] > v) {
      g_atp_ri_cand[b] = g_atp_ri_cand[b - 1u]; b--;
    }
    g_atp_ri_cand[b] = v;
  }
  g_atp_gjp_cand += ncand;
  for (u32 c = 0; c < ncand; c++) {
    u32 i = g_atp_ri_cand[c];
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;
    if (i == s->gj_exclude) continue;
    if (protect != 0 && !atp_gj_strictly_more_general(s->lhs[i], protect))
      continue;
    RewriteSubst subst = {{0}};
    if (!thvm_match(s->lhs[i], t, &subst)) continue;   // index over-approx
    g_atp_gjp_match++;
    Term reduct = thvm_subst_apply(s->rhs[i], &subst);
    // Verdict-identical gate reordering (see gj_rule_flags):
    //   * VARSOK proves gj_vars_subset for every match -- skip the walk.
    //   * An ORIENTED rule has lσ > rσ strictly (KBO substitution
    //     stability), so reduct != t is guaranteed -- the orient-free
    //     fire can precede the kbo_eq no-op check.
    //   * SIZENEUT proves gj_size_less_in == 0 (sizes forced equal) for
    //     every instance/model -- jump straight to the lex walk.
    u8 rf = gj_rule_flags(s, i);
    if (!(rf & GJ_RF_VARSOK) && !gj_vars_subset(reduct, t)) continue;
    if (g_gj_orient_free && s->r_orient[i]) { *out = reduct; return 1u; }
    if (kbo_eq(reduct, t)) { g_atp_gjp_noop++; continue; }
    g_atp_gjp_gate++;
    int r = (rf & GJ_RF_SIZENEUT)
        ? gj_lex_less_in(m, cfg, reduct, t, cp_vars, n_cp_vars, 0)
        : gj_less_in(m, cfg, reduct, t, cp_vars, n_cp_vars, 0);
    if (r == GJ_STRICT || r == GJ_NONSTRICT) {
      g_atp_gjp_gate_pass++;
      *out = reduct;
      return 1u;
    }
  }
  return 0u;
}

static Term gj_rewrite_step(AtpState *s, const GjModel *m, Term t,
                            const u32 *cp_vars, u32 n_cp_vars, u8 *fired,
                            Term protect) {
  const KboConfig *cfg = s->kbo;
  if (protect == 0 && gj_nf_hit(t)) { *fired = 0; return t; }
  // Discrimination-tree top scan: byte-parity with the linear scan by
  // construction (candidates gated in ascending rule order), measured
  // trajectory-identical on GRP753-1/LAT138-1 tickers and 3.6-10x
  // faster on push-GJ (2026-07-11).  Default ON; THVM_ATP_GJ_INDEX=0
  // is the kill switch.  The default WM-parity path never runs GJ, so
  // this only affects the opt-in GroundJoin/BackwardGroundJoin modes.
  static int gjidx = -1;
  if (gjidx < 0) {
    const char *e = getenv("THVM_ATP_GJ_INDEX");
    gjidx = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  u8 did_top = 0u;
  if (gjidx) {
    Term red = 0;
    u8 rc = gj_try_top_indexed(s, m, t, cp_vars, n_cp_vars, protect, &red);
    if (rc == 1u) { *fired = 1; return red; }
    if (rc == 0u) did_top = 1u;       // index authoritative: no top redex
    // rc == 2u: bail -> run the exact linear top scan below
  }
  if (!did_top)
  for (u32 i = 0; i < s->n_rules; i++) {
    // A well-formed rewrite rule never has a bare-variable LHS (it would
    // match and "rewrite" everything).  Skip such malformed rules so the
    // GJ test is robust even if handed an ill-formed rule set.
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;
    // Fact-level GZ test: the victim may not reduce itself (WM
    // DarfNichtReduzieren, Hauptkomponenten.c:267/:287, consumed in
    // MatchOperationen.c:641/742/1660/1697).
    if (i == s->gj_exclude) continue;
    // Dreieck root gate on a protected side.
    if (protect != 0 && !atp_gj_strictly_more_general(s->lhs[i], protect)) {
      continue;
    }
    RewriteSubst subst = {{0}};
    if (thvm_match(s->lhs[i], t, &subst)) {
      Term reduct = thvm_subst_apply(s->rhs[i], &subst);
      // Defense-in-depth: a well-formed rewrite rule has vars(rhs) subset
      // of vars(lhs), so a successful match binds every rhs variable and
      // `reduct` is variable-free over the rule's own variables.  If the
      // rule introduces an extra variable (malformed input), `reduct`
      // would carry an unbound variable and corrupt the join comparison.
      // Skip such a step: every variable of `reduct` must also occur in
      // `t` (the term we are rewriting).  No-op for well-formed rules.
      // Gate order + flag shortcuts mirror gj_try_top_indexed exactly
      // (verdict-identical; see gj_rule_flags).
      u8 rf = gj_rule_flags(s, i);
      if (!(rf & GJ_RF_VARSOK) && !gj_vars_subset(reduct, t)) continue;
      if (g_gj_orient_free && s->r_orient[i]) {
        *fired = 1;
        return reduct;
      }
      if (kbo_eq(reduct, t)) continue;     // no-op image
      // fire iff t (=l-image) >=_C reduct (=r-image), i.e.
      // gj_less_in(reduct, t) is defined (reduct <= t under C).
      int r = (rf & GJ_RF_SIZENEUT)
          ? gj_lex_less_in(m, cfg, reduct, t, cp_vars, n_cp_vars, 0)
          : gj_less_in(m, cfg, reduct, t, cp_vars, n_cp_vars, 0);
      if (r == GJ_STRICT || r == GJ_NONSTRICT) {
        *fired = 1;
        return reduct;
      }
    }
  }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) {
      if (protect == 0) gj_nf_add(t);
      *fired = 0;
      return t;
    }
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      u8 sub_fired = 0;
      Term rc = gj_rewrite_step(s, m, children[i], cp_vars, n_cp_vars,
                                &sub_fired, 0);
      if (sub_fired) {
        children[i] = rc;
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  if (protect == 0) gj_nf_add(t);
  *fired = 0;
  return t;
}

// Normalise `t` under model C with C-parameterised steps (step_cap).
// `protect` (0 = none) anchors the Dreieck root gate for the whole
// pass -- WM's MO_schuetzeTerm spans the entire NF_NormalformRE call.
// (WM additionally LIFTS protection for deeper instantiation levels
// once a pass reduced the side -- a permissive refinement; keeping the
// gate for the whole fact test only under-sterilizes, never over.)
static Term gj_normalize(AtpState *s, const GjModel *m, Term t,
                         const u32 *cp_vars, u32 n_cp_vars, u32 step_cap,
                         Term protect) {
  g_atp_gjp_norms++;
  gj_sess_arm(m, s->kbo, cp_vars, n_cp_vars);
  // Fresh NF-memo generation (see gj_nf_hit): the verdict function is
  // fixed only within this normalize.  On u32 wrap, clear the stamp
  // table so ancient generations cannot alias the new one.
  if (++g_gj_nf_cur == 0u) {
    memset(g_gj_nf_gen, 0, sizeof g_gj_nf_gen);
    g_gj_nf_cur = 1u;
  }
  g_gj_nf_n = 0u;
  for (u32 k = 0; k < step_cap; k++) {
    u8 fired = 0;
    Term t2 = gj_rewrite_step(s, m, t, cp_vars, n_cp_vars, &fired, protect);
    if (!fired) { gj_sess_disarm(); return t; }
    g_atp_gjp_norm_steps++;
    t = t2;
  }
  gj_sess_disarm();
  return t;
}

// ── Model-independent GJ pre-normalization ─────────────────────────
// A rewrite step by an ORIENTED rule (r_orient[i]: lhs > rhs in KBO on
// open terms) strictly decreases EVERY ground instance -- KBO is stable
// under substitution -- so it is sound under every branch model C.
// gj_cover can therefore normalize both CP sides ONCE with the oriented
// subset of R and share the result across all ordered-set-partition
// branches; only unorientable-equation steps stay model-dependent.
// Most joinable CPs exit at the post-pass kbo_eq without touching the
// branch enumeration at all -- this is what makes the forward CP-drop
// GJ (THVM_ATP_GROUND_JOIN) affordable (the raw form re-normalized the
// raw pair under each of the ~B(n_vars) models).  Applied ONLY on the
// forward CP path (no protect anchors, no exclude): the fact-level WM
// -gj test keeps its exact WM-faithful evaluation order.
static Term gj_prenorm_step(AtpState *s, Term t, u8 *fired) {
  for (u32 i = 0; i < s->n_rules; i++) {
    if (!s->r_orient[i]) continue;
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;
    RewriteSubst subst = {{0}};
    if (thvm_match(s->lhs[i], t, &subst)) {
      Term reduct = thvm_subst_apply(s->rhs[i], &subst);
      if (!gj_vars_subset(reduct, t)) continue;
      *fired = 1;
      return reduct;
    }
  }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      u8 sub_fired = 0;
      Term rc = gj_prenorm_step(s, children[i], &sub_fired);
      if (sub_fired) {
        children[i] = rc;
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

static Term gj_prenorm(AtpState *s, Term t) {
  for (u32 k = 0; k < ATP_GJ_NORM_CAP; k++) {
    u8 fired = 0;
    Term t2 = gj_prenorm_step(s, t, &fired);
    if (!fired) return t;
    t = t2;
  }
  return t;
}

// Join the two CP sides under model C: normalise both and compare.
// The per-side protection anchors live on AtpState (constant for one
// fact-level GZ test; 0 on the forward CP-drop path): the recursion in
// gj_cover never swaps side order, so side 0 is always the victim-LHS
// face and side 1 the victim-RHS face.
static u8 gj_joins_under(AtpState *s, const GjModel *m, Term lhs, Term rhs,
                         const u32 *cp_vars, u32 n_cp_vars) {
  g_atp_gjp_models++;
  Term nl = gj_normalize(s, m, lhs, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP,
                         s->gj_protect_l);
  Term nr = gj_normalize(s, m, rhs, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP,
                         s->gj_protect_r);
  return kbo_eq(nl, nr);
}

// ── Coverage driver (gj_spec.md C) ──────────────────────────────────
// We must verify joinability under EVERY total preorder of the CP's
// atoms (variables + relevant constants).  Coverage (P3) is realised by
// EXHAUSTIVE enumeration, structured exactly as Twee's groundJoin:
//
//  * The variables are partitioned by a total preorder (ordered set
//    partition).  When two variables fall in the SAME class they are
//    forced EQUAL in SIZE; for KBO that is genuinely an `=` case, and
//    Twee step 5 handles it by UNIFYING the equal variables and
//    recursing on the smaller CP.  We do exactly that: collapse each
//    multi-variable class to a single representative (lowest id), apply
//    the var->representative substitution to both CP sides, and recurse
//    gj_cover on the reduced CP.  The recursion re-covers every sub-
//    ordering (incl. further ties and constant placements) of the
//    representatives, so the `=` region is fully covered.  Termination:
//    each recursion strictly drops the variable count.
//
//  * When all variables are in DISTINCT classes (a strict total order on
//    the variables), there is no `=` to discharge.  We then interleave
//    the relevant CONSTANTS into that strict variable chain in every
//    order (constants among themselves are pinned by their fixed
//    weights), build the resulting Model, and run the SYMBOLIC join:
//    gj_less_in reasons about ALL grounding sigma consistent with the
//    order (including the `<=` no-op steps), which is what lets the AC
//    pair join where a single ground representative could not (F.1).
//
// Soundness: gj_less_in never OVER-approximates >=_C (P2), so no illegal
// rewrite ever fires; the enumeration covers every ground ordering of
// vars (ties via unify-recurse, strict via models) and every constant
// placement (P3); any budget overflow / OOM / unjoined case returns 0
// (KEEP).  Constants of different weight are never asserted equal (the
// model rejects such an unsatisfiable preorder), so the unsound
// "merge two constants" never happens.

// Collect distinct nullary-constant labels in `t`.
static u8 gj_collect_consts(Term t, u32 *labs, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      if (k == 0) {
        u32 lab = term_ext(t);
        for (u32 i = 0; i < *n; i++) if (labs[i] == lab) return 1;
        if (*n >= cap) return 0;
        labs[(*n)++] = lab;
        return 1;
      }
      for (u32 i = 0; i < k; i++)
        if (!gj_collect_consts(term_ctr_at(t, i), labs, n, cap)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// Build a Model from a STRICT order on variables interleaved with the
// CP's constants.  `atoms[0..n_atoms)` lists the model atoms; `cls[i]`
// is atom i's size-rank (class).  Distinct VARIABLES must have distinct
// classes here (the caller guarantees a strict variable order).
// Returns 0 (reject this preorder as unsatisfiable) if it ties two
// constants of different weight, or orders two constants against their
// fixed-weight order -- no ground sigma realises such a C, so skipping
// it loses no coverage (it vacuously joins).
static u8 gj_build_model(GjModel *m, const GjAtom *atoms, u32 n_atoms,
                         const u32 *cls, const KboConfig *cfg) {
  for (u32 i = 0; i < n_atoms; i++) {
    if (!atoms[i].is_const) continue;
    for (u32 j = 0; j < n_atoms; j++) {
      if (!atoms[j].is_const) continue;
      u32 si = gj_const_size(cfg, atoms[i].id);
      u32 sj = gj_const_size(cfg, atoms[j].id);
      if (cls[i] < cls[j] && si > sj) return 0;
      if (cls[i] > cls[j] && si < sj) return 0;
      if (cls[i] == cls[j] && si != sj) return 0;
    }
  }
  m->n = n_atoms;
  for (u32 i = 0; i < n_atoms; i++) {
    m->atom[i]  = atoms[i];
    m->major[i] = cls[i];
  }
  return 1;
}

// Given a STRICT total order on the variables (var at atom-index i has
// rank var_rank[i]), interleave the constants among the variables in
// every position and run the symbolic join under each resulting Model.
// Returns 1 iff all join.
//
// Coarse placement: variable at rank r gets coarse slot 2r+1.  Each
// constant is independently placed into a coarse slot in [0, 2*nvars]:
//   even slot 2k  = strictly between var-rank-(k-1) and var-rank-k
//                   (slot 0 = below all vars, 2*nvars = above all);
//   odd slot 2k+1 = tied (equal size) with the variable at rank k.
// This covers every position of a constant relative to the strict
// variable chain.  Two or more constants landing in the SAME even slot
// (the same gap) are then SUB-ORDERED by their fixed weights: distinct
// weights -> distinct majors in weight order; equal weights -> tied
// (the true ground fact).  This closes the coverage gap that a single
// shared even slot would otherwise create (two different-weight
// constants in one gap must still be tested in their forced order).
// Constants tied with a variable (odd slot) keep that exact major (a
// ground term genuinely can have a variable image's size).
// gj_build_model rejects placements that contradict the constants'
// fixed weight order (an unsatisfiable preorder -> skipped, vacuously
// joinable, no coverage lost).
//
// Final majors are produced by sorting all atoms on the key
// (coarse_slot, weight_if_const_in_even_slot) and assigning a dense
// rank that increments only when the key strictly increases, so equal
// keys share a major (a tie) and strict keys get distinct majors.
static u8 gj_cover_consts(AtpState *s, Term lhs, Term rhs,
                          const GjAtom *atoms, u32 n_atoms,
                          const u32 *var_rank,  // var_rank[i] for var atoms
                          u32 n_vars_model,
                          const u32 *cp_vars, u32 n_cp_vars, u32 *budget) {
  const KboConfig *cfg = s->kbo;
  u32 const_idx[ATP_GJ_MAX_ATOMS];
  u32 nc = 0;
  for (u32 i = 0; i < n_atoms; i++) if (atoms[i].is_const) const_idx[nc++] = i;

  u32 grid = 2u * n_vars_model + 1u;     // coarse const slot range [0, grid)
  u32 place[ATP_GJ_MAX_ATOMS];
  for (u32 c = 0; c < nc; c++) place[c] = 0;

  for (;;) {
    if (*budget == 0) return 0;
    (*budget)--;

    // coarse slot per atom
    u32 coarse[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++)
      if (!atoms[i].is_const) coarse[i] = 2u * var_rank[i] + 1u;
    for (u32 c = 0; c < nc; c++) coarse[const_idx[c]] = place[c];

    // sub-order key: for a constant in an EVEN coarse slot, break ties
    // between same-slot constants by weight; everything else keeps a 0
    // secondary key (vars, and constants tied with a var, are pinned to
    // their slot).
    long long key2[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++) {
      if (atoms[i].is_const && (coarse[i] % 2u) == 0u)
        key2[i] = (long long)gj_const_size(cfg, atoms[i].id);
      else
        key2[i] = 0;
    }

    // dense-rank by (coarse, key2): sort indices, assign majors.
    u32 ord[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++) ord[i] = i;
    for (u32 i = 1; i < n_atoms; i++) {
      u32 k = ord[i]; int j = (int)i - 1;
      while (j >= 0 &&
             (coarse[ord[j]] > coarse[k] ||
              (coarse[ord[j]] == coarse[k] && key2[ord[j]] > key2[k]))) {
        ord[j + 1] = ord[j]; j--;
      }
      ord[j + 1] = k;
    }
    u32 cls[ATP_GJ_MAX_ATOMS];
    u32 cur = 0;
    for (u32 r = 0; r < n_atoms; r++) {
      if (r > 0) {
        u32 a = ord[r - 1], b = ord[r];
        if (coarse[a] != coarse[b] || key2[a] != key2[b]) cur++;
      }
      cls[ord[r]] = cur;
    }

    GjModel m;
    if (gj_build_model(&m, atoms, n_atoms, cls, cfg)) {
      if (!gj_joins_under(s, &m, lhs, rhs, cp_vars, n_cp_vars)) return 0;
    }

    if (nc == 0) return 1;
    u32 c = 0;
    for (; c < nc; c++) {
      if (++place[c] < grid) break;
      place[c] = 0;
    }
    if (c == nc) return 1;
  }
}

// Recursively cover all total preorders of the CP's variables.  Strategy
// (gj_spec.md C steps 1-5):
//   * Enumerate ordered set partitions of the variables.
//   * A partition with two variables in one class is an `=` case: unify
//     (substitute each tied variable to the class representative) and
//     RECURSE on the smaller CP.
//   * A strict (all-singleton) partition: interleave constants and run
//     the symbolic join (gj_cover_consts).
// Returns 1 iff every case joins.
static u8 gj_cover(AtpState *s, Term lhs, Term rhs,
                   const u32 *var_ids, u32 n_vars, u32 *budget) {
  // Shared model-independent pre-pass (see gj_prenorm): forward CP path
  // only -- the fact-level test's protect/exclude anchors are active
  // there and its evaluation order is WM-pinned.  Runs at every
  // recursion level (the unify case substitutes and re-enters), so tie
  // branches also normalize before enumerating their sub-orders.
  // THVM_ATP_GJ_PRENORM=0 is the A/B kill.
  static int gj_pre = -1;
  if (gj_pre < 0) {
    const char *e = getenv("THVM_ATP_GJ_PRENORM");
    gj_pre = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  u32 pre_vars[ATP_GJ_MAX_VARS];
  if (gj_pre && s->gj_protect_l == 0 && s->gj_protect_r == 0 &&
      s->gj_exclude == ATP_GJ_NO_EXCLUDE) {
    lhs = gj_prenorm(s, lhs);
    rhs = gj_prenorm(s, rhs);
    if (kbo_eq(lhs, rhs)) return 1;
    // The pre-pass can only erase variables; re-collect from the
    // reduced pair so the partition enumeration ranges over what
    // actually remains.
    u32 n_pre = 0;
    if (!atp_gj_collect_vars(lhs, pre_vars, &n_pre, ATP_GJ_MAX_VARS)) return 0;
    if (!atp_gj_collect_vars(rhs, pre_vars, &n_pre, ATP_GJ_MAX_VARS)) return 0;
    var_ids = pre_vars;
    n_vars = n_pre;
  }
  // Re-collect the constants of the (current, possibly substituted) CP.
  u32 const_labs[ATP_GJ_MAX_ATOMS];
  u32 n_consts = 0;
  if (!gj_collect_consts(lhs, const_labs, &n_consts, ATP_GJ_MAX_ATOMS)) return 0;
  if (!gj_collect_consts(rhs, const_labs, &n_consts, ATP_GJ_MAX_ATOMS)) return 0;

  if (n_vars == 0) {
    // Ground CP (after all unifications): build a model over constants
    // only (their order is fixed by weight) and join.  No variable ranks.
    GjAtom atoms[ATP_GJ_MAX_ATOMS];
    u32 n_atoms = 0;
    for (u32 i = 0; i < n_consts; i++) {
      if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
      atoms[n_atoms].is_const = 1; atoms[n_atoms].id = const_labs[i]; n_atoms++;
    }
    u32 dummy_rank[ATP_GJ_MAX_ATOMS] = {0};
    return gj_cover_consts(s, lhs, rhs, atoms, n_atoms, dummy_rank, 0,
                           var_ids, n_vars, budget);
  }

  // Enumerate ordered set partitions of the variables via restricted-
  // growth string cls[] + a permutation of the classes.
  u32 cls[ATP_GJ_MAX_VARS];
  u32 perm[ATP_GJ_MAX_VARS];
  for (u32 i = 0; i < n_vars; i++) cls[i] = 0;

  for (;;) {
    u32 n_cls = 0;
    for (u32 i = 0; i < n_vars; i++) if (cls[i] + 1 > n_cls) n_cls = cls[i] + 1;

    for (u32 i = 0; i < n_cls; i++) perm[i] = i;
    for (;;) {
      if (*budget == 0) return 0;
      u32 ocls[ATP_GJ_MAX_VARS];
      for (u32 i = 0; i < n_vars; i++) ocls[i] = perm[cls[i]];

      if (n_cls < n_vars) {
        // Some class holds >=2 variables -> `=` case.  Unify each class
        // to its representative (lowest-id var in the class) and recurse.
        (*budget)--;
        RewriteSubst sub = {{0}};
        u32 rep_of_class[ATP_GJ_MAX_VARS];
        for (u32 c = 0; c < n_cls; c++) rep_of_class[c] = (u32)-1;
        // Representative = the smallest var id in the class.
        for (u32 i = 0; i < n_vars; i++) {
          u32 c = ocls[i];
          if (rep_of_class[c] == (u32)-1 || var_ids[i] < rep_of_class[c])
            rep_of_class[c] = var_ids[i];
        }
        u32 new_vars[ATP_GJ_MAX_VARS];
        u32 n_new = 0;
        u8 sub_ok = 1;
        for (u32 i = 0; i < n_vars; i++) {
          u32 rep = rep_of_class[ocls[i]];
          if (var_ids[i] != rep) {
            if (var_ids[i] >= REWRITE_MAX_VAR) { sub_ok = 0; break; }
            sub.bindings[var_ids[i]] = term_new_fvr(rep);
          }
        }
        // The reduced variable set = the representatives (one per class).
        for (u32 c = 0; c < n_cls && sub_ok; c++) {
          u8 seen = 0;
          for (u32 k = 0; k < n_new; k++) if (new_vars[k] == rep_of_class[c]) seen = 1;
          if (!seen) new_vars[n_new++] = rep_of_class[c];
        }
        if (sub_ok) {
          Term sl = thvm_subst_apply(lhs, &sub);
          Term sr = thvm_subst_apply(rhs, &sub);
          if (!gj_cover(s, sl, sr, new_vars, n_new, budget)) return 0;
        } else {
          return 0;   // can't form the substitution -> KEEP (sound)
        }
      } else {
        // Strict order on the variables: var i sits at rank ocls[i].
        // Build the atom list (vars + consts) and interleave constants.
        GjAtom atoms[ATP_GJ_MAX_ATOMS];
        u32 var_rank[ATP_GJ_MAX_ATOMS];
        u32 n_atoms = 0;
        for (u32 i = 0; i < n_vars; i++) {
          if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
          atoms[n_atoms].is_const = 0; atoms[n_atoms].id = var_ids[i];
          var_rank[n_atoms] = ocls[i];
          n_atoms++;
        }
        for (u32 i = 0; i < n_consts; i++) {
          if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
          atoms[n_atoms].is_const = 1; atoms[n_atoms].id = const_labs[i];
          var_rank[n_atoms] = 0;
          n_atoms++;
        }
        if (!gj_cover_consts(s, lhs, rhs, atoms, n_atoms, var_rank, n_vars,
                             var_ids, n_vars, budget))
          return 0;
      }

      // next permutation of classes
      if (n_cls < 2) break;
      u32 a = n_cls - 1;
      while (a > 0 && perm[a - 1] >= perm[a]) a--;
      if (a == 0) break;
      u32 b = n_cls - 1;
      while (perm[b] <= perm[a - 1]) b--;
      u32 tmp = perm[a - 1]; perm[a - 1] = perm[b]; perm[b] = tmp;
      u32 lo = a, hi = n_cls - 1;
      while (lo < hi) { tmp = perm[lo]; perm[lo] = perm[hi]; perm[hi] = tmp; lo++; hi--; }
    }

    // next restricted-growth partition
    u32 i = n_vars;
    while (i > 0) {
      i--;
      u32 max_prefix = 0;
      for (u32 j = 0; j < i; j++) if (cls[j] + 1 > max_prefix) max_prefix = cls[j] + 1;
      if (cls[i] < max_prefix) {
        cls[i]++;
        for (u32 j = i + 1; j < n_vars; j++) cls[j] = 0;
        break;
      }
      if (i == 0) return 1;              // exhausted -> all cases joined
      cls[i] = 0;
    }
  }
}

// ── Demand-driven coverage driver (Twee Join.groundJoin shape) ──────
// gj_cover enumerates every ordered set partition of the atoms UP
// FRONT (75 leaf models for 4 variables), re-joining under each.  Twee
// refines the order lazily instead: normalize under a PARTIAL preorder
// -- gj_less_in is conservative for atoms it cannot relate, so only
// steps valid under EVERY total extension fire -- and the moment the
// two normal forms meet, the WHOLE subtree of extensions is proved
// joinable by those same (monotone) steps.  Branching inserts one atom
// placement at a time, and only when the partial-order normalize is
// stuck; most joinable CPs settle after 0-2 placements, which is what
// collapses the ~Bell(n) leaf count to a handful of nodes.
//
// Coverage: a node's placement loop inserts atom x at every position
// of the current K-level chain -- strictly below/between/above every
// level (even slots) and tied with every level (odd slots) -- so the
// sub-branches partition all total preorders refining the node's model
// by x's relation to the placed atoms.  Atoms no longer OCCURRING in
// the normalized pair are never placed: their order cannot influence
// further steps on this pair, and every extension over them joins via
// the same steps.  Unsatisfiable const placements (contradicting the
// fixed weight order) are skipped exactly as gj_build_model rejects
// them: no ground sigma realises such a preorder.  A tie between two
// VARIABLES unifies to the placed representative and recurses on the
// substituted pair (the `=`-case treatment gj_cover uses).  Soundness
// rests on the same primitives (gj_normalize / gj_less_in, P2); every
// stuck / overflow / budget case returns 0 = KEEP.
static int gj_has_const(Term t, u32 lab) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n == 0) return term_ext(t) == lab;
      for (u32 i = 0; i < n; i++)
        if (gj_has_const(term_ctr_at(t, i), lab)) return 1;
      return 0;
    }
    default: return 0;
  }
}

// Incremental gj_build_model consistency: would const x (weight wx) at
// new major mx contradict the fixed weight order against the model's
// placed constants?  `shift_from` is the level index an even placement
// opens (existing majors >= shift_from read +1); pass 0xffffffff for a
// tie placement (no shift).
static u8 gj_dd_const_ok(const GjModel *m, const KboConfig *cfg,
                         u32 wx, u32 mx, u32 shift_from) {
  for (u32 i = 0; i < m->n; i++) {
    if (!m->atom[i].is_const) continue;
    u32 mi = m->major[i] + ((m->major[i] >= shift_from) ? 1u : 0u);
    u32 wi = gj_const_size(cfg, m->atom[i].id);
    if (mx < mi && wx > wi) return 0;
    if (mx > mi && wx < wi) return 0;
    if (mx == mi && wx != wi) return 0;
  }
  return 1;
}

static u8 gj_join_dd(AtpState *s, Term lhs, Term rhs, const GjModel *m,
                     const GjAtom *pend, u32 n_pend, u32 *budget,
                     u32 depth) {
  if (depth > ATP_GJ_MAX_ATOMS) return 0;
  if (*budget == 0) return 0;
  (*budget)--;

  u32 cp_vars[ATP_GJ_MAX_VARS];
  u32 n_cp = 0;
  if (!atp_gj_collect_vars(lhs, cp_vars, &n_cp, ATP_GJ_MAX_VARS)) return 0;
  if (!atp_gj_collect_vars(rhs, cp_vars, &n_cp, ATP_GJ_MAX_VARS)) return 0;
  g_gj_orient_free = 1;
  Term nl = gj_normalize(s, m, lhs, cp_vars, n_cp, ATP_GJ_NORM_CAP, 0);
  Term nr = gj_normalize(s, m, rhs, cp_vars, n_cp, ATP_GJ_NORM_CAP, 0);
  g_gj_orient_free = 0;
  if (kbo_eq(nl, nr)) return 1;

  // Branches recurse on the RAW (post-substitution) pair, not on this
  // node's normal forms: mid-completion R is not confluent, so a step
  // valid under the partial model can commit to a path the refined
  // leaf normalize would not take, losing joins gj_cover's from-raw
  // leaf test finds.  The NFs above serve only the early exit.  The
  // occurrence filter accordingly tests the raw pair.
  u32 px = n_pend;
  for (u32 i = 0; i < n_pend; i++) {
    u8 occurs = pend[i].is_const
        ? (u8)(gj_has_const(lhs, pend[i].id) || gj_has_const(rhs, pend[i].id))
        : (u8)(gj_has_var(lhs, pend[i].id) || gj_has_var(rhs, pend[i].id));
    if (occurs) { px = i; break; }
  }
  if (px == n_pend) return 0;  // order fully decided for this pair, yet unequal
  GjAtom x = pend[px];
  GjAtom rest[ATP_GJ_MAX_ATOMS];
  u32 n_rest = 0;
  for (u32 i = 0; i < n_pend; i++) if (i != px) rest[n_rest++] = pend[i];

  const KboConfig *cfg = s->kbo;
  u32 wx = x.is_const ? gj_const_size(cfg, x.id) : 0u;
  u32 K = 0;
  for (u32 i = 0; i < m->n; i++) if (m->major[i] + 1u > K) K = m->major[i] + 1u;

  for (u32 p = 0; p <= 2u * K; p++) {
    u32 k = p >> 1;
    if (p & 1u) {
      // Tie with level k.
      if (!x.is_const) {
        u32 v_id = 0; u8 v_found = 0;
        for (u32 i = 0; i < m->n; i++) {
          if (m->major[i] != k || m->atom[i].is_const) continue;
          v_found = 1; v_id = m->atom[i].id; break;
        }
        if (v_found) {
          if (x.id >= REWRITE_MAX_VAR) return 0;
          RewriteSubst sub = {{0}};
          sub.bindings[x.id] = term_new_fvr(v_id);
          Term sl = thvm_subst_apply(lhs, &sub);
          Term sr = thvm_subst_apply(rhs, &sub);
          if (!gj_join_dd(s, sl, sr, m, rest, n_rest, budget, depth + 1))
            return 0;
          continue;
        }
        // Const-only level: a variable's ground image can genuinely
        // share the constants' size -- tie in the model.
      } else if (!gj_dd_const_ok(m, cfg, wx, k, 0xffffffffu)) {
        continue;                       // unsatisfiable -> vacuously joins
      }
      GjModel m2 = *m;
      if (m2.n >= ATP_GJ_MAX_ATOMS) return 0;
      m2.atom[m2.n] = x; m2.major[m2.n] = k; m2.n++;
      if (!gj_join_dd(s, lhs, rhs, &m2, rest, n_rest, budget, depth + 1))
        return 0;
      continue;
    }
    // Strictly below level k (p == 2K: above all levels).
    if (x.is_const && !gj_dd_const_ok(m, cfg, wx, k, k)) continue;
    GjModel m2;
    m2.n = m->n;
    if (m2.n >= ATP_GJ_MAX_ATOMS) return 0;
    for (u32 i = 0; i < m->n; i++) {
      m2.atom[i]  = m->atom[i];
      m2.major[i] = m->major[i] + ((m->major[i] >= k) ? 1u : 0u);
    }
    m2.atom[m2.n] = x; m2.major[m2.n] = k; m2.n++;
    if (!gj_join_dd(s, lhs, rhs, &m2, rest, n_rest, budget, depth + 1))
      return 0;
  }
  return 1;
}

// Top-level: return 1 iff (lhs, rhs) is PROVABLY ground-joinable
// (would-DELETE), 0 otherwise (would-KEEP).  Sound (no false DELETE):
// gj_less_in never over-approximates >=_C (P2), the recursion + strict-
// order/constant-interleave enumeration covers every ground ordering
// (P3), and any uncertainty / budget / overflow returns 0 (KEEP).
static int atp_cp_ground_joinable(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  // No ordering => keep (no GJ verdict can be made).
  if (s->kbo == NULL && s->lpo == NULL && s->rpo == NULL && s->wpo == NULL) return 0;
  // GJ's gj_less_in is implemented for KBO only; under LPO/RPO/WPO the
  // ground-join check is off (caller falls back to keep).
  if (s->lpo != NULL || s->rpo != NULL || s->wpo != NULL) return 0;

  u32 var_ids[ATP_GJ_MAX_VARS];
  u32 n_vars = 0;
  if (!atp_gj_collect_vars(lhs, var_ids, &n_vars, ATP_GJ_MAX_VARS)) return 0;
  if (!atp_gj_collect_vars(rhs, var_ids, &n_vars, ATP_GJ_MAX_VARS)) return 0;

  // THVM_ATP_GJ_BUDGET: gj_cover's branch budget (default 4096, exhaustive).
  // If the over-generating CPs are easy ground-joinable (few branches), a
  // much smaller budget stays faithful while cutting the per-CP cost that
  // makes GroundJoin->True ~100x too slow.
  static int gjb = -1;
  if (gjb < 0) {
    const char *e = getenv("THVM_ATP_GJ_BUDGET");
    gjb = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : (int)ATP_GJ_BRANCH_CAP;
    if (gjb < 1) gjb = ATP_GJ_BRANCH_CAP;
  }
  u32 budget = (u32)gjb;
  // Demand-driven driver (gj_join_dd) on the forward CP path; the
  // fact-level WM -gj test (protect/exclude anchors active) keeps the
  // WM-pinned exhaustive gj_cover.  THVM_ATP_GJ_DD=0 is the A/B kill.
  static int gj_dd = -1;
  if (gj_dd < 0) {
    const char *e = getenv("THVM_ATP_GJ_DD");
    gj_dd = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  if (gj_dd && s->gj_protect_l == 0 && s->gj_protect_r == 0 &&
      s->gj_exclude == ATP_GJ_NO_EXCLUDE) {
    GjAtom pend[ATP_GJ_MAX_ATOMS];
    u32 n_pend = 0;
    for (u32 i = 0; i < n_vars; i++) {
      pend[n_pend].is_const = 0; pend[n_pend].id = var_ids[i]; n_pend++;
    }
    u32 clabs[ATP_GJ_MAX_ATOMS];
    u32 nc = 0;
    if (!gj_collect_consts(lhs, clabs, &nc, ATP_GJ_MAX_ATOMS)) return 0;
    if (!gj_collect_consts(rhs, clabs, &nc, ATP_GJ_MAX_ATOMS)) return 0;
    for (u32 i = 0; i < nc; i++) {
      if (n_pend >= ATP_GJ_MAX_ATOMS) return 0;
      pend[n_pend].is_const = 1; pend[n_pend].id = clabs[i]; n_pend++;
    }
    GjModel m0;
    m0.n = 0;
    // THVM_ATP_GJ_XCHECK: forensic A/B -- run BOTH drivers, report
    // disagreements, let the exhaustive verdict drive the trajectory.
    static int gj_xchk = -1;
    if (gj_xchk < 0) gj_xchk = atp_env_on("THVM_ATP_GJ_XCHECK");
    if (gj_xchk) {
      u32 b2 = budget;
      int vdd = gj_join_dd(s, lhs, rhs, &m0, pend, n_pend, &budget, 0) ? 1 : 0;
      int vcv = gj_cover(s, lhs, rhs, var_ids, n_vars, &b2) ? 1 : 0;
      if (vdd != vcv) {
        char lb[512], rb[512];
        atp_pretty_term(lhs, lb, sizeof lb);
        atp_pretty_term(rhs, rb, sizeof rb);
        fprintf(stderr, "GJXCHK dd=%d cover=%d bleft_dd=%u bleft_cv=%u %s = %s\n",
                vdd, vcv, budget, b2, lb, rb);
      }
      return vcv;
    }
    return gj_join_dd(s, lhs, rhs, &m0, pend, n_pend, &budget, 0) ? 1 : 0;
  }
  return gj_cover(s, lhs, rhs, var_ids, n_vars, &budget) ? 1 : 0;
}

// ── Pop-time ground-joinability CASE-SPLIT (Twee Join.hs port) ──────
// Faithful port of Twee's groundJoin / groundJoinFrom loop
// (src/Twee/Join.hs:173-256 at commit d425378 "CASC", the CASC-30
// build) with the branch machinery of src/Twee/Constraints.hs
// (Branch / addLess / addEquals / branches: 101-187, solve: 278-294,
// weakenModel: 220-234).  Where the legacy gj_cover / gj_join_dd
// drivers enumerate atom placements up front (a branch tree that
// exhausts any budget on the 6-plus-variable assoc-lifted CPs -- and
// gj_cover's ATP_GJ_MAX_VARS=6 collect cap refuses them outright),
// Twee's loop works model-by-model:
//   1. extract ONE total-order model per round from the branch
//      context (solve = topological completion of the accumulated
//      order constraints over the CP's atoms),
//   2. join the CP under that model with model-parameterised
//      rewriting (gj_normalize; a failure is a COUNTEREXAMPLE
//      ordering -> KEEP immediately, Join.hs:194),
//   3. weaken the successful model to the weakest chain that still
//      joins (optimise weakenModel, Join.hs:197-200; delete-only
//      here -- Twee's glue weakening needs the major/minor nonstrict
//      model form its lessIn carries and our size-rank GjModel does
//      not, so we cover fewer orderings per round, never wrong ones),
//   4. conjoin the NEGATION of the weakened chain's literal diagram
//      onto every branch (diag / formAnd, Join.hs:204-208): link
//      c_{j-1} < c_j fails as either c_j < c_{j-1} or c_j = c_{j-1}
//      (negateFormula + branches, Constraints.hs:69-73/:147-150), so
//      the round count is the number of case ORDERINGS examined --
//      Twee's --ground-joining-limit -- not a leaf-placement count,
//   5. an `=` link merges the two atoms; a branch that has merges
//      when the context runs out of models yields an INSTANCE
//      (solve's Right sub, Constraints.hs:283-287): the CP with the
//      tied atoms unified.  Twee re-`consider`s instances through the
//      full pipeline and discards the parent regardless (Twee.hs:
//      539-544; a non-joining instance becomes its own smaller rule);
//      we instead REQUIRE every instance to pop-gj-join recursively
//      and keep the parent when one fails -- a strictly weaker
//      discard (the kept parent subsumes its instances, so no new
//      derivation is created and proof extraction is untouched).
// No simplerThan gate on the model-normalize steps (Twee Join.hs:225
// restricts rewrites to rules simpler than the CP): every gj step is
// descending under the branch model, so each ground instance's join
// stays below its own peak -- the AHL/Martin-Nipkow redundancy
// argument the legacy gj drivers already rely on.
// Soundness: a discard claims every ground instance of the CP joins.
// Every branch model is join-tested with gj_normalize's descending
// model-gated steps (P2: gj_less_in never over-approximates >=_C),
// instance cases are join-tested recursively, and any atom-collect
// overflow / branch-store overflow / cycle / limit hit returns 0 =
// KEEP.  A wrong KEEP costs speed only; a DISCARD is always certified.

#define ATP_PGJ_MAX_BRANCH 4096u
#define ATP_PGJ_MAX_DEPTH  20u

// Diagnostic (THVM_ATP_POP_GJ_DIAG): why a pop-gj case-split returned
// KEEP.  Indices: 0=atom overflow / n>MAX, 1=malloc, 2=limit, 3=cycle,
// 4=counterexample ordering (join under a model failed), 5=branch
// overflow, 6=instance non-join, 7=depth cap.  Behaviour-neutral.
static u64 g_pgj_keep_reason[8] = {0};

typedef struct {
  u8  rep[ATP_GJ_MAX_ATOMS];   // union-collapsed representative index
  u32 less[ATP_GJ_MAX_ATOMS];  // bit j of row i: atom i < atom j
} PgjBranch;

// Twee Atom Ord (Constraints.hs:21 deriving Ord): Constant < Variable,
// then by id.  Decides the representative of an `=` merge (the lesser
// atom, Constraints.hs:172), so var = const substitutes the CONSTANT
// into the instance.
static u8 pgj_atom_before(GjAtom a, GjAtom b) {
  if (a.is_const != b.is_const) return a.is_const ? 1u : 0u;
  return (a.id < b.id) ? 1u : 0u;
}

// Kahn topological order of the branch's less-DAG over all n atoms.
// Returns 0 on a cycle (contradictory branch, Constraints.hs:121-129).
// Tie-break: lowest atom index first -- deterministic where Twee
// shuffles the edge list with a fixed seed (Constraints.hs:290); any
// linear extension is a valid model.  Merged-away atoms are isolated
// vertices here; only merge-free branches use the chain as a model.
static u8 pgj_topo(const PgjBranch *b, u32 n, u8 *chain) {
  u32 indeg[ATP_GJ_MAX_ATOMS] = {0};
  for (u32 i = 0; i < n; i++)
    for (u32 j = 0; j < n; j++)
      if (b->less[i] & (1u << j)) indeg[j]++;
  u32 done = 0, placed = 0;
  while (placed < n) {
    u32 pick = n;
    for (u32 i = 0; i < n; i++)
      if (!(done & (1u << i)) && indeg[i] == 0u) { pick = i; break; }
    if (pick == n) return 0;   // cycle
    chain[placed++] = (u8)pick;
    done |= 1u << pick;
    for (u32 j = 0; j < n; j++)
      if (b->less[pick] & (1u << j)) indeg[j]--;
  }
  return 1;
}

// addLess (Constraints.hs:152-160): assert atom i < atom j.  Indices
// are normalised through rep[] first.  Returns 0 iff contradictory.
static u8 pgj_add_less(PgjBranch *b, u32 n, u32 i, u32 j) {
  i = b->rep[i];
  j = b->rep[j];
  if (i == j) return 0;              // x < x
  b->less[i] |= (1u << j);
  u8 chain[ATP_GJ_MAX_ATOMS];
  return pgj_topo(b, n, chain);      // acyclic = satisfiable order part
}

// addEquals (Constraints.hs:162-176): merge atoms i and j; the lesser
// atom (pgj_atom_before) becomes the representative, existing edges
// are rewritten through the merge, and the branch is re-checked for
// contradiction (two distinct constants equal, Constraints.hs:124, or
// a less-cycle surfaced by the rewrite).  Returns 0 iff contradictory.
static u8 pgj_add_eq(PgjBranch *b, u32 n, const GjAtom *at, u32 i, u32 j) {
  i = b->rep[i];
  j = b->rep[j];
  if (i == j) return 1;
  if (at[i].is_const && at[j].is_const) return 0;
  u32 keep = pgj_atom_before(at[i], at[j]) ? i : j;
  u32 drop = (keep == i) ? j : i;
  for (u32 k = 0; k < n; k++)
    if (b->rep[k] == drop) b->rep[k] = (u8)keep;
  b->less[keep] |= b->less[drop];
  b->less[drop] = 0;
  for (u32 k = 0; k < n; k++)
    if (b->less[k] & (1u << drop)) {
      b->less[k] &= ~(1u << drop);
      b->less[k] |= (1u << keep);
    }
  if (b->less[keep] & (1u << keep)) return 0;   // x < x through the merge
  u8 chain[ATP_GJ_MAX_ATOMS];
  return pgj_topo(b, n, chain);
}

// Dedup insert into the branch store (Twee's usort over Ord Branch).
// Returns 1 on store overflow (caller must KEEP the CP), 0 otherwise.
static u8 pgj_push_branch(PgjBranch *arr, u32 *n_arr, const PgjBranch *b,
                          u32 n) {
  for (u32 i = 0; i < *n_arr; i++)
    if (memcmp(arr[i].rep, b->rep, n) == 0 &&
        memcmp(arr[i].less, b->less, n * sizeof(u32)) == 0)
      return 0;
  if (*n_arr >= ATP_PGJ_MAX_BRANCH) return 1;
  arr[(*n_arr)++] = *b;
  return 0;
}

static void pgj_model_from_chain(GjModel *m, const GjAtom *at,
                                 const u8 *chain, u32 len) {
  m->n = len;
  for (u32 i = 0; i < len; i++) {
    m->atom[i]  = at[chain[i]];
    m->major[i] = i;
  }
}

// Join both sides under model `m` (the per-model half of Twee's
// groundJoinFrom check, Join.hs:194/:216-220): model-gated normalize
// with globally-oriented rules firing unconditionally (KBO stability
// under substitution -- the gj_join_dd precedent) and verdict by
// equality of the normal forms up to AC (see below).
static u8 pgj_join_under(AtpState *s, const GjModel *m, Term l, Term r,
                         const u32 *cp_vars, u32 n_cp_vars) {
  g_gj_orient_free = 1;
  Term nl = gj_normalize(s, m, l, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP, 0);
  Term nr = gj_normalize(s, m, r, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP, 0);
  g_gj_orient_free = 0;
  if (kbo_eq(nl, nr)) return 1;
#ifdef THVM_ATP_AC
  // AC-equality completion of the model join test (the same redundancy
  // the push-side NF check applies, see the atp_ac_eq gate ~23670).
  // The model-gated gj_normalize applies commutativity/associativity as
  // oriented-by-the-model steps, but oriented AC is NOT confluent on
  // ground terms (meet(c,meet(a,b)) and meet(a,meet(b,c)) are AC-equal
  // yet distinct oriented normal forms), so two genuinely joinable sides
  // can settle at AC-equal-but-distinct NFs -- a FALSE counterexample
  // that keeps assoc-lifted CPs.  When the AC mask is registered
  // (thvm_atp_auto_ac detected the AC operators) AC-equal NFs ARE
  // joinable: the AC axioms are rules in R, so under this model both
  // sides descend to the shared AC-canonical form.  Sound (a join via
  // descending AC steps); recovers the assoc-lifted joins Twee's
  // AC-aware normalisation reaches.
  if (thvm_atp_get_ac_mask() != 0ull) {
    AtpAcInfo ac = { .ac_mask = thvm_atp_get_ac_mask() };
    if (atp_ac_eq(nl, nr, &ac)) return 1;
  }
#endif
  return 0;
}

static u8 atp_pop_gj_level(AtpState *s, Term l, Term r, u32 depth);

// One equality-branch instance (solve's Right sub, Constraints.hs:
// 283-287): substitute each merged variable by its representative atom
// and recurse the whole case-split on the instance.
static u8 atp_pop_gj_instance(AtpState *s, const PgjBranch *b,
                              const GjAtom *at, u32 n, Term l, Term r,
                              u32 depth) {
  RewriteSubst sub = {{0}};
  for (u32 k = 0; k < n; k++) {
    if (b->rep[k] == k) continue;
    if (at[k].is_const) return 0;         // reps are minimal: cannot happen
    if (at[k].id >= REWRITE_MAX_VAR) return 0;
    u32 rk = b->rep[k];
    sub.bindings[at[k].id] = at[rk].is_const
                                 ? term_new_ctr(at[rk].id, NULL, 0)
                                 : term_new_fvr(at[rk].id);
  }
  Term sl = thvm_subst_apply(l, &sub);
  Term sr = thvm_subst_apply(r, &sub);
  return atp_pop_gj_level(s, sl, sr, depth + 1);
}

// One case-split level: the full groundJoin loop for one (sub-)pair.
// Depth 0 receives the pop R+E normal forms; instance levels
// re-normalise with the model-free oriented pre-pass first.
static u8 atp_pop_gj_level(AtpState *s, Term l, Term r, u32 depth) {
  if (depth > ATP_PGJ_MAX_DEPTH) { g_pgj_keep_reason[7]++; return 0; }
  if (depth > 0) {
    l = gj_prenorm(s, l);
    r = gj_prenorm(s, r);
  }
  if (kbo_eq(l, r)) return 1;

  // Atom table: constants first, then variables, each ascending by id
  // (Twee usort over Atom Ord, Join.hs:181 `usort (atoms t ++ atoms u)`).
  u32 var_ids[ATP_GJ_MAX_ATOMS];
  u32 n_vars = 0;
  if (!atp_gj_collect_vars(l, var_ids, &n_vars, ATP_GJ_MAX_ATOMS)) { g_pgj_keep_reason[0]++; return 0; }
  if (!atp_gj_collect_vars(r, var_ids, &n_vars, ATP_GJ_MAX_ATOMS)) { g_pgj_keep_reason[0]++; return 0; }
  u32 clabs[ATP_GJ_MAX_ATOMS];
  u32 n_consts = 0;
  if (!gj_collect_consts(l, clabs, &n_consts, ATP_GJ_MAX_ATOMS)) { g_pgj_keep_reason[0]++; return 0; }
  if (!gj_collect_consts(r, clabs, &n_consts, ATP_GJ_MAX_ATOMS)) { g_pgj_keep_reason[0]++; return 0; }
  u32 n = n_consts + n_vars;
  if (n > ATP_GJ_MAX_ATOMS) { g_pgj_keep_reason[0]++; return 0; }
  for (u32 a = 1; a < n_consts; a++) {          // insertion sort, ascending
    u32 v = clabs[a];
    u32 b = a;
    while (b > 0 && clabs[b - 1] > v) { clabs[b] = clabs[b - 1]; b--; }
    clabs[b] = v;
  }
  for (u32 a = 1; a < n_vars; a++) {
    u32 v = var_ids[a];
    u32 b = a;
    while (b > 0 && var_ids[b - 1] > v) { var_ids[b] = var_ids[b - 1]; b--; }
    var_ids[b] = v;
  }
  GjAtom at[ATP_GJ_MAX_ATOMS];
  for (u32 k = 0; k < n_consts; k++) { at[k].is_const = 1; at[k].id = clabs[k]; }
  for (u32 k = 0; k < n_vars; k++) {
    at[n_consts + k].is_const = 0;
    at[n_consts + k].id = var_ids[k];
  }

  PgjBranch *cur = (PgjBranch *)malloc(sizeof(PgjBranch) * ATP_PGJ_MAX_BRANCH);
  PgjBranch *nxt = (PgjBranch *)malloc(sizeof(PgjBranch) * ATP_PGJ_MAX_BRANCH);
  if (cur == NULL || nxt == NULL) {
    free(cur);
    free(nxt);
    g_pgj_keep_reason[1]++;
    return 0;
  }
  // Initial context = one true branch (Join.hs:65 `branches (And [])`)
  // carrying the constants' FIXED relative order as less-edges (Twee
  // addTerm's f << g edges, Constraints.hs:178-187; our constant order
  // is the ground KBO verdict).
  memset(&cur[0], 0, sizeof cur[0]);
  for (u32 k = 0; k < n; k++) cur[0].rep[k] = (u8)k;
  for (u32 i = 0; i < n_consts; i++)
    for (u32 j = i + 1; j < n_consts; j++) {
      KboCmp c = atp_compare(s, term_new_ctr(at[i].id, NULL, 0),
                             term_new_ctr(at[j].id, NULL, 0));
      if (c == KBO_LT)      cur[0].less[i] |= (1u << j);
      else if (c == KBO_GT) cur[0].less[j] |= (1u << i);
    }
  u32 n_cur = 1;

  u32 ticks = 0;
  u8 verdict = 0;
  for (;;) {
    if (n_cur == 0) { verdict = 1; break; }   // every ordering covered
    // solve each branch (Constraints.hs:278-294); the round's model is
    // the first merge-free branch's chain (Join.hs:185 takes the first
    // Left of partitionEithers).
    u32 mb = n_cur;
    for (u32 b = 0; b < n_cur; b++) {
      u8 merged = 0;
      for (u32 k = 0; k < n; k++)
        if (cur[b].rep[k] != k) { merged = 1; break; }
      if (!merged) { mb = b; break; }
    }
    if (mb == n_cur) {
      // No branch yields a model: the strict-order space is covered and
      // only equality cases remain (Join.hs:181-184).  Each distinct
      // substitution is an instance that must itself join.
      verdict = 1;
      for (u32 b = 0; b < n_cur && verdict; b++) {
        u8 dup = 0;
        for (u32 p = 0; p < b; p++)
          if (memcmp(cur[p].rep, cur[b].rep, n) == 0) { dup = 1; break; }
        if (dup) continue;
        if (!atp_pop_gj_instance(s, &cur[b], at, n, l, r, depth)) {
          g_pgj_keep_reason[6]++;
          verdict = 0;
        }
      }
      break;
    }
    ticks++;
    s->n_pop_gj_ticks++;
    if (s->pop_gj_limit != 0u && ticks > s->pop_gj_limit) {
      g_pgj_keep_reason[2]++;
      verdict = 0;                            // Join.hs:193 limit -> Left
      break;
    }
    u8 chain[ATP_GJ_MAX_ATOMS];
    if (!pgj_topo(&cur[mb], n, chain)) { g_pgj_keep_reason[3]++; verdict = 0; break; }  // defensive
    u32 clen = n;
    GjModel m;
    pgj_model_from_chain(&m, at, chain, clen);
    if (!pgj_join_under(s, &m, l, r, var_ids, n_vars)) {
      g_pgj_keep_reason[4]++;
      verdict = 0;                            // counterexample ordering
      break;
    }
    // optimise weakenModel (Join.hs:197-200 + 264-268, delete-only):
    // greedily drop chain atoms while the pair still joins under the
    // reduced chain -- the weaker the surviving model, the larger the
    // region its diagram's negation removes below.
    for (u32 k = 0; k < clen && clen > 1; ) {
      u8 tryc[ATP_GJ_MAX_ATOMS];
      u32 y = 0;
      for (u32 x = 0; x < clen; x++)
        if (x != k) tryc[y++] = chain[x];
      GjModel mw;
      pgj_model_from_chain(&mw, at, tryc, clen - 1);
      if (pgj_join_under(s, &mw, l, r, var_ids, n_vars)) {
        memcpy(chain, tryc, clen - 1);
        clen--;                               // retry the same position
      } else {
        k++;
      }
    }
    // diag + formAnd (Join.hs:204-208, Constraints.hs:131-150): every
    // surviving branch is split into the cases that violate the
    // weakened chain at their first failing link, all earlier links
    // enforced strictly; the fully-satisfied region was certified by
    // the join above and is dropped.
    u32 n_nxt = 0;
    u8 overflow = 0;
    for (u32 b = 0; b < n_cur && !overflow; b++) {
      PgjBranch acc = cur[b];
      u8 alive = 1;
      for (u32 j = 1; j < clen && alive && !overflow; j++) {
        PgjBranch c1 = acc;                   // c_j < c_{j-1}
        if (pgj_add_less(&c1, n, chain[j], chain[j - 1]))
          overflow |= pgj_push_branch(nxt, &n_nxt, &c1, n);
        if (!overflow) {
          PgjBranch c2 = acc;                 // c_j = c_{j-1}
          if (pgj_add_eq(&c2, n, at, chain[j], chain[j - 1]))
            overflow |= pgj_push_branch(nxt, &n_nxt, &c2, n);
        }
        alive = pgj_add_less(&acc, n, chain[j - 1], chain[j]);
      }
    }
    if (overflow) { g_pgj_keep_reason[5]++; verdict = 0; break; }
    memcpy(cur, nxt, sizeof(PgjBranch) * n_nxt);
    n_cur = n_nxt;
  }
  free(cur);
  free(nxt);
  return verdict;
}

// Top-level pop-time entry: 1 = the popped CP's normal-form pair is
// PROVABLY ground-joinable under the case-split (drop it), 0 = keep.
static u8 atp_pop_gj_joinable(AtpState *s, Term l, Term r) {
  if (s == NULL || s->kbo == NULL) return 0;
  // gj_less_in is implemented for plain KBO only (the
  // atp_cp_ground_joinable precedent).
  if (s->lpo != NULL || s->rpo != NULL || s->wpo != NULL) return 0;
  if (s->gj_protect_l != 0 || s->gj_protect_r != 0 ||
      s->gj_exclude != ATP_GJ_NO_EXCLUDE)
    return 0;
  return atp_pop_gj_level(s, l, r, 0u);
}

// ── Fact-level -gj: WM RueckwaertsGrundzusammenfuehrbarkeit ─────────
// (INF/Hauptkomponenten.c:260-306 + the forward halves
// RUndEVerwaltung.c:182-183/:457-460.)  Default OFF, matching WM's
// -gj CLI default (RUN/Parameter.c:317; no strategy table emits the
// YFiles.c:125 `gj()` primitive).  Opt-in: Method
// {... "BackwardGroundJoin" -> True} / THVM_ATP_BWD_GROUND_JOIN.

// Binary CTR destructure: t = f(a, b)?
static u8 atp_gj_bin(Term t, u32 f, Term *a, Term *b) {
  if (term_tag(t) != TAG_CTR || term_ext(t) != f || term_ctr_n(t) != 2) {
    return 0;
  }
  *a = term_ctr_at(t, 0);
  *b = term_ctr_at(t, 1);
  return 1;
}

static u8 atp_gj_var_id(Term t, u32 *id) {
  if (term_tag(t) != TAG_FVR) return 0;
  *id = term_ext(t);
  return 1;
}

// t = f(x, f(y, z)) with all-variable leaves (right-nested).
static u8 atp_gj_shape_rr(Term t, u32 f, u32 *x, u32 *y, u32 *z) {
  Term a, b, c, d;
  if (!atp_gj_bin(t, f, &a, &b) || !atp_gj_bin(b, f, &c, &d)) return 0;
  return atp_gj_var_id(a, x) && atp_gj_var_id(c, y) && atp_gj_var_id(d, z);
}

// t = f(f(x, y), z) with all-variable leaves (left-nested).
static u8 atp_gj_shape_ll(Term t, u32 f, u32 *x, u32 *y, u32 *z) {
  Term a, b, c, d;
  if (!atp_gj_bin(t, f, &a, &b) || !atp_gj_bin(a, f, &c, &d)) return 0;
  return atp_gj_var_id(c, x) && atp_gj_var_id(d, y) && atp_gj_var_id(b, z);
}

// WM PROTECT_3_PERMS (Grundzusammenfuehrung.c:76, consumed :808-814):
// the -gj fact test refuses to sterilize associativity, commutativity,
// and extended-commutativity facts -- GZ_wertvoll, valuable for the
// permutative cleanup even when provably ground-joinable.  Shapes per
// WASIC/TermOperationen.c:1207-1248 (TO_IstAssoziativitaet /
// TO_IstKommutativitaet / TO_IstErweiterteKommutativitaet); thvm rules
// are var-normalized at push, so plain id equality realizes WM's
// positional -1/-2/-3 placeholders.
static u8 atp_gj_perm_valuable(Term l, Term r) {
  if (term_tag(l) != TAG_CTR || term_ctr_n(l) != 2) return 0;
  u32 f = term_ext(l);
  Term p, q, p2, q2;
  u32 a, b, c, d, x, y, z, u, v, w;
  // C: f(x1, x2) = f(x2, x1)
  if (atp_gj_bin(l, f, &p, &q) && atp_gj_bin(r, f, &p2, &q2) &&
      atp_gj_var_id(p, &a) && atp_gj_var_id(q, &b) &&
      atp_gj_var_id(p2, &c) && atp_gj_var_id(q2, &d) &&
      a != b && c == b && d == a) {
    return 1;
  }
  // A: f(f(x1,x2),x3) = f(x1,f(x2,x3)), either nesting on either side.
  if (atp_gj_shape_ll(l, f, &x, &y, &z) && atp_gj_shape_rr(r, f, &u, &v, &w) &&
      x == u && y == v && z == w && x != y && y != z && x != z) {
    return 1;
  }
  if (atp_gj_shape_rr(l, f, &x, &y, &z) && atp_gj_shape_ll(r, f, &u, &v, &w) &&
      x == u && y == v && z == w && x != y && y != z && x != z) {
    return 1;
  }
  // C': f(x1,f(x2,x3)) = one of the four rotated right-nested perms.
  if (atp_gj_shape_rr(l, f, &x, &y, &z) && atp_gj_shape_rr(r, f, &u, &v, &w) &&
      x != y && y != z && x != z) {
    if ((u == z && v == x && w == y) ||      // f(x3, f(x1, x2))
        (u == y && v == x && w == z) ||      // f(x2, f(x1, x3))
        (u == z && v == y && w == x) ||      // f(x3, f(x2, x1))
        (u == y && v == z && w == x)) {      // f(x2, f(x3, x1))
      return 1;
    }
  }
  return 0;
}

// Classify fact slot `i` per WM's fact-level -gj test:
//   * HOPELESS: <= 1 distinct variable, covering both-sides-ground
//     (MN90Check noVar gate, Grundzusammenfuehrung.c:729-733, + the
//     both-ground precheck :767-771).  Sticky.
//   * VALUABLE: PROTECT_3_PERMS A/C/C' shape.  Sticky.  (WM tags these
//     in the FORWARD test; under -gj every fact passes that test at
//     creation, so the backward walk's `gzfbStatus <= GZ_aussichtslos`
//     skip :824 never re-reaches them -- thvm classifies in the shared
//     path for the same effect.)
//   * JOINABLE: the groundJoin driver proves every ground instance of
//     lhs = rhs joinable under R u E MINUS the victim (gj_exclude =
//     WM DarfNichtReduzieren), root steps on the protected face(s)
//     Dreieck-gated.  Sticky -- the fact is sterile.
//   * FAILED: not shown joinable (also WM's GZ_zuTeuer var-overflow
//     case); retested on later facts.
// Protection per WM Grundzusammenfuehrung.c:841: rules protect the
// LHS face only (sSch -- the RHS face is strictly below in the
// reduction order); unorientable equations protect BOTH faces
// (allesSch; the per-branch comparability resolution that would
// unprotect the smaller instance face is skipped -- conservative,
// see gj_normalize).
static u8 atp_gj_fact_test(AtpState *s, u32 i) {
  // Lazy slots: this test runs at rule-push time, BEFORE the sweep-entry
  // materializer -- a NUM sentinel here collects 0 variables and the
  // n_vars <= 1 gate misclassifies every lazy-born fact HOPELESS
  // (sentinel-consumer audit, 2026-07-11).  Materialize side-exact from
  // the mirrors first; no-op on eager slots.
  Term l = atp_rule_term(s, i, 0u);
  Term r = atp_rule_term(s, i, 1u);
  u32 var_ids[ATP_GJ_MAX_VARS];
  u32 n_vars = 0;
  u8 var_overflow =
      !atp_gj_collect_vars(l, var_ids, &n_vars, ATP_GJ_MAX_VARS) ||
      !atp_gj_collect_vars(r, var_ids, &n_vars, ATP_GJ_MAX_VARS);
  if (!var_overflow && n_vars <= 1u) return ATP_GJ_ST_HOPELESS;
  if (atp_gj_perm_valuable(l, r)) return ATP_GJ_ST_VALUABLE;
  if (var_overflow) return ATP_GJ_ST_FAILED;
  s->gj_exclude   = i;
  // Twee interreduce1 (Twee.hs:787) tests joinability against R\{i} with
  // NO root protection -- it has no analog of WM's MO_schuetzeTerm
  // Dreieck root gate.  That gate makes the WM-faithful fact test far more
  // conservative (on the AC/idempotent GRP721/MVA009 signatures it proves
  // ZERO rules joinable, so the rule-delete port would never fire).  Under
  // the opt-in twee-delete flag drop the protection to recover twee's
  // (sound: joinable against R\{i} => the equation is a consequence of the
  // other rules => rule i is redundant).  Default path keeps WM protection
  // for byte-parity.
  if (s->use_gj_delete_rule) {
    s->gj_protect_l = 0;
    s->gj_protect_r = 0;
  } else {
    s->gj_protect_l = l;
    s->gj_protect_r = s->r_orient[i] ? 0 : r;
  }
  int joinable = atp_cp_ground_joinable(s, l, r);
  s->gj_exclude   = ATP_GJ_NO_EXCLUDE;
  s->gj_protect_l = 0;
  s->gj_protect_r = 0;
  return joinable ? ATP_GJ_ST_JOINABLE : ATP_GJ_ST_FAILED;
}

// The driver reasons in KBO sizes only; under LPO/RPO/WPO (or no
// order) every fact test would return KEEP, so skip the walks.
static u8 atp_gj_active(const AtpState *s) {
  return s->kbo != NULL && s->lpo == NULL && s->rpo == NULL &&
         s->wpo == NULL;
}

// Forward -gj fact test at creation (WM RUndEVerwaltung.c:182-183 for
// rules, :457-460 for equations -- Grundzusammenfuehrbar is decided
// BEFORE ArbeitsAufnahme interreduces and generates CPs, so a fact
// ground-joinable at birth never forms a CP at all (Weggefiltert,
// Unifikation1.c:1518/:1588 + :967-972)).  No KillParent here: a
// newborn has no queued children to orphan.
static void atp_gj_classify_added(AtpState *s, AtpAddedRange added) {
  if (!atp_gj_active(s)) return;
  for (u32 k = 0; k < added.count; k++) {
    u32 i = added.first + k;
    if (i >= s->n_rules || s->r_dead[i]) continue;
    u8 st = atp_gj_fact_test(s, i);
    s->r_gj_status[i] = st;
    if (st == ATP_GJ_ST_JOINABLE) s->n_facts_bwd_ground_joinable++;
  }
}

// WM RueckwaertsGrundzusammenfuehrbarkeit (Hauptkomponenten.c:260-306),
// run from the step tail AFTER CP generation + the IR-victim drain
// (ArbeitsAufnahme :327-329).  Walks every EXISTING live fact -- rules
// (:265 RE_forRegelnRobust) and equations (:286, the distinguished
// direction = thvm's single unorientable slot) -- skipping the
// just-added range (`Faktum != Neues`) and the sticky statuses
// (`gzfbStatus <= GZ_aussichtslos`, :824), and re-tests ground
// joinability against the system now extended by the new fact.  A fact
// shown joinable is STERILIZED per the compiled GZ_ZSFB_BEHALTEN=1
// (Grundzusammenfuehrung.h:52): the JOINABLE status excludes it from
// all future CP formation (atp_gen_one's Weggefiltert check) and
// atp_trace_mark_dead orphans its already-queued CPs (KPV_KillParent
// lebtNoch=FALSE, KPVerwaltung.c:343-351 -> selectNonOrphan :540;
// thvm's lazy at-pop discard reads the same bitmap when
// use_orphan_murder is on, the WM -ocrit layout).  The fact itself
// STAYS in R/E and keeps rewriting.  Walks NEITHER queued CPs NOR
// goals -- WM re-tests active facts only.
static void atp_bwd_ground_join_walk(AtpState *s, u32 skip_lo, u32 skip_hi) {
  if (!atp_gj_active(s)) return;
  for (u32 i = 0; i < s->n_rules; i++) {
    if (i >= skip_lo && i < skip_hi) continue;
    if (s->r_dead[i]) continue;
    u8 st = s->r_gj_status[i];
    if (st == ATP_GJ_ST_JOINABLE || st == ATP_GJ_ST_VALUABLE ||
        st == ATP_GJ_ST_HOPELESS) {
      continue;
    }
    st = atp_gj_fact_test(s, i);
    s->r_gj_status[i] = st;
    if (st == ATP_GJ_ST_JOINABLE) {
      s->n_facts_bwd_ground_joinable++;
      if (atp_rule_trace_on()) {
        char la[8192], ra[8192];
        atp_pretty_term(s->lhs[i], la, sizeof la);
        atp_pretty_term(s->rhs[i], ra, sizeof ra);
        fprintf(stderr, "  RETIRE bwd-gz (slot %u, trace %u): %s = %s\n",
                i, s->r_trace[i], la, ra);
      }
      atp_trace_mark_dead(s, s->r_trace[i]);
      if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[i]);
      // Twee interreduce1 deleteActive (opt-in): WM keeps the fact live as
      // a rewriter; twee DELETES a ground-joinable active rule from R (its
      // rewrite is recoverable via the rules that witness joinability, so
      // completeness holds).  Soft-delete via the bwd-subsume dead-sentinel
      // recipe (_.c:12237-12264): sentinel faces make thvm_match/thvm_unify
      // return 0, so every rewrite / CP-generation site skips the slot;
      // originals go to r_dead_*_save[] for proof reconstruction.  n_unorient
      // is NOT decremented -- matching the bwd-subsume convention (dead slots
      // are skipped via r_dead/sentinel; the count stays an upper bound).
      if (s->use_gj_delete_rule && !s->r_dead[i]) {
        Term dead_sentinel = term_new(0, TAG_FVR, 255u, 0);
        s->r_dead_lhs_save[i] = s->lhs[i];   // sentinel rides; save_ft is authoritative
        s->r_dead_rhs_save[i] = s->rhs[i];
        s->lhs[i] = dead_sentinel;
        s->rhs[i] = dead_sentinel;
        s->r_dead[i] = 1;
        s->r_revision++;
        s->n_facts_gj_deleted++;
        if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_uid[i]);
#ifdef ATP_RULE_INDEX
        s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
#ifdef THVM_ATPFT_RULES
        s->r_dead_lhs_save_ft[i] = s->lhs_ft[i];
        s->r_dead_rhs_save_ft[i] = s->rhs_ft[i];
        s->lhs_ft[i] = ftnew_var((AtpFt *)s->ft_arena_ptr, 255u, 0);
        s->rhs_ft[i] = ftnew_var((AtpFt *)s->ft_arena_ptr, 255u, 0);
#endif
      }
    }
  }
}

#endif  // ATP_CP_GROUND_JOIN

