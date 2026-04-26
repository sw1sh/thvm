// test_cp.c - critical-pair enumeration on TAG_CTR + TAG_FVR.
//
// Stage 4 demo (docs/plans/waldmeister_ic_atp.md): given the rule
// set { f(e, x) = x ;  f(f(x, y), z) = f(x, f(y, z)) }, the
// non-trivial CP comes from overlapping the left-id rule into the
// [0] position of the assoc rule's LHS, yielding (f(y, z), f(e, f(y, z))).

#include "../src/thvm.c"
#include "test.h"

#define LAB_e 1u
#define LAB_f 3u
#define VAR_x 0u
#define VAR_y 1u
#define VAR_z 2u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_v(u32 id) { return term_new_fvr(id); }

// Search for a CP with structure (f(y, z), f(e, f(y, z))) -- shape
// only, ignoring concrete var ids (which depend on rename offset).
// Returns 1 if any CP in `cps[0..n)` matches that pattern.
static u8 has_assoc_leftid_cp(const CriticalPair *cps, u32 n) {
  for (u32 i = 0; i < n; i++) {
    Term l = cps[i].lhs;
    Term r = cps[i].rhs;
    // l should be f(?, ?) -- both arguments are FVRs.
    if (term_tag(l) != TAG_CTR || term_ext(l) != LAB_f) continue;
    if (term_ctr_n(l) != 2) continue;
    if (term_tag(term_ctr_at(l, 0)) != TAG_FVR) continue;
    if (term_tag(term_ctr_at(l, 1)) != TAG_FVR) continue;
    // r should be f(e, f(?, ?))
    if (term_tag(r) != TAG_CTR || term_ext(r) != LAB_f) continue;
    if (term_ctr_n(r) != 2) continue;
    Term r0 = term_ctr_at(r, 0);
    Term r1 = term_ctr_at(r, 1);
    if (term_tag(r0) != TAG_CTR || term_ext(r0) != LAB_e) continue;
    if (term_tag(r1) != TAG_CTR || term_ext(r1) != LAB_f) continue;
    return 1;
  }
  return 0;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("cp/single-rule-self-overlap-trivial");
  {
    // Rule R: f(x, e) -> x.  Self-overlap at top is trivial; no CP
    // at sub-positions because [0] is FVR (x), [1] is e (a label
    // with arity 0 -- structural recursion stops at non-CTR).
    // Actually [1] = e IS a CTR, so the visit fires; but unifying
    // e with the renamed lhs f(x_32, e) fails (different labels).
    // Net: only the trivial top CP.
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_e()) };
    Term rhs[1] = { mk_v(VAR_x) };
    CriticalPair cps[16] = {{0}};
    u32 n = thvm_critical_pairs(lhs, rhs, 1, cps, 16);
    CHECK(n >= 1u);  // at least the trivial top-overlap
  }

  TEST_BEGIN("cp/assoc-and-leftid-produces-expected-cp");
  {
    // R1 (left-id): f(e, x) -> x
    // R2 (assoc):   f(f(x, y), z) -> f(x, f(y, z))
    //
    // Overlap at R2's [0] position (f(x, y)) with renamed R1's
    // f(e, x_32) unifies (x = e, y = x_32), yielding the CP
    //   ( σ(R2.lhs[[0] ← R1.rhs]) , σ(R2.rhs) )
    // = ( f(x_32, z),                f(e, f(x_32, z)) )
    Term lhs[2] = {
      mk_f(mk_e(), mk_v(VAR_x)),                                // f(e, x)
      mk_f(mk_f(mk_v(VAR_x), mk_v(VAR_y)), mk_v(VAR_z)),         // f(f(x,y), z)
    };
    Term rhs[2] = {
      mk_v(VAR_x),                                               // x
      mk_f(mk_v(VAR_x), mk_f(mk_v(VAR_y), mk_v(VAR_z))),         // f(x, f(y, z))
    };
    CriticalPair cps[32] = {{0}};
    u32 n = thvm_critical_pairs(lhs, rhs, 2, cps, 32);
    CHECK(n > 0u);
    CHECK(has_assoc_leftid_cp(cps, n));
  }

  TEST_BEGIN("cp/no-overlap-yields-only-trivial");
  {
    // R1: f(x, e) -> x   -- arity 2 with constant e
    // R2: f(x, y) where outer is some unrelated label -- no overlap
    // Just test self-overlap on R1 alone has only at-top CPs.
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_e()) };
    Term rhs[1] = { mk_v(VAR_x) };
    CriticalPair cps[16] = {{0}};
    u32 n = thvm_critical_pairs(lhs, rhs, 1, cps, 16);
    // Self-overlap at top always succeeds (unifies via renaming).
    // Self-overlap at [0] = x is FVR, skipped.
    // Self-overlap at [1] = e: unify with renamed f(x_32, e) -- fails.
    CHECK_EQ(n, 1u);
  }

  TEST_BEGIN("cp/cap-truncates");
  {
    Term lhs[2] = {
      mk_f(mk_e(), mk_v(VAR_x)),
      mk_f(mk_f(mk_v(VAR_x), mk_v(VAR_y)), mk_v(VAR_z)),
    };
    Term rhs[2] = {
      mk_v(VAR_x),
      mk_f(mk_v(VAR_x), mk_f(mk_v(VAR_y), mk_v(VAR_z))),
    };
    CriticalPair cps[1] = {{0}};
    u32 n = thvm_critical_pairs(lhs, rhs, 2, cps, 1);
    CHECK_EQ(n, 1u);  // capped
  }

  thvm_free();
  TEST_REPORT();
}
