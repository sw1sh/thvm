// thvm_unify - most-general-unifier on TAG_CTR + TAG_FVR terms.
//
// Standard Robinson algorithm with occurs check.  Produces a
// substitution `subst` such that applying it to both inputs makes
// them structurally equal.  Returns 1 on success, 0 on failure (mgu
// doesn't exist, e.g. clash on different constructors or occurs
// check fails).
//
// `subst` is the same RewriteSubst structure used by stage 3's
// matcher (declared in src/thvm.h).  Callers should clear it (zero-
// init) before the first call.  On success, walking `bindings[id]`
// recursively yields the most general unifier.
//
// Differs from one-way matching (thvm_match): both sides may contain
// variables; bindings flow in either direction.

// Recursively follow subst chains: while `t` is FVR with a binding,
// step to the binding.  Returns the final non-bound term.
static Term unify_walk(Term t, const RewriteSubst *subst) {
  while (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return t;
    Term b = subst->bindings[id];
    if (b == 0) return t;
    t = b;
  }
  return t;
}

// Occurs check: does the free variable `var_id` appear (after walking)
// anywhere in `t`?
static u8 unify_occurs(u32 var_id, Term t, const RewriteSubst *subst) {
  t = unify_walk(t, subst);
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (unify_occurs(var_id, term_ctr_at(t, i), subst)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

fn u8 thvm_unify(Term s, Term t, RewriteSubst *subst) {
  s = unify_walk(s, subst);
  t = unify_walk(t, subst);
  if (kbo_eq(s, t)) return 1;

  if (term_tag(s) == TAG_FVR) {
    u32 id = term_ext(s);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (unify_occurs(id, t, subst)) return 0;
    subst->bindings[id] = t;
    return 1;
  }
  if (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (unify_occurs(id, s, subst)) return 0;
    subst->bindings[id] = s;
    return 1;
  }
  if (term_tag(s) == TAG_CTR && term_tag(t) == TAG_CTR) {
    if (term_ext(s) != term_ext(t)) return 0;
    u32 ns = term_ctr_n(s);
    u32 nt = term_ctr_n(t);
    if (ns != nt) return 0;
    for (u32 i = 0; i < ns; i++) {
      if (!thvm_unify(term_ctr_at(s, i), term_ctr_at(t, i), subst)) return 0;
    }
    return 1;
  }
  return 0;
}

// Rebuild `t` with every FVR id shifted by `offset` so two rules can
// be unified without variable-name collisions.  Stage 4 uses this to
// rename rule_j's variables apart from rule_i's before the overlap.
fn Term thvm_rename_vars(Term t, u32 offset) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_new_fvr(term_ext(t) + offset);
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        children[i] = thvm_rename_vars(term_ctr_at(t, i), offset);
      }
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// Apply a unifier (which may contain chained FVR -> FVR -> term
// links) recursively, returning a fully-resolved term.  Differs from
// thvm_subst_apply in that it follows chains via unify_walk.
fn Term thvm_unify_apply(Term t, const RewriteSubst *subst) {
  t = unify_walk(t, subst);
  switch (term_tag(t)) {
    case TAG_FVR: return t;  // unbound -- leave as-is
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        children[i] = thvm_unify_apply(term_ctr_at(t, i), subst);
      }
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// === 7c: canonical variable normalization ==========================
//
// `thvm_normalize_vars` alpha-renames the variables of a stored
// (lhs, rhs) equation/rule so they carry a canonically dense set of
// ids `[0, k)`, k = the count of DISTINCT variables across BOTH
// sides.  This is the convergence-fix unblocker (milestone 7c):
//
//   - The matcher / subst-apply (`src/rewrite/_.c`) silently treat
//     any FVR with id >= REWRITE_MAX_VAR (=64) as an unmatchable
//     constant.  The CP enumerator renames rule j by
//     CP_RENAME_OFFSET and BAKES that offset into the stored CP, so
//     deep overlaps carry ids that cross 64 -- past which the
//     rewriter can no longer fire, killing joinability / subsumption.
//   - Renumbering every stored rule and CP to a dense `[0, k)` set
//     guarantees no stored variable ever crosses the cliff.
//   - It is alpha-renaming: it does NOT change a term's meaning.  A
//     variable occurring in both sides is ONE variable and gets ONE
//     id (lhs and rhs share the numbering).
//   - It makes alpha-equivalent rules/CPs BYTE-IDENTICAL, so the
//     dedup / subsumption criteria start catching them.
//
// The walk is a fixed deterministic preorder: lhs fully, then rhs.
// Each distinct old id is assigned the next free dense id by first
// occurrence.

// nv_map: old-id -> dense-id, as a first-occurrence-ordered list of
// pairs.  Linear search is fine -- stored rules carry few distinct
// variables.  REWRITE_MAX_VAR slots is ample headroom (canonical
// output is always [0, k) with k far below REWRITE_MAX_VAR).
typedef struct {
  u32 old_id[REWRITE_MAX_VAR];
  u32 n;
} NvMap;

// Look up `old`'s dense id, assigning the next free one on first
// sight.  If the map is full (pathological -- more distinct vars
// than REWRITE_MAX_VAR), fold onto the last slot rather than
// overflow; downstream is no worse than the pre-7c behavior.
static u32 nv_map_index(NvMap *m, u32 old) {
  for (u32 i = 0; i < m->n; i++) {
    if (m->old_id[i] == old) return i;
  }
  if (m->n >= REWRITE_MAX_VAR) return REWRITE_MAX_VAR - 1u;
  m->old_id[m->n] = old;
  return m->n++;
}

// First pass: walk `t` in preorder, registering each distinct FVR id
// in `m` (assigning dense ids by first occurrence).
static void nv_collect(Term t, NvMap *m) {
  switch (term_tag(t)) {
    case TAG_FVR: nv_map_index(m, term_ext(t)); break;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return;
      for (u32 i = 0; i < n; i++) nv_collect(term_ctr_at(t, i), m);
      break;
    }
    default: break;
  }
}

// Second pass: rebuild `t` with every FVR id replaced by its dense
// id from the (already-populated) map.
static Term nv_rewrite(Term t, const NvMap *m) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 old = term_ext(t);
      for (u32 i = 0; i < m->n; i++) {
        if (m->old_id[i] == old) return term_new_fvr(i);
      }
      return t;  // unreachable: nv_collect registered every id
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        children[i] = nv_rewrite(term_ctr_at(t, i), m);
      }
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// Canonically renumber the variables of the equation/rule
// `(*lhs, *rhs)` in place.  The two sides SHARE the numbering: a
// variable occurring in both is one variable with one dense id.
// After the call, every FVR id is in `[0, k)` for some k <=
// REWRITE_MAX_VAR.  Alpha-renaming -- preserves meaning.
fn void thvm_normalize_vars(Term *lhs, Term *rhs) {
  if (lhs == NULL || rhs == NULL) return;
  NvMap m;
  m.n = 0;
  nv_collect(*lhs, &m);
  nv_collect(*rhs, &m);
  *lhs = nv_rewrite(*lhs, &m);
  *rhs = nv_rewrite(*rhs, &m);
}
