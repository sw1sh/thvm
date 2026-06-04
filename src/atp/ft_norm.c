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
// SUBST_FRESH bit is clear, try every rule's LHS.  If a rule matches,
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
static int find_redex_ft(AtpState        *s,
                         AtpFtCell       *root,
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
    for (u32 r = 0; r < s->n_rules; r++) {
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
        if (ft_match(lhs, p, subst_buf)) {
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
      // and the instantiated repl is strictly less than the redex under
      // the reduction order.
      if (ft_vars_contained(rhs, lhs)) {
        ft_subst_reset(subst_buf);
        if (ft_match(lhs, p, subst_buf)) {
          // KBO gate via Term round-trip: build a Term mirror of p and
          // the instantiated RHS, compare.  Correct + simple baseline.
          AtpFt *a = (AtpFt *)s->ft_arena_ptr;
          AtpFtCell *repl_ft = ft_subst_apply(a, rhs, subst_buf, 0);
          if (repl_ft != NULL) {
            Term t_term  = ft_to_term(p);
            Term r_term  = ft_to_term(repl_ft);
            if (atp_compare(s, t_term, r_term) == KBO_GT) {
              *parent_out = (p == root) ? NULL : prev;
              *redex_out  = p;
              *rule_out   = r;
              *dir_out    = 0u;
              return 1;
            }
          }
          ft_subst_reset(subst_buf);
        }
      }
      // Backward: same gates with l/r swapped.
      if (ft_vars_contained(lhs, rhs)) {
        ft_subst_reset(subst_buf);
        if (ft_match(rhs, p, subst_buf)) {
          AtpFt *a = (AtpFt *)s->ft_arena_ptr;
          AtpFtCell *repl_ft = ft_subst_apply(a, lhs, subst_buf, 0);
          if (repl_ft != NULL) {
            Term t_term  = ft_to_term(p);
            Term r_term  = ft_to_term(repl_ft);
            if (atp_compare(s, t_term, r_term) == KBO_GT) {
              *parent_out = (p == root) ? NULL : prev;
              *redex_out  = p;
              *rule_out   = r;
              *dir_out    = 1u;
              return 1;
            }
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

AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap);
AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Entry-clear pass.
  ft_clear_subst_fresh(t);
  // Stack-allocate an AtpFtSubst.  We use a u8 buffer sized to hold a
  // full AtpFtSubst -- the actual type is defined in ft_match.c and
  // sized by ATPFT_MAX_VARS=64.  sizeof(AtpFtSubst) at this TU sees
  // the full definition because ft_norm.c is included AFTER ft_match.c
  // in every consumer TU.
  AtpFtSubst subst;
  // Zero-init: AtpFtSubst's `bind[]` must be all NULL on entry, `wm`
  // must be 0.  Stage 5's contract: callers MUST zero the struct
  // before first use.
  memset(&subst, 0, sizeof(subst));

#ifdef THVM_ATPFT_RI
  // Stage 6b: opt-in via THVM_ATPFT_RI=1 (env knob).  Default OFF so
  // the baseline Stage 6 linear-scan path is unchanged for the default
  // build (which also has THVM_ATPFT_RI not defined at all).
  static int ri_env_cached = -1;
  if (ri_env_cached < 0) {
    const char *env = getenv("THVM_ATPFT_RI");
    ri_env_cached = (env != NULL && env[0] != '0') ? 1 : 0;
  }
  int use_ri = ri_env_cached;
  if (use_ri) {
    // Ensure the Term-side rule index is current; rebuild on dirty.
    if (s->rule_index == NULL || s->rule_index_dirty ||
        s->rule_index->n_rules_built != s->n_rules) {
      atp_ri_rebuild(s);
    }
    // Refresh FT pattern mirror from s->lhs_ft[].
    atp_ri_ft_sync(s);
  }
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
  for (u32 outer = 0; outer < step_cap; outer++) {
    // Step 1: orientable fixpoint.
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
          hit = find_redex_ft(s, t, &parent, &redex, &rule, &dir, &subst,
                              /*try_orient=*/1u, /*try_unorient=*/0u);
        }
      } else {
        hit = find_redex_ft(s, t, &parent, &redex, &rule, &dir, &subst,
                            /*try_orient=*/1u, /*try_unorient=*/0u);
      }
#else
      hit = find_redex_ft(s, t, &parent, &redex, &rule, &dir, &subst,
                          /*try_orient=*/1u, /*try_unorient=*/0u);
#endif
      if (!hit) break;
      AtpFtCell *rhs_tmpl = (dir == 0u) ? s->rhs_ft[rule] : s->lhs_ft[rule];
      AtpFtCell *new_root = ft_splice(a, t, parent, redex, rhs_tmpl, &subst);
      if (new_root != NULL) t = new_root;
    }
    if (!have_unorient) break;
    // Step 2: one unorient step.
    AtpFtCell *parent = NULL;
    AtpFtCell *redex  = NULL;
    u32        rule   = 0u;
    u8         dir    = 0u;
    int hit = find_redex_ft(s, t, &parent, &redex, &rule, &dir, &subst,
                            /*try_orient=*/0u, /*try_unorient=*/1u);
    if (!hit) break;                  // joint fixpoint -- done.
    AtpFtCell *rhs_tmpl = (dir == 0u) ? s->rhs_ft[rule] : s->lhs_ft[rule];
    AtpFtCell *new_root = ft_splice(a, t, parent, redex, rhs_tmpl, &subst);
    if (new_root != NULL) t = new_root;
  }
  return t;
}

#endif // THVM_ATPFT_NORM
