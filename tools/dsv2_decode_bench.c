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
// REAL WEIGHTS (Stage 2): DSV2_WEIGHTS=<pack> loads a flat weight pack written
// by tools/dsv2_pack_weights.py (the tinygrad load_weights mapping,
// deepseek_v2.py:1257-1272 + the kv_b absorb split :993-997, pre-applied so
// every payload is already in the exact layout the step consumes).
// DSV2_PROMPT_IDS=comma-list prefills token-by-token through the same decode
// step (each prompt id is force-written into the device token buffer, the
// step's own argmax prediction is discarded until the last prompt token), then
// generates DSV2_COUNT (24) greedy tokens and prints them for comparison with
// the tinygrad reference trajectory ("MY gen ids").  DSV2_TOP2=1 additionally
// dumps the top-2 logits per generated token (near-tie divergence forensics).
//
//   build:  make bin/dsv2_decode_bench            (Linux+CUDA adds the CUDA backend)
//   run:    DEV=cuda ./bin/dsv2_decode_bench      (full model shape, ~31 GB device)
//           DEV=cuda DSV2_WEIGHTS=dsv2.pack DSV2_PROMPT_IDS=100000,504,... \
//             DSV2_COUNT=24 ./bin/dsv2_decode_bench   (real-weight greedy decode)
//           DEV=cpu DSV2_DIM=128 DSV2_LAYERS=4 DSV2_HEADS=8 DSV2_KVLORA=64 \
//             DSV2_DNOPE=32 DSV2_DROPE=16 DSV2_DV=32 DSV2_EXPERTS=8 DSV2_TOPK=3 \
//             DSV2_MOEINTER=64 DSV2_DENSEINTER=256 DSV2_VOCAB=1024 DSV2_MAXSEQ=128 \
//             ./bin/dsv2_decode_bench             (local CPU smoke, no GPU)
//   knobs:  WARM (20) STEPS (40) + every DSV2_* shape knob.
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
    "#pragma unroll 4\n"
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

// === native uop pieces (mirror the WL example) ==============================
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

// Per-layer GEMV output buffers.  A shared buffer would make the layers'
// downstream native uop DAGs structurally identical -> the Term dedup would
// memo the realize and layers 1..N-1 would record NO kernel.  Per-layer tids
// keep every native consumer unique (~11 KB/layer, negligible).
static u32 T_q[MAX_LAYERS], T_kva[MAX_LAYERS], T_attn[MAX_LAYERS];
static u32 T_gateo, T_g1o, T_u1o, T_mlpo;

// One GEMV instance per distinct (K, N, dtypes): source + split factor.
typedef struct { char *sk_src, *red_src; char sk_nm[16], red_nm[16]; u32 N, sk; } Gemv;
static Gemv G_q, G_kv, G_o, G_gate, G_ffw, G_down, G_lm;

static void gemv_make(Gemv *g, char const *tag, u32 K, u32 N,
                      int x_f32, int w_f32, int out_bf16) {
  snprintf(g->sk_nm, sizeof g->sk_nm, "gemv_%s", tag);
  snprintf(g->red_nm, sizeof g->red_nm, "gred_%s", tag);
  g->N       = N;
  g->sk      = gemv_pick_sk(K, N);
  g->sk_src  = src_gemv(g->sk_nm, K, N, g->sk, x_f32, w_f32);
  g->red_src = src_gred(g->red_nm, N, g->sk, out_bf16);
}

static void gemv_run(Gemv const *g, u32 x_tid, u32 w_tid, u32 out_tid) {
  gemv2(g->sk_nm, g->sk_src, g->red_nm, g->red_src, g->N, g->sk,
        x_tid, w_tid, out_tid);
}

static char *S_EMBED, *S_ROPEQ, *S_ROPEK, *S_APPEND, *S_QLAT, *S_SCORE, *S_MIX, *S_O;
static char *S_ROUTE, *S_MOEH, *S_MOEOUT, *S_APART, *S_AFIN, *S_BUMP;

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
      W_wq[l]    = pack_lw(l, "wq", d, hn * (dn + dr), DT_BF16);
      W_wkv[l]   = pack_lw(l, "wkv", d, kv + dr, DT_BF16);
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
    }
    L_ckv[l] = mk_bf16_zero(C.max_seq, kv);
    L_kpe[l] = mk_bf16_zero(C.max_seq, dr);
    T_q[l]    = mk_bf16_zero(1, hn * (dn + dr));
    T_kva[l]  = mk_bf16_zero(1, kv + dr);
    T_attn[l] = mk_bf16_zero(1, d);
  }
  gemv_make(&G_q,    "q",    d, hn * (dn + dr), 0, 0, 1);
  gemv_make(&G_kv,   "kv",   d, kv + dr,        0, 0, 1);
  gemv_make(&G_o,    "o",    hn * dv, d,        0, 0, 1);
  gemv_make(&G_gate, "gate", d, C.n_exp,        1, 1, 0);
  gemv_make(&G_ffw,  "ffw",  d, C.dense_inter,  0, 0, 1);
  gemv_make(&G_down, "down", C.dense_inter, d,  0, 0, 1);
  gemv_make(&G_lm,   "lm",   d, C.vocab,        1, 0, 0);
  {
    Gemv const *gs[7] = {&G_q, &G_kv, &G_o, &G_gate, &G_ffw, &G_down, &G_lm};
    u64 pmax = 0;
    for (u32 i = 0; i < 7; i++) {
      u64 n = (u64)gs[i]->sk * gs[i]->N;
      if (n > pmax) pmax = n;
    }
    T_part = mk_f32_fill((u32)pmax, 0, 0.0f);
  }
  T_gateo = mk_f32_fill(1, C.n_exp, 0.0f);
  T_g1o   = mk_bf16_zero(1, C.dense_inter);
  T_u1o   = mk_bf16_zero(1, C.dense_inter);
  T_mlpo  = mk_bf16_zero(1, d);
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
  LOGITS_TID = mk_f32_fill(1, C.vocab, 0.0f);
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
    u32 xnb_tid = rz(uop_cast(tt(xn_tid), DT_BF16));
    gemv_run(&G_q, xnb_tid, W_wq[l], T_q[l]);
    u32 hdd[2] = {hn, dn + dr};
    Term qm = uop_reshape(tt(T_q[l]), 2, hdd);
    u32 sn_[4] = {0, hn, 0, dn}, sp_[4] = {0, hn, dn, dn + dr};
    u32 qn_tid = rz(uop_shrink(qm, 2, sn_));
    u32 qp_tid = rz(uop_shrink(qm, 2, sp_));
    {
      u32 a[4] = {qp_tid, T_cos, T_sin, T_len}, o[1] = {0};
      krun("rope_dev", S_ROPEQ, hn, dr / 2, a, 4, o, 1);
    }
    gemv_run(&G_kv, xnb_tid, W_wkv[l], T_kva[l]);
    u32 sc_[4] = {0, 1, 0, kv}, sk_[4] = {0, 1, kv, kv + dr};
    Term kvam = tt(T_kva[l]);
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
    gemv_run(&G_o, T_ob, W_wo[l], T_attn[l]);
    u32 x1_tid = rz(uop_binary(UOP_ADD, x, tt(T_attn[l])));
    Term x1 = tt(x1_tid);
    u32 xn2_tid = rms_norm(uop_cast(x1, DT_FP32), d, W_wpost[l], C.eps);
    if (l < C.first_dense) {
      u32 xb_tid = rz(uop_cast(tt(xn2_tid), DT_BF16));
      gemv_run(&G_ffw, xb_tid, W_wg1[l], T_g1o);
      gemv_run(&G_ffw, xb_tid, W_wu1[l], T_u1o);
      Term h = uop_binary(UOP_MUL, silu(tt(T_g1o), C.dense_inter), tt(T_u1o));
      u32 h_tid = rz(h);
      gemv_run(&G_down, h_tid, W_wd1[l], T_mlpo);
      x = tt(rz(uop_binary(UOP_ADD, x1, tt(T_mlpo))));
    } else {
      gemv_run(&G_gate, xn2_tid, W_gate[l], T_gateo);
      {
        u32 a[3] = {T_sel, T_probs, T_gateo}, o[2] = {0, 1};
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
  gemv_run(&G_lm, xnf_tid, W_lm, LOGITS_TID);
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
  // prompt mode: DSV2_PROMPT_IDS=comma-list (token-by-token prefill + greedy generation)
  int prompt[512];
  u32 np = 0, count = envu("DSV2_COUNT", 24);
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
  }
  if (np > 0 ? (np + count > C.max_seq)
             : (1 + warm + steps + 1 > C.max_seq)) {
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
  printf("  %s init: %.2f s\n", PACK != NULL ? "weight" : "synth", (now_ms() - t0) / 1e3);
  if (np > 0) write_i32(T_tok, prompt[0]);

  // capture: build + fire the whole step once (each recorded buffer pinned)
  t0 = now_ms();
  u32 slot = jit_capture_begin();
  if (slot == 0) { fprintf(stderr, "FATAL: no capture slot\n"); return 1; }
  step_build();
  jit_capture_end();
  printf("  capture (step 1): %.2f s   ops=%u   tok=%d len=%d\n",
         (now_ms() - t0) / 1e3, jit_capture_op_count(slot), read_i32(T_tok), read_i32(T_len));

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
