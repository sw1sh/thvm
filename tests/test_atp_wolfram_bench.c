// test_atp_wolfram_bench.c -- standalone completion-engine bench.
//
// Runs a single-Wolfram-axiom completion directly against the C
// engine (thvm_atp_init / _step), printing the rule / CP-queue
// trajectory.  Milestone-7b convergence probe + inference-ladder
// debugger.
//
// Not a pass/fail test (not in the Makefile TESTS list) -- build
// with `make bin/test_atp_wolfram_bench` and run:
//   bin/test_atp_wolfram_bench [goal] [step_cap] [wall_cap_s]
// goal in { thm, cpl1, cpl2, subl2 } (default thm).
//
// The "ladder" idea (debug whether inference is even right): WL's
// own FindEquationalProof[DoubleNegation, WolframAxioms] proof is a
// DAG of CriticalPairLemma + SubstitutionLemma nodes.  cpl1 / cpl2
// are critical pairs of the axiom with itself (distance 1 from the
// axiom); subl2 is ~4 inferences deep; thm is the 54-step target.
// If the engine cannot prove a distance-1 lemma, the inference
// (CP generation / orientation / goal-check) is broken -- not just
// slow.
//
// Unlike a `timeout`-wrapped wolframscript probe, this is a plain C
// process with an in-loop wall-clock guard: it always exits
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

// Wolfram axiom:
//   nand(nand(nand(a,b),c), nand(a,nand(nand(a,c),a))) == c
static Term axiom_lhs(void) {
  Term a = fv(0), b = fv(1), c = fv(2);
  return nand2(nand2(nand2(a, b), c),
               nand2(a, nand2(nand2(a, c), a)));
}

// Goal equations.  *_lhs / *_rhs build the two sides; free
// variables fv(0..3) = a,b,c,(e2|w) are universally quantified.

// thm = DoubleNegation: nand(nand(w,w),nand(w,w)) == w
static void goal_thm(Term *l, Term *r) {
  Term w = fv(3);
  *l = nand2(nand2(w, w), nand2(w, w));
  *r = fv(3);
}

// cpl1 (CriticalPairLemma 1, distance 1): a critical pair of the
// axiom with itself.
//   nand(a,nand(nand(a,b),a))
//     == nand(b,nand(nand(a,c),
//               nand(nand(nand(a,c),nand(a,nand(nand(a,b),a))),
//                    nand(a,c))))
static void goal_cpl1(Term *l, Term *r) {
  Term a = fv(0), b = fv(1), c = fv(2);
  Term W  = nand2(a, nand2(nand2(a, b), a));   // == lhs
  Term ac = nand2(a, c);
  Term Z  = nand2(ac, W);
  Term Y  = nand2(Z, nand2(a, c));
  Term X  = nand2(nand2(a, c), Y);
  *l = nand2(a, nand2(nand2(a, b), a));
  *r = nand2(b, X);
}

// cpl2 (CriticalPairLemma 2, distance 1):
//   a == nand(nand(b,a),
//             nand(P, nand(nand(P,a), P)))   where P = nand(nand(c,e2),b)
static void goal_cpl2(Term *l, Term *r) {
  Term a = fv(0), b = fv(1), c = fv(2), e2 = fv(3);
  Term P = nand2(nand2(c, e2), b);
  *l = fv(0);
  *r = nand2(nand2(b, a),
             nand2(P, nand2(nand2(P, a), P)));
}

// subl2 (SubstitutionLemma 2, ~4 inferences deep):
//   a == nand(nand(nand(b,Q),a), nand(c,nand(nand(c,a),c)))
//        where Q = nand(nand(b,c),b)
static void goal_subl2(Term *l, Term *r) {
  Term a = fv(0), b = fv(1), c = fv(2);
  Term Q = nand2(nand2(b, c), b);
  *l = fv(0);
  *r = nand2(nand2(nand2(b, Q), a),
             nand2(c, nand2(nand2(c, a), c)));
}

int main(int argc, char **argv) {
  thvm_init();

  const char *goal = (argc > 1) ? argv[1] : "thm";
  u32    step_cap  = (argc > 2) ? (u32)strtoul(argv[2], NULL, 10) : 200000u;
  double wall_cap  = (argc > 3) ? strtod(argv[3], NULL)          : 120.0;

  // KBO: one binary symbol `nand` (label 1), weight 1, var weight 1
  // -- KBO weight is then plain symbol count, so a deeply-nested
  // lhs outweighs a bare-variable rhs and orients lhs -> rhs.
  static u32 weights[2]    = { 0u, 1u };
  static u32 precedence[2] = { 0u, 1u };
  KboConfig cfg = {
    .weights    = weights,
    .precedence = precedence,
    .n_labels   = 2u,
    .var_weight = 1u,
  };

  // cpgen mode: generate the critical pairs of the axiom with
  // itself -- the distance-1 lemmas -- in ONE CP-generation call,
  // no completion loop.  This is the direct "does inference work
  // one step from the axiom" test: CriticalPairLemma 1/2 of WL's
  // proof should appear among the output.
  if (strcmp(goal, "cpgen") == 0) {
    Term ax_l = axiom_lhs();
    Term ax_r = fv(2);
    static CriticalPair cps[8192];
    u32 n = thvm_critical_pairs(&ax_l, &ax_r, 1u, cps, 8192u);
    printf("=== axiom critical pairs (1-step, axiom x axiom) ===\n");
    printf("generated %u critical pairs\n", n);
    u32 show = (n < 60u) ? n : 60u;
    for (u32 k = 0; k < show; k++) {
      char lb[768], rb[768];
      atp_pretty_term(cps[k].lhs, lb, sizeof lb);
      atp_pretty_term(cps[k].rhs, rb, sizeof rb);
      printf("  CP%-3u %s = %s\n", k, lb, rb);
    }
    return (n > 0u) ? 0 : 1;
  }

  AtpState *s = thvm_atp_init(&cfg, step_cap);
  thvm_atp_add_equation(s, axiom_lhs(), fv(2));

  Term gl = 0, gr = 0;
  if      (strcmp(goal, "cpl1")  == 0) goal_cpl1(&gl, &gr);
  else if (strcmp(goal, "cpl2")  == 0) goal_cpl2(&gl, &gr);
  else if (strcmp(goal, "subl2") == 0) goal_subl2(&gl, &gr);
  else                                 goal_thm(&gl, &gr);
  thvm_atp_set_goal(s, gl, gr);

  printf("=== Wolfram-axiom completion : goal=%s ===\n", goal);
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
  printf("   goal=%s  steps=%u  rules=%u  cps=%u  max_cps=%u  %.1fs\n",
         goal, i, s->n_rules, s->n_cps, max_cps, el);

  thvm_atp_free(s);
  return (st == ATP_PROVED) ? 0 : 1;
}
