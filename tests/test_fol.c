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

  thvm_free();
  TEST_REPORT();
}
