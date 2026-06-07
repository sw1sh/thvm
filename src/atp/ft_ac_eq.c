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

static u8 ft_ac_eq(const AtpFtCell *a, const AtpFtCell *b,
                   const AtpAcInfo *ac) {
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
  if (ft_ac_eq_is_ac_label(ac, a->sym)) {
    const AtpFtCell *A[ATPFT_AC_FLATTEN_CAP];
    const AtpFtCell *B[ATPFT_AC_FLATTEN_CAP];
    u32 na = 0u, nb = 0u;
    if (!ft_ac_flatten_under(a, a->sym, A, &na, ATPFT_AC_FLATTEN_CAP)) {
      // Overflow: conservatively fall back to ft_eq (syntactic).  This
      // matches the Term-side spirit: a >64-leaf AC node bails out of
      // canonicalization and lets the outer kbo_eq decide.
      return (u8)(ft_eq(a, b) ? 1u : 0u);
    }
    if (!ft_ac_flatten_under(b, b->sym, B, &nb, ATPFT_AC_FLATTEN_CAP)) {
      return (u8)(ft_eq(a, b) ? 1u : 0u);
    }
    if (na != nb) return 0u;

    u8 used[ATPFT_AC_FLATTEN_CAP] = {0};
    for (u32 i = 0; i < na; i++) {
      u8 matched = 0u;
      for (u32 j = 0; j < nb; j++) {
        if (used[j]) continue;
        if (ft_ac_eq(A[i], B[j], ac)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) return 0u;
    }
    return 1u;
  }

  // Non-AC CTR: same-arity recursive descent over children.
  u16 arity = a->arity;
  const AtpFtCell *ca = a->next;
  const AtpFtCell *cb = b->next;
  for (u16 i = 0; i < arity; i++) {
    if (!ft_ac_eq(ca, cb, ac)) return 0u;
    ca = ca->end->next;
    cb = cb->end->next;
  }
  return 1u;
}

#endif // !ATPFT_FT_AC_EQ_C_INCLUDED
#endif // THVM_ATP_AC && THVM_ATPFT_MATCH
