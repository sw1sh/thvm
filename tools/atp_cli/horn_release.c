// ===================================================================
// Encoded-Horn (ifeq) decode + forward-chaining release
// ===================================================================
//
// TPTP's -10 UEQ problems encode Horn logic as unit equations: a
// dispatcher axiom ifeqN(X,X,Y,Z) = Y (several functor names may
// coexist: ifeq, ifeq2, ...; arity-3 variants ifeq(X,X,Y) = Y too)
// plus conditional clauses ifeqN(c, t, then, e) = e -- "if c = t
// then then = e" -- nesting in the then-position for multiple
// conditions, with propositions as terms equated to the constant
// true.  Pure completion drowns re-deriving the conditional
// machinery, so this pass decodes the encoding and runs a budgeted
// forward-chaining release loop (positive hyper-resolution): whenever
// every condition of a decoded clause simultaneously unifies with
// known unit facts (each fact renamed apart), the instantiated
// conclusion is released as a new unit equation.  Released units are
// logical consequences of the input, so Unsatisfiable stays valid;
// g_cli_horn_applied gates the Satisfiable claim exactly like
// g_cli_sine_applied does.  THVM_ATP_HORN=0 disables; otherwise the
// pass is a no-op unless a dispatcher axiom is detected.

static int g_cli_horn_applied = 0;
static int g_prov_on;   // --proof lineage capture (provenance module below)

// Terms live in a growable node/arg arena.  head >= 0 is an index
// into the TpProblem symbol table; head < 0 is variable -(v+1) with v
// local to the owning clause.  Unification runs over (node, var-space
// offset) pairs: the rule's variables are offset 0, the fact renamed
// into condition i's space at offset (i+1)*HORN_VSPACE.
#define HORN_MAX_COND   8
#define HORN_VSPACE     64
// Var spaces: rule 0, condition facts 1..HORN_MAX_COND, subgoal
// patterns HORN_SG_OFF, probe side HORN_PROBE_OFF (a clean space for
// unifying a canonical release against a subgoal while a join's
// bindings are still live in spaces 0..HORN_MAX_COND).
#define HORN_SG_OFF     ((HORN_MAX_COND + 1) * HORN_VSPACE)
#define HORN_PROBE_OFF  ((HORN_MAX_COND + 2) * HORN_VSPACE)
#define HORN_NSLOTS     ((HORN_MAX_COND + 3) * HORN_VSPACE)
#define HORN_MAX_RELEASES 400000
#define HORN_MAX_ATTEMPTS 10000000L
#define HORN_WALL_SEC   2.5
#define HORN_SIDE_MAX   900     // printed chars per released side
#define HORN_APPLY_MAX  400     // nodes per released side
#define HORN_REL_VARS   12      // distinct vars per released unit

typedef struct { int head, nargs, argp; } HNode;
static HNode *g_hn = NULL; static int g_hn_n = 0, g_hn_cap = 0;
static int   *g_ha = NULL; static int g_ha_n = 0, g_ha_cap = 0;

static int horn_node(int head, int nargs, const int *args) {
  if (g_hn_n >= g_hn_cap) {
    g_hn_cap = (g_hn_cap == 0) ? (1 << 16) : g_hn_cap * 2;
    g_hn = (HNode *)realloc(g_hn, (size_t)g_hn_cap * sizeof(HNode));
  }
  if (nargs > 0 && g_ha_n + nargs > g_ha_cap) {
    g_ha_cap = (g_ha_cap == 0) ? (1 << 16) : g_ha_cap * 2 + nargs;
    g_ha = (int *)realloc(g_ha, (size_t)g_ha_cap * sizeof(int));
  }
  if (g_hn == NULL || (nargs > 0 && g_ha == NULL)) tp_giveup("OOM in Horn release");
  HNode *nd = &g_hn[g_hn_n];
  nd->head = head; nd->nargs = nargs; nd->argp = g_ha_n;
  for (int i = 0; i < nargs; i++) g_ha[g_ha_n + i] = args[i];
  g_ha_n += nargs;
  return g_hn_n++;
}
#define HARG(t, i) (g_ha[g_hn[(t)].argp + (i)])

// --- per-clause recursive-descent parser ----------------------------
typedef struct {
  const char *p;
  int  nv;
  char vnames[HORN_VSPACE][TP_NAME_LEN];
  int  fail;
} HParse;

static int horn_parse_term(HParse *ps, TpProblem *tp, int depth) {
  if (ps->fail || depth > 200) { ps->fail = 1; return -1; }
  while (isspace((unsigned char)*ps->p)) ps->p++;
  const char *name = ps->p;
  size_t n = 0;
  while (isalnum((unsigned char)name[n]) || name[n] == '_') n++;
  if (n == 0) { ps->fail = 1; return -1; }
  ps->p += n;
  while (isspace((unsigned char)*ps->p)) ps->p++;
  if (*ps->p == '(') {
    ps->p++;
    int args[16], na = 0;
    for (;;) {
      if (na >= 16) { ps->fail = 1; return -1; }
      args[na++] = horn_parse_term(ps, tp, depth + 1);
      if (ps->fail) return -1;
      while (isspace((unsigned char)*ps->p)) ps->p++;
      if (*ps->p == ',') { ps->p++; continue; }
      if (*ps->p == ')') { ps->p++; break; }
      ps->fail = 1; return -1;
    }
    return horn_node(tp_sym_note_ix(tp, name, n, na), na, args);
  }
  if (isupper((unsigned char)*name)) {
    if (n >= TP_NAME_LEN) n = TP_NAME_LEN - 1;
    for (int i = 0; i < ps->nv; i++)
      if (strlen(ps->vnames[i]) == n && memcmp(ps->vnames[i], name, n) == 0)
        return horn_node(-(i + 1), 0, NULL);
    if (ps->nv >= HORN_VSPACE) { ps->fail = 1; return -1; }
    memcpy(ps->vnames[ps->nv], name, n);
    ps->vnames[ps->nv][n] = '\0';
    return horn_node(-(++ps->nv), 0, NULL);
  }
  return horn_node(tp_sym_note_ix(tp, name, n, 0), 0, NULL);
}

// Parse one clause side into the arena; vars are clause-local via ps.
static int horn_parse_side(TpProblem *tp, HParse *ps, const char *s) {
  ps->p = s; ps->fail = 0;
  int t = horn_parse_term(ps, tp, 0);
  if (ps->fail) return -1;
  while (isspace((unsigned char)*ps->p)) ps->p++;
  return (*ps->p == '\0') ? t : -1;
}

// --- structural equality (same var space) ---------------------------
static int horn_eq(int a, int b) {
  if (g_hn[a].head != g_hn[b].head || g_hn[a].nargs != g_hn[b].nargs) return 0;
  for (int i = 0; i < g_hn[a].nargs; i++)
    if (!horn_eq(HARG(a, i), HARG(b, i))) return 0;
  return 1;
}

// --- unification with trail ------------------------------------------
typedef struct { int t, off; } HBind;
static HBind g_hb[HORN_NSLOTS];
static int g_htrail[HORN_NSLOTS]; static int g_htrail_n = 0;
static long g_horn_attempts = 0;

static void horn_deref(int *t, int *off) {
  while (g_hn[*t].head < 0) {
    int slot = *off + (-g_hn[*t].head - 1);
    if (g_hb[slot].t < 0) return;
    int nt = g_hb[slot].t, noff = g_hb[slot].off;
    *t = nt; *off = noff;
  }
}

static int horn_occurs(int slot, int t, int off, int depth) {
  if (depth > 400) return 1;   // treat overdeep as occurs: fail the bind
  horn_deref(&t, &off);
  if (g_hn[t].head < 0) return off + (-g_hn[t].head - 1) == slot;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_occurs(slot, HARG(t, i), off, depth + 1)) return 1;
  return 0;
}

static int horn_unify(int t1, int o1, int t2, int o2, int depth) {
  if (depth > 400) return 0;
  horn_deref(&t1, &o1); horn_deref(&t2, &o2);
  if (g_hn[t1].head < 0 && g_hn[t2].head < 0 &&
      o1 + (-g_hn[t1].head - 1) == o2 + (-g_hn[t2].head - 1)) return 1;
  if (g_hn[t1].head < 0) {
    int slot = o1 + (-g_hn[t1].head - 1);
    if (horn_occurs(slot, t2, o2, 0)) return 0;
    g_hb[slot].t = t2; g_hb[slot].off = o2;
    g_htrail[g_htrail_n++] = slot;
    return 1;
  }
  if (g_hn[t2].head < 0) {
    int slot = o2 + (-g_hn[t2].head - 1);
    if (horn_occurs(slot, t1, o1, 0)) return 0;
    g_hb[slot].t = t1; g_hb[slot].off = o1;
    g_htrail[g_htrail_n++] = slot;
    return 1;
  }
  if (g_hn[t1].head != g_hn[t2].head || g_hn[t1].nargs != g_hn[t2].nargs) return 0;
  for (int i = 0; i < g_hn[t1].nargs; i++)
    if (!horn_unify(HARG(t1, i), o1, HARG(t2, i), o2, depth + 1)) return 0;
  return 1;
}

static void horn_undo(int mark) {
  while (g_htrail_n > mark) g_hb[g_htrail[--g_htrail_n]].t = -1;
}

// --- substitution application with canonical var renaming ------------
// Unbound slots map to fresh canonical vars V0, V1, ... in traversal
// order; the map is reset per release via the touched list.
static int g_can_map[HORN_NSLOTS];
static int g_can_touch[HORN_NSLOTS]; static int g_can_touch_n = 0, g_can_nv = 0;

static void horn_can_reset(void) {
  while (g_can_touch_n > 0) g_can_map[g_can_touch[--g_can_touch_n]] = -1;
  g_can_nv = 0;
}

static int horn_apply(int t, int off, int *budget) {
  if (--*budget < 0) return -1;
  horn_deref(&t, &off);
  if (g_hn[t].head < 0) {
    int slot = off + (-g_hn[t].head - 1);
    if (g_can_map[slot] < 0) {
      if (g_can_nv >= HORN_REL_VARS) return -1;
      g_can_map[slot] = g_can_nv++;
      g_can_touch[g_can_touch_n++] = slot;
    }
    return horn_node(-(g_can_map[slot] + 1), 0, NULL);
  }
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_apply(HARG(t, i), off, budget);
    if (args[i] < 0) return -1;
  }
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

// --- printing (canonical, offset-free terms) --------------------------
// Variables are renumbered by first occurrence in the printed order via
// remap (size HORN_REL_VARS, -1-initialized, shared across the sides of
// one equation) -- so an equation and its mirror print to comparable
// dedup keys.
static int horn_print(const TpProblem *tp, char *buf, size_t cap, size_t *o,
                      int t, int *remap, int *nv) {
  if (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1;
    if (v >= HORN_REL_VARS) return 0;
    if (remap[v] < 0) remap[v] = (*nv)++;
    if (*o + 8 >= cap) return 0;
    *o += (size_t)snprintf(buf + *o, cap - *o, "V%d", remap[v]);
    return 1;
  }
  const char *name = tp->syms[g_hn[t].head].name;
  size_t nl = strlen(name);
  if (*o + nl + 2 >= cap) return 0;
  memcpy(buf + *o, name, nl); *o += nl;
  if (g_hn[t].nargs > 0) {
    buf[(*o)++] = '(';
    for (int i = 0; i < g_hn[t].nargs; i++) {
      if (i > 0) { if (*o + 1 >= cap) return 0; buf[(*o)++] = ','; }
      if (!horn_print(tp, buf, cap, o, HARG(t, i), remap, nv)) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// --- dedup set over canonical "lhs=rhs" strings ------------------------
// Sized 2x the release cap (two keys per release: itself + mirror) so
// the open-addressing probe never wraps a full table.  The CD lane
// allocates 4x that: its wall-long runs push several million accepts,
// and a saturated table silently rejects every later candidate as a
// duplicate.
#define HORN_DEDUP_SLOTS (1 << 21)
static char **g_hdup = NULL;
static int g_hdup_n = 0;
static u32 g_hdup_slots = HORN_DEDUP_SLOTS;

static int horn_dedup_add(const char *key) {   // 1 = new
  if ((u32)g_hdup_n >= g_hdup_slots - (g_hdup_slots >> 2)) return 0;
  u64 h = tp_name_hash(key, strlen(key));
  u32 slot = (u32)(h % (u64)g_hdup_slots);
  for (;;) {
    if (g_hdup[slot] == NULL) break;
    if (strcmp(g_hdup[slot], key) == 0) return 0;
    slot = (slot + 1u) % g_hdup_slots;
  }
  g_hdup[slot] = strdup(key);
  g_hdup_n++;
  return 1;
}

// --- facts, rules, head index ------------------------------------------
// src (--proof provenance): where this fact came from -- a release
// index (>= 0), an input-clause snapshot (~g_hpc index, < 0), or
// HORN_SRC_NONE when capture is off / unavailable.
#define HORN_SRC_NONE (-(1 << 30))
typedef struct {
  int l, r, nv, size, src;
  u8 processed, sghit, rel, conj, given, dead;
} HFact;
static HFact *g_hf = NULL; static int g_hf_n = 0, g_hf_cap = 0;

static int horn_size(int t) {
  int s = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) s += horn_size(HARG(t, i));
  return s;
}

// --- condensed-detachment lane (--horn-cd) ----------------------------
// The LCL condensed-detachment family regresses the blind v1 chaining
// into promiscuous major-premise towers (weakening instances or(V, t)
// growing without bound) and an instance flood no exact dedup catches.
// The classic CD prover discipline (Otter on the LCL corpus) is:
//   - a WEIGHT CAP on released conclusions, raised by ITERATIVE
//     DEEPENING when the capped space saturates without the goal
//     (kills the towers);
//   - GENERALIZATION SUBSUMPTION: a released theorem(t) is redundant
//     if a kept theorem(s) matches onto it (forward), and a kept
//     instance is dead weight once a more general release arrives
//     (backward, done lazily at pop time here);
//   - pure smallest-first given selection.
// All of it is gated on g_horn_cd so every other flavor's trajectory
// stays byte-identical.
static int  g_horn_cd = 0;             // THVM_ATP_HORN_CD (2 = slice mode)
// Parsed negative units (the goals), for the subsumption rank check
// and the CD resonators.
static int g_hg_l[64], g_hg_r[64]; static int g_hg_n = 0;
static int  g_cd_cap = 0;              // live released-size cap
static int  g_cd_step = 4;             // THVM_ATP_HORN_CD_STEP
static int  g_cd_maxvars = 5;          // THVM_ATP_HORN_CD_VARS
static int  g_cd_overcap = 0;          // some conclusion exceeded the cap
static int  g_cd_rounds = 0;
static long g_cd_fsub = 0, g_cd_bsub = 0, g_cd_reso_hits = 0;

// One-way match: pattern variables live in the HORN_SG_OFF slot space
// and bind to canonical (offset-free) instance subterms; instance
// variables are rigid constants.  Bindings ride the shared trail (the
// caller undoes to its mark).  A bound pattern variable re-encountered
// recurses with po = 0, where this reduces to structural equality of
// two canonical instance terms -- instance slots are never bound.
static int horn_cd_match(int p, int po, int t, int depth) {
  if (depth > 400) return 0;
  if (g_hn[p].head < 0) {
    if (po == HORN_SG_OFF) {
      int slot = po + (-g_hn[p].head - 1);
      if (g_hb[slot].t >= 0)
        return horn_cd_match(g_hb[slot].t, g_hb[slot].off, t, depth + 1);
      g_hb[slot].t = t; g_hb[slot].off = 0;
      g_htrail[g_htrail_n++] = slot;
      return 1;
    }
    return g_hn[t].head == g_hn[p].head;   // instance var vs instance var
  }
  if (g_hn[t].head != g_hn[p].head || g_hn[t].nargs != g_hn[p].nargs) return 0;
  for (int i = 0; i < g_hn[p].nargs; i++)
    if (!horn_cd_match(HARG(p, i), po, HARG(t, i), depth + 1)) return 0;
  return 1;
}

// --- discrimination tree over pattern sides (generalization retrieval)
// The subsumers of a candidate are found by walking its l side in
// preorder against a tree of pattern sides: a pattern variable keys as
// STAR (-1) and consumes one whole candidate subterm on lookup, so the
// visit cost is bounded by the shared-prefix structure instead of the
// fact count (a flat scan spent 89% of the wall here on LCL195-10 --
// every CD fact shares the theorem(..) = true shape, so no root filter
// discriminates).  BOTH sides of every pattern fact are inserted
// (orient 0 = l, 1 = r): one query on the candidate's l covers both
// subsumption pairings, and the leaf match validates the full equation
// with variable consistency.  Fully ground facts are never inserted --
// after dedup they cannot strictly subsume anything.
typedef struct { int key, child, next; } DtEdge;
typedef struct { int edge0, leaf0; } DtNode;
typedef struct { int fact, orient, next; } DtLeaf;
static DtNode *g_dtn = NULL; static int g_dtn_n = 0, g_dtn_cap = 0;
static DtEdge *g_dte = NULL; static int g_dte_n = 0, g_dte_cap = 0;
static DtLeaf *g_dtl = NULL; static int g_dtl_n = 0, g_dtl_cap = 0;

static int dt_node_new(void) {
  if (g_dtn_n >= g_dtn_cap) {
    g_dtn_cap = (g_dtn_cap == 0) ? (1 << 12) : g_dtn_cap * 2;
    g_dtn = (DtNode *)realloc(g_dtn, (size_t)g_dtn_cap * sizeof(DtNode));
    if (g_dtn == NULL) tp_giveup("OOM in Horn release");
  }
  g_dtn[g_dtn_n].edge0 = -1;
  g_dtn[g_dtn_n].leaf0 = -1;
  return g_dtn_n++;
}

static int dt_child(int node, int key, int create) {
  for (int e = g_dtn[node].edge0; e >= 0; e = g_dte[e].next)
    if (g_dte[e].key == key) return g_dte[e].child;
  if (!create) return -1;
  int c = dt_node_new();
  if (g_dte_n >= g_dte_cap) {
    g_dte_cap = (g_dte_cap == 0) ? (1 << 12) : g_dte_cap * 2;
    g_dte = (DtEdge *)realloc(g_dte, (size_t)g_dte_cap * sizeof(DtEdge));
    if (g_dte == NULL) tp_giveup("OOM in Horn release");
  }
  g_dte[g_dte_n].key = key;
  g_dte[g_dte_n].child = c;
  g_dte[g_dte_n].next = g_dtn[node].edge0;
  g_dtn[node].edge0 = g_dte_n++;
  return c;
}

static void dt_insert(int t, int fact, int orient) {
  if (g_dtn_n == 0) dt_node_new();   // root
  int stack[512], sp = 0, node = 0;
  stack[sp++] = t;
  while (sp > 0) {
    int cur = stack[--sp];
    int key = (g_hn[cur].head < 0) ? -1 : g_hn[cur].head;
    node = dt_child(node, key, 1);
    if (key >= 0) {
      // A truncated walk would file the fact under a WRONG (shorter)
      // path and let it subsume non-instances: skip oversized patterns
      // entirely (they are also beyond the query flattener's bound).
      if (sp + g_hn[cur].nargs > 500) return;
      for (int i = g_hn[cur].nargs - 1; i >= 0; i--)
        stack[sp++] = HARG(cur, i);
    }
  }
  if (g_dtl_n >= g_dtl_cap) {
    g_dtl_cap = (g_dtl_cap == 0) ? (1 << 12) : g_dtl_cap * 2;
    g_dtl = (DtLeaf *)realloc(g_dtl, (size_t)g_dtl_cap * sizeof(DtLeaf));
    if (g_dtl == NULL) tp_giveup("OOM in Horn release");
  }
  g_dtl[g_dtl_n].fact = fact;
  g_dtl[g_dtl_n].orient = orient;
  g_dtl[g_dtl_n].next = g_dtn[node].leaf0;
  g_dtn[node].leaf0 = g_dtl_n++;
}

// Query state: the candidate's l side, flattened to preorder with
// per-position subterm extents (STAR skips).
static int g_dt_flat[512], g_dt_skipn[512], g_dt_flat_n = 0, g_dt_over = 0;
static void dt_flatten(int t) {
  int start = g_dt_flat_n;
  if (g_dt_flat_n >= 500) { g_dt_over = 1; return; }
  g_dt_flat[g_dt_flat_n++] = t;
  if (g_hn[t].head >= 0)
    for (int i = 0; i < g_hn[t].nargs && !g_dt_over; i++)
      dt_flatten(HARG(t, i));
  g_dt_skipn[start] = g_dt_flat_n - start;
}

// Walk the tree along the flattened candidate: exact-symbol edge, or a
// STAR edge that swallows the subterm.  Complete terms are prefix-free
// in preorder (fixed arities), so leaves are only ever checked when
// the candidate is fully consumed.  A leaf hit still runs the full
// two-sided match (variable consistency, the r side, orientation).
static int dt_probe(int node, int pos, int cl, int cr, int sz_max,
                    int min_idx) {
  if (pos >= g_dt_flat_n) {
    for (int L = g_dtn[node].leaf0; L >= 0; L = g_dtl[L].next) {
      int f = g_dtl[L].fact;
      if (g_hf[f].dead || g_hf[f].size > sz_max || f < min_idx) continue;
      int pl = g_dtl[L].orient ? g_hf[f].r : g_hf[f].l;
      int pr = g_dtl[L].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      int hit = horn_cd_match(pl, HORN_SG_OFF, cl, 0) &&
                horn_cd_match(pr, HORN_SG_OFF, cr, 0);
      horn_undo(mark);
      if (hit) return 1;
    }
    return 0;
  }
  int t = g_dt_flat[pos], c;
  if (g_hn[t].head >= 0 && (c = dt_child(node, g_hn[t].head, 0)) >= 0 &&
      dt_probe(c, pos + 1, cl, cr, sz_max, min_idx)) return 1;
  if ((c = dt_child(node, -1, 0)) >= 0 &&
      dt_probe(c, pos + g_dt_skipn[pos], cl, cr, sz_max, min_idx)) return 1;
  return 0;
}

// Forward subsumption: is the candidate (l, r) an instance of a live
// fact?  Every indexed fact predates the candidate, so no age filter.
static int horn_cd_fwd_subsumed(int l, int r, int sz) {
  if (g_dtn_n == 0) return 0;
  g_dt_flat_n = 0; g_dt_over = 0;
  dt_flatten(l);
  if (g_dt_over) return 0;
  return dt_probe(0, 0, l, r, sz, 0);
}

// --- resonance (Wos): goal-shape steering by skeleton MATCH ------------
// The backward cone on a CD goal regresses only into the promiscuous
// major-premise tower theorem(or(not(V0), .. G)), and probing releases
// by UNIFICATION lets every var-tailed weakening hit it (measured: 11k
// of 19k releases "sg-hit" on LCL195-10).  What is selective is the
// goal's SHAPE: each deep-enough goal subterm, with its leaves
// wildcarded to fresh pattern variables, becomes a one-way MATCH
// pattern.  A release that matches structurally refines the goal
// (Wos's resonators, generated from the goal instead of known proofs):
//   - the weakening tower or(V, ..) fails at the pattern's rigid
//     not(*) position;
//   - the deep PM intermediates (a hypothetical-syllogism instance is
//     itself goal-shaped) match, earning the cap exemption and the
//     priority pop that pure smallest-first would never grant them.
#define CD_RESO_MAX       32
#define CD_RESO_MIN_RIGID 5
static int g_cd_reso[CD_RESO_MAX]; static int g_cd_reso_n = 0;

static int horn_cd_skel_rigids(int t) {   // non-nullary heads
  if (g_hn[t].head < 0 || g_hn[t].nargs == 0) return 0;
  int n = 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    n += horn_cd_skel_rigids(HARG(t, i));
  return n;
}

// Copy t with every leaf (variable or nullary symbol) replaced by a
// fresh pattern variable; -1 when the variable space is exhausted.
static int horn_cd_skel(int t, int *nv) {
  if (g_hn[t].head < 0 || g_hn[t].nargs == 0) {
    if (*nv >= HORN_VSPACE) return -1;
    return horn_node(-(++*nv), 0, NULL);
  }
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_cd_skel(HARG(t, i), nv);
    if (args[i] < 0) return -1;
  }
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

static void horn_cd_reso_build(void) {
  g_cd_reso_n = 0;
  for (int g = 0; g < g_hg_n; g++) {
    int stack[512], sp = 0;
    stack[sp++] = g_hg_l[g]; stack[sp++] = g_hg_r[g];
    while (sp > 0 && g_cd_reso_n < CD_RESO_MAX) {
      int t = stack[--sp];
      if (g_hn[t].head >= 0)
        for (int i = 0; i < g_hn[t].nargs && sp < 500; i++)
          stack[sp++] = HARG(t, i);
      if (horn_cd_skel_rigids(t) < CD_RESO_MIN_RIGID) continue;
      int nv = 0;
      int p = horn_cd_skel(t, &nv);
      if (p < 0) continue;
      int dup = 0;
      for (int i = 0; i < g_cd_reso_n && !dup; i++)
        dup = horn_eq(g_cd_reso[i], p);
      if (!dup) g_cd_reso[g_cd_reso_n++] = p;
    }
  }
}

// Does either side of the release structurally refine a goal skeleton?
// (Strict one-way match: pattern wildcards are only the skeleton's own
// leaf variables.  A linked-instance-variable tolerance was tried and
// measured to flood -- 395k of 400k releases "resonant" on LCL195-10,
// whose 1.6-family goal shape saturates the whole detachment space.)
static int horn_cd_resonant(int l, int r) {
  int hit = 0;
  for (int i = 0; i < g_cd_reso_n && !hit; i++) {
    for (int side = 0; side < 2 && !hit; side++) {
      int t = side ? r : l;
      int mark = g_htrail_n;
      hit = horn_cd_match(g_cd_reso[i], HORN_SG_OFF, t, 0);
      horn_undo(mark);
    }
  }
  return hit;
}

// Lazy back subsumption at pop time: only facts added AFTER f can
// subsume it (older generals rejected f at its own emit).
static int horn_cd_pop_subsumed(int f) {
  if (g_dtn_n == 0) return 0;
  g_dt_flat_n = 0; g_dt_over = 0;
  dt_flatten(g_hf[f].l);
  if (g_dt_over) return 0;
  return dt_probe(0, 0, g_hf[f].l, g_hf[f].r, g_hf[f].size, f + 1);
}

// Min-heaps over unprocessed fact indices, keyed by term size
// (smallest first, ties by insertion order).  g_hheap is the main
// given-fact agenda; g_hpheap is the goal-directed priority agenda
// (facts that unify with a backward-cone subgoal), popped first.
typedef struct { int *a; int n, cap; } HHeap;
static HHeap g_hheap  = {NULL, 0, 0};
static HHeap g_hpheap = {NULL, 0, 0};
static int horn_heap_lt(int a, int b) {
  if (g_hf[a].size != g_hf[b].size) return g_hf[a].size < g_hf[b].size;
  return a < b;
}
static void horn_heap_push(HHeap *h, int f) {
  if (h->n >= h->cap) {
    h->cap = (h->cap == 0) ? (1 << 12) : h->cap * 2;
    h->a = (int *)realloc(h->a, (size_t)h->cap * sizeof(int));
    if (h->a == NULL) tp_giveup("OOM in Horn release");
  }
  int i = h->n++;
  h->a[i] = f;
  while (i > 0) {
    int p = (i - 1) / 2;
    if (!horn_heap_lt(h->a[i], h->a[p])) break;
    int t = h->a[i]; h->a[i] = h->a[p]; h->a[p] = t;
    i = p;
  }
}
static int horn_heap_pop(HHeap *h) {
  if (h->n == 0) return -1;
  int top = h->a[0];
  h->a[0] = h->a[--h->n];
  int i = 0;
  for (;;) {
    int l = 2 * i + 1, r = l + 1, m = i;
    if (l < h->n && horn_heap_lt(h->a[l], h->a[m])) m = l;
    if (r < h->n && horn_heap_lt(h->a[r], h->a[m])) m = r;
    if (m == i) break;
    int t = h->a[i]; h->a[i] = h->a[m]; h->a[m] = t;
    i = m;
  }
  return top;
}

// Head index: per symbol, a chain of (fact, orient) entries; orient 0
// matches conditions against (l, r), orient 1 against (r, l).  A fact
// side headed by a variable goes on the var-head chain (matches any
// condition head).
typedef struct { int fact, orient, next; } HIdxE;
static HIdxE *g_hix = NULL; static int g_hix_n = 0, g_hix_cap = 0;
static int *g_hix_head = NULL;   // per symbol id
static int g_hix_varhead = -1;

static void horn_index_entry(int head, int fact, int orient) {
  if (g_hix_n >= g_hix_cap) {
    g_hix_cap = (g_hix_cap == 0) ? (1 << 14) : g_hix_cap * 2;
    g_hix = (HIdxE *)realloc(g_hix, (size_t)g_hix_cap * sizeof(HIdxE));
    if (g_hix == NULL) tp_giveup("OOM in Horn release");
  }
  HIdxE *e = &g_hix[g_hix_n];
  e->fact = fact; e->orient = orient;
  if (head < 0) { e->next = g_hix_varhead; g_hix_varhead = g_hix_n; }
  else          { e->next = g_hix_head[head]; g_hix_head[head] = g_hix_n; }
  g_hix_n++;
}

static void horn_fact_add(int l, int r, int nv) {
  if (g_hf_n >= g_hf_cap) {
    g_hf_cap = (g_hf_cap == 0) ? (1 << 12) : g_hf_cap * 2;
    g_hf = (HFact *)realloc(g_hf, (size_t)g_hf_cap * sizeof(HFact));
    if (g_hf == NULL) tp_giveup("OOM in Horn release");
  }
  // CD lane: index the new fact as a subsumption pattern (both sides).
  if (g_horn_cd && nv > 0) {
    dt_insert(l, g_hf_n, 0);
    dt_insert(r, g_hf_n, 1);
  }
  g_hf[g_hf_n].l = l; g_hf[g_hf_n].r = r; g_hf[g_hf_n].nv = nv;
  g_hf[g_hf_n].src = HORN_SRC_NONE;
  g_hf[g_hf_n].size = horn_size(l) + horn_size(r);
  g_hf[g_hf_n].processed = 0;
  g_hf[g_hf_n].sghit = 0;
  g_hf[g_hf_n].rel = 1;
  g_hf[g_hf_n].conj = 0;
  g_hf[g_hf_n].given = 0;
  g_hf[g_hf_n].dead = 0;
  horn_index_entry(g_hn[l].head, g_hf_n, 0);
  horn_index_entry(g_hn[r].head, g_hf_n, 1);
  horn_heap_push(&g_hheap, g_hf_n);
  g_hf_n++;
}

typedef struct {
  int cl[HORN_MAX_COND], ct[HORN_MAX_COND];
  int nc, concl_l, concl_r, nv;
  int hpc;                // --proof: g_hpc snapshot of the rule clause (-1 off)
  u8 rel, conj, ghrel;    // ghrel: admitted by the goalward head closure
} HRule;
static HRule *g_hr = NULL; static int g_hr_n = 0, g_hr_cap = 0;

// Rule-condition index: (rule, condition position) entries bucketed by
// the condition's head symbol, so a given fact only visits the rules
// that can bind it (the CSR knowledge bases carry ~5k rules; scanning
// them all per given burns the whole budget before the goal chain).
typedef struct { int rule, cpos, next; } HRIdxE;
static HRIdxE *g_hrix = NULL; static int g_hrix_n = 0, g_hrix_cap = 0;
static int *g_hrix_head = NULL;   // per symbol id
static int g_hrix_varhead = -1;

static void horn_rule_index(int rule, int cpos, int head) {
  if (g_hrix_n >= g_hrix_cap) {
    g_hrix_cap = (g_hrix_cap == 0) ? (1 << 12) : g_hrix_cap * 2;
    g_hrix = (HRIdxE *)realloc(g_hrix, (size_t)g_hrix_cap * sizeof(HRIdxE));
    if (g_hrix == NULL) tp_giveup("OOM in Horn release");
  }
  HRIdxE *e = &g_hrix[g_hrix_n];
  e->rule = rule; e->cpos = cpos;
  if (head < 0) { e->next = g_hrix_varhead; g_hrix_varhead = g_hrix_n; }
  else          { e->next = g_hrix_head[head]; g_hrix_head[head] = g_hrix_n; }
  g_hrix_n++;
}

// Pending releases (kept as strings for ranking + TpClause append;
// tl/tr keep the arena terms for the goal-subsumption rank check;
// n_weak counts single-occurrence variables -- weakening instances
// like or(V, lemma) rank below their workhorse cores).
typedef struct {
  char *l, *r;
  int tl, tr;
  int goal_hit, sg_hit, goal_syms, ground, n_weak, size, order;
  int grp, w;   // head-symbol group + within-group rank (diversity quota)
  int dead;     // back-subsumed (CD lane): never appended to the engine
  int prov;     // --proof: g_hpr derivation record (-1 = none/failed)
} HRel;
static HRel *g_hrel = NULL; static int g_hrel_n = 0, g_hrel_cap = 0;

static u8 *g_horn_disp = NULL;   // per symbol id: 1 = dispatcher functor

// --- definitional expansion --------------------------------------------
// A unit fact f(X1..Xk) = r (k >= 1, pairwise-distinct variable
// arguments, f absent from r, every variable of r among the arguments)
// DEFINES f.  The chaining universe is normalized by expanding every
// defined symbol, so unification sees a single alphabet -- e.g. the
// LCL encodings state detachment over implies(..) but their axioms
// interleave it with or/not through implies(X,Y) = or(not(X),Y).
// Expansion preserves equational consequence, so releases stay sound.
static int *g_hdef_l = NULL, *g_hdef_r = NULL;   // per symbol: node or -1
static int *g_hdef_cls = NULL;                   // per symbol: defining clause
static int g_hexp_fail = 0, g_hexp_limit = 0;

// --- release provenance capture (PROV_HORN; --proof runs only) ---------
// Each accepted release records the positive hyper-resolution step that
// produced it: the rule clause, the fact bound at every condition (an
// input clause or a prior release), and the definitional-expansion
// axioms used to normalize those parents into the chaining alphabet.
// The proof emitter materializes the cited cone as cnf(h<k>, ...,
// inference(hyper_resolution, ...)) records, so a released unit the
// engine refutation cites is a derived record with problem-formula
// leaves instead of a fake file() leaf (CASC-J13).  Cap-bounded like
// the release pool: past the caps a release carries a TRUNCATED record
// and is excluded from --proof engine input (the pre-provenance
// behavior for that unit) -- never misrepresented.
#define HORN_PROV_DEFS     12         // def parents per clause / per step
#define HORN_PROV_MAX_REL  (1 << 20)  // recorded releases
#define HORN_PROV_MAX_POOL (1 << 23)  // parent-pool ints
typedef struct {   // snapshot of an input clause the chaining used
  char *l, *r;
  int  conj, kind, ix;               // TpClause role + its own provenance
  int  nd, defs[HORN_PROV_DEFS];     // g_hpc defs its normalization expanded
  u8   trunc, emitted;
} HProvCls;
static HProvCls *g_hpc = NULL; static int g_hpc_n = 0, g_hpc_cap = 0;
static int *g_hpc_of = NULL;    // per input-clause index (horn-pass lifetime)
// Dispatcher-clause snapshots (ifeqN(X,X,Y[,Z]) = Y): every decoded
// hyper-resolution step uses the reducer implicitly, so every emitted
// h-record must CITE it -- without it the released unit is not
// entailed by its listed parents and the checker abstains (the
// PLA/PUZ/GRP-10 empty-then-unverifiable class).
static int g_horn_disp_hpc[4]; static int g_horn_disp_hpc_n = 0;
typedef struct {   // one release's hyper-resolution step
  char *l, *r;                       // released unit (ownership taken
                                     // from the HRel strings at cleanup)
  int rule;                          // g_hpc index of the rule clause
  int po;                            // offset into g_hpp
  u16 nf, nd;                        // fact parents / def parents
  u8  trunc, emitted;
} HPr;
static HPr *g_hpr = NULL; static int g_hpr_n = 0, g_hpr_cap = 0;
// Pool layout per release: nf fact refs (>= 0 = release index, < 0 =
// ~g_hpc index) then nd g_hpc def indices.
static int *g_hpp = NULL; static int g_hpp_n = 0, g_hpp_cap = 0;

// Defs touched by the current clause normalization (clause indices).
static int g_hdt[HORN_PROV_DEFS];
static int g_hdt_n = 0, g_hdt_over = 0;

static void horn_prov_touch_reset(void) { g_hdt_n = 0; g_hdt_over = 0; }

static void horn_prov_def_touch(int f) {
  int c = (g_hdef_cls != NULL) ? g_hdef_cls[f] : -1;
  if (c < 0) { g_hdt_over = 1; return; }
  for (int i = 0; i < g_hdt_n; i++) if (g_hdt[i] == c) return;
  if (g_hdt_n >= HORN_PROV_DEFS) { g_hdt_over = 1; return; }
  g_hdt[g_hdt_n++] = c;
}

// Intern input clause c into the snapshot table; -1 on failure.  The
// snapshot is taken NOW because SInE compacts tp->cls after the pass.
static int horn_prov_cls(const TpProblem *tp, int c) {
  if (!g_prov_on || g_hpc_of == NULL || c < 0 || c >= tp->n_cls) return -1;
  if (g_hpc_of[c] >= 0) return g_hpc_of[c];
  if (g_hpc_n >= g_hpc_cap) {
    g_hpc_cap = (g_hpc_cap == 0) ? 256 : g_hpc_cap * 2;
    g_hpc = (HProvCls *)realloc(g_hpc, (size_t)g_hpc_cap * sizeof(HProvCls));
    if (g_hpc == NULL) tp_giveup("OOM in Horn release");
  }
  HProvCls *pc = &g_hpc[g_hpc_n];
  memset(pc, 0, sizeof *pc);
  pc->l = strdup(tp->cls[c].lhs);
  pc->r = strdup(tp->cls[c].rhs);
  pc->conj = tp->cls[c].conj;
  pc->kind = tp->cls[c].prov_kind;
  pc->ix   = tp->cls[c].prov_ix;
  if (pc->l == NULL || pc->r == NULL) pc->trunc = 1;
  g_hpc_of[c] = g_hpc_n;
  return g_hpc_n++;
}

// Attach the currently-touched defs to snapshot j (the defs expanded
// while normalizing that clause into the chaining alphabet).
static void horn_prov_cls_defs(const TpProblem *tp, int j) {
  if (j < 0 || j >= g_hpc_n) return;
  HProvCls *pc = &g_hpc[j];
  if (g_hdt_over) { pc->trunc = 1; return; }
  for (int i = 0; i < g_hdt_n; i++) {
    int dj = horn_prov_cls(tp, g_hdt[i]);
    if (dj < 0 || g_hpc[dj].trunc) { pc->trunc = 1; return; }
    int dup = 0;
    for (int k = 0; k < pc->nd; k++) dup |= (pc->defs[k] == dj);
    if (dup) continue;
    if (pc->nd >= HORN_PROV_DEFS) { pc->trunc = 1; return; }
    pc->defs[pc->nd++] = dj;
  }
}

// Is release k's derivation record complete?
static int horn_prov_ok(int k) {
  return g_hpr != NULL && k >= 0 && k < g_hpr_n && !g_hpr[k].trunc;
}

static int horn_sym_occurs(int f, int t) {
  if (g_hn[t].head == f) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_sym_occurs(f, HARG(t, i))) return 1;
  return 0;
}

static int horn_vars_within(int t, const u8 *allowed) {
  if (g_hn[t].head < 0) return allowed[-g_hn[t].head - 1];
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (!horn_vars_within(HARG(t, i), allowed)) return 0;
  return 1;
}

static void horn_def_note(int l, int r, int cls) {
  if (g_hn[l].head < 0 || g_hn[l].nargs < 1) return;
  int f = g_hn[l].head;
  if (g_horn_disp[f] || g_hdef_r[f] >= 0) return;
  u8 seen[HORN_VSPACE];
  memset(seen, 0, sizeof seen);
  for (int i = 0; i < g_hn[l].nargs; i++) {
    int a = HARG(l, i);
    if (g_hn[a].head >= 0) return;
    int v = -g_hn[a].head - 1;
    if (seen[v]) return;
    seen[v] = 1;
  }
  if (horn_sym_occurs(f, r)) return;
  if (!horn_vars_within(r, seen)) return;
  g_hdef_l[f] = l; g_hdef_r[f] = r;
  if (g_hdef_cls != NULL) g_hdef_cls[f] = cls;
}

static int horn_subst_build(int t, const int *m, int depth) {
  if (depth > 200 || g_hexp_fail || g_hn_n > g_hexp_limit) { g_hexp_fail = 1; return t; }
  if (g_hn[t].head < 0) return m[-g_hn[t].head - 1];
  int args[16];
  for (int i = 0; i < g_hn[t].nargs; i++)
    args[i] = horn_subst_build(HARG(t, i), m, depth + 1);
  if (g_hexp_fail) return t;
  return horn_node(g_hn[t].head, g_hn[t].nargs, args);
}

static int horn_expand(int t, int depth) {
  if (depth > 64 || g_hexp_fail || g_hn_n > g_hexp_limit) { g_hexp_fail = 1; return t; }
  if (g_hn[t].head < 0) return t;
  int args[16], changed = 0;
  for (int i = 0; i < g_hn[t].nargs; i++) {
    args[i] = horn_expand(HARG(t, i), depth + 1);
    changed |= (args[i] != HARG(t, i));
  }
  if (g_hexp_fail) return t;
  int f = g_hn[t].head;
  if (g_hdef_r != NULL && g_hdef_r[f] >= 0) {
    if (g_prov_on) horn_prov_def_touch(f);
    int m[HORN_VSPACE];
    int L = g_hdef_l[f];
    for (int i = 0; i < g_hn[L].nargs; i++)
      m[-g_hn[HARG(L, i)].head - 1] = args[i];
    int e = horn_subst_build(g_hdef_r[f], m, depth + 1);
    if (g_hexp_fail) return t;
    return horn_expand(e, depth + 1);   // chained definitions
  }
  if (!changed) return t;
  return horn_node(f, g_hn[t].nargs, args);
}

// Normalize with fallback: an expansion failure (cyclic definitions,
// blow-up) rolls the arena back and keeps the original term -- either
// form is sound.
static int horn_norm(int t) {
  if (g_hdef_r == NULL) return t;
  g_hexp_fail = 0;
  int mark_n = g_hn_n, mark_a = g_ha_n;
  g_hexp_limit = g_hn_n + 4000;
  int e = horn_expand(t, 0);
  if (g_hexp_fail) { g_hn_n = mark_n; g_ha_n = mark_a; return t; }
  return e;
}

// --- dispatcher / conditional-clause shape checks -----------------------
// ifeqN(X,X,Y[,Z]) = Y with every argument a variable and the other
// side the bare variable Y (either clause orientation).
static int horn_disp_check(int s, int other) {
  if (g_hn[s].head < 0 || g_hn[other].head >= 0) return 0;
  int na = g_hn[s].nargs;
  if (na != 3 && na != 4) return 0;
  for (int i = 0; i < na; i++)
    if (g_hn[HARG(s, i)].head >= 0) return 0;
  int a0 = g_hn[HARG(s, 0)].head, a1 = g_hn[HARG(s, 1)].head;
  int a2 = g_hn[HARG(s, 2)].head, ov = g_hn[other].head;
  if (a0 != a1 || a2 != ov || a2 == a0) return 0;
  return 1;
}

// Push condition (cl, ct), splitting PAIRED conditions first: the CSR
// consistency KBs pack conjunctions as tuple2(p,q) = tuple2(true,true),
// which no unit fact can ever unify.  When both sides carry the same
// rigid head and arity, the argument equations are pushed instead --
// they IMPLY the packed equation by congruence (then the dispatcher
// fires), so the decoded rule stays a sound consequence.  Returns 0 on
// condition-count overflow.
static int horn_cond_push(HRule *ru, int *nc, int cl, int ct, int depth) {
  if (depth < 8 && g_hn[cl].head >= 0 && g_hn[cl].head == g_hn[ct].head &&
      g_hn[cl].nargs == g_hn[ct].nargs && g_hn[cl].nargs > 0) {
    for (int i = 0; i < g_hn[cl].nargs; i++)
      if (!horn_cond_push(ru, nc, HARG(cl, i), HARG(ct, i), depth + 1))
        return 0;
    return 1;
  }
  if (*nc >= HORN_MAX_COND) return 0;
  ru->cl[*nc] = cl;
  ru->ct[*nc] = ct;
  (*nc)++;
  return 1;
}

// Unwrap ifeqN(c, t, then, e) = e (else side must equal the clause's
// other side for arity 4; arity 3 has no else).  Nesting in the
// then-position accumulates conditions; a partial unwrap is still a
// sound rule (the residual term is the conclusion).
static int horn_decode_side(int s, int other, HRule *ru) {
  int nc = 0, cur = s;
  for (;;) {
    if (g_hn[cur].head < 0 || !g_horn_disp[g_hn[cur].head]) break;
    int na = g_hn[cur].nargs;
    if (na != 3 && na != 4) break;
    if (na == 4 && !horn_eq(HARG(cur, 3), other)) break;
    if (!horn_cond_push(ru, &nc, HARG(cur, 0), HARG(cur, 1), 0)) return 0;
    cur = HARG(cur, 2);
  }
  if (nc == 0) return 0;
  ru->nc = nc; ru->concl_l = cur; ru->concl_r = other;
  return 1;
}

// --- the forward-chaining join ------------------------------------------
static int g_horn_stop = 0;
// The release budget g_horn_wall and every fraction of it -- the phase-2
// trigger, the backward cone's slice, the gate relax, CD deepening, the
// backward CD lane -- run on the work clock (cli_work_now): g_horn_t0 is
// the release start there, g_horn_t0_wall on the wall (reports only).
static double g_horn_t0 = 0.0;
static double g_horn_t0_wall = 0.0;
static double g_horn_wall = HORN_WALL_SEC;      // THVM_ATP_HORN_WALL
// Absolute wall-clock stop deadline (process-start + slice wall), set in
// tp_horn_release.  Independent of g_horn_wall (which is release-start-
// relative and shapes the search): the STOP polls trip on EITHER, so a
// late-starting release self-terminates at the slice wall without the
// gate-relax/phase-2 timing (fractions of g_horn_wall) shifting.  0 = off.
static double g_horn_abs_deadline = 0.0;
static int g_horn_max_rel = HORN_MAX_RELEASES;  // THVM_ATP_HORN_MAX
// Attempt budget: the CD lane runs wall-governed (its subsumed space
// stays small), every other flavor keeps the fixed v1 cap.  Same for
// the arena-node ceiling: a wall-long CD run holds millions of
// accepted facts live.
static long g_horn_max_attempts = HORN_MAX_ATTEMPTS;
static int  g_horn_max_nodes = (1 << 22);
static double horn_now(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}
// The search-shaping budgets read horn_budget_now, the work clock.  A
// --proof run captures its derivation records inside work-clock
// exclusion windows (atp_work_pause / atp_work_resume), so it releases
// what the no-proof run releases in its budget; the absolute slice
// deadline stays on the wall clock.
static inline double horn_budget_now(void) {
  return cli_work_now();
}
// STOP-condition test: the release-relative budget (search-shaping, work
// clock) OR the absolute slice deadline (overrun guard, wall clock),
// whichever comes first.  Used only where the loop would set
// g_horn_stop; the search-shaping deadlines (phase-2 trigger, gate-relax
// cones) keep the bare g_horn_wall form so a release that finishes
// before the wall stays byte-identical.
static inline int horn_wall_over(void) {
  if (horn_budget_now() - g_horn_t0 > g_horn_wall) return 1;
  if (g_horn_abs_deadline > 0.0 && horn_now() > g_horn_abs_deadline) return 1;
  return 0;
}

// --- goal-directed backward subgoal cone (v2) ---------------------------
// SLD/UR-style regression from the negated goals through the decoded
// rules, bounded by depth, count and a wall slice.  A subgoal is an
// equation PATTERN: any released fact that unifies with one is a
// candidate step of a goal-closing chain, so it is (a) popped from the
// agenda with priority -- its forward joins produce the parent
// subgoal's instance next -- and (b) ranked into the kept window.
// Subgoals are relevance markers only, never released as equations, so
// soundness is untouched.
//
// Regression steps, for a subgoal G and rule C1..Ck -> L=R with the
// conclusion unified against G (either orientation):
//   - abstract:  each condition Ci becomes a subgoal under the
//     conclusion bindings alone (SLD with the siblings postponed);
//     rejected unless enough rigid structure survives to discriminate.
//   - UR-style:  all conditions but one are satisfied against the
//     CURRENT fact set (input units; the cone runs before the loop),
//     and the open condition becomes the subgoal -- the classic
//     unit-resulting negative-unit step, fully constrained.
typedef struct { int l, r, depth, hl, hr, hits; } HGoalPat;
static HGoalPat *g_hsg = NULL; static int g_hsg_n = 0, g_hsg_cap = 0;
static int g_hsg_depth_cap = 6;             // THVM_ATP_HORN_BWD_DEPTH
static int g_hsg_max = 512;                 // THVM_ATP_HORN_BWD_MAX
static int g_hsg_stop = 0;
static double g_hsg_deadline = 0.0;
static long g_hsg_checks = 0;               // release-vs-subgoal unify budget
#define HORN_SG_BUDGET 20000000L
#define HORN_SG_MIN_RIGID 3
// Steering is AUTO-GATED (g_horn_steer): priority pops run only when
// the goal is existential (a reachability/planning query -- PUZ
// states, PLA plans -- where only regression can carve a path through
// an astronomical forward space) or the relevance gate is active (CSR
// KBs).  For ground saturation-style goals (the LCL condensed-
// detachment family) the baseline agenda already reaches goals on a
// razor-thin trajectory, and ANY dilution of its pop order loses them
// (LCL192/LCL211 measured); there the cone is skipped entirely and
// only the direct goal probe runs.  THVM_ATP_HORN_BWD: 0=off, 2=force
// steering, default auto.
static int g_horn_steer = 0;
static int g_horn_goal_stop = 1;            // THVM_ATP_HORN_GOAL_STOP
static int g_horn_goal_reached = 0;

static int horn_rigid_count(int t) {
  int n = (g_hn[t].head >= 0);
  for (int i = 0; i < g_hn[t].nargs; i++) n += horn_rigid_count(HARG(t, i));
  return n;
}

static int horn_has_var(int t) {
  if (g_hn[t].head < 0) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_has_var(HARG(t, i))) return 1;
  return 0;
}

// Canonicalize (cl, ct) under the live bindings into a new subgoal;
// dedup ("?"-prefixed keys share the release dedup set), reject
// under-constrained patterns (too little rigid structure to steer).
static void horn_bwd_emit(const TpProblem *tp, int cl, int ct, int depth) {
  if (g_hsg_n >= g_hsg_max) { g_hsg_stop = 1; return; }
  int hn_mark = g_hn_n, ha_mark = g_ha_n;
  horn_can_reset();
  int budget = HORN_APPLY_MAX;
  int l2 = horn_apply(cl, 0, &budget);
  int r2 = (l2 >= 0) ? horn_apply(ct, 0, &budget) : -1;
  if (l2 < 0 || r2 < 0) goto reject;
  // Specificity guard: a pattern needs enough rigid structure to
  // discriminate -- except a GROUND pattern, which is maximally
  // specific however small (a CSR goal is a bare `a = b`).
  if (g_can_nv > 0 &&
      horn_rigid_count(l2) + horn_rigid_count(r2) < HORN_SG_MIN_RIGID)
    goto reject;
  {
    static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 3];
    size_t lo = 0, ro = 0;
    int remap[HORN_REL_VARS], nv = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
    if (!horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv)) goto reject;
    if (!horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv)) goto reject;
    snprintf(key, sizeof key, "?%s=%s", lb, rb);
    if (!horn_dedup_add(key)) goto reject;
    size_t fo = 0, go = 0;
    int remap2[HORN_REL_VARS], nv2 = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap2[i] = -1;
    if (horn_print(tp, rb, HORN_SIDE_MAX, &fo, r2, remap2, &nv2) &&
        horn_print(tp, lb, HORN_SIDE_MAX, &go, l2, remap2, &nv2)) {
      snprintf(key, sizeof key, "?%s=%s", rb, lb);
      horn_dedup_add(key);
    }
  }
  if (g_hsg_n >= g_hsg_cap) {
    g_hsg_cap = (g_hsg_cap == 0) ? (1 << 9) : g_hsg_cap * 2;
    g_hsg = (HGoalPat *)realloc(g_hsg, (size_t)g_hsg_cap * sizeof(HGoalPat));
    if (g_hsg == NULL) tp_giveup("OOM in Horn release");
  }
  g_hsg[g_hsg_n].l = l2; g_hsg[g_hsg_n].r = r2;
  g_hsg[g_hsg_n].depth = depth;
  g_hsg[g_hsg_n].hl = g_hn[l2].head; g_hsg[g_hsg_n].hr = g_hn[r2].head;
  g_hsg[g_hsg_n].hits = 0;
  g_hsg_n++;
  {
    static int diag = -1;
    if (diag < 0) { const char *d = getenv("THVM_ATP_HORN_DIAG"); diag = (d != NULL && d[0] != '0' && d[0] != '\0'); }
    if (diag && g_hsg_n <= 64) {
      static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
      size_t lo = 0, ro = 0;
      int remap[HORN_REL_VARS], nv = 0;
      for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
      if (horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv) &&
          horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv))
        fprintf(stderr, "[horn-sg %d d%d] %s = %s\n", g_hsg_n - 1, depth, lb, rb);
    }
  }
  return;
reject:
  g_hn_n = hn_mark; g_ha_n = ha_mark;
}

// UR fill: satisfy every condition except `open` from the current fact
// set (no processed filter -- the cone runs before the loop), then emit
// the open condition as a subgoal under the accumulated bindings.
static void horn_bwd_fill(const TpProblem *tp, const HRule *ru, int ci,
                          int open, int depth) {
  if (g_hsg_stop) return;
  while (ci == open) ci++;
  if (ci >= ru->nc) { horn_bwd_emit(tp, ru->cl[open], ru->ct[open], depth); return; }
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > HORN_MAX_ATTEMPTS || horn_budget_now() > g_hsg_deadline ||
       g_hn_n > (1 << 22))) { g_hsg_stop = 1; return; }
  int c = ru->cl[ci], coff = 0;
  horn_deref(&c, &coff);
  int off = (ci + 1) * HORN_VSPACE;
  for (int pass = 0; pass < 2 && !g_hsg_stop; pass++) {
    int e;
    if (pass == 0) e = (g_hn[c].head >= 0) ? g_hix_head[g_hn[c].head] : -2;
    else           e = g_hix_varhead;
    if (e == -2) {
      for (int f = 0; f < g_hf_n && !g_hsg_stop; f++) {
        if (!g_hf[f].rel) continue;   // cone fills stay in the relevant cone
        for (int orient = 0; orient < 2; orient++) {
          int fl = orient ? g_hf[f].r : g_hf[f].l;
          int fr = orient ? g_hf[f].l : g_hf[f].r;
          int mark = g_htrail_n;
          g_horn_attempts++;
          if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
              horn_unify(ru->ct[ci], 0, fr, off, 0))
            horn_bwd_fill(tp, ru, ci + 1, open, depth);
          horn_undo(mark);
        }
      }
      continue;
    }
    for (; e >= 0 && !g_hsg_stop; e = g_hix[e].next) {
      int f = g_hix[e].fact;
      if (!g_hf[f].rel) continue;     // cone fills stay in the relevant cone
      int fl = g_hix[e].orient ? g_hf[f].r : g_hf[f].l;
      int fr = g_hix[e].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      g_horn_attempts++;
      if (horn_unify(ru->cl[ci], 0, fl, off, 0) &&
          horn_unify(ru->ct[ci], 0, fr, off, 0))
        horn_bwd_fill(tp, ru, ci + 1, open, depth);
      horn_undo(mark);
    }
  }
}

// Breadth-first cone: seed from the parsed negative units, regress
// through every rule whose conclusion unifies (either orientation).
static void horn_backward_cone(const TpProblem *tp) {
  for (int g = 0; g < g_hg_n && !g_hsg_stop; g++)
    horn_bwd_emit(tp, g_hg_l[g], g_hg_r[g], 0);
  for (int qi = 0; qi < g_hsg_n && !g_hsg_stop; qi++) {
    if (g_hsg[qi].depth >= g_hsg_depth_cap) continue;
    int sgl = g_hsg[qi].l, sgr = g_hsg[qi].r, d1 = g_hsg[qi].depth + 1;
    for (int r = 0; r < g_hr_n && !g_hsg_stop; r++) {
      const HRule *ru = &g_hr[r];
      if (!ru->rel) continue;   // relevance-gated (oversized KBs)
      int chl = g_hn[ru->concl_l].head, chr = g_hn[ru->concl_r].head;
      for (int orient = 0; orient < 2 && !g_hsg_stop; orient++) {
        int gl = orient ? sgr : sgl, gr = orient ? sgl : sgr;
        if (chl >= 0 && g_hn[gl].head >= 0 && chl != g_hn[gl].head) continue;
        if (chr >= 0 && g_hn[gr].head >= 0 && chr != g_hn[gr].head) continue;
        int mark = g_htrail_n;
        g_horn_attempts++;
        if (horn_unify(ru->concl_l, 0, gl, HORN_SG_OFF, 0) &&
            horn_unify(ru->concl_r, 0, gr, HORN_SG_OFF, 0)) {
          for (int j = 0; j < ru->nc && !g_hsg_stop; j++) {
            horn_bwd_emit(tp, ru->cl[j], ru->ct[j], d1);
            if (ru->nc >= 2) horn_bwd_fill(tp, ru, 0, j, d1);
          }
        }
        horn_undo(mark);
        if ((g_horn_attempts & 0xfff) == 0 && horn_budget_now() > g_hsg_deadline)
          g_hsg_stop = 1;
      }
    }
  }
}

// Does the fact (l, r) unify with some subgoal?  l/r live in a var
// space of their own (canonical release vars, or an input fact's
// clause-local vars): probe space vs subgoal space, both clean even
// while a join's bindings are live in spaces 0..HORN_MAX_COND.
static int horn_sg_probe(int l, int r) {
  if (g_hsg_n == 0 || g_hsg_checks > HORN_SG_BUDGET) return 0;
  if (g_hpheap.n > 4096) return 0;    // steering saturated: stop paying
  int hl = g_hn[l].head, hr = g_hn[r].head;
  for (int s = 0; s < g_hsg_n; s++) {
    for (int orient = 0; orient < 2; orient++) {
      int a = orient ? r : l, b = orient ? l : r;
      int ha = orient ? hr : hl, hb = orient ? hl : hr;
      if (ha >= 0 && g_hsg[s].hl >= 0 && ha != g_hsg[s].hl) continue;
      if (hb >= 0 && g_hsg[s].hr >= 0 && hb != g_hsg[s].hr) continue;
      int mark = g_htrail_n;
      g_hsg_checks++;
      int hit = horn_unify(a, HORN_PROBE_OFF, g_hsg[s].l, HORN_SG_OFF, 0) &&
                horn_unify(b, HORN_PROBE_OFF, g_hsg[s].r, HORN_SG_OFF, 0);
      horn_undo(mark);
      if (hit) { g_hsg[s].hits++; return 1; }   // hits: diagnostics only
    }
  }
  return 0;
}

// --- SInE-lite relevance gate for oversized KBs (v2) --------------------
// The CSR knowledge bases carry 20-40k clauses; a blind given-fact loop
// burns the whole attempt budget on ontology noise and never touches
// the conjecture's neighborhood.  Compute Hoder-Voronkov relevance over
// the DECODED universe (facts + rules), seeded from the conjecture
// clauses: an unreached fact is pre-marked processed -- it stays a
// passive join partner but is never popped as given -- and an
// unreached rule neither joins givens nor hosts cone regressions.
// Purely a search-order device: no clause is dropped, releases stay
// sound consequences.
static u32 horn_syms_count(int t, u32 *gen, u32 *stamp, u32 tag) {
  u32 n = 0;
  if (g_hn[t].head >= 0) {
    if (stamp[g_hn[t].head] != tag) { stamp[g_hn[t].head] = tag; gen[g_hn[t].head]++; n++; }
    for (int i = 0; i < g_hn[t].nargs; i++)
      n += horn_syms_count(HARG(t, i), gen, stamp, tag);
  }
  return n;
}
static u32 horn_syms_min(int t, const u32 *gen) {
  u32 m = 0xffffffffu;
  if (g_hn[t].head >= 0) {
    m = gen[g_hn[t].head];
    for (int i = 0; i < g_hn[t].nargs; i++) {
      u32 c = horn_syms_min(HARG(t, i), gen);
      if (c < m) m = c;
    }
  }
  return m;
}
static int horn_syms_trig(int t, const u32 *gen, const u8 *reach, double lim) {
  if (g_hn[t].head < 0) return 0;
  if (reach[g_hn[t].head] && (double)gen[g_hn[t].head] <= lim) return 1;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (horn_syms_trig(HARG(t, i), gen, reach, lim)) return 1;
  return 0;
}
static void horn_syms_reach(int t, u8 *reach) {
  if (g_hn[t].head < 0) return;
  reach[g_hn[t].head] = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) horn_syms_reach(HARG(t, i), reach);
}

static double g_horn_gate_tol = 1.5;
static int    g_horn_gate_on  = 0;
static int    g_horn_input_nf = 0;   // facts below this index are input units

// Recomputable: a WIDER tol yields a superset of relevant clauses, so
// the fixpoint loop can relax the gate iteratively.  Facts that become
// relevant after being parked as passive re-enter the agenda.
// admit_gh: also activate the goalward-head-closure rules (see below)
// -- deferred to the SECOND phase of the chaining wall, because from
// the start the ~19k admitted rules dilute a focused agenda into a
// release flood (CSR036: goal reached at 24k releases under the 511
// generality-gated rules, buried past 291k with the closure active).
static void horn_relevance_gate(const TpProblem *tp, double tol,
                                int admit_gh) {
  u32 *gen   = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
  u32 *stamp = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
  u8  *reach = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
  if (gen == NULL || stamp == NULL || reach == NULL)
    tp_giveup("OOM in Horn release");
  u32 tag = 0;
  for (int f = 0; f < g_hf_n; f++) {
    tag++;
    horn_syms_count(g_hf[f].l, gen, stamp, tag);
    horn_syms_count(g_hf[f].r, gen, stamp, tag);
    // Releases (f >= the input-fact watermark) descend from relevant
    // chains: always active.
    g_hf[f].rel = (f >= g_horn_input_nf) ? 1 : g_hf[f].conj;
  }
  for (int r = 0; r < g_hr_n; r++) {
    tag++;
    for (int j = 0; j < g_hr[r].nc; j++) {
      horn_syms_count(g_hr[r].cl[j], gen, stamp, tag);
      horn_syms_count(g_hr[r].ct[j], gen, stamp, tag);
    }
    horn_syms_count(g_hr[r].concl_l, gen, stamp, tag);
    horn_syms_count(g_hr[r].concl_r, gen, stamp, tag);
    g_hr[r].rel = g_hr[r].conj;
  }
  for (int g = 0; g < g_hg_n; g++) {
    horn_syms_reach(g_hg_l[g], reach);
    horn_syms_reach(g_hg_r[g], reach);
  }
  for (int f = 0; f < g_hf_n; f++)
    if (g_hf[f].conj) { horn_syms_reach(g_hf[f].l, reach); horn_syms_reach(g_hf[f].r, reach); }
  for (int r = 0; r < g_hr_n; r++)
    if (g_hr[r].conj) {
      for (int j = 0; j < g_hr[r].nc; j++) {
        horn_syms_reach(g_hr[r].cl[j], reach);
        horn_syms_reach(g_hr[r].ct[j], reach);
      }
      horn_syms_reach(g_hr[r].concl_l, reach);
      horn_syms_reach(g_hr[r].concl_r, reach);
    }
  // Goalward head closure (magic-set shape, RULES only): admit every
  // rule whose conclusion's root predicate can feed a goal-relevant
  // predicate, walking conclusion -> condition heads backward to the
  // fixpoint.  Generality triggering alone cannot see definition-
  // shaped bridges: CSR040's proof crosses
  //   firstordercollection (gen 244) -> fixedordercollection (gen 9),
  // which needs tol >= gen(trigger)/min_gen = 27 -- a tolerance that
  // floods everything else long before the relax ladder reaches it.
  // Facts stay generality-gated (they ARE the agenda; admitting rules
  // is join-time free: a rule only fires when an ACTIVE given matches
  // one of its condition heads through the rule index).  Encoding
  // scaffolding (dispatcher functors, true/false) never carries
  // goal-relevance between rules: every conclusion is `p(...) = true`
  // shaped, so a `true` head would relate ALL rules.
  {
    u8 *gh = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
    if (gh == NULL) tp_giveup("OOM in Horn release");
    #define HORN_GH_ADD(t) do {                                        \
      int h_ = g_hn[(t)].head;                                         \
      if (h_ >= 0 && !g_horn_disp[h_] &&                               \
          strcmp(tp->syms[h_].name, "true") != 0 &&                    \
          strcmp(tp->syms[h_].name, "false") != 0) gh[h_] = 1;         \
    } while (0)
    for (int g = 0; g < g_hg_n; g++) {
      HORN_GH_ADD(g_hg_l[g]);
      HORN_GH_ADD(g_hg_r[g]);
    }
    for (int f = 0; f < g_hf_n; f++)
      if (g_hf[f].conj) { HORN_GH_ADD(g_hf[f].l); HORN_GH_ADD(g_hf[f].r); }
    for (int r = 0; r < g_hr_n; r++)
      if (g_hr[r].conj)
        for (int j = 0; j < g_hr[r].nc; j++) {
          HORN_GH_ADD(g_hr[r].cl[j]);
          HORN_GH_ADD(g_hr[r].ct[j]);
        }
    for (;;) {
      int grew = 0;
      for (int r = 0; r < g_hr_n; r++) {
        if (g_hr[r].ghrel) continue;
        int hl = g_hn[g_hr[r].concl_l].head, hr = g_hn[g_hr[r].concl_r].head;
        if (!((hl >= 0 && gh[hl]) || (hr >= 0 && gh[hr]))) continue;
        g_hr[r].ghrel = 1;
        grew = 1;
        for (int j = 0; j < g_hr[r].nc; j++) {
          HORN_GH_ADD(g_hr[r].cl[j]);
          HORN_GH_ADD(g_hr[r].ct[j]);
        }
      }
      if (!grew) break;
    }
    #undef HORN_GH_ADD
    int nghr = 0;
    for (int r = 0; r < g_hr_n; r++)
      if (g_hr[r].ghrel) {
        if (admit_gh) g_hr[r].rel = 1;
        nghr++;
      }
    fprintf(stderr, "FindProof: Horn goalward head closure: %d rule(s)%s\n",
            nghr, admit_gh ? " (active)" : " (phase 2)");
    free(gh);
  }
  for (int rd = 0; rd < 24; rd++) {
    int grew = 0;
    for (int f = 0; f < g_hf_n; f++) {
      if (g_hf[f].rel) continue;
      u32 m = horn_syms_min(g_hf[f].l, gen);
      u32 m2 = horn_syms_min(g_hf[f].r, gen);
      if (m2 < m) m = m2;
      double lim = tol * (double)m;
      if (horn_syms_trig(g_hf[f].l, gen, reach, lim) ||
          horn_syms_trig(g_hf[f].r, gen, reach, lim)) {
        g_hf[f].rel = 1; grew = 1;
        horn_syms_reach(g_hf[f].l, reach);
        horn_syms_reach(g_hf[f].r, reach);
      }
    }
    for (int r = 0; r < g_hr_n; r++) {
      if (g_hr[r].rel) continue;
      u32 m = horn_syms_min(g_hr[r].concl_l, gen);
      u32 m2 = horn_syms_min(g_hr[r].concl_r, gen);
      if (m2 < m) m = m2;
      for (int j = 0; j < g_hr[r].nc; j++) {
        u32 c = horn_syms_min(g_hr[r].cl[j], gen);  if (c < m) m = c;
        u32 d = horn_syms_min(g_hr[r].ct[j], gen);  if (d < m) m = d;
      }
      double lim = tol * (double)m;
      int hit = horn_syms_trig(g_hr[r].concl_l, gen, reach, lim) ||
                horn_syms_trig(g_hr[r].concl_r, gen, reach, lim);
      for (int j = 0; j < g_hr[r].nc && !hit; j++)
        hit = horn_syms_trig(g_hr[r].cl[j], gen, reach, lim) ||
              horn_syms_trig(g_hr[r].ct[j], gen, reach, lim);
      if (hit) {
        g_hr[r].rel = 1; grew = 1;
        for (int j = 0; j < g_hr[r].nc; j++) {
          horn_syms_reach(g_hr[r].cl[j], reach);
          horn_syms_reach(g_hr[r].ct[j], reach);
        }
        horn_syms_reach(g_hr[r].concl_l, reach);
        horn_syms_reach(g_hr[r].concl_r, reach);
      }
    }
    if (!grew) break;
  }
  int nf = 0, nr = 0;
  for (int f = 0; f < g_hf_n; f++) {
    if (g_hf[f].rel) {
      nf++;
      if (g_hf[f].processed && !g_hf[f].given) {
        // Parked passive by a tighter gate: back onto the agenda.
        g_hf[f].processed = 0;
        horn_heap_push(&g_hheap, f);
        if (g_hf[f].sghit) horn_heap_push(&g_hpheap, f);
      }
    } else if (!g_hf[f].given) {
      g_hf[f].processed = 1;      // passive: join partner, never given
    }
  }
  for (int r = 0; r < g_hr_n; r++) nr += g_hr[r].rel;
  fprintf(stderr, "FindProof: Horn relevance gate: %d/%d fact(s), "
          "%d/%d rule(s) active (tol=%.2f)\n", nf, g_hf_n, nr, g_hr_n, tol);
  free(gen); free(stamp); free(reach);
}

// Does the fact (l, r) unify with an ORIGINAL goal?  Any common
// instance refutes that negative unit, so the chaining can stop: the
// goal-subsuming release is kept first and the engine closes at once.
static int horn_goal_probe(int l, int r) {
  for (int g = 0; g < g_hg_n; g++) {
    for (int orient = 0; orient < 2; orient++) {
      int a = orient ? r : l, b = orient ? l : r;
      int mark = g_htrail_n;
      int hit = horn_unify(a, HORN_PROBE_OFF, g_hg_l[g], HORN_SG_OFF, 0) &&
                horn_unify(b, HORN_PROBE_OFF, g_hg_r[g], HORN_SG_OFF, 0);
      horn_undo(mark);
      if (hit) return 1;
    }
  }
  return 0;
}

// The fact bound at each rule condition of the join being emitted
// (horn_given_try fills the given's slot, horn_fill the rest) -- the
// hyper-resolution step's fact parents, read at release-record time.
static int g_hpf[HORN_MAX_COND];
static int g_horn_given_ix = -1;

// Append one derivation record: `rule` = g_hpc index of the rule
// clause, fps/nf = the fact parents (>= 0 = g_hpr index, < 0 = ~g_hpc
// index), l/r = the derived unit (ownership passes to the record).
// The record unions in the rule's and every input-fact parent's
// definitional-expansion axioms.  Returns the g_hpr index, or -1 when
// a piece is unavailable and no honest record exists (caps hit,
// truncated ancestor) -- the unit is then PROV_TRUNCATED.
static int horn_prov_new(int rule, const int *fps, int nf, char *l, char *r) {
  if (!g_prov_on || l == NULL || r == NULL) return -1;
  if (g_hpr_n >= HORN_PROV_MAX_REL) return -1;
  if (rule < 0 || rule >= g_hpc_n || g_hpc[rule].trunc) return -1;
  int defs[2 * HORN_PROV_DEFS], ndefs = 0;
  for (int i = 0; i < g_hpc[rule].nd; i++)
    defs[ndefs++] = g_hpc[rule].defs[i];
  for (int ci = 0; ci < nf; ci++) {
    int src = fps[ci];
    if (src == HORN_SRC_NONE) return -1;
    if (src >= 0) {                     // a prior derived unit
      if (!horn_prov_ok(src)) return -1;
    } else {                            // an input clause: union its defs
      int j = ~src;
      if (j >= g_hpc_n || g_hpc[j].trunc) return -1;
      for (int i = 0; i < g_hpc[j].nd; i++) {
        int dj = g_hpc[j].defs[i], dup = 0;
        for (int k = 0; k < ndefs; k++) dup |= (defs[k] == dj);
        if (dup) continue;
        if (ndefs >= (int)(sizeof defs / sizeof defs[0])) return -1;
        defs[ndefs++] = dj;
      }
    }
  }
  if (g_hpp_n + nf + ndefs > HORN_PROV_MAX_POOL) return -1;
  if (g_hpr_n >= g_hpr_cap) {
    g_hpr_cap = (g_hpr_cap == 0) ? (1 << 12) : g_hpr_cap * 2;
    g_hpr = (HPr *)realloc(g_hpr, (size_t)g_hpr_cap * sizeof(HPr));
    if (g_hpr == NULL) tp_giveup("OOM in Horn release");
  }
  if (g_hpp_n + nf + ndefs > g_hpp_cap) {
    g_hpp_cap = (g_hpp_cap == 0) ? (1 << 14) : g_hpp_cap;
    while (g_hpp_cap < g_hpp_n + nf + ndefs) g_hpp_cap *= 2;
    g_hpp = (int *)realloc(g_hpp, (size_t)g_hpp_cap * sizeof(int));
    if (g_hpp == NULL) tp_giveup("OOM in Horn release");
  }
  HPr *p = &g_hpr[g_hpr_n];
  memset(p, 0, sizeof *p);
  p->l = l; p->r = r;
  p->rule = rule;
  p->po = g_hpp_n;
  p->nf = (u16)nf;
  p->nd = (u16)ndefs;
  for (int i = 0; i < nf; i++) g_hpp[g_hpp_n++] = fps[i];
  for (int i = 0; i < ndefs; i++) g_hpp[g_hpp_n++] = defs[i];
  return g_hpr_n++;
}

// Record a forward release's hyper-resolution step (rule + the fact
// bound at every condition, from g_hpf).
static int horn_prov_record(const HRule *ru, const HRel *re) {
  if (!g_prov_on) return -1;
  atp_work_pause();
  int fps[HORN_MAX_COND];
  for (int ci = 0; ci < ru->nc; ci++) {
    int f = g_hpf[ci];
    fps[ci] = (f >= 0 && f < g_hf_n) ? g_hf[f].src : HORN_SRC_NONE;
  }
  int k = horn_prov_new(ru->hpc, fps, ru->nc, re->l, re->r);
  atp_work_resume();
  return k;
}

static void horn_emit_release(TpProblem *tp, const HRule *ru) {
  // Rejected releases (dedup hits, guards) roll their applied terms
  // back out of the arena -- only accepted facts persist.
  int hn_mark = g_hn_n, ha_mark = g_ha_n;
  horn_can_reset();
  int budget = HORN_APPLY_MAX;
  int l2 = horn_apply(ru->concl_l, 0, &budget);
  int r2 = (l2 >= 0) ? horn_apply(ru->concl_r, 0, &budget) : -1;
  if (l2 < 0 || r2 < 0) goto reject;
  if (horn_eq(l2, r2)) goto reject;                  // trivial
  // Collapse guards: a bare-variable side whose variable does not
  // occur in the other side releases a near-degenerate equation.
  if (g_hn[l2].head < 0 || g_hn[r2].head < 0) {
    int v = (g_hn[l2].head < 0) ? l2 : r2, w = (g_hn[l2].head < 0) ? r2 : l2;
    if (g_hn[w].head < 0) goto reject;               // X = Y
    int slot = -g_hn[v].head - 1, hit = 0;
    // canonical terms are offset-free: reuse occurs via a tiny scan
    int stack[512], sp = 0; stack[sp++] = w;
    while (sp > 0 && !hit) {
      int t = stack[--sp];
      if (g_hn[t].head < 0) { if (-g_hn[t].head - 1 == slot) hit = 1; }
      else for (int i = 0; i < g_hn[t].nargs && sp < 512; i++) stack[sp++] = HARG(t, i);
    }
    if (!hit) goto reject;
  }
  // CD lane: the weight cap.  Over-cap conclusions are rejected BEFORE
  // the dedup add, so a later deepening round re-derives and admits
  // them; g_cd_overcap records that the cap (not saturation) pruned.
  // Emissions are wall-checked directly: the subsumption machinery
  // below runs between the join loops' attempt-counter checks, and an
  // emit-heavy stretch must not outrun the budget.
  // CONE EXEMPTION (the resonance idea, with the backward cone as the
  // hint generator): a conclusion that unifies a goal-regression
  // subgoal is a candidate step of a goal-closing chain -- it is kept
  // WHATEVER its size.  The deep PM intermediates (hypothetical
  // syllogism instances run ~25 symbols) sit above any cap the
  // forward space can saturate, but they lie ON the regression cone.
  {
    int cd_size = 0, cd_reso = 0;
    if (g_horn_cd) {
      static int cd_emits = 0;
      if ((++cd_emits & 0xff) == 0 && horn_wall_over()) {
        g_horn_stop = 1;
        goto reject;
      }
      // Otter's max_distinct_vars: the classic CD branching cut.  A
      // conclusion sprouting many distinct variables is a promiscuous
      // joiner whatever its size.
      if (g_can_nv > g_cd_maxvars) goto reject;
      cd_size = horn_size(l2) + horn_size(r2);
      cd_reso = horn_cd_resonant(l2, r2);
      if (cd_size > g_cd_cap && !cd_reso) { g_cd_overcap = 1; goto reject; }
    }
  {
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 2];
  size_t lo = 0, ro = 0;
  int remap[HORN_REL_VARS], nv = 0;
  for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
  if (!horn_print(tp, lb, HORN_SIDE_MAX, &lo, l2, remap, &nv)) goto reject;
  if (!horn_print(tp, rb, HORN_SIDE_MAX, &ro, r2, remap, &nv)) goto reject;
  snprintf(key, sizeof key, "%s=%s", lb, rb);
  if (!horn_dedup_add(key)) goto reject;
  // Suppress the mirror too: print r-then-l under a fresh renumbering
  // and pre-claim that key, so the flipped orientation is never
  // released as a separate unit.
  {
    static char fl[TP_SIDE_LEN], fr[TP_SIDE_LEN];
    size_t fo = 0, go = 0;
    int remap2[HORN_REL_VARS], nv2 = 0;
    for (int i = 0; i < HORN_REL_VARS; i++) remap2[i] = -1;
    if (horn_print(tp, fr, HORN_SIDE_MAX, &fo, r2, remap2, &nv2) &&
        horn_print(tp, fl, HORN_SIDE_MAX, &go, l2, remap2, &nv2)) {
      snprintf(key, sizeof key, "%s=%s", fr, fl);
      horn_dedup_add(key);
    }
  }
  // CD lane: generalization subsumption -- an instance of a live fact
  // (input schema or earlier release) is redundant.  The dedup entry
  // above stays claimed: the candidate is redundant forever (whatever
  // subsumes its subsumer subsumes it).
  if (g_horn_cd && horn_cd_fwd_subsumed(l2, r2, cd_size)) {
    g_cd_fsub++;
    goto reject;
  }
  // New unit: joins the fact set immediately (chains within the round
  // are picked up next round via the semi-naive base) and the pending
  // release list for the final append.  In the CD lane the resonance
  // hit plays the sg-hit role: priority pop + kept-window tier.
  int sgh = g_horn_cd ? cd_reso : horn_sg_probe(l2, r2);
  if (g_horn_cd && cd_reso) {
    g_cd_reso_hits++;
    // A resonator most of the space matches carries no signal -- the
    // goal is shaped like a generic lemma of its calculus (measured:
    // 252k of 255k releases on LCL054-10, whose Frege goal is the
    // generic implication-composition shape).  Drop the resonators and
    // let the pure weight discipline run.
    if (g_cd_reso_hits > 2000 && g_cd_reso_hits * 2 > (long)g_hrel_n) {
      g_cd_reso_n = 0;
      fprintf(stderr, "FindProof: Horn CD: resonators flooded "
              "(%ld of %d released) -- disabled\n",
              g_cd_reso_hits, g_hrel_n);
    }
  }
  horn_fact_add(l2, r2, g_can_nv);
  if (sgh) {
    g_hf[g_hf_n - 1].sghit = 1;
    horn_heap_push(&g_hpheap, g_hf_n - 1);
  }
  if (g_horn_goal_stop && !g_horn_goal_reached && g_hg_n > 0 &&
      horn_goal_probe(l2, r2)) {
    g_horn_goal_reached = 1;
    g_horn_stop = 1;
    fprintf(stderr, "FindProof: Horn release: goal-subsuming unit %s = %s\n",
            lb, rb);
  }
  if (g_hrel_n >= g_hrel_cap) {
    g_hrel_cap = (g_hrel_cap == 0) ? (1 << 12) : g_hrel_cap * 2;
    g_hrel = (HRel *)realloc(g_hrel, (size_t)g_hrel_cap * sizeof(HRel));
    if (g_hrel == NULL) tp_giveup("OOM in Horn release");
  }
  HRel *re = &g_hrel[g_hrel_n];
  re->l = strdup(lb); re->r = strdup(rb);
  re->tl = l2; re->tr = r2;
  re->ground = (g_can_nv == 0);
  re->sg_hit = sgh;
  re->goal_syms = 0;   // filled at ranking time
  re->n_weak = 0;
  {
    int cnt[HORN_REL_VARS];
    memset(cnt, 0, sizeof cnt);
    int stack[512], sp = 0;
    stack[sp++] = l2; stack[sp++] = r2;
    while (sp > 0) {
      int t = stack[--sp];
      if (g_hn[t].head < 0) cnt[-g_hn[t].head - 1]++;
      else for (int i = 0; i < g_hn[t].nargs && sp < 512; i++)
        stack[sp++] = HARG(t, i);
    }
    for (int i = 0; i < g_can_nv; i++) re->n_weak += (cnt[i] == 1);
  }
  re->size = (int)(lo + ro);
  re->order = g_hrel_n;
  re->goal_hit = 0;   // filled at ranking time, once bindings are clean
  re->dead = 0;
  re->prov = horn_prov_record(ru, re);
  if (re->prov >= 0) g_hf[g_hf_n - 1].src = re->prov;   // the fact above
  g_hrel_n++;
  if (g_hrel_n >= g_horn_max_rel) g_horn_stop = 1;
  return;
  }
  }
reject:
  g_hn_n = hn_mark; g_ha_n = ha_mark;
}

// Backtracking fill: satisfy every condition except skip_pos (already
// bound to the given fact) from PROCESSED facts, under the current
// bindings.  The given-fact discipline enumerates each fact
// combination once -- when its last-processed member is the given.
static void horn_fill(TpProblem *tp, const HRule *ru, int ci, int skip_pos) {
  if (g_horn_stop) return;
  while (ci == skip_pos) ci++;
  if (ci >= ru->nc) { horn_emit_release(tp, ru); return; }
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > g_horn_max_attempts || horn_wall_over() ||
       g_hn_n > g_horn_max_nodes)) { g_horn_stop = 1; return; }
  int c = ru->cl[ci], coff = 0;
  horn_deref(&c, &coff);
  int off = (ci + 1) * HORN_VSPACE;
  // candidate chains: the condition head's bucket (or every bucket for
  // a still-unbound condition head) plus the var-headed facts.
  for (int pass = 0; pass < 2 && !g_horn_stop; pass++) {
    int e;
    if (pass == 0) e = (g_hn[c].head >= 0) ? g_hix_head[g_hn[c].head] : -2;
    else           e = g_hix_varhead;
    if (e == -2) {  // unbound condition head: scan all facts directly
      for (int f = 0; f < g_hf_n && !g_horn_stop; f++) {
        if (!g_hf[f].processed || g_hf[f].dead) continue;
        for (int orient = 0; orient < 2; orient++) {
          int fl = orient ? g_hf[f].r : g_hf[f].l;
          int fr = orient ? g_hf[f].l : g_hf[f].r;
          int mark = g_htrail_n;
          g_horn_attempts++;
          // Wall-only inner poll: the attempt/node caps stay ENTRY-gated
          // (as before this fix) so a fast release that trips the node
          // ceiling mid-loop stops at the exact same point and its proof
          // stays byte-identical; only the wall overrun needed catching
          // inside the loop (one horn_fill can sweep a long candidate
          // chain without re-entry).
          if ((g_horn_attempts & 0xfff) == 0 && horn_wall_over())
            g_horn_stop = 1;
          if (!g_horn_stop &&
              horn_unify(ru->cl[ci], 0, fl, off, 0) &&
              horn_unify(ru->ct[ci], 0, fr, off, 0)) {
            g_hpf[ci] = f;
            horn_fill(tp, ru, ci + 1, skip_pos);
          }
          horn_undo(mark);
        }
      }
      continue;
    }
    for (; e >= 0 && !g_horn_stop; e = g_hix[e].next) {
      int f = g_hix[e].fact;
      if (!g_hf[f].processed || g_hf[f].dead) continue;
      int fl = g_hix[e].orient ? g_hf[f].r : g_hf[f].l;
      int fr = g_hix[e].orient ? g_hf[f].l : g_hf[f].r;
      int mark = g_htrail_n;
      g_horn_attempts++;
      if ((g_horn_attempts & 0xfff) == 0 && horn_wall_over())
        g_horn_stop = 1;
      if (!g_horn_stop &&
          horn_unify(ru->cl[ci], 0, fl, off, 0) &&
          horn_unify(ru->ct[ci], 0, fr, off, 0)) {
        g_hpf[ci] = f;
        horn_fill(tp, ru, ci + 1, skip_pos);
      }
      horn_undo(mark);
    }
  }
}

// One given step: bind condition cpos of ru with the given fact
// (orientation fixed), then fill the remaining conditions from
// processed facts.
static void horn_given_try(TpProblem *tp, const HRule *ru, int cpos,
                           int fl, int fr) {
  int off = (cpos + 1) * HORN_VSPACE;
  int mark = g_htrail_n;
  g_horn_attempts++;
  if ((g_horn_attempts & 0xfff) == 0 &&
      (g_horn_attempts > g_horn_max_attempts || horn_wall_over() ||
       g_hn_n > g_horn_max_nodes)) { g_horn_stop = 1; return; }
  if (horn_unify(ru->cl[cpos], 0, fl, off, 0) &&
      horn_unify(ru->ct[cpos], 0, fr, off, 0)) {
    g_hpf[cpos] = g_horn_given_ix;
    horn_fill(tp, ru, 0, cpos);
  }
  horn_undo(mark);
}

// Dispatch a given fact through the rule-condition index: only the
// rules whose condition head matches (plus var-headed conditions, and
// every entry when the fact side itself is a bare variable) join.
static void horn_given_dispatch(TpProblem *tp, int given) {
  g_horn_given_ix = given;
  for (int orient = 0; orient < 2 && !g_horn_stop; orient++) {
    int fl = orient ? g_hf[given].r : g_hf[given].l;
    int fr = orient ? g_hf[given].l : g_hf[given].r;
    int h = g_hn[fl].head;
    if (h < 0) {   // var-headed side: any condition could bind it
      for (int e = 0; e < g_hrix_n && !g_horn_stop; e++) {
        if (!g_hr[g_hrix[e].rule].rel) continue;
        horn_given_try(tp, &g_hr[g_hrix[e].rule], g_hrix[e].cpos, fl, fr);
      }
      continue;
    }
    for (int pass = 0; pass < 2 && !g_horn_stop; pass++) {
      int e = (pass == 0) ? g_hrix_head[h] : g_hrix_varhead;
      for (; e >= 0 && !g_horn_stop; e = g_hrix[e].next) {
        if (!g_hr[g_hrix[e].rule].rel) continue;
        horn_given_try(tp, &g_hr[g_hrix[e].rule], g_hrix[e].cpos, fl, fr);
      }
    }
  }
}

static int g_horn_rank_ground = 0;   // THVM_ATP_HORN_GROUND: ground tier on

static int horn_rel_cmp(const void *a, const void *b) {
  const HRel *x = (const HRel *)a, *y = (const HRel *)b;
  if (x->goal_hit != y->goal_hit) return y->goal_hit - x->goal_hit;
  if (x->sg_hit != y->sg_hit) return y->sg_hit - x->sg_hit;   // goal-cone step
  if (g_horn_rank_ground && x->ground != y->ground) return y->ground - x->ground;
  if (x->goal_syms != y->goal_syms) return y->goal_syms - x->goal_syms;
  if (x->n_weak != y->n_weak) return x->n_weak - y->n_weak;   // no weakening vars
  if (x->size != y->size) return x->size - y->size;           // smaller first
  return x->order - y->order;                                 // earlier first
}

// Final selection order: within-group rank FIRST (a round-robin over
// the head-symbol groups), then the quality tiers.  One prolific
// fragment (a ring's additive lemmas) can no longer crowd every other
// signature group out of the kept window; single-group problems are
// unaffected (w then equals the global rank).
static int horn_rel_cmp_rr(const void *a, const void *b) {
  const HRel *x = (const HRel *)a, *y = (const HRel *)b;
  if (x->goal_hit != y->goal_hit) return y->goal_hit - x->goal_hit;
  if (x->sg_hit != y->sg_hit) return y->sg_hit - x->sg_hit;
  if (x->w != y->w) return x->w - y->w;
  return horn_rel_cmp(a, b);
}

