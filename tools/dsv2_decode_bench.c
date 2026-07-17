// tools/dsv2_decode_bench.c - DeepSeek-V2-Lite BS=1 decode-step microbenchmark
// at the C level (no Wolfram on the pod).  Same math as the WL example
// (wl/THVMLink/Kernel/NN/Examples/DeepSeekV2/Decode.wl), port of tinygrad
// examples/deepseek_v2.py, restructured into ~14 fused custom kernels per
// layer (the original native-uop lowering ran 20.8 ms/step on H100; this
// pipeline runs ~5.1 ms/step, gen ids byte-identical):
//
//   embed_row -> N x [ rms1 -> gemv_qkv + gred_q(fold+rope) + kv_prep(fold+
//                      rms+rope+cache append) -> mla_qlat/score/mix/o ->
//                      gemv_o + gred_o_rms(fold+residual+rms) ->
//                      gemv_gate + route(fold+softmax+topk) -> moe_full_h ->
//                      moe_full_out(+residual) ]
//   -> rms_f -> gemv_lm/gred_lm -> argmax_part/argmax_fin -> len_bump
//
// Every M=1 projection is a two-phase split-K GEMV (phase 1 writes
// part[s*N+n] partials, K split over blockIdx.y -- the flat one-thread-
// per-output form ran at 17-90 GB/s; split-K reaches ~1-1.3 TB/s); each
// phase-2 fold is fused into the consumer kernel.  All kernels are injected
// customs (kernel_emit_custom_program) with per-backend sources: CUDA-C,
// plain-C functional twins for the CPU custom ABI.  The one remaining
// scheduler kernel is the dense-MLP silu*u elementwise.
//
// NUMERICS CONTRACT: the decode trajectory must stay byte-identical to the
// tinygrad reference, and near-tie tokens flip under ANY f32 accumulation
// reorder (bf16 rounding chaos-amplifies a 1-ulp drift across 26 layers;
// measured margin at the first tie is 0.19 logits).  Rules baked into the
// sources: rms sums of squares replicate the native scalar-reduce lowering
// (16 strided lane accumulators + in-order fold); GEMV folds keep fixed
// K-chunk boundaries (the merged q|kv GEMV pins sk to the q/kv pick);
// matmul outputs round to bf16 exactly where the native chain did
// (bf2f(f2bfd(s)) before rope/residual use); MoE lane loops keep
// k = lane + 32*i (a ushort4 remap was 0.3 ms/step faster and REFUTED by
// the gate); route parallelizes only exact ops (compares/fmaxf), keeping
// the serial softmax fold.
//
// The whole step is built + fired ONCE under jit_capture_begin/end (capture
// pins every recorded buffer), then WARM + STEPS replays run it; the token
// argmax feeds back into the device token buffer and the position lives in a
// device int32 buffer, so a replay is one full decode step with zero host
// values.
//
// REAL WEIGHTS (Stage 2): DSV2_WEIGHTS=<pack> loads a flat weight pack written
// by tools/dsv2_pack_weights.py (the tinygrad load_weights mapping,
// deepseek_v2.py:1257-1272 + the kv_b absorb split :993-997, pre-applied so
// every payload is already in the exact layout the step consumes; the merged
// q|kv weight is row-concatenated at load).
// DSV2_PROMPT_IDS=comma-list prefills token-by-token through the same decode
// step (each prompt id is force-written into the device token buffer, the
// step's own argmax prediction is discarded until the last prompt token), then
// generates DSV2_COUNT (24) greedy tokens and prints them for comparison with
// the tinygrad reference trajectory ("MY gen ids").  DSV2_TOP2=1 additionally
// dumps the top-2 logits per generated token (near-tie divergence forensics);
// DSV2_DUMP=1 prints byte fingerprints (per-layer cache rows, activations,
// logits) after the captured step for build-vs-build divergence bisection.
//
// SPECULATIVE DECODE (Stage 3): DSV2_SPEC=S arms a SECOND captured step, the
// blind spec step (port of tinygrad examples/deepseek_v2_speculative.py
// --blind): draft_prep (most-recent-bigram lookup over a persistent device ids
// buffer) -> S-token verify forward (S-row twins of every base kernel; the
// weight-heavy phases hoist each weight and accumulate per row in the base
// kernel's exact fold order, so verify row 0 is bit-equal to the base step --
// asserted at startup by a memcmp of the two logit rows) -> accept_commit
// (longest verified draft prefix + bonus token; the device length advances by
// k+1).  Bit-exact verify makes the spec stream reproduce the base greedy
// trajectory EXACTLY; drafts only buy throughput.  Replayed in host-blind
// bursts (one length readback per burst).  DSV2_FORCE_IDS=<base gen ids> seeds
// the draft stream for the forced-acceptance gate (every fully-seeded step
// must advance by exactly S).
//
//   build:  make bin/dsv2_decode_bench            (Linux+CUDA adds the CUDA backend)
//   run:    DEV=cuda ./bin/dsv2_decode_bench      (full model shape, ~31 GB device)
//           DEV=cuda DSV2_WEIGHTS=dsv2.pack DSV2_PROMPT_IDS=100000,504,... \
//             DSV2_COUNT=24 ./bin/dsv2_decode_bench   (real-weight greedy decode)
//           DEV=cpu DSV2_DIM=128 DSV2_LAYERS=4 DSV2_HEADS=8 DSV2_KVLORA=64 \
//             DSV2_DNOPE=32 DSV2_DROPE=16 DSV2_DV=32 DSV2_EXPERTS=8 DSV2_TOPK=3 \
//             DSV2_MOEINTER=64 DSV2_DENSEINTER=256 DSV2_VOCAB=1024 DSV2_MAXSEQ=128 \
//             ./bin/dsv2_decode_bench             (local CPU smoke, no GPU)
//   knobs:  WARM (20) STEPS (40) DSV2_SPEC (0) DSV2_FORCE_IDS + every DSV2_* shape knob.
//
// SERVE (DSV2_SERVE=1): stdin request loop for the tools/thvm_llm_service
// front-end -- weights + captured step(s) stay resident, "ready" then one
// "ids: ..." response line per prompt-ids request line; see serve_loop below.
// Knobs: DSV2_MAXNEW (256, per-request default cap), DSV2_EOS (100001).
#include "../src/thvm.c"
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

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

static u32 cdiv(u32 a, u32 b) { return (a + b - 1) / b; }

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

// === weight pack (tools/dsv2_pack_weights.py) ===============================
// "DSV2PK01" magic, u64 count, count x {char name[48]; u32 dtype (0=bf16,
// 1=f32); u32 pad; u64 offset; u64 nbytes}, then 64-byte-aligned payloads
// already in the exact layouts the kernels index (transposed {in,out}
// projections, absorbed w_uk/w_uv, stacked experts, f32 norm/gate weights).
typedef struct { char name[48]; u32 dtype; u32 pad; u64 off; u64 nbytes; } PackEnt;
static u8 const *PACK = NULL;
static u64 PACK_N = 0;
static PackEnt const *PACK_TAB = NULL;

static void pack_open(char const *path) {
  int fd = open(path, O_RDONLY);
  struct stat st;
  if (fd < 0 || fstat(fd, &st) != 0) { fprintf(stderr, "FATAL: DSV2_WEIGHTS open %s\n", path); exit(1); }
  PACK = (u8 const *)mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (PACK == MAP_FAILED || memcmp(PACK, "DSV2PK01", 8) != 0) {
    fprintf(stderr, "FATAL: bad pack %s\n", path);
    exit(1);
  }
  memcpy(&PACK_N, PACK + 8, 8);
  PACK_TAB = (PackEnt const *)(PACK + 16);
  printf("  weight pack: %s (%llu tensors, %.2f GB)\n", path,
         (unsigned long long)PACK_N, (double)st.st_size / 1e9);
}

// Find + upload one pack tensor; the byte count double-checks the layout
// contract between the packer and this step's shapes.
static u32 pack_ld(char const *name, u32 a, u32 b, u32 dtype) {
  u64 want = (u64)a * (b ? b : 1) * (dtype == DT_BF16 ? 2 : 4);
  for (u64 i = 0; i < PACK_N; i++) {
    if (strncmp(PACK_TAB[i].name, name, 48) != 0) continue;
    if (PACK_TAB[i].dtype != (dtype == DT_BF16 ? 0u : 1u) || PACK_TAB[i].nbytes != want) {
      fprintf(stderr, "FATAL: pack tensor %s: dtype %u nbytes %llu, want %u/%llu\n",
              name, PACK_TAB[i].dtype, (unsigned long long)PACK_TAB[i].nbytes,
              dtype == DT_BF16 ? 0u : 1u, (unsigned long long)want);
      exit(1);
    }
    u32 tid = alloc2(a, b, dtype);
    BK->buf_write(TENS[tid].buf_id, PACK + PACK_TAB[i].off, want);
    return tid;
  }
  fprintf(stderr, "FATAL: pack tensor %s missing\n", name);
  exit(1);
}

// Find one pack payload pointer (bf16), validating the byte count.
static u8 const *pack_find(char const *name, u64 want) {
  for (u64 i = 0; i < PACK_N; i++) {
    if (strncmp(PACK_TAB[i].name, name, 48) != 0) continue;
    if (PACK_TAB[i].dtype != 0u || PACK_TAB[i].nbytes != want) {
      fprintf(stderr, "FATAL: pack tensor %s: nbytes %llu, want %llu\n",
              name, (unsigned long long)PACK_TAB[i].nbytes, (unsigned long long)want);
      exit(1);
    }
    return PACK + PACK_TAB[i].off;
  }
  fprintf(stderr, "FATAL: pack tensor %s missing\n", name);
  exit(1);
}

// Row-concat two bf16 [K, na] / [K, nb] host payloads into one [K, na+nb]
// device tensor (the merged q|kv projection weight).
static u32 concat_rows_bf16(u16 const *a, u32 na, u16 const *b, u32 nb, u32 K) {
  u32 n = na + nb;
  u32 tid = alloc2(K, n, DT_BF16);
  u16 *pbuf = (u16 *)stage((u64)K * n * 2);
  for (u32 k = 0; k < K; k++) {
    memcpy(pbuf + (u64)k * n,      a + (u64)k * na, (u64)na * 2);
    memcpy(pbuf + (u64)k * n + na, b + (u64)k * nb, (u64)nb * 2);
  }
  BK->buf_write(TENS[tid].buf_id, pbuf, (u64)K * n * 2);
  return tid;
}

static u32 pack_lw(u32 l, char const *suffix, u32 a, u32 b, u32 dtype) {
  char nm[48];
  snprintf(nm, sizeof nm, "l%u.%s", l, suffix);
  return pack_ld(nm, a, b, dtype);
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

// fused rms-norm: y = bf16/f32 of (bf2f(x)*inv)*w, inv = 1/sqrt(mean(x^2)+eps)
// (replaces the native 3-kernel chain reduce[1] + normalize[1,K] + cast).
// The CUDA sum of squares replicates the native scalar-reduce lowering
// BIT-EXACTLY: 16 strided lane accumulators (lane j folds elements j, j+16,
// ...) then an in-order fold of the 16 partials -- any other order drifts
// the trajectory off the reference at near-tie tokens (seen at gen step 8).
static char *src_rms(char const *nm, u32 K, int out_bf16) {
  char const *ot  = out_bf16 ? "bf16t" : "float";
  char const *ost = out_bf16 ? "f2bfd(v)" : "v";
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void %s(%s* y, const bf16t* x, const float* w) {\n"
    "  int t = threadIdx.x; int nt = blockDim.x;\n"
    "  __shared__ float acc[16]; __shared__ float invs;\n"
    "  if (t < 16) {\n"
    "    float a = 0.0f;\n"
    "    for (int i = t; i < %u; i += 16) { float xv = bf2f(x[i]); a = a + xv * xv; }\n"
    "    acc[t] = a;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t == 0) {\n"
    "    float s = 0.0f;\n"
    "    for (int i = 0; i < 16; i++) s = s + acc[i];\n"
    "    invs = 1.0f / sqrtf((s * %.10ef) + %.10ef);\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float inv = invs;\n"
    "  for (int i = t; i < %u; i += nt) { float v = (bf2f(x[i]) * inv) * w[i]; y[i] = %s; }\n"
    "}\n", nm, ot, K, 1.0 / (double)K, (double)C.eps, K, ost);
  return mks(
    "#include <math.h>\n"
    "void %s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  %s *y = (%s *)bufs[0]; const bf16t *x = (const bf16t *)bufs[1]; const float *w = (const float *)bufs[2];\n"
    "  float ss = 0.f;\n"
    "  for (unsigned i = 0; i < %uu; i++) { float xv = bf2f(x[i]); ss += xv * xv; }\n"
    "  float inv = 1.0f / sqrtf(ss * %.10ef + %.10ef);\n"
    "  for (unsigned i = 0; i < %uu; i++) { float v = (bf2f(x[i]) * inv) * w[i]; y[i] = %s; }\n"
    "}\n", nm, ot, ot, K, 1.0 / (double)K, (double)C.eps, K, ost);
}

// q fold + rope: folds the q GEMV partials into bf16 q [NH, DN+DR] and ropes
// the pe slice in place at position L[0]-1 (round-to-bf16 BEFORE the rope,
// matching the native mm -> bf16 -> rope_dev chain).  Thread space: NH*DN
// nope folds then NH*DR/2 pe pairs.
static char *src_gred_q_rope(u32 sk, u32 ns) {
  u32 hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, half = dr / 2;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void gred_q(bf16t* q, const float* part, const float* cs, const float* sn, const int* L) {\n"
    "  int t = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (t < %u) {\n"
    "    int h = t / %u, d = t %% %u; int col = h * %u + d;\n"
    "    float s = 0.f;\n"
    "    for (int i = 0; i < %u; i++) s += part[(long)i * %u + col];\n"
    "    q[col] = f2bfd(s);\n"
    "  } else if (t < %u) {\n"
    "    int u = t - %u; int h = u / %u, j = u %% %u;\n"
    "    int base = h * %u + %u;\n"
    "    float xe = 0.f, xo = 0.f;\n"
    "    for (int i = 0; i < %u; i++) { xe += part[(long)i * %u + base + 2 * j]; xo += part[(long)i * %u + base + 2 * j + 1]; }\n"
    "    xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "    int p = L[0] - 1;\n"
    "    float c = cs[(long)p * %u + j], sv = sn[(long)p * %u + j];\n"
    "    q[base + j]      = f2bfd(xe * c - xo * sv);\n"
    "    q[base + %u + j] = f2bfd(xo * c + xe * sv);\n"
    "  }\n"
    "}\n", hn * dn, dn, dn, dn + dr, sk, ns, hn * dn + hn * half, hn * dn,
         half, half, dn + dr, dn, sk, ns, ns, dr, dr, half);
  return mks(
    "void gred_q(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *q = (bf16t *)bufs[0]; const float *part = (const float *)bufs[1];\n"
    "  const float *cs = (const float *)bufs[2]; const float *sn = (const float *)bufs[3]; const int *L = (const int *)bufs[4];\n"
    "  int p = L[0] - 1;\n"
    "  for (int h = 0; h < %u; h++) {\n"
    "    for (int d = 0; d < %u; d++) {\n"
    "      int col = h * %u + d; float s = 0.f;\n"
    "      for (unsigned i = 0; i < %uu; i++) s += part[(unsigned long)i * %uu + col];\n"
    "      q[col] = f2bfd(s);\n"
    "    }\n"
    "    for (int j = 0; j < %u; j++) {\n"
    "      int base = h * %u + %u;\n"
    "      float xe = 0.f, xo = 0.f;\n"
    "      for (unsigned i = 0; i < %uu; i++) { xe += part[(unsigned long)i * %uu + base + 2 * j]; xo += part[(unsigned long)i * %uu + base + 2 * j + 1]; }\n"
    "      xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "      float c = cs[(long)p * %u + j], sv = sn[(long)p * %u + j];\n"
    "      q[base + j]      = f2bfd(xe * c - xo * sv);\n"
    "      q[base + %u + j] = f2bfd(xo * c + xe * sv);\n"
    "    }\n"
    "  }\n"
    "}\n", hn, dn, dn + dr, sk, ns, half, dn + dr, dn, sk, ns, ns, dr, dr, half);
}

// kv prep: folds the kv GEMV partials, rms-norms the compressed-kv half
// (round-to-bf16 first, matching the native kva bf16 boundary), ropes the
// pe tail, and appends BOTH rows into the layer caches at L[0]-1.  One
// block; threads < KV handle ckv columns, the next DR/2 handle pe pairs.
static char *src_kv_prep(u32 sk, u32 ns, u32 qoff) {
  u32 kv = C.kv_lora, dr = C.d_rope, half = dr / 2;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void kv_prep(bf16t* ckv, bf16t* kpe, const float* part, const float* wn,\n"
    "    const float* cs, const float* sn, const int* L) {\n"
    "  int t = threadIdx.x; int off = L[0] - 1;\n"
    "  __shared__ float sh[%u]; __shared__ float acc[16]; __shared__ float invs;\n"
    "  float v = 0.f;\n"
    "  if (t < %u) {\n"
    "    float s = 0.f;\n"
    "    for (int i = 0; i < %u; i++) s += part[(long)i * %u + %u + t];\n"
    "    v = bf2f(f2bfd(s)); sh[t] = v;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < 16) {\n"
    "    float a = 0.0f;\n"
    "    for (int i = t; i < %u; i += 16) a = a + sh[i] * sh[i];\n"
    "    acc[t] = a;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t == 0) {\n"
    "    float s = 0.0f;\n"
    "    for (int i = 0; i < 16; i++) s = s + acc[i];\n"
    "    invs = 1.0f / sqrtf((s * %.10ef) + %.10ef);\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float inv = invs;\n"
    "  if (t < %u) {\n"
    "    ckv[(long)off * %u + t] = f2bfd((v * inv) * wn[t]);\n"
    "  } else if (t < %u) {\n"
    "    int j = t - %u; int p = off;\n"
    "    float xe = 0.f, xo = 0.f;\n"
    "    for (int i = 0; i < %u; i++) { xe += part[(long)i * %u + %u + 2 * j]; xo += part[(long)i * %u + %u + 2 * j + 1]; }\n"
    "    xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "    float c = cs[(long)p * %u + j], sv = sn[(long)p * %u + j];\n"
    "    kpe[(long)off * %u + j]      = f2bfd(xe * c - xo * sv);\n"
    "    kpe[(long)off * %u + %u + j] = f2bfd(xo * c + xe * sv);\n"
    "  }\n"
    "}\n", kv, kv, sk, ns, qoff, kv, 1.0 / (double)kv, (double)C.eps, kv, kv,
         kv + half, kv, sk, ns, qoff + kv, ns, qoff + kv, dr, dr, dr, dr, half);
  return mks(
    "#include <math.h>\n"
    "void kv_prep(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *ckv = (bf16t *)bufs[0]; bf16t *kpe = (bf16t *)bufs[1]; const float *part = (const float *)bufs[2];\n"
    "  const float *wn = (const float *)bufs[3]; const float *cs = (const float *)bufs[4];\n"
    "  const float *sn = (const float *)bufs[5]; const int *L = (const int *)bufs[6];\n"
    "  int off = L[0] - 1;\n"
    "  float v[%u]; float ss = 0.f;\n"
    "  for (unsigned t = 0; t < %uu; t++) {\n"
    "    float s = 0.f;\n"
    "    for (unsigned i = 0; i < %uu; i++) s += part[(unsigned long)i * %uu + %uu + t];\n"
    "    v[t] = bf2f(f2bfd(s)); ss += v[t] * v[t];\n"
    "  }\n"
    "  float inv = 1.0f / sqrtf(ss * %.10ef + %.10ef);\n"
    "  for (unsigned t = 0; t < %uu; t++) ckv[(long)off * %uu + t] = f2bfd((v[t] * inv) * wn[t]);\n"
    "  for (unsigned j = 0; j < %uu; j++) {\n"
    "    float xe = 0.f, xo = 0.f;\n"
    "    for (unsigned i = 0; i < %uu; i++) { xe += part[(unsigned long)i * %uu + %uu + 2 * j]; xo += part[(unsigned long)i * %uu + %uu + 2 * j + 1]; }\n"
    "    xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "    float c = cs[(long)off * %uu + j], sv = sn[(long)off * %uu + j];\n"
    "    kpe[(long)off * %uu + j]      = f2bfd(xe * c - xo * sv);\n"
    "    kpe[(long)off * %uu + %uu + j] = f2bfd(xo * c + xe * sv);\n"
    "  }\n"
    "}\n", kv, kv, sk, ns, qoff, 1.0 / (double)kv, (double)C.eps, kv, kv, half,
         sk, ns, qoff + kv, ns, qoff + kv, dr, dr, dr, dr, half);
}

// o fold + residual + post-attn rms: x1 = x + fold(o partials) (bf16-rounded
// like the native mm out + add chain), then xn2 = rms(x1) emitted BOTH bf16
// (MoE / dense input) and f32 (router gate input, the native mm read f32).
static char *src_gred_o_rms(u32 sk) {
  u32 d = C.dim;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void gred_o_rms(bf16t* x1, bf16t* xn2b, float* xn2f,\n"
    "    const float* part, const bf16t* x, const float* w) {\n"
    "  int t = threadIdx.x; int nt = blockDim.x;\n"
    "  __shared__ float sh[%u]; __shared__ float acc[16]; __shared__ float invs;\n"
    "  for (int i = t; i < %u; i += nt) {\n"
    "    float s = 0.f;\n"
    "    for (int k = 0; k < %u; k++) s += part[(long)k * %u + i];\n"
    "    float x1v = bf2f(f2bfd(bf2f(x[i]) + bf2f(f2bfd(s))));\n"
    "    x1[i] = f2bfd(x1v); sh[i] = x1v;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < 16) {\n"
    "    float a = 0.0f;\n"
    "    for (int i = t; i < %u; i += 16) a = a + sh[i] * sh[i];\n"
    "    acc[t] = a;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t == 0) {\n"
    "    float s = 0.0f;\n"
    "    for (int i = 0; i < 16; i++) s = s + acc[i];\n"
    "    invs = 1.0f / sqrtf((s * %.10ef) + %.10ef);\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float inv = invs;\n"
    "  for (int i = t; i < %u; i += nt) {\n"
    "    float v = (sh[i] * inv) * w[i];\n"
    "    xn2f[i] = v; xn2b[i] = f2bfd(v);\n"
    "  }\n"
    "}\n", d, d, sk, d, d, 1.0 / (double)d, (double)C.eps, d);
  return mks(
    "#include <math.h>\n"
    "void gred_o_rms(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *x1 = (bf16t *)bufs[0]; bf16t *xn2b = (bf16t *)bufs[1]; float *xn2f = (float *)bufs[2];\n"
    "  const float *part = (const float *)bufs[3]; const bf16t *x = (const bf16t *)bufs[4]; const float *w = (const float *)bufs[5];\n"
    "  float ss = 0.f;\n"
    "  for (unsigned i = 0; i < %uu; i++) {\n"
    "    float s = 0.f;\n"
    "    for (unsigned k = 0; k < %uu; k++) s += part[(unsigned long)k * %uu + i];\n"
    "    float x1v = bf2f(f2bfd(bf2f(x[i]) + bf2f(f2bfd(s))));\n"
    "    x1[i] = f2bfd(x1v); ss += x1v * x1v;\n"
    "  }\n"
    "  float inv = 1.0f / sqrtf(ss * %.10ef + %.10ef);\n"
    "  for (unsigned i = 0; i < %uu; i++) {\n"
    "    float v = (bf2f(x1[i]) * inv) * w[i];\n"
    "    xn2f[i] = v; xn2b[i] = f2bfd(v);\n"
    "  }\n"
    "}\n", d, sk, d, 1.0 / (double)d, (double)C.eps, d);
}

// down fold + residual (dense MLP tail): x = x1 + fold(down partials), both
// bf16-rounded exactly like the native mm out + add chain.
static char *src_gred_down_res(u32 sk) {
  u32 d = C.dim;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void gred_down(bf16t* xo, const float* part, const bf16t* x1) {\n"
    "  int n = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (n >= %u) return;\n"
    "  float s = 0.f;\n"
    "  for (int i = 0; i < %u; i++) s += part[(long)i * %u + n];\n"
    "  xo[n] = f2bfd(bf2f(x1[n]) + bf2f(f2bfd(s)));\n"
    "}\n", d, sk, d);
  return mks(
    "void gred_down(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *xo = (bf16t *)bufs[0]; const float *part = (const float *)bufs[1]; const bf16t *x1 = (const bf16t *)bufs[2];\n"
    "  for (unsigned n = 0; n < %uu; n++) {\n"
    "    float s = 0.f;\n"
    "    for (unsigned i = 0; i < %uu; i++) s += part[(unsigned long)i * %uu + n];\n"
    "    xo[n] = f2bfd(bf2f(x1[n]) + bf2f(f2bfd(s)));\n"
    "  }\n"
    "}\n", d, sk, d);
}

// MLA3 chain (port of deepseek_v2.py:651-717).  q is the packed [NH, DN+DR]
// buffer gred_q writes: the nope slice sits at row stride DN+DR, the roped pe
// slice at offset DN, so no shrink kernels are needed.
static char *src_qlat(void) {
  u32 h = C.n_heads, kv = C.kv_lora, dn = C.d_nope, qs = C.d_nope + C.d_rope;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_qlat(float* qlat, const bf16t* qn, const bf16t* wuk) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (idx >= %u) return;\n"
    "  int h = idx / %u, k = idx %% %u;\n"
    "  const bf16t* q = qn + (long)h * %u;\n"
    "  const bf16t* w = wuk + (long)h * %u;\n"
    "  float acc = 0.f;\n"
    "#pragma unroll 8\n"
"  for (int d = 0; d < %u; d++) acc += bf2f(q[d]) * bf2f(w[(long)d * %u + k]);\n"
    "  qlat[(long)h * %u + k] = acc;\n"
    "}\n", h * kv, kv, kv, qs, dn * kv, dn, kv, kv);
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
    "}\n", h, kv, dn, qs, dn, kv, kv);
}

static char *src_score(void) {
  u32 kv = C.kv_lora, dr = C.d_rope, ml = C.max_seq, qs = C.d_nope + C.d_rope;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_score(float* sm, const float* qlat, const bf16t* qp,\n"
    "    const bf16t* Cn, const bf16t* Pn, const int* Lp) {\n"
    "  int h = blockIdx.x; int t = threadIdx.x; int nt = blockDim.x; int L = Lp[0];\n"
    "  const bf16t* q_pe = qp + (long)h * %u + %u;\n"
    "  const float* ql = qlat + (long)h * %u;\n"
    "  __shared__ float sc[%u]; __shared__ float red[32];\n"
    "  int lane = t & 31, w = t >> 5, nw = nt >> 5;\n"
    "  for (int j = w; j < L; j += nw) {\n"
    "    const bf16t* cj = Cn + (long)j * %u;\n"
    "    const bf16t* pj = Pn + (long)j * %u;\n"
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
    "}\n", qs, C.d_nope, kv, ml, kv, dr, kv, dr, C.scale, ml);
  return mks(
    "#include <math.h>\n"
    "void mla_score(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *sm = (float *)bufs[0]; const float *qlat = (const float *)bufs[1]; const bf16t *qp = (const bf16t *)bufs[2];\n"
    "  const bf16t *Cn = (const bf16t *)bufs[3]; const bf16t *Pn = (const bf16t *)bufs[4];\n"
    "  const int *Lp = (const int *)bufs[5];\n"
    "  int L = Lp[0];\n"
    "  for (int h = 0; h < %u; h++) {\n"
    "    float sc[%u]; float gmax = -1e30f;\n"
    "    for (int j = 0; j < L; j++) {\n"
    "      const bf16t *cj = Cn + (long)j * %u;\n"
    "      const bf16t *pj = Pn + (long)j * %u;\n"
    "      float s = 0.f;\n"
    "      for (int k = 0; k < %u; k++) s += qlat[(long)h * %u + k] * bf2f(cj[k]);\n"
    "      for (int k = 0; k < %u; k++) s += bf2f(qp[(long)h * %u + %u + k]) * bf2f(pj[k]);\n"
    "      sc[j] = s * %.10ff;\n"
    "      if (sc[j] > gmax) gmax = sc[j];\n"
    "    }\n"
    "    float gsum = 0.f;\n"
    "    for (int j = 0; j < L; j++) { sc[j] = expf(sc[j] - gmax); gsum += sc[j]; }\n"
    "    for (int j = 0; j < L; j++) sm[(long)h * %u + j] = sc[j] / gsum;\n"
    "  }\n"
    "}\n", C.n_heads, ml, kv, dr, kv, kv, dr, qs, C.d_nope, C.scale, ml);
}

static char *src_mix(void) {
  u32 h = C.n_heads, kv = C.kv_lora, ml = C.max_seq;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void mla_mix(float* olat, const float* sm, const bf16t* Cn, const int* Lp) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (idx >= %u) return;\n"
    "  int h = idx / %u, k = idx %% %u; int L = Lp[0];\n"
    "  const float* smh = sm + (long)h * %u;\n"
    "  float a = 0.f;\n"
    "#pragma unroll 4\n"
"  for (int j = 0; j < L; j++) a += smh[j] * bf2f(Cn[(long)j * %u + k]);\n"
    "  olat[(long)h * %u + k] = a;\n"
    "}\n", h * kv, kv, kv, ml, kv, kv);
  return mks(
    "void mla_mix(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *olat = (float *)bufs[0]; const float *sm = (const float *)bufs[1];\n"
    "  const bf16t *Cn = (const bf16t *)bufs[2];\n"
    "  const int *Lp = (const int *)bufs[3];\n"
    "  int L = Lp[0];\n"
    "  for (int h = 0; h < %u; h++)\n"
    "    for (int k = 0; k < %u; k++) {\n"
    "      float a = 0.f;\n"
    "      for (int j = 0; j < L; j++) a += sm[(long)h * %u + j] * bf2f(Cn[(long)j * %u + k]);\n"
    "      olat[(long)h * %u + k] = a;\n"
    "    }\n"
    "}\n", h, kv, ml, kv, kv);
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
    "#pragma unroll 8\n"
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

// route: folds the gate GEMV partials into the E logits (same serial fold
// order as gred), then greedy top-K + full-softmax probs, no renorm
// (replaces deepseek_v2.py:1141-1143 on-device topk); ties -> lowest id
static char *src_route(u32 sk) {
  u32 e = C.n_exp, k = C.top_k;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void route(int* sel, float* probs, const float* part) {\n"
    "  int t = threadIdx.x;\n"
    "  __shared__ float g[%u]; __shared__ int chosen[%u]; __shared__ float smx, ssum;\n"
    "  float s = 0.f;\n"
    "  if (t < %u) {\n"
    "    for (int i = 0; i < %u; i++) s += part[(long)i * %u + t];\n"
    "    g[t] = s;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  // parallel max (fmaxf is exact: any order matches the serial scan)\n"
    "  if (t < 32) {\n"
    "    float m = -3.4e38f;\n"
    "    for (int e = t; e < %u; e += 32) m = fmaxf(m, g[e]);\n"
    "    for (int o = 16; o > 0; o >>= 1) m = fmaxf(m, __shfl_down_sync(0xffffffffu, m, o));\n"
    "    if (t == 0) smx = m;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float mx = smx;\n"
    "  // top-K rounds: warp argmax with lowest-index tie-break == serial scan\n"
    "  for (int kk = 0; kk < %u; kk++) {\n"
    "    if (t < 32) {\n"
    "      float bv = -3.4e38f; int bi = 2147483647;\n"
    "      for (int e = t; e < %u; e += 32) {\n"
    "        int used = 0;\n"
    "        for (int j = 0; j < kk; j++) if (chosen[j] == e) used = 1;\n"
    "        if (!used && (g[e] > bv || (g[e] == bv && e < bi))) { bv = g[e]; bi = e; }\n"
    "      }\n"
    "      for (int o = 16; o > 0; o >>= 1) {\n"
    "        float ov = __shfl_down_sync(0xffffffffu, bv, o);\n"
    "        int oi = __shfl_down_sync(0xffffffffu, bi, o);\n"
    "        if (ov > bv || (ov == bv && oi < bi)) { bv = ov; bi = oi; }\n"
    "      }\n"
    "      if (t == 0) { chosen[kk] = bi; sel[kk] = bi; }\n"
    "    }\n"
    "    __syncthreads();\n"
    "  }\n"
    "  if (t == 0) {\n"
    "    float sum = 0.f;\n"
    "    for (int e = 0; e < %u; e++) sum += __expf(g[e] - mx);\n"
    "    ssum = sum;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < %u) probs[t] = __expf(g[chosen[t]] - mx) / ssum;\n"
    "}\n", e, k, e, sk, e, e, k, e, e, k);
  return mks(
    "#include <math.h>\n"
    "void route(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *sel = (int *)bufs[0]; float *probs = (float *)bufs[1]; const float *part = (const float *)bufs[2];\n"
    "  float g[%u];\n"
    "  for (unsigned t = 0; t < %uu; t++) {\n"
    "    float s = 0.f;\n"
    "    for (unsigned i = 0; i < %uu; i++) s += part[(unsigned long)i * %uu + t];\n"
    "    g[t] = s;\n"
    "  }\n"
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
    "}\n", e, e, sk, e, e, e, k, k, e);
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
    "#pragma unroll 8\n"
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

// moe_full_out + residual epilogue: out = x1 + moe (the moe accumulator is
// bf16-rounded first, then added in f32 and rounded, exactly the native
// mm-out + add chain this replaces).
static char *src_moe_out(void) {
  u32 d = C.dim, mi = C.moe_inter, k = C.top_k, si = C.moe_inter * C.n_shared;
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void moe_full_out(bf16t* out, const float* h, const int* sel,\n"
    "    const float* probs, const bf16t* wd, const bf16t* swd, const bf16t* x1) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= %u) return;\n"
    "  int d = warp; float acc = 0.f;\n"
    "  for (int e = 0; e < %u; e++) {\n"
    "    int se = sel[e];\n"
    "    const bf16t* wde = wd + ((long)se * %u + d) * %u;\n"
    "    const float* he = h + e * %u;\n"
    "    float s = 0.f;\n"
    "#pragma unroll 8\n"
    "    for (int i = lane; i < %u; i += 32) s += he[i] * bf2f(wde[i]);\n"
    "    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);\n"
    "    if (lane == 0) acc += probs[e] * s;\n"
    "  }\n"
    "  {\n"
    "    const bf16t* wde = swd + (long)d * %u;\n"
    "    const float* hs = h + %u;\n"
    "    float s = 0.f;\n"
    "#pragma unroll 8\n"
    "    for (int i = lane; i < %u; i += 32) s += hs[i] * bf2f(wde[i]);\n"
    "    for (int o = 16; o > 0; o >>= 1) s += __shfl_down_sync(0xffffffffu, s, o);\n"
    "    if (lane == 0) acc += s;\n"
    "  }\n"
    "  if (lane == 0) out[d] = f2bfd(bf2f(x1[d]) + bf2f(f2bfd(acc)));\n"
    "}\n", d, k, d, mi, mi, mi, si, k * mi, si);
  return mks(
    "void moe_full_out(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *out = (bf16t *)bufs[0]; const float *h = (const float *)bufs[1]; const int *sel = (const int *)bufs[2];\n"
    "  const float *probs = (const float *)bufs[3]; const bf16t *wd = (const bf16t *)bufs[4]; const bf16t *swd = (const bf16t *)bufs[5];\n"
    "  const bf16t *x1 = (const bf16t *)bufs[6];\n"
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
    "    out[d] = f2bfd(bf2f(x1[d]) + bf2f(f2bfd(acc)));\n"
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

// === split-K GEMV (replaces the native TMatMul lowering for M=1) ============
// The flat one-thread-per-output uop kernel launches only ceil(N/256) blocks
// (2-12 for the 576-3072-wide projections), measured 17-90 GB/s on H100 --
// 2-5% of HBM.  Phase 1 splits K over blockIdx.y (part[s*N+n] = the s-th
// K-chunk's dot), phase 2 folds the SK partials per column and applies the
// output rounding the native mm had (bf16 round once, after a full f32
// accumulate).  Column indexing keeps the coalesced W[k*N+n] pattern.
static u32 gemv_pick_sk(u32 K, u32 N) {
  u32 gx = cdiv(N, 256);
  u32 sk = 1;
  while (gx * sk < 232u && sk * 64u < K) sk *= 2;
  return sk;
}

static u32 gemv_block(u32 N) {
  return N < 256 ? ((N + 31) / 32 * 32) : 256;
}

static char *src_gemv(char const *nm, u32 K, u32 N, u32 sk, int x_f32, int w_f32) {
  u32 chunk = cdiv(K, sk);
  char xl[32], wl[64];
  snprintf(xl, sizeof xl, x_f32 ? "x[k]" : "bf2f(x[k])");
  snprintf(wl, sizeof wl, w_f32 ? "w[(long)k * %uu + n]" : "bf2f(w[(long)k * %uu + n])", N);
  char const *xt = x_f32 ? "float" : "bf16t";
  char const *wt = w_f32 ? "float" : "bf16t";
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void %s(float* part, const %s* x, const %s* w) {\n"
    "  int n = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (n >= %u) return;\n"
    "  int k0 = blockIdx.y * %u, k1 = k0 + %u;\n"
    "  if (k1 > %u) k1 = %u;\n"
    "  float acc = 0.f;\n"
    "#pragma unroll 8\n"
    "  for (int k = k0; k < k1; k++) acc += %s * %s;\n"
    "  part[(long)blockIdx.y * %u + n] = acc;\n"
    "}\n", nm, xt, wt, N, chunk, chunk, K, K, xl, wl, N);
  return mks(
    "void %s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *part = (float *)bufs[0]; const %s *x = (const %s *)bufs[1]; const %s *w = (const %s *)bufs[2];\n"
    "  for (unsigned s = 0; s < %uu; s++) {\n"
    "    unsigned k0 = s * %uu, k1 = k0 + %uu > %uu ? %uu : k0 + %uu;\n"
    "    for (unsigned n = 0; n < %uu; n++) {\n"
    "      float acc = 0.f;\n"
    "      for (unsigned k = k0; k < k1; k++) acc += %s * %s;\n"
    "      part[(unsigned long)s * %uu + n] = acc;\n"
    "    }\n"
    "  }\n"
    "}\n", nm, xt, xt, wt, wt, sk, chunk, chunk, K, K, chunk, N, xl, wl, N);
}

static char *src_gred(char const *nm, u32 N, u32 sk, int out_bf16) {
  char const *ot  = out_bf16 ? "bf16t" : "float";
  char const *ost = out_bf16 ? "f2bfd(s)" : "s";
  if (IS_CUDA) return mks(
    "extern \"C\" __global__ void %s(%s* y, const float* part) {\n"
    "  int n = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (n >= %u) return;\n"
    "  float s = 0.f;\n"
    "  for (int i = 0; i < %u; i++) s += part[(long)i * %uu + n];\n"
    "  y[n] = %s;\n"
    "}\n", nm, ot, N, sk, N, ost);
  return mks(
    "void %s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  %s *y = (%s *)bufs[0]; const float *part = (const float *)bufs[1];\n"
    "  for (unsigned n = 0; n < %uu; n++) {\n"
    "    float s = 0.f;\n"
    "    for (unsigned i = 0; i < %uu; i++) s += part[(unsigned long)i * %uu + n];\n"
    "    y[n] = %s;\n"
    "  }\n"
    "}\n", nm, ot, ot, N, sk, N, ost);
}

// === speculative-verify kernel sources (DSV2_SPEC=S) ========================
// Port of the tinygrad blind speculative step (examples/deepseek_v2_speculative.py
// --blind; _BLIND_SRC_T / _verify_src / _moe_verify_src in examples/deepseek_v2.py).
// Device state: T_ids[max_seq] holds the token stream and T_len keeps the base
// step's convention L = P + 1, where P = index of the PENDING token (cache rows
// [0..P-1] committed) -- the spec kernels read P = L[0]-1 and accept_commit
// advances L by k+1.  NUMERICS: every S-row kernel keeps each row's accumulation
// chain IDENTICAL to its base-step kernel (weights are hoisted and reused across
// s, but each acc[s] folds in the base kernel's exact serial / lane-strided
// order), so verify row 0 at the base state is bit-equal to the base step; the
// startup gate memcmps the two logit rows.

// draft_prep: cand[0] = the pending token ids[P]; cand[1..S-1] continue the most
// recent bigram (ids[P-1], ids[P]) match inside ids[0..P-1].  cand_acc keeps -1
// for draft holes (acceptance must not false-match); cand_emb clamps holes to a
// valid token for the embedding.  forced (DSV2_FORCE_IDS): drafts are read
// straight from the pre-seeded ids[P+1..] -- the acceptance-must-be-full gate.
static char *src_draft_prep(u32 S, int forced) {
  if (IS_CUDA) return mks(
    "#define S %u\n"
    "extern \"C\" __global__ void draft_prep(int* ca, int* ce, const int* ids, const int* L) {\n"
    "  __shared__ int best;\n"
    "  int P = L[0] - 1;\n"
    "  if (threadIdx.x == 0) { best = -1; ca[0] = ids[P]; ce[0] = ids[P]; }\n"
    "  __syncthreads();\n"
    "%s"
    "}\n", S, forced ?
    "  if (threadIdx.x < S - 1) {\n"
    "    int t = threadIdx.x; int v = ids[P + 1 + t];\n"
    "    ca[1 + t] = v; ce[1 + t] = (v >= 0) ? v : ids[P];\n"
    "  }\n" :
    "  int a = ids[P - 1], b = ids[P];\n"
    "  for (int j = threadIdx.x; j + 1 < P; j += blockDim.x)\n"
    "    if (ids[j] == a && ids[j + 1] == b) atomicMax(&best, j);\n"
    "  __syncthreads();\n"
    "  if (threadIdx.x < S - 1) {\n"
    "    int t = threadIdx.x;\n"
    "    int v = (best >= 0 && best + 2 + t <= P) ? ids[best + 2 + t] : -1;\n"
    "    ca[1 + t] = v; ce[1 + t] = (v >= 0) ? v : ids[P];\n"
    "  }\n");
  return mks(
    "#define S %u\n"
    "void draft_prep(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *ca = (int *)bufs[0]; int *ce = (int *)bufs[1];\n"
    "  const int *ids = (const int *)bufs[2]; const int *L = (const int *)bufs[3];\n"
    "  int P = L[0] - 1;\n"
    "  ca[0] = ids[P]; ce[0] = ids[P];\n"
    "%s"
    "}\n", S, forced ?
    "  for (int t = 0; t < S - 1; t++) {\n"
    "    int v = ids[P + 1 + t];\n"
    "    ca[1 + t] = v; ce[1 + t] = (v >= 0) ? v : ids[P];\n"
    "  }\n" :
    "  int a = ids[P - 1], b = ids[P]; int best = -1;\n"
    "  for (int j = 0; j + 1 < P; j++) if (ids[j] == a && ids[j + 1] == b) best = j;\n"
    "  for (int t = 0; t < S - 1; t++) {\n"
    "    int v = (best >= 0 && best + 2 + t <= P) ? ids[best + 2 + t] : -1;\n"
    "    ca[1 + t] = v; ce[1 + t] = (v >= 0) ? v : ids[P];\n"
    "  }\n");
}

// accept_commit: k = longest verified draft prefix (cand[i+1] == am[i]); commit
// cand[1..k] plus the bonus am[k]; the new pending sits at ids[P+k+1] and the
// device length advances by k+1.
static char *src_accept_commit(u32 S) {
  if (IS_CUDA) return mks(
    "#define S %u\n"
    "extern \"C\" __global__ void accept_commit(int* ids, int* L, const int* ca, const int* am) {\n"
    "  if (threadIdx.x != 0 || blockIdx.x != 0) return;\n"
    "  int P = L[0] - 1; int k = 0;\n"
    "  while (k < S - 1 && ca[k + 1] >= 0 && ca[k + 1] == am[k]) k++;\n"
    "  for (int i = 0; i < k; i++) ids[P + 1 + i] = ca[1 + i];\n"
    "  ids[P + 1 + k] = am[k];\n"
    "  L[0] = P + k + 2;\n"
    "}\n", S);
  return mks(
    "#define S %u\n"
    "void accept_commit(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *ids = (int *)bufs[0]; int *L = (int *)bufs[1];\n"
    "  const int *ca = (const int *)bufs[2]; const int *am = (const int *)bufs[3];\n"
    "  int P = L[0] - 1; int k = 0;\n"
    "  while (k < S - 1 && ca[k + 1] >= 0 && ca[k + 1] == am[k]) k++;\n"
    "  for (int i = 0; i < k; i++) ids[P + 1 + i] = ca[1 + i];\n"
    "  ids[P + 1 + k] = am[k];\n"
    "  L[0] = P + k + 2;\n"
    "}\n", S);
}

// embed_row_s: xS[s*D + i] = emb[cand_emb[s]*D + i]
static char *src_embed_s(u32 S) {
  u32 d = C.dim;
  if (IS_CUDA) return mks(
    "#define S %u\n#define D %u\n"
    "extern \"C\" __global__ void embed_row_s(bf16t* x, const bf16t* emb, const int* ce) {\n"
    "  int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (i < S * D) { int s = i / D, d = i %% D; x[i] = emb[(long)ce[s] * D + d]; }\n"
    "}\n", S, d);
  return mks(
    "#define S %u\n#define D %u\n"
    "void embed_row_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *x = (bf16t *)bufs[0]; const bf16t *emb = (const bf16t *)bufs[1]; const int *ce = (const int *)bufs[2];\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int d = 0; d < D; d++) x[(long)s * D + d] = emb[(long)ce[s] * D + d];\n"
    "}\n", S, d);
}

// rms over S rows: one block per row (blockIdx.y = s), the base rms body verbatim
// per row (16 strided lane accumulators + in-order fold on CUDA).
static char *src_rms_s(char const *nm, u32 K, int out_bf16, u32 S) {
  char const *ot  = out_bf16 ? "bf16t" : "float";
  char const *ost = out_bf16 ? "f2bfd(v)" : "v";
  if (IS_CUDA) return mks(
    "#define K %u\n#define S %u\n"
    "extern \"C\" __global__ void %s(%s* y, const bf16t* x, const float* w) {\n"
    "  int t = threadIdx.x; int nt = blockDim.x; int row = blockIdx.y;\n"
    "  const bf16t* xs = x + (long)row * K; %s* ys = y + (long)row * K;\n"
    "  __shared__ float acc[16]; __shared__ float invs;\n"
    "  if (t < 16) {\n"
    "    float a = 0.0f;\n"
    "    for (int i = t; i < K; i += 16) { float xv = bf2f(xs[i]); a = a + xv * xv; }\n"
    "    acc[t] = a;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t == 0) {\n"
    "    float sm = 0.0f;\n"
    "    for (int i = 0; i < 16; i++) sm = sm + acc[i];\n"
    "    invs = 1.0f / sqrtf((sm * %.10ef) + %.10ef);\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float inv = invs;\n"
    "  for (int i = t; i < K; i += nt) { float v = (bf2f(xs[i]) * inv) * w[i]; ys[i] = %s; }\n"
    "}\n", K, S, nm, ot, ot, 1.0 / (double)K, (double)C.eps, ost);
  return mks(
    "#include <math.h>\n#define K %u\n#define S %u\n"
    "void %s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  %s *y = (%s *)bufs[0]; const bf16t *x = (const bf16t *)bufs[1]; const float *w = (const float *)bufs[2];\n"
    "  for (int row = 0; row < S; row++) {\n"
    "    const bf16t *xs = x + (long)row * K; %s *ys = y + (long)row * K;\n"
    "    float ss = 0.f;\n"
    "    for (int i = 0; i < K; i++) { float xv = bf2f(xs[i]); ss += xv * xv; }\n"
    "    float inv = 1.0f / sqrtf(ss * %.10ef + %.10ef);\n"
    "    for (int i = 0; i < K; i++) { float v = (bf2f(xs[i]) * inv) * w[i]; ys[i] = %s; }\n"
    "  }\n"
    "}\n", K, S, nm, ot, ot, ot, 1.0 / (double)K, (double)C.eps, ost);
}

// split-K GEMV over S rows: phase 1 keeps the base grid {ceil(N/bx), sk}; each
// thread reads the weight column chunk ONCE and folds it into S independent
// accumulators, each in the base kernel's exact serial k order (fma per element,
// same as the base acc += x*w chain) -- partials land at part[(s*SK + chunk)*N+n].
static char *src_gemv_s(char const *nm, u32 K, u32 N, u32 sk, int x_f32, int w_f32, u32 S) {
  u32 chunk = cdiv(K, sk);
  char const *xl = x_f32 ? "x[(long)s * K + k]" : "bf2f(x[(long)s * K + k])";
  char const *wl = w_f32 ? "w[(long)k * N + n]" : "bf2f(w[(long)k * N + n])";
  char const *xt = x_f32 ? "float" : "bf16t";
  char const *wt = w_f32 ? "float" : "bf16t";
  if (IS_CUDA) return mks(
    "#define K %u\n#define N %u\n#define SK %u\n#define CH %u\n#define S %u\n"
    "extern \"C\" __global__ void %s(float* part, const %s* x, const %s* w) {\n"
    "  int n = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (n >= N) return;\n"
    "  int k0 = blockIdx.y * CH, k1 = k0 + CH;\n"
    "  if (k1 > K) k1 = K;\n"
    "  float acc[S];\n"
    "#pragma unroll\n"
    "  for (int s = 0; s < S; s++) acc[s] = 0.f;\n"
    "#pragma unroll 4\n"
    "  for (int k = k0; k < k1; k++) {\n"
    "    float wv = %s;\n"
    "#pragma unroll\n"
    "    for (int s = 0; s < S; s++) acc[s] += %s * wv;\n"
    "  }\n"
    "#pragma unroll\n"
    "  for (int s = 0; s < S; s++) part[((long)s * SK + blockIdx.y) * N + n] = acc[s];\n"
    "}\n", K, N, sk, chunk, S, nm, xt, wt, wl, xl);
  return mks(
    "#define K %u\n#define N %u\n#define SK %u\n#define CH %u\n#define S %u\n"
    "void %s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *part = (float *)bufs[0]; const %s *x = (const %s *)bufs[1]; const %s *w = (const %s *)bufs[2];\n"
    "  for (int c = 0; c < SK; c++) {\n"
    "    int k0 = c * CH, k1 = k0 + CH > K ? K : k0 + CH;\n"
    "    for (int n = 0; n < N; n++)\n"
    "      for (int s = 0; s < S; s++) {\n"
    "        float acc = 0.f;\n"
    "        for (int k = k0; k < k1; k++) acc += %s * %s;\n"
    "        part[((long)s * SK + c) * N + n] = acc;\n"
    "      }\n"
    "  }\n"
    "}\n", K, N, sk, chunk, S, nm, xt, xt, wt, wt, xl, wl);
}

// per-row split-K fold (blockIdx.y = s), the base gred body per row.
static char *src_gred_s(char const *nm, u32 N, u32 sk, int out_bf16, u32 S) {
  char const *ot  = out_bf16 ? "bf16t" : "float";
  char const *ost = out_bf16 ? "f2bfd(v)" : "v";
  if (IS_CUDA) return mks(
    "#define N %u\n#define SK %u\n#define S %u\n"
    "extern \"C\" __global__ void %s(%s* y, const float* part) {\n"
    "  int n = blockIdx.x * blockDim.x + threadIdx.x; int s = blockIdx.y;\n"
    "  if (n >= N) return;\n"
    "  float v = 0.f;\n"
    "  for (int i = 0; i < SK; i++) v += part[((long)s * SK + i) * N + n];\n"
    "  y[(long)s * N + n] = %s;\n"
    "}\n", N, sk, S, nm, ot, ost);
  return mks(
    "#define N %u\n#define SK %u\n#define S %u\n"
    "void %s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  %s *y = (%s *)bufs[0]; const float *part = (const float *)bufs[1];\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int n = 0; n < N; n++) {\n"
    "      float v = 0.f;\n"
    "      for (int i = 0; i < SK; i++) v += part[((long)s * SK + i) * N + n];\n"
    "      y[(long)s * N + n] = %s;\n"
    "    }\n"
    "}\n", N, sk, S, nm, ot, ot, ost);
}

// q fold + rope over S rows (blockIdx.y = s): the base gred_q body on the s-th
// partial slab, roped at position L[0]-1+s.
static char *src_gred_q_rope_s(u32 sk, u32 ns, u32 S) {
  u32 hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, half = dr / 2;
  if (IS_CUDA) return mks(
    "#define HN %u\n#define DN %u\n#define DR %u\n#define HALF %u\n#define QS %u\n"
    "#define SK %u\n#define NS %u\n#define S %u\n"
    "extern \"C\" __global__ void gred_q_s(bf16t* q, const float* part, const float* cs, const float* sn, const int* L) {\n"
    "  int t = blockIdx.x * blockDim.x + threadIdx.x; int s = blockIdx.y;\n"
    "  const float* ps = part + (long)s * SK * NS; bf16t* qs = q + (long)s * HN * QS;\n"
    "  int p = L[0] - 1 + s;\n"
    "  if (t < HN * DN) {\n"
    "    int h = t / DN, d = t %% DN; int col = h * QS + d;\n"
    "    float v = 0.f;\n"
    "    for (int i = 0; i < SK; i++) v += ps[(long)i * NS + col];\n"
    "    qs[col] = f2bfd(v);\n"
    "  } else if (t < HN * DN + HN * HALF) {\n"
    "    int u = t - HN * DN; int h = u / HALF, j = u %% HALF;\n"
    "    int base = h * QS + DN;\n"
    "    float xe = 0.f, xo = 0.f;\n"
    "    for (int i = 0; i < SK; i++) { xe += ps[(long)i * NS + base + 2 * j]; xo += ps[(long)i * NS + base + 2 * j + 1]; }\n"
    "    xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "    float c = cs[(long)p * DR + j], sv = sn[(long)p * DR + j];\n"
    "    qs[base + j]        = f2bfd(xe * c - xo * sv);\n"
    "    qs[base + HALF + j] = f2bfd(xo * c + xe * sv);\n"
    "  }\n"
    "}\n", hn, dn, dr, half, dn + dr, sk, ns, S);
  return mks(
    "#define HN %u\n#define DN %u\n#define DR %u\n#define HALF %u\n#define QS %u\n"
    "#define SK %u\n#define NS %u\n#define S %u\n"
    "void gred_q_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *q = (bf16t *)bufs[0]; const float *part = (const float *)bufs[1];\n"
    "  const float *cs = (const float *)bufs[2]; const float *sn = (const float *)bufs[3]; const int *L = (const int *)bufs[4];\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    const float *ps = part + (long)s * SK * NS; bf16t *qs = q + (long)s * HN * QS;\n"
    "    int p = L[0] - 1 + s;\n"
    "    for (int h = 0; h < HN; h++) {\n"
    "      for (int d = 0; d < DN; d++) {\n"
    "        int col = h * QS + d; float v = 0.f;\n"
    "        for (int i = 0; i < SK; i++) v += ps[(long)i * NS + col];\n"
    "        qs[col] = f2bfd(v);\n"
    "      }\n"
    "      for (int j = 0; j < HALF; j++) {\n"
    "        int base = h * QS + DN;\n"
    "        float xe = 0.f, xo = 0.f;\n"
    "        for (int i = 0; i < SK; i++) { xe += ps[(long)i * NS + base + 2 * j]; xo += ps[(long)i * NS + base + 2 * j + 1]; }\n"
    "        xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "        float c = cs[(long)p * DR + j], sv = sn[(long)p * DR + j];\n"
    "        qs[base + j]        = f2bfd(xe * c - xo * sv);\n"
    "        qs[base + HALF + j] = f2bfd(xo * c + xe * sv);\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n", hn, dn, dr, half, dn + dr, sk, ns, S);
}

// kv prep over S rows (blockIdx.y = s): the base kv_prep body on the s-th
// partial slab; row s lands in the caches at offset L[0]-1+s.
static char *src_kv_prep_s(u32 sk, u32 ns, u32 qoff, u32 S) {
  u32 kv = C.kv_lora, dr = C.d_rope, half = dr / 2;
  if (IS_CUDA) return mks(
    "#define KV %u\n#define DR %u\n#define HALF %u\n#define SK %u\n#define NS %u\n#define QOFF %u\n#define S %u\n"
    "extern \"C\" __global__ void kv_prep_s(bf16t* ckv, bf16t* kpe, const float* part, const float* wn,\n"
    "    const float* cs, const float* sn, const int* L) {\n"
    "  int t = threadIdx.x; int s = blockIdx.y; int off = L[0] - 1 + s;\n"
    "  const float* ps = part + (long)s * SK * NS;\n"
    "  __shared__ float sh[KV]; __shared__ float acc[16]; __shared__ float invs;\n"
    "  float v = 0.f;\n"
    "  if (t < KV) {\n"
    "    float sm = 0.f;\n"
    "    for (int i = 0; i < SK; i++) sm += ps[(long)i * NS + QOFF + t];\n"
    "    v = bf2f(f2bfd(sm)); sh[t] = v;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < 16) {\n"
    "    float a = 0.0f;\n"
    "    for (int i = t; i < KV; i += 16) a = a + sh[i] * sh[i];\n"
    "    acc[t] = a;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t == 0) {\n"
    "    float sm = 0.0f;\n"
    "    for (int i = 0; i < 16; i++) sm = sm + acc[i];\n"
    "    invs = 1.0f / sqrtf((sm * %.10ef) + %.10ef);\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float inv = invs;\n"
    "  if (t < KV) {\n"
    "    ckv[(long)off * KV + t] = f2bfd((v * inv) * wn[t]);\n"
    "  } else if (t < KV + HALF) {\n"
    "    int j = t - KV;\n"
    "    float xe = 0.f, xo = 0.f;\n"
    "    for (int i = 0; i < SK; i++) { xe += ps[(long)i * NS + QOFF + KV + 2 * j]; xo += ps[(long)i * NS + QOFF + KV + 2 * j + 1]; }\n"
    "    xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "    float c = cs[(long)off * DR + j], sv = sn[(long)off * DR + j];\n"
    "    kpe[(long)off * DR + j]        = f2bfd(xe * c - xo * sv);\n"
    "    kpe[(long)off * DR + HALF + j] = f2bfd(xo * c + xe * sv);\n"
    "  }\n"
    "}\n", kv, dr, half, sk, ns, qoff, S, 1.0 / (double)kv, (double)C.eps);
  return mks(
    "#include <math.h>\n"
    "#define KV %u\n#define DR %u\n#define HALF %u\n#define SK %u\n#define NS %u\n#define QOFF %u\n#define S %u\n"
    "void kv_prep_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *ckv = (bf16t *)bufs[0]; bf16t *kpe = (bf16t *)bufs[1]; const float *part = (const float *)bufs[2];\n"
    "  const float *wn = (const float *)bufs[3]; const float *cs = (const float *)bufs[4];\n"
    "  const float *sn = (const float *)bufs[5]; const int *L = (const int *)bufs[6];\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    int off = L[0] - 1 + s;\n"
    "    const float *ps = part + (long)s * SK * NS;\n"
    "    float v[KV]; float ss = 0.f;\n"
    "    for (int t = 0; t < KV; t++) {\n"
    "      float sm = 0.f;\n"
    "      for (int i = 0; i < SK; i++) sm += ps[(long)i * NS + QOFF + t];\n"
    "      v[t] = bf2f(f2bfd(sm)); ss += v[t] * v[t];\n"
    "    }\n"
    "    float inv = 1.0f / sqrtf(ss * %.10ef + %.10ef);\n"
    "    for (int t = 0; t < KV; t++) ckv[(long)off * KV + t] = f2bfd((v[t] * inv) * wn[t]);\n"
    "    for (int j = 0; j < HALF; j++) {\n"
    "      float xe = 0.f, xo = 0.f;\n"
    "      for (int i = 0; i < SK; i++) { xe += ps[(long)i * NS + QOFF + KV + 2 * j]; xo += ps[(long)i * NS + QOFF + KV + 2 * j + 1]; }\n"
    "      xe = bf2f(f2bfd(xe)); xo = bf2f(f2bfd(xo));\n"
    "      float c = cs[(long)off * DR + j], sv = sn[(long)off * DR + j];\n"
    "      kpe[(long)off * DR + j]        = f2bfd(xe * c - xo * sv);\n"
    "      kpe[(long)off * DR + HALF + j] = f2bfd(xo * c + xe * sv);\n"
    "    }\n"
    "  }\n"
    "}\n", kv, dr, half, sk, ns, qoff, S, 1.0 / (double)kv, (double)C.eps);
}

// verify MLA chain (port of _verify_src, deepseek_v2.py:870-923).  Query s sits
// at absolute position P+s (P = L[0]-1) and attends keys [0..P+s]; the candidate
// rows [P..P+S-1] were just written into the caches by kv_prep_s, so the kernels
// read ONE cache (no curc/curp split -- same bytes, launch-ordered).  Weight-
// heavy phases (qlat_v reads w_uk, o_v reads w_uv) hoist the weight over s.
static char *src_qlat_v(u32 S) {
  u32 h = C.n_heads, kv = C.kv_lora, dn = C.d_nope, dr = C.d_rope;
  if (IS_CUDA) return mks(
    "#define H %u\n#define KV %u\n#define DN %u\n#define QS %u\n#define S %u\n"
    "extern \"C\" __global__ void qlat_v(float* ql, const bf16t* qc, const bf16t* wuk) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (idx >= H * KV) return;\n"
    "  int h = idx / KV, k = idx %% KV;\n"
    "  float acc[S];\n"
    "#pragma unroll\n"
    "  for (int s = 0; s < S; s++) acc[s] = 0.f;\n"
    "  for (int d = 0; d < DN; d++) {\n"
    "    float w = bf2f(wuk[((long)h * DN + d) * KV + k]);\n"
    "#pragma unroll\n"
    "    for (int s = 0; s < S; s++) acc[s] += bf2f(qc[((long)s * H + h) * QS + d]) * w;\n"
    "  }\n"
    "#pragma unroll\n"
    "  for (int s = 0; s < S; s++) ql[((long)s * H + h) * KV + k] = acc[s];\n"
    "}\n", h, kv, dn, dn + dr, S);
  return mks(
    "#define H %u\n#define KV %u\n#define DN %u\n#define QS %u\n#define S %u\n"
    "void qlat_v(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *ql = (float *)bufs[0]; const bf16t *qc = (const bf16t *)bufs[1]; const bf16t *wuk = (const bf16t *)bufs[2];\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int h = 0; h < H; h++)\n"
    "      for (int k = 0; k < KV; k++) {\n"
    "        float acc = 0.f;\n"
    "        for (int d = 0; d < DN; d++) acc += bf2f(qc[((long)s * H + h) * QS + d]) * bf2f(wuk[((long)h * DN + d) * KV + k]);\n"
    "        ql[((long)s * H + h) * KV + k] = acc;\n"
    "      }\n"
    "}\n", h, kv, dn, dn + dr, S);
}

static char *src_score_v(u32 S) {
  u32 h = C.n_heads, kv = C.kv_lora, dn = C.d_nope, dr = C.d_rope, ml = C.max_seq;
  if (IS_CUDA) return mks(
    "#define H %u\n#define KV %u\n#define DN %u\n#define DR %u\n#define QS %u\n#define MS %u\n#define SCALE %.10ff\n#define S %u\n"
    "extern \"C\" __global__ void score_v(float* sm, const float* ql, const bf16t* qp,\n"
    "    const bf16t* Cn, const bf16t* Pn, const int* Lp) {\n"
    "  int h = blockIdx.x; int s = blockIdx.y; int t = threadIdx.x; int nt = blockDim.x;\n"
    "  int P = Lp[0] - 1; int L = P + s + 1;\n"
    "  const bf16t* q_pe = qp + ((long)s * H + h) * QS + DN;\n"
    "  const float* qlh = ql + ((long)s * H + h) * KV;\n"
    "  __shared__ float sc[MS]; __shared__ float red[32];\n"
    "  int lane = t & 31, w = t >> 5, nw = nt >> 5;\n"
    "  for (int j = w; j < L; j += nw) {\n"
    "    const bf16t* cj = Cn + (long)j * KV;\n"
    "    const bf16t* pj = Pn + (long)j * DR;\n"
    "    float sv = 0.f;\n"
    "    for (int k = lane; k < KV; k += 32) sv += qlh[k] * bf2f(cj[k]);\n"
    "    for (int k = lane; k < DR; k += 32) sv += bf2f(q_pe[k]) * bf2f(pj[k]);\n"
    "    for (int o = 16; o > 0; o >>= 1) sv += __shfl_down_sync(0xffffffffu, sv, o);\n"
    "    if (lane == 0) sc[j] = sv * SCALE;\n"
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
    "  for (int j = t; j < L; j += nt) sm[((long)s * H + h) * MS + j] = sc[j] / gsum;\n"
    "}\n", h, kv, dn, dr, dn + dr, ml, C.scale, S);
  return mks(
    "#include <math.h>\n"
    "#define H %u\n#define KV %u\n#define DN %u\n#define DR %u\n#define QS %u\n#define MS %u\n#define SCALE %.10ff\n#define S %u\n"
    "void score_v(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *sm = (float *)bufs[0]; const float *ql = (const float *)bufs[1]; const bf16t *qp = (const bf16t *)bufs[2];\n"
    "  const bf16t *Cn = (const bf16t *)bufs[3]; const bf16t *Pn = (const bf16t *)bufs[4];\n"
    "  const int *Lp = (const int *)bufs[5];\n"
    "  int P = Lp[0] - 1;\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    int L = P + s + 1;\n"
    "    for (int h = 0; h < H; h++) {\n"
    "      float sc[MS]; float gmax = -1e30f;\n"
    "      for (int j = 0; j < L; j++) {\n"
    "        const bf16t *cj = Cn + (long)j * KV;\n"
    "        const bf16t *pj = Pn + (long)j * DR;\n"
    "        float sv = 0.f;\n"
    "        for (int k = 0; k < KV; k++) sv += ql[((long)s * H + h) * KV + k] * bf2f(cj[k]);\n"
    "        for (int k = 0; k < DR; k++) sv += bf2f(qp[((long)s * H + h) * QS + DN + k]) * bf2f(pj[k]);\n"
    "        sc[j] = sv * SCALE;\n"
    "        if (sc[j] > gmax) gmax = sc[j];\n"
    "      }\n"
    "      float gsum = 0.f;\n"
    "      for (int j = 0; j < L; j++) { sc[j] = expf(sc[j] - gmax); gsum += sc[j]; }\n"
    "      for (int j = 0; j < L; j++) sm[((long)s * H + h) * MS + j] = sc[j] / gsum;\n"
    "    }\n"
    "  }\n"
    "}\n", h, kv, dn, dr, dn + dr, ml, C.scale, S);
}

static char *src_mix_v(u32 S) {
  u32 h = C.n_heads, kv = C.kv_lora, ml = C.max_seq;
  if (IS_CUDA) return mks(
    "#define H %u\n#define KV %u\n#define MS %u\n#define S %u\n"
    "extern \"C\" __global__ void mix_v(float* ol, const float* sm, const bf16t* Cn, const int* Lp) {\n"
    "  int idx = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "  if (idx >= H * KV) return;\n"
    "  int h = idx / KV, k = idx %% KV; int P = Lp[0] - 1;\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    const float* smh = sm + ((long)s * H + h) * MS; int L = P + s + 1;\n"
    "    float a = 0.f;\n"
    "#pragma unroll 4\n"
    "    for (int j = 0; j < L; j++) a += smh[j] * bf2f(Cn[(long)j * KV + k]);\n"
    "    ol[((long)s * H + h) * KV + k] = a;\n"
    "  }\n"
    "}\n", h, kv, ml, S);
  return mks(
    "#define H %u\n#define KV %u\n#define MS %u\n#define S %u\n"
    "void mix_v(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *ol = (float *)bufs[0]; const float *sm = (const float *)bufs[1];\n"
    "  const bf16t *Cn = (const bf16t *)bufs[2];\n"
    "  const int *Lp = (const int *)bufs[3];\n"
    "  int P = Lp[0] - 1;\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int h = 0; h < H; h++)\n"
    "      for (int k = 0; k < KV; k++) {\n"
    "        int L = P + s + 1; float a = 0.f;\n"
    "        for (int j = 0; j < L; j++) a += sm[((long)s * H + h) * MS + j] * bf2f(Cn[(long)j * KV + k]);\n"
    "        ol[((long)s * H + h) * KV + k] = a;\n"
    "      }\n"
    "}\n", h, kv, ml, S);
}

static char *src_o_v(u32 S) {
  u32 h = C.n_heads, kv = C.kv_lora, dv = C.d_v;
  if (IS_CUDA) return mks(
    "#define H %u\n#define KV %u\n#define DV %u\n#define S %u\n"
    "extern \"C\" __global__ void o_v(bf16t* o, const float* olat, const bf16t* wuv) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= H * DV) return;\n"
    "  int h = warp / DV, mm = warp %% DV;\n"
    "  const bf16t* wm = wuv + ((long)h * DV + mm) * KV;\n"
    "  float acc[S];\n"
    "#pragma unroll\n"
    "  for (int s = 0; s < S; s++) acc[s] = 0.f;\n"
    "  for (int k = lane; k < KV; k += 32) {\n"
    "    float w = bf2f(wm[k]);\n"
    "#pragma unroll\n"
    "    for (int s = 0; s < S; s++) acc[s] += olat[((long)s * H + h) * KV + k] * w;\n"
    "  }\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    float a = acc[s];\n"
    "    for (int oo = 16; oo > 0; oo >>= 1) a += __shfl_down_sync(0xffffffffu, a, oo);\n"
    "    if (lane == 0) o[((long)s * H + h) * DV + mm] = f2bfd(a);\n"
    "  }\n"
    "}\n", h, kv, dv, S);
  return mks(
    "#define H %u\n#define KV %u\n#define DV %u\n#define S %u\n"
    "void o_v(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *o = (bf16t *)bufs[0]; const float *olat = (const float *)bufs[1]; const bf16t *wuv = (const bf16t *)bufs[2];\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int h = 0; h < H; h++)\n"
    "      for (int mm = 0; mm < DV; mm++) {\n"
    "        float acc = 0.f;\n"
    "        for (int k = 0; k < KV; k++) acc += olat[((long)s * H + h) * KV + k] * bf2f(wuv[((long)h * DV + mm) * KV + k]);\n"
    "        o[((long)s * H + h) * DV + mm] = f2bfd(acc);\n"
    "      }\n"
    "}\n", h, kv, dv, S);
}

// o fold + residual + post-attn rms over S rows (blockIdx.y = s).
static char *src_gred_o_rms_s(u32 sk, u32 S) {
  u32 d = C.dim;
  if (IS_CUDA) return mks(
    "#define D %u\n#define SK %u\n#define S %u\n"
    "extern \"C\" __global__ void gred_o_rms_s(bf16t* x1, bf16t* xn2b, float* xn2f,\n"
    "    const float* part, const bf16t* x, const float* w) {\n"
    "  int t = threadIdx.x; int nt = blockDim.x; int s = blockIdx.y;\n"
    "  const float* ps = part + (long)s * SK * D; const bf16t* xs = x + (long)s * D;\n"
    "  bf16t* x1s = x1 + (long)s * D; bf16t* n2bs = xn2b + (long)s * D; float* n2fs = xn2f + (long)s * D;\n"
    "  __shared__ float sh[D]; __shared__ float acc[16]; __shared__ float invs;\n"
    "  for (int i = t; i < D; i += nt) {\n"
    "    float sm = 0.f;\n"
    "    for (int k = 0; k < SK; k++) sm += ps[(long)k * D + i];\n"
    "    float x1v = bf2f(f2bfd(bf2f(xs[i]) + bf2f(f2bfd(sm))));\n"
    "    x1s[i] = f2bfd(x1v); sh[i] = x1v;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < 16) {\n"
    "    float a = 0.0f;\n"
    "    for (int i = t; i < D; i += 16) a = a + sh[i] * sh[i];\n"
    "    acc[t] = a;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t == 0) {\n"
    "    float sm = 0.0f;\n"
    "    for (int i = 0; i < 16; i++) sm = sm + acc[i];\n"
    "    invs = 1.0f / sqrtf((sm * %.10ef) + %.10ef);\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float inv = invs;\n"
    "  for (int i = t; i < D; i += nt) {\n"
    "    float v = (sh[i] * inv) * w[i];\n"
    "    n2fs[i] = v; n2bs[i] = f2bfd(v);\n"
    "  }\n"
    "}\n", d, sk, S, 1.0 / (double)d, (double)C.eps);
  return mks(
    "#include <math.h>\n#define D %u\n#define SK %u\n#define S %u\n"
    "void gred_o_rms_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *x1 = (bf16t *)bufs[0]; bf16t *xn2b = (bf16t *)bufs[1]; float *xn2f = (float *)bufs[2];\n"
    "  const float *part = (const float *)bufs[3]; const bf16t *x = (const bf16t *)bufs[4]; const float *w = (const float *)bufs[5];\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    const float *ps = part + (long)s * SK * D; const bf16t *xs = x + (long)s * D;\n"
    "    bf16t *x1s = x1 + (long)s * D; bf16t *n2bs = xn2b + (long)s * D; float *n2fs = xn2f + (long)s * D;\n"
    "    float ss = 0.f;\n"
    "    for (int i = 0; i < D; i++) {\n"
    "      float sm = 0.f;\n"
    "      for (int k = 0; k < SK; k++) sm += ps[(long)k * D + i];\n"
    "      float x1v = bf2f(f2bfd(bf2f(xs[i]) + bf2f(f2bfd(sm))));\n"
    "      x1s[i] = f2bfd(x1v); ss += x1v * x1v;\n"
    "    }\n"
    "    float inv = 1.0f / sqrtf(ss * %.10ef + %.10ef);\n"
    "    for (int i = 0; i < D; i++) {\n"
    "      float v = (bf2f(x1s[i]) * inv) * w[i];\n"
    "      n2fs[i] = v; n2bs[i] = f2bfd(v);\n"
    "    }\n"
    "  }\n"
    "}\n", d, sk, S, 1.0 / (double)d, (double)C.eps);
}

// down fold + residual over S rows (blockIdx.y = s).
static char *src_gred_down_res_s(u32 sk, u32 S) {
  u32 d = C.dim;
  if (IS_CUDA) return mks(
    "#define D %u\n#define SK %u\n#define S %u\n"
    "extern \"C\" __global__ void gred_down_s(bf16t* xo, const float* part, const bf16t* x1) {\n"
    "  int n = blockIdx.x * blockDim.x + threadIdx.x; int s = blockIdx.y;\n"
    "  if (n >= D) return;\n"
    "  const float* ps = part + (long)s * SK * D;\n"
    "  float sm = 0.f;\n"
    "  for (int i = 0; i < SK; i++) sm += ps[(long)i * D + n];\n"
    "  xo[(long)s * D + n] = f2bfd(bf2f(x1[(long)s * D + n]) + bf2f(f2bfd(sm)));\n"
    "}\n", d, sk, S);
  return mks(
    "#define D %u\n#define SK %u\n#define S %u\n"
    "void gred_down_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *xo = (bf16t *)bufs[0]; const float *part = (const float *)bufs[1]; const bf16t *x1 = (const bf16t *)bufs[2];\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int n = 0; n < D; n++) {\n"
    "      float sm = 0.f;\n"
    "      for (int i = 0; i < SK; i++) sm += part[((long)s * SK + i) * D + n];\n"
    "      xo[(long)s * D + n] = f2bfd(bf2f(x1[(long)s * D + n]) + bf2f(f2bfd(sm)));\n"
    "    }\n"
    "}\n", d, sk, S);
}

// route over S tokens (blockIdx.y = s): the base route body on the s-th partial
// slab, writing sel[s*K..] / probs[s*K..].
static char *src_route_s(u32 sk, u32 S) {
  u32 e = C.n_exp, k = C.top_k;
  if (IS_CUDA) return mks(
    "#define E %u\n#define TK %u\n#define SK %u\n#define S %u\n"
    "extern \"C\" __global__ void route_s(int* sel, float* probs, const float* part) {\n"
    "  int t = threadIdx.x; int s = blockIdx.y;\n"
    "  const float* ps = part + (long)s * SK * E; int* sels = sel + s * TK; float* prs = probs + s * TK;\n"
    "  __shared__ float g[E]; __shared__ int chosen[TK]; __shared__ float smx, ssum;\n"
    "  float sv = 0.f;\n"
    "  if (t < E) {\n"
    "    for (int i = 0; i < SK; i++) sv += ps[(long)i * E + t];\n"
    "    g[t] = sv;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < 32) {\n"
    "    float m = -3.4e38f;\n"
    "    for (int e = t; e < E; e += 32) m = fmaxf(m, g[e]);\n"
    "    for (int o = 16; o > 0; o >>= 1) m = fmaxf(m, __shfl_down_sync(0xffffffffu, m, o));\n"
    "    if (t == 0) smx = m;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  float mx = smx;\n"
    "  for (int kk = 0; kk < TK; kk++) {\n"
    "    if (t < 32) {\n"
    "      float bv = -3.4e38f; int bi = 2147483647;\n"
    "      for (int e = t; e < E; e += 32) {\n"
    "        int used = 0;\n"
    "        for (int j = 0; j < kk; j++) if (chosen[j] == e) used = 1;\n"
    "        if (!used && (g[e] > bv || (g[e] == bv && e < bi))) { bv = g[e]; bi = e; }\n"
    "      }\n"
    "      for (int o = 16; o > 0; o >>= 1) {\n"
    "        float ov = __shfl_down_sync(0xffffffffu, bv, o);\n"
    "        int oi = __shfl_down_sync(0xffffffffu, bi, o);\n"
    "        if (ov > bv || (ov == bv && oi < bi)) { bv = ov; bi = oi; }\n"
    "      }\n"
    "      if (t == 0) { chosen[kk] = bi; sels[kk] = bi; }\n"
    "    }\n"
    "    __syncthreads();\n"
    "  }\n"
    "  if (t == 0) {\n"
    "    float sum = 0.f;\n"
    "    for (int e = 0; e < E; e++) sum += __expf(g[e] - mx);\n"
    "    ssum = sum;\n"
    "  }\n"
    "  __syncthreads();\n"
    "  if (t < TK) prs[t] = __expf(g[chosen[t]] - mx) / ssum;\n"
    "}\n", e, k, sk, S);
  return mks(
    "#include <math.h>\n#define E %u\n#define TK %u\n#define SK %u\n#define S %u\n"
    "void route_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *sel = (int *)bufs[0]; float *probs = (float *)bufs[1]; const float *part = (const float *)bufs[2];\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    const float *ps = part + (long)s * SK * E;\n"
    "    float g[E];\n"
    "    for (int t = 0; t < E; t++) {\n"
    "      float sv = 0.f;\n"
    "      for (int i = 0; i < SK; i++) sv += ps[(long)i * E + t];\n"
    "      g[t] = sv;\n"
    "    }\n"
    "    float mx = g[0];\n"
    "    for (int e = 1; e < E; e++) if (g[e] > mx) mx = g[e];\n"
    "    float sum = 0.f;\n"
    "    for (int e = 0; e < E; e++) sum += expf(g[e] - mx);\n"
    "    int taken[TK];\n"
    "    for (int kk = 0; kk < TK; kk++) {\n"
    "      int best = -1; float bv = -3.4e38f;\n"
    "      for (int e = 0; e < E; e++) {\n"
    "        int used = 0;\n"
    "        for (int j = 0; j < kk; j++) if (taken[j] == e) used = 1;\n"
    "        if (!used && g[e] > bv) { bv = g[e]; best = e; }\n"
    "      }\n"
    "      taken[kk] = best; sel[s * TK + kk] = best;\n"
    "      probs[s * TK + kk] = expf(g[best] - mx) / sum;\n"
    "    }\n"
    "  }\n"
    "}\n", e, k, sk, S);
}

// verify MoE h-phase (port of moe_h_u, deepseek_v2.py:344-354, + the base
// moe_full_h shared-expert tail): routed slots dedup ON-DEVICE across the S*K
// window (only the first slot with expert e reads wg/wu, computing for every
// token), shared-expert warps hoist the weight and loop tokens.  h layout:
// routed h[(t*K+kk)*I + i], shared at S*K*I + t*SI + i.
static char *src_moe_h_u(u32 S) {
  u32 d = C.dim, mi = C.moe_inter, k = C.top_k, si = C.moe_inter * C.n_shared;
  if (IS_CUDA) return mks(
    "#define D %u\n#define I %u\n#define TK %u\n#define SI %u\n#define S %u\n"
    "extern \"C\" __global__ void moe_h_u(float* h, const bf16t* x, const int* sel,\n"
    "    const bf16t* wg, const bf16t* wu, const bf16t* swg, const bf16t* swu) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= S * TK * I + SI) return;\n"
    "  const bf16t *wgi, *wui;\n"
    "  int routed = warp < S * TK * I;\n"
    "  int e = 0;\n"
    "  if (routed) {\n"
    "    int slot = warp / I, i = warp %% I; e = sel[slot];\n"
    "    for (int jj = 0; jj < slot; jj++) if (sel[jj] == e) return;\n"
    "    wgi = wg + ((long)e * I + i) * D; wui = wu + ((long)e * I + i) * D;\n"
    "  } else {\n"
    "    int i = warp - S * TK * I;\n"
    "    wgi = swg + (long)i * D; wui = swu + (long)i * D;\n"
    "  }\n"
    "  float g[S], u[S];\n"
    "#pragma unroll\n"
    "  for (int t = 0; t < S; t++) { g[t] = 0.f; u[t] = 0.f; }\n"
    "  for (int k = lane; k < D; k += 32) {\n"
    "    float a = bf2f(wgi[k]), b = bf2f(wui[k]);\n"
    "#pragma unroll\n"
    "    for (int t = 0; t < S; t++) { float xk = bf2f(x[(long)t * D + k]); g[t] += xk * a; u[t] += xk * b; }\n"
    "  }\n"
    "  for (int t = 0; t < S; t++) {\n"
    "    float gg = g[t], ug = u[t];\n"
    "    for (int o = 16; o > 0; o >>= 1) { gg += __shfl_down_sync(0xffffffffu, gg, o); ug += __shfl_down_sync(0xffffffffu, ug, o); }\n"
    "    if (lane == 0) {\n"
    "      float gs = gg / (1.f + __expf(-gg));\n"
    "      if (routed) {\n"
    "        for (int kk = 0; kk < TK; kk++) if (sel[t * TK + kk] == e) { h[(long)(t * TK + kk) * I + (warp %% I)] = gs * ug; break; }\n"
    "      } else {\n"
    "        h[(long)S * TK * I + (long)t * SI + (warp - S * TK * I)] = gs * ug;\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n", d, mi, k, si, S);
  return mks(
    "#include <math.h>\n#define D %u\n#define I %u\n#define TK %u\n#define SI %u\n#define S %u\n"
    "void moe_h_u(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *h = (float *)bufs[0]; const bf16t *x = (const bf16t *)bufs[1]; const int *sel = (const int *)bufs[2];\n"
    "  const bf16t *wg = (const bf16t *)bufs[3]; const bf16t *wu = (const bf16t *)bufs[4];\n"
    "  const bf16t *swg = (const bf16t *)bufs[5]; const bf16t *swu = (const bf16t *)bufs[6];\n"
    "  for (int t = 0; t < S; t++) {\n"
    "    for (int kk = 0; kk < TK; kk++) {\n"
    "      int e = sel[t * TK + kk];\n"
    "      for (int i = 0; i < I; i++) {\n"
    "        const bf16t *wgi = wg + ((long)e * I + i) * D; const bf16t *wui = wu + ((long)e * I + i) * D;\n"
    "        float g = 0.f, u = 0.f;\n"
    "        for (int k = 0; k < D; k++) { float xk = bf2f(x[(long)t * D + k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }\n"
    "        float gs = g / (1.f + expf(-g));\n"
    "        h[(long)(t * TK + kk) * I + i] = gs * u;\n"
    "      }\n"
    "    }\n"
    "    for (int i = 0; i < SI; i++) {\n"
    "      const bf16t *wgi = swg + (long)i * D; const bf16t *wui = swu + (long)i * D;\n"
    "      float g = 0.f, u = 0.f;\n"
    "      for (int k = 0; k < D; k++) { float xk = bf2f(x[(long)t * D + k]); g += xk * bf2f(wgi[k]); u += xk * bf2f(wui[k]); }\n"
    "      float gs = g / (1.f + expf(-g));\n"
    "      h[(long)S * TK * I + (long)t * SI + i] = gs * u;\n"
    "    }\n"
    "  }\n"
    "}\n", d, mi, k, si, S);
}

// verify MoE out-phase (port of moe_out_u, deepseek_v2.py:356-364, + the base
// moe_full_out shared tail and residual epilogue): warp per (t, d).
static char *src_moe_out_u(u32 S) {
  u32 d = C.dim, mi = C.moe_inter, k = C.top_k, si = C.moe_inter * C.n_shared;
  if (IS_CUDA) return mks(
    "#define D %u\n#define I %u\n#define TK %u\n#define SI %u\n#define S %u\n"
    "extern \"C\" __global__ void moe_out_u(bf16t* out, const float* h, const int* sel,\n"
    "    const float* probs, const bf16t* wd, const bf16t* swd, const bf16t* x1) {\n"
    "  int warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5; int lane = threadIdx.x & 31;\n"
    "  if (warp >= S * D) return;\n"
    "  int t = warp / D, d = warp %% D; float acc = 0.f;\n"
    "  for (int kk = 0; kk < TK; kk++) {\n"
    "    int e = sel[t * TK + kk];\n"
    "    const bf16t* wde = wd + ((long)e * D + d) * I;\n"
    "    const float* he = h + (long)(t * TK + kk) * I;\n"
    "    float sv = 0.f;\n"
    "#pragma unroll 8\n"
    "    for (int i = lane; i < I; i += 32) sv += he[i] * bf2f(wde[i]);\n"
    "    for (int o = 16; o > 0; o >>= 1) sv += __shfl_down_sync(0xffffffffu, sv, o);\n"
    "    if (lane == 0) acc += probs[t * TK + kk] * sv;\n"
    "  }\n"
    "  {\n"
    "    const bf16t* wde = swd + (long)d * SI;\n"
    "    const float* hs = h + (long)S * TK * I + (long)t * SI;\n"
    "    float sv = 0.f;\n"
    "#pragma unroll 8\n"
    "    for (int i = lane; i < SI; i += 32) sv += hs[i] * bf2f(wde[i]);\n"
    "    for (int o = 16; o > 0; o >>= 1) sv += __shfl_down_sync(0xffffffffu, sv, o);\n"
    "    if (lane == 0) acc += sv;\n"
    "  }\n"
    "  if (lane == 0) out[(long)t * D + d] = f2bfd(bf2f(x1[(long)t * D + d]) + bf2f(f2bfd(acc)));\n"
    "}\n", d, mi, k, si, S);
  return mks(
    "#define D %u\n#define I %u\n#define TK %u\n#define SI %u\n#define S %u\n"
    "void moe_out_u(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  bf16t *out = (bf16t *)bufs[0]; const float *h = (const float *)bufs[1]; const int *sel = (const int *)bufs[2];\n"
    "  const float *probs = (const float *)bufs[3]; const bf16t *wd = (const bf16t *)bufs[4]; const bf16t *swd = (const bf16t *)bufs[5];\n"
    "  const bf16t *x1 = (const bf16t *)bufs[6];\n"
    "  for (int t = 0; t < S; t++)\n"
    "    for (int d = 0; d < D; d++) {\n"
    "      float acc = 0.f;\n"
    "      for (int kk = 0; kk < TK; kk++) {\n"
    "        int e = sel[t * TK + kk];\n"
    "        float sv = 0.f;\n"
    "        for (int i = 0; i < I; i++) sv += h[(long)(t * TK + kk) * I + i] * bf2f(wd[((long)e * D + d) * I + i]);\n"
    "        acc += probs[t * TK + kk] * sv;\n"
    "      }\n"
    "      {\n"
    "        float sv = 0.f;\n"
    "        for (int i = 0; i < SI; i++) sv += h[(long)S * TK * I + (long)t * SI + i] * bf2f(swd[(long)d * SI + i]);\n"
    "        acc += sv;\n"
    "      }\n"
    "      out[(long)t * D + d] = f2bfd(bf2f(x1[(long)t * D + d]) + bf2f(f2bfd(acc)));\n"
    "    }\n"
    "}\n", d, mi, k, si, S);
}

// argmax over S logit rows: the base 2-pass argmax per row (blockIdx.y = s),
// final pass writes am[s] (ties -> lowest index, same as the base).
static char *src_apart_s(u32 S) {
  u32 v = C.vocab, nb = C.nb, ch = C.chunk;
  if (IS_CUDA) return mks(
    "#define V %u\n#define NB %u\n#define CH %u\n#define S %u\n"
    "extern \"C\" __global__ void argmax_part_s(float* pv, int* pi, const float* lg) {\n"
    "  __shared__ float sv[256]; __shared__ int si[256];\n"
    "  int b = blockIdx.x; int t = threadIdx.x; int s = blockIdx.y;\n"
    "  const float* ls = lg + (long)s * V;\n"
    "  long lo = (long)b * CH; long hi = lo + CH; if (hi > V) hi = V;\n"
    "  float bv = -3.4e38f; int bi = 2147483647;\n"
    "  for (long i = lo + t; i < hi; i += blockDim.x) {\n"
    "    float x = ls[i];\n"
    "    if (x > bv || (x == bv && (int)i < bi)) { bv = x; bi = (int)i; }\n"
    "  }\n"
    "  sv[t] = bv; si[t] = bi;\n"
    "  __syncthreads();\n"
    "  for (int r = blockDim.x / 2; r > 0; r >>= 1) {\n"
    "    if (t < r && (sv[t + r] > sv[t] || (sv[t + r] == sv[t] && si[t + r] < si[t]))) { sv[t] = sv[t + r]; si[t] = si[t + r]; }\n"
    "    __syncthreads();\n"
    "  }\n"
    "  if (t == 0) { pv[s * NB + b] = sv[0]; pi[s * NB + b] = si[0]; }\n"
    "}\n", v, nb, ch, S);
  return mks(
    "#define V %u\n#define NB %u\n#define CH %u\n#define S %u\n"
    "void argmax_part_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  float *pv = (float *)bufs[0]; int *pi = (int *)bufs[1]; const float *lg = (const float *)bufs[2];\n"
    "  for (int s = 0; s < S; s++)\n"
    "    for (int b = 0; b < NB; b++) {\n"
    "      long lo = (long)b * CH; long hi = lo + CH; if (hi > V) hi = V;\n"
    "      float bv = -3.4e38f; int bi = 2147483647;\n"
    "      for (long i = lo; i < hi; i++)\n"
    "        if (lg[(long)s * V + i] > bv) { bv = lg[(long)s * V + i]; bi = (int)i; }\n"
    "      pv[s * NB + b] = bv; pi[s * NB + b] = bi;\n"
    "    }\n"
    "}\n", v, nb, ch, S);
}

static char *src_afin_s(u32 S) {
  u32 nb = C.nb;
  if (IS_CUDA) return mks(
    "#define NB %u\n#define S %u\n"
    "extern \"C\" __global__ void argmax_fin_s(int* am, const float* pv, const int* pi) {\n"
    "  if (threadIdx.x != 0) return;\n"
    "  int s = blockIdx.x;\n"
    "  float bv = pv[s * NB]; int bi = pi[s * NB];\n"
    "  for (int b = 1; b < NB; b++)\n"
    "    if (pv[s * NB + b] > bv || (pv[s * NB + b] == bv && pi[s * NB + b] < bi)) { bv = pv[s * NB + b]; bi = pi[s * NB + b]; }\n"
    "  am[s] = bi;\n"
    "}\n", nb, S);
  return mks(
    "#define NB %u\n#define S %u\n"
    "void argmax_fin_s(void *const *bufs, unsigned const *grid, unsigned const *block) {\n"
    "  (void)grid; (void)block;\n"
    "  int *am = (int *)bufs[0]; const float *pv = (const float *)bufs[1]; const int *pi = (const int *)bufs[2];\n"
    "  for (int s = 0; s < S; s++) {\n"
    "    float bv = pv[s * NB]; int bi = pi[s * NB];\n"
    "    for (int b = 1; b < NB; b++)\n"
    "      if (pv[s * NB + b] > bv || (pv[s * NB + b] == bv && pi[s * NB + b] < bi)) { bv = pv[s * NB + b]; bi = pi[s * NB + b]; }\n"
    "    am[s] = bi;\n"
    "  }\n"
    "}\n", nb, S);
}

// === custom fire ============================================================
static void krun2(char const *name, char const *src, u32 gx, u32 gy, u32 bx,
                  u32 const *args, u32 nargs, u32 const *outs, u32 nouts) {
  u32 grid[3] = {gx, gy, 1}, block[3] = {bx, 1, 1};
  u32 kid = kernel_emit_custom_program(name, src, grid, block, args, nargs, outs, nouts, 0);
  if (kid == 0 || kernel_custom_fire(kid) != 0) {
    fprintf(stderr, "FATAL: custom kernel %s failed\n", name);
    exit(1);
  }
}

static void krun(char const *name, char const *src, u32 gx, u32 bx,
                 u32 const *args, u32 nargs, u32 const *outs, u32 nouts) {
  krun2(name, src, gx, 1, bx, args, nargs, outs, nouts);
}

// Fire one split-K GEMV pair: phase 1 into the shared partials buffer,
// phase 2 folds into out_tid.
static u32 T_part = 0;
static void gemv2(char const *nm_sk, char const *s_sk, char const *nm_red,
                  char const *s_red, u32 N, u32 sk, u32 x_tid, u32 w_tid,
                  u32 out_tid) {
  u32 bx = gemv_block(N), gx = cdiv(N, bx);
  u32 a1[3] = {T_part, x_tid, w_tid}, o1[1] = {0};
  krun2(nm_sk, s_sk, gx, sk, bx, a1, 3, o1, 1);
  u32 a2[2] = {out_tid, T_part}, o2[1] = {0};
  krun2(nm_red, s_red, gx, 1, bx, a2, 2, o2, 1);
}

// === native uop pieces (the dense-MLP silu*u elementwise is the one
// remaining scheduler kernel; everything else is injected custom) ===========
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
static u32 W_win[MAX_LAYERS], W_wqkv[MAX_LAYERS], W_wkvn[MAX_LAYERS];
static u32 W_wuk[MAX_LAYERS], W_wuv[MAX_LAYERS], W_wo[MAX_LAYERS], W_wpost[MAX_LAYERS];
static u32 W_wg1[MAX_LAYERS], W_wu1[MAX_LAYERS], W_wd1[MAX_LAYERS];
static u32 W_gate[MAX_LAYERS], W_wg[MAX_LAYERS], W_wu[MAX_LAYERS], W_wd[MAX_LAYERS];
static u32 W_swg[MAX_LAYERS], W_swu[MAX_LAYERS], W_swd[MAX_LAYERS];
static u32 L_ckv[MAX_LAYERS], L_kpe[MAX_LAYERS];
static u32 T_tok, T_len, T_ql, T_sm, T_ol, T_ob, T_sel, T_probs, T_h, T_pv, T_pi;
static u32 LOGITS_TID = 0;

// Activation buffers, all written/read by custom kernels only (no native
// consumers -> no Term-dedup hazard -> one shared set for every layer).
// T_xr is the residual stream: embed writes it, rms1 reads it, gred_o_rms
// pairs it with the attention out, moe_full_out / gred_down write it back.
static u32 T_xr, T_xnb, T_x1, T_xn2b, T_xn2f, T_q, T_xnf, T_g1o, T_u1o;

// One GEMV instance per distinct (K, N, dtypes): source + split factor.
typedef struct { char *sk_src, *red_src; char sk_nm[16], red_nm[16]; u32 N, sk; } Gemv;
static Gemv G_qkv, G_o, G_gate, G_ffw, G_down, G_lm;

static void gemv_make(Gemv *g, char const *tag, u32 K, u32 N,
                      int x_f32, int w_f32, int out_bf16) {
  snprintf(g->sk_nm, sizeof g->sk_nm, "gemv_%s", tag);
  snprintf(g->red_nm, sizeof g->red_nm, "gred_%s", tag);
  g->N       = N;
  g->sk      = g->sk != 0 ? g->sk : gemv_pick_sk(K, N);
  g->sk_src  = src_gemv(g->sk_nm, K, N, g->sk, x_f32, w_f32);
  g->red_src = src_gred(g->red_nm, N, g->sk, out_bf16);
}

static void gemv_run(Gemv const *g, u32 x_tid, u32 w_tid, u32 out_tid) {
  gemv2(g->sk_nm, g->sk_src, g->red_nm, g->red_src, g->N, g->sk,
        x_tid, w_tid, out_tid);
}

// Phase 1 only: the partials are folded by a fused consumer (gred_q /
// kv_prep / gred_o_rms / route / gred_down) instead of a plain gred.
static void gemv_sk_run(Gemv const *g, u32 x_tid, u32 w_tid) {
  u32 bx = gemv_block(g->N), gx = cdiv(g->N, bx);
  u32 a1[3] = {T_part, x_tid, w_tid}, o1[1] = {0};
  krun2(g->sk_nm, g->sk_src, gx, g->sk, bx, a1, 3, o1, 1);
}

// S-row spec twin of Gemv: the split factor is INHERITED from the base instance
// (identical K-chunk boundaries are part of the numerics contract), the partials
// land in the S-sized T_partS.
typedef struct { char *sk_src, *red_src; char sk_nm[24], red_nm[24]; u32 N, sk; } SGemv;
static u32 SPEC = 0;    // DSV2_SPEC: S-token speculative verify window (0 = off)
static u32 T_partS = 0;

static void sgemv_make(SGemv *g, char const *tag, u32 K, u32 N, u32 sk,
                       int x_f32, int w_f32, int out_bf16) {
  snprintf(g->sk_nm, sizeof g->sk_nm, "gemv_%s_s", tag);
  snprintf(g->red_nm, sizeof g->red_nm, "gred_%s_s", tag);
  g->N       = N;
  g->sk      = sk;
  g->sk_src  = src_gemv_s(g->sk_nm, K, N, sk, x_f32, w_f32, SPEC);
  g->red_src = src_gred_s(g->red_nm, N, sk, out_bf16, SPEC);
}

static void sgemv_sk_run(SGemv const *g, u32 x_tid, u32 w_tid) {
  u32 bx = gemv_block(g->N), gx = cdiv(g->N, bx);
  u32 a1[3] = {T_partS, x_tid, w_tid}, o1[1] = {0};
  krun2(g->sk_nm, g->sk_src, gx, g->sk, bx, a1, 3, o1, 1);
}

static void sgemv_run(SGemv const *g, u32 x_tid, u32 w_tid, u32 out_tid) {
  sgemv_sk_run(g, x_tid, w_tid);
  u32 bx = gemv_block(g->N), gx = cdiv(g->N, bx);
  u32 a2[2] = {out_tid, T_partS}, o2[1] = {0};
  krun2(g->red_nm, g->red_src, gx, SPEC, bx, a2, 2, o2, 1);
}

static char *S_EMBED, *S_QLAT, *S_SCORE, *S_MIX, *S_O;
static char *S_ROUTE, *S_MOEH, *S_MOEOUT, *S_APART, *S_AFIN, *S_BUMP;
static char *S_RMS1, *S_RMSF, *S_GREDQ, *S_KVPREP, *S_OREDRMS, *S_DOWNRES;

static void model_init(void) {
  u32 d = C.dim, hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, kv = C.kv_lora, dv = C.d_v;
  u32 si = C.moe_inter * C.n_shared;
  if (PACK != NULL) {
    W_embed = pack_ld("embed", C.vocab, d, DT_BF16);
    W_norm  = pack_ld("norm", d, 0, DT_FP32);
    W_lm    = pack_ld("lm_head", d, C.vocab, DT_BF16);
  } else {
    W_embed = mk_bf16_rand(C.vocab, d);
    W_norm  = mk_f32_fill(d, 0, 1.0f);
    W_lm    = mk_bf16_rand(d, C.vocab);
  }
  rope_tables(&T_cos, &T_sin);
  for (u32 l = 0; l < C.n_layers; l++) {
    if (PACK != NULL) {
      W_win[l]   = pack_lw(l, "win", d, 0, DT_FP32);
      {
        char nq_nm[48], nkv_nm[48];
        u32 nq = hn * (dn + dr), nkv = kv + dr;
        snprintf(nq_nm, sizeof nq_nm, "l%u.wq", l);
        snprintf(nkv_nm, sizeof nkv_nm, "l%u.wkv", l);
        W_wqkv[l] = concat_rows_bf16(
            (u16 const *)pack_find(nq_nm, (u64)d * nq * 2), nq,
            (u16 const *)pack_find(nkv_nm, (u64)d * nkv * 2), nkv, d);
      }
      W_wkvn[l]  = pack_lw(l, "wkvn", kv, 0, DT_FP32);
      W_wuk[l]   = pack_lw(l, "wuk", hn * dn * kv, 0, DT_BF16);
      W_wuv[l]   = pack_lw(l, "wuv", hn * dv * kv, 0, DT_BF16);
      W_wo[l]    = pack_lw(l, "wo", hn * dv, d, DT_BF16);
      W_wpost[l] = pack_lw(l, "wpost", d, 0, DT_FP32);
      if (l < C.first_dense) {
        W_wg1[l] = pack_lw(l, "wg1", d, C.dense_inter, DT_BF16);
        W_wu1[l] = pack_lw(l, "wu1", d, C.dense_inter, DT_BF16);
        W_wd1[l] = pack_lw(l, "wd1", C.dense_inter, d, DT_BF16);
      } else {
        W_gate[l] = pack_lw(l, "gate", d, C.n_exp, DT_FP32);
        W_wg[l]   = pack_lw(l, "wg", C.n_exp * C.moe_inter, d, DT_BF16);
        W_wu[l]   = pack_lw(l, "wu", C.n_exp * C.moe_inter, d, DT_BF16);
        W_wd[l]   = pack_lw(l, "wd", C.n_exp * d, C.moe_inter, DT_BF16);
        W_swg[l]  = pack_lw(l, "swg", si, d, DT_BF16);
        W_swu[l]  = pack_lw(l, "swu", si, d, DT_BF16);
        W_swd[l]  = pack_lw(l, "swd", d, si, DT_BF16);
      }
    } else {
      W_win[l]   = mk_f32_fill(d, 0, 1.0f);
      {
        u32 nq = hn * (dn + dr), nkv = kv + dr;
        u16 *wq  = (u16 *)malloc((u64)d * nq * 2);
        u16 *wkv = (u16 *)malloc((u64)d * nkv * 2);
        for (u64 i = 0; i < (u64)d * nq; i++)  wq[i]  = rbf16();
        for (u64 i = 0; i < (u64)d * nkv; i++) wkv[i] = rbf16();
        W_wqkv[l] = concat_rows_bf16(wq, nq, wkv, nkv, d);
        free(wq); free(wkv);
      }
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
    }
    L_ckv[l] = mk_bf16_zero(C.max_seq, kv);
    L_kpe[l] = mk_bf16_zero(C.max_seq, dr);
  }
  G_qkv.sk = gemv_pick_sk(d, hn * (dn + dr));
  gemv_make(&G_qkv,  "qkv",  d, hn * (dn + dr) + kv + dr, 0, 0, 1);
  gemv_make(&G_o,    "o",    hn * dv, d,        0, 0, 1);
  gemv_make(&G_gate, "gate", d, C.n_exp,        1, 1, 0);
  gemv_make(&G_ffw,  "ffw",  d, C.dense_inter,  0, 0, 1);
  gemv_make(&G_down, "down", C.dense_inter, d,  0, 0, 1);
  gemv_make(&G_lm,   "lm",   d, C.vocab,        1, 0, 0);
  {
    Gemv const *gs[6] = {&G_qkv, &G_o, &G_gate, &G_ffw, &G_down, &G_lm};
    u64 pmax = 0;
    for (u32 i = 0; i < 6; i++) {
      u64 n = (u64)gs[i]->sk * gs[i]->N;
      if (n > pmax) pmax = n;
    }
    T_part = mk_f32_fill((u32)pmax, 0, 0.0f);
  }
  T_g1o   = mk_bf16_zero(1, C.dense_inter);
  T_u1o   = mk_bf16_zero(1, C.dense_inter);
  T_xr    = mk_bf16_zero(d, 0);
  T_xnb   = mk_bf16_zero(d, 0);
  T_x1    = mk_bf16_zero(d, 0);
  T_xn2b  = mk_bf16_zero(d, 0);
  T_xn2f  = mk_f32_fill(d, 0, 0.0f);
  T_xnf   = mk_f32_fill(d, 0, 0.0f);
  T_q     = mk_bf16_zero(hn * (dn + dr), 0);
  T_tok   = mk_i32(1, 1);
  T_len   = mk_i32(1, 1);
  T_ql    = mk_f32_fill(hn * kv, 0, 0.0f);
  T_sm    = mk_f32_fill(hn * C.max_seq, 0, 0.0f);
  T_ol    = mk_f32_fill(hn * kv, 0, 0.0f);
  T_ob    = mk_bf16_zero(hn * dv, 0);
  T_sel   = mk_i32(C.top_k, 0);
  T_probs = mk_f32_fill(C.top_k, 0, 0.0f);
  T_h     = mk_f32_fill(C.top_k * C.moe_inter + si, 0, 0.0f);
  T_pv    = mk_f32_fill(C.nb, 0, 0.0f);
  T_pi    = mk_i32(C.nb, 0);
  LOGITS_TID = mk_f32_fill(1, C.vocab, 0.0f);
  S_EMBED = src_embed(); S_QLAT = src_qlat(); S_SCORE = src_score();
  S_MIX = src_mix(); S_O = src_mla_o();
  S_ROUTE = src_route(G_gate.sk); S_MOEH = src_moe_h(); S_MOEOUT = src_moe_out();
  S_APART = src_apart(); S_AFIN = src_afin(); S_BUMP = src_bump();
  S_RMS1 = src_rms("rms1", d, 1);
  S_RMSF = src_rms("rms_f", d, 0);
  S_GREDQ = src_gred_q_rope(G_qkv.sk, G_qkv.N);
  S_KVPREP = src_kv_prep(G_qkv.sk, G_qkv.N, hn * (dn + dr));
  S_OREDRMS = src_gred_o_rms(G_o.sk);
  S_DOWNRES = src_gred_down_res(G_down.sk);
}

// === speculative state (DSV2_SPEC=S) ========================================
static u32 T_ids, T_ca, T_ce, T_am;
static u32 T_xrS, T_xnbS, T_x1S, T_xn2bS, T_xn2fS, T_xnfS;
static u32 T_qS, T_qlS, T_smS, T_olS, T_obS, T_selS, T_probsS, T_hS;
static u32 T_g1oS, T_u1oS, T_pvS, T_piS, LOGITS_S = 0;
static char *S_DRAFT, *S_ACCEPT, *S_EMBED_S, *S_RMS1_S, *S_RMSF_S;
static char *S_GREDQ_S, *S_KVPREP_S, *S_QLATV, *S_SCOREV, *S_MIXV, *S_OV;
static char *S_OREDRMS_S, *S_DOWNRES_S, *S_ROUTE_S, *S_MOEHU, *S_MOEOUTU;
static char *S_APART_S, *S_AFIN_S;
static SGemv GS_qkv, GS_o, GS_gate, GS_ffw, GS_down, GS_lm;

static void spec_init(int forced) {
  u32 S = SPEC, d = C.dim, hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, kv = C.kv_lora, dv = C.d_v;
  u32 si = C.moe_inter * C.n_shared;
  sgemv_make(&GS_qkv,  "qkv",  d, hn * (dn + dr) + kv + dr, G_qkv.sk,  0, 0, 1);
  sgemv_make(&GS_o,    "o",    hn * dv, d,        G_o.sk,    0, 0, 1);
  sgemv_make(&GS_gate, "gate", d, C.n_exp,        G_gate.sk, 1, 1, 0);
  sgemv_make(&GS_ffw,  "ffw",  d, C.dense_inter,  G_ffw.sk,  0, 0, 1);
  sgemv_make(&GS_down, "down", C.dense_inter, d,  G_down.sk, 0, 0, 1);
  sgemv_make(&GS_lm,   "lm",   d, C.vocab,        G_lm.sk,   1, 0, 0);
  {
    SGemv const *gs[6] = {&GS_qkv, &GS_o, &GS_gate, &GS_ffw, &GS_down, &GS_lm};
    u64 pmax = 0;
    for (u32 i = 0; i < 6; i++) {
      u64 n = (u64)gs[i]->sk * gs[i]->N;
      if (n > pmax) pmax = n;
    }
    T_partS = mk_f32_fill((u32)(pmax * S), 0, 0.0f);
  }
  T_ids    = mk_i32(C.max_seq, 0);
  T_ca     = mk_i32(S, 0);
  T_ce     = mk_i32(S, 0);
  T_am     = mk_i32(S, 0);
  T_xrS    = mk_bf16_zero(S, d);
  T_xnbS   = mk_bf16_zero(S, d);
  T_x1S    = mk_bf16_zero(S, d);
  T_xn2bS  = mk_bf16_zero(S, d);
  T_xn2fS  = mk_f32_fill(S, d, 0.0f);
  T_xnfS   = mk_f32_fill(S, d, 0.0f);
  T_qS     = mk_bf16_zero(S, hn * (dn + dr));
  T_qlS    = mk_f32_fill(S, hn * kv, 0.0f);
  T_smS    = mk_f32_fill(S, hn * C.max_seq, 0.0f);
  T_olS    = mk_f32_fill(S, hn * kv, 0.0f);
  T_obS    = mk_bf16_zero(S, hn * dv);
  T_selS   = mk_i32(S * C.top_k, 0);
  T_probsS = mk_f32_fill(S * C.top_k, 0, 0.0f);
  T_hS     = mk_f32_fill(S * C.top_k * C.moe_inter + S * si, 0, 0.0f);
  T_g1oS   = mk_bf16_zero(1, S * C.dense_inter);
  T_u1oS   = mk_bf16_zero(1, S * C.dense_inter);
  T_pvS    = mk_f32_fill(S * C.nb, 0, 0.0f);
  T_piS    = mk_i32(S * C.nb, 0);
  LOGITS_S = mk_f32_fill(S, C.vocab, 0.0f);
  S_DRAFT     = src_draft_prep(S, forced);
  S_ACCEPT    = src_accept_commit(S);
  S_EMBED_S   = src_embed_s(S);
  S_RMS1_S    = src_rms_s("rms1_s", d, 1, S);
  S_RMSF_S    = src_rms_s("rms_f_s", d, 0, S);
  S_GREDQ_S   = src_gred_q_rope_s(G_qkv.sk, G_qkv.N, S);
  S_KVPREP_S  = src_kv_prep_s(G_qkv.sk, G_qkv.N, hn * (dn + dr), S);
  S_QLATV     = src_qlat_v(S);
  S_SCOREV    = src_score_v(S);
  S_MIXV      = src_mix_v(S);
  S_OV        = src_o_v(S);
  S_OREDRMS_S = src_gred_o_rms_s(G_o.sk, S);
  S_DOWNRES_S = src_gred_down_res_s(G_down.sk, S);
  S_ROUTE_S   = src_route_s(G_gate.sk, S);
  S_MOEHU     = src_moe_h_u(S);
  S_MOEOUTU   = src_moe_out_u(S);
  S_APART_S   = src_apart_s(S);
  S_AFIN_S    = src_afin_s(S);
}

// === the decode step (built + fired once, under capture) ===================
static void step_build(void) {
  u32 d = C.dim, hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, kv = C.kv_lora, dv = C.d_v;
  u32 si = C.moe_inter * C.n_shared;
  {
    u32 a[3] = {T_xr, W_embed, T_tok}, o[1] = {0};
    krun("embed_row", S_EMBED, cdiv(d, 256), 256, a, 3, o, 1);
  }
  for (u32 l = 0; l < C.n_layers; l++) {
    {
      u32 a[3] = {T_xnb, T_xr, W_win[l]}, o[1] = {0};
      krun("rms1", S_RMS1, 1, 256, a, 3, o, 1);
    }
    gemv_sk_run(&G_qkv, T_xnb, W_wqkv[l]);
    {
      u32 a[5] = {T_q, T_part, T_cos, T_sin, T_len}, o[1] = {0};
      krun("gred_q", S_GREDQ, cdiv(hn * dn + hn * dr / 2, 256), 256, a, 5, o, 1);
    }
    {
      u32 a[7] = {L_ckv[l], L_kpe[l], T_part, W_wkvn[l], T_cos, T_sin, T_len}, o[2] = {0, 1};
      krun("kv_prep", S_KVPREP, 1, (kv + dr / 2 + 31) / 32 * 32, a, 7, o, 2);
    }
    {
      u32 a[3] = {T_ql, T_q, W_wuk[l]}, o[1] = {0};
      krun("mla_qlat", S_QLAT, cdiv(hn * kv, 64), 64, a, 3, o, 1);
    }
    {
      u32 a[6] = {T_sm, T_ql, T_q, L_ckv[l], L_kpe[l], T_len}, o[1] = {0};
      krun("mla_score", S_SCORE, hn, 256, a, 6, o, 1);
    }
    {
      u32 a[4] = {T_ol, T_sm, L_ckv[l], T_len}, o[1] = {0};
      krun("mla_mix", S_MIX, cdiv(hn * kv, 64), 64, a, 4, o, 1);
    }
    {
      u32 a[3] = {T_ob, T_ol, W_wuv[l]}, o[1] = {0};
      krun("mla_o", S_O, cdiv(hn * dv * 32, 256), 256, a, 3, o, 1);
    }
    gemv_sk_run(&G_o, T_ob, W_wo[l]);
    {
      u32 a[6] = {T_x1, T_xn2b, T_xn2f, T_part, T_xr, W_wpost[l]}, o[3] = {0, 1, 2};
      krun("gred_o_rms", S_OREDRMS, 1, 256, a, 6, o, 3);
    }
    if (l < C.first_dense) {
      gemv_run(&G_ffw, T_xn2b, W_wg1[l], T_g1o);
      gemv_run(&G_ffw, T_xn2b, W_wu1[l], T_u1o);
      Term h = uop_binary(UOP_MUL, silu(tt(T_g1o), C.dense_inter), tt(T_u1o));
      u32 h_tid = rz(h);
      gemv_sk_run(&G_down, h_tid, W_wd1[l]);
      {
        u32 a[3] = {T_xr, T_part, T_x1}, o[1] = {0};
        krun("gred_down", S_DOWNRES, cdiv(d, 256), 256, a, 3, o, 1);
      }
    } else {
      gemv_sk_run(&G_gate, T_xn2f, W_gate[l]);
      {
        u32 a[3] = {T_sel, T_probs, T_part}, o[2] = {0, 1};
        krun("route", S_ROUTE, 1, (C.n_exp + 31) / 32 * 32, a, 3, o, 2);
      }
      {
        u32 a[7] = {T_h, T_xn2b, T_sel, W_wg[l], W_wu[l], W_swg[l], W_swu[l]}, o[1] = {0};
        krun("moe_full_h", S_MOEH, cdiv((C.top_k * C.moe_inter + si) * 32, 256), 256, a, 7, o, 1);
      }
      {
        u32 a[7] = {T_xr, T_h, T_sel, T_probs, W_wd[l], W_swd[l], T_x1}, o[1] = {0};
        krun("moe_full_out", S_MOEOUT, cdiv(d * 32, 256), 256, a, 7, o, 1);
      }
    }
  }
  {
    u32 a[3] = {T_xnf, T_xr, W_norm}, o[1] = {0};
    krun("rms_f", S_RMSF, 1, 256, a, 3, o, 1);
  }
  gemv_run(&G_lm, T_xnf, W_lm, LOGITS_TID);
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

// === the spec step (built + fired once, under a SECOND capture slot) ========
// draft_prep -> S-row verify forward -> per-position argmax -> accept_commit;
// one replay advances the device ids/len state by 1..S committed tokens with
// zero host values.
static void step_build_spec(void) {
  u32 S = SPEC, d = C.dim, hn = C.n_heads, dn = C.d_nope, dr = C.d_rope, kv = C.kv_lora, dv = C.d_v;
  u32 si = C.moe_inter * C.n_shared;
  {
    u32 a[4] = {T_ca, T_ce, T_ids, T_len}, o[2] = {0, 1};
    krun("draft_prep", S_DRAFT, 1, 256, a, 4, o, 2);
  }
  {
    u32 a[3] = {T_xrS, W_embed, T_ce}, o[1] = {0};
    krun("embed_row_s", S_EMBED_S, cdiv(S * d, 256), 256, a, 3, o, 1);
  }
  for (u32 l = 0; l < C.n_layers; l++) {
    {
      u32 a[3] = {T_xnbS, T_xrS, W_win[l]}, o[1] = {0};
      krun2("rms1_s", S_RMS1_S, 1, S, 256, a, 3, o, 1);
    }
    sgemv_sk_run(&GS_qkv, T_xnbS, W_wqkv[l]);
    {
      u32 a[5] = {T_qS, T_partS, T_cos, T_sin, T_len}, o[1] = {0};
      krun2("gred_q_s", S_GREDQ_S, cdiv(hn * dn + hn * dr / 2, 256), S, 256, a, 5, o, 1);
    }
    {
      u32 a[7] = {L_ckv[l], L_kpe[l], T_partS, W_wkvn[l], T_cos, T_sin, T_len}, o[2] = {0, 1};
      krun2("kv_prep_s", S_KVPREP_S, 1, S, (kv + dr / 2 + 31) / 32 * 32, a, 7, o, 2);
    }
    {
      u32 a[3] = {T_qlS, T_qS, W_wuk[l]}, o[1] = {0};
      krun("qlat_v", S_QLATV, cdiv(hn * kv, 64), 64, a, 3, o, 1);
    }
    {
      u32 a[6] = {T_smS, T_qlS, T_qS, L_ckv[l], L_kpe[l], T_len}, o[1] = {0};
      krun2("score_v", S_SCOREV, hn, S, 256, a, 6, o, 1);
    }
    {
      u32 a[4] = {T_olS, T_smS, L_ckv[l], T_len}, o[1] = {0};
      krun("mix_v", S_MIXV, cdiv(hn * kv, 64), 64, a, 4, o, 1);
    }
    {
      u32 a[3] = {T_obS, T_olS, W_wuv[l]}, o[1] = {0};
      krun("o_v", S_OV, cdiv(hn * dv * 32, 256), 256, a, 3, o, 1);
    }
    sgemv_sk_run(&GS_o, T_obS, W_wo[l]);
    {
      u32 a[6] = {T_x1S, T_xn2bS, T_xn2fS, T_partS, T_xrS, W_wpost[l]}, o[3] = {0, 1, 2};
      krun2("gred_o_rms_s", S_OREDRMS_S, 1, S, 256, a, 6, o, 3);
    }
    if (l < C.first_dense) {
      sgemv_run(&GS_ffw, T_xn2bS, W_wg1[l], T_g1oS);
      sgemv_run(&GS_ffw, T_xn2bS, W_wu1[l], T_u1oS);
      Term h = uop_binary(UOP_MUL, silu(tt(T_g1oS), S * C.dense_inter), tt(T_u1oS));
      u32 h_tid = rz(h);
      sgemv_sk_run(&GS_down, h_tid, W_wd1[l]);
      {
        u32 a[3] = {T_xrS, T_partS, T_x1S}, o[1] = {0};
        krun2("gred_down_s", S_DOWNRES_S, cdiv(d, 256), S, 256, a, 3, o, 1);
      }
    } else {
      sgemv_sk_run(&GS_gate, T_xn2fS, W_gate[l]);
      {
        u32 a[3] = {T_selS, T_probsS, T_partS}, o[2] = {0, 1};
        krun2("route_s", S_ROUTE_S, 1, S, (C.n_exp + 31) / 32 * 32, a, 3, o, 2);
      }
      {
        u32 a[7] = {T_hS, T_xn2bS, T_selS, W_wg[l], W_wu[l], W_swg[l], W_swu[l]}, o[1] = {0};
        krun("moe_h_u", S_MOEHU, cdiv((S * C.top_k * C.moe_inter + si) * 32, 256), 256, a, 7, o, 1);
      }
      {
        u32 a[7] = {T_xrS, T_hS, T_selS, T_probsS, W_wd[l], W_swd[l], T_x1S}, o[1] = {0};
        krun("moe_out_u", S_MOEOUTU, cdiv(S * d * 32, 256), 256, a, 7, o, 1);
      }
    }
  }
  {
    u32 a[3] = {T_xnfS, T_xrS, W_norm}, o[1] = {0};
    krun2("rms_f_s", S_RMSF_S, 1, S, 256, a, 3, o, 1);
  }
  sgemv_run(&GS_lm, T_xnfS, W_lm, LOGITS_S);
  {
    u32 a[3] = {T_pvS, T_piS, LOGITS_S}, o[2] = {0, 1};
    krun2("argmax_part_s", S_APART_S, C.nb, S, 256, a, 3, o, 2);
  }
  {
    u32 a[3] = {T_am, T_pvS, T_piS}, o[1] = {0};
    krun("argmax_fin_s", S_AFIN_S, S, 32, a, 3, o, 1);
  }
  {
    u32 a[4] = {T_ids, T_len, T_ca, T_am}, o[2] = {0, 1};
    krun("accept_commit", S_ACCEPT, 1, 32, a, 4, o, 2);
  }
}

static int read_i32(u32 tid) {
  int v = 0;
  TENS[tid].backend->buf_read(TENS[tid].buf_id, &v, 4);
  return v;
}

// DSV2_DUMP=1: byte-exact fingerprints after the captured step -- per-layer
// cache row 0 (u16-bit sums) + the logits bit-sum.  Diffing two builds'
// dumps localizes the first layer where a numerics change enters.

static void dump_tid(char const *nm, u32 tid, u64 nbytes) {
  static u8 dbuf[1 << 22];
  u64 s = 0;
  TENS[tid].backend->buf_read(TENS[tid].buf_id, dbuf, nbytes);
  for (u64 i = 0; i < nbytes; i++) s += dbuf[i] * (u64)(1 + (i & 7));
  printf("  [dump] %s bytesum=%llu\n", nm, (unsigned long long)s);
}

static void dump_fingerprints(void) {
  static u8 buf[1 << 20];
  dump_tid("T_ql", T_ql, (u64)C.n_heads * C.kv_lora * 4);
  dump_tid("T_sm", T_sm, (u64)C.n_heads * C.max_seq * 4);
  dump_tid("T_ol", T_ol, (u64)C.n_heads * C.kv_lora * 4);
  dump_tid("T_ob", T_ob, (u64)C.n_heads * C.d_v * 2);
  dump_tid("T_g1o", T_g1o, (u64)C.dense_inter * 2);
  dump_tid("T_u1o", T_u1o, (u64)C.dense_inter * 2);
  printf("  [dump] per-layer cache row 0 fingerprints:\n");
  for (u32 l = 0; l < C.n_layers; l++) {
    u64 sc = 0, sp = 0;
    TENS[L_ckv[l]].backend->buf_read(TENS[L_ckv[l]].buf_id, buf, (u64)C.kv_lora * 2);
    for (u32 i = 0; i < C.kv_lora; i++) sc += ((u16 *)buf)[i];
    TENS[L_kpe[l]].backend->buf_read(TENS[L_kpe[l]].buf_id, buf, (u64)C.d_rope * 2);
    for (u32 i = 0; i < C.d_rope; i++) sp += ((u16 *)buf)[i];
    printf("  [dump] l%02u ckv=%llu kpe=%llu\n", l,
           (unsigned long long)sc, (unsigned long long)sp);
  }
  {
    u64 s = 0;
    float first2[2];
    TENS[LOGITS_TID].backend->buf_read(TENS[LOGITS_TID].buf_id, buf, (u64)C.vocab * 4);
    for (u32 i = 0; i < C.vocab; i++) s += ((u32 *)buf)[i];
    memcpy(first2, buf, 8);
    printf("  [dump] logits bitsum=%llu first2=%.9g %.9g\n",
           (unsigned long long)s, first2[0], first2[1]);
  }
}

static void write_i32(u32 tid, int v) {
  BK->buf_write(TENS[tid].buf_id, &v, 4);
}

// Top-2 logits readback (ties -> lowest index, matching argmax_part): the
// near-tie forensic for a trajectory divergence -- a tiny margin proves the
// flip is bf16 rounding, not a wrong weight.
static void top2(int *i1, float *v1, int *i2, float *v2) {
  static float *host = NULL;
  if (host == NULL) host = (float *)malloc((u64)C.vocab * 4);
  TENS[LOGITS_TID].backend->buf_read(TENS[LOGITS_TID].buf_id, host, (u64)C.vocab * 4);
  *i1 = 0; *v1 = host[0]; *i2 = -1; *v2 = -3.4e38f;
  for (u32 i = 1; i < C.vocab; i++) {
    if (host[i] > *v1) { *i2 = *i1; *v2 = *v1; *i1 = (int)i; *v1 = host[i]; }
    else if (host[i] > *v2) { *i2 = (int)i; *v2 = host[i]; }
  }
}

// Real-weight greedy decode: token-by-token prefill of the prompt ids through
// the captured step (the device argmax is overwritten by the next prompt id,
// mirroring the tinygrad prefill semantics one position at a time), then
// `count` generated tokens.  read_i32 after each replay is the sync point
// (cuda buf_read drains the capture stream); the following buf_write of the
// next prompt id is therefore ordered after the step that consumed the
// previous one.
static int run_prompt(u32 slot, int const *prompt, u32 np, u32 count, int dump2) {
  for (u32 i = 1; i < np; i++) {
    (void)read_i32(T_tok);
    write_i32(T_tok, prompt[i]);
    jit_replay(slot);
  }
  int *gen = (int *)malloc((u64)count * 4);
  double mn = 1e30, total = 0;
  gen[0] = read_i32(T_tok);   // prediction after the full prompt
  for (u32 i = 1; i < count; i++) {
    double s0 = now_ms();
    jit_replay(slot);
    gen[i] = read_i32(T_tok);
    double dt = now_ms() - s0;
    total += dt;
    if (dt < mn) mn = dt;
    if (dump2) {
      int i1, i2; float v1, v2;
      top2(&i1, &v1, &i2, &v2);
      printf("  top2 step %u: tok=%d  %d:%.6f  %d:%.6f  margin=%.6g\n",
             i, gen[i], i1, v1, i2, v2, (double)(v1 - v2));
    }
  }
  if (count > 1)
    printf("  gen timing: %u steps  min %.2f ms/step  mean %.2f ms/step  %.1f tok/s (min)  %.1f tok/s (mean)\n",
           count - 1, mn, total / (double)(count - 1), 1e3 / mn, 1e3 * (double)(count - 1) / total);
  printf("thvm gen ids: [");
  for (u32 i = 0; i < count; i++) printf(i ? ", %d" : "%d", gen[i]);
  printf("]\n");
  int len = read_i32(T_len), want_len = (int)(np + count);
  int ok = (len == want_len);
  for (u32 i = 0; i < count; i++) ok = ok && gen[i] >= 0 && (u32)gen[i] < C.vocab;
  printf("  final len=%d (want %d)\n", len, want_len);
  printf("%s\n", ok ? "OK" : "FAIL(state)");
  free(gen);
  return ok ? 0 : 1;
}

// Speculative greedy decode: prefill all but the last prompt token through the
// BASE captured step, run the last prompt token through the base step once (its
// logits row is the bit-exactness reference and its argmax is gen[0]), rewind
// the device length, seed the ids buffer, then capture + blind-replay the spec
// step.  Verify is bit-exact vs the base kernels, so the spec stream reproduces
// the base greedy trajectory EXACTLY regardless of draft quality (a wrong draft
// only costs acceptance).  DSV2_FORCE_IDS=<base gen ids> arms the forced-draft
// gate: drafts come from the pre-seeded ids, every fully-seeded step must
// advance by exactly S.
static int run_spec(u32 slotA, int const *prompt, u32 np, u32 count,
                    int const *force, u32 nf) {
  u32 S = SPEC;
  for (u32 i = 1; i + 1 < np; i++) {
    (void)read_i32(T_tok);
    write_i32(T_tok, prompt[i]);
    jit_replay(slotA);
  }
  (void)read_i32(T_tok);
  write_i32(T_tok, prompt[np - 1]);
  jit_replay(slotA);
  int gen0 = read_i32(T_tok);
  float *lg_base = (float *)malloc((u64)C.vocab * 4);
  TENS[LOGITS_TID].backend->buf_read(TENS[LOGITS_TID].buf_id, lg_base, (u64)C.vocab * 4);
  // rewind: the spec step re-verifies prompt[np-1] as its pending token (P = np-1)
  write_i32(T_len, (int)np);
  {
    int *ih = (int *)stage((u64)C.max_seq * 4);
    for (u32 i = 0; i < C.max_seq; i++) ih[i] = -1;
    for (u32 i = 0; i < np; i++) ih[i] = prompt[i];
    for (u32 i = 0; i < nf && np + i < C.max_seq; i++) ih[np + i] = force[i];
    BK->buf_write(TENS[T_ids].buf_id, ih, (u64)C.max_seq * 4);
  }
  if (nf > 0 && force[0] != gen0)
    printf("  [gate] WARNING: DSV2_FORCE_IDS[0]=%d != base gen[0]=%d\n", force[0], gen0);

  double t0 = now_ms();
  u32 slotB = jit_capture_begin();
  if (slotB == 0) { fprintf(stderr, "FATAL: no spec capture slot\n"); return 1; }
  step_build_spec();
  jit_capture_end();
  printf("  spec capture (S=%u): %.2f s   ops=%u   len=%d\n",
         S, (now_ms() - t0) / 1e3, jit_capture_op_count(slotB), read_i32(T_len));

  // GATE: verify row 0 at the base state must be bit-equal to the base logits
  {
    float *lg_s0 = (float *)malloc((u64)C.vocab * 4);
    TENS[LOGITS_S].backend->buf_read(TENS[LOGITS_S].buf_id, lg_s0, (u64)C.vocab * 4);
    if (memcmp(lg_base, lg_s0, (u64)C.vocab * 4) != 0) {
      u32 nd = 0, first = 0;
      for (u32 i = 0; i < C.vocab; i++)
        if (fbits(lg_base[i]) != fbits(lg_s0[i])) { if (nd == 0) first = i; nd++; }
      printf("  [gate] s0 logits memcmp vs base: FAIL (%u/%u words differ, first at %u: %.9g vs %.9g)\n",
             nd, C.vocab, first, lg_base[first], lg_s0[first]);
      free(lg_base); free(lg_s0);
      return 1;
    }
    printf("  [gate] s0 logits memcmp vs base: OK (%u words)\n", C.vocab);
    free(lg_base); free(lg_s0);
  }

  // generation: the capture fire was spec step 1; blind bursts from here
  int Ln = read_i32(T_len) - 1;   // committed count == pending index
  u32 steps = 1;
  long produced = (long)Ln + 1 - (long)np;   // generated incl the pending token
  int force_ok = 1;
  if (nf > 0) {
    // forced-draft gate: single-step replays, per-step advance checked
    if (Ln - ((int)np - 1) != (int)S && (u32)(np - 1 + S) <= np + nf) {
      force_ok = 0;
      printf("  [gate] forced step 1: advance %d != S=%u\n", Ln - ((int)np - 1), S);
    }
    while (produced < (long)count && (u32)Ln + S <= C.max_seq - 1) {
      jit_replay(slotB);
      steps++;
      int prev = Ln;
      Ln = read_i32(T_len) - 1;
      produced = (long)Ln + 1 - (long)np;
      int fully = (u32)prev + S <= np + nf;   // drafts ids[prev+1..prev+S-1] all seeded
      if (fully && Ln - prev != (int)S) {
        force_ok = 0;
        printf("  [gate] forced step %u: advance %d != S=%u (P=%d)\n", steps, Ln - prev, S, prev);
      }
    }
  } else {
    double g0 = now_ms();
    u32 steps0 = steps;
    long prod0 = produced;
    while (produced < (long)count) {
      u32 room = (C.max_seq - 1 - (u32)Ln) / S;   // steps that surely fit
      u32 burst = room < 8 ? room : 8;
      if (burst == 0) break;
      for (u32 b = 0; b < burst; b++) jit_replay(slotB);
      steps += burst;
      Ln = read_i32(T_len) - 1;
      produced = (long)Ln + 1 - (long)np;
    }
    double wall = now_ms() - g0;
    u32 gsteps = steps - steps0;
    long gprod = produced - prod0;
    if (gsteps > 0)
      printf("  spec gen: %ld tok in %u steps  %.2f ms/step  %.1f tok/s  %.2f tok/step\n",
             gprod, gsteps, wall / gsteps, gprod * 1e3 / wall, (double)gprod / gsteps);
  }

  {
    u32 nout = produced < (long)count ? (u32)produced : count;
    int *ih = (int *)stage((u64)C.max_seq * 4);
    TENS[T_ids].backend->buf_read(TENS[T_ids].buf_id, ih, (u64)C.max_seq * 4);
    printf("thvm spec gen ids: [");
    for (u32 i = 0; i < nout; i++) printf(i ? ", %d" : "%d", ih[np + i]);
    printf("]\n");
    int ok = 1;
    for (u32 i = 0; i < nout; i++) ok = ok && ih[np + i] >= 0 && (u32)ih[np + i] < C.vocab;
    if (nf > 0) {
      u32 ncmp = nf < nout ? nf : nout;
      u32 nmatch = 0;
      for (u32 i = 0; i < ncmp; i++) if (ih[np + i] == force[i]) nmatch++;
      printf("  [gate] forced-draft: %s (ids match base %u/%u, per-step advance %s)\n",
             (nmatch == ncmp && force_ok) ? "PASS" : "FAIL", nmatch, ncmp,
             force_ok ? "OK" : "VIOLATED");
      ok = ok && nmatch == ncmp && force_ok;
    }
    printf("  accept: %ld committed+pending over %u spec steps = %.2f tok/step  final len=%d\n",
           produced, steps, (double)produced / (double)steps, read_i32(T_len));
    printf("%s\n", ok ? "OK" : "FAIL(spec)");
    return ok ? 0 : 1;
  }
}

// === serve mode (DSV2_SERVE=1) ==============================================
// Weights + the captured step(s) stay resident; the process answers prompt
// requests on stdin.  Protocol: "ready" is printed (stdout, flushed) once the
// capture is live; each request line is comma-separated prompt token ids,
// optionally "ids|max_new" for a per-request generation cap (default
// DSV2_MAXNEW, 256); the response is ONE line "ids: <comma-separated
// generated ids>", flushed; EOF exits cleanly.  Per request the device state
// is re-prefilled from position 0 over the same buffers (T_len=1 +
// T_tok=prompt[0], one replay per prompt token): cache rows are overwritten
// in place and the score kernels only read rows < len, so nothing stale
// survives.  Generation stops at DSV2_EOS (100001, the DeepSeek-V2 eos id;
// the eos token is NOT emitted).  DSV2_SPEC=S serves the blind-speculative
// path: the spec step is captured on the first request (its verify row 0 is
// memcmp-gated against the base logits exactly like run_spec) and replayed
// with a fresh T_len/T_ids seed for every later one.  Timing (prefill ms,
// gen ms/tok) goes to stderr, keeping the stdout protocol clean.
static u32 SLOT_SPEC = 0;
static double SERVE_MS_PRE = 0, SERVE_MS_GEN = 0;

static void serve_prefill(u32 slot, int const *prompt, u32 np) {
  (void)read_i32(T_tok);          // drain the previous request's stream
  write_i32(T_len, 1);
  write_i32(T_tok, prompt[0]);
  jit_replay(slot);               // consumes prompt[0] at position 0
  for (u32 i = 1; i < np; i++) {
    (void)read_i32(T_tok);
    write_i32(T_tok, prompt[i]);
    jit_replay(slot);
  }
}

static u32 serve_gen_base(u32 slot, int const *prompt, u32 np, u32 max_new,
                          int eos, int *out) {
  double t0 = now_ms();
  serve_prefill(slot, prompt, np);
  u32 n = 0;
  int tok = read_i32(T_tok);      // prediction after the full prompt
  SERVE_MS_PRE = now_ms() - t0;
  t0 = now_ms();
  while (n < max_new && tok != eos) {
    out[n++] = tok;
    if (n == max_new) break;
    jit_replay(slot);
    tok = read_i32(T_tok);
  }
  SERVE_MS_GEN = now_ms() - t0;
  return n;
}

static u32 serve_gen_spec(u32 slot, int const *prompt, u32 np, u32 max_new,
                          int eos, int *out) {
  u32 S = SPEC;
  double t0 = now_ms();
  serve_prefill(slot, prompt, np);
  (void)read_i32(T_tok);          // sync; the spec step re-verifies prompt[np-1]
  write_i32(T_len, (int)np);
  static int *ih = NULL;
  if (ih == NULL) ih = (int *)malloc((u64)C.max_seq * 4);
  for (u32 i = 0; i < C.max_seq; i++) ih[i] = i < np ? prompt[i] : -1;
  BK->buf_write(TENS[T_ids].buf_id, ih, (u64)C.max_seq * 4);
  SERVE_MS_PRE = now_ms() - t0;
  t0 = now_ms();
  if (SLOT_SPEC == 0) {
    // first request: capture the spec step (the fire is spec step 1) + gate
    float *lg_base = (float *)malloc((u64)C.vocab * 4);
    TENS[LOGITS_TID].backend->buf_read(TENS[LOGITS_TID].buf_id, lg_base, (u64)C.vocab * 4);
    SLOT_SPEC = jit_capture_begin();
    if (SLOT_SPEC == 0) { fprintf(stderr, "FATAL: no spec capture slot\n"); exit(1); }
    step_build_spec();
    jit_capture_end();
    float *lg_s0 = (float *)malloc((u64)C.vocab * 4);
    TENS[LOGITS_S].backend->buf_read(TENS[LOGITS_S].buf_id, lg_s0, (u64)C.vocab * 4);
    if (memcmp(lg_base, lg_s0, (u64)C.vocab * 4) != 0) {
      fprintf(stderr, "FATAL: spec s0 logits memcmp vs base FAIL\n");
      exit(1);
    }
    fprintf(stderr, "[serve] spec captured: S=%u ops=%u gate OK\n",
            S, jit_capture_op_count(SLOT_SPEC));
    free(lg_base);
    free(lg_s0);
  } else {
    jit_replay(SLOT_SPEC);
  }
  int Ln;
  long produced;
  for (;;) {
    Ln = read_i32(T_len) - 1;     // committed count == pending index
    produced = (long)Ln + 1 - (long)np;
    TENS[T_ids].backend->buf_read(TENS[T_ids].buf_id, ih, (u64)C.max_seq * 4);
    int hit_eos = 0;
    for (long i = 0; i < produced && !hit_eos; i++) hit_eos = ih[np + i] == eos;
    if (hit_eos || produced >= (long)max_new) break;
    u32 room = (C.max_seq - 1 - (u32)Ln) / S;   // steps that surely fit
    u32 burst = room < 8 ? room : 8;
    if (burst == 0) break;
    for (u32 b = 0; b < burst; b++) jit_replay(SLOT_SPEC);
  }
  u32 n = 0;
  for (long i = 0; i < produced && n < max_new && ih[np + i] != eos; i++)
    out[n++] = ih[np + i];
  SERVE_MS_GEN = now_ms() - t0;
  return n;
}

static int serve_loop(u32 slot) {
  int eos = (int)envu("DSV2_EOS", 100001);
  u32 defmax = envu("DSV2_MAXNEW", 256);
  int *prompt = (int *)malloc((u64)C.max_seq * 4);
  int *out = (int *)malloc((u64)C.max_seq * 4);
  char *line = NULL;
  size_t lcap = 0;
  printf("ready\n");
  fflush(stdout);
  for (;;) {
    ssize_t r = getline(&line, &lcap, stdin);
    if (r < 0) break;             // EOF: clean exit
    u32 np = 0, max_new = defmax;
    {
      char *bar = strchr(line, '|');
      if (bar != NULL) { max_new = (u32)strtoul(bar + 1, NULL, 10); *bar = '\0'; }
      char const *p = line;
      while (np < C.max_seq && (*p == '-' || (*p >= '0' && *p <= '9'))) {
        prompt[np++] = (int)strtol(p, (char **)&p, 10);
        if (*p != ',') break;
        p++;
      }
    }
    long cap = (long)C.max_seq - (long)np - (SPEC > 0 ? (long)SPEC + 1 : 0) - 1;
    if (cap < (long)max_new) max_new = cap < 0 ? 0 : (u32)cap;
    u32 n = 0;
    SERVE_MS_PRE = SERVE_MS_GEN = 0;
    if (np > 0 && max_new > 0)
      n = (SPEC > 0 && np >= 2) ? serve_gen_spec(slot, prompt, np, max_new, eos, out)
                                : serve_gen_base(slot, prompt, np, max_new, eos, out);
    printf("ids: ");
    for (u32 i = 0; i < n; i++) printf(i ? ",%d" : "%d", out[i]);
    printf("\n");
    fflush(stdout);
    fprintf(stderr, "[serve] np=%u gen=%u  prefill %.1f ms (%.2f ms/tok)  gen %.1f ms (%.2f ms/tok, %.1f tok/s)\n",
            np, n, SERVE_MS_PRE, np > 0 ? SERVE_MS_PRE / np : 0.0, SERVE_MS_GEN,
            n > 0 ? SERVE_MS_GEN / n : 0.0, SERVE_MS_GEN > 0 ? n * 1e3 / SERVE_MS_GEN : 0.0);
    fflush(stderr);
  }
  free(prompt);
  free(out);
  free(line);
  return 0;
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
  SPEC = envu("DSV2_SPEC", 0);
  u32 warm = envu("WARM", 20), steps = envu("STEPS", 40);
  // prompt mode: DSV2_PROMPT_IDS=comma-list (token-by-token prefill + greedy generation)
  int prompt[512], force[512];
  u32 np = 0, nf = 0, count = envu("DSV2_COUNT", 24);
  {
    char const *pids = getenv("DSV2_PROMPT_IDS");
    if (pids != NULL && pids[0] != '\0') {
      char const *p = pids;
      while (np < 512) {
        prompt[np++] = (int)strtol(p, (char **)&p, 10);
        if (*p != ',') break;
        p++;
      }
    }
    // DSV2_FORCE_IDS (spec mode): the base run's gen ids, seeded as drafts for
    // the forced-acceptance gate
    char const *fids = getenv("DSV2_FORCE_IDS");
    if (fids != NULL && fids[0] != '\0') {
      char const *p = fids;
      while (nf < 512) {
        force[nf++] = (int)strtol(p, (char **)&p, 10);
        if (*p != ',') break;
        p++;
      }
    }
  }
  u32 serve = envu("DSV2_SERVE", 0);
  if (SPEC > 0 && (serve != 0 ? SPEC < 2 : (np < 2 || SPEC < 2))) {
    fprintf(stderr, "FATAL: DSV2_SPEC needs S >= 2 and DSV2_PROMPT_IDS with >= 2 tokens\n");
    return 1;
  }
  if (serve == 0 && (np > 0 ? (np + count + (SPEC > 0 ? SPEC + 1 : 0) > C.max_seq)
                            : (1 + warm + steps + 1 > C.max_seq))) {
    fprintf(stderr, "FATAL: token budget exceeds DSV2_MAXSEQ=%u\n", C.max_seq);
    return 1;
  }
  printf("dsv2 decode bench: dev=%s dim=%u layers=%u heads=%u kv=%u experts=%u topk=%u vocab=%u maxseq=%u\n",
         IS_CUDA ? "cuda" : "cpu", C.dim, C.n_layers, C.n_heads, C.kv_lora, C.n_exp, C.top_k, C.vocab, C.max_seq);

  {
    char const *wpath = getenv("DSV2_WEIGHTS");
    if (wpath != NULL && wpath[0] != '\0') pack_open(wpath);
  }
  double t0 = now_ms();
  model_init();
  if (SPEC > 0) spec_init(nf > 0);
  printf("  %s init: %.2f s%s\n", PACK != NULL ? "weight" : "synth",
         (now_ms() - t0) / 1e3, SPEC > 0 ? "  (spec armed)" : "");
  if (np > 0) write_i32(T_tok, prompt[0]);

  // capture: build + fire the whole step once (each recorded buffer pinned)
  t0 = now_ms();
  u32 slot = jit_capture_begin();
  if (slot == 0) { fprintf(stderr, "FATAL: no capture slot\n"); return 1; }
  step_build();
  jit_capture_end();
  printf("  capture (step 1): %.2f s   ops=%u   tok=%d len=%d\n",
         (now_ms() - t0) / 1e3, jit_capture_op_count(slot), read_i32(T_tok), read_i32(T_len));
  if (envu("DSV2_DUMP", 0) != 0) dump_fingerprints();

  if (serve != 0) return serve_loop(slot);
  if (np > 0 && SPEC > 0) return run_spec(slot, prompt, np, count, force, nf);
  if (np > 0) return run_prompt(slot, prompt, np, count, envu("DSV2_TOP2", 0) != 0);

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
