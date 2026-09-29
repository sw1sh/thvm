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
// The Skolem and Tseitin families sit above the user labels; a caller
// with a signature past the connectives (fol_set_label_floor) moves both
// up before allocating any.
static u32 g_fol_lab_tseitin = 0x10000u;
static u32 g_fol_lab_skolem  = 0x20000u;
#define FOL_LAB_SKOLEM_BASE g_fol_lab_skolem

static u32 g_fol_skolem_next = 0u;
// Set when a Skolem or Tseitin family runs out of labels (the CTR label
// field is 18 bits): a label past its range would alias an existing
// symbol, so the clausification must be abandoned, never wrapped.
static u8  g_fol_sk_out = 0u;
static u8  g_fol_ts_out = 0u;

fn void fol_reset_skolem(void) { g_fol_skolem_next = 0u; g_fol_sk_out = 0u; }
fn u8   fol_labels_exhausted(void) { return (u8)(g_fol_sk_out | g_fol_ts_out); }

// Substitute `var_id` in `t` with `repl`; structure-shares unchanged
// subterms.  Local to this file to keep the Skolem path free of any
// thvm_subst_apply dependency (whose REWRITE_MAX_VAR cap would force
// the user to fit all formula vars under 256).
static Term fol_var_subst(Term t, u32 var_id, Term repl) {
  switch (term_tag(t)) {
    case TAG_FVR: return (term_ext(t) == var_id) ? repl : t;
    case TAG_CTR: {
      // An inner quantifier over the SAME variable shadows it: the
      // substitution must not cross the binder (it would replace the
      // binder's own var slot with a non-variable term, wedging the
      // Skolemizer and leaving the quantifier in the "CNF").
      u32 lab = term_ext(t);
      if (lab == FOL_LAB_ALL || lab == FOL_LAB_EX) {
        Term var = term_ctr_at(t, 0u);
        if (term_tag(var) == TAG_FVR && term_ext(var) == var_id) return t;
      }
      u32 n = term_ctr_n(t);
      if (n == 0u) return t;
      Term stk[REWRITE_MAX_ARITY];
      Term *kids = term_kids_buf(stk, n);
      u8 changed = 0u;
      for (u32 i = 0; i < n; i++) {
        Term orig = term_ctr_at(t, i);
        kids[i] = fol_var_subst(orig, var_id, repl);
        if (kids[i] != orig) changed = 1u;
      }
      Term r = changed ? term_new_ctr(term_ext(t), kids, n) : t;
      term_kids_release(kids, stk);
      return r;
    }
    default: return t;
  }
}

// Variable-id occurrence stamps shared by the Skolemizer and the
// Tseitin/chunking passes: stamp[id] == epoch marks id as seen in the
// current walk.  Grown on demand (a Coq-exported record binds 1300
// variables; a fixed table silently dropped the rest).
static u32 *g_fol_vstamp = NULL;
static u32  g_fol_vstamp_cap = 0u;
static u32  g_fol_vepoch = 0u;

static void fol_vstamp_fit(u32 id) {
  if (id < g_fol_vstamp_cap) return;
  u32 ncap = (g_fol_vstamp_cap == 0u) ? 1024u : g_fol_vstamp_cap;
  while (ncap <= id) ncap *= 2u;
  u32 *ns = (u32 *)realloc(g_fol_vstamp, (size_t)ncap * sizeof(u32));
  if (ns == NULL) {
    fprintf(stderr, "thvm: out of memory in clausification\n");
    abort();
  }
  memset(ns + g_fol_vstamp_cap, 0, (size_t)(ncap - g_fol_vstamp_cap) * sizeof(u32));
  g_fol_vstamp = ns;
  g_fol_vstamp_cap = ncap;
}

static void fol_vepoch_next(void) {
  if (++g_fol_vepoch == 0u) {
    if (g_fol_vstamp != NULL) memset(g_fol_vstamp, 0, (size_t)g_fol_vstamp_cap * sizeof(u32));
    g_fol_vepoch = 1u;
  }
}

// Stamp every variable id occurring in `t` (binders included).
static void fol_stamp_vars(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      fol_vstamp_fit(id);
      g_fol_vstamp[id] = g_fol_vepoch;
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) fol_stamp_vars(term_ctr_at(t, i));
      return;
    }
    default: return;
  }
}

static u8 fol_var_stamped(u32 id) {
  return (id < g_fol_vstamp_cap && g_fol_vstamp[id] == g_fol_vepoch) ? 1u : 0u;
}

// The universal scope of the Skolemizer: one shared stack (a scope
// copy per binder was O(depth^2), and a fixed 256-slot copy returned
// the formula with its quantifiers still inside past that depth).
static Term *g_fol_sk_univ = NULL;
static u32   g_fol_sk_univ_cap = 0u;
static Term *g_fol_sk_args = NULL;
static u32   g_fol_sk_args_cap = 0u;

static void fol_sk_fit(Term **a, u32 *cap, u32 need) {
  if (need < *cap) return;
  u32 ncap = (*cap == 0u) ? 256u : *cap;
  while (ncap <= need) ncap *= 2u;
  Term *na = (Term *)realloc(*a, (size_t)ncap * sizeof(Term));
  if (na == NULL) {
    fprintf(stderr, "thvm: out of memory in Skolemization\n");
    abort();
  }
  *a = na;
  *cap = ncap;
}

static Term fol_skolem_rec(Term f, u32 n_univ) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  switch (lab) {
    case FOL_LAB_NOT:
      return fol_mk_not(fol_skolem_rec(term_ctr_at(f, 0u), n_univ));
    case FOL_LAB_AND:
      return fol_mk_and(fol_skolem_rec(term_ctr_at(f, 0u), n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), n_univ));
    case FOL_LAB_OR:
      return fol_mk_or (fol_skolem_rec(term_ctr_at(f, 0u), n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), n_univ));
    case FOL_LAB_IMP:
      return fol_mk_imp(fol_skolem_rec(term_ctr_at(f, 0u), n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), n_univ));
    case FOL_LAB_IFF:
      return fol_mk_iff(fol_skolem_rec(term_ctr_at(f, 0u), n_univ),
                        fol_skolem_rec(term_ctr_at(f, 1u), n_univ));
    case FOL_LAB_ALL: {
      // Extend univ scope; drop the `∀` -- everything stays implicit.
      fol_sk_fit(&g_fol_sk_univ, &g_fol_sk_univ_cap, n_univ);
      g_fol_sk_univ[n_univ] = term_ctr_at(f, 0u);
      return fol_skolem_rec(term_ctr_at(f, 1u), n_univ + 1u);
    }
    case FOL_LAB_EX: {
      Term var  = term_ctr_at(f, 0u);
      Term body = term_ctr_at(f, 1u);
      if (term_tag(var) != TAG_FVR) return f;
      u32 vid = term_ext(var);

      // Fresh Skolem function symbol over the ∀-scope.  A scope wider
      // than the engine's native arity keeps only the universals the
      // body mentions (dedup'd, scope order) -- the same Skolemization
      // semantically (a witness cannot depend on a variable its
      // formula never mentions), without 50-argument Skolem terms from
      // the deep binder nests of the Coq/Mizar exports.  Narrow scopes
      // keep every universal, as the solved FOF set was validated with.
      if (FOL_LAB_SKOLEM_BASE + g_fol_skolem_next > (u32)EXT_MASK) {
        g_fol_sk_out = 1u;
        return f;
      }
      u32 sk_label = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next;
      g_fol_skolem_next++;
      Term sk_term;
      if (n_univ == 0u) {
        sk_term = term_new_ctr(sk_label, NULL, 0u);
      } else if (n_univ <= REWRITE_MAX_ARITY) {
        sk_term = term_new_ctr(sk_label, g_fol_sk_univ, n_univ);
      } else {
        fol_vepoch_next();
        fol_stamp_vars(body);
        u32 na = 0u;
        for (u32 i = 0; i < n_univ; i++) {
          Term u = g_fol_sk_univ[i];
          if (term_tag(u) != TAG_FVR || !fol_var_stamped(term_ext(u))) continue;
          u32 dup = 0u;
          for (u32 j = 0; j < na && !dup; j++) dup = (g_fol_sk_args[j] == u);
          if (dup) continue;
          fol_sk_fit(&g_fol_sk_args, &g_fol_sk_args_cap, na);
          g_fol_sk_args[na++] = u;
        }
        sk_term = term_new_ctr(sk_label, na ? g_fol_sk_args : NULL, na);
      }
      Term substituted = fol_var_subst(body, vid, sk_term);
      return fol_skolem_rec(substituted, n_univ);
    }
    default:
      // Atom / non-connective: recurse into args defensively (some
      // formulas embed nested formula-typed atoms; the common case
      // returns the atom unchanged).
      return f;
  }
}

fn Term fol_skolemize(Term f) {
  return fol_skolem_rec(f, 0u);
}

// True iff `label` is a Skolem function symbol generated by the
// pipeline.  Useful for trace printers / proof reconstruction.
fn u8 fol_is_skolem(u32 label) {
  return (label >= FOL_LAB_SKOLEM_BASE) ? 1u : 0u;
}

// === CNF distribution ===============================================
//
// Take an NNF + skolemized formula (whose only connectives are AND,
// OR, NOT-on-atom) and rewrite it into a conjunction-of-disjunctions:
//
//   distribute(p ∧ q) = distribute(p) ∧ distribute(q)
//   distribute(p ∨ q):
//     dp = distribute(p),  dq = distribute(q)
//     if dp = (a ∧ b):   distribute((a ∨ dq) ∧ (b ∨ dq))
//     if dq = (a ∧ b):   distribute((dp ∨ a) ∧ (dp ∨ b))
//     else:              dp ∨ dq
//   distribute(literal) = literal
//
// Worst-case exponential in nested-OR depth -- the classical Tseitin
// alternative (intro auxiliary atoms) avoids that blowup but enlarges
// the signature.  For initial coverage we lean on the simpler
// distribution; Tseitin-CNF is a follow-up if a real bench hits the
// blowup.

fn Term fol_distribute(Term f) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  if (lab == FOL_LAB_AND) {
    Term p = term_ctr_at(f, 0u);
    Term q = term_ctr_at(f, 1u);
    return fol_mk_and(fol_distribute(p), fol_distribute(q));
  }
  if (lab == FOL_LAB_OR) {
    Term dp = fol_distribute(term_ctr_at(f, 0u));
    Term dq = fol_distribute(term_ctr_at(f, 1u));
    // (a ∧ b) ∨ dq  =>  (a ∨ dq) ∧ (b ∨ dq)
    if (term_tag(dp) == TAG_CTR && term_ext(dp) == FOL_LAB_AND) {
      Term a = term_ctr_at(dp, 0u);
      Term b = term_ctr_at(dp, 1u);
      return fol_mk_and(fol_distribute(fol_mk_or(a, dq)),
                        fol_distribute(fol_mk_or(b, dq)));
    }
    // dp ∨ (a ∧ b)  =>  (dp ∨ a) ∧ (dp ∨ b)
    if (term_tag(dq) == TAG_CTR && term_ext(dq) == FOL_LAB_AND) {
      Term a = term_ctr_at(dq, 0u);
      Term b = term_ctr_at(dq, 1u);
      return fol_mk_and(fol_distribute(fol_mk_or(dp, a)),
                        fol_distribute(fol_mk_or(dp, b)));
    }
    return fol_mk_or(dp, dq);
  }
  // NOT-on-atom or literal atom -- leave alone.
  return f;
}

// === clause extraction ==============================================
//
// Walk a fully-distributed (NNF + skolemized + distributed) formula
// and emit one FolClause per top-level disjunct.

// Append `lit` to the literals array, growing if needed.
static void fol_extract_lit(FolLit **lits, u32 *n, u32 *cap, FolLit lit) {
  if (*n >= *cap) {
    u32 ncap = (*cap == 0u) ? 8u : (*cap) * 2u;
    FolLit *grow = (FolLit *)realloc(*lits, (size_t)ncap * sizeof(FolLit));
    if (grow == NULL) return;
    *lits = grow;
    *cap = ncap;
  }
  (*lits)[*n] = lit;
  *n += 1u;
}

// Walk an OR subtree and append every literal into `*lits`.
static void fol_extract_or(Term f, FolLit **lits, u32 *n, u32 *cap) {
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_OR) {
    fol_extract_or(term_ctr_at(f, 0u), lits, n, cap);
    fol_extract_or(term_ctr_at(f, 1u), lits, n, cap);
    return;
  }
  FolLit lit;
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_NOT) {
    lit.atom = term_ctr_at(f, 0u);
    lit.sign = 1u;
  } else {
    lit.atom = f;
    lit.sign = 0u;
  }
  fol_extract_lit(lits, n, cap, lit);
}

static void fol_push_clause(FolClause ***out, u32 *n, u32 *cap, FolClause *c) {
  if (*n >= *cap) {
    u32 ncap = (*cap == 0u) ? 8u : (*cap) * 2u;
    FolClause **grow = (FolClause **)realloc(*out, (size_t)ncap * sizeof(FolClause *));
    if (grow == NULL) { fol_clause_free(c); return; }
    *out = grow;
    *cap = ncap;
  }
  (*out)[*n] = c;
  *n += 1u;
}

static void fol_push_lits(FolClause ***out, u32 *n, u32 *cap,
                          const FolLit *lits, u32 nl) {
  FolClause *c = fol_clause_new(nl);
  if (c == NULL) return;
  for (u32 i = 0; i < nl; i++) c->lits[i] = lits[i];
  fol_push_clause(out, n, cap, c);
}

static Term fol_fresh_aux(const Term *vars, u32 n_vars);   // Tseitin, below

// Distinct variables of a subformula (or of a chunked clause's shared
// part), first-occurrence order, in one shared growable list.
static Term *g_fol_ts_vars = NULL;
static u32   g_fol_ts_vars_cap = 0u;

// Append (once, first occurrence) every variable of `t` stamped in the
// current epoch; a taken variable's stamp is cleared.
static void fol_collect_shared(Term t, u32 *n) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (!fol_var_stamped(id)) return;
      g_fol_vstamp[id] = 0u;
      fol_sk_fit(&g_fol_ts_vars, &g_fol_ts_vars_cap, *n);
      g_fol_ts_vars[(*n)++] = t;
      return;
    }
    case TAG_CTR: {
      u32 nk = term_ctr_n(t);
      for (u32 i = 0; i < nk; i++) fol_collect_shared(term_ctr_at(t, i), n);
      return;
    }
    default: return;
  }
}

// Literals per piece of a chunked wide clause.  WHY 8: an all-negative
// (goal) clause then chunks into Horn rules ~L1..~L7 | a1,
// ~a1 | ~L8..~L13 | a2, ... whose bodies the Horn release can join (it
// takes at most 8 conditions); 63-literal pieces were rules no Horn lane
// could use.
#define FOL_WIDE_PIECE 8u

// Chunk a disjunction into pieces of at most FOL_WIDE_PIECE literals,
// naming each remainder:
//   A | B  ~~>  { A | a(x),  ~a(x) | B },  x = vars(A) & vars(B).
// Sound: any model of A | B satisfies both with a(x) := !y B(x,y)
// (y = the rest of B's variables): where A fails, B holds for every
// y.  Resolving on a(x) gives A | B back.  The names come from the
// Tseitin family, so the proof layer declares them as definitions.
static void fol_append_pieces(FolClause ***out, u32 *n, u32 *cap,
                              const FolLit *lits, u32 nl) {
  FolLit buf[FOL_WIDE_PIECE];
  FolLit prev = { 0, 0u };
  u32 have_prev = 0u;
  u32 i = 0u;
  while (nl - i > FOL_WIDE_PIECE - have_prev) {
    u32 take = FOL_WIDE_PIECE - 1u - have_prev;
    u32 k = 0u;
    if (have_prev) buf[k++] = prev;
    for (u32 j = 0; j < take; j++) buf[k++] = lits[i + j];
    i += take;
    // Shared variables: those of everything so far that the rest uses.
    fol_vepoch_next();
    for (u32 j = i; j < nl; j++) fol_stamp_vars(lits[j].atom);
    u32 nv = 0u;
    for (u32 j = 0; j < i; j++) fol_collect_shared(lits[j].atom, &nv);
    Term aux = fol_fresh_aux(g_fol_ts_vars, nv);
    buf[k++] = (FolLit){ .atom = aux, .sign = 0u };
    fol_push_lits(out, n, cap, buf, k);
    prev = (FolLit){ .atom = aux, .sign = 1u };
    have_prev = 1u;
  }
  u32 k = 0u;
  if (have_prev) buf[k++] = prev;
  for (; i < nl; i++) buf[k++] = lits[i];
  fol_push_lits(out, n, cap, buf, k);
}

// Variable-disjoint components of a clause: union-find over literal
// indices, a variable's first literal kept in g_fol_vowner (valid where
// g_fol_vstamp carries the current epoch).
static u32 *g_fol_vowner = NULL;
static u32  g_fol_vowner_cap = 0u;

static u32 fol_uf_find(u32 *par, u32 x) {
  while (par[x] != x) {
    par[x] = par[par[x]];
    x = par[x];
  }
  return x;
}

static void fol_comp_walk(Term t, u32 lit, u32 *par) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (fol_var_stamped(id)) {
        u32 a = fol_uf_find(par, lit), b = fol_uf_find(par, g_fol_vowner[id]);
        if (a != b) par[a > b ? a : b] = (a > b) ? b : a;
        return;
      }
      fol_vstamp_fit(id);
      if (g_fol_vowner_cap < g_fol_vstamp_cap) {
        u32 *no = (u32 *)realloc(g_fol_vowner, (size_t)g_fol_vstamp_cap * sizeof(u32));
        if (no == NULL) {
          fprintf(stderr, "thvm: out of memory in clausification\n");
          abort();
        }
        g_fol_vowner = no;
        g_fol_vowner_cap = g_fol_vstamp_cap;
      }
      g_fol_vstamp[id] = g_fol_vepoch;
      g_fol_vowner[id] = lit;
      return;
    }
    case TAG_CTR: {
      u32 nk = term_ctr_n(t);
      for (u32 i = 0; i < nk; i++) fol_comp_walk(term_ctr_at(t, i), lit, par);
      return;
    }
    default: return;
  }
}

// A disjunction wider than FOL_MAX_LITS (a 201-literal domain closure
// X = c1 | ... | X = c201; an 85-way negated existential conjecture)
// used to fail clause allocation and vanish -- the record then
// "clausified" to nothing.  Its variable-disjoint components C1..Cm
// (m >= 2) are named first, each by a fresh propositional atom:
//   C1 | ... | Cm  ~~>  { Cj | dj  (Cj all negative),  Cj | ~nj (else),
//                         ~d.. | n.. }
// Sound: with components sharing no variable, C1 | ... | Cm holds iff
// some !Cj holds; a model of the clause satisfies the named set with
// dj := ~!Cj and nj := !Cj, and resolving the top clause against the
// namings gives the clause back.  An all-negative component keeps its
// naming clause Horn (the body of dj), so a goal such as
// ~(?[X]:p1(X) & ... & ?[X]:p85(X)) becomes 85 one-condition rules and
// a propositional goal instead of one 85-condition join.  Every clause
// wider than FOL_WIDE_PIECE is then chunked (fol_append_pieces).
static u32 g_fol_wide_clauses = 0u;   // clauses chunked (FOFSTATS)

static void fol_append_wide(FolClause ***out, u32 *n, u32 *cap,
                            const FolLit *lits, u32 nl) {
  g_fol_wide_clauses++;
  u32 *par = (u32 *)malloc((size_t)nl * sizeof(u32));
  u32 *start = (u32 *)calloc((size_t)nl + 1u, sizeof(u32));
  u32 *order = (u32 *)malloc((size_t)nl * sizeof(u32));
  FolLit *top = (FolLit *)malloc((size_t)nl * sizeof(FolLit));
  FolLit *comp = (FolLit *)malloc((size_t)(nl + 1u) * sizeof(FolLit));
  if (par == NULL || start == NULL || order == NULL || top == NULL || comp == NULL) {
    fprintf(stderr, "thvm: out of memory in clausification\n");
    abort();
  }
  for (u32 i = 0; i < nl; i++) par[i] = i;
  fol_vepoch_next();
  for (u32 i = 0; i < nl; i++) fol_comp_walk(lits[i].atom, i, par);
  // Literals grouped by component root (its smallest literal index),
  // input order kept within one.
  for (u32 i = 0; i < nl; i++) par[i] = fol_uf_find(par, i);
  for (u32 i = 0; i < nl; i++) start[par[i] + 1u]++;
  for (u32 r = 0; r < nl; r++) start[r + 1u] += start[r];
  for (u32 i = 0; i < nl; i++) order[start[par[i]]++] = i;
  for (u32 r = nl; r > 0; r--) start[r] = start[r - 1u];
  start[0] = 0u;
  u32 m = 0u;
  for (u32 r = 0; r < nl; r++) {
    if (start[r + 1u] > start[r]) m++;
  }
  if (m < 2u) {
    fol_append_pieces(out, n, cap, lits, nl);
  } else {
    u32 nt = 0u;
    for (u32 r = 0; r < nl; r++) {
      u32 b = start[r], e = start[r + 1u];
      if (b == e) continue;
      u32 k = 0u;
      u8 all_neg = 1u;
      for (u32 q = b; q < e; q++) {
        comp[k++] = lits[order[q]];
        if (lits[order[q]].sign == 0u) all_neg = 0u;
      }
      Term aux = fol_fresh_aux(NULL, 0u);
      comp[k++] = (FolLit){ .atom = aux, .sign = all_neg ? 0u : 1u };
      top[nt++] = (FolLit){ .atom = aux, .sign = all_neg ? 1u : 0u };
      fol_append_pieces(out, n, cap, comp, k);
    }
    fol_append_pieces(out, n, cap, top, nt);
  }
  free(comp);
  free(top);
  free(order);
  free(start);
  free(par);
}

// Append a clause built from `or_root` to `*out`, growing as needed.
static void fol_append_clause(FolClause ***out, u32 *n, u32 *cap, Term or_root) {
  FolLit *lits = NULL;
  u32 nl = 0u, capl = 0u;
  fol_extract_or(or_root, &lits, &nl, &capl);
  if (nl > FOL_MAX_LITS) fol_append_wide(out, n, cap, lits, nl);
  else fol_push_lits(out, n, cap, lits, nl);
  free(lits);
}

// Walk the AND-of-... tree and emit a clause per top-level disjunct.
static void fol_extract_and(Term f, FolClause ***out, u32 *n, u32 *cap) {
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_AND) {
    fol_extract_and(term_ctr_at(f, 0u), out, n, cap);
    fol_extract_and(term_ctr_at(f, 1u), out, n, cap);
    return;
  }
  fol_append_clause(out, n, cap, f);
}

fn FolClause **fol_extract_clauses(Term cnf_formula, u32 *n_out) {
  if (n_out == NULL) return NULL;
  FolClause **out = NULL;
  u32 n = 0u, cap = 0u;
  fol_extract_and(cnf_formula, &out, &n, &cap);
  *n_out = n;
  return out;
}

// === end-to-end formula -> clauses ==================================
//
// One-shot: f -> NNF -> Skolem -> distribute -> extract.  Returns a
// heap-allocated array of `*n_out` FolClause*; caller owns each
// clause (free via fol_clause_free) and the array (free via free()).
// Resets the Skolem counter so each call is independent.
fn FolClause **fol_formula_to_clauses(Term f, u32 *n_out) {
  if (n_out == NULL) return NULL;
  fol_reset_skolem();
  Term nnf  = fol_nnf(f);
  Term sk   = fol_skolemize(nnf);
  Term cnf_ = fol_distribute(sk);
  return fol_extract_clauses(cnf_, n_out);
}

// === Tseitin structural CNF =========================================
//
// Naïve `fol_distribute` is O(exp) in nested-OR depth.  The Tseitin
// transformation produces a clause set whose size is linear in the
// input formula by introducing a fresh auxiliary atom for every
// compound subformula and encoding the equivalence `aux ↔ G` as a
// small set of clauses.
//
// Auxiliaries are predicates whose arguments are the FREE variables
// of the subformula they name -- so the aux properly captures the
// subformula's truth value at a particular variable binding.
//
// Caller must run fol_nnf + fol_skolemize first; this pass only
// handles ∧, ∨, ¬-on-atom, and atoms.

#define FOL_LAB_TSEITIN_BASE g_fol_lab_tseitin

// Place the Tseitin family at `floor` (>= 0x10000, rounded up to 4096)
// and the Skolem family 0x10000 above it.  0 when no Skolem room would
// remain.  Only before the first Tseitin/Skolem label of a problem.
fn u8 fol_set_label_floor(u32 floor) {
  if (floor < 0x10000u) floor = 0x10000u;
  floor = (floor + 0xFFFu) & ~0xFFFu;
  if (floor + 0x10000u + 0x8000u > (u32)EXT_MASK) return 0u;
  g_fol_lab_tseitin = floor;
  g_fol_lab_skolem  = floor + 0x10000u;
  return 1u;
}

static u32 g_fol_tseitin_next = 0u;

fn void fol_reset_tseitin(void) { g_fol_tseitin_next = 0u; g_fol_ts_out = 0u; }

fn u8 fol_is_tseitin_aux(u32 label) {
  return (label >= FOL_LAB_TSEITIN_BASE && label < FOL_LAB_SKOLEM_BASE) ? 1u : 0u;
}

// The auxiliary predicate naming a subformula carries EVERY variable
// of it: a truncated argument list (the old 128-variable cap) makes
// `aux(v1..v128) <-> G(v1..v200)` assert that G ignores v129..v200 --
// a strengthening, i.e. unsound.

static void fol_collect_vars_rec(Term t, u32 *n) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (fol_var_stamped(id)) return;
      fol_vstamp_fit(id);
      g_fol_vstamp[id] = g_fol_vepoch;
      fol_sk_fit(&g_fol_ts_vars, &g_fol_ts_vars_cap, *n);
      g_fol_ts_vars[(*n)++] = t;
      return;
    }
    case TAG_CTR: {
      u32 nk = term_ctr_n(t);
      for (u32 i = 0; i < nk; i++) fol_collect_vars_rec(term_ctr_at(t, i), n);
      return;
    }
    default: return;
  }
}

static u32 fol_collect_vars(Term t) {
  u32 n = 0u;
  fol_vepoch_next();
  fol_collect_vars_rec(t, &n);
  return n;
}

// Build a fresh auxiliary atom A_n(v1, ..., vk) from the var list.
static Term fol_fresh_aux(const Term *vars, u32 n_vars) {
  if (FOL_LAB_TSEITIN_BASE + g_fol_tseitin_next >= FOL_LAB_SKOLEM_BASE) {
    g_fol_ts_out = 1u;   // callers abandon the clause set
    return term_new_ctr(FOL_LAB_TSEITIN_BASE, NULL, 0u);
  }
  u32 label = FOL_LAB_TSEITIN_BASE + g_fol_tseitin_next;
  g_fol_tseitin_next++;
  return term_new_ctr(label, n_vars ? vars : NULL, n_vars);
}

// Append a clause from a literal list (literals are copied).
static void fol_tseitin_emit(FolClause ***out, u32 *n, u32 *cap,
                             const FolLit *lits, u32 nl) {
  FolClause *c = fol_clause_new(nl);
  if (c == NULL) return;
  for (u32 i = 0; i < nl; i++) c->lits[i] = lits[i];
  if (*n >= *cap) {
    u32 ncap = (*cap == 0u) ? 8u : (*cap) * 2u;
    FolClause **grow = (FolClause **)realloc(*out, (size_t)ncap * sizeof(FolClause *));
    if (grow == NULL) { fol_clause_free(c); return; }
    *out = grow;
    *cap = ncap;
  }
  (*out)[*n] = c;
  *n += 1u;
}

// Decompose `f` into a "name" literal that's equisatisfiable with f.
// For an atom or ¬-atom, the literal IS the name (no aux needed).
// For a compound (∧ or ∨), introduce an auxiliary atom, emit the
// encoding clauses for aux ↔ f, and return aux as the name.
static FolLit fol_tseitin_rec(Term f, FolClause ***out, u32 *n, u32 *cap) {
  // ¬atom case.
  if (term_tag(f) == TAG_CTR && term_ext(f) == FOL_LAB_NOT) {
    Term inner = term_ctr_at(f, 0u);
    return (FolLit){ .atom = inner, .sign = 1u };
  }
  // Pure atom (any non-connective CTR or FVR).
  if (term_tag(f) != TAG_CTR || !fol_is_connective(term_ext(f))) {
    return (FolLit){ .atom = f, .sign = 0u };
  }

  u32 lab = term_ext(f);
  Term left  = term_ctr_at(f, 0u);
  Term right = term_ctr_at(f, 1u);
  FolLit name_b = fol_tseitin_rec(left,  out, n, cap);
  FolLit name_c = fol_tseitin_rec(right, out, n, cap);

  u32 n_vars = fol_collect_vars(f);
  Term aux = fol_fresh_aux(g_fol_ts_vars, n_vars);

  FolLit neg_aux = { .atom = aux, .sign = 1u };
  FolLit pos_aux = { .atom = aux, .sign = 0u };
  FolLit neg_b   = { .atom = name_b.atom, .sign = (u8)(name_b.sign ^ 1u) };
  FolLit neg_c   = { .atom = name_c.atom, .sign = (u8)(name_c.sign ^ 1u) };

  if (lab == FOL_LAB_AND) {
    // aux ↔ B ∧ C
    //   ¬aux ∨ B            ¬aux ∨ C            ¬B ∨ ¬C ∨ aux
    FolLit c1[2] = { neg_aux, name_b };
    FolLit c2[2] = { neg_aux, name_c };
    FolLit c3[3] = { neg_b,  neg_c, pos_aux };
    fol_tseitin_emit(out, n, cap, c1, 2);
    fol_tseitin_emit(out, n, cap, c2, 2);
    fol_tseitin_emit(out, n, cap, c3, 3);
  } else {                     // FOL_LAB_OR
    // aux ↔ B ∨ C
    //   ¬aux ∨ B ∨ C        ¬B ∨ aux            ¬C ∨ aux
    FolLit c1[3] = { neg_aux, name_b, name_c };
    FolLit c2[2] = { neg_b, pos_aux };
    FolLit c3[2] = { neg_c, pos_aux };
    fol_tseitin_emit(out, n, cap, c1, 3);
    fol_tseitin_emit(out, n, cap, c2, 2);
    fol_tseitin_emit(out, n, cap, c3, 2);
  }
  return pos_aux;
}

fn FolClause **fol_tseitin_extract_clauses(Term nnf_skolemized, u32 *n_out) {
  if (n_out == NULL) return NULL;
  // Counter reset is the CALLER's job (the thvm.h contract): a
  // multi-record problem resets once and lets the counter run across
  // records so aux labels never collide between formulas.
  FolClause **out = NULL;
  u32 n = 0u, cap = 0u;
  FolLit root = fol_tseitin_rec(nnf_skolemized, &out, &n, &cap);
  // Assert the root: emit `{ root }` as a unit clause.
  fol_tseitin_emit(&out, &n, &cap, &root, 1u);
  *n_out = n;
  return out;
}

// End-to-end Tseitin pipeline: f -> NNF -> Skolem -> Tseitin CNF.
// Drop-in replacement for fol_formula_to_clauses on
// deeply-nested formulas where naïve distribute blows up.
fn FolClause **fol_formula_to_clauses_tseitin(Term f, u32 *n_out) {
  if (n_out == NULL) return NULL;
  fol_reset_skolem();
  fol_reset_tseitin();
  Term nnf = fol_nnf(f);
  Term sk  = fol_skolemize(nnf);
  return fol_tseitin_extract_clauses(sk, n_out);
}
