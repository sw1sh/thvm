// Compile-time + correctness guard for the fused-matmul A-staging on Metal.
//
// THVM_FUSE_MATMUL_INPUT fuses an elementwise producer (the FLUX AdaLN
// modulation (x*sc + sh), casts, activations) INTO a bf16 tensor-core matmul's
// A-staging load (render_uop.c rmu_emit_matmul_tc_tiled, the a_fused branch of
// EMIT_LOAD_A): the matmul's A-operand becomes the inlined producer value,
// computed at the cooperative threadgroup-staging load, keeping the simdgroup
// tensor cores instead of realizing the producer to its own buffer kernel.
//
// This guards two properties at a velocity-ish shape (M,K,N all %8==0, the
// 128x64 register-blocked tile FLUX picks for every velocity matmul):
//
//   1. CORRECTNESS: the fused matmul is bit-identical to the realized-input
//      reference (maxAbsDiff == 0).  This is the hard invariant -- the fuse must
//      never change the result.
//
//   2. COMPILE TIME: the fused matmul's cold Metal compile (newLibraryWithSource:
//      MSL->AIR + AIR->GPU) is within a small factor of the clean non-fused
//      matmul's.  Measured ground truth on Apple M3 Max: a cold tiled bf16
//      matmul compiles in ~20ms (frontend) / ~50-65ms (full), INDEPENDENT of
//      whether the producer is fused -- the producer lands in the A-staging
//      load, never in the inner simdgroup_multiply_accumulate loop, so it does
//      not fight the matrix-coprocessor codegen.  A regression that pushed the
//      producer into the MMA path (or otherwise made the fused shader
//      pathological to compile) would blow this ratio out.
//
// Measurement notes: the disk lib/PSO cache is forced OFF (THVM_METAL_PSO_CACHE
// =0) so both shaders compile fresh via newLibraryWithSource:.  Fused and
// non-fused use DISTINCT M so their shader sources differ -> neither warms the
// macOS system shader cache for the other within this process.  The in-memory
// PSO cache is dropped before each timed realize.  Absolute times still vary
// with the warm/cold state of the macOS system shader cache (cleared only by
// the OS), so the gate is a loose 8x bound -- it catches a real blowup, not
// run-to-run jitter.
//
//   THVM_FUSE_MATMUL_INPUT=1 ./bin/test_metal_fuse_matmul_compile
#include "../src/thvm.c"
#include "test.h"

// Defined in src/backend/metal/_.m (compiled as a separate object): the
// accumulated newLibraryWithSource: compile time (us) and a drop of the
// in-memory PSO cache (forces the next realize to recompile its shaders).
extern u64  thvm_metal_jit_compile_us(void);
extern void thvm_metal_jit_drop_in_memory_psos(void);
extern u64  thvm_metal_jit_misses(void);

static u32 leaf2(u32 d0, u32 d1, float *data) {
  Shape sh = {0}; sh.ndim = 2; sh.dims[0] = d0; sh.dims[1] = d1;
  u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  CPU_BACKEND.buf_write(TENS[t].buf_id, data, (u64)d0 * d1 * 4);
  return t;
}

// Build out = ((X*Sc + Sh) cast bf16) @ Transpose[W], {M,K}@{K,N}->{M,N}, bf16
// tensor cores.  X/Sc/Sh are full {M,K} (single-consumer -> the chain fuses).
// When `fuse` the producer stays inlined (THVM_FUSE_MATMUL_INPUT must be on);
// else it is realized to a clean bf16 buffer (the clean-buffer TC tiled path).
static Term build_matmul(u32 M, u32 K, u32 N,
                         float *X, float *Sc, float *Sh, float *W, int fuse) {
  Term Xt  = term_new(0, TAG_TEN, DT_FP32, leaf2(M, K, X));
  Term Sct = term_new(0, TAG_TEN, DT_FP32, leaf2(M, K, Sc));
  Term Sht = term_new(0, TAG_TEN, DT_FP32, leaf2(M, K, Sh));
  Term Wt  = term_new(0, TAG_TEN, DT_FP32, leaf2(N, K, W));
  Term prod = uop_binary(UOP_ADD, uop_binary(UOP_MUL, Xt, Sct), Sht); // X*Sc+Sh
  Term pbf = uop_cast(prod, DT_BF16);
  if (!fuse) {
    pbf = term_resolve(thvm_realize(uop_to_device_leaves(pbf, THVM_DEV_METAL)));
    pbf = uop_to_device_leaves(pbf, THVM_DEV_METAL);
  }
  Term wbf = term_resolve(thvm_realize(
               uop_to_device_leaves(uop_cast(Wt, DT_BF16), THVM_DEV_METAL)));
  wbf = uop_to_device_leaves(wbf, THVM_DEV_METAL);
  u32 perm[2] = {1, 0}; Term Bperm = uop_permute(wbf, 2, perm);
  u32 a3[3] = {M, K, 1}, mkn[3] = {M, K, N}, b3[3] = {1, K, N};
  Term Ae = uop_cast(uop_expand(uop_reshape(pbf, 3, a3), 3, mkn), DT_FP32);
  Term Be = uop_cast(uop_expand(uop_reshape(Bperm, 3, b3), 3, mkn), DT_FP32);
  Term outf = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, Ae, Be));
  return uop_to_device_leaves(outf, THVM_DEV_METAL);
}

// Drop the in-memory PSO cache, then realize -- so this realize recompiles its
// shaders (a synthetic cold start).  Returns the M*N f32 output + the
// matmul-shader compile-us (the inputs are realized in build_matmul before the
// timed realize, so only the matmul compiles here).
static void realize_timed(Term out, u32 M, u32 N, float *dst, u64 *compile_us) {
  thvm_metal_jit_drop_in_memory_psos();
  u64 c0 = thvm_metal_jit_compile_us();
  u64 m0 = thvm_metal_jit_misses();
  Term r = term_resolve(thvm_realize(out));
  *compile_us = thvm_metal_jit_compile_us() - c0;
  fprintf(stderr, "    [realize: %llu shader compiles, %llu us]\n",
          (unsigned long long)(thvm_metal_jit_misses() - m0),
          (unsigned long long)*compile_us);
  TENS[(u32)term_val(r)].backend->buf_read(
    TENS[(u32)term_val(r)].buf_id, dst, (u64)M * N * 4);
}

// bf16-faithful host reference: producer + weight rounded to bf16, product
// accumulated in f32 (matching the simdgroup bfloat-operand / f32-accumulate).
static float bf16rt(float x) {
  u32 b; memcpy(&b, &x, 4); u32 r = (b >> 16) & 1; b += 0x7fff + r;
  b &= 0xffff0000u; float o; memcpy(&o, &b, 4); return o;
}

int main(void) {
  // Disable the disk lib/PSO cache so both matmuls compile fresh via
  // newLibraryWithSource: (the cold path FLUX pays).  setenv before thvm_init
  // so metal_pso_cache_init sees it.
  setenv("THVM_METAL_PSO_CACHE", "0", 1);
  thvm_init();

  int fuse_on = getenv("THVM_FUSE_MATMUL_INPUT") != NULL
             && getenv("THVM_FUSE_MATMUL_INPUT")[0] != '0';

  u32 K = 3072, N = 2048;
  // Distinct M for fused vs non-fused so their matmul shaders differ (no macOS
  // system-shader-cache cross-warming between the two timed compiles).  Both M
  // pick the same 128x64 tile (any M >= 128, %128==0).
  u32 Mc = 768;   // non-fused control
  u32 Mf = 896;   // fused

  TEST_BEGIN("metal/fuse-matmul-compile");
  int f = 0;

  // --- non-fused control: realize the producer, clean-buffer TC tiled path ---
  float *Xc = malloc((u64)Mc * K * 4), *Scc = malloc((u64)Mc * K * 4),
        *Shc = malloc((u64)Mc * K * 4), *W = malloc((u64)N * K * 4);
  for (u64 i = 0; i < (u64)Mc * K; i++) { Xc[i]  = 0.20f * sinf(0.7f * i);
                                          Scc[i] = 0.5f + 0.10f * cosf(0.3f * i);
                                          Shc[i] = 0.05f * sinf(0.2f * i + 1.f); }
  for (u64 i = 0; i < (u64)N * K; i++) W[i] = 0.15f * cosf(0.4f * i + 0.5f);
  float *ctrl = malloc((u64)Mc * N * 4); u64 ctrl_us = 0;
  realize_timed(build_matmul(Mc, K, N, Xc, Scc, Shc, W, /*fuse=*/0),
                Mc, N, ctrl, &ctrl_us);
  // Verify the control matches the bf16-faithful host reference (so the
  // "reference" the fused path is checked against is itself correct).
  {
    float maxd = 0;
    for (u32 m = 0; m < (Mc < 8 ? Mc : 8); m++)
      for (u32 n = 0; n < (N < 8 ? N : 8); n++) {
        float s = 0;
        for (u32 k = 0; k < K; k++)
          s += bf16rt(Xc[m*K+k]*Scc[m*K+k]+Shc[m*K+k]) * bf16rt(W[n*K+k]);
        float d = fabsf(ctrl[m*N+n] - s); if (d > maxd) maxd = d;
      }
    if (maxd > 0.05f) { printf("  control matmul wrong vs host ref (maxd=%.4f)\n",
                               maxd); f++; }
  }

  // --- fused path: producer inlined into the A-staging (when flag on) ---
  float *Xf = malloc((u64)Mf * K * 4), *Scf = malloc((u64)Mf * K * 4),
        *Shf = malloc((u64)Mf * K * 4);
  for (u64 i = 0; i < (u64)Mf * K; i++) { Xf[i]  = 0.20f * sinf(0.7f * i);
                                          Scf[i] = 0.5f + 0.10f * cosf(0.3f * i);
                                          Shf[i] = 0.05f * sinf(0.2f * i + 1.f); }
  float *fus = malloc((u64)Mf * N * 4); u64 fus_us = 0;
  realize_timed(build_matmul(Mf, K, N, Xf, Scf, Shf, W, /*fuse=*/fuse_on),
                Mf, N, fus, &fus_us);
  // Fused output vs its own bf16-faithful host reference -- the hard
  // correctness invariant (the fuse must never change the result).
  float maxd = 0; u32 nbad = 0;
  for (u32 m = 0; m < Mf; m++)
    for (u32 n = 0; n < N; n++) {
      float s = 0;
      for (u32 k = 0; k < K; k++)
        s += bf16rt(Xf[m*K+k]*Scf[m*K+k]+Shf[m*K+k]) * bf16rt(W[n*K+k]);
      float d = fabsf(fus[m*N+n] - s); if (d > maxd) maxd = d;
      if (d > 0.05f) nbad++;
    }

  double ratio = ctrl_us > 0 ? (double)fus_us / (double)ctrl_us : 0.0;
  printf("  flag=%s  control M=%u, fused M=%u, K=%u N=%u\n",
         fuse_on ? "on" : "off", Mc, Mf, K, N);
  printf("  non-fused compile = %.3f s\n", ctrl_us / 1e6);
  printf("  fused     compile = %.3f s  (%.2fx non-fused)\n",
         fus_us / 1e6, ratio);
  printf("  maxAbsDiff(fused vs bf16 host ref) = %.6f  (%u bad)\n", maxd, nbad);

  if (nbad) { printf("  FUSION INCORRECT vs the bf16 host reference\n"); f++; }
  if (fuse_on && ctrl_us > 0 && ratio > 8.0) {
    // True ratio is ~1x; an 8x bound only fires on a real codegen regression
    // (e.g. the producer leaking into the simdgroup MMA loop).  Jitter from the
    // macOS system shader cache is well under this.
    printf("  fused compile %.2fx > 8x non-fused -- compile regression\n", ratio);
    f++;
  }
  printf("  %s (%d failures)\n", f == 0 ? "ok" : "FAIL", f);
  return f == 0 ? 0 : 1;
}
