// test_ft_norm.c - AtpFt-native normalize fixpoint
// differential corpus against the Term-side reference.
//
// T1: ground-only rules; AtpFt NF matches Term NF.
// T2: rules with vars; AtpFt NF matches Term NF.
// T3: large random corpus -- 1000 random subjects + random orientable
//     rule sets, AtpFt NF must equal Term NF cell-for-cell.

// Per the Makefile rule for test_ft_norm: THVM_ATPFT_ALLOC / _CONVERT /
// _LPO / _MATCH / _RULES / _NORM are all set on the command line, so
// src/thvm.c -> src/atp/_.c pulls in ft.h, ft_alloc.c, ft.c, ft_order.c,
// ft_match.c, ft_splice.c, ft_norm.c on its own.  We do NOT re-include
// them here -- a double-include would redefine every static symbol.

#include "../src/thvm.c"
#include "../src/atp/ft.h"

#include "test.h"

// --- Random term generator ------------------------------------------
//
// Same pattern as tests/test_ft_match.c.  Var ids cycle mod 4 (well
// under ATPFT_MAX_VARS=64), CTR labels mod 4 (small signature so
// random matches actually fire).
static u32 rng32(u32 *seed) {
  u32 x = *seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *seed = x ? x : 0xdeadbeefu;
  return *seed;
}

static Term mk_random_term(u32 *seed, u32 depth) {
  u32 r = rng32(seed);
  if (depth == 0u || (r & 3u) == 0u) {
    if (r & 0x10u) {
      return term_new_fvr((r >> 5) & 3u);
    }
    return term_new_ctr((r >> 5) & 3u, NULL, 0u);
  }
  Term kids[2];
  kids[0] = mk_random_term(seed, depth - 1u);
  kids[1] = mk_random_term(seed, depth - 1u);
  return term_new_ctr((r >> 5) & 3u, kids, 2u);
}

int main(void) {
  thvm_init();

  // ---- T1: single-rule ground reduction --------------------------
  TEST_BEGIN("ft_norm/single-ground-rule");
  {
    // Rule: a -> b.  Subject: f(a, a) -> f(b, b).
    Term a = term_new_ctr(1u, NULL, 0u);
    Term b = term_new_ctr(2u, NULL, 0u);
    Term kids[2] = {a, a};
    Term subj = term_new_ctr(7u, kids, 2u);

    KboConfig cfg = (KboConfig){0};
    AtpState *s = thvm_atp_init(&cfg, 64);
    CHECK(thvm_atp_add_equation(s, a, b));
    // After orient: rule 0 is a -> b.

    // Term-side NF.
    Term tnf = atp_rewrite_normalize(s, subj, s->lhs, s->rhs, s->n_rules, 64u);
    // AtpFt-side NF.
    AtpFt *arena = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fsubj = ft_from_term(arena, subj, 0);
    AtpFtCell *fnf   = atp_rewrite_normalize_ft(s, fsubj, 64u);

    // Compare via ft_eq vs Term -> ft.
    AtpFtCell *tnf_ft = ft_from_term(arena, tnf, 0);
    CHECK(ft_eq(fnf, tnf_ft));

    thvm_atp_free(s);
  }

  // ---- T2: rule with a variable ----------------------------------
  TEST_BEGIN("ft_norm/var-rule");
  {
    // Rule: f(x, x) -> x.  Subject: f(a, a) -> a.
    Term x   = term_new_fvr(0u);
    Term lhs_kids[2] = {x, x};
    Term lhs = term_new_ctr(7u, lhs_kids, 2u);
    Term rhs = x;

    KboConfig cfg = (KboConfig){0};
    AtpState *s = thvm_atp_init(&cfg, 64);
    CHECK(thvm_atp_add_equation(s, lhs, rhs));

    Term a = term_new_ctr(1u, NULL, 0u);
    Term subj_kids[2] = {a, a};
    Term subj = term_new_ctr(7u, subj_kids, 2u);

    Term tnf = atp_rewrite_normalize(s, subj, s->lhs, s->rhs, s->n_rules, 64u);
    AtpFt *arena = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fsubj = ft_from_term(arena, subj, 0);
    AtpFtCell *fnf   = atp_rewrite_normalize_ft(s, fsubj, 64u);
    AtpFtCell *tnf_ft = ft_from_term(arena, tnf, 0);
    CHECK(ft_eq(fnf, tnf_ft));

    thvm_atp_free(s);
  }

  // ---- T3: random differential corpus ----------------------------
  TEST_BEGIN("ft_norm/random-differential");
  {
    u32 seed = 0xabcd1234u;
    u32 trials = 1000u;
    u32 matched = 0u;
    u32 verified = 0u;
    for (u32 trial = 0; trial < trials; trial++) {
      KboConfig cfg = (KboConfig){0};
      AtpState *s = thvm_atp_init(&cfg, 64);
      // Add 1-3 random equations.  Many will be unorientable and
      // simply skipped by thvm_atp_add_equation.
      u32 n_eq = 1u + (rng32(&seed) % 3u);
      for (u32 i = 0; i < n_eq; i++) {
        Term l = mk_random_term(&seed, 2u);
        Term r = mk_random_term(&seed, 2u);
        thvm_atp_add_equation(s, l, r);
      }
      if (s->n_rules == 0u) { thvm_atp_free(s); continue; }
      Term subj = mk_random_term(&seed, 3u);
      // Term NF.
      Term tnf = atp_rewrite_normalize(s, subj, s->lhs, s->rhs, s->n_rules, 64u);
      // AtpFt NF.
      AtpFt *arena = (AtpFt *)s->ft_arena_ptr;
      AtpFtCell *fsubj = ft_from_term(arena, subj, 0);
      AtpFtCell *fnf   = atp_rewrite_normalize_ft(s, fsubj, 64u);
      AtpFtCell *tnf_ft = ft_from_term(arena, tnf, 0);
      matched++;
      if (ft_eq(fnf, tnf_ft)) verified++;
      thvm_atp_free(s);
    }
    // Expect tight parity.  The two paths use the same rule
    // orientation and the same matcher semantics; any deviation is a
    // splice bug.
    if (verified != matched) {
      fprintf(stderr, "ft_norm random: matched=%u verified=%u\n",
              matched, verified);
    }
    CHECK(verified == matched);
  }

  TEST_REPORT();
  return 0;
}
