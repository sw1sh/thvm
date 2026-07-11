// ProofCheck: FindProof's proof-CHECKING mode -- a pure-C validator for
// first-order TSTP refutation proofs (the ProoVer 2026 task).  Included
// by tools/thvm_atp_cli.c AFTER src/thvm.c, so it reuses the FOL layer
// already linked into the binary: the formula constructors (fol_mk_*),
// the NNF/Skolemize/clausify pipeline (fol_formula_to_clauses), and the
// clause saturation prover (cnf_init/cnf_add_clause/cnf_run).  No Wolfram
// kernel, no external prover.
//
//   FindProof --check proof.p           -> % SZS status <verdict> for proof.p
//
// A proof is a sequence of annotated formulae; each DERIVED step carries
// an inference rule and cites parents.  Every step is validated
// independently by dispatching on its rule, and one SZS verdict is
// reported for the whole proof:
//
//   Verified        every derived step independently confirmed  (+1)
//   FailedVerified  a step is provably unsound                   (+2)
//   NotVerified     a step could not be settled -- abstain       ( 0)
//
// The scoring penalises accepting a bad proof far more than abstaining,
// so the checker is conservative: an INVALID verdict is emitted only when
// a step is CERTAIN to be unsound (a completed saturation exhibits a
// counter-model, a Skolem symbol is demonstrably not fresh), and anything
// undecided within the resource bound falls back to NotVerified.
//
// Obligations, all discharged through the saturation engine except the
// structural Skolem check:
//   instantiate        parent |= conclusion          (fast path: match)
//   negated_conjecture conclusion == not(parent)      (both entailments)
//   skolemize          a fresh Skolem symbol is introduced and used
//   consequence/horn/deduction   parents |= conclusion
//   (final)            parents |= $false  (refutation closes)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

// --- limits ---------------------------------------------------------
#define PC_MAX_STEPS    8192
#define PC_MAX_PARENTS  64
#define PC_NAME         192
#define PC_MAX_SYMS     16384
#define PC_MAX_VARS     256
#define PC_MAX_TOK      16384
#define PC_ENTAIL_CAP   1500    // saturation step cap per entailment (default)

static u32 pc_cap = PC_ENTAIL_CAP;   // runtime override via env, set in worker

// --- FOL label conventions (mirror src/fol/cnf.c; file-local there) --
#define PC_LAB_EQ    0u
#define PC_LAB_FALSE 90u
#define PC_LAB_TRUE  91u
#define PC_LAB_BASE  100u       // user predicate/function labels start here
#define PC_LAB_NOT   0xFFF0u
#define PC_LAB_AND   0xFFF1u
#define PC_LAB_OR    0xFFF2u
#define PC_LAB_IMP   0xFFF3u
#define PC_LAB_IFF   0xFFF4u
#define PC_LAB_ALL   0xFFF5u
#define PC_LAB_EX    0xFFF6u
#define PC_NOT_FOUND 0xFFFFFFFFu

// ===================================================================
// symbol interning: predicate/function/constant name -> stable label
// ===================================================================

static char pc_sym[PC_MAX_SYMS][PC_NAME];
static int  pc_nsyms = 0;

static u32 pc_intern(const char *name) {
  for (int i = 0; i < pc_nsyms; i++)
    if (strcmp(pc_sym[i], name) == 0) return PC_LAB_BASE + (u32)i;
  if (pc_nsyms >= PC_MAX_SYMS) return PC_LAB_BASE;  // saturate; harmless
  snprintf(pc_sym[pc_nsyms], PC_NAME, "%s", name);
  return PC_LAB_BASE + (u32)pc_nsyms++;
}
// Label of an already-seen name, or PC_NOT_FOUND (never interned).
static u32 pc_lookup(const char *name) {
  for (int i = 0; i < pc_nsyms; i++)
    if (strcmp(pc_sym[i], name) == 0) return PC_LAB_BASE + (u32)i;
  return PC_NOT_FOUND;
}

// ===================================================================
// tokenizer (one record at a time)
// ===================================================================

enum {
  K_IDENT, K_VAR, K_LP, K_RP, K_LB, K_RB, K_COMMA, K_COLON,
  K_ALL, K_EX, K_NOT, K_AND, K_OR, K_IMP, K_IFF, K_EQ, K_NEQ,
  K_STR, K_EOF
};
typedef struct { int kind; char text[PC_NAME]; } PcTok;

// Lex `s` into `out` (up to `max`).  Returns token count, or -1 on
// overflow / bad character.
static int pc_lex(const char *s, PcTok *out, int max) {
  int n = 0;
  while (*s) {
    if (isspace((unsigned char)*s)) { s++; continue; }
    if (n >= max - 1) return -1;
    PcTok *t = &out[n];
    t->text[0] = '\0';
    char c = *s;
    if (c == '(') { t->kind = K_LP;    s++; }
    else if (c == ')') { t->kind = K_RP;    s++; }
    else if (c == '[') { t->kind = K_LB;    s++; }
    else if (c == ']') { t->kind = K_RB;    s++; }
    else if (c == ',') { t->kind = K_COMMA; s++; }
    else if (c == ':') { t->kind = K_COLON; s++; }
    else if (c == '~') { t->kind = K_NOT;   s++; }
    else if (c == '&') { t->kind = K_AND;   s++; }
    else if (c == '|') { t->kind = K_OR;    s++; }
    else if (c == '?') { t->kind = K_EX;    s++; }
    else if (c == '!') {
      if (s[1] == '=') { t->kind = K_NEQ; s += 2; }
      else             { t->kind = K_ALL; s += 1; }
    }
    else if (c == '=') {
      if (s[1] == '>') { t->kind = K_IMP; s += 2; }
      else             { t->kind = K_EQ;  s += 1; }
    }
    else if (c == '<') {
      if (s[1] == '=' && s[2] == '>') { t->kind = K_IFF; s += 3; }
      else return -1;
    }
    else if (c == '\'') {                      // 'quoted atom'
      s++; int k = 0;
      while (*s && *s != '\'' && k < PC_NAME - 1) t->text[k++] = *s++;
      t->text[k] = '\0';
      if (*s == '\'') s++;
      t->kind = K_STR;
    }
    else if (c == '$' || c == '_' || isalnum((unsigned char)c)) {
      int k = 0;
      while (*s && (*s == '$' || *s == '_' || isalnum((unsigned char)*s))
             && k < PC_NAME - 1)
        t->text[k++] = *s++;
      t->text[k] = '\0';
      // TSTP: a variable starts with an uppercase letter.
      t->kind = isupper((unsigned char)t->text[0]) ? K_VAR : K_IDENT;
    }
    else return -1;
    n++;
  }
  out[n].kind = K_EOF; out[n].text[0] = '\0';
  return n;
}

// ===================================================================
// recursive-descent formula parser: tokens -> Term
// ===================================================================

typedef struct {
  PcTok *t;
  int    pos;
  int    ok;
  char   vname[PC_MAX_VARS][PC_NAME];   // per-record variable scope
  int    nv;
} PcParse;

static int pc_peek(PcParse *p) { return p->t[p->pos].kind; }
static PcTok *pc_cur(PcParse *p) { return &p->t[p->pos]; }
static void pc_adv(PcParse *p) { p->pos++; }
static void pc_expect(PcParse *p, int kind) {
  if (pc_peek(p) == kind) pc_adv(p); else p->ok = 0;
}
static u32 pc_varid(PcParse *p, const char *name) {
  for (int i = 0; i < p->nv; i++)
    if (strcmp(p->vname[i], name) == 0) return (u32)i;
  if (p->nv >= PC_MAX_VARS) { p->ok = 0; return 0; }
  snprintf(p->vname[p->nv], PC_NAME, "%s", name);
  return (u32)p->nv++;
}

static Term pc_formula(PcParse *p);   // fwd

// term := VAR | ident [ '(' term (',' term)* ')' ]
static Term pc_term(PcParse *p) {
  if (pc_peek(p) == K_VAR) {
    u32 id = pc_varid(p, pc_cur(p)->text);
    pc_adv(p);
    return term_new_fvr(id);
  }
  if (pc_peek(p) == K_IDENT || pc_peek(p) == K_STR) {
    char name[PC_NAME];
    snprintf(name, PC_NAME, "%s", pc_cur(p)->text);
    pc_adv(p);
    if (pc_peek(p) == K_LP) {
      pc_adv(p);
      Term kids[REWRITE_MAX_ARITY];
      u32 n = 0;
      for (;;) {
        if (n >= REWRITE_MAX_ARITY) { p->ok = 0; return 0; }
        kids[n++] = pc_term(p);
        if (pc_peek(p) == K_COMMA) { pc_adv(p); continue; }
        break;
      }
      pc_expect(p, K_RP);
      return term_new_ctr(pc_intern(name), kids, n);
    }
    return term_new_ctr(pc_intern(name), NULL, 0);   // constant
  }
  p->ok = 0;
  return 0;
}

// atom := '$false' | '$true' | term [ ('='|'!=') term ]
static Term pc_atom(PcParse *p) {
  if (pc_peek(p) == K_IDENT) {
    const char *nm = pc_cur(p)->text;
    if (strcmp(nm, "$false") == 0) { pc_adv(p); return term_new_ctr(PC_LAB_FALSE, NULL, 0); }
    if (strcmp(nm, "$true")  == 0) { pc_adv(p); return term_new_ctr(PC_LAB_TRUE,  NULL, 0); }
  }
  Term a = pc_term(p);
  if (pc_peek(p) == K_EQ) {
    pc_adv(p);
    Term b = pc_term(p);
    Term k[2] = {a, b};
    return term_new_ctr(PC_LAB_EQ, k, 2);
  }
  if (pc_peek(p) == K_NEQ) {
    pc_adv(p);
    Term b = pc_term(p);
    Term k[2] = {a, b};
    return fol_mk_not(term_new_ctr(PC_LAB_EQ, k, 2));
  }
  return a;   // predicate atom
}

// quant := ('!'|'?') '[' VAR (',' VAR)* ']' ':' unary
static Term pc_quant(PcParse *p) {
  int q = pc_peek(p);   // K_ALL / K_EX
  pc_adv(p);
  pc_expect(p, K_LB);
  u32 ids[PC_MAX_VARS];
  int nq = 0;
  for (;;) {
    if (pc_peek(p) != K_VAR) { p->ok = 0; break; }
    if (nq < PC_MAX_VARS) ids[nq++] = pc_varid(p, pc_cur(p)->text);
    pc_adv(p);
    if (pc_peek(p) == K_COMMA) { pc_adv(p); continue; }
    break;
  }
  pc_expect(p, K_RB);
  pc_expect(p, K_COLON);
  Term body = pc_formula(p);
  for (int i = nq - 1; i >= 0; i--)
    body = (q == K_ALL) ? fol_mk_all(term_new_fvr(ids[i]), body)
                        : fol_mk_ex (term_new_fvr(ids[i]), body);
  return body;
}

// unary := '~' unary | quant | '(' formula ')' | atom
static Term pc_unary(PcParse *p) {
  int k = pc_peek(p);
  if (k == K_NOT) { pc_adv(p); return fol_mk_not(pc_unary(p)); }
  if (k == K_ALL || k == K_EX) return pc_quant(p);
  if (k == K_LP) { pc_adv(p); Term f = pc_formula(p); pc_expect(p, K_RP); return f; }
  return pc_atom(p);
}

// and := unary ('&' unary)*
static Term pc_and(PcParse *p) {
  Term a = pc_unary(p);
  while (pc_peek(p) == K_AND) { pc_adv(p); a = fol_mk_and(a, pc_unary(p)); }
  return a;
}
// or := and ('|' and)*
static Term pc_or(PcParse *p) {
  Term a = pc_and(p);
  while (pc_peek(p) == K_OR) { pc_adv(p); a = fol_mk_or(a, pc_and(p)); }
  return a;
}
// formula := or [ ('=>'|'<=>') formula ]   (=>, <=> right-associative)
static Term pc_formula(PcParse *p) {
  Term a = pc_or(p);
  if (pc_peek(p) == K_IMP) { pc_adv(p); return fol_mk_imp(a, pc_formula(p)); }
  if (pc_peek(p) == K_IFF) { pc_adv(p); return fol_mk_iff(a, pc_formula(p)); }
  return a;
}

// Parse a whole formula string; 0 on failure.
static Term pc_parse_formula(const char *fstr, PcParse *scope) {
  PcTok toks[PC_MAX_TOK];
  int nt = pc_lex(fstr, toks, PC_MAX_TOK);
  if (nt < 0) return 0;
  scope->t = toks; scope->pos = 0; scope->ok = 1; scope->nv = 0;
  Term f = pc_formula(scope);
  if (!scope->ok || pc_peek(scope) != K_EOF) return 0;
  return f;
}

// ===================================================================
// term utilities
// ===================================================================

static int pc_is_false(Term t) {
  return term_tag(t) == TAG_CTR && term_ext(t) == PC_LAB_FALSE;
}
static int pc_has_label(Term t, u32 L) {
  if (term_tag(t) != TAG_CTR) return 0;
  if (term_ext(t) == L) return 1;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (pc_has_label(term_ctr_at(t, i), L)) return 1;
  return 0;
}

// Free-variable universal closure: any FVR not bound by an enclosing
// quantifier is closed with a leading forall (TSTP treats a formula's
// free variables as universally quantified).
static void pc_free_rec(Term t, unsigned char *bound, unsigned char *freev) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < PC_MAX_VARS && !bound[id]) freev[id] = 1;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      if (lab == PC_LAB_ALL || lab == PC_LAB_EX) {
        Term var = term_ctr_at(t, 0);
        u32 vid = term_ext(var);
        unsigned char saved = (vid < PC_MAX_VARS) ? bound[vid] : 0;
        if (vid < PC_MAX_VARS) bound[vid] = 1;
        pc_free_rec(term_ctr_at(t, 1), bound, freev);
        if (vid < PC_MAX_VARS) bound[vid] = saved;
        return;
      }
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) pc_free_rec(term_ctr_at(t, i), bound, freev);
      return;
    }
    default: return;
  }
}
static Term pc_close(Term f) {
  unsigned char bound[PC_MAX_VARS], freev[PC_MAX_VARS];
  memset(bound, 0, sizeof bound);
  memset(freev, 0, sizeof freev);
  pc_free_rec(f, bound, freev);
  Term r = f;
  for (int id = PC_MAX_VARS - 1; id >= 0; id--)
    if (freev[id]) r = fol_mk_all(term_new_fvr((u32)id), r);
  return r;
}

// alpha-equivalence (structural equality up to a variable bijection).
static int pc_alpha_rec(Term a, Term b, int *ab, int *ba) {
  u32 ta = term_tag(a), tb = term_tag(b);
  if (ta != tb) return 0;
  if (ta == TAG_FVR) {
    int ia = (int)term_ext(a), ib = (int)term_ext(b);
    if (ia < 0 || ia >= PC_MAX_VARS || ib < 0 || ib >= PC_MAX_VARS) return ia == ib;
    if (ab[ia] == -1 && ba[ib] == -1) { ab[ia] = ib; ba[ib] = ia; return 1; }
    return ab[ia] == ib && ba[ib] == ia;
  }
  if (ta == TAG_CTR) {
    if (term_ext(a) != term_ext(b)) return 0;
    u32 na = term_ctr_n(a), nb = term_ctr_n(b);
    if (na != nb) return 0;
    for (u32 i = 0; i < na; i++)
      if (!pc_alpha_rec(term_ctr_at(a, i), term_ctr_at(b, i), ab, ba)) return 0;
    return 1;
  }
  return a == b;
}
static int pc_alpha_eq(Term a, Term b) {
  int ab[PC_MAX_VARS], ba[PC_MAX_VARS];
  for (int i = 0; i < PC_MAX_VARS; i++) { ab[i] = -1; ba[i] = -1; }
  return pc_alpha_rec(a, b, ab, ba);
}
// Strip leading universal quantifiers -> matrix (bound vars become free
// pattern variables usable by thvm_match).
static Term pc_strip_forall(Term f) {
  while (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_ALL)
    f = term_ctr_at(f, 1);
  return f;
}
static u32 pc_max_var(Term t) {
  if (term_tag(t) == TAG_FVR) return term_ext(t) + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 m = 0, n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) { u32 c = pc_max_var(term_ctr_at(t, i)); if (c > m) m = c; }
    return m;
  }
  return 0;
}

// ===================================================================
// entailment oracle -- clausify a formula list and saturate
// ===================================================================

// Refute a conjunction: clausify all (universally closed) formulas and
// saturate.  ATP_PROVED/ATP_REFUTED = unsatisfiable, ATP_QUEUE_EMPTY =
// saturated satisfiable, anything else = undecided within the bound.
//
// The list is conjoined into ONE formula before clausification, because
// fol_formula_to_clauses resets the Skolem counter on entry: clausifying
// each formula separately would give distinct existentials the SAME
// Skolem constant and turn a satisfiable set spuriously unsatisfiable.
static AtpStatus pc_refute(Term *fs, int n) {
  if (n <= 0) return ATP_QUEUE_EMPTY;
  Term conj = pc_close(fs[0]);
  for (int i = 1; i < n; i++) conj = fol_mk_and(conj, pc_close(fs[i]));
  u32 nc = 0;
  FolClause **cl = fol_formula_to_clauses(conj, &nc);
  if (cl == NULL) return ATP_ABORTED;
  CnfState *s = cnf_init(pc_cap);
  for (u32 j = 0; j < nc; j++) cnf_add_clause(s, cl[j]);
  free(cl);
  AtpStatus st = cnf_run(s);
  cnf_free(s);
  return st;
}

enum { PC_VALID, PC_INVALID, PC_UNKNOWN };

// parents |= concl  (concl==0 means the conclusion is $false).
static int pc_entails(Term *parents, int np, Term concl) {
  Term fs[PC_MAX_PARENTS + 1];
  int n = 0;
  for (int i = 0; i < np && n < PC_MAX_PARENTS; i++) fs[n++] = parents[i];
  if (concl != 0) fs[n++] = fol_mk_not(pc_close(concl));
  AtpStatus st = pc_refute(fs, n);
  if (st == ATP_PROVED || st == ATP_REFUTED) return PC_VALID;
  if (st == ATP_QUEUE_EMPTY) return PC_INVALID;   // counter-model exists
  return PC_UNKNOWN;
}

// ===================================================================
// proof records
// ===================================================================

enum { R_INPUT, R_INSTANTIATE, R_NEGCONJ, R_SKOLEMIZE,
       R_CONSEQUENCE, R_DEDUCTION, R_HORN, R_UNKNOWN };

typedef struct {
  char name[PC_NAME];
  Term formula;
  int  rule;
  char parents[PC_MAX_PARENTS][PC_NAME];
  int  n_parents;
  char new_sym[PC_NAME];      // declared Skolem symbol, or ""
} PcStep;

static PcStep pc_steps[PC_MAX_STEPS];
static int pc_nstep = 0;

static int pc_rule_of(const char *r) {
  if (strcmp(r, "instantiate") == 0)        return R_INSTANTIATE;
  if (strcmp(r, "negated_conjecture") == 0) return R_NEGCONJ;
  if (strcmp(r, "skolemize") == 0)          return R_SKOLEMIZE;
  if (strcmp(r, "consequence") == 0)        return R_CONSEQUENCE;
  if (strcmp(r, "deduction") == 0)          return R_DEDUCTION;
  if (strcmp(r, "horn") == 0)               return R_HORN;
  return R_UNKNOWN;
}
static int pc_find(const char *name) {
  for (int i = 0; i < pc_nstep; i++)
    if (strcmp(pc_steps[i].name, name) == 0) return i;
  return -1;
}

// Read a run of identifier characters from `s` into `buf`; returns the
// advanced pointer.
static const char *pc_ident(const char *s, char *buf, int cap) {
  int k = 0;
  while (*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '$') && k < cap - 1)
    buf[k++] = *s++;
  buf[k] = '\0';
  return s;
}

// Fill rule / parents / new_sym from an inference(...) or file(...) source.
static void pc_parse_source(const char *src, PcStep *st) {
  st->rule = R_UNKNOWN;
  st->n_parents = 0;
  st->new_sym[0] = '\0';
  while (*src && isspace((unsigned char)*src)) src++;
  if (strncmp(src, "file", 4) == 0) { st->rule = R_INPUT; return; }
  const char *inf = strstr(src, "inference");
  if (inf == NULL) return;
  const char *p = strchr(inf, '(');
  if (p == NULL) return;
  p++;
  while (*p && isspace((unsigned char)*p)) p++;
  char rule[PC_NAME];
  pc_ident(p, rule, PC_NAME);
  st->rule = pc_rule_of(rule);

  // declared new Skolem symbol: new_symbols(skolem, [sK...])
  const char *ns = strstr(src, "new_symbols");
  if (ns != NULL) {
    const char *lb = strchr(ns, '[');
    if (lb != NULL) {
      const char *q = lb + 1;
      while (*q && isspace((unsigned char)*q)) q++;
      pc_ident(q, st->new_sym, PC_NAME);
    }
  }
  // parents: the LAST bracketed list (it follows the annotation block).
  const char *lastlb = NULL;
  for (const char *c = inf; *c; c++) if (*c == '[') lastlb = c;
  if (lastlb != NULL) {
    const char *q = lastlb + 1;
    while (*q && *q != ']') {
      while (*q && (isspace((unsigned char)*q) || *q == ',')) q++;
      if (*q == ']' || *q == '\0') break;
      char nm[PC_NAME];
      const char *nq = pc_ident(q, nm, PC_NAME);
      if (nq == q) break;                 // not an identifier -> stop
      q = nq;
      // skip a nested parenthesised term (a compound parent we can't
      // resolve to a step) rather than mis-recording it.
      if (*q == '(') { int d = 0; do { if (*q=='(') d++; else if (*q==')') d--; q++; } while (*q && d); }
      else if (st->n_parents < PC_MAX_PARENTS)
        snprintf(st->parents[st->n_parents++], PC_NAME, "%s", nm);
    }
  }
}

// Split fof(...)/cnf(...) inner content on TOP-LEVEL commas (respecting
// (), [] and quotes).  Returns the argument count.
static int pc_split_args(const char *s, char args[][8192], int max) {
  int depth = 0, inq = 0, na = 0, k = 0;
  if (na < max) args[na][0] = '\0';
  for (; *s; s++) {
    char c = *s;
    if (inq) { if (c == '\'') inq = 0; if (na < max && k < 8191) args[na][k++] = c; continue; }
    if (c == '\'') { inq = 1; if (na < max && k < 8191) args[na][k++] = c; continue; }
    if (c == '(' || c == '[') depth++;
    else if (c == ')' || c == ']') depth--;
    if (c == ',' && depth == 0) {
      if (na < max) args[na][k] = '\0';
      na++; k = 0;
      if (na < max) args[na][0] = '\0';
      continue;
    }
    if (na < max && k < 8191) args[na][k++] = c;
  }
  if (na < max) args[na][k] = '\0';
  return na + 1;
}

static void pc_trim(char *s) {
  int n = (int)strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
  int i = 0; while (s[i] && isspace((unsigned char)s[i])) i++;
  if (i) memmove(s, s + i, strlen(s + i) + 1);
}

// Parse one `fof(...)` / `cnf(...)` record.  Returns 1 on success.
static int pc_parse_record(const char *rec) {
  while (*rec && isspace((unsigned char)*rec)) rec++;
  int is_fof = strncmp(rec, "fof", 3) == 0;
  int is_cnf = strncmp(rec, "cnf", 3) == 0;
  if (!is_fof && !is_cnf) return 0;
  const char *op = strchr(rec, '(');
  if (op == NULL) return 0;
  // inner = between the outermost ( ) of the record
  const char *inner = op + 1;
  static char args[8][8192];
  int na = pc_split_args(inner, args, 8);
  if (na < 3) return 0;
  // Strip the ONE record-closing ')' left on the last argument.  The
  // old greedy loop ate every trailing paren, mangling any formula that
  // ends in a nested application -- exactly the shape of FindProof's
  // own axiom records (no file() source, so the formula IS the last
  // argument): 'multiply(V,W,multiply(X,Y,Z))' lost both inner parens
  // and the whole step silently failed to parse ('parent missing').
  char *last = args[na - 1];
  int ll = (int)strlen(last);
  while (ll > 0 && isspace((unsigned char)last[ll - 1])) last[--ll] = '\0';
  if (ll > 0 && last[ll - 1] == ')') last[--ll] = '\0';
  while (ll > 0 && isspace((unsigned char)last[ll - 1])) last[--ll] = '\0';

  if (pc_nstep >= PC_MAX_STEPS) return 0;
  PcStep *st = &pc_steps[pc_nstep];
  memset(st, 0, sizeof *st);
  pc_trim(args[0]);
  snprintf(st->name, PC_NAME, "%s", args[0]);

  PcParse scope;
  Term f = pc_parse_formula(args[2], &scope);
  if (f == 0) {
    if (getenv("THVM_PROOFCHECK_DBG") != NULL) {
      fprintf(stderr, "PCDBG parse-skip name=%s formula=[%.120s]\n",
              args[0], args[2]);
    }
    return 0;                             // unparseable formula -> skip
  }
  st->formula = f;

  if (na >= 4) pc_parse_source(args[3], st);
  else         st->rule = R_INPUT;        // no source annotation = given
  pc_nstep++;
  return 1;
}

// ===================================================================
// per-rule step checks
// ===================================================================

// gather the cited parents' formulas; returns 0 if any is missing.
static int pc_parent_formulas(const PcStep *st, Term *out, int *nout) {
  int n = 0;
  for (int i = 0; i < st->n_parents; i++) {
    int idx = pc_find(st->parents[i]);
    if (idx < 0) return 0;
    if (n < PC_MAX_PARENTS) out[n++] = pc_steps[idx].formula;
  }
  *nout = n;
  return 1;
}

static int pc_check_instantiate(const PcStep *st, const char **why) {
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) { *why = "parent missing"; return PC_UNKNOWN; }
  // fast path: conclusion is a direct substitution instance of parent.
  Term pat = pc_strip_forall(par[0]);
  if (pc_max_var(pat) < REWRITE_MAX_VAR) {
    RewriteSubst sub; memset(&sub, 0, sizeof sub);
    if (thvm_match(pat, st->formula, &sub)) return PC_VALID;
  }
  // otherwise settle by entailment (an instance is a consequence).
  int v = pc_entails(par, np, st->formula);
  *why = "conclusion is not an instance of the parent";
  return v;
}

static int pc_check_negconj(const PcStep *st, const char **why) {
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) { *why = "parent missing"; return PC_UNKNOWN; }
  Term negp = fol_mk_not(par[0]);
  // fast path: syntactically the negation, up to NNF + renaming.
  if (pc_alpha_eq(fol_nnf(negp), fol_nnf(st->formula))) return PC_VALID;
  // sound path: conclusion == not(parent) iff each entails the other.
  Term d1a = negp,          d1b = fol_mk_not(st->formula);   // (~parent) & (~concl)
  Term d2a = st->formula,   d2b = par[0];                    // (concl) & (parent)
  Term f1[2] = {d1a, d1b};
  Term f2[2] = {d2a, d2b};
  AtpStatus s1 = pc_refute(f1, 2);
  AtpStatus s2 = pc_refute(f2, 2);
  int e1 = (s1 == ATP_PROVED || s1 == ATP_REFUTED);
  int e2 = (s2 == ATP_PROVED || s2 == ATP_REFUTED);
  if (e1 && e2) return PC_VALID;
  if (s1 == ATP_QUEUE_EMPTY || s2 == ATP_QUEUE_EMPTY) {
    *why = "not the negation of the conjecture";
    return PC_INVALID;
  }
  *why = "negation could not be settled";
  return PC_UNKNOWN;
}

static int pc_check_skolemize(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  if (st->new_sym[0] == '\0') { *why = "no declared Skolem symbol"; return PC_UNKNOWN; }
  u32 L = pc_lookup(st->new_sym);
  // must actually occur in the resulting formula
  if (L == PC_NOT_FOUND || !pc_has_label(st->formula, L)) {
    *why = "declared Skolem symbol absent from the result";
    return PC_INVALID;
  }
  // must be FRESH: absent from every earlier step (incl. the parent).
  for (int j = 0; j < idx; j++)
    if (pc_has_label(pc_steps[j].formula, L)) {
      *why = "Skolem symbol is not fresh (reused)";
      return PC_INVALID;
    }
  return PC_VALID;
}

static int pc_check_entail(const PcStep *st, const char **why) {
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) { *why = "parent missing"; return PC_UNKNOWN; }
  Term concl = pc_is_false(st->formula) ? 0 : st->formula;
  int v = pc_entails(par, np, concl);
  *why = "conclusion is not entailed by the parents";
  return v;
}

static int pc_check_step(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  switch (st->rule) {
    case R_INSTANTIATE: return pc_check_instantiate(st, why);
    case R_NEGCONJ:     return pc_check_negconj(st, why);
    case R_SKOLEMIZE:   return pc_check_skolemize(idx, why);
    case R_CONSEQUENCE:
    case R_DEDUCTION:
    case R_HORN:        return pc_check_entail(st, why);
    default:
      // Unknown inference names (the equational engine's own cp /
      // orient / simp records, other provers' rule vocabularies): try
      // the semantic route -- parents |= conclusion via the engine --
      // instead of abstaining outright.  Every equationally-derived
      // step is an entailment, so FindProof's own CNFRefutations now
      // verify end-to-end; an unsettled entailment still returns
      // PC_UNKNOWN (NotVerified abstain), never a false Verified.
      return pc_check_entail(st, why);
  }
}

// ===================================================================
// driver
// ===================================================================

// Strip TSTP comments (% to end of line, /* ... */) in place.
static void pc_strip_comments(char *s) {
  int inq = 0;
  for (char *p = s; *p; p++) {
    if (inq) { if (*p == '\'') inq = 0; continue; }
    if (*p == '\'') { inq = 1; continue; }
    if (*p == '%') { while (*p && *p != '\n') *p++ = ' '; if (!*p) break; }
    else if (*p == '/' && p[1] == '*') {
      *p++ = ' '; *p++ = ' ';
      while (*p && !(*p == '*' && p[1] == '/')) *p++ = ' ';
      if (*p) { *p = ' '; p[1] = ' '; p++; }
    }
  }
}

static char *pc_read(const char *file, size_t *len) {
  FILE *fp = fopen(file, "rb");
  if (fp == NULL) return NULL;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (n < 0) { fclose(fp); return NULL; }
  char *buf = (char *)malloc((size_t)n + 1);
  if (buf == NULL) { fclose(fp); return NULL; }
  size_t got = fread(buf, 1, (size_t)n, fp);
  buf[got] = '\0';
  fclose(fp);
  if (len) *len = got;
  return buf;
}

// Checking-pass accumulators.  File-scope + volatile so their values
// after a heap-exhaustion longjmp are the ones set before it (locals
// clobbered across setjmp/longjmp would be indeterminate).
static jmp_buf     pc_oom_jb;
static volatile int pc_bad;
static volatile int pc_any_unknown;
static volatile int pc_saw_false;
static const char * volatile pc_bad_why;

// The actual checking pass; prints the SZS verdict on clean completion.
// The whole pass runs under a setjmp on thvm's heap-exhaustion hook: an
// entailment whose saturation (or clausification) blows the term heap
// unwinds here and the proof abstains -- NotVerified -- rather than the
// process dying with no verdict.  Because ProoVer runs one proof per
// process, a whole-proof abstain on exhaustion is the sound response.
static int proof_check_worker(const char *file, int szs) {
  thvm_init();
  const char *cap = getenv("THVM_PROOFCHECK_CAP");
  if (cap != NULL) { long v = atol(cap); if (v > 0) pc_cap = (u32)v; }
  const char *base = strrchr(file, '/');
  base = (base != NULL) ? base + 1 : file;

  char *text = pc_read(file, NULL);
  if (text == NULL) {
    if (szs) printf("%% SZS status Error for %s\n", base);
    fprintf(stderr, "FindProof: cannot open %s\n", file);
    return 2;
  }
  pc_strip_comments(text);

  pc_bad = -1; pc_any_unknown = 0; pc_saw_false = 0; pc_bad_why = "";
  if (getenv("THVM_PROOFCHECK_DBG") != NULL) {
    fprintf(stderr, "PCDBG loaded steps=%d\n", pc_nstep);
  }
  const char *verdict;

  thvm_heap_exhausted = 0;
  thvm_heap_exhaust_jmp = &pc_oom_jb;
  if (setjmp(pc_oom_jb) != 0) {
    // Heap exhausted mid-check.  A step already confirmed unsound stays
    // a bug; otherwise abstain.
    thvm_heap_exhaust_jmp = NULL;
    verdict = (pc_bad >= 0) ? "FailedVerified" : "NotVerified";
  } else {
    // Split into '.'-terminated records (respecting quotes / brackets).
    pc_nstep = 0; pc_nsyms = 0;
    int depth = 0, inq = 0;
    char *start = text;
    for (char *p = text; *p; p++) {
      if (inq) { if (*p == '\'') inq = 0; continue; }
      if (*p == '\'') { inq = 1; continue; }
      if (*p == '(' || *p == '[') depth++;
      else if (*p == ')' || *p == ']') depth--;
      else if (*p == '.' && depth == 0) {
        char save = p[1]; p[1] = '\0';
        char rec[1 << 16];
        snprintf(rec, sizeof rec, "%.*s", (int)(p - start), start);
        pc_parse_record(rec);
        p[1] = save;
        start = p + 1;
      }
    }

    // Validate every derived step; report one verdict.
    for (int i = 0; i < pc_nstep; i++) {
      if (pc_steps[i].rule == R_INPUT) continue;   // axioms / given clauses
      const char *why = "";
      // Per-step arena discipline: pc_entails replays a whole engine
      // refutation per step on the global heap; without reclamation a
      // ~30-step equational proof exhausts from-space mid-check (LAT171-1)
      // and the isolation child dies -> blanket NotVerified.  Nothing a
      // step check allocates outlives it (verdicts are ints; pc_steps'
      // formulas predate the loop), so pop the watermark -- unless a GC
      // pass moved the heap under us, where the checkpoint is stale.
      u64 _pc_hp = thvm_atp_heap_checkpoint();
      u64 _pc_gc = gc_count();
      // Per-step OOM containment: one hard entailment replay can exhaust
      // from-space on its own (LAT171-1's lattice steps).  Catch the
      // exhaustion HERE, roll the heap back, and abstain on that step
      // only -- the rest of the proof still gets checked.
      jmp_buf _pc_step_jb;
      jmp_buf *_pc_saved = thvm_heap_exhaust_jmp;
      int v;
      thvm_heap_exhaust_jmp = &_pc_step_jb;
      if (setjmp(_pc_step_jb) != 0) {
        v = PC_UNKNOWN;
        why = "step check exhausted the term heap";
        thvm_heap_exhausted = 0;
      } else {
        v = pc_check_step(i, &why);
      }
      thvm_heap_exhaust_jmp = _pc_saved;
      if (gc_count() == _pc_gc) thvm_atp_heap_reset(_pc_hp);
      {
        static int pcdbg = -1;
        if (pcdbg < 0) pcdbg = getenv("THVM_PROOFCHECK_DBG") != NULL ? 1 : 0;
        if (pcdbg) {
          fprintf(stderr, "PCDBG step=%s rule=%d verdict=%s why=%s\n",
                  pc_steps[i].name, pc_steps[i].rule,
                  v == 0 ? "VALID" : v == 1 ? "INVALID" : "UNKNOWN", why);
        }
      }
      if (v == PC_VALID && pc_is_false(pc_steps[i].formula)) pc_saw_false = 1;
      if (v == PC_INVALID) { pc_bad = i; pc_bad_why = why; break; }
      if (v == PC_UNKNOWN) pc_any_unknown = 1;
    }
    thvm_heap_exhaust_jmp = NULL;
    if (pc_bad >= 0)          verdict = "FailedVerified";
    else if (pc_any_unknown)  verdict = "NotVerified";
    else if (!pc_saw_false)   verdict = "NotVerified";  // no refutation closure
    else                      verdict = "Verified";
  }

  if (szs) printf("%% SZS status %s for %s\n", verdict, base);
  else     printf("%s\t%s\n", verdict, base);
  if (pc_bad >= 0) printf("%% bad step: %s (%s)\n", pc_steps[pc_bad].name, pc_bad_why);
  fflush(stdout);
  return 0;
}

static void pc_alarm(int sig) { (void)sig; }   // interrupt waitpid only

// FindProof --check entry point.  Isolates the checking pass in a child
// process: whatever happens to it -- a clean verdict, a heap-exhaustion
// fatal, or a saturation that outlives the wall bound -- the proof still
// gets exactly one SZS line.  The conservative fallback is NotVerified
// (0 points), never a wrong FailedVerified (-10).
static int proof_check_main(const char *file, int szs) {
  const char *base = strrchr(file, '/');
  base = (base != NULL) ? base + 1 : file;

  unsigned wall = 25;
  const char *w = getenv("THVM_PROOFCHECK_WALL");
  if (w != NULL) { long v = atol(w); if (v > 0 && v < 100000) wall = (unsigned)v; }

  fflush(stdout);
  if (getenv("THVM_PROOFCHECK_INLINE") != NULL) {
    return proof_check_worker(file, szs);              // debug: no isolation
  }
  pid_t pid = fork();
  if (pid < 0) return proof_check_worker(file, szs);   // no fork: run inline
  if (pid == 0) _exit(proof_check_worker(file, szs));  // child prints on success

  signal(SIGALRM, pc_alarm);
  alarm(wall);
  int status = 0;
  pid_t r = waitpid(pid, &status, 0);
  alarm(0);
  int ok = (r == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  if (!ok) {
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    // The worker crashed or timed out before printing: abstain cleanly.
    if (szs) printf("%% SZS status NotVerified for %s\n", base);
    else     printf("NotVerified\t%s\n", base);
    fflush(stdout);
  }
  return 0;
}
