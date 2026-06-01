// test_ac.c -- AC declaration + canonical-form unit tests.
//
// Built with -DTHVM_ATP_AC.  Exercises atp_acinfo_compute,
// atp_ac_flatten, and atp_ac_canon on small Sheffer-like terms.

#include "../src/thvm.c"

#ifndef THVM_ATP_AC
#define THVM_ATP_AC 1
#endif

#include "../src/atp/ac.c"

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

  thvm_free();
  TEST_REPORT();
}
