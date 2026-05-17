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
// goal in { thm, cpl1, cpl2, subl2, chain3, chain4, chain6, deep5,
// cpgen } (default thm).  chain3/chain4/chain6/deep5 are multi-step
// join goals -- the rungs between the distance-1 lemmas and the
// distance-54 thm -- and all join+verify cleanly.
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
// axiom_inst(a,b,c) is the LHS with its three variables free: for ANY
// a, b, c it matches the axiom rule and rewrites to `c` in one step.
static Term axiom_inst(Term a, Term b, Term c) {
  return nand2(nand2(nand2(a, b), c),
               nand2(a, nand2(nand2(a, c), a)));
}
static Term axiom_lhs(void) { return axiom_inst(fv(0), fv(1), fv(2)); }

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

// Multi-step goals -- the missing rungs between the distance-1 lemmas
// (cpl1/cpl2/subl2) and the distance-54 thm.  wrapk stacks `k` axiom
// instances: each axiom_inst(a,b,X) rewrites to X in one step, so the
// stack reduces to M in exactly k steps (reduce the outermost each
// time).  These join via the AXIOM ALONE -- no completion needed --
// exercising the MNF parent-chain machinery on chains longer than one.
// The axiom LHS holds its `c` argument twice, so |wrapk| ~ 2^k --
// ~885 symbols/side at k=6, still joined by the depth-first deque.
static Term wrapk(Term a, Term b, Term m, u32 k) {
  Term t = m;
  for (u32 i = 0; i < k; i++) t = axiom_inst(a, b, t);
  return t;
}

// Two-sided meet: both fronts reduce to M = nand(fv7,fv8) along
// DISJOINT paths (distinct wrap variables), so the join is a genuine
// middle-meet after ~k steps on each side.
static void goal_chain(Term *l, Term *r, u32 k) {
  Term m = nand2(fv(7), fv(8));
  *l = wrapk(fv(0), fv(1), m, k);
  *r = wrapk(fv(4), fv(5), m, k);
}

// One-sided deep chain: L reduces to M in k steps, R = M -- a single
// long GREEN parent chain, the RED seed static at M.
static void goal_deep(Term *l, Term *r, u32 k) {
  Term m = nand2(fv(7), fv(8));
  *l = wrapk(fv(0), fv(1), m, k);
  *r = m;
}

int main(int argc, char **argv) {
  thvm_init();

  const char *goal = (argc > 1) ? argv[1] : "thm";
  u32    step_cap  = (argc > 2) ? (u32)strtoul(argv[2], NULL, 10) : 200000u;
  double wall_cap  = (argc > 3) ? strtod(argv[3], NULL)          : 120.0;

  // Reduction ordering.  Default KBO (one binary symbol `nand`,
  // weight 1, var weight 1 -- KBO weight is plain symbol count, so a
  // deeply-nested lhs outweighs a bare-variable rhs).  ATP_BENCH_ORD
  // selects an alternative for the ordering experiment:
  //   "kbo0" -- KBO with var_weight 0 (variables weigh nothing)
  //   "lpo"  -- lexicographic path ordering instead of KBO
  const char *ord_env = getenv("ATP_BENCH_ORD");
  int use_lpo  = (ord_env != NULL && strcmp(ord_env, "lpo")  == 0);
  int use_kbo0 = (ord_env != NULL && strcmp(ord_env, "kbo0") == 0);
  static u32 weights[2]    = { 0u, 1u };
  static u32 precedence[2] = { 0u, 1u };
  KboConfig cfg = {
    .weights    = weights,
    .precedence = precedence,
    .n_labels   = 2u,
    .var_weight = use_kbo0 ? 0u : 1u,
  };
  static u32 lpo_prec[2] = { 0u, 1u };
  static LpoConfig lpo = { .precedence = lpo_prec, .n_labels = 2u };

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

  // Opt-in in-loop GC.  ATP_BENCH_GC=<half-space-cells> enables the
  // Cheney collector so a long completion run floats around its live
  // working set instead of climbing into from-space exhaustion.  Off
  // by default (flat 128M heap) so the cost numbers stay comparable
  // to earlier milestones; set it to exercise thvm_atp_gc_collect --
  // and, under -DATP_CP_GRAPH, the 8b cp_graph GC root.
  {
    const char *gc_env = getenv("ATP_BENCH_GC");
    if (gc_env != NULL && gc_env[0] != '\0') {
      u64 half = strtoull(gc_env, NULL, 10);
      if (half > 0) gc_init(half);
    }
  }

  AtpState *s = thvm_atp_init(&cfg, step_cap);
  if (use_lpo) thvm_atp_set_lpo(s, &lpo);
  thvm_atp_add_equation(s, axiom_lhs(), fv(2));

  Term gl = 0, gr = 0;
  if      (strcmp(goal, "cpl1")   == 0) goal_cpl1(&gl, &gr);
  else if (strcmp(goal, "cpl2")   == 0) goal_cpl2(&gl, &gr);
  else if (strcmp(goal, "subl2")  == 0) goal_subl2(&gl, &gr);
  else if (strcmp(goal, "chain3") == 0) goal_chain(&gl, &gr, 3u);
  else if (strcmp(goal, "chain4") == 0) goal_chain(&gl, &gr, 4u);
  else if (strcmp(goal, "chain6") == 0) goal_chain(&gl, &gr, 6u);
  else if (strcmp(goal, "deep5")  == 0) goal_deep(&gl, &gr, 5u);
  else                                  goal_thm(&gl, &gr);
  thvm_atp_set_goal(s, gl, gr);

  printf("=== Wolfram-axiom completion : goal=%s ===\n", goal);
  printf("ordering=%s  step_cap=%u  wall_cap=%.0fs\n",
         use_lpo ? "lpo" : (use_kbo0 ? "kbo0" : "kbo"), step_cap, wall_cap);

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
  printf("   dropped: joinable=%u queue-subsumed=%u "
         "rule-subsumed=%u connected=%u\n",
         s->n_cps_dropped_joinable, s->n_cps_dropped_queue_subsumed,
         s->n_cps_dropped_rule_subsumed, s->n_cps_dropped_connected);

#ifdef ATP_NORM_STATS
  // 8b: optimal-sharing ratio of the cross-CP normalization memo.
  // A hit = a subterm shared with an already-visited CP whose normal
  // form was reused; misses = distinct subterm cells actually
  // normalized.  hits/(hits+misses) is the work the sharing saved.
  {
    u64 hits = 0, misses = 0;
    double secs = 0.0;
    thvm_atp_norm_stats(&hits, &misses, &secs);
    u64 total = hits + misses;
    double ratio = (total > 0) ? (100.0 * (double)hits / (double)total) : 0.0;
    double frac  = (el > 0.0) ? (100.0 * secs / el) : 0.0;
    printf("   norm-memo: %llu hits / %llu distinct-cells  (%.1f%% shared)\n",
           (unsigned long long)hits, (unsigned long long)misses, ratio);
    printf("   norm-sweep: %.2fs  (%.1f%% of %.1fs total)\n",
           secs, frac, el);
  }
#endif

#ifdef ATP_MATCH_STATS
  // 8e: shared-traversal match stats.  memo_hits / (hits + misses) is
  // the per-subterm sharing ratio -- how often a (pattern_cell,
  // subject_cell) pair was served from the memo rather than walked.
  // node_visits / calls is the average traversal size.
  {
    u64 calls = 0, nodes = 0, hits = 0, miss = 0;
    double secs = 0.0;
    thvm_atp_match_stats(&calls, &nodes, &hits, &miss, &secs);
    u64 mtot = hits + miss;
    double mratio = (mtot > 0) ? (100.0 * (double)hits / (double)mtot) : 0.0;
    double frac   = (el > 0.0) ? (100.0 * secs / el) : 0.0;
    printf("   match-multi: %llu calls  %llu node-visits\n",
           (unsigned long long)calls, (unsigned long long)nodes);
    printf("   match-memo: %llu hits / %llu miss  (%.1f%% shared)\n",
           (unsigned long long)hits, (unsigned long long)miss, mratio);
    printf("   match-sweep: %.2fs  (%.1f%% of %.1fs total)\n",
           secs, frac, el);
  }
#endif

#ifdef ATP_FV_INDEX
  // 7d: subsumption-index retrieval stats.  candidates / query is the
  // average CPs the discrimination tree handed to thvm_match (the
  // array scan handed it n_cps); matchcalls is the thvm_match volume
  // the index issued -- the milestone-7 scan would have issued
  // ~4 * n_cps per query.  node_visits / query is the average tree
  // nodes the descent touched.
  {
    u64 calls = 0, nodevisits = 0, cands = 0, matchcalls = 0;
    u32 nodes = 0;
    thvm_atp_fv_stats(s, &calls, &nodevisits, &cands, &matchcalls, &nodes);
    double avg_cand = (calls > 0) ? ((double)cands / (double)calls) : 0.0;
    double avg_node = (calls > 0) ? ((double)nodevisits / (double)calls) : 0.0;
    printf("   fv-index: %llu queries  %u tree-nodes\n",
           (unsigned long long)calls, nodes);
    printf("   fv-retrieval: %.1f candidates/query  %.1f tree-nodes/query\n",
           avg_cand, avg_node);
    printf("   fv-match: %llu thvm_match calls on survivors\n",
           (unsigned long long)matchcalls);
  }
#endif

  thvm_atp_free(s);
  return (st == ATP_PROVED) ? 0 : 1;
}
