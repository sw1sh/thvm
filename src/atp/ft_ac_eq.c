// ft_ac_eq.c - AtpFt-native AC-equivalent equality.  Port of the
// Term-side `atp_ac_eq` (src/atp/ac.c:236) to flatterm cells so the
// goal_check FT path can detect AC-equal normal forms without
// round-tripping through Term cells.
//
// Algorithm:
//   * Both NULL -> equal; one NULL -> not equal.
//   * Both vars -> ft_eq (sym carries the var id, arity is 0).
//   * Var XOR CTR -> not equal.
//   * Different sym or arity -> not equal.
//   * Top is AC label (atp_ac_is_ac_label):
//       Flatten both via ft_ac_flatten_under, then compare as multisets
//       under recursive ft_ac_eq (with a used[] bool array).  After
//       flatten the cardinalities must match.
//   * Else (non-AC CTR):
//       Same-arity recursive descent over children via the FT sibling
//       stride c = c->end->next.
//
// Unlike the Term-side which goes canon-then-kbo_eq, this implementation
// uses a direct multiset compare.  Net effect is the same on `s ≡_AC t`,
// and avoids allocating a canonical Term for the FT-side hot path.
//
// Gated on THVM_ATP_AC && THVM_ATPFT_MATCH.  Sits next to ft_ac_match.c
// in the include order from _.c.

#if defined(THVM_ATP_AC) && defined(THVM_ATPFT_MATCH)
#ifndef ATPFT_FT_AC_EQ_C_INCLUDED
#define ATPFT_FT_AC_EQ_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"      // WF_VAR_BIT

// Forward decl: recursive multiset compare needs to call itself from
// inside the leaf-pairing loop.
static u8 ft_ac_eq(const AtpFtCell *a, const AtpFtCell *b,
                   const AtpAcInfo *ac);

// AC label test -- local copy, same shape as ft_ac_match.c's helper.
// Keeps this file self-contained in case it ever ships without
// ft_ac_match.c in scope.
static inline u8 ft_ac_eq_is_ac_label(const AtpAcInfo *ac, u32 label) {
  if (ac == NULL) return 0;
  if (label >= 64u) return 0;
  return (u8)((ac->ac_mask >> label) & 1ull);
}

// A pair compared in full (`*open` = 0), or two same-head CTRs whose
// children / leaves must be compared next (`*open` = 1, the verdict
// pending).  An AC pair's flattened leaves land at A / B with counts
// *na / *nb.
static u8 ft_ac_eq_head(const AtpFtCell *a, const AtpFtCell *b,
                        const AtpAcInfo *ac, const AtpFtCell **A,
                        const AtpFtCell **B, u32 *na, u32 *nb, u8 *is_ac,
                        u8 *open) {
  *open = 0u;
  if (a == NULL && b == NULL) return 1u;
  if (a == NULL || b == NULL) return 0u;

  u8 a_var = (u8)((a->sym & WF_VAR_BIT) != 0u);
  u8 b_var = (u8)((b->sym & WF_VAR_BIT) != 0u);

  // Both vars -> ft_eq checks sym (carries id) and arity (==0).
  if (a_var && b_var) return (u8)(ft_eq(a, b) ? 1u : 0u);
  // var XOR CTR -> not equal.
  if (a_var != b_var) return 0u;

  // Both CTRs.  Sym + arity must match.
  if (a->sym != b->sym) return 0u;
  if (a->arity != b->arity) return 0u;

  // AC top: flatten both and compare as multisets.
  *is_ac = ft_ac_eq_is_ac_label(ac, a->sym);
  if (*is_ac) {
    *na = 0u;
    *nb = 0u;
    if (!ft_ac_flatten_under(a, a->sym, A, na, ATPFT_AC_FLATTEN_CAP)) {
      // Overflow: conservatively fall back to ft_eq (syntactic).  This
      // matches the Term-side spirit: a >64-leaf AC node bails out of
      // canonicalization and lets the outer kbo_eq decide.
      return (u8)(ft_eq(a, b) ? 1u : 0u);
    }
    if (!ft_ac_flatten_under(b, b->sym, B, nb, ATPFT_AC_FLATTEN_CAP)) {
      return (u8)(ft_eq(a, b) ? 1u : 0u);
    }
    if (*na != *nb) return 0u;
    if (*na == 0u) return 1u;
  } else if (a->arity == 0u) {
    return 1u;
  }
  *open = 1u;
  return 0u;
}

static u8 ft_ac_eq(const AtpFtCell *a, const AtpFtCell *b,
                   const AtpAcInfo *ac) {
  // One frame per open pair.  A non-AC pair steps its children in
  // lockstep (`ca` / `cb` the pair under comparison, `left` the pairs
  // after it).  An AC pair pairs each leaf A[i] with the first unused
  // B[j] that compares equal (`j` the leaf under comparison); its leaf
  // lists sit at leaves[base, base + 2 * cap) and its use marks at
  // used[ubase, ubase + cap).
  typedef struct {
    const AtpFtCell *ca;
    const AtpFtCell *cb;
    u32 left;
    u32 base;
    u32 ubase;
    u32 n;
    u32 i;
    u32 j;
    u8  is_ac;
  } FtAcEqFrame;
  enum { CAP = ATPFT_AC_FLATTEN_CAP };
  FtAcEqFrame       fr_inl[FT_WALK_INL];
  FtAcEqFrame      *fr  = fr_inl;
  u32               nfr = 0u;
  u32               cfr = FT_WALK_INL;
  const AtpFtCell  *lv_inl[4u * CAP];
  const AtpFtCell **lv  = lv_inl;
  u32               nlv = 0u;
  u32               clv = 4u * CAP;
  u8                us_inl[2u * CAP];
  u8               *us  = us_inl;
  u32               nus = 0u;
  u32               cus = 2u * CAP;
  u8                r;
  for (;;) {
    while (nlv + 2u * CAP > clv) {
      lv = (const AtpFtCell **)term_stack_grow((void *)lv, (const void *)lv_inl,
                                               &clv, sizeof *lv);
    }
    u32 na = 0u, nb = 0u;
    u8  is_ac = 0u, open = 0u;
    r = ft_ac_eq_head(a, b, ac, lv + nlv, lv + nlv + CAP, &na, &nb, &is_ac,
                      &open);
    if (open) {
      if (nfr == cfr) {
        fr = (FtAcEqFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      FtAcEqFrame *f = &fr[nfr++];
      f->is_ac = is_ac;
      if (is_ac) {
        while (nus + CAP > cus) {
          us = (u8 *)term_stack_grow(us, us_inl, &cus, sizeof *us);
        }
        f->base  = nlv;
        f->ubase = nus;
        f->n     = na;
        f->i     = 0u;
        f->j     = 0u;
        memset(us + nus, 0, CAP);
        nlv += 2u * CAP;
        nus += CAP;
        a = lv[f->base];
        b = lv[f->base + CAP];
      } else {
        f->ca   = a->next;
        f->cb   = b->next;
        f->left = (u32)a->arity - 1u;
        a = f->ca;
        b = f->cb;
      }
      continue;
    }
    // Hand the verdict r to the open frames; stop at the next pair to
    // compare.
    for (;;) {
      if (nfr == 0u) {
        term_stack_release(fr, fr_inl);
        term_stack_release((void *)lv, (const void *)lv_inl);
        term_stack_release(us, us_inl);
        return r;
      }
      FtAcEqFrame *f = &fr[nfr - 1u];
      u8 next = 0u;
      if (!f->is_ac) {
        if (r) {
          f->ca = f->ca->end->next;
          f->cb = f->cb->end->next;
          if (f->left > 0u) {
            f->left--;
            a = f->ca;
            b = f->cb;
            next = 1u;
          }
        }
      } else {
        const AtpFtCell **A = lv + f->base;
        const AtpFtCell **B = lv + f->base + CAP;
        u8               *u = us + f->ubase;
        if (r) {
          u[f->j] = 1u;
          f->i++;
          f->j = 0u;
        } else {
          f->j++;
        }
        if (f->i < f->n) {
          while (f->j < f->n && u[f->j]) {
            f->j++;
          }
          if (f->j < f->n) {
            a = A[f->i];
            b = B[f->j];
            next = 1u;
          } else {
            r = 0u;                    // A[i] matched no unused B[j]
          }
        } else {
          r = 1u;
        }
        if (!next) {
          nlv = f->base;
          nus = f->ubase;
        }
      }
      if (next) {
        break;
      }
      nfr--;
    }
  }
}

#endif // !ATPFT_FT_AC_EQ_C_INCLUDED
#endif // THVM_ATP_AC && THVM_ATPFT_MATCH
