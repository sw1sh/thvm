// ===================================================================
// Canonical input presentation (orientation + order + variable names)
// ===================================================================
//
// CASC deliberately varies how the SAME theory is presented: corpus
// releases permute clause order and flip equation sides (the TPTP 8.2
// vs 9.x MVA001-0.ax differ ONLY in those two ways, and the encompass
// config proves MVA003-1 under one presentation while timing out
// under the other; a controlled morph pinned clause ORDER as the
// discriminator).  Normalize the presentation at ingestion so the
// outcome cannot depend on it:
//   orientation  the GREATER side goes LEFT; greater = more symbol
//                occurrences (term size), ties broken toward the
//                lexicographically greater canonical clause text
//                (both candidate orientations compared under
//                clause-level first-occurrence variable renaming);
//   names        variables renamed <pfx>1, <pfx>2, ... by first
//                occurrence within their clause (whitespace dropped);
//   order        positive clauses stable-sorted by (clause size,
//                canonical text); negative units keep their slots and
//                relative order (tp_emit_pr already lists them last,
//                under CONCLUSION).
// Runs AFTER the content transforms (horn/equals/curry/SInE) and the
// capacity gates, immediately before .pr emission; the .pr bench path
// never sees it.  THVM_ATP_CANON_INPUT=0 disables (kill switch).

#define TP_CANON_MAX_CVARS 256

// Term size = number of identifier tokens (functors, constants,
// variables), the KBO weight under the emitted all-1 weight table.
static int tp_side_weight(const char *s) {
  int w = 0;
  for (const char *p = s; *p != '\0'; ) {
    if (isalnum((unsigned char)*p) || *p == '_') {
      w++;
      while (isalnum((unsigned char)*p) || *p == '_') p++;
    } else {
      p++;
    }
  }
  return w;
}

// Rewrite side `src` into `dst`, dropping whitespace and renaming each
// variable to pfx + (1 + clause-level first-occurrence index).  `map`
// carries the original names seen so far in this clause (shared across
// both sides).  pfx == NULL keeps original variable names (the
// no-safe-prefix fallback).  Returns 0 on overflow.
static int tp_canon_rewrite(const char *src, char *dst, size_t cap,
                            char (*map)[TP_NAME_LEN], int *n_map,
                            const char *pfx) {
  size_t o = 0;
  for (const char *p = src; *p != '\0'; ) {
    unsigned char c = (unsigned char)*p;
    if (isspace(c)) { p++; continue; }
    if (!(isalnum(c) || c == '_')) {
      if (o + 1 >= cap) return 0;
      dst[o++] = *p++;
      continue;
    }
    size_t n = 0;
    while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
    const char *q = p + n;
    while (isspace((unsigned char)*q)) q++;
    if (*q == '(' || !isupper(c) || pfx == NULL) {
      // functor / constant (or fallback): verbatim
      if (o + n >= cap) return 0;
      memcpy(dst + o, p, n);
      o += n;
    } else {
      int ix = -1;
      for (int i = 0; i < *n_map; i++)
        if (strlen(map[i]) == n && memcmp(map[i], p, n) == 0) { ix = i; break; }
      if (ix < 0) {
        if (*n_map >= TP_CANON_MAX_CVARS || n >= TP_NAME_LEN) return 0;
        memcpy(map[*n_map], p, n);
        map[*n_map][n] = '\0';
        ix = (*n_map)++;
      }
      char tok[TP_NAME_LEN + 16];
      int tl = snprintf(tok, sizeof tok, "%s%d", pfx, ix + 1);
      if (o + (size_t)tl >= cap) return 0;
      memcpy(dst + o, tok, (size_t)tl);
      o += (size_t)tl;
    }
    p += n;
  }
  dst[o] = '\0';
  return 1;
}

// A printable variable prefix that cannot collide with the signature:
// no symbol may read as prefix + digits.  (An uppercase-initial name
// only enters the symbol table as a FUNCTOR -- tp_scan_term classifies
// uppercase non-functors as variables -- so collisions are near
// impossible; the scan is a guard.)
static const char *tp_canon_var_prefix(const TpProblem *tp) {
  static const char *cands[] = {"X", "Y", "Z", "V", "W", "U"};
  for (size_t c = 0; c < sizeof cands / sizeof cands[0]; c++) {
    size_t pl = strlen(cands[c]);
    int clash = 0;
    for (int i = 0; i < tp->n_syms && !clash; i++) {
      const char *nm = tp->syms[i].name;
      if (strncmp(nm, cands[c], pl) != 0 || nm[pl] == '\0') continue;
      clash = 1;
      for (const char *q = nm + pl; *q != '\0'; q++)
        if (!isdigit((unsigned char)*q)) { clash = 0; break; }
    }
    if (!clash) return cands[c];
  }
  return NULL;
}

// Canonicalize one clause in place: pick the orientation (greater side
// left), then rewrite both sides with clause-level variable renaming.
static void tp_canon_clause(TpClause *cl, const char *pfx) {
  static char ka[4 * TP_SIDE_LEN], kb[4 * TP_SIDE_LEN];
  char map[TP_CANON_MAX_CVARS][TP_NAME_LEN];
  int wl = tp_side_weight(cl->lhs), wr = tp_side_weight(cl->rhs);
  int flip = 0;
  if (wl < wr) flip = 1;
  else if (wl == wr) {
    // weight tie: compare the two candidate orientations as full
    // canonical clause texts ("lhs=rhs", variables renamed by clause
    // first occurrence to \x01<i> -- a token no TPTP identifier can
    // contain).  Per-side comparison is NOT enough: for sides with
    // identical skeletons (f(X,Y,Z) = f(Y,Z,X)) only the clause-level
    // renaming distinguishes the orientations consistently across
    // presentations.
    int nm = 0;
    size_t half = sizeof ka / 2;
    if (tp_canon_rewrite(cl->lhs, ka, half, map, &nm, "\x01")) {
      size_t la = strlen(ka);
      ka[la++] = '=';
      if (tp_canon_rewrite(cl->rhs, ka + la, sizeof ka - la, map, &nm, "\x01")) {
        nm = 0;
        if (tp_canon_rewrite(cl->rhs, kb, half, map, &nm, "\x01")) {
          size_t lb = strlen(kb);
          kb[lb++] = '=';
          if (tp_canon_rewrite(cl->lhs, kb + lb, sizeof kb - lb, map, &nm, "\x01"))
            flip = (strcmp(ka, kb) < 0);
        }
      }
    }
  }
  int nm = 0;
  const char *nl_src = flip ? cl->rhs : cl->lhs;
  const char *nr_src = flip ? cl->lhs : cl->rhs;
  if (!tp_canon_rewrite(nl_src, ka, sizeof ka, map, &nm, pfx) ||
      !tp_canon_rewrite(nr_src, kb, sizeof kb, map, &nm, pfx))
    tp_giveup("canonical clause exceeds rewrite bound");
  if (strlen(ka) >= TP_SIDE_LEN || strlen(kb) >= TP_SIDE_LEN)
    tp_giveup("canonical term exceeds side-length bound");
  memcpy(cl->lhs, ka, strlen(ka) + 1u);
  memcpy(cl->rhs, kb, strlen(kb) + 1u);
}

typedef struct { int w; int ix; } TpCanonKey;
static TpProblem *g_canon_tp = NULL;
static int tp_canon_cmp(const void *a, const void *b) {
  const TpCanonKey *x = (const TpCanonKey *)a, *y = (const TpCanonKey *)b;
  if (x->w != y->w) return (x->w < y->w) ? -1 : 1;
  const TpClause *cx = &g_canon_tp->cls[x->ix], *cy = &g_canon_tp->cls[y->ix];
  int c = strcmp(cx->lhs, cy->lhs);
  if (c != 0) return c;
  c = strcmp(cx->rhs, cy->rhs);
  if (c != 0) return c;
  // original position: a total tie-break makes qsort's ordering
  // deterministic (stable) regardless of libc pivot choices
  return (x->ix < y->ix) ? -1 : (x->ix > y->ix);
}

static void tp_canon_input(TpProblem *tp) {
  const char *pfx = tp_canon_var_prefix(tp);
  for (int i = 0; i < tp->n_cls; i++) tp_canon_clause(&tp->cls[i], pfx);
  int n_pos = 0;
  int      *pos  = (int *)malloc(sizeof(int) * (size_t)(tp->n_cls + 1));
  TpCanonKey *keys = (TpCanonKey *)malloc(sizeof(TpCanonKey) * (size_t)(tp->n_cls + 1));
  if (pos == NULL || keys == NULL) tp_giveup("OOM in canonical ordering");
  for (int i = 0; i < tp->n_cls; i++) {
    if (tp->cls[i].neg) continue;
    pos[n_pos] = i;
    keys[n_pos].ix = i;
    keys[n_pos].w  = tp_side_weight(tp->cls[i].lhs) + tp_side_weight(tp->cls[i].rhs);
    n_pos++;
  }
  g_canon_tp = tp;
  qsort(keys, (size_t)n_pos, sizeof(TpCanonKey), tp_canon_cmp);
  TpClause *tmp = (TpClause *)malloc(sizeof(TpClause) * (size_t)(n_pos + 1));
  if (tmp == NULL) tp_giveup("OOM in canonical ordering");
  for (int k = 0; k < n_pos; k++) tmp[k] = tp->cls[keys[k].ix];
  for (int k = 0; k < n_pos; k++) tp->cls[pos[k]] = tmp[k];
  free(tmp); free(keys); free(pos);
  // Rebuild signature + variable list from the canonical text in
  // canonical order (the VARIABLES line follows first-seen order).
  tp_syms_reset(tp);
  for (int i = 0; i < tp->n_cls; i++) {
    tp_scan_term(tp, tp->cls[i].lhs);
    tp_scan_term(tp, tp->cls[i].rhs);
  }
}

