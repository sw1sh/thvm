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
