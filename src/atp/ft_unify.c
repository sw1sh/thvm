// ft_unify.c - AtpFt-native most-general unifier (Robinson) +
// substitution-apply over AtpFtCell trees.
//
// Mirrors src/unify/_.c thvm_unify + thvm_unify_apply but operates
// on the AtpFt flat-cell layout (sym/arity/end/next).  Reuses the
// AtpFtSubst storage shape declared in ft_match.c -- bind[id] = NULL
// for unbound, AtpFtCell* for bound -- so the matcher and the
// unifier share one bookkeeping struct.  The unifier writes into
// bind[id] directly (no watermark backtracking on the public entry;
// the caller stages the entry watermark with ft_subst_reset).
//
// Build new subterms during ft_unify_apply into the SCRATCH arena
// (arena B) -- the caller resets the scratch base between candidate
// positions, so unify products are cheap to discard.  The caller
// can re-encode the final CP slot via ft_to_term ONCE per accepted
// CP (mirrors how ft_norm builds RHS instantiations).
//
// Gated under THVM_ATPFT_MATCH (same envelope as ft_match.c so the
// AtpFtSubst type is in scope when this file is pulled in).

#ifdef THVM_ATPFT_MATCH
#ifndef ATPFT_FT_UNIFY_C_INCLUDED
#define ATPFT_FT_UNIFY_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"   // WF_VAR_BIT

// --- Var-bit helpers (local copies, byte-identical to ft_match.c's) ---
static inline int  ft_u_is_var (const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) != 0u;
}
static inline u32  ft_u_var_id (const AtpFtCell *c) {
  return c->sym & ~WF_VAR_BIT;
}

// --- Walk through bind chain ------------------------------------------
//
// If `t` is a bound variable, follow the binding.  Returns the final
// non-bound cell.  AtpFtSubst can bind a var to another var, forming
// FVR -> FVR -> CTR chains -- walk them all in one shot.
static const AtpFtCell *ft_unify_walk(const AtpFtCell *t,
                                      const AtpFtSubst *s) {
  while (t != NULL && ft_u_is_var(t)) {
    u32 id = ft_u_var_id(t);
    if (id >= ATPFT_MAX_VARS) return t;
    AtpFtCell *b = s->bind[id];
    if (b == NULL) return t;
    t = b;
  }
  return t;
}

// --- Occurs check -----------------------------------------------------
//
// Does the variable `var_id` (after walking) appear anywhere in `t`?
static u8 ft_unify_occurs(u32 var_id, const AtpFtCell *t,
                          const AtpFtSubst *s) {
  t = ft_unify_walk(t, s);
  if (t == NULL) return 0;
  if (ft_u_is_var(t)) {
    return ft_u_var_id(t) == var_id;
  }
  // CTR: scan children via the next/end sibling stride.
  u16 arity = t->arity;
  const AtpFtCell *c = t->next;
  for (u16 i = 0; i < arity; i++) {
    if (ft_unify_occurs(var_id, c, s)) return 1;
    c = c->end->next;
  }
  return 0;
}

// --- Bind helper: install a fresh binding, updating bound_ids[] ------
//
// Caller has already verified id < ATPFT_MAX_VARS and bind[id] is
// NULL.  Appends to the watermark list so a later ft_subst_reset/
// rewind clears it correctly.
static inline void ft_unify_bind(AtpFtSubst *s, u32 id,
                                 const AtpFtCell *val) {
  s->bind[id]              = (AtpFtCell *)val;
  s->bound_ids[s->wm]      = id;
  s->wm                   += 1u;
}

// --- ft_unify ---------------------------------------------------------
//
// Robinson MGU.  Returns 1 on success, 0 on clash or occurs-fail.
// Both `s` and `t` may contain variables on either side; the
// substitution accumulates incrementally and `bound_ids[]` records
// every id installed so the caller can rewind via ft_subst_reset.
int ft_unify(const AtpFtCell *s, const AtpFtCell *t, AtpFtSubst *subst) {
  if (s == NULL || t == NULL) return 0;
  s = ft_unify_walk(s, subst);
  t = ft_unify_walk(t, subst);
  // Same cell after walking -> definitely equal (no work).
  if (s == t) return 1;

  int s_is_var = ft_u_is_var(s);
  int t_is_var = ft_u_is_var(t);

  // Two variables: bind the lower-id (or s -> t) without occurs check.
  if (s_is_var && t_is_var) {
    u32 sid = ft_u_var_id(s);
    u32 tid = ft_u_var_id(t);
    if (sid == tid) return 1;
    if (sid >= ATPFT_MAX_VARS) return 0;
    ft_unify_bind(subst, sid, t);
    return 1;
  }

  if (s_is_var) {
    u32 id = ft_u_var_id(s);
    if (id >= ATPFT_MAX_VARS) return 0;
    if (ft_unify_occurs(id, t, subst)) return 0;
    ft_unify_bind(subst, id, t);
    return 1;
  }
  if (t_is_var) {
    u32 id = ft_u_var_id(t);
    if (id >= ATPFT_MAX_VARS) return 0;
    if (ft_unify_occurs(id, s, subst)) return 0;
    ft_unify_bind(subst, id, s);
    return 1;
  }

  // Both CTR: heads must match (sym + arity), then recurse on each
  // child pair via the AtpFt sibling stride.
  if (s->sym != t->sym) return 0;
  if (s->arity != t->arity) return 0;
  u16 arity = s->arity;
  const AtpFtCell *sc = s->next;
  const AtpFtCell *tc = t->next;
  for (u16 i = 0; i < arity; i++) {
    if (!ft_unify(sc, tc, subst)) return 0;
    sc = sc->end->next;
    tc = tc->end->next;
  }
  return 1;
}

// --- ft_unify_apply ---------------------------------------------------
//
// Build a fresh AtpFt term mirroring `t` with every variable
// recursively walked through the substitution chain.  Allocated in
// the requested arena (scratch=1 typical for transient CP-gen).
//
// Bound CTR variables: walk to the CTR target, then deep-copy that
// target (vars inside the target may themselves be bound; recurse).
// Unbound vars: emit as-is (a fresh var cell with the same id).

extern AtpFtCell *ftnew_var  (AtpFt *a, u32 var_id, int scratch);
extern AtpFtCell *ftnew_const(AtpFt *a, u32 sym, int scratch);
extern AtpFtCell *ftnew_ctr  (AtpFt *a, u32 sym, u16 arity,
                              AtpFtCell *const *kids, int scratch);

#define FT_UNIFY_APPLY_KIDS_STACK 16u

AtpFtCell *ft_unify_apply(AtpFt *a, const AtpFtCell *t,
                          const AtpFtSubst *s, int scratch) {
  if (t == NULL) return NULL;
  t = ft_unify_walk(t, s);
  if (t == NULL) return NULL;
  if (ft_u_is_var(t)) {
    // Walked to an unbound var -- emit a fresh var cell with the id.
    return ftnew_var(a, ft_u_var_id(t), scratch);
  }
  u32 sym   = t->sym;
  u16 arity = t->arity;
  if (arity == 0u) {
    return ftnew_const(a, sym, scratch);
  }
  AtpFtCell *stack_kids[FT_UNIFY_APPLY_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_UNIFY_APPLY_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_unify_apply: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = t->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ft_unify_apply(a, child, s, scratch);
    if (kids[i] == NULL) {
      free(heap_kids);
      return NULL;
    }
    child = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

#endif // !ATPFT_FT_UNIFY_C_INCLUDED
#endif // THVM_ATPFT_MATCH
