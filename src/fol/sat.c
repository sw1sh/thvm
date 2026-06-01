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
  if (s->clauses == NULL || s->active == NULL || s->passive == NULL) {
    free(s->clauses); free(s->active); free(s->passive); free(s);
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
  free(s->clauses);
  free(s->active);
  free(s->passive);
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

// Append clause `c` (state takes ownership), assign next id, queue in
// passive.  Returns the assigned id, or -1 on overflow.
fn i32 cnf_add_clause(CnfState *s, FolClause *c) {
  if (s == NULL || c == NULL) return -1;
  if (s->n >= s->cap) {
    cnf_grow(&s->cap, (u8 **)&s->clauses, sizeof(FolClause *));
    if (s->n >= s->cap) return -1;
    // Zero the new slots.
    for (u32 i = s->n; i < s->cap; i++) s->clauses[i] = NULL;
  }
  u32 id = s->n;
  s->clauses[id] = c;
  s->n++;
  if (s->n_passive >= s->cap_passive) {
    cnf_grow(&s->cap_passive, (u8 **)&s->passive, sizeof(u32));
    if (s->n_passive >= s->cap_passive) return -1;
  }
  s->passive[s->n_passive++] = id;
  return (i32)id;
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

// Push a derived clause through the redundancy filters; consume on
// success (state-owns), free on drop.
static void cnf_consider(CnfState *s, FolClause *c) {
  if (c == NULL) return;
  if (s->status != ATP_RUNNING) { fol_clause_free(c); return; }
  if (fol_clause_is_empty(c)) {
    // The empty clause -- UNSAT, refutation found.
    cnf_add_clause(s, c);
    s->status = ATP_PROVED;
    return;
  }
  if (fol_is_tautology(c)) { fol_clause_free(c); return; }
  if (cnf_is_subsumed(s, c)) { fol_clause_free(c); return; }
  cnf_add_clause(s, c);
}

// Generate all binary-resolution inferences between two clauses.
static void cnf_gen_resolution(CnfState *s, u32 a_id, u32 b_id) {
  const FolClause *a = s->clauses[a_id];
  const FolClause *b = s->clauses[b_id];
  if (a == NULL || b == NULL) return;
  for (u32 i = 0; i < a->n_lits; i++) {
    for (u32 j = 0; j < b->n_lits; j++) {
      if (a->lits[i].sign == b->lits[j].sign) continue;  // need complementary
      FolClause *r = fol_resolve(a, i, b, j);
      cnf_consider(s, r);
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
      cnf_consider(s, r);
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
    cnf_consider(s, r);
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
  const FolClause *target;
  u32              target_idx;
} CnfParamodCtx;

static void cnf_paramod_at_pos(const u32 *p, u32 p_len, void *raw) {
  CnfParamodCtx *ctx = (CnfParamodCtx *)raw;
  if (ctx->s->status != ATP_RUNNING) return;
  // Both orientations (s->t, t->s) -- paramodulation needs both for
  // refutation completeness with non-Horn clauses.
  FolClause *r0 = fol_paramodulate(ctx->eq_clause, ctx->eq_idx, /*swap*/ 0,
                                   ctx->target, ctx->target_idx, p, p_len);
  cnf_consider(ctx->s, r0);
  if (ctx->s->status != ATP_RUNNING) return;
  FolClause *r1 = fol_paramodulate(ctx->eq_clause, ctx->eq_idx, /*swap*/ 1,
                                   ctx->target, ctx->target_idx, p, p_len);
  cnf_consider(ctx->s, r1);
}

// All paramodulation results of eq_clause (as the source of an
// equality literal) into target (any literal, any non-variable
// position).
static void cnf_gen_paramod(CnfState *s, u32 eq_id, u32 target_id) {
  const FolClause *eqc = s->clauses[eq_id];
  const FolClause *tgt = s->clauses[target_id];
  if (eqc == NULL || tgt == NULL) return;
  for (u32 i = 0; i < eqc->n_lits; i++) {
    if (eqc->lits[i].sign != 0u) continue;          // positive equalities only
    if (!fol_atom_is_eq(eqc->lits[i].atom)) continue;
    for (u32 j = 0; j < tgt->n_lits; j++) {
      // Skip paramodulating an equality literal into itself when
      // eq_id == target_id and i == j (same literal in same clause).
      if (eq_id == target_id && i == j) continue;
      CnfParamodCtx ctx = {
        .s = s, .eq_clause = eqc, .eq_idx = i,
        .target = tgt, .target_idx = j,
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
      cnf_consider(s, r);
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

  // FIFO pop: find the lowest-index unprocessed passive id.
  if (s->passive_head >= s->n_passive) {
    s->status = ATP_QUEUE_EMPTY;
    return s->status;
  }
  u32 given_id = s->passive[s->passive_head++];

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
  // step go straight to passive, not active.
  u32 active_snapshot = s->n_active;
  for (u32 k = 0; k + 1u < active_snapshot; k++) {
    u32 a_id = s->active[k];
    if (a_id == given_id) continue;
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
