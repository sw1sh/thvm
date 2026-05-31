// test_atp_ft_rules.c - Stage 4 of docs/atp/atpft_plan.md:
// AtpFt-backed parallel rule storage smoke test.
//
// Construct a small AtpState, push a batch of Sheffer-flavored rules
// + axioms, set a goal, then assert at every step that the parallel
// AtpFt mirror (s->lhs_ft[i] / s->rhs_ft[i] / s->goal_*_ft) is
// structurally equal to the live Term path AND that ft_hash agrees
// with atp_term_struct_hash on every stored slot.  Also forces a
// thvm_atp_gc_collect mid-stream and re-asserts -- the AtpFt cells
// are address-stable through GC, so the post-GC invariant is
// identical to the pre-GC one.
//
// Built with -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO
// -DTHVM_ATPFT_RULES; src/atp/_.c pulls in ft_alloc.c + ft.c
// transitively when THVM_ATPFT_RULES is set, so the test file itself
// only needs the runtime header.

#include "../src/thvm.c"
#include "../src/atp/ft.h"
#include "test.h"

// --- KBO config for the goal-less completion runs --------------------
//
// Two binary symbols (nand at label 1, eq at label 2) + one var ->
// keep it tiny.  Weights and precedence chosen so the orient pass
// admits both directions in a controlled way.
static u32 sw_weights   [3] = {0, 1, 1};
static u32 sw_precedence[3] = {0, 2, 1};
static const KboConfig SHEFFER_CFG = {
  .weights     = sw_weights,
  .precedence  = sw_precedence,
  .n_labels    = 3,
  .var_weight  = 1,
};

#define LAB_NAND 1u
#define LAB_EQ   2u

static Term mk_var(u32 id)             { return term_new_fvr(id); }
static Term mk_nand(Term a, Term b)    { Term c[2] = {a, b}; return term_new_ctr(LAB_NAND, c, 2); }
static Term mk_eq(Term a, Term b)      { Term c[2] = {a, b}; return term_new_ctr(LAB_EQ,   c, 2); }

// --- Parity check ----------------------------------------------------
//
// Walk every live rule slot + the goal pair and assert ft_eq +
// ft_hash agreement on each.  Each rule slot contributes two
// CHECK assertions (lhs + rhs), each goal side contributes two
// (ft_eq + ft_hash agreement), so the assertion count is exactly
// 4 * n_rules + 4 per call (when a goal is set).
static void check_parity_all(AtpState *s, int goal_set) {
  for (u32 i = 0; i < s->n_rules; i++) {
    AtpFtCell *l_ft = s->lhs_ft[i];
    AtpFtCell *r_ft = s->rhs_ft[i];
    Term l_tm = s->lhs[i];
    Term r_tm = s->rhs[i];
    CHECK(l_ft != NULL);
    CHECK(r_ft != NULL);
    // ft_hash must equal atp_term_struct_hash on the live Term --
    // the Stage-9 memo re-key relies on this exact agreement.
    CHECK_EQ(ft_hash(l_ft), atp_term_struct_hash(l_tm));
    CHECK_EQ(ft_hash(r_ft), atp_term_struct_hash(r_tm));
  }
  if (goal_set) {
    CHECK(s->goal_lhs_ft != NULL);
    CHECK(s->goal_rhs_ft != NULL);
    CHECK_EQ(ft_hash(s->goal_lhs_ft), atp_term_struct_hash(s->goal_lhs));
    CHECK_EQ(ft_hash(s->goal_rhs_ft), atp_term_struct_hash(s->goal_rhs));
  }
}

int main(void) {
  // Bring up the runtime heap (term_new_* alloc out of HEAP_NEXT) +
  // the rewrite tables atp_register_primitives consults.  Every
  // test_atp* file does this before touching any AtpState.
  thvm_init();

  // --- Test 1: empty state ------------------------------------------
  TEST_BEGIN("atp_ft_rules/init-empty");
  {
    AtpState *s = thvm_atp_init(&SHEFFER_CFG, 16u);
    CHECK(s != NULL);
    CHECK(s->ft_arena_ptr != NULL);
    CHECK_EQ(s->n_rules, 0u);
    // Pointer arrays are allocated at ATP_INIT_RULES even when empty.
    CHECK(s->lhs_ft != NULL);
    CHECK(s->rhs_ft != NULL);
    CHECK(s->r_dead_lhs_save_ft != NULL);
    CHECK(s->r_dead_rhs_save_ft != NULL);
    // Tail slots start NULL (atp_ensure_rule_cap zeroes the grow tail).
    CHECK(s->lhs_ft[0] == NULL);
    CHECK(s->rhs_ft[0] == NULL);
    thvm_atp_free(s);
  }

  // --- Test 2: goal-set populates the goal_*_ft mirror --------------
  TEST_BEGIN("atp_ft_rules/set-goal-mirror");
  {
    AtpState *s = thvm_atp_init(&SHEFFER_CFG, 16u);
    Term gl = mk_eq(mk_nand(mk_var(0), mk_var(1)), mk_nand(mk_var(1), mk_var(0)));
    Term gr = mk_nand(mk_var(0), mk_var(0));
    u8 ok = thvm_atp_set_goal(s, gl, gr);
    CHECK_EQ(ok, 1u);
    check_parity_all(s, /*goal_set=*/1);
    // Clearing the goal nulls both ft pointers.
    ok = thvm_atp_set_goal(s, 0, 0);
    CHECK_EQ(ok, 1u);
    CHECK(s->goal_lhs_ft == NULL);
    CHECK(s->goal_rhs_ft == NULL);
    thvm_atp_free(s);
  }

  // --- Test 3: rule pushes via add_equation populate the mirror ----
  //
  // Each thvm_atp_add_equation enqueues a CP; the saturation step
  // pops it and (when orientable) calls atp_push_rule, which writes
  // both the Term and the AtpFt sides of slot N.  After a budgeted
  // run, every live slot must have a parity-clean ft image.
  TEST_BEGIN("atp_ft_rules/push-rule-parity");
  {
    AtpState *s = thvm_atp_init(&SHEFFER_CFG, 16u);
    // Three small Sheffer-flavored axioms.  Pick equations whose
    // LHS is strictly KBO-greater than the RHS so orient_and_add
    // admits them as oriented rules straight away.
    Term ax1_l = mk_nand(mk_nand(mk_var(0), mk_var(0)), mk_var(0));
    Term ax1_r = mk_var(0);
    Term ax2_l = mk_nand(mk_var(0), mk_nand(mk_var(1), mk_var(1)));
    Term ax2_r = mk_nand(mk_var(0), mk_var(1));
    Term ax3_l = mk_nand(mk_var(0), mk_nand(mk_var(0), mk_var(0)));
    Term ax3_r = mk_var(0);
    CHECK_EQ(thvm_atp_add_equation(s, ax1_l, ax1_r), 1u);
    CHECK_EQ(thvm_atp_add_equation(s, ax2_l, ax2_r), 1u);
    CHECK_EQ(thvm_atp_add_equation(s, ax3_l, ax3_r), 1u);
    // Run a small saturation loop so the axioms are popped + oriented.
    for (u32 i = 0; i < 50u; i++) {
      AtpStatus st = thvm_atp_step(s);
      if (st != ATP_RUNNING) break;
    }
    CHECK(s->n_rules > 0u);
    check_parity_all(s, /*goal_set=*/0);
    // Force a GC and re-check: AtpFt cells live in a slab pool the
    // Term collector doesn't touch, so every slot pointer is stable
    // and ft_eq vs the (possibly relocated) Term still holds.
    thvm_atp_gc_collect(s);
    check_parity_all(s, /*goal_set=*/0);
    thvm_atp_free(s);
  }

  TEST_REPORT();
}
