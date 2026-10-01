// ===================================================================
// Function-definition unfolding (Vampire definition_unfolding port)
// ===================================================================
//
// A positive unit axiom  f(X1,...,Xn) = t  (either side) whose
// arguments are DISTINCT bare variables, with vars(t) inside
// {X1,...,Xn} and f absent from t, pins f's interpretation pointwise
// in every model: replacing each occurrence f(s1,...,sn) by
// t{Xi := si} in EVERY clause and dropping the axiom preserves
// satisfiability in both directions (Vampire's definition_unfolding
// preprocessing, applied by its default fde=all).  The COL
// combinator family is the motivating shape: COL002-5's
// weak_fixed_point(X) = S-C-B-term is such a definition, and KBO
// orients it big-term -> weak_fixed_point(X), so the folded goal
// side is IRREDUCIBLE and completion must wander presentation-
// dependent detours (1.5s or Timeout at 30s depending on nothing but
// the input clause order/orientation -- the canonical presentation
// LOSES it in every corpus release).  Unfolded, flatten-goal names
// the conjecture's ground subterms (= Vampire tgt=ground) and the
// oracle's 6-superposition derivation surfaces within the first few
// completion steps under ANY presentation (5 steps, 0.0s).
// THVM_ATP_DEF_UNFOLD: 1 = apply, 2 = apply and give up when no
// definition was unfolded (the schedule's exit-fast slice
// discipline, like --equals-goal=2).
#define TP_DU_MAX_ARGS 8
static int g_cli_defunfold_n = 0;

static size_t tp_du_ident_len(const char *p) {
  size_t n = 0;
  while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
  return n;
}

// Parse `side` as  f(V1,...,Vn)  with 1 <= n <= TP_DU_MAX_ARGS bare
// DISTINCT variable arguments; fill fname/args and return n, else -1.
static int tp_du_parse_head(const char *side, char *fname, size_t fcap,
                            char args[][TP_NAME_LEN]) {
  const char *p = side;
  while (isspace((unsigned char)*p)) p++;
  size_t flen = tp_du_ident_len(p);
  if (flen == 0u || flen >= fcap || isupper((unsigned char)*p)) return -1;
  memcpy(fname, p, flen);
  fname[flen] = '\0';
  p += flen;
  while (isspace((unsigned char)*p)) p++;
  if (*p != '(') return -1;
  p++;
  int n = 0;
  for (;;) {
    while (isspace((unsigned char)*p)) p++;
    size_t an = tp_du_ident_len(p);
    if (an == 0u || an >= TP_NAME_LEN || !isupper((unsigned char)*p) ||
        n >= TP_DU_MAX_ARGS)
      return -1;
    memcpy(args[n], p, an);
    args[n][an] = '\0';
    for (int i = 0; i < n; i++)
      if (strcmp(args[i], args[n]) == 0) return -1;    // repeated variable
    p += an;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '(') return -1;                          // compound argument
    n++;
    if (*p == ',') { p++; continue; }
    if (*p == ')') { p++; break; }
    return -1;
  }
  while (isspace((unsigned char)*p)) p++;
  return (*p == '\0' && n >= 1) ? n : -1;
}

// 1 iff every variable token of `t` is among args and the token
// `fname` never occurs in `t`.
static int tp_du_body_ok(const char *t, const char *fname,
                         char args[][TP_NAME_LEN], int nargs) {
  size_t fl = strlen(fname);
  for (const char *p = t; *p != '\0'; ) {
    if (!(isalnum((unsigned char)*p) || *p == '_')) { p++; continue; }
    size_t n = tp_du_ident_len(p);
    if (n == fl && memcmp(p, fname, fl) == 0) return 0;
    if (isupper((unsigned char)*p)) {
      const char *q = p + n;
      while (isspace((unsigned char)*q)) q++;
      if (*q != '(') {
        int known = 0;
        for (int i = 0; i < nargs && !known; i++)
          known = (strlen(args[i]) == n && memcmp(args[i], p, n) == 0);
        if (!known) return 0;
      }
    }
    p += n;
  }
  return 1;
}

// One left-to-right expansion pass: every occurrence of fname(...) in
// src is replaced by body{args := the occurrence's argument spans}
// (argument text copied verbatim, so occurrences nested inside the
// arguments survive into the output and need a further pass).
// Returns the number of occurrences expanded, or -1 on overflow /
// arity mismatch / malformed input.
static int tp_du_pass(const char *src, char *dst, size_t cap,
                      const char *fname, int nargs,
                      char args[][TP_NAME_LEN], const char *body) {
  size_t fl = strlen(fname), o = 0;
  int expanded = 0;
  const char *p = src;
  while (*p != '\0') {
    if (!(isalnum((unsigned char)*p) || *p == '_')) {
      if (o + 1 >= cap) return -1;
      dst[o++] = *p++;
      continue;
    }
    size_t n = tp_du_ident_len(p);
    const char *q = p + n;
    while (isspace((unsigned char)*q)) q++;
    if (!(n == fl && memcmp(p, fname, fl) == 0 && *q == '(')) {
      if (o + n >= cap) return -1;
      memcpy(dst + o, p, n);
      o += n;
      p += n;
      continue;
    }
    // f(...): collect the nargs top-level argument spans
    const char *as[TP_DU_MAX_ARGS];
    size_t      al[TP_DU_MAX_ARGS];
    const char *r = q + 1, *astart = q + 1;
    int depth = 1, ai = 0;
    while (*r != '\0' && depth > 0) {
      if (*r == '(') depth++;
      else if (*r == ')') { depth--; if (depth == 0) break; }
      else if (*r == ',' && depth == 1) {
        if (ai >= nargs) return -1;                    // arity mismatch
        as[ai] = astart;
        al[ai] = (size_t)(r - astart);
        ai++;
        astart = r + 1;
      }
      r++;
    }
    if (*r != ')' || ai >= nargs) return -1;
    as[ai] = astart;
    al[ai] = (size_t)(r - astart);
    ai++;
    if (ai != nargs) return -1;                        // arity mismatch
    // emit body with argument substitution
    for (const char *b = body; *b != '\0'; ) {
      if (!(isalnum((unsigned char)*b) || *b == '_')) {
        if (o + 1 >= cap) return -1;
        dst[o++] = *b++;
        continue;
      }
      size_t bn = tp_du_ident_len(b);
      int sub = -1;
      if (isupper((unsigned char)*b)) {
        const char *bq = b + bn;
        while (isspace((unsigned char)*bq)) bq++;
        if (*bq != '(')
          for (int i = 0; i < nargs && sub < 0; i++)
            if (strlen(args[i]) == bn && memcmp(args[i], b, bn) == 0) sub = i;
      }
      if (sub >= 0) {
        if (o + al[sub] >= cap) return -1;
        memcpy(dst + o, as[sub], al[sub]);
        o += al[sub];
      } else {
        if (o + bn >= cap) return -1;
        memcpy(dst + o, b, bn);
        o += bn;
      }
      b += bn;
    }
    expanded++;
    p = r + 1;
  }
  dst[o] = '\0';
  return expanded;
}

// Fully expand `side` into out (TP_SIDE_LEN); iterated passes because
// argument text is copied verbatim.  Each pass strictly decreases the
// occurrence count (f is absent from body), so this terminates.
// Returns 0 on failure (overflow / arity mismatch / runaway growth).
static int tp_du_expand_side(const char *side, char *out,
                             const char *fname, int nargs,
                             char args[][TP_NAME_LEN], const char *body) {
  static char bufa[8 * TP_SIDE_LEN], bufb[8 * TP_SIDE_LEN];
  snprintf(bufa, sizeof bufa, "%s", side);
  char *cur = bufa, *nxt = bufb;
  for (int guard = 0; ; guard++) {
    if (guard >= 64) return 0;
    int e = tp_du_pass(cur, nxt, 8 * TP_SIDE_LEN, fname, nargs, args, body);
    if (e < 0) return 0;
    char *t = cur; cur = nxt; nxt = t;
    if (e == 0) break;
  }
  if (strlen(cur) >= TP_SIDE_LEN) return 0;
  strcpy(out, cur);
  return 1;
}

// One round of the definition search: the first candidate definition in
// clause order whose symbol is not in skip[], dry-run against every other
// clause.  Returns the definition's clause index with fname, args, nargs
// and body filled; TP_DU_NONE when no candidate is left; TP_DU_SKIPPED
// when the candidate's dry run failed, its symbol then joins skip[].
enum { TP_DU_NONE = -1, TP_DU_SKIPPED = -2 };

static int tp_du_round(const TpProblem *tp, char skip[][TP_NAME_LEN],
                       int *n_skip, char *fname,
                       char args[][TP_NAME_LEN], int *nargs,
                       const char **body) {
  int def_ix = -1;
  for (int i = 0; i < tp->n_cls && def_ix < 0; i++) {
    const TpClause *cl = &tp->cls[i];
    if (cl->neg || cl->conj) {
      continue;
    }
    for (int side = 0; side < 2 && def_ix < 0; side++) {
      const char *h = side ? cl->rhs : cl->lhs;
      const char *t = side ? cl->lhs : cl->rhs;
      int na = tp_du_parse_head(h, fname, TP_NAME_LEN, args);
      if (na < 1) {
        continue;
      }
      if (!tp_du_body_ok(t, fname, args, na)) {
        continue;
      }
      int skipped = 0;
      for (int k = 0; k < *n_skip && !skipped; k++) {
        skipped = (strcmp(skip[k], fname) == 0);
      }
      if (skipped) {
        continue;
      }
      *nargs = na;
      def_ix = i;
      *body = t;
    }
  }
  if (def_ix < 0) {
    return TP_DU_NONE;
  }
  // Dry-run: every side of every other clause must expand within the
  // side buffer -- unfolding is all-or-nothing per symbol (a partial
  // unfold that dropped the axiom would be unsound).
  char tmp[TP_SIDE_LEN];
  int ok = 1;
  for (int i = 0; i < tp->n_cls && ok; i++) {
    if (i == def_ix) {
      continue;
    }
    ok = tp_du_expand_side(tp->cls[i].lhs, tmp, fname, *nargs, args, *body) &&
         tp_du_expand_side(tp->cls[i].rhs, tmp, fname, *nargs, args, *body);
  }
  if (!ok) {
    if (*n_skip < 64) {
      snprintf(skip[(*n_skip)++], TP_NAME_LEN, "%s", fname);
    }
    return TP_DU_SKIPPED;
  }
  return def_ix;
}

static void tp_def_unfold(TpProblem *tp) {
  char skip[64][TP_NAME_LEN];
  int  n_skip = 0;
  for (int round = 0; round < 64; round++) {
    char fname[TP_NAME_LEN], args[TP_DU_MAX_ARGS][TP_NAME_LEN];
    int  nargs = -1;
    const char *body = NULL;
    int def_ix = tp_du_round(tp, skip, &n_skip, fname, args, &nargs, &body);
    if (def_ix == TP_DU_NONE) {
      break;
    }
    if (def_ix == TP_DU_SKIPPED) {
      continue;
    }
    // Commit: rewrite every clause, then drop the definition axiom.
    // Under --proof, keep the eliminated definition and every
    // clause's pre-unfold text so the emitter can cite
    // inference(definition_unfolding, ..., [u<k>, d<j>..]).
    int def_prov = g_prov_on ? prov_def_new(&tp->cls[def_ix]) : -1;
    for (int i = 0; i < tp->n_cls; i++) {
      if (i == def_ix) continue;
      char nl[TP_SIDE_LEN], nr[TP_SIDE_LEN];
      tp_du_expand_side(tp->cls[i].lhs, nl, fname, nargs, args, body);
      tp_du_expand_side(tp->cls[i].rhs, nr, fname, nargs, args, body);
      if (def_prov >= 0 && (strcmp(nl, tp->cls[i].lhs) != 0 ||
                            strcmp(nr, tp->cls[i].rhs) != 0)) {
        TpClause *cl = &tp->cls[i];
        if (cl->prov_kind != PROV_UNFOLD) {
          int u = prov_unf_new(cl);
          if (u >= 0) { cl->prov_kind = PROV_UNFOLD; cl->prov_ix = u; }
        }
        if (cl->prov_kind == PROV_UNFOLD)
          prov_unf_add_def(cl->prov_ix, def_prov);
      }
      strcpy(tp->cls[i].lhs, nl);
      strcpy(tp->cls[i].rhs, nr);
    }
    for (int i = def_ix + 1; i < tp->n_cls; i++) tp->cls[i - 1] = tp->cls[i];
    tp->n_cls--;
    g_cli_defunfold_n++;
  }
  if (g_cli_defunfold_n > 0) {
    tp_syms_reset(tp);
    for (int i = 0; i < tp->n_cls; i++) {
      tp_scan_term(tp, tp->cls[i].lhs);
      tp_scan_term(tp, tp->cls[i].rhs);
    }
    fprintf(stderr, "%% def-unfold: %d function definition(s) eliminated\n",
            g_cli_defunfold_n);
  }
}

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

// Does the equals encoding apply?  A non-ground goal, room for the two
// clauses it adds, and every encoded goal inside the side bound; else the
// narrowing lane keeps the goal.  Fills the fresh eq / true / false names
// (TP_NAME_LEN each) and the number of non-ground goals.  The
// --equals-goal=2 gate.
static int tp_equals_goal_applies(const TpProblem *tp, char *eqn, char *tru,
                                  char *fls, int *n_nonground) {
  *n_nonground = 0;
  for (int i = 0; i < tp->n_cls; i++) {
    if (tp->cls[i].neg && (tp_side_has_var(tp->cls[i].lhs) ||
                           tp_side_has_var(tp->cls[i].rhs))) {
      (*n_nonground)++;
    }
  }
  if (*n_nonground == 0) {
    return 0;
  }
  if (tp->n_cls + 2 > TP_MAX_CLAUSES) {
    return 0;
  }
  tp_fresh_sym(tp, "eq",     eqn, TP_NAME_LEN);
  tp_fresh_sym(tp, "etrue",  tru, TP_NAME_LEN);
  tp_fresh_sym(tp, "efalse", fls, TP_NAME_LEN);
  for (int i = 0; i < tp->n_cls; i++) {
    const TpClause *cl = &tp->cls[i];
    if (!cl->neg || (!tp_side_has_var(cl->lhs) && !tp_side_has_var(cl->rhs))) {
      continue;
    }
    if (strlen(eqn) + strlen(cl->lhs) + strlen(cl->rhs) + 4 >= TP_SIDE_LEN) {
      return 0;
    }
  }
  return 1;
}

static void tp_equals_goal(TpProblem *tp) {
  char eqn[TP_NAME_LEN], tru[TP_NAME_LEN], fls[TP_NAME_LEN];
  int n_nonground = 0;
  if (!tp_equals_goal_applies(tp, eqn, tru, fls, &n_nonground)) {
    return;
  }
  // Rewrite each non-ground goal in place:  eq(s,t) = false.
  for (int i = 0; i < tp->n_cls; i++) {
    TpClause *cl = &tp->cls[i];
    if (!cl->neg || (!tp_side_has_var(cl->lhs) && !tp_side_has_var(cl->rhs)))
      continue;
    char enc[TP_SIDE_LEN];
    snprintf(enc, sizeof enc, "%s(%s,%s)", eqn, cl->lhs, cl->rhs);
    // --proof: the reified unit cites the ORIGINAL negative clause
    // through an equality_encoding node.  A PROV_CNF clause keeps its
    // provenance (the reification is still an encoding of that same
    // CNF clause).
    if (g_prov_on && cl->prov_kind != PROV_CNF) {
      int e = prov_eqg_new(cl);
      if (e >= 0) { cl->prov_kind = PROV_EQGOAL; cl->prov_ix = e; }
    }
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
  rfl->prov_kind = g_prov_on ? PROV_HELPER : PROV_NONE;
  rfl->prov_ix   = 0;
  tp_scan_term(tp, rfl->lhs);
  tp_scan_term(tp, rfl->rhs);
  // The replacement goal:  true != false.
  TpClause *gl = &tp->cls[tp->n_cls++];
  snprintf(gl->lhs, sizeof gl->lhs, "%s", tru);
  snprintf(gl->rhs, sizeof gl->rhs, "%s", fls);
  gl->neg  = 1;
  gl->conj = 1;
  gl->prov_kind = g_prov_on ? PROV_HELPER : PROV_NONE;
  gl->prov_ix   = 0;
  tp_scan_term(tp, gl->lhs);
  tp_scan_term(tp, gl->rhs);
  g_cli_equals_applied = 1;
  fprintf(stderr,
          "FindProof: equals-encoded %d existential goal(s) (%s/%s/%s)\n",
          n_nonground, eqn, tru, fls);
}

