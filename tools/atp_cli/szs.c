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
  // Twee --max-term-size: a per-side hard CP drop, lossy exactly like the
  // -mw sum cap, so a saturation under it certifies nothing (GaveUp, never
  // Satisfiable) -- the dropped CPs could have refuted.
  const char *mts = getenv("THVM_ATP_MAX_TERM_SIZE");
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
  // Pop-time ground-joinability case-split: the discard is a redundancy
  // claim (ground-joinable CPs are redundant), completeness-preserving
  // in theory -- but the implementation is young, and an over-discard
  // would make a saturation certify nothing.  Conservative: GaveUp,
  // never Satisfiable, until the mechanism has corpus history.
  const char *pgj = getenv("THVM_ATP_POP_GJ");
  // Without unfailing CP formation (use_unfailing_cp, armed by the WM
  // preset; THVM_ATP_UNFAILING overrides either way) an unorientable
  // equation contributes NO ordered overlaps, so a QUEUE_EMPTY that
  // leaves one in the final rule store certifies nothing -- the missing
  // overlaps could refute.  COL002-5's S-combinator axiom is inherently
  // KBO-unorientable (the unfolding direction duplicates a variable,
  // the folding direction loses the weight-tie lexicographic compare)
  // and the bare tgt slice "saturated" in 8 steps / 0 CPs on the
  // unsatisfiable problem, claiming Satisfiable.
  const char *wmp = getenv("THVM_ATP_WALDMEISTER");
  const char *uf  = getenv("THVM_ATP_UNFAILING");
  u8 unfailing = (uf != NULL && uf[0] != '\0')
                   ? (uf[0] != '0')
                   : (wmp != NULL && wmp[0] != '\0' && wmp[0] != '0');
  if (!unfailing && g_atp_bench_unorient > 0) return 0;
  return !g_cli_goal_existential && !g_cli_sine_applied && !g_cli_horn_applied
      && !g_cli_equals_applied && !g_cli_fof_applied
      && (mw == NULL || *mw == '0') && (am == NULL || *am == '0')
      && (mts == NULL || *mts == '0')
      && (rr == NULL || *rr == '0') && (en == NULL || *en == '0')
      && (sj == NULL || *sj == '0') && (pgj == NULL || *pgj == '0');
}

// --- term printing (names from the parsed .pr) ---------------------

// Collision-free name prefix for variable ids past the .pr's declared
// set.  Derived equations regularly hold MORE distinct variables than
// the input declares, and the old fallback ("X%u") collided with the
// declared X1..Xn names: id n printed "Xn" -- the DECLARED name of id
// n-1 -- so two distinct variables emitted as one, the record claimed
// a strictly stronger equation, and every later step citing it failed
// syntactic replay (GRP706-1's 23-abstain class; the checker's deep
// sessions were re-deriving what the citation would have settled).
// Scan the declared names once per problem and pick the first prefix
// no declared "<prefix><digits>" name uses.
static const char *cli_var_prefix(const PrProblem *pr) {
  static const PrProblem *cached_pr = NULL;
  static const char *cached_prefix = "X";
  if (pr == cached_pr) return cached_prefix;
  static const char *cands[] = { "X", "Y", "Z", "U", "V", "W", "XV" };
  cached_pr = pr;
  cached_prefix = "XV";
  for (u32 c = 0; c < sizeof cands / sizeof cands[0]; c++) {
    u8 clash = 0u;
    size_t plen = strlen(cands[c]);
    for (u32 v = 0; pr != NULL && v < pr->n_vars && !clash; v++) {
      const char *nm = pr->vars[v];
      if (strncmp(nm, cands[c], plen) != 0) continue;
      const char *d = nm + plen;
      if (*d == '\0') continue;
      while (*d >= '0' && *d <= '9') d++;
      if (*d == '\0') clash = 1u;        // declared "<prefix><digits>"
    }
    if (!clash) { cached_prefix = cands[c]; break; }
  }
  return cached_prefix;
}

// Does the term mention a definitional fresh symbol (pr_flatten_goal's
// sFlat* constants or pr_tgt_transform's sF* functions)?  Such an
// axiom leaf is an introduced definition, not a problem clause, and
// must be sourced as introduced(definition) so a derivation verifier
// (GDV) does not try to match it against the problem's formulae.
// Decided by the recorded fresh-label RANGE, immune to user symbols
// that happen to spell sF*/sFlat*.
static int cli_term_has_flat_sym(const PrProblem *pr, Term t) {
  if (t == 0 || term_tag(t) != TAG_CTR) return 0;
  u32 l = term_ext(t);
  if (pr != NULL && pr->n_fresh_syms > 0u && l >= PR_LABEL_BASE) {
    u32 si = l - PR_LABEL_BASE;
    if (si >= pr->fresh_sym_base &&
        si < pr->fresh_sym_base + pr->n_fresh_syms) return 1;
  }
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (cli_term_has_flat_sym(pr, term_ctr_at(t, i))) return 1;
  return 0;
}

static void cli_print_term(FILE *fp, const PrProblem *pr, Term t) {
  if (t == 0) { fputs("_", fp); return; }
  u32 tag = term_tag(t);
  if (tag == TAG_FVR) {
    u32 v = term_ext(t);
    if (pr != NULL && v < pr->n_vars) fputs(pr->vars[v], fp);
    else fprintf(fp, "%s%u", cli_var_prefix(pr), v);
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

// Buffer variant of cli_print_term: the provenance leaf-match key.
// Overflow sets *fail (the key then matches nothing and the leaf
// falls back to the legacy file() citation).
static void cli_sprint_term(char *buf, size_t cap, size_t *o, int *fail,
                            const PrProblem *pr, Term t) {
  char nm[TP_NAME_LEN + 32];
  if (*fail) return;
  if (t == 0) { nm[0] = '_'; nm[1] = '\0'; }
  else if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (pr != NULL && v < pr->n_vars) snprintf(nm, sizeof nm, "%s", pr->vars[v]);
    else snprintf(nm, sizeof nm, "%s%u", cli_var_prefix(pr), v);
  } else if (term_tag(t) == TAG_CTR) {
    u32 lab = term_ext(t);
    if (pr != NULL && lab >= PR_LABEL_BASE && lab - PR_LABEL_BASE < pr->n_syms)
      snprintf(nm, sizeof nm, "%s", pr->syms[lab - PR_LABEL_BASE].name);
    else snprintf(nm, sizeof nm, "f%u", lab);
  } else {
    *fail = 1;
    return;
  }
  size_t l = strlen(nm);
  if (*o + l + 2 >= cap) { *fail = 1; return; }
  memcpy(buf + *o, nm, l);
  *o += l;
  if (t != 0 && term_tag(t) == TAG_CTR && term_ctr_n(t) > 0) {
    u32 n = term_ctr_n(t);
    buf[(*o)++] = '(';
    for (u32 i = 0; i < n && !*fail; i++) {
      if (i > 0) {
        if (*o + 1 >= cap) { *fail = 1; return; }
        buf[(*o)++] = ',';
      }
      cli_sprint_term(buf, cap, o, fail, pr, term_ctr_at(t, i));
    }
    if (*o + 1 >= cap) { *fail = 1; return; }
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
}

// Provenance lookup for one (lhs, rhs) unit: prints the conversion
// chain to fp and fills src with the citation tail.  NULL when the
// unit carries no provenance (legacy file() path).
static const char *cli_prov_source(FILE *fp, const PrProblem *pr,
                                   Term lhs, Term rhs, int neg,
                                   char *src, size_t cap) {
  if (g_prov_nleaf == 0) return NULL;
  static char lb[4 * TP_SIDE_LEN], rb[4 * TP_SIDE_LEN];
  size_t lo = 0, ro = 0;
  int fail = 0;
  cli_sprint_term(lb, sizeof lb, &lo, &fail, pr, lhs);
  cli_sprint_term(rb, sizeof rb, &ro, &fail, pr, rhs);
  if (fail) return NULL;
  const ProvLeaf *pl = prov_leaf_find(lb, rb, neg);
  if (pl == NULL) return NULL;
  if (!prov_src_citation(fp, pl->kind, pl->ix, src, cap)) return NULL;
  return src;
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
  // FOF-path axiom leaf: resolve the conversion lineage FIRST -- the
  // chain records it cites must precede this record in the output.
  const char *prov_src = NULL;
  char srcbuf[1024];
  if (reason == TRACE_AXIOM)
    prov_src = cli_prov_source(fp, pr, lhs, rhs, 0, srcbuf, sizeof srcbuf);
  fprintf(fp, "cnf(t%u, %s, ", ti,
          (reason == TRACE_AXIOM && prov_src == NULL) ? "axiom" : "plain");
  cli_print_term(fp, pr, lhs);
  fputs(" = ", fp);
  cli_print_term(fp, pr, rhs);
  if (reason == TRACE_AXIOM) {
    // Leaf clause: a problem axiom cites the problem file so GDV can
    // match it and complete leaf verification; a goal-flattening
    // definition (fresh sFlat constant = subterm) is an introduced
    // definitional extension, not a problem clause, and is sourced as
    // such.  An equationalized FOF clause cites its conversion chain
    // (J13: leaves must be formulae from the problem).
    if (prov_src != NULL)
      fputs(prov_src, fp);
    else if (cli_term_has_flat_sym(pr, lhs) || cli_term_has_flat_sym(pr, rhs))
      fputs(", introduced(definition)).\n", fp);
    else
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
  // FOF-path goal: cite the encoding lineage of the negative unit
  // (direct goals cite their CNF record; the funnel goal cites every
  // negative clause's record) -- chains print before the record.
  const char *goal_src = NULL;
  char gsrcbuf[1024];
  goal_src = cli_prov_source(fp, pr, gl, gr, 1, gsrcbuf, sizeof gsrcbuf);
  fprintf(fp, "cnf(goal_%u, negated_conjecture, ", goal_id);
  cli_print_term(fp, pr, gl);
  fputs(" != ", fp);
  cli_print_term(fp, pr, gr);
  if (goal_src != NULL)
    fputs(goal_src, fp);
  else
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
    // FOF ontology when a conjecture was negated (Theorem); CNF
    // ontology for clausal input (Unsatisfiable).
    fprintf(out, "%% SZS status %s for %s\n",
            g_cli_fof_conjecture ? "Theorem" :
            g_cli_tptp_mode      ? "Unsatisfiable" : "Theorem",
            g_cli_problem);
    g_cli_szs_printed = 1;
  }
  // FOF-path derivations include fof() conversion records, so the
  // output form is a Proof; pure-CNF input keeps CNFRefutation
  // byte-identically.
  const char *szs_form = g_cli_fof_applied ? "Proof" : "CNFRefutation";
  fprintf(out, "%% SZS output start %s for %s\n", szs_form, g_cli_problem);
  // Source the goal terms from the ENGINE's GC-rooted slots, NOT the
  // bench PrProblem's parse-time handles.  The PrProblem is not a GC
  // root, so pr->goals_l[]/pr->goal_l dangle after any mid-search GC:
  // they read back as garbage (typically a nullary cell) whose two
  // sides then look trivially "joined" at zero steps, which is exactly
  // why the extractor returned an empty chain and the hook printed
  // "no single-NF chain extractable" for GC-triggering proofs.
  // s->goals_lhs[]/s->goal_lhs relocate with the heap and are the only
  // valid goal terms at emit time.  A single goal is already the live
  // alias (goal_lhs == goals_lhs[0], MNF join state intact) so it is
  // used directly -- re-setting it would rebuild the goals array and
  // destroy the MNF front the extractor needs.  Multi-goal re-points
  // the alias per conjunct with atp_goals_alias (no array rebuild).
  u32 n_goals = (s->n_goals > 0u) ? s->n_goals
              : ((pr->n_goals > 0u) ? pr->n_goals : 1u);
  for (u32 g = 0; g < n_goals; g++) {
    Term gl, gr;
    if (s->n_goals > 1u) {
      atp_goals_alias(s, g);
      gl = s->goal_lhs; gr = s->goal_rhs;
    } else if (s->n_goals == 1u) {
      gl = s->goal_lhs; gr = s->goal_rhs;
    } else {
      gl = (pr->n_goals > 0u) ? pr->goals_l[g] : pr->goal_l;
      gr = (pr->n_goals > 0u) ? pr->goals_r[g] : pr->goal_r;
      if (thvm_atp_set_goal(s, gl, gr) == 0u) continue;
    }
    u32 n = thvm_atp_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    if (n == 0u) n = thvm_atp_mnf_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    if (n == 0u && !(term_tag(gl) == term_tag(gr) && gl == gr)) {
      fprintf(out, "%% goal %u: no single-NF chain extractable\n", g);
      continue;
    }
    cli_emit_goal_chain(out, s, pr, gl, gr, g, steps, n, printed);
  }
  fprintf(out, "%% SZS output end %s for %s\n", szs_form, g_cli_problem);
  fflush(out);
  free(printed);
}

