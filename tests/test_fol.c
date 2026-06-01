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

  thvm_free();
  TEST_REPORT();
}
