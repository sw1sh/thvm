// test_fol_pelletier.c -- Pelletier problems through the FOL pipeline.
//
// The classic Pelletier propositional / first-order problems are a
// standard sanity bench for resolution provers.  Each test:
//   1. Build the conjecture as a Term-tree using the FOL connectives.
//   2. NEGATE it (refutation proof: ¬conjecture must be unsatisfiable).
//   3. Run fol_formula_to_clauses to NNF + Skolemize + CNF.
//   4. cnf_run the resulting clause set; expect ATP_PROVED.
//
// Each problem cited as P<n> from Pelletier 1986, "Seventy-five
// problems for testing automatic theorem provers".

#include "../src/thvm.c"
#include "test.h"

// Propositional symbols.  Pick labels above the user-defined ones
// in test_fol.c but well below FOL_LAB_NOT (0xFFF0).
#define LAB_P 100u
#define LAB_Q 101u
#define LAB_R 102u
#define LAB_S 103u

// Predicate constructors.
static Term P_atom(void)  { return term_new_ctr(LAB_P, NULL, 0); }
static Term Q_atom(void)  { return term_new_ctr(LAB_Q, NULL, 0); }
static Term R_atom(void)  { return term_new_ctr(LAB_R, NULL, 0); }
static Term S_atom(void)  { return term_new_ctr(LAB_S, NULL, 0); }

// FOL unary / binary predicates (for the FOL-flavored Pelletier cases).
#define LAB_P1 110u   // P(x)
#define LAB_Q1 111u   // Q(x)
static Term P1(Term x) { Term k[1] = {x}; return term_new_ctr(LAB_P1, k, 1); }
static Term Q1(Term x) { Term k[1] = {x}; return term_new_ctr(LAB_Q1, k, 1); }

static Term v(u32 id) { return term_new_fvr(id); }

// Run a refutation: negate conjecture, CNF-ify, saturate, expect PROVED.
static AtpStatus refute(Term conjecture, u32 step_cap) {
  Term negated = fol_mk_not(conjecture);
  u32 nc = 0;
  FolClause **clauses = fol_formula_to_clauses(negated, &nc);
  CnfState *s = cnf_init(step_cap);
  for (u32 i = 0; i < nc; i++) cnf_add_clause(s, clauses[i]);
  free(clauses);
  AtpStatus st = cnf_run(s);
  cnf_free(s);
  return st;
}

int main(void) {
  thvm_init();

  // P1: (P → Q) ↔ (¬Q → ¬P)  [contrapositive].
  // The conjecture is the biconditional.
  TEST_BEGIN("pelletier/p1-contrapositive");
  {
    Term p = P_atom(), q = Q_atom();
    Term conj = fol_mk_iff(
      fol_mk_imp(p, q),
      fol_mk_imp(fol_mk_not(q), fol_mk_not(p))
    );
    AtpStatus st = refute(conj, 512);
    CHECK(st == ATP_PROVED);
  }

  // P2: ¬¬P ↔ P
  TEST_BEGIN("pelletier/p2-double-negation");
  {
    Term p = P_atom();
    Term conj = fol_mk_iff(fol_mk_not(fol_mk_not(p)), p);
    AtpStatus st = refute(conj, 256);
    CHECK(st == ATP_PROVED);
  }

  // P3: ¬(P → Q) → (Q → P)
  TEST_BEGIN("pelletier/p3-imp-converse");
  {
    Term p = P_atom(), q = Q_atom();
    Term conj = fol_mk_imp(fol_mk_not(fol_mk_imp(p, q)), fol_mk_imp(q, p));
    AtpStatus st = refute(conj, 512);
    CHECK(st == ATP_PROVED);
  }

  // P4: (¬P → Q) ↔ (¬Q → P)
  TEST_BEGIN("pelletier/p4-contrapositive-neg");
  {
    Term p = P_atom(), q = Q_atom();
    Term conj = fol_mk_iff(
      fol_mk_imp(fol_mk_not(p), q),
      fol_mk_imp(fol_mk_not(q), p)
    );
    AtpStatus st = refute(conj, 1024);
    CHECK(st == ATP_PROVED);
  }

  // P5: ((P ∨ Q) → (P ∨ R)) → (P ∨ (Q → R))
  TEST_BEGIN("pelletier/p5-disjunction-distribute");
  {
    Term p = P_atom(), q = Q_atom(), r = R_atom();
    Term conj = fol_mk_imp(
      fol_mk_imp(fol_mk_or(p, q), fol_mk_or(p, r)),
      fol_mk_or(p, fol_mk_imp(q, r))
    );
    AtpStatus st = refute(conj, 2048);
    CHECK(st == ATP_PROVED);
  }

  // P6: P ∨ ¬P   [excluded middle].
  TEST_BEGIN("pelletier/p6-excluded-middle");
  {
    Term p = P_atom();
    Term conj = fol_mk_or(p, fol_mk_not(p));
    AtpStatus st = refute(conj, 64);
    CHECK(st == ATP_PROVED);
  }

  // P7: P ∨ ¬¬¬P
  TEST_BEGIN("pelletier/p7-excluded-middle-triple-neg");
  {
    Term p = P_atom();
    Term conj = fol_mk_or(p, fol_mk_not(fol_mk_not(fol_mk_not(p))));
    AtpStatus st = refute(conj, 128);
    CHECK(st == ATP_PROVED);
  }

  // P8: ((P → Q) → P) → P   [Peirce's law].
  TEST_BEGIN("pelletier/p8-peirce");
  {
    Term p = P_atom(), q = Q_atom();
    Term conj = fol_mk_imp(fol_mk_imp(fol_mk_imp(p, q), p), p);
    AtpStatus st = refute(conj, 2048);
    CHECK(st == ATP_PROVED);
  }

  // P9: ((P ∨ Q) ∧ (¬P ∨ Q) ∧ (P ∨ ¬Q)) → ¬(¬P ∨ ¬Q)
  TEST_BEGIN("pelletier/p9-three-clauses");
  {
    Term p = P_atom(), q = Q_atom();
    Term lhs = fol_mk_and(
      fol_mk_and(
        fol_mk_or(p, q),
        fol_mk_or(fol_mk_not(p), q)
      ),
      fol_mk_or(p, fol_mk_not(q))
    );
    Term rhs = fol_mk_not(fol_mk_or(fol_mk_not(p), fol_mk_not(q)));
    Term conj = fol_mk_imp(lhs, rhs);
    AtpStatus st = refute(conj, 4096);
    CHECK(st == ATP_PROVED);
  }

  // P10: ((Q → R) ∧ (R → P ∧ Q) ∧ (P → Q ∨ R)) → (P ↔ Q)
  TEST_BEGIN("pelletier/p10-mutual-implications");
  {
    Term p = P_atom(), q = Q_atom(), r = R_atom();
    Term lhs = fol_mk_and(
      fol_mk_and(
        fol_mk_imp(q, r),
        fol_mk_imp(r, fol_mk_and(p, q))
      ),
      fol_mk_imp(p, fol_mk_or(q, r))
    );
    Term rhs = fol_mk_iff(p, q);
    Term conj = fol_mk_imp(lhs, rhs);
    AtpStatus st = refute(conj, 8192);
    CHECK(st == ATP_PROVED);
  }

  // P11: P ↔ P [trivial tautology].
  TEST_BEGIN("pelletier/p11-trivial-iff");
  {
    Term p = P_atom();
    Term conj = fol_mk_iff(p, p);
    AtpStatus st = refute(conj, 64);
    CHECK(st == ATP_PROVED);
  }

  // P12: ((P ↔ Q) ↔ R) ↔ (P ↔ (Q ↔ R))  [biconditional associativity].
  TEST_BEGIN("pelletier/p12-iff-assoc");
  {
    Term p = P_atom(), q = Q_atom(), r = R_atom();
    Term lhs = fol_mk_iff(fol_mk_iff(p, q), r);
    Term rhs = fol_mk_iff(p, fol_mk_iff(q, r));
    Term conj = fol_mk_iff(lhs, rhs);
    AtpStatus st = refute(conj, 16384);
    CHECK(st == ATP_PROVED);
  }

  // P13: (P ∨ (Q ∧ R)) ↔ ((P ∨ Q) ∧ (P ∨ R))  [distributivity].
  TEST_BEGIN("pelletier/p13-distrib");
  {
    Term p = P_atom(), q = Q_atom(), r = R_atom();
    Term lhs = fol_mk_or(p, fol_mk_and(q, r));
    Term rhs = fol_mk_and(fol_mk_or(p, q), fol_mk_or(p, r));
    Term conj = fol_mk_iff(lhs, rhs);
    AtpStatus st = refute(conj, 8192);
    CHECK(st == ATP_PROVED);
  }

  // P14: (P ↔ Q) ↔ ((Q ∨ ¬P) ∧ (¬Q ∨ P)).
  TEST_BEGIN("pelletier/p14-iff-via-or");
  {
    Term p = P_atom(), q = Q_atom();
    Term lhs = fol_mk_iff(p, q);
    Term rhs = fol_mk_and(fol_mk_or(q, fol_mk_not(p)),
                           fol_mk_or(fol_mk_not(q), p));
    Term conj = fol_mk_iff(lhs, rhs);
    AtpStatus st = refute(conj, 4096);
    CHECK(st == ATP_PROVED);
  }

  // P15 (FOL): (∀x.(P(x) → Q(x))) → ((∀x.P(x)) → (∀x.Q(x)))
  // This is a basic FOL theorem; tests Skolemization + paramod.
  TEST_BEGIN("pelletier/p15-fol-implication");
  {
    Term x = v(0);
    Term conj = fol_mk_imp(
      fol_mk_all(x, fol_mk_imp(P1(x), Q1(x))),
      fol_mk_imp(
        fol_mk_all(x, P1(x)),
        fol_mk_all(x, Q1(x))
      )
    );
    AtpStatus st = refute(conj, 4096);
    CHECK(st == ATP_PROVED);
  }

  // P16: (P → Q) ∨ (Q → P)  [classical but not intuitionistic].
  TEST_BEGIN("pelletier/p16-or-imp");
  {
    Term p = P_atom(), q = Q_atom();
    Term conj = fol_mk_or(fol_mk_imp(p, q), fol_mk_imp(q, p));
    AtpStatus st = refute(conj, 1024);
    CHECK(st == ATP_PROVED);
  }

  // P18: ∃y.∀x. (P(y) → P(x))   [drinker paradox].
  // Negation: ∀y.∃x. P(y) ∧ ¬P(x).
  // Skolemize x -> f(y): ∀y. P(y) ∧ ¬P(f(y)).
  // Resolution: P(y0) vs ¬P(f(y1)) with σ = {y0 ↦ f(y1)} -> empty.
  TEST_BEGIN("pelletier/p18-drinker-paradox");
  {
    Term y = v(0);
    Term x = v(1);
    Term conj = fol_mk_ex(y,
      fol_mk_all(x,
        fol_mk_imp(P1(y), P1(x))));
    AtpStatus st = refute(conj, 256);
    CHECK(st == ATP_PROVED);
  }

  // P19: ∃x.∀y.∀z. ((P(y) → Q(z)) → (P(x) → Q(x)))
  // After negation + Skolemization gives a more complex clause set.
  TEST_BEGIN("pelletier/p19-existential-bound");
  {
    Term x = v(0);
    Term y = v(1);
    Term z = v(2);
    Term inner = fol_mk_imp(
      fol_mk_imp(P1(y), Q1(z)),
      fol_mk_imp(P1(x), Q1(x))
    );
    Term conj = fol_mk_ex(x,
      fol_mk_all(y,
        fol_mk_all(z, inner)));
    AtpStatus st = refute(conj, 4096);
    CHECK(st == ATP_PROVED);
  }

  // === proof display ===========================================
  // Demonstrates the proof-reconstruction pipeline end-to-end:
  // build a Pelletier problem, prove it, print the proof tree.
  // The proof goes to stderr so test runners with `2>&1 | head`
  // can inspect it; the test itself just verifies the file is
  // non-empty / contains the expected sentinel substring.

  TEST_BEGIN("pelletier/p1-print-proof");
  {
    // Build P1 again, but this time capture the proof tree.
    Term p = P_atom(), q = Q_atom();
    Term conj = fol_mk_iff(
      fol_mk_imp(p, q),
      fol_mk_imp(fol_mk_not(q), fol_mk_not(p))
    );
    Term negated = fol_mk_not(conj);
    u32 nc = 0;
    FolClause **clauses = fol_formula_to_clauses(negated, &nc);
    CnfState *s = cnf_init(512);
    for (u32 i = 0; i < nc; i++) cnf_add_clause(s, clauses[i]);
    free(clauses);
    AtpStatus st = cnf_run(s);
    CHECK(st == ATP_PROVED);

    // Find the empty clause and print its proof DAG.
    u32 empty_id = 0;
    u8 found = 0;
    for (u32 i = 0; i < s->n; i++) {
      if (s->clauses[i] != NULL && s->clauses[i]->n_lits == 0u) {
        empty_id = i; found = 1; break;
      }
    }
    CHECK(found);
    FILE *proof = fopen("/tmp/thvm_fol_pelletier_p1_proof.txt", "w");
    CHECK(proof != NULL);
    fprintf(proof, "=== Pelletier P1: (P -> Q) <-> (~Q -> ~P) ===\n");
    fprintf(proof, "negated formula -> %u clauses; saturation -> PROVED at c%u.\n\n",
            nc, empty_id);
    cnf_print_proof(s, proof, empty_id);
    fclose(proof);

    // Verify the file is non-empty (sanity-check the writer didn't bail).
    FILE *check = fopen("/tmp/thvm_fol_pelletier_p1_proof.txt", "r");
    CHECK(check != NULL);
    fseek(check, 0, SEEK_END);
    long sz = ftell(check);
    fclose(check);
    CHECK(sz > 100);   // a real proof DAG is dozens of lines at minimum

    cnf_free(s);
  }

  // Used a P_atom helper that doesn't get warned out.
  (void)S_atom();

  thvm_free();
  TEST_REPORT();
}
