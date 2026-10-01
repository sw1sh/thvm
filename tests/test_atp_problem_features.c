// Problem features phi(p) and the static gate replica
// (tools/atp_cli/problem_features.c) on small inline problems: phi(p)'s
// gate bits, sizes, goal shape, algebra and TPTP header fields, the
// replica's verdict for the lanes each gate bit decides, a .pr the bench's
// reader rejects, and the same answers under --proof.  Runs no search: the
// replica replays the front end only.
//
// It links the whole FindProof translation unit; FINDPROOF_LIB renames
// its main.
#define FINDPROOF_LIB 1
#include "../tools/thvm_atp_cli.c"

static int g_checks = 0;
static int g_fails  = 0;

static void check(int ok, const char *problem, const char *what) {
  g_checks++;
  if (!ok) {
    g_fails++;
    printf("  FAIL %s: %s\n", problem, what);
  }
}

static void check_phi(const char *problem, const AtpPhi *phi, int col,
                      double want) {
  char what[200];
  double got = (double)phi->v[col];
  snprintf(what, sizeof what, "%s = %.9g, want %.9g", PHI_NAMES[col], got,
           want);
  check(fabs(got - want) < 1e-6, problem, what);
}

static int lane_ix(const char *name) {
  for (int i = 0; i < N_SCHED; i++) {
    if (strcmp(SCHEDULE[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

// verdict: TP_LANE_*; why: the predicted give-up line, NULL for none.
static void check_lane(const char *problem, const TpLaneGate *g,
                       const char *lane, int verdict, int strict,
                       const char *why) {
  char what[400];
  int i = lane_ix(lane);
  if (i < 0) {
    snprintf(what, sizeof what, "no schedule lane %s", lane);
    check(0, problem, what);
    return;
  }
  const char *want = (why != NULL) ? why : "";
  int ok = g[i].verdict == verdict && g[i].strict == strict &&
           strcmp(g[i].why, want) == 0;
  snprintf(what, sizeof what, "lane %s: %s strict=%d \"%s\", want %s "
           "strict=%d \"%s\"", lane, phi_verdict_name(g[i].verdict),
           g[i].strict, g[i].why, phi_verdict_name(verdict), strict, want);
  check(ok, problem, what);
}

static void check_gates(const char *problem, const TpLaneGate *g,
                        const char *lane, const char *gates) {
  char what[200];
  int i = lane_ix(lane);
  int ok = i >= 0 && strcmp(g[i].gates, gates) == 0;
  snprintf(what, sizeof what, "lane %s arms \"%s\", want \"%s\"", lane,
           (i >= 0) ? g[i].gates : "?", gates);
  check(ok, problem, what);
}

// Write `text` to a fresh file, compute its features, remove the file.
static int features(const char *text, AtpPhi *phi, TpLaneGate *g) {
  const char *dir = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_phi_test_XXXXXX",
           (dir != NULL && dir[0] != '\0') ? dir : "/tmp");
  int fd = mkstemp(path);
  if (fd < 0) {
    return -1;
  }
  size_t n = strlen(text);
  int ok = (write(fd, text, n) == (ssize_t)n);
  close(fd);
  int rc = ok ? tp_problem_features(path, phi, g) : -1;
  unlink(path);
  return rc;
}

// A group fragment with a ground goal and a TPTP header.
static const char *const P_GROUP =
  "% Rating   : 0.35 v9.0.0, 0.30 v8.2.0\n"
  "% SPC      : CNF_UNS_RFO_PEQ_UEQ\n"
  "cnf(left_identity, axiom, mult(e,X) = X).\n"
  "cnf(left_inverse, axiom, mult(inv(X),X) = e).\n"
  "cnf(associativity, axiom, mult(mult(X,Y),Z) = mult(X,mult(Y,Z))).\n"
  "cnf(goal, negated_conjecture, mult(a,b) != mult(b,a)).\n";

// An AC operator, a distributivity law, and a goal over every symbol.
static const char *const P_RING =
  "cnf(add_comm, axiom, add(X,Y) = add(Y,X)).\n"
  "cnf(add_assoc, axiom, add(add(X,Y),Z) = add(X,add(Y,Z))).\n"
  "cnf(distrib, axiom, mult(X,add(Y,Z)) = add(mult(X,Y),mult(X,Z))).\n"
  "cnf(goal, negated_conjecture, mult(a,add(b,c)) != add(mult(a,c),mult(a,b))).\n";

// TPTP's -10 encoding: a dispatcher, one Horn rule, a ground fact.
static const char *const P_HORN =
  "cnf(ifeq_axiom, axiom, ifeq(A,A,B,C) = B).\n"
  "cnf(rule_1, axiom, ifeq(p(X),true,q(X),true) = true).\n"
  "cnf(fact_1, axiom, p(a) = true).\n"
  "cnf(goal, negated_conjecture, q(a) != true).\n";

// A pure function definition and an existential goal.
static const char *const P_DEF =
  "cnf(def_f, axiom, f(X) = g(X,X)).\n"
  "cnf(ax_g, axiom, g(X,c) = X).\n"
  "cnf(goal, negated_conjecture, f(Y) != Y).\n";

// No negated conjecture: every lane gives up before searching.
static const char *const P_NOGOAL =
  "cnf(a1, axiom, f(X) = X).\n";

// A symbol whose name holds "LPO": the bench's .pr reader takes the
// ORDERING line that names it for an LPO ordering and ends the run (pr_die,
// exit 2).  The replica returns that line for the lanes that reach the
// reader and keeps its own process.
static const char *const P_LPO =
  "cnf(a1, axiom, fLPO(fLPO(X)) = X).\n"
  "cnf(goal, negated_conjecture, fLPO(a) != a).\n";

static void test_names(void) {
  for (int i = 0; i < ATP_PHI_N; i++) {
    char what[80];
    snprintf(what, sizeof what, "column %d has a name", i);
    check(PHI_NAMES[i] != NULL && PHI_NAMES[i][0] != '\0', "names", what);
    for (int j = 0; j < i && PHI_NAMES[i] != NULL; j++) {
      if (PHI_NAMES[j] != NULL && strcmp(PHI_NAMES[i], PHI_NAMES[j]) == 0) {
        snprintf(what, sizeof what, "columns %d and %d share a name", j, i);
        check(0, "names", what);
      }
    }
  }
}

static void test_group(TpLaneGate *g) {
  const char *p = "group";
  AtpPhi phi;
  check(features(P_GROUP, &phi, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_GOAL_EXISTENTIAL, 0);
  check_phi(p, &phi, ATP_PHI_EQUALS_GOAL, 0);
  check_phi(p, &phi, ATP_PHI_IFEQ_DISPATCHER, 0);
  check_phi(p, &phi, ATP_PHI_HORN_SLICE, 0);
  check_phi(p, &phi, ATP_PHI_DEF_UNFOLD, 0);
  check_phi(p, &phi, ATP_PHI_AX_GROUND_COMPOUND, 0);
  check_phi(p, &phi, ATP_PHI_CONJ_CONE_PARTIAL, 1);
  check_phi(p, &phi, ATP_PHI_DISTRIB_LAW, 0);
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 1);
  check_phi(p, &phi, ATP_PHI_SINE, 0);
  check_phi(p, &phi, ATP_PHI_ASSOC_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_COMM_SYMS, 0);
  check_phi(p, &phi, ATP_PHI_AC_PAIRS, 0);
  check_phi(p, &phi, ATP_PHI_UNIT_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_INVERSE_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_AXIOMS, 3);
  check_phi(p, &phi, ATP_PHI_HYPOTHESES, 0);
  check_phi(p, &phi, ATP_PHI_GOALS, 1);
  check_phi(p, &phi, ATP_PHI_SYMS, 5);
  check_phi(p, &phi, ATP_PHI_SYMS_AR0, 3);
  check_phi(p, &phi, ATP_PHI_SYMS_AR1, 1);
  check_phi(p, &phi, ATP_PHI_SYMS_AR2, 1);
  check_phi(p, &phi, ATP_PHI_MAX_ARITY, 2);
  check_phi(p, &phi, ATP_PHI_VAR_NAMES, 3);
  check_phi(p, &phi, ATP_PHI_AX_SIZE_MAX, 10);
  check_phi(p, &phi, ATP_PHI_AX_SIZE_MEAN, (double)(float)(19.0 / 3.0));
  check_phi(p, &phi, ATP_PHI_AX_DEPTH_MAX, 3);
  check_phi(p, &phi, ATP_PHI_AX_VARS_MAX, 3);
  check_phi(p, &phi, ATP_PHI_AX_GROUND, 0);
  check_phi(p, &phi, ATP_PHI_GOAL_SIZE_MAX, 6);
  check_phi(p, &phi, ATP_PHI_GOAL_DEPTH_MAX, 2);
  check_phi(p, &phi, ATP_PHI_GOAL_VARS_MAX, 0);
  check_phi(p, &phi, ATP_PHI_CONJ_SYM_FRAC, (double)(3.0f / 5.0f));
  check_phi(p, &phi, ATP_PHI_RATING, (double)0.35f);
  check_phi(p, &phi, ATP_PHI_SPC_KNOWN, 1);
  check_phi(p, &phi, ATP_PHI_SPC_UNS, 1);
  check_phi(p, &phi, ATP_PHI_SPC_SAT, 0);
  check_phi(p, &phi, ATP_PHI_SPC_OPEN, 0);
  check_phi(p, &phi, ATP_PHI_SPC_UEQ, 1);
  // No dispatcher: no lane runs a Horn release, so every verdict is exact.
  check_lane(p, g, "wm", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "fof-fifo", TP_LANE_DEAD, 1, TP_GATE_FOF);
  check_lane(p, g, "equals", TP_LANE_DEAD, 1, TP_GATE_EQUALS);
  check_lane(p, g, "narrowq", TP_LANE_DEAD, 1, TP_GATE_NARROWQ);
  check_lane(p, g, "horn-cd", TP_LANE_DEAD, 1,
             "horn-cd slice: no encoded-Horn dispatcher");
  check_lane(p, g, "horn-back", TP_LANE_DEAD, 1,
             "horn-back slice: no encoded-Horn dispatcher");
  check_lane(p, g, "unfold", TP_LANE_DEAD, 1, TP_GATE_DEF);
  check_lane(p, g, "tgt-ax", TP_LANE_DEAD, 1, TP_GATE_TGT_AX);
  check_lane(p, g, "tgt-sel", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "otter-lpo", TP_LANE_DEAD, 1, PR_GATE_OTTER);
  check_lane(p, g, "ifeq", TP_LANE_DEAD, 1, TP_GATE_IFEQ);
  check_gates(p, g, "wm", "-");
  check_gates(p, g, "unfold", "def");
  check_gates(p, g, "horn-cd", "horn");
  check_gates(p, g, "otter-lpo", "otter");
  check_gates(p, g, "tgt-sel", "tgt-sel");
}

static void test_ring(TpLaneGate *g) {
  const char *p = "ring";
  AtpPhi phi;
  check(features(P_RING, &phi, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_AC_PAIRS, 1);
  check_phi(p, &phi, ATP_PHI_COMM_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_ASSOC_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_DISTRIB_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_DISTRIB_LAW, 1);
  check_phi(p, &phi, ATP_PHI_CONJ_CONE_PARTIAL, 0);
  check_phi(p, &phi, ATP_PHI_CONJ_SYM_FRAC, 1);
  check_phi(p, &phi, ATP_PHI_RATING, -1);
  check_phi(p, &phi, ATP_PHI_SPC_KNOWN, 0);
  // mult is rarer than add and later in the alphabet: every otter lane's
  // precedence puts it above add, so the law orients as expansion.
  check_lane(p, g, "otter-lpo", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "otter-lpo-cmin", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "otter-ladr", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "tgt-sel", TP_LANE_DEAD, 1, TP_GATE_TGT_SEL);
}

static void test_horn(TpLaneGate *g) {
  const char *p = "horn";
  AtpPhi phi;
  check(features(P_HORN, &phi, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_IFEQ_DISPATCHER, 1);
  check_phi(p, &phi, ATP_PHI_HORN_SLICE, 1);
  check_phi(p, &phi, ATP_PHI_AX_GROUND_COMPOUND, 1);
  check_phi(p, &phi, ATP_PHI_DEF_UNFOLD, 0);
  check_phi(p, &phi, ATP_PHI_GOAL_EXISTENTIAL, 0);
  check_phi(p, &phi, ATP_PHI_SYMS_AR4P, 1);
  check_phi(p, &phi, ATP_PHI_MAX_ARITY, 4);
  check_phi(p, &phi, ATP_PHI_AX_GROUND, 1);
  check_lane(p, g, "ifeq", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "horn-cd", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "horn-back", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "tgt-ax", TP_LANE_LIVE, 1, NULL);
  // Lanes with the Horn release on reach these gates after it: the
  // release may prove the problem first, so the verdict is undet; the
  // equals and narrowq gates read only the goals (strict), the def-unfold
  // gate reads the released units too.
  check_lane(p, g, "narrowq", TP_LANE_UNDET, 1, TP_GATE_NARROWQ);
  check_lane(p, g, "equals", TP_LANE_UNDET, 1, TP_GATE_EQUALS);
  check_lane(p, g, "unfold", TP_LANE_UNDET, 0, TP_GATE_DEF);
  check_lane(p, g, "wm", TP_LANE_LIVE, 1, NULL);
  // The same gates without the release are dead.
  check_lane(p, g, "equals-twee", TP_LANE_DEAD, 1, TP_GATE_EQUALS);
  check_lane(p, g, "unfold-tgt", TP_LANE_DEAD, 1, TP_GATE_DEF);
}

static void test_def(TpLaneGate *g) {
  const char *p = "def";
  AtpPhi phi;
  check(features(P_DEF, &phi, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_GOAL_EXISTENTIAL, 1);
  check_phi(p, &phi, ATP_PHI_EQUALS_GOAL, 1);
  check_phi(p, &phi, ATP_PHI_DEF_UNFOLD, 1);
  check_phi(p, &phi, ATP_PHI_AX_GROUND_COMPOUND, 0);
  check_phi(p, &phi, ATP_PHI_GOAL_VARS_MAX, 1);
  check_phi(p, &phi, ATP_PHI_AX_SIZE_MEAN, 4.5);
  check_lane(p, g, "narrowq", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "equals", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "equals-twee", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "unfold", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "unfold-tgt", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "unfold-tgt-awr", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "tgt-sel", TP_LANE_LIVE, 1, NULL);
}

static void test_nogoal(TpLaneGate *g) {
  const char *p = "nogoal";
  AtpPhi phi;
  check(features(P_NOGOAL, &phi, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 0);
  check_phi(p, &phi, ATP_PHI_AXIOMS, 1);
  check_phi(p, &phi, ATP_PHI_GOALS, 0);
  check_lane(p, g, "wm", TP_LANE_DEAD, 1, TP_GATE_NO_GOAL);
  check_lane(p, g, "fof-fifo", TP_LANE_DEAD, 1, TP_GATE_FOF);
  check_lane(p, g, "narrowq", TP_LANE_DEAD, 1, TP_GATE_NO_GOAL);
}

static void test_loader(TpLaneGate *g) {
  const char *p = "loader";
  AtpPhi phi;
  check(features(P_LPO, &phi, g) == 0, p, "features computed");
  check(g_tp_giveup_trap == NULL && g_pr_die_trap == NULL, p,
        "the give-up traps are released");
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 0);
  check_phi(p, &phi, ATP_PHI_AXIOMS, 1);
  check_phi(p, &phi, ATP_PHI_GOALS, 1);
  check_phi(p, &phi, ATP_PHI_SYMS, 2);
  check_phi(p, &phi, ATP_PHI_CONJ_CONE_PARTIAL, 0);
  check_lane(p, g, "wm", TP_LANE_DEAD, 1, "LPO ordering unsupported");
  check_lane(p, g, "otter-lpo", TP_LANE_DEAD, 1, "LPO ordering unsupported");
  // The gates before the reader still decide first.
  check_lane(p, g, "narrowq", TP_LANE_DEAD, 1, TP_GATE_NARROWQ);
  check_lane(p, g, "tgt-ax", TP_LANE_DEAD, 1, TP_GATE_TGT_AX);
}

// A --proof run turns on the conversion-lineage capture (g_prov_on) before
// its front end.  phi(p) and the verdicts must not depend on it (no
// feature may make the search depend on --proof, plan section 6.1), and
// the replay must not write the tables that run's proof cites.  P_DEF
// replays both def-unfold and the equals encoding, which capture.
static void test_proof_invariant(TpLaneGate *g) {
  const char *p = "proof";
  TpLaneGate *g_on = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *g_on);
  if (g_on == NULL) {
    check(0, p, "out of memory");
    return;
  }
  AtpPhi off, on;
  check(features(P_DEF, &off, g) == 0, p, "features computed");
  g_prov_on = 1;
  check(features(P_DEF, &on, g_on) == 0, p, "features computed under --proof");
  check(g_prov_on == 1, p, "the capture flag is restored");
  g_prov_on = 0;
  check(memcmp(off.v, on.v, sizeof off.v) == 0, p,
        "phi(p) is the same under --proof");
  int same = 1;
  for (int i = 0; i < N_SCHED; i++) {
    same = same && g[i].verdict == g_on[i].verdict &&
           g[i].strict == g_on[i].strict && strcmp(g[i].why, g_on[i].why) == 0;
  }
  check(same, p, "the lane verdicts are the same under --proof");
  check(g_prov_ndef == 0 && g_prov_nunf == 0 && g_prov_neqg == 0, p,
        "the replay records no conversion lineage");
  free(g_on);
}

int main(void) {
  TpLaneGate *g = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *g);
  if (g == NULL) {
    printf("FAIL: out of memory\n");
    return 1;
  }
  test_names();
  test_group(g);
  test_ring(g);
  test_horn(g);
  test_def(g);
  test_nogoal(g);
  test_loader(g);
  test_proof_invariant(g);
  free(g);
  printf("test_atp_problem_features: %d/%d checks passed\n",
         g_checks - g_fails, g_checks);
  return g_fails == 0 ? 0 : 1;
}
