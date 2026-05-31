// ft_match.c - Stage 5 of AtpFt: AtpFt-native one-way matching +
// substitution-apply.
//
// Stage 5 lifts the Stage-3 (rewrite/_.c) `thvm_match` / `thvm_subst_apply`
// pair onto AtpFt cells.  Goal: equivalent behavior to thvm_match on
// the heap-cell Term path -- same boolean verdict, same recorded
// bindings -- while reading exclusively through the flat
// `next` / `end` / `sym` / `arity` cell layout (so callers can wire
// the AtpFt-native rule-index descent without the per-call
// AtpFtCell -> Term encode that today's bridge would force).
//
// API shape (mirrors docs/atp/atpft_plan.md):
//
//   typedef struct AtpFtSubst {
//     AtpFtCell *bind[ATPFT_MAX_VARS];   // NULL = unbound
//     u32        wm;                      // watermark = count of bound ids
//     u32        bound_ids[ATPFT_MAX_VARS];  // ids set in order
//   } AtpFtSubst;
//
//   void       ft_subst_reset(AtpFtSubst *s);
//   int        ft_match     (const AtpFtCell *pat, const AtpFtCell *subj,
//                            AtpFtSubst *out);
//   AtpFtCell *ft_subst_apply(AtpFt *a, const AtpFtCell *tmpl,
//                             const AtpFtSubst *s, int scratch);
//
// Differences from rewrite/_.c thvm_match:
//   * The dense REWRITE_MAX_VAR=64 slot table is identical (we reuse
//     ATPFT_MAX_VARS=64 by definition), but Stage 5 ALSO maintains a
//     watermark + `bound_ids[]` list so the matcher restores partial
//     bindings on a child mismatch instead of leaving the caller to
//     zero the table.  This is the WM-style backtracking discipline
//     (waldmeister/sources/INF/MatchOperationen.c:127-128 -- the
//     `xMletztes` slot in WM's xM table).  Callers can stack subst
//     state by saving `wm` (and restoring it on failure manually) for
//     a future nested-match path; the entry-level `ft_match` returns
//     a subst restored to the entry watermark on its own failure path,
//     so a caller who hands in an empty subst gets an empty subst back
//     on failure.
//   * Repeated-var consistency goes through `ft_eq` (subtree-structural
//     equality) -- equivalent to `kbo_eq` on the Term side, but does
//     not synthesize a Term.
//   * `ft_subst_apply` deep-copies the bound subterm at every var
//     occurrence.  Stage 6 splice will replace this with a zero-copy
//     splice; Stage 5's deep copy is the "obviously correct" baseline
//     against which the splice is differential-tested.
//
// Gated on THVM_ATPFT_MATCH.  This is purely additive in Stage 5: no
// live caller in src/atp/_.c wires through ft_match yet (per
// docs/atp/atpft_plan.md, that wiring lands in Stage 5b / 6 to keep
// the Stage 5 commit risk-bounded).

#ifdef THVM_ATPFT_MATCH

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"   // WF_VAR_BIT -- same encoding as ft.c

// --- Var-bit helpers --------------------------------------------------
//
// ft.c keeps the same accessor names (ft_is_var, ft_var_id, ft_ctr_sym)
// as `static inline` in its TU.  In the single-TU build the test
// includes ft.c BEFORE ft_match.c (Stage 2 lands the converters); the
// helpers are then in scope under the file-static `static inline`
// linkage and reusing them is byte-identical to redefining.  For the
// avoidance of "implicit declaration" warnings if ft_match.c is ever
// pulled into a TU that does not include ft.c (currently impossible
// because THVM_ATPFT_MATCH requires THVM_ATPFT_CONVERT per the
// Makefile rule) we keep our own local copies here under a different
// name -- ft_m_*.  No ABI cost.
static inline int  ft_m_is_var (const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) != 0u;
}
static inline u32  ft_m_var_id (const AtpFtCell *c) {
  return c->sym & ~WF_VAR_BIT;
}

// --- Subst storage ----------------------------------------------------
//
// ATPFT_MAX_VARS matches REWRITE_MAX_VAR (src/thvm.h:3084) so an
// AtpFtSubst is exactly the same "address space" as a RewriteSubst.
// All slot ids beyond this cap reject at match time -- mirrors
// rewrite/_.c:31.
#define ATPFT_MAX_VARS 64u

typedef struct AtpFtSubst {
  AtpFtCell *bind[ATPFT_MAX_VARS];
  u32        wm;
  u32        bound_ids[ATPFT_MAX_VARS];
} AtpFtSubst;

// Reset for reuse.  We zero only the slots that the previous match
// touched -- watermark-walks through `bound_ids[0..wm)`, clears each
// `bind[id]` back to NULL, then drops the watermark.  This keeps the
// invariant "id is bound iff bind[id] != NULL" so the matcher's
// repeated-var consistency check can read bind[id] directly.
//
// O(wm), where wm is the number of variables bound by the previous
// match (typically a handful for any plausible rule -- WM's xMletztes
// follows the same shape).  Strictly cheaper than the dense
// REWRITE_MAX_VAR=64 zero-pass.
//
// First-use note: a freshly stack-alloc'd AtpFtSubst has indeterminate
// `bind[]` and `wm`.  Callers MUST zero-init the struct (e.g.
// `AtpFtSubst s = {0};`) before the FIRST ft_subst_reset / ft_match
// call -- subsequent reuse goes through this reset which preserves
// the invariant.
void ft_subst_reset(AtpFtSubst *s) {
  while (s->wm > 0u) {
    s->wm -= 1u;
    u32 id = s->bound_ids[s->wm];
    s->bind[id] = NULL;
  }
}

// Internal: rewind to a saved watermark.  Walks the trailing
// `bound_ids[wm_target..s->wm)` slice and clears each `bind[id]` back
// to NULL, then sets `s->wm = wm_target`.  Used on a child-match
// failure to undo bindings introduced by the current attempt without
// disturbing bindings the caller installed before calling us.
static void ft_subst_rewind(AtpFtSubst *s, u32 wm_target) {
  while (s->wm > wm_target) {
    s->wm -= 1u;
    u32 id = s->bound_ids[s->wm];
    s->bind[id] = NULL;
  }
}

// --- ft_match ---------------------------------------------------------
//
// Recursive descent over `pat` walked against `subj`.  Behavior on
// each cell tag:
//
//   * pat var (high-bit set on sym):
//     - var id >= ATPFT_MAX_VARS -> fail.
//     - bind[id] == NULL: install bind[id] = subj, append id to
//       bound_ids[wm], increment wm; succeed.
//     - bind[id] != NULL: succeed iff ft_eq(bind[id], subj).
//
//   * pat CTR:
//     - subj must also be CTR (not var); pat->sym == subj->sym;
//       pat->arity == subj->arity.  Then walk children via the
//       AtpFt sibling stride `child = child->end->next`, recursing
//       on each (pat_child, subj_child) pair.
//
// On any failure, the watermark is rewound to whatever it was at
// entry to THIS call -- so a deep child mismatch unwinds its own
// bindings but leaves the caller's pre-existing bindings intact.
// The outer ft_match entry point sees this for free because its
// own "entry watermark" is 0 for a fresh AtpFtSubst (or whatever
// the caller staged it at, for a nested match).
int ft_match(const AtpFtCell *pat, const AtpFtCell *subj, AtpFtSubst *out) {
  if (pat == NULL || subj == NULL) return 0;

  // Variable pattern: bind or check consistency.
  if (ft_m_is_var(pat)) {
    u32 id = ft_m_var_id(pat);
    if (id >= ATPFT_MAX_VARS) return 0;
    AtpFtCell *prev = out->bind[id];
    if (prev == NULL) {
      out->bind[id]                = (AtpFtCell *)subj;
      out->bound_ids[out->wm]      = id;
      out->wm                     += 1u;
      return 1;
    }
    // Repeated-var consistency check -- structural equality, matches
    // rewrite/_.c:36 (kbo_eq on the Term side).
    return ft_eq(prev, subj);
  }

  // CTR pattern: subj must be a same-shape CTR.
  if (ft_m_is_var(subj)) return 0;          // pat is CTR, subj is var -> fail
  if (pat->sym != subj->sym)   return 0;
  if (pat->arity != subj->arity) return 0;

  // Walk children via the sibling stride.  Save the entry watermark
  // so a deep failure rewinds the bindings we introduced here.
  u32 wm_save = out->wm;
  u16 arity   = pat->arity;
  const AtpFtCell *pc = pat->next;
  const AtpFtCell *sc = subj->next;
  for (u16 i = 0; i < arity; i++) {
    if (!ft_match(pc, sc, out)) {
      ft_subst_rewind(out, wm_save);
      return 0;
    }
    pc = pc->end->next;
    sc = sc->end->next;
  }
  return 1;
}

// --- ft_subst_apply ---------------------------------------------------
//
// Recursive construction of a fresh AtpFt term mirroring `tmpl` with
// every variable replaced by its binding from `s`.
//
//   * tmpl var: look up bind[id]; if NULL, return NULL (defensive --
//     a valid CP-RHS template should only mention vars bound by the
//     LHS match, so the NULL path should be unreachable on the live
//     path but is signalled explicitly here).  Otherwise deep-copy
//     the binding into the destination arena (no aliasing back into
//     Arena A from a scratch result, and vice versa).
//
//   * tmpl CTR(sym, c0..c_{n-1}): recurse on each child, then stitch
//     via ftnew_ctr.  The arity is bounded by AtpFt's REWRITE_MAX_ARITY
//     analog (we use the same 16-cap in scratch/persistent paths;
//     a CTR with arity > 16 cannot be expressed in a RewriteSubst-
//     compatible flow anyway -- thvm_subst_apply enforces the same
//     bound at rewrite/_.c:80).
//
// scratch flag is threaded through every allocation so the returned
// term lives in a single arena (caller chooses).
#define ATPFT_APPLY_KIDS_STACK 16u

// Forward decl for the mutually-recursive deep-copy + apply.
static AtpFtCell *ft_deep_copy_rec(AtpFt *a, const AtpFtCell *src, int scratch);

// Deep copy: rebuild `src` cell-for-cell into the destination arena.
// Variables stay variables (they're real subterm contents now -- a
// bound subterm in a CP-substitution is whatever was matched against,
// which may itself contain free variables).
static AtpFtCell *ft_deep_copy_rec(AtpFt *a, const AtpFtCell *src, int scratch) {
  if (src == NULL) return NULL;
  if (ft_m_is_var(src)) {
    return ftnew_var(a, ft_m_var_id(src), scratch);
  }
  u32 sym   = src->sym;
  u16 arity = src->arity;
  if (arity == 0u) {
    return ftnew_const(a, sym, scratch);
  }
  AtpFtCell *stack_kids[ATPFT_APPLY_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > ATPFT_APPLY_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_subst_apply/deep_copy: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = src->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ft_deep_copy_rec(a, child, scratch);
    child   = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

static AtpFtCell *ft_subst_apply_rec(AtpFt *a, const AtpFtCell *tmpl,
                                     const AtpFtSubst *s, int scratch) {
  if (tmpl == NULL) return NULL;
  if (ft_m_is_var(tmpl)) {
    u32 id = ft_m_var_id(tmpl);
    if (id >= ATPFT_MAX_VARS) return NULL;
    AtpFtCell *binding = s->bind[id];
    if (binding == NULL) return NULL;
    return ft_deep_copy_rec(a, binding, scratch);
  }
  u32 sym   = tmpl->sym;
  u16 arity = tmpl->arity;
  if (arity == 0u) {
    return ftnew_const(a, sym, scratch);
  }
  AtpFtCell *stack_kids[ATPFT_APPLY_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > ATPFT_APPLY_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_subst_apply: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = tmpl->next;
  for (u16 i = 0; i < arity; i++) {
    AtpFtCell *kid = ft_subst_apply_rec(a, child, s, scratch);
    if (kid == NULL) {
      // Bubble the NULL up -- defensive bail-out for an unbound var
      // referenced by tmpl.  Frees the heap kid buffer if used.
      free(heap_kids);
      return NULL;
    }
    kids[i] = kid;
    child   = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

AtpFtCell *ft_subst_apply(AtpFt *a, const AtpFtCell *tmpl,
                          const AtpFtSubst *s, int scratch) {
  return ft_subst_apply_rec(a, tmpl, s, scratch);
}

// --- Type-erased accessors for Stage 6 splice ------------------------
//
// ft_splice.c only sees `const void *subst` so it doesn't take a hard
// build-time dep on AtpFtSubst's layout.  These two helpers are the
// thin bridge: id -> binding pointer, and the slot-table cap.
//
// Returns NULL when id is out of range or unbound -- the splice
// translates either to "regime (a) unbound-var" or treats it as a
// defensive failure (callers should never feed an out-of-range id).
AtpFtCell *ft_subst_lookup(const void *subst, u32 var_id) {
  const AtpFtSubst *s = (const AtpFtSubst *)subst;
  if (s == NULL) return NULL;
  if (var_id >= ATPFT_MAX_VARS) return NULL;
  return s->bind[var_id];
}
u32 ft_subst_max_vars(void) {
  return ATPFT_MAX_VARS;
}

#endif // THVM_ATPFT_MATCH
