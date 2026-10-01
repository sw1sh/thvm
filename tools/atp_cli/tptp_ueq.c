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
// --proof run whose front-end already decided the problem exactly as the
// no-proof run does (Horn goal reached, clausification or the clausal
// pre-pass refuted it) and printed that status: the engine run that
// follows only reconstructs the proof.  Nothing after the decision may
// give up -- the status stands and the exit is 0 whatever the engine
// does.
static int g_cli_fe_decided = 0;
// Diagnostic verbosity.  Default OFF: a CASC/competition run must emit
// ONLY the SZS status line + solution block on stdout and nothing on
// stderr.  The engine bench TU writes its per-run diagnostics (PRSYM,
// the completion stats block, progress lines) to stderr; the schedule
// slice children inherit fd 2, so silencing the parent narration is not
// enough -- the children's stderr is sent to /dev/null too.  --verbose
// or THVM_ATP_VERBOSE=1 restores everything for development.
static int g_cli_verbose = 0;
static const char *g_cli_probpath = "";
// Defined with the provenance module / FOF front-end below; the Horn
// release pass (earlier in the file) gates on them.
static int g_prov_on;
static int g_cli_fof_applied;

// Runs (once) before a give-up takes effect; may exit(0) with a result.
// The FOF front-end arms it while its clausal pre-pass is deferred: an
// equational stage that cannot run hands the wall to the saturation.
static void (*g_tp_giveup_hook)(void) = NULL;

// The static gate replica (tp_problem_features) replays front-end stages
// that may give up; while it holds the trap, a give-up returns there with
// its reason instead of ending the process.
static jmp_buf *g_tp_giveup_trap = NULL;
static char     g_tp_giveup_trap_why[256];

static void tp_giveup(const char *why) {
  if (g_tp_giveup_trap != NULL) {
    snprintf(g_tp_giveup_trap_why, sizeof g_tp_giveup_trap_why, "%s", why);
    longjmp(*g_tp_giveup_trap, 1);
  }
  if (g_cli_fe_decided) {
    // The status is out; only the proof reconstruction gives up.
    fprintf(stderr, "FindProof: %s (proof not reconstructed)\n", why);
    fflush(stdout);
    exit(0);
  }
  if (g_tp_giveup_hook != NULL) {
    void (*h)(void) = g_tp_giveup_hook;
    g_tp_giveup_hook = NULL;
    fprintf(stderr, "FindProof: %s -- the clausal pre-pass takes the wall\n", why);
    h();
  }
  // Under a schedule slice the wrapper owns the single final status, so
  // stay silent on stdout and let the non-zero exit advance the slice.
  if (g_cli_szs_wanted && getenv("FINDPROOF_SLICE") == NULL)
    printf("%% SZS status GaveUp for %s\n", g_cli_probpath);
  fprintf(stderr, "FindProof: %s\n", why);
  exit(1);
}

// The give-up lines of the schedule lanes' exit-fast gates, shared by each
// gate and the static gate replica, which predicts them per lane.
#define TP_GATE_FOF     "fof slice: input has no fof() record"
#define TP_GATE_NO_GOAL "no negated conjecture (satisfiability problem; not refutable)"
#define TP_GATE_IFEQ    "ifeq-guard slice: no ifeq dispatcher axiom"
#define TP_GATE_DEF     "def-unfold slice: no function definition"
#define TP_GATE_TGT_AX  "tgt-ax slice: no axiom-side ground compound subterm"
#define TP_GATE_TGT_SEL "tgt-sel slice: conjecture symbols cover the signature"
#define TP_GATE_EQUALS  "equals-goal slice: no existential goal to encode"
#define TP_GATE_NARROWQ "narrow-queue slice: goal is ground"

// --- front-end wall ----------------------------------------------------
// The engine measures its wall from its own start; the stages before it
// (clausification, the one-element gate, the equational encoding, the
// clausal pre-pass, the Horn release, axiom selection, the transforms)
// run inside the same --wall and abstain once it is spent: a stage blind
// to it ran a schedule slice past the parent's status line (LCL641+1.010
// printed nothing for 90 s at --wall=30).  The deadline is process start
// + --wall on the wall clock (the hard stop; the budgets that steer the
// search read the work clock instead).  0 = none (the schedule parent,
// CheckProof).
static double g_cli_wall_sec = 120.0;  // engine wall, for the CD lane budget
static double g_cli_main_t0  = 0.0;    // process start (main); wall-remaining anchor
static double g_cli_fe_deadline = 0.0;

static double cli_wall_now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

static int cli_fe_late(void) {
  return g_cli_fe_deadline > 0.0 && cli_wall_now() >= g_cli_fe_deadline;
}

// Give up once the wall is spent, naming the stage that reached it; the
// deferred pre-pass does not take over (it has no wall left either).
static void cli_fe_check(const char *stage) {
  if (!cli_fe_late()) {
    return;
  }
  char why[160];
  snprintf(why, sizeof why, "%s reached the wall", stage);
  g_tp_giveup_hook = NULL;
  tp_giveup(why);
}

// The work clock of the front-end budgets that steer the search -- the
// Horn release budget and its fractions, the clausal pre-pass guard --
// in seconds: atp_work_peek_us (it leaves the engine's --proof windows
// unarmed), process CPU by default and the deterministic instruction
// count under --work-clock=instr, less the front end's own windows
// below, so a stage does the same work for the same budget whatever
// else shares the machine.  On the wall clock the Horn release kept ~55k
// units in one loaded GEO535+1 run and ~60k in another, and SInE then
// kept a different clause set.  The wall stays the hard stop.
static double g_cli_work_excl   = 0.0;
static double g_cli_work_pause0 = 0.0;

static double cli_work_now(void) {
  return (double)atp_work_peek_us() * 1e-6 - g_cli_work_excl;
}

// Exclusion window around one --proof bookkeeping record, open only on
// the instruction clock.  A window costs two clock reads; on the CPU
// clock each is a syscall (~1 us on the reference pods) against the
// ~0.1 us the record takes, so the window would cost the budget more
// than the record it keeps off it (GEO535+1 --proof: 129k reads, 63911
// units released against the no-proof run's 64135).  On the instruction
// clock the reads' kernel side is not counted and the window stays
// exact.  Windows nest.
static u32 g_cli_work_depth = 0u;

static void cli_work_pause(void) {
  if (atp_work_mode() == 1 && g_cli_work_depth++ == 0u) {
    g_cli_work_pause0 = (double)atp_work_peek_us() * 1e-6;
  }
}

static void cli_work_resume(void) {
  if (atp_work_mode() == 1 && g_cli_work_depth > 0u && --g_cli_work_depth == 0u) {
    g_cli_work_excl += (double)atp_work_peek_us() * 1e-6 - g_cli_work_pause0;
  }
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
  // Conversion-lineage provenance for --proof output (CASC-J13
  // solution compliance): which conversion produced this unit and the
  // index into the matching provenance table.  0/0 (PROV_NONE, the
  // calloc default) = parsed verbatim from the problem file -- the
  // emitter's file() citation stays byte-identical for that case.
  int  prov_kind;
  int  prov_ix;
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

// Provenance kinds (TpClause.prov_kind).  PROV_NONE keeps the legacy
// file() citation; the rest key the --proof emitter's conversion-chain
// records (see the "CASC-J13 solution-output provenance" section).
enum {
  PROV_NONE = 0,      // verbatim problem clause: file() leaf
  PROV_CNF,           // equality-encoding of CNF clause g_prov_cnf[ix]
  PROV_HELPER,        // encoding helper axiom: introduced(definition)
  PROV_UNFOLD,        // def-unfold rewrite: g_prov_unf[ix]
  PROV_EQGOAL,        // equals-goal reification: g_prov_eqg[ix]
  PROV_FUNNEL_GOAL,   // false != true funnel goal: parents = every
                      // negative clause's CNF record
  PROV_TRIVIAL_GOAL,  // clausification derived $false: ix = CNF clause
  PROV_HORN,          // Horn-chaining released unit: g_hpr[ix] carries
                      // the hyper-resolution derivation record
  PROV_SPLIT,         // fof-split branch clause: g_prov_split[ix] carries
                      // the avatar-shaped case-assumption records
};
// Capture entry points defined with the provenance module below; the
// def-unfold / equals-goal transforms (earlier in the file) call them.
static int  prov_def_new(const TpClause *cl);
static int  prov_unf_new(const TpClause *cl);
static void prov_unf_add_def(int unf, int def);
static int  prov_eqg_new(const TpClause *cl);

// Drop %-comment lines and splice include directives through
// tp_include_expand (selections honoured, nested includes followed).  A
// directive that does not expand is an input error.  A % comment that
// starts mid-line and a /* */ block are blanked last: the record scanner
// would read them as formula text.
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
    else if (tl > 8 && tp_is_include(t)) {
      const char *end = NULL;
      char why[1400];
      char *body = tp_include_expand(problem_path, t, 1, &end, why, sizeof why);
      if (body == NULL) {
        fprintf(stderr, "FindProof: %s\n", why);
        exit(2);
      }
      // drop the comment lines of the expansion too
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
      // A directive that spans lines ends on the line of its '.'.
      eol = strchr(end, '\n');
    } else {
      if (o + ll + 2 >= cap) { cap = cap * 2 + ll; out = (char *)realloc(out, cap); }
      memcpy(out + o, p, ll); o += ll; out[o++] = '\n';
    }
    if (eol == NULL) break;
    p = eol + 1;
  }
  out[o] = '\0';
  tp_blank_comments(out);
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

// Input renamings the proof must undo.  tp_unquote_names and
// tp_shorten_names rewrite identifiers the internal grammar cannot
// carry; a proof printed in the stand-in names cites formulas the
// problem never states (CSR038-10's hashed constant: a leaf no
// problem formula matches).  Each stand-in -> original pair is noted
// here and tp_unrename_write maps the stand-ins back on output.
typedef struct { char *from, *to; } TpRename;
static TpRename *g_tp_ren = NULL;
static u32 g_tp_ren_cap = 0;
static int g_tp_nren = 0;
static int g_tp_ren_on = 0;   // set for TPTP input (main): proof + model output

static int tp_ren_slot(const char *from, size_t n) {
  if (g_tp_ren_cap == 0u) return -1;
  u32 slot = (u32)(tp_name_hash(from, n) % (u64)g_tp_ren_cap);
  for (;;) {
    TpRename *e = &g_tp_ren[slot];
    if (e->from == NULL) return (int)slot;
    if (strlen(e->from) == n && memcmp(e->from, from, n) == 0) return (int)slot;
    slot = (slot + 1u) % g_tp_ren_cap;
  }
}

static void tp_ren_note(const char *from, size_t from_n, const char *to, size_t to_n) {
  if (!g_tp_ren_on) return;
  if ((u32)(g_tp_nren + 1) * 2u > g_tp_ren_cap) {
    TpRename *old = g_tp_ren;
    u32 ocap = g_tp_ren_cap;
    g_tp_ren_cap = (ocap == 0u) ? 1024u : 2u * ocap;
    g_tp_ren = (TpRename *)calloc(g_tp_ren_cap, sizeof(TpRename));
    if (g_tp_ren == NULL) tp_giveup("OOM in rename table");
    for (u32 i = 0; i < ocap; i++) {
      if (old[i].from == NULL) continue;
      g_tp_ren[tp_ren_slot(old[i].from, strlen(old[i].from))] = old[i];
    }
    free(old);
  }
  int slot = tp_ren_slot(from, from_n);
  if (g_tp_ren[slot].from != NULL) return;
  g_tp_ren[slot].from = strndup(from, from_n);
  g_tp_ren[slot].to = strndup(to, to_n);
  if (g_tp_ren[slot].from == NULL || g_tp_ren[slot].to == NULL)
    tp_giveup("OOM in rename table");
  g_tp_nren++;
}

// Write `text` with every stand-in identifier restored.  Quoted spans
// are copied verbatim (file paths, already-restored atoms).
static void tp_unrename_write(FILE *out, const char *text, size_t len) {
  if (g_tp_nren == 0) { fwrite(text, 1, len, out); return; }
  size_t i = 0;
  while (i < len) {
    char c = text[i];
    if (c == '\'') {
      size_t j = i + 1;
      while (j < len && text[j] != '\'') j += (text[j] == '\\' && j + 1 < len) ? 2 : 1;
      if (j < len) j++;
      fwrite(text + i, 1, j - i, out);
      i = j;
      continue;
    }
    if (isalnum((unsigned char)c) || c == '_') {
      size_t j = i;
      while (j < len && (isalnum((unsigned char)text[j]) || text[j] == '_')) j++;
      int slot = tp_ren_slot(text + i, j - i);
      if (slot >= 0 && g_tp_ren[slot].from != NULL) fputs(g_tp_ren[slot].to, out);
      else fwrite(text + i, 1, j - i, out);
      i = j;
      continue;
    }
    fputc(c, out);
    i++;
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
// A quoted lower_word ('abc') IS the atom abc in TPTP and simply loses
// its quotes.  Distinct objects ("text") and numbers outside the
// identifier grammar (-1, 2.5, 1/3) take fresh names from their own
// families (dob<n>, num<n>): each is a constant, and dropping their
// implicit pairwise-distinctness only weakens the theory.  One hash
// table, linear in the text -- the Coq-exported SEV sets carry 3000
// distinct quoted names, the BIO knowledge base 25000.
typedef struct { const char *key; u32 len; u32 ix; } TpQuoted;

static int tp_is_lower_word(const char *s, size_t n) {
  if (n == 0 || !islower((unsigned char)s[0])) return 0;
  for (size_t i = 1; i < n; i++)
    if (!(isalnum((unsigned char)s[i]) || s[i] == '_')) return 0;
  return 1;
}

// A TPTP number that is NOT already a plain identifier token: signed,
// decimal, rational or signed-exponent forms.  Returns its length at
// p (0 when p does not start one).
static size_t tp_number_len(const char *text, const char *p) {
  if (p > text && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) return 0;
  const char *q = p;
  int odd = 0;
  if (*q == '+' || *q == '-') { q++; odd = 1; }
  if (!isdigit((unsigned char)*q)) return 0;
  while (isdigit((unsigned char)*q)) q++;
  if (q[0] == '.' && isdigit((unsigned char)q[1])) {
    q++; odd = 1;
    while (isdigit((unsigned char)*q)) q++;
  }
  if ((q[0] == 'e' || q[0] == 'E') &&
      (isdigit((unsigned char)q[1]) ||
       ((q[1] == '+' || q[1] == '-') && isdigit((unsigned char)q[2])))) {
    q++;
    if (*q == '+' || *q == '-') { q++; odd = 1; }
    while (isdigit((unsigned char)*q)) q++;
  } else if (q[0] == '/' && isdigit((unsigned char)q[1])) {
    q++; odd = 1;
    while (isdigit((unsigned char)*q)) q++;
  }
  if (!odd || isalnum((unsigned char)*q) || *q == '_') return 0;
  return (size_t)(q - p);
}

// Grow `prefix` until no identifier of the text (quoted lower_words
// included, since they lose their quotes) starts with it: every
// <prefix><digits> name is then fresh.
static void tp_fresh_family(const char *text, char *prefix, size_t cap) {
  for (;;) {
    size_t pl = strlen(prefix);
    int clash = 0;
    for (const char *p = text; *p != '\0' && !clash; ) {
      if (*p == '\'' || *p == '"') {
        const char *e = tp_quote_end(p);
        if (e == NULL) { p++; continue; }
        size_t il = (size_t)(e - p) - 1u;
        if (*p == '\'' && tp_is_lower_word(p + 1, il) && il >= pl &&
            strncmp(p + 1, prefix, pl) == 0)
          clash = 1;
        p = e + 1;
        continue;
      }
      if (isalnum((unsigned char)*p) || *p == '_') {
        const char *e = p;
        while (isalnum((unsigned char)*e) || *e == '_') e++;
        if ((size_t)(e - p) >= pl && strncmp(p, prefix, pl) == 0) clash = 1;
        p = e;
        continue;
      }
      p++;
    }
    if (!clash) return;
    if (pl + 2 >= cap) tp_giveup("cannot make a fresh name family");
    prefix[pl] = 'x';
    prefix[pl + 1] = '\0';
  }
}

static char *tp_unquote_names(char *text) {
  int any = 0;
  for (const char *p = text; *p != '\0' && !any; p++) {
    if (*p == '\'' || *p == '"') any = 1;
    else if ((*p == '+' || *p == '-' || isdigit((unsigned char)*p)) &&
             tp_number_len(text, p) != 0) any = 1;
  }
  if (!any) return text;
  char fam[3][24] = { "qtd", "dob", "num" };
  for (int k = 0; k < 3; k++) tp_fresh_family(text, fam[k], sizeof fam[k]);
  u32 hcap = 1024u;
  u32 nent = 0u;
  u32 next_ix[3] = { 0u, 0u, 0u };
  TpQuoted *ht = (TpQuoted *)calloc(hcap, sizeof(TpQuoted));
  TpBuf out = {0};
  if (ht == NULL) tp_giveup("OOM in quoted-atom rename");
  tp_buf_put(&out, "", 0);
  const char *r = text, *seg = text;
  while (*r != '\0') {
    int kind = -1;
    size_t tl = 0;           // token length, quotes included
    if (*r == '\'' || *r == '"') {
      const char *e = tp_quote_end(r);
      if (e == NULL || e == r + 1) { r++; continue; }   // open or empty: as-is
      kind = (*r == '\'') ? 0 : 1;
      tl = (size_t)(e - r) + 1u;
      if (kind == 0 && tp_is_lower_word(r + 1, tl - 2u)) {
        tp_buf_put(&out, seg, (size_t)(r - seg));
        tp_buf_put(&out, r + 1, tl - 2u);
        r = e + 1;
        seg = r;
        continue;
      }
    } else if (*r == '+' || *r == '-' || isdigit((unsigned char)*r)) {
      tl = tp_number_len(text, r);
      if (tl != 0) kind = 2;
    }
    if (kind < 0) {
      if (isalnum((unsigned char)*r) || *r == '_') {
        while (isalnum((unsigned char)*r) || *r == '_') r++;   // skip a word
      } else {
        r++;
      }
      continue;
    }
    if (2u * (nent + 1u) > hcap) {
      u32 ncap = 2u * hcap;
      TpQuoted *nh = (TpQuoted *)calloc(ncap, sizeof(TpQuoted));
      if (nh == NULL) tp_giveup("OOM in quoted-atom rename");
      for (u32 i = 0; i < hcap; i++) {
        if (ht[i].key == NULL) continue;
        u32 sl = (u32)(tp_name_hash(ht[i].key, ht[i].len) % ncap);
        while (nh[sl].key != NULL) sl = (sl + 1u) % ncap;
        nh[sl] = ht[i];
      }
      free(ht);
      ht = nh;
      hcap = ncap;
    }
    u32 sl = (u32)(tp_name_hash(r, tl) % hcap);
    while (ht[sl].key != NULL &&
           !(ht[sl].len == tl && memcmp(ht[sl].key, r, tl) == 0))
      sl = (sl + 1u) % hcap;
    char nm[48];
    if (ht[sl].key == NULL) {
      ht[sl].key = r;
      ht[sl].len = (u32)tl;
      ht[sl].ix = next_ix[kind]++ | ((u32)kind << 30);
      nent++;
      snprintf(nm, sizeof nm, "%s%u", fam[kind], ht[sl].ix & 0x3FFFFFFFu);
      tp_ren_note(nm, strlen(nm), r, tl);
    } else {
      u32 ix = ht[sl].ix;
      snprintf(nm, sizeof nm, "%s%u", fam[ix >> 30], ix & 0x3FFFFFFFu);
    }
    tp_buf_put(&out, seg, (size_t)(r - seg));
    tp_buf_put(&out, nm, strlen(nm));
    r += tl;
    seg = r;
  }
  tp_buf_put(&out, seg, (size_t)(r - seg));
  free(ht);
  free(text);
  return out.s;
}

static void tp_shorten_names(char *text) {
  char *r = text, *w = text;
  while (*r != '\0') {
    if (isalnum((unsigned char)*r) || *r == '_') {
      size_t n = 0;
      while (isalnum((unsigned char)r[n]) || r[n] == '_') n++;
      if (n >= TP_NAME_LEN) {
        u64 h = tp_name_hash(r, n);
        char *orig = g_tp_ren_on ? strndup(r, n) : NULL;
        char *w0 = w;
        memmove(w, r, 40);
        w += 40;
        w += snprintf(w, 20, "_h%016llx", (unsigned long long)h);
        if (orig != NULL) {
          tp_ren_note(w0, (size_t)(w - w0), orig, n);
          free(orig);
        }
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

// Bounded substring test.  memmem is a GNU extension that the strict Linux
// feature macros in thvm.h leave undeclared: an error on gcc 14+ / clang 16+,
// and a pointer silently truncated to int on older compilers.
static int tp_span_has(const char *h, size_t hl, const char *n, size_t nl) {
  if (nl > hl) {
    return 0;
  }
  for (size_t i = 0; i + nl <= hl; i++) {
    if (memcmp(h + i, n, nl) == 0) {
      return 1;
    }
  }
  return 0;
}
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
      is_conj = tp_span_has(role, rl, "negated_conjecture", 18);
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
    cl->prov_kind = PROV_NONE;
    cl->prov_ix   = 0;
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
//              gen(s) <= gt  or  gen(s) <= tol * min_gen(C)
//              (s is near-least-general in C), where min_gen(C) runs
//              over the gt-GENERAL symbols of C only (gen > gt);
//   reach    = start from the negative units' symbols; a triggered
//              clause joins the selection and its symbols become
//              reached; iterate to fixpoint (or `rounds` levels).
// The generality threshold gt is Vampire's sine_generality_threshold
// (Shell/SineUtils.cpp): a near-fresh symbol (gen <= gt) neither
// defines the clause's min generality nor needs the tolerance test to
// trigger.  Without it a single fresh constant drags min_gen(C) to 1
// and no reachable symbol can ever trigger C: CSR058-10's one missing
// axiom holds the answer-string constant s_terrorist... (gen=1), so
// its best reached trigger (c_terroristgroup, gen=26) failed
// 26 <= tol*1 at every tolerance and the cone saturated goal-free.
// Raw Vampire semantics over-corrects, though: excluding the fresh
// constant from `mtvisible(c_fresh_mt) = true` hands min_gen to the
// HUB symbol mtvisible (gen 1746), which then self-triggers -- all
// 1746 facts join and the cone explodes 66 -> 5141 on CSR058.  The
// rare-cap rc keeps the exclusion exactly where it helps: the
// threshold-filtered min stands only if it is still RARE (<= rc);
// otherwise the plain min over all symbols governs, restoring the
// old untriggerable state for fresh-defined hub facts.  Swept offline
// against the dumped selection inputs: gt=1 rc=64 selects every
// oracle-proof axiom of CSR058 at cone 109 (was: 1 missing at 66) and
// of CSR036 at tol 1.5 (932), with the hub flood contained.
// The encoding glue (ifeq4/true, generality in the thousands) never
// triggers, exactly as SInE intends.  Negative units are always kept.
// Returns the kept count; compacts tp->cls in place.
static int tp_sine_pass(TpProblem *tp, double tol, int rounds, u32 gt,
                        u32 rc) {
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
      u32 min_all = 0xffffffffu, min_ex = 0xffffffffu;
      for (int k = 0; k < csym_n[c]; k++) {
        u32 g = gen[csym[c][k]];
        if (g < min_all) min_all = g;
        if (g > gt && g < min_ex) min_ex = g;
      }
      u32 mingen = (min_ex <= rc) ? min_ex : min_all;
      u8 hit = 0;
      for (int k = 0; k < csym_n[c] && !hit; k++) {
        int s = csym[c][k];
        if (!reach[s]) continue;
        if (gen[s] <= gt || (double)gen[s] <= tol * (double)mingen) hit = 1;
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

// Debug: THVM_ATP_SINE_DUMP=path writes the final cone to `path` and
// the selection's full input set to `path.in`, one clause per line.
static void tp_sine_dump(const TpProblem *tp, const char *path, int n) {
  FILE *df = fopen(path, "w");
  if (df == NULL) return;
  for (int i = 0; i < n; i++)
    fprintf(df, "%s %s %s\n", tp->cls[i].lhs,
            tp->cls[i].neg ? "!=" : (tp->cls[i].conj ? "=c" : "="),
            tp->cls[i].rhs);
  fclose(df);
}

// Driver: progressively tighten until the selection fits the engine
// caps -- TOLERANCE first at full depth, rounds only as a last resort.
// The CSR knowledge-base proofs ride 15+-trigger-deep chains (CSR036's
// genls towers, CSR040's tptpcol_k subsumption tower: the Vampire-4.9
// and E-3.1 oracle proofs use ~16 chain axioms each), so the old
// rounds={64,3,2,1} ladder amputated exactly the axioms the goal
// needs whenever the first pass overflowed the 256-equation cap.
// On success rebuilds the signature from the kept clauses (the
// dropped ones own most of the 10k-symbol table).  THVM_ATP_SINE=0
// disables; THVM_ATP_SINE_TOL overrides the first tolerance;
// THVM_ATP_SINE_GT overrides the generality threshold.
static void tp_sine_select(TpProblem *tp) {
  double tol0 = 1.5;
  {
    const char *e = getenv("THVM_ATP_SINE_TOL");
    if (e != NULL && e[0] != '\0') tol0 = atof(e);
    if (tol0 < 1.0) tol0 = 1.0;
  }
  u32 gt = 1;
  {
    const char *e = getenv("THVM_ATP_SINE_GT");
    if (e != NULL && e[0] != '\0') {
      long v = atol(e);
      gt = (v < 0) ? 0u : (u32)v;
    }
  }
  u32 rc = 64;
  {
    const char *e = getenv("THVM_ATP_SINE_RC");
    if (e != NULL && e[0] != '\0') {
      long v = atol(e);
      rc = (v < 0) ? 0u : (u32)v;
    }
  }
  const char *dump = getenv("THVM_ATP_SINE_DUMP");
  if (dump != NULL && dump[0] != '\0') {
    char inpath[1024];
    snprintf(inpath, sizeof inpath, "%s.in", dump);
    tp_sine_dump(tp, inpath, tp->n_cls);
  }
  const double tols[6]  = { tol0, 1.2, 1.0, 1.0, 1.0, 1.0 };
  const int    rnds[6]  = { 64,   64,  64,  3,   2,   1  };
  int n0 = tp->n_cls;
  for (int a = 0; a < 6; a++) {
    tp->n_cls = n0;   // each attempt filters the (previously kept) set
    int kept = tp_sine_pass(tp, tols[a], rnds[a], gt, rc);
    fprintf(stderr,
            "FindProof: SInE pass %d tol=%.2f rounds=%d gt=%u rc=%u "
            "kept=%d/%d\n", a, tols[a], rnds[a], gt, rc, kept, n0);
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
      if (dump != NULL && dump[0] != '\0') tp_sine_dump(tp, dump, kept);
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
  // Even the tightest ladder rung is over the cap (the AGT-class agent
  // KBs hold a goal cone wider than the engine's equation capacity:
  // 26 of 29 schedule lanes died HERE on AGT011+2, spending the whole
  // wall on nothing).  Truncate deterministically instead of dying:
  // negatives all stay, then the positive conjecture units and the
  // kept axioms in selection order up to the cap -- a conjecture that
  // clausifies to more hypothesis units than the cap (LCL646+1.010:
  // 356, every one conjecture-derived) keeps its first PR_MAX_EQNS.
  // A subset is sound for Theorem; saturation already reports GaveUp
  // under g_cli_sine_applied.
  {
    int n = tp->n_cls;
    int w = 0, n_ax = 0, ax_cap = (int)PR_MAX_EQNS;
    for (int pass = 0; pass < 2; pass++)
      for (int c = 0; c < n; c++) {
        int is_goal = tp->cls[c].neg || tp->cls[c].conj;
        if ((pass == 0) != (is_goal != 0)) continue;
        if (pass == 1 && n_ax >= ax_cap) continue;
        if (pass == 0 && !tp->cls[c].neg && ax_cap <= 0) continue;
        if (pass == 1) n_ax++;
        else if (!tp->cls[c].neg) ax_cap--;   // positive conjecture units
                                              // count against the eqn cap
        if (w != c) {
          TpClause tmp = tp->cls[w];
          tp->cls[w] = tp->cls[c];
          if (pass == 0) tp->cls[c] = tmp;   // pass 1 discards, no swap-back
        }
        w++;
      }
    int n_neg = 0;
    for (int i = 0; i < w; i++) n_neg += tp->cls[i].neg;
    if ((w - n_neg) > (int)PR_MAX_EQNS)
      tp_giveup("axiom selection cannot fit the engine caps");
    tp->n_cls = w;
    tp_syms_reset(tp);
    for (int i = 0; i < w; i++) {
      tp_scan_term(tp, tp->cls[i].lhs);
      tp_scan_term(tp, tp->cls[i].rhs);
    }
    fprintf(stderr, "FindProof: SInE over cap at every tolerance; "
            "truncated to %d/%d clauses\n", w, n);
    if (dump != NULL && dump[0] != '\0') tp_sine_dump(tp, dump, w);
    g_cli_sine_applied = 1;
  }
}


// --- tgt-ax lane prescreen -----------------------------------------
// Does any NON-conjecture positive unit carry a ground compound
// subterm (a function application whose span contains no variable
// token)?  This is exactly the target set of the tgt=4 axiom-side
// naming transform (Twee --flatten): the ground order-condition
// towers of the loop-theory family (GRP689-1's op_d^9 = unit).  A
// variable token is an identifier starting [A-Z] (TPTP CNF), so the
// string-level scan is exact for the UEQ grammar.  Conjecture-role
// units are excluded: their subterms are already in the tgt goal
// cone (modes 1/2), so they are not the lane's distinctive content.
static int tp_side_has_ground_compound(const char *s) {
  for (const char *p = s; *p != '\0'; p++) {
    if (*p != '(') continue;
    int depth = 1, ground = 1;
    for (const char *q = p + 1; *q != '\0' && depth > 0; q++) {
      if (*q == '(') depth++;
      else if (*q == ')') depth--;
      else if (isupper((unsigned char)*q)
               && !(isalnum((unsigned char)q[-1]) || q[-1] == '_'))
        ground = 0;
    }
    if (ground && depth == 0) return 1;
  }
  return 0;
}
static int tp_has_ax_ground_compound(const TpProblem *tp) {
  for (int c = 0; c < tp->n_cls; c++) {
    if (tp->cls[c].neg || tp->cls[c].conj) continue;
    if (tp_side_has_ground_compound(tp->cls[c].lhs) ||
        tp_side_has_ground_compound(tp->cls[c].rhs)) return 1;
  }
  return 0;
}

// --- tgt-sel lane prescreen ------------------------------------------
// The signature symbols occurring in a conjecture-role clause (a
// word-boundary match on the clause strings): their number, and their
// names, space-separated, into buf when buf is not NULL (room for every
// symbol name plus a separator).  The tgt-sel lane taxes the symbols
// outside this cone; when the cone is empty or covers the signature the
// scale is uniform and the lane duplicates bare.
static int tp_conj_syms(const TpProblem *tp, char *buf) {
  size_t len = 0;
  int n_hit = 0;
  if (buf != NULL) {
    buf[0] = '\0';
  }
  for (int i = 0; i < tp->n_syms; i++) {
    const char *nm = tp->syms[i].name;
    size_t nl = strlen(nm);
    int hit = 0;
    for (int c = 0; c < tp->n_cls && !hit; c++) {
      if (!tp->cls[c].conj) {
        continue;
      }
      for (int side = 0; side < 2 && !hit; side++) {
        const char *hay = side ? tp->cls[c].rhs : tp->cls[c].lhs;
        for (const char *p = strstr(hay, nm); p != NULL; p = strstr(p + 1, nm)) {
          int lb = (p == hay) ||
                   !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
          int rb = !(isalnum((unsigned char)p[nl]) || p[nl] == '_');
          if (lb && rb) {
            hit = 1;
            break;
          }
        }
      }
    }
    if (hit) {
      n_hit++;
      if (buf != NULL) {
        if (len > 0) {
          buf[len++] = ' ';
        }
        memcpy(buf + len, nm, nl);
        len += nl;
        buf[len] = '\0';
      }
    }
  }
  return n_hit;
}

static int tp_conj_cone_partial(int n_hit, int n_syms) {
  return n_hit > 0 && n_hit < n_syms;
}

// The engine's capacity gates, after axiom selection: an oversized
// problem gives up cleanly instead of aborting the .pr reader (PR_MAX_*
// come from the bench TU).  The flatterm reader holds at most 8
// arguments per node (TOP052's tuple/16 is curried before this).
static void tp_check_capacity(const TpProblem *tp, int n_neg) {
  if ((tp->n_cls - n_neg) > (int)PR_MAX_EQNS) {
    tp_giveup("problem exceeds equation capacity");
  }
  if (tp->n_syms > (int)PR_MAX_SYMS) {
    tp_giveup("problem exceeds symbol capacity");
  }
  if (tp->n_vars > (int)PR_MAX_VARS) {
    tp_giveup("problem exceeds variable capacity");
  }
  for (int i = 0; i < tp->n_syms; i++) {
    if (tp->syms[i].arity > 8) {
      tp_giveup("functor arity exceeds 8");
    }
  }
}
