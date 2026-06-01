// FOL formula -> CNF preprocessing pipeline.
//
// thvm's FOL clausal layer (src/fol/_.c) consumes FolClauses directly.
// Real-world FOL inputs are formulas with quantifiers + connectives.
// The pipeline:
//   1. NNF -- push negations down to atoms; eliminate `->` and `<->`.
//   2. Skolemize -- replace ∃-bound variables with Skolem functions
//      whose arguments are the enclosing ∀-bound variables.  After
//      this step every variable is implicitly universal.
//   3. Distribute ∨ over ∧ to reach clausal form.
//   4. Extract the conjunction-of-disjunctions as FolClause*.
//
// Formulas are encoded as Term trees with these reserved CTR labels.
// Predicate / function symbols live OUTSIDE this range (callers pick
// freely from labels < FOL_LAB_NOT).  Equality keeps the existing
// FOL_LAB_EQ = 0 convention.
//
// This tick lands NNF + the formula-label conventions; the rest of
// the pipeline (Skolemize, distribute, extract) follows in tick 8.

// Reserved labels.  Chosen high so they don't collide with typical
// user-picked predicate / function labels in tests (which sit in the
// low double-digit range).
#define FOL_LAB_NOT 0xFFF0u   // unary: ¬p
#define FOL_LAB_AND 0xFFF1u   // n-ary: p1 ∧ p2 ∧ ... ∧ pn
#define FOL_LAB_OR  0xFFF2u   // n-ary: p1 ∨ p2 ∨ ... ∨ pn
#define FOL_LAB_IMP 0xFFF3u   // binary: p -> q
#define FOL_LAB_IFF 0xFFF4u   // binary: p <-> q
#define FOL_LAB_ALL 0xFFF5u   // binary: (var, body), var = TAG_FVR
#define FOL_LAB_EX  0xFFF6u   // binary: (var, body)

fn u8 fol_is_connective(u32 label) {
  return (label >= FOL_LAB_NOT && label <= FOL_LAB_EX) ? 1u : 0u;
}

// Constructors -- pure thin wrappers over term_new_ctr.
fn Term fol_mk_not(Term p) {
  Term k[1] = { p };
  return term_new_ctr(FOL_LAB_NOT, k, 1u);
}
fn Term fol_mk_and(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_AND, k, 2u);
}
fn Term fol_mk_or(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_OR, k, 2u);
}
fn Term fol_mk_imp(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_IMP, k, 2u);
}
fn Term fol_mk_iff(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_IFF, k, 2u);
}
fn Term fol_mk_all(Term var, Term body) {
  Term k[2] = { var, body };
  return term_new_ctr(FOL_LAB_ALL, k, 2u);
}
fn Term fol_mk_ex(Term var, Term body) {
  Term k[2] = { var, body };
  return term_new_ctr(FOL_LAB_EX, k, 2u);
}

// === NNF ============================================================
//
// Push negations down to atoms; eliminate `->` and `<->`.  Returns a
// fresh Term tree whose only `¬` occurrences are on predicate atoms.
//
// Rewrite rules (applied recursively):
//   ¬¬p          -> p
//   ¬(p ∧ q)     -> ¬p ∨ ¬q
//   ¬(p ∨ q)     -> ¬p ∧ ¬q
//   ¬(p -> q)    -> p ∧ ¬q
//   ¬(p <-> q)   -> (p ∧ ¬q) ∨ (¬p ∧ q)
//   ¬(∀x.p)      -> ∃x.¬p
//   ¬(∃x.p)      -> ∀x.¬p
//   p -> q       -> ¬p ∨ q
//   p <-> q      -> (¬p ∨ q) ∧ (¬q ∨ p)
//   p ∧ q        -> nnf(p) ∧ nnf(q)
//   p ∨ q        -> nnf(p) ∨ nnf(q)
//   ∀x.p         -> ∀x.nnf(p)
//   ∃x.p         -> ∃x.nnf(p)
//
// `negate` selects positive (0) or negated (1) descent.  Atoms get
// wrapped in `¬` only when `negate == 1`.

static Term fol_nnf_rec(Term f, u8 negate);

static Term fol_nnf_atom(Term atom, u8 negate) {
  return negate ? fol_mk_not(atom) : atom;
}

static Term fol_nnf_rec(Term f, u8 negate) {
  if (term_tag(f) != TAG_CTR) {
    return fol_nnf_atom(f, negate);
  }
  u32 lab = term_ext(f);
  switch (lab) {
    case FOL_LAB_NOT: {
      // ¬p under `negate`: flip and recurse.
      return fol_nnf_rec(term_ctr_at(f, 0u), negate ^ 1u);
    }
    case FOL_LAB_AND: {
      Term p = term_ctr_at(f, 0u);
      Term q = term_ctr_at(f, 1u);
      if (negate) {
        // ¬(p ∧ q) -> ¬p ∨ ¬q
        return fol_mk_or(fol_nnf_rec(p, 1u), fol_nnf_rec(q, 1u));
      }
      return fol_mk_and(fol_nnf_rec(p, 0u), fol_nnf_rec(q, 0u));
    }
    case FOL_LAB_OR: {
      Term p = term_ctr_at(f, 0u);
      Term q = term_ctr_at(f, 1u);
      if (negate) {
        return fol_mk_and(fol_nnf_rec(p, 1u), fol_nnf_rec(q, 1u));
      }
      return fol_mk_or(fol_nnf_rec(p, 0u), fol_nnf_rec(q, 0u));
    }
    case FOL_LAB_IMP: {
      Term p = term_ctr_at(f, 0u);
      Term q = term_ctr_at(f, 1u);
      // p -> q  <=>  ¬p ∨ q
      // ¬(p -> q)  <=>  p ∧ ¬q
      if (negate) {
        return fol_mk_and(fol_nnf_rec(p, 0u), fol_nnf_rec(q, 1u));
      }
      return fol_mk_or(fol_nnf_rec(p, 1u), fol_nnf_rec(q, 0u));
    }
    case FOL_LAB_IFF: {
      Term p = term_ctr_at(f, 0u);
      Term q = term_ctr_at(f, 1u);
      // p <-> q  <=>  (¬p ∨ q) ∧ (¬q ∨ p)
      // ¬(p <-> q)  <=>  (p ∧ ¬q) ∨ (¬p ∧ q)
      if (negate) {
        Term left  = fol_mk_and(fol_nnf_rec(p, 0u), fol_nnf_rec(q, 1u));
        Term right = fol_mk_and(fol_nnf_rec(p, 1u), fol_nnf_rec(q, 0u));
        return fol_mk_or(left, right);
      }
      Term a = fol_mk_or(fol_nnf_rec(p, 1u), fol_nnf_rec(q, 0u));
      Term b = fol_mk_or(fol_nnf_rec(p, 0u), fol_nnf_rec(q, 1u));
      return fol_mk_and(a, b);
    }
    case FOL_LAB_ALL: {
      Term var  = term_ctr_at(f, 0u);
      Term body = term_ctr_at(f, 1u);
      // ¬(∀x.p) -> ∃x.¬p ;   ∀x.p -> ∀x.nnf(p).
      if (negate) {
        return fol_mk_ex(var, fol_nnf_rec(body, 1u));
      }
      return fol_mk_all(var, fol_nnf_rec(body, 0u));
    }
    case FOL_LAB_EX: {
      Term var  = term_ctr_at(f, 0u);
      Term body = term_ctr_at(f, 1u);
      if (negate) {
        return fol_mk_all(var, fol_nnf_rec(body, 1u));
      }
      return fol_mk_ex(var, fol_nnf_rec(body, 0u));
    }
    default:
      // Non-connective CTR: a predicate atom.  Wrap in ¬ iff negate.
      return fol_nnf_atom(f, negate);
  }
}

fn Term fol_nnf(Term f) {
  return fol_nnf_rec(f, 0u);
}
