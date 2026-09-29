// ===================================================================
// CASC-J13 solution-output provenance
// ===================================================================
//
// J13 evaluation requires the solution output to document the
// derivation from the ORIGINAL formulae: "Proofs must have formulae
// from the problem as leaves" and "inference steps must list exactly
// those parents that are used in the inference".  The FOF front-end
// (tp_fof_load) clausifies and equationalizes silently, so a bare
// CNFRefutation whose leaves cite file() misrepresents encoded
// clauses as problem formulae.  This section captures, per TPTP
// record, the conversion lineage as it actually happened --
//
//   fof(f<i>, <role>, <original>, file('<path>', <name>)).
//   fof(f<i>_neg, negated_conjecture, ~(<original>),
//       inference(negated_conjecture, [status(cth)], [f<i>])).
//   fof(f<i>_nnf, plain, <NNF>,
//       inference(nnf_transformation, [status(thm)], [...])).
//   fof(f<i>_sk, plain, <skolemized>,
//       inference(skolemisation, [status(esa),
//                 new_symbols(skolem, [sk..])], [f<i>_nnf])).
//   cnf(c<k>, plain, <clause>,
//       inference(cnf_transformation, [status(esa)], [f<i>_sk])).
//
// -- following Vampire's record shapes (the de-facto standard; see
// its SEU140+2 output).  Engine-proof leaves that came from the
// encoding then cite inference(equality_encoding, [status(esa)],
// [c<k>]) instead of file(); encoding helper axioms (the ifeq
// dispatcher, or/not/eq truth tables, equals-goal reflexivity) are
// introduced(definition) -- they define fresh symbols, which is
// exactly TPTP's definition introduction.  def-unfold emits
// inference(definition_unfolding, [status(thm)], [u<k>, d<j>...])
// like Vampire.  Everything is captured only under --proof
// (g_prov_on); non-proof runs and non-FOF inputs are byte-identical.

static int g_prov_on = 0;              // capture enabled (--proof)

typedef struct {
  char *name, *role, *text;   // record name / role / formula text
  int   is_conj;              // role conjecture: negated for refutation
  int   src_cnf;              // source record was cnf(...) (clause text)
  int   tseitin;              // clausified definitionally
  u32   sk0, skn;             // Skolem counter range introduced here
  u32   ts0, tsn;             // Tseitin aux counter range
  Term  t_nnf, t_sk;          // pass-A stage terms (dead after load)
  char **vnames;              // scope names, nv entries
  int   nv;
  char *nnf, *sk;             // serialized stage formulas (pass B)
  int   emitted;
} ProvRec;

typedef struct {
  int   rec;                  // ProvRec index
  char *text;                 // TPTP literal-disjunction text
  int   emitted;
} ProvCnf;

typedef struct {              // def-unfold: pre-unfold clause
  char *lhs, *rhs;
  int   neg;
  int   kind, ix;             // the clause's provenance BEFORE unfolding
  int   defs[64];             // ProvDef indices applied (64 = round cap)
  int   n_defs;
  int   emitted;
} ProvUnf;

typedef struct {              // def-unfold: eliminated definition axiom
  char *lhs, *rhs;
  int   kind, ix;
  int   emitted;
} ProvDef;

typedef struct {              // equals-goal: original negative unit
  char *lhs, *rhs;
  int   kind, ix;
  int   emitted;
} ProvEqg;

static ProvRec *g_prov_rec = NULL; static int g_prov_nrec = 0, g_prov_rec_cap = 0;
static ProvCnf *g_prov_cnf = NULL; static int g_prov_ncnf = 0;
static ProvUnf *g_prov_unf = NULL; static int g_prov_nunf = 0, g_prov_unf_cap = 0;
static ProvDef *g_prov_def = NULL; static int g_prov_ndef = 0, g_prov_def_cap = 0;
static ProvEqg *g_prov_eqg = NULL; static int g_prov_neqg = 0, g_prov_eqg_cap = 0;
static int *g_prov_funnel = NULL;  static int g_prov_nfunnel = 0;
static int  g_prov_trivial_cnf = -1;   // CNF clause that clausified to $false

// Whitespace-normalized copy: newlines/tabs collapse to single spaces
// (TPTP formula texts wrap across source lines).
static char *prov_strdup_norm(const char *s, size_t n) {
  char *out = (char *)malloc(n + 1u);
  if (out == NULL) return NULL;
  size_t o = 0;
  int sp = 1;   // swallow leading whitespace
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isspace(c)) {
      if (!sp && o > 0) out[o++] = ' ';
      sp = 1;
    } else {
      out[o++] = (char)c;
      sp = 0;
    }
  }
  while (o > 0 && out[o - 1] == ' ') o--;
  out[o] = '\0';
  return out;
}

// --- stage-formula serializer (fol Term -> TPTP FOF text) -------------
// Failure-tolerant: any unsupported shape or buffer overflow sets
// g_prov_ser_fail and the caller stores NULL -- the affected leaves
// then fall back to the legacy file() citation instead of aborting a
// provable run.
static int g_prov_ser_fail = 0;

static void prov_emit_s(char *buf, size_t cap, size_t *o, const char *s) {
  size_t l = strlen(s);
  if (*o + l + 1 >= cap) { g_prov_ser_fail = 1; return; }
  memcpy(buf + *o, s, l + 1);
  *o += l;
}

static int prov_sym_name(u32 lab, char *nm, size_t cap) {
  if (lab >= FOL_LAB_SKOLEM_BASE)
    snprintf(nm, cap, "%s%u", g_fof_sk_prefix, lab - FOL_LAB_SKOLEM_BASE);
  else if (fol_is_tseitin_aux(lab))
    snprintf(nm, cap, "%s%u", g_fof_def_prefix, lab - FOL_LAB_TSEITIN_BASE);
  else if (pc_ix_of(lab) >= 0)
    snprintf(nm, cap, "%s", pc_sym[pc_ix_of(lab)]);
  else
    return 0;
  return 1;
}

static void prov_var_name(const ProvRec *pr, u32 id, char *nm, size_t cap) {
  if (pr != NULL && pr->vnames != NULL && id < (u32)pr->nv &&
      pr->vnames[id] != NULL && pr->vnames[id][0] != '\0')
    snprintf(nm, cap, "%s", pr->vnames[id]);
  else
    snprintf(nm, cap, "X%u", id);
}

static void prov_ser_term(Term t, const ProvRec *pr,
                          char *buf, size_t cap, size_t *o) {
  if (g_prov_ser_fail) return;
  char nm[PC_NAME + 16];
  if (term_tag(t) == TAG_FVR) {
    prov_var_name(pr, term_ext(t), nm, sizeof nm);
    prov_emit_s(buf, cap, o, nm);
    return;
  }
  if (term_tag(t) != TAG_CTR) { g_prov_ser_fail = 1; return; }
  u32 lab = term_ext(t);
  if (lab == PC_LAB_TRUE)  { prov_emit_s(buf, cap, o, "$true");  return; }
  if (lab == PC_LAB_FALSE) { prov_emit_s(buf, cap, o, "$false"); return; }
  if (!prov_sym_name(lab, nm, sizeof nm)) { g_prov_ser_fail = 1; return; }
  prov_emit_s(buf, cap, o, nm);
  u32 n = term_ctr_n(t);
  if (n == 0u) return;
  prov_emit_s(buf, cap, o, "(");
  for (u32 i = 0; i < n; i++) {
    if (i > 0) prov_emit_s(buf, cap, o, ",");
    prov_ser_term(term_ctr_at(t, i), pr, buf, cap, o);
  }
  prov_emit_s(buf, cap, o, ")");
}

// The formula levels run on an explicit stack of text pieces (any
// depth): a FORM item serializes a subformula, a TERM item a term (terms
// keep the recursive prov_ser_term), a TEXT item emits its string.
// Items are pushed in reverse so the text comes out left to right.
enum { PROV_SER_FORM, PROV_SER_TERM, PROV_SER_TEXT };
typedef struct { Term t; const char *s; u8 kind; } ProvSerItem;
static ProvSerItem *g_prov_ser = NULL;
static u32          g_prov_ser_cap = 0u;

static void prov_ser_push(u32 *n, u8 kind, Term t, const char *s) {
  FOL_RESERVE(g_prov_ser, g_prov_ser_cap, *n);
  g_prov_ser[(*n)++] = (ProvSerItem){ t, s, kind };
}

static void prov_ser_formula(Term f, const ProvRec *pr,
                             char *buf, size_t cap, size_t *o) {
  char nm[PC_NAME + 16];
  u32 n = 0u;
  prov_ser_push(&n, PROV_SER_FORM, f, NULL);
  while (n > 0u && !g_prov_ser_fail) {
    ProvSerItem it = g_prov_ser[--n];
    if (it.kind == PROV_SER_TEXT) {
      prov_emit_s(buf, cap, o, it.s);
      continue;
    }
    if (it.kind == PROV_SER_TERM || term_tag(it.t) == TAG_FVR) {
      prov_ser_term(it.t, pr, buf, cap, o);
      continue;
    }
    Term g = it.t;
    if (term_tag(g) != TAG_CTR) {
      g_prov_ser_fail = 1;
      break;
    }
    u32 lab = term_ext(g);
    u32 k = term_ctr_n(g);
    switch (lab) {
    case FOL_LAB_NOT: {
      Term a = term_ctr_at(g, 0u);
      if (term_tag(a) == TAG_CTR && term_ext(a) == PC_LAB_EQ &&
          term_ctr_n(a) == 2u) {
        prov_ser_push(&n, PROV_SER_TERM, term_ctr_at(a, 1u), NULL);
        prov_ser_push(&n, PROV_SER_TEXT, 0, " != ");
        prov_ser_push(&n, PROV_SER_TERM, term_ctr_at(a, 0u), NULL);
      } else {
        prov_ser_push(&n, PROV_SER_TEXT, 0, ")");
        prov_ser_push(&n, PROV_SER_FORM, a, NULL);
        prov_ser_push(&n, PROV_SER_TEXT, 0, "~(");
      }
      break;
    }
    case FOL_LAB_AND:
    case FOL_LAB_OR:
      prov_ser_push(&n, PROV_SER_TEXT, 0, ")");
      for (u32 i = k; i-- > 0u;) {
        prov_ser_push(&n, PROV_SER_FORM, term_ctr_at(g, i), NULL);
        if (i > 0u) {
          prov_ser_push(&n, PROV_SER_TEXT, 0, (lab == FOL_LAB_AND) ? " & " : " | ");
        }
      }
      prov_ser_push(&n, PROV_SER_TEXT, 0, "(");
      break;
    case FOL_LAB_IMP:
    case FOL_LAB_IFF:
      prov_ser_push(&n, PROV_SER_TEXT, 0, ")");
      prov_ser_push(&n, PROV_SER_FORM, term_ctr_at(g, 1u), NULL);
      prov_ser_push(&n, PROV_SER_TEXT, 0, (lab == FOL_LAB_IMP) ? " => " : " <=> ");
      prov_ser_push(&n, PROV_SER_FORM, term_ctr_at(g, 0u), NULL);
      prov_ser_push(&n, PROV_SER_TEXT, 0, "(");
      break;
    case FOL_LAB_ALL:
    case FOL_LAB_EX: {
      prov_emit_s(buf, cap, o, (lab == FOL_LAB_ALL) ? "! [" : "? [");
      Term cur = g;
      int first = 1;
      while (term_tag(cur) == TAG_CTR && term_ext(cur) == lab &&
             term_ctr_n(cur) == 2u) {
        Term v = term_ctr_at(cur, 0u);
        if (term_tag(v) != TAG_FVR) {
          g_prov_ser_fail = 1;
          break;
        }
        if (!first) {
          prov_emit_s(buf, cap, o, ",");
        }
        prov_var_name(pr, term_ext(v), nm, sizeof nm);
        prov_emit_s(buf, cap, o, nm);
        first = 0;
        cur = term_ctr_at(cur, 1u);
      }
      prov_emit_s(buf, cap, o, "] : (");
      prov_ser_push(&n, PROV_SER_TEXT, 0, ")");
      prov_ser_push(&n, PROV_SER_FORM, cur, NULL);
      break;
    }
    default:   // atom
      if (lab == PC_LAB_EQ && k == 2u) {
        prov_ser_push(&n, PROV_SER_TERM, term_ctr_at(g, 1u), NULL);
        prov_ser_push(&n, PROV_SER_TEXT, 0, " = ");
        prov_ser_push(&n, PROV_SER_TERM, term_ctr_at(g, 0u), NULL);
      } else {
        prov_ser_term(g, pr, buf, cap, o);
      }
      break;
    }
  }
}

// Collect free variables (a quantifier-free skolemized formula's
// implicit universals) so the emitted fof record is closed.  Bound
// occurrences (under a surviving quantifier) are excluded.
// `bound`/`seen` are indexed by variable id (sized by the caller from
// prov_var_span); `order` receives each free id once, first occurrence
// (pre-order, left to right, over an explicit stack: a binder queues
// the restore of its variable's old mark beneath its body).
typedef struct { Term t; u32 v; u8 restore, saved; } ProvFvItem;
static ProvFvItem *g_prov_fv = NULL;
static u32         g_prov_fv_cap = 0u;

static void prov_free_vars(Term f, u8 *bound, u8 *seen, u32 *order, u32 *n) {
  u32 ns = 0u;
  FOL_RESERVE(g_prov_fv, g_prov_fv_cap, ns);
  g_prov_fv[ns++] = (ProvFvItem){ f, 0u, 0u, 0u };
  while (ns > 0u) {
    ProvFvItem it = g_prov_fv[--ns];
    if (it.restore) {
      bound[it.v] = it.saved;
      continue;
    }
    Term x = it.t;
    if (term_tag(x) == TAG_FVR) {
      u32 id = term_ext(x);
      if (!bound[id] && !seen[id]) {
        seen[id] = 1u;
        order[(*n)++] = id;
      }
      continue;
    }
    if (term_tag(x) != TAG_CTR) {
      continue;
    }
    u32 lab = term_ext(x);
    if ((lab == FOL_LAB_ALL || lab == FOL_LAB_EX) && term_ctr_n(x) == 2u &&
        term_tag(term_ctr_at(x, 0u)) == TAG_FVR) {
      u32 v = term_ext(term_ctr_at(x, 0u));
      FOL_RESERVE(g_prov_fv, g_prov_fv_cap, ns + 2u);
      g_prov_fv[ns++] = (ProvFvItem){ 0, v, 1u, bound[v] };
      bound[v] = 1u;
      g_prov_fv[ns++] = (ProvFvItem){ term_ctr_at(x, 1u), 0u, 0u, 0u };
      continue;
    }
    u32 k = term_ctr_n(x);
    FOL_RESERVE(g_prov_fv, g_prov_fv_cap, ns + k);
    for (u32 i = k; i-- > 0u;) {
      g_prov_fv[ns++] = (ProvFvItem){ term_ctr_at(x, i), 0u, 0u, 0u };
    }
  }
}

// One past the largest variable id in `f` (binder variables included).
static u32 prov_var_span(Term f) {
  u32 m = 0u, ns = 0u;
  FOL_RESERVE(g_fol_walk, g_fol_walk_cap, ns);
  g_fol_walk[ns++] = f;
  while (ns > 0u) {
    Term x = g_fol_walk[--ns];
    if (term_tag(x) == TAG_FVR) {
      if (term_ext(x) + 1u > m) {
        m = term_ext(x) + 1u;
      }
    } else if (term_tag(x) == TAG_CTR) {
      u32 k = term_ctr_n(x);
      FOL_RESERVE(g_fol_walk, g_fol_walk_cap, ns + k);
      for (u32 i = 0; i < k; i++) {
        g_fol_walk[ns++] = term_ctr_at(x, i);
      }
    }
  }
  return m;
}

// Serialize a formula, wrapping free variables in an outer `! [..] :`.
static char *prov_formula_text(Term f, const ProvRec *pr) {
  static char buf[1 << 18];
  size_t o = 0;
  g_prov_ser_fail = 0;
  u32 span = prov_var_span(f);
  u8  *bound = (u8 *)calloc((size_t)span + 1u, 1u);
  u8  *seen  = (u8 *)calloc((size_t)span + 1u, 1u);
  u32 *order = (u32 *)malloc(((size_t)span + 1u) * sizeof(u32));
  if (bound == NULL || seen == NULL || order == NULL) {
    free(bound); free(seen); free(order);
    return NULL;
  }
  u32 nfree = 0;
  prov_free_vars(f, bound, seen, order, &nfree);
  if (nfree > 0) {
    prov_emit_s(buf, sizeof buf, &o, "! [");
    char nm[PC_NAME + 16];
    for (u32 i = 0; i < nfree; i++) {
      if (i > 0) prov_emit_s(buf, sizeof buf, &o, ",");
      prov_var_name(pr, order[i], nm, sizeof nm);
      prov_emit_s(buf, sizeof buf, &o, nm);
    }
    prov_emit_s(buf, sizeof buf, &o, "] : (");
    prov_ser_formula(f, pr, buf, sizeof buf, &o);
    prov_emit_s(buf, sizeof buf, &o, ")");
  } else {
    prov_ser_formula(f, pr, buf, sizeof buf, &o);
  }
  free(bound); free(seen); free(order);
  if (g_prov_ser_fail) return NULL;
  return strdup(buf);
}

// Serialize one FolClause as a TPTP cnf literal disjunction (implicit
// universal quantification; variables named X<id>).
static char *prov_clause_text(const FolClause *c) {
  static char buf[1 << 18];
  size_t o = 0;
  g_prov_ser_fail = 0;
  if (c == NULL) return NULL;
  if (c->n_lits == 0u) return strdup("$false");
  for (u32 i = 0; i < c->n_lits; i++) {
    if (i > 0) prov_emit_s(buf, sizeof buf, &o, " | ");
    Term a = c->lits[i].atom;
    if (term_tag(a) == TAG_CTR && term_ext(a) == PC_LAB_EQ &&
        term_ctr_n(a) == 2u) {
      prov_ser_term(term_ctr_at(a, 0u), NULL, buf, sizeof buf, &o);
      prov_emit_s(buf, sizeof buf, &o, c->lits[i].sign ? " != " : " = ");
      prov_ser_term(term_ctr_at(a, 1u), NULL, buf, sizeof buf, &o);
    } else {
      if (c->lits[i].sign) prov_emit_s(buf, sizeof buf, &o, "~");
      if (term_tag(a) == TAG_CTR && term_ext(a) == PC_LAB_TRUE)
        prov_emit_s(buf, sizeof buf, &o, "$true");
      else if (term_tag(a) == TAG_CTR && term_ext(a) == PC_LAB_FALSE)
        prov_emit_s(buf, sizeof buf, &o, "$false");
      else
        prov_ser_term(a, NULL, buf, sizeof buf, &o);
    }
  }
  if (g_prov_ser_fail) return NULL;
  return strdup(buf);
}

