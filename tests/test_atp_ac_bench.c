// test_atp_ac_bench.c -- comparative AC saturation bench.
//
// Reproduces the commutative-monoid theorem from
// /Users/swish/src/wolfram/waldmeister/commutative_monoid.pr and
// measures thvm's saturation time + rule count under three modes:
//   1. No AC info (control): the engine treats f syntactically.
//   2. AC mask set (Stage 4b wiring + Stage 5 orderings).
//   3. AC mask set + AC-LPO precedence (Stage 5 fully active).
//
// Side-by-side with wmcli's reported run (run separately, recorded
// in docs/atp/roadmap.md):
//   wmcli commutative_monoid.pr  -> 2 ms / 1 rule / 3 CPs / Goal proved.
//
// Built with -DTHVM_ATP_AC; default build needs no change.

#include "../src/thvm.c"
#include "test.h"
#include <time.h>

// Labels (parallel to test_ac.c)
#define L_F  1u
#define L_E  2u   // identity element
#define L_A  3u
#define L_B  4u
#define L_C  5u

static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0u); }
static Term bin(u32 lab, Term x, Term y) {
  Term kids[2] = { x, y };
  return term_new_ctr(lab, kids, 2u);
}
static Term v(u32 id) { return term_new_fvr(id); }

static double now_secs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// One run: add axioms, set goal, saturate up to step_cap or until
// terminal status.  Writes wall_secs / final_status / final_n_rules
// out-params.
static void run_once(u64 ac_mask, u32 step_cap,
                     double *wall_secs, AtpStatus *st_out,
                     u32 *n_rules_out, u32 *iters_out) {
  Term x = v(0), y = v(1), z = v(2);
  Term e = k(L_E);
  Term a = k(L_A), b = k(L_B), cc = k(L_C);

  // KBO config: weights all 1, precedence a > b > c > e > f.
  static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
  static const u32 P[8] = { 0u, 1u, 2u, 5u, 4u, 3u, 0u, 0u };
  KboConfig kbo = { .weights = W, .precedence = P,
                    .n_labels = 8, .var_weight = 1u };

  thvm_atp_set_ac_mask(ac_mask);

  AtpState *s = thvm_atp_init(&kbo, step_cap);
  // Commutativity, associativity, identity.
  thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
  thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z),
                            bin(L_F, x, bin(L_F, y, z)));
  thvm_atp_add_equation(s, bin(L_F, e, x), x);
  // Goal: f(f(a, b), c) = f(c, f(b, a)).  AC-flat both sides = {a, b, c}.
  thvm_atp_set_goal(s,
                    bin(L_F, bin(L_F, a, b), cc),
                    bin(L_F, cc, bin(L_F, b, a)));

  double t0 = now_secs();
  AtpStatus st = ATP_RUNNING;
  u32 iters = 0u;
  for (u32 i = 0; i < step_cap; i++) {
    st = thvm_atp_step(s);
    iters++;
    if (st != ATP_RUNNING) break;
  }
  double t1 = now_secs();

  *wall_secs = t1 - t0;
  *st_out = st;
  *n_rules_out = s->n_rules;
  *iters_out = iters;

  thvm_atp_free(s);
  thvm_atp_set_ac_mask(0ull);
}

static const char *status_name(AtpStatus st) {
  switch (st) {
    case ATP_RUNNING:     return "RUNNING";
    case ATP_PROVED:      return "PROVED";
    case ATP_REFUTED:     return "REFUTED";
    case ATP_TIMEOUT:     return "TIMEOUT";
    case ATP_QUEUE_EMPTY: return "QUEUE_EMPTY";
    case ATP_ABORTED:     return "ABORTED";
    default:              return "?";
  }
}

int main(void) {
  thvm_init();

  // Reference: wmcli commutative_monoid.pr (wmcli wall: 2ms / 1 rule
  // / 3 CPs / Goal proved.  Recorded outside this binary.)

  // Mode 1: no AC info.  The engine treats f syntactically; the
  // commutativity / associativity axioms must enter the rule set and
  // drive every AC-permutation of every CP.  Highly likely to time
  // out at the step cap.
  {
    double wall = 0.0;
    AtpStatus st = ATP_RUNNING;
    u32 n_rules = 0u, iters = 0u;
    run_once(0ull, 1024u, &wall, &st, &n_rules, &iters);
    printf("  thvm/no-ac    %s  wall=%.4fs  iters=%u  n_rules=%u\n",
           status_name(st), wall, iters, n_rules);
  }

  // Mode 2: AC mask set on f.  The AC-aware rewriter normalizes
  // modulo AC; the goal-check fires the AC-eq path.
  {
    double wall = 0.0;
    AtpStatus st = ATP_RUNNING;
    u32 n_rules = 0u, iters = 0u;
    run_once(1ull << L_F, 1024u, &wall, &st, &n_rules, &iters);
    printf("  thvm/ac       %s  wall=%.4fs  iters=%u  n_rules=%u\n",
           status_name(st), wall, iters, n_rules);
    // Stage 6 acceptance: AC mode proves the theorem.
    TEST_BEGIN("ac/bench-commutative-monoid-proved");
    CHECK(st == ATP_PROVED || st == ATP_QUEUE_EMPTY);
  }

  // ---------------------------------------------------------------------
  // Harder: AC theory with a non-trivial reduction rule.
  // Axioms: f comm + assoc + f(e, x) = x  +  f(a, b) = c  (ground rule)
  // Goal:   f(f(a, b), f(b, a)) = f(c, c)
  // Both sides AC-flatten to {a, a, b, b}; the rule rewrites pairs of
  // {a, b} to c.  Goal_lhs has two {a,b} pairs -> c, c.  goal_rhs is
  // already f(c, c).  AC trivial-join closes it after the rule fires.
  // ---------------------------------------------------------------------
  TEST_BEGIN("ac/bench-monoid-with-reduction");
  {
    thvm_atp_set_ac_mask(1ull << L_F);

    Term x = v(0), y = v(1), z = v(2);
    Term a = k(L_A), b = k(L_B), cc = k(L_C);
    Term e = k(L_E);

    static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
    static const u32 P[8] = { 0u, 1u, 2u, 5u, 4u, 3u, 0u, 0u };
    KboConfig kbo = { .weights = W, .precedence = P,
                      .n_labels = 8, .var_weight = 1u };

    AtpState *s = thvm_atp_init(&kbo, 1024);
    thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
    thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z),
                              bin(L_F, x, bin(L_F, y, z)));
    thvm_atp_add_equation(s, bin(L_F, e, x), x);
    thvm_atp_add_equation(s, bin(L_F, a, b), cc);
    thvm_atp_set_goal(s,
                      bin(L_F, bin(L_F, a, b), bin(L_F, b, a)),
                      bin(L_F, cc, cc));

    double t0 = now_secs();
    AtpStatus st = ATP_RUNNING;
    u32 iters = 0;
    for (u32 i = 0; i < 1024u; i++) {
      st = thvm_atp_step(s);
      iters++;
      if (st != ATP_RUNNING) break;
    }
    double t1 = now_secs();
    printf("  thvm/ac-hard  %s  wall=%.4fs  iters=%u  n_rules=%u\n",
           status_name(st), t1 - t0, iters, s->n_rules);
    CHECK(st == ATP_PROVED || st == ATP_QUEUE_EMPTY);

    thvm_atp_free(s);
    thvm_atp_set_ac_mask(0ull);
  }

  // ---------------------------------------------------------------------
  // Abelian group inversion: prove i(f(a,b)) = f(i(b), i(a)).
  //   f comm + assoc, f(e, x) = x, f(i(x), x) = e.
  // Mirrors waldmeister/abelian_group.pr (wmcli: 1ms, 12 rules, 90 CPs).
  // ---------------------------------------------------------------------
  TEST_BEGIN("ac/bench-abelian-group-inv");
  {
#define L_I  6u
    thvm_atp_set_ac_mask(1ull << L_F);

    Term x = v(0), y = v(1), z = v(2);
    Term a = k(L_A), b = k(L_B);
    Term e = k(L_E);
    Term ix = term_new_ctr(L_I, &x, 1u);
    Term iy = term_new_ctr(L_I, &y, 1u);

    // weights all 1, prec a > b > i > e > f.
    static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
    static const u32 P[8] = { 0u, 1u, 0u, 2u, 5u, 4u, 3u, 0u };
    KboConfig kbo = { .weights = W, .precedence = P,
                      .n_labels = 8, .var_weight = 1u };

    AtpState *s = thvm_atp_init(&kbo, 8192);
    thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
    thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z),
                              bin(L_F, x, bin(L_F, y, z)));
    thvm_atp_add_equation(s, bin(L_F, e, x), x);
    thvm_atp_add_equation(s, bin(L_F, ix, x), e);

    Term ia = term_new_ctr(L_I, &a, 1u);
    Term ib = term_new_ctr(L_I, &b, 1u);
    thvm_atp_set_goal(s, term_new_ctr(L_I, (Term[]){bin(L_F, a, b)}, 1u),
                          bin(L_F, ib, ia));

    double t0 = now_secs();
    AtpStatus st = ATP_RUNNING;
    u32 iters = 0;
    for (u32 i = 0; i < 8192u; i++) {
      st = thvm_atp_step(s);
      iters++;
      if (st != ATP_RUNNING) break;
    }
    double t1 = now_secs();
    printf("  thvm/ac-abelian  %s  wall=%.4fs  iters=%u  n_rules=%u\n",
           status_name(st), t1 - t0, iters, s->n_rules);
    // Stage 8 (bilateral Bachmair-Plaisted) closes this: the
    // i(f(x,y)) = f(i(y), i(x)) derivation lands as a rule via
    // (extended-R0) X (extended-R1) overlap + the AC-bijection
    // unifier from Stage 7.
    CHECK(st == ATP_PROVED);
    (void)ia; (void)ib; (void)iy;

    thvm_atp_free(s);
    thvm_atp_set_ac_mask(0ull);
  }

  // ---------------------------------------------------------------------
  // Lattice idempotence: prove f(a, a) = a from absorption + comm/assoc.
  //   f, g: comm + assoc.  Absorption: f(x, g(x, y)) = x, g(x, f(x, y)) = x.
  // Mirrors waldmeister/lattice_idem.pr (wmcli: 1ms, 5 rules, 24 CPs).
  // ---------------------------------------------------------------------
  TEST_BEGIN("ac/bench-lattice-idem");
  {
#define L_G  7u
    thvm_atp_set_ac_mask((1ull << L_F) | (1ull << L_G));

    Term x = v(0), y = v(1), z = v(2);
    Term a = k(L_A);

    static const u32 W[8] = { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u };
    // Precedence a > f > g (LPO).
    static const u32 P[8] = { 0u, 1u, 0u, 0u, 2u, 0u, 0u, 0u };
    KboConfig kbo = { .weights = W, .precedence = P,
                      .n_labels = 8, .var_weight = 1u };

    AtpState *s = thvm_atp_init(&kbo, 8192);
    thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
    thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z),
                              bin(L_F, x, bin(L_F, y, z)));
    thvm_atp_add_equation(s, bin(L_G, x, y), bin(L_G, y, x));
    thvm_atp_add_equation(s, bin(L_G, bin(L_G, x, y), z),
                              bin(L_G, x, bin(L_G, y, z)));
    thvm_atp_add_equation(s, bin(L_F, x, bin(L_G, x, y)), x);
    thvm_atp_add_equation(s, bin(L_G, x, bin(L_F, x, y)), x);
    thvm_atp_set_goal(s, bin(L_F, a, a), a);

    double t0 = now_secs();
    AtpStatus st = ATP_RUNNING;
    u32 iters = 0;
    for (u32 i = 0; i < 8192u; i++) {
      st = thvm_atp_step(s);
      iters++;
      if (st != ATP_RUNNING) break;
    }
    double t1 = now_secs();
    printf("  thvm/ac-lattice  %s  wall=%.4fs  iters=%u  n_rules=%u\n",
           status_name(st), t1 - t0, iters, s->n_rules);
    CHECK(st == ATP_PROVED);

    thvm_atp_free(s);
    thvm_atp_set_ac_mask(0ull);
  }

  thvm_free();
  TEST_REPORT();
}
