// test_fol.c -- First-order clause representation + binary resolution.

#include "../src/thvm.c"
#include "test.h"

// Predicate labels.
#define P_P  1u   // P(_)
#define P_Q  2u   // Q(_)
#define P_R  3u   // R(_,_)
// Object constants / functions.
#define L_a  10u
#define L_b  11u
#define L_f  12u
#define L_c  13u
#define L_d  14u

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term v(u32 id) { return term_new_fvr(id); }
static Term pred1(u32 lab, Term x) { Term cs[1] = {x}; return term_new_ctr(lab, cs, 1); }
static Term pred2(u32 lab, Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(lab, cs, 2); }

int main(void) {
  thvm_init();

  TEST_BEGIN("fol/empty-clause-detection");
  {
    FolClause *c = fol_clause_new(0);
    CHECK(c != NULL);
    CHECK(fol_clause_is_empty(c));
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/clause-eq-modulo-order");
  {
    // P(a) v ¬Q(b)  ==  ¬Q(b) v P(a)  (multiset of signed atoms).
    FolClause *a = fol_clause_new(2);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    a->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    FolClause *b = fol_clause_new(2);
    b->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    b->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_clause_eq(a, b));
    // Different sign on otherwise-equal atoms => not equal.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    c->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    CHECK(!fol_clause_eq(a, c));
    fol_clause_free(a);
    fol_clause_free(b);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/binary-resolution-ground");
  {
    // C1 = P(a)        (one positive literal)
    // C2 = ¬P(a) v Q(b) (negative P(a), positive Q(b))
    // Resolvent = Q(b).
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_Q, k(L_b))));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/binary-resolution-with-unification");
  {
    // C1 = P(x)         (variable x)
    // C2 = ¬P(a) v Q(b) (ground)
    // Resolvent (after σ = {x ↦ a}) = Q(b).
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    FolClause *c2 = fol_clause_new(2);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    c2->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_Q, k(L_b))));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/resolution-to-empty-clause");
  {
    // C1 = P(a)   C2 = ¬P(a).  Resolvent is the empty clause (FALSE).
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(fol_clause_is_empty(r));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/resolution-fails-same-sign");
  {
    // Two positives can't resolve directly.
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r == NULL);
    fol_clause_free(c1);
    fol_clause_free(c2);
  }

  TEST_BEGIN("fol/resolution-fails-non-unifying-atoms");
  {
    // P(a) and ¬P(b): different ground atoms => no unifier => no resolvent.
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r == NULL);
    fol_clause_free(c1);
    fol_clause_free(c2);
  }

  TEST_BEGIN("fol/resolution-with-shared-vars");
  {
    // C1 = P(x) v Q(x)   (x in both literals)
    // C2 = ¬P(a)
    // Resolvent: Q(x') where x' is the c1-side variable renamed (or
    // not -- here it's the c1's x, since c2 has no vars).
    // After σ = {x ↦ a}, resolvent = Q(a).
    FolClause *c1 = fol_clause_new(2);
    c1->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    c1->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_Q, k(L_a))));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/factoring");
  {
    // C = P(x) v P(a).  Factor at (0, 1): σ = {x ↦ a}, drop one P-lit.
    // Result = P(a).
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *r = fol_factor(c, 0, 1);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(r->lits[0].sign == 0);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/factoring-non-unifying");
  {
    // P(a) v P(b): no unifier on distinct grounds.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    FolClause *r = fol_factor(c, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/factoring-sign-mismatch");
  {
    // P(a) v ¬P(a): opposite signs -- factoring (same-polarity) fails.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_factor(c, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/two-place-predicate");
  {
    // C1 = R(x, b),  C2 = ¬R(a, y).  σ = {x ↦ a, y ↦ b'} (b in c2 renamed).
    // Actually y in c2 is FVR-id 0 too; after rename y becomes FVR-id
    // (FOL_RENAME_OFFSET).  σ unifies (x, b) with (a, y_renamed):
    //   x ↦ a, y_renamed ↦ b.  Empty resolvent.
    FolClause *c1 = fol_clause_new(1);
    c1->lits[0] = (FolLit){ .atom = pred2(P_R, v(0), k(L_b)), .sign = 0 };
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = pred2(P_R, k(L_a), v(0)), .sign = 1 };
    FolClause *r = fol_resolve(c1, 0, c2, 0);
    CHECK(r != NULL);
    CHECK(fol_clause_is_empty(r));
    fol_clause_free(c1);
    fol_clause_free(c2);
    fol_clause_free(r);
  }

  // === paramodulation ==========================================

  TEST_BEGIN("fol/paramod-basic");
  {
    // eq_clause = (a = b)        positive equality
    // target    = P(a)
    // path      = [0]            (P's child 0 = a)
    // paramodulant = P(b)
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));      // FOL_LAB_EQ = 0
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, /*swap*/ 0, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 1);
    CHECK(p->lits[0].sign == 0);
    CHECK(kbo_eq(p->lits[0].atom, pred1(P_P, k(L_b))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  TEST_BEGIN("fol/paramod-swapped-orientation");
  {
    // eq_clause = (a = b)
    // target    = P(b)
    // path      = [0]
    // With swap=1, use orientation b -> a; paramodulant = P(a).
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, /*swap*/ 1, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 1);
    CHECK(kbo_eq(p->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  TEST_BEGIN("fol/paramod-with-unification");
  {
    // eq_clause = (f(x) = b)     (with variable x)
    // target    = P(f(a))
    // path      = [0]            (P's child 0 = f(a))
    // unify f(x) with f(a): x ↦ a.  paramodulant = P(b).
    Term f_a = pred1(L_f, k(L_a));
    Term eq_atom = pred2(0u, pred1(L_f, v(0)), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = eq_atom, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, f_a), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 1);
    CHECK(kbo_eq(p->lits[0].atom, pred1(P_P, k(L_b))));
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  TEST_BEGIN("fol/paramod-fails-on-negative-equality");
  {
    // eq_clause = ¬(a = b): negative equality -- paramod fires only on
    // POSITIVE equality.  Should return NULL.
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 1 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/paramod-into-variable-position-fails");
  {
    // target = P(x), path [0] = x (FVR).  Paramod into variable
    // positions is disallowed (no useful CP).
    Term eq_atom = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(1);
    eqc->lits[0] = (FolLit){ .atom = eq_atom, .sign = 0 };
    FolClause *tgt = fol_clause_new(1);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, v(5)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p == NULL);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
  }

  TEST_BEGIN("fol/paramod-multi-literal");
  {
    // eq_clause = (a = b) v Q(c)
    // target    = P(a) v R(a, a)
    // paramod at target_idx=0, path=[0]:
    //   drop eq -> keep Q(c).  Replace P(a) with P(b).
    //   Final: Q(c) v P(b) v R(a, a).
    Term a_eq_b = pred2(0u, k(L_a), k(L_b));
    FolClause *eqc = fol_clause_new(2);
    eqc->lits[0] = (FolLit){ .atom = a_eq_b, .sign = 0 };
    eqc->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *tgt = fol_clause_new(2);
    tgt->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    tgt->lits[1] = (FolLit){ .atom = pred2(P_R, k(L_a), k(L_a)), .sign = 0 };
    u32 path[1] = { 0u };
    FolClause *p = fol_paramodulate(eqc, 0, 0, tgt, 0, path, 1);
    CHECK(p != NULL);
    CHECK(p->n_lits == 3);
    fol_clause_free(eqc);
    fol_clause_free(tgt);
    fol_clause_free(p);
  }

  // === reflexivity resolution =====================================

  TEST_BEGIN("fol/reflex-resolve-trivial");
  {
    // C = ¬(a = a) v P(b).  Atoms unify trivially => resolvent = P(b).
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 1 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_b)), .sign = 0 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_b))));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/reflex-resolve-with-unifier");
  {
    // C = ¬(x = a) v P(x).  σ = {x ↦ a} unifies x and a.
    // Resolvent = P(a).
    Term eq_atom = pred2(0u, v(0), k(L_a));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = eq_atom, .sign = 1 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r != NULL);
    CHECK(r->n_lits == 1);
    CHECK(kbo_eq(r->lits[0].atom, pred1(P_P, k(L_a))));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/reflex-resolve-fails-positive");
  {
    // Positive equality literal -- reflex-resolve requires negative.
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 0 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/reflex-resolve-fails-non-eq");
  {
    // ¬P(a) is not an equality literal.
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    FolClause *r = fol_reflex_resolve(c, 0);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  // === subsumption =================================================

  TEST_BEGIN("fol/subsumes-identical");
  {
    FolClause *a = fol_clause_new(1);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *b = fol_clause_new(1);
    b->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(a, b));
    CHECK(fol_subsumes(b, a));
    fol_clause_free(a);
    fol_clause_free(b);
  }

  TEST_BEGIN("fol/subsumes-via-instance");
  {
    // P(x) subsumes P(a) (σ = {x ↦ a}).  Not the reverse.
    FolClause *general = fol_clause_new(1);
    general->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    FolClause *specific = fol_clause_new(1);
    specific->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(general, specific));
    CHECK(!fol_subsumes(specific, general));
    fol_clause_free(general);
    fol_clause_free(specific);
  }

  TEST_BEGIN("fol/subsumes-subset-of-literals");
  {
    // P(a) subsumes P(a) v Q(b)  (the proper-subset case).
    FolClause *small = fol_clause_new(1);
    small->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *big = fol_clause_new(2);
    big->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    big->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    CHECK(fol_subsumes(small, big));
    CHECK(!fol_subsumes(big, small));
    fol_clause_free(small);
    fol_clause_free(big);
  }

  TEST_BEGIN("fol/subsumes-sign-mismatch");
  {
    // P(a) does NOT subsume ¬P(a) (signs differ).
    FolClause *a = fol_clause_new(1);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    FolClause *b = fol_clause_new(1);
    b->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    CHECK(!fol_subsumes(a, b));
    fol_clause_free(a);
    fol_clause_free(b);
  }

  TEST_BEGIN("fol/subsumes-shared-vars-consistent");
  {
    // P(x) v Q(x) subsumes P(a) v Q(a):
    //   σ = {x ↦ a} maps consistently across both literals.
    FolClause *gen = fol_clause_new(2);
    gen->lits[0] = (FolLit){ .atom = pred1(P_P, v(0)), .sign = 0 };
    gen->lits[1] = (FolLit){ .atom = pred1(P_Q, v(0)), .sign = 0 };
    FolClause *spec = fol_clause_new(2);
    spec->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    spec->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(gen, spec));
    // Inconsistent: P(x) v Q(x) does NOT subsume P(a) v Q(b).
    FolClause *bad = fol_clause_new(2);
    bad->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    bad->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    CHECK(!fol_subsumes(gen, bad));
    fol_clause_free(gen);
    fol_clause_free(spec);
    fol_clause_free(bad);
  }

  TEST_BEGIN("fol/subsumes-empty-clause");
  {
    // The empty clause subsumes everything.
    FolClause *empty = fol_clause_new(0);
    FolClause *anything = fol_clause_new(2);
    anything->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    anything->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 1 };
    CHECK(fol_subsumes(empty, anything));
    // But a non-empty clause does not subsume the empty clause (no
    // literal to match into).
    CHECK(!fol_subsumes(anything, empty));
    fol_clause_free(empty);
    fol_clause_free(anything);
  }

  TEST_BEGIN("fol/subsumes-permuted");
  {
    // P(a) v Q(b)  subsumes  Q(b) v P(a)  (multiset match).
    FolClause *a = fol_clause_new(2);
    a->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    a->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    FolClause *b = fol_clause_new(2);
    b->lits[0] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    b->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    CHECK(fol_subsumes(a, b));
    CHECK(fol_subsumes(b, a));
    fol_clause_free(a);
    fol_clause_free(b);
  }

  // === tautology ====================================================

  TEST_BEGIN("fol/tautology-pm-A");
  {
    // P(a) v ¬P(a) is a tautology.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 1 };
    CHECK(fol_is_tautology(c));
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/tautology-reflex-eq");
  {
    // (a = a) is a tautology.
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c = fol_clause_new(1);
    c->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 0 };
    CHECK(fol_is_tautology(c));
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/tautology-negative");
  {
    // P(a) v Q(b): not a tautology.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred1(P_Q, k(L_b)), .sign = 0 };
    CHECK(!fol_is_tautology(c));
    // ¬(a = a) is also NOT a tautology (it's UNSAT in fact).
    Term a_eq_a = pred2(0u, k(L_a), k(L_a));
    FolClause *c2 = fol_clause_new(1);
    c2->lits[0] = (FolLit){ .atom = a_eq_a, .sign = 1 };
    CHECK(!fol_is_tautology(c2));
    fol_clause_free(c);
    fol_clause_free(c2);
  }

  // === equality factoring =========================================

  TEST_BEGIN("fol/eq-factor-ground");
  {
    // C = (a = b) v (a = c).
    // σ unifies a with a (trivial), derive (a = b) v ¬(b = c).
    Term a_b = pred2(0u, k(L_a), k(L_b));
    Term a_c = pred2(0u, k(L_a), k(L_c));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = a_b, .sign = 0 };
    c->lits[1] = (FolLit){ .atom = a_c, .sign = 0 };
    FolClause *r = fol_eq_factor(c, 0, 1);
    CHECK(r != NULL);
    CHECK(r->n_lits == 2);
    // [0] = (a = b) positive
    CHECK(r->lits[0].sign == 0);
    CHECK(fol_atom_is_eq(r->lits[0].atom));
    // [1] = ¬(b = c) negative
    CHECK(r->lits[1].sign == 1);
    CHECK(fol_atom_is_eq(r->lits[1].atom));
    CHECK(kbo_eq(term_ctr_at(r->lits[1].atom, 0), k(L_b)));
    CHECK(kbo_eq(term_ctr_at(r->lits[1].atom, 1), k(L_c)));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/eq-factor-with-unification");
  {
    // C = (x = b) v (a = c).  σ = {x ↦ a} unifies x with a.
    // Result: (a = b) v ¬(b = c).
    Term x_b = pred2(0u, v(0), k(L_b));
    Term a_c = pred2(0u, k(L_a), k(L_c));
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = x_b, .sign = 0 };
    c->lits[1] = (FolLit){ .atom = a_c, .sign = 0 };
    FolClause *r = fol_eq_factor(c, 0, 1);
    CHECK(r != NULL);
    CHECK(r->n_lits == 2);
    // [0] = (a = b)
    CHECK(kbo_eq(term_ctr_at(r->lits[0].atom, 0), k(L_a)));
    CHECK(kbo_eq(term_ctr_at(r->lits[0].atom, 1), k(L_b)));
    fol_clause_free(c);
    fol_clause_free(r);
  }

  TEST_BEGIN("fol/eq-factor-non-eq-fails");
  {
    // Non-equality literal -- factoring doesn't fire.
    FolClause *c = fol_clause_new(2);
    c->lits[0] = (FolLit){ .atom = pred1(P_P, k(L_a)), .sign = 0 };
    c->lits[1] = (FolLit){ .atom = pred2(0u, k(L_a), k(L_b)), .sign = 0 };
    FolClause *r = fol_eq_factor(c, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(c);
  }

  TEST_BEGIN("fol/eq-factor-non-unify-fails");
  {
    // (a = b) v (c = d): a doesn't unify with c.
    Term a_b = pred2(0u, k(L_a), k(L_b));
    Term c_d = pred2(0u, k(L_c), k(L_a));   // use a-distinct LHS for clarity
    FolClause *cl = fol_clause_new(2);
    cl->lits[0] = (FolLit){ .atom = a_b, .sign = 0 };
    cl->lits[1] = (FolLit){ .atom = c_d, .sign = 0 };
    FolClause *r = fol_eq_factor(cl, 0, 1);
    CHECK(r == NULL);
    fol_clause_free(cl);
  }

  thvm_free();
  TEST_REPORT();
}
