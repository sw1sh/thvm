// ===================================================================
// FOF / clausal-CNF front-end: TPTP first-order input -> Horn ifeq UEQ
// ===================================================================
//
// Parses fof() records (and cnf() records with non-unit / non-
// equational clauses), clausifies through the src/fol pipeline
// (NNF + Skolemization + distribution), and equationalizes the HORN
// fragment with the ifeq encoding -- byte-for-byte the scheme of
// TPTP's '-10' UEQ problem versions ([Sma18], the Claessen-Smallbone
// CADE-2017 encoding).  Verified against LCL306-3 (CNF, non-unit)
// vs the official LCL306-10:
//   * predicate atom P(t...)      ->  the term P(t...), asserted = true
//   * definite clause C | ~G1 | ... | ~Gk  (C, Gi as equations ci=cr,
//     gil=gir after the predicate mapping)
//       ->  ifeq(g1l,g1r, ifeq(g2l,g2r, ... ifeq(gkl,gkr, cl, cr)
//           ..., cr), cr) = cr
//   * one dispatcher axiom        ifeq(A,A,B,C) = B   (iff ifeq used)
//   * a single unit negative clause ~(s=t) is the goal  s != t
//   * otherwise every negative clause ~G1|...|~Gk becomes the rule
//       ifeq(g1l,g1r, ... ifeq(gkl,gkr, false, true) ..., true) = true
//     and the single goal is      false != true
//     (complete for Horn: an unsatisfiable Horn set is refuted by ONE
//     negative clause against the definite subset's least model).
// A conjecture role is universally closed, negated, and clausified
// with the rest; its clauses carry the conjecture mark for SInE / tgt.
//
// Non-Horn clauses (2+ positive literals P1..Pm after tautology
// deletion, negative literals G1..Gk) take the classical or-encoding
// (THVM_ATP_FOF_OR, default on; =0 restores the old clean give-up):
//   * each positive literal reifies to a boolean-valued term: a
//     predicate atom is itself, an equational literal s=t becomes
//     eq(s,t) with a fresh binary eq (--fof-or=2 eq-reifies predicate
//     atoms too, as eq(p(..),true));
//   * the positives or-chain: D = or(P1', or(..., Pm')...) with a
//     fresh binary or;
//   * the negatives wrap D in the same ifeq scheme:
//       ifeq(g1l,g1r, ... ifeq(gkl,gkr, D, true) ..., true) = true
//     (all-positive clause: just D = true).
//
// Bare or-chains cannot cancel a disjunct, so once the lane fires the
// NEGATIVE clauses additionally export their information in rewrite
// form -- together these give the encoding DPLL-style unit
// propagation over or-chains (interreduction rewrites a falsified
// disjunct away, a true one collapses the chain):
//   * a negative UNIT ~G additionally emits  g = false  (predicate
//     atom: p(t..) = false; equational literal ~(s=t): eq(s,t) =
//     false) -- the funnel/goal emission is unchanged;
//   * a negative NON-unit ~G1|...|~Gk additionally emits the chain
//     or(not(G1'), or(..., not(Gk'))...) = true with a fresh unary
//     not (Gi' = the atom, eq(s,t) for an equational literal);
//   * helper axioms, once each if the family is used:
//       or(true,X) = true      or(X,true) = true
//       or(false,X) = X        or(X,false) = X
//       eq(X,X) = true         not(true) = false    not(false) = true
//     and two ifeq bridges that turn DERIVED boolean units back into
//     usable rewrite rules (a critical pair with a derived rule
//     eq(s,t) -> true yields s = t; with not(g) -> true yields
//     g = false):
//       ifeq(eq(X,Y),true,X,Y) = Y
//       ifeq(not(X),true,X,false) = false
//
// SOUNDNESS of the or-lane: take any model M of the original clause
// set over domain D and extend it to D' = D (+) {tt, ff} with
//   true -> tt, false -> ff,
//   p'(a..) -> tt if M |= P(a..) else ff      (predicate reification),
//   eq(a,b) -> tt if a = b else ff            (identity test on D'),
//   or(a,b) -> tt if a = tt or b = tt; else b if a = ff; else a if
//              b = ff; else ff,
//   not(a)  -> ff if a = tt; tt if a = ff; else ff,
//   ifeq(a,b,c,d) -> c if a = b else d,
// original functions/constants as in M (Skolem/Tseitin symbols by
// their defining property).  Every emitted equation is valid in this
// extension: the dispatcher, helpers, and bridges by case analysis
// on the definitions above; a Horn-encoded clause because either
// some guard gil=gir fails (every ifeq level collapses to its else
// branch = rhs) or all hold and the clause's conclusion literal is
// true in M; an or-encoded clause because when all guards hold some
// literal of the clause is true in M, making its reified disjunct tt
// and the or-chain tt; a negative-unit's g = false because the
// clause says exactly that G is false in M.  So the encoded equation
// set is satisfiable whenever the original set is; deriving the goal
// contradiction (the direct goal pair or false=true) therefore
// implies the original set + negated conjecture has no model, and
// the Theorem claim is sound.  The converse fails -- completion has
// no case split, so or-reasoning beyond unit propagation is out of
// reach (a clause whose disjuncts must be analyzed under separate
// assumptions never resolves) -- but failure is GaveUp, never
// CounterSatisfiable: g_cli_fof_applied joins cli_strategy_complete
// (the horn/equals pattern), which is exactly the honest-incomplete
// discipline.
//
// Distribution blow-up: fol_distribute is worst-case exponential, so
// each record's CNF size is estimated first (sum over AND, capped
// product over OR); past THVM_ATP_FOF_DISTRIB (default 512 clauses)
// the record clausifies via fol_tseitin_extract_clauses instead --
// the definitional (Tseitin) encoding is a conservative extension
// (any model of the original interprets each aux predicate by its
// definition), so the soundness argument above carries over with the
// aux predicates treated as ordinary predicates.  Tseitin output is
// mostly non-Horn, so it rides the or-lane.

static int g_cli_fof_applied     = 0;  // encoding ran: GaveUp, never Satisfiable
static int g_cli_fof_conjecture  = 0;  // conjecture role seen: SZS FOF ontology (Theorem)
static int g_cli_fof_trivial     = 0;  // clausification derived the empty clause
static int g_cli_fof_satref      = 0;  // clausal pre-pass derived the empty clause
static int g_cli_want_proof;           // set in main; defined near the extractor

// Does the (preprocessed) text need the clausal front-end?  Any fof()
// record, or any cnf() record whose literal part is not a unit
// (in)equation.  THVM_ATP_FOF=0 forces the old UEQ-only path.
static int tp_needs_clausal(const char *text) {
  {
    const char *e = getenv("THVM_ATP_FOF");
    if (e != NULL && e[0] == '0') return 0;
  }
  for (const char *p = text; (p = strstr(p, "fof(")) != NULL; p += 4)
    if (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) return 1;
  const char *p = text;
  while ((p = strstr(p, "cnf(")) != NULL) {
    if (p != text && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) { p += 4; continue; }
    p += 4;
    int depth = 1;
    const char *body = p;
    while (*p != '\0' && depth > 0) {
      if (*p == '(') depth++;
      else if (*p == ')') depth--;
      p++;
    }
    size_t blen = (size_t)(p - body - 1);
    // literal part = after the 2nd top-level comma
    int d = 0, commas = 0;
    const char *lit = NULL;
    for (size_t i = 0; i < blen; i++) {
      char c = body[i];
      if (c == '(') d++;
      else if (c == ')') d--;
      else if (c == ',' && d == 0 && ++commas == 2) { lit = body + i + 1; break; }
    }
    if (lit == NULL) continue;
    size_t llen = blen - (size_t)(lit - body);
    int eq = 0;
    for (size_t i = 0; i < llen; i++) {
      char c = lit[i];
      if (c == '|' || c == '~') return 1;           // disjunction / negation
      else if (c == '=') eq = 1;                    // covers '!=' too
    }
    if (!eq) return 1;                              // bare predicate literal
  }
  return 0;
}

// A genuine fof( record (word-boundary) in the raw problem text: the
// --fof-slice=2 exit-fast probe.  Same detection as tp_needs_clausal's
// fof( branch, but on the UNpreprocessed text so the schedule's FOF
// slices can free their pool slot before any include resolution or
// front-end work (the tp_has_ifeq_dispatcher cheap-probe shape).
static int tp_input_is_fof(const char *text) {
  for (const char *p = text; (p = strstr(p, "fof(")) != NULL; p += 4)
    if (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) return 1;
  return 0;
}

// Comment-aware marker sniff: does `raw` contain `marker` OUTSIDE any
// %-comment?  TPTP comments run % to end-of-line; problem headers are
// wall-to-wall comment text and regularly contain words like AXIOMS or
// record-keyword substrings (SEV326^5: "GVB-MB-AXIOMS" in a comment
// matched the Waldmeister .pr marker AXIOMS and routed a higher-order
// file to the bench default -- the CASC-J13 bench-default bug class,
// resurrected through a comment).  Every input-format decision below
// must use this, never a bare strstr on raw.
static int tp_raw_marker(const char *raw, const char *marker) {
  size_t mlen = strlen(marker);
  const char *line = raw;
  while (*line != '\0') {
    const char *eol = strchr(line, '\n');
    const char *end = (eol != NULL) ? eol : line + strlen(line);
    const char *pct = memchr(line, '%', (size_t)(end - line));
    const char *stop = (pct != NULL) ? pct : end;
    for (const char *p = line; p + mlen <= stop; p++) {
      if (memcmp(p, marker, mlen) == 0) return 1;
    }
    if (eol == NULL) break;
    line = eol + 1;
  }
  return 0;
}

// --- fresh symbol names against the pc_intern table ------------------
static char g_fof_true_name[TP_NAME_LEN]  = "true";
static char g_fof_false_name[TP_NAME_LEN] = "false";
static char g_fof_ifeq_name[TP_NAME_LEN]  = "ifeq";
static char g_fof_or_name[TP_NAME_LEN]    = "or";
static char g_fof_eq_name[TP_NAME_LEN]    = "eq";
static char g_fof_not_name[TP_NAME_LEN]   = "not";
static char g_fof_sk_prefix[TP_NAME_LEN]  = "sk";
static char g_fof_def_prefix[TP_NAME_LEN] = "def";

static void fof_fresh_const(char *name, size_t cap) {
  while (pc_lookup(name) != PC_NOT_FOUND) {
    size_t l = strlen(name);
    if (l + 2 >= cap) tp_giveup("cannot make a fresh symbol name");
    name[l] = 'q'; name[l + 1] = '\0';
  }
}
// Numbered-family prefix (Skolem sk<n>, Tseitin def<n>): no interned
// symbol may be <prefix><digits>.
static void fof_fresh_prefix(char *prefix, size_t cap) {
  for (;;) {
    size_t plen = strlen(prefix);
    int clash = 0;
    for (int i = 0; i < pc_nsyms && !clash; i++) {
      const char *nm = pc_sym[i];
      if (strncmp(nm, prefix, plen) != 0) continue;
      const char *d = nm + plen;
      if (*d == '\0') continue;
      while (*d >= '0' && *d <= '9') d++;
      if (*d == '\0') clash = 1;
    }
    if (!clash) return;
    if (plen + 2 >= cap)
      tp_giveup("cannot make a fresh symbol-family prefix");
    prefix[plen] = 'q'; prefix[plen + 1] = '\0';
  }
}

// --- clause-side serializer: fol Term -> .pr-ready text ---------------
// A clause with more variables than the equational engine could ever
// load is unrepresentable in the encoding (never in the clausal
// pre-pass, which keeps the clause): the oversize discipline below
// drops it on the axiom side.
#define FOF_ENC_MAX_VARS 256
typedef struct {
  u32 id[FOF_ENC_MAX_VARS];   // clause-local variable ids, first-occurrence order
  int n;
} FofVarScope;

// Oversize discipline: inside the axiom-side emitters
// (g_fof_soft_oversize) an encoding that overflows the serializer
// buffer or the .pr side bound DROPS that clause instead of giving up
// the run.  Dropping an input clause only shrinks the set -- a subset
// refutation is still a refutation, and the FOF path never reports
// Satisfiable -- so the cost is completeness the run never had, where
// the hard give-up cost the WHOLE problem (the Mizar SET007 families
// carry a handful of giant definition clauses whose ifeq towers blow
// the 2048-char side bound; every slice died on the first one).  The
// goal-side pushes (direct goal, or-lane helpers, dispatcher) keep
// the hard give-up: an unrepresentable goal has no honest Theorem.
static int g_fof_soft_oversize = 0;   // 1 inside axiom-side emitters
static int g_fof_ser_overflow  = 0;   // current clause overflowed: junk
static int g_fof_dropped_n     = 0;   // oversized clauses dropped

static void fof_ser_emit(char *buf, size_t cap, size_t *o, const char *s) {
  if (g_fof_ser_overflow) return;
  size_t l = strlen(s);
  if (*o + l + 1 >= cap) {
    if (g_fof_soft_oversize) { g_fof_ser_overflow = 1; return; }
    tp_giveup("FOF-encoded clause exceeds side-length bound");
  }
  memcpy(buf + *o, s, l + 1);
  *o += l;
}

static void fof_ser_term(Term t, FofVarScope *sc, char *buf, size_t cap, size_t *o) {
  if (term_tag(t) == TAG_FVR) {
    u32 vid = term_ext(t);
    int ix = -1;
    for (int i = 0; i < sc->n; i++) if (sc->id[i] == vid) { ix = i; break; }
    if (ix < 0) {
      if (sc->n >= FOF_ENC_MAX_VARS) {
        if (g_fof_soft_oversize) { g_fof_ser_overflow = 1; return; }
        tp_giveup("FOF clause exceeds variable bound");
      }
      ix = sc->n;
      sc->id[sc->n++] = vid;
    }
    char nm[16];
    snprintf(nm, sizeof nm, "X%d", ix);
    fof_ser_emit(buf, cap, o, nm);
    return;
  }
  if (term_tag(t) != TAG_CTR) tp_giveup("unsupported term in clausified formula");
  u32 lab = term_ext(t);
  char nm[TP_NAME_LEN + 16];
  if (lab >= FOL_LAB_SKOLEM_BASE) {
    snprintf(nm, sizeof nm, "%s%u", g_fof_sk_prefix, lab - FOL_LAB_SKOLEM_BASE);
  } else if (fol_is_tseitin_aux(lab)) {
    snprintf(nm, sizeof nm, "%s%u", g_fof_def_prefix, lab - FOL_LAB_TSEITIN_BASE);
  } else if (pc_ix_of(lab) >= 0) {
    snprintf(nm, sizeof nm, "%s", pc_sym[pc_ix_of(lab)]);
  } else {
    tp_giveup("unsupported symbol in clausified formula");
    return;
  }
  fof_ser_emit(buf, cap, o, nm);
  u32 n = term_ctr_n(t);
  if (n == 0u) return;
  fof_ser_emit(buf, cap, o, "(");
  for (u32 i = 0; i < n; i++) {
    if (i > 0) fof_ser_emit(buf, cap, o, ",");
    fof_ser_term(term_ctr_at(t, i), sc, buf, cap, o);
  }
  fof_ser_emit(buf, cap, o, ")");
}

// --- literal classification -------------------------------------------
// A literal maps to an equation pair (l, r): an equality atom keeps its
// sides; a predicate atom P(t...) pairs with the fresh `true` constant
// (r == 0 marks it, serialized late so the fresh name is final).
// Verdicts for $true/$false atoms: DROP the literal or the whole
// clause (tautology).
enum { FOF_LIT_PAIR, FOF_LIT_DROP_LIT, FOF_LIT_DROP_CLAUSE };

static int fof_lit_classify(const FolLit *lit, Term *l, Term *r) {
  Term a = lit->atom;
  if (term_tag(a) == TAG_FVR)
    tp_giveup("a bare variable is not a first-order atom");
  if (term_tag(a) != TAG_CTR) tp_giveup("unsupported atom in clausified formula");
  u32 lab = term_ext(a);
  if (lab == PC_LAB_TRUE)
    return lit->sign ? FOF_LIT_DROP_LIT : FOF_LIT_DROP_CLAUSE;
  if (lab == PC_LAB_FALSE)
    return lit->sign ? FOF_LIT_DROP_CLAUSE : FOF_LIT_DROP_LIT;
  if (fol_is_connective(lab))
    tp_giveup("clausification left a connective inside an atom");
  if (lab == PC_LAB_EQ) {
    *l = term_ctr_at(a, 0u);
    *r = term_ctr_at(a, 1u);
    return FOF_LIT_PAIR;
  }
  *l = a;
  *r = 0;   // predicate atom: right side is the fresh `true` constant
  return FOF_LIT_PAIR;
}

// --- clause emission ---------------------------------------------------

// Stamp applied by tp_fof_push to the clause it appends; call sites
// set it immediately before each push.  Left at 0/0 outside --proof
// FOF runs, so every other path stays byte-identical.
static int g_cur_prov_kind = PROV_NONE;
static int g_cur_prov_ix   = 0;

static void tp_fof_push(TpProblem *tp, const char *lhs, const char *rhs,
                        int neg, int conj) {
  if (g_fof_ser_overflow ||
      strlen(lhs) >= TP_SIDE_LEN || strlen(rhs) >= TP_SIDE_LEN) {
    if (g_fof_soft_oversize) {   // axiom-side: drop, don't die (see
      g_fof_ser_overflow = 0;    // fof_ser_emit's oversize discipline)
      g_fof_dropped_n++;
      return;
    }
    tp_giveup("FOF-encoded clause exceeds side-length bound");
  }
  // The unit table is the equational lanes' input only: past it, an
  // axiom-side clause drops like an oversized one (the clausal pre-pass
  // keeps the whole set), and the last slots stay reserved for the goal
  // and the encoding helpers.
  if (g_fof_soft_oversize && tp->n_cls >= TP_MAX_CLAUSES - 64) {
    g_fof_dropped_n++;
    return;
  }
  if (tp->n_cls >= TP_MAX_CLAUSES) tp_giveup("problem exceeds clause bound");
  TpClause *cl = &tp->cls[tp->n_cls++];
  snprintf(cl->lhs, sizeof cl->lhs, "%s", lhs);
  snprintf(cl->rhs, sizeof cl->rhs, "%s", rhs);
  cl->neg = neg;
  cl->conj = conj;
  cl->prov_kind = g_cur_prov_kind;
  cl->prov_ix   = g_cur_prov_ix;
}

// Serialize one Horn clause as a single unit equation.  `conds` are the
// negative literals' pairs in clause order; the conclusion pair is
// (cl, cr).  cr==0 / cond r==0 stand for the fresh `true` constant;
// cl==0 (with cr==0) is the fresh `false` constant (the funnel rule).
static void fof_emit_horn(TpProblem *tp, Term *cond_l, Term *cond_r, int nc,
                          Term cl, Term cr, int neg, int conj) {
  static char lbuf[TP_SIDE_LEN * 4];
  static char rbuf[TP_SIDE_LEN * 4];
  FofVarScope sc;
  sc.n = 0;
  size_t lo = 0, ro = 0;
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_ifeq_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    fof_ser_term(cond_l[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    if (cond_r[i] == 0) fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_true_name);
    else fof_ser_term(cond_r[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  // innermost then-branch: the conclusion's left side
  if (cl == 0) fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_false_name);
  else fof_ser_term(cl, &sc, lbuf, sizeof lbuf, &lo);
  // right side (and each level's else-branch): the conclusion's right side
  if (cr == 0) fof_ser_emit(rbuf, sizeof rbuf, &ro, g_fof_true_name);
  else fof_ser_term(cr, &sc, rbuf, sizeof rbuf, &ro);
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    fof_ser_emit(lbuf, sizeof lbuf, &lo, rbuf);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  }
  tp_fof_push(tp, lbuf, rbuf, neg, conj);
}

// --- non-Horn or-lane ---------------------------------------------------
// THVM_ATP_FOF_OR: 0 = off (Horn-only give-up), 1 = on with predicate
// atoms contributed directly when the whole disjunction is predicate
// atoms (default), 2 = on with every disjunct eq-reified.
static int fof_or_mode(void) {
  const char *e = getenv("THVM_ATP_FOF_OR");
  if (e == NULL || *e == '\0') return 1;
  return atoi(e);
}
static int g_fof_or_used  = 0;  // or-encoded clause emitted (helpers due)
static int g_fof_eq_used  = 0;  // an eq-reified disjunct emitted
static int g_fof_not_used = 0;  // a not-reified negative literal emitted
static int g_fof_or_ncl   = 0;  // or-encoded clause count (summary line)

// Reify one literal pair as a boolean-valued term: a predicate atom
// is itself, an equational pair goes through eq(l,r).  force_eq
// (--fof-or=2) eq-reifies predicate atoms too, as eq(p(..),true).
static void fof_ser_reified(Term l, Term r, int force_eq, FofVarScope *sc,
                            char *buf, size_t cap, size_t *o) {
  if (r == 0 && !force_eq) {
    fof_ser_term(l, sc, buf, cap, o);
    return;
  }
  g_fof_eq_used = 1;
  fof_ser_emit(buf, cap, o, g_fof_eq_name);
  fof_ser_emit(buf, cap, o, "(");
  fof_ser_term(l, sc, buf, cap, o);
  fof_ser_emit(buf, cap, o, ",");
  if (r == 0) fof_ser_emit(buf, cap, o, g_fof_true_name);
  else fof_ser_term(r, sc, buf, cap, o);
  fof_ser_emit(buf, cap, o, ")");
}

// Serialize one non-Horn clause: negative literals wrap the positive
// disjunction D in the ifeq scheme (else branch / rhs = true).
static void fof_emit_or(TpProblem *tp, Term *cond_l, Term *cond_r, int nc,
                        Term *pos_l, Term *pos_r, int np, int conj) {
  static char lbuf[TP_SIDE_LEN * 4];
  FofVarScope sc;
  sc.n = 0;
  size_t lo = 0;
  int force_eq = (fof_or_mode() == 2);
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_ifeq_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    fof_ser_term(cond_l[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    if (cond_r[i] == 0) fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_true_name);
    else fof_ser_term(cond_r[i], &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  // D = or(P1', or(..., Pm')...): right-nested or-chain of the
  // reified positives.
  for (int i = 0; i < np; i++) {
    if (i < np - 1) {
      fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_or_name);
      fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    }
    fof_ser_reified(pos_l[i], pos_r[i], force_eq, &sc, lbuf, sizeof lbuf, &lo);
    if (i < np - 1) fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  for (int i = 0; i < np - 1; i++)
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  for (int i = 0; i < nc; i++) {
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_true_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  }
  tp_fof_push(tp, lbuf, g_fof_true_name, 0, conj);
  g_fof_or_used = 1;
  g_fof_or_ncl++;
}

// Negative-clause rewrite export (or-lane only): a negative unit ~G
// emits g = false, a negative non-unit ~G1|...|~Gk emits the
// not()-reified or-chain.  Both ADD to the funnel/goal emission
// (which stays byte-identical), giving or-chains unit propagation.
static void fof_emit_neg_extra(TpProblem *tp, Term *lit_l, Term *lit_r,
                               int nl, int conj) {
  static char lbuf[TP_SIDE_LEN * 4];
  FofVarScope sc;
  sc.n = 0;
  size_t lo = 0;
  if (nl == 1) {
    fof_ser_reified(lit_l[0], lit_r[0], 0, &sc, lbuf, sizeof lbuf, &lo);
    tp_fof_push(tp, lbuf, g_fof_false_name, 0, conj);
    return;
  }
  g_fof_not_used = 1;
  for (int i = 0; i < nl; i++) {
    if (i < nl - 1) {
      fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_or_name);
      fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    }
    fof_ser_emit(lbuf, sizeof lbuf, &lo, g_fof_not_name);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, "(");
    fof_ser_reified(lit_l[i], lit_r[i], 0, &sc, lbuf, sizeof lbuf, &lo);
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
    if (i < nl - 1) fof_ser_emit(lbuf, sizeof lbuf, &lo, ",");
  }
  for (int i = 0; i < nl - 1; i++)
    fof_ser_emit(lbuf, sizeof lbuf, &lo, ")");
  tp_fof_push(tp, lbuf, g_fof_true_name, 0, conj);
}

