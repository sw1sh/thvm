// === ENIGMA Tier 2: in-engine GCN critical-pair scorer ==============
// Runs the GCN forward on thvm's OWN tensor runtime: it builds a UOP
// graph out of the documented constructors (uop_reshape / uop_expand /
// uop_binary / uop_reduce / uop_unary), thvm_realize's it, and reads the
// per-graph proof-relevance score back from the realized buffer.  This
// REPLICATES the WL reference forward (atpGnnForwardLogits in
// wl/THVMLink/Kernel/ATP/ATP.wl): the same row-normalised-adjacency
// message passing, masked-mean pool, and two-class readout, so a model
// trained in WL scores identically in the engine (the test_atp_gnn_score
// differential pins this to ~1e-3).

// The loaded model.  Weights live in one heap-allocated f32 blob laid
// out exactly as the on-the-wire f64 blob documents (see thvm.h): per
// round r, W1[r] (in_r*H), Ws[r] (in_r*H), Bh[r] (H), with in_r = 6 on
// round 0 and H thereafter; then Wout (H*2), Bout (2).  Offsets are
// recomputed on the fly from R/H so the struct stays a single owned
// buffer rather than a fixed-cap matrix.
typedef struct {
  u32    rounds;     // R >= 1
  u32    hidden;     // H >= 1
  float *w;          // owned flat parameter buffer
  u32    w_len;      // element count of `w`
} AtpGnnScorer;

static AtpGnnScorer g_atp_gnn = {0u, 0u, NULL, 0u};
static int          g_atp_gnn_active = 0;
// Dedicated thvm context the GCN forward runs in (lazily created), so its
// per-re-rank tensor/UOP/kernel scratch is reclaimed (thvm_reset) without
// disturbing the ATP engine's term heap in the default context.
static u32          g_atp_gnn_ctx = 0u;

// Input dim feeding round r: ATP_CPG_FEAT_DIM on round 0, H thereafter.
static u32 atp_gnn_in_dim(const AtpGnnScorer *m, u32 r) {
  return (r == 0u) ? ATP_CPG_FEAT_DIM : m->hidden;
}

// Round up to the next power of two.  The GCN score-batch buckets its
// batch (B) dim to powers of two (the node dim N is fixed at a cap) so the
// rendered kernel source is identical across re-ranks with different
// live-queue sizes -- which makes the CPU JIT's on-disk dylib cache hit
// (one ~2s compile per distinct B bucket, then ~5ms per re-rank, vs a 2s
// recompile every time the queue size changed).  Padded rows stay zero
// and their scores are discarded, so bucketing is numerically invisible.
static u32 atp_gnn_pow2_ceil(u32 v) {
  u32 p = 1u;
  while (p < v) p <<= 1;
  return p;
}

// Total flat element count for an (R, H) model - the on-wire blob
// length minus the leading 2 header words.
static u32 atp_gnn_param_count(u32 rounds, u32 hidden) {
  u32 n = 0u;
  for (u32 r = 0u; r < rounds; r++) {
    u32 in_r = (r == 0u) ? ATP_CPG_FEAT_DIM : hidden;
    n += 2u * in_r * hidden;   // W1[r] + Ws[r]
    n += hidden;               // Bh[r]
  }
  n += hidden * 2u;            // Wout
  n += 2u;                     // Bout
  return n;
}

// Offset (into m->w) of W1[r] / Ws[r] / Bh[r] / Wout / Bout.
static u32 atp_gnn_off_round(const AtpGnnScorer *m, u32 r) {
  u32 off = 0u;
  for (u32 i = 0u; i < r; i++) {
    u32 in_i = atp_gnn_in_dim(m, i);
    off += 2u * in_i * m->hidden + m->hidden;
  }
  return off;
}
static const float *atp_gnn_w1(const AtpGnnScorer *m, u32 r) {
  return m->w + atp_gnn_off_round(m, r);
}
static const float *atp_gnn_ws(const AtpGnnScorer *m, u32 r) {
  return m->w + atp_gnn_off_round(m, r) + atp_gnn_in_dim(m, r) * m->hidden;
}
static const float *atp_gnn_bh(const AtpGnnScorer *m, u32 r) {
  return m->w + atp_gnn_off_round(m, r) + 2u * atp_gnn_in_dim(m, r) * m->hidden;
}
static const float *atp_gnn_wout(const AtpGnnScorer *m) {
  return m->w + atp_gnn_off_round(m, m->rounds);   // past the last round block
}
static const float *atp_gnn_bout(const AtpGnnScorer *m) {
  return atp_gnn_wout(m) + m->hidden * 2u;
}

fn int thvm_atp_set_gnn_scorer(const double *blob, u32 len) {
  if (blob == NULL || len < 4u) return 0;
  u32 rounds = (u32)blob[0];
  u32 hidden = (u32)blob[1];
  if (rounds == 0u || hidden == 0u) return 0;
  u32 want = 2u + atp_gnn_param_count(rounds, hidden);
  if (len != want) return 0;

  float *w = (float *)malloc((size_t)(want - 2u) * sizeof(float));
  if (w == NULL) return 0;
  for (u32 i = 0u; i < want - 2u; i++) w[i] = (float)blob[2u + i];

  free(g_atp_gnn.w);
  g_atp_gnn.rounds = rounds;
  g_atp_gnn.hidden = hidden;
  g_atp_gnn.w      = w;
  g_atp_gnn.w_len  = want - 2u;
  g_atp_gnn_active = 1;
  return 1;
}

static void atp_gnn_capture_cache_clear(void);

fn void thvm_atp_clear_gnn_scorer(void) {
  free(g_atp_gnn.w);
  g_atp_gnn.w = NULL;
  g_atp_gnn.w_len = 0u;
  g_atp_gnn.rounds = 0u;
  g_atp_gnn.hidden = 0u;
  g_atp_gnn_active = 0;
  // The captured forward bakes the model weights as constants; a reload
  // with different weights must re-capture, so invalidate the cache.  This
  // runs in the engine context, but atp_gnn_capture_cache_clear selects the
  // GNN sandbox context for the drop so the retained-buffer unpin/decref hit
  // the correct context's CPU_BUFS.
  atp_gnn_capture_cache_clear();
}

fn int thvm_atp_gnn_loaded(void) { return g_atp_gnn_active; }

// Read one named f32 tensor (`count` elements) out of a safetensors file
// into `dst` (as doubles).  `json` is the file's JSON header; `data_start`
// is the absolute byte offset of the data section (8 + header length).
// Tolerant scan: find "<name>": then its "data_offsets":[begin,end].
static int atp_st_read(FILE *f, const char *json, u64 data_start,
                       const char *name, u32 count, double *dst) {
  char key[32];
  snprintf(key, sizeof key, "\"%s\":", name);
  const char *p = strstr(json, key);
  if (p == NULL) return 0;
  const char *q = strstr(p, "\"data_offsets\":[");
  if (q == NULL) return 0;
  long begin = 0, end = 0;
  if (sscanf(q + 16, "%ld,%ld", &begin, &end) != 2) return 0;
  if (end < begin || (u32)((end - begin) / 4) != count) return 0;
  if (fseek(f, (long)(data_start + (u64)begin), SEEK_SET) != 0) return 0;
  float *tmp = (float *)malloc((size_t)count * sizeof(float));
  if (tmp == NULL) return 0;
  int ok = (fread(tmp, sizeof(float), count, f) == count);
  for (u32 i = 0u; ok && i < count; i++) dst[i] = (double)tmp[i];
  free(tmp);
  return ok;
}

// Load a GCN scorer from a .safetensors file (the C-side analog of WL's
// TAtpSetGnnScorer[path]) and push it to the engine -- no WL needed.  The
// file is the one TAtpSaveGnnScorer writes / the bundled GCNAtpScorer
// asset: named f32 tensors W1_<r>/Ws_<r>/Bh_<r> (r 0-based) + Wout + Bout,
// with Rounds/Hidden in __metadata__.  Layout is tinygrad's safetensors
// (nn/state.py): an 8-byte LE u64 header length, that many JSON bytes
// {name:{dtype,shape,data_offsets:[begin,end]},...}, then concatenated LE
// tensor data.  The named tensors are packed into the flat blob order the
// engine expects (per round W1,Ws,Bh; then Wout,Bout), mirroring
// serializeGnnModel.  Returns 1 on success, 0 on any error.
fn int thvm_atp_load_gnn_safetensors(const char *path) {
  if (path == NULL) return 0;
  FILE *f = fopen(path, "rb");
  if (f == NULL) return 0;

  u64 hlen = 0u;
  if (fread(&hlen, 8u, 1u, f) != 1u || hlen == 0u || hlen > (1u << 24)) {
    fclose(f); return 0;
  }
  char *json = (char *)malloc((size_t)hlen + 1u);
  if (json == NULL) { fclose(f); return 0; }
  if (fread(json, 1u, (size_t)hlen, f) != (size_t)hlen) {
    free(json); fclose(f); return 0;
  }
  json[hlen] = '\0';
  u64 data_start = 8u + hlen;

  u32 R = 0u, H = 0u;
  { const char *p = strstr(json, "\"Rounds\":\""); if (p) R = (u32)strtoul(p + 10, NULL, 10); }
  { const char *p = strstr(json, "\"Hidden\":\""); if (p) H = (u32)strtoul(p + 10, NULL, 10); }
  if (R == 0u || H == 0u) { free(json); fclose(f); return 0; }
  // Feature-dim version guard: the forward is compiled for exactly
  // ATP_CPG_FEAT_DIM inputs, so a model saved at a different schema (e.g. a
  // legacy F=6 blob against the F=12 engine) cannot be fed the current
  // graphs.  Reject it cleanly rather than mis-reshaping W1_0.  Absent
  // "Feat" (pre-versioning files) falls through to the per-tensor element
  // count check below, which also catches the mismatch.
  { const char *p = strstr(json, "\"Feat\":\"");
    if (p) {
      u32 feat = (u32)strtoul(p + 8, NULL, 10);
      if (feat != ATP_CPG_FEAT_DIM) { free(json); fclose(f); return 0; }
    } }

  u32 want = 2u + atp_gnn_param_count(R, H);
  double *blob = (double *)malloc((size_t)want * sizeof(double));
  if (blob == NULL) { free(json); fclose(f); return 0; }
  blob[0] = (double)R; blob[1] = (double)H;
  u32 bp = 2u;
  int ok = 1;
  char nm[16];
  for (u32 r = 0u; r < R; r++) {
    u32 inr = (r == 0u) ? ATP_CPG_FEAT_DIM : H;
    snprintf(nm, sizeof nm, "W1_%u", r); ok &= atp_st_read(f, json, data_start, nm, inr * H, blob + bp); bp += inr * H;
    snprintf(nm, sizeof nm, "Ws_%u", r); ok &= atp_st_read(f, json, data_start, nm, inr * H, blob + bp); bp += inr * H;
    snprintf(nm, sizeof nm, "Bh_%u", r); ok &= atp_st_read(f, json, data_start, nm, H,       blob + bp); bp += H;
  }
  ok &= atp_st_read(f, json, data_start, "Wout", H * 2u, blob + bp); bp += H * 2u;
  ok &= atp_st_read(f, json, data_start, "Bout", 2u,     blob + bp); bp += 2u;
  fclose(f);
  free(json);

  int set = (ok && bp == want) ? thvm_atp_set_gnn_scorer(blob, want) : 0;
  free(blob);
  return set;
}

// Wrap a host f32 buffer as a fresh CPU-backend TAG_TEN of the given
// shape.  Allocates an owning buffer (tensor_alloc) and copies `data`
// into it, so the caller's buffer can be reused/freed immediately.
static Term atp_gnn_ten(const float *data, u32 ndim, const u32 *dims) {
  Shape sh = {0};
  sh.ndim = ndim;
  for (u32 i = 0u; i < ndim; i++) sh.dims[i] = dims[i];
  u32 tid = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u64 nbytes = dtype_storage_bytes(DT_FP32, (u64)TENS[tid].view.numel);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, nbytes);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

// Batched matmul (B,P,K).(B,K,Q) -> (B,P,Q) via the reshape/expand/
// MUL/reduce-SUM pattern - the exact lowering atpBatchMatMul uses in
// the WL reference.
static Term atp_gnn_bmm(Term a, Term b, u32 B, u32 P, u32 K, u32 Q) {
  u32 a4[4] = {B, P, K, 1u}, e4[4] = {B, P, K, Q}, b4[4] = {B, 1u, K, Q};
  Term ae = uop_expand(uop_reshape(a, 4u, a4), 4u, e4);
  Term be = uop_expand(uop_reshape(b, 4u, b4), 4u, e4);
  return uop_reduce(REDUCE_SUM, 2u, uop_binary(UOP_MUL, ae, be));   // {B,P,Q}
}

// Apply a per-feature weight w {K,H} to node tensor h {B,P,K} -> {B,P,H}.
// The WL reference does a 2-D TMatMul over the flattened (B*P, K); here
// we broadcast w to {B,K,H} and reuse the batched matmul - numerically
// identical (a sum over K of h[b,p,k]*w[k,hh]), keeping every op on the
// thvm runtime with one primitive.
static Term atp_gnn_applyw(Term h, Term w, u32 B, u32 P, u32 K, u32 H) {
  u32 w3[3] = {1u, K, H}, we[3] = {B, K, H};
  Term wb = uop_expand(uop_reshape(w, 3u, w3), 3u, we);
  return atp_gnn_bmm(h, wb, B, P, K, H);
}

// Broadcast a length-D bias to a {B,P,D} tensor.
static Term atp_gnn_bcast_bias(Term bias, u32 B, u32 P, u32 D) {
  u32 b3[3] = {1u, 1u, D}, be[3] = {B, P, D};
  return uop_expand(uop_reshape(bias, 3u, b3), 3u, be);
}

// ReLU(x) = x * (0 < x), the TReLU lowering (MUL[x, CMPLT[const0, x]]).
static Term atp_gnn_relu(Term x) {
  Term zero = uop_const(DT_FP32, 0u);          // 0.0f bits
  return uop_binary(UOP_MUL, x, uop_binary(UOP_CMPLT, zero, x));
}

// Bound the GCN inference shape.  The node dim N is FIXED at a cap (a CP
// term with hundreds of nodes is far outside the trained NMax, and the
// dense {B,N,N} adjacency is O(N^2) -- an 800-node CP at batch 4096 is
// ~17 GB, which calloc overcommits and the kernel then SIGBUSes on touch),
// so graphs larger than the cap are TRUNCATED to the first N_CAP nodes.
// The batch is processed in chunks of <= B_CAP.  Both caps are powers of
// two; with N fixed, only a handful of B buckets ever compile (each once,
// persisted on disk), and peak scratch is bounded by B_CAP * N_CAP^2.
#define ATP_GNN_N_CAP 64u
#define ATP_GNN_B_CAP 1024u
// Neutral secondary priority for a not-yet-GNN-scored CP in coop mode:
// the score-0 point of the GNN priority map (1e6 - 1e4*score), so fresh
// CPs sit mid-band until the next re-rank instead of on the heuristic
// weight scale (~tens), which would otherwise always out-rank GNN picks.
#define ATP_GNN_COOP_NEUTRAL_PRI 1000000u

// Per-batch-shape JIT capture cache.  The GCN forward is structurally
// IDENTICAL across re-ranks at a given batch bucket B (the node dim N is
// fixed; weights are constant for the run), so re-running the scheduler
// (realize_classify / topo-sort / materialize / codegen) on every re-rank
// is ~4-5 ms of pure waste -- the kernels themselves are cheap.  We use
// thvm's JIT capture/replay (src/jit/capture.c): the first re-rank at a
// bucket builds the forward, captures the kernel-dispatch sequence around
// one thvm_realize, and pins the persistent input + output buffers; later
// re-ranks at the same bucket rewrite the input buffer bytes in place and
// jit_replay the recorded sequence -- no re-schedule, no re-codegen.
//
// B is bucketed to a power of two in [4, ATP_GNN_B_CAP=1024], so at most
// 9 distinct buckets ever compile (each captured once).  thvm_reset (fired
// near the descriptor caps) drops every capture slot AND frees the sandbox
// buffers, so it must invalidate this cache; atp_gnn_capture_cache_clear
// does that wherever the reset is issued.
typedef struct {
  u32  B;            // batch bucket this capture serves (0 = empty slot)
  u32  slot;         // jit capture slot id (>= 1 when valid)
  u32  x_tid;        // persistent input TenDescs (stable buf_ids)
  u32  a_tid;
  u32  mask_tid;
  u32  nn_tid;
  u32  x_buf;        // their buf_ids, declared as the jit input baseline
  u32  a_buf;
  u32  mask_buf;
  u32  nn_buf;
  u32  out_tid;      // realized logits tensor (its buf is rewritten by replay)
} AtpGnnCapture;

// One entry per power-of-two bucket from 4 up to ATP_GNN_B_CAP.
#define ATP_GNN_CAP_SLOTS 12
static AtpGnnCapture g_atp_gnn_captures[ATP_GNN_CAP_SLOTS];

// Drop every cached GCN capture.  Called wherever thvm_reset is issued in
// the sandbox context, AND from thvm_atp_clear_gnn_scorer (which runs in the
// engine context).  jit_capture_drop releases each capture's retained
// buffers via buf_jit_unpin / buf_decref, and those hooks index the CURRENT
// context's CPU_BUFS (CPU_BUFS == CURRENT_CTX->cpu_bufs).  The retained
// buf_ids name buffers in the GNN sandbox context (g_atp_gnn_ctx), so the
// drop MUST run with that context selected -- otherwise the unpin/decref
// lands on whatever buffer happens to share that id in the engine context,
// dropping a live engine buffer's refcount to 0 and freeing it under a kernel
// that still reads it (the train-then-rerank SIGSEGV).  Select the sandbox
// context for the drop and restore the caller's context after.
static void atp_gnn_capture_cache_clear(void) {
  int have_slots = 0;
  for (u32 i = 0u; i < ATP_GNN_CAP_SLOTS; i++) {
    if (g_atp_gnn_captures[i].slot != 0u) { have_slots = 1; break; }
  }
  u32 prev = 0u;
  int switched = 0;
  if (have_slots && g_atp_gnn_ctx != 0u
      && thvm_context_current() != g_atp_gnn_ctx) {
    prev = thvm_context_select(g_atp_gnn_ctx);
    switched = 1;
  }
  for (u32 i = 0u; i < ATP_GNN_CAP_SLOTS; i++) {
    if (g_atp_gnn_captures[i].slot != 0u) {
      jit_capture_drop(g_atp_gnn_captures[i].slot);
    }
    g_atp_gnn_captures[i] = (AtpGnnCapture){0};
  }
  if (switched) thvm_context_select(prev);
}

// Find the cache entry for bucket B, or the first empty slot to fill.
// Returns NULL only when every slot is taken by a different bucket (cannot
// happen: at most 9 distinct buckets, 12 slots).
static AtpGnnCapture *atp_gnn_capture_for(u32 B) {
  AtpGnnCapture *empty = NULL;
  for (u32 i = 0u; i < ATP_GNN_CAP_SLOTS; i++) {
    if (g_atp_gnn_captures[i].B == B && g_atp_gnn_captures[i].slot != 0u) {
      return &g_atp_gnn_captures[i];
    }
    if (empty == NULL && g_atp_gnn_captures[i].slot == 0u) {
      empty = &g_atp_gnn_captures[i];
    }
  }
  return empty;
}

// Allocate a persistent f32 input tensor of the given shape in the current
// (sandbox) context.  Unlike atp_gnn_ten this leaves the buffer un-written:
// the caller fills it via atp_gnn_buf_fill before each capture/replay.
static u32 atp_gnn_input_tensor(u32 ndim, const u32 *dims) {
  Shape sh = {0};
  sh.ndim = ndim;
  for (u32 i = 0u; i < ndim; i++) sh.dims[i] = dims[i];
  return tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
}

// Write `n` floats into a tensor's buffer in place (the captured kernels
// read this buf_id, so this refreshes their input bytes for replay).
static void atp_gnn_buf_fill(u32 tid, const float *data, u64 n) {
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (const void *)data,
                        (u64)n * sizeof(float));
}

// Build the host X / A / mask / NNodes arrays for one chunk of `nc` CP
// graphs padded to batch B at the fixed node dim N.  Padded rows keep
// NNodes=1 (finite pooled reciprocal) and all-zero X/A/mask, so they are
// inert and their scores are discarded.
static void atp_gnn_build_host(const AtpCpGraph *gs, u32 nc, u32 B,
                               float *xArr, float *aArr, float *maskArr,
                               float *nnArr) {
  const u32 F = ATP_CPG_FEAT_DIM, N = ATP_GNN_N_CAP;
  memset(xArr,    0, (size_t)B * N * F * sizeof(float));
  memset(aArr,    0, (size_t)B * N * N * sizeof(float));
  memset(maskArr, 0, (size_t)B * N * sizeof(float));
  memset(nnArr,   0, (size_t)B * sizeof(float));
  for (u32 bi = nc; bi < B; bi++) nnArr[bi] = 1.0f;
  for (u32 bi = 0u; bi < nc; bi++) {
    const AtpCpGraph *g = &gs[bi];
    u32 nn = (g->n_nodes < N) ? g->n_nodes : N;   // truncate giant graphs
    nnArr[bi] = (float)nn;
    for (u32 ni = 0u; ni < nn; ni++) {
      maskArr[(size_t)bi * N + ni] = 1.0f;
      for (u32 fj = 0u; fj < F; fj++) {
        xArr[((size_t)bi * N + ni) * F + fj] =
            g->node_feat[(size_t)ni * ATP_CPG_FEAT_DIM + fj];
      }
    }
    float *adj = aArr + (size_t)bi * N * N;
    for (u32 e = 0u; e < g->n_edges; e++) {
      u32 src = g->edge_src[e], dst = g->edge_dst[e];
      if (src >= nn || dst >= nn) continue;        // drop truncated edges
      adj[(size_t)src * N + dst] = 1.0f;
      adj[(size_t)dst * N + src] = 1.0f;
    }
    for (u32 ii = 0u; ii < nn; ii++) adj[(size_t)ii * N + ii] = 1.0f;
    for (u32 ii = 0u; ii < N; ii++) {              // row-normalise
      float rowsum = 0.0f;
      for (u32 jj = 0u; jj < N; jj++) rowsum += adj[(size_t)ii * N + jj];
      if (rowsum > 0.0f) {
        for (u32 jj = 0u; jj < N; jj++) adj[(size_t)ii * N + jj] /= rowsum;
      }
    }
  }
}

// Build the GCN forward UOP graph over the four input Terms and return the
// {B,2} logits node.  No kernels fire here (lazy until thvm_realize).
static Term atp_gnn_forward(const AtpGnnScorer *m, u32 B,
                            Term xT, Term aT, Term maskT, Term nnT) {
  const u32 F = ATP_CPG_FEAT_DIM, H = m->hidden, R = m->rounds;
  const u32 N = ATP_GNN_N_CAP;

  Term h = xT;
  u32 lastDim = F;
  for (u32 r = 0u; r < R; r++) {
    u32 inr = atp_gnn_in_dim(m, r);
    Term w1T = atp_gnn_ten(atp_gnn_w1(m, r), 2u, (u32[]){inr, H});
    Term wsT = atp_gnn_ten(atp_gnn_ws(m, r), 2u, (u32[]){inr, H});
    Term bhT = atp_gnn_ten(atp_gnn_bh(m, r), 1u, (u32[]){H});
    Term ah  = atp_gnn_bmm(aT, h, B, N, N, lastDim);          // A.H {B,N,lastDim}
    Term nb  = atp_gnn_applyw(ah, w1T, B, N, lastDim, H);     // (A.H).W1
    Term sf  = atp_gnn_applyw(h, wsT, B, N, lastDim, H);      // H.Ws
    Term msg = uop_binary(UOP_ADD,
                          uop_binary(UOP_ADD, nb, sf),
                          atp_gnn_bcast_bias(bhT, B, N, H));
    h = atp_gnn_relu(msg);
    lastDim = H;
  }

  // Masked-mean pool: (B,1,N).(B,N,H) -> (B,1,H), divide by NNodes -> (B,H).
  Term pooled3 = atp_gnn_bmm(maskT, h, B, 1u, N, H);          // {B,1,H}
  Term nnInv   = uop_unary(UOP_RECIP, nnT);                   // {B,1,1}
  Term nnInvE  = uop_expand(nnInv, 3u, (u32[]){B, 1u, H});
  Term pooled  = uop_reshape(uop_binary(UOP_MUL, pooled3, nnInvE),
                             2u, (u32[]){B, H});              // {B,H}

  // Readout: pooled.Wout + Bout -> {B,2}.
  Term woutT   = atp_gnn_ten(atp_gnn_wout(m), 2u, (u32[]){H, 2u});
  Term boutT   = atp_gnn_ten(atp_gnn_bout(m), 1u, (u32[]){2u});
  Term pooled3b = uop_reshape(pooled, 3u, (u32[]){B, 1u, H});
  Term woutE   = uop_expand(uop_reshape(woutT, 3u, (u32[]){1u, H, 2u}),
                            3u, (u32[]){B, H, 2u});
  Term logit3  = atp_gnn_bmm(pooled3b, woutE, B, 1u, H, 2u);  // {B,1,2}
  Term logits  = uop_reshape(logit3, 2u, (u32[]){B, 2u});
  Term boutE   = uop_expand(uop_reshape(boutT, 2u, (u32[]){1u, 2u}),
                            2u, (u32[]){B, 2u});
  return uop_binary(UOP_ADD, logits, boutE);                 // {B,2}
}

// Read the {B,2} logits buffer and write nc scores (logit_pos - logit_neg).
static int atp_gnn_read_scores(u32 out_tid, u32 B, u32 nc, float *out) {
  if (out_tid == 0u || out_tid >= TENS_NEXT) return 0;
  float *o = (float *)malloc((size_t)B * 2u * sizeof(float));
  if (o == NULL) return 0;
  TENS[out_tid].backend->buf_read(TENS[out_tid].buf_id, o,
                                  (u64)B * 2u * sizeof(float));
  for (u32 i = 0u; i < nc; i++) {
    out[i] = o[(size_t)i * 2u + 1u] - o[(size_t)i * 2u + 0u];
  }
  free(o);
  return 1;
}

// Score one chunk of <= ATP_GNN_B_CAP CP graphs at the FIXED node dim
// N = ATP_GNN_N_CAP.  Must run inside the sandbox context (g_atp_gnn_ctx).
// Writes `nc` scores to out[0..nc); returns 1 on success.
//
// First call at a given batch bucket B captures the forward's kernel-
// dispatch sequence; every later call at the same bucket rewrites the
// persistent input buffers in place and replays the capture -- bypassing
// the scheduler entirely.
static int atp_gnn_score_chunk(const AtpGnnScorer *m, const AtpCpGraph *gs,
                               u32 nc, float *out) {
  const u32 F = ATP_CPG_FEAT_DIM;
  const u32 N = ATP_GNN_N_CAP;
  const u32 B = atp_gnn_pow2_ceil(nc < 4u ? 4u : nc);   // <= ATP_GNN_B_CAP

  // X (B,N,F), A (B,N,N row-normalised adjacency + self-loops), MaskRow
  // (B,1,N), NNodes (B,1,1).
  float *xArr    = (float *)malloc((size_t)B * N * F * sizeof(float));
  float *aArr    = (float *)malloc((size_t)B * N * N * sizeof(float));
  float *maskArr = (float *)malloc((size_t)B * N * sizeof(float));
  float *nnArr   = (float *)malloc((size_t)B * sizeof(float));
  if (xArr == NULL || aArr == NULL || maskArr == NULL || nnArr == NULL) {
    free(xArr); free(aArr); free(maskArr); free(nnArr);
    return 0;
  }
  atp_gnn_build_host(gs, nc, B, xArr, aArr, maskArr, nnArr);

  int ok = 0;
  AtpGnnCapture *cap = atp_gnn_capture_for(B);

  if (cap != NULL && cap->slot != 0u) {
    // Warm path: rewrite the persistent input buffers in place, replay the
    // recorded dispatch sequence (no re-schedule), read the output buffer.
    atp_gnn_buf_fill(cap->x_tid,    xArr,    (u64)B * N * F);
    atp_gnn_buf_fill(cap->a_tid,    aArr,    (u64)B * N * N);
    atp_gnn_buf_fill(cap->mask_tid, maskArr, (u64)B * N);
    atp_gnn_buf_fill(cap->nn_tid,   nnArr,   (u64)B);
    u32 ids[4] = {cap->x_buf, cap->a_buf, cap->mask_buf, cap->nn_buf};
    jit_replay_with_inputs(cap->slot, ids, 4u);
    ok = atp_gnn_read_scores(cap->out_tid, B, nc, out);
  } else if (cap != NULL) {
    // Cold path: allocate persistent inputs, fill them, build + capture the
    // forward around one realize, declare the input baseline, and cache it.
    u32 xTid    = atp_gnn_input_tensor(3u, (u32[]){B, N, F});
    u32 aTid    = atp_gnn_input_tensor(3u, (u32[]){B, N, N});
    u32 maskTid = atp_gnn_input_tensor(3u, (u32[]){B, 1u, N});
    u32 nnTid   = atp_gnn_input_tensor(3u, (u32[]){B, 1u, 1u});
    atp_gnn_buf_fill(xTid,    xArr,    (u64)B * N * F);
    atp_gnn_buf_fill(aTid,    aArr,    (u64)B * N * N);
    atp_gnn_buf_fill(maskTid, maskArr, (u64)B * N);
    atp_gnn_buf_fill(nnTid,   nnArr,   (u64)B);

    Term xT    = term_new(0, TAG_TEN, DT_FP32, xTid);
    Term aT    = term_new(0, TAG_TEN, DT_FP32, aTid);
    Term maskT = term_new(0, TAG_TEN, DT_FP32, maskTid);
    Term nnT   = term_new(0, TAG_TEN, DT_FP32, nnTid);
    Term logits = atp_gnn_forward(m, B, xT, aT, maskT, nnT);

    u32 slot = jit_capture_begin();
    Term realized = term_resolve(thvm_realize(logits));
    if (slot != 0u) {
      jit_capture_end_with_result(realized);
    }
    if (term_tag(realized) == TAG_TEN) {
      u32 outTid = (u32)term_val(realized);
      ok = atp_gnn_read_scores(outTid, B, nc, out);
      if (ok && slot != 0u) {
        u32 ids[4] = {TENS[xTid].buf_id, TENS[aTid].buf_id,
                      TENS[maskTid].buf_id, TENS[nnTid].buf_id};
        jit_capture_set_inputs(slot, ids, 4u);
        *cap = (AtpGnnCapture){
          .B = B, .slot = slot,
          .x_tid = xTid, .a_tid = aTid, .mask_tid = maskTid, .nn_tid = nnTid,
          .x_buf = ids[0], .a_buf = ids[1], .mask_buf = ids[2], .nn_buf = ids[3],
          .out_tid = outTid
        };
      } else if (slot != 0u) {
        jit_capture_drop(slot);
      }
    } else if (slot != 0u) {
      jit_capture_drop(slot);
    }
  }

  free(xArr); free(aArr); free(maskArr); free(nnArr);
  return ok;
}

// M2: score `n` CPs with the loaded GNN on the thvm UOP runtime.  CP graphs
// are extracted in the CURRENT (engine) context; the tensor forward then
// runs in a dedicated sandbox context (g_atp_gnn_ctx) so its scratch is
// reclaimable without touching the engine's term heap, and the batch is
// chunked at ATP_GNN_B_CAP / capped at ATP_GNN_N_CAP nodes.
fn int thvm_atp_gnn_score_batch(const Term *lhs, const Term *rhs,
                                u32 n, float *out_scores) {
  if (!g_atp_gnn_active || n == 0u || lhs == NULL || rhs == NULL
      || out_scores == NULL) {
    return 0;
  }
  const AtpGnnScorer *m = &g_atp_gnn;

  // Extract every CP graph in the engine context (reads the engine's CP
  // terms).  gs[] is plain memory, so it survives the context switch below.
  AtpCpGraph *gs = (AtpCpGraph *)malloc((size_t)n * sizeof(AtpCpGraph));
  if (gs == NULL) return 0;
  for (u32 i = 0u; i < n; i++) {
    // No goal context here (the M2 anchor scores bare Term pairs); the
    // goal-relevance columns stay 0, matching the offline parity harness.
    if (!thvm_atp_cp_graph(NULL, lhs[i], rhs[i], &gs[i])) {
      free(gs); return 0;
    }
  }

  // Switch to the sandbox context; reclaim prior scratch when near the
  // descriptor caps (amortised to ~one reset per thousands of re-ranks).
  u32 prev = thvm_context_current();
  if (g_atp_gnn_ctx == 0u) g_atp_gnn_ctx = thvm_context_create("cpu");
  if (g_atp_gnn_ctx == 0u) { free(gs); return 0; }
  thvm_context_select(g_atp_gnn_ctx);
  if (TENS_NEXT > TENS_CAP / 2u || KERNELS_NEXT > KERNELS_CAP / 2u) {
    // thvm_reset frees the sandbox buffers + every capture slot, so the
    // cached forward captures are now stale -- drop them before reset so
    // the next re-rank at each bucket re-captures from scratch.
    atp_gnn_capture_cache_clear();
    thvm_reset();
  }

  int ok = 1;
  for (u32 off = 0u; ok && off < n; off += ATP_GNN_B_CAP) {
    u32 nc = (n - off < ATP_GNN_B_CAP) ? (n - off) : ATP_GNN_B_CAP;
    ok = atp_gnn_score_chunk(m, gs + off, nc, out_scores + off);
  }

  thvm_context_select(prev);
  free(gs);
  return ok;
}

// Score a batch of PRE-BUILT graphs with the loaded GNN -- the same
// sandbox-context + chunked forward as thvm_atp_gnn_score_batch, minus
// the per-CP thvm_atp_cp_graph extraction (the caller supplies gs[]).
// The offline parity harness uses this to score exported graphs by the
// exact in-engine forward.
fn int thvm_atp_gnn_score_graphs(const AtpCpGraph *gs, u32 n,
                                 float *out_scores) {
  if (!g_atp_gnn_active || n == 0u || gs == NULL || out_scores == NULL) {
    return 0;
  }
  const AtpGnnScorer *m = &g_atp_gnn;
  u32 prev = thvm_context_current();
  if (g_atp_gnn_ctx == 0u) g_atp_gnn_ctx = thvm_context_create("cpu");
  if (g_atp_gnn_ctx == 0u) return 0;
  thvm_context_select(g_atp_gnn_ctx);
  if (TENS_NEXT > TENS_CAP / 2u || KERNELS_NEXT > KERNELS_CAP / 2u) {
    atp_gnn_capture_cache_clear();
    thvm_reset();
  }
  int ok = 1;
  for (u32 off = 0u; ok && off < n; off += ATP_GNN_B_CAP) {
    u32 nc = (n - off < ATP_GNN_B_CAP) ? (n - off) : ATP_GNN_B_CAP;
    ok = atp_gnn_score_chunk(m, gs + off, nc, out_scores + off);
  }
  thvm_context_select(prev);
  return ok;
}

// Parental pre-filter: pick the heap positions of the CPs to GNN-score
// this re-rank.  When gnn_topk > 0 and the queue is larger, keep only the
// top gnn_topk by the CHEAP score -- the Tier-1 logit if that model is
// loaded, else the classical cp_pri read straight off the slot with NO
// term unpack -- via a bounded top-k min-heap (root = worst kept).  This
// bounds the GNN forward to O(gnn_topk) graphs no matter how large the
// live queue grows.  Fills pos[0..return) with heap positions; returns the
// count.  topk == 0 or nq <= topk keeps the whole queue in heap order.
static u32 atp_gnn_rerank_select(AtpState *s, u32 nq, u32 topk, u32 *pos) {
  if (topk == 0u || nq <= topk) {
    for (u32 i = 0u; i < nq; i++) pos[i] = i;
    return nq;
  }
  int tier1 = g_atp_learned_active;
  float *hg = (float *)malloc((size_t)topk * sizeof(float));  // goodness
  u32   *hp = (u32 *)malloc((size_t)topk * sizeof(u32));       // position
  if (hg == NULL || hp == NULL) {              // OOM: whole queue in order
    free(hg); free(hp);
    for (u32 i = 0u; i < nq; i++) pos[i] = i;
    return nq;
  }
  u32 hn = 0u;
  for (u32 i = 0u; i < nq; i++) {
    u32 vv = s->cp_heap[i].row;
    // Higher goodness = more worth GNN-scoring.  cp_pri is "lower is
    // better", so negate it; the Tier-1 logit is already "higher better".
    float good;
    if (tier1) {
      Term l = 0, r = 0;
      atp_cp_slot_read(s, vv, &l, &r);
      float feat[ATP_CP_FEATURE_DIM];
      thvm_atp_cp_features(s, l, r, s->cp_seq[vv], feat);
      good = atp_learned_forward(&g_atp_learned, feat);
    } else {
      good = -(float)s->cp_pri[vv];
    }
    if (hn < topk) {                           // grow, sift up
      u32 c = hn++;
      hg[c] = good; hp[c] = i;
      while (c > 0u) {
        u32 p = (c - 1u) / 2u;
        if (hg[p] <= hg[c]) break;
        float tg = hg[p]; hg[p] = hg[c]; hg[c] = tg;
        u32   tp = hp[p]; hp[p] = hp[c]; hp[c] = tp;
        c = p;
      }
    } else if (good > hg[0]) {                  // evict worst, sift down
      hg[0] = good; hp[0] = i;
      u32 c = 0u;
      for (;;) {
        u32 l2 = 2u * c + 1u, r2 = 2u * c + 2u, mn = c;
        if (l2 < topk && hg[l2] < hg[mn]) mn = l2;
        if (r2 < topk && hg[r2] < hg[mn]) mn = r2;
        if (mn == c) break;
        float tg = hg[mn]; hg[mn] = hg[c]; hg[c] = tg;
        u32   tp = hp[mn]; hp[mn] = hp[c]; hp[c] = tp;
        c = mn;
      }
    }
  }
  for (u32 i = 0u; i < hn; i++) pos[i] = hp[i];
  free(hg); free(hp);
  return hn;
}

// M3: re-rank the live CP queue with the loaded GNN.  Select -> build ->
// score -> map to priority -> push back.  Only permutes order
// (completeness safe).  The parental pre-filter (atp_gnn_rerank_select)
// caps the GNN forward at gnn_topk graphs regardless of queue size, and
// thvm_atp_cp_graph reads the goal cone via `s` so the online graphs carry
// the same goal-relevance columns the training corpus does.
fn u32 thvm_atp_gnn_rerank(AtpState *s) {
  if (s == NULL || !g_atp_gnn_active) return 0u;
  u32 nq = thvm_atp_queued_cp_count(s);
  if (nq == 0u) return 0u;

  u32 topk = s->gnn_topk;
  u32 m = (topk > 0u && nq > topk) ? topk : nq;   // CPs actually GNN-scored

  u32        *pos = (u32 *)malloc((size_t)nq * sizeof(u32));    // selection
  AtpCpGraph *gs  = (AtpCpGraph *)malloc((size_t)m * sizeof(AtpCpGraph));
  u32        *seq = (u32 *)malloc((size_t)m * sizeof(u32));
  float      *sc  = (float *)malloc((size_t)m * sizeof(float));
  u32        *pri = (u32 *)malloc((size_t)m * sizeof(u32));
  if (pos == NULL || gs == NULL || seq == NULL || sc == NULL || pri == NULL) {
    free(pos); free(gs); free(seq); free(sc); free(pri);
    return 0u;
  }

  u32 nsel = atp_gnn_rerank_select(s, nq, topk, pos);
  // Unpack the selected CPs and build their goal-aware graphs in the
  // engine context; skip any that overflow the node cap, compacting into
  // gs / seq.
  u32 k = 0u;
  for (u32 i = 0u; i < nsel; i++) {
    u32 vv = s->cp_heap[pos[i]].row;
    Term l = 0, r = 0;
    atp_cp_slot_read(s, vv, &l, &r);
    if (thvm_atp_cp_graph(s, l, r, &gs[k])) { seq[k] = s->cp_seq[vv]; k++; }
  }
  if (k == 0u || !thvm_atp_gnn_score_graphs(gs, k, sc)) {
    free(pos); free(gs); free(seq); free(sc); free(pri);
    return 0u;
  }
  // Higher score -> lower priority, clamped to a safe u32 band - the same
  // map atp_cp_learned_priority + atpRerankPriority use.  Neutral (score
  // 0) maps to 1.0e6, the cp_pri2 push-time default in coop mode.
  for (u32 i = 0u; i < k; i++) {
    float pr = 1.0e6f - 1.0e4f * sc[i];
    if (pr < 0.0f) pr = 0.0f;
    if (pr > 2.0e9f) pr = 2.0e9f;
    pri[i] = (u32)pr;
  }
  // Coop mode: write the SECONDARY dimension (cp_pri2) so the GNN drives
  // the w2 coop pick while the primary heap stays the hand heuristic
  // (e.g. Waldmeister).  Otherwise overwrite the primary heap (GT-mode).
  if (s->gnn_coop) {
    thvm_atp_set_cp_pri2_by_seq(s, seq, pri, k);
  } else {
    thvm_atp_set_cp_pri_by_seq(s, seq, pri, k);
  }
  s->n_gnn_reranks++;

  free(pos); free(gs); free(seq); free(sc); free(pri);
  return k;
}

fn void thvm_atp_set_gnn_rerank_period(AtpState *s, u32 period) {
  if (s == NULL) return;
  s->gnn_rerank_period = period;
}

fn void thvm_atp_set_gnn_topk(AtpState *s, u32 topk) {
  if (s == NULL) return;
  s->gnn_topk = topk;
}

// Enable GNN coop: the re-rank writes the secondary dimension (cp_pri2)
// and select_cp picks the GNN's top CP every `ratio`-th selection (the w2
// coop branch), while the primary heap keeps the hand heuristic.  ratio 0
// disables coop (GNN overwrites the primary heap).  Sets w2_modulo so the
// coop pick fires; the caller still sets gnn_rerank_period (how often the
// GNN re-scores) and loads the scorer.
fn void thvm_atp_set_gnn_coop(AtpState *s, u32 ratio) {
  if (s == NULL) return;
  if (ratio == 0u) { s->gnn_coop = 0u; return; }
  s->gnn_coop  = 1u;
  s->w2_modulo = ratio;
}

static u32 atp_cp_learned_priority(AtpState *s, Term lhs, Term rhs) {
  float feat[ATP_CP_FEATURE_DIM];
  thvm_atp_cp_features(s, lhs, rhs, s->cp_seq_next, feat);
  float score;
  if (g_atp_learned_active) {
    score = atp_learned_forward(&g_atp_learned, feat);
  } else {
    score = ATP_LEARNED_B;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) score += ATP_LEARNED_W[i] * feat[i];
  }
  // Map score (typically ~[-6, 4]) to a u32 priority, higher score ->
  // lower priority.  Clamp into a safe positive band.
  float pr = 1.0e6f - 1.0e4f * score;
  if (pr < 0.0f) pr = 0.0f;
  if (pr > 2.0e9f) pr = 2.0e9f;
  return (u32)pr;
}

static u32 atp_cp_priority(AtpState *s, Term lhs, Term rhs) {
  if (s->cp_weight_mode == ATP_CP_WEIGHT_LEARNED) {
    return atp_cp_learned_priority(s, lhs, rhs);
  }
  return atp_cp_priority_sized(s, lhs, rhs,
                               atp_symbol_count(lhs) + atp_symbol_count(rhs));
}

