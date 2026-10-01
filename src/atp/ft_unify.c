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
  // Open CTRs on an explicit stack: `child` is the next (unwalked)
  // child cell, `left` how many remain.
  typedef struct { const AtpFtCell *child; u32 left; } OccFrame;
  OccFrame  fr_inl[FT_WALK_INL];
  OccFrame *fr  = fr_inl;
  u32       nfr = 0u;
  u32       cfr = FT_WALK_INL;
  u8        hit = 0u;
  for (;;) {
    t = ft_unify_walk(t, s);
    if (t != NULL) {
      if (ft_u_is_var(t)) {
        hit = (u8)(ft_u_var_id(t) == var_id);
      } else if (t->arity > 0u) {
        // CTR: scan children via the next/end sibling stride.
        if (nfr == cfr) {
          fr = (OccFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].child = t->next;
        fr[nfr].left  = t->arity;
        nfr++;
      }
    }
    while (nfr > 0u && fr[nfr - 1u].left == 0u) {
      nfr--;
    }
    if (hit || nfr == 0u) {
      break;
    }
    OccFrame *f = &fr[nfr - 1u];
    t        = f->child;
    f->child = t->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  return hit;
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
  // Open CTR pairs on an explicit stack: `sc` / `tc` are the next
  // children of the two (walked) parents, `left` how many remain.
  typedef struct { const AtpFtCell *sc; const AtpFtCell *tc; u32 left; } UniFrame;
  UniFrame  fr_inl[FT_WALK_INL];
  UniFrame *fr  = fr_inl;
  u32       nfr = 0u;
  u32       cfr = FT_WALK_INL;
  int       ok  = 1;
  for (;;) {
    if (s == NULL || t == NULL) {
      ok = 0;
      break;
    }
    s = ft_unify_walk(s, subst);
    t = ft_unify_walk(t, subst);
    // Same cell after walking -> definitely equal (no work).
    if (s != t) {
      int s_is_var = ft_u_is_var(s);
      int t_is_var = ft_u_is_var(t);
      if (s_is_var && t_is_var) {
        // Two variables: bind the lower-id (or s -> t) without occurs check.
        u32 sid = ft_u_var_id(s);
        u32 tid = ft_u_var_id(t);
        if (sid != tid) {
          if (sid >= ATPFT_MAX_VARS) {
            ok = 0;
            break;
          }
          ft_unify_bind(subst, sid, t);
        }
      } else if (s_is_var) {
        u32 id = ft_u_var_id(s);
        if (id >= ATPFT_MAX_VARS || ft_unify_occurs(id, t, subst)) {
          ok = 0;
          break;
        }
        ft_unify_bind(subst, id, t);
      } else if (t_is_var) {
        u32 id = ft_u_var_id(t);
        if (id >= ATPFT_MAX_VARS || ft_unify_occurs(id, s, subst)) {
          ok = 0;
          break;
        }
        ft_unify_bind(subst, id, s);
      } else {
        // Both CTR: heads must match (sym + arity), then each child
        // pair via the AtpFt sibling stride.
        if (s->sym != t->sym || s->arity != t->arity) {
          ok = 0;
          break;
        }
        if (s->arity > 0u) {
          if (nfr == cfr) {
            fr = (UniFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].sc   = s->next;
          fr[nfr].tc   = t->next;
          fr[nfr].left = s->arity;
          nfr++;
        }
      }
    }
    while (nfr > 0u && fr[nfr - 1u].left == 0u) {
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    UniFrame *f = &fr[nfr - 1u];
    s     = f->sc;
    t     = f->tc;
    f->sc = s->end->next;
    f->tc = t->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  return ok;
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


AtpFtCell *ft_unify_apply(AtpFt *a, const AtpFtCell *t,
                          const AtpFtSubst *s, int scratch) {
  // Bottom-up over an explicit stack: children (of the WALKED cell) are
  // built before ftnew_ctr stitches them, the recursion's order.
  FtSpanFrame  fr_inl[FT_WALK_INL];
  AtpFtCell   *val_inl[FT_WALK_INL];
  FtSpanFrame *fr   = fr_inl;
  AtpFtCell  **val  = val_inl;
  u32          nfr  = 0u;
  u32          cfr  = FT_WALK_INL;
  u32          nval = 0u;
  u32          cval = FT_WALK_INL;
  AtpFtCell   *out  = NULL;
  for (;;) {
    if (t == NULL) {
      break;
    }
    t = ft_unify_walk(t, s);
    if (t == NULL) {
      break;
    }
    AtpFtCell *done = NULL;
    if (ft_u_is_var(t)) {
      // Walked to an unbound var -- emit a fresh var cell with the id.
      done = ftnew_var(a, ft_u_var_id(t), scratch);
    } else if (t->arity == 0u) {
      done = ftnew_const(a, t->sym, scratch);
    } else {
      if (nfr == cfr) {
        fr = (FtSpanFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].child = t->next;
      fr[nfr].left  = t->arity;
      fr[nfr].sym   = t->sym;
      fr[nfr].arity = t->arity;
      nfr++;
    }
    if (done != NULL) {
      if (nval == cval) {
        val = (AtpFtCell **)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = done;
    }
    while (nfr > 0u && fr[nfr - 1u].left == 0u) {
      FtSpanFrame *f = &fr[nfr - 1u];
      nval -= f->arity;
      val[nval] = ftnew_ctr(a, f->sym, (u16)f->arity, &val[nval], scratch);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      out = val[0];
      break;
    }
    FtSpanFrame *f = &fr[nfr - 1u];
    t        = f->child;
    f->child = t->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

#endif // !ATPFT_FT_UNIFY_C_INCLUDED
#endif // THVM_ATPFT_MATCH
