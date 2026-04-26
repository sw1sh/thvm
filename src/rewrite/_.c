// thvm_rewrite_* - one-shot equational rewriter on TAG_CTR + TAG_FVR.
//
// Stage 3 of docs/plans/waldmeister_ic_atp.md.
//
// A rule is a pair (lhs, rhs); lhs may contain TAG_FVR variables and
// rhs may use the same variables.  thvm_rewrite_step tries each rule
// in order, matching the LHS against the term *at the top position*
// (no recursive descent yet -- that lands when stage 5 introduces the
// saturation loop).  On the first match, rhs is returned with the
// matched substitution applied.  Otherwise the term is returned
// unchanged.
//
// thvm_rewrite_normalize iterates rewrite_step up to step_cap times
// or until the term reaches a fixpoint.
//
// Heap allocation: the substituted RHS is rebuilt (one fresh CTR cell
// block per CTR layer in rhs).  Variables substituted by sub-terms of
// the input share heap cells with the input -- no deep copy.
//
// Reuses kbo_eq (src/kbo/_.c) for structural equality.
// REWRITE_MAX_VAR / REWRITE_MAX_ARITY / RewriteSubst are declared in
// src/thvm.h so external callers can stack-allocate the substitution.

// One-way pattern matching.  pattern may contain TAG_FVR; term is the
// concrete input.  A variable seen twice must match the same sub-term
// (linear matching with consistency check via kbo_eq).
fn u8 thvm_match(Term pattern, Term term, RewriteSubst *subst) {
  switch (term_tag(pattern)) {
    case TAG_FVR: {
      u32 id = term_ext(pattern);
      if (id >= REWRITE_MAX_VAR) return 0;
      if (subst->bindings[id] == 0) {
        subst->bindings[id] = term;
        return 1;
      }
      return kbo_eq(subst->bindings[id], term);
    }
    case TAG_CTR: {
      if (term_tag(term) != TAG_CTR) return 0;
      if (term_ext(pattern) != term_ext(term)) return 0;
      u32 np = term_ctr_n(pattern);
      u32 nt = term_ctr_n(term);
      if (np != nt) return 0;
      for (u32 i = 0; i < np; i++) {
        if (!thvm_match(term_ctr_at(pattern, i), term_ctr_at(term, i), subst)) return 0;
      }
      return 1;
    }
    default: return 0;
  }
}

// Apply substitution.  TAG_FVR -> bound sub-term; TAG_CTR rebuilt with
// substituted children; everything else passes through unchanged.
fn Term thvm_subst_apply(Term t, const RewriteSubst *subst) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < REWRITE_MAX_VAR && subst->bindings[id] != 0) {
        return subst->bindings[id];
      }
      return t;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        children[i] = thvm_subst_apply(term_ctr_at(t, i), subst);
      }
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// Try each rule in order.  First successful match wins; rhs is
// returned with substitution applied.  No match: return t unchanged.
fn Term thvm_rewrite_step(Term t, const Term *lhs, const Term *rhs, u32 n_rules) {
  for (u32 i = 0; i < n_rules; i++) {
    RewriteSubst subst = {{0}};
    if (thvm_match(lhs[i], t, &subst)) {
      return thvm_subst_apply(rhs[i], &subst);
    }
  }
  return t;
}

// Iterate rewrite_step until fixpoint or step_cap exhausted.  Returns
// the last term seen.
fn Term thvm_rewrite_normalize(Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules, u32 step_cap) {
  for (u32 i = 0; i < step_cap; i++) {
    Term t2 = thvm_rewrite_step(t, lhs, rhs, n_rules);
    if (kbo_eq(t, t2)) return t;
    t = t2;
  }
  return t;
}
