// test_atp_guide.c - the ENIGMA selection layer (src/atp/guide.c).
//
//   registry          the compiled-in example head, lookups, validation
//   lpo-quiet         thvm_lpo_quiet's verdict, the LPO statistics unmoved
//   goal-versions     the goal versions logged where the goal changes, and
//                     the version each seq maps to
//   push-features     the layer's push features equal the history export's
//                     'F' records seq by seq over whole runs (goal before
//                     and after the axioms, KBO and LPO)
//   zero-exact        --guide-zero selects exactly as the unguided engine:
//                     synthetic queues and whole runs (KBO, LPO, no goal,
//                     goal after the axioms, CP-set interreduction with
//                     orphan murder), both integrations, with and without
//                     coop picks
//   forward-parity    the layer's correction against a plain reference
//                     forward on the same inputs; the V1 input columns;
//                     phi set after arming; topk18 inputs against
//                     thvm_atp_cp_features
//   scratch-neutral   a decoding pick pops its heap scratch and moves no
//                     memo epoch or LPO statistic (KBO and LPO lanes)
//   normalization     the per-problem running mean
//   lazy              accept / demote / fallback / blocked, the k cap
//   coop              every coop-th call returns the root unscored
//   lrs-side-key      demotion never touches cp_pri, cp_seq or the heap,
//                     and follows its CP across row moves
//   cost-counters     the counters' invariants and the stats line
//   dump              the parity dump's rows
//   env               arming from THVM_ATP_GUIDE* in thvm_atp_init

#include "../src/thvm.c"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define LAB_b 5u
#define LAB_g 6u
#define VAR_x 0u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_b(void) { return term_new_ctr(LAB_b, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_i(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }
static Term mk_g(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_g, cs, 1); }

// KBO: i weight 0 and greatest; precedence i > f > g > b > e > a.
static u32 gw[7] = {0, 1, 0, 1, 1, 1, 1};
static u32 gp[7] = {0, 2, 6, 5, 1, 3, 4};
static const KboConfig CFG = {
  .weights = gw, .precedence = gp, .n_labels = 7, .var_weight = 1,
};
static u32 lp[7] = {0, 1, 6, 5, 2, 3, 4};
static const LpoConfig LPO = { .precedence = lp, .n_labels = 7 };

// --- heads built at run time -------------------------------------------

typedef struct {
  AtpGuideModel  m;
  AtpGuideFamily fam;
  float          mu[ATP_GUIDE_NX_MAX];
  float          sd[ATP_GUIDE_NX_MAX];
  float          w1[ATP_GUIDE_HMAX * ATP_GUIDE_NX_MAX];
  float          b1[ATP_GUIDE_HMAX];
  float          w2[ATP_GUIDE_HMAX];
} TestHead;

static TestHead H_RAND, H_TOPK18, H_LAZY, H_RANK;

static void head_init(TestHead *t, u32 layout, u32 n_phi, u32 n_lane, u32 hid) {
  memset(t, 0, sizeof *t);
  u8 v1 = (layout == ATP_GUIDE_LAYOUT_V1);
  t->m.id     = "test";
  t->m.layout = layout;
  t->m.n_phi  = v1 ? n_phi : 0u;
  t->m.n_lane = v1 ? n_lane : 0u;
  t->m.n_x    = v1 ? ATP_GUIDE_NC + ATP_GUIDE_NS + n_phi + n_lane : ATP_GUIDE_NX_TOPK18;
  t->m.hid    = hid;
  t->m.mu     = t->mu;
  t->m.sd     = t->sd;
  t->m.w1     = t->w1;
  t->m.b1     = t->b1;
  t->m.w2     = t->w2;
  t->m.b2     = 0.0f;
  t->m.kappa  = 1.0f;
  t->m.n_fam  = 1u;
  t->m.fam    = &t->fam;
  for (u32 j = 0; j < t->m.n_x; j++) {
    t->sd[j] = 1.0f;
  }
  t->fam.name = "*";
  t->fam.lane = 0u;
  t->fam.mode = ATP_GUIDE_TOPK;
  t->fam.k    = 8u;
  t->fam.tau  = 0.0;
  t->fam.coop = 0u;
}

static u32 g_lcg = 1u;
static u32 lcg(void) {
  g_lcg = g_lcg * 1103515245u + 12345u;
  return (g_lcg >> 16) & 0x7fffu;
}
static double rnd(void) {
  return (double)lcg() / 16384.0 - 1.0;
}

static void head_random(TestHead *t, u32 seed) {
  g_lcg = seed;
  for (u32 j = 0; j < t->m.n_x; j++) {
    t->mu[j] = (float)(0.5 * rnd());
    t->sd[j] = (float)(0.5 + fabs(rnd()));
  }
  for (u32 i = 0; i < t->m.hid * t->m.n_x; i++) {
    t->w1[i] = (float)(0.5 * rnd());
  }
  for (u32 h = 0; h < t->m.hid; h++) {
    t->b1[h] = (float)(0.1 * rnd());
    t->w2[h] = (float)rnd();
  }
  t->m.b2 = (float)(0.1 * rnd());
}

// The correction, computed the plain way: every column, in order.
static double ref_forward(const AtpGuideModel *m, const double *x) {
  double out = (double)m->b2;
  for (u32 h = 0; h < m->hid; h++) {
    double a = (double)m->b1[h];
    for (u32 j = 0; j < m->n_x; j++) {
      a += (double)m->w1[h * m->n_x + j] * ((x[j] - (double)m->mu[j]) / (double)m->sd[j]);
    }
    if (a > 0.0) {
      out += (double)m->w2[h] * a;
    }
  }
  return out;
}

static double rel(double a, double b) {
  double d = fabs(a - b);
  double s = fabs(b) > 1.0 ? fabs(b) : 1.0;
  return d / s;
}

static AtpGuide *G(AtpState *s) {
  return (AtpGuide *)s->guide;
}

static int arm(AtpState *s, TestHead *t, u32 mode, u32 k, double tau, u32 coop, u8 zero) {
  t->fam.mode = mode;
  t->fam.k    = k;
  t->fam.tau  = tau;
  t->fam.coop = coop;
  return atp_guide_arm(s, &t->m, &t->fam, "test", zero);
}

// A temporary file path (mkstemp template) under $TMPDIR.
static void tmp_template(char *buf, size_t n, const char *stem) {
  const char *d = getenv("TMPDIR");
  snprintf(buf, n, "%s/%s_XXXXXX", (d != NULL && d[0] != '\0') ? d : "/tmp", stem);
}

// --- problems ------------------------------------------------------------

// Group axioms (right identity, right inverse, associativity) with:
//   0 the goal i(i(a)) = a set before the axioms, KBO
//   1 no goal (completion)
//   2 problem 0 under LPO
//   3 problem 0 with the goal set after the axioms, FindProof's order
//   4 problem 3 under LPO
//   5 problem 0 with CP-set interreduction every 4 rules and orphan murder
//   6 the goal i(f(a, b)) = f(i(b), i(a)) set after the axioms, KBO
//   7 problem 6 under LPO
#define N_PROBLEMS 8u

static AtpState *problem(u32 which) {
  AtpState *s = thvm_atp_init(&CFG, 5000u);
  if (which == 2u || which == 4u || which == 7u) {
    thvm_atp_set_lpo(s, &LPO);
  }
  if (which == 5u) {
    thvm_atp_set_cp_set_interreduce(s, 1u);
    s->cp_set_ir_period = 4u;
    thvm_atp_set_use_orphan_murder(s, 1u);
  }
  return s;
}

static void problem_goal(AtpState *s, u32 which) {
  if (which >= 6u) {
    thvm_atp_set_goal(s, mk_i(mk_f(mk_a(), mk_b())), mk_f(mk_i(mk_b()), mk_i(mk_a())));
  } else {
    thvm_atp_set_goal(s, mk_i(mk_i(mk_a())), mk_a());
  }
}

static void problem_load(AtpState *s, u32 which) {
  u8 goal_first = (which == 0u || which == 2u || which == 5u);
  u8 goal_last  = (which == 3u || which == 4u || which >= 6u);
  if (goal_first) {
    problem_goal(s, which);
  }
  thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),           mk_v(VAR_x));
  thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
  thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                        mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
  if (goal_last) {
    problem_goal(s, which);
  }
}

#define TRAJ_MAX 4096u
typedef struct {
  u32 n;
  u32 seq[TRAJ_MAX];
  int status;
  u32 sel;
  u32 rules;
  u32 cps;
  u32 steps;
} Traj;

static Traj T_BASE, T_ARM;

static void run_traj(AtpState *s, u32 max_steps, Traj *t) {
  t->n = 0u;
  AtpStatus st = ATP_RUNNING;
  for (u32 k = 0; k < max_steps; k++) {
    u32 before = s->cp_select_count;
    st = thvm_atp_step(s);
    if (s->cp_select_count != before && t->n < TRAJ_MAX) {
      t->seq[t->n++] = s->last_popped_seq;
    }
    if (st != ATP_RUNNING) {
      break;
    }
  }
  t->status = (int)st;
  t->sel    = s->cp_select_count;
  t->rules  = s->n_rules;
  t->cps    = s->n_cps;
  t->steps  = s->step;
}

static int traj_eq(const Traj *a, const Traj *b) {
  if (a->n != b->n || a->status != b->status || a->sel != b->sel || a->rules != b->rules ||
      a->cps != b->cps || a->steps != b->steps) {
    return 0;
  }
  for (u32 i = 0; i < a->n; i++) {
    if (a->seq[i] != b->seq[i]) {
      return 0;
    }
  }
  return 1;
}

// --- synthetic queues ------------------------------------------------------

static Term synth(u32 depth) {
  u32 r = lcg() % 6u;
  if (depth == 0u || r < 2u) {
    u32 leaf = lcg() % 4u;
    if (leaf == 0u) {
      return mk_a();
    }
    if (leaf == 1u) {
      return mk_b();
    }
    if (leaf == 2u) {
      return mk_e();
    }
    return mk_v(lcg() % 3u);
  }
  if (r == 2u) {
    return mk_i(synth(depth - 1u));
  }
  if (r == 3u) {
    return mk_g(synth(depth - 1u));
  }
  Term x = synth(depth - 1u);
  Term y = synth(depth - 1u);
  return mk_f(x, y);
}

static void queue_put(AtpState *s, u32 i, Term l, Term r, u32 depth) {
  thvm_atp_cp_set(s, i, l, r);
  s->cp_trace[i] = ATP_TRACE_NONE;
  s->cp_depth[i] = depth;
}

// Heapify rows [0, n): the queue a run's pushes would have left.
static void queue_done(AtpState *s, u32 n) {
  s->n_cps = n;
  thvm_atp_cp_reheapify(s);
}

// The goal f(a, i(a)) = e, then rows 0-5 = six pri-2 CPs over b and g
// (no goal symbol), row 6 = f(a, a) = a (pri 3, only goal symbols), row
// 7 = g(g(g(b))) = b (pri 4).  The engine order is the row order.
static void lazy_queue(AtpState *s) {
  thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
  for (u32 i = 0; i < 6u; i++) {
    if (i % 2u == 0u) {
      queue_put(s, i, mk_b(), mk_b(), 0u);
    } else {
      queue_put(s, i, mk_g(mk_b()), mk_b(), 0u);
    }
  }
  queue_put(s, 6u, mk_f(mk_a(), mk_a()), mk_a(), 0u);
  queue_put(s, 7u, mk_g(mk_g(mk_g(mk_b()))), mk_b(), 0u);
  queue_done(s, 8u);
  s->fifo_modulo = 1000000u;
}

// Ten CPs g^k(b) = b, k = 1..10: pri k + 1, engine order = row order.
static void rank_queue(AtpState *s) {
  for (u32 k = 1; k <= 10u; k++) {
    Term l = mk_b();
    for (u32 j = 0; j < k; j++) {
      l = mk_g(l);
    }
    queue_put(s, k - 1u, l, mk_b(), k % 3u);
  }
  queue_done(s, 10u);
  s->fifo_modulo = 1000000u;
}

static u32 pos_of_pri(const AtpState *s, u32 pri) {
  for (u32 p = 0; p < s->n_cps; p++) {
    if (s->cp_heap[p].pri == pri) {
      return p;
    }
  }
  return 0xffffffffu;
}

static u32 pop_seq(AtpState *s) {
  Term l = 0, r = 0;
  if (!thvm_atp_select_cp(s, &l, &r)) {
    return 0xffffffffu;
  }
  return s->last_popped_seq;
}

// demoted(row) == (its seq is in want[0..n)) for every queued row.
static int dem_matches(AtpState *s, const u32 *want, u32 n) {
  for (u32 r = 0; r < s->n_cps; r++) {
    u8 in = 0u;
    for (u32 k = 0; k < n; k++) {
      if (want[k] == s->cp_seq[r]) {
        in = 1u;
      }
    }
    if (atp_guide_demoted(s, G(s), r) != in) {
      return 0;
    }
  }
  return 1;
}

// --- the history export's push features --------------------------------

#define FEAT_CAP 65536u
#define SEEN_CAP 65536u

static float F_REC[FEAT_CAP][ATP_CP_FEATURE_DIM];
static u8    F_HAS[FEAT_CAP];
static u32   SEEN_SEQ[SEEN_CAP];
static float SEEN_F[SEEN_CAP][ATP_CP_FEATURE_DIM];

// The 'F' records of an export file by seq, the last one per seq; seqs
// past FEAT_CAP are counted in *beyond.
static u32 read_feat_records(const char *path, u32 *beyond) {
  memset(F_HAS, 0, sizeof F_HAS);
  *beyond = 0u;
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    return 0u;
  }
  u8 rec[ATP_HX_REC_SIZE];
  u32 n = 0u;
  while (fread(rec, ATP_HX_REC_SIZE, 1u, f) == 1u) {
    if (rec[0] != 'F') {
      continue;
    }
    AtpHxFeatRec fr;
    memcpy(&fr, rec, sizeof fr);
    if (fr.seq >= FEAT_CAP) {
      (*beyond)++;
      continue;
    }
    memcpy(F_REC[fr.seq], fr.feat, sizeof F_REC[0]);
    F_HAS[fr.seq] = 1u;
    n++;
  }
  fclose(f);
  return n;
}

int main(void) {
  thvm_init();

  head_init(&H_RAND, ATP_GUIDE_LAYOUT_V1, 3u, 4u, 16u);
  head_random(&H_RAND, 7u);
  H_RAND.fam.lane = 2u;
  head_init(&H_TOPK18, ATP_GUIDE_LAYOUT_TOPK18, 0u, 0u, 8u);
  head_random(&H_TOPK18, 11u);
  // corr = 2 * (goal-symbol share) - 1.
  head_init(&H_LAZY, ATP_GUIDE_LAYOUT_V1, 0u, 0u, 1u);
  H_LAZY.w1[13] = 2.0f;
  H_LAZY.w2[0]  = 1.0f;
  H_LAZY.m.b2   = -1.0f;
  // kappa 0, corr = log1p(rank): the argmax is the last candidate.
  head_init(&H_RANK, ATP_GUIDE_LAYOUT_V1, 0u, 0u, 1u);
  H_RANK.w1[0]   = 1.0f;
  H_RANK.w2[0]   = 1.0f;
  H_RANK.m.kappa = 0.0f;

  TEST_BEGIN("atp/guide/registry");
  {
    const AtpGuideModel *m = atp_guide_model(NULL);
    CHECK(m != NULL);
    CHECK(m != NULL && strcmp(m->id, "example") == 0);
    CHECK(m == atp_guide_model(""));
    CHECK(m == atp_guide_model("example"));
    CHECK(atp_guide_model("no-such-head") == NULL);
    if (m != NULL) {
      CHECK_EQ(m->layout, ATP_GUIDE_LAYOUT_V1);
      CHECK_EQ(m->n_x, ATP_GUIDE_NC + ATP_GUIDE_NS + m->n_phi + m->n_lane);
      const AtpGuideFamily *f = atp_guide_family(m, "twee");
      CHECK(f != NULL && strcmp(f->name, "*") == 0);
      CHECK(atp_guide_valid(m, f));
    }
    AtpGuideFamily f = H_RAND.fam;
    CHECK(atp_guide_valid(&H_RAND.m, &f));
    CHECK(atp_guide_valid(&H_TOPK18.m, &f));
    f.k = 0u;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    f.k = ATP_GUIDE_KMAX + 1u;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    f.k = ATP_GUIDE_KMAX;
    f.tau = -0.5;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    f.tau = 0.0;
    f.mode = 7u;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    f.mode = ATP_GUIDE_LAZY;
    CHECK(atp_guide_valid(&H_RAND.m, &f));
    H_RAND.m.kappa = -1.0f;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    H_RAND.m.kappa = 1.0f;
    float sd0 = H_RAND.sd[5];
    H_RAND.sd[5] = 0.0f;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    H_RAND.sd[5] = sd0;
    H_RAND.m.n_x++;
    CHECK(!atp_guide_valid(&H_RAND.m, &f));
    H_RAND.m.n_x--;

    AtpGuideFamily p = f;
    CHECK(atp_guide_parse_mode("topk:16", &p) && p.mode == ATP_GUIDE_TOPK && p.k == 16u);
    CHECK(atp_guide_parse_mode("lazy:8:0.25", &p) && p.mode == ATP_GUIDE_LAZY && p.k == 8u &&
          p.tau == 0.25);
    CHECK(atp_guide_parse_mode("lazy:5", &p) && p.k == 5u && p.tau == 0.0);
    CHECK(!atp_guide_parse_mode("lazy:65", &p));
    CHECK(!atp_guide_parse_mode("topk:0", &p));
    CHECK(!atp_guide_parse_mode("topk:4:1", &p));
    CHECK(!atp_guide_parse_mode("wide:4", &p));

    AtpState *s = thvm_atp_init(&CFG, 100u);
    CHECK_EQ(thvm_atp_guide_arm(s, "twee", NULL, 1u), 1);
    CHECK(s->guide != NULL && G(s)->m == m && G(s)->zero == 1u);
    CHECK_EQ(thvm_atp_guide_arm(s, "twee", NULL, 1u), 0);
    thvm_atp_free(s);
    s = thvm_atp_init(&CFG, 100u);
    CHECK_EQ(thvm_atp_guide_arm(s, "twee", "no-such-head", 0u), 0);
    CHECK(s->guide == NULL);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/lpo-quiet");
  {
    // thvm_lpo's verdict, with the LPO statistics unmoved (the memo is
    // persistent here, as in a run).
    AtpState *s = thvm_atp_init(&CFG, 100u);
    g_lcg = 5u;
    u32 agree = 0u;
    for (u32 k = 0; k < 200u; k++) {
      Term l = synth(4u);
      Term r = synth(4u);
      u64 a0 = 0, b0 = 0, c0 = 0, d0 = 0, a1 = 0, b1 = 0, c1 = 0, d1 = 0;
      thvm_lpo_flat_stats(&a0, &b0, &c0, &d0);
      LpoCmp q = LPO_EQ;
      CHECK(thvm_lpo_quiet(l, r, &LPO, &q));
      thvm_lpo_flat_stats(&a1, &b1, &c1, &d1);
      CHECK(a0 == a1 && b0 == b1 && c0 == c1 && d0 == d1);
      agree += (u32)(q == thvm_lpo(l, r, &LPO));
    }
    CHECK_EQ(agree, 200u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/goal-versions");
  {
    // Armed with no goal: one version from seq 0.  thvm_atp_set_goals
    // logs a version at cp_seq_next; one nothing was pushed under is
    // replaced, and an unchanged goal logs nothing.
    AtpState *s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 8u, 0.0, 0u, 1u));
    AtpGuide *g = G(s);
    CHECK_EQ(g->n_gv, 1u);
    CHECK(g->gv[0].seq == 0u && g->gv[0].l == 0 && g->gv[0].r == 0);
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()), mk_v(VAR_x));
    thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
    u32 n0 = s->cp_seq_next;
    CHECK(n0 > 0u);
    Term g1l = mk_i(mk_i(mk_a()));
    Term g1r = mk_a();
    CHECK(thvm_atp_set_goal(s, g1l, g1r));
    CHECK_EQ(g->n_gv, 2u);
    CHECK_EQ(g->gv[1].seq, n0);
    CHECK(g->gv[1].l == g1l && g->gv[1].r == g1r);
    Term g2l = mk_f(mk_a(), mk_b());
    Term g2r = mk_b();
    CHECK(thvm_atp_set_goal(s, g2l, g2r));
    CHECK_EQ(g->n_gv, 2u);
    CHECK(g->gv[1].l == g2l && g->gv[1].r == g2r);
    thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                          mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
    CHECK(thvm_atp_set_goal(s, g2l, g2r));
    CHECK_EQ(g->n_gv, 2u);
    CHECK(atp_guide_goal_at(g, 0u) == &g->gv[0]);
    CHECK(atp_guide_goal_at(g, n0 - 1u) == &g->gv[0]);
    CHECK(atp_guide_goal_at(g, n0) == &g->gv[1]);
    CHECK(atp_guide_goal_at(g, 1000u) == &g->gv[1]);
    u32 n1 = s->cp_seq_next;
    CHECK(n1 > n0);
    CHECK(thvm_atp_set_goal(s, 0, 0));
    CHECK_EQ(g->n_gv, 3u);
    CHECK_EQ(g->gv[2].seq, n1);
    CHECK(g->gv[2].l == 0 && g->gv[2].r == 0);
    CHECK(atp_guide_goal_at(g, n1 - 1u) == &g->gv[1]);
    thvm_atp_free(s);

    // Whole runs: after every step the last version is the goal's
    // current normal form (both goal checks log), and pushes add none.
    u32 grew = 0u;
    for (u32 which = 3u; which <= 6u; which += 3u) {
      s = problem(which);
      CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 8u, 0.0, 0u, 1u));
      problem_load(s, which);
      u32 steps = 0u, bad = 0u;
      for (u32 k = 0; k < 400u; k++) {
        u32 nv = G(s)->n_gv;
        AtpStatus st = thvm_atp_step(s);
        steps++;
        Term gl, gr;
        atp_guide_goal_now(s, &gl, &gr);
        const AtpGuideGoal *last = &G(s)->gv[G(s)->n_gv - 1u];
        bad += (u32)(last->l != gl || last->r != gr);
        grew += (u32)(G(s)->n_gv > nv);
        if (st != ATP_RUNNING) {
          break;
        }
      }
      CHECK_EQ(bad, 0u);
      CHECK(G(s)->n_gv <= 2u + 2u * steps);
      CHECK(G(s)->n_gv < s->cp_seq_next);
      thvm_atp_free(s);
    }
    CHECK(grew > 0u);
  }

  TEST_BEGIN("atp/guide/push-features");
  {
    // Every candidate a topk18 pick decodes carries the push features the
    // history export records for its seq ('F': thvm_atp_cp_features at the
    // push, against the goal of that moment), bit for bit.
    static const u32 which_list[5] = {0u, 2u, 3u, 6u, 7u};
    for (u32 w = 0; w < 5u; w++) {
      u32 which = which_list[w];
      char path[512];
      tmp_template(path, sizeof path, "thvm_guide_hx");
      int fd = mkstemp(path);
      CHECK(fd >= 0);
      if (fd < 0) {
        continue;
      }
      close(fd);
      setenv("THVM_ATP_HISTORY_EXPORT", path, 1);
      setenv("THVM_ATP_HISTORY_RATE", "1", 1);
      setenv("THVM_ATP_HISTORY_CPFEAT", "1", 1);
      AtpState *s = problem(which);
      unsetenv("THVM_ATP_HISTORY_EXPORT");
      unsetenv("THVM_ATP_HISTORY_RATE");
      unsetenv("THVM_ATP_HISTORY_CPFEAT");
      CHECK(arm(s, &H_TOPK18, ATP_GUIDE_TOPK, 64u, 0.0, 0u, 1u));
      problem_load(s, which);
      u32 n_seen = 0u;
      for (u32 k = 0; k < 400u; k++) {
        AtpGuide *g = G(s);
        u64 sc0 = g->n_scored;
        AtpStatus st = thvm_atp_step(s);
        if (g->n_scored != sc0) {
          for (u32 i = 0; i < g->n_cand && n_seen < SEEN_CAP; i++) {
            if (g->cand[i].ft.has) {
              SEEN_SEQ[n_seen] = g->cand[i].seq;
              memcpy(SEEN_F[n_seen], g->cand[i].ft.f, sizeof SEEN_F[0]);
              n_seen++;
            }
          }
        }
        if (st != ATP_RUNNING) {
          break;
        }
      }
      CHECK_EQ(G(s)->n_dirty, 0u);
      thvm_atp_free(s);
      u32 beyond = 0u;
      u32 n_rec = read_feat_records(path, &beyond);
      unlink(path);
      CHECK(n_rec > 0u);
      CHECK_EQ(beyond, 0u);
      CHECK(n_seen > 20u);
      u32 missing = 0u, differ = 0u, goal_col = 0u, no_goal_col = 0u;
      for (u32 i = 0; i < n_seen; i++) {
        u32 q = SEEN_SEQ[i];
        if (q >= FEAT_CAP || !F_HAS[q]) {
          missing++;
          continue;
        }
        for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) {
          if (SEEN_F[i][j] != F_REC[q][j]) {
            differ++;
            break;
          }
        }
        goal_col    += (u32)(F_REC[q][7] != 0.0f);
        no_goal_col += (u32)(F_REC[q][7] == 0.0f);
      }
      CHECK_EQ(missing, 0u);
      CHECK_EQ(differ, 0u);
      CHECK(goal_col > 0u);
      if (which == 3u || which == 6u || which == 7u) {
        // The axioms were pushed before the goal: no goal weight.
        CHECK(no_goal_col > 0u);
      }
    }
  }

  TEST_BEGIN("atp/guide/zero-exact-synthetic");
  {
    // One random queue, popped to the end unarmed and under each
    // zero-correction integration: the same CPs in the same order.
    static u32 base[64], got[64];
    u32 nb = 0u;
    u64 hc0 = thvm_atp_heap_checkpoint();
    u64 gc0 = gc_count();
    for (u32 cfg = 0; cfg < 5u; cfg++) {
      g_lcg = 99u;
      if (gc_count() == gc0) {
        thvm_atp_heap_reset(hc0);
      }
      AtpState *s = thvm_atp_init(&CFG, 100u);
      thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
      switch (cfg) {
        case 1u: CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 8u, 0.0, 0u, 1u)); break;
        case 2u: CHECK(arm(s, &H_RAND, ATP_GUIDE_LAZY, 8u, 0.0, 0u, 1u)); break;
        case 3u: CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 64u, 0.0, 2u, 1u)); break;
        case 4u: CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 64u, 0.5, 3u, 1u)); break;
        default: break;
      }
      for (u32 i = 0; i < 40u; i++) {
        Term l = synth(3u);
        Term r = synth(3u);
        queue_put(s, i, l, r, i % 5u);
      }
      queue_done(s, 40u);
      u32 n = 0u, q;
      while ((q = pop_seq(s)) != 0xffffffffu && n < 64u) {
        got[n++] = q;
      }
      if (cfg == 0u) {
        nb = n;
        memcpy(base, got, sizeof base);
        CHECK_EQ(n, 40u);
      } else {
        CHECK_EQ(n, nb);
        CHECK(memcmp(base, got, n * sizeof(u32)) == 0);
        CHECK(G(s)->n_scored > 0u);
        CHECK(G(s)->n_eval > 0u);
        CHECK_EQ(G(s)->n_changed, 0u);
        CHECK_EQ(G(s)->n_demote, 0u);
      }
      thvm_atp_free(s);
    }
  }

  TEST_BEGIN("atp/guide/zero-exact-run");
  {
    // Whole runs: every selection, the status and the end counters of a
    // zero-correction twin equal its unguided run's.  Every run starts at
    // the same heap cell: the persistent LPO memo is keyed on cell
    // addresses, so two runs at different offsets could differ by
    // themselves.
    u64 hc0 = thvm_atp_heap_checkpoint();
    u64 gc0 = gc_count();
    for (u32 which = 0; which < N_PROBLEMS; which++) {
      if (gc_count() == gc0) {
        thvm_atp_heap_reset(hc0);
      }
      AtpState *s = problem(which);
      problem_load(s, which);
      run_traj(s, 600u, &T_BASE);
      thvm_atp_free(s);
      CHECK(T_BASE.n > 3u);
      for (u32 cfg = 0; cfg < 5u; cfg++) {
        if (gc_count() == gc0) {
          thvm_atp_heap_reset(hc0);
        }
        s = problem(which);
        switch (cfg) {
          case 0u: CHECK_EQ(thvm_atp_guide_arm(s, "twee", NULL, 1u), 1); break;
          case 1u: CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 8u, 0.0, 0u, 1u)); break;
          case 2u: CHECK(arm(s, &H_RAND, ATP_GUIDE_LAZY, 8u, 0.0, 0u, 1u)); break;
          case 3u: CHECK(arm(s, &H_TOPK18, ATP_GUIDE_TOPK, 64u, 0.0, 3u, 1u)); break;
          default: CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 64u, 1.0, 2u, 1u)); break;
        }
        problem_load(s, which);
        run_traj(s, 600u, &T_ARM);
        CHECK(traj_eq(&T_BASE, &T_ARM));
        CHECK(G(s)->n_scored > 0u);
        CHECK(G(s)->n_feat > 0u);
        CHECK_EQ(G(s)->n_changed, 0u);
        CHECK_EQ(G(s)->n_demote, 0u);
        CHECK_EQ(G(s)->n_dirty, 0u);
        thvm_atp_free(s);
      }
    }
  }

  TEST_BEGIN("atp/guide/forward-parity");
  {
    // phi set after arming reaches the first scored pick.
    AtpGuidePhi phi;
    memset(&phi, 0, sizeof phi);
    phi.n = 2u;
    phi.v[0] = 0.75f;
    phi.v[1] = -1.5f;
    AtpState *s = problem(0u);
    CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 16u, 0.0, 0u, 0u));
    thvm_atp_guide_set_phi(&phi);
    problem_load(s, 0u);
    for (u32 k = 0; k < 40u && thvm_atp_step(s) == ATP_RUNNING; k++) {
    }
    AtpGuide *g = G(s);
    s->rss_guard_cap    = 1000;
    s->rss_guard_growth = 300;
    if (s->n_cps >= 2u) {
      u32 pos = atp_guide(s);
      u32 n = (s->n_cps < 16u) ? s->n_cps : 16u;
      CHECK_EQ(g->n_cand, n);
      const AtpGuideModel *m = &H_RAND.m;
      u32 best = 0u;
      for (u32 i = 0; i < n; i++) {
        const double *x = g->x[i];
        CHECK(rel(g->cand[i].corr, ref_forward(m, x)) < 1e-9);
        CHECK(rel(g->cand[i].score, -(double)m->kappa * x[0] + g->cand[i].corr) < 1e-12);
        CHECK(x[0] == log1p((double)i));
        CHECK(x[17] == log1p((double)s->n_cps));
        u32 w = (g->n_bounds < 64u) ? g->n_bounds : 64u;
        double grow = w ? (double)(s->cp_seq_next - g->bounds[g->n_bounds - w]) / (double)w : 0.0;
        CHECK(x[18] == log1p(grow));
        CHECK(x[19] == 19.0 / 64.0);
        CHECK(x[20] == 0.75);
        CHECK(x[21] == -1.5);
        CHECK(x[22] == (double)H_RAND.mu[22]);
        for (u32 j = 0; j < 4u; j++) {
          CHECK(x[23u + j] == ((j == 2u) ? 1.0 : 0.0));
        }
        CHECK(x[16] == (g->cand[i].ft.has ? 0.0 : 1.0));
        CHECK(x[12] >= log1p(1.0));
        if (g->cand[i].score > g->cand[best].score) {
          best = i;
        }
      }
      CHECK_EQ(pos, g->cand[best].pos);
      // phi set anew is folded in at the next pick.
      phi.v[0] = 2.0f;
      thvm_atp_guide_set_phi(&phi);
      (void)atp_guide(s);
      CHECK(g->x[0][20] == 2.0);
      CHECK(rel(g->cand[0].corr, ref_forward(m, g->x[0])) < 1e-9);
    }
    thvm_atp_guide_set_phi(NULL);
    thvm_atp_free(s);

    // topk18: the inputs are topk.c's, the push features the engine's.
    s = problem(0u);
    CHECK(arm(s, &H_TOPK18, ATP_GUIDE_TOPK, 16u, 0.0, 0u, 0u));
    problem_load(s, 0u);
    for (u32 k = 0; k < 30u && thvm_atp_step(s) == ATP_RUNNING; k++) {
    }
    g = G(s);
    if (s->n_cps >= 2u) {
      (void)atp_guide(s);
      u32 n = (s->n_cps < 16u) ? s->n_cps : 16u;
      for (u32 i = 0; i < n; i++) {
        const AtpGuideCand *c = &g->cand[i];
        const double *x = g->x[i];
        CHECK(rel(c->corr, ref_forward(&H_TOPK18.m, x)) < 1e-9);
        CHECK(x[0] == log1p((double)i));
        CHECK(x[1] == log1p((double)s->n_cps));
        CHECK(x[2] == log1p((double)s->cp_pri[c->row]));
        if (!c->ft.has) {
          continue;
        }
        Term l = 0, r = 0;
        atp_cp_slot_read(s, c->row, &l, &r);
        AtpGuideGoal *gv = atp_guide_goal_at(g, c->seq);
        float feat[ATP_CP_FEATURE_DIM];
        atp_cp_features_vs(s, l, r, c->seq, gv != NULL ? gv->l : 0, gv != NULL ? gv->r : 0, feat);
        for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) {
          CHECK((float)x[3u + j] == feat[j]);
        }
      }
    }
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/scratch-neutral");
  {
    // A pick that decodes pops its scratch, and in a KBO or LPO lane it
    // moves no memo epoch and no LPO statistic.
    for (u32 which = 0; which < 3u; which += 2u) {
      AtpState *s = problem(which);
      CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 16u, 0.0, 0u, 0u));
      problem_load(s, which);
      for (u32 k = 0; k < 25u && thvm_atp_step(s) == ATP_RUNNING; k++) {
      }
      AtpGuide *g = G(s);
      if (s->n_cps >= 2u) {
        g->cache_epoch++;
        u64 f0 = g->n_feat;
        u64 h0 = HEAP_NEXT;
        u32 ke = g_kbo_epoch, le = g_lpo_epoch;
        u64 a0 = 0, b0 = 0, c0 = 0, d0 = 0, a1 = 0, b1 = 0, c1 = 0, d1 = 0;
        thvm_lpo_flat_stats(&a0, &b0, &c0, &d0);
        (void)atp_guide(s);
        thvm_lpo_flat_stats(&a1, &b1, &c1, &d1);
        CHECK(g->n_feat > f0);
        CHECK_EQ(HEAP_NEXT, h0);
        CHECK_EQ(g_kbo_epoch, ke);
        CHECK_EQ(g_lpo_epoch, le);
        CHECK(a0 == a1 && b0 == b1 && c0 == c1 && d0 == d1);
        CHECK_EQ(g->n_dirty, 0u);
      }
      thvm_atp_free(s);
    }
  }

  TEST_BEGIN("atp/guide/normalization");
  {
    // Zero correction keeps the engine's picks: roots of pri 2, 3, 4, 5.
    AtpState *s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 4u, 0.0, 0u, 1u));
    rank_queue(s);
    double sum = 0.0;
    for (u32 k = 0; k < 4u; k++) {
      CHECK_EQ(s->cp_heap[0].pri, 2u + k);
      sum += log1p((double)(2u + k));
      (void)pop_seq(s);
      CHECK_EQ(G(s)->norm_n[0], k + 1u);
      CHECK(rel(G(s)->norm_m[0], sum / (double)(k + 1u)) < 1e-12);
    }
    (void)atp_guide(s);
    sum += log1p(6.0);
    CHECK(rel(G(s)->x[0][1], log1p(6.0) - sum / 5.0) < 1e-12);
    CHECK(rel(G(s)->x[1][1], log1p(7.0) - sum / 5.0) < 1e-12);
    thvm_atp_free(s);

    // The first scored pick puts its first candidate at 0.
    s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 4u, 0.0, 0u, 0u));
    rank_queue(s);
    (void)atp_guide(s);
    for (u32 i = 0; i < ATP_GUIDE_NNORM; i++) {
      CHECK(G(s)->x[0][ATP_GUIDE_NORM_COL[i]] == 0.0);
    }
    CHECK(G(s)->x[1][1] == log1p(3.0) - log1p(2.0));
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/lazy");
  {
    // k 8: the six b-CPs are demoted, f(a, a) = a is accepted.
    AtpState *s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 8u, 0.5, 0u, 0u));
    lazy_queue(s);
    AtpGuide *g = G(s);
    CHECK_EQ(pop_seq(s), 6u);
    CHECK_EQ(g->n_eval, 7u);
    CHECK_EQ(g->n_demote, 6u);
    CHECK_EQ(g->n_accept, 1u);
    CHECK_EQ(g->n_visit, 7u);
    // Only the pri-4 CP is fresh: rejected, demoted, taken as the best
    // scored.
    CHECK_EQ(pop_seq(s), 7u);
    CHECK_EQ(g->n_eval, 8u);
    CHECK_EQ(g->n_demote, 7u);
    CHECK_EQ(g->n_fallback, 1u);
    CHECK_EQ(g->n_visit, 14u);
    // Every queued CP is demoted: blocked, the root.
    CHECK_EQ(pop_seq(s), 0u);
    CHECK_EQ(g->n_blocked, 1u);
    CHECK_EQ(g->n_eval, 8u);
    CHECK_EQ(g->n_visit, 20u);
    CHECK_EQ(g->n_scored, 3u);
    CHECK_EQ(g->n_changed, 2u);
    thvm_atp_free(s);

    // k 3: three evaluations per pick, then the best scored.
    s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 3u, 0.5, 0u, 0u));
    lazy_queue(s);
    g = G(s);
    static const u32 want3[5] = {0u, 3u, 6u, 7u, 1u};
    for (u32 k = 0; k < 5u; k++) {
      CHECK_EQ(pop_seq(s), want3[k]);
    }
    CHECK_EQ(g->n_fallback, 3u);
    CHECK_EQ(g->n_accept, 1u);
    CHECK_EQ(g->n_blocked, 1u);
    CHECK_EQ(g->n_demote, 7u);
    CHECK(g->n_eval <= 3u * g->n_scored);
    thvm_atp_free(s);

    // tau 2 accepts every candidate: the engine's order.
    s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 8u, 2.0, 0u, 0u));
    lazy_queue(s);
    for (u32 k = 0; k < 8u; k++) {
      CHECK_EQ(pop_seq(s), k);
    }
    CHECK_EQ(G(s)->n_demote, 0u);
    CHECK_EQ(G(s)->n_eval, G(s)->n_scored);
    thvm_atp_free(s);

    // --guide-zero under the same head: the engine's order.
    s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 8u, 0.5, 0u, 1u));
    lazy_queue(s);
    for (u32 k = 0; k < 8u; k++) {
      CHECK_EQ(pop_seq(s), k);
    }
    CHECK_EQ(G(s)->n_demote, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/coop");
  {
    // Top-K 4 prefers rank 3; every 3rd call returns the root unscored,
    // through the layer's own counter (no w2 pick).
    AtpState *s = thvm_atp_init(&CFG, 100u);
    CHECK(arm(s, &H_RANK, ATP_GUIDE_TOPK, 4u, 0.0, 3u, 0u));
    rank_queue(s);
    AtpGuide *g = G(s);
    u32 p5 = pos_of_pri(s, 5u);
    CHECK(p5 != 0u && p5 != 0xffffffffu);
    for (u32 call = 1; call <= 9u; call++) {
      u64 ev0 = g->n_eval;
      u32 pos = atp_guide(s);
      if (call % 3u == 0u) {
        CHECK_EQ(pos, 0u);
        CHECK_EQ(g->n_eval, ev0);
      } else {
        CHECK_EQ(pos, p5);
        CHECK_EQ(g->n_eval, ev0 + 4u);
      }
    }
    CHECK_EQ(g->n_pick, 9u);
    CHECK_EQ(g->n_coop, 3u);
    CHECK_EQ(g->n_scored, 6u);
    CHECK_EQ(g->n_changed, 6u);
    CHECK_EQ(s->w2_modulo, 0u);
    CHECK_EQ(s->n_cps_w2_picks, 0u);
    // Through select_cp: calls 10, 11 pop pri 5 and pri 6 (seq 3, 4),
    // call 12 the root (pri 2, seq 0).
    CHECK_EQ(pop_seq(s), 3u);
    CHECK_EQ(pop_seq(s), 4u);
    CHECK_EQ(pop_seq(s), 0u);
    CHECK_EQ(g->n_coop, 4u);
    CHECK_EQ(g->n_bounds, 3u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/lrs-side-key");
  {
    // An --lrs state: demotion writes no key LRS reads (cp_pri, cp_seq,
    // the heap nodes), and the side key follows its CP across the row
    // moves of the pops.
    AtpState *s = thvm_atp_init(&CFG, 100u);
    s->use_lrs = 1u;
    CHECK(arm(s, &H_LAZY, ATP_GUIDE_LAZY, 8u, 0.5, 0u, 0u));
    lazy_queue(s);
    static u32 pri0[8], seq0[8];
    static AtpCpHeapNode heap0[8];
    u32 n0 = s->n_cps;
    memcpy(pri0, s->cp_pri, n0 * sizeof(u32));
    memcpy(seq0, s->cp_seq, n0 * sizeof(u32));
    memcpy(heap0, s->cp_heap, n0 * sizeof(AtpCpHeapNode));
    u32 p6 = 0xffffffffu;
    for (u32 p = 0; p < n0; p++) {
      if (s->cp_seq[s->cp_heap[p].row] == 6u) {
        p6 = p;
      }
    }
    CHECK_EQ(atp_guide(s), p6);
    CHECK_EQ(atp_guide(s), p6);
    CHECK_EQ(G(s)->n_demote, 6u);
    CHECK_EQ(s->n_cps, n0);
    CHECK(memcmp(pri0, s->cp_pri, n0 * sizeof(u32)) == 0);
    CHECK(memcmp(seq0, s->cp_seq, n0 * sizeof(u32)) == 0);
    CHECK(memcmp(heap0, s->cp_heap, n0 * sizeof(AtpCpHeapNode)) == 0);
    static const u32 dem6[6] = {0u, 1u, 2u, 3u, 4u, 5u};
    CHECK(dem_matches(s, dem6, 6u));
    CHECK_EQ(pop_seq(s), 6u);
    CHECK(dem_matches(s, dem6, 6u));
    CHECK_EQ(pop_seq(s), 7u);
    CHECK(dem_matches(s, dem6, 6u));
    CHECK_EQ(pop_seq(s), 0u);
    CHECK(dem_matches(s, dem6 + 1, 5u));
    CHECK_EQ(pop_seq(s), 1u);
    CHECK(dem_matches(s, dem6 + 2, 4u));
    CHECK_EQ(s->lrs_horizon, 0u);
    CHECK_EQ(s->n_cps_dropped_lrs, 0u);
    thvm_atp_free(s);
  }

  TEST_BEGIN("atp/guide/cost-counters");
  {
    FILE *tf = tmpfile();
    CHECK(tf != NULL);
    for (u32 mode = 0; mode < 2u; mode++) {
      AtpState *s = problem(0u);
      if (mode == 0u) {
        CHECK(arm(s, &H_RAND, ATP_GUIDE_LAZY, 4u, 0.0, 5u, 0u));
      } else {
        CHECK(arm(s, &H_RAND, ATP_GUIDE_TOPK, 12u, 0.0, 4u, 0u));
      }
      problem_load(s, 0u);
      Traj *t = &T_ARM;
      run_traj(s, 300u, t);
      AtpGuide *g = G(s);
      CHECK(g->n_pick > 0u);
      CHECK_EQ(g->n_pick, g->n_trivial + g->n_coop + g->n_scored);
      CHECK(g->n_coop <= g->n_pick / 4u);
      if (mode == 0u) {
        CHECK_EQ(g->n_scored, g->n_accept + g->n_fallback + g->n_blocked);
        CHECK(g->n_demote <= g->n_eval);
        CHECK(g->n_eval <= 4u * g->n_scored);
      } else {
        CHECK_EQ(g->n_demote, 0u);
        CHECK(g->n_eval <= 12u * g->n_scored);
        CHECK(g->n_eval >= g->n_scored);
      }
      CHECK(g->n_feat + g->n_nofeat <= g->n_eval);
      CHECK(g->n_changed <= g->n_scored);
      CHECK(g->n_visit >= g->n_eval);
      CHECK(g->n_dirty <= g->n_scored);
      CHECK(g->t_max_ns <= g->t_ns);
      CHECK(g->n_scored == 0u || g->t_ns > 0u);
      CHECK(g->n_bounds == s->cp_select_count);
      if (tf != NULL) {
        rewind(tf);
        thvm_atp_guide_print_stats(s, tf);
        fflush(tf);
        rewind(tf);
        char line[1024];
        CHECK(fgets(line, sizeof line, tf) != NULL);
        char want[96];
        CHECK(strncmp(line, "% guide: model=test family=test mode=", 37) == 0);
        snprintf(want, sizeof want, " picks=%llu ", (unsigned long long)g->n_pick);
        CHECK(strstr(line, want) != NULL);
        snprintf(want, sizeof want, " scored=%llu ", (unsigned long long)g->n_scored);
        CHECK(strstr(line, want) != NULL);
        snprintf(want, sizeof want, " evals=%llu ", (unsigned long long)g->n_eval);
        CHECK(strstr(line, want) != NULL);
        snprintf(want, sizeof want, " dirty=%llu ", (unsigned long long)g->n_dirty);
        CHECK(strstr(line, want) != NULL);
        CHECK(strstr(line, " us_per_scored=") != NULL);
      }
      thvm_atp_free(s);
    }
    if (tf != NULL) {
      // Unarmed: no line.
      AtpState *s = thvm_atp_init(&CFG, 100u);
      FILE *uf = tmpfile();
      CHECK(uf != NULL);
      if (uf != NULL) {
        thvm_atp_guide_print_stats(s, uf);
        CHECK_EQ(ftell(uf), 0);
        fclose(uf);
      }
      thvm_atp_free(s);
      fclose(tf);
    }
  }

  TEST_BEGIN("atp/guide/dump");
  {
    char p[512];
    tmp_template(p, sizeof p, "thvm_guide_dump");
    int fd = mkstemp(p);
    CHECK(fd >= 0);
    if (fd >= 0) {
      close(fd);
      setenv("THVM_ATP_GUIDE_DUMP", p, 1);
      setenv("THVM_ATP_GUIDE_DUMP_MAX", "2", 1);
      AtpState *s = thvm_atp_init(&CFG, 100u);
      CHECK(arm(s, &H_RANK, ATP_GUIDE_TOPK, 4u, 0.0, 0u, 0u));
      unsetenv("THVM_ATP_GUIDE_DUMP");
      unsetenv("THVM_ATP_GUIDE_DUMP_MAX");
      rank_queue(s);
      for (u32 k = 0; k < 3u; k++) {
        (void)atp_guide(s);
      }
      thvm_atp_free(s);
      FILE *f = fopen(p, "r");
      CHECK(f != NULL);
      if (f != NULL) {
        static char line[16384];
        u32 rows = 0u, chosen = 0u, header = 0u;
        while (fgets(line, sizeof line, f) != NULL) {
          if (line[0] == '#') {
            header++;
            CHECK(strncmp(line, "# guide model=test layout=v1 n_x=20 ", 36) == 0);
            continue;
          }
          u32 fields = 1u;
          for (char *c = line; *c != '\0'; c++) {
            if (*c == '\t') {
              fields++;
            }
          }
          CHECK_EQ(fields, 4u + 20u + 2u);
          u32 t = 0u, i = 0u, seq = 0u, ch = 0u;
          CHECK_EQ(sscanf(line, "%u\t%u\t%u\t%u", &t, &i, &seq, &ch), 4);
          CHECK_EQ(i, rows % 4u);
          CHECK_EQ(ch, (i == 3u) ? 1u : 0u);
          chosen += ch;
          rows++;
        }
        CHECK_EQ(header, 1u);
        CHECK_EQ(rows, 8u);
        CHECK_EQ(chosen, 2u);
        fclose(f);
      }
      unlink(p);
    }
  }

  TEST_BEGIN("atp/guide/env");
  {
    // thvm_atp_init arms from the environment, read once per process.
    setenv("THVM_ATP_GUIDE", "twee", 1);
    setenv("THVM_ATP_GUIDE_ZERO", "1", 1);
    setenv("THVM_ATP_GUIDE_MODE", "lazy:4:0.5", 1);
    setenv("THVM_ATP_GUIDE_COOP", "5", 1);
    g_atp_guide_env = -1;
    for (u32 rep = 0; rep < 2u; rep++) {
      AtpState *s = thvm_atp_init(&CFG, 100u);
      CHECK(s->guide != NULL);
      if (s->guide != NULL) {
        AtpGuide *g = G(s);
        CHECK(g->m == atp_guide_model(NULL));
        CHECK_EQ(g->fam.mode, ATP_GUIDE_LAZY);
        CHECK_EQ(g->fam.k, 4u);
        CHECK(g->fam.tau == 0.5);
        CHECK_EQ(g->fam.coop, 5u);
        CHECK_EQ(g->zero, 1u);
        CHECK(strcmp(g->family, "twee") == 0);
        CHECK_EQ(g->n_gv, 1u);
        CHECK_EQ(g->gv[0].seq, 0u);
      }
      thvm_atp_free(s);
    }

    setenv("THVM_ATP_GUIDE_MODE", "lazy:65", 1);
    g_atp_guide_env = -1;
    AtpState *s = thvm_atp_init(&CFG, 100u);
    CHECK(s->guide == NULL);
    CHECK_EQ(g_atp_guide_env, 0);
    thvm_atp_free(s);

    unsetenv("THVM_ATP_GUIDE_MODE");
    setenv("THVM_ATP_GUIDE_MODEL", "no-such-head", 1);
    g_atp_guide_env = -1;
    s = thvm_atp_init(&CFG, 100u);
    CHECK(s->guide == NULL);
    thvm_atp_free(s);

    unsetenv("THVM_ATP_GUIDE");
    unsetenv("THVM_ATP_GUIDE_ZERO");
    unsetenv("THVM_ATP_GUIDE_COOP");
    unsetenv("THVM_ATP_GUIDE_MODEL");
    g_atp_guide_env = -1;
    s = thvm_atp_init(&CFG, 100u);
    thvm_atp_add_equation(s, mk_f(mk_a(), mk_e()), mk_a());
    CHECK(s->guide == NULL);
    CHECK_EQ(g_atp_guide_env, 0);
    thvm_atp_free(s);
  }

  TEST_REPORT();
}
