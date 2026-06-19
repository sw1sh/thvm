// Regression: THVM_FUSE_MATMUL_INPUT must be SAFE on CPU.
//
// Models a FLUX projection fragment -- an AdaLN modulation (x-centered then
// (1+sc)*x + sh, a multi-op single-consumer elementwise chain) feeding a Q/K/V
// projection matmul.  The matmul-input fuse (rangeify_unified.c) un-realizes
// the producer so it inlines into the matmul; that inline is emittable ONLY by
// the Metal register-blocked tiled emitter (which reconstructs the producer's
// (m,k) directly).  On CPU/CUDA the producer feeds the matmul through the
// matmul-lowering reshape(+unit)+expand(N) -- a rank-changing movement the
// POSITIONAL per-consumer re-index cannot bind -- so un-realizing it leaks the
// producer's own M/K ranges as extra output loops (a |M||N||M||K||K| runaway
// nest that hung the kernel).  ru_fuse_matmul_input_target_ok keeps the producer
// realized off Metal, so the matmul stays a clean BLAS operand.  This test runs
// on the CPU backend; it must terminate quickly and match the realized-input
// reference whether THVM_FUSE_MATMUL_INPUT is set or not.
#include "../src/thvm.c"
#include "test.h"

static u32 ten(u32 ndim, u32 *dims, float scl) {
  Shape sh = {0}; sh.ndim = ndim;
  u64 n = 1; for (u32 i = 0; i < ndim; i++) { sh.dims[i] = dims[i]; n *= dims[i]; }
  u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  float *b = malloc(n * 4);
  for (u64 i = 0; i < n; i++) b[i] = scl * (float)((i % 11) - 5) * 0.03f;
  CPU_BACKEND.buf_write(TENS[t].buf_id, b, n * 4); free(b);
  return t;
}

// out{M,N} = (centered(X)*(1+sc)+sh) @ W   -- AdaLN-modulated projection.
// realize_input: hard-realize the modulated activation before the matmul
// (the WL TRealize[x] barrier).  Returns out[]; caller reads KERNELS_NEXT.
static void modproj(u32 M, u32 K, u32 N, int realize_input, float *out) {
  u32 mk[2] = {M, K}, kn[2] = {K, N}, m1[2] = {M, 1}, _1k[2] = {1, K};
  Term X  = term_new(0, TAG_TEN, DT_FP32, ten(2, mk, 1.0f));
  Term W  = term_new(0, TAG_TEN, DT_FP32, ten(2, kn, 1.0f));
  Term sc = term_new(0, TAG_TEN, DT_FP32, ten(2, _1k, 0.5f));
  Term sh = term_new(0, TAG_TEN, DT_FP32, ten(2, _1k, 0.2f));
  Term sum  = uop_reduce(REDUCE_SUM, 1, X);             // reduce-fed centering term
  Term cen  = uop_binary(UOP_ADD, X,
                uop_binary(UOP_MUL,
                  uop_expand(uop_reshape(sum, 2, m1), 2, mk),
                  term_new(0, TAG_TEN, DT_FP32, ten(2, mk, 0.001f))));
  Term scB  = uop_expand(uop_reshape(sc, 2, _1k), 2, mk);
  Term shB  = uop_expand(uop_reshape(sh, 2, _1k), 2, mk);
  Term one  = term_new(0, TAG_TEN, DT_FP32, ten(2, mk, 0.0f));
  Term modu = uop_binary(UOP_ADD,
                uop_binary(UOP_MUL, cen, uop_binary(UOP_ADD, scB, one)), shB);
  if (realize_input) modu = term_resolve(thvm_realize(modu));   // WL TRealize[x]
  u32 mk1[3] = {M,K,1}, mkn[3] = {M,K,N}, _kn[3] = {1,K,N};
  Term me = uop_expand(uop_reshape(modu, 3, mk1), 3, mkn);
  Term we = uop_expand(uop_reshape(W, 3, _kn), 3, mkn);
  Term mm = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, me, we));  // {M,N}
  Term r = term_resolve(thvm_realize(mm));
  u32 rv = (u32)term_val(r);
  TENS[rv].backend->buf_read(TENS[rv].buf_id, out, (u64)M * N * 4);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("flux/fuse-matmul-input-cpu-safe");
  int f = 0;
  const u32 M = 256, K = 512, N = 512;
  float *o_real = malloc((u64)M * N * 4);
  float *o_fuse = malloc((u64)M * N * 4);
  modproj(M, K, N, 1, o_real);     // hard-realize input (the WL barrier)
  modproj(M, K, N, 0, o_fuse);     // input left to the scheduler (FMI path off Metal: stays realized)
  int bad = 0; double me = 0;
  for (u64 i = 0; i < (u64)M * N; i++) {
    double d = fabs((double)o_real[i] - (double)o_fuse[i]);
    if (d > me) me = d;
    if (d > 1e-3) bad++;
  }
  printf("  M=%u K=%u N=%u  flag=%s  max|diff|=%.3e  bad=%d\n",
         M, K, N, getenv("THVM_FUSE_MATMUL_INPUT") ? "1" : "off", me, bad);
  if (bad) { printf("  numeric mismatch: scheduler matmul-input path diverged\n"); f++; }
  free(o_real); free(o_fuse);
  printf("  %s (%d failures)\n", f == 0 ? "ok" : "FAIL", f);
  return f == 0 ? 0 : 1;
}
