// Repro: a reduce (layernorm-mean) feeding an elementwise feeding a matmul
// reduce.  tinygrad's "one reduce per kernel" should keep the mean-reduce
// and the matmul-reduce in SEPARATE kernels -> the matmul reads a clean
// buffer -> BLAS.  At M==1 (leading-unit axis) the faithful seed fails to
// realize the matmul's elementwise input -> fused scalar (the GPT-2 decode
// LM-head 582ms bug).  Checks whether the matmul dispatches through BLAS at
// M=2 (control) vs M=1.
#include "../src/thvm.c"
#include "test.h"
static u32 ten(u32 d0, u32 d1, float fill) {
  Shape sh = {0}; sh.ndim = 2; sh.dims[0] = d0; sh.dims[1] = d1;
  u32 t = tensor_alloc(&CPU_BACKEND, sh, DT_FP32);
  u32 n = TENS[t].view.numel; float *b = malloc(n*4);
  for (u32 i=0;i<n;i++) b[i]=fill; CPU_BACKEND.buf_write(TENS[t].buf_id,b,n*4); free(b);
  return t;
}
static int check(u32 M, u32 K, u32 N, u64 *gemm) {
  Term X = term_new(0,TAG_TEN,DT_FP32, ten(M, K, 1.0f));
  Term W = term_new(0,TAG_TEN,DT_FP32, ten(K, N, 1.0f));
  Term s   = uop_reduce(REDUCE_SUM, 1, X);                 // {M,1} mean*K
  u32 m1[2]={M,1}, mk[2]={M,K};
  Term sb  = uop_expand(uop_reshape(s,2,m1), 2, mk);
  Term invK = term_new(0,TAG_TEN,DT_FP32, ten(M, K, -1.0f/(float)K));
  Term negmean = uop_binary(UOP_MUL, sb, invK);
  Term c   = uop_binary(UOP_ADD, X, negmean);              // {M,K} = X - mean
  u32 mk1[3]={M,K,1}, mkn[3]={M,K,N}, _kn[3]={1,K,N};
  Term ce = uop_expand(uop_reshape(c,3,mk1), 3, mkn);
  Term we = uop_expand(uop_reshape(W,3,_kn), 3, mkn);
  Term mm = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, ce, we));  // {M,N}
  u64 g0 = cpu_blas_gemm_dispatch_dag_count();
  Term r = term_resolve(thvm_realize(mm));
  *gemm = cpu_blas_gemm_dispatch_dag_count() - g0;
  float *out = malloc((u64)M*N*4);
  TENS[(u32)term_val(r)].backend->buf_read(TENS[(u32)term_val(r)].buf_id, out, (u64)M*N*4);
  int bad=0; for (u32 i=0;i<M*N;i++) if (!(out[i] > -0.01f && out[i] < 0.01f)) bad++;
  free(out); return bad;
}
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("blas/ln-matmul-reduce-boundary-m1");
  u64 g2=0,g1=0;
  int b2 = check(2,64,300,&g2);   // control: M=2 fires BLAS today
  int b1 = check(1,64,300,&g1);   // M=1: currently FUSED (no BLAS) -- the bug
  printf("  M=2: bad=%d gemm=%llu  | M=1: bad=%d gemm=%llu\n",
         b2,(unsigned long long)g2,b1,(unsigned long long)g1);
  if (b2||b1) f++;                          // numerics must stay correct
  if (g2==0) { printf("  control M=2 lost BLAS -- regression\n"); f++; }
  if (g1==0) { printf("  M=1 matmul-after-reduce did NOT fire BLAS (the bug)\n"); f++; }
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
