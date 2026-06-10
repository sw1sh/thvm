// A vector-times-matrix out{1,N} = a{1,K} @ B{K,N} (the M==1 leading-unit
// row) must dispatch through cblas_sgemm (M=1), not the slow scalar walk.
// The lift elides the unit M row so A's address collapses to a bare
// RANGE(k): the matmul classifier (2 ranges/operand) declines and the
// gemv emit can't express the {K,N}-matrix orientation, so without the
// dedicated vecmat classifier the single-token-decode projections + the
// LM-head matvec ([1,50257] ~40 ms scalar vs ~ms on cblas) run scalar.
#include "../src/thvm.c"
#include "test.h"
static u32 ten(u32 d0, u32 d1, float fill) {
  Shape sh = {0}; sh.ndim = 2; sh.dims[0] = d0; sh.dims[1] = d1;
  u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u32 n = TENS[t].view.numel; float *b = malloc(n*4);
  for (u32 i=0;i<n;i++) b[i]=fill; CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4); free(b);
  return t;
}
// realize {M,N} matmul of ones; return (#value mismatches), report BLAS fired.
static int check(u32 M, u32 K, u32 N, u64 *gemm) {
  Term A = term_new(0,TAG_TEN,DT_FP32, ten(M, K, 1.0f));
  Term B = term_new(0,TAG_TEN,DT_FP32, ten(K, N, 1.0f));
  u32 mk1[3]={M,K,1}, mkn[3]={M,K,N}, _kn[3]={1,K,N};
  Term ae = uop_expand(uop_reshape(A,3,mk1), 3, mkn);
  Term be = uop_expand(uop_reshape(B,3,_kn), 3, mkn);
  Term mm = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, ae, be));  // {M,N}
  u64 g0 = cpu_blas_gemm_dispatch_dag_count();
  Term r = term_resolve(thvm_realize(mm));
  *gemm = cpu_blas_gemm_dispatch_dag_count() - g0;
  float *out = malloc((u64)M*N*4);
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, out, (u64)M*N*4);
  int bad = 0;
  for (u32 i=0;i<M*N;i++) if (!(out[i] > K-0.1f && out[i] < K+0.1f)) bad++;
  free(out); return bad;
}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("blas/matvec-leading-unit-axis");
  u64 g2=0, g1=0, glm=0;
  int b2  = check(2, 64, 300, &g2);     // control: M=2 (matmul GEMM)
  int b1  = check(1, 64, 300, &g1);     // M=1 vec x mat
  int blm = check(1, 768, 50257, &glm); // real LM-head shape
  printf("  M=2 ctrl: bad=%d gemm=%llu | M=1: bad=%d gemm=%llu | LM-head: bad=%d gemm=%llu\n",
         b2, (unsigned long long)g2, b1, (unsigned long long)g1, blm, (unsigned long long)glm);
  if (b2 || b1 || blm) f++;                  // numerics
  if (g2==0 || g1==0 || glm==0) {            // BLAS must fire on each
    printf("  (BLAS did not fire on an M=1 matvec -- vecmat path unexercised)\n"); f++;
  }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
