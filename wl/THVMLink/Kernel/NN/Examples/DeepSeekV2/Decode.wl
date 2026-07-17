(* Decode.wl -- DeepSeek-V2-Lite BS=1 decode step for thvm (Stage 1: synthetic weights).

   Port of tinygrad examples/deepseek_v2.py (the fused decode recipe: absorbed MLA
   latent cache + MLA3 4-kernel attention + fully-fused MoE + device-length state),
   spec at /Users/swish/src/tinygrad-dsv2/examples/deepseek_v2.py; every custom
   kernel cites its source lines.  The step is:

       embed_row -> 27 x [ RMSNorm -> q/kv projections (native bf16 matmuls)
                            -> rope_dev(q_pe) -> rope_dev(k_pe) -> cache_append
                            -> mla_qlat/mla_score/mla_mix/mla_o -> o_proj matmul
                            -> RMSNorm -> gate matmul -> route -> moe_full_h/out ]
       -> RMSNorm -> lm_head matmul -> argmax_part/argmax_fin -> len_bump

   Design decisions vs the tinygrad source:
     * Projections (q/kv/o/gate/lm_head/dense MLP) stay NATIVE matmuls: tinygrad
       measured hand GEMVs losing to the tiled projection path (deepseek_v2.py:1002-1007,
       the MEGA regression note), so only the ops BEAM cannot emit are custom kernels.
     * qc is passed to the MLA kernels as SPLIT q_nope/q_pe buffers and `cur` as split
       ckv/kpe buffers (tinygrad concatenated them only to cut buffer count,
       deepseek_v2.py:562; the Stage-0 bridge takes them as separate args, dropping
       the per-step cat kernels and the anchor workaround of deepseek_v2.py:1060-1068).
     * All custom->custom intermediates live in PERSISTENT scratch buffers (the
       tinygrad planner lesson, deepseek_v2.py:844-850); one shared set serves all
       layers because the captured step replays strictly in order.
     * Device-length state: the position lives in an int32 device buffer (the Ltok
       pattern, deepseek_v2.py:166-219); a len_bump kernel (+1) ends the step, and
       argmax writes the next token id back into the device token buffer, so a
       captured step replays with ZERO host values.
     * Three kernels tinygrad does natively are custom here (no on-device topk /
       gather / argmax surface in thvm): route (top-6 of 64 gate scores),
       embed_row (embedding row gather), argmax_lastpos (2-pass argmax over the
       vocab: argmax_part + argmax_fin).
     * Per-backend sources: the CUDA kernels are near-verbatim ports; each has a
       plain-C functional twin for the CPU custom-kernel ABI
       (void name(void *const *bufs, unsigned const *grid, unsigned const *block)).

   Everything is sized from a config association, so a scaled-down variant runs on
   the CPU backend (tests / local bench) and the full model shape on CUDA. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

dsvConfig::usage = "dsvConfig[opts] returns the DeepSeek-V2-Lite decode config association (defaults = the real model constants, tinygrad examples/deepseek_v2.py:26-29); override any key as an option, e.g. dsvConfig[\"NLayers\" -> 2, \"Dim\" -> 64].";
dsvSynthInit::usage = "dsvSynthInit[cfg] allocates synthetic bf16 weights, per-layer latent caches, persistent scratch, and the device token/length state for cfg, returning the decode state association (prefill-free: caches empty, position 0, token id 1).";
dsvHfWeights::usage = "dsvHfWeights[cfg, sd] maps a HuggingFace DeepSeek-V2-Lite checkpoint association sd (name -> TTerm, e.g. from TSafeTensorLoad of the sharded snapshot's index.json) into the decode weight layouts: nn.Linear {out,in} matrices transposed to the {in,out} the matmuls consume, kv_b_proj absorb-split into flat w_uk/w_uv (tinygrad deepseek_v2.py:993-997), per-layer experts stacked into flat w_gate/w_up/w_down (deepseek_v2.py:1266-1271), and norm/gate weights widened to f32.";
dsvHfInit::usage = "dsvHfInit[cfg, sd] returns the decode state association over the real checkpoint weights dsvHfWeights[cfg, sd] (caches empty, position 0, token id 1); the pod-scale twin is the C bench's DSV2_WEIGHTS pack loader (tools/dsv2_pack_weights.py + tools/dsv2_decode_bench.c).";
dsvDecodeStep::usage = "dsvDecodeStep[state] builds and runs one BS=1 decode step over state's device buffers and returns {token, logits} TTerms; the greedy next token is written on-device into state[\"Tok\"] and the device length advances by 1.";
dsvDecodeJit::usage = "dsvDecodeJit[state] returns a zero-argument TJit closure over dsvDecodeStep[state]: the first call captures the step, later calls replay it (each replay = one decode step, no host values in the loop).";
dsvReset::usage = "dsvReset[state] rewinds the device decode state to a fresh conversation (length 1, token id 1) in place, so the captured step and its buffers are reused across requests; stale cache rows beyond the length are never read.";
dsvGenerate::usage = "dsvGenerate[state, promptIds] or dsvGenerate[state, promptIds, opts] rewinds state, prefills the 0-indexed token id list promptIds through the captured decode step, then greedily decodes further tokens, returning the generated ids (prompt excluded). Options: \"MaxTokens\" (default 256) caps new tokens; \"EosId\" (default the DeepSeek-V2 end-of-sentence id 100001) stops decoding when produced.";
dsvChatPrompt::usage = "dsvChatPrompt[messages] renders a chat message list into the DeepSeek-V2-Chat prompt string (the model's tokenizer_config chat template): a begin-of-sentence marker, then each message as \"User: ...\", \"Assistant: ...\" (closed by the end-of-sentence marker), or a bare system line, ending with the \"Assistant:\" generation cue. Each message is an association with \"Role\" (\"user\"/\"assistant\"/\"system\") and \"Content\"; a bare string is taken as a single user turn.";
dsvKernels::usage = "dsvKernels[cfg] returns the association of TKernel custom-kernel objects for cfg (CUDA sources on \"cuda\", plain-C twins on \"cpu\").";
dsvRefInit::usage = "dsvRefInit[state] returns the host-side (machine-precision) reference decode state over the SAME synthetic weights: an independent implementation of the step math used to validate the fused pipeline.";
dsvRefStep::usage = "dsvRefStep[ref, tokenId] runs one reference decode step for input tokenId and returns <|\"State\" -> ref', \"Logits\" -> logits, \"Token\" -> argmax|>.";
dsvToken::usage = "dsvToken[state] reads the current device token id (the last argmax result).";
dsvPosition::usage = "dsvPosition[state] reads the device length buffer (tokens decoded so far + 1).";

Begin["`Private`"];

(* === config ===================================================== *)

(* deepseek-ai/DeepSeek-V2-Lite config.json (tinygrad examples/deepseek_v2.py:26-29). *)
Options[dsvConfig] = {
    "Dim" -> 2048,
    "NLayers" -> 27,
    "NHeads" -> 16,
    "KvLora" -> 512,
    "DNope" -> 128,
    "DRope" -> 64,
    "DV" -> 128,
    "NExperts" -> 64,
    "NShared" -> 2,
    "TopK" -> 6,
    "MoeInter" -> 1408,
    "DenseInter" -> 10944,
    "FirstDense" -> 1,
    "Vocab" -> 102400,
    "Eps" -> 1.*^-6,
    "RopeTheta" -> 10000.,
    "MaxSeq" -> 1024,
    "YarnFactor" -> 40.,
    "YarnOrig" -> 4096,
    "YarnBetaFast" -> 32.,
    "YarnBetaSlow" -> 1.,
    "YarnMscale" -> 0.707,
    "Device" -> "cpu",
    "Seed" -> 7,
    "WeightScale" -> 0.05
}

(* Derived: SharedInter, the yarn-corrected attention scale (deepseek_v2.py:549-550,
   953), and the argmax partial-block count. *)
dsvConfig[opts : OptionsPattern[]] := Block[{c, mscale},
    c = Association[Options[dsvConfig], {opts}];
    mscale = If[c["YarnFactor"] <= 1, 1., 0.1 * c["YarnMscale"] * Log[c["YarnFactor"]] + 1.];
    c["SharedInter"] = c["MoeInter"] * c["NShared"];
    c["Scale"] = N[(c["DNope"] + c["DRope"])^-0.5 * mscale^2];
    c["ArgmaxBlocks"] = Min[256, Ceiling[c["Vocab"] / 256]];
    c
]

(* === YaRN rope tables (deepseek_v2.py:955-967) =================== *)

dsvYarnFreqs[cfg_] := Block[{d, theta, factor, orig, posFreqs, invExtra, invInter, corrDim, lo, hi, ramp},
    d = cfg["DRope"]; theta = cfg["RopeTheta"]; factor = cfg["YarnFactor"]; orig = cfg["YarnOrig"];
    posFreqs = theta^(Range[0., d - 2, 2] / d);
    invExtra = 1. / posFreqs;
    invInter = 1. / (factor * posFreqs);
    corrDim = r |-> (d * Log[orig / (r * 2 * Pi)]) / (2 * Log[theta]);
    lo = Max[Floor @ corrDim[cfg["YarnBetaFast"]], 0];
    hi = Min[Ceiling @ corrDim[cfg["YarnBetaSlow"]], d - 1];
    ramp = Clip[(Range[0., d/2 - 1] - lo) / Max[hi - lo, 1.*^-3], {0., 1.}];
    invInter * ramp + invExtra * (1 - ramp)
]

(* cos/sin tables {maxSeq, dRope} f32, halves duplicated so cos[[p, j]] is the
   pair-j cos for j <= dRope/2 (deepseek_v2.py:964-967). *)
dsvRopeTables[cfg_] := Block[{f, emb},
    f = Outer[Times, Range[0., cfg["MaxSeq"] - 1], dsvYarnFreqs[cfg]];
    emb = Join[f, f, 2];
    {Cos[emb], Sin[emb]}
]

(* === custom-kernel sources ======================================= *)

(* Both dialects carry their own bf16<->f32 converters (bit-identical to
   src/dtype/fp_convert.c:113-136: bf16 = high 16 f32 bits; f32->bf16 is
   round-to-nearest-even with quiet-NaN preservation), so the CUDA sources
   need no cuda_bf16.h include (nvrtc include-path independence). *)
$dsvCudaPre = "
typedef unsigned short bf16t;
static __device__ __forceinline__ float bf2f(bf16t h) { return __uint_as_float(((unsigned int)h) << 16); }
static __device__ __forceinline__ bf16t f2bf(float f) {
  unsigned int b = __float_as_uint(f);
  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u)
    return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);
  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16);
}
"

$dsvCpuPre = "
typedef unsigned short bf16t;
static inline float bf2f(bf16t h) { union { unsigned int u; float f; } v; v.u = ((unsigned int)h) << 16; return v.f; }
static inline bf16t f2bf(float f) {
  union { unsigned int u; float f; } v; v.f = f;
  unsigned int b = v.u;
  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u)
    return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);
  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16);
}
"

(* Metal Shading Language preamble: same bit-exact bf16<->f32 converters as the
   CUDA/CPU pres (bf16 = high 16 f32 bits; f32->bf16 is round-to-nearest-even
   with quiet-NaN preservation), using MSL's as_type<> for the reinterpret
   casts.  The metal3.1 `bfloat` type is deliberately NOT used: the manual bit
   ops keep the rounding byte-identical to the other two dialects. *)
$dsvMetalPre = "
#include <metal_stdlib>
using namespace metal;
typedef ushort bf16t;
static inline float bf2f(bf16t h) { return as_type<float>(((uint)h) << 16); }
static inline bf16t f2bf(float f) {
  uint b = as_type<uint>(f);
  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u)
    return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);
  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16);
}
"

(* Template parameter strings for a config.  `MI` = MoeInter (avoids the WL
   symbol I), `NB`/`CHUNK` size the 2-pass argmax. *)
dsvTemplateParams[cfg_] := <|
    "D" -> ToString @ cfg["Dim"],
    "H" -> ToString @ cfg["NHeads"],
    "DN" -> ToString @ cfg["DNope"],
    "DR" -> ToString @ cfg["DRope"],
    "KV" -> ToString @ cfg["KvLora"],
    "DV" -> ToString @ cfg["DV"],
    "MAXL" -> ToString @ cfg["MaxSeq"],
    "E" -> ToString @ cfg["NExperts"],
    "K" -> ToString @ cfg["TopK"],
    "MI" -> ToString @ cfg["MoeInter"],
    "SI" -> ToString @ cfg["SharedInter"],
    "V" -> ToString @ cfg["Vocab"],
    "NB" -> ToString @ cfg["ArgmaxBlocks"],
    "CHUNK" -> ToString @ Ceiling[cfg["Vocab"] / cfg["ArgmaxBlocks"]],
    "SCALE" -> ToString @ CForm @ cfg["Scale"]
|>

(* --- embed_row: x[i] = emb[tok[0]*D + i] (bf16 row gather; replaces
       tinygrad's nn.Embedding lookup, deepseek_v2.py:1226) --- *)
$dsvEmbedCuda = "
extern \"C\" __global__ void embed_row(bf16t* x, const bf16t* emb, const int* tok) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < `D`) x[i] = emb[(long)tok[0] * `D` + i];
}
"

$dsvEmbedCpu = "
void embed_row(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  bf16t *x = (bf16t *)bufs[0];
  const bf16t *emb = (const bf16t *)bufs[1];
  const int *tok = (const int *)bufs[2];
  for (int i = 0; i < `D`; i++) x[i] = emb[(long)tok[0] * `D` + i];
}
"

(* --- rope_dev: io [NH, DR] roped IN PLACE with the HF de-interleaved pair
       convention at device position L[0]-1 (S=1, OFFM1=1); port of
       deepseek_v2.py:180-189.  NHS is a template slot so the same source
       serves q_pe (NH heads) and k_pe (1 row). --- *)
$dsvRopeCuda = "
extern \"C\" __global__ void rope_dev(bf16t* io, const float* cs, const float* sn, const int* L) {
  int row = blockIdx.x; int j = threadIdx.x;
  if (row >= `NHS` || j >= `DR` / 2) return;
  long base = (long)row * `DR`; int p = L[0] - 1;
  float xe = bf2f(io[base + 2 * j]), xo = bf2f(io[base + 2 * j + 1]);
  float c = cs[(long)p * `DR` + j], sv = sn[(long)p * `DR` + j];
  __syncthreads();
  io[base + j]            = f2bf(xe * c - xo * sv);
  io[base + `DR` / 2 + j] = f2bf(xo * c + xe * sv);
}
"

$dsvRopeCpu = "
void rope_dev(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  bf16t *io = (bf16t *)bufs[0];
  const float *cs = (const float *)bufs[1];
  const float *sn = (const float *)bufs[2];
  const int *L = (const int *)bufs[3];
  int p = L[0] - 1;
  for (int row = 0; row < `NHS`; row++) {
    long base = (long)row * `DR`;
    float tmp[`DR`];
    for (int j = 0; j < `DR` / 2; j++) {
      float xe = bf2f(io[base + 2 * j]), xo = bf2f(io[base + 2 * j + 1]);
      float c = cs[(long)p * `DR` + j], sv = sn[(long)p * `DR` + j];
      tmp[j]            = xe * c - xo * sv;
      tmp[`DR` / 2 + j] = xo * c + xe * sv;
    }
    for (int j = 0; j < `DR`; j++) io[base + j] = f2bf(tmp[j]);
  }
}
"

(* --- cache_append: write this step's latent row at offset L[0]-1
       (port of deepseek_v2.py:192-197, S=1, OFFM1=1) --- *)
$dsvAppendCuda = "
extern \"C\" __global__ void cache_append(bf16t* ckv, bf16t* kpe, const bf16t* ckv_new, const bf16t* kpe_new, const int* L) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x; int off = L[0] - 1;
  if (idx < `KV`) ckv[(long)off * `KV` + idx] = ckv_new[idx];
  else if (idx < `KV` + `DR`) kpe[(long)off * `DR` + (idx - `KV`)] = kpe_new[idx - `KV`];
}
"

$dsvAppendCpu = "
void cache_append(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  bf16t *ckv = (bf16t *)bufs[0];
  bf16t *kpe = (bf16t *)bufs[1];
  const bf16t *ckv_new = (const bf16t *)bufs[2];
  const bf16t *kpe_new = (const bf16t *)bufs[3];
  const int *L = (const int *)bufs[4];
  int off = L[0] - 1;
  for (int i = 0; i < `KV`; i++) ckv[(long)off * `KV` + i] = ckv_new[i];
  for (int i = 0; i < `DR`; i++) kpe[(long)off * `DR` + i] = kpe_new[i];
}
"

(* --- MLA3 4-kernel chain (port of deepseek_v2.py:651-717; qc split into
       q_nope/q_pe buffers and cur split into ckvc/kpec, see header note) --- *)
$dsvQlatCuda = "
extern \"C\" __global__ void mla_qlat(float* qlat, const bf16t* qn, const bf16t* wuk) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= `H` * `KV`) return;
  int h = idx / `KV`, k = idx % `KV`;
  const bf16t* q = qn + (long)h * `DN`;
  const bf16t* w = wuk + (long)h * `DN` * `KV`;
  float acc = 0.f;
  for (int d = 0; d < `DN`; d++) acc += bf2f(q[d]) * bf2f(w[(long)d * `KV` + k]);
  qlat[(long)h * `KV` + k] = acc;
}
"

$dsvQlatCpu = "
void mla_qlat(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  float *qlat = (float *)bufs[0];
  const bf16t *qn = (const bf16t *)bufs[1];
  const bf16t *wuk = (const bf16t *)bufs[2];
  for (int h = 0; h < `H`; h++)
    for (int k = 0; k < `KV`; k++) {
      float acc = 0.f;
      for (int d = 0; d < `DN`; d++)
        acc += bf2f(qn[(long)h * `DN` + d]) * bf2f(wuk[((long)h * `DN` + d) * `KV` + k]);
      qlat[(long)h * `KV` + k] = acc;
    }
}
"

$dsvScoreCuda = "
extern \"C\" __global__ void mla_score(float* sm, const float* qlat, const bf16t* qp,
    const bf16t* Cn, const bf16t* Pn, const bf16t* ckvc, const bf16t* kpec, const int* Lp) {
  int h = blockIdx.x; int t = threadIdx.x; int nt = blockDim.x; int L = Lp[0]; int Lh = L - 1;
  const bf16t* q_pe = qp + (long)h * `DR`;
  const float* ql = qlat + (long)h * `KV`;
  __shared__ float sc[`MAXL`]; __shared__ float red[32];
  int lane = t & 31, w = t >> 5, nw = nt >> 5;
  for (int j = w; j < L; j += nw) {
    const bf16t* cj = (j < Lh) ? (Cn + (long)j * `KV`) : ckvc;
    const bf16t* pj = (j < Lh) ? (Pn + (long)j * `DR`) : kpec;
    float s = 0.f;
    for (int k = lane; k < `KV`; k += 32) s += ql[k] * bf2f(cj[k]);
    for (int k = lane; k < `DR`; k += 32) s += bf2f(q_pe[k]) * bf2f(pj[k]);
    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);
    if (lane == 0) sc[j] = s * `SCALE`f;
  }
  __syncthreads();
  float m = -1e30f;
  for (int j = t; j < L; j += nt) m = fmaxf(m, sc[j]);
  for (int o = 16; o > 0; o >>= 1) m = fmaxf(m, __shfl_down_sync(0xffffffffu, m, o));
  if (lane == 0) red[w] = m;
  __syncthreads();
  if (t == 0) { float mm = -1e30f; for (int i = 0; i < (nt + 31) / 32; i++) mm = fmaxf(mm, red[i]); red[0] = mm; }
  __syncthreads(); float gmax = red[0]; __syncthreads();
  float sum = 0.f;
  for (int j = t; j < L; j += nt) { float e = __expf(sc[j] - gmax); sc[j] = e; sum += e; }
  for (int o = 16; o > 0; o >>= 1) sum += __shfl_down_sync(0xffffffffu, sum, o);
  if (lane == 0) red[w] = sum;
  __syncthreads();
  if (t == 0) { float ss = 0.f; for (int i = 0; i < (nt + 31) / 32; i++) ss += red[i]; red[0] = ss; }
  __syncthreads(); float gsum = red[0]; __syncthreads();
  for (int j = t; j < L; j += nt) sm[(long)h * `MAXL` + j] = sc[j] / gsum;
}
"

$dsvScoreCpu = "
#include <math.h>
void mla_score(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  float *sm = (float *)bufs[0];
  const float *qlat = (const float *)bufs[1];
  const bf16t *qp = (const bf16t *)bufs[2];
  const bf16t *Cn = (const bf16t *)bufs[3];
  const bf16t *Pn = (const bf16t *)bufs[4];
  const bf16t *ckvc = (const bf16t *)bufs[5];
  const bf16t *kpec = (const bf16t *)bufs[6];
  const int *Lp = (const int *)bufs[7];
  int L = Lp[0], Lh = L - 1;
  for (int h = 0; h < `H`; h++) {
    float sc[`MAXL`];
    float gmax = -1e30f;
    for (int j = 0; j < L; j++) {
      const bf16t *cj = (j < Lh) ? (Cn + (long)j * `KV`) : ckvc;
      const bf16t *pj = (j < Lh) ? (Pn + (long)j * `DR`) : kpec;
      float s = 0.f;
      for (int k = 0; k < `KV`; k++) s += qlat[(long)h * `KV` + k] * bf2f(cj[k]);
      for (int k = 0; k < `DR`; k++) s += bf2f(qp[(long)h * `DR` + k]) * bf2f(pj[k]);
      sc[j] = s * `SCALE`f;
      if (sc[j] > gmax) gmax = sc[j];
    }
    float gsum = 0.f;
    for (int j = 0; j < L; j++) { sc[j] = expf(sc[j] - gmax); gsum += sc[j]; }
    for (int j = 0; j < L; j++) sm[(long)h * `MAXL` + j] = sc[j] / gsum;
  }
}
"

$dsvMixCuda = "
extern \"C\" __global__ void mla_mix(float* olat, const float* sm, const bf16t* Cn, const bf16t* ckvc, const int* Lp) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= `H` * `KV`) return;
  int h = idx / `KV`, k = idx % `KV`; int L = Lp[0], Lh = L - 1;
  const float* smh = sm + (long)h * `MAXL`;
  float a = 0.f;
  for (int j = 0; j < Lh; j++) a += smh[j] * bf2f(Cn[(long)j * `KV` + k]);
  a += smh[Lh] * bf2f(ckvc[k]);
  olat[(long)h * `KV` + k] = a;
}
"

$dsvMixCpu = "
void mla_mix(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  float *olat = (float *)bufs[0];
  const float *sm = (const float *)bufs[1];
  const bf16t *Cn = (const bf16t *)bufs[2];
  const bf16t *ckvc = (const bf16t *)bufs[3];
  const int *Lp = (const int *)bufs[4];
  int L = Lp[0], Lh = L - 1;
  for (int h = 0; h < `H`; h++)
    for (int k = 0; k < `KV`; k++) {
      float a = 0.f;
      for (int j = 0; j < Lh; j++) a += sm[(long)h * `MAXL` + j] * bf2f(Cn[(long)j * `KV` + k]);
      a += sm[(long)h * `MAXL` + Lh] * bf2f(ckvc[k]);
      olat[(long)h * `KV` + k] = a;
    }
}
"

$dsvOCuda = "
extern \"C\" __global__ void mla_o(bf16t* o, const float* olat, const bf16t* wuv) {
  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;
  if (warp >= `H` * `DV`) return;
  int h = warp / `DV`, mm = warp % `DV`;
  const float* ol = olat + (long)h * `KV`;
  const bf16t* wm = wuv + ((long)h * `DV` + mm) * `KV`;
  float acc = 0.f;
  for (int k = lane; k < `KV`; k += 32) acc += ol[k] * bf2f(wm[k]);
  for (int oo = 16; oo > 0; oo >>= 1) acc += __shfl_down_sync(0xffffffffu, acc, oo);
  if (lane == 0) o[(long)h * `DV` + mm] = f2bf(acc);
}
"

$dsvOCpu = "
void mla_o(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  bf16t *o = (bf16t *)bufs[0];
  const float *olat = (const float *)bufs[1];
  const bf16t *wuv = (const bf16t *)bufs[2];
  for (int h = 0; h < `H`; h++)
    for (int mm = 0; mm < `DV`; mm++) {
      float acc = 0.f;
      for (int k = 0; k < `KV`; k++) acc += olat[(long)h * `KV` + k] * bf2f(wuv[((long)h * `DV` + mm) * `KV` + k]);
      o[(long)h * `DV` + mm] = f2bf(acc);
    }
}
"

(* --- route: greedy top-K of the E gate logits + full-softmax probs of the
       selected experts, no renorm (replaces tinygrad's on-device
       softmax+topk, deepseek_v2.py:1141-1143).  E is tiny (64), so a single
       thread is cheap and keeps the selection order deterministic
       (ties -> lowest expert id, matching Ordering on the host). --- *)
$dsvRouteCuda = "
extern \"C\" __global__ void route(int* sel, float* probs, const float* g) {
  if (threadIdx.x != 0 || blockIdx.x != 0) return;
  float mx = g[0];
  for (int e = 1; e < `E`; e++) mx = fmaxf(mx, g[e]);
  float sum = 0.f;
  for (int e = 0; e < `E`; e++) sum += __expf(g[e] - mx);
  int taken[`K`];
  for (int kk = 0; kk < `K`; kk++) {
    int best = -1; float bv = -3.4e38f;
    for (int e = 0; e < `E`; e++) {
      int used = 0;
      for (int j = 0; j < kk; j++) if (taken[j] == e) used = 1;
      if (!used && g[e] > bv) { bv = g[e]; best = e; }
    }
    taken[kk] = best;
    sel[kk] = best;
    probs[kk] = __expf(g[best] - mx) / sum;
  }
}
"

$dsvRouteCpu = "
#include <math.h>
void route(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  int *sel = (int *)bufs[0];
  float *probs = (float *)bufs[1];
  const float *g = (const float *)bufs[2];
  float mx = g[0];
  for (int e = 1; e < `E`; e++) if (g[e] > mx) mx = g[e];
  float sum = 0.f;
  for (int e = 0; e < `E`; e++) sum += expf(g[e] - mx);
  int taken[`K`];
  for (int kk = 0; kk < `K`; kk++) {
    int best = -1; float bv = -3.4e38f;
    for (int e = 0; e < `E`; e++) {
      int used = 0;
      for (int j = 0; j < kk; j++) if (taken[j] == e) used = 1;
      if (!used && g[e] > bv) { bv = g[e]; best = e; }
    }
    taken[kk] = best;
    sel[kk] = best;
    probs[kk] = expf(g[best] - mx) / sum;
  }
}
"

(* --- fully-fused MoE: routed top-K + shared experts in TWO kernels
       (verbatim port of moe_full_h / moe_full_out, deepseek_v2.py:445-472) --- *)
$dsvMoeHCuda = "
extern \"C\" __global__ void moe_full_h(float* h, const bf16t* x, const int* sel,
    const bf16t* wg, const bf16t* wu, const bf16t* swg, const bf16t* swu) {
  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;
  if (warp >= `K` * `MI` + `SI`) return;
  const bf16t *wgi, *wui;
  if (warp < `K` * `MI`) {
    int e = warp / `MI`, i = warp % `MI`; int se = sel[e];
    wgi = wg + ((long)se * `MI` + i) * `D`; wui = wu + ((long)se * `MI` + i) * `D`;
  } else {
    int i = warp - `K` * `MI`;
    wgi = swg + (long)i * `D`; wui = swu + (long)i * `D`;
  }
  float g = 0.f, u = 0.f;
  for (int k = lane; k < `D`; k += 32) { float xk = bf2f(x[k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }
  for (int o = 16; o > 0; o >>= 1) { g += __shfl_down_sync(0xffffffffu, g, o); u += __shfl_down_sync(0xffffffffu, u, o); }
  if (lane == 0) { float gs = g / (1.f + __expf(-g)); h[warp] = gs * u; }
}
"

$dsvMoeHCpu = "
#include <math.h>
void moe_full_h(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  float *h = (float *)bufs[0];
  const bf16t *x = (const bf16t *)bufs[1];
  const int *sel = (const int *)bufs[2];
  const bf16t *wg = (const bf16t *)bufs[3];
  const bf16t *wu = (const bf16t *)bufs[4];
  const bf16t *swg = (const bf16t *)bufs[5];
  const bf16t *swu = (const bf16t *)bufs[6];
  for (int warp = 0; warp < `K` * `MI` + `SI`; warp++) {
    const bf16t *wgi, *wui;
    if (warp < `K` * `MI`) {
      int e = warp / `MI`, i = warp % `MI`; int se = sel[e];
      wgi = wg + ((long)se * `MI` + i) * `D`; wui = wu + ((long)se * `MI` + i) * `D`;
    } else {
      int i = warp - `K` * `MI`;
      wgi = swg + (long)i * `D`; wui = swu + (long)i * `D`;
    }
    float g = 0.f, u = 0.f;
    for (int k = 0; k < `D`; k++) { float xk = bf2f(x[k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }
    float gs = g / (1.f + expf(-g));
    h[warp] = gs * u;
  }
}
"

$dsvMoeOutCuda = "
extern \"C\" __global__ void moe_full_out(bf16t* out, const float* h, const int* sel,
    const float* probs, const bf16t* wd, const bf16t* swd) {
  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;
  if (warp >= `D`) return;
  int d = warp; float acc = 0.f;
  for (int e = 0; e < `K`; e++) {
    int se = sel[e];
    const bf16t* wde = wd + ((long)se * `D` + d) * `MI`;
    const float* he = h + e * `MI`;
    float s = 0.f;
    for (int i = lane; i < `MI`; i += 32) s += he[i] * bf2f(wde[i]);
    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);
    if (lane == 0) acc += probs[e] * s;
  }
  {
    const bf16t* wde = swd + (long)d * `SI`;
    const float* hs = h + `K` * `MI`;
    float s = 0.f;
    for (int i = lane; i < `SI`; i += 32) s += hs[i] * bf2f(wde[i]);
    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);
    if (lane == 0) acc += s;
  }
  if (lane == 0) out[d] = f2bf(acc);
}
"

$dsvMoeOutCpu = "
void moe_full_out(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  bf16t *out = (bf16t *)bufs[0];
  const float *h = (const float *)bufs[1];
  const int *sel = (const int *)bufs[2];
  const float *probs = (const float *)bufs[3];
  const bf16t *wd = (const bf16t *)bufs[4];
  const bf16t *swd = (const bf16t *)bufs[5];
  for (int d = 0; d < `D`; d++) {
    float acc = 0.f;
    for (int e = 0; e < `K`; e++) {
      int se = sel[e];
      float s = 0.f;
      for (int i = 0; i < `MI`; i++) s += h[e * `MI` + i] * bf2f(wd[((long)se * `D` + d) * `MI` + i]);
      acc += probs[e] * s;
    }
    {
      float s = 0.f;
      for (int i = 0; i < `SI`; i++) s += h[`K` * `MI` + i] * bf2f(swd[(long)d * `SI` + i]);
      acc += s;
    }
    out[d] = f2bf(acc);
  }
}
"

(* --- argmax_lastpos, 2-pass: argmax_part reduces per-block chunks of the
       vocab to (max, idx) partials; argmax_fin folds the partials and writes
       the winning index into the device token buffer.  Ties resolve to the
       LOWEST index (matches host Ordering).  Replaces tinygrad's native
       lg[:, -1].argmax(-1) (deepseek_v2.py:1255). --- *)
$dsvArgPartCuda = "
extern \"C\" __global__ void argmax_part(float* pv, int* pi, const float* lg) {
  __shared__ float sv[256]; __shared__ int si[256];
  int b = blockIdx.x; int t = threadIdx.x;
  long lo = (long)b * `CHUNK`; long hi = lo + `CHUNK`; if (hi > `V`) hi = `V`;
  float bv = -3.4e38f; int bi = 2147483647;
  for (long i = lo + t; i < hi; i += blockDim.x) {
    float v = lg[i];
    if (v > bv || (v == bv && (int)i < bi)) { bv = v; bi = (int)i; }
  }
  sv[t] = bv; si[t] = bi;
  __syncthreads();
  for (int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (t < s && (sv[t + s] > sv[t] || (sv[t + s] == sv[t] && si[t + s] < si[t]))) {
      sv[t] = sv[t + s]; si[t] = si[t + s];
    }
    __syncthreads();
  }
  if (t == 0) { pv[b] = sv[0]; pi[b] = si[0]; }
}
"

$dsvArgPartCpu = "
void argmax_part(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  float *pv = (float *)bufs[0];
  int *pi = (int *)bufs[1];
  const float *lg = (const float *)bufs[2];
  for (int b = 0; b < `NB`; b++) {
    long lo = (long)b * `CHUNK`; long hi = lo + `CHUNK`; if (hi > `V`) hi = `V`;
    float bv = -3.4e38f; int bi = 2147483647;
    for (long i = lo; i < hi; i++)
      if (lg[i] > bv) { bv = lg[i]; bi = (int)i; }
    pv[b] = bv; pi[b] = bi;
  }
}
"

$dsvArgFinCuda = "
extern \"C\" __global__ void argmax_fin(int* tok, const float* pv, const int* pi) {
  if (threadIdx.x != 0 || blockIdx.x != 0) return;
  float bv = pv[0]; int bi = pi[0];
  for (int b = 1; b < `NB`; b++)
    if (pv[b] > bv || (pv[b] == bv && pi[b] < bi)) { bv = pv[b]; bi = pi[b]; }
  tok[0] = bi;
}
"

$dsvArgFinCpu = "
void argmax_fin(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  int *tok = (int *)bufs[0];
  const float *pv = (const float *)bufs[1];
  const int *pi = (const int *)bufs[2];
  float bv = pv[0]; int bi = pi[0];
  for (int b = 1; b < `NB`; b++)
    if (pv[b] > bv || (pv[b] == bv && pi[b] < bi)) { bv = pv[b]; bi = pi[b]; }
  tok[0] = bi;
}
"

(* --- len_bump: the device position advances by 1 at step end (the Ltok
       device-length pattern, deepseek_v2.py:166-170) --- *)
$dsvBumpCuda = "
extern \"C\" __global__ void len_bump(int* L) {
  if (threadIdx.x == 0 && blockIdx.x == 0) L[0] = L[0] + 1;
}
"

$dsvBumpCpu = "
void len_bump(void *const *bufs, unsigned const *grid, unsigned const *block) {
  (void)grid; (void)block;
  ((int *)bufs[0])[0] += 1;
}
"

(* === Metal Shading Language ports ================================ *)

(* Faithful line-by-line ports of the $dsv*Cuda kernels to MSL.  Translation:
   extern \"C\" __global__ -> kernel; T* param -> device T* [[buffer(i)]] in the
   SAME order as the CUDA pointer params (slot i = i-th pointer); blockIdx/
   threadIdx/blockDim -> tgid/tid3/ntg (the [[threadgroup_position_in_grid]] /
   [[thread_position_in_threadgroup]] / [[threads_per_threadgroup]] builtins);
   __shared__ T s[N] -> threadgroup T s[N]; __syncthreads() ->
   threadgroup_barrier (mem_threadgroup for the shared-memory reductions;
   rope_dev's in-place io swap orders DEVICE memory, so mem_device);
   __shfl_down_sync(0xffffffffu, v, o) -> simd_shuffle_down(v, o) (Apple simd
   width 32 = the warp assumption); __expf/expf -> exp; fmaxf -> fmax;
   __uint_as_float/__float_as_uint -> as_type (in $dsvMetalPre).  A local
   pointer into a device buffer must carry the `device` address space. *)

$dsvEmbedMetal = "
kernel void embed_row(device bf16t* x [[buffer(0)]], device const bf16t* emb [[buffer(1)]], device const int* tok [[buffer(2)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int i = tgid.x * ntg.x + tid3.x;
  if (i < `D`) x[i] = emb[(long)tok[0] * `D` + i];
}
"

$dsvRopeMetal = "
kernel void rope_dev(device bf16t* io [[buffer(0)]], device const float* cs [[buffer(1)]], device const float* sn [[buffer(2)]], device const int* L [[buffer(3)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]]) {
  int row = tgid.x; int j = tid3.x;
  if (row >= `NHS` || j >= `DR` / 2) return;
  long base = (long)row * `DR`; int p = L[0] - 1;
  float xe = bf2f(io[base + 2 * j]), xo = bf2f(io[base + 2 * j + 1]);
  float c = cs[(long)p * `DR` + j], sv = sn[(long)p * `DR` + j];
  threadgroup_barrier(mem_flags::mem_device);
  io[base + j]            = f2bf(xe * c - xo * sv);
  io[base + `DR` / 2 + j] = f2bf(xo * c + xe * sv);
}
"

$dsvAppendMetal = "
kernel void cache_append(device bf16t* ckv [[buffer(0)]], device bf16t* kpe [[buffer(1)]], device const bf16t* ckv_new [[buffer(2)]], device const bf16t* kpe_new [[buffer(3)]], device const int* L [[buffer(4)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int idx = tgid.x * ntg.x + tid3.x; int off = L[0] - 1;
  if (idx < `KV`) ckv[(long)off * `KV` + idx] = ckv_new[idx];
  else if (idx < `KV` + `DR`) kpe[(long)off * `DR` + (idx - `KV`)] = kpe_new[idx - `KV`];
}
"

$dsvQlatMetal = "
kernel void mla_qlat(device float* qlat [[buffer(0)]], device const bf16t* qn [[buffer(1)]], device const bf16t* wuk [[buffer(2)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int idx = tgid.x * ntg.x + tid3.x;
  if (idx >= `H` * `KV`) return;
  int h = idx / `KV`, k = idx % `KV`;
  device const bf16t* q = qn + (long)h * `DN`;
  device const bf16t* w = wuk + (long)h * `DN` * `KV`;
  float acc = 0.f;
  for (int d = 0; d < `DN`; d++) acc += bf2f(q[d]) * bf2f(w[(long)d * `KV` + k]);
  qlat[(long)h * `KV` + k] = acc;
}
"

$dsvScoreMetal = "
kernel void mla_score(device float* sm [[buffer(0)]], device const float* qlat [[buffer(1)]], device const bf16t* qp [[buffer(2)]],
    device const bf16t* Cn [[buffer(3)]], device const bf16t* Pn [[buffer(4)]], device const bf16t* ckvc [[buffer(5)]], device const bf16t* kpec [[buffer(6)]], device const int* Lp [[buffer(7)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int h = tgid.x; int t = tid3.x; int nt = ntg.x; int L = Lp[0]; int Lh = L - 1;
  device const bf16t* q_pe = qp + (long)h * `DR`;
  device const float* ql = qlat + (long)h * `KV`;
  threadgroup float sc[`MAXL`]; threadgroup float red[32];
  int lane = t & 31, w = t >> 5, nw = nt >> 5;
  for (int j = w; j < L; j += nw) {
    device const bf16t* cj = (j < Lh) ? (Cn + (long)j * `KV`) : ckvc;
    device const bf16t* pj = (j < Lh) ? (Pn + (long)j * `DR`) : kpec;
    float s = 0.f;
    for (int k = lane; k < `KV`; k += 32) s += ql[k] * bf2f(cj[k]);
    for (int k = lane; k < `DR`; k += 32) s += bf2f(q_pe[k]) * bf2f(pj[k]);
    for (int o = 16; o > 0; o >>= 1) s += simd_shuffle_down(s, o);
    if (lane == 0) sc[j] = s * `SCALE`f;
  }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  float m = -1e30f;
  for (int j = t; j < L; j += nt) m = fmax(m, sc[j]);
  for (int o = 16; o > 0; o >>= 1) m = fmax(m, simd_shuffle_down(m, o));
  if (lane == 0) red[w] = m;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if (t == 0) { float mm = -1e30f; for (int i = 0; i < (nt + 31) / 32; i++) mm = fmax(mm, red[i]); red[0] = mm; }
  threadgroup_barrier(mem_flags::mem_threadgroup); float gmax = red[0]; threadgroup_barrier(mem_flags::mem_threadgroup);
  float sum = 0.f;
  for (int j = t; j < L; j += nt) { float e = exp(sc[j] - gmax); sc[j] = e; sum += e; }
  for (int o = 16; o > 0; o >>= 1) sum += simd_shuffle_down(sum, o);
  if (lane == 0) red[w] = sum;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if (t == 0) { float ss = 0.f; for (int i = 0; i < (nt + 31) / 32; i++) ss += red[i]; red[0] = ss; }
  threadgroup_barrier(mem_flags::mem_threadgroup); float gsum = red[0]; threadgroup_barrier(mem_flags::mem_threadgroup);
  for (int j = t; j < L; j += nt) sm[(long)h * `MAXL` + j] = sc[j] / gsum;
}
"

$dsvMixMetal = "
kernel void mla_mix(device float* olat [[buffer(0)]], device const float* sm [[buffer(1)]], device const bf16t* Cn [[buffer(2)]], device const bf16t* ckvc [[buffer(3)]], device const int* Lp [[buffer(4)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int idx = tgid.x * ntg.x + tid3.x;
  if (idx >= `H` * `KV`) return;
  int h = idx / `KV`, k = idx % `KV`; int L = Lp[0], Lh = L - 1;
  device const float* smh = sm + (long)h * `MAXL`;
  float a = 0.f;
  for (int j = 0; j < Lh; j++) a += smh[j] * bf2f(Cn[(long)j * `KV` + k]);
  a += smh[Lh] * bf2f(ckvc[k]);
  olat[(long)h * `KV` + k] = a;
}
"

$dsvOMetal = "
kernel void mla_o(device bf16t* o [[buffer(0)]], device const float* olat [[buffer(1)]], device const bf16t* wuv [[buffer(2)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int warp = (tgid.x * ntg.x + tid3.x) >> 5; int lane = tid3.x & 31;
  if (warp >= `H` * `DV`) return;
  int h = warp / `DV`, mm = warp % `DV`;
  device const float* ol = olat + (long)h * `KV`;
  device const bf16t* wm = wuv + ((long)h * `DV` + mm) * `KV`;
  float acc = 0.f;
  for (int k = lane; k < `KV`; k += 32) acc += ol[k] * bf2f(wm[k]);
  for (int oo = 16; oo > 0; oo >>= 1) acc += simd_shuffle_down(acc, oo);
  if (lane == 0) o[(long)h * `DV` + mm] = f2bf(acc);
}
"

$dsvRouteMetal = "
kernel void route(device int* sel [[buffer(0)]], device float* probs [[buffer(1)]], device const float* g [[buffer(2)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]]) {
  if (tid3.x != 0 || tgid.x != 0) return;
  float mx = g[0];
  for (int e = 1; e < `E`; e++) mx = fmax(mx, g[e]);
  float sum = 0.f;
  for (int e = 0; e < `E`; e++) sum += exp(g[e] - mx);
  int taken[`K`];
  for (int kk = 0; kk < `K`; kk++) {
    int best = -1; float bv = -3.4e38f;
    for (int e = 0; e < `E`; e++) {
      int used = 0;
      for (int j = 0; j < kk; j++) if (taken[j] == e) used = 1;
      if (!used && g[e] > bv) { bv = g[e]; best = e; }
    }
    taken[kk] = best;
    sel[kk] = best;
    probs[kk] = exp(g[best] - mx) / sum;
  }
}
"

$dsvMoeHMetal = "
kernel void moe_full_h(device float* h [[buffer(0)]], device const bf16t* x [[buffer(1)]], device const int* sel [[buffer(2)]],
    device const bf16t* wg [[buffer(3)]], device const bf16t* wu [[buffer(4)]], device const bf16t* swg [[buffer(5)]], device const bf16t* swu [[buffer(6)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int warp = (tgid.x * ntg.x + tid3.x) >> 5; int lane = tid3.x & 31;
  if (warp >= `K` * `MI` + `SI`) return;
  device const bf16t *wgi, *wui;
  if (warp < `K` * `MI`) {
    int e = warp / `MI`, i = warp % `MI`; int se = sel[e];
    wgi = wg + ((long)se * `MI` + i) * `D`; wui = wu + ((long)se * `MI` + i) * `D`;
  } else {
    int i = warp - `K` * `MI`;
    wgi = swg + (long)i * `D`; wui = swu + (long)i * `D`;
  }
  float g = 0.f, u = 0.f;
  for (int k = lane; k < `D`; k += 32) { float xk = bf2f(x[k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }
  for (int o = 16; o > 0; o >>= 1) { g += simd_shuffle_down(g, o); u += simd_shuffle_down(u, o); }
  if (lane == 0) { float gs = g / (1.f + exp(-g)); h[warp] = gs * u; }
}
"

$dsvMoeOutMetal = "
kernel void moe_full_out(device bf16t* out [[buffer(0)]], device const float* h [[buffer(1)]], device const int* sel [[buffer(2)]],
    device const float* probs [[buffer(3)]], device const bf16t* wd [[buffer(4)]], device const bf16t* swd [[buffer(5)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  int warp = (tgid.x * ntg.x + tid3.x) >> 5; int lane = tid3.x & 31;
  if (warp >= `D`) return;
  int d = warp; float acc = 0.f;
  for (int e = 0; e < `K`; e++) {
    int se = sel[e];
    device const bf16t* wde = wd + ((long)se * `D` + d) * `MI`;
    device const float* he = h + e * `MI`;
    float s = 0.f;
    for (int i = lane; i < `MI`; i += 32) s += he[i] * bf2f(wde[i]);
    for (int o = 16; o > 0; o >>= 1) s += simd_shuffle_down(s, o);
    if (lane == 0) acc += probs[e] * s;
  }
  {
    device const bf16t* wde = swd + (long)d * `SI`;
    device const float* hs = h + `K` * `MI`;
    float s = 0.f;
    for (int i = lane; i < `SI`; i += 32) s += hs[i] * bf2f(wde[i]);
    for (int o = 16; o > 0; o >>= 1) s += simd_shuffle_down(s, o);
    if (lane == 0) acc += s;
  }
  if (lane == 0) out[d] = f2bf(acc);
}
"

$dsvArgPartMetal = "
kernel void argmax_part(device float* pv [[buffer(0)]], device int* pi [[buffer(1)]], device const float* lg [[buffer(2)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]], uint3 ntg [[threads_per_threadgroup]]) {
  threadgroup float sv[256]; threadgroup int si[256];
  int b = tgid.x; int t = tid3.x;
  long lo = (long)b * `CHUNK`; long hi = lo + `CHUNK`; if (hi > `V`) hi = `V`;
  float bv = -3.4e38f; int bi = 2147483647;
  for (long i = lo + t; i < hi; i += ntg.x) {
    float v = lg[i];
    if (v > bv || (v == bv && (int)i < bi)) { bv = v; bi = (int)i; }
  }
  sv[t] = bv; si[t] = bi;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  for (int s = ntg.x / 2; s > 0; s >>= 1) {
    if (t < s && (sv[t + s] > sv[t] || (sv[t + s] == sv[t] && si[t + s] < si[t]))) {
      sv[t] = sv[t + s]; si[t] = si[t + s];
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
  }
  if (t == 0) { pv[b] = sv[0]; pi[b] = si[0]; }
}
"

$dsvArgFinMetal = "
kernel void argmax_fin(device int* tok [[buffer(0)]], device const float* pv [[buffer(1)]], device const int* pi [[buffer(2)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]]) {
  if (tid3.x != 0 || tgid.x != 0) return;
  float bv = pv[0]; int bi = pi[0];
  for (int b = 1; b < `NB`; b++)
    if (pv[b] > bv || (pv[b] == bv && pi[b] < bi)) { bv = pv[b]; bi = pi[b]; }
  tok[0] = bi;
}
"

$dsvBumpMetal = "
kernel void len_bump(device int* L [[buffer(0)]],
    uint3 tgid [[threadgroup_position_in_grid]], uint3 tid3 [[thread_position_in_threadgroup]]) {
  if (tid3.x == 0 && tgid.x == 0) L[0] = L[0] + 1;
}
"

(* Instantiate one kernel: pick the backend dialect, apply the template
   params, and wrap in the TKernel program spec. *)
dsvMakeKernel[cfg_, name_, cudaSrc_, cpuSrc_, metalSrc_, grid_, blockDims_, outs_, extra_ : <||>] := TKernel[<|
    "Program" -> StringTemplate[
        Replace[cfg["Device"], {
            "metal" :> $dsvMetalPre <> metalSrc,
            "cpu" :> $dsvCpuPre <> cpuSrc,
            _ :> $dsvCudaPre <> cudaSrc
        }]
    ][Join[dsvTemplateParams[cfg], extra]],
    "Name" -> name,
    "Grid" -> grid,
    "Block" -> blockDims,
    "Outs" -> outs
|>]

(* The full kernel set for a config.  Grid/block mirror the _fxn_* launch
   shapes in deepseek_v2.py (warp-per-output kernels use ceil(nout*32/256)
   blocks of 256; the CPU twins loop themselves and ignore the dims). *)
dsvKernels[cfg_] := dsvKernels[cfg] = Block[{d, h, dn, dr, kv, dv, e, k, mi, si, v, nb, cdiv},
    {d, h, dn, dr, kv, dv, e, k, mi, si, v, nb} = cfg /@
        {"Dim", "NHeads", "DNope", "DRope", "KvLora", "DV", "NExperts", "TopK", "MoeInter", "SharedInter", "Vocab", "ArgmaxBlocks"};
    cdiv = Ceiling[#1 / #2] &;
    <|
        "EmbedRow" -> dsvMakeKernel[cfg, "embed_row", $dsvEmbedCuda, $dsvEmbedCpu, $dsvEmbedMetal, {cdiv[d, 256]}, {256}, {1}],
        "RopeQ" -> dsvMakeKernel[cfg, "rope_dev", $dsvRopeCuda, $dsvRopeCpu, $dsvRopeMetal, {h}, {dr/2}, {1}, <|"NHS" -> ToString[h]|>],
        "RopeK" -> dsvMakeKernel[cfg, "rope_dev", $dsvRopeCuda, $dsvRopeCpu, $dsvRopeMetal, {1}, {dr/2}, {1}, <|"NHS" -> "1"|>],
        "CacheAppend" -> dsvMakeKernel[cfg, "cache_append", $dsvAppendCuda, $dsvAppendCpu, $dsvAppendMetal, {cdiv[kv + dr, 256]}, {256}, {1, 2}],
        "Qlat" -> dsvMakeKernel[cfg, "mla_qlat", $dsvQlatCuda, $dsvQlatCpu, $dsvQlatMetal, {cdiv[h * kv, 256]}, {256}, {1}],
        "Score" -> dsvMakeKernel[cfg, "mla_score", $dsvScoreCuda, $dsvScoreCpu, $dsvScoreMetal, {h}, {256}, {1}],
        "Mix" -> dsvMakeKernel[cfg, "mla_mix", $dsvMixCuda, $dsvMixCpu, $dsvMixMetal, {cdiv[h * kv, 256]}, {256}, {1}],
        "MlaO" -> dsvMakeKernel[cfg, "mla_o", $dsvOCuda, $dsvOCpu, $dsvOMetal, {cdiv[h * dv * 32, 256]}, {256}, {1}],
        "Route" -> dsvMakeKernel[cfg, "route", $dsvRouteCuda, $dsvRouteCpu, $dsvRouteMetal, {1}, {32}, {1, 2}],
        "MoeH" -> dsvMakeKernel[cfg, "moe_full_h", $dsvMoeHCuda, $dsvMoeHCpu, $dsvMoeHMetal, {cdiv[(k * mi + si) * 32, 256]}, {256}, {1}],
        "MoeOut" -> dsvMakeKernel[cfg, "moe_full_out", $dsvMoeOutCuda, $dsvMoeOutCpu, $dsvMoeOutMetal, {cdiv[d * 32, 256]}, {256}, {1}],
        "ArgmaxPart" -> dsvMakeKernel[cfg, "argmax_part", $dsvArgPartCuda, $dsvArgPartCpu, $dsvArgPartMetal, {nb}, {256}, {1, 2}],
        "ArgmaxFin" -> dsvMakeKernel[cfg, "argmax_fin", $dsvArgFinCuda, $dsvArgFinCpu, $dsvArgFinMetal, {1}, {32}, {1}],
        "LenBump" -> dsvMakeKernel[cfg, "len_bump", $dsvBumpCuda, $dsvBumpCpu, $dsvBumpMetal, {1}, {1}, {1}]
    |>
]

(* === synthetic-weight state ====================================== *)

(* Move-to-device + realize: everything (weights, tables, scratch) is a
   realized contiguous leaf so it can bind as a custom-kernel argument. *)
dsvUp[t_, dev_] := TRealize @ If[dev === "cpu", t, TToDevice[t, dev]]

dsvBf[data_, dev_] := dsvUp[TUOpCast[TTensorCreate @ NumericArray[data, "Real32"], "bf16"], dev]

dsvF32[data_, dev_] := dsvUp[TTensorCreate @ NumericArray[N @ data, "Real32"], dev]

dsvI32[data_, dev_] := dsvUp[TTensorCreate[data, "i32"], dev]

dsvRnd[cfg_, dims_] := RandomReal[{-cfg["WeightScale"], cfg["WeightScale"]}, dims]

(* One layer's synthetic weights.  Matmul weights are stored {in, out} so the
   forward is a plain x . W; the MLA absorbed factors w_uk/w_uv and the MoE
   expert stacks keep the flat layouts the kernels index
   (deepseek_v2.py:993-997 absorb, :1133-1135 expert stacks). *)
dsvLayerWeights[cfg_, l_] := Block[{d, hn, dn, dr, kv, dv, e, mi, si, dev, dense},
    {d, hn, dn, dr, kv, dv, e, mi, si, dev} = cfg /@
        {"Dim", "NHeads", "DNope", "DRope", "KvLora", "DV", "NExperts", "MoeInter", "SharedInter", "Device"};
    dense = l <= cfg["FirstDense"];
    <|
        "WIn" -> dsvF32[ConstantArray[1., d], dev],
        "Wq" -> dsvBf[dsvRnd[cfg, {d, hn * (dn + dr)}], dev],
        "Wkv" -> dsvBf[dsvRnd[cfg, {d, kv + dr}], dev],
        "WkvNorm" -> dsvF32[ConstantArray[1., kv], dev],
        "Wuk" -> dsvBf[dsvRnd[cfg, {hn * dn * kv}], dev],
        "Wuv" -> dsvBf[dsvRnd[cfg, {hn * dv * kv}], dev],
        "Wo" -> dsvBf[dsvRnd[cfg, {hn * dv, d}], dev],
        "WPost" -> dsvF32[ConstantArray[1., d], dev],
        If[ dense,
            <|
                "Wg1" -> dsvBf[dsvRnd[cfg, {d, cfg["DenseInter"]}], dev],
                "Wu1" -> dsvBf[dsvRnd[cfg, {d, cfg["DenseInter"]}], dev],
                "Wd1" -> dsvBf[dsvRnd[cfg, {cfg["DenseInter"], d}], dev]
            |>
            ,
            <|
                "WGate" -> dsvF32[dsvRnd[cfg, {d, e}], dev],
                "Wg" -> dsvBf[dsvRnd[cfg, {e * mi * d}], dev],
                "Wu" -> dsvBf[dsvRnd[cfg, {e * mi * d}], dev],
                "Wd" -> dsvBf[dsvRnd[cfg, {e * d * mi}], dev],
                "Swg" -> dsvBf[dsvRnd[cfg, {si * d}], dev],
                "Swu" -> dsvBf[dsvRnd[cfg, {si * d}], dev],
                "Swd" -> dsvBf[dsvRnd[cfg, {d * si}], dev]
            |>
        ]
    |>
]

dsvSynthInit[cfg_Association] := (
    SeedRandom[cfg["Seed"]];
    dsvState[cfg, <|
        "Embed" -> dsvBf[dsvRnd[cfg, {cfg["Vocab"], cfg["Dim"]}], cfg["Device"]],
        "Layers" -> Table[dsvLayerWeights[cfg, l], {l, cfg["NLayers"]}],
        "Norm" -> dsvF32[ConstantArray[1., cfg["Dim"]], cfg["Device"]],
        "LmHead" -> dsvBf[dsvRnd[cfg, {cfg["Dim"], cfg["Vocab"]}], cfg["Device"]]
    |>]
)

(* Caches, scratch, rope tables, and device token/length state around a weight
   association (shared by the synthetic and checkpoint inits). *)
dsvState[cfg_Association, weights_Association] := Block[{dev, d, hn, kv, dr, dv, maxSeq, nb, cos, sin},
    dev = cfg["Device"];
    {d, hn, kv, dr, dv, maxSeq, nb} = cfg /@ {"Dim", "NHeads", "KvLora", "DRope", "DV", "MaxSeq", "ArgmaxBlocks"};
    {cos, sin} = dsvRopeTables[cfg];
    <|
        "Config" -> cfg,
        "Weights" -> weights,
        "Kernels" -> dsvKernels[cfg],
        "Cos" -> dsvF32[cos, dev],
        "Sin" -> dsvF32[sin, dev],
        (* device decode state: token id 1, length 1 (= this step's cache row + 1,
           the S==1 fused-path convention Ltok = pos+1, deepseek_v2.py:1013,1322) *)
        "Tok" -> dsvI32[{1}, dev],
        "Len" -> dsvI32[{1}, dev],
        (* per-layer latent caches, zeroed (rows beyond the device length are
           never read; zeros keep readbacks finite) *)
        "CkvCache" -> Table[dsvBf[ConstantArray[0., {maxSeq, kv}], dev], {cfg["NLayers"]}],
        "KpeCache" -> Table[dsvBf[ConstantArray[0., {maxSeq, dr}], dev], {cfg["NLayers"]}],
        (* persistent custom->custom scratch, shared across layers (the step
           replays strictly in order, so layer l+1 overwrites only after layer
           l's consumers ran) *)
        "X" -> dsvBf[ConstantArray[0., d], dev],
        "QlScr" -> dsvF32[ConstantArray[0., hn * kv], dev],
        "SmScr" -> dsvF32[ConstantArray[0., hn * maxSeq], dev],
        "OlScr" -> dsvF32[ConstantArray[0., hn * kv], dev],
        "OBuf" -> dsvBf[ConstantArray[0., hn * dv], dev],
        "Sel" -> dsvI32[ConstantArray[0, cfg["TopK"]], dev],
        "Probs" -> dsvF32[ConstantArray[0., cfg["TopK"]], dev],
        "HScr" -> dsvF32[ConstantArray[0., cfg["TopK"] * cfg["MoeInter"] + cfg["SharedInter"]], dev],
        "MoeBuf" -> dsvBf[ConstantArray[0., d], dev],
        "PartV" -> dsvF32[ConstantArray[0., nb], dev],
        "PartI" -> dsvI32[ConstantArray[0, nb], dev]
    |>
]

(* === real (HF checkpoint) weights ================================ *)

(* Map a HuggingFace checkpoint association (name -> TTerm; bf16 mmap views
   from TSafeTensorLoad or in-memory tensors) into the decode weight layouts.
   Movement + exact-widening casts only, so the mapped values are bit-identical
   to the checkpoint:
     * nn.Linear weights are {out, in}; the step's matmuls consume {in, out}
       -> Transpose (wq/wkv/wo/dense MLP/lm_head; gate also widens to f32,
       tinygrad keeps it bf16 and upcasts in-op, deepseek_v2.py:1141).
     * kv_b_proj {H (dn+dv), kv} absorb-splits into flat w_uk {H dn kv} /
       w_uv {H dv kv} (deepseek_v2.py:993-997), the layouts mla_qlat / mla_o
       index directly.
     * routed experts stack flat, expert-major (deepseek_v2.py:1266-1271);
       shared-expert matrices flatten as-is (moe_full_h/out index row-major
       {out, in}).
     * norm gains widen bf16 -> f32 (exact).
   The C twin is tools/dsv2_pack_weights.py (same mapping, pre-applied into a
   flat pack for the no-Wolfram pod bench). *)
dsvHfLayer[cfg_, sd_, l_] := Block[{
    d = cfg["Dim"], hn = cfg["NHeads"], dn = cfg["DNope"], dr = cfg["DRope"],
    kv = cfg["KvLora"], dv = cfg["DV"], e = cfg["NExperts"], mi = cfg["MoeInter"],
    dev = cfg["Device"], p, w, kvb
},
    p = "model.layers." <> ToString[l - 1] <> ".";
    w = name |-> sd[p <> name <> ".weight"];
    kvb = ArrayReshape[w["self_attn.kv_b_proj"], {hn, dn + dv, kv}];
    <|
        "WIn" -> dsvUp[TUOpCast[w["input_layernorm"], "f32"], dev],
        "Wq" -> dsvUp[Transpose @ w["self_attn.q_proj"], dev],
        "Wkv" -> dsvUp[Transpose @ w["self_attn.kv_a_proj_with_mqa"], dev],
        "WkvNorm" -> dsvUp[TUOpCast[w["self_attn.kv_a_layernorm"], "f32"], dev],
        "Wuk" -> dsvUp[ArrayReshape[kvb[[All, 1 ;; dn, All]], {hn * dn * kv}], dev],
        "Wuv" -> dsvUp[ArrayReshape[kvb[[All, dn + 1 ;;, All]], {hn * dv * kv}], dev],
        "Wo" -> dsvUp[Transpose @ w["self_attn.o_proj"], dev],
        "WPost" -> dsvUp[TUOpCast[w["post_attention_layernorm"], "f32"], dev],
        If[ l <= cfg["FirstDense"],
            <|
                "Wg1" -> dsvUp[Transpose @ w["mlp.gate_proj"], dev],
                "Wu1" -> dsvUp[Transpose @ w["mlp.up_proj"], dev],
                "Wd1" -> dsvUp[Transpose @ w["mlp.down_proj"], dev]
            |>
            ,
            <|
                "WGate" -> dsvUp[Transpose @ TUOpCast[w["mlp.gate"], "f32"], dev],
                "Wg" -> dsvUp[dsvExpertStack[w, "gate_proj", e], dev],
                "Wu" -> dsvUp[dsvExpertStack[w, "up_proj", e], dev],
                "Wd" -> dsvUp[dsvExpertStack[w, "down_proj", e], dev],
                "Swg" -> dsvUp[dsvFlat @ w["mlp.shared_experts.gate_proj"], dev],
                "Swu" -> dsvUp[dsvFlat @ w["mlp.shared_experts.up_proj"], dev],
                "Swd" -> dsvUp[dsvFlat @ w["mlp.shared_experts.down_proj"], dev]
            |>
        ]
    |>
]

dsvFlat[t_] := ArrayReshape[t, {Times @@ TTensorShape[t]}]

(* Flat expert stack {e*inter*dim}, expert-major.  A checkpoint may store the
   experts pre-stacked as one {e, inter, dim} tensor (mlp.experts.<proj>) -- then
   a single reshape suffices; the per-expert form is concatenated instead.
   thvm has no native CAT (tCatAxis emulates it by pad-and-sum, O(n^2) in the
   input count), so the pre-stacked path is the one that scales to 64 experts. *)
dsvExpertStack[w_, proj_, e_] := With[{stacked = w["mlp.experts." <> proj]},
    If[ MatchQ[stacked, _TTerm],
        dsvFlat @ stacked,
        Join @@ Table[dsvFlat @ w["mlp.experts." <> ToString[i - 1] <> "." <> proj], {i, e}]
    ]
]

dsvHfWeights[cfg_Association, sd_Association] := <|
    "Embed" -> dsvUp[sd["model.embed_tokens.weight"], cfg["Device"]],
    "Layers" -> Table[dsvHfLayer[cfg, sd, l], {l, cfg["NLayers"]}],
    "Norm" -> dsvUp[TUOpCast[sd["model.norm.weight"], "f32"], cfg["Device"]],
    "LmHead" -> dsvUp[Transpose @ sd["lm_head.weight"], cfg["Device"]]
|>

dsvHfInit[cfg_Association, sd_Association] := dsvState[cfg, dsvHfWeights[cfg, sd]]

(* === the decode step ============================================= *)

(* RMS norm with a MATERIALIZE BOUNDARY on the reduce scalar.  Same math as
   TRMSNorm (NN/Norm.wl:89), but `inv` is realized before it broadcasts:
   (a) the fused EXPAND(broadcast)-over-REDUCE-over-CAST(bf16) chain is the
   open CPU-lift correctness bug staged in
   Tests/pending_cpu_bf16_reduce_broadcast.wlt, and (b) tinygrad's block
   inserts materialize boundaries at every sublayer for the same structural
   reason (deepseek_v2.py:1211-1216).  `shape` is passed explicitly (the
   static shape walk is not part of the public surface). *)
dsvRmsNorm[x_, weight_, eps_, shape_List] := Block[{
    rank = Length[shape],
    d = Last[shape],
    inv, invB, wB
},
    inv = TRealize[1 / Sqrt[TUOpReduce[x * x, rank - 1, "SUM"] / d + eps]];
    invB = TUOpExpand[TUOpReshape[inv, ReplacePart[shape, rank -> 1]], shape];
    wB = TUOpExpand[TUOpReshape[weight, Append[ConstantArray[1, rank - 1], d]], shape];
    (* realize WHILE `inv` is still WL-alive: the product graph references
       inv's buffer through heap terms only, and once the Block local drops
       the WL pin the buffer can be recycled before a later realize reads it
       (observed as a constant-factor-wrong norm; the second half of the
       pending_cpu_bf16_reduce_broadcast.wlt story) *)
    TRealize[x * invB * wB]
]

(* One transformer layer.  x is the {1, dim} bf16 residual stream (a realized
   leaf); returns the updated stream, realized.  Norms compute in f32 and cast
   back (deepseek_v2.py:976-978); projections are plain bf16 matmuls on the
   native codegen path; the attention core and MoE are the custom-kernel
   chains.  Every sublayer output is realized, mirroring tinygrad's
   .contiguous() at layer + sublayer boundaries (deepseek_v2.py:1211-1216):
   each op stays a clean matvec reading its weight once, and the residual
   chain never re-derives. *)
dsvLayer[state_, l_, x_] := Block[{
    cfg = state["Config"], lw, k, eps, d, dn, dr, hn, kv,
    xn, q, qm, qn, qp, kva, ckv, kp, attnOut, x1, xn2, g, xb, g1, u1
},
    lw = state["Weights"]["Layers"][[l]];
    k = state["Kernels"];
    eps = cfg["Eps"]; d = cfg["Dim"]; dn = cfg["DNope"]; dr = cfg["DRope"]; hn = cfg["NHeads"]; kv = cfg["KvLora"];
    xn = TRealize @ TUOpCast[dsvRmsNorm[TUOpCast[x, "f32"], lw["WIn"], eps, {1, d}], "bf16"];
    (* q/kv projections: plain bf16 matmuls on the native codegen path *)
    q = TRealize[xn . lw["Wq"]];
    qm = ArrayReshape[q, {hn, dn + dr}];
    qn = TRealize @ qm[[All, 1 ;; dn]];
    qp = TRealize @ qm[[All, dn + 1 ;;]];
    k["RopeQ"][qp, state["Cos"], state["Sin"], state["Len"]];
    kva = TRealize[xn . lw["Wkv"]];
    (* rank-2 slices throughout: the rank-1 form (kva[[1, 1;;kv]], a
       CAST(RESHAPE(SHRINK(matmul))) reduce input) hits the open CPU stale-
       kernel-adoption bug (Tests/pending_cpu_bf16_reduce_broadcast.wlt); the
       rank-2 view chain fuses into its consumers and is validated in-context.
       The custom kernels read linear buffers, so {1, kv} vs {kv} is free. *)
    ckv = TRealize @ TUOpCast[dsvRmsNorm[TUOpCast[kva[[All, 1 ;; kv]], "f32"], lw["WkvNorm"], eps, {1, kv}], "bf16"];
    kp = TRealize @ kva[[All, kv + 1 ;;]];
    k["RopeK"][kp, state["Cos"], state["Sin"], state["Len"]];
    k["CacheAppend"][state["CkvCache"][[l]], state["KpeCache"][[l]], ckv, kp, state["Len"]];
    k["Qlat"][state["QlScr"], qn, lw["Wuk"]];
    k["Score"][state["SmScr"], state["QlScr"], qp, state["CkvCache"][[l]], state["KpeCache"][[l]], ckv, kp, state["Len"]];
    k["Mix"][state["OlScr"], state["SmScr"], state["CkvCache"][[l]], ckv, state["Len"]];
    k["MlaO"][state["OBuf"], state["OlScr"], lw["Wuv"]];
    attnOut = TRealize[ArrayReshape[state["OBuf"], {1, hn * cfg["DV"]}] . lw["Wo"]];
    x1 = TRealize[x + attnOut];
    xn2 = TRealize @ dsvRmsNorm[TUOpCast[x1, "f32"], lw["WPost"], eps, {1, d}];
    If[ l <= cfg["FirstDense"]
        ,
        (* dense MLP (layer 0 in tinygrad indexing; deepseek_v2.py:1123-1127) *)
        xb = TRealize @ TUOpCast[xn2, "bf16"];
        g1 = TRealize[xb . lw["Wg1"]];
        u1 = TRealize[xb . lw["Wu1"]];
        TRealize[x1 + TRealize[(TSiLU[g1] * u1) . lw["Wd1"]]]
        ,
        g = TRealize[xn2 . lw["WGate"]];
        k["Route"][state["Sel"], state["Probs"], g];
        k["MoeH"][state["HScr"], TRealize @ TUOpCast[xn2, "bf16"],
            state["Sel"], lw["Wg"], lw["Wu"], lw["Swg"], lw["Swu"]];
        k["MoeOut"][state["MoeBuf"], state["HScr"], state["Sel"], state["Probs"], lw["Wd"], lw["Swd"]];
        TRealize[x1 + ArrayReshape[state["MoeBuf"], {1, d}]]
    ]
]

dsvDecodeStep[state_Association] := Block[{cfg = state["Config"], w = state["Weights"], k = state["Kernels"], x, xnF, logits},
    k["EmbedRow"][state["X"], w["Embed"], state["Tok"]];
    x = ArrayReshape[state["X"], {1, cfg["Dim"]}];
    Do[x = dsvLayer[state, l, x], {l, cfg["NLayers"]}];
    xnF = TRealize @ dsvRmsNorm[TUOpCast[x, "f32"], w["Norm"], cfg["Eps"], {1, cfg["Dim"]}];
    logits = TRealize[xnF . w["LmHead"]];
    k["ArgmaxPart"][state["PartV"], state["PartI"], logits];
    k["ArgmaxFin"][state["Tok"], state["PartV"], state["PartI"]];
    k["LenBump"][state["Len"]];
    {state["Tok"], logits}
]

(* Zero-argument JIT closure: all state lives in device buffers, so a replay
   is one full decode step with no host values (the tinygrad zero-sync loop,
   deepseek_v2.py:1315-1335). *)
dsvDecodeJit[state_Association] := TJit[dsvDecodeStep[state] &]

dsvToken[state_Association] := First @ Normal @ TTensorData @ state["Tok"]

dsvPosition[state_Association] := First @ Normal @ TTensorData @ state["Len"]

(* Rewind to a fresh conversation: length back to 1, token to the seed.  The
   caches keep their bytes (rows >= the length are never read), so nothing is
   reallocated and the captured step reuses the same device buffers. *)
dsvReset[state_Association] := (
    TSetData[state["Len"], {1}];
    TSetData[state["Tok"], {1}];
    state
)

Options[dsvGenerate] = {"MaxTokens" -> 256, "EosId" -> 100001}

(* Prefill + greedy decode over the captured step.  Each prompt token is set
   into the device token buffer before a replay (TSetData, in place), so the
   argmax the step writes back is overridden until the last prompt token; from
   there the model's own argmax feeds the next step -- the standard S=1 decode
   loop.  Returns the generated ids only (prompt excluded). *)
dsvGenerate[state_Association, promptIds_List, opts : OptionsPattern[]] := Block[{
    jit = dsvDecodeJit[state], maxNew = OptionValue["MaxTokens"], eos = OptionValue["EosId"],
    maxSeq = state["Config"]["MaxSeq"], pids, gen, t
},
    dsvReset[state];
    (* the caches hold MaxSeq rows; a step writes cache row Len-1 and reads
       rows 0..Len-1, so Len must stay <= MaxSeq -- clamp the prompt and stop
       decoding at the horizon rather than overrun the buffers (SIGABRT). *)
    pids = Take[promptIds, UpTo[maxSeq]];
    Do[TSetData[state["Tok"], {pids[[i]]}]; jit[], {i, Length[pids]}];
    Flatten @ Reap[
        Do[
            If[dsvPosition[state] > maxSeq, Break[]];
            t = dsvToken[state];
            If[t === eos, Break[]];
            Sow[t];
            jit[],
            {maxNew}
        ]
    ][[2]]
]

(* DeepSeek-V2-Chat prompt template (tokenizer_config chat_template): bos, then
   per-turn "User:"/"Assistant:"/system lines, ending with the generation cue.
   The bos/eos markers are literal special-token strings the tokenizer maps to
   ids 100000/100001. *)
$dsvBos = "<\:ff5cbegin\:2581of\:2581sentence\:ff5c>"
$dsvEos = "<\:ff5cend\:2581of\:2581sentence\:ff5c>"

dsvChatPrompt[text_String] := dsvChatPrompt[{<|"Role" -> "user", "Content" -> text|>}]

dsvChatPrompt[messages_List] := $dsvBos <> StringJoin[
    (Switch[ToLowerCase @ Lookup[#, "Role", "user"],
        "user", "User: " <> #Content <> "\n\n",
        "assistant", "Assistant: " <> #Content <> $dsvEos,
        "system", #Content <> "\n\n",
        _, ""
    ] &) /@ messages
] <> "Assistant:"

(* === host reference (validation) ================================= *)

(* An INDEPENDENT machine-precision implementation of the same math, computed
   host-side from Normal readbacks of the same bf16 weights (so it shares no
   thvm codegen with the pipeline under test; the bf16 quantization of the
   weights is common to both, which is what the 2e-2 tolerance is about). *)

dsvRefRms[v_List, gain_List, eps_] := v / Sqrt[Mean[v^2] + eps] * gain

dsvRefRope[v_List, cs_List, sn_List] := Block[{e = v[[1 ;; ;; 2]], o = v[[2 ;; ;; 2]]},
    Join[e * cs - o * sn, o * cs + e * sn]
]

dsvRefLayerWeights[lw_Association] := Association @ KeyValueMap[#1 -> Normal[#2] &, lw]

dsvRefInit[state_Association] := Block[{cfg = state["Config"], w = state["Weights"]},
    <|
        "Config" -> cfg,
        "Embed" -> Normal @ w["Embed"],
        "Layers" -> (dsvRefLayerWeights /@ w["Layers"]),
        "Norm" -> Normal @ w["Norm"],
        "LmHead" -> Normal @ w["LmHead"],
        "Cos" -> Normal @ state["Cos"],
        "Sin" -> Normal @ state["Sin"],
        "Ckv" -> Table[{}, {cfg["NLayers"]}],
        "Kpe" -> Table[{}, {cfg["NLayers"]}],
        "Pos" -> 0
    |>
]

(* One reference layer over host vectors.  Same layouts as the kernels: wuk
   flat [H*DN*KV], wuv flat [H*DV*KV], expert stacks flat (see
   dsvLayerWeights). *)
dsvRefLayer[ref_, cfg_, l_, lw_, x_, cs_, sn_] := Block[{
    eps = cfg["Eps"], hn = cfg["NHeads"], dn = cfg["DNope"], dr = cfg["DRope"],
    kv = cfg["KvLora"], dv = cfg["DV"], kTop = cfg["TopK"], mi = cfg["MoeInter"], si = cfg["SharedInter"],
    xn, q, kva, ckv, kp, ckvRows, kpeRows, heads, attnOut, x1, xn2, out
},
    xn = dsvRefRms[x, lw["WIn"], eps];
    q = xn . lw["Wq"];
    kva = xn . lw["Wkv"];
    ckv = dsvRefRms[kva[[1 ;; kv]], lw["WkvNorm"], eps];
    kp = dsvRefRope[kva[[kv + 1 ;;]], cs, sn];
    ckvRows = Append[ref["Ckv"][[l]], ckv];
    kpeRows = Append[ref["Kpe"][[l]], kp];
    heads = Table[
        Block[{qh, qnh, qph, wukh, wuvh, qlat, scores, sm, olat},
            qh = q[[(h - 1) (dn + dr) + 1 ;; h (dn + dr)]];
            qnh = qh[[1 ;; dn]];
            qph = dsvRefRope[qh[[dn + 1 ;;]], cs, sn];
            wukh = ArrayReshape[lw["Wuk"][[(h - 1) dn kv + 1 ;; h dn kv]], {dn, kv}];
            wuvh = ArrayReshape[lw["Wuv"][[(h - 1) dv kv + 1 ;; h dv kv]], {dv, kv}];
            qlat = qnh . wukh;
            scores = cfg["Scale"] * (ckvRows . qlat + kpeRows . qph);
            sm = Exp[scores - Max[scores]];
            sm = sm / Total[sm];
            olat = sm . ckvRows;
            wuvh . olat
        ],
        {h, hn}
    ];
    attnOut = Flatten[heads] . lw["Wo"];
    x1 = x + attnOut;
    xn2 = dsvRefRms[x1, lw["WPost"], eps];
    out = If[ l <= cfg["FirstDense"]
        ,
        Block[{silu = # / (1 + Exp[-#]) &, g1, u1},
            g1 = xn2 . lw["Wg1"];
            u1 = xn2 . lw["Wu1"];
            (silu[g1] * u1) . lw["Wd1"]
        ]
        ,
        Block[{gl, probsAll, sel, silu = # / (1 + Exp[-#]) &, routed, wg3, wu3, wd3, swg2, swu2, swd2, hShared},
            gl = xn2 . lw["WGate"];
            probsAll = Exp[gl - Max[gl]];
            probsAll = probsAll / Total[probsAll];
            sel = Reverse @ Ordering[gl, -kTop];   (* descending, ties -> lowest id *)
            wg3 = ArrayReshape[lw["Wg"], {cfg["NExperts"], mi, cfg["Dim"]}];
            wu3 = ArrayReshape[lw["Wu"], {cfg["NExperts"], mi, cfg["Dim"]}];
            wd3 = ArrayReshape[lw["Wd"], {cfg["NExperts"], cfg["Dim"], mi}];
            routed = Total @ Table[
                Block[{e = sel[[kk]], he},
                    he = silu[wg3[[e]] . xn2] * (wu3[[e]] . xn2);
                    probsAll[[e]] * (wd3[[e]] . he)
                ],
                {kk, kTop}
            ];
            swg2 = ArrayReshape[lw["Swg"], {si, cfg["Dim"]}];
            swu2 = ArrayReshape[lw["Swu"], {si, cfg["Dim"]}];
            swd2 = ArrayReshape[lw["Swd"], {cfg["Dim"], si}];
            hShared = silu[swg2 . xn2] * (swu2 . xn2);
            routed + swd2 . hShared
        ]
    ];
    {x1 + out, ckvRows, kpeRows}
]

dsvRefStep[ref_Association, tokenId_Integer] := Block[{
    cfg = ref["Config"], pos = ref["Pos"], cs, sn, x, ckvNew = ref["Ckv"], kpeNew = ref["Kpe"], logits, r
},
    cs = ref["Cos"][[pos + 1, 1 ;; cfg["DRope"]/2]];
    sn = ref["Sin"][[pos + 1, 1 ;; cfg["DRope"]/2]];
    x = ref["Embed"][[tokenId + 1]];
    Do[
        r = dsvRefLayer[<|"Ckv" -> ckvNew, "Kpe" -> kpeNew|>, cfg, l, ref["Layers"][[l]], x, cs, sn];
        x = r[[1]];
        ckvNew[[l]] = r[[2]];
        kpeNew[[l]] = r[[3]],
        {l, cfg["NLayers"]}
    ];
    logits = dsvRefRms[x, ref["Norm"], cfg["Eps"]] . ref["LmHead"];
    <|
        "State" -> <|ref, "Ckv" -> ckvNew, "Kpe" -> kpeNew, "Pos" -> pos + 1|>,
        "Logits" -> logits,
        "Token" -> First[Ordering[logits, -1]] - 1
    |>
]

End[];

EndPackage[];
