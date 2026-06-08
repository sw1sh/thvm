// test_atp_multiproof.c - regression for the AtpFt multi-proof span crash.
//
// A completion that collapses a NON-RIGHTMOST subterm to a single cell
// (regime-(a) of ft_splice) used to leave the displaced subtree's
// ancestors with a dangling `->end` pointer.  The next find_redex_ft
// walk TRUSTS `->end` (it does not re-derive it from the head chain), so
// a later ft_splice was handed a redex whose `->end` precedes it in
// memory; ft_free_span then walked the free-list chain and tripped
//   ft_alloc: free_span underflow (N > M)   -> exit(1).
//
// The crash is data-dependent on the rule set the saturation builds, so
// a trivial 1-step proof never triggered it.  The {left-id, left-inv,
// assoc} group axioms with a DIVERGING goal pump enough non-confluent
// intermediate rule sets to reach the collapse; pre-fix this aborts the
// whole process on the first run, so it surfaced as "the 2nd proof in a
// kernel session crashes" via the WL paclet (whichever run happened to
// hit the collapse).
//
// This test runs several full proof cycles in ONE process:
//   - two PROVABLE goals (assert PROVED + correct survival),
//   - a DIVERGING completion at the small step cap that crashed pre-fix
//     (assert the process SURVIVES -- reaching the post-run line at all
//     means ft_free_span did not exit(1)),
// repeated 5x back-to-back to also cover the cross-proof reporting.

#include "../src/thvm.c"
#include "test.h"

#include <stdio.h>

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_i(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }

// Same KBO config the headline group-axiom proofs use (weights i=0,
// f=1, e=1, a=1; precedence i > f > e > a; var weight 1).
static u32 gw[5] = {0, 1, 0, 1, 1};
static u32 gp[5] = {0, 2, 4, 3, 1};
static const KboConfig GROUP_CFG = {
  .weights = gw, .precedence = gp, .n_labels = 5, .var_weight = 1,
};

// Left-presented group axioms: left-id, left-inv, assoc.  Completing
// these is exactly the trajectory that collapses a non-rightmost
// subterm in a CP normalize.
static void load_left_group_axioms(AtpState *s) {
  thvm_atp_add_equation(s, mk_f(mk_e(), mk_v(0u)),           mk_v(0u));
  thvm_atp_add_equation(s, mk_f(mk_i(mk_v(0u)), mk_v(0u)),   mk_e());
  thvm_atp_add_equation(s, mk_f(mk_f(mk_v(0u), mk_v(1u)), mk_v(2u)),
                        mk_f(mk_v(0u), mk_f(mk_v(1u), mk_v(2u))));
}

// One full proof cycle: init -> record-norm -> goal -> axioms -> run ->
// extract -> free.  Returns the status; reaching the return at all
// means ft_free_span did not exit(1) mid-run.
static AtpStatus proof_cycle(Term gl, Term gr, u32 step_cap) {
  AtpState *s = thvm_atp_init(&GROUP_CFG, step_cap);
  thvm_atp_set_record_norm_steps(s, 1u);
  thvm_atp_set_goal(s, gl, gr);
  load_left_group_axioms(s);
  AtpStatus st = thvm_atp_run(s);
  static AtpProofStep steps[ATP_PROOF_MAX_STEPS];
  (void)thvm_atp_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
  thvm_atp_free(s);
  return st;
}

int main(void) {
  thvm_init();

  // === Multiple full proof cycles in one process ======================
  // Each iteration runs three proofs back-to-back.  Pre-fix, the
  // diverging completion aborts the process (free_span underflow) on
  // the very first iteration; surviving all five iterations proves the
  // ft_splice regime-(a) ancestor end-pointer fix-up holds.
  TEST_BEGIN("atp/multiproof/no-crash-across-cycles");
  for (int it = 0; it < 5; it++) {
    // (1) right-identity f(a, e) = a -- provable from the left axioms.
    AtpStatus s1 = proof_cycle(mk_f(mk_a(), mk_e()), mk_a(), 300u);
    CHECK_EQ((int)s1, (int)ATP_PROVED);

    // (2) right-inverse f(a, i(a)) = e -- provable from the left axioms.
    AtpStatus s2 = proof_cycle(mk_f(mk_a(), mk_i(mk_a())), mk_e(), 300u);
    CHECK_EQ((int)s2, (int)ATP_PROVED);

    // (3) i(i(x)) = x as a SCHEMA goal drives a diverging completion;
    // the step cap of 64 is past the CP that collapses a non-rightmost
    // subterm (which crashed pre-fix) but bounded so the test is fast.
    // We only assert the run RETURNS -- a crash would exit(1) and the
    // assertion below would never execute.
    AtpStatus s3 = proof_cycle(mk_i(mk_i(mk_v(0u))), mk_v(0u), 64u);
    CHECK(s3 == ATP_PROVED || s3 == ATP_TIMEOUT || s3 == ATP_QUEUE_EMPTY);
  }

  TEST_REPORT();
}
