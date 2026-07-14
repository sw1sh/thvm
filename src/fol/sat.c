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
// Given-clause selection: pick-best (smallest symbol-count weight
// first, min-heap) with an age-ratio interleave -- every K-th pick
// takes the OLDEST passive clause (FIFO lane) so no clause starves
// (fairness => refutation completeness).  K defaults to 5;
// THVM_FOL_AGE_RATIO=1 restores pure FIFO, =0 pure best-first.
//
// Ordered inference restrictions (only when a reduction ordering is
// attached via cnf_set_*): resolution + paramodulation fire only on
// MAXIMAL literals (no other literal strictly greater) unless the
// selection function picked a literal; paramodulation additionally
// only rewrites FROM the larger equation side and never from a
// clause whose selection picked a (negative) literal -- the standard
// superposition discipline.  Without an ordering, inference stays
// unrestricted (the conservative complete default).
//
// Indexing: a per-clause (root label, sign) bloom mask prefilters
// subsumption candidates (a subsumes c only if mask[a] subset of
// mask[c]); forward demodulation walks a dedicated list of active
// unit positive equalities instead of the whole active set.  Full
// discrimination-tree indexing can layer on later.
//
// Memory: the state OWNS every FolClause* it stores.  Free via
// cnf_free.

#define CNF_DEFAULT_CAP 64u
#define CNF_AGE_RATIO_DEFAULT 5u
// Heap-key penalty for clauses with no conjecture ancestry: goal
// descendants form a small cone and always pop first.
#define CNF_NONGOAL_PENALTY (1u << 20)

fn CnfState *cnf_init(u32 step_cap) {
  CnfState *s = (CnfState *)calloc(1, sizeof(CnfState));
  if (s == NULL) return NULL;
  s->cap = CNF_DEFAULT_CAP;
  s->clauses = (FolClause **)calloc(s->cap, sizeof(FolClause *));
  s->cweight = (u32 *)calloc(s->cap, sizeof(u32));
  s->cpri    = (u32 *)calloc(s->cap, sizeof(u32));
  s->cmask   = (u64 *)calloc(s->cap, sizeof(u64));
  s->cpicked = (u8 *)calloc(s->cap, 1);
  s->csel        = (u32 *)calloc(s->cap, sizeof(u32));
  s->cmaxlits    = (u64 *)calloc(s->cap, sizeof(u64));
  s->celig_valid = (u8 *)calloc(s->cap, 1);
  s->cgoal       = (u8 *)calloc(s->cap, 1);
  s->cfrozen     = (FolLit **)calloc(s->cap, sizeof(FolLit *));
  s->cnvars      = (u32 *)calloc(s->cap, sizeof(u32));
  s->cap_active = CNF_DEFAULT_CAP;
  s->active = (u32 *)calloc(s->cap_active, sizeof(u32));
  s->cap_passive = CNF_DEFAULT_CAP;
  s->passive = (u32 *)calloc(s->cap_passive, sizeof(u32));
  s->cap_pheap = CNF_DEFAULT_CAP;
  s->pheap = (u32 *)calloc(s->cap_pheap, sizeof(u32));
  s->cap_ueq = CNF_DEFAULT_CAP;
  s->ueq = (u32 *)calloc(s->cap_ueq, sizeof(u32));
  s->trace_cap = CNF_DEFAULT_CAP;
  s->trace = (FolTrace *)calloc(s->trace_cap, sizeof(FolTrace));
  s->cap_deferred = CNF_DEFAULT_CAP;
  s->deferred_free = (FolClause **)calloc(s->cap_deferred, sizeof(FolClause *));
  if (s->clauses == NULL || s->cweight == NULL || s->cpri == NULL ||
      s->cmask == NULL ||
      s->cpicked == NULL || s->csel == NULL || s->cmaxlits == NULL ||
      s->celig_valid == NULL || s->cgoal == NULL || s->cfrozen == NULL ||
      s->cnvars == NULL ||
      s->active == NULL || s->passive == NULL ||
      s->pheap == NULL || s->ueq == NULL || s->trace == NULL ||
      s->deferred_free == NULL) {
    free(s->clauses); free(s->cweight); free(s->cpri); free(s->cmask); free(s->cpicked);
    free(s->csel); free(s->cmaxlits); free(s->celig_valid); free(s->cgoal);
    free(s->cfrozen); free(s->cnvars);
    free(s->active); free(s->passive); free(s->pheap); free(s->ueq);
    free(s->trace); free(s->deferred_free); free(s);
    return NULL;
  }
  s->age_ratio = CNF_AGE_RATIO_DEFAULT;
  {
    const char *e = getenv("THVM_FOL_AGE_RATIO");
    if (e != NULL) s->age_ratio = (u32)atoi(e);
  }
  s->step_cap = (step_cap == 0u) ? 1u << 30 : step_cap;
  s->status = ATP_RUNNING;
  return s;
}

fn void cnf_free(CnfState *s) {
  if (s == NULL) return;
  for (u32 i = 0; i < s->n; i++) {
    if (s->clauses[i] != NULL) fol_clause_free(s->clauses[i]);
    if (s->corpse != NULL && s->corpse[i] != NULL) fol_clause_free(s->corpse[i]);
    free(s->cfrozen[i]);
  }
  free(s->corpse);
  // Drain any deferred frees still pending.
  for (u32 i = 0; i < s->n_deferred; i++) {
    if (s->deferred_free[i] != NULL) fol_clause_free(s->deferred_free[i]);
  }
  free(s->clauses);
  free(s->cweight);
  free(s->cpri);
  free(s->cmask);
  free(s->cpicked);
  free(s->csel);
  free(s->cmaxlits);
  free(s->celig_valid);
  free(s->cgoal);
  free(s->cfrozen);
  free(s->cnvars);
  free(s->active);
  free(s->passive);
  free(s->pheap);
  free(s->ueq);
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

// Symbol-count weight: every CTR node and every variable counts 1,
// plus 1 per literal.  The pick-best heap key -- smaller = simpler =
// picked earlier.
static u32 cnf_term_symcount(Term t) {
  if (term_tag(t) != TAG_CTR) return 1u;
  u32 w = 1u;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) w += cnf_term_symcount(term_ctr_at(t, i));
  return w;
}

static u32 cnf_clause_weight(const FolClause *c) {
  u32 w = 0u;
  for (u32 i = 0; i < c->n_lits; i++)
    w += 1u + cnf_term_symcount(c->lits[i].atom);
  return w;
}

// (root label, sign) bloom mask.  Clause a can multiset-subsume
// clause c only if every (label, sign) bit of a appears in c, i.e.
// mask[a] & ~mask[c] == 0.  A one-word necessary condition that
// prunes the O(n) subsumption walks before the exponential matcher.
static u64 cnf_clause_mask(const FolClause *c) {
  u64 m = 0u;
  for (u32 i = 0; i < c->n_lits; i++) {
    Term a = c->lits[i].atom;
    u32 lab = (term_tag(a) == TAG_CTR) ? term_ext(a) : 0xFFFFFFu;
    u32 h = lab * 2654435761u + (u32)c->lits[i].sign * 97u;
    m |= 1ull << (h & 63u);
  }
  return m;
}

// --- passive priority heap (min by (cweight, id)) -------------------

static u8 cnf_pq_less(const CnfState *s, u32 a, u32 b) {
  if (s->cpri[a] != s->cpri[b]) return (s->cpri[a] < s->cpri[b]) ? 1u : 0u;
  return (a < b) ? 1u : 0u;   // tie: older first
}

static void cnf_pq_push(CnfState *s, u32 id) {
  if (s->n_pheap >= s->cap_pheap) {
    cnf_grow(&s->cap_pheap, (u8 **)&s->pheap, sizeof(u32));
    if (s->n_pheap >= s->cap_pheap) return;   // OOM: FIFO lane still holds id
  }
  u32 i = s->n_pheap++;
  s->pheap[i] = id;
  while (i > 0u) {
    u32 parent = (i - 1u) / 2u;
    if (!cnf_pq_less(s, s->pheap[i], s->pheap[parent])) break;
    u32 tmp = s->pheap[i]; s->pheap[i] = s->pheap[parent]; s->pheap[parent] = tmp;
    i = parent;
  }
}

static u32 cnf_pq_pop(CnfState *s) {
  if (s->n_pheap == 0u) return ~0u;
  u32 top = s->pheap[0];
  s->n_pheap--;
  if (s->n_pheap > 0u) {
    s->pheap[0] = s->pheap[s->n_pheap];
    u32 i = 0u;
    for (;;) {
      u32 l = 2u * i + 1u, r = l + 1u, m = i;
      if (l < s->n_pheap && cnf_pq_less(s, s->pheap[l], s->pheap[m])) m = l;
      if (r < s->n_pheap && cnf_pq_less(s, s->pheap[r], s->pheap[m])) m = r;
      if (m == i) break;
      u32 tmp = s->pheap[i]; s->pheap[i] = s->pheap[m]; s->pheap[m] = tmp;
      i = m;
    }
  }
  return top;
}

// Internal: append clause + record trace metadata.  Used by every
// inference path so the provenance DAG stays consistent.
static i32 cnf_add_clause_traced(CnfState *s, FolClause *c,
                                 FolInference rule,
                                 u32 parent_a, u32 parent_b) {
  if (s == NULL || c == NULL) return -1;
  if (s->n >= s->cap) {
    // Grow the parallel per-clause arrays together, bumping cap only
    // when every realloc lands (each success is committed so a
    // partial failure can't leak).
    u32 nc = s->cap * 2u;
    void *na = realloc(s->clauses, (size_t)nc * sizeof(FolClause *));
    if (na != NULL) s->clauses = (FolClause **)na;
    void *nw = realloc(s->cweight, (size_t)nc * sizeof(u32));
    if (nw != NULL) s->cweight = (u32 *)nw;
    void *nq = realloc(s->cpri, (size_t)nc * sizeof(u32));
    if (nq != NULL) s->cpri = (u32 *)nq;
    void *nm = realloc(s->cmask, (size_t)nc * sizeof(u64));
    if (nm != NULL) s->cmask = (u64 *)nm;
    void *np = realloc(s->cpicked, (size_t)nc);
    if (np != NULL) s->cpicked = (u8 *)np;
    void *ns = realloc(s->csel, (size_t)nc * sizeof(u32));
    if (ns != NULL) s->csel = (u32 *)ns;
    void *nx = realloc(s->cmaxlits, (size_t)nc * sizeof(u64));
    if (nx != NULL) s->cmaxlits = (u64 *)nx;
    void *nv = realloc(s->celig_valid, (size_t)nc);
    if (nv != NULL) s->celig_valid = (u8 *)nv;
    void *ng = realloc(s->cgoal, (size_t)nc);
    if (ng != NULL) s->cgoal = (u8 *)ng;
    void *nf = realloc(s->cfrozen, (size_t)nc * sizeof(FolLit *));
    if (nf != NULL) s->cfrozen = (FolLit **)nf;
    void *nn = realloc(s->cnvars, (size_t)nc * sizeof(u32));
    if (nn != NULL) s->cnvars = (u32 *)nn;
    void *nd = NULL;
    if (s->corpse != NULL) {
      nd = realloc(s->corpse, (size_t)nc * sizeof(FolClause *));
      if (nd != NULL) s->corpse = (FolClause **)nd;
    }
    if (na == NULL || nw == NULL || nq == NULL || nm == NULL || np == NULL ||
        ns == NULL || nx == NULL || nv == NULL || ng == NULL ||
        nf == NULL || nn == NULL || (s->corpse != NULL && nd == NULL)) return -1;
    s->cap = nc;
    for (u32 i = s->n; i < s->cap; i++) s->clauses[i] = NULL;
    if (s->corpse != NULL)
      for (u32 i = s->n; i < s->cap; i++) s->corpse[i] = NULL;
  }
  while (s->n >= s->trace_cap) {
    cnf_grow(&s->trace_cap, (u8 **)&s->trace, sizeof(FolTrace));
    if (s->n >= s->trace_cap) return -1;
  }
  u32 id = s->n;
  s->clauses[id] = c;
  // Goal-relatedness inherits from either parent.  cweight stays the
  // TRUE symbol count (the subsumption prefilter depends on it); the
  // heap key cpri adds a large penalty to clauses with no conjecture
  // ancestry, so the (small) goal cone always pops first while the
  // FIFO age lane keeps axiom-side saturation fair.
  u8 goal = 0u;
  if (parent_a != FOL_NO_PARENT && s->cgoal[parent_a]) goal = 1u;
  if (parent_b != FOL_NO_PARENT && s->cgoal[parent_b]) goal = 1u;
  u32 w = cnf_clause_weight(c);
  s->cgoal[id]   = goal;
  s->cweight[id] = w;
  s->cpri[id]    = w + (goal ? 0u : CNF_NONGOAL_PENALTY);
  s->cmask[id]   = cnf_clause_mask(c);
  s->cpicked[id] = 0u;
  s->celig_valid[id] = 0u;
  // Freeze once: the rename-apart target-literal copy every later
  // subsumption test against this clause reuses.
  s->cfrozen[id] = (FolLit *)malloc(sizeof(FolLit) * (size_t)(c->n_lits ? c->n_lits : 1u));
  if (s->cfrozen[id] != NULL) fol_freeze_lits(c, s->cfrozen[id]);
  s->cnvars[id]  = fol_clause_nvars(c);
  s->trace[id].rule     = rule;
  s->trace[id].parent_a = parent_a;
  s->trace[id].parent_b = parent_b;
  s->n++;
  if (s->n_passive >= s->cap_passive) {
    cnf_grow(&s->cap_passive, (u8 **)&s->passive, sizeof(u32));
    if (s->n_passive >= s->cap_passive) return -1;
  }
  s->passive[s->n_passive++] = id;   // age (FIFO) lane
  cnf_pq_push(s, id);                // pick-best lane
  return (i32)id;
}

// Public entry point: user-added clauses get the INPUT marker.
fn i32 cnf_add_clause(CnfState *s, FolClause *c) {
  fol_clause_canon_vars(c);
  return cnf_add_clause_traced(s, c, FOL_INF_INPUT,
                                FOL_NO_PARENT, FOL_NO_PARENT);
}

fn i32 cnf_add_clause_goal(CnfState *s, FolClause *c, u8 goal_related) {
  i32 id = cnf_add_clause(s, c);
  if (id >= 0 && goal_related) {
    s->cgoal[id] = 1u;
    s->cpri[id]  = s->cweight[id];   // drop the non-goal penalty
    // The heap entry was pushed with the penalized key; the goal key
    // is only SMALLER, so correct it by re-pushing -- the stale
    // duplicate is skipped via cpicked when it surfaces later.
    cnf_pq_push(s, (u32)id);
  }
  return id;
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

// The per-clause eligibility cache derives from the ordering +
// selection config; any config change invalidates it wholesale.
static void cnf_elig_invalidate(CnfState *s) {
  if (s->celig_valid != NULL && s->n > 0u) memset(s->celig_valid, 0, s->n);
}

fn void cnf_set_retain_dead(CnfState *s) {
  if (s == NULL || s->retain_dead) return;
  s->corpse = (FolClause **)calloc(s->cap, sizeof(FolClause *));
  if (s->corpse != NULL) s->retain_dead = 1u;
}

fn const FolClause *cnf_clause_at(const CnfState *s, u32 id) {
  if (s == NULL || id >= s->n) return NULL;
  if (s->clauses[id] != NULL) return s->clauses[id];
  return (s->corpse != NULL) ? s->corpse[id] : NULL;
}

fn void cnf_set_kbo(CnfState *s, const KboConfig *kbo) { if (s) { s->cnf_kbo = kbo; cnf_elig_invalidate(s); } }
fn void cnf_set_lpo(CnfState *s, const LpoConfig *lpo) { if (s) { s->cnf_lpo = lpo; cnf_elig_invalidate(s); } }
fn void cnf_set_rpo(CnfState *s, const RpoConfig *rpo) { if (s) { s->cnf_rpo = rpo; cnf_elig_invalidate(s); } }
fn void cnf_set_wpo(CnfState *s, const WpoConfig *wpo) { if (s) { s->cnf_wpo = wpo; cnf_elig_invalidate(s); } }
fn void cnf_set_select(CnfState *s, CnfSelection sel) { if (s) { s->cnf_select = sel; cnf_elig_invalidate(s); } }

// --- term-arena junk reclamation ------------------------------------
// Candidate inference generation allocates renamed copies, failed
// unifier applications and rewrite intermediates on the bump arena
// even when the inference fails or the redundancy filters drop the
// result.  Whenever an operation is known to have retained NO new
// term (nothing stored in the clause set), everything allocated
// after its entry mark is unreachable -- roll the bump pointer back.
// Guarded against an interleaved GC pass, which compacts the arena
// and re-anchors HEAP_NEXT below the mark.  The KBO per-term memo
// keys off cell locs, so bump its epoch: a reused loc must not serve
// a stale weight/hash entry.
static void cnf_reclaim(u64 mark, u64 gc_snap) {
  if (gc_count() == gc_snap && HEAP_NEXT > mark) {
    HEAP_NEXT = mark;
    thvm_kbo_invalidate();
  }
}

// Reclaim after a consider: only when the candidate was NOT stored
// (s->n unchanged) and the loop is still running.
static void cnf_reclaim_consider(CnfState *s, u32 n_before,
                                 u64 mark, u64 gc_snap) {
  if (s->n == n_before && s->status == ATP_RUNNING) cnf_reclaim(mark, gc_snap);
}

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

// Ordered-inference gate: literal i of `c` is maximal iff no other
// literal's atom is strictly greater under the attached ordering
// (incomparable counts as not-greater -- the sound over-
// approximation; ordering stability under substitution makes the
// pre-unification check a safe restriction).  Without an ordering
// every literal is maximal, i.e. inference stays unrestricted.
static u8 cnf_lit_maximal(const CnfState *s, const FolClause *c, u32 i) {
  if (!cnf_has_ordering(s)) return 1u;
  for (u32 j = 0; j < c->n_lits; j++) {
    if (j == i) continue;
    if (cnf_compare(s, c->lits[j].atom, c->lits[i].atom) == KBO_GT) return 0u;
  }
  return 1u;
}

// Fill the per-clause eligibility cache: selected literal +
// maximal-literal bitmask (FOL_MAX_LITS = 64 fits a u64 exactly).
// Static per clause for a fixed config, so the O(lits^2) ordering
// scan runs once per clause, not once per candidate pair.
static void cnf_elig_ensure(CnfState *s, u32 id) {
  if (s->celig_valid[id]) return;
  const FolClause *c = s->clauses[id];
  if (c == NULL) return;   // dead slot: callers skip NULL clauses
  s->csel[id] = cnf_select_lit(s, c);
  u64 m = 0ull;
  for (u32 i = 0; i < c->n_lits && i < 64u; i++) {
    if (cnf_lit_maximal(s, c, i)) m |= 1ull << i;
  }
  s->cmaxlits[id] = m;
  s->celig_valid[id] = 1u;
}

// Eligibility under selection + ordering: when the selection policy
// picked a literal, only that literal participates; otherwise the
// literal must be maximal.  The standard "eligible literal" of
// ordered resolution / superposition.
static u8 cnf_elig(CnfState *s, u32 id, u32 i) {
  cnf_elig_ensure(s, id);
  if (s->csel[id] != ~0u) return (i == s->csel[id]) ? 1u : 0u;
  return (i < 64u) ? (u8)((s->cmaxlits[id] >> i) & 1ull) : 1u;
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
    // Only active unit positive equalities can demodulate; walk the
    // dedicated rule list instead of the whole active set.
    for (u32 i = 0; i < s->n_ueq; i++) {
      FolClause *rule = s->clauses[s->ueq[i]];
      if (rule == NULL) continue;
      // Skip when target IS the rule (no self-rewrite).
      if (rule == c) continue;
      u64 mark = HEAP_NEXT, gc_snap = gc_count();
      FolClause *d = cnf_demodulate_ordered(s, rule, c);
      if (d == NULL) { cnf_reclaim(mark, gc_snap); continue; }
      fol_clause_free(c);
      c = d;
      // An unorientable rule's RHS can inject renamed-range
      // variable ids; keep the working clause canonical.
      fol_clause_canon_vars(c);
      changed = 1u;
      break;
    }
    if (!changed) break;
  }
  *pc = c;
}

// Internal: check whether `c` is subsumed by any currently-stored
// clause (every live clause is active or passive; the store covers
// both).  Used as a forward-redundancy filter on derived clauses.
// The bloom-mask + literal-count prefilters cut most candidates
// before the exponential matcher (and before its per-call literal
// renames hit the term heap).
static u8 cnf_is_subsumed(const CnfState *s, const FolClause *c) {
  if (c->n_lits > FOL_MAX_LITS) return 0u;
  u64 cm = cnf_clause_mask(c);
  u32 cw = cnf_clause_weight(c);
  u64 mark = HEAP_NEXT, gc_snap = gc_count();
  // Freeze the candidate ONCE; every surviving pattern reuses it.
  FolLit frozen[FOL_MAX_LITS];
  fol_freeze_lits(c, frozen);
  u8 hit = 0u;
  for (u32 i = 0; i < s->n; i++) {
    const FolClause *a = s->clauses[i];
    if (a == NULL) continue;
    if (a->n_lits > c->n_lits) continue;
    if (s->cweight[i] > cw) continue;   // substitution never shrinks
    if ((s->cmask[i] & ~cm) != 0ull) continue;
    if (fol_subsumes_frozen_nv(a, frozen, c->n_lits, s->cnvars[i])) { hit = 1u; break; }
  }
  // The matcher retains nothing -- its renames are pure scratch.
  cnf_reclaim(mark, gc_snap);
  return hit;
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
    u32 n_before = s->n;
    u64 mark = HEAP_NEXT, gc_snap = gc_count();
    FolClause *d = cnf_demodulate_ordered(s, rule, old);
    if (d == NULL) { cnf_reclaim(mark, gc_snap); continue; }
    fol_clause_canon_vars(d);

    // Clobber the old slot; defer free (or retain as a corpse so a
    // proof emitter can still print the cited parent).
    s->clauses[i] = NULL;
    if (s->retain_dead) {
      s->corpse[i] = old;
    } else {
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
        cnf_reclaim(mark, gc_snap);
        continue;
      }
    }

    // Push d through tautology + subsumption (forward-demod is
    // intentionally skipped here -- see comment above).
    if (fol_clause_is_empty(d)) {
      cnf_add_clause_traced(s, d, FOL_INF_DEMOD, new_id, i);
      s->status = ATP_PROVED;
      return;
    }
    if (fol_is_tautology(d)) { fol_clause_free(d); cnf_reclaim(mark, gc_snap); continue; }
    if (cnf_is_subsumed(s, d)) { fol_clause_free(d); cnf_reclaim(mark, gc_snap); continue; }
    cnf_add_clause_traced(s, d, FOL_INF_DEMOD, new_id, i);
    cnf_reclaim_consider(s, n_before, mark, gc_snap);   // add may have failed
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
  u64 nm = s->cmask[new_id];
  u64 mark = HEAP_NEXT, gc_snap = gc_count();
  for (u32 i = 0; i < s->n; i++) {
    if (i == new_id) continue;
    FolClause *old = s->clauses[i];
    if (old == NULL) continue;
    if (new_c->n_lits > old->n_lits) continue;
    if (s->cweight[new_id] > s->cweight[i]) continue;
    if ((nm & ~s->cmask[i]) != 0ull) continue;
    u8 sub = (s->cfrozen[i] != NULL)
               ? fol_subsumes_frozen_nv(new_c, s->cfrozen[i], old->n_lits,
                                        s->cnvars[new_id])
               : fol_subsumes(new_c, old);
    if (sub) {
      s->clauses[i] = NULL;
      if (s->retain_dead) {
        s->corpse[i] = old;
        continue;
      }
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
  // Subsumption scratch (per-candidate renames) retains nothing.
  cnf_reclaim(mark, gc_snap);
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
  // Canonize FIRST: binary inferences leave second-premise variables
  // in the renamed range; every downstream consumer (demod, the
  // filters, later inferences on the stored clause) needs dense ids.
  fol_clause_canon_vars(c);
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
// policy finds no matching lit); when an ordering is attached the
// fallback tightens to MAXIMAL literals -- ordered resolution with
// selection.
static void cnf_gen_resolution(CnfState *s, u32 a_id, u32 b_id) {
  const FolClause *a = s->clauses[a_id];
  const FolClause *b = s->clauses[b_id];
  if (a == NULL || b == NULL) return;
  for (u32 i = 0; i < a->n_lits; i++) {
    if (!cnf_elig(s, a_id, i)) continue;
    for (u32 j = 0; j < b->n_lits; j++) {
      if (a->lits[i].sign == b->lits[j].sign) continue;  // need complementary
      // Cheap unify prefilter: two CTR atoms with different root
      // labels can never unify -- skip before fol_resolve's rename
      // allocates on the term heap.
      if (term_tag(a->lits[i].atom) == TAG_CTR &&
          term_tag(b->lits[j].atom) == TAG_CTR &&
          term_ext(a->lits[i].atom) != term_ext(b->lits[j].atom)) continue;
      if (!cnf_elig(s, b_id, j)) continue;
      u32 n0 = s->n;
      u64 mark = HEAP_NEXT, gc_snap = gc_count();
      FolClause *r = fol_resolve(a, i, b, j);
      cnf_consider(s, r, FOL_INF_RESOLVE, a_id, b_id);
      cnf_reclaim_consider(s, n0, mark, gc_snap);
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
      u32 n0 = s->n;
      u64 mark = HEAP_NEXT, gc_snap = gc_count();
      FolClause *r = fol_factor(c, i, j);
      cnf_consider(s, r, FOL_INF_FACTOR, c_id, FOL_NO_PARENT);
      cnf_reclaim_consider(s, n0, mark, gc_snap);
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
    u32 n0 = s->n;
    u64 mark = HEAP_NEXT, gc_snap = gc_count();
    FolClause *r = fol_reflex_resolve(c, i);
    cnf_consider(s, r, FOL_INF_REFLEX, c_id, FOL_NO_PARENT);
    cnf_reclaim_consider(s, n0, mark, gc_snap);
    if (s->status != ATP_RUNNING) return;
  }
}

// Recursive position walker: invokes `cb` at every non-variable
// position inside `atom`, passing the current path.  Caps depth at
// CP_MAX_DEPTH (the existing cp/_.c walker's bound -- positions
// stored on the CP are tagged with a u8 array of length CP_MAX_DEPTH,
// reusing the same convention here keeps the depth budget consistent).
typedef void (*CnfPosCb)(const u32 *p, u32 p_len, Term sub, void *ctx);

static void cnf_walk_positions(Term t, u32 *path, u32 depth,
                               CnfPosCb cb, void *ctx) {
  if (term_tag(t) != TAG_CTR) return;
  cb(path, depth, t, ctx);
  if (depth >= CP_MAX_DEPTH) return;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) {
    path[depth] = i;
    cnf_walk_positions(term_ctr_at(t, i), path, depth + 1u, cb, ctx);
  }
}

// Paramodulation visitor context: emits a CP for each visited
// position of `target_atom`.  allow_fwd / allow_rev carry the
// ordering gate on the equation's orientation (s->t / t->s).
typedef struct {
  CnfState        *s;
  const FolClause *eq_clause;
  u32              eq_idx;
  u32              eq_id;        // for the trace
  const FolClause *target;
  u32              target_idx;
  u32              target_id;    // for the trace
  u8               allow_fwd;    // orientation s -> t permitted
  u8               allow_rev;    // orientation t -> s permitted
  Term             es;           // eq literal side 0 (unrenamed)
  Term             et;           // eq literal side 1 (unrenamed)
} CnfParamodCtx;

// Root-label unify prefilter: the visited subterm is always a CTR
// node (the walker skips variables); a CTR from-side with a different
// root label can never unify with it.  Renaming preserves labels, so
// checking the UNRENAMED sides is exact -- and skips the per-position
// rename + unification entirely.
static u8 cnf_paramod_root_ok(Term side, Term sub) {
  if (term_tag(side) != TAG_CTR) return 1u;   // variable side: anything goes
  return (term_ext(side) == term_ext(sub)) ? 1u : 0u;
}

static void cnf_paramod_at_pos(const u32 *p, u32 p_len, Term sub, void *raw) {
  CnfParamodCtx *ctx = (CnfParamodCtx *)raw;
  if (ctx->s->status != ATP_RUNNING) return;
  // Both orientations (s->t, t->s) are needed for refutation
  // completeness UNLESS the ordering already orients the equation --
  // then only the larger-side-to-smaller rewrite fires (ordered
  // paramodulation; the reverse instance can never be maximal).
  if (ctx->allow_fwd && cnf_paramod_root_ok(ctx->es, sub)) {
    u32 n0 = ctx->s->n;
    u64 mark = HEAP_NEXT, gc_snap = gc_count();
    FolClause *r0 = fol_paramodulate(ctx->eq_clause, ctx->eq_idx, /*swap*/ 0,
                                     ctx->target, ctx->target_idx, p, p_len);
    cnf_consider(ctx->s, r0, FOL_INF_PARAMOD, ctx->eq_id, ctx->target_id);
    cnf_reclaim_consider(ctx->s, n0, mark, gc_snap);
    if (ctx->s->status != ATP_RUNNING) return;
  }
  if (ctx->allow_rev && cnf_paramod_root_ok(ctx->et, sub)) {
    u32 n0 = ctx->s->n;
    u64 mark = HEAP_NEXT, gc_snap = gc_count();
    FolClause *r1 = fol_paramodulate(ctx->eq_clause, ctx->eq_idx, /*swap*/ 1,
                                     ctx->target, ctx->target_idx, p, p_len);
    cnf_consider(ctx->s, r1, FOL_INF_PARAMOD, ctx->eq_id, ctx->target_id);
    cnf_reclaim_consider(ctx->s, n0, mark, gc_snap);
  }
}

// All paramodulation results of eq_clause (as the source of an
// equality literal) into target (any literal, any non-variable
// position -- the position walker never descends to variables).
//
// Selection: paramod only fires INTO the target's selected literal
// when one is picked (else its maximal literals under an attached
// ordering).  The SOURCE clause must have no selected literal --
// a clause whose selection picked a negative literal only resolves
// on it (the superposition positive-premise discipline) -- and the
// equality literal itself must be maximal.
static void cnf_gen_paramod(CnfState *s, u32 eq_id, u32 target_id) {
  const FolClause *eqc = s->clauses[eq_id];
  const FolClause *tgt = s->clauses[target_id];
  if (eqc == NULL || tgt == NULL) return;
  cnf_elig_ensure(s, eq_id);
  u32 sel_e = s->csel[eq_id];
  if (sel_e != ~0u && eqc->lits[sel_e].sign == 1u) return;
  for (u32 i = 0; i < eqc->n_lits; i++) {
    if (eqc->lits[i].sign != 0u) continue;          // positive equalities only
    if (!fol_atom_is_eq(eqc->lits[i].atom)) continue;
    if (i < 64u && ((s->cmaxlits[eq_id] >> i) & 1ull) == 0ull) continue;
    // Ordering gate on the equation's own sides: only rewrite from a
    // side that is not strictly smaller.  KBO stability under
    // substitution makes the pre-unification check a sound
    // restriction; incomparable sides keep both orientations.
    Term es = term_ctr_at(eqc->lits[i].atom, 0u);
    Term et = term_ctr_at(eqc->lits[i].atom, 1u);
    u8 allow_fwd = 1u, allow_rev = 1u;
    if (cnf_has_ordering(s)) {
      KboCmp cmp = cnf_compare(s, es, et);
      if (cmp == KBO_GT) allow_rev = 0u;
      else if (cmp == KBO_LT) allow_fwd = 0u;
    }
    for (u32 j = 0; j < tgt->n_lits; j++) {
      if (!cnf_elig(s, target_id, j)) continue;
      // Skip paramodulating an equality literal into itself when
      // eq_id == target_id and i == j (same literal in same clause).
      if (eq_id == target_id && i == j) continue;
      CnfParamodCtx ctx = {
        .s = s, .eq_clause = eqc, .eq_idx = i, .eq_id = eq_id,
        .target = tgt, .target_idx = j, .target_id = target_id,
        .allow_fwd = allow_fwd, .allow_rev = allow_rev,
        .es = es, .et = et,
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
      u32 n0 = s->n;
      u64 mark = HEAP_NEXT, gc_snap = gc_count();
      FolClause *r = fol_eq_factor(c, i, j);
      cnf_consider(s, r, FOL_INF_EQ_FACTOR, c_id, FOL_NO_PARENT);
      cnf_reclaim_consider(s, n0, mark, gc_snap);
      if (s->status != ATP_RUNNING) return;
    }
  }
}

// Pop the next given clause.  Default lane: the pick-best heap
// (smallest weight first).  Every age_ratio-th pick the FIFO lane
// supplies the OLDEST surviving passive clause instead, so heavy
// clauses can't starve (fairness).  Entries consumed by the other
// lane (cpicked) or clobbered by backward subsumption (NULL slot)
// are skipped lazily.  Returns ~0u when both lanes are empty.
static u32 cnf_pop_passive(CnfState *s) {
  s->pick_count++;
  u8 age_turn = (s->age_ratio != 0u &&
                 (s->pick_count % s->age_ratio) == 0u) ? 1u : 0u;
  if (!age_turn) {
    for (;;) {
      u32 id = cnf_pq_pop(s);
      if (id == ~0u) break;
      if (s->clauses[id] != NULL && !s->cpicked[id]) {
        s->cpicked[id] = 1u;
        return id;
      }
    }
  }
  while (s->passive_head < s->n_passive) {
    u32 id = s->passive[s->passive_head++];
    if (s->clauses[id] != NULL && !s->cpicked[id]) {
      s->cpicked[id] = 1u;
      return id;
    }
  }
  // Age turn with a drained FIFO: fall back to the heap.
  for (;;) {
    u32 id = cnf_pq_pop(s);
    if (id == ~0u) break;
    if (s->clauses[id] != NULL && !s->cpicked[id]) {
      s->cpicked[id] = 1u;
      return id;
    }
  }
  return ~0u;
}

// One Otter-style given-clause step.
fn AtpStatus cnf_step(CnfState *s) {
  if (s == NULL) return ATP_ABORTED;
  if (s->status != ATP_RUNNING) return s->status;
  if (s->step >= s->step_cap) {
    s->status = ATP_ABORTED;
    return s->status;
  }

  u32 given_id = cnf_pop_passive(s);
  if (given_id == ~0u) {
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

  // Register unit positive equalities on the demod rule list.
  {
    const FolClause *g = s->clauses[given_id];
    if (g != NULL && g->n_lits == 1u && g->lits[0].sign == 0u &&
        fol_atom_is_eq(g->lits[0].atom)) {
      if (s->n_ueq >= s->cap_ueq) {
        cnf_grow(&s->cap_ueq, (u8 **)&s->ueq, sizeof(u32));
      }
      if (s->n_ueq < s->cap_ueq) s->ueq[s->n_ueq++] = given_id;
    }
  }

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
