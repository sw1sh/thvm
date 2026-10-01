// === ENIGMA-style CP feature extraction + labelled dataset ==========
// Data-foundation step for a LEARNED critical-pair selector.  See
// thvm.h for the feature schema; training (logistic regression / GBDT)
// and the resulting fast C scorer in select_cp are a later step.

// Count distinct FVR ids and total FVR occurrences across (l, r) in a
// single pair of walks.  `seen` is a small membership bitset keyed on
// the var id modulo its width (var ids are dense [0,k) after
// ATP_VAR_NORM, so collisions don't happen on the canonical CP forms
// this records; the count is exact for k <= 64).
static void atp_cp_var_stats_rec(Term t, u64 *seen, u32 *distinct,
                                 u32 *occ) {
  enum { STATS_INL = 64 };
  Term  st_inl[STATS_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = STATS_INL;
  for (;;) {
    switch (term_tag(t)) {
      case TAG_FVR: {
        u32 id = term_ext(t);
        (*occ)++;
        u32 bit = id & 63u;
        if (!((*seen >> bit) & 1u)) {
          *seen |= (1ull << bit);
          (*distinct)++;
        }
        break;
      }
      case TAG_CTR: {
        u32 n = term_ctr_n(t);
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) {
          st[nst++] = term_ctr_at(t, i);
        }
        break;
      }
      default:
        break;
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}

// Top CTR label of a term (the feature's "top symbol"); 0 for a
// variable / atom / non-CTR root.
static u32 atp_cp_top_symbol(Term t) {
  return (term_tag(t) == TAG_CTR) ? term_ext(t) : 0u;
}

// Does any top-symbol-headed subterm of `t` also occur (by top symbol)
// as a subterm of the goal term `g`?  A cheap structural-overlap proxy
// for ENIGMA's goal-distance feature: 1 if the CP touches a function
// symbol the goal uses at some position, else 0.  Variables carry no
// goal signal (a var generalizes everything), mirroring the MinStruct
// spirit of the CPinGoal classifier.
static int atp_term_label_set(Term t, u64 *labels) {
  enum { LABEL_INL = 64 };
  Term  st_inl[LABEL_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = LABEL_INL;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 lab = term_ext(t);
      *labels |= (1ull << (lab & 63u));
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return 0;
}
static int atp_cp_shares_goal_symbol_vs(Term gl, Term gr, Term l, Term r) {
  u64 goal_labels = 0u;
  atp_term_label_set(gl, &goal_labels);
  atp_term_label_set(gr, &goal_labels);
  u64 cp_labels = 0u;
  atp_term_label_set(l, &cp_labels);
  atp_term_label_set(r, &cp_labels);
  return (goal_labels & cp_labels) != 0u;
}

// === ENIGMA Tier 2: anonymised CP hypergraph builder ================
// Build state threaded through the preorder walk.  The dedup map keys a
// SYMBOL/VAR node by (kind, identity): SYM_CTR uses the CTR label,
// SYM_NUM uses the raw numeric value, and VAR uses the FVR id.  These
// share one positional key table (distinct `kind` keeps a CTR label `3`
// and a NUM value `3` from colliding).  `node_idx` is the graph node the
// key maps to; `occ` counts occurrences so we can write the structural
// occurrence_count feature after the walk.  Crucially the key VALUE is
// only ever used for dedup equality -- it never reaches node_feat, so
// the graph is invariant under any consistent renaming.
typedef enum {
  ATP_CPG_KEY_CTR = 0,   // a CTR label
  ATP_CPG_KEY_NUM = 1,   // a NUM raw value
  ATP_CPG_KEY_VAR = 2,   // an FVR id
} AtpCpgKeyKind;

typedef struct {
  u8         kind;        // AtpCpgKeyKind
  u64        key;         // the concrete identity (label / value / var id)
  u32        node_idx;    // the SYMBOL/VAR graph node for this key
  u32        occ;         // occurrence count across both CP sides
  u8         side_mask;   // bit0 = seen on lhs, bit1 = seen on rhs
} AtpCpgKeyEnt;

typedef struct {
  AtpCpGraph      *g;
  const AtpState  *s;      // goal cone for cols 8-9; NULL = anonymised only
  AtpCpgKeyEnt     keys[ATP_CPG_MAX_NODES];   // one entry per distinct sym/var
  u32              n_keys;
} AtpCpgBuild;

// Goal-relevance columns for a SYMBOL node with concrete CTR label
// `label`: col 8 goal_closeness = 1/(1+sine_level) from the reused s2a
// table (only when it has been filled for this goal), col 9 is_goal_symbol
// = conjecture-cone membership.  Both are 0 without a goal / state, so the
// default anonymised graph is unchanged.  A consistent relabel of the
// whole problem preserves both, keeping the graph rename-invariant.
static void atp_cpg_symbol_goal_cols(const AtpState *s, u32 label,
                                     float *f) {
  if (s == NULL || s->goal_lhs == 0 || label >= WALD_MAX_SYMBOLS) return;
  if (s->conj_sym_mask & ((u64)1 << label)) f[9] = 1.0f;
  if (s->goal_levels_ready) {
    u8 lvl = s->sym_slvl[label];
    if (lvl != ATP_REL_LEVEL_REMOTE) f[8] = 1.0f / (1.0f + (float)lvl);
  }
}

// Append a node of the given type; returns its index, or U32_MAX on cap
// overflow (b->g->overflow is set).  node_feat is cleared here; the
// caller patches in the type-specific structural columns.
static u32 atp_cpg_add_node(AtpCpgBuild *b, u8 type) {
  AtpCpGraph *g = b->g;
  if (g->n_nodes >= ATP_CPG_MAX_NODES) { g->overflow = 1u; return 0xffffffffu; }
  u32 idx = g->n_nodes++;
  g->node_type[idx] = type;
  g->node_label[idx] = 0u;   // TERM / CP-super carry no symbol; intern overwrites
  float *f = g->node_feat + (size_t)idx * ATP_CPG_FEAT_DIM;
  for (u32 j = 0; j < ATP_CPG_FEAT_DIM; j++) f[j] = 0.0f;
  return idx;
}

// Append a directed edge; sets overflow on cap.
static void atp_cpg_add_edge(AtpCpgBuild *b, u32 src, u32 dst, u8 type) {
  AtpCpGraph *g = b->g;
  if (g->n_edges >= ATP_CPG_MAX_EDGES) { g->overflow = 1u; return; }
  u32 e = g->n_edges++;
  g->edge_src[e]  = src;
  g->edge_dst[e]  = dst;
  g->edge_type[e] = type;
}

// Find-or-create the SYMBOL/VAR node for (kind, key) in first-appearance
// order; bumps its occurrence count.  Returns the node index, or
// U32_MAX on overflow.  `node_type` is ATP_CPG_SYMBOL or ATP_CPG_VAR;
// `arity` is the CTR arity for a symbol (0 otherwise) and is written
// only when the node is first created.
static u32 atp_cpg_intern(AtpCpgBuild *b, u8 kind, u64 key,
                          u8 node_type, u32 arity, u8 side) {
  u8 side_bit = (u8)(1u << side);
  for (u32 i = 0; i < b->n_keys; i++) {
    if (b->keys[i].kind == kind && b->keys[i].key == key) {
      b->keys[i].occ++;
      b->keys[i].side_mask |= side_bit;
      return b->keys[i].node_idx;
    }
  }
  u32 idx = atp_cpg_add_node(b, node_type);
  if (idx == 0xffffffffu) return idx;
  // Record the concrete symbol identity (CTR label / FVR id / NUM value)
  // for exact reconstruction; the feature columns below stay anonymised.
  b->g->node_label[idx] = key;
  // Set the structural type columns now; occurrence_count, occurs_lhs/rhs
  // are patched in after the whole walk so repeat occurrences are counted.
  float *f = b->g->node_feat + (size_t)idx * ATP_CPG_FEAT_DIM;
  if (node_type == ATP_CPG_SYMBOL) {
    f[1] = 1.0f; f[3] = (float)arity;
    // Goal-relevance columns key on the CTR label only; a NUM-atom symbol
    // (kind KEY_NUM) has no conjecture label, so it stays goal-neutral.
    if (kind == ATP_CPG_KEY_CTR) atp_cpg_symbol_goal_cols(b->s, (u32)key, f);
  } else /* ATP_CPG_VAR */ { f[2] = 1.0f; }
  if (b->n_keys < ATP_CPG_MAX_NODES) {
    AtpCpgKeyEnt *ke = &b->keys[b->n_keys++];
    ke->kind = kind; ke->key = key; ke->node_idx = idx; ke->occ = 1u;
    ke->side_mask = side_bit;
  }
  return idx;
}

// Preorder walk: create a TERM node for `t`, wire it to its root
// symbol/var node (E_TERM_SYM), recurse into children wiring
// E_TERM_CHILD edges.  Returns the TERM node index for `t`, or U32_MAX
// on overflow (which short-circuits the rest of the walk).
static u32 atp_cpg_walk(AtpCpgBuild *b, Term t, u32 depth, u32 arg_index,
                        u8 side) {
  u32 tn = atp_cpg_add_node(b, ATP_CPG_TERM);
  if (tn == 0xffffffffu) return tn;
  // A TERM node is a single occurrence; mark is_term + occ 1, plus its
  // structural position (depth, argument index) and CP side.
  float *f = b->g->node_feat + (size_t)tn * ATP_CPG_FEAT_DIM;
  f[0] = 1.0f; f[4] = 1.0f;
  f[6] = (float)depth; f[7] = (float)arg_index;
  f[10 + side] = 1.0f;             // occurs_lhs (side 0) / occurs_rhs (side 1)

  u8  tag = term_tag(t);
  u32 root;
  if (tag == TAG_CTR) {
    u32 n = term_ctr_n(t);
    root = atp_cpg_intern(b, ATP_CPG_KEY_CTR, (u64)term_ext(t),
                          ATP_CPG_SYMBOL, n, side);
    if (root == 0xffffffffu) return root;
    atp_cpg_add_edge(b, tn, root, ATP_CPG_E_TERM_SYM);
    for (u32 i = 0; i < n; i++) {
      u32 cn = atp_cpg_walk(b, term_ctr_at(t, i), depth + 1u, i, side);
      if (cn == 0xffffffffu) return cn;
      atp_cpg_add_edge(b, tn, cn, ATP_CPG_E_TERM_CHILD);
    }
  } else if (tag == TAG_FVR) {
    root = atp_cpg_intern(b, ATP_CPG_KEY_VAR, (u64)term_ext(t),
                          ATP_CPG_VAR, 0u, side);
    if (root == 0xffffffffu) return root;
    atp_cpg_add_edge(b, tn, root, ATP_CPG_E_TERM_SYM);
  } else { /* TAG_NUM and any other atom: a nullary constant-like symbol */
    root = atp_cpg_intern(b, ATP_CPG_KEY_NUM, (u64)term_val(t),
                          ATP_CPG_SYMBOL, 0u, side);
    if (root == 0xffffffffu) return root;
    atp_cpg_add_edge(b, tn, root, ATP_CPG_E_TERM_SYM);
  }
  return tn;
}

fn int thvm_atp_cp_graph(const AtpState *s, Term lhs, Term rhs,
                         AtpCpGraph *out) {
  if (out == NULL) return 0;
  out->n_nodes  = 0u;
  out->n_edges  = 0u;
  out->overflow = 0u;

  AtpCpgBuild b;
  b.g = out;
  b.s = s;
  b.n_keys = 0u;

  // Node 0 is always the CP super-node.  It spans both sides of the CP.
  u32 super = atp_cpg_add_node(&b, ATP_CPG_CPSUPER);
  if (super == 0xffffffffu) return 0;     // cap of 0 -- defensive
  {
    float *f = out->node_feat + (size_t)super * ATP_CPG_FEAT_DIM;
    f[5] = 1.0f; f[4] = 1.0f;             // is_cpsuper + single occurrence
    f[10] = 1.0f; f[11] = 1.0f;           // occurs on both sides
  }

  // Walk lhs (side 0) then rhs (side 1) in preorder; each side root is at
  // depth 0 / argument index 0.
  u32 lroot = atp_cpg_walk(&b, lhs, 0u, 0u, 0u);
  if (lroot == 0xffffffffu) return 0;
  u32 rroot = atp_cpg_walk(&b, rhs, 0u, 0u, 1u);
  if (rroot == 0xffffffffu) return 0;

  atp_cpg_add_edge(&b, super, lroot, ATP_CPG_E_CP_LHS);
  atp_cpg_add_edge(&b, super, rroot, ATP_CPG_E_CP_RHS);
  if (out->overflow) return 0;

  // Patch each SYMBOL/VAR node's occurrence_count + occurs_lhs/rhs from the
  // dedup table (a symbol can appear on both sides / many times).
  for (u32 i = 0; i < b.n_keys; i++) {
    float *f = out->node_feat
             + (size_t)b.keys[i].node_idx * ATP_CPG_FEAT_DIM;
    f[4]  = (float)b.keys[i].occ;
    f[10] = (b.keys[i].side_mask & 1u) ? 1.0f : 0.0f;
    f[11] = (b.keys[i].side_mask & 2u) ? 1.0f : 0.0f;
  }
  return 1;
}

// The feature vector against an explicit goal pair (gl, gr) -- the goal
// normal form a caller captured earlier (the selection-time re-ranker
// scores a CP against the goal as it stood at the CP's push); gl == 0
// means no goal.  thvm_atp_cp_features passes the current goal.
static void atp_cp_features_vs(const AtpState *s, Term lhs, Term rhs, u32 age,
                               Term gl, Term gr, float *out) {
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) out[i] = 0.0f;

  u32 sl = atp_symbol_count(lhs), sr = atp_symbol_count(rhs);
  u32 dl = atp_term_depth(lhs),   dr = atp_term_depth(rhs);
  u64 seen = 0u; u32 distinct = 0u, occ = 0u;
  atp_cp_var_stats_rec(lhs, &seen, &distinct, &occ);
  atp_cp_var_stats_rec(rhs, &seen, &distinct, &occ);

  // `s` is logically const for feature reads but the weight helpers
  // take a non-const AtpState (they only read the rule set / config).
  AtpState *sm = (AtpState *)s;

  out[0]  = (float)(sl + sr);
  out[1]  = (float)(dl > dr ? dl : dr);
  out[2]  = (float)distinct;
  out[3]  = (float)occ;
  out[4]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_ADD);
  out[5]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_GT);
  out[6]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_MIX2);
  out[7]  = (gl != 0) ? (float)atp_goal_weight_vs(gl, gr, lhs, rhs) : 0.0f;
  out[8]  = (float)age;
  out[9]  = (float)atp_cp_top_symbol(lhs);
  out[10] = (float)atp_cp_top_symbol(rhs);
  out[11] = (gl != 0) ? (float)atp_cp_shares_goal_symbol_vs(gl, gr, lhs, rhs) : 0.0f;
  KboCmp c = atp_compare(sm, lhs, rhs);
  out[12] = (float)((c == KBO_GT || c == KBO_LT) ? 1 : 0);
  out[13] = (float)atp_unif_measure(lhs, rhs);
}

fn void thvm_atp_cp_features(const AtpState *s, Term lhs, Term rhs,
                             u32 age, float *out) {
  if (out == NULL) return;
  Term gl = 0, gr = 0;
  if (s != NULL && s->goal_lhs != 0) {
    gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
    gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  }
  atp_cp_features_vs(s, lhs, rhs, age, gl, gr, out);
}

fn void thvm_atp_set_record_cp_features(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_cp_features = on ? 1u : 0u;
}

fn void thvm_atp_set_record_cp_graphs(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_cp_graphs = on ? 1u : 0u;
}

// Grow the CP-graph byte arena to hold at least `need` total bytes.
static void atp_cp_graph_arena_ensure(AtpState *s, size_t need) {
  if (need <= s->cp_graph_cap) return;
  size_t cap = s->cp_graph_cap ? s->cp_graph_cap * 2u : 65536u;
  while (cap < need) cap *= 2u;
  u8 *na = (u8 *)realloc(s->cp_graph_arena, cap);
  if (na == NULL) return;   // OOM: leave arena as-is; record is dropped
  s->cp_graph_arena = na;
  s->cp_graph_cap   = cap;
}

// Serialise the anonymised hypergraph for the feature row at index `idx`
// into the byte arena, one self-describing record:
//   [u32 n_nodes][u32 n_edges][n_nodes*6 f32 node_feat][n_edges*2 u32 edges]
// cp_graph_off[idx] records the record's arena offset so the exporter
// pairs it with cp_feat_label[idx] exactly; on builder overflow or arena
// OOM the offset is SIZE_MAX and the exporter skips that row.  Only ever
// called under s->record_cp_graphs, so the default run is untouched.
static void atp_cp_graph_record(AtpState *s, u32 idx, Term lhs, Term rhs) {
  s->cp_graph_off[idx] = (size_t)-1;   // default: skipped
  if (s->cp_graph_scratch == NULL) {
    s->cp_graph_scratch = malloc(sizeof(AtpCpGraph));
    if (s->cp_graph_scratch == NULL) return;
  }
  AtpCpGraph *g = (AtpCpGraph *)s->cp_graph_scratch;
  // Pass `s` so the exported training graph carries the goal-relevance
  // columns (conjecture membership + s2a goal distance) for this problem.
  if (!thvm_atp_cp_graph(s, lhs, rhs, g)) return;   // overflow -> skip
  u32 nn = g->n_nodes, ne = g->n_edges;
  size_t rec = 8u + (size_t)nn * ATP_CPG_FEAT_DIM * sizeof(float)
             + (size_t)ne * 2u * sizeof(u32);
  atp_cp_graph_arena_ensure(s, s->cp_graph_len + rec);
  if (s->cp_graph_arena == NULL || s->cp_graph_len + rec > s->cp_graph_cap) {
    return;   // arena OOM -> skip
  }
  s->cp_graph_off[idx] = s->cp_graph_len;
  u8 *p = s->cp_graph_arena + s->cp_graph_len;
  memcpy(p, &nn, 4u); p += 4u;
  memcpy(p, &ne, 4u); p += 4u;
  if (nn) {
    size_t fb = (size_t)nn * ATP_CPG_FEAT_DIM * sizeof(float);
    memcpy(p, g->node_feat, fb); p += fb;   // nodes 0..nn-1 are contiguous
  }
  for (u32 e = 0u; e < ne; e++) {
    u32 sd[2] = { g->edge_src[e], g->edge_dst[e] };
    memcpy(p, sd, 8u); p += 8u;
  }
  s->cp_graph_len = (size_t)(p - s->cp_graph_arena);
}

// Grow the recording arrays to hold at least `need` rows.
static void atp_cp_feat_ensure(AtpState *s, u32 need) {
  if (need <= s->cp_feat_cap) return;
  u32 cap = s->cp_feat_cap ? s->cp_feat_cap * 2u : 256u;
  while (cap < need) cap *= 2u;
  s->cp_feat_rows  = (float *)realloc(s->cp_feat_rows,
                                      (size_t)cap * ATP_CP_FEATURE_DIM
                                        * sizeof(float));
  s->cp_feat_trace = (u32 *)realloc(s->cp_feat_trace, cap * sizeof(u32));
  s->cp_feat_label = (u8 *)realloc(s->cp_feat_label, cap * sizeof(u8));
  if (s->record_cp_graphs) {
    s->cp_graph_off = (size_t *)realloc(s->cp_graph_off,
                                        (size_t)cap * sizeof(size_t));
  }
  s->cp_feat_cap   = cap;
}

// Record one PROCESSED CP (called from thvm_atp_select_cp under the
// flag).  `trace_id` is the popped CP's trace-entry index.
static void atp_cp_feat_record(AtpState *s, Term lhs, Term rhs,
                               u32 trace_id) {
  atp_cp_feat_ensure(s, s->n_cp_feat + 1u);
  float *row = s->cp_feat_rows + (size_t)s->n_cp_feat * ATP_CP_FEATURE_DIM;
  thvm_atp_cp_features(s, lhs, rhs, s->n_cp_feat, row);
  s->cp_feat_trace[s->n_cp_feat] = trace_id;
  s->cp_feat_label[s->n_cp_feat] = 0u;
  if (s->record_cp_graphs && s->cp_graph_off != NULL) {
    atp_cp_graph_record(s, s->n_cp_feat, lhs, rhs);
  }
  s->n_cp_feat++;
}

// Parents (in the trace DAG) of trace entry `ti`: for the 4-child
// reasons (AXIOM/ORIENT/CP/SIMPLIFY) children 0 and 1 are parent trace
// ids; for TRACE_NORM_STEP only child 0 is a parent trace id (child 1
// is a RULE index, not a trace id).  Writes up to 2 parents into
// out[0..2) and returns the count.
static u32 atp_trace_parents(const AtpState *s, u32 ti, u32 *out) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace) return 0u;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) return 0u;
  u32 reason = term_ext(e);
  u32 n = 0u;
  u32 p0 = term_val(term_ctr_at(e, 0));
  if (p0 != ATP_TRACE_NONE) out[n++] = p0;
  if (reason != TRACE_NORM_STEP) {
    u32 p1 = term_val(term_ctr_at(e, 1));
    if (p1 != ATP_TRACE_NONE) out[n++] = p1;
  }
  return n;
}

fn u32 thvm_atp_cp_label(AtpState *s) {
  if (s == NULL || s->n_cp_feat == 0u) return 0u;

  // Proof set seed: the RULES that join the goal, via the existing
  // single-NF proof extractor.  Each fired rule's r_trace[] is its
  // TRACE_ORIENT entry; the proof-relevant CPs are the transitive
  // trace-DAG ancestors of those entries.
  static AtpProofStep steps[ATP_PROOF_MAX_STEPS * 2u];
  u32 n_steps = thvm_atp_proof_extract(s, steps,
                                       ATP_PROOF_MAX_STEPS * 2u);
  if (n_steps == 0u) return 0u;   // not single-NF extractable

  // Reachable-trace bitset over [0, n_trace).  Marked entries are the
  // proof set: ancestors of every fired rule's TRACE_ORIENT.
  u8 *reach = (u8 *)calloc(s->n_trace, 1u);
  if (reach == NULL) return 0u;

  // Worklist DFS from each proof rule's TRACE_ORIENT trace id.
  u32 *stack = (u32 *)malloc((size_t)s->n_trace * sizeof(u32));
  if (stack == NULL) { free(reach); return 0u; }
  u32 sp = 0u;
  for (u32 k = 0; k < n_steps; k++) {
    u32 ru = steps[k].rule;
    if (ru >= s->n_rules) continue;
    u32 ti = s->r_trace[ru];
    if (ti != ATP_TRACE_NONE && ti < s->n_trace && !reach[ti]) {
      reach[ti] = 1u;
      stack[sp++] = ti;
    }
  }
  while (sp > 0u) {
    u32 ti = stack[--sp];
    u32 par[2];
    u32 np = atp_trace_parents(s, ti, par);
    for (u32 i = 0; i < np; i++) {
      u32 p = par[i];
      if (p < s->n_trace && !reach[p]) {
        reach[p] = 1u;
        stack[sp++] = p;
      }
    }
  }

  // Label each recorded selected-CP: 1 iff its trace id is in the
  // proof set.  Count distinct proof-relevant selected CPs.
  u32 n_pos = 0u;
  for (u32 i = 0; i < s->n_cp_feat; i++) {
    u32 ti = s->cp_feat_trace[i];
    u8 lab = (ti != ATP_TRACE_NONE && ti < s->n_trace && reach[ti]) ? 1u : 0u;
    s->cp_feat_label[i] = lab;
    if (lab) n_pos++;
  }

  free(stack);
  free(reach);
  return n_pos;
}

fn u32 thvm_atp_cp_dataset_append(const AtpState *s, const char *path,
                                  u8 header) {
  if (s == NULL || path == NULL || s->n_cp_feat == 0u) return 0u;
  FILE *f = fopen(path, "a");
  if (f == NULL) return 0u;
  if (header) {
    fprintf(f, "label");
    static const char *names[ATP_CP_FEATURE_DIM] = {
      "size_sum", "max_depth", "n_distinct_vars", "n_var_occ",
      "weight_add", "weight_gt", "weight_mix2", "goal_weight",
      "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub",
      "orientable", "unif_measure",
    };
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) fprintf(f, "\t%s", names[j]);
    fprintf(f, "\n");
  }
  for (u32 i = 0; i < s->n_cp_feat; i++) {
    const float *row = s->cp_feat_rows + (size_t)i * ATP_CP_FEATURE_DIM;
    fprintf(f, "%u", (unsigned)s->cp_feat_label[i]);
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) fprintf(f, "\t%.6g", row[j]);
    fprintf(f, "\n");
  }
  fclose(f);
  return s->n_cp_feat;
}

// Append the labelled CP graphs as JSONL (one line per recorded CP with a
// non-skipped graph).  Each line: {"y":<0|1>,"x":[[6 feats],...],
// "e":[[src,dst],...]}.  Pairs the i-th feature row's arena record with
// cp_feat_label[i] via cp_graph_off[i].  Reads are memcpy'd out of the
// arena (no aliasing/alignment assumption).  Returns the lines written.
fn u32 thvm_atp_cp_graph_dataset_append(const AtpState *s, const char *path) {
  if (s == NULL || path == NULL || s->n_cp_feat == 0u
      || s->cp_graph_arena == NULL || s->cp_graph_off == NULL) {
    return 0u;
  }
  FILE *f = fopen(path, "a");
  if (f == NULL) return 0u;
  u32 written = 0u;
  for (u32 i = 0u; i < s->n_cp_feat; i++) {
    size_t off = s->cp_graph_off[i];
    if (off == (size_t)-1) continue;             // overflow/OOM: skipped
    const u8 *p = s->cp_graph_arena + off;
    u32 nn = 0u, ne = 0u;
    memcpy(&nn, p, 4u); memcpy(&ne, p + 4u, 4u);
    if (nn == 0u) continue;
    const u8 *fp = p + 8u;
    const u8 *ep = fp + (size_t)nn * ATP_CPG_FEAT_DIM * sizeof(float);
    fprintf(f, "{\"y\":%u,\"x\":[", (unsigned)s->cp_feat_label[i]);
    for (u32 nix = 0u; nix < nn; nix++) {
      if (nix) fputc(',', f);
      fputc('[', f);
      for (u32 fj = 0u; fj < ATP_CPG_FEAT_DIM; fj++) {
        float v;
        memcpy(&v, fp + ((size_t)nix * ATP_CPG_FEAT_DIM + fj) * sizeof(float),
               sizeof(float));
        fprintf(f, "%s%g", fj ? "," : "", (double)v);
      }
      fputc(']', f);
    }
    fprintf(f, "],\"e\":[");
    for (u32 e = 0u; e < ne; e++) {
      u32 sd[2];
      memcpy(sd, ep + (size_t)e * 2u * sizeof(u32), 2u * sizeof(u32));
      fprintf(f, "%s[%u,%u]", e ? "," : "", sd[0], sd[1]);
    }
    fprintf(f, "]}\n");
    written++;
  }
  fclose(f);
  return written;
}
