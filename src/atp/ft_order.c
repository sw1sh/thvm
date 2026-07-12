// ft_order.c - AtpFt-native LPO/KBO entry points.
//
// Stage 3 wires the existing flatterm LPO/KBO comparators (src/kbo/_.c
// and src/lpo/_.c -- both operate on KboFlatNode arrays internally) to
// AtpFtCell inputs.
//
// Two layouts diverge non-trivially:
//   * KboFlatNode is 12 B {i32 sym, i32 w, u32 sz}, packed into a
//     contiguous pre-order array indexed by integer position.  Variables
//     use sym = -(varid + 1); each node carries its own pre-baked
//     KBO/LPO weight.
//   * AtpFtCell is 24 B {next, end, sym, arity, flags, _pad}, a linked
//     list stepped via cell->end->next for sibling jumps.  Variables
//     use sym = WF_VAR_BIT | varid (high bit) and store no per-cell
//     weight.
//
// Direct casting therefore does not work (Strategy A in the plan).
// Stage 3 uses Strategy B: ~30-line pre-order encoders that walk the
// AtpFtCell tree once, emitting a KboFlatNode array with the KBO/LPO
// conventions; then delegate to thvm_kbo_flat_slice (KBO) and a small
// dispatcher mirroring thvm_lpo lines 691-748 (LPO -- the slice path
// is file-static in src/lpo/_.c, so we reproduce the encode +
// lpo_flat_rec_compute wiring locally with the same epoch bump and
// var-set incomparability check).
//
// Gated on THVM_ATPFT_CONVERT (same envelope as ft.c).  Double-include
// guard mirrors ft_alloc.c / ft.c: tests/test_ft_order.c #includes this
// directly, and src/atp/_.c reaches it indirectly through ft.c.

#ifdef THVM_ATPFT_CONVERT
#ifndef ATPFT_FT_ORDER_C_INCLUDED
#define ATPFT_FT_ORDER_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"   // WF_VAR_BIT / WF_IS_VAR / WF_VAR_ID

// The flatterm node + the KBO/LPO state we touch live inside
// src/kbo/_.c and src/lpo/_.c, both of which are #included from
// src/thvm.c ahead of any test TU that pulls in ft_order.c.  The
// declarations we need:
//
//   * struct KboFlatNode (12 B {i32 sym, i32 w, u32 sz})
//   * thvm_kbo_flat_slice(const KboFlatNode *a, u32 na,
//                         const KboFlatNode *b, u32 nb,
//                         const KboConfig *cfg)
//   * lpo_flat_rec_compute / g_lpo_flat_a / g_lpo_flat_b /
//     g_lpo_flat_epoch / g_lpo_flat_memo / LPO_FLAT_MEMO_SIZE
//
// thvm.h forward-declares struct KboFlatNode + thvm_kbo_flat_slice for
// us (src/thvm.h:3041-3044).  The lpo_flat_* symbols are file-static
// in src/lpo/_.c -- we mirror the same dispatch shape here.

// === local flatten buffers ===========================================
//
// Sized to KBO_FLAT_CAP (the same 4096-node cap the KBO/LPO flat
// encoders use).  We keep separate buffers from g_kbo_flat_a/b and
// g_lpo_flat_a/b so a Term-tree thvm_kbo / thvm_lpo call interleaved
// with an AtpFt-native one does not stomp the other's encode.  The
// KBO path delegates to thvm_kbo_flat_slice, which then reuses the
// shared g_kbo_st KBO scratch -- that is fine because the slice path
// owns g_kbo_st for the duration of the call (no interleave).
//
// For the LPO path we need to encode into g_lpo_flat_a/g_lpo_flat_b
// directly: lpo_flat_rec stamps `side` based on the
// (a == g_lpo_flat_a) pointer-identity check, so the memo lookups
// inside the recursion only stay consistent if our flat arrays ARE
// the same buffers.  The Term-tree thvm_lpo path uses the same
// buffers -- the ATP layer serialises calls.

#define ATPFT_KBO_FLAT_CAP 4096u

static KboFlatNode g_atpft_kbo_flat_a[ATPFT_KBO_FLAT_CAP];
static KboFlatNode g_atpft_kbo_flat_b[ATPFT_KBO_FLAT_CAP];

// Success marker for the LAST (tmpl, subst) encode that
// thvm_kbo_ft_subst_with_prepared left in flat_b (0 = failed or
// absent).  Every other writer of flat_b clears it.
// thvm_kbo_ft_skolem_after_subst keys on it to run the skolem-ground
// compare directly on those bytes instead of re-encoding the identical
// pair (the encode is deterministic, so the historical re-encode
// emitted byte-for-byte the same nodes).
static u32 g_atpft_b_subst_ok = 0u;

// === KBO encode ======================================================
//
// Walk the AtpFt subterm in pre-order, writing one KboFlatNode per
// AtpFtCell.  Children at position p start immediately after p+1 and
// are stepped via their own sz on the producer side: we count cells
// (n_cells in *pos) as we go and back-patch sz at the parent.
//
// Variable encoding maps AtpFt's high-bit (WF_VAR_BIT | id) to KBO's
// negative-sym convention (sym = -(id + 1)).
//
// Returns 1 on success, 0 if the cap was overrun (caller falls back
// or aborts).

static u8 atpft_kbo_flat_encode(const AtpFtCell *c, const KboConfig *cfg,
                                KboFlatNode *out, u32 *pos) {
  u32 here = *pos;
  if (here >= ATPFT_KBO_FLAT_CAP) return 0;
  *pos = here + 1u;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    u32 vid = c->sym & ~WF_VAR_BIT;
    out[here].sym = -(i32)(vid + 1u);
    out[here].w   = (i32)cfg->var_weight;
    out[here].sz  = 1u;
    return 1;
  }
  u32 lab = c->sym;
  out[here].sym = (i32)lab;
  out[here].w   = (lab < cfg->n_labels) ? (i32)cfg->weights[lab] : 0;
  u16 n = c->arity;
  const AtpFtCell *child = c->next;
  for (u16 i = 0; i < n; i++) {
    if (!atpft_kbo_flat_encode(child, cfg, out, pos)) return 0;
    child = child->end->next;
  }
  out[here].sz = *pos - here;
  return 1;
}

// === LPO encode ======================================================
//
// Same shape as the KBO encoder but stores precedence in .w (matches
// lpo_flat_encode_rec in src/lpo/_.c).  Reads from AtpFtCell instead
// of Term.

static u8 atpft_lpo_flat_encode(const AtpFtCell *c, const LpoConfig *cfg,
                                KboFlatNode *out, u32 *pos) {
  u32 here = *pos;
  if (here >= ATPFT_KBO_FLAT_CAP) return 0;
  *pos = here + 1u;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    u32 vid = c->sym & ~WF_VAR_BIT;
    out[here].sym = -(i32)(vid + 1u);
    out[here].w   = 0;
    out[here].sz  = 1u;
    return 1;
  }
  u32 lab = c->sym;
  out[here].sym = (i32)lab;
  out[here].w   = (lab < cfg->n_labels) ? (i32)cfg->precedence[lab] : 0;
  u16 n = c->arity;
  const AtpFtCell *child = c->next;
  for (u16 i = 0; i < n; i++) {
    if (!atpft_lpo_flat_encode(child, cfg, out, pos)) return 0;
    child = child->end->next;
  }
  out[here].sz = *pos - here;
  return 1;
}

// === KBO encode with inline substitution =============================
//
// Same shape as atpft_kbo_flat_encode, but every variable cell in `c`
// is expanded inline through `subst` (looked up via the type-erased
// ft_subst_lookup helper from ft_match.c).  This lets thvm_kbo_ft_subst
// compare a redex against an instantiated rule RHS WITHOUT ever
// materialising the substituted AtpFtCell tree -- the dominant cost
// in find_redex_ft's unorient gate after ft_to_term was retired.
//
// Unbound vars in the template are emitted as plain variables (their
// own id) -- mirrors ft_subst_apply's regime (a) fallthrough; the
// caller's vars_contained guard ensures every template variable IS
// bound in the unorient gate, so this branch only fires defensively.

extern AtpFtCell *ft_subst_lookup(const void *subst, u32 var_id);

static u8 atpft_kbo_flat_encode_subst(const AtpFtCell *c,
                                      const KboConfig *cfg,
                                      const void      *subst,
                                      KboFlatNode     *out,
                                      u32             *pos) {
  if (c == NULL) return 0;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    u32 vid = c->sym & ~WF_VAR_BIT;
    AtpFtCell *bound = ft_subst_lookup(subst, vid);
    if (bound != NULL) {
      // Inline subst(vid): recurse into the bound subterm using the
      // bare encoder (no further substitution -- bound subterms are
      // already concrete, never contain rule-template vars).
      return atpft_kbo_flat_encode(bound, cfg, out, pos);
    }
    // Unbound: emit the variable as itself.
    u32 here = *pos;
    if (here >= ATPFT_KBO_FLAT_CAP) return 0;
    *pos = here + 1u;
    out[here].sym = -(i32)(vid + 1u);
    out[here].w   = (i32)cfg->var_weight;
    out[here].sz  = 1u;
    return 1;
  }
  u32 here = *pos;
  if (here >= ATPFT_KBO_FLAT_CAP) return 0;
  *pos = here + 1u;
  u32 lab = c->sym;
  out[here].sym = (i32)lab;
  out[here].w   = (lab < cfg->n_labels) ? (i32)cfg->weights[lab] : 0;
  u16 n = c->arity;
  const AtpFtCell *child = c->next;
  for (u16 i = 0; i < n; i++) {
    if (!atpft_kbo_flat_encode_subst(child, cfg, subst, out, pos)) return 0;
    child = child->end->next;
  }
  out[here].sz = *pos - here;
  return 1;
}

// === KBO public entry ================================================
//
// Encode both sides into private buffers, then delegate to the
// existing thvm_kbo_flat_slice.  Falls back to LPO_UN... no wait,
// KBO_UN... on encode overflow.  Encode overflow would mean a term
// deeper than ATPFT_KBO_FLAT_CAP=4096 cells; the harvested ATP
// corpora never approach that.
fn KboCmp thvm_kbo_ft(const AtpFtCell *a, const AtpFtCell *b,
                      const KboConfig *cfg) {
  g_atpft_b_subst_ok = 0u;
  u32 na = 0u, nb = 0u;
  if (!atpft_kbo_flat_encode(a, cfg, g_atpft_kbo_flat_a, &na) ||
      !atpft_kbo_flat_encode(b, cfg, g_atpft_kbo_flat_b, &nb)) {
    return KBO_UN;
  }
  return thvm_kbo_flat_slice(g_atpft_kbo_flat_a, na,
                             g_atpft_kbo_flat_b, nb, cfg);
}

// Streaming variant: encode (template, subst) inline into the right-
// hand buffer without materialising the AtpFtCell tree for subst(tmpl).
// Used by find_redex_ft's unorient gate to skip an entire ft_subst_apply
// + ft_deep_copy_rec round per attempted rewrite.
fn KboCmp thvm_kbo_ft_subst(const AtpFtCell *redex,
                            const AtpFtCell *tmpl,
                            const void      *subst,
                            const KboConfig *cfg) {
  g_atpft_b_subst_ok = 0u;
  u32 na = 0u, nb = 0u;
  if (!atpft_kbo_flat_encode(redex, cfg, g_atpft_kbo_flat_a, &na) ||
      !atpft_kbo_flat_encode_subst(tmpl, cfg, subst,
                                   g_atpft_kbo_flat_b, &nb)) {
    return KBO_UN;
  }
  return thvm_kbo_flat_slice(g_atpft_kbo_flat_a, na,
                             g_atpft_kbo_flat_b, nb, cfg);
}

// Two-stage entry: pre-encode the redex ONCE (caller threads the
// resulting cell-count through across many rule attempts), then
// reuse it on each subsequent kbo_ft_subst.  Callers that hit the
// unorient gate against many rule candidates at the same cell
// (find_redex_ft's r-loop) save N-1 redex re-encodings out of N.

// Pre-encode redex into the kbo_flat_a buffer.  Returns the cell
// count on success, or 0 on encode overflow (caller falls through
// to the un-cached path or bails).
fn u32 thvm_kbo_ft_subst_prepare_redex(const AtpFtCell *redex,
                                       const KboConfig *cfg) {
  u32 na = 0u;
  if (!atpft_kbo_flat_encode(redex, cfg, g_atpft_kbo_flat_a, &na)) return 0u;
  return na;
}

// Run the compare against a redex that's already in g_atpft_kbo_flat_a
// with `na` cells (from thvm_kbo_ft_subst_prepare_redex above).  No
// state is held across calls beyond what the caller threads.
fn KboCmp thvm_kbo_ft_subst_with_prepared(u32 na,
                                          const AtpFtCell *tmpl,
                                          const void      *subst,
                                          const KboConfig *cfg) {
  g_atpft_b_subst_ok = 0u;
  if (na == 0u) return KBO_UN;
  u32 nb = 0u;
  if (!atpft_kbo_flat_encode_subst(tmpl, cfg, subst,
                                   g_atpft_kbo_flat_b, &nb)) {
    return KBO_UN;
  }
  g_atpft_b_subst_ok = nb;
  return thvm_kbo_flat_slice(g_atpft_kbo_flat_a, na,
                             g_atpft_kbo_flat_b, nb, cfg);
}

// === Skolem-ground KBO (encompassment demodulation) =================
//
// Ground KBO over the skolemised pair: every variable reads as a fresh
// CONSTANT (weight = cfg->var_weight, precedence below every function
// symbol, ordered among themselves by id).  Twee's lessEqSkolem
// (Constraints.hs:309) -- the comparison behind reducesSkolem.  On the
// skolemised (ground) pair the order is TOTAL: weight sum, then head
// (var < function symbol; vars by id; function symbols by precedence
// with a label tie-break), then lexicographic children.  Closed under
// contexts and well-founded (plain ground KBO with a refined total
// precedence), so a rewrite pass gated on "instance strictly greater"
// terminates even for steps the open-term KBO calls incomparable
// (variable-permuted instances of commutativity et al).
static KboCmp atpft_kbo_skolem_cmp(const KboFlatNode *a, u32 pa,
                                   const KboFlatNode *b, u32 pb,
                                   const KboConfig *cfg) {
  u32 ea = pa + a[pa].sz, eb = pb + b[pb].sz;
  long long wa = 0, wb = 0;
  for (u32 i = pa; i < ea; i++) wa += a[i].w;
  for (u32 i = pb; i < eb; i++) wb += b[i].w;
  if (wa != wb) return (wa > wb) ? KBO_GT : KBO_LT;
  i32 sa = a[pa].sym, sb = b[pb].sym;
  u8 va = (sa < 0), vb = (sb < 0);
  if (va != vb) return va ? KBO_LT : KBO_GT;   // skolem const < any fn sym
  if (va) {
    if (sa == sb) return KBO_EQ;               // same variable
    return (sa < sb) ? KBO_GT : KBO_LT;        // by id (total, arbitrary)
  }
  u32 la = (u32)sa, lb = (u32)sb;
  if (la != lb) {
    u32 ps = (la < cfg->n_labels) ? cfg->precedence[la] : 0;
    u32 pt = (lb < cfg->n_labels) ? cfg->precedence[lb] : 0;
    if (ps != pt) return (ps > pt) ? KBO_GT : KBO_LT;
    return (la > lb) ? KBO_GT : KBO_LT;        // label tie-break (total)
  }
  u32 ca = pa + 1u, cb = pb + 1u;
  while (ca < ea && cb < eb) {
    KboCmp c = atpft_kbo_skolem_cmp(a, ca, b, cb, cfg);
    if (c != KBO_EQ) return c;
    ca += a[ca].sz;
    cb += b[cb].sz;
  }
  return KBO_EQ;
}

// Skolem-ground compare against the (tmpl, subst) encode the
// immediately preceding thvm_kbo_ft_subst_with_prepared call left in
// flat_b.  The historical entry re-encoded the SAME pair into the SAME
// buffer -- deterministic, so byte-for-byte what already sits there --
// on every encompassment probe; this one just runs the compare.
// Caller contract: invoked with the same `na` directly after
// thvm_kbo_ft_subst_with_prepared, with no flat-buffer call between
// (ft_cell_try_rules' unorient gate -- the only caller -- satisfies
// this by adjacency).  KBO_UN when the preceding encode failed or was
// skipped: the re-encode would have overflowed identically.
fn KboCmp thvm_kbo_ft_skolem_after_subst(u32 na, const KboConfig *cfg) {
  if (na == 0u || g_atpft_b_subst_ok == 0u) return KBO_UN;
  return atpft_kbo_skolem_cmp(g_atpft_kbo_flat_a, 0u,
                              g_atpft_kbo_flat_b, 0u, cfg);
}

// === Simpler-join equation compare (Twee Equation.hs simplerThan) ====
//
// The CP-join discipline (AtpState.use_simpler_join) gates every
// rewrite step on the rule's equation being strictly SIMPLER than the
// CP equation under a ground-total order on equations: order each
// equation by its skolem-ground-larger side, then compare
// lexicographically (larger side first, smaller side on a tie) with
// atpft_kbo_skolem_cmp.  This is Twee's orderedSimplerThan
// (Equation.hs:46-48) over lessEqSkolem, minus the variable
// canonicalisation inside Twee's `order` (canonicalise renames
// variables before comparing; here variants tie-break on raw var ids
// -- still a strict total order on concrete terms, which is what the
// well-foundedness of the join induction needs).
//
// Dedicated buffers: the join pass interleaves these compares with
// find_redex_ft's unorient gate, whose prepared redex lives in
// g_atpft_kbo_flat_a -- sharing buffers would clobber it mid-scan.

static KboFlatNode g_ftsj_cp_hi[ATPFT_KBO_FLAT_CAP];   // CP eq, larger side
static KboFlatNode g_ftsj_cp_lo[ATPFT_KBO_FLAT_CAP];   // CP eq, smaller side
static u32 g_ftsj_cp_hi_n = 0u;
// CP-side total hi weight, latched by thvm_ftsj_prepare_eq (consumed
// by the rule-encode cache's weight fast key below).
static long long g_ftsj_cp_whi = 0;
static KboFlatNode g_ftsj_scr_a[ATPFT_KBO_FLAT_CAP];   // rule-side scratch
static KboFlatNode g_ftsj_scr_b[ATPFT_KBO_FLAT_CAP];

// Encode the CP equation (l, r) into the prepared hi/lo buffers.
// Returns 0 on encode overflow (caller skips the join pass).
fn int thvm_ftsj_prepare_eq(const AtpFtCell *l, const AtpFtCell *r,
                            const KboConfig *cfg) {
  u32 nl = 0u, nr = 0u;
  g_ftsj_cp_hi_n = 0u;
  if (!atpft_kbo_flat_encode(l, cfg, g_ftsj_scr_a, &nl) ||
      !atpft_kbo_flat_encode(r, cfg, g_ftsj_scr_b, &nr)) {
    return 0;
  }
  KboCmp c = atpft_kbo_skolem_cmp(g_ftsj_scr_a, 0u, g_ftsj_scr_b, 0u, cfg);
  const KboFlatNode *hi = (c == KBO_LT) ? g_ftsj_scr_b : g_ftsj_scr_a;
  const KboFlatNode *lo = (c == KBO_LT) ? g_ftsj_scr_a : g_ftsj_scr_b;
  u32 nhi = (c == KBO_LT) ? nr : nl;
  u32 nlo = (c == KBO_LT) ? nl : nr;
  memcpy(g_ftsj_cp_hi, hi, (size_t)nhi * sizeof(KboFlatNode));
  memcpy(g_ftsj_cp_lo, lo, (size_t)nlo * sizeof(KboFlatNode));
  g_ftsj_cp_hi_n = nhi;
  // Latch hi's total skolem weight for the rule-cache weight fast key
  // (see thvm_ftsj_rule_simpler_cached).
  {
    long long w = 0;
    for (u32 i = 0; i < nhi; i++) w += g_ftsj_cp_hi[i].w;
    g_ftsj_cp_whi = w;
  }
  return 1;
}

// Is the rule/equation (rl, rr) strictly simpler than the prepared CP
// equation?  `oriented` rules have rl skolem-greater by construction
// (open-term KBO GT is stable under the skolem grounding);
// unorientable equations pick the larger side via the total skolem
// compare.  Returns 0 when no CP equation is prepared or on encode
// overflow (fail-closed: the step does not fire).
fn int thvm_ftsj_rule_simpler(const AtpFtCell *rl, const AtpFtCell *rr,
                              u8 oriented, const KboConfig *cfg) {
  if (g_ftsj_cp_hi_n == 0u) return 0;
  u32 na = 0u, nb = 0u;
  if (!atpft_kbo_flat_encode(rl, cfg, g_ftsj_scr_a, &na) ||
      !atpft_kbo_flat_encode(rr, cfg, g_ftsj_scr_b, &nb)) {
    return 0;
  }
  const KboFlatNode *hi = g_ftsj_scr_a;
  const KboFlatNode *lo = g_ftsj_scr_b;
  if (!oriented &&
      atpft_kbo_skolem_cmp(g_ftsj_scr_a, 0u, g_ftsj_scr_b, 0u, cfg)
          == KBO_LT) {
    hi = g_ftsj_scr_b;
    lo = g_ftsj_scr_a;
  }
  // Twee orderedSimplerThan: (hi < HI) or (hi == HI and lo < LO).
  KboCmp ch = atpft_kbo_skolem_cmp(hi, 0u, g_ftsj_cp_hi, 0u, cfg);
  if (ch == KBO_LT) return 1;
  if (ch != KBO_EQ) return 0;
  return atpft_kbo_skolem_cmp(lo, 0u, g_ftsj_cp_lo, 0u, cfg) == KBO_LT;
}

// --- Per-rule-slot encode cache for the simpler-join gate -----------
//
// thvm_ftsj_rule_simpler re-encoded BOTH rule sides and re-ran the
// hi/lo orientation pick on EVERY (rule, CP) gate query -- 13.2M
// re-encodes per 12k steps on RNG021-6 under --simpler-join, all
// recomputing a value that depends only on the rule's content.  Cache
// per rule slot: the oriented (hi, lo) flat encodes plus hi's total
// skolem weight (atpft_kbo_skolem_cmp's first key over the full span).
// The cached bytes are exactly what the uncached path would encode, so
// every verdict is byte-identical.
//
// Invalidation contract (drivers live in ft_norm.c / _.c):
//   * thvm_ftsj_rulecache_sync(owner, r_revision, n_rules) runs once
//     per join pass: same owner + unchanged (rev, n) keeps everything;
//     a pure-append witness (rev delta == count delta, n grew) keeps
//     existing slots (their content is untouched by pushes); any other
//     movement -- drop, orient-flip, bwd-subsume, slot backfill --
//     bumps rev without the append witness and drops the whole cache.
//   * thvm_ftsj_rulecache_note_edit(slot) covers the ONE mutation
//     r_revision does not: interreduction's in-place RHS compose
//     (the two atp_wf_mm_note_rhs_edit sites in _.c).
//   * thvm_ftsj_rulecache_invalidate_all() runs from thvm_atp_init
//     (new engine, possibly new ordering config).
typedef struct {
  KboFlatNode *hi;     // oriented larger side (post hi/lo pick)
  KboFlatNode *lo;
  u32 cap_hi, cap_lo;  // allocated node capacity
  u32 nhi, nlo;
  long long whi;       // total skolem weight of hi (sum of node .w)
  u8 valid;            // 1 = hi/lo/whi live
  u8 overflow;         // 1 = encode overflowed (gate always answers 0)
} FtsjRuleEnc;
static FtsjRuleEnc *g_ftsj_renc       = NULL;
static u32          g_ftsj_renc_cap   = 0u;
static const void  *g_ftsj_renc_owner = NULL;
static u32          g_ftsj_renc_rev   = 0u;
static u32          g_ftsj_renc_n     = 0u;

static void ftsj_renc_drop_all(void) {
  for (u32 i = 0; i < g_ftsj_renc_cap; i++) {
    g_ftsj_renc[i].valid = 0u;
    g_ftsj_renc[i].overflow = 0u;
  }
}

fn void thvm_ftsj_rulecache_invalidate_all(void) {
  ftsj_renc_drop_all();
  g_ftsj_renc_owner = NULL;
}

fn void thvm_ftsj_rulecache_note_edit(u32 slot) {
  if (slot < g_ftsj_renc_cap) {
    g_ftsj_renc[slot].valid = 0u;
    g_ftsj_renc[slot].overflow = 0u;
  }
}

fn void thvm_ftsj_rulecache_sync(const void *owner, u32 r_revision,
                                 u32 n_rules) {
  if (owner == g_ftsj_renc_owner && r_revision == g_ftsj_renc_rev &&
      n_rules == g_ftsj_renc_n) {
    return;
  }
  // Pure-append witness: only pushes since the last sync -- existing
  // slots' content is unchanged, new slots are invalid by construction.
  if (owner == g_ftsj_renc_owner && n_rules >= g_ftsj_renc_n &&
      r_revision - g_ftsj_renc_rev == n_rules - g_ftsj_renc_n) {
    g_ftsj_renc_rev = r_revision;
    g_ftsj_renc_n   = n_rules;
    return;
  }
  ftsj_renc_drop_all();
  g_ftsj_renc_owner = owner;
  g_ftsj_renc_rev   = r_revision;
  g_ftsj_renc_n     = n_rules;
}

// Slot-cached variant of thvm_ftsj_rule_simpler: same verdicts, the
// rule-side encode + orientation pick amortized across join passes.
fn int thvm_ftsj_rule_simpler_cached(u32 slot, const AtpFtCell *rl,
                                     const AtpFtCell *rr, u8 oriented,
                                     const KboConfig *cfg) {
  if (g_ftsj_cp_hi_n == 0u) return 0;
  if (slot >= g_ftsj_renc_cap) {
    u32 cap = slot + 257u;
    FtsjRuleEnc *n = (FtsjRuleEnc *)realloc(g_ftsj_renc,
                                            (size_t)cap * sizeof(FtsjRuleEnc));
    if (n == NULL) return thvm_ftsj_rule_simpler(rl, rr, oriented, cfg);
    memset(n + g_ftsj_renc_cap, 0,
           (size_t)(cap - g_ftsj_renc_cap) * sizeof(FtsjRuleEnc));
    g_ftsj_renc     = n;
    g_ftsj_renc_cap = cap;
  }
  FtsjRuleEnc *e = &g_ftsj_renc[slot];
  if (e->overflow) return 0;
  if (!e->valid) {
    u32 na = 0u, nb = 0u;
    if (!atpft_kbo_flat_encode(rl, cfg, g_ftsj_scr_a, &na) ||
        !atpft_kbo_flat_encode(rr, cfg, g_ftsj_scr_b, &nb)) {
      // Fail-closed exactly like the uncached path -- and the overflow
      // is content-pure, so the "always 0" verdict is cacheable too.
      e->overflow = 1u;
      return 0;
    }
    const KboFlatNode *hi = g_ftsj_scr_a;
    const KboFlatNode *lo = g_ftsj_scr_b;
    u32 nhi = na, nlo = nb;
    if (!oriented &&
        atpft_kbo_skolem_cmp(g_ftsj_scr_a, 0u, g_ftsj_scr_b, 0u, cfg)
            == KBO_LT) {
      hi = g_ftsj_scr_b;
      lo = g_ftsj_scr_a;
      nhi = nb;
      nlo = na;
    }
    if (e->cap_hi < nhi) {
      KboFlatNode *p = (KboFlatNode *)realloc(e->hi,
                                              (size_t)nhi * sizeof(KboFlatNode));
      if (p == NULL) return thvm_ftsj_rule_simpler(rl, rr, oriented, cfg);
      e->hi = p;
      e->cap_hi = nhi;
    }
    if (e->cap_lo < nlo) {
      KboFlatNode *p = (KboFlatNode *)realloc(e->lo,
                                              (size_t)nlo * sizeof(KboFlatNode));
      if (p == NULL) return thvm_ftsj_rule_simpler(rl, rr, oriented, cfg);
      e->lo = p;
      e->cap_lo = nlo;
    }
    memcpy(e->hi, hi, (size_t)nhi * sizeof(KboFlatNode));
    memcpy(e->lo, lo, (size_t)nlo * sizeof(KboFlatNode));
    e->nhi = nhi;
    e->nlo = nlo;
    e->whi = 0;
    for (u32 i = 0; i < nhi; i++) e->whi += e->hi[i].w;
    e->valid = 1u;
  }
  // Twee orderedSimplerThan: (hi < HI) or (hi == HI and lo < LO).
  // Weight fast key: atpft_kbo_skolem_cmp's FIRST decision over the
  // full spans is the total-weight compare; both sums are precomputed
  // (rule side cached, CP side latched by prepare_eq), so an unequal
  // weight resolves the hi compare without touching the nodes.
  KboCmp ch;
  if (e->whi != g_ftsj_cp_whi) {
    ch = (e->whi > g_ftsj_cp_whi) ? KBO_GT : KBO_LT;
  } else {
    ch = atpft_kbo_skolem_cmp(e->hi, 0u, g_ftsj_cp_hi, 0u, cfg);
  }
  if (ch == KBO_LT) return 1;
  if (ch != KBO_EQ) return 0;
  return atpft_kbo_skolem_cmp(e->lo, 0u, g_ftsj_cp_lo, 0u, cfg) == KBO_LT;
}

// === LPO public entry ================================================
//
// Mirrors the dispatcher in thvm_lpo (src/lpo/_.c:680-748): encode
// both sides into g_lpo_flat_a/b (the buffers lpo_flat_rec keys its
// memo against via the `side` bit), run the cheap var-set
// incomparability check, bump the per-call memo epoch, and call
// lpo_flat_rec_compute directly.
//
// We do NOT take the (s, t) -> verdict top-level memo path (that one
// is keyed on heap-Term cells; AtpFtCell* is a different namespace
// and would alias incorrectly).  We DO bump g_lpo_flat_epoch so the
// inner (pa, pb, side) memo stays consistent for THIS call.
//
// On encode overflow we return LPO_UN (same caller-visible fallback
// as the KBO path).
fn LpoCmp thvm_lpo_ft(const AtpFtCell *a, const AtpFtCell *b,
                      const LpoConfig *cfg) {
  u32 na = 0u, nb = 0u;
  if (!atpft_lpo_flat_encode(a, cfg, g_lpo_flat_a, &na) ||
      !atpft_lpo_flat_encode(b, cfg, g_lpo_flat_b, &nb)) {
    return LPO_UN;
  }
  // Var-set incomparability pretest (src/lpo/_.c:706-730): if one
  // side has a variable the other doesn't, the pair is LPO_UN.  The
  // walk is cheap (single pass over the two flat arrays) and saves
  // the recursive descent in the common ATP "two equations sharing
  // disjoint variables" case.
  {
    u32 vs[LPO_MAX_VAR] = {0};
    u32 vt[LPO_MAX_VAR] = {0};
    u8 in_range = 1u;
    for (u32 i = 0; i < na; i++) {
      if (g_lpo_flat_a[i].sym < 0) {
        u32 vid = (u32)(-g_lpo_flat_a[i].sym - 1);
        if (vid >= LPO_MAX_VAR) { in_range = 0u; break; }
        vs[vid] = 1u;
      }
    }
    if (in_range) for (u32 i = 0; i < nb; i++) {
      if (g_lpo_flat_b[i].sym < 0) {
        u32 vid = (u32)(-g_lpo_flat_b[i].sym - 1);
        if (vid >= LPO_MAX_VAR) { in_range = 0u; break; }
        vt[vid] = 1u;
      }
    }
    if (in_range) {
      u8 s_extra = 0u, t_extra = 0u;
      for (u32 i = 0; i < LPO_MAX_VAR; i++) {
        if (vs[i] && !vt[i]) s_extra = 1u;
        if (vt[i] && !vs[i]) t_extra = 1u;
      }
      if (s_extra && t_extra) return LPO_UN;
    }
  }
  // Bump the per-call (pa, pb, side) memo epoch -- positions are
  // only meaningful for THIS encode pair (the buffers get reused).
  if (++g_lpo_flat_epoch == 0u) {
    for (u32 i = 0; i < LPO_FLAT_MEMO_SIZE; i++) g_lpo_flat_memo[i].epoch = 0u;
    g_lpo_flat_epoch = 1u;
  }
  return lpo_flat_rec_compute(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u, cfg);
}

#endif // !ATPFT_FT_ORDER_C_INCLUDED
#endif // THVM_ATPFT_CONVERT
