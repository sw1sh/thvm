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
// The full-range entry point (`atp_rewrite_normalize_ft`) passes
// slice_first=0 / slice_count=s->n_rules; the slice entry point
// (`atp_rewrite_normalize_ft_slice`) passes the caller's [first,count)
// directly.  Iterating only the slice is the whole point of the slice
// path -- interreduce wants to rewrite an EXISTING rule against the
// just-added rules, NOT against itself (would loop) or against the
// older rules (those gave it CPs; rewriting through them is a no-op
// since the rule is already in normal form w.r.t. the older R).
static int find_redex_ft(AtpState        *s,
                         AtpFtCell       *root,
                         u32              slice_first,
                         u32              slice_count,
                         AtpFtCell      **parent_out,
                         AtpFtCell      **redex_out,
                         u32             *rule_out,
                         AtpFtSubst      *subst_buf) {
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  AtpFtCell *prev = NULL;
  u32 slice_end = slice_first + slice_count;
  if (slice_end > s->n_rules) slice_end = s->n_rules;
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
    for (u32 r = slice_first; r < slice_end; r++) {
      AtpFtCell *lhs = s->lhs_ft[r];
      if (lhs == NULL) continue;
      ft_subst_reset(subst_buf);
      if (ft_match(lhs, p, subst_buf)) {
        *parent_out = (p == root) ? NULL : prev;
        *redex_out  = p;
        *rule_out   = r;
        return 1;
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
                                AtpFtSubst      *subst_buf) {
  AtpFtCell *redex_cell = NULL;
  u32        rule       = 0u;
  if (!atp_ri_find_redex_ft_pub(s, root, &redex_cell, &rule)) {
    return 0;
  }
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
  return 1;
}
#endif

AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap);
AtpFtCell *atp_rewrite_normalize_ft_slice(AtpState *s, AtpFtCell *t,
                                          u32 slice_first, u32 slice_count,
                                          u32 step_cap);

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
                                          u32 step_cap) {
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

  for (u32 i = 0; i < step_cap; i++) {
    AtpFtCell *parent = NULL;
    AtpFtCell *redex  = NULL;
    u32        rule   = 0u;
    int hit = 0;
#ifdef THVM_ATPFT_RI
    if (use_ri) {
      hit = find_redex_ft_via_ri(s, t, &parent, &redex, &rule, &subst);
      // Folded rule-index or stale: fall back to linear scan THIS step.
      if (!hit) hit = find_redex_ft(s, t, slice_first, slice_count,
                                    &parent, &redex, &rule, &subst);
    } else {
      hit = find_redex_ft(s, t, slice_first, slice_count,
                          &parent, &redex, &rule, &subst);
    }
#else
    hit = find_redex_ft(s, t, slice_first, slice_count,
                        &parent, &redex, &rule, &subst);
#endif
    if (!hit) break;
    AtpFtCell *rhs_tmpl = s->rhs_ft[rule];
    AtpFtCell *new_root = ft_splice(a, t, parent, redex, rhs_tmpl, &subst);
    if (new_root != NULL) t = new_root;
  }
  return t;
}

AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  // Full-range delegation -- bench-neutral by construction (same scan
  // bounds, same RI path, same memset).
  return atp_rewrite_normalize_ft_slice(s, t, 0u, s->n_rules, step_cap);
}

#endif // THVM_ATPFT_NORM
