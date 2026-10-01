// ===================================================================
// Problem features phi(p)
// ===================================================================
//
// tp_problem_features computes phi(p) (AtpPhi, src/thvm.h) and, when asked,
// the static gate replica's verdict for every SCHEDULE lane (lane_gates.c),
// in a forked child that hands back only the numbers.  The replay inside
// calls thvm_init, which sets an environment variable every lane would
// inherit (and the environment's size moves the instruction work clock);
// it loads .pr copies into the term heap, which FindProof never collects;
// it writes a .pr under /tmp; and an input error exits.  None of that may
// reach a schedule parent.
//
// The front end is replayed stage by stage in main's order, once per
// distinct configuration: phi(p)'s own (var[0]: the defaults, whatever the
// environment holds) and, for the replica, each lane's.  A stage's give-up
// returns through g_tp_giveup_trap and the bench's .pr reader's through
// g_pr_die_trap, as the line a lane would print.  An allocation failure is
// the replay's own (it holds several copies of the problem, a lane one),
// so it fails the features instead.

static const char *const PHI_NAMES[ATP_PHI_N] = {
  [ATP_PHI_GOAL_EXISTENTIAL]   = "goal_existential",
  [ATP_PHI_EQUALS_GOAL]        = "equals_goal",
  [ATP_PHI_IFEQ_DISPATCHER]    = "ifeq_dispatcher",
  [ATP_PHI_HORN_SLICE]         = "horn_slice",
  [ATP_PHI_DEF_UNFOLD]         = "def_unfold",
  [ATP_PHI_AX_GROUND_COMPOUND] = "ax_ground_compound",
  [ATP_PHI_CONJ_CONE_PARTIAL]  = "conj_cone_partial",
  [ATP_PHI_DISTRIB_LAW]        = "distrib_law",
  [ATP_PHI_FE_LIVE]            = "fe_live",
  [ATP_PHI_SINE]               = "sine",
  [ATP_PHI_AC_PAIRS]           = "ac_pairs",
  [ATP_PHI_COMM_SYMS]          = "comm_syms",
  [ATP_PHI_ASSOC_SYMS]         = "assoc_syms",
  [ATP_PHI_IDEM_SYMS]          = "idem_syms",
  [ATP_PHI_UNIT_SYMS]          = "unit_syms",
  [ATP_PHI_INVERSE_SYMS]       = "inverse_syms",
  [ATP_PHI_DISTRIB_SYMS]       = "distrib_syms",
  [ATP_PHI_AXIOMS]             = "axioms",
  [ATP_PHI_HYPOTHESES]         = "hypotheses",
  [ATP_PHI_GOALS]              = "goals",
  [ATP_PHI_SYMS]               = "syms",
  [ATP_PHI_SYMS_AR0]           = "syms_ar0",
  [ATP_PHI_SYMS_AR1]           = "syms_ar1",
  [ATP_PHI_SYMS_AR2]           = "syms_ar2",
  [ATP_PHI_SYMS_AR3]           = "syms_ar3",
  [ATP_PHI_SYMS_AR4P]          = "syms_ar4p",
  [ATP_PHI_MAX_ARITY]          = "max_arity",
  [ATP_PHI_VAR_NAMES]          = "var_names",
  [ATP_PHI_AX_SIZE_MAX]        = "ax_size_max",
  [ATP_PHI_AX_SIZE_MEAN]       = "ax_size_mean",
  [ATP_PHI_AX_DEPTH_MAX]       = "ax_depth_max",
  [ATP_PHI_AX_DEPTH_MEAN]      = "ax_depth_mean",
  [ATP_PHI_AX_VARS_MAX]        = "ax_vars_max",
  [ATP_PHI_AX_VARS_MEAN]       = "ax_vars_mean",
  [ATP_PHI_AX_GROUND]          = "ax_ground",
  [ATP_PHI_GOAL_SIZE_MAX]      = "goal_size_max",
  [ATP_PHI_GOAL_SIZE_MEAN]     = "goal_size_mean",
  [ATP_PHI_GOAL_DEPTH_MAX]     = "goal_depth_max",
  [ATP_PHI_GOAL_VARS_MAX]      = "goal_vars_max",
  [ATP_PHI_CONJ_SYM_FRAC]      = "conj_sym_frac",
};

// Columns printed after phi(p) and never fed to the selection layer: the
// layout version, and the TPTP header fields that build and weight the
// training sets (plan section 3.2).  Deployed problems carry no header,
// and the SPC status is the label.
enum {
  PHI_META_PHI_VERSION,
  PHI_META_RATING,
  PHI_META_SPC_KNOWN,
  PHI_META_SPC_UNS,
  PHI_META_SPC_SAT,
  PHI_META_SPC_OPEN,
  PHI_META_SPC_UEQ,
  PHI_META_N
};
typedef struct { float v[PHI_META_N]; } PhiMeta;

static const char *const PHI_META_NAMES[PHI_META_N] = {
  [PHI_META_PHI_VERSION] = "meta_phi_version",
  [PHI_META_RATING]      = "meta_rating",
  [PHI_META_SPC_KNOWN]   = "meta_spc_known",
  [PHI_META_SPC_UNS]     = "meta_spc_uns",
  [PHI_META_SPC_SAT]     = "meta_spc_sat",
  [PHI_META_SPC_OPEN]    = "meta_spc_open",
  [PHI_META_SPC_UEQ]     = "meta_spc_ueq",
};

#define PHI_WHY_LEN 256

enum { TP_LANE_LIVE = 0, TP_LANE_DEAD, TP_LANE_UNDET };

// One lane's replica verdict.  strict: the predicted line (or none, for a
// live lane) is exactly what a run of the lane without --szs prints.
typedef struct {
  u8   verdict;
  u8   strict;
  char gates[64];
  char why[PHI_WHY_LEN];
} TpLaneGate;

static const char *phi_verdict_name(int verdict) {
  switch (verdict) {
    case TP_LANE_LIVE:  return "live";
    case TP_LANE_DEAD:  return "dead";
    default:            return "undet";
  }
}

// --- the replayed front end --------------------------------------------

typedef struct {
  u8 def, eq, sine, canon;
  u8 sine_env;   // axiom selection under the environment's tolerances
} PhiCfg;

typedef struct {
  PhiCfg     cfg;
  u8         eq_applied;
  u8         ax_ground;
  u8         reached_sel;          // past the capacity gates and canon
  int        n_hit, n_syms;
  int        n_pos_fe;             // positive units before axiom selection
  char       why[PHI_WHY_LEN];     // def-unfold .. the capacity gates
  char       canon_why[PHI_WHY_LEN];
  char       pr_why[PHI_WHY_LEN];  // the bench's .pr reader
  PrProblem *prp;
} PhiVariant;

#define PHI_MAX_VARIANTS 16

typedef struct {
  int        parsed;
  char       parse_why[PHI_WHY_LEN];
  int        no_goal;
  int        goal_existential;
  int        ifeq;
  int        curried;              // currying rewrote a wide functor
  char       curry_why[PHI_WHY_LEN];
  int        bits;                 // the stated problem's gate bits exist
  int        horn_gate, n_disp, def_bit, eq_bit, ax_ground;
  int        n_syms, n_vars, n_pos, n_neg, n_hyp;
  char       fail[PHI_WHY_LEN];
  PhiVariant var[PHI_MAX_VARIANTS];
  int        n_var;
} PhiCtx;

// The replica (lane_gates.c, included after this file).
static void tp_lane_configs(PhiCtx *c);
static void tp_lane_gates(PhiCtx *c, TpLaneGate *lanes);

static int g_phi_thvm_ready = 0;

typedef void (*PhiStage)(TpProblem *tp, void *arg);

// 0 when the stage returns, 1 when it gives up (g_tp_giveup_trap_why).
static int phi_run(PhiStage stage, TpProblem *tp, void *arg) {
  jmp_buf jb;
  g_tp_giveup_trap = &jb;
  if (setjmp(jb) != 0) {
    g_tp_giveup_trap = NULL;
    return 1;
  }
  stage(tp, arg);
  g_tp_giveup_trap = NULL;
  return 0;
}

static void phi_trapped(PhiCtx *c, char *line) {
  if (strncmp(g_tp_giveup_trap_why, "OOM", 3) == 0) {
    snprintf(c->fail, sizeof c->fail, "%s", g_tp_giveup_trap_why);
  } else {
    snprintf(line, PHI_WHY_LEN, "%s", g_tp_giveup_trap_why);
  }
}

static int phi_count_neg(const TpProblem *tp) {
  int n = 0;
  for (int i = 0; i < tp->n_cls; i++) {
    n += tp->cls[i].neg;
  }
  return n;
}

static void phi_stage_unquote(TpProblem *tp, void *arg) {
  (void)tp;
  char **pre = (char **)arg;
  *pre = tp_unquote_names(*pre);
  tp_shorten_names(*pre);
}

static void phi_stage_parse(TpProblem *tp, void *arg) {
  tp_parse(tp, (const char *)arg);
}

static void phi_stage_ifeq(TpProblem *tp, void *arg) {
  ((PhiCtx *)arg)->ifeq = tp_has_ifeq_dispatcher(tp);
}

static void phi_stage_curry(TpProblem *tp, void *arg) {
  (void)arg;
  tp_curry_wide(tp);
}

static void phi_stage_bits(TpProblem *tp, void *arg) {
  PhiCtx *c = (PhiCtx *)arg;
  char eqn[TP_NAME_LEN], tru[TP_NAME_LEN], fls[TP_NAME_LEN];
  int n_nonground = 0;
  c->n_disp = tp_horn_probe(tp, &c->horn_gate);
  c->def_bit = tp_def_unfold_applies(tp);
  c->eq_bit = tp_equals_goal_applies(tp, eqn, tru, fls, &n_nonground);
  c->ax_ground = tp_has_ax_ground_compound(tp);
}

// Axiom selection under the environment's tolerances.  The replay never
// writes THVM_ATP_SINE_DUMP's files: they belong to a real run.
static TpSineCfg phi_sine_cfg_env(void) {
  TpSineCfg cfg = tp_sine_cfg_env();
  cfg.dump = NULL;
  return cfg;
}

static void phi_stage_front(TpProblem *tp, void *arg) {
  PhiVariant *v = (PhiVariant *)arg;
  if (v->cfg.def) {
    tp_def_unfold(tp);
  }
  v->ax_ground = (u8)tp_has_ax_ground_compound(tp);
  if (v->cfg.eq) {
    tp_equals_goal(tp);
    v->eq_applied = (u8)g_cli_equals_applied;
  }
  int n_neg = phi_count_neg(tp);
  v->n_pos_fe = tp->n_cls - n_neg;
  if (v->cfg.sine && tp->n_cls - n_neg > (int)PR_MAX_EQNS) {
    if (v->cfg.sine_env) {
      TpSineCfg env = phi_sine_cfg_env();
      tp_sine_select_cfg(tp, &env);
    } else {
      tp_sine_select_cfg(tp, &TP_SINE_DEFAULT);
    }
    n_neg = phi_count_neg(tp);
  }
  tp_check_capacity(tp, n_neg);
}

static void phi_stage_canon(TpProblem *tp, void *arg) {
  (void)arg;
  tp_canon_input(tp);
}

enum { PHI_PR_OK, PHI_PR_DIED, PHI_PR_IO };

// The bench's .pr reader under its trap.  A file the reader cannot open is
// this process's failure, not the lane's verdict.
static int phi_pr_read(const char *path, PrProblem *p) {
  jmp_buf jb;
  g_pr_die_trap = &jb;
  if (setjmp(jb) != 0) {
    g_pr_die_trap = NULL;
    if (g_pr_walk_fp != NULL) {
      fclose(g_pr_walk_fp);
      g_pr_walk_fp = NULL;
    }
    return (strcmp(g_pr_die_trap_why, PR_DIE_OPEN) == 0) ? PHI_PR_IO
                                                          : PHI_PR_DIED;
  }
  pr_load_header(path, p);
  pr_load_terms(p);
  g_pr_die_trap = NULL;
  return PHI_PR_OK;
}

static void phi_stage_emit(TpProblem *tp, void *arg) {
  *(char **)arg = tp_emit_pr(tp, "phi");
}

static void phi_load_pr(PhiCtx *c, TpProblem *tp, PhiVariant *v) {
  char path[64];
  snprintf(path, sizeof path, "/tmp/thvm_phi_%d.pr", (int)getpid());
  char *text = NULL;
  if (phi_run(phi_stage_emit, tp, &text)) {
    snprintf(c->fail, sizeof c->fail, "%s", g_tp_giveup_trap_why);
    return;
  }
  FILE *fp = fopen(path, "w");
  int ok = (fp != NULL);
  if (ok) {
    ok = (fputs(text, fp) >= 0);
    ok = (fclose(fp) == 0) && ok;
  }
  free(text);
  PrProblem *p = ok ? (PrProblem *)malloc(sizeof *p) : NULL;
  if (p == NULL) {
    unlink(path);
    snprintf(c->fail, sizeof c->fail, "cannot write the replayed .pr to /tmp");
    return;
  }
  if (!g_phi_thvm_ready) {
    thvm_init();
    g_phi_thvm_ready = 1;
  }
  int r = phi_pr_read(path, p);
  unlink(path);
  if (r == PHI_PR_OK) {
    v->prp = p;
    return;
  }
  free(p);
  if (r == PHI_PR_IO) {
    snprintf(c->fail, sizeof c->fail, "cannot read the replayed .pr back");
  } else {
    snprintf(v->pr_why, sizeof v->pr_why, "pr parse error: %.200s",
             g_pr_die_trap_why);
  }
}

static void phi_variant_run(PhiCtx *c, TpProblem *tp, PhiVariant *v) {
  v->n_pos_fe = c->n_pos;
  g_cli_defunfold_n = 0;
  g_cli_equals_applied = 0;
  if (phi_run(phi_stage_front, tp, v)) {
    phi_trapped(c, v->why);
    return;
  }
  if (v->cfg.canon && phi_run(phi_stage_canon, tp, NULL)) {
    phi_trapped(c, v->canon_why);
    return;
  }
  v->reached_sel = 1;
  v->n_hit = tp_conj_syms(tp, NULL);
  v->n_syms = tp->n_syms;
  phi_load_pr(c, tp, v);
}

static PhiVariant *phi_variant(PhiCtx *c, const PhiCfg *cfg, int add) {
  for (int i = 0; i < c->n_var; i++) {
    const PhiCfg *h = &c->var[i].cfg;
    if (h->def == cfg->def && h->eq == cfg->eq && h->sine == cfg->sine &&
        h->canon == cfg->canon && h->sine_env == cfg->sine_env) {
      return &c->var[i];
    }
  }
  if (!add || c->n_var >= PHI_MAX_VARIANTS) {
    return NULL;
  }
  PhiVariant *v = &c->var[c->n_var++];
  memset(v, 0, sizeof *v);
  v->cfg = *cfg;
  return v;
}

static TpProblem *phi_clone(const TpProblem *src) {
  TpProblem *w = (TpProblem *)calloc(1, sizeof *w);
  if (w == NULL) {
    return NULL;
  }
  memcpy(w->cls, src->cls, (size_t)src->n_cls * sizeof(TpClause));
  w->n_cls = src->n_cls;
  memcpy(w->syms, src->syms, (size_t)src->n_syms * sizeof(TpSym));
  w->n_syms = src->n_syms;
  memcpy(w->vars, src->vars, (size_t)src->n_vars * TP_NAME_LEN);
  w->n_vars = src->n_vars;
  return w;
}

// A stage replayed on a clone rebuilt the symbol hash for the clone.
static void phi_sym_hash_rebuild(const TpProblem *tp) {
  for (int i = 0; i < TP_SYM_HASH_SLOTS; i++) {
    g_tp_sym_hash[i] = -1;
  }
  g_tp_sym_hash_init = 1;
  for (int i = 0; i < tp->n_syms; i++) {
    const char *nm = tp->syms[i].name;
    u32 slot = (u32)(tp_name_hash(nm, strlen(nm)) % (u64)TP_SYM_HASH_SLOTS);
    while (g_tp_sym_hash[slot] >= 0) {
      slot = (slot + 1u) % (u32)TP_SYM_HASH_SLOTS;
    }
    g_tp_sym_hash[slot] = i;
  }
}

// var[0] runs last, on tp itself.
static void phi_variants(PhiCtx *c, TpProblem *tp, int lanes) {
  static const PhiCfg dflt = { 0, 0, 1, 1, 0 };
  phi_variant(c, &dflt, 1);
  if (lanes) {
    tp_lane_configs(c);
  }
  for (int i = 1; i < c->n_var && c->fail[0] == '\0'; i++) {
    TpProblem *w = phi_clone(tp);
    if (w == NULL) {
      snprintf(c->fail, sizeof c->fail, "OOM in problem features");
      return;
    }
    phi_variant_run(c, w, &c->var[i]);
    free(w);
    phi_sym_hash_rebuild(tp);
  }
  if (c->fail[0] == '\0') {
    phi_variant_run(c, tp, &c->var[0]);
  }
}

// main's front end over the parsed problem: the dispatcher probe, currying,
// then the stated problem's gate bits, whose predicates print no lane's
// line when they give up; then the replayed configurations.  The bits are
// computed on a problem without a goal too, which every lane gives up on
// before reading them.
static void phi_front(PhiCtx *c, TpProblem *tp, int lanes) {
  if (phi_run(phi_stage_ifeq, tp, c)) {
    snprintf(c->fail, sizeof c->fail, "%s", g_tp_giveup_trap_why);
    return;
  }
  for (int i = 0; i < tp->n_syms; i++) {
    if (tp->syms[i].arity > CURRY_ARITY_CAP) {
      c->curried = 1;
    }
  }
  if (phi_run(phi_stage_curry, tp, NULL)) {
    phi_trapped(c, c->curry_why);
    return;
  }
  if (phi_run(phi_stage_bits, tp, c)) {
    snprintf(c->fail, sizeof c->fail, "%s", g_tp_giveup_trap_why);
    return;
  }
  c->bits = 1;
  c->n_syms = tp->n_syms;
  c->n_vars = tp->n_vars;
  c->n_neg = phi_count_neg(tp);
  c->n_pos = tp->n_cls - c->n_neg;
  for (int i = 0; i < tp->n_cls; i++) {
    c->n_hyp += (!tp->cls[i].neg && tp->cls[i].conj);
  }
  if (!c->no_goal) {
    phi_variants(c, tp, lanes);
  }
}

// --- phi(p) ----------------------------------------------------------------

// floor(log2(1 + x)) for x >= 0, on the integer part: floor(log2(y)) ==
// floor(log2(floor(y))) for y >= 1, and the integer form has no rounding
// at the powers of two.
static float phi_log2_bucket(double x) {
  double y = 1.0 + x;
  u64 n = (y < 1.8e19) ? (u64)y : UINT64_MAX;
  int b = 0;
  while (n > 1u) {
    n >>= 1;
    b++;
  }
  return (float)b;
}

static int phi_side_depth(const char *s) {
  int d = 0, best = 0;
  for (; *s != '\0'; s++) {
    if (*s == '(') {
      d++;
      if (d > best) {
        best = d;
      }
    } else if (*s == ')') {
      d--;
    }
  }
  return best + 1;
}

// Distinct variables of a clause, by tp_scan_term's rule (an
// uppercase-initial token that is no functor); at most 64 are told apart.
static int phi_clause_vars(const TpClause *cl) {
  char seen[64][TP_NAME_LEN];
  int n = 0;
  for (int side = 0; side < 2; side++) {
    const char *p = side ? cl->rhs : cl->lhs;
    while (*p != '\0') {
      if (!(isalnum((unsigned char)*p) || *p == '_')) {
        p++;
        continue;
      }
      size_t len = 0;
      while (isalnum((unsigned char)p[len]) || p[len] == '_') {
        len++;
      }
      const char *q = p + len;
      while (isspace((unsigned char)*q)) {
        q++;
      }
      if (isupper((unsigned char)*p) && *q != '(' && len < TP_NAME_LEN) {
        int dup = 0;
        for (int i = 0; i < n && !dup; i++) {
          dup = (strlen(seen[i]) == len && memcmp(seen[i], p, len) == 0);
        }
        if (!dup && n < 64) {
          memcpy(seen[n], p, len);
          seen[n][len] = '\0';
          n++;
        }
      }
      p += len;
    }
  }
  return n;
}

static void phi_stated(const TpProblem *tp, AtpPhi *phi) {
  float *v = phi->v;
  int n_ax = 0, n_hyp = 0, n_goal = 0, n_ground = 0;
  int ax_size_max = 0, ax_depth_max = 0, ax_vars_max = 0;
  int goal_size_max = 0, goal_depth_max = 0, goal_vars_max = 0;
  double ax_size = 0.0, ax_depth = 0.0, ax_vars = 0.0, goal_size = 0.0;
  for (int c = 0; c < tp->n_cls; c++) {
    const TpClause *cl = &tp->cls[c];
    int size = tp_side_weight(cl->lhs) + tp_side_weight(cl->rhs);
    int dl = phi_side_depth(cl->lhs), dr = phi_side_depth(cl->rhs);
    int depth = (dl > dr) ? dl : dr;
    int vars = phi_clause_vars(cl);
    if (cl->neg) {
      n_goal++;
      goal_size += size;
      goal_size_max  = (size > goal_size_max) ? size : goal_size_max;
      goal_depth_max = (depth > goal_depth_max) ? depth : goal_depth_max;
      goal_vars_max  = (vars > goal_vars_max) ? vars : goal_vars_max;
    } else if (cl->conj) {
      n_hyp++;
    } else {
      n_ax++;
      ax_size += size;
      ax_depth += depth;
      ax_vars += vars;
      ax_size_max  = (size > ax_size_max) ? size : ax_size_max;
      ax_depth_max = (depth > ax_depth_max) ? depth : ax_depth_max;
      ax_vars_max  = (vars > ax_vars_max) ? vars : ax_vars_max;
      n_ground += (vars == 0);
    }
  }
  int by_arity[5] = { 0, 0, 0, 0, 0 };
  int max_arity = 0;
  for (int i = 0; i < tp->n_syms; i++) {
    int ar = tp->syms[i].arity;
    by_arity[(ar >= 4) ? 4 : ar]++;
    max_arity = (ar > max_arity) ? ar : max_arity;
  }
  v[ATP_PHI_AXIOMS]         = phi_log2_bucket(n_ax);
  v[ATP_PHI_HYPOTHESES]     = phi_log2_bucket(n_hyp);
  v[ATP_PHI_GOALS]          = phi_log2_bucket(n_goal);
  v[ATP_PHI_SYMS]           = phi_log2_bucket(tp->n_syms);
  v[ATP_PHI_SYMS_AR0]       = phi_log2_bucket(by_arity[0]);
  v[ATP_PHI_SYMS_AR1]       = phi_log2_bucket(by_arity[1]);
  v[ATP_PHI_SYMS_AR2]       = phi_log2_bucket(by_arity[2]);
  v[ATP_PHI_SYMS_AR3]       = phi_log2_bucket(by_arity[3]);
  v[ATP_PHI_SYMS_AR4P]      = phi_log2_bucket(by_arity[4]);
  v[ATP_PHI_MAX_ARITY]      = phi_log2_bucket(max_arity);
  v[ATP_PHI_VAR_NAMES]      = phi_log2_bucket(tp->n_vars);
  v[ATP_PHI_AX_SIZE_MAX]    = phi_log2_bucket(ax_size_max);
  v[ATP_PHI_AX_SIZE_MEAN]   = phi_log2_bucket((n_ax > 0) ? ax_size / n_ax : 0.0);
  v[ATP_PHI_AX_DEPTH_MAX]   = phi_log2_bucket(ax_depth_max);
  v[ATP_PHI_AX_DEPTH_MEAN]  = phi_log2_bucket((n_ax > 0) ? ax_depth / n_ax : 0.0);
  v[ATP_PHI_AX_VARS_MAX]    = phi_log2_bucket(ax_vars_max);
  v[ATP_PHI_AX_VARS_MEAN]   = phi_log2_bucket((n_ax > 0) ? ax_vars / n_ax : 0.0);
  v[ATP_PHI_AX_GROUND]      = phi_log2_bucket(n_ground);
  v[ATP_PHI_GOAL_SIZE_MAX]  = phi_log2_bucket(goal_size_max);
  v[ATP_PHI_GOAL_SIZE_MEAN] = phi_log2_bucket((n_goal > 0) ? goal_size / n_goal : 0.0);
  v[ATP_PHI_GOAL_DEPTH_MAX] = phi_log2_bucket(goal_depth_max);
  v[ATP_PHI_GOAL_VARS_MAX]  = phi_log2_bucket(goal_vars_max);
}

// A TPTP header field ("% SPC      : ..." / "% Rating   : ...") of the raw
// text, copied into buf; 0 when the file has none.
static int phi_header_field(const char *raw, const char *key, char *buf,
                            size_t cap) {
  size_t kl = strlen(key);
  for (const char *line = raw; *line != '\0'; ) {
    const char *eol = strchr(line, '\n');
    const char *end = (eol != NULL) ? eol : line + strlen(line);
    const char *p = line;
    if (*p == '%') {
      p++;
      while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
      }
      if ((size_t)(end - p) > kl && strncmp(p, key, kl) == 0) {
        const char *q = p + kl;
        while (q < end && (*q == ' ' || *q == '\t')) {
          q++;
        }
        if (q < end && *q == ':') {
          q++;
          while (q < end && (*q == ' ' || *q == '\t')) {
            q++;
          }
          const char *e = end;
          while (e > q && isspace((unsigned char)e[-1])) {
            e--;
          }
          size_t vl = (size_t)(e - q);
          if (vl >= cap) {
            vl = cap - 1;
          }
          memcpy(buf, q, vl);
          buf[vl] = '\0';
          return 1;
        }
      }
    }
    if (eol == NULL) {
      break;
    }
    line = eol + 1;
  }
  return 0;
}

static void phi_header(const char *raw, PhiMeta *meta) {
  char buf[256];
  if (phi_header_field(raw, "Rating", buf, sizeof buf)) {
    char *e = NULL;
    double r = strtod(buf, &e);
    if (e != buf) {
      meta->v[PHI_META_RATING] = (float)r;
    }
  }
  if (phi_header_field(raw, "SPC", buf, sizeof buf)) {
    meta->v[PHI_META_SPC_KNOWN] = 1.0f;
    int field = 0;
    for (char *tok = buf; *tok != '\0'; field++) {
      size_t tl = strcspn(tok, "_ \t");
      if (field == 1) {
        meta->v[PHI_META_SPC_UNS]  = (tl == 3 && strncmp(tok, "UNS", 3) == 0);
        meta->v[PHI_META_SPC_SAT]  = (tl == 3 && strncmp(tok, "SAT", 3) == 0);
        meta->v[PHI_META_SPC_OPEN] = (tl == 3 && (strncmp(tok, "UNK", 3) == 0 ||
                                                  strncmp(tok, "OPN", 3) == 0));
      }
      if (tl == 3 && strncmp(tok, "UEQ", 3) == 0) {
        meta->v[PHI_META_SPC_UEQ] = 1.0f;
      }
      tok += tl;
      if (*tok != '_') {
        break;
      }
      tok++;
    }
  }
}

// Waldmeister's structure detection (atp_analyze_axioms) over the .pr the
// engine loads.
static void phi_algebra(const PrProblem *p, AtpPhi *phi) {
  u32 cap = p->n_syms + PR_LABEL_BASE;
  AtpSymProps *props = (AtpSymProps *)calloc((size_t)cap, sizeof(AtpSymProps));
  if (props == NULL) {
    return;
  }
  atp_analyze_axioms(p->eq_l, p->eq_r, p->n_eqns, props, cap);
  for (u32 l = 0; l < cap; l++) {
    const AtpSymProps *s = &props[l];
    phi->v[ATP_PHI_AC_PAIRS]     += (float)(s->is_commutative && s->is_associative);
    phi->v[ATP_PHI_COMM_SYMS]    += (float)(s->is_commutative != 0);
    phi->v[ATP_PHI_ASSOC_SYMS]   += (float)(s->is_associative != 0);
    phi->v[ATP_PHI_IDEM_SYMS]    += (float)(s->is_idempotent != 0);
    phi->v[ATP_PHI_UNIT_SYMS]    += (float)(s->has_left_unit || s->has_right_unit);
    phi->v[ATP_PHI_INVERSE_SYMS] += (float)(s->has_inverse != 0);
    phi->v[ATP_PHI_DISTRIB_SYMS] += (float)(s->distributes != 0);
  }
  free(props);
  for (u32 e = 0; e < p->n_eqns; e++) {
    u32 f = 0, g = 0;
    if (pr_distrib_shape(p->eq_l[e], p->eq_r[e], &f, &g) ||
        pr_distrib_shape(p->eq_r[e], p->eq_l[e], &f, &g)) {
      phi->v[ATP_PHI_DISTRIB_LAW] = 1.0f;
      break;
    }
  }
}

static void phi_fill(const PhiCtx *c, AtpPhi *phi) {
  float *v = phi->v;
  v[ATP_PHI_GOAL_EXISTENTIAL] = (float)c->goal_existential;
  v[ATP_PHI_IFEQ_DISPATCHER]  = (float)c->ifeq;
  if (c->bits) {
    v[ATP_PHI_EQUALS_GOAL]        = (float)c->eq_bit;
    v[ATP_PHI_HORN_SLICE]         = (float)(c->horn_gate == HORN_GATE_LIVE);
    v[ATP_PHI_DEF_UNFOLD]         = (float)c->def_bit;
    v[ATP_PHI_AX_GROUND_COMPOUND] = (float)c->ax_ground;
    v[ATP_PHI_SINE]               = (float)(c->n_pos > (int)PR_MAX_EQNS);
  }
  const PhiVariant *base = (c->n_var > 0) ? &c->var[0] : NULL;
  if (base == NULL || !base->reached_sel) {
    return;
  }
  v[ATP_PHI_CONJ_CONE_PARTIAL] =
      (float)tp_conj_cone_partial(base->n_hit, base->n_syms);
  v[ATP_PHI_CONJ_SYM_FRAC] =
      (base->n_syms > 0) ? (float)base->n_hit / (float)base->n_syms : 0.0f;
  if (base->prp != NULL) {
    v[ATP_PHI_FE_LIVE] = 1.0f;
    phi_algebra(base->prp, phi);
  }
}

// The replayed stages narrate on stderr.
static int phi_quiet_begin(void) {
  if (g_cli_verbose) {
    return -1;
  }
  fflush(stderr);
  int saved = dup(2);
  if (saved < 0) {
    return -1;
  }
  int dn = open("/dev/null", O_WRONLY);
  if (dn >= 0) {
    dup2(dn, 2);
    close(dn);
  }
  return saved;
}

static void phi_quiet_end(int saved) {
  if (saved < 0) {
    return;
  }
  fflush(stderr);
  dup2(saved, 2);
  close(saved);
}

// phi(p) and, when lanes is not NULL, the replica (lanes[N_SCHED]), in this
// process: tp_problem_features runs it in a child.  Returns 0, or 2 when the
// file cannot be read, is not unit-equality CNF (FOF and non-unit CNF go
// through the clausal front end, which is not replayed) or the replay
// fails.  The front-end state it writes (the symbol hash, the curry table,
// the transform flags, the rename recording) is reset on return.
static int phi_compute(const char *file, AtpPhi *phi, PhiMeta *meta,
                       TpLaneGate *lanes) {
  memset(phi, 0, sizeof *phi);
  memset(meta, 0, sizeof *meta);
  meta->v[PHI_META_PHI_VERSION] = (float)ATP_PHI_VERSION;
  meta->v[PHI_META_RATING] = -1.0f;
  size_t rl = 0;
  char *raw = tp_read_file(file, &rl);
  if (raw == NULL) {
    fprintf(stderr, "FindProof: cannot open %s\n", file);
    return 2;
  }
  if (!tp_raw_marker(raw, "cnf(") && !tp_raw_marker(raw, "fof(") &&
      !tp_raw_marker(raw, "include(")) {
    fprintf(stderr, "FindProof: problem features: %s is not TPTP input\n", file);
    free(raw);
    return 2;
  }
  if (tp_input_is_fof(raw)) {
    fprintf(stderr, "FindProof: problem features: %s needs the clausal front "
            "end (FOF or non-unit CNF); they cover unit-equality CNF\n", file);
    free(raw);
    return 2;
  }
  phi_header(raw, meta);
  PhiCtx *c = (PhiCtx *)calloc(1, sizeof *c);
  TpProblem *tp = (TpProblem *)calloc(1, sizeof *tp);
  if (c == NULL || tp == NULL) {
    fprintf(stderr, "FindProof: OOM\n");
    free(c);
    free(tp);
    free(raw);
    return 2;
  }
  c->horn_gate = -1;
  char *pre = tp_preprocess(file, raw);
  free(raw);
  int saved_goal_ex = g_cli_goal_existential;
  int saved_equals  = g_cli_equals_applied;
  int saved_defun   = g_cli_defunfold_n;
  int saved_sine    = g_cli_sine_applied;
  int saved_curry_n = g_curry_nfn;
  char (*saved_curry_fn)[TP_NAME_LEN] = g_curry_fn;
  int *saved_curry_ar = g_curry_ar;
  int saved_prov    = g_prov_on;
  int saved_ren_on  = g_tp_ren_on;
  char saved_ap2[TP_NAME_LEN];
  snprintf(saved_ap2, sizeof saved_ap2, "%s", g_curry_ap2);
  g_curry_fn = NULL;
  g_curry_ar = NULL;
  g_cli_goal_existential = 0;
  // A --proof run's lineage capture and rename table would record the
  // replayed transforms into the tables its proof cites.
  g_prov_on = 0;
  g_tp_ren_on = 0;
  int rc = 0;
  if (phi_run(phi_stage_unquote, tp, &pre)) {
    phi_trapped(c, c->parse_why);
  } else if (tp_needs_clausal(pre)) {
    fprintf(stderr, "FindProof: problem features: %s needs the clausal front "
            "end (FOF or non-unit CNF); they cover unit-equality CNF\n", file);
    rc = 2;
  } else if (phi_run(phi_stage_parse, tp, pre)) {
    phi_trapped(c, c->parse_why);
  } else {
    c->parsed = 1;
  }
  // The parse speaks only to end the process on malformed input (exit 2),
  // and that line must reach the caller: quiet after it.
  if (rc == 0) {
    int quiet = phi_quiet_begin();
    if (c->parsed) {
      phi_stated(tp, phi);
      c->goal_existential = g_cli_goal_existential;
      int n_neg = phi_count_neg(tp);
      c->no_goal = (n_neg == 0 || tp->n_cls == n_neg);
      phi_front(c, tp, lanes != NULL);
    }
    phi_fill(c, phi);
    if (lanes != NULL && c->fail[0] == '\0') {
      tp_lane_gates(c, lanes);
    }
    phi_quiet_end(quiet);
  }
  for (int i = 0; i < c->n_var; i++) {
    free(c->var[i].prp);
  }
  tp_syms_reset(tp);
  g_cli_goal_existential = saved_goal_ex;
  g_cli_equals_applied   = saved_equals;
  g_cli_defunfold_n      = saved_defun;
  g_cli_sine_applied     = saved_sine;
  free(g_curry_fn);
  free(g_curry_ar);
  g_curry_fn             = saved_curry_fn;
  g_curry_ar             = saved_curry_ar;
  g_curry_nfn            = saved_curry_n;
  g_prov_on              = saved_prov;
  g_tp_ren_on            = saved_ren_on;
  snprintf(g_curry_ap2, sizeof g_curry_ap2, "%s", saved_ap2);
  if (c->fail[0] != '\0') {
    fprintf(stderr, "FindProof: problem features: %s: %s\n", file, c->fail);
    rc = 2;
  }
  free(pre);
  free(tp);
  free(c);
  return rc;
}

static int phi_write_all(int fd, const char *buf, size_t n) {
  while (n > 0) {
    ssize_t w = write(fd, buf, n);
    if (w < 0 && errno == EINTR) {
      continue;
    }
    if (w <= 0) {
      return 0;
    }
    buf += w;
    n -= (size_t)w;
  }
  return 1;
}

static size_t phi_read_all(int fd, char *buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    ssize_t r = read(fd, buf + got, n - got);
    if (r < 0 && errno == EINTR) {
      continue;
    }
    if (r <= 0) {
      break;
    }
    got += (size_t)r;
  }
  return got;
}

// The CPU time phi(p) may take: a schedule parent computes it before it
// launches the lanes, so a problem past it goes without phi(p) (the
// selection layer stays unarmed) instead of delaying every lane.  CPU
// rather than wall time, so that load does not decide which problems
// have phi(p).
#define PHI_CPU_BUDGET_S 1.0

enum { PHI_OK = 0, PHI_FAILED = 2, PHI_OVER_BUDGET = 3 };

// phi(p) of the TPTP problem in `file` and, when lanes is not NULL, the
// static gate replica for every SCHEDULE lane (lanes[N_SCHED]), computed
// in a child process that gets cpu_budget_s seconds of CPU (0: no limit).
// Returns PHI_OK, PHI_OVER_BUDGET, or PHI_FAILED when the child fails (it
// names the reason on stderr).
static int tp_problem_features(const char *file, AtpPhi *phi, PhiMeta *meta,
                               TpLaneGate *lanes, double cpu_budget_s) {
  size_t n_lanes = (lanes != NULL) ? (size_t)N_SCHED : 0u;
  size_t off_meta = sizeof(AtpPhi);
  size_t off_lanes = off_meta + sizeof(PhiMeta);
  size_t want = off_lanes + n_lanes * sizeof(TpLaneGate);
  char *buf = (char *)malloc(want);
  int fd[2];
  if (buf == NULL || pipe(fd) != 0) {
    fprintf(stderr, "FindProof: problem features: cannot start the replay\n");
    free(buf);
    return 2;
  }
  fflush(stdout);
  fflush(stderr);
  pid_t parent = getpid();
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "FindProof: problem features: cannot fork\n");
    close(fd[0]);
    close(fd[1]);
    free(buf);
    return 2;
  }
  if (pid == 0) {
    close(fd[0]);
#if defined(__linux__) && defined(PR_SET_PDEATHSIG)
    // A parent stopped while phi(p) runs takes the replay with it.
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (getppid() != parent) {
      _exit(PHI_FAILED);
    }
#else
    (void)parent;
#endif
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    signal(SIGPROF, SIG_DFL);
    if (cpu_budget_s > 0.0) {
      struct itimerval it;
      memset(&it, 0, sizeof it);
      it.it_value.tv_sec = (time_t)cpu_budget_s;
      it.it_value.tv_usec =
          (suseconds_t)((cpu_budget_s - (double)it.it_value.tv_sec) * 1e6);
      if (it.it_value.tv_sec == 0 && it.it_value.tv_usec == 0) {
        it.it_value.tv_usec = 1;
      }
      setitimer(ITIMER_PROF, &it, NULL);
    }
    AtpPhi cp;
    PhiMeta cm;
    TpLaneGate *cl = (n_lanes > 0)
        ? (TpLaneGate *)calloc(n_lanes, sizeof(TpLaneGate)) : NULL;
    int rc = (n_lanes > 0 && cl == NULL) ? PHI_FAILED
                                         : phi_compute(file, &cp, &cm, cl);
    if (rc == 0) {
      memcpy(buf, &cp, sizeof cp);
      memcpy(buf + off_meta, &cm, sizeof cm);
      if (n_lanes > 0) {
        memcpy(buf + off_lanes, cl, n_lanes * sizeof(TpLaneGate));
      }
      rc = phi_write_all(fd[1], buf, want) ? PHI_OK : PHI_FAILED;
    }
    _exit(rc);
  }
  close(fd[1]);
  size_t got = phi_read_all(fd[0], buf, want);
  close(fd[0]);
  int st = 0;
  pid_t w;
  do {
    w = waitpid(pid, &st, 0);
  } while (w < 0 && errno == EINTR);
  if (w != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0 || got != want) {
    // A child stopped while it held the replayed .pr leaves it behind.
    char pr_path[64];
    snprintf(pr_path, sizeof pr_path, "/tmp/thvm_phi_%d.pr", (int)pid);
    unlink(pr_path);
    // Exit 2 has named its reason already.
    if (w != pid) {
      fprintf(stderr, "FindProof: problem features: %s: lost the replay\n", file);
    } else if (WIFSIGNALED(st) && WTERMSIG(st) == SIGPROF &&
               cpu_budget_s > 0.0) {
      fprintf(stderr, "FindProof: problem features: %s: phi(p) exceeded its "
              "%.2f s CPU budget\n", file, cpu_budget_s);
      free(buf);
      return PHI_OVER_BUDGET;
    } else if (WIFSIGNALED(st)) {
      fprintf(stderr, "FindProof: problem features: %s: the replay died "
              "(signal %d)\n", file, WTERMSIG(st));
    } else if (WEXITSTATUS(st) != 2) {
      fprintf(stderr, "FindProof: problem features: %s: the replay ended "
              "(exit %d, %zu of %zu bytes)\n", file, WEXITSTATUS(st), got, want);
    }
    free(buf);
    return PHI_FAILED;
  }
  memcpy(phi, buf, sizeof *phi);
  memcpy(meta, buf + off_meta, sizeof *meta);
  if (lanes != NULL) {
    memcpy(lanes, buf + off_lanes, n_lanes * sizeof(TpLaneGate));
  }
  free(buf);
  return PHI_OK;
}

// --- FindProof --problem-features ---------------------------------------

static void phi_problem_name(const char *file, char *buf, size_t cap) {
  const char *base = strrchr(file, '/');
  base = (base != NULL) ? base + 1 : file;
  size_t n = strlen(base);
  if (n > 2 && strcmp(base + n - 2, ".p") == 0) {
    n -= 2;
  }
  snprintf(buf, cap, "%.*s", (int)n, base);
}

// The column names of a phi(p) row, tab-separated: problem, the ATP_PHI_N
// columns, the PHI_META_N columns.  Its length, or -1 when it does not fit.
static int phi_format_header(char *buf, size_t cap) {
  int o = snprintf(buf, cap, "problem");
  for (int i = 0; i < ATP_PHI_N && o >= 0 && (size_t)o < cap; i++) {
    o += snprintf(buf + o, cap - (size_t)o, "\t%s", PHI_NAMES[i]);
  }
  for (int i = 0; i < PHI_META_N && o >= 0 && (size_t)o < cap; i++) {
    o += snprintf(buf + o, cap - (size_t)o, "\t%s", PHI_META_NAMES[i]);
  }
  return (o >= 0 && (size_t)o < cap) ? o : -1;
}

// One phi(p) row under phi_format_header.  %.9g reads back as the same
// float (FLT_DECIMAL_DIG digits).
static int phi_format_row(char *buf, size_t cap, const char *name,
                          const AtpPhi *phi, const PhiMeta *meta) {
  int o = snprintf(buf, cap, "%s", name);
  for (int i = 0; i < ATP_PHI_N && o >= 0 && (size_t)o < cap; i++) {
    o += snprintf(buf + o, cap - (size_t)o, "\t%.9g", (double)phi->v[i]);
  }
  for (int i = 0; i < PHI_META_N && o >= 0 && (size_t)o < cap; i++) {
    o += snprintf(buf + o, cap - (size_t)o, "\t%.9g", (double)meta->v[i]);
  }
  return (o >= 0 && (size_t)o < cap) ? o : -1;
}

// FindProof --problem-features [--header] [--lanes] [problem]: one
// tab-separated row of phi(p) and its meta columns, or with --lanes one row
// per SCHEDULE lane; --header prints the column names first (alone when no
// problem is given).  Exits 3 when phi(p) exceeds PHI_CPU_BUDGET_S.
static int cli_problem_features_main(const char *file, int header, int lanes) {
  char line[8192];
  if (header) {
    if (lanes) {
      printf("problem\tlane\tverdict\tstrict\tgates\treason\n");
    } else if (phi_format_header(line, sizeof line) >= 0) {
      printf("%s\n", line);
    }
    if (file == NULL) {
      return 0;
    }
  }
  if (file == NULL) {
    usage(stderr);
    return 2;
  }
  AtpPhi phi;
  PhiMeta meta;
  TpLaneGate *g = NULL;
  if (lanes) {
    g = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *g);
    if (g == NULL) {
      fprintf(stderr, "FindProof: OOM\n");
      return 2;
    }
  }
  // The replica is an offline tool and replays many configurations;
  // phi(p) alone runs under the budget a schedule parent gives it.
  int rc = tp_problem_features(file, &phi, &meta, g,
                               lanes ? 0.0 : PHI_CPU_BUDGET_S);
  if (rc != 0) {
    free(g);
    return rc;
  }
  char name[512];
  phi_problem_name(file, name, sizeof name);
  if (lanes) {
    for (int i = 0; i < N_SCHED; i++) {
      printf("%s\t%s\t%s\t%d\t%s\t%s\n", name, SCHEDULE[i].name,
             phi_verdict_name(g[i].verdict), g[i].strict, g[i].gates,
             (g[i].why[0] != '\0') ? g[i].why : "-");
    }
  } else if (phi_format_row(line, sizeof line, name, &phi, &meta) >= 0) {
    printf("%s\n", line);
  } else {
    fprintf(stderr, "FindProof: problem features: %s: the row is too long\n",
            file);
    rc = 2;
  }
  fflush(stdout);
  free(g);
  return rc;
}
