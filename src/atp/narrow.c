// === 8.10b: top-K CP peek ==========================================
//
// Copies the top K cheapest CPs' (lhs, rhs) into the caller's
// buffers WITHOUT modifying the queue.  "Cheapest" is the same
// (cp_pri, cp_seq) order the heap pops in, so peek[0] always
// equals the next `thvm_atp_select_cp` result.
//
// Caller-side use case: see top-K candidates before deciding
// which (if any) to commit; useful for branching CP selectors,
// multi-CP batch heuristics, lookahead.

// (pri, seq, idx) triple sorted to realize the peek order.
typedef struct { u32 pri; u32 seq; u32 idx; } AtpPeekEnt;
static int atp_peek_cmp(const void *a, const void *b) {
  const AtpPeekEnt *x = (const AtpPeekEnt *)a;
  const AtpPeekEnt *y = (const AtpPeekEnt *)b;
  if (x->pri != y->pri) return (x->pri < y->pri) ? -1 : 1;
  if (x->seq != y->seq) return (x->seq < y->seq) ? -1 : 1;
  return 0;
}

fn u32 thvm_atp_peek_top_k(AtpState *s, u32 k,
                           Term *out_lhs, Term *out_rhs) {
  if (s == NULL || s->n_cps == 0) return 0;
  if (k > s->n_cps) k = s->n_cps;
  if (k == 0) return 0;

  // The heap array is ordered by the heap invariant, not fully
  // sorted -- so copy the (pri, seq, idx) triples and sort them
  // by the queue's selection key to read off the top K.
  AtpPeekEnt *ent = (AtpPeekEnt *)malloc((size_t)s->n_cps * sizeof(AtpPeekEnt));
  if (ent == NULL) return 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    // idx stores the payload ROW (slot_read target below); the qsort
    // key (pri, seq) is total, so the fill order is immaterial.
    u32 vv = s->cp_heap[i].row;
    ent[i].pri = s->cp_pri[vv];
    ent[i].seq = s->cp_seq[vv];
    ent[i].idx = vv;
  }
  qsort(ent, s->n_cps, sizeof(AtpPeekEnt), atp_peek_cmp);
  for (u32 i = 0; i < k; i++) {
    atp_cp_slot_read(s, ent[i].idx, &out_lhs[i], &out_rhs[i]);
  }
  free(ent);
  return k;
}

// === Narrowing proof recorder ======================================
//
// See the AtpState.nrec block comment in thvm.h.  Both narrowing
// lanes (the greedy walk and the critical-goal queue) record COMPACT
// step descriptors -- trace id of the equation used, goal side, redex
// path, face direction -- never Terms, so the recorder needs no GC
// rooting and retains no heap.  The CLI emitter replays the winning
// derivation from the pristine conjecture pair; every emitted term is
// re-derived at emission, so a recording defect can only cost the
// proof (marker fallback), never its soundness.
//
// Layout: one append-only step arena.  The greedy lane's chain is the
// arena prefix in order (nr->greedy).  The queue lane commits one
// SEGMENT per stored critical goal -- [NR_NARROW, NR_NORM*, NR_CANON]
// -- plus the closing segment [NR_NARROW, NR_NORM*, NR_CLOSE]; the
// winning lineage is the parent chain from the closing segment up to
// the seed (parent NR_ROOT).  A segment that cannot be recorded
// (depth/caps) commits as NR_NONE: search proceeds identically, and
// emission falls back to the marker only if the winning lineage
// crosses it.

#define NR_NONE      0xFFFFFFFFu
#define NR_ROOT      0xFFFFFFFEu
#define NR_STEP_MAX  (1u << 21)     // arena hard cap (~40B per step)
#define NR_SCR_CAP   4096u          // steps per in-flight admit

enum { NR_NARROW = 0u, NR_NORM = 1u, NR_CLOSE = 2u, NR_CANON = 3u };

typedef struct {
  u32 trace;                        // trace id of the equation used
  u8  kind, side, fwd, pos_len;
  u8  pos[ATP_PROOF_MAX_DEPTH];
} NrStep;

typedef struct AtpNrec {
  NrStep *steps; u32 n, cap;        // committed step arena
  u32 *cg_parent, *cg_off, *cg_len; // per stored critical goal
  u32  n_cg, cg_cap;
  NrStep scr[NR_SCR_CAP];           // in-flight admit scratch
  u32  sn;
  u8   scr_bad;                     // this admit's segment unrecordable
  u32  pending_parent;              // CG being narrowed (NR_ROOT = seed)
  u32  close_parent, close_off, close_len;
  u8   closed;
  u8   greedy;                      // arena prefix is the greedy chain
  u8   lost;                        // greedy chain broken: no proof
} AtpNrec;

static AtpNrec *nrec_get(AtpState *s) {
  if (s->nrec == NULL) {
    AtpNrec *q = (AtpNrec *)calloc(1u, sizeof(AtpNrec));
    if (q == NULL) return NULL;
    q->pending_parent = NR_ROOT;
    q->close_parent   = NR_NONE;
    s->nrec = (struct AtpNrec *)q;
  }
  return (AtpNrec *)s->nrec;
}

fn void thvm_atp_nrec_free(AtpState *s) {
  AtpNrec *q = (AtpNrec *)s->nrec;
  if (q == NULL) return;
  free(q->steps);
  free(q->cg_parent); free(q->cg_off); free(q->cg_len);
  free(q);
  s->nrec = NULL;
}

// Recording armed for this state?  (The greedy lane additionally
// gates on narrow_rec_active -- thvm_atp_narrow_step is public API.)
static u8 nrec_on(const AtpState *s) {
  return (u8)(s->record_narrow != 0u);
}

// TRACE id of live rule slot k, or NR_NONE (recording then degrades
// to a broken segment / lost chain -- never to a wrong citation:
// emission replays every step against the loaded trace equation).
static u32 nrec_slot_trace(const AtpState *s, u32 k) {
  if (s->r_trace == NULL || k >= s->n_rules) return NR_NONE;
  u32 t = s->r_trace[k];
  return (t == ATP_TRACE_NONE) ? NR_NONE : t;
}

static void nrec_fill(NrStep *st, u32 kind, u32 trace, u32 side, u8 fwd,
                      const u32 *pos, u32 pos_len) {
  st->trace   = trace;
  st->kind    = (u8)kind;
  st->side    = (u8)side;
  st->fwd     = fwd;
  st->pos_len = (u8)pos_len;
  for (u32 i = 0; i < pos_len; i++) st->pos[i] = (u8)pos[i];
}

// Append one step to the arena (greedy chain).  Failure latches lost.
static void nrec_push(AtpState *s, u32 kind, u32 trace, u32 side, u8 fwd,
                      const u32 *pos, u32 pos_len) {
  AtpNrec *q = nrec_get(s);
  if (q == NULL || q->lost) { if (q != NULL) q->lost = 1u; return; }
  q->greedy = 1u;
  if ((kind == NR_NARROW || kind == NR_NORM) && trace == NR_NONE) {
    q->lost = 1u;
    return;
  }
  if (pos_len > ATP_PROOF_MAX_DEPTH || q->n >= NR_STEP_MAX) {
    q->lost = 1u;
    return;
  }
  if (q->n == q->cap) {
    u32 nc = q->cap ? q->cap * 2u : 1024u;
    NrStep *ns = (NrStep *)realloc(q->steps, (size_t)nc * sizeof(NrStep));
    if (ns == NULL) { q->lost = 1u; return; }
    q->steps = ns; q->cap = nc;
  }
  nrec_fill(&q->steps[q->n++], kind, trace, side, fwd, pos, pos_len);
}

// Scratch ops (queue lane): reset per admit, push, commit on store.
static void nrec_scr_reset(AtpState *s, u32 parent) {
  AtpNrec *q = nrec_get(s);
  if (q == NULL) return;
  q->sn = 0u;
  q->scr_bad = 0u;
  q->pending_parent = parent;
}

static void nrec_scr_push(AtpState *s, u32 kind, u32 trace, u32 side,
                          u8 fwd, const u32 *pos, u32 pos_len) {
  AtpNrec *q = nrec_get(s);
  if (q == NULL || q->scr_bad) { if (q != NULL) q->scr_bad = 1u; return; }
  if ((kind == NR_NARROW || kind == NR_NORM) && trace == NR_NONE) {
    q->scr_bad = 1u;
    return;
  }
  if (pos_len > ATP_PROOF_MAX_DEPTH || q->sn >= NR_SCR_CAP) {
    q->scr_bad = 1u;
    return;
  }
  nrec_fill(&q->scr[q->sn++], kind, trace, side, fwd, pos, pos_len);
}

// Commit the scratch segment to the arena; returns the offset or
// NR_NONE (bad/overflowing segments commit as broken -- the search is
// never perturbed by recording pressure).
static u32 nrec_scr_commit(AtpState *s) {
  AtpNrec *q = (AtpNrec *)s->nrec;
  if (q == NULL || q->scr_bad || q->n + q->sn > NR_STEP_MAX) return NR_NONE;
  while (q->n + q->sn > q->cap) {
    u32 nc = q->cap ? q->cap * 2u : 1024u;
    NrStep *ns = (NrStep *)realloc(q->steps, (size_t)nc * sizeof(NrStep));
    if (ns == NULL) return NR_NONE;
    q->steps = ns; q->cap = nc;
  }
  u32 off = q->n;
  for (u32 i = 0; i < q->sn; i++) q->steps[q->n++] = q->scr[i];
  return off;
}

// Register stored CG idx's segment (parent + committed slice).
static void nrec_cg_store(AtpState *s, u32 idx, u32 off, u32 len) {
  AtpNrec *q = (AtpNrec *)s->nrec;
  if (q == NULL) return;
  if (idx >= q->cg_cap) {
    u32 nc = q->cg_cap ? q->cg_cap : 1024u;
    while (nc <= idx) nc *= 2u;
    u32 *np = (u32 *)realloc(q->cg_parent, (size_t)nc * sizeof(u32));
    u32 *no = (u32 *)realloc(q->cg_off, (size_t)nc * sizeof(u32));
    u32 *nl = (u32 *)realloc(q->cg_len, (size_t)nc * sizeof(u32));
    if (np != NULL) q->cg_parent = np;
    if (no != NULL) q->cg_off = no;
    if (nl != NULL) q->cg_len = nl;
    if (np == NULL || no == NULL || nl == NULL) return;   // idx unrecorded
    for (u32 i = q->cg_cap; i < nc; i++) {
      q->cg_parent[i] = NR_NONE;
      q->cg_off[i] = NR_NONE;
      q->cg_len[i] = 0u;
    }
    q->cg_cap = nc;
  }
  q->cg_parent[idx] = q->pending_parent;
  q->cg_off[idx] = off;
  q->cg_len[idx] = (off == NR_NONE) ? 0u : len;
  if (idx >= q->n_cg) q->n_cg = idx + 1u;
}

// Single-step recording normalize: the proof extractor's machinery
// (atp_proof_rewrite_step: leftmost-outermost, oriented rules plus
// order-gated unorientable faces), each step pushed to the scratch.
// A different rewrite order from the engine's normalizer, so it only
// ever runs as a CHECKED side computation (see nrec_normalize): its
// chain is kept when it lands on the engine's normal form.
static Term nrec_normalize_rec(AtpState *s, Term t, u32 side) {
  for (u32 it = 0; it < ATP_GOAL_NORM_CAP; it++) {
    u8  pos8[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0u;
    u8  pos_len = 0u, fwd = 1u, fired = 0u;
    g_atp_proof_oriented_only = 0u;
    Term t2 = atp_proof_rewrite_step(s, t, pos8, 0u, &rule, &pos_len,
                                     &fwd, &fired);
    if (!fired || kbo_eq(t, t2)) return t;
    u32 pos[ATP_PROOF_MAX_DEPTH];
    for (u8 i = 0; i < pos_len; i++) pos[i] = pos8[i];
    nrec_scr_push(s, NR_NORM, nrec_slot_trace(s, rule), side, fwd,
                  pos, pos_len);
    t = t2;
  }
  return t;
}

static void nrec_scr_mark_bad(AtpState *s) {
  AtpNrec *q = (AtpNrec *)s->nrec;
  if (q != NULL) q->scr_bad = 1u;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Mixmost splice sink (g_ft_step_sink): one NR_NORM per splice, cited
// by the fired slot's trace id -- the step the emitter replays.
static void nrec_ft_sink(AtpState *s, u8 side, u32 rule, u8 dir,
                         const u8 *pos, u8 pos_len, int have_pos) {
  if (!have_pos) {
    nrec_scr_mark_bad(s);
    return;
  }
  u32 p[ATP_PROOF_MAX_DEPTH];
  for (u8 i = 0; i < pos_len; i++) p[i] = pos[i];
  nrec_scr_push(s, NR_NORM, nrec_slot_trace(s, rule), side,
                (u8)(dir == 0u), p, pos_len);
}
#endif

// Recording normalize (queue lane, recording runs only).  The search
// sees exactly the unrecorded normal form: the engine's own normalizer
// runs unchanged and its mixmost walk reports every splice to
// nrec_ft_sink.  A normalizer without a splice log gets a chain from
// the single-step recorder inside a heap bracket, kept only when it
// reaches the same normal form; otherwise the segment is unrecordable
// (the emitter falls back to the marker if the winning lineage needs it).
static Term nrec_normalize(AtpState *s, Term t, u32 side) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (s->use_mixmost_nf) {
    g_ft_step_sink      = nrec_ft_sink;
    g_ft_step_sink_side = (u8)side;
    Term nf = atp_rewrite_normalize(s, t, s->lhs, s->rhs, s->n_rules,
                                    ATP_GOAL_NORM_CAP);
    u8 logged = (u8)(g_ft_step_sink == NULL);
    g_ft_step_sink = NULL;
    if (!logged && !kbo_eq(nf, t)) nrec_scr_mark_bad(s);
    return nf;
  }
#endif
  Term nf = atp_rewrite_normalize(s, t, s->lhs, s->rhs, s->n_rules,
                                  ATP_GOAL_NORM_CAP);
  u64 hp = thvm_atp_heap_checkpoint();
  u64 g0 = gc_count();
  Term rec = nrec_normalize_rec(s, t, side);
  if (!kbo_eq(rec, nf)) nrec_scr_mark_bad(s);
  if (gc_count() == g0) thvm_atp_heap_reset(hp);
  return nf;
}

fn void thvm_atp_set_record_narrow(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_narrow = on ? 1u : 0u;
}

// === 8.9b: narrowing primitives ====================================

// One-shot narrow visitor: tries each rule at the current position,
// commits the first successful unification.  `success` flag stops
// the walk after a hit.
typedef struct {
  AtpState     *s;
  Term          side;        // currently being narrowed
  Term          other;       // the other side (sigma is applied here too)
  Term         *out_side;
  Term         *out_other;
  RewriteSubst *witness;
  u8            success;
  u8            side_ix;     // 0 = narrowing lhs, 1 = narrowing rhs
} NarrowCtx;

static u32 narrow_visit(const u32 *p, u32 p_len, void *raw) {
  NarrowCtx *ctx = (NarrowCtx *)raw;
  if (ctx->success) return 0;

  Term sub = cp_subterm_at(ctx->side, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;

  for (u32 k = 0; k < ctx->s->n_rules; k++) {
    Term fl = ctx->s->lhs[k], fr = ctx->s->rhs[k];
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Lazy slots (TAG_NUM sentinels, live only as FT mirrors) never
    // unify as sentinels -- materialize them, or the greedy walk is
    // structurally blind to every lazy rule.
    if (term_tag(fl) == TAG_NUM || term_tag(fr) == TAG_NUM) {
      fl = atp_rule_term(ctx->s, k, 0u);
      fr = atp_rule_term(ctx->s, k, 1u);
    }
#endif
    if (fl == 0 || fr == 0 ||
        term_tag(fl) == TAG_NUM || term_tag(fr) == TAG_NUM) continue;
    Term lj = thvm_rename_vars(fl, CP_RENAME_OFFSET);
    Term rj = thvm_rename_vars(fr, CP_RENAME_OFFSET);
    RewriteSubst subst = {{0}};
    if (!thvm_unify(sub, lj, &subst)) continue;

    // Narrow: replace the subterm at p with the rule's RHS,
    // then sigma-apply across both sides.
    Term replaced  = cp_replace_at(ctx->side, p, p_len, rj);
    *ctx->out_side  = thvm_unify_apply(replaced, &subst);
    *ctx->out_other = thvm_unify_apply(ctx->other, &subst);

    // Accumulate sigma into witness.  No composition step
    // needed: previous-step sigmas have already been applied
    // to side/other before this call, so each new binding lives
    // in the post-sigma universe.
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
      if (subst.bindings[i] != 0) {
        ctx->witness->bindings[i] = subst.bindings[i];
      }
    }
    // Proof recording (scoped to goal_check's greedy loop): the step
    // is fully determined by (rule trace id, side, position) -- the
    // emitter re-unifies at replay.  Greedy uses the l->r face only.
    // Off the work clock (atp_work_pause), in the no-proof run too.
    if (ctx->s->narrow_rec_active) {
      atp_work_pause();
      if (nrec_on(ctx->s))
        nrec_push(ctx->s, NR_NARROW, nrec_slot_trace(ctx->s, k),
                  ctx->side_ix, 1u, p, p_len);
      atp_work_resume();
    }
    ctx->success = 1;
    return 0;
  }
  return 0;
}

fn u8 thvm_atp_narrow_step(AtpState *s, Term lhs, Term rhs,
                           Term *out_lhs, Term *out_rhs,
                           RewriteSubst *witness) {
  if (s == NULL || s->n_rules == 0) return 0;
  if (out_lhs == NULL || out_rhs == NULL || witness == NULL) return 0;

  NarrowCtx ctx;
  ctx.s        = s;
  ctx.witness  = witness;
  ctx.success  = 0;

  u32 path[CP_MAX_DEPTH];

  // Try narrowing on lhs first.
  ctx.side       = lhs;
  ctx.other      = rhs;
  ctx.out_side   = out_lhs;
  ctx.out_other  = out_rhs;
  ctx.side_ix    = 0u;
  cp_walk_positions(lhs, path, 0, CP_MAX_DEPTH, narrow_visit, &ctx, 0);
  if (ctx.success) return 1;

  // Then rhs.
  ctx.side       = rhs;
  ctx.other      = lhs;
  ctx.out_side   = out_rhs;
  ctx.out_other  = out_lhs;
  ctx.side_ix    = 1u;
  cp_walk_positions(rhs, path, 0, CP_MAX_DEPTH, narrow_visit, &ctx, 0);
  return ctx.success;
}

fn Term thvm_atp_get_witness(const AtpState *s, u32 var_id) {
  if (s == NULL || var_id >= REWRITE_MAX_VAR) return 0;
  return s->witness_subst.bindings[var_id];
}

// === Stage 9.1b: bounded DFS multi-witness narrowing ================
// Enumerates up to N witnesses by recursively trying every (position,
// rule) choice at each node.  Stateless w.r.t. AtpState; populates
// the caller's RewriteSubst array directly.

typedef struct {
  AtpState     *s;
  RewriteSubst *witnesses;
  u32           max_witnesses;
  u32           found;
  u32           max_depth;
} NarrowAllCtx;

typedef struct {
  NarrowAllCtx *ctx;
  Term          side;       // currently being narrowed
  Term          other;      // the other side (sigma is applied here too)
  RewriteSubst  acc;        // accumulator at this DFS frame
  u32           depth;
  u8            narrowing_lhs;  // 1 = narrow side==lhs; 0 = narrow side==rhs
} NarrowAllVisitor;

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc,
                           u32 depth);

static u32 narrow_all_visit(const u32 *p, u32 p_len, void *raw) {
  NarrowAllVisitor *v = (NarrowAllVisitor *)raw;
  if (v->ctx->found >= v->ctx->max_witnesses) return 0;

  Term sub = cp_subterm_at(v->side, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;

  for (u32 k = 0; k < v->ctx->s->n_rules; k++) {
    if (v->ctx->found >= v->ctx->max_witnesses) return 0;

    Term lj = thvm_rename_vars(v->ctx->s->lhs[k], CP_RENAME_OFFSET);
    Term rj = thvm_rename_vars(v->ctx->s->rhs[k], CP_RENAME_OFFSET);
    RewriteSubst subst = {{0}};
    if (!thvm_unify(sub, lj, &subst)) continue;

    Term replaced  = cp_replace_at(v->side, p, p_len, rj);
    Term new_side  = thvm_unify_apply(replaced,  &subst);
    Term new_other = thvm_unify_apply(v->other,  &subst);

    // Compose sigma into a fresh accumulator copy: each branch sees
    // its own bindings, siblings stay independent.
    RewriteSubst new_acc = v->acc;
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
      if (subst.bindings[i] != 0) new_acc.bindings[i] = subst.bindings[i];
    }

    if (v->narrowing_lhs)
      narrow_all_dfs(v->ctx, new_side, new_other, &new_acc, v->depth + 1);
    else
      narrow_all_dfs(v->ctx, new_other, new_side, &new_acc, v->depth + 1);
  }
  return 0;
}

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc,
                           u32 depth) {
  if (ctx->found >= ctx->max_witnesses) return;
  if (kbo_eq(lhs, rhs)) {
    ctx->witnesses[ctx->found++] = *acc;
    return;
  }
  if (depth >= ctx->max_depth) return;

  u32 path[CP_MAX_DEPTH];
  NarrowAllVisitor v;
  v.ctx   = ctx;
  v.acc   = *acc;
  v.depth = depth;

  // Narrow on lhs first.
  v.side          = lhs;
  v.other         = rhs;
  v.narrowing_lhs = 1;
  cp_walk_positions(lhs, path, 0, CP_MAX_DEPTH, narrow_all_visit, &v, 0);
  if (ctx->found >= ctx->max_witnesses) return;

  // Then rhs.
  v.side          = rhs;
  v.other         = lhs;
  v.narrowing_lhs = 0;
  cp_walk_positions(rhs, path, 0, CP_MAX_DEPTH, narrow_all_visit, &v, 0);
}

fn u32 thvm_atp_narrow_all(AtpState *s,
                           Term lhs, Term rhs,
                           u32 max_depth, u32 max_witnesses,
                           RewriteSubst *witnesses) {
  if (s == NULL || witnesses == NULL || max_witnesses == 0) return 0;

  NarrowAllCtx ctx;
  ctx.s             = s;
  ctx.witnesses     = witnesses;
  ctx.max_witnesses = max_witnesses;
  ctx.found         = 0;
  ctx.max_depth     = max_depth;

  RewriteSubst empty = {{0}};
  narrow_all_dfs(&ctx, lhs, rhs, &empty, 0);
  return ctx.found;
}

// === Critical-goal queue: best-first existential-goal narrowing ====
//
// The missing WM existential lane (PM_Existenzziele + Rueckwaertige
// Ueberlappung: the CG set the -cg/-cgclas/-back stubs above gate) in
// the shape GKC's winning COL011-1 strategy demonstrates
// ({"strategy":["unit"],"query_preference":1}: goal-descendant
// preference, 0.54s where thvm timed out at 240s).  An existential
// conjecture  exists X.. s = t  is refuted by NARROWING: unify a rule
// side into a goal subterm, instantiate both sides, repeat until the
// two sides UNIFY (equality resolution).  COL011-1's derivation is 5
// such steps (Twee's proof: q1_definition x4 + o_definition x1); every
// completion-side lane treats the goal variable universally and
// structurally cannot close it, and the greedy walk in
// thvm_atp_narrow_step cannot either -- it commits the FIRST
// (position, rule) hit, rewrites the goal pair DESTRUCTIVELY (one
// wrong branch loses the goal forever), only ever uses the L->R face
// (COL011-1's opening narrow needs the unorientable o-definition's
// RHS face: no L->R axiom face unifies with apply(combinator,Y) at
// all), and closes on syntactic equality only (the oracle's last step
// is a unification, not an identity).
//
// This lane keeps a STORE of critical goals (narrowed goal variants),
// selected best-first by symbol size with a 1-in-16 FIFO injection
// (GKC weight_select_ratio).  A popped CG narrows against rule faces
// [watermark, n_rules) -- oriented rules contribute their LHS,
// unorientable equations BOTH faces -- in budgeted chunks; a CG that
// has seen every current rule parks in a processed list, and newly
// added rules sweep the processed list incrementally (the backward
// overlap: WM RueckwartigeUeberlappung, GKC's paramodulation of
// derived two-sided lemmas into the goal).  Every derived CG is
// R-normalized, var-canonicalized, deduped, and close-checked by
// UNIFYING its sides.
//
// Soundness: narrowing steps are instance-preserving (if sigma solves
// the child, the accumulated unifier composed with sigma solves the
// parent -- standard narrowing soundness), R-normalization rewrites
// with equational consequences of E, and the close step's mgu makes
// both sides syntactically equal, i.e. a 0-step join of an instance:
// each PROVED verdict witnesses an instance of the original
// existential conjecture derivable from E.  No witness substitution
// is materialized (proof extraction already skips existential goals,
// _.c "Single-NF extraction only"); the greedy lane below remains the
// default so thvm_atp_get_witness consumers are unchanged.
//
// Opt-in: THVM_ATP_NARROW_QUEUE / --narrow-queue /
// thvm_atp_set_use_narrow_queue.  Default OFF: byte-identical
// trajectories everywhere.

#define ATP_CGQ_MAX_CGS      (1u << 18)  // CG store cap
#define ATP_CGQ_MAX_SIZE     120u        // symbols per CG (lhs+rhs)
#define ATP_CGQ_BUDGET       4096u       // unify attempts per goal_check
#define ATP_CGQ_FIFO_MOD     16u         // 1-in-N pops from the FIFO lane
#define ATP_CGQ_HASH_INIT    (1u << 14)

typedef struct AtpCgq {
  // CG store (append-only; index = creation order = FIFO lane order).
  Term *lhs;
  Term *rhs;
  u32  *wm;        // rule watermark: faces [0, wm) already narrowed
  u32  *weight;    // symbol count lhs+rhs (selection key)
  u8   *state;     // 0 = queued (in heap), 1 = processed (parked)
  u32   n, cap;
  // Min-heap of queued CG indices by (weight, index).
  u32  *heap;
  u32   heap_n;
  // Processed list in park order (watermarks non-decreasing), swept
  // by the backward rule cursor.
  u32  *done;
  u32   done_n;
  // Backward sweep cursor: next (rule, done-position) pair.
  u32   bw_rule;
  u32   bw_pos;
  // FIFO lane cursor (next store index to consider for the 1-in-N pop).
  u32   fifo;
  // Dedup: open-addressing hash of var-canonical pairs.
  u64  *hkey;
  u32  *hidx;
  u32   hcap;      // power of two
  u32   hn;
  u64   pops;      // statistics + FIFO-mod counter
  u8    full;      // store cap hit: keep processing, stop enqueuing
  u8    proved;    // latched PROVED verdict
} AtpCgq;

static u32 cgq_term_size(Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u32   sz  = 0u;
  for (;;) {
    sz++;
    if (term_tag(t) == TAG_CTR) {
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
  return sz;
}

static u32 cgq_term_max_var(Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u32   m   = 0u;
  for (;;) {
    if (term_tag(t) == TAG_FVR) {
      if (term_ext(t) + 1u > m) {
        m = term_ext(t) + 1u;
      }
    } else if (term_tag(t) == TAG_CTR) {
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
  return m;
}

// A preorder fold: each node mixes its tag/label, a CTR its arity, then
// its children left to right.
static u64 cgq_term_hash(Term t, u64 h) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    h = (h ^ ((u64)term_tag(t) << 32 ^ (u64)term_ext(t)))
        * 0x100000001B3ull;
    if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      h = (h ^ n) * 0x100000001B3ull;
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
  return h;
}

static AtpCgq *cgq_create(void) {
  AtpCgq *q = (AtpCgq *)calloc(1u, sizeof(AtpCgq));
  if (q == NULL) return NULL;
  q->cap  = 1024u;
  q->lhs    = (Term *)malloc((size_t)q->cap * sizeof(Term));
  q->rhs    = (Term *)malloc((size_t)q->cap * sizeof(Term));
  q->wm     = (u32 *)malloc((size_t)q->cap * sizeof(u32));
  q->weight = (u32 *)malloc((size_t)q->cap * sizeof(u32));
  q->state  = (u8 *)malloc(q->cap);
  q->heap   = (u32 *)malloc((size_t)q->cap * sizeof(u32));
  q->done   = (u32 *)malloc((size_t)q->cap * sizeof(u32));
  q->hcap   = ATP_CGQ_HASH_INIT;
  q->hkey   = (u64 *)calloc(q->hcap, sizeof(u64));
  q->hidx   = (u32 *)malloc((size_t)q->hcap * sizeof(u32));
  if (q->lhs == NULL || q->rhs == NULL || q->wm == NULL ||
      q->weight == NULL || q->state == NULL || q->heap == NULL ||
      q->done == NULL || q->hkey == NULL || q->hidx == NULL) {
    atp_cgq_free(q);
    return NULL;
  }
  return q;
}

static void atp_cgq_free(struct AtpCgq *q) {
  if (q == NULL) return;
  free(q->lhs); free(q->rhs); free(q->wm); free(q->weight);
  free(q->state); free(q->heap); free(q->done);
  free(q->hkey); free(q->hidx);
  free(q);
}

// Heap ops: min by (weight, index).
static u8 cgq_heap_less(const AtpCgq *q, u32 a, u32 b) {
  if (q->weight[a] != q->weight[b]) return q->weight[a] < q->weight[b];
  return a < b;
}
static void cgq_heap_push(AtpCgq *q, u32 idx) {
  u32 i = q->heap_n++;
  q->heap[i] = idx;
  while (i > 0u) {
    u32 p = (i - 1u) / 2u;
    if (!cgq_heap_less(q, q->heap[i], q->heap[p])) break;
    u32 t = q->heap[i]; q->heap[i] = q->heap[p]; q->heap[p] = t;
    i = p;
  }
}
static u32 cgq_heap_pop(AtpCgq *q) {
  u32 top = q->heap[0];
  q->heap[0] = q->heap[--q->heap_n];
  u32 i = 0u;
  for (;;) {
    u32 l = 2u * i + 1u, r = l + 1u, m = i;
    if (l < q->heap_n && cgq_heap_less(q, q->heap[l], q->heap[m])) m = l;
    if (r < q->heap_n && cgq_heap_less(q, q->heap[r], q->heap[m])) m = r;
    if (m == i) break;
    u32 t = q->heap[i]; q->heap[i] = q->heap[m]; q->heap[m] = t;
    i = m;
  }
  return top;
}

// Dedup probe: 1 if the var-canonical pair (l, r) is new (and record
// it under store index idx), 0 if an equal pair is already present.
static u8 cgq_seen_add(AtpCgq *q, Term l, Term r, u32 idx) {
  if (q->hn * 2u >= q->hcap) {   // grow at 50% load
    u32 ncap = q->hcap * 2u;
    u64 *nk = (u64 *)calloc(ncap, sizeof(u64));
    u32 *ni = (u32 *)malloc((size_t)ncap * sizeof(u32));
    if (nk == NULL || ni == NULL) { free(nk); free(ni); return 0u; }
    for (u32 i = 0; i < q->hcap; i++) {
      if (q->hkey[i] == 0ull) continue;
      u32 s2 = (u32)q->hkey[i] & (ncap - 1u);
      while (nk[s2] != 0ull) s2 = (s2 + 1u) & (ncap - 1u);
      nk[s2] = q->hkey[i];
      ni[s2] = q->hidx[i];
    }
    free(q->hkey); free(q->hidx);
    q->hkey = nk; q->hidx = ni; q->hcap = ncap;
  }
  u64 h = cgq_term_hash(r, cgq_term_hash(l, 0xCBF29CE484222325ull));
  if (h == 0ull) h = 1ull;   // 0 marks an empty slot
  u32 s2 = (u32)h & (q->hcap - 1u);
  while (q->hkey[s2] != 0ull) {
    if (q->hkey[s2] == h) {
      u32 j = q->hidx[s2];
      if (kbo_eq(q->lhs[j], l) && kbo_eq(q->rhs[j], r)) return 0u;
    }
    s2 = (s2 + 1u) & (q->hcap - 1u);
  }
  q->hkey[s2] = h;
  q->hidx[s2] = idx;
  q->hn++;
  return 1u;
}

// Normalize + canonicalize + dedup + close-check + enqueue one derived
// CG.  Returns ATP_PROVED when the pair's sides unify (equality
// resolution), ATP_RUNNING otherwise.
static AtpStatus cgq_admit(AtpState *s, AtpCgq *q, Term l, Term r) {
  u8 rec = nrec_on(s);
  // R-normalize both sides (equational consequences only: sound).
  // Recording runs log the same normalization (nrec_normalize).
  if (rec) {
    l = nrec_normalize(s, l, 0u);
    r = nrec_normalize(s, r, 1u);
  } else {
    l = atp_rewrite_normalize(s, l, s->lhs, s->rhs, s->n_rules,
                              ATP_GOAL_NORM_CAP);
    r = atp_rewrite_normalize(s, r, s->lhs, s->rhs, s->n_rules,
                              ATP_GOAL_NORM_CAP);
  }
  // Close: the two sides unify -> an instance is a syntactic identity.
  {
    RewriteSubst mg = {{0}};
    if (thvm_unify(l, r, &mg)) {
      if (rec) {
        AtpNrec *nq = (AtpNrec *)s->nrec;
        nrec_scr_push(s, NR_CLOSE, NR_NONE, 0u, 1u, NULL, 0u);
        u32 off = nrec_scr_commit(s);
        if (nq != NULL && off != NR_NONE) {
          nq->closed       = 1u;
          nq->close_parent = nq->pending_parent;
          nq->close_off    = off;
          nq->close_len    = nq->sn;
        }
      }
      s->goal_lhs = thvm_unify_apply(l, &mg);
      s->goal_rhs = thvm_unify_apply(r, &mg);
      q->proved = 1u;
      return ATP_PROVED;
    }
  }
  if (q->full) return ATP_RUNNING;
  thvm_normalize_vars(&l, &r);
  if (rec) nrec_scr_push(s, NR_CANON, NR_NONE, 0u, 1u, NULL, 0u);
  // Guards: size cap; var ids must stay below CP_RENAME_OFFSET so the
  // per-step rename-apart never crosses the REWRITE_MAX_VAR cliff.
  u32 w = cgq_term_size(l) + cgq_term_size(r);
  if (w > ATP_CGQ_MAX_SIZE) return ATP_RUNNING;
  u32 mv = cgq_term_max_var(l);
  u32 mv2 = cgq_term_max_var(r);
  if (mv2 > mv) mv = mv2;
  if (mv >= CP_RENAME_OFFSET) return ATP_RUNNING;
  if (q->n == q->cap) {
    u32 ncap = q->cap * 2u;
    Term *nl = (Term *)realloc(q->lhs, (size_t)ncap * sizeof(Term));
    Term *nr = (Term *)realloc(q->rhs, (size_t)ncap * sizeof(Term));
    u32 *nw  = (u32 *)realloc(q->wm, (size_t)ncap * sizeof(u32));
    u32 *nwt = (u32 *)realloc(q->weight, (size_t)ncap * sizeof(u32));
    u8  *ns  = (u8 *)realloc(q->state, ncap);
    u32 *nh  = (u32 *)realloc(q->heap, (size_t)ncap * sizeof(u32));
    u32 *nd  = (u32 *)realloc(q->done, (size_t)ncap * sizeof(u32));
    if (nl) q->lhs = nl;
    if (nr) q->rhs = nr;
    if (nw) q->wm = nw;
    if (nwt) q->weight = nwt;
    if (ns) q->state = ns;
    if (nh) q->heap = nh;
    if (nd) q->done = nd;
    if (!nl || !nr || !nw || !nwt || !ns || !nh || !nd) {
      q->full = 1u;
      return ATP_RUNNING;
    }
    q->cap = ncap;
  }
  if (!cgq_seen_add(q, l, r, q->n)) return ATP_RUNNING;   // duplicate
  u32 idx = q->n++;
  q->lhs[idx] = l;
  q->rhs[idx] = r;
  q->wm[idx] = 0u;
  q->weight[idx] = w;
  q->state[idx] = 0u;
  if (rec) {
    AtpNrec *nq = (AtpNrec *)s->nrec;
    u32 off = nrec_scr_commit(s);
    if (nq != NULL) nrec_cg_store(s, idx, off, nq->sn);
  }
  cgq_heap_push(q, idx);
  if (q->n >= ATP_CGQ_MAX_CGS) q->full = 1u;
  return ATP_RUNNING;
}

// One CG x one rule face: try every non-var position of both sides.
// `face_l -> face_r` is the narrowing direction (face_l already
// renamed apart); `face_oriented` = 1 when the face is an oriented
// rule's LHS (its instances are always maximal, so the ordered-
// paramodulation gate below is free).  Charges one budget unit per
// unification attempt.
//
// Ordered-narrowing restrictions (GKC prohibit_unordered_para /
// standard superposition-into-goal):
//   (a) skip the CG side that is strictly KBO-smaller than its
//       partner -- superposition rewrites only maximal sides;
//   (b) skip a narrow whose USED face instance is strictly smaller
//       than its partner face instance -- paramodulation only FROM a
//       maximal side.
// Both preserve refutational completeness of ordered narrowing and
// cut the branching the unordered walk drowns in.
static AtpStatus cgq_narrow_face(AtpState *s, AtpCgq *q, u32 idx,
                                 Term face_l, Term face_r,
                                 u8 face_oriented, u32 *budget,
                                 u32 rec_trace, u8 rec_fwd) {
  for (u32 side = 0u; side < 2u; side++) {
    Term me    = side == 0u ? q->lhs[idx] : q->rhs[idx];
    Term other = side == 0u ? q->rhs[idx] : q->lhs[idx];
    if (atp_compare(s, me, other) == KBO_LT) continue;   // (a)
    // Iterative preorder over positions (the cp_walk_positions walker
    // is callback-based; an explicit stack keeps this loop flat and
    // lets a budget exhaust return cleanly mid-walk).
    typedef struct { Term t; u32 next; } CgqFrame;
    CgqFrame st[CP_MAX_DEPTH];
    u32 sp = 0u;
    st[0].t = me; st[0].next = 0u;
    for (;;) {
      Term sub = st[sp].t;
      if (st[sp].next == 0u && term_tag(sub) == TAG_CTR) {
        if (*budget == 0u) return ATP_RUNNING;
        (*budget)--;
        RewriteSubst mg = {{0}};
        if (thvm_unify(sub, face_l, &mg)) {
          // (b) ordered-paramodulation gate on the face instance.
          u8 ok = 1u;
          if (!face_oriented) {
            Term fl = thvm_unify_apply(face_l, &mg);
            Term fr = thvm_unify_apply(face_r, &mg);
            if (atp_compare(s, fl, fr) == KBO_LT) ok = 0u;
          }
          if (ok) {
            // Rebuild `me` with the subterm replaced, bottom-up along
            // the stack spine, then instantiate both sides.
            Term repl = face_r;
            for (u32 d = sp; d > 0u; d--) {
              Term parent = st[d - 1u].t;
              u32 n = term_ctr_n(parent);
              if (n > REWRITE_MAX_ARITY) { ok = 0u; break; }
              Term kids[REWRITE_MAX_ARITY];
              for (u32 i = 0; i < n; i++) kids[i] = term_ctr_at(parent, i);
              kids[st[d - 1u].next - 1u] = repl;
              repl = term_new_ctr(term_ext(parent), kids, n);
            }
            if (ok) {
              Term nl = thvm_unify_apply(repl, &mg);
              Term nr = thvm_unify_apply(other, &mg);
              if (nrec_on(s)) {
                // Redex path: the child index taken from each stack
                // frame above the hit.  The emitter re-unifies the
                // recorded face at this position, so the descriptor
                // (trace, side, path, face) fully determines the step.
                nrec_scr_reset(s, idx);
                u32 rpos[CP_MAX_DEPTH];
                for (u32 d = 0; d < sp; d++) rpos[d] = st[d].next - 1u;
                nrec_scr_push(s, NR_NARROW, rec_trace, side, rec_fwd,
                              rpos, sp);
              }
              if (cgq_admit(s, q, side == 0u ? nl : nr,
                            side == 0u ? nr : nl) == ATP_PROVED)
                return ATP_PROVED;
            }
          }
        }
      }
      // Advance: descend into the next child, else pop.
      if (term_tag(sub) == TAG_CTR && st[sp].next < term_ctr_n(sub)
          && sp + 1u < CP_MAX_DEPTH) {
        Term child = term_ctr_at(sub, st[sp].next);
        st[sp].next++;
        sp++;
        st[sp].t = child;
        st[sp].next = 0u;
      } else if (sp > 0u) {
        sp--;
      } else {
        break;
      }
    }
  }
  return ATP_RUNNING;
}

// One CG x rule k, both admissible faces.
static AtpStatus cgq_narrow_rule(AtpState *s, AtpCgq *q, u32 idx,
                                 u32 k, u32 *budget) {
  if (s->lhs[k] == 0) return ATP_RUNNING;   // deleted rule slot
  u8 oriented = (u8)(s->r_orient != NULL && s->r_orient[k]);
  u32 rtr = nrec_on(s) ? nrec_slot_trace(s, k) : NR_NONE;
  Term lj = thvm_rename_vars(s->lhs[k], CP_RENAME_OFFSET);
  Term rj = thvm_rename_vars(s->rhs[k], CP_RENAME_OFFSET);
  if (cgq_narrow_face(s, q, idx, lj, rj, oriented, budget,
                      rtr, 1u) == ATP_PROVED)
    return ATP_PROVED;
  if (!oriented) {
    // Unorientable equation: the reversed face narrows too (COL011-1's
    // opening step is o_definition's RHS face).
    if (cgq_narrow_face(s, q, idx, rj, lj, 0u, budget,
                        rtr, 0u) == ATP_PROVED)
      return ATP_PROVED;
  }
  return ATP_RUNNING;
}

static AtpStatus atp_cgq_check(AtpState *s) {
  AtpCgq *q = (AtpCgq *)s->cgq;
  if (q == NULL) {
    q = cgq_create();
    if (q == NULL) return ATP_RUNNING;
    s->cgq = (struct AtpCgq *)q;
    // Seed CG0 = the original goal pair (kept immutable in the goal
    // slots -- unlike the greedy lane, the queue never rewrites them).
    if (nrec_on(s)) nrec_scr_reset(s, NR_ROOT);
    if (cgq_admit(s, q, s->goal_lhs, s->goal_rhs) == ATP_PROVED)
      return ATP_PROVED;
  }
  if (q->proved) return ATP_PROVED;

  // THVM_ATP_CGQ_DEBUG: periodic lane telemetry on stderr.
  { static int dl = -1; static u64 last = 0; static u64 calls = 0;
    if (dl < 0) dl = (getenv("THVM_ATP_CGQ_DEBUG") != NULL) ? 1 : 0;
    calls++;
    if (dl && (last == 0u || q->pops - last >= 1024u ||
               calls % 4096u == 0u)) {
      last = q->pops ? q->pops : 1u;
      fprintf(stderr, "[cgq] calls=%llu pops=%llu cgs=%u heap=%u "
                      "done=%u bw_rule=%u/%u top_w=%u full=%u\n",
              (unsigned long long)calls,
              (unsigned long long)q->pops, q->n, q->heap_n, q->done_n,
              q->bw_rule, s->n_rules,
              q->heap_n > 0u ? q->weight[q->heap[0]] : 0u, q->full);
    }
  }

  u32 budget = ATP_CGQ_BUDGET;

  // (1) Backward sweep FIRST, on a reserved half of the budget: rules
  // the parked CGs have not seen narrow into them incrementally
  // (RueckwartigeUeberlappung).  This is the lane's lemma route --
  // GKC's COL011-1 refutation narrows the ORIGINAL goal with a
  // derived two-sided lemma (its clause 4), i.e. exactly parked-CG0 x
  // late-rule -- and a pops-first ordering starves it (measured stuck
  // at bw_rule=2/431: pops always exhaust the budget).  done[] is in
  // park order, watermarks non-decreasing: for rule bw_rule the
  // eligible CGs are the prefix with wm <= bw_rule; the cursor pair
  // resumes across calls.
  {
    u32 bw_budget = ATP_CGQ_BUDGET / 2u;
    while (bw_budget > 0u && q->bw_rule < s->n_rules) {
      if (q->bw_pos >= q->done_n) { q->bw_rule++; q->bw_pos = 0u; continue; }
      u32 idx = q->done[q->bw_pos];
      if (q->wm[idx] > q->bw_rule) {
        // This CG (and every later parked one) already covers bw_rule.
        q->bw_rule++;
        q->bw_pos = 0u;
        continue;
      }
      if (cgq_narrow_rule(s, q, idx, q->bw_rule, &bw_budget) == ATP_PROVED)
        return ATP_PROVED;
      if (bw_budget == 0u) break;   // resume this (rule, pos) next call
      q->bw_pos++;
    }
    budget -= ATP_CGQ_BUDGET / 2u - bw_budget;
  }

  // (2) Best-first pops: narrow the lightest queued CG against rule
  // faces [wm, n_rules); a CG that exhausts the budget mid-sweep goes
  // back on the heap with its watermark advanced.
  while (budget > 0u && q->heap_n > 0u) {
    u32 idx;
    q->pops++;
    if (q->pops % ATP_CGQ_FIFO_MOD == 0u) {
      // FIFO lane: oldest still-queued CG (GKC weight_select_ratio).
      while (q->fifo < q->n && q->state[q->fifo] != 0u) q->fifo++;
      if (q->fifo < q->n) {
        idx = q->fifo;
        // Leave it in the heap; mark processed only when complete.
      } else {
        idx = q->heap[0];
      }
    } else {
      idx = q->heap[0];
    }
    u32 before = q->wm[idx];
    while (q->wm[idx] < s->n_rules && budget > 0u) {
      u32 k = q->wm[idx];
      if (cgq_narrow_rule(s, q, idx, k, &budget) == ATP_PROVED)
        return ATP_PROVED;
      if (budget == 0u &&
          q->wm[idx] == k) break;      // suspended mid-rule: retry k
      q->wm[idx] = k + 1u;
    }
    if (q->wm[idx] >= s->n_rules) {
      // Fully swept: park it.
      q->state[idx] = 1u;
      if (idx == q->heap[0]) (void)cgq_heap_pop(q);
      else {
        // FIFO pick that wasn't the heap top: lazily skipped when it
        // surfaces (state==1 check below).
      }
      q->done[q->done_n++] = idx;
    }
    // Drop already-parked entries that surface at the heap top (lazy
    // deletion for FIFO picks).
    while (q->heap_n > 0u && q->state[q->heap[0]] != 0u)
      (void)cgq_heap_pop(q);
    if (before == q->wm[idx] && budget == 0u) break;   // no progress
  }

  return ATP_RUNNING;
}

// GC rooting (see thvm_atp_gc_collect): the CG store's Terms are the
// lane's only live heap references.  The dedup hash keys are
// STRUCTURAL (content over tags/labels/arity), so relocated cells
// keep their hashes and stay in their buckets -- no table fixup.
static u32 cgq_gc_count(struct AtpCgq *q) {
  return q == NULL ? 0u : 2u * q->n;
}
static void cgq_gc_gather(struct AtpCgq *q, Term *roots, u32 *w) {
  if (q == NULL) return;
  for (u32 i = 0; i < q->n; i++) {
    roots[(*w)++] = q->lhs[i];
    roots[(*w)++] = q->rhs[i];
  }
}
static void cgq_gc_writeback(struct AtpCgq *q, const Term *roots,
                             u32 base) {
  if (q == NULL) return;
  for (u32 i = 0; i < q->n; i++) {
    q->lhs[i] = roots[base + 2u * i];
    q->rhs[i] = roots[base + 2u * i + 1u];
  }
}

fn void thvm_atp_set_use_narrow_queue(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_narrow_queue = on ? 1u : 0u;
}

// ===================================================================
// Refutational goal driver (unified-engine Phase 2)   --goal-drive
// ===================================================================
// The '-10' condensed-detachment (CD) family encodes modus ponens as a
// dispatcher equation
//   ifeq(theorem(implies(Y,X)),true, ifeq(theorem(Y),true,theorem(X),
//        true), true) = true               (rule_2)
// plus rule_1 ifeq(axiom(X),true,theorem(X),true)=true, the reducer
// ifeq(A,A,B,C)=B, ground axiom-schema facts, and a GROUND goal
// theorem(G).  Forward completion floods (it releases ever more general
// theorem SCHEMAS while the goal is one specific formula), and the
// engine's goal is only a PASSIVE normal-form check: oriented rules
// rewrite it, but nothing performs inference INTO it, so record_norm
// stays 0.  Vampire proves these by keeping the negated goal an ACTIVE
// clause that unorientable/conditional equations SUPERPOSE into.
//
// This driver is the equational-form complement.  It regresses the
// ground goal backward through the raw ifeq-guarded equations,
// interleaved with completion in goal_check so the LIVE rule set R
// serves two jobs at once: (1) its definitional rewrites bridge the
// goal's connective forms in the goal-side normal form (BASE-1), and
// (2) its completion-derived theorem(P)=true facts discharge the ifeq
// guards without regressing them all the way to axioms (BASE-2 /
// bidirectional meet-in-the-middle).  Every closure is a genuine
// ground instance of R union the dispatcher equations, so a closed
// goal is a sound refutation.
//
// Design: functional SLD over immutable hash-consed Terms (no binding
// trail -- the unifier sigma is applied to the continuation goals
// before recursing, and backtracking just tries the next equation
// against the ORIGINAL, un-substituted continuation).  Iterative
// deepening + a ground-goal memo (proved lemmas, regularity/onpath
// loop prune, per-depth fail cutoff) bound the search.
//
// Env: THVM_ATP_GOAL_DRIVE arms it; THVM_ATP_GOAL_DRIVE_{STRIDE,NODES,
// DEPTH} tune the interleave cadence / per-run node cap / depth cap.

#define GD_VBASE       32u          // rule/fact vars renamed to [32,64)
#define GD_MAX_GUARDS   6u
#define GD_MAX_OPEN    48u
#define GD_MEMO_SLOTS  (1u << 18)
#define GD_NF_CAP      4096u        // goal-side NF step ceiling per side

typedef struct { Term a, b; } GdG;

typedef struct {                    // a decomposed ifeq-guarded rule:
  Term concl, w;                    //   concl = w  <=  AND_i (gl_i = gr_i)
  Term gl[GD_MAX_GUARDS], gr[GD_MAX_GUARDS];
  u32  ng;
} GdRule;

typedef struct {
  u64 h; Term a, b; u32 gen; i32 faildepth; u8 onpath, proved;
} GdMemo;

struct AtpGoalDrive {
  u32     built_rev;                // r_revision the rule cache was built at
  GdRule *rules; u32 n_rules, rules_cap;
  GdMemo *memo;
  u32     gen;                      // memo generation (bumped per run)
  u32     last_run_rules;           // throttle cursor
  long    nodes, node_cap;
  u64     heap_start, cell_cap;     // per-run bump-heap window / budget
  u64     deadline_us;              // wall deadline mirrored per run
  u32     size_cap;                 // reject subgoal sides bigger than this
  int     over;                     // budget exhausted this run
};

// Diagnostic counters (extern-printed by the bench stats block).
u64 g_goal_drive_calls    = 0;      // driver invocations that searched
u64 g_goal_drive_variants = 0;      // goal variants (subgoal nodes) explored
u64 g_goal_drive_closes   = 0;      // BASE closures (NF or fact-unify)
u64 g_goal_drive_discharg = 0;      // successful ifeq-guard STEP regressions
u64 g_goal_drive_proved   = 0;      // top goals proved

// The goal driver's term walks recurse once per term level (gd_hash and
// gd_size stop at 64).  The driver is off by default (THVM_ATP_GOAL_DRIVE=1
// or thvm_atp_set_use_goal_drive turns it on; no FindProof schedule lane
// does).
static u8 gd_ground(Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return 0u;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        if (!gd_ground(term_ctr_at(t, i))) return 0u;
      return 1u;
    }
    default: return 1u;
  }
}

static u64 gd_hash(Term t, u32 d) {
  if (d > 64u) return 1469598103934665603ULL;
  u8 tg = term_tag(t);
  u64 h = 1099511628211ULL * (u64)((u32)tg + 1u);
  if (tg == TAG_FVR)
    return h ^ (0x9e3779b97f4a7c15ULL * (u64)(term_ext(t) + 1u));
  if (tg == TAG_CTR) {
    h ^= 0x100000001b3ULL * (u64)(term_ext(t) + 1u);
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      u64 c = gd_hash(term_ctr_at(t, i), d + 1u);
      h = ((h << 7) | (h >> 57)) ^ c;
    }
    return h;
  }
  return h ^ (u64)t;
}

static u32 gd_size(Term t, u32 d) {
  if (d > 64u) return 1u;
  if (term_tag(t) != TAG_CTR) return 1u;
  u32 n = term_ctr_n(t), s = 1u;
  for (u32 i = 0; i < n; i++) s += gd_size(term_ctr_at(t, i), d + 1u);
  return s;
}

static u32 gd_maxvar(Term t, u32 acc) {
  switch (term_tag(t)) {
    case TAG_FVR: { u32 v = term_ext(t) + 1u; return v > acc ? v : acc; }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) acc = gd_maxvar(term_ctr_at(t, i), acc);
      return acc;
    }
    default: return acc;
  }
}

static int gd_rigid(Term t, int *vars) {
  switch (term_tag(t)) {
    case TAG_FVR: (*vars)++; return 0;
    case TAG_CTR: {
      int n = 1; u32 k = term_ctr_n(t);
      for (u32 i = 0; i < k; i++) n += gd_rigid(term_ctr_at(t, i), vars);
      return n;
    }
    default: return 1;
  }
}
// Selection score: ground goals first (memoizable, no shared vars);
// otherwise the most rigidly-constrained (binds the free premise
// variable Y against a constrained fact before "prove SOME theorem").
static long gd_score(Term t) {
  int vars = 0;
  int rigid = gd_rigid(t, &vars);
  if (vars == 0) return 1L << 30;
  return (long)rigid * 8 - vars;
}

static Term gd_canon_walk(Term t, i32 *map, u32 *next) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id >= 64u) return t;
      if (map[id] < 0) map[id] = (i32)((*next)++);
      return term_new_fvr((u32)map[id]);
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n == 0u || n > REWRITE_MAX_ARITY) return t;
      Term ch[REWRITE_MAX_ARITY]; u8 changed = 0u;
      for (u32 i = 0; i < n; i++) {
        Term orig = term_ctr_at(t, i);
        ch[i] = gd_canon_walk(orig, map, next);
        if (ch[i] != orig) changed = 1u;
      }
      if (!changed) return t;
      return term_new_ctr(term_ext(t), ch, n);
    }
    default: return t;
  }
}
// Renumber every FVR across the goal list to a shared dense [0,next)
// range so the next rule instantiation (renamed by GD_VBASE) never
// collides.  Returns 0 if the list needs >= GD_VBASE distinct vars
// (bail the branch -- never happens on the CD family).
static u8 gd_canon(GdG *gs, u32 n) {
  i32 map[64]; for (u32 i = 0; i < 64u; i++) map[i] = -1;
  u32 next = 0u;
  for (u32 i = 0; i < n; i++) {
    gs[i].a = gd_canon_walk(gs[i].a, map, &next);
    gs[i].b = gd_canon_walk(gs[i].b, map, &next);
  }
  return next < GD_VBASE;
}

// An ifeq-guarded node: a detected dispatcher functor, arity 3/4, whose
// first two args DIFFER (a real guard, not the reducer ifeq(A,A,B,C)).
static u8 gd_ifeq_guarded(const AtpState *s, Term t) {
  if (term_tag(t) != TAG_CTR) return 0u;
  u32 n = term_ctr_n(t);
  if (n != 3u && n != 4u) return 0u;
  if (!atp_ifeq_sym_is(s, term_ext(t))) return 0u;
  return (u8)!atp_ifeq_term_eq(term_ctr_at(t, 0), term_ctr_at(t, 1));
}

// Unwind an ifeq nest `ifeq(g1,c1, ifeq(g2,c2, ..., CONCL, e), e) = w`
// into (CONCL, w, guards [(g_i,c_i)]).  Descends the 'then' branch
// (arg 2) at each level.  0 if not actually guarded / too deep.
static u8 gd_decompose(const AtpState *s, Term nest, Term w, GdRule *r) {
  Term cur = nest; u32 guard = 0u;
  while (gd_ifeq_guarded(s, cur)) {
    if (guard >= GD_MAX_GUARDS) return 0u;
    r->gl[guard] = term_ctr_at(cur, 0);
    r->gr[guard] = term_ctr_at(cur, 1);
    guard++;
    cur = term_ctr_at(cur, 2);
  }
  if (guard == 0u) return 0u;
  r->concl = cur; r->w = w; r->ng = guard;
  // Reject any rule carrying a var id that would overflow the [32,64)
  // rename window (never true for parser-numbered CD rules).
  u32 mv = gd_maxvar(cur, 0u);
  for (u32 i = 0; i < guard; i++) {
    mv = gd_maxvar(r->gl[i], mv);
    mv = gd_maxvar(r->gr[i], mv);
  }
  mv = gd_maxvar(w, mv);
  return (u8)(mv < GD_VBASE);
}

static void gd_build(AtpState *s, struct AtpGoalDrive *g) {
  if (g->built_rev == s->r_revision && g->rules != NULL) return;
  g->n_rules = 0u;
  for (u32 i = 0; i < s->n_rules; i++) {
    if (s->r_dead != NULL && s->r_dead[i]) continue;
    Term l = s->lhs[i], rr = s->rhs[i];
    GdRule rule; u8 ok = 0u;
    if (gd_ifeq_guarded(s, l))       ok = gd_decompose(s, l, rr, &rule);
    else if (gd_ifeq_guarded(s, rr)) ok = gd_decompose(s, rr, l, &rule);
    if (!ok) continue;
    if (g->n_rules >= g->rules_cap) {
      u32 nc = g->rules_cap ? g->rules_cap * 2u : 16u;
      GdRule *nr = (GdRule *)realloc(g->rules, (size_t)nc * sizeof(GdRule));
      if (nr == NULL) break;
      g->rules = nr; g->rules_cap = nc;
    }
    g->rules[g->n_rules++] = rule;
  }
  g->built_rev = s->r_revision;
}

static GdMemo *gd_memo_find(struct AtpGoalDrive *g, Term a, Term b,
                            u64 h, u8 create) {
  if (g->memo == NULL) return NULL;
  u32 slot = (u32)(h & (u64)(GD_MEMO_SLOTS - 1u));
  for (u32 probe = 0; probe < 64u; probe++) {
    GdMemo *m = &g->memo[slot];
    if (m->gen != g->gen) {
      if (!create) return NULL;
      m->gen = g->gen; m->h = h; m->a = a; m->b = b;
      m->faildepth = -1; m->onpath = 0u; m->proved = 0u;
      return m;
    }
    if (m->h == h && kbo_eq(m->a, a) && kbo_eq(m->b, b)) return m;
    slot = (slot + 1u) & (GD_MEMO_SLOTS - 1u);
  }
  return NULL;
}

static int gd_budget(struct AtpGoalDrive *g) {
  if (g->over) return 0;
  if (++g->nodes > g->node_cap) { g->over = 1; return 0; }
  // Guard the bump heap: stop the run well before from-space is
  // exhausted (the transient search terms are reclaimed wholesale by
  // the heap_reset at run end, but a single run must stay bounded).
  if ((g->nodes & 0x3f) == 0) {
    if ((HEAP_NEXT - g->heap_start) > g->cell_cap) { g->over = 1; return 0; }
    if (g->deadline_us != 0u && atp_now_us() > g->deadline_us) {
      g->over = 1; return 0;
    }
  }
  return 1;
}

static int gd_prove_seq(AtpState *s, struct AtpGoalDrive *g,
                        GdG *gs, u32 n, int depth);

// Close subgoal (a,b) at remaining `depth`, then prove `tail`.
static int gd_solve_tail(AtpState *s, struct AtpGoalDrive *g,
                         Term a, Term b, int depth, GdG *tail, u32 tn) {
  g_goal_drive_variants++;
  // BASE-1: goal-side normal form.  Applies every oriented AND
  // unorientable rule in R (KBO-gated), so the definitional rewrites
  // bridge the goal's connective forms and derived theorem(P)=true /
  // axiom-schema rules collapse an established atom to `true`.
  Term na = atp_rewrite_normalize(s, a, s->lhs, s->rhs, s->n_rules, GD_NF_CAP);
  Term nb = atp_rewrite_normalize(s, b, s->lhs, s->rhs, s->n_rules, GD_NF_CAP);
  if (kbo_eq(na, nb)) {
    g_goal_drive_closes++;
    return gd_prove_seq(s, g, tail, tn, depth);
  }
  a = na; b = nb;
  // Size cap: reject subgoals grown far past the goal.  Backward MP
  // regression otherwise diverges into problem-INDEPENDENT nested
  // implications theorem(implies(Y,implies(Y',...))) that never close
  // (horn-back's g_bk_size_cap guards the same way); this keeps the
  // frontier near the derived-theorem sizes the meet must reach.
  if (g->size_cap != 0u &&
      gd_size(a, 0u) + gd_size(b, 0u) > g->size_cap) return 0;
  u8 gnd = (u8)(gd_ground(a) && gd_ground(b));
  u32 ha = (term_tag(a) == TAG_CTR) ? term_ext(a) : 0xffffffffu;
  u32 hb = (term_tag(b) == TAG_CTR) ? term_ext(b) : 0xffffffffu;
  // BASE-2: unify against a stored equation (both faces).  BINDS the
  // free premise variable against a derived theorem / axiom-schema
  // fact.  Oriented facts on a ground subgoal are already covered by
  // BASE-1, so scan only unorientable ones there.
  for (u32 i = 0; i < s->n_rules && !g->over; i++) {
    if (s->r_dead != NULL && s->r_dead[i]) continue;
    Term u = s->lhs[i], v = s->rhs[i];
    if (gd_ifeq_guarded(s, u) || gd_ifeq_guarded(s, v)) continue;
    if (gnd && s->r_orient != NULL && s->r_orient[i]) continue;
    if (gd_maxvar(u, 0u) >= GD_VBASE || gd_maxvar(v, 0u) >= GD_VBASE) continue;
    u32 hu = (term_tag(u) == TAG_CTR) ? term_ext(u) : 0xfffffffeu;
    u32 hv = (term_tag(v) == TAG_CTR) ? term_ext(v) : 0xfffffffeu;
    u8 av = (u8)(term_tag(a) == TAG_FVR), bv = (u8)(term_tag(b) == TAG_FVR);
    u8 uv = (u8)(term_tag(u) == TAG_FVR), vv = (u8)(term_tag(v) == TAG_FVR);
    u8 or0 = (u8)((av || uv || hu == ha) && (bv || vv || hv == hb));
    u8 or1 = (u8)((av || vv || hv == ha) && (bv || uv || hu == hb));
    if (!or0 && !or1) continue;
    Term ru = thvm_rename_vars(u, GD_VBASE), rv = thvm_rename_vars(v, GD_VBASE);
    for (int orient = 0; orient < 2; orient++) {
      if ((orient == 0 && !or0) || (orient == 1 && !or1)) {
        if (kbo_eq(ru, rv)) break;
        continue;
      }
      Term U = orient ? rv : ru, V = orient ? ru : rv;
      RewriteSubst sub; memset(&sub, 0, sizeof sub);
      if (thvm_unify(a, U, &sub) && thvm_unify(b, V, &sub)) {
        if (!gd_budget(g)) return 0;
        GdG nt[GD_MAX_OPEN]; u32 m = 0u; u8 okc = 1u;
        for (u32 k = 0; k < tn; k++) {
          if (m >= GD_MAX_OPEN) { okc = 0u; break; }
          nt[m].a = thvm_unify_apply(tail[k].a, &sub);
          nt[m].b = thvm_unify_apply(tail[k].b, &sub);
          m++;
        }
        if (okc && gd_canon(nt, m)) {
          g_goal_drive_closes++;
          if (gd_prove_seq(s, g, nt, m, depth)) return 1;
        }
      }
      if (kbo_eq(ru, rv)) break;
    }
  }
  // STEP: regress through an ifeq-guarded rule conclusion (both faces);
  // spawn its guards as new subgoals ahead of the tail.
  if (depth > 0) {
    for (u32 ri = 0; ri < g->n_rules && !g->over; ri++) {
      GdRule *r = &g->rules[ri];
      Term rc = thvm_rename_vars(r->concl, GD_VBASE);
      Term rw = thvm_rename_vars(r->w, GD_VBASE);
      for (int orient = 0; orient < 2; orient++) {
        Term C = orient ? rw : rc, W = orient ? rc : rw;
        RewriteSubst sub; memset(&sub, 0, sizeof sub);
        if (thvm_unify(a, C, &sub) && thvm_unify(b, W, &sub)) {
          if (!gd_budget(g)) return 0;
          g_goal_drive_discharg++;   // a guarded-rule regression attempt
          GdG ng[GD_MAX_OPEN]; u32 m = 0u; u8 okc = 1u;
          for (u32 gi = 0; gi < r->ng && okc; gi++) {
            if (m >= GD_MAX_OPEN) { okc = 0u; break; }
            Term rgl = thvm_rename_vars(r->gl[gi], GD_VBASE);
            Term rgr = thvm_rename_vars(r->gr[gi], GD_VBASE);
            ng[m].a = thvm_unify_apply(rgl, &sub);
            ng[m].b = thvm_unify_apply(rgr, &sub);
            m++;
          }
          for (u32 k = 0; k < tn && okc; k++) {
            if (m >= GD_MAX_OPEN) { okc = 0u; break; }
            ng[m].a = thvm_unify_apply(tail[k].a, &sub);
            ng[m].b = thvm_unify_apply(tail[k].b, &sub);
            m++;
          }
          if (okc && gd_canon(ng, m)) {
            if (gd_prove_seq(s, g, ng, m, depth - 1)) return 1;
          }
        }
        if (kbo_eq(rc, rw)) break;
      }
    }
  }
  return 0;
}

// Prove the conjunction gs[0..n): select the most-constrained subgoal,
// solve it, then the rest.  A ground subgoal is solved in isolation
// (its proof binds nothing shared) and memoized; a non-ground one
// carries the tail so its solution's bindings propagate.
static int gd_prove_seq(AtpState *s, struct AtpGoalDrive *g,
                        GdG *gs, u32 n, int depth) {
  if (n == 0u) return 1;
  if (!gd_budget(g)) return 0;
  u32 best = 0u; long bestsc = -(1L << 40);
  for (u32 i = 0; i < n; i++) {
    long sc = gd_score(gs[i].a);
    if (sc > bestsc) { bestsc = sc; best = i; }
  }
  GdG gcur = gs[best];
  GdG tail[GD_MAX_OPEN]; u32 tn = 0u;
  for (u32 i = 0; i < n; i++) if (i != best) tail[tn++] = gs[i];
  if (gd_ground(gcur.a) && gd_ground(gcur.b)) {
    u64 h = gd_hash(gcur.a, 0u) * 1000003ULL ^ gd_hash(gcur.b, 0u);
    GdMemo *m = gd_memo_find(g, gcur.a, gcur.b, h, 0u);
    if (m != NULL) {
      if (m->proved)  return gd_prove_seq(s, g, tail, tn, depth);
      if (m->onpath)  return 0;
      if (m->faildepth >= depth) return 0;
    }
    GdMemo *pm = gd_memo_find(g, gcur.a, gcur.b, h, 1u);
    if (pm != NULL) pm->onpath = 1u;
    int ok = gd_solve_tail(s, g, gcur.a, gcur.b, depth, NULL, 0u);
    if (pm != NULL) pm->onpath = 0u;
    if (ok) {
      if (pm != NULL) pm->proved = 1u;
      return gd_prove_seq(s, g, tail, tn, depth);
    }
    if (pm != NULL && !g->over && depth > pm->faildepth) pm->faildepth = depth;
    return 0;
  }
  return gd_solve_tail(s, g, gcur.a, gcur.b, depth, tail, tn);
}

static AtpStatus atp_goal_drive_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;
  if (s->n_goals > 1u || s->goal_existential) return ATP_RUNNING;
  struct AtpGoalDrive *g = s->gdrive;
  if (g == NULL) {
    g = (struct AtpGoalDrive *)calloc(1, sizeof *g);
    if (g == NULL) return ATP_RUNNING;
    g->memo = (GdMemo *)calloc(GD_MEMO_SLOTS, sizeof(GdMemo));
    if (g->memo == NULL) { free(g); return ATP_RUNNING; }
    g->built_rev = 0xffffffffu;
    s->gdrive = g;
  }
  static u32 stride = 0u;
  static long node_cap = 15000;
  static int depth_cap = 12, gd_dbg = 0;
  if (stride == 0u) {
    stride = 100u;
    const char *e = getenv("THVM_ATP_GOAL_DRIVE_STRIDE");
    if (e != NULL && atoi(e) > 0) stride = (u32)atoi(e);
    e = getenv("THVM_ATP_GOAL_DRIVE_NODES");
    if (e != NULL && atoi(e) > 0) node_cap = atol(e);
    e = getenv("THVM_ATP_GOAL_DRIVE_DEPTH");
    if (e != NULL && atoi(e) > 0) depth_cap = atoi(e);
    gd_dbg = getenv("THVM_ATP_GOAL_DRIVE_DEBUG") != NULL;
  }
  // Interleave cadence: run once, then only after R has grown by
  // `stride` rules (completion enriches the fact base between runs --
  // the bidirectional meet-in-the-middle needs fresh derived theorems).
  if (g->last_run_rules != 0u && s->n_rules < g->last_run_rules + stride)
    return ATP_RUNNING;
  gd_build(s, g);
  if (g->n_rules == 0u) return ATP_RUNNING;   // no dispatcher rule (don't
                                              // latch: retry once detected)
  g->last_run_rules = s->n_rules ? s->n_rules : 1u;
  if (gd_dbg) {
    // Ground unconditional facts (compound = nullary-const) -- the
    // forward-derived theorem/axiom leaves the backward search meets.
    u32 gfacts = 0u;
    for (u32 fi = 0; fi < s->n_rules; fi++) {
      if (s->r_dead != NULL && s->r_dead[fi]) continue;
      Term l = s->lhs[fi], rr = s->rhs[fi];
      u8 lc = (u8)(term_tag(l) == TAG_CTR && term_ctr_n(l) > 0u && gd_ground(l));
      u8 rc = (u8)(term_tag(rr) == TAG_CTR && term_ctr_n(rr) == 0u);
      u8 lc2 = (u8)(term_tag(rr) == TAG_CTR && term_ctr_n(rr) > 0u && gd_ground(rr));
      u8 rc2 = (u8)(term_tag(l) == TAG_CTR && term_ctr_n(l) == 0u);
      if ((lc && rc) || (lc2 && rc2)) gfacts++;
    }
    fprintf(stderr, "[goal-drive] run=%llu n_rules=%u dispatchers=%u "
            "gfacts=%u\n", (unsigned long long)g_goal_drive_calls,
            s->n_rules, g->n_rules, gfacts);
  }
  if (gd_dbg && g_goal_drive_calls < 1u) {
    fprintf(stderr, "[goal-drive] seed lhs="); atp_dbg_print_term(stderr, s->goal_lhs);
    fprintf(stderr, "  rhs="); atp_dbg_print_term(stderr, s->goal_rhs);
    fprintf(stderr, "  n_rules=%u dispatchers=%u ifeq_syms=%u\n",
            s->n_rules, g->n_rules, s->n_ifeq_syms);
    for (u32 di = 0; di < g->n_rules && di < 6u; di++) {
      fprintf(stderr, "[goal-drive]  rule%u ng=%u concl=", di, g->rules[di].ng);
      atp_dbg_print_term(stderr, g->rules[di].concl);
      fprintf(stderr, " w="); atp_dbg_print_term(stderr, g->rules[di].w);
      for (u32 gi = 0; gi < g->rules[di].ng; gi++) {
        fprintf(stderr, " g%u=", gi); atp_dbg_print_term(stderr, g->rules[di].gl[gi]);
        fprintf(stderr, "=="); atp_dbg_print_term(stderr, g->rules[di].gr[gi]);
      }
      fprintf(stderr, "\n");
    }
  }
  g->gen++;
  g->nodes = 0; g->node_cap = node_cap; g->over = 0;
  g->deadline_us = s->wall_deadline_us;
  // Size ceiling for subgoals: the goal's own size times a generous
  // factor (proof intermediates exceed the goal, but not without
  // bound).  Env THVM_ATP_GOAL_DRIVE_SIZE overrides absolutely.
  { u32 gsz = gd_size(s->goal_lhs, 0u) + gd_size(s->goal_rhs, 0u);
    g->size_cap = gsz * 6u + 64u;
    const char *e = getenv("THVM_ATP_GOAL_DRIVE_SIZE");
    if (e != NULL && atoi(e) > 0) g->size_cap = (u32)atoi(e); }
  // Cell budget: the driver is the sole allocator during its
  // synchronous run and pops the heap afterward, so it may safely use
  // most of the remaining from-space (75%, leaving slack for the
  // reset epoch).  Env THVM_ATP_GOAL_DRIVE_CELLS overrides absolutely.
  { u64 fend = gc_from_end();
    u64 remaining = (fend > HEAP_NEXT) ? (fend - HEAP_NEXT) : 0u;
    g->cell_cap = remaining - (remaining >> 2);
    const char *e = getenv("THVM_ATP_GOAL_DRIVE_CELLS");
    if (e != NULL && atoll(e) > 0) g->cell_cap = (u64)atoll(e); }
  g_goal_drive_calls++;
  // The regression allocates a large volume of transient Terms (NF
  // scratch, renamed rule instances, unifier images) into the bump
  // heap; none survive the run (a proof is a boolean verdict, not a
  // term kept in R).  Checkpoint the heap now and pop it back at the
  // end -- reclaiming EVERY search allocation in O(1), without a GC
  // pass that would relocate the driver's unrooted live set.  The
  // matching cell budget (below) bounds a single run under the live
  // heap's headroom so it can never exhaust from-space mid-search.
  g->heap_start = thvm_atp_heap_checkpoint();
  AtpStatus verdict = ATP_RUNNING;
  GdG seed; seed.a = s->goal_lhs; seed.b = s->goal_rhs;
  for (int d = 1; d <= depth_cap && !g->over; d++) {
    GdG lst[GD_MAX_OPEN]; lst[0] = seed;
    if (!gd_canon(lst, 1u)) break;
    if (gd_prove_seq(s, g, lst, 1u, d)) {
      g_goal_drive_proved++;
      verdict = ATP_PROVED;
      break;
    }
  }
  thvm_atp_heap_reset(g->heap_start);
  return verdict;
}

static void atp_goal_drive_free(struct AtpGoalDrive *g) {
  if (g == NULL) return;
  free(g->rules);
  free(g->memo);
  free(g);
}

fn void thvm_atp_set_use_goal_drive(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_goal_drive = on ? 1u : 0u;
}
