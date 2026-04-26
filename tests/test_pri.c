// test_pri.c -- stage 8.1b: TAG_PRI primitive function call.
//
// 4-6 cases covering term_new_pri, prim_register / prim_fun /
// prim_arity, and APP-PRI argument accumulation + invocation.

#include "../src/thvm.c"
#include "test.h"

// === sample primitives ============================================
//
// All primitives have the same signature: `Term fn(Term *args)`.
// Each test below registers what it needs into a unique prim_id.

// Arity 1: identity.
static Term prim_identity(Term *args) {
  return args[0];
}

// Arity 2: build a CTR(7, [a, b]).
static Term prim_pair_ctr7(Term *args) {
  Term cs[2] = {args[0], args[1]};
  return term_new_ctr(7u, cs, 2);
}

// Arity 3: pick the middle arg (drops the others).
static Term prim_pick_mid(Term *args) {
  return args[1];
}

int main(void) {
  thvm_init();

  TEST_BEGIN("pri/term_new_pri-tag-and-ext");
  {
    Term t = term_new_pri(42u);
    CHECK_EQ(term_tag(t), TAG_PRI);
    CHECK_EQ(term_ext(t), 42u);
    CHECK_EQ(term_val(t), 0u);   // no heap on a fresh PRI
  }

  TEST_BEGIN("pri/registry-roundtrip");
  {
    u32 id = prim_register(0u, prim_identity, 1u);
    CHECK_EQ(id, 0u);
    CHECK(prim_fun(0u) == prim_identity);
    CHECK_EQ(prim_arity(0u), 1u);
    // Out-of-range id returns NULL / 0 cleanly.
    CHECK(prim_fun(PRIM_TABLE_CAP) == NULL);
    CHECK_EQ(prim_arity(PRIM_TABLE_CAP + 5u), 0u);
  }

  TEST_BEGIN("pri/app-pri-arity-1-fires");
  {
    // identity primitive: APP(PRI(1), arg) -> arg.
    prim_register(1u, prim_identity, 1u);
    Term arg  = term_new_ctr(99u, NULL, 0);
    u64  loc  = heap_alloc(2);
    heap_set(loc + 0, term_new_pri(1u));
    heap_set(loc + 1, arg);
    Term app  = term_new(0, TAG_APP, 0, loc);
    Term out  = wnf(app);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), 99u);
  }

  TEST_BEGIN("pri/app-pri-arity-2-saturates");
  {
    // pair_ctr7 primitive: APP(APP(PRI(2), a), b) -> CTR(7, [a, b]).
    prim_register(2u, prim_pair_ctr7, 2u);
    Term a = term_new_ctr(11u, NULL, 0);
    Term b = term_new_ctr(22u, NULL, 0);

    // First APP: builds APP(PRI(2), a).  Should evaluate to a
    // partial PRI carrying [count=1, a].
    u64  inner_loc = heap_alloc(2);
    heap_set(inner_loc + 0, term_new_pri(2u));
    heap_set(inner_loc + 1, a);
    Term inner_app = term_new(0, TAG_APP, 0, inner_loc);

    u64  outer_loc = heap_alloc(2);
    heap_set(outer_loc + 0, inner_app);
    heap_set(outer_loc + 1, b);
    Term outer_app = term_new(0, TAG_APP, 0, outer_loc);

    Term out = wnf(outer_app);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), 7u);
    CHECK_EQ(term_ctr_n(out), 2u);
    CHECK_EQ(term_ext(term_ctr_at(out, 0)), 11u);
    CHECK_EQ(term_ext(term_ctr_at(out, 1)), 22u);
  }

  TEST_BEGIN("pri/app-pri-arity-3-saturates");
  {
    // pick_mid primitive of arity 3: returns args[1].
    prim_register(3u, prim_pick_mid, 3u);
    Term a = term_new_ctr(1u, NULL, 0);
    Term b = term_new_ctr(2u, NULL, 0);
    Term c = term_new_ctr(3u, NULL, 0);

    u64  l1 = heap_alloc(2);
    heap_set(l1 + 0, term_new_pri(3u));
    heap_set(l1 + 1, a);
    Term step1 = term_new(0, TAG_APP, 0, l1);

    u64  l2 = heap_alloc(2);
    heap_set(l2 + 0, step1);
    heap_set(l2 + 1, b);
    Term step2 = term_new(0, TAG_APP, 0, l2);

    u64  l3 = heap_alloc(2);
    heap_set(l3 + 0, step2);
    heap_set(l3 + 1, c);
    Term step3 = term_new(0, TAG_APP, 0, l3);

    Term out = wnf(step3);
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), 2u);   // middle arg
  }

  TEST_BEGIN("pri/unregistered-id-falls-through-to-ERA");
  {
    // PRI with no registered fn: arity_lookup returns 0, so
    // APP-PRI saturates immediately (count >= 0) and the
    // null-fn branch returns ERA.
    Term t   = term_new_pri(50u);   // unregistered slot
    Term arg = term_new_ctr(0u, NULL, 0);
    u64  loc = heap_alloc(2);
    heap_set(loc + 0, t);
    heap_set(loc + 1, arg);
    Term app = term_new(0, TAG_APP, 0, loc);
    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_ERA);
  }

  // === Stage 8.1c: ATP_PRIM_UNIFY_APPLY round-trip =================

  // Helper: build APP(APP(PRI(id), s), t) and reduce.
  // We can't use a static helper that calls wnf cleanly due to
  // the top-level test_pri layout, so this shape is repeated
  // in each test case below.

  TEST_BEGIN("pri/unify-apply/var-ctr");
  {
    // (f(x), f(a)) unifies with σ = {x -> a}; thvm_unify_apply(s)
    // returns f(a).
    AtpState *s = thvm_atp_init(NULL, 0);   // registers primitive

    Term a = term_new_ctr(20u, NULL, 0);              // a
    Term s_arg = term_new_ctr(30u, (Term[]){term_new_fvr(0u)}, 1);  // f(x)
    Term t_arg = term_new_ctr(30u, (Term[]){a}, 1);   // f(a)

    u64 l1 = heap_alloc(2);
    heap_set(l1 + 0, term_new_pri(ATP_PRIM_UNIFY_APPLY));
    heap_set(l1 + 1, s_arg);
    Term step1 = term_new(0, TAG_APP, 0, l1);

    u64 l2 = heap_alloc(2);
    heap_set(l2 + 0, step1);
    heap_set(l2 + 1, t_arg);
    Term step2 = term_new(0, TAG_APP, 0, l2);

    Term out = wnf(step2);
    // Expect CTR(30, [CTR(20, [])]) -- f(a).
    CHECK_EQ(term_tag(out), TAG_CTR);
    CHECK_EQ(term_ext(out), 30u);
    CHECK_EQ(term_ctr_n(out), 1u);
    CHECK_EQ(term_ext(term_ctr_at(out, 0)), 20u);

    thvm_atp_free(s);
  }

  TEST_BEGIN("pri/unify-apply/incompatible-ctrs-give-ERA");
  {
    // (f(_), g(_)) cannot unify (different head symbols).
    AtpState *s = thvm_atp_init(NULL, 0);

    Term s_arg = term_new_ctr(30u, (Term[]){term_new_fvr(0u)}, 1);  // f(x)
    Term t_arg = term_new_ctr(31u, (Term[]){term_new_fvr(1u)}, 1);  // g(y)

    u64 l1 = heap_alloc(2);
    heap_set(l1 + 0, term_new_pri(ATP_PRIM_UNIFY_APPLY));
    heap_set(l1 + 1, s_arg);
    Term step1 = term_new(0, TAG_APP, 0, l1);

    u64 l2 = heap_alloc(2);
    heap_set(l2 + 0, step1);
    heap_set(l2 + 1, t_arg);
    Term step2 = term_new(0, TAG_APP, 0, l2);

    Term out = wnf(step2);
    CHECK_EQ(term_tag(out), TAG_ERA);

    thvm_atp_free(s);
  }

  TEST_BEGIN("pri/unify-apply/identical-vars-trivial-success");
  {
    // (x, x) unifies with σ = {} (or σ = {x -> x}, idempotent).
    // thvm_unify_apply(x) just returns x.
    AtpState *s = thvm_atp_init(NULL, 0);

    Term v = term_new_fvr(7u);

    u64 l1 = heap_alloc(2);
    heap_set(l1 + 0, term_new_pri(ATP_PRIM_UNIFY_APPLY));
    heap_set(l1 + 1, v);
    Term step1 = term_new(0, TAG_APP, 0, l1);

    u64 l2 = heap_alloc(2);
    heap_set(l2 + 0, step1);
    heap_set(l2 + 1, v);
    Term step2 = term_new(0, TAG_APP, 0, l2);

    Term out = wnf(step2);
    CHECK_EQ(term_tag(out), TAG_FVR);
    CHECK_EQ(term_ext(out), 7u);

    thvm_atp_free(s);
  }

  thvm_free();
  TEST_REPORT();
}
