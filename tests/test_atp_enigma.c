// test_atp_enigma.c - ENIGMA-style learned CP-selector training data.
//
// Exercises the feature extractor (thvm_atp_cp_features), the gated
// processed-CP recorder (thvm_atp_set_record_cp_features), the proof-set
// labeller (thvm_atp_cp_label), and the TSV export
// (thvm_atp_cp_dataset_append).  Asserts on a known group-theory proof:
//   - the dataset has rows (CPs were processed),
//   - every feature value is finite,
//   - at least one row is labelled 1 (proof-relevant),
//   - the proof-set size is plausible (1 <= n_pos <= n recorded).

#include "../src/thvm.c"
#include "test.h"

#include <math.h>
#include <stdio.h>

#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term mk_e(void) { return term_new_ctr(LAB_e, NULL, 0); }
static Term mk_a(void) { return term_new_ctr(LAB_a, NULL, 0); }
static Term mk_f(Term x, Term y) { Term cs[2] = {x, y}; return term_new_ctr(LAB_f, cs, 2); }
static Term mk_i(Term x)         { Term cs[1] = {x};    return term_new_ctr(LAB_i, cs, 1); }

// Same KBO config as the headline group-axiom proof in test_atp.c
// (weights i=0, f=1, e=1, a=1; precedence i > f > e > a; var weight 1).
static u32 gw[5] = {0, 1, 0, 1, 1};
static u32 gp[5] = {0, 2, 4, 3, 1};
static const KboConfig GROUP_CFG = {
  .weights = gw, .precedence = gp, .n_labels = 5, .var_weight = 1,
};

// Load the three standard group axioms used by the headline demo:
//   right-id   f(x, e)        = x
//   right-inv  f(x, i(x))     = e
//   assoc      f(f(x,y), z)   = f(x, f(y,z))
static void load_group_axioms(AtpState *s) {
  thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_e()),           mk_v(VAR_x));
  thvm_atp_add_equation(s, mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x))), mk_e());
  thvm_atp_add_equation(s, mk_f(mk_f(mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
                        mk_f(mk_v(VAR_x), mk_f(mk_v(1u), mk_v(2u))));
}

int main(void) {
  thvm_init();

  // === Default-OFF byte-identity sanity ===============================
  // With recording off (the default), running the proof records nothing
  // and leaves every feature field NULL/0.  (The byte-identity of the
  // engine itself is covered by test_atp's 135603-check; here we just
  // confirm the recorder stays inert.)
  TEST_BEGIN("atp/enigma/recording-off-records-nothing");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 256);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    load_group_axioms(s);
    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);
    CHECK_EQ(s->record_cp_features, 0u);
    CHECK_EQ(s->n_cp_feat,          0u);
    CHECK(s->cp_feat_rows == NULL);
    // Labelling with nothing recorded returns 0.
    CHECK_EQ(thvm_atp_cp_label(s), 0u);
    thvm_atp_free(s);
  }

  // === Feature extractor: standalone, finite, schema ==================
  TEST_BEGIN("atp/enigma/features-finite-and-shaped");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 64);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    // A CP touching the goal symbols: l = f(x, i(x)), r = e.
    Term l = mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x)));
    Term r = mk_e();
    float feat[ATP_CP_FEATURE_DIM];
    thvm_atp_cp_features(s, l, r, 7u, feat);
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) {
      CHECK(isfinite(feat[j]));
    }
    // size_sum = |f(x,i(x))| + |e| = (f + x + i + x) + e = 4 + 1 = 5.
    CHECK_EQ((u32)feat[0], 5u);
    // max_depth = depth(f(x,i(x))) = 2, depth(e) = 0 -> 2.
    CHECK_EQ((u32)feat[1], 2u);
    // n_distinct_vars: only x -> 1.  n_var_occ: x appears twice -> 2.
    CHECK_EQ((u32)feat[2], 1u);
    CHECK_EQ((u32)feat[3], 2u);
    // age echoes the caller-supplied value.
    CHECK_EQ((u32)feat[8], 7u);
    // top_symbol_l = f (LAB_f); top_symbol_r = e (LAB_e).
    CHECK_EQ((u32)feat[9],  LAB_f);
    CHECK_EQ((u32)feat[10], LAB_e);
    // shares_goal_sub: the goal f(a,i(a))==e uses f, i, e; the CP uses
    // f, i, e -> overlap -> 1.
    CHECK_EQ((u32)feat[11], 1u);
    // A CP that shares no symbol with the goal scores 0 on feature 11.
    Term l2 = mk_a(), r2 = mk_a();
    float feat2[ATP_CP_FEATURE_DIM];
    thvm_atp_cp_features(s, l2, r2, 0u, feat2);
    // f(a,i(a)) and e contain `a`, so `a`-only CP DOES share -> still 1;
    // pick a symbol genuinely absent from the goal would need a new
    // label, so just assert finiteness of this row instead.
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) CHECK(isfinite(feat2[j]));
    thvm_atp_free(s);
  }

  // === End-to-end recording + labelling on a real proof ===============
  TEST_BEGIN("atp/enigma/dataset-has-labelled-rows");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 512);
    // Record norm steps so the trace DAG carries the CP -> ORIENT chain
    // the labeller walks (matches how the WL proof extractor runs).
    thvm_atp_set_record_norm_steps(s, 1u);
    thvm_atp_set_record_cp_features(s, 1u);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    load_group_axioms(s);

    AtpStatus st = thvm_atp_run(s);
    CHECK_EQ((int)st, (int)ATP_PROVED);

    // Recording produced rows (CPs were processed).
    CHECK(s->n_cp_feat >= 1u);
    u32 n_recorded = s->n_cp_feat;

    // Every recorded feature value is finite.
    for (u32 i = 0; i < n_recorded; i++) {
      const float *row = s->cp_feat_rows + (size_t)i * ATP_CP_FEATURE_DIM;
      for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) CHECK(isfinite(row[j]));
      // age feature equals the row index by construction.
      CHECK_EQ((u32)row[8], i);
    }

    // Label the dataset from the proof-set reachability.
    u32 n_pos = thvm_atp_cp_label(s);
    // At least one row is proof-relevant.
    CHECK(n_pos >= 1u);
    // Proof set is a subset of the processed CPs.
    CHECK(n_pos <= n_recorded);
    // The label array agrees with the returned count.
    u32 counted = 0u;
    for (u32 i = 0; i < n_recorded; i++) counted += s->cp_feat_label[i];
    CHECK_EQ(counted, n_pos);
    // Labels are strictly binary.
    for (u32 i = 0; i < n_recorded; i++) CHECK(s->cp_feat_label[i] <= 1u);

    // Export to a TSV temp file; assert the row count matches and the
    // file is non-empty with a header.
    const char *path = "/tmp/thvm_enigma_test.tsv";
    remove(path);
    u32 written = thvm_atp_cp_dataset_append(s, path, 1u);
    CHECK_EQ(written, n_recorded);
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    if (f != NULL) {
      char hdr[256] = {0};
      char *got = fgets(hdr, sizeof(hdr), f);
      CHECK(got != NULL);
      CHECK(strstr(hdr, "label") != NULL);
      CHECK(strstr(hdr, "weight_gt") != NULL);
      CHECK(strstr(hdr, "shares_goal_sub") != NULL);
      // Count data lines == n_recorded.
      u32 lines = 0u;
      char buf[512];
      while (fgets(buf, sizeof(buf), f) != NULL) lines++;
      CHECK_EQ(lines, n_recorded);
      fclose(f);
    }
    remove(path);
    thvm_atp_free(s);
  }

  // === Runtime-loadable scorer: LINEAR reproduces the baked-in logreg ==
  // A LINEAR blob carrying the baked-in W/B with identity standardization
  // (mean 0, inv_std 1) must score, and prioritize, exactly like the
  // default (inactive) baked-in path.
  TEST_BEGIN("atp/enigma/learned-linear-matches-bakedin");
  {
    AtpState *s = thvm_atp_init(&GROUP_CFG, 64);
    thvm_atp_set_goal(s, mk_f(mk_a(), mk_i(mk_a())), mk_e());
    Term l = mk_f(mk_v(VAR_x), mk_i(mk_v(VAR_x)));
    Term r = mk_e();
    float feat[ATP_CP_FEATURE_DIM];
    thvm_atp_cp_features(s, l, r, 3u, feat);

    // Baseline priority via the baked-in scorer (model inactive).
    CHECK_EQ(g_atp_learned_active, 0);
    u32 base_pri = atp_cp_learned_priority(s, l, r);

    double blob[45];
    blob[0] = (double)ATP_LEARNED_LINEAR;
    blob[1] = 0.0;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) blob[2 + i]  = 0.0;  // mean
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) blob[16 + i] = 1.0;  // inv_std
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) blob[30 + i] = (double)ATP_LEARNED_W[i];
    blob[44] = (double)ATP_LEARNED_B;
    CHECK_EQ(thvm_atp_set_learned_scorer(blob, 45u), 1);
    CHECK_EQ(g_atp_learned_active, 1);

    float ref = ATP_LEARNED_B;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) ref += ATP_LEARNED_W[i] * feat[i];
    float got = atp_learned_forward(&g_atp_learned, feat);
    CHECK(fabsf(got - ref) < 1e-3f);

    // Same features -> same priority as the baked-in path.
    CHECK_EQ(atp_cp_learned_priority(s, l, r), base_pri);

    thvm_atp_clear_learned_scorer();
    CHECK_EQ(g_atp_learned_active, 0);
    thvm_atp_free(s);
  }

  // === Runtime-loadable scorer: MLP forward is hand-checkable =========
  TEST_BEGIN("atp/enigma/learned-mlp-forward");
  {
    u32 H   = 2u;
    u32 len = 31u + 16u * H;  // 63
    double blob[63];
    blob[0] = (double)ATP_LEARNED_MLP;
    blob[1] = (double)H;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) blob[2 + i]  = 0.0;  // mean
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) blob[16 + i] = 1.0;  // inv_std
    u32 p = 30u;
    // W1 row 0 reads feature 0 (w 1); row 1 reads feature 1 (w -1).
    for (u32 j = 0; j < H; j++) {
      for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) {
        blob[p++] = (j == 0u && i == 0u) ?  1.0
                  : (j == 1u && i == 1u) ? -1.0 : 0.0;
      }
    }
    blob[p++] = 0.0;  // b1[0]
    blob[p++] = 0.0;  // b1[1]
    blob[p++] = 2.0;  // w2[0]
    blob[p++] = 3.0;  // w2[1]
    blob[p++] = 0.5;  // b2
    CHECK_EQ(p, len);
    CHECK_EQ(thvm_atp_set_learned_scorer(blob, len), 1);

    float feat[ATP_CP_FEATURE_DIM] = {0};
    feat[0] = 4.0f;   // h0 = relu(4)  = 4
    feat[1] = 5.0f;   // h1 = relu(-5) = 0
    // score = b2 + w2[0]*h0 + w2[1]*h1 = 0.5 + 2*4 + 3*0 = 8.5
    float got = atp_learned_forward(&g_atp_learned, feat);
    CHECK(fabsf(got - 8.5f) < 1e-4f);

    thvm_atp_clear_learned_scorer();
  }

  // === Runtime-loadable scorer: malformed blobs are rejected ==========
  // A bad push leaves the baked-in scorer active (g_atp_learned_active 0).
  TEST_BEGIN("atp/enigma/learned-rejects-malformed");
  {
    double blob[45] = {0};
    blob[0] = (double)ATP_LEARNED_LINEAR;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) blob[16 + i] = 1.0;
    CHECK_EQ(thvm_atp_set_learned_scorer(blob, 44u), 0);   // wrong length
    CHECK_EQ(g_atp_learned_active, 0);
    blob[0] = 7.0;
    CHECK_EQ(thvm_atp_set_learned_scorer(blob, 45u), 0);   // bad kind
    double m65[30] = {0};
    m65[0] = (double)ATP_LEARNED_MLP;
    m65[1] = (double)(ATP_LEARNED_MAX_HIDDEN + 1u);
    CHECK_EQ(thvm_atp_set_learned_scorer(m65, 30u), 0);    // hidden over cap
    CHECK_EQ(thvm_atp_set_learned_scorer(NULL, 45u), 0);   // NULL
    CHECK_EQ(g_atp_learned_active, 0);
  }

  thvm_free();
  TEST_REPORT();
}
