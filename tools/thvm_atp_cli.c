// FindProof: thvm's ATP completion engine as a standalone CLI --
// the CASC UEQ entry.  (Former name: thvm-atp; bin/thvm-atp remains an
// alias.)
//
// Exposes the same knob surface as TFindProof's Method option, mapped
// onto the engine's env-var toggles, and runs the exact code path of
// tests/test_atp_wolfram_bench.c (this file includes that translation
// unit with its main() renamed, so a CLI run is byte-identical to a
// bench run given the same knobs).
//
//   thvm-atp [--preset=waldmeister|waldmeister-lazy|none]
//            [--<knob>[=<value>] ...]
//            [--max-steps=N] [--wall=SEC] [--szs] [--proof]
//            problem.pr | problem.p
//
// CASC compatibility: a TPTP CNF UEQ problem (unit equalities, one or
// more negative units) is converted in-process to Waldmeister .pr form
// -- include('Axioms/X.ax') directives resolve against the problem's
// directory, $TPTP, and the CWD -- and the SZS result line uses the
// CNF ontology (Unsatisfiable / Satisfiable / GaveUp / ResourceOut /
// Timeout).  --proof prints an SZS CNFRefutation block: the input
// clauses, the derivation of every rule the closing rewrite chain
// cites (walked from the completion trace), and the goal chain itself.
//
// A multi-goal CONCLUSION block (or several negative TPTP units) is
// proved as a conjunction: every goal must join.  For several negative
// units that is sound for Unsatisfiable but stricter than refutation
// requires (any single one suffices); CASC UEQ problems carry exactly
// one.

#define main atp_bench_main
#include "../tests/test_atp_wolfram_bench.c"
#undef main

// ===================================================================
// knob table
// ===================================================================

// One CLI knob: kebab-case flag (named after the TFindProof Method
// suboption), the engine env var it drives, whether it takes a value
// (0 = boolean 0/1 with default 1 when bare), and the Method name.
typedef struct {
  const char *flag;
  const char *env;
  int         has_value;
  const char *method;
} AtpKnob;

static const AtpKnob KNOBS[] = {
  {"cp-weight",              "THVM_ATP_CP_WEIGHT",       1, "CriticalPairWeight"},
  {"max-weight",             "THVM_ATP_MAXW",            1, "MaxWeight"},
  {"auto-max-weight",        "THVM_ATP_AUTO_MAXW",       1, "AutoMaxWeight"},
  {"max-weight-slope",       "THVM_ATP_MAXW_SLOPE",      1, "AutoMaxWeight slope"},
  {"selection-ratio",        "THVM_ATP_SELECTION_RATIO", 1, "SelectionRatio"},
  {"goal-interleave",        "THVM_ATP_GOAL_INTERLEAVE", 1, "GoalInterleave"},
  {"ground-join",            "THVM_ATP_GROUND_JOIN",     0, "GroundJoin"},
  {"connectedness",          "THVM_ATP_CONNECT",         0, "Connectedness"},
  {"rhs-interreduce",        "THVM_ATP_RHS_IR",          0, "RHSInterreduce"},
  {"unfailing-cp",           "THVM_ATP_UNFAILING",       0, "UnfailingCP"},
  {"cp-set-interreduce",     "THVM_ATP_CP_SET_IR",       0, "CPSetInterreduce"},
  {"cp-set-ir-period",       "THVM_ATP_CP_SET_IR_PERIOD",1, "CPSetInterreduce period"},
  {"lazy-normalize",         "THVM_ATP_LAZY_NORM",       0, "LazyNormalize"},
  {"demote-on-lhs-simplify", "THVM_ATP_WM_DEMOTE",       0, "DemoteOnLhsSimplify"},
  {"orphan-murder",          "THVM_ATP_ORPHAN",          0, "OrphanMurder"},
  {"pop-subsume",            "THVM_ATP_POP_SUBSUME",     0, "PopSubsume"},
  {"eset-subsume",           "THVM_ATP_ESET_SUBSUME",    0, "ESetSubsume"},
  {"queue-subsume",          "THVM_ATP_QUEUE_SUBSUME",   0, "QueueSubsume"},
  {"flat-subsume",           "THVM_ATP_FLAT_SUBSUME",    0, "FlatSubsume"},
  {"comm-subsume",           "THVM_ATP_COMM_SUBSUME",    0, "CommSubsume"},
  {"emission-order",         "THVM_ATP_EMISSION_ORDER",  0, "EmissionOrder"},
  {"intake-order",           "THVM_ATP_INTAKE_ORDER",    0, "IntakeOrder"},
  {"mixmost-nf",             "THVM_ATP_MIXMOST_NF",      0, "MixmostNF"},
  {"formation-fifo",         "THVM_ATP_FORMATION_FIFO",  0, "FormationFifo"},
  {"fifo-threshold",         "THVM_ATP_FIFO_THRESHOLD",  1, "FIFO interleave threshold"},
  {"eset-distdir",           "THVM_ATP_ESET_DISTDIR",    0, "EsetDistdir"},
  {"drain-chainpos",         "THVM_ATP_DRAIN_CHAINPOS",  0, "FormationFifo drain (chainpos)"},
  {"drain-revface",          "THVM_ATP_DRAIN_REVFACE",   0, "FormationFifo drain (revface)"},
  {"mnf",                    "THVM_ATP_MNF",             0, "GoalDirected (MNF front)"},
  {"record-norm",            "THVM_ATP_RECORD_NORM",     0, "RecordNorm"},
  {"free-var-instance",      "THVM_ATP_FVI",             0, "FreeVarInstance"},
  {"implicit-cp",            "THVM_ATP_IMPLICIT_CP",     0, "UseImplicitCp"},
  {"einsstern",              "THVM_ATP_EINSSTERN",       0, "Einsstern"},
  {"no-overlap-below-skolem","THVM_ATP_NO_OVERLAP_BELOW_SKOLEM", 0, "NoOverlapBelowSkolem"},
  {"reclassify",             "THVM_ATP_RECLASSIFY",      0, "Reclassify"},
  {"reversed-completion",    "THVM_ATP_REVERSED",        0, "ReversedCompletion"},
  {"sue-management",         "THVM_ATP_SOS",             0, "SUEManagement"},
  {"goal-subterm",           "THVM_ATP_GOAL_SUBTERM",    0, "GoalSubtermRelevance"},
  {"backward-ground-join",   "THVM_ATP_BWD_GROUND_JOIN", 0, "BackwardGroundJoin"},
  {"cp-side",                "THVM_ATP_CP_SIDE",         0, "CPSide"},
  {"wm-trie-faithful",       "THVM_ATP_WM_TRIE_FAITHFUL",0, "(faithful trie)"},
  {"initial-ultimate",       "THVM_ATP_INITIAL_ULTIMATE",0, "IntakeOrder initial stamp"},
  {"database-ultimate",      "THVM_ATP_DATABASE_ULTIMATE",0,"(WM database=ultimate)"},
  {"incremental-ir",         "THVM_ATP_INCR_IR",         0, "(incremental interreduce)"},
  {"cp-index",               "THVM_ATP_CP_INDEX",        0, "(CP index)"},
  {"right-reduce",           "THVM_ATP_RIGHT_REDUCE",    0, "(right reduce)"},
  {"lr-sortieren",           "THVM_ATP_LR_SORTIEREN",    0, "(WM LR sort)"},
  {"w2-mode",                "THVM_ATP_W2_MODE",         1, "(FIFO w2 mode)"},
  {"w2-modulo",              "THVM_ATP_W2_MODULO",       1, "(FIFO w2 modulo)"},
  {"perm-subsume",           "THVM_ATP_PERM_SUB",        0, "(permutative subsume)"},
  {"random-ratio",           "THVM_ATP_RANDOM_RATIO",    1, "(random CP ratio)"},
  {"random-seed",            "THVM_ATP_RANDOM_SEED",     1, "(random seed)"},
  {"heap-cells",             "THVM_HEAP_CELLS",          1, "(heap size, cells)"},
  {"profile",                "THVM_ATP_PROFILE",         1, "(profiling level)"},
  {"diag",                   "THVM_ATP_DIAG",            1, "(diagnostics)"},
};
static const int N_KNOBS = (int)(sizeof(KNOBS) / sizeof(KNOBS[0]));

// ===================================================================
// TPTP CNF UEQ -> .pr conversion
// ===================================================================

// Set during TPTP parsing when any negative unit carries a variable
// (the re-negated goal is existential); gates the Satisfiable claim.
static int g_cli_goal_existential = 0;
// Set in main before conversion so the over-bound handler can emit a
// competition-valid SZS line instead of crashing.
static int g_cli_szs_wanted = 0;
static const char *g_cli_probpath = "";

static void tp_giveup(const char *why) {
  // Under a schedule slice the wrapper owns the single final status, so
  // stay silent on stdout and let the non-zero exit advance the slice.
  if (g_cli_szs_wanted && getenv("FINDPROOF_SLICE") == NULL)
    printf("%% SZS status GaveUp for %s\n", g_cli_probpath);
  fprintf(stderr, "FindProof: %s\n", why);
  exit(1);
}

#define TP_MAX_CLAUSES  512
#define TP_MAX_SYMS     2048
#define TP_MAX_VARS     256
#define TP_NAME_LEN     64
#define TP_SIDE_LEN     2048

typedef struct {
  char lhs[TP_SIDE_LEN], rhs[TP_SIDE_LEN];
  int  neg;
} TpClause;

typedef struct {
  char name[TP_NAME_LEN];
  int  arity;
} TpSym;

typedef struct {
  TpClause cls[TP_MAX_CLAUSES];
  int      n_cls;
  TpSym    syms[TP_MAX_SYMS];
  int      n_syms;
  char     vars[TP_MAX_VARS][TP_NAME_LEN];
  int      n_vars;
} TpProblem;

static char *tp_read_file(const char *path, size_t *len_out) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return NULL;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1u);
  if (buf == NULL) { fclose(fp); return NULL; }
  size_t rd = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  buf[rd] = '\0';
  if (len_out != NULL) *len_out = rd;
  return buf;
}

// Resolve an include('NAME') against dirname(problem), $TPTP, CWD.
static char *tp_resolve_include(const char *problem_path, const char *inc) {
  static char cand[4096];
  const char *slash = strrchr(problem_path, '/');
  if (slash != NULL) {
    snprintf(cand, sizeof cand, "%.*s/%s",
             (int)(slash - problem_path), problem_path, inc);
    char *b = tp_read_file(cand, NULL);
    if (b != NULL) return b;
  }
  const char *tptp = getenv("TPTP");
  if (tptp != NULL) {
    snprintf(cand, sizeof cand, "%s/%s", tptp, inc);
    char *b = tp_read_file(cand, NULL);
    if (b != NULL) return b;
  }
  return tp_read_file(inc, NULL);
}

// Strip %-comments and splice include directives (one level -- TPTP
// axiom files do not nest for the UEQ library).
static char *tp_preprocess(const char *problem_path, const char *raw) {
  size_t cap = strlen(raw) * 4u + 65536u;
  char *out = (char *)malloc(cap);
  size_t o = 0;
  const char *p = raw;
  while (*p != '\0') {
    const char *eol = strchr(p, '\n');
    size_t ll = (eol != NULL) ? (size_t)(eol - p) : strlen(p);
    const char *t = p;
    size_t tl = ll;
    while (tl > 0 && isspace((unsigned char)*t)) { t++; tl--; }
    if (tl > 0 && *t == '%') { /* comment line: drop */ }
    else if (tl > 8 && strncmp(t, "include(", 8) == 0) {
      const char *q1 = strchr(t, '\'');
      const char *q2 = (q1 != NULL) ? strchr(q1 + 1, '\'') : NULL;
      if (q1 != NULL && q2 != NULL) {
        char inc[1024];
        size_t n = (size_t)(q2 - q1 - 1);
        if (n >= sizeof inc) n = sizeof inc - 1;
        memcpy(inc, q1 + 1, n);
        inc[n] = '\0';
        char *body = tp_resolve_include(problem_path, inc);
        if (body == NULL) {
          fprintf(stderr, "FindProof: cannot resolve include('%s')\n", inc);
          exit(2);
        }
        // strip comments in the included body too (no nested includes)
        const char *bp = body;
        while (*bp != '\0') {
          const char *beol = strchr(bp, '\n');
          size_t bll = (beol != NULL) ? (size_t)(beol - bp) : strlen(bp);
          const char *bt = bp;
          size_t btl = bll;
          while (btl > 0 && isspace((unsigned char)*bt)) { bt++; btl--; }
          if (!(btl > 0 && *bt == '%')) {
            if (o + bll + 2 >= cap) { cap = cap * 2 + bll; out = (char *)realloc(out, cap); }
            memcpy(out + o, bp, bll); o += bll; out[o++] = '\n';
          }
          if (beol == NULL) break;
          bp = beol + 1;
        }
        free(body);
      }
    } else {
      if (o + ll + 2 >= cap) { cap = cap * 2 + ll; out = (char *)realloc(out, cap); }
      memcpy(out + o, p, ll); o += ll; out[o++] = '\n';
    }
    if (eol == NULL) break;
    p = eol + 1;
  }
  out[o] = '\0';
  return out;
}

static void tp_sym_note(TpProblem *tp, const char *name, size_t n, int arity) {
  if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
  for (int i = 0; i < tp->n_syms; i++) {
    if (strlen(tp->syms[i].name) == n && memcmp(tp->syms[i].name, name, n) == 0) {
      if (tp->syms[i].arity != arity) {
        fprintf(stderr, "FindProof: symbol %.*s used with arities %d and %d\n",
                (int)n, name, tp->syms[i].arity, arity);
        exit(2);
      }
      return;
    }
  }
  if (tp->n_syms >= TP_MAX_SYMS) tp_giveup("problem exceeds symbol bound");
  memcpy(tp->syms[tp->n_syms].name, name, n);
  tp->syms[tp->n_syms].name[n] = '\0';
  tp->syms[tp->n_syms].arity = arity;
  tp->n_syms++;
}

static void tp_var_note(TpProblem *tp, const char *name, size_t n) {
  if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
  for (int i = 0; i < tp->n_vars; i++) {
    if (strlen(tp->vars[i]) == n && memcmp(tp->vars[i], name, n) == 0) return;
  }
  if (tp->n_vars >= TP_MAX_VARS) tp_giveup("problem exceeds variable bound");
  memcpy(tp->vars[tp->n_vars], name, n);
  tp->vars[tp->n_vars][n] = '\0';
  tp->n_vars++;
}

// Walk a term string, recording functor arities and variables.
static void tp_scan_term(TpProblem *tp, const char *s) {
  const char *p = s;
  while (*p != '\0') {
    if (isalnum((unsigned char)*p) || *p == '_') {
      const char *name = p;
      size_t n = 0;
      while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
      const char *q = p + n;
      while (isspace((unsigned char)*q)) q++;
      if (*q == '(') {
        // functor: count top-level args inside the balanced parens
        int depth = 1, args = 1;
        const char *r = q + 1;
        while (*r != '\0' && depth > 0) {
          if (*r == '(') depth++;
          else if (*r == ')') depth--;
          else if (*r == ',' && depth == 1) args++;
          r++;
        }
        tp_sym_note(tp, name, n, args);
      } else if (isupper((unsigned char)*name)) {
        tp_var_note(tp, name, n);
      } else {
        tp_sym_note(tp, name, n, 0);
      }
      p += n;
    } else {
      p++;
    }
  }
}

// Parse the preprocessed text into clauses + signature.
static void tp_parse(TpProblem *tp, const char *text) {
  const char *p = text;
  while ((p = strstr(p, "cnf(")) != NULL) {
    p += 4;
    // balanced scan to the closing ')' of this cnf(...)
    int depth = 1;
    const char *body = p;
    while (*p != '\0' && depth > 0) {
      if (*p == '(') depth++;
      else if (*p == ')') depth--;
      p++;
    }
    size_t blen = (size_t)(p - body - 1);
    // split off name and role at the first two top-level commas
    int d = 0, commas = 0;
    const char *lit = NULL;
    for (size_t i = 0; i < blen; i++) {
      char c = body[i];
      if (c == '(') d++;
      else if (c == ')') d--;
      else if (c == ',' && d == 0) {
        commas++;
        if (commas == 2) { lit = body + i + 1; break; }
      }
    }
    if (lit == NULL) continue;
    size_t llen = blen - (size_t)(lit - body);
    // trim spaces and one level of ( )
    while (llen > 0 && isspace((unsigned char)*lit)) { lit++; llen--; }
    while (llen > 0 && isspace((unsigned char)lit[llen - 1])) llen--;
    while (llen >= 2 && lit[0] == '(' && lit[llen - 1] == ')') {
      // only strip if balanced as an outer wrapper
      int dd = 0; int wraps = 1;
      for (size_t i = 0; i + 1 < llen; i++) {
        if (lit[i] == '(') dd++;
        else if (lit[i] == ')') { dd--; if (dd == 0 && i + 1 < llen - 1) { wraps = 0; break; } }
      }
      if (!wraps) break;
      lit++; llen -= 2;
      while (llen > 0 && isspace((unsigned char)*lit)) { lit++; llen--; }
      while (llen > 0 && isspace((unsigned char)lit[llen - 1])) llen--;
    }
    // find the (top-level) equality operator
    int neg = 0;
    size_t oppos = (size_t)-1;
    int d2 = 0;
    for (size_t i = 0; i < llen; i++) {
      if (lit[i] == '(') d2++;
      else if (lit[i] == ')') d2--;
      else if (d2 == 0 && lit[i] == '!' && i + 1 < llen && lit[i + 1] == '=') { neg = 1; oppos = i; break; }
      else if (d2 == 0 && lit[i] == '=' && (i == 0 || lit[i - 1] != '!')) { oppos = i; break; }
    }
    if (oppos == (size_t)-1) { fprintf(stderr, "FindProof: non-equational clause\n"); exit(2); }
    if (tp->n_cls >= TP_MAX_CLAUSES) tp_giveup("problem exceeds clause bound");
    TpClause *cl = &tp->cls[tp->n_cls++];
    size_t ln = oppos, rs = oppos + (neg ? 2u : 1u);
    while (ln > 0 && isspace((unsigned char)lit[ln - 1])) ln--;
    while (rs < llen && isspace((unsigned char)lit[rs])) rs++;
    size_t rn = llen - rs;
    if (ln >= sizeof cl->lhs || rn >= sizeof cl->rhs) tp_giveup("term exceeds side-length bound");
    memcpy(cl->lhs, lit, ln); cl->lhs[ln] = '\0';
    memcpy(cl->rhs, lit + rs, rn); cl->rhs[rn] = '\0';
    cl->neg = neg;
    if (neg) {
      // uppercase-initial token anywhere in the goal = goal variable
      for (const char *q = lit; *q != '\0' && q < lit + llen; q++) {
        if (isupper((unsigned char)*q) &&
            (q == lit || !(isalnum((unsigned char)q[-1]) || q[-1] == '_'))) {
          g_cli_goal_existential = 1;
          break;
        }
      }
    }
    tp_scan_term(tp, cl->lhs);
    tp_scan_term(tp, cl->rhs);
  }
}

static int tp_sym_cmp(const void *a, const void *b) {
  const TpSym *x = (const TpSym *)a, *y = (const TpSym *)b;
  if (x->arity != y->arity) return x->arity - y->arity;
  return strcmp(x->name, y->name);
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

// ===================================================================
// SZS + proof output
// ===================================================================

// Competition-clean output: in --szs/--proof mode the bench TU's
// diagnostic chatter is rerouted to stderr and the SZS lines go to the
// preserved real stdout via this stream.
static FILE *g_cli_out = NULL;
static int  g_cli_tptp_mode = 0;
static int  g_cli_want_proof = 0;
static int  g_cli_want_szs = 0;
static int  g_cli_szs_printed = 0;
static const char *g_cli_problem = NULL;

// 1 when no completeness-breaking pruning is active AND the goal is
// ground, so a saturation (QUEUE_EMPTY) soundly reports Satisfiable;
// else GaveUp.  An existential goal (variables in the negative unit)
// is decided by BUDGETED narrowing, so its saturation witnesses
// nothing.
static int cli_strategy_complete(void) {
  const char *mw = getenv("THVM_ATP_MAXW");
  const char *am = getenv("THVM_ATP_AUTO_MAXW");
  const char *rr = getenv("THVM_ATP_RANDOM_RATIO");
  return !g_cli_goal_existential
      && (mw == NULL || *mw == '0') && (am == NULL || *am == '0')
      && (rr == NULL || *rr == '0');
}

// --- term printing (names from the parsed .pr) ---------------------
static void cli_print_term(FILE *fp, const PrProblem *pr, Term t) {
  if (t == 0) { fputs("_", fp); return; }
  u32 tag = term_tag(t);
  if (tag == TAG_FVR) {
    u32 v = term_ext(t);
    if (pr != NULL && v < pr->n_vars) fputs(pr->vars[v], fp);
    else fprintf(fp, "X%u", v);
    return;
  }
  if (tag == TAG_CTR) {
    u32 lab = term_ext(t);
    u32 n = term_ctr_n(t);
    if (pr != NULL && lab >= PR_LABEL_BASE && lab - PR_LABEL_BASE < pr->n_syms)
      fputs(pr->syms[lab - PR_LABEL_BASE].name, fp);
    else fprintf(fp, "f%u", lab);
    if (n > 0) {
      fputc('(', fp);
      for (u32 i = 0; i < n; i++) {
        if (i > 0) fputc(',', fp);
        cli_print_term(fp, pr, term_ctr_at(t, i));
      }
      fputc(')', fp);
    }
    return;
  }
  fprintf(fp, "?t%u", tag);
}

// --- derivation closure over the completion trace ------------------
// Emit the trace entry `ti` (and, recursively, its parents) as cnf()
// lines; printed[] de-duplicates.  Names: t<ti>.
static void cli_emit_trace(FILE *fp, AtpState *s, const PrProblem *pr,
                           u32 ti, u8 *printed) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace || printed[ti]) return;
  printed[ti] = 1u;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) return;
  u32 reason = term_ext(e);
  u32 pa = (u32)term_val(term_ctr_at(e, 0));
  u32 pb = (u32)term_val(term_ctr_at(e, 1));
  Term lhs = term_ctr_at(e, 2);
  Term rhs = term_ctr_at(e, 3);
  // Under the off-heap packed trace (TracePack) a non-cone entry's pair
  // slots hold NUM sentinels; load the real pair from the packed store.
  if (term_tag(lhs) == TAG_NUM && term_tag(rhs) == TAG_NUM) {
    Term ll = 0, rr = 0;
    if (atp_trace_eq_load(s, ti, &ll, &rr)) { lhs = ll; rhs = rr; }
  }
  cli_emit_trace(fp, s, pr, pa, printed);
  cli_emit_trace(fp, s, pr, pb, printed);
  fprintf(fp, "cnf(t%u, %s, ", ti,
          (reason == TRACE_AXIOM) ? "axiom" : "plain");
  cli_print_term(fp, pr, lhs);
  fputs(" = ", fp);
  cli_print_term(fp, pr, rhs);
  if (reason == TRACE_AXIOM) {
    fputs(").\n", fp);
  } else {
    const char *rule =
        (reason == TRACE_CP)      ? "cp" :
        (reason == TRACE_ORIENT)  ? "orient" :
        (reason == TRACE_SIMPLIFY)? "rw" : "step";
    fprintf(fp, ", inference(%s, [status(thm)], [", rule);
    int firstp = 1;
    if (pa != ATP_TRACE_NONE && pa < s->n_trace) { fprintf(fp, "t%u", pa); firstp = 0; }
    if (pb != ATP_TRACE_NONE && pb < s->n_trace) { fprintf(fp, "%st%u", firstp ? "" : ", ", pb); }
    fputs("])).\n", fp);
  }
}

// Emit one goal's closing rewrite chain (steps from
// thvm_atp_proof_extract) plus the derivations of every rule cited.
static void cli_emit_goal_chain(FILE *fp, AtpState *s, const PrProblem *pr,
                                Term gl, Term gr, u32 goal_id,
                                const AtpProofStep *steps, u32 n_steps,
                                u8 *printed) {
  for (u32 k = 0; k < n_steps; k++) {
    u32 r = steps[k].rule;
    if (r < s->n_rules && s->r_trace != NULL && s->r_trace[r] != ATP_TRACE_NONE)
      cli_emit_trace(fp, s, pr, s->r_trace[r], printed);
  }
  fprintf(fp, "cnf(goal_%u, negated_conjecture, ", goal_id);
  cli_print_term(fp, pr, gl);
  fputs(" != ", fp);
  cli_print_term(fp, pr, gr);
  fputs(").\n", fp);
  Term cur_l = gl, cur_r = gr;
  char prev[32];
  snprintf(prev, sizeof prev, "goal_%u", goal_id);
  for (u32 k = 0; k < n_steps; k++) {
    const AtpProofStep *st = &steps[k];
    if (st->side == 0u) cur_l = st->after; else cur_r = st->after;
    fprintf(fp, "cnf(g%u_%u, plain, ", goal_id, k);
    cli_print_term(fp, pr, cur_l);
    fputs(" != ", fp);
    cli_print_term(fp, pr, cur_r);
    u32 rt = (st->rule < s->n_rules && s->r_trace != NULL)
           ? s->r_trace[st->rule] : ATP_TRACE_NONE;
    fprintf(fp, ", inference(rw, [status(thm)], [%s", prev);
    if (rt != ATP_TRACE_NONE) fprintf(fp, ", t%u", rt);
    fputs("])).\n", fp);
    snprintf(prev, sizeof prev, "g%u_%u", goal_id, k);
  }
  fprintf(fp, "cnf(contradiction_%u, plain, $false, "
              "inference(trivial_inequality_removal, [status(thm)], [%s])).\n",
          goal_id, prev);
}

static void cli_proof_hook(AtpState *s) {
  if (!g_cli_want_proof || g_atp_bench_status != (int)ATP_PROVED) return;
  PrProblem *pr = (PrProblem *)g_atp_bench_pr;
  if (pr == NULL) return;
  static AtpProofStep steps[ATP_PROOF_MAX_STEPS];
  u8 *printed = (u8 *)calloc(s->n_trace, 1u);
  if (printed == NULL) return;
  // The bench's stdout is rerouted to stderr in competition mode; the
  // SZS stream goes to the preserved real stdout (g_cli_out).
  FILE *out = (g_cli_out != NULL) ? g_cli_out : stdout;
  if (g_cli_want_szs && !g_cli_szs_printed) {
    fprintf(out, "%% SZS status %s for %s\n",
            g_cli_tptp_mode ? "Unsatisfiable" : "Theorem", g_cli_problem);
    g_cli_szs_printed = 1;
  }
  fprintf(out, "%% SZS output start CNFRefutation for %s\n", g_cli_problem);
  u32 n_goals = (pr->n_goals > 0u) ? pr->n_goals : 1u;
  for (u32 g = 0; g < n_goals; g++) {
    Term gl = (pr->n_goals > 0u) ? pr->goals_l[g] : pr->goal_l;
    Term gr = (pr->n_goals > 0u) ? pr->goals_r[g] : pr->goal_r;
    if (thvm_atp_set_goal(s, gl, gr) == 0u) continue;
    u32 n = thvm_atp_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    if (n == 0u) n = thvm_atp_mnf_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    if (n == 0u && !(term_tag(gl) == term_tag(gr) && gl == gr)) {
      fprintf(out, "%% goal %u: no single-NF chain extractable\n", g);
      continue;
    }
    cli_emit_goal_chain(out, s, pr, gl, gr, g, steps, n, printed);
  }
  fprintf(out, "%% SZS output end CNFRefutation for %s\n", g_cli_problem);
  fflush(out);
  free(printed);
}

// ===================================================================
// main
// ===================================================================

// --schedule: run a fixed sequence of configurations, each in a child
// process (fork/exec of argv[0] with the config's flags and a slice of
// the wall budget), stopping at the first proof -- the standard CASC
// portfolio-wrapper shape, with process isolation guaranteeing each
// slice a fresh engine.  Slice fractions are provisional until the
// CASC-30 coverage matrix tunes them.
typedef struct { const char *name; const char *flags; double frac; } SchedSlice;
static const SchedSlice SCHEDULE[] = {
  {"wm",      "",                                          0.35},
  {"wm-gj",   "--ground-join --connectedness",             0.22},
  {"lazy-gj", "--preset=waldmeister-lazy --ground-join --connectedness", 0.13},
  {"wm-mnf",  "--mnf",                                     0.08},
  // Goal-direction (the EXISTING MNF front) COMBINED with the pruning
  // that keeps its front lean -- ground-join + a growing MaxWeight
  // bound.  The bare wm-mnf slice above explodes (no pruning) and the
  // pruning slices carry no goal-direction; this slice is the missing
  // intersection, assembled from existing knobs only (no new engine
  // code -- MNF is the WM-faithful front already in the engine).
  {"mnf-gj-amw", "--mnf --ground-join --connectedness --auto-max-weight=6 --max-weight-slope=1", 0.12},
  {"bare-gj", "--preset=none --ground-join --connectedness", 0.10},
};
static const int N_SCHED = (int)(sizeof(SCHEDULE) / sizeof(SCHEDULE[0]));

static int run_schedule(const char *self, const char *file, double wall,
                        int szs, int want_proof) {
  for (int i = 0; i < N_SCHED; i++) {
    double slice = wall * SCHEDULE[i].frac;
    if (slice < 1.0) slice = 1.0;
    char cmd[4096];
    snprintf(cmd, sizeof cmd, "FINDPROOF_SLICE=1 '%s' %s --wall=%.0f%s%s '%s'",
             self, SCHEDULE[i].flags, slice,
             szs ? " --szs" : "", want_proof ? " --proof" : "", file);
    fprintf(stderr, "%% FindProof schedule slice %s (%.0fs)\n",
            SCHEDULE[i].name, slice);
    int rc = system(cmd);
    if (rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0) return 0;
  }
  if (szs) printf("%% SZS status GaveUp for %s\n", file);
  return 1;
}

static void usage(FILE *fp) {
  fprintf(fp,
    "usage: FindProof [options] problem.pr | problem.p\n"
    "  --preset=waldmeister        TFindProof Method -> \"Waldmeister\" stack\n"
    "  --preset=waldmeister-lazy   Method -> \"WaldmeisterLazy\" stack\n"
    "  --preset=none               bare engine defaults\n"
    "  --<knob>[=v]                individual Method suboptions (override preset);\n"
    "                              bare boolean flags mean =1 (--list-knobs for all)\n"
    "  --max-steps=N               CP-processing cap        (default 500000)\n"
    "  --wall=SEC                  wall-clock cap           (default 120)\n"
    "  --szs                       print an SZS status line (CNF ontology for TPTP input)\n"
    "  --schedule                  time-sliced portfolio of configurations (child\n"
    "                              process per slice; first proof wins)\n"
    "  --proof                     print an SZS CNFRefutation block on success\n"
    "  --keep-pr                   keep (and name) the .pr generated from TPTP input\n"
    "  --list-knobs                print the knob table and exit\n"
    "TPTP CNF UEQ input (.p) is converted to Waldmeister .pr form in-process;\n"
    "include('Axioms/*.ax') resolves against the problem directory, $TPTP, CWD.\n");
}

int main(int argc, char **argv) {
  const char *preset = "waldmeister";
  const char *file = NULL;
  const char *max_steps = "500000";
  const char *wall = "120";
  int szs = 0, keep_pr = 0, want_schedule = 0;

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    if (strncmp(a, "--", 2) != 0) {
      if (file != NULL) { fprintf(stderr, "FindProof: one problem file only\n"); return 2; }
      file = a;
      continue;
    }
    char *name = a + 2;
    char *val = strchr(name, '=');
    size_t nlen = (val != NULL) ? (size_t)(val - name) : strlen(name);
    if (val != NULL) val++;
    if (nlen == 4 && strncmp(name, "help", 4) == 0) { usage(stdout); return 0; }
    if (nlen == 10 && strncmp(name, "list-knobs", 10) == 0) {
      for (int k = 0; k < N_KNOBS; k++)
        printf("  --%-26s %-34s %s\n", KNOBS[k].flag, KNOBS[k].env, KNOBS[k].method);
      return 0;
    }
    if (nlen == 3 && strncmp(name, "szs", 3) == 0) { szs = 1; continue; }
    if (nlen == 8 && strncmp(name, "schedule", 8) == 0) { want_schedule = 1; continue; }
    if (nlen == 5 && strncmp(name, "proof", 5) == 0) { g_cli_want_proof = 1; continue; }
    if (nlen == 7 && strncmp(name, "keep-pr", 7) == 0) { keep_pr = 1; continue; }
    if (nlen == 12 && strncmp(name, "convert-only", 12) == 0) { keep_pr = 2; continue; }
    if (nlen == 6 && strncmp(name, "preset", 6) == 0) { preset = (val != NULL) ? val : "waldmeister"; continue; }
    if (nlen == 9 && strncmp(name, "max-steps", 9) == 0 && val != NULL) { max_steps = val; continue; }
    if (nlen == 4 && strncmp(name, "wall", 4) == 0 && val != NULL) { wall = val; continue; }
    int hit = 0;
    for (int k = 0; k < N_KNOBS; k++) {
      if (strlen(KNOBS[k].flag) == nlen && strncmp(name, KNOBS[k].flag, nlen) == 0) {
        if (KNOBS[k].has_value && val == NULL) {
          fprintf(stderr, "FindProof: --%s needs a value\n", KNOBS[k].flag);
          return 2;
        }
        setenv(KNOBS[k].env, (val != NULL) ? val : "1", 1);
        hit = 1;
        break;
      }
    }
    if (!hit) { fprintf(stderr, "FindProof: unknown option --%.*s\n", (int)nlen, name); usage(stderr); return 2; }
  }
  if (file == NULL) { usage(stderr); return 2; }

  if (want_schedule) {
    return run_schedule(argv[0], file, atof(wall), szs, g_cli_want_proof);
  }

  // Presets replicate the WL $AtpPresetDefaults stacks; setenv(...,0)
  // never overwrites an explicit knob flag, so flags override the
  // preset exactly like Method suboptions override a preset name.
  if (strcmp(preset, "waldmeister") == 0) {
    setenv("THVM_ATP_WALDMEISTER", "1", 0);
  } else if (strcmp(preset, "waldmeister-lazy") == 0) {
    setenv("THVM_ATP_WALDMEISTER", "1", 0);
    setenv("THVM_ATP_LAZY_NORM", "1", 0);
    setenv("THVM_ATP_CP_SET_IR", "1", 0);
    setenv("THVM_ATP_AUTO_MAXW", "30", 0);
  } else if (strcmp(preset, "none") == 0) {
    // bare engine defaults
  } else {
    fprintf(stderr, "FindProof: unknown preset %s\n", preset);
    return 2;
  }

  // TPTP input -> temp .pr (detected by content: a cnf( block).
  g_cli_problem = file;
  const char *run_file = file;
  char tmp_pr[1024] = "";
  {
    size_t rl = 0;
    char *raw = tp_read_file(file, &rl);
    if (raw == NULL) { fprintf(stderr, "FindProof: cannot open %s\n", file); return 2; }
    if (strstr(raw, "cnf(") != NULL) {
      g_cli_tptp_mode = 1;
      g_cli_szs_wanted = szs;
      g_cli_probpath = file;
      char *pre = tp_preprocess(file, raw);
      TpProblem *tpp = (TpProblem *)calloc(1, sizeof(TpProblem));
      if (tpp == NULL) { fprintf(stderr, "FindProof: OOM\n"); return 2; }
#define tp (*tpp)
      tp_parse(&tp, pre);
      free(pre);
      int n_neg = 0;
      for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
      if (n_neg == 0 || tp.n_cls == n_neg) {
        fprintf(stderr, "FindProof: need at least one positive and one negative unit\n");
        return 2;
      }
      // Never hand the .pr reader more than it can hold: an oversized
      // problem (the CSR knowledge bases run to thousands of equations)
      // exits cleanly as GaveUp rather than aborting downstream with no
      // SZS line.  PR_MAX_* come from the included bench translation unit.
      if ((tp.n_cls - n_neg) > (int)PR_MAX_EQNS) tp_giveup("problem exceeds equation capacity");
      if (tp.n_syms          > (int)PR_MAX_SYMS) tp_giveup("problem exceeds symbol capacity");
      if (tp.n_vars          > (int)PR_MAX_VARS) tp_giveup("problem exceeds variable capacity");
      // The flatterm term reader holds at most 8 arguments per node; a
      // higher-arity functor (e.g. TOP052's tuple/16) is beyond the term
      // representation, so give up cleanly instead of aborting the reader.
      for (int i = 0; i < tp.n_syms; i++)
        if (tp.syms[i].arity > 8) tp_giveup("functor arity exceeds 8");
      const char *base = strrchr(file, '/');
      base = (base != NULL) ? base + 1 : file;
      char pname[256];
      snprintf(pname, sizeof pname, "%.*s",
               (int)strcspn(base, "."), base);
      char *pr_text = tp_emit_pr(&tp, pname);
      if (keep_pr != 0) snprintf(tmp_pr, sizeof tmp_pr, "%s.pr", file);
      else {
        snprintf(tmp_pr, sizeof tmp_pr, "/tmp/thvm_atp_%d.pr", (int)getpid());
      }
      FILE *fp = fopen(tmp_pr, "w");
      if (fp == NULL) { fprintf(stderr, "FindProof: cannot write %s\n", tmp_pr); return 2; }
      fputs(pr_text, fp);
      fclose(fp);
      free(pr_text);
      if (keep_pr == 2) { printf("%s\n", tmp_pr); free(tpp); return 0; }
#undef tp
      free(tpp);
      run_file = tmp_pr;
    }
    free(raw);
  }

  g_cli_want_szs = szs;
  if (g_cli_want_proof) g_atp_bench_pre_free_hook = cli_proof_hook;
  // Competition-clean stdout: reroute the bench TU's diagnostic chatter
  // to stderr; the SZS lines go to the preserved real stdout.
  int real_stdout = -1;
  if (szs || g_cli_want_proof) {
    real_stdout = dup(1);
    g_cli_out = fdopen(dup(real_stdout), "w");
    fflush(stdout);
    dup2(2, 1);
  }

  char *bench_argv[5];
  bench_argv[0] = (char *)"FindProof";
  bench_argv[1] = (char *)run_file;
  bench_argv[2] = (char *)max_steps;
  bench_argv[3] = (char *)wall;
  bench_argv[4] = NULL;
  int rc = atp_bench_main(4, bench_argv);

  if (real_stdout >= 0) {
    fflush(stdout);
    dup2(real_stdout, 1);   // restore the real stdout for the SZS line
    close(real_stdout);
  }
  // Inside a schedule slice, only a SUCCESS prints its SZS line -- the
  // schedule wrapper owns the single final status otherwise.
  if (szs && getenv("FINDPROOF_SLICE") != NULL
      && g_atp_bench_status != (int)ATP_PROVED) szs = 0;
  if (g_cli_szs_printed) szs = 0;
  if (szs) {
    const char *tag;
    if (g_cli_tptp_mode) {
      tag = (g_atp_bench_status == (int)ATP_PROVED)      ? "Unsatisfiable" :
            (g_atp_bench_status == (int)ATP_QUEUE_EMPTY) ?
                (cli_strategy_complete() ? "Satisfiable" : "GaveUp") :
            (g_atp_bench_status == (int)ATP_TIMEOUT)     ? "ResourceOut" :
                                                           "Timeout";
    } else {
      tag = (g_atp_bench_status == (int)ATP_PROVED)      ? "Theorem" :
            (g_atp_bench_status == (int)ATP_QUEUE_EMPTY) ? "GaveUp" :
                                                           "ResourceOut";
    }
    printf("%% SZS status %s for %s\n", tag, file);
  }
  if (tmp_pr[0] != '\0' && !keep_pr) unlink(tmp_pr);
  if (g_atp_bench_status == (int)ATP_PROVED) return 0;
  return (rc != 0) ? rc : 1;
}
