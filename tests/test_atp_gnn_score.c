// test_atp_gnn_score.c - ENIGMA Tier 2 in-engine GCN scorer (M2 anchor).
//
// thvm_atp_gnn_score_batch runs the GCN forward on thvm's OWN tensor
// runtime: it builds the row-normalised-adjacency message passing,
// masked-mean pool, and two-class readout as a UOP graph, thvm_realize's
// it, and reads back the per-graph score logit_pos - logit_neg.
//
// The headline DIFFERENTIAL: an independent plain-C reference forward
// (direct nested loops over the SAME math, only here, for cross-check)
// must match the thvm-runtime path within ~1e-3.  Agreement proves the
// UOP graph computes the spec correctly; it is the whole point of M2.
//
// The same forward is what the WL TAtpGnnScore runs (atpGnnForwardLogits
// in wl/THVMLink/Kernel/ATP/ATP.wl), so this also pins C-via-thvm ==
// the WL-trained forward.

#include "../src/thvm.c"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Hand-built CP labels + a variable id.
#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term ctr0(u32 l)                 { return term_new_ctr(l, NULL, 0); }
static Term ctr1(u32 l, Term a)         { Term cs[1] = {a};    return term_new_ctr(l, cs, 1); }
static Term ctr2(u32 l, Term a, Term b) { Term cs[2] = {a, b}; return term_new_ctr(l, cs, 2); }

// A tiny known model: R = 2 rounds, H = 3 hidden, in_0 = 6 (ATP_CPG_FEAT_DIM),
// in_1 = 3.  Weights are arbitrary fixed values (deterministic, spread
// positive + negative so ReLU actually clips).  Built as the on-wire f64
// blob the engine parses.
#define R 2u
#define H 3u
#define F ATP_CPG_FEAT_DIM   // 6

// Deterministic pseudo-weight: a smooth function of a running index, in
// [-0.5, 0.5], so every parameter is distinct and the forward is
// non-degenerate.
static double w_at(u32 *idx) {
  u32 k = (*idx)++;
  return 0.5 * sin(0.37 * (double)k + 0.11);
}

// === Independent plain-C reference forward (cross-check ONLY) =========
// Mirrors atpGnnForwardLogits with naive nested loops over host arrays.
// Returns score = logit[1] - logit[0] for one graph.
static float ref_score(const float *x,        // {n, F} padded to {N,F}
                        const float *adj,      // {N, N} row-normalised
                        const float *mask,     // {N}
                        float nnodes,          // node count (float)
                        u32 N,
                        const double *w1[R], const double *ws[R],
                        const double *bh[R],
                        const double *wout, const double *bout) {
  // h starts as x ({N, F}); after round 0 it is {N, H}.
  // We keep a max-width buffer.
  u32 maxdim = (F > H) ? F : H;
  float *h    = (float *)malloc((size_t)N * maxdim * sizeof(float));
  float *hnew = (float *)malloc((size_t)N * maxdim * sizeof(float));
  float *ah   = (float *)malloc((size_t)N * maxdim * sizeof(float));
  memset(h, 0, (size_t)N * maxdim * sizeof(float));
  for (u32 ni = 0; ni < N; ni++)
    for (u32 fj = 0; fj < F; fj++) h[ni * maxdim + fj] = x[ni * F + fj];

  u32 lastDim = F;
  for (u32 r = 0; r < R; r++) {
    u32 inr = lastDim;
    // ah = A . h   ({N,N}.{N,inr} -> {N,inr})
    for (u32 i = 0; i < N; i++) {
      for (u32 c = 0; c < inr; c++) {
        double acc = 0.0;
        for (u32 k = 0; k < N; k++)
          acc += (double)adj[i * N + k] * (double)h[k * maxdim + c];
        ah[i * maxdim + c] = (float)acc;
      }
    }
    // hnew[i][hh] = relu( bh[r][hh]
    //                     + sum_c ah[i][c]*w1[r][c][hh]
    //                     + sum_c h[i][c]*ws[r][c][hh] )
    for (u32 i = 0; i < N; i++) {
      for (u32 hh = 0; hh < H; hh++) {
        double acc = bh[r][hh];
        for (u32 c = 0; c < inr; c++) {
          acc += (double)ah[i * maxdim + c] * w1[r][c * H + hh];
          acc += (double)h[i * maxdim + c]  * ws[r][c * H + hh];
        }
        hnew[i * maxdim + hh] = (acc > 0.0) ? (float)acc : 0.0f;
      }
    }
    float *tmp = h; h = hnew; hnew = tmp;
    lastDim = H;
  }

  // Masked-mean pool: pooled[hh] = (sum_i mask[i]*h[i][hh]) / nnodes
  // then logit[o] = bout[o] + sum_hh pooled[hh]*wout[hh][o].
  double pooled[H];
  for (u32 hh = 0; hh < H; hh++) {
    double acc = 0.0;
    for (u32 i = 0; i < N; i++) acc += (double)mask[i] * (double)h[i * maxdim + hh];
    pooled[hh] = acc / (double)nnodes;
  }
  double logit[2];
  for (u32 o = 0; o < 2; o++) {
    double acc = bout[o];
    for (u32 hh = 0; hh < H; hh++) acc += pooled[hh] * wout[hh * 2 + o];
    logit[o] = acc;
  }
  free(h); free(hnew); free(ah);
  return (float)(logit[1] - logit[0]);
}

// Build the {N,F}, {N,N} row-normalised adjacency, {N} mask, nnodes for
// one CP graph - the exact padding/normalisation atpGnnTensors and
// thvm_atp_gnn_score_batch use, so the reference sees identical inputs.
static void host_inputs(const AtpCpGraph *g, u32 N,
                        float *x, float *adj, float *mask, float *nnodes) {
  memset(x, 0, (size_t)N * F * sizeof(float));
  memset(adj, 0, (size_t)N * N * sizeof(float));
  memset(mask, 0, (size_t)N * sizeof(float));
  *nnodes = (float)g->n_nodes;
  for (u32 ni = 0; ni < g->n_nodes; ni++) {
    mask[ni] = 1.0f;
    for (u32 fj = 0; fj < F; fj++)
      x[ni * F + fj] = g->node_feat[(size_t)ni * ATP_CPG_FEAT_DIM + fj];
  }
  for (u32 e = 0; e < g->n_edges; e++) {
    u32 s = g->edge_src[e], d = g->edge_dst[e];
    adj[s * N + d] = 1.0f;
    adj[d * N + s] = 1.0f;
  }
  for (u32 i = 0; i < g->n_nodes; i++) adj[i * N + i] = 1.0f;
  for (u32 i = 0; i < N; i++) {
    float rs = 0.0f;
    for (u32 j = 0; j < N; j++) rs += adj[i * N + j];
    if (rs > 0.0f) for (u32 j = 0; j < N; j++) adj[i * N + j] /= rs;
  }
}

int main(void) {
  thvm_init();
  int f = 0;

  // === Build the known model blob ====================================
  // Layout (see thvm.h): [R, H], then per round W1(in*H), Ws(in*H), Bh(H),
  // then Wout(H*2), Bout(2).
  u32 blob_len = 2u;
  for (u32 r = 0; r < R; r++) {
    u32 inr = (r == 0u) ? F : H;
    blob_len += 2u * inr * H + H;
  }
  blob_len += H * 2u + 2u;

  double *blob = (double *)malloc(blob_len * sizeof(double));
  // Keep raw pointers into the blob for the reference forward.
  const double *w1[R], *ws[R], *bh[R], *wout, *bout;
  {
    u32 p = 0u, idx = 0u;
    blob[p++] = (double)R;
    blob[p++] = (double)H;
    for (u32 r = 0; r < R; r++) {
      u32 inr = (r == 0u) ? F : H;
      w1[r] = &blob[p]; for (u32 i = 0; i < inr * H; i++) blob[p++] = w_at(&idx);
      ws[r] = &blob[p]; for (u32 i = 0; i < inr * H; i++) blob[p++] = w_at(&idx);
      bh[r] = &blob[p]; for (u32 i = 0; i < H; i++)       blob[p++] = w_at(&idx);
    }
    wout = &blob[p]; for (u32 i = 0; i < H * 2u; i++) blob[p++] = w_at(&idx);
    bout = &blob[p]; for (u32 i = 0; i < 2u; i++)     blob[p++] = w_at(&idx);
  }

  TEST_BEGIN("atp/gnn_score/load-blob");
  CHECK_EQ(thvm_atp_gnn_loaded(), 0);
  CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
  CHECK_EQ(thvm_atp_gnn_loaded(), 1);
  // A malformed (wrong length) blob is rejected, model stays loaded.
  CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len - 1u), 0);
  CHECK_EQ(thvm_atp_gnn_loaded(), 1);

  // === A small batch of hand-built CPs ===============================
  // CP 0: f(x, i(x)) = e            (group inverse axiom)
  // CP 1: f(f(x,x), e) = a          (deeper, different structure)
  // CP 2: i(e) = e                  (tiny)
  Term lhs[3], rhs[3];
  lhs[0] = ctr2(LAB_f, mk_v(VAR_x), ctr1(LAB_i, mk_v(VAR_x)));
  rhs[0] = ctr0(LAB_e);
  lhs[1] = ctr2(LAB_f, ctr2(LAB_f, mk_v(VAR_x), mk_v(VAR_x)), ctr0(LAB_e));
  rhs[1] = ctr0(LAB_a);
  lhs[2] = ctr1(LAB_i, ctr0(LAB_e));
  rhs[2] = ctr0(LAB_e);
  const u32 B = 3u;

  // === Product path: score on the thvm UOP runtime ===================
  float c_scores[3];
  TEST_BEGIN("atp/gnn_score/thvm-runtime-forward");
  CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, B, c_scores), 1);

  // === Reference path: independent plain-C forward ===================
  // Build each graph, find the common padded N (the batch path pads to
  // the max), then run the naive reference at that N so inputs match.
  AtpCpGraph gs[3];
  u32 N = 1u;
  for (u32 i = 0; i < B; i++) {
    CHECK_EQ(thvm_atp_cp_graph(lhs[i], rhs[i], &gs[i]), 1);
    if (gs[i].n_nodes > N) N = gs[i].n_nodes;
  }
  float ref[3];
  for (u32 i = 0; i < B; i++) {
    float *x   = (float *)malloc((size_t)N * F * sizeof(float));
    float *adj = (float *)malloc((size_t)N * N * sizeof(float));
    float *msk = (float *)malloc((size_t)N * sizeof(float));
    float nn;
    host_inputs(&gs[i], N, x, adj, msk, &nn);
    ref[i] = ref_score(x, adj, msk, nn, N, w1, ws, bh, wout, bout);
    free(x); free(adj); free(msk);
  }

  // === Differential: thvm-runtime == reference within 1e-3 ===========
  TEST_BEGIN("atp/gnn_score/differential-thvm-vs-reference");
  for (u32 i = 0; i < B; i++) {
    float d = fabsf(c_scores[i] - ref[i]);
    printf("  CP %u: thvm=% .6f ref=% .6f  |diff|=%.2e\n",
           i, c_scores[i], ref[i], d);
    CHECK(d < 1.0e-3f);
  }

  // === M3: in-engine re-rank permutes the live CP queue =============
  // Build a real CP queue by stepping the group-axiom completion, snapshot
  // the priorities, call thvm_atp_gnn_rerank, and confirm: (a) the queue
  // size is unchanged (re-rank only permutes; completeness safe), (b)
  // every priority lands in the GNN score->priority band, and (c) at
  // least one priority actually changed (the GNN reordered something).
  {
    // Re-load the model (cleared just above is fine; reload here).
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);

    // Same KBO config + axioms as test_atp_rerank's group completion.
    static u32 gw[5] = {0, 1, 0, 1, 1};
    static u32 gp[5] = {0, 2, 4, 3, 1};
    static const KboConfig GROUP_CFG = {
      gw, gp, 5, 1
    };
    AtpState *s = thvm_atp_init(&GROUP_CFG, 256);
    // f(x,e)=x, f(x,i(x))=e, associativity.
    thvm_atp_add_equation(s, ctr2(LAB_f, mk_v(VAR_x), ctr0(LAB_e)), mk_v(VAR_x));
    thvm_atp_add_equation(s, ctr2(LAB_f, mk_v(VAR_x), ctr1(LAB_i, mk_v(VAR_x))),
                          ctr0(LAB_e));
    thvm_atp_add_equation(s,
        ctr2(LAB_f, ctr2(LAB_f, mk_v(VAR_x), mk_v(1u)), mk_v(2u)),
        ctr2(LAB_f, mk_v(VAR_x), ctr2(LAB_f, mk_v(1u), mk_v(2u))));
    for (u32 k = 0; k < 64u; k++) {
      if (thvm_atp_queued_cp_count(s) > 4u) break;
      if (thvm_atp_step(s) != ATP_RUNNING) break;
    }

    TEST_BEGIN("atp/gnn_score/rerank-permutes-queue");
    u32 nq = thvm_atp_queued_cp_count(s);
    CHECK(nq > 1u);
    u32 seqB[64], priBefore[64];
    thvm_atp_queued_cps(s, NULL, NULL, seqB, 64u);
    // Capture current priorities by seq via a snapshot of the heap.
    // (cp_pri lives on the state; pull through queued_cps + a small read.)
    for (u32 i = 0; i < nq; i++) priBefore[i] = s->cp_pri[i];

    u32 reranked = thvm_atp_gnn_rerank(s);
    CHECK_EQ(reranked, nq);
    // Queue size unchanged: re-rank never adds/drops a CP.
    CHECK_EQ(thvm_atp_queued_cp_count(s), nq);
    // Every priority now sits in the safe band [0, 2e9].
    for (u32 i = 0; i < nq; i++) CHECK(s->cp_pri[i] <= 2000000000u);
    // At least one priority changed vs the size-weighted baseline (the
    // GNN scores differ across structurally distinct CPs).
    int any_changed = 0;
    for (u32 i = 0; i < nq; i++) if (s->cp_pri[i] != priBefore[i]) any_changed = 1;
    CHECK(any_changed);

    // The engine keeps stepping soundly after re-ranking (re-rank only
    // permutes selection order; it never corrupts the queue).  We just
    // confirm a bounded run returns a defined AtpStatus without crashing
    // the specific terminal status depends on the step cap.
    AtpStatus st = ATP_RUNNING;
    for (u32 k = 0; k < 200u && st == ATP_RUNNING; k++) st = thvm_atp_step(s);
    CHECK(st <= ATP_QUEUE_EMPTY);   // a valid enum value, no garbage/crash

    thvm_atp_free(s);
    thvm_atp_clear_gnn_scorer();
  }

  // Bucketing invariance: thvm_atp_gnn_score_batch buckets B and N to
  // powers of two so the JIT kernel source is identical across re-ranks
  // (the CPU on-disk dylib cache then hits instead of recompiling every
  // time the live-queue size changes).  Padded rows are masked out of
  // the pool and have no edges, so a CP's score must be IDENTICAL whether
  // it is scored alone (B=1, padded to bucket 4) or inside a batch (B=3).
  TEST_BEGIN("atp/gnn_score/bucketing-invariance");
  {
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    float solo[1], batch3[3];
    CHECK_EQ(thvm_atp_gnn_score_batch(&lhs[0], &rhs[0], 1u, solo), 1);
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, 3u, batch3), 1);
    CHECK(fabsf(solo[0] - batch3[0]) < 1.0e-6f);
    thvm_atp_clear_gnn_scorer();
  }

  // A model-free call is a no-op (returns 0), and clear unloads.
  TEST_BEGIN("atp/gnn_score/clear-and-noop");
  thvm_atp_clear_gnn_scorer();
  CHECK_EQ(thvm_atp_gnn_loaded(), 0);
  float dummy[3];
  CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, B, dummy), 0);
  CHECK_EQ(thvm_atp_gnn_rerank(NULL), 0u);

  free(blob);
  (void)f;
  thvm_free();
  TEST_REPORT();
}
