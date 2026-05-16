// test_atp_wolfram_bench.c -- standalone completion-engine bench.
//
// Runs the single Wolfram axiom -> DoubleNegation completion
// directly against the C engine (thvm_atp_init / _step), printing
// the rule / CP-queue trajectory.  Milestone-7b convergence probe.
//
// Not a pass/fail test (not in the Makefile TESTS list) -- build
// with `make bin/test_atp_wolfram_bench` and run:
//   bin/test_atp_wolfram_bench [step_cap] [wall_cap_seconds]
//
// Unlike a `timeout`-wrapped wolframscript probe, this is a plain C
// process with its own in-loop wall-clock guard: it always exits
// cleanly and can never orphan a WolframKernel.

#include "../src/thvm.c"
#include <time.h>

// nand is the single binary function symbol (CTR label 1).
#define L_NAND 1u

static Term nand2(Term x, Term y) {
  Term c[2] = { x, y };
  return term_new_ctr(L_NAND, c, 2);
}
static Term fv(u32 id) { return term_new_fvr(id); }

int main(int argc, char **argv) {
  thvm_init();

  u32    step_cap = (argc > 1) ? (u32)strtoul(argv[1], NULL, 10) : 200000u;
  double wall_cap = (argc > 2) ? strtod(argv[2], NULL)          : 120.0;

  // KBO: one binary symbol `nand` (label 1), weight 1, var weight 1
  // -- KBO weight is then plain symbol count, so the deeply-nested
  // axiom lhs outweighs the bare-variable rhs and orients lhs -> rhs.
  static u32 weights[2]    = { 0u, 1u };
  static u32 precedence[2] = { 0u, 1u };
  KboConfig cfg = {
    .weights    = weights,
    .precedence = precedence,
    .n_labels   = 2u,
    .var_weight = 1u,
  };

  AtpState *s = thvm_atp_init(&cfg, step_cap);

  // Axiom: nand(nand(nand(a,b),c), nand(a,nand(nand(a,c),a))) == c
  Term a = fv(0), b = fv(1), c = fv(2);
  Term ax_lhs = nand2(nand2(nand2(a, b), c),
                      nand2(a, nand2(nand2(a, c), a)));
  thvm_atp_add_equation(s, ax_lhs, fv(2));

  // Goal (DoubleNegation): nand(nand(w,w), nand(w,w)) == w
  Term w = fv(3);
  thvm_atp_set_goal(s, nand2(nand2(w, w), nand2(w, w)), fv(3));

  printf("=== Wolfram-axiom DoubleNegation completion ===\n");
  printf("step_cap=%u  wall_cap=%.0fs\n", step_cap, wall_cap);

  clock_t   t0  = clock();
  AtpStatus st  = ATP_RUNNING;
  u32       i   = 0;
  u32       max_cps = 0;
  for (; i < step_cap; i++) {
    st = thvm_atp_step(s);
    if (s->n_cps > max_cps) max_cps = s->n_cps;
    if (st != ATP_RUNNING) break;
    double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
    if (i > 0 && i % 250u == 0u) {
      printf("  step %7u  rules=%-6u cps=%-8u %.1fs\n",
             i, s->n_rules, s->n_cps, el);
      fflush(stdout);
    }
    if (el > wall_cap) {
      printf("  [wall cap %.0fs reached]\n", wall_cap);
      break;
    }
  }
  double el = (double)(clock() - t0) / CLOCKS_PER_SEC;

  const char *sn =
      (st == ATP_PROVED)      ? "PROVED" :
      (st == ATP_REFUTED)     ? "REFUTED" :
      (st == ATP_QUEUE_EMPTY) ? "QUEUE_EMPTY (saturated, no proof)" :
      (st == ATP_TIMEOUT)     ? "TIMEOUT (step cap)" :
                                "RUNNING (wall cap)";
  printf("=> %s\n", sn);
  printf("   steps=%u  rules=%u  cps=%u  max_cps=%u  %.1fs\n",
         i, s->n_rules, s->n_cps, max_cps, el);

  thvm_atp_free(s);
  return (st == ATP_PROVED) ? 0 : 1;
}
