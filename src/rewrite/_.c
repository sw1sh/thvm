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
      // Direct heap_read for both pattern and term's arity + children
      // (avoids term_ctr_at's redundant arity refetch per call).
      u64 pbase = term_val(pattern);
      u64 tbase = term_val(term);
      Term pn_cell = heap_read(pbase);
      Term tn_cell = heap_read(tbase);
      if (term_tag(pn_cell) != TAG_NUM || term_tag(tn_cell) != TAG_NUM) return 0;
      u32 np = (u32)term_val(pn_cell);
      u32 nt = (u32)term_val(tn_cell);
      if (np != nt) return 0;
      for (u32 i = 0; i < np; i++) {
        Term pc = heap_read(pbase + 1u + (u64)i);
        Term tc = heap_read(tbase + 1u + (u64)i);
        if (!thvm_match(pc, tc, subst)) return 0;
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
      // Direct heap_read for arity + children (avoids term_ctr_at's
      // redundant arity refetch per call).
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return t;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return t;
      Term children[REWRITE_MAX_ARITY];
      u8 changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term orig = heap_read(base + 1u + (u64)i);
        children[i] = thvm_subst_apply(orig, subst);
        if (children[i] != orig) changed = 1;
      }
      // Hash-cons skip: if no child rewrote, reuse original t pointer
      // (saves a term_new_ctr allocation per unchanged subtree).
      if (!changed) return t;
      return term_new_ctr(term_ext(t), children, n);
    }
    default: return t;
  }
}

// Try each rule at the top position only.  Returns the rewritten
// term and sets *fired = 1 on a hit; *fired = 0 if no rule matches
// here.  Internal helper used by `thvm_rewrite_step`.
static Term rewrite_try_top(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules, u8 *fired) {
  for (u32 i = 0; i < n_rules; i++) {
    RewriteSubst subst = {{0}};
    if (thvm_match(lhs[i], t, &subst)) {
      *fired = 1;
      return thvm_subst_apply(rhs[i], &subst);
    }
  }
  *fired = 0;
  return t;
}

// One outermost-leftmost rewrite anywhere in `t`.  Tries the top
// first; if no rule matches, descends into TAG_CTR children
// left-to-right, recursing.  Stops after the first successful
// rewrite (one step = one redex fired).  Returns t unchanged if
// nothing reducible was found.
//
// Stage 5.4 of docs/plans/waldmeister_ic_atp_tasks.md: extends
// the original top-only rewriter so saturation's interreduce,
// goal_check, and normalize work on compound terms.
fn Term thvm_rewrite_step(Term t, const Term *lhs, const Term *rhs, u32 n_rules) {
  u8 fired = 0;
  Term r = rewrite_try_top(t, lhs, rhs, n_rules, &fired);
  if (fired) return r;

  // Descend into CTR children, left-to-right; first sub-rewrite wins.
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return t;
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      Term original = children[i];
      Term rewritten = thvm_rewrite_step(original, lhs, rhs, n_rules);
      if (!kbo_eq(rewritten, original)) {
        children[i] = rewritten;
        return term_new_ctr(term_ext(t), children, n);
      }
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
