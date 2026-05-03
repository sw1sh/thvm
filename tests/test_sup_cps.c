// test_sup_cps.c -- stage 8.1d-ii: SUP-encoded CP fan-out demo.
//
// Demonstrates that a SUP of PRI-unify calls reduces to the same
// terms as C-side `thvm_unify_apply` would produce for each pair.
// This is the structural parity check the design memo
// (`docs/plans/sup_encoded_cps.md`) calls for.
//
// Caveat: APP-SUP shares its argument via a DUP, but our IC has
// no DUP-CTR or DUP-FVR yet, so passing CTR-shaped rule LHSs via
// APP-SUP gets stuck at DP0/DP1.  This demo therefore uses the
// "fully-applied PRI inside the SUP" pattern -- each child of the
// SUP is `APP(APP(PRI_unify, s_i), t_i)`, fully saturated and
// ready to fire when wnf'd.  An additional smaller test
// exercises APP-SUP itself with DUP-friendly NUM args.

#include "../src/thvm.c"
#include "test.h"

static Term mk_app(Term fun, Term arg) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, fun);
  heap_set(loc + 1, arg);
  return term_new(0, TAG_APP, 0, loc);
}

static Term mk_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

// Build a fully-saturated APP(APP(PRI_unify, s), t).
static Term mk_unify_call(Term s, Term t) {
  Term step1 = mk_app(term_new_pri(ATP_PRIM_UNIFY_APPLY), s);
  return mk_app(step1, t);
}

// Identity primitive used by the APP-SUP NUM demonstration.
static Term id_prim(Term *args) { return args[0]; }

// === expected-output helpers (C-side reference) ====================

static Term ref_unify_apply(Term s, Term t) {
  RewriteSubst subst = {{0}};
  if (!thvm_unify(s, t, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_unify_apply(s, &subst);
}

// Structural equality copied from kbo_eq's pattern; can't call
// kbo_eq directly because it's static in src/kbo/_.c.
static u8 term_eq(Term s, Term t) {
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      if (ns != term_ctr_n(t)) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!term_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

int main(void) {
  thvm_init();

  // Register the unify primitive (no AtpState here; just the
  // registry call).
  prim_register(ATP_PRIM_UNIFY_APPLY, prim_unify_apply, 2u);

  TEST_BEGIN("sup-cps/two-positions-both-unify");
  {
    // Two pairs that both unify:
    //   (f(x), f(a))      -> f(a)
    //   (g(y), g(b))      -> g(b)
    Term a = term_new_ctr(20u, NULL, 0);
    Term b = term_new_ctr(21u, NULL, 0);
    Term s_1 = term_new_ctr(30u, (Term[]){term_new_fvr(0u)}, 1);   // f(x)
    Term t_1 = term_new_ctr(30u, (Term[]){a}, 1);                  // f(a)
    Term s_2 = term_new_ctr(31u, (Term[]){term_new_fvr(1u)}, 1);   // g(y)
    Term t_2 = term_new_ctr(31u, (Term[]){b}, 1);                  // g(b)

    Term ref_1 = ref_unify_apply(s_1, t_1);
    Term ref_2 = ref_unify_apply(s_2, t_2);
    CHECK_EQ(term_tag(ref_1), TAG_CTR);
    CHECK_EQ(term_tag(ref_2), TAG_CTR);

    Term sup = mk_sup(7u, mk_unify_call(s_1, t_1), mk_unify_call(s_2, t_2));
    Term outcome = wnf(sup);
    CHECK_EQ(term_tag(outcome), TAG_SUP);
    CHECK_EQ(term_ext(outcome), 7u);
    Term out_1 = wnf(heap_read(term_val(outcome) + 0));
    Term out_2 = wnf(heap_read(term_val(outcome) + 1));
    CHECK(term_eq(out_1, ref_1));
    CHECK(term_eq(out_2, ref_2));
  }

  TEST_BEGIN("sup-cps/one-unifies-one-fails");
  {
    // Mixed: one pair unifies, the other has incompatible heads.
    //   (f(x), f(a))       -> f(a)
    //   (f(_), g(_))       -> ERA
    Term a = term_new_ctr(20u, NULL, 0);
    Term s_1 = term_new_ctr(30u, (Term[]){term_new_fvr(0u)}, 1);   // f(x)
    Term t_1 = term_new_ctr(30u, (Term[]){a}, 1);                  // f(a)
    Term s_2 = term_new_ctr(30u, (Term[]){term_new_fvr(0u)}, 1);   // f(x)
    Term t_2 = term_new_ctr(31u, (Term[]){term_new_fvr(1u)}, 1);   // g(y)

    Term sup = mk_sup(7u, mk_unify_call(s_1, t_1), mk_unify_call(s_2, t_2));
    (void)wnf(sup);
    Term out_1 = wnf(heap_read(term_val(sup) + 0));
    Term out_2 = wnf(heap_read(term_val(sup) + 1));
    CHECK_EQ(term_tag(out_1), TAG_CTR);
    CHECK_EQ(term_ext(out_1), 30u);
    CHECK_EQ(term_tag(out_2), TAG_ERA);
  }

  TEST_BEGIN("sup-cps/three-positions-mixed");
  {
    // Three pairs to demonstrate the encoding scales beyond two.
    // SUPs are binary; we nest: &L_outer{ p1, &L_inner{ p2, p3 } }.
    Term a = term_new_ctr(20u, NULL, 0);
    Term s_1 = term_new_fvr(0u);                                   // x
    Term t_1 = a;                                                  // a
    Term s_2 = term_new_ctr(30u, (Term[]){term_new_fvr(1u)}, 1);   // f(y)
    Term t_2 = term_new_ctr(30u, (Term[]){a}, 1);                  // f(a)
    Term s_3 = term_new_ctr(31u, NULL, 0);                         // g
    Term t_3 = term_new_ctr(32u, NULL, 0);                         // h

    Term ref_1 = ref_unify_apply(s_1, t_1);
    Term ref_2 = ref_unify_apply(s_2, t_2);
    Term ref_3 = ref_unify_apply(s_3, t_3);

    Term inner = mk_sup(8u, mk_unify_call(s_2, t_2), mk_unify_call(s_3, t_3));
    Term outer = mk_sup(7u, mk_unify_call(s_1, t_1), inner);
    (void)wnf(outer);
    Term out_1 = wnf(heap_read(term_val(outer) + 0));
    Term inner_term = wnf(heap_read(term_val(outer) + 1));
    CHECK_EQ(term_tag(inner_term), TAG_SUP);
    CHECK_EQ(term_ext(inner_term), 8u);
    Term out_2 = wnf(heap_read(term_val(inner_term) + 0));
    Term out_3 = wnf(heap_read(term_val(inner_term) + 1));
    CHECK(term_eq(out_1, ref_1));
    CHECK(term_eq(out_2, ref_2));
    CHECK_EQ(term_tag(out_3), TAG_ERA);
    CHECK_EQ(term_tag(ref_3), TAG_ERA);
  }

  TEST_BEGIN("sup-cps/app-sup-fan-out-with-num-args");
  {
    // Demonstrates APP-SUP commutation actually firing on a
    // SUP-of-partial-PRIs.  Args here are NUMs (DUP-friendly)
    // so the standard fan-out works.
    //
    // sup_partials = &L{ APP(PRI(40), 0), APP(PRI(40), 0) }
    // (each branch is a partial PRI with 1 of 1 args -- saturates
    // immediately and returns the arg via id_prim)
    prim_register(40u, id_prim, 1u);
    Term n1 = term_new(0, TAG_NUM, DT_INT32, 11);
    Term n2 = term_new(0, TAG_NUM, DT_INT32, 22);
    Term sup = mk_sup(9u, term_new_pri(40u), term_new_pri(40u));
    // APP(sup, n1) -- but PRI is arity 1 and the SUP children are
    // already constructed, so APP-SUP fan-out picks the per-child
    // arg via DUP.  We reuse n1 once; APP-SUP DUPs it to n1 and
    // n1 (NUM dups identically).
    (void)n2;   // unused on this path; kept for clarity
    Term app = mk_app(sup, n1);
    Term out = wnf(app);
    CHECK_EQ(term_tag(out), TAG_SUP);
    CHECK_EQ(term_ext(out), 9u);
    // SUP children carry DP-wrapped values (Levy-opaque under wnf);
    // cnf is the readback that resolves them.
    Term out_l = cnf(heap_read(term_val(out) + 0));
    Term out_r = cnf(heap_read(term_val(out) + 1));
    CHECK_EQ(term_tag(out_l), TAG_NUM);
    CHECK_EQ((u32)term_val(out_l), 11u);
    CHECK_EQ(term_tag(out_r), TAG_NUM);
    CHECK_EQ((u32)term_val(out_r), 11u);
  }

  thvm_free();
  TEST_REPORT();
}
