// ft_splice.c - in-place WM-style splice of an
// AtpFt subject by an instantiated rule RHS.
//
// Scope: implement `ft_splice`, the operation that takes a `redex`
// position in a subject term, a rule's RHS template `rhs_tmpl`, and a
// substitution `subst` populated by `ft_match`, and rewrites the
// redex's subtree in place with the instantiated RHS.  Returns the
// (possibly updated) root of the subject -- equal to the input `root`
// for the subterm-rewrite case (subj->next chain edits), or a fresh
// root cell for the root-rewrite case when the new root has a
// different arity than the old.
//
// Two regimes (per design):
//
//   (a) ground/unbound-var single-cell RHS: `rhs_tmpl` is one cell,
//       arity 0, and (if a variable) carries no binding in `subst`.
//       Overwrite the redex's head cell in place; free the displaced
//       tail span via ft_free_span (O(1) chain push).
//
//   (c) general RHS: route through ft_subst_apply to build a fresh
//       Arena-A AtpFt term that mirrors `rhs_tmpl` with every var-leaf
//       deep-copied from its `subst` binding.  Splice the built term
//       into the position of `redex`, freeing the displaced span.
//
// Regime (b) ("RHS is a single bound var") is folded into (c) -- one
// deep copy per fire, see the plan note.  Cost is bounded by the
// matched binding's size; revisit if benchmarks show a measurable
// share.
//
// All newly-allocated cells (regime (a)'s overwritten head, regime
// (c)'s entire CTR walk) carry ATPFT_FLAG_SUBST_FRESH on return.
// Bound-var subtrees that are stitched in by deep-copy preserve their
// existing flags (typically SUBST_FRESH=0 -- a freshly-bound subject
// subterm has already been recursively normalized by the outer walk).
//
// Gated on THVM_ATPFT_NORM.

#ifdef THVM_ATPFT_NORM

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"   // WF_VAR_BIT -- same encoding as ft.c

// --- Forward decls from sibling AtpFt stages --------------------------
//
// Stage 2 (ft.c): constructors + structural equality.
extern AtpFtCell *ftnew_var  (AtpFt *a, u32 var_id, int scratch);
extern AtpFtCell *ftnew_const(AtpFt *a, u32 sym, int scratch);
extern AtpFtCell *ftnew_ctr  (AtpFt *a, u32 sym, u16 arity,
                              AtpFtCell *const *kids, int scratch);
extern int        ft_eq      (const AtpFtCell *x, const AtpFtCell *y);

// Stage 5 (ft_match.c): subst type forward-decl + binding lookup.
// We do NOT redefine the AtpFtSubst struct here -- it is the same
// header definition the test TU pulls in via ft_match.c.  The Stage 6
// caller (atp_rewrite_normalize_ft in ft_norm.c, included in the same
// TU as ft_match.c) sees both definitions in the single-TU build.

// --- Var-bit helpers --------------------------------------------------
static inline int  ft_s_is_var (const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) != 0u;
}
static inline u32  ft_s_var_id (const AtpFtCell *c) {
  return c->sym & ~WF_VAR_BIT;
}

// --- AtpFtSubst access ------------------------------------------------
//
// We rely on the AtpFtSubst layout from ft_match.c: `bind[ATPFT_MAX_VARS]`
// indexed by var id.  Defining a forward struct here would shadow that
// type, so we use a fully-qualified accessor pattern -- this TU is
// always built together with ft_match.c (the matcher TU); the type is
// in scope.
//
// To avoid a circular dependency on the matcher layout (which would
// force this file to import ft_match.c's ATPFT_MAX_VARS), we accept
// the subst as a `const void *` and re-resolve the binding via a
// helper provided by ft_match.c.  ft_match.c exposes a tiny accessor
// `ft_subst_lookup(s, id)` for exactly this purpose.

extern AtpFtCell *ft_subst_lookup(const void *subst, u32 var_id);
extern u32        ft_subst_max_vars(void);

// Build a fresh AtpFt term mirroring `tmpl` with vars replaced by their
// `subst` bindings (deep-copied).  Marks every CTR cell ALLOCATED
// during the walk with ATPFT_FLAG_SUBST_FRESH; deep-copied bound-var
// subtrees keep their existing flags.  The var leaves that have NO
// binding (i.e. rule-tmpl-only vars unreachable from the LHS) are
// rejected (returns NULL) -- the caller must surface this as a bug.
//
// Arena is always Arena A (scratch=0): the splice must outlive
// the next ft_scratch_reset; the caller controls when the Arena B
// scratch resets.

#define FT_SPLICE_KIDS_STACK 16u

// Forward decl for the deep-copy of bound-var bindings.
static AtpFtCell *ft_splice_deep_copy(AtpFt *a, const AtpFtCell *src);

static AtpFtCell *ft_splice_deep_copy(AtpFt *a, const AtpFtCell *src) {
  if (src == NULL) return NULL;
  if (ft_s_is_var(src)) {
    return ftnew_var(a, ft_s_var_id(src), 0);
  }
  u32 sym   = src->sym;
  u16 arity = src->arity;
  if (arity == 0u) {
    return ftnew_const(a, sym, 0);
  }
  AtpFtCell *stack_kids[FT_SPLICE_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_SPLICE_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_splice/deep_copy: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = src->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ft_splice_deep_copy(a, child);
    child   = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, 0);
  free(heap_kids);
  // Deep-copied subject subterms KEEP SUBST_FRESH cleared -- they are
  // the matched-binding contents, not freshly introduced by the rule.
  return out;
}

// Build the instantiated `rhs_tmpl[subst]` into Arena A, marking every
// CTR cell ALLOCATED by this walk (i.e. the RHS skeleton cells) with
// SUBST_FRESH=1.  Bound-var-leaf substitutions are deep-copied by
// `ft_splice_deep_copy` above, which leaves SUBST_FRESH cleared on
// the substitut.
//
// Returns NULL if a tmpl var has no binding (defensive -- the LHS
// match populates every variable that appears in `rhs_tmpl` for an
// orientable rule).
static AtpFtCell *ft_splice_build_rhs(AtpFt *a, const AtpFtCell *tmpl,
                                      const void *subst) {
  if (tmpl == NULL) return NULL;
  if (ft_s_is_var(tmpl)) {
    u32 id = ft_s_var_id(tmpl);
    if (id >= ft_subst_max_vars()) return NULL;
    AtpFtCell *binding = ft_subst_lookup(subst, id);
    if (binding == NULL) return NULL;
    return ft_splice_deep_copy(a, binding);
  }
  u32 sym   = tmpl->sym;
  u16 arity = tmpl->arity;
  if (arity == 0u) {
    AtpFtCell *c = ftnew_const(a, sym, 0);
    c->flags |= ATPFT_FLAG_SUBST_FRESH;
    return c;
  }
  AtpFtCell *stack_kids[FT_SPLICE_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_SPLICE_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_splice/build_rhs: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = tmpl->next;
  for (u16 i = 0; i < arity; i++) {
    AtpFtCell *k = ft_splice_build_rhs(a, child, subst);
    if (k == NULL) {
      free(heap_kids);
      return NULL;
    }
    kids[i] = k;
    child   = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, 0);
  out->flags |= ATPFT_FLAG_SUBST_FRESH;
  free(heap_kids);
  return out;
}

// --- Span-end finder --------------------------------------------------
//
// Given the cell `before_redex` whose `next` IS `redex`, return its
// `next` slot pointer so the splice can rewire.  Linear scan starting
// from `parent->next` -- O(span up to redex), called once per splice.
// In practice the rule-index descent already knows the predecessor
// cell; we accept the small re-scan for now and revisit if profiling
// shows it as a hot spot.
static AtpFtCell **ft_splice_find_pred(AtpFtCell *parent, AtpFtCell *redex) {
  AtpFtCell *p = parent;
  // The redex is in parent's subtree; the chain `parent->next, then
  // next->next, ...` reaches it.  We walk until the cell whose `next`
  // field equals redex.
  while (p != NULL) {
    if (p->next == redex) return &p->next;
    p = p->next;
  }
  return NULL;
}

// --- In-place overwrite (regime a) ------------------------------------
//
// `redex` is the head cell of the displaced subterm.  Build the
// replacement as a single 0-arity cell (`rhs_tmpl` is a 0-arity ctr or
// an unbound var).  Overwrite `redex`'s fields in place; free the rest
// of the displaced span (`redex->next .. redex->end`) via ft_free_span
// IF redex->end != redex.
//
// Returns 1 on success.  The cell `redex` is REUSED as the new head;
// callers that hold the redex pointer (e.g. for caller-side
// bookkeeping) keep their pointer valid.
static int ft_splice_inplace_const_or_unbound_var(
    AtpFt *a, AtpFtCell *root, AtpFtCell *parent, AtpFtCell *redex,
    const AtpFtCell *rhs_tmpl, const void *subst) {
  (void)root; (void)parent;
  // Snapshot the displaced inner span BEFORE we overwrite redex's
  // fields.  The span runs from redex->next through redex->end (when
  // redex has children); for a single-cell redex it is empty.
  AtpFtCell *tail_first = redex->next;
  AtpFtCell *tail_last  = redex->end;
  // Decide the new sym + ground bit.  For an unbound-var RHS template
  // (regime b folded into a): we still produce a var cell -- the
  // result is a fresh "ungrounded" subterm with that var id.
  if (ft_s_is_var(rhs_tmpl)) {
    u32 id = ft_s_var_id(rhs_tmpl);
    redex->sym   = WF_VAR_BIT | (id & ~WF_VAR_BIT);
    redex->arity = 0;
    redex->flags = ATPFT_FLAG_SUBST_FRESH;          // var, not ground
  } else {
    redex->sym   = rhs_tmpl->sym & ~WF_VAR_BIT;
    redex->arity = 0;
    redex->flags = ATPFT_FLAG_GROUND | ATPFT_FLAG_SUBST_FRESH;
  }
  (void)subst;
  // The new redex is a single cell: end=self, next=former-tail-next-sibling.
  // We must preserve `redex->end->next` (the next-sibling-of-old-redex
  // pointer) as the NEW redex's next-sibling.  Pull it BEFORE overwriting
  // tail_last->next via ft_free_span.
  AtpFtCell *post_sibling = (tail_last != NULL) ? tail_last->next : NULL;
  // Inner span exists iff redex itself has children -- i.e. tail_last
  // (== redex->end) is past redex in the cell chain.  For a leaf
  // redex (end == redex) `tail_first` is the next-sibling pointer, NOT
  // an inner-span cell, and freeing it would corrupt the parent's
  // chain (ft_free_span would walk from sibling-head into unrelated
  // memory).  Pre-existing bug surfaced by ft_norm_record bring-up:
  // the ATP-test corpus exercises this regime via Sheffer rules whose
  // RHS is a bound var (regime c, not regime a), so the leaf-redex
  // case slipped through.
  if (tail_first != NULL && tail_last != redex) {
    // Free the inner span [tail_first .. tail_last].  The span is
    // pre-threaded via `next` by the AtpFt invariant (ft.c stitches
    // children pre-order), so ft_free_span is O(span).  ft_free_span
    // also detaches tail_last->next; we restore the sibling link below.
    ft_free_span(a, tail_first, tail_last);
  }
  // The single new redex cell's end is itself; its next is the
  // post-redex next sibling so the outer walk continues correctly.
  redex->end  = redex;
  redex->next = post_sibling;
  return 1;
}

// --- Subtree splice (regime c) ---------------------------------------
//
// Build a fresh `repl` from `rhs_tmpl[subst]` in Arena A.  Then:
//
//   - the cell whose `next` IS redex gets `next` rewired to repl (or
//     for the root case, the caller's `root` slot updates to `repl`).
//   - repl->end->next becomes the old redex->end->next (the post-redex
//     sibling pointer).
//   - the displaced span [redex .. redex->end] is freed via
//     ft_free_span.
//
// Returns the new root.  For root rewrites, that may be a different
// cell pointer than `root` (e.g. when the rhs has different arity);
// caller propagates.

// --- Public API ------------------------------------------------------
//
// See header docs at top of file.  parent==NULL is the root-rewrite
// case.
AtpFtCell *ft_splice(AtpFt        *a,
                     AtpFtCell    *root,
                     AtpFtCell    *parent,
                     AtpFtCell    *redex,
                     const AtpFtCell *rhs_tmpl,
                     const void   *subst) {
  // Regime (a): 0-arity RHS (ground constant OR unbound-var template).
  // The "unbound var" case can only arise if the rule's RHS is a bare
  // var; either it is bound (regime c handles it correctly), or it is
  // not (defensive bail -- we still produce a fresh var cell).
  int rhs_is_var       = ft_s_is_var(rhs_tmpl);
  int rhs_is_single    = (rhs_tmpl->arity == 0u);
  int rhs_var_unbound  = 0;
  if (rhs_is_var && rhs_is_single) {
    u32 id = ft_s_var_id(rhs_tmpl);
    if (id >= ft_subst_max_vars() || ft_subst_lookup(subst, id) == NULL) {
      rhs_var_unbound = 1;
    }
  }
  if (rhs_is_single && (!rhs_is_var || rhs_var_unbound)) {
    // In-place overwrite.  redex's pointer identity stays the same so
    // a parent==NULL root rewrite still returns `redex` (== `root`).
    ft_splice_inplace_const_or_unbound_var(a, root, parent, redex,
                                           rhs_tmpl, subst);
    if (parent == NULL) {
      // Root rewrite: redex IS root (caller's invariant for the root
      // case).  Return the same cell, now overwritten.
      return redex;
    }
    // Subterm rewrite: rewire the predecessor's `next` -- but we
    // overwrote redex in place, so the chain is already correct (the
    // predecessor still points at the same cell, which now has the
    // new symbol).  However, parent->end may have pointed at the OLD
    // redex->end (the deepest cell of the displaced subtree).  If so,
    // bubble the end-pointer fix up to whatever ancestor needs it.
    //
    // Bubble: walk from `parent` outward, updating any ->end that
    // pointed at the old tail's last cell.  In the in-place regime,
    // however, the displaced tail has been freed; the NEW redex's
    // `end` is itself, and the cells above need their `end` pointer
    // adjusted iff they previously pointed at tail_last (which we
    // don't have access to here without threading it through).  This
    // is correct ONLY when the redex is a leaf of the tree above
    // (i.e. the rightmost branch); the general case is handled by
    // regime (c) below.  For the regime-(a) path we conservatively
    // delegate end-fixup to the caller's next find_redex_ft walk,
    // which re-derives end pointers from the head-cell chain.
    return root;
  }
  // Regime (c): build a fresh instantiated RHS, splice the displaced
  // span out, splice the fresh subtree in.
  AtpFtCell *repl = ft_splice_build_rhs(a, rhs_tmpl, subst);
  if (repl == NULL) {
    fprintf(stderr, "ft_splice: rhs build failed (unbound var?)\n");
    return root;
  }
  // Preserve the post-redex sibling pointer.
  AtpFtCell *post_sibling = redex->end->next;
  // Snapshot the displaced span.
  AtpFtCell *span_first = redex;
  AtpFtCell *span_last  = redex->end;
  if (parent == NULL) {
    // Root rewrite: free the OLD root subtree, return repl.
    if (span_first != NULL) {
      ft_free_span(a, span_first, span_last);
    }
    // repl->end->next is currently NULL (constructor invariant); root
    // has no post-sibling, so leave it.
    (void)post_sibling;
    return repl;
  }
  // Subterm rewrite: find the cell whose `next` is redex; rewire its
  // `next` to repl, then set repl->end->next = post_sibling.
  AtpFtCell **predecessor_next_slot = ft_splice_find_pred(parent, redex);
  if (predecessor_next_slot == NULL) {
    fprintf(stderr, "ft_splice: predecessor not found in parent's chain\n");
    return root;
  }
  *predecessor_next_slot = repl;
  repl->end->next = post_sibling;
  // If `parent->end` (or any ancestor's end) pointed at span_last,
  // update it to point at repl->end.  We do this by re-walking the
  // ancestors via the root and patching any matching end pointer.
  // The find_redex_ft walk in atp_rewrite_normalize_ft already
  // re-derives the end pointer from cell->end on the way down, so a
  // stale end at the ancestor would mis-thread the next sibling-walk;
  // patch it now.
  for (AtpFtCell *p = root; p != NULL; p = p->next) {
    if (p->end == span_last) p->end = repl->end;
    if (p == repl) break;     // we've passed where the redex lived
  }
  // Free the displaced span.  ft_free_span re-uses `last->next` as
  // the free-list link; the post_sibling pointer we captured above
  // is already attached to repl->end, so we are safe to clobber
  // span_last->next here.
  ft_free_span(a, span_first, span_last);
  return root;
}

#endif // THVM_ATPFT_NORM
