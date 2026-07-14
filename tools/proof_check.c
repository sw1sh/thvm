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
//   VerifiedGood        every derived step independently confirmed  (+1)
//   VerifiedBad  a step is provably unsound                   (+2)
//   Unknown     a step could not be settled -- abstain       ( 0)
//
// The scoring penalises accepting a bad proof far more than abstaining,
// so the checker is conservative: an INVALID verdict is emitted only when
// a step is CERTAIN to be unsound (a completed saturation exhibits a
// counter-model, a Skolem symbol is demonstrably not fresh), and anything
// undecided within the resource bound falls back to Unknown.
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
  K_REVIMP, K_XOR, K_STR, K_EOF
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
  // TPTP: the quantified body is a UNITARY formula ('~f', a nested
  // quantification, '(...)', or an atom).  Parsing a full formula here
  // swallowed any '&'/'|' continuation into the body -- for
  //   ! [X] : (p(X)) & ! [X] : (q(X))
  // the second quantifier nested INSIDE the first, and the shadowed X
  // then skolemized into a captured binder (KRS135+1's conjecture).
  Term body = pc_unary(p);
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
// formula := or [ ('=>'|'<=>'|'<='|'<~>') formula ]   (right-associative;
// TPTP forbids unparenthesised mixing, so one binary tail suffices)
static Term pc_formula(PcParse *p) {
  Term a = pc_or(p);
  if (pc_peek(p) == K_IMP) { pc_adv(p); return fol_mk_imp(a, pc_formula(p)); }
  if (pc_peek(p) == K_IFF) { pc_adv(p); return fol_mk_iff(a, pc_formula(p)); }
  if (pc_peek(p) == K_REVIMP) { pc_adv(p); return fol_mk_imp(pc_formula(p), a); }
  if (pc_peek(p) == K_XOR) { pc_adv(p); return fol_mk_not(fol_mk_iff(a, pc_formula(p))); }
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
// formula plus the current step's derived pair handles.  Registered as
// gc_extra_roots on each session so a mid-session collection relocates
// them IN PLACE; pc_steps and the locals are refreshed from it after.
static Term g_pc_roots[PC_MAX_STEPS + 2 * PC_MAX_PARENTS + 2];
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
  u32 nb = pc_fill_roots(g_pc_roots, PC_MAX_STEPS);
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

typedef struct {
  char name[PC_NAME];
  Term formula;
  int  rule;
  char parents[PC_MAX_PARENTS][PC_NAME];
  int  n_parents;
  char new_sym[PC_NAME];      // declared Skolem symbol, or ""
  int  is_conj;               // TPTP role was `conjecture`
} PcStep;

static PcStep pc_steps[PC_MAX_STEPS];
static int pc_nstep = 0;

// Collect the axiom (R_INPUT, plain-equation) pairs for the fallback
// entailment: an `orient` trace record's conclusion embodies push-side
// TREATMENT rewrites that are not separately recorded, so parents-only
// entailment can be genuinely insufficient; axioms + parent is still a
// sound (weaker-provenance) verification.
static u32 pc_axiom_pairs(Term *pl, Term *pr, u32 cap) {
  u32 n = 0;
  for (int i = 0; i < pc_nstep && n < cap; i++) {
    if (pc_steps[i].rule != R_INPUT) continue;
    Term l, r;
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
  int v = pc_check_cp_cfg(parents, np, cl, cr, 1);
  if (v == PC_UNKNOWN) v = pc_check_cp_cfg(parents, np, cl, cr, 0);
  return v;
}

static u32 pc_fill_roots(Term *buf, u32 cap) {
  u32 nb = 0;
  for (int i = 0; i < pc_nstep && nb < cap; i++) {
    buf[nb++] = pc_steps[i].formula;
  }
  return nb;
}
static void pc_refresh_formulas(const Term *buf) {
  for (int i = 0; i < pc_nstep && i < PC_MAX_STEPS; i++) {
    pc_steps[i].formula = buf[i];
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
  pc_trim(args[1]);
  st->is_conj = (strcmp(args[1], "conjecture") == 0);

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

// Gather the cited parents' formulas; returns 0 if any is missing.
// Parents must resolve to STRICTLY EARLIER steps: a derivation is a
// DAG rooted in the inputs, and accepting a forward or self reference
// would admit circular justification (a formula "derived" from its own
// descendants) -- a false VerifiedGood.
static int pc_parent_formulas(const PcStep *st, Term *out, int *nout) {
  int self = (int)(st - pc_steps);
  int n = 0;
  for (int i = 0; i < st->n_parents; i++) {
    int idx = -1;
    for (int j = 0; j < self; j++)
      if (strcmp(pc_steps[j].name, st->parents[i]) == 0) { idx = j; break; }
    if (idx < 0) return 0;
    if (n < PC_MAX_PARENTS) out[n++] = pc_steps[idx].formula;
  }
  *nout = n;
  return 1;
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
  // The parent must BE the conjecture: negating any other formula (an
  // axiom, say) yields a refutation of axioms-versus-their-own-negation
  // that proves nothing about the conjecture, yet can pass the
  // equivalence check below when the negations happen to align.
  {
    int self = (int)(st - pc_steps);
    int cidx = -1;
    for (int j = 0; j < self; j++)
      if (strcmp(pc_steps[j].name, st->parents[0]) == 0) { cidx = j; break; }
    if (cidx < 0 || !pc_steps[cidx].is_conj) {
      *why = "negated_conjecture parent is not the conjecture";
      return PC_INVALID;
    }
  }
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

// Collect the argument tuple of every L-headed subterm in `t`.  All
// occurrences must carry the SAME tuple of plain variables (a proper
// Skolem function is applied uniformly to its scope variables);
// anything else returns -1.  Returns 0 if no occurrence, 1 otherwise.
static int pc_sk_args(Term t, u32 L, u32 *av, u32 *na, int *have) {
  if (term_tag(t) != TAG_CTR) return 0;
  if (term_ext(t) == L) {
    u32 n = term_ctr_n(t);
    if (n > PC_MAX_VARS) return -1;
    u32 cur[PC_MAX_VARS];
    for (u32 i = 0; i < n; i++) {
      Term a = term_ctr_at(t, i);
      if (term_tag(a) != TAG_FVR) return -1;      // non-variable argument
      cur[i] = term_ext(a);
    }
    if (!*have) { *have = 1; *na = n; for (u32 i = 0; i < n; i++) av[i] = cur[i]; }
    else if (*na != n) return -1;
    else for (u32 i = 0; i < n; i++) if (av[i] != cur[i]) return -1;
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
  // The step must relate to its PARENT: freshness alone would let a
  // "skolemize" step smuggle in an arbitrary claim wrapped around a
  // fresh symbol.  Soundness condition: the parent entails the
  // conclusion with the Skolem terms abstracted to an existential
  // placed exactly where the Skolem's argument tuple binds -- then any
  // model of the parent interprets the fresh symbol as the witness
  // function, so the step is a conservative (esa) extension.
  Term par[PC_MAX_PARENTS]; int np;
  if (!pc_parent_formulas(st, par, &np) || np < 1) {
    *why = "skolemize parent missing";
    return PC_UNKNOWN;
  }
  u32 av[PC_MAX_VARS], na = 0;
  int have = 0;
  if (pc_sk_args(st->formula, L, av, &na, &have) < 0) {
    *why = "Skolem symbol applied non-uniformly";
    return PC_UNKNOWN;
  }
  u32 V = pc_max_var(st->formula) + 1u;
  Term abstracted = pc_sk_abstract(st->formula, L, V);
  int placed = 1;
  Term target = pc_sk_place(abstracted, V, av, na, 0, &placed);
  if (!placed) {
    *why = "no sound placement for the Skolem witness";
    return PC_UNKNOWN;
  }
  int v = pc_entails(par, np, target);
  if (v == PC_VALID) return PC_VALID;
  *why = (v == PC_INVALID)
       ? "conclusion is not a Skolemization of the parent"
       : "Skolemization could not be settled";
  return v;
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
      // PC_UNKNOWN (Unknown abstain), never a false VerifiedGood.
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
// unwinds here and the proof abstains -- Unknown -- rather than the
// process dying with no verdict.  Because ProoVer runs one proof per
// process, a whole-proof abstain on exhaustion is the sound response.
static char g_pc_alarm_line[1300];
static void pc_worker_alarm(int sig) {
  (void)sig;
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
  }

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
    verdict = (pc_bad >= 0) ? "VerifiedBad" : "Unknown";
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
          u32 nr = (u32)(pc_nstep < PC_MAX_STEPS ? pc_nstep : PC_MAX_STEPS);
          gc_collect(g_pc_roots, nr);
          thvm_lpo_invalidate();
          thvm_kbo_invalidate();
          pc_refresh_formulas(g_pc_roots);
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
      if (v == PC_VALID && pc_is_false(pc_steps[i].formula)) pc_saw_false = 1;
      if (v == PC_INVALID) { pc_bad = i; pc_bad_why = why; break; }
      if (v == PC_UNKNOWN) pc_any_unknown = 1;
    }
    thvm_heap_exhaust_jmp = NULL;
    if (pc_bad >= 0)          verdict = "VerifiedBad";
    else if (pc_any_unknown)  verdict = "Unknown";
    else if (!pc_saw_false)   verdict = "Unknown";  // no refutation closure
    else                      verdict = "VerifiedGood";
  }

  alarm(0);
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
// gets exactly one SZS line.  The conservative fallback is Unknown
// (0 points), never a wrong VerifiedBad (-10).
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
