// test_icc.c - ICC type-flow primitives: TAG_BRI + TAG_ANN.
//
// Three reductions from icc_spec.md (TinyHVM/resources/gists):
//   APP (θx.body) arg = θx (APP body[x ← λ$k.x]  (ANN $k arg))
//   ANN val (λx.body) = λx (ANN (APP val $k) body[x ← θ$k.x])
//   ANN val (θx.body) = body[x ← val]                          (type erasure)
// plus DUP-BRI commutation (mirrors DUP-LAM).
//
// These are the actual ICC rules, not the "BRI ≡ LAM" alias TinyHVM
// shipped with.  Type erasure (ANN-BRI) is the cleanest demo: the
// annotation is consumed and the type's body is returned with x
// replaced by the value.

#include "../src/thvm.c"
#include "test.h"

// Build θx.body where body is a Term that may reference VAR(loc) for
// the bound x.  Returns the BRI Term and writes the binder loc out
// via *binder_out so the caller can construct VAR(*binder_out) for
// the bound x.
static Term build_bri(Term *binder_out) {
  u64 loc = heap_alloc(1);
  *binder_out = term_new(0, TAG_VAR, 0, loc);  // the bound x
  // body filled in by caller via heap_set(loc, ...).
  return term_new(0, TAG_BRI, 0, loc);
}

static Term build_lam(Term *binder_out) {
  u64 loc = heap_alloc(1);
  *binder_out = term_new(0, TAG_VAR, 0, loc);
  return term_new(0, TAG_LAM, 0, loc);
}

int main(void) {
  thvm_init();

  // === ANN-BRI: type erasure =======================================

  TEST_BEGIN("icc/ann-bri-type-erasure-on-identity");
  {
    // ANN(NUM(42), θx.x)  ->  x[x ← NUM(42)]  =  NUM(42)
    Term x;
    Term bri = build_bri(&x);
    heap_set(term_val(bri), x);  // body of θx is just x

    Term val  = term_new(0, TAG_NUM, 0, 42);
    Term ann  = term_new_ann(val, bri);
    u64  itrs0 = ITRS;
    Term r    = wnf(ann);
    CHECK_EQ(term_tag(r), TAG_NUM);
    CHECK_EQ(term_val(r), 42u);
    // Two interactions fire under the hood: ANN-BRI (type erasure)
    // plus the wnf of the typ (which is already WHNF, so no extra
    // beyond the dispatch).  Loose check.
    CHECK(ITRS - itrs0 >= 1);
  }

  TEST_BEGIN("icc/ann-bri-discards-typ-keeps-val");
  {
    // ANN(LAM_x.x, θy.y) -- the type θy.y "consumes" itself, the
    // bridge body is its own bound y, so y[y ← lam_xx] = lam_xx.
    Term x;
    Term lam_xx = build_lam(&x);
    heap_set(term_val(lam_xx), x);

    Term y;
    Term bri_yy = build_bri(&y);
    heap_set(term_val(bri_yy), y);

    Term ann = term_new_ann(lam_xx, bri_yy);
    Term r   = wnf(ann);
    CHECK_EQ(term_tag(r), TAG_LAM);
  }

  // === APP-BRI: returns a new BRI ==================================

  TEST_BEGIN("icc/app-bri-fires-and-returns-bri");
  {
    // APP (θx.x) NUM(7)
    // ICC: yields a fresh θx wrapping APP/ANN machinery; head tag
    // remains BRI, but the inner structure has changed.  The exact
    // arithmetic doesn't simplify (ICC is type-flow, not value-flow).
    Term x;
    Term bri = build_bri(&x);
    heap_set(term_val(bri), x);

    Term arg = term_new(0, TAG_NUM, 0, 7);
    u64  app_loc = heap_alloc(2);
    heap_set(app_loc + 0, bri);
    heap_set(app_loc + 1, arg);
    Term app = term_new(0, TAG_APP, 0, app_loc);

    u64  itrs0 = ITRS;
    Term r     = wnf(app);
    CHECK_EQ(term_tag(r), TAG_BRI);
    CHECK(ITRS - itrs0 >= 1);   // APP-BRI fired at least once
  }

  // === ANN-LAM: returns a new LAM ==================================

  TEST_BEGIN("icc/ann-lam-fires-and-returns-lam");
  {
    // ANN val (λx.x) -- not a real type-check, just verify the rule
    // fires and the head wraps in a new λ.
    Term x;
    Term lam_xx = build_lam(&x);
    heap_set(term_val(lam_xx), x);

    Term val  = term_new(0, TAG_NUM, 0, 99);
    Term ann  = term_new_ann(val, lam_xx);
    u64  itrs0 = ITRS;
    Term r    = wnf(ann);
    CHECK_EQ(term_tag(r), TAG_LAM);
    CHECK(ITRS - itrs0 >= 1);
  }

  // === DUP-BRI: commutation ========================================

  TEST_BEGIN("icc/dup-bri-commutes-into-two-bris");
  {
    // ! &7{F0, F1} = θx.x   ->   F0 = θx0.x0   F1 = θx1.x1
    Term x;
    Term bri = build_bri(&x);
    heap_set(term_val(bri), x);

    u64  dup_loc = heap_alloc(1);
    heap_set(dup_loc, bri);
    Term dp0 = term_new(0, TAG_DP0, 7, dup_loc);
    Term dp1 = term_new(0, TAG_DP1, 7, dup_loc);

    // Plain DPs are Levy-opaque under wnf; cnf is the readback that
    // fires DUP-BRI (commute).
    Term r0 = cnf(dp0);
    Term r1 = cnf(dp1);
    CHECK_EQ(term_tag(r0), TAG_BRI);
    CHECK_EQ(term_tag(r1), TAG_BRI);
  }

  // === stuck case ==================================================

  TEST_BEGIN("icc/ann-with-num-typ-stays-stuck");
  {
    // ANN(LAM, NUM) -- typ isn't LAM/BRI, no rule fires.
    Term x;
    Term lam_xx = build_lam(&x);
    heap_set(term_val(lam_xx), x);

    Term ann = term_new_ann(lam_xx, term_new(0, TAG_NUM, 0, 0));
    Term r   = wnf(ann);
    CHECK_EQ(term_tag(r), TAG_ANN);
  }

  thvm_free();
  TEST_REPORT();
}
