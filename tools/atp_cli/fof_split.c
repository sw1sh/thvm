// ===================================================================
// k-bounded ground case splitting (AVATAR-lite, --fof-split)
// ===================================================================
//
// SOUNDNESS.  Let S = H u {D_1..D_m} be the clausified set with H
// Horn and each D_j = N_j v P_j1 v ... v P_jw -- N_j the negative
// literals, every P_ji positive and GROUND.  A branch b picks one
// positive per or-clause: S_b = H u {N_j v P_jb(j) : all j}.  Each
// branch clause keeps ONE positive, so S_b is Horn; and every branch
// clause entails its or-clause (fewer disjuncts), so S_b |= S: a
// branch is a STRENGTHENING, and one refuted branch alone proves
// nothing.  Exhaustiveness is where groundness enters: take M |= S
// and any j.  Either M |= P_ji for some i -- a truth value
// independent of the clause's universally quantified variables,
// exactly because P_ji is ground -- and then M |= all(N_j v P_ji);
// or M falsifies every P_ji, forcing M |= all(N_j), and then M
// satisfies EVERY branch clause of D_j.  Either way M satisfies some
// branch clause of every D_j simultaneously, i.e. M |= S_b for some
// b.  Contrapositive: all branches unsatisfiable => S unsatisfiable.
// (For a NON-ground positive the first case breaks: M may satisfy
// different disjuncts at different instances, satisfying D_j while
// falsifying every universally quantified branch clause.  The AGT
// trichotomy axiom less(X,Y) | less(Y,X) | X = Y is exactly such a
// clause -- the gate detects it and the lane exits.)
//
// MECHANICS.  The parent detects the shape during pass B, skips the
// clausal pre-pass, and (from main, after the Horn-release early
// exits) runs one child process per branch SEQUENTIALLY, each with
// remaining-wall / branches-left: the run_schedule re-exec pattern,
// so every knob env inherits; THVM_ATP_FOF_BRANCH carries the picks.
// A child re-clausifies identically, emits each or-clause as the
// Horn clause (negatives + picked positive) -- the pure-Horn lanes
// then get the whole problem -- and exits 0 only on refutation.  The
// first open branch short-circuits the case analysis to GaveUp.
//
// PROVENANCE (--proof).  A child's branch-clause leaves cite an
// avatar-shaped assumption chain (prov_src_citation PROV_SPLIT):
//   fof(spl<j>_<i>, definition, (spl<j>_<i> <=> <P_ji>),
//       introduced(definition, [new_symbols(naming, [spl<j>_<i>])],
//                  [avatar_definition])).
//   cnf(c<k>s<i>, plain, <branch clause>,
//       inference(avatar_split_clause, [status(thm)],
//                 [c<k>, spl<j>_<i>])).
// and the parent stitches the child Proof blocks, renames each
// branch's terminal record br<b>_*, then closes the DAG with the
// split clauses over the spl names, one avatar_contradiction_clause
// per branch, and a final avatar_sat_refutation $false -- the
// Vampire record shapes, checkable by tools/casc/proof_dag_check.py.

#define FOF_SPLIT_MAX_OR   8    // detection cap (--fof-split-k <= this)
#define FOF_SPLIT_MAX_POS  8    // per-or-clause width cap
#define FOF_SPLIT_MAX_BR   64   // branch-product cap

typedef struct {
  int   cnf_ix;                      // clausified-clause index (c<k>)
  int   width;                       // positive-literal count
  char *pi_text[FOF_SPLIT_MAX_POS];  // ground positive texts  (--proof)
} FofSplitOr;
static FofSplitOr g_fof_split_or[FOF_SPLIT_MAX_OR];
static int g_fof_split_nor     = 0;  // or-clauses seen (or-ordinal)
static int g_fof_split_unfit   = 0;  // a shape gate failed
static int g_fof_split_pending = 0;  // parent: orchestrate after load
static int g_fof_split_child   = 0;  // THVM_ATP_FOF_BRANCH present
static int g_fof_split_choice[FOF_SPLIT_MAX_OR];
static int g_fof_split_nchoice = 0;

static int fof_split_mode(void) {
  const char *e = getenv("THVM_ATP_FOF_SPLIT");
  return (e == NULL || *e == '\0') ? 0 : atoi(e);
}

static int fof_split_k(void) {
  const char *e = getenv("THVM_ATP_FOF_SPLIT_K");
  int k = (e != NULL && *e != '\0') ? atoi(e) : 4;
  if (k < 1) k = 1;
  if (k > FOF_SPLIT_MAX_OR) k = FOF_SPLIT_MAX_OR;
  return k;
}

static u8 fof_term_is_ground(Term t) {
  if (term_tag(t) == TAG_FVR) return 0u;
  if (term_tag(t) != TAG_CTR) return 1u;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (!fof_term_is_ground(term_ctr_at(t, i))) return 0u;
  return 1u;
}

// Child init: parse THVM_ATP_FOF_BRANCH ("i0,i1,..." -- the picked
// positive per or-clause, in pass-B scan order).
static void fof_split_read_branch(void) {
  const char *b = getenv("THVM_ATP_FOF_BRANCH");
  g_fof_split_child = (b != NULL && *b != '\0');
  g_fof_split_nchoice = 0;
  if (!g_fof_split_child) return;
  const char *p = b;
  while (*p != '\0' && g_fof_split_nchoice < FOF_SPLIT_MAX_OR) {
    g_fof_split_choice[g_fof_split_nchoice++] = atoi(p);
    const char *cma = strchr(p, ',');
    if (cma == NULL) break;
    p = cma + 1;
  }
}

// Branch-clause text: the or-clause's (classified) negative literals
// plus the picked positive -- prov_clause_text on a synthetic clause.
static char *fof_split_branch_text(FolClause *cl, int pos_lit) {
  FolLit lits[FOL_MAX_LITS];
  u32 n = 0;
  for (u32 i = 0; i < cl->n_lits; i++) {
    if (cl->lits[i].sign != 1u) continue;
    Term l, r;
    if (fof_lit_classify(&cl->lits[i], &l, &r) != FOF_LIT_PAIR) continue;
    if (n < FOL_MAX_LITS) lits[n++] = cl->lits[i];
  }
  if (n < FOL_MAX_LITS) lits[n++] = cl->lits[pos_lit];
  FolClause tmp = { lits, n };
  return prov_clause_text(&tmp);
}

// Parent-side scan, called per or-clause during pass B: record the
// stash and evaluate the shape gates (width cap, GROUND positives).
// pos_src maps the classified positive index to the clause's literal.
static void fof_split_scan_or(FolClause *cl, int cnf_ix,
                              const int *pos_src, int npos) {
  if (g_fof_split_nor >= FOF_SPLIT_MAX_OR || npos > FOF_SPLIT_MAX_POS) {
    g_fof_split_unfit = 1;
    return;
  }
  FofSplitOr *so = &g_fof_split_or[g_fof_split_nor];
  memset(so, 0, sizeof *so);
  so->cnf_ix = cnf_ix;
  so->width = npos;
  for (int i = 0; i < npos; i++) {
    const FolLit *pl = &cl->lits[pos_src[i]];
    if (!fof_term_is_ground(pl->atom)) { g_fof_split_unfit = 1; return; }
    if (g_cli_want_proof) {
      FolLit one = *pl;
      FolClause tmp = { &one, 1u };
      so->pi_text[i] = prov_clause_text(&tmp);
    }
  }
  g_fof_split_nor++;
}

// --- avatar-shaped branch-assumption provenance (--proof) ------------
typedef struct {
  int   cnf_ix, orj, pick;
  char *pi_text;   // the picked ground positive
  char *br_text;   // the branch clause (negatives + picked positive)
  int   emitted;
} ProvSplit;
static ProvSplit *g_prov_split = NULL;
static int g_prov_nsplit = 0, g_prov_split_cap = 0;

static int prov_split_new(FolClause *cl, int cnf_ix, int orj,
                          int pick, int pos_lit) {
  if (g_prov_nsplit >= g_prov_split_cap) {
    g_prov_split_cap = (g_prov_split_cap == 0) ? 8 : g_prov_split_cap * 2;
    g_prov_split = (ProvSplit *)realloc(
        g_prov_split, (size_t)g_prov_split_cap * sizeof(ProvSplit));
    if (g_prov_split == NULL) { g_prov_nsplit = 0; return -1; }
  }
  ProvSplit *sp = &g_prov_split[g_prov_nsplit];
  memset(sp, 0, sizeof *sp);
  sp->cnf_ix = cnf_ix;
  sp->orj    = orj;
  sp->pick   = pick;
  FolLit one = cl->lits[pos_lit];
  FolClause tmp = { &one, 1u };
  sp->pi_text = prov_clause_text(&tmp);
  sp->br_text = fof_split_branch_text(cl, pos_lit);
  if (sp->pi_text == NULL || sp->br_text == NULL) return -1;
  return g_prov_nsplit++;
}

// --- capture (called from tp_fof_load) --------------------------------
static int prov_rec_new(const char *name, size_t name_len,
                        const char *role, const char *text, int src_cnf) {
  if (g_prov_nrec >= g_prov_rec_cap) {
    g_prov_rec_cap = (g_prov_rec_cap == 0) ? 256 : g_prov_rec_cap * 2;
    g_prov_rec = (ProvRec *)realloc(g_prov_rec,
                                    (size_t)g_prov_rec_cap * sizeof(ProvRec));
    if (g_prov_rec == NULL) { g_prov_on = 0; return -1; }
  }
  ProvRec *r = &g_prov_rec[g_prov_nrec];
  memset(r, 0, sizeof *r);
  r->name = prov_strdup_norm(name, name_len);
  r->role = strdup(role);
  r->text = prov_strdup_norm(text, strlen(text));
  r->src_cnf = src_cnf;
  return g_prov_nrec++;
}

static void prov_rec_stage(int rec, int is_conj, Term nnf, Term sk,
                           u32 sk0, u32 skn, u32 ts0, u32 tsn, int tseitin,
                           const PcParse *scope) {
  if (rec < 0 || rec >= g_prov_nrec) return;
  ProvRec *r = &g_prov_rec[rec];
  r->is_conj = is_conj;
  r->t_nnf = nnf;
  r->t_sk  = sk;
  r->sk0 = sk0; r->skn = skn;
  r->ts0 = ts0; r->tsn = tsn;
  r->tseitin = tseitin;
  r->nv = scope->nv;
  if (scope->nv > 0) {
    r->vnames = (char **)calloc((size_t)scope->nv, sizeof(char *));
    if (r->vnames != NULL)
      for (int i = 0; i < scope->nv; i++) r->vnames[i] = strdup(pc_vname[i]);
  }
}

// Pass-B serialization: fresh symbol names are final, stage terms
// still live.  Fills the per-record formula texts and the per-clause
// CNF texts (g_prov_cnf parallels the load-time cls[] array).
static void prov_serialize_all(FolClause **cls, const int *cls_rec, int ncl) {
  g_prov_cnf = (ProvCnf *)calloc((size_t)(ncl + 1), sizeof(ProvCnf));
  if (g_prov_cnf == NULL) { g_prov_on = 0; return; }
  g_prov_ncnf = ncl;
  for (int c = 0; c < ncl; c++) {
    g_prov_cnf[c].rec = (cls_rec != NULL) ? cls_rec[c] : -1;
    g_prov_cnf[c].text = prov_clause_text(cls[c]);
  }
  for (int r = 0; r < g_prov_nrec; r++) {
    ProvRec *pr = &g_prov_rec[r];
    if (pr->t_nnf != 0) pr->nnf = prov_formula_text(pr->t_nnf, pr);
    if (pr->t_sk != 0 && pr->t_sk != pr->t_nnf)
      pr->sk = prov_formula_text(pr->t_sk, pr);
    pr->t_nnf = pr->t_sk = 0;   // heap handles die with the load pass
  }
}

// --- transform capture (def-unfold / equals-goal) ----------------------
static int prov_def_new(const TpClause *cl) {
  if (g_prov_ndef >= g_prov_def_cap) {
    g_prov_def_cap = (g_prov_def_cap == 0) ? 16 : g_prov_def_cap * 2;
    g_prov_def = (ProvDef *)realloc(g_prov_def,
                                    (size_t)g_prov_def_cap * sizeof(ProvDef));
    if (g_prov_def == NULL) { g_prov_on = 0; return -1; }
  }
  ProvDef *d = &g_prov_def[g_prov_ndef];
  memset(d, 0, sizeof *d);
  d->lhs = strdup(cl->lhs);
  d->rhs = strdup(cl->rhs);
  d->kind = cl->prov_kind;
  d->ix   = cl->prov_ix;
  return g_prov_ndef++;
}

static int prov_unf_new(const TpClause *cl) {
  if (g_prov_nunf >= g_prov_unf_cap) {
    g_prov_unf_cap = (g_prov_unf_cap == 0) ? 64 : g_prov_unf_cap * 2;
    g_prov_unf = (ProvUnf *)realloc(g_prov_unf,
                                    (size_t)g_prov_unf_cap * sizeof(ProvUnf));
    if (g_prov_unf == NULL) { g_prov_on = 0; return -1; }
  }
  ProvUnf *u = &g_prov_unf[g_prov_nunf];
  memset(u, 0, sizeof *u);
  u->lhs = strdup(cl->lhs);
  u->rhs = strdup(cl->rhs);
  u->neg = cl->neg;
  u->kind = cl->prov_kind;
  u->ix   = cl->prov_ix;
  return g_prov_nunf++;
}

static void prov_unf_add_def(int unf, int def) {
  if (unf < 0 || unf >= g_prov_nunf || def < 0) return;
  ProvUnf *u = &g_prov_unf[unf];
  for (int i = 0; i < u->n_defs; i++) if (u->defs[i] == def) return;
  if (u->n_defs < (int)(sizeof u->defs / sizeof u->defs[0]))
    u->defs[u->n_defs++] = def;
}

static int prov_eqg_new(const TpClause *cl) {
  if (g_prov_neqg >= g_prov_eqg_cap) {
    g_prov_eqg_cap = (g_prov_eqg_cap == 0) ? 16 : g_prov_eqg_cap * 2;
    g_prov_eqg = (ProvEqg *)realloc(g_prov_eqg,
                                    (size_t)g_prov_eqg_cap * sizeof(ProvEqg));
    if (g_prov_eqg == NULL) { g_prov_on = 0; return -1; }
  }
  ProvEqg *e = &g_prov_eqg[g_prov_neqg];
  memset(e, 0, sizeof *e);
  e->lhs = strdup(cl->lhs);
  e->rhs = strdup(cl->rhs);
  e->kind = cl->prov_kind;
  e->ix   = cl->prov_ix;
  return g_prov_neqg++;
}

// --- conversion-chain emission -----------------------------------------
// prov_emit_rec prints the record's fof chain (once); prov_rec_last
// names the chain's last node.  prov_emit_cnf adds the cnf record.
// All return 0 when the capture is incomplete (serialization failed),
// in which case the caller falls back to the legacy file() citation.

static void prov_rec_last(const ProvRec *r, int rec, char *buf, size_t cap) {
  if (r->sk != NULL)       snprintf(buf, cap, "f%d_sk", rec);
  else if (r->nnf != NULL) snprintf(buf, cap, "f%d_nnf", rec);
  else if (r->is_conj)     snprintf(buf, cap, "f%d_neg", rec);
  else                     snprintf(buf, cap, "f%d", rec);
}

static int prov_emit_rec(FILE *fp, int rec) {
  if (rec < 0 || rec >= g_prov_nrec) return 0;
  ProvRec *r = &g_prov_rec[rec];
  if (r->text == NULL || r->name == NULL || r->role == NULL) return 0;
  if (!r->src_cnf && r->nnf == NULL) return 0;
  if (r->emitted) return 1;
  r->emitted = 1;
  const char *form = r->src_cnf ? "cnf" : "fof";
  fprintf(fp, "%s(f%d, %s, %s, file('%s', %s)).\n",
          form, rec, r->role, r->text, g_cli_probpath,
          (r->name[0] != '\0') ? r->name : "unknown");
  char prev[32];
  snprintf(prev, sizeof prev, "f%d", rec);
  if (r->is_conj) {
    fprintf(fp, "fof(f%d_neg, negated_conjecture, ~(%s), "
                "inference(negated_conjecture, [status(cth)], [%s])).\n",
            rec, r->text, prev);
    snprintf(prev, sizeof prev, "f%d_neg", rec);
  }
  if (r->nnf != NULL) {
    fprintf(fp, "fof(f%d_nnf, plain, %s, "
                "inference(nnf_transformation, [status(thm)], [%s])).\n",
            rec, r->nnf, prev);
    snprintf(prev, sizeof prev, "f%d_nnf", rec);
  }
  if (r->sk != NULL) {
    fprintf(fp, "fof(f%d_sk, plain, %s, "
                "inference(skolemisation, [status(esa)",
            rec, r->sk);
    if (r->skn > r->sk0) {
      fprintf(fp, ", new_symbols(skolem, [");
      for (u32 s = r->sk0; s < r->skn; s++)
        fprintf(fp, "%s%s%u", (s > r->sk0) ? "," : "", g_fof_sk_prefix, s);
      fprintf(fp, "])");
    }
    fprintf(fp, "], [%s])).\n", prev);
  }
  return 1;
}

// A Tseitin definition symbol is NEW exactly once: on the first emitted
// record that mentions it.  Printing the record's whole [ts0,tsn) range on
// every clause it clausified into (the pre-2026-07-31 behaviour) repeated an
// identical 38-name list across all 8 CSR021+1 clauses and declared def36 as
// "new" on records that only USE it -- a checker verifying new-symbol
// freshness sees the symbol already in scope and rejects the annotation.
// `g_prov_newsym_seen` is the per-problem declared set; reset in tp_fof_load.
static unsigned char *g_prov_newsym_seen = NULL;
static u32            g_prov_newsym_cap  = 0;

static void prov_def_reset(void) {
  free(g_prov_newsym_seen);
  g_prov_newsym_seen = NULL;
  g_prov_newsym_cap  = 0;
}

// Claim `idx` for the record being emitted; 0 once some earlier record has.
static int prov_def_claim(u32 idx) {
  if (idx >= g_prov_newsym_cap) {
    u32 ncap = (g_prov_newsym_cap == 0u) ? 64u : g_prov_newsym_cap;
    while (idx >= ncap) ncap *= 2u;
    unsigned char *n = (unsigned char *)realloc(g_prov_newsym_seen, ncap);
    if (n == NULL) return 0;
    memset(n + g_prov_newsym_cap, 0, ncap - g_prov_newsym_cap);
    g_prov_newsym_seen = n;
    g_prov_newsym_cap  = ncap;
  }
  if (g_prov_newsym_seen[idx]) return 0;
  g_prov_newsym_seen[idx] = 1u;
  return 1;
}

// Does `text` mention <def_prefix><idx> as a whole token?  The trailing
// digit check keeps def3 from matching inside def36.
static int prov_def_mentions(const char *text, u32 idx) {
  char name[TP_NAME_LEN + 16];
  int  n = snprintf(name, sizeof name, "%s%u", g_fof_def_prefix, idx);
  if (n <= 0) return 0;
  for (const char *p = strstr(text, name); p != NULL;
       p = strstr(p + 1, name)) {
    char before = (p == text) ? '\0' : p[-1];
    char after  = p[n];
    if ((before == '\0' || (!isalnum((unsigned char)before) && before != '_'))
        && !isdigit((unsigned char)after)) {
      return 1;
    }
  }
  return 0;
}

static int prov_emit_cnf(FILE *fp, int k) {
  if (g_prov_cnf == NULL || k < 0 || k >= g_prov_ncnf) return 0;
  ProvCnf *c = &g_prov_cnf[k];
  if (c->text == NULL) return 0;
  if (!prov_emit_rec(fp, c->rec)) return 0;
  if (c->emitted) return 1;
  c->emitted = 1;
  ProvRec *r = &g_prov_rec[c->rec];
  char last[32];
  prov_rec_last(r, c->rec, last, sizeof last);
  fprintf(fp, "cnf(c%d, plain, %s, inference(cnf_transformation, "
              "[status(esa)", k, c->text);
  if (r->tseitin && r->tsn > r->ts0) {
    // Declare only the definitions THIS clause introduces: mentioned here,
    // and not already declared by an earlier-emitted clause.
    u32 first = 1u;
    for (u32 s = r->ts0; s < r->tsn; s++) {
      if (!prov_def_mentions(c->text, s)) continue;
      if (!prov_def_claim(s)) continue;
      fprintf(fp, "%s%s%u", first ? ", new_symbols(definition, [" : ",",
              g_fof_def_prefix, s);
      first = 0u;
    }
    if (!first) fprintf(fp, "])");
  }
  fprintf(fp, "], [%s])).\n", last);
  return 1;
}

// Emit the source citation for one provenance-carrying unit into
// `src` (including the leading ", " and closing ")).\n"), printing
// whatever chain records it cites to `fp` first.  `neg` picks the
// disequation rendering for the emitted virtual leaves.  Returns 0
// (caller falls back to file()) when the chain is unavailable.
static int prov_src_citation(FILE *fp, int kind, int ix, char *src, size_t cap);

static int prov_emit_unf(FILE *fp, int ix) {
  if (g_prov_unf == NULL || ix < 0 || ix >= g_prov_nunf) return 0;
  ProvUnf *u = &g_prov_unf[ix];
  if (u->lhs == NULL || u->rhs == NULL) return 0;
  char inner[512];
  if (!u->emitted) {
    // Emit the pre-unfold clause and every definition used, each with
    // its OWN provenance source (file() for verbatim problem clauses).
    if (!prov_src_citation(fp, u->kind, u->ix, inner, sizeof inner)) return 0;
    fprintf(fp, "cnf(u%d, %s, %s %s %s%s", ix,
            u->neg ? "negated_conjecture" : "axiom",
            u->lhs, u->neg ? "!=" : "=", u->rhs, inner);
    for (int i = 0; i < u->n_defs; i++) {
      ProvDef *d = &g_prov_def[u->defs[i]];
      if (d->emitted) continue;
      if (d->lhs == NULL || d->rhs == NULL) return 0;
      if (!prov_src_citation(fp, d->kind, d->ix, inner, sizeof inner)) return 0;
      fprintf(fp, "cnf(d%d, axiom, %s = %s%s", u->defs[i], d->lhs, d->rhs, inner);
      d->emitted = 1;
    }
    u->emitted = 1;
  }
  return 1;
}

// --- Horn-release chain emission (PROV_HORN) ---------------------------
// A released unit the refutation cites materializes its derivation
// cone: every ancestor release as a cnf(h<k>, ...) hyper_resolution
// record and every input clause used (rule / fact / definitional-
// expansion axiom) as a cnf(hi<j>, ...) record with its own
// conversion-chain source.  Truncated records never reach here (the
// append pass excludes them from engine input), so a 0 return is a
// hard capture bug, not a fallback path.

static int prov_emit_hpc(FILE *fp, int j) {
  if (g_hpc == NULL || j < 0 || j >= g_hpc_n) return 0;
  HProvCls *pc = &g_hpc[j];
  if (pc->trunc || pc->l == NULL || pc->r == NULL) return 0;
  if (pc->emitted) return 1;
  char inner[512];
  if (!prov_src_citation(fp, pc->kind, pc->ix, inner, sizeof inner)) return 0;
  fprintf(fp, "cnf(hi%d, %s, %s = %s%s", j,
          pc->conj ? "negated_conjecture" : "axiom", pc->l, pc->r, inner);
  pc->emitted = 1;
  return 1;
}

// The parent list of release k: "hi<rule>, <facts...>, hi<defs...>,
// hi<dispatchers...>" -- the reducer ifeqN(X,X,Y[,Z]) = Y is applied
// implicitly by every decoded modus-ponens step, and the checker's
// entailment discharge needs it among the cited parents.
static int prov_horn_plist(int k, char *buf, size_t cap) {
  const HPr *p = &g_hpr[k];
  size_t o = (size_t)snprintf(buf, cap, "hi%d", p->rule);
  for (int i = 0; i < p->nf + p->nd && o + 16 < cap; i++) {
    int e = g_hpp[p->po + i];
    if (i >= p->nf)   o += (size_t)snprintf(buf + o, cap - o, ", hi%d", e);
    else if (e >= 0)  o += (size_t)snprintf(buf + o, cap - o, ", h%d", e);
    else              o += (size_t)snprintf(buf + o, cap - o, ", hi%d", ~e);
  }
  for (int i = 0; i < g_horn_disp_hpc_n && o + 16 < cap; i++)
    o += (size_t)snprintf(buf + o, cap - o, ", hi%d", g_horn_disp_hpc[i]);
  return o + 16 < cap;
}

// Emit the input-clause records one release's derivation cites (its
// rule, its input-fact parents, its defs, the dispatcher reducers).
static int prov_emit_horn_hpcs(FILE *fp, const HPr *p) {
  if (!prov_emit_hpc(fp, p->rule)) return 0;
  for (int i = 0; i < p->nf + p->nd; i++) {
    int e = g_hpp[p->po + i];
    int hj = (i >= p->nf) ? e : (e >= 0 ? -1 : ~e);
    if (hj >= 0 && !prov_emit_hpc(fp, hj)) return 0;
  }
  for (int i = 0; i < g_horn_disp_hpc_n; i++)
    if (!prov_emit_hpc(fp, g_horn_disp_hpc[i])) return 0;
  return 1;
}

// Push release e onto the emission worklist unless already emitted.
// Returns 1 pushed, 0 already emitted, -1 invalid record.
static int prov_horn_push(int e, int *sp, int **stk, int *cap) {
  if (!horn_prov_ok(e)) return -1;
  if (g_hpr[e].emitted) return 0;
  if (*sp >= *cap) {
    *cap = (*cap == 0) ? (1 << 12) : *cap * 2;
    *stk = (int *)realloc(*stk, (size_t)*cap * sizeof(int));
    if (*stk == NULL) return -1;
  }
  (*stk)[(*sp)++] = e;
  return 1;
}

// Emit every record release k's derivation cites (its ancestors, NOT a
// record for k itself -- the caller's record plays that role).
// Iterative DFS: the release chains run thousands of steps deep.
static int prov_emit_horn_cone(FILE *fp, int k) {
  if (!horn_prov_ok(k)) return 0;
  static int *stk = NULL; static int stk_cap = 0;
  int sp = 0;
  const HPr *pk = &g_hpr[k];
  for (int i = 0; i < pk->nf; i++) {
    int e = g_hpp[pk->po + i];
    if (e >= 0 && prov_horn_push(e, &sp, &stk, &stk_cap) < 0) return 0;
  }
  while (sp > 0) {
    int j = stk[sp - 1];
    if (g_hpr[j].emitted) { sp--; continue; }
    const HPr *p = &g_hpr[j];
    int pending = 0;
    for (int i = 0; i < p->nf; i++) {
      int e = g_hpp[p->po + i];
      if (e < 0) continue;
      int st = prov_horn_push(e, &sp, &stk, &stk_cap);
      if (st < 0) return 0;
      pending += st;
    }
    if (pending) continue;
    // Parents ready (no pushes happened, so j is still the stack top):
    // input-clause records first, then this hyper-resolution record.
    if (!prov_emit_horn_hpcs(fp, p)) return 0;
    char plist[768];
    if (!prov_horn_plist(j, plist, sizeof plist)) return 0;
    if (p->l == NULL || p->r == NULL) return 0;
    fprintf(fp, "cnf(h%d, plain, %s = %s, inference(hyper_resolution, "
                "[status(thm)], [%s])).\n", j, p->l, p->r, plist);
    g_hpr[j].emitted = 1;
    sp--;
  }
  return prov_emit_horn_hpcs(fp, pk);
}

static int prov_src_citation(FILE *fp, int kind, int ix, char *src, size_t cap) {
  switch (kind) {
  case PROV_NONE:
    snprintf(src, cap, ", file('%s')).\n", g_cli_probpath);
    return 1;
  case PROV_HELPER:
    snprintf(src, cap, ", introduced(definition)).\n");
    return 1;
  case PROV_CNF:
    if (!prov_emit_cnf(fp, ix)) return 0;
    snprintf(src, cap,
             ", inference(equality_encoding, [status(esa)], [c%d])).\n", ix);
    return 1;
  case PROV_UNFOLD: {
    if (!prov_emit_unf(fp, ix)) return 0;
    ProvUnf *u = &g_prov_unf[ix];
    size_t o = (size_t)snprintf(src, cap,
        ", inference(definition_unfolding, [status(thm)], [u%d", ix);
    for (int i = 0; i < u->n_defs && o + 16 < cap; i++)
      o += (size_t)snprintf(src + o, cap - o, ", d%d", u->defs[i]);
    if (o + 8 >= cap) return 0;
    snprintf(src + o, cap - o, "])).\n");
    return 1;
  }
  case PROV_EQGOAL: {
    if (g_prov_eqg == NULL || ix < 0 || ix >= g_prov_neqg) return 0;
    ProvEqg *e = &g_prov_eqg[ix];
    if (e->lhs == NULL || e->rhs == NULL) return 0;
    if (!e->emitted) {
      char inner[512];
      if (!prov_src_citation(fp, e->kind, e->ix, inner, sizeof inner)) return 0;
      fprintf(fp, "cnf(eg%d, negated_conjecture, %s != %s%s",
              ix, e->lhs, e->rhs, inner);
      e->emitted = 1;
    }
    snprintf(src, cap,
             ", inference(equality_encoding, [status(esa)], [eg%d])).\n", ix);
    return 1;
  }
  case PROV_FUNNEL_GOAL: {
    if (g_prov_funnel == NULL || g_prov_nfunnel == 0) return 0;
    size_t o = (size_t)snprintf(src, cap,
        ", inference(equality_encoding, [status(esa)], [");
    for (int i = 0; i < g_prov_nfunnel; i++) {
      if (!prov_emit_cnf(fp, g_prov_funnel[i])) return 0;
      if (o + 16 >= cap) return 0;
      o += (size_t)snprintf(src + o, cap - o, "%sc%d",
                            (i > 0) ? ", " : "", g_prov_funnel[i]);
    }
    if (o + 8 >= cap) return 0;
    snprintf(src + o, cap - o, "])).\n");
    return 1;
  }
  case PROV_TRIVIAL_GOAL:
    if (!prov_emit_cnf(fp, ix)) return 0;
    snprintf(src, cap,
             ", inference(equality_encoding, [status(esa)], [c%d])).\n", ix);
    return 1;
  case PROV_HORN: {
    // Released unit: print its ancestor cone, then cite its own
    // hyper-resolution step directly on the leaf record.
    if (!prov_emit_horn_cone(fp, ix)) return 0;
    char plist[768];
    if (!prov_horn_plist(ix, plist, sizeof plist)) return 0;
    size_t o = (size_t)snprintf(src, cap,
        ", inference(hyper_resolution, [status(thm)], [%s])).\n", plist);
    return o + 1 < cap;
  }
  case PROV_SPLIT: {
    // fof-split branch clause: the or-clause's conversion chain, the
    // avatar definition naming the picked ground positive, and the
    // branch clause as an avatar_split_clause -- the encoded unit
    // then cites the ASSUMPTION record, never the or-clause directly
    // (a branch clause is a strengthening, not a consequence).
    if (g_prov_split == NULL || ix < 0 || ix >= g_prov_nsplit) return 0;
    ProvSplit *sp = &g_prov_split[ix];
    if (sp->pi_text == NULL || sp->br_text == NULL) return 0;
    if (!prov_emit_cnf(fp, sp->cnf_ix)) return 0;
    if (!sp->emitted) {
      fprintf(fp, "fof(spl%d_%d, definition, (spl%d_%d <=> (%s)), "
                  "introduced(definition, [new_symbols(naming, "
                  "[spl%d_%d])], [avatar_definition])).\n",
              sp->orj, sp->pick, sp->orj, sp->pick, sp->pi_text,
              sp->orj, sp->pick);
      fprintf(fp, "cnf(c%ds%d, plain, %s, "
                  "inference(avatar_split_clause, [status(thm)], "
                  "[c%d, spl%d_%d])).\n",
              sp->cnf_ix, sp->pick, sp->br_text, sp->cnf_ix,
              sp->orj, sp->pick);
      sp->emitted = 1;
    }
    snprintf(src, cap,
             ", inference(equality_encoding, [status(esa)], [c%ds%d])).\n",
             sp->cnf_ix, sp->pick);
    return 1;
  }
  default:
    return 0;
  }
}

// --- final-unit leaf table ---------------------------------------------
// Built from the TpProblem AFTER every content transform + canon,
// i.e. the exact strings the .pr carries: an engine-trace axiom leaf
// prints back to exactly one of these (either orientation).
typedef struct { char *l, *r; int neg; int kind, ix; } ProvLeaf;
static ProvLeaf *g_prov_leaf = NULL;
static int g_prov_nleaf = 0;

static void prov_build_leaf_table(const TpProblem *tp) {
  int n = 0;
  for (int i = 0; i < tp->n_cls; i++)
    if (tp->cls[i].prov_kind != PROV_NONE) n++;
  if (n == 0) return;
  g_prov_leaf = (ProvLeaf *)calloc((size_t)n, sizeof(ProvLeaf));
  if (g_prov_leaf == NULL) return;
  for (int i = 0; i < tp->n_cls; i++) {
    const TpClause *cl = &tp->cls[i];
    if (cl->prov_kind == PROV_NONE) continue;
    ProvLeaf *pl = &g_prov_leaf[g_prov_nleaf];
    pl->l = strdup(cl->lhs);
    pl->r = strdup(cl->rhs);
    pl->neg = cl->neg;
    pl->kind = cl->prov_kind;
    pl->ix = cl->prov_ix;
    if (pl->l != NULL && pl->r != NULL) g_prov_nleaf++;
  }
}

static const ProvLeaf *prov_leaf_find(const char *l, const char *r, int neg) {
  for (int i = 0; i < g_prov_nleaf; i++) {
    const ProvLeaf *pl = &g_prov_leaf[i];
    if (pl->neg != neg) continue;
    if ((strcmp(pl->l, l) == 0 && strcmp(pl->r, r) == 0) ||
        (strcmp(pl->l, r) == 0 && strcmp(pl->r, l) == 0))
      return pl;
  }
  return NULL;
}

// A failed emission attempt may have printed chain records into a
// discarded stream; clear the dedup flags so a later (real) emission
// re-prints every record it cites.
static void prov_reset_emitted(void) {
  for (int i = 0; i < g_prov_nrec; i++) g_prov_rec[i].emitted = 0;
  for (int i = 0; i < g_prov_ncnf; i++) g_prov_cnf[i].emitted = 0;
  for (int i = 0; i < g_prov_nunf; i++) g_prov_unf[i].emitted = 0;
  for (int i = 0; i < g_prov_ndef; i++) g_prov_def[i].emitted = 0;
  for (int i = 0; i < g_prov_neqg; i++) g_prov_eqg[i].emitted = 0;
  for (int i = 0; i < g_hpc_n; i++) g_hpc[i].emitted = 0;
  for (int i = 0; i < g_hpr_n; i++) g_hpr[i].emitted = 0;
  for (int i = 0; i < g_prov_nsplit; i++) g_prov_split[i].emitted = 0;
}

// --- clausal pre-pass refutation emission ------------------------------
// When the FOF clausal pre-pass (bounded resolution saturation on the
// ORIGINAL clauses) derives the empty clause under --proof, its
// refutation is the honest solution output: conversion chains for the
// input clauses used, then the recorded resolution steps, ending at
// $false.  Stashed here (the pre-pass runs before stdout rerouting);
// main prints it with the SZS status and skips the engine.
static char *g_cli_prepass_proof = NULL;

static const char *prepass_name(const CnfState *s, u32 id, const int *id2cnf,
                                int map_cap, char *buf, size_t cap) {
  if (s->trace[id].rule == FOL_INF_INPUT && id2cnf != NULL &&
      id < (u32)map_cap && id2cnf[id] >= 0)
    snprintf(buf, cap, "c%d", id2cnf[id]);
  else
    snprintf(buf, cap, "p%u", id);
  return buf;
}

// Assumption tail: the ~spl<g> disjuncts a conditional clause carries
// (the Vampire presentation of an AVATAR-conditional clause).
static void prepass_emit_tail(FILE *out, const CnfGmask *asum) {
  for (u32 g = 0; g < CNF_AVATAR_MAX_GUARDS; g++) {
    if (cnf_gm_test(asum, g)) fprintf(out, " | ~spl%u", g);
  }
}

// The avatar_definition leaf for guard g (once, def_done-deduped):
//   fof(sdef<g>, definition, (spl<g> <=> (<component>)), introduced(...)).
static int prepass_emit_def(FILE *out, CnfState *s, u32 g, u8 *def_done) {
  if (def_done[g]) return 1;
  const FolClause *comp = cnf_clause_at(s, s->guard_comp[g]);
  if (comp == NULL) return 1;   // unretrievable component: the def stays
                                // un-emitted and citations skip it
  char *txt = prov_clause_text(comp);
  if (txt == NULL) return 0;
  fprintf(out, "fof(sdef%u, definition, (spl%u <=> (%s)), "
               "introduced(definition, [new_symbols(naming, [spl%u])], "
               "[avatar_definition])).\n", g, g, txt, g);
  free(txt);
  def_done[g] = 1;
  return 1;
}

static int prepass_emit_c(FILE *out, CnfState *s, u32 id, u8 *done,
                          const int *id2cnf, int map_cap, u8 *def_done) {
  if (id == FOL_NO_PARENT || id >= s->n || done[id]) return 1;
  done[id] = 1;
  const FolTrace *t = &s->trace[id];
  const FolClause *c = cnf_clause_at(s, id);
  if (t->rule == FOL_INF_INPUT) {
    int k = (id2cnf != NULL && id < (u32)map_cap) ? id2cnf[id] : -1;
    if (k >= 0 && prov_emit_cnf(out, k)) return 1;
    // Capture incomplete for this record: cite the file directly so
    // the DAG stays closed (validator-visible degradation, never an
    // aborted proof).
    char *txt = prov_clause_text(c);
    if (txt == NULL) return 0;
    fprintf(out, "cnf(p%u, axiom, %s, file('%s')).\n",
            id, txt, g_cli_probpath);
    free(txt);
    return 1;
  }
  if (t->rule == FOL_INF_SPLIT) {
    // AVATAR component intro: guarded by spl<g>, justified by the
    // guard's definition alone (a_g -> component); the case analysis
    // over the original clause lives in the avatar_split_clause
    // records the closing block emits.
    u32 g = (u32)s->cguard[id] - 1u;
    if (!prepass_emit_def(out, s, g, def_done)) return 0;
    char *txt = prov_clause_text(c);
    if (txt == NULL) return 0;
    fprintf(out, "cnf(p%u, plain, (%s | ~spl%u), "
                 "inference(avatar_component_clause, [status(thm)], "
                 "[sdef%u])).\n", id, txt, g, g);
    free(txt);
    return 1;
  }
  if (!prepass_emit_c(out, s, t->parent_a, done, id2cnf, map_cap, def_done)) return 0;
  if (!prepass_emit_c(out, s, t->parent_b, done, id2cnf, map_cap, def_done)) return 0;
  static const char *rn[] = {
    "input", "resolution", "factoring", "superposition",
    "equality_resolution", "equality_factoring", "demodulation" };
  const char *rule = ((u32)t->rule < sizeof rn / sizeof rn[0])
                       ? rn[t->rule] : "unknown";
  const CnfGmask *asum = (s->cassume != NULL) ? &s->cassume[id] : NULL;
  char na[32];
  // Forward demodulation normalized the inference's conclusion before
  // it was stored: the inference record carries the RAW conclusion,
  // then one demodulation record per rewrite cites its unit equation.
  const CnfFdRec *fd = cnf_fdrec_at(s, id);
  if (fd != NULL && fd->n > 0u && fd->pre != NULL) {
    for (u32 k = 0; k < fd->n; k++)
      if (!prepass_emit_c(out, s, fd->rule[k], done, id2cnf, map_cap, def_done)) return 0;
  }
  u32 nfd = (fd != NULL && fd->pre != NULL) ? fd->n : 0u;
  for (u32 k = 0; k <= nfd; k++) {
    const FolClause *cc = (k == nfd) ? c : (k == 0u ? fd->pre : fd->mid[k - 1u]);
    char *txt = prov_clause_text(cc);
    if (txt == NULL) return 0;
    if (k == nfd) fprintf(out, "cnf(p%u, plain, ", id);
    else fprintf(out, "cnf(p%u_f%u, plain, ", id, k);
    if (asum != NULL && !cnf_gm_is_zero(asum)) {
      fprintf(out, "(%s", txt);
      prepass_emit_tail(out, asum);
      fputs(")", out);
    } else {
      fputs(txt, out);
    }
    free(txt);
    if (k > 0u) {
      fprintf(out, ", inference(demodulation, [status(thm)], [p%u_f%u, %s])).\n",
              id, k - 1u,
              prepass_name(s, fd->rule[k - 1u], id2cnf, map_cap, na, sizeof na));
      continue;
    }
    fprintf(out, ", inference(%s, [status(thm)], [", rule);
    int first = 1;
    if (t->parent_a != FOL_NO_PARENT && t->parent_a < s->n) {
      fputs(prepass_name(s, t->parent_a, id2cnf, map_cap, na, sizeof na), out);
      first = 0;
    }
    if (t->parent_b != FOL_NO_PARENT && t->parent_b < s->n) {
      if (!first) fputs(", ", out);
      fputs(prepass_name(s, t->parent_b, id2cnf, map_cap, na, sizeof na), out);
    }
    fputs("])).\n", out);
  }
  return 1;
}

// The AVATAR closing block: every branch refutation as an
// avatar_contradiction_clause, every split event as an
// avatar_split_clause (citing the split original + the guard
// definitions), and the terminal avatar_sat_refutation $false citing
// them all -- the family the engine's DPLL verified unsatisfiable.
static int prepass_emit_avatar(FILE *out, CnfState *s, u8 *done,
                               const int *id2cnf, int map_cap, u8 *def_done) {
  char na[32];
  for (u32 r = 0; r < s->n_refut; r++) {
    if (!prepass_emit_c(out, s, s->refut_id[r], done, id2cnf, map_cap, def_done))
      return 0;
    fprintf(out, "cnf(sct%u, plain, (", r);
    int first = 1;
    for (u32 g = 0; g < CNF_AVATAR_MAX_GUARDS; g++) {
      if (!cnf_gm_test(&s->refut_mask[r], g)) continue;
      fprintf(out, "%s~spl%u", first ? "" : " | ", g);
      first = 0;
    }
    fprintf(out, "), inference(avatar_contradiction_clause, [status(thm)], "
                 "[p%u])).\n", s->refut_id[r]);
  }
  for (u32 e = 0; e < s->n_split; e++) {
    if (!prepass_emit_c(out, s, s->split_orig[e], done, id2cnf, map_cap, def_done))
      return 0;
    for (u32 g = 0; g < CNF_AVATAR_MAX_GUARDS; g++) {
      if (!cnf_gm_test(&s->split_pos[e], g)) continue;
      if (s->guard_comp[g] < s->n &&
          !prepass_emit_def(out, s, g, def_done)) return 0;
    }
    fprintf(out, "cnf(ssp%u, plain, (", e);
    int first = 1;
    for (u32 g = 0; g < CNF_AVATAR_MAX_GUARDS; g++) {
      if (!cnf_gm_test(&s->split_neg[e], g)) continue;
      fprintf(out, "%s~spl%u", first ? "" : " | ", g);
      first = 0;
    }
    for (u32 g = 0; g < CNF_AVATAR_MAX_GUARDS; g++) {
      if (!cnf_gm_test(&s->split_pos[e], g)) continue;
      fprintf(out, "%sspl%u", first ? "" : " | ", g);
      first = 0;
    }
    fprintf(out, "), inference(avatar_split_clause, [status(thm)], [%s",
            prepass_name(s, s->split_orig[e], id2cnf, map_cap, na, sizeof na));
    for (u32 g = 0; g < CNF_AVATAR_MAX_GUARDS; g++) {
      if (!cnf_gm_test(&s->split_pos[e], g)) continue;
      if (def_done[g]) fprintf(out, ", sdef%u", g);
    }
    fputs("])).\n", out);
  }
  fputs("cnf(sat_ref, plain, $false, "
        "inference(avatar_sat_refutation, [status(thm)], [", out);
  int first = 1;
  for (u32 e = 0; e < s->n_split; e++) {
    fprintf(out, "%sssp%u", first ? "" : ", ", e);
    first = 0;
  }
  for (u32 r = 0; r < s->n_refut; r++) {
    fprintf(out, "%ssct%u", first ? "" : ", ", r);
    first = 0;
  }
  fputs("])).\n", out);
  return 1;
}

static void prepass_emit_proof(CnfState *s, const int *id2cnf, int map_cap) {
  // The outright root: an empty clause with EMPTY assumptions.  When
  // only conditional empties exist, the refutation is the AVATAR SAT
  // closure over the recorded guard theory.
  u32 root = ~0u;
  for (u32 i = 0; i < s->n; i++) {
    const FolClause *c = cnf_clause_at(s, i);
    if (c != NULL && c->n_lits == 0u &&
        (s->cassume == NULL ||
         cnf_gm_is_zero(&s->cassume[i]))) { root = i; break; }
  }
  // The avatar block claims the recorded guard theory is UNSAT --
  // only the engine's own DPLL closure (ATP_PROVED) justifies that.
  u8 avatar = (root == ~0u && s->n_refut > 0u &&
               s->status == ATP_PROVED) ? 1u : 0u;
  if (root == ~0u && !avatar) return;
  char *bufp = NULL;
  size_t plen = 0;
  FILE *ms = open_memstream(&bufp, &plen);
  if (ms == NULL) return;
  u8 *done = (u8 *)calloc(s->n, 1u);
  u8 def_done[CNF_AVATAR_MAX_GUARDS] = {0};
  int ok = (done != NULL);
  if (ok) {
    fprintf(ms, "%% SZS output start Proof for %s\n", g_cli_probpath);
    ok = avatar
           ? prepass_emit_avatar(ms, s, done, id2cnf, map_cap, def_done)
           : prepass_emit_c(ms, s, root, done, id2cnf, map_cap, def_done);
    fprintf(ms, "%% SZS output end Proof for %s\n", g_cli_probpath);
  }
  fclose(ms);
  free(done);
  if (ok && bufp != NULL) {
    g_cli_prepass_proof = bufp;
  } else {
    free(bufp);
    prov_reset_emitted();
  }
}

// --- distribution blow-up guard ------------------------------------------
// Estimate the naive-distribution clause count of an NNF+Skolemized
// formula: a literal counts 1, AND sums, OR multiplies.  Saturates at
// cap + 1 so callers compare against the bound without overflow.
static u32 fof_cnf_estimate(Term f, u32 cap) {
  if (term_tag(f) != TAG_CTR) return 1u;
  u32 lab = term_ext(f);
  if (lab != FOL_LAB_AND && lab != FOL_LAB_OR) return 1u;
  u32 a = fof_cnf_estimate(term_ctr_at(f, 0u), cap);
  u32 b = fof_cnf_estimate(term_ctr_at(f, 1u), cap);
  u64 v = (lab == FOL_LAB_AND) ? ((u64)a + (u64)b) : ((u64)a * (u64)b);
  return (v > (u64)cap) ? cap + 1u : (u32)v;
}
static u32 fof_distrib_bound(void) {
  const char *e = getenv("THVM_ATP_FOF_DISTRIB");
  if (e != NULL && atoi(e) > 0) return (u32)atoi(e);
  return 512u;
}
// Literal occurrences of an NNF+Skolemized formula (saturating at cap).
static u32 fof_atom_count(Term f, u32 cap) {
  if (term_tag(f) != TAG_CTR) return 1u;
  u32 lab = term_ext(f);
  if (lab != FOL_LAB_AND && lab != FOL_LAB_OR) return 1u;
  u64 v = (u64)fof_atom_count(term_ctr_at(f, 0u), cap) +
          (u64)fof_atom_count(term_ctr_at(f, 1u), cap);
  return (v > (u64)cap) ? cap : (u32)v;
}
// Distribute unless it BLOWS UP: past the bound AND past twice the
// formula's literal count.  A big conjunction (or one disjunct over a big
// conjunction: p(X) => q1(X) & ... & q10000(X)) distributes into as many
// clauses as it has conjuncts -- linear -- and naming its every node
// instead ran the 27MB BIO knowledge base out of Tseitin labels.
static int fof_route_tseitin(Term sk, u32 bound) {
  if (fof_cnf_estimate(sk, bound) <= bound) return 0;
  u32 atoms = fof_atom_count(sk, 0x3FFFFFFFu);
  u32 cap = (atoms > bound / 2u) ? 2u * atoms : bound;
  return fof_cnf_estimate(sk, cap) > cap;
}
static int g_fof_tseitin_recs = 0;   // records clausified definitionally

// --- clausal pre-pass SInE cone -------------------------------------------
// Large non-Horn knowledge bases (the AGT/CSR/MED FOF families run to
// thousands of clauses) swamp the bounded saturation before it ever
// touches the conjecture.  This is tp_sine_pass's trigger discipline
// (generality threshold gt=1 with the rare-cap rc=64 fresh-constant
// fix, tolerance ladder at full trigger depth) replayed at the
// FolClause level over the CLAUSIFIED set, so the pre-pass saturates
// the goal-relevant cone instead of the whole KB.  gen(lab) = number
// of clauses whose literals contain symbol label lab (predicates and
// functions alike; the equality label and the encoding glue are hubs
// and never trigger, exactly as SInE intends).  Conjecture-descended
// clauses (ccnj) plus all-negative clauses (the negated-conjecture
// shape when no role survived) seed the reached set; a clause joins
// when some reached symbol triggers it.  Dropping clauses only
// SHRINKS the saturation input -- a subset refutation is still a
// refutation, and the pre-pass never reports saturation as a result,
// so a cone miss just means giving up the pre-pass wall sooner and
// falling through to the equational lanes (an honest GaveUp).
// Returns a keep[] mask, or NULL for "keep everything" (off, OOM, or
// no seed).
enum { FSINE_CS = 48 };   // per-clause dedup'd symbol-list cap

static void fof_sine_clause_syms(Term t, u32 *lst, int *n) {
  if (term_tag(t) != TAG_CTR) return;
  u32 lab = term_ext(t);
  u8 seen = 0;
  for (int k = 0; k < *n; k++)
    if (lst[k] == lab) { seen = 1; break; }
  if (!seen && *n < (int)FSINE_CS) lst[(*n)++] = lab;
  u32 nn = term_ctr_n(t);
  for (u32 i = 0; i < nn; i++) fof_sine_clause_syms(term_ctr_at(t, i), lst, n);
}

static u8 *fof_sine_cone(FolClause **cls, const u8 *ccnj, int ncl, int thr) {
  u32 n_labels = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next + 1u;
  u32 (*csym)[FSINE_CS] = malloc((size_t)ncl * sizeof *csym);
  int *csym_n = calloc((size_t)ncl, sizeof(int));
  u32 *gen    = calloc((size_t)n_labels, sizeof(u32));
  u8  *reach  = malloc((size_t)n_labels);
  u8  *keep   = malloc((size_t)ncl);
  u8  *seed   = calloc((size_t)ncl, 1u);
  if (csym == NULL || csym_n == NULL || gen == NULL || reach == NULL ||
      keep == NULL || seed == NULL) {
    free(csym); free(csym_n); free(gen); free(reach); free(keep); free(seed);
    return NULL;
  }
  int n_seed = 0;
  for (int c = 0; c < ncl; c++) {
    for (u32 i = 0; i < cls[c]->n_lits; i++)
      fof_sine_clause_syms(cls[c]->lits[i].atom, csym[c], &csym_n[c]);
    for (int k = 0; k < csym_n[c]; k++) gen[csym[c][k]]++;
    u8 sd = ccnj[c];
    if (!sd && cls[c]->n_lits > 0u) {
      sd = 1u;
      for (u32 i = 0; i < cls[c]->n_lits && sd; i++)
        if (!cls[c]->lits[i].sign) sd = 0u;
    }
    seed[c] = sd;
    n_seed += sd;
  }
  if (n_seed == 0) {   // nothing goal-shaped to grow from: keep everything
    free(csym); free(csym_n); free(gen); free(reach); free(keep); free(seed);
    return NULL;
  }
  // Tolerance-first ladder at full trigger depth (no rounds cuts: the
  // pre-pass has no hard capacity cap, and depth amputation is what
  // loses the 15+-trigger CSR-style chains).  First cone at or under
  // the threshold wins; when even tol=1.0 stays over, the LOOSEST cone
  // (tol=1.5) stands -- the pre-pass has no capacity cap to fit, so
  // past the threshold the only cost of width is speed, while
  // over-tightening PROVABLY loses refutations (CSR201's tol=1.0 cone
  // saturates in half the wall guard: the miss is then certain for
  // that lane, where the wider cone keeps the search alive).
  const u32 gt = 1u, rc = 64u;
  const double tols[3] = { 1.5, 1.2, 1.0 };
  u8 *keep15 = NULL;   // the tol=1.5 cone, kept as the no-fit fallback
  int kept = ncl;
  double tol_used = tols[0];
  for (int a = 0; a < 3; a++) {
    double tol = tols[a];
    memset(keep, 0, (size_t)ncl);
    memset(reach, 0, (size_t)n_labels);
    for (int c = 0; c < ncl; c++) {
      if (!seed[c]) continue;
      keep[c] = 1u;
      for (int k = 0; k < csym_n[c]; k++) reach[csym[c][k]] = 1u;
    }
    for (int rd = 0; rd < 64; rd++) {
      u8 grew = 0;
      for (int c = 0; c < ncl; c++) {
        if (keep[c]) continue;
        u32 min_all = 0xffffffffu, min_ex = 0xffffffffu;
        for (int k = 0; k < csym_n[c]; k++) {
          u32 g = gen[csym[c][k]];
          if (g < min_all) min_all = g;
          if (g > gt && g < min_ex) min_ex = g;
        }
        u32 mingen = (min_ex <= rc) ? min_ex : min_all;
        u8 hit = 0;
        for (int k = 0; k < csym_n[c] && !hit; k++) {
          u32 sy = csym[c][k];
          if (!reach[sy]) continue;
          if (gen[sy] <= gt || (double)gen[sy] <= tol * (double)mingen) hit = 1;
        }
        if (hit) {
          keep[c] = 1u;
          grew = 1u;
          for (int k = 0; k < csym_n[c]; k++) reach[csym[c][k]] = 1u;
        }
      }
      if (!grew) break;
    }
    kept = 0;
    for (int c = 0; c < ncl; c++) kept += keep[c];
    tol_used = tol;
    if (kept <= thr) break;
    if (a == 0) {
      keep15 = malloc((size_t)ncl);
      if (keep15 != NULL) memcpy(keep15, keep, (size_t)ncl);
    }
  }
  if (kept > thr && keep15 != NULL) {   // no fit: the loosest cone stands
    memcpy(keep, keep15, (size_t)ncl);
    kept = 0;
    for (int c = 0; c < ncl; c++) kept += keep[c];
    tol_used = tols[0];
  }
  free(keep15);
  fprintf(stderr, "FindProof: FOF pre-pass SInE: %d/%d clause(s) "
          "(tol=%.2f gt=%u rc=%u)\n", kept, ncl, tol_used, gt, rc);
  free(csym); free(csym_n); free(gen); free(reach); free(seed);
  return keep;
}

// s2a (Vampire sine_to_age): per-SYMBOL SInE level over the clausified
// set, seeded from the conjecture (explicit ccnj or all-negative)
// clauses.  Level 0 = a conjecture symbol; k = k D-relation steps away;
// ATP_REL_LEVEL_REMOTE = goal-unreachable.  Same generality-triggered
// trigger as fof_sine_cone, but records the round each symbol is first
// reached instead of a keep set.  Returns a malloc'd label-indexed table
// (caller frees) with *out_n entries; NULL on OOM or when nothing is
// goal-shaped to seed from.  Feeds cnf_set_sine_levels for the age lane.
static u8 *fof_sine_sym_levels(FolClause **cls, const u8 *ccnj, int ncl,
                               u32 *out_n) {
  u32 n_labels = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next + 1u;
  u32 (*csym)[FSINE_CS] = malloc((size_t)ncl * sizeof *csym);
  int *csym_n = calloc((size_t)ncl, sizeof(int));
  u32 *gen    = calloc((size_t)n_labels, sizeof(u32));
  u8  *reach  = calloc((size_t)n_labels, 1u);
  u8  *lvl    = malloc((size_t)n_labels);
  if (csym == NULL || csym_n == NULL || gen == NULL || reach == NULL ||
      lvl == NULL) {
    free(csym); free(csym_n); free(gen); free(reach); free(lvl);
    return NULL;
  }
  for (u32 i = 0; i < n_labels; i++) lvl[i] = ATP_REL_LEVEL_REMOTE;
  for (int c = 0; c < ncl; c++) {
    for (u32 i = 0; i < cls[c]->n_lits; i++)
      fof_sine_clause_syms(cls[c]->lits[i].atom, csym[c], &csym_n[c]);
    for (int k = 0; k < csym_n[c]; k++) gen[csym[c][k]]++;
  }
  int n_seed = 0;
  for (int c = 0; c < ncl; c++) {
    u8 sd = ccnj[c];
    if (!sd && cls[c]->n_lits > 0u) {
      sd = 1u;
      for (u32 i = 0; i < cls[c]->n_lits && sd; i++)
        if (!cls[c]->lits[i].sign) sd = 0u;
    }
    if (!sd) continue;
    n_seed++;
    for (int k = 0; k < csym_n[c]; k++) {
      if (lvl[csym[c][k]] == ATP_REL_LEVEL_REMOTE) lvl[csym[c][k]] = 0u;
      reach[csym[c][k]] = 1u;
    }
  }
  if (n_seed == 0) {
    free(csym); free(csym_n); free(gen); free(reach); free(lvl);
    return NULL;
  }
  const u32 gt = 1u, rc = 64u;
  const double tol = 1.5;
  for (int rd = 0; rd < 64; rd++) {
    u8 grew = 0;
    for (int c = 0; c < ncl; c++) {
      u32 min_all = 0xffffffffu, min_ex = 0xffffffffu;
      for (int k = 0; k < csym_n[c]; k++) {
        u32 g = gen[csym[c][k]];
        if (g < min_all) min_all = g;
        if (g > gt && g < min_ex) min_ex = g;
      }
      u32 mingen = (min_ex <= rc) ? min_ex : min_all;
      u8 hit = 0;
      for (int k = 0; k < csym_n[c] && !hit; k++) {
        u32 sy = csym[c][k];
        if (!reach[sy]) continue;
        if (gen[sy] <= gt || (double)gen[sy] <= tol * (double)mingen) hit = 1;
      }
      if (!hit) continue;
      for (int k = 0; k < csym_n[c]; k++) {
        u32 sy = csym[c][k];
        if (lvl[sy] == ATP_REL_LEVEL_REMOTE) {
          lvl[sy] = (u8)(rd + 1);
          reach[sy] = 1u;
          grew = 1u;
        }
      }
    }
    if (!grew) break;
  }
  free(csym); free(csym_n); free(gen); free(reach);
  *out_n = n_labels;
  return lvl;
}

// --- symbol precedence census (Vampire sp= / spb=goal ports) --------------
// Occurrence counts over the pre-pass input clause set; goal_sym marks
// labels appearing in conjecture-descended clauses.  Feeds the KBO
// precedence fill below (THVM_FOL_SP / THVM_FOL_SPB).
static void fof_census_term(Term t, u32 *freq, u8 *goal_sym, u8 is_goal,
                            u32 n_labels) {
  if (term_tag(t) != TAG_CTR) return;
  u32 lab = term_ext(t);
  if (lab < n_labels) { freq[lab]++; if (is_goal) goal_sym[lab] = 1u; }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    fof_census_term(term_ctr_at(t, i), freq, goal_sym, is_goal, n_labels);
}

static int fof_u64_cmp(const void *a, const void *b) {
  u64 x = *(const u64 *)a, y = *(const u64 *)b;
  return (x < y) ? -1 : (x > y) ? 1 : 0;
}

// Floor on the pre-pass share of the remaining wall (fof_prepass_run).
static double g_fof_pp_min_frac = 0.0;

// --- clausal-saturation pre-pass ------------------------------------------
// Non-Horn refutations regularly need case analysis (paramodulation
// from equality literals inside multi-literal clauses; PUZ001+1's
// suspect elimination is the canonical shape), and NO equational
// encoding hands that to a completion engine -- the or-lane's unit
// propagation covers only the unit-resulting fragment.  The binary
// already carries the ProoVer checker's first-order saturation
// (resolution + factoring + paramodulation, src/fol cnf_run), so
// when the or-lane fired the ORIGINAL clause set first gets a
// bounded slice of it.  A derived empty clause is a genuine
// refutation of axioms + negated conjecture -- every inference of
// the calculus preserves models, so reporting Theorem is sound.  Any
// other outcome (saturation, cap, wall) falls through to the
// equational lane and the usual GaveUp discipline; a SATURATION here
// is never reported (the step/wall bounds make it inconclusive).
// THVM_ATP_FOF_SAT = step budget (default wall-governed, 0 disables).
// `ccnj` marks conjecture-descended records: those clauses (and their
// inference descendants, tracked inside the engine) get the pick-best
// goal bonus.
static int fof_clausal_prepass_once(FolClause **cls, const u8 *ccnj, int ncl,
                                    const u8 *sine_keep, u32 budget,
                                    AtpStatus *st_out) {
  CnfState *s = cnf_init(budget);
  if (s == NULL) return 0;
  // Literal selection (THVM_FOL_SELECT): default first-negative, the
  // historical hardwired policy.  0 = none (pure ordered-maximal),
  // 2 = first-positive, 3 = largest-negative -- the selection-
  // diversity lever the fof-sel* slices race.
  {
    const char *e = getenv("THVM_FOL_SELECT");
    cnf_set_select(s, (e == NULL || e[0] == '\0')
                          ? CNF_SELECT_NEGATIVE
                          : (CnfSelection)atoi(e));
  }
  // AVATAR-lite component splitting: OFF by default -- the 240s pod
  // A/B showed default-ON costs net solves (-8/+3 vs the unsplit
  // prepass; splitting reshapes search on problems the plain prepass
  // already wins).  It runs as its own portfolio slice (fof-avatar)
  // instead, the same discipline as every other lane: new mechanisms
  // join the schedule, they do not move a winning default.
  // THVM_FOL_AVATAR=1 splits every stored clause, =2 scopes the guard
  // spend to input + conjecture-descended clauses (see sat.c).
  {
    const char *e = getenv("THVM_FOL_AVATAR");
    cnf_set_avatar(s, (u8)((e == NULL || e[0] == '\0') ? 0u
                                                       : (u8)atoi(e)));
  }
  // Limited-resource strategy (THVM_FOL_LRS): discard passive clauses
  // the observed pick rate says cannot be reached before the wall
  // guard (Vampire's lrs saturation shape).  Off by default; the
  // fof-lrs slice turns it on.  The run loop below feeds the estimate
  // via cnf_lrs_tick.
  {
    const char *e = getenv("THVM_FOL_LRS");
    if (e != NULL && atoi(e) != 0) cnf_set_lrs(s, 1u);
  }
  // --proof: keep backward-subsumed clauses printable and remember
  // which CNF record each input clause id maps to, so a refutation
  // here emits the full conversion-lineage proof.  The engine id
  // space outgrows ncl under AVATAR splitting (each split original
  // and each fresh component takes an id): size the map for the worst
  // case -- one original per input plus the guard-supply component cap.
  int id2cap = 2 * ncl + (int)CNF_AVATAR_MAX_GUARDS + 1;
  int *id2cnf = NULL;
  if (g_cli_want_proof) {
    cnf_set_retain_dead(s);
    id2cnf = (int *)malloc(sizeof(int) * (size_t)id2cap);
    if (id2cnf != NULL)
      for (int i = 0; i < id2cap; i++) id2cnf[i] = -1;
  }
  // Attach a KBO (all weights 1, precedence = label) so demodulation
  // is ordering-gated; without one the engine demodulates naively and
  // equality-heavy sets rewrite-storm the term heap.
  u32 n_labels = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next + 1u;
  u32 *kbo_w = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  u32 *kbo_p = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  KboConfig kbo = { .weights = kbo_w, .precedence = kbo_p,
                    .n_labels = n_labels, .var_weight = 1u };
  if (kbo_w != NULL && kbo_p != NULL) {
    for (u32 i = 0; i < n_labels; i++) { kbo_w[i] = 1u; kbo_p[i] = i; }
    // Symbol precedence (THVM_FOL_SP): 1 = frequency (rarer symbols
    // rank GREATER -- Vampire's sp=frequency, its most-used
    // precedence, 2396 of 5307 CASC schedule strategies), 2 = reverse
    // frequency.  THVM_FOL_SPB != 0 additionally lifts every
    // conjecture-cone symbol above all the rest (spb=goal).  Unset
    // keeps the label-order default, byte-identical to before.
    long sp = 0, spb = 0;
    { const char *e = getenv("THVM_FOL_SP");  if (e != NULL && e[0] != '\0') sp  = atol(e); }
    { const char *e = getenv("THVM_FOL_SPB"); if (e != NULL && e[0] != '\0') spb = atol(e); }
    if ((sp == 1 || sp == 2 || spb != 0) && n_labels > 0u &&
        n_labels < (1u << 24)) {
      u32 *freq     = (u32 *)calloc(n_labels, sizeof(u32));
      u8  *goal_sym = (u8 *)calloc(n_labels, 1);
      u64 *keys     = (u64 *)malloc(sizeof(u64) * (size_t)n_labels);
      if (freq != NULL && goal_sym != NULL && keys != NULL) {
        for (int ci = 0; ci < ncl; ci++)
          for (u32 j = 0; j < cls[ci]->n_lits; j++)
            fof_census_term(cls[ci]->lits[j].atom, freq, goal_sym,
                            ccnj[ci], n_labels);
        for (u32 i = 0; i < n_labels; i++) {
          u64 f = (sp == 2) ? (u64)(0xFFFFFFFFu - freq[i])
                : (sp == 1) ? (u64)freq[i]
                            : (u64)i;        // spb-only: keep label order
          keys[i] = (f << 24) | (u64)i;      // n_labels < 2^24 guarded
        }
        qsort(keys, n_labels, sizeof(u64), fof_u64_cmp);
        for (u32 r = 0; r < n_labels; r++)
          kbo_p[(u32)(keys[r] & 0xFFFFFFu)] = r;
        if (spb != 0)
          for (u32 i = 0; i < n_labels; i++)
            if (goal_sym[i]) kbo_p[i] += n_labels;   // goal cone on top
      }
      free(freq); free(goal_sym); free(keys);
    }
    cnf_set_kbo(s, &kbo);
  }
  // Persist the KBO per-term weight/hash memo across the whole pre-pass
  // instead of the "safe default" of invalidating it on every single
  // thvm_kbo() call.  cnf_rewrite_once re-walks every literal against
  // every active unit rule to fixpoint (forward + backward demod), so
  // the same subterms recur across thousands of comparisons per step on
  // a wide clause set.  Safe here: the pre-pass term heap is a pure bump
  // arena for its own duration (see the "bump pointer rolls back
  // afterwards" comment below), terms are immutable once built, and
  // cnf_reclaim (sat.c) already bumps the epoch at every point a
  // discarded allocation's cell range could be reused (gated on
  // g_cnf_kbo_calls having moved since the mark -- see cnf_reclaim's
  // header comment), so a Term ID is never silently reinterpreted while
  // an epoch is still live.  Turned back off before returning so no
  // other caller (proof_check.c, tests) inherits persistence it doesn't
  // manage invalidation for.  Force a fresh epoch on entry: `&kbo` is a
  // stack address that a SInE-cone retry's second _once call could
  // coincidentally share with the first, and thvm_kbo()'s cfg-pointer-
  // change check alone must not be the only thing standing between this
  // call and a stale table.
  thvm_kbo_set_persist(1u);
  thvm_kbo_invalidate();
  // s2a (Vampire sine_to_age): compute the per-symbol SInE level table
  // over the input clauses and arm the cnf age lane so goal-distant
  // clauses are postponed.  THVM_ATP_S2A=<coeff> (0/unset = off).
  {
    const char *e = getenv("THVM_ATP_S2A");
    if (e != NULL && e[0] != '\0') {
      long coeff = atol(e);
      if (coeff > 0) {
        u32 nlab = 0;
        u8 *lvl = fof_sine_sym_levels(cls, ccnj, ncl, &nlab);
        if (lvl != NULL) {
          cnf_set_sine_levels(s, lvl, nlab, (u32)coeff);
          free(lvl);
        }
      }
    }
  }
  for (int i = 0; i < ncl; i++) {
    if (sine_keep != NULL && !sine_keep[i]) continue;   // outside the cone
    // A node wider than REWRITE_MAX_ARITY (a Skolem term over more
    // universals, a Tseitin name over as many variables) stays out, which
    // only shrinks the set: the walkers rebuild any arity, but one that
    // passed it through unsubstituted would derive what the clause does
    // not entail (pc_term_wide).  The equational lanes read it curried.
    if (pc_clause_wide(cls[i])) continue;
    FolClause *c = fol_clause_new(cls[i]->n_lits);
    if (c == NULL) {
      thvm_kbo_set_persist(0u);   // restore the safe per-call default
      cnf_free(s); free(id2cnf); return 0;
    }
    for (u32 j = 0; j < cls[i]->n_lits; j++) c->lits[j] = cls[i]->lits[j];
    i32 cid = cnf_add_clause_goal(s, c, ccnj[i]);   // takes ownership (a
                            // dropped clause only shrinks the set: a
                            // subset refutation is still a refutation)
    if (id2cnf != NULL && cid >= 0 && cid < id2cap) id2cnf[cid] = i;
  }
  // cnf_run with a wall guard and a heap guard: paramodulation storms
  // can flood the term arena (PUZ001+1 burned the whole 134M-cell
  // heap inside the step budget before ordered paramod landed).  The
  // bump pointer rolls back afterwards -- the pre-pass retains
  // nothing on the term heap (clause structs are malloc'd and freed).
  //
  // Wall: this pre-pass is the ONLY complete engine a non-Horn
  // problem gets (the or-lane equational encoding after it covers
  // just the unit-resulting fragment), and the call site gates on
  // non-Horn -- so it may spend up to 60% of the REMAINING --wall
  // rather than a token fixed slice.
  u64 heap_mark = HEAP_NEXT;
  u64 heap_cap  = gc_enabled() ? gc_from_end() : thvm_heap_cells();
  u64 heap_lim  = heap_cap - (heap_cap >> 2);   // keep 25% headroom
  // THVM_ATP_FOF_SAT_WALL: the pre-pass share of the REMAINING wall,
  // percent (default 60 -- the historical guard).  95+ hands the whole
  // slice wall to the clausal saturation: the pure-saturation lanes,
  // where the equational hand-off after the guard would only re-cover
  // the unit-resulting fragment anyway.
  double wall_frac = 0.6;
  {
    const char *e = getenv("THVM_ATP_FOF_SAT_WALL");
    if (e != NULL && e[0] != '\0') {
      long p = atol(e);
      if (p > 0 && p <= 100) wall_frac = (double)p / 100.0;
    }
  }
  if (wall_frac < g_fof_pp_min_frac) wall_frac = g_fof_pp_min_frac;
  double wall_guard = wall_frac * (g_cli_wall_sec - (horn_now() - g_cli_main_t0));
  if (wall_guard < 0.0) wall_guard = 0.0;
  double t0 = horn_now();
  // Arm the single-step self-abort so ONE long cnf_step (a giant backward-
  // subsumption sweep, a conclusion flood) trips ATP_ABORTED at the guard
  // instead of overrunning this between-steps poll (GEO340 burned 45s+ in
  // one step).  Disarmed after the loop so no later cnf caller inherits it.
  cnf_set_wall_deadline(wall_guard);
  const char *tripped = NULL;
  u32 lrs_next = 256u;
  while (s->status == ATP_RUNNING) {
    if (cnf_step(s) != ATP_RUNNING) break;
    if (horn_now() - t0 > wall_guard) { tripped = "wall"; break; }
    if (HEAP_NEXT > heap_lim) { tripped = "heap"; break; }
    // LRS estimate: picks left = observed pick rate x remaining guard.
    // Re-tick every 256 steps once a second of signal exists; the
    // watermark walk is O(passive), so the cadence keeps it noise.
    if (s->lrs_on && s->step >= lrs_next) {
      lrs_next = s->step + 256u;
      double el = horn_now() - t0;
      if (el > 1.0 && el < wall_guard) {
        double rem = ((double)s->step / el) * (wall_guard - el);
        cnf_lrs_tick(s, (rem >= 4294967295.0) ? ~0u : (u32)rem);
      }
    }
  }
  cnf_set_wall_deadline(0.0);   // disarm: no other cnf caller inherits it
  // A single-step self-abort breaks the loop with ATP_ABORTED and no
  // tripped tag; label it as the wall bound it is (guard already passed).
  if (tripped == NULL && s->status == ATP_ABORTED &&
      horn_now() - t0 > wall_guard) tripped = "wall";
  thvm_kbo_set_persist(0u);   // restore the safe per-call default
  AtpStatus st = s->status;
  u32 steps = s->step;
  u32 nstore = s->n;
  u32 nguard = s->n_guards;
  u32 nrefut = s->n_refut;
  u32 nmodel = s->n_models;
  u32 nparked = s->n_parked;
  u32 ndropped = s->lrs_dropped;
  // Refutation under --proof: stash the full conversion-lineage +
  // resolution proof BEFORE the state (and its term-heap slice) dies.
  if ((st == ATP_PROVED || st == ATP_REFUTED) && g_cli_want_proof)
    prepass_emit_proof(s, id2cnf, id2cap);
  free(id2cnf);
  // THVM_ATP_FOF_SAT_DEBUG=1: dump the live clause set (diagnosis of
  // spurious saturation -- a theorem must never reach QUEUE_EMPTY).
  {
    const char *dbg = getenv("THVM_ATP_FOF_SAT_DEBUG");
    if (dbg != NULL && dbg[0] != '0') {
      fprintf(stderr, "pre-pass clause store (n=%u):\n", s->n);
      for (u32 i = 0; i < s->n; i++) {
        if (s->clauses[i] == NULL) continue;
        fprintf(stderr, "  [w=%u%s] ", s->cweight[i],
                s->cpicked[i] ? " given" : "");
        cnf_print_clause_line(s, i, stderr);
      }
    }
  }
  cnf_free(s);
  free(kbo_w); free(kbo_p);
  HEAP_NEXT = heap_mark;
  if (st_out != NULL) *st_out = st;
  if (st == ATP_PROVED || st == ATP_REFUTED) {
    fprintf(stderr, "FindProof: FOF clausal pre-pass: refuted in %u "
            "step(s), %.1fs", steps, horn_now() - t0);
    if (nrefut > 0u)
      fprintf(stderr, " (avatar: %u guard(s), %u branch refutation(s), "
              "%u model move(s))", nguard, nrefut, nmodel);
    fprintf(stderr, "\n");
    return 1;
  }
  if (tripped == NULL)
    tripped = (st == ATP_ABORTED) ? "steps" :
              (st == ATP_QUEUE_EMPTY) ? "saturated" : "engine";
  fprintf(stderr, "FindProof: FOF clausal pre-pass: undecided "
          "(%u step(s), %u clause(s), %.1fs, %s-bounded, guard %.1fs",
          steps, nstore, horn_now() - t0, tripped, wall_guard);
  if (nguard > 0u)
    fprintf(stderr, "; avatar %u guard(s), %u branch refutation(s), "
            "%u model move(s), %u parked", nguard, nrefut, nmodel, nparked);
  if (ndropped > 0u)
    fprintf(stderr, "; lrs %u discarded", ndropped);
  fprintf(stderr, ")\n");
  return 0;
}

// --- one-element-model satisfiability gate --------------------------------
// A clause set satisfiable in the ONE-ELEMENT interpretation is
// satisfiable, full stop -- no lane may claim a refutation.  In that
// interpretation every equality atom is TRUE (both sides denote the
// single element): a positive equality literal satisfies its clause
// outright, a negative one is false and drops; what remains is boolean
// satisfiability over the PREDICATE symbols (each collapses to one
// truth value).  This is exactly the model the eq/true/false
// equational encoding cannot see -- the encoding is faithful only
// where true != false, so a collapse-satisfiable input (CAT015-3:
// `X = Y | there_exists(X) | there_exists(Y)` against
// `~there_exists(Z)`) lets the or-tables derive a spurious refutation
// (caught by the 2026-07-19 full-library soundness sweep).  Deciding
// here, BEFORE any lane runs, is sound for every lane at once, and
// costs a tiny DPLL: the sets are minute after equality filtering.
// Budgeted; budget exhaustion returns "not shown satisfiable" (lanes
// proceed) -- the guard must never block real proving on a timeout.
// The search is DPLL with unit propagation over literal-occurrence
// lists: the knowledge-base FOF sets (10k-100k clauses) are Horn-heavy,
// where propagation alone decides, and the old per-node rescan of every
// clause was quadratic there (it only ever ran below a 4096-predicate
// cap).  Any complete procedure returns the same verdict; the budget
// (decisions plus literal visits) only bounds when it may give up.
// $true / $false literals take their fixed value (the clausifier keeps
// them as atoms; read as free predicates, `p => $false` plus `p` came
// out "satisfiable").  A bare-variable or connective atom is a
// clausification the model cannot read: no verdict (pass B gives up on
// those shapes anyway).  On success the assignment is kept in
// g_oe_key / g_oe_val for fof_one_elt_claim.
static u64         *g_oe_key = NULL;   // predicate label of each gate variable
static signed char *g_oe_val = NULL;   // its value; -1: left open (no clause reached the search)
static int          g_oe_nvar = 0;

// DPLL over the gate's clauses: 1 satisfiable (the full assignment in
// model[0..nvar)), 0 unsatisfiable, -1 budget spent.
static int fof_one_elt_solve(int nvar, int ncl, const int *cl_off,
                             const int *cl_len, const int *lv, const int *lp,
                             signed char *model) {
  int nlit = (ncl > 0) ? cl_off[ncl - 1] + cl_len[ncl - 1] : 0;
  int *occ_start = (int *)calloc((size_t)nvar + 1u, sizeof(int));
  int *occ = (int *)malloc(((size_t)nlit + 1u) * sizeof(int));
  int *lit_cl = (int *)malloc(((size_t)nlit + 1u) * sizeof(int));
  int *nsat = (int *)calloc((size_t)ncl + 1u, sizeof(int));
  int *nfalse = (int *)calloc((size_t)ncl + 1u, sizeof(int));
  signed char *asg = (signed char *)malloc((size_t)nvar + 1u);
  int *trail = (int *)malloc(((size_t)nvar + 1u) * sizeof(int));
  int *dec_pos = (int *)malloc(((size_t)nvar + 1u) * sizeof(int));  // trail index of each decision
  signed char *dec_flipped = (signed char *)malloc((size_t)nvar + 1u);
  // Between two decisions a clause turns unit at most once: the queue
  // never holds more than one entry per clause plus the decision.
  int *queue = (int *)malloc(((size_t)ncl + (size_t)nvar + 2u) * sizeof(int));
  if (occ_start == NULL || occ == NULL || lit_cl == NULL || nsat == NULL ||
      nfalse == NULL || asg == NULL || trail == NULL || dec_pos == NULL ||
      dec_flipped == NULL || queue == NULL) tp_giveup("OOM in FOF front-end");
  for (int c = 0; c < ncl; c++)
    for (int k = 0; k < cl_len[c]; k++) {
      lit_cl[cl_off[c] + k] = c;
      occ_start[lv[cl_off[c] + k] + 1]++;
    }
  for (int v = 0; v < nvar; v++) occ_start[v + 1] += occ_start[v];
  {
    int *fill = (int *)malloc(((size_t)nvar + 1u) * sizeof(int));
    if (fill == NULL) tp_giveup("OOM in FOF front-end");
    memcpy(fill, occ_start, ((size_t)nvar + 1u) * sizeof(int));
    for (int li = 0; li < nlit; li++) occ[fill[lv[li]]++] = li;
    free(fill);
  }
  memset(asg, -1, (size_t)nvar + 1u);
  int ntrail = 0, ndec = 0, qh = 0, qt = 0, next_var = 0, verdict = -1;
  long work = 0, decisions = 0;
  const long work_cap = 400000000L, dec_cap = 1000000L;
  // Unit clauses seed the queue (a literal is queued as var*2+value).
  for (int c = 0; c < ncl; c++)
    if (cl_len[c] == 1) queue[qt++] = lv[cl_off[c]] * 2 + lp[cl_off[c]];
  for (;;) {
    // Propagate the queue; a conflict sets `conflict`.
    int conflict = 0;
    while (qh < qt && !conflict) {
      int v = queue[qh] >> 1, val = queue[qh] & 1;
      qh++;
      if (asg[v] >= 0) { if (asg[v] != val) conflict = 1; continue; }
      asg[v] = (signed char)val;
      trail[ntrail++] = v;
      // Every occurrence is counted even past a conflict: the undo on
      // backtrack reverses all of them.
      for (int o = occ_start[v]; o < occ_start[v + 1]; o++) {
        int li = occ[o], c = lit_cl[li];
        work++;
        if (lp[li] == val) { nsat[c]++; continue; }
        nfalse[c]++;
        if (nsat[c] > 0 || conflict) continue;
        if (nfalse[c] == cl_len[c]) { conflict = 1; continue; }
        if (nfalse[c] == cl_len[c] - 1) {
          for (int k = 0; k < cl_len[c]; k++) {
            int lk = cl_off[c] + k;
            work++;
            if (asg[lv[lk]] < 0) { queue[qt++] = lv[lk] * 2 + lp[lk]; break; }
          }
        }
      }
    }
    if (work > work_cap || decisions > dec_cap) break;   // unknown
    if (conflict) {
      // Backtrack to the latest decision whose other value is untried.
      int resumed = 0;
      while (ndec > 0 && !resumed) {
        int d = ndec - 1;
        int dv = trail[dec_pos[d]];
        int dval = asg[dv];
        while (ntrail > dec_pos[d]) {          // undo down to the decision
          int u = trail[--ntrail];
          int uval = asg[u];
          for (int o = occ_start[u]; o < occ_start[u + 1]; o++) {
            int li = occ[o], c = lit_cl[li];
            if (lp[li] == uval) nsat[c]--; else nfalse[c]--;
          }
          asg[u] = -1;
        }
        qh = qt = 0;
        if (!dec_flipped[d]) {
          dec_flipped[d] = 1;
          queue[qt++] = dv * 2 + (1 - dval);
          resumed = 1;
        } else {
          ndec--;
        }
      }
      if (!resumed) { verdict = 0; break; }       // refuted at level 0
      next_var = 0;
      continue;
    }
    while (next_var < nvar && asg[next_var] >= 0) { next_var++; work++; }
    if (next_var >= nvar) { verdict = 1; break; }  // every variable set
    decisions++;
    dec_pos[ndec] = ntrail;
    dec_flipped[ndec] = 0;
    ndec++;
    qh = qt = 0;
    queue[qt++] = next_var * 2 + 0;
  }
  if (verdict == 1) memcpy(model, asg, (size_t)nvar);
  free(occ_start); free(occ); free(lit_cl); free(nsat); free(nfalse);
  free(asg); free(trail); free(dec_pos); free(dec_flipped); free(queue);
  return verdict;
}

static int fof_one_elt_sat(FolClause **cls, int ncl) {
  // Tables sized by the input (a fixed 4096-predicate / 65536-literal
  // table returned "not shown satisfiable" past it -- the gate then
  // silently skipped the big problems); predicate keys hashed.
  size_t lit_cap = 1024, cls_cap = (size_t)ncl + 1;
  int *cl_off = (int *)malloc(cls_cap * sizeof(int));
  int *cl_len = (int *)malloc(cls_cap * sizeof(int));
  int *lv = (int *)malloc(lit_cap * sizeof(int));
  int *lp = (int *)malloc(lit_cap * sizeof(int));
  u32 hcap = 1024u;
  u64 *hkey = (u64 *)malloc(hcap * sizeof(u64));
  int *hvar = (int *)malloc(hcap * sizeof(int));
  if (cl_off == NULL || cl_len == NULL || lv == NULL || lp == NULL ||
      hkey == NULL || hvar == NULL) tp_giveup("OOM in FOF front-end");
  for (u32 h = 0; h < hcap; h++) hvar[h] = -1;
  int nvar = 0, nlit = 0, nout = 0, verdict = -1;
  g_oe_nvar = 0;
  for (int i = 0; i < ncl && verdict < 0; i++) {
    const FolClause *c = cls[i];
    if (c == NULL) continue;
    int off = nlit, len = 0, sat = 0;
    for (u32 j = 0; j < c->n_lits && !sat; j++) {
      Term a = c->lits[j].atom;
      int pos = (c->lits[j].sign == 0u);
      if (term_tag(a) != TAG_CTR) { verdict = 0; break; }   // no verdict
      if (fol_atom_is_eq(a)) {
        if (pos) sat = 1;                      // positive eq: true
        continue;                              // negative eq: drops
      }
      u32 lab = term_ext(a);
      if (lab == PC_LAB_TRUE || lab == PC_LAB_FALSE) {
        if (pos == (lab == PC_LAB_TRUE)) sat = 1;
        continue;
      }
      if (fol_is_connective(lab)) { verdict = 0; break; }  // no verdict
      u64 key = (u64)lab;
      if (2u * (u32)(nvar + 1) > hcap) {
        u32 ncap = 2u * hcap;
        u64 *nk = (u64 *)malloc(ncap * sizeof(u64));
        int *nv = (int *)malloc(ncap * sizeof(int));
        if (nk == NULL || nv == NULL) tp_giveup("OOM in FOF front-end");
        for (u32 h = 0; h < ncap; h++) nv[h] = -1;
        for (u32 h = 0; h < hcap; h++) {
          if (hvar[h] < 0) continue;
          u32 sl = (u32)((hkey[h] * 0x9E3779B97F4A7C15ull) >> 32) & (ncap - 1u);
          while (nv[sl] >= 0) sl = (sl + 1u) & (ncap - 1u);
          nk[sl] = hkey[h];
          nv[sl] = hvar[h];
        }
        free(hkey); free(hvar);
        hkey = nk; hvar = nv; hcap = ncap;
      }
      u32 sl = (u32)((key * 0x9E3779B97F4A7C15ull) >> 32) & (hcap - 1u);
      while (hvar[sl] >= 0 && hkey[sl] != key) sl = (sl + 1u) & (hcap - 1u);
      if (hvar[sl] < 0) { hkey[sl] = key; hvar[sl] = nvar++; }
      if ((size_t)nlit >= lit_cap) {
        lit_cap *= 2;
        lv = (int *)realloc(lv, lit_cap * sizeof(int));
        lp = (int *)realloc(lp, lit_cap * sizeof(int));
        if (lv == NULL || lp == NULL) tp_giveup("OOM in FOF front-end");
      }
      lv[nlit] = hvar[sl]; lp[nlit] = pos;
      nlit++; len++;
    }
    if (verdict >= 0) break;
    if (sat) { nlit = off; continue; }
    if (len == 0) verdict = 0;   // clause false in the one-element model
    else { cl_off[nout] = off; cl_len[nout] = len; nout++; }
  }
  signed char *model = NULL;
  if (verdict < 0) {
    model = (signed char *)malloc((size_t)nvar + 1u);
    if (model == NULL) tp_giveup("OOM in FOF front-end");
    memset(model, -1, (size_t)nvar + 1u);
    if (nout == 0) {
      verdict = 1;
    } else {
      verdict = fof_one_elt_solve(nvar, nout, cl_off, cl_len, lv, lp, model) == 1;
    }
  }
  if (verdict == 1) {
    // The assignment for fof_one_elt_claim: variable hvar[h] is the
    // predicate label hkey[h].
    u64 *keys = (u64 *)malloc(((size_t)nvar + 1u) * sizeof(u64));
    if (keys == NULL) tp_giveup("OOM in FOF front-end");
    for (u32 h = 0; h < hcap; h++)
      if (hvar[h] >= 0) keys[hvar[h]] = hkey[h];
    free(g_oe_key); free(g_oe_val);
    g_oe_key = keys;
    g_oe_val = model;
    g_oe_nvar = nvar;
    model = NULL;
  }
  free(model);
  free(cl_off); free(cl_len); free(lv); free(lp); free(hkey); free(hvar);
  return verdict;
}

// --- one-element model: the Satisfiable / CounterSatisfiable answer ------
// SOUNDNESS.  In a one-element domain {e} every term denotes e, so an
// atom p(t..) reads one truth bit per predicate symbol, s = t is true,
// and a quantifier is its body.  The gate's clause set comes from NNF
// (equivalence), Skolemization and Tseitin definitions aux <-> G (a
// model of the result, restricted to the input signature, is a model of
// the input -- in {e} every Skolem function maps to e), and naive
// distribution (equivalence).  So its assignment, restricted to the
// problem's predicates, is a model of the problem.  The claim does not
// rest on that argument alone: an allocation failure in the clausifier
// drops a clause while the record's others survive (so did a clause wider
// than FOL_MAX_LITS before the extractor chunked it: the gate then read
// the Theorems LCL662+1.010 and LCL668+1.015 as satisfiable), so every
// ORIGINAL record formula -- as parsed, universally closed, the
// conjecture negated -- is re-evaluated under the assignment (predicates
// the search left open read false) and one false formula withdraws the
// claim.  Input shapes that read differently from the TPTP semantics
// withdraw it too: numbers and distinct objects (interpreted as distinct
// values; the unquoting pass renames the non-identifier ones to fresh
// constants, and the rename table still names them), $-symbols, a name
// used at two arities or as both predicate and function, a stand-in
// spelled as a quoted lower word (the unquoting pass strips those
// quotes: 'w' IS w), records the scanner does not read (typed records,
// unexpanded nested include(), a comment preprocessing left), and roles
// outside the axiom/conjecture family.  Output: Vampire's FiniteModel
// form (tff declarations, finite_domain, one definition per symbol),
// every input symbol in the problem's own spelling.
static Term *g_oe_form = NULL;     // record formulas (closed, conjecture negated)
static int   g_oe_nform = 0;
static int   g_oe_cap = 0;
static int   g_oe_role_bad = 0;    // a role outside the axiom/conjecture family

static int fof_oe_role_ok(const char *role) {
  static const char *ok[] = {
    "axiom", "hypothesis", "definition", "assumption", "lemma", "theorem",
    "corollary", "conjecture", "negated_conjecture", "plain", NULL };
  for (int i = 0; ok[i] != NULL; i++) {
    if (strcmp(role, ok[i]) == 0) return 1;
  }
  return 0;
}

// Truth of an original formula: 1 / 0, -1 when it cannot be read.
static int fof_oe_eval(Term f, const signed char *pval) {
  if (term_tag(f) != TAG_CTR) return -1;
  u32 lab = term_ext(f);
  switch (lab) {
    case FOL_LAB_NOT: {
      int a = fof_oe_eval(term_ctr_at(f, 0u), pval);
      return (a < 0) ? -1 : !a;
    }
    case FOL_LAB_AND: case FOL_LAB_OR: case FOL_LAB_IMP: case FOL_LAB_IFF: {
      int a = fof_oe_eval(term_ctr_at(f, 0u), pval);
      int b = fof_oe_eval(term_ctr_at(f, 1u), pval);
      if (a < 0 || b < 0) return -1;
      switch (lab) {
        case FOL_LAB_AND: return a && b;
        case FOL_LAB_OR:  return a || b;
        case FOL_LAB_IMP: return !a || b;
        default:          return a == b;
      }
    }
    case FOL_LAB_ALL: case FOL_LAB_EX:
      return fof_oe_eval(term_ctr_at(f, 1u), pval);
    case PC_LAB_TRUE:  return (term_ctr_n(f) == 0u) ? 1 : -1;
    case PC_LAB_FALSE: return (term_ctr_n(f) == 0u) ? 0 : -1;
    case PC_LAB_EQ:    return (term_ctr_n(f) == 2u) ? 1 : -1;
    default: {
      int ix = pc_ix_of(lab);
      return (ix < 0) ? -1 : pval[ix];
    }
  }
}

// Signature of the original formulas: kind 1 = predicate, 2 = function.
// 0 on a shape the model cannot print (clash, non-symbol subterm).
static int fof_oe_note(u32 lab, u8 kind, u32 arity, u8 *skind, u32 *sarity) {
  int i = pc_ix_of(lab);
  if (i < 0) return 0;
  if (skind[i] == 0u) { skind[i] = kind; sarity[i] = arity; return 1; }
  return skind[i] == kind && sarity[i] == arity;
}

static int fof_oe_sig_term(Term t, u8 *skind, u32 *sarity) {
  if (term_tag(t) == TAG_FVR) return 1;
  if (term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  if (!fof_oe_note(term_ext(t), 2u, n, skind, sarity)) return 0;
  for (u32 i = 0; i < n; i++) {
    if (!fof_oe_sig_term(term_ctr_at(t, i), skind, sarity)) return 0;
  }
  return 1;
}

static int fof_oe_sig_form(Term f, u8 *skind, u32 *sarity) {
  if (term_tag(f) != TAG_CTR) return 0;
  u32 lab = term_ext(f);
  u32 n = term_ctr_n(f);
  switch (lab) {
    case FOL_LAB_NOT:
      return fof_oe_sig_form(term_ctr_at(f, 0u), skind, sarity);
    case FOL_LAB_AND: case FOL_LAB_OR: case FOL_LAB_IMP: case FOL_LAB_IFF:
      return fof_oe_sig_form(term_ctr_at(f, 0u), skind, sarity) &&
             fof_oe_sig_form(term_ctr_at(f, 1u), skind, sarity);
    case FOL_LAB_ALL: case FOL_LAB_EX:
      return fof_oe_sig_form(term_ctr_at(f, 1u), skind, sarity);
    case PC_LAB_TRUE: case PC_LAB_FALSE:
      return n == 0u;
    case PC_LAB_EQ:
      return n == 2u &&
             fof_oe_sig_term(term_ctr_at(f, 0u), skind, sarity) &&
             fof_oe_sig_term(term_ctr_at(f, 1u), skind, sarity);
    default:
      if (!fof_oe_note(lab, 1u, n, skind, sarity)) return 0;
      for (u32 i = 0; i < n; i++) {
        if (!fof_oe_sig_term(term_ctr_at(f, i), skind, sarity)) return 0;
      }
      return 1;
  }
}

static int fof_oe_lower_word(const char *s, size_t n) {
  if (n == 0 || !islower((unsigned char)s[0])) return 0;
  for (size_t i = 1; i < n; i++) {
    if (!isalnum((unsigned char)s[i]) && s[i] != '_') return 0;
  }
  return 1;
}

// The problem's own spelling of an interned name (undoes the quoted-atom
// and long-name stand-ins); NULL when the name is not a TPTP functor --
// a number or $-symbol, or the stand-in of a distinct object or number:
// the unquoting pass renames "text", -1, 2.5 and 1/3 to fresh constants
// (dob<n> / num<n>), whose original spelling the rename table keeps.
static const char *fof_oe_spelling(const char *nm) {
  if (!fof_oe_lower_word(nm, strlen(nm))) return NULL;
  int slot = tp_ren_slot(nm, strlen(nm));
  if (slot < 0 || g_tp_ren[slot].from == NULL) return nm;
  const char *to = g_tp_ren[slot].to;
  if (to[0] == '\'' || fof_oe_lower_word(to, strlen(to))) return to;
  return NULL;
}

// A quoted atom 'w' with w a lower word IS the functor w (TPTP BNF).  The
// unquoting pass strips those quotes, so a stand-in spelled 'w' next to a
// plain w would be one symbol read as two.
static int fof_oe_quote_alias(const char *sp) {
  if (sp[0] != '\'') return 0;
  size_t n = strlen(sp);
  if (n < 2 || sp[n - 1] != '\'') return 1;
  const char *w = sp + 1;
  size_t wl = n - 2;
  if (!fof_oe_lower_word(w, wl)) return 0;
  if (wl >= TP_NAME_LEN) return 1;
  char buf[TP_NAME_LEN];
  memcpy(buf, w, wl);
  buf[wl] = '\0';
  return pc_lookup(buf) != PC_NOT_FOUND;
}

// Text the record scanner reads as the problem: a comment, quote or
// record kind it does not handle means the parsed formulas may not be
// the problem's.  A distinct object anywhere in the input refuses: the
// unquoting pass renames each to a fresh constant, and the rename table
// (main.c keeps it on every TPTP run) names it.
static const char *fof_oe_text_refusal(const char *text) {
  if (!g_tp_ren_on) return "input renames not recorded";
  if (strchr(text, '\'') != NULL) return "quoted text left after unquoting";
  if (strchr(text, '"') != NULL) return "distinct object";
  for (u32 i = 0; i < g_tp_ren_cap; i++) {
    if (g_tp_ren[i].from != NULL && g_tp_ren[i].to[0] == '"') return "distinct object";
  }
  if (strchr(text, '%') != NULL) return "trailing comment";
  if (strstr(text, "/*") != NULL) return "block comment";
  static const char *kinds[] = { "include(", "tff(", "thf(", "tcf(", "tpi(", NULL };
  for (int k = 0; kinds[k] != NULL; k++) {
    size_t kl = strlen(kinds[k]);
    for (const char *q = text; (q = strstr(q, kinds[k])) != NULL; q += kl) {
      if (q == text || !(isalnum((unsigned char)q[-1]) || q[-1] == '_'))
        return "unread record kind";
    }
  }
  return NULL;
}

// Vampire-style declaration / definition names: prefix + name, quoted
// when the name is.
static void fof_oe_prefixed(FILE *out, const char *prefix, const char *sp) {
  if (sp[0] == '\'') fprintf(out, "'%s%s", prefix, sp + 1);
  else fprintf(out, "%s%s", prefix, sp);
}
static void fof_oe_suffixed(FILE *out, const char *sp, const char *suffix) {
  size_t n = strlen(sp);
  if (sp[0] == '\'') fprintf(out, "%.*s%s'", (int)(n - 1), sp, suffix);
  else fprintf(out, "%s%s", sp, suffix);
}

#define FOF_OE_ELT "'fmb_$i_1'"

static void fof_oe_args(FILE *out, u32 n) {
  fputc('(', out);
  for (u32 k = 0; k < n; k++) fprintf(out, "%s" FOF_OE_ELT, k ? "," : "");
  fputc(')', out);
}

static void fof_oe_print(FILE *out, const u8 *skind, const u32 *sarity,
                         const signed char *pval) {
  fprintf(out, "tff('declare_$i1',type," FOF_OE_ELT ":$i).\n");
  fprintf(out, "tff('finite_domain_$i',axiom,\n"
               "      ! [X:$i] : (\n"
               "         X = " FOF_OE_ELT "\n"
               "      ) ).\n\n");
  // constants, functions, propositions, predicates (Vampire's order)
  for (int pass = 0; pass < 4; pass++) {
    u8 kind = (pass < 2) ? 2u : 1u;
    for (int i = 0; i < pc_nsyms; i++) {
      if (skind[i] != kind || ((sarity[i] == 0u) != (pass % 2 == 0))) continue;
      const char *sp = fof_oe_spelling(pc_sym[i]);
      u32 n = sarity[i];
      fputs("tff(", out);
      fof_oe_prefixed(out, "declare_", sp);
      fprintf(out, ",type,%s: ", sp);
      if (n > 0u) {
        fputc('(', out);
        for (u32 k = 0; k < n; k++) fprintf(out, "%s$i", k ? " * " : "");
        fprintf(out, ") > ");
      }
      fprintf(out, "%s).\n", (kind == 2u) ? "$i" : "$o");
      if (n == 0u) {
        fputs("tff(", out);
        fof_oe_suffixed(out, sp, "_definition");
        if (kind == 2u) fprintf(out, ",axiom,%s = " FOF_OE_ELT ").\n", sp);
        else fprintf(out, ",axiom,%s%s).\n", pval[i] ? "" : "~", sp);
        continue;
      }
      fputs("tff(", out);
      fof_oe_prefixed(out, (kind == 2u) ? "function_" : "predicate_", sp);
      fprintf(out, ",axiom,\n           ");
      if (kind == 2u) {
        fputs(sp, out);
        fof_oe_args(out, n);
        fprintf(out, " = " FOF_OE_ELT "\n");
      } else {
        fprintf(out, "%s%s", pval[i] ? "" : "~", sp);
        fof_oe_args(out, n);
        fputc('\n', out);
      }
      fprintf(out, "\n).\n\n");
    }
  }
}

// Claim the gate's one-element model: print the SZS status and the
// FiniteModel block and exit 0 (the schedule's winner signal).  Returns
// (lanes proceed to the old give-up) when any check refuses the claim.
static void fof_one_elt_claim(const char *text) {
  const char *why = fof_oe_text_refusal(text);
  if (why == NULL && g_oe_role_bad) why = "role outside the axiom/conjecture family";
  u8 *skind = (u8 *)calloc((size_t)pc_nsyms + 1u, 1u);
  u32 *sarity = (u32 *)calloc((size_t)pc_nsyms + 1u, sizeof(u32));
  signed char *pval = (signed char *)calloc((size_t)pc_nsyms + 1u, 1u);
  if (skind == NULL || sarity == NULL || pval == NULL) why = "OOM";
  for (int r = 0; why == NULL && r < g_oe_nform; r++) {
    if (!fof_oe_sig_form(g_oe_form[r], skind, sarity))
      why = "symbol shape (arity clash, predicate used as function, ...)";
  }
  for (int i = 0; why == NULL && i < pc_nsyms; i++) {
    if (skind[i] == 0u) continue;
    const char *sp = fof_oe_spelling(pc_sym[i]);
    if (sp == NULL) why = "number, distinct object or $-symbol";
    else if (fof_oe_quote_alias(sp)) why = "quoted atom aliasing a plain name";
  }
  if (why == NULL) {
    for (int k = 0; k < g_oe_nvar; k++) {
      u64 key = g_oe_key[k];
      int ix = (key <= 0xFFFFFFFFull) ? pc_ix_of((u32)key) : -1;
      if (ix >= 0 && g_oe_val[k] == 1) pval[ix] = 1;
    }
    for (int r = 0; why == NULL && r < g_oe_nform; r++) {
      if (fof_oe_eval(g_oe_form[r], pval) != 1)
        why = "a record is false under the clause-level model";
    }
  }
  if (why != NULL) {
    fprintf(stderr, "FindProof: one-element model not claimed: %s\n", why);
    free(skind); free(sarity); free(pval);
    return;
  }
  int npred = 0, nfun = 0;
  for (int i = 0; i < pc_nsyms; i++) {
    if (skind[i] == 1u) npred++;
    else if (skind[i] == 2u) nfun++;
  }
  printf("%% SZS status %s for %s\n",
         g_cli_fof_conjecture ? "CounterSatisfiable" : "Satisfiable",
         g_cli_probpath);
  printf("%% SZS output start FiniteModel for %s\n", g_cli_probpath);
  fof_oe_print(stdout, skind, sarity, pval);
  printf("%% SZS output end FiniteModel for %s\n", g_cli_probpath);
  fflush(stdout);
  fprintf(stderr, "FindProof: one-element model: %d predicate(s), "
          "%d function(s), %d record(s) re-evaluated true\n",
          npred, nfun, g_oe_nform);
  exit(0);
}

static int fof_clausal_prepass(FolClause **cls, const u8 *ccnj, int ncl) {
  // Step budget: wall-governed by default (cnf_init(0) = 1<<30 steps;
  // the 60%-of-remaining-wall guard + the heap guard in _once are the
  // real bounds -- a fixed step cap predates the indexing speedups and
  // bound SYN938+1 at 0.5s of a 36s guard).  THVM_ATP_FOF_SAT: N > 0
  // caps the steps exactly, 0 (or negative) disables the pre-pass.
  u32 budget = 0u;
  {
    const char *e = getenv("THVM_ATP_FOF_SAT");
    if (e != NULL) {
      long v = atol(e);
      if (v <= 0) return 0;
      budget = (u32)v;
    }
  }
  // SInE cone over the clausified set (THVM_ATP_FOF_SINE): default ON
  // above 512 clauses, =0 off, N>1 sets the threshold.  Only the
  // pre-pass input shrinks; the equational lanes downstream still see
  // the full clause set.
  u8 *sine_keep = NULL;
  {
    const char *e = getenv("THVM_ATP_FOF_SINE");
    long thr = 512;
    if (e != NULL && e[0] != '\0') {
      long v = atol(e);
      if (v == 0) thr = -1;
      else if (v > 1) thr = v;
    }
    if (thr >= 0 && ncl > thr)
      sine_keep = fof_sine_cone(cls, ccnj, ncl, (int)thr);
  }
  AtpStatus st = ATP_RUNNING;
  int r = fof_clausal_prepass_once(cls, ccnj, ncl, sine_keep, budget, &st);
  // A SATURATED cone is a PROVEN miss for that subset (no refutation
  // exists from it) with wall to spare -- the one outcome where more
  // axioms are known to be the fix.  Retry once on the full set under
  // whatever 60%-of-remaining guard is left; every other undecided
  // outcome (steps/wall/heap) falls through as before.
  if (r == 0 && sine_keep != NULL && st == ATP_QUEUE_EMPTY) {
    fprintf(stderr, "FindProof: FOF pre-pass SInE cone saturated; "
            "retrying on the full clause set\n");
    r = fof_clausal_prepass_once(cls, ccnj, ncl, NULL, budget, &st);
  }
  free(sine_keep);
  return r;
}

// --- deferred pre-pass ---------------------------------------------------
static FolClause **g_fof_pp_cls = NULL;
static u8         *g_fof_pp_ccnj = NULL;
static int         g_fof_pp_ncl = 0;
static int         g_fof_pp_pending = 0;
static jmp_buf     g_fof_pp_oom_jb;

static void cli_proof_write(FILE *out, const char *text, size_t len);   // szs.c

static int fof_prepass_enabled(void) {
  const char *e = getenv("THVM_ATP_FOF_SAT");
  return !(e != NULL && atol(e) <= 0);
}

// Run the deferred pre-pass (once) with at least `min_frac` of the
// remaining wall; releases the clause set.  1 = refuted.
static int fof_prepass_run(double min_frac) {
  if (!g_fof_pp_pending) return 0;
  g_fof_pp_pending = 0;
  g_tp_giveup_hook = NULL;
  thvm_heap_exhausted = 0;
  thvm_heap_exhaust_jmp = &g_fof_pp_oom_jb;
  if (setjmp(g_fof_pp_oom_jb) != 0) {
    thvm_heap_exhaust_jmp = NULL;
    tp_giveup("FOF clausification exhausted the term heap "
              "(raise --heap-cells)");
  }
  g_fof_pp_min_frac = min_frac;
  int r = fof_clausal_prepass(g_fof_pp_cls, g_fof_pp_ccnj, g_fof_pp_ncl);
  g_fof_pp_min_frac = 0.0;
  thvm_heap_exhaust_jmp = NULL;
  for (int c = 0; c < g_fof_pp_ncl; c++) fol_clause_free(g_fof_pp_cls[c]);
  free(g_fof_pp_cls); free(g_fof_pp_ccnj);
  g_fof_pp_cls = NULL; g_fof_pp_ccnj = NULL; g_fof_pp_ncl = 0;
  if (r) g_cli_fof_satref = 1;
  return r;
}

// The pre-pass refutation IS the result: status line (+ the recorded
// proof under --proof), as main prints it for a front-end win.
static void fof_prepass_report(void) {
  if (g_cli_szs_wanted)
    printf("%% SZS status %s for %s\n",
           g_cli_fof_conjecture ? "Theorem" : "Unsatisfiable", g_cli_probpath);
  if (g_cli_want_proof && g_cli_prepass_proof != NULL)
    cli_proof_write(stdout, g_cli_prepass_proof, strlen(g_cli_prepass_proof));
  fflush(stdout);
}

static void fof_prepass_fallback(void) {
  if (fof_prepass_run(0.95)) {
    fof_prepass_report();
    exit(0);
  }
}

// --- the front-end ------------------------------------------------------
// Next fof( / cnf( record keyword at an identifier boundary, scanning
// forward from p (quote-aware); NULL at the end.  One pass over the
// text in total -- the old per-record strstr for BOTH keywords rescanned
// the rest of a fof-only file for "cnf(" every record (quadratic: 25k
// records x 27MB on the BIO knowledge base).
static const char *fof_next_record(const char *text, const char *p, int *is_cnf) {
  for (; *p != '\0'; p++) {
    if (*p == '\'' || *p == '"') {
      const char *e = tp_quote_end(p);
      if (e == NULL) return NULL;
      p = e;
      continue;
    }
    if ((p[0] == 'f' && p[1] == 'o' && p[2] == 'f' && p[3] == '(') ||
        (p[0] == 'c' && p[1] == 'n' && p[2] == 'f' && p[3] == '(')) {
      if (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
        *is_cnf = (p[0] == 'c');
        return p;
      }
    }
  }
  return NULL;
}

// Clausified-clause arrays, grown together.
static void fof_cls_reserve(FolClause ***cls, u8 **ccnj, int **cls_rec,
                            int *cap, int need) {
  if (need < *cap) return;
  int ncap = (*cap == 0) ? 4096 : *cap;
  while (ncap <= need) ncap *= 2;
  FolClause **nc = (FolClause **)realloc(*cls, (size_t)ncap * sizeof(FolClause *));
  u8 *nj = (u8 *)realloc(*ccnj, (size_t)ncap);
  if (nc == NULL || nj == NULL) tp_giveup("OOM in FOF front-end");
  *cls = nc;
  *ccnj = nj;
  if (cls_rec != NULL && *cls_rec != NULL) {
    int *nr = (int *)realloc(*cls_rec, (size_t)ncap * sizeof(int));
    if (nr == NULL) tp_giveup("OOM in FOF front-end");
    *cls_rec = nr;
  }
  *cap = ncap;
}

static jmp_buf g_fof_oom_jb;

// Upper bound on the distinct symbols of the text: every distinct word
// that is not a variable (record names and role words included).  Only
// consulted when the text could hold more symbols than the default
// label range.
static int fof_symbol_bound(const char *text) {
  size_t len = strlen(text);
  if (len < 2u * (size_t)PC_MAX_SYMS) return 0;
  u32 cap = 1u << 18;
  const char **key = NULL;
  u32 *klen = NULL;
  int n = 0;
  for (;;) {
    key = (const char **)calloc(cap, sizeof(char *));
    klen = (u32 *)calloc(cap, sizeof(u32));
    if (key == NULL || klen == NULL) tp_giveup("OOM in FOF front-end");
    n = 0;
    int full = 0;
    for (const char *p = text; *p != '\0' && !full; ) {
      if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) { p++; continue; }
      const char *w = p;
      while (isalnum((unsigned char)*p) || *p == '_' || *p == '$') p++;
      if (isupper((unsigned char)*w) || *w == '_') continue;   // variable
      u32 l = (u32)(p - w);
      u32 sl = (u32)(tp_name_hash(w, l) & (cap - 1u));
      while (key[sl] != NULL && !(klen[sl] == l && memcmp(key[sl], w, l) == 0))
        sl = (sl + 1u) & (cap - 1u);
      if (key[sl] == NULL) {
        key[sl] = w; klen[sl] = l; n++;
        if (2u * (u32)n > cap) full = 1;
      }
    }
    free(key); free(klen);
    if (!full) break;
    cap *= 2u;
  }
  return n;
}

static void tp_fof_load(TpProblem *tp, const char *text) {
  thvm_init();
  g_cli_fof_applied = 1;
  pc_sym_reset();
  fol_reset_skolem();
  fol_reset_tseitin();   // aux labels run per-problem, across records
  prov_def_reset();      // definition symbols are declared once per problem
  g_fof_or_used = g_fof_eq_used = g_fof_not_used = 0;
  g_fof_or_ncl = g_fof_tseitin_recs = 0;
  g_fof_split_nor = g_fof_split_unfit = g_fof_split_pending = 0;
  fof_split_read_branch();
  // A signature past the default label range (the BIO knowledge base:
  // ~81k symbols) takes labels above the connectives; the Skolem and
  // Tseitin families move up past them first.
  {
    int nsym = fof_symbol_bound(text);
    if (nsym > PC_MAX_SYMS) {
      u32 floor = (nsym > PC_LOW_SYMS)
                    ? PC_LAB_EXT + (u32)(nsym - PC_LOW_SYMS) : 0x10000u;
      if (!pc_sym_extend(nsym) || !fol_set_label_floor(floor))
        tp_giveup("problem exceeds symbol bound");
    }
  }
  thvm_heap_exhausted = 0;
  thvm_heap_exhaust_jmp = &g_fof_oom_jb;
  if (setjmp(g_fof_oom_jb) != 0) {
    thvm_heap_exhaust_jmp = NULL;
    tp_giveup("FOF clausification exhausted the term heap "
              "(raise --heap-cells)");
  }

  // Pass A: parse + clausify every record.  Terms persist (no engine
  // session runs before serialization, so nothing moves the heap).
  FolClause **cls = NULL;
  u8 *ccnj = NULL;
  int cls_cap = 0;
  g_oe_nform = 0;
  g_oe_role_bad = 0;
  // --proof: conversion-lineage capture (record chain + per-clause
  // source-record map for the emitted cnf(c<k>, ...) layer).
  g_prov_on = g_cli_want_proof;
  int *cls_rec = NULL;
  if (g_prov_on) {
    cls_rec = (int *)malloc(sizeof(int));
    if (cls_rec == NULL) g_prov_on = 0;
  }
  int ncl = 0;
  int n_conjectures = 0;
  const char *p = text;
  for (;;) {
    int rec_is_cnf = 0;
    const char *rec = fof_next_record(text, p, &rec_is_cnf);
    if (rec == NULL) break;
    p = rec + 4;
    int depth = 1;
    const char *body = p;
    while (*p != '\0' && depth > 0) {
      if (*p == '\'' || *p == '"') {
        const char *e = tp_quote_end(p);
        p = (e != NULL) ? e + 1 : p + strlen(p);
        continue;
      }
      if (*p == '(') depth++;
      else if (*p == ')') depth--;
      p++;
    }
    if (depth != 0) tp_giveup("unbalanced parentheses in TPTP record");
    size_t blen = (size_t)(p - body - 1);
    // split off role (arg 2) and formula (arg 3) at top-level commas;
    // trailing annotation args are legal and ignored.
    int d = 0, commas = 0;
    const char *role = NULL, *form = NULL;
    size_t role_len = 0, form_len = 0;
    for (size_t i = 0; i < blen; i++) {
      char c = body[i];
      if (c == '(' || c == '[') d++;
      else if (c == ')' || c == ']') d--;
      else if (c == ',' && d == 0) {
        commas++;
        if (commas == 1) role = body + i + 1;
        else if (commas == 2) {
          role_len = (size_t)(body + i - role);
          form = body + i + 1;
          form_len = blen - i - 1;
        } else if (commas == 3) {
          form_len = (size_t)(body + i - form);
          break;
        }
      }
    }
    if (form == NULL) tp_giveup("malformed TPTP record (need name, role, formula)");
    if (commas == 2) form_len = blen - (size_t)(form - body);
    while (role_len > 0 && isspace((unsigned char)*role)) { role++; role_len--; }
    while (role_len > 0 && isspace((unsigned char)role[role_len - 1])) role_len--;
    char rolebuf[32];
    snprintf(rolebuf, sizeof rolebuf, "%.*s",
             (int)(role_len < 31 ? role_len : 31), role);
    char *fbuf = (char *)malloc(form_len + 1);
    if (fbuf == NULL) tp_giveup("OOM in FOF front-end");
    memcpy(fbuf, form, form_len);
    fbuf[form_len] = '\0';
    int rec_ix = -1;
    if (g_prov_on) {
      size_t nm_len = (role != NULL) ? (size_t)(role - 1 - body) : 0u;
      rec_ix = prov_rec_new(body, nm_len, rolebuf, fbuf, rec_is_cnf);
    }
    PcParse scope;
    Term f = pc_parse_formula(fbuf, &scope);
    free(fbuf);
    if (f == 0) {
      char why[256];
      size_t nm_len = (role != NULL) ? (size_t)(role - 1 - body) : 0u;
      snprintf(why, sizeof why, "cannot parse a TPTP formula (record %.*s: %s)",
               (int)(nm_len < 96 ? nm_len : 96), body,
               pc_err != NULL ? pc_err : "syntax");
      tp_giveup(why);
    }
    if (pc_nsyms >= pc_sym_limit - 1) tp_giveup("problem exceeds symbol bound");
    int is_conj = (strcmp(rolebuf, "conjecture") == 0);
    int conj_mark = is_conj || (strcmp(rolebuf, "negated_conjecture") == 0);
    if (is_conj) {
      if (++n_conjectures > 1) tp_giveup("multiple conjecture records");
      g_cli_fof_conjecture = 1;
    }
    f = pc_close(f);                     // TPTP: free variables are universal
    if (is_conj) f = fol_mk_not(f);      // refutation: negate the conjecture
    if (!fof_oe_role_ok(rolebuf)) g_oe_role_bad = 1;
    if (g_oe_nform >= g_oe_cap) {
      int ncap = (g_oe_cap == 0) ? 1024 : 2 * g_oe_cap;
      Term *nf = (Term *)realloc(g_oe_form, sizeof(Term) * (size_t)ncap);
      if (nf == NULL) tp_giveup("OOM in FOF front-end");
      g_oe_form = nf;
      g_oe_cap = ncap;
    }
    g_oe_form[g_oe_nform++] = f;
    // Clausify WITHOUT resetting the Skolem counter (fresh across records).
    u32 sk0 = g_fol_skolem_next;
    Term nnf = fol_nnf(f);
    Term sk  = fol_skolemize(nnf);
    u32 ts0 = g_fol_tseitin_next;
    u32 nc = 0;
    FolClause **recc;
    u32 bound = fof_distrib_bound();
    int rec_tseitin = 0;
    if (fof_route_tseitin(sk, bound)) {
      // Naive distribution would blow past the bound: clausify this
      // record definitionally (Tseitin).  The aux counter runs across
      // records (reset once above) so labels never collide.  A clause
      // wider than FOL_MAX_LITS, on either route, is chunked by the
      // extractor with named remainders -- never dropped.
      recc = fol_tseitin_extract_clauses(sk, &nc);
      g_fof_tseitin_recs++;
      rec_tseitin = 1;
    } else {
      Term cnf_ = fol_distribute(sk);
      recc = fol_extract_clauses(cnf_, &nc);
      // A disjunction past FOL_MAX_LITS was chunked with named
      // remainders: the record's clauses then carry definitions too.
      if (g_fol_tseitin_next > ts0) rec_tseitin = 1;
    }
    if (fol_labels_exhausted()) {
      char why[160];
      snprintf(why, sizeof why, "clausification exhausted the Skolem/definition "
               "label space (%u Skolem, %u definition labels)",
               g_fol_skolem_next, g_fol_tseitin_next);
      tp_giveup(why);
    }
    if (recc == NULL) {
      char why[256];
      size_t nm_len = (role != NULL) ? (size_t)(role - 1 - body) : 0u;
      snprintf(why, sizeof why, "clausification failed (record %.*s%s)",
               (int)(nm_len < 96 ? nm_len : 96), body,
               rec_tseitin ? ", Tseitin" : "");
      tp_giveup(why);
    }
    if (rec_ix >= 0)
      prov_rec_stage(rec_ix, is_conj, nnf, sk, sk0, g_fol_skolem_next,
                     ts0, g_fol_tseitin_next, rec_tseitin, &scope);
    for (u32 i = 0; i < nc; i++) {
      fof_cls_reserve(&cls, &ccnj, &cls_rec, &cls_cap, ncl);
      cls[ncl] = recc[i];
      ccnj[ncl] = (u8)conj_mark;
      if (cls_rec != NULL) cls_rec[ncl] = rec_ix;
      ncl++;
    }
    free(recc);
  }
  if (ncl == 0) tp_giveup("no TPTP records parsed");

  // --fof-stats: front-end-only probe.  Report, per clausified set,
  // how many clauses have >= 2 variable-disjoint literal components
  // (the AVATAR-lite split shapes) and exit -- no engine, no encoding.
  if (getenv("THVM_ATP_FOF_STATS") != NULL) {
    int wide = 0, nonhorn = 0, split = 0, split_nonhorn = 0;
    int split_input_guards = 0;
    u32 max_comp = 1u;
    for (int c = 0; c < ncl; c++) {
      if (cls[c]->n_lits < 2u) continue;
      wide++;
      int npos = 0;
      for (u32 i = 0; i < cls[c]->n_lits; i++)
        if (!cls[c]->lits[i].sign) npos++;
      if (npos >= 2) nonhorn++;
      u32 m = fol_components(cls[c], NULL);
      if (m < 2u) continue;
      split++;
      split_input_guards += (int)m;
      if (npos >= 2) split_nonhorn++;
      if (m > max_comp) max_comp = m;
    }
    printf("FOFSTATS file=%s ncl=%d wide=%d nonhorn=%d split=%d "
           "split_nonhorn=%d input_guards=%d max_comp=%u tseitin=%d\n",
           g_cli_probpath, ncl, wide, nonhorn, split,
           split_nonhorn, split_input_guards, max_comp, g_fof_tseitin_recs);
    fflush(stdout);
    exit(0);
  }

  // One-element-model gate on the full clause set, before any encoding
  // (see fof_one_elt_sat).  A verified model IS the answer
  // (fof_one_elt_claim prints it and exits 0); a refused claim still
  // gives up after pass B, as before.  Split children inherit the
  // parent's verdict path.
  int oe_sat = !g_fof_split_child && fof_one_elt_sat(cls, ncl);
  if (oe_sat) fof_one_elt_claim(text);
  if (getenv("THVM_ATP_ONE_ELT_PROBE") != NULL) {
    char why[160];
    snprintf(why, sizeof why, "one-element probe: %s (%d record(s) via "
             "Tseitin)",
             oe_sat ? "model found, claim refused" : "no one-element model",
             g_fof_tseitin_recs);
    tp_giveup(why);
  }

  // Fresh names: the pc_intern table now holds every problem symbol.
  fof_fresh_const(g_fof_true_name, sizeof g_fof_true_name);
  fof_fresh_const(g_fof_false_name, sizeof g_fof_false_name);
  fof_fresh_const(g_fof_ifeq_name, sizeof g_fof_ifeq_name);
  fof_fresh_const(g_fof_or_name, sizeof g_fof_or_name);
  fof_fresh_const(g_fof_eq_name, sizeof g_fof_eq_name);
  fof_fresh_const(g_fof_not_name, sizeof g_fof_not_name);
  fof_fresh_prefix(g_fof_sk_prefix, sizeof g_fof_sk_prefix);
  fof_fresh_prefix(g_fof_def_prefix, sizeof g_fof_def_prefix);

  // Conversion-lineage serialization: fresh symbol-family prefixes are
  // final and the stage terms are still live.
  if (g_prov_on) prov_serialize_all(cls, cls_rec, ncl);
  free(cls_rec);

  // Pass B: classify + emit.  Negative clauses are collected first so
  // the goal shape (direct vs funnel) is known before emission.
  Term  cond_l[FOL_MAX_LITS], cond_r[FOL_MAX_LITS];
  Term  pos_l[FOL_MAX_LITS],  pos_r[FOL_MAX_LITS];
  int   pos_src[FOL_MAX_LITS];   // positive's literal index (fof-split)
  int  *neg_ix = (int *)malloc(sizeof(int) * (size_t)(ncl + 1));
  if (neg_ix == NULL) tp_giveup("OOM in FOF front-end");
  int n_neg_cls = 0;
  int any_ifeq = 0;
  int neg_unit = 0;              // the (single) negative clause is a unit
  Term neg_unit_l = 0, neg_unit_r = 0;
  g_fof_soft_oversize = 1;       // axiom-side: oversized encodings drop
  for (int c = 0; c < ncl; c++) {
    FolClause *cl = cls[c];
    if (fol_is_tautology(cl)) continue;
    int npos = 0, nneg = 0, drop_clause = 0;
    for (u32 i = 0; i < cl->n_lits && !drop_clause; i++) {
      Term l, r;
      int v = fof_lit_classify(&cl->lits[i], &l, &r);
      if (v == FOF_LIT_DROP_CLAUSE) drop_clause = 1;
      else if (v == FOF_LIT_DROP_LIT) continue;
      else if (cl->lits[i].sign) {
        if (nneg >= (int)FOL_MAX_LITS) tp_giveup("FOF clause exceeds literal bound");
        cond_l[nneg] = l; cond_r[nneg] = r; nneg++;
      } else {
        if (npos >= (int)FOL_MAX_LITS) tp_giveup("FOF clause exceeds literal bound");
        pos_l[npos] = l; pos_r[npos] = r; pos_src[npos] = (int)i; npos++;
      }
    }
    if (drop_clause) continue;
    g_cur_prov_kind = PROV_CNF;
    g_cur_prov_ix   = c;
    if (npos > 1) {
      if (fof_or_mode() == 0)
        tp_giveup("FOF equationalization covers the Horn fragment only "
                  "(a clause has 2+ positive literals)");
      // fof-split branch child: emit the or-clause as the branch's
      // Horn clause (negatives + the picked positive; the other
      // positives drop) -- the whole problem is then pure Horn.  The
      // avatar-shaped assumption records carry the provenance (see
      // the split module's soundness header).
      if (g_fof_split_child) {
        int j = g_fof_split_nor++;
        if (j >= g_fof_split_nchoice)
          tp_giveup("fof-split branch: or-clause count drifted");
        int pick = g_fof_split_choice[j];
        if (pick < 0 || pick >= npos)
          tp_giveup("fof-split branch: pick out of range");
        if (nneg > 0) any_ifeq = 1;
        if (g_prov_on) {
          int six = prov_split_new(cl, c, j, pick, pos_src[pick]);
          g_cur_prov_kind = (six >= 0) ? PROV_SPLIT : PROV_NONE;
          g_cur_prov_ix   = (six >= 0) ? six : 0;
        }
        fof_emit_horn(tp, cond_l, cond_r, nneg,
                      pos_l[pick], pos_r[pick], 0, ccnj[c]);
        continue;
      }
      if (fof_split_mode() > 0) fof_split_scan_or(cl, c, pos_src, npos);
      if (nneg > 0) any_ifeq = 1;
      fof_emit_or(tp, cond_l, cond_r, nneg, pos_l, pos_r, npos, ccnj[c]);
      continue;
    }
    if (npos == 0 && nneg == 0) {
      // empty clause: the input is unsatisfiable outright
      g_cli_fof_trivial = 1;
      if (g_prov_trivial_cnf < 0) g_prov_trivial_cnf = c;
      continue;
    }
    if (npos == 0) {
      neg_ix[n_neg_cls++] = c;
      neg_unit = (nneg == 1);
      if (neg_unit) { neg_unit_l = cond_l[0]; neg_unit_r = cond_r[0]; }
      continue;
    }
    if (nneg > 0) any_ifeq = 1;
    fof_emit_horn(tp, cond_l, cond_r, nneg, pos_l[0], pos_r[0], 0, ccnj[c]);
  }
  g_fof_soft_oversize = 0;
  g_cur_prov_kind = PROV_NONE;
  g_cur_prov_ix   = 0;
  // One-element-model gate (decided before pass B): a set satisfiable
  // in the one-element interpretation is satisfiable -- give up before
  // ANY lane can run an encoding that would spuriously refute it.
  if (oe_sat) tp_giveup("satisfiable in the one-element interpretation");
  // fof-split fitness (parent): Horn except 1..k or-clauses, every
  // positive ground, branch product bounded.  A fitting shape hands
  // the whole wall to the branch orchestration in main -- including
  // the pre-pass slice below.
  if (!g_fof_split_child && fof_split_mode() > 0 && !g_cli_fof_trivial &&
      !g_fof_split_unfit &&
      g_fof_split_nor >= 1 && g_fof_split_nor <= fof_split_k()) {
    long prod = 1;
    for (int j = 0; j < g_fof_split_nor; j++) {
      prod *= g_fof_split_or[j].width;
      if (prod > FOF_SPLIT_MAX_BR) break;
    }
    if (prod >= 2 && prod <= FOF_SPLIT_MAX_BR) g_fof_split_pending = 1;
  }
  if (g_fof_split_child && g_fof_split_nor != g_fof_split_nchoice)
    tp_giveup("fof-split branch: or-clause count drifted");
  if (!g_fof_split_child && fof_split_mode() >= 2 && !g_fof_split_pending &&
      !g_cli_fof_trivial)
    tp_giveup("fof-split slice: no k-bounded ground or-split");
  // Non-Horn input: bounded clausal saturation on the original
  // clauses (case analysis lives there, not in any equational
  // encoding).  Under --proof a win emits the recorded resolution
  // refutation with its conversion lineage (prepass_emit_proof).
  // DEFERRED: main runs it (fof_prepass_run) once the encoding is built
  // and curried, with its usual share of the wall; a give-up before that
  // point -- no negative clause, an unrepresentable goal, a curried goal
  // past the side buffer, no negated conjecture -- runs it through the
  // tp_giveup hook with the whole remaining wall instead of ending the
  // lane with the wall unspent.
  int defer_prepass = (g_fof_or_ncl > 0 && !g_cli_fof_trivial &&
                       !g_fof_split_pending && fof_prepass_enabled());
  if (defer_prepass) {
    g_fof_pp_cls = cls;
    g_fof_pp_ccnj = ccnj;
    g_fof_pp_ncl = ncl;
    g_fof_pp_pending = 1;
    g_tp_giveup_hook = fof_prepass_fallback;
  }
  if (g_cli_fof_trivial && g_cli_want_proof && g_prov_trivial_cnf >= 0 &&
      g_cli_prepass_proof == NULL && g_prov_cnf != NULL) {
    // Clausification itself derived $false: the whole proof is that
    // record's conversion chain (plus an explicit terminal $false
    // when the clause text is a $true/$false literal simplification).
    char *bufp = NULL;
    size_t plen = 0;
    FILE *ms = open_memstream(&bufp, &plen);
    if (ms != NULL) {
      fprintf(ms, "%% SZS output start Proof for %s\n", g_cli_probpath);
      int ok = prov_emit_cnf(ms, g_prov_trivial_cnf);
      if (ok && g_prov_cnf[g_prov_trivial_cnf].text != NULL &&
          strcmp(g_prov_cnf[g_prov_trivial_cnf].text, "$false") != 0)
        fprintf(ms, "cnf(refute_0, plain, $false, "
                    "inference(evaluation, [status(thm)], [c%d])).\n",
                g_prov_trivial_cnf);
      fprintf(ms, "%% SZS output end Proof for %s\n", g_cli_probpath);
      fclose(ms);
      if (ok) g_cli_prepass_proof = bufp;
      else { free(bufp); prov_reset_emitted(); }
    }
  }
  if (g_cli_fof_trivial || g_cli_fof_satref) {
    // Trivially unsatisfiable: a joinable goal (plus one dummy axiom so
    // the positive/negative shape gates pass) proves instantly.
    char tt[TP_NAME_LEN + 4];
    snprintf(tt, sizeof tt, "%s", g_fof_true_name);
    g_cur_prov_kind = g_prov_on ? PROV_HELPER : PROV_NONE;
    tp_fof_push(tp, tt, tt, 0, 0);
    if (g_prov_on) {
      g_cur_prov_kind = (g_prov_trivial_cnf >= 0) ? PROV_TRIVIAL_GOAL
                                                  : PROV_HELPER;
      g_cur_prov_ix = (g_prov_trivial_cnf >= 0) ? g_prov_trivial_cnf : 0;
    }
    tp_fof_push(tp, tt, tt, 1, 1);
  } else if (n_neg_cls == 1 && neg_unit) {
    // exactly one negative clause and it is a unit: the goal is direct
    static char lbuf[TP_SIDE_LEN * 4], rbuf[TP_SIDE_LEN * 4];
    FofVarScope sc; sc.n = 0;
    size_t lo = 0, ro = 0;
    fof_ser_term(neg_unit_l, &sc, lbuf, sizeof lbuf, &lo);
    if (neg_unit_r == 0) fof_ser_emit(rbuf, sizeof rbuf, &ro, g_fof_true_name);
    else fof_ser_term(neg_unit_r, &sc, rbuf, sizeof rbuf, &ro);
    g_cur_prov_kind = PROV_CNF;
    g_cur_prov_ix   = neg_ix[0];
    tp_fof_push(tp, lbuf, rbuf, 1, 1);
  } else if (n_neg_cls >= 1) {
    // several negative clauses: refutation needs ANY of them, but the
    // engine proves multiple negative units as a CONJUNCTION -- funnel
    // each into a false-concluding rule and use the single ground goal.
    // Funnel rules are individually droppable when oversized: each is
    // one refutation PATH, and the ground goal below stays.
    any_ifeq = 1;
    g_fof_soft_oversize = 1;
    for (int k = 0; k < n_neg_cls; k++) {
      FolClause *cl = cls[neg_ix[k]];
      int nneg = 0;
      for (u32 i = 0; i < cl->n_lits; i++) {
        Term l, r;
        int v = fof_lit_classify(&cl->lits[i], &l, &r);
        if (v == FOF_LIT_PAIR) {
          if (nneg >= (int)FOL_MAX_LITS) tp_giveup("FOF clause exceeds literal bound");
          cond_l[nneg] = l; cond_r[nneg] = r; nneg++;
        }
      }
      g_cur_prov_kind = PROV_CNF;
      g_cur_prov_ix   = neg_ix[k];
      fof_emit_horn(tp, cond_l, cond_r, nneg, 0, 0, 0, ccnj[neg_ix[k]]);
    }
    g_fof_soft_oversize = 0;
    if (g_prov_on) {
      g_prov_funnel = (int *)malloc(sizeof(int) * (size_t)n_neg_cls);
      if (g_prov_funnel != NULL) {
        for (int k = 0; k < n_neg_cls; k++) g_prov_funnel[k] = neg_ix[k];
        g_prov_nfunnel = n_neg_cls;
      }
    }
    char gl[TP_NAME_LEN], gr[TP_NAME_LEN];
    snprintf(gl, sizeof gl, "%s", g_fof_false_name);
    snprintf(gr, sizeof gr, "%s", g_fof_true_name);
    g_cur_prov_kind = g_prov_on ? PROV_FUNNEL_GOAL : PROV_NONE;
    g_cur_prov_ix   = 0;
    tp_fof_push(tp, gl, gr, 1, 1);
  } else {
    tp_giveup("no negative clause after clausification (nothing to refute)");
  }
  g_cur_prov_kind = PROV_NONE;
  g_cur_prov_ix   = 0;
  // Or-lane extras: every negative clause exports its content in
  // rewrite form (unit propagation over or-chains).  Gated on the
  // lane having fired so Horn problems stay byte-identical.
  if (g_fof_or_used && !g_cli_fof_trivial && !g_cli_fof_satref) {
    g_fof_soft_oversize = 1;   // redundant or-lane extras: droppable
    for (int k = 0; k < n_neg_cls; k++) {
      FolClause *cl = cls[neg_ix[k]];
      int nneg = 0;
      for (u32 i = 0; i < cl->n_lits; i++) {
        Term l, r;
        int v = fof_lit_classify(&cl->lits[i], &l, &r);
        if (v == FOF_LIT_PAIR) {
          if (nneg >= (int)FOL_MAX_LITS) tp_giveup("FOF clause exceeds literal bound");
          cond_l[nneg] = l; cond_r[nneg] = r; nneg++;
        }
      }
      g_cur_prov_kind = PROV_CNF;
      g_cur_prov_ix   = neg_ix[k];
      fof_emit_neg_extra(tp, cond_l, cond_r, nneg, ccnj[neg_ix[k]]);
    }
    g_fof_soft_oversize = 0;
    g_cur_prov_kind = PROV_NONE;
    g_cur_prov_ix   = 0;
  }
  if (g_prov_on) g_cur_prov_kind = PROV_HELPER;
  if (g_fof_or_used && !g_cli_fof_trivial && !g_cli_fof_satref) {
    char hl[TP_NAME_LEN * 4 + 16];
    snprintf(hl, sizeof hl, "%s(%s,X0)", g_fof_or_name, g_fof_true_name);
    tp_fof_push(tp, hl, g_fof_true_name, 0, 0);
    snprintf(hl, sizeof hl, "%s(X0,%s)", g_fof_or_name, g_fof_true_name);
    tp_fof_push(tp, hl, g_fof_true_name, 0, 0);
    snprintf(hl, sizeof hl, "%s(%s,X0)", g_fof_or_name, g_fof_false_name);
    tp_fof_push(tp, hl, "X0", 0, 0);
    snprintf(hl, sizeof hl, "%s(X0,%s)", g_fof_or_name, g_fof_false_name);
    tp_fof_push(tp, hl, "X0", 0, 0);
    if (g_fof_eq_used) {
      snprintf(hl, sizeof hl, "%s(X0,X0)", g_fof_eq_name);
      tp_fof_push(tp, hl, g_fof_true_name, 0, 0);
      // bridge: a derived eq(s,t) -> true CP-resolves to s = t
      snprintf(hl, sizeof hl, "%s(%s(X0,X1),%s,X0,X1)",
               g_fof_ifeq_name, g_fof_eq_name, g_fof_true_name);
      tp_fof_push(tp, hl, "X1", 0, 0);
      any_ifeq = 1;
    }
    if (g_fof_not_used) {
      snprintf(hl, sizeof hl, "%s(%s)", g_fof_not_name, g_fof_true_name);
      tp_fof_push(tp, hl, g_fof_false_name, 0, 0);
      snprintf(hl, sizeof hl, "%s(%s)", g_fof_not_name, g_fof_false_name);
      tp_fof_push(tp, hl, g_fof_true_name, 0, 0);
      // bridge: a derived not(g) -> true CP-resolves to g = false
      snprintf(hl, sizeof hl, "%s(%s(X0),%s,X0,%s)",
               g_fof_ifeq_name, g_fof_not_name, g_fof_true_name,
               g_fof_false_name);
      tp_fof_push(tp, hl, g_fof_false_name, 0, 0);
      any_ifeq = 1;
    }
  }
  if (any_ifeq) {
    char dl[TP_NAME_LEN * 4 + 16];
    snprintf(dl, sizeof dl, "%s(X0,X0,X1,X2)", g_fof_ifeq_name);
    tp_fof_push(tp, dl, "X1", 0, 0);
  }
  g_cur_prov_kind = PROV_NONE;
  g_cur_prov_ix   = 0;
  if (!defer_prepass) {
    for (int c = 0; c < ncl; c++) fol_clause_free(cls[c]);
    free(cls); free(ccnj);
  }
  free(neg_ix);
  thvm_heap_exhaust_jmp = NULL;
  // Signature + variable table + the existential-goal flag, exactly as
  // tp_parse computes them.
  for (int i = 0; i < tp->n_cls; i++) {
    tp_scan_term(tp, tp->cls[i].lhs);
    tp_scan_term(tp, tp->cls[i].rhs);
    if (tp->cls[i].neg && !g_cli_goal_existential &&
        (tp_side_has_var(tp->cls[i].lhs) || tp_side_has_var(tp->cls[i].rhs)))
      g_cli_goal_existential = 1;
  }
  char orinfo[64] = "";
  if (g_fof_or_ncl > 0)
    snprintf(orinfo, sizeof orinfo, ", %d or-encoded", g_fof_or_ncl);
  char tsinfo[64] = "";
  if (g_fof_tseitin_recs > 0)
    snprintf(tsinfo, sizeof tsinfo, ", %d record(s) via Tseitin",
             g_fof_tseitin_recs);
  char drinfo[64] = "";
  if (g_fof_dropped_n > 0)
    snprintf(drinfo, sizeof drinfo, ", %d oversized clause(s) dropped",
             g_fof_dropped_n);
  fprintf(stderr, "FindProof: FOF/clausal front-end: %d clause(s) -> %d unit "
          "equation(s)%s%s%s%s%s\n", ncl, tp->n_cls,
          any_ifeq ? " (ifeq encoding)" : "",
          orinfo, tsinfo, drinfo,
          g_cli_fof_conjecture ? ", conjecture negated" : "");
}

// Emit the .pr text, following the WL wmGenerateProblem conventions:
// KBO, every weight 1, precedence sorted by (arity ascending, name
// ascending) with the FIRST symbol highest.
static char *tp_emit_pr(TpProblem *tp, const char *name) {
  size_t cap = 1u << 20;
  char *out = (char *)malloc(cap);
  size_t o = 0;
#define EMIT(...) do { o += (size_t)snprintf(out + o, cap - o, __VA_ARGS__); } while (0)
  EMIT("NAME          %s\n", name);
  EMIT("MODE          PROOF\n");
  EMIT("SORTS         ANY\n");
  // SIGNATURE: alphabetical (wmBuildPr sorts the sig listing by name)
  TpSym *alpha = (TpSym *)malloc(sizeof(TpSym) * (size_t)(tp->n_syms + 1));
  memcpy(alpha, tp->syms, sizeof(TpSym) * (size_t)tp->n_syms);
  for (int i = 0; i < tp->n_syms; i++)
    for (int j = i + 1; j < tp->n_syms; j++)
      if (strcmp(alpha[j].name, alpha[i].name) < 0) { TpSym t = alpha[i]; alpha[i] = alpha[j]; alpha[j] = t; }
  EMIT("SIGNATURE     ");
  for (int i = 0; i < tp->n_syms; i++) {
    if (i > 0) EMIT("              ");
    EMIT("%s: ", alpha[i].name);
    for (int a = 0; a < alpha[i].arity; a++) EMIT("ANY ");
    EMIT("-> ANY\n");
  }
  EMIT("ORDERING      KBO\n");
  EMIT("              ");
  for (int i = 0; i < tp->n_syms; i++) EMIT("%s%s=1", (i > 0) ? ", " : "", alpha[i].name);
  EMIT("\n");
  TpSym *prec = (TpSym *)malloc(sizeof(TpSym) * (size_t)(tp->n_syms + 1));
  memcpy(prec, tp->syms, sizeof(TpSym) * (size_t)tp->n_syms);
  qsort(prec, (size_t)tp->n_syms, sizeof(TpSym), tp_sym_cmp);
  EMIT("              ");
  for (int i = 0; i < tp->n_syms; i++) EMIT("%s%s", (i > 0) ? " > " : "", prec[i].name);
  EMIT("\n");
  EMIT("VARIABLES     ");
  if (tp->n_vars == 0) EMIT(": ANY\n");
  else {
    for (int i = 0; i < tp->n_vars; i++) EMIT("%s%s", (i > 0) ? ", " : "", tp->vars[i]);
    EMIT(": ANY\n");
  }
  EMIT("EQUATIONS     ");
  int first = 1;
  for (int i = 0; i < tp->n_cls; i++) {
    if (tp->cls[i].neg) continue;
    if (!first) EMIT("              ");
    EMIT("%s = %s\n", tp->cls[i].lhs, tp->cls[i].rhs);
    first = 0;
  }
  EMIT("CONCLUSION    ");
  first = 1;
  for (int i = 0; i < tp->n_cls; i++) {
    if (!tp->cls[i].neg) continue;
    if (!first) EMIT("              ");
    EMIT("%s = %s\n", tp->cls[i].lhs, tp->cls[i].rhs);
    first = 0;
  }
#undef EMIT
  free(alpha); free(prec);
  return out;
}

