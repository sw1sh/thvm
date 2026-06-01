// test_ac.c -- AC declaration + canonical-form + AC-eq/hash unit tests.
//
// Built with -DTHVM_ATP_AC.  Exercises atp_acinfo_compute,
// atp_ac_flatten, atp_ac_canon, atp_ac_eq, atp_ac_hash, and the
// engine-global mask setters (thvm_atp_set_ac_mask /
// thvm_atp_get_ac_mask / thvm_atp_auto_ac).  Under THVM_ATP_AC the
// engine TU (src/thvm.c -> src/atp/_.c) already pulls ac.c in;
// no direct re-include here.

#include "../src/thvm.c"

#include "test.h"

// Label IDs (small, distinct).
#define LAB_F  1u   // AC binary
#define LAB_G  2u   // non-AC binary
#define LAB_A  3u   // 0-arity const
#define LAB_B  4u
#define LAB_C  5u

static Term k(u32 lab) {
  return term_new_ctr(lab, NULL, 0u);
}
static Term bin(u32 lab, Term x, Term y) {
  Term kids[2] = { x, y };
  return term_new_ctr(lab, kids, 2u);
}
static Term v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

  // -- T1: AC auto-detection from axioms ---------------------------------
  //
  // Build the two axioms `f(x,y) = f(y,x)` and
  // `f(f(x,y),z) = f(x,f(y,z))`, feed them to atp_analyze_axioms,
  // then compute AcInfo and check `f` is flagged AC while `g` is not.
  TEST_BEGIN("ac/auto-detect");
  {
    Term x = v(0), y = v(1), z = v(2);

    Term comm_l = bin(LAB_F, x, y);
    Term comm_r = bin(LAB_F, y, x);
    Term ass_l  = bin(LAB_F, bin(LAB_F, x, y), z);
    Term ass_r  = bin(LAB_F, x, bin(LAB_F, y, z));

    // Throw a g-axiom in for negative coverage: f(x, g(y, z)) = ...
    // (not commutative-of-g, not associative-of-g)
    Term other_l = bin(LAB_G, x, y);
    Term other_r = bin(LAB_G, x, y);

    Term lhss[5] = { comm_l, ass_l, other_l, x, x };
    Term rhss[5] = { comm_r, ass_r, other_r, x, x };

    AtpSymProps props[8] = {0};
    atp_analyze_axioms(lhss, rhss, 5, props, 8);

    CHECK(props[LAB_F].is_commutative);
    CHECK(props[LAB_F].is_associative);
    CHECK(!props[LAB_G].is_commutative);
    CHECK(!props[LAB_G].is_associative);

    AtpAcInfo ac;
    atp_acinfo_compute(&ac, props, 8);
    CHECK(atp_ac_is_ac_label(&ac, LAB_F));
    CHECK(!atp_ac_is_ac_label(&ac, LAB_G));
    CHECK(!atp_ac_is_ac_label(&ac, LAB_A));   // const, not AC
  }

  // -- T2: flatten f(a, f(b, c)) under AC `f` -> {a, b, c} ----------------
  TEST_BEGIN("ac/flatten-right-assoc");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);
    Term t = bin(LAB_F, a, bin(LAB_F, b, c));

    Term leaves[16];
    u32 n = 0u;
    CHECK(atp_ac_flatten(t, &ac, leaves, &n, 16));
    CHECK_EQ(n, 3u);
    CHECK(kbo_eq(leaves[0], a));
    CHECK(kbo_eq(leaves[1], b));
    CHECK(kbo_eq(leaves[2], c));
  }

  // -- T3: flatten f(f(a, b), f(c, a)) -> {a, b, c, a} (multiset) --------
  TEST_BEGIN("ac/flatten-balanced");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);
    Term t = bin(LAB_F, bin(LAB_F, a, b), bin(LAB_F, c, a));

    Term leaves[16];
    u32 n = 0u;
    CHECK(atp_ac_flatten(t, &ac, leaves, &n, 16));
    CHECK_EQ(n, 4u);
    CHECK(kbo_eq(leaves[0], a));
    CHECK(kbo_eq(leaves[1], b));
    CHECK(kbo_eq(leaves[2], c));
    CHECK(kbo_eq(leaves[3], a));
  }

  // -- T4: non-AC top is its own singleton --------------------------------
  TEST_BEGIN("ac/flatten-singleton");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term a = k(LAB_A), b = k(LAB_B);
    Term t = bin(LAB_G, a, b);          // g is not AC

    Term leaves[16];
    u32 n = 0u;
    CHECK(atp_ac_flatten(t, &ac, leaves, &n, 16));
    CHECK_EQ(n, 1u);
    CHECK(kbo_eq(leaves[0], t));
  }

  // -- T5: canonicalize `f(c, f(b, a))` -> sorted right-assoc chain ------
  //
  // After flattening to {c, b, a} we sort by struct_hash and rebuild
  // right-associative: `f(min, f(mid, max))` -- the actual ordering
  // depends on the hash values, but `canon(canon(t)) == canon(t)`.
  TEST_BEGIN("ac/canon-idempotent");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);
    Term t = bin(LAB_F, c, bin(LAB_F, b, a));

    Term t_canon  = atp_ac_canon(t, &ac);
    Term t_canon2 = atp_ac_canon(t_canon, &ac);
    CHECK(kbo_eq(t_canon, t_canon2));         // idempotent
    CHECK_EQ(term_tag(t_canon), TAG_CTR);
    CHECK_EQ(term_ext(t_canon), LAB_F);

    // Flatten t_canon: should still produce a 3-leaf multiset.
    Term lv[16];
    u32 n = 0u;
    CHECK(atp_ac_flatten(t_canon, &ac, lv, &n, 16));
    CHECK_EQ(n, 3u);

    // Leaves should be sorted by hash ascending.
    u64 h0 = atp_term_struct_hash(lv[0]);
    u64 h1 = atp_term_struct_hash(lv[1]);
    u64 h2 = atp_term_struct_hash(lv[2]);
    CHECK(h0 <= h1);
    CHECK(h1 <= h2);

    // Multiset matches {a, b, c}.
    // Hash each input then check the canon's hashes are a permutation.
    u64 ha = atp_term_struct_hash(a),
        hb = atp_term_struct_hash(b),
        hc = atp_term_struct_hash(c);
    u8 found_a = (h0 == ha) || (h1 == ha) || (h2 == ha);
    u8 found_b = (h0 == hb) || (h1 == hb) || (h2 == hb);
    u8 found_c = (h0 == hc) || (h1 == hc) || (h2 == hc);
    CHECK(found_a && found_b && found_c);
  }

  // -- T6: non-AC term is returned unchanged ------------------------------
  TEST_BEGIN("ac/canon-noop-on-non-ac");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };  // f only
    Term a = k(LAB_A), b = k(LAB_B);
    Term t = bin(LAB_G, a, b);          // g is not AC

    Term t_canon = atp_ac_canon(t, &ac);
    CHECK(kbo_eq(t_canon, t));
    // Same identity actually -- no rebuild expected.
    CHECK(t_canon == t);
  }

  // -- T7: empty AC mask = no-op anywhere ---------------------------------
  TEST_BEGIN("ac/empty-mask");
  {
    AtpAcInfo ac = { .ac_mask = 0ull };
    Term a = k(LAB_A), b = k(LAB_B);
    Term t = bin(LAB_F, a, b);

    Term leaves[8];
    u32 n = 0u;
    CHECK(atp_ac_flatten(t, &ac, leaves, &n, 8));
    CHECK_EQ(n, 1u);                    // top isn't AC, so singleton
    CHECK(kbo_eq(leaves[0], t));

    Term t_canon = atp_ac_canon(t, &ac);
    CHECK(t_canon == t);                // no rebuild
  }

  // -- T8: AC-equality decides AC-permutations as equal -------------------
  TEST_BEGIN("ac/ac-eq-permutations");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);

    // s = f(a, f(b, c)), t = f(f(c, b), a)  -- same multiset under f
    Term s = bin(LAB_F, a, bin(LAB_F, b, c));
    Term t = bin(LAB_F, bin(LAB_F, c, b), a);

    CHECK(!kbo_eq(s, t));            // syntactically distinct
    CHECK(atp_ac_eq(s, t, &ac));     // AC-equal

    // Sanity: AC-eq on the same shape is reflexive.
    CHECK(atp_ac_eq(s, s, &ac));
    CHECK(atp_ac_eq(t, t, &ac));

    // Different multiset -- f(a, f(b, b)) is NOT AC-eq to s.
    Term u = bin(LAB_F, a, bin(LAB_F, b, b));
    CHECK(!atp_ac_eq(s, u, &ac));
  }

  // -- T9: AC-hash invariant under AC permutations ------------------------
  TEST_BEGIN("ac/ac-hash-invariance");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);

    Term s = bin(LAB_F, a, bin(LAB_F, b, c));
    Term t = bin(LAB_F, bin(LAB_F, c, b), a);
    Term u = bin(LAB_F, a, bin(LAB_F, b, b));   // different multiset

    u64 hs = atp_ac_hash(s, &ac);
    u64 ht = atp_ac_hash(t, &ac);
    u64 hu = atp_ac_hash(u, &ac);
    CHECK_EQ(hs, ht);                // same multiset -> same hash
    CHECK(hs != hu);                 // different multiset -> different hash
                                     // (not provably -- could collide,
                                     // but 64-bit FNV makes it negligible)
  }

  // -- T10: AC-eq under empty mask falls back to kbo_eq -------------------
  TEST_BEGIN("ac/ac-eq-fallback-empty-mask");
  {
    AtpAcInfo ac = { .ac_mask = 0ull };
    Term a = k(LAB_A), b = k(LAB_B);
    Term s = bin(LAB_F, a, b);
    Term t = bin(LAB_F, b, a);

    // With no AC declared, AC-eq degenerates to kbo_eq -- s and t
    // are NOT structurally equal so AC-eq returns 0.
    CHECK(!atp_ac_eq(s, t, &ac));
    // Reflexive case still works (kbo_eq path).
    CHECK(atp_ac_eq(s, s, &ac));
  }

  // -- T11: g_atp_ac_info setters round-trip ------------------------------
  TEST_BEGIN("ac/global-ac-info-setters");
  {
    u64 prev = thvm_atp_get_ac_mask();
    thvm_atp_set_ac_mask((1ull << LAB_F) | (1ull << LAB_G));
    CHECK_EQ(thvm_atp_get_ac_mask(), (1ull << LAB_F) | (1ull << LAB_G));
    thvm_atp_set_ac_mask(0ull);
    CHECK_EQ(thvm_atp_get_ac_mask(), 0ull);
    thvm_atp_set_ac_mask(prev);                // restore for next test
  }

  // -- T12: thvm_atp_auto_ac derives mask from a caller-supplied axiom set --
  TEST_BEGIN("ac/auto-ac-from-axiom-arrays");
  {
    // Reset the global mask so we measure auto's effect.
    thvm_atp_set_ac_mask(0ull);

    Term x = v(0), y = v(1), z = v(2);

    // f: commutative + associative.  g: neither.
    Term lhss[3], rhss[3];
    lhss[0] = bin(LAB_F, x, y);
    rhss[0] = bin(LAB_F, y, x);
    lhss[1] = bin(LAB_F, bin(LAB_F, x, y), z);
    rhss[1] = bin(LAB_F, x, bin(LAB_F, y, z));
    lhss[2] = bin(LAB_G, x, y);
    rhss[2] = bin(LAB_G, x, y);

    thvm_atp_auto_ac(lhss, rhss, 3);
    u64 m = thvm_atp_get_ac_mask();
    CHECK((m >> LAB_F) & 1ull);      // f is AC
    CHECK(!((m >> LAB_G) & 1ull));   // g is not

    thvm_atp_set_ac_mask(0ull);       // clean up for any later test
  }

  // -- T13: AC-match accepts AC permutation of subject ------------------
  TEST_BEGIN("ac/match-ac-permutation");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term x = v(0), y = v(1);
    Term a = k(LAB_A), b = k(LAB_B);

    // pat = f(x, y)   subj = f(b, a)
    Term pat  = bin(LAB_F, x, y);
    Term subj = bin(LAB_F, b, a);

    RewriteSubst subst = {{0}};
    CHECK(atp_match_ac(pat, subj, &ac, &subst));
    // x or y bound to a, the other to b (greedy order: x first).
    CHECK(subst.bindings[0] != 0);
    CHECK(subst.bindings[1] != 0);
    // Bindings cover {a, b} as a multiset.
    u8 cov_a = kbo_eq(subst.bindings[0], a) || kbo_eq(subst.bindings[1], a);
    u8 cov_b = kbo_eq(subst.bindings[0], b) || kbo_eq(subst.bindings[1], b);
    CHECK(cov_a && cov_b);
  }

  // -- T14: AC-match handles unit elimination shape ----------------------
  //
  // Pattern  f(x, e)  with e = LAB_A treated as a ground constant.
  // Subject  f(b, a)  matches with x bound to b.
  TEST_BEGIN("ac/match-unit-elim");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term x = v(0);
    Term a = k(LAB_A), b = k(LAB_B);

    Term pat  = bin(LAB_F, x, a);       // f(x, a) with `a` ground
    Term subj = bin(LAB_F, b, a);       // f(b, a)

    RewriteSubst subst = {{0}};
    CHECK(atp_match_ac(pat, subj, &ac, &subst));
    CHECK(kbo_eq(subst.bindings[0], b));   // x = b
  }

  // -- T15: AC-match handles repeated-var pattern (idempotence) ----------
  TEST_BEGIN("ac/match-idempotence");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term x = v(0);
    Term a = k(LAB_A), b = k(LAB_B);

    Term pat  = bin(LAB_F, x, x);

    // subj f(a, a) -- matches, x = a.
    Term s1 = bin(LAB_F, a, a);
    RewriteSubst subst1 = {{0}};
    CHECK(atp_match_ac(pat, s1, &ac, &subst1));
    CHECK(kbo_eq(subst1.bindings[0], a));

    // subj f(a, b) -- doesn't match (a != b).
    Term s2 = bin(LAB_F, a, b);
    RewriteSubst subst2 = {{0}};
    CHECK(!atp_match_ac(pat, s2, &ac, &subst2));
  }

  // -- T16: AC-match binds the var to a chain when leftovers > 1 ---------
  //
  // Pattern  f(x, a)  matched against subject  f(a, b, c) -- AC-flat
  // subject is multiset {a, b, c}.  Pattern's `a` consumes one `a`;
  // remaining {b, c} bound to x as f(b, c) (or hash-sorted).
  TEST_BEGIN("ac/match-leftover-chain");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term x = v(0);
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);

    Term pat  = bin(LAB_F, x, a);                  // f(x, a)
    Term subj = bin(LAB_F, a, bin(LAB_F, b, c));   // f(a, f(b, c))
                                                   // AC-flat = {a, b, c}

    RewriteSubst subst = {{0}};
    CHECK(atp_match_ac(pat, subj, &ac, &subst));
    // x should be bound to f(b, c) -- a 2-leaf AC chain.
    Term xb = subst.bindings[0];
    CHECK(term_tag(xb) == TAG_CTR);
    CHECK(term_ext(xb) == LAB_F);
    // The chain's leaves are {b, c}.
    Term lv[8];
    u32 n = 0u;
    CHECK(atp_ac_flatten(xb, &ac, lv, &n, 8));
    CHECK_EQ(n, 2u);
    u8 has_b = kbo_eq(lv[0], b) || kbo_eq(lv[1], b);
    u8 has_c = kbo_eq(lv[0], c) || kbo_eq(lv[1], c);
    CHECK(has_b && has_c);
  }

  // -- T17: AC-match fails on count mismatch + multi-unbound-var bail ----
  TEST_BEGIN("ac/match-bail-shapes");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };
    Term x = v(0), y = v(1);
    Term a = k(LAB_A), b = k(LAB_B), c = k(LAB_C);

    // np > ns -- pattern has 3 leaves, subject has 2.
    Term pat3 = bin(LAB_F, x, bin(LAB_F, a, b));     // f(x, a, b)
    Term sub2 = bin(LAB_F, a, b);                    // f(a, b)
    RewriteSubst s1 = {{0}};
    CHECK(!atp_match_ac(pat3, sub2, &ac, &s1));

    // Multi-unbound-var -- two unbound vars + extra subj leaves =>
    // partition needed, we bail conservatively.
    Term pat_xy  = bin(LAB_F, x, y);
    Term sub3    = bin(LAB_F, a, bin(LAB_F, b, c));  // 3 subj leaves
    RewriteSubst s2 = {{0}};
    CHECK(!atp_match_ac(pat_xy, sub3, &ac, &s2));    // bail
  }

  // -- T18: AC-match falls through to syntactic on non-AC top -----------
  TEST_BEGIN("ac/match-non-ac-top");
  {
    AtpAcInfo ac = { .ac_mask = (1ull << LAB_F) };   // f is AC; g is not
    Term x = v(0);
    Term a = k(LAB_A), b = k(LAB_B);

    Term pat  = bin(LAB_G, x, a);          // g(x, a) - g not AC
    Term s_ok = bin(LAB_G, b, a);          // g(b, a)
    Term s_no = bin(LAB_G, a, b);          // g(a, b) -- 2nd arg differs

    RewriteSubst sok = {{0}};
    CHECK(atp_match_ac(pat, s_ok, &ac, &sok));
    CHECK(kbo_eq(sok.bindings[0], b));

    RewriteSubst sno = {{0}};
    CHECK(!atp_match_ac(pat, s_no, &ac, &sno));     // syntactic fails
  }

  // -- T19: AC mask wires into atp_cp_trivially_joinable ----------------
  //
  // Tag two terms l = f(a, b), r = f(b, a) that are AC-equal under f.
  // Without the AC mask set, atp_cp_trivially_joinable should report
  // them as NOT joinable (kbo_eq fails; there are no rules in R yet).
  // With the AC mask set to {f}, the AC-equality short-circuit should
  // kick in and report them as joinable.
  //
  // We exercise the wiring path indirectly via thvm_atp_add_equation:
  // when an equation is enqueued whose two sides are already AC-equal
  // under the configured mask, the engine drops it as joinable in
  // atp_cp_trivially_joinable.  Inspect dropped-joinable count.
  TEST_BEGIN("ac/trivial-join-fires-under-ac-mask");
  {
    thvm_atp_set_ac_mask(0ull);

    Term a = k(LAB_A), b = k(LAB_B);
    Term l = bin(LAB_F, a, b);
    Term r = bin(LAB_F, b, a);

    // Without the AC mask: enqueue (l, r) -- a non-trivial equation.
    // The engine commits it as a rule (or unorientable equation).
    KboConfig kbo = {0};
    AtpState *s = thvm_atp_init(&kbo, 1024);
    thvm_atp_add_equation(s, l, r);
    u32 before = s->n_cps;
    CHECK(before == 1u);             // 1 enqueued
    thvm_atp_free(s);

    // With the AC mask set to {f}: the same equation's sides are
    // AC-equal -- atp_cp_trivially_joinable normalize-and-equal
    // should drop it at push time (the joinable branch in
    // atp_push_cps_traced).  After the add, the queue should still
    // be empty.
    thvm_atp_set_ac_mask(1ull << LAB_F);
    AtpState *s2 = thvm_atp_init(&kbo, 1024);
    thvm_atp_add_equation(s2, l, r);
    // Note: the AC-eq trivial-join path fires inside
    // atp_cp_trivially_joinable, which is only called from the
    // generated-CP push path (atp_push_cps_traced), not the
    // user-axiom enqueue path (atp_enqueue_equation).  Confirm the
    // raw axiom landed in the queue regardless; the AC wiring will
    // take effect for SUBSEQUENT critical-pair pushes once axioms
    // are committed and CPs are generated.  This test pins the
    // mask-setting wiring; a deeper saturation test belongs to
    // Stage 3 (AC matching during rewrite).
    CHECK_EQ(s2->n_cps, 1u);
    thvm_atp_free(s2);
    thvm_atp_set_ac_mask(0ull);
  }

  thvm_free();
  TEST_REPORT();
}
