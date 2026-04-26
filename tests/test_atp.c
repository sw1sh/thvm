// test_atp.c - AtpState construction (stage 5.1).
//
// Init/free/add_equation/set_goal only.  The saturation step lands
// in 5.2 and gets its own broader test.

#include "../src/thvm.c"
#include "test.h"

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_i(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }
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

  TEST_BEGIN("atp/select-cp-empty-queue");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term l = 0, r = 0;
    CHECK_EQ(thvm_atp_select_cp(s, &l, &r), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/select-cp-priority-order");
  {
    // Stage 5.3: cheapest CP wins.  Symbol counts:
    //   l1 = f(x, e), r1 = x       -> k_1 = (1+1+1) + 1 = 4
    //   l2 = a,        r2 = e       -> k_2 = 1 + 1     = 2
    //   l3 = e,        r3 = a       -> k_3 = 1 + 1     = 2
    // Pop order: l2/r2 (k=2, dfs=1), l3/r3 (k=2, dfs=2), l1/r1 (k=4).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term l1 = mk_f(mk_v(VAR_x), mk_e()),  r1 = mk_v(VAR_x);
    Term l2 = mk_a(),                      r2 = mk_e();
    Term l3 = mk_e(),                      r3 = mk_a();
    thvm_atp_add_equation(s, l1, r1);
    thvm_atp_add_equation(s, l2, r2);
    thvm_atp_add_equation(s, l3, r3);
    CHECK_EQ(s->n_cps, 3u);

    Term lo = 0, ro = 0;
    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK_EQ(lo, l2);
    CHECK_EQ(ro, r2);
    CHECK_EQ(s->n_cps, 2u);

    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK_EQ(lo, l3);
    CHECK_EQ(ro, r3);
    CHECK_EQ(s->n_cps, 1u);

    CHECK(thvm_atp_select_cp(s, &lo, &ro));
    CHECK_EQ(lo, l1);
    CHECK_EQ(ro, r1);
    CHECK_EQ(s->n_cps, 0u);

    // Now empty.
    CHECK_EQ(thvm_atp_select_cp(s, &lo, &ro), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/select-cp-shifts-tail-densely");
  {
    // After one pop, the remaining items should occupy slots [0..n-1).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_e(), mk_e());
    thvm_atp_add_equation(s, mk_a(), mk_a());

    Term lo = 0, ro = 0;
    thvm_atp_select_cp(s, &lo, &ro);
    CHECK_EQ(s->n_cps, 1u);
    // The remaining equation is now at slot 0.
    CHECK(s->cp_lhs[0] != 0);
    CHECK_EQ(term_ext(s->cp_lhs[0]), LAB_a);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-gt");
  {
    // f(x, e) > x  -> push f(x, e) -> x as a single rule.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    AtpAddedRange r = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r.first, 0u);
    CHECK_EQ(r.count, 1u);
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(s->lhs[0], lhs);
    CHECK_EQ(s->rhs[0], rhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-lt-swaps");
  {
    // x < f(x, e)  -> KBO_LT, push f(x, e) -> x (swapped).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_v(VAR_x);
    Term rhs = mk_f(mk_v(VAR_x), mk_e());
    AtpAddedRange r = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r.count, 1u);
    CHECK_EQ(s->n_rules, 1u);
    // Stored as rhs -> lhs (the swap).
    CHECK_EQ(s->lhs[0], rhs);
    CHECK_EQ(s->rhs[0], lhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-un-pushes-both");
  {
    // Two distinct FVRs: x and y.  Neither dominates the other on
    // var counts, so KBO returns UN -- unfailing fallback adds both.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_v(VAR_x);
    Term rhs = mk_v(1u);   // VAR_y
    AtpAddedRange r = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(r.first, 0u);
    CHECK_EQ(r.count, 2u);
    CHECK_EQ(s->n_rules, 2u);
    CHECK_EQ(s->lhs[0], lhs);
    CHECK_EQ(s->rhs[0], rhs);
    CHECK_EQ(s->lhs[1], rhs);
    CHECK_EQ(s->rhs[1], lhs);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/orient-and-add-kbo-eq-no-op");
  {
    // Caller bug case: lhs and rhs structurally identical (KBO_EQ).
    // orient_and_add returns count = 0, R unchanged.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange r = thvm_atp_orient_and_add(s, mk_e(), mk_e());
    CHECK_EQ(r.count, 0u);
    CHECK_EQ(s->n_rules, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-empty-added-no-op");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange empty = {0, 0};
    u32 pushed = thvm_atp_generate_cps(s, empty);
    CHECK_EQ(pushed, 0u);
    CHECK_EQ(s->n_cps, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-single-rule-self-overlap");
  {
    // Add one rule via orient_and_add, then generate_cps.  Since R
    // contains only the new rule, the only enumeration is the
    // 1x1 self-overlap (which thvm_critical_pairs always produces
    // at the top position via the fresh rename of j).
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term lhs = mk_f(mk_v(VAR_x), mk_e());
    Term rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(added.count, 1u);
    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 1u);                   // at least the trivial top-overlap
    CHECK_EQ(s->n_cps, pushed);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/generate-cps-old-times-new-direction");
  {
    // Pre-populate R with the assoc rule (manually, no orient).
    // Then add the left-id rule via orient_and_add and run
    // generate_cps.  The (new x all) sweep covers
    // left-id-overlapped-into-assoc; the (old x new) sweep
    // covers assoc-overlapping-leftid.  We don't check the exact
    // count (depends on how many CPs survive each unification);
    // we just check that at least one was emitted.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    // R[0] = assoc: f(f(x,y), z) -> f(x, f(y, z))
    s->lhs[0] = mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u));
    s->rhs[0] = mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u)));
    s->n_rules = 1;

    // Add left-id: f(e, x) -> x.  Right side is var, left side is
    // a CTR, so KBO_GT under our config.
    Term lhs = mk_f(mk_e(), mk_v(VAR_x));
    Term rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, lhs, rhs);
    CHECK_EQ(added.count, 1u);
    CHECK_EQ(added.first, 1u);

    u32 pushed = thvm_atp_generate_cps(s, added);
    CHECK(pushed >= 1u);
    CHECK_EQ(s->n_cps, pushed);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/cp-range-equals-full-when-bounds-cover-all");
  {
    // thvm_critical_pairs == thvm_critical_pairs_range over [0, n) x [0, n).
    Term lhs[1] = { mk_f(mk_v(VAR_x), mk_e()) };
    Term rhs[1] = { mk_v(VAR_x) };
    CriticalPair a[16] = {{0, 0}}, b[16] = {{0, 0}};
    u32 na = thvm_critical_pairs(lhs, rhs, 1, a, 16);
    u32 nb = thvm_critical_pairs_range(lhs, rhs, 1, 0, 1, 0, 1, b, 16);
    CHECK_EQ(na, nb);
  }

  TEST_BEGIN("atp/interreduce-empty-added-no-op");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    AtpAddedRange empty = {0, 0};
    CHECK_EQ(thvm_atp_interreduce(s, empty), 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-drops-specialization");
  {
    // Pre-populate R with a SPECIALIZED rule:
    //   R[0]: f(a, e) -> f(a, a)
    // Add the more-general rule via orient_and_add:
    //   R[1]: f(x, e) -> x          (KBO_GT under DUMMY_CFG)
    // After interreduce, R[0]'s LHS reduces under R[1] (top match
    // with x = a), so it should be dropped and the simplified
    // equation (a, f(a, a)) requeued.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);

    s->lhs[0] = mk_f(mk_a(), mk_e());
    s->rhs[0] = mk_f(mk_a(), mk_a());
    s->n_rules = 1;
    u32 n_cps_before = s->n_cps;

    Term gen_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term gen_rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, gen_lhs, gen_rhs);
    CHECK_EQ(added.count, 1u);
    CHECK_EQ(added.first, 1u);
    CHECK_EQ(s->n_rules, 2u);

    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 1u);
    // R now holds only the new general rule, shifted down to slot 0.
    CHECK_EQ(s->n_rules, 1u);
    CHECK_EQ(term_ext(s->lhs[0]), LAB_f);
    // CP queue grew by one: the requeued (reduced, old_rhs).
    CHECK_EQ(s->n_cps, n_cps_before + 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-keeps-irreducible-rules");
  {
    // R[0]: i(a) -> i(a)         (degenerate; just to fill a slot)
    // Add R[1]: f(x, e) -> x.  R[0]'s LHS i(a) doesn't match
    // f(?, ?) at top, so nothing reduces; R[0] stays.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term ia0 = term_new_ctr(LAB_f - 1, /* unused; want different label */ NULL, 0);
    (void)ia0;
    Term cs[1] = { mk_a() };
    s->lhs[0] = term_new_ctr(2, cs, 1);  // label 2 (i)
    s->rhs[0] = term_new_ctr(2, cs, 1);
    s->n_rules = 1;

    Term gen_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term gen_rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, gen_lhs, gen_rhs);
    CHECK_EQ(added.count, 1u);

    u32 dropped = thvm_atp_interreduce(s, added);
    CHECK_EQ(dropped, 0u);
    CHECK_EQ(s->n_rules, 2u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/interreduce-no-old-rules-no-op");
  {
    // First-rule-add case: added.first == 0, nothing older to
    // interreduce.  Function should return 0 without underflowing.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    Term gen_lhs = mk_f(mk_v(VAR_x), mk_e());
    Term gen_rhs = mk_v(VAR_x);
    AtpAddedRange added = thvm_atp_orient_and_add(s, gen_lhs, gen_rhs);
    CHECK_EQ(added.first, 0u);
    CHECK_EQ(thvm_atp_interreduce(s, added), 0u);
    CHECK_EQ(s->n_rules, 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-no-goal-runs-on");
  {
    // goal_lhs == 0 -> completion mode -> never returns PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-trivial-goal-proves-without-rules");
  {
    // Goal is e == e under empty R; both sides normalize to e
    // (identity), kbo_eq holds, returns ATP_PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_e(), mk_e());
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-closes-under-rule");
  {
    // Goal: f(a, e) == a.  Add rule f(x, e) -> x.  Normalizing
    // f(a, e) under R -> a; rhs already a; goal proves.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    s->lhs[0] = mk_f(mk_v(VAR_x), mk_e());
    s->rhs[0] = mk_v(VAR_x);
    s->n_rules = 1;
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_e()), mk_a());
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/goal-check-doesnt-close-still-running");
  {
    // Goal: a == e.  No rule applies; both sides normalize to
    // themselves; kbo_eq fails (different labels); RUNNING.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_a(), mk_e());
    CHECK_EQ((int)thvm_atp_goal_check(s), (int)ATP_RUNNING);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/step-empty-queue-no-goal-yields-queue-empty");
  {
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    CHECK_EQ((int)thvm_atp_step(s), (int)ATP_QUEUE_EMPTY);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/step-trivial-goal-proves-without-work");
  {
    // Goal e == e is already trivially true; goal_check at top of
    // step fires before anything else.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_e(), mk_e());
    CHECK_EQ((int)thvm_atp_step(s), (int)ATP_PROVED);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/step-cap-zero-times-out-when-no-goal-trivial");
  {
    // step_cap = 0 with a non-trivial goal -- should TIMEOUT before
    // doing any work.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 0);
    thvm_atp_set_goal(s, mk_a(), mk_e());
    CHECK_EQ((int)thvm_atp_step(s), (int)ATP_TIMEOUT);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/run-one-step-prove");
  {
    // Goal: f(a, e) == a.  Push axiom f(x, e) = x onto queue.
    // First step: pops the axiom, normalizes (no R, both stay),
    // not trivially equal, KBO_GT orients it as f(x, e) -> x,
    // adds to R, no interreduce (no old rules), generates self-CPs,
    // goal_check fires: f(a, e) reduces to a, equal -> ATP_PROVED.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_e()), mk_a());
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    CHECK(s->n_rules >= 1u);
    CHECK(s->step <= 4u);   // converges in one or two real steps
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/run-saturates-empty-queue-completion-mode");
  {
    // No goal, push one trivially-self-equal equation.  Step pops
    // it, normalizes (both sides identical), trivializes; queue
    // empties; next step returns QUEUE_EMPTY.  thvm_atp_run loops
    // through and returns QUEUE_EMPTY.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 100);
    thvm_atp_add_equation(s, mk_e(), mk_e());
    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_QUEUE_EMPTY);
    CHECK(s->step >= 1u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/headline-prove-f-a-ia-equals-e-from-group-axioms");
  {
    // Stage 5.5 demo from docs/plans/waldmeister_ic_atp.md sec.5:
    // prove f(a, i(a)) == e from the standard group axioms via
    // saturation.  Under the same KBO config as test_kbo.c
    // (weights i=0, f=1, e=1, a=1; precedence i > f > e > a; w0=1)
    // the right-inverse axiom directly closes the goal once it
    // lands in R.
    //
    //   right-id:    f(x, e)         = x        k = 4
    //   right-inv:   f(x, i(x))      = e        k = 5
    //   assoc:       f(f(x, y), z)   = f(x, f(y, z))   k = 10
    //
    // Cheapest-first selection pops trivial self-CPs early; the
    // right-inv axiom typically fires within ~5 steps.
    AtpState *s = thvm_atp_init(&DUMMY_CFG, 64);

    thvm_atp_set_goal(s,
                      mk_f(mk_a(), mk_i(mk_a())),
                      mk_e());

    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),                 mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))),       mk_e());
    thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));

    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    // The proof is short: well under the step cap.
    CHECK(s->step <= 20u);
    // R picked up at least the right-inverse rule.
    CHECK(s->n_rules >= 1u);
    thvm_atp_free(s);
  }

  thvm_free();
  TEST_REPORT();
}
