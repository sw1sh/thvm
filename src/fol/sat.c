// FOL saturation loop (Otter-style given-clause).
//
// Holds a set of clauses split into active (already used for
// inference) + passive (waiting).  Each step:
//   1. Pop a clause from the passive queue.
//   2. Move it to active.
//   3. Generate inferences against every active clause (resolution +
//      paramodulation + factoring + reflex-resolve + eq-factoring),
//      plus the given-clause self-inferences.
//   4. For each derived clause:
//        - empty clause   -> status = PROVED, return.
//        - tautology      -> drop.
//        - subsumed       -> drop.
//        - otherwise      -> push to passive.
//   5. step++; if step >= step_cap, status = ABORTED.
//
// Selection: FIFO (oldest-first) -- the conservative default that
// E and Vampire fall back to.  Smarter policies (literal-pick on
// positive-or-max-weight) land in a follow-up tick.
//
// Indexing: none yet.  Subsumption walks the full active+passive
// list; CP generation walks the full active list.  These O(n^2)
// scans match thvm's existing equational layer's pre-index baseline;
// can be lifted to a discrimination tree once a real workload runs.
//
// Memory: the state OWNS every FolClause* it stores.  Free via
// cnf_free.

#define CNF_DEFAULT_CAP 64u

fn CnfState *cnf_init(u32 step_cap) {
  CnfState *s = (CnfState *)calloc(1, sizeof(CnfState));
  if (s == NULL) return NULL;
  s->cap = CNF_DEFAULT_CAP;
  s->clauses = (FolClause **)calloc(s->cap, sizeof(FolClause *));
  s->cap_active = CNF_DEFAULT_CAP;
  s->active = (u32 *)calloc(s->cap_active, sizeof(u32));
  s->cap_passive = CNF_DEFAULT_CAP;
  s->passive = (u32 *)calloc(s->cap_passive, sizeof(u32));
  s->trace_cap = CNF_DEFAULT_CAP;
  s->trace = (FolTrace *)calloc(s->trace_cap, sizeof(FolTrace));
  if (s->clauses == NULL || s->active == NULL || s->passive == NULL || s->trace == NULL) {
    free(s->clauses); free(s->active); free(s->passive); free(s->trace); free(s);
    return NULL;
  }
  s->cap_deferred = CNF_DEFAULT_CAP;
  s->deferred_free = (FolClause **)calloc(s->cap_deferred, sizeof(FolClause *));
  if (s->deferred_free == NULL) {
    free(s->clauses); free(s->active); free(s->passive); free(s->trace); free(s);
    return NULL;
  }
  s->step_cap = (step_cap == 0u) ? 1u << 30 : step_cap;
  s->status = ATP_RUNNING;
  return s;
}

fn void cnf_free(CnfState *s) {
  if (s == NULL) return;
  for (u32 i = 0; i < s->n; i++) {
    if (s->clauses[i] != NULL) fol_clause_free(s->clauses[i]);
  }
  // Drain any deferred frees still pending.
  for (u32 i = 0; i < s->n_deferred; i++) {
    if (s->deferred_free[i] != NULL) fol_clause_free(s->deferred_free[i]);
  }
  free(s->clauses);
  free(s->active);
  free(s->passive);
  free(s->deferred_free);
  free(s->trace);
  free(s);
}

static void cnf_grow(u32 *cap, u8 **arr, u32 elsz) {
  u32 new_cap = (*cap) * 2u;
  void *new_arr = realloc(*arr, (size_t)new_cap * elsz);
  if (new_arr != NULL) {
    *arr = (u8 *)new_arr;
    *cap = new_cap;
  }
}

// Internal: append clause + record trace metadata.  Used by every
// inference path so the provenance DAG stays consistent.
static i32 cnf_add_clause_traced(CnfState *s, FolClause *c,
                                 FolInference rule,
                                 u32 parent_a, u32 parent_b) {
  if (s == NULL || c == NULL) return -1;
  if (s->n >= s->cap) {
    cnf_grow(&s->cap, (u8 **)&s->clauses, sizeof(FolClause *));
    if (s->n >= s->cap) return -1;
    for (u32 i = s->n; i < s->cap; i++) s->clauses[i] = NULL;
  }
  while (s->n >= s->trace_cap) {
    cnf_grow(&s->trace_cap, (u8 **)&s->trace, sizeof(FolTrace));
    if (s->n >= s->trace_cap) return -1;
  }
  u32 id = s->n;
  s->clauses[id] = c;
  s->trace[id].rule     = rule;
  s->trace[id].parent_a = parent_a;
  s->trace[id].parent_b = parent_b;
  s->n++;
  if (s->n_passive >= s->cap_passive) {
    cnf_grow(&s->cap_passive, (u8 **)&s->passive, sizeof(u32));
    if (s->n_passive >= s->cap_passive) return -1;
  }
  s->passive[s->n_passive++] = id;
  return (i32)id;
}

// Public entry point: user-added clauses get the INPUT marker.
fn i32 cnf_add_clause(CnfState *s, FolClause *c) {
  return cnf_add_clause_traced(s, c, FOL_INF_INPUT,
                                FOL_NO_PARENT, FOL_NO_PARENT);
}

// Ordering-aware demodulation: when CnfState carries a reduction
// ordering, gate each rewrite `s -> t` on σ(s) > σ(t).  Without any
// ordering set, demod fires unconditionally (the same naïve behaviour
// the Stage 11 forward + Stage 12 backward demod relied on).
//
// Dispatch priority for multiple set orderings: WPO > RPO > LPO > KBO.
static KboCmp cnf_compare(const CnfState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  if (s->cnf_wpo != NULL) return (KboCmp)thvm_wpo(lhs, rhs, s->cnf_wpo);
  if (s->cnf_rpo != NULL) return (KboCmp)thvm_rpo(lhs, rhs, s->cnf_rpo);
  if (s->cnf_lpo != NULL) return (KboCmp)thvm_lpo(lhs, rhs, s->cnf_lpo);
  if (s->cnf_kbo != NULL) return thvm_kbo(lhs, rhs, s->cnf_kbo);
  return KBO_UN;
}

static u8 cnf_has_ordering(const CnfState *s) {
  return (s != NULL && (s->cnf_kbo != NULL || s->cnf_lpo != NULL
                        || s->cnf_rpo != NULL || s->cnf_wpo != NULL))
         ? 1u : 0u;
}

fn void cnf_set_kbo(CnfState *s, const KboConfig *kbo) { if (s) s->cnf_kbo = kbo; }
fn void cnf_set_lpo(CnfState *s, const LpoConfig *lpo) { if (s) s->cnf_lpo = lpo; }
fn void cnf_set_rpo(CnfState *s, const RpoConfig *rpo) { if (s) s->cnf_rpo = rpo; }
fn void cnf_set_wpo(CnfState *s, const WpoConfig *wpo) { if (s) s->cnf_wpo = wpo; }
fn void cnf_set_select(CnfState *s, CnfSelection sel) { if (s) s->cnf_select = sel; }

// Pick a selected literal of `c` under the state's selection
// policy.  Returns the literal index or ~0u when the policy says
// "all literals allowed" (either NONE was set, or the policy found
// no matching literal -- the all-literals fallback preserves
// completeness for Horn-like clauses).
static u32 cnf_select_lit(const CnfState *s, const FolClause *c) {
  if (s == NULL || c == NULL || s->cnf_select == CNF_SELECT_NONE) return ~0u;
  u8 want_sign = (s->cnf_select == CNF_SELECT_NEGATIVE) ? 1u : 0u;
  for (u32 i = 0; i < c->n_lits; i++) {
    if (c->lits[i].sign == want_sign) return i;
  }
  return ~0u;  // fallback: all literals
}

// Rewrite-once with optional ordering gate.  When the state has any
// ordering attached, only fires if σ(s) > σ(t) at the match site
// (σ(s) is the matched subterm `term` itself -- thvm_match is
// one-way, so σ applied to the pattern reproduces the subject).
static Term cnf_rewrite_once(const CnfState *s, Term term, Term lhs, Term rhs) {
  // Root match attempt.
  RewriteSubst subst = {{0}};
  if (thvm_match(lhs, term, &subst)) {
    Term sub_rhs = thvm_subst_apply(rhs, &subst);
    u8 ok = 1u;
    if (cnf_has_ordering(s)) {
      // σ(lhs) == term under a successful one-way match.
      ok = (cnf_compare(s, term, sub_rhs) == KBO_GT) ? 1u : 0u;
    }
    if (ok) return sub_rhs;
  }
  if (term_tag(term) != TAG_CTR) return term;
  u32 n = term_ctr_n(term);
  if (n == 0u) return term;
  if (n > REWRITE_MAX_ARITY) return term;
  Term kids[REWRITE_MAX_ARITY];
  u8 changed = 0u;
  for (u32 i = 0; i < n; i++) {
    Term orig = term_ctr_at(term, i);
    kids[i] = cnf_rewrite_once(s, orig, lhs, rhs);
    if (kids[i] != orig) changed = 1u;
  }
  if (!changed) return term;
  return term_new_ctr(term_ext(term), kids, n);
}

// Apply eq_clause's positive equality to every literal of target,
// gated by the state's ordering when set.  Returns a fresh clause
// if anything changed; NULL otherwise.  Caller owns the result.
static FolClause *cnf_demodulate_ordered(const CnfState *s,
                                         const FolClause *eq_clause,
                                         const FolClause *target) {
  if (eq_clause == NULL || target == NULL) return NULL;
  if (eq_clause->n_lits != 1u) return NULL;
  if (eq_clause->lits[0].sign != 0u) return NULL;
  Term eq_atom = eq_clause->lits[0].atom;
  if (!fol_atom_is_eq(eq_atom)) return NULL;
  Term s_raw = term_ctr_at(eq_atom, 0u);
  Term t_raw = term_ctr_at(eq_atom, 1u);
  Term lhs = thvm_rename_vars(s_raw, FOL_RENAME_OFFSET);
  Term rhs = thvm_rename_vars(t_raw, FOL_RENAME_OFFSET);

  u32 n = target->n_lits;
  Term *cache = (Term *)calloc(n, sizeof(Term));
  if (cache == NULL) return NULL;
  u8 any_change = 0u;
  for (u32 i = 0; i < n; i++) {
    Term raw = target->lits[i].atom;
    Term re  = cnf_rewrite_once(s, raw, lhs, rhs);
    if (re != raw) any_change = 1u;
    cache[i] = re;
  }
  if (!any_change) { free(cache); return NULL; }
  FolClause *r = fol_clause_new(n);
  if (r == NULL) { free(cache); return NULL; }
  for (u32 i = 0; i < n; i++) {
    r->lits[i].atom = cache[i];
    r->lits[i].sign = target->lits[i].sign;
  }
  free(cache);
  return r;
}

// Forward demodulation: normalize `*pc` against every unit positive
// equality currently in the active set.  Iterates to a fixpoint
// bounded by CNF_DEMOD_BUDGET steps -- without a reduction-ordering
// check, demodulation can in principle loop on cyclic rule pairs;
// with one set, well-foundedness makes the budget cap untouched.
#define CNF_DEMOD_BUDGET 16u

static void cnf_forward_demod(CnfState *s, FolClause **pc) {
  if (pc == NULL || *pc == NULL) return;
  FolClause *c = *pc;
  for (u32 iter = 0; iter < CNF_DEMOD_BUDGET; iter++) {
    u8 changed = 0u;
    for (u32 i = 0; i < s->n_active; i++) {
      FolClause *rule = s->clauses[s->active[i]];
      if (rule == NULL) continue;
      // Skip when target IS the rule (no self-rewrite).
      if (rule == c) continue;
      FolClause *d = cnf_demodulate_ordered(s, rule, c);
      if (d != NULL) {
        fol_clause_free(c);
        c = d;
        changed = 1u;
        break;
      }
    }
    if (!changed) break;
  }
  *pc = c;
}

// Internal: check whether `c` is subsumed by any currently-stored
// active or passive clause.  Used as a forward-redundancy filter on
// derived clauses.
static u8 cnf_is_subsumed(const CnfState *s, const FolClause *c) {
  for (u32 i = 0; i < s->n_active; i++) {
    const FolClause *a = s->clauses[s->active[i]];
    if (a == NULL) continue;
    if (fol_subsumes(a, c)) return 1u;
  }
  for (u32 i = 0; i < s->n_passive; i++) {
    const FolClause *p = s->clauses[s->passive[i]];
    if (p == NULL) continue;
    if (fol_subsumes(p, c)) return 1u;
  }
  return 0u;
}

// Backward demodulation: when a fresh unit positive equality lands,
// re-normalize every existing clause through it.  Modified clauses
// take a fresh id (the old slot gets NULLed + deferred-free); the
// new version still passes tautology + subsumption filters.
//
// Snapshot s->n BEFORE the loop so demodulated derivatives we add
// don't get re-visited.  Doesn't recurse into cnf_consider's
// forward-demod (the rule has already been applied; further demod
// would be no-op against this particular rule, and we don't fire
// the OTHER active rules here -- those run on the next iteration
// of the saturation loop when this clause comes off passive).
static void cnf_backward_demod(CnfState *s, u32 new_id) {
  FolClause *rule = s->clauses[new_id];
  if (rule == NULL) return;
  if (rule->n_lits != 1u || rule->lits[0].sign != 0u) return;
  if (!fol_atom_is_eq(rule->lits[0].atom)) return;
  u32 n_snap = s->n;
  for (u32 i = 0; i < n_snap; i++) {
    if (i == new_id) continue;
    FolClause *old = s->clauses[i];
    if (old == NULL) continue;
    FolClause *d = cnf_demodulate_ordered(s, rule, old);
    if (d == NULL) continue;

    // Clobber the old slot; defer free.
    s->clauses[i] = NULL;
    if (s->n_deferred >= s->cap_deferred) {
      cnf_grow(&s->cap_deferred, (u8 **)&s->deferred_free, sizeof(FolClause *));
    }
    if (s->n_deferred < s->cap_deferred) {
      s->deferred_free[s->n_deferred++] = old;
    } else {
      // Couldn't grow the deferred queue -- last resort, leak and
      // skip the demodulated derivative entirely.
      s->clauses[i] = old;
      fol_clause_free(d);
      continue;
    }

    // Push d through tautology + subsumption (forward-demod is
    // intentionally skipped here -- see comment above).
    if (fol_clause_is_empty(d)) {
      cnf_add_clause_traced(s, d, FOL_INF_DEMOD, new_id, i);
      s->status = ATP_PROVED;
      return;
    }
    if (fol_is_tautology(d)) { fol_clause_free(d); continue; }
    if (cnf_is_subsumed(s, d)) { fol_clause_free(d); continue; }
    cnf_add_clause_traced(s, d, FOL_INF_DEMOD, new_id, i);
  }
}

// Backward subsumption: a freshly-added clause may subsume some
// older active/passive ones, making them redundant.  We mark them
// for removal by NULLing the slot in `clauses[]` immediately and
// pushing the FolClause* to a deferred-free queue.  Concurrent
// inference loops that captured a clause pointer at entry stay
// safe; the actual `free` happens at end-of-step.
static void cnf_backward_subsume(CnfState *s, u32 new_id) {
  const FolClause *new_c = s->clauses[new_id];
  if (new_c == NULL) return;
  for (u32 i = 0; i < s->n; i++) {
    if (i == new_id) continue;
    FolClause *old = s->clauses[i];
    if (old == NULL) continue;
    if (fol_subsumes(new_c, old)) {
      s->clauses[i] = NULL;
      if (s->n_deferred >= s->cap_deferred) {
        cnf_grow(&s->cap_deferred, (u8 **)&s->deferred_free, sizeof(FolClause *));
      }
      if (s->n_deferred < s->cap_deferred) {
        s->deferred_free[s->n_deferred++] = old;
      } else {
        // Couldn't grow -- last resort, leak the clause (rather than
        // free it and risk a dangling pointer).  cnf_free will sweep.
        // Pin to clauses[] so it gets freed by cnf_free.
        s->clauses[i] = old;
      }
    }
  }
}

// Process the deferred-free queue.  Call at the END of cnf_step,
// after every inference-loop frame has unwound.
static void cnf_flush_deferred(CnfState *s) {
  for (u32 i = 0; i < s->n_deferred; i++) {
    fol_clause_free(s->deferred_free[i]);
  }
  s->n_deferred = 0u;
}

// Push a derived clause through the redundancy filters; consume on
// success (state-owns), free on drop.  Records the supplied
// (rule, parent_a, parent_b) in the trace if the clause survives.
//
// Order matters: forward demodulation FIRST (normalize against
// active unit equalities), then tautology / subsumption checks on
// the normalized form.  Demod-normalized clauses are more likely to
// subsume / be subsumed, so running demod before subsumption maximizes
// the redundancy filtering's bite.
//
// Forward demod doesn't update the trace -- the derivation's
// "inference rule" is whatever produced the pre-demod candidate;
// demod just normalizes the literal contents.  This matches Vampire's
// convention of treating demod as a non-inference simplifier.
static void cnf_consider(CnfState *s, FolClause *c,
                         FolInference rule,
                         u32 parent_a, u32 parent_b) {
  if (c == NULL) return;
  if (s->status != ATP_RUNNING) { fol_clause_free(c); return; }
  cnf_forward_demod(s, &c);
  if (c == NULL) return;
  if (fol_clause_is_empty(c)) {
    cnf_add_clause_traced(s, c, rule, parent_a, parent_b);
    s->status = ATP_PROVED;
    return;
  }
  if (fol_is_tautology(c)) { fol_clause_free(c); return; }
  if (cnf_is_subsumed(s, c)) { fol_clause_free(c); return; }
  i32 new_id = cnf_add_clause_traced(s, c, rule, parent_a, parent_b);
  if (new_id >= 0) {
    cnf_backward_subsume(s, (u32)new_id);
    cnf_backward_demod(s, (u32)new_id);
  }
}

// Generate all binary-resolution inferences between two clauses.
// Under a selection function, only the selected literal of each
// clause participates (with the all-literals fallback when the
// policy finds no matching lit).
static void cnf_gen_resolution(CnfState *s, u32 a_id, u32 b_id) {
  const FolClause *a = s->clauses[a_id];
  const FolClause *b = s->clauses[b_id];
  if (a == NULL || b == NULL) return;
  u32 sel_a = cnf_select_lit(s, a);
  u32 sel_b = cnf_select_lit(s, b);
  for (u32 i = 0; i < a->n_lits; i++) {
    if (sel_a != ~0u && i != sel_a) continue;
    for (u32 j = 0; j < b->n_lits; j++) {
      if (sel_b != ~0u && j != sel_b) continue;
      if (a->lits[i].sign == b->lits[j].sign) continue;  // need complementary
      FolClause *r = fol_resolve(a, i, b, j);
      cnf_consider(s, r, FOL_INF_RESOLVE, a_id, b_id);
      if (s->status != ATP_RUNNING) return;
    }
  }
}

// All same-polarity factor pairs within a single clause.
static void cnf_gen_factoring(CnfState *s, u32 c_id) {
  const FolClause *c = s->clauses[c_id];
  if (c == NULL) return;
  for (u32 i = 0; i < c->n_lits; i++) {
    for (u32 j = i + 1u; j < c->n_lits; j++) {
      FolClause *r = fol_factor(c, i, j);
      cnf_consider(s, r, FOL_INF_FACTOR, c_id, FOL_NO_PARENT);
      if (s->status != ATP_RUNNING) return;
    }
  }
}

// Reflex-resolve on every negative-equality literal.
static void cnf_gen_reflex(CnfState *s, u32 c_id) {
  const FolClause *c = s->clauses[c_id];
  if (c == NULL) return;
  for (u32 i = 0; i < c->n_lits; i++) {
    if (c->lits[i].sign != 1u) continue;
    if (!fol_atom_is_eq(c->lits[i].atom)) continue;
    FolClause *r = fol_reflex_resolve(c, i);
    cnf_consider(s, r, FOL_INF_REFLEX, c_id, FOL_NO_PARENT);
    if (s->status != ATP_RUNNING) return;
  }
}

// Recursive position walker: invokes `cb` at every non-variable
// position inside `atom`, passing the current path.  Caps depth at
// CP_MAX_DEPTH (the existing cp/_.c walker's bound -- positions
// stored on the CP are tagged with a u8 array of length CP_MAX_DEPTH,
// reusing the same convention here keeps the depth budget consistent).
typedef void (*CnfPosCb)(const u32 *p, u32 p_len, void *ctx);

static void cnf_walk_positions(Term t, u32 *path, u32 depth,
                               CnfPosCb cb, void *ctx) {
  if (term_tag(t) != TAG_CTR) return;
  cb(path, depth, ctx);
  if (depth >= CP_MAX_DEPTH) return;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) {
    path[depth] = i;
    cnf_walk_positions(term_ctr_at(t, i), path, depth + 1u, cb, ctx);
  }
}

// Paramodulation visitor context: emits a CP for each visited
// position of `target_atom`.
typedef struct {
  CnfState        *s;
  const FolClause *eq_clause;
  u32              eq_idx;
  u32              eq_id;        // for the trace
  const FolClause *target;
  u32              target_idx;
  u32              target_id;    // for the trace
} CnfParamodCtx;

static void cnf_paramod_at_pos(const u32 *p, u32 p_len, void *raw) {
  CnfParamodCtx *ctx = (CnfParamodCtx *)raw;
  if (ctx->s->status != ATP_RUNNING) return;
  // Both orientations (s->t, t->s) -- paramodulation needs both for
  // refutation completeness with non-Horn clauses.
  FolClause *r0 = fol_paramodulate(ctx->eq_clause, ctx->eq_idx, /*swap*/ 0,
                                   ctx->target, ctx->target_idx, p, p_len);
  cnf_consider(ctx->s, r0, FOL_INF_PARAMOD, ctx->eq_id, ctx->target_id);
  if (ctx->s->status != ATP_RUNNING) return;
  FolClause *r1 = fol_paramodulate(ctx->eq_clause, ctx->eq_idx, /*swap*/ 1,
                                   ctx->target, ctx->target_idx, p, p_len);
  cnf_consider(ctx->s, r1, FOL_INF_PARAMOD, ctx->eq_id, ctx->target_id);
}

// All paramodulation results of eq_clause (as the source of an
// equality literal) into target (any literal, any non-variable
// position).  Selection function applies to the TARGET literal --
// paramod only fires into the selected literal when one is picked.
// (The eq_clause's source-equality literal is unconstrained -- it
// must be a positive equality regardless of selection.)
static void cnf_gen_paramod(CnfState *s, u32 eq_id, u32 target_id) {
  const FolClause *eqc = s->clauses[eq_id];
  const FolClause *tgt = s->clauses[target_id];
  if (eqc == NULL || tgt == NULL) return;
  u32 sel_t = cnf_select_lit(s, tgt);
  for (u32 i = 0; i < eqc->n_lits; i++) {
    if (eqc->lits[i].sign != 0u) continue;          // positive equalities only
    if (!fol_atom_is_eq(eqc->lits[i].atom)) continue;
    for (u32 j = 0; j < tgt->n_lits; j++) {
      if (sel_t != ~0u && j != sel_t) continue;
      // Skip paramodulating an equality literal into itself when
      // eq_id == target_id and i == j (same literal in same clause).
      if (eq_id == target_id && i == j) continue;
      CnfParamodCtx ctx = {
        .s = s, .eq_clause = eqc, .eq_idx = i, .eq_id = eq_id,
        .target = tgt, .target_idx = j, .target_id = target_id,
      };
      u32 path[CP_MAX_DEPTH];
      cnf_walk_positions(tgt->lits[j].atom, path, 0u,
                         cnf_paramod_at_pos, &ctx);
      if (s->status != ATP_RUNNING) return;
    }
  }
}

// Eq-factor every pair of positive-equality literals.
static void cnf_gen_eq_factoring(CnfState *s, u32 c_id) {
  const FolClause *c = s->clauses[c_id];
  if (c == NULL) return;
  for (u32 i = 0; i < c->n_lits; i++) {
    if (c->lits[i].sign != 0u) continue;
    if (!fol_atom_is_eq(c->lits[i].atom)) continue;
    for (u32 j = 0; j < c->n_lits; j++) {
      if (i == j) continue;
      if (c->lits[j].sign != 0u) continue;
      if (!fol_atom_is_eq(c->lits[j].atom)) continue;
      FolClause *r = fol_eq_factor(c, i, j);
      cnf_consider(s, r, FOL_INF_EQ_FACTOR, c_id, FOL_NO_PARENT);
      if (s->status != ATP_RUNNING) return;
    }
  }
}

// One Otter-style given-clause step.
fn AtpStatus cnf_step(CnfState *s) {
  if (s == NULL) return ATP_ABORTED;
  if (s->status != ATP_RUNNING) return s->status;
  if (s->step >= s->step_cap) {
    s->status = ATP_ABORTED;
    return s->status;
  }

  // FIFO pop: find the lowest-index unprocessed passive id, skipping
  // entries whose clauses[] was NULLed by backward subsumption.
  u32 given_id = 0u;
  u8 found = 0u;
  while (s->passive_head < s->n_passive) {
    u32 cand = s->passive[s->passive_head++];
    if (s->clauses[cand] != NULL) { given_id = cand; found = 1u; break; }
  }
  if (!found) {
    s->status = ATP_QUEUE_EMPTY;
    return s->status;
  }

  // Move given to active.
  if (s->n_active >= s->cap_active) {
    cnf_grow(&s->cap_active, (u8 **)&s->active, sizeof(u32));
    if (s->n_active >= s->cap_active) {
      s->status = ATP_ABORTED;
      return s->status;
    }
  }
  s->active[s->n_active++] = given_id;

  // Self-inferences first.
  cnf_gen_factoring(s, given_id);
  if (s->status != ATP_RUNNING) return s->status;
  cnf_gen_reflex(s, given_id);
  if (s->status != ATP_RUNNING) return s->status;
  cnf_gen_eq_factoring(s, given_id);
  if (s->status != ATP_RUNNING) return s->status;
  // Self-paramod: given's own equality literals against its other
  // literals.  Useful when a clause carries both an equation and a
  // predicate atom that the equation rewrites.
  cnf_gen_paramod(s, given_id, given_id);
  if (s->status != ATP_RUNNING) return s->status;

  // Cross-inferences with every other active clause.  Snapshot the
  // active count before the loop -- new clauses added during the
  // step go straight to passive, not active.  Skip slots whose
  // clauses[] has been NULLed by backward subsumption.
  u32 active_snapshot = s->n_active;
  if (s->clauses[given_id] == NULL) {
    // Backward subsumption clobbered the given clause; skip its
    // cross-inferences.  Continue stepping (another passive will
    // pop next iteration).
    s->step++;
    cnf_flush_deferred(s);
    return s->status;
  }
  for (u32 k = 0; k + 1u < active_snapshot; k++) {
    u32 a_id = s->active[k];
    if (a_id == given_id) continue;
    if (s->clauses[a_id] == NULL) continue;
    cnf_gen_resolution(s, given_id, a_id);
    if (s->status != ATP_RUNNING) return s->status;
    cnf_gen_resolution(s, a_id, given_id);   // symmetric face
    if (s->status != ATP_RUNNING) return s->status;
    // Paramodulation BOTH directions: given's equalities -> active,
    // active's equalities -> given.
    cnf_gen_paramod(s, given_id, a_id);
    if (s->status != ATP_RUNNING) return s->status;
    cnf_gen_paramod(s, a_id, given_id);
    if (s->status != ATP_RUNNING) return s->status;
  }

  s->step++;
  cnf_flush_deferred(s);
  return s->status;
}

// Run the loop to a terminal status (or step_cap).  Returns the
// final status.
fn AtpStatus cnf_run(CnfState *s) {
  if (s == NULL) return ATP_ABORTED;
  while (s->status == ATP_RUNNING) {
    AtpStatus st = cnf_step(s);
    if (st != ATP_RUNNING) break;
  }
  return s->status;
}

// === proof printer ===================================================
//
// Recursively walk the trace DAG from a root clause.  Print each
// ancestor exactly once (track visited via a bitmap on the stack).
// The output format is text-only; one line per visited clause:
//   c<id>: <rule> [c<a>[, c<b>]]   <literals>
// Children before parents would be the topological reversal; we
// instead print parents first (post-order on the inverse DAG) so a
// reader sees the dependencies before the clause that uses them.

static const char *cnf_rule_name(FolInference r) {
  switch (r) {
    case FOL_INF_INPUT:     return "input";
    case FOL_INF_RESOLVE:   return "resolve";
    case FOL_INF_FACTOR:    return "factor";
    case FOL_INF_PARAMOD:   return "paramod";
    case FOL_INF_REFLEX:    return "reflex";
    case FOL_INF_EQ_FACTOR: return "eq-factor";
    case FOL_INF_DEMOD:     return "demod";
    default:                return "?";
  }
}

static void cnf_print_atom(Term atom, FILE *out) {
  if (term_tag(atom) == TAG_FVR) {
    fprintf(out, "v%u", term_ext(atom));
    return;
  }
  if (term_tag(atom) == TAG_CTR) {
    u32 lab = term_ext(atom);
    u32 n   = term_ctr_n(atom);
    fprintf(out, "s%u", lab);
    if (n > 0u) {
      fprintf(out, "(");
      for (u32 i = 0; i < n; i++) {
        if (i > 0u) fprintf(out, ", ");
        cnf_print_atom(term_ctr_at(atom, i), out);
      }
      fprintf(out, ")");
    }
    return;
  }
  fprintf(out, "?");
}

static void cnf_print_clause_line(const CnfState *s, u32 id, FILE *out) {
  const FolClause *c = s->clauses[id];
  if (c == NULL) {
    fprintf(out, "c%u: <freed>\n", id);
    return;
  }
  const FolTrace *t = &s->trace[id];
  fprintf(out, "c%u: %s", id, cnf_rule_name(t->rule));
  if (t->parent_a != FOL_NO_PARENT) {
    fprintf(out, " [c%u", t->parent_a);
    if (t->parent_b != FOL_NO_PARENT) fprintf(out, ", c%u", t->parent_b);
    fprintf(out, "]");
  }
  fprintf(out, "   ");
  if (c->n_lits == 0u) {
    fprintf(out, "FALSE");
  } else {
    for (u32 i = 0; i < c->n_lits; i++) {
      if (i > 0u) fprintf(out, " | ");
      if (c->lits[i].sign != 0u) fprintf(out, "~");
      cnf_print_atom(c->lits[i].atom, out);
    }
  }
  fprintf(out, "\n");
}

static void cnf_print_proof_rec(const CnfState *s, u32 id,
                                u8 *visited, u32 n, FILE *out) {
  if (id >= n || visited[id]) return;
  visited[id] = 1u;
  const FolTrace *t = &s->trace[id];
  if (t->parent_a != FOL_NO_PARENT) {
    cnf_print_proof_rec(s, t->parent_a, visited, n, out);
  }
  if (t->parent_b != FOL_NO_PARENT) {
    cnf_print_proof_rec(s, t->parent_b, visited, n, out);
  }
  cnf_print_clause_line(s, id, out);
}

fn void cnf_print_proof(const CnfState *s, void *out_raw, u32 root_id) {
  if (s == NULL || out_raw == NULL) return;
  if (root_id >= s->n) return;
  FILE *out = (FILE *)out_raw;
  u8 *visited = (u8 *)calloc(s->n, 1);
  if (visited == NULL) return;
  cnf_print_proof_rec(s, root_id, visited, s->n, out);
  free(visited);
}
