// ===================================================================
// The static gate replica
// ===================================================================
//
// tp_lane_gates gives every SCHEDULE lane's verdict over the front end
// problem_features.c replays: live (its exit-fast gates let it search),
// dead (a gate ends it first; the line it prints) or undet.
//
// Not replayed: the Horn release, whose budget runs on the work clock.  It
// can decide the problem (a lane run with --szs then proves it before any
// later gate) and it adds units that later gates read.  A lane that runs
// the release on a problem with an ifeq dispatcher and fails a gate after
// it is "undet"; "strict" says whether the predicted line is still exactly
// what that lane prints without --szs.  The equals and narrowq gates read
// only the goals (the equals gate also the clause count, far from its
// bound), so their lines stay strict; the def-unfold gate, the capacity
// gates near their bounds and axiom selection read the released units
// too.

// The knobs one lane's front end reads: its SCHEDULE flags, else the
// environment the schedule hands down.
typedef struct {
  u8   fof, ifeq, curry, horn, horn_cd, horn_back, def, def_gate, tgt_exit;
  u8   eq, eq_gate, sine, canon, tgt_sel_exit, nq, otter;
  u8   prec_extra;   // the bench rewrites the .pr or its precedence first
  u8   unmodelled;   // a gate the replica does not evaluate (--full-ac=2)
  char sym_prec[64];
  char spb[16];
} PhiKnobs;

// The tests the gate sites apply to a knob's value (NULL when unset), each
// copied from its site: they differ on "" and "0".
static int phi_knob_level(const char *v) {
  return (v != NULL) ? atoi(v) : 0;
}

static int phi_knob_on(const char *v) {           // atp_env_on: "" is off
  return v != NULL && v[0] != '\0' && v[0] != '0';
}

static int phi_knob_not0(const char *v) {         // "" is on
  return v != NULL && v[0] != '0';
}

static int phi_knob_default_on(const char *v) {
  return v == NULL || v[0] != '0';
}

// The value lane `lane`'s child reads for knob env `env`: its flag in the
// lane's SCHEDULE string (the last one wins, as main's setenv overwrites),
// else the environment.  NULL when unset.
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
  k->fof = (phi_knob_level(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_IFEQ_GUARD", b1, sizeof b1);
  k->ifeq = (phi_knob_level(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_CURRY", b1, sizeof b1);
  k->curry = (u8)phi_knob_default_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_HORN", b1, sizeof b1);
  k->horn = (u8)phi_knob_default_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_HORN_CD", b1, sizeof b1);
  k->horn_cd = (phi_knob_not0(v) && phi_knob_level(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_HORN_BACK", b1, sizeof b1);
  k->horn_back = (phi_knob_not0(v) && phi_knob_level(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_DEF_UNFOLD", b1, sizeof b1);
  k->def = (v != NULL && (v[0] == '1' || v[0] == '2'));
  k->def_gate = (v != NULL && v[0] == '2');
  v = phi_lane_knob(lane, "THVM_ATP_TGT_EXIT", b1, sizeof b1);
  k->tgt_exit = (u8)phi_knob_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_EQUALS_GOAL", b1, sizeof b1);
  k->eq = (u8)phi_knob_not0(v);
  k->eq_gate = (phi_knob_level(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_SINE", b1, sizeof b1);
  k->sine = (u8)phi_knob_default_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_CANON_INPUT", b1, sizeof b1);
  k->canon = (u8)phi_knob_default_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_TGT_SEL", b1, sizeof b1);
  w = phi_lane_knob(lane, "THVM_ATP_TGT_SEL_EXIT", b2, sizeof b2);
  k->tgt_sel_exit = (u8)(phi_knob_on(v) && phi_knob_on(w));
  v = phi_lane_knob(lane, "THVM_ATP_NARROW_QUEUE", b1, sizeof b1);
  k->nq = (phi_knob_level(v) >= 2);
  v = phi_lane_knob(lane, "THVM_ATP_OTTER_EXIT", b1, sizeof b1);
  k->otter = (u8)phi_knob_on(v);
  v = phi_lane_knob(lane, "THVM_ATP_SYM_PREC", b1, sizeof b1);
  snprintf(k->sym_prec, sizeof k->sym_prec, "%s", (v != NULL) ? v : "");
  v = phi_lane_knob(lane, "THVM_ATP_SPB_PREC", b1, sizeof b1);
  snprintf(k->spb, sizeof k->spb, "%s", (v != NULL) ? v : "");
  // The bench's .pr transforms and explicit precedences that run before
  // the otter gate, as its main tests them.
  v = phi_lane_knob(lane, "THVM_ATP_TGT", b1, sizeof b1);
  k->prec_extra |= (phi_knob_level(v) >= 1);
  v = phi_lane_knob(lane, "THVM_ATP_FLATTEN_GOAL", b1, sizeof b1);
  k->prec_extra |= (u8)phi_knob_not0(v);
  v = phi_lane_knob(lane, "THVM_ATP_DISTRIB", b1, sizeof b1);
  k->prec_extra |= (u8)phi_knob_not0(v);
  v = phi_lane_knob(lane, "THVM_ATP_PRECEDENCE", b1, sizeof b1);
  k->prec_extra |= (v != NULL && v[0] != '\0');
  k->prec_extra |= (getenv("ATP_BENCH_AUTO_WEIGHTS") != NULL);
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

// A lane's configuration: a transform it arms that changes nothing on this
// problem leaves it with the default one.
static PhiVariant *phi_lane_variant(PhiCtx *c, const PhiKnobs *k, int add) {
  TpSineCfg env = phi_sine_cfg_env();
  PhiCfg cfg;
  cfg.def = (u8)(k->def && c->def_bit);
  cfg.eq = (u8)(k->eq && c->eq_bit);
  cfg.sine = k->sine;
  cfg.canon = k->canon;
  cfg.sine_env = (u8)(k->sine && !tp_sine_cfg_is_default(&env));
  return phi_variant(c, &cfg, add);
}

// The otter gate under one lane's precedence knobs: 1 expands, 0 not.
static int phi_otter_expands(const PhiVariant *v, const PhiKnobs *k) {
  static u32 prec[PR_MAX_SYMS + PR_LABEL_BASE], wts[PR_MAX_SYMS + PR_LABEL_BASE];
  PrProblem *p = v->prp;
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

// The most units a Horn release adds: it fills the positive units up to
// the engine's equation capacity, or adds that many past it.
static int phi_release_max(const PhiCtx *c) {
  return (c->n_pos < (int)PR_MAX_EQNS) ? (int)PR_MAX_EQNS - c->n_pos
                                       : (int)PR_MAX_EQNS;
}

// The equals encoding gives up when two more clauses would cross the
// clause bound, which released units move closer.
static int phi_release_clause_room(const PhiCtx *c) {
  return c->n_pos + c->n_neg + phi_release_max(c) + 2 <= (int)TP_MAX_CLAUSES;
}

// Released units reuse the signature and add at most HORN_REL_VARS
// variable names (V0, V1, ...) and no conjecture unit.  A release that
// fills the positive units to capacity leaves the equation count under
// it, unless the equals encoding then turns goals into positive units:
// whether the count crosses the capacity (axiom selection runs, or with
// it off the capacity gate fires) must not depend on the release.  With
// this much room the capacity gates and axiom selection's own give-up
// decide alike with and without them.
static int phi_release_neutral(const PhiCtx *c, const PhiVariant *v) {
  int syms = c->n_syms + (v->cfg.eq ? 3 : 0);
  int goal_units = c->n_neg + c->n_hyp + (v->cfg.eq ? 2 : 0);
  int over = v->n_pos_fe > (int)PR_MAX_EQNS;
  int over_rel = v->n_pos_fe + phi_release_max(c) > (int)PR_MAX_EQNS;
  return syms <= (int)PR_MAX_SYMS &&
         c->n_vars + HORN_REL_VARS <= (int)PR_MAX_VARS &&
         goal_units <= (int)PR_MAX_EQNS &&
         over == over_rel &&
         (!v->cfg.eq || phi_release_clause_room(c));
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
  // The features cover CNF input, which carries no fof() record.
  if (k.fof) {
    why = TP_GATE_FOF;
  } else if (c->parse_why[0] != '\0') {
    why = c->parse_why;
  } else if (c->no_goal) {
    why = TP_GATE_NO_GOAL;
  } else if (k.ifeq && !c->ifeq) {
    why = TP_GATE_IFEQ;
  } else if (!k.curry && c->curried) {
    phi_lane_set(g, TP_LANE_UNDET, 0, NULL);
    return;
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
    if (k.def_gate && !c->def_bit) {
      why = TP_GATE_DEF;
      dep = 1;
    } else if (k.tgt_exit && !v->ax_ground) {
      why = TP_GATE_TGT_AX;
      dep = 1;
    } else if (v->why[0] != '\0') {
      why = v->why;
      dep = !phi_release_neutral(c, v);
    } else if (v->canon_why[0] != '\0') {
      why = v->canon_why;
      dep = 1;
    } else if (k.tgt_sel_exit && !tp_conj_cone_partial(v->n_hit, v->n_syms)) {
      why = TP_GATE_TGT_SEL;
      dep = 1;
    } else if (k.eq_gate && !v->eq_applied) {
      why = TP_GATE_EQUALS;
      dep = !phi_release_clause_room(c);
    } else if (k.nq && !c->goal_existential) {
      why = TP_GATE_NARROWQ;
    } else if (v->pr_why[0] != '\0') {
      why = v->pr_why;
    } else if (k.unmodelled || (k.otter && k.prec_extra)) {
      phi_lane_set(g, TP_LANE_UNDET, 0, NULL);
      return;
    } else if (k.otter && !phi_otter_expands(v, &k)) {
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

// The replayed configurations the lanes need, before the replay runs.
static void tp_lane_configs(PhiCtx *c) {
  for (int lane = 0; lane < N_SCHED; lane++) {
    PhiKnobs k;
    phi_lane_knobs(lane, &k);
    phi_lane_variant(c, &k, 1);
  }
}

static void tp_lane_gates(PhiCtx *c, TpLaneGate *lanes) {
  for (int lane = 0; lane < N_SCHED; lane++) {
    phi_lane_gate(c, lane, &lanes[lane]);
  }
}
