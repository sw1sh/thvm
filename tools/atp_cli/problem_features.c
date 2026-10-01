// ===================================================================
// Problem features phi(p) and the static gate replica
// ===================================================================
//
// tp_problem_features reads a TPTP unit-equality problem the way a
// schedule lane does and computes phi(p), the per-problem input of the
// selection layer (AtpPhi, src/thvm.h; docs/plans/enigma_portfolio.md,
// section 3.1), and, for every SCHEDULE lane, the verdict of its
// exit-fast gates.  FindProof --problem-features prints both, so training
// rows and the deployed layer read the same numbers.
//
// A gate bit is the gate's own predicate -- tp_has_ifeq_dispatcher,
// horn_slice_gate (through tp_horn_probe), tp_def_unfold_applies,
// tp_has_ax_ground_compound, tp_equals_goal_applies, tp_conj_syms,
// pr_otter_expands, the parse's g_cli_goal_existential -- read at the
// stage of main's front end where the gate reads it.  The stages
// themselves (currying, def-unfold, the equals encoding, axiom selection,
// the capacity gates, the canonical presentation, the .pr the bench
// loads) are replayed once per distinct lane configuration; a give-up
// returns through g_tp_giveup_trap, and a .pr the bench's reader rejects
// through g_pr_die_trap, as the line a lane would print.
//
// Not replayed: the Horn release, whose budget runs on the work clock.
// It can decide the problem (a lane run with --szs then proves it before
// any later gate) and it adds units that later gates read.  A lane that
// runs the release on a problem with an ifeq dispatcher and fails a gate
// after it is "undet"; "strict" says whether the predicted give-up line
// is still exactly what that lane prints without --szs.  The equals and
// narrowq gates read only the goals, so their lines stay strict; the
// def-unfold gate, the capacity gates near their bounds and axiom
// selection read the released units too.
//
// The replay writes process state no lane shares (the symbol hash, the
// curry table, the transform flags); all of it is reset on return, so
// call it before the process parses a problem of its own.

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
  [ATP_PHI_RATING]             = "rating",
  [ATP_PHI_SPC_KNOWN]          = "spc_known",
  [ATP_PHI_SPC_UNS]            = "spc_uns",
  [ATP_PHI_SPC_SAT]            = "spc_sat",
  [ATP_PHI_SPC_OPEN]           = "spc_open",
  [ATP_PHI_SPC_UEQ]            = "spc_ueq",
};

#define PHI_WHY_LEN 256

// Static gate replica verdicts.
enum { TP_LANE_LIVE = 0, TP_LANE_DEAD, TP_LANE_UNDET };

typedef struct {
  u8   verdict;            // TP_LANE_*
  u8   strict;             // why (or no gate line) is what a run without --szs prints
  char gates[64];          // the exit-fast gates the lane arms, comma-separated
  char why[PHI_WHY_LEN];   // the first gate's give-up line ("" when none fails)
} TpLaneGate;

// The front-end knobs one lane's child reads, from its SCHEDULE flags and
// the environment, parsed as the gate sites parse them.
typedef struct {
  u8   fof;            // --fof-slice >= 2
  u8   ifeq;           // --ifeq-guard >= 2
  u8   horn;           // the Horn release runs (--horn not 0)
  u8   horn_cd;        // --horn-cd >= 2
  u8   horn_back;      // --horn-back >= 2
  u8   def;            // --def-unfold 1 or 2: the transform runs
  u8   def_gate;       // --def-unfold=2
  u8   tgt_exit;       // --tgt-exit
  u8   eq;             // --equals-goal not 0: the encoding runs
  u8   eq_gate;        // --equals-goal >= 2
  u8   sine;           // axiom selection (THVM_ATP_SINE not 0)
  u8   canon;          // --canon-input not 0
  u8   tgt_sel_exit;   // --tgt-sel with --tgt-sel-exit
  u8   nq;             // --narrow-queue >= 2
  u8   otter;          // --otter-exit
  u8   prec_extra;     // a .pr transform or explicit precedence the replica leaves out
  u8   unmodelled;     // a gate the replica does not evaluate (--full-ac=2)
  char sym_prec[64];   // --sym-prec ("" when unset)
  char spb[16];        // --spb-prec ("" when unset)
} PhiKnobs;

// The front end replayed for one lane configuration.
typedef struct {
  u8   def, eq, sine;          // the configuration
  u8   eq_applied;             // g_cli_equals_applied after the encoding
  u8   ax_ground;              // the tgt-exit gate's input
  u8   pr_io;                  // the .pr could not be written or allocated
  int  n_hit, n_syms;          // the tgt-sel gate's input
  char why[PHI_WHY_LEN];       // the replayed stages' give-up line
  char canon_why[PHI_WHY_LEN]; // the canonical presentation's give-up line
  char pr_why[PHI_WHY_LEN];    // the bench's .pr reader's give-up line (pr_die)
  PrProblem *prp;              // the .pr the bench loads (NULL: not reached)
} PhiVariant;

#define PHI_MAX_VARIANTS 8

typedef struct {
  int  fof_input;
  char parse_why[PHI_WHY_LEN];
  int  no_goal;
  int  ifeq;
  char curry_why[PHI_WHY_LEN];
  int  horn_gate;              // HORN_GATE_*, -1 before the probe
  int  n_disp;
  int  def_bit, eq_bit, goal_existential;
  int  n_syms, n_vars;         // after currying
  int  n_pos, n_neg, n_hyp;
  PhiVariant var[PHI_MAX_VARIANTS];
  int  n_var;
} PhiCtx;

static int g_phi_thvm_ready = 0;

static int phi_on(const char *v) {
  return v != NULL && v[0] != '\0' && v[0] != '0';
}

// The value lane `lane`'s child reads for knob env `env`: its flag in the
// lane's SCHEDULE string (the last one wins, as main's setenv
// overwrites), else the environment the schedule hands down.  NULL when
// unset.
static const char *phi_lane_knob(int lane, const char *env, char *buf,
                                 size_t cap) {
  const char *val = getenv(env);
  const char *p = SCHEDULE[lane].flags;
  while (*p != '\0') {
    while (*p == ' ') {
      p++;
    }
    const char *tok = p;
    while (*p != '\0' && *p != ' ') {
      p++;
    }
    size_t tl = (size_t)(p - tok);
    if (tl < 3 || tok[0] != '-' || tok[1] != '-') {
      continue;
    }
    const char *name = tok + 2;
    const char *eq = memchr(name, '=', tl - 2);
    size_t nl = (eq != NULL) ? (size_t)(eq - name) : tl - 2;
    for (int k = 0; k < N_KNOBS; k++) {
      if (strlen(KNOBS[k].flag) != nl || strncmp(KNOBS[k].flag, name, nl) != 0) {
        continue;
      }
      if (strcmp(KNOBS[k].env, env) == 0) {
        if (eq != NULL) {
          size_t vl = (size_t)(p - eq - 1);
          if (vl >= cap) {
            vl = cap - 1;
          }
          memcpy(buf, eq + 1, vl);
          buf[vl] = '\0';
          val = buf;
        } else {
          val = "1";
        }
      }
      break;
    }
  }
  return val;
}

static void phi_lane_knobs(int lane, PhiKnobs *k) {
  char b1[64], b2[64];
  const char *v, *w;
  memset(k, 0, sizeof *k);
  v = phi_lane_knob(lane, "THVM_ATP_FOF_SLICE", b1, sizeof b1);
  k->fof = (v != NULL && atoi(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_IFEQ_GUARD", b1, sizeof b1);
  k->ifeq = (v != NULL && atoi(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_HORN", b1, sizeof b1);
  k->horn = (v == NULL || v[0] != '0');
  v = phi_lane_knob(lane, "THVM_ATP_HORN_CD", b1, sizeof b1);
  k->horn_cd = (v != NULL && v[0] != '0' && atoi(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_HORN_BACK", b1, sizeof b1);
  k->horn_back = (v != NULL && v[0] != '0' && atoi(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_DEF_UNFOLD", b1, sizeof b1);
  k->def = (v != NULL && (v[0] == '1' || v[0] == '2'));
  k->def_gate = (v != NULL && v[0] == '2');
  v = phi_lane_knob(lane, "THVM_ATP_TGT_EXIT", b1, sizeof b1);
  k->tgt_exit = (u8)phi_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_EQUALS_GOAL", b1, sizeof b1);
  k->eq = (v != NULL && v[0] != '0');
  k->eq_gate = (v != NULL && atoi(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_SINE", b1, sizeof b1);
  k->sine = (v == NULL || v[0] != '0');
  v = phi_lane_knob(lane, "THVM_ATP_CANON_INPUT", b1, sizeof b1);
  k->canon = (v == NULL || v[0] != '0');
  v = phi_lane_knob(lane, "THVM_ATP_TGT_SEL", b1, sizeof b1);
  w = phi_lane_knob(lane, "THVM_ATP_TGT_SEL_EXIT", b2, sizeof b2);
  k->tgt_sel_exit = (u8)(phi_on(v) && phi_on(w));
  v = phi_lane_knob(lane, "THVM_ATP_NARROW_QUEUE", b1, sizeof b1);
  k->nq = (v != NULL && atoi(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_OTTER_EXIT", b1, sizeof b1);
  k->otter = (u8)phi_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_SYM_PREC", b1, sizeof b1);
  snprintf(k->sym_prec, sizeof k->sym_prec, "%s", (v != NULL) ? v : "");
  v = phi_lane_knob(lane, "THVM_ATP_SPB_PREC", b1, sizeof b1);
  snprintf(k->spb, sizeof k->spb, "%s", (v != NULL) ? v : "");
  // The bench rewrites the .pr (tgt, goal flattening, distributivity
  // definitions) or the precedence (an explicit chain, the -auto path)
  // before the otter gate reads it; the replica does not.
  static const char *const extra[] = {
    "THVM_ATP_TGT", "THVM_ATP_FLATTEN_GOAL", "THVM_ATP_DISTRIB",
    "THVM_ATP_PRECEDENCE",
  };
  for (size_t i = 0; i < sizeof extra / sizeof extra[0]; i++) {
    v = phi_lane_knob(lane, extra[i], b1, sizeof b1);
    if (v != NULL && v[0] != '\0' && strcmp(v, "0") != 0) {
      k->prec_extra = 1;
    }
  }
  if (getenv("ATP_BENCH_AUTO_WEIGHTS") != NULL) {
    k->prec_extra = 1;
  }
  v = phi_lane_knob(lane, "THVM_ATP_FULL_AC", b1, sizeof b1);
  k->unmodelled = (v != NULL && v[0] == '2');
}

static void phi_gate_names(const PhiKnobs *k, char *buf, size_t cap) {
  const char *names[9];
  int n = 0;
  if (k->fof) {
    names[n++] = "fof";
  }
  if (k->ifeq) {
    names[n++] = "ifeq";
  }
  if (k->horn && (k->horn_cd || k->horn_back)) {
    names[n++] = "horn";
  }
  if (k->def_gate) {
    names[n++] = "def";
  }
  if (k->tgt_exit) {
    names[n++] = "tgt-ax";
  }
  if (k->tgt_sel_exit) {
    names[n++] = "tgt-sel";
  }
  if (k->eq_gate) {
    names[n++] = "equals";
  }
  if (k->nq) {
    names[n++] = "narrowq";
  }
  if (k->otter) {
    names[n++] = "otter";
  }
  size_t o = 0;
  buf[0] = '\0';
  for (int i = 0; i < n && o < cap; i++) {
    o += (size_t)snprintf(buf + o, cap - o, "%s%s", (i > 0) ? "," : "",
                          names[i]);
  }
  if (n == 0) {
    snprintf(buf, cap, "-");
  }
}

// --- front-end replay -------------------------------------------------

typedef void (*PhiStage)(TpProblem *tp, void *arg);

// Run one front-end stage with the give-up trap held: 0 when it returns,
// 1 when it gives up (its line is in g_tp_giveup_trap_why).
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

static int phi_count_neg(const TpProblem *tp) {
  int n = 0;
  for (int i = 0; i < tp->n_cls; i++) {
    n += tp->cls[i].neg;
  }
  return n;
}

static void phi_stage_parse(TpProblem *tp, void *arg) {
  tp_parse(tp, (const char *)arg);
}

static void phi_stage_curry(TpProblem *tp, void *arg) {
  (void)arg;
  tp_curry_wide(tp);
}

// main's front end from def-unfold to the capacity gates, for one lane
// configuration; the gates on the way are evaluated per lane.
static void phi_stage_front(TpProblem *tp, void *arg) {
  PhiVariant *v = (PhiVariant *)arg;
  if (v->def) {
    tp_def_unfold(tp);
  }
  v->ax_ground = (u8)tp_has_ax_ground_compound(tp);
  if (v->eq) {
    tp_equals_goal(tp);
    v->eq_applied = (u8)g_cli_equals_applied;
  }
  int n_neg = phi_count_neg(tp);
  if (v->sine && tp->n_cls - n_neg > (int)PR_MAX_EQNS) {
    tp_sine_select(tp);
    n_neg = phi_count_neg(tp);
  }
  tp_check_capacity(tp, n_neg);
}

static void phi_stage_canon(TpProblem *tp, void *arg) {
  (void)arg;
  tp_canon_input(tp);
}

// The bench's .pr reader with its give-up trapped: 0 when the .pr loads, 1
// when the reader gives up (its line is in g_pr_die_trap_why).  A give-up
// inside the reader's line walk leaves that walk's FILE open.
static int phi_pr_read(const char *path, PrProblem *p) {
  jmp_buf jb;
  g_pr_die_trap = &jb;
  if (setjmp(jb) != 0) {
    g_pr_die_trap = NULL;
    return 1;
  }
  pr_load_header(path, p);
  pr_load_terms(p);
  g_pr_die_trap = NULL;
  return 0;
}

// The .pr the bench would load, read by the bench's own reader: v->prp, or
// the reader's give-up line in v->pr_why, or v->pr_io when the file cannot
// be written.
static void phi_load_pr(TpProblem *tp, PhiVariant *v) {
  char path[64];
  snprintf(path, sizeof path, "/tmp/thvm_phi_%d.pr", (int)getpid());
  char *text = tp_emit_pr(tp, "phi");
  FILE *fp = fopen(path, "w");
  if (fp == NULL) {
    free(text);
    v->pr_io = 1;
    return;
  }
  int ok = (fputs(text, fp) >= 0);
  ok = (fclose(fp) == 0) && ok;
  free(text);
  PrProblem *p = ok ? (PrProblem *)malloc(sizeof *p) : NULL;
  if (p == NULL) {
    unlink(path);
    v->pr_io = 1;
    return;
  }
  if (!g_phi_thvm_ready) {
    thvm_init();
    g_phi_thvm_ready = 1;
  }
  if (phi_pr_read(path, p)) {
    snprintf(v->pr_why, sizeof v->pr_why, "%s", g_pr_die_trap_why);
    free(p);
    p = NULL;
  }
  unlink(path);
  v->prp = p;
}

// The bench's .pr reader gives up on this front end's output: its line.
static const char *phi_pr_why(const PhiVariant *v) {
  return (v->pr_why[0] != '\0') ? v->pr_why : NULL;
}

static void phi_variant_run(TpProblem *tp, PhiVariant *v) {
  g_cli_defunfold_n = 0;
  g_cli_equals_applied = 0;
  if (phi_run(phi_stage_front, tp, v)) {
    snprintf(v->why, sizeof v->why, "%s", g_tp_giveup_trap_why);
    return;
  }
  v->n_hit = tp_conj_syms(tp, NULL);
  v->n_syms = tp->n_syms;
  if (phi_run(phi_stage_canon, tp, NULL)) {
    snprintf(v->canon_why, sizeof v->canon_why, "%s", g_tp_giveup_trap_why);
    return;
  }
  phi_load_pr(tp, v);
}

// The configuration (def, eq, sine), added when `add` and not yet there.
static PhiVariant *phi_variant(PhiCtx *c, int def, int eq, int sine, int add) {
  for (int i = 0; i < c->n_var; i++) {
    PhiVariant *v = &c->var[i];
    if (v->def == def && v->eq == eq && v->sine == sine) {
      return v;
    }
  }
  if (!add || c->n_var >= PHI_MAX_VARIANTS) {
    return NULL;
  }
  PhiVariant *v = &c->var[c->n_var++];
  memset(v, 0, sizeof *v);
  v->def = (u8)def;
  v->eq = (u8)eq;
  v->sine = (u8)sine;
  return v;
}

// A lane's configuration: a transform it arms that changes nothing on
// this problem leaves it with the default one.
static PhiVariant *phi_lane_variant(PhiCtx *c, const PhiKnobs *k, int add) {
  return phi_variant(c, k->def && c->def_bit, k->eq && c->eq_bit, k->sine,
                     add);
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

// Point the symbol hash at tp's table: a stage replayed on a clone rebuilt
// it for the clone.
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

// The otter gate under one lane's precedence knobs: 1 expands, 0 not,
// -1 when the .pr was never reached.
static int phi_otter_expands(const PhiVariant *v, const PhiKnobs *k) {
  static u32 prec[PR_MAX_SYMS + PR_LABEL_BASE], wts[PR_MAX_SYMS + PR_LABEL_BASE];
  PrProblem *p = v->prp;
  if (p == NULL) {
    return -1;
  }
  memcpy(prec, p->precedence, sizeof prec);
  memcpy(wts, p->weights, sizeof wts);
  pr_sym_prec(p, (k->sym_prec[0] != '\0') ? k->sym_prec : NULL,
              (k->spb[0] != '\0') ? k->spb : NULL);
  atp_precedence_floor_reserved(p->precedence, p->n_syms + PR_LABEL_BASE);
  int e = pr_otter_expands(p);
  memcpy(p->precedence, prec, sizeof prec);
  memcpy(p->weights, wts, sizeof wts);
  return e;
}

// Released units reuse the signature and add at most HORN_REL_VARS
// variable names (V0, V1, ...) and no conjecture unit: with this much room
// the capacity gates and axiom selection's own give-up decide alike with
// and without them.
static int phi_release_neutral(const PhiCtx *c, const PhiVariant *v) {
  int syms = c->n_syms + (v->eq ? 3 : 0);
  int goal_units = c->n_neg + c->n_hyp + (v->eq ? 2 : 0);
  return syms <= (int)PR_MAX_SYMS &&
         c->n_vars + HORN_REL_VARS <= (int)PR_MAX_VARS &&
         goal_units <= (int)PR_MAX_EQNS;
}

static void phi_lane_set(TpLaneGate *g, int verdict, int strict,
                         const char *why) {
  g->verdict = (u8)verdict;
  g->strict = (u8)strict;
  snprintf(g->why, sizeof g->why, "%s", (why != NULL) ? why : "");
}

// The lane's gates in main's order.  Up to the horn-cd / horn-back gate no
// Horn release has run; after it, a lane with the release on a dispatcher
// problem may have proved the problem or read released units.
static void phi_lane_gate(PhiCtx *c, int lane, TpLaneGate *g) {
  PhiKnobs k;
  phi_lane_knobs(lane, &k);
  phi_gate_names(&k, g->gates, sizeof g->gates);
  const char *why = NULL;
  int release = 0, dep = 0;
  if (k.fof && !c->fof_input) {
    why = TP_GATE_FOF;
  } else if (c->parse_why[0] != '\0') {
    why = c->parse_why;
  } else if (c->no_goal) {
    why = TP_GATE_NO_GOAL;
  } else if (k.ifeq && !c->ifeq) {
    why = TP_GATE_IFEQ;
  } else if (c->curry_why[0] != '\0') {
    why = c->curry_why;
  } else if (k.horn && (k.horn_cd || k.horn_back) &&
             c->horn_gate > HORN_GATE_LIVE) {
    why = horn_slice_why(c->horn_gate, k.horn_cd, k.horn_back);
  } else {
    release = k.horn && c->n_disp > 0;
    const PhiVariant *v = phi_lane_variant(c, &k, 0);
    if (v == NULL) {
      phi_lane_set(g, TP_LANE_UNDET, 0, NULL);
      return;
    }
    int ex = 1;
    if (k.def_gate && !c->def_bit) {
      why = TP_GATE_DEF;
      dep = 1;
    } else if (k.tgt_exit && !v->ax_ground) {
      why = TP_GATE_TGT_AX;
      dep = 1;
    } else if (v->why[0] != '\0') {
      why = v->why;
      dep = !phi_release_neutral(c, v);
    } else if (k.canon && v->canon_why[0] != '\0') {
      why = v->canon_why;
      dep = 1;
    } else if (k.tgt_sel_exit && !tp_conj_cone_partial(v->n_hit, v->n_syms)) {
      why = TP_GATE_TGT_SEL;
      dep = 1;
    } else if (k.eq_gate && !v->eq_applied) {
      why = TP_GATE_EQUALS;
    } else if (k.nq && !c->goal_existential) {
      why = TP_GATE_NARROWQ;
    } else if (phi_pr_why(v) != NULL) {
      why = phi_pr_why(v);
    } else if (k.unmodelled) {
      phi_lane_set(g, TP_LANE_UNDET, 0, NULL);
      return;
    } else if (k.otter && (k.prec_extra || (ex = phi_otter_expands(v, &k)) < 0)) {
      phi_lane_set(g, TP_LANE_UNDET, 0, NULL);
      return;
    } else if (k.otter && ex == 0) {
      why = PR_GATE_OTTER;
      dep = 1;
    }
    if (why == NULL && release && (k.def || !phi_release_neutral(c, v))) {
      phi_lane_set(g, TP_LANE_UNDET, 0, NULL);
      return;
    }
  }
  if (why == NULL) {
    phi_lane_set(g, TP_LANE_LIVE, 1, NULL);
  } else {
    phi_lane_set(g, release ? TP_LANE_UNDET : TP_LANE_DEAD,
                 !(release && dep), why);
  }
}

// --- the problem as stated ----------------------------------------------

// Term depth of a clause side: a constant or a variable is 1.
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
  int n_ax = 0, n_goal = 0;
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
      v[ATP_PHI_GOAL_SIZE_MAX]  = fmaxf(v[ATP_PHI_GOAL_SIZE_MAX], (float)size);
      v[ATP_PHI_GOAL_DEPTH_MAX] = fmaxf(v[ATP_PHI_GOAL_DEPTH_MAX], (float)depth);
      v[ATP_PHI_GOAL_VARS_MAX]  = fmaxf(v[ATP_PHI_GOAL_VARS_MAX], (float)vars);
    } else if (cl->conj) {
      v[ATP_PHI_HYPOTHESES] += 1.0f;
    } else {
      n_ax++;
      ax_size += size;
      ax_depth += depth;
      ax_vars += vars;
      v[ATP_PHI_AX_SIZE_MAX]  = fmaxf(v[ATP_PHI_AX_SIZE_MAX], (float)size);
      v[ATP_PHI_AX_DEPTH_MAX] = fmaxf(v[ATP_PHI_AX_DEPTH_MAX], (float)depth);
      v[ATP_PHI_AX_VARS_MAX]  = fmaxf(v[ATP_PHI_AX_VARS_MAX], (float)vars);
      if (vars == 0) {
        v[ATP_PHI_AX_GROUND] += 1.0f;
      }
    }
  }
  v[ATP_PHI_AXIOMS] = (float)n_ax;
  v[ATP_PHI_GOALS] = (float)n_goal;
  if (n_ax > 0) {
    v[ATP_PHI_AX_SIZE_MEAN]  = (float)(ax_size / n_ax);
    v[ATP_PHI_AX_DEPTH_MEAN] = (float)(ax_depth / n_ax);
    v[ATP_PHI_AX_VARS_MEAN]  = (float)(ax_vars / n_ax);
  }
  if (n_goal > 0) {
    v[ATP_PHI_GOAL_SIZE_MEAN] = (float)(goal_size / n_goal);
  }
  v[ATP_PHI_SYMS] = (float)tp->n_syms;
  for (int i = 0; i < tp->n_syms; i++) {
    int ar = tp->syms[i].arity;
    v[(ar >= 4) ? ATP_PHI_SYMS_AR4P : ATP_PHI_SYMS_AR0 + ar] += 1.0f;
    v[ATP_PHI_MAX_ARITY] = fmaxf(v[ATP_PHI_MAX_ARITY], (float)ar);
  }
  v[ATP_PHI_VAR_NAMES] = (float)tp->n_vars;
}

// A TPTP header field ("% SPC      : ..." / "% Rating   : ...") of the raw
// text, copied into buf; 0 when the file has none (CASC copies carry no
// header).
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

static void phi_header(const char *raw, AtpPhi *phi) {
  char buf[256];
  if (phi_header_field(raw, "Rating", buf, sizeof buf)) {
    char *e = NULL;
    double r = strtod(buf, &e);
    if (e != buf) {
      phi->v[ATP_PHI_RATING] = (float)r;
    }
  }
  if (phi_header_field(raw, "SPC", buf, sizeof buf)) {
    phi->v[ATP_PHI_SPC_KNOWN] = 1.0f;
    int field = 0;
    for (char *tok = buf; *tok != '\0'; field++) {
      size_t tl = strcspn(tok, "_ \t");
      if (field == 1) {
        phi->v[ATP_PHI_SPC_UNS]  = (tl == 3 && strncmp(tok, "UNS", 3) == 0);
        phi->v[ATP_PHI_SPC_SAT]  = (tl == 3 && strncmp(tok, "SAT", 3) == 0);
        phi->v[ATP_PHI_SPC_OPEN] = (tl == 3 && (strncmp(tok, "UNK", 3) == 0 ||
                                                strncmp(tok, "OPN", 3) == 0));
      }
      if (tl == 3 && strncmp(tok, "UEQ", 3) == 0) {
        phi->v[ATP_PHI_SPC_UEQ] = 1.0f;
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

// The replayed stages narrate on stderr; a schedule parent stays silent.
static int phi_quiet_begin(void) {
  if (g_cli_verbose) {
    return -1;
  }
  fflush(stderr);
  int saved = dup(2);
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

// The gates and the replayed front end over the parsed, still uncurried
// problem tp, which the base configuration consumes last.
static void phi_front(PhiCtx *c, TpProblem *tp) {
  c->ifeq = tp_has_ifeq_dispatcher(tp);
  const char *ce = getenv("THVM_ATP_CURRY");
  if ((ce == NULL || ce[0] != '0') && phi_run(phi_stage_curry, tp, NULL)) {
    snprintf(c->curry_why, sizeof c->curry_why, "%s", g_tp_giveup_trap_why);
    return;
  }
  c->n_disp = tp_horn_probe(tp, &c->horn_gate);
  c->def_bit = tp_def_unfold_applies(tp);
  char eqn[TP_NAME_LEN], tru[TP_NAME_LEN], fls[TP_NAME_LEN];
  int n_nonground = 0;
  c->eq_bit = tp_equals_goal_applies(tp, eqn, tru, fls, &n_nonground);
  c->n_syms = tp->n_syms;
  c->n_vars = tp->n_vars;
  c->n_neg = phi_count_neg(tp);
  c->n_pos = tp->n_cls - c->n_neg;
  for (int i = 0; i < tp->n_cls; i++) {
    c->n_hyp += (!tp->cls[i].neg && tp->cls[i].conj);
  }
  const char *se = getenv("THVM_ATP_SINE");
  phi_variant(c, 0, 0, se == NULL || se[0] != '0', 1);   // var[0]: the default
  for (int lane = 0; lane < N_SCHED; lane++) {
    PhiKnobs k;
    phi_lane_knobs(lane, &k);
    phi_lane_variant(c, &k, 1);
  }
  for (int i = 1; i < c->n_var; i++) {
    TpProblem *w = phi_clone(tp);
    if (w == NULL) {
      snprintf(c->var[i].why, sizeof c->var[i].why, "OOM in problem features");
      continue;
    }
    phi_variant_run(w, &c->var[i]);
    free(w);
    phi_sym_hash_rebuild(tp);
  }
  phi_variant_run(tp, &c->var[0]);
}

// phi(p) of the TPTP problem in `file` and, when lanes is not NULL, the
// static gate replica for every SCHEDULE lane (lanes[N_SCHED]).  Returns 0,
// or 2 when the file cannot be read or is not unit-equality CNF (FOF and
// non-unit CNF go through the clausal front end, which this does not
// replay) or when the replayed .pr cannot be written.
static int tp_problem_features(const char *file, AtpPhi *phi,
                               TpLaneGate *lanes) {
  memset(phi, 0, sizeof *phi);
  phi->v[ATP_PHI_RATING] = -1.0f;
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
  phi_header(raw, phi);
  PhiCtx *c = (PhiCtx *)calloc(1, sizeof *c);
  TpProblem *tp = (TpProblem *)calloc(1, sizeof *tp);
  if (c == NULL || tp == NULL) {
    fprintf(stderr, "FindProof: OOM\n");
    free(c);
    free(tp);
    free(raw);
    return 2;
  }
  c->fof_input = tp_input_is_fof(raw);
  c->horn_gate = -1;
  char *pre = tp_preprocess(file, raw);
  pre = tp_unquote_names(pre);
  tp_shorten_names(pre);
  if (c->fof_input || tp_needs_clausal(pre)) {
    fprintf(stderr, "FindProof: problem features: %s needs the clausal front "
            "end (FOF or non-unit CNF); they cover unit-equality CNF\n", file);
    free(pre);
    free(raw);
    free(c);
    free(tp);
    return 2;
  }
  int saved_goal_ex = g_cli_goal_existential;
  int saved_equals  = g_cli_equals_applied;
  int saved_defun   = g_cli_defunfold_n;
  int saved_sine    = g_cli_sine_applied;
  int saved_curry_n = g_curry_nfn;
  int saved_prov    = g_prov_on;
  char saved_ap2[TP_NAME_LEN];
  snprintf(saved_ap2, sizeof saved_ap2, "%s", g_curry_ap2);
  g_cli_goal_existential = 0;
  // A --proof run's lineage capture would record the replayed transforms
  // into the tables its proof cites.
  g_prov_on = 0;
  // The parse speaks only to end the process on malformed input (exit 2),
  // and that line must reach the caller: quiet after it.
  int parsed = !phi_run(phi_stage_parse, tp, pre);
  if (!parsed) {
    snprintf(c->parse_why, sizeof c->parse_why, "%s", g_tp_giveup_trap_why);
  }
  int quiet = phi_quiet_begin();
  if (parsed) {
    phi_stated(tp, phi);
    c->goal_existential = g_cli_goal_existential;
    int n_neg = phi_count_neg(tp);
    c->no_goal = (n_neg == 0 || tp->n_cls == n_neg);
    if (!c->no_goal) {
      phi_front(c, tp);
    }
  }
  float *v = phi->v;
  const PhiVariant *base = (c->n_var > 0) ? &c->var[0] : NULL;
  int fe_live = base != NULL && base->why[0] == '\0' &&
                base->canon_why[0] == '\0' && phi_pr_why(base) == NULL;
  v[ATP_PHI_GOAL_EXISTENTIAL]   = (float)c->goal_existential;
  v[ATP_PHI_EQUALS_GOAL]        = (float)c->eq_bit;
  v[ATP_PHI_IFEQ_DISPATCHER]    = (float)c->ifeq;
  v[ATP_PHI_HORN_SLICE]         = (float)(c->horn_gate == HORN_GATE_LIVE);
  v[ATP_PHI_DEF_UNFOLD]         = (float)c->def_bit;
  v[ATP_PHI_FE_LIVE]            = (float)fe_live;
  v[ATP_PHI_SINE]               = (float)(c->n_pos > (int)PR_MAX_EQNS);
  if (base != NULL) {
    v[ATP_PHI_AX_GROUND_COMPOUND] = (float)base->ax_ground;
  }
  if (fe_live) {
    v[ATP_PHI_CONJ_CONE_PARTIAL] =
        (float)tp_conj_cone_partial(base->n_hit, base->n_syms);
    v[ATP_PHI_CONJ_SYM_FRAC] =
        (base->n_syms > 0) ? (float)base->n_hit / (float)base->n_syms : 0.0f;
    if (base->prp != NULL) {
      phi_algebra(base->prp, phi);
    }
  }
  if (lanes != NULL) {
    for (int lane = 0; lane < N_SCHED; lane++) {
      phi_lane_gate(c, lane, &lanes[lane]);
    }
  }
  phi_quiet_end(quiet);
  int rc = 0;
  for (int i = 0; i < c->n_var; i++) {
    if (c->var[i].pr_io) {
      rc = 2;
    }
    free(c->var[i].prp);
  }
  if (rc != 0) {
    fprintf(stderr, "FindProof: problem features: cannot write the "
            "replayed .pr to /tmp\n");
  }
  tp_syms_reset(tp);
  g_cli_goal_existential = saved_goal_ex;
  g_cli_equals_applied   = saved_equals;
  g_cli_defunfold_n      = saved_defun;
  g_cli_sine_applied     = saved_sine;
  g_curry_nfn            = saved_curry_n;
  g_prov_on              = saved_prov;
  snprintf(g_curry_ap2, sizeof g_curry_ap2, "%s", saved_ap2);
  free(pre);
  free(raw);
  free(tp);
  free(c);
  return rc;
}

static const char *phi_verdict_name(int verdict) {
  switch (verdict) {
    case TP_LANE_LIVE:  return "live";
    case TP_LANE_DEAD:  return "dead";
    default:            return "undet";
  }
}

// The problem's name: the file's base name less a ".p".
static void phi_problem_name(const char *file, char *buf, size_t cap) {
  const char *base = strrchr(file, '/');
  base = (base != NULL) ? base + 1 : file;
  size_t n = strlen(base);
  if (n > 2 && strcmp(base + n - 2, ".p") == 0) {
    n -= 2;
  }
  snprintf(buf, cap, "%.*s", (int)n, base);
}

// FindProof --problem-features [--header] [--lanes] [problem]: one
// tab-separated row of phi(p) with the replica's live and undet lane
// counts, or with --lanes one row per SCHEDULE lane; --header prints the
// column names first (alone when no problem is given).
static int cli_problem_features_main(const char *file, int header, int lanes) {
  if (header) {
    if (lanes) {
      printf("problem\tlane\tverdict\tstrict\tgates\treason\n");
    } else {
      printf("problem");
      for (int i = 0; i < ATP_PHI_N; i++) {
        printf("\t%s", PHI_NAMES[i]);
      }
      printf("\tlanes_live\tlanes_undet\n");
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
  TpLaneGate *g = (TpLaneGate *)calloc((size_t)N_SCHED, sizeof *g);
  if (g == NULL) {
    return 2;
  }
  int rc = tp_problem_features(file, &phi, g);
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
  } else {
    int live = 0, undet = 0;
    for (int i = 0; i < N_SCHED; i++) {
      live  += (g[i].verdict == TP_LANE_LIVE);
      undet += (g[i].verdict == TP_LANE_UNDET);
    }
    printf("%s", name);
    for (int i = 0; i < ATP_PHI_N; i++) {
      printf("\t%.9g", (double)phi.v[i]);
    }
    printf("\t%d\t%d\n", live, undet);
  }
  fflush(stdout);
  free(g);
  return 0;
}
