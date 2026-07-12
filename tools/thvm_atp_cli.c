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
//
// Encoded-Horn preprocessing: problems that encode Horn logic through
// a dispatcher axiom ifeqN(X,X,Y,Z) = Y (TPTP's -10 UEQ class) are
// decoded and a budgeted forward-chaining release loop adds the
// modus-ponens consequences as extra unit equations before the engine
// runs -- see tp_horn_release.  --horn=0 disables.

#define main atp_bench_main
#include "../tests/test_atp_wolfram_bench.c"
#undef main

// Concurrent-portfolio process management (run_schedule).
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

// --check mode: validate a first-order TSTP proof (the ProoVer task).
// Reuses the src/fol saturation engine already linked into this binary.
#include "proof_check.c"

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
  {"max-cp-queue",           "THVM_ATP_MAX_CP_QUEUE",    1, "MaxCpQueue"},
  {"max-weight-slope",       "THVM_ATP_MAXW_SLOPE",      1, "AutoMaxWeight slope"},
  {"selection-ratio",        "THVM_ATP_SELECTION_RATIO", 1, "SelectionRatio"},
  {"goal-interleave",        "THVM_ATP_GOAL_INTERLEAVE", 1, "GoalInterleave"},
  {"ground-join",            "THVM_ATP_GROUND_JOIN",     0, "GroundJoin"},
  {"lazy-ground-join",       "THVM_ATP_LAZY_GJ",         0, "LazyGroundJoin"},
  {"connectedness",          "THVM_ATP_CONNECT",         0, "Connectedness"},
  {"rhs-interreduce",        "THVM_ATP_RHS_IR",          0, "RHSInterreduce"},
  {"unfailing-cp",           "THVM_ATP_UNFAILING",       0, "UnfailingCP"},
  {"cp-set-interreduce",     "THVM_ATP_CP_SET_IR",       0, "CPSetInterreduce"},
  {"cp-set-ir-period",       "THVM_ATP_CP_SET_IR_PERIOD",1, "CPSetInterreduce period"},
  {"cp-set-ir-budget",       "THVM_ATP_CP_SET_IR_BUDGET",1, "CPSetInterreduce budget % (Twee task discipline)"},
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
  {"flatten-goal",           "THVM_ATP_FLATTEN_GOAL",    0, "FlattenGoal"},
  {"goal-narrow",            "THVM_ATP_GOAL_NARROW",     0, "GoalNarrow (ground-CC + goal-cone e-match endgame)"},
  {"gn-stride",              "THVM_ATP_GN_STRIDE",       1, "(goal-narrow sweep stride, rule revisions)"},
  {"gn-iters",               "THVM_ATP_GN_ITERS",        1, "(goal-narrow e-match passes per sweep)"},
  {"gn-max-nodes",           "THVM_ATP_GN_MAX_NODES",    1, "(goal-narrow E-graph node cap)"},
  {"gn-max-inst",            "THVM_ATP_GN_MAX_INST",     1, "(goal-narrow e-match instance-node cap)"},
  {"gn-inst-sweep",          "THVM_ATP_GN_INST_SWEEP",   1, "(goal-narrow instance quota per sweep)"},
  {"gn-up-slack",            "THVM_ATP_GN_UP_SLACK",     1, "(goal-narrow expansion slack, symbols)"},
  {"gn-cone",                "THVM_ATP_GN_CONE",         1, "(goal-narrow subjects: 1 goal-cone only, 0 any small node)"},
  {"gn-subjects",            "THVM_ATP_GN_SUBJECTS",     1, "(goal-narrow e-match subjects per pass)"},
  {"gn-subj-size",           "THVM_ATP_GN_SUBJ_SIZE",    1, "(goal-narrow max subject symbols)"},
  {"gn-inst-size",           "THVM_ATP_GN_INST_SIZE",    1, "(goal-narrow max instance symbols)"},
  {"backward-subsume",       "THVM_ATP_BWD_SUB",         0, "BackwardSubsume"},
  {"backward-demod",         "THVM_ATP_BWD_DEMOD",       0, "BackwardDemod"},
  {"backward-ground-join",   "THVM_ATP_BWD_GROUND_JOIN", 0, "BackwardGroundJoin"},
  {"gj-delete-rule",         "THVM_ATP_GJ_DELETE_RULE",  0, "GjDeleteRule"},
  {"gj-index",               "THVM_ATP_GJ_INDEX",        1, "(GJ indexed top scan; default 1)"},
  {"encompass-demod",        "THVM_ATP_ENCOMPASS",       0, "(skolem-ground unorient demodulation)"},
  {"simpler-join",           "THVM_ATP_SIMPLER_JOIN",    0, "(Twee simplerThan-gated CP join)"},
  {"pop-rules-only",         "THVM_ATP_POP_RULES_ONLY",  0, "(Twee pop-join base: oriented rules only, no ungated doE)"},
  {"gj-budget",              "THVM_ATP_GJ_BUDGET",       1, "(GJ branch budget; small = early-KEEP)"},
  {"full-interreduce",       "THVM_ATP_FULL_INTERREDUCE",0, "FullInterreduce"},
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
  {"sym-prec",               "THVM_ATP_SYM_PREC",        1, "(Vampire symbol precedence: reverse-frequency|frequency)"},
  {"equals-goal",            "THVM_ATP_EQUALS_GOAL",     1, "(existential-goal equals encoding; 2 = give up if inapplicable)"},
  {"curry",                  "THVM_ATP_CURRY",           1, "(wide-functor n-ary->binary ap2 decomposition; 0 disables)"},
  {"horn",                   "THVM_ATP_HORN",            1, "(encoded-Horn release: 0 disables)"},
  {"horn-ratio",             "THVM_ATP_HORN_RATIO",      1, "(Horn pick-given size:age ratio, 0..4)"},
  {"horn-keep",              "THVM_ATP_HORN_KEEP",       1, "(Horn kept-release cap)"},
  {"horn-wall",              "THVM_ATP_HORN_WALL",       1, "(Horn chaining wall, seconds)"},
  {"horn-max",               "THVM_ATP_HORN_MAX",        1, "(Horn release-pool cap)"},
  {"horn-ground",            "THVM_ATP_HORN_GROUND",     1, "(Horn rank: ground tier)"},
  {"horn-bwd",               "THVM_ATP_HORN_BWD",        1, "(Horn backward subgoal cone: 0 disables)"},
  {"horn-bwd-depth",         "THVM_ATP_HORN_BWD_DEPTH",  1, "(Horn backward cone depth cap)"},
  {"horn-bwd-max",           "THVM_ATP_HORN_BWD_MAX",    1, "(Horn backward cone subgoal cap)"},
  {"horn-goal-stop",         "THVM_ATP_HORN_GOAL_STOP",  1, "(stop chaining on a goal-subsuming release: 0 disables)"},
  {"horn-rel",               "THVM_ATP_HORN_REL",        1, "(Horn relevance gate on oversized KBs: 0 disables)"},
  {"horn-rel-tol",           "THVM_ATP_HORN_REL_TOL",    1, "(Horn relevance-gate SInE tolerance)"},
  {"horn-cd",                "THVM_ATP_HORN_CD",         1, "(condensed-detachment lane: subsumption + size-cap deepening; 2 = give up if inapplicable)"},
  {"horn-cd-cap",            "THVM_ATP_HORN_CD_CAP",     1, "(CD initial released-size cap; default auto)"},
  {"horn-cd-step",           "THVM_ATP_HORN_CD_STEP",    1, "(CD deepening cap increment)"},
  {"horn-cd-vars",           "THVM_ATP_HORN_CD_VARS",    1, "(CD max distinct variables per conclusion)"},
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
// g_cli_sine_applied: SInE dropped axioms, so the engine saturated an
// INCOMPLETE subset -- a saturation then proves nothing about the full
// problem and must report GaveUp, never Satisfiable.
static int g_cli_goal_existential = 0;
static int g_cli_sine_applied     = 0;
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

#define TP_MAX_CLAUSES  65536
#define TP_MAX_SYMS     262144
#define TP_MAX_VARS     4096
#define TP_NAME_LEN     64
#define TP_SIDE_LEN     2048

typedef struct {
  char lhs[TP_SIDE_LEN], rhs[TP_SIDE_LEN];
  int  neg;
  int  conj;   // TPTP role negated_conjecture (a positive '=' unit may
               // carry it: the refutation encoding's hypothesis parts)
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

// Open-addressing name hash over the symbol table: the linear lookup is
// O(n_syms) per token, unusable on the CSR-class 8k-clause / 10k-symbol
// problems.  Key = name only (the arity-clash check still runs on hit).
#define TP_SYM_HASH_SLOTS (TP_MAX_SYMS * 2)
static int g_tp_sym_hash[TP_SYM_HASH_SLOTS];
static u8  g_tp_sym_hash_init = 0;
static u64 tp_name_hash(const char *name, size_t n) {
  u64 h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) h = (h ^ (u8)name[i]) * 1099511628211ull;
  return h;
}
static int tp_sym_note_ix(TpProblem *tp, const char *name, size_t n, int arity) {
  if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
  if (!g_tp_sym_hash_init) {
    for (int i = 0; i < TP_SYM_HASH_SLOTS; i++) g_tp_sym_hash[i] = -1;
    g_tp_sym_hash_init = 1;
  }
  u64 h = tp_name_hash(name, n);
  u32 slot = (u32)(h % (u64)TP_SYM_HASH_SLOTS);
  for (;;) {
    int ix = g_tp_sym_hash[slot];
    if (ix < 0) break;
    if (strlen(tp->syms[ix].name) == n &&
        memcmp(tp->syms[ix].name, name, n) == 0) {
      if (tp->syms[ix].arity != arity) {
        fprintf(stderr, "FindProof: symbol %.*s used with arities %d and %d\n",
                (int)n, name, tp->syms[ix].arity, arity);
        exit(2);
      }
      return ix;
    }
    slot = (slot + 1u) % (u32)TP_SYM_HASH_SLOTS;
  }
  if (tp->n_syms >= TP_MAX_SYMS) tp_giveup("problem exceeds symbol bound");
  memcpy(tp->syms[tp->n_syms].name, name, n);
  tp->syms[tp->n_syms].name[n] = '\0';
  tp->syms[tp->n_syms].arity = arity;
  g_tp_sym_hash[slot] = tp->n_syms;
  return tp->n_syms++;
}
static void tp_sym_note(TpProblem *tp, const char *name, size_t n, int arity) {
  (void)tp_sym_note_ix(tp, name, n, arity);
}
// Reset the symbol table + hash (the post-SInE signature rebuild).
static void tp_syms_reset(TpProblem *tp) {
  tp->n_syms = 0;
  tp->n_vars = 0;
  for (int i = 0; i < TP_SYM_HASH_SLOTS; i++) g_tp_sym_hash[i] = -1;
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

// The CSR knowledge bases carry symbol names up to 224 chars; both the
// TpSym table and the engine's .pr reader cap names at 64, and silent
// truncation would conflate distinct symbols (and desync the SIGNATURE
// from the verbatim equation text).  Shorten every over-long
// identifier IN PLACE to prefix + "_h" + 16-hex-FNV(full name) -- 58
// chars, deterministic, collision-safe in practice, case of the first
// character (variable-ness) preserved.
// TPTP quoted atoms ('+', '>=', ...) survive preprocessing as-is, but
// no downstream identifier grammar (.pr emission, the engine reader,
// the Horn parser) admits them -- LCL890-10 died on a pr parse error.
// Rewrite every DISTINCT quoted atom to a fresh plain name (qtd0,
// qtd1, ...): an isomorphic renaming, so provability is untouched.
// Candidate names are collision-guarded against the input text.
static char *tp_unquote_names(char *text) {
  if (strchr(text, '\'') == NULL) return text;
#define TP_MAX_QUOTED 512
  char *qname[TP_MAX_QUOTED];
  char  qrepl[TP_MAX_QUOTED][24];
  int nq = 0;
  size_t cap = strlen(text) + 4096;
  char *out = (char *)malloc(cap);
  size_t o = 0;
  if (out == NULL) tp_giveup("OOM in quoted-atom rename");
  for (const char *r = text; *r != '\0'; ) {
    if (o + 32 >= cap) {
      cap *= 2;
      out = (char *)realloc(out, cap);
      if (out == NULL) tp_giveup("OOM in quoted-atom rename");
    }
    if (*r != '\'') { out[o++] = *r++; continue; }
    const char *q2 = r + 1;
    while (*q2 != '\0' && *q2 != '\'')
      q2 += (q2[0] == '\\' && q2[1] != '\0') ? 2 : 1;
    if (*q2 == '\0' || q2 == r + 1) { out[o++] = *r++; continue; }
    size_t n = (size_t)(q2 - (r + 1));
    int ix = -1;
    for (int i = 0; i < nq; i++)
      if (strlen(qname[i]) == n && memcmp(qname[i], r + 1, n) == 0) { ix = i; break; }
    if (ix < 0 && nq < TP_MAX_QUOTED) {
      qname[nq] = strndup(r + 1, n);
      snprintf(qrepl[nq], sizeof qrepl[nq], "qtd%d", nq);
      while (strstr(text, qrepl[nq]) != NULL &&
             strlen(qrepl[nq]) + 2 < sizeof qrepl[nq])
        strcat(qrepl[nq], "x");
      ix = nq++;
    }
    if (ix < 0) { out[o++] = *r++; continue; }   // table full: leave as-is
    size_t rl = strlen(qrepl[ix]);
    memcpy(out + o, qrepl[ix], rl); o += rl;
    r = q2 + 1;
  }
  out[o] = '\0';
  for (int i = 0; i < nq; i++) free(qname[i]);
  free(text);
  return out;
}

static void tp_shorten_names(char *text) {
  char *r = text, *w = text;
  while (*r != '\0') {
    if (isalnum((unsigned char)*r) || *r == '_') {
      size_t n = 0;
      while (isalnum((unsigned char)r[n]) || r[n] == '_') n++;
      if (n >= TP_NAME_LEN) {
        u64 h = tp_name_hash(r, n);
        memmove(w, r, 40);
        w += 40;
        w += snprintf(w, 20, "_h%016llx", (unsigned long long)h);
      } else {
        memmove(w, r, n);
        w += n;
      }
      r += n;
    } else {
      *w++ = *r++;
    }
  }
  *w = '\0';
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
    const char *role = NULL;
    for (size_t i = 0; i < blen; i++) {
      char c = body[i];
      if (c == '(') d++;
      else if (c == ')') d--;
      else if (c == ',' && d == 0) {
        commas++;
        if (commas == 1) role = body + i + 1;
        if (commas == 2) { lit = body + i + 1; break; }
      }
    }
    if (lit == NULL) continue;
    int is_conj = 0;
    if (role != NULL) {
      size_t rl = (size_t)(lit - 1 - role);
      is_conj = (memmem(role, rl, "negated_conjecture", 18) != NULL);
    }
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
    cl->conj = is_conj;
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

// ── SInE-style goal-directed axiom selection (Hoder-Voronkov shape) ──
// The CSR-class UEQ problems are 8k-clause knowledge bases; completion
// drowns before touching the goal.  Select the goal-relevant slice:
//   gen(s)   = number of clauses containing symbol s (generality);
//   trigger  = clause C is triggered by symbol s in C iff
//              gen(s) <= tol * min_gen(C)  (s is near-least-general in C);
//   reach    = start from the negative units' symbols; a triggered
//              clause joins the selection and its symbols become
//              reached; iterate to fixpoint (or `rounds` levels).
// The encoding glue (ifeq4/true, generality in the thousands) never
// triggers, exactly as SInE intends.  Negative units are always kept.
// Returns the kept count; compacts tp->cls in place.
static int tp_sine_pass(TpProblem *tp, double tol, int rounds) {
  int n = tp->n_cls;
  enum { SINE_CS = 48 };
  int (*csym)[SINE_CS] = malloc((size_t)n * sizeof *csym);
  int *csym_n = calloc((size_t)n, sizeof(int));
  u32 *gen    = calloc((size_t)tp->n_syms, sizeof(u32));
  u8  *reach  = calloc((size_t)tp->n_syms, 1);
  u8  *keep   = calloc((size_t)n, 1);
  if (csym == NULL || csym_n == NULL || gen == NULL || reach == NULL ||
      keep == NULL) tp_giveup("OOM in axiom selection");
  // Per-clause dedup'd symbol-id lists (functors + constants, no vars).
  for (int c = 0; c < n; c++) {
    const char *sides[2] = { tp->cls[c].lhs, tp->cls[c].rhs };
    for (int sd = 0; sd < 2; sd++) {
      const char *pp = sides[sd];
      while (*pp != '\0') {
        if (isalnum((unsigned char)*pp) || *pp == '_') {
          const char *name = pp;
          size_t ln = 0;
          while (isalnum((unsigned char)pp[ln]) || pp[ln] == '_') ln++;
          if (!isupper((unsigned char)*name)) {
            const char *q = pp + ln;
            while (isspace((unsigned char)*q)) q++;
            int arity = 0;
            if (*q == '(') {
              int depth = 1; arity = 1;
              const char *r = q + 1;
              while (*r != '\0' && depth > 0) {
                if (*r == '(') depth++;
                else if (*r == ')') depth--;
                else if (*r == ',' && depth == 1) arity++;
                r++;
              }
            }
            int ix = tp_sym_note_ix(tp, name, ln, arity);
            u8 seen = 0;
            for (int k = 0; k < csym_n[c]; k++)
              if (csym[c][k] == ix) { seen = 1; break; }
            if (!seen && csym_n[c] < SINE_CS) csym[c][csym_n[c]++] = ix;
          }
          pp += ln;
        } else pp++;
      }
    }
    for (int k = 0; k < csym_n[c]; k++) gen[csym[c][k]]++;
  }
  // Seed: every conjecture-role unit (the refutation encoding splits
  // the query into a '!=' goal plus positive '=' hypothesis units, all
  // carrying negated_conjecture) -- kept unconditionally, symbols
  // reached.
  for (int c = 0; c < n; c++) {
    if (!tp->cls[c].neg && !tp->cls[c].conj) continue;
    keep[c] = 1;
    for (int k = 0; k < csym_n[c]; k++) reach[csym[c][k]] = 1;
  }
  // Levels to fixpoint (or the round cap).
  for (int rd = 0; rd < rounds; rd++) {
    u8 grew = 0;
    for (int c = 0; c < n; c++) {
      if (keep[c]) continue;
      u32 mingen = 0xffffffffu;
      for (int k = 0; k < csym_n[c]; k++)
        if (gen[csym[c][k]] < mingen) mingen = gen[csym[c][k]];
      u8 hit = 0;
      for (int k = 0; k < csym_n[c] && !hit; k++) {
        int s = csym[c][k];
        if (reach[s] && (double)gen[s] <= tol * (double)mingen) hit = 1;
      }
      if (hit) {
        keep[c] = 1;
        grew = 1;
        for (int k = 0; k < csym_n[c]; k++) reach[csym[c][k]] = 1;
      }
    }
    if (!grew) break;
  }
  int w = 0;
  for (int c = 0; c < n; c++) {
    if (!keep[c]) continue;
    if (w != c) tp->cls[w] = tp->cls[c];
    w++;
  }
  free(csym); free(csym_n); free(gen); free(reach); free(keep);
  return w;
}

// Driver: progressively tighten until the selection fits the engine
// caps.  On success rebuilds the signature from the kept clauses (the
// dropped ones own most of the 10k-symbol table).  THVM_ATP_SINE=0
// disables; THVM_ATP_SINE_TOL overrides the first tolerance.
static void tp_sine_select(TpProblem *tp) {
  double tol0 = 1.5;
  {
    const char *e = getenv("THVM_ATP_SINE_TOL");
    if (e != NULL && e[0] != '\0') tol0 = atof(e);
    if (tol0 < 1.0) tol0 = 1.0;
  }
  const double tols[4]  = { tol0, 1.5, 1.0, 1.0 };
  const int    rnds[4]  = { 64,   3,   2,   1  };
  int n0 = tp->n_cls;
  for (int a = 0; a < 4; a++) {
    tp->n_cls = n0;   // each attempt filters the (previously kept) set
    int kept = tp_sine_pass(tp, tols[a], rnds[a]);
    int n_neg = 0;
    for (int i = 0; i < kept; i++) n_neg += tp->cls[i].neg;
    if ((kept - n_neg) <= (int)PR_MAX_EQNS) {
      tp->n_cls = kept;
      tp_syms_reset(tp);
      for (int i = 0; i < kept; i++) {
        tp_scan_term(tp, tp->cls[i].lhs);
        tp_scan_term(tp, tp->cls[i].rhs);
      }
      fprintf(stderr, "FindProof: SInE selected %d/%d clauses (tol=%.2f)\n",
              kept, n0, tols[a]);
      g_cli_sine_applied = 1;
      return;
    }
    // over the cap: the pass already compacted keep-first, but a
    // tighter re-run must see the ORIGINAL clause set -- it does not
    // (dropped clauses were overwritten).  Tighten on the KEPT set
    // instead: strictly narrower each attempt, so this converges.
    tp->n_cls = kept;
    n0 = kept;
  }
  tp_giveup("axiom selection cannot fit the engine caps");
}

// ===================================================================
// Existential-goal equals encoding (Twee addNarrowing port)
// ===================================================================
//
// Twee never narrows a non-ground goal directly: addNarrowing
// (twee executable/SequentialMain.hs:721) encodes each existential
// goal  s != t  (a variable in the re-negated conjecture is
// existentially quantified) equationally --
//     eq(X,X) = true          (reflexivity)
//     eq(s,t) = false         (one per non-ground goal; the goal's
//                              variables turn universally quantified)
//     true   != false         (the single GROUND replacement goal)
// -- and completion narrows the eq(s,t) axiom at its argument
// positions until the two arguments unify, at which point the root
// overlap with reflexivity yields true = false: the refutation closes
// through an ordinary ground-goal join, no narrowing lane involved.
//
// Conservativity (Twee tags the step "esa" -- equisatisfiable): with
// eq/true/false FRESH symbols,
//   (=>) if S u { s != t } is unsatisfiable, some instance s@ = t@ is
//        S-derivable (Birkhoff), so eq(s@,t@) rewrites to true via
//        reflexivity AND to false via the goal axiom: true = false,
//        contradicting the new goal -- the encoding is unsatisfiable.
//   (<=) a model M of S u { forall X. s != t } extends to the
//        encoding: the goal instances give two distinct elements
//        a != b; set true^M = a, false^M = b, eq^M(u,v) = a if u = v
//        else b.  Reflexivity holds, each eq(s,t) = false holds
//        (every instance of s,t differs in M), true != false holds --
//        the encoding is satisfiable.
// Unsatisfiable therefore transfers both ways.  A saturation under
// the encoding still reports GaveUp, never Satisfiable
// (g_cli_equals_applied, the g_cli_*_applied gate pattern) -- belt
// and suspenders on top of the g_cli_goal_existential gate.
//
// OPT-IN (--equals-goal=1), not the default: the engine's narrowing
// lane decides most of the corpus's existential goals instantly
// (COL004-1 proves at 0.0s narrowing but TIMES OUT encoded), while
// the encoding cracks the goals narrowing starves on (COL009-1
// proves at 0.1s encoded but times out narrowing) -- complementary
// lanes, so the schedule runs the encoding as its own slice.
// --equals-goal=2 additionally EXITS (give-up) when the encoding
// does not apply, so the slice never duplicates the WM preset on
// ground-goal (or .pr) problems.

static int g_cli_equals_applied = 0;

// Does a clause side contain a variable (uppercase-initial token)?
static int tp_side_has_var(const char *s) {
  for (const char *q = s; *q != '\0'; q++) {
    if (isupper((unsigned char)*q) &&
        (q == s || !(isalnum((unsigned char)q[-1]) || q[-1] == '_')))
      return 1;
  }
  return 0;
}

// A fresh function-symbol name: `base`, suffixed with 'q's until it
// matches nothing in the symbol table (any arity).
static void tp_fresh_sym(const TpProblem *tp, const char *base,
                         char *out, size_t cap) {
  snprintf(out, cap, "%s", base);
  for (;;) {
    int clash = 0;
    for (int i = 0; i < tp->n_syms && !clash; i++)
      clash = (strcmp(tp->syms[i].name, out) == 0);
    if (!clash) return;
    size_t l = strlen(out);
    if (l + 2 >= cap) tp_giveup("cannot make a fresh symbol name");
    out[l] = 'q'; out[l + 1] = '\0';
  }
}

static void tp_equals_goal(TpProblem *tp) {
  int n_nonground = 0;
  for (int i = 0; i < tp->n_cls; i++)
    if (tp->cls[i].neg && (tp_side_has_var(tp->cls[i].lhs) ||
                           tp_side_has_var(tp->cls[i].rhs)))
      n_nonground++;
  if (n_nonground == 0) return;
  // Fall back to the narrowing lane if the encoding cannot fit.
  if (tp->n_cls + 2 > TP_MAX_CLAUSES) return;
  char eqn[TP_NAME_LEN], tru[TP_NAME_LEN], fls[TP_NAME_LEN];
  tp_fresh_sym(tp, "eq",     eqn, sizeof eqn);
  tp_fresh_sym(tp, "etrue",  tru, sizeof tru);
  tp_fresh_sym(tp, "efalse", fls, sizeof fls);
  for (int i = 0; i < tp->n_cls; i++) {
    TpClause *cl = &tp->cls[i];
    if (!cl->neg || (!tp_side_has_var(cl->lhs) && !tp_side_has_var(cl->rhs)))
      continue;
    if (strlen(eqn) + strlen(cl->lhs) + strlen(cl->rhs) + 4 >= TP_SIDE_LEN)
      return;
  }
  // Rewrite each non-ground goal in place:  eq(s,t) = false.
  for (int i = 0; i < tp->n_cls; i++) {
    TpClause *cl = &tp->cls[i];
    if (!cl->neg || (!tp_side_has_var(cl->lhs) && !tp_side_has_var(cl->rhs)))
      continue;
    char enc[TP_SIDE_LEN];
    snprintf(enc, sizeof enc, "%s(%s,%s)", eqn, cl->lhs, cl->rhs);
    memcpy(cl->lhs, enc, strlen(enc) + 1u);
    snprintf(cl->rhs, sizeof cl->rhs, "%s", fls);
    cl->neg  = 0;
    cl->conj = 1;   // conjecture-derived: SInE keeps it unconditionally
    tp_scan_term(tp, cl->lhs);
    tp_scan_term(tp, cl->rhs);
  }
  // Reflexivity:  eq(V,V) = true  (any declared variable serves; a
  // non-ground goal guarantees at least one).
  TpClause *rfl = &tp->cls[tp->n_cls++];
  snprintf(rfl->lhs, sizeof rfl->lhs, "%s(%s,%s)",
           eqn, tp->vars[0], tp->vars[0]);
  snprintf(rfl->rhs, sizeof rfl->rhs, "%s", tru);
  rfl->neg  = 0;
  rfl->conj = 1;
  tp_scan_term(tp, rfl->lhs);
  tp_scan_term(tp, rfl->rhs);
  // The replacement goal:  true != false.
  TpClause *gl = &tp->cls[tp->n_cls++];
  snprintf(gl->lhs, sizeof gl->lhs, "%s", tru);
  snprintf(gl->rhs, sizeof gl->rhs, "%s", fls);
  gl->neg  = 1;
  gl->conj = 1;
  tp_scan_term(tp, gl->lhs);
  tp_scan_term(tp, gl->rhs);
  g_cli_equals_applied = 1;
  fprintf(stderr,
          "FindProof: equals-encoded %d existential goal(s) (%s/%s/%s)\n",
          n_nonground, eqn, tru, fls);
}

// ===================================================================
// Wide-functor currying (n-ary decomposition into binary applications)
// ===================================================================
//
// The .pr flatterm term reader holds at most 8 arguments per node
// (test_atp_wolfram_bench.c pr_term: Term kids[8]); a higher-arity
// functor (TOP052's tuple/140) is otherwise unreadable and the CLI
// gives up.  Curry each application of a wide functor f/n (n > 8) into
// a left-nested chain of a fresh BINARY symbol ap2:
//     f(t1,...,tn)  ->  ap2(ap2(...ap2(f, c1), c2)..., cn)
// where f becomes a 0-arity constant and each ci is the recursively
// curried argument.  This is a bijective term re-encoding: ap2 is a
// fresh free binary symbol carrying no equations, so two curried terms
// are equal (under congruence) iff their heads agree and every argument
// is equal -- exactly the equality the original n-ary functor induced.
// Provability is therefore unchanged; only the representation shrinks
// to arities the reader admits.  A no-op unless some functor exceeds 8,
// so it never perturbs the byte-exact WM emulation on existing corpora.
#define CURRY_ARITY_CAP 8

// Recursive-descent term rewriter over a clause-side string.  Returns a
// malloc'd rewritten term; sets *endp past the input it consumed.
static char *curry_term(const char *s, const char **endp, const char *ap2) {
  while (*s == ' ') s++;
  const char *name = s;
  size_t n = 0;
  while (isalnum((unsigned char)s[n]) || s[n] == '_') n++;
  const char *p = s + n;
  while (*p == ' ') p++;
  if (*p != '(') {                       // variable or 0-arity atom
    *endp = p;
    return strndup(name, n);
  }
  p++;                                   // consume '('
  char **args = NULL;
  int na = 0, cap = 0;
  for (;;) {
    const char *e;
    char *a = curry_term(p, &e, ap2);
    if (na >= cap) { cap = cap ? cap * 2 : 8; args = realloc(args, (size_t)cap * sizeof *args); }
    args[na++] = a;
    p = e;
    while (*p == ' ') p++;
    if (*p == ',') { p++; continue; }
    if (*p == ')') { p++; break; }
    break;                               // malformed: stop at first junk
  }
  *endp = p;
  char *out;
  if (na <= CURRY_ARITY_CAP) {           // narrow: re-emit as-is
    size_t len = n + 2;
    for (int i = 0; i < na; i++) len += strlen(args[i]) + 1;
    out = (char *)malloc(len + 1);
    size_t o = 0;
    memcpy(out + o, name, n); o += n; out[o++] = '(';
    for (int i = 0; i < na; i++) {
      if (i) out[o++] = ',';
      size_t l = strlen(args[i]); memcpy(out + o, args[i], l); o += l;
    }
    out[o++] = ')'; out[o] = '\0';
  } else {                               // wide: left-nested ap2 chain
    char *cur = strndup(name, n);        // head becomes a 0-arity atom
    size_t apl = strlen(ap2);
    for (int i = 0; i < na; i++) {
      size_t cl = strlen(cur), al = strlen(args[i]);
      size_t l = apl + 1 + cl + 1 + al + 1;
      char *nx = (char *)malloc(l + 1);
      size_t o = 0;
      memcpy(nx + o, ap2, apl); o += apl; nx[o++] = '(';
      memcpy(nx + o, cur, cl); o += cl; nx[o++] = ',';
      memcpy(nx + o, args[i], al); o += al; nx[o++] = ')'; nx[o] = '\0';
      free(cur); cur = nx;
    }
    out = cur;
  }
  for (int i = 0; i < na; i++) free(args[i]);
  free(args);
  return out;
}

static void tp_curry_wide(TpProblem *tp) {
  int wide = 0;
  for (int i = 0; i < tp->n_syms; i++)
    if (tp->syms[i].arity > CURRY_ARITY_CAP) { wide = 1; break; }
  if (!wide) return;
  char ap2[TP_NAME_LEN];
  tp_fresh_sym(tp, "ap2", ap2, sizeof ap2);
  int changed = 0;
  for (int c = 0; c < tp->n_cls; c++) {
    TpClause *cl = &tp->cls[c];
    char *sides[2] = { cl->lhs, cl->rhs };
    for (int sd = 0; sd < 2; sd++) {
      const char *e;
      char *rw = curry_term(sides[sd], &e, ap2);
      if (strcmp(rw, sides[sd]) != 0) {
        if (strlen(rw) >= TP_SIDE_LEN) { free(rw); tp_giveup("curried term exceeds side buffer"); }
        memcpy(sides[sd], rw, strlen(rw) + 1u);
        changed++;
      }
      free(rw);
    }
  }
  tp_syms_reset(tp);
  for (int i = 0; i < tp->n_cls; i++) {
    tp_scan_term(tp, tp->cls[i].lhs);
    tp_scan_term(tp, tp->cls[i].rhs);
  }
  fprintf(stderr,
          "FindProof: curried %d wide-functor side(s) via %s (bijective n-ary decomposition)\n",
          changed, ap2);
}

// ===================================================================
// Encoded-Horn (ifeq) decode + forward-chaining release
// ===================================================================
//
// TPTP's -10 UEQ problems encode Horn logic as unit equations: a
// dispatcher axiom ifeqN(X,X,Y,Z) = Y (several functor names may
// coexist: ifeq, ifeq2, ...; arity-3 variants ifeq(X,X,Y) = Y too)
// plus conditional clauses ifeqN(c, t, then, e) = e -- "if c = t
// then then = e" -- nesting in the then-position for multiple
// conditions, with propositions as terms equated to the constant
// true.  Pure completion drowns re-deriving the conditional
// machinery, so this pass decodes the encoding and runs a budgeted
// forward-chaining release loop (positive hyper-resolution): whenever
// every condition of a decoded clause simultaneously unifies with
// known unit facts (each fact renamed apart), the instantiated
// conclusion is released as a new unit equation.  Released units are
// logical consequences of the input, so Unsatisfiable stays valid;
// g_cli_horn_applied gates the Satisfiable claim exactly like
// g_cli_sine_applied does.  THVM_ATP_HORN=0 disables; otherwise the
// pass is a no-op unless a dispatcher axiom is detected.

static int g_cli_horn_applied = 0;

// Terms live in a growable node/arg arena.  head >= 0 is an index
// into the TpProblem symbol table; head < 0 is variable -(v+1) with v
// local to the owning clause.  Unification runs over (node, var-space
// offset) pairs: the rule's variables are offset 0, the fact renamed
// into condition i's space at offset (i+1)*HORN_VSPACE.
#define HORN_MAX_COND   8
#define HORN_VSPACE     64
// Var spaces: rule 0, condition facts 1..HORN_MAX_COND, subgoal
// patterns HORN_SG_OFF, probe side HORN_PROBE_OFF (a clean space for
// unifying a canonical release against a subgoal while a join's
// bindings are still live in spaces 0..HORN_MAX_COND).
#define HORN_SG_OFF     ((HORN_MAX_COND + 1) * HORN_VSPACE)
#define HORN_PROBE_OFF  ((HORN_MAX_COND + 2) * HORN_VSPACE)
#define HORN_NSLOTS     ((HORN_MAX_COND + 3) * HORN_VSPACE)
#define HORN_MAX_RELEASES 400000
#define HORN_MAX_ATTEMPTS 10000000L
#define HORN_WALL_SEC   2.5
#define HORN_SIDE_MAX   900     // printed chars per released side
#define HORN_APPLY_MAX  400     // nodes per released side
#define HORN_REL_VARS   12      // distinct vars per released unit

typedef struct { int head, nargs, argp; } HNode;
static HNode *g_hn = NULL; static int g_hn_n = 0, g_hn_cap = 0;
static int   *g_ha = NULL; static int g_ha_n = 0, g_ha_cap = 0;

static int horn_node(int head, int nargs, const int *args) {
  if (g_hn_n >= g_hn_cap) {
    g_hn_cap = (g_hn_cap == 0) ? (1 << 16) : g_hn_cap * 2;
    g_hn = (HNode *)realloc(g_hn, (size_t)g_hn_cap * sizeof(HNode));
  }
  if (nargs > 0 && g_ha_n + nargs > g_ha_cap) {
    g_ha_cap = (g_ha_cap == 0) ? (1 << 16) : g_ha_cap * 2 + nargs;
    g_ha = (int *)realloc(g_ha, (size_t)g_ha_cap * sizeof(int));
  }
  if (g_hn == NULL || (nargs > 0 && g_ha == NULL)) tp_giveup("OOM in Horn release");
  HNode *nd = &g_hn[g_hn_n];
  nd->head = head; nd->nargs = nargs; nd->argp = g_ha_n;
  for (int i = 0; i < nargs; i++) g_ha[g_ha_n + i] = args[i];
  g_ha_n += nargs;
  return g_hn_n++;
}
#define HARG(t, i) (g_ha[g_hn[(t)].argp + (i)])

// --- per-clause recursive-descent parser ----------------------------
typedef struct {
  const char *p;
  int  nv;
  char vnames[HORN_VSPACE][TP_NAME_LEN];
  int  fail;
} HParse;

static int horn_parse_term(HParse *ps, TpProblem *tp, int depth) {
  if (ps->fail || depth > 200) { ps->fail = 1; return -1; }
  while (isspace((unsigned char)*ps->p)) ps->p++;
  const char *name = ps->p;
  size_t n = 0;
  while (isalnum((unsigned char)name[n]) || name[n] == '_') n++;
  if (n == 0) { ps->fail = 1; return -1; }
  ps->p += n;
  while (isspace((unsigned char)*ps->p)) ps->p++;
  if (*ps->p == '(') {
    ps->p++;
    int args[16], na = 0;
    for (;;) {
      if (na >= 16) { ps->fail = 1; return -1; }
      args[na++] = horn_parse_term(ps, tp, depth + 1);
      if (ps->fail) return -1;
      while (isspace((unsigned char)*ps->p)) ps->p++;
      if (*ps->p == ',') { ps->p++; continue; }
      if (*ps->p == ')') { ps->p++; break; }
      ps->fail = 1; return -1;
    }
    return horn_node(tp_sym_note_ix(tp, name, n, na), na, args);
  }
  if (isupper((unsigned char)*name)) {
    if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
    for (int i = 0; i < ps->nv; i++)
      if (strlen(ps->vnames[i]) == n && memcmp(ps->vnames[i], name, n) == 0)
        return horn_node(-(i + 1), 0, NULL);
    if (ps->nv >= HORN_VSPACE) { ps->fail = 1; return -1; }
    memcpy(ps->vnames[ps->nv], name, n);
    ps->vnames[ps->nv][n] = '\0';
    return horn_node(-(++ps->nv), 0, NULL);
  }
  return horn_node(tp_sym_note_ix(tp, name, n, 0), 0, NULL);
}

// Parse one clause side into the arena; vars are clause-local via ps.
static int horn_parse_side(TpProblem *tp, HParse *ps, const char *s) {
  ps->p = s; ps->fail = 0;
  int t = horn_parse_term(ps, tp, 0);
  if (ps->fail) return -1;
  while (isspace((unsigned char)*ps->p)) ps->p++;
  return (*ps->p == '\0') ? t : -1;
}

// --- structural equality (same var space) ---------------------------
static int horn_eq(int a, int b) {
  if (g_hn[a].head != g_hn[b].head || g_hn[a].nargs != g_hn[b].nargs) return 0;
  for (int i = 0; i < g_hn[a].nargs; i++)
    if (!horn_eq(HARG(a, i), HARG(b, i))) return 0;
  return 1;
}

// --- unification with trail ------------------------------------------
typedef struct { int t, off; } HBind;
static HBind g_hb[HORN_NSLOTS];
static int g_htrail[HORN_NSLOTS]; static int g_htrail_n = 0;
static long g_horn_attempts = 0;

static void horn_deref(int *t, int *off) {
  while (g_hn[*t].head < 0) {
    int slot = *off + (-g_hn[*t].head - 1);
    if (g_hb[slot].t < 0) return;
    int nt = g_hb[slot].t, noff = g_hb[slot].off;
    *t = nt; *off = noff;
  }
}

static int horn_occurs(int slot, int t, int off, int depth) {
  if (depth > 400) return 1;   // treat overdeep as occurs: fail the bind
  horn_deref(&t, &off);
  if (g_hn[t].head < 0) return off + (-g_hn[t].head - 1) == slot;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_occurs(slot, HARG(t, i), off, depth + 1)) return 1;
  return 0;
}

static int horn_unify(int t1, int o1, int t2, int o2, int depth) {
  if (depth > 400) return 0;
  horn_deref(&t1, &o1); horn_deref(&t2, &o2);
  if (g_hn[t1].head < 0 && g_hn[t2].head < 0 &&
      o1 + (-g_hn[t1].head - 1) == o2 + (-g_hn[t2].head - 1)) return 1;
  if (g_hn[t1].head < 0) {
    int slot = o1 + (-g_hn[t1].head - 1);
    if (horn_occurs(slot, t2, o2, 0)) return 0;
    g_hb[slot].t = t2; g_hb[slot].off = o2;
    g_htrail[g_htrail_n++] = slot;
    return 1;
  }
  if (g_hn[t2].head < 0) {
    int slot = o2 + (-g_hn[t2].head - 1);
    if (horn_occurs(slot, t1, o1, 0)) return 0;
    g_hb[slot].t = t1; g_hb[slot].off = o1;
    g_htrail[g_htrail_n++] = slot;
    return 1;
  }
  if (g_hn[t1].head != g_hn[t2].head || g_hn[t1].nargs != g_hn[t2].nargs) return 0;
  for (int i = 0; i < g_hn[t1].nargs; i++)
    if (!horn_unify(HARG(t1, i), o1, HARG(t2, i), o2, depth + 1)) return 0;
  return 1;
}

static void horn_undo(int mark) {
  while (g_htrail_n > mark) g_hb[g_htrail[--g_htrail_n]].t = -1;
}

// --- substitution application with canonical var renaming ------------
// Unbound slots map to fresh canonical vars V0, V1, ... in traversal
// order; the map is reset per release via the touched list.
static int g_can_map[HORN_NSLOTS];
static int g_can_touch[HORN_NSLOTS]; static int g_can_touch_n = 0, g_can_nv = 0;

static void horn_can_reset(void) {
  while (g_can_touch_n > 0) g_can_map[g_can_touch[--g_can_touch_n]] = -1;
  g_can_nv = 0;
}

static int horn_apply(int t, int off, int *budget) {
  if (--*budget < 0) return -1;
  horn_deref(&t, &off);
  if (g_hn[t].head < 0) {
    int slot = off + (-g_hn[t].head - 1);
    if (g_can_map[slot] < 0) {
      if (g_can_nv >= HORN_REL_VARS) return -1;
      g_can_map[slot] = g_can_nv++;
      g_can_touch[g_can_touch_n++] = slot;
    }
    return horn_node(-(g_can_map[slot] + 1), 0, NULL);
  }
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_apply(HARG(t, i), off, budget);
    if (args[i] < 0) return -1;
  }
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

// --- printing (canonical, offset-free terms) --------------------------
// Variables are renumbered by first occurrence in the printed order via
// remap (size HORN_REL_VARS, -1-initialized, shared across the sides of
// one equation) -- so an equation and its mirror print to comparable
// dedup keys.
static int horn_print(const TpProblem *tp, char *buf, size_t cap, size_t *o,
                      int t, int *remap, int *nv) {
  if (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1;
    if (v >= HORN_REL_VARS) return 0;
    if (remap[v] < 0) remap[v] = (*nv)++;
    if (*o + 8 >= cap) return 0;
    *o += (size_t)snprintf(buf + *o, cap - *o, "V%d", remap[v]);
    return 1;
  }
  const char *name = tp->syms[g_hn[t].head].name;
  size_t nl = strlen(name);
  if (*o + nl + 2 >= cap) return 0;
  memcpy(buf + *o, name, nl); *o += nl;
  if (g_hn[t].nargs > 0) {
    buf[(*o)++] = '(';
    for (int i = 0; i < g_hn[t].nargs; i++) {
      if (i > 0) { if (*o + 1 >= cap) return 0; buf[(*o)++] = ','; }
      if (!horn_print(tp, buf, cap, o, HARG(t, i), remap, nv)) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// --- dedup set over canonical "lhs=rhs" strings ------------------------
// Sized 2x the release cap (two keys per release: itself + mirror) so
// the open-addressing probe never wraps a full table.  The CD lane
// allocates 4x that: its wall-long runs push several million accepts,
// and a saturated table silently rejects every later candidate as a
// duplicate.
#define HORN_DEDUP_SLOTS (1 << 21)
static char **g_hdup = NULL;
static int g_hdup_n = 0;
static u32 g_hdup_slots = HORN_DEDUP_SLOTS;

static int horn_dedup_add(const char *key) {   // 1 = new
  if ((u32)g_hdup_n >= g_hdup_slots - (g_hdup_slots >> 2)) return 0;
  u64 h = tp_name_hash(key, strlen(key));
  u32 slot = (u32)(h % (u64)g_hdup_slots);
  for (;;) {
    if (g_hdup[slot] == NULL) break;
    if (strcmp(g_hdup[slot], key) == 0) return 0;
    slot = (slot + 1u) % g_hdup_slots;
  }
  g_hdup[slot] = strdup(key);
  g_hdup_n++;
  return 1;
}

// --- facts, rules, head index ------------------------------------------
typedef struct {
  int l, r, nv, size;
  u8 processed, sghit, rel, conj, given, dead;
} HFact;
static HFact *g_hf = NULL; static int g_hf_n = 0, g_hf_cap = 0;

static int horn_size(int t) {
  int s = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) s += horn_size(HARG(t, i));
  return s;
}

// --- condensed-detachment lane (--horn-cd) ----------------------------
// The LCL condensed-detachment family regresses the blind v1 chaining
// into promiscuous major-premise towers (weakening instances or(V, t)
// growing without bound) and an instance flood no exact dedup catches.
// The classic CD prover discipline (Otter on the LCL corpus) is:
//   - a WEIGHT CAP on released conclusions, raised by ITERATIVE
//     DEEPENING when the capped space saturates without the goal
//     (kills the towers);
//   - GENERALIZATION SUBSUMPTION: a released theorem(t) is redundant
//     if a kept theorem(s) matches onto it (forward), and a kept
//     instance is dead weight once a more general release arrives
//     (backward, done lazily at pop time here);
//   - pure smallest-first given selection.
// All of it is gated on g_horn_cd so every other flavor's trajectory
// stays byte-identical.
static int  g_horn_cd = 0;             // THVM_ATP_HORN_CD (2 = slice mode)
static double g_cli_wall_sec = 120.0;  // engine wall, for the CD lane budget
// Parsed negative units (the goals), for the subsumption rank check
// and the CD resonators.
static int g_hg_l[64], g_hg_r[64]; static int g_hg_n = 0;
static int  g_cd_cap = 0;              // live released-size cap
static int  g_cd_step = 4;             // THVM_ATP_HORN_CD_STEP
static int  g_cd_maxvars = 5;          // THVM_ATP_HORN_CD_VARS
static int  g_cd_overcap = 0;          // some conclusion exceeded the cap
static int  g_cd_rounds = 0;
static long g_cd_fsub = 0, g_cd_bsub = 0, g_cd_reso_hits = 0;

// One-way match: pattern variables live in the HORN_SG_OFF slot space
// and bind to canonical (offset-free) instance subterms; instance
// variables are rigid constants.  Bindings ride the shared trail (the
// caller undoes to its mark).  A bound pattern variable re-encountered
// recurses with po = 0, where this reduces to structural equality of
// two canonical instance terms -- instance slots are never bound.
static int horn_cd_match(int p, int po, int t, int depth) {
  if (depth > 400) return 0;
  if (g_hn[p].head < 0) {
    if (po == HORN_SG_OFF) {
      int slot = po + (-g_hn[p].head - 1);
      if (g_hb[slot].t >= 0)
        return horn_cd_match(g_hb[slot].t, g_hb[slot].off, t, depth + 1);
      g_hb[slot].t = t; g_hb[slot].off = 0;
      g_htrail[g_htrail_n++] = slot;
      return 1;
    }
    return g_hn[t].head == g_hn[p].head;   // instance var vs instance var
  }
  if (g_hn[t].head != g_hn[p].head || g_hn[t].nargs != g_hn[p].nargs) return 0;
  for (int i = 0; i < g_hn[p].nargs; i++)
    if (!horn_cd_match(HARG(p, i), po, HARG(t, i), depth + 1)) return 0;
  return 1;
}

// --- discrimination tree over pattern sides (generalization retrieval)
// The subsumers of a candidate are found by walking its l side in
// preorder against a tree of pattern sides: a pattern variable keys as
// STAR (-1) and consumes one whole candidate subterm on lookup, so the
// visit cost is bounded by the shared-prefix structure instead of the
// fact count (a flat scan spent 89% of the wall here on LCL195-10 --
// every CD fact shares the theorem(..) = true shape, so no root filter
// discriminates).  BOTH sides of every pattern fact are inserted
// (orient 0 = l, 1 = r): one query on the candidate's l covers both
// subsumption pairings, and the leaf match validates the full equation
// with variable consistency.  Fully ground facts are never inserted --
// after dedup they cannot strictly subsume anything.
typedef struct { int key, child, next; } DtEdge;
typedef struct { int edge0, leaf0; } DtNode;
typedef struct { int fact, orient, next; } DtLeaf;
static DtNode *g_dtn = NULL; static int g_dtn_n = 0, g_dtn_cap = 0;
static DtEdge *g_dte = NULL; static int g_dte_n = 0, g_dte_cap = 0;
static DtLeaf *g_dtl = NULL; static int g_dtl_n = 0, g_dtl_cap = 0;

static int dt_node_new(void) {
  if (g_dtn_n >= g_dtn_cap) {
    g_dtn_cap = (g_dtn_cap == 0) ? (1 << 12) : g_dtn_cap * 2;
    g_dtn = (DtNode *)realloc(g_dtn, (size_t)g_dtn_cap * sizeof(DtNode));
    if (g_dtn == NULL) tp_giveup("OOM in Horn release");
  }
  g_dtn[g_dtn_n].edge0 = -1;
  g_dtn[g_dtn_n].leaf0 = -1;
  return g_dtn_n++;
}

static int dt_child(int node, int key, int create) {
  for (int e = g_dtn[node].edge0; e >= 0; e = g_dte[e].next)
    if (g_dte[e].key == key) return g_dte[e].child;
  if (!create) return -1;
  int c = dt_node_new();
  if (g_dte_n >= g_dte_cap) {
    g_dte_cap = (g_dte_cap == 0) ? (1 << 12) : g_dte_cap * 2;
    g_dte = (DtEdge *)realloc(g_dte, (size_t)g_dte_cap * sizeof(DtEdge));
    if (g_dte == NULL) tp_giveup("OOM in Horn release");
  }
  g_dte[g_dte_n].key = key;
  g_dte[g_dte_n].child = c;
  g_dte[g_dte_n].next = g_dtn[node].edge0;
  g_dtn[node].edge0 = g_dte_n++;
  return c;
}

static void dt_insert(int t, int fact, int orient) {
  if (g_dtn_n == 0) dt_node_new();   // root
  int stack[512], sp = 0, node = 0;
  stack[sp++] = t;
  while (sp > 0) {
    int cur = stack[--sp];
    int key = (g_hn[cur].head < 0) ? -1 : g_hn[cur].head;
    node = dt_child(node, key, 1);
    if (key >= 0) {
      // A truncated walk would file the fact under a WRONG (shorter)
      // path and let it subsume non-instances: skip oversized patterns
      // entirely (they are also beyond the query flattener's bound).
      if (sp + g_hn[cur].nargs > 500) return;
      for (int i = g_hn[cur].nargs - 1; i >= 0; i--)
        stack[sp++] = HARG(cur, i);
    }
  }
  if (g_dtl_n >= g_dtl_cap) {
    g_dtl_cap = (g_dtl_cap == 0) ? (1 << 12) : g_dtl_cap * 2;
    g_dtl = (DtLeaf *)realloc(g_dtl, (size_t)g_dtl_cap * sizeof(DtLeaf));
    if (g_dtl == NULL) tp_giveup("OOM in Horn release");
  }
  g_dtl[g_dtl_n].fact = fact;
  g_dtl[g_dtl_n].orient = orient;
  g_dtl[g_dtl_n].next = g_dtn[node].leaf0;
  g_dtn[node].leaf0 = g_dtl_n++;
}

// Query state: the candidate's l side, flattened to preorder with
// per-position subterm extents (STAR skips).
static int g_dt_flat[512], g_dt_skipn[512], g_dt_flat_n = 0, g_dt_over = 0;
static void dt_flatten(int t) {
  int start = g_dt_flat_n;
  if (g_dt_flat_n >= 500) { g_dt_over = 1; return; }
  g_dt_flat[g_dt_flat_n++] = t;
  if (g_hn[t].head >= 0)
    for (int i = 0; i < g_hn[t].nargs && !g_dt_over; i++)
      dt_flatten(HARG(t, i));
  g_dt_skipn[start] = g_dt_flat_n - start;
}

// Walk the tree along the flattened candidate: exact-symbol edge, or a
// STAR edge that swallows the subterm.  Complete terms are prefix-free
// in preorder (fixed arities), so leaves are only ever checked when
// the candidate is fully consumed.  A leaf hit still runs the full
// two-sided match (variable consistency, the r side, orientation).
static int dt_probe(int node, int pos, int cl, int cr, int sz_max,
                    int min_idx) {
  if (pos >= g_dt_flat_n) {
    for (int L = g_dtn[node].leaf0; L >= 0; L = g_dtl[L].next) {
      int f = g_dtl[L].fact;
      if (g_hf[f].dead || g_hf[f].size > sz_max || f < min_idx) continue;
      int pl = g_dtl[L].orient ? g_hf[f].r : g_hf[f].l;
      int pr = g_dtl[L].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      int hit = horn_cd_match(pl, HORN_SG_OFF, cl, 0) &&
                horn_cd_match(pr, HORN_SG_OFF, cr, 0);
      horn_undo(mark);
      if (hit) return 1;
    }
    return 0;
  }
  int t = g_dt_flat[pos], c;
  if (g_hn[t].head >= 0 && (c = dt_child(node, g_hn[t].head, 0)) >= 0 &&
      dt_probe(c, pos + 1, cl, cr, sz_max, min_idx)) return 1;
  if ((c = dt_child(node, -1, 0)) >= 0 &&
      dt_probe(c, pos + g_dt_skipn[pos], cl, cr, sz_max, min_idx)) return 1;
  return 0;
}

// Forward subsumption: is the candidate (l, r) an instance of a live
// fact?  Every indexed fact predates the candidate, so no age filter.
static int horn_cd_fwd_subsumed(int l, int r, int sz) {
  if (g_dtn_n == 0) return 0;
  g_dt_flat_n = 0; g_dt_over = 0;
  dt_flatten(l);
  if (g_dt_over) return 0;
  return dt_probe(0, 0, l, r, sz, 0);
}

// --- resonance (Wos): goal-shape steering by skeleton MATCH ------------
// The backward cone on a CD goal regresses only into the promiscuous
// major-premise tower theorem(or(not(V0), .. G)), and probing releases
// by UNIFICATION lets every var-tailed weakening hit it (measured: 11k
// of 19k releases "sg-hit" on LCL195-10).  What is selective is the
// goal's SHAPE: each deep-enough goal subterm, with its leaves
// wildcarded to fresh pattern variables, becomes a one-way MATCH
// pattern.  A release that matches structurally refines the goal
// (Wos's resonators, generated from the goal instead of known proofs):
//   - the weakening tower or(V, ..) fails at the pattern's rigid
//     not(*) position;
//   - the deep PM intermediates (a hypothetical-syllogism instance is
//     itself goal-shaped) match, earning the cap exemption and the
//     priority pop that pure smallest-first would never grant them.
#define CD_RESO_MAX       32
#define CD_RESO_MIN_RIGID 5
static int g_cd_reso[CD_RESO_MAX]; static int g_cd_reso_n = 0;

static int horn_cd_skel_rigids(int t) {   // non-nullary heads
  if (g_hn[t].head < 0 || g_hn[t].nargs == 0) return 0;
  int n = 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    n += horn_cd_skel_rigids(HARG(t, i));
  return n;
}

// Copy t with every leaf (variable or nullary symbol) replaced by a
// fresh pattern variable; -1 when the variable space is exhausted.
static int horn_cd_skel(int t, int *nv) {
  if (g_hn[t].head < 0 || g_hn[t].nargs == 0) {
    if (*nv >= HORN_VSPACE) return -1;
    return horn_node(-(++*nv), 0, NULL);
  }
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_cd_skel(HARG(t, i), nv);
    if (args[i] < 0) return -1;
  }
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

static void horn_cd_reso_build(void) {
  g_cd_reso_n = 0;
  for (int g = 0; g < g_hg_n; g++) {
    int stack[512], sp = 0;
    stack[sp++] = g_hg_l[g]; stack[sp++] = g_hg_r[g];
    while (sp > 0 && g_cd_reso_n < CD_RESO_MAX) {
      int t = stack[--sp];
      if (g_hn[t].head >= 0)
        for (int i = 0; i < g_hn[t].nargs && sp < 500; i++)
          stack[sp++] = HARG(t, i);
      if (horn_cd_skel_rigids(t) < CD_RESO_MIN_RIGID) continue;
      int nv = 0;
      int p = horn_cd_skel(t, &nv);
      if (p < 0) continue;
      int dup = 0;
      for (int i = 0; i < g_cd_reso_n && !dup; i++)
        dup = horn_eq(g_cd_reso[i], p);
      if (!dup) g_cd_reso[g_cd_reso_n++] = p;
    }
  }
}

// Does either side of the release structurally refine a goal skeleton?
// (Strict one-way match: pattern wildcards are only the skeleton's own
// leaf variables.  A linked-instance-variable tolerance was tried and
// measured to flood -- 395k of 400k releases "resonant" on LCL195-10,
// whose 1.6-family goal shape saturates the whole detachment space.)
static int horn_cd_resonant(int l, int r) {
  int hit = 0;
  for (int i = 0; i < g_cd_reso_n && !hit; i++) {
    for (int side = 0; side < 2 && !hit; side++) {
      int t = side ? r : l;
      int mark = g_htrail_n;
      hit = horn_cd_match(g_cd_reso[i], HORN_SG_OFF, t, 0);
      horn_undo(mark);
    }
  }
  return hit;
}

// Lazy back subsumption at pop time: only facts added AFTER f can
// subsume it (older generals rejected f at its own emit).
static int horn_cd_pop_subsumed(int f) {
  if (g_dtn_n == 0) return 0;
  g_dt_flat_n = 0; g_dt_over = 0;
  dt_flatten(g_hf[f].l);
  if (g_dt_over) return 0;
  return dt_probe(0, 0, g_hf[f].l, g_hf[f].r, g_hf[f].size, f + 1);
}

// Min-heaps over unprocessed fact indices, keyed by term size
// (smallest first, ties by insertion order).  g_hheap is the main
// given-fact agenda; g_hpheap is the goal-directed priority agenda
// (facts that unify with a backward-cone subgoal), popped first.
typedef struct { int *a; int n, cap; } HHeap;
static HHeap g_hheap  = {NULL, 0, 0};
static HHeap g_hpheap = {NULL, 0, 0};
static int horn_heap_lt(int a, int b) {
  if (g_hf[a].size != g_hf[b].size) return g_hf[a].size < g_hf[b].size;
  return a < b;
}
static void horn_heap_push(HHeap *h, int f) {
  if (h->n >= h->cap) {
    h->cap = (h->cap == 0) ? (1 << 12) : h->cap * 2;
    h->a = (int *)realloc(h->a, (size_t)h->cap * sizeof(int));
    if (h->a == NULL) tp_giveup("OOM in Horn release");
  }
  int i = h->n++;
  h->a[i] = f;
  while (i > 0) {
    int p = (i - 1) / 2;
    if (!horn_heap_lt(h->a[i], h->a[p])) break;
    int t = h->a[i]; h->a[i] = h->a[p]; h->a[p] = t;
    i = p;
  }
}
static int horn_heap_pop(HHeap *h) {
  if (h->n == 0) return -1;
  int top = h->a[0];
  h->a[0] = h->a[--h->n];
  int i = 0;
  for (;;) {
    int l = 2 * i + 1, r = l + 1, m = i;
    if (l < h->n && horn_heap_lt(h->a[l], h->a[m])) m = l;
    if (r < h->n && horn_heap_lt(h->a[r], h->a[m])) m = r;
    if (m == i) break;
    int t = h->a[i]; h->a[i] = h->a[m]; h->a[m] = t;
    i = m;
  }
  return top;
}

// Head index: per symbol, a chain of (fact, orient) entries; orient 0
// matches conditions against (l, r), orient 1 against (r, l).  A fact
// side headed by a variable goes on the var-head chain (matches any
// condition head).
typedef struct { int fact, orient, next; } HIdxE;
static HIdxE *g_hix = NULL; static int g_hix_n = 0, g_hix_cap = 0;
static int *g_hix_head = NULL;   // per symbol id
static int g_hix_varhead = -1;

static void horn_index_entry(int head, int fact, int orient) {
  if (g_hix_n >= g_hix_cap) {
    g_hix_cap = (g_hix_cap == 0) ? (1 << 14) : g_hix_cap * 2;
    g_hix = (HIdxE *)realloc(g_hix, (size_t)g_hix_cap * sizeof(HIdxE));
    if (g_hix == NULL) tp_giveup("OOM in Horn release");
  }
  HIdxE *e = &g_hix[g_hix_n];
  e->fact = fact; e->orient = orient;
  if (head < 0) { e->next = g_hix_varhead; g_hix_varhead = g_hix_n; }
  else          { e->next = g_hix_head[head]; g_hix_head[head] = g_hix_n; }
  g_hix_n++;
}

static void horn_fact_add(int l, int r, int nv) {
  if (g_hf_n >= g_hf_cap) {
    g_hf_cap = (g_hf_cap == 0) ? (1 << 12) : g_hf_cap * 2;
    g_hf = (HFact *)realloc(g_hf, (size_t)g_hf_cap * sizeof(HFact));
    if (g_hf == NULL) tp_giveup("OOM in Horn release");
  }
  // CD lane: index the new fact as a subsumption pattern (both sides).
  if (g_horn_cd && nv > 0) {
    dt_insert(l, g_hf_n, 0);
    dt_insert(r, g_hf_n, 1);
  }
  g_hf[g_hf_n].l = l; g_hf[g_hf_n].r = r; g_hf[g_hf_n].nv = nv;
  g_hf[g_hf_n].size = horn_size(l) + horn_size(r);
  g_hf[g_hf_n].processed = 0;
  g_hf[g_hf_n].sghit = 0;
  g_hf[g_hf_n].rel = 1;
  g_hf[g_hf_n].conj = 0;
  g_hf[g_hf_n].given = 0;
  g_hf[g_hf_n].dead = 0;
  horn_index_entry(g_hn[l].head, g_hf_n, 0);
  horn_index_entry(g_hn[r].head, g_hf_n, 1);
  horn_heap_push(&g_hheap, g_hf_n);
  g_hf_n++;
}

typedef struct {
  int cl[HORN_MAX_COND], ct[HORN_MAX_COND];
  int nc, concl_l, concl_r, nv;
  u8 rel, conj;
} HRule;
static HRule *g_hr = NULL; static int g_hr_n = 0, g_hr_cap = 0;

// Rule-condition index: (rule, condition position) entries bucketed by
// the condition's head symbol, so a given fact only visits the rules
// that can bind it (the CSR knowledge bases carry ~5k rules; scanning
// them all per given burns the whole budget before the goal chain).
typedef struct { int rule, cpos, next; } HRIdxE;
static HRIdxE *g_hrix = NULL; static int g_hrix_n = 0, g_hrix_cap = 0;
static int *g_hrix_head = NULL;   // per symbol id
static int g_hrix_varhead = -1;

static void horn_rule_index(int rule, int cpos, int head) {
  if (g_hrix_n >= g_hrix_cap) {
    g_hrix_cap = (g_hrix_cap == 0) ? (1 << 12) : g_hrix_cap * 2;
    g_hrix = (HRIdxE *)realloc(g_hrix, (size_t)g_hrix_cap * sizeof(HRIdxE));
    if (g_hrix == NULL) tp_giveup("OOM in Horn release");
  }
  HRIdxE *e = &g_hrix[g_hrix_n];
  e->rule = rule; e->cpos = cpos;
  if (head < 0) { e->next = g_hrix_varhead; g_hrix_varhead = g_hrix_n; }
  else          { e->next = g_hrix_head[head]; g_hrix_head[head] = g_hrix_n; }
  g_hrix_n++;
}

// Pending releases (kept as strings for ranking + TpClause append;
// tl/tr keep the arena terms for the goal-subsumption rank check;
// n_weak counts single-occurrence variables -- weakening instances
// like or(V, lemma) rank below their workhorse cores).
typedef struct {
  char *l, *r;
  int tl, tr;
  int goal_hit, sg_hit, goal_syms, ground, n_weak, size, order;
  int grp, w;   // head-symbol group + within-group rank (diversity quota)
  int dead;     // back-subsumed (CD lane): never appended to the engine
} HRel;
static HRel *g_hrel = NULL; static int g_hrel_n = 0, g_hrel_cap = 0;

static u8 *g_horn_disp = NULL;   // per symbol id: 1 = dispatcher functor

// --- definitional expansion --------------------------------------------
// A unit fact f(X1..Xk) = r (k >= 1, pairwise-distinct variable
// arguments, f absent from r, every variable of r among the arguments)
// DEFINES f.  The chaining universe is normalized by expanding every
// defined symbol, so unification sees a single alphabet -- e.g. the
// LCL encodings state detachment over implies(..) but their axioms
// interleave it with or/not through implies(X,Y) = or(not(X),Y).
// Expansion preserves equational consequence, so releases stay sound.
static int *g_hdef_l = NULL, *g_hdef_r = NULL;   // per symbol: node or -1
static int g_hexp_fail = 0, g_hexp_limit = 0;

static int horn_sym_occurs(int f, int t) {
  if (g_hn[t].head == f) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_sym_occurs(f, HARG(t, i))) return 1;
  return 0;
}

static int horn_vars_within(int t, const u8 *allowed) {
  if (g_hn[t].head < 0) return allowed[-g_hn[t].head - 1];
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (!horn_vars_within(HARG(t, i), allowed)) return 0;
  return 1;
}

static void horn_def_note(int l, int r) {
  if (g_hn[l].head < 0 || g_hn[l].nargs < 1) return;
  int f = g_hn[l].head;
  if (g_horn_disp[f] || g_hdef_r[f] >= 0) return;
  u8 seen[HORN_VSPACE];
  memset(seen, 0, sizeof seen);
  for (int i = 0; i < g_hn[l].nargs; i++) {
    int a = HARG(l, i);
    if (g_hn[a].head >= 0) return;
    int v = -g_hn[a].head - 1;
    if (seen[v]) return;
    seen[v] = 1;
  }
  if (horn_sym_occurs(f, r)) return;
  if (!horn_vars_within(r, seen)) return;
  g_hdef_l[f] = l; g_hdef_r[f] = r;
}

static int horn_subst_build(int t, const int *m, int depth) {
  if (depth > 200 || g_hexp_fail || g_hn_n > g_hexp_limit) { g_hexp_fail = 1; return t; }
  if (g_hn[t].head < 0) return m[-g_hn[t].head - 1];
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++)
    args[i] = horn_subst_build(HARG(t, i), m, depth + 1);
  if (g_hexp_fail) return t;
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

static int horn_expand(int t, int depth) {
  if (depth > 64 || g_hexp_fail || g_hn_n > g_hexp_limit) { g_hexp_fail = 1; return t; }
  if (g_hn[t].head < 0) return t;
  int args[16], changed = 0;
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_expand(HARG(t, i), depth + 1);
    changed |= (args[i] != HARG(t, i));
  }
  if (g_hexp_fail) return t;
  int f = g_hn[t].head;
  if (g_hdef_r != NULL && g_hdef_r[f] >= 0) {
    int m[HORN_VSPACE];
    int L = g_hdef_l[f];
    for (int i = 0; i < g_hn[L].nargs; i++)
      m[-g_hn[HARG(L, i)].head - 1] = args[i];
    int e = horn_subst_build(g_hdef_r[f], m, depth + 1);
    if (g_hexp_fail) return t;
    return horn_expand(e, depth + 1);   // chained definitions
  }
  if (!changed) return t;
  return horn_node(f, g_hn[t].nargs, args);
}

// Normalize with fallback: an expansion failure (cyclic definitions,
// blow-up) rolls the arena back and keeps the original term -- either
// form is sound.
static int horn_norm(int t) {
  if (g_hdef_r == NULL) return t;
  g_hexp_fail = 0;
  int mark_n = g_hn_n, mark_a = g_ha_n;
  g_hexp_limit = g_hn_n + 4000;
  int e = horn_expand(t, 0);
  if (g_hexp_fail) { g_hn_n = mark_n; g_ha_n = mark_a; return t; }
  return e;
}

// --- dispatcher / conditional-clause shape checks -----------------------
// ifeqN(X,X,Y[,Z]) = Y with every argument a variable and the other
// side the bare variable Y (either clause orientation).
static int horn_disp_check(int s, int other) {
  if (g_hn[s].head < 0 || g_hn[other].head >= 0) return 0;
  int na = g_hn[s].nargs;
  if (na != 3 && na != 4) return 0;
  for (int i = 0; i < na; i++)
    if (g_hn[HARG(s, i)].head >= 0) return 0;
  int a0 = g_hn[HARG(s, 0)].head, a1 = g_hn[HARG(s, 1)].head;
  int a2 = g_hn[HARG(s, 2)].head, ov = g_hn[other].head;
  if (a0 != a1 || a2 != ov || a2 == a0) return 0;
  return 1;
}

// Push condition (cl, ct), splitting PAIRED conditions first: the CSR
// consistency KBs pack conjunctions as tuple2(p,q) = tuple2(true,true),
// which no unit fact can ever unify.  When both sides carry the same
// rigid head and arity, the argument equations are pushed instead --
// they IMPLY the packed equation by congruence (then the dispatcher
// fires), so the decoded rule stays a sound consequence.  Returns 0 on
// condition-count overflow.
static int horn_cond_push(HRule *ru, int *nc, int cl, int ct, int depth) {
  if (depth < 8 && g_hn[cl].head >= 0 && g_hn[cl].head == g_hn[ct].head &&
      g_hn[cl].nargs == g_hn[ct].nargs && g_hn[cl].nargs > 0) {
    for (int i = 0; i < g_hn[cl].nargs; i++)
      if (!horn_cond_push(ru, nc, HARG(cl, i), HARG(ct, i), depth + 1))
        return 0;
    return 1;
  }
  if (*nc >= HORN_MAX_COND) return 0;
  ru->cl[*nc] = cl;
  ru->ct[*nc] = ct;
  (*nc)++;
  return 1;
}

// Unwrap ifeqN(c, t, then, e) = e (else side must equal the clause's
// other side for arity 4; arity 3 has no else).  Nesting in the
// then-position accumulates conditions; a partial unwrap is still a
// sound rule (the residual term is the conclusion).
static int horn_decode_side(int s, int other, HRule *ru) {
  int nc = 0, cur = s;
  for (;;) {
    if (g_hn[cur].head < 0 || !g_horn_disp[g_hn[cur].head]) break;
    int na = g_hn[cur].nargs;
    if (na != 3 && na != 4) break;
    if (na == 4 && !horn_eq(HARG(cur, 3), other)) break;
    if (!horn_cond_push(ru, &nc, HARG(cur, 0), HARG(cur, 1), 0)) return 0;
    cur = HARG(cur, 2);
  }
  if (nc == 0) return 0;
  ru->nc = nc; ru->concl_l = cur; ru->concl_r = other;
  return 1;
}

// --- the forward-chaining join ------------------------------------------
static int g_horn_stop = 0;
static double g_horn_t0 = 0.0;
static double g_horn_wall = HORN_WALL_SEC;      // THVM_ATP_HORN_WALL
static int g_horn_max_rel = HORN_MAX_RELEASES;  // THVM_ATP_HORN_MAX
// Attempt budget: the CD lane runs wall-governed (its subsumed space
// stays small), every other flavor keeps the fixed v1 cap.  Same for
// the arena-node ceiling: a wall-long CD run holds millions of
// accepted facts live.
static long g_horn_max_attempts = HORN_MAX_ATTEMPTS;
static int  g_horn_max_nodes = (1 << 22);
static double horn_now(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

// --- goal-directed backward subgoal cone (v2) ---------------------------
// SLD/UR-style regression from the negated goals through the decoded
// rules, bounded by depth, count and a wall slice.  A subgoal is an
// equation PATTERN: any released fact that unifies with one is a
// candidate step of a goal-closing chain, so it is (a) popped from the
// agenda with priority -- its forward joins produce the parent
// subgoal's instance next -- and (b) ranked into the kept window.
// Subgoals are relevance markers only, never released as equations, so
// soundness is untouched.
//
// Regression steps, for a subgoal G and rule C1..Ck -> L=R with the
// conclusion unified against G (either orientation):
//   - abstract:  each condition Ci becomes a subgoal under the
//     conclusion bindings alone (SLD with the siblings postponed);
//     rejected unless enough rigid structure survives to discriminate.
//   - UR-style:  all conditions but one are satisfied against the
//     CURRENT fact set (input units; the cone runs before the loop),
//     and the open condition becomes the subgoal -- the classic
//     unit-resulting negative-unit step, fully constrained.
typedef struct { int l, r, depth, hl, hr, hits; } HGoalPat;
static HGoalPat *g_hsg = NULL; static int g_hsg_n = 0, g_hsg_cap = 0;
static int g_hsg_depth_cap = 6;             // THVM_ATP_HORN_BWD_DEPTH
static int g_hsg_max = 512;                 // THVM_ATP_HORN_BWD_MAX
static int g_hsg_stop = 0;
static double g_hsg_deadline = 0.0;
static long g_hsg_checks = 0;               // release-vs-subgoal unify budget
#define HORN_SG_BUDGET 20000000L
#define HORN_SG_MIN_RIGID 3
// Steering is AUTO-GATED (g_horn_steer): priority pops run only when
// the goal is existential (a reachability/planning query -- PUZ
// states, PLA plans -- where only regression can carve a path through
// an astronomical forward space) or the relevance gate is active (CSR
// KBs).  For ground saturation-style goals (the LCL condensed-
// detachment family) the baseline agenda already reaches goals on a
// razor-thin trajectory, and ANY dilution of its pop order loses them
// (LCL192/LCL211 measured); there the cone is skipped entirely and
// only the direct goal probe runs.  THVM_ATP_HORN_BWD: 0=off, 2=force
// steering, default auto.
static int g_horn_steer = 0;
static int g_horn_goal_stop = 1;            // THVM_ATP_HORN_GOAL_STOP
static int g_horn_goal_reached = 0;

static int horn_rigid_count(int t) {
  int n = (g_hn[t].head >= 0);
  for (int i = 0; i < g_hn[t].nargs; i++) n += horn_rigid_count(HARG(t, i));
  return n;
}

static int horn_has_var(int t) {
  if (g_hn[t].head < 0) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_has_var(HARG(t, i))) return 1;
  return 0;
}

// Canonicalize (cl, ct) under the live bindings into a new subgoal;
// dedup ("?"-prefixed keys share the release dedup set), reject
// under-constrained patterns (too little rigid structure to steer).
static void horn_bwd_emit(const TpProblem *tp, int cl, int ct, int depth) {
  if (g_hsg_n >= g_hsg_max) { g_hsg_stop = 1; return; }
  int hn_mark = g_hn_n, ha_mark = g_ha_n;
  horn_can_reset();
  int budget = HORN_APPLY_MAX;
  int l2 = horn_apply(cl, 0, &budget);
  int r2 = (l2 >= 0) ? horn_apply(ct, 0, &budget) : -1;
  if (l2 < 0 || r2 < 0) goto reject;
  // Specificity guard: a pattern needs enough rigid structure to
  // discriminate -- except a GROUND pattern, which is maximally
  // specific however small (a CSR goal is a bare `a = b`).
  if (g_can_nv > 0 &&
      horn_rigid_count(l2) + horn_rigid_count(r2) < HORN_SG_MIN_RIGID)
    goto reject;
  {
    static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 3];
    size_t lo = 0, ro = 0;
    int remap[HORN_REL_VARS], nv = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
    if (!horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv)) goto reject;
    if (!horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv)) goto reject;
    snprintf(key, sizeof key, "?%s=%s", lb, rb);
    if (!horn_dedup_add(key)) goto reject;
    size_t fo = 0, go = 0;
    int remap2[HORN_REL_VARS], nv2 = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap2[i] = -1;
    if (horn_print(tp, rb, HORN_SIDE_MAX, &fo, r2, remap2, &nv2) &&
        horn_print(tp, lb, HORN_SIDE_MAX, &go, l2, remap2, &nv2)) {
      snprintf(key, sizeof key, "?%s=%s", rb, lb);
      horn_dedup_add(key);
    }
  }
  if (g_hsg_n >= g_hsg_cap) {
    g_hsg_cap = (g_hsg_cap == 0) ? (1 << 9) : g_hsg_cap * 2;
    g_hsg = (HGoalPat *)realloc(g_hsg, (size_t)g_hsg_cap * sizeof(HGoalPat));
    if (g_hsg == NULL) tp_giveup("OOM in Horn release");
  }
  g_hsg[g_hsg_n].l = l2; g_hsg[g_hsg_n].r = r2;
  g_hsg[g_hsg_n].depth = depth;
  g_hsg[g_hsg_n].hl = g_hn[l2].head; g_hsg[g_hsg_n].hr = g_hn[r2].head;
  g_hsg[g_hsg_n].hits = 0;
  g_hsg_n++;
  {
    static int diag = -1;
    if (diag < 0) { const char *d = getenv("THVM_ATP_HORN_DIAG"); diag = (d != NULL && d[0] != '0' && d[0] != '\0'); }
    if (diag && g_hsg_n <= 64) {
      static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
      size_t lo = 0, ro = 0;
      int remap[HORN_REL_VARS], nv = 0;
      for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
      if (horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv) &&
          horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv))
        fprintf(stderr, "[horn-sg %d d%d] %s = %s\n", g_hsg_n - 1, depth, lb, rb);
    }
  }
  return;
reject:
  g_hn_n = hn_mark; g_ha_n = ha_mark;
}

// UR fill: satisfy every condition except `open` from the current fact
// set (no processed filter -- the cone runs before the loop), then emit
// the open condition as a subgoal under the accumulated bindings.
static void horn_bwd_fill(const TpProblem *tp, const HRule *ru, int ci,
                          int open, int depth) {
  if (g_hsg_stop) return;
  while (ci == open) ci++;
  if (ci >= ru->nc) { horn_bwd_emit(tp, ru->cl[open], ru->ct[open], depth); return; }
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > HORN_MAX_ATTEMPTS || horn_now() > g_hsg_deadline ||
       g_hn_n > (1 << 22))) { g_hsg_stop = 1; return; }
  int c = ru->cl[ci], coff = 0;
  horn_deref(&c, &coff);
  int off = (ci + 1) * HORN_VSPACE;
  for (int pass = 0; pass < 2 && !g_hsg_stop; pass++) {
    int e;
    if (pass == 0) e = (g_hn[c].head >= 0) ? g_hix_head[g_hn[c].head] : -2;
    else           e = g_hix_varhead;
    if (e == -2) {
      for (int f = 0; f < g_hf_n && !g_hsg_stop; f++) {
        if (!g_hf[f].rel) continue;   // cone fills stay in the relevant cone
        for (int orient = 0; orient < 2; orient++) {
          int fl = orient ? g_hf[f].r : g_hf[f].l;
          int fr = orient ? g_hf[f].l : g_hf[f].r;
          int mark = g_htrail_n;
          g_horn_attempts++;
          if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
              horn_unify(ru->ct[ci], 0, fr, off, 0))
            horn_bwd_fill(tp, ru, ci + 1, open, depth);
          horn_undo(mark);
        }
      }
      continue;
    }
    for (; e >= 0 && !g_hsg_stop; e = g_hix[e].next) {
      int f = g_hix[e].fact;
      if (!g_hf[f].rel) continue;     // cone fills stay in the relevant cone
      int fl = g_hix[e].orient ? g_hf[f].r : g_hf[f].l;
      int fr = g_hix[e].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      g_horn_attempts++;
      if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
          horn_unify(ru->ct[ci], 0, fr, off, 0))
        horn_bwd_fill(tp, ru, ci + 1, open, depth);
      horn_undo(mark);
    }
  }
}

// Breadth-first cone: seed from the parsed negative units, regress
// through every rule whose conclusion unifies (either orientation).
static void horn_backward_cone(const TpProblem *tp) {
  for (int g = 0; g < g_hg_n && !g_hsg_stop; g++)
    horn_bwd_emit(tp, g_hg_l[g], g_hg_r[g], 0);
  for (int qi = 0; qi < g_hsg_n && !g_hsg_stop; qi++) {
    if (g_hsg[qi].depth >= g_hsg_depth_cap) continue;
    int sgl = g_hsg[qi].l, sgr = g_hsg[qi].r, d1 = g_hsg[qi].depth + 1;
    for (int r = 0; r < g_hr_n && !g_hsg_stop; r++) {
      const HRule *ru = &g_hr[r];
      if (!ru->rel) continue;   // relevance-gated (oversized KBs)
      int chl = g_hn[ru->concl_l].head, chr = g_hn[ru->concl_r].head;
      for (int orient = 0; orient < 2 && !g_hsg_stop; orient++) {
        int gl = orient ? sgr : sgl, gr = orient ? sgl : sgr;
        if (chl >= 0 && g_hn[gl].head >= 0 && chl != g_hn[gl].head) continue;
        if (chr >= 0 && g_hn[gr].head >= 0 && chr != g_hn[gr].head) continue;
        int mark = g_htrail_n;
        g_horn_attempts++;
        if (horn_unify(ru->concl_l, 0, gl, HORN_SG_OFF, 0) &&
            horn_unify(ru->concl_r, 0, gr, HORN_SG_OFF, 0)) {
          for (int j = 0; j < ru->nc && !g_hsg_stop; j++) {
            horn_bwd_emit(tp, ru->cl[j], ru->ct[j], d1);
            if (ru->nc >= 2) horn_bwd_fill(tp, ru, 0, j, d1);
          }
        }
        horn_undo(mark);
        if ((g_horn_attempts & 0xfff) == 0 && horn_now() > g_hsg_deadline)
          g_hsg_stop = 1;
      }
    }
  }
}

// Does the fact (l, r) unify with some subgoal?  l/r live in a var
// space of their own (canonical release vars, or an input fact's
// clause-local vars): probe space vs subgoal space, both clean even
// while a join's bindings are live in spaces 0..HORN_MAX_COND.
static int horn_sg_probe(int l, int r) {
  if (g_hsg_n == 0 || g_hsg_checks > HORN_SG_BUDGET) return 0;
  if (g_hpheap.n > 4096) return 0;    // steering saturated: stop paying
  int hl = g_hn[l].head, hr = g_hn[r].head;
  for (int s = 0; s < g_hsg_n; s++) {
    for (int orient = 0; orient < 2; orient++) {
      int a = orient ? r : l, b = orient ? l : r;
      int ha = orient ? hr : hl, hb = orient ? hl : hr;
      if (ha >= 0 && g_hsg[s].hl >= 0 && ha != g_hsg[s].hl) continue;
      if (hb >= 0 && g_hsg[s].hr >= 0 && hb != g_hsg[s].hr) continue;
      int mark = g_htrail_n;
      g_hsg_checks++;
      int hit = horn_unify(a, HORN_PROBE_OFF, g_hsg[s].l, HORN_SG_OFF, 0) &&
                horn_unify(b, HORN_PROBE_OFF, g_hsg[s].r, HORN_SG_OFF, 0);
      horn_undo(mark);
      if (hit) { g_hsg[s].hits++; return 1; }   // hits: diagnostics only
    }
  }
  return 0;
}

// --- SInE-lite relevance gate for oversized KBs (v2) --------------------
// The CSR knowledge bases carry 20-40k clauses; a blind given-fact loop
// burns the whole attempt budget on ontology noise and never touches
// the conjecture's neighborhood.  Compute Hoder-Voronkov relevance over
// the DECODED universe (facts + rules), seeded from the conjecture
// clauses: an unreached fact is pre-marked processed -- it stays a
// passive join partner but is never popped as given -- and an
// unreached rule neither joins givens nor hosts cone regressions.
// Purely a search-order device: no clause is dropped, releases stay
// sound consequences.
static u32 horn_syms_count(int t, u32 *gen, u32 *stamp, u32 tag) {
  u32 n = 0;
  if (g_hn[t].head >= 0) {
    if (stamp[g_hn[t].head] != tag) { stamp[g_hn[t].head] = tag; gen[g_hn[t].head]++; n++; }
    for (int i = 0; i < g_hn[t].nargs; i++)
      n += horn_syms_count(HARG(t, i), gen, stamp, tag);
  }
  return n;
}
static u32 horn_syms_min(int t, const u32 *gen) {
  u32 m = 0xffffffffu;
  if (g_hn[t].head >= 0) {
    m = gen[g_hn[t].head];
    for (int i = 0; i < g_hn[t].nargs; i++) {
      u32 c = horn_syms_min(HARG(t, i), gen);
      if (c < m) m = c;
    }
  }
  return m;
}
static int horn_syms_trig(int t, const u32 *gen, const u8 *reach, double lim) {
  if (g_hn[t].head < 0) return 0;
  if (reach[g_hn[t].head] && (double)gen[g_hn[t].head] <= lim) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_syms_trig(HARG(t, i), gen, reach, lim)) return 1;
  return 0;
}
static void horn_syms_reach(int t, u8 *reach) {
  if (g_hn[t].head < 0) return;
  reach[g_hn[t].head] = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) horn_syms_reach(HARG(t, i), reach);
}

static double g_horn_gate_tol = 1.5;
static int    g_horn_gate_on  = 0;
static int    g_horn_input_nf = 0;   // facts below this index are input units

// Recomputable: a WIDER tol yields a superset of relevant clauses, so
// the fixpoint loop can relax the gate iteratively.  Facts that become
// relevant after being parked as passive re-enter the agenda.
static void horn_relevance_gate(double tol) {
  u32 *gen   = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
  u32 *stamp = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
  u8  *reach = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
  if (gen == NULL || stamp == NULL || reach == NULL)
    tp_giveup("OOM in Horn release");
  u32 tag = 0;
  for (int f = 0; f < g_hf_n; f++) {
    tag++;
    horn_syms_count(g_hf[f].l, gen, stamp, tag);
    horn_syms_count(g_hf[f].r, gen, stamp, tag);
    // Releases (f >= the input-fact watermark) descend from relevant
    // chains: always active.
    g_hf[f].rel = (f >= g_horn_input_nf) ? 1 : g_hf[f].conj;
  }
  for (int r = 0; r < g_hr_n; r++) {
    tag++;
    for (int j = 0; j < g_hr[r].nc; j++) {
      horn_syms_count(g_hr[r].cl[j], gen, stamp, tag);
      horn_syms_count(g_hr[r].ct[j], gen, stamp, tag);
    }
    horn_syms_count(g_hr[r].concl_l, gen, stamp, tag);
    horn_syms_count(g_hr[r].concl_r, gen, stamp, tag);
    g_hr[r].rel = g_hr[r].conj;
  }
  for (int g = 0; g < g_hg_n; g++) {
    horn_syms_reach(g_hg_l[g], reach);
    horn_syms_reach(g_hg_r[g], reach);
  }
  for (int f = 0; f < g_hf_n; f++)
    if (g_hf[f].conj) { horn_syms_reach(g_hf[f].l, reach); horn_syms_reach(g_hf[f].r, reach); }
  for (int r = 0; r < g_hr_n; r++)
    if (g_hr[r].conj) {
      for (int j = 0; j < g_hr[r].nc; j++) {
        horn_syms_reach(g_hr[r].cl[j], reach);
        horn_syms_reach(g_hr[r].ct[j], reach);
      }
      horn_syms_reach(g_hr[r].concl_l, reach);
      horn_syms_reach(g_hr[r].concl_r, reach);
    }
  for (int rd = 0; rd < 24; rd++) {
    int grew = 0;
    for (int f = 0; f < g_hf_n; f++) {
      if (g_hf[f].rel) continue;
      u32 m = horn_syms_min(g_hf[f].l, gen);
      u32 m2 = horn_syms_min(g_hf[f].r, gen);
      if (m2 < m) m = m2;
      double lim = tol * (double)m;
      if (horn_syms_trig(g_hf[f].l, gen, reach, lim) ||
          horn_syms_trig(g_hf[f].r, gen, reach, lim)) {
        g_hf[f].rel = 1; grew = 1;
        horn_syms_reach(g_hf[f].l, reach);
        horn_syms_reach(g_hf[f].r, reach);
      }
    }
    for (int r = 0; r < g_hr_n; r++) {
      if (g_hr[r].rel) continue;
      u32 m = horn_syms_min(g_hr[r].concl_l, gen);
      u32 m2 = horn_syms_min(g_hr[r].concl_r, gen);
      if (m2 < m) m = m2;
      for (int j = 0; j < g_hr[r].nc; j++) {
        u32 c = horn_syms_min(g_hr[r].cl[j], gen);  if (c < m) m = c;
        u32 d = horn_syms_min(g_hr[r].ct[j], gen);  if (d < m) m = d;
      }
      double lim = tol * (double)m;
      int hit = horn_syms_trig(g_hr[r].concl_l, gen, reach, lim) ||
                horn_syms_trig(g_hr[r].concl_r, gen, reach, lim);
      for (int j = 0; j < g_hr[r].nc && !hit; j++)
        hit = horn_syms_trig(g_hr[r].cl[j], gen, reach, lim) ||
              horn_syms_trig(g_hr[r].ct[j], gen, reach, lim);
      if (hit) {
        g_hr[r].rel = 1; grew = 1;
        for (int j = 0; j < g_hr[r].nc; j++) {
          horn_syms_reach(g_hr[r].cl[j], reach);
          horn_syms_reach(g_hr[r].ct[j], reach);
        }
        horn_syms_reach(g_hr[r].concl_l, reach);
        horn_syms_reach(g_hr[r].concl_r, reach);
      }
    }
    if (!grew) break;
  }
  int nf = 0, nr = 0;
  for (int f = 0; f < g_hf_n; f++) {
    if (g_hf[f].rel) {
      nf++;
      if (g_hf[f].processed && !g_hf[f].given) {
        // Parked passive by a tighter gate: back onto the agenda.
        g_hf[f].processed = 0;
        horn_heap_push(&g_hheap, f);
        if (g_hf[f].sghit) horn_heap_push(&g_hpheap, f);
      }
    } else if (!g_hf[f].given) {
      g_hf[f].processed = 1;      // passive: join partner, never given
    }
  }
  for (int r = 0; r < g_hr_n; r++) nr += g_hr[r].rel;
  fprintf(stderr, "FindProof: Horn relevance gate: %d/%d fact(s), "
          "%d/%d rule(s) active (tol=%.2f)\n", nf, g_hf_n, nr, g_hr_n, tol);
  free(gen); free(stamp); free(reach);
}

// Does the fact (l, r) unify with an ORIGINAL goal?  Any common
// instance refutes that negative unit, so the chaining can stop: the
// goal-subsuming release is kept first and the engine closes at once.
static int horn_goal_probe(int l, int r) {
  for (int g = 0; g < g_hg_n; g++) {
    for (int orient = 0; orient < 2; orient++) {
      int a = orient ? r : l, b = orient ? l : r;
      int mark = g_htrail_n;
      int hit = horn_unify(a, HORN_PROBE_OFF, g_hg_l[g], HORN_SG_OFF, 0) &&
                horn_unify(b, HORN_PROBE_OFF, g_hg_r[g], HORN_SG_OFF, 0);
      horn_undo(mark);
      if (hit) return 1;
    }
  }
  return 0;
}

static void horn_emit_release(TpProblem *tp, const HRule *ru) {
  // Rejected releases (dedup hits, guards) roll their applied terms
  // back out of the arena -- only accepted facts persist.
  int hn_mark = g_hn_n, ha_mark = g_ha_n;
  horn_can_reset();
  int budget = HORN_APPLY_MAX;
  int l2 = horn_apply(ru->concl_l, 0, &budget);
  int r2 = (l2 >= 0) ? horn_apply(ru->concl_r, 0, &budget) : -1;
  if (l2 < 0 || r2 < 0) goto reject;
  if (horn_eq(l2, r2)) goto reject;                  // trivial
  // Collapse guards: a bare-variable side whose variable does not
  // occur in the other side releases a near-degenerate equation.
  if (g_hn[l2].head < 0 || g_hn[r2].head < 0) {
    int v = (g_hn[l2].head < 0) ? l2 : r2, w = (g_hn[l2].head < 0) ? r2 : l2;
    if (g_hn[w].head < 0) goto reject;               // X = Y
    int slot = -g_hn[v].head - 1, hit = 0;
    // canonical terms are offset-free: reuse occurs via a tiny scan
    int stack[512], sp = 0; stack[sp++] = w;
    while (sp > 0 && !hit) {
      int t = stack[--sp];
      if (g_hn[t].head < 0) { if (-g_hn[t].head - 1 == slot) hit = 1; }
      else for (int i = 0; i < g_hn[t].nargs && sp < 512; i++) stack[sp++] = HARG(t, i);
    }
    if (!hit) goto reject;
  }
  // CD lane: the weight cap.  Over-cap conclusions are rejected BEFORE
  // the dedup add, so a later deepening round re-derives and admits
  // them; g_cd_overcap records that the cap (not saturation) pruned.
  // Emissions are wall-checked directly: the subsumption machinery
  // below runs between the join loops' attempt-counter checks, and an
  // emit-heavy stretch must not outrun the budget.
  // CONE EXEMPTION (the resonance idea, with the backward cone as the
  // hint generator): a conclusion that unifies a goal-regression
  // subgoal is a candidate step of a goal-closing chain -- it is kept
  // WHATEVER its size.  The deep PM intermediates (hypothetical
  // syllogism instances run ~25 symbols) sit above any cap the
  // forward space can saturate, but they lie ON the regression cone.
  {
    int cd_size = 0, cd_reso = 0;
    if (g_horn_cd) {
      static int cd_emits = 0;
      if ((++cd_emits & 0xff) == 0 && horn_now() - g_horn_t0 > g_horn_wall) {
        g_horn_stop = 1;
        goto reject;
      }
      // Otter's max_distinct_vars: the classic CD branching cut.  A
      // conclusion sprouting many distinct variables is a promiscuous
      // joiner whatever its size.
      if (g_can_nv > g_cd_maxvars) goto reject;
      cd_size = horn_size(l2) + horn_size(r2);
      cd_reso = horn_cd_resonant(l2, r2);
      if (cd_size > g_cd_cap && !cd_reso) { g_cd_overcap = 1; goto reject; }
    }
  {
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 2];
  size_t lo = 0, ro = 0;
  int remap[HORN_REL_VARS], nv = 0;
  for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
  if (!horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv)) goto reject;
  if (!horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv)) goto reject;
  snprintf(key, sizeof key, "%s=%s", lb, rb);
  if (!horn_dedup_add(key)) goto reject;
  // Suppress the mirror too: print r-then-l under a fresh renumbering
  // and pre-claim that key, so the flipped orientation is never
  // released as a separate unit.
  {
    static char fl[TP_SIDE_LEN], fr[TP_SIDE_LEN];
    size_t fo = 0, go = 0;
    int remap2[HORN_REL_VARS], nv2 = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap2[i] = -1;
    if (horn_print(tp, fr, HORN_SIDE_MAX, &fo, r2, remap2, &nv2) &&
        horn_print(tp, fl, HORN_SIDE_MAX, &go, l2, remap2, &nv2)) {
      snprintf(key, sizeof key, "%s=%s", fr, fl);
      horn_dedup_add(key);
    }
  }
  // CD lane: generalization subsumption -- an instance of a live fact
  // (input schema or earlier release) is redundant.  The dedup entry
  // above stays claimed: the candidate is redundant forever (whatever
  // subsumes its subsumer subsumes it).
  if (g_horn_cd && horn_cd_fwd_subsumed(l2, r2, cd_size)) {
    g_cd_fsub++;
    goto reject;
  }
  // New unit: joins the fact set immediately (chains within the round
  // are picked up next round via the semi-naive base) and the pending
  // release list for the final append.  In the CD lane the resonance
  // hit plays the sg-hit role: priority pop + kept-window tier.
  int sgh = g_horn_cd ? cd_reso : horn_sg_probe(l2, r2);
  if (g_horn_cd && cd_reso) {
    g_cd_reso_hits++;
    // A resonator most of the space matches carries no signal -- the
    // goal is shaped like a generic lemma of its calculus (measured:
    // 252k of 255k releases on LCL054-10, whose Frege goal is the
    // generic implication-composition shape).  Drop the resonators and
    // let the pure weight discipline run.
    if (g_cd_reso_hits > 2000 && g_cd_reso_hits * 2 > (long)g_hrel_n) {
      g_cd_reso_n = 0;
      fprintf(stderr, "FindProof: Horn CD: resonators flooded "
              "(%ld of %d released) -- disabled\n",
              g_cd_reso_hits, g_hrel_n);
    }
  }
  horn_fact_add(l2, r2, g_can_nv);
  if (sgh) {
    g_hf[g_hf_n - 1].sghit = 1;
    horn_heap_push(&g_hpheap, g_hf_n - 1);
  }
  if (g_horn_goal_stop && !g_horn_goal_reached && g_hg_n > 0 &&
      horn_goal_probe(l2, r2)) {
    g_horn_goal_reached = 1;
    g_horn_stop = 1;
    fprintf(stderr, "FindProof: Horn release: goal-subsuming unit %s = %s\n",
            lb, rb);
  }
  if (g_hrel_n >= g_hrel_cap) {
    g_hrel_cap = (g_hrel_cap == 0) ? (1 << 12) : g_hrel_cap * 2;
    g_hrel = (HRel *)realloc(g_hrel, (size_t)g_hrel_cap * sizeof(HRel));
    if (g_hrel == NULL) tp_giveup("OOM in Horn release");
  }
  HRel *re = &g_hrel[g_hrel_n];
  re->l = strdup(lb); re->r = strdup(rb);
  re->tl = l2; re->tr = r2;
  re->ground = (g_can_nv == 0);
  re->sg_hit = sgh;
  re->goal_syms = 0;   // filled at ranking time
  re->n_weak = 0;
  {
    int cnt[HORN_REL_VARS];
    memset(cnt, 0, sizeof cnt);
    int stack[512], sp = 0;
    stack[sp++] = l2; stack[sp++] = r2;
    while (sp > 0) {
      int t = stack[--sp];
      if (g_hn[t].head < 0) cnt[-g_hn[t].head - 1]++;
      else for (int i = 0; i < g_hn[t].nargs && sp < 512; i++)
        stack[sp++] = HARG(t, i);
    }
    for (int i = 0; i < g_can_nv; i++) re->n_weak += (cnt[i] == 1);
  }
  re->size = (int)(lo + ro);
  re->order = g_hrel_n;
  re->goal_hit = 0;   // filled at ranking time, once bindings are clean
  re->dead = 0;
  g_hrel_n++;
  if (g_hrel_n >= g_horn_max_rel) g_horn_stop = 1;
  return;
  }
  }
reject:
  g_hn_n = hn_mark; g_ha_n = ha_mark;
}

// Backtracking fill: satisfy every condition except skip_pos (already
// bound to the given fact) from PROCESSED facts, under the current
// bindings.  The given-fact discipline enumerates each fact
// combination once -- when its last-processed member is the given.
static void horn_fill(TpProblem *tp, const HRule *ru, int ci, int skip_pos) {
  if (g_horn_stop) return;
  while (ci == skip_pos) ci++;
  if (ci >= ru->nc) { horn_emit_release(tp, ru); return; }
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > g_horn_max_attempts || horn_now() - g_horn_t0 > g_horn_wall ||
       g_hn_n > g_horn_max_nodes)) { g_horn_stop = 1; return; }
  int c = ru->cl[ci], coff = 0;
  horn_deref(&c, &coff);
  int off = (ci + 1) * HORN_VSPACE;
  // candidate chains: the condition head's bucket (or every bucket for
  // a still-unbound condition head) plus the var-headed facts.
  for (int pass = 0; pass < 2 && !g_horn_stop; pass++) {
    int e;
    if (pass == 0) e = (g_hn[c].head >= 0) ? g_hix_head[g_hn[c].head] : -2;
    else           e = g_hix_varhead;
    if (e == -2) {  // unbound condition head: scan all facts directly
      for (int f = 0; f < g_hf_n && !g_horn_stop; f++) {
        if (!g_hf[f].processed || g_hf[f].dead) continue;
        for (int orient = 0; orient < 2; orient++) {
          int fl = orient ? g_hf[f].r : g_hf[f].l;
          int fr = orient ? g_hf[f].l : g_hf[f].r;
          int mark = g_htrail_n;
          g_horn_attempts++;
          if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
              horn_unify(ru->ct[ci], 0, fr, off, 0))
            horn_fill(tp, ru, ci + 1, skip_pos);
          horn_undo(mark);
        }
      }
      continue;
    }
    for (; e >= 0 && !g_horn_stop; e = g_hix[e].next) {
      int f = g_hix[e].fact;
      if (!g_hf[f].processed || g_hf[f].dead) continue;
      int fl = g_hix[e].orient ? g_hf[f].r : g_hf[f].l;
      int fr = g_hix[e].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      g_horn_attempts++;
      if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
          horn_unify(ru->ct[ci], 0, fr, off, 0))
        horn_fill(tp, ru, ci + 1, skip_pos);
      horn_undo(mark);
    }
  }
}

// One given step: bind condition cpos of ru with the given fact
// (orientation fixed), then fill the remaining conditions from
// processed facts.
static void horn_given_try(TpProblem *tp, const HRule *ru, int cpos,
                           int fl, int fr) {
  int off = (cpos + 1) * HORN_VSPACE;
  int mark = g_htrail_n;
  g_horn_attempts++;
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > g_horn_max_attempts || horn_now() - g_horn_t0 > g_horn_wall ||
       g_hn_n > g_horn_max_nodes)) { g_horn_stop = 1; return; }
  if (horn_unify(ru->cl[cpos], 0, fl, off, 0) &&
      horn_unify(ru->ct[cpos], 0, fr, off, 0))
    horn_fill(tp, ru, 0, cpos);
  horn_undo(mark);
}

// Dispatch a given fact through the rule-condition index: only the
// rules whose condition head matches (plus var-headed conditions, and
// every entry when the fact side itself is a bare variable) join.
static void horn_given_dispatch(TpProblem *tp, int given) {
  for (int orient = 0; orient < 2 && !g_horn_stop; orient++) {
    int fl = orient ? g_hf[given].r : g_hf[given].l;
    int fr = orient ? g_hf[given].l : g_hf[given].r;
    int h = g_hn[fl].head;
    if (h < 0) {   // var-headed side: any condition could bind it
      for (int e = 0; e < g_hrix_n && !g_horn_stop; e++) {
        if (!g_hr[g_hrix[e].rule].rel) continue;
        horn_given_try(tp, &g_hr[g_hrix[e].rule], g_hrix[e].cpos, fl, fr);
      }
      continue;
    }
    for (int pass = 0; pass < 2 && !g_horn_stop; pass++) {
      int e = (pass == 0) ? g_hrix_head[h] : g_hrix_varhead;
      for (; e >= 0 && !g_horn_stop; e = g_hrix[e].next) {
        if (!g_hr[g_hrix[e].rule].rel) continue;
        horn_given_try(tp, &g_hr[g_hrix[e].rule], g_hrix[e].cpos, fl, fr);
      }
    }
  }
}

static int g_horn_rank_ground = 0;   // THVM_ATP_HORN_GROUND: ground tier on

static int horn_rel_cmp(const void *a, const void *b) {
  const HRel *x = (const HRel *)a, *y = (const HRel *)b;
  if (x->goal_hit != y->goal_hit) return y->goal_hit - x->goal_hit;
  if (x->sg_hit != y->sg_hit) return y->sg_hit - x->sg_hit;   // goal-cone step
  if (g_horn_rank_ground && x->ground != y->ground) return y->ground - x->ground;
  if (x->goal_syms != y->goal_syms) return y->goal_syms - x->goal_syms;
  if (x->n_weak != y->n_weak) return x->n_weak - y->n_weak;   // no weakening vars
  if (x->size != y->size) return x->size - y->size;           // smaller first
  return x->order - y->order;                                 // earlier first
}

// Final selection order: within-group rank FIRST (a round-robin over
// the head-symbol groups), then the quality tiers.  One prolific
// fragment (a ring's additive lemmas) can no longer crowd every other
// signature group out of the kept window; single-group problems are
// unaffected (w then equals the global rank).
static int horn_rel_cmp_rr(const void *a, const void *b) {
  const HRel *x = (const HRel *)a, *y = (const HRel *)b;
  if (x->goal_hit != y->goal_hit) return y->goal_hit - x->goal_hit;
  if (x->sg_hit != y->sg_hit) return y->sg_hit - x->sg_hit;
  if (x->w != y->w) return x->w - y->w;
  return horn_rel_cmp(a, b);
}

// The pass: decode, chain, rank, append.  Runs after tp_parse and
// BEFORE SInE so released facts participate in axiom selection.
static void tp_horn_release(TpProblem *tp) {
  int n = tp->n_cls;
  // CD lane flag before any fact is added: fingerprints ride the adds.
  {
    const char *ce = getenv("THVM_ATP_HORN_CD");
    g_horn_cd = (ce != NULL && ce[0] != '0') ? atoi(ce) : 0;
    g_cd_fsub = g_cd_bsub = g_cd_reso_hits = 0;
    g_cd_rounds = 0; g_cd_overcap = 0; g_cd_reso_n = 0;
  }
  // Parse every positive clause; remember each clause's term pair.
  int *pl = (int *)malloc((size_t)n * sizeof(int));
  int *prr = (int *)malloc((size_t)n * sizeof(int));
  int *pnv = (int *)malloc((size_t)n * sizeof(int));
  u8  *is_disp = (u8 *)calloc((size_t)n, 1);
  HParse *ps = (HParse *)malloc(sizeof(HParse));
  if (pl == NULL || prr == NULL || pnv == NULL || is_disp == NULL || ps == NULL)
    tp_giveup("OOM in Horn release");
  g_horn_disp = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
  if (g_horn_disp == NULL) tp_giveup("OOM in Horn release");
  int n_disp = 0;
  g_hg_n = 0;
  for (int c = 0; c < n; c++) {
    pl[c] = prr[c] = -1;
    ps->nv = 0;
    int l = horn_parse_side(tp, ps, tp->cls[c].lhs);
    int r = (l >= 0) ? horn_parse_side(tp, ps, tp->cls[c].rhs) : -1;
    if (l < 0 || r < 0) continue;
    if (tp->cls[c].neg) {
      if (g_hg_n < 64) { g_hg_l[g_hg_n] = l; g_hg_r[g_hg_n] = r; g_hg_n++; }
      continue;
    }
    pl[c] = l; prr[c] = r; pnv[c] = ps->nv;
    if (horn_disp_check(l, r) || horn_disp_check(r, l)) {
      is_disp[c] = 1;
      int f = (g_hn[l].head >= 0) ? g_hn[l].head : g_hn[r].head;
      if (!g_horn_disp[f]) { g_horn_disp[f] = 1; n_disp++; }
    }
  }
  int released_kept = 0;
  if (n_disp == 0) {
    // Slice mode: free the pool slot at once on a non-CD problem.
    if (g_horn_cd >= 2)
      tp_giveup("horn-cd slice: no encoded-Horn dispatcher");
    goto done;   // not an encoded-Horn problem
  }
  if (g_horn_cd >= 2) {
    // The CD discipline targets ground saturation-style goals; the
    // existential (planning) and oversized-KB families already crack
    // via the v2 steered lane in the wm slice.
    int ex = 0;
    for (int g = 0; g < g_hg_n && !ex; g++)
      ex = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    if (ex) tp_giveup("horn-cd slice: existential goal (v2 lane covers it)");
    if (tp->n_cls > 4000) tp_giveup("horn-cd slice: oversized KB");
  }
  g_hix_head = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdup_slots = g_horn_cd ? (HORN_DEDUP_SLOTS << 2) : HORN_DEDUP_SLOTS;
  g_hdup = (char **)calloc(g_hdup_slots, sizeof(char *));
  if (g_hix_head == NULL || g_hdup == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) g_hix_head[i] = -1;
  for (int i = 0; i < HORN_NSLOTS; i++) { g_hb[i].t = -1; g_can_map[i] = -1; }
  // Definition pass: collect the definitional unit facts first, so
  // every fact, rule and goal below normalizes into one alphabet.
  g_hdef_l = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdef_r = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  if (g_hdef_l == NULL || g_hdef_r == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) { g_hdef_l[i] = g_hdef_r[i] = -1; }
  int n_defs = 0;
  for (int c = 0; c < n; c++) {
    if (pl[c] < 0 || is_disp[c] || tp->cls[c].neg) continue;
    HRule ru;
    if (horn_decode_side(pl[c], prr[c], &ru) ||
        horn_decode_side(prr[c], pl[c], &ru)) continue;
    horn_def_note(pl[c], prr[c]);
    horn_def_note(prr[c], pl[c]);
  }
  for (int i = 0; i < TP_MAX_SYMS; i++) n_defs += (g_hdef_r[i] >= 0);
  // Split parsed positives into rules and facts; seed the dedup set
  // with every original unit (both orientations) so releases never
  // re-add an input equation.
  int n_rules = 0;
  for (int c = 0; c < n; c++) {
    if (pl[c] < 0 || is_disp[c]) continue;
    HRule ru;
    if (horn_decode_side(pl[c], prr[c], &ru) ||
        horn_decode_side(prr[c], pl[c], &ru)) {
      ru.nv = pnv[c];
      ru.rel = 1;
      ru.conj = tp->cls[c].conj;
      for (int i = 0; i < ru.nc; i++) {
        ru.cl[i] = horn_norm(ru.cl[i]);
        ru.ct[i] = horn_norm(ru.ct[i]);
      }
      ru.concl_l = horn_norm(ru.concl_l);
      ru.concl_r = horn_norm(ru.concl_r);
      if (g_hr_n >= g_hr_cap) {
        g_hr_cap = (g_hr_cap == 0) ? (1 << 10) : g_hr_cap * 2;
        g_hr = (HRule *)realloc(g_hr, (size_t)g_hr_cap * sizeof(HRule));
        if (g_hr == NULL) tp_giveup("OOM in Horn release");
      }
      g_hr[g_hr_n++] = ru;
      n_rules++;
    } else {
      int fl = horn_norm(pl[c]), fr = horn_norm(prr[c]);
      if (!horn_eq(fl, fr)) {          // definitions trivialize themselves
        horn_fact_add(fl, fr, pnv[c]);
        g_hf[g_hf_n - 1].conj = tp->cls[c].conj;
      }
    }
    // dedup seed: canonical print of both orientations
    static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 2];
    for (int orient = 0; orient < 2; orient++) {
      horn_can_reset();
      int hn_mark = g_hn_n, ha_mark = g_ha_n;
      int budget = 1 << 16;
      int a = horn_apply(orient ? prr[c] : pl[c], 0, &budget);
      int b = (a >= 0) ? horn_apply(orient ? pl[c] : prr[c], 0, &budget) : -1;
      size_t lo = 0, ro = 0;
      int remap[HORN_REL_VARS], nv = 0;
      for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
      if (a >= 0 && b >= 0 &&
          horn_print(tp, lb, sizeof lb, &lo, a, remap, &nv) &&
          horn_print(tp, rb, sizeof rb, &ro, b, remap, &nv)) {
        snprintf(key, sizeof key, "%s=%s", lb, rb);
        horn_dedup_add(key);
      }
      g_hn_n = hn_mark; g_ha_n = ha_mark;   // seed applies are temporary
    }
  }
  if (n_rules == 0 || g_hf_n == 0) goto done;
  // Given-fact agenda loop (UR-resolution shape): pop the smallest
  // unprocessed fact, mark it processed, and join it into every rule
  // position against the already-processed facts.  Smallest-first
  // keeps the chaining on small lemmas instead of drowning in deep
  // instantiations, exactly like a weight-ordered given-clause loop.
  g_horn_t0 = horn_now();
  g_horn_stop = 0; g_horn_attempts = 0;
  // CD lane defaults: wall-governed chaining (the deep detachment
  // chains ARE the proof search here, not a preprocessing garnish), so
  // most of the run budget goes to the lane and the attempt cap steps
  // aside.  An explicit --horn-wall still overrides below.
  if (g_horn_cd) {
    double w = 0.8 * g_cli_wall_sec;
    g_horn_wall = (w > HORN_WALL_SEC) ? w : HORN_WALL_SEC;
    g_horn_max_attempts = (long)4e18;
    // Post-subsumption accepts ARE the CD search space: the v1 pool
    // cap ended a healthy cap-18 run mid-wall (400k accepts at 14s).
    g_horn_max_rel = 4000000;
    g_horn_max_nodes = (1 << 24);
  }
  {
    const char *we = getenv("THVM_ATP_HORN_WALL");
    const char *me = getenv("THVM_ATP_HORN_MAX");
    if (we != NULL && atof(we) > 0.0) g_horn_wall = atof(we);
    if (me != NULL && atoi(me) > 0) g_horn_max_rel = atoi(me);
  }
  g_hrix_head = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  if (g_hrix_head == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) g_hrix_head[i] = -1;
  for (int i = 0; i < g_hr_n; i++)
    for (int j = 0; j < g_hr[i].nc; j++)
      horn_rule_index(i, j, g_hn[g_hr[i].cl[j]].head);
  // Goals normalize into the releases' (expanded) alphabet BEFORE the
  // loop: the backward cone regresses from them and the emit-time goal
  // probe unifies releases against them.
  for (int g = 0; g < g_hg_n; g++) {
    g_hg_l[g] = horn_norm(g_hg_l[g]);
    g_hg_r[g] = horn_norm(g_hg_r[g]);
  }
  // CD lane: the initial weight cap.  Auto = the largest INPUT fact
  // plus slack; the goal deliberately does NOT enter the max -- the
  // and/equivalent PM goals expand to 25-50 symbols and a goal-sized
  // initial cap admits a space no wall can saturate, so the deepening
  // discipline never engages.  The goal probe is by unification (a
  // small general schema closes a big ground goal) and a goal-sized
  // final conclusion strictly matches a goal resonator, which is
  // cap-exempt.
  if (g_horn_cd) {
    int cap = 0;
    for (int f = 0; f < g_hf_n; f++)
      if (g_hf[f].size > cap) cap = g_hf[f].size;
    g_cd_cap = cap + 2;
    const char *cc = getenv("THVM_ATP_HORN_CD_CAP");
    const char *cs = getenv("THVM_ATP_HORN_CD_STEP");
    const char *cv = getenv("THVM_ATP_HORN_CD_VARS");
    if (cc != NULL && atoi(cc) > 0) g_cd_cap = atoi(cc);
    if (cs != NULL && atoi(cs) > 0) g_cd_step = atoi(cs);
    if (cv != NULL && atoi(cv) > 0) g_cd_maxvars = atoi(cv);
    // Goal-shape resonators from the normalized goals; input facts
    // already goal-shaped (an axiom the goal instantiates) join the
    // priority agenda immediately.
    horn_cd_reso_build();
    for (int f = 0; f < g_hf_n; f++)
      if (horn_cd_resonant(g_hf[f].l, g_hf[f].r)) {
        g_hf[f].sghit = 1;
        horn_heap_push(&g_hpheap, f);
      }
    fprintf(stderr, "FindProof: Horn CD lane: cap %d (step %d, wall %.1fs, "
            "%d resonator(s))\n",
            g_cd_cap, g_cd_step, g_horn_wall, g_cd_reso_n);
  }
  // Oversized KBs: focus the loop on the conjecture's relevance cone.
  // THVM_ATP_HORN_REL=0 disables.
  g_horn_input_nf = g_hf_n;
  {
    const char *rl = getenv("THVM_ATP_HORN_REL");
    const char *te = getenv("THVM_ATP_HORN_REL_TOL");
    if (te != NULL && atof(te) > 0.0) g_horn_gate_tol = atof(te);
    if (!(rl != NULL && rl[0] == '0') && tp->n_cls > 4000) {
      g_horn_gate_on = 1;
      horn_relevance_gate(g_horn_gate_tol);
    }
  }
  // Backward subgoal cone (v2): bounded regression from the goals, a
  // wall slice ahead of the forward loop.  Auto-gated (see
  // g_horn_steer); THVM_ATP_HORN_BWD: 0 disables, 2 forces.
  {
    const char *be = getenv("THVM_ATP_HORN_BWD");
    const char *bd = getenv("THVM_ATP_HORN_BWD_DEPTH");
    const char *bm = getenv("THVM_ATP_HORN_BWD_MAX");
    const char *gs = getenv("THVM_ATP_HORN_GOAL_STOP");
    if (bd != NULL && atoi(bd) > 0) g_hsg_depth_cap = atoi(bd);
    if (bm != NULL && atoi(bm) > 0) g_hsg_max = atoi(bm);
    g_horn_goal_stop = !(gs != NULL && gs[0] == '0');
    g_hsg_stop = 0; g_hsg_checks = 0; g_horn_goal_reached = 0;
    int existential = 0;
    for (int g = 0; g < g_hg_n && !existential; g++)
      existential = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    g_horn_steer = (be != NULL && be[0] == '2')
                     ? 1 : (g_horn_gate_on || existential);
    if (be != NULL && be[0] == '0') g_horn_steer = 0;
    if (g_horn_steer && g_hg_n > 0) {
      double cw = 0.25 * g_horn_wall;
      g_hsg_deadline = g_horn_t0 + ((cw < 0.6) ? cw : 0.6);
      horn_backward_cone(tp);
      // Input facts already on a subgoal join the priority agenda too.
      for (int f = 0; f < g_hf_n && g_hsg_n > 0; f++)
        if (horn_sg_probe(g_hf[f].l, g_hf[f].r)) {
          g_hf[f].sghit = 1;
          horn_heap_push(&g_hpheap, f);
        }
    }
  }
  // Pick-given ratio (Otter shape): of every 4 pops, HORN_RATIO come
  // from the size heap and the rest by age (FIFO = delta-driven
  // breadth-first).  Pure smallest-first starves any fragment whose
  // small lemmas are unbounded (a ring's additive permutations bury
  // every multiplicative fact), while the age pops keep the frontier
  // fair and reach the DEEP releases (x*0 = 0 needs the 4-condition
  // distributivity joins).  THVM_ATP_HORN_RATIO overrides (0..4).
  {
    // The CD lane is pure smallest-first: its size cap already bounds
    // every fragment, so the age pops that keep v1's frontier fair
    // would only dilute the small-lemma order Otter's CD runs rely on.
    int ratio = g_horn_cd ? 4 : 1;
    const char *re = getenv("THVM_ATP_HORN_RATIO");
    if (re != NULL && re[0] != '\0') ratio = atoi(re);
    if (ratio < 0) ratio = 0;
    if (ratio > 4) ratio = 4;
    int age_cursor = 0;
    long n_pops = 0;
    for (;;) {
      while (!g_horn_stop) {
        int given = -1;
        // Goal-directed pops preempt the ratio scheme: a fact that
        // unifies with a backward subgoal is a candidate step of a
        // goal-closing chain -- joining it first produces the parent
        // subgoal's instance next, walking the cone up to the goal.
        // g_horn_steer keeps this honest: the queue is only ever fed
        // for existential/relevance-gated goals, so the baseline
        // agenda that reaches ground goals on its own (LCL192) stays
        // undiluted.
        do { given = horn_heap_pop(&g_hpheap); }
        while (given >= 0 && (g_hf[given].processed || g_hf[given].dead));
        if (given < 0 && (n_pops++ & 3) >= ratio) {
          while (age_cursor < g_hf_n &&
                 (g_hf[age_cursor].processed || g_hf[age_cursor].dead))
            age_cursor++;
          if (age_cursor < g_hf_n) given = age_cursor;
        }
        if (given < 0) {
          do { given = horn_heap_pop(&g_hheap); }
          while (given >= 0 && (g_hf[given].processed || g_hf[given].dead));
        }
        if (given < 0) {
          while (age_cursor < g_hf_n &&
                 (g_hf[age_cursor].processed || g_hf[age_cursor].dead))
            age_cursor++;
          if (age_cursor >= g_hf_n) break;   // fixpoint: agenda exhausted
          given = age_cursor;
        }
        // CD lane, lazy back subsumption: a fact a LATER release
        // generalizes is dead weight -- its joins only re-derive
        // instances the subsumer covers.  Checked at pop (the only
        // point it would cost anything), tail-scanned (older facts
        // were checked forward at this fact's emit).
        if (g_horn_cd && horn_cd_pop_subsumed(given)) {
          g_hf[given].dead = 1;
          if (given >= g_horn_input_nf)
            g_hrel[given - g_horn_input_nf].dead = 1;
          g_cd_bsub++;
          continue;
        }
        g_hf[given].processed = 1;
        g_hf[given].given = 1;
        horn_given_dispatch(tp, given);
      }
      // CD lane, iterative deepening: the capped space saturated
      // without the goal and the cap (not true saturation) pruned --
      // raise it and re-run every live fact as given again.  The
      // dedup set short-circuits the replayed joins; only conclusions
      // the old cap rejected are new.
      if (g_horn_cd && !g_horn_stop && !g_horn_goal_reached && g_cd_overcap &&
          horn_now() - g_horn_t0 < g_horn_wall) {
        g_cd_cap += g_cd_step;
        g_cd_overcap = 0;
        g_cd_rounds++;
        for (int f = 0; f < g_hf_n; f++)
          if (!g_hf[f].dead && g_hf[f].processed) {
            g_hf[f].processed = 0;
            g_hf[f].given = 0;
            horn_heap_push(&g_hheap, f);
          }
        age_cursor = 0;
        continue;
      }
      // Iterative widening: the RELEVANT closure saturated without the
      // goal and budget remains -- relax the gate, re-run the cone over
      // the newly admitted rules, and continue the loop.
      if (g_horn_stop || !g_horn_gate_on || g_horn_goal_reached) break;
      if (g_horn_gate_tol > 100.0) break;
      g_horn_gate_tol *= 3.0;
      horn_relevance_gate(g_horn_gate_tol);
      if (g_horn_steer && g_hg_n > 0 && g_hsg_n < g_hsg_max) {
        g_hsg_stop = 0;
        double cd = horn_now() + 0.25 * g_horn_wall;
        g_hsg_deadline = (cd < g_horn_t0 + g_horn_wall)
                           ? cd : g_horn_t0 + g_horn_wall;
        horn_backward_cone(tp);
      }
      age_cursor = 0;   // rescan: parked facts may have re-entered
    }
  }
  // Rank + append.  Under the engine cap keep everything that fits;
  // over it (SInE will run) hand selection up to 256 ranked releases.
  {
    // Goal subsumption: a release some instance of which IS a goal
    // equation closes the refutation in one rewrite -- rank it first.
    // The join's bindings are all undone here, so slot spaces 0
    // (release canonical vars) and HORN_VSPACE (goal vars) are clean.
    // Goals were already normalized into the releases' (expanded)
    // alphabet before the loop.
    int n_goal_hit = 0;
    // Goal-symbol relevance (SInE flavor): the DISCRIMINATING goal
    // symbols are those NOT ubiquitous across the input (frequency
    // <= max(2, n/4) clauses) -- a release mentioning them speaks
    // about the goal's constants and outranks the anonymous
    // variable-lemma flood.
    u32 *gen = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
    u32 *stamp = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
    u8  *gsym = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
    if (gen == NULL || stamp == NULL || gsym == NULL) tp_giveup("OOM in Horn release");
    for (int c = 0; c < n; c++) {
      if (pl[c] < 0) continue;
      int st[512], sp = 0;
      st[sp++] = pl[c]; st[sp++] = prr[c];
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (stamp[g_hn[t].head] != (u32)(c + 1)) {
            stamp[g_hn[t].head] = (u32)(c + 1);
            gen[g_hn[t].head]++;
          }
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
    }
    u32 ubiq = (u32)((n > 8) ? n / 4 : 2);
    for (int g = 0; g < g_hg_n; g++) {
      int st[512], sp = 0;
      st[sp++] = g_hg_l[g]; st[sp++] = g_hg_r[g];
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (gen[g_hn[t].head] <= ubiq) gsym[g_hn[t].head] = 1;
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
    }
    for (int i = 0; i < g_hrel_n; i++) {
      HRel *re = &g_hrel[i];
      int st[512], sp = 0, seen_n = 0;
      int seen[64];
      st[sp++] = re->tl; st[sp++] = re->tr;
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (gsym[g_hn[t].head]) {
            int dup = 0;
            for (int k = 0; k < seen_n; k++) dup |= (seen[k] == g_hn[t].head);
            if (!dup && seen_n < 64) seen[seen_n++] = g_hn[t].head;
          }
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
      re->goal_syms = seen_n;
      re->grp = (g_hn[re->tl].head >= 0) ? g_hn[re->tl].head
                                         : g_hn[re->tr].head;
      if (re->grp < 0) re->grp = 0;
      for (int g = 0; g < g_hg_n && !re->goal_hit; g++) {
        for (int orient = 0; orient < 2 && !re->goal_hit; orient++) {
          int gl = orient ? g_hg_r[g] : g_hg_l[g];
          int gr = orient ? g_hg_l[g] : g_hg_r[g];
          int mark = g_htrail_n;
          if (horn_unify(re->tl, 0, gl, HORN_VSPACE, 0) &&
              horn_unify(re->tr, 0, gr, HORN_VSPACE, 0)) re->goal_hit = 1;
          horn_undo(mark);
        }
      }
      n_goal_hit += re->goal_hit;
    }
    free(gen); free(stamp); free(gsym);
    if (n_goal_hit > 0)
      fprintf(stderr, "FindProof: Horn release: %d goal-subsuming unit(s)\n",
              n_goal_hit);
    int n_pos = 0;
    for (int c = 0; c < tp->n_cls; c++) n_pos += !tp->cls[c].neg;
    int room = (n_pos < (int)PR_MAX_EQNS) ? (int)PR_MAX_EQNS - n_pos
                                          : (int)PR_MAX_EQNS;
    {
      const char *ke = getenv("THVM_ATP_HORN_KEEP");
      const char *ge = getenv("THVM_ATP_HORN_GROUND");
      if (ke != NULL && atoi(ke) > 0 && atoi(ke) < room) room = atoi(ke);
      g_horn_rank_ground = (ge != NULL && ge[0] != '\0' && ge[0] != '0');
    }
    if (g_hrel_n > room) {
      qsort(g_hrel, (size_t)g_hrel_n, sizeof(HRel), horn_rel_cmp);
      u32 *taken = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
      if (taken == NULL) tp_giveup("OOM in Horn release");
      for (int i = 0; i < g_hrel_n; i++)
        g_hrel[i].w = (int)taken[g_hrel[i].grp]++;
      free(taken);
      qsort(g_hrel, (size_t)g_hrel_n, sizeof(HRel), horn_rel_cmp_rr);
    }
    for (int i = 0, kept = 0; i < g_hrel_n && kept < room; i++) {
      if (g_hrel[i].dead) continue;   // back-subsumed (CD lane)
      if (tp->n_cls >= TP_MAX_CLAUSES) break;
      TpClause *cl = &tp->cls[tp->n_cls++];
      snprintf(cl->lhs, sizeof cl->lhs, "%s", g_hrel[i].l);
      snprintf(cl->rhs, sizeof cl->rhs, "%s", g_hrel[i].r);
      cl->neg = 0; cl->conj = 0;
      tp_scan_term(tp, cl->lhs);
      tp_scan_term(tp, cl->rhs);
      released_kept++;
      kept++;
    }
  }
  if (released_kept > 0) g_cli_horn_applied = 1;
  {
    int n_sg_hit = 0;
    for (int i = 0; i < g_hrel_n; i++) n_sg_hit += g_hrel[i].sg_hit;
    fprintf(stderr, "FindProof: Horn release: %d dispatcher(s), %d rule(s), "
            "%d fact(s), %d definition(s), %d subgoal(s), %d released "
            "(%d sg-hit%s), %d kept\n",
            n_disp, n_rules, g_hf_n - g_hrel_n, n_defs, g_hsg_n, g_hrel_n,
            n_sg_hit, g_horn_goal_reached ? ", goal reached" : "",
            released_kept);
    if (g_horn_cd)
      fprintf(stderr, "FindProof: Horn CD: cap %d after %d deepening "
              "round(s), %ld fwd-subsumed, %ld back-subsumed, %ld "
              "resonant, %.1fs\n",
              g_cd_cap, g_cd_rounds, g_cd_fsub, g_cd_bsub, g_cd_reso_hits,
              horn_now() - g_horn_t0);
  }
done:
  free(pl); free(prr); free(pnv); free(is_disp); free(ps);
  free(g_horn_disp); g_horn_disp = NULL;
  free(g_hn); g_hn = NULL; g_hn_n = g_hn_cap = 0;
  free(g_ha); g_ha = NULL; g_ha_n = g_ha_cap = 0;
  free(g_hf); g_hf = NULL; g_hf_n = g_hf_cap = 0;
  free(g_dtn); g_dtn = NULL; g_dtn_n = g_dtn_cap = 0;
  free(g_dte); g_dte = NULL; g_dte_n = g_dte_cap = 0;
  free(g_dtl); g_dtl = NULL; g_dtl_n = g_dtl_cap = 0;
  free(g_hix); g_hix = NULL; g_hix_n = g_hix_cap = 0;
  free(g_hix_head); g_hix_head = NULL; g_hix_varhead = -1;
  free(g_hr); g_hr = NULL; g_hr_n = g_hr_cap = 0;
  free(g_hheap.a);  g_hheap.a = NULL;  g_hheap.n = g_hheap.cap = 0;
  free(g_hpheap.a); g_hpheap.a = NULL; g_hpheap.n = g_hpheap.cap = 0;
  free(g_hsg); g_hsg = NULL; g_hsg_n = g_hsg_cap = 0;
  free(g_hdef_l); g_hdef_l = NULL;
  free(g_hdef_r); g_hdef_r = NULL;
  free(g_hrix); g_hrix = NULL; g_hrix_n = g_hrix_cap = 0;
  free(g_hrix_head); g_hrix_head = NULL; g_hrix_varhead = -1;
  if (g_hdup != NULL) {
    for (u32 i = 0; i < g_hdup_slots; i++) free(g_hdup[i]);
    free(g_hdup); g_hdup = NULL; g_hdup_n = 0;
  }
  for (int i = 0; i < g_hrel_n; i++) { free(g_hrel[i].l); free(g_hrel[i].r); }
  free(g_hrel); g_hrel = NULL; g_hrel_n = g_hrel_cap = 0;
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
  // Encompassment demodulation drops CPs joined via skolem-ground steps
  // without Twee's simplerThan redundancy discipline, so a saturation
  // under it certifies nothing about the full problem: GaveUp, never
  // Satisfiable.
  const char *en = getenv("THVM_ATP_ENCOMPASS");
  // Simpler-join carries the simplerThan discipline but not the
  // below-peak connectedness side condition of Twee's step3, so its
  // CP drops are not exactly completeness-preserving either: GaveUp,
  // never Satisfiable.
  const char *sj = getenv("THVM_ATP_SIMPLER_JOIN");
  return !g_cli_goal_existential && !g_cli_sine_applied && !g_cli_horn_applied
      && !g_cli_equals_applied
      && (mw == NULL || *mw == '0') && (am == NULL || *am == '0')
      && (rr == NULL || *rr == '0') && (en == NULL || *en == '0')
      && (sj == NULL || *sj == '0');
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
    // Leaf clause: cite the problem file so GDV can match it against the
    // problem's axioms (includes are resolved by GDV) and complete leaf
    // verification.
    fprintf(fp, ", file('%s')).\n", g_cli_problem);
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

// Resolve a chain step's rule citation by REPLAY: the recorded index is
// wrong for unorientable-equation steps (the FT normalize hands the
// proofsteps fill an ESET-internal index, which the emitter would map
// through RULE slots -- the LAT171-1 mislabeled-citation bug).  Try the
// recorded slot first, then scan every slot for the equation that
// actually reproduces before -> after in one application (either
// direction, any position).  Returns the resolved slot or n_rules.
// Returns the resolved TRACE id (dead rules live only in the trace --
// an interreduced-away equation is absent from the final slots but its
// derivation record remains), or ATP_TRACE_NONE.
static u8 cli_trace_pair(AtpState *s, u32 ti, Term *l, Term *r) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace) return 0;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) return 0;
  *l = term_ctr_at(e, 2);
  *r = term_ctr_at(e, 3);
  if (term_tag(*l) == TAG_NUM && term_tag(*r) == TAG_NUM) {
    Term ll = 0, rr = 0;
    if (!atp_trace_eq_load(s, ti, &ll, &rr)) return 0;
    *l = ll;
    *r = rr;
  }
  return (*l != 0 && *r != 0);
}
static u8 cli_step_replays(AtpState *s, const AtpProofStep *st,
                           Term l, Term r) {
  PcRwSet acc;
  acc.n = 0;
  pc_rewrite_all(st->before, l, r, &acc);
  pc_rewrite_all(st->before, r, l, &acc);
  for (int k = 0; k < acc.n; k++) {
    if (kbo_eq(acc.out[k], st->after)) return 1;
  }
  return 0;
}
// Trace-pair cache: decode each trace entry's equation at most once per
// emission (the deep resolver's midpoint search was re-loading and
// re-scanning per candidate -- the quadratic blowout that truncated
// emission under the prove timeout).
#define CLI_TP_CACHE_MAX 4096
static struct { Term l, r; u8 ok, loaded; } g_cli_tp[CLI_TP_CACHE_MAX];
static u32 g_cli_tp_n = 0;
static void cli_tp_cache_reset(AtpState *s) {
  g_cli_tp_n = (s->n_trace < CLI_TP_CACHE_MAX) ? s->n_trace : CLI_TP_CACHE_MAX;
  // Eager load: the deep resolver's heap rollbacks must never reclaim a
  // cached pair's Terms, so decode everything BEFORE any checkpoint.
  for (u32 i = 0; i < g_cli_tp_n; i++) {
    g_cli_tp[i].ok = cli_trace_pair(s, i, &g_cli_tp[i].l, &g_cli_tp[i].r);
    g_cli_tp[i].loaded = 1;
  }
}
static u8 cli_tp_cached(AtpState *s, u32 ti, Term *l, Term *r) {
  if (ti >= g_cli_tp_n) return cli_trace_pair(s, ti, l, r);
  if (!g_cli_tp[ti].loaded) {
    g_cli_tp[ti].ok = cli_trace_pair(s, ti, &g_cli_tp[ti].l, &g_cli_tp[ti].r);
    g_cli_tp[ti].loaded = 1;
  }
  if (!g_cli_tp[ti].ok) return 0;
  *l = g_cli_tp[ti].l;
  *r = g_cli_tp[ti].r;
  return 1;
}

static u32 cli_resolve_step_trace(AtpState *s, const AtpProofStep *st) {
  if (st->before == 0 || st->after == 0) return ATP_TRACE_NONE;
  // Recorded slot first (correct for oriented-rule steps).
  if (st->rule < s->n_rules && s->r_trace != NULL) {
    Term l = s->lhs[st->rule], r = s->rhs[st->rule];
    if (term_tag(l) != TAG_NUM && term_tag(r) != TAG_NUM &&
        cli_step_replays(s, st, l, r)) {
      return s->r_trace[st->rule];
    }
  }
  // Live slots next (cheap; covers mis-indexed eset steps whose
  // equation is still alive).
  for (u32 rr = 0; rr < s->n_rules; rr++) {
    Term l = s->lhs[rr], r = s->rhs[rr];
    if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) continue;
    if (cli_step_replays(s, st, l, r)) {
      return (s->r_trace != NULL) ? s->r_trace[rr] : ATP_TRACE_NONE;
    }
  }
  // Full trace scan: the applied equation was interreduced away.
  // Costly (replay per trace entry) -- opt-in, the deep resolve pass.
  static int deep = -1;
  if (deep < 0) {
    const char *e = getenv("THVM_ATP_PROOF_RESOLVE_DEEP");
    deep = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  }
  if (!deep) return ATP_TRACE_NONE;
  for (u32 ti = 0; ti < s->n_trace; ti++) {
    Term l = 0, r = 0;
    if (!cli_tp_cached(s, ti, &l, &r)) continue;
    if (cli_step_replays(s, st, l, r)) return ti;
  }
  return ATP_TRACE_NONE;
}

// Emit one goal's closing rewrite chain (steps from
// thvm_atp_proof_extract) plus the derivations of every rule cited.
static void cli_emit_goal_chain(FILE *fp, AtpState *s, const PrProblem *pr,
                                Term gl, Term gr, u32 goal_id,
                                const AtpProofStep *steps, u32 n_steps,
                                u8 *printed) {
  static u32 resolved[ATP_PROOF_MAX_STEPS];
  static u32 resolved2[ATP_PROOF_MAX_STEPS];   // 2nd id of a split step
  static Term midpoint[ATP_PROOF_MAX_STEPS];   // its intermediate term
  cli_tp_cache_reset(s);
  for (u32 k = 0; k < n_steps; k++) {
    resolved[k]  = cli_resolve_step_trace(s, &steps[k]);
    resolved2[k] = ATP_TRACE_NONE;
    midpoint[k]  = 0;
    static int deep2 = -1;
    if (deep2 < 0) {
      const char *e = getenv("THVM_ATP_PROOF_RESOLVE_DEEP");
      deep2 = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
    }
    if (deep2 && resolved[k] == ATP_TRACE_NONE) {
      // No single recorded equation reproduces before -> after: the
      // recorder coalesced two rewrites into one step.  Find a 2-step
      // decomposition through any midpoint and SPLIT the record at
      // emission so the checker sees single-step inferences.
      const AtpProofStep *st = &steps[k];
      for (u32 ta = 0; ta < s->n_trace && midpoint[k] == 0; ta++) {
        Term la = 0, ra = 0;
        if (!cli_tp_cached(s, ta, &la, &ra)) continue;
        // The candidate replays allocate heavily (fanout x trace Term
        // builds); without rollback the emission child exhausts
        // from-space and dies mid-write (the 4-record stub).  Nothing
        // survives a failed candidate; a found midpoint skips the
        // rollback so its Term stays live.
        u64 _hp = thvm_atp_heap_checkpoint();
        u64 _gc = gc_count();
        PcRwSet acc;
        acc.n = 0;
        pc_rewrite_all(st->before, la, ra, &acc);
        pc_rewrite_all(st->before, ra, la, &acc);
        for (int m = 0; m < acc.n && midpoint[k] == 0; m++) {
          AtpProofStep half = *st;
          half.before = acc.out[m];
          for (u32 tb = 0; tb < s->n_trace && midpoint[k] == 0; tb++) {
            Term lb = 0, rb = 0;
            if (!cli_tp_cached(s, tb, &lb, &rb)) continue;
            if (cli_step_replays(s, &half, lb, rb)) {
              resolved[k]  = ta;
              resolved2[k] = tb;
              midpoint[k]  = acc.out[m];
            }
          }
        }
        if (midpoint[k] == 0 && gc_count() == _gc) thvm_atp_heap_reset(_hp);
      }
    }
    if (resolved[k] != ATP_TRACE_NONE)
      cli_emit_trace(fp, s, pr, resolved[k], printed);
    if (resolved2[k] != ATP_TRACE_NONE)
      cli_emit_trace(fp, s, pr, resolved2[k], printed);
  }
  fprintf(fp, "cnf(goal_%u, negated_conjecture, ", goal_id);
  cli_print_term(fp, pr, gl);
  fputs(" != ", fp);
  cli_print_term(fp, pr, gr);
  fprintf(fp, ", file('%s')).\n", g_cli_problem);
  Term cur_l = gl, cur_r = gr;
  char prev[32];
  snprintf(prev, sizeof prev, "goal_%u", goal_id);
  for (u32 k = 0; k < n_steps; k++) {
    const AtpProofStep *st = &steps[k];
    if (midpoint[k] != 0) {
      // Split record: emit the intermediate disequation first (one
      // application of resolved[k]), then the target via resolved2[k].
      Term mid_l = (st->side == 0u) ? midpoint[k] : cur_l;
      Term mid_r = (st->side == 0u) ? cur_r : midpoint[k];
      fprintf(fp, "cnf(g%u_%um, plain, ", goal_id, k);
      cli_print_term(fp, pr, mid_l);
      fputs(" != ", fp);
      cli_print_term(fp, pr, mid_r);
      fprintf(fp, ", inference(rw, [status(thm)], [%s, t%u])).\n",
              prev, resolved[k]);
      snprintf(prev, sizeof prev, "g%u_%um", goal_id, k);
    }
    if (st->side == 0u) cur_l = st->after; else cur_r = st->after;
    fprintf(fp, "cnf(g%u_%u, plain, ", goal_id, k);
    cli_print_term(fp, pr, cur_l);
    fputs(" != ", fp);
    cli_print_term(fp, pr, cur_r);
    u32 rt = (midpoint[k] != 0) ? resolved2[k] : resolved[k];
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
// Ordered by value: run_schedule runs slices CONCURRENTLY in a pool of
// SCHED_MAX_PAR, EACH with the FULL wall, so the first SCHED_MAX_PAR
// entries are the ones that run to completion -- strongest + most
// complementary first.  frac is now vestigial (each slice gets the full
// wall, not frac*wall); kept so the struct/consumers are unchanged.
static const SchedSlice SCHEDULE[] = {
  // (1) The WM preset -- the strongest general UEQ solver.  Needs the FULL
  // wall for its slower proofs (LCL113-10 23s, REL028-2 15s), which the old
  // sequential 0.22 slice (6.6s at a 30s wall) starved.
  {"wm",      "",                                          0.34},
  // (1b) EXISTENTIAL-GOAL EQUALS ENCODING (Twee addNarrowing port):
  // the WM preset over the eq/true/false-encoded problem.
  // Complementary to the narrowing lane every other slice uses on a
  // non-ground goal: COL009-1 proves in 0.1s encoded (schedule-wide
  // Timeout narrowing); COL004-1 is the exact reverse.  =2 makes the
  // slice exit immediately on ground-goal problems, so it only ever
  // spends a pool slot on the existential family.
  {"equals",  "--equals-goal=2",                           0.0},
  // (1c) CONDENSED-DETACHMENT lane: the encoded-Horn chaining under
  // the classic CD prover discipline (Otter's max_weight + subsumption
  // on the LCL corpus) -- a formula-size cap with iterative deepening
  // kills the promiscuous major-premise towers, generalization
  // subsumption collapses the instance flood, pure smallest-first
  // given selection, and ~80% of the wall on the chaining itself.
  // The blind v1 agenda saturates its 400k-release budget on these
  // without ever nearing the goal (and Vampire's whole casc schedule
  // times out on LCL195-10 too).  =2: exits immediately unless a
  // dispatcher + ground goal is detected, freeing the pool slot on
  // every non-CD problem exactly like the equals slice.
  {"horn-cd", "--horn-cd=2",                               0.0},
  // (2) Goal FLATTENING (Twee/Vampire naming port) + LAZY ground-join
  // (Twee selection-time redundancy) + backward-simplify + auto-max-weight.
  // Cracks deep-goal UEQ that completion stalls on (COL060-1, GRP167-4);
  // connectedness dropped (net cost under lazy-GJ; GRP167-4 26s->1.8s).
  {"flatten",  "--flatten-goal --lazy-ground-join --backward-subsume --backward-demod --auto-max-weight=6 --max-weight-slope=1", 0.33},
  // (2b) BARE flattening: the dressed flatten slice's extras (lazy-GJ,
  // backward-simplify, auto-max-weight) each independently push the
  // wall-edge flatten proofs past the limit (ALG212-10 proves in 27.5s
  // bare, TIMES OUT with ANY of them added) -- the unadorned transform
  // needs its own lane.  Horn-free: with (6) it keeps a released-unit-
  // free lane for problems whose baseline proof the Horn releases
  // perturb (BOO017-10).
  {"flatten-bare", "--flatten-goal --horn=0", 0.0},
  // (3) Goal-directed MNF front + ground-join + a growing MaxWeight bound
  // -- complementary to both (goal-direction the WM preset lacks, pruning
  // the bare MNF front lacks).
  // (5) TWEE-SCORED completion: flatten + Twee's CP score (mode 8, now with
  // the faithful formation-depth term) + pure best-first (w2-modulo=0, no
  // WM age interleave) + dynamic passive re-scoring (cp-set-interreduce =
  // Twee simplifyPassive) + the simplerThan-gated join discipline
  // (--simpler-join, Twee Join.hs allSteps): the join pass culls CPs the
  // plain doR/doE joinability keeps, which on the lattice family turns a
  // wall-starved backup lane into a sprint -- LAT159-1 proves in 1.8s
  // with the gate vs 20.3s without (the 20.3s run never fit the backup
  // slot's leftover wall, so the problem TIMED OUT schedule-wide).  A
  // genuinely different selection regime from the WM/MNF slices above --
  // the portfolio's Twee-native angle.  Displaced from the top-5
  // concurrent pool by the encompass slice (its flatten + lazy-GJ core
  // overlaps slice 2); launches as a pool slot frees.
  // --cp-set-ir-budget=5 puts the passive re-scoring on Twee's OWN task
  // discipline (freq 10s + 5% budget + 20%-dead sample trigger) instead
  // of the every-16-rules full sweep, which ate 82% of the wall on the
  // lattice/group residuals (LAT138-1 @30s: 24.5s in the sweep) -- 3x
  // the completion throughput at the same score function.  Flips
  // LAT149-1 (17.7s), GRP177-1 (14.9s), GRP753-1 (2.7s), GRP766-1
  // (11.9s) standalone at 30s, each a prior schedule-wide Timeout.
  // --orphan-murder=0: Twee has NO Waisenmord -- passive CPs survive
  // parent-rule collapse (Twee re-scores them; WM deletes them).  The
  // GRP706-1 dissection showed the lane's missing content is exactly a
  // buried CP class whose parents collapse before selection: the
  // ld(X,X)-chain CP (twee lemma 100 generalization) forms at pick ~750
  // (pri 839 vs a MixWeight frontier that never passes 265), then its
  // mother (twee lemma 98's rule) is collapsed by the late lemma-96 rule
  // -- under Waisenmord the class must re-derive from scratch each time.
  // With the flag the slice matches Twee's passive-set semantics and
  // GRP706-1 proves (102s dressed; 12.8s with the lean core
  // --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --simpler-join),
  // a prior Timeout at every wall tried.
  {"twee",   "--simpler-join --flatten-goal --lazy-ground-join --backward-subsume --backward-demod --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5", 0.0},
  {"mnf-gj-amw", "--mnf --ground-join --connectedness --auto-max-weight=6 --max-weight-slope=1", 0.33},
  // (4) ENCOMPASSMENT DEMODULATION (Twee reducesSkolem): the WM preset
  // with the skolem-ground unorient gate -- variable-permuted variants
  // canonicalize, keeping R lean on the AC/comm-heavy families where
  // completion otherwise drowns in variants.  Cracks 6 of the 23
  // twee-residual CASC-30 UEQ targets standalone in <=30s (MVA003-1
  // 7.2s, MVA009-1 7.1s, NUN134-1 0.3s, LAT163-1 5.6s, LAT174-1 21.3s,
  // GRP721-1 1.0s -- all Twee-oracle-confirmed), each unreachable by
  // every prior slice.  --max-steps raised: the canonical search runs
  // 30k+ steps/s and hits the 500k default mid-wall.
  {"encompass", "--encompass-demod --max-steps=5000000", 0.0},

  // (5) PURE bare completion (bare engine defaults, GT weight, NO WM preset
  // heuristics, NO ground-join/connectedness).  The WM preset's selection +
  // MaxWeight actively DIVERGES on some CASC UEQ problems -- notably the
  // ifeq-encoded Boolean/relation families (BOO014-10: WM preset + WM-emu
  // fail even @150s, but pure bare PROVES it in 6.0s; ground-join or
  // connectedness on top of bare break it again).  This slice is the
  // portfolio's bare-completion angle for the WM-preset-divergent set.
  {"bare",   "--preset=none --horn=0", 0.0},   // horn-free (see flatten-bare)
  // Backups -- only launch if one of the first five exits early.
  {"wm-gj",   "--ground-join --connectedness",             0.0},
  {"lazy-gj", "--preset=waldmeister-lazy --ground-join --connectedness", 0.0},
  {"bare-gj", "--preset=none --ground-join --connectedness", 0.0},
};
static const int N_SCHED = (int)(sizeof(SCHEDULE) / sizeof(SCHEDULE[0]));

// Per-slice RSS cap (MB) when running slices concurrently, so K parallel
// engines stay well under the machine ceiling.  Each child inherits it as
// THVM_ATP_RSS_ABORT_MB unless the caller already set a (lower) value.
#define SCHED_SLICE_RSS_MB 4500
// Max slices resident at once.  K concurrent engines * SCHED_SLICE_RSS_MB
// bounds peak memory (5 * 4.5GB = 22.5GB, under a 24GB ceiling on 48GB;
// measured practical peak is far lower -- ~7GB for 4 slices on GRP721-1 --
// since slices rarely approach their cap).
#define SCHED_MAX_PAR 5

// Signal-safe teardown of the concurrent schedule's slice children: an
// external SIGTERM/SIGINT to THIS process (e.g. a wrapping `timeout`) would
// otherwise kill the parent while its slice subtrees (child -> shell ->
// FindProof engine) keep running orphaned and accumulate across problems.
// The handler killpg's every launched child group before exiting.  pids are
// stored by slice index; a reaped slot is set back to -1.
#define SCHED_PID_MAX 32
static pid_t g_sched_pids[SCHED_PID_MAX];
static volatile sig_atomic_t g_sched_active = 0;
static void sched_sig_teardown(int sig) {
  if (g_sched_active) {
    for (int i = 0; i < SCHED_PID_MAX; i++)
      if (g_sched_pids[i] > 0) {
        killpg(g_sched_pids[i], SIGKILL);
        kill(g_sched_pids[i], SIGKILL);
      }
  }
  _exit(128 + sig);
}

// CONCURRENT portfolio: run the slices in a bounded pool (SCHED_MAX_PAR at
// a time), EACH with the FULL wall -- not wall/N as the old sequential
// scheme did.  Sequential slicing starved every strategy (a problem
// needing >frac*wall of one config failed even though that config solves
// it within the full wall -- e.g. LCL113-10 needs 23s of the WM preset but
// the 0.22 slice gave it 6.6s).  First slice to exit 0 (proof found) wins;
// the rest are killed.  Each child writes its stdout to a temp file; only
// the winner's is replayed, so SZS/proof output never interleaves.
static int run_schedule(const char *self, const char *file, double wall,
                        int szs, int want_proof) {
  // Install the teardown handler + register the (initially empty) pid slots
  // so an external timeout can't orphan the slice subtrees.
  for (int i = 0; i < SCHED_PID_MAX; i++) g_sched_pids[i] = -1;
  struct sigaction sa, old_term, old_int;
  sa.sa_handler = sched_sig_teardown;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, &old_term);
  sigaction(SIGINT,  &sa, &old_int);
  g_sched_active = 1;
  // Bound each child's RSS unless the caller set a tighter cap already.
  if (getenv("THVM_ATP_RSS_ABORT_MB") == NULL) {
    char capbuf[16];
    snprintf(capbuf, sizeof capbuf, "%d", SCHED_SLICE_RSS_MB);
    setenv("THVM_ATP_RSS_ABORT_MB", capbuf, 1);
  }
  pid_t pids[N_SCHED];
  char  outs[N_SCHED][64];
  int   done[N_SCHED];
  for (int i = 0; i < N_SCHED; i++) { pids[i] = -1; done[i] = 0; outs[i][0] = 0; }
  int launched = 0, next = 0, winner = -1;
  // Launch up to SCHED_MAX_PAR, then refill as children exit.
  while (winner < 0 && (next < N_SCHED || launched > 0)) {
    while (winner < 0 && launched < SCHED_MAX_PAR && next < N_SCHED) {
      int i = next++;
      snprintf(outs[i], sizeof outs[i], "/tmp/thvm_sched_%d_%d.out",
               (int)getpid(), i);
      fprintf(stderr, "%% FindProof schedule slice %s (%.0fs, concurrent)\n",
              SCHEDULE[i].name, wall);
      pid_t pid = fork();
      if (pid == 0) {
        // Child: own process group so the parent can kill the WHOLE
        // subtree (this child + the shell system() forks + its FindProof
        // grandchild) with one killpg -- otherwise killing the child
        // blocked in system() would orphan the grandchild engine, which
        // keeps running to its wall and could overlap the next problem.
        setpgid(0, 0);
        // Redirect stdout to the slice temp file, run the slice.
        int fd = open(outs[i], O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) { dup2(fd, 1); close(fd); }
        char cmd[4096];
        snprintf(cmd, sizeof cmd, "FINDPROOF_SLICE=1 '%s' %s --wall=%.0f%s%s '%s'",
                 self, SCHEDULE[i].flags, wall,
                 szs ? " --szs" : "", want_proof ? " --proof" : "", file);
        _exit(system(cmd) == 0 ? 0 : 1);
      }
      // Set the child's pgid parent-side too (race-free: whichever of
      // parent/child wins, the group id equals the child pid).
      if (pid > 0) setpgid(pid, pid);
      pids[i] = pid;
      if (i < SCHED_PID_MAX) g_sched_pids[i] = pid;   // for the signal handler
      launched++;
    }
    if (launched == 0) break;
    int status = 0;
    pid_t w = wait(&status);
    if (w <= 0) break;
    for (int i = 0; i < N_SCHED; i++) {
      if (pids[i] == w) {
        done[i] = 1; launched--;
        if (i < SCHED_PID_MAX) g_sched_pids[i] = -1;   // reaped: clear
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) winner = i;
        break;
      }
    }
  }
  // Kill + reap any still-running slices (the winner made them redundant).
  // killpg on the child's group takes down the shell + FindProof grandchild
  // too, so no engine is left orphaned to run to its wall.
  for (int i = 0; i < N_SCHED; i++)
    if (pids[i] > 0 && !done[i]) { killpg(pids[i], SIGKILL); kill(pids[i], SIGKILL); }
  for (int i = 0; i < N_SCHED; i++)
    if (pids[i] > 0 && !done[i]) { int st; waitpid(pids[i], &st, 0);
      if (i < SCHED_PID_MAX) g_sched_pids[i] = -1; }
  // Teardown handler no longer needed: restore the previous dispositions.
  g_sched_active = 0;
  sigaction(SIGTERM, &old_term, NULL);
  sigaction(SIGINT,  &old_int,  NULL);
  int rc = 1;
  if (winner >= 0) {
    // Replay only the winning slice's captured output.
    FILE *fp = fopen(outs[winner], "r");
    if (fp != NULL) {
      char buf[4096]; size_t n;
      while ((n = fread(buf, 1, sizeof buf, fp)) > 0) fwrite(buf, 1, n, stdout);
      fclose(fp);
    }
    rc = 0;
  } else if (szs) {
    printf("%% SZS status GaveUp for %s\n", file);
  }
  for (int i = 0; i < N_SCHED; i++) if (outs[i][0]) unlink(outs[i]);
  return rc;
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
  int szs = 0, keep_pr = 0, want_schedule = 0, want_check = 0;
  double main_t0 = horn_now();

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
    if (nlen == 5 && strncmp(name, "check", 5) == 0) { want_check = 1; continue; }
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

  if (want_check) return proof_check_main(file, szs);

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
      pre = tp_unquote_names(pre);
      tp_shorten_names(pre);
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
      // Encoded-Horn (ifeq) problems: decode the dispatcher encoding
      // and forward-chain released units BEFORE SInE, so the releases
      // participate in axiom selection.  THVM_ATP_HORN=0 disables.
      {
        const char *he = getenv("THVM_ATP_HORN");
        g_cli_wall_sec = (atof(wall) > 0.0) ? atof(wall) : 120.0;
        if (he == NULL || he[0] != '0') tp_horn_release(&tp);
      }
      // Goal reached by the chaining itself: some released unit (a
      // logical consequence, by the modus-ponens-only invariant)
      // unifies with a negative clause, i.e. the axioms entail an
      // instance of the negated equation -- a refutation with no
      // engine run.  Decisive for problems the engine cannot even
      // load (PUZ039's state/12 exceeds the term reader's arity cap).
      // --proof runs stay on the engine path for the CNFRefutation.
      if (g_horn_goal_reached && szs && !g_cli_want_proof) {
        printf("%% SZS status Unsatisfiable for %s\n", file);
        fflush(stdout);
        return 0;
      }
      // Existential (non-ground) goals, opt-in: encode them
      // equationally the way Twee's addNarrowing does, BEFORE SInE
      // (the encoding's clauses carry the conjecture role and seed
      // the selection).
      {
        const char *ee = getenv("THVM_ATP_EQUALS_GOAL");
        if (ee != NULL && ee[0] != '0') tp_equals_goal(&tp);
        n_neg = 0;
        for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
      }
      // Wide-functor currying: decompose any n-ary functor (n > 8) into
      // nested binary ap2 applications so the arity-8 term reader can
      // load it (TOP052's tuple/140).  Sound bijective re-encoding; a
      // no-op unless a wide functor is present.  THVM_ATP_CURRY=0 keeps
      // the old clean give-up.
      {
        const char *ce = getenv("THVM_ATP_CURRY");
        if (ce == NULL || ce[0] != '0') tp_curry_wide(&tp);
      }
      // Oversized problems (the CSR knowledge bases run to 8k clauses):
      // goal-directed SInE selection cuts them to the relevant slice
      // BEFORE the capacity checks.  THVM_ATP_SINE=0 restores the old
      // clean give-up.
      {
        const char *se = getenv("THVM_ATP_SINE");
        u8 sine_on = (se == NULL || se[0] != '0');
        if (sine_on && (tp.n_cls - n_neg) > (int)PR_MAX_EQNS) {
          tp_sine_select(&tp);
          n_neg = 0;
          for (int i = 0; i < tp.n_cls; i++) n_neg += tp.cls[i].neg;
        }
      }
      // Never hand the .pr reader more than it can hold: an oversized
      // problem exits cleanly as GaveUp rather than aborting downstream
      // with no SZS line.  PR_MAX_* come from the included bench TU.
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

  // --equals-goal=2 (the schedule's equals slice): exit cleanly when
  // the encoding did not apply (ground goal, over-bound goal, or
  // non-TPTP input), freeing the pool slot immediately instead of
  // duplicating the WM lane for a whole wall.
  {
    const char *ee = getenv("THVM_ATP_EQUALS_GOAL");
    if (ee != NULL && atoi(ee) >= 2 && !g_cli_equals_applied)
      tp_giveup("equals-goal slice: no existential goal to encode");
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

  // The CD lane spends most of the wall chaining before the engine
  // even starts; hand the engine only what remains so the process
  // respects its overall budget.  Other flavors keep the full wall
  // (the 2.5s v1 chaining slice was always inside their slack).
  char wall_left[32];
  if (g_horn_cd) {
    double left = atof(wall) - (horn_now() - main_t0);
    if (left < 1.0) left = 1.0;
    snprintf(wall_left, sizeof wall_left, "%.1f", left);
    wall = wall_left;
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
            // Memory-guard aborts are ResourceOut in the SZS ontology
            // (Timeout is time only; the guard pops fire mid-wall).
            (g_atp_bench_status == (int)ATP_ABORTED)     ? "ResourceOut" :
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
