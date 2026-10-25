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
// curried argument (f occurs at one arity only: tp_sym_note_ix exits on
// a clash, so the constant f names nothing else).  The encoding is
// injective on terms, but entailment survives it only where a model can
// host the partial applications ap2(..ap2(f, c1).., ck) as domain
// elements.  A model of the curried set is a model of the original
// (read f off the chains); the converse fails for a non-Horn set --
// X = a | X = b with four distinct binary residuals of f is satisfiable,
// its curried form is not (tests/atp/soundness/curry_wide*_dc.p).  This
// pass curries the table AFTER the unit encoding (tp_parse's unit
// equations, or tp_fof_load's ifeq / or-chain equations) with negative
// unit goals: a Horn set.  A satisfiable Horn set has a one-element
// model or, by direct products, an infinite one, which hosts every
// partial application injectively, so the engine's, the Horn release's
// and the backward chainer's refutations of the curried table refute
// the original, and a saturation of it (Satisfiable) is a model of the
// original.  The lanes that reason over non-Horn clauses -- the clausal
// pre-pass, the one-element gate and the fof-split detection -- read
// tp_fof_load's uncurried clauses (the deferred pre-pass runs after this
// pass, never on its output).  A no-op unless some functor exceeds 8, so
// it never perturbs the byte-exact WM emulation on existing corpora.
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

// The curried signature, kept for the proof output: every emitted
// record prints in the ORIGINAL signature (tp_uncurry_text), since a
// leaf in the ap2 encoding states a formula the problem never gives.
static char g_curry_ap2[TP_NAME_LEN];
static char (*g_curry_fn)[TP_NAME_LEN] = NULL;   // g_curry_nfn entries
static int  *g_curry_ar = NULL;
static int   g_curry_nfn = 0;

static void tp_curry_wide(TpProblem *tp) {
  int wide = 0;
  for (int i = 0; i < tp->n_syms; i++)
    if (tp->syms[i].arity > CURRY_ARITY_CAP) { wide = 1; break; }
  if (!wide) return;
  char ap2[TP_NAME_LEN];
  tp_fresh_sym(tp, "ap2", ap2, sizeof ap2);
  snprintf(g_curry_ap2, sizeof g_curry_ap2, "%s", ap2);
  g_curry_nfn = 0;
  int nwide = 0;
  for (int i = 0; i < tp->n_syms; i++)
    if (tp->syms[i].arity > CURRY_ARITY_CAP) nwide++;
  g_curry_fn = realloc(g_curry_fn, (size_t)nwide * sizeof *g_curry_fn);
  g_curry_ar = (int *)realloc(g_curry_ar, (size_t)nwide * sizeof(int));
  if (g_curry_fn == NULL || g_curry_ar == NULL) tp_giveup("OOM in currying");
  for (int i = 0; i < tp->n_syms; i++) {
    if (tp->syms[i].arity <= CURRY_ARITY_CAP) continue;
    snprintf(g_curry_fn[g_curry_nfn], TP_NAME_LEN, "%s", tp->syms[i].name);
    g_curry_ar[g_curry_nfn++] = tp->syms[i].arity;
  }
  // A curried side past the side buffer cannot be represented: such an
  // AXIOM-side unit drops (a subset refutation is still a refutation,
  // and the equational lanes never claim satisfiability); only an
  // unrepresentable goal ends the lane.
  int changed = 0, dropped = 0, w = 0;
  for (int c = 0; c < tp->n_cls; c++) {
    TpClause *cl = &tp->cls[c];
    char *sides[2] = { cl->lhs, cl->rhs };
    int keep = 1;
    for (int sd = 0; sd < 2 && keep; sd++) {
      const char *e;
      char *rw = curry_term(sides[sd], &e, ap2);
      if (strcmp(rw, sides[sd]) != 0) {
        if (strlen(rw) >= TP_SIDE_LEN) {
          if (cl->neg) { free(rw); tp_giveup("curried goal exceeds side buffer"); }
          keep = 0;
        } else {
          memcpy(sides[sd], rw, strlen(rw) + 1u);
          changed++;
        }
      }
      free(rw);
    }
    if (!keep) { dropped++; continue; }
    if (w != c) tp->cls[w] = tp->cls[c];
    w++;
  }
  tp->n_cls = w;
  tp_syms_reset(tp);
  for (int i = 0; i < tp->n_cls; i++) {
    tp_scan_term(tp, tp->cls[i].lhs);
    tp_scan_term(tp, tp->cls[i].rhs);
  }
  fprintf(stderr,
          "FindProof: curried %d wide-functor side(s) via %s (bijective n-ary decomposition)"
          "%s\n", changed, ap2, dropped ? ", oversized axiom unit(s) dropped" : "");
}


// --- output: back to the original signature -------------------------
// A parsed term: text of a complete term, or (for an ap2 chain still
// short of its head's arity) the head index + the collected args.
typedef struct { char *text; int head; int na; char **args; } CurryTm;

static void curry_tm_free(CurryTm *t) {
  free(t->text);
  for (int i = 0; i < t->na; i++) free(t->args[i]);
  free(t->args);
  memset(t, 0, sizeof *t);
}

// Render a (possibly partial) chain back as ap2 text.
static char *curry_tm_text(CurryTm *t) {
  if (t->text != NULL) { char *r = t->text; t->text = NULL; return r; }
  char *cur = strdup(g_curry_fn[t->head]);
  for (int i = 0; i < t->na; i++) {
    size_t l = strlen(g_curry_ap2) + strlen(cur) + strlen(t->args[i]) + 4;
    char *nx = (char *)malloc(l);
    snprintf(nx, l, "%s(%s,%s)", g_curry_ap2, cur, t->args[i]);
    free(cur);
    cur = nx;
  }
  return cur;
}

// Parse one term at s (identifier with optional argument list),
// rebuilding every complete ap2 chain as its n-ary application.
static CurryTm curry_uncurry_term(const char *s, const char **endp) {
  CurryTm out;
  memset(&out, 0, sizeof out);
  out.head = -1;
  while (*s == ' ') s++;
  const char *name = s;
  size_t n = 0;
  while (isalnum((unsigned char)s[n]) || s[n] == '_') n++;
  const char *p = s + n;
  while (*p == ' ') p++;
  if (*p != '(') {
    *endp = p;
    for (int i = 0; i < g_curry_nfn; i++) {
      if (strlen(g_curry_fn[i]) == n && memcmp(g_curry_fn[i], name, n) == 0) {
        out.head = i;
        return out;
      }
    }
    out.text = strndup(name, n);
    return out;
  }
  p++;
  CurryTm args[2];
  int na = 0;
  char **plain = NULL;
  int np = 0, cap = 0;
  int is_ap2 = (strlen(g_curry_ap2) == n && memcmp(g_curry_ap2, name, n) == 0);
  for (;;) {
    const char *e;
    CurryTm a = curry_uncurry_term(p, &e);
    if (is_ap2 && na < 2) {
      args[na++] = a;
    } else {
      if (np >= cap) { cap = cap ? 2 * cap : 8; plain = realloc(plain, (size_t)cap * sizeof *plain); }
      plain[np++] = curry_tm_text(&a);
      curry_tm_free(&a);
    }
    p = e;
    while (*p == ' ') p++;
    if (*p == ',') { p++; continue; }
    if (*p == ')') p++;
    break;
  }
  *endp = p;
  if (is_ap2 && na == 2 && np == 0 && args[0].text == NULL && args[0].head >= 0) {
    // Extend the chain; complete at the head's arity.
    out = args[0];
    out.args = realloc(out.args, (size_t)(out.na + 1) * sizeof *out.args);
    out.args[out.na++] = curry_tm_text(&args[1]);
    curry_tm_free(&args[1]);
    if (out.na == g_curry_ar[out.head]) {
      size_t l = strlen(g_curry_fn[out.head]) + 3;
      for (int i = 0; i < out.na; i++) l += strlen(out.args[i]) + 1;
      char *t = (char *)malloc(l);
      size_t o = (size_t)snprintf(t, l, "%s(", g_curry_fn[out.head]);
      for (int i = 0; i < out.na; i++)
        o += (size_t)snprintf(t + o, l - o, "%s%s", i ? "," : "", out.args[i]);
      snprintf(t + o, l - o, ")");
      for (int i = 0; i < out.na; i++) free(out.args[i]);
      free(out.args);
      out.args = NULL;
      out.na = 0;
      out.head = -1;
      out.text = t;
    }
    return out;
  }
  // Not a chain step: plain application text.
  size_t l = n + 3;
  char *kids[2 + 1];
  int nk = 0;
  for (int i = 0; i < na; i++) { kids[nk++] = curry_tm_text(&args[i]); curry_tm_free(&args[i]); }
  for (int i = 0; i < nk; i++) l += strlen(kids[i]) + 1;
  for (int i = 0; i < np; i++) l += strlen(plain[i]) + 1;
  char *t = (char *)malloc(l);
  size_t o = (size_t)snprintf(t, l, "%.*s(", (int)n, name);
  int first = 1;
  for (int i = 0; i < nk; i++) { o += (size_t)snprintf(t + o, l - o, "%s%s", first ? "" : ",", kids[i]); first = 0; free(kids[i]); }
  for (int i = 0; i < np; i++) { o += (size_t)snprintf(t + o, l - o, "%s%s", first ? "" : ",", plain[i]); first = 0; free(plain[i]); }
  snprintf(t + o, l - o, ")");
  free(plain);
  out.text = t;
  return out;
}

// The proof text with every ap2 chain printed as its original n-ary
// application; malloc'd.  Only terms starting at an ap2 occurrence are
// reparsed -- record names, roles and sources pass through verbatim.
static char *tp_uncurry_text(const char *text, size_t len, size_t *out_len) {
  char *buf = NULL;
  size_t blen = 0;
  FILE *ms = open_memstream(&buf, &blen);
  if (ms == NULL) return NULL;
  size_t an = strlen(g_curry_ap2);
  size_t i = 0;
  while (i < len) {
    char c = text[i];
    if (c == '\'') {
      size_t j = i + 1;
      while (j < len && text[j] != '\'') j++;
      if (j < len) j++;
      fwrite(text + i, 1, j - i, ms);
      i = j;
      continue;
    }
    if (isalnum((unsigned char)c) || c == '_') {
      size_t j = i;
      while (j < len && (isalnum((unsigned char)text[j]) || text[j] == '_')) j++;
      if (j - i == an && memcmp(text + i, g_curry_ap2, an) == 0 &&
          j < len && text[j] == '(') {
        const char *e;
        CurryTm t = curry_uncurry_term(text + i, &e);
        char *tt = curry_tm_text(&t);
        fputs(tt, ms);
        free(tt);
        curry_tm_free(&t);
        i = (size_t)(e - text);
        continue;
      }
      fwrite(text + i, 1, j - i, ms);
      i = j;
      continue;
    }
    fputc(c, ms);
    i++;
  }
  fclose(ms);
  *out_len = blen;
  return buf;
}
