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
  u32 na = 0u, nb = 0u;
  if (!atpft_kbo_flat_encode(redex, cfg, g_atpft_kbo_flat_a, &na) ||
      !atpft_kbo_flat_encode_subst(tmpl, cfg, subst,
                                   g_atpft_kbo_flat_b, &nb)) {
    return KBO_UN;
  }
  return thvm_kbo_flat_slice(g_atpft_kbo_flat_a, na,
                             g_atpft_kbo_flat_b, nb, cfg);
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
