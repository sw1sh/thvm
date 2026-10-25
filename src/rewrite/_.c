// thvm_rewrite_* - one-shot equational rewriter on TAG_CTR + TAG_FVR.
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

// The walks in this file and in src/unify/_.c keep their pending
// frames on an explicit stack (REWRITE_WALK_INL entries on the C stack,
// then the heap via term_stack_grow), so a deep term costs heap, never
// C stack.  Each visits nodes in the recursion's left-to-right
// depth-first order, binds and allocates in that order, and returns
// what the recursion returned.
enum { REWRITE_WALK_INL = 64 };

// Two CTR layout cells walked in lockstep (pattern / subject, or the
// two sides of a unification).
typedef struct {
  u64 pbase;
  u64 tbase;
  u32 i;
  u32 n;
} RewritePairFrame;

// A CTR rebuilt from its children's results: child i of the layout
// cell at `base` is heap_read(base + 1 + i).
typedef struct {
  Term t;
  u64  base;
  u32  i;
  u32  n;
} RewriteNodeFrame;

// The finished frame's node: rebuilt over `kids` (its children's
// results) when one of them changed, else the original node itself
// (hash-cons skip -- no term_new_ctr for an untouched subtree).
static inline Term rewrite_node_rebuild(const RewriteNodeFrame *f,
                                        const Term *kids) {
  u8 changed = 0u;
  for (u32 k = 0; k < f->n; k++) {
    if (kids[k] != heap_read(f->base + 1u + (u64)k)) {
      changed = 1u;
    }
  }
  return changed ? term_new_ctr(term_ext(f->t), kids, f->n) : f->t;
}

// One-way pattern matching.  pattern may contain TAG_FVR; term is the
// concrete input.  A variable seen twice must match the same sub-term
// (linear matching with consistency check via kbo_eq).
fn u8 thvm_match(Term pattern, Term term, RewriteSubst *subst) {
  RewritePairFrame  fr_inl[REWRITE_WALK_INL];
  RewritePairFrame *fr  = fr_inl;
  u32               nfr = 0u;
  u32               cfr = REWRITE_WALK_INL;
  u8                ok  = 1u;
  for (;;) {
    switch (term_tag(pattern)) {
      case TAG_FVR: {
        u32 id = term_ext(pattern);
        if (id >= REWRITE_MAX_VAR) {
          ok = 0u;
          goto done;
        }
        if (subst->bindings[id] == 0) {
          subst->bindings[id] = term;
        } else if (!kbo_eq(subst->bindings[id], term)) {
          ok = 0u;
          goto done;
        }
        break;
      }
      case TAG_CTR: {
        if (term_tag(term) != TAG_CTR || term_ext(pattern) != term_ext(term)) {
          ok = 0u;
          goto done;
        }
        // Direct heap_read for both pattern and term's arity + children
        // (avoids term_ctr_at's redundant arity refetch per node).
        u64 pbase = term_val(pattern);
        u64 tbase = term_val(term);
        Term pn_cell = heap_read(pbase);
        Term tn_cell = heap_read(tbase);
        if (term_tag(pn_cell) != TAG_NUM || term_tag(tn_cell) != TAG_NUM) {
          ok = 0u;
          goto done;
        }
        u32 np = (u32)term_val(pn_cell);
        u32 nt = (u32)term_val(tn_cell);
        if (np != nt) {
          ok = 0u;
          goto done;
        }
        if (np > 0u) {
          if (nfr == cfr) {
            fr = (RewritePairFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].pbase = pbase;
          fr[nfr].tbase = tbase;
          fr[nfr].i     = 0u;
          fr[nfr].n     = np;
          nfr++;
        }
        break;
      }
      default:
        // ATP lazy-rule sentinel tripwire (THVM_ATP_SENTINEL_STRICT=1):
        // a TAG_NUM pattern with the lazy-store value tags reaching the
        // matcher means some consumer read a rule slot raw instead of
        // materializing through atp_rule_term/the mirrors -- the silent
        // never-matches failure class behind the lazy+classic formation
        // collapse (e798dba8).  Abort loudly so the blind site can be
        // enumerated empirically instead of by code audit.  Zero cost
        // unset (one cached getenv).
        if (term_tag(pattern) == TAG_NUM &&
            (term_val(pattern) == 0xFA151u || term_val(pattern) == 0xFA152u)) {
          static int strict = -1;
          if (strict < 0) {
            const char *e = getenv("THVM_ATP_SENTINEL_STRICT");
            strict = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
          }
          if (strict) {
            fprintf(stderr,
                    "SENTINEL-STRICT: lazy rule sentinel 0x%llX reached "
                    "thvm_match as a pattern\n",
                    (unsigned long long)term_val(pattern));
            abort();
          }
        }
        ok = 0u;
        goto done;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    RewritePairFrame *f = &fr[nfr - 1u];
    pattern = heap_read(f->pbase + 1u + (u64)f->i);
    term    = heap_read(f->tbase + 1u + (u64)f->i);
    f->i++;
  }
done:
  term_stack_release(fr, fr_inl);
  return ok;
}

// Apply substitution.  TAG_FVR -> bound sub-term; TAG_CTR rebuilt with
// substituted children; everything else passes through unchanged.
fn Term thvm_subst_apply(Term t, const RewriteSubst *subst) {
  RewriteNodeFrame  fr_inl[REWRITE_WALK_INL];
  Term              val_inl[REWRITE_WALK_INL];
  RewriteNodeFrame *fr   = fr_inl;
  Term             *val  = val_inl;
  u32               nfr  = 0u;
  u32               cfr  = REWRITE_WALK_INL;
  u32               nval = 0u;
  u32               cval = REWRITE_WALK_INL;
  for (;;) {
    u8   leaf = 1u;
    Term v    = t;
    switch (term_tag(t)) {
      case TAG_FVR: {
        u32 id = term_ext(t);
        if (id < REWRITE_MAX_VAR && subst->bindings[id] != 0) {
          v = subst->bindings[id];
        }
        break;
      }
      case TAG_CTR: {
        u64  base   = term_val(t);
        Term n_cell = heap_read(base);
        u32  n      = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0u;
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (RewriteNodeFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].t    = t;
          fr[nfr].base = base;
          fr[nfr].i    = 0u;
          fr[nfr].n    = n;
          nfr++;
          leaf = 0u;
        }
        break;
      }
      default:
        break;
    }
    if (leaf) {
      if (nval == cval) {
        val = (Term *)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = v;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      RewriteNodeFrame *f = &fr[nfr - 1u];
      nval -= f->n;
      val[nval] = rewrite_node_rebuild(f, &val[nval]);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    RewriteNodeFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  Term out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

#ifdef THVM_ATP_AC
// AC-modulo one-way match.  Declared in thvm.h alongside the AC
// mask setters; defined in src/atp/ac.c (transitively included by
// src/atp/_.c into the same single-TU build).  Under THVM_ATP_AC
// rewrite_try_top consults the engine-global AC mask: a non-zero
// mask routes match attempts through atp_match_ac so AC-permuted
// subjects align with the pattern.
fn u8 atp_match_ac(Term pattern, Term subject,
                   const AtpAcInfo *ac, RewriteSubst *subst);
#endif

// Try each rule at the top position only.  Returns the rewritten
// term and sets *fired = 1 on a hit; *fired = 0 if no rule matches
// here.  Internal helper used by `thvm_rewrite_step`.
static Term rewrite_try_top(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules, u8 *fired) {
#ifdef THVM_ATP_AC
  u64 ac_mask = thvm_atp_get_ac_mask();
#endif
  for (u32 i = 0; i < n_rules; i++) {
    RewriteSubst subst = {{0}};
#ifdef THVM_ATP_AC
    if (ac_mask != 0ull) {
      AtpAcInfo ac = { .ac_mask = ac_mask };
      if (atp_match_ac(lhs[i], t, &ac, &subst)) {
        *fired = 1;
        return thvm_subst_apply(rhs[i], &subst);
      }
      continue;
    }
#endif
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
// left-to-right.  Stops after the first successful rewrite (one step =
// one redex fired) whose child result differs (kbo_eq) from the child;
// a child whose step returns an equal term passes to the next sibling.
// Returns t unchanged if nothing reducible was found.
//
// Extends the original top-only rewriter so saturation's interreduce,
// goal_check, and normalize work on compound terms.
//
// The descent keeps one frame per open CTR (`i` = the child being
// stepped); a child's result returns to its frame, which either
// rebuilds around a changed child and returns in turn, or moves on.
fn Term thvm_rewrite_step(Term t, const Term *lhs, const Term *rhs, u32 n_rules) {
  RewriteNodeFrame  fr_inl[REWRITE_WALK_INL];
  RewriteNodeFrame *fr  = fr_inl;
  u32               nfr = 0u;
  u32               cfr = REWRITE_WALK_INL;
  Term              res;
  for (;;) {
    u8 fired = 0;
    res = rewrite_try_top(t, lhs, rhs, n_rules, &fired);
    if (!fired) {
      u32 n = (term_tag(t) == TAG_CTR) ? term_ctr_n(t) : 0u;
      if (n > 0u) {
        if (nfr == cfr) {
          fr = (RewriteNodeFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].t    = t;
        fr[nfr].base = term_val(t);
        fr[nfr].i    = 0u;
        fr[nfr].n    = n;
        nfr++;
        t = heap_read(term_val(t) + 1u);
        continue;
      }
      res = t;
    }
    for (;;) {
      if (nfr == 0u) {
        term_stack_release(fr, fr_inl);
        return res;
      }
      RewriteNodeFrame *f = &fr[nfr - 1u];
      if (!kbo_eq(res, heap_read(f->base + 1u + (u64)f->i))) {
        Term  stk[REWRITE_MAX_ARITY];
        Term *children = term_kids_buf(stk, f->n);
        for (u32 k = 0; k < f->n; k++) {
          children[k] = (k == f->i) ? res : heap_read(f->base + 1u + (u64)k);
        }
        res = term_new_ctr(term_ext(f->t), children, f->n);
        term_kids_release(children, stk);
        nfr--;
        continue;
      }
      f->i++;
      if (f->i < f->n) {
        t = heap_read(f->base + 1u + (u64)f->i);
        break;
      }
      res = f->t;
      nfr--;
    }
  }
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
