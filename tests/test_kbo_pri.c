// test_kbo_pri.c -- stage 8.2b: thvm_kbo as a TAG_PRI primitive.
//
// Verifies that `prim_kbo` registered at ATP_PRIM_KBO can be
// invoked via APP-PRI evaluation, returning NUM(KboCmp) outcomes
// matching the C-side `thvm_kbo` for the same inputs.

#include "../src/thvm.c"
#include "test.h"

// Sample signature: e=1, i=2, f=3, a=4 (matches test_atp.c
// conventions so the cases line up).  Variable weights / precedences
// chosen so that f(x, e) > x and f(x, y) is incomparable with f(y, x).
static u32 demo_weights   [5] = {0, 1, 0, 1, 1};
static u32 demo_precedence[5] = {0, 2, 4, 3, 1};
static const KboConfig DEMO_CFG = {
  .weights     = demo_weights,
  .precedence  = demo_precedence,
  .n_labels    = 5,
  .var_weight  = 1,
};

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u
#define VAR_y 1u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_v(u32 id) { return term_new_fvr(id); }

// Helper: invoke prim_kbo_eq_ic via 2-step APP chain and wnf.
// The primitive itself either returns NUM directly (leaf cases)
// or returns an AND chain of self-recursive calls (CTR case)
// -- in either case wnf collapses it to a single NUM.
static Term ic_kbo_eq(Term s, Term t) {
  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_KBO_EQ_IC));
  heap_set(l1 + 1, s);
  Term step1 = term_new(0, TAG_APP, 0, l1);
  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, t);
  return wnf(term_new(0, TAG_APP, 0, l2));
}

// Helper: invoke prim_kbo via 3-step APP chain and return the wnf.
static Term ic_kbo(Term s, Term t, u32 cfg_id) {
  Term cid = term_new(0, TAG_NUM, DT_I32, cfg_id);

  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_KBO));
  heap_set(l1 + 1, s);
  Term step1 = term_new(0, TAG_APP, 0, l1);

  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, t);
  Term step2 = term_new(0, TAG_APP, 0, l2);

  u64 l3 = heap_alloc(2);
  heap_set(l3 + 0, step2);
  heap_set(l3 + 1, cid);
  Term step3 = term_new(0, TAG_APP, 0, l3);

  return wnf(step3);
}

int main(void) {
  thvm_init();

  // Register the demo config at id 0.  Also need to register the
  // primitive itself; thvm_atp_init does that, but we don't need
  // a saturation state here -- call atp_register_primitives by
  // proxy via thvm_atp_init.  Discard the state immediately.
  AtpState *bootstrap = thvm_atp_init(&DEMO_CFG, 0);
  CHECK(bootstrap != NULL);
  kbo_cfg_register(0u, &DEMO_CFG);

  TEST_BEGIN("kbo-pri/registry-roundtrip");
  {
    CHECK(kbo_cfg_get(0u) == &DEMO_CFG);
    // Out-of-range yields NULL.
    CHECK(kbo_cfg_get(KBO_CFG_TABLE_CAP) == NULL);
    CHECK(kbo_cfg_get(KBO_CFG_TABLE_CAP + 7u) == NULL);
  }

  TEST_BEGIN("kbo-pri/eq-outcome");
  {
    // s == t should give KBO_EQ.
    Term s = mk_f(mk_v(VAR_x), mk_e());
    Term t = mk_f(mk_v(VAR_x), mk_e());
    Term out = ic_kbo(s, t, 0u);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), (u32)KBO_EQ);
    // Parity: matches direct C call.
    CHECK_EQ((int)thvm_kbo(s, t, &DEMO_CFG), (int)KBO_EQ);
  }

  TEST_BEGIN("kbo-pri/gt-outcome");
  {
    // f(x, e) > x under our config (lhs is heavier and dominates).
    Term s = mk_f(mk_v(VAR_x), mk_e());
    Term t = mk_v(VAR_x);
    Term out = ic_kbo(s, t, 0u);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), (u32)KBO_GT);
    CHECK_EQ((int)thvm_kbo(s, t, &DEMO_CFG), (int)KBO_GT);
  }

  TEST_BEGIN("kbo-pri/lt-outcome");
  {
    // x < f(x, e) -- mirror image of the GT test.
    Term s = mk_v(VAR_x);
    Term t = mk_f(mk_v(VAR_x), mk_e());
    Term out = ic_kbo(s, t, 0u);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), (u32)KBO_LT);
    CHECK_EQ((int)thvm_kbo(s, t, &DEMO_CFG), (int)KBO_LT);
  }

  TEST_BEGIN("kbo-pri/un-outcome");
  {
    // f(x, y) vs f(y, x): incomparable -- neither side dominates.
    Term s = mk_f(mk_v(VAR_x), mk_v(VAR_y));
    Term t = mk_f(mk_v(VAR_y), mk_v(VAR_x));
    Term out = ic_kbo(s, t, 0u);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), (u32)KBO_UN);
    CHECK_EQ((int)thvm_kbo(s, t, &DEMO_CFG), (int)KBO_UN);
  }

  TEST_BEGIN("kbo-pri/unregistered-cfg-falls-through-to-ERA");
  {
    // cfg_id 7 has no config registered -- prim_kbo returns ERA.
    Term s = mk_a();
    Term t = mk_e();
    Term out = ic_kbo(s, t, 7u);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  // === Stage 8.2c: pure-IC kbo_eq via prim_kbo_eq_ic =================
  // Uses the `ic_kbo_eq` helper defined at file scope above.

  TEST_BEGIN("kbo-eq-ic/leaf-fvr-same-id");
  {
    Term out = ic_kbo_eq(mk_v(VAR_x), mk_v(VAR_x));
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 1u);
  }

  TEST_BEGIN("kbo-eq-ic/leaf-fvr-different-id");
  {
    Term out = ic_kbo_eq(mk_v(VAR_x), mk_v(VAR_y));
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 0u);
  }

  TEST_BEGIN("kbo-eq-ic/leaf-tag-mismatch");
  {
    // FVR vs CTR: different tags, immediate NUM(0).
    Term out = ic_kbo_eq(mk_v(VAR_x), mk_e());
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 0u);
  }

  TEST_BEGIN("kbo-eq-ic/nullary-ctr-same-label");
  {
    Term out = ic_kbo_eq(mk_e(), mk_e());
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 1u);
  }

  TEST_BEGIN("kbo-eq-ic/nullary-ctr-different-label");
  {
    Term out = ic_kbo_eq(mk_a(), mk_e());
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 0u);
  }

  TEST_BEGIN("kbo-eq-ic/binary-ctr-equal");
  {
    // f(x, e) vs f(x, e) -- recursion + AND(NUM(1), NUM(1)) = NUM(1).
    Term s = mk_f(mk_v(VAR_x), mk_e());
    Term t = mk_f(mk_v(VAR_x), mk_e());
    Term out = ic_kbo_eq(s, t);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 1u);
  }

  TEST_BEGIN("kbo-eq-ic/binary-ctr-second-child-differs");
  {
    // f(x, e) vs f(x, a) -- AND short-circuits on second child:
    // first NUM(1), second NUM(0); chain returns NUM(0).
    Term s = mk_f(mk_v(VAR_x), mk_e());
    Term t = mk_f(mk_v(VAR_x), mk_a());
    Term out = ic_kbo_eq(s, t);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 0u);
  }

  TEST_BEGIN("kbo-eq-ic/binary-ctr-first-child-differs");
  {
    // f(x, e) vs f(y, e) -- first child differs, AND short-circuits
    // immediately on NUM(0).
    Term s = mk_f(mk_v(VAR_x), mk_e());
    Term t = mk_f(mk_v(VAR_y), mk_e());
    Term out = ic_kbo_eq(s, t);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 0u);
  }

  TEST_BEGIN("kbo-eq-ic/nested-ctr-deep-equality");
  {
    // f(f(x, e), f(x, e)) vs same -- 3 levels of recursion.
    Term inner = mk_f(mk_v(VAR_x), mk_e());
    Term s = mk_f(inner, mk_f(mk_v(VAR_x), mk_e()));
    Term t = mk_f(mk_f(mk_v(VAR_x), mk_e()), mk_f(mk_v(VAR_x), mk_e()));
    Term out = ic_kbo_eq(s, t);
    CHECK_EQ(term_tag(out), TAG_NUM);
    CHECK_EQ((u32)term_val(out), 1u);
  }

  thvm_atp_free(bootstrap);
  thvm_free();
  TEST_REPORT();
}
