// ft_norm.c - AtpFt-native innermost-rewrite
// normalize fixpoint.
//
// Loop shape (mirrors WM `BL_NormalformInnermost2` in NFBildung.c:622):
//
//   1. Entry-clear pass: walk root, clear ATPFT_FLAG_SUBST_FRESH on
//      every cell (O(|t|) once per call).
//
//   2. Step loop (bounded by `step_cap`):
//      a. find_redex_ft: pre-order walk via `cell->next`, skipping any
//         cell with SUBST_FRESH=1 (the innermost-rewrite shortcut --
//         a cell installed by an earlier step in this same fixpoint
//         doesn't need a redex re-check, by construction of WM-style
//         right-reduction).
//      b. For each non-fresh cell, attempt one-way match against
//         every rule LHS (s->lhs_ft[0..n_rules)).  Rule selection is
//         "first-match-wins" -- the rule-index discrimination tree
//         would replace this linear scan; Stage 6 ships the simple
//         scan to keep the splice splice's correctness gate decoupled
//         from the index plumbing.
//      c. If no redex -> break.
//      d. Splice (ft_splice).  The root may change identity (regime
//         (c) root rewrite); we propagate via the new-root return.
//
// All cells live in Arena A (the persistent slab pool).  Stage 6 leaks
// Arena A growth across normalize calls -- the Stage 4 GC sweeps on
// schedule.  Scratch (Arena B) is unused on this path.
//
// Gated on THVM_ATPFT_NORM.

#ifdef THVM_ATPFT_NORM

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"

// Symbols brought in from sibling stages.  The AtpFtSubst type is
// declared by ft_match.c (always included earlier in the same TU);
// `ft_match` / `ft_subst_reset` keep their natural typed signatures.
extern int  ft_eq      (const AtpFtCell *x, const AtpFtCell *y);

// Forward decl for the trace-push helper -- defined in src/atp/_.c
// further down the same TU.  ft_norm.c is included BEFORE the helper
// (the include ordering wires _.c's later trace machinery), so a
// matching `static` forward decl here is what the record-mode splice
// loop below references.
static u32 atp_trace_push_norm_step(AtpState *s, u32 p_a, u32 rule_idx,
                                    Term lhs, Term rhs,
                                    u8 side, u8 fwd,
                                    const u8 *pos, u8 pos_len);

extern AtpFtCell *ft_splice(AtpFt          *a,
                            AtpFtCell      *root,
                            AtpFtCell      *parent,
                            AtpFtCell      *redex,
                            const AtpFtCell *rhs_tmpl,
                            const void     *subst);

// Stage 6b hooks -- defined in ft_ri.c when THVM_ATPFT_RI is on.
#ifdef THVM_ATPFT_RI
extern int  atp_ri_find_redex_ft_pub(AtpState *s, AtpFtCell *root,
                                     AtpFtCell **redex_out, u32 *rule_out);
extern void atp_ri_ft_sync(AtpState *s);
#endif

// --- Variable-containment helper for unorientable equations ---------
//
// Mirrors `atp_vars_contained` in src/atp/_.c (the Term-side ordered
// rewriter's extension-variable guard): returns 1 iff every variable
// id occurring in `target` also occurs somewhere in `source`.  Used to
// forbid extension variables when an unorientable equation l == r
// fires backward (r -> l): r must not introduce any free variable not
// already present on the l side, otherwise the rewrite is unsound (the
// rule isn't actually a rewrite rule in that direction).
//
// Built around a 64-bit bitmask of var ids -- consistent with
// ATPFT_MAX_VARS=64 (ft_match.c).  Var ids >= 64 disable the fast
// path (return 0 = "not contained" -> the rewrite is skipped, same
// effect as the Term path's bail-out via thvm_match cap).
//
// Walks the AtpFtCell tree via the `next` / `end` chain -- the same
// stride find_redex_ft uses; no recursion, O(|cells|).
static u64 ft_vars_mask(const AtpFtCell *c) {
  if (c == NULL) return 0ull;
  u64 mask = 0ull;
  const AtpFtCell *end_after = (c->end != NULL) ? c->end->next : NULL;
  for (const AtpFtCell *p = c; p != NULL && p != end_after; p = p->next) {
    if ((p->sym & WF_VAR_BIT) != 0u) {
      u32 id = p->sym & ~WF_VAR_BIT;
      if (id < 64u) mask |= (1ull << id);
      else          return ~0ull;     // poison: contains an out-of-range id
    }
  }
  return mask;
}

static int ft_vars_contained(const AtpFtCell *target,
                             const AtpFtCell *source) {
  u64 tm = ft_vars_mask(target);
  u64 sm = ft_vars_mask(source);
  if (tm == ~0ull || sm == ~0ull) return 0;
  return (tm & ~sm) == 0ull;
}

// `ft_to_term` / `atp_compare` are defined elsewhere in the same TU
// (src/atp/_.c) when ft_norm.c is `#include`'d after them.  No extern
// declarations needed -- the call-site sees the in-TU defs directly.
// AtpFtSubst typedef comes from ft_match.c (already included earlier).
//
// FT-native KBO compare lives in src/atp/ft_order.c, which is NOT
// pulled in by _.c -- the symbol is fn (exported), so we forward-
// declare it here before find_redex_ft / its discrim variant.
extern KboCmp thvm_kbo_ft(const AtpFtCell *a, const AtpFtCell *b,
                          const KboConfig *cfg);
// Streaming variant: encodes (tmpl, subst) inline into the kbo_flat
// buffer instead of materialising subst(tmpl) as AtpFtCell* first.
extern KboCmp thvm_kbo_ft_subst(const AtpFtCell *redex,
                                const AtpFtCell *tmpl,
                                const void      *subst,
                                const KboConfig *cfg);
// Two-stage variant: encode redex once into the shared buffer, reuse
// across many rule attempts at the same cell.
extern u32    thvm_kbo_ft_subst_prepare_redex(const AtpFtCell *redex,
                                              const KboConfig *cfg);
extern KboCmp thvm_kbo_ft_subst_with_prepared(u32 na,
                                              const AtpFtCell *tmpl,
                                              const void      *subst,
                                              const KboConfig *cfg);

// --- SUBST_FRESH entry-clear ----------------------------------------
//
// Walk via `cell->next` pre-order.  This visits every cell of the
// term exactly once (the AtpFt invariant: a term's cells form a
// next-threaded chain from `root` to `root->end`, with `root->end->next`
// being NULL or pointing into the parent's chain -- and for a root
// term the latter is NULL).
//
// We stop AT root->end, inclusive: that's the last cell of THIS term.
static void ft_clear_subst_fresh(AtpFtCell *root) {
  if (root == NULL) return;
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  for (AtpFtCell *p = root; p != NULL && p != end_after; p = p->next) {
    p->flags &= (u8)~ATPFT_FLAG_SUBST_FRESH;
  }
}

// --- find_redex_ft --------------------------------------------------
//
// Walk `root` pre-order via `cell->next`.  For each cell whose
// SUBST_FRESH bit is clear, try the rule LHSs in the slice
// [slice_first, slice_first + slice_count).  If a rule matches,
// fill `subst_out`, set `redex_out` to the cell and `parent_out` to
// the cell whose `next` reaches the redex (NULL for the root case),
// return 1.  Return 0 if no redex.
//
// `parent_out`: the cell BEFORE the redex in the pre-order chain.  We
// track it as we walk; it's the cell whose `next` is the current
// candidate.  For the root, no predecessor exists -> NULL.
//
// `dir_out`: 0 = forward (LHS -> RHS), 1 = backward (RHS -> LHS).
// Always 0 in `try_orient` mode (orientable rules only).
// May be either when `try_unorient` is set.
//
// `try_orient`: when set, the linear scan considers orientable rules
//   (s->r_orient[r] == 1, or every rule when s->n_unorient == 0).
// `try_unorient`: when set, the linear scan considers unorientable
//   equations (s->r_orient[r] == 0, s->n_unorient > 0).
//
// The Term-side mixed loop (atp_rewrite_normalize_ordered) alternates
// an orientable-only fixpoint with ONE unorientable step; mirror that
// shape here so the two NFs agree under THVM_ATPFT_NORM_VERIFY.  A
// single-pass linear scan that tries every rule kind at every position
// (the original Stage 6 shape) reaches the same NF in a confluent
// system, but the live ATP saturation pumps non-confluent intermediate
// rule sets at every CP, and a strategy split there is exactly what
// the VERIFY-mode mismatch detects.  Staging the two kinds in lock-step
// with the Term path keeps the verdicts byte-equal.
//
// Unorientable equations are gated like atp_ordered_try_top
// (src/atp/_.c:5436):
//   1. Both directions are TRIED via ft_match.
//   2. Each direction's RHS template must have its variables contained
//      in its LHS template (no extension vars) -- ft_vars_contained.
//   3. The instantiated rewrite must strictly decrease the redex in the
//      reduction order: atp_compare(redex_term, repl_term) == KBO_GT.
//      Implemented via a per-step ft_to_term round-trip; correct +
//      simple, refine if profiling shows it as a hot spot.
// Without these guards, an unorientable rule whose RHS legitimately
// contains extension variables (e.g. introduced by symmetric goal
// equations) would feed ft_splice an unbound-var template and bail
// with NULL, looping until step_cap and producing an under-rewritten
// normal form -- the headline bug fix.
//
// The full-range entry point (`atp_rewrite_normalize_ft`) passes
// slice_first=0 / slice_count=s->n_rules; the slice entry point
// (`atp_rewrite_normalize_ft_slice`) passes the caller's [first,count)
// directly.  Iterating only the slice is the whole point of the slice
// path -- interreduce wants to rewrite an EXISTING rule against the
// just-added rules, NOT against itself (would loop) or against the
// older rules (those gave it CPs; rewriting through them is a no-op
// since the rule is already in normal form w.r.t. the older R).

// AC-aware match dispatch.  When an AC mask has been registered
// (thvm_atp_get_ac_mask() != 0), we route through the AC-modulo
// matcher in ft_ac_match.c by default.  Set THVM_ATPFT_AC_MATCH=0 in
// env to fall back to the syntactic ft_match path.  Flipped on by
// default 2026-06-07 after measuring: ac-ring iters 24 -> 14 (-42%),
// ac-abelian QUEUE_EMPTY iters 14 -> 10, no regression on mccune
// (14.9s) / thm / test_atp (135623/135623) / test_atp_ft_rules.
// The env flag is cached on first call -- changing it mid-process
// has no effect, by design (same convention as THVM_ATPFT_KBO_DIFF).
static inline int ft_match_maybe_ac(AtpFt *a,
                                    const AtpFtCell *pat,
                                    const AtpFtCell *subj,
                                    AtpFtSubst *subst) {
#if defined(THVM_ATP_AC) && defined(THVM_ATPFT_MATCH)
  static int ac_on = -1;
  if (ac_on < 0) {
    const char *e = getenv("THVM_ATPFT_AC_MATCH");
    ac_on = (e == NULL || (e[0] != '0' && e[0] != '\0')) ? 1 : 0;
  }
  if (ac_on) {
    u64 mask = thvm_atp_get_ac_mask();
    if (mask != 0ull) {
      AtpAcInfo ac = { .ac_mask = mask };
      return ft_match_ac(a, pat, subj, &ac, subst);
    }
  }
#endif
  (void)a;
  return ft_match(pat, subj, subst);
}

static int find_redex_ft(AtpState        *s,
                         AtpFtCell       *root,
                         u32              slice_first,
                         u32              slice_count,
                         AtpFtCell      **parent_out,
                         AtpFtCell      **redex_out,
                         u32             *rule_out,
                         u8              *dir_out,
                         AtpFtSubst      *subst_buf,
                         u8               try_orient,
                         u8               try_unorient) {
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  AtpFtCell *prev = NULL;
  u8 have_unorient = (u8)(s->n_unorient > 0u);
  u32 slice_end = slice_first + slice_count;
  if (slice_end > s->n_rules) slice_end = s->n_rules;
  // Arena handle for the AC-match dispatch (AC-chain bindings allocate
  // into the scratch arena).  NULL-safe: the dispatch only deref's it
  // when the AC path actually fires.
  AtpFt *ft_arena_local = (AtpFt *)s->ft_arena_ptr;
  for (AtpFtCell *p = root; p != NULL && p != end_after; p = p->next) {
    if ((p->flags & ATPFT_FLAG_SUBST_FRESH) != 0u) {
      prev = p;
      continue;
    }
    // Skip variable cells -- a free var is never a redex on the
    // subject side.
    if ((p->sym & WF_VAR_BIT) != 0u) {
      prev = p;
      continue;
    }
    // Lazy redex pre-encode: thvm_kbo_ft_subst encodes both sides on
    // every call; we attempt up to `slice_count` rules at this cell,
    // each running the unorient gate with the SAME redex `p`.
    // Pre-encode once, reuse across attempts.  redex_na==0 means
    // "not yet encoded"; the FIRST gate call lazily fills it.
    u32 redex_na = 0u;
    for (u32 r = slice_first; r < slice_end; r++) {
      // Filter dead (bwd-subsumed) rules first: their lhs_ft/rhs_ft are
      // overwritten with a sentinel FVR (id 255) at deletion time, which
      // an unorientable backward-direction match could spuriously bind.
      // The Term-side indexed paths (atp_ri_rebuild) skip them; mirror
      // that here so the linear FT scan agrees.
      if (s->r_dead != NULL && s->r_dead[r]) continue;
      AtpFtCell *lhs = s->lhs_ft[r];
      AtpFtCell *rhs = s->rhs_ft[r];
      if (lhs == NULL || rhs == NULL) continue;
      u8 oriented = (u8)(!have_unorient || s->r_orient[r]);
      if (oriented) {
        if (!try_orient) continue;
        // Forward rewrite, no order check needed (oriented).
        ft_subst_reset(subst_buf);
        if (ft_match_maybe_ac(ft_arena_local, lhs, p, subst_buf)) {
          *parent_out = (p == root) ? NULL : prev;
          *redex_out  = p;
          *rule_out   = r;
          *dir_out    = 0u;
          return 1;
        }
        continue;
      }
      if (!try_unorient) continue;
      // Unorientable equation: try forward (l -> r) then backward (r -> l).
      // Forward: lhs matches p, rhs's vars are subset of lhs's vars,
      // instantiated repl < redex under the reduction order.  Streaming
      // KBO via thvm_kbo_ft_subst_with_prepared; THVM_ATPFT_KBO_DIFF=1
      // runs a side-by-side Term-side atp_compare to surface verdict
      // divergences (a probe across the AC bench found zero -- the
      // streaming KBO matches atp_compare on the unorient gate inputs).
      if (ft_vars_contained(rhs, lhs)) {
        ft_subst_reset(subst_buf);
        if (ft_match_maybe_ac(ft_arena_local, lhs, p, subst_buf)) {
          if (redex_na == 0u) redex_na = thvm_kbo_ft_subst_prepare_redex(p, s->kbo);
          KboCmp ft_v = thvm_kbo_ft_subst_with_prepared(redex_na, rhs, subst_buf, s->kbo);
          static int dbg_diff = -1;
          if (dbg_diff < 0) dbg_diff = getenv("THVM_ATPFT_KBO_DIFF") != NULL ? 1 : 0;
          if (dbg_diff) {
            AtpFt *arena_chk = (AtpFt *)s->ft_arena_ptr;
            AtpFtCell *repl = ft_subst_apply(arena_chk, rhs, subst_buf, 1);
            if (repl != NULL) {
              Term t_p    = ft_to_term(p);
              Term t_repl = ft_to_term(repl);
              KboCmp tt_v = atp_compare(s, t_p, t_repl);
              if (ft_v != tt_v) {
                fprintf(stderr,
                        "[KBO DIFF fwd] ft=%d tt=%d  p=0x%016llx  sigma_r=0x%016llx\n",
                        (int)ft_v, (int)tt_v,
                        (unsigned long long)t_p,
                        (unsigned long long)t_repl);
              }
            }
          }
          if (ft_v == KBO_GT) {
            *parent_out = (p == root) ? NULL : prev;
            *redex_out  = p;
            *rule_out   = r;
            *dir_out    = 0u;
            return 1;
          }
          ft_subst_reset(subst_buf);
        }
      }
      // Backward: same gates with l/r swapped.
      if (ft_vars_contained(lhs, rhs)) {
        ft_subst_reset(subst_buf);
        if (ft_match_maybe_ac(ft_arena_local, rhs, p, subst_buf)) {
          if (redex_na == 0u) redex_na = thvm_kbo_ft_subst_prepare_redex(p, s->kbo);
          KboCmp ft_v = thvm_kbo_ft_subst_with_prepared(redex_na, lhs, subst_buf, s->kbo);
          if (ft_v == KBO_GT) {
            *parent_out = (p == root) ? NULL : prev;
            *redex_out  = p;
            *rule_out   = r;
            *dir_out    = 1u;
            return 1;
          }
          ft_subst_reset(subst_buf);
        }
      }
    }
    prev = p;
  }
  return 0;
}

// --- Fixpoint --------------------------------------------------------
//
// Public entry: normalize `t` against the AtpFt rule mirror in
// `s->lhs_ft / s->rhs_ft`.  Returns the normal form (a cell in Arena A;
// may be the same pointer as the input if no rewrite fired, OR a fresh
// root cell when the root was rewritten).
//
// Step cap is the caller's budget -- mirrors the Term-side NORM_CAP=64
// in atp_cp_trivially_joinable.  Hitting the cap returns the partial
// reduction; the caller's downstream `ft_eq` check then treats it as
// "not joinable".

// find_redex_ft via the discrim-tree descent (Stage 6b).  When enabled,
// we ALSO need a parent-cell for the splice and a verified substitution.
// The discrim-tree descent's leaf-collect returns the rule + star_ft
// bindings; we then re-derive parent via a pre-order scan from root,
// and rebuild the subst via ft_match (cheap: pattern-side traversal).
//
// This keeps the splice contract unchanged.  The Stage-6b WIN is on
// the find phase: O(|subject| * average descent depth) instead of
// O(|subject| * n_rules).
#ifdef THVM_ATPFT_RI
static int find_redex_ft_via_ri(AtpState        *s,
                                AtpFtCell       *root,
                                AtpFtCell      **parent_out,
                                AtpFtCell      **redex_out,
                                u32             *rule_out,
                                u8              *dir_out,
                                AtpFtSubst      *subst_buf,
                                u8               try_orient) {
  // The discrim-tree descent indexes orientable rules only; if the
  // caller is doing an unorientable-only step, skip the index query.
  if (!try_orient) return 0;
  AtpFtCell *redex_cell = NULL;
  u32        rule       = 0u;
  if (!atp_ri_find_redex_ft_pub(s, root, &redex_cell, &rule)) {
    return 0;
  }
  // The discrim-tree descent already filters unorientable rules
  // (atp_ri_sync_ft_patterns skips them), so any rule we get here is
  // safe to fire forward without the ordered-equation gate.
  // Find parent by pre-order walk -- the cell whose `next` reaches
  // the redex.  O(|subject|) but limited by the linear-scan WIN above.
  AtpFtCell *parent = NULL;
  if (redex_cell != root) {
    AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
    AtpFtCell *prev = root;
    for (AtpFtCell *p = root->next; p != NULL && p != end_after;
         p = p->next) {
      if (p == redex_cell) { parent = prev; break; }
      prev = p;
    }
  }
  // Rebuild subst: ft_match against the rule's pattern.
  ft_subst_reset(subst_buf);
  if (!ft_match(s->lhs_ft[rule], redex_cell, subst_buf)) {
    // Defensive: a perfect-descent hit MUST match.  If it doesn't,
    // the rule was bwd-subsumed mid-step or the index is stale; bail
    // and let the next find_redex re-query (or the caller fall back).
    return 0;
  }
  *parent_out = parent;
  *redex_out  = redex_cell;
  *rule_out   = rule;
  *dir_out    = 0u;
  return 1;
}
#endif

// --- Position helper -------------------------------------------------
//
// Compute the child-index path from `root` to `target` via FT child-walk
// semantics: the first child of a CTR cell is `cell->next`, and the next
// sibling of a child is `child->end->next`.  Variables and consts are
// leaves.
//
// On hit, fills `pos[0..*pos_len)` and returns 1.  On miss, returns 0.
// Caller-owned `pos` of size at least ATP_PROOF_MAX_DEPTH; if the path
// would exceed the cap, returns 0 (caller can fall back to a "no
// recording" step -- but in practice ATP terms stay shallow under the
// completion cap, so the cap is informational).
static int ft_find_position(AtpFtCell *root, AtpFtCell *target,
                            u8 *pos, u8 *pos_len) {
  if (root == target) { *pos_len = 0u; return 1; }
  if (root == NULL || target == NULL) return 0;
  if ((root->sym & WF_VAR_BIT) != 0u) return 0;   // leaves don't host children
  u16 arity = root->arity;
  if (arity == 0u) return 0;
  // Recursive DFS over children.  Each child's head cell is root->next
  // for child 0, and child[i+1] = child[i]->end->next.
  AtpFtCell *child = root->next;
  for (u16 i = 0; i < arity; i++) {
    if (child == NULL) return 0;
    if (*pos_len >= ATP_PROOF_MAX_DEPTH) return 0;
    pos[*pos_len] = (u8)i;
    (*pos_len)++;
    if (ft_find_position(child, target, pos, pos_len)) return 1;
    (*pos_len)--;
    child = (child->end != NULL) ? child->end->next : NULL;
  }
  return 0;
}

// Shared internal entry: optional record-mode parameters.  When
// `record` is 0, behaves exactly like the historic `atp_rewrite_
// normalize_ft` (zero overhead -- no Term conversions, no trace
// pushes).  When `record` is 1, after each successful splice converts
// the new root via ft_to_term and emits a TRACE_NORM_STEP entry
// chained from `*chain_tail` with the rule's TRACE_ORIENT id; the
// (side, eq_other) pair matches the term-side
// atp_rewrite_normalize_record contract.
extern Term ft_to_term(const AtpFtCell *x);
static AtpFtCell *atp_rewrite_normalize_ft_impl(AtpState *s, AtpFtCell *t,
                                                u32 slice_first,
                                                u32 slice_count,
                                                u32 step_cap, int record,
                                                Term eq_other, u8 side,
                                                u32 *chain_tail);

AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap);
AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  // Full-range delegation -- bench-neutral by construction (same scan
  // bounds, same RI path, same memset).
  return atp_rewrite_normalize_ft_impl(s, t, /*slice_first=*/0u,
                                       /*slice_count=*/s->n_rules,
                                       step_cap, /*record=*/0,
                                       (Term){0}, 0u, NULL);
}

// Record-mode entry.  Mirrors atp_rewrite_normalize_record's contract
// on the term side: each FT splice emits one TRACE_NORM_STEP chained
// off `*chain_tail`, advancing the tail.  `eq_other` is the unchanged
// side of the equation we're normalizing (Term form); `side` is 0 for
// the LHS, 1 for the RHS.  The new tail is left in `*chain_tail`.
// Returns the FT NF (same allocator semantics as the bare variant).
// Always runs over the full rule range -- record-mode pairs with the
// term-side `atp_rewrite_normalize_record`, never with the slice
// interreduce path.
AtpFtCell *atp_rewrite_normalize_ft_record(AtpState *s, AtpFtCell *t,
                                           u32 step_cap, Term eq_other,
                                           u8 side, u32 *chain_tail);
AtpFtCell *atp_rewrite_normalize_ft_record(AtpState *s, AtpFtCell *t,
                                           u32 step_cap, Term eq_other,
                                           u8 side, u32 *chain_tail) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  return atp_rewrite_normalize_ft_impl(s, t, /*slice_first=*/0u,
                                       /*slice_count=*/s->n_rules,
                                       step_cap, /*record=*/1,
                                       eq_other, side, chain_tail);
}

// Slice-aware fixpoint.  Rewrites `t` against only the rule slice
// [slice_first, slice_first + slice_count) of s->lhs_ft / s->rhs_ft.
// Mirrors the Term-side `atp_proof_rewrite_step_slice` /
// `atp_rewrite_normalize_slice_record` calling convention used by
// interreduce, except this path does NOT record TRACE_NORM_STEP --
// callers (e.g. the connectedness check) that want only the FT-side
// normal form invoke this directly; norm-step recording stays on the
// Term-side path until the FT loop owns the trace.
//
// When slice_first==0 && slice_count==s->n_rules this MUST be
// bench-neutral with `atp_rewrite_normalize_ft`: the linear scan
// iterates the same range, and the public `atp_rewrite_normalize_ft`
// delegates here in that case.  (The RI/discrim-tree descent path is
// skipped on slice calls -- the rule_index reflects the FULL R, so
// a hit could resolve to an out-of-slice rule and the linear-scan
// fallback would loop.  In the full-range delegation we keep RI on.)
AtpFtCell *atp_rewrite_normalize_ft_slice(AtpState *s, AtpFtCell *t,
                                          u32 slice_first, u32 slice_count,
                                          u32 step_cap);
AtpFtCell *atp_rewrite_normalize_ft_slice(AtpState *s, AtpFtCell *t,
                                          u32 slice_first, u32 slice_count,
                                          u32 step_cap) {
  if (t == NULL) return NULL;
  if (slice_count == 0u) return t;
  if (slice_first >= s->n_rules) return t;
  if (slice_first + slice_count > s->n_rules) {
    slice_count = s->n_rules - slice_first;
  }
  return atp_rewrite_normalize_ft_impl(s, t, slice_first, slice_count,
                                       step_cap, /*record=*/0,
                                       (Term){0}, 0u, NULL);
}

static AtpFtCell *atp_rewrite_normalize_ft_impl(AtpState *s, AtpFtCell *t,
                                                u32 slice_first,
                                                u32 slice_count,
                                                u32 step_cap, int record,
                                                Term eq_other, u8 side,
                                                u32 *chain_tail) {
  if (t == NULL) return NULL;
  if (slice_count == 0u) return t;
  if (slice_first >= s->n_rules) return t;
  if (slice_first + slice_count > s->n_rules) {
    slice_count = s->n_rules - slice_first;
  }
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Entry-clear pass.
  ft_clear_subst_fresh(t);
  // Stack-allocate an AtpFtSubst.  See AtpFtSubst declaration in
  // ft_match.c; we zero before first use per the Stage 5 contract.
  AtpFtSubst subst;
  memset(&subst, 0, sizeof(subst));

  int use_full_range = (slice_first == 0u && slice_count == s->n_rules);
#ifdef THVM_ATPFT_RI
  // Stage 6b: opt-in via THVM_ATPFT_RI=1 (env knob).  Default OFF so
  // the baseline Stage 6 linear-scan path is unchanged for the default
  // build (which also has THVM_ATPFT_RI not defined at all).  The RI
  // path indexes the FULL rule set; only enable it when the slice IS
  // the full range.
  static int ri_env_cached = -1;
  if (ri_env_cached < 0) {
    const char *env = getenv("THVM_ATPFT_RI");
    ri_env_cached = (env != NULL && env[0] != '0') ? 1 : 0;
  }
  int use_ri = ri_env_cached && use_full_range;
  if (use_ri) {
    // Ensure the Term-side rule index is current; rebuild on dirty.
    if (s->rule_index == NULL || s->rule_index_dirty ||
        s->rule_index->n_rules_built != s->n_rules) {
      atp_ri_rebuild(s);
    }
    // Refresh FT pattern mirror from s->lhs_ft[].
    atp_ri_ft_sync(s);
  }
#else
  (void)use_full_range;
#endif

  // Mixed loop, mirroring the Term-side atp_rewrite_normalize_ordered
  // when n_unorient > 0:
  //   1. Run an orientable-only fixpoint (up to step_cap rewrites).
  //   2. Try ONE unorientable rewrite (KBO-gated).
  //   3. If step 2 fired, go to 1.  Otherwise we're done.
  // When n_unorient == 0 step 2 is a no-op and the loop collapses to
  // the single orientable fixpoint.  The outer step_cap caps unorient
  // rewrites; the inner cap is also step_cap, so the total orientable
  // budget is step_cap * step_cap -- matching the Term-side mixed loop
  // exactly (each outer iteration calls atp_rewrite_normalize_indexed
  // with step_cap).
  u8 have_unorient = (u8)(s->n_unorient > 0u);
  // Batched-orient fast path: when we have a full-range non-record call
  // (i.e. NOT slice-restricted and NOT in trace-recording mode), the
  // orientable fixpoint can be run by the Term-side discrim-tree
  // normalizer (atp_rewrite_normalize_indexed) in ONE batched call.
  // That path keeps the subject flat across many rewrites; here we pay
  // a single ft_to_term/ft_from_term round-trip per outer iteration
  // instead of one ft_match per cell per rule per inner step.
  //
  // Opt-in via THVM_ATPFT_BATCH_ORIENT=1.  When the slice path is in
  // use, or when recording, we fall back to the per-step inner loop
  // (the indexed path would rewrite against the full R, not the
  // slice, and it does not emit TRACE_NORM_STEP entries).
  static int batch_env_cached = -1;
  if (batch_env_cached < 0) {
    const char *env = getenv("THVM_ATPFT_BATCH_ORIENT");
    batch_env_cached = (env != NULL && env[0] != '0' && env[0] != '\0') ? 1 : 0;
  }
  int use_batch_orient = batch_env_cached && use_full_range && !record;
  (void)use_batch_orient;  // legacy batched-orient hint; per-position loop below subsumes it
  // Single per-position try-all-rules loop (matches WM `BL_Normalform*` and
  // Term-side `atp_ordered_try_top` -- both try R + E AT EACH POSITION in
  // one pass instead of running orient to fixpoint before unorient).
  // The prior split-phase (orient-fixpoint, ONE unorient, repeat) produced
  // different NFs from Term/WM on AC-class workloads; blocked migrations
  // of goal_check + CP-set-IR.
  for (u32 i = 0; i < step_cap; i++) {
    AtpFtCell *parent = NULL;
    AtpFtCell *redex  = NULL;
    u32        rule   = 0u;
    u8         dir    = 0u;
    int hit = 0;
#ifdef THVM_ATPFT_RI
    if (use_ri) {
      hit = find_redex_ft_via_ri(s, t, &parent, &redex, &rule, &dir,
                                 &subst, /*try_orient=*/1u);
      if (!hit) {
        hit = find_redex_ft(s, t, slice_first, slice_count,
                            &parent, &redex, &rule, &dir, &subst,
                            /*try_orient=*/1u, /*try_unorient=*/have_unorient);
      }
    } else {
      hit = find_redex_ft(s, t, slice_first, slice_count,
                          &parent, &redex, &rule, &dir, &subst,
                          /*try_orient=*/1u, /*try_unorient=*/have_unorient);
    }
#else
    hit = find_redex_ft(s, t, slice_first, slice_count,
                        &parent, &redex, &rule, &dir, &subst,
                        /*try_orient=*/1u, /*try_unorient=*/have_unorient);
#endif
    if (!hit) break;
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u8  pos_len  = 0u;
    int have_pos = 0;
    if (record) {
      have_pos = ft_find_position(t, redex, pos, &pos_len);
    }
    AtpFtCell *rhs_tmpl = (dir == 0u) ? s->rhs_ft[rule] : s->lhs_ft[rule];
    AtpFtCell *new_root = ft_splice(a, t, parent, redex, rhs_tmpl, &subst);
    if (new_root != NULL) t = new_root;
    if (record) {
      Term step_term = ft_to_term(t);
      u8  fwd        = (dir == 0u) ? 1u : 0u;
      u32 rule_trace = (rule < s->n_rules) ? s->r_trace[rule]
                                           : ATP_TRACE_NONE;
      Term step_lhs = (side == 0u) ? step_term : eq_other;
      Term step_rhs = (side == 0u) ? eq_other  : step_term;
      if (!have_pos) pos_len = 0u;
      u32 ti = atp_trace_push_norm_step(s, *chain_tail, rule_trace,
                                        step_lhs, step_rhs, side, fwd,
                                        pos, pos_len);
      if (ti != ATP_TRACE_NONE) *chain_tail = ti;
    }
  }
  return t;
}

#endif // THVM_ATPFT_NORM
