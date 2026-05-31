// ft_norm.c - Stage 6 of AtpFt: AtpFt-native innermost-rewrite
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
static int find_redex_ft(AtpState        *s,
                         AtpFtCell       *root,
                         AtpFtCell      **parent_out,
                         AtpFtCell      **redex_out,
                         u32             *rule_out,
                         AtpFtSubst      *subst_buf) {
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  AtpFtCell *prev = NULL;
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
  for (u32 i = 0; i < step_cap; i++) {
    AtpFtCell *parent = NULL;
    AtpFtCell *redex  = NULL;
    u32        rule   = 0u;
    if (!find_redex_ft(s, t, &parent, &redex, &rule, &subst)) {
      break;
    }
    AtpFtCell *rhs_tmpl = s->rhs_ft[rule];
    AtpFtCell *new_root = ft_splice(a, t, parent, redex, rhs_tmpl, &subst);
    if (new_root != NULL) t = new_root;
  }
  return t;
}

#endif // THVM_ATPFT_NORM
