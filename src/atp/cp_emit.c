// === FT-direct CP emission (always on; certifier THVM_ATP_FT_EMIT_CHECK) ==
//
// The walk former's raw >= 50 KPBehandelt class -- ~84% of all packed
// CP nodes on OrAssociativity -- is queued UNTREATED: the (lhs, rhs)
// heap Terms that two thvm_unify_apply passes build for it are
// consumed ONLY by two byte serializers (the off-heap trace store and
// the fused queue pack) and then abandoned as garbage.  Under the
// route conditions (see sw_form_push) nothing else ever dereferences
// them: the eq-parent order gate runs VIRTUALLY (phase 2a), every
// push-side filter is off under the fused pipeline, and the packed
// queue + the packed trace serve every later reader (a surviving CP
// materializes at pop from cp_packed, a cited trace entry from
// trace_eq_buf).  So for that class the Term build is pure
// serialization overhead.  The treated < 50 class dies too (phase 2b
// below): its cells build straight from the composite.
//
// This path serializes the formed CP (sigma(lo[p<-rin]), sigma(ro))
// STRAIGHT from the parent terms + the unifier: one virtual preorder
// walk that resolves variables through the substitution on the fly,
// producing the identical scratch bytes, the identical fused-KBO flat
// encoding/balances, and the identical node counts, without
// allocating a single heap cell.  Byte-identity argument:
// thvm_unify_apply emits exactly one output node per resolved input
// node (unify_walk chase, then CTR children recurse / unbound FVR
// stays), and cp_replace_at only swaps the subtree at the overlap
// position p -- the virtual walk visits the same resolved nodes in
// the same preorder, so bytes / dense renumbering / flat nodes /
// balances / counts all coincide with the classic apply-then-
// acp_pack_fused route.  Certified per CP by THVM_ATP_FT_EMIT_CHECK=1
// (vcp_emit_certify, defined next to sw_form_push).
//
// This is NOT the retired THVM_ATPFT_UNIFY former (later-8 refutation:
// 30% slower, 2x RSS): that one built AtpFtCell trees in a scratch
// arena as an ADDITIONAL representation and still decoded them back
// through ft_to_term into Term-typed CP slots.  Here no second tree
// exists -- the byte string IS the output, and the raw class never
// becomes Terms at all.
//
// Anomalies (torn arity cell, over-arity, NUM/ERA atom,
// > REWRITE_MAX_VAR distinct vars, depth cap) bail to the classic
// Term build, mirroring the fused-pack fallback discipline.
enum { ATP_WM_BEHANDELN_GATE = 50 };     // WM KPVerwaltung.c:437
// KPBehandeln treat-vs-raw SIZE GATE (used at every treat/raw DECISION site:
// the two FT formers + the classic push branch).  Default = 50: a CP with
// combined raw size < gate is treated (doR normalize + joined-drop) at push;
// bigger CPs queue RAW.  The DISCOUNT lane (use_discount) forces it to 0, so
// EVERY CP queues raw (passive rawness).  ALL decision sites MUST read this
// one value: if a former arms the treated FT stash (size < gate) while the
// push branch expects raw (its gate says >= gate), the stashed cells strand
// on the raw arm.  Count-cap uses of ATP_WM_BEHANDELN_GATE stay 50 (they only
// bound a walk; the decision reads atp_behandeln_gate).
static inline u32 atp_behandeln_gate(const AtpState *s) {
  return (s != NULL && s->use_discount) ? 0u : (u32)ATP_WM_BEHANDELN_GATE;
}
#define VCP_SPD_NONE      0xffffffffu
#define VCP_COUNT_ANOMALY 0xffffffffu

typedef struct {
  u8  armed;       // stash live for exactly the one armed CP push
  u32 len;         // packed pair bytes at g_acp_scratch[0, len)
  u32 nodes;       // combined node count (== the unify_apply tally)
  u32 lhs_nodes;   // lhs-only node count
} VcpEmit;
static VcpEmit g_vcp_emit;

// FT-emit shape counters (PROFILE=2 printout; fb should stay 0 on ATP
// signatures).
u64 g_vcp_emit_n_ok = 0;
u64 g_vcp_emit_n_fb = 0;

// unify_walk mirror (src/unify/_.c): chase FVR binding chains to the
// final unbound variable / constructor.
static inline Term vcp_resolve(Term t, const RewriteSubst *subst) {
  while (term_tag(t) == TAG_FVR) {
    u32 id = term_ext(t);
    if (id >= REWRITE_MAX_VAR) return t;
    Term b = subst->bindings[id];
    if (b == 0) return t;
    t = b;
  }
  return t;
}

typedef struct { u64 base; u32 n; u32 i; u32 here; u32 spd; } VcpFrame;
static VcpFrame g_vcp_stk[ACP_FUSE_MAX_DEPTH];

// Child ci of a frame: the raw heap child, spine-substituted -- along
// the overlap path the child at p[spd] is the next spine node, and at
// the path's end it IS the replacement term rin (cp_replace_at's
// splice, virtualized).
static inline Term vcp_child(const VcpFrame *f, u32 ci, const u32 *p,
                             u32 p_len, Term rin, u32 *cspd) {
  Term c = heap_read(f->base + 1u + (u64)ci);
  *cspd = VCP_SPD_NONE;
  if (f->spd != VCP_SPD_NONE && p[f->spd] == ci) {
    if (f->spd + 1u == p_len) c = rin;
    else *cspd = f->spd + 1u;
  }
  return c;
}

// Node count of one virtual side, capped at `cap` (the walk stops
// there -- the caller consumes only the < gate verdict plus the exact
// under-cap total, exactly atp_symbol_count_upto's contract).  `acc`
// carries the count across sides.  Returns VCP_COUNT_ANOMALY on a
// torn arity cell / over-arity node / frame overflow -- the classes
// where thvm_unify_apply taints its tally -- so the caller falls back
// to the classic route.  A NUM/ERA atom counts one node and does not
// descend, matching the apply tally (the EMIT walk bails on it later;
// the count stays comparable either way).
static u32 vcp_count_side(Term root, const u32 *p, u32 p_len, Term rin,
                          const RewriteSubst *subst, u32 cap, u32 acc) {
  u32 sp = 0u;
  Term cur     = (rin != 0 && p_len == 0u) ? rin : root;
  u32  cur_spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  if (g_atp_phase_detail) { extern u64 g_vcp_cnt_calls; g_vcp_cnt_calls++; }
  for (;;) {
    cur = vcp_resolve(cur, subst);
    if (g_atp_phase_detail) { extern u64 g_vcp_cnt_nodes; g_vcp_cnt_nodes++; }
    if (++acc >= cap) return cap;
    if (term_tag(cur) == TAG_CTR) {
      u64 base = term_val(cur);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return VCP_COUNT_ANOMALY;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return VCP_COUNT_ANOMALY;
      if (n > 0u) {
        if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return VCP_COUNT_ANOMALY;
        g_vcp_stk[sp].base = base;
        g_vcp_stk[sp].n    = n;
        g_vcp_stk[sp].i    = 0u;
        g_vcp_stk[sp].spd  = cur_spd;
        sp++;
        cur = vcp_child(&g_vcp_stk[sp - 1u], 0u, p, p_len, rin, &cur_spd);
        continue;
      }
    }
    // Atom (leaf CTR / FVR / NUM): ascend to the next pending sibling.
    for (;;) {
      if (sp == 0u) return acc;
      VcpFrame *f = &g_vcp_stk[sp - 1u];
      if (++f->i < f->n) {
        cur = vcp_child(f, f->i, p, p_len, rin, &cur_spd);
        break;
      }
      sp--;
    }
  }
}

static u32 vcp_count_pair(Term lo, const u32 *p, u32 p_len, Term rin,
                          Term ro, const RewriteSubst *subst, u32 cap) {
  u32 acc = vcp_count_side(lo, p, p_len, rin, subst, cap, 0u);
  if (acc == VCP_COUNT_ANOMALY || acc >= cap) return acc;
  return vcp_count_side(ro, NULL, 0u, 0, subst, cap, acc);
}

// One virtual side of the fused emit: acp_fuse_walk_iter's exact byte
// / flat-node / balance emission, with the node source swapped from
// materialized Term cells to the resolve-through-subst composite
// above.  `fz` == NULL emits BYTES ONLY (no flat encode, no balances)
// -- the acp_trace_pack_fused analogue the treated FT route uses for
// its raw trace pair.
static u8 vcp_emit_side(Term root, const u32 *p, u32 p_len, Term rin,
                        const RewriteSubst *subst, const KboConfig *cfg,
                        AcpNvMap *m, u32 *pos, u32 *nodes, AcpFuse *fz,
                        int sign, u32 *fpos) {
  u32 lpos = *pos, lnodes = *nodes, lfpos = *fpos;
  u32 sp = 0u;
  Term cur     = (rin != 0 && p_len == 0u) ? rin : root;
  u32  cur_spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  if (g_atp_phase_detail) { extern u64 g_vcp_emt_calls; g_vcp_emt_calls++; }
  for (;;) {
    cur = vcp_resolve(cur, subst);
    lnodes++;
    if (g_atp_phase_detail) { extern u64 g_vcp_emt_nodes; g_vcp_emt_nodes++; }
    acp_scratch_ensure(lpos, 21u);
    u8 *bp = g_acp_scratch + lpos;
    switch (term_tag(cur)) {
      case TAG_CTR: {
        u64 base = term_val(cur);
        Term n_cell = heap_read(base);
        if (term_tag(n_cell) != TAG_NUM) return 0;
        u32 n = (u32)term_val(n_cell);
        if (n > REWRITE_MAX_ARITY) return 0;
        u32 lab = term_ext(cur);
        *bp++ = (u8)'C';
        acp_put_varint(&bp, lab);
        acp_put_varint(&bp, n);
        lpos = (u32)(bp - g_acp_scratch);
        u32 here = 0u;
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          here = lfpos++;
          u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
          g_acpf_pool[here].sym = (i32)lab;
          g_acpf_pool[here].w   = w;
          g_acpf_pool[here].ar  = n;
          fz->phidiff += (long long)sign * (long long)w;
        }
        if (n > 0u) {
          if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
          g_vcp_stk[sp].base = base;
          g_vcp_stk[sp].n    = n;
          g_vcp_stk[sp].i    = 0u;
          g_vcp_stk[sp].here = here;
          g_vcp_stk[sp].spd  = cur_spd;
          sp++;
          cur = vcp_child(&g_vcp_stk[sp - 1u], 0u, p, p_len, rin, &cur_spd);
          continue;
        }
        if (fz != NULL) g_acpf_pool[here].sz = 1u;
        break;
      }
      case TAG_FVR: {
        u32 old = term_ext(cur);
        u32 d = 0u;
        for (; d < m->n; d++) {
          if (m->old_id[d] == old) break;
        }
        if (d == m->n) {
          if (m->n >= REWRITE_MAX_VAR) return 0;   // full map; fall back
          m->old_id[m->n++] = old;
          if (fz != NULL) fz->bal[d] = 0;
        }
        *bp++ = (u8)'V';
        acp_put_varint(&bp, d);
        lpos = (u32)(bp - g_acp_scratch);
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          g_acpf_pool[lfpos].sym = -(i32)(d + 1u);
          g_acpf_pool[lfpos].w   = cfg->var_weight;
          g_acpf_pool[lfpos].sz  = 1u;
          g_acpf_pool[lfpos].ar  = 0u;
          lfpos++;
          fz->phidiff += (long long)sign * (long long)cfg->var_weight;
          fz->bal[d]  += sign;
        }
        break;
      }
      default:
        return 0;   // NUM / ERA atom: classic path handles
    }
    // Ascend: advance to the next pending sibling, fixing up each
    // completed CTR's subtree span on the way out.
    for (;;) {
      if (sp == 0u) {
        *pos = lpos; *nodes = lnodes; *fpos = lfpos;
        return 1;
      }
      VcpFrame *f = &g_vcp_stk[sp - 1u];
      if (++f->i < f->n) {
        cur = vcp_child(f, f->i, p, p_len, rin, &cur_spd);
        break;
      }
      if (fz != NULL) g_acpf_pool[f->here].sz = lfpos - f->here;
      sp--;
    }
  }
}

// Full fused virtual emit of the formed CP.  On success the packed
// bytes / counts sit in g_vcp_emit (NOT yet armed -- the caller arms
// around the push, exactly the g_cp_rawsz_hint discipline) and the
// fused-KBO weigh state is live in g_acp_fuse.  0 on an anomaly
// (nothing armed; the caller runs the classic Term build).
static u8 vcp_emit_fused(const KboConfig *cfg, Term lo, const u32 *p,
                         u32 p_len, Term rin, Term ro,
                         const RewriteSubst *subst) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
  if (!vcp_emit_side(ro, NULL, 0u, 0, subst, cfg, &fz->map,
                     &pos, &nodes, fz, +1, &fpos)) {
    return 0u;
  }
  u32 lhs_nodes = nodes, fl = fpos;
  if (!vcp_emit_side(lo, p, p_len, rin, subst, cfg, &fz->map,
                     &pos, &nodes, fz, -1, &fpos)) {
    return 0u;
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  g_vcp_emit.armed     = 0u;
  g_vcp_emit.len       = pos;
  g_vcp_emit.nodes     = nodes;
  g_vcp_emit.lhs_nodes = lhs_nodes;
  return 1u;
}

// --- Treated (< 50) FT-direct route (phase 2b) ----------------------
//
// The treated class flows through the WM KPBehandelt push-norm
// treatment: classic pipeline = thvm_unify_apply x2 (heap Terms) ->
// ft_from_term x2 -> FT normalize -> [joined? drop : ft_to_term x2 ->
// trace pack + queue pack].  Under the same route conditions as the
// raw class, the ONLY consumers of the built Terms are (a) the
// ft_from_term boundary into the FT treatment and (b) the two byte
// serializers on the survivor path.  So: build the FT cells STRAIGHT
// from the parents + unifier (vcp_ft_build_rec -- the ft_from_term
// image of the composite, cell-identical by the same resolve
// argument as the raw emit), serialize the renumbered RAW pair bytes
// for the trace at formation (vcp_emit_side, bytes-only), treat the
// cells directly (the atp_cp_trivially_joinable FT core), and pack a
// survivor's NF cells with ftp_pack_fused below -- the heap Term
// intermediate dies for the whole class, along with its
// ft_from_term / ft_to_term boundary walks.
//
// Compiled only with the FT stages (the treatment itself needs them);
// without THVM_ATPFT_NORM a never-armed stub keeps the two byte
// consumers' guards compiling.
#ifdef THVM_ATPFT_NORM
typedef struct {
  u8         armed;     // stash live for exactly the one armed CP push
  AtpFtCell *fl, *fr;   // raw pair FT cells; the treatment rewrites
                        // them to the NF cells in place (same stash)
  u32        raw_len;   // renumbered raw pair bytes at g_acp_scratch[0,raw_len)
                        // (0 = not emitted yet: the raw-pair emit is
                        // LAZY, run by the trace store for survivors
                        // only -- a joined CP never pays it)
  // Lazy-emit operands: the formation faces + the walk subst, all
  // stable through the one armed push (the subst is the former's
  // static, reset per attempt; the spans are rule mirrors / the
  // subject, untouched by the CP's own treatment).
  const AtpFtCell   *lo, *sub, *rin, *ro;
  const AtpFtSubst  *subst;
  // Simpler-join pop-peak: sigma(lo) rebuilt by vcp_treat_joinable
  // (raw formation var space, same as fl/fr) and kept for the push
  // loop's peak-stash pack; owned by the stash, freed on release.
  AtpFtCell *peak;
} VcpTreat;
#else
typedef struct {
  u8  armed;            // never set without the FT stages
  u32 raw_len;
} VcpTreat;
#endif
static VcpTreat g_vcp_treat;

// Arena-A net-retention attribution (mem-census): net persistent-cell
// growth (allocs - frees) bracketed by phase.  Unconditional i64
// arithmetic on existing counters -- no branch, no behavior change.
// Consumed by the bench's THVM_ATP_MEM_CENSUS report.
#ifdef THVM_ATPFT_ALLOC
i64 g_ftnet_form = 0;   // sw_form_push (formation + treat + queue push)
i64 g_ftnet_step = 0;   // whole thvm_atp_step
i64 g_ftnet_treatform = 0;   // vcp_ft_treat_form / vcp_treat_form / emit
i64 g_ftnet_join      = 0;   // vcp_treat_joinable (treat normalize + SJ)
i64 g_ftnet_push      = 0;   // atp_cp_heap_push
i64 g_ftnet_release   = 0;   // tail vcp_treat_release
static inline i64 ftnet_now(void) {
  return (i64)(g_ft_persist_alloc_total - g_ft_free_span_total);
}
#endif

u64 g_vcp_treat_n_ok   = 0;   // treated CPs formed FT-direct
u64 g_vcp_treat_n_fb   = 0;   // anomaly fallbacks to the classic build
u64 g_vcp_treat_n_pkfb = 0;   // NF-pack anomaly fallbacks (heap push)
u64 g_vcp_gate_n_test  = 0;   // eq-parent CPs whose order gate ran virtually
u64 g_vcp_gate_n_drop  = 0;   // ... of which dropped at formation
u64 g_vcp_gate_n_fb    = 0;   // ... anomaly fallbacks to the classic gate
u64 g_vcp_lb_hit       = 0;   // PROFILE=2 probe: raw CPs the unsubst LB catches
u64 g_vcp_lb_raw       = 0;   // PROFILE=2 probe: raw CPs total

// later-25 per-CP walk-count attribution (PROFILE=2 only; g_atp_phase_detail
// gated so timed runs are unperturbed).  Decompose CP formation into its
// separate composite traversals: the KPBehandelt size-gate count walk
// (vcp_count_side, capped at the gate), the raw-class emit walk
// (vcp_emit_side, bytes + flat-KBO), and the treated-class fused build+emit
// walk (vcp_ft_build_emit_rec, bytes + FT cells).  `_calls` = walk
// invocations (one per side), `_nodes` = node-visits summed across all walks.
u64 g_vcp_cnt_calls = 0, g_vcp_cnt_nodes = 0;   // count-side walk (size gate)
u64 g_vcp_emt_calls = 0, g_vcp_emt_nodes = 0;   // raw emit-side walk
u64 g_vcp_bem_calls = 0, g_vcp_bem_nodes = 0;   // treated build+emit walk

// later-25 heap-Term interchange counters (PROFILE=2 only): the dual-rep
// round-trips.  ft_from_term (Term -> FT) + ft_to_term (FT -> Term) +
// acp_unpack_term (packed bytes -> Term) invocations and node-visits, so the
// steady-state Term<->FT<->packed interchange volume can be sized against the
// tiles.  Defined here, incremented from ft.c (extern) + acp_unpack_term.
u64 g_ft_from_calls = 0, g_ft_from_nodes = 0;
u64 g_ft_to_calls   = 0, g_ft_to_nodes   = 0;
// Region-tagged attribution of the two interchange counters: the step
// loop stamps g_ft_region at each phase bracket (same spots as the
// phase timers) so the PROFILE=2 dump can say WHERE the conversions
// happen, not just how many.  0=other 1=pop-norm 2=interreduce
// 3=cp-gen/push 4=goal-check.
u8  g_ft_region = 0u;
u64 g_ft_from_by_region[8] = {0};
u64 g_ft_to_by_region[8]   = {0};
u64 g_acp_unterm_calls = 0, g_acp_unterm_nodes = 0;

#ifdef THVM_ATPFT_NORM
// FT-cell image of one virtual side: ft_from_term's exact recursive
// construction (children first, then ftnew_ctr stitches -- same arena
// allocation order, same GROUND fold, same var-id encoding), with the
// node source swapped to the resolve-through-subst composite.  NULL on
// an anomaly (NUM/ERA atom, torn arity cell, over-arity); partially
// built child spans are returned to the free list so a bail leaves the
// arena exactly as the classic path (which never built cells for the
// dropped candidate) would.
static AtpFtCell *vcp_ft_build_rec(AtpFt *a, Term t, u32 spd,
                                   const u32 *p, u32 p_len, Term rin,
                                   const RewriteSubst *subst) {
  t = vcp_resolve(t, subst);
  switch (term_tag(t)) {
    case TAG_FVR:
      return ftnew_var(a, term_ext(t), 0);
    case TAG_CTR: {
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return NULL;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return NULL;
      u32 lab = term_ext(t);
      if (n == 0u) return ftnew_const(a, lab, 0);
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        Term c = heap_read(base + 1u + (u64)i);
        u32 cspd = VCP_SPD_NONE;
        if (spd != VCP_SPD_NONE && p[spd] == i) {
          if (spd + 1u == p_len) c = rin;
          else cspd = spd + 1u;
        }
        kids[i] = vcp_ft_build_rec(a, c, cspd, p, p_len, rin, subst);
        if (kids[i] == NULL) {
          for (u32 k = 0; k < i; k++) {
            ft_free_span(a, kids[k], kids[k]->end);
          }
          return NULL;
        }
      }
      return ftnew_ctr(a, lab, (u16)n, kids, 0);
    }
    default:
      return NULL;   // NUM / ERA atom: classic path handles
  }
}

static AtpFtCell *vcp_ft_build_side(AtpFt *a, Term root, const u32 *p,
                                    u32 p_len, Term rin,
                                    const RewriteSubst *subst) {
  Term t  = (rin != 0 && p_len == 0u) ? rin : root;
  u32 spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  return vcp_ft_build_rec(a, t, spd, p, p_len, rin, subst);
}

// Fused build+emit of one virtual side: vcp_ft_build_rec's exact cell
// construction with vcp_emit_side's bytes-only emission folded into
// the SAME resolve walk -- the treated formation's two composite
// traversals (raw trace bytes, then FT cells) become one.  Bytes are
// emitted preorder at node entry ('C'/'V' before children), so the
// byte stream and the AcpNvMap first-occurrence renumbering coincide
// exactly with the two-walk form; the cells keep ORIGINAL var ids
// (the byte side renumbers, matching acp_trace_pack_fused).  NULL on
// an anomaly (the union of both walks' classes: NUM/ERA atom, torn
// arity, over-arity, full renumber map, depth cap); partially built
// child spans are freed and the scratch is left dirty -- the caller
// falls back to the classic build, which never reads these bytes.
// Certified per CP by vcp_treat_certify (raw-byte equality + cell
// identity vs the classic rebuild), unchanged.
static AtpFtCell *vcp_ft_build_emit_rec(AtpFt *a, Term t, u32 spd,
                                        const u32 *p, u32 p_len, Term rin,
                                        const RewriteSubst *subst,
                                        AcpNvMap *m, u32 *pos, u32 depth) {
  t = vcp_resolve(t, subst);
  if (g_atp_phase_detail) { extern u64 g_vcp_bem_nodes; g_vcp_bem_nodes++; }
  acp_scratch_ensure(*pos, 21u);
  u8 *bp = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return NULL;
        m->old_id[m->n++] = old;
      }
      *bp++ = (u8)'V';
      acp_put_varint(&bp, d);
      *pos = (u32)(bp - g_acp_scratch);
      return ftnew_var(a, old, 0);
    }
    case TAG_CTR: {
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return NULL;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return NULL;
      u32 lab = term_ext(t);
      *bp++ = (u8)'C';
      acp_put_varint(&bp, lab);
      acp_put_varint(&bp, n);
      *pos = (u32)(bp - g_acp_scratch);
      if (n == 0u) return ftnew_const(a, lab, 0);
      if (depth >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) {
        Term c = heap_read(base + 1u + (u64)i);
        u32 cspd = VCP_SPD_NONE;
        if (spd != VCP_SPD_NONE && p[spd] == i) {
          if (spd + 1u == p_len) c = rin;
          else cspd = spd + 1u;
        }
        kids[i] = vcp_ft_build_emit_rec(a, c, cspd, p, p_len, rin, subst,
                                        m, pos, depth + 1u);
        if (kids[i] == NULL) {
          for (u32 k = 0; k < i; k++) {
            ft_free_span(a, kids[k], kids[k]->end);
          }
          return NULL;
        }
      }
      return ftnew_ctr(a, lab, (u16)n, kids, 0);
    }
    default:
      return NULL;   // NUM / ERA atom: classic path handles
  }
}

static AtpFtCell *vcp_ft_build_emit_side(AtpFt *a, Term root, const u32 *p,
                                         u32 p_len, Term rin,
                                         const RewriteSubst *subst,
                                         AcpNvMap *m, u32 *pos) {
  Term t  = (rin != 0 && p_len == 0u) ? rin : root;
  u32 spd = (rin != 0 && p_len > 0u) ? 0u : VCP_SPD_NONE;
  if (g_atp_phase_detail) { extern u64 g_vcp_bem_calls; g_vcp_bem_calls++; }
  return vcp_ft_build_emit_rec(a, t, spd, p, p_len, rin, subst, m, pos, 0u);
}

// Renumbered raw pair bytes of the virtual CP (no flat encode, no
// balances): the acp_trace_pack_fused image of the composite.  Serves
// the treated route's trace store.  0 on an anomaly.
static u8 vcp_emit_pair_bytes(Term lo, const u32 *p, u32 p_len, Term rin,
                              Term ro, const RewriteSubst *subst,
                              u32 *len) {
  AcpNvMap m;
  m.n = 0u;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  // WM side order: sigma(ro) first (see the classic arm).
  if (!vcp_emit_side(ro, NULL, 0u, 0, subst, NULL, &m,
                     &pos, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (!vcp_emit_side(lo, p, p_len, rin, subst, NULL, &m,
                     &pos, &nodes, NULL, -1, &fpos)) {
    return 0u;
  }
  *len = pos;
  return 1u;
}

// FT-side fused pack: acp_pack_fused's exact byte / flat-node /
// balance emission with the node source swapped from Term cells to an
// AtpFt span -- the treated survivor's NF cells go straight to packed
// bytes + the armed fused-KBO weigh state, no ft_to_term.  The span's
// `next` chain IS the preorder, so the walk is a linear scan with an
// arity-countdown frame stack for the subtree-span fixups.  NULL on an
// anomaly (converted NUM / foreign-tag marker syms, over-arity, depth
// cap, > REWRITE_MAX_VAR distinct vars); nothing armed and the caller
// decodes the cells and runs the classic pipeline.  want_buf == 0
// skips the byte-string malloc (bytes stay in the scratch) and returns
// the scratch pointer as a NOT-owned success sentinel -- the mode-8
// byte-queue-retirement caller stores NULL and the adopted FT spans
// carry the queue payload.
static u8 *ftp_pack_fused(const KboConfig *cfg, const AtpFtCell *lhs,
                          const AtpFtCell *rhs, u32 *out_len,
                          u32 *out_nodes, u32 *out_lhs_nodes,
                          u8 want_buf) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u, lhs_nodes = 0u, fl = 0u;
  u32 nvar = 0u, nvar_l = 0u;   // var occurrences (total / lhs), twee score
  static u32 here_stk[ACP_FUSE_MAX_DEPTH];
  static u32 rem_stk[ACP_FUSE_MAX_DEPTH];
  for (u32 side = 0u; side < 2u; side++) {
    const AtpFtCell *c = (side == 0u) ? lhs : rhs;
    int sign = (side == 0u) ? +1 : -1;
    u32 sp = 0u;
    for (;;) {
      nodes++;
      acp_scratch_ensure(pos, 21u);
      u8 *bp = g_acp_scratch + pos;
      u8 leaf;
      if (ft_is_var(c)) {
        u32 old = ft_var_id(c);
        u32 d = 0u;
        for (; d < fz->map.n; d++) {
          if (fz->map.old_id[d] == old) break;
        }
        if (d == fz->map.n) {
          if (fz->map.n >= REWRITE_MAX_VAR) return NULL;
          fz->map.old_id[fz->map.n++] = old;
          fz->bal[d] = 0;
        }
        *bp++ = (u8)'V';
        acp_put_varint(&bp, d);
        pos = (u32)(bp - g_acp_scratch);
        acpf_ensure(fpos + 1u);
        g_acpf_pool[fpos].sym = -(i32)(d + 1u);
        g_acpf_pool[fpos].w   = cfg->var_weight;
        g_acpf_pool[fpos].sz  = 1u;
        g_acpf_pool[fpos].ar  = 0u;
        fpos++;
        fz->phidiff += (long long)sign * (long long)cfg->var_weight;
        fz->bal[d]  += sign;
        nvar++;
        leaf = 1u;
      } else {
        u32 lab = ft_ctr_sym(c);
        // Converted-NUM / foreign-tag marker namespace (ft.c): the
        // classic Term pack would emit 'N' / bail here -- classic path.
        if (lab >= 0x20000000u) return NULL;
        u32 n = c->arity;
        if (n > REWRITE_MAX_ARITY) return NULL;
        *bp++ = (u8)'C';
        acp_put_varint(&bp, lab);
        acp_put_varint(&bp, n);
        pos = (u32)(bp - g_acp_scratch);
        acpf_ensure(fpos + 1u);
        u32 here = fpos++;
        u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
        g_acpf_pool[here].sym = (i32)lab;
        g_acpf_pool[here].w   = w;
        g_acpf_pool[here].ar  = n;
        fz->phidiff += (long long)sign * (long long)w;
        if (n > 0u) {
          if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
          here_stk[sp] = here;
          rem_stk[sp]  = n;
          sp++;
          leaf = 0u;
        } else {
          g_acpf_pool[here].sz = 1u;
          leaf = 1u;
        }
      }
      if (leaf) {
        // Complete enclosing subtrees whose last child this was.
        u8 done = 0u;
        for (;;) {
          if (sp == 0u) { done = 1u; break; }
          if (--rem_stk[sp - 1u] > 0u) break;
          g_acpf_pool[here_stk[sp - 1u]].sz = fpos - here_stk[sp - 1u];
          sp--;
        }
        if (done) break;
      }
      c = c->next;
      if (c == NULL) return NULL;   // torn span; classic path
    }
    if (side == 0u) {
      lhs_nodes = nodes;
      fl        = fpos;
      nvar_l    = nvar;
    }
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  // ATP_CP_WEIGHT_TWEE per-side scores (see the AcpFuse field comment):
  // plain weighted count over the unshared treated NF -- 7 per function
  // symbol / constant, 6 per variable occurrence.  Exact vs the classic
  // atp_twee_term_score(nv(ft_to_term(side))) whenever the side stays
  // under the classic walk's 256-entry stack (a <=256-node side can
  // never overflow it, nor saturate the 512-slot seen table).
  fz->twee_l     = 7u * fz->fl_n - (nvar_l);
  fz->twee_r     = 7u * fz->fr_n - (nvar - nvar_l);
  fz->twee_valid = (fz->fl_n <= 256u && fz->fr_n <= 256u) ? 1u : 0u;
  // STRUCTURAL shared-subterm discount (THVM_ATP_TWEE_SD): the plain
  // 7*n - nvar count above has no dup discount (exact only vs the
  // pointer-dedup classic scorer on unshared cells).  Under the knob
  // the consumer must materialize and score with the structural
  // dedup, so decline the stashed pair.
  {
    static int sd_on = -1;
    if (sd_on < 0) { const char *e = getenv("THVM_ATP_TWEE_SD");
                     sd_on = (e != NULL && e[0] != '\0' && e[0] != '0');
                     e = getenv("THVM_ATP_TWEE_EQB");
                     // --twee-eq-bonus supersets the structural
                     // discount (same size' in the reference): the
                     // consumer must materialize and score, so the
                     // stash declines under either knob.
                     if (e != NULL && e[0] != '\0' && e[0] != '0')
                       sd_on = 1; }
    if (sd_on) fz->twee_valid = 0u;
  }
  *out_len       = pos;
  *out_nodes     = nodes;
  *out_lhs_nodes = lhs_nodes;
  if (!want_buf) return g_acp_scratch;   // success sentinel, not owned
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("ftp_pack_fused: OOM");
  memcpy(buf, g_acp_scratch, pos);
  return buf;
}

// Treated (< 50) FT-direct formation: emit the renumbered raw pair
// bytes into the scratch (they sit at g_acp_scratch[0, raw_len) until
// the trace store consumes them -- audited: nothing between formation
// and the trace push touches the scratch on the routed path) and
// build the raw pair's FT cells straight from the composite.  On
// success the stash holds both, NOT yet armed (the caller arms around
// the push).  0 on an anomaly: nothing allocated, nothing armed, the
// caller runs the classic Term build.
static u8 vcp_treat_form(AtpState *s, Term lo, const u32 *p, u32 p_len,
                         Term rin, Term ro, const RewriteSubst *subst) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Fused single-walk formation (default; THVM_ATP_NO_VCP_FUSEWALK=1
  // restores the two-walk form for one bake round): bytes + cells from
  // ONE composite traversal per side.
  static int no_fusewalk = -1;
  if (no_fusewalk < 0) no_fusewalk = atp_env_on("THVM_ATP_NO_VCP_FUSEWALK");
  u32 raw_len = 0u;
  AtpFtCell *fl, *fr;
  if (!no_fusewalk) {
    AcpNvMap m;
    m.n = 0u;
    // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
    fl = vcp_ft_build_emit_side(a, ro, NULL, 0u, 0, subst, &m, &raw_len);
    if (fl == NULL) return 0u;
    fr = vcp_ft_build_emit_side(a, lo, p, p_len, rin, subst, &m, &raw_len);
    if (fr == NULL) {
      ft_free_span(a, fl, fl->end);
      return 0u;
    }
  } else {
    if (!vcp_emit_pair_bytes(lo, p, p_len, rin, ro, subst, &raw_len)) {
      return 0u;
    }
    fl = vcp_ft_build_side(a, ro, NULL, 0u, 0, subst);
    if (fl == NULL) return 0u;
    fr = vcp_ft_build_side(a, lo, p, p_len, rin, subst);
    if (fr == NULL) {
      ft_free_span(a, fl, fl->end);
      return 0u;
    }
  }
  g_vcp_treat.armed   = 0u;
  g_vcp_treat.fl      = fl;
  g_vcp_treat.fr      = fr;
  g_vcp_treat.raw_len = raw_len;
  // Term-operand route: no FT faces / walk subst back this stash.
  // Clear them so the simpler-join peak rebuild (vcp_treat_joinable)
  // can never consume a PREVIOUS FT-route CP's stale operands -- it
  // degrades to the peakless verdict on this route instead.
  g_vcp_treat.lo      = NULL;
  g_vcp_treat.sub     = NULL;
  g_vcp_treat.rin     = NULL;
  g_vcp_treat.ro      = NULL;
  g_vcp_treat.subst   = NULL;
  g_vcp_treat.peak    = NULL;
  return 1u;
}

// Release a formed-but-unpushed treated stash (a post-formation gate
// dropped the CP): the classic path never built cells for a dropped
// candidate, so return the spans to the free list.
static void vcp_treat_release(AtpState *s) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  if (g_vcp_treat.fl != NULL) ft_free_span(a, g_vcp_treat.fl, g_vcp_treat.fl->end);
  if (g_vcp_treat.fr != NULL) ft_free_span(a, g_vcp_treat.fr, g_vcp_treat.fr->end);
  if (g_vcp_treat.peak != NULL) {
    ft_free_span(a, g_vcp_treat.peak, g_vcp_treat.peak->end);
    g_vcp_treat.peak = NULL;
  }
  g_vcp_treat.fl = g_vcp_treat.fr = NULL;
  g_vcp_treat.armed = 0u;
}
#endif // THVM_ATPFT_NORM

// --- FT-emit virtual order gate (phase 2a) ---------------------------
//
// The WM KPAction ordering gate (atp_cp_order_gate) is the only peak
// consumer on the routed path: for an equation parent it drops the CP
// at formation when
//   outer (o_eq):  sigma(ro)  >kbo  sigma(lo)      (cp.rhs vs cp.peak)
//   inner (in_eq): sigma(rin) >kbo  sigma(sub)     (the overlap-position
//                                    subterms of cp.lhs / cp.peak)
// after a joint thvm_normalize_vars.  Every operand is a plain
// resolve-through-subst composite of a term the walk former already
// holds, so the verdict is computable without building ANY of the
// three CP Terms: two bytes+flat walks into a LOCAL fuse state, then
// the flat-KBO mirror (acp_fuse_kbo_at == thvm_kbo_nomemo on the
// jointly-renumbered pair == the classic gate's normalize+compare;
// route conditions pin atp_compare to plain KBO).  Returns 1 with
// *drop set; 0 on an anomaly (caller falls back to the classic Term
// build, whose classic gate then decides).
static u8 vcp_order_gate_virtual(AtpState *s, Term lo, Term rin, Term ro,
                                 Term sub, const RewriteSubst *subst,
                                 u8 o_eq, u8 in_eq, u8 *drop) {
  *drop = 0u;
  AcpFuse fz;
  u32 pos, nodes, fpos, fl;
  if (o_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_emit_side(ro, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_emit_side(lo, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) {
      *drop = 1u;
      return 1u;
    }
  }
  if (in_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_emit_side(rin, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_emit_side(sub, NULL, 0u, 0, subst, s->kbo, &fz.map,
                       &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) *drop = 1u;
  }
  return 1u;
}

// Composite fused emit on spans: the FT sibling of vcp_emit_side --
// walk `c`'s composite (splice `rin` at cell `*at_io`, resolve vars
// through `subst`, chains followed) emitting the same 'C'/'V' byte
// stream into g_acp_scratch and the same flat-KBO pool entries /
// balances into `fz` (joint appearance-order renumber via `m`).  A
// TAG_NUM marker or a full var map returns 0 -- the same anomaly
// cliffs that send the Term emitter to the classic build.  Recursive
// (composite depth); the Term version's frame cap plays the same
// role.  The CP-walk port's serializer; no caller yet.
#if defined(THVM_ATPFT_NORM)
// Iterative emit core: the preorder chain IS the recursive visit
// order, so CTR descent needs no calls at all -- only a bound-var
// jump (emit the binding's span, resume after the var) and the one
// splice point push a resume frame.  Byte- and pool-identical to the
// recursive walk it replaces (same appearance-order var map, same
// scratch bytes, same fuse entries; the CTR sz backfill runs as a
// linear post-pass over the new flat entries).  The splice test is
// scoped to SUBJECT frames only, exactly like the recursion's
// at_io=NULL hand-off into bindings (a binding can BE the splice
// cell when the inner lhs is a bare variable).
static u8 vcp_ft_emit_cell(const AtpFtCell *c, const AtpFtCell **at_io,
                           const AtpFtCell *rin, const AtpFtSubst *subst,
                           const KboConfig *cfg, AcpNvMap *m,
                           u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                           u32 *fpos) {
  static struct { const AtpFtCell *next, *stop; u8 subj; }
      rs[ACP_FUSE_MAX_DEPTH];   // single-threaded engine: static, not stack
  u32 sp = 0u;
  const u32 f0 = (fz != NULL) ? *fpos : 0u;
  const AtpFtCell *cur  = c;
  const AtpFtCell *stop = (c->end != NULL) ? c->end->next : NULL;
  u8 subj = 1u;
  for (;;) {
    while (cur == stop) {
      if (sp == 0u) goto done;
      --sp;
      cur  = rs[sp].next;
      stop = rs[sp].stop;
      subj = rs[sp].subj;
    }
    if (subj && at_io != NULL && *at_io == cur) {
      *at_io = NULL;                     // single splice point
      if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
      rs[sp].next = (cur->end != NULL) ? cur->end->next : stop;
      rs[sp].stop = stop;
      rs[sp].subj = 1u;
      sp++;
      cur  = rin;
      stop = (rin->end != NULL) ? rin->end->next : NULL;
      subj = 0u;
      continue;
    }
    if ((cur->sym & WF_VAR_BIT) != 0u) {
      u32 id = cur->sym & ~WF_VAR_BIT;
      const AtpFtCell *b =
          (subst != NULL && id < ATPFT_MAX_VARS) ? subst->bind[id] : NULL;
      if (b != NULL) {
        if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
        rs[sp].next = cur->next;         // var is a leaf: end == self
        rs[sp].stop = stop;
        rs[sp].subj = subj;
        sp++;
        cur  = b;
        stop = (b->end != NULL) ? b->end->next : NULL;
        subj = 0u;
        continue;
      }
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == id) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;   // full map; fall back
        m->old_id[m->n++] = id;
        if (fz != NULL) fz->bal[d] = 0;
      }
      (*nodes)++;
      acp_scratch_ensure(*pos, 21u);
      u8 *bp = g_acp_scratch + *pos;
      *bp++ = (u8)'V';
      acp_put_varint(&bp, d);
      *pos = (u32)(bp - g_acp_scratch);
      if (fz != NULL) {
        acpf_ensure(*fpos + 1u);
        g_acpf_pool[*fpos].sym = -(i32)(d + 1u);
        g_acpf_pool[*fpos].w   = cfg->var_weight;
        g_acpf_pool[*fpos].sz  = 1u;
        g_acpf_pool[*fpos].ar  = 0u;
        (*fpos)++;
        fz->phidiff += (long long)sign * (long long)cfg->var_weight;
        fz->bal[d]  += sign;
      }
      cur = cur->next;
      continue;
    }
    if (ft_ctr_sym(cur) == ATPFT_NUM_MARKER) return 0;  // NUM: classic path
    u32 lab = ft_ctr_sym(cur);
    u32 n   = (u32)cur->arity;
    if (n > REWRITE_MAX_ARITY) return 0;
    (*nodes)++;
    acp_scratch_ensure(*pos, 21u);
    u8 *bp = g_acp_scratch + *pos;
    *bp++ = (u8)'C';
    acp_put_varint(&bp, lab);
    acp_put_varint(&bp, n);
    *pos = (u32)(bp - g_acp_scratch);
    if (fz != NULL) {
      acpf_ensure(*fpos + 1u);
      u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
      g_acpf_pool[*fpos].sym = (i32)lab;
      g_acpf_pool[*fpos].w   = w;
      g_acpf_pool[*fpos].ar  = n;
      (*fpos)++;
      fz->phidiff += (long long)sign * (long long)w;
    }
    cur = cur->next;                     // preorder: first kid (or next)
  }
done:
  if (fz != NULL) {
    // sz backfill: preorder + arity determine every CTR's flat-subtree
    // length (identical to the recursion's *fpos - here).
    static struct { u32 idx; u32 kids; } open[ACP_FUSE_MAX_DEPTH];
    u32 on = 0u;
    for (u32 i = f0; i < *fpos; i++) {
      u32 ar = g_acpf_pool[i].ar;
      if (ar > 0u) {
        if (on >= (u32)ACP_FUSE_MAX_DEPTH) return 0;
        open[on].idx = i;
        open[on].kids = ar;
        on++;
        continue;
      }
      // Leaf (var or 0-ary CTR): sz = 1, then cascade completions.
      g_acpf_pool[i].sz = 1u;
      while (on > 0u && --open[on - 1u].kids == 0u) {
        on--;
        g_acpf_pool[open[on].idx].sz = i + 1u - open[on].idx;
      }
    }
  }
  return 1;
}

static u8 vcp_ft_emit_side(const AtpFtCell *root, const AtpFtCell *at,
                           const AtpFtCell *rin, const AtpFtSubst *subst,
                           const KboConfig *cfg, AcpNvMap *m,
                           u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                           u32 *fpos) {
  const AtpFtCell *at_local = at;
  // Top-level splice (p_len == 0): the whole side IS the inner face.
  const AtpFtCell *start = (at != NULL && at == root) ? rin : root;
  const AtpFtCell **aio  = (at != NULL && at == root) ? NULL : &at_local;
  return vcp_ft_emit_cell(start, aio, rin, subst, cfg, m, pos, nodes,
                          fz, sign, fpos);
}
#endif // THVM_ATPFT_NORM

// FT sibling of vcp_order_gate_virtual: the WM KPAction eq-parent
// ordering gate decided entirely on span composites -- two
// vcp_ft_emit_side walks into a LOCAL fuse state, then the flat-KBO
// mirror, per active parent face.  Returns 1 with *drop set; 0 on an
// anomaly (caller falls back to the Term route).  The CP-walk port's
// gate; no caller yet.
#if defined(THVM_ATPFT_NORM)
static u8 vcp_ft_order_gate(AtpState *s, const AtpFtCell *lo,
                            const AtpFtCell *rin, const AtpFtCell *ro,
                            const AtpFtCell *sub, const AtpFtSubst *subst,
                            u8 o_eq, u8 in_eq, u8 *drop) {
  *drop = 0u;
  AcpFuse fz;
  u32 pos, nodes, fpos, fl;
  if (o_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_ft_emit_side(ro, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_ft_emit_side(lo, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) {
      *drop = 1u;
      return 1u;
    }
  }
  if (in_eq) {
    fz.map.n = 0u; fz.phidiff = 0;
    pos = nodes = fpos = 0u;
    if (!vcp_ft_emit_side(rin, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, +1, &fpos)) {
      return 0u;
    }
    fl = fpos;
    if (!vcp_ft_emit_side(sub, NULL, NULL, subst, s->kbo, &fz.map,
                          &pos, &nodes, &fz, -1, &fpos)) {
      return 0u;
    }
    fz.fl_n = fl;
    fz.fr_n = fpos - fl;
    if (acp_fuse_kbo_at(s->kbo, &fz, g_acpf_pool) == KBO_GT) *drop = 1u;
  }
  return 1u;
}
#endif // THVM_ATPFT_NORM

// Composite CELL builder on spans: the FT sibling of
// vcp_ft_build_emit_rec -- build the formed CP side's AtpFt cells
// straight from the span composite (splice `rin` at the overlap cell,
// resolve vars through the AtpFtSubst, chains followed), applying the
// same joint appearance-order renumber the byte emit uses (shared
// AcpNvMap), through the same ftnew_* constructors.  NULL on an
// anomaly (TAG_NUM marker / full var map / arity cliff) -- caller
// falls back to the Term build.  The CP-walk port's treated-class
// builder; no caller yet.
#if defined(THVM_ATPFT_NORM)
// Preorder-STREAMING build: cells allocate in visit order (the slab
// bump makes the fresh CP span physically adjacent -- WM's
// SigmaRGanzNeu discipline; every downstream walk on it gets the
// cache locality), next-threaded as emitted, end pointers + the
// GROUND fold patched by a completion cascade.  Field-identical to
// the recursive ftnew_* build it replaces; addresses differ only in
// adjacency.  Splice test scoped to subject frames (a binding can BE
// the splice cell).  NULL on the same anomaly set (NUM marker, full
// map, arity cliff, frame cliff); partials leak into the arena.
static AtpFtCell *vcp_ft_build_cell(AtpFt *a, const AtpFtCell *c,
                                    const AtpFtCell **at_io,
                                    const AtpFtCell *rin,
                                    const AtpFtSubst *subst,
                                    AcpNvMap *m) {
  static struct { const AtpFtCell *next, *stop; u8 subj; }
      rs[ACP_FUSE_MAX_DEPTH];
  static struct { AtpFtCell *cell; u32 kids; } open[ACP_FUSE_MAX_DEPTH];
  u32 sp = 0u, on = 0u;
  AtpFtCell *root_out = NULL, *prev = NULL;
  const AtpFtCell *cur = c;
  const AtpFtCell *stop = (c->end != NULL) ? c->end->next : NULL;
  u8 subj = 1u;
  for (;;) {
    while (cur == stop) {
      if (sp == 0u) goto done;
      --sp;
      cur  = rs[sp].next;
      stop = rs[sp].stop;
      subj = rs[sp].subj;
    }
    if (subj && at_io != NULL && *at_io == cur) {
      *at_io = NULL;
      if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
      rs[sp].next = (cur->end != NULL) ? cur->end->next : stop;
      rs[sp].stop = stop;
      rs[sp].subj = 1u;
      sp++;
      cur  = rin;
      stop = (rin->end != NULL) ? rin->end->next : NULL;
      subj = 0u;
      continue;
    }
    AtpFtCell *nc;
    u8 leaf_flags;
    if ((cur->sym & WF_VAR_BIT) != 0u) {
      u32 id = cur->sym & ~WF_VAR_BIT;
      const AtpFtCell *b =
          (subst != NULL && id < ATPFT_MAX_VARS) ? subst->bind[id] : NULL;
      if (b != NULL) {
        if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
        rs[sp].next = cur->next;
        rs[sp].stop = stop;
        rs[sp].subj = subj;
        sp++;
        cur  = b;
        stop = (b->end != NULL) ? b->end->next : NULL;
        subj = 0u;
        continue;
      }
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == id) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return NULL;
        m->old_id[m->n++] = id;
      }
      nc = ft_alloc_one(a, 0);
      nc->sym   = WF_VAR_BIT | d;
      nc->arity = 0;
      nc->flags = 0;
      nc->next  = NULL;
      nc->end   = nc;
      ft_raw_check(nc);
      leaf_flags = 0u;
      cur = cur->next;
    } else {
      if (ft_ctr_sym(cur) == ATPFT_NUM_MARKER) return NULL;
      u32 lab = ft_ctr_sym(cur);
      u32 n   = (u32)cur->arity;
      if (n > REWRITE_MAX_ARITY) return NULL;
      nc = ft_alloc_one(a, 0);
      nc->sym   = lab & ~WF_VAR_BIT;
      nc->arity = (u16)n;
      nc->flags = ATPFT_FLAG_GROUND;   // fold clears on a var below
      nc->next  = NULL;
      nc->end   = nc;
      ft_raw_check(nc);
      if (n > 0u) {
        if (on >= (u32)ACP_FUSE_MAX_DEPTH) return NULL;
        if (prev != NULL) prev->next = nc;
        else              root_out = nc;
        prev = nc;
        open[on].cell = nc;
        open[on].kids = n;
        on++;
        cur = cur->next;               // preorder: first kid
        continue;
      }
      leaf_flags = ATPFT_FLAG_GROUND;
      cur = cur->next;
    }
    // Leaf emitted: thread + cascade completions upward.
    if (prev != NULL) prev->next = nc;
    else              root_out = nc;
    prev = nc;
    u8 child_flags = leaf_flags;
    while (on > 0u) {
      AtpFtCell *pc = open[on - 1u].cell;
      if ((child_flags & ATPFT_FLAG_GROUND) == 0u)
        pc->flags &= (u8)~ATPFT_FLAG_GROUND;
      if (--open[on - 1u].kids != 0u) break;
      on--;
      pc->end = nc;                    // subtree's last preorder cell
      child_flags = pc->flags;
    }
  }
done:
  return root_out;
}
#endif // THVM_ATPFT_NORM

// Composite-size count on spans: the node count of `root` with the
// subterm at cell `at` replaced by `rin` and every variable resolved
// through `subst` (chains followed) -- the FT sibling of
// vcp_count_side/vcp_count_pair.  Iterative over span segments; an
// unbound variable counts 1.  Returns the accumulated count, saturated
// at `cap` (callers only compare against the KPBehandeln gate).  The
// CP-walk port's size classifier; no caller yet.
#if defined(THVM_ATPFT_NORM)
enum { VCP_FT_STK = 96 };
static u32 vcp_ft_count_side(const AtpFtCell *root, const AtpFtCell *at,
                             const AtpFtCell *rin, const AtpFtSubst *subst,
                             u32 cap, u32 acc) {
  struct { const AtpFtCell *c, *e; } stk[VCP_FT_STK];
  u32 sp = 0u;
  const AtpFtCell *c = root;
  const AtpFtCell *e = (root->end != NULL) ? root->end->next : NULL;
  for (;;) {
    while (c == NULL || c == e) {
      if (sp == 0u) return acc;
      sp--;
      c = stk[sp].c;
      e = stk[sp].e;
    }
    if (c == at) {
      // Splice point: count rin's composite instead of this subtree,
      // then resume after the replaced subtree.
      if (sp >= (u32)VCP_FT_STK) return VCP_COUNT_ANOMALY;
      stk[sp].c = c->end->next;
      stk[sp].e = e;
      sp++;
      at = NULL;                                  // single occurrence
      c = rin;
      e = (rin->end != NULL) ? rin->end->next : NULL;
      continue;
    }
    if ((c->sym & WF_VAR_BIT) != 0u) {
      u32 id = c->sym & ~WF_VAR_BIT;
      const AtpFtCell *b =
          (subst != NULL && id < ATPFT_MAX_VARS) ? subst->bind[id] : NULL;
      if (b != NULL) {
        // Resolve through the binding (its own vars resolve on the way).
        if (sp >= (u32)VCP_FT_STK) return VCP_COUNT_ANOMALY;
        stk[sp].c = c->next;
        stk[sp].e = e;
        sp++;
        c = b;
        e = (b->end != NULL) ? b->end->next : NULL;
        continue;
      }
      acc++;                                      // unbound var: one node
      if (acc >= cap) return acc;
      c = c->next;
      continue;
    }
    if (ft_ctr_sym(c) == ATPFT_NUM_MARKER) {
      return VCP_COUNT_ANOMALY;   // TAG_NUM counts 1 Term-side: fall back
    }
    acc++;                                        // constructor cell
    if (acc >= cap) return acc;
    c = c->next;
  }
}

// Pair wrapper -- the FT sibling of vcp_count_pair (same early-outs,
// same anomaly propagation).
static u32 vcp_ft_count_pair(const AtpFtCell *lo_ft, const AtpFtCell *sub_ft,
                             const AtpFtCell *rin_ft, const AtpFtCell *ro_ft,
                             const AtpFtSubst *subst, u32 cap) {
  u32 acc = vcp_ft_count_side(lo_ft, sub_ft, rin_ft, subst, cap, 0u);
  if (acc == VCP_COUNT_ANOMALY || acc >= cap) return acc;
  return vcp_ft_count_side(ro_ft, NULL, NULL, subst, cap, acc);
}
#endif // THVM_ATPFT_NORM

// FT sibling of vcp_emit_fused: arm g_vcp_emit / g_acp_fuse from span
// composites (same side order, same signs).  0 on an anomaly -- caller
// falls back to the Term route.
#if defined(THVM_ATPFT_NORM)
static u8 vcp_ft_emit_fused(const KboConfig *cfg, const AtpFtCell *lo_ft,
                            const AtpFtCell *sub_ft,
                            const AtpFtCell *rin_ft,
                            const AtpFtCell *ro_ft,
                            const AtpFtSubst *subst) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
  if (!vcp_ft_emit_side(ro_ft, NULL, NULL, subst, cfg, &fz->map,
                        &pos, &nodes, fz, +1, &fpos)) {
    return 0u;
  }
  u32 lhs_nodes = nodes, fl = fpos;
  if (!vcp_ft_emit_side(lo_ft, sub_ft, rin_ft, subst, cfg, &fz->map,
                        &pos, &nodes, fz, -1, &fpos)) {
    return 0u;
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  g_vcp_emit.armed     = 0u;
  g_vcp_emit.len       = pos;
  g_vcp_emit.nodes     = nodes;
  g_vcp_emit.lhs_nodes = lhs_nodes;
  return 1u;
}

// FT sibling of vcp_treat_form: the treated raw pair's bytes (into
// g_acp_scratch[0, raw_len), fz == NULL walk) and cells, one shared
// appearance-order map across both sides -- identical map content to
// the Term fused walk (the byte walk of a side appends; the cell walk
// of the same composite only looks up).  Arms the g_vcp_treat stash on
// success; 0 on an anomaly with any built span returned.
static u8 vcp_ft_treat_form(AtpState *s, const AtpFtCell *lo_ft,
                            const AtpFtCell *sub_ft,
                            const AtpFtCell *rin_ft,
                            const AtpFtCell *ro_ft,
                            const AtpFtSubst *subst) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AcpNvMap m;
  m.n = 0u;
  // WM side order: sigma(ro) is the pair's LHS (see the classic arm).
  // The raw-pair BYTE emit is deferred to the trace store (see
  // vcp_ft_treat_emit_raw): ~a third of treated CPs join at push and
  // never consume the bytes.  The builder shares the same appearance-
  // order var map the emit would use, so the deferred bytes are
  // byte-identical to the formation-time emit they replace.
  AtpFtCell *fl = vcp_ft_build_cell(a, ro_ft, NULL, NULL, subst, &m);
  if (fl == NULL) return 0u;
  const AtpFtCell *at2 = sub_ft;
  AtpFtCell *fr = vcp_ft_build_cell(a, lo_ft, &at2, rin_ft, subst, &m);
  if (fr == NULL) {
    ft_free_span(a, fl, fl->end);
    return 0u;
  }
  g_vcp_treat.armed   = 0u;
  g_vcp_treat.fl      = fl;
  g_vcp_treat.fr      = fr;
  g_vcp_treat.raw_len = 0u;
  g_vcp_treat.lo      = lo_ft;
  g_vcp_treat.sub     = sub_ft;
  g_vcp_treat.rin     = rin_ft;
  g_vcp_treat.ro      = ro_ft;
  g_vcp_treat.subst   = subst;
  g_vcp_treat.peak    = NULL;
  return 1u;
}

// The deferred raw-pair emit: exactly the two vcp_ft_emit_side calls
// treat_form ran at formation, from the stashed operands + the still-
// armed walk subst.  Returns the byte length at g_acp_scratch[0,len)
// or 0 on an emit anomaly (the caller stores the no-pair sentinel;
// never observed -- the count walk screens the same cliffs).
static u32 vcp_ft_treat_emit_raw(AtpState *s) {
  AcpNvMap m;
  m.n = 0u;
  u32 raw_len = 0u, nodes = 0u, fpos = 0u;
  if (!vcp_ft_emit_side(g_vcp_treat.ro, NULL, NULL, g_vcp_treat.subst,
                        s->kbo, &m, &raw_len, &nodes, NULL, +1, &fpos)) {
    return 0u;
  }
  if (!vcp_ft_emit_side(g_vcp_treat.lo, g_vcp_treat.sub, g_vcp_treat.rin,
                        g_vcp_treat.subst, s->kbo, &m, &raw_len, &nodes,
                        NULL, -1, &fpos)) {
    return 0u;
  }
  return raw_len;
}
#endif // THVM_ATPFT_NORM

// Route gate for the FT-direct emission: ALWAYS ON where the route
// conditions admit it (the THVM_ATP_FT_EMIT=0 one-round kill switch
// was retired 2026-07-04 after the bake round -- corpus certifier
// sweeps clean and every pin byte-identical either way).  The classic
// Term-build arm in sw_form_push is NOT deletable: it still serves
// every non-routed configuration (MNF, kapur, on-heap trace, fuse-off,
// batch former, anomaly fallbacks) AND it is the per-CP rebuild
// reference for the THVM_ATP_FT_EMIT_CHECK certifier -- deleting it
// would orphan the certification.  The size-hint kill/certifier envs
// force the historic Term-walk size gate, which needs the Terms --
// route off under either.
static int atp_ft_emit_on(void) {
  static int v = -1;
  if (v < 0) {
    v = !atp_env_on("THVM_NO_CP_SZHINT") &&
        !atp_env_on("THVM_ATP_SZHINT_CHECK");
  }
  return v;
}
static int atp_ft_emit_check_on(void) {
  static int v = -1;
  if (v < 0) v = atp_env_on("THVM_ATP_FT_EMIT_CHECK");
  return v;
}

// THVM_ATP_FT_EMIT_CHECK=1: per-CP differential certifier for the
// FT-direct emission (the FUSE_CHECK discipline).  Rebuilds the CP
// classically -- cp_replace_at + thvm_unify_apply x2, the exact Term
// route -- and asserts the virtual emit produced byte-identical
// packed bytes, identical node counts (== the size-gate verdict),
// an identical fused-KBO state (renumber map / balances / flat
// nodes) and the identical weigh verdict.  Aborts on any mismatch.
// Must run while the unifier is still armed.
static void vcp_emit_certify(const KboConfig *kbo, Term lo, const u32 *p,
                             u32 p_len, Term rin, Term ro,
                             const RewriteSubst *subst) {
  // Snapshot the virtual results (bytes + fuse state + verdict).
  u32 vlen = g_vcp_emit.len, vnodes = g_vcp_emit.nodes;
  u32 vlhs = g_vcp_emit.lhs_nodes;
  u8 *vbytes = (u8 *)malloc(vlen ? vlen : 1u);
  if (vbytes == NULL) thvm_fatal("vcp_emit_certify: OOM");
  memcpy(vbytes, g_acp_scratch, vlen);
  AcpFuse vfz = g_acp_fuse;   // struct copy: map, phidiff, bal, counts
  u32 vfn = vfz.fl_n + vfz.fr_n;
  AcpFlatNode *vflat = (AcpFlatNode *)malloc((size_t)vfn * sizeof *vflat);
  if (vflat == NULL) thvm_fatal("vcp_emit_certify: OOM");
  memcpy(vflat, g_acpf_pool, (size_t)vfn * sizeof *vflat);
  KboCmp vk = acp_fuse_kbo(kbo);
  // Classic rebuild on the same armed subst (WM order: sigma(ro) left).
  u64 c0 = g_unify_apply_cells, t0 = g_unify_apply_taint;
  Term cl = thvm_unify_apply(ro, subst);
  Term cr = thvm_unify_apply(cp_replace_at(lo, p, p_len, rin), subst);
  u32 ctot = (g_unify_apply_taint == t0)
                 ? (u32)(g_unify_apply_cells - c0) : 0u;
  u32 rlen = 0u, rn = 0u, rl = 0u;
  u8 *ref = acp_pack_fused(kbo, cl, cr, &rlen, &rn, &rl);
  u8 bad = 0u;
  if (ref == NULL || rlen != vlen || memcmp(ref, vbytes, vlen) != 0) bad = 1u;
  if (rn != vnodes || rl != vlhs) bad = 1u;
  // Size-gate verdict: the FT route fired on vcount >= gate, so the
  // untainted classic tally must be at/over the gate too.
  if (ctot != 0u && ctot < (u32)ATP_WM_BEHANDELN_GATE) bad = 1u;
  if (!bad) {
    const AcpFuse *cf = &g_acp_fuse;
    if (cf->map.n != vfz.map.n || cf->phidiff != vfz.phidiff ||
        cf->fl_n != vfz.fl_n || cf->fr_n != vfz.fr_n) {
      bad = 1u;
    }
    for (u32 k = 0; !bad && k < vfz.map.n; k++) {
      if (cf->map.old_id[k] != vfz.map.old_id[k] ||
          cf->bal[k] != vfz.bal[k]) bad = 1u;
    }
    if (!bad &&
        memcmp(g_acpf_pool, vflat, (size_t)vfn * sizeof *vflat) != 0) {
      bad = 1u;
    }
    if (!bad && acp_fuse_kbo(kbo) != vk) bad = 1u;
  }
  if (bad) {
    fprintf(stderr,
            "ATP_FT_EMIT_CHECK MISMATCH len=%u/%u nodes=%u/%u lhs=%u/%u "
            "ctot=%u kbo=%d\n",
            vlen, rlen, vnodes, rn, vlhs, rl, ctot, (int)vk);
    abort();
  }
  free(ref);
  free(vbytes);
  free(vflat);
  // g_acp_fuse + the scratch now hold the classic (identical) state;
  // the FT route proceeds on them.
}

// Count nodes in one packed preorder term, advancing `*pp` past it.
// Non-allocating (unlike acp_unpack_term) so it is safe to call from the
// hot CP-selection comparator without touching the term heap.
static u32 acp_term_nodes_skip(const u8 **pp) {
  // Preorder records are self-delimiting: count records until every
  // CTR's (clamped) children have been read -- no stack needed.
  u32 tot     = 0u;
  u32 pending = 1u;
  while (pending > 0u) {
    pending--;
    tot++;
    u8 disc = *(*pp)++;
    switch (disc) {
      case 'C': {
        (void)acp_get_varint(pp);           // label
        u32 n = (u32)acp_get_varint(pp);    // arity
        if (n > REWRITE_MAX_ARITY) {
          n = REWRITE_MAX_ARITY;
        }
        pending += n;
        break;
      }
      case 'N':
        (void)acp_get_varint(pp);           // dtype
        (void)acp_get_varint(pp);           // val
        break;
      case 'V':
      default:
        (void)acp_get_varint(pp);           // ext
        break;
    }
  }
  return tot;
}

// Total node count of a packed CP's rhs (skip lhs, size rhs).  Used by the
// weight-tie tiebreak to distinguish compound CPs (large rhs) from
// collapsers (bare-variable rhs) at selection time.
static u32 acp_rhs_nodes(const u8 *buf) {
  const u8 *p = buf;
  (void)acp_term_nodes_skip(&p);          // skip lhs
  return acp_term_nodes_skip(&p);         // size rhs
}

// One-way match of a PACKED pattern term against a heap Term subject --
// the Stringterms counterpart of thvm_match (rewrite/_.c:27), and the
// Waldmeister technique of matching on the packed representation
// directly.  The pattern is never unpacked to a heap tree: the matcher
// walks the preorder byte string and the subject term in lockstep, so
// a head-symbol mismatch fast-fails after one discriminator byte with
// zero allocation.  This is what keeps the subsumption index off the
// per-candidate acp_unpack that an unpack-then-thvm_match would pay.
//
// `*pp` advances past the pattern term on a full match; on a mismatch
// it is left mid-record (the caller aborts the whole match, so the
// stale cursor is never used).  Verdict is bit-identical to
// thvm_match: a NUM / ERA pattern matches nothing (thvm_match's
// default branch returns 0); a variable id past the REWRITE_MAX_VAR
// matcher cliff fails; a repeated variable is confirmed with kbo_eq.
static u8 acp_match_term(const u8 **pp, Term subj, RewriteSubst *sub) {
  // Open CTR records on an explicit stack: child i of the subject is
  // heap_read(base + 1 + i); the pattern's children follow in the byte
  // stream, so a failure stops the cursor where the recursion did.
  typedef struct { u64 base; u32 i; u32 n; } PkFrame;
  PkFrame  fr_inl[ACP_WALK_INL];
  PkFrame *fr  = fr_inl;
  u32      nfr = 0u;
  u32      cfr = ACP_WALK_INL;
  u8       ok  = 1u;
  for (;;) {
    u8 disc = *(*pp)++;
    switch (disc) {
      case 'C': {
        u32 label = (u32)acp_get_varint(pp);
        u32 n     = (u32)acp_get_varint(pp);
        if (term_tag(subj) != TAG_CTR || term_ext(subj) != label ||
            term_ctr_n(subj) != n) {
          ok = 0u;
          break;
        }
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (PkFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].base = term_val(subj);
          fr[nfr].i    = 0u;
          fr[nfr].n    = n;
          nfr++;
        }
        break;
      }
      case 'V': {
        u32 id = (u32)acp_get_varint(pp);
        if (id >= REWRITE_MAX_VAR) {
          ok = 0u;
        } else if (sub->bindings[id] == 0) {
          sub->bindings[id] = subj;
        } else if (!kbo_eq(sub->bindings[id], subj)) {
          ok = 0u;
        }
        break;
      }
      default:         // 'N' (NUM) / 'E' -- thvm_match's default: no match
        ok = 0u;
        break;
    }
    if (!ok) {
      break;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    PkFrame *f = &fr[nfr - 1u];
    subj = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  term_stack_release(fr, fr_inl);
  return ok;
}

// Two-sided one-way match: does the packed CP `pack(plhs)++pack(prhs)`
// match (qlhs, qrhs) under a single shared substitution?  Equivalent
// to `thvm_match(plhs,qlhs,&s) && thvm_match(prhs,qrhs,&s)` with the
// pattern kept packed.  The lhs match advances the cursor exactly past
// `pack(plhs)` (preorder is self-delimiting), so the rhs match resumes
// at `pack(prhs)`.
static u8 acp_match_pair(const u8 *packed, Term qlhs, Term qrhs,
                         RewriteSubst *sub) {
  const u8 *p = packed;
  if (!acp_match_term(&p, qlhs, sub)) return 0;
  return acp_match_term(&p, qrhs, sub);
}

// One-time port self-check, run at engine init: a hand-built pair --
// nested CTRs, repeated variable, a NUM -- must survive a pack/unpack
// round-trip structurally intact, so a broken Stringterms port fails
// loudly and immediately rather than corrupting the CP queue later.
static void acp_selftest(void) {
  static u8 done = 0;
  if (done) return;
  done = 1;
  Term v0 = term_new_fvr(0u), v1 = term_new_fvr(1u);
  Term inner[2]  = { v0, v1 };
  Term f         = term_new_ctr(7u, inner, 2u);          // f(v0, v1)
  Term spine[2]  = { f, v0 };                            // var v0 repeats
  Term lhs       = term_new_ctr(7u, spine, 2u);          // f(f(v0,v1), v0)
  Term rhs       = term_new(0, TAG_NUM, 0u, 42u);        // NUM 42
  u32  len = 0;
  u8  *packed = acp_pack(lhs, rhs, &len, NULL);
  Term ul = 0, ur = 0;
  acp_unpack(packed, &ul, &ur);
  free(packed);
  if (!kbo_eq(lhs, ul) || !kbo_eq(rhs, ur)) {
    fprintf(stderr, "acp_selftest: Stringterms round-trip FAILED\n");
    exit(1);
  }
}

// Pack (lhs, rhs) into CP queue slot i, freeing any byte string
// already there.  Callers that build the queue directly -- chiefly
// tests -- use this instead of writing the (now packed) queue slots
// by hand.  The insertion age (cp_seq) is stamped here; the slot's
// priority is filled by a subsequent thvm_atp_cp_reheapify.
fn void thvm_atp_cp_set(AtpState *s, u32 i, Term lhs, Term rhs) {
  if (s == NULL) return;
  atp_ensure_cp_cap(s, i + 1u);
  // Position-indirection mapping: overwriting a LIVE queue slot means
  // "replace the CP at heap position i" (the historical position ==
  // payload-index semantics); a fresh build slot beyond n_cps stamps
  // the identity mapping -- callers populate [0, k) then set n_cps and
  // reheapify, and identity reproduces the historical fresh layout.
  u32 row;
  if (i < s->n_cps) {
    row = s->cp_heap[i].row;
  } else {
    // Fresh build slot: identity mapping.  cp_pri[row] is not defined
    // yet (the contract fills it via thvm_atp_cp_reheapify, whose
    // rebuild restamps the packed caches per position before any
    // compare), so stamp a zero pri cache rather than read garbage.
    s->cp_heap[i].pri = 0u;
    s->cp_heap[i].seq = 0u;
    s->cp_heap[i].row = i;
    s->cp_heap[i].aux = 0u;
    s->cp_pos[i]  = i;
    row = i;
  }
  free(s->cp_packed[row]);               // free(NULL) is a no-op
  // The slot now holds packed bytes -- drop any deferred-CP tag so the
  // `cp_is_implicit[i] <=> cp_packed[i] == NULL` invariant holds, and
  // retire any raw-desc descriptor (the bytes are being replaced) plus
  // any peak / raw-overlap stash (external content, no overlap backing).
  atp_cp_stash_clear(s, row);
  atp_cp_implicit_clear(s, row);
  if (s->cp_implicit != NULL) {
    s->cp_implicit[row].out_face = ATP_RDESC_NONE;
  }
  s->cp_packed[row] = acp_pack(lhs, rhs, NULL, NULL);
  // Insertion age, stamped once here (mirroring the heap-push path) and
  // preserved across every later sweep / reheapify -- WM keeps the w2
  // FIFO key untouched on reweight (C_ReClassify, CLAS/
  // NewClassification.c:399-406 "w2 wird nicht geaendert").
  s->cp_seq[row] = s->cp_seq_next++;
  // Refresh the packed seq cache at position i (cp_seq[row] just
  // changed for a possibly-QUEUED row).  The pri cache is left to the
  // contract-mandated reheapify, exactly like cp_pri[row] itself.
  s->cp_heap[i].seq = s->cp_seq[row];
#ifdef THVM_ATPFT_CPQ
  // Stage 7: mirror the slot into the parallel FT queue.  Test
  // harnesses populate the queue via thvm_atp_cp_set + reheapify;
  // keep the two views in lockstep (entries live at the same
  // stationary rows as the legacy arrays).
  atp_cp_ft_clear(s, row);
  atp_cp_ft_set(s, row, lhs, rhs, /*priority_hint=*/0u, /*origin=*/0xffffu);
#endif
}

// Read CP queue slot i back into two Terms -- the read counterpart of
// thvm_atp_cp_set.  A packed slot unpacks fresh transient heap Terms;
// a deferred (implicit) slot returns its trace-resident raw pair
// zero-copy.  Read-only either way.  `i` is a heap POSITION (the
// public queue-slot notion); the payload row is mapped internally.
static void atp_cp_slot_read(const AtpState *s, u32 i,
                             Term *lhs, Term *rhs);
fn void thvm_atp_cp_get(const AtpState *s, u32 i, Term *lhs, Term *rhs) {
  atp_cp_slot_read(s, s->cp_heap[i].row, lhs, rhs);
}


