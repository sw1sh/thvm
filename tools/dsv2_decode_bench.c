// tools/dsv2_decode_bench.c - DeepSeek-V2-Lite BS=1 decode-step microbenchmark
// at the C level (no Wolfram on the pod).  Hardcodes the same step the WL
// example builds (wl/THVMLink/Kernel/NN/Examples/DeepSeekV2/Decode.wl), port of
// tinygrad examples/deepseek_v2.py --synth:
//
//   embed_row -> N x [ rms-norm -> q/kv projections (native uop matmuls)
//                      -> rope_dev(q_pe) -> rope_dev(k_pe) -> cache_append
//                      -> mla_qlat/mla_score/mla_mix/mla_o -> o_proj matmul
//                      -> rms-norm -> gate matmul -> route -> moe_full_h/out ]
//   -> rms-norm -> lm_head matmul -> argmax_part/argmax_fin -> len_bump
//
// Projections and norms are NATIVE uop graphs (reshape+expand+mul+reduce, the
// exact TMatMul lowering of wl/THVMLink/Kernel/NN/Linear.wl:49-58) so the
// codegen path matches the WL example; the attention core / MoE / decode
// state are injected custom kernels (kernel_emit_custom_program) with
// per-backend sources: CUDA-C near-verbatim from deepseek_v2.py, plain-C
// functional twins for the CPU custom ABI.
//
// The whole step is built + fired ONCE under jit_capture_begin/end (capture
// pins every recorded buffer), then WARM + STEPS replays run it; the token
// argmax feeds back into the device token buffer and the position lives in a
// device int32 buffer, so a replay is one full decode step with zero host
// values.
//
//   build:  make bin/dsv2_decode_bench            (Linux+CUDA adds the CUDA backend)
//   run:    DEV=cuda ./bin/dsv2_decode_bench      (full model shape, ~31 GB device)
//           DEV=cpu DSV2_DIM=128 DSV2_LAYERS=4 DSV2_HEADS=8 DSV2_KVLORA=64 \
//             DSV2_DNOPE=32 DSV2_DROPE=16 DSV2_DV=32 DSV2_EXPERTS=8 DSV2_TOPK=3 \
//             DSV2_MOEINTER=64 DSV2_DENSEINTER=256 DSV2_VOCAB=1024 DSV2_MAXSEQ=128 \
//             ./bin/dsv2_decode_bench             (local CPU smoke, no GPU)
//   knobs:  WARM (20) STEPS (40) + every DSV2_* shape knob.
#include "../src/thvm.c"
#include <time.h>

// === config (deepseek-ai/DeepSeek-V2-Lite, deepseek_v2.py:26-29) ============
typedef struct {
  u32 dim, n_layers, n_heads, kv_lora, d_nope, d_rope, d_v;
  u32 n_exp, n_shared, top_k, moe_inter, dense_inter, first_dense;
  u32 vocab, max_seq, nb, chunk;
  float eps, scale;
} Cfg;
static Cfg C;
static Backend *BK;
static int IS_CUDA = 0;

static u32 envu(char const *k, u32 d) {
  char const *e = getenv(k);
  return (e != NULL && e[0] != '\0') ? (u32)strtoul(e, NULL, 10) : d;
}

static void cfg_init(void) {
  C.dim         = envu("DSV2_DIM", 2048);
  C.n_layers    = envu("DSV2_LAYERS", 27);
  C.n_heads     = envu("DSV2_HEADS", 16);
  C.kv_lora     = envu("DSV2_KVLORA", 512);
  C.d_nope      = envu("DSV2_DNOPE", 128);
  C.d_rope      = envu("DSV2_DROPE", 64);
  C.d_v         = envu("DSV2_DV", 128);
  C.n_exp       = envu("DSV2_EXPERTS", 64);
  C.n_shared    = envu("DSV2_NSHARED", 2);
  C.top_k       = envu("DSV2_TOPK", 6);
  C.moe_inter   = envu("DSV2_MOEINTER", 1408);
  C.dense_inter = envu("DSV2_DENSEINTER", 10944);
  C.first_dense = envu("DSV2_FIRSTDENSE", 1);
  C.vocab       = envu("DSV2_VOCAB", 102400);
  C.max_seq     = envu("DSV2_MAXSEQ", 1024);
  C.nb          = C.vocab < 256u * 256u ? (C.vocab + 255u) / 256u : 256u;
  C.chunk       = (C.vocab + C.nb - 1u) / C.nb;
  C.eps         = 1e-6f;
  // yarn-corrected attention scale (deepseek_v2.py:549-550,953):
  // mscale = 0.1 * 0.707 * ln(40) + 1; scale = (dn+dr)^-0.5 * mscale^2
  {
    double mscale = 0.1 * 0.707 * log(40.0) + 1.0;
    C.scale = (float)(pow((double)(C.d_nope + C.d_rope), -0.5) * mscale * mscale);
  }
}

// === helpers ================================================================
static u32 fbits(float f) { u32 u; memcpy(&u, &f, 4); return u; }

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static Term tt(u32 tid) { return term_new(0, TAG_TEN, TENS[tid].dtype, tid); }

// Full realize entry (scope + bufferize/fusion classify + materialize), the
// same path the WL layer's TRealize takes (thvm_wl_realize -> thvm_realize).
// A raw wnf(thvm_materialize(t)) skips the fusion classify and explodes the
// step into ~5x the kernels.
static u32 rz(Term t) { return (u32)term_val(thvm_realize(t)); }

// One staging buffer, grown on demand (largest full-size tensor: a stacked
// MoE expert matrix, n_exp*moe_inter*dim bf16 words).
static void *STAGE = NULL;
static u64   STAGE_CAP = 0;
static void *stage(u64 bytes) {
  if (bytes > STAGE_CAP) {
    free(STAGE);
    STAGE = malloc(bytes);
    if (STAGE == NULL) { fprintf(stderr, "FATAL: staging malloc %llu\n", (unsigned long long)bytes); exit(1); }
    STAGE_CAP = bytes;
  }
  return STAGE;
}

// Cheap synthetic bf16 words: random sign/mantissa with the exponent pinned
// to 2^-6..2^-5 so magnitudes sit around the 0.05 scale the WL example uses
// (bounded activations through 27 layers; values are irrelevant to the
// bandwidth-bound speed being measured).
static u32 RNG = 0xC0FFEE01u;
static inline u32 xr(void) { RNG ^= RNG << 13; RNG ^= RNG >> 17; RNG ^= RNG << 5; return RNG; }
static inline u16 rbf16(void) {
  u32 r = xr();
  return (u16)((r & 0x8000u) | ((121u + (r & 1u)) << 7) | ((r >> 3) & 0x7Fu));
}

static u32 alloc2(u32 a, u32 b, u32 dtype) {
  Shape s = {0};
  if (b == 0) { s.ndim = 1; s.dims[0] = a; }
  else        { s.ndim = 2; s.dims[0] = a; s.dims[1] = b; }
  u32 tid = tensor_alloc(BK, s, dtype);
  if (tid == 0) { fprintf(stderr, "FATAL: tensor_alloc %ux%u\n", a, b); exit(1); }
  return tid;
}

static u32 mk_bf16_rand(u32 a, u32 b) {
  u32 tid = alloc2(a, b, DT_BF16);
  u64 n = (u64)a * (b ? b : 1);
  u16 *p = (u16 *)stage(n * 2);
  for (u64 i = 0; i < n; i++) p[i] = rbf16();
  BK->buf_write(TENS[tid].buf_id, p, n * 2);
  return tid;
}

static u32 mk_bf16_zero(u32 a, u32 b) {
  u32 tid = alloc2(a, b, DT_BF16);
  u64 n = (u64)a * (b ? b : 1);
  memset(stage(n * 2), 0, n * 2);
  BK->buf_write(TENS[tid].buf_id, STAGE, n * 2);
  return tid;
}

static u32 mk_f32_fill(u32 a, u32 b, float v) {
  u32 tid = alloc2(a, b, DT_FP32);
  u64 n = (u64)a * (b ? b : 1);
  float *p = (float *)stage(n * 4);
  for (u64 i = 0; i < n; i++) p[i] = v;
  BK->buf_write(TENS[tid].buf_id, p, n * 4);
  return tid;
}

static u32 mk_f32_rand(u32 a, u32 b, float sc) {
  u32 tid = alloc2(a, b, DT_FP32);
  u64 n = (u64)a * (b ? b : 1);
  float *p = (float *)stage(n * 4);
  for (u64 i = 0; i < n; i++) p[i] = ((float)(xr() >> 8) * (1.0f / 16777216.0f) * 2.0f - 1.0f) * sc;
  BK->buf_write(TENS[tid].buf_id, p, n * 4);
  return tid;
}

static u32 mk_i32(u32 n, int v0) {
  u32 tid = alloc2(n, 0, DT_INT32);
  int *p = (int *)stage((u64)n * 4);
  for (u32 i = 0; i < n; i++) p[i] = v0;
  BK->buf_write(TENS[tid].buf_id, p, (u64)n * 4);
  return tid;
}

// YaRN rope tables {max_seq, d_rope} f32, halves duplicated
// (deepseek_v2.py:955-967).
static void rope_tables(u32 *cos_tid, u32 *sin_tid) {
  u32 dr = C.d_rope, half = dr / 2;
  double theta = 10000.0, factor = 40.0, orig = 4096.0, bfast = 32.0, bslow = 1.0;
  double *freqs = (double *)malloc(half * sizeof(double));
  double lo = floor((dr * log(orig / (bfast * 2.0 * M_PI))) / (2.0 * log(theta)));
  double hi = ceil((dr * log(orig / (bslow * 2.0 * M_PI))) / (2.0 * log(theta)));
  if (lo < 0) lo = 0;
  if (hi > dr - 1) hi = dr - 1;
  for (u32 j = 0; j < half; j++) {
    double pf = pow(theta, (double)(2 * j) / (double)dr);
    double ramp = ((double)j - lo) / ((hi - lo) > 1e-3 ? (hi - lo) : 1e-3);
    if (ramp < 0) ramp = 0;
    if (ramp > 1) ramp = 1;
    freqs[j] = (1.0 / (factor * pf)) * ramp + (1.0 / pf) * (1.0 - ramp);
  }
  u64 n = (u64)C.max_seq * dr;
  float *cs = (float *)malloc(n * 4), *sn = (float *)malloc(n * 4);
  for (u32 p = 0; p < C.max_seq; p++)
    for (u32 j = 0; j < half; j++) {
      double a = (double)p * freqs[j];
      cs[(u64)p * dr + j] = cs[(u64)p * dr + half + j] = (float)cos(a);
      sn[(u64)p * dr + j] = sn[(u64)p * dr + half + j] = (float)sin(a);
    }
  *cos_tid = alloc2(C.max_seq, dr, DT_FP32);
  *sin_tid = alloc2(C.max_seq, dr, DT_FP32);
  BK->buf_write(TENS[*cos_tid].buf_id, cs, n * 4);
  BK->buf_write(TENS[*sin_tid].buf_id, sn, n * 4);
  free(cs); free(sn); free(freqs);
}

// === custom-kernel sources ==================================================
// Both dialects carry bf16<->f32 converters bit-identical to
// src/dtype/fp_convert.c:113-136 (no cuda_bf16.h dependency).
static char const *PRE_CU =
  "typedef unsigned short bf16t;\n"
  "static __device__ __forceinline__ float bf2f(bf16t h) { return __uint_as_float(((unsigned int)h) << 16); }\n"
  "static __device__ __forceinline__ bf16t f2bfd(float f) {\n"
  "  unsigned int b = __float_as_uint(f);\n"
  "  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u)\n"
  "    return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);\n"
  "  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16);\n"
  "}\n";

static char const *PRE_C =
  "typedef unsigned short bf16t;\n"
  "static inline float bf2f(bf16t h) { union { unsigned int u; float f; } v; v.u = ((unsigned int)h) << 16; return v.f; }\n"
  "static inline bf16t f2bfd(float f) {\n"
  "  union { unsigned int u; float f; } v; v.f = f;\n"
  "  unsigned int b = v.u;\n"
  "  if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0u)\n"
  "    return (bf16t)(((b & 0x80000000u) | 0x7f800000u | 0x400000u) >> 16);\n"
  "  return (bf16t)((b + 0x7fffu + ((b >> 16) & 1u)) >> 16);\n"
  "}\n";

static char *mks(char const *fmt, ...) {
  char *buf = (char *)malloc(32768);
  size_t off = (size_t)snprintf(buf, 32768, "%s", IS_CUDA ? PRE_CU : PRE_C);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf + off, 32768 - off, fmt, ap);
  va_end(ap);
  return buf;
}

// embed_row: x[i] = emb[tok[0]*D + i] (replaces nn.Embedding, deepseek_v2.py:1226)
static char *src_embed(void) {
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void embed_row(bf16t* x, const bf16t* emb, const int* tok) {\n"
    "  int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (i < %u) x[i] = emb[(long)tok[0] * %u + i];\n"
    "}\n", C.dim, C.dim);
  return mks(
    "void embed_row(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *x = (bf16t *)bufs[0]; const bf16t *emb = (const bf16t *)bufs[1]; const int *tok = (const int *)bufs[2];\n"
    "  for (int i = 0; i < %u; i++) x[i] = emb[(long)tok[0] * %u + i];\n"
    "}\n", C.dim, C.dim);
}

// rope_dev: io [NH, DR] roped in place at position L[0]-1 (S=1, OFFM1=1;
// port of deepseek_v2.py:180-189)
static char *src_rope(u32 nh) {
  u32 dr = C.d_rope;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void rope_dev(bf16t* io, const float* cs, const float* sn, const int* L) {\n"
    "  int row = blockIdx.x; int j = threadIdx.x;\n"
    "  if (row >= %u || j >= %u) return;\n"
    "  long base = (long)row * %u; int p = L[0] - 1;\n"
    "  float xe = bf2f(io[base + 2 * j]), xo = bf2f(io[base + 2 * j + 1]);\n"
    "  float c = cs[(long)p * %u + j], sv = sn[(long)p * %u + j];\n"
    "  __syncthreads();\n"
    "  io[base + j]      = f2bfd(xe * c - xo * sv);\n"
    "  io[base + %u + j] = f2bfd(xo * c + xe * sv);\n"
    "}\n", nh, dr / 2, dr, dr, dr, dr / 2);
  return mks(
    "void rope_dev(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *io = (bf16t *)bufs[0]; const float *cs = (const float *)bufs[1];\n"
    "  const float *sn = (const float *)bufs[2]; const int *L = (const int *)bufs[3];\n"
    "  int p = L[0] - 1;\n"
    "  for (int row = 0; row < %u; row++) {\n"
    "    long base = (long)row * %u;\n"
    "    float tmp[%u];\n"
    "    for (int j = 0; j < %u; j++) {\n"
    "      float xe = bf2f(io[base + 2 * j]), xo = bf2f(io[base + 2 * j + 1]);\n"
    "      float c = cs[(long)p * %u + j], sv = sn[(long)p * %u + j];\n"
    "      tmp[j] = xe * c - xo * sv; tmp[%u + j] = xo * c + xe * sv;\n"
    "    }\n"
    "    for (int j = 0; j < %u; j++) io[base + j] = f2bfd(tmp[j]);\n"
    "  }\n"
    "}\n", nh, dr, dr, dr / 2, dr, dr, dr / 2, dr);
}

// cache_append at offset L[0]-1 (port of deepseek_v2.py:192-197)
static char *src_append(void) {
  u32 kv = C.kv_lora, dr = C.d_rope;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void cache_append(bf16t* ckv, bf16t* kpe, const bf16t* ckv_new, const bf16t* kpe_new, const int* L) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x; int off = L[0] - 1;\n"
    "  if (idx < %u) ckv[(long)off * %u + idx] = ckv_new[idx];\n"
    "  else if (idx < %u) kpe[(long)off * %u + (idx - %u)] = kpe_new[idx - %u];\n"
    "}\n", kv, kv, kv + dr, dr, kv, kv);
  return mks(
    "void cache_append(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *ckv = (bf16t *)bufs[0]; bf16t *kpe = (bf16t *)bufs[1];\n"
    "  const bf16t *ckv_new = (const bf16t *)bufs[2]; const bf16t *kpe_new = (const bf16t *)bufs[3];\n"
    "  const int *L = (const int *)bufs[4];\n"
    "  int off = L[0] - 1;\n"
    "  for (int i = 0; i < %u; i++) ckv[(long)off * %u + i] = ckv_new[i];\n"
    "  for (int i = 0; i < %u; i++) kpe[(long)off * %u + i] = kpe_new[i];\n"
    "}\n", kv, kv, dr, dr);
}

// MLA3 chain (port of deepseek_v2.py:651-717; qc split into qn/qp, cur split
// into ckvc/kpec, matching the WL example)
static char *src_qlat(void) {
  u32 h = C.n_heads, kv = C.kv_lora, dn = C.d_nope;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_qlat(float* qlat, const bf16t* qn, const bf16t* wuk) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (idx >= %u) return;\n"
    "  int h = idx / %u, k = idx %% %u;\n"
    "  const bf16t* q = qn + (long)h * %u;\n"
    "  const bf16t* w = wuk + (long)h * %u;\n"
    "  float acc = 0.f;\n"
    "  for (int d = 0; d < %u; d++) acc += bf2f(q[d]) * bf2f(w[(long)d * %u + k]);\n"
    "  qlat[(long)h * %u + k] = acc;\n"
    "}\n", h * kv, kv, kv, dn, dn * kv, dn, kv, kv);
  return mks(
    "void mla_qlat(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *qlat = (float *)bufs[0]; const bf16t *qn = (const bf16t *)bufs[1]; const bf16t *wuk = (const bf16t *)bufs[2];\n"
    "  for (int h = 0; h < %u; h++)\n"
    "    for (int k = 0; k < %u; k++) {\n"
    "      float acc = 0.f;\n"
    "      for (int d = 0; d < %u; d++) acc += bf2f(qn[(long)h * %u + d]) * bf2f(wuk[((long)h * %u + d) * %u + k]);\n"
    "      qlat[(long)h * %u + k] = acc;\n"
    "    }\n"
    "}\n", h, kv, dn, dn, dn, kv, kv);
}

static char *src_score(void) {
  u32 kv = C.kv_lora, dr = C.d_rope, ml = C.max_seq;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_score(float* sm, const float* qlat, const bf16t* qp,\n"
    "    const bf16t* Cn, const bf16t* Pn, const bf16t* ckvc, const bf16t* kpec, const int* Lp) {\n"
    "  int h = blockIdx.x; int t = threadIdx.x; int nt = blockDim.x; int L = Lp[0]; int Lh = L - 1;\n"
    "  const bf16t* q_pe = qp + (long)h * %u;\n"
    "  const float* ql = qlat + (long)h * %u;\n"
    "  __shared__ float sc[%u]; __shared__ float red[32];\n"
    "  int lane = t & 31, w = t >> 5, nw = nt >> 5;\n"
    "  for (int j = w; j < L; j += nw) {\n"
    "    const bf16t* cj = (j < Lh) ? (Cn + (long)j * %u) : ckvc;\n"
    "    const bf16t* pj = (j < Lh) ? (Pn + (long)j * %u) : kpec;\n"
    "    float s = 0.f;\n"
    "    for (int k = lane; k < %u; k += 32) s += ql[k] * bf2f(cj[k]);\n"
    "    for (int k = lane; k < %u; k += 32) s += bf2f(q_pe[k]) * bf2f(pj[k]);\n"
    "    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);\n"
    "    if (lane == 0) sc[j] = s * %.10ff;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float m = -1e30f;\n"
    "  for (int j = t; j < L; j += nt) m = fmaxf(m, sc[j]);\n"
    "  for (int o = 16; o > 0; o >>= 1) m = fmaxf(m, __shfl_down_sync(0xffffffffu, m, o));\n"
    "  if (lane == 0) red[w] = m;\n"
    "  __syncthreads();\n"
    "  if (t == 0) { float mm = -1e30f; for (int i = 0; i < (nt + 31) / 32; i++) mm = fmaxf(mm, red[i]); red[0] = mm; }\n"
    "  __syncthreads(); float gmax = red[0]; __syncthreads();\n"
    "  float sum = 0.f;\n"
    "  for (int j = t; j < L; j += nt) { float e = __expf(sc[j] - gmax); sc[j] = e; sum += e; }\n"
    "  for (int o = 16; o > 0; o >>= 1) sum += __shfl_down_sync(0xffffffffu, sum, o);\n"
    "  if (lane == 0) red[w] = sum;\n"
    "  __syncthreads();\n"
    "  if (t == 0) { float ss = 0.f; for (int i = 0; i < (nt + 31) / 32; i++) ss += red[i]; red[0] = ss; }\n"
    "  __syncthreads(); float gsum = red[0]; __syncthreads();\n"
    "  for (int j = t; j < L; j += nt) sm[(long)h * %u + j] = sc[j] / gsum;\n"
    "}\n", dr, kv, ml, kv, dr, kv, dr, C.scale, ml);
  return mks(
    "#include <math.h>\n"
    "void mla_score(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *sm = (float *)bufs[0]; const float *qlat = (const float *)bufs[1]; const bf16t *qp = (const bf16t *)bufs[2];\n"
    "  const bf16t *Cn = (const bf16t *)bufs[3]; const bf16t *Pn = (const bf16t *)bufs[4];\n"
    "  const bf16t *ckvc = (const bf16t *)bufs[5]; const bf16t *kpec = (const bf16t *)bufs[6];\n"
    "  const int *Lp = (const int *)bufs[7];\n"
    "  int L = Lp[0], Lh = L - 1;\n"
    "  for (int h = 0; h < %u; h++) {\n"
    "    float sc[%u]; float gmax = -1e30f;\n"
    "    for (int j = 0; j < L; j++) {\n"
    "      const bf16t *cj = (j < Lh) ? (Cn + (long)j * %u) : ckvc;\n"
    "      const bf16t *pj = (j < Lh) ? (Pn + (long)j * %u) : kpec;\n"
    "      float s = 0.f;\n"
    "      for (int k = 0; k < %u; k++) s += qlat[(long)h * %u + k] * bf2f(cj[k]);\n"
    "      for (int k = 0; k < %u; k++) s += bf2f(qp[(long)h * %u + k]) * bf2f(pj[k]);\n"
    "      sc[j] = s * %.10ff;\n"
    "      if (sc[j] > gmax) gmax = sc[j];\n"
    "    }\n"
    "    float gsum = 0.f;\n"
    "    for (int j = 0; j < L; j++) { sc[j] = expf(sc[j] - gmax); gsum += sc[j]; }\n"
    "    for (int j = 0; j < L; j++) sm[(long)h * %u + j] = sc[j] / gsum;\n"
    "  }\n"
    "}\n", C.n_heads, ml, kv, dr, kv, kv, dr, dr, C.scale, ml);
}

static char *src_mix(void) {
  u32 h = C.n_heads, kv = C.kv_lora, ml = C.max_seq;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_mix(float* olat, const float* sm, const bf16t* Cn, const bf16t* ckvc, const int* Lp) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (idx >= %u) return;\n"
    "  int h = idx / %u, k = idx %% %u; int L = Lp[0], Lh = L - 1;\n"
    "  const float* smh = sm + (long)h * %u;\n"
    "  float a = 0.f;\n"
    "  for (int j = 0; j < Lh; j++) a += smh[j] * bf2f(Cn[(long)j * %u + k]);\n"
    "  a += smh[Lh] * bf2f(ckvc[k]);\n"
    "  olat[(long)h * %u + k] = a;\n"
    "}\n", h * kv, kv, kv, ml, kv, kv);
  return mks(
    "void mla_mix(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *olat = (float *)bufs[0]; const float *sm = (const float *)bufs[1];\n"
    "  const bf16t *Cn = (const bf16t *)bufs[2]; const bf16t *ckvc = (const bf16t *)bufs[3];\n"
    "  const int *Lp = (const int *)bufs[4];\n"
    "  int L = Lp[0], Lh = L - 1;\n"
    "  for (int h = 0; h < %u; h++)\n"
    "    for (int k = 0; k < %u; k++) {\n"
    "      float a = 0.f;\n"
    "      for (int j = 0; j < Lh; j++) a += sm[(long)h * %u + j] * bf2f(Cn[(long)j * %u + k]);\n"
    "      a += sm[(long)h * %u + Lh] * bf2f(ckvc[k]);\n"
    "      olat[(long)h * %u + k] = a;\n"
    "    }\n"
    "}\n", h, kv, ml, kv, ml, kv);
}

static char *src_mla_o(void) {
  u32 h = C.n_heads, kv = C.kv_lora, dv = C.d_v;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_o(bf16t* o, const float* olat, const bf16t* wuv) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= %u) return;\n"
    "  int h = warp / %u, mm = warp %% %u;\n"
    "  const float* ol = olat + (long)h * %u;\n"
    "  const bf16t* wm = wuv + ((long)h * %u + mm) * %u;\n"
    "  float acc = 0.f;\n"
    "  for (int k = lane; k < %u; k += 32) acc += ol[k] * bf2f(wm[k]);\n"
    "  for (int oo = 16; oo > 0; oo >>= 1) acc += __shfl_down_sync(0xffffffffu, acc, oo);\n"
    "  if (lane == 0) o[(long)h * %u + mm] = f2bfd(acc);\n"
    "}\n", h * dv, dv, dv, kv, dv, kv, kv, dv);
  return mks(
    "void mla_o(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *o = (bf16t *)bufs[0]; const float *olat = (const float *)bufs[1]; const bf16t *wuv = (const bf16t *)bufs[2];\n"
    "  for (int h = 0; h < %u; h++)\n"
    "    for (int mm = 0; mm < %u; mm++) {\n"
    "      float acc = 0.f;\n"
    "      for (int k = 0; k < %u; k++) acc += olat[(long)h * %u + k] * bf2f(wuv[((long)h * %u + mm) * %u + k]);\n"
    "      o[(long)h * %u + mm] = f2bfd(acc);\n"
    "    }\n"
    "}\n", h, dv, kv, kv, dv, kv, dv);
}

// route: greedy top-K of the E gate logits + full-softmax probs, no renorm
// (replaces deepseek_v2.py:1141-1143 on-device topk); ties -> lowest id
static char *src_route(void) {
  u32 e = C.n_exp, k = C.top_k;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void route(int* sel, float* probs, const float* g) {\n"
    "  if (threadIdx.x != 0 || blockIdx.x != 0) return;\n"
    "  float mx = g[0];\n"
    "  for (int e = 1; e < %u; e++) mx = fmaxf(mx, g[e]);\n"
    "  float sum = 0.f;\n"
    "  for (int e = 0; e < %u; e++) sum += __expf(g[e] - mx);\n"
    "  int taken[%u];\n"
    "  for (int kk = 0; kk < %u; kk++) {\n"
    "    int best = -1; float bv = -3.4e38f;\n"
    "    for (int e = 0; e < %u; e++) {\n"
    "      int used = 0;\n"
    "      for (int j = 0; j < kk; j++) if (taken[j] == e) used = 1;\n"
    "      if (!used && g[e] > bv) { bv = g[e]; best = e; }\n"
    "    }\n"
    "    taken[kk] = best; sel[kk] = best;\n"
    "    probs[kk] = __expf(g[best] - mx) / sum;\n"
    "  }\n"
    "}\n", e, e, k, k, e);
  return mks(
    "#include <math.h>\n"
    "void route(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *sel = (int *)bufs[0]; float *probs = (float *)bufs[1]; const float *g = (const float *)bufs[2];\n"
    "  float mx = g[0];\n"
    "  for (int e = 1; e < %u; e++) if (g[e] > mx) mx = g[e];\n"
    "  float sum = 0.f;\n"
    "  for (int e = 0; e < %u; e++) sum += expf(g[e] - mx);\n"
    "  int taken[%u];\n"
    "  for (int kk = 0; kk < %u; kk++) {\n"
    "    int best = -1; float bv = -3.4e38f;\n"
    "    for (int e = 0; e < %u; e++) {\n"
    "      int used = 0;\n"
    "      for (int j = 0; j < kk; j++) if (taken[j] == e) used = 1;\n"
    "      if (!used && g[e] > bv) { bv = g[e]; best = e; }\n"
    "    }\n"
    "    taken[kk] = best; sel[kk] = best;\n"
    "    probs[kk] = expf(g[best] - mx) / sum;\n"
    "  }\n"
    "}\n", e, e, k, k, e);
}

// fully-fused MoE: routed top-K + shared experts in two kernels (verbatim
// port of moe_full_h / moe_full_out, deepseek_v2.py:445-472)
static char *src_moe_h(void) {
  u32 d = C.dim, mi = C.moe_inter, k = C.top_k, si = C.moe_inter * C.n_shared;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void moe_full_h(float* h, const bf16t* x, const int* sel,\n"
    "    const bf16t* wg, const bf16t* wu, const bf16t* swg, const bf16t* swu) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= %u) return;\n"
    "  const bf16t *wgi, *wui;\n"
    "  if (warp < %u) {\n"
    "    int e = warp / %u, i = warp %% %u; int se = sel[e];\n"
    "    wgi = wg + ((long)se * %u + i) * %u; wui = wu + ((long)se * %u + i) * %u;\n"
    "  } else {\n"
    "    int i = warp - %u;\n"
    "    wgi = swg + (long)i * %u; wui = swu + (long)i * %u;\n"
    "  }\n"
    "  float g = 0.f, u = 0.f;\n"
    "  for (int k = lane; k < %u; k += 32) { float xk = bf2f(x[k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }\n"
    "  for (int o = 16; o > 0; o >>= 1) { g += __shfl_down_sync(0xffffffffu, g, o); u += __shfl_down_sync(0xffffffffu, u, o); }\n"
    "  if (lane == 0) { float gs = g / (1.f + __expf(-g)); h[warp] = gs * u; }\n"
    "}\n", k * mi + si, k * mi, mi, mi, mi, d, mi, d, k * mi, d, d, d);
  return mks(
    "#include <math.h>\n"
    "void moe_full_h(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *h = (float *)bufs[0]; const bf16t *x = (const bf16t *)bufs[1]; const int *sel = (const int *)bufs[2];\n"
    "  const bf16t *wg = (const bf16t *)bufs[3]; const bf16t *wu = (const bf16t *)bufs[4];\n"
    "  const bf16t *swg = (const bf16t *)bufs[5]; const bf16t *swu = (const bf16t *)bufs[6];\n"
    "  for (int warp = 0; warp < %u; warp++) {\n"
    "    const bf16t *wgi, *wui;\n"
    "    if (warp < %u) {\n"
    "      int e = warp / %u, i = warp %% %u; int se = sel[e];\n"
    "      wgi = wg + ((long)se * %u + i) * %u; wui = wu + ((long)se * %u + i) * %u;\n"
    "    } else {\n"
    "      int i = warp - %u;\n"
    "      wgi = swg + (long)i * %u; wui = swu + (long)i * %u;\n"
    "    }\n"
    "    float g = 0.f, u = 0.f;\n"
    "    for (int k = 0; k < %u; k++) { float xk = bf2f(x[k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }\n"
    "    float gs = g / (1.f + expf(-g));\n"
    "    h[warp] = gs * u;\n"
    "  }\n"
    "}\n", k * mi + si, k * mi, mi, mi, mi, d, mi, d, k * mi, d, d, d);
}

static char *src_moe_out(void) {
  u32 d = C.dim, mi = C.moe_inter, k = C.top_k, si = C.moe_inter * C.n_shared;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void moe_full_out(bf16t* out, const float* h, const int* sel,\n"
    "    const float* probs, const bf16t* wd, const bf16t* swd) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= %u) return;\n"
    "  int d = warp; float acc = 0.f;\n"
    "  for (int e = 0; e < %u; e++) {\n"
    "    int se = sel[e];\n"
    "    const bf16t* wde = wd + ((long)se * %u + d) * %u;\n"
    "    const float* he = h + e * %u;\n"
    "    float s = 0.f;\n"
    "    for (int i = lane; i < %u; i += 32) s += he[i] * bf2f(wde[i]);\n"
    "    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);\n"
    "    if (lane == 0) acc += probs[e] * s;\n"
    "  }\n"
    "  {\n"
    "    const bf16t* wde = swd + (long)d * %u;\n"
    "    const float* hs = h + %u;\n"
    "    float s = 0.f;\n"
    "    for (int i = lane; i < %u; i += 32) s += hs[i] * bf2f(wde[i]);\n"
    "    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);\n"
    "    if (lane == 0) acc += s;\n"
    "  }\n"
    "  if (lane == 0) out[d] = f2bfd(acc);\n"
    "}\n", d, k, d, mi, mi, mi, si, k * mi, si);
  return mks(
    "void moe_full_out(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *out = (bf16t *)bufs[0]; const float *h = (const float *)bufs[1]; const int *sel = (const int *)bufs[2];\n"
    "  const float *probs = (const float *)bufs[3]; const bf16t *wd = (const bf16t *)bufs[4]; const bf16t *swd = (const bf16t *)bufs[5];\n"
    "  for (int d = 0; d < %u; d++) {\n"
    "    float acc = 0.f;\n"
    "    for (int e = 0; e < %u; e++) {\n"
    "      int se = sel[e];\n"
    "      float s = 0.f;\n"
    "      for (int i = 0; i < %u; i++) s += h[e * %u + i] * bf2f(wd[((long)se * %u + d) * %u + i]);\n"
    "      acc += probs[e] * s;\n"
    "    }\n"
    "    {\n"
    "      float s = 0.f;\n"
    "      for (int i = 0; i < %u; i++) s += h[%u + i] * bf2f(swd[(long)d * %u + i]);\n"
    "      acc += s;\n"
    "    }\n"
    "    out[d] = f2bfd(acc);\n"
    "  }\n"
    "}\n", d, k, mi, mi, d, mi, si, k * mi, si);
}

// argmax_lastpos, 2-pass (replaces deepseek_v2.py:1255); ties -> lowest index
static char *src_apart(void) {
  u32 v = C.vocab, nb = C.nb, ch = C.chunk;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void argmax_part(float* pv, int* pi, const float* lg) {\n"
    "  __shared__ float sv[256]; __shared__ int si[256];\n"
    "  int b = blockIdx.x; int t = threadIdx.x;\n"
    "  long lo = (long)b * %u; long hi = lo + %u; if (hi > %u) hi = %u;\n"
    "  float bv = -3.4e38f; int bi = 2147483647;\n"
    "  for (long i = lo + t; i < hi; i += blockDim.x) {\n"
    "    float x = lg[i];\n"
    "    if (x > bv || (x == bv && (int)i < bi)) { bv = x; bi = (int)i; }\n"
    "  }\n"
    "  sv[t] = bv; si[t] = bi;\n"
    "  __syncthreads();\n"
    "  for (int s = blockDim.x / 2; s > 0; s >>= 1) {\n"
    "    if (t < s && (sv[t + s] > sv[t] || (sv[t + s] == sv[t] && si[t + s] < si[t]))) { sv[t] = sv[t + s]; si[t] = si[t + s]; }\n"
    "    __syncthreads();\n"
    "  }\n"
    "  if (t == 0) { pv[b] = sv[0]; pi[b] = si[0]; }\n"
    "}\n", ch, ch, v, v);
  return mks(
    "void argmax_part(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *pv = (float *)bufs[0]; int *pi = (int *)bufs[1]; const float *lg = (const float *)bufs[2];\n"
    "  for (int b = 0; b < %u; b++) {\n"
    "    long lo = (long)b * %u; long hi = lo + %u; if (hi > %u) hi = %u;\n"
    "    float bv = -3.4e38f; int bi = 2147483647;\n"
    "    for (long i = lo; i < hi; i++)\n"
    "      if (lg[i] > bv) { bv = lg[i]; bi = (int)i; }\n"
    "    pv[b] = bv; pi[b] = bi;\n"
    "  }\n"
    "}\n", nb, ch, ch, v, v);
}

static char *src_afin(void) {
  u32 nb = C.nb;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void argmax_fin(int* tok, const float* pv, const int* pi) {\n"
    "  if (threadIdx.x != 0 || blockIdx.x != 0) return;\n"
    "  float bv = pv[0]; int bi = pi[0];\n"
    "  for (int b = 1; b < %u; b++)\n"
    "    if (pv[b] > bv || (pv[b] == bv && pi[b] < bi)) { bv = pv[b]; bi = pi[b]; }\n"
    "  tok[0] = bi;\n"
    "}\n", nb);
  return mks(
    "void argmax_fin(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *tok = (int *)bufs[0]; const float *pv = (const float *)bufs[1]; const int *pi = (const int *)bufs[2];\n"
    "  float bv = pv[0]; int bi = pi[0];\n"
    "  for (int b = 1; b < %u; b++)\n"
    "    if (pv[b] > bv || (pv[b] == bv && pi[b] < bi)) { bv = pv[b]; bi = pi[b]; }\n"
    "  tok[0] = bi;\n"
    "}\n", nb);
}

// len_bump: the device position advances by 1 (the Ltok pattern,
// deepseek_v2.py:166-170)
static char *src_bump(void) {
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void len_bump(int* L) {\n"
    "  if (threadIdx.x == 0 && blockIdx.x == 0) L[0] = L[0] + 1;\n"
    "}\n");
  return mks(
    "void len_bump(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  ((int *)bufs[0])[0] += 1;\n"
    "}\n");
}

// === custom fire ============================================================
static void krun(char const *name, char const *src, u32 gx, u32 bx,
                 u32 const *args, u32 nargs, u32 const *outs, u32 nouts) {
  u32 grid[3] = {gx, 1, 1}, block[3] = {bx, 1, 1};
  u32 kid = kernel_emit_custom_program(name, src, grid, block, args, nargs, outs, nouts, 0);
  if (kid == 0 || kernel_custom_fire(kid) != 0) {
    fprintf(stderr, "FATAL: custom kernel %s failed\n", name);
    exit(1);
  }
}

// === native uop pieces (mirror the WL example) ==============================
static u32 cdiv(u32 a, u32 b) { return (a + b - 1) / b; }

// x{1,K} @ W{K,N} -> {1,N}: the TMatMul lowering (NN/Linear.wl:49-58)
static Term mm(Term x, u32 K, u32 w_tid, u32 N) {
  u32 a3[3] = {1, K, 1}, b3[3] = {1, K, N}, e3[3] = {1, K, N};
  Term ab = uop_expand(uop_reshape(x, 3, a3), 3, e3);
  Term bb = uop_expand(uop_reshape(tt(w_tid), 3, b3), 3, e3);
  return uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, ab, bb));
}

// RMS norm over {1,K} f32 with a realize boundary on the reduce scalar
// (the dsvRmsNorm structure; tinygrad materializes at every sublayer,
// deepseek_v2.py:1211-1216).  Returns a REALIZED {1,K} f32 tensor id.
static u32 rms_norm(Term x, u32 K, u32 w_tid, float eps) {
  u32 one1[1] = {1}, d11[2] = {1, 1}, d1k[2] = {1, K};
  Term xx = uop_binary(UOP_MUL, x, x);
  Term red = uop_reduce(REDUCE_SUM, 1, xx);   // {1}
  Term ms = uop_binary(UOP_MUL, red, uop_expand(uop_const(DT_FP32, fbits(1.0f / (float)K)), 1, one1));
  Term inv = uop_unary(UOP_RECIP, uop_unary(UOP_SQRT,
      uop_binary(UOP_ADD, ms, uop_expand(uop_const(DT_FP32, fbits(eps)), 1, one1))));
  u32 inv_tid = rz(inv);
  Term invB = uop_expand(uop_reshape(tt(inv_tid), 2, d11), 2, d1k);
  Term wB = uop_expand(uop_reshape(tt(w_tid), 2, d1k), 2, d1k);
  return rz(uop_binary(UOP_MUL, uop_binary(UOP_MUL, x, invB), wB));
}

// silu(x) = x / (1 + exp(-x)) via EXP2 (the TSiLU lowering); x bf16 {1,K}
static Term silu(Term x, u32 K) {
  u32 d1k[2] = {1, K};
  Term nlog2e = uop_expand(uop_const(DT_BF16, fbits(-1.4426950408889634f)), 2, d1k);
  Term one = uop_expand(uop_const(DT_BF16, fbits(1.0f)), 2, d1k);
  Term sig = uop_unary(UOP_RECIP,
      uop_binary(UOP_ADD, one, uop_unary(UOP_EXP2, uop_binary(UOP_MUL, x, nlog2e))));
  return uop_binary(UOP_MUL, x, sig);
}

// === model state ============================================================
#define MAX_LAYERS 64
static u32 W_embed, W_norm, W_lm, T_cos, T_sin;
static u32 W_win[MAX_LAYERS], W_wq[MAX_LAYERS], W_wkv[MAX_LAYERS], W_wkvn[MAX_LAYERS];
static u32 W_wuk[MAX_LAYERS], W_wuv[MAX_LAYERS], W_wo[MAX_LAYERS], W_wpost[MAX_LAYERS];
static u32 W_wg1[MAX_LAYERS], W_wu1[MAX_LAYERS], W_wd1[MAX_LAYERS];
static u32 W_gate[MAX_LAYERS], W_wg[MAX_LAYERS], W_wu[MAX_LAYERS], W_wd[MAX_LAYERS];
static u32 W_swg[MAX_LAYERS], W_swu[MAX_LAYERS], W_swd[MAX_LAYERS];
static u32 L_ckv[MAX_LAYERS], L_kpe[MAX_LAYERS];
static u32 T_x, T_tok, T_len, T_ql, T_sm, T_ol, T_ob, T_sel, T_probs, T_h, T_moe, T_pv, T_pi;
static u32 LOGITS_TID = 0;

static char *S_EMBED, *S_ROPEQ, *S_ROPEK, *S_APPEND, *S_QLAT, *S_SCORE, *S_MIX, *S_O;
static char *S_ROUTE, *S_MOEH, *S_MOEOUT, *S_APART, *S_AFIN, *S_BUMP;

static void model_init(void) {
  u32 d = C.dim, hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, kv = C.kv_lora, dv = C.d_v;
  u32 si = C.moe_inter * C.n_shared;
  W_embed = mk_bf16_rand(C.vocab, d);
  W_norm  = mk_f32_fill(d, 0, 1.0f);
  W_lm    = mk_bf16_rand(d, C.vocab);
  rope_tables(&T_cos, &T_sin);
  for (u32 l = 0; l < C.n_layers; l++) {
    W_win[l]   = mk_f32_fill(d, 0, 1.0f);
    W_wq[l]    = mk_bf16_rand(d, hn * (dn + dr));
    W_wkv[l]   = mk_bf16_rand(d, kv + dr);
    W_wkvn[l]  = mk_f32_fill(kv, 0, 1.0f);
    W_wuk[l]   = mk_bf16_rand(hn * dn * kv, 0);
    W_wuv[l]   = mk_bf16_rand(hn * dv * kv, 0);
    W_wo[l]    = mk_bf16_rand(hn * dv, d);
    W_wpost[l] = mk_f32_fill(d, 0, 1.0f);
    if (l < C.first_dense) {
      W_wg1[l] = mk_bf16_rand(d, C.dense_inter);
      W_wu1[l] = mk_bf16_rand(d, C.dense_inter);
      W_wd1[l] = mk_bf16_rand(C.dense_inter, d);
    } else {
      W_gate[l] = mk_f32_rand(d, C.n_exp, 0.05f);
      W_wg[l]   = mk_bf16_rand(C.n_exp * C.moe_inter, d);
      W_wu[l]   = mk_bf16_rand(C.n_exp * C.moe_inter, d);
      W_wd[l]   = mk_bf16_rand(C.n_exp * d, C.moe_inter);
      W_swg[l]  = mk_bf16_rand(si, d);
      W_swu[l]  = mk_bf16_rand(si, d);
      W_swd[l]  = mk_bf16_rand(d, si);
    }
    L_ckv[l] = mk_bf16_zero(C.max_seq, kv);
    L_kpe[l] = mk_bf16_zero(C.max_seq, dr);
  }
  T_x     = mk_bf16_zero(d, 0);
  T_tok   = mk_i32(1, 1);
  T_len   = mk_i32(1, 1);
  T_ql    = mk_f32_fill(hn * kv, 0, 0.0f);
  T_sm    = mk_f32_fill(hn * C.max_seq, 0, 0.0f);
  T_ol    = mk_f32_fill(hn * kv, 0, 0.0f);
  T_ob    = mk_bf16_zero(hn * dv, 0);
  T_sel   = mk_i32(C.top_k, 0);
  T_probs = mk_f32_fill(C.top_k, 0, 0.0f);
  T_h     = mk_f32_fill(C.top_k * C.moe_inter + si, 0, 0.0f);
  T_moe   = mk_bf16_zero(d, 0);
  T_pv    = mk_f32_fill(C.nb, 0, 0.0f);
  T_pi    = mk_i32(C.nb, 0);
  S_EMBED = src_embed(); S_ROPEQ = src_rope(hn); S_ROPEK = src_rope(1);
  S_APPEND = src_append(); S_QLAT = src_qlat(); S_SCORE = src_score();
  S_MIX = src_mix(); S_O = src_mla_o();
  S_ROUTE = src_route(); S_MOEH = src_moe_h(); S_MOEOUT = src_moe_out();
  S_APART = src_apart(); S_AFIN = src_afin(); S_BUMP = src_bump();
}

// === the decode step (built + fired once, under capture) ===================
static void step_build(void) {
  u32 d = C.dim, hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, kv = C.kv_lora, dv = C.d_v;
  u32 si = C.moe_inter * C.n_shared;
  u32 d1d[2] = {1, d};
  {
    u32 a[3] = {T_x, W_embed, T_tok}, o[1] = {0};
    krun("embed_row", S_EMBED, cdiv(d, 256), 256, a, 3, o, 1);
  }
  Term x = uop_reshape(tt(T_x), 2, d1d);
  for (u32 l = 0; l < C.n_layers; l++) {
    u32 xn_tid = rms_norm(uop_cast(x, DT_FP32), d, W_win[l], C.eps);
    Term xn = tt(rz(uop_cast(tt(xn_tid), DT_BF16)));
    u32 q_tid = rz(mm(xn, d, W_wq[l], hn * (dn + dr)));
    u32 hdd[2] = {hn, dn + dr};
    Term qm = uop_reshape(tt(q_tid), 2, hdd);
    u32 sn_[4] = {0, hn, 0, dn}, sp_[4] = {0, hn, dn, dn + dr};
    u32 qn_tid = rz(uop_shrink(qm, 2, sn_));
    u32 qp_tid = rz(uop_shrink(qm, 2, sp_));
    {
      u32 a[4] = {qp_tid, T_cos, T_sin, T_len}, o[1] = {0};
      krun("rope_dev", S_ROPEQ, hn, dr / 2, a, 4, o, 1);
    }
    u32 kva_tid = rz(mm(xn, d, W_wkv[l], kv + dr));
    u32 sc_[4] = {0, 1, 0, kv}, sk_[4] = {0, 1, kv, kv + dr};
    Term kvam = tt(kva_tid);
    u32 ckvn_tid = rms_norm(uop_cast(uop_shrink(kvam, 2, sc_), DT_FP32), kv, W_wkvn[l], C.eps);
    u32 ckv_tid = rz(uop_cast(tt(ckvn_tid), DT_BF16));
    u32 kp_tid = rz(uop_shrink(kvam, 2, sk_));
    {
      u32 a[4] = {kp_tid, T_cos, T_sin, T_len}, o[1] = {0};
      krun("rope_dev", S_ROPEK, 1, dr / 2, a, 4, o, 1);
    }
    {
      u32 a[5] = {L_ckv[l], L_kpe[l], ckv_tid, kp_tid, T_len}, o[2] = {0, 1};
      krun("cache_append", S_APPEND, cdiv(kv + dr, 256), 256, a, 5, o, 2);
    }
    {
      u32 a[3] = {T_ql, qn_tid, W_wuk[l]}, o[1] = {0};
      krun("mla_qlat", S_QLAT, cdiv(hn * kv, 256), 256, a, 3, o, 1);
    }
    {
      u32 a[8] = {T_sm, T_ql, qp_tid, L_ckv[l], L_kpe[l], ckv_tid, kp_tid, T_len}, o[1] = {0};
      krun("mla_score", S_SCORE, hn, 256, a, 8, o, 1);
    }
    {
      u32 a[5] = {T_ol, T_sm, L_ckv[l], ckv_tid, T_len}, o[1] = {0};
      krun("mla_mix", S_MIX, cdiv(hn * kv, 256), 256, a, 5, o, 1);
    }
    {
      u32 a[3] = {T_ob, T_ol, W_wuv[l]}, o[1] = {0};
      krun("mla_o", S_O, cdiv(hn * dv * 32, 256), 256, a, 3, o, 1);
    }
    u32 dhv[2] = {1, hn * dv};
    u32 attn_tid = rz(mm(uop_reshape(tt(T_ob), 2, dhv), hn * dv, W_wo[l], d));
    u32 x1_tid = rz(uop_binary(UOP_ADD, x, tt(attn_tid)));
    Term x1 = tt(x1_tid);
    u32 xn2_tid = rms_norm(uop_cast(x1, DT_FP32), d, W_wpost[l], C.eps);
    if (l < C.first_dense) {
      Term xb = tt(rz(uop_cast(tt(xn2_tid), DT_BF16)));
      u32 g1 = rz(mm(xb, d, W_wg1[l], C.dense_inter));
      u32 u1 = rz(mm(xb, d, W_wu1[l], C.dense_inter));
      Term h = uop_binary(UOP_MUL, silu(tt(g1), C.dense_inter), tt(u1));
      u32 mlp_tid = rz(mm(h, C.dense_inter, W_wd1[l], d));
      x = tt(rz(uop_binary(UOP_ADD, x1, tt(mlp_tid))));
    } else {
      u32 g_tid = rz(mm(tt(xn2_tid), d, W_gate[l], C.n_exp));
      {
        u32 a[3] = {T_sel, T_probs, g_tid}, o[2] = {0, 1};
        krun("route", S_ROUTE, 1, 32, a, 3, o, 2);
      }
      u32 moein_tid = rz(uop_cast(tt(xn2_tid), DT_BF16));
      {
        u32 a[7] = {T_h, moein_tid, T_sel, W_wg[l], W_wu[l], W_swg[l], W_swu[l]}, o[1] = {0};
        krun("moe_full_h", S_MOEH, cdiv((C.top_k * C.moe_inter + si) * 32, 256), 256, a, 7, o, 1);
      }
      {
        u32 a[6] = {T_moe, T_h, T_sel, T_probs, W_wd[l], W_swd[l]}, o[1] = {0};
        krun("moe_full_out", S_MOEOUT, cdiv(d * 32, 256), 256, a, 6, o, 1);
      }
      x = tt(rz(uop_binary(UOP_ADD, x1, uop_reshape(tt(T_moe), 2, d1d))));
    }
  }
  u32 xnf_tid = rms_norm(uop_cast(x, DT_FP32), d, W_norm, C.eps);
  LOGITS_TID = rz(mm(tt(xnf_tid), d, W_lm, C.vocab));
  {
    u32 a[3] = {T_pv, T_pi, LOGITS_TID}, o[2] = {0, 1};
    krun("argmax_part", S_APART, C.nb, 256, a, 3, o, 2);
  }
  {
    u32 a[3] = {T_tok, T_pv, T_pi}, o[1] = {0};
    krun("argmax_fin", S_AFIN, 1, 32, a, 3, o, 1);
  }
  {
    u32 a[1] = {T_len}, o[1] = {0};
    krun("len_bump", S_BUMP, 1, 1, a, 1, o, 1);
  }
}

static int read_i32(u32 tid) {
  int v = 0;
  TENS[tid].backend->buf_read(TENS[tid].buf_id, &v, 4);
  return v;
}

int main(void) {
  char const *dev = getenv("DEV");
  IS_CUDA = (dev != NULL && strcmp(dev, "cuda") == 0);
  thvm_init();
  if (IS_CUDA) {
#ifdef THVM_HAS_CUDA
    if (thvm_cuda_init() != 0) {
      printf("pend: no CUDA device: %s\n", thvm_cuda_last_error());
      thvm_free();
      return 0;
    }
    BK = &CUDA_BACKEND;
#else
    fprintf(stderr, "FATAL: DEV=cuda but built without CUDA\n");
    return 1;
#endif
  } else {
    BK = &CPU_BACKEND;
  }
  cfg_init();
  u32 warm = envu("WARM", 20), steps = envu("STEPS", 40);
  if (1 + warm + steps + 1 > C.max_seq) {
    fprintf(stderr, "FATAL: 1+WARM+STEPS+1=%u exceeds DSV2_MAXSEQ=%u\n", 1 + warm + steps + 1, C.max_seq);
    return 1;
  }
  printf("dsv2 decode bench: dev=%s dim=%u layers=%u heads=%u kv=%u experts=%u topk=%u vocab=%u maxseq=%u\n",
         IS_CUDA ? "cuda" : "cpu", C.dim, C.n_layers, C.n_heads, C.kv_lora, C.n_exp, C.top_k, C.vocab, C.max_seq);

  double t0 = now_ms();
  model_init();
  printf("  synth init: %.2f s\n", (now_ms() - t0) / 1e3);

  // capture: build + fire the whole step once (each recorded buffer pinned)
  t0 = now_ms();
  u32 slot = jit_capture_begin();
  if (slot == 0) { fprintf(stderr, "FATAL: no capture slot\n"); return 1; }
  step_build();
  jit_capture_end();
  printf("  capture (step 1): %.2f s   ops=%u   tok=%d len=%d\n",
         (now_ms() - t0) / 1e3, jit_capture_op_count(slot), read_i32(T_tok), read_i32(T_len));

  for (u32 i = 0; i < warm; i++) jit_replay(slot);
  printf("  warm: %u replays done, tok=%d len=%d\n", warm, read_i32(T_tok), read_i32(T_len));

  double mn = 1e30, total = 0;
  for (u32 i = 0; i < steps; i++) {
    double s0 = now_ms();
    jit_replay(slot);
    double dt = now_ms() - s0;
    total += dt;
    if (dt < mn) mn = dt;
  }
  double mean = total / (double)steps;
  printf("  timed %u steps: min %.2f ms/step   mean %.2f ms/step   %.1f tok/s (min)   %.1f tok/s (mean)\n",
         steps, mn, mean, 1e3 / mn, 1e3 / mean);

  int tok = read_i32(T_tok), len = read_i32(T_len);
  int want_len = (int)(1 + 1 + warm + steps);
  float lg[4] = {0};
  TENS[LOGITS_TID].backend->buf_read(TENS[LOGITS_TID].buf_id, lg, sizeof lg);
  int finite = isfinite(lg[0]) && isfinite(lg[1]) && isfinite(lg[2]) && isfinite(lg[3]);
  printf("  final tok=%d len=%d (want %d)  logits[0..3]=%.4f %.4f %.4f %.4f\n",
         tok, len, want_len, lg[0], lg[1], lg[2], lg[3]);
  int ok = (len == want_len) && tok >= 0 && (u32)tok < C.vocab && finite;
  printf("%s\n", ok ? "OK" : "FAIL(state)");
  return ok ? 0 : 1;
}
