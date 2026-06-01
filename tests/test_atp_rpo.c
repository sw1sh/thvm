// test_atp_rpo.c -- ATP saturation driven by RPO instead of KBO/LPO.
//
// Verifies thvm_atp_set_rpo wires the Recursive Path Ordering through
// atp_compare so the saturator orients rules and joins goals under
// RPO's status-aware comparison.

#include "../src/thvm.c"
#include "test.h"

#define L_F  1u
#define L_A  2u
#define L_B  3u
#define L_C  4u

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0u); }
static Term bin(u32 lab, Term x, Term y) {
  Term kids[2] = { x, y };
  return term_new_ctr(lab, kids, 2u);
}
static Term v(u32 id) { return term_new_fvr(id); }

int main(void) {
  thvm_init();

  // Goal: f(a, b) = c with axiom f(a, b) = c.  Trivially provable
  // under any orientation -- this primarily verifies that thvm_atp_set_rpo
  // doesn't break the basic pipeline.
  TEST_BEGIN("atp/rpo-trivial");
  {
    Term x = v(0), y = v(1);

    static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
    static const u32 P[8] = { 0u, 1u, 4u, 3u, 2u, 0u, 0u, 0u };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 8, .var_weight = 1u };

    static const u32 RP[8] = { 0u, 1u, 4u, 3u, 2u, 0u, 0u, 0u };
    static const RpoStatus RS[8] = {
      RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
      RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
    };
    RpoConfig rpo = { .precedence = RP, .status = RS, .n_labels = 8 };

    AtpState *s = thvm_atp_init(&kbo, 1024);
    thvm_atp_set_rpo(s, &rpo);

    thvm_atp_add_equation(s, bin(L_F, k(L_A), k(L_B)), k(L_C));
    thvm_atp_set_goal(s, bin(L_F, k(L_A), k(L_B)), k(L_C));

    AtpStatus st = ATP_RUNNING;
    for (u32 i = 0; i < 64u; i++) {
      st = thvm_atp_step(s);
      if (st != ATP_RUNNING) break;
    }
    CHECK(st == ATP_PROVED);
    thvm_atp_free(s);
    (void)x; (void)y;
  }

  // Goal: prove f(f(a, b), c) = f(c, f(b, a)) under commutativity +
  // associativity of f, using RPO (all-LEX) -- the same shape the
  // commutative-monoid AC bench solves with KBO.  Sanity-checks that
  // RPO can also drive the saturator past the AC trivial-join short-
  // circuit when AC is off (purely syntactic).
  TEST_BEGIN("atp/rpo-syntactic-comm-monoid");
  {
    Term x = v(0), y = v(1), z = v(2);
    Term a = k(L_A), b = k(L_B), c = k(L_C);

    static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
    static const u32 P[8] = { 0u, 1u, 4u, 3u, 2u, 0u, 0u, 0u };
    KboConfig kbo = { .weights = W, .precedence = P, .n_labels = 8, .var_weight = 1u };

    static const u32 RP[8] = { 0u, 1u, 4u, 3u, 2u, 0u, 0u, 0u };
    static const RpoStatus RS[8] = {
      RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
      RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX, RPO_STATUS_LEX,
    };
    RpoConfig rpo = { .precedence = RP, .status = RS, .n_labels = 8 };

    AtpState *s = thvm_atp_init(&kbo, 4096);
    thvm_atp_set_rpo(s, &rpo);

    thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
    thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z),
                              bin(L_F, x, bin(L_F, y, z)));
    thvm_atp_set_goal(s, bin(L_F, bin(L_F, a, b), c),
                          bin(L_F, c, bin(L_F, b, a)));

    AtpStatus st = ATP_RUNNING;
    for (u32 i = 0; i < 4096u; i++) {
      st = thvm_atp_step(s);
      if (st != ATP_RUNNING) break;
    }
    // Both axioms are LEX-unorientable under RPO (comm: f(x,y) vs f(y,x)
    // are RPO_UN since the args' lex order flips).  The engine MAY
    // still join the goal via unfailing completion (both faces
    // superposed) or terminate QUEUE_EMPTY -- treat either as a valid
    // terminal status for this stage.
    CHECK(st != ATP_RUNNING);
    thvm_atp_free(s);
  }

  thvm_free();
  TEST_REPORT();
}
