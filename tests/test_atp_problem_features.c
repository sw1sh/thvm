// Problem features phi(p) and the static gate replica
// (tools/atp_cli/problem_features.c, lane_gates.c) on small inline and
// generated problems: the gate bits, sizes, goal shape, algebra and meta
// columns, the replica's verdict for the lanes each gate decides, the
// bench's .pr reader under its trap, the TSV row, and the isolation of the
// forked entry point (the caller's environment and state are untouched,
// phi(p) does not depend on --proof, the environment or the header).  Runs
// no search: the replica replays the front end only.
//
// It links the whole FindProof translation unit; FINDPROOF_LIB renames
// its main.
#define FINDPROOF_LIB 1
#include "../tools/thvm_atp_cli.c"

extern char **environ;

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

static void check_meta(const char *problem, const PhiMeta *meta, int col,
                       double want) {
  char what[200];
  double got = (double)meta->v[col];
  snprintf(what, sizeof what, "%s = %.9g, want %.9g", PHI_META_NAMES[col],
           got, want);
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

static int write_temp(const char *text, char *path, size_t cap) {
  const char *dir = getenv("TMPDIR");
  snprintf(path, cap, "%s/thvm_phi_test_XXXXXX",
           (dir != NULL && dir[0] != '\0') ? dir : "/tmp");
  int fd = mkstemp(path);
  if (fd < 0) {
    return -1;
  }
  size_t n = strlen(text);
  int ok = (write(fd, text, n) == (ssize_t)n);
  close(fd);
  if (!ok) {
    unlink(path);
    return -1;
  }
  return 0;
}

// The features of `text` through the forked entry point (local == 0) or
// in this process (local == 1).
static int features_via(const char *text, AtpPhi *phi, PhiMeta *meta,
                        TpLaneGate *g, int local) {
  char path[512];
  if (write_temp(text, path, sizeof path) != 0) {
    return -1;
  }
  int rc = local ? phi_compute(path, phi, meta, g)
                 : tp_problem_features(path, phi, meta, g);
  unlink(path);
  return rc;
}

static int features(const char *text, AtpPhi *phi, PhiMeta *meta,
                    TpLaneGate *g) {
  return features_via(text, phi, meta, g, 0);
}

static int env_count(void) {
  int n = 0;
  while (environ[n] != NULL) {
    n++;
  }
  return n;
}

// A group fragment with a ground goal and a TPTP header.
#define P_GROUP_HEADER \
  "% Rating   : 0.35 v9.0.0, 0.30 v8.2.0\n" \
  "% SPC      : CNF_UNS_RFO_PEQ_UEQ\n"
#define P_GROUP_BODY \
  "cnf(left_identity, axiom, mult(e,X) = X).\n" \
  "cnf(left_inverse, axiom, mult(inv(X),X) = e).\n" \
  "cnf(associativity, axiom, mult(mult(X,Y),Z) = mult(X,mult(Y,Z))).\n" \
  "cnf(goal, negated_conjecture, mult(a,b) != mult(b,a)).\n"
static const char *const P_GROUP = P_GROUP_HEADER P_GROUP_BODY;
static const char *const P_GROUP_BARE = P_GROUP_BODY;

// An AC operator, a distributivity law, and a goal over every symbol.
#define P_RING_BODY \
  "cnf(add_comm, axiom, add(X,Y) = add(Y,X)).\n" \
  "cnf(add_assoc, axiom, add(add(X,Y),Z) = add(X,add(Y,Z))).\n" \
  "cnf(distrib, axiom, mult(X,add(Y,Z)) = add(mult(X,Y),mult(X,Z))).\n" \
  "cnf(goal, negated_conjecture, mult(a,add(b,c)) != add(mult(a,c),mult(a,b))).\n"
static const char *const P_RING = P_RING_BODY;

// P_RING with mult AC too: mult now occurs 12 times (9 in the axioms) and
// add 10 (8).  Rarer-greater (const-frequency, LADR's fewer-occurrences
// rule) puts add above mult, so the law does not orient as expansion
// (otter-lpo, otter-ladr dead); const-min ties the two binary operators
// and breaks the tie by label, alphabetical in the .pr, mult above add
// (otter-lpo-cmin live).  Every symbol occurs in the goal, so --spb-prec's
// goal band changes nothing.
static const char *const P_RING_AC =
  P_RING_BODY
  "cnf(mult_comm, axiom, mult(X,Y) = mult(Y,X)).\n"
  "cnf(mult_assoc, axiom, mult(mult(X,Y),Z) = mult(X,mult(Y,Z))).\n";

// TPTP's -10 encoding: a dispatcher, one Horn rule, a ground fact.
#define P_HORN_AXIOMS \
  "cnf(ifeq_axiom, axiom, ifeq(A,A,B,C) = B).\n" \
  "cnf(rule_1, axiom, ifeq(p(X),true,q(X),true) = true).\n" \
  "cnf(fact_1, axiom, p(a) = true).\n"
static const char *const P_HORN =
  P_HORN_AXIOMS "cnf(goal, negated_conjecture, q(a) != true).\n";

// The same with an existential goal: the CD lanes give up on it.
static const char *const P_HORN_EX =
  P_HORN_AXIOMS "cnf(goal, negated_conjecture, q(X) != true).\n";

// A pure function definition and an existential goal.
static const char *const P_DEF =
  "cnf(def_f, axiom, f(X) = g(X,X)).\n"
  "cnf(ax_g, axiom, g(X,c) = X).\n"
  "cnf(goal, negated_conjecture, f(Y) != Y).\n";

// No negated conjecture: every lane gives up before searching, and the
// stated problem's bits are still computed (f(X) = X is a definition).
static const char *const P_NOGOAL =
  "cnf(a1, axiom, f(X) = X).\n";

// Spaces inside the terms: the canonical presentation drops them, and the
// bench's .pr reader, which takes a single space only at a term's start,
// rejects the raw text the --canon-input=0 lanes load.
static const char *const P_SPACE =
  "cnf(comm, axiom, f(X , Y) = f(Y , X)).\n"
  "cnf(goal, negated_conjecture, f(a , b) != f(b , a)).\n";

// A dispatcher problem with 120 variable names: a Horn release (at most
// HORN_REL_VARS more) could cross the engine's 128-variable capacity.
static char *gen_vars_problem(void) {
  size_t cap = 120 * 64 + 512;
  char *t = (char *)malloc(cap);
  if (t == NULL) {
    return NULL;
  }
  size_t o = (size_t)snprintf(t, cap, "%s", P_HORN_AXIOMS);
  for (int i = 0; i < 120 && o < cap; i++) {
    o += (size_t)snprintf(t + o, cap - o, "cnf(h%d, axiom, h(V%d) = V%d).\n",
                          i, i, i);
  }
  snprintf(t + o, cap - o, "cnf(goal, negated_conjecture, q(a) != true).\n");
  return t;
}

// 300 axioms fN(a) = a and a goal over f0 and f1: axiom selection keeps
// the goal, a0 and a1 (fN occurs once, a everywhere).
static char *gen_sine_problem(void) {
  size_t cap = 300 * 48 + 256;
  char *t = (char *)malloc(cap);
  if (t == NULL) {
    return NULL;
  }
  size_t o = 0;
  for (int i = 0; i < 300 && o < cap; i++) {
    o += (size_t)snprintf(t + o, cap - o, "cnf(a%d, axiom, f%d(a) = a).\n",
                          i, i);
  }
  snprintf(t + o, cap - o, "cnf(goal, negated_conjecture, f0(f1(a)) != a).\n");
  return t;
}

// A dispatcher problem past the CD lanes' 4000-clause bound.
static char *gen_horn_big_problem(void) {
  size_t cap = 4000 * 48 + 512;
  char *t = (char *)malloc(cap);
  if (t == NULL) {
    return NULL;
  }
  size_t o = (size_t)snprintf(t, cap, "%s", P_HORN_AXIOMS);
  for (int i = 0; i < 4000 && o < cap; i++) {
    o += (size_t)snprintf(t + o, cap - o, "cnf(k%d, axiom, p(c%d) = true).\n",
                          i, i);
  }
  snprintf(t + o, cap - o, "cnf(goal, negated_conjecture, q(c0) != true).\n");
  return t;
}

// The forked entry point leaves the caller's environment alone (the
// replay's thvm_init sets THVM_FUSE_CONV_BWD), and phi(p) is the same
// whether or not the replica is asked for.  Runs first, before anything
// in this process initializes the runtime.
static void test_isolation(TpLaneGate *g) {
  const char *p = "isolation";
  unsetenv("THVM_FUSE_CONV_BWD");
  int n0 = env_count();
  AtpPhi alone, with;
  PhiMeta m1, m2;
  check(features(P_HORN, &alone, &m1, NULL) == 0, p,
        "features computed without the replica");
  check(env_count() == n0, p, "the environment keeps its size");
  check(getenv("THVM_FUSE_CONV_BWD") == NULL, p,
        "THVM_FUSE_CONV_BWD stays unset");
  check(features(P_HORN, &with, &m2, g) == 0, p,
        "features computed with the replica");
  check(memcmp(alone.v, with.v, sizeof alone.v) == 0, p,
        "phi(p) is the same with and without the replica");
  check(memcmp(m1.v, m2.v, sizeof m1.v) == 0, p,
        "the meta columns are the same with and without the replica");
  check(env_count() == n0 && getenv("THVM_FUSE_CONV_BWD") == NULL, p,
        "the environment is unchanged after the replica");
}

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
  for (int i = 0; i < PHI_META_N; i++) {
    char what[80];
    snprintf(what, sizeof what, "meta column %d is named meta_*", i);
    check(PHI_META_NAMES[i] != NULL &&
          strncmp(PHI_META_NAMES[i], "meta_", 5) == 0, "names", what);
  }
}

static void test_group(TpLaneGate *g) {
  const char *p = "group";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_GROUP, &phi, &meta, g) == 0, p, "features computed");
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
  // Sizes are floor(log2(1 + n)).
  check_phi(p, &phi, ATP_PHI_AXIOMS, 2);          // 3
  check_phi(p, &phi, ATP_PHI_HYPOTHESES, 0);      // 0
  check_phi(p, &phi, ATP_PHI_GOALS, 1);           // 1
  check_phi(p, &phi, ATP_PHI_SYMS, 2);            // 5
  check_phi(p, &phi, ATP_PHI_SYMS_AR0, 2);        // 3
  check_phi(p, &phi, ATP_PHI_SYMS_AR1, 1);        // 1
  check_phi(p, &phi, ATP_PHI_SYMS_AR2, 1);        // 1
  check_phi(p, &phi, ATP_PHI_MAX_ARITY, 1);       // 2
  check_phi(p, &phi, ATP_PHI_VAR_NAMES, 2);       // 3
  check_phi(p, &phi, ATP_PHI_AX_SIZE_MAX, 3);     // 10
  check_phi(p, &phi, ATP_PHI_AX_SIZE_MEAN, 2);    // 19/3
  check_phi(p, &phi, ATP_PHI_AX_DEPTH_MAX, 2);    // 3
  check_phi(p, &phi, ATP_PHI_AX_VARS_MAX, 2);     // 3
  check_phi(p, &phi, ATP_PHI_AX_GROUND, 0);       // 0
  check_phi(p, &phi, ATP_PHI_GOAL_SIZE_MAX, 2);   // 6
  check_phi(p, &phi, ATP_PHI_GOAL_DEPTH_MAX, 1);  // 2
  check_phi(p, &phi, ATP_PHI_GOAL_VARS_MAX, 0);   // 0
  check_phi(p, &phi, ATP_PHI_CONJ_SYM_FRAC, (double)(3.0f / 5.0f));
  check_meta(p, &meta, PHI_META_PHI_VERSION, ATP_PHI_VERSION);
  check_meta(p, &meta, PHI_META_RATING, (double)0.35f);
  check_meta(p, &meta, PHI_META_SPC_KNOWN, 1);
  check_meta(p, &meta, PHI_META_SPC_UNS, 1);
  check_meta(p, &meta, PHI_META_SPC_SAT, 0);
  check_meta(p, &meta, PHI_META_SPC_OPEN, 0);
  check_meta(p, &meta, PHI_META_SPC_UEQ, 1);
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
  check_lane(p, g, "bare-raw", TP_LANE_LIVE, 1, NULL);
  check_gates(p, g, "wm", "-");
  check_gates(p, g, "unfold", "def");
  check_gates(p, g, "horn-cd", "horn");
  check_gates(p, g, "otter-lpo", "otter");
  check_gates(p, g, "tgt-sel", "tgt-sel");
}

// phi(p) reads nothing of the TPTP header: training files have one, the
// deployed problems do not.  Only the meta columns see it.
static void test_header_free(void) {
  const char *p = "header";
  AtpPhi with, without;
  PhiMeta m_with, m_without;
  check(features(P_GROUP, &with, &m_with, NULL) == 0, p,
        "features with the header");
  check(features(P_GROUP_BARE, &without, &m_without, NULL) == 0, p,
        "features without the header");
  check(memcmp(with.v, without.v, sizeof with.v) == 0, p,
        "phi(p) is the same without the header lines");
  check_meta(p, &m_without, PHI_META_RATING, -1);
  check_meta(p, &m_without, PHI_META_SPC_KNOWN, 0);
  check_meta(p, &m_without, PHI_META_PHI_VERSION, ATP_PHI_VERSION);
}

static void test_ring(TpLaneGate *g) {
  const char *p = "ring";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_RING, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_AC_PAIRS, 1);
  check_phi(p, &phi, ATP_PHI_COMM_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_ASSOC_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_DISTRIB_SYMS, 1);
  check_phi(p, &phi, ATP_PHI_DISTRIB_LAW, 1);
  check_phi(p, &phi, ATP_PHI_CONJ_CONE_PARTIAL, 0);
  check_phi(p, &phi, ATP_PHI_CONJ_SYM_FRAC, 1);
  check_meta(p, &meta, PHI_META_RATING, -1);
  check_meta(p, &meta, PHI_META_SPC_KNOWN, 0);
  // mult is rarer than add and later in the alphabet: every otter lane's
  // precedence puts it above add, so the law orients as expansion.
  check_lane(p, g, "otter-lpo", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "otter-lpo-cmin", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "otter-ladr", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "tgt-sel", TP_LANE_DEAD, 1, TP_GATE_TGT_SEL);
}

// The distributivity shape is there for every otter lane; only the
// orientation under each lane's precedence tells them apart.
static void test_ring_ac(TpLaneGate *g) {
  const char *p = "ring-ac";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_RING_AC, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_DISTRIB_LAW, 1);
  check_phi(p, &phi, ATP_PHI_AC_PAIRS, 2);
  check_phi(p, &phi, ATP_PHI_COMM_SYMS, 2);
  check_phi(p, &phi, ATP_PHI_ASSOC_SYMS, 2);
  check_lane(p, g, "otter-lpo", TP_LANE_DEAD, 1, PR_GATE_OTTER);
  check_lane(p, g, "otter-ladr", TP_LANE_DEAD, 1, PR_GATE_OTTER);
  check_lane(p, g, "otter-lpo-cmin", TP_LANE_LIVE, 1, NULL);
  // An empty THVM_ATP_DISTRIB is on for the bench (its test is "set and
  // not 0"), which rewrites the .pr before the otter gate.
  setenv("THVM_ATP_DISTRIB", "", 1);
  AtpPhi phi_env;
  check(features(P_RING_AC, &phi_env, &meta, g) == 0, p,
        "features computed with THVM_ATP_DISTRIB=\"\"");
  unsetenv("THVM_ATP_DISTRIB");
  check_lane(p, g, "otter-lpo", TP_LANE_UNDET, 0, NULL);
  check_lane(p, g, "otter-lpo-cmin", TP_LANE_UNDET, 0, NULL);
  check(memcmp(phi.v, phi_env.v, sizeof phi.v) == 0, p,
        "phi(p) does not read THVM_ATP_DISTRIB");
}

static void test_horn(TpLaneGate *g) {
  const char *p = "horn";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_HORN, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_IFEQ_DISPATCHER, 1);
  check_phi(p, &phi, ATP_PHI_HORN_SLICE, 1);
  check_phi(p, &phi, ATP_PHI_AX_GROUND_COMPOUND, 1);
  check_phi(p, &phi, ATP_PHI_DEF_UNFOLD, 0);
  check_phi(p, &phi, ATP_PHI_GOAL_EXISTENTIAL, 0);
  check_phi(p, &phi, ATP_PHI_SYMS_AR4P, 1);       // 1
  check_phi(p, &phi, ATP_PHI_MAX_ARITY, 2);       // 4
  check_phi(p, &phi, ATP_PHI_AX_GROUND, 1);       // 1
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

static void test_horn_existential(TpLaneGate *g) {
  const char *p = "horn-existential";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_HORN_EX, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_IFEQ_DISPATCHER, 1);
  check_phi(p, &phi, ATP_PHI_HORN_SLICE, 0);
  check_phi(p, &phi, ATP_PHI_GOAL_EXISTENTIAL, 1);
  check_lane(p, g, "horn-cd", TP_LANE_DEAD, 1,
             "horn-cd slice: existential goal (v2 lane covers it)");
  check_lane(p, g, "horn-back", TP_LANE_DEAD, 1,
             "horn-back slice: existential goal (v2 lane covers it)");
}

static void test_horn_oversized(TpLaneGate *g) {
  const char *p = "horn-oversized";
  char *text = gen_horn_big_problem();
  AtpPhi phi;
  PhiMeta meta;
  check(text != NULL && features(text, &phi, &meta, g) == 0, p,
        "features computed");
  free(text);
  check_phi(p, &phi, ATP_PHI_IFEQ_DISPATCHER, 1);
  check_phi(p, &phi, ATP_PHI_HORN_SLICE, 0);
  check_phi(p, &phi, ATP_PHI_SINE, 1);
  check_lane(p, g, "horn-cd", TP_LANE_DEAD, 1, "horn-cd slice: oversized KB");
  check_lane(p, g, "horn-back", TP_LANE_DEAD, 1,
             "horn-back slice: oversized KB");
}

// A release could cross the variable capacity, so a lane that runs it is
// undet even with every gate passing; the same lane without it is live.
static void test_release_not_neutral(TpLaneGate *g) {
  const char *p = "release";
  char *text = gen_vars_problem();
  AtpPhi phi;
  PhiMeta meta;
  check(text != NULL && features(text, &phi, &meta, g) == 0, p,
        "features computed");
  free(text);
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 1);
  check_lane(p, g, "wm", TP_LANE_UNDET, 0, NULL);
  check_lane(p, g, "bare", TP_LANE_LIVE, 1, NULL);
}

static void test_def(TpLaneGate *g) {
  const char *p = "def";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_DEF, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_GOAL_EXISTENTIAL, 1);
  check_phi(p, &phi, ATP_PHI_EQUALS_GOAL, 1);
  check_phi(p, &phi, ATP_PHI_DEF_UNFOLD, 1);
  check_phi(p, &phi, ATP_PHI_AX_GROUND_COMPOUND, 0);
  check_phi(p, &phi, ATP_PHI_GOAL_VARS_MAX, 1);   // 1
  check_phi(p, &phi, ATP_PHI_AX_SIZE_MEAN, 2);    // 4.5
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
  PhiMeta meta;
  check(features(P_NOGOAL, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 0);
  check_phi(p, &phi, ATP_PHI_AXIOMS, 1);          // 1
  check_phi(p, &phi, ATP_PHI_GOALS, 0);
  check_phi(p, &phi, ATP_PHI_DEF_UNFOLD, 1);
  check_lane(p, g, "wm", TP_LANE_DEAD, 1, TP_GATE_NO_GOAL);
  check_lane(p, g, "fof-fifo", TP_LANE_DEAD, 1, TP_GATE_FOF);
  check_lane(p, g, "narrowq", TP_LANE_DEAD, 1, TP_GATE_NO_GOAL);
  check_lane(p, g, "unfold", TP_LANE_DEAD, 1, TP_GATE_NO_GOAL);
}

// The --canon-input=0 lanes load the problem as written.
static void test_raw(TpLaneGate *g) {
  const char *p = "raw";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_SPACE, &phi, &meta, g) == 0, p, "features computed");
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 1);
  check_phi(p, &phi, ATP_PHI_COMM_SYMS, 1);
  check_lane(p, g, "bare", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "wm", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "bare-raw", TP_LANE_DEAD, 1,
             "pr parse error: expected , or ) in term");
  check_lane(p, g, "twee-raw", TP_LANE_DEAD, 1,
             "pr parse error: expected , or ) in term");
  check_lane(p, g, "flatten-bare-raw", TP_LANE_DEAD, 1,
             "pr parse error: expected , or ) in term");
}

// Axiom selection keeps phi(p)'s front end alive; THVM_ATP_SINE=0 in the
// environment moves the lanes (they read it) and not phi(p).
static void test_sine(TpLaneGate *g) {
  const char *p = "sine";
  char *text = gen_sine_problem();
  AtpPhi phi, phi_env;
  PhiMeta meta;
  check(text != NULL && features(text, &phi, &meta, g) == 0, p,
        "features computed");
  check_phi(p, &phi, ATP_PHI_SINE, 1);
  check_phi(p, &phi, ATP_PHI_FE_LIVE, 1);
  check_phi(p, &phi, ATP_PHI_AXIOMS, 8);          // 300
  check_phi(p, &phi, ATP_PHI_SYMS, 8);            // 301
  check_phi(p, &phi, ATP_PHI_CONJ_CONE_PARTIAL, 0);
  check_phi(p, &phi, ATP_PHI_CONJ_SYM_FRAC, 1);
  check_lane(p, g, "wm", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "bare", TP_LANE_LIVE, 1, NULL);
  check_lane(p, g, "tgt-sel", TP_LANE_DEAD, 1, TP_GATE_TGT_SEL);
  setenv("THVM_ATP_SINE", "0", 1);
  check(text != NULL && features(text, &phi_env, &meta, g) == 0, p,
        "features computed with THVM_ATP_SINE=0");
  unsetenv("THVM_ATP_SINE");
  free(text);
  check(memcmp(phi.v, phi_env.v, sizeof phi.v) == 0, p,
        "phi(p) does not read THVM_ATP_SINE");
  check_lane(p, g, "wm", TP_LANE_DEAD, 1, "problem exceeds equation capacity");
  check_lane(p, g, "bare", TP_LANE_DEAD, 1, "problem exceeds equation capacity");
}

// The TSV row: as many columns as the header, and every value reads back
// as the float it was printed from.
static void test_row(void) {
  const char *p = "row";
  AtpPhi phi;
  PhiMeta meta;
  check(features(P_GROUP, &phi, &meta, NULL) == 0, p, "features computed");
  char hdr[8192], row[8192];
  int hl = phi_format_header(hdr, sizeof hdr);
  int rl = phi_format_row(row, sizeof row, "group", &phi, &meta);
  check(hl > 0 && rl > 0, p, "header and row fit");
  if (hl <= 0 || rl <= 0) {
    return;
  }
  int want = 1 + ATP_PHI_N + PHI_META_N, hn = 1, rn = 1;
  for (const char *q = hdr; *q != '\0'; q++) {
    hn += (*q == '\t');
  }
  for (const char *q = row; *q != '\0'; q++) {
    rn += (*q == '\t');
  }
  check(hn == want, p, "the header has 1 + ATP_PHI_N + PHI_META_N columns");
  check(rn == want, p, "the row has 1 + ATP_PHI_N + PHI_META_N columns");
  check(strncmp(hdr, "problem\t", 8) == 0 && strncmp(row, "group\t", 6) == 0,
        p, "the first column is the problem");
  const char *q = strchr(row, '\t');
  int same = 1;
  for (int i = 0; i < ATP_PHI_N + PHI_META_N && q != NULL; i++) {
    char *end = NULL;
    float f = strtof(q + 1, &end);
    const float *src = (i < ATP_PHI_N) ? &phi.v[i] : &meta.v[i - ATP_PHI_N];
    same = same && end != q + 1 && memcmp(&f, src, sizeof f) == 0;
    q = strchr(q + 1, '\t');
  }
  check(same, p, "every value reads back as the same float");
}

// Calls A, B, A give A's numbers twice: no state of one problem reaches
// the next, through the forked entry point or in this process.
static void test_stable(int local) {
  const char *p = local ? "stable-local" : "stable";
  TpLaneGate *ga = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *ga);
  TpLaneGate *gb = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *gb);
  TpLaneGate *gc = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *gc);
  if (ga == NULL || gb == NULL || gc == NULL) {
    check(0, p, "out of memory");
    free(ga);
    free(gb);
    free(gc);
    return;
  }
  AtpPhi a1, b, a2;
  PhiMeta ma1, mb, ma2;
  check(features_via(P_DEF, &a1, &ma1, ga, local) == 0, p, "A computed");
  check(features_via(P_HORN, &b, &mb, gb, local) == 0, p, "B computed");
  check(features_via(P_DEF, &a2, &ma2, gc, local) == 0, p, "A again computed");
  check(memcmp(a1.v, a2.v, sizeof a1.v) == 0, p, "phi(A) is the same after B");
  int same = 1;
  for (int i = 0; i < N_SCHED; i++) {
    same = same && ga[i].verdict == gc[i].verdict &&
           ga[i].strict == gc[i].strict && strcmp(ga[i].why, gc[i].why) == 0;
  }
  check(same, p, "A's lane verdicts are the same after B");
  if (local) {
    check(g_tp_giveup_trap == NULL && g_pr_die_trap == NULL &&
          g_pr_walk_fp == NULL, p, "the give-up traps are released");
  }
  free(ga);
  free(gb);
  free(gc);
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
  AtpPhi off, on, on_local;
  PhiMeta meta;
  check(features(P_DEF, &off, &meta, g) == 0, p, "features computed");
  g_prov_on = 1;
  check(features(P_DEF, &on, &meta, g_on) == 0, p,
        "features computed under --proof");
  check(memcmp(off.v, on.v, sizeof off.v) == 0, p,
        "phi(p) is the same under --proof");
  int same = 1;
  for (int i = 0; i < N_SCHED; i++) {
    same = same && g[i].verdict == g_on[i].verdict &&
           g[i].strict == g_on[i].strict && strcmp(g[i].why, g_on[i].why) == 0;
  }
  check(same, p, "the lane verdicts are the same under --proof");
  check(features_via(P_DEF, &on_local, &meta, g_on, 1) == 0, p,
        "features computed in this process under --proof");
  check(g_prov_on == 1, p, "the capture flag is restored");
  g_prov_on = 0;
  check(memcmp(off.v, on_local.v, sizeof off.v) == 0, p,
        "phi(p) in this process is the same under --proof");
  check(g_prov_ndef == 0 && g_prov_nunf == 0 && g_prov_neqg == 0, p,
        "the replay records no conversion lineage");
  free(g_on);
}

// The lowest free descriptor: the same before and after a call that
// leaks none.
static int lowest_free_fd(void) {
  int fd = dup(2);
  if (fd >= 0) {
    close(fd);
  }
  return fd;
}

// The bench's .pr reader under the replica's trap: a parse error returns
// with its line and closes the file the walk had open; a file it cannot
// open is the replay's failure, not a verdict.
static void test_pr_reader(void) {
  const char *p = "pr-reader";
  static const char *const NO_CONCLUSION =
    "NAME          t\n"
    "MODE          PROOF\n"
    "SORTS         ANY\n"
    "SIGNATURE     a: -> ANY\n"
    "              f: ANY ANY -> ANY\n"
    "ORDERING      KBO\n"
    "              a=1, f=1\n"
    "              f > a\n"
    "VARIABLES     X, Y: ANY\n"
    "EQUATIONS     f(X,Y) = f(Y,X)\n";
  static const char *const BAD_TERM =
    "NAME          t\n"
    "MODE          PROOF\n"
    "SORTS         ANY\n"
    "SIGNATURE     a: -> ANY\n"
    "              f: ANY ANY -> ANY\n"
    "ORDERING      KBO\n"
    "              a=1, f=1\n"
    "              f > a\n"
    "VARIABLES     X, Y: ANY\n"
    "EQUATIONS     f(X , Y) = f(Y,X)\n"
    "CONCLUSION    f(a,a) = a\n";
  if (!g_phi_thvm_ready) {
    thvm_init();
    g_phi_thvm_ready = 1;
  }
  PrProblem *pr = (PrProblem *)malloc(sizeof *pr);
  char path[512];
  if (pr == NULL) {
    check(0, p, "out of memory");
    return;
  }
  check(write_temp(NO_CONCLUSION, path, sizeof path) == 0, p, "wrote a .pr");
  int r = phi_pr_read(path, pr);
  unlink(path);
  check(r == PHI_PR_DIED && strcmp(g_pr_die_trap_why, "no CONCLUSION") == 0,
        p, "a .pr without CONCLUSION gives up with its line");
  check(g_pr_die_trap == NULL && g_pr_walk_fp == NULL, p,
        "the trap is released");
  int fd0 = lowest_free_fd();
  check(write_temp(BAD_TERM, path, sizeof path) == 0, p, "wrote a .pr");
  r = phi_pr_read(path, pr);
  unlink(path);
  check(r == PHI_PR_DIED &&
        strcmp(g_pr_die_trap_why, "expected , or ) in term") == 0, p,
        "a bad term gives up inside the walk with its line");
  check(g_pr_walk_fp == NULL && lowest_free_fd() == fd0, p,
        "the walk's file is closed");
  r = phi_pr_read("/nonexistent/thvm_phi.pr", pr);
  check(r == PHI_PR_IO, p, "an unreadable .pr is an I/O failure");
  free(pr);
}

int main(void) {
  TpLaneGate *g = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *g);
  if (g == NULL) {
    printf("FAIL: out of memory\n");
    return 1;
  }
  test_isolation(g);
  test_names();
  test_group(g);
  test_header_free();
  test_ring(g);
  test_ring_ac(g);
  test_horn(g);
  test_horn_existential(g);
  test_horn_oversized(g);
  test_release_not_neutral(g);
  test_def(g);
  test_nogoal(g);
  test_raw(g);
  test_sine(g);
  test_row();
  test_stable(0);
  test_proof_invariant(g);
  test_stable(1);
  test_pr_reader();
  free(g);
  printf("test_atp_problem_features: %d/%d checks passed\n",
         g_checks - g_fails, g_checks);
  return g_fails == 0 ? 0 : 1;
}
