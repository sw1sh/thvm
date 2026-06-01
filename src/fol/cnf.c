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

// === Skolemization ===================================================
//
// Replace every `∃y` with the Skolem term `sk_n(x1, ..., xk)` where
// x1..xk are the enclosing ∀-bound variables in scope.  After this
// step every variable is implicitly universal, so the `∀` wrappers
// also drop -- the result is a quantifier-free formula whose free
// variables are universally quantified at the outermost level.
//
// Fresh Skolem function symbols live at `FOL_LAB_SKOLEM_BASE +
// counter`, where the BASE sits well above the connective labels so
// they don't collide.  A global counter assigns them in increasing
// order; reset with fol_reset_skolem before each independent run.
//
// Caller convention: run `fol_nnf` first so the input has no `->` /
// `<->` connectives.  This Skolemizer handles `->` / `<->` defensively
// as well (it recurses through their args) -- correctness then depends
// on those subforms being NNF-equivalent.

// thvm CTR labels are encoded in EXT_MASK = 0x3FFFF (18 bits, max
// 262143).  Connectives live at 0xFFF0..0xFFF6 (~65520); user
// predicate labels typically take low values.  Skolems fit safely
// above the connectives but below the 18-bit ceiling.
#define FOL_LAB_SKOLEM_BASE 0x20000u
#define FOL_SKOLEM_MAX_UNIV  256u

static u32 g_fol_skolem_next = 0u;

fn void fol_reset_skolem(void) { g_fol_skolem_next = 0u; }

// Substitute `var_id` in `t` with `repl`; structure-shares unchanged
// subterms.  Local to this file to keep the Skolem path free of any
// thvm_subst_apply dependency (whose REWRITE_MAX_VAR cap would force
// the user to fit all formula vars under 256).
static Term fol_var_subst(Term t, u32 var_id, Term repl) {
  switch (term_tag(t)) {
    case TAG_FVR: return (term_ext(t) == var_id) ? repl : t;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n == 0u) return t;
      Term kids[REWRITE_MAX_ARITY];
      if (n > REWRITE_MAX_ARITY) return t;
      u8 changed = 0u;
      for (u32 i = 0; i < n; i++) {
        Term orig = term_ctr_at(t, i);
        kids[i] = fol_var_subst(orig, var_id, repl);
        if (kids[i] != orig) changed = 1u;
      }
      if (!changed) return t;
      return term_new_ctr(term_ext(t), kids, n);
    }
    default: return t;
  }
}

static Term fol_skolem_rec(Term f, const Term *univ, u32 n_univ) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  switch (lab) {
    case FOL_LAB_NOT:
      return fol_mk_not(fol_skolem_rec(term_ctr_at(f, 0u), univ, n_univ));
    case FOL_LAB_AND:
      return fol_mk_and(fol_skolem_rec(term_ctr_at(f, 0u), univ, n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), univ, n_univ));
    case FOL_LAB_OR:
      return fol_mk_or (fol_skolem_rec(term_ctr_at(f, 0u), univ, n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), univ, n_univ));
    case FOL_LAB_IMP:
      return fol_mk_imp(fol_skolem_rec(term_ctr_at(f, 0u), univ, n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), univ, n_univ));
    case FOL_LAB_IFF:
      return fol_mk_iff(fol_skolem_rec(term_ctr_at(f, 0u), univ, n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), univ, n_univ));
    case FOL_LAB_ALL: {
      // Extend univ scope; drop the `∀` -- everything stays implicit.
      Term var  = term_ctr_at(f, 0u);
      Term body = term_ctr_at(f, 1u);
      if (n_univ >= FOL_SKOLEM_MAX_UNIV) return f;  // bail safely
      Term new_univ[FOL_SKOLEM_MAX_UNIV];
      for (u32 i = 0; i < n_univ; i++) new_univ[i] = univ[i];
      new_univ[n_univ] = var;
      return fol_skolem_rec(body, new_univ, n_univ + 1u);
    }
    case FOL_LAB_EX: {
      Term var  = term_ctr_at(f, 0u);
      Term body = term_ctr_at(f, 1u);
      if (term_tag(var) != TAG_FVR) return f;
      u32 vid = term_ext(var);

      // Fresh Skolem function symbol; arity = current ∀-scope size.
      u32 sk_label = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next;
      g_fol_skolem_next++;
      Term sk_term;
      if (n_univ == 0u) {
        sk_term = term_new_ctr(sk_label, NULL, 0u);
      } else {
        sk_term = term_new_ctr(sk_label, (Term *)univ, n_univ);
      }
      Term substituted = fol_var_subst(body, vid, sk_term);
      return fol_skolem_rec(substituted, univ, n_univ);
    }
    default:
      // Atom / non-connective: recurse into args defensively (some
      // formulas embed nested formula-typed atoms; the common case
      // returns the atom unchanged).
      return f;
  }
}

fn Term fol_skolemize(Term f) {
  Term univ_buf[FOL_SKOLEM_MAX_UNIV];
  return fol_skolem_rec(f, univ_buf, 0u);
}

// True iff `label` is a Skolem function symbol generated by the
// pipeline.  Useful for trace printers / proof reconstruction.
fn u8 fol_is_skolem(u32 label) {
  return (label >= FOL_LAB_SKOLEM_BASE) ? 1u : 0u;
}
