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
// unit positive equalities instead of the whole active set.
//
// Literal index (THVM_FOL_INDEX=0 disables): three bucket families
// keyed by (atom root label, sign) replace the two scans that
// dominate 20k+-clause runs (measured on the AGT011+2 pre-pass) --
//   * resolution retrieval: each ACTIVE clause's ELIGIBLE literals
//     (the selected one when the policy picked, else every maximal
//     one) are listed under their key; the given clause's partners
//     come from the complementary buckets, visited in ACTIVATION
//     order, so the candidate set and the generated-clause order are
//     byte-identical to the full cross-scan (a pair with no
//     complementary eligible root-label match generates nothing
//     there: fol_resolve requires complementary signs, and two CTR
//     atoms with different roots never unify).  Clauses with an
//     eligible non-CTR atom (or a literal index past the 8-bit
//     packing) go on an always-visited fallback list; a GIVEN clause
//     with such a literal -- or with an eligible positive equality,
//     whose paramodulation TARGETS are label-unconstrained -- falls
//     back to the full scan for that step.  Active paramod sources
//     live on their own list (idx_peq), so a given without equality
//     literals skips the whole paramodulation sweep.
//   * forward subsumption: every stored clause is listed once under
//     its FIRST literal's key (non-CTR/absent -> wildcard list).  A
//     subsumer maps each of its literals onto a same-key literal of
//     the candidate (substitution preserves a CTR root and the
//     sign), so scanning the buckets of the candidate's keys plus
//     the wildcard list reaches every possible subsumer.  Verdict-
//     identical: existence is scan-order independent.
//   * backward subsumption: every stored clause is listed under each
//     DISTINCT literal key it contains; a fresh clause retrieves the
//     bucket of its own first literal's key.  Entries append in id
//     order, so the visit order matches the full 0..n walk on a
//     candidate superset -- the NULLed-slot set comes out identical.
// The subsumption families are eligibility-independent; the
// resolution family is rebuilt from the active list after any
// cnf_set_* config change (lazily, at the next step).  Any index
// allocation failure permanently clears idx_on: the full scans are
// the ground truth and need no index state.
//
// AVATAR-lite component splitting (cnf_set_avatar / THVM_FOL_AVATAR;
// engine default OFF, the FOF clausal pre-pass turns it on):
//   * SPLIT.  A stored clause C whose literals partition into m >= 2
//     VARIABLE-DISJOINT components C_1 | ... | C_m (no variable spans
//     two parts -- the sound splitting criterion; ground literals are
//     singleton components, so ground splitting is the special case)
//     is replaced by m component clauses, each guarded by a
//     propositional assumption a_g ("component g holds"):  C_g is
//     stored with assumption set {a_g}, and the case analysis is
//     recorded as the SAT split clause  ~A -> a_g1 v ... v a_gm
//     (A = C's own assumption set).  C itself stays in the store as a
//     provenance-only record (cpicked, never given).  Components are
//     memoized by content: a re-derived component reuses its guard
//     (Vampire's component index), so the guard supply
//     (CNF_AVATAR_MAX_GUARDS) is spent
//     on distinct components only; a clause needing fresh guards past
//     the cap stores unsplit.
//   * ASSUMPTION TRACKING.  Every inference conclusion carries the
//     union of its parents' assumption sets (u64 bitset).  The
//     simplifiers respect assumptions: clause D (assumptions B) may
//     subsume / demodulate clause C (assumptions A) only when
//     B is a subset of A -- then (/\B -> D) entails the redundancy of
//     (/\A -> C) exactly as in the unconditional calculus.
//   * CLOSURE.  An empty clause with EMPTY assumptions refutes
//     outright (unchanged).  An empty clause under assumptions A
//     records the SAT refutation clause ~(/\A) -- and, being an empty
//     clause that subsumes under the B-subset gate, sweeps every
//     stored clause of that refuted branch out of the store.  After
//     each new refutation clause a node-budgeted DPLL (unit
//     propagation + branch on a shortest open clause) decides the
//     guard theory {split clauses} u {refutation clauses}; UNSAT =>
//     the case analysis is exhausted => ATP_PROVED.
//   * MODEL-GUIDED ACTIVATION (full AVATAR's focusing discipline).
//     When the DPLL run is satisfiable it returns a MODEL (TRUE-guard
//     bitset; unassigned guards default TRUE, so guards allocated
//     after the run enter TRUE).  A clause is ACTIVE iff every guard
//     of its assumption set is TRUE under the current model -- the
//     saturation works one branch combination at a time.  Inactive
//     clauses are skipped as given (parked, NOT consumed) and as
//     inference partners; they re-enter both pick lanes when a model
//     change flips their guards.  Conclusions inherit their parents'
//     assumptions, so clauses are always active at birth; parking
//     only begins once a branch refutation moves the model.  The
//     model is refreshed whenever the theory strengthens against it:
//     after every refutation clause (the refuted branch is a subset
//     of the current model, so the model always moves or the theory
//     closes) and after a recorded split event the model happens to
//     falsify (memoized-component splits; fresh-guard splits are
//     satisfied by the TRUE default).  A DPLL node-budget trip or
//     OOM falls back to the all-TRUE keep-all model: every clause
//     stays active, which is the pre-activation behavior.
//   * TERMINATION / COMPLETENESS.  Soundness never depends on the
//     model (activation only steers the search; every inference
//     stays assumption-tracked).  The SAT theory only strengthens:
//     refutation masks form a growing antichain and each new one
//     excludes the current model permanently, so a model, once
//     abandoned for a refutation, never returns -- over finitely many
//     guards
//     the branch case analysis is finite and either some branch
//     combination saturates (a genuine finite model of the input:
//     ATP_QUEUE_EMPTY, reported as saturation exactly as before) or
//     the theory closes (ATP_PROVED).  Within one model the loop is
//     the ordinary fair given-clause saturation.
//   * SOUNDNESS.  Each stored clause with assumption set A stands for
//     /\A -> C, where a_g stands for the (closed) formula ALL(C_g).
//     Splitting is sound because variable-disjointness makes
//     ALL(C_1 | ... | C_m) equivalent to ALL(C_1) v ... v ALL(C_m):
//     a model satisfies the disjunction clause-wide iff it satisfies
//     some disjunct uniformly -- no instance can mix disjuncts, as no
//     variable crosses components.  Every recorded SAT clause is thus
//     a consequence of the input set: their propositional
//     unsatisfiability over the a_g witnesses the input's
//     unsatisfiability.
//
// Memory: the state OWNS every FolClause* it stores.  Free via
// cnf_free.

#define CNF_DEFAULT_CAP 64u
#define CNF_AGE_RATIO_DEFAULT 5u

static void cnf_idx_teardown(FolIdx *ix);
static void cnf_idx_disable(CnfState *s);
static void cnf_backward_subsume(CnfState *s, u32 new_id);
static void cnf_empty_derived(CnfState *s, FolClause *c, FolInference rule,
                              u32 parent_a, u32 parent_b);
// Heap-key penalty for clauses with no conjecture ancestry: goal
// descendants form a small cone and always pop first.
// THVM_FOL_GOAL_TIER=0 drops the penalty (flat weight order).
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
  s->cassume     = (CnfGmask *)calloc(s->cap, sizeof(CnfGmask));
  s->cguard      = (u32 *)calloc(s->cap, sizeof(u32));
  cnf_gm_fill(&s->model);   // all guards TRUE: everything active
  s->cap_parked  = CNF_DEFAULT_CAP;
  s->parked      = (u32 *)calloc(s->cap_parked, sizeof(u32));
  s->cstale      = (u8 *)calloc(s->cap, 1);
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
      s->cnvars == NULL || s->cassume == NULL || s->cguard == NULL ||
      s->parked == NULL || s->cstale == NULL ||
      s->active == NULL || s->passive == NULL ||
      s->pheap == NULL || s->ueq == NULL || s->trace == NULL ||
      s->deferred_free == NULL) {
    free(s->clauses); free(s->cweight); free(s->cpri); free(s->cmask); free(s->cpicked);
    free(s->csel); free(s->cmaxlits); free(s->celig_valid); free(s->cgoal);
    free(s->cfrozen); free(s->cnvars); free(s->cassume); free(s->cguard);
    free(s->parked); free(s->cstale);
    free(s->active); free(s->passive); free(s->pheap); free(s->ueq);
    free(s->trace); free(s->deferred_free); free(s);
    return NULL;
  }
  s->age_ratio = CNF_AGE_RATIO_DEFAULT;
  {
    const char *e = getenv("THVM_FOL_AGE_RATIO");
    if (e != NULL) s->age_ratio = (u32)atoi(e);
  }
  s->goal_tier = 1u;
  {
    const char *e = getenv("THVM_FOL_GOAL_TIER");
    if (e != NULL && e[0] == '0') s->goal_tier = 0u;
  }
  s->idx_on = 1u;
  {
    const char *e = getenv("THVM_FOL_INDEX");
    if (e != NULL && e[0] == '0') s->idx_on = 0u;
  }
  // AVATAR-lite: engine default OFF; THVM_FOL_AVATAR=1 turns it on for
  // every CnfState (the both-modes test lever; =2 scopes guard spend
  // to input + conjecture-descended clauses), and the FOF clausal
  // pre-pass enables it explicitly via cnf_set_avatar unless =0.
  {
    const char *e = getenv("THVM_FOL_AVATAR");
    s->avatar_on = (e == NULL) ? 0u : (u8)atoi(e);
  }
  if (s->idx_on) {
    s->cact   = (u32 *)calloc(s->cap, sizeof(u32));
    s->cpeq   = (u8 *)calloc(s->cap, 1);
    s->cfbl   = (u8 *)calloc(s->cap, 1);
    s->cstamp = (u32 *)calloc(s->cap, sizeof(u32));
    if (s->cact == NULL || s->cpeq == NULL || s->cfbl == NULL ||
        s->cstamp == NULL) cnf_idx_disable(s);
  }
  s->mask_fine = s->idx_on;
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
  free(s->cassume);
  free(s->cguard);
  free(s->split_pos);
  free(s->split_neg);
  free(s->split_orig);
  free(s->refut_mask);
  free(s->refut_id);
  free(s->parked);
  free(s->cstale);
  free(s->active);
  free(s->passive);
  free(s->pheap);
  free(s->ueq);
  free(s->deferred_free);
  free(s->trace);
  cnf_idx_teardown(&s->idx_res);
  cnf_idx_teardown(&s->idx_fsub);
  cnf_idx_teardown(&s->idx_bsub);
  cnf_idx_teardown(&s->idx_bsub_f);
  free(s->idx_peq);
  free(s->idx_fb);
  free(s->idx_fsub_wc);
  free(s->cact);
  free(s->cpeq);
  free(s->cfbl);
  free(s->cstamp);
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

// Argument-fingerprint mask (the mask_fine family, latched with the
// index at init): the syntactic matcher maps a pattern literal onto a
// target literal root-for-root, argument position by position, so
// every σ-stable pattern feature must be present verbatim in the
// target --
//   coarse   (label, sign)                  every literal
//   fine     (label, sign, pos, arg root)   every CTR argument
// A pattern's variable argument requires nothing (it binds anything)
// and a frozen target variable satisfies nothing (a pattern CTR never
// matches it), so ONE bit set works for both roles and the legacy
// subset test stays: mask[a] & ~mask[c] != 0 refutes "a subsumes c".
// The fine bits are what discriminate the AGT-class flood clauses,
// which share their (label, sign) multiset and differ only in ground
// constants.
static u64 cnf_clause_mask_fine(const FolClause *c) {
  u64 m = 0u;
  for (u32 i = 0; i < c->n_lits; i++) {
    Term a = c->lits[i].atom;
    u32 lab = (term_tag(a) == TAG_CTR) ? term_ext(a) : 0xFFFFFFu;
    u32 h = lab * 2654435761u + (u32)c->lits[i].sign * 97u;
    m |= 1ull << (h & 63u);
    if (term_tag(a) != TAG_CTR) continue;
    u32 n = term_ctr_n(a);
    for (u32 p = 0; p < n; p++) {
      Term ap = term_ctr_at(a, p);
      if (term_tag(ap) != TAG_CTR) continue;
      u32 hf = (h ^ ((p + 1u) * 0x85EBCA6Bu)) + term_ext(ap) * 2246822519u;
      m |= 1ull << (hf & 63u);
    }
  }
  return m;
}

static u64 cnf_mask_for(const CnfState *s, const FolClause *c) {
  return s->mask_fine ? cnf_clause_mask_fine(c) : cnf_clause_mask(c);
}

// --- literal index (see the header comment) --------------------------

// Bucket key for a literal: atoms are CTR terms (callers route
// non-CTR atoms to the wildcard/fallback paths before keying).
static u64 cnf_idx_key(Term atom, u8 sign) {
  return 1ull + (((u64)term_ext(atom) << 1) | (u64)sign);
}

// Fine subsumption key: (root label, sign, arg0 fingerprint).  The
// fingerprint is the first argument's root label, or CNF_ARG_VAR for
// a variable argument, or CNF_ARG_NONE for arity 0.  Substitution
// preserves a CTR argument's root and can only instantiate VAR, so
//   pattern arg0 = CTR(r)  ->  matched target arg0 = CTR(r)
//   pattern arg0 = VAR     ->  matched target arg0 = anything
// which the two retrieval sides below exploit (target vars are
// frozen: a pattern CTR argument never matches them).
#define CNF_ARG_VAR  0xFFFFFFFEull
#define CNF_ARG_NONE 0xFFFFFFFDull

static u64 cnf_idx_fine_key(Term atom, u8 sign, u64 arg0) {
  u64 h = (((u64)term_ext(atom) << 1) | (u64)sign) + 0x9E37u;
  h = h * 0x100000001B3ull ^ arg0;
  return 1ull + (h << 8 >> 8);   // keep clear of the 0 = empty marker
}

// The literal's own arg0 fingerprint (insert side / pattern side).
static u64 cnf_idx_arg0(Term atom) {
  if (term_ctr_n(atom) == 0u) return CNF_ARG_NONE;
  Term a0 = term_ctr_at(atom, 0u);
  return (term_tag(a0) == TAG_CTR) ? (u64)term_ext(a0) : CNF_ARG_VAR;
}

static void cnf_idx_reset(FolIdx *ix) {
  for (u32 i = 0; i < ix->cap; i++) ix->slots[i].n = 0u;
}

static void cnf_idx_teardown(FolIdx *ix) {
  for (u32 i = 0; i < ix->cap; i++) free(ix->slots[i].ents);
  free(ix->slots);
  ix->slots = NULL;
  ix->cap = ix->n_used = 0u;
}

// Find the bucket for `key`; when `create` is set, claim an empty
// slot (growing the table past 75% load).  Returns NULL when absent
// (create == 0) or on allocation failure (caller disables idx_on).
static FolIdxBucket *cnf_idx_bucket(FolIdx *ix, u64 key, u8 create) {
  if (ix->cap == 0u) {
    if (!create) return NULL;
    ix->cap = 256u;
    ix->slots = (FolIdxBucket *)calloc(ix->cap, sizeof(FolIdxBucket));
    if (ix->slots == NULL) { ix->cap = 0u; return NULL; }
  }
  for (;;) {
    u32 mask = ix->cap - 1u;
    u32 i = (u32)(key * 0x9E3779B97F4A7C15ull >> 32) & mask;
    for (;;) {
      FolIdxBucket *b = &ix->slots[i];
      if (b->key == key) return b;
      if (b->key == 0ull) {
        if (!create) return NULL;
        if (ix->n_used * 4u >= ix->cap * 3u) break;   // grow first
        b->key = key;
        ix->n_used++;
        return b;
      }
      i = (i + 1u) & mask;
    }
    // Rehash into a doubled table (buckets move; entry arrays are
    // carried over by pointer).
    u32 ncap = ix->cap * 2u;
    FolIdxBucket *ns = (FolIdxBucket *)calloc(ncap, sizeof(FolIdxBucket));
    if (ns == NULL) return NULL;
    for (u32 k = 0; k < ix->cap; k++) {
      FolIdxBucket *ob = &ix->slots[k];
      if (ob->key == 0ull) continue;
      u32 j = (u32)(ob->key * 0x9E3779B97F4A7C15ull >> 32) & (ncap - 1u);
      while (ns[j].key != 0ull) j = (j + 1u) & (ncap - 1u);
      ns[j] = *ob;
    }
    free(ix->slots);
    ix->slots = ns;
    ix->cap = ncap;
  }
}

// Append an entry; returns 0 on allocation failure.
static u8 cnf_idx_push(FolIdx *ix, u64 key, u64 ent) {
  FolIdxBucket *b = cnf_idx_bucket(ix, key, 1u);
  if (b == NULL) return 0u;
  if (b->n >= b->cap) {
    u32 nc = (b->cap == 0u) ? 8u : b->cap * 2u;
    void *ne = realloc(b->ents, (size_t)nc * sizeof(u64));
    if (ne == NULL) return 0u;
    b->ents = (u64 *)ne;
    b->cap = nc;
  }
  b->ents[b->n++] = ent;
  return 1u;
}

static u8 cnf_u32_push(u32 **arr, u32 *n, u32 *cap, u32 v) {
  if (*n >= *cap) {
    u32 nc = (*cap == 0u) ? 16u : *cap * 2u;
    void *na = realloc(*arr, (size_t)nc * sizeof(u32));
    if (na == NULL) return 0u;
    *arr = (u32 *)na;
    *cap = nc;
  }
  (*arr)[(*n)++] = v;
  return 1u;
}

// Any failure to keep the index complete falls back to the full
// scans for good -- a partial index would silently drop inferences.
static void cnf_idx_disable(CnfState *s) {
  s->idx_on = 0u;
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
    void *nu = realloc(s->cassume, (size_t)nc * sizeof(CnfGmask));
    if (nu != NULL) s->cassume = (CnfGmask *)nu;
    void *nb = realloc(s->cguard, (size_t)nc * sizeof(u32));
    if (nb != NULL) s->cguard = (u32 *)nb;
    void *nl = realloc(s->cstale, (size_t)nc);
    if (nl != NULL) s->cstale = (u8 *)nl;
    void *nd = NULL;
    if (s->corpse != NULL) {
      nd = realloc(s->corpse, (size_t)nc * sizeof(FolClause *));
      if (nd != NULL) s->corpse = (FolClause **)nd;
    }
    if (s->idx_on) {
      void *ia = realloc(s->cact,   (size_t)nc * sizeof(u32));
      void *ip = realloc(s->cpeq,   (size_t)nc);
      void *ib = realloc(s->cfbl,   (size_t)nc);
      void *is = realloc(s->cstamp, (size_t)nc * sizeof(u32));
      if (ia != NULL) s->cact   = (u32 *)ia;
      if (ip != NULL) s->cpeq   = (u8 *)ip;
      if (ib != NULL) s->cfbl   = (u8 *)ib;
      if (is != NULL) s->cstamp = (u32 *)is;
      if (ia == NULL || ip == NULL || ib == NULL || is == NULL)
        cnf_idx_disable(s);
      else {
        memset(s->cpeq + s->n, 0, (size_t)(nc - s->n));
        memset(s->cfbl + s->n, 0, (size_t)(nc - s->n));
        for (u32 i = s->n; i < nc; i++) { s->cact[i] = 0u; s->cstamp[i] = 0u; }
      }
    }
    if (na == NULL || nw == NULL || nq == NULL || nm == NULL || np == NULL ||
        ns == NULL || nx == NULL || nv == NULL || ng == NULL ||
        nf == NULL || nn == NULL || nu == NULL || nb == NULL || nl == NULL ||
        (s->corpse != NULL && nd == NULL)) return -1;
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
  // Assumption set: the parents' union (AVATAR-lite).  Component
  // clauses get their {guard} singleton patched in by the splitter.
  CnfGmask asum;
  cnf_gm_zero(&asum);
  if (parent_a != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_a]);
  if (parent_b != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_b]);
  s->cassume[id] = asum;
  s->cguard[id]  = 0u;
  u32 w = cnf_clause_weight(c);
  s->cgoal[id]   = goal;
  s->cweight[id] = w;
  s->cpri[id]    = w + ((goal || s->goal_tier == 0u) ? 0u : CNF_NONGOAL_PENALTY);
  s->cmask[id]   = cnf_mask_for(s, c);
  s->cpicked[id] = 0u;
  s->cstale[id]  = 0u;
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
  if (s->idx_on) {
    s->cact[id] = 0u;
    s->cpeq[id] = s->cfbl[id] = 0u;
    s->cstamp[id] = 0u;
    // Forward-subsumption index: one designated (first) literal,
    // fine-keyed by the pattern's own arg0 fingerprint.
    if (c->n_lits == 0u || term_tag(c->lits[0].atom) != TAG_CTR) {
      if (!cnf_u32_push(&s->idx_fsub_wc, &s->n_fsw, &s->cap_fsw, id))
        cnf_idx_disable(s);
    } else if (!cnf_idx_push(&s->idx_fsub,
                             cnf_idx_fine_key(c->lits[0].atom,
                                              c->lits[0].sign,
                                              cnf_idx_arg0(c->lits[0].atom)),
                             (u64)id)) {
      cnf_idx_disable(s);
    }
    // Backward-subsumption index, target side: every distinct coarse
    // (label, sign) key plus every distinct fine (label, sign, arg0)
    // key -- a CTR-arg0 pattern retrieves fine, a VAR-arg0 pattern
    // falls back to coarse.
    for (u32 i = 0; i < c->n_lits && s->idx_on; i++) {
      if (term_tag(c->lits[i].atom) != TAG_CTR) continue;
      u64 k  = cnf_idx_key(c->lits[i].atom, c->lits[i].sign);
      u64 kf = cnf_idx_fine_key(c->lits[i].atom, c->lits[i].sign,
                                cnf_idx_arg0(c->lits[i].atom));
      u8 dup = 0u, dupf = 0u;
      for (u32 j = 0; j < i && !(dup && dupf); j++) {
        if (term_tag(c->lits[j].atom) != TAG_CTR) continue;
        if (cnf_idx_key(c->lits[j].atom, c->lits[j].sign) == k) dup = 1u;
        if (cnf_idx_fine_key(c->lits[j].atom, c->lits[j].sign,
                             cnf_idx_arg0(c->lits[j].atom)) == kf) dupf = 1u;
      }
      if (!dup && !cnf_idx_push(&s->idx_bsub, k, (u64)id)) cnf_idx_disable(s);
      if (s->idx_on && !dupf &&
          !cnf_idx_push(&s->idx_bsub_f, kf, (u64)id)) cnf_idx_disable(s);
    }
  }
  if (s->n_passive >= s->cap_passive) {
    cnf_grow(&s->cap_passive, (u8 **)&s->passive, sizeof(u32));
    if (s->n_passive >= s->cap_passive) return -1;
  }
  s->passive[s->n_passive++] = id;   // age (FIFO) lane
  cnf_pq_push(s, id);                // pick-best lane
  return (i32)id;
}

// === AVATAR-lite component splitting =================================
// See the header comment ("AVATAR-lite component splitting") for the
// semantics + soundness argument.

// CNF_AVATAR_MAX_GUARDS (thvm.h): the guard supply -- CnfGmask width.

// on: 0 = off, 1 = split every stored clause, 2 = scoped (input +
// conjecture-descended only; the guard-economy mode).
fn void cnf_set_avatar(CnfState *s, u8 on) {
  if (s != NULL) s->avatar_on = on;
}

// Variable bitset of a term (dense canon ids).  Sets *over when an id
// >= 64 appears -- the caller then treats the clause as unsplittable.
static void fol_term_varmask(Term t, u64 *m, u8 *over) {
  if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (v >= 64u) *over = 1u;
    else *m |= 1ull << v;
    return;
  }
  if (term_tag(t) != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) fol_term_varmask(term_ctr_at(t, i), m, over);
}

fn u32 fol_components(const FolClause *c, u8 *comp_of) {
  if (c == NULL || c->n_lits == 0u) return 0u;
  u32 n = c->n_lits;   // <= FOL_MAX_LITS by construction
  u64 vm[FOL_MAX_LITS];
  u8 over = 0u;
  for (u32 i = 0; i < n; i++) {
    vm[i] = 0ull;
    fol_term_varmask(c->lits[i].atom, &vm[i], &over);
  }
  if (over) {
    if (comp_of != NULL) memset(comp_of, 0, n);
    return 1u;
  }
  // Pairwise label merging to a fixpoint: literals sharing a variable
  // end up with one label.  O(n^3) worst case at n <= 64 -- fine.
  u8 lab[FOL_MAX_LITS];
  for (u32 i = 0; i < n; i++) lab[i] = (u8)i;
  for (u8 changed = 1u; changed; ) {
    changed = 0u;
    for (u32 i = 0; i < n; i++) {
      for (u32 j = i + 1u; j < n; j++) {
        if ((vm[i] & vm[j]) == 0ull || lab[i] == lab[j]) continue;
        u8 from = lab[j], to = lab[i];
        for (u32 k = 0; k < n; k++)
          if (lab[k] == from) lab[k] = to;
        changed = 1u;
      }
    }
  }
  u8 dense[FOL_MAX_LITS];
  memset(dense, 0xFF, sizeof dense);
  u32 m = 0u;
  for (u32 i = 0; i < n; i++) {
    if (dense[lab[i]] == 0xFF) dense[lab[i]] = (u8)m++;
    if (comp_of != NULL) comp_of[i] = dense[lab[i]];
  }
  return m;
}

// Grow the split-event arrays together (shared n/cap).
static u8 cnf_split_reserve(CnfState *s) {
  if (s->n_split < s->cap_split) return 1u;
  u32 nc = (s->cap_split == 0u) ? 8u : s->cap_split * 2u;
  void *p = realloc(s->split_pos,  (size_t)nc * sizeof(CnfGmask));
  void *q = realloc(s->split_neg,  (size_t)nc * sizeof(CnfGmask));
  void *r = realloc(s->split_orig, (size_t)nc * sizeof(u32));
  if (p != NULL) s->split_pos  = (CnfGmask *)p;
  if (q != NULL) s->split_neg  = (CnfGmask *)q;
  if (r != NULL) s->split_orig = (u32 *)r;
  if (p == NULL || q == NULL || r == NULL) return 0u;
  s->cap_split = nc;
  return 1u;
}

static u8 cnf_refut_reserve(CnfState *s) {
  if (s->n_refut < s->cap_refut) return 1u;
  u32 nc = (s->cap_refut == 0u) ? 8u : s->cap_refut * 2u;
  void *p = realloc(s->refut_mask, (size_t)nc * sizeof(CnfGmask));
  void *q = realloc(s->refut_id,   (size_t)nc * sizeof(u32));
  if (p != NULL) s->refut_mask = (CnfGmask *)p;
  if (q != NULL) s->refut_id   = (u32 *)q;
  if (p == NULL || q == NULL) return 0u;
  s->cap_refut = nc;
  return 1u;
}

// Find an existing guard whose component clause matches `comp`
// (post-canon content equality; fol_clause_eq is literal-order-
// insensitive).  ~0u when absent.  A guard whose component clause is
// no longer retrievable (clobbered, no corpse) just misses -- the
// re-derived component then spends a fresh guard, which is sound.
static u32 cnf_guard_find(const CnfState *s, const FolClause *comp) {
  for (u32 g = 0; g < s->n_guards; g++) {
    const FolClause *have = cnf_clause_at(s, s->guard_comp[g]);
    if (have != NULL && fol_clause_eq(have, comp)) return g;
  }
  return ~0u;
}

// --- propositional closure over the split guards ----------------------
// SAT clause (pos, neg) = \/_{g in pos} a_g  v  \/_{g in neg} ~a_g.
// One split clause per split event (~A -> \/ a_g), one refutation
// clause per conditional empty clause (~(/\A)).  Deciding the family
// is a <= CNF_AVATAR_MAX_GUARDS-variable DPLL: unit propagation, then
// branch on a variable of a shortest open clause, node-budgeted.
// Only an UNSAT verdict is acted on (PROVED), so a budget trip erring
// "satisfiable" is sound -- the loop just keeps saturating (under the
// keep-all fallback model).
typedef struct { CnfGmask pos, neg; } CnfSatClause;

// 1 = satisfiable (or budget exhausted), 0 = unsatisfiable.  On
// genuine satisfaction *model_out = the TRUE-guard set, unassigned
// guards defaulting TRUE; a budget trip leaves *model_out untouched
// (the caller's fallback stands).
static u8 cnf_dpll(const CnfSatClause *cl, u32 n,
                   CnfGmask asg, CnfGmask val, u32 *budget,
                   CnfGmask *model_out) {
  for (;;) {
    if (*budget == 0u) return 1u;
    (*budget)--;
    u8 open = 0u;
    u32 unit_g = ~0u;
    u8 unit_pos = 0u;
    u32 best_free = ~0u;
    CnfGmask best_mask;
    cnf_gm_zero(&best_mask);
    const CnfGmask *best_pos = NULL;
    for (u32 i = 0; i < n; i++) {
      u8 satisfied = 0u;
      CnfGmask fr;
      u64 fr_any = 0ull;
      for (u32 w = 0; w < CNF_GMASK_WORDS; w++) {
        u64 p = cl[i].pos.w[w], q = cl[i].neg.w[w];
        u64 a = asg.w[w],       v = val.w[w];
        if (((p & a & v) | (q & a & ~v)) != 0ull) {   // a true positive /
          satisfied = 1u;                             // a false negated
          break;
        }
        fr.w[w] = (p | q) & ~a;
        fr_any |= fr.w[w];
      }
      if (satisfied) continue;
      if (fr_any == 0ull) return 0u;                  // falsified clause
      open = 1u;
      u32 k = cnf_gm_pop(&fr);
      if (k == 1u) {
        unit_g   = cnf_gm_first(&fr);
        unit_pos = cnf_gm_test(&cl[i].pos, unit_g);
        break;
      }
      if (k < best_free) { best_free = k; best_mask = fr; best_pos = &cl[i].pos; }
    }
    if (!open) {                                      // all satisfied
      if (model_out != NULL)
        for (u32 w = 0; w < CNF_GMASK_WORDS; w++)
          model_out->w[w] = val.w[w] | ~asg.w[w];
      return 1u;
    }
    if (unit_g != ~0u) {
      cnf_gm_set(&asg, unit_g);
      if (unit_pos) cnf_gm_set(&val, unit_g);
      continue;
    }
    u32 g = cnf_gm_first(&best_mask);
    u8 sat_first = cnf_gm_test(best_pos, g);          // satisfy it first
    CnfGmask asg2 = asg, val_t = val, val_f = val;
    cnf_gm_set(&asg2, g);
    cnf_gm_set(&val_t, g);
    if (cnf_dpll(cl, n, asg2, sat_first ? val_t : val_f,
                 budget, model_out))
      return 1u;
    return cnf_dpll(cl, n, asg2, sat_first ? val_f : val_t,
                    budget, model_out);
  }
}

// ACTIVE = every guard of the clause's assumption set is TRUE under
// the current model (trivially so with avatar off: cassume all-zero).
static u8 cnf_is_active(const CnfState *s, u32 id) {
  return cnf_gm_subset(&s->cassume[id], &s->model);
}

// Park an inactive clause for later reactivation (a passive pop, or
// an active clause hit as a skipped inference partner).  cstale
// dedups: one parked entry per deactivation cycle.  0 on OOM -- the
// caller then just keeps the clause active (sound; activation only
// steers the search).
static u8 cnf_park(CnfState *s, u32 id) {
  if (s->cstale[id]) return 1u;   // already parked
  if (s->n_parked >= s->cap_parked) {
    cnf_grow(&s->cap_parked, (u8 **)&s->parked, sizeof(u32));
    if (s->n_parked >= s->cap_parked) return 0u;
  }
  s->parked[s->n_parked++] = id;
  s->cstale[id] = 1u;
  return 1u;
}

// Adopt a model.  On change, every parked clause whose guards are now
// all-TRUE re-enters both pick lanes (matching add-time queueing);
// still-FALSE ids stay parked.  A previously GIVEN clause re-enters
// with cpicked cleared, so it redoes its given-step against the full
// active set -- regenerating the partner pairs skipped while its
// branch was off (the re-given discipline; its duplicate active[]
// entry is benign: conclusions dedup via forward subsumption and the
// self-pair is skipped by id).  An id that fails BOTH queue appends
// (OOM) stays parked rather than being silently dropped.
static void cnf_model_set(CnfState *s, const CnfGmask *m) {
  if (cnf_gm_eq(m, &s->model)) return;
  s->model = *m;
  s->n_models++;
  u32 keep = 0u;
  for (u32 i = 0; i < s->n_parked; i++) {
    u32 id = s->parked[i];
    if (s->clauses[id] == NULL) { s->cstale[id] = 0u; continue; }
    if (!cnf_gm_subset(&s->cassume[id], m)) { s->parked[keep++] = id; continue; }
    u32 heap0 = s->n_pheap;
    cnf_pq_push(s, id);
    u8 queued = (s->n_pheap > heap0) ? 1u : 0u;
    if (s->n_passive >= s->cap_passive)
      cnf_grow(&s->cap_passive, (u8 **)&s->passive, sizeof(u32));
    if (s->n_passive < s->cap_passive) {
      s->passive[s->n_passive++] = id;   // age (FIFO) lane
      queued = 1u;
    }
    if (!queued) { s->parked[keep++] = id; continue; }
    s->cpicked[id] = 0u;
    s->cstale[id]  = 0u;
  }
  s->n_parked = keep;
}

// Decide the guard theory {split clauses} u {refutation clauses} and
// refresh the model.  0 iff propositionally UNSAT -- the case
// analysis is exhausted; the caller closes the run.  On SAT the
// adopted model is genuine; a DPLL budget trip or OOM falls back to
// the all-TRUE keep-all model (everything active -- the
// pre-activation behavior).
static u8 cnf_avatar_solve(CnfState *s) {
  CnfGmask allon;
  cnf_gm_fill(&allon);
  if (s->n_refut == 0u) { cnf_model_set(s, &allon); return 1u; }
  u32 n = s->n_split + s->n_refut;
  CnfSatClause *cl = (CnfSatClause *)malloc((size_t)n * sizeof(CnfSatClause));
  if (cl == NULL) { cnf_model_set(s, &allon); return 1u; }
  for (u32 i = 0; i < s->n_split; i++) {
    cl[i].pos = s->split_pos[i];
    cl[i].neg = s->split_neg[i];
  }
  for (u32 i = 0; i < s->n_refut; i++) {
    cnf_gm_zero(&cl[s->n_split + i].pos);
    cl[s->n_split + i].neg = s->refut_mask[i];
  }
  u32 budget = 1u << 20;
  CnfGmask model = allon, zero;
  cnf_gm_zero(&zero);
  u8 sat = cnf_dpll(cl, n, zero, zero, &budget, &model);
  free(cl);
  if (sat) cnf_model_set(s, &model);
  return sat;
}

// Attempt the component split of `c` (provenance rule/parents; the
// assumption set is the parents' union).  On success the ORIGINAL is
// stored as a provenance-only record (skipped by both pick lanes),
// each NEW component is stored under its guard, and the SAT split
// clause is recorded; returns the original's id (c consumed).  A
// duplicate split event (all components memo-hit an already-recorded
// event) frees c and returns the recorded original.  Returns -2 when
// the clause does not split (c untouched; caller stores normally) and
// -1 on allocation failure (c untouched).
static i32 cnf_try_split(CnfState *s, FolClause *c, FolInference rule,
                         u32 parent_a, u32 parent_b, u8 goal_force) {
  if (!s->avatar_on || c->n_lits < 2u) return -2;
  // Scoped splitting (avatar_on == 2): spend guards on INPUT clauses
  // and conjecture-descended conclusions only.  The guard supply is
  // the scarce resource; unscoped runs on flood-class problems burn it
  // on axiom-side junk before the case analysis near the goal starts.
  // Splitting scope is a strategy choice, never a soundness one.
  if (s->avatar_on == 2u && rule != FOL_INF_INPUT && !goal_force) {
    u8 goal = 0u;
    if (parent_a != FOL_NO_PARENT && s->cgoal[parent_a]) goal = 1u;
    if (parent_b != FOL_NO_PARENT && s->cgoal[parent_b]) goal = 1u;
    if (!goal) return -2;
  }
  u8 comp_of[FOL_MAX_LITS];
  u32 m = fol_components(c, comp_of);
  if (m < 2u) return -2;
  CnfGmask asum;
  cnf_gm_zero(&asum);
  if (parent_a != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_a]);
  if (parent_b != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_b]);
  // Build the component clauses first: guard demand is only known
  // after the memo lookups.
  FolClause *comp[FOL_MAX_LITS];
  u32 comp_guard[FOL_MAX_LITS];
  u32 n_fresh = 0u;
  for (u32 k = 0; k < m; k++) {
    u32 cn = 0u;
    for (u32 i = 0; i < c->n_lits; i++)
      if (comp_of[i] == k) cn++;
    comp[k] = fol_clause_new(cn);
    if (comp[k] == NULL) {
      for (u32 j = 0; j < k; j++) fol_clause_free(comp[j]);
      return -1;
    }
    cn = 0u;
    for (u32 i = 0; i < c->n_lits; i++)
      if (comp_of[i] == k) comp[k]->lits[cn++] = c->lits[i];
    fol_clause_canon_vars(comp[k]);
    comp_guard[k] = cnf_guard_find(s, comp[k]);
    if (comp_guard[k] == ~0u) n_fresh++;
  }
  if (s->n_guards + n_fresh > CNF_AVATAR_MAX_GUARDS) {
    // Guard supply exhausted: store the clause unsplit (sound; the
    // wide clause is simply kept in the ordinary calculus).
    for (u32 k = 0; k < m; k++) fol_clause_free(comp[k]);
    return -2;
  }
  if (!cnf_split_reserve(s)) {
    for (u32 k = 0; k < m; k++) fol_clause_free(comp[k]);
    return -1;
  }
  if (n_fresh == 0u) {
    // Every component memoized: dedupe the split event.
    CnfGmask pos;
    cnf_gm_zero(&pos);
    for (u32 k = 0; k < m; k++) cnf_gm_set(&pos, comp_guard[k]);
    for (u32 e = 0; e < s->n_split; e++) {
      if (cnf_gm_eq(&s->split_pos[e], &pos) &&
          cnf_gm_eq(&s->split_neg[e], &asum)) {
        for (u32 k = 0; k < m; k++) fol_clause_free(comp[k]);
        fol_clause_free(c);
        return (i32)s->split_orig[e];
      }
    }
  }
  // Store the original: provenance-only (never given, so it generates
  // nothing; it still serves the subsumption indexes, which is sound
  // under the assumption-subset gate).
  i32 orig = cnf_add_clause_traced(s, c, rule, parent_a, parent_b);
  if (orig < 0) {
    for (u32 k = 0; k < m; k++) fol_clause_free(comp[k]);
    return -1;
  }
  s->cpicked[(u32)orig] = 1u;
  if (goal_force) s->cgoal[(u32)orig] = 1u;   // components inherit below
  CnfGmask pos;
  cnf_gm_zero(&pos);
  for (u32 k = 0; k < m; k++) {
    if (comp_guard[k] != ~0u) {
      // Memoized: the guard's component clause already stands.
      cnf_gm_set(&pos, comp_guard[k]);
      fol_clause_free(comp[k]);
      continue;
    }
    u32 g = s->n_guards;
    i32 idc = cnf_add_clause_traced(s, comp[k], FOL_INF_SPLIT,
                                    (u32)orig, FOL_NO_PARENT);
    if (idc < 0) {
      // Component store failed (OOM): the guard is still emitted into
      // the split clause -- a free positive the SAT side can satisfy,
      // so nothing becomes unsound; this branch just never closes.
      fol_clause_free(comp[k]);
      s->guard_comp[g] = ~0u;   // no clause: memo lookups miss it
    } else {
      cnf_gm_zero(&s->cassume[(u32)idc]);
      cnf_gm_set(&s->cassume[(u32)idc], g);
      s->cguard[(u32)idc]  = g + 1u;
      s->guard_comp[g] = (u32)idc;
    }
    s->n_guards++;
    comp_guard[k] = g;
    cnf_gm_set(&pos, g);
  }
  s->split_pos[s->n_split]  = pos;
  s->split_neg[s->n_split]  = asum;
  s->split_orig[s->n_split] = (u32)orig;
  s->n_split++;
  // Model maintenance: fresh guards enter TRUE, so an event that
  // allocated any is satisfied as recorded.  A memo-only event can be
  // falsified (every component branch FALSE while the parent
  // assumptions hold): re-solve -- UNSAT here means every branch
  // combination is already refuted.
  if (!cnf_gm_intersects(&pos, &s->model) &&
      cnf_gm_subset(&asum, &s->model) &&
      !cnf_avatar_solve(s))
    s->status = ATP_PROVED;
  return orig;
}

// Shared store: the AVATAR split attempt, else the plain traced add.
// goal_force marks the clause (and its components) conjecture-related.
static i32 cnf_store_new(CnfState *s, FolClause *c, FolInference rule,
                         u32 parent_a, u32 parent_b, u8 goal_force) {
  if (s == NULL || c == NULL) return -1;
  i32 sp = cnf_try_split(s, c, rule, parent_a, parent_b, goal_force);
  if (sp >= 0) return sp;
  i32 id = cnf_add_clause_traced(s, c, rule, parent_a, parent_b);
  if (id >= 0 && goal_force && !s->cgoal[id]) {
    s->cgoal[id] = 1u;
    s->cpri[id]  = s->cweight[id];   // drop the non-goal penalty
    // The heap entry was pushed with the penalized key; the goal key
    // is only SMALLER, so correct it by re-pushing -- the stale
    // duplicate is skipped via cpicked when it surfaces later.
    cnf_pq_push(s, (u32)id);
  }
  return id;
}

// Public entry point: user-added clauses get the INPUT marker.
fn i32 cnf_add_clause(CnfState *s, FolClause *c) {
  if (s == NULL || c == NULL) return -1;
  fol_clause_canon_vars(c);
  return cnf_store_new(s, c, FOL_INF_INPUT,
                       FOL_NO_PARENT, FOL_NO_PARENT, 0u);
}

fn i32 cnf_add_clause_goal(CnfState *s, FolClause *c, u8 goal_related) {
  if (s == NULL || c == NULL) return -1;
  fol_clause_canon_vars(c);
  return cnf_store_new(s, c, FOL_INF_INPUT,
                       FOL_NO_PARENT, FOL_NO_PARENT,
                       goal_related ? 1u : 0u);
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
// selection config; any config change invalidates it wholesale --
// along with the active-side literal index built from it (rebuilt
// lazily at the next cnf_step).
static void cnf_elig_invalidate(CnfState *s) {
  if (s->celig_valid != NULL && s->n > 0u) memset(s->celig_valid, 0, s->n);
  s->idx_res_valid = 0u;
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

static void cnf_forward_demod(CnfState *s, FolClause **pc,
                              const CnfGmask *asum) {
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
      // Assumption gate (AVATAR): a rule under assumptions B may
      // rewrite a target under assumptions A only when B is a subset
      // of A (the result keeps A).  All-zero when avatar is off.
      if (!cnf_gm_subset(&s->cassume[s->ueq[i]], asum)) continue;
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
// One stored-subsumer test, shared by the scan and bucket paths.
// `ca` is the candidate's assumption set: a stored clause under
// assumptions B subsumes only when B is a subset of ca (AVATAR gate;
// all-zero, i.e. always-true, when avatar is off).  A stored
// conditional EMPTY clause thus sweeps exactly its refuted branch.
static u8 cnf_fsub_try(const CnfState *s, u32 i, const FolClause *c,
                       u64 cm, u32 cw, const FolLit *frozen,
                       const CnfGmask *ca) {
  const FolClause *a = s->clauses[i];
  if (a == NULL) return 0u;
  if (a->n_lits > c->n_lits) return 0u;
  if (s->cweight[i] > cw) return 0u;   // substitution never shrinks
  if ((s->cmask[i] & ~cm) != 0ull) return 0u;
  if (!cnf_gm_subset(&s->cassume[i], ca)) return 0u;
  return fol_subsumes_frozen_nv(a, frozen, c->n_lits, s->cnvars[i]);
}

static u8 cnf_is_subsumed(const CnfState *s, const FolClause *c,
                          const CnfGmask *ca) {
  if (c->n_lits > FOL_MAX_LITS) return 0u;
  u64 cm = cnf_mask_for(s, c);
  u32 cw = cnf_clause_weight(c);
  u64 mark = HEAP_NEXT, gc_snap = gc_count();
  // Freeze the candidate ONCE; every surviving pattern reuses it.
  FolLit frozen[FOL_MAX_LITS];
  fol_freeze_lits(c, frozen);
  u8 hit = 0u;
  if (s->idx_on) {
    // Bucket retrieval: a subsumer's designated (first) literal maps
    // onto one of c's literals, so its fine key is determined by that
    // literal up to the pattern's arg0 being a variable -- scan, per
    // literal of c, the exact-arg0 bucket plus the VAR-arg0 bucket,
    // then the wildcard patterns.
    for (u32 i = 0; i < c->n_lits && !hit; i++) {
      if (term_tag(c->lits[i].atom) != TAG_CTR) continue;
      Term atom = c->lits[i].atom;
      u8 sign = c->lits[i].sign;
      u64 a0 = cnf_idx_arg0(atom);
      for (int pass = 0; pass < 2 && !hit; pass++) {
        // pass 0: patterns with this exact arg0 fingerprint;
        // pass 1: patterns whose arg0 is a variable (skip when the
        //         target's own fingerprint IS the variable/none case:
        //         pass 0 already covered that bucket).
        u64 pa0 = (pass == 0) ? a0 : CNF_ARG_VAR;
        if (pass == 1 && (a0 == CNF_ARG_VAR || a0 == CNF_ARG_NONE)) break;
        u64 k = cnf_idx_fine_key(atom, sign, pa0);
        // Skip buckets an earlier literal ALREADY scanned: literal j
        // scanned fine(j, arg0_j), plus fine(j, VAR) unless its own
        // fingerprint was the VAR/NONE case.
        u8 dup = 0u;
        for (u32 j = 0; j < i && !dup; j++) {
          if (term_tag(c->lits[j].atom) != TAG_CTR) continue;
          u64 ja0 = cnf_idx_arg0(c->lits[j].atom);
          if (cnf_idx_fine_key(c->lits[j].atom, c->lits[j].sign, ja0) == k)
            dup = 1u;
          else if (ja0 != CNF_ARG_VAR && ja0 != CNF_ARG_NONE &&
                   cnf_idx_fine_key(c->lits[j].atom, c->lits[j].sign,
                                    CNF_ARG_VAR) == k) dup = 1u;
        }
        if (dup) continue;
        const FolIdxBucket *b =
            cnf_idx_bucket((FolIdx *)&s->idx_fsub, k, 0u);
        for (u32 e = 0; b != NULL && e < b->n && !hit; e++)
          hit = cnf_fsub_try(s, (u32)b->ents[e], c, cm, cw, frozen, ca);
      }
    }
    for (u32 e = 0; e < s->n_fsw && !hit; e++)
      hit = cnf_fsub_try(s, s->idx_fsub_wc[e], c, cm, cw, frozen, ca);
  } else {
    for (u32 i = 0; i < s->n && !hit; i++)
      hit = cnf_fsub_try(s, i, c, cm, cw, frozen, ca);
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
    // AVATAR gate: rewrite only targets whose assumption set contains
    // the rule's (the derivative keeps the target's set, which the
    // traced add reproduces as the parents' union).
    if (!cnf_gm_subset(&s->cassume[new_id], &s->cassume[i])) continue;
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
      cnf_empty_derived(s, d, FOL_INF_DEMOD, new_id, i);
      if (s->status != ATP_RUNNING) return;
      cnf_reclaim_consider(s, n_before, mark, gc_snap);
      continue;
    }
    if (fol_is_tautology(d)) { fol_clause_free(d); cnf_reclaim(mark, gc_snap); continue; }
    CnfGmask du = s->cassume[new_id];
    cnf_gm_or(&du, &s->cassume[i]);
    if (cnf_is_subsumed(s, d, &du)) {
      fol_clause_free(d);
      cnf_reclaim(mark, gc_snap);
      continue;
    }
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
// One backward-subsumption target test + removal, shared by the scan
// and bucket paths.
static void cnf_bsub_try(CnfState *s, u32 new_id, const FolClause *new_c,
                         u64 nm, u32 i) {
  if (i == new_id) return;
  FolClause *old = s->clauses[i];
  if (old == NULL) return;
  if (new_c->n_lits > old->n_lits) return;
  if (s->cweight[new_id] > s->cweight[i]) return;
  if ((nm & ~s->cmask[i]) != 0ull) return;
  // AVATAR gate: the subsumer's assumptions must be a subset of the
  // removed clause's (all-zero when avatar is off).
  if (!cnf_gm_subset(&s->cassume[new_id], &s->cassume[i])) return;
  u8 sub = (s->cfrozen[i] != NULL)
             ? fol_subsumes_frozen_nv(new_c, s->cfrozen[i], old->n_lits,
                                      s->cnvars[new_id])
             : fol_subsumes(new_c, old);
  if (sub) {
    s->clauses[i] = NULL;
    if (s->retain_dead) {
      s->corpse[i] = old;
      return;
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

static void cnf_backward_subsume(CnfState *s, u32 new_id) {
  const FolClause *new_c = s->clauses[new_id];
  if (new_c == NULL) return;
  u64 nm = s->cmask[new_id];
  u64 mark = HEAP_NEXT, gc_snap = gc_count();
  // Bucket retrieval: any clause new_c subsumes contains a literal
  // carrying new_c's first literal's key (substitution preserves a
  // CTR root and the sign, and the target literal it maps onto is
  // concrete -- frozen target variables never match a pattern CTR).
  // A CTR arg0 on the pattern pins the target literal's fine key;
  // a VAR arg0 falls back to the coarse (label, sign) family.
  // Entries append in id order, so the removal walk matches the full
  // 0..n scan on a candidate superset.  A non-CTR first literal can
  // map anywhere -- full scan for those rare patterns (an empty
  // clause never reaches here: PROVED first).
  if (s->idx_on && new_c->n_lits > 0u &&
      term_tag(new_c->lits[0].atom) == TAG_CTR) {
    Term p0 = new_c->lits[0].atom;
    u8 sgn = new_c->lits[0].sign;
    u64 a0 = cnf_idx_arg0(p0);
    const FolIdxBucket *b;
    if (a0 == CNF_ARG_VAR) {
      b = cnf_idx_bucket(&s->idx_bsub, cnf_idx_key(p0, sgn), 0u);
    } else {
      b = cnf_idx_bucket(&s->idx_bsub_f, cnf_idx_fine_key(p0, sgn, a0), 0u);
    }
    for (u32 e = 0; b != NULL && e < b->n; e++)
      cnf_bsub_try(s, new_id, new_c, nm, (u32)b->ents[e]);
  } else {
    for (u32 i = 0; i < s->n; i++) cnf_bsub_try(s, new_id, new_c, nm, i);
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

// A derived EMPTY clause (consumes c).  With empty assumptions this is
// the outright refutation.  Under assumptions A (AVATAR): record the
// SAT refutation clause ~(/\A), keep the empty clause as a
// provenance-only record -- which, via the assumption-gated
// subsumption sweep, retires every stored clause of the refuted
// branch -- and close the run when the guard theory goes
// propositionally UNSAT.
static void cnf_empty_derived(CnfState *s, FolClause *c, FolInference rule,
                              u32 parent_a, u32 parent_b) {
  CnfGmask asum;
  cnf_gm_zero(&asum);
  if (parent_a != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_a]);
  if (parent_b != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_b]);
  if (!s->avatar_on || cnf_gm_is_zero(&asum)) {
    cnf_add_clause_traced(s, c, rule, parent_a, parent_b);
    s->status = ATP_PROVED;
    return;
  }
  // The recorded refutation masks stay an ANTICHAIN: a new empty
  // whose assumptions contain a recorded mask adds nothing (its SAT
  // clause is propositionally subsumed) -- drop it; and a new
  // narrower mask retires every recorded superset (their clause
  // records stay in the store; the SAT theory only strengthens).
  // Deep flood runs otherwise accumulate tens of thousands of wide
  // masks, each paying a store + sweep + DPLL.
  for (u32 r = 0; r < s->n_refut; r++) {
    if (cnf_gm_subset(&s->refut_mask[r], &asum)) { fol_clause_free(c); return; }
  }
  {
    u32 w = 0u;
    for (u32 r = 0; r < s->n_refut; r++) {
      if (cnf_gm_subset(&asum, &s->refut_mask[r])) continue;  // superset: retire
      s->refut_mask[w] = s->refut_mask[r];
      s->refut_id[w]   = s->refut_id[r];
      w++;
    }
    s->n_refut = w;
  }
  if (!cnf_refut_reserve(s)) { fol_clause_free(c); return; }
  i32 id = cnf_add_clause_traced(s, c, rule, parent_a, parent_b);
  if (id < 0) return;   // OOM: c stays unfreed by the traced add's contract
  s->cpicked[(u32)id] = 1u;   // provenance-only: never given
  s->refut_mask[s->n_refut] = asum;
  s->refut_id[s->n_refut]   = (u32)id;
  s->n_refut++;
  cnf_backward_subsume(s, (u32)id);   // sweep the refuted branch
  // The refuted branch lay inside the current model (active parents),
  // so the re-solve either closes the theory or moves the model on --
  // deactivating this branch combination and activating another.
  if (!cnf_avatar_solve(s)) s->status = ATP_PROVED;
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
  // The conclusion's assumption set (the parents' union) gates the
  // simplifiers below; all-zero when avatar is off.
  CnfGmask asum;
  cnf_gm_zero(&asum);
  if (parent_a != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_a]);
  if (parent_b != FOL_NO_PARENT) cnf_gm_or(&asum, &s->cassume[parent_b]);
  // Canonize FIRST: binary inferences leave second-premise variables
  // in the renamed range; every downstream consumer (demod, the
  // filters, later inferences on the stored clause) needs dense ids.
  fol_clause_canon_vars(c);
  cnf_forward_demod(s, &c, &asum);
  if (c == NULL) return;
  if (fol_clause_is_empty(c)) {
    cnf_empty_derived(s, c, rule, parent_a, parent_b);
    return;
  }
  if (fol_is_tautology(c)) { fol_clause_free(c); return; }
  if (cnf_is_subsumed(s, c, &asum)) { fol_clause_free(c); return; }
  // Store (splitting into guarded components when avatar applies);
  // run the backward passes for every fresh QUEUED clause -- the one
  // stored clause on the plain path, the components on a split (the
  // provenance-only original is cpicked and skipped).
  u32 first = s->n;
  cnf_store_new(s, c, rule, parent_a, parent_b, 0u);
  u32 last = s->n;
  for (u32 id = first; id < last && s->status == ATP_RUNNING; id++) {
    if (s->clauses[id] == NULL || s->cpicked[id]) continue;
    cnf_backward_subsume(s, id);
    cnf_backward_demod(s, id);
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

// --- active-side literal index maintenance ---------------------------

// Does the clause qualify as a paramodulation SOURCE under the
// current config?  Mirrors cnf_gen_paramod's gates exactly: the
// selection must not have picked a negative literal, and some
// positive equality literal must be maximal (indexes past the
// 64-bit eligibility mask count as maximal, like cnf_gen_paramod).
static u8 cnf_clause_is_peq(const CnfState *s, u32 id) {
  const FolClause *c = s->clauses[id];
  if (c == NULL) return 0u;
  u32 sel = s->csel[id];
  if (sel != ~0u && c->lits[sel].sign == 1u) return 0u;
  for (u32 i = 0; i < c->n_lits; i++) {
    if (c->lits[i].sign != 0u) continue;
    if (!fol_atom_is_eq(c->lits[i].atom)) continue;
    if (i < 64u && ((s->cmaxlits[id] >> i) & 1ull) == 0ull) continue;
    return 1u;
  }
  return 0u;
}

// Insert a freshly-activated clause: eligible CTR literals go to the
// (label, sign) buckets; a clause with an eligible non-CTR atom (or
// a literal index past the 8-bit packing) goes on the always-visited
// fallback list instead; paramod sources additionally join idx_peq.
static void cnf_idx_activate(CnfState *s, u32 id) {
  const FolClause *c = s->clauses[id];
  if (c == NULL || !s->idx_on) return;
  cnf_elig_ensure(s, id);
  u8 fb = 0u;
  for (u32 i = 0; i < c->n_lits && !fb; i++) {
    if (!cnf_elig(s, id, i)) continue;
    if (term_tag(c->lits[i].atom) != TAG_CTR || i > 255u) fb = 1u;
  }
  s->cfbl[id] = fb;
  if (fb) {
    if (!cnf_u32_push(&s->idx_fb, &s->n_fb, &s->cap_fb, id))
      cnf_idx_disable(s);
  } else {
    for (u32 i = 0; i < c->n_lits && s->idx_on; i++) {
      if (!cnf_elig(s, id, i)) continue;
      u64 k = cnf_idx_key(c->lits[i].atom, c->lits[i].sign);
      if (!cnf_idx_push(&s->idx_res, k, ((u64)id << 8) | (u64)i))
        cnf_idx_disable(s);
    }
  }
  u8 peq = cnf_clause_is_peq(s, id);
  s->cpeq[id] = peq;
  if (peq && !cnf_u32_push(&s->idx_peq, &s->n_peq, &s->cap_peq, id))
    cnf_idx_disable(s);
}

// Rebuild the active-side index from the active list (after a
// cnf_set_* config change moved literal eligibility).  Also refreshes
// the activation ordinals the candidate sort keys off.
static void cnf_idx_rebuild(CnfState *s) {
  cnf_idx_reset(&s->idx_res);
  s->n_peq = s->n_fb = 0u;
  for (u32 k = 0; k < s->n_active && s->idx_on; k++) {
    u32 id = s->active[k];
    s->cact[id] = k;
    cnf_idx_activate(s, id);
  }
  s->idx_res_valid = 1u;
}

// Indexed cross-inference dispatch for the given clause.  Stamps the
// partner candidates -- complementary-key bucket hits, the paramod
// sources, the fallback list -- then walks the prior active list in
// ACTIVATION order, visiting only stamped clauses with exactly the
// calls the full scan would make (a skipped pair generates nothing
// there; see the header comment).  The walk keeps the visit order
// byte-identical to the full scan without a per-step sort; its
// O(n_active) stamp probes are two loads each, three orders of
// magnitude cheaper than the generation calls they replace.
// Returns 0 when the index cannot narrow this given (an eligible
// non-CTR literal, or the given is itself a paramod source: its
// TARGETS are label-unconstrained) -- the caller then runs the full
// scan.
static u8 cnf_idx_partners(CnfState *s, u32 given_id, u32 active_snapshot) {
  const FolClause *g = s->clauses[given_id];
  if (g == NULL || !s->idx_on) return 0u;
  if (s->cfbl[given_id] || s->cpeq[given_id]) return 0u;
  if (++s->idx_epoch == 0u) {   // epoch wrap: clear the stamps once
    memset(s->cstamp, 0, (size_t)s->cap * sizeof(u32));
    s->idx_epoch = 1u;
  }
  for (u32 i = 0; i < g->n_lits; i++) {
    if (!cnf_elig(s, given_id, i)) continue;
    u64 k = cnf_idx_key(g->lits[i].atom, (u8)(1u - g->lits[i].sign));
    const FolIdxBucket *b = cnf_idx_bucket(&s->idx_res, k, 0u);
    for (u32 e = 0; b != NULL && e < b->n; e++)
      s->cstamp[(u32)(b->ents[e] >> 8)] = s->idx_epoch;
  }
  for (u32 e = 0; e < s->n_peq; e++) s->cstamp[s->idx_peq[e]] = s->idx_epoch;
  for (u32 e = 0; e < s->n_fb;  e++) s->cstamp[s->idx_fb[e]]  = s->idx_epoch;
  for (u32 k = 0; k + 1u < active_snapshot; k++) {
    // A mid-step model flip can deactivate the GIVEN itself (an empty
    // clause derived from its conclusions): stop expanding the dead
    // branch; the parked given redoes the whole step on reactivation.
    if (!cnf_is_active(s, given_id) && cnf_park(s, given_id)) return 1u;
    u32 a_id = s->active[k];
    if (a_id == given_id) continue;
    if (s->cstamp[a_id] != s->idx_epoch) continue;
    if (s->clauses[a_id] == NULL) continue;
    // Model-guided activation: a partner on a FALSE branch is parked
    // for a re-given pass if its branch reactivates (park failure =
    // OOM: keep the partner -- sound, just unfocused).
    if (!cnf_is_active(s, a_id) && cnf_park(s, a_id)) continue;
    cnf_gen_resolution(s, given_id, a_id);
    if (s->status != ATP_RUNNING) return 1u;
    cnf_gen_resolution(s, a_id, given_id);   // symmetric face
    if (s->status != ATP_RUNNING) return 1u;
    // given -> a paramodulation is a no-op here (the given carries no
    // eligible positive equality: cpeq gate above); a -> given fires
    // only for paramod sources, exactly as the full scan's calls do.
    if (s->cpeq[a_id]) {
      cnf_gen_paramod(s, a_id, given_id);
      if (s->status != ATP_RUNNING) return 1u;
    }
  }
  return 1u;
}

// Pop the next given clause.  Default lane: the pick-best heap
// (smallest weight first).  Every age_ratio-th pick the FIFO lane
// supplies the OLDEST surviving passive clause instead, so heavy
// clauses can't starve (fairness).  Entries consumed by the other
// lane (cpicked) or clobbered by backward subsumption (NULL slot)
// are skipped lazily; INACTIVE entries (a FALSE guard in the current
// model) are parked, not consumed -- they re-queue on a model change.
// Returns ~0u when both lanes are empty (with parked clauses pending
// that is the current branch combination's saturation -- a genuine
// model of the input, see the header's TERMINATION note).
static u32 cnf_pop_passive(CnfState *s) {
  s->pick_count++;
  u8 age_turn = (s->age_ratio != 0u &&
                 (s->pick_count % s->age_ratio) == 0u) ? 1u : 0u;
  if (!age_turn) {
    for (;;) {
      u32 id = cnf_pq_pop(s);
      if (id == ~0u) break;
      if (s->clauses[id] == NULL || s->cpicked[id]) continue;
      if (!cnf_is_active(s, id) && cnf_park(s, id)) continue;
      s->cpicked[id] = 1u;
      return id;
    }
  }
  while (s->passive_head < s->n_passive) {
    u32 id = s->passive[s->passive_head++];
    if (s->clauses[id] == NULL || s->cpicked[id]) continue;
    if (!cnf_is_active(s, id) && cnf_park(s, id)) continue;
    s->cpicked[id] = 1u;
    return id;
  }
  // Age turn with a drained FIFO: fall back to the heap.
  for (;;) {
    u32 id = cnf_pq_pop(s);
    if (id == ~0u) break;
    if (s->clauses[id] == NULL || s->cpicked[id]) continue;
    if (!cnf_is_active(s, id) && cnf_park(s, id)) continue;
    s->cpicked[id] = 1u;
    return id;
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
  if (s->idx_on) {
    s->cact[given_id] = s->n_active - 1u;
    if (!s->idx_res_valid) cnf_idx_rebuild(s);   // config changed (or first step)
    else cnf_idx_activate(s, given_id);
  }

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
  if (!(s->idx_on && cnf_idx_partners(s, given_id, active_snapshot))) {
    for (u32 k = 0; k + 1u < active_snapshot; k++) {
      // Mid-step model flip: see cnf_idx_partners.
      if (!cnf_is_active(s, given_id) && cnf_park(s, given_id)) break;
      u32 a_id = s->active[k];
      if (a_id == given_id) continue;
      if (s->clauses[a_id] == NULL) continue;
      // Model-guided activation: park FALSE-branch partners (see
      // cnf_idx_partners).
      if (!cnf_is_active(s, a_id) && cnf_park(s, a_id)) continue;
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
  }
  if (s->status != ATP_RUNNING) return s->status;

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
    case FOL_INF_SPLIT:     return "split";
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
  // AVATAR: show the assumption guards a conditional clause depends on.
  if (s->cassume != NULL && !cnf_gm_is_zero(&s->cassume[id])) {
    fprintf(out, "   <-");
    for (u32 g = 0; g < 64u; g++) {
      if (cnf_gm_test(&s->cassume[id], g)) fprintf(out, " a%u", g);
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
