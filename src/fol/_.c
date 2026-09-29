// First-order clause representation + binary resolution.
//
// thvm's existing equational layer (src/atp/, src/cp/) is unit-
// equational: every axiom and conjecture is a single equation
// `lhs = rhs`.  Full first-order clausal reasoning (Vampire/E-class)
// generalises this:
//   * a literal is a signed atom: ±P(t_1..t_n) or ±(s = t)
//   * a clause is a disjunction of literals: L_1 v L_2 v ... v L_n
//   * the empty clause (n_lits == 0) is `false` and witnesses
//     unsatisfiability
//
// This module is the foundation: types + binary resolution + a few
// structural helpers.  Saturation (subsumption, paramodulation,
// selection function, redundancy) layers on in subsequent ticks.

#define FOL_RENAME_OFFSET (REWRITE_MAX_VAR / 2u)
#define FOL_MAX_LITS 64u

// Allocate a fresh clause with `n_lits` literals (caller fills them).
// Returns a heap-owned Clause*; release with fol_clause_free.
fn FolClause *fol_clause_new(u32 n_lits) {
  if (n_lits > FOL_MAX_LITS) return NULL;
  FolClause *c = (FolClause *)calloc(1, sizeof(FolClause));
  if (c == NULL) return NULL;
  if (n_lits == 0u) {
    c->lits = NULL;
    c->n_lits = 0u;
    return c;
  }
  c->lits = (FolLit *)calloc(n_lits, sizeof(FolLit));
  if (c->lits == NULL) { free(c); return NULL; }
  c->n_lits = n_lits;
  return c;
}

fn void fol_clause_free(FolClause *c) {
  if (c == NULL) return;
  free(c->lits);
  free(c);
}

// Structural-equal modulo literal order: clauses are multisets of
// signed atoms.  O(n^2) marker-based match -- callers don't store
// astronomic numbers of literals per clause (FOL_MAX_LITS = 64).
fn u8 fol_clause_eq(const FolClause *a, const FolClause *b) {
  if (a == NULL || b == NULL) return (a == b) ? 1u : 0u;
  if (a->n_lits != b->n_lits) return 0u;
  if (a->n_lits == 0u) return 1u;
  u8 used[FOL_MAX_LITS] = {0};
  for (u32 i = 0; i < a->n_lits; i++) {
    u8 found = 0u;
    for (u32 j = 0; j < b->n_lits; j++) {
      if (used[j]) continue;
      if (a->lits[i].sign != b->lits[j].sign) continue;
      if (!kbo_eq(a->lits[i].atom, b->lits[j].atom)) continue;
      used[j] = 1u;
      found = 1u;
      break;
    }
    if (!found) return 0u;
  }
  return 1u;
}

// Rename every FVR id in `t` shifted by `offset`.  Reuses
// thvm_rename_vars from src/unify/_.c -- same semantics: a no-op for
// closed terms.
static Term fol_rename_term(Term t, u32 offset) {
  return thvm_rename_vars(t, offset);
}

// Canonically renumber a clause's variables to a dense [0, k), all
// literals SHARING one numbering (a variable spanning literals stays
// one variable).  Alpha-renaming via the same nv_rewrite walk the
// equational layer uses (src/unify/_.c).
//
// Every binary inference renames its second premise's variables up
// by FOL_RENAME_OFFSET; unbound renamed variables SURVIVE into the
// conclusion.  Without canonization a stored clause carries ids in
// [OFFSET, 2*OFFSET), the NEXT inference renames those past
// REWRITE_MAX_VAR where the matcher treats them as unmatchable
// constants -- and unification against an id that happens to equal a
// freshly-renamed premise variable CAPTURES it.  Both silently lose
// inferences the calculus needs (PUZ001+1 spuriously saturated).
// Canonizing every clause at store time keeps all ids far below the
// cliff and makes alpha-equivalent clauses byte-comparable for
// subsumption.
fn void fol_clause_canon_vars(FolClause *c) {
  if (c == NULL) return;
  NvMap m;
  m.n = 0;
  for (u32 i = 0; i < c->n_lits; i++) {
    c->lits[i].atom = nv_rewrite(c->lits[i].atom, &m);
  }
}

// Binary resolution:
//   Given clauses C1 and C2 with literals L_i in C1 and L_j in C2 of
//   opposite signs, and atoms that unify with mgu σ, the resolvent is
//   σ(C1 \ {L_i}) ∪ σ(C2 \ {L_j}).
//
// Variables of C2 are renamed by FOL_RENAME_OFFSET before the unify
// so two clauses with shared variable names don't collide.  Returns a
// freshly-allocated FolClause* on success, NULL when:
//   * sign mismatch fails (same sign -- not a complementary pair),
//   * atoms don't unify,
//   * the combined literal count exceeds FOL_MAX_LITS,
//   * allocation fails.
//
// The caller owns the returned clause and must fol_clause_free it.
fn FolClause *fol_resolve(const FolClause *c1, u32 i,
                          const FolClause *c2, u32 j) {
  if (c1 == NULL || c2 == NULL) return NULL;
  if (i >= c1->n_lits || j >= c2->n_lits) return NULL;
  // Resolvent requires complementary polarity.
  if (c1->lits[i].sign == c2->lits[j].sign) return NULL;
  // Both atoms must unify (after renaming c2's variables apart).
  Term atom2_renamed = fol_rename_term(c2->lits[j].atom, FOL_RENAME_OFFSET);
  RewriteSubst subst = {{0}};
  if (!thvm_unify(c1->lits[i].atom, atom2_renamed, &subst)) return NULL;

  u32 out_n = (c1->n_lits - 1u) + (c2->n_lits - 1u);
  if (out_n > FOL_MAX_LITS) return NULL;

  FolClause *r = fol_clause_new(out_n);
  if (r == NULL) return NULL;
  u32 idx = 0u;
  for (u32 k = 0; k < c1->n_lits; k++) {
    if (k == i) continue;
    r->lits[idx].atom = thvm_unify_apply(c1->lits[k].atom, &subst);
    r->lits[idx].sign = c1->lits[k].sign;
    idx++;
  }
  for (u32 k = 0; k < c2->n_lits; k++) {
    if (k == j) continue;
    Term renamed = fol_rename_term(c2->lits[k].atom, FOL_RENAME_OFFSET);
    r->lits[idx].atom = thvm_unify_apply(renamed, &subst);
    r->lits[idx].sign = c2->lits[k].sign;
    idx++;
  }
  return r;
}

// Factoring (positive form): if two same-polarity literals in C have
// unifiable atoms, the factor is σ(C with one of them dropped).  Used
// to derive shorter clauses that may close more easily.  Returns NULL
// when atoms don't unify or signs differ.
fn FolClause *fol_factor(const FolClause *c, u32 i, u32 j) {
  if (c == NULL) return NULL;
  if (i >= c->n_lits || j >= c->n_lits || i == j) return NULL;
  if (c->lits[i].sign != c->lits[j].sign) return NULL;
  RewriteSubst subst = {{0}};
  if (!thvm_unify(c->lits[i].atom, c->lits[j].atom, &subst)) return NULL;

  FolClause *r = fol_clause_new(c->n_lits - 1u);
  if (r == NULL) return NULL;
  u32 idx = 0u;
  for (u32 k = 0; k < c->n_lits; k++) {
    if (k == j) continue;
    r->lits[idx].atom = thvm_unify_apply(c->lits[k].atom, &subst);
    r->lits[idx].sign = c->lits[k].sign;
    idx++;
  }
  return r;
}

// Convenience: a clause is the empty clause `□` (FALSE) iff it has
// zero literals.  This is the saturation success witness.
fn u8 fol_clause_is_empty(const FolClause *c) {
  return (c != NULL && c->n_lits == 0u) ? 1u : 0u;
}

// === paramodulation ==================================================
//
// Given clauses
//   C1 = (s = t) v R1    (`eq_idx` points at a positive equality literal)
//   C2 = L[u]    v R2    (`target_idx` points at any literal; the position
//                         `path` walks into that literal's atom to `u`)
// with mgu σ of s and u, the paramodulant is
//   σ( R1 v L[t] v R2 )
//
// Variables of C2 are renamed apart by FOL_RENAME_OFFSET before the
// unify so the two clauses' free variables don't collide.  `swap`
// selects the orientation of the equality (0: use s -> t, 1: use t -> s)
// -- standard paramodulation explores both.  Returns a fresh
// heap-allocated clause on success, NULL on failure (sign / shape /
// unify / cap / alloc / position-into-variable).
//
// Equality predicate is identified by the FOL_LAB_EQ label
// (= 0 by convention; callers wrap (s = t) atoms as CTR(FOL_LAB_EQ,
// [s, t]).  Reuses cp_subterm_at + cp_replace_at from src/cp for
// position handling.
#define FOL_LAB_EQ 0u

fn u8 fol_atom_is_eq(Term atom) {
  return (term_tag(atom) == TAG_CTR
          && term_ext(atom) == FOL_LAB_EQ
          && term_ctr_n(atom) == 2u) ? 1u : 0u;
}

fn FolClause *fol_paramodulate(const FolClause *eq_clause, u32 eq_idx,
                               u8 swap,
                               const FolClause *target, u32 target_idx,
                               const u32 *path, u32 path_len) {
  if (eq_clause == NULL || target == NULL) return NULL;
  if (eq_idx >= eq_clause->n_lits) return NULL;
  if (target_idx >= target->n_lits) return NULL;
  if (eq_clause->lits[eq_idx].sign != 0u) return NULL;
  Term eq_atom = eq_clause->lits[eq_idx].atom;
  if (!fol_atom_is_eq(eq_atom)) return NULL;

  Term s = term_ctr_at(eq_atom, swap ? 1u : 0u);
  Term t = term_ctr_at(eq_atom, swap ? 0u : 1u);

  // SOUNDNESS (CAT015-3 false refutation, 2026-07-19): two ill-formed
  // inference shapes must be rejected here, at the choke point every
  // caller (saturation, proof checker, tests) shares.
  //   * path_len == 0 addresses the WHOLE ATOM: "rewriting" a
  //     predicate atom to a term manufactures a variable-as-literal
  //     clause (~X0), which then resolves with anything.  Atoms are
  //     not terms; paramodulation positions must be proper subterms.
  //   * a bare-variable from-side unifies with ANY subterm (including
  //     whole atoms via the path guard above) and replaces it with an
  //     unconstrained fresh variable.  Standard superposition forbids
  //     paramodulation from a variable side outright (an ordering can
  //     never orient var > term); unrestricted mode inherits the ban
  //     for well-formedness.
  if (path_len == 0u) return NULL;
  if (term_tag(s) == TAG_FVR) return NULL;

  Term target_atom = target->lits[target_idx].atom;
  Term target_atom_renamed = thvm_rename_vars(target_atom, FOL_RENAME_OFFSET);
  Term u = cp_subterm_at(target_atom_renamed, path, path_len);
  if (u == 0) return NULL;
  if (term_tag(u) == TAG_FVR) return NULL;   // no paramod into vars

  RewriteSubst subst = {{0}};
  if (!thvm_unify(s, u, &subst)) return NULL;

  Term replaced_atom = cp_replace_at(target_atom_renamed, path, path_len, t);
  Term new_atom     = thvm_unify_apply(replaced_atom, &subst);

  u32 out_n = (eq_clause->n_lits - 1u) + target->n_lits;
  if (out_n > FOL_MAX_LITS) return NULL;

  FolClause *r = fol_clause_new(out_n);
  if (r == NULL) return NULL;
  u32 idx = 0u;
  // R1: lits of eq_clause minus the equality.
  for (u32 k = 0; k < eq_clause->n_lits; k++) {
    if (k == eq_idx) continue;
    r->lits[idx].atom = thvm_unify_apply(eq_clause->lits[k].atom, &subst);
    r->lits[idx].sign = eq_clause->lits[k].sign;
    idx++;
  }
  // R2 + L[t]: every target literal, with target_idx replaced.
  for (u32 k = 0; k < target->n_lits; k++) {
    Term ak;
    if (k == target_idx) {
      ak = new_atom;
    } else {
      Term raw = thvm_rename_vars(target->lits[k].atom, FOL_RENAME_OFFSET);
      ak = thvm_unify_apply(raw, &subst);
    }
    r->lits[idx].atom = ak;
    r->lits[idx].sign = target->lits[k].sign;
    idx++;
  }
  return r;
}

// === subsumption ====================================================
//
// Clause C1 subsumes clause C2 iff there is a substitution σ
// (mapping C1's free variables) such that σ(C1) is a multiset
// SUBSET of C2 -- i.e. every literal of σ(C1) appears (with matching
// sign + structure) in C2 and each C2 literal is used at most once.
//
// Standard recursive backtracking match.  At each step pick the
// lowest-index unmatched C1 literal and try every unused C2 literal
// of matching sign that one-way-matches under the current σ.  The
// substitution is snapshotted before each attempt and restored on
// backtrack.
//
// Variables of C2 are renamed apart by FOL_RENAME_OFFSET so they
// can't be bound by the matcher -- they act as ground atoms.
//
// Complexity is |C1|! worst case; bounded by FOL_MAX_LITS = 64 but
// in practice clauses subsumed in saturation are small.

// A saturation's armed deadline (sat.c: the pre-pass guard, the wall)
// reaches inside one subsumption test: past it the matcher answers "not
// subsumed" -- which only keeps a clause -- and the step aborts.  A test
// over many literals of one predicate backtracks exponentially (the
// LCL6xx modal pre-pass sat in one for over a minute).  Disarmed
// deadlines never fire.
static u8 g_cnf_late = 0u;
static inline int cnf_late_tick(void);

// `nvars` = number of binding slots the PATTERN can touch (its max
// variable id + 1; canon clauses have dense ids).  Snapshot/restore
// only that prefix -- a full RewriteSubst copy per literal trial is
// 512 bytes, and ground patterns (fact-heavy problems) need no copy
// at all.
static u8 fol_subsume_rec(const FolClause *c1, u32 c1_idx,
                          const FolLit *c2_lits, u32 n2,
                          u8 *used,
                          RewriteSubst *subst, u32 nvars) {
  if (c1_idx >= c1->n_lits) return 1u;
  const FolLit *l1 = &c1->lits[c1_idx];
  for (u32 j = 0; j < n2; j++) {
    if (cnf_late_tick()) return 0u;
    if (used[j]) continue;
    if (c2_lits[j].sign != l1->sign) continue;
    Term snap[REWRITE_MAX_VAR];
    if (nvars > 0u) memcpy(snap, subst->bindings, nvars * sizeof(Term));
    if (thvm_match(l1->atom, c2_lits[j].atom, subst)) {
      used[j] = 1u;
      if (fol_subsume_rec(c1, c1_idx + 1u, c2_lits, n2, used, subst, nvars)) {
        return 1u;
      }
      used[j] = 0u;
    }
    if (nvars > 0u) memcpy(subst->bindings, snap, nvars * sizeof(Term));
  }
  return 0u;
}

// Max variable id + 1 across the clause (clamped to REWRITE_MAX_VAR).
// Canon clauses have dense ids, so this equals the distinct-variable
// count.
static u32 fol_term_nvars(Term t) {
  if (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    return (id >= REWRITE_MAX_VAR) ? REWRITE_MAX_VAR : id + 1u;
  }
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 m = 0u;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) {
    u32 v = fol_term_nvars(term_ctr_at(t, i));
    if (v > m) m = v;
  }
  return m;
}

fn u32 fol_clause_nvars(const FolClause *c) {
  if (c == NULL) return 0u;
  u32 m = 0u;
  for (u32 i = 0; i < c->n_lits; i++) {
    u32 v = fol_term_nvars(c->lits[i].atom);
    if (v > m) m = v;
  }
  return m;
}

// Core entry against PRE-FROZEN target literals (variables already
// renamed apart so the matcher can't bind them).  Callers that test
// one target against MANY patterns freeze once and reuse -- the
// per-call rename otherwise dominates saturation profiles.
// `nvars` variant: caller supplies the pattern's variable-slot count
// (fol_clause_nvars, cacheable per stored clause).
fn u8 fol_subsumes_frozen_nv(const FolClause *c1, const FolLit *frozen,
                             u32 n2, u32 nvars) {
  if (c1 == NULL || frozen == NULL) return 0u;
  if (c1->n_lits > n2) return 0u;
  if (c1->n_lits == 0u) return 1u;   // empty subsumes everything
  if (n2 > FOL_MAX_LITS) return 0u;
  u8 used[FOL_MAX_LITS] = {0};
  RewriteSubst subst;
  if (nvars > REWRITE_MAX_VAR) nvars = REWRITE_MAX_VAR;
  if (nvars > 0u) memset(subst.bindings, 0, nvars * sizeof(Term));
  return fol_subsume_rec(c1, 0u, frozen, n2, used, &subst, nvars);
}

fn u8 fol_subsumes_frozen(const FolClause *c1, const FolLit *frozen, u32 n2) {
  if (c1 == NULL) return 0u;
  return fol_subsumes_frozen_nv(c1, frozen, n2, fol_clause_nvars(c1));
}

// Freeze `c`'s literals into `out` (size >= c->n_lits): variables
// renamed apart by FOL_RENAME_OFFSET so a matcher treats them as
// constants.
fn void fol_freeze_lits(const FolClause *c, FolLit *out) {
  for (u32 k = 0; k < c->n_lits; k++) {
    out[k].atom = thvm_rename_vars(c->lits[k].atom, FOL_RENAME_OFFSET);
    out[k].sign = c->lits[k].sign;
  }
}

fn u8 fol_subsumes(const FolClause *c1, const FolClause *c2) {
  if (c1 == NULL || c2 == NULL) return 0u;
  if (c1->n_lits > c2->n_lits) return 0u;
  if (c1->n_lits == 0u) return 1u;   // empty subsumes everything
  if (c2->n_lits > FOL_MAX_LITS) return 0u;

  // Rename C2's variables apart so the matcher can't bind them.
  FolLit renamed[FOL_MAX_LITS];
  fol_freeze_lits(c2, renamed);
  return fol_subsumes_frozen(c1, renamed, c2->n_lits);
}

// Tautology check: a clause is a tautology iff it contains both a
// positive and a negative occurrence of the same atom.  Cheap O(n^2)
// scan, used by saturation loops to drop derived clauses before they
// enter the passive set.
fn u8 fol_is_tautology(const FolClause *c) {
  if (c == NULL) return 0u;
  for (u32 i = 0; i < c->n_lits; i++) {
    for (u32 j = i + 1u; j < c->n_lits; j++) {
      if (c->lits[i].sign == c->lits[j].sign) continue;
      if (kbo_eq(c->lits[i].atom, c->lits[j].atom)) return 1u;
    }
  }
  // Equality-reflexivity tautology: (s = s) positive.
  for (u32 i = 0; i < c->n_lits; i++) {
    if (c->lits[i].sign != 0u) continue;
    if (!fol_atom_is_eq(c->lits[i].atom)) continue;
    Term lhs = term_ctr_at(c->lits[i].atom, 0u);
    Term rhs = term_ctr_at(c->lits[i].atom, 1u);
    if (kbo_eq(lhs, rhs)) return 1u;
  }
  return 0u;
}

// === equality factoring =============================================
//
// Standard (Vampire-style) equality factoring: given a clause
//   C = (s1 = t1) v (s2 = t2) v R          (both positive equalities)
// and σ that unifies s1 and s2, derive
//   σ((s1 = t1) v ¬(t1 = t2) v R)
// dropping the second equality and adding the negative `¬(t1 = t2)`
// witness.  Together with paramodulation + reflex-resolve, this
// completes the equality-inference family.
//
// Returns NULL on:
//   * either literal is not a positive equality,
//   * the equalities don't unify on the chosen side,
//   * literal-cap overflow.
fn FolClause *fol_eq_factor(const FolClause *c, u32 i, u32 j) {
  if (c == NULL || i == j) return NULL;
  if (i >= c->n_lits || j >= c->n_lits) return NULL;
  const FolLit *li = &c->lits[i];
  const FolLit *lj = &c->lits[j];
  if (li->sign != 0u || lj->sign != 0u) return NULL;
  if (!fol_atom_is_eq(li->atom) || !fol_atom_is_eq(lj->atom)) return NULL;

  Term s1 = term_ctr_at(li->atom, 0u);
  Term t1 = term_ctr_at(li->atom, 1u);
  Term s2 = term_ctr_at(lj->atom, 0u);
  Term t2 = term_ctr_at(lj->atom, 1u);

  RewriteSubst subst = {{0}};
  if (!thvm_unify(s1, s2, &subst)) return NULL;

  Term t1_sub = thvm_unify_apply(t1, &subst);
  Term t2_sub = thvm_unify_apply(t2, &subst);
  Term s1_sub = thvm_unify_apply(s1, &subst);

  // The new clause has same size as c: keeps `i`, drops `j`, and
  // adds the witness ¬(t1 = t2).  Net: -1 + 1 = 0.
  if (c->n_lits > FOL_MAX_LITS) return NULL;
  FolClause *r = fol_clause_new(c->n_lits);
  if (r == NULL) return NULL;
  u32 idx = 0u;
  for (u32 k = 0; k < c->n_lits; k++) {
    if (k == j) continue;
    if (k == i) {
      // Rebuild s1 = t1 under σ.
      Term kids[2] = { s1_sub, t1_sub };
      r->lits[idx].atom = term_new_ctr(FOL_LAB_EQ, kids, 2u);
      r->lits[idx].sign = 0u;
    } else {
      r->lits[idx].atom = thvm_unify_apply(c->lits[k].atom, &subst);
      r->lits[idx].sign = c->lits[k].sign;
    }
    idx++;
  }
  // Append ¬(t1 = t2)_σ.
  Term kids[2] = { t1_sub, t2_sub };
  r->lits[idx].atom = term_new_ctr(FOL_LAB_EQ, kids, 2u);
  r->lits[idx].sign = 1u;
  return r;
}

// === reflexivity resolution =========================================
//
// If a clause contains a NEGATIVE equality literal ¬(s = t) and s and
// t unify with mgu σ, the resolvent is σ(C minus that literal).  This
// is the standard reflexivity-resolution inference, sometimes called
// "equality resolution".  Returns NULL when the literal isn't a
// negative equality / atoms don't unify / cap.
fn FolClause *fol_reflex_resolve(const FolClause *c, u32 idx) {
  if (c == NULL || idx >= c->n_lits) return NULL;
  if (c->lits[idx].sign != 1u) return NULL;
  Term atom = c->lits[idx].atom;
  if (!fol_atom_is_eq(atom)) return NULL;

  RewriteSubst subst = {{0}};
  if (!thvm_unify(term_ctr_at(atom, 0u), term_ctr_at(atom, 1u), &subst)) {
    return NULL;
  }
  FolClause *r = fol_clause_new(c->n_lits - 1u);
  if (r == NULL) return NULL;
  u32 j = 0u;
  for (u32 k = 0; k < c->n_lits; k++) {
    if (k == idx) continue;
    r->lits[j].atom = thvm_unify_apply(c->lits[k].atom, &subst);
    r->lits[j].sign = c->lits[k].sign;
    j++;
  }
  return r;
}

// === demodulation ===================================================
//
// A unit positive equality clause `[s = t]` can act as an oriented
// rewrite rule.  Demodulation walks every other clause's atoms,
// matches the rule's LHS against any subterm, and replaces with the
// rule's RHS (with the matching σ applied).
//
// Naïve directed-equality demodulation (no ordering check):
//   * `s` is taken as the rule's input, `t` as the output.
//   * No σ(s) > σ(t) check -- callers may give us a non-orientable
//     equation, in which case demodulation may not preserve
//     completeness.  An ordering-aware version is a follow-up that
//     plugs in a KboConfig / LpoConfig / RpoConfig / WpoConfig.
//
// Returns a new clause IF any literal changed; otherwise NULL
// (caller can skip the swap).  Recursive rewrite is bottom-up,
// applying the rule innermost-first (left-to-right at each node).

static Term fol_rewrite_once(Term term, Term s, Term t);

static Term fol_rewrite_at_root(Term term, Term s, Term t) {
  RewriteSubst subst = {{0}};
  if (thvm_match(s, term, &subst)) {
    return thvm_subst_apply(t, &subst);
  }
  return term;
}

static Term fol_rewrite_once(Term term, Term s, Term t) {
  // Try root first.
  Term root = fol_rewrite_at_root(term, s, t);
  if (root != term) return root;

  if (term_tag(term) != TAG_CTR) return term;
  u32 n = term_ctr_n(term);
  if (n == 0u) return term;
  Term stk[REWRITE_MAX_ARITY];
  Term *kids = term_kids_buf(stk, n);
  u8 changed = 0u;
  for (u32 i = 0; i < n; i++) {
    Term orig = term_ctr_at(term, i);
    kids[i] = fol_rewrite_once(orig, s, t);
    if (kids[i] != orig) changed = 1u;
  }
  Term r = changed ? term_new_ctr(term_ext(term), kids, n) : term;
  term_kids_release(kids, stk);
  return r;
}

// Apply the rewrite `s -> t` to a single literal's atom.  Returns
// the new atom (== old atom if no rewrite fired).
fn Term fol_demodulate_atom(Term s, Term t, Term atom) {
  return fol_rewrite_once(atom, s, t);
}

// Apply `eq_clause`'s positive equality as a rewrite rule to every
// literal of `target`.  Returns a fresh clause if anything rewrote,
// NULL otherwise.  Caller owns the result.
//
// Variables: the RULE's free variables are renamed apart by
// FOL_RENAME_OFFSET (not the target's).  thvm_match binds only the
// PATTERN-side variables, so renaming the rule's s/t makes its vars
// distinct from the target's; the substitution lands in the
// rule-renamed-var range and gets eliminated when applied to t.
// Target variables stay in their original IDs and remain consistent
// across the resulting clause's literals.
fn FolClause *fol_demodulate(const FolClause *eq_clause, const FolClause *target) {
  if (eq_clause == NULL || target == NULL) return NULL;
  if (eq_clause->n_lits != 1u) return NULL;
  if (eq_clause->lits[0].sign != 0u) return NULL;
  Term eq_atom = eq_clause->lits[0].atom;
  if (!fol_atom_is_eq(eq_atom)) return NULL;
  Term s_raw = term_ctr_at(eq_atom, 0u);
  Term t_raw = term_ctr_at(eq_atom, 1u);
  Term s = thvm_rename_vars(s_raw, FOL_RENAME_OFFSET);
  Term t = thvm_rename_vars(t_raw, FOL_RENAME_OFFSET);

  u32 n = target->n_lits;
  u8 any_change = 0u;
  Term *cache = (Term *)calloc(n, sizeof(Term));
  if (cache == NULL) return NULL;
  for (u32 i = 0; i < n; i++) {
    Term raw = target->lits[i].atom;
    Term re  = fol_rewrite_once(raw, s, t);
    if (re != raw) any_change = 1u;
    cache[i] = re;
  }
  if (!any_change) { free(cache); return NULL; }

  FolClause *r = fol_clause_new(n);
  if (r == NULL) { free(cache); return NULL; }
  for (u32 i = 0; i < n; i++) {
    r->lits[i].atom = cache[i];
    r->lits[i].sign = target->lits[i].sign;
  }
  free(cache);
  return r;
}

// === saturation loop ================================================
// CnfState + given-clause driver.  See `sat.c` for the
// implementation; we include it here so the single-TU build sees
// everything in one place.
#include "sat.c"

// === CNF preprocessing ==============================================
// Formula -> CNF pipeline (NNF + Skolemize + distribute + extract).
#include "cnf.c"
