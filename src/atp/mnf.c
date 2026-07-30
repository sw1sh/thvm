#ifdef ATP_MNF
// === Milestone 10: MNF -- the Multiple-Normal-Forms goal search =====
//
// A port of Waldmeister's MNF module.  The conjecture stops being a
// passive single-normal-form check and becomes a goal search: goal_lhs
// seeds a GREEN front, goal_rhs a RED one.  Each front *normalises* its
// term forward (l->r) with the current rule set R; because R is not
// confluent a term has many forward reducts, so each front is a set,
// held in a hash table.  When a reduct collides with a term already in
// the table of the OPPOSITE colour the fronts have met: goal_lhs and
// goal_rhs share a rewrite path, so the goal is proved.
//
// The set is fed incrementally -- each rule completion derives is
// applied to the already-reached terms -- so progress comes from
// completion growing R until the fronts' forward reducts coincide.
//
// Search order is Waldmeister's irreducible-adaptive deque: expand
// depth-first (newest node) while reductions stay productive, switch
// to breadth-first (oldest node) the moment the last node expanded was
// irreducible -- a normal form, a dead end (see mnf_pop / mnf_step).
//
#define MNF_RED        0u
#define MNF_GREEN      1u
// WM zb=mnf preset knobs (the ONLY parametrisation Waldmeister's -auto
// ever arms: `mnf=re.0:s:mnfanalysis=1000.2000.10:mnfantiWOVar:
// mnfantir=5:mnfup=5`, YFiles.c:83/:155; index 0 "rnf" and index 1
// "small" are dead code in the Automodus tables):
//   mnfup=5     -- at most 5 order-INCREASING equation steps per
//                  derivation path (MNF.c:609, counter anzGlSchritte).
//   mnfantir=5  -- at most 5 reversed-RULE steps per path (MNF.c:726/
//                  :755, anzAntiRSchritte), and only for GROUND rules
//                  (antiWOVar, MNF_ConvertRegelToAntiRegel :1419-1430);
//                  tried only in the new-fact sweep, never in the PQ
//                  expansion.
//   mnf=re.0    -- expand with rules AND equations; ".0" = the hard
//                  set-size cap is DISABLED (MNF.c:1137).
//   mnfanalysis=1000.2000.10 -- at 1000 members the new-fact sweep
//                  visits only irreducible-or-origin members
//                  (MNF_AddTryAllElements=FALSE, MNF.c:849-851); at
//                  2000 the set switches to ExpandRewrite mode: prune
//                  to origins + the first 10 irreducible members per
//                  colour, and from then on each check fully
//                  normalizes those members (NF_NormalformRE) hunting
//                  a cross-colour NF collision (MNF.c:844-848,
//                  :1657-1704, :1822-1910).
#define MNF_UP_MAX       5u
#define MNF_ANTIR_MAX    5u
#define MNF_SWITCH_IRRED 1000u
#define MNF_SWITCH_NF    2000u
#define MNF_COUNT_NF     10u
#define MNF_MAX_NODES  400000u
#define MNF_SUCC_CAP   2048u
// mnf_step's budget parameter is IGNORED: WM's MNF_ExpandNM runs the
// expansion loop until join, queue exhaustion, or the mnfanalysis
// switch (MNF.c:858-906) -- no per-check budget.  The cost is bounded
// by the order gates (Groesser steps always, up-steps capped) plus the
// 2000-member switch to the pruned full-normalize mode.
#define MNF_BUDGET     16u
#define MNF_N_BUCKETS  (1u << 21)        // 2097152, power of two
#define MNF_ROOT_PARENT 0xFFFFFFFFu      // mnf_insert: this term is a seed

typedef struct {
  Term term;
  u32  hash;
  u32  parent;     // node this term was first reached from (root: self)
  u8   colour;     // MNF_RED / MNF_GREEN
  u8   gl_steps;   // order-increasing EQUATION steps on the path (mnfup)
  u8   antir_steps;// reversed-RULE steps on the path (mnfantir)
  u8   expanded;   // successors already generated against [0, n_rules_seen)
  u8   irred;      // 1 until an order-decreasing successor is found
  u16  tot_steps;  // total steps on the path (WM anzGesSchritte)
} MnfNode;

typedef struct AtpMnf {
  MnfNode *nodes;
  u32      n_nodes;
  u32      cap_nodes;
  // Each front is a per-colour STACK of pending node indices (WM's PQ
  // default discipline, MNF.c:1094): successors push left, pops take
  // the left (newest) end.
  u32     *qred;
  u32     *qgreen;
  u32      qred_head, qred_tail;       // live span is q[head, tail)
  u32      qgreen_head, qgreen_tail;
  u32      q_cap;
  u8       red_last_irred;             // diag only (stack pops ignore it)
  u8       green_last_irred;
  u32     *buckets;          // open addressing: bucket -> node_idx + 1
  u32      n_buckets;
  u32      n_rules_seen;     // rules already fed in
  Term     seed_red;         // goal_rhs -- the RED front's origin
  Term     seed_green;       // goal_lhs -- the GREEN front's origin
  u32      n_red, n_green;   // nodes reached per colour
  u32      n_anti;           // nodes reached via an up (order-inc) step
  u32      n_dup;            // same-colour duplicate terms (dropped)
  u32      n_trunc;          // node expansions that hit MNF_SUCC_CAP
  u8       full;             // node table reached MNF_MAX_NODES
  u8       joined;
  // WM mnfanalysis state: try_all mirrors MNF_AddTryAllElements (the
  // new-fact sweep visits all members vs irred-or-origin only); mode 1
  // = ExpandRewrite (post-2000 switch); interest[] holds the pruned
  // member set the ExpandRewrite checks normalize (origins + first
  // MNF_COUNT_NF irreducibles per colour, insertion order).
  u8       try_all;
  u8       mode;             // 0 = normal expansion, 1 = ExpandRewrite
  u32      interest[2u + 2u * MNF_COUNT_NF];
  u32      n_interest;
  // Pending new-fact notifications (WM Z_ZieleErledigtMitNeuemFaktum
  // passes THE FACT; a slot-range diff misses facts that are kept and
  // then re-slotted/retired by interreduction churn between checks --
  // MeredithAxioms comm landed in slot 5 at fact 6 and slot 5 held a
  // different rule by the next goal check).  Terms here are GC roots
  // (see mnf_gc_*).
  Term     pend_l[256];
  Term     pend_r[256];
  u8       pend_or[256];
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Mirror pointers of the pending pair (stable across GC/compaction;
  // rules never free their mirror cells).  When set, the fact sweep
  // matches/instantiates off the spans and the Term slots may hold
  // sentinels (never walked).
  const AtpFtCell *pend_l_ft[256];
  const AtpFtCell *pend_r_ft[256];
  // Mode-1 incremental member-NF cache: per interesting node, the
  // INPUT span (built once), the last NF span + Term, and the
  // (generation, rule watermark) it was computed at.  A check reuses
  // the NF while the ftnfm generation is unchanged and no delta rule
  // [wm, n_rules) fires on the NF span (the interreduce-probe trick);
  // only a real NF change renormalizes and decodes.
  const AtpFtCell **nf_in_span;
  AtpFtCell **nf_span;
  u64   *nf_gen;
  u32   *nf_wm;
  // Per-node span twin (first MNF_SPAN_NODE_CAP nodes; see
  // g_mnf_ins_span).  The mode-1 intake aliases these, so the front
  // search never converts a member.
  const AtpFtCell **node_span;
  void  *ft;                 // the owning state's AtpFt arena (frees)
#endif
  u32      n_pend;
  u8       pend_of;          // overflow: fall back to a full re-sweep
  // The join witness (also used by the proof extractor, so always
  // present, not behind ATP_MNF_DIAG).  Side A is the existing table
  // node a fresh reduct collided with; side B is that fresh reduct --
  // never created as a node, so it is identified by its parent and the
  // colliding term.  meet_term == nodes[meet_a].term == the reduct.
  u32      meet_a;           // existing table node the join collided with
  u32      meet_b_parent;    // parent of the colliding (uncreated) node
  Term     meet_term;        // the term both fronts reached
  u8       meet_b_col;       // colour of the colliding (uncreated) node
} AtpMnf;

// Groundness (no free variables) -- the antiWOVar eligibility test for
// reversed-rule steps (WM MNF_ConvertRegelToAntiRegel, MNF.c:1419-1430).
static u8 mnf_term_is_ground(Term t) {
  if (term_tag(t) == TAG_FVR) return 0u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!mnf_term_is_ground(term_ctr_at(t, i))) return 0u;
    }
  }
  return 1u;
}

// Structural hash of a term (FNV-ish mix over the preorder).
static u32 mnf_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ mnf_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// Find a node whose term is kbo_eq to `t` (hash `h`); MNF_MAX_NODES if
// absent.  The table holds one node per distinct term -- the first
// colour to reach it.
static u32 mnf_lookup(const AtpMnf *m, Term t, u32 h) {
  u32 mask = m->n_buckets - 1u;
  for (u32 probe = h & mask; ; probe = (probe + 1u) & mask) {
    u32 slot = m->buckets[probe];
    if (slot == 0u) return MNF_MAX_NODES;
    u32 idx = slot - 1u;
    if (m->nodes[idx].hash == h && kbo_eq(m->nodes[idx].term, t)) return idx;
  }
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span sibling of mnf_hash: IDENTICAL values for equal content (the
// span-keyed mode-1 lookup depends on it; CTR sym == term_ext, var id
// == term_ext, arity == term_ctr_n by the ft encoding contract; NUM
// markers never appear in member NFs).
static u32 mnf_hash_span(const AtpFtCell *c) {
  if ((c->sym & WF_VAR_BIT) != 0u)
    return (0x2545f491u ^ (c->sym & ~WF_VAR_BIT)) * 0x01000193u;
  u32 h = 0x811c9dc5u ^ (ft_ctr_sym(c) * 0x01000193u);
  u32 n = (u32)c->arity;
  const AtpFtCell *k = c->next;
  for (u32 i = 0; i < n; i++) {
    h = (h ^ mnf_hash_span(k)) * 0x01000193u;
    k = k->end->next;
  }
  return h ^ (n + 0x9e3779b9u);
}

// Span-subject lookup: same probe sequence as mnf_lookup, the exact
// FT-vs-Term compare against the node terms.
static u32 mnf_lookup_span(const AtpMnf *m, const AtpFtCell *c, u32 h) {
  u32 mask = m->n_buckets - 1u;
  for (u32 probe = h & mask; ; probe = (probe + 1u) & mask) {
    u32 slot = m->buckets[probe];
    if (slot == 0u) return MNF_MAX_NODES;
    u32 idx = slot - 1u;
    if (m->nodes[idx].hash == h && ft_eq_term(c, m->nodes[idx].term))
      return idx;
  }
}

// Node span twins ride only the EARLY nodes -- the interest set is
// drawn from the members alive at the mode switch (MNF_SWITCH_NF), so
// spans past this index would never be read and 400k of them would
// cost real memory.
#define MNF_SPAN_NODE_CAP 4096u

// Rebuild `c` with the subterm at child-index path p[0..p_len)
// replaced by `rep` (adopted verbatim) -- the FT sibling of
// cp_replace_at, through ftnew_* so the GROUND fold and preorder
// threading come out exactly as a fresh conversion would.
static AtpFtCell *ft_replace_at_span(AtpFt *a, const AtpFtCell *c,
                                     const u32 *p, u32 p_len,
                                     AtpFtCell *rep) {
  if (p_len == 0u) return rep;
  u32 n = (u32)c->arity;
  if (p[0] >= n) return NULL;
  AtpFtCell *kids_stack[16];
  AtpFtCell **kids = kids_stack;
  AtpFtCell **kids_heap = NULL;
  if (n > 16u) {
    kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)n);
    if (kids_heap == NULL) thvm_fatal("ft_replace_at_span: OOM");
    kids = kids_heap;
  }
  const AtpFtCell *k = c->next;
  u8 fail = 0u;
  for (u32 i = 0; i < n; i++) {
    if (i == p[0]) {
      kids[i] = ft_replace_at_span(a, k, p + 1, p_len - 1, rep);
      if (kids[i] == NULL) fail = 1u;
    } else {
      kids[i] = ft_clone_span(a, k, 0);
    }
    k = k->end->next;
  }
  AtpFtCell *out = fail ? NULL : ftnew_ctr(a, ft_ctr_sym(c), (u16)n, kids, 0);
  free(kids_heap);
  return out;
}

// Consume-once side-band into mnf_insert (the g_mnf_trace_slot
// precedent): the candidate's span twin.  Ownership passes to the
// insert -- adopted by a fresh node under MNF_SPAN_NODE_CAP, freed on
// every other outcome.
static const AtpFtCell *g_mnf_ins_span = NULL;

// Mode-1 member INPUT span intake.  A seed member IS one goal side:
// adopt a clone of its mirror (ft_eq_term-proven, bit-identical to a
// fresh conversion by the clone contract) instead of converting; the
// mid-front irreducibles fall back to the one-time conversion.
static const AtpFtCell *mnf_member_in_span(AtpState *s, const AtpMnf *m,
                                           u32 par) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Expansion-born members carry a span twin from birth: alias it
  // (read-only input; the per-check normalize clones it anyway).
  if (m->node_span != NULL && m->node_span[par] != NULL)
    return m->node_span[par];
  Term t = m->nodes[par].term;
  const AtpFtCell *cand[2] = { s->goal_lhs_ft, s->goal_rhs_ft };
  for (u32 i = 0; i < 2u; i++) {
    if (cand[i] != NULL && ft_eq_term(cand[i], t))
      return ft_clone_span(a, cand[i], 0);
  }
  return ft_from_term(a, t, 0);
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// THVM_ATP_MNF_TRACE side-band: the creating step's fact slot, face
// (0 = lhs->rhs), and redex position, set by mnf_exp_visit right
// before each insert.  Debug-only.
static int  g_mnf_trace_slot = -1;
static int  g_mnf_trace_face = -1;
static char g_mnf_trace_pos[64] = "-";

// Insert `t` (colour `col`, lineage step counters `gl`/`antir`, reached
// from node `parent` -- MNF_ROOT_PARENT for a seed).  Returns 1 if this
// insertion JOINED the fronts (an opposite-colour node already held
// `t`).  Same-colour duplicate -> dropped.  Fresh term -> added,
// indexed, enqueued.  `out_idx` (optional) receives the node index of
// the term -- the fresh node, or the existing same-colour duplicate.
static int mnf_insert(AtpMnf *m, Term t, u8 col, u8 gl, u8 antir,
                      u32 parent, u32 *out_idx) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Consume the span side-band exactly once per call.
  const AtpFtCell *ins_span = g_mnf_ins_span;
  g_mnf_ins_span = NULL;
#endif
  u32 h   = mnf_hash(t);
  u32 idx = mnf_lookup(m, t, h);
  u16 tot = (parent == MNF_ROOT_PARENT)
          ? 0u : (u16)(m->nodes[parent].tot_steps + 1u);
  if (out_idx != NULL) *out_idx = idx;
  if (idx != MNF_MAX_NODES) {
    if (m->nodes[idx].colour != col) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      if (ins_span != NULL && m->ft != NULL)
        ft_free_span((AtpFt *)m->ft, (AtpFtCell *)ins_span,
                     (AtpFtCell *)ins_span->end);
#endif
      m->meet_a        = idx;
      m->meet_b_parent = parent;
      m->meet_term     = t;
      m->meet_b_col    = col;
      m->joined = 1u;
      return 1;
    }
    // WM AddData duplicate merge (MNF.c:254-287): a strictly SHORTER
    // re-derivation replaces the existing member's step counters and
    // lineage -- the up-step budgets of future expansions then follow
    // the shorter path, exactly like WM's anzGesSchritte adoption.
    if (tot < m->nodes[idx].tot_steps) {
      m->nodes[idx].tot_steps   = tot;
      m->nodes[idx].gl_steps    = gl;
      m->nodes[idx].antir_steps = antir;
      m->nodes[idx].parent      = parent;
    }
    m->n_dup++;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (ins_span != NULL && m->ft != NULL)
      ft_free_span((AtpFt *)m->ft, (AtpFtCell *)ins_span,
                   (AtpFtCell *)ins_span->end);
#endif
    return 0;
  }
  if (m->n_nodes >= MNF_MAX_NODES) {
    m->full = 1u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (ins_span != NULL && m->ft != NULL)
      ft_free_span((AtpFt *)m->ft, (AtpFtCell *)ins_span,
                   (AtpFtCell *)ins_span->end);
#endif
    return 0;
  }
  u32 ni = m->n_nodes++;
  if (out_idx != NULL) *out_idx = ni;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (m->node_span != NULL) {
    if (ins_span != NULL && ni >= MNF_SPAN_NODE_CAP) {
      if (m->ft != NULL)
        ft_free_span((AtpFt *)m->ft, (AtpFtCell *)ins_span,
                     (AtpFtCell *)ins_span->end);
      ins_span = NULL;
    }
    if (ni < MNF_SPAN_NODE_CAP) m->node_span[ni] = ins_span;
  }
#endif
  m->nodes[ni].term        = t;
  m->nodes[ni].hash        = h;
  m->nodes[ni].parent      = (parent == MNF_ROOT_PARENT) ? ni : parent;
  m->nodes[ni].colour      = col;
  m->nodes[ni].gl_steps    = gl;
  m->nodes[ni].antir_steps = antir;
  m->nodes[ni].expanded    = 0u;
  m->nodes[ni].irred       = 1u;  // until an order-decreasing successor
  m->nodes[ni].tot_steps   = tot;
  if (col == MNF_RED) m->n_red++; else m->n_green++;
  if (gl > 0u || antir > 0u) m->n_anti++;
  u32 mask = m->n_buckets - 1u;
  u32 probe = h & mask;
  while (m->buckets[probe] != 0u) probe = (probe + 1u) & mask;
  m->buckets[probe] = ni + 1u;
  // Push left onto the colour's stack (each node enqueued exactly once).
  if (col == MNF_RED) m->qred[--m->qred_head]     = ni;
  else                m->qgreen[--m->qgreen_head] = ni;
  // THVM_ATP_MNF_TRACE: one line per fresh front member, format-aligned
  // with the instrumented Waldmeister MNF_BearbeiteInsertElem dump
  // (==MNFINS, WM colour code 2=green 1=red, ids counting from 1) so
  // the two member streams diff directly.  slot/face/pos describe the
  // creating step (set by mnf_exp_visit; -1 = seed or NF chain).
  {
    static int mt = -1;
    if (mt < 0) mt = getenv("THVM_ATP_MNF_TRACE") != NULL ? 1 : 0;
    if (mt) {
      fprintf(stderr, "MNFINS id=%u col=%d gl=%u antir=%u slot=%d "
              "face=%d pos=%s t=",
              ni + 1u, (col == MNF_GREEN) ? 2 : 1, gl, antir,
              g_mnf_trace_slot, g_mnf_trace_face, g_mnf_trace_pos);
      atp_dbg_print_term(stderr, t);
      fputc('\n', stderr);
    }
  }
  return 0;
}

// Per-rule caches, refreshed once per mnf_step over the whole rule
// set so the successor recursion reads them instead of recomputing:
//   g_mnf_vc     -- vars(lhs) subset of vars(rhs) (rhs->lhs face safe)
//   g_mnf_vfwd   -- vars(rhs) subset of vars(lhs) (lhs->rhs face safe)
//   g_mnf_ground -- lhs is ground (antiWOVar: reversed-rule eligible)
//   g_mnf_ln     -- nodes(lhs[j]) -- the forward-match size pre-filter
//   g_mnf_rn     -- nodes(rhs[j]) -- the backward-match size pre-filter
// A one-way match needs nodes(pattern) <= nodes(subject), so a rule
// whose matched side outsizes the term cannot fire -- the integer
// compare skips the thvm_match entirely.
static u8  *g_mnf_vc     = NULL;
static u8  *g_mnf_vfwd   = NULL;
static u8  *g_mnf_ground = NULL;
static u32 *g_mnf_ln     = NULL;
static u32 *g_mnf_rn     = NULL;
static u32  g_mnf_vc_cap = 0u;

// One-step rewrites of `t` (all positions, rules [rule_lo, rule_hi),
// forward + -- when allow_anti -- variable-safe backward) collected
// (retired: the buffer-based mnf_successors; expansion now inserts
// inline during two preorder position walks, matching WM's
// BearbeiteR-then-BearbeiteE pass structure.)

// Expansion visitor context: node `ni`'s term is rewritten at every
// non-variable position (preorder -- WM's TO_Schwanz prefix walk) in
// TWO passes, rules first then equations (WM MNF_BearbeiteRE,
// MNF.c:703-710), each successor inserted inline.
//
// Step classes (WM's per-step Vergleich tag, MNF.c:341-422):
//   pass 0, fwd rule       -> Groesser a priori (class "down").
//   pass 0, reversed rule  -> Kleiner; only in the new-fact sweep, only
//                             GROUND rules (antiWOVar), mnfantir-gated.
//   pass 1, eq either face -> per-instance order on the redex pair
//                             (context-compatible == WM's whole-term
//                             ORD_VergleicheTerme, MNF.c:605): Groesser
//                             admitted; Kleiner mnfup-gated;
//                             Gleich/Unvergleichbar DISCARDED (:623).
typedef struct {
  AtpState *s;
  AtpMnf   *m;
  u32       ni;
  Term      root;
  u32       rule_lo, rule_hi;
  u8        in_sweep;
  u8        pass;            // 0 = rules, 1 = equations
  // Explicit single-fact sweep (WM BearbeiteEineR/EineGl): when
  // use_fact, the visitor tries THIS pair instead of the slot range --
  // immune to slot reuse between the keep and the goal check.
  u8        use_fact;
  Term      fl, fr;
  u8        f_orient;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Span twins of the fact pair (the pend queue stores mirror
  // pointers; a NULL keeps the Term route).
  const AtpFtCell *fl_ft, *fr_ft;
  // The expanded node's span twin (NULL past MNF_SPAN_NODE_CAP or on
  // a twin-less member): candidates then splice their own twins.
  const AtpFtCell *root_ft;
#endif
} MnfExpCtx;

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Term-side extras grounding for the unorientable r2 face: bind every
// template variable the match left unbound to the minimal constant.
// Applying the subst to the template is then exactly applying
// atp_unorient_template's grounded template -- without materializing
// it.  Returns 0 when a template var is outside the subst address
// space (mirrors ft_subst_ground_extras's ATPFT_MAX_VARS guard).
static u8 mnf_subst_ground_extras_term(AtpState *s, const AtpFtCell *tmpl,
                                       RewriteSubst *sub) {
  const AtpFtCell *e = (tmpl->end != NULL) ? tmpl->end->next : NULL;
  for (const AtpFtCell *q = tmpl; q != e; q = q->next) {
    if ((q->sym & WF_VAR_BIT) == 0u) continue;
    u32 id = q->sym & ~WF_VAR_BIT;
    if (id >= REWRITE_MAX_VAR) return 0u;
    if (sub->bindings[id] == 0) sub->bindings[id] = s->min_const;
  }
  return 1u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Build a candidate's span twin: match the pattern span against the
// subject cell of the node's twin, instantiate the replacement span
// under the span bindings (grounding r2 extras when asked), splice at
// the position.  Every step goes through the ftnew_* constructors, so
// the twin is bit-identical to converting the candidate Term.  NULL
// (twin-less) on any missing input -- the intake then falls back.
static const AtpFtCell *mnf_cand_span(AtpState *s,
                                      const AtpFtCell *root_ft,
                                      const u32 *p, u32 p_len,
                                      const AtpFtCell *pat_ft,
                                      const AtpFtCell *rep_ft,
                                      u8 ground_extras) {
  if (root_ft == NULL || pat_ft == NULL || rep_ft == NULL) return NULL;
  const AtpFtCell *t_ft = ft_subterm_at(root_ft, p, p_len);
  if (t_ft == NULL) return NULL;
  AtpFtSubst fs = {0};
  if (!ft_match(pat_ft, t_ft, &fs)) return NULL;
  if (ground_extras && !ft_subst_ground_extras(s, rep_ft, &fs)) return NULL;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *out = ft_subst_apply(a, rep_ft, &fs, 0);
  if (out == NULL) return NULL;
  AtpFtCell *whole = ft_replace_at_span(a, root_ft, p, p_len, out);
  if (whole == NULL) ft_free_span(a, out, out->end);
  return whole;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

static u32 mnf_exp_visit(const u32 *p, u32 p_len, void *raw) {
  MnfExpCtx *c = (MnfExpCtx *)raw;
  AtpState  *s = c->s;
  AtpMnf    *m = c->m;
  if (m->joined) return 0u;
  // Throttled wall-deadline / abort poll (a deep front over a large
  // rule set can spend seconds inside one expansion).
  {
    static u32 tick = 0u;
    if ((++tick & 0x3FFu) == 0u) {
      if (s->wall_deadline_us != 0u) {
        u64 now = atp_now_us();
        if (now != 0u && now >= s->wall_deadline_us) return 0u;
      }
      if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 0u;
    }
  }
  Term t = cp_subterm_at(c->root, p, p_len);
  if (t == 0 || term_tag(t) == TAG_FVR) return 0u;
  u32 size     = atp_symbol_count(t);
  u8  col      = m->nodes[c->ni].colour;
  u8  gl       = m->nodes[c->ni].gl_steps;
  u8  antir    = m->nodes[c->ni].antir_steps;
  u8  gl_ok    = (u8)(gl < MNF_UP_MAX);
  u8  antir_ok = (u8)(antir < MNF_ANTIR_MAX);
  {
    u32 pn = 0;
    g_mnf_trace_pos[pn++] = 'L';
    for (u32 d = 0; d < p_len && pn < 60u; d++)
      pn += (u32)snprintf(g_mnf_trace_pos + pn,
                          sizeof g_mnf_trace_pos - pn, ".%u", p[d] + 1u);
    g_mnf_trace_pos[pn] = '\0';
  }
  // Per-position candidate buffer: WM enumerates the matching faces at
  // a position in Gleichungsbaum/Regelbaum DFS order (MO_AllRegelMatch/
  // MO_AllGleichungMatch); thvm's slot scan collects then sorts by the
  // same pattern order (atp_ft_wm_pattern_before), newest fact first on
  // alpha-identical patterns (leaf chains are newest-first).
  enum { MNF_POS_CAND_CAP = 96 };
  struct MnfPosCand {
    Term whole;
    u8   gl_inc, antir_inc, down;
    u32  uid;
    int  slot, face;
    const AtpFtCell *pat;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    const AtpFtCell *whole_ft;   // the candidate's span twin (or NULL)
#endif
  } cand[MNF_POS_CAND_CAP];
  u32 n_cand = 0u;
  u32 n_iter = c->use_fact ? 1u : (c->rule_hi - c->rule_lo);
  for (u32 it = 0; it < n_iter && !m->joined; it++) {
    Term fl, fr;
    u8   orient, vfwd, vc, ground;
    u32  ln, rn;
    int  slot;
    if (c->use_fact) {
      fl = c->fl; fr = c->fr; orient = c->f_orient;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      if (c->fl_ft != NULL && c->fr_ft != NULL) {
        vc     = ft_vars_contained_span(c->fl_ft, c->fr_ft);
        vfwd   = ft_vars_contained_span(c->fr_ft, c->fl_ft);
        ground = (u8)((c->fl_ft->flags & ATPFT_FLAG_GROUND) != 0u);
        ln     = ft_symbol_count_span(c->fl_ft);
        rn     = ft_symbol_count_span(c->fr_ft);
      } else
#endif
      {
      vc     = (u8)atp_vars_contained(fl, fr);
      vfwd   = (u8)atp_vars_contained(fr, fl);
      ground = mnf_term_is_ground(fl);
      ln     = atp_symbol_count(fl);
      rn     = atp_symbol_count(fr);
      }
      slot   = -2;
    } else {
      u32 j = c->rule_lo + it;
      if (s->r_dead != NULL && s->r_dead[j]) continue;
      fl = s->lhs[j]; fr = s->rhs[j]; orient = s->r_orient[j];
      vc = g_mnf_vc[j]; vfwd = g_mnf_vfwd[j]; ground = g_mnf_ground[j];
      ln = g_mnf_ln[j]; rn = g_mnf_rn[j];
      slot = (int)j;
    }
    // Mirror patterns for the rule arm (NULL on the fact arm or when a
    // harness left a slot unmirrored) -- matching/instantiating off the
    // spans keeps lazy slots un-materialized.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    const AtpFtCell *fl_ft =
        (slot >= 0) ? s->lhs_ft[slot]
                    : (c->use_fact ? c->fl_ft : NULL);
    const AtpFtCell *fr_ft =
        (slot >= 0) ? s->rhs_ft[slot]
                    : (c->use_fact ? c->fr_ft : NULL);
#endif
    if (c->pass == 0u) {
      if (!orient) continue;
      if (ln <= size && n_cand < MNF_POS_CAND_CAP) {
        RewriteSubst sub = {{0}};
        u8 hit0;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        if (fl_ft != NULL) hit0 = (u8)ft_match_term(fl_ft, t, &sub);
        else
#endif
        hit0 = thvm_match(fl, t, &sub);
        if (hit0) {
          struct MnfPosCand *k = &cand[n_cand++];
          Term red0;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          if (fr_ft != NULL) red0 = ft_subst_apply_term(fr_ft, &sub);
          else
#endif
          red0 = thvm_subst_apply(fr, &sub);
          k->whole = cp_replace_at(c->root, p, p_len, red0);
          k->gl_inc = 0u; k->antir_inc = 0u; k->down = 1u;
          k->slot = slot; k->face = 0;
          k->uid = (slot >= 0) ? s->r_uid[slot] : 0xFFFFFFFFu;
#ifdef THVM_ATPFT_RULES
          k->pat = (slot >= 0) ? s->lhs_ft[slot] : NULL;
#else
          k->pat = NULL;
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          k->whole_ft =
              mnf_cand_span(s, c->root_ft, p, p_len, fl_ft, fr_ft, 0u);
#endif
        }
      }
      if (c->in_sweep && antir_ok && ground && rn <= size &&
          n_cand < MNF_POS_CAND_CAP) {
        RewriteSubst sb = {{0}};
        u8 hit1;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        if (fr_ft != NULL) hit1 = (u8)ft_match_term(fr_ft, t, &sb);
        else
#endif
        hit1 = thvm_match(fr, t, &sb);
        if (hit1) {
          struct MnfPosCand *k = &cand[n_cand++];
          Term red1;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          if (fl_ft != NULL) red1 = ft_subst_apply_term(fl_ft, &sb);
          else
#endif
          red1 = thvm_subst_apply(fl, &sb);
          k->whole = cp_replace_at(c->root, p, p_len, red1);
          k->gl_inc = 0u; k->antir_inc = 1u; k->down = 0u;
          k->slot = slot; k->face = 1;
          k->uid = (slot >= 0) ? s->r_uid[slot] : 0xFFFFFFFFu;
#ifdef THVM_ATPFT_RULES
          k->pat = (slot >= 0) ? s->rhs_ft[slot] : NULL;
#else
          k->pat = NULL;
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
          k->whole_ft =
              mnf_cand_span(s, c->root_ft, p, p_len, fr_ft, fl_ft, 0u);
#endif
        }
      }
    } else {
      if (orient) continue;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      // A lazy unorientable slot stays a sentinel: the span arm below
      // matches and instantiates off the mirrors, grounding the r2
      // extras through the subst.  Materialize only when a mirror is
      // missing (harness rows), where the Term arm still runs.
      if (slot >= 0 && (fl_ft == NULL || fr_ft == NULL) &&
          (term_tag(fl) == TAG_NUM || term_tag(fr) == TAG_NUM)) {
        fl = atp_rule_term(s, (u32)slot, 0u);
        fr = atp_rule_term(s, (u32)slot, 1u);
      }
#endif
      for (u8 face = 0u; face < 2u && !m->joined; face++) {
        Term pat = face ? fr : fl;
        Term rep = face ? fl : fr;
        u8   ok  = face ? vc : vfwd;
        u32  psz = face ? rn : ln;
        if (psz > size) continue;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        if (fl_ft != NULL && fr_ft != NULL) {
          const AtpFtCell *pat_ft = face ? fr_ft : fl_ft;
          const AtpFtCell *rep_ft = face ? fl_ft : fr_ft;
          if (!ok && (s->wpo != NULL || s->rpo != NULL))
            continue;                  // no ground-total order: no r2
          RewriteSubst sub = {{0}};
          if (!ft_match_term(pat_ft, t, &sub)) continue;
          if (!ok && !mnf_subst_ground_extras_term(s, rep_ft, &sub))
            continue;
          Term out = ft_subst_apply_term(rep_ft, &sub);
          KboCmp cmp = thvm_kbo(t, out, s->kbo);
          u8 up;
          if (cmp == KBO_GT) up = 0u;
          else if (cmp == KBO_LT && gl_ok) up = 1u;
          else continue;               // Gleich/Unvergleichbar/over-budget
          if (n_cand >= MNF_POS_CAND_CAP) break;
          struct MnfPosCand *k = &cand[n_cand++];
          k->whole = cp_replace_at(c->root, p, p_len, out);
          k->gl_inc = up; k->antir_inc = 0u; k->down = (u8)!up;
          k->slot = slot; k->face = (int)face;
          k->uid = (slot >= 0) ? s->r_uid[slot] : 0xFFFFFFFFu;
          k->pat = (slot >= 0) ? pat_ft : NULL;
          k->whole_ft = mnf_cand_span(s, c->root_ft, p, p_len,
                                      pat_ft, rep_ft, (u8)!ok);
          continue;
        }
#endif
        // WM r2 (RechtsUnfreiErzeugen, RUndEVerwaltung.c:391-430): a
        // face whose replacement has variables outside the matched side
        // is applied with those variables GROUNDED to the minimal
        // SIGNATURE constant (ground-total precedence only) -- this is
        // how skC3-grounded material enters the front (MeredithAxioms
        // member id 17, eq -4's l->r2 face).  Skipping the face instead
        // starves the front of every grounded lineage.
        Term rep_use = rep;
        if (!ok) {
          rep_use = atp_unorient_template(s, pat, rep);
          if (rep_use == 0) continue;   // no ground-total order: no r2
        }
        RewriteSubst sub = {{0}};
        if (!thvm_match(pat, t, &sub)) continue;
        Term out = thvm_subst_apply(rep_use, &sub);
        KboCmp cmp = thvm_kbo(t, out, s->kbo);
        u8 up;
        if (cmp == KBO_GT) up = 0u;
        else if (cmp == KBO_LT && gl_ok) up = 1u;
        else continue;                 // Gleich/Unvergleichbar/over-budget
        if (n_cand >= MNF_POS_CAND_CAP) break;
        struct MnfPosCand *k = &cand[n_cand++];
        k->whole = cp_replace_at(c->root, p, p_len, out);
        k->gl_inc = up; k->antir_inc = 0u; k->down = (u8)!up;
        k->slot = slot; k->face = (int)face;
        k->uid = (slot >= 0) ? s->r_uid[slot] : 0xFFFFFFFFu;
#ifdef THVM_ATPFT_RULES
        k->pat = (slot >= 0)
               ? (face ? s->rhs_ft[slot] : s->lhs_ft[slot]) : NULL;
#else
        k->pat = NULL;
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        k->whole_ft = NULL;          // mirror-less Term arm: no twin
#endif
      }
    }
  }
  // Stable insertion-sort into WM tree order: pattern DFS-minimum
  // first; alpha-identical patterns newest-fact-first.
  for (u32 a = 1; a < n_cand; a++) {
    struct MnfPosCand key = cand[a];
    u32 b = a;
    while (b > 0) {
      struct MnfPosCand *pr = &cand[b - 1];
      int key_first;
      if (key.pat != NULL && pr->pat != NULL) {
        if (atp_ft_wm_pattern_before(key.pat, pr->pat)) key_first = 1;
        else if (atp_ft_wm_pattern_before(pr->pat, key.pat)) key_first = 0;
        else key_first = (key.uid > pr->uid);
      } else {
        key_first = 0;                 // no FT patterns: keep scan order
      }
      if (!key_first) break;
      cand[b] = *pr;
      b--;
    }
    cand[b] = key;
  }
  for (u32 a = 0; a < n_cand && !m->joined; a++) {
    if (cand[a].down) m->nodes[c->ni].irred = 0u;
    g_mnf_trace_slot = cand[a].slot; g_mnf_trace_face = cand[a].face;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    g_mnf_ins_span = cand[a].whole_ft;
#endif
    mnf_insert(m, cand[a].whole, col,
               (u8)(gl + cand[a].gl_inc),
               (u8)(antir + cand[a].antir_inc), c->ni, NULL);
  }
  return 0u;
}

// Generate node `ni`'s successors against facts [rule_lo, rule_hi):
// pass 0 = rule steps at every position, pass 1 = equation steps at
// every position (WM MNF_BearbeiteRE order).  `in_sweep` distinguishes
// the new-fact sweep (reversed-rule steps allowed) from the PQ
// expansion, per WM MNF_BearbeiteEineR vs MNF_BearbeiteR.
static void mnf_expand_node(AtpState *s, AtpMnf *m, u32 ni,
                            u32 rule_lo, u32 rule_hi, u8 in_sweep) {
  u32 path[CP_MAX_DEPTH];
  MnfExpCtx c = { s, m, ni, m->nodes[ni].term, rule_lo, rule_hi,
                  in_sweep, 0u, 0u, 0, 0, 0u };
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  c.root_ft = (m->node_span != NULL && ni < MNF_SPAN_NODE_CAP)
                  ? m->node_span[ni] : NULL;
#endif
  cp_walk_positions(c.root, path, 0u, CP_MAX_DEPTH, mnf_exp_visit, &c, 0u);
  if (m->joined) return;
  c.pass = 1u;
  cp_walk_positions(c.root, path, 0u, CP_MAX_DEPTH, mnf_exp_visit, &c, 0u);
}

// Sweep node `ni` against ONE explicit fact (WM MNF_BearbeiteEineR /
// MNF_BearbeiteEineGl): rule pass then equation pass, reversed-rule
// steps admitted (in_sweep semantics).
static void mnf_expand_node_fact(AtpState *s, AtpMnf *m, u32 ni,
                                 Term fl, Term fr, u8 orient,
                                 const AtpFtCell *fl_ft,
                                 const AtpFtCell *fr_ft) {
  u32 path[CP_MAX_DEPTH];
  MnfExpCtx c = { s, m, ni, m->nodes[ni].term, 0u, 0u,
                  /*in_sweep=*/1u, 0u, /*use_fact=*/1u, fl, fr, orient };
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  c.root_ft = (m->node_span != NULL && ni < MNF_SPAN_NODE_CAP)
                  ? m->node_span[ni] : NULL;
  c.fl_ft = fl_ft;
  c.fr_ft = fr_ft;
#else
  (void)fl_ft;
  (void)fr_ft;
#endif
  cp_walk_positions(c.root, path, 0u, CP_MAX_DEPTH, mnf_exp_visit, &c, 0u);
  if (m->joined) return;
  c.pass = 1u;
  cp_walk_positions(c.root, path, 0u, CP_MAX_DEPTH, mnf_exp_visit, &c, 0u);
}

// Keep-site notification (WM ArbeitsAufnahme -> Z_ZieleErledigtMitNeuemFaktum
// passes the kept fact itself).  Queued here, drained by the next
// mnf_step -- immune to interreduction re-slotting between the keep and
// the goal check.
// 1 while the front search still consumes the pending fact pairs
// (sweep mode); the ExpandRewrite switch discards them unread.
static u8 mnf_in_sweep_mode(struct AtpMnf *m) {
  return (u8)(m != NULL && m->mode == 0u);
}

static void mnf_notify_fact(struct AtpMnf *m, Term l, Term r, u8 orient) {
  if (m == NULL) return;
  if (m->n_pend >= 256u) { m->pend_of = 1u; return; }
  m->pend_l [m->n_pend] = l;
  m->pend_r [m->n_pend] = r;
  m->pend_or[m->n_pend] = orient;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  m->pend_l_ft[m->n_pend] = g_mnf_pend_ft_l;   // staged by the keep site
  m->pend_r_ft[m->n_pend] = g_mnf_pend_ft_r;
  g_mnf_pend_ft_l = NULL;
  g_mnf_pend_ft_r = NULL;
#endif
  m->n_pend++;
}

// Create the MNF set for the current goal: goal_lhs -> GREEN front,
// goal_rhs -> RED front.  An immediate collision (goal_lhs kbo_eq
// goal_rhs) already joins.
static AtpMnf *mnf_create(AtpState *s) {
  AtpMnf *m = (AtpMnf *)calloc(1, sizeof(AtpMnf));
  if (m == NULL) return NULL;
  m->cap_nodes = MNF_MAX_NODES;
  m->q_cap     = MNF_MAX_NODES;
  m->n_buckets = MNF_N_BUCKETS;
  m->nodes   = (MnfNode *)calloc(m->cap_nodes, sizeof(MnfNode));
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  m->nf_in_span = (const AtpFtCell **)calloc(m->cap_nodes, sizeof(void *));
  m->nf_span    = (AtpFtCell **)calloc(m->cap_nodes, sizeof(void *));

  m->nf_gen     = (u64 *)calloc(m->cap_nodes, sizeof(u64));
  m->nf_wm      = (u32 *)calloc(m->cap_nodes, sizeof(u32));
  m->node_span  = (const AtpFtCell **)calloc(MNF_SPAN_NODE_CAP,
                                             sizeof(void *));
  m->ft         = s->ft_arena_ptr;
#endif
  m->qred    = (u32 *)calloc(m->q_cap, sizeof(u32));
  m->qgreen  = (u32 *)calloc(m->q_cap, sizeof(u32));
  m->buckets = (u32 *)calloc(m->n_buckets, sizeof(u32));
  if (m->nodes == NULL || m->qred == NULL || m->qgreen == NULL ||
      m->buckets == NULL) {
    free(m->nodes); free(m->qred); free(m->qgreen);
    free(m->buckets); free(m);
    return NULL;
  }
  // Stacks start empty at the right end -- mnf_insert pushes left.
  m->qred_head   = m->qred_tail   = m->q_cap;
  m->qgreen_head = m->qgreen_tail = m->q_cap;
  // WM canonicalizes the GOAL pair with LRSortieren like every stored
  // pair (the MeredithAxioms OrAssociativity goal prints with the .pr
  // sides swapped: orginalLinks = the structurally-smaller side).  The
  // GREEN front seeds from the stored links; mirror the sort here so
  // the two engines' colour/side assignment -- and with it the
  // red-first expansion order and the whole member stream -- line up.
  Term gl_seed = s->goal_lhs, gr_seed = s->goal_rhs;
  if (atp_lr_sortieren_rec(gl_seed, gr_seed) > 0) {
    Term tmp = gl_seed; gl_seed = gr_seed; gr_seed = tmp;
  }
  m->seed_green = gl_seed;         // the GREEN front's origin
  m->seed_red   = gr_seed;         // the RED front's origin
  m->try_all    = 1u;              // MNF_AddTryAllElements initial state
  m->mode       = 0u;
  m->n_interest = 0u;
  m->n_pend     = 0u;
  m->pend_of    = 0u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Seed span twins: adopt goal-mirror clones (ft_eq_term-proven; the
  // LRSortieren swap above may have crossed the pairing, so each seed
  // tries both mirrors).  A missing/stale mirror leaves the twin NULL
  // -- the mode-1 intake then falls back to its own proof-or-convert.
  {
    AtpFt *sfa = (AtpFt *)s->ft_arena_ptr;
    const AtpFtCell *mc[2] = { s->goal_lhs_ft, s->goal_rhs_ft };
    Term seeds[2] = { gl_seed, gr_seed };
    const AtpFtCell *seed_ft[2] = { NULL, NULL };
    for (u32 si = 0; si < 2u; si++) {
      for (u32 ci = 0; ci < 2u; ci++) {
        if (mc[ci] != NULL && ft_eq_term(mc[ci], seeds[si])) {
          seed_ft[si] = ft_clone_span(sfa, mc[ci], 0);
          break;
        }
      }
    }
    g_mnf_ins_span = seed_ft[0];
    mnf_insert(m, gl_seed, MNF_GREEN, 0u, 0u, MNF_ROOT_PARENT, NULL);
    g_mnf_ins_span = seed_ft[1];
    mnf_insert(m, gr_seed, MNF_RED,   0u, 0u, MNF_ROOT_PARENT, NULL);
  }
#else
  mnf_insert(m, gl_seed, MNF_GREEN, 0u, 0u, MNF_ROOT_PARENT, NULL);
  mnf_insert(m, gr_seed, MNF_RED,   0u, 0u, MNF_ROOT_PARENT, NULL);
#endif
  return m;
}

static void mnf_destroy(struct AtpMnf *m) {
  if (m == NULL) return;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  free(m->nf_in_span);
  free(m->nf_span);

  free(m->nf_gen);
  free(m->nf_wm);
  free(m->node_span);
#endif
  free(m->nodes); free(m->qred); free(m->qgreen); free(m->buckets); free(m);
}

// GC support: the coloured nodes' terms are collector roots.  Gather
// them for thvm_atp_gc_collect and write the relocated terms back; the
// hash table (structural hashes, node indices) is GC-invariant.
static u32 mnf_gc_count(struct AtpMnf *m) {
  if (m == NULL) return 0u;
  return m->n_nodes + 2u * m->n_pend;
}
static void mnf_gc_gather(struct AtpMnf *m, Term *roots, u32 *w) {
  if (m == NULL) return;
  for (u32 i = 0; i < m->n_nodes; i++) roots[(*w)++] = m->nodes[i].term;
  for (u32 i = 0; i < m->n_pend; i++) {
    roots[(*w)++] = m->pend_l[i];
    roots[(*w)++] = m->pend_r[i];
  }
}
static void mnf_gc_writeback(struct AtpMnf *m, const Term *roots, u32 base) {
  if (m == NULL) return;
  for (u32 i = 0; i < m->n_nodes; i++) m->nodes[i].term = roots[base + i];
  u32 w = base + m->n_nodes;
  for (u32 i = 0; i < m->n_pend; i++) {
    m->pend_l[i] = roots[w++];
    m->pend_r[i] = roots[w++];
  }
}

// Take a node from a colour's worklist.  WM's PQ default discipline is
// a plain STACK (MNF.c:1094: stack unless the -zb string says deque or
// fifo; the -auto preset says neither) -- always pop the NEWEST.
static u32 mnf_pop(u32 *q, u32 *head, u32 *tail, u8 last_irred) {
  (void)tail; (void)last_irred;
  return q[(*head)++];                   // LIFO end: newest
}

#ifdef ATP_MNF_DIAG
// Independent join verifier.  A join is sound because every MNF node is,
// by construction, a one-step equational rewrite of its parent (forward
// l->r or variable-safe backward r->l) -- so goal_lhs ->* meet <-* goal_rhs
// is a closed equational chain.  This re-checks that claim from the parent
// pointers: walk `ni` up to its root and confirm each child term is a
// genuine one-step rewrite of its parent under the FINAL rule set.  Steps
// taken with a rule that interreduction has since retired will not replay
// (still equationally valid -- just no longer in R); `*replayed` counts
// the ones that do.  Returns the root node index.
static int mnf_edge_step(AtpState *s, Term before, Term after,
                         const Term *rl, const Term *rr, u32 nr,
                         u8 *pos, u8 depth,
                         u32 *out_rule, u8 *out_pos_len, u8 *out_fwd);

static u32 mnf_verify_chain(AtpState *s, AtpMnf *m, u32 ni,
                            u32 *len, u32 *replayed) {
  u32 guard = 0u;
  while (m->nodes[ni].parent != ni && guard++ < m->n_nodes) {
    u32 p = m->nodes[ni].parent;
    u8 pos[ATP_PROOF_MAX_DEPTH]; u32 er; u8 epl, efw;
    if (mnf_edge_step(s, m->nodes[p].term, m->nodes[ni].term,
                      s->lhs, s->rhs, s->n_rules, pos, 0u,
                      &er, &epl, &efw)) (*replayed)++;
    (*len)++;
    ni = p;
  }
  return ni;
}

// Re-check the join captured by mnf_insert and report to stderr.
static void mnf_verify(AtpState *s, AtpMnf *m) {
  u32 lenA = 0u, repA = 0u;
  u32 rootA = mnf_verify_chain(s, m, m->meet_a, &lenA, &repA);
  // Side B's node was never created -- meet_term is a one-step rewrite of
  // meet_b_parent; verify that link, then walk meet_b_parent to its root.
  u32 lenB = 1u, repB = 0u;
  {
    u8 pos[ATP_PROOF_MAX_DEPTH]; u32 er; u8 epl, efw;
    if (mnf_edge_step(s, m->nodes[m->meet_b_parent].term, m->meet_term,
                      s->lhs, s->rhs, s->n_rules, pos, 0u,
                      &er, &epl, &efw)) repB++;
  }
  u32 rootB = mnf_verify_chain(s, m, m->meet_b_parent, &lenB, &repB);
  u8   colA      = m->nodes[m->meet_a].colour;
  Term grootA    = m->nodes[rootA].term;
  Term grootB    = m->nodes[rootB].term;
  // colA owns side A; meet_b_col (opposite) owns side B.
  int roots_ok = (colA == MNF_GREEN)
      ? (kbo_eq(grootA, m->seed_green) && kbo_eq(grootB, m->seed_red))
      : (kbo_eq(grootA, m->seed_red)   && kbo_eq(grootB, m->seed_green));
  u32 green_len = (colA == MNF_GREEN) ? lenA : lenB;
  u32 red_len   = (colA == MNF_GREEN) ? lenB : lenA;
  u32 green_rep = (colA == MNF_GREEN) ? repA : repB;
  u32 red_rep   = (colA == MNF_GREEN) ? repB : repA;
  fprintf(stderr,
      "[mnf] JOIN: meet has %u symbols; green-side chain %u step(s) "
      "(%u replay under final R), red-side chain %u step(s) (%u replay); "
      "chain roots == goal: %s\n",
      atp_symbol_count(m->meet_term),
      green_len, green_rep, red_len, red_rep,
      roots_ok ? "YES" : "NO -- BUG");
}
#endif /* ATP_MNF_DIAG */

// One MNF advance: (a) feed any rules completion derived since the last
// call to every already-expanded node; (b) expand up to `budget` queued
// (first-expansion) nodes against the full current R.  Returns 1 once
// the fronts have joined.
// Per-node guard for the MNF expansion loops.  Returns 1 when the loop
// must stop, for either of two reasons:
//
//  - Wall deadline.  A single mnf_step can re-expand a large node table
//    against many newly-derived rules without ever returning to
//    thvm_atp_step's per-step wall check, so a hard goal can run far
//    past TimeConstraint inside one mnf_step.  Poll the deadline here
//    (throttled to every 256th call, since clock_gettime per node over
//    a 400k-node table is pure overhead) and bail; goal_check returns
//    and the next step iteration reports ATP_TIMEOUT.
//  - Heap pressure.  Between node expansions every live MNF term is
//    parked in m->nodes (rooted by thvm_atp_gc_collect) and no
//    expand-local term is allocated yet, so a Cheney collection here is
//    safe.  If heap is still under pressure after a collection -- the
//    live working set alone exceeds the half-space -- the front must
//    stop growing or it would exhaust from-space and abort the process;
//    set m->full so the caller stops.  Completion keeps running and the
//    goal still closes by single-NF or a later collision.
static int mnf_loop_guard(AtpState *s, AtpMnf *m) {
  static u32 tick = 0u;
  if ((++tick & 0xFFu) == 0u) {
    if (s->wall_deadline_us != 0u) {
      u64 now = atp_now_us();
      if (now != 0u && now >= s->wall_deadline_us) return 1;
    }
    if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 1;
  }
  if (!atp_heap_under_pressure()) return 0;
  thvm_atp_gc_collect(s);
  if (atp_heap_under_pressure()) { m->full = 1u; return 1; }
  return 0;
}

// The mnfanalysis=1000.2000.10 switch (WM MNF_AnalyseNM MNF.c:841-854 +
// MNF_SearchInteresting :1678-1704): enter ExpandRewrite mode and prune
// the working set to the origins plus the FIRST MNF_COUNT_NF
// irreducible members per colour, in insertion order.  (WM's hash keeps
// every member for dedup/join -- so does thvm's table; only the set of
// members the per-check normalize visits shrinks.)
static void mnf_switch_rewrite_mode(AtpMnf *m) {
  m->mode = 1u;
  m->n_interest = 0u;
  // WM MNF_SearchInteresting (MNF.c:1692-1699): the per-colour counters
  // START AT 1 (the origins are pre-counted), so only CountNF-1 = 9
  // additional irreducibles per colour survive the prune.
  u32 kept_red = 1u, kept_green = 1u;
  for (u32 i = 0; i < m->n_nodes; i++) {
    if (m->n_interest >= 2u + 2u * MNF_COUNT_NF) break;
    u8 origin = (m->nodes[i].parent == i);
    if (origin) { m->interest[m->n_interest++] = i; continue; }
    // WM MNF_FilterInteresting (MNF.c:1682) tests the raw `irred` FLAG,
    // NOT the MNF_tirred macro (irred && expanded): an unexpanded member
    // keeps its default irred=1 and qualifies; the flag is cleared only
    // when the member is expanded and yields a strictly smaller child.
    // Requiring `expanded` here dropped the not-yet-popped grounded
    // members the join needs, delaying it one check.
    if (!m->nodes[i].irred) continue;
    if (m->nodes[i].colour == MNF_RED) {
      if (kept_red   < MNF_COUNT_NF) { m->interest[m->n_interest++] = i; kept_red++; }
    } else {
      if (kept_green < MNF_COUNT_NF) { m->interest[m->n_interest++] = i; kept_green++; }
    }
  }
  {
    static int mt = -1;
    if (mt < 0) mt = getenv("THVM_ATP_MNF_TRACE") != NULL ? 1 : 0;
    if (mt) {
      fprintf(stderr, "MNFSWITCH nodes=%u interest=[", m->n_nodes);
      for (u32 k = 0; k < m->n_interest; k++)
        fprintf(stderr, "%s%u/%c", k ? "," : "", m->interest[k] + 1u,
                m->nodes[m->interest[k]].colour == MNF_GREEN ? 'g' : 'r');
      fprintf(stderr, "]\n");
    }
  }
}

// One R+E-ordered rewrite step of `t` (the same mixmost strategy the
// goal check normalizes with; step_cap caps the splice budget).
// Returns `t` itself when no redex fires.
static Term mnf_norm_steps(AtpState *s, Term t, u32 step_cap) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  return ft_to_term(atp_rewrite_normalize_ft(s, ft_from_term(a, t, 0),
                                             step_cap));
#else
  return atp_rewrite_normalize(s, t, s->lhs, s->rhs, s->n_rules, step_cap);
#endif
}

static int mnf_step(AtpState *s, AtpMnf *m, u32 budget) {
  (void)budget;   // WM's expansion loop has no per-check budget
  if (m->joined) return 1;
  // Refresh the per-rule caches (face var-safety, groundness, node
  // counts) for the whole current rule set -- mnf_successors reads
  // them instead of recomputing at every node it expands this call.
  if (s->n_rules > g_mnf_vc_cap) {
    u32 cap = g_mnf_vc_cap ? g_mnf_vc_cap : 256u;
    while (cap < s->n_rules) cap *= 2u;
    u8  *nv = (u8  *)realloc(g_mnf_vc, cap);
    u8  *nf = (u8  *)realloc(g_mnf_vfwd, cap);
    u8  *ng = (u8  *)realloc(g_mnf_ground, cap);
    u32 *nl = (u32 *)realloc(g_mnf_ln, cap * sizeof(u32));
    u32 *nr = (u32 *)realloc(g_mnf_rn, cap * sizeof(u32));
    if (nv == NULL || nf == NULL || ng == NULL || nl == NULL || nr == NULL)
      thvm_fatal("mnf_step: rule cache OOM");
    g_mnf_vc = nv; g_mnf_vfwd = nf; g_mnf_ground = ng;
    g_mnf_ln = nl; g_mnf_rn = nr; g_mnf_vc_cap = cap;
  }
  for (u32 j = 0; j < s->n_rules; j++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Lazy slots: every cache entry computes off the mirrors (the
    // expansion itself matches span patterns now), so no rule ever
    // materializes for the front search.
    if (term_tag(s->lhs[j]) == TAG_NUM &&
        s->lhs_ft[j] != NULL && s->rhs_ft[j] != NULL) {
      g_mnf_vc[j]     = ft_vars_contained_span(s->lhs_ft[j], s->rhs_ft[j]);
      g_mnf_vfwd[j]   = ft_vars_contained_span(s->rhs_ft[j], s->lhs_ft[j]);
      g_mnf_ground[j] =
          (u8)((s->lhs_ft[j]->flags & ATPFT_FLAG_GROUND) != 0u);
      g_mnf_ln[j]     = ft_symbol_count_span(s->lhs_ft[j]);
      g_mnf_rn[j]     = ft_symbol_count_span(s->rhs_ft[j]);
      continue;
    }
#endif
    g_mnf_vc[j]     = (u8)atp_vars_contained(s->lhs[j], s->rhs[j]);
    g_mnf_vfwd[j]   = (u8)atp_vars_contained(s->rhs[j], s->lhs[j]);
    g_mnf_ground[j] = mnf_term_is_ground(s->lhs[j]);
    g_mnf_ln[j]     = atp_symbol_count(s->lhs[j]);
    g_mnf_rn[j]     = atp_symbol_count(s->rhs[j]);
  }
#ifdef ATP_MNF_DIAG
  { static u32 c = 0;
    if (c++ % 16u == 0u) {
      fprintf(stderr,
        "[mnf] call=%-6u mode=%u nodes=%-7u red=%-7u green=%-7u "
        "queue(r=%u g=%u) anti=%u dup=%u trunc=%u full=%u rules=%u/%u\n",
        c, m->mode, m->n_nodes, m->n_red, m->n_green,
        m->qred_tail - m->qred_head, m->qgreen_tail - m->qgreen_head,
        m->n_anti, m->n_dup, m->n_trunc, m->full,
        m->n_rules_seen, s->n_rules);
    } }
#endif
  if (m->mode == 1u) {
    // ExpandRewrite (WM MNF_PseudoExpand, MNF.c:1822-1910): per check,
    // fully normalize each interesting member under the CURRENT rule
    // set and test ONLY the final NF -- against the main table (an
    // opposite-colour member = join, MNF.c:1887-1897) and against this
    // round's other NFs (WM's per-round side hash, :1866-1878).
    // Intermediates are NOT tested and nothing enters the main table
    // here (WM's round NFs live in the side hash only) -- testing the
    // whole chain over-joined 190 kept facts early on MeredithAxioms
    // OrAssociativity.  Only once a join is established is the winning
    // chain materialized node-by-node for the proof extractor.  New
    // facts need no separate sweep -- WM's AddRegelNM/AddGleichungNM
    // are no-ops in this mode; the fresh normalize sees the new rules.
    m->n_rules_seen = s->n_rules;
    m->n_pend  = 0u;
    m->pend_of = 0u;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    const AtpFtCell *nf_spanr[2u + 2u * MNF_COUNT_NF];
#else
    Term nf_term[2u + 2u * MNF_COUNT_NF];
#endif
    u8   nf_col [2u + 2u * MNF_COUNT_NF];
    u32  nf_from[2u + 2u * MNF_COUNT_NF];
    u32  n_nf = 0u;
    if (mnf_loop_guard(s, m)) return m->joined;
    for (u32 k = 0; k < m->n_interest && !m->joined; k++) {
      u32 par = m->interest[k];
      u8  col = m->nodes[par].colour;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
      // Incremental member NF, span-authoritative: reuse while the
      // ftnfm generation holds and no delta rule fires on the cached
      // NF span; renormalize the cached INPUT span (built once per
      // member) only on a hit.  No Term is decoded in this loop --
      // the table lookup and the round side hash both run on the
      // span; only a latched JOIN materializes the witness Terms.
      const AtpFtCell *nfsp;
      atp_ft_mirror_ensure(s);
      AtpFt *nfa = (AtpFt *)s->ft_arena_ptr;
      if (m->nf_span[par] != NULL &&
          m->nf_gen[par] == atp_ftnfm_gen() &&
          (m->nf_wm[par] == s->n_rules ||
           !atp_ft_slice_fires(s, m->nf_span[par], m->nf_wm[par],
                               s->n_rules - m->nf_wm[par]))) {
        m->nf_wm[par] = s->n_rules;
        nfsp = m->nf_span[par];
      } else {
        if (m->nf_in_span[par] == NULL) {
          m->nf_in_span[par] = mnf_member_in_span(s, m, par);
        }
        AtpFtCell *nfs = atp_rewrite_normalize_ft(
            s, ft_clone_span(nfa, m->nf_in_span[par], 0), 4096u);
        m->nf_span[par] = nfs;
        m->nf_gen[par]  = atp_ftnfm_gen();
        m->nf_wm[par]   = s->n_rules;
        nfsp = nfs;
      }
      {
        static int mt = -1;
        if (mt < 0) mt = getenv("THVM_ATP_MNF_NF_TRACE") != NULL ? 1 : 0;
        if (mt) {
          fprintf(stderr, "MNFNF id=%u col=%d nf=", par + 1u,
                  (col == MNF_GREEN) ? 2 : 1);
          atp_dbg_print_term(stderr, ft_to_term(nfsp));
          fputc('\n', stderr);
        }
      }
      // (a) main-table collision on the NF.  Record the join by
      // inserting the NF as a node under `par` (ONE normalize edge):
      // the insert collides with the opposite-colour member and latches
      // the witness.  Do NOT re-derive the chain stepwise -- a
      // restart-from-root single-step walk can reach a DIFFERENT normal
      // form than the one-shot mixmost normalize (which continues from
      // the splice position), silently losing the join (MeredithAxioms
      // AndAssociativity joined 3 checks late).  The normalize edge is
      // multi-step, so the proof extractor's single-step replay skips
      // it (extraction degrades, the verdict does not).
      u32 idx = mnf_lookup_span(m, nfsp, mnf_hash_span(nfsp));
      if (idx != MNF_MAX_NODES && m->nodes[idx].colour != col) {
        g_mnf_ins_span = ft_clone_span(nfa, nfsp, 0);
        mnf_insert(m, ft_to_term(nfsp), col, m->nodes[par].gl_steps,
                   m->nodes[par].antir_steps, par, NULL);
        break;
      }
      // (b) this round's opposite-colour NFs (the side hash): insert
      // the earlier member's NF as its node first, then ours -- the
      // second insert collides cross-colour and latches the join.
      for (u32 j = 0; j < n_nf; j++) {
        if (nf_col[j] != col && ft_eq(nf_spanr[j], nfsp)) {
          u32 fidx = MNF_MAX_NODES;
          g_mnf_ins_span = ft_clone_span(nfa, nf_spanr[j], 0);
          mnf_insert(m, ft_to_term(nf_spanr[j]), nf_col[j],
                     m->nodes[nf_from[j]].gl_steps,
                     m->nodes[nf_from[j]].antir_steps, nf_from[j], &fidx);
          if (!m->joined) {
            g_mnf_ins_span = ft_clone_span(nfa, nfsp, 0);
            mnf_insert(m, ft_to_term(nfsp), col, m->nodes[par].gl_steps,
                       m->nodes[par].antir_steps, par, NULL);
          }
          break;
        }
      }
      if (m->joined) break;
      nf_spanr[n_nf] = nfsp;
      nf_col [n_nf] = col;
      nf_from[n_nf] = par;
      n_nf++;
#else
      Term nf = mnf_norm_steps(s, m->nodes[par].term, 4096u);
      {
        static int mt = -1;
        if (mt < 0) mt = getenv("THVM_ATP_MNF_NF_TRACE") != NULL ? 1 : 0;
        if (mt) {
          fprintf(stderr, "MNFNF id=%u col=%d nf=", par + 1u,
                  (col == MNF_GREEN) ? 2 : 1);
          atp_dbg_print_term(stderr, nf);
          fputc('\n', stderr);
        }
      }
      u32 idx = mnf_lookup(m, nf, mnf_hash(nf));
      if (idx != MNF_MAX_NODES && m->nodes[idx].colour != col) {
        mnf_insert(m, nf, col, m->nodes[par].gl_steps,
                   m->nodes[par].antir_steps, par, NULL);
        break;
      }
      for (u32 j = 0; j < n_nf; j++) {
        if (nf_col[j] != col && kbo_eq(nf_term[j], nf)) {
          u32 fidx = MNF_MAX_NODES;
          mnf_insert(m, nf_term[j], nf_col[j],
                     m->nodes[nf_from[j]].gl_steps,
                     m->nodes[nf_from[j]].antir_steps, nf_from[j], &fidx);
          if (!m->joined)
            mnf_insert(m, nf, col, m->nodes[par].gl_steps,
                       m->nodes[par].antir_steps, par, NULL);
          break;
        }
      }
      if (m->joined) break;
      nf_term[n_nf] = nf;
      nf_col [n_nf] = col;
      nf_from[n_nf] = par;
      n_nf++;
#endif
    }
#ifdef ATP_MNF_DIAG
    if (m->joined) mnf_verify(s, m);
#endif
    return m->joined;
  }
  // (a) New-fact sweep (WM MNF_AddRENM, MNF.c:1554-1609): feed the
  // facts completion derived since the last call to every already-
  // expanded member -- under try_all==0 (past the SwitchIrred count)
  // only irreducible-or-origin members.  Reversed-rule steps are
  // admitted here (in_sweep=1), never in the PQ expansion below.
  {
    // Pending-fact overflow: re-sweep everything (rare; 256 keeps
    // between two goal checks).
    if (m->pend_of) {
      m->pend_of = 0u;
      m->n_pend  = 0u;
      u32 upto = m->n_nodes;
      for (u32 ni = 0; ni < upto && !m->joined; ni++) {
        if (!m->nodes[ni].expanded) continue;
        if (!m->try_all &&
            !(m->nodes[ni].irred || m->nodes[ni].parent == ni)) continue;
        if (mnf_loop_guard(s, m)) break;
        mnf_expand_node(s, m, ni, 0u, s->n_rules, /*in_sweep=*/1u);
      }
    }
    u32 upto = m->n_nodes;
    {
      static int mt = -1;
      if (mt < 0) mt = getenv("THVM_ATP_MNF_TRACE") != NULL ? 1 : 0;
      if (mt)
        fprintf(stderr, "MNFCHK nodes=%u pend=%u\n", m->n_nodes, m->n_pend);
    }
    for (u32 f = 0; f < m->n_pend && !m->joined; f++) {
      for (u32 ni = 0; ni < upto && !m->joined; ni++) {
        if (!m->nodes[ni].expanded) continue;
        if (!m->try_all &&
            !(m->nodes[ni].irred || m->nodes[ni].parent == ni)) continue;
        if (mnf_loop_guard(s, m)) break;
        mnf_expand_node_fact(s, m, ni, m->pend_l[f], m->pend_r[f],
                             m->pend_or[f],
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
                             m->pend_l_ft[f], m->pend_r_ft[f]
#else
                             NULL, NULL
#endif
        );
      }
    }
    m->n_pend = 0u;
    m->n_rules_seen = s->n_rules;
  }
  // (b) WM MNF_ExpandNM (MNF.c:858-906): strict red/green alternation,
  // one pop per colour per round, run until join, exhaustion, or the
  // mnfanalysis switch.
  while (!m->joined) {
    if (mnf_loop_guard(s, m)) break;
    int did = 0;
    if (m->qred_head < m->qred_tail) {
      u32 ni = mnf_pop(m->qred, &m->qred_head, &m->qred_tail,
                       m->red_last_irred);
      if (!m->nodes[ni].expanded) {
        mnf_expand_node(s, m, ni, 0u, s->n_rules, /*in_sweep=*/0u);
        m->nodes[ni].expanded = 1u;
      }
      m->red_last_irred = m->nodes[ni].irred;
      did = 1;
    }
    if (!m->joined && m->qgreen_head < m->qgreen_tail) {
      u32 ni = mnf_pop(m->qgreen, &m->qgreen_head, &m->qgreen_tail,
                       m->green_last_irred);
      if (!m->nodes[ni].expanded) {
        mnf_expand_node(s, m, ni, 0u, s->n_rules, /*in_sweep=*/0u);
        m->nodes[ni].expanded = 1u;
      }
      m->green_last_irred = m->nodes[ni].irred;
      did = 1;
    }
    // WM iteration-boundary order (MNF_ExpandNM :894-901): the
    // mnfanalysis check runs BEFORE the empty-queue break -- a front
    // that crosses 2000 on its final pops still switches THIS check.
    if (m->n_nodes > MNF_SWITCH_NF) { mnf_switch_rewrite_mode(m); break; }
    if (m->n_nodes > MNF_SWITCH_IRRED) m->try_all = 0u;
    if (!did) break;
  }
#ifdef ATP_MNF_DIAG
  if (m->joined) mnf_verify(s, m);
#endif
  return m->joined;
}

// === MNF proof extraction ============================================
//
// A join is goal_lhs ->* meet <-* goal_rhs: every MNF node is a one-step
// equational rewrite of its parent (forward l->r, or variable-safe
// backward r->l), so walking the two parent chains up from `meet` to the
// two seeds reconstructs a closed equational chain.  This is the
// goal-directed analog of atp_proof_record_side: it fills AtpProofStep[]
// with the same {side, rule, fwd, pos, before, after} shape the WL
// ProofObject builder already consumes, so the symmetric goal joins the
// same dataset machinery as a single-NF proof.
//
// GREEN side terms are emitted as side 0 (the goal_lhs chain), RED as
// side 1 (the goal_rhs chain); both chains run goal -> meet, so the
// assembled equation reaches `meet == meet` -- exactly the NF == NF
// tautology the single-NF path reaches, with `meet` as the common form.

// Find the one-step rewrite of `before` that yields `after` using the
// rule slice (rl[j] -> rr[j], j in [0,nr)): try every rule (forward
// l->r, and -- gated by vars-contained -- backward r->l) at every
// position, returning the redex path / rule / direction of the first
// match.  Mirrors mnf_successors' search but stops at the edge whose
// result is kbo_eq to `after`.  `out_rule` indexes into the slice.
// Returns 1 on success.
//
// r2 grounded-template faces: mnf_exp_visit applies an equation face
// whose replacement carries variables OUTSIDE the matched side by
// grounding the extras to the minimal signature constant
// (atp_unorient_template -- WM RechtsUnfreiErzeugen).  An edge taken
// that way replays against no plain face (the plain apply leaves the
// extras as variables, never kbo_eq to the grounded `after`), which
// silently emptied the whole extraction on every r2-carried join
// (GRP439-1's multiply(inverse(X),X) = multiply(inverse(Y),Y) meet).
// Try the templates after the plain faces so well-behaved edges keep
// their citation order.
static int mnf_edge_step(AtpState *s, Term before, Term after,
                         const Term *rl, const Term *rr, u32 nr,
                         u8 *pos, u8 depth,
                         u32 *out_rule, u8 *out_pos_len, u8 *out_fwd) {
  for (u32 j = 0; j < nr; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(rl[j], before, &sub)) {                     // forward l->r
      Term repl = thvm_subst_apply(rr[j], &sub);
      if (kbo_eq(repl, after)) {
        *out_rule = j; *out_pos_len = depth; *out_fwd = 1u; return 1;
      }
    }
    if (atp_vars_contained(rl[j], rr[j])) {                    // backward r->l
      RewriteSubst sb = {{0}};
      if (thvm_match(rr[j], before, &sb)) {
        Term repl = thvm_subst_apply(rl[j], &sb);
        if (kbo_eq(repl, after)) {
          *out_rule = j; *out_pos_len = depth; *out_fwd = 0u; return 1;
        }
      }
    }
  }
  for (u32 j = 0; j < nr; j++) {                // r2 grounded templates
    for (u8 face = 0u; face < 2u; face++) {
      Term pat = face ? rr[j] : rl[j];
      Term rep = face ? rl[j] : rr[j];
      if (atp_vars_contained(rep, pat)) continue;   // plain face covered
      RewriteSubst sub = {{0}};
      if (!thvm_match(pat, before, &sub)) continue;
      Term tmpl = atp_unorient_template(s, pat, rep);
      if (tmpl == 0) continue;
      Term repl = thvm_subst_apply(tmpl, &sub);
      if (kbo_eq(repl, after)) {
        *out_rule = j; *out_pos_len = depth;
        *out_fwd = face ? 0u : 1u;
        return 1;
      }
    }
  }
  // Descend: the rewrite was below the top.  `before` and `after` agree
  // everywhere except the one child holding the redex.
  if (term_tag(before) == TAG_CTR && term_tag(after) == TAG_CTR &&
      term_ext(before) == term_ext(after) &&
      term_ctr_n(before) == term_ctr_n(after) &&
      depth < ATP_PROOF_MAX_DEPTH) {
    u32 nc = term_ctr_n(before);
    for (u32 i = 0; i < nc; i++) {
      Term cb = term_ctr_at(before, i);
      Term ca = term_ctr_at(after, i);
      if (kbo_eq(cb, ca)) continue;     // unchanged child: not the redex
      pos[depth] = (u8)i;
      if (mnf_edge_step(s, cb, ca, rl, rr, nr, pos, (u8)(depth + 1u),
                        out_rule, out_pos_len, out_fwd)) {
        return 1;
      }
    }
  }
  return 0;
}

// Build the HISTORICAL rule set: every equation that ever entered R,
// not just the rules surviving in s->lhs/s->rhs after interreduction.
// An MNF path step taken with a rule interreduction later retired only
// replays against this superset.  Sources: the live rules plus every
// TRACE_ORIENT / TRACE_AXIOM / TRACE_SIMPLIFY trace entry's (lhs, rhs).
// Caller-owned out_l / out_r arrays of capacity `cap`; returns count.
// Build the historical rule set (live rules + every ORIENT/AXIOM/
// SIMPLIFY equation that ever entered R via the trace).  out_trace[i]
// records the TRACE index that resolves slot i, so the WL extractor can
// resolve an MNF step's cited rule through resolveTrace exactly as it
// resolves a completion MainStep: a live derived rule lands on its
// critical-pair lemma, a retired rule on its own trace node.
static u32 mnf_historical_rules(AtpState *s, Term *out_l, Term *out_r,
                                u32 *out_trace, u32 cap) {
  u32 n = 0u;
  for (u32 i = 0; i < s->n_rules && n < cap; i++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    out_l[n] = atp_rule_term(s, i, 0u);
    out_r[n] = atp_rule_term(s, i, 1u);
#else
    out_l[n] = s->lhs[i]; out_r[n] = s->rhs[i];
#endif
    out_trace[n] = s->r_trace[i];
    n++;
  }
  for (u32 i = 0; i < s->n_trace && n < cap; i++) {
    Term e = s->trace[i];
    u32  reason = term_ext(e);
    if (reason == TRACE_ORIENT || reason == TRACE_AXIOM ||
        reason == TRACE_SIMPLIFY || reason == TRACE_FVI) {
      if (!atp_trace_eq_load(s, i, &out_l[n], &out_r[n])) {
        out_l[n] = term_ctr_at(e, 2);
        out_r[n] = term_ctr_at(e, 3);
      }
      out_trace[n] = i;
      n++;
    }
  }
  return n;
}

// Walk node `ni` up to its seed, collecting the chain of terms
// root..ni into buf[0..*len) (root first).  Returns the root index.
static u32 mnf_collect_lineage(AtpMnf *m, u32 ni, u32 *buf, u32 *len,
                               u32 cap) {
  u32 stack[ATP_PROOF_MAX_STEPS + 2u];
  u32 sp = 0u;
  u32 guard = 0u;
  while (sp < cap && sp < (ATP_PROOF_MAX_STEPS + 2u) &&
         guard++ < m->n_nodes + 1u) {
    stack[sp++] = ni;
    if (m->nodes[ni].parent == ni) break;
    ni = m->nodes[ni].parent;
  }
  // stack holds ni..root (leaf first); reverse into buf (root first).
  for (u32 i = 0; i < sp; i++) buf[i] = stack[sp - 1u - i];
  *len = sp;
  return ni;
}

// MODE-1 (ExpandRewrite) joins record ONE normalize edge: parent ->
// full mixmost NF, a MULTI-step rewrite no single mnf_edge_step can
// replay -- the admitted extraction gap that emptied every mode-1
// proof.  Decompose it by re-running the SAME mixmost normalizer with
// AtpProofStep recording: R is unchanged since the join latched (the
// engine stops at PROVED), so the deterministic replay reaches the
// same NF, and each recorded single step resolves through the CLI/WL
// citation machinery like any goal-chain step.  The step cap matches
// the mode-1 normalize call (4096) so a cap-truncated NF reproduces.
// Returns 1 when the replay lands exactly on `after`.
static int mnf_edge_norm_decompose(AtpState *s, Term before, Term after,
                                   u32 side, AtpProofStep *out, u32 cap,
                                   u32 *n) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  u32 n0 = *n;
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *nf = atp_rewrite_normalize_ft_proofsteps(
      s, ft_from_term(a, before, 0), 4096u, (u8)side, out, n, cap);
  if (nf == NULL || *n >= cap || !kbo_eq(ft_to_term(nf), after)) {
    *n = n0;
    return 0;
  }
  // Map each step's rule SLOT to its TRACE index (the citation form
  // every other MNF step carries); a slot beyond the live set resolves
  // by replay on the consumer side.
  for (u32 k = n0; k < *n; k++) {
    if (out[k].rule < s->n_rules && s->r_trace != NULL)
      out[k].rule = s->r_trace[out[k].rule];
  }
  return 1;
#else
  (void)s; (void)before; (void)after; (void)side;
  (void)out; (void)cap; (void)n;
  return 0;
#endif
}

// Emit the steps along a lineage (terms[0..n_terms), seed/root first)
// into out[], tagged with `side`.  Each consecutive pair is one
// rewrite, reconstructed by mnf_edge_step against the (rl, rr, nr)
// rule slice.  Both fronts are emitted seed -> meet, so the GREEN and
// RED chains meet at the join term.  Returns 1 if every edge replayed.
static int mnf_emit_lineage(AtpState *s, AtpMnf *m, const u32 *terms,
                            u32 n_terms, u32 side,
                            const Term *rl, const Term *rr,
                            const u32 *rtr, u32 nr,
                            AtpProofStep *out, u32 cap, u32 *n) {
  for (u32 k = 0; k + 1u < n_terms; k++) {
    Term before = m->nodes[terms[k]].term;
    Term after  = m->nodes[terms[k + 1u]].term;
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0u; u8 pos_len = 0u, fwd = 1u;
    if (!mnf_edge_step(s, before, after, rl, rr, nr,
                       pos, 0u, &rule, &pos_len, &fwd)) {
      // Not a single-step edge: try the mode-1 normalize decomposition.
      if (mnf_edge_norm_decompose(s, before, after, side, out, cap, n))
        continue;
      return 0;   // an edge replays against no historical rule
    }
    if (*n >= cap) return 0;
    AtpProofStep *st = &out[*n];
    st->side    = side;
    // rtr[rule] is the TRACE index of the cited historical rule, so WL
    // resolves it via resolveTrace just like a completion MainStep.
    st->rule    = rtr[rule];
    st->fwd     = fwd;
    st->pos_len = pos_len;
    for (u8 p = 0; p < pos_len; p++) st->pos[p] = pos[p];
    st->before  = before;
    st->after   = after;
    (*n)++;
  }
  return 1;
}

// Reconstruct the MNF join as a two-sided AtpProofStep chain.  Side A is
// the existing table node meet_a; side B is the fresh reduct (meet_term)
// of meet_b_parent that collided with it.  Both A and B equal meet_term,
// so each chain runs seed -> meet.  GREEN -> side 0, RED -> side 1.
// Returns the step count, 0 if no join / not reconstructable.
fn u32 thvm_atp_mnf_proof_extract(AtpState *s, AtpProofStep *out, u32 cap) {
  if (s == NULL || out == NULL || cap == 0u) return 0;
  AtpMnf *m = s->mnf;
  if (m == NULL || !m->joined) return 0;

  // Lineage A: meet_a up to its seed.
  static u32 lin_a[ATP_PROOF_MAX_STEPS + 2u];
  static u32 lin_b[ATP_PROOF_MAX_STEPS + 2u];
  u32 len_a = 0u, len_b = 0u;
  mnf_collect_lineage(m, m->meet_a, lin_a, &len_a, ATP_PROOF_MAX_STEPS + 2u);

  // Lineage B: the colliding reduct was never created as a node.  Walk
  // its PARENT to the seed, then append a synthetic final node holding
  // meet_term.  We reuse meet_b_parent's node slot terms; the final
  // edge (meet_b_parent.term -> meet_term) is emitted explicitly.
  mnf_collect_lineage(m, m->meet_b_parent, lin_b, &len_b,
                      ATP_PROOF_MAX_STEPS + 1u);

  u8 col_a = m->nodes[m->meet_a].colour;     // colour that owns lineage A
  // Colour -> goal-side mapping through the SEEDS: mnf_create
  // LRSortieren-canonicalizes the goal pair (WM parity), so the GREEN
  // front may be seeded from goal_RHS.  Mapping GREEN to side 0
  // unconditionally emitted the two chains against the swapped goal
  // orientation -- every record claimed the wrong side rewrote
  // (GRP697-1's g0_0 abstain).
  u32 green_side = 0u;
  if (!kbo_eq(m->seed_green, s->goal_lhs) &&
      kbo_eq(m->seed_green, s->goal_rhs))
    green_side = 1u;
  // meet_b_col is the opposite colour (the fresh reduct's colour).
  u32 side_a = (col_a == MNF_GREEN) ? green_side : 1u - green_side;
  u32 side_b = (m->meet_b_col == MNF_GREEN) ? green_side : 1u - green_side;

  // Reconstruct each edge against the HISTORICAL rule set (live rules +
  // every equation that ever entered R via the trace DAG), so an edge
  // taken with a since-retired rule still replays.
  u32 hist_cap = s->n_rules + s->n_trace;
  Term *hist_l = (Term *)malloc((size_t)hist_cap * sizeof(Term));
  Term *hist_r = (Term *)malloc((size_t)hist_cap * sizeof(Term));
  u32  *hist_t = (u32  *)malloc((size_t)hist_cap * sizeof(u32));
  if (hist_l == NULL || hist_r == NULL || hist_t == NULL) {
    free(hist_l); free(hist_r); free(hist_t); return 0; }
  u32 nr = mnf_historical_rules(s, hist_l, hist_r, hist_t, hist_cap);

  u32 n = 0u;
#ifdef ATP_MNF_DIAG
  fprintf(stderr, "[mnf-extract] meet_a=%u len_a=%u meet_b_parent=%u "
          "len_b=%u col_a=%u meet_b_col=%u hist_rules=%u\n",
          m->meet_a, len_a, m->meet_b_parent, len_b, col_a, m->meet_b_col,
          nr);
#endif
  // An edge that replays against no historical rule leaves the lineage
  // not reconstructable as a forward/backward chain.  Return 0 (no
  // extractable proof) rather than emit a partial, unsound chain.
  if (!mnf_emit_lineage(s, m, lin_a, len_a, side_a, hist_l, hist_r, hist_t,
                        nr, out, cap, &n)) {
#ifdef ATP_MNF_DIAG
    fprintf(stderr, "[mnf-extract] lineage A unreplayable at step %u/%u\n",
            n, len_a > 0u ? len_a - 1u : 0u);
#endif
    free(hist_l); free(hist_r); free(hist_t); return 0;
  }
  if (!mnf_emit_lineage(s, m, lin_b, len_b, side_b, hist_l, hist_r, hist_t,
                        nr, out, cap, &n)) {
#ifdef ATP_MNF_DIAG
    fprintf(stderr, "[mnf-extract] lineage B unreplayable at step %u\n", n);
#endif
    free(hist_l); free(hist_r); free(hist_t); return 0;
  }
  // Final B edge: meet_b_parent.term -> meet_term (the collision).
  {
    Term before = m->nodes[m->meet_b_parent].term;
    Term after  = m->meet_term;
    if (!kbo_eq(before, after)) {
      u8  pos[ATP_PROOF_MAX_DEPTH];
      u32 rule = 0u; u8 pos_len = 0u, fwd = 1u;
      if (!mnf_edge_step(s, before, after, hist_l, hist_r, nr,
                         pos, 0u, &rule, &pos_len, &fwd)) {
        if (mnf_edge_norm_decompose(s, before, after, side_b,
                                    out, cap, &n)) {
          free(hist_l); free(hist_r); free(hist_t);
          return n;
        }
        free(hist_l); free(hist_r); free(hist_t); return 0;
      }
      if (n >= cap) return 0;
      AtpProofStep *st = &out[n];
      st->side    = side_b;
      st->rule    = hist_t[rule];   // TRACE index, see mnf_emit_lineage
      st->fwd     = fwd;
      st->pos_len = pos_len;
      for (u8 p = 0; p < pos_len; p++) st->pos[p] = pos[p];
      st->before  = before;
      st->after   = after;
      n++;
    }
  }
  free(hist_l); free(hist_r); free(hist_t);
  return n;
}
#endif /* ATP_MNF */

// Goal-side normalization cap.  Generous (not the per-step CP cap
// of 64): a goal can close purely by normalization when the axioms
// generate no critical pairs -- e.g. combinatory-logic identities
// (B/C/W <-> S/K), whose rules are non-overlapping, so completion's
// CP queue is empty and the ONLY way the goal joins is by reducing
// both sides to a common normal form.  Deep combinator terms like
// S(S(K(S(KS)K))S)(KK) x y z take many hundreds of rewrites to
// normalize; a cap of 64 left them un-joined (reported QUEUE_EMPTY
// with the goal actually provable).  A terminating rewrite system
// reaches fixpoint and returns early regardless of the bound, so
// the only risk is a non-terminating R (e.g. the Y axiom in scope),
// which the wall deadline already bounds.
#define ATP_GOAL_NORM_CAP (1u << 16)

// Universal-goal join check for ONE conjecture pair (gl, gr):
// normalize both sides under the current R and compare (flatterm
// fixpoint equality on the non-AC path, then kbo_eq, then AC-equality
// when an AC mask is live).  Writes the normal forms to *nf_l/*nf_r in
// every case; returns 1 iff the pair joins.  Shared by the single-goal
// path and the multi-goal conjunct loop -- the MNF front search is NOT
// part of this check (it is per-alias-goal and stateful).
extern int g_goalnf_active;   // ft_norm.c: [mixstep] tracer scope tag
extern int atp_ft_wm_pattern_before(const AtpFtCell *a,
                                    const AtpFtCell *b);   // ft_norm.c

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Do any live consumers read the DECODED goal NF Terms?  The span
// verdict decides the join; the Terms feed only the goal-similarity
// heuristics (goal interleave / CPinGoal / learned features), the
// feature recorder, and the AC-equality fallback.  With all of them
// off, the goal check never decodes.
static u8 atp_goal_nf_consumers_on(const AtpState *s) {
  if (s->use_goal_interleave > 0u) return 1u;
  if (s->cp_weight_mode == ATP_CP_WEIGHT_GOAL) return 1u;
  if (s->cp_weight_mode == ATP_CP_WEIGHT_LEARNED) return 1u;
  if (s->record_cp_features) return 1u;
  return 0u;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

static u8 atp_goal_pair_joined(AtpState *s, Term gl, Term gr,
                               Term *nf_l, Term *nf_r) {
  // FT path for AC-mask=0; Term path otherwise.  Earlier commit
  // 2133702a tried to unify on FT via ft_ac_eq, but ac-bool-idem-embed
  // still regresses to QUEUE_EMPTY iters=2 because the FT NF reaches
  // a different fixpoint than the Term NF on the f(x,x)=x reduction
  // trajectory.  Keep the Term-side path for AC workloads until the
  // FT normalize semantics match the Term-side leftmost-outermost
  // descent under AC.
  Term l, r;
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) {
    l = atp_rewrite_normalize(s, gl, s->lhs, s->rhs,
                              s->n_rules, ATP_GOAL_NORM_CAP);
    r = atp_rewrite_normalize(s, gr, s->lhs, s->rhs,
                              s->n_rules, ATP_GOAL_NORM_CAP);
  } else
#endif
  {
    atp_ft_mirror_ensure(s);
    AtpFt *gft = (AtpFt *)s->ft_arena_ptr;
    // Locate the pair's mirror + NF-cache slots: the aliased goal or a
    // conjunct (both keep live FT mirrors; the normalize below is
    // destructive, so it gets a CLONE -- bit-identical to the
    // ft_from_term re-encode it replaces).  An unmirrored pair
    // re-encodes.
    const AtpFtCell *mir_l = NULL, *mir_r = NULL;
    struct AtpFtCell **cache_l = NULL, **cache_r = NULL;
    Term prev_nf_l = 0, prev_nf_r = 0;
    if (gl == s->goal_lhs && gr == s->goal_rhs &&
        s->goal_lhs_ft != NULL && s->goal_rhs_ft != NULL) {
      mir_l = s->goal_lhs_ft;
      mir_r = s->goal_rhs_ft;
      cache_l = (struct AtpFtCell **)&s->goal_nf_ft_l;
      cache_r = (struct AtpFtCell **)&s->goal_nf_ft_r;
      prev_nf_l = s->goal_lhs_nf;
      prev_nf_r = s->goal_rhs_nf;
    } else if (s->goals_lhs_ft != NULL) {
      for (u32 gi = 0; gi < s->n_goals; gi++) {
        if (gl == s->goals_lhs[gi] && gr == s->goals_rhs[gi] &&
            s->goals_lhs_ft[gi] != NULL && s->goals_rhs_ft[gi] != NULL) {
          mir_l = s->goals_lhs_ft[gi];
          mir_r = s->goals_rhs_ft[gi];
          cache_l = (struct AtpFtCell **)&s->goals_nf_ft_l[gi];
          cache_r = (struct AtpFtCell **)&s->goals_nf_ft_r[gi];
          prev_nf_l = s->goals_lhs_nf[gi];
          prev_nf_r = s->goals_rhs_nf[gi];
          break;
        }
      }
    }
    // Verify the located mirror against the live pair (a rewrite of
    // the goal Terms after set_goals would leave it stale) and
    // SELF-REPAIR on any miss: rebuild the spans once and store them
    // back into the located slots, so each unique pair encodes at
    // most once.
    if (mir_l != NULL &&
        (!ft_eq_term(mir_l, gl) || !ft_eq_term(mir_r, gr))) {
      mir_l = NULL;
      mir_r = NULL;
    }

    u8 gl_mir = (u8)(mir_l != NULL && mir_r != NULL);
    AtpFtCell *fl_in;
    AtpFtCell *fr_in;
    if (gl_mir) {
      fl_in = ft_clone_span(gft, mir_l, 0);
      fr_in = ft_clone_span(gft, mir_r, 0);
    } else {
      AtpFtCell *nl = ft_from_term(gft, gl, 0);
      AtpFtCell *nr = ft_from_term(gft, gr, 0);
      // Store the repaired mirrors: the aliased slots when the pair is
      // the alias, else the matching conjunct's slots.
      if (gl == s->goal_lhs && gr == s->goal_rhs) {
        s->goal_lhs_ft = nl;
        s->goal_rhs_ft = nr;
        s->goal_nf_ft_l = NULL;
        s->goal_nf_ft_r = NULL;
        cache_l = (struct AtpFtCell **)&s->goal_nf_ft_l;
        cache_r = (struct AtpFtCell **)&s->goal_nf_ft_r;
        prev_nf_l = s->goal_lhs_nf;
        prev_nf_r = s->goal_rhs_nf;
        gl_mir = 1u;
        // The alias POINTS AT a conjunct's mirror: repair that source
        // slot too, or every alias move re-points at the stale spans.
        if (s->goals_lhs_ft != NULL) {
          for (u32 gi = 0; gi < s->n_goals; gi++) {
            if (gl == s->goals_lhs[gi] && gr == s->goals_rhs[gi]) {
              s->goals_lhs_ft[gi] = nl;
              s->goals_rhs_ft[gi] = nr;
              s->goals_nf_ft_l[gi] = NULL;
              s->goals_nf_ft_r[gi] = NULL;
              break;
            }
          }
        }
      } else if (s->goals_lhs_ft != NULL) {
        for (u32 gi = 0; gi < s->n_goals; gi++) {
          if (gl == s->goals_lhs[gi] && gr == s->goals_rhs[gi]) {
            s->goals_lhs_ft[gi] = nl;
            s->goals_rhs_ft[gi] = nr;
            s->goals_nf_ft_l[gi] = NULL;
            s->goals_nf_ft_r[gi] = NULL;
            cache_l = (struct AtpFtCell **)&s->goals_nf_ft_l[gi];
            cache_r = (struct AtpFtCell **)&s->goals_nf_ft_r[gi];
            prev_nf_l = s->goals_lhs_nf[gi];
            prev_nf_r = s->goals_rhs_nf[gi];
            gl_mir = 1u;
            break;
          }
        }
      }
      fl_in = gl_mir ? ft_clone_span(gft, nl, 0) : nl;
      fr_in = gl_mir ? ft_clone_span(gft, nr, 0) : nr;
    }
    // THVM_ATP_GOAL_NF_DEBUG scoping (see ft_norm.c g_goalnf_active):
    // tag the two goal walks 1/2 so the [mixstep] tracer separates the
    // L and R reduction sequences.
    g_goalnf_active = 1;
    AtpFtCell *fl = atp_rewrite_normalize_ft(s, fl_in, ATP_GOAL_NORM_CAP);
    g_goalnf_active = 2;
    AtpFtCell *fr = atp_rewrite_normalize_ft(s, fr_in, ATP_GOAL_NORM_CAP);
    g_goalnf_active = 0;
    // NF decode cache (goal_nf_ft_l/r, see thvm.h): the goal's NF is
    // content-stable across most checks, so when the fresh span
    // ft_eq-matches the previous one, reuse the previously decoded
    // Term (the caller stored it in goal_lhs_nf/goal_rhs_nf right
    // after the last call -- the pairing invariant).  Scoped to the
    // aliased goal (gl_mir); conjunct-loop calls neither read nor
    // refresh it.
    u8 joined_ft = (u8)ft_eq(fl, fr);
    static int goalnf_dbg = -1;
    if (goalnf_dbg < 0)
      goalnf_dbg = getenv("THVM_ATP_GOAL_NF_DEBUG") != NULL ? 1 : 0;
    if (joined_ft || goalnf_dbg || atp_goal_nf_consumers_on(s)) {
      if (gl_mir && *cache_l != NULL && prev_nf_l != 0 &&
          ft_eq(fl, *cache_l)) {
        l = prev_nf_l;
      } else {
        l = ft_to_term(fl);
        if (gl_mir) *cache_l = fl;
      }
      if (gl_mir && *cache_r != NULL && prev_nf_r != 0 &&
          ft_eq(fr, *cache_r)) {
        r = prev_nf_r;
      } else {
        r = ft_to_term(fr);
        if (gl_mir) *cache_r = fr;
      }
    } else {
      // Consumer-free run: the span verdict is the whole story; the
      // Terms stay unmaterialized (callers' ?:-fallbacks read the raw
      // goal, which none of them do with the consumers off).
      l = 0;
      r = 0;
    }
    if (goalnf_dbg) {
      char lb[512], rb[512];
      atp_pretty_term(l, lb, sizeof lb);
      atp_pretty_term(r, rb, sizeof rb);
      fprintf(stderr, "[goalnf] n_rules=%u n_unorient=%u mixmost=%u  L=%s  R=%s  eq=%d\n",
              s->n_rules, s->n_unorient, s->use_mixmost_nf, lb, rb, ft_eq(fl, fr));
    }
    if (joined_ft) {
      *nf_l = l;
      *nf_r = r;
      return 1;
    }
    // FT arm ending: the span verdicts are complete (kbo_eq of the
    // decoded pair IS ft_eq of the spans); the AC fallback runs off
    // the spans too, materializing the witness pair only on an
    // AC-join (once per proof).
#ifdef THVM_ATP_AC
    if (thvm_atp_get_ac_mask() != 0ull) {
      AtpAcInfo ac = { .ac_mask = thvm_atp_get_ac_mask() };
      if (ft_ac_eq(fl, fr, &ac)) {
        if (l == 0) {
          l = ft_to_term(fl);
          r = ft_to_term(fr);
        }
        *nf_l = l;
        *nf_r = r;
        return 1;
      }
    }
#endif
    *nf_l = l;
    *nf_r = r;
    return 0;
  }
  *nf_l = l;
  *nf_r = r;
  if (kbo_eq(l, r)) return 1;
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) {
    AtpAcInfo ac = { .ac_mask = thvm_atp_get_ac_mask() };
    if (atp_ac_eq(l, r, &ac)) return 1;
  }
#endif
  return 0;
}

// Point the single-goal alias (goal_lhs/goal_rhs, their NFs, the FT
// mirror) at conjunct g.  When the alias actually MOVES to a different
// conjunct, the MNF front set is dropped: its GREEN/RED fronts were
// seeded from the previous alias goal and would keep searching for a
// join of the wrong conjunct.  goal_check recreates it lazily.
static void atp_goals_alias(AtpState *s, u32 g) {
  u8 moved = (s->goal_lhs != s->goals_lhs[g] ||
              s->goal_rhs != s->goals_rhs[g]);
  s->goal_lhs    = s->goals_lhs[g];
  s->goal_rhs    = s->goals_rhs[g];
  s->goal_lhs_nf = s->goals_lhs_nf[g];
  s->goal_rhs_nf = s->goals_rhs_nf[g];
  if (!moved) return;
#ifdef THVM_ATPFT_RULES
  // Alias the conjunct's mirror (built at set_goals) -- no re-encode.
  if (s->goals_lhs_ft != NULL && s->goals_lhs_ft[g] != NULL) {
    s->goal_lhs_ft = (struct AtpFtCell *)s->goals_lhs_ft[g];
    s->goal_rhs_ft = (struct AtpFtCell *)s->goals_rhs_ft[g];
  } else {
    s->goal_lhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, s->goal_lhs, 0);
    s->goal_rhs_ft = ft_from_term((AtpFt *)s->ft_arena_ptr, s->goal_rhs, 0);
  }
  s->goal_nf_ft_l = NULL;
  s->goal_nf_ft_r = NULL;
#endif
#ifdef ATP_MNF
  if (s->mnf != NULL) {
    mnf_destroy(s->mnf);
    s->mnf = NULL;
  }
#endif
}
