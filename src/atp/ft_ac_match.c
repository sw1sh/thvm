// ft_ac_match.c - AtpFt-native AC (associative-commutative)
// one-way matching.  Port of the Term-side `atp_match_ac`
// (src/atp/ac.c:317) to the flatterm cell representation, so the
// FT normalize path (find_redex_ft) can match AC-modulo without
// round-tripping through Term cells.
//
// Algorithm parity:
//   * ft_ac_flatten_under / ft_ac_flatten -- mirror the Term-side
//     recursion: if `t` is a CTR with `t->sym == top_label`, recurse
//     on each child via the FT sibling stride `c = c->end->next`;
//     else `t` becomes a singleton leaf.  Cap = ATP_AC_FLATTEN_CAP.
//   * ft_match_ac -- dispatch: AC top -> flatten both sides, AC-flat
//     match; non-AC CTR -> same-arity recursive descent; var ->
//     bind-or-check via ft_eq.  Falls back to ft_match when the AC
//     mask is empty / the top is not AC.
//   * ft_match_ac_flat -- two-pass multiset match.  Pass 1: every
//     non-unbound-var pattern leaf consumes a subject leaf (ground /
//     bound-var via ft_eq, CTR via recursive ft_match_ac).  Pass 2:
//     distribute leftover subject leaves over the unbound vars.
//     Cases mirror ac.c:470-528 exactly: 0 -> leftover empty; 1 with
//     mult 1 -> single binding or AC-chain over leftovers; 1 with
//     mult m > 1 -> leftover is m copies of one value; >1 -> greedy
//     pair-up by multiplicity (sum-of-mults == leftover.count).
//
// Watermark-based rewind: on every failure path we call
// `ft_subst_rewind(subst, wm_save)` to undo bindings installed during
// the failed attempt -- same discipline as ft_match.  AC-chain bindings
// (the leftover absorption case) ARE recorded in `bound_ids[]`, so
// a downstream rewind correctly clears them.
//
// AC chains: built in the SCRATCH arena via `ftnew_ctr(a, ..., 1)`.
// The scratch arena gets reset by the caller (normalize) on every
// fixpoint iteration; chain cells live exactly through the current
// match attempt.  Persistent (Arena A) is reserved for splice products.
//
// Gated on THVM_ATP_AC && THVM_ATPFT_MATCH.  Routed at runtime via
// `THVM_ATPFT_AC_MATCH=1` env var inside find_redex_ft; default off
// keeps the syntactic ft_match path the only matcher.
//
// Coverage notes -- same as Term-side:
//   * f(x, e) -> x, f(x, x) -> x, f(x, y, z) -> ... all handled.
//   * Nested f(x, f(y, z)) handled via the flatten step.
//   * f(x, y) against subject f(a, b, c) with both unbound: partition
//     enumeration is the deferred work; greedy pair-up returns 0 when
//     sum-of-mults != leftover, matching the Term-side bail.
//   * Backtracking inside Pass 1 (greedy CTR matched a leaf needed
//     by a later var) is the same conservative-fail behaviour.

#if defined(THVM_ATP_AC) && defined(THVM_ATPFT_MATCH)
#ifndef ATPFT_FT_AC_MATCH_C_INCLUDED
#define ATPFT_FT_AC_MATCH_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"      // WF_VAR_BIT (same encoding as ft_match.c)

// Cap on flattened leaves per AC position.  Matches Term-side
// ATP_AC_FLATTEN_CAP (ac.c:146).  AC nodes with more than 64 leaves
// fall through to the syntactic ft_match path (no AC match attempted).
#define ATPFT_AC_FLATTEN_CAP 64u

// Cap on distinct unbound vars at one AC position.  Mirrors
// ATPFT_AC_MAX_UNBOUND (ac.c:342) -- beyond this we bail conservatively
// (no partition enumeration yet).
#define ATPFT_AC_MAX_UNBOUND_VARS 8u

// Test whether `label` is an AC label under `ac`.  Mirrors
// `atp_ac_is_ac_label` (ac.c:69) but kept local so this file is
// includable without ac.c's statics in scope.
static inline u8 ft_ac_is_ac_label(const AtpAcInfo *ac, u32 label) {
  if (ac == NULL) return 0;
  if (label >= 64u) return 0;
  return (u8)((ac->ac_mask >> label) & 1ull);
}

// Test whether an FT cell's top symbol is AC.  False for variable
// cells and for symbols outside the 64-bit mask range.
static inline u8 ft_ac_is_ac_top(const AtpAcInfo *ac, const AtpFtCell *t) {
  if (t == NULL) return 0;
  if ((t->sym & WF_VAR_BIT) != 0u) return 0;
  return ft_ac_is_ac_label(ac, t->sym);
}

// Recursive flatten under a fixed `top_label`.  Walks the FT sibling
// chain.  Pre-order.  Returns 0 on cap overflow with `*n` left at the
// last successful write; 1 on success.
//
// For a `t` whose top is NOT `top_label` (or which is a var cell),
// emits `t` as a single leaf.
static u8 ft_ac_flatten_under(const AtpFtCell *t, u32 top_label,
                              const AtpFtCell **out, u32 *n, u32 cap) {
  if (t == NULL) return 0;
  u8 is_ctr = (u8)((t->sym & WF_VAR_BIT) == 0u);
  if (is_ctr && t->sym == top_label && t->arity > 0u) {
    const AtpFtCell *c = t->next;
    for (u16 i = 0; i < t->arity; i++) {
      if (!ft_ac_flatten_under(c, top_label, out, n, cap)) return 0;
      c = c->end->next;
    }
    return 1;
  }
  if (*n >= cap) return 0;
  out[*n] = t;
  *n += 1u;
  return 1;
}

// Public entry: flatten under `t`'s top label, if AC.  Otherwise emit
// `t` as a singleton.  Returns 0 on cap overflow; 1 on success.
static u8 ft_ac_flatten(const AtpFtCell *t, const AtpAcInfo *ac,
                        const AtpFtCell **out, u32 *n_out, u32 cap) {
  *n_out = 0u;
  if (t == NULL) return 0;
  if (ft_ac_is_ac_top(ac, t)) {
    return ft_ac_flatten_under(t, t->sym, out, n_out, cap);
  }
  if (cap == 0u) return 0;
  out[0] = t;
  *n_out = 1u;
  return 1;
}

// Build a right-associative AC chain `f(leaves[0], f(leaves[1], ...,
// leaves[n-1]))` for binding to a single unbound var that absorbs all
// leftover subject leaves.  Mirrors `atp_ac_build_chain` (ac.c:170)
// but allocates the new CTR cells into the SCRATCH arena (scratch=1)
// since chains live exactly through the current match attempt.
//
// Caller guarantees `n >= 1`.  For n == 1 returns leaves[0] verbatim
// (no allocation -- saves a chain wrap on the common case).
static AtpFtCell *ft_ac_build_chain(AtpFt *a, u32 label,
                                    const AtpFtCell **leaves, u32 n) {
  if (n == 1u) return (AtpFtCell *)leaves[0];
  // Fold right-to-left: acc starts as the tail, then we prepend each
  // earlier leaf as the left child of a fresh f-node.
  AtpFtCell *acc = (AtpFtCell *)leaves[n - 1u];
  for (i32 i = (i32)n - 2; i >= 0; i--) {
    AtpFtCell *kids[2] = { (AtpFtCell *)leaves[(u32)i], acc };
    acc = ftnew_ctr(a, label, 2u, kids, /*scratch=*/1);
    if (acc == NULL) return NULL;
  }
  return acc;
}

// Forward decls -- top-level dispatch and flat multiset match are
// mutually recursive (a CTR pattern leaf inside the multiset triggers
// a recursive ft_match_ac descent).
static u8 ft_match_ac(AtpFt *a, const AtpFtCell *pat, const AtpFtCell *subj,
                      const AtpAcInfo *ac, AtpFtSubst *subst);

// AC-flat-vs-flat match: pattern leaves P[] vs subject leaves S[],
// both already AC-flattened under the same AC label `label`.
//
// Watermark-based rewind: the caller saves `subst->wm` before this
// call, and on any false-return WE rewind to that saved watermark
// ourselves so the caller doesn't have to.
static u8 ft_match_ac_flat(AtpFt *a, u32 label,
                           const AtpFtCell **P, u32 np,
                           const AtpFtCell **S, u32 ns,
                           const AtpAcInfo *ac, AtpFtSubst *subst) {
  u32 wm_save = subst->wm;
  if (np > ns) return 0;
  if (ns > ATPFT_AC_FLATTEN_CAP) return 0;

  // Pass 0: identify each distinct unbound-var pattern leaf with its
  // multiplicity.  Mirrors ac.c:351-373.
  u32 ub_vid [ATPFT_AC_MAX_UNBOUND_VARS];
  u32 ub_mult[ATPFT_AC_MAX_UNBOUND_VARS];
  u32 n_ub = 0u;
  for (u32 i = 0; i < np; i++) {
    const AtpFtCell *p = P[i];
    if ((p->sym & WF_VAR_BIT) == 0u) continue;       // not a var leaf
    u32 vid = p->sym & ~WF_VAR_BIT;
    if (vid >= ATPFT_MAX_VARS) { ft_subst_rewind(subst, wm_save); return 0; }
    if (subst->bind[vid] != NULL) continue;          // already bound
    u32 k = 0u;
    while (k < n_ub && ub_vid[k] != vid) k++;
    if (k < n_ub) {
      ub_mult[k] += 1u;
    } else if (n_ub < ATPFT_AC_MAX_UNBOUND_VARS) {
      ub_vid [n_ub] = vid;
      ub_mult[n_ub] = 1u;
      n_ub += 1u;
    } else {
      ft_subst_rewind(subst, wm_save);
      return 0;                                       // too many distinct vars
    }
  }

  // Pass 1: every NON-unbound-var pattern leaf greedily consumes one
  // subject leaf.  Bound vars use ft_eq against their binding; CTR
  // leaves recurse via ft_match_ac (which may itself bind fresh vars
  // -- those bindings ride on the same watermark trail and rewind
  // together on a later failure).
  u8 used[ATPFT_AC_FLATTEN_CAP] = {0};
  for (u32 i = 0; i < np; i++) {
    const AtpFtCell *p = P[i];
    u8 is_var = (u8)((p->sym & WF_VAR_BIT) != 0u);
    if (is_var) {
      u32 vid = p->sym & ~WF_VAR_BIT;
      if (vid >= ATPFT_MAX_VARS) { ft_subst_rewind(subst, wm_save); return 0; }
      AtpFtCell *prev = subst->bind[vid];
      if (prev == NULL) continue;                     // unbound, deferred to Pass 2
      u8 matched = 0u;
      for (u32 j = 0; j < ns; j++) {
        if (used[j]) continue;
        if (ft_eq(S[j], prev)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) { ft_subst_rewind(subst, wm_save); return 0; }
    } else if (p->arity > 0u) {
      // CTR with children: recurse.  Each recursion may bind fresh
      // vars; ft_match_ac records them via the shared watermark trail.
      u8 matched = 0u;
      for (u32 j = 0; j < ns; j++) {
        if (used[j]) continue;
        u32 wm_attempt = subst->wm;
        if (ft_match_ac(a, p, S[j], ac, subst)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
        // Recursion failed mid-attempt -- ft_match_ac is supposed to
        // rewind on failure, but belt-and-braces guard here too.
        ft_subst_rewind(subst, wm_attempt);
      }
      if (!matched) { ft_subst_rewind(subst, wm_save); return 0; }
    } else {
      // 0-arity const leaf: ft_eq against an unused subject leaf.
      u8 matched = 0u;
      for (u32 j = 0; j < ns; j++) {
        if (used[j]) continue;
        if (ft_eq(S[j], p)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) { ft_subst_rewind(subst, wm_save); return 0; }
    }
  }

  // Pass 2: leftover subject leaves distributed over unbound vars.
  // First collect the leftover sequence.
  const AtpFtCell *left_buf[ATPFT_AC_FLATTEN_CAP];
  u32 leftover = 0u;
  for (u32 j = 0; j < ns; j++) {
    if (!used[j]) left_buf[leftover++] = S[j];
  }

  // Re-scan ub_vid for vars that pass 1 might have bound via a CTR
  // sub-match (e.g. `f(g(x), z)` pattern -- the g(x) recursion binds
  // x, so x is no longer "unbound" by the time pass 2 runs).  Skip
  // those to avoid double-binding via the leftover-absorption clause
  // below (which would silently overwrite the pass-1 binding and
  // produce an unsound match).  Mirrors ac.c:452-468.
  {
    u32 still = 0u;
    u32 nv_vid [ATPFT_AC_MAX_UNBOUND_VARS];
    u32 nv_mult[ATPFT_AC_MAX_UNBOUND_VARS];
    for (u32 k = 0; k < n_ub; k++) {
      if (subst->bind[ub_vid[k]] == NULL) {
        nv_vid [still] = ub_vid[k];
        nv_mult[still] = ub_mult[k];
        still++;
      }
    }
    n_ub = still;
    for (u32 k = 0; k < still; k++) {
      ub_vid[k]  = nv_vid[k];
      ub_mult[k] = nv_mult[k];
    }
  }

  // Helper: install a binding for `vid` and trail it on the watermark
  // so a later failure restores cleanly.
  #define FT_AC_BIND(vid_, cell_) do {                                   \
    u32 _v = (vid_);                                                     \
    if (_v >= ATPFT_MAX_VARS || (cell_) == NULL) {                       \
      ft_subst_rewind(subst, wm_save);                                   \
      return 0;                                                          \
    }                                                                    \
    subst->bind[_v]                = (AtpFtCell *)(cell_);               \
    subst->bound_ids[subst->wm]    = _v;                                 \
    subst->wm                     += 1u;                                 \
  } while (0)

  if (n_ub == 0u) {
    if (leftover == 0u) return 1u;
    ft_subst_rewind(subst, wm_save);
    return 0;
  }

  if (n_ub == 1u) {
    u32 m = ub_mult[0];
    if (leftover < m) { ft_subst_rewind(subst, wm_save); return 0; }
    if (m == 1u) {
      // One pattern occurrence -- bind to the single leftover (np==1)
      // or the AC-chain over all leftovers.
      AtpFtCell *binding = (leftover == 1u)
        ? (AtpFtCell *)left_buf[0]
        : ft_ac_build_chain(a, label, left_buf, leftover);
      if (binding == NULL) { ft_subst_rewind(subst, wm_save); return 0; }
      FT_AC_BIND(ub_vid[0], binding);
      return 1u;
    }
    // Multiplicity > 1: leftover must be exactly m copies of one value.
    if (leftover != m) { ft_subst_rewind(subst, wm_save); return 0; }
    const AtpFtCell *candidate = left_buf[0];
    for (u32 j = 1; j < leftover; j++) {
      if (!ft_eq(left_buf[j], candidate)) {
        ft_subst_rewind(subst, wm_save);
        return 0;
      }
    }
    FT_AC_BIND(ub_vid[0], candidate);
    return 1u;
  }

  // n_ub >= 2.  Greedy pair-up requires sum-of-multiplicities ==
  // leftover.count -- otherwise the assignment is ambiguous and a
  // full partition enumerator would be needed (deferred).  Mirrors
  // ac.c:495-527.
  u32 total_slots = 0u;
  for (u32 k = 0; k < n_ub; k++) total_slots += ub_mult[k];
  if (total_slots != leftover) { ft_subst_rewind(subst, wm_save); return 0; }

  u8 used2[ATPFT_AC_FLATTEN_CAP] = {0};
  for (u32 k = 0; k < n_ub; k++) {
    u32 m = ub_mult[k];
    // First unused leftover leaf becomes the candidate.
    u32 base = (u32)-1;
    for (u32 j = 0; j < leftover; j++) {
      if (!used2[j]) { base = j; break; }
    }
    if (base == (u32)-1) { ft_subst_rewind(subst, wm_save); return 0; }
    const AtpFtCell *candidate = left_buf[base];
    used2[base] = 1u;
    u32 need = m - 1u;
    for (u32 j = base + 1u; need > 0u && j < leftover; j++) {
      if (used2[j]) continue;
      if (ft_eq(left_buf[j], candidate)) {
        used2[j] = 1u;
        need -= 1u;
      }
    }
    if (need > 0u) { ft_subst_rewind(subst, wm_save); return 0; }
    FT_AC_BIND(ub_vid[k], candidate);
  }

  #undef FT_AC_BIND
  return 1u;
}

// Top-level AC match: same shape as ft_match, but routes through the
// AC-flat multiset matcher when the top symbol is an AC label.
//
// On any failure WE rewind to whatever watermark the caller staged
// before invoking us -- callers don't have to track partial state.
static u8 ft_match_ac(AtpFt *a, const AtpFtCell *pat, const AtpFtCell *subj,
                      const AtpAcInfo *ac, AtpFtSubst *subst) {
  if (pat == NULL || subj == NULL) return 0;
  u32 wm_save = subst->wm;

  // Variable pattern: bind on first sight, ft_eq on second.  Same as
  // ft_match (ft_match.c:155).
  if ((pat->sym & WF_VAR_BIT) != 0u) {
    u32 id = pat->sym & ~WF_VAR_BIT;
    if (id >= ATPFT_MAX_VARS) return 0;
    AtpFtCell *prev = subst->bind[id];
    if (prev == NULL) {
      subst->bind[id]              = (AtpFtCell *)subj;
      subst->bound_ids[subst->wm]  = id;
      subst->wm                   += 1u;
      return 1u;
    }
    return ft_eq(prev, subj) ? 1u : 0u;
  }

  // CTR pattern: subj must be CTR with matching sym.  For an AC top
  // we ignore the arity equality (the two sides can be flattened to
  // different leaf counts and still match -- e.g. f(x, y) vs f(a, b, c)
  // with x bound to a, y absorbing the AC-chain f(b, c)).  For non-AC
  // CTR top, arity must match exactly.
  if ((subj->sym & WF_VAR_BIT) != 0u) return 0;        // subj is var, pat is CTR -> fail
  if (pat->sym != subj->sym) return 0;

  if (ft_ac_is_ac_label(ac, pat->sym)) {
    const AtpFtCell *P[ATPFT_AC_FLATTEN_CAP];
    const AtpFtCell *S[ATPFT_AC_FLATTEN_CAP];
    u32 np = 0u, ns = 0u;
    if (!ft_ac_flatten_under(pat,  pat->sym, P, &np, ATPFT_AC_FLATTEN_CAP)) {
      ft_subst_rewind(subst, wm_save);
      return 0;
    }
    if (!ft_ac_flatten_under(subj, subj->sym, S, &ns, ATPFT_AC_FLATTEN_CAP)) {
      ft_subst_rewind(subst, wm_save);
      return 0;
    }
    return ft_match_ac_flat(a, pat->sym, P, np, S, ns, ac, subst);
  }

  // Non-AC CTR: same-arity recursive descent.  Walks the FT sibling
  // chain like ft_match does.
  if (pat->arity != subj->arity) return 0;
  u16 arity = pat->arity;
  const AtpFtCell *pc = pat->next;
  const AtpFtCell *sc = subj->next;
  for (u16 i = 0; i < arity; i++) {
    if (!ft_match_ac(a, pc, sc, ac, subst)) {
      ft_subst_rewind(subst, wm_save);
      return 0;
    }
    pc = pc->end->next;
    sc = sc->end->next;
  }
  return 1u;
}

#endif // !ATPFT_FT_AC_MATCH_C_INCLUDED
#endif // THVM_ATP_AC && THVM_ATPFT_MATCH
