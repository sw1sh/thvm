// test_ft_match.c - AtpFt-native ft_match +
// ft_subst_apply.
//
// T1: ft_match of pattern x against subject f(a,b) -> binds x.
// T2: ft_match of f(x,x) against f(a,a) -> succeeds, x=a.
// T3: ft_match of f(x,x) against f(a,b) -> fails.
// T4: ft_match of f(x,y) against f(a,b) -> x=a, y=b.
// T5: shape mismatch (different sym, different arity) fails.
// T6: Watermark restore on failure -- after a failed match, wm is back
//     to what it was before the call.
// T7: ft_subst_apply on tmpl=f(x, g(y)) with x=a, y=b builds f(a, g(b)).
// T8/T9: Differential against thvm_match over 1k + 100k random
//     (pattern, subject) pairs; on success, decode the AtpFt binding
//     for every id back to Term and compare with thvm_match's binding.

#include "../src/thvm.c"

#ifndef THVM_ATPFT_ALLOC
#define THVM_ATPFT_ALLOC 1
#endif
#ifndef THVM_ATPFT_CONVERT
#define THVM_ATPFT_CONVERT 1
#endif
#ifndef THVM_ATPFT_LPO
#define THVM_ATPFT_LPO 1
#endif
#ifndef THVM_ATPFT_MATCH
#define THVM_ATPFT_MATCH 1
#endif

#include "../src/atp/ft.h"
#include "../src/atp/ft_alloc.c"
#include "../src/atp/ft.c"
#include "../src/atp/ft_order.c"
#include "../src/atp/ft_match.c"

#include "test.h"

// --- Random term generator (ported from tests/test_ft.c) -------------
//
// Same xorshift32 + recursive builder.  Var ids cycle mod 8 (well under
// ATPFT_MAX_VARS=64 so the slot table never refuses); CTR labels mod 16.
// The depth bound keeps the corpus bounded; mixing leaves/CTRs at each
// step gives enough shape variety to hit the repeated-var consistency
// path with non-trivial frequency.

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
      return term_new_fvr((r >> 5) & 7u);          // var id 0..7
    }
    return term_new_ctr((r >> 5) & 15u, NULL, 0u); // 0-arity ctr
  }
  Term kids[2];
  kids[0] = mk_random_term(seed, depth - 1u);
  kids[1] = mk_random_term(seed, depth - 1u);
  return term_new_ctr((r >> 5) & 15u, kids, 2u);
}

// Structural equality on Term (matches tests/test_ft.c's term_struct_eq).
static int term_struct_eq(Term a, Term b) {
  if (term_tag(a) != term_tag(b)) return 0;
  if (term_ext(a) != term_ext(b)) return 0;
  if (term_tag(a) == TAG_FVR) return 1;
  if (term_tag(a) == TAG_CTR) {
    u32 na = term_ctr_n(a);
    u32 nb = term_ctr_n(b);
    if (na != nb) return 0;
    for (u32 i = 0; i < na; i++) {
      if (!term_struct_eq(term_ctr_at(a, i), term_ctr_at(b, i))) return 0;
    }
    return 1;
  }
  return 1;
}

int main(void) {
  thvm_init();

  // ---- T1: pattern x against subject f(a,b) ------------------------
  TEST_BEGIN("ft_match/var-binds-compound");
  {
    AtpFt arena;
    ft_init(&arena);
    AtpFtCell *a   = ftnew_const(&arena, 1u, 0);
    AtpFtCell *b   = ftnew_const(&arena, 2u, 0);
    AtpFtCell *kids[2] = {a, b};
    AtpFtCell *fab = ftnew_ctr(&arena, 7u, 2u, kids, 0);

    AtpFtCell *x   = ftnew_var(&arena, 0u, 0);

    AtpFtSubst s = {0};
    CHECK(ft_match(x, fab, &s));
    CHECK_EQ(s.wm, 1u);
    CHECK(s.bind[0] == fab);
    CHECK_EQ(s.bound_ids[0], 0u);
    ft_destroy(&arena);
  }

  // ---- T2: f(x,x) vs f(a,a) -- repeated var consistent --------------
  TEST_BEGIN("ft_match/repeated-var-consistent");
  {
    AtpFt arena;
    ft_init(&arena);
    // Pattern: f(x, x)
    AtpFtCell *xp0    = ftnew_var(&arena, 0u, 0);
    AtpFtCell *xp1    = ftnew_var(&arena, 0u, 0);   // SAME var id
    AtpFtCell *pkids[2] = {xp0, xp1};
    AtpFtCell *pat    = ftnew_ctr(&arena, 5u, 2u, pkids, 0);
    // Subject: f(a, a)
    AtpFtCell *a0     = ftnew_const(&arena, 9u, 0);
    AtpFtCell *a1     = ftnew_const(&arena, 9u, 0);
    AtpFtCell *skids[2] = {a0, a1};
    AtpFtCell *subj   = ftnew_ctr(&arena, 5u, 2u, skids, 0);

    AtpFtSubst s = {0};
    CHECK(ft_match(pat, subj, &s));
    CHECK_EQ(s.wm, 1u);
    CHECK(ft_eq(s.bind[0], a0));
    ft_destroy(&arena);
  }

  // ---- T3: f(x,x) vs f(a,b) -- repeated var INCONSISTENT ------------
  TEST_BEGIN("ft_match/repeated-var-inconsistent");
  {
    AtpFt arena;
    ft_init(&arena);
    AtpFtCell *xp0  = ftnew_var(&arena, 0u, 0);
    AtpFtCell *xp1  = ftnew_var(&arena, 0u, 0);
    AtpFtCell *pk[2] = {xp0, xp1};
    AtpFtCell *pat  = ftnew_ctr(&arena, 5u, 2u, pk, 0);
    AtpFtCell *a    = ftnew_const(&arena, 1u, 0);
    AtpFtCell *b    = ftnew_const(&arena, 2u, 0);
    AtpFtCell *sk[2] = {a, b};
    AtpFtCell *subj = ftnew_ctr(&arena, 5u, 2u, sk, 0);

    AtpFtSubst s = {0};
    CHECK(!ft_match(pat, subj, &s));
    // On failure the entry watermark (0) is restored.
    CHECK_EQ(s.wm, 0u);
    ft_destroy(&arena);
  }

  // ---- T4: f(x,y) vs f(a,b) ----------------------------------------
  TEST_BEGIN("ft_match/two-distinct-vars");
  {
    AtpFt arena;
    ft_init(&arena);
    AtpFtCell *vx   = ftnew_var(&arena, 0u, 0);
    AtpFtCell *vy   = ftnew_var(&arena, 1u, 0);
    AtpFtCell *pk[2] = {vx, vy};
    AtpFtCell *pat  = ftnew_ctr(&arena, 5u, 2u, pk, 0);
    AtpFtCell *a    = ftnew_const(&arena, 11u, 0);
    AtpFtCell *b    = ftnew_const(&arena, 22u, 0);
    AtpFtCell *sk[2] = {a, b};
    AtpFtCell *subj = ftnew_ctr(&arena, 5u, 2u, sk, 0);

    AtpFtSubst s = {0};
    CHECK(ft_match(pat, subj, &s));
    CHECK_EQ(s.wm, 2u);
    CHECK(ft_eq(s.bind[0], a));
    CHECK(ft_eq(s.bind[1], b));
    ft_destroy(&arena);
  }

  // ---- T5: shape mismatch (sym + arity) -----------------------------
  TEST_BEGIN("ft_match/shape-mismatch");
  {
    AtpFt arena;
    ft_init(&arena);
    AtpFtCell *a   = ftnew_const(&arena, 1u, 0);
    AtpFtCell *b   = ftnew_const(&arena, 2u, 0);
    AtpFtCell *pk[2] = {a, b};
    AtpFtCell *pat1 = ftnew_ctr(&arena, 5u, 2u, pk, 0);   // f(a,b)
    AtpFtCell *pat2 = ftnew_ctr(&arena, 6u, 2u, pk, 0);   // g(a,b) -- different sym
    AtpFtSubst s = {0};
    CHECK(!ft_match(pat1, pat2, &s));
    CHECK_EQ(s.wm, 0u);
    // Different arity: f(a) vs f(a,b)
    AtpFtCell *ak[1] = {a};
    AtpFtCell *fa  = ftnew_ctr(&arena, 5u, 1u, ak, 0);
    ft_subst_reset(&s);
    CHECK(!ft_match(fa, pat1, &s));
    CHECK_EQ(s.wm, 0u);
    // Var vs CTR (CTR pattern, var subject) is mismatch.
    AtpFtCell *vv = ftnew_var(&arena, 0u, 0);
    ft_subst_reset(&s);
    CHECK(!ft_match(pat1, vv, &s));
    CHECK_EQ(s.wm, 0u);
    ft_destroy(&arena);
  }

  // ---- T6: watermark restore on a partial-match failure -------------
  TEST_BEGIN("ft_match/watermark-restore-on-failure");
  {
    // Pattern f(x, g(x))  Subject f(a, g(b))  -- the first child binds
    // x=a, then the second child's recursion finds x already bound to
    // a but subj is b -> the inconsistency rewinds the binding.  At
    // the entry to ft_match the watermark was 0; on failure it should
    // be back to 0.
    AtpFt arena;
    ft_init(&arena);
    AtpFtCell *vx_a = ftnew_var(&arena, 0u, 0);
    AtpFtCell *vx_b = ftnew_var(&arena, 0u, 0);   // same id
    AtpFtCell *gx_k[1] = {vx_b};
    AtpFtCell *gx     = ftnew_ctr(&arena, 8u, 1u, gx_k, 0);
    AtpFtCell *pk[2]  = {vx_a, gx};
    AtpFtCell *pat    = ftnew_ctr(&arena, 5u, 2u, pk, 0);

    AtpFtCell *a    = ftnew_const(&arena, 1u, 0);
    AtpFtCell *b    = ftnew_const(&arena, 2u, 0);
    AtpFtCell *gb_k[1] = {b};
    AtpFtCell *gb     = ftnew_ctr(&arena, 8u, 1u, gb_k, 0);
    AtpFtCell *sk[2]  = {a, gb};
    AtpFtCell *subj   = ftnew_ctr(&arena, 5u, 2u, sk, 0);

    AtpFtSubst s = {0};
    CHECK(!ft_match(pat, subj, &s));
    CHECK_EQ(s.wm, 0u);
    CHECK(s.bind[0] == NULL);

    // Now show that a nested-match style save+restore preserves the
    // caller's pre-existing bindings: install y=c then trigger the
    // failing match; verify y stays bound.
    AtpFtCell *vy   = ftnew_var(&arena, 1u, 0);
    AtpFtCell *c    = ftnew_const(&arena, 3u, 0);
    ft_subst_reset(&s);
    // Manually install y=c so it lives below the watermark of the
    // upcoming match attempt.
    CHECK(ft_match(vy, c, &s));
    CHECK_EQ(s.wm, 1u);
    // Now attempt the failing match -- entry wm=1, should restore to 1.
    CHECK(!ft_match(pat, subj, &s));
    CHECK_EQ(s.wm, 1u);
    CHECK(s.bind[1] == c);
    CHECK(s.bind[0] == NULL);

    ft_destroy(&arena);
  }

  // ---- T7: ft_subst_apply on tmpl=f(x, g(y)) with x=a, y=b ----------
  TEST_BEGIN("ft_subst_apply/basic");
  {
    AtpFt arena;
    ft_init(&arena);
    // Template f(x, g(y)).
    AtpFtCell *tx   = ftnew_var(&arena, 0u, 0);
    AtpFtCell *ty   = ftnew_var(&arena, 1u, 0);
    AtpFtCell *gy_k[1] = {ty};
    AtpFtCell *gy   = ftnew_ctr(&arena, 8u, 1u, gy_k, 0);
    AtpFtCell *tk[2] = {tx, gy};
    AtpFtCell *tmpl = ftnew_ctr(&arena, 5u, 2u, tk, 0);

    // Bindings x=a, y=b.
    AtpFtCell *a   = ftnew_const(&arena, 11u, 0);
    AtpFtCell *b   = ftnew_const(&arena, 22u, 0);
    AtpFtSubst s = {0};
    s.bind[0]               = a;
    s.bind[1]               = b;
    s.bound_ids[s.wm++]     = 0u;
    s.bound_ids[s.wm++]     = 1u;

    AtpFtCell *out = ft_subst_apply(&arena, tmpl, &s, 0);
    CHECK(out != NULL);

    // Expected: f(a, g(b)).
    AtpFtCell *ea   = ftnew_const(&arena, 11u, 0);
    AtpFtCell *eb   = ftnew_const(&arena, 22u, 0);
    AtpFtCell *egk[1] = {eb};
    AtpFtCell *eg   = ftnew_ctr(&arena, 8u, 1u, egk, 0);
    AtpFtCell *ek[2] = {ea, eg};
    AtpFtCell *expected = ftnew_ctr(&arena, 5u, 2u, ek, 0);

    CHECK(ft_eq(out, expected));
    ft_destroy(&arena);
  }

  // ---- Helpers for the differential suites --------------------------
  //
  // The differential test compares ft_match (AtpFt-side) against
  // thvm_match (Term-side) on the SAME (pattern, subject) pair.  We
  // materialize one corpus of Term pairs, convert both to AtpFt, then
  // run both matchers and compare:
  //   * Boolean verdict identical.
  //   * On success, for every bound id, ft_to_term(ft_subst.bind[id])
  //     is structurally equal to thvm_match's subst.bindings[id].
  //
  // Var ids stay in [0, 8) per mk_random_term, well under
  // REWRITE_MAX_VAR=64 and ATPFT_MAX_VARS=64, so the matcher's slot
  // cap is never the source of a mismatch.

  enum { N_DIFF_SMALL = 1000, N_DIFF_STRESS = 100000, DEPTH = 4 };

  // ---- T8: 1k random pairs ------------------------------------------
  TEST_BEGIN("ft_match/diff-vs-thvm-match-1k");
  {
    AtpFt arena;
    ft_init(&arena);

    u32 seed = 0xc0ffee42u;
    u32 verdict_ok    = 0;
    u32 binding_ok    = 0;
    u32 n_succ        = 0;
    u32 n_total       = 0;
    AtpFtSubst f_subst = {0};   // zero-init once; ft_subst_reset reuses
    for (u32 i = 0; i < N_DIFF_SMALL; i++) {
      Term pt = mk_random_term(&seed, DEPTH);
      Term st = mk_random_term(&seed, DEPTH);
      AtpFtCell *pf = ft_from_term(&arena, pt, 0);
      AtpFtCell *sf = ft_from_term(&arena, st, 0);

      RewriteSubst t_subst = {{0}};
      u8 t_ok = thvm_match(pt, st, &t_subst);

      ft_subst_reset(&f_subst);
      int f_ok = ft_match(pf, sf, &f_subst);

      n_total += 1u;
      if (((int)t_ok) == f_ok) verdict_ok += 1u;
      if (t_ok && f_ok) {
        n_succ += 1u;
        // Walk every id touched by either matcher; compare the
        // recovered Term against the binding the Term matcher
        // produced.
        u32 mismatch = 0;
        for (u32 id = 0; id < ATPFT_MAX_VARS; id++) {
          Term       tb = t_subst.bindings[id];
          AtpFtCell *fb = f_subst.bind[id];
          if (tb == 0 && fb == NULL) continue;
          if (tb == 0 || fb == NULL) { mismatch = 1; break; }
          Term decoded = ft_to_term(fb);
          if (!term_struct_eq(decoded, tb)) { mismatch = 1; break; }
        }
        if (!mismatch) binding_ok += 1u;
      }
    }
    CHECK_EQ(verdict_ok, (u32)N_DIFF_SMALL);
    CHECK_EQ(binding_ok, n_succ);
    fprintf(stdout, "  ft_match/diff-1k: %u/%u verdicts, %u/%u bindings (n_succ=%u)\n",
            verdict_ok, n_total, binding_ok, n_succ, n_succ);
    ft_destroy(&arena);
  }

  // ---- T9: 100k random pairs ----------------------------------------
  TEST_BEGIN("ft_match/diff-vs-thvm-match-100k");
  {
    AtpFt arena;
    ft_init(&arena);

    u32 seed = 0xa11ce777u;
    u32 verdict_ok    = 0;
    u32 binding_ok    = 0;
    u32 n_succ        = 0;
    AtpFtSubst f_subst = {0};   // zero-init once; ft_subst_reset reuses
    for (u32 i = 0; i < N_DIFF_STRESS; i++) {
      Term pt = mk_random_term(&seed, DEPTH);
      Term st = mk_random_term(&seed, DEPTH);
      AtpFtCell *pf = ft_from_term(&arena, pt, 1);   // scratch arena
      AtpFtCell *sf = ft_from_term(&arena, st, 1);

      RewriteSubst t_subst = {{0}};
      u8 t_ok = thvm_match(pt, st, &t_subst);

      ft_subst_reset(&f_subst);
      int f_ok = ft_match(pf, sf, &f_subst);

      if (((int)t_ok) == f_ok) verdict_ok += 1u;
      if (t_ok && f_ok) {
        n_succ += 1u;
        u32 mismatch = 0;
        for (u32 id = 0; id < ATPFT_MAX_VARS; id++) {
          Term       tb = t_subst.bindings[id];
          AtpFtCell *fb = f_subst.bind[id];
          if (tb == 0 && fb == NULL) continue;
          if (tb == 0 || fb == NULL) { mismatch = 1; break; }
          Term decoded = ft_to_term(fb);
          if (!term_struct_eq(decoded, tb)) { mismatch = 1; break; }
        }
        if (!mismatch) binding_ok += 1u;
      }

      // Scratch arena pressure release: reset after a batch so the
      // bump pointer doesn't grow without bound.  AtpFtCell* pointers
      // handed out before the reset are invalid afterwards, so this
      // must happen AFTER we've consumed pf/sf for this iter and
      // BEFORE the next iter allocs.
      if ((i & 1023u) == 1023u) {
        ft_scratch_reset(&arena);
      }
    }
    CHECK_EQ(verdict_ok, (u32)N_DIFF_STRESS);
    CHECK_EQ(binding_ok, n_succ);
    fprintf(stdout, "  ft_match/diff-100k: %u/%d verdicts, %u/%u bindings\n",
            verdict_ok, N_DIFF_STRESS, binding_ok, n_succ);
    ft_destroy(&arena);
  }

  thvm_free();
  TEST_REPORT();
}
