// GOAL-NARROW (THVM_ATP_GOAL_NARROW, default OFF): the ground
// goal-narrowing endgame -- a budgeted congruence closure over the
// GROUND slice of the derived system, extended by bounded two-way
// e-matching of the full rule set over the goal cone.
//
// WHY.  The CASC GRP dissection isolated a capability both oracle
// provers use to close the mult(a,b)=mult(b,a)-style commutator
// goals (GRP725-1 twee, GRP678-1/GRP690-1 vampire) that plain
// completion + reductive goal normalization miss: after goal
// flattening names the goal's ground subterms (sFlat/sF constants),
// the proof lives in a lane of GROUND equations over those names,
// and the closing chain rewrites goal terms EXPANSIVELY (R->L steps
// on ground terms).  Ordered rewriting cannot take those steps; the
// completion loop only reaches them after deriving thousands of
// bridging ground rules (twee's GRP725-1 proof forms `mult2 -> mult3`
// as active rule 8478 of 8478).  A congruence closure over the ground
// equations derived SO FAR decides that entailment immediately --
// every expanding ground chain is an equality path in the E-graph --
// so the goal closes as soon as the ground slice of R logically
// implies it, not when ordered completion gets around to orienting
// the bridge.
//
// WHAT.  Two cooperating pieces, both sound (every equation ingested
// or instantiated below is a consequence of the input E; congruence
// closure decides ground equational entailment exactly):
//
//  (1) GROUND CC: every rule/equation of R whose BOTH sides are
//      ground is unioned in a classic Nelson-Oppen congruence closure
//      (hash-consed subterm DAG, union-find, use-list congruence
//      propagation).
//  (2) GOAL-CONE E-MATCHING: on a revision stride, every rule face
//      (l->r and r->l, vars(replacement) subset of vars(pattern)) is
//      root-matched against the small members of the goal cone --
//      the classes reachable from the goal sides' subterm closure
//      plus everything previously merged into or instantiated from
//      them.  A hit adds `subject = instance` to the CC.  This is
//      bounded two-way ground superposition ON THE GOAL: a face
//      whose replacement outweighs the subject IS the expanding
//      (R->L) step, taken without any ordering gate, and congruence
//      propagation applies it under every context for free (the
//      giant intermediate terms of the oracle chains are never
//      materialized -- classes stand in for them).
//
// The goal is proved when the two goal sides' classes coincide.
//
// BUDGETED.  Node growth is capped (gn max-nodes); subjects are
// size- and count-capped; e-match sweeps run every gn-stride
// revisions with gn-iters passes; the sweep polls the wall deadline
// and the heap-pressure guard exactly like the MNF front
// (mnf_loop_guard) and latches give_up cleanly when a cap or the
// heap is hit -- completion continues untouched.
//
// PROOF SIDE.  A goal-narrow join sets the same PROVED surface as an
// MNF front collision.  The closing chain is an equality path in the
// CC, which the single-NF chain extractor cannot replay; the CLI
// emits its honest "no single-NF chain extractable" marker for such
// goals (same certification level as the pre-existing flatten-lane
// wins).  Proof-producing union-find is the scoped follow-up.
//
// Default OFF: without THVM_ATP_GOAL_NARROW the engine never
// allocates the structure and every hook below is one branch test --
// byte-identical trajectories.

#define GN_NONE 0xFFFFFFFFu

// Budget defaults (each env-tunable; resolved once at creation).
#define GN_DEF_MAX_NODES   262144u  // hash-consed DAG nodes (hard cap)
#define GN_DEF_MAX_INST    32768u   // e-match-created nodes (soft cap:
                                    // beyond it sweeps stop growing the
                                    // graph and only DISCOVER equalities
                                    // between existing nodes)
#define GN_DEF_SUBJECTS    2048u    // e-match subjects per pass
#define GN_DEF_SUBJ_SIZE   16u      // max subject symbol count
#define GN_DEF_INST_SIZE   20u      // absolute created-instance ceiling
#define GN_DEF_UP_SLACK    4u       // expansion slack: an instance may
                                    // outgrow its subject by this many
                                    // symbols (WM mnfup-style bound)
#define GN_DEF_ITERS       4u       // e-match passes per sweep
#define GN_DEF_STRIDE      32u      // rule revisions between sweeps
#define GN_DEF_INST_SWEEP  1024u    // instance-node quota per sweep, so
                                    // the max_inst budget spreads across
                                    // the run instead of burning out on
                                    // the (uninformed) first seconds
#define GN_MAX_KIDS        64u      // arity cap (matches engine reality)

typedef struct AtpGoalNarrow {
  // Hash-consed ground-term DAG (SoA, append-only node ids).
  u32  *label;      // CTR ext label
  u32  *kid_off;    // offset into kids[]
  u16  *n_kids;
  u32  *uf;         // union-find parent
  u8   *rank;
  u8   *cone;       // goal-cone flag (or-merged onto class roots)
  u16  *size;       // symbol count of this node's term
  Term *term;       // representative Term (GC-rooted via gn_gc_*)
  u32  *use_head;   // per class root: head+1 into use_next/use_parent
  u32  *swept_hi;   // e-match watermark: rules [0, swept_hi) tried
  u32   n_nodes, cap_nodes;
  u32  *kids;       // kid node-id pool
  u32   kids_n, kids_cap;
  // Congruence signature table: open addressing, entries node_id+1
  // (0 = empty).  Probes validate against the CURRENT canonical
  // signature, so stale entries are skipped, never trusted.
  u32  *sig_tab;
  u32   sig_cap;    // power of two
  u32   sig_used;
  // Use-list edge pool.
  u32  *use_next;
  u32  *use_parent;
  u32   use_n, use_cap;
  // Merge worklist (packed pairs).
  u32  *wl;
  u32   wl_n, wl_cap;
  // Ingestion cursor + sweep cadence.
  u32   rules_seen;
  u64   last_sweep_rev;
  u8    swept_once;
  u8    give_up;    // budget/pressure latch: stop growing
  u8    joined;
  // Budgets.
  u32   max_nodes, max_inst, max_subjects, subj_size, inst_size,
        up_slack, iters, stride, inst_sweep;
  u8    cone_only;  // 1: subjects gated on goal-reachability; 0: any
                    // small node (vampire-style: superpose among ALL
                    // derived ground-lane facts)
  u64   t0_us;      // first-check wall clock, for the endgame burst
  u64   w0_us;      // first-check work clock (atp_work_peek_us)
  u32   n_inst;     // e-match-created nodes so far
  // Stats.
  u64   st_eqs, st_merges, st_hits, st_sweeps;
} AtpGoalNarrow;

static u32 gn_env_u32(const char *name, u32 dflt) {
  const char *e = getenv(name);
  if (e == NULL || e[0] == '\0') return dflt;
  unsigned long v = strtoul(e, NULL, 10);
  return (v == 0ul) ? dflt : (u32)v;
}

static AtpGoalNarrow *gn_create(void) {
  AtpGoalNarrow *g = (AtpGoalNarrow *)calloc(1u, sizeof(AtpGoalNarrow));
  if (g == NULL) return NULL;
  g->max_nodes    = gn_env_u32("THVM_ATP_GN_MAX_NODES", GN_DEF_MAX_NODES);
  g->max_inst     = gn_env_u32("THVM_ATP_GN_MAX_INST",  GN_DEF_MAX_INST);
  g->max_subjects = gn_env_u32("THVM_ATP_GN_SUBJECTS",  GN_DEF_SUBJECTS);
  g->subj_size    = gn_env_u32("THVM_ATP_GN_SUBJ_SIZE", GN_DEF_SUBJ_SIZE);
  g->inst_size    = gn_env_u32("THVM_ATP_GN_INST_SIZE", GN_DEF_INST_SIZE);
  g->up_slack     = gn_env_u32("THVM_ATP_GN_UP_SLACK",  GN_DEF_UP_SLACK);
  g->iters        = gn_env_u32("THVM_ATP_GN_ITERS",     GN_DEF_ITERS);
  g->stride       = gn_env_u32("THVM_ATP_GN_STRIDE",    GN_DEF_STRIDE);
  g->inst_sweep   = gn_env_u32("THVM_ATP_GN_INST_SWEEP", GN_DEF_INST_SWEEP);
  {
    const char *e = getenv("THVM_ATP_GN_CONE");
    g->cone_only = (e != NULL && e[0] == '0') ? 0u : 1u;
  }
  g->sig_cap = 1u << 16;
  g->sig_tab = (u32 *)calloc(g->sig_cap, sizeof(u32));
  if (g->sig_tab == NULL) { free(g); return NULL; }
  return g;
}

static void gn_free(struct AtpGoalNarrow *g) {
  if (g == NULL) return;
  free(g->label); free(g->kid_off); free(g->n_kids);
  free(g->uf); free(g->rank); free(g->cone); free(g->size);
  free(g->term); free(g->use_head); free(g->swept_hi);
  free(g->kids); free(g->sig_tab);
  free(g->use_next); free(g->use_parent);
  free(g->wl);
  free(g);
}

// Groundness (goal-narrow's own copy: mnf_term_is_ground lives behind
// -DATP_MNF, which the C test builds do not define).
static u8 gn_term_is_ground(Term t) {
  if (term_tag(t) == TAG_FVR) return 0u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!gn_term_is_ground(term_ctr_at(t, i))) return 0u;
    }
  }
  return 1u;
}

static u32 gn_find(AtpGoalNarrow *g, u32 x) {
  while (g->uf[x] != x) {
    g->uf[x] = g->uf[g->uf[x]];   // path halving
    x = g->uf[x];
  }
  return x;
}

static u64 gn_sig_hash(AtpGoalNarrow *g, u32 label, u16 nk,
                       const u32 *kids) {
  u64 h = 1469598103934665603ull ^ ((u64)label << 1) ^ ((u64)nk << 40);
  for (u16 i = 0; i < nk; i++) {
    h ^= (u64)gn_find(g, kids[i]) + 0x9e3779b97f4a7c15ull
       + (h << 6) + (h >> 2);
  }
  return h;
}

// Identical CURRENT canonical signatures?
static u8 gn_sig_eq(AtpGoalNarrow *g, u32 a, u32 b) {
  if (g->label[a] != g->label[b]) return 0u;
  if (g->n_kids[a] != g->n_kids[b]) return 0u;
  const u32 *ka = g->kids + g->kid_off[a];
  const u32 *kb = g->kids + g->kid_off[b];
  for (u16 i = 0; i < g->n_kids[a]; i++) {
    if (gn_find(g, ka[i]) != gn_find(g, kb[i])) return 0u;
  }
  return 1u;
}

// Lookup-only probe: a COMMITTED node congruent to `id` (validated by
// current canonical signature), or GN_NONE.  Never mutates the table.
static u32 gn_sig_lookup(AtpGoalNarrow *g, u32 id) {
  u64 h = gn_sig_hash(g, g->label[id], g->n_kids[id],
                      g->kids + g->kid_off[id]);
  u32 mask = g->sig_cap - 1u;
  u32 i = (u32)h & mask;
  for (u32 probes = 0; probes < g->sig_cap; probes++, i = (i + 1u) & mask) {
    u32 e = g->sig_tab[i];
    if (e == 0u) return GN_NONE;
    u32 cand = e - 1u;
    if (cand != id && cand < g->n_nodes && gn_sig_eq(g, id, cand)) {
      return cand;
    }
  }
  return GN_NONE;
}

static void gn_wl_push(AtpGoalNarrow *g, u32 a, u32 b) {
  if (g->wl_n + 2u > g->wl_cap) {
    u32 nc = (g->wl_cap == 0u) ? 256u : g->wl_cap * 2u;
    u32 *nw = (u32 *)realloc(g->wl, (size_t)nc * sizeof(u32));
    if (nw == NULL) { g->give_up = 1u; return; }
    g->wl = nw; g->wl_cap = nc;
  }
  g->wl[g->wl_n++] = a;
  g->wl[g->wl_n++] = b;
}

static void gn_sig_grow(AtpGoalNarrow *g);

// Insert committed node `id` under its current canonical signature.
// (Duplicates in the table are tolerated: probes validate.)
static void gn_sig_insert(AtpGoalNarrow *g, u32 id) {
  if (g->sig_used * 10u >= g->sig_cap * 7u) {
    gn_sig_grow(g);
    if (g->give_up) return;
  }
  u64 h = gn_sig_hash(g, g->label[id], g->n_kids[id],
                      g->kids + g->kid_off[id]);
  u32 mask = g->sig_cap - 1u;
  u32 i = (u32)h & mask;
  for (u32 probes = 0; probes < g->sig_cap; probes++, i = (i + 1u) & mask) {
    u32 e = g->sig_tab[i];
    if (e == 0u) {
      g->sig_tab[i] = id + 1u;
      g->sig_used++;
      return;
    }
    if (e == id + 1u) return;   // already present
  }
  g->give_up = 1u;              // table full (shouldn't happen: load gate)
}

// Rebuild the table at 2x capacity from the live nodes' CURRENT
// canonical signatures.  Congruent pairs masked by stale entries are
// discovered here and pushed for merging.
static void gn_sig_grow(AtpGoalNarrow *g) {
  u32 nc = g->sig_cap * 2u;
  u32 *nt = (u32 *)calloc(nc, sizeof(u32));
  if (nt == NULL) { g->give_up = 1u; return; }
  free(g->sig_tab);
  g->sig_tab = nt;
  g->sig_cap = nc;
  g->sig_used = 0u;
  for (u32 id = 0; id < g->n_nodes; id++) {
    u32 hit = gn_sig_lookup(g, id);
    if (hit != GN_NONE && gn_find(g, hit) != gn_find(g, id)) {
      gn_wl_push(g, id, hit);
    } else {
      gn_sig_insert(g, id);
    }
  }
}

static void gn_use_add(AtpGoalNarrow *g, u32 klass, u32 parent) {
  if (g->use_n >= g->use_cap) {
    u32 nc = (g->use_cap == 0u) ? 1024u : g->use_cap * 2u;
    u32 *nn = (u32 *)realloc(g->use_next,   (size_t)nc * sizeof(u32));
    if (nn == NULL) { g->give_up = 1u; return; }
    g->use_next = nn;
    u32 *np = (u32 *)realloc(g->use_parent, (size_t)nc * sizeof(u32));
    if (np == NULL) { g->give_up = 1u; return; }
    g->use_parent = np;
    g->use_cap = nc;
  }
  u32 e = g->use_n++;
  g->use_parent[e] = parent;
  g->use_next[e] = g->use_head[klass];
  g->use_head[klass] = e + 1u;   // +1: 0 = empty
}

static u8 gn_nodes_grow(AtpGoalNarrow *g) {
  u32 nc = (g->cap_nodes == 0u) ? 4096u : g->cap_nodes * 2u;
  if (nc > g->max_nodes) nc = g->max_nodes;
  if (nc <= g->cap_nodes) return 0u;
#define GN_GROW(f, ty) do {                                        \
    void *p = realloc(g->f, (size_t)nc * sizeof(ty));              \
    if (p == NULL) return 0u;                                      \
    g->f = (ty *)p;                                                \
  } while (0)
  GN_GROW(label, u32); GN_GROW(kid_off, u32); GN_GROW(n_kids, u16);
  GN_GROW(uf, u32); GN_GROW(rank, u8); GN_GROW(cone, u8);
  GN_GROW(size, u16); GN_GROW(term, Term); GN_GROW(use_head, u32);
  GN_GROW(swept_hi, u32);
#undef GN_GROW
  g->cap_nodes = nc;
  return 1u;
}

// Hash-cons ground term `t` into the DAG.  Returns its node id (an
// existing congruent node when one is known), or GN_NONE on
// budget/shape failure.  `mark_cone` stamps the reached class -- and,
// via the recursion, every subterm's class -- goal-reachable.
static u32 gn_add_term(AtpGoalNarrow *g, Term t, u8 mark_cone) {
  if (g->give_up) return GN_NONE;
  if (term_tag(t) != TAG_CTR) return GN_NONE;   // var/num: not ground
  u32 nk32 = term_ctr_n(t);
  if (nk32 > GN_MAX_KIDS) return GN_NONE;
  u16 nk = (u16)nk32;
  u32 kid_ids[GN_MAX_KIDS];
  u32 sz = 1u;
  for (u16 i = 0; i < nk; i++) {
    u32 k = gn_add_term(g, term_ctr_at(t, i), mark_cone);
    if (k == GN_NONE) return GN_NONE;
    kid_ids[i] = k;
    sz += g->size[k];
  }
  if (sz > 0xFFFFu) sz = 0xFFFFu;
  if (g->n_nodes >= g->cap_nodes && !gn_nodes_grow(g)) {
    g->give_up = 1u;
    return GN_NONE;
  }
  if (g->kids_n + nk > g->kids_cap) {
    u32 nc = (g->kids_cap == 0u) ? 8192u : g->kids_cap * 2u;
    while (nc < g->kids_n + nk) nc *= 2u;
    u32 *np = (u32 *)realloc(g->kids, (size_t)nc * sizeof(u32));
    if (np == NULL) { g->give_up = 1u; return GN_NONE; }
    g->kids = np; g->kids_cap = nc;
  }
  // Tentative node (needed so the probe can compare signatures).
  u32 id = g->n_nodes;
  g->label[id] = term_ext(t);
  g->kid_off[id] = g->kids_n;
  g->n_kids[id] = nk;
  for (u16 i = 0; i < nk; i++) g->kids[g->kids_n + i] = kid_ids[i];
  g->uf[id] = id;
  g->rank[id] = 0u;
  g->cone[id] = 0u;
  g->size[id] = (u16)sz;
  g->term[id] = t;
  g->use_head[id] = 0u;
  g->swept_hi[id] = 0u;
  u32 hit = gn_sig_lookup(g, id);   // lookup-only: no table mutation
  if (hit != GN_NONE) {
    if (mark_cone) g->cone[gn_find(g, hit)] = 1u;
    return hit;                     // tentative node abandoned
  }
  // Commit.
  g->n_nodes++;
  g->kids_n += nk;
  gn_sig_insert(g, id);
  for (u16 i = 0; i < nk; i++) {
    u32 kr = gn_find(g, kid_ids[i]);
    u8 dup = 0u;
    for (u16 j = 0; j < i; j++) {
      if (gn_find(g, kid_ids[j]) == kr) { dup = 1u; break; }
    }
    if (!dup) gn_use_add(g, kr, id);
  }
  if (mark_cone) g->cone[id] = 1u;
  return id;
}

// Symbol count of the INSTANCE rep*sub without building it: CTR nodes
// count 1 + kids; a variable counts its binding's size.  Lets the size
// gates run before thvm_subst_apply, so rejected instances allocate
// nothing (the sweep's dominant allocation otherwise).
static u32 gn_inst_size_of(Term rep, const RewriteSubst *sub) {
  switch (term_tag(rep)) {
    case TAG_FVR: {
      Term b = sub->bindings[term_ext(rep)];
      return (b == 0) ? 1u : atp_symbol_count(b);
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(rep);
      u32 s = 1u;
      for (u32 i = 0; i < n; i++)
        s += gn_inst_size_of(term_ctr_at(rep, i), sub);
      return s;
    }
    default: return 1u;
  }
}

// Lookup-only twin of gn_add_term: the node id of ground term `t` if
// t (hash-consed) is ALREADY in the DAG, else GN_NONE.  No mutation.
static u32 gn_lookup_term(AtpGoalNarrow *g, Term t) {
  if (term_tag(t) != TAG_CTR) return GN_NONE;
  u32 nk32 = term_ctr_n(t);
  if (nk32 > GN_MAX_KIDS) return GN_NONE;
  u16 nk = (u16)nk32;
  u32 kid_ids[GN_MAX_KIDS];
  for (u16 i = 0; i < nk; i++) {
    u32 k = gn_lookup_term(g, term_ctr_at(t, i));
    if (k == GN_NONE) return GN_NONE;
    kid_ids[i] = k;
  }
  // Probe with a TENTATIVE (uncommitted) node exactly like gn_add_term.
  if (g->n_nodes >= g->cap_nodes && !gn_nodes_grow(g)) return GN_NONE;
  if (g->kids_n + nk > g->kids_cap) {
    u32 nc = (g->kids_cap == 0u) ? 8192u : g->kids_cap * 2u;
    while (nc < g->kids_n + nk) nc *= 2u;
    u32 *np = (u32 *)realloc(g->kids, (size_t)nc * sizeof(u32));
    if (np == NULL) return GN_NONE;
    g->kids = np; g->kids_cap = nc;
  }
  u32 id = g->n_nodes;
  g->label[id] = term_ext(t);
  g->kid_off[id] = g->kids_n;
  g->n_kids[id] = nk;
  for (u16 i = 0; i < nk; i++) g->kids[g->kids_n + i] = kid_ids[i];
  return gn_sig_lookup(g, id);   // tentative node abandoned either way
}

// Union the classes of a and b and propagate congruences to fixpoint.
static void gn_merge(AtpGoalNarrow *g, u32 a, u32 b) {
  gn_wl_push(g, a, b);
  while (g->wl_n >= 2u && !g->give_up) {
    u32 y = g->wl[--g->wl_n];
    u32 x = g->wl[--g->wl_n];
    u32 rx = gn_find(g, x), ry = gn_find(g, y);
    if (rx == ry) continue;
    if (g->rank[rx] < g->rank[ry]) { u32 t = rx; rx = ry; ry = t; }
    if (g->rank[rx] == g->rank[ry]) g->rank[rx]++;
    g->uf[ry] = rx;                   // ry loses
    g->cone[rx] |= g->cone[ry];
    g->st_merges++;
    // Reprocess ry's use list: each parent's canonical signature just
    // changed; re-probe and merge with any congruent peer, else
    // re-insert under the new signature.
    u32 e = g->use_head[ry];
    u32 tail_e = 0u;
    while (e != 0u) {
      u32 p = g->use_parent[e - 1u];
      u32 hit = gn_sig_lookup(g, p);
      if (hit != GN_NONE && gn_find(g, hit) != gn_find(g, p)) {
        gn_wl_push(g, p, hit);
      } else {
        gn_sig_insert(g, p);
      }
      tail_e = e;
      e = g->use_next[e - 1u];
    }
    // Concat ry's use list onto rx's (those parents must be
    // reprocessed again when the merged class merges again).
    if (tail_e != 0u) {
      g->use_next[tail_e - 1u] = g->use_head[rx];
      g->use_head[rx] = g->use_head[ry];
      g->use_head[ry] = 0u;
    }
  }
}

// Ingest `l = r` (both ground).
static void gn_add_eq(AtpGoalNarrow *g, Term l, Term r) {
  u32 a = gn_add_term(g, l, 0u);
  if (a == GN_NONE) return;
  u32 b = gn_add_term(g, r, 0u);
  if (b == GN_NONE) return;
  g->st_eqs++;
  gn_merge(g, a, b);
}

// Wall/heap guard for the e-match sweep -- the MNF loop-guard
// discipline: every live goal-narrow Term is parked in g->term[]
// (rooted by thvm_atp_gc_collect) at the points this is polled, so a
// Cheney collection here is safe.  Returns 1 when the sweep must stop.
static int gn_loop_guard(AtpState *s, AtpGoalNarrow *g) {
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
  if (atp_heap_under_pressure()) { g->give_up = 1u; return 1; }
  return 0;
}

// Ingest every not-yet-seen rule slot whose both sides are ground.
// (Slots later killed by interreduction/subsumption STAY ingested:
// a deleted rule is still a consequence of E, so the entailment
// check only gains strength.)
static void gn_ingest_rules(AtpState *s, AtpGoalNarrow *g) {
  while (g->rules_seen < s->n_rules && !g->give_up) {
    u32 i = g->rules_seen++;
    if (s->r_dead != NULL && s->r_dead[i]) continue;
    Term l = atp_rule_term(s, i, 0u);
    Term r = atp_rule_term(s, i, 1u);
    if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) continue;
    if (!gn_term_is_ground(l) || !gn_term_is_ground(r)) continue;
    gn_add_eq(g, l, r);
  }
}

// One e-match sweep: root-match rule faces against the small
// goal-cone members; each hit merges `subject = instance`.  Returns 1
// if the goal classes joined mid-sweep.
//
// TRY-ONCE: e-matching is per NODE TERM, which is immutable, so a
// (node, rule) pair can never produce a new result -- class merges
// change the E-graph, not the syntactic match.  Each node carries a
// rule watermark (swept_hi); a sweep only processes rules the node
// has not seen, making sweeps exactly incremental: the steady-state
// cost is new-rules x subjects + new-subjects x rules.  Passes cover
// cascades (an instance created in pass k is a fresh subject in pass
// k+1); the pass loop exits early once no node did work.
static u8 gn_ematch_sweep(AtpState *s, AtpGoalNarrow *g,
                          u32 goal_a, u32 goal_b, u8 endgame) {
  g->st_sweeps++;
  u32 rules_hi = s->n_rules;
  u32 inst_cap_sweep = g->n_inst + g->inst_sweep;   // per-sweep quota
  if (inst_cap_sweep > g->max_inst) inst_cap_sweep = g->max_inst;
  if (endgame) inst_cap_sweep = g->max_inst;        // burst: spend it all
  for (u32 pass = 0; pass < g->iters && !g->give_up; pass++) {
    u32 n_subj = 0u;
    u8  progress = 0u;
    u32 nodes_at_pass = g->n_nodes;   // instances become subjects next pass
    for (u32 id = 0; id < nodes_at_pass && n_subj < g->max_subjects; id++) {
      if (g->size[id] > g->subj_size) continue;
      if (g->swept_hi[id] >= rules_hi) continue;
      if (g->cone_only && !g->cone[gn_find(g, id)]) continue;
      u32 ri_lo = g->swept_hi[id];
      n_subj++;
      progress = 1u;
      u32 ri = ri_lo;
      u8  aborted = 0u;
      for (; ri < rules_hi; ri++) {
        if (s->r_dead != NULL && s->r_dead[ri]) continue;
        if (gn_loop_guard(s, g)) { aborted = 1u; break; }
        // Per-sweep instance quota reached: suspend the sweep HERE.
        // The watermark below stays at ri, so no (node, rule) pair is
        // consumed without its create-arm -- the next sweep resumes
        // exactly where this one stopped.  (Discovery-only mode is
        // reserved for the GLOBAL max_inst exhaustion.)
        if (!endgame &&
            g->n_inst >= inst_cap_sweep && g->n_inst < g->max_inst) {
          aborted = 1u;
          break;
        }
        Term fl = atp_rule_term(s, ri, 0u);
        Term fr = atp_rule_term(s, ri, 1u);
        if (term_tag(fl) == TAG_NUM || term_tag(fr) == TAG_NUM) continue;
        for (u8 face = 0u; face < 2u; face++) {
          Term pat = face ? fr : fl;
          Term rep = face ? fl : fr;
          // Root-label prefilter (a bare-variable pattern matches any
          // subject; a CTR pattern only its own label).
          if (term_tag(pat) == TAG_CTR &&
              term_ext(pat) != g->label[id]) continue;
          // The instance must come out ground: every replacement
          // variable must be bound by the pattern match.
          if (!atp_vars_contained(rep, pat)) continue;
          RewriteSubst sub = {{0}};
          if (!thvm_match(pat, g->term[id], &sub)) continue;
          // Size discipline (the WM mnfup analog), BEFORE building the
          // instance: it may outgrow its subject by up_slack symbols,
          // under an absolute ceiling.  Shrinking steps are always in.
          u32 osz = gn_inst_size_of(rep, &sub);
          if (osz > g->inst_size) continue;
          if (osz > (u32)g->size[id] + g->up_slack) continue;
          Term out = thvm_subst_apply(rep, &sub);
          u32 nid;
          if (g->n_inst < g->max_inst) {
            u32 before = g->n_nodes;
            nid = gn_add_term(g, out, 1u);
            if (nid == GN_NONE) {
              if (g->give_up) { aborted = 1u; break; }
              continue;
            }
            g->n_inst += g->n_nodes - before;
          } else {
            // Global instance budget exhausted: discovery-only -- the
            // sweep may still MERGE existing nodes, never create.
            nid = gn_lookup_term(g, out);
            if (nid == GN_NONE) continue;
          }
          if (gn_find(g, nid) != gn_find(g, id)) {
            g->st_hits++;
            gn_merge(g, id, nid);
            if (gn_find(g, goal_a) == gn_find(g, goal_b)) {
              g->swept_hi[id] = ri;    // conservative: pair set resumes
              return 1u;
            }
          }
        }
        if (aborted) break;
      }
      // Watermark exactly what was processed: [ri_lo, ri) on abort,
      // the full [ri_lo, rules_hi) otherwise.
      g->swept_hi[id] = aborted ? ri : rules_hi;
      if (aborted) return (u8)(gn_find(g, goal_a) == gn_find(g, goal_b));
    }
    if (!progress) break;
  }
  return (u8)(gn_find(g, goal_a) == gn_find(g, goal_b));
}

// GC hooks (wired into thvm_atp_gc_collect beside the MNF hooks): the
// node terms are the only heap cells goal-narrow keeps across steps.
static u32 gn_gc_count(struct AtpGoalNarrow *g) {
  return (g == NULL) ? 0u : g->n_nodes;
}
static void gn_gc_gather(struct AtpGoalNarrow *g, Term *roots, u32 *w) {
  if (g == NULL) return;
  for (u32 i = 0; i < g->n_nodes; i++) roots[(*w)++] = g->term[i];
}
static void gn_gc_writeback(struct AtpGoalNarrow *g, const Term *roots,
                            u32 base) {
  if (g == NULL) return;
  for (u32 i = 0; i < g->n_nodes; i++) g->term[i] = roots[base + i];
}

// The goal_check hook.  Returns 1 when the pair (gl, gr) is entailed
// by the goal-narrow E-graph (possibly after an e-match sweep).
static u8 atp_goal_narrow_joined(AtpState *s, Term gl, Term gr) {
  if (!s->use_goal_narrow) return 0u;
  if (s->gnarrow == NULL) {
    s->gnarrow = (struct AtpGoalNarrow *)gn_create();
    if (s->gnarrow == NULL) { s->use_goal_narrow = 0u; return 0u; }
  }
  AtpGoalNarrow *g = (AtpGoalNarrow *)s->gnarrow;
  static int dbg = -1;
  if (dbg < 0) dbg = (getenv("THVM_ATP_GN_DEBUG") != NULL) ? 1 : 0;
  // Seeding + rule ingestion are incremental: cheap on every check.
  u32 a = gn_add_term(g, gl, 1u);
  u32 b = gn_add_term(g, gr, 1u);
  if (a == GN_NONE || b == GN_NONE) return 0u;   // non-ground goal
  gn_ingest_rules(s, g);
  // Endgame burst: past 85% of the wall the interleave pacing has
  // nothing left to protect -- lift the stride and the per-sweep
  // quota so the remaining instance budget converts a near-miss CC
  // before the deadline (the mission's post-saturation endgame).  The
  // span is the wall from the first check to the deadline; the progress
  // through it is read on the work clock (atp_work_peek_us), so the burst
  // starts at the same point of the search under any load, as the LRS
  // reach estimate does.
  u8 endgame = 0u;
  {
    u64 now = atp_now_us();
    u64 work = atp_work_peek_us();
    if (g->t0_us == 0ull && now != 0ull) {
      g->t0_us = now;
      g->w0_us = work;
    }
    if (s->wall_deadline_us != 0ull && g->t0_us != 0ull &&
        now != 0ull && s->wall_deadline_us > g->t0_us) {
      u64 span = s->wall_deadline_us - g->t0_us;
      if (work >= g->w0_us + (span * 85ull) / 100ull) endgame = 1u;
    }
  }
  u8 joined = (u8)(gn_find(g, a) == gn_find(g, b));
  if (!joined && !g->give_up) {
    // Stride-gated e-match sweep (the expensive part).
    if (!g->swept_once || endgame ||
        (u64)s->r_revision >= g->last_sweep_rev + g->stride) {
      g->swept_once = 1u;
      g->last_sweep_rev = (u64)s->r_revision;
      joined = gn_ematch_sweep(s, g, a, b, endgame);
    }
  }
  if (joined) g->joined = 1u;
  if (joined || dbg) {
    fprintf(stderr,
            "[gn] %s nodes=%u inst=%u eqs=%llu merges=%llu hits=%llu "
            "sweeps=%llu give_up=%u rules_seen=%u rev=%u\n",
            joined ? "JOINED" : "stats",
            g->n_nodes, g->n_inst, (unsigned long long)g->st_eqs,
            (unsigned long long)g->st_merges,
            (unsigned long long)g->st_hits,
            (unsigned long long)g->st_sweeps,
            g->give_up, g->rules_seen, s->r_revision);
  }
  return joined;
}
