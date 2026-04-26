// test_atp.c - AtpState construction (stage 5.1).
//
// Init/free/add_equation/set_goal only.  The saturation step lands
// in 5.2 and gets its own broader test.

#include "../src/thvm.c"
#include "test.h"

#define LAB_e 1u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_v(u32 id) { return term_new_fvr(id); }

static u32 dummy_weights   [5] = {0, 1, 0, 1, 1};
static u32 dummy_precedence[5] = {0, 2, 4, 3, 1};
static const KboConfig DUMMY_CFG = {
  .weights     = dummy_weights,
  .precedence  = dummy_precedence,
  .n_labels    = 5,
  .var_weight  = 1,
};

int main(void) {
  thvm_init();

  TEST_BEGIN("atp/init-and-free");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK(s != NULL);
    CHECK(s->kbo == &DUMMY_CFG);
    CHECK_EQ(s->step_cap,     100u);
    CHECK_EQ(s->n_rules,        0u);
    CHECK_EQ(s->n_cps,          0u);
    CHECK_EQ(s->goal_lhs,       0u);
    CHECK_EQ(s->goal_rhs,       0u);
    CHECK_EQ(s->step,           0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/free-null-is-safe");
  {
    thvm_atp_free(NULL);   // no crash, no-op
  }

  TEST_BEGIN("atp/add-equation-pushes-to-queue");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    CHECK(thvm_atp_add_equation(s, lhs, rhs));
    CHECK_EQ(s->n_cps,      1u);
    CHECK_EQ(s->cp_lhs[0], lhs);
    CHECK_EQ(s->cp_rhs[0], rhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/add-equation-rejects-when-full");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    // Fill the queue with cheap equations.
    Term lhs = mk_e();
    Term rhs = mk_e();
    for (u32 i = 0; i < ATP_MAX_CPS; i++) {
      CHECK(thvm_atp_add_equation(s, lhs, rhs));
    }
    CHECK_EQ(s->n_cps, (u64)ATP_MAX_CPS);
    // One more should fail.
    CHECK_EQ(thvm_atp_add_equation(s, lhs, rhs), 0u);
    CHECK_EQ(s->n_cps, (u64)ATP_MAX_CPS);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/set-goal-stores-pair");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_a(), mk_e());
    Term rhs = mk_a();
    thvm_atp_set_goal(s, lhs, rhs);
    CHECK_EQ(s->goal_lhs, lhs);
    CHECK_EQ(s->goal_rhs, rhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/set-goal-zero-clears");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_e(), mk_e());
    thvm_atp_set_goal(s, 0, 0);
    CHECK_EQ(s->goal_lhs, 0u);
    CHECK_EQ(s->goal_rhs, 0u);
    thvm_atp_free(s);
  }

  thvm_free();
  TEST_REPORT();
}
