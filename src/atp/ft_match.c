// ft_match.c - AtpFt-native one-way matching +
// substitution-apply.
//
// lifts the (rewrite/_.c) `thvm_match` / `thvm_subst_apply`
// pair onto AtpFt cells.  Goal: equivalent behavior to thvm_match on
// the heap-cell Term path -- same boolean verdict, same recorded
// bindings -- while reading exclusively through the flat
// `next` / `end` / `sym` / `arity` cell layout (so callers can wire
// the AtpFt-native rule-index descent without the per-call
// AtpFtCell -> Term encode that today's bridge would force).
//
// API shape (see docs/atp/engineering.md):
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
//     ATPFT_MAX_VARS=64 by definition), but ALSO maintains a
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
//     occurrence.  ft_splice replaces this with a zero-copy in-place
//     splice; the deep copy is the "obviously correct" baseline
//     against which the splice is differential-tested.
//
// Gated on THVM_ATPFT_MATCH.  ATPFT_FT_MATCH_C_INCLUDED guards against
// double-inclusion (tests/test_ft_match.c includes this directly while
// src/atp/_.c pulls it through under THVM_ATPFT_MATCH).

#ifdef THVM_ATPFT_MATCH
#ifndef ATPFT_FT_MATCH_C_INCLUDED
#define ATPFT_FT_MATCH_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"   // WF_VAR_BIT -- same encoding as ft.c

// --- Var-bit helpers --------------------------------------------------
//
// ft.c keeps the same accessor names (ft_is_var, ft_var_id, ft_ctr_sym)
// as `static inline` in its TU.  In the single-TU build the test
// includes ft.c BEFORE ft_match.c (lands the converters); the
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

  // Iterative lockstep walk of the two flatterm preorders.  Both `pat`
  // and `subj` are flat (preorder) AtpFtCell chains, and syntactic
  // matching is deterministic (no backtracking), so the recursive
  // descent collapses to a single linear scan -- no per-node call frame.
  //   * pat CTR cell: the aligned subj cell must be a same-shape CTR;
  //     descend BOTH (->next) into their children.
  //   * pat var cell (a leaf): bind it to the aligned subj SUBTERM
  //     (the cell stands for [sc, sc->end]); on a repeated var, check
  //     structural equality (ft_eq, == the Term-side kbo_eq).  Then
  //     skip the consumed subj subterm (->end->next) and advance pat
  //     to its next preorder cell (->next; a var's end is itself).
  // The single entry watermark rewinds every binding on any failure --
  // the net post-failure subst state is identical to the recursive
  // version's incremental per-level rewinds (there is no retry path).
  u32 wm_save = out->wm;
  const AtpFtCell *pc     = pat;
  const AtpFtCell *pc_end = (pat->end != NULL) ? pat->end->next : NULL;
  const AtpFtCell *sc     = subj;
  while (pc != pc_end) {
    if (sc == NULL) { ft_subst_rewind(out, wm_save); return 0; }
    if (ft_m_is_var(pc)) {
      u32 id = ft_m_var_id(pc);
      if (id >= ATPFT_MAX_VARS) { ft_subst_rewind(out, wm_save); return 0; }
      AtpFtCell *prev = out->bind[id];
      if (prev == NULL) {
        out->bind[id]           = (AtpFtCell *)sc;
        out->bound_ids[out->wm] = id;
        out->wm                += 1u;
      } else if (!ft_eq(prev, sc)) {
        ft_subst_rewind(out, wm_save);
        return 0;
      }
      sc = (sc->end != NULL) ? sc->end->next : NULL;   // skip subj subterm
      pc = pc->next;                                   // var leaf -> next cell
    } else {
      // pat CTR: subj must be a same-shape CTR.
      if (ft_m_is_var(sc) || pc->sym != sc->sym || pc->arity != sc->arity) {
        ft_subst_rewind(out, wm_save);
        return 0;
      }
      pc = pc->next;   // descend into children (both preorders)
      sc = sc->next;
    }
  }
  return 1;
}

// --- ft_subst_apply ---------------------------------------------------
//
// Construction of a fresh AtpFt term mirroring `tmpl` with every
// variable replaced by its binding from `s`, bottom-up over an explicit
// stack (children before ftnew_ctr, the recursion's allocation order).
//
//   * tmpl var: look up bind[id]; if NULL, return NULL (defensive --
//     a valid CP-RHS template should only mention vars bound by the
//     LHS match, so the NULL path should be unreachable on the live
//     path but is signalled explicitly here).  Otherwise deep-copy
//     the binding into the destination arena (no aliasing back into
//     Arena A from a scratch result, and vice versa).
//
//   * tmpl CTR(sym, c0..c_{n-1}): build each child, then stitch
//     via ftnew_ctr.
//
// scratch flag is threaded through every allocation so the returned
// term lives in a single arena (caller chooses).

// Deep copy: rebuild `src` cell-for-cell into the destination arena.
// Variables stay variables (they're real subterm contents now -- a
// bound subterm in a CP-substitution is whatever was matched against,
// which may itself contain free variables).
static AtpFtCell *ft_deep_copy_rec(AtpFt *a, const AtpFtCell *src, int scratch) {
  if (src == NULL) return NULL;
  return ft_copy_span(a, src, 0u, scratch);
}

static AtpFtCell *ft_subst_apply_rec(AtpFt *a, const AtpFtCell *tmpl,
                                     const AtpFtSubst *s, int scratch) {
  if (tmpl == NULL) return NULL;
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
    AtpFtCell *done = NULL;
    if (ft_m_is_var(tmpl)) {
      // An unbound (or out-of-range) template var fails the whole
      // apply -- a defensive bail-out; cells built so far stay in the
      // arena as garbage, as with the recursion.
      u32 id = ft_m_var_id(tmpl);
      AtpFtCell *binding = (id < ATPFT_MAX_VARS) ? s->bind[id] : NULL;
      if (binding == NULL) {
        break;
      }
      done = ft_deep_copy_rec(a, binding, scratch);
    } else if (tmpl->arity == 0u) {
      done = ftnew_const(a, tmpl->sym, scratch);
    } else {
      if (nfr == cfr) {
        fr = (FtSpanFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].child = tmpl->next;
      fr[nfr].left  = tmpl->arity;
      fr[nfr].sym   = tmpl->sym;
      fr[nfr].arity = tmpl->arity;
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
    tmpl     = f->child;
    f->child = tmpl->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

// Preorder-streaming twins of the two builders above (FT-representation
// P1): allocate the PARENT cell before its children so the reduct /
// bound-subterm copy threads through physically-consecutive arena cells
// (WM SigmaRGanzNeu), instead of bottom-up ftnew_ctr where every ->next
// is a backward load.  These are the HOT construction sites (the reduct
// built on every rewrite step, the bound subterm deep-copied at each var
// occurrence).  Byte-identical RESULT to the *_rec twins: same sym (the
// masked value ftnew_const/ftnew_ctr would store), arity, GROUND flag
// (OR-folded), next/end -- only addresses differ, invisible to every
// verdict.  Gated by ft.c's g_ft_preorder (same TU).  On an unbound-var
// NULL, bubble NULL exactly like *_rec; the half-built parent is arena
// garbage on an error path that cannot arise for well-formed rules.
// A parent cell (already allocated) whose children are being built from
// a span: `child` is the next source child, `prev_end` the last cell of
// the child stitched last (NULL before the first).
typedef struct {
  const AtpFtCell *child;
  u32              left;
  AtpFtCell       *c;
  AtpFtCell       *prev_end;
} FtPreSpanFrame;

// Pre-order copy of the span `src` (s == NULL) or instantiation of the
// template `src` under `s` (each bound var deep-copied, pre-order too).
// An unbound / out-of-range var yields NULL; the half-built parents are
// arena garbage, as with the recursion.
static AtpFtCell *ft_build_pre(AtpFt *a, const AtpFtCell *src,
                               const AtpFtSubst *s, int scratch) {
  FtPreSpanFrame  fr_inl[FT_WALK_INL];
  FtPreSpanFrame *fr   = fr_inl;
  u32             nfr  = 0u;
  u32             cfr  = FT_WALK_INL;
  AtpFtCell      *done = NULL;
  for (;;) {
    done = NULL;
    if (ft_m_is_var(src)) {
      if (s != NULL) {
        u32 id = ft_m_var_id(src);
        AtpFtCell *binding = (id < ATPFT_MAX_VARS) ? s->bind[id] : NULL;
        if (binding == NULL) {
          break;
        }
        done = ft_build_pre(a, binding, NULL, scratch);
      } else {
        AtpFtCell *c = ft_alloc_one(a, scratch);
        c->next = NULL; c->end = c;
        c->sym = WF_VAR_BIT | (ft_m_var_id(src) & ~WF_VAR_BIT);
        c->arity = 0; c->flags = 0;
        ft_raw_check(c);
        done = c;
      }
    } else {
      u16 arity = src->arity;
      AtpFtCell *c = ft_alloc_one(a, scratch);          // parent first
      c->sym = src->sym & ~WF_VAR_BIT;
      c->arity = arity;
      c->flags = ATPFT_FLAG_GROUND;
      if (arity == 0u) {
        c->next = NULL; c->end = c;
        ft_raw_check(c);
        done = c;
      } else {
        if (nfr == cfr) {
          fr = (FtPreSpanFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].child    = src->next;
        fr[nfr].left     = arity;
        fr[nfr].c        = c;
        fr[nfr].prev_end = NULL;
        nfr++;
      }
    }
    // Stitch the finished span under its parent; a parent whose last
    // child is stitched is finished in turn.
    while (done != NULL && nfr > 0u) {
      FtPreSpanFrame *f = &fr[nfr - 1u];
      if (f->prev_end == NULL) {
        f->c->next = done;
      } else {
        f->prev_end->next = done;
      }
      if ((done->flags & ATPFT_FLAG_GROUND) == 0u) {
        f->c->flags = 0;
      }
      f->prev_end = done->end;
      if (f->left > 0u) {
        done = NULL;
        break;
      }
      AtpFtCell *c = f->c;
      c->end = f->prev_end; c->end->next = NULL;
      ft_raw_check(c);
      done = c;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtPreSpanFrame *f = &fr[nfr - 1u];
    src      = f->child;
    f->child = src->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  return (nfr == 0u) ? done : NULL;
}

static AtpFtCell *ft_deep_copy_pre(AtpFt *a, const AtpFtCell *src, int scratch) {
  if (src == NULL) return NULL;
  return ft_build_pre(a, src, NULL, scratch);
}
static AtpFtCell *ft_subst_apply_pre(AtpFt *a, const AtpFtCell *tmpl,
                                     const AtpFtSubst *s, int scratch) {
  if (tmpl == NULL) return NULL;
  return ft_build_pre(a, tmpl, s, scratch);
}

AtpFtCell *ft_subst_apply(AtpFt *a, const AtpFtCell *tmpl,
                          const AtpFtSubst *s, int scratch) {
  if (ft_preorder_on()) return ft_subst_apply_pre(a, tmpl, s, scratch);
  return ft_subst_apply_rec(a, tmpl, s, scratch);
}

// --- Type-erased accessors for splice ------------------------
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

#endif // !ATPFT_FT_MATCH_C_INCLUDED
#endif // THVM_ATPFT_MATCH
