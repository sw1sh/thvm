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
#include <time.h>

// Hand-built CP labels + a variable id.
#define LAB_e 1u
#define LAB_i 2u
#define LAB_f 3u
#define LAB_a 4u
#define VAR_x 0u

static Term ctr0(u32 l)                 { return term_new_ctr(l, NULL, 0); }
static Term ctr1(u32 l, Term a)         { Term cs[1] = {a};    return term_new_ctr(l, cs, 1); }
static Term ctr2(u32 l, Term a, Term b) { Term cs[2] = {a, b}; return term_new_ctr(l, cs, 2); }

// Write the on-wire GCN blob [R, H, per-round W1/Ws/Bh, Wout, Bout] to a
// .safetensors file (named f32 tensors + Rounds/Hidden in __metadata__),
// matching what TAtpSaveGnnScorer writes, so thvm_atp_load_gnn_safetensors
// can be round-trip tested without WL.  Shapes are written 1-D (the loader
// keys on element count); offsets are sequential in canonical order.
static void write_gcn_safetensors(const char *path, const double *blob) {
  u32 rR = (u32)blob[0], hH = (u32)blob[1];
  char  nm[64][16]; u32 ct[64]; u32 nt = 0u;
  for (u32 r = 0u; r < rR; r++) {
    u32 inr = (r == 0u) ? (u32)ATP_CPG_FEAT_DIM : hH;
    snprintf(nm[nt], 16, "W1_%u", r); ct[nt++] = inr * hH;
    snprintf(nm[nt], 16, "Ws_%u", r); ct[nt++] = inr * hH;
    snprintf(nm[nt], 16, "Bh_%u", r); ct[nt++] = hH;
  }
  snprintf(nm[nt], 16, "Wout"); ct[nt++] = hH * 2u;
  snprintf(nm[nt], 16, "Bout"); ct[nt++] = 2u;

  char json[8192]; int jp = 0; u32 off = 0u;
  jp += snprintf(json + jp, sizeof json - jp, "{");
  for (u32 i = 0u; i < nt; i++) {
    jp += snprintf(json + jp, sizeof json - jp,
      "\"%s\":{\"dtype\":\"F32\",\"shape\":[%u],\"data_offsets\":[%u,%u]},",
      nm[i], ct[i], off, off + ct[i] * 4u);
    off += ct[i] * 4u;
  }
  jp += snprintf(json + jp, sizeof json - jp,
    "\"__metadata__\":{\"Rounds\":\"%u\",\"Hidden\":\"%u\"}}", rR, hH);
  while (jp % 8 != 0) json[jp++] = ' ';

  FILE *f = fopen(path, "wb");
  u64 hl = (u64)jp;
  fwrite(&hl, 8u, 1u, f);
  fwrite(json, 1u, (size_t)jp, f);
  u32 total = 0u; for (u32 i = 0u; i < nt; i++) total += ct[i];
  for (u32 k = 0u; k < total; k++) { float v = (float)blob[2u + k]; fwrite(&v, 4u, 1u, f); }
  fclose(f);
}

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

  // === Coop: GNN drives the SECONDARY dimension, primary heap intact ===
  // With gnn_coop set, the re-rank writes cp_pri2 (the w2 coop dimension)
  // and leaves cp_pri (the primary heap) untouched, so the GNN guides the
  // every-w2_modulo-th selection while the primary preset (e.g.
  // Waldmeister) still drives the heap root.  Fresh CPs get the neutral
  // cp_pri2 band until scored.
  {
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    static u32 cw[5] = {0, 1, 0, 1, 1};
    static u32 cp[5] = {0, 2, 4, 3, 1};
    static const KboConfig COOP_CFG = {cw, cp, 5, 1};
    AtpState *s = thvm_atp_init(&COOP_CFG, 256);
    thvm_atp_set_gnn_coop(s, 2);     // GNN -> cp_pri2, coop pick every 2nd
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
    TEST_BEGIN("atp/gnn_score/coop-targets-secondary");
    u32 nq = thvm_atp_queued_cp_count(s);
    CHECK(nq > 1u);
    u32 priBefore[64], pri2Before[64];
    for (u32 i = 0; i < nq; i++) { priBefore[i] = s->cp_pri[i]; pri2Before[i] = s->cp_pri2[i]; }
    // Fresh CPs got the neutral secondary (gnn_coop set before any push).
    for (u32 i = 0; i < nq; i++) CHECK_EQ(pri2Before[i], ATP_GNN_COOP_NEUTRAL_PRI);
    CHECK_EQ(thvm_atp_gnn_rerank(s), nq);
    // Primary heap untouched: the WM/primary preset still owns the root.
    for (u32 i = 0; i < nq; i++) CHECK_EQ(s->cp_pri[i], priBefore[i]);
    // Secondary now carries GNN priorities (>= one moved off neutral, band-bounded).
    int any2 = 0;
    for (u32 i = 0; i < nq; i++) {
      if (s->cp_pri2[i] != pri2Before[i]) any2 = 1;
      CHECK(s->cp_pri2[i] <= 2000000000u);
    }
    CHECK(any2);
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

  // JIT capture/replay: the first score at a batch bucket B captures the
  // forward; later scores at the same bucket rewrite the persistent input
  // buffers in place and replay the recorded kernel sequence (no
  // re-schedule).  The replay path MUST produce scores bit-identical to the
  // cold realize path.  This is the whole correctness contract of the cache.
  //   (a) Same batch scored twice -> warm replay == cold realize, exactly.
  //   (b) A DIFFERENT batch at the same bucket through the warm path ==
  //       the same different batch scored cold in a fresh model (which
  //       drops the cache, forcing a re-capture).  This proves the in-place
  //       buffer rewrite actually feeds the replayed kernels fresh data.
  TEST_BEGIN("atp/gnn_score/jit-replay-bit-identical");
  {
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    // (a) cold then warm over the IDENTICAL batch (B padded to bucket 4).
    float cold[3], warm[3];
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, 3u, cold), 1);   // captures
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, 3u, warm), 1);   // replays
    for (u32 i = 0; i < 3u; i++) {
      printf("  replay-same CP %u: cold=% .8f warm=% .8f\n", i, cold[i], warm[i]);
      CHECK(cold[i] == warm[i]);   // bit-identical: same buffers, same kernels
    }

    // (b) a DIFFERENT batch through the now-warm bucket-4 capture.
    Term lhs2[2], rhs2[2];
    lhs2[0] = ctr2(LAB_f, ctr1(LAB_i, mk_v(VAR_x)), mk_v(VAR_x));   // i(x).x
    rhs2[0] = ctr0(LAB_e);
    lhs2[1] = ctr1(LAB_i, ctr1(LAB_i, mk_v(VAR_x)));                // i(i(x))
    rhs2[1] = mk_v(VAR_x);
    float warm2[2];
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs2, rhs2, 2u, warm2), 1);   // replays

    // Reference: clear (drops cache) + reload, then cold-score lhs2/rhs2.
    thvm_atp_clear_gnn_scorer();
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    float cold2[2];
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs2, rhs2, 2u, cold2), 1);
    for (u32 i = 0; i < 2u; i++) {
      printf("  replay-diff CP %u: cold=% .8f warm=% .8f  |diff|=%.2e\n",
             i, cold2[i], warm2[i], fabsf(cold2[i] - warm2[i]));
      // Same data through the SAME compiled kernels -> bit-identical.
      CHECK(warm2[i] == cold2[i]);
    }
    thvm_atp_clear_gnn_scorer();
  }

  // C-side safetensors load: thvm_atp_load_gnn_safetensors reads a GCN
  // .safetensors (the bundled GCNAtpScorer asset / TAtpSaveGnnScorer
  // output) and pushes it to the engine with no WL.  Round-trip the known
  // blob through a temp file and confirm the reloaded weights are bit-
  // identical to the originals.
  TEST_BEGIN("atp/gnn_score/safetensors-load-roundtrip");
  {
    char p[] = "/tmp/thvm_gcn_load_XXXXXX";
    int fd = mkstemp(p);
    CHECK(fd >= 0);
    close(fd);
    write_gcn_safetensors(p, blob);
    thvm_atp_clear_gnn_scorer();
    CHECK_EQ(thvm_atp_load_gnn_safetensors(p), 1);
    CHECK_EQ(g_atp_gnn.rounds, (u32)blob[0]);
    CHECK_EQ(g_atp_gnn.hidden, (u32)blob[1]);
    for (u32 i = 0u; i < blob_len - 2u; i++) {
      CHECK(fabsf(g_atp_gnn.w[i] - (float)blob[2u + i]) < 1.0e-6f);
    }
    thvm_atp_clear_gnn_scorer();
    unlink(p);
  }

  // Regression: a CP whose graph exceeds the node cap (ATP_GNN_N_CAP) used
  // to force a giant dense {B,N,N} adjacency and SIGBUS the realized
  // kernel; and a batch larger than ATP_GNN_B_CAP must chunk.  Score both
  // and confirm no crash + finite scores.  (A ~200-node left-nested
  // f-tower well exceeds the 64-node cap; it is truncated.)
  TEST_BEGIN("atp/gnn_score/giant-graph-and-chunk-no-crash");
  {
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    Term big = ctr0(LAB_a);
    for (u32 d = 0u; d < 200u; d++) big = ctr2(LAB_f, big, ctr0(LAB_a));
    Term bl[2] = {big, lhs[0]}, br[2] = {ctr0(LAB_a), rhs[0]};
    float gsc[2];
    CHECK_EQ(thvm_atp_gnn_score_batch(bl, br, 2u, gsc), 1);
    CHECK(isfinite(gsc[0]) && isfinite(gsc[1]));
    // Batch > ATP_GNN_B_CAP (1024) exercises the chunk loop.
    enum { NB = 1100u };
    Term *bbl = (Term *)malloc(NB * sizeof(Term));
    Term *bbr = (Term *)malloc(NB * sizeof(Term));
    float *bsc = (float *)malloc(NB * sizeof(float));
    for (u32 i = 0u; i < NB; i++) { bbl[i] = lhs[i % 3u]; bbr[i] = rhs[i % 3u]; }
    CHECK_EQ(thvm_atp_gnn_score_batch(bbl, bbr, NB, bsc), 1);
    CHECK(isfinite(bsc[0]) && isfinite(bsc[NB - 1u]));
    // A CP scored in a >cap batch matches the same CP scored small (batch
    // independence holds across chunks).
    float one[1];
    CHECK_EQ(thvm_atp_gnn_score_batch(&lhs[0], &rhs[0], 1u, one), 1);
    CHECK(fabsf(one[0] - bsc[0]) < 1.0e-5f);
    free(bbl); free(bbr); free(bsc);
    thvm_atp_clear_gnn_scorer();
  }

  // Regression (train-then-rerank in one kernel): the GNN scorer runs its
  // forward in a dedicated sandbox context (g_atp_gnn_ctx), whose `kernels`
  // table starts at kid 1 -- the SAME indices the default/engine context
  // uses.  cpu/jit.c's per-kid resolved-fn cache used to be a process-global
  // KID_JIT_FN[kid], so the sandbox's compiled GCN fns aliased kids 1..N and
  // a later default-context CPU-JIT dispatch (a training realize) at those
  // kids ran the GCN fn against the wrong buffers / input arity -> SIGSEGV.
  // With KID_JIT_FN per-context the indices no longer collide.  This case
  // scores (populating the sandbox's per-kid cache) then fires unrelated
  // default-context JIT kernels at the same kid indices; it must not crash.
  TEST_BEGIN("atp/gnn_score/realize-then-score-no-crash");
  {
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    // A first score compiles + caches the GCN kernels in the sandbox context
    // at kids 1..N (populating that context's per-kid fn cache).
    float pre[3];
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, 3u, pre), 1);
    // Then fire unrelated default-context CPU-JIT kernels at the same kid
    // indices (this is what a TAtpTrainGnn step does between re-ranks).  A
    // shared global per-kid cache would dispatch the GCN fn here and crash.
    for (int step = 0; step < 8; step++) {
      u32 dims[2] = {16u, 16u};
      u32 t1 = atp_gnn_input_tensor(2u, dims);
      u32 t2 = atp_gnn_input_tensor(2u, dims);
      float fb[256];
      for (u32 i = 0u; i < 256u; i++) fb[i] = 0.001f * (float)i;
      atp_gnn_buf_fill(t1, fb, 256u);
      atp_gnn_buf_fill(t2, fb, 256u);
      Term ta = term_new(0, TAG_TEN, DT_FP32, t1);
      Term tb = term_new(0, TAG_TEN, DT_FP32, t2);
      Term tc = uop_binary(UOP_ADD, ta, tb);
      Term td = uop_binary(UOP_MUL, tc, ta);
      (void)term_resolve(thvm_realize(td));
    }
    // Score again (warm replay through the sandbox capture) after the
    // intervening default-context fires: must still produce finite scores
    // identical to the pre-fire score.
    float rs[3];
    CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, 3u, rs), 1);
    CHECK(isfinite(rs[0]) && isfinite(rs[1]) && isfinite(rs[2]));
    for (u32 i = 0u; i < 3u; i++) CHECK(fabsf(rs[i] - pre[i]) < 1.0e-5f);
    thvm_atp_clear_gnn_scorer();
  }

  // A model-free call is a no-op (returns 0), and clear unloads.
  TEST_BEGIN("atp/gnn_score/clear-and-noop");
  thvm_atp_clear_gnn_scorer();
  CHECK_EQ(thvm_atp_gnn_loaded(), 0);
  float dummy[3];
  CHECK_EQ(thvm_atp_gnn_score_batch(lhs, rhs, B, dummy), 0);
  CHECK_EQ(thvm_atp_gnn_rerank(NULL), 0u);

  // Optional microbenchmark (THVM_ATP_GNN_BENCH=1): score a large fixed
  // batch repeatedly and report the COLD (first, full realize + capture)
  // vs WARM (subsequent, jit-replay) per-call wall time.  Isolates the
  // documented per-re-rank scheduler cost the capture cache removes.
  if (getenv("THVM_ATP_GNN_BENCH") != NULL) {
    CHECK_EQ(thvm_atp_set_gnn_scorer(blob, blob_len), 1);
    const u32 NB = 256u;          // bucketed to B = 256
    Term *bl = (Term *)malloc(NB * sizeof(Term));
    Term *br = (Term *)malloc(NB * sizeof(Term));
    float *sc = (float *)malloc(NB * sizeof(float));
    for (u32 i = 0; i < NB; i++) { bl[i] = lhs[i % 3u]; br[i] = rhs[i % 3u]; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    thvm_atp_gnn_score_batch(bl, br, NB, sc);     // cold: realize + capture
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double cold_ms = (t1.tv_sec - t0.tv_sec) * 1e3
                   + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    enum { REP = 50 };
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (u32 k = 0; k < REP; k++) thvm_atp_gnn_score_batch(bl, br, NB, sc);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double warm_ms = ((t1.tv_sec - t0.tv_sec) * 1e3
                   + (t1.tv_nsec - t0.tv_nsec) / 1e6) / (double)REP;
    printf("  [gnn-bench] B=%u  cold=%.3f ms  warm=%.3f ms  speedup=%.1fx\n",
           NB, cold_ms, warm_ms, cold_ms / warm_ms);
    free(bl); free(br); free(sc);
    thvm_atp_clear_gnn_scorer();
  }

  free(blob);
  (void)f;
  thvm_free();
  TEST_REPORT();
}
