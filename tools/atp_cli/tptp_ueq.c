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

static void tp_giveup(const char *why) {
  if (g_cli_fe_decided) {
    // The status is out; only the proof reconstruction gives up.
    fprintf(stderr, "FindProof: %s (proof not reconstructed)\n", why);
    fflush(stdout);
    exit(0);
  }
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
static int g_tp_ren_on = 0;   // --proof runs only: the default path never notes

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
      tp_ren_note(qrepl[nq], strlen(qrepl[nq]), r, (size_t)(q2 - r) + 1u);
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
  // negatives + conjecture units all stay, then the kept axioms in
  // selection order up to the cap.  A subset is sound for Theorem;
  // saturation already reports GaveUp under g_cli_sine_applied.
  {
    int n = tp->n_cls;
    int w = 0, n_ax = 0, ax_cap = (int)PR_MAX_EQNS;
    for (int pass = 0; pass < 2; pass++)
      for (int c = 0; c < n; c++) {
        int is_goal = tp->cls[c].neg || tp->cls[c].conj;
        if ((pass == 0) != (is_goal != 0)) continue;
        if (pass == 1 && n_ax >= ax_cap) continue;
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
