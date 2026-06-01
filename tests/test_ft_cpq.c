// test_ft_cpq.c - AtpFt-native CP queue (dual-store) smoke test.
// See docs/atp/engineering.md.
//
// Built with -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO
// -DTHVM_ATPFT_MATCH -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM
// -DTHVM_ATPFT_CPQ; src/atp/_.c pulls in ft_cpq.c transitively when
// THVM_ATPFT_CPQ is set, so the test only needs the runtime header.
//
// What we cover:
//
//   T1: push N CPs onto the queue and assert each slot's parallel
//       FT view (cp_packed_ft[i]) round-trips back to the same Term
//       pair held by the legacy cp_packed[i] byte string.  The
//       acceptance criterion is byte-identical Term reconstruction
//       on the FT side (ft_to_term must match the unpacked legacy
//       pair on every slot).
//
//   T2: pop the heap min repeatedly and assert the slot's FT entry
//       was cleared (lhs == NULL / rhs == NULL) and any backfilled
//       slot still owns its FT spans (lhs != NULL).
//
//   T3: heap-swap reach -- push enough CPs to drive sift_up /
//       sift_down through several levels, then verify each live slot's
//       FT view still matches its cp_packed[] view (lockstep
//       invariant under arbitrary swaps).
//
//   T4: trivially-joinable FT verdict (atp_cp_trivially_joinable_ft)
//       matches the Term-path verdict on a hand-built CP that's
//       joinable under a tiny rule set.
//
// All assertions go through tests/test.h's CHECK macro.

#include "../src/thvm.c"
#include "../src/atp/ft.h"
#include "test.h"

#include <stdlib.h>

// --- Tiny KBO config -------------------------------------------------
//
// Two binary symbols (f at 1, g at 2) + a constant (c at 3) -- enough
// to construct nontrivial CPs without dragging in axiom files.
static u32 cpq_weights   [5] = {0, 1, 1, 1, 1};
static u32 cpq_precedence[5] = {0, 2, 1, 3, 4};
static const KboConfig CPQ_CFG = {
  .weights     = cpq_weights,
  .precedence  = cpq_precedence,
  .n_labels    = 5,
  .var_weight  = 1,
};

#define LAB_F 1u
#define LAB_G 2u
#define LAB_C 3u
#define LAB_H 4u  // unary, for the joinable-verdict test

static Term mk_var(u32 id)         { return term_new_fvr(id); }
static Term mk_c(void)             { return term_new_ctr(LAB_C, NULL, 0); }
static Term mk_f(Term a, Term b)   { Term c[2] = {a, b}; return term_new_ctr(LAB_F, c, 2); }
static Term mk_g(Term a, Term b)   { Term c[2] = {a, b}; return term_new_ctr(LAB_G, c, 2); }
static Term mk_h(Term a)           { Term c[1] = {a};    return term_new_ctr(LAB_H, c, 1); }

// Re-declare AtpCpEntry locally: the struct is defined inside
// ft_cpq.c (file-static).  Mirrored verbatim here so the test can
// read the parallel array via the void* in AtpState without having
// to expose ft_cpq.c's internals through the public header.  Layout
// is a 32-byte cache-line-aligned struct -- a _Static_assert guards
// against drift.
typedef struct AtpCpEntryMirror {
  AtpFtCell *lhs;
  AtpFtCell *rhs;
  u32        size;
  u32        weight;
  u16        priority;
  u16        origin_rule;
  u32        _pad;
} AtpCpEntryMirror;
_Static_assert(sizeof(AtpCpEntryMirror) == 32,
               "AtpCpEntryMirror must mirror AtpCpEntry layout");

static AtpCpEntryMirror *cpq_arr(AtpState *s) {
  return (AtpCpEntryMirror *)s->cp_packed_ft;
}

int main(void) {
  thvm_init();

  // ---- T1: push then verify slot-parity ---------------------------
  TEST_BEGIN("ft_cpq/push-roundtrip");
  {
    AtpState *s = thvm_atp_init(&CPQ_CFG, /*step_cap=*/1000u);

    // Push 16 hand-built CPs onto the queue.  thvm_atp_cp_set
    // populates both the legacy cp_packed[] byte queue AND
    // cp_packed_ft[] (Stage 7 dual-store).
    enum { N_CPS = 16 };
    for (u32 i = 0; i < N_CPS; i++) {
      Term lhs = mk_f(mk_var(0), mk_g(mk_c(), mk_var(1)));
      Term rhs = mk_g(mk_var(0), mk_f(mk_var(1), mk_c()));
      thvm_atp_cp_set(s, i, lhs, rhs);
    }
    s->n_cps = N_CPS;
    thvm_atp_cp_reheapify(s);

    CHECK(s->cp_packed_ft != NULL);
    AtpCpEntryMirror *arr = cpq_arr(s);
    for (u32 i = 0; i < s->n_cps; i++) {
      // Each live slot owns its FT spans + reproduces the legacy pair
      // exactly under ft_to_term.
      CHECK(arr[i].lhs != NULL);
      CHECK(arr[i].rhs != NULL);
      Term ul = 0, ur = 0;
      thvm_atp_cp_get(s, i, &ul, &ur);
      Term tl = ft_to_term(arr[i].lhs);
      Term tr = ft_to_term(arr[i].rhs);
      CHECK(kbo_eq(tl, ul));
      CHECK(kbo_eq(tr, ur));
    }

    thvm_atp_free(s);
  }

  // ---- T2: pop frees FT spans + backfill keeps lockstep -----------
  TEST_BEGIN("ft_cpq/pop-clears-and-backfills");
  {
    AtpState *s = thvm_atp_init(&CPQ_CFG, /*step_cap=*/1000u);
    enum { N_CPS = 8 };
    for (u32 i = 0; i < N_CPS; i++) {
      // Use varying lhs depths so cp_pri orders distinctly and the
      // heap actually moves slots around.
      Term lhs = (i & 1u) ? mk_f(mk_var(0), mk_var(1)) : mk_g(mk_c(), mk_c());
      Term rhs = mk_g(mk_var(0), mk_c());
      thvm_atp_cp_set(s, i, lhs, rhs);
    }
    s->n_cps = N_CPS;
    thvm_atp_cp_reheapify(s);

    // Pop two -- after each, the live tail slots must still have FT
    // spans and the now-vacated trailing slot must be cleared.
    AtpCpEntryMirror *arr = cpq_arr(s);
    u32 before = s->n_cps;
    for (u32 k = 0; k < 2u; k++) {
      Term lo = 0, ro = 0;
      u8 ok = thvm_atp_select_cp(s, &lo, &ro);
      CHECK(ok == 1u);
      CHECK(s->n_cps == before - 1u);
      // Vacated tail slot was cleared.
      CHECK(arr[s->n_cps].lhs == NULL);
      CHECK(arr[s->n_cps].rhs == NULL);
      // Every still-live slot retains its FT spans.
      for (u32 i = 0; i < s->n_cps; i++) {
        CHECK(arr[i].lhs != NULL);
        CHECK(arr[i].rhs != NULL);
      }
      before = s->n_cps;
    }

    thvm_atp_free(s);
  }

  // ---- T3: heap sift keeps FT view in lockstep --------------------
  TEST_BEGIN("ft_cpq/heap-swap-lockstep");
  {
    AtpState *s = thvm_atp_init(&CPQ_CFG, /*step_cap=*/1000u);
    enum { N_CPS = 32 };
    for (u32 i = 0; i < N_CPS; i++) {
      // Mix shallow / deep CPs so reheapify drives several levels of
      // sift_up / sift_down.
      Term lhs = (i % 3u == 0u)
                   ? mk_f(mk_f(mk_var(0), mk_var(1)), mk_g(mk_c(), mk_var(0)))
                   : mk_g(mk_var(0), mk_c());
      Term rhs = (i % 5u == 0u)
                   ? mk_g(mk_var(0), mk_f(mk_var(1), mk_c()))
                   : mk_f(mk_var(0), mk_c());
      thvm_atp_cp_set(s, i, lhs, rhs);
    }
    s->n_cps = N_CPS;
    thvm_atp_cp_reheapify(s);

    AtpCpEntryMirror *arr = cpq_arr(s);
    for (u32 i = 0; i < s->n_cps; i++) {
      Term ul = 0, ur = 0;
      thvm_atp_cp_get(s, i, &ul, &ur);
      Term tl = ft_to_term(arr[i].lhs);
      Term tr = ft_to_term(arr[i].rhs);
      CHECK(kbo_eq(tl, ul));
      CHECK(kbo_eq(tr, ur));
    }

    thvm_atp_free(s);
  }

  // ---- T4: atp_cp_trivially_joinable_ft verdict parity ------------
  //
  // Build a 1-rule rewrite system { f(c, x) -> h(x) } and a CP
  // (f(c, a), h(a)) that's trivially joined under one rewrite.  Check
  // that the FT-native joinability verdict matches the Term verdict.
  TEST_BEGIN("ft_cpq/joinable-ft-verdict-parity");
  {
    AtpState *s = thvm_atp_init(&CPQ_CFG, /*step_cap=*/1000u);
    Term r_lhs = mk_f(mk_c(), mk_var(0));
    Term r_rhs = mk_h(mk_var(0));
    u8 added = thvm_atp_add_equation(s, r_lhs, r_rhs);
    CHECK(added == 1u);

    // CP: (f(c, a), h(a)) -- joinable under the rule above.  Use a
    // distinct var id so the matcher actually fires.
    Term a = mk_var(7u);
    Term cp_l_term = mk_f(mk_c(), a);
    Term cp_r_term = mk_h(a);

    // Term-path verdict (reference).
    Term ref_l = cp_l_term, ref_r = cp_r_term;
    u8 ref_joined = atp_cp_trivially_joinable(s, &ref_l, &ref_r);

    // FT-path verdict (Stage 7 hot path).  Build the FT cells once
    // (this is the cost the queue's push site already pays into
    // cp_packed_ft -- here we do it inline).
    AtpFt *a_arena = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fl = ft_from_term(a_arena, cp_l_term, 0);
    AtpFtCell *fr = ft_from_term(a_arena, cp_r_term, 0);
    u8 ft_joined = atp_cp_trivially_joinable_ft(s, &fl, &fr);

    CHECK(ft_joined == ref_joined);

    thvm_atp_free(s);
  }

  TEST_REPORT();
  return 0;
}
