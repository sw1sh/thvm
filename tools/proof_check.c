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
//   VerifiedGood   every step positively confirmed         (+1 right, -10 wrong)
//   VerifiedBad    a demonstrated soundness violation       (+2 right,  -1 wrong)
//   Unknown        anything else -- ABSTAIN BY DEFAULT      ( 0 always)
//
// ProoVer's scoring is brutally asymmetric: a wrong VerifiedGOOD (a bad
// proof accepted) costs -10, ten times a wrong VerifiedBad's -1, and
// abstaining is always free.  The 2026 entry inverted this in its head
// -- it guarded the cheap mistake and left the expensive one open, and
// 18 accepted-invalid proofs (-180) sank it.  The policy is therefore
// abstain-by-default on BOTH axes:
//
//   * VerifiedGood requires EVERY record confirmed: every leaf verified
//     against the linked problem file (strict mode), every derived step
//     discharged by a sound internal check or the external-ATP hook,
//     every record PARSED (a record this checker cannot read caps the
//     verdict at Unknown -- it must never be silently skipped), unique
//     names, an acyclic citation graph, and a confirmed $false root.
//   * VerifiedBad requires a DEMONSTRATED violation: a completed
//     saturation exhibiting a counter-model of a thm/cth claim, a leaf
//     whose formula semantically differs from the problem formula it
//     cites, a Skolem symbol occurring outside its introduction cone, a
//     citation cycle, duplicate names with differing formulas.  A failed
//     heuristic or an undecided obligation is NEVER VerifiedBad.
//
// Two profiles share this file (same TU, selected by binary name in
// main.c via pc_strict_default, overridable with THVM_PROOFCHECK_STRICT):
//
//   strict (CheckProof, the ProoVer entry): leaves are VERIFIED -- the
//     file() directive's problem file is loaded and the cited formula
//     alpha-compared (a semantic mismatch is VerifiedBad; introduced()
//     and directive-less inputs abstain); unsettled thm/cth steps are
//     discharged by re-exec'ing this binary as a per-step ATP oracle
//     (the rules permit exactly this: the step and its premises only).
//   legacy (FindProof --check): file()/introduced() leaves are trusted
//     inputs -- the profile used to certify FindProof's own proofs.
//
// Obligations, all discharged through the saturation engine except the
// structural Skolem check:
//   input (strict)     leaf formula == the problem-file formula it cites
//   instantiate        parent |= conclusion          (fast path: match)
//   negated_conjecture conclusion == not(parent)      (both entailments)
//   skolemize          fresh Skolem symbol + parent entails the
//                      re-existentialised conclusion
//   consequence/horn/deduction   parents |= conclusion
//   other rules        parents |= conclusion when status is thm/cth;
//                      esa steps are never judged by entailment
//   (final)            parents |= $false  (refutation closes)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>

// --- limits ---------------------------------------------------------
// Step and problem tables grow on demand up to PC_MAX_STEPS (the
// official set carries 10006-record proofs).  PC_MAX_SYMS keeps every
// user label below the FOL connective labels (0xFFF0).  PC_MAX_VARS
// bounds one record's variables in the NAME-keyed parse the FindProof
// FOF front-end shares; the checker's binder-fresh parse (every
// quantifier binding its own id) counts binders, up to PC_MAX_RVARS.
#define PC_MAX_STEPS    262144
#define PC_MAX_PARENTS  64
#define PC_NAME         192
#define PC_MAX_SYMS     60000
#define PC_MAX_VARS     256
#define PC_MAX_RVARS    4096
#define PC_MAX_TOK      65536
#define PC_ENTAIL_CAP   1500    // saturation step cap per entailment (default)

static u32 pc_cap = PC_ENTAIL_CAP;   // runtime override via env, set in worker

// --- ProoVer strict profile ----------------------------------------
// main.c sets pc_strict_default = 1 when the binary is invoked as
// CheckProof (the competition entry); THVM_PROOFCHECK_STRICT=0/1
// overrides in either direction.  pc_self_path is argv[0], the binary
// the external-ATP hook re-execs (with THVM_CHECKPROOF_PROVE=1) as its
// per-step oracle.
static int pc_strict_default = 0;
static const char *pc_self_path = NULL;
static double pc_wall_now(void);        // defined with the ATP hook below
static double g_pc_deadline = 0.0;      // absolute wall budget; 0 = untracked
static int pc_strict(void) {
  static int v = -1;
  if (v < 0) {
    const char *e = getenv("THVM_PROOFCHECK_STRICT");
    v = (e != NULL) ? (e[0] != '\0' && e[0] != '0') : pc_strict_default;
  }
  return v;
}

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
// Open-addressing index over pc_sym (slot = symbol index + 1, 0 empty).
#define PC_SYM_HT (1u << 17)
static int pc_sym_ht[PC_SYM_HT];

static u32 pc_strhash(const char *s) {
  u32 h = 2166136261u;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h;
}
static void pc_sym_reset(void) {
  pc_nsyms = 0;
  memset(pc_sym_ht, 0, sizeof pc_sym_ht);
}
// Label of an already-seen name, or PC_NOT_FOUND (never interned).
static u32 pc_lookup(const char *name) {
  for (u32 h = pc_strhash(name) & (PC_SYM_HT - 1u);; h = (h + 1u) & (PC_SYM_HT - 1u)) {
    int s = pc_sym_ht[h];
    if (s == 0) return PC_NOT_FOUND;
    if (strcmp(pc_sym[s - 1], name) == 0) return PC_LAB_BASE + (u32)(s - 1);
  }
}
// Intern a symbol.  Table full returns PC_NOT_FOUND and the caller
// fails the record: folding distinct symbols onto one label would let
// unrelated predicates unify.
static u32 pc_intern(const char *name) {
  u32 h = pc_strhash(name) & (PC_SYM_HT - 1u);
  for (;; h = (h + 1u) & (PC_SYM_HT - 1u)) {
    int s = pc_sym_ht[h];
    if (s == 0) break;
    if (strcmp(pc_sym[s - 1], name) == 0) return PC_LAB_BASE + (u32)(s - 1);
  }
  if (pc_nsyms >= PC_MAX_SYMS) return PC_NOT_FOUND;
  snprintf(pc_sym[pc_nsyms], PC_NAME, "%s", name);
  pc_sym_ht[h] = pc_nsyms + 1;
  return PC_LAB_BASE + (u32)pc_nsyms++;
}

// ===================================================================
// tokenizer (one record at a time)
// ===================================================================

enum {
  K_IDENT, K_VAR, K_LP, K_RP, K_LB, K_RB, K_COMMA, K_COLON,
  K_ALL, K_EX, K_NOT, K_AND, K_OR, K_IMP, K_IFF, K_EQ, K_NEQ,
  K_REVIMP, K_XOR, K_NAND, K_NOR, K_STR, K_EOF
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
    else if (c == '~' && s[1] == '&') { t->kind = K_NAND; s += 2; }
    else if (c == '~' && s[1] == '|') { t->kind = K_NOR;  s += 2; }
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
      if      (s[1] == '=' && s[2] == '>') { t->kind = K_IFF;    s += 3; }
      else if (s[1] == '=')                { t->kind = K_REVIMP; s += 2; }
      else if (s[1] == '~' && s[2] == '>') { t->kind = K_XOR;    s += 3; }
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

// Variables: every quantifier binding gets a FRESH id, and an occurrence
// resolves to the innermost enclosing binder of its name (else to the
// record's free variable of that name).  Distinct binders therefore
// never share an id, so no downstream consumer -- the clausifier's
// binder-dropping Skolemizer, the matcher, the Skolem-shape check --
// can confuse a shadowed variable with its shadower (PRV001's
// ![X]..?[X] nest drew a false countermodel from exactly that).
// pc_vname keeps id -> source name for the most recent parse.
typedef struct {
  PcTok *t;
  int    pos;
  int    ok;
  int    nv;        // ids allocated in this record
  int    nscope;    // binder stack depth
  int    nfree;     // free variables seen
} PcParse;
static char pc_vname[PC_MAX_RVARS][PC_NAME];
static int  pc_scope_id[PC_MAX_RVARS];
static int  pc_free_id[PC_MAX_RVARS];
// 1 in the checker (binder-fresh ids); 0 keeps the name-keyed ids the
// FindProof FOF front-end was validated with (one id per NAME per
// record) -- the prover's behavior must not move with checker work.
static int  pc_fresh_binders = 0;

static int pc_peek(PcParse *p) { return p->t[p->pos].kind; }
static PcTok *pc_cur(PcParse *p) { return &p->t[p->pos]; }
static void pc_adv(PcParse *p) { p->pos++; }
static void pc_expect(PcParse *p, int kind) {
  if (pc_peek(p) == kind) pc_adv(p); else p->ok = 0;
}
static int pc_newvar(PcParse *p, const char *name) {
  if (p->nv >= (pc_fresh_binders ? PC_MAX_RVARS : PC_MAX_VARS)) { p->ok = 0; return 0; }
  snprintf(pc_vname[p->nv], PC_NAME, "%s", name);
  return p->nv++;
}
// Name-keyed id (the FindProof front-end's convention): one id per
// distinct name in the record, binders and occurrences alike.
static int pc_nameid(PcParse *p, const char *name) {
  for (int i = 0; i < p->nv; i++)
    if (strcmp(pc_vname[i], name) == 0) return i;
  return pc_newvar(p, name);
}
static u32 pc_varid(PcParse *p, const char *name) {
  if (!pc_fresh_binders) return (u32)pc_nameid(p, name);
  for (int i = p->nscope - 1; i >= 0; i--)
    if (strcmp(pc_vname[pc_scope_id[i]], name) == 0) return (u32)pc_scope_id[i];
  for (int i = 0; i < p->nfree; i++)
    if (strcmp(pc_vname[pc_free_id[i]], name) == 0) return (u32)pc_free_id[i];
  int id = pc_newvar(p, name);
  if (p->ok && p->nfree < PC_MAX_RVARS) pc_free_id[p->nfree++] = id;
  return (u32)id;
}

static Term pc_formula(PcParse *p);   // fwd
static Term pc_unary(PcParse *p);     // fwd

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
      u32 lab = pc_intern(name);
      if (lab == PC_NOT_FOUND) { p->ok = 0; return 0; }
      return term_new_ctr(lab, kids, n);
    }
    u32 lab = pc_intern(name);
    if (lab == PC_NOT_FOUND) { p->ok = 0; return 0; }
    return term_new_ctr(lab, NULL, 0);   // constant
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
  int first = p->nscope;
  for (;;) {
    if (pc_peek(p) != K_VAR) { p->ok = 0; break; }
    int id = pc_fresh_binders ? pc_newvar(p, pc_cur(p)->text)
                              : pc_nameid(p, pc_cur(p)->text);
    if (!p->ok || p->nscope >= PC_MAX_RVARS) { p->ok = 0; break; }
    pc_scope_id[p->nscope++] = id;
    pc_adv(p);
    if (pc_peek(p) == K_COMMA) { pc_adv(p); continue; }
    break;
  }
  pc_expect(p, K_RB);
  pc_expect(p, K_COLON);
  // TPTP: the quantified body is a UNITARY formula ('~f', a nested
  // quantification, '(...)', or an atom).  Parsing a full formula here
  // swallowed any '&'/'|' continuation into the body -- for
  //   ! [X] : (p(X)) & ! [X] : (q(X))
  // the second quantifier nested INSIDE the first, and the shadowed X
  // then skolemized into a captured binder (KRS135+1's conjecture).
  Term body = p->ok ? pc_unary(p) : 0;
  for (int i = p->nscope - 1; i >= first; i--)
    body = (q == K_ALL) ? fol_mk_all(term_new_fvr((u32)pc_scope_id[i]), body)
                        : fol_mk_ex (term_new_fvr((u32)pc_scope_id[i]), body);
  p->nscope = first;
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
// formula := or [ ('=>'|'<=>'|'<='|'<~>') formula ]   (right-associative;
// TPTP forbids unparenthesised mixing, so one binary tail suffices)
static Term pc_formula(PcParse *p) {
  Term a = pc_or(p);
  if (pc_peek(p) == K_IMP) { pc_adv(p); return fol_mk_imp(a, pc_formula(p)); }
  if (pc_peek(p) == K_IFF) { pc_adv(p); return fol_mk_iff(a, pc_formula(p)); }
  if (pc_peek(p) == K_REVIMP) { pc_adv(p); return fol_mk_imp(pc_formula(p), a); }
  if (pc_peek(p) == K_XOR) { pc_adv(p); return fol_mk_not(fol_mk_iff(a, pc_formula(p))); }
  if (pc_peek(p) == K_NAND) { pc_adv(p); return fol_mk_not(fol_mk_and(a, pc_formula(p))); }
  if (pc_peek(p) == K_NOR)  { pc_adv(p); return fol_mk_not(fol_mk_or(a, pc_formula(p))); }
  return a;
}

// Parse a whole formula string; 0 on failure.  The token buffer is
// static (single-threaded checker): PC_MAX_TOK tokens would otherwise
// be a multi-megabyte stack frame.  On success pc_vname[0..scope->nv)
// names every variable id of the result.
static Term pc_parse_formula(const char *fstr, PcParse *scope) {
  static PcTok toks[PC_MAX_TOK];
  int nt = pc_lex(fstr, toks, PC_MAX_TOK);
  if (nt < 0) return 0;
  scope->t = toks; scope->pos = 0; scope->ok = 1; scope->nv = 0;
  scope->nscope = 0; scope->nfree = 0;
  Term f = pc_formula(scope);
  if (!scope->ok || pc_peek(scope) != K_EOF) return 0;
  return f;
}
// Parse a TERM in an existing variable context: `scope_ids` are the
// binder ids in scope (outermost first; the innermost binder of a name
// wins), `free_ids` the formula's free variables; pc_vname must name
// them all.  A variable resolving to neither fails the parse.  Used for
// the skolemize(Var, sK(...)) annotation, whose argument variables
// name binders of the parent formula.
static Term pc_parse_term_ctx(const char *tstr, const int *scope_ids,
                              int nscope, const int *free_ids, int nfree) {
  static PcTok toks[4096];
  int nt = pc_lex(tstr, toks, 4096);
  if (nt < 0) return 0;
  PcParse sc;
  sc.t = toks; sc.pos = 0; sc.ok = 1;
  sc.nv = PC_MAX_RVARS;              // no new variables: unknown names fail
  sc.nscope = 0; sc.nfree = 0;
  for (int i = 0; i < nscope && i < PC_MAX_RVARS; i++) pc_scope_id[sc.nscope++] = scope_ids[i];
  for (int i = 0; i < nfree && i < PC_MAX_RVARS; i++) pc_free_id[sc.nfree++] = free_ids[i];
  Term t = pc_term(&sc);
  if (!sc.ok || pc_peek(&sc) != K_EOF) return 0;
  return t;
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
      if (id < PC_MAX_RVARS && !bound[id]) freev[id] = 1;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      if (lab == PC_LAB_ALL || lab == PC_LAB_EX) {
        Term var = term_ctr_at(t, 0);
        u32 vid = term_ext(var);
        unsigned char saved = (vid < PC_MAX_RVARS) ? bound[vid] : 0;
        if (vid < PC_MAX_RVARS) bound[vid] = 1;
        pc_free_rec(term_ctr_at(t, 1), bound, freev);
        if (vid < PC_MAX_RVARS) bound[vid] = saved;
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
  unsigned char bound[PC_MAX_RVARS], freev[PC_MAX_RVARS];
  memset(bound, 0, sizeof bound);
  memset(freev, 0, sizeof freev);
  pc_free_rec(f, bound, freev);
  Term r = f;
  for (int id = PC_MAX_RVARS - 1; id >= 0; id--)
    if (freev[id]) r = fol_mk_all(term_new_fvr((u32)id), r);
  return r;
}

// alpha-equivalence (structural equality up to a variable bijection).
static int pc_alpha_rec(Term a, Term b, int *ab, int *ba) {
  u32 ta = term_tag(a), tb = term_tag(b);
  if (ta != tb) return 0;
  if (ta == TAG_FVR) {
    int ia = (int)term_ext(a), ib = (int)term_ext(b);
    if (ia < 0 || ia >= PC_MAX_RVARS || ib < 0 || ib >= PC_MAX_RVARS) return ia == ib;
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
static u32 pc_max_var(Term t);
static int pc_alpha_eq(Term a, Term b) {
  static int ab[PC_MAX_RVARS], ba[PC_MAX_RVARS];
  u32 ma = pc_max_var(a), mb = pc_max_var(b);
  u32 m = ma > mb ? ma : mb;
  if (m > PC_MAX_RVARS) m = PC_MAX_RVARS;
  for (u32 i = 0; i < m; i++) { ab[i] = -1; ba[i] = -1; }
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

// $true / $false are logical constants, but the clausifier reads every
// non-connective CTR as an uninterpreted atom -- a $false premise was an
// ordinary satisfiable literal (PRV030's closure step, whose parent IS
// $false, drew a "countermodel").  Fold them away first: pc_simp_tf
// returns a formula free of $true/$false, or exactly the $true / $false
// constant.
static Term pc_tf(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static int pc_is_true(Term t) {
  return term_tag(t) == TAG_CTR && term_ext(t) == PC_LAB_TRUE && term_ctr_n(t) == 0;
}
static int pc_is_false0(Term t) {
  return term_tag(t) == TAG_CTR && term_ext(t) == PC_LAB_FALSE && term_ctr_n(t) == 0;
}
static Term pc_simp_tf(Term f) {
  if (term_tag(f) != TAG_CTR) return f;
  u32 lab = term_ext(f);
  switch (lab) {
    case PC_LAB_NOT: {
      Term a = pc_simp_tf(term_ctr_at(f, 0));
      if (pc_is_true(a))   return pc_tf(PC_LAB_FALSE);
      if (pc_is_false0(a)) return pc_tf(PC_LAB_TRUE);
      return a == term_ctr_at(f, 0) ? f : fol_mk_not(a);
    }
    case PC_LAB_AND: case PC_LAB_OR: case PC_LAB_IMP: case PC_LAB_IFF: {
      Term a0 = term_ctr_at(f, 0), b0 = term_ctr_at(f, 1);
      Term a = pc_simp_tf(a0), b = pc_simp_tf(b0);
      int at = pc_is_true(a), af = pc_is_false0(a);
      int bt = pc_is_true(b), bf = pc_is_false0(b);
      if (lab == PC_LAB_AND) {
        if (af || bf) return pc_tf(PC_LAB_FALSE);
        if (at) return b;
        if (bt) return a;
        return (a == a0 && b == b0) ? f : fol_mk_and(a, b);
      }
      if (lab == PC_LAB_OR) {
        if (at || bt) return pc_tf(PC_LAB_TRUE);
        if (af) return b;
        if (bf) return a;
        return (a == a0 && b == b0) ? f : fol_mk_or(a, b);
      }
      if (lab == PC_LAB_IMP) {
        if (af || bt) return pc_tf(PC_LAB_TRUE);
        if (at) return b;
        if (bf) return pc_simp_tf(fol_mk_not(a));
        return (a == a0 && b == b0) ? f : fol_mk_imp(a, b);
      }
      // IFF
      if (at) return b;
      if (bt) return a;
      if (af) return pc_simp_tf(fol_mk_not(b));
      if (bf) return pc_simp_tf(fol_mk_not(a));
      return (a == a0 && b == b0) ? f : fol_mk_iff(a, b);
    }
    case PC_LAB_ALL: case PC_LAB_EX: {
      Term v = term_ctr_at(f, 0), b0 = term_ctr_at(f, 1);
      Term b = pc_simp_tf(b0);
      // Domains are non-empty: a quantified constant is that constant.
      if (pc_is_true(b) || pc_is_false0(b)) return b;
      if (b == b0) return f;
      return (lab == PC_LAB_ALL) ? fol_mk_all(v, b) : fol_mk_ex(v, b);
    }
    default:
      return f;
  }
}

// Clause-count estimate of the distributed CNF (saturating), to route
// huge disjunctive formulas through the Tseitin transformation instead
// of the exponential distribution (equisatisfiable either way, so a
// saturation verdict keeps its meaning).
static double pc_cnf_est(Term f) {
  if (term_tag(f) != TAG_CTR) return 1.0;
  u32 lab = term_ext(f);
  if (lab == PC_LAB_AND) {
    double v = pc_cnf_est(term_ctr_at(f, 0)) + pc_cnf_est(term_ctr_at(f, 1));
    return v > 1e9 ? 1e9 : v;
  }
  if (lab == PC_LAB_OR) {
    double v = pc_cnf_est(term_ctr_at(f, 0)) * pc_cnf_est(term_ctr_at(f, 1));
    return v > 1e9 ? 1e9 : v;
  }
  return 1.0;
}

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
  // Clock bound: a single saturation at the step cap can otherwise burn
  // minutes on heavy equational inputs (the whole proof only has ~30s).
  // ABORTED reads as "undecided" everywhere, so firing the deadline can
  // only turn a would-be verdict into an abstain.
  double rem = (g_pc_deadline > 0.0) ? g_pc_deadline - pc_wall_now() : 12.0;
  double cap = rem / 4.0;
  if (cap > 3.0) cap = 3.0;
  if (cap < 0.3) return ATP_ABORTED;             // no wall left: undecided
  Term conj = 0;
  for (int i = 0; i < n; i++) {
    Term c = pc_simp_tf(pc_close(fs[i]));
    if (pc_is_false0(c)) return ATP_PROVED;      // a $false conjunct
    if (pc_is_true(c)) continue;                 // a $true conjunct
    conj = (conj == 0) ? c : fol_mk_and(conj, c);
  }
  if (conj == 0) return ATP_QUEUE_EMPTY;         // only $true: satisfiable
  u32 nc = 0;
  FolClause **cl;
  fol_reset_skolem();
  Term sk = fol_skolemize(fol_nnf(conj));
  if (pc_cnf_est(sk) <= 4096.0) {
    cl = fol_extract_clauses(fol_distribute(sk), &nc);
  } else {
    fol_reset_tseitin();
    cl = fol_tseitin_extract_clauses(sk, &nc);
  }
  if (cl == NULL) return ATP_ABORTED;
  CnfState *s = cnf_init(pc_cap);
  // Step replay must not depend on the AVATAR splitting env: the
  // checker's per-step refutations are tiny and want the plain
  // deterministic calculus.
  cnf_set_avatar(s, 0u);
  for (u32 j = 0; j < nc; j++) cnf_add_clause(s, cl[j]);
  free(cl);
  cnf_set_wall_deadline(cap);
  AtpStatus st = cnf_run(s);
  cnf_set_wall_deadline(0.0);
  cnf_free(s);
  return st;
}

enum { PC_VALID, PC_INVALID, PC_UNKNOWN };

// Plain (unquantified) equation atom PC_LAB_EQ(l, r)?
static u8 pc_plain_eq(Term f, Term *l, Term *r) {
  if (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_EQ &&
      term_ctr_n(f) == 2) {
    *l = term_ctr_at(f, 0);
    *r = term_ctr_at(f, 1);
    return 1;
  }
  return 0;
}

// Leaf-grade syntactic equivalence: plain alpha; alpha of the
// universal-prefix-stripped matrices (a permuted ALL-prefix is
// invisible to the bijection over the stripped matrices, and the
// TSTP closure of either is the same); and the side-swapped variant
// when both matrices are one (dis)equation.  Real provers reprint
// imported leaves with permuted binders and reoriented equations, so
// plain alpha alone rejects genuine imports.
static int pc_leaf_syn_eq(Term a, Term b) {
  if (pc_alpha_eq(a, b)) return 1;
  Term ma = pc_strip_forall(a), mb = pc_strip_forall(b);
  if (pc_alpha_eq(ma, mb)) return 1;
  u8 nega = 0, negb = 0;
  if (term_tag(ma) == TAG_CTR && term_ext(ma) == PC_LAB_NOT &&
      term_ctr_n(ma) == 1) { nega = 1; ma = term_ctr_at(ma, 0); }
  if (term_tag(mb) == TAG_CTR && term_ext(mb) == PC_LAB_NOT &&
      term_ctr_n(mb) == 1) { negb = 1; mb = term_ctr_at(mb, 0); }
  if (nega != negb) return 0;
  Term la, ra, lb, rb;
  if (pc_plain_eq(ma, &la, &ra) && pc_plain_eq(mb, &lb, &rb)) {
    Term sw[2] = { rb, lb };
    if (pc_alpha_eq(ma, term_new_ctr(PC_LAB_EQ, sw, 2))) return 1;
  }
  return 0;
}

// One-step rewrites of `t` by l->r at every position (both the term
// itself and its subterms); calls `emit` per reduct.  Bounded fanout.
#define PC_RW_MAX 512
typedef struct { Term out[PC_RW_MAX]; int n; } PcRwSet;
static void pc_rw_add(PcRwSet *s, Term t) {
  if (s->n < PC_RW_MAX) s->out[s->n++] = t;
}
static void pc_rewrite_all(Term t, Term l, Term r, PcRwSet *acc) {
  RewriteSubst sub = {{0}};
  if (thvm_match(l, t, &sub)) pc_rw_add(acc, thvm_subst_apply(r, &sub));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > 8u) return;
    Term kids[8];
    for (u32 i = 0; i < n; i++) kids[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      PcRwSet inner;
      inner.n = 0;
      pc_rewrite_all(kids[i], l, r, &inner);
      for (int k = 0; k < inner.n; k++) {
        Term save = kids[i];
        kids[i] = inner.out[k];
        pc_rw_add(acc, term_new_ctr(term_ext(t), kids, n));
        kids[i] = save;
      }
    }
  }
}

// Modulo-renaming pair equality: (a1,b1) == (a2,b2) up to one variable
// bijection, tried in both side orders.
static u8 pc_pair_alpha_eq(Term a1, Term b1, Term a2, Term b2) {
  Term x1 = a1, y1 = b1, x2 = a2, y2 = b2;
  thvm_normalize_vars(&x1, &y1);
  thvm_normalize_vars(&x2, &y2);
  if (kbo_eq(x1, x2) && kbo_eq(y1, y2)) return 1;
  Term s2 = b2, t2 = a2;
  thvm_normalize_vars(&s2, &t2);
  return (kbo_eq(x1, s2) && kbo_eq(y1, t2)) ? 1u : 0u;
}

// Instance check: the goal pair (gl, gr) is a substitution instance of
// the derived pair (a, b) under ONE shared match, tried in both side
// orders.  Sound: a derived equation is universally quantified, so
// every instance is entailed -- including the engine's EXTENSION-
// variable records, where a rewrite introduced a fresh rhs-only
// variable and the recorded conclusion pins it to a concrete term
// (GRP706-1 t1017296: transitivity through a shared lhs binds C to
// op_c).  Requires (a, b)'s variable ids < REWRITE_MAX_VAR.
static u8 pc_pair_instance(Term gl, Term gr, Term a, Term b) {
  RewriteSubst sub;
  memset(&sub, 0, sizeof sub);
  if (thvm_match(a, gl, &sub) && thvm_match(b, gr, &sub)) return 1;
  memset(&sub, 0, sizeof sub);
  return (thvm_match(a, gr, &sub) && thvm_match(b, gl, &sub)) ? 1u : 0u;
}

// Equational fast path: a unit-equality step (the equational engine's
// own cp / orient / simp records) is settled by COMPLETION on the
// parent equations with the conclusion as the goal -- orders of
// magnitude cheaper than the blind FO refutation below, and it returns
// a POSITIVE verdict where the FO route churns until the wall or the
// heap.  Free variables are implicitly universal on both sides, which
// is exactly the completion engine's reading.  Sound: PROVED means the
// goal joins under a completion of the parents.  Anything else falls
// through to the FO route.
// Master GC-root buffer for the per-step engine sessions: every parsed
// formula (proof steps FIRST, then the strict-mode problem-file
// formulas) plus the current step's derived pair handles.  Registered
// as gc_extra_roots on each session so a mid-session collection
// relocates them IN PLACE; pc_steps / pc_prob and the locals are
// refreshed from it after.
static Term g_pc_roots[2 * PC_MAX_STEPS + 2 * PC_MAX_PARENTS + 2];
static u8   g_pc_eq_gc_refreshed = 0;   // an eq session gc'd + refreshed
// 1 while an eq session holds g_pc_roots as its gc_extra_roots: filled
// at session start, cleared on NORMAL session exit.  If a session
// heap-exhausts (longjmp to the driver's per-step handler), the exit
// refresh is skipped -- but the collector relocated the root slots IN
// PLACE, so the driver can still pc_refresh_formulas(g_pc_roots) and
// keep checking the remaining steps instead of abstaining on the whole
// proof (the check14 failure mode: one blown deep session at record ~66
// silently ended the pass).
static u8   g_pc_roots_armed = 0;
// Prior-context window for the per-step cp-cone check (see pc_entails_eq
// / pc_check_cp): the index of the record currently under check, so a
// `cp` step's superposition is normalised only by records STRICTLY
// BEFORE it -- the demodulators that existed when the CP formed.  Sound
// + non-circular: cli_emit_trace prints parents before an entry, so the
// trace is topological, and VerifiedGood still requires every record to pass
// its own (earlier-context) check, so a bogus demodulator cannot yield a
// false VerifiedGood -- it fails its own step first.
static int  g_pc_cur_idx   = -1;
// Last-resort budget boost for the axiom-augmented completion retry: a
// treatment-compressed record can depend on a demodulator OUTSIDE the
// proof cone (GRP706-1 t769119's f/ld swap), so no syntactic route over
// prior records reproduces it -- only re-deriving from the axioms does.
// The deep session's 30s clock bail cuts that completion at a machine-
// load-dependent selection count (the run-to-run VALID/UNKNOWN flap);
// the boosted retry raises the clock cap so the DETERMINISTIC selection
// cap is the binding budget.
static u8   g_pc_eq_boost  = 0;
// Defined after the PcStep section (they touch pc_steps/pc_nstep).
static u32  pc_fill_roots(Term *buf, u32 cap);
static void pc_refresh_formulas(const Term *buf);
static u32  pc_axiom_pairs(Term *pl, Term *pr, u32 cap);
static int  pc_check_cp(Term *parents, int np, Term concl);
static int  pc_refresh_step_handles(Term *out, int np, Term *concl);

// Deep validation mode (THVM_PROOFCHECK_DEEP=1): 10x budgets; the
// default stays wall-friendly.
static int pc_deep_mode(void) {
  static int deep = -1;
  if (deep < 0) {
    const char *e = getenv("THVM_PROOFCHECK_DEEP");
    deep = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  }
  return deep;
}

// do_session == 0 runs only the syntactic settles (pair alpha/instance,
// one-step rewrite): cheap, deterministic, no heap growth.  1 adds the
// completion session.  The caller runs the syntactic pass and the
// cp-cone routes BEFORE any session: a session on a hard step can grow
// the heap to exhaustion (longjmp past the caller), which used to rob
// the deterministic routes of steps they settle instantly.
static int pc_entails_eq(Term *parents, int np, Term concl, int do_session) {
  Term gl = 0, gr = 0;
  Term pl[PC_MAX_PARENTS], pr[PC_MAX_PARENTS];
  int npl = 0;
  if (concl == 0) {
    // Refutation step ($false): admissible shape is equations plus
    // EXACTLY ONE disequation ~(l = r); the step is valid iff l and r
    // join under completion of the equations.  The one-parent
    // trivial-inequality case (l identical to r) settles immediately.
    int nneg = 0;
    for (int i = 0; i < np; i++) {
      Term f = parents[i];
      if (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_NOT &&
          term_ctr_n(f) == 1 &&
          pc_plain_eq(term_ctr_at(f, 0), &gl, &gr)) {
        nneg++;
        continue;
      }
      if (!pc_plain_eq(f, &pl[npl], &pr[npl])) return PC_UNKNOWN;
      npl++;
    }
    if (nneg != 1) return PC_UNKNOWN;
    if (kbo_eq(gl, gr)) return PC_VALID;      // x != x: contradiction
  } else if (term_tag(concl) == TAG_CTR && term_ext(concl) == PC_LAB_NOT &&
             term_ctr_n(concl) == 1) {
    // Disequation conclusion (the goal-rewriting chain: from parent
    // disequation a != b derive c != d under the equation parents).
    // Contraposition: {eqs, a != b} |= c != d  iff  {eqs, c = d} |= a = b
    // -- so add the CONCLUSION's pair as an equation and prove the
    // parent disequation's pair as the goal.
    Term cl, cr;
    if (!pc_plain_eq(term_ctr_at(concl, 0), &cl, &cr)) return PC_UNKNOWN;
    int nneg = 0;
    for (int i = 0; i < np; i++) {
      Term f = parents[i];
      if (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_NOT &&
          term_ctr_n(f) == 1 &&
          pc_plain_eq(term_ctr_at(f, 0), &gl, &gr)) {
        nneg++;
        continue;
      }
      if (!pc_plain_eq(f, &pl[npl], &pr[npl])) return PC_UNKNOWN;
      npl++;
    }
    if (nneg != 1) return PC_UNKNOWN;
    if (npl >= PC_MAX_PARENTS) return PC_UNKNOWN;
    pl[npl] = cl;
    pr[npl] = cr;
    npl++;
    if (kbo_eq(gl, gr)) return PC_VALID;
  } else {
    if (!pc_plain_eq(concl, &gl, &gr)) return PC_UNKNOWN;
    for (int i = 0; i < np; i++) {
      if (!pc_plain_eq(parents[i], &pl[i], &pr[i])) return PC_UNKNOWN;
    }
    npl = np;
  }
  // Syntactic settles first (cheap, exact):
  //   * orient / symmetry / renaming: conclusion pair == some parent
  //     pair modulo side order + variable bijection;
  //   * single rewrite: conclusion pair reachable from a parent pair by
  //     ONE application of ONE parent equation (either direction) at
  //     any position of either side -- the rw goal-chain records.
  for (int i = 0; i < npl; i++) {
    if (pc_pair_alpha_eq(gl, gr, pl[i], pr[i])) return PC_VALID;
  }
  {
    Term base_l = 0, base_r = 0;
    u8 have_base = 0;
    if (concl == 0 || (term_tag(concl) == TAG_CTR &&
                       term_ext(concl) == PC_LAB_NOT)) {
      // goal-chain shape: base pair = the parent disequation (gl, gr)
      // was set from it above only for concl==0; for the diseq
      // conclusion the roles are already (gl, gr) = parent diseq and
      // the conclusion pair rode in as an equation parent -- the
      // one-step check below still applies to every parent pair.
    }
    (void)base_l; (void)base_r; (void)have_base;
    for (int i = 0; i < npl; i++) {          // pair being transformed
      for (int j = 0; j < npl; j++) {        // equation applied
        // j == i is admissible: a `step` record can rewrite one side
        // of a pair by the pair's OWN equation (the extension-variable
        // transitivity shape, GRP706-1 t1017294: b -> a where a has a
        // variable b lacks).  The applied faces are renamed apart
        // first, so equation variables not shared with the rewritten
        // side come out FRESH (extension variables) instead of being
        // captured by same-id variables of the subject pair.
        Term lj = thvm_rename_vars(pl[j], REWRITE_MAX_VAR / 2);
        Term rj = thvm_rename_vars(pr[j], REWRITE_MAX_VAR / 2);
        PcRwSet ls, rs;
        ls.n = rs.n = 0;
        pc_rewrite_all(pl[i], lj, rj, &ls);
        pc_rewrite_all(pl[i], rj, lj, &ls);
        pc_rewrite_all(pr[i], lj, rj, &rs);
        pc_rewrite_all(pr[i], rj, lj, &rs);
        for (int k = 0; k < ls.n; k++) {
          if (pc_pair_alpha_eq(gl, gr, ls.out[k], pr[i])) return PC_VALID;
          if (pc_pair_instance(gl, gr, ls.out[k], pr[i])) return PC_VALID;
        }
        for (int k = 0; k < rs.n; k++) {
          if (pc_pair_alpha_eq(gl, gr, pl[i], rs.out[k])) return PC_VALID;
          if (pc_pair_instance(gl, gr, pl[i], rs.out[k])) return PC_VALID;
        }
      }
    }
  }
  if (!do_session) return PC_UNKNOWN;
  // KBO over the checker's label space: weight 1 everywhere, precedence
  // by label id (an arbitrary total order is admissible).
  u32 nl = PC_LAB_BASE + (u32)pc_nsyms + 1u;
  u32 *w  = (u32 *)malloc((size_t)nl * sizeof(u32));
  u32 *pr_ = (u32 *)malloc((size_t)nl * sizeof(u32));
  if (w == NULL || pr_ == NULL) { free(w); free(pr_); return PC_UNKNOWN; }
  for (u32 i = 0; i < nl; i++) { w[i] = 1u; pr_[i] = i; }
  KboConfig cfg = { w, pr_, nl, 1u };
  // Root set: all parsed formulas + this step's pair handles.
  u32 nb = pc_fill_roots(g_pc_roots, 2u * PC_MAX_STEPS);
  u32 base_pl = nb;
  for (int i = 0; i < npl; i++) {
    g_pc_roots[nb++] = pl[i];
    g_pc_roots[nb++] = pr[i];
  }
  u32 base_g = nb;
  g_pc_roots[nb++] = gl;
  g_pc_roots[nb++] = gr;
  int pc_deep = pc_deep_mode();
  u32 sel_cap = pc_deep ? 40000u : 400u;
  AtpState *s = thvm_atp_init(&cfg, sel_cap);
  int v = PC_UNKNOWN;
  u64 gc0 = gc_count();
  if (s != NULL) {
    s->gc_extra_roots = g_pc_roots;
    s->n_gc_extra_roots = nb;
    g_pc_roots_armed = 1;
    // Unfailing completion: the step theories are comm-heavy (lattice
    // equations); without E-face CPs an unorientable parent can never
    // participate and the entailment is undecidable for the session.
    thvm_atp_set_use_unfailing_cp(s, 1u);
    // Mixmost NF: goal joining must rewrite THROUGH the unorientable
    // equations (ordered E-steps), not only the oriented rules.
    thvm_atp_set_use_mixmost_nf(s, 1u);
    u8 ok = 1;
    for (int i = 0; i < npl && ok; i++) {
      // Read through the root slots: a collection during an earlier add
      // relocates them in place.
      ok = thvm_atp_add_equation(s, g_pc_roots[base_pl + 2u * (u32)i],
                                 g_pc_roots[base_pl + 2u * (u32)i + 1u]);
    }
    if (ok && thvm_atp_set_goal(s, g_pc_roots[base_g],
                                g_pc_roots[base_g + 1u])) {
      // A cp/orient/simp step's conclusion joins within a handful of
      // selections when the parents really derive it; a tight budget
      // keeps the whole-proof check inside the wall.  Heap bail: these
      // sessions share the global heap with the outer checker.
      u64 h0 = thvm_atp_heap_checkpoint();
      clock_t c0 = clock();
      AtpStatus st = ATP_RUNNING;
      long cl_cap = (pc_deep ? (g_pc_eq_boost ? 120 : 30) : 2) * CLOCKS_PER_SEC;
      // Deep sessions are GC-rooted now: let collections run instead of
      // bailing on growth (the 64MB bail was starving the budget --
      // lattice completions allocate that in under a second).
      u64 hp_cap = pc_deep ? ~0ull : (32u << 20);
      u32 k = 0;
      for (; k < sel_cap && st == ATP_RUNNING; k++) {
        st = thvm_atp_step(s);
        if ((thvm_atp_heap_checkpoint() - h0) > hp_cap) break;
        if ((clock() - c0) > cl_cap) break;
      }
      if (g_pc_eq_boost && getenv("THVM_PROOFCHECK_DBG") != NULL) {
        fprintf(stderr,
                "PCDBG boost session idx=%d npl=%d st=%d sels=%u secs=%.1f\n",
                g_pc_cur_idx, npl, (int)st, k,
                (double)(clock() - c0) / CLOCKS_PER_SEC);
      }
      if (st == ATP_PROVED) v = PC_VALID;
    }
    thvm_atp_free(s);
    g_pc_roots_armed = 0;
  }
  if (gc_count() != gc0) {
    // The session collected: the root slots hold the relocated
    // formulas -- write them back so every later step reads live Terms.
    pc_refresh_formulas(g_pc_roots);
    g_pc_eq_gc_refreshed = 1;
  }
  free(w);
  free(pr_);
  return v;
}

// parents |= concl  (concl==0 means the conclusion is $false).
static int pc_entails(Term *parents, int np, Term concl) {
  // All-equational steps: settle by completion or abstain -- the FO
  // route on a pure-equation step burns seconds to heap-exhaustion per
  // step and never answers (the LAT171-1 whole-proof wall blowout).
  {
    Term l, r;
    u8 alleq = 1;
    if (concl != 0 && !pc_plain_eq(concl, &l, &r) &&
        !(term_tag(concl) == TAG_CTR && term_ext(concl) == PC_LAB_NOT &&
          term_ctr_n(concl) == 1 &&
          pc_plain_eq(term_ctr_at(concl, 0), &l, &r))) {
      alleq = 0;
    }
    for (int i = 0; alleq && i < np; i++) {
      Term f = parents[i];
      if (pc_plain_eq(f, &l, &r)) continue;
      if (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_NOT &&
          term_ctr_n(f) == 1 && pc_plain_eq(term_ctr_at(f, 0), &l, &r)) {
        continue;                    // disequation parent: still equational
      }
      alleq = 0;
    }
    if (alleq) {
      // 1. Syntactic settles (pair alpha/instance, one-step rewrite):
      // cheap, deterministic, allocation-light.
      int v = pc_entails_eq(parents, np, concl, 0);
      if (v != PC_UNKNOWN) return v;
      // 2. Focused cp-cone routes: the 2-parent completion under-derives
      // a recorded `cp` because the pair is R-NORMALISED -- the
      // demodulators that normalise are OTHER rules.  Reproduce the
      // inference directly (superpose the 2 parents / fold treatment
      // into the parent pair, normalise with the prior records) instead
      // of re-deriving the whole theory.  These run BEFORE any engine
      // session: a session on a hard step can blow the heap
      // (longjmp past this frame) and rob them of steps they settle
      // instantly.
      if (concl != 0 && g_pc_cur_idx >= 0) {
        v = pc_check_cp(parents, np, concl);
        if (v != PC_UNKNOWN) return v;
      }
      // 3. Two-parent completion session.
      u64 eq_gc0 = gc_count();
      v = pc_entails_eq(parents, np, concl, 1);
      if (v != PC_UNKNOWN || concl == 0) return v;
      // A rooted eq-session collection relocated the heap: pc_steps was
      // refreshed through g_pc_roots (pc_refresh_formulas), but THIS
      // frame's parents/concl handles are stale from-space Terms -- the
      // axiom-augmented fallback below would read garbage (the 3
      // residual GRP706-1 abstains: every one a deep 30s/40k-selection
      // session that collected).  Re-fetch the live handles from
      // pc_steps before any further route touches them.
      Term rp[PC_MAX_PARENTS];
      if (gc_count() != eq_gc0) {
        if (!pc_refresh_step_handles(rp, np, &concl)) {
          return PC_UNKNOWN;   // cannot refresh: abstain, don't read garbage
        }
        parents = rp;
        eq_gc0  = gc_count();
      }
      // 4. Axiom-augmented fallback (sound, weaker provenance):
      // treatment rewrites inside orient records are not separately
      // recorded, and their demodulators can live OUTSIDE the proof
      // cone entirely (GRP706-1 t769119's f/ld swap) -- only
      // re-deriving from the axioms reaches those.
      Term apl[PC_MAX_PARENTS], apr[PC_MAX_PARENTS];
      u32 na = pc_axiom_pairs(apl, apr, PC_MAX_PARENTS);
      if (na > 0) {
        // pc_entails_eq's pair arrays are PC_MAX_PARENTS-sized: cap the
        // augmented list there (parents first, axioms fill the rest).
        Term aug[PC_MAX_PARENTS];
        int n2 = 0;
        for (int i = 0; i < np && n2 < PC_MAX_PARENTS; i++) {
          aug[n2++] = parents[i];
        }
        for (u32 i = 0; i < na && n2 < PC_MAX_PARENTS; i++) {
          Term k[2] = { apl[i], apr[i] };
          aug[n2++] = term_new_ctr(PC_LAB_EQ, k, 2);
        }
        v = pc_entails_eq(aug, n2, concl, 1);
        // Same staleness hazard after the second session.
        if (gc_count() != eq_gc0) {
          if (!pc_refresh_step_handles(rp, np, &concl)) {
            return v == PC_VALID ? v : PC_UNKNOWN;
          }
          parents = rp;
        }
      }
      // 5. Last resort (deep mode): the axiom-augmented completion
      // again with a raised CLOCK cap, so the deterministic selection
      // cap is the binding budget -- the plain 30s clock bail cuts the
      // t769119-class re-derivation off at a machine-load-dependent
      // selection count (the run-to-run VALID/UNKNOWN flap).
      if (v == PC_UNKNOWN && concl != 0 && pc_deep_mode()) {
        na = pc_axiom_pairs(apl, apr, PC_MAX_PARENTS);   // fresh handles
        if (na > 0) {
          Term aug[PC_MAX_PARENTS];
          int n2 = 0;
          for (int i = 0; i < np && n2 < PC_MAX_PARENTS; i++) {
            aug[n2++] = parents[i];
          }
          for (u32 i = 0; i < na && n2 < PC_MAX_PARENTS; i++) {
            Term k[2] = { apl[i], apr[i] };
            aug[n2++] = term_new_ctr(PC_LAB_EQ, k, 2);
          }
          g_pc_eq_boost = 1;
          v = pc_entails_eq(aug, n2, concl, 1);
          g_pc_eq_boost = 0;
        }
      }
      return v;
    }
  }
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
enum { PC_ST_NONE, PC_ST_THM, PC_ST_ESA, PC_ST_CTH };
enum { PC_ROLE_AXIOM, PC_ROLE_CONJ, PC_ROLE_NEGCONJ, PC_ROLE_PLAIN,
       PC_ROLE_OTHER };

#define PC_MAX_NEWSYMS 16
// new_symbols(kind, [...]) kinds.
enum { PC_NS_NONE, PC_NS_SKOLEM, PC_NS_DEFINITION, PC_NS_OTHER };

typedef struct {
  char  name[PC_NAME];
  Term  formula;
  int   rule;
  char  rule_name[PC_NAME];   // raw inference rule name, or ""
  char **parents;             // cited parent names (malloc'd)
  int  *par;                  // resolved step index per parent, -1 = none
  int   n_parents;
  u8    par_compound;         // a cited parent was not a plain name
  char  new_sym[PC_NAME];     // first declared new symbol, or ""
  char **new_syms;            // full new_symbols list (malloc'd)
  int   n_new_syms;
  int   ns_kind;              // PC_NS_* of the (first) new_symbols
  int   n_ns_ann;             // new_symbols(...) annotations seen
  int   role;                 // PC_ROLE_*
  int   status;               // PC_ST_*: the FIRST status(...) seen
  u8    st_mask;              // every status(...) seen: 1 << PC_ST_*
  u8    introduced;           // source was introduced(...)
  u8    sourceless;           // the record carries no source at all
  char  src_file[PC_NAME];    // file() directive path, or ""
  char  src_name[PC_NAME];    // file() directive formula name, or ""
  char *ftext;                // formula text (re-parsed with names)
  char *sk_var;               // skolemize(Var, T): Var, or NULL
  char *sk_term;              // skolemize(Var, T): T's text, or NULL
  int   n_sk_ann;             // skolemize(...) annotations seen
} PcStep;

static PcStep *pc_steps = NULL;
static int pc_steps_cap = 0;
static int pc_nstep = 0;
// Step-name index (open addressing; slot = step index + 1).
static int *pc_name_ht = NULL;
static u32  pc_name_ht_n = 0;
// Records the record splitter could NOT turn into a PcStep (parse
// failure, token/step overflow, non-fof/cnf content).  Any skipped
// record caps the whole-proof verdict at Unknown: a proof this checker
// cannot fully read must never be VerifiedGood -- the 2026 entry
// silently dropped such records and verified the remainder.
static int pc_nskipped = 0;
// A cited parent name that resolves to NO step: per-step abstain (the
// annotation grammar has forms this parser does not model), and it also
// disables the Skolem outside-the-cone violation (the cone may be
// incomplete).
static int pc_dangling = 0;

// --- strict-profile problem-file table ------------------------------
// The formulas of the problem file named by the proof's file()
// directives, for leaf verification.  Their Terms live on the same GC
// heap as the step formulas and are rooted through g_pc_roots (after
// the steps).
typedef struct {
  char name[PC_NAME];
  Term formula;
  int  role;
} PcProb;
static PcProb *pc_prob = NULL;
static int    pc_prob_cap = 0;
static int    pc_nprob = 0;
static int    pc_prob_unparsed = 0;   // problem records we could not read
static int    pc_prob_state = 0;      // 0 not tried, 1 loaded, -1 unavailable
static char   pc_prob_directive[PC_NAME]; // the directive path loaded
static char   pc_proof_dir[1024];     // directory of the proof file, or ""

// Collect the axiom (R_INPUT, plain-equation) pairs for the fallback
// entailment: an `orient` trace record's conclusion embodies push-side
// TREATMENT rewrites that are not separately recorded, so parents-only
// entailment can be genuinely insufficient; axioms + parent is still a
// sound (weaker-provenance) verification.
static u8 *pc_conj_pos = NULL;        // record depends positively on a conjecture
static u32 pc_axiom_pairs(Term *pl, Term *pr, u32 cap) {
  u32 n = 0;
  for (int i = 0; i < pc_nstep && n < cap; i++) {
    if (pc_steps[i].rule != R_INPUT) continue;
    // The conjecture is what the refutation must NOT assume: an
    // equational conjecture leaf here confirmed any step concluding it
    // (a bogus `cp` record deriving the goal equation, closed against
    // the negated conjecture).
    if (pc_steps[i].role == PC_ROLE_CONJ) continue;
    if (pc_conj_pos != NULL && pc_conj_pos[i]) continue;
    if (term_tag(pc_steps[i].formula) == TAG_CTR &&
        term_ext(pc_steps[i].formula) == PC_LAB_EQ &&
        term_ctr_n(pc_steps[i].formula) == 2) {
      pl[n] = term_ctr_at(pc_steps[i].formula, 0);
      pr[n] = term_ctr_at(pc_steps[i].formula, 1);
      n++;
    }
  }
  return n;
}

// Focused critical-pair check (the cp-cone route).  A recorded `cp`
// conclusion cl=cr is the SUPERPOSITION of its two cited equation
// parents, then R-NORMALISED -- so the 2-parent completion (pc_entails_eq)
// under-derives it: the demodulators that reduce the raw CP to the
// recorded (already-R-normal) sides are OTHER, lower-id rules.  Instead
// of re-completing the whole theory per step, reproduce the inference
// directly: overlap the two parents in BOTH faces, normalise each raw CP
// side with the prior proof equations (+ the two parents) KBO-oriented,
// and compare to the (also normalised) recorded pair.  Sound: a raw CP is
// a genuine equational consequence of the two parents; the demodulators
// are consequences of the axioms (each passes its own earlier-context
// check, so a bogus one fails first and blocks a false VerifiedGood); and a
// no-match is a plain abstain (PC_UNKNOWN), never a refutation.
#define PC_CP_CAP        512u
#define PC_NORM_RULES    8192u
#define PC_NORM_STEPCAP  4096u
static Term        pc_nrl[PC_NORM_RULES], pc_nrr[PC_NORM_RULES];
// Unoriented prior-equation faces for the one-E-step route (a3): every
// plain-equation parent + prior record, both sides as stored --
// including the KBO-incomparable ones the demodulator set skips.
static Term        pc_nel[PC_NORM_RULES], pc_ner[PC_NORM_RULES];
static CriticalPair pc_cp_buf[PC_CP_CAP];

// Debug term printer (THVM_PROOFCHECK_DBG diagnostics): symbol names
// from the intern table, variables as V<id>.
static void pc_dbg_term(FILE *fp, Term t) {
  if (term_tag(t) == TAG_FVR) { fprintf(fp, "V%u", (unsigned)term_ext(t)); return; }
  if (term_tag(t) != TAG_CTR) { fputc('?', fp); return; }
  u32 lab = term_ext(t);
  if (lab >= PC_LAB_BASE && lab < PC_LAB_BASE + (u32)pc_nsyms)
    fprintf(fp, "%s", pc_sym[lab - PC_LAB_BASE]);
  else fprintf(fp, "L%u", (unsigned)lab);
  u32 n = term_ctr_n(t);
  if (n == 0) return;
  fputc('(', fp);
  for (u32 i = 0; i < n; i++) {
    if (i) fputc(',', fp);
    pc_dbg_term(fp, term_ctr_at(t, i));
  }
  fputc(')', fp);
}

// Record the largest arity seen for each CTR label into ar[label] (the
// KBO precedence in pc_check_cp ranks lower-arity symbols higher).
static void pc_scan_arity(Term t, u8 *ar, u32 nl) {
  if (term_tag(t) != TAG_CTR) return;
  u32 lab = term_ext(t), n = term_ctr_n(t);
  if (lab < nl && n > ar[lab]) ar[lab] = (u8)(n > 255u ? 255u : n);
  for (u32 i = 0; i < n; i++) pc_scan_arity(term_ctr_at(t, i), ar, nl);
}
// One full route battery under ONE KBO precedence.  engine_prec == 1
// mirrors the ENGINE's term order exactly -- tp_emit_pr's convention,
// weight 1 everywhere, precedence sorted by (arity ascending, name
// ascending) with the first symbol HIGHEST (GRP706-1:
// a>b>c>op_c>f>ld>mult>rd) -- so prior records orient as the engine
// oriented them and normal forms reproduce the recorded (R-normal)
// sides.  engine_prec == 0 is the arity-first / intern-order tie-break
// (a second, independent admissible order; some joins land only here).
static int pc_topo;                   // defined with the structure pass
static u8 *pc_anc_mark = NULL;        // ancestors of the step under check
static void pc_anc_build(int idx) {
  free(pc_anc_mark);
  pc_anc_mark = (u8 *)calloc((size_t)pc_nstep + 1, 1);
  int *stk = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  if (pc_anc_mark == NULL || stk == NULL || idx < 0 || idx >= pc_nstep) {
    free(stk);
    return;
  }
  int sp = 0;
  stk[sp++] = idx;
  while (sp > 0) {
    int n = stk[--sp];
    for (int k = 0; k < pc_steps[n].n_parents; k++) {
      int p = pc_steps[n].par[k];
      if (p >= 0 && !pc_anc_mark[p]) { pc_anc_mark[p] = 1; stk[sp++] = p; }
    }
  }
  free(stk);
}
static int pc_check_cp_cfg(Term *parents, int np, Term cl, Term cr,
                           int engine_prec) {
  u32 nl = PC_LAB_BASE + (u32)pc_nsyms + 1u;
  u32 *w  = (u32 *)malloc((size_t)nl * sizeof(u32));
  u32 *pc = (u32 *)malloc((size_t)nl * sizeof(u32));
  u8  *ar = (u8 *)calloc((size_t)nl, 1u);
  if (w == NULL || pc == NULL || ar == NULL) {
    free(w); free(pc); free(ar); return PC_UNKNOWN; }
  for (int i = 0; i < pc_nstep; i++) pc_scan_arity(pc_steps[i].formula, ar, nl);
  for (u32 i = 0; i < nl; i++) w[i] = 1u;
  if (engine_prec) {
    // rank[i] = how many user symbols sort strictly before symbol i in
    // (arity asc, name asc); first-sorted = HIGHEST precedence.
    for (u32 i = 0; i < nl; i++) pc[i] = 0u;   // non-user labels: lowest
    for (int i = 0; i < pc_nsyms; i++) {
      u32 li = PC_LAB_BASE + (u32)i;
      u32 rank = 0;
      for (int j = 0; j < pc_nsyms; j++) {
        if (j == i) continue;
        u32 lj = PC_LAB_BASE + (u32)j;
        if (ar[lj] != ar[li] ? ar[lj] < ar[li]
                             : strcmp(pc_sym[j], pc_sym[i]) < 0) rank++;
      }
      pc[li] = (u32)pc_nsyms - rank;           // earlier sort = bigger
    }
  } else {
    for (u32 i = 0; i < nl; i++) {
      // higher precedence value == bigger symbol; lower arity -> bigger.
      pc[i] = ((u32)(255u - ar[i]) << 16) | (i & 0xffffu);
    }
  }
  KboConfig cfg = { w, pc, nl, 1u };
  // Demodulator set: the cited parents, then every prior plain-equation
  // record, each oriented larger->smaller (skip KBO-incomparable so the
  // rewrite terminates).  The first two plain-equation parents are also
  // kept for the superposition route.
  u32 nn = 0, ne = 0;
  Term sl[2] = { 0, 0 }, sr[2] = { 0, 0 };
  int nsup = 0;
  for (int e = 0; e < np && nn < PC_NORM_RULES; e++) {
    Term a, b;
    if (!pc_plain_eq(parents[e], &a, &b)) continue;
    if (nsup < 2) { sl[nsup] = a; sr[nsup] = b; nsup++; }
    if (ne < PC_NORM_RULES) { pc_nel[ne] = a; pc_ner[ne] = b; ne++; }
    KboCmp c = thvm_kbo(a, b, &cfg);
    if      (c == KBO_GT) { pc_nrl[nn] = a; pc_nrr[nn] = b; nn++; }
    else if (c == KBO_LT) { pc_nrl[nn] = b; pc_nrr[nn] = a; nn++; }
  }
  for (int i = 0; i < g_pc_cur_idx && i < pc_nstep && nn < PC_NORM_RULES; i++) {
    Term a, b;
    // File-prior records are demodulators only when the file order is a
    // well-founded order of BOTH dependency kinds (every citation points
    // back).  Otherwise an earlier record may descend from this one and
    // the two would confirm each other circularly: use ancestors only.
    if (!pc_topo && (pc_anc_mark == NULL || !pc_anc_mark[i])) continue;
    // Nor may a demodulator carry the conjecture itself (or anything
    // derived from it without negation): it is not a premise.
    if (pc_conj_pos != NULL && pc_conj_pos[i]) continue;
    if (!pc_plain_eq(g_pc_roots[i], &a, &b)) continue;   // rooted formula
    if (ne < PC_NORM_RULES) { pc_nel[ne] = a; pc_ner[ne] = b; ne++; }
    KboCmp c = thvm_kbo(a, b, &cfg);
    if      (c == KBO_GT) { pc_nrl[nn] = a; pc_nrr[nn] = b; nn++; }
    else if (c == KBO_LT) { pc_nrl[nn] = b; pc_nrr[nn] = a; nn++; }
  }
  // Recorded pair, normalised into the checker's rewrite system.
  Term ncl = thvm_rewrite_normalize(cl, pc_nrl, pc_nrr, nn, PC_NORM_STEPCAP);
  Term ncr = thvm_rewrite_normalize(cr, pc_nrl, pc_nrr, nn, PC_NORM_STEPCAP);
  int v = PC_UNKNOWN;
  // (a) Demodulation route (orient / simp / norm-step records + any cp
  // whose sides already join): the conclusion sides reduce to one common
  // form under the parents + prior equations.
  if (kbo_eq(ncl, ncr)) v = PC_VALID;
  // (a2) Treated orient / step route: the record's pair is its parent
  // pair with TREATMENT rewrites folded into either side (the engine
  // normalises a new rule against R before storing it; those
  // demodulation steps cite no separate records -- GRP706-1 t21951's
  // lhs is its parent's lhs pushed through the prior t142 equation).
  // Normalise the parent pair into the same system and compare:
  // alpha for the plain case, instance for extension variables.
  if (v == PC_UNKNOWN) {
    static int dbgcp = -1;
    if (dbgcp < 0) dbgcp = getenv("THVM_PROOFCHECK_DBGCP") != NULL ? 1 : 0;
    for (int e = 0; e < np; e++) {
      Term a, b;
      if (!pc_plain_eq(parents[e], &a, &b)) continue;
      Term na_ = thvm_rewrite_normalize(a, pc_nrl, pc_nrr, nn, PC_NORM_STEPCAP);
      Term nb_ = thvm_rewrite_normalize(b, pc_nrl, pc_nrr, nn, PC_NORM_STEPCAP);
      if (dbgcp) {
        fprintf(stderr, "PCDBGCP idx=%d eng=%d nn=%u parent=%d\n  ncl=",
                g_pc_cur_idx, engine_prec, nn, e);
        pc_dbg_term(stderr, ncl); fprintf(stderr, "\n  ncr=");
        pc_dbg_term(stderr, ncr); fprintf(stderr, "\n  na =");
        pc_dbg_term(stderr, na_); fprintf(stderr, "\n  nb =");
        pc_dbg_term(stderr, nb_); fprintf(stderr, "\n");
        const char *only = getenv("THVM_PROOFCHECK_DBGCP_RULES");
        if (only != NULL && atoi(only) == g_pc_cur_idx && e == 0) {
          fprintf(stderr, "PCDBGCP syms nl=%u:", nl);
          for (int q = 0; q < pc_nsyms && q < 12; q++)
            fprintf(stderr, " %s(lab=%u,ar=%u,pc=%u)", pc_sym[q],
                    PC_LAB_BASE + (u32)q, ar[PC_LAB_BASE + q],
                    pc[PC_LAB_BASE + q]);
          fprintf(stderr, "\n");
          for (u32 q = 0; q < nn; q++) {
            fprintf(stderr, "PCDBGCP rule %u: ", q);
            pc_dbg_term(stderr, pc_nrl[q]); fprintf(stderr, " -> ");
            pc_dbg_term(stderr, pc_nrr[q]); fprintf(stderr, "\n");
          }
        }
      }
      if (pc_pair_alpha_eq(ncl, ncr, na_, nb_) ||
          pc_pair_instance(ncl, ncr, na_, nb_)) {
        v = PC_VALID;
        break;
      }
    }
  }
  // (b) Superposition route (cp records): overlap the two parents in both
  // faces and match a normalised raw CP against the recorded pair --
  // alpha, or instance when the engine pinned an extension variable.
  if (v == PC_UNKNOWN && nsup == 2) {
    Term rl4[4] = { sl[0], sr[0], sl[1], sr[1] };
    Term rr4[4] = { sr[0], sl[0], sr[1], sl[1] };
    u32 ncp = thvm_critical_pairs(rl4, rr4, 4u, pc_cp_buf, PC_CP_CAP);
    for (u32 k = 0; k < ncp; k++) {
      Term a = thvm_rewrite_normalize(pc_cp_buf[k].lhs, pc_nrl, pc_nrr, nn,
                                      PC_NORM_STEPCAP);
      Term b = thvm_rewrite_normalize(pc_cp_buf[k].rhs, pc_nrl, pc_nrr, nn,
                                      PC_NORM_STEPCAP);
      if (pc_pair_alpha_eq(ncl, ncr, a, b) ||
          pc_pair_instance(ncl, ncr, a, b)) { v = PC_VALID; break; }
    }
  }
  // (a3) E-step beam + join (LAST -- the widest and costliest route): a
  // treated side can be STUCK as a checker normal form when the engine's
  // rewrite ran a prior equation in the direction this KBO orients the
  // other way (GRP706-1 t21951: t142's engine face is irreducible under
  // the intern-order tie-break).  From each conclusion side, apply prior
  // equations -- both faces, every position, bare-variable patterns
  // excluded -- one at a time, re-normalising after each; two rounds of
  // a small deduplicated beam.  The sides join when the beams meet.
  // Sound: every beam member is reached from a recorded conclusion side
  // by equational replacements with PRIOR records (each independently
  // checked) plus R-normalisation -- pure congruence.
  if (v == PC_UNKNOWN) {
#define PC_EBEAM 48
    Term seta[PC_EBEAM], setb[PC_EBEAM];
    u32 nA = 0, nB = 0;
    seta[nA++] = ncl;
    setb[nB++] = ncr;
    u32 expA = 0, expB = 0;              // expansion watermarks
    for (int round = 0; round < 2 && v == PC_UNKNOWN; round++) {
      u32 topA = nA, topB = nB;
      for (u32 s = expA; s < topA && v == PC_UNKNOWN; s++) {
        for (u32 q = 0; q < ne && v == PC_UNKNOWN; q++) {
          for (int face = 0; face < 2 && v == PC_UNKNOWN; face++) {
            Term fl = face == 0 ? pc_nel[q] : pc_ner[q];
            Term fr = face == 0 ? pc_ner[q] : pc_nel[q];
            if (term_tag(fl) != TAG_CTR) continue;  // bare-var: skip
            PcRwSet es;
            es.n = 0;
            pc_rewrite_all(seta[s], fl, fr, &es);
            for (int k = 0; k < es.n && v == PC_UNKNOWN; k++) {
              Term nf = thvm_rewrite_normalize(es.out[k], pc_nrl, pc_nrr,
                                               nn, PC_NORM_STEPCAP);
              u8 dup = 0;
              for (u32 d = 0; d < nA && !dup; d++) dup = kbo_eq(nf, seta[d]);
              if (dup) continue;
              for (u32 d = 0; d < nB; d++) {
                if (kbo_eq(nf, setb[d])) { v = PC_VALID; break; }
              }
              if (nA < PC_EBEAM) seta[nA++] = nf;
            }
          }
        }
      }
      for (u32 s = expB; s < topB && v == PC_UNKNOWN; s++) {
        for (u32 q = 0; q < ne && v == PC_UNKNOWN; q++) {
          for (int face = 0; face < 2 && v == PC_UNKNOWN; face++) {
            Term fl = face == 0 ? pc_nel[q] : pc_ner[q];
            Term fr = face == 0 ? pc_ner[q] : pc_nel[q];
            if (term_tag(fl) != TAG_CTR) continue;  // bare-var: skip
            PcRwSet es;
            es.n = 0;
            pc_rewrite_all(setb[s], fl, fr, &es);
            for (int k = 0; k < es.n && v == PC_UNKNOWN; k++) {
              Term nf = thvm_rewrite_normalize(es.out[k], pc_nrl, pc_nrr,
                                               nn, PC_NORM_STEPCAP);
              u8 dup = 0;
              for (u32 d = 0; d < nB && !dup; d++) dup = kbo_eq(nf, setb[d]);
              if (dup) continue;
              for (u32 d = 0; d < nA; d++) {
                if (kbo_eq(nf, seta[d])) { v = PC_VALID; break; }
              }
              if (nB < PC_EBEAM) setb[nB++] = nf;
            }
          }
        }
      }
      expA = topA;
      expB = topB;
    }
  }
  free(w); free(pc); free(ar);
  return v;
}

// Focused critical-pair check dispatcher: run the route battery under
// the ENGINE-mirror precedence first (most records are engine-R-normal,
// so their normal forms reproduce there), then under the arity-first /
// intern-order precedence (an independent admissible order that closes
// some joins the mirror misses).  Both orders are sound -- every route
// only ever certifies "recorded conclusion follows equationally from
// the cited parents + PRIOR records".
static int pc_check_cp(Term *parents, int np, Term concl) {
  if (concl == 0 || np < 1) return PC_UNKNOWN;
  Term cl, cr;
  if (!pc_plain_eq(concl, &cl, &cr)) return PC_UNKNOWN;
  if (!pc_topo) pc_anc_build(g_pc_cur_idx);
  int v = pc_check_cp_cfg(parents, np, cl, cr, 1);
  if (v == PC_UNKNOWN) v = pc_check_cp_cfg(parents, np, cl, cr, 0);
  return v;
}

// Step formulas first (pc_check_cp indexes the step region by record
// index), then the strict-profile problem-file formulas.
static u32 pc_fill_roots(Term *buf, u32 cap) {
  u32 nb = 0;
  for (int i = 0; i < pc_nstep && nb < cap; i++) {
    buf[nb++] = pc_steps[i].formula;
  }
  for (int i = 0; i < pc_nprob && nb < cap; i++) {
    buf[nb++] = pc_prob[i].formula;
  }
  return nb;
}
static void pc_refresh_formulas(const Term *buf) {
  u32 nb = 0;
  for (int i = 0; i < pc_nstep && i < PC_MAX_STEPS; i++) {
    pc_steps[i].formula = buf[nb++];
  }
  for (int i = 0; i < pc_nprob && i < PC_MAX_STEPS; i++) {
    pc_prob[i].formula = buf[nb++];
  }
}

static int pc_rule_of(const char *r) {
  if (strcmp(r, "instantiate") == 0)        return R_INSTANTIATE;
  if (strcmp(r, "negated_conjecture") == 0) return R_NEGCONJ;
  if (strcmp(r, "skolemize") == 0)          return R_SKOLEMIZE;
  if (strcmp(r, "consequence") == 0)        return R_CONSEQUENCE;
  if (strcmp(r, "deduction") == 0)          return R_DEDUCTION;
  if (strcmp(r, "horn") == 0)               return R_HORN;
  return R_UNKNOWN;
}

// Step lookup by name through pc_name_ht (built by pc_index_steps once
// parsing is complete; before that, a linear scan).
static int pc_find(const char *name) {
  if (pc_name_ht == NULL) {
    for (int i = 0; i < pc_nstep; i++)
      if (strcmp(pc_steps[i].name, name) == 0) return i;
    return -1;
  }
  u32 m = pc_name_ht_n - 1u;
  for (u32 h = pc_strhash(name) & m;; h = (h + 1u) & m) {
    int s = pc_name_ht[h];
    if (s == 0) return -1;
    if (strcmp(pc_steps[s - 1].name, name) == 0) return s - 1;
  }
}

// Index step names (FIRST record of a name wins, matching the old
// linear scan) and resolve every citation to a step index.
static void pc_index_steps(void) {
  free(pc_name_ht);
  u32 n = 1024u;
  while (n < 2u * (u32)pc_nstep + 16u) n <<= 1;
  pc_name_ht = (int *)calloc(n, sizeof(int));
  pc_name_ht_n = n;
  if (pc_name_ht == NULL) { pc_name_ht_n = 0; }
  for (int i = 0; pc_name_ht != NULL && i < pc_nstep; i++) {
    u32 m = n - 1u;
    for (u32 h = pc_strhash(pc_steps[i].name) & m;; h = (h + 1u) & m) {
      int s = pc_name_ht[h];
      if (s == 0) { pc_name_ht[h] = i + 1; break; }
      if (strcmp(pc_steps[s - 1].name, pc_steps[i].name) == 0) break;
    }
  }
  for (int i = 0; i < pc_nstep; i++) {
    PcStep *st = &pc_steps[i];
    for (int k = 0; k < st->n_parents; k++) st->par[k] = pc_find(st->parents[k]);
  }
}

// ---- TSTP source annotations ----------------------------------------
// A small general-term parser over the raw source text:
//   gterm := word [ '(' gterm {',' gterm} ')' ] [ ':' gterm ]
//          | '[' [ gterm {',' gterm} ] ']' | VAR | number | 'quoted'
//          | "distinct"
// Each node keeps its source span so annotation sub-terms (the Skolem
// term of skolemize(Var, T)) can be re-parsed as formulas' terms.
#define PC_GT_MAXKIDS 256
typedef struct PcGt {
  char functor[PC_NAME];      // "" for a list
  u8   is_list;
  struct PcGt **kid;
  int  nkid;
  const char *b, *e;          // source span [b, e)
} PcGt;

static void pc_gt_free(PcGt *g) {
  if (g == NULL) return;
  for (int i = 0; i < g->nkid; i++) pc_gt_free(g->kid[i]);
  free(g->kid);
  free(g);
}
static const char *pc_gt_ws(const char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  return s;
}
static PcGt *pc_gt_parse(const char **ps, int depth);
static int pc_gt_push(PcGt *g, PcGt *k) {
  if (g->nkid >= PC_GT_MAXKIDS) return 0;
  if ((g->nkid & 7) == 0) {
    PcGt **nk = (PcGt **)realloc(g->kid, (size_t)(g->nkid + 8) * sizeof(PcGt *));
    if (nk == NULL) return 0;
    g->kid = nk;
  }
  g->kid[g->nkid++] = k;
  return 1;
}
// Parse `sep`-separated gterms up to `close`; 0 on malformed input.
static int pc_gt_args(PcGt *g, const char **ps, char close, int depth) {
  const char *s = pc_gt_ws(*ps);
  if (*s == close) { *ps = s + 1; return 1; }
  for (;;) {
    PcGt *k = pc_gt_parse(&s, depth + 1);
    if (k == NULL) return 0;
    if (!pc_gt_push(g, k)) { pc_gt_free(k); return 0; }
    s = pc_gt_ws(s);
    if (*s == ',') { s++; continue; }
    if (*s == close) { *ps = s + 1; return 1; }
    return 0;
  }
}
static PcGt *pc_gt_parse(const char **ps, int depth) {
  if (depth > 200) return NULL;
  const char *s = pc_gt_ws(*ps);
  PcGt *g = (PcGt *)calloc(1, sizeof(PcGt));
  if (g == NULL) return NULL;
  g->b = s;
  if (*s == '[') {
    g->is_list = 1;
    s++;
    if (!pc_gt_args(g, &s, ']', depth)) { pc_gt_free(g); return NULL; }
  } else {
    int k = 0;
    if (*s == '\'' || *s == '"') {
      char q = *s++;
      while (*s && *s != q) {
        if (*s == '\\' && s[1]) s++;
        if (k < PC_NAME - 1) g->functor[k++] = *s;
        s++;
      }
      if (*s != q) { pc_gt_free(g); return NULL; }
      s++;
    } else {
      while (*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '$' ||
                    *s == '.' || *s == '-' || *s == '+')) {
        if (k < PC_NAME - 1) g->functor[k++] = *s;
        s++;
      }
      if (k == 0) { pc_gt_free(g); return NULL; }
    }
    g->functor[k] = '\0';
    s = pc_gt_ws(s);
    if (*s == '(') {
      s++;
      if (!pc_gt_args(g, &s, ')', depth)) { pc_gt_free(g); return NULL; }
    }
  }
  s = pc_gt_ws(s);
  if (*s == ':' && !(s[1] == '=')) {   // name:details (parent_info)
    s++;
    PcGt *d = pc_gt_parse(&s, depth + 1);
    if (d == NULL) { pc_gt_free(g); return NULL; }
    pc_gt_free(d);                      // details carry no citation
  }
  g->e = s;
  *ps = s;
  return g;
}
static char *pc_span_dup(const char *b, const char *e) {
  size_t n = (size_t)(e - b);
  char *r = (char *)malloc(n + 1);
  if (r == NULL) return NULL;
  memcpy(r, b, n);
  r[n] = '\0';
  return r;
}
static void pc_add_parent(PcStep *st, const char *nm) {
  if (st->n_parents >= PC_MAX_PARENTS) { st->par_compound = 1; return; }
  if ((st->n_parents & 7) == 0) {
    char **np = (char **)realloc(st->parents, (size_t)(st->n_parents + 8) * sizeof(char *));
    int *pi = (int *)realloc(st->par, (size_t)(st->n_parents + 8) * sizeof(int));
    if (np == NULL || pi == NULL) {
      if (np) st->parents = np;
      if (pi) st->par = pi;
      st->par_compound = 1;
      return;
    }
    st->parents = np;
    st->par = pi;
  }
  st->parents[st->n_parents] = strdup(nm);
  st->par[st->n_parents] = -1;
  st->n_parents++;
}
// new_symbols(kind, [s1, ...]) -> st.  A second annotation is counted
// (n_ns_ann) but its symbols are appended too.
static void pc_gt_new_symbols(const PcGt *g, PcStep *st) {
  if (g->nkid < 2) return;
  st->n_ns_ann++;
  const char *kind = g->kid[0]->functor;
  int k = strcmp(kind, "skolem") == 0 ? PC_NS_SKOLEM
        : strcmp(kind, "definition") == 0 ? PC_NS_DEFINITION : PC_NS_OTHER;
  if (st->ns_kind == PC_NS_NONE) st->ns_kind = k;
  const PcGt *l = g->kid[1];
  if (!l->is_list) return;
  for (int i = 0; i < l->nkid; i++) {
    if (l->kid[i]->is_list || l->kid[i]->nkid != 0) continue;
    if (st->n_new_syms >= PC_MAX_NEWSYMS) break;
    if ((st->n_new_syms & 3) == 0) {
      char **ns = (char **)realloc(st->new_syms, (size_t)(st->n_new_syms + 4) * sizeof(char *));
      if (ns == NULL) break;
      st->new_syms = ns;
    }
    st->new_syms[st->n_new_syms++] = strdup(l->kid[i]->functor);
  }
  if (st->n_new_syms > 0 && st->new_sym[0] == '\0')
    snprintf(st->new_sym, PC_NAME, "%s", st->new_syms[0]);
}
// Useful-info list items: status(...), new_symbols(...), skolemize(...).
static void pc_gt_info(const PcGt *info, PcStep *st) {
  if (info == NULL || !info->is_list) return;
  for (int i = 0; i < info->nkid; i++) {
    const PcGt *it = info->kid[i];
    if (it->is_list) continue;
    if (strcmp(it->functor, "status") == 0 && it->nkid == 1) {
      const char *s = it->kid[0]->functor;
      int v = strcmp(s, "thm") == 0 ? PC_ST_THM
            : strcmp(s, "esa") == 0 ? PC_ST_ESA
            : strcmp(s, "cth") == 0 ? PC_ST_CTH : -1;
      if (v < 0) { st->st_mask |= (u8)(1u << 7); continue; }  // other status
      if (st->status == PC_ST_NONE) st->status = v;
      st->st_mask |= (u8)(1u << v);
    } else if (strcmp(it->functor, "new_symbols") == 0) {
      pc_gt_new_symbols(it, st);
    } else if (strcmp(it->functor, "skolemize") == 0) {
      st->n_sk_ann++;
      if (it->nkid == 2 && st->sk_var == NULL) {
        st->sk_var = pc_span_dup(it->kid[0]->b, it->kid[0]->e);
        st->sk_term = pc_span_dup(it->kid[1]->b, it->kid[1]->e);
        if (st->sk_var != NULL) {          // trim trailing blanks
          size_t n = strlen(st->sk_var);
          while (n > 0 && isspace((unsigned char)st->sk_var[n - 1])) st->sk_var[--n] = '\0';
        }
      }
    }
  }
}

// Fill rule / parents / status / new_symbols / skolemize / file-directive
// fields from the record's source term (file(...), inference(...),
// introduced(...)).  An unreadable source leaves rule = R_UNKNOWN with
// no parents and par_compound set, which can only abstain.
static void pc_parse_source(const char *src, PcStep *st) {
  st->rule = R_UNKNOWN;
  st->n_parents = 0;
  st->new_sym[0] = '\0';
  const char *s = src;
  PcGt *g = pc_gt_parse(&s, 0);
  if (g == NULL || *pc_gt_ws(s) != '\0') {
    pc_gt_free(g);
    st->par_compound = 1;
    return;
  }
  if (strcmp(g->functor, "file") == 0 && g->nkid >= 1) {
    // file('Problems/x.p', name): record path + cited name so the
    // strict profile can verify the leaf against the problem file.
    st->rule = R_INPUT;
    snprintf(st->src_file, PC_NAME, "%s", g->kid[0]->functor);
    if (g->nkid >= 2 && !g->kid[1]->is_list && g->kid[1]->nkid == 0)
      snprintf(st->src_name, PC_NAME, "%s", g->kid[1]->functor);
    pc_gt_free(g);
    return;
  }
  // introduced(definition): a conservative-extension leaf (goal-flatten /
  // tgt subterm naming -- fresh symbol defined equal to a term).  The
  // legacy profile trusts it as an input of the ENCODED problem exactly
  // like file(); the strict profile abstains unless the definition
  // SHAPE is positively confirmed (pc_check_definition) -- an arbitrary
  // formula could be smuggled this way (a confirmed -180 hole).  The
  // declared new_symbols list is parsed for that check.
  if (strcmp(g->functor, "introduced") == 0) {
    st->rule = R_INPUT;
    st->introduced = 1;
    for (int i = 1; i < g->nkid; i++) {
      const PcGt *l = g->kid[i];
      if (!l->is_list) continue;
      for (int j = 0; j < l->nkid; j++)
        if (!l->kid[j]->is_list && strcmp(l->kid[j]->functor, "new_symbols") == 0)
          pc_gt_new_symbols(l->kid[j], st);
    }
    pc_gt_free(g);
    return;
  }
  if (strcmp(g->functor, "inference") != 0 || g->nkid < 3) {
    st->par_compound = 1;
    pc_gt_free(g);
    return;
  }
  snprintf(st->rule_name, PC_NAME, "%s", g->kid[0]->functor);
  st->rule = pc_rule_of(st->rule_name);
  // status(thm|esa|cth): the step's semantic claim(s); new_symbols;
  // the skolemize(Var, T) witness annotation.
  pc_gt_info(g->kid[1], st);
  // Parents: the THIRD argument.  A compound parent (a nested inference,
  // an unnamed intermediate) cannot be resolved to a record: flag it so
  // countermodels over the partial premise set never become verdicts.
  // theory(equality) cites built-in equality, which every route has.
  const PcGt *pl = g->kid[2];
  if (!pl->is_list) {
    st->par_compound = 1;
  } else {
    for (int i = 0; i < pl->nkid; i++) {
      const PcGt *p = pl->kid[i];
      if (p->is_list) { st->par_compound = 1; continue; }
      if (p->nkid == 0) { pc_add_parent(st, p->functor); continue; }
      if (strcmp(p->functor, "theory") == 0) continue;
      st->par_compound = 1;
    }
  }
  pc_gt_free(g);
}

// Split fof(...)/cnf(...) inner content on TOP-LEVEL commas (respecting
// (), [] and quotes).  Each returned args[i] is heap-allocated (caller
// frees) and carries the FULL argument -- the old fixed 8K buffers
// silently truncated large formulas, which corrupted the record instead
// of failing it.  Returns the argument count (clamped to max).
static int pc_split_args(const char *s, char **args, int max) {
  int depth = 0, inq = 0, na = 0;
  const char *seg = s;
  const char *p = s;
  for (;; p++) {
    char c = *p;
    if (c != '\0') {
      if (inq) { if (c == '\'') inq = 0; continue; }
      if (c == '\'') { inq = 1; continue; }
      if (c == '(' || c == '[') { depth++; continue; }
      if (c == ')' || c == ']') { depth--; continue; }
      if (c != ',' || depth != 0) continue;
    }
    if (na < max) {
      size_t ln = (size_t)(p - seg);
      char *b = (char *)malloc(ln + 1);
      if (b == NULL) return na;      // short count reads as parse failure
      memcpy(b, seg, ln);
      b[ln] = '\0';
      args[na] = b;
    }
    na++;
    if (c == '\0') break;
    seg = p + 1;
  }
  return na > max ? max : na;
}

static void pc_trim(char *s) {
  int n = (int)strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
  int i = 0; while (s[i] && isspace((unsigned char)s[i])) i++;
  if (i) memmove(s, s + i, strlen(s + i) + 1);
}

static int pc_role_of(const char *r) {
  if (strcmp(r, "conjecture") == 0)         return PC_ROLE_CONJ;
  if (strcmp(r, "axiom") == 0)              return PC_ROLE_AXIOM;
  if (strcmp(r, "negated_conjecture") == 0) return PC_ROLE_NEGCONJ;
  if (strcmp(r, "plain") == 0)              return PC_ROLE_PLAIN;
  return PC_ROLE_OTHER;
}

// Parse one `fof(...)` / `cnf(...)` record.  Returns 1 on success; a 0
// return is counted by the caller (pc_nskipped) and caps the verdict at
// Unknown -- an unread record must never be silently dropped.
static int pc_parse_record(const char *rec) {
  while (*rec && isspace((unsigned char)*rec)) rec++;
  int is_fof = strncmp(rec, "fof", 3) == 0;
  int is_cnf = strncmp(rec, "cnf", 3) == 0;
  if (!is_fof && !is_cnf) return 0;
  const char *op = strchr(rec, '(');
  if (op == NULL) return 0;
  // inner = between the outermost ( ) of the record
  const char *inner = op + 1;
  char *args[8] = {0};
  int na = pc_split_args(inner, args, 8);
  int ok = 0;
  if (na < 3) goto done;
  {
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

    if (pc_nstep >= PC_MAX_STEPS) goto done;
    if (pc_nstep >= pc_steps_cap) {
      int ncap = pc_steps_cap ? 2 * pc_steps_cap : 256;
      PcStep *ns = (PcStep *)realloc(pc_steps, (size_t)ncap * sizeof(PcStep));
      if (ns == NULL) goto done;
      pc_steps = ns;
      pc_steps_cap = ncap;
    }
    PcStep *st = &pc_steps[pc_nstep];
    memset(st, 0, sizeof *st);
    pc_trim(args[0]);
    {
      // A quoted record name cites unquoted ('n1' and n1 are one name).
      char *nm = args[0];
      size_t ln = strlen(nm);
      if (ln >= 2 && nm[0] == '\'' && nm[ln - 1] == '\'') { nm[ln - 1] = '\0'; nm++; }
      snprintf(st->name, PC_NAME, "%s", nm);
    }
    pc_trim(args[1]);
    st->role = pc_role_of(args[1]);

    PcParse scope;
    Term f = pc_parse_formula(args[2], &scope);
    if (f == 0) {
      if (getenv("THVM_PROOFCHECK_DBG") != NULL) {
        fprintf(stderr, "PCDBG parse-skip name=%s formula=[%.120s]\n",
                args[0], args[2]);
      }
      goto done;                          // unparseable formula
    }
    st->formula = f;
    st->ftext = strdup(args[2]);

    if (na >= 4) {
      pc_parse_source(args[3], st);
    } else {
      st->rule = R_INPUT;                 // no source annotation = given
      st->sourceless = 1;
    }
    pc_nstep++;
    ok = 1;
  }
done:
  for (int i = 0; i < na; i++) free(args[i]);
  return ok;
}

// ===================================================================
// strict profile: problem-file loading + leaf verification
// ===================================================================

static void  pc_strip_comments(char *s);              // fwd (driver section)
static char *pc_read(const char *file, size_t *len);  // fwd (driver section)

// Parse one problem-file record into pc_prob.  Returns 1 on success.
static int pc_parse_problem_record(const char *rec) {
  while (*rec && isspace((unsigned char)*rec)) rec++;
  if (strncmp(rec, "fof", 3) != 0 && strncmp(rec, "cnf", 3) != 0) return 0;
  const char *op = strchr(rec, '(');
  if (op == NULL) return 0;
  char *args[4] = {0};
  int na = pc_split_args(op + 1, args, 4);
  int ok = 0;
  if (na >= 3 && pc_nprob >= pc_prob_cap && pc_nprob < PC_MAX_STEPS) {
    int ncap = pc_prob_cap ? 2 * pc_prob_cap : 256;
    PcProb *np = (PcProb *)realloc(pc_prob, (size_t)ncap * sizeof(PcProb));
    if (np != NULL) { pc_prob = np; pc_prob_cap = ncap; }
  }
  if (na >= 3 && pc_nprob < pc_prob_cap) {
    char *last = args[na - 1];
    int ll = (int)strlen(last);
    while (ll > 0 && isspace((unsigned char)last[ll - 1])) last[--ll] = '\0';
    if (ll > 0 && last[ll - 1] == ')') last[--ll] = '\0';
    while (ll > 0 && isspace((unsigned char)last[ll - 1])) last[--ll] = '\0';
    pc_trim(args[0]);
    pc_trim(args[1]);
    PcParse scope;
    Term f = pc_parse_formula(args[2], &scope);
    if (f != 0) {
      PcProb *pr = &pc_prob[pc_nprob];
      char *nm = args[0];
      size_t ln = strlen(nm);
      if (ln >= 2 && nm[0] == '\'' && nm[ln - 1] == '\'') { nm[ln - 1] = '\0'; nm++; }
      snprintf(pr->name, PC_NAME, "%s", nm);
      pr->role    = pc_role_of(args[1]);
      pr->formula = f;
      pc_nprob++;
      ok = 1;
    }
  }
  for (int i = 0; i < na; i++) free(args[i]);
  return ok;
}

// Load the problem file named by a file() directive.  Returns 0 when
// loaded (possibly with pc_prob_unparsed records), -1 when unavailable.
// One problem file per proof (the rules guarantee every directive names
// the same file as the Proof: header); a second, different directive
// path abstains rather than mixing tables.
static int pc_load_problem(const char *dpath) {
  if (pc_prob_state == 1) {
    return strcmp(pc_prob_directive, dpath) == 0 ? 0 : -1;
  }
  if (pc_prob_state == -1) return -1;
  pc_prob_state = -1;
  snprintf(pc_prob_directive, PC_NAME, "%s", dpath);
  const char *base = strrchr(dpath, '/');
  base = (base != NULL) ? base + 1 : dpath;
  char cand[4][1200];
  int nc = 0;
  if (pc_proof_dir[0] != '\0') {
    snprintf(cand[nc++], 1200, "%s/%s", pc_proof_dir, dpath);
    snprintf(cand[nc++], 1200, "%s/Problems/%s", pc_proof_dir, base);
    snprintf(cand[nc++], 1200, "%s/../Problems/%s", pc_proof_dir, base);
  }
  snprintf(cand[nc++], 1200, "%s", dpath);
  char *text = NULL;
  for (int i = 0; i < nc && text == NULL; i++) text = pc_read(cand[i], NULL);
  if (text == NULL) return -1;
  pc_strip_comments(text);
  int depth = 0, inq = 0;
  char *start = text;
  for (char *p = text; *p; p++) {
    if (inq) { if (*p == '\'') inq = 0; continue; }
    if (*p == '\'') { inq = 1; continue; }
    if (*p == '(' || *p == '[') depth++;
    else if (*p == ')' || *p == ']') depth--;
    else if (*p == '.' && depth == 0) {
      char save = *p;
      *p = '\0';
      int has_content = 0;
      for (const char *q = start; q < p; q++)
        if (!isspace((unsigned char)*q)) { has_content = 1; break; }
      if (has_content && !pc_parse_problem_record(start)) pc_prob_unparsed++;
      *p = save;
      start = p + 1;
    }
  }
  free(text);
  pc_prob_state = 1;
  return 0;
}

// Positive confirmation for introduced(definition) leaves (Vampire's
// function_definition records: mult(a,b) = sF0).  VALID only when the
// record declares exactly one new symbol and the formula (under its
// universal prefix) is an equation whose one side is EXACTLY that
// symbol applied to pairwise-distinct variables, the other side free
// of it; the symbol is absent from the problem file, defined only
// once, and no definition body uses a symbol introduced at the same or
// a later record (well-founded chain).  Such a definition SET is a
// conservative extension -- and every CONSUMER of the symbols must
// still confirm its own step, so nothing can be smuggled through one.
// Never INVALID: shapes this check cannot read simply abstain.
static int pc_check_definition(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  (void)why;
  if (st->n_new_syms != 1) return PC_UNKNOWN;
  u32 L = pc_lookup(st->new_syms[0]);
  if (L == PC_NOT_FOUND) return PC_UNKNOWN;
  Term f = pc_strip_forall(st->formula);
  if (!(term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_EQ &&
        term_ctr_n(f) == 2)) return PC_UNKNOWN;
  Term lhs = term_ctr_at(f, 0), rhs = term_ctr_at(f, 1);
  Term def, body;
  if      (term_tag(lhs) == TAG_CTR && term_ext(lhs) == L) { def = lhs; body = rhs; }
  else if (term_tag(rhs) == TAG_CTR && term_ext(rhs) == L) { def = rhs; body = lhs; }
  else return PC_UNKNOWN;
  if (pc_has_label(body, L)) return PC_UNKNOWN;      // recursive definition
  u32 n = term_ctr_n(def);
  for (u32 i = 0; i < n; i++) {
    Term a = term_ctr_at(def, i);
    if (term_tag(a) != TAG_FVR) return PC_UNKNOWN;   // non-variable argument
    for (u32 j = 0; j < i; j++)
      if (term_ext(term_ctr_at(def, j)) == term_ext(a)) return PC_UNKNOWN;
  }
  for (int j = 0; j < pc_nprob; j++)
    if (pc_has_label(pc_prob[j].formula, L)) return PC_UNKNOWN;
  for (int j = 0; j < pc_nstep; j++) {
    const PcStep *o = &pc_steps[j];
    if (j == idx || !o->introduced || o->n_new_syms < 1) continue;
    for (int s = 0; s < o->n_new_syms; s++) {
      u32 M = pc_lookup(o->new_syms[s]);
      if (M == PC_NOT_FOUND) continue;
      if (M == L) return PC_UNKNOWN;                 // defined twice
      if (j >= idx && pc_has_label(body, M)) return PC_UNKNOWN;  // not well-founded
    }
  }
  return PC_VALID;
}

// Problem-table lookup by formula name (first match), or -1.
static int pc_prob_find(const char *name) {
  for (int i = 0; i < pc_nprob; i++)
    if (strcmp(pc_prob[i].name, name) == 0) return i;
  return -1;
}
// A file() formula name that names nothing: absent, or Vampire's
// placeholder 'unknown'.
static int pc_nameless(const char *nm) {
  return nm[0] == '\0' || strcmp(nm, "unknown") == 0;
}

// Compare a leaf formula against candidate problem formulas.  A
// syntactically (alpha, permuted ALL-prefix, reoriented equation) or
// PROVABLY equivalent candidate confirms the leaf when allow_valid
// (else the match only abstains).  Only a SEMANTIC difference from
// EVERY candidate -- a countermodel of one direction for each -- is a
// demonstrated lie; an undecided comparison abstains.  Large candidate
// sets are not worth the saturations: abstain.
static int pc_leaf_compare(Term leaf, const Term *cand, int ncand,
                           int allow_valid) {
  for (int k = 0; k < ncand; k++)
    if (pc_leaf_syn_eq(leaf, cand[k])) return allow_valid ? PC_VALID : PC_UNKNOWN;
  if (ncand == 0 || ncand > 8) return PC_UNKNOWN;
  int all_differ = 1;
  for (int k = 0; k < ncand; k++) {
    Term a = pc_close(leaf);
    Term b = pc_close(cand[k]);
    Term d1[2] = { a, fol_mk_not(b) };
    AtpStatus s1 = pc_refute(d1, 2);
    if (s1 == ATP_QUEUE_EMPTY) continue;               // demonstrated difference
    Term d2[2] = { b, fol_mk_not(a) };
    AtpStatus s2 = pc_refute(d2, 2);
    if (s2 == ATP_QUEUE_EMPTY) continue;               // demonstrated difference
    if ((s1 == ATP_PROVED || s1 == ATP_REFUTED) &&
        (s2 == ATP_PROVED || s2 == ATP_REFUTED))
      return allow_valid ? PC_VALID : PC_UNKNOWN;      // equivalent
    all_differ = 0;                                    // undecided comparison
  }
  return all_differ ? PC_INVALID : PC_UNKNOWN;
}

// A record with NO source annotation is neither an import (the rules
// make every axiom and conjecture import its formula through a file
// directive) nor an inference: an unjustified assertion.  When its
// formula differs semantically from every problem formula it could
// stand for (the conjecture for a conjecture record; otherwise every
// axiom and the NEGATION of every conjecture) it asserts something the
// problem never gave -- demonstrated (PRV054's axiom p(a), PRV039's
// bare plain q(a)).  A record restating a problem formula without the
// directive only abstains.
static int pc_check_sourceless(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  *why = "record carries no source annotation";
  if (pc_prob_state != 1 || pc_prob_unparsed) return PC_UNKNOWN;
  static Term cand[16];
  int ncand = 0;
  int want_conj = (st->role == PC_ROLE_CONJ);
  for (int i = 0; i < pc_nprob && ncand < 9; i++) {
    int is_conj = (pc_prob[i].role == PC_ROLE_CONJ);
    if (want_conj) {
      if (is_conj) cand[ncand++] = pc_prob[i].formula;
    } else {
      cand[ncand++] = is_conj ? fol_mk_not(pc_close(pc_prob[i].formula))
                              : pc_prob[i].formula;
    }
  }
  if (pc_leaf_compare(st->formula, cand, ncand, 0) == PC_INVALID) {
    *why = "record has no source and asserts a formula the problem does not give";
    return PC_INVALID;
  }
  return PC_UNKNOWN;
}

// Verify an input step.  Legacy profile: inputs are trusted (the
// internal-trace certification use).  Strict profile: the leaf must
// positively match the problem-file formula it cites --
//   * introduced(definition) of confirmed shape          -> confirmed
//   * other introduced() / missing directive             -> Unknown
//   * no source at all, formula given by no problem
//     formula (pc_check_sourceless)                      -> VerifiedBad
//   * cited name absent (problem file fully parsed)      -> VerifiedBad
//     (the rules: "the checker must verify that the referenced item
//     exists in the problem file")
//   * role lie on the conjecture (either direction)      -> VerifiedBad
//   * alpha-equivalent OR proved-equivalent formula      -> confirmed
//     (real provers reprint leaves with permuted binders; the rules
//     themselves allow the alpha check "using an external ATP", which
//     can only mean equivalence)
//   * semantically DIFFERENT from everything it could
//     cite (countermodel)                                -> VerifiedBad
//   * otherwise                                          -> Unknown
static int pc_check_leaf(int idx, const char **why) {
  PcStep *st = &pc_steps[idx];
  if (!pc_strict()) return PC_VALID;
  if (st->introduced) {
    int v = pc_check_definition(idx, why);
    if (v == PC_VALID) return v;
    *why = "introduced() formula cannot be positively confirmed";
    return PC_UNKNOWN;
  }
  if (st->sourceless) return pc_check_sourceless(idx, why);
  if (st->src_file[0] == '\0') {
    *why = "input step lacks a checkable file directive";
    return PC_UNKNOWN;
  }
  if (pc_load_problem(st->src_file) != 0) {
    *why = "problem file unavailable";
    return PC_UNKNOWN;
  }
  if (getenv("THVM_PROOFCHECK_DBG") != NULL) {
    fprintf(stderr, "PCDBG leaf name=%s cites=%s@%s nprob=%d unparsed=%d\n",
            st->name, st->src_name, st->src_file, pc_nprob, pc_prob_unparsed);
  }
  // Candidate problem formulas this leaf may legitimately import: the
  // named entry, or for a nameless directive (FindProof's bare
  // file('x.p'), Vampire's 'unknown') every ROLE-COMPATIBLE entry.  Role
  // compatibility bakes in the conjecture integrity: claiming a
  // non-conjecture as the conjecture (refuting the wrong statement) or
  // importing the problem's conjecture as a premise (assuming the goal)
  // leaves the candidate set without the lying entry, and the mismatch
  // surfaces as a demonstrated difference below.
  static Term cand[16];
  int ncand = 0, nrole = 0;
  int want_conj = (st->role == PC_ROLE_CONJ);
  if (!pc_nameless(st->src_name)) {
    int named = pc_prob_find(st->src_name);
    if (named < 0) {
      if (pc_prob_unparsed) {
        *why = "cited formula not found (problem file only partially parsed)";
        return PC_UNKNOWN;
      }
      *why = "cites a formula name absent from the problem file";
      return PC_INVALID;
    }
    int is_conj_entry = (pc_prob[named].role == PC_ROLE_CONJ);
    if (want_conj != is_conj_entry) {
      *why = want_conj
           ? "conjecture step cites a non-conjecture problem formula"
           : "the problem's conjecture is imported as a premise";
      return PC_INVALID;
    }
    cand[ncand++] = pc_prob[named].formula;
  } else {
    for (int i = 0; i < pc_nprob; i++) {
      if ((pc_prob[i].role == PC_ROLE_CONJ) != want_conj) continue;
      nrole++;
      if (ncand < 9) cand[ncand++] = pc_prob[i].formula;
    }
    // Syntactic match over the WHOLE role-compatible set (cheap).
    if (nrole > ncand) {
      for (int i = 0; i < pc_nprob; i++)
        if ((pc_prob[i].role == PC_ROLE_CONJ) == want_conj &&
            pc_leaf_syn_eq(st->formula, pc_prob[i].formula)) return PC_VALID;
      *why = "formula matches no problem formula syntactically";
      return PC_UNKNOWN;
    }
  }
  if (ncand == 0) {
    if (pc_prob_unparsed) {
      *why = "cited formula not found (problem file only partially parsed)";
      return PC_UNKNOWN;
    }
    *why = "cites a formula absent from the problem file";
    return PC_INVALID;
  }
  int v = pc_leaf_compare(st->formula, cand, ncand, 1);
  if (v == PC_VALID) return v;
  if (v == PC_INVALID && !pc_prob_unparsed) {
    *why = "formula differs from every problem formula it could cite";
    return PC_INVALID;
  }
  *why = "formula is not alpha-equivalent to the cited problem formula";
  return PC_UNKNOWN;
}

// ===================================================================
// per-rule step checks
// ===================================================================

// Gather the cited parents' formulas; returns 0 if any is missing.
// Parents resolve ANYWHERE in the file: the ProoVer rules place no
// requirement on record order (the 2026 entry's strictly-earlier scan
// turned shuffled-but-valid proofs into abstains and worse).  Circular
// justification is excluded globally instead: pc_check_structure
// rejects any citation cycle BEFORE step checking starts, so a
// full-range resolution here cannot admit a formula derived from its
// own descendants.
static int pc_parent_formulas(const PcStep *st, Term *out, int *nout) {
  int n = 0;
  for (int i = 0; i < st->n_parents; i++) {
    int idx = st->par[i];
    if (idx < 0) return 0;
    if (n < PC_MAX_PARENTS) out[n++] = pc_steps[idx].formula;
  }
  *nout = n;
  return 1;
}

// Structural pass, run before any step check.  Returns 0 with *why /
// *bad_idx set on a DEMONSTRATED structural violation:
//   * duplicate step names with differing formulas (ambiguous
//     citations -- every parent reference to that name is unsound);
//   * a citation cycle (circular justification: with cycles admitted,
//     two steps can each "derive" the other and both pass entailment).
// Also sets pc_dangling when some cited parent resolves to no step, and
// pc_topo when every citation points to an EARLIER record.
static int pc_topo = 1;
static int pc_check_structure(const char **why, int *bad_idx) {
  pc_topo = 1;
  for (int i = 0; i < pc_nstep; i++) {
    int f = pc_find(pc_steps[i].name);
    if (f >= 0 && f != i &&
        !pc_alpha_eq(pc_steps[i].formula, pc_steps[f].formula)) {
      *why = "duplicate step name with differing formulas";
      *bad_idx = i;
      return 0;
    }
  }
  u8  *colour = (u8 *)calloc((size_t)pc_nstep + 1, 1);   // 0 white 1 grey 2 black
  int *stk = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  int *pi  = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  if (colour == NULL || stk == NULL || pi == NULL) {
    free(colour); free(stk); free(pi);
    pc_dangling = 1;                                    // cannot decide: abstain paths
    return 1;
  }
  int ok = 1;
  for (int r = 0; r < pc_nstep && ok; r++) {
    for (int k = 0; k < pc_steps[r].n_parents; k++) {
      int p = pc_steps[r].par[k];
      if (p < 0) pc_dangling = 1;
      else if (p >= r) pc_topo = 0;
    }
    if (pc_steps[r].par_compound) pc_topo = 0;
    if (colour[r] != 0) continue;
    int sp = 0;
    stk[0] = r;
    pi[0] = 0;
    colour[r] = 1;
    while (sp >= 0) {
      int node = stk[sp];
      if (pi[sp] >= pc_steps[node].n_parents) {
        colour[node] = 2;
        sp--;
        continue;
      }
      int k = pi[sp]++;
      int nx = pc_steps[node].par[k];
      if (nx < 0) continue;
      if (colour[nx] == 1) {
        *why = "circular derivation";
        *bad_idx = node;
        ok = 0;
        break;
      }
      if (colour[nx] == 0 && sp + 1 < pc_nstep + 1) {
        colour[nx] = 1;
        sp++;
        stk[sp] = nx;
        pi[sp] = 0;
      }
    }
  }
  free(colour); free(stk); free(pi);
  return ok;
}

// A refutation may use the conjecture ONLY through its negation (the
// rules: the conjecture is negated and the proof ends with $false).
// Walk the citation DAG down from every derived $false record; a
// NEGATION step (the negated_conjecture rule, a cth claim, or a
// negated_conjecture-role record citing only conjecture records) stops
// the walk.  Reaching a conjecture-role record any other way means the
// refutation ASSUMED what it set out to prove -- PRV031's `assume`
// step turns the conjecture into a premise and closes against its own
// negation.  Structural (roles, rule names, statuses): each negation
// step still confirms its own negation shape in the step loop.
static int pc_is_negation_step(const PcStep *st) {
  if (st->rule == R_NEGCONJ) return 1;
  if (st->st_mask & (1u << PC_ST_CTH)) return 1;
  if (st->role == PC_ROLE_NEGCONJ && st->n_parents > 0) {
    for (int k = 0; k < st->n_parents; k++) {
      int p = st->par[k];
      if (p < 0 || pc_steps[p].role != PC_ROLE_CONJ) return 0;
    }
    return 1;
  }
  return 0;
}
// pc_conj_pos[i]: record i is a conjecture-role record or derived from
// one through non-negation steps (memoized DFS; the citation graph is
// acyclic once pc_check_structure passed).
static void pc_conj_pos_build(void) {
  free(pc_conj_pos);
  pc_conj_pos = (u8 *)calloc((size_t)pc_nstep + 1, 1);   // 0 unknown 1 no 2 yes
  int *stk = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  int *pi = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  if (pc_conj_pos == NULL || stk == NULL || pi == NULL) {
    free(stk); free(pi);
    return;
  }
  for (int r = 0; r < pc_nstep; r++) {
    if (pc_conj_pos[r]) continue;
    int sp = 0;
    stk[0] = r; pi[0] = 0;
    while (sp >= 0) {
      int node = stk[sp];
      const PcStep *st = &pc_steps[node];
      if (st->role == PC_ROLE_CONJ) { pc_conj_pos[node] = 2; sp--; continue; }
      if (pc_is_negation_step(st)) { pc_conj_pos[node] = 1; sp--; continue; }
      if (pi[sp] < st->n_parents) {
        int p = st->par[pi[sp]++];
        if (p >= 0 && !pc_conj_pos[p] && sp + 1 <= pc_nstep) {
          sp++; stk[sp] = p; pi[sp] = 0;
        }
        continue;
      }
      u8 v = 1;
      for (int k = 0; k < st->n_parents; k++) {
        int p = st->par[k];
        if (p >= 0 && pc_conj_pos[p] == 2) v = 2;
      }
      pc_conj_pos[node] = v;
      sp--;
    }
  }
  for (int r = 0; r < pc_nstep; r++) pc_conj_pos[r] = (pc_conj_pos[r] == 2);
  free(stk); free(pi);
}
static int pc_check_conj_premise(const char **why, int *bad_idx) {
  pc_conj_pos_build();
  u8  *seen = (u8 *)calloc((size_t)pc_nstep + 1, 1);
  int *stk = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  if (seen == NULL || stk == NULL) { free(seen); free(stk); return 1; }
  int ok = 1;
  for (int r = 0; r < pc_nstep && ok; r++) {
    const PcStep *root = &pc_steps[r];
    if (root->rule == R_INPUT || !pc_is_false(root->formula) || seen[r]) continue;
    int sp = 0;
    stk[sp++] = r;
    seen[r] = 1;
    while (sp > 0 && ok) {
      int node = stk[--sp];
      const PcStep *st = &pc_steps[node];
      if (pc_is_negation_step(st)) continue;
      for (int k = 0; k < st->n_parents; k++) {
        int p = st->par[k];
        if (p < 0) continue;
        if (pc_steps[p].role == PC_ROLE_CONJ) {
          *why = "the refutation uses the conjecture as a premise (not through its negation)";
          *bad_idx = node;
          ok = 0;
          break;
        }
        if (!seen[p]) { seen[p] = 1; stk[sp++] = p; }
      }
    }
  }
  free(seen); free(stk);
  return ok;
}

// --- symbol occurrence index -------------------------------------------
// Per label: the records whose formula carries it, and whether the
// problem file does.  Built once per pass (pc_sym_index_build) so the
// per-step freshness questions stay linear in the occurrences.
static int *pc_occ_start = NULL;      // CSR offsets, pc_nsyms + 1 entries
static int *pc_occ_rec = NULL;        // record indices
static u8  *pc_in_prob = NULL;        // label - PC_LAB_BASE -> in problem file
static int  pc_occ_built = 0;
static int  pc_occ_ns = 0;          // pc_nsyms when the index was built
static void pc_collect_labels(Term t, int *stamp, int mark, int *out, int *n, int cap) {
  if (term_tag(t) != TAG_CTR) return;
  u32 lab = term_ext(t);
  if (lab >= PC_LAB_BASE && lab < PC_LAB_BASE + (u32)pc_nsyms) {
    int s = (int)(lab - PC_LAB_BASE);
    if (stamp[s] != mark) {
      stamp[s] = mark;
      if (*n < cap) out[(*n)++] = s;
    }
  }
  u32 k = term_ctr_n(t);
  for (u32 i = 0; i < k; i++) pc_collect_labels(term_ctr_at(t, i), stamp, mark, out, n, cap);
}
static void pc_sym_index_build(void) {
  free(pc_occ_start); free(pc_occ_rec); free(pc_in_prob);
  pc_occ_start = NULL; pc_occ_rec = NULL; pc_in_prob = NULL;
  pc_occ_built = 0;
  int ns = pc_nsyms;
  pc_occ_ns = ns;
  int *stamp = (int *)malloc(((size_t)ns + 1) * sizeof(int));
  int *cnt = (int *)calloc((size_t)ns + 2, sizeof(int));
  int *buf = (int *)malloc(((size_t)ns + 1) * sizeof(int));
  pc_in_prob = (u8 *)calloc((size_t)ns + 1, 1);
  if (stamp == NULL || cnt == NULL || buf == NULL || pc_in_prob == NULL) {
    free(stamp); free(cnt); free(buf); return;
  }
  for (int s = 0; s < ns; s++) stamp[s] = -1;
  for (int i = 0; i < pc_nstep; i++) {
    int n = 0;
    pc_collect_labels(pc_steps[i].formula, stamp, i, buf, &n, ns);
    for (int k = 0; k < n; k++) cnt[buf[k] + 1]++;
  }
  for (int s = 0; s < ns; s++) cnt[s + 1] += cnt[s];
  pc_occ_start = (int *)malloc(((size_t)ns + 1) * sizeof(int));
  pc_occ_rec = (int *)malloc(((size_t)cnt[ns] + 1) * sizeof(int));
  if (pc_occ_start == NULL || pc_occ_rec == NULL) {
    free(stamp); free(cnt); free(buf); return;
  }
  memcpy(pc_occ_start, cnt, ((size_t)ns + 1) * sizeof(int));
  for (int s = 0; s < ns; s++) stamp[s] = -1;
  for (int i = 0; i < pc_nstep; i++) {
    int n = 0;
    pc_collect_labels(pc_steps[i].formula, stamp, i, buf, &n, ns);
    for (int k = 0; k < n; k++) pc_occ_rec[cnt[buf[k]]++] = i;
  }
  for (int s = 0; s < ns; s++) stamp[s] = -1;
  for (int j = 0; j < pc_nprob; j++) {
    int n = 0;
    pc_collect_labels(pc_prob[j].formula, stamp, 1000000 + j, buf, &n, ns);
    for (int k = 0; k < n; k++) pc_in_prob[buf[k]] = 1;
  }
  free(stamp); free(cnt); free(buf);
  pc_occ_built = 1;
}
static int pc_label_in_prob(u32 L) {
  if (!pc_occ_built || L < PC_LAB_BASE || L >= PC_LAB_BASE + (u32)pc_occ_ns) {
    for (int j = 0; j < pc_nprob; j++)
      if (pc_has_label(pc_prob[j].formula, L)) return 1;
    return 0;
  }
  return pc_in_prob[L - PC_LAB_BASE];
}
static int pc_step_has(int j, u32 L) { return pc_has_label(pc_steps[j].formula, L); }

// Occurrence modes of label L in a FORMULA: bit 1 = as a predicate
// (atom head), bit 2 = inside a term.
static int pc_label_modes_t(Term t, u32 L) {
  if (term_tag(t) != TAG_CTR) return 0;
  int m = (term_ext(t) == L) ? 2 : 0;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) m |= pc_label_modes_t(term_ctr_at(t, i), L);
  return m;
}
static int pc_label_modes(Term f, u32 L) {
  if (term_tag(f) != TAG_CTR) return 0;
  u32 lab = term_ext(f);
  switch (lab) {
    case PC_LAB_NOT: case PC_LAB_AND: case PC_LAB_OR:
    case PC_LAB_IMP: case PC_LAB_IFF: {
      int m = 0;
      u32 n = term_ctr_n(f);
      for (u32 i = 0; i < n; i++) m |= pc_label_modes(term_ctr_at(f, i), L);
      return m;
    }
    case PC_LAB_ALL: case PC_LAB_EX:
      return pc_label_modes(term_ctr_at(f, 1), L);
    case PC_LAB_EQ: {
      int m = 0;
      for (u32 i = 0; i < term_ctr_n(f); i++) m |= pc_label_modes_t(term_ctr_at(f, i), L);
      return m;
    }
    default: {
      int m = (lab == L) ? 1 : 0;
      for (u32 i = 0; i < term_ctr_n(f); i++) m |= pc_label_modes_t(term_ctr_at(f, i), L);
      return m;
    }
  }
}

// Same resolved parent set, rule and status: one inference's outputs.
static int pc_siblings(const PcStep *a, const PcStep *b) {
  if (a->n_parents != b->n_parents || a->n_parents == 0) return 0;
  if (a->par_compound || b->par_compound) return 0;
  if (strcmp(a->rule_name, b->rule_name) != 0 || a->st_mask != b->st_mask) return 0;
  for (int i = 0; i < a->n_parents; i++) {
    int hit = 0;
    for (int j = 0; j < b->n_parents && !hit; j++) hit = (a->par[i] == b->par[j]);
    if (!hit || a->par[i] < 0) return 0;
  }
  return 1;
}

// Skolem/definition-symbol freshness by UNIQUE INTRODUCTION SOURCE.
// Collect every record carrying L; a SOURCE is one none of whose
// resolved parents carries L.  Acyclicity makes every occurrence chain
// end at a source, so ONE introduction source means the symbol flows
// only downstream of one witness choice.  A descendant-cone-of-the-
// skolemize-step test is WRONG here: Vampire introduces the symbol in a
// separate introduced(..[skolem_symbol_introduction]) LEAF that the
// skolemisation step cites as a PARENT.  Sources that cannot constrain
// L are not introductions:
//   * a thm-only record: its conclusion is entailed by L-free premises,
//     so it holds for EVERY interpretation of L (a use such as
//     instantiating a universal axiom at the Skolem constant -- PRV019,
//     PRV089 -- or a tautology mentioning it -- PRV076); it must still
//     confirm on its own before any VerifiedGood;
//   * a record declaring only OTHER symbols (a definition/naming USE:
//     Vampire's avatar_definition leaves mention Skolem terms with no
//     parents).
// Sibling sources (one inference over the same parents: same rule and
// status) carrying a symbol used only as a PREDICATE are one
// introduction: a definitional (Tseitin) transformation names its
// subformulas across several emitted clauses (CSR021's def36).  A
// FUNCTION symbol gets no such grouping: two sibling skolemisations
// sharing a witness fabricate p(sk)&q(sk) from ?[X]p(X)&?[X]q(X).
// Two or more introductions are the demonstrated reuse violation, as
// is L in the problem file.  `specified` (the mandated skolemize rule,
// which declares the symbol NEW at this very step) also makes an
// occurrence in the step's own parent a violation (EVL003's groom
// re-skolemizes with sK0), and an introduction anywhere else when this
// step's result does not carry the symbol (a vacuous binder).
static int pc_sk_fresh(int idx, u32 L, int specified, const char **why) {
  const PcStep *me = &pc_steps[idx];
  if (pc_label_in_prob(L)) {
    *why = "Skolem symbol already occurs in the problem file";
    return PC_INVALID;
  }
  if (specified) {
    for (int k = 0; k < me->n_parents; k++) {
      int p = me->par[k];
      if (p >= 0 && pc_step_has(p, L)) {
        *why = "declared new symbol already occurs in the step's parent";
        return PC_INVALID;
      }
    }
  }
  if (pc_dangling) {
    *why = "Skolem freshness undecidable (incomplete citation graph)";
    return PC_UNKNOWN;
  }
  const int *list = NULL;
  int nlist = 0;
  static int *tmp = NULL, *grp = NULL;
  static int tmp_cap = 0;
  if (tmp_cap < pc_nstep + 1) {
    free(tmp); free(grp);
    tmp_cap = pc_nstep + 1;
    tmp = (int *)malloc((size_t)tmp_cap * sizeof(int));
    grp = (int *)malloc((size_t)tmp_cap * sizeof(int));
    if (tmp == NULL || grp == NULL) { tmp_cap = 0; return PC_UNKNOWN; }
  }
  if (pc_occ_built && L >= PC_LAB_BASE && L < PC_LAB_BASE + (u32)pc_occ_ns) {
    int s = (int)(L - PC_LAB_BASE);
    list = pc_occ_rec + pc_occ_start[s];
    nlist = pc_occ_start[s + 1] - pc_occ_start[s];
  } else {
    for (int j = 0; j < pc_nstep; j++) if (pc_step_has(j, L)) tmp[nlist++] = j;
    list = tmp;
  }
  int pred_only = 1;
  for (int q = 0; q < nlist; q++)
    if (pc_label_modes(pc_steps[list[q]].formula, L) & 2) pred_only = 0;
  int nsrc = 0, me_src = 0;
  for (int q = 0; q < nlist; q++) {
    int j = list[q];
    const PcStep *o = &pc_steps[j];
    int from_parent = 0;
    for (int k = 0; k < o->n_parents && !from_parent; k++) {
      int p = o->par[k];
      if (p >= 0 && pc_step_has(p, L)) from_parent = 1;
    }
    if (from_parent) continue;
    if (o->n_new_syms > 0) {
      int declares_L = 0;
      for (int s = 0; s < o->n_new_syms && !declares_L; s++)
        if (pc_lookup(o->new_syms[s]) == L) declares_L = 1;
      if (!declares_L) continue;                 // a use of L by a naming record
    }
    if (o->st_mask == (1u << PC_ST_THM) && o->rule != R_INPUT) continue;
    int merged = 0;
    if (pred_only)
      for (int g = 0; g < nsrc && !merged; g++) merged = pc_siblings(o, &pc_steps[grp[g]]);
    if (j == idx) me_src = 1;
    if (merged) continue;
    grp[nsrc++] = j;
  }
  if (specified && !me_src && nsrc > 0) {
    *why = "declared new symbol is introduced elsewhere in the proof";
    return PC_INVALID;
  }
  if (nsrc == 1 || (nsrc == 0 && specified)) return PC_VALID;
  if (nsrc == 0) {                       // unreachable once cycles are barred
    *why = "Skolem freshness undecidable";
    return PC_UNKNOWN;
  }
  *why = "Skolem symbol is not fresh (reused)";
  return PC_INVALID;
}

// Re-fetch the current step's parent/conclusion handles from pc_steps
// after a rooted eq-session collection: pc_refresh_formulas already
// rewrote pc_steps with the relocated Terms, but the pc_entails frame's
// handles are stale from-space Terms.  Fills out[0..np) and *concl;
// returns 0 (caller abstains) if the step context is missing or the
// parent count changed.
static int pc_refresh_step_handles(Term *out, int np, Term *concl) {
  if (g_pc_cur_idx < 0 || g_pc_cur_idx >= pc_nstep) return 0;
  int rnp = 0;
  if (!pc_parent_formulas(&pc_steps[g_pc_cur_idx], out, &rnp)) return 0;
  if (rnp != np) return 0;
  *concl = pc_steps[g_pc_cur_idx].formula;
  return 1;
}

static int pc_has_quant(Term t) {
  if (term_tag(t) != TAG_CTR) return 0;
  u32 lab = term_ext(t);
  if (lab == PC_LAB_ALL || lab == PC_LAB_EX) return 1;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) if (pc_has_quant(term_ctr_at(t, i))) return 1;
  return 0;
}

// A countermodel over a PARTIAL premise set (an unresolvable compound
// parent was dropped) demonstrates nothing about the real step.
static int pc_demote_partial(const PcStep *st, int v) {
  return (v == PC_INVALID && st->par_compound) ? PC_UNKNOWN : v;
}

static int pc_check_instantiate(const PcStep *st, const char **why) {
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) { *why = "parent missing"; return PC_UNKNOWN; }
  // Fast path: the conclusion is a direct substitution instance of the
  // parent's matrix.  Only for QUANTIFIER-FREE matrix and conclusion:
  // the matcher reads binders as ordinary variables, so a nested
  // quantifier would let a free pattern variable capture a bound one
  // (![X]:?[Y]:r(X,Y) "instantiated" to ?[Y]:r(Y,Y) -- not entailed).
  Term pat = pc_strip_forall(par[0]);
  if (!pc_has_quant(pat) && !pc_has_quant(st->formula) &&
      pc_max_var(pat) < REWRITE_MAX_VAR) {
    RewriteSubst sub; memset(&sub, 0, sizeof sub);
    if (thvm_match(pat, st->formula, &sub)) return PC_VALID;
  }
  // otherwise settle by entailment (an instance is a consequence).
  int v = pc_entails(par, np, st->formula);
  *why = "conclusion is not an instance of the parent";
  if (v == PC_INVALID && !(st->st_mask & ((1u << PC_ST_THM) | (1u << PC_ST_CTH)))) {
    *why = "step does not claim entailment; left unconfirmed";
    v = PC_UNKNOWN;
  }
  return pc_demote_partial(st, v);
}

// conclusion == not(parent) by the NNF fast path or mutual entailment.
// PC_INVALID only on a countermodel of one direction.
static int pc_negation_equiv(Term parent, Term concl) {
  Term negp = fol_mk_not(parent);
  if (pc_alpha_eq(fol_nnf(negp), fol_nnf(concl))) return PC_VALID;
  Term f1[2] = { negp, fol_mk_not(concl) };     // (~parent) & (~concl)
  Term f2[2] = { concl, parent };               // (concl) & (parent)
  AtpStatus s1 = pc_refute(f1, 2);
  AtpStatus s2 = pc_refute(f2, 2);
  int e1 = (s1 == ATP_PROVED || s1 == ATP_REFUTED);
  int e2 = (s2 == ATP_PROVED || s2 == ATP_REFUTED);
  if (e1 && e2) return PC_VALID;
  if (s1 == ATP_QUEUE_EMPTY || s2 == ATP_QUEUE_EMPTY) return PC_INVALID;
  return PC_UNKNOWN;
}

static int pc_check_negconj(const PcStep *st, const char **why) {
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) { *why = "parent missing"; return PC_UNKNOWN; }
  // The parent must BE the conjecture: negating any other formula (an
  // axiom, say) yields a refutation of axioms-versus-their-own-negation
  // that proves nothing about the conjecture, yet can pass the
  // equivalence check below when the negations happen to align.  A
  // RESOLVED non-conjecture parent is the demonstrated violation.
  {
    int cidx = st->par[0];
    if (cidx < 0) {
      *why = "negated_conjecture parent missing";
      return PC_UNKNOWN;
    }
    if (pc_steps[cidx].role != PC_ROLE_CONJ) {
      *why = "negated_conjecture parent is not the conjecture";
      return PC_INVALID;
    }
  }
  int v = pc_negation_equiv(par[0], st->formula);
  if (v == PC_INVALID) *why = "not the negation of the conjecture";
  else if (v == PC_UNKNOWN) *why = "negation could not be settled";
  return v;
}

// Collect the argument tuple of every L-headed subterm in `t`.  All
// occurrences must carry the SAME tuple of plain variables (a proper
// Skolem function is applied uniformly to its scope variables);
// anything else returns -1.  Returns 0 if no occurrence, 1 otherwise.
static int pc_sk_args(Term t, u32 L, u32 *av, u32 *na, int *have) {
  if (term_tag(t) != TAG_CTR) return 0;
  if (term_ext(t) == L) {
    u32 n = term_ctr_n(t);
    if (n > PC_MAX_RVARS) return -1;
    for (u32 i = 0; i < n; i++)
      if (term_tag(term_ctr_at(t, i)) != TAG_FVR) return -1;   // non-variable argument
    if (!*have) {
      *have = 1; *na = n;
      for (u32 i = 0; i < n; i++) av[i] = term_ext(term_ctr_at(t, i));
    } else if (*na != n) {
      return -1;
    } else {
      for (u32 i = 0; i < n; i++) if (av[i] != term_ext(term_ctr_at(t, i))) return -1;
    }
    return 1;
  }
  int found = 0;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) {
    int r = pc_sk_args(term_ctr_at(t, i), L, av, na, have);
    if (r < 0) return -1;
    found |= r;
  }
  return found;
}

// Replace every L-headed subterm by the variable V.
static Term pc_sk_abstract(Term t, u32 L, u32 V) {
  if (term_tag(t) != TAG_CTR) return t;
  if (term_ext(t) == L) return term_new_fvr(V);
  u32 n = term_ctr_n(t);
  if (n == 0) return t;
  Term kids[REWRITE_MAX_ARITY];
  if (n > REWRITE_MAX_ARITY) return t;
  int changed = 0;
  for (u32 i = 0; i < n; i++) {
    Term c = term_ctr_at(t, i);
    kids[i] = pc_sk_abstract(c, L, V);
    if (kids[i] != c) changed = 1;
  }
  return changed ? term_new_ctr(term_ext(t), kids, n) : t;
}

// Insert `exists V` into the quantifier prefix of `f` at the outermost
// point where every Skolem argument variable is already universally
// bound.  V must not fall under a universal that is NOT an argument
// (the witness would gain a dependence the Skolem term does not have);
// existentials may be passed freely (their witnesses are functions of
// the outer universals already).  *ok = 0 when no sound placement
// exists.
static Term pc_sk_place(Term f, u32 V, const u32 *av, u32 na, u32 bound,
                        int *ok) {
  if (bound == na)
    return fol_mk_ex(term_new_fvr(V), f);
  if (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_ALL) {
    Term var = term_ctr_at(f, 0), body = term_ctr_at(f, 1);
    u32 vid = term_ext(var);
    int is_arg = 0;
    for (u32 i = 0; i < na; i++) if (av[i] == vid) is_arg = 1;
    if (!is_arg) { *ok = 0; return f; }
    return fol_mk_all(var, pc_sk_place(body, V, av, na, bound + 1, ok));
  }
  if (term_tag(f) == TAG_CTR && term_ext(f) == PC_LAB_EX) {
    Term var = term_ctr_at(f, 0), body = term_ctr_at(f, 1);
    return fol_mk_ex(var, pc_sk_place(body, V, av, na, bound, ok));
  }
  *ok = 0;   // matrix reached with argument variables unbound
  return f;
}

// Semantic skolemisation check: the legacy profile's `skolemize` and
// every profile's FOREIGN esa records that declare new symbols (a
// prover's skolemisation / cnf_transformation).  `specified` == 1: the
// mandated rule name, so shape failures are demonstrated; 0: only the
// freshness violation stands, other failures abstain.
static int pc_check_skolemize(int idx, int specified, const char **why) {
  const PcStep *st = &pc_steps[idx];
  if (st->new_sym[0] == '\0') { *why = "no declared Skolem symbol"; return PC_UNKNOWN; }
  // The full declared list: real prover skolemisation records introduce
  // SEVERAL symbols in one step (SEU140+2's f50_sk declares three).
  int nsyms = (st->n_new_syms > 0) ? st->n_new_syms : 1;
  u32 Ls[PC_MAX_NEWSYMS];
  for (int s = 0; s < nsyms; s++) {
    const char *nm = (st->n_new_syms > 0) ? st->new_syms[s] : st->new_sym;
    Ls[s] = pc_lookup(nm);
    // every declared symbol must actually occur in the resulting formula
    if (Ls[s] == PC_NOT_FOUND || !pc_has_label(st->formula, Ls[s])) {
      *why = "declared Skolem symbol absent from the result";
      return specified ? PC_INVALID : PC_UNKNOWN;
    }
    // Abstracting a symbol into a first-order variable is meaningful
    // only in TERM position: a predicate (a definition such as a Tseitin
    // name) would become a variable ATOM, which the saturation unifies
    // with every literal -- confirming anything.
    if (pc_label_modes(st->formula, Ls[s]) & 1) {
      *why = "declared symbol is a predicate (definition), not a Skolem function";
      int fv = pc_sk_fresh(idx, Ls[s], specified, why);
      return fv == PC_INVALID ? fv : PC_UNKNOWN;
    }
  }
  // must be FRESH: no other introduction source per symbol
  for (int s = 0; s < nsyms; s++) {
    int fv = pc_sk_fresh(idx, Ls[s], specified, why);
    if (fv != PC_VALID) return fv;
  }
  // The step must relate to its PARENT: freshness alone would let a
  // "skolemize" step smuggle in an arbitrary claim wrapped around a
  // fresh symbol.  Soundness condition: the parent entails the
  // conclusion with every Skolem term abstracted to an existential
  // placed exactly where that Skolem's argument tuple binds -- then any
  // model of the parent interprets the fresh symbols as the witness
  // functions, so the step is a conservative (esa) extension.  Symbols
  // are re-existentialised in declaration order, outermost first.
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) {
    *why = "skolemize parent missing";
    return PC_UNKNOWN;
  }
  static u32 avs[PC_MAX_NEWSYMS][PC_MAX_RVARS];
  u32 nas[PC_MAX_NEWSYMS];
  for (int s = 0; s < nsyms; s++) {
    int have = 0;
    nas[s] = 0;
    if (pc_sk_args(st->formula, Ls[s], avs[s], &nas[s], &have) < 0) {
      *why = "Skolem symbol applied non-uniformly";
      return PC_UNKNOWN;
    }
  }
  u32 Vbase = pc_max_var(st->formula) + 1u;
  Term target = st->formula;
  for (int s = 0; s < nsyms; s++)
    target = pc_sk_abstract(target, Ls[s], Vbase + (u32)s);
  for (int s = nsyms - 1; s >= 0; s--) {
    int placed = 1;
    target = pc_sk_place(target, Vbase + (u32)s, avs[s], nas[s], 0, &placed);
    if (!placed) {
      *why = "no sound placement for the Skolem witness";
      return PC_UNKNOWN;
    }
  }
  int v = pc_entails(par, np, target);
  if (v == PC_VALID) return PC_VALID;
  if (v == PC_INVALID && !specified) v = PC_UNKNOWN;
  *why = (v == PC_INVALID)
       ? "conclusion is not a Skolemization of the parent"
       : "Skolemization could not be settled";
  return pc_demote_partial(st, v);
}

// --- the specified skolemize rule (strict profile) ----------------------
// ProoVer 2026 rules: a skolemization step must use the rule name
// skolemize; have status esa; introduce exactly one new Skolem symbol,
// declared with new_symbols(skolem, [sK]); indicate with
// skolemize(Var, sK(...)) the existentially quantified variable it
// eliminates and the Skolem term replacing it; the Skolem term must
// depend on at least the universally quantified variables appearing in
// the skolemized existential formula; and the result must be a correct
// skolemization of its parent.  Every obligation is syntactic, so each
// failure is demonstrated -- including skolemizations that happen to
// be semantically harmless (PRV083 eliminates a UNIVERSAL, PRV084 an
// existential of negative polarity, PRV095 puts the witness where the
// vacuous binder had no occurrence, PRV092 cites two parents, PRV093
// omits the annotation, PRV038/PRV058 claim status thm).
//
// Candidate positions: quantifier nodes of the parent binding a
// variable NAMED Var whose effective polarity is existential (a
// positive ? or a negative !; nothing under <=>).  Each is tried: the
// Skolem term is parsed in the scope of that node (its arguments must
// name binders in scope, or free variables), and the expected result
// -- the parent with that one quantifier removed and Var := the Skolem
// term -- must be alpha-equivalent to the step's formula.
typedef struct {
  Term node;                 // the eliminated quantifier node
  int  scope[PC_MAX_RVARS];  // binder ids on the path, outermost first
  u8   univ[PC_MAX_RVARS];   // ... universal in polarity?
  int  nscope;
} PcSkCand;
#define PC_SK_MAXCAND 16
static PcSkCand pc_skc[PC_SK_MAXCAND];
static int pc_nskc;
static int pc_skc_trunc;     // candidate search hit a bound: never INVALID
static const char *pc_sk_varname;
// pol: +1 positive, -1 negative, 0 both (under <=>).
static void pc_sk_find(Term f, int pol, int *scope, u8 *univ, int ns) {
  if (term_tag(f) != TAG_CTR) return;
  u32 lab = term_ext(f);
  switch (lab) {
    case PC_LAB_NOT: pc_sk_find(term_ctr_at(f, 0), -pol, scope, univ, ns); return;
    case PC_LAB_AND: case PC_LAB_OR:
      pc_sk_find(term_ctr_at(f, 0), pol, scope, univ, ns);
      pc_sk_find(term_ctr_at(f, 1), pol, scope, univ, ns);
      return;
    case PC_LAB_IMP:
      pc_sk_find(term_ctr_at(f, 0), -pol, scope, univ, ns);
      pc_sk_find(term_ctr_at(f, 1), pol, scope, univ, ns);
      return;
    case PC_LAB_IFF:
      pc_sk_find(term_ctr_at(f, 0), 0, scope, univ, ns);
      pc_sk_find(term_ctr_at(f, 1), 0, scope, univ, ns);
      return;
    case PC_LAB_ALL: case PC_LAB_EX: {
      Term var = term_ctr_at(f, 0);
      u32 vid = term_ext(var);
      int is_ex = (lab == PC_LAB_EX);
      int exist = (pol == 1 && is_ex) || (pol == -1 && !is_ex);
      int unive = (pol == 1 && !is_ex) || (pol == -1 && is_ex);
      if (exist && vid < PC_MAX_RVARS && strcmp(pc_vname[vid], pc_sk_varname) == 0) {
        if (pc_nskc < PC_SK_MAXCAND) {
          PcSkCand *c = &pc_skc[pc_nskc++];
          c->node = f;
          c->nscope = ns;
          for (int i = 0; i < ns; i++) { c->scope[i] = scope[i]; c->univ[i] = univ[i]; }
        } else {
          pc_skc_trunc = 1;
        }
      }
      if (ns < PC_MAX_RVARS) {
        scope[ns] = (int)vid;
        univ[ns] = (u8)unive;
        pc_sk_find(term_ctr_at(f, 1), pol, scope, univ, ns + 1);
      } else {
        pc_skc_trunc = 1;
      }
      return;
    }
    default: return;
  }
}
// Substitute variable id `vid` by `repl` (ids are binder-unique after
// parsing, so no capture can arise).
static Term pc_subst_var(Term t, u32 vid, Term repl) {
  if (term_tag(t) == TAG_FVR) return term_ext(t) == vid ? repl : t;
  if (term_tag(t) != TAG_CTR) return t;
  u32 n = term_ctr_n(t);
  if (n == 0) return t;
  Term kids[REWRITE_MAX_ARITY];
  if (n > REWRITE_MAX_ARITY) return t;
  int ch = 0;
  for (u32 i = 0; i < n; i++) {
    Term c = term_ctr_at(t, i);
    kids[i] = pc_subst_var(c, vid, repl);
    if (kids[i] != c) ch = 1;
  }
  return ch ? term_new_ctr(term_ext(t), kids, n) : t;
}
// Replace the node `target` (by identity) inside f with `repl`.
static Term pc_replace_node(Term f, Term target, Term repl, int *done) {
  if (f == target) { *done = 1; return repl; }
  if (term_tag(f) != TAG_CTR) return f;
  u32 n = term_ctr_n(f);
  if (n == 0) return f;
  Term kids[REWRITE_MAX_ARITY];
  if (n > REWRITE_MAX_ARITY) return f;
  int ch = 0;
  for (u32 i = 0; i < n; i++) {
    Term c = term_ctr_at(f, i);
    kids[i] = (*done) ? c : pc_replace_node(c, target, repl, done);
    if (kids[i] != c) ch = 1;
  }
  return ch ? term_new_ctr(term_ext(f), kids, n) : f;
}
static int pc_var_free_in(Term t, u32 vid) {
  if (term_tag(t) == TAG_FVR) return term_ext(t) == vid;
  if (term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) if (pc_var_free_in(term_ctr_at(t, i), vid)) return 1;
  return 0;
}

static int pc_check_skolemize_spec(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  if (st->st_mask != (1u << PC_ST_ESA)) {
    *why = "skolemize step must have status esa (and no other status)";
    return PC_INVALID;
  }
  if (st->n_parents != 1 || st->par_compound) {
    *why = "skolemize step must cite exactly one parent";
    return PC_INVALID;
  }
  if (st->n_ns_ann != 1 || st->ns_kind != PC_NS_SKOLEM || st->n_new_syms != 1) {
    *why = "skolemize step must declare exactly one symbol with new_symbols(skolem, [sK])";
    return PC_INVALID;
  }
  if (st->n_sk_ann != 1 || st->sk_var == NULL || st->sk_term == NULL) {
    *why = "skolemize step lacks its skolemize(Var, sK(...)) annotation";
    return PC_INVALID;
  }
  int pidx = st->par[0];
  if (pidx < 0) { *why = "skolemize parent missing"; return PC_UNKNOWN; }
  // Interned on demand: a vacuous binder's Skolem symbol legitimately
  // occurs in no formula at all (PRV096 eliminates ?[Y] from p(a)).
  u32 L = pc_intern(st->new_syms[0]);
  if (L == PC_NOT_FOUND) { *why = "symbol table full"; return PC_UNKNOWN; }
  // Re-parse the parent with its variable NAMES (the annotation speaks
  // in them).  pc_vname then names the parent's ids until the next parse.
  const PcStep *ps = &pc_steps[pidx];
  PcParse sc;
  Term P = (ps->ftext != NULL) ? pc_parse_formula(ps->ftext, &sc) : 0;
  if (P == 0) { *why = "skolemize parent could not be re-read"; return PC_UNKNOWN; }
  static int pfree[PC_MAX_RVARS];
  int npfree = sc.nfree;
  for (int i = 0; i < npfree; i++) pfree[i] = pc_free_id[i];
  pc_nskc = 0;
  pc_skc_trunc = 0;
  pc_sk_varname = st->sk_var;
  {
    static int scope[PC_MAX_RVARS];
    static u8 univ[PC_MAX_RVARS];
    pc_sk_find(P, 1, scope, univ, 0);
  }
  if (pc_nskc == 0) {
    *why = "skolemize(Var, ...) names no existentially quantified variable of the parent";
    return pc_skc_trunc ? PC_UNKNOWN : PC_INVALID;
  }
  const char *fail = "result is not the parent with the variable replaced by the Skolem term";
  for (int c = 0; c < pc_nskc; c++) {
    PcSkCand *cd = &pc_skc[c];
    Term T = pc_parse_term_ctx(st->sk_term, cd->scope, cd->nscope, pfree, npfree);
    if (T == 0) { fail = "Skolem term uses a variable not in scope"; continue; }
    if (term_tag(T) != TAG_CTR || term_ext(T) != L) {
      fail = "Skolem term's head is not the declared symbol";
      continue;
    }
    int args_ok = 1;
    for (u32 i = 0; i < term_ctr_n(T); i++)
      if (term_tag(term_ctr_at(T, i)) != TAG_FVR) args_ok = 0;
    if (!args_ok) { fail = "Skolem term arguments must be variables"; continue; }
    // Dependency: every universally quantified variable in scope (or
    // free in the parent) occurring free in the eliminated subformula
    // must be an argument of the Skolem term.
    int dep_ok = 1;
    for (int i = 0; i < cd->nscope + npfree && dep_ok; i++) {
      int vid = (i < cd->nscope) ? cd->scope[i] : pfree[i - cd->nscope];
      int uni = (i < cd->nscope) ? cd->univ[i] : 1;
      if (!uni || !pc_var_free_in(cd->node, (u32)vid)) continue;
      int hit = 0;
      for (u32 a = 0; a < term_ctr_n(T) && !hit; a++)
        hit = (term_ext(term_ctr_at(T, a)) == (u32)vid);
      if (!hit) dep_ok = 0;
    }
    if (!dep_ok) { fail = "Skolem term omits a universally quantified variable it depends on"; continue; }
    Term body = term_ctr_at(cd->node, 1);
    u32 vid = term_ext(term_ctr_at(cd->node, 0));
    int done = 0;
    Term expect = pc_replace_node(P, cd->node, pc_subst_var(body, vid, T), &done);
    if (!done) continue;
    if (pc_alpha_eq(expect, st->formula) ||
        pc_leaf_syn_eq(pc_close(expect), pc_close(st->formula))) {
      int fv = pc_sk_fresh(idx, L, 1, why);
      return fv;                          // VALID when fresh, else INVALID/UNKNOWN
    }
  }
  *why = fail;
  return pc_skc_trunc ? PC_UNKNOWN : PC_INVALID;
}

// Does `f` carry a symbol absent from all of the step's parents and from
// the problem file (a symbol the step could be introducing)?
static int pc_has_fresh_symbol(const PcStep *st) {
  static int *stamp = NULL;
  static int stamp_cap = 0;
  static int *buf = NULL;
  if (stamp_cap < pc_nsyms + 1) {
    free(stamp); free(buf);
    stamp_cap = pc_nsyms + 1;
    stamp = (int *)malloc((size_t)stamp_cap * sizeof(int));
    buf = (int *)malloc((size_t)stamp_cap * sizeof(int));
    if (stamp == NULL || buf == NULL) { stamp_cap = 0; return 1; }
  }
  for (int s = 0; s < pc_nsyms; s++) stamp[s] = -1;
  int n = 0;
  pc_collect_labels(st->formula, stamp, 1, buf, &n, pc_nsyms);
  for (int k = 0; k < n; k++) {
    u32 L = PC_LAB_BASE + (u32)buf[k];
    if (pc_label_in_prob(L)) continue;
    int in_par = 0;
    for (int q = 0; q < st->n_parents && !in_par; q++) {
      int p = st->par[q];
      if (p >= 0 && pc_step_has(p, L)) in_par = 1;
    }
    if (!in_par) return 1;
  }
  return 0;
}

// Semantic check by the step's status claim(s):
//   thm (alone or among others) : parents |= conclusion; a countermodel
//                                 refutes the claim.
//   esa                         : proved entailment confirms; when the
//                                 conclusion carries NO symbol new to its
//                                 parents and the problem, there is no
//                                 fresh symbol whose interpretation could
//                                 extend a model, so satisfiability
//                                 preservation IS entailment: a
//                                 countermodel refutes (PRV097's trivial
//                                 p(a) |- q(a)).
//   cth                         : parents |= ~conclusion is the claim; the
//                                 step confirms only as the negation of a
//                                 conjecture (anything else would smuggle
//                                 a hypothesis into the refutation).
//   no status                   : proved entailment confirms, nothing refutes.
static int pc_check_entail(const PcStep *st, const char **why) {
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np)) { *why = "parent missing"; return PC_UNKNOWN; }
  // A premise-free step can only claim a theorem of NOTHING -- a valid
  // formula (PRV076's `tautology` record): checked as |= conclusion.
  if (np < 1 && (st->par_compound || !(st->st_mask & (1u << PC_ST_THM)))) {
    *why = "parent missing";
    return PC_UNKNOWN;
  }
  if ((st->st_mask & (1u << PC_ST_CTH)) && !(st->st_mask & (1u << PC_ST_THM))) {
    Term nc = pc_is_false(st->formula) ? term_new_ctr(PC_LAB_TRUE, NULL, 0)
                                       : fol_mk_not(st->formula);
    int v = pc_entails(par, np, nc);
    if (v == PC_INVALID) {
      *why = "cth claim refuted: the premises do not entail the conclusion's negation";
      return pc_demote_partial(st, v);
    }
    int conj_par = (np == 1 && st->par[0] >= 0 && pc_steps[st->par[0]].role == PC_ROLE_CONJ);
    if (!conj_par) {
      // A cth step brings a HYPOTHESIS (its premise's negation) into
      // the refutation -- legitimate only for the conjecture.  Negating
      // an axiom or a derived formula "refutes" something the proof was
      // never asked to refute: the specified negated_conjecture rule's
      // parent requirement, holding for every cth claim.
      for (int k = 0; k < st->n_parents; k++) {
        int p = st->par[k];
        if (p >= 0 && pc_steps[p].role != PC_ROLE_CONJ) {
          *why = "cth step negates a premise that is not the conjecture";
          return PC_INVALID;
        }
      }
      *why = "cth step whose premise is not the conjecture";
      return PC_UNKNOWN;
    }
    // Fresh handles: a rooted session inside pc_entails may have
    // relocated the heap (pc_steps is refreshed; this frame's are not).
    int e = pc_negation_equiv(pc_steps[st->par[0]].formula, st->formula);
    if (e == PC_VALID) return PC_VALID;
    *why = "cth step is not the negation of the conjecture";
    return PC_UNKNOWN;
  }
  Term concl = pc_is_false(st->formula) ? 0 : st->formula;
  int v = pc_entails(par, np, concl);
  *why = "conclusion is not entailed by the parents";
  if (v == PC_INVALID) {
    int claims_thm = (st->st_mask & (1u << PC_ST_THM)) != 0;
    int esa_closed = (st->st_mask & (1u << PC_ST_ESA)) && !pc_has_fresh_symbol(st);
    if (!claims_thm && !esa_closed) {
      // A countermodel of parents |= conclusion for an esa step that may
      // introduce a symbol, or an unannotated step, demonstrates nothing
      // -- judging esa steps by entailment is exactly how the 2026 entry
      // turned valid prover output into false VerifiedBads.
      *why = "step does not claim entailment; left unconfirmed";
      v = PC_UNKNOWN;
    } else if (!claims_thm) {
      *why = "esa step introduces no new symbol yet is not entailed by its parents";
    }
  }
  return pc_demote_partial(st, v);
}

static int pc_check_step(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  if (st->rule == R_INPUT) return pc_check_leaf(idx, why);
  // A cited plain name that resolves to no record justifies the step by
  // a premise that does not exist.  Strict profile: when the name is
  // not a problem-file formula either, that is demonstrated (PRV025's
  // `ghost`); otherwise abstain.
  for (int k = 0; k < st->n_parents; k++) {
    if (st->par[k] >= 0) continue;
    if (pc_strict() && pc_prob_state == 1 && !pc_prob_unparsed &&
        pc_prob_find(st->parents[k]) < 0) {
      *why = "cites a parent that is no record of the proof";
      return PC_INVALID;
    }
    *why = "parent missing";
    return PC_UNKNOWN;
  }
  switch (st->rule) {
    case R_INSTANTIATE: return pc_check_instantiate(st, why);
    case R_NEGCONJ:     return pc_check_negconj(st, why);
    case R_SKOLEMIZE:
      return pc_strict() ? pc_check_skolemize_spec(idx, why)
                         : pc_check_skolemize(idx, 1, why);
    case R_CONSEQUENCE:
    case R_DEDUCTION:
    case R_HORN:        return pc_check_entail(st, why);
    default:
      // Foreign rule names (other provers' vocabularies, the equational
      // engine's own cp / orient / simp records).  An esa step that
      // annotates its Skolem symbol gets the positive skolemization
      // check first; then the semantic route by status claim, which can
      // CONFIRM any step but refutes only a claim it contradicts
      // (pc_check_entail).  An unsettled step returns PC_UNKNOWN
      // (abstain), never a false VerifiedGood.
      if ((st->st_mask & (1u << PC_ST_ESA)) && st->new_sym[0] != '\0') {
        int v = pc_check_skolemize(idx, 0, why);
        if (v != PC_UNKNOWN) return v;
      }
      return pc_check_entail(st, why);
  }
}

// ===================================================================
// external-ATP hook (strict profile)
// ===================================================================
//
// The ProoVer rules permit validating an unspecified inference step by
// invoking an external ATP -- but only for steps with status thm or
// cth, and the ATP may see ONLY that step and its stated premises
// (re-checking the whole conjecture is forbidden).  The oracle here is
// this very binary re-exec'd in prover mode (THVM_CHECKPROOF_PROVE=1):
// the full FindProof CLI pipeline -- equational completion plus the FOF
// clausal front -- is far stronger than the small per-step saturation
// above.  A Theorem/Unsatisfiable answer confirms the step; a
// CounterSatisfiable/Satisfiable answer on a thm claim is a concrete
// countermodel witness (VerifiedBad); anything else abstains.

static double pc_wall_now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}
static volatile pid_t g_pc_hook_child = 0;     // killed by the wall alarm

static unsigned long long pc_fnv(const char *s) {
  unsigned long long h = 1469598103934665603ull;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ull; }
  return h;
}

// TPTP renderers for the obligation file.  Terms print with the intern
// table's names (re-quoted when not plain lower_words); variables print
// as V<id>, which pc_close has bound.
static void pc_print_sym(FILE *fp, u32 lab) {
  if (lab == PC_LAB_FALSE) { fputs("$false", fp); return; }
  if (lab == PC_LAB_TRUE)  { fputs("$true", fp);  return; }
  if (lab >= PC_LAB_BASE && lab < PC_LAB_BASE + (u32)pc_nsyms) {
    const char *nm = pc_sym[lab - PC_LAB_BASE];
    int plain = (nm[0] != '\0') &&
                (islower((unsigned char)nm[0]) ||
                 isdigit((unsigned char)nm[0]) || nm[0] == '$');
    for (const char *q = nm; plain && *q; q++)
      if (!(isalnum((unsigned char)*q) || *q == '_' || *q == '$')) plain = 0;
    if (plain) { fputs(nm, fp); return; }
    fputc('\'', fp);
    for (const char *q = nm; *q; q++) {
      if (*q == '\'' || *q == '\\') fputc('\\', fp);
      fputc(*q, fp);
    }
    fputc('\'', fp);
    return;
  }
  fprintf(fp, "l%u", (unsigned)lab);
}
static void pc_print_tm(FILE *fp, Term t) {
  if (term_tag(t) == TAG_FVR) { fprintf(fp, "V%u", (unsigned)term_ext(t)); return; }
  if (term_tag(t) != TAG_CTR) { fputs("unprintable", fp); return; }
  pc_print_sym(fp, term_ext(t));
  u32 n = term_ctr_n(t);
  if (n == 0) return;
  fputc('(', fp);
  for (u32 i = 0; i < n; i++) {
    if (i) fputc(',', fp);
    pc_print_tm(fp, term_ctr_at(t, i));
  }
  fputc(')', fp);
}
static void pc_print_fof(FILE *fp, Term f) {
  if (term_tag(f) == TAG_CTR) {
    u32 lab = term_ext(f);
    if (lab == PC_LAB_NOT) {
      fputs("~(", fp);
      pc_print_fof(fp, term_ctr_at(f, 0));
      fputc(')', fp);
      return;
    }
    if (lab == PC_LAB_AND || lab == PC_LAB_OR ||
        lab == PC_LAB_IMP || lab == PC_LAB_IFF) {
      const char *op = (lab == PC_LAB_AND) ? " & "
                     : (lab == PC_LAB_OR)  ? " | "
                     : (lab == PC_LAB_IMP) ? " => " : " <=> ";
      fputc('(', fp);
      pc_print_fof(fp, term_ctr_at(f, 0));
      fputs(op, fp);
      pc_print_fof(fp, term_ctr_at(f, 1));
      fputc(')', fp);
      return;
    }
    if (lab == PC_LAB_ALL || lab == PC_LAB_EX) {
      fprintf(fp, "%c [V%u] : (", (lab == PC_LAB_ALL) ? '!' : '?',
              (unsigned)term_ext(term_ctr_at(f, 0)));
      pc_print_fof(fp, term_ctr_at(f, 1));
      fputc(')', fp);
      return;
    }
    if (lab == PC_LAB_EQ && term_ctr_n(f) == 2) {
      fputc('(', fp);
      pc_print_tm(fp, term_ctr_at(f, 0));
      fputs(" = ", fp);
      pc_print_tm(fp, term_ctr_at(f, 1));
      fputc(')', fp);
      return;
    }
  }
  pc_print_tm(fp, f);
}

// Run the oracle on `axioms |= conj` (conj == 0 encodes $false: the
// axioms alone must be unsatisfiable).  Returns PC_VALID / PC_INVALID /
// PC_UNKNOWN.  Decided results are cached under THVM_PROOFCHECK_CACHE
// when set (validation sweeps); timeouts are never cached -- they
// depend on the budget.
static int pc_atp_call(Term *axioms, int nax, Term conj, int budget_s) {
  const char *atp = getenv("THVM_PROOFCHECK_ATP");
  if (atp != NULL && (atp[0] == '\0' || strcmp(atp, "0") == 0))
    return PC_UNKNOWN;                       // hook disabled
  if (atp == NULL) {
    if (!pc_strict() || pc_self_path == NULL) return PC_UNKNOWN;
    atp = pc_self_path;
  }
  char *obuf = NULL;
  size_t olen = 0;
  FILE *ms = open_memstream(&obuf, &olen);
  if (ms == NULL) return PC_UNKNOWN;
  for (int i = 0; i < nax; i++) {
    fprintf(ms, "fof(p%d, axiom, ", i);
    pc_print_fof(ms, pc_close(axioms[i]));
    fputs(").\n", ms);
  }
  fputs("fof(goal, conjecture, ", ms);
  if (conj != 0) pc_print_fof(ms, pc_close(conj));
  else           fputs("$false", ms);
  fputs(").\n", ms);
  fclose(ms);
  if (obuf == NULL) return PC_UNKNOWN;

  char cpath[1200];
  cpath[0] = '\0';
  const char *cdir = getenv("THVM_PROOFCHECK_CACHE");
  if (cdir != NULL && cdir[0] != '\0') {
    snprintf(cpath, sizeof cpath, "%s/%016llx", cdir, pc_fnv(obuf));
    FILE *cf = fopen(cpath, "r");
    if (cf != NULL) {
      int c = fgetc(cf);
      fclose(cf);
      free(obuf);
      return (c == 'V') ? PC_VALID : (c == 'I') ? PC_INVALID : PC_UNKNOWN;
    }
  }

  char tmpl[1240];
  const char *tdir = getenv("TMPDIR");
  snprintf(tmpl, sizeof tmpl, "%s/pc_ob_XXXXXX",
           (tdir != NULL && tdir[0] != '\0') ? tdir : "/tmp");
  int fd = mkstemp(tmpl);
  if (fd < 0) { free(obuf); return PC_UNKNOWN; }
  ssize_t w = write(fd, obuf, olen);
  close(fd);
  if (w != (ssize_t)olen) { unlink(tmpl); free(obuf); return PC_UNKNOWN; }

  int pfd[2];
  int v = PC_UNKNOWN;
  if (pipe(pfd) == 0) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
      dup2(pfd[1], 1);
      close(pfd[0]);
      close(pfd[1]);
      int devnull = open("/dev/null", O_RDWR);
      if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 2); }
      setenv("THVM_CHECKPROOF_PROVE", "1", 1);       // self-exec: prover mode
      setenv("THVM_ATP_RSS_ABORT_MB", "4000", 0);    // bound the oracle
      char wallbuf[32];
      snprintf(wallbuf, sizeof wallbuf, "--wall=%d", budget_s);
      signal(SIGALRM, SIG_DFL);
      alarm((unsigned)budget_s + 3u);                // belt over the --wall
      execl(atp, atp, "--szs", wallbuf, tmpl, (char *)NULL);
      _exit(127);
    }
    close(pfd[1]);
    if (pid > 0) {
      g_pc_hook_child = pid;
      char out[8192];
      size_t got = 0;
      ssize_t r;
      while ((r = read(pfd[0], out + got, sizeof out - 1 - got)) > 0) {
        got += (size_t)r;
        if (got >= sizeof out - 1) {
          char drain[4096];
          while (read(pfd[0], drain, sizeof drain) > 0) {}
          break;
        }
      }
      out[got] = '\0';
      close(pfd[0]);
      int status = 0;
      waitpid(pid, &status, 0);
      g_pc_hook_child = 0;
      const char *sz = strstr(out, "SZS status ");
      if (sz != NULL) {
        sz += 11;
        if (strncmp(sz, "Theorem", 7) == 0 ||
            strncmp(sz, "Unsatisfiable", 13) == 0 ||
            strncmp(sz, "ContradictoryAxioms", 19) == 0) v = PC_VALID;
        else if (strncmp(sz, "CounterSatisfiable", 18) == 0 ||
                 strncmp(sz, "Satisfiable", 11) == 0) v = PC_INVALID;
      }
    } else {
      close(pfd[0]);
    }
  }
  unlink(tmpl);
  if (cpath[0] != '\0' && v != PC_UNKNOWN) {
    FILE *cf = fopen(cpath, "w");
    if (cf != NULL) {
      fputc(v == PC_VALID ? 'V' : 'I', cf);
      fclose(cf);
    }
  }
  free(obuf);
  return v;
}

// Discharge one unsettled derived step through the oracle.  Gates: a
// thm status (entailment IS the claim), or a negation of the
// conjecture (the specified negated_conjecture rule, or a cth claim on
// a single conjecture parent: checked as ~parent <=> conclusion);
// parents resolvable; wall budget remaining.  esa steps NEVER reach the
// ATP (the rules forbid it); a foreign cth step's countermodel is not
// treated as a violation, nor is a countermodel over a partial premise
// set.
static int *pc_pending_after = NULL;    // derived steps at or after idx
static void pc_pending_build(void) {
  free(pc_pending_after);
  pc_pending_after = (int *)malloc(((size_t)pc_nstep + 1) * sizeof(int));
  if (pc_pending_after == NULL) return;
  pc_pending_after[pc_nstep] = 0;
  for (int j = pc_nstep - 1; j >= 0; j--)
    pc_pending_after[j] = pc_pending_after[j + 1] + (pc_steps[j].rule != R_INPUT);
}
static int pc_atp_discharge_step(int idx, const char **why) {
  const PcStep *st = &pc_steps[idx];
  if (st->rule == R_INPUT || st->rule == R_SKOLEMIZE) return PC_UNKNOWN;
  Term par[PC_MAX_PARENTS];
  int np;
  if (!pc_parent_formulas(st, par, &np)) return PC_UNKNOWN;
  if (np < 1 && (st->par_compound || !(st->st_mask & (1u << PC_ST_THM)))) return PC_UNKNOWN;
  double rem = (g_pc_deadline > 0.0) ? g_pc_deadline - pc_wall_now() : 8.0;
  if (rem < 1.5) return PC_UNKNOWN;
  int pending = (pc_pending_after != NULL) ? pc_pending_after[idx] : 1;
  if (pending < 1) pending = 1;
  int budget = (int)(rem / (double)pending);
  if (budget < 1) budget = 1;
  if (budget > 8) budget = 8;
  int thm = (st->st_mask & (1u << PC_ST_THM)) != 0;
  int cth = (st->st_mask & (1u << PC_ST_CTH)) != 0;
  if (st->rule == R_NEGCONJ || (cth && !thm)) {
    if (np != 1 || st->par[0] < 0 || pc_steps[st->par[0]].role != PC_ROLE_CONJ)
      return PC_UNKNOWN;
    Term equiv = fol_mk_iff(fol_mk_not(pc_close(par[0])),
                            pc_close(st->formula));
    int v = pc_atp_call(NULL, 0, equiv, budget);
    if (v == PC_INVALID) {
      if (st->rule != R_NEGCONJ) return PC_UNKNOWN;  // foreign cth: abstain
      *why = "external ATP: not the negation of the conjecture";
    }
    return v;
  }
  if (!thm) return PC_UNKNOWN;
  Term concl = pc_is_false(st->formula) ? 0 : st->formula;
  int v = pc_atp_call(par, np, concl, budget);
  if (v == PC_INVALID) {
    if (st->par_compound) return PC_UNKNOWN;
    *why = "external ATP found the step counter-satisfiable";
  }
  return v;
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

// Parse every '.'-terminated record of the (comment-stripped) proof
// text into pc_steps, counting unparseable content-bearing segments in
// pc_nskipped.  Restores the text bytes it NUL-patches, so it can run
// again on the same buffer.
static void pc_parse_all(char *text) {
  pc_nstep = 0;
  int depth = 0, inq = 0;
  char *start = text;
  char *p = text;
  for (; *p; p++) {
    if (inq) { if (*p == '\'') inq = 0; continue; }
    if (*p == '\'') { inq = 1; continue; }
    if (*p == '(' || *p == '[') depth++;
    else if (*p == ')' || *p == ']') depth--;
    else if (*p == '.' && depth == 0) {
      char save = *p;
      *p = '\0';
      int has_content = 0;
      for (const char *q = start; q < p; q++)
        if (!isspace((unsigned char)*q)) { has_content = 1; break; }
      if (has_content && !pc_parse_record(start)) pc_nskipped++;
      *p = save;
      start = p + 1;
    }
  }
  for (const char *q = start; *q; q++)           // unterminated trailing record
    if (!isspace((unsigned char)*q)) { pc_nskipped++; break; }
}

// Strict profile: load the problem table BEFORE the checking loop.  A
// lazy load inside the first leaf check sat ABOVE that step's heap
// watermark: the per-step rollback reclaimed the table's Terms, later
// sessions overwrote the memory, and subsequent leaf checks then
// "demonstrated" differences against recycled heap -- a false
// VerifiedBad on ANA034-10's own harvested proof.
static void pc_preload_problem(void) {
  if (!pc_strict()) return;
  for (int i = 0; i < pc_nstep; i++) {
    if (pc_steps[i].rule == R_INPUT && pc_steps[i].src_file[0] != '\0') {
      pc_load_problem(pc_steps[i].src_file);
      return;
    }
  }
}

// A collection ran OUTSIDE any rooted engine session (the strict leaf
// checks' refutations allocate on the shared heap): every parsed Term
// dangles, but the comment-stripped proof text is still live -- re-parse
// everything onto the post-collection heap and keep checking.  Verdicts
// already recorded are plain ints.  Returns 0 (caller abstains) when the
// re-parse does not reproduce the same step count.
static int pc_reparse(char *text) {
  int nstep0 = pc_nstep, nskip0 = pc_nskipped;
  pc_nskipped = 0;
  pc_parse_all(text);
  pc_index_steps();
  int ok = (pc_nstep == nstep0);
  pc_nskipped = nskip0;
  // The problem table dangles too: rebuild it NOW (the caller skips the
  // current step's watermark rollback after a collection, so these
  // allocations survive; a lazy reload inside a later step would not).
  pc_nprob = 0;
  pc_prob_state = 0;
  pc_prob_unparsed = 0;
  pc_prob_directive[0] = '\0';
  pc_preload_problem();
  return ok;
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
// unwinds here and the proof abstains -- Unknown -- rather than the
// process dying with no verdict.  Because ProoVer runs one proof per
// process, a whole-proof abstain on exhaustion is the sound response.
static char g_pc_alarm_line[1300];
static void pc_worker_alarm(int sig) {
  (void)sig;
  pid_t c = g_pc_hook_child;          // reap a live oracle child first
  if (c > 0) kill(c, SIGKILL);
  size_t n = 0;
  while (g_pc_alarm_line[n] != '\0') n++;
  if (n > 0) { ssize_t w = write(1, g_pc_alarm_line, n); (void)w; }
  _exit(0);
}

static int proof_check_worker(const char *file, int szs) {
  thvm_init();
  const char *cap = getenv("THVM_PROOFCHECK_CAP");
  if (cap != NULL) { long v = atol(cap); if (v > 0) pc_cap = (u32)v; }
  const char *base = strrchr(file, '/');
  base = (base != NULL) ? base + 1 : file;
  // Hard time bound INSIDE the worker: the forked wrapper's timeout
  // covers the normal path, but when fork is unavailable the worker
  // runs inline and a long entailment saturation would otherwise be
  // unbounded.  On fire: abstain (Unknown) -- sound at zero points.
  {
    unsigned wsec = 25;
    const char *w = getenv("THVM_PROOFCHECK_WALL");
    if (w != NULL) { long v = atol(w); if (v > 0 && v < 100000) wsec = (unsigned)v; }
    if (szs) snprintf(g_pc_alarm_line, sizeof g_pc_alarm_line,
                      "%% SZS status Unknown for %s\n", base);
    else     snprintf(g_pc_alarm_line, sizeof g_pc_alarm_line,
                      "Unknown\t%s\n", base);
    signal(SIGALRM, pc_worker_alarm);
    alarm(wsec);
    // Absolute budget the external-ATP hook spends down; the margin
    // keeps the final verdict line inside the alarm.
    g_pc_deadline = pc_wall_now() + (double)wsec - 2.5;
  }

  char *text = pc_read(file, NULL);
  if (text == NULL) {
    if (szs) printf("%% SZS status Error for %s\n", base);
    fprintf(stderr, "FindProof: cannot open %s\n", file);
    return 2;
  }
  pc_strip_comments(text);
  // The strict profile resolves file() directive paths relative to the
  // proof file's directory (the samples' layout: Problems/ beside the
  // proof).
  {
    const char *sl = strrchr(file, '/');
    if (sl != NULL && (size_t)(sl - file) < sizeof pc_proof_dir) {
      memcpy(pc_proof_dir, file, (size_t)(sl - file));
      pc_proof_dir[sl - file] = '\0';
    } else {
      pc_proof_dir[0] = '\0';
    }
  }

  pc_bad = -1; pc_any_unknown = 0; pc_saw_false = 0; pc_bad_why = "";
  pc_nskipped = 0; pc_dangling = 0; pc_sym_reset();
  pc_fresh_binders = 1;
  pc_nprob = 0; pc_prob_state = 0; pc_prob_unparsed = 0;
  pc_prob_directive[0] = '\0';
  const char *verdict;

  thvm_heap_exhausted = 0;
  thvm_heap_exhaust_jmp = &pc_oom_jb;
  if (setjmp(pc_oom_jb) != 0) {
    // Heap exhausted mid-check.  A step already confirmed unsound stays
    // a bug; otherwise abstain.
    thvm_heap_exhaust_jmp = NULL;
    verdict = (pc_bad >= 0) ? "VerifiedBad" : "Unknown";
  } else {
    // Split into '.'-terminated records (respecting quotes / brackets)
    // and parse each IN PLACE -- the old fixed 64K copy buffer silently
    // truncated large records.  Every content-bearing segment that does
    // not become a step is COUNTED: an unread record caps the verdict
    // at Unknown (the 2026 entry dropped such records silently and
    // verified the remainder -- a confirmed false-accept hole).
    pc_parse_all(text);
    pc_index_steps();
    pc_preload_problem();
    pc_sym_index_build();
    pc_pending_build();
    if (getenv("THVM_PROOFCHECK_DBG") != NULL) {
      fprintf(stderr, "PCDBG loaded steps=%d skipped=%d nprob=%d\n",
              pc_nstep, pc_nskipped, pc_nprob);
    }

    // Structural verification first: unique names and an acyclic
    // citation graph.  Both violations are demonstrated (organizer-
    // grade bad proofs), and full-range parent resolution below is only
    // sound once cycles are excluded.
    {
      const char *swhy = NULL;
      int sbad = -1;
      if (!pc_check_structure(&swhy, &sbad)) {
        pc_bad = sbad;
        pc_bad_why = swhy;
      } else if (!pc_check_conj_premise(&swhy, &sbad)) {
        pc_bad = sbad;
        pc_bad_why = swhy;
      }
    }

    // Validate every step (inputs included: the strict profile verifies
    // leaves against the problem file); report one verdict.
    int pc_retry_idx = -1;   // step already redone after a relocation
    for (int i = 0; i < pc_nstep && pc_bad < 0; i++) {
      const char *why = "";
      // Per-step arena discipline: pc_entails replays a whole engine
      // refutation per step on the global heap; without reclamation a
      // ~30-step equational proof exhausts from-space mid-check (LAT171-1)
      // and the isolation child dies -> blanket Unknown.  Nothing a
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
        g_pc_cur_idx = i;
        v = pc_check_step(i, &why);
        g_pc_cur_idx = -1;
        // Unsettled thm/cth step: per the rules, discharge it through
        // the external ATP (this binary re-exec'd as prover) -- the one
        // mechanism that turns an unverifiable step into a decided one.
        if (v == PC_UNKNOWN) {
          int hv = pc_atp_discharge_step(i, &why);
          if (hv != PC_UNKNOWN) v = hv;
        }
      }
      thvm_heap_exhaust_jmp = _pc_saved;
      if (gc_count() == _pc_gc) {
        thvm_atp_heap_reset(_pc_hp);
      } else {
        // A collection moved the heap: every parsed formula Term in
        // pc_steps now dangles (they are not GC roots of the per-step
        // engine sessions).  Any verdict computed after this point could
        // be garbage -- including a WRONG VerifiedBad (observed on
        // LAT171-1 deep mode: the \$false closure step read a relocated
        // parent and 'refuted' it).  Abstain for the whole proof --
        // unless the collection ran inside a ROOTED eq session, whose
        // root slots hold the live formulas.
        if (g_pc_eq_gc_refreshed) {
          // Rooted eq-session collection, normal exit: formulas already
          // refreshed.
          g_pc_eq_gc_refreshed = 0;
        } else if (g_pc_roots_armed) {
          // Rooted eq session ended by a heap-exhaustion longjmp (the
          // per-step handler above): its exit refresh was skipped, but
          // the collector relocated g_pc_roots in place -- write the
          // live formulas back.  The abandoned session's litter still
          // fills from-space, so collect again with the formulas as the
          // ONLY roots (everything engine-internal is unreachable now)
          // and drop the address-keyed order memos; the remaining steps
          // then check on a clean heap instead of OOM-abstaining one by
          // one.
          // The root slots hold the STEP formulas and then the strict-
          // profile PROBLEM formulas (pc_fill_roots' layout): collect
          // BOTH regions -- rooting the steps alone left the problem
          // table dangling, and later leaf checks then "demonstrated"
          // differences against relocated garbage (a false VerifiedBad
          // on ANA034-10's own harvested proof).
          u32 nr = (u32)(pc_nstep < PC_MAX_STEPS ? pc_nstep : PC_MAX_STEPS)
                 + (u32)(pc_nprob < PC_MAX_STEPS ? pc_nprob : PC_MAX_STEPS);
          gc_collect(g_pc_roots, nr);
          thvm_lpo_invalidate();
          thvm_kbo_invalidate();
          pc_refresh_formulas(g_pc_roots);
        } else if (pc_reparse(text)) {
          // Unrooted collection (a strict leaf check's refutation, the
          // negconj FO route): everything re-parsed onto the live heap.
          // The verdict computed DURING the collection may have read
          // relocated Terms -- discard it in BOTH directions (a garbage
          // comparison can fabricate a countermodel OR a match).  Redo
          // the step ONCE on the fresh state; a second relocation in
          // the same step abstains on it.
          g_pc_roots_armed = 0;
          if (pc_retry_idx != i) {
            pc_retry_idx = i;
            i--;
            continue;
          }
          v = PC_UNKNOWN;
          why = "heap relocated mid-check";
        } else {
          pc_any_unknown = 1;
          pc_bad = -1;
          break;
        }
      }
      g_pc_roots_armed = 0;
      {
        static int pcdbg = -1;
        if (pcdbg < 0) pcdbg = getenv("THVM_PROOFCHECK_DBG") != NULL ? 1 : 0;
        if (pcdbg) {
          fprintf(stderr, "PCDBG step=%s rule=%d verdict=%s why=%s\n",
                  pc_steps[i].name, pc_steps[i].rule,
                  v == 0 ? "VALID" : v == 1 ? "INVALID" : "UNKNOWN", why);
        }
      }
      if (v == PC_VALID && pc_steps[i].rule != R_INPUT &&
          pc_is_false(pc_steps[i].formula)) pc_saw_false = 1;
      if (v == PC_INVALID) { pc_bad = i; pc_bad_why = why; break; }
      if (v == PC_UNKNOWN) pc_any_unknown = 1;
    }
    thvm_heap_exhaust_jmp = NULL;
    // Abstain-by-default assembly: VerifiedGood requires EVERY record
    // read and EVERY step confirmed, plus a confirmed derived $false.
    if (pc_bad >= 0)           verdict = "VerifiedBad";
    else if (pc_nskipped > 0)  verdict = "Unknown";  // unread records
    else if (pc_any_unknown)   verdict = "Unknown";
    else if (!pc_saw_false)    verdict = "Unknown";  // no refutation closure
    else                       verdict = "VerifiedGood";
  }

  alarm(0);
  if (szs) printf("%% SZS status %s for %s\n", verdict, base);
  else     printf("%s\t%s\n", verdict, base);
  if (pc_bad >= 0) printf("%% bad step: %s (%s)\n", pc_steps[pc_bad].name, pc_bad_why);
  fflush(stdout);
  return 0;
}

static void pc_alarm(int sig) { (void)sig; }   // interrupt waitpid only

// FindProof --check / CheckProof entry point.  Isolates the checking
// pass in a child process: whatever happens to it -- a clean verdict, a
// heap-exhaustion fatal, or a saturation that outlives the wall bound --
// the proof still gets exactly one SZS line.  The conservative fallback
// is Unknown (0 points): under ProoVer scoring a wrong VerifiedGood
// costs -10 and a wrong VerifiedBad costs -1, so no crash or timeout
// path may ever guess a verdict.
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
    if (szs) printf("%% SZS status Unknown for %s\n", base);
    else     printf("Unknown\t%s\n", base);
    fflush(stdout);
  }
  return 0;
}
