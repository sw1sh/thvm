// ===================================================================
// Backward goal-directed condensed detachment (--horn-back)
// ===================================================================
// The LCL condensed-detachment family encodes modus ponens over one
// binary connective as a dispatcher rule
//   theorem(implies(Y,X)) & theorem(Y) -> theorem(X)
// plus axiom schemas (axiom(A_k) & rule_1 theorem-ify them) and a
// GROUND goal theorem(G).  Forward chaining floods: it releases ever
// more general theorem SCHEMAS while the goal is one specific ground
// formula, so the schema space explodes before the ground instance is
// reached (231k-1.46M releases, 0 resonance).  This lane regresses the
// goal instead -- an iterative-deepening DFS over the decoded rules
// with the ground goal as the root, so the ground goal anchors every
// subgoal's structure (the major-premise unification is constrained by
// the goal, not generative as in forward chaining).
//
// Soundness: each closure is a real unifier of a subgoal with an axiom
// instance (base) or a rule conclusion (a modus-ponens step); a proved
// GROUND goal is a genuine theorem, memoized and reusable anywhere.  On
// success theorem(G)=true is released as a unit (a real consequence),
// which refutes the negated goal true != theorem(G); g_cli_horn_applied
// gates the Satisfiable claim.  THVM_ATP_HORN_BACK unset/0 disables and
// the whole lane is a no-op (default build byte-identical).

static int    g_bk_on = 0;                 // THVM_ATP_HORN_BACK
static int   *g_bk_bind = NULL;            // per global var id: node or -1
static int    g_bk_bind_cap = 0;
static int   *g_bk_trail = NULL;
static int    g_bk_trail_n = 0, g_bk_trail_cap = 0;
static int    g_bk_nv = 0;                 // next fresh global var id
static int    g_bk_over = 0;               // terminal budget exhaustion
static long   g_bk_nodes = 0, g_bk_node_cap = 0;
static double g_bk_deadline = 0.0;
static int    g_bk_size_cap = 0;
static int    g_bk_goal_hpr[64];           // per goal: derivation record
#define BK_MAXOPEN   256
#define BK_KEYLEN    (2 * TP_SIDE_LEN + 2)
#define BK_DEPTH_MAX 200

typedef struct { int l, r, d, id; } BkGoal;   // id: derivation-log node

static void bk_bind_ensure(int v) {
  if (v < g_bk_bind_cap) return;
  int nc = (g_bk_bind_cap == 0) ? (1 << 14) : g_bk_bind_cap;
  while (nc <= v) nc *= 2;
  g_bk_bind = (int *)realloc(g_bk_bind, (size_t)nc * sizeof(int));
  if (g_bk_bind == NULL) tp_giveup("OOM in Horn back");
  for (int i = g_bk_bind_cap; i < nc; i++) g_bk_bind[i] = -1;
  g_bk_bind_cap = nc;
}
static void bk_trail_push(int v) {
  if (g_bk_trail_n >= g_bk_trail_cap) {
    g_bk_trail_cap = (g_bk_trail_cap == 0) ? (1 << 14) : g_bk_trail_cap * 2;
    g_bk_trail = (int *)realloc(g_bk_trail, (size_t)g_bk_trail_cap * sizeof(int));
    if (g_bk_trail == NULL) tp_giveup("OOM in Horn back");
  }
  g_bk_trail[g_bk_trail_n++] = v;
}
static void bk_undo(int mark) {
  while (g_bk_trail_n > mark) g_bk_bind[g_bk_trail[--g_bk_trail_n]] = -1;
}

// All backward-prover variables are globally unique (standardized apart
// at rule/fact instantiation), so unification runs in one flat space --
// no per-clause offsets.
static int bk_deref(int t) {
  while (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1;
    if (v >= g_bk_bind_cap || g_bk_bind[v] < 0) break;
    t = g_bk_bind[v];
  }
  return t;
}
static int bk_occurs(int v, int t, int depth) {
  if (depth > 400) return 1;
  t = bk_deref(t);
  if (g_hn[t].head < 0) return (-g_hn[t].head - 1) == v;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (bk_occurs(v, HARG(t, i), depth + 1)) return 1;
  return 0;
}
static int bk_unify(int a, int b, int depth) {
  if (depth > 400) return 0;
  a = bk_deref(a); b = bk_deref(b);
  if (a == b) return 1;
  if (g_hn[a].head < 0) {
    int v = -g_hn[a].head - 1;
    if (bk_occurs(v, b, 0)) return 0;
    bk_bind_ensure(v); g_bk_bind[v] = b; bk_trail_push(v); return 1;
  }
  if (g_hn[b].head < 0) {
    int v = -g_hn[b].head - 1;
    if (bk_occurs(v, a, 0)) return 0;
    bk_bind_ensure(v); g_bk_bind[v] = a; bk_trail_push(v); return 1;
  }
  if (g_hn[a].head != g_hn[b].head || g_hn[a].nargs != g_hn[b].nargs) return 0;
  for (int i = 0; i < g_hn[a].nargs; i++)
    if (!bk_unify(HARG(a, i), HARG(b, i), depth + 1)) return 0;
  return 1;
}

// Standardize a clause term apart: its clause-local variables (head
// -(loc+1), loc < HORN_VSPACE) map to fresh global ids via `map`, which
// the caller shares across the clause's conclusion and conditions so
// their variables link.
static int bk_rename(int t, int *map) {
  if (g_hn[t].head < 0) {
    int loc = -g_hn[t].head - 1;
    if (loc >= HORN_VSPACE) { g_bk_over = 1; return t; }
    if (map[loc] < 0) {
      int fv = g_bk_nv++;
      bk_bind_ensure(fv); g_bk_bind[fv] = -1;
      map[loc] = fv;
    }
    return horn_node(-(map[loc] + 1), 0, NULL);
  }
  int na = g_hn[t].nargs;
  if (na > 16) { g_bk_over = 1; return t; }
  int args[16];
  for (int i = 0; i < na; i++) args[i] = bk_rename(HARG(t, i), map);
  return horn_node(g_hn[t].head, na, args);
}

static int bk_size(int t, int depth) {
  if (depth > 400) return 1 << 20;
  t = bk_deref(t);
  if (g_hn[t].head < 0) return 1;
  int s = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) s += bk_size(HARG(t, i), depth + 1);
  return s;
}
static int bk_is_ground(int t, int depth) {
  if (depth > 400) return 0;
  t = bk_deref(t);
  if (g_hn[t].head < 0) return 0;
  for (int i = 0; i < g_hn[t].nargs; i++)
    if (!bk_is_ground(HARG(t, i), depth + 1)) return 0;
  return 1;
}
// Print a ground (deref'd) term canonically into buf; 0 on overflow.
static int bk_print(const TpProblem *tp, char *buf, size_t cap, size_t *o, int t) {
  t = bk_deref(t);
  if (g_hn[t].head < 0) return 0;   // caller guarantees ground
  const char *name = tp->syms[g_hn[t].head].name;
  size_t nl = strlen(name);
  if (*o + nl + 2 >= cap) return 0;
  memcpy(buf + *o, name, nl); *o += nl;
  if (g_hn[t].nargs > 0) {
    buf[(*o)++] = '(';
    for (int i = 0; i < g_hn[t].nargs; i++) {
      if (i > 0) { if (*o + 1 >= cap) return 0; buf[(*o)++] = ','; }
      if (!bk_print(tp, buf, cap, o, HARG(t, i))) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// --- ground-goal memo -------------------------------------------------
// success: a set of ground goal keys proved (a real lemma, reusable at
// any depth).  failure: per key, the largest remaining-depth at which
// the goal was fully explored and failed (a true exhaustion -- never a
// budget cutoff), so any query with <= that depth prunes.
#define BK_MEMO_SLOTS (1 << 20)
// faildepth<0 (=-1): proved.  onpath: this ground goal is an open
// ancestor on the current DFS branch (regularity: re-deriving it is a
// loop -- prune).  hpr: the committed derivation record of a proved
// key (--proof), -1 when none.
typedef struct { char *key; int faildepth; int hpr; u8 onpath; } BkMemo;
static BkMemo *g_bk_memo = NULL;
static int     g_bk_memo_n = 0;

static BkMemo *bk_memo_find(const char *key, int create) {
  if (g_bk_memo == NULL) return NULL;
  u64 h = tp_name_hash(key, strlen(key));
  u32 slot = (u32)(h % (u64)BK_MEMO_SLOTS);
  for (;;) {
    if (g_bk_memo[slot].key == NULL) {
      if (!create) return NULL;
      if ((u32)g_bk_memo_n >= BK_MEMO_SLOTS - (BK_MEMO_SLOTS >> 3)) return NULL;
      g_bk_memo[slot].key = strdup(key);
      g_bk_memo[slot].faildepth = -2;   // fresh, unset
      g_bk_memo[slot].hpr = -1;
      g_bk_memo[slot].onpath = 0;
      g_bk_memo_n++;
      return &g_bk_memo[slot];
    }
    if (strcmp(g_bk_memo[slot].key, key) == 0) return &g_bk_memo[slot];
    slot = (slot + 1u) % BK_MEMO_SLOTS;
  }
}

// Build the ground key for goal (l,r); 0 if either side is non-ground or
// oversize.
static int bk_key(const TpProblem *tp, int l, int r, char *key) {
  size_t o = 0;
  if (!bk_is_ground(l, 0) || !bk_is_ground(r, 0)) return 0;
  if (!bk_print(tp, key, BK_KEYLEN - 2, &o, l)) return 0;
  key[o++] = '=';
  if (!bk_print(tp, key, BK_KEYLEN, &o, r)) return 0;
  return 1;
}

// --- backward-proof derivation log (--proof) ---------------------------
// Under g_prov_on the DFS logs every goal closure (id-keyed entries,
// truncated with the trail on backtrack).  When a GROUND goal commits,
// its self-contained log segment is serialized into g_hpr derivation
// records (bk_pf_commit) and replaced by one committed-record entry,
// so the released theorem(G) = true unit carries a full
// hyper-resolution chain exactly like a forward release.
typedef struct {
  int id, gl, gr;              // goal node + its terms at closure
  int kind, who;               // 0 fact / 1 rule / 2 committed record
  int kids[HORN_MAX_COND];     // rule closures: the condition node ids
  int nkids;
} BkLog;
static BkLog *g_bkl = NULL; static int g_bkl_n = 0, g_bkl_cap = 0;
static int g_bk_id = 0;

static void bk_log_push(int id, int gl, int gr, int kind, int who,
                        const int *kids, int nkids) {
  if (!g_prov_on) return;
  atp_work_pause();   // off the search budget
  if (g_bkl_n >= g_bkl_cap) {
    g_bkl_cap = (g_bkl_cap == 0) ? (1 << 12) : g_bkl_cap * 2;
    g_bkl = (BkLog *)realloc(g_bkl, (size_t)g_bkl_cap * sizeof(BkLog));
    if (g_bkl == NULL) tp_giveup("OOM in Horn back");
  }
  BkLog *e = &g_bkl[g_bkl_n++];
  e->id = id; e->gl = gl; e->gr = gr; e->kind = kind; e->who = who;
  e->nkids = (nkids > HORN_MAX_COND) ? HORN_MAX_COND : nkids;
  for (int i = 0; i < e->nkids; i++) e->kids[i] = kids[i];
  atp_work_resume();
}

// Print a term instance under the CURRENT bindings; leftover free
// variables render canonically per record (V0, V1, ...).
static int bk_print_inst(const TpProblem *tp, char *buf, size_t cap,
                         size_t *o, int t, int *vids, int *nv) {
  t = bk_deref(t);
  if (g_hn[t].head < 0) {
    int v = -g_hn[t].head - 1, slot = -1;
    for (int i = 0; i < *nv; i++) if (vids[i] == v) slot = i;
    if (slot < 0) {
      if (*nv >= HORN_REL_VARS) return 0;
      vids[*nv] = v; slot = (*nv)++;
    }
    if (*o + 12 >= cap) return 0;
    *o += (size_t)snprintf(buf + *o, cap - *o, "V%d", slot);
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
      if (!bk_print_inst(tp, buf, cap, o, HARG(t, i), vids, nv)) return 0;
    }
    if (*o + 1 >= cap) return 0;
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
  return 1;
}

// Serialize node `id` of the log segment [lo, g_bkl_n).  Returns a
// parent ref (>= 0 g_hpr record, < 0 ~g_hpc input clause) or
// HORN_SRC_NONE on failure.
static int bk_pf_build(const TpProblem *tp, int id, int lo) {
  const BkLog *e = NULL;
  for (int i = g_bkl_n - 1; i >= lo; i--)
    if (g_bkl[i].id == id) { e = &g_bkl[i]; break; }
  if (e == NULL) return HORN_SRC_NONE;
  if (e->kind == 2)
    return (e->who >= 0) ? e->who : HORN_SRC_NONE;
  if (e->kind == 0)
    return (e->who >= 0 && e->who < g_hf_n) ? g_hf[e->who].src
                                            : HORN_SRC_NONE;
  const HRule *ru = (e->who >= 0 && e->who < g_hr_n) ? &g_hr[e->who] : NULL;
  if (ru == NULL || e->nkids != ru->nc) return HORN_SRC_NONE;
  int fps[HORN_MAX_COND];
  for (int i = 0; i < e->nkids; i++) {
    fps[i] = bk_pf_build(tp, e->kids[i], lo);
    if (fps[i] == HORN_SRC_NONE) return HORN_SRC_NONE;
  }
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
  size_t ob = 0, orr = 0;
  int vids[HORN_REL_VARS], nv = 0;
  if (!bk_print_inst(tp, lb, sizeof lb, &ob, e->gl, vids, &nv) ||
      !bk_print_inst(tp, rb, sizeof rb, &orr, e->gr, vids, &nv))
    return HORN_SRC_NONE;
  char *sl = strdup(lb), *sr = strdup(rb);
  int ix = horn_prov_new(ru->hpc, fps, e->nkids, sl, sr);
  if (ix < 0) { free(sl); free(sr); return HORN_SRC_NONE; }
  return ix;
}

// Commit the just-proved goal node (bindings still live): serialize
// its subtree; a bare input-clause ref (the goal closed as one fact
// instance) wraps in a record so the result is citable.  Returns the
// g_hpr index or -1.
static int bk_pf_commit_impl(const TpProblem *tp, int id, int gl, int gr,
                             int lo) {
  int ref = bk_pf_build(tp, id, lo);
  if (ref == HORN_SRC_NONE) return -1;
  if (ref >= 0) return ref;
  static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
  size_t ob = 0, orr = 0;
  int vids[HORN_REL_VARS], nv = 0;
  if (!bk_print_inst(tp, lb, sizeof lb, &ob, gl, vids, &nv) ||
      !bk_print_inst(tp, rb, sizeof rb, &orr, gr, vids, &nv)) return -1;
  char *sl = strdup(lb), *sr = strdup(rb);
  int ix = horn_prov_new(~ref, NULL, 0, sl, sr);
  if (ix < 0) { free(sl); free(sr); return -1; }
  return ix;
}
static int bk_pf_commit(const TpProblem *tp, int id, int gl, int gr, int lo) {
  atp_work_pause();   // off the search budget
  int ix = bk_pf_commit_impl(tp, id, gl, gr, lo);
  atp_work_resume();
  return ix;
}

// Polled every 64 nodes: a node over the curried terms of a modal
// problem costs milliseconds (LCL637+1.015 checked first at 16384
// nodes, 50 s in, past a 23 s budget).
static int bk_budget_ok(void) {
  if (g_bk_over) return 0;
  if ((++g_bk_nodes & 0x3f) == 0) {
    if (horn_budget_now() > g_bk_deadline) { g_bk_over = 1; return 0; }
    if (g_horn_abs_deadline > 0.0 && horn_now() > g_horn_abs_deadline) {
      g_bk_over = 1;
      return 0;
    }
  }
  if (g_bk_node_cap > 0 && g_bk_nodes > g_bk_node_cap) { g_bk_over = 1; return 0; }
  if (g_hn_n > (1 << 24)) { g_bk_over = 1; return 0; }   // arena ceiling
  return 1;
}

static int bk_prove_seq(const TpProblem *tp, BkGoal *gs, int n);

// Try every way to close goal (gl,gr) at remaining depth d, then prove
// the pending tail: BASE = a unit fact (an axiom instance); STEP = a
// rule conclusion (a modus-ponens regression), spawning the rule's
// conditions ahead of the tail (major condition first).  `id` is the
// goal's derivation-log node.
static int bk_prove_choices(const TpProblem *tp, int id, int gl, int gr,
                            int d, BkGoal *tail, int tn) {
  // BASE: match a unit fact.  Input clauses arrive in either orientation
  // (true = atom OR atom = true), so try both pairings.
  for (int f = 0; f < g_hf_n && !g_bk_over; f++) {
    for (int orient = 0; orient < 2 && !g_bk_over; orient++) {
      int mark = g_bk_trail_n, an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv;
      int lmark = g_bkl_n;
      int map[HORN_VSPACE];
      for (int i = 0; i < HORN_VSPACE; i++) map[i] = -1;
      int fl = bk_rename(orient ? g_hf[f].r : g_hf[f].l, map);
      int fr = bk_rename(orient ? g_hf[f].l : g_hf[f].r, map);
      if (!g_bk_over && bk_unify(gl, fl, 0) && bk_unify(gr, fr, 0)) {
        bk_log_push(id, gl, gr, 0, f, NULL, 0);
        if (bk_prove_seq(tp, tail, tn)) return 1;
      }
      g_bkl_n = lmark;
      bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
      if (horn_eq(g_hf[f].l, g_hf[f].r)) break;  // symmetric: 2nd orient redundant
    }
  }
  if (g_bk_over) return 0;
  // STEP: regress through a rule conclusion (either orientation).
  if (d > 0) {
    for (int r = 0; r < g_hr_n && !g_bk_over; r++) {
      const HRule *ru = &g_hr[r];
      for (int orient = 0; orient < 2 && !g_bk_over; orient++) {
        int mark = g_bk_trail_n, an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv;
        int lmark = g_bkl_n;
        int map[HORN_VSPACE];
        for (int i = 0; i < HORN_VSPACE; i++) map[i] = -1;
        int cl0 = bk_rename(orient ? ru->concl_r : ru->concl_l, map);
        int cr0 = bk_rename(orient ? ru->concl_l : ru->concl_r, map);
        if (!g_bk_over && bk_unify(gl, cl0, 0) && bk_unify(gr, cr0, 0)) {
          BkGoal ng[BK_MAXOPEN];
          int kids[HORN_MAX_COND];
          int m = 0, ok = 1;
          for (int c = 0; c < ru->nc && ok; c++) {
            if (m >= BK_MAXOPEN - tn) { ok = 0; break; }
            ng[m].l = bk_rename(ru->cl[c], map);
            ng[m].r = bk_rename(ru->ct[c], map);
            ng[m].d = d - 1;
            ng[m].id = g_prov_on ? g_bk_id++ : 0;
            kids[c] = ng[m].id;
            m++;
          }
          for (int c = 0; c < tn && ok; c++) {
            if (m >= BK_MAXOPEN) { ok = 0; break; }
            ng[m++] = tail[c];
          }
          if (ok) bk_log_push(id, gl, gr, 1, r, kids, ru->nc);
          if (ok && !g_bk_over && bk_prove_seq(tp, ng, m)) return 1;
        }
        g_bkl_n = lmark;
        bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
        if (horn_eq(ru->concl_l, ru->concl_r)) break;  // symmetric
      }
    }
  }
  return 0;
}

// Selection score for the pending subgoal: prefer GROUND (memoizable,
// committed) then the most rigidly-constrained (most non-variable
// structure, fewest variable occurrences).  Decisive for the CD rule
// families whose conditions include a still-free premise theorem(Y)
// alongside a highly-constrained axiom lookup axiom(imp(Y,X)): solving
// the constrained one first binds Y instead of "prove SOME theorem".
static int bk_rigid_score(int t, int depth, int *vars) {
  if (depth > 400) return 0;
  t = bk_deref(t);
  if (g_hn[t].head < 0) { (*vars)++; return 0; }
  int n = 1;
  for (int i = 0; i < g_hn[t].nargs; i++) n += bk_rigid_score(HARG(t, i), depth + 1, vars);
  return n;
}
static int bk_score(int gl) {
  int vars = 0;
  int rigid = bk_rigid_score(gl, 0, &vars);
  if (vars == 0) return 1 << 20;            // ground: pick first
  return rigid * 8 - vars;
}

// Prove the conjunction gs[0..n): select the most-constrained pending
// subgoal, solve it, then the rest.  A conjunction is order-independent
// for correctness, so selection is a pure efficiency lever.  A GROUND
// subgoal is solved in isolation and committed (its proof binds only its
// own fresh variables, none shared with the rest), with memoization; a
// non-ground subgoal backtracks over its solutions against the rest.
static int bk_prove_seq(const TpProblem *tp, BkGoal *gs, int n) {
  if (n == 0) return 1;
  if (!bk_budget_ok()) return 0;
  // Select the best subgoal; move the rest into a contiguous tail.
  int b = 0, best = -(1 << 30);
  for (int i = 0; i < n; i++) {
    int sc = bk_score(bk_deref(gs[i].l));
    if (sc > best) { best = sc; b = i; }
  }
  BkGoal g = gs[b];
  BkGoal tail[BK_MAXOPEN];
  int tn = 0;
  for (int i = 0; i < n; i++) if (i != b) tail[tn++] = gs[i];
  int gl = bk_deref(g.l), gr = bk_deref(g.r);
  int d = (g.d > BK_DEPTH_MAX) ? BK_DEPTH_MAX : g.d;
  if (bk_size(gl, 0) + bk_size(gr, 0) > g_bk_size_cap) return 0;
  if (bk_is_ground(gl, 0) && bk_is_ground(gr, 0)) {
    char key[BK_KEYLEN];
    int have = bk_key(tp, gl, gr, key);
    if (have) {
      BkMemo *mm = bk_memo_find(key, 0);
      if (mm != NULL) {
        if (mm->faildepth == -1) {
          // Reuse: reference the committed derivation record.
          int lmark = g_bkl_n;
          bk_log_push(g.id, gl, gr, 2, mm->hpr, NULL, 0);
          if (bk_prove_seq(tp, tail, tn)) return 1;
          g_bkl_n = lmark;
          return 0;
        }
        if (mm->onpath) return 0;             // regularity: ancestor loop
        if (mm->faildepth >= d) return 0;
      }
    }
    BkMemo *pm = have ? bk_memo_find(key, 1) : NULL;
    if (pm != NULL) pm->onpath = 1;
    int mark = g_bk_trail_n, an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv;
    int lmark = g_bkl_n;
    int ok = bk_prove_choices(tp, g.id, gl, gr, d, NULL, 0);
    // Commit the derivation BEFORE the bindings unwind (the record
    // prints the closure instances), then drop the consumed segment.
    int hpr = (ok && g_prov_on) ? bk_pf_commit(tp, g.id, gl, gr, lmark) : -1;
    bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
    g_bkl_n = lmark;
    if (pm != NULL) pm->onpath = 0;
    if (!ok) {
      if (pm != NULL && !g_bk_over && pm->faildepth != -1 && d > pm->faildepth)
        pm->faildepth = d;
      return 0;
    }
    if (pm != NULL) {
      pm->faildepth = -1;       // proved: reusable at any depth
      pm->hpr = hpr;
    }
    // An enclosing rule closure still finds this node id via the
    // committed-record entry.
    bk_log_push(g.id, gl, gr, 2, hpr, NULL, 0);
    return bk_prove_seq(tp, tail, tn);
  }
  return bk_prove_choices(tp, g.id, gl, gr, d, tail, tn);
}

// Identify the goal equation to derive: the negated unit true != theorem(G).
// Returns 1 and sets (*tl,*tr) to (theorem(G), true) oriented for the rule
// conclusions; requires theorem(G) ground.
static int bk_goal_pick(const TpProblem *tp, int gi, int *tl, int *tr) {
  int a = g_hg_l[gi], b = g_hg_r[gi];
  // The `true` side is a nullary constant; the other is the derived atom.
  int atom = -1, tru = -1;
  for (int s = 0; s < 2; s++) {
    int x = s ? b : a, y = s ? a : b;
    if (g_hn[x].head >= 0 && g_hn[x].nargs > 0 && g_hn[y].head >= 0 &&
        g_hn[y].nargs == 0) { atom = x; tru = y; break; }
  }
  if (atom < 0) return 0;
  if (!bk_is_ground(atom, 0)) return 0;   // existential goal: not this lane
  *tl = atom; *tr = tru;
  return 1;
}

// Entry: iterative-deepening backward regression of every ground goal.
// Returns 1 iff some goal is proved (a refutation).
static int horn_backward_prove(const TpProblem *tp) {
  if (g_hr_n == 0 || g_hf_n == 0 || g_hg_n == 0) return 0;
  int depth_max = 40, size_cap = 0;
  {
    const char *de = getenv("THVM_ATP_HORN_BACK_DEPTH");
    const char *se = getenv("THVM_ATP_HORN_BACK_SIZE");
    if (de != NULL && atoi(de) > 0) depth_max = atoi(de);
    if (se != NULL && atoi(se) > 0) size_cap = atoi(se);
  }
  // Default term-size cap: the goal's own size plus generous slack (deep
  // detachment intermediates can exceed the goal, but not without bound).
  int gmax = 0, any = 0;
  for (int gi = 0; gi < g_hg_n; gi++) {
    int tl, tr;
    if (bk_goal_pick(tp, gi, &tl, &tr)) {
      any = 1;
      int s = bk_size(tl, 0) + bk_size(tr, 0);
      if (s > gmax) gmax = s;
    }
  }
  if (!any) return 0;
  if (size_cap == 0) size_cap = gmax + 24;
  g_bk_memo = (BkMemo *)calloc(BK_MEMO_SLOTS, sizeof(BkMemo));
  if (g_bk_memo == NULL) tp_giveup("OOM in Horn back");
  g_bk_memo_n = 0;
  for (int gi = 0; gi < 64; gi++) g_bk_goal_hpr[gi] = -1;
  double wall = 0.85 * g_horn_wall;
  g_bk_deadline = g_horn_t0 + wall;
  g_bk_node_cap = 0;   // wall-governed
  int proved = 0;
  fprintf(stderr, "FindProof: Horn backward CD: %d goal(s), size-cap %d, "
          "depth<=%d, wall %.1fs\n", g_hg_n, size_cap, depth_max, wall);
  for (int D = 1; D <= depth_max && !proved; D++) {
    g_bk_over = 0;
    g_bk_size_cap = size_cap;
    for (int gi = 0; gi < g_hg_n && !proved; gi++) {
      int tl, tr;
      if (!bk_goal_pick(tp, gi, &tl, &tr)) continue;
      // Re-instantiate the goal into a fresh proof space each iteration.
      int an = g_hn_n, aa = g_ha_n, nvm = g_bk_nv, mark = g_bk_trail_n;
      int map[HORN_VSPACE];
      for (int i = 0; i < HORN_VSPACE; i++) map[i] = -1;
      // Goal is ground, so rename is a copy (no vars); keeps the search
      // arena separable from the parsed goal terms.
      BkGoal root;
      root.l = bk_rename(tl, map);
      root.r = bk_rename(tr, map);
      root.d = D;
      root.id = 0;
      g_bkl_n = 0;
      g_bk_id = 1;
      if (!g_bk_over && bk_prove_seq(tp, &root, 1)) {
        proved = 1;
        // The root is ground: its commit landed in the memo -- stash
        // the record for the release site (the memo table frees below).
        if (g_prov_on && gi < 64) {
          char key[BK_KEYLEN];
          BkMemo *mm = bk_key(tp, root.l, root.r, key)
                         ? bk_memo_find(key, 0) : NULL;
          if (mm != NULL && mm->faildepth == -1) g_bk_goal_hpr[gi] = mm->hpr;
        }
      }
      bk_undo(mark); g_hn_n = an; g_ha_n = aa; g_bk_nv = nvm;
    }
    if (g_bk_over && horn_budget_now() > g_bk_deadline) break;   // out of wall
  }
  fprintf(stderr, "FindProof: Horn backward CD: %s (%ld nodes, %d memo, %.1fs)\n",
          proved ? "goal PROVED" : "no proof", g_bk_nodes, g_bk_memo_n,
          horn_now() - g_horn_t0_wall);
  if (g_bk_memo != NULL) {
    for (int i = 0; i < BK_MEMO_SLOTS; i++) free(g_bk_memo[i].key);
    free(g_bk_memo); g_bk_memo = NULL; g_bk_memo_n = 0;
  }
  free(g_bk_bind); g_bk_bind = NULL; g_bk_bind_cap = 0;
  free(g_bk_trail); g_bk_trail = NULL; g_bk_trail_n = g_bk_trail_cap = 0;
  free(g_bkl); g_bkl = NULL; g_bkl_n = g_bkl_cap = 0;
  g_bk_nv = 0;
  return proved;
}

// The pass: decode, chain, rank, append.  Runs after tp_parse and
// BEFORE SInE so released facts participate in axiom selection.
// Standalone dispatcher probe for the --ifeq-guard=2 give-up: does any
// positive clause have the ifeqN(X,X,Y[,Z]) = Y dispatcher shape?
// Reuses the Horn parser (its node arena self-grows) without any of
// tp_horn_release's release machinery, so it works under --horn=0 too.
static int tp_has_ifeq_dispatcher(TpProblem *tp) {
  HParse *ps = (HParse *)malloc(sizeof(HParse));
  if (ps == NULL) return 0;
  int mark_n = g_hn_n, mark_a = g_ha_n, found = 0;
  for (int c = 0; c < tp->n_cls && !found; c++) {
    if (tp->cls[c].neg) continue;
    ps->nv = 0;
    int l = horn_parse_side(tp, ps, tp->cls[c].lhs);
    int r = (l >= 0) ? horn_parse_side(tp, ps, tp->cls[c].rhs) : -1;
    if (l < 0 || r < 0) continue;
    if (horn_disp_check(l, r) || horn_disp_check(r, l)) found = 1;
  }
  g_hn_n = mark_n;   // release the probe's arena nodes
  g_ha_n = mark_a;
  free(ps);
  return found;
}

static void tp_horn_release(TpProblem *tp) {
  int n = tp->n_cls;
  // CD lane flag before any fact is added: fingerprints ride the adds.
  {
    const char *ce = getenv("THVM_ATP_HORN_CD");
    g_horn_cd = (ce != NULL && ce[0] != '0') ? atoi(ce) : 0;
    g_cd_fsub = g_cd_bsub = g_cd_reso_hits = 0;
    g_cd_rounds = 0; g_cd_overcap = 0; g_cd_reso_n = 0;
    const char *be = getenv("THVM_ATP_HORN_BACK");
    g_bk_on = (be != NULL && be[0] != '0') ? atoi(be) : 0;
    g_bk_nodes = 0; g_bk_over = 0;
  }
  // Parse every positive clause; remember each clause's term pair.
  int *pl = (int *)malloc((size_t)n * sizeof(int));
  int *prr = (int *)malloc((size_t)n * sizeof(int));
  int *pnv = (int *)malloc((size_t)n * sizeof(int));
  u8  *is_disp = (u8 *)calloc((size_t)n, 1);
  HParse *ps = (HParse *)malloc(sizeof(HParse));
  if (pl == NULL || prr == NULL || pnv == NULL || is_disp == NULL || ps == NULL)
    tp_giveup("OOM in Horn release");
  g_horn_disp = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
  if (g_horn_disp == NULL) tp_giveup("OOM in Horn release");
  int n_disp = 0;
  g_hg_n = 0;
  for (int c = 0; c < n; c++) {
    pl[c] = prr[c] = -1;
    ps->nv = 0;
    int l = horn_parse_side(tp, ps, tp->cls[c].lhs);
    int r = (l >= 0) ? horn_parse_side(tp, ps, tp->cls[c].rhs) : -1;
    if (l < 0 || r < 0) continue;
    if (tp->cls[c].neg) {
      if (g_hg_n < 64) { g_hg_l[g_hg_n] = l; g_hg_r[g_hg_n] = r; g_hg_n++; }
      continue;
    }
    pl[c] = l; prr[c] = r; pnv[c] = ps->nv;
    if (horn_disp_check(l, r) || horn_disp_check(r, l)) {
      is_disp[c] = 1;
      int f = (g_hn[l].head >= 0) ? g_hn[l].head : g_hn[r].head;
      if (!g_horn_disp[f]) { g_horn_disp[f] = 1; n_disp++; }
    }
  }
  int released_kept = 0;
  if (n_disp == 0) {
    // Slice mode: free the pool slot at once on a non-CD problem.
    if (g_horn_cd >= 2)
      tp_giveup("horn-cd slice: no encoded-Horn dispatcher");
    if (g_bk_on >= 2)
      tp_giveup("horn-back slice: no encoded-Horn dispatcher");
    goto done;   // not an encoded-Horn problem
  }
  if (g_bk_on >= 2) {
    // The backward lane targets ground condensed-detachment goals; the
    // existential (planning) and oversized-KB families crack via the v2
    // steered forward lane in the wm slice.
    int ex = 0;
    for (int g = 0; g < g_hg_n && !ex; g++)
      ex = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    if (ex) tp_giveup("horn-back slice: existential goal (v2 lane covers it)");
    if (tp->n_cls > 4000) tp_giveup("horn-back slice: oversized KB");
  }
  if (g_horn_cd >= 2) {
    // The CD discipline targets ground saturation-style goals; the
    // existential (planning) and oversized-KB families already crack
    // via the v2 steered lane in the wm slice.
    int ex = 0;
    for (int g = 0; g < g_hg_n && !ex; g++)
      ex = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    if (ex) tp_giveup("horn-cd slice: existential goal (v2 lane covers it)");
    if (tp->n_cls > 4000) tp_giveup("horn-cd slice: oversized KB");
  }
  g_hix_head = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdup_slots = g_horn_cd ? (HORN_DEDUP_SLOTS << 2) : HORN_DEDUP_SLOTS;
  g_hdup = (char **)calloc(g_hdup_slots, sizeof(char *));
  if (g_hix_head == NULL || g_hdup == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) g_hix_head[i] = -1;
  for (int i = 0; i < HORN_NSLOTS; i++) { g_hb[i].t = -1; g_can_map[i] = -1; }
  // Definition pass: collect the definitional unit facts first, so
  // every fact, rule and goal below normalizes into one alphabet.
  g_hdef_l = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdef_r = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  g_hdef_cls = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  if (g_hdef_l == NULL || g_hdef_r == NULL || g_hdef_cls == NULL)
    tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) {
    g_hdef_l[i] = g_hdef_r[i] = g_hdef_cls[i] = -1;
  }
  // --proof: the clause-index -> snapshot map for provenance capture.
  if (g_prov_on && g_hpc_of == NULL) {
    g_hpc_of = (int *)malloc((size_t)n * sizeof(int));
    if (g_hpc_of == NULL) tp_giveup("OOM in Horn release");
    for (int i = 0; i < n; i++) g_hpc_of[i] = -1;
  }
  // Snapshot the dispatcher clauses: every emitted hyper-resolution
  // record cites them (the decode applies the reducer implicitly).
  if (g_prov_on) {
    g_horn_disp_hpc_n = 0;
    for (int c = 0; c < n; c++) {
      if (!is_disp[c]) continue;
      int j = horn_prov_cls(tp, c);
      if (j >= 0 && g_horn_disp_hpc_n < 4)
        g_horn_disp_hpc[g_horn_disp_hpc_n++] = j;
    }
  }
  int n_defs = 0;
  for (int c = 0; c < n; c++) {
    if (pl[c] < 0 || is_disp[c] || tp->cls[c].neg) continue;
    HRule ru;
    if (horn_decode_side(pl[c], prr[c], &ru) ||
        horn_decode_side(prr[c], pl[c], &ru)) continue;
    horn_def_note(pl[c], prr[c], c);
    horn_def_note(prr[c], pl[c], c);
  }
  for (int i = 0; i < TP_MAX_SYMS; i++) n_defs += (g_hdef_r[i] >= 0);
  // Split parsed positives into rules and facts; seed the dedup set
  // with every original unit (both orientations) so releases never
  // re-add an input equation.
  int n_rules = 0;
  for (int c = 0; c < n; c++) {
    if (pl[c] < 0 || is_disp[c]) continue;
    HRule ru;
    if (horn_decode_side(pl[c], prr[c], &ru) ||
        horn_decode_side(prr[c], pl[c], &ru)) {
      ru.nv = pnv[c];
      ru.rel = 1;
      ru.conj = tp->cls[c].conj;
      ru.ghrel = 0;
      horn_prov_touch_reset();
      for (int i = 0; i < ru.nc; i++) {
        ru.cl[i] = horn_norm(ru.cl[i]);
        ru.ct[i] = horn_norm(ru.ct[i]);
      }
      ru.concl_l = horn_norm(ru.concl_l);
      ru.concl_r = horn_norm(ru.concl_r);
      ru.hpc = horn_prov_cls(tp, c);
      horn_prov_cls_defs(tp, ru.hpc);
      if (g_hr_n >= g_hr_cap) {
        g_hr_cap = (g_hr_cap == 0) ? (1 << 10) : g_hr_cap * 2;
        g_hr = (HRule *)realloc(g_hr, (size_t)g_hr_cap * sizeof(HRule));
        if (g_hr == NULL) tp_giveup("OOM in Horn release");
      }
      g_hr[g_hr_n++] = ru;
      n_rules++;
    } else {
      horn_prov_touch_reset();
      int fl = horn_norm(pl[c]), fr = horn_norm(prr[c]);
      if (!horn_eq(fl, fr)) {          // definitions trivialize themselves
        horn_fact_add(fl, fr, pnv[c]);
        g_hf[g_hf_n - 1].conj = tp->cls[c].conj;
        if (g_prov_on) {
          int j = horn_prov_cls(tp, c);
          horn_prov_cls_defs(tp, j);
          if (j >= 0) g_hf[g_hf_n - 1].src = ~j;
        }
      }
    }
    // dedup seed: canonical print of both orientations
    static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN], key[2 * TP_SIDE_LEN + 2];
    for (int orient = 0; orient < 2; orient++) {
      horn_can_reset();
      int hn_mark = g_hn_n, ha_mark = g_ha_n;
      int budget = 1 << 16;
      int a = horn_apply(orient ? prr[c] : pl[c], 0, &budget);
      int b = (a >= 0) ? horn_apply(orient ? pl[c] : prr[c], 0, &budget) : -1;
      size_t lo = 0, ro = 0;
      int remap[HORN_REL_VARS], nv = 0;
      for (int i = 0; i < HORN_REL_VARS; i++) remap[i] = -1;
      if (a >= 0 && b >= 0 &&
          horn_print(tp, lb, sizeof lb, &lo, a, remap, &nv) &&
          horn_print(tp, rb, sizeof rb, &ro, b, remap, &nv)) {
        snprintf(key, sizeof key, "%s=%s", lb, rb);
        horn_dedup_add(key);
      }
      g_hn_n = hn_mark; g_ha_n = ha_mark;   // seed applies are temporary
    }
  }
  if (n_rules == 0 || g_hf_n == 0) goto done;
  // Given-fact agenda loop (UR-resolution shape): pop the smallest
  // unprocessed fact, mark it processed, and join it into every rule
  // position against the already-processed facts.  Smallest-first
  // keeps the chaining on small lemmas instead of drowning in deep
  // instantiations, exactly like a weight-ordered given-clause loop.
  g_horn_t0 = horn_budget_now();
  g_horn_t0_wall = horn_now();
  g_horn_stop = 0; g_horn_attempts = 0;
  // Absolute stop deadline for the release: the slice's whole wall,
  // measured from process start.  g_horn_wall (0.4..0.9 of the wall,
  // from g_horn_t0, on the work clock) still shapes the search -- the
  // gate-relax ladder and the phase-2 head-closure trigger are fractions
  // of it, so leaving it untouched keeps a fast release byte-identical.
  // But g_horn_wall is anchored at the release START, which comes AFTER
  // the clausal pre-pass (up to 0.6 of the wall), and a loaded machine
  // stretches work time: g_horn_t0 + 0.4..0.9*wall runs PAST the slice
  // deadline and gets hard-killed with no SZS line (the 240s FOF NoOutput
  // class).  The STOP polls -- the forward loop's and the backward CD
  // lane's -- honor this absolute cap too, so the release self-terminates
  // at the slice wall without reshaping the search that finishes before
  // it.  An explicit --horn-wall keeps the old unbounded behavior (a
  // manual single-lane override).
  if (getenv("THVM_ATP_HORN_WALL") == NULL)
    g_horn_abs_deadline = g_cli_main_t0 + g_cli_wall_sec;
  else
    g_horn_abs_deadline = 0.0;

  // CD lane defaults: wall-governed chaining (the deep detachment
  // chains ARE the proof search here, not a preprocessing garnish), so
  // most of the run budget goes to the lane and the attempt cap steps
  // aside.  An explicit --horn-wall still overrides below.
  if (g_horn_cd) {
    double w = 0.8 * g_cli_wall_sec;
    g_horn_wall = (w > HORN_WALL_SEC) ? w : HORN_WALL_SEC;
    g_horn_max_attempts = (long)4e18;
    // Post-subsumption accepts ARE the CD search space: the v1 pool
    // cap ended a healthy cap-18 run mid-wall (400k accepts at 14s).
    g_horn_max_rel = 4000000;
    g_horn_max_nodes = (1 << 24);
  }
  if (g_bk_on) {
    // The backward regression IS the proof search here: give it the wall.
    double w = 0.9 * g_cli_wall_sec;
    g_horn_wall = (w > HORN_WALL_SEC) ? w : HORN_WALL_SEC;
  }
  {
    const char *we = getenv("THVM_ATP_HORN_WALL");
    const char *me = getenv("THVM_ATP_HORN_MAX");
    const char *ae = getenv("THVM_ATP_HORN_ATT");
    if (we != NULL && atof(we) > 0.0) g_horn_wall = atof(we);
    if (me != NULL && atoi(me) > 0) g_horn_max_rel = atoi(me);
    // An EXPLICIT wall governs the run: the 10M attempt cap otherwise
    // stops a deep-KB chain (CSR036: 13k releases, 903 subgoal hits,
    // goal unreached) long before the granted wall, and the
    // relevance-gate relax ladder never engages.
    if (ae != NULL && atol(ae) > 0) g_horn_max_attempts = atol(ae);
    else if (we != NULL && atof(we) > 0.0) g_horn_max_attempts = (long)4e18;
  }
  g_hrix_head = (int *)malloc((size_t)TP_MAX_SYMS * sizeof(int));
  if (g_hrix_head == NULL) tp_giveup("OOM in Horn release");
  for (int i = 0; i < TP_MAX_SYMS; i++) g_hrix_head[i] = -1;
  for (int i = 0; i < g_hr_n; i++)
    for (int j = 0; j < g_hr[i].nc; j++)
      horn_rule_index(i, j, g_hn[g_hr[i].cl[j]].head);
  // Goals normalize into the releases' (expanded) alphabet BEFORE the
  // loop: the backward cone regresses from them and the emit-time goal
  // probe unifies releases against them.
  for (int g = 0; g < g_hg_n; g++) {
    g_hg_l[g] = horn_norm(g_hg_l[g]);
    g_hg_r[g] = horn_norm(g_hg_r[g]);
  }
  // Backward goal-directed CD lane: regress the ground goal to axiom
  // instances instead of flooding forward.  On success theorem(G)=true
  // is a proved consequence -- release it as a unit (refutes the negated
  // goal true != theorem(G)) and claim the direct SZS route.
  if (g_bk_on && horn_backward_prove(tp)) {
    g_horn_goal_reached = 1;
    g_cli_horn_applied = 1;
    // Emit the proved theorem unit so --proof / the engine reconstruct a
    // refutation (the direct SZS route skips the engine when no proof is
    // requested; the unit is a real consequence either way).  Under
    // --proof the unit carries its committed backward-derivation record
    // (PROV_HORN); a goal whose record failed to commit is withheld --
    // a released theorem citing file() would misrepresent it as a
    // problem formula (CASC-J13).
    for (int gi = 0; gi < g_hg_n && tp->n_cls < TP_MAX_CLAUSES; gi++) {
      int tl, tr;
      if (!bk_goal_pick(tp, gi, &tl, &tr)) continue;
      int hpr = (gi < 64) ? g_bk_goal_hpr[gi] : -1;
      if (g_prov_on && !horn_prov_ok(hpr)) {
        fprintf(stderr, "FindProof: Horn backward CD: proved unit lacks a "
                "complete derivation record -- not released under --proof\n");
        continue;
      }
      static char lb[TP_SIDE_LEN], rb[TP_SIDE_LEN];
      size_t lo = 0, ro = 0;
      if (bk_print(tp, lb, sizeof lb, &lo, tl) &&
          bk_print(tp, rb, sizeof rb, &ro, tr)) {
        TpClause *cl = &tp->cls[tp->n_cls++];
        snprintf(cl->lhs, sizeof cl->lhs, "%s", lb);
        snprintf(cl->rhs, sizeof cl->rhs, "%s", rb);
        cl->neg = 0; cl->conj = 0;
        cl->prov_kind = g_prov_on ? PROV_HORN : PROV_NONE;
        cl->prov_ix = g_prov_on ? hpr : 0;
        tp_scan_term(tp, cl->lhs);
        tp_scan_term(tp, cl->rhs);
      }
    }
    goto done;
  }
  if (g_bk_on >= 2) tp_giveup("horn-back slice: no proof within budget");
  // CD lane: the initial weight cap.  Auto = the largest INPUT fact
  // plus slack; the goal deliberately does NOT enter the max -- the
  // and/equivalent PM goals expand to 25-50 symbols and a goal-sized
  // initial cap admits a space no wall can saturate, so the deepening
  // discipline never engages.  The goal probe is by unification (a
  // small general schema closes a big ground goal) and a goal-sized
  // final conclusion strictly matches a goal resonator, which is
  // cap-exempt.
  if (g_horn_cd) {
    int cap = 0;
    for (int f = 0; f < g_hf_n; f++)
      if (g_hf[f].size > cap) cap = g_hf[f].size;
    g_cd_cap = cap + 2;
    const char *cc = getenv("THVM_ATP_HORN_CD_CAP");
    const char *cs = getenv("THVM_ATP_HORN_CD_STEP");
    const char *cv = getenv("THVM_ATP_HORN_CD_VARS");
    if (cc != NULL && atoi(cc) > 0) g_cd_cap = atoi(cc);
    if (cs != NULL && atoi(cs) > 0) g_cd_step = atoi(cs);
    if (cv != NULL && atoi(cv) > 0) g_cd_maxvars = atoi(cv);
    // Goal-shape resonators from the normalized goals; input facts
    // already goal-shaped (an axiom the goal instantiates) join the
    // priority agenda immediately.
    horn_cd_reso_build();
    for (int f = 0; f < g_hf_n; f++)
      if (horn_cd_resonant(g_hf[f].l, g_hf[f].r)) {
        g_hf[f].sghit = 1;
        horn_heap_push(&g_hpheap, f);
      }
    fprintf(stderr, "FindProof: Horn CD lane: cap %d (step %d, wall %.1fs, "
            "%d resonator(s))\n",
            g_cd_cap, g_cd_step, g_horn_wall, g_cd_reso_n);
  }
  // Oversized KBs: focus the loop on the conjecture's relevance cone.
  // THVM_ATP_HORN_REL=0 disables.
  g_horn_input_nf = g_hf_n;
  {
    const char *rl = getenv("THVM_ATP_HORN_REL");
    const char *te = getenv("THVM_ATP_HORN_REL_TOL");
    if (te != NULL && atof(te) > 0.0) g_horn_gate_tol = atof(te);
    if (!(rl != NULL && rl[0] == '0') && tp->n_cls > 4000) {
      g_horn_gate_on = 1;
      horn_relevance_gate(tp, g_horn_gate_tol, 0);
      // Gated KBs: the relevance-cone chaining IS the credible proof
      // lane (completion drowns in whatever SInE cone fits the engine
      // caps), so it earns a real share of the run wall and the
      // attempt cap steps aside, exactly like the CD lane.  The 2.5s
      // garnish budget stopped CSR036 at 13k releases with the goal a
      // few thousand releases out (goal reached at 24k), and CSR040's
      // gate relax ladder never engaged at all.  Explicit --horn-wall
      // / THVM_ATP_HORN_ATT still override.
      if (getenv("THVM_ATP_HORN_WALL") == NULL && !g_horn_cd && !g_bk_on) {
        double w = 0.4 * g_cli_wall_sec;
        if (w > g_horn_wall) g_horn_wall = w;
        if (getenv("THVM_ATP_HORN_ATT") == NULL)
          g_horn_max_attempts = (long)4e18;
      }
    }
  }
  // Backward subgoal cone (v2): bounded regression from the goals, a
  // wall slice ahead of the forward loop.  Auto-gated (see
  // g_horn_steer); THVM_ATP_HORN_BWD: 0 disables, 2 forces.
  {
    const char *be = getenv("THVM_ATP_HORN_BWD");
    const char *bd = getenv("THVM_ATP_HORN_BWD_DEPTH");
    const char *bm = getenv("THVM_ATP_HORN_BWD_MAX");
    const char *gs = getenv("THVM_ATP_HORN_GOAL_STOP");
    if (bd != NULL && atoi(bd) > 0) g_hsg_depth_cap = atoi(bd);
    if (bm != NULL && atoi(bm) > 0) g_hsg_max = atoi(bm);
    g_horn_goal_stop = !(gs != NULL && gs[0] == '0');
    g_hsg_stop = 0; g_hsg_checks = 0; g_horn_goal_reached = 0;
    int existential = 0;
    for (int g = 0; g < g_hg_n && !existential; g++)
      existential = horn_has_var(g_hg_l[g]) || horn_has_var(g_hg_r[g]);
    g_horn_steer = (be != NULL && be[0] == '2')
                     ? 1 : (g_horn_gate_on || existential);
    if (be != NULL && be[0] == '0') g_horn_steer = 0;
    if (g_horn_steer && g_hg_n > 0) {
      double cw = 0.25 * g_horn_wall;
      g_hsg_deadline = g_horn_t0 + ((cw < 0.6) ? cw : 0.6);
      horn_backward_cone(tp);
      // Input facts already on a subgoal join the priority agenda too.
      for (int f = 0; f < g_hf_n && g_hsg_n > 0; f++)
        if (horn_sg_probe(g_hf[f].l, g_hf[f].r)) {
          g_hf[f].sghit = 1;
          horn_heap_push(&g_hpheap, f);
        }
    }
  }
  // Pick-given ratio (Otter shape): of every 4 pops, HORN_RATIO come
  // from the size heap and the rest by age (FIFO = delta-driven
  // breadth-first).  Pure smallest-first starves any fragment whose
  // small lemmas are unbounded (a ring's additive permutations bury
  // every multiplicative fact), while the age pops keep the frontier
  // fair and reach the DEEP releases (x*0 = 0 needs the 4-condition
  // distributivity joins).  THVM_ATP_HORN_RATIO overrides (0..4).
  {
    // The CD lane is pure smallest-first: its size cap already bounds
    // every fragment, so the age pops that keep v1's frontier fair
    // would only dilute the small-lemma order Otter's CD runs rely on.
    int ratio = g_horn_cd ? 4 : 1;
    const char *re = getenv("THVM_ATP_HORN_RATIO");
    if (re != NULL && re[0] != '\0') ratio = atoi(re);
    if (ratio < 0) ratio = 0;
    if (ratio > 4) ratio = 4;
    int age_cursor = 0;
    long n_pops = 0;
    int gh_admitted = 0;
    for (;;) {
      while (!g_horn_stop) {
        // Phase 2 of the gated-KB wall: the generality-gated cone ran
        // 70% of the budget without closing -- activate the goalward
        // head-closure rules (the definition-shaped bridges generality
        // can never trigger, e.g. CSR040's firstordercollection ->
        // fixedordercollection) and replay every live fact as given so
        // the new rules see the old units (dedup short-circuits the
        // replayed joins, exactly like CD deepening).
        if (g_horn_gate_on && !gh_admitted && !g_horn_goal_reached &&
            (n_pops & 255) == 0 &&
            horn_budget_now() - g_horn_t0 > 0.4 * g_horn_wall) {
          gh_admitted = 1;
          int newr = 0;
          for (int r = 0; r < g_hr_n; r++)
            if (g_hr[r].ghrel && !g_hr[r].rel) { g_hr[r].rel = 1; newr++; }
          if (newr > 0) {
            // Replay INPUT facts only: phase-1 releases stay passive
            // join partners.  A full replay re-pops the phase-1 flood
            // (hundreds of thousands of small releases) ahead of the
            // bridge-crossing input facts and the remaining wall never
            // reaches them; the inputs alone re-fire the newly admitted
            // rules (CSR040: firstordercollection(c) is an input), and
            // each new conclusion enters the agenda as usual.
            for (int f = 0; f < g_horn_input_nf; f++)
              if (!g_hf[f].dead && g_hf[f].processed) {
                g_hf[f].processed = 0;
                g_hf[f].given = 0;
                horn_heap_push(&g_hheap, f);
              }
            age_cursor = 0;
            fprintf(stderr, "FindProof: Horn head-closure phase 2: "
                    "%d rule(s) admitted, input agenda replayed\n", newr);
          }
        }
        int given = -1;
        // Goal-directed pops preempt the ratio scheme: a fact that
        // unifies with a backward subgoal is a candidate step of a
        // goal-closing chain -- joining it first produces the parent
        // subgoal's instance next, walking the cone up to the goal.
        // g_horn_steer keeps this honest: the queue is only ever fed
        // for existential/relevance-gated goals, so the baseline
        // agenda that reaches ground goals on its own (LCL192) stays
        // undiluted.
        do { given = horn_heap_pop(&g_hpheap); }
        while (given >= 0 && (g_hf[given].processed || g_hf[given].dead));
        if (given < 0 && (n_pops++ & 3) >= ratio) {
          while (age_cursor < g_hf_n &&
                 (g_hf[age_cursor].processed || g_hf[age_cursor].dead))
            age_cursor++;
          if (age_cursor < g_hf_n) given = age_cursor;
        }
        if (given < 0) {
          do { given = horn_heap_pop(&g_hheap); }
          while (given >= 0 && (g_hf[given].processed || g_hf[given].dead));
        }
        if (given < 0) {
          while (age_cursor < g_hf_n &&
                 (g_hf[age_cursor].processed || g_hf[age_cursor].dead))
            age_cursor++;
          if (age_cursor >= g_hf_n) break;   // fixpoint: agenda exhausted
          given = age_cursor;
        }
        // CD lane, lazy back subsumption: a fact a LATER release
        // generalizes is dead weight -- its joins only re-derive
        // instances the subsumer covers.  Checked at pop (the only
        // point it would cost anything), tail-scanned (older facts
        // were checked forward at this fact's emit).
        if (g_horn_cd && horn_cd_pop_subsumed(given)) {
          g_hf[given].dead = 1;
          if (given >= g_horn_input_nf)
            g_hrel[given - g_horn_input_nf].dead = 1;
          g_cd_bsub++;
          continue;
        }
        g_hf[given].processed = 1;
        g_hf[given].given = 1;
        horn_given_dispatch(tp, given);
      }
      // CD lane, iterative deepening: the capped space saturated
      // without the goal and the cap (not true saturation) pruned --
      // raise it and re-run every live fact as given again.  The
      // dedup set short-circuits the replayed joins; only conclusions
      // the old cap rejected are new.
      if (g_horn_cd && !g_horn_stop && !g_horn_goal_reached && g_cd_overcap &&
          horn_budget_now() - g_horn_t0 < g_horn_wall) {
        g_cd_cap += g_cd_step;
        g_cd_overcap = 0;
        g_cd_rounds++;
        for (int f = 0; f < g_hf_n; f++)
          if (!g_hf[f].dead && g_hf[f].processed) {
            g_hf[f].processed = 0;
            g_hf[f].given = 0;
            horn_heap_push(&g_hheap, f);
          }
        age_cursor = 0;
        continue;
      }
      // Iterative widening: the RELEVANT closure saturated without the
      // goal and budget remains -- relax the gate, re-run the cone over
      // the newly admitted rules, and continue the loop.
      if (g_horn_stop || !g_horn_gate_on || g_horn_goal_reached) break;
      if (g_horn_gate_tol > 100.0) break;
      g_horn_gate_tol *= 3.0;
      horn_relevance_gate(tp, g_horn_gate_tol, 1);
      if (!gh_admitted) {
        // The relax admitted the head-closure rules: replay the input
        // givens so the new rules see the old units (see phase 2).
        gh_admitted = 1;
        for (int f = 0; f < g_horn_input_nf; f++)
          if (!g_hf[f].dead && g_hf[f].processed) {
            g_hf[f].processed = 0;
            g_hf[f].given = 0;
            horn_heap_push(&g_hheap, f);
          }
      }
      if (g_horn_steer && g_hg_n > 0 && g_hsg_n < g_hsg_max) {
        g_hsg_stop = 0;
        double cd = horn_budget_now() + 0.25 * g_horn_wall;
        g_hsg_deadline = (cd < g_horn_t0 + g_horn_wall)
                           ? cd : g_horn_t0 + g_horn_wall;
        horn_backward_cone(tp);
      }
      age_cursor = 0;   // rescan: parked facts may have re-entered
    }
  }
  // Rank + append.  Under the engine cap keep everything that fits;
  // over it (SInE will run) hand selection up to 256 ranked releases.
  {
    // Goal subsumption: a release some instance of which IS a goal
    // equation closes the refutation in one rewrite -- rank it first.
    // The join's bindings are all undone here, so slot spaces 0
    // (release canonical vars) and HORN_VSPACE (goal vars) are clean.
    // Goals were already normalized into the releases' (expanded)
    // alphabet before the loop.
    int n_goal_hit = 0;
    // Goal-symbol relevance (SInE flavor): the DISCRIMINATING goal
    // symbols are those NOT ubiquitous across the input (frequency
    // <= max(2, n/4) clauses) -- a release mentioning them speaks
    // about the goal's constants and outranks the anonymous
    // variable-lemma flood.
    u32 *gen = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
    u32 *stamp = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
    u8  *gsym = (u8 *)calloc((size_t)TP_MAX_SYMS, 1);
    if (gen == NULL || stamp == NULL || gsym == NULL) tp_giveup("OOM in Horn release");
    for (int c = 0; c < n; c++) {
      if (pl[c] < 0) continue;
      int st[512], sp = 0;
      st[sp++] = pl[c]; st[sp++] = prr[c];
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (stamp[g_hn[t].head] != (u32)(c + 1)) {
            stamp[g_hn[t].head] = (u32)(c + 1);
            gen[g_hn[t].head]++;
          }
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
    }
    u32 ubiq = (u32)((n > 8) ? n / 4 : 2);
    for (int g = 0; g < g_hg_n; g++) {
      int st[512], sp = 0;
      st[sp++] = g_hg_l[g]; st[sp++] = g_hg_r[g];
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (gen[g_hn[t].head] <= ubiq) gsym[g_hn[t].head] = 1;
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
    }
    for (int i = 0; i < g_hrel_n; i++) {
      HRel *re = &g_hrel[i];
      int st[512], sp = 0, seen_n = 0;
      int seen[64];
      st[sp++] = re->tl; st[sp++] = re->tr;
      while (sp > 0) {
        int t = st[--sp];
        if (g_hn[t].head >= 0) {
          if (gsym[g_hn[t].head]) {
            int dup = 0;
            for (int k = 0; k < seen_n; k++) dup |= (seen[k] == g_hn[t].head);
            if (!dup && seen_n < 64) seen[seen_n++] = g_hn[t].head;
          }
          for (int k = 0; k < g_hn[t].nargs && sp < 512; k++)
            st[sp++] = HARG(t, k);
        }
      }
      re->goal_syms = seen_n;
      re->grp = (g_hn[re->tl].head >= 0) ? g_hn[re->tl].head
                                         : g_hn[re->tr].head;
      if (re->grp < 0) re->grp = 0;
      for (int g = 0; g < g_hg_n && !re->goal_hit; g++) {
        for (int orient = 0; orient < 2 && !re->goal_hit; orient++) {
          int gl = orient ? g_hg_r[g] : g_hg_l[g];
          int gr = orient ? g_hg_l[g] : g_hg_r[g];
          int mark = g_htrail_n;
          if (horn_unify(re->tl, 0, gl, HORN_VSPACE, 0) &&
              horn_unify(re->tr, 0, gr, HORN_VSPACE, 0)) re->goal_hit = 1;
          horn_undo(mark);
        }
      }
      n_goal_hit += re->goal_hit;
    }
    free(gen); free(stamp); free(gsym);
    if (n_goal_hit > 0)
      fprintf(stderr, "FindProof: Horn release: %d goal-subsuming unit(s)\n",
              n_goal_hit);
    int n_pos = 0;
    for (int c = 0; c < tp->n_cls; c++) n_pos += !tp->cls[c].neg;
    int room = (n_pos < (int)PR_MAX_EQNS) ? (int)PR_MAX_EQNS - n_pos
                                          : (int)PR_MAX_EQNS;
    {
      const char *ke = getenv("THVM_ATP_HORN_KEEP");
      const char *ge = getenv("THVM_ATP_HORN_GROUND");
      if (ke != NULL && atoi(ke) > 0 && atoi(ke) < room) room = atoi(ke);
      g_horn_rank_ground = (ge != NULL && ge[0] != '\0' && ge[0] != '0');
    }
    // --proof: released units carry their hyper-resolution derivation
    // record (PROV_HORN) and emit it as the proof's cnf(h<k>, ...)
    // chain, so keeping them is J13-compliant.  A release whose record
    // is TRUNCATED (capture caps hit, or an unrecorded ancestor) is
    // excluded instead -- an axiom leaf citing file() for a derived
    // unit would misrepresent it as a problem formula.
    if (g_hrel_n > room) {
      qsort(g_hrel, (size_t)g_hrel_n, sizeof(HRel), horn_rel_cmp);
      u32 *taken = (u32 *)calloc((size_t)TP_MAX_SYMS, sizeof(u32));
      if (taken == NULL) tp_giveup("OOM in Horn release");
      for (int i = 0; i < g_hrel_n; i++)
        g_hrel[i].w = (int)taken[g_hrel[i].grp]++;
      free(taken);
      qsort(g_hrel, (size_t)g_hrel_n, sizeof(HRel), horn_rel_cmp_rr);
    }
    int n_prov_trunc = 0;
    for (int i = 0, kept = 0; i < g_hrel_n && kept < room; i++) {
      if (g_hrel[i].dead) continue;   // back-subsumed (CD lane)
      if (g_prov_on && !horn_prov_ok(g_hrel[i].prov)) {
        n_prov_trunc++;
        continue;
      }
      if (tp->n_cls >= TP_MAX_CLAUSES) break;
      TpClause *cl = &tp->cls[tp->n_cls++];
      snprintf(cl->lhs, sizeof cl->lhs, "%s", g_hrel[i].l);
      snprintf(cl->rhs, sizeof cl->rhs, "%s", g_hrel[i].r);
      cl->neg = 0; cl->conj = 0;
      cl->prov_kind = g_prov_on ? PROV_HORN : PROV_NONE;
      cl->prov_ix = g_prov_on ? g_hrel[i].prov : 0;
      tp_scan_term(tp, cl->lhs);
      tp_scan_term(tp, cl->rhs);
      released_kept++;
      kept++;
    }
    if (n_prov_trunc > 0)
      fprintf(stderr, "FindProof: Horn release: %d kept release(s) lack a "
              "complete derivation record -- excluded from --proof input\n",
              n_prov_trunc);
  }
  if (released_kept > 0) g_cli_horn_applied = 1;
  {
    int n_sg_hit = 0;
    for (int i = 0; i < g_hrel_n; i++) n_sg_hit += g_hrel[i].sg_hit;
    fprintf(stderr, "FindProof: Horn release: %d dispatcher(s), %d rule(s), "
            "%d fact(s), %d definition(s), %d subgoal(s), %d released "
            "(%d sg-hit%s), %d kept\n",
            n_disp, n_rules, g_hf_n - g_hrel_n, n_defs, g_hsg_n, g_hrel_n,
            n_sg_hit, g_horn_goal_reached ? ", goal reached" : "",
            released_kept);
    if (g_horn_cd)
      fprintf(stderr, "FindProof: Horn CD: cap %d after %d deepening "
              "round(s), %ld fwd-subsumed, %ld back-subsumed, %ld "
              "resonant, %.1fs\n",
              g_cd_cap, g_cd_rounds, g_cd_fsub, g_cd_bsub, g_cd_reso_hits,
              horn_now() - g_horn_t0_wall);
  }
done:
  free(pl); free(prr); free(pnv); free(is_disp); free(ps);
  free(g_horn_disp); g_horn_disp = NULL;
  free(g_hn); g_hn = NULL; g_hn_n = g_hn_cap = 0;
  free(g_ha); g_ha = NULL; g_ha_n = g_ha_cap = 0;
  free(g_hf); g_hf = NULL; g_hf_n = g_hf_cap = 0;
  free(g_dtn); g_dtn = NULL; g_dtn_n = g_dtn_cap = 0;
  free(g_dte); g_dte = NULL; g_dte_n = g_dte_cap = 0;
  free(g_dtl); g_dtl = NULL; g_dtl_n = g_dtl_cap = 0;
  free(g_hix); g_hix = NULL; g_hix_n = g_hix_cap = 0;
  free(g_hix_head); g_hix_head = NULL; g_hix_varhead = -1;
  free(g_hr); g_hr = NULL; g_hr_n = g_hr_cap = 0;
  free(g_hheap.a);  g_hheap.a = NULL;  g_hheap.n = g_hheap.cap = 0;
  free(g_hpheap.a); g_hpheap.a = NULL; g_hpheap.n = g_hpheap.cap = 0;
  free(g_hsg); g_hsg = NULL; g_hsg_n = g_hsg_cap = 0;
  free(g_hdef_l); g_hdef_l = NULL;
  free(g_hdef_r); g_hdef_r = NULL;
  free(g_hdef_cls); g_hdef_cls = NULL;
  free(g_hpc_of); g_hpc_of = NULL;
  free(g_hrix); g_hrix = NULL; g_hrix_n = g_hrix_cap = 0;
  free(g_hrix_head); g_hrix_head = NULL; g_hrix_varhead = -1;
  if (g_hdup != NULL) {
    for (u32 i = 0; i < g_hdup_slots; i++) free(g_hdup[i]);
    free(g_hdup); g_hdup = NULL; g_hdup_n = 0;
  }
  // A recorded release's l/r strings are aliased by its g_hpr record,
  // which the proof emitter reads after the pass -- ownership moves
  // there; unrecorded releases free theirs here.
  for (int i = 0; i < g_hrel_n; i++) {
    if (g_hrel[i].prov >= 0) continue;
    free(g_hrel[i].l); free(g_hrel[i].r);
  }
  free(g_hrel); g_hrel = NULL; g_hrel_n = g_hrel_cap = 0;
}

